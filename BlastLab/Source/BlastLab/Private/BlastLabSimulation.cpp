#include "BlastLabSimulation.h"

#include "BattleSmokeSubsystem.h"
#include "BlastEffects.h"
#include "BattleSmokeSim.h"
#include "BlastDebris.h"
#include "BlastFlipbook.h"
#include "BlastLabCamera.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"
#include "BlastLabEnvironment.h"
#include "BlastLabGround.h"
#include "BlastLabMover.h"
#include "BlastRecipes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShaderCompiler.h"

namespace
{
	// As variantes gravadas (Tools/build_explosion.py): a mesma carga com sementes diferentes.
	const TCHAR* BakePaths[] = {
		TEXT("/Game/Explosions/DA_HeavyCharge_v0.DA_HeavyCharge_v0"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v1.DA_HeavyCharge_v1"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v2.DA_HeavyCharge_v2"),
	};
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_Explosion.M_Explosion");

	// Variação de tamanho por detonação (instances.js spawn: jitter 0.14): uma
	// mesma arma não sai sempre igual. Tamanho e duração juntos, pela mesma lei.
	constexpr float SizeJitter = 0.14f;

	// quantas explosões na tela ficam com detalhe máximo
	constexpr int32 FullDetailBudget = 3;

	// a deformação pelo que passa: acima dessa escala a explosão não cabe no
	// campo de 96 m que dá o vento (o ataque aéreo, 2.2, cabe; a nuclear não)
	constexpr float MaxWarpScale = 3.0f;

	// a poeira levantada (FBlastWeapon::DustBody) a 35% da densidade gravada: rala,
	// mostra o chão e o tanque (rodada d, 07/10: inteira, era nuvem creme cobrindo o casco)
	constexpr float DustThickness = 0.35f, DustHeightScale = 0.3f, DustHollowM = 4.0f;

	// A cara da bola de gasolina da EG (a da morte de tanque e as pequenas do fogo
	// da carcaça): as curvas são no relógio da gravação, valem em qualquer escala
	void ApplyGasolineBallLook(AExplosionVolume* E)
	{
		// a bola da EG pálida e translúcida até ~1 s (painel, item 5; rodada j):
		// 1.8× de fuligem e fogo por metro no começo, voltando a 1 entre 0.85 e
		// 1.2 s; a batida de 1.3–1.5 s (a casca com brasas) fica como está
		E->EarlyThickness = 1.8f;
		E->EarlyEnd = 1.2f;
		// A temperatura de 0.6 a 1.2 s: mais BAIXA (hipótese "creme = quente
		// demais") não mudou nada (0.7 e 0.6, captures/ue/_ab_fire_ball_m.jpg; o
		// creme era a luz de dentro, abaixo). Sem o creme, a bola ficava escura com
		// brasas aos 0.8–1.0 s, onde o vídeo é laranja-amarelo: 1.3× (1.6 estoura
		// em branco; captures/ue/_ab_ball_temp_o.jpg). -BallTempDip=x pra A/B
		static const float Dip = [] { float V = 1.3f; FParse::Value(FCommandLine::Get(), TEXT("-BallTempDip="), V); return V; }();
		E->TempBase = 1.8f;
		E->TempDip = Dip;
		// Depois da casca com brasas o mapa quente (1.8) deixava a fumaça já fria
		// acima do piso da emissão: aos 3 s o chapéu brilhava marrom (L ~80 na
		// contraluz contra ≤ 50; a fuligem mais escura não mudou nada, então não
		// era luz refletida). -BallTempLate=x pra A/B
		static const float Late = [] { float V = 1.0f; FParse::Value(FCommandLine::Get(), TEXT("-BallTempLate="), V); return V; }();
		E->TempLate = Late;
		// o creme dos 0.8–1.0 s era a luz de dentro da bola acendendo a própria
		// fumaça rala: a 0.3 ele some e a coluna volta a carvão (0.1 também; a 0.3
		// sobra um pouco de brilho no miolo; captures/ue/_ab_ball_light_n.jpg). A
		// luz no chão e nos tanques fica. -BallVolumeLight=x pra A/B
		static const float BallVolumeLight = [] { float V = 0.3f; FParse::Value(FCommandLine::Get(), TEXT("-BallVolumeLight="), V); return V; }();
		E->VolumeLightScale = BallVolumeLight;
		// Rodada p: a bola de 0.7–1.0 s mede R/B 1.2–1.5 (creme-amarelo; o vídeo é
		// ≥ 2, laranja-amarelo). Quente demais nas partes claras: mais brilho com a
		// temperatura do resto (o laranja forte). -BallEmissionDip=x pra A/B
		static const float EmDip = [] { float V = 1.0f; FParse::Value(FCommandLine::Get(), TEXT("-BallEmissionDip="), V); return V; }();
		E->EmissionBase = 16.0f;                   // o EmissionBoost do material (build_explosion.py COLOR)
		E->EmissionDip = EmDip;
	}
}

UBlastLabSimulation* UBlastLabSimulation::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBlastLabSimulation>() : nullptr;
}

const TArray<FBlastWeapon>& UBlastLabSimulation::Arsenal()
{
	// Granada 0.05 = 1 kg de TNT, então 1 = 20 kg. Nuclear 729 = 9³ (~15 t,
	// a ordem do W54 do Davy Crockett): a coluna de ~38 m vira ~340 m e a
	// sequência de 6 s dura ~54 s.
	static const TArray<FBlastWeapon> Weapons = {
		// a granada, o morteiro e o paiol eram a Carga Pesada encolhida ou aumentada: cada tipo
		// vai ganhar receita própria (10/10); até lá ficam fora do painel
		{ TEXT("granada"),  TEXT("Granada"),       0.05f,  TEXT("HE · 1 kg"),                false, 0.0f, false, 0.0f, 1.0f, 1.0f, nullptr, nullptr, true },
		{ TEXT("morteiro"), TEXT("Morteiro"),      0.22f,  TEXT("HE · 5 kg"),                false, 0.0f, false, 0.0f, 1.0f, 1.0f, nullptr, nullptr, true },
		{ TEXT("tanque"),   TEXT("Carga Pesada"),  1.0f,   TEXT("HE · alto rendimento"),     false },
		{ TEXT("deposito"), TEXT("Paiol"),         4.0f,   TEXT("detonação secundária"),     false, 0.0f, false, 0.0f, 1.0f, 1.0f, nullptr, nullptr, true },
		{ TEXT("aereo"),    TEXT("Ataque Aéreo"),  11.0f,  TEXT("bomba de demolição"),       false },
		{ TEXT("nuclear"),  TEXT("Bomba Nuclear"), 729.0f, TEXT("nuclear tática · ~15 t"),   true  },
		// A morte de tanque do vídeo do Veo (a cena herói, docs/CONCEITO_EXPLOSOES.md
		// §4b). Medido 07/10 no gameplay_v3 (tanque = 7.9 m) contra a gravação
		// (Intermediate/BakeRaw, largura onde a fuligem fica opaca): no Veo a bola
		// tem 6–8 m aos 0.3 s, o chapéu 6.7 × 9.4 m (largura × topo) a 1.06 s,
		// 8.6 m a 1.8 s e 10.5 m a 2.8 s; a Carga Pesada gravada tem 11.5 / 13.1 ×
		// 16.2 / ~15 / ~20 m nos mesmos instantes. Escala 0.55 no relógio da
		// gravação (sem acelerar) bate nos quatro. (Até 07/10: 2.2, 4× grande.)
		// O choque é o de uma carga de ~20 kg (tremor de tanque, 0.4% da tela).
		// Poeira: no vídeo ela fica rente (1.5–2 m) e a ~1 tanque do casco, quase
		// sumida em 2.3 s (o estouro é dentro do casco, acima do chão).
		// O corpo é a Gasoline Explosion da JangaFX (EmberGen, CC0), convertida
		// pela sessão de arte em metros reais (pico de 9–10 m de largura, topo
		// ~11 m, casca rachando a ~1.4 s): escala 1. A poeira sai do volume e
		// vira camada própria. Sem o corpo, a Carga Pesada na proporção medida.
		// a morte de tanque atual foi pro lixo (10/10, "muito ruim"): será refeita por partes no EmberGen
		{ TEXT("tankdeath"), TEXT("Morte de Tanque"), 1.0f, TEXT("munição do tanque · bola de ~9 m"), false, 1.0f, true,
			1.0f, 0.0f, 1.0f, TEXT("EgGasolineB"), TEXT("EgRing"), true },
		// A Carga Pesada simulada no EmberGen (sessão Projeto complexo, 10/10; 192³, 19.8 cm, a mesma
		// especificação da do WebGL): o clarão curto e o rolo largo de fumaça preta e oleosa de uma
		// explosão de combustível; o nome é a arma clássica do GLA. O corpo da Unreal (luz, clarão, chão)
		// é o da Carga Pesada; o raymarch toca a gravação dela e a luz segue a curva dela.
		// Toca a versão 96³ (Tools/brm2_downsample.py): na câmera do jogo igual à 192³ e 3× mais
		// barata (3 delas: 7,4 contra 22,6 ms, 11/10). Regravada no EmberGen em 11/10 (v4,
		// captures/embergen/carro_bomba_v2.ember): o calor dura mais (dissipação 25 → 8 %/s, difusão
		// 0,77 → 0,2 m/s) e a bola nasce do tamanho da física (esfera 3,2 → 2 m, pressão 40 → 20 %/s):
		// a fumaça sobe e sai pelo alto em vez de virar névoa do chão ao teto; 3 delas 7,3 → 5,3 ms.
		// v5f (11–12/10, captures/embergen/carro_bomba_v5f.ember): o fogo vivo dentro da fumaça preta até
		// ~1,2 s, como a arte (combustão avançada: o combustível dura mais e queima devagar, a fumaça nasce
		// da queima e não quente na fonte, emissor a 2600 K, a fumaça some aos ~3 s); o Felipe: "serve".
		// As anteriores ficam pra comparar (-RmSwap=eg_cb_v5f_96:eg_cb_v4_96).
		{ TEXT("carrobomba"), TEXT("Carro-Bomba"), 1.0f, TEXT("simulada no EmberGen"), false, 0.0f, false, 0.0f, 1.0f, 1.0f,
			nullptr, nullptr, false, TEXT("eg_cb_v5f_96") },
	};
	return Weapons;
}

bool UBlastLabSimulation::LoadAssets()
{
	if (Bakes.Num() && VolumeMaterial)
	{
		return true;
	}
	Bakes.Reset();
	for (const TCHAR* Path : BakePaths)
	{
		if (UExplosionBake* B = LoadObject<UExplosionBake>(nullptr, Path))
		{
			Bakes.Add(B);
		}
	}
	VolumeMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (!Bakes.Num() || !VolumeMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("BlastLab: faltam as explosões gravadas (rode Tools/blast.ps1 build-explosion)"));
		return false;
	}
	return true;
}

bool UBlastLabSimulation::LoadBody(const TCHAR* Body, UExplosionBake*& OutBake, UMaterialInterface*& OutMaterial)
{
	const FName Key(Body);
	if (!BodyBakes.Contains(Key))
	{
		BodyBakes.Add(Key, LoadObject<UExplosionBake>(nullptr, *FString::Printf(TEXT("/Game/Explosions/DA_%s.DA_%s"), Body, Body)));
		BodyMaterials.Add(Key, LoadObject<UMaterialInterface>(nullptr,
			*FString::Printf(TEXT("/Game/Explosions/M_Explosion_%s.M_Explosion_%s"), Body, Body)));
		if (!BodyBakes[Key] || !BodyMaterials[Key])
		{
			UE_LOG(LogTemp, Warning, TEXT("BlastLab: falta o corpo %s (rode build_explosion.py com BLAST_BAKE); usando a Carga Pesada"), Body);
		}
	}
	OutBake = BodyBakes[Key];
	OutMaterial = BodyMaterials[Key];
	return OutBake && OutMaterial;
}

AExplosionVolume* UBlastLabSimulation::Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, UMaterialInterface* Material)
{
	AExplosionVolume* E = GetWorld()->SpawnActorDeferred<AExplosionVolume>(AExplosionVolume::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Bake = Bake;
	E->SizeScale = SizeScale;
	E->Seed = FMath::FRand();
	E->Age = 0.0f;
	E->bSelfAdvance = true;
	E->Volume->SetMaterial(0, Material ? Material : VolumeMaterial.Get());
	E->FinishSpawning(FTransform(Ground));
	return E;
}

AExplosionVolume* UBlastLabSimulation::Detonate(const FVector& Ground, int32 Weapon)
{
	if (!Arsenal().IsValidIndex(Weapon) || !LoadAssets())
	{
		return nullptr;
	}
	const FBlastWeapon& W = Arsenal()[Weapon];
	const float Jitter = 1.0f + (FMath::FRand() - 0.5f) * SizeJitter;
	UExplosionBake* BodyBake = nullptr;
	UMaterialInterface* BodyMaterial = nullptr;
	float BallScale = W.BallScale, SkirtHeight = W.SkirtHeight, SkirtLife = W.SkirtLife;
	if (!W.Body || !LoadBody(W.Body, BodyBake, BodyMaterial))
	{
		BodyBake = Bakes[FMath::RandRange(0, Bakes.Num() - 1)];
		BodyMaterial = nullptr;
		if (W.Body)
		{
			// sem o corpo próprio: a Carga Pesada na proporção medida contra o
			// vídeo (07/10: bola 0.55, poeira com 40% da altura, metade da vida)
			BallScale = 0.55f;
			SkirtHeight = 0.4f;
			SkirtLife = 0.5f;
		}
	}
	const float Scale = BallScale > 0.0f ? BallScale : W.SizeScale();
	AExplosionVolume* E = Spawn(Ground, BodyBake, Scale * Jitter, BodyMaterial);
	if (E && W.TimeYield > 0.0f)
	{
		E->TimeScale = W.TimeScale() * Jitter;
	}
	if (E)
	{
		E->bNuclear = W.bNuclear;
		if (W.RaymarchSource)
		{
			E->RaymarchSource = FName(W.RaymarchSource);
		}
	}
	if (E && BallScale > 0.0f && !BodyMaterial)
	{
		// a bola gravada menor que o rendimento fica tão opaca quanto a Carga
		// Pesada aprovada (a fuligem por metro da gravação levaria a bola de 0.55
		// a meia opacidade, rala contra a bola cheia do vídeo)
		E->Thickness = 1.0f / BallScale;
	}
	if (E)
	{
		if (W.bTankDeath && BodyMaterial)
		{
			ApplyGasolineBallLook(E);
			// O primeiro segundo (painel, rodada f/p): a luz do clarão cai em 0.2–0.3 s e
			// acende o chão em volta (o "anel de luz" do vídeo é o chão iluminado, não um
			// disco desenhado; o núcleo branco de 14 m saiu em 10/10, ExplosionVolume.h bFlash)
			E->FlashTauS = 0.08f;
			E->FlashHeightCm = 100.0f;
		}
		E->SkirtHeight = SkirtHeight;
		E->SkirtLife = SkirtLife;
	}
	const float Energy = W.SizeScale() * Jitter;      // o choque e o empurrão: o rendimento, não o tamanho da bola
	UExplosionBake* DustBake = nullptr;
	UMaterialInterface* DustMaterial = nullptr;
	if (E && W.DustBody && LoadBody(W.DustBody, DustBake, DustMaterial))
	{
		// a poeira levantada: outro volume no mesmo pé, sem luz (entra antes da
		// explosão na lista: a última continua sendo a explosão, alvo das passagens)
		if (AExplosionVolume* Dust = Spawn(Ground, DustBake, Jitter, DustMaterial))
		{
			Dust->TimeScale = 1.0f;
			Dust->bLights = false;
			Dust->bFlash = false;
			Dust->SkirtHeight = 0.0f;
			Dust->Thickness = DustThickness;
			// no vídeo o anel é claro e RENTE, nunca cobre o casco: a gravação tem
			// ~3 m de poeira em pé; achatada a 0.3 fica no ~1 m de baixo
			Dust->HeightScale = DustHeightScale;
			// e o miolo livre: o casco (7.9 × 3.7 m) nunca fica coberto
			Dust->MaterialParams.Add(TEXT("HollowM"), DustHollowM);
			// só o céu acende a poeira (canal 2, BlastLabEnvironment SetVolumeChannel):
			// a luz do fogo de dentro da bola (canal 1) a acendia por dentro
			Dust->Volume->SetLightingChannels(false, false, true);
			Alive.Add(Dust);
		}
	}
	if (E && W.bTankDeath)
	{
		SpawnFlipbook(E, Ground);
		TankDeath(Ground);
	}
	if (E)
	{
		Alive.Add(E);
		LastBlastGround = Ground;
		bHasLastBlast = true;
		// o choque sai agora e chega ao ponto de mira em d/343 s
		FShock S;
		S.Ground = Ground;
		S.SizeScale = Energy;
		S.Weapon = Weapon;
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (const ABlastLabCamera* Cam = PC ? Cast<ABlastLabCamera>(PC->GetPawn()) : nullptr)
		{
			S.DistanceM = float(FVector::Dist(Ground, Cam->AimPoint()) / 100.0);
		}
		S.Delay = S.DistanceM / 343.0f;
		Shocks.Add(S);
		// a expansão empurra a fumaça das outras pra fora (main.js: a 2 m·escala
		// do chão); as que ela alcança passam a ser deformadas pelo campo. A
		// própria não: a expansão dela já está na gravação.
		// As camadas da fase 1 (ABlastEffects: clarão em bola, anel chapado, cubos,
		// decal de ruído) foram rejeitadas em 06/10: gráfico de protótipo, nada
		// parecido com as artes. Desligadas até serem refeitas com qualidade AAA.
		if (bLayersEnabled)
		{
			SpawnEffects(Ground, W);
		}
		if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
		{
			const FVector Center = Ground + FVector(0.0f, 0.0f, 200.0f * Energy);
			Battle->AddBlast(Center, Energy);
			const float ReachCm = UBattleSmokeSubsystem::BlastRadiusM(Energy) * 100.0f;
			for (AExplosionVolume* O : Alive)
			{
				if (O != E && IsValid(O) && O->SizeScale <= MaxWarpScale && Battle->Covers(O->GetActorLocation())
					&& O->SmokeBoxWorld().ExpandBy(ReachCm).IsInside(Center))
				{
					Battle->AttachWarp(O);
				}
			}
		}
	}
	return E;
}

namespace
{
	// A torre (painel da sessão de arte sobre a rodada f, 07/10): a 18 m/s na
	// vertical ela subia ~17 m e caía de volta EM CIMA do casco. Sobe a 12.5 m/s
	// (~8 m, 2.5 s no ar) e sai de lado a 3 m/s pro lado da câmera: pousa a ~1
	// tanque do casco, à vista, e não some atrás dele; gira ~300°/s em 2 eixos;
	// solta 2 quadros depois do clarão (a munição estoura, depois a torre sai);
	// quica uma vez e fica (é cobertura).
	constexpr float TurretUp = 1250.0f, TurretSide = 300.0f, TurretDelay = 0.067f, TurretSpinDeg = 300.0f, Gravity = 980.0f;
	// A carcaça queimando (docs/EXPLOSOES.md §7b): um foco de ~2.2 m de raio e
	// chama até ~3 m (a altura do casco), pulsando a 1.5/√D Hz (D ≈ 4 m: 0.75
	// Hz); a fuligem que ela solta vira a coluna preta pelo empuxo do campo.
	constexpr float WreckDelay = 0.25f, WreckDuration = 120.0f, WreckRadiusM = 2.2f, WreckFlameM = 3.0f;
	constexpr float WreckHeat = 0.85f, WreckSoot = 1.2f, WreckPulseHz = 0.75f;
	// O fogo da carcaça (P1 do Felipe, 07/10: "o fogo tem que VIRAR a fumaça"). O
	// loop do Mantaflow ficava em cima, do lado e atrás do casco, piscando, e não
	// virava fumaça (o Mantaflow não tem a expansão da combustão: não faz bolas).
	// No vídeo (6–10 s, captures/concept/40_feedback_0710/veo_fogo/tira_v3.jpg) o
	// fogo vem em BOLAS de couve-flor saindo do anel da torre, uma a cada ~0.3–0.5
	// s; cada uma sobe e em ~0.5–0.8 s ganha a crosta preta com rachaduras laranja
	// e vira a fumaça preta da coluna. Aqui: bolas pequenas da gravação da bola
	// grande (EgGasolineB já faz bola → crosta → fumaça), nascidas no meio do teto
	// do casco. No relógio da gravação (TimeScale 1): no da escala (1/0.42, 2.4×
	// mais rápido) a fase amarela durava ~0.2 s e a bola nascia já crosta e fumaça
	// (1ª rodada do P1); a 1× ela fica amarela ~0.5 s e a crosta cai em ~0.5–1.2 s
	constexpr float WreckBallStart = 0.8f;          // s depois da detonação (a bola grande ainda cobre o casco)
	constexpr float WreckBallScale = 0.42f;         // ×0.8–1.2: bola de ~3.8 m (~0.48 tanque); a grande (1.0) tem ~9 m
	constexpr float WreckBallSkipS = 0.3f;          // s da gravação: começa depois da esfera do clarão
	constexpr float WreckBallTimeScale = 1.0f;      // -WreckBallTime=x pra A/B
	constexpr float WreckBallEvery[2] = { 0.3f, 0.5f };
	// a munição acaba: de 8 a 30 s as bolas rareiam (até uma a cada ~2 s) e encolhem
	constexpr float WreckBallTireS[2] = { 8.0f, 30.0f };
	constexpr float WreckBallTiredEvery = 2.0f, WreckBallTiredScale = 0.7f;
	// a luz de cada bola no casco e no chão: a física da gravação (área ∝ escala²),
	// sem sombra (várias vivas ao mesmo tempo; uma luz pontual com sombra ~2 ms)
	constexpr float WreckBallLightScale = 1.0f;
	// a coluna (Smoke Plume em loop): entra com o chapéu da bola subindo (~1.4 s no
	// vídeo), cresce em 1.2 s
	constexpr float PlumeStart = 1.4f, PlumeRamp = 1.2f, PlumeScale = 0.85f, PlumeBaseClearM = 4.0f, PlumeTopFade = 0.45f;
	constexpr float PlumeSourceY = 8.5f;        // m locais: onde a fonte da Smoke Plume fica no cubo
	// a coluna: mais opaca e escura (painel: L 84–92 contra 35–60 no alvo; a
	// fuligem da plume vem rala do conversor) e, de 5 a 8 s, decai pra um penacho
	// a ~35% (a munição acabou; fica a carcaça queimando)
	constexpr float PlumeSootExtScale = 5.0f, PlumeTail = 0.35f;
	// O topo: a caixa dos dados vai a 14.5 m locais (a fumaça rala), a tampa densa
	// fica a ~13 m. Some até 11 m, a partir de 7 m (A/B de 07/10 na câmera baixa,
	// 14.5/13/12/11 m: com 12 o topo ainda saía reto aos 6 s; com 11 sai em fiapos).
	// Corta ~3 m do mundo do alto da coluna (escala 0.85)
	constexpr float PlumeTopCutM = 11.0f, PlumeTopCutFadeM = 4.0f;

	// o nome do render (gas_hero_agx) → a etiqueta dos assets (GasHeroAgx)
	FString FlipbookTag(const FString& Name)
	{
		TArray<FString> Parts;
		Name.ParseIntoArray(Parts, TEXT("_"));
		FString Tag;
		for (const FString& P : Parts)
		{
			Tag += P.Left(1).ToUpper() + P.Mid(1);
		}
		return Tag;
	}

	// -FlipbookSet=<nome do render> (ex. gas_hero_side): outro conjunto de atlas pra A/B
	FString FlipbookSetArg()
	{
		FString Set;
		FParse::Value(FCommandLine::Get(), TEXT("FlipbookSet="), Set);
		return Set;
	}
}

void UBlastLabSimulation::SpawnFlipbook(AExplosionVolume* E, const FVector& Ground)
{
	static const bool bFlipbook = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("BallFlipbook="), V); return V != 0; }();
	if (!bFlipbook)
	{
		return;
	}
	float ElevDeg = 90.0f;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			ElevDeg = -PC->PlayerCameraManager->GetCameraRotation().Pitch;
		}
	}
	const FString Set = FlipbookSetArg();
	if (Set.StartsWith(TEXT("sixway")))
	{
		// o 6-way (Tools/build_sixway.py; captures/concept/41_sixway/<conjunto>): a
		// bola renderizada sob 6 luzes e reacendida pela cena. 8×8 quadros a 24 qps
		// (bake_g1b.py), célula de 28 m com o centro 10 m acima do pé (sixway_meta.json)
		const FString Tag = FlipbookTag(Set);       // sixway_p1 → SixwayP1
		auto Load = [&Tag](const TCHAR* Kind)
		{
			const FString Asset = FString::Printf(TEXT("T_SixWay%s_%s"), *Tag.RightChop(6), Kind);
			return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Explosions/SixWay/%s.%s"), *Asset, *Asset));
		};
		UTexture* A = Load(TEXT("A"));
		UTexture* B = Load(TEXT("B"));
		UTexture* MV = Load(TEXT("MV"));
		if (ABlastFlipbook* Card = GetWorld()->SpawnActor<ABlastFlipbook>(ABlastFlipbook::StaticClass(), FTransform(Ground)))
		{
			Card->StartSixWay(A, B, MV, 8, 8, 64, 24.0f, 28.0f, 10.0f);
			E->bHideVolume = true;
			UE_LOG(LogTemp, Display, TEXT("BlastLab: 6-way %s (%s)"), *Set, A && B && MV ? TEXT("ok") : TEXT("faltam texturas"));
		}
		return;
	}
	const FString Name = !Set.IsEmpty() ? Set : ElevDeg < 25.0f ? FString(TEXT("gas_hero")) : FString(TEXT("gas_rts"));
	const FString Tag = FlipbookTag(Name);
	UTexture* Atlas = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Explosions/Flipbook/T_%s_Atlas.T_%s_Atlas"), *Tag, *Tag));
	// o atlas.json da sessão de arte (captures/concept/38_flipbook/<nome>/): grade,
	// quadros, qps e o tamanho do quadro; faltando, os valores combinados (8 × 6,
	// 42 quadros, 15 qps, 18 m, chão a 0.5 m)
	int32 Cols = 8, Rows = 6, Frames = 42;
	double Fps = 15.0, SizeM = 18.0, GroundM = 0.5;
	FString Text;
	// (o dos atlas separados fica na pasta <nome>_lit)
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../captures/concept/38_flipbook"));
	const FString JsonPath = FPaths::FileExists(Root / (Name + TEXT("_lit")) / TEXT("atlas.json")) ? Root / (Name + TEXT("_lit")) / TEXT("atlas.json")
		: Root / Name / TEXT("atlas.json");
	TSharedPtr<FJsonObject> J;
	if (FFileHelper::LoadFileToString(Text, *JsonPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
	{
		J->TryGetNumberField(TEXT("cols"), Cols);
		J->TryGetNumberField(TEXT("rows"), Rows);
		J->TryGetNumberField(TEXT("frames"), Frames);
		J->TryGetNumberField(TEXT("fps"), Fps);
		J->TryGetNumberField(TEXT("side_m"), SizeM);
		J->TryGetNumberField(TEXT("ground_m"), GroundM);      // (o atlas de 07/10 não traz: 0.5 m, combinado)
	}
	ABlastFlipbook* Card = Atlas ? GetWorld()->SpawnActor<ABlastFlipbook>(ABlastFlipbook::StaticClass(), FTransform(Ground)) : nullptr;
	if (!Card)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlastLab: flipbook %s não encontrado: fica o volume"), *Tag);
		return;
	}
	UTexture* Emission = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Explosions/Flipbook/T_%s_Emission.T_%s_Emission"), *Tag, *Tag));
	Card->Start(Atlas, Emission, Cols, Rows, Frames, float(Fps), float(SizeM), float(GroundM), Tag.EndsWith(TEXT("Agx")));
	E->bHideVolume = true;
	UE_LOG(LogTemp, Display, TEXT("BlastLab: flipbook %s (câmera a %.0f°): %d×%d, %d quadros a %.0f qps, %.0f m"), *Tag, ElevDeg, Cols, Rows, Frames, Fps, SizeM);
}

void UBlastLabSimulation::TankDeath(const FVector& Ground)
{
	// a vítima: as peças com as tags da cena herói até 15 m do ponto
	FThrown T;
	AActor* Pivot = nullptr;
	FVector Hull = Ground;
	FVector RoofCm = Ground;
	bool bHull = false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A->ActorHasTag(TEXT("HeroVictim")) || FVector::Dist2D(A->GetActorLocation(), Ground) > 1500.0)
		{
			continue;
		}
		if (A->ActorHasTag(TEXT("Hull")))
		{
			// o meio da caixa do casco (o pivô do modelo não fica no meio: o fogo
			// saía ao lado do casco) e o teto dele, o anel da torre
			FVector Center, Extent;
			A->GetActorBounds(false, Center, Extent);
			Hull = Center;
			RoofCm = FVector(Center.X, Center.Y, Center.Z + Extent.Z);
			bHull = true;
		}
		else if (A->ActorHasTag(TEXT("Turret")) || A->ActorHasTag(TEXT("Barrel")))
		{
			if (!Pivot || A->ActorHasTag(TEXT("Turret")))
			{
				Pivot = A;
			}
			T.Parts.Add({ A, FTransform::Identity });
		}
	}
	if (Pivot)
	{
		const FTransform PivotT = Pivot->GetActorTransform();
		for (FThrownPart& Part : T.Parts)
		{
			AActor* A = Part.Actor.Get();
			A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			if (USceneComponent* Root = A->GetRootComponent())
			{
				Root->SetMobility(EComponentMobility::Movable);
			}
			Part.Local = A->GetActorTransform().GetRelativeTransform(PivotT);
		}
		T.P = PivotT.GetLocation();
		T.Q = PivotT.GetRotation();
		// de lado NA TELA (perpendicular à linha de visão, pra esquerda ou pra
		// direita, como na arte 4.3): voando pra câmera ela passava na frente da
		// bola de 1.3 a 2 s e tapava a casca com brasas (rodada j). ±20° de sorteio
		FVector Side = PivotT.GetRotation().GetRightVector().GetSafeNormal2D();
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				const FVector View = (T.P - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal2D();
				Side = FVector::CrossProduct(FVector::UpVector, View).GetSafeNormal();
			}
		}
		Side = (FMath::RandBool() ? Side : -Side).RotateAngleAxis(FMath::FRandRange(-20.0f, 20.0f), FVector::UpVector);
		T.V = FVector(0.0f, 0.0f, TurretUp) + Side * TurretSide;
		// giro em dois eixos (cambalhota + rolamento), quase nada em torno da vertical
		T.Spin = FVector(FMath::FRandRange(0.6f, 1.0f) * (FMath::RandBool() ? 1.0f : -1.0f), FMath::FRandRange(0.4f, 0.8f), 0.1f).GetSafeNormal()
			* FMath::DegreesToRadians(TurretSpinDeg);
		T.Delay = TurretDelay;
	}
	FWreckFire W;
	W.FootCm = FVector(Hull.X, Hull.Y, BlastLabGround::HeightAt(GetWorld(), Hull.X, Hull.Y));
	if (!bHull)
	{
		// sem casco por perto (clique no chão vazio, teste do Felipe de 07/10): a
		// explosão e os destroços, mas nada de carcaça queimando sem carcaça
		if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			Debris->Start(W.FootCm + FVector(0.0, 0.0, 100.0), FMath::Rand());
		}
		UE_LOG(LogTemp, Display, TEXT("BlastLab: morte de tanque sem casco por perto em %s: só a explosão e os destroços"), *W.FootCm.ToString());
		return;
	}
	// o fogo da carcaça: as bolas nascem no teto do casco (UpdateTankDeaths)
	W.RoofCm = RoofCm;
	W.NextBallS = WreckBallStart;
	// A coluna preta: o loop de 3 s da Smoke Plume da JangaFX (BakeRaw/eg_plume_loop),
	// entrando quando o chapéu da bola sobe; a base rala nos ~4 m de baixo (vista de
	// cima ela cobria o tanque). Escala 0.85: a coluna de ~12 m vira ~10 m, 1.3
	// tanque (o alvo é 1.1–1.4)
	UExplosionBake* PlumeBake = nullptr;
	UMaterialInterface* PlumeMaterial = nullptr;
	if (LoadBody(TEXT("EgPlumeLoop"), PlumeBake, PlumeMaterial) && PlumeBake->FrameTimes.Num() > 1)
	{
		// A fonte da Smoke Plume não fica no meio do cubo: nasce a +8.5 m em y e
		// tomba ~55° pra −y com o vento do EmberGen (medido 07/10 nos dados). O pé
		// do volume vai pra trás pra fonte cair no casco, e ele gira pra tombar
		// pra longe da câmera do momento (a coluna fica de pé na tela; até o campo
		// ter vento próprio)
		float Yaw = 0.0f;
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				const FVector D = (W.FootCm - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal2D();
				Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y));
			}
		}
		const FVector SourceLocal(0.0, PlumeSourceY * PlumeScale * 100.0, 0.0);
		const FVector PlumeFoot = W.FootCm - FRotator(0.0f, Yaw, 0.0f).RotateVector(SourceLocal);
		if (AExplosionVolume* Plume = Spawn(PlumeFoot, PlumeBake, PlumeScale, PlumeMaterial))
		{
			Plume->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
			Plume->TimeScale = 1.0f;
			Plume->LoopPeriod = PlumeBake->FrameTimes.Last() + (PlumeBake->FrameTimes[1] - PlumeBake->FrameTimes[0]);
			Plume->LoopLife = PlumeStart + WreckDuration;
			Plume->Age = -PlumeStart;
			Plume->Opacity = 0.0f;
			Plume->bLights = false;
			Plume->bFlash = false;
			Plume->SkirtHeight = 0.0f;
			Plume->MaterialParams.Add(TEXT("BaseClearM"), PlumeBaseClearM / PlumeScale);
			Plume->MaterialParams.Add(TEXT("TopFade"), PlumeTopFade);       // ela sai pelo teto: some devagar, sem topo reto
			// Os dados acabam num corte reto (o teto do domínio do EmberGen): sem a
			// bola do campo por cima ele virava uma tampa. A fumaça se dissolve nos
			// TopCutFadeM abaixo de TopCutM (material)
			float TopCutM = PlumeTopCutM, TopCutFadeM = PlumeTopCutFadeM;
			// A/B: -PlumeTopCut=<m locais> e -PlumeTopCutFade=<m>
			FParse::Value(FCommandLine::Get(), TEXT("-PlumeTopCut="), TopCutM);
			FParse::Value(FCommandLine::Get(), TEXT("-PlumeTopCutFade="), TopCutFadeM);
			if (TopCutM > 0.0f)
			{
				Plume->MaterialParams.Add(TEXT("TopCutM"), TopCutM);
				Plume->MaterialParams.Add(TEXT("TopCutFadeM"), TopCutFadeM);
			}
			UE_LOG(LogTemp, Display, TEXT("BlastLab: coluna com o topo dos dados a %.1f m locais, dissolvendo em %.1f m"), TopCutM, TopCutFadeM);
			static const float PlumeExt = [] { float V = PlumeSootExtScale; FParse::Value(FCommandLine::Get(), TEXT("-PlumeSootExt="), V); return V; }();
			Plume->MaterialParams.Add(TEXT("SootExtScale"), PlumeExt);
			Alive.Add(Plume);
			W.Plume = Plume;
		}
	}
	// os destroços: chapas, cometas e o rastro da torre, saindo do alto do casco
	// (~1.8 m). Os vértices são do mundo (malha procedural): o ator fica na origem
	if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Debris->Start(W.FootCm + FVector(0.0, 0.0, 180.0), FMath::Rand());
		if (Pivot)
		{
			// a torre solta fuligem (albedo ~0.07): rastro de ~0.7 m que engorda 0.7 m/s
			// e se desfaz em 2.5 s. Sem o brilho de fogo na fita: visto de longe ele
			// virava um "sabre" laranja reto (painel, rodada f)
			T.Debris = Debris;
			T.Trail = Debris->AddFollowTrail(0.7f, 0.7f, 2.5f, 0.07f, 0.0f, 0.7f);
		}
	}
	if (Pivot)
	{
		Thrown.Add(T);
	}
	W.Age = -WreckDelay;
	Wrecks.Add(W);
	UE_LOG(LogTemp, Display, TEXT("BlastLab: morte de tanque, %d peças voando, carcaça queimando em %s"), T.Parts.Num(), *W.FootCm.ToString());
}

void UBlastLabSimulation::UpdateTankDeaths(float Dt)
{
	if (Dt <= 0.0f)
	{
		return;
	}
	for (FThrown& T : Thrown)
	{
		if (T.bRest)
		{
			continue;
		}
		if (T.Delay > 0.0f)
		{
			T.Delay -= Dt;      // ainda no casco: a munição estourou, a torre sai no quadro seguinte
			continue;
		}
		T.V.Z -= Gravity * Dt;
		T.P += T.V * Dt;
		const float SpinRad = T.Spin.Size() * Dt;
		if (SpinRad > 0.0f)
		{
			T.Q = FQuat(T.Spin.GetSafeNormal(), SpinRad) * T.Q;
		}
		const double Floor = BlastLabGround::HeightAt(GetWorld(), T.P.X, T.P.Y) + 60.0;
		if (T.P.Z < Floor && T.V.Z < 0.0f)
		{
			// quica uma vez, sem força, e fica
			T.P.Z = Floor;
			T.V = FVector(T.V.X * 0.4f, T.V.Y * 0.4f, -T.V.Z * 0.2f);
			T.Spin *= 0.35f;
			if (T.V.Size() < 250.0f)
			{
				T.bRest = true;
			}
		}
		const FTransform PivotT(T.Q, T.P);
		for (const FThrownPart& Part : T.Parts)
		{
			if (AActor* A = Part.Actor.Get())
			{
				A->SetActorTransform(Part.Local * PivotT);
			}
		}
		if (ABlastDebris* Debris = T.Debris.Get())
		{
			Debris->SetTrailHead(T.Trail, T.P, !T.bRest);
		}
	}
	UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this);
	for (int32 i = Wrecks.Num() - 1; i >= 0; --i)
	{
		FWreckFire& W = Wrecks[i];
		W.Age += Dt;
		if (W.Age > WreckDuration)
		{
			Wrecks.RemoveAtSwap(i);
			continue;
		}
		// as bolas do fogo entram e saem sem estalo (a gravação acaba de uma vez):
		// a esfera do clarão fica de fora, e o último quinto se desfaz
		W.Balls.RemoveAll([](const TWeakObjectPtr<AExplosionVolume>& B) { return !B.IsValid(); });
		for (const TWeakObjectPtr<AExplosionVolume>& B : W.Balls)
		{
			AExplosionVolume* E = B.Get();
			const float T = E->BakeTime(), Dur = E->Bake ? E->Bake->Duration() : 1.0f;
			E->Opacity = FMath::SmoothStep(WreckBallSkipS, WreckBallSkipS + 0.08f, T) * (1.0f - FMath::SmoothStep(0.8f * Dur, Dur, T));
		}
		if (W.Age < 0.0f)
		{
			continue;
		}
		// -WreckBalls=0: sem as bolas (pra ver o 6-way sozinho)
		static const bool bWreckBalls = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("WreckBalls="), V); return V != 0; }();
		if (bWreckBalls && W.RoofCm != FVector::ZeroVector && W.Age >= W.NextBallS && W.Age < 0.9f * WreckDuration)
		{
			SpawnWreckBall(W);
		}
		if (AExplosionVolume* Plume = W.Plume.Get())
		{
			Plume->Opacity = FMath::Clamp(Plume->Age / PlumeRamp, 0.0f, 1.0f)
				* FMath::Lerp(1.0f, PlumeTail, FMath::SmoothStep(5.0f, 8.0f, Plume->Age + PlumeStart))
				* (1.0f - FMath::SmoothStep(0.66f * WreckDuration, WreckDuration, Plume->Age));
			// Com a coluna, o fogo e a fumaça da carcaça são só as bolas e ela: nada
			// de fogo, calor ou fuligem do campo ali (a fuligem dele virava a bola
			// cinza-clara em cima da coluna; o fogo do campo, outra chama)
			continue;
		}
		if (!Battle)
		{
			continue;
		}
		// Sem a coluna (gravação faltando): o fogo e a fumaça pelo campo. Cresce em
		// ~1 s e fraqueja no último terço; a chama treme em duas frequências
		const float Life = FMath::Min(W.Age / 1.0f, 1.0f) * (1.0f - FMath::SmoothStep(0.66f * WreckDuration, WreckDuration, W.Age));
		const float Pulse = 1.0f + 0.35f * FMath::Sin(2.0f * PI * WreckPulseHz * W.Age) + 0.15f * FMath::Sin(2.0f * PI * 1.9f * W.Age + 1.3f);
		const FVector3f P(W.FootCm / 100.0);
		FBattleSmokeDisturber D;
		D.A = FVector4f(P.X, P.Y, P.Z, WreckRadiusM);
		D.B = FVector4f(P.X, P.Y, P.Z, 7.0f);
		D.C = FVector4f(WreckHeat * Life, 0.0f, 0.0f, WreckSoot * Life * Pulse);
		D.D = FVector4f(WreckFlameM, 0.0f, 0.0f, 0.0f);
		Battle->AddDisturber(D);
	}
}

void UBlastLabSimulation::SpawnWreckBall(FWreckFire& W)
{
	// a munição acabando: de 8 a 30 s as bolas rareiam e encolhem
	const float Tired = FMath::SmoothStep(WreckBallTireS[0], WreckBallTireS[1], W.Age);
	W.NextBallS = W.Age + FMath::Lerp(FMath::FRandRange(WreckBallEvery[0], WreckBallEvery[1]), WreckBallTiredEvery, Tired);
	UExplosionBake* Bake = nullptr;
	UMaterialInterface* Material = nullptr;
	if (!LoadBody(TEXT("EgGasolineB"), Bake, Material))
	{
		return;
	}
	const float Scale = WreckBallScale * FMath::FRandRange(0.8f, 1.2f) * FMath::Lerp(1.0f, WreckBallTiredScale, Tired);
	AExplosionVolume* E = Spawn(W.RoofCm, Bake, Scale, Material);
	if (!E)
	{
		return;
	}
	E->SetActorRotation(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
	ApplyGasolineBallLook(E);
	static const float BallTime = [] { float V = WreckBallTimeScale; FParse::Value(FCommandLine::Get(), TEXT("-WreckBallTime="), V); return V; }();
	E->TimeScale = BallTime;
	E->Age = WreckBallSkipS * BallTime;     // começa depois da esfera do clarão (BakeTime = Age / TimeScale)
	E->Opacity = 0.0f;
	E->Thickness = 1.0f / Scale;            // tão opaca por metro quanto a bola grande (a fuligem gravada é por voxel)
	E->bFlash = false;
	E->LightScale = WreckBallLightScale;
	E->bSceneShadow = false;
	E->SkirtHeight = 0.0f;                  // sem a saia de poeira
	E->StreamMip = 1.0f;                    // ~8 cm de voxel no mundo, como a bola grande no mip 0
	Alive.Add(E);
	W.Balls.Add(E);
}

void UBlastLabSimulation::ShockArrived(const FShock& S)
{
	// Tremor: a Carga Pesada no ponto de mira dá ~0.4% da altura da tela
	// (trauma 0.36 → 0.36²·1° = 0.13°), a nuclear ~3% (trauma 1); cai com a
	// distância como 1/(1 + d/R), R = 60 m·W^(1/3) (docs/CONCEITO_EXPLOSOES.md,
	// pesquisa 05: tiro 0.15%, tanque 0.4%, prédio 0.8%, nuke 3%)
	const float Base = FMath::Min(1.0f, 0.36f * FMath::Pow(S.SizeScale, 0.47f));
	const float Trauma = Base / (1.0f + S.DistanceM / (60.0f * S.SizeScale));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (ABlastLabCamera* Cam = PC ? Cast<ABlastLabCamera>(PC->GetPawn()) : nullptr)
	{
		Cam->AddTrauma(Trauma);
	}
	// Som: o gancho. A lista de sons por evento e a mixagem vêm da sessão de arte
	// (C11: estalo, corpo grave, destroços, cauda; o resto da mixagem abaixa;
	// vozes nunca cortadas); os arquivos só com o ok do Felipe pro download.
	UE_LOG(LogTemp, Display, TEXT("BlastLab: choque de %s chegou a %.0f m (%.2f s): tremor %.2f, som (gancho)"),
		Arsenal().IsValidIndex(S.Weapon) ? Arsenal()[S.Weapon].Id : TEXT("?"), S.DistanceM, S.DistanceM / 343.0f, Trauma);
}

void UBlastLabSimulation::SpawnEffects(const FVector& Ground, const FBlastWeapon& W)
{
	// o chão no ponto: o material físico de quem está embaixo (PM_Areia, PM_Asfalto...).
	// O traço sai de meio metro acima: de 2 m ele batia nos destroços do cenário
	FString SurfaceName = TEXT("terra");
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BlastSurface), true);
	Q.bReturnPhysicalMaterial = true;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Ground + FVector(0, 0, 50), Ground - FVector(0, 0, 500), ECC_Visibility, Q)
		&& Hit.PhysMaterial.IsValid())
	{
		SurfaceName = Hit.PhysMaterial->GetName();
	}
	ABlastEffects* Fx = GetWorld()->SpawnActorDeferred<ABlastEffects>(ABlastEffects::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Fx)
	{
		Fx->FinishSpawning(FTransform(Ground));
		Fx->Start(FBlastRecipes::Get(W.Id), FBlastRecipes::Surface(SurfaceName), FMath::Rand());
	}
	UE_LOG(LogTemp, Display, TEXT("BlastLab: %s no chão %s"), W.Id, *SurfaceName);
}

int32 UBlastLabSimulation::PendingShaders()
{
	return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
}

void UBlastLabSimulation::Prewarm(const FVector& At)
{
	if (!LoadAssets())
	{
		return;
	}
	MoverMaterials.Reset();
	// o teste do flipbook: os atlas montam na carga, não na detonação (na 1ª rodada
	// a textura era montada no clique e os primeiros quadros sumiam). Só o conjunto
	// em uso: os HDR têm 100 MB cada e os sete juntos estouravam a memória de vídeo
	// (o aviso vermelho de streaming na rodada agx2 de 07/10)
	if (FParse::Param(FCommandLine::Get(), TEXT("BallFlipbook")) || FCString::Strifind(FCommandLine::Get(), TEXT("BallFlipbook=1")))
	{
		const FString Set = FlipbookSetArg();
		TArray<FString> Tags;
		if (Set.IsEmpty())
		{
			Tags = { TEXT("GasHero"), TEXT("GasRts") };    // a câmera escolhe na detonação
		}
		else
		{
			Tags.Add(FlipbookTag(Set));
		}
		for (const FString& Tag : Tags)
		{
			for (const TCHAR* Kind : { TEXT("Atlas"), TEXT("Emission") })
			{
				// (o atlas de cor de tela não tem emissão separada)
				const FString Package = FString::Printf(TEXT("/Game/Explosions/Flipbook/T_%s_%s"), *Tag, Kind);
				if (FPackageName::DoesPackageExist(Package))
				{
					if (UTexture* T = LoadObject<UTexture>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package))))
					{
						FlipbookAtlases.Add(T);
					}
				}
			}
		}
	}
	// os do avião/míssil/disparo e os das camadas da explosão (BlastEffects): compilam na abertura
	for (const TCHAR* Path : { TEXT("/Game/Materials/M_Emissive.M_Emissive"), TEXT("/Game/Materials/M_Glow.M_Glow"),
		TEXT("/Game/Materials/M_GroundRing.M_GroundRing"), TEXT("/Game/Materials/M_Scorch.M_Scorch") })
	{
		MoverMaterials.Add(LoadObject<UMaterialInterface>(nullptr, Path));
	}
	TArray<TPair<UExplosionBake*, UMaterialInterface*>> Bodies;
	for (UExplosionBake* B : Bakes)
	{
		Bodies.Emplace(B, nullptr);
	}
	TArray<const TCHAR*> BodyNames = { TEXT("EgPlumeLoop") };      // a coluna da carcaça (TankDeath; o fogo dela são bolas da EgGasolineB)
	for (const FBlastWeapon& W : Arsenal())
	{
		for (const TCHAR* Name : { W.Body, W.DustBody })
		{
			if (Name)
			{
				BodyNames.AddUnique(Name);
			}
		}
	}
	for (const TCHAR* Name : BodyNames)
	{
		UExplosionBake* B = nullptr;
		UMaterialInterface* M = nullptr;
		if (LoadBody(Name, B, M))
		{
			Bodies.Emplace(B, M);
		}
	}
	// a torre e o cano da vítima voam como móveis: a Unreal compila os shaders de
	// peça móvel no primeiro quadro em que ela se mexe ("Preparing Shaders" aos
	// 0.07 s da morte de tanque, 07/10); móveis desde já, compilam no aquecimento
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if ((*It)->ActorHasTag(TEXT("HeroVictim")) && ((*It)->ActorHasTag(TEXT("Turret")) || (*It)->ActorHasTag(TEXT("Barrel"))))
		{
			if (USceneComponent* Root = (*It)->GetRootComponent())
			{
				Root->SetMobility(EComponentMobility::Movable);
			}
		}
	}
	for (const TPair<UExplosionBake*, UMaterialInterface*>& Body : Bodies)
	{
		if (AExplosionVolume* E = Spawn(At, Body.Key, 1.0f, Body.Value))
		{
			E->Opacity = 0.0f;
			E->Age = 0.3f;                 // já com bola de fogo: o material inteiro em uso
			E->bSelfAdvance = false;       // parada até os shaders ficarem prontos
			Warmups.Add(E);
		}
	}
	// as camadas também (anel, marca, pedaços): minúsculas na frente da câmera, os
	// shaders delas compilam junto (a Unreal compila sob demanda, no primeiro desenho)
	FBlastRecipe Tiny;
	Tiny.FlashRadiusM = 0.01f;
	Tiny.RingRadiusM = 0.05f;
	Tiny.RingDelayMs = 0.0f;
	Tiny.RingMs = 20000.0f;
	Tiny.ScorchRadiusM = 0.05f;
	Tiny.ScorchLifeS = 20.0f;
	Tiny.Debris = Tiny.Clods = Tiny.Sparks = 1;
	Tiny.DebrisSpeed = Tiny.ClodSpeed = Tiny.SparkSpeed = FBlastRange{ 0.0f, 0.0f };
	Tiny.DebrisSizeM = Tiny.ClodSizeM = FBlastRange{ 0.01f, 0.01f };
	Tiny.MinPixels = 0.0f;
	if (ABlastEffects* Fx = bLayersEnabled ? GetWorld()->SpawnActor<ABlastEffects>(ABlastEffects::StaticClass(), FTransform(At)) : nullptr)
	{
		Fx->Start(Tiny, FBlastSurface(), 1);
		Fx->SetLifeSpan(30.0f);
	}
	// os pedaços da morte de tanque (chapas, brasas, rastros) também: os materiais
	// deles estreavam no 1º quadro da explosão e a Unreal montava o PSO ali (quadro
	// de 140 ms aos 0.38 s, 07/10). Mil vezes menores em volta de um ponto a 10 m
	// do chão (os vértices são do mundo: a escala do ator encolhe em volta dele)
	const FVector DebrisAt = At + FVector(0.0, 0.0, 1000.0);
	if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(),
		FTransform(FQuat::Identity, DebrisAt * (1.0 - 0.001), FVector(0.001))))
	{
		Debris->Start(DebrisAt, 1);
		Debris->SetLifeSpan(30.0f);
	}
	WarmupClock = 0.0f;
}

void UBlastLabSimulation::UpdateView()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	bHasView = false;
	if (!PC || !PC->PlayerCameraManager || !Viewport)
	{
		return;
	}
	FVector2D Size;
	Viewport->GetViewportSize(Size);
	if (Size.X < 1.0 || Size.Y < 1.0)
	{
		return;
	}
	const FRotationMatrix M(PC->PlayerCameraManager->GetCameraRotation());
	ViewLocation = PC->PlayerCameraManager->GetCameraLocation();
	ViewForward = M.GetScaledAxis(EAxis::X);
	ViewRight = M.GetScaledAxis(EAxis::Y);
	ViewUp = M.GetScaledAxis(EAxis::Z);
	// a câmera guarda o fov horizontal de 16:9 com o vertical fixo (BlastLabCamera)
	TanHalfY = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f) * 9.0f / 16.0f;
	TanHalfX = TanHalfY * float(Size.X / Size.Y);
	ViewHeightPx = float(Size.Y);
	bHasView = true;
	if (const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
	{
		CachedAmbientLux = Env->RenderLux();
	}
}

float UBlastLabSimulation::RadiansPerPixel() const
{
	// o render é menor que a janela (o TSR amplia): o pixel que conta é o dele
	static const IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	const float Pct = ScreenPercentage ? FMath::Clamp(ScreenPercentage->GetFloat(), 10.0f, 200.0f) : 100.0f;
	return 2.0f * TanHalfY / FMath::Max(ViewHeightPx * Pct / 100.0f, 1.0f);
}

float UBlastLabSimulation::ScreenFraction(const FVector& Center, float Radius, bool& bInView) const
{
	bInView = true;
	if (!bHasView)
	{
		return 1.0f;
	}
	// esfera contra as 4 faces e o plano da câmera, nos eixos da vista
	const FVector P = Center - ViewLocation;
	const float Z = FVector::DotProduct(P, ViewForward);
	const float X = FVector::DotProduct(P, ViewRight);
	const float Y = FVector::DotProduct(P, ViewUp);
	const float KX = Radius * FMath::Sqrt(1.0f + TanHalfX * TanHalfX);
	const float KY = Radius * FMath::Sqrt(1.0f + TanHalfY * TanHalfY);
	bInView = Z > -Radius && FMath::Abs(X) - Z * TanHalfX <= KX && FMath::Abs(Y) - Z * TanHalfY <= KY;
	if (!bInView)
	{
		return 0.0f;
	}
	// dentro dela (ou quase): ocupa a tela toda
	return Z <= Radius ? 1.0f : FMath::Min(Radius / (Z * TanHalfY), 1.0f);
}

void UBlastLabSimulation::Tick(float DeltaTime)
{
	UpdateView();
	UpdateTankDeaths(SimDelta(DeltaTime));
	// os choques a caminho do ponto de mira (relógio do simulador)
	for (int32 i = Shocks.Num() - 1; i >= 0; --i)
	{
		Shocks[i].Delay -= SimDelta(DeltaTime);
		if (Shocks[i].Delay <= 0.0f)
		{
			const FShock S = Shocks[i];
			Shocks.RemoveAtSwap(i);
			ShockArrived(S);
		}
	}
	// as receitas (Config/BlastRecipes.json) são relidas quando o arquivo muda
	RecipeClock += DeltaTime;
	if (RecipeClock > 0.5f)
	{
		RecipeClock = 0.0f;
		FBlastRecipes::Poll();
	}
	// aquecimento: sai quando os shaders acabaram de compilar (e o volume já
	// desenhou alguns quadros), ou em 60 s de qualquer jeito
	if (Warmups.Num())
	{
		WarmupClock += DeltaTime;
		if ((PendingShaders() == 0 && WarmupClock > 2.0f) || WarmupClock > 60.0f)
		{
			for (AExplosionVolume* E : Warmups)
			{
				if (IsValid(E))
				{
					E->Destroy();
				}
			}
			Warmups.Reset();
		}
	}
	// a gravação acabou: a explosão sai de cena (o WebGL fazia o mesmo)
	for (int32 i = Alive.Num() - 1; i >= 0; --i)
	{
		AExplosionVolume* E = Alive[i];
		if (!IsValid(E) || E->IsFinished())
		{
			if (IsValid(E))
			{
				E->Destroy();
			}
			Alive.RemoveAtSwap(i);
		}
	}
	// Orçamento de detalhe (como o Significance Manager): as mais importantes
	// na tela (as maiores) ficam com tudo; passando delas, as seguintes descem
	// um nível e o resto dois. Medido 05/10 com 8 Cargas Pesadas na tela: o
	// volume custava 15–25 ms de GPU, e as que estão atrás das outras quase não
	// aparecem.
	TArray<AExplosionVolume*, TInlineAllocator<32>> Seen;
	for (AExplosionVolume* E : Alive)
	{
		if (E->IsOnScreen())
		{
			Seen.Add(E);
		}
		else
		{
			E->LodFloor = 0;
		}
	}
	Seen.Sort([](const AExplosionVolume& A, const AExplosionVolume& B) { return A.ScreenFraction() > B.ScreenFraction(); });
	for (int32 i = 0; i < Seen.Num(); ++i)
	{
		Seen[i]->LodFloor = i < FullDetailBudget ? 0 : (i < 2 * FullDetailBudget ? 1 : 2);
	}

	// Interação com o que passa: uma explosão que algo atravessa passa a ser
	// deformada pelo vento do campo até o fim dela (UBattleSmokeSubsystem
	// AttachWarp), sem trocar de fumaça. Fora do campo coberto pela grade, ou
	// grande demais pra ele (a nuclear), ela toca como foi gravada.
	if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
	{
		for (AExplosionVolume* E : Alive)
		{
			if (!E->IsWarped() && E->SizeScale <= MaxWarpScale && Battle->Covers(E->GetActorLocation()) && CrossedThisFrame(E))
			{
				Battle->AttachWarp(E);
			}
		}
	}
	MoverSegments.Reset();
	Movers.RemoveAll([](const TWeakObjectPtr<ABlastLabMover>& M) { return !M.IsValid(); });
}

void UBlastLabSimulation::AddMoverSegment(const FVector& FromCm, const FVector& ToCm, float RadiusCm)
{
	MoverSegments.Add({ FromCm, ToCm, RadiusCm });
}

ABlastLabMover* UBlastLabSimulation::SpawnMover(EBlastMover Kind)
{
	// o alvo: o pé da última explosão; sem nenhuma, o chão no centro da tela
	FVector Target = LastBlastGround;
	if (!bHasLastBlast)
	{
		if (!bHasView || !BlastLabGround::Ray(GetWorld(), ViewLocation, ViewForward, 1.0e6, Target))
		{
			Target = FVector(0.0, 0.0, BlastLabGround::HeightAt(GetWorld(), 0.0, 0.0));
		}
	}
	// cruza a tela da esquerda pra direita
	FVector Side = FVector(ViewRight.X, ViewRight.Y, 0.0f).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}
	// Passa pela altura média da fuligem da explosão ali (gravada quadro a
	// quadro, Tools/export_bake_velocity.py; battlesmoke.js smokeHeightAt). O meio
	// da caixa não serve: a poeira rente ao chão a puxa pra baixo e o avião
	// passava por baixo da nuvem. Sem explosão ali, a fuligem do campo, se houver.
	float AltM = -1.0f;
	for (int32 i = Alive.Num() - 1; i >= 0; --i)
	{
		const AExplosionVolume* E = Alive[i];
		if (IsValid(E) && FVector::Dist2D(E->GetActorLocation(), Target) < 2000.0f * FMath::Max(E->SizeScale, 1.0f))
		{
			// onde a fuligem vai estar quando ele chegar (a nuvem sobe ~10 m/s no começo)
			const float Now = (E->SootHeightWorld() - Target.Z) / 100.0f;
			const float Ahead = ABlastLabMover::TimeToTarget(Kind, Now);
			AltM = (E->SootHeightWorld(Ahead) - Target.Z) / 100.0f;
			AltM = (E->SootHeightWorld(ABlastLabMover::TimeToTarget(Kind, AltM)) - Target.Z) / 100.0f;
			break;
		}
	}
	FBox Box;
	float SootCm = 0.0f;
	const UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this);
	if (AltM < 0.0f && Battle && Battle->SmokeBox(Box) && Box.ExpandBy(FVector(800.0f, 800.0f, 0.0f)).IsInsideXY(Target) && Battle->SootHeight(SootCm))
	{
		AltM = SootCm / 100.0f;
	}
	UE_LOG(LogTemp, Display, TEXT("BlastLab: passagem %d em %s, altura da fumaça %.1f m"), int32(Kind), *Target.ToString(), AltM);
	ABlastLabMover* M = ABlastLabMover::Launch(GetWorld(), Kind, Target, Side, AltM);
	if (M)
	{
		Movers.Add(M);
	}
	return M;
}

bool UBlastLabSimulation::IsMoverActive(EBlastMover Kind) const
{
	for (const TWeakObjectPtr<ABlastLabMover>& M : Movers)
	{
		if (M.IsValid() && M->GetKind() == Kind)
		{
			return true;
		}
	}
	return false;
}

bool UBlastLabSimulation::CrossedThisFrame(const AExplosionVolume* E) const
{
	// algum objeto deste quadro passou pela caixa da fumaça dela (com folga do raio)? (battlesmoke.js _crossed)
	if (!MoverSegments.Num())
	{
		return false;
	}
	const FBox Box = E->SmokeBoxWorld();
	for (const FMoverSegment& S : MoverSegments)
	{
		const FBox B = Box.ExpandBy(S.RadiusCm);
		FVector Hit, Normal;
		float Time;
		if (B.IsInside(S.From) || B.IsInside(S.To) || FMath::LineExtentBoxIntersection(B, S.From, S.To, FVector::ZeroVector, Hit, Normal, Time))
		{
			return true;
		}
	}
	return false;
}
