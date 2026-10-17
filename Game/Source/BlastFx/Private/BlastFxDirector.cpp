#include "BlastFxDirector.h"

#include "BlastDebris.h"
#include "BlastRaymarchSubsystem.h"
#include "RTSSceneLights.h"
#include "RTSSunShadow.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/UObjectIterator.h"
#include "BlastFxGround.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "ProfilingDebugging/CsvProfiler.h"

// as mortes no perfil (-csvGpuStats -csvCaptureFrames=N): um evento por morte de tanque, pra achar o quadro
CSV_DEFINE_CATEGORY(BlastFxDirector, true);

namespace
{
	const TCHAR* BakePaths[] = {
		TEXT("/Game/Explosions/DA_HeavyCharge_v0.DA_HeavyCharge_v0"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v1.DA_HeavyCharge_v1"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v2.DA_HeavyCharge_v2"),
	};
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_Explosion.M_Explosion");

	// A morte de tanque sem o corpo do EmberGen, na proporção medida contra o
	// vídeo da cena herói (BlastLabSimulation.cpp Detonate, 07/10): bola 0.55,
	// poeira com 40% da altura e metade da vida, a gravação no relógio dela.
	// (Desde 13/10 a morte de tanque é a nova, abaixo; estas ficam pro aquecimento e pra volta, -TankBlast=0.)
	constexpr float TankBallScale = 0.55f;

	// A morte de tanque da v2 (o Felipe, 13/10: "ficaremos desse jeito pra explosão de tanques"; as construções mantêm
	// a antiga): a gravação do EmberGen do BlastLab (eg_tq_v2: a bola e a areia que sobe), do tamanho e no ritmo medidos
	// contra os game shots (escala 0,35 do Carro-Bomba, relógio 0,59 = Froude), tão opaca quanto a gravação, com a
	// fuligem cinza-carvão das colunas de tanque dos game shots e a luz do fim de tarde: o sol inteiro, a luz do céu
	// física e a areia que devolve o sol (BlastLab: -RmSwap=eg_cb_v5f_96:eg_tq_v2 -BlastScale=0.35 -BlastTime=0.59
	// -RmSoot=1.0,0.97,0.94,0.30 -RmDust=0.90,0.72,0.50,0.80 -RmFullSun=1 -RmSkyGain=6.28 -RmGround=0.50,0.38,0.26).
	// Sem o fogo da carcaça (fica pra v3).
	constexpr float TankBlastScale = 0.35f, TankBlastTime = 0.59f;
	const FVector4f TankSoot(1.0f, 0.97f, 0.94f, 0.30f), TankDust(0.90f, 0.72f, 0.50f, 0.80f);
	constexpr float TankSunFull = 1.0f, TankSkyGain = 6.28f;
	const FVector4f TankGround(0.50f, 0.38f, 0.26f, 1.0f);
	// os destroços da v2 (BlastDebris, 14/10: "rastro de fumaça também, é tudo que a gente homologou"): chapas, cometas
	// com rastro da cor da fuligem e faíscas do alto do casco (~1,8 m), como no vídeo aprovado (-DebrisGlow=0.15: as
	// cabeças e faíscas laranja de dia, não brancas), e o rastro escuro da torre arremessada (BlastLabSimulation.cpp)
	constexpr float DebrisHeightCm = 180.0f, DebrisGlow = 0.15f;
	// O tiro (14/10, a v3; números de partida, a conferir no jogo contra a arte 4.2 e os game shots: docs/TIRO_E_ACERTO.md).
	// A areia é a DustImpact da JangaFX (40 m de domínio): a escala dá o tamanho, o relógio a pressa.
	const TCHAR* DustSourceName = TEXT("jg_dust_impact_96");
	// (17/10, a falha crítica que o Felipe viu: 6 tanques atirando juntos, metade dos disparos nem nascia e os que nasciam
	// sumiam quando os tiros caíam na areia) cada tiro gasta 2–3 (o disparo, a areia do sopro, o tiro na areia): 12
	// (17/10 tarde, de novo com 6 tanques: com 3–4 por tiro, 12 lotava no 4º) 24, com o raymarch em 32 por quadro
	constexpr int32 MaxDustsHigh = 24;               // no Alta/Ultra; Média metade, Baixa 1
	// o brilho das cabeças e faíscas (× os 2·10⁵ cd/m² do M_DebrisGlow). 16/10, no jogo: com 1 tudo estourava em branco
	// (as línguas e a bola laranja do disparo, o traçante laranja); a morte usa 0,15 pras faíscas laranja. O acerto
	// fica mais alto: o miolo do metal é branco-quente na arte 4.2
	// (16/10) na exposição de referência dos destroços (BlastDebris, a compensação): o acerto a 0,8 é o 0,25 sem compensar
	// da fx_acerto_v8, cuja cor dos riscos bateu com a 4.2 (matiz 41°, saturação 0,07)
	// (16/10, depois da correção da conversão sRGB dos brilhos: a cor do miolo ficou igual e a área acesa do leque caiu 30%,
	// srgb_acerto × fx_acerto_v10; 0,8 → 1,1 devolve a área com a cor mais quente)
	constexpr float MuzzleGlow = 0.3f, TracerGlow = 0.3f, ImpactGlow = 1.1f;
	// a bola do disparo no raymarch: a eg_tq_v2 (38 m) a 0,07 (~2,7 m), tocada a 0,18 (~1 s inteira, o fogo nos primeiros
	// ~0,15–0,25 s), 1,6 m à frente da boca; a fumaça do propelente cinza-clara (não a fuligem preta da morte)
	constexpr float MuzzleBallScale = 0.07f, MuzzleBallTime = 0.18f, MuzzleBallThickness = 6.0f;
	constexpr float MuzzleBallAheadCm = 160.0f, MuzzleBallDropCm = 80.0f;
	// (16/10) o fogo nos primeiros 0,6 s da gravação (~0,11 s de verdade: o clarão secundário de um canhão grande dura
	// dezenas de ms, Klingenberg & Heimerl, "Gun Muzzle Blast and Flash", AIAA 1992), e o resto (a fumaça) a 0,35, ~2 s
	// (as gravações têm ~6 s, medido no registro de 16/10: a 0,65 a fumaça ficava 3,5 s);
	// a fumaça cinza-areia (a 0,92 ela saía branca ao sol, a arte é cinza-bege)
	// (A/B no jogo, 16/10: emissão 1 / 0,55 / 2 com a fumaça 0,74 ou 0,60: com menos fogo saía mais creme, com 2 e a fumaça
	// cinza o fogo aparece amarelo-laranja nos primeiros 0,15 s e a fumaça sai cinza-acastanhada; captures/game/fx_disparo_v6*)
	constexpr float MuzzleBallSlowAfterS = 0.6f, MuzzleBallSlowTime = 0.35f, MuzzleBallEmit = 2.0f;
	const FVector4f MuzzleSmokeSoot(0.60f, 0.57f, 0.52f, 0.40f), MuzzleSmokeDust(0.78f, 0.74f, 0.68f, 0.6f);
	// a areia do sopro: o golpe nos primeiros 0,5 s da gravação (0,25 s de verdade) e o resto a 0,35 (~2 s pairando;
	// sumia em ~0,8 s como um disco, e a 0,9 ficava 5,3 s)
	constexpr float MuzzleDustSlowAfterS = 0.5f, MuzzleDustSlowTime = 0.35f;
	// (v7, 17/10) o disparo simulado (BlastLab/Tools/blender_pyro_muzzle.py, sim_boca; o item 8 da fila, contra o vídeo de
	// referência do Felipe): o jato pelo eixo do cano (sem freio de boca), a bola do clarão secundário a ~1–4 m da boca e a
	// fumaça do propelente. A boca fica no meio da gravação, 2,3 m acima do chão dela, e a gravação gira pro rumo do cano.
	// Os dois andamentos da simulação: os 36 primeiros quadros (1,2 s da gravação) em câmera lenta 0,25× (o fogo, ~0,13 s
	// de verdade) e o resto em tempo real
	const TCHAR* MuzzleSimSource = TEXT("sim_boca");
	// (17/10, A/B no jogo, disp_v7b–d: a gravação com a fumaça ×3 e a curva 0,6 na conversão; a ×8 saía preta e opaca)
	// a fumaça do propelente cinza-clara (albedo 0,75: a da pólvora, não a fuligem) e o fogo a 4 (a bola branco-amarela dos
	// primeiros ~0,07 s, como no vídeo de referência); a fumaça depois a 1,6 (some em ~1 s em vez de ~0,5)
	// (17/10, o Felipe: "grande, lento e formato errado"; o vídeo de referência medido quadro a quadro: a bola de ~1,5 m colada
	// na boca por ~0,2 s e um sopro pequeno e ralo que some por volta de 0,5 s, sem nuvem) a gravação a 0,4 (a bola de ~3–4 m
	// vira ~1,5 m; a fumaça fica rala sozinha, a espessura óptica cai com o tamanho), o fogo a 0,1 (~0,12 s) e o resto a 0,2
	// (~0,35 s): tudo some em ~0,5 s. -ShotFxSimScale=, -ShotFxSimFast=, -ShotFxSimSlow= trocam, pro A/B
	// (17/10, rodadas disp_v8a–e contra o vídeo: a 0,1 a bola durava ~0,07 s; com o fogo a 0,4 ela fica ~0,2 s como no vídeo;
	// a fumaça a 0,3 de espessura e albedo 0,85 sai clara e rala, sumindo antes de 1 s; o fogo a 8 pra bola branco-amarela)
	constexpr float MuzzleSimHeightCm = 230.0f, MuzzleSimFastTime = 0.4f, MuzzleSimSlowAfterS = 1.2f, MuzzleSimThickness = 0.3f, MuzzleSimEmit = 8.0f;
	constexpr float MuzzleSimSlowTime = 0.15f, MuzzleSimScale = 0.4f;
	// (17/10 tarde, o Felipe no vídeo de perto: "na hora que ele vai desfazer ele infla do nada"; quadro a quadro, aos 0,6 s a
	// fumaça dobrava em 2 quadros e sumia no seguinte) o 2º andamento a 0,15 atravessava o resto da simulação a 6,7×: a
	// fumaça se espalhando de 2 s de gravação em 0,25 s. Agora a gravação toca num andamento só (0,4) e a fumaça some pela
	// opacidade entre 0,3 e 0,7 s, rala aos poucos (o vídeo de referência: o sopro ralo some por volta de 0,5 s).
	// -ShotFxSimLife= e -ShotFxSimFade= trocam, pro A/B
	constexpr float MuzzleSimLifeS = 0.7f, MuzzleSimFadeFromS = 0.3f;
	const FVector4f MuzzleSimSoot(0.85f, 0.82f, 0.78f, 0.85f);

	float CmdFloat(const TCHAR* Key, float Default)
	{
		float V = Default;
		FParse::Value(FCommandLine::Get(), Key, V);
		return V;
	}

	FVector4f CmdColor(const TCHAR* Key, const FVector4f& Default)
	{
		FString S;
		if (!FParse::Value(FCommandLine::Get(), Key, S, false))
		{
			return Default;
		}
		TArray<FString> P;
		S.ParseIntoArray(P, TEXT(","));
		return P.Num() == 4 ? FVector4f(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3])) : Default;
	}
	constexpr float MuzzleDustAheadCm = 150.0f, MuzzleDustScale = 0.15f, MuzzleDustTime = 0.5f, MuzzleDustOpacity = 0.6f;
	constexpr float HitHeightMinCm = 150.0f, HitHeightMaxCm = 230.0f;   // do alto do casco à torre (a arte acerta a bochecha dela)
	// 16/10, no jogo contra a arte 4.2: com 0,18/0,7 a areia em volta quase não aparecia e a baforada (0,07/0,45) era um fio
	// (16/10, lado a lado com a 4.2: a fumaça cinza da arte sobe atrás do leque e a areia cobre a base; as nossas mal
	// apareciam) a baforada maior, mais rápida e cinza-escura; a areia um pouco maior
	constexpr float HitDustScale = 0.28f, HitDustTime = 0.6f, HitDustOpacity = 0.9f;
	constexpr float HitPuffScale = 0.14f, HitPuffTime = 0.6f, HitPuffOpacity = 0.85f;
	const FVector4f HitPuffColor(0.45f, 0.43f, 0.40f, 0.6f);         // cinza: a fumaça do metal e da tinta, não a areia
	constexpr float GroundDustScale = 0.25f, GroundDustTime = 0.6f, GroundDustOpacity = 0.85f;
	// (16/10, contra a 4.1: o anel de areia da onda de choque; a saia simétrica do EmberGen a 0,6 (raio final ~8,5 m), o
	// golpe (3,6 s da gravação) em 0,3 s e o resto a 0,6 (~1,4 s), rala no alto (espessura 0,7)
	// (v6, contra a 4.1 lado a lado: na arte o anel é o herói, marcado, ~10 m de raio; a 0,6 com espessura 0,7 ele era
	// uma névoa rala) a 0,8 (raio final ~11 m) e denso
	constexpr float GroundRingScale = 0.8f, GroundRingTime = 0.083f, GroundRingOpacity = 1.0f, GroundRingThickness = 1.4f;
	constexpr float GroundRingSlowAfterS = 3.6f, GroundRingSlowTime = 0.6f;
	// (v7) a gravação simulada do anel: metros reais (escala 1), tempo real (1), a densidade de areia da simulação
	const TCHAR* GroundSimSource = TEXT("sim_chao_anel");
	// (v7b: os gomos do anel saíam opacos e volumosos contra a 4.1, mais baixa e translúcida)
	constexpr float GroundSimScale = 1.0f, GroundSimTime = 1.0f, GroundSimOpacity = 0.75f, GroundSimThickness = 0.8f;
	// (v8, 16/10 noite) a cortina de ejeção: a terra fina que a cratera joga num cone invertido com raios, simulada como
	// grãos (BlastLab/Tools/ejecta_curtain.py: Housen e Holsapple, o arrasto de Schiller e Naumann, o gás empoeirado que
	// leva os finos no jato denso); sobe a ~8 m, abre a ~8 m de raio e desce pairando. Escura: a terra revirada da 4.1
	const TCHAR* GroundCurtainSource = TEXT("sim_chao_cortina");
	// (17/10, o Felipe) o tiro na areia a 0,85 do tamanho (o anel, a cortina, e o leque balístico sobe menos)
	constexpr float GroundHitSize = 0.85f;
	constexpr float GroundCurtainOpacity = 0.9f, GroundCurtainThickness = 1.0f;
	const FVector4f GroundCurtainDirt(0.22f, 0.17f, 0.12f, 0.3f);
	constexpr float GroundImpactGlow = 0.5f;
	// a Patriot (16/10; docs/ESTUDO_MISSIL_PATRIOT.md): o brilho da chama, a poeira do lançamento (a areia gravada a 0,2,
	// ~8 m, o golpe a 0,5 e o resto a 0,35) e a bola da ogiva (a Carga Pesada a 0,07: ~3 m de bola), fuligem cinza-escura
	constexpr float MissileGlow = 0.3f;
	// o carro esmagado (16/10; docs/ESTUDO_ESMAGAR_E_POSTE.md): a poeira do contato (a areia gravada a 0,07, ~2,8 m), a dos
	// pneus (0,05) e a das janelas (0,06), rasteiras e curtas (teto de ~1,5 m: não cobrir o tanque); o brilho dos cacos
	// (16/10 noite, carro_picape_v4 ampliado 5×: a poeira na cor da areia sobre a areia sumia; o FX_CarCrush do ZH usa 5
	// nuvens CINZA, que é o que lê de cima) o contato cinza-terra (o pó velho do carro e da lataria), maior e mais denso; a
	// das janelas mais escura (o pó e a fuligem de dentro); o pneu na cor do chão
	constexpr float CrushDustScale = 0.1f, CrushDustTime = 0.6f, CrushDustOpacity = 0.9f;
	constexpr float TireDustScale = 0.07f, TireDustTime = 0.5f, TireDustOpacity = 0.7f;
	constexpr float WindowDustScale = 0.07f, WindowDustTime = 0.6f, WindowDustOpacity = 0.7f;
	const FVector4f CrushGray(0.62f, 0.58f, 0.53f, 0.75f), WindowGray(0.48f, 0.46f, 0.43f, 0.6f);
	// (16/10 noite, carro_picape_v2b: a poeira não aparecia) a espessura não acompanha o tamanho: a 0,05–0,09 a gravação ficava
	// 11–20× mais transparente (o jato da v6 usava 1/escala); ~4× devolve a densidade de uma poeira de ~3 m
	constexpr float CrushThick = 4.0f;
	constexpr float GlassGlow = 0.6f;
	// o arco do poste: o brilho (o núcleo estoura no branco-azulado de propósito: é a coisa mais clara da cena) e a fumacinha
	// do metal e do isolante queimando (branco-acinzentada, ~1 m, 2–4 s)
	constexpr float ArcGlow = 1.0f;
	// a marca do tiro no casco: os dados de primitiva 12–35 da peça (0–9 são do jogo), 6 vagas; o reforço a menos de 25 cm
	constexpr int32 HullMarkBase = 12, HullMarkSlots = 6;
	constexpr float HullMarkMergeCm = 25.0f;
	constexpr float ArcSmokeScale = 0.03f, ArcSmokeTime = 0.5f, ArcSmokeOpacity = 0.5f;
	const FVector4f ArcSmokeColor(0.78f, 0.77f, 0.75f, 0.8f);
	// a carcaça queimada: a cinza (fuligem com a tinta queimada, escura e fosca) e as lascas; a picape: lanterna e borracha
	constexpr float AshDustScale = 0.08f, AshDustOpacity = 0.6f;
	const FVector4f CrushAsh(0.40f, 0.38f, 0.36f, 0.45f);
	const FLinearColor CrushRust(0.20f, 0.085f, 0.04f, 1.0f), CrushChar(0.035f, 0.033f, 0.03f, 1.0f);
	const FLinearColor CrushLampRed(0.6f, 0.04f, 0.03f, 1.0f), CrushLampAmber(0.85f, 0.4f, 0.04f, 1.0f), CrushRubber(0.03f, 0.03f, 0.03f, 1.0f);
	// (16/10, patriot_v2: a 0,2 e 0,8 a poeira saía uma bola branca atrás da caixa, a areia ao sol estourada; mais rala,
	// menor e na cor da terra um pouco mais escura)
	constexpr float LaunchDustScale = 0.16f, LaunchDustTime = 0.5f, LaunchDustOpacity = 0.45f;
	const FVector4f LaunchDustColor(0.78f, 0.62f, 0.44f, 0.7f);
	constexpr float WarheadScale = 0.07f;
	const FVector4f WarheadSoot(0.22f, 0.21f, 0.20f, 0.3f);
	// o jato: a jg_ground_96 (30 m de domínio, a coluna sobe até ~29 m em 6,6 s) a 0,28 (~8 m) tocada a 0,15 (~1 s), na
	// cor da terra revirada (albedo 0,45)
	constexpr float GroundJetScale = 0.28f, GroundJetTime = 0.15f, GroundJetOpacity = 0.9f;
	const FVector4f GroundJetDirt(0.26f, 0.20f, 0.14f, 0.35f);   // (v6: mais escura, a terra revirada da 4.1)
	// o tanque ferido (16/10): os limiares do ZH; o deck do motor 2,3 m atrás do centro a 2,0 m, o anel da torre a 2,3 m
	constexpr float DamagedHealth = 0.7f, ReallyDamagedHealth = 0.35f;
	constexpr float DeckBackCm = 230.0f, DeckUpCm = 200.0f, RingUpCm = 230.0f;
	// a carcaça (16/10): começa 4 s depois da morte (a bola e a areia da morte já se desfizeram: duas fumaças no mesmo
	// quadro é o defeito nº 1) e dura 30 s, o mesmo tempo do miolo quente do material da carcaça (RTSUnitActor)
	constexpr float WreckDelayS = 4.0f, WreckFireS = 30.0f;
	// (17/10) a luz do fogo da carcaça acende 0,8 s depois da morte (a bola gravada apaga em ~1 s; o diesel segue queimando
	// embaixo e pinta a fumaça da morte por baixo, a pergunta da Arte à noite); a fumaça e as chamas seguem aos 4 s.
	// -WreckLightDelay= troca (4 = o de antes)
	constexpr float WreckLightDelayS = 0.8f;
	// (v2, 17/10) a coluna da carcaça em volume (docs/ESTUDO_CARCACA.md; BlastLab/Tools/blender_pyro_wreck.py, gravação
	// sim_carcaca: o casco de 8,1 m queimando no anel da torre e na grade do motor, a chama de ~1,5–2 m pulsando a ~1 Hz e
	// a fumaça preta subindo ~24 m; o loop de 10 s do trecho estável, inclinado 15° na conversão): no pé da carcaça, girada
	// pro vento do mapa, entra em 1,5 s (dentro da fumaça da morte) e sai nos últimos 5 s. A fuligem preta (albedo 0,25: a
	// da fuligem fresca) e o fogo baixo. Por enquanto só com -WreckV=2 (a 1ª coluna saiu um "bastão" escuro e fino)
	const TCHAR* WreckColumnSource = TEXT("sim_carcaca");
	constexpr float WreckColumnInS = 1.5f, WreckColumnOutS = 5.0f, WreckColumnOpacity = 1.0f, WreckColumnThickness = 1.0f, WreckColumnEmit = 0.6f;
	const FVector4f WreckColumnSoot(0.10f, 0.095f, 0.09f, 0.25f);
	// a torre do jogo: o componente que o RTSUnitActor arremessa, na unidade que morreu (o centro dela a menos de 1,5 m
	// do ponto da morte); o nome é o do RTSUnitActor
	USceneComponent* FindTurret(const UWorld* World, const FVector& Ground)
	{
		USceneComponent* Best = nullptr;
		double BestD = 150.0 * 150.0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const double D = FVector::DistSquared2D(It->GetActorLocation(), Ground);
			if (D < BestD)
			{
				for (UActorComponent* C : It->GetComponents())
				{
					if (C->GetFName() == TEXT("TurretPivot"))
					{
						Best = Cast<USceneComponent>(C);
						BestD = D;
						break;
					}
				}
			}
		}
		return Best;
	}
	// a unidade perto de um ponto (o alvo do traçante: o jogo manda o centro dela): o ator com a TurretPivot mais perto
	AActor* FindUnitNear(const UWorld* World, const FVector& At)
	{
		AActor* Best = nullptr;
		double BestD = 300.0 * 300.0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const double D = FVector::DistSquared2D(It->GetActorLocation(), At);
			if (D < BestD)
			{
				for (UActorComponent* C : It->GetComponents())
				{
					if (C->GetFName() == TEXT("TurretPivot"))
					{
						Best = *It;
						BestD = D;
						break;
					}
				}
			}
		}
		return Best;
	}
	// a peça do tanque mais perto de um ponto (até 1,5 m) e o ponto mais perto na caixa dela, no espaço da malha
	UStaticMeshComponent* PartNear(AActor* Target, const FVector& PointCm, FVector& OutLocal)
	{
		UStaticMeshComponent* Best = nullptr;
		double BestD = 150.0 * 150.0;
		for (UActorComponent* Comp : Target->GetComponents())
		{
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
			if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
			{
				continue;
			}
			const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
			if (Box.GetSize().Z < 30.0)
			{
				continue;
			}
			const FVector L = Mesh->GetComponentTransform().InverseTransformPosition(PointCm);
			const FVector Near = Box.GetClosestPointTo(L);
			const double D = FVector::DistSquared(Near, L);
			if (D < BestD)
			{
				BestD = D;
				Best = Mesh;
				OutLocal = Near;
			}
		}
		return Best;
	}
	bool NewTankBlast()
	{
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("TankBlast="), V); return V != 0; }();
		return bOn;
	}
	constexpr float TankSkirtHeight = 0.4f;
	constexpr float TankSkirtLife = 0.5f;
	constexpr float SizeJitter = 0.1f;
	/** As 3 maiores na tela em detalhe total; as 3 seguintes um nível abaixo; o resto dois (BlastLab). */
	constexpr int32 FullDetailBudget = 3;
	constexpr float WarmSeconds = 2.5f;

	float VariantFraction(uint32 V, uint32 Salt)
	{
		uint32 X = (V ^ Salt) * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return float(X & 0xFFFF) / 65535.0f;
	}
}

UBlastFxDirector* UBlastFxDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBlastFxDirector>() : nullptr;
}

TStatId UBlastFxDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlastFxDirector, STATGROUP_Tickables);
}

bool UBlastFxDirector::LoadAssets()
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
		UE_LOG(LogTemp, Error, TEXT("BlastFx: faltam as explosões gravadas (Game/Tools/game.ps1 worlds traz do BlastLab)"));
		return false;
	}
	return true;
}

AExplosionVolume* UBlastFxDirector::Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed)
{
	AExplosionVolume* E = GetWorld()->SpawnActorDeferred<AExplosionVolume>(AExplosionVolume::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Bake = Bake;
	E->SizeScale = SizeScale;
	E->Seed = Seed;
	E->Age = 0.0f;
	E->bSelfAdvance = true;
	E->Volume->SetMaterial(0, VolumeMaterial.Get());
	E->FinishSpawning(FTransform(Ground));
	return E;
}

AExplosionVolume* UBlastFxDirector::TankDeath(const FVector& Ground, uint32 Variant, bool bClassic, AActor* WreckOf)
{
	CSV_EVENT(BlastFxDirector, TEXT("TankDeath"));
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, TankDeath);
	// a carcaça acende pela morte (17/10): se o tiro que matou não passou pelo ArmorHit (a morte por míssil, por ordem da cena,
	// ou o aviso da morte antes do acerto), ela ficava sem fogo
	if (WreckOf && !bClassic)
	{
		const FDamaged* D = Damaged.Find(WreckOf);
		if (!D || D->WreckStart < 0.0)
		{
			EndDamaged(WreckOf);
			StartWreck(WreckOf);
		}
	}
	if (!LoadAssets())
	{
		return nullptr;
	}
	UExplosionBake* Bake = Bakes[Variant % uint32(Bakes.Num())];
	const float Jitter = 1.0f + (VariantFraction(Variant, 0x51ED) - 0.5f) * SizeJitter;
	// bClassic: a explosão do prédio fica a clássica (o Felipe, 13/10: "a nova é só pra tanque"; o jogo chama com true)
	if (NewTankBlast() && !bClassic)
	{
		// a nova: a gravação própria no raymarch (a variante da Carga Pesada só segura o ator e o volume de reserva)
		// O pé 70 cm abaixo do chão (16/10, a moldura clara em volta da carcaça, o defeito mais visível na câmera de jogo): a
		// gravação assenta a areia numa laje rente ao chão da simulação nos quadros finais, e no jogo o chão de verdade fica
		// abaixo dela em volta do casco (o avental da cratera afunda até 50–90 cm; na vila as molduras seguiam o contorno de
		// cada casco). A/B fx_acerto_67_v2 × fx67_drop70: a moldura some e a bola fica igual. -DeathFloorDrop= troca
		static const float FloorDrop = [] { float V = 70.0f; FParse::Value(FCommandLine::Get(), TEXT("DeathFloorDrop="), V); return V; }();
		AExplosionVolume* N = Spawn(Ground - FVector(0.0, 0.0, FloorDrop), Bake, TankBlastScale * Jitter, VariantFraction(Variant, 0xA11CE));
		if (!N)
		{
			return nullptr;
		}
		N->RaymarchSource = FName(TEXT("eg_tq_v2"));
		N->TimeScale = TankBlastTime * Jitter;
		N->Thickness = 1.0f / TankBlastScale;
		N->RaymarchSoot = TankSoot;
		N->RaymarchDust = TankDust;
		N->RaymarchSunFull = TankSunFull;
		N->RaymarchSkyGain = TankSkyGain;
		N->RaymarchGround = TankGround;
		Alive.Add(N);
		if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			Debris->BallScale = TankBlastScale;
			// o orçamento da morte em massa (16/10, o custo de processador: as 11 mortes juntas da vila davam picos de
			// 2,8 ms nos destroços): da 5ª morte em 3 s em diante, metade das peças (o ZH corta partículas por
			// prioridade quando a cena enche; numa morte em massa a diferença não se vê)
			const double NowD = GetWorld()->GetTimeSeconds();
			RecentDeaths.RemoveAll([NowD](double T0) { return NowD - T0 > 3.0; });
			Debris->DetailK = RecentDeaths.Num() >= 4 ? 0.5f : 1.0f;
			RecentDeaths.Add(NowD);
			Debris->TrailTint = FLinearColor(TankSoot.X, TankSoot.Y, TankSoot.Z, 1.0f);
			Debris->GlowScale = DebrisGlow;
			Debris->Start(Ground + FVector(0.0, 0.0, DebrisHeightCm), int32(Variant * 2654435761u >> 1));
			// a torre solta fuligem (albedo ~0,07): rastro de ~0,7 m que engorda 0,7 m/s e se desfaz em 2,5 s
			Debris->FollowComponent(FindTurret(GetWorld(), Ground), 0.7f, 0.7f, 2.5f, 0.07f, 0.0f, 0.7f);
		}
		return N;
	}
	AExplosionVolume* E = Spawn(Ground, Bake, TankBallScale * Jitter, VariantFraction(Variant, 0xA11CE));
	if (!E)
	{
		return nullptr;
	}
	E->TimeScale = Jitter;                    // a gravação no relógio dela, sem esticar pelo tamanho
	E->Thickness = 1.0f / TankBallScale;      // a bola menor tão opaca quanto a Carga Pesada aprovada
	E->SkirtHeight = TankSkirtHeight;
	E->SkirtLife = TankSkirtLife;
	// sem o halo de ofuscamento: é um cartaz virado pra câmera que, na câmera alta do jogo,
	// atravessa o chão e mostra uma borda reta (o BlastLab também o tirou da morte de tanque nova)
	Alive.Add(E);
	return E;
}

void UBlastFxDirector::Prewarm(const FVector& At)
{
	if (!LoadAssets() || bWarm || Warmups.Num())
	{
		return;
	}
	// no centro da vista (a Unreal só compila o shader no primeiro desenho: fora da tela não aquece)
	UpdateView();
	for (int32 I = 0; I < Bakes.Num(); ++I)
	{
		if (AExplosionVolume* E = Spawn(At + ViewRight * (I - 1) * 800.0f, Bakes[I], TankBallScale, 0.0f))
		{
			E->Opacity = 0.0f;       // desenha, mas não mostra nada
			E->Age = 0.3f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
	}
	// os destroços também (os materiais deles estreavam no 1º quadro da morte e a Unreal montava o PSO ali: quadro de
	// 140 ms no BlastLab, 07/10): mil vezes menores em volta de um ponto 10 m acima, somem em 30 s
	if (NewTankBlast())
	{
		// e o raymarch da morte nova (14/10; a sessão do jogo: "a 1ª morte da partida derruba pra ~15 qps por 2 s"). As
		// de cima têm opacidade 0 e o raymarch pula opacidade 0, então tudo dele estreava na 1ª morte: o atlas de blocos
		// (198 MB), a subida da gravação eg_tq_v2, a light function da sombra no sol (no editor, a compilação do material
		// dela) e o 1º uso de cada passe. Esta passa no filtro (opacidade 0,001, invisível) com a bola já cheia, parada:
		// tudo isso acontece aqui, no carregamento. A sombra no sol, depois de ligada, custa ~0,04 ms por quadro (perfil).
		if (AExplosionVolume* E = Spawn(At, Bakes[0], TankBlastScale, 0.0f))
		{
			E->RaymarchSource = FName(TEXT("eg_tq_v2"));
			E->TimeScale = TankBlastTime;
			E->Thickness = 1.0f / TankBlastScale;
			E->RaymarchSoot = TankSoot;
			E->RaymarchDust = TankDust;
			E->RaymarchSunFull = TankSunFull;
			E->RaymarchSkyGain = TankSkyGain;
			E->RaymarchGround = TankGround;
			E->Opacity = 1.0e-3f;
			E->Age = 0.6f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
		// e a areia do tiro (a gravação dela sobe aqui, não no 1º disparo)
		if (AExplosionVolume* E = Spawn(At + ViewRight * 1200.0f, Bakes[0], MuzzleDustScale, 0.0f))
		{
			E->RaymarchSource = FName(DustSourceName);
			E->TimeScale = MuzzleDustTime;
			E->Opacity = 1.0e-3f;
			E->Age = 0.3f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
		const FVector DebrisAt = At + FVector(0.0, 0.0, 1000.0);
		if (ABlastDebris* Debris = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(),
			FTransform(FQuat::Identity, DebrisAt * (1.0 - 0.001), FVector(0.001))))
		{
			Debris->GlowScale = DebrisGlow;
			Debris->Start(DebrisAt, 1);
			Debris->SetLifeSpan(30.0f);
		}
	}
	WarmClock = 0.0f;
}

void UBlastFxDirector::UpdateView()
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
	// o campo de visão da câmera do jogo é o horizontal (padrão da Unreal)
	TanHalfX = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f);
	TanHalfY = TanHalfX * float(Size.Y / Size.X);
	bHasView = true;
}

float UBlastFxDirector::ScreenFraction(const FVector& Center, float Radius, bool& bInView) const
{
	bInView = true;
	if (!bHasView)
	{
		return 1.0f;
	}
	// esfera contra as 4 faces e o plano da câmera, nos eixos da vista (como no BlastLab)
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
	return Z <= Radius ? 1.0f : FMath::Min(Radius / (Z * TanHalfY), 1.0f);
}

void UBlastFxDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateView();
	UpdateDusts();
	UpdateDamaged();
	UpdateCrushes();
	UpdateCrushTest();
	UpdateArcTest();
	GatherSceneLights();
	UpdateShadowTest();
	if (Missiles.Num())
	{
		const double NowM = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		for (auto It = Missiles.CreateIterator(); It; ++It)
		{
			if (!It.Value().Fx.IsValid() || NowM - It.Value().Seen > 1.0)
			{
				if (ABlastDebris* M = It.Value().Fx.Get())
				{
					M->EndMissile();
				}
				It.RemoveCurrent();
			}
		}
	}
	if (DelayedHits.Num() || PendingShots.Num())
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		for (int32 I = DelayedHits.Num() - 1; I >= 0; --I)
		{
			if (Now >= DelayedHits[I].Shot.Arrive)
			{
				const FDelayedHit Dh = DelayedHits[I];
				DelayedHits.RemoveAtSwap(I);
				SpawnArmorImpact(Dh.Shot, Dh.Target.Get(), Dh.GroundZCm, Dh.bKill);
			}
		}
		for (auto It = PendingShots.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value().Born > 3.0)
			{
				It.RemoveCurrent();
				continue;
			}
			// (16/10, o Felipe: "chega, some, depois aparece") o que se vê manda: as faíscas nascem no quadro em que o
			// traçante chega, no ponto dele; o aviso da simulação depois só traz a morte (o jeito dos jogos: o efeito
			// segue o projétil desenhado, o dano fica com a simulação)
			FShotPoint& P = It.Value();
			if (!P.bSpawned && Now >= P.Arrive)
			{
				P.bSpawned = true;
				LastHitAt.Add(It.Key(), Now);
				SpawnArmorImpact(P, It.Key().Get(), P.GroundZCm, false);
			}
		}
	}
	if (Warmups.Num())
	{
		WarmClock += DeltaTime;
		const bool bShadersDone = !GShaderCompilingManager || GShaderCompilingManager->GetNumRemainingJobs() == 0;
		if ((WarmClock > WarmSeconds && bShadersDone) || WarmClock > 60.0f)
		{
			for (AExplosionVolume* E : Warmups)
			{
				if (IsValid(E))
				{
					E->Destroy();
				}
			}
			Warmups.Reset();
			bWarm = true;
		}
	}
	// a gravação acabou: a explosão sai de cena. A morte de tanque nova toca a gravação própria (a eg_tq_v2, 3 s) num
	// ator que segura a variante da Carga Pesada; ela acabava pelo relógio da Carga Pesada (16/10, fx_acerto: mais
	// longa), e o último quadro da própria (a areia assentada que enche o chão da caixa) ficava congelado ~1 s em volta
	// da carcaça: a borda clara reta. Agora sai no fim da gravação própria, esmaecendo no último 0,8 s dela (o
	// EndFadeS da noite de 13/10)
	const UBlastRaymarchSubsystem* RmLife = GetWorld() ? GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>() : nullptr;
	for (int32 I = Alive.Num() - 1; I >= 0; --I)
	{
		AExplosionVolume* E = Alive[I];
		const float Own = IsValid(E) && RmLife && !E->RaymarchSource.IsNone() ? RmLife->SourceDuration(E->RaymarchSource) : 0.0f;
		const bool bDone = !IsValid(E) || (Own > 0.0f ? E->BakeTime() >= Own : E->IsFinished());
		if (bDone)
		{
			if (IsValid(E))
			{
				E->Destroy();
			}
			Alive.RemoveAtSwap(I);
			continue;
		}
		if (Own > 0.0f)
		{
			// (A/B, 16/10: -DeathFadeFrom= s da gravação onde começa o esmaecer; padrão os últimos 0,8 s)
			static const float FadeFrom = [] { float V = -1.0f; FParse::Value(FCommandLine::Get(), TEXT("DeathFadeFrom="), V); return V; }();
			E->Opacity = 1.0f - FMath::SmoothStep(FadeFrom >= 0.0f ? FMath::Min(FadeFrom, Own - 0.05f) : Own - 0.8f, Own, E->BakeTime());
		}
	}
	// orçamento de detalhe: as maiores na tela ficam com tudo
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
	for (int32 I = 0; I < Seen.Num(); ++I)
	{
		Seen[I]->LodFloor = I < FullDetailBudget ? 0 : (I < 2 * FullDetailBudget ? 1 : 2);
	}
}

// =============================================================================
// O tiro (14/10, a v3). Os alvos: a arte 4.2 (o acerto na blindagem, docs/concept/explosions/type_4.2_armor_sparks.webp),
// a 4.1 (o tiro na areia) e os game shots (o clarão de boca de close_game_cam_gpt, os traçantes laranja curtos). O estudo
// e os números: docs/TIRO_E_ACERTO.md.

bool UBlastFxDirector::ShotFxEnabled()
{
	static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("ShotFx="), V); return V != 0; }();
	return bOn;
}

AExplosionVolume* UBlastFxDirector::SpawnDust(const FVector& GroundCm, float Scale, float TimeScale, float Opacity, int32 Prio, uint32 Variant,
	const FVector4f& Color, const TCHAR* Source, float Thickness, const FVector4f& Soot)
{
	const FName SourceName(Source ? Source : DustSourceName);
	if (!LoadAssets())
	{
		return nullptr;
	}
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	const float Duration = Rm ? Rm->SourceDuration(SourceName) : 0.0f;
	if (Duration <= 0.0f)
	{
		return nullptr;                             // sem a gravação, sem areia (o volume da Unreal não a desenharia igual)
	}
	// o orçamento: o raymarch desenha até 16 por quadro e as mortes vêm primeiro. Cheio, o acerto tira de cena o sopro
	// de disparo mais velho; o sopro, cheio, não nasce
	Dusts.RemoveAll([](const FDust& D) { return !D.E.IsValid(); });
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const int32 Budget = Eq >= 3 ? MaxDustsHigh : (Eq == 2 ? MaxDustsHigh / 2 : 1);
	if (Dusts.Num() >= Budget)
	{
		// (17/10) cheio, sai a que está mais perto do fim (a fração já tocada da gravação dela), nunca a que acabou de nascer:
		// antes o tiro na areia tirava de cena o disparo mais velho (vivo, no meio do fogo) e o disparo cheio não nascia
		// (17/10 tarde) pela importância, como o Effect Type do Niagara (significância e orçamento): primeiro as fora da tela,
		// depois a mais perto do fim (a fração da vida dela); as de menos de 0,25 s só se não houver outra
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
		auto OnScreen = [Cam](const AExplosionVolume* V)
		{
			if (!Cam)
			{
				return true;
			}
			// um cone na diagonal da vista, alargado pelo raio da areia (~12 m × a escala, folgado)
			const FVector D = V->GetActorLocation() + FVector(0.0, 0.0, 300.0) - Cam->GetCameraLocation();
			const double Dist = D.Size(), R = 1200.0 * FMath::Max(V->SizeScale, 0.3f) + 500.0;
			if (Dist <= R)
			{
				return true;
			}
			const double Half = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(0.5 * Cam->GetFOVAngle())) * 1.15);
			const double Ang = FMath::Acos(FMath::Clamp(FVector::DotProduct(D / Dist, Cam->GetCameraRotation().Vector()), -1.0, 1.0));
			return Ang < Half + FMath::Asin(FMath::Min(R / Dist, 1.0));
		};
		int32 Victim = INDEX_NONE;
		float Most = -1.0f;
		for (int32 k = 0; k < Dusts.Num(); ++k)
		{
			const AExplosionVolume* V = Dusts[k].E.Get();
			const float Done = Dusts[k].LifeS > 0.0f ? V->Age / Dusts[k].LifeS : V->BakeTime() / FMath::Max(Dusts[k].Duration, 0.01f);
			const float Score = (OnScreen(V) ? 0.0f : 10.0f) + (V->Age < 0.25f ? -20.0f : 0.0f) + Done;
			if (Score > Most)
			{
				Most = Score;
				Victim = k;
			}
		}
		if (Victim == INDEX_NONE)
		{
			return nullptr;
		}
		Dusts[Victim].E->Destroy();
		Dusts.RemoveAt(Victim);
	}
	AExplosionVolume* E = Spawn(GroundCm, Bakes[Variant % uint32(Bakes.Num())], Scale, VariantFraction(Variant, 0xD057));
	if (!E)
	{
		return nullptr;
	}
	E->RaymarchSource = SourceName;
	E->TimeScale = TimeScale;
	E->Thickness = Thickness;
	E->Opacity = Opacity;
	E->bLights = false;                             // a areia não tem fogo: a luz do tiro é a dos destroços
	E->bFlash = false;                              // nem o clarão da detonação (na bola do disparo ele a deixava branca, 16/10)
	E->RaymarchSoot = Soot.X >= 0.0f ? Soot : TankSoot;
	E->RaymarchDust = Color.X >= 0.0f ? Color : TankDust;   // a areia do mapa (a mesma da morte de tanque) ou a cor pedida
	E->RaymarchSunFull = TankSunFull;
	E->RaymarchSkyGain = TankSkyGain;
	E->RaymarchGround = TankGround;
	Dusts.Add({ E, Duration, Opacity, Prio });
	return E;
}

void UBlastFxDirector::UpdateDusts()
{
	// a areia some no último quarto da gravação e sai de cena no fim (a vida do ator seria a da Carga Pesada)
	for (int32 I = Dusts.Num() - 1; I >= 0; --I)
	{
		AExplosionVolume* E = Dusts[I].E.Get();
		if (!E)
		{
			Dusts.RemoveAtSwap(I);
			continue;
		}
		const float T = E->BakeTime();
		const float Life = Dusts[I].LifeS;
		if (T >= Dusts[I].Duration || (Life > 0.0f && E->Age >= Life))
		{
			E->Destroy();
			Dusts.RemoveAtSwap(I);
			continue;
		}
		// e entra nos primeiros 0,25 s da gravação (16/10: a gravação da areia nasce como uma esfera lisa, e no 1º quadro
		// do disparo ela era uma bola clara de sabão à frente da boca)
		E->Opacity = Dusts[I].Opacity0 * (1.0f - FMath::SmoothStep(0.75f * Dusts[I].Duration, Dusts[I].Duration, T))
			* FMath::SmoothStep(0.0f, 0.25f, T);
		if (Life > 0.0f)
		{
			// (17/10 tarde) a vida de verdade: rala aos poucos até sumir (o disparo)
			E->Opacity *= 1.0f - FMath::SmoothStep(Dusts[I].FadeFromS, Life, E->Age);
		}
	}
}

void UBlastFxDirector::MuzzleBlast(const FVector& MuzzleCm, const FVector& Dir, float GroundZCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, MuzzleBlast);
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	const FVector D = FVector(Dir.X, Dir.Y, 0.0).GetSafeNormal();
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = MuzzleGlow;
		Fx->StartMuzzle(MuzzleCm, Dir, int32(Variant * 2246822519u >> 1));
	}
	// a bola secundária (o gás que reacende à frente da boca, 15–150 ms) e a fumaça do propelente depois: a gravação da
	// morte de tanque minúscula e rápida no raymarch (fogo de verdade, que tapa o que está atrás; a aditiva virava creme
	// sobre a areia clara, 16/10)
	// (opções de A/B, sem recompilar: -ShotFxBallEmit=, -ShotFxBallSoot=r,g,b,a, -ShotFxBallSlow=, -ShotFxDustSlow=)
	static const float BallEmit = CmdFloat(TEXT("ShotFxBallEmit="), MuzzleBallEmit);
	static const float BallSlow = CmdFloat(TEXT("ShotFxBallSlow="), MuzzleBallSlowTime);
	static const float DustSlow = CmdFloat(TEXT("ShotFxDustSlow="), MuzzleDustSlowTime);
	static const FVector4f BallSoot = CmdColor(TEXT("ShotFxBallSoot="), MuzzleSmokeSoot);
	// (v7, 17/10) a gravação simulada no rumo do cano; -MuzzleV=6 volta a bola de antes (a morte em miniatura)
	static const int32 MuzzleVer = [] { int32 V = 7; FParse::Value(FCommandLine::Get(), TEXT("MuzzleV="), V); return V; }();
	static const float SimEmit = CmdFloat(TEXT("ShotFxSimEmit="), MuzzleSimEmit);
	static const float SimThick = CmdFloat(TEXT("ShotFxSimThick="), MuzzleSimThickness);
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	if (MuzzleVer >= 7 && Rm && Rm->SourceDuration(FName(MuzzleSimSource)) > 0.0f)
	{
		static const FVector4f SimSoot = CmdColor(TEXT("ShotFxBallSoot="), MuzzleSimSoot);
		static const float SimScale = CmdFloat(TEXT("ShotFxSimScale="), MuzzleSimScale);
		static const float SimFast = CmdFloat(TEXT("ShotFxSimFast="), MuzzleSimFastTime);
		static const float SimSlow = CmdFloat(TEXT("ShotFxSimSlow="), MuzzleSimSlowTime);
		static const float SimLife = CmdFloat(TEXT("ShotFxSimLife="), MuzzleSimLifeS);
		static const float SimFade = CmdFloat(TEXT("ShotFxSimFade="), MuzzleSimFadeFromS);
		if (AExplosionVolume* Sim = SpawnDust(FVector(MuzzleCm.X, MuzzleCm.Y, MuzzleCm.Z - MuzzleSimHeightCm * SimScale), SimScale, SimFast, 1.0f, 0,
			Variant ^ 0xB411u, MuzzleSmokeDust, MuzzleSimSource, SimThick, SimSoot))
		{
			// (17/10 tarde) num andamento só (o 2º, a SimSlow, fazia a fumaça inflar no fim); -ShotFxSimLife=0 volta o de antes
			if (SimLife <= 0.0f)
			{
				Sim->SlowAfterS = MuzzleSimSlowAfterS;
				Sim->SlowTimeScale = SimSlow;
			}
			else if (FDust* Dd = Dusts.FindByPredicate([Sim](const FDust& X) { return X.E.Get() == Sim; }))
			{
				Dd->LifeS = SimLife;
				Dd->FadeFromS = FMath::Min(SimFade, SimLife);
			}
			Sim->RaymarchEmit = SimEmit;
			Sim->RaymarchYawRad = float(FMath::Atan2(Dir.Y, Dir.X));
		}
	}
	else if (AExplosionVolume* Ball = SpawnDust(FVector(MuzzleCm.X + D.X * MuzzleBallAheadCm, MuzzleCm.Y + D.Y * MuzzleBallAheadCm, MuzzleCm.Z - MuzzleBallDropCm),
		MuzzleBallScale, MuzzleBallTime, 1.0f, 0, Variant ^ 0xB411u, MuzzleSmokeDust, TEXT("eg_tq_v2"), MuzzleBallThickness, BallSoot))
	{
		Ball->SlowAfterS = MuzzleBallSlowAfterS;
		Ball->SlowTimeScale = BallSlow;
		Ball->RaymarchEmit = BallEmit;
	}
	// a areia que o sopro levanta do chão em volta, à frente da boca (a sobrepressão bate no chão)
	if (AExplosionVolume* Sand = SpawnDust(FVector(MuzzleCm.X + D.X * MuzzleDustAheadCm, MuzzleCm.Y + D.Y * MuzzleDustAheadCm, GroundZCm),
		MuzzleDustScale, MuzzleDustTime, MuzzleDustOpacity, 0, Variant))
	{
		Sand->SlowAfterS = MuzzleDustSlowAfterS;
		Sand->SlowTimeScale = DustSlow;
	}
}

void UBlastFxDirector::Tracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	// o tiro no tanque: o ponto na blindagem já aqui, e o traçante termina nele (o acerto usa o mesmo ponto)
	FVector End = ToCm;
	if (AActor* Target = FindUnitNear(GetWorld(), ToCm))
	{
		FShotPoint Shot;
		const uint32 ShotVariant = Variant ^ (++ShotSerial) * 0x9E3779B9u;
		if (ArmorPoint(Target, FromCm, ToCm, float(ToCm.Z - 120.0), ShotVariant, Shot))
		{
			const double Now = GetWorld()->GetTimeSeconds();
			Shot.Born = Now;
			Shot.Arrive = Now + FVector::Dist(FromCm, Shot.Point) / FMath::Max(double(SpeedCmPerS), 1.0);
			Shot.Variant = ShotVariant;
			Shot.GroundZCm = float(ToCm.Z - 120.0);
			PendingShots.Add(Target, Shot);
			End = Shot.Point;
		}
	}
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = TracerGlow;
		Fx->StartTracer(FromCm, End, SpeedCmPerS, int32(Variant * 3266489917u >> 1));
	}
}

void UBlastFxDirector::ArmorHit(const FVector& AtCm, const FVector& FromCm, AActor* Target, float GroundZCm, uint32 Variant, bool bKill)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	CSV_SCOPED_TIMING_STAT(BlastFxDirector, ArmorHit);
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	// no máximo um efeito por vítima a cada 100 ms (o ZH, ActiveBody.cpp): vários tiros no mesmo quadro não somam clarões
	const double Now = GetWorld()->GetTimeSeconds();
	if (Target)
	{
		if (const double* Last = LastHitAt.Find(Target); Last && Now - *Last < 0.1)
		{
			return;
		}
		LastHitAt.Add(Target, Now);
		if (LastHitAt.Num() > 256)
		{
			for (auto It = LastHitAt.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid() || Now - It.Value() > 1.0)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
	FShotPoint Shot;
	const FShotPoint* Pending = Target ? PendingShots.Find(Target) : nullptr;
	if (Pending && Now - Pending->Born < 3.0 && Pending->bSpawned)
	{
		// (16/10) as faíscas já nasceram quando o traçante chegou (o aviso da simulação vem 1–3 quadros depois: o
		// projétil dela sai do centro do tanque); daqui só a morte
		PendingShots.Remove(Target);
		if (bKill)
		{
			EndDamaged(Target);
			StartWreck(Target);
		}
		return;
	}
	if (Pending && Now - Pending->Born < 3.0)
	{
		Shot = *Pending;
		PendingShots.Remove(Target);
	}
	else
	{
		ArmorPoint(Target, FromCm, AtCm, GroundZCm, Variant, Shot);
		Shot.Variant = Variant;
		Shot.Arrive = Now;
	}
	if (bKill)
	{
		EndDamaged(Target);                         // a morte assume (a fumaça do ferido para)
		StartWreck(Target);                         // e a carcaça fica queimando depois que a explosão se desfaz
	}
	if (Shot.Arrive > Now + 0.005)
	{
		// o traçante ainda não chegou: as faíscas esperam por ele
		FDelayedHit& Dh = DelayedHits.AddDefaulted_GetRef();
		Dh.Shot = Shot;
		Dh.Target = Target;
		Dh.GroundZCm = GroundZCm;
		Dh.bKill = bKill;
		return;
	}
	SpawnArmorImpact(Shot, Target, GroundZCm, bKill);
}

bool UBlastFxDirector::ArmorPoint(AActor* Target, const FVector& FromCm, const FVector& AtCm, float GroundZCm, uint32 Variant, FShotPoint& Out) const
{
	// o ponto: o raio do atirador até o alvo entra na caixa do próprio tanque (no espaço dele, a caixa justa das malhas),
	// na face virada pro atirador; a altura sorteada entre o casco e a torre (a arte acerta a bochecha da torre)
	Out.Point = AtCm;
	Out.Normal = (FromCm - AtCm).GetSafeNormal2D();
	const float AimZ = GroundZCm + FMath::Lerp(HitHeightMinCm, HitHeightMaxCm, VariantFraction(Variant, 0x4A1F));
	const FVector From(FromCm.X, FromCm.Y, AimZ);
	const FVector Across = FVector::CrossProduct((AtCm - FromCm).GetSafeNormal2D(), FVector::UpVector);
	const FVector Aim = FVector(AtCm.X, AtCm.Y, AimZ) + Across * FMath::Lerp(-70.0f, 70.0f, VariantFraction(Variant, 0x51DE));
	Out.ShellDir = (Aim - From).GetSafeNormal();
	float BestT = TNumericLimits<float>::Max();
	if (Target)
	{
		// cada peça visível que faz sombra (casco, torre, lagartas, rodas, cano), na caixa da própria malha e no espaço
		// dela: o 1º toque do raio do atirador. As placas da poeira das lagartas e a sombra de contato não fazem sombra
		const FVector RayEnd = Aim + (Aim - From).GetSafeNormal() * 300.0;
		for (UActorComponent* Comp : Target->GetComponents())
		{
			const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp);
			if (!Mesh || !Mesh->IsVisible() || !Mesh->GetStaticMesh() || !Mesh->CastShadow)
			{
				continue;
			}
			if (const UMaterialInterface* M = Mesh->GetMaterial(0); M && M->GetBlendMode() != BLEND_Opaque && M->GetBlendMode() != BLEND_Masked)
			{
				continue;
			}
			const FTransform Cx = Mesh->GetComponentTransform();
			const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
			if (Box.GetSize().Z * Cx.GetScale3D().Z < 30.0)
			{
				continue;
			}
			FVector HitL, NormalL;
			float Time = 0.0f;
			if (FMath::LineExtentBoxIntersection(Box, Cx.InverseTransformPosition(From), Cx.InverseTransformPosition(RayEnd), FVector::ZeroVector, HitL, NormalL, Time)
				&& Time < BestT)
			{
				BestT = Time;
				Out.Point = Cx.TransformPosition(HitL);
				Out.Normal = Cx.TransformVectorNoScale(NormalL).GetSafeNormal();
				Out.Part = Cast<UStaticMeshComponent>(Comp);
				Out.LocalCm = HitL;
			}
		}
		static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
		if (bLog)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx acerto: alvo %s em %s; de %s pra %s; achou %d em %s (normal %s)"), *Target->GetName(),
				*Target->GetActorLocation().ToString(), *From.ToString(), *Aim.ToString(), BestT < 2.0f ? 1 : 0, *Out.Point.ToString(), *Out.Normal.ToString());
		}
	}
	return BestT < 2.0f;
}

void UBlastFxDirector::SpawnArmorImpact(const FShotPoint& Shot, AActor* Target, float GroundZCm, bool bKill)
{
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = ImpactGlow;
		Fx->StartImpact(Shot.Point, Shot.Normal, Shot.ShellDir, int32(Shot.Variant * 2654435761u >> 1));
	}
	MarkHull(Shot.Part.Get(), Shot.LocalCm, 0);
	// a areia que a onda de choque levanta em volta do tanque (no tiro que mata, a morte já traz a dela)
	if (!bKill)
	{
		SpawnDust(FVector(Shot.Point.X, Shot.Point.Y, GroundZCm), HitDustScale, HitDustTime, HitDustOpacity, 1, Shot.Variant);
		// a baforada cinza no ponto do acerto (0,25–0,4 do tanque, alfa 0,3–0,5): a primeira a cair quando falta orçamento
		SpawnDust(Shot.Point - FVector(0.0, 0.0, 60.0), HitPuffScale, HitPuffTime, HitPuffOpacity, 0, Shot.Variant ^ 0x9E37u, HitPuffColor);
	}
}

void UBlastFxDirector::GroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	// o tiro na areia (a arte 4.1; o estudo com fontes: docs/TIRO_E_ACERTO.md, "O tiro na areia"). O anel de areia: a
	// onda de choque levanta a poeira da superfície até onde ela ainda passa de Mach ~1,1 (Ugarte, Houim & Oran 2017),
	// ~7–10 m pra 1–2,5 kg de TNT (Kingery-Bulmash), quase de uma vez, e a poeira fica pairando: a saia de areia
	// simétrica do EmberGen (eg_poeira3_96, 28,5 m) a 0,6 (o anel até ~8,5 m), com o relógio em dois andamentos: o
	// golpe nos primeiros 3,6 s da gravação em ~0,3 s, e o resto pairando ~1,4 s
	// (v7, 16/10) o anel simulado no Mantaflow (BlastLab/Tools/blender_pyro_ground.py, sim_chao_anel): a areia de perto
	// varrida pelo gás do buraco na horizontal rola num anel com o meio limpo até ~7 m em ~0,5 s, a de longe levantada no
	// lugar pelo choque, baixa (2–4 m) e assentando; em metros reais e no tempo real (as versões de antes esticavam
	// gravações feitas pra outra coisa: nascia pronta, domo cheio, bola marrom). A terra escura vai pelo leque balístico
	// (17/10, a escolha do Felipe no A/B v7 × v8: "a primeira tá boa pra um primeiro tiro num lugar limpo, a segunda pra um
	// tiro onde já tem uma cratera, porque não tem mais areia ali"; "reduzir um pouco ambos, uma capadinha no final") o
	// tiro lembra onde caíram os anteriores: a até 2,5 m de uma cratera, a terra escura sobe (a v8: o anel e a cortina); em
	// areia limpa, só o anel (a v7). As duas a 0,85 do tamanho. -GroundHitV=7 ou 8 força uma delas
	static const int32 Ver = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("GroundHitV="), V); return V; }();
	bool bCrater = Ver >= 8;
	if (Ver == 0)
	{
		const FVector2D P(AtCm.X, AtCm.Y);
		bCrater = GroundCraters.ContainsByPredicate([&P](const FVector2D& C) { return FVector2D::DistSquared(C, P) < 250.0 * 250.0; });
		GroundCraters.Add(P);
		if (GroundCraters.Num() > 512)
		{
			GroundCraters.RemoveAt(0, GroundCraters.Num() - 512);
		}
	}
	const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
	if ((Ver == 0 || Ver >= 7) && Rm && Rm->SourceDuration(FName(GroundSimSource)) > 0.0f)
	{
		SpawnDust(AtCm, GroundSimScale * GroundHitSize, GroundSimTime * GroundHitSize, GroundSimOpacity, 1, Variant, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
			GroundSimSource, GroundSimThickness / GroundHitSize);
		if (bCrater && Rm->SourceDuration(FName(GroundCurtainSource)) > 0.0f)
		{
			SpawnDust(AtCm, GroundHitSize, GroundHitSize, GroundCurtainOpacity, 1, Variant ^ 0xC07u, GroundCurtainDirt, GroundCurtainSource,
				GroundCurtainThickness / GroundHitSize, GroundCurtainDirt);
		}
	}
	else
	{
		if (AExplosionVolume* Ring = SpawnDust(AtCm, GroundRingScale, GroundRingTime, GroundRingOpacity, 1, Variant, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
			TEXT("eg_poeira3_96"), GroundRingThickness))
		{
			Ring->SlowAfterS = GroundRingSlowAfterS;
			Ring->SlowTimeScale = GroundRingSlowTime;
		}
		// o jato de terra escura da v6: a explosão no chão gravada da JangaFX (jg_ground_96) pequena, tingida de terra escura
		if (AExplosionVolume* Jet = SpawnDust(AtCm, GroundJetScale, GroundJetTime, GroundJetOpacity, 1, Variant ^ 0x7E7u, GroundJetDirt,
			TEXT("jg_ground_96"), 1.0f / GroundJetScale, GroundJetDirt))
		{
			Jet->bFlash = false;
			Jet->RaymarchEmit = 0.0f;
		}
	}
	// o clarão, as faíscas, a luz e os torrões
	if (ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		Fx->GlowScale = GroundImpactGlow;
		Fx->TrailTint = FLinearColor(1.0f, 0.82f, 0.62f, 1.0f);   // × o albedo 0,18 do rastro: terra marrom-escura
		Fx->StartGroundImpact(AtCm, (AtCm - FromCm).GetSafeNormal2D(), int32(Variant * 3266489917u >> 1));
	}
}

void UBlastFxDirector::VehicleDamage(AActor* Unit, float Health01)
{
	if (!ShotFxEnabled() || !Unit || !GetWorld())
	{
		return;
	}
	const int32 Stage = Health01 <= ReallyDamagedHealth ? 2 : (Health01 <= DamagedHealth ? 1 : 0);
	FDamaged* D = Damaged.Find(Unit);
	if (Stage == 0 || (D && Stage <= D->Stage))
	{
		return;                                     // só as passagens (o jogo chama a cada quadro)
	}
	if (!D)
	{
		D = &Damaged.Add(Unit);
	}
	ABlastDebris* Fx = D->Fx.Get();
	if (!Fx)
	{
		Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity);
		if (Fx)
		{
			Fx->GlowScale = MuzzleGlow;             // o jato de chama laranja (a 1 estourava em branco)
		}
		D->Fx = Fx;
	}
	if (!Fx)
	{
		return;
	}
	if (Stage >= 1 && D->Light < 0)
	{
		// o fio claro (o BAR: fumaça abaixo de 65%; o ZH liga a fumaça no "danificado"): fino, sobe ~1 m/s, se desfaz em 3 s
		static const bool bSmokeTest = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxSmokeTest="), V); return V != 0; }();
		D->Light = bSmokeTest ? Fx->StartSmoke(2.0f, 0.5f, 3.0f, 0.1f, 1.0f, 1.1f) : Fx->StartSmoke(0.4f, 0.45f, 3.5f, 0.2f, 0.6f, 1.2f);
		static const bool bSmokeDbg = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxSmokeDbg="), V); return V != 0; }();
		if (bSmokeDbg)
		{
			D->Dbg[0] = Fx->StartSmoke(0.4f, 0.5f, 3.5f, 0.2f, 0.45f, 0.35f);
			D->Dbg[1] = Fx->StartSmoke(0.7f, 0.7f, 2.5f, 0.07f, 0.7f, -1.0f);
			D->Dbg[2] = Fx->StartSmoke(0.4f, 0.5f, 3.5f, 0.2f, 0.45f, 1.2f);
		}
	}
	if (Stage >= 2 && D->Heavy < 0)
	{
		// a passagem pra "muito danificado": uma vez o jato de chama pelo anel da torre; depois a fumaça escura e grossa
		D->Heavy = Fx->StartSmoke(0.6f, 0.6f, 4.0f, 0.07f, 0.85f, 1.5f);
		Fx->StartFlameJet(Unit->GetActorLocation() + FVector(0.0, 0.0, RingUpCm), int32(GetTypeHash(Unit)));
	}
	D->Stage = Stage;
}

void UBlastFxDirector::EndDamaged(AActor* Unit)
{
	if (FDamaged* D = Unit ? Damaged.Find(Unit) : nullptr)
	{
		if (ABlastDebris* Fx = D->Fx.Get())
		{
			// para de soltar e se desfaz (o rastro vive ~3 s)
			for (const int32 Trail : { D->Light, D->Heavy })
			{
				if (Trail >= 0)
				{
					Fx->SetTrailHead(Trail, Unit->GetActorLocation() + FVector(0.0, 0.0, DeckUpCm), false);
				}
			}
			Fx->SetLifeSpan(5.0f);
		}
		Damaged.Remove(Unit);
	}
}

void UBlastFxDirector::UpdateDamaged()
{
	static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
	static double LastLog = 0.0;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bLogNow = bLog && Now - LastLog > 1.5;
	if (bLogNow)
	{
		LastLog = Now;
	}
	for (auto It = Damaged.CreateIterator(); It; ++It)
	{
		AActor* Unit = It.Key().Get();
		ABlastDebris* Fx = It.Value().Fx.Get();
		if (!Unit || !Fx)
		{
			if (Fx)
			{
				Fx->SetLifeSpan(5.0f);
			}
			It.RemoveCurrent();
			continue;
		}
		const FVector At = Unit->GetActorLocation();
		const FVector Back = -Unit->GetActorForwardVector().GetSafeNormal2D();
		if (It.Value().WreckStart >= 0.0)
		{
			// a carcaça: espera a explosão se desfazer, queima ~30 s e se apaga
			const double Now2 = GetWorld()->GetTimeSeconds();
			const bool bOn = Now2 >= It.Value().WreckStart && Now2 < It.Value().WreckUntil;
			// a luz do fogo de baixo, antes da fumaça e das chamas (17/10)
			if (!It.Value().bWreckLightOn && Now2 >= It.Value().WreckLightStart && Now2 < It.Value().WreckUntil)
			{
				It.Value().bWreckLightOn = true;
				Fx->StartWreckFire(float(It.Value().WreckUntil - Now2), false);
			}
			if (bOn && !It.Value().bWreckLit)
			{
				It.Value().bWreckLit = true;
				// (v2, 17/10) a coluna em volume; sem a gravação (ou -WreckV=1), a fumaça em fita e as chamas de antes
				static const int32 WreckVer = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("WreckV="), V); return V; }();   // (a v2 só com -WreckV=2 até o visual ficar certo)
				const UBlastRaymarchSubsystem* Rm = GetWorld()->GetSubsystem<UBlastRaymarchSubsystem>();
				AExplosionVolume* Col = nullptr;
				if (WreckVer >= 2 && Rm && Rm->SourceDuration(FName(WreckColumnSource)) > 0.0f)
				{
					const double Gz = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
					Col = SpawnDust(FVector(At.X, At.Y, Gz), 1.0f, 1.0f, 0.0f, 1, uint32(Unit->GetUniqueID()) * 0x9E3779B9u, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
						WreckColumnSource, WreckColumnThickness, WreckColumnSoot);
				}
				if (Col)
				{
					// fora da lista das areias (ela some no fim da gravação, e esta é em loop): a vida e a entrada/saída são daqui
					Dusts.RemoveAll([Col](const FDust& D) { return D.E.Get() == Col; });
					Col->HoldAge = float(It.Value().WreckUntil - Now2) + 0.5f;
					Col->RaymarchEmit = WreckColumnEmit;
					Col->RaymarchYawRad = WindYawRad;
					Col->Seed = float(Unit->GetUniqueID() % 97u) * 0.37f;
					It.Value().Column = Col;
					if (!It.Value().bWreckLightOn)
					{
						Fx->StartWreckFire(float(It.Value().WreckUntil - Now2), false);
					}
				}
				else
				{
					It.Value().Heavy = Fx->StartSmoke(0.7f, 0.7f, 5.0f, 0.06f, 0.85f, 1.6f);
					if (It.Value().bWreckLightOn)
					{
						Fx->EnableWreckFlames();
					}
					else
					{
						Fx->StartWreckFire(float(It.Value().WreckUntil - Now2));
					}
				}
			}
			if (AExplosionVolume* Col = It.Value().Column.Get())
			{
				const float In = FMath::Clamp(float(Now2 - It.Value().WreckStart) / WreckColumnInS, 0.0f, 1.0f);
				const float Out = FMath::Clamp(float(It.Value().WreckUntil - Now2) / WreckColumnOutS, 0.0f, 1.0f);
				Col->Opacity = WreckColumnOpacity * FMath::SmoothStep(0.0f, 1.0f, In) * FMath::SmoothStep(0.0f, 1.0f, Out);
			}
			if (It.Value().Heavy >= 0)
			{
				Fx->SetTrailHead(It.Value().Heavy, At + FVector(0.0, 0.0, RingUpCm), bOn);
			}
			Fx->SetFirePos(At + FVector(0.0, 0.0, RingUpCm - 20.0));
			if (Now2 >= It.Value().WreckUntil + 4.0)
			{
				if (AExplosionVolume* Col = It.Value().Column.Get())
				{
					Col->Destroy();
				}
				Fx->SetLifeSpan(1.0f);
				It.RemoveCurrent();
			}
			continue;
		}
		if (It.Value().Light >= 0)
		{
			Fx->SetTrailHead(It.Value().Light, At + Back * DeckBackCm + FVector(0.0, 0.0, DeckUpCm), true);
		}
		for (int32 k = 0; k < 3; ++k)
		{
			if (It.Value().Dbg[k] >= 0)
			{
				const double Tn = GetWorld()->GetTimeSeconds() * 3.0;
				const FVector Spin = k == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Tn), FMath::Sin(Tn), 0.0) * 150.0;
				Fx->SetTrailHead(It.Value().Dbg[k], At + FVector::CrossProduct(FVector::UpVector, Back) * (450.0 + 300.0 * k) + Spin + FVector(0.0, 0.0, DeckUpCm), true);
			}
		}
		if (It.Value().Heavy >= 0)
		{
			Fx->SetTrailHead(It.Value().Heavy, At + Back * (0.5f * DeckBackCm) + FVector(0.0, 0.0, RingUpCm), true);
		}
		if (bLogNow)
		{
			UE_LOG(LogTemp, Display, TEXT("BlastFx ferido: %s estágio %d, fumaça clara %d pontos (%d), escura %d pontos (%d), em %s, ator %s tique %d"),
				*Unit->GetName(), It.Value().Stage, Fx->TrailPointCount(It.Value().Light), Fx->TrailEmitting(It.Value().Light) ? 1 : 0,
				Fx->TrailPointCount(It.Value().Heavy), Fx->TrailEmitting(It.Value().Heavy) ? 1 : 0, *At.ToString(), *Fx->GetName(),
				Fx->IsActorTickEnabled() ? 1 : 0);
		}
	}
}

void UBlastFxDirector::SetWind(float YawDeg, float SpeedMs)
{
	WindYawRad = FMath::DegreesToRadians(YawDeg);
	WindSpeedMs = SpeedMs;
}

void UBlastFxDirector::StartWreck(AActor* Unit)
{
	if (!Unit || !GetWorld())
	{
		return;
	}
	ABlastDebris* Fx = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity);
	if (!Fx)
	{
		return;
	}
	Fx->GlowScale = MuzzleGlow;                     // as chamas laranja (a 1 estourava em branco)
	FDamaged& D = Damaged.Add(Unit);
	D.Fx = Fx;
	D.Stage = 3;
	D.Light = -1;
	D.Heavy = -1;
	const double Now = GetWorld()->GetTimeSeconds();
	D.WreckStart = Now + WreckDelayS;
	D.WreckUntil = D.WreckStart + WreckFireS;
	static const float LightDelay = [] { float V = WreckLightDelayS; FParse::Value(FCommandLine::Get(), TEXT("WreckLightDelay="), V); return V; }();
	D.WreckLightStart = Now + FMath::Min(LightDelay, WreckDelayS);
}

void UBlastFxDirector::MissileLaunch(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	Variant ^= (++ShotSerial) * 0x9E3779B9u;
	const FVector D = Dir.GetSafeNormal();
	const double Now = GetWorld()->GetTimeSeconds();
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->GlowScale = MuzzleGlow;
		L->StartLaunch(PosCm, D, int32(Variant * 2246822519u >> 1));
	}
	if (ABlastDebris* M = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		M->GlowScale = MissileGlow;
		M->TrailTint = FLinearColor(0.94f, 0.94f, 0.95f, 1.0f);   // a fumaça branca do motor (óxido de alumínio)
		M->StartMissile(PosCm, D, int32(Variant * 3266489917u >> 1), !bExternalMissileSmoke);
		FMissileFx& F = Missiles.FindOrAdd(Id);
		if (ABlastDebris* Old = F.Fx.Get())
		{
			Old->EndMissile();
		}
		F.Fx = M;
		F.Seen = Now;
	}
	// a poeira do lançamento quente (o motor sai aceso da caixa e sopra o chão: a nuvem cor de terra das fotos da Patriot),
	// uma por rajada (no máximo uma a cada 0,6 s), ~3 m atrás da caixa, na cor do chão: o golpe em ~0,25 s e o resto
	// pairando ~1,5 s, abaixo de ~2,5 m
	if (Now - LastLaunchDust > 0.6)
	{
		LastLaunchDust = Now;
		const FVector Back = -FVector(D.X, D.Y, 0.0).GetSafeNormal() * 300.0;
		const FVector At(PosCm.X + Back.X, PosCm.Y + Back.Y, BlastFxGround::HeightAtCached(GetWorld(), PosCm.X + Back.X, PosCm.Y + Back.Y));
		if (AExplosionVolume* Dz = SpawnDust(At, LaunchDustScale, LaunchDustTime, LaunchDustOpacity, 0, Variant, LaunchDustColor))
		{
			Dz->SlowAfterS = 0.5f;
			Dz->SlowTimeScale = 0.35f;
		}
	}
}

void UBlastFxDirector::MissileMove(int32 Id, const FVector& PosCm, const FVector& Dir, float SpeedCmPerS)
{
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->UpdateMissile(PosCm, Dir);
			F->Seen = GetWorld() ? GetWorld()->GetTimeSeconds() : F->Seen;
		}
	}
}

void UBlastFxDirector::MissileHit(int32 Id, const FVector& PointCm, AActor* Victim, float GroundZCm, bool bKill)
{
	if (FMissileFx* F = Missiles.Find(Id))
	{
		if (ABlastDebris* M = F->Fx.Get())
		{
			M->EndMissile();
		}
		Missiles.Remove(Id);
	}
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	const uint32 Variant = uint32(Id) * 2654435761u ^ (++ShotSerial) * 0x9E3779B9u;
	const double Now = GetWorld()->GetTimeSeconds();
	FVector Normal = FVector::UpVector;
	if (Victim)
	{
		Normal = (PointCm - Victim->GetActorLocation()).GetSafeNormal2D();
		Normal = (Normal + FVector(0.0, 0.0, 0.35)).GetSafeNormal();
	}
	if (ABlastDebris* H = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		H->GlowScale = ImpactGlow;
		H->StartMissileHit(PointCm, Normal, int32(Variant * 2654435761u >> 1));
	}
	if (Victim)
	{
		FVector Local = FVector::ZeroVector;
		if (UStaticMeshComponent* Part = PartNear(Victim, PointCm, Local))
		{
			MarkHull(Part, Local, 1);
		}
	}
	// a bola da ogiva: uma carga explosiva de 3–5 kg (bola de ~1 m de raio por kg^⅓, LANL) é o fenômeno da Carga Pesada,
	// não a morte de tanque: a gravação dela pequena (~3 m de bola), no relógio do tamanho (o fogo passa rápido). Só o 1º
	// acerto da rajada no mesmo alvo (no máximo uma a cada 0,6 s); os outros ficam com o clarão e as faíscas
	const double* Last = Victim ? LastWarheadBall.Find(Victim) : nullptr;
	if (LoadAssets() && (!Last || Now - *Last > 0.6))
	{
		if (Victim)
		{
			LastWarheadBall.Add(Victim, Now);
		}
		if (AExplosionVolume* Ball = Spawn(FVector(PointCm.X, PointCm.Y, FMath::Max(double(GroundZCm), PointCm.Z - 150.0)), Bakes[Variant % uint32(Bakes.Num())],
			WarheadScale, VariantFraction(Variant, 0xB0B0)))
		{
			Ball->bLights = false;                  // a luz é a dos destroços (a compensação pela exposição)
			Ball->bFlash = false;
			Ball->RaymarchSoot = WarheadSoot;
			Alive.Add(Ball);
		}
	}
	if (bKill)
	{
		EndDamaged(Victim);
		StartWreck(Victim);
	}
}

void UBlastFxDirector::DefenseAssist(const FVector& FromCm, const FVector& ToCm)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		L->GlowScale = MissileGlow;
		L->StartLaser(FromCm, ToCm, 0.5f);
	}
}

void UBlastFxDirector::CarCrushStart(AActor* Car, const FVector& TankDirCm, float PassSeconds, uint32 Variant, bool bAlarm)
{
	if (!ShotFxEnabled() || !Car || !GetWorld())
	{
		return;
	}
	if (Crushes.ContainsByPredicate([Car](const FCrush& C) { return C.Car.Get() == Car; }))
	{
		return;
	}
	FCrush& C = Crushes.AddDefaulted_GetRef();
	C.Car = Car;
	const FBox Box = Car->GetComponentsBoundingBox(true);
	C.Center = Box.GetCenter();
	C.Fwd = Car->GetActorForwardVector().GetSafeNormal2D();
	C.Right = FVector::CrossProduct(FVector::UpVector, C.Fwd);
	// o tamanho pela caixa no eixo do carro (a caixa do mundo girada: o maior lado é o comprimento)
	const FVector Ext = Box.GetExtent();
	const float A = float(FMath::Abs(C.Fwd.X) * Ext.X + FMath::Abs(C.Fwd.Y) * Ext.Y);
	const float B = float(FMath::Abs(C.Right.X) * Ext.X + FMath::Abs(C.Right.Y) * Ext.Y);
	C.HalfLen = FMath::Clamp(FMath::Max(A, B), 150.0f, 320.0f);
	C.HalfWid = FMath::Clamp(FMath::Min(A, B), 70.0f, 120.0f);
	if (B > A)
	{
		Swap(C.Fwd, C.Right);                       // a malha com a frente no Y: o comprimento é o outro eixo
		C.Right = FVector::CrossProduct(FVector::UpVector, C.Fwd);
	}
	C.Height = float(FMath::Clamp(Box.Max.Z - Box.Min.Z, 100.0, 220.0));
	C.GroundZ = float(Box.Min.Z);
	C.TankDir = TankDirCm.GetSafeNormal2D();
	C.T0 = GetWorld()->GetTimeSeconds();
	C.Pass = FMath::Clamp(PassSeconds, 0.6f, 5.0f);
	C.Variant = Variant ^ (++ShotSerial) * 0x9E3779B9u;
	// a carcaça queimada (SM_civ_car_burnt e as amassadas dela): já sem vidro, lanterna, pneu nem bateria (nem alarme)
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Car);
	for (const UStaticMeshComponent* M : Meshes)
	{
		if (M->GetStaticMesh() && M->GetStaticMesh()->GetName().Contains(TEXT("burnt")))
		{
			C.bBurnt = true;
		}
	}
	static const bool bLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
	if (bLog)
	{
		UE_LOG(LogTemp, Display, TEXT("BlastFx carro: começo %s, centro %s, meio comprimento %.0f, meia largura %.0f, altura %.0f, queimado %d, passagem %.2f s"),
			*Car->GetName(), *C.Center.ToString(), C.HalfLen, C.HalfWid, C.Height, C.bBurnt ? 1 : 0, C.Pass);
	}
	if (bAlarm && !C.bBurnt)
	{
		if (ABlastDebris* L = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
		{
			L->StartBlink(C.Center + FVector(0.0, 0.0, 0.3 * C.Height), 10.0f + 20.0f * VariantFraction(C.Variant, 0xA1A1));
		}
	}
}

void UBlastFxDirector::CarCrushEnd(AActor* Car)
{
	for (FCrush& C : Crushes)
	{
		if (C.Car.Get() == Car)
		{
			C.bEnded = true;
		}
	}
}

void UBlastFxDirector::UpdateCrushes()
{
	if (!Crushes.Num() || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 I = Crushes.Num() - 1; I >= 0; --I)
	{
		FCrush& C = Crushes[I];
		const float F = float((Now - C.T0) / C.Pass);
		if (C.bEnded || F > 1.2f)
		{
			static const bool bLogEnd = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), V); return V != 0; }();
			if (bLogEnd)
			{
				UE_LOG(LogTemp, Display, TEXT("BlastFx carro: fim (%s) na fração %.2f, etapas feitas 0x%x"), C.bEnded ? TEXT("CarCrushEnd") : TEXT("passagem"), F, C.Done);
			}
			Crushes.RemoveAtSwap(I);
			continue;
		}
		// o lado que o tanque pega primeiro: a ponta do carro virada pra ele
		const float Toward = FVector::DotProduct(C.Fwd, -C.TankDir) >= 0.0f ? 1.0f : -1.0f;
		const FVector Near = C.Center + C.Fwd * (Toward * C.HalfLen * 0.85f);
		const FVector Far = C.Center - C.Fwd * (Toward * C.HalfLen * 0.85f);
		const FVector Lamp(0.0, 0.0, 0.55 * C.Height - (C.Center.Z - C.GroundZ));
		auto Due = [&C, F](int32 Bit, float At)
		{
			if (!(C.Done & (1 << Bit)) && F >= At)
			{
				C.Done |= 1 << Bit;
				return true;
			}
			return false;
		};
		auto Debris = [this]() { return GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity); };
		// as lascas de uma cor (lanterna, borracha, ferrugem, tinta queimada)
		auto Chips = [&Debris](const FVector& At, const FVector& Dir, const FLinearColor& Tint, float Rough, int32 Count, float SizeMin, float SizeMax, float SpeedMin, float SpeedMax, int32 Seed)
		{
			if (ABlastDebris* L = Debris())
			{
				L->PlateTint = Tint;
				L->PlateRoughNow = Rough;
				L->StartChips(At, Dir, Count, SizeMin, SizeMax, SpeedMin, SpeedMax, Seed);
			}
		};
		const uint32 V = C.Variant;
		// 0: a lagarta encosta: a poeira do contato (rente, na cor do chão) e a ponta: as lanternas (vermelha e âmbar) ou,
		// na carcaça, a ferrugem e a tinta queimada que se soltam
		if (Due(0, 0.0f))
		{
			const AExplosionVolume* Dz = SpawnDust(FVector(Near.X, Near.Y, C.GroundZ), CrushDustScale, CrushDustTime, CrushDustOpacity, 0, V, CrushGray, nullptr, CrushThick);
			static const bool bLog0 = [] { int32 Vl = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), Vl); return Vl != 0; }();
			if (bLog0)
			{
				UE_LOG(LogTemp, Display, TEXT("BlastFx carro: etapa 0 em %s, poeira %s, poeiras vivas %d"), *Near.ToString(), Dz ? TEXT("nasceu") : TEXT("NAO nasceu"), Dusts.Num());
			}
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector At = Near + Lamp + C.Right * ((k == 0 ? 0.7f : -0.7f) * C.HalfWid);
				if (C.bBurnt)
				{
					Chips(At, C.Fwd * Toward, k == 0 ? CrushRust : CrushChar, 0.85f, 6, 0.06f, 0.16f, 1.0f, 3.0f, int32(V * (17u + k)));
				}
				else
				{
					Chips(At, C.Fwd * Toward, k == 0 ? CrushLampRed : CrushLampAmber, 0.2f, 6, 0.08f, 0.2f, 1.5f, 4.0f, int32(V * (17u + k)));
				}
			}
		}
		// 0,2T: as laterais da frente cedem: os vidros estouram (piscando ao sol) ou, na carcaça, a cinza sai pelas janelas
		if (Due(1, 0.2f))
		{
			const FVector Front = C.Center + C.Fwd * (Toward * C.HalfLen * 0.25f);
			if (C.bBurnt)
			{
				SpawnDust(FVector(Front.X, Front.Y, C.GroundZ + 0.3 * C.Height), WindowDustScale, WindowDustTime, AshDustOpacity, 0, V ^ 0x3131u, CrushAsh, nullptr, CrushThick);
				Chips(Front + FVector(0.0, 0.0, 0.4 * C.Height), C.Right, CrushRust, 0.85f, 6, 0.03f, 0.1f, 1.0f, 2.5f, int32(V * 31u));
			}
			else
			{
				for (int32 k = 0; k < 2; ++k)
				{
					const float Side = k == 0 ? 1.0f : -1.0f;
					if (ABlastDebris* G = Debris())
					{
						G->GlowScale = GlassGlow;
						G->StartGlass(Front + C.Right * (Side * C.HalfWid) + FVector(0.0, 0.0, 0.25 * C.Height), C.Right * Side, int32(V * (31u + k)));
					}
				}
			}
		}
		// 0,35T e 0,8T: os eixos: o pneu estoura (um sopro de poeira e pedaços de borracha); na carcaça o aro só afunda a
		// areia (o sopro, sem borracha)
		for (int32 k = 0; k < 2; ++k)
		{
			if (Due(2 + k, k == 0 ? 0.35f : 0.8f))
			{
				const FVector Axle = k == 0 ? C.Center + C.Fwd * (Toward * C.HalfLen * 0.6f) : C.Center - C.Fwd * (Toward * C.HalfLen * 0.6f);
				SpawnDust(FVector(Axle.X, Axle.Y, C.GroundZ), TireDustScale, TireDustTime, C.bBurnt ? 0.5f * TireDustOpacity : TireDustOpacity, 0, V ^ (0x7173u + k), FVector4f(-1.0f, 0.0f, 0.0f, 0.0f), nullptr, CrushThick);
				if (!C.bBurnt)
				{
					Chips(Axle + C.Right * C.HalfWid + FVector(0.0, 0.0, 40.0), C.Right, CrushRubber, 0.9f, 4, 0.08f, 0.18f, 1.0f, 3.0f, int32(V * (41u + k)));
				}
			}
		}
		// 0,5T: o teto cede: o vidro de trás estoura e a poeira sai pelas janelas; na carcaça, a nuvem de cinza (a maior)
		if (Due(4, 0.5f))
		{
			if (C.bBurnt)
			{
				SpawnDust(FVector(C.Center.X, C.Center.Y, C.GroundZ + 0.3 * C.Height), AshDustScale, WindowDustTime, AshDustOpacity, 0, V ^ 0x5151u, CrushAsh, nullptr, CrushThick);
				Chips(C.Center + FVector(0.0, 0.0, 0.6 * C.Height), -C.TankDir, CrushChar, 0.85f, 6, 0.04f, 0.14f, 1.0f, 3.0f, int32(V * 53u));
			}
			else
			{
				if (ABlastDebris* G = Debris())
				{
					G->GlowScale = GlassGlow;
					G->StartGlass(Far + FVector(0.0, 0.0, 0.3 * C.Height), -C.Fwd * Toward, int32(V * 53u));
				}
				SpawnDust(FVector(C.Center.X, C.Center.Y, C.GroundZ + 0.3 * C.Height), WindowDustScale, WindowDustTime, WindowDustOpacity, 0, V ^ 0x5151u, WindowGray, nullptr, CrushThick);
			}
		}
		// 0,6T: a outra ponta (as lanternas ou a ferrugem)
		if (Due(5, 0.6f))
		{
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector At = Far + Lamp + C.Right * ((k == 0 ? -0.7f : 0.7f) * C.HalfWid);
				if (C.bBurnt)
				{
					Chips(At, -C.Fwd * Toward, k == 0 ? CrushChar : CrushRust, 0.85f, 6, 0.06f, 0.16f, 1.0f, 3.0f, int32(V * (61u + k)));
				}
				else
				{
					Chips(At, -C.Fwd * Toward, k == 0 ? CrushLampRed : CrushLampAmber, 0.2f, 6, 0.08f, 0.2f, 1.5f, 4.0f, int32(V * (61u + k)));
				}
			}
		}
		// 0,65T: o casco raspa o teto (a faísca, fraca de dia; na carcaça é metal nu, sai igual)
		if (Due(6, 0.65f))
		{
			if (ABlastDebris* S = Debris())
			{
				S->GlowScale = MuzzleGlow;
				S->StartScrape(C.Center + FVector(0.0, 0.0, 0.5 * C.Height), -C.TankDir, int32(V * 71u));
			}
		}
	}
}

void UBlastFxDirector::UpdateCrushTest()
{
	// o teste do carro esmagado sem o jogo ligar a chamada (-CrushFxTest): o jogo troca a instância do carro por um ator
	// próprio (o "Prop") quando o esmaga; aqui ele é achado no quadro seguinte e o tanque mais perto dá a direção e a
	// velocidade. Só pra teste: varre os atores todo quadro
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("CrushFxTest"));
	UWorld* World = GetWorld();
	if (!bOn || !World)
	{
		return;
	}
	const float Dt = World->GetDeltaSeconds();
	TArray<AActor*> Tanks, Cars;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Ac = *It;
		if (const UStaticMeshComponent* M = Cast<UStaticMeshComponent>(Ac->GetRootComponent()); M && M->GetFName() == TEXT("Prop"))
		{
			const FString N = M->GetStaticMesh() ? M->GetStaticMesh()->GetName() : FString();
			if ((N.Contains(TEXT("civ_pickup")) || N.Contains(TEXT("civ_car_burnt"))) && !CrushTestSeen.Contains(Ac))
			{
				Cars.Add(Ac);
			}
			continue;
		}
		for (const UActorComponent* Cp : Ac->GetComponents())
		{
			if (Cp && Cp->GetFName() == TEXT("TurretPivot"))
			{
				Tanks.Add(Ac);
				break;
			}
		}
	}
	for (AActor* Tk : Tanks)
	{
		const FVector P = Tk->GetActorLocation();
		if (const FVector* Prev = CrushTestTankPos.Find(Tk); Prev && Dt > 0.0f)
		{
			float& Sp = CrushTestTankSpeed.FindOrAdd(Tk);
			Sp = FMath::Lerp(Sp, float(FVector::Dist2D(P, *Prev) / Dt), 0.2f);
		}
		CrushTestTankPos.Add(Tk, P);
	}
	for (AActor* Car : Cars)
	{
		CrushTestSeen.Add(Car);
		AActor* Best = nullptr;
		double BestD = 900.0 * 900.0;
		for (AActor* Tk : Tanks)
		{
			const double D = FVector::DistSquared2D(Tk->GetActorLocation(), Car->GetActorLocation());
			if (D < BestD)
			{
				BestD = D;
				Best = Tk;
			}
		}
		if (!Best)
		{
			continue;
		}
		const FVector Ext = Car->GetComponentsBoundingBox(true).GetExtent();
		const float Len = 2.0f * float(FMath::Max(Ext.X, Ext.Y));
		const float Speed = FMath::Max(CrushTestTankSpeed.FindRef(Best), 100.0f);
		const bool bAlarm = (CrushTestSeen.Num() & 1) != 0;
		UE_LOG(LogTemp, Display, TEXT("BlastFx carro: %s esmagado por %s a %.1f m/s, passagem %.2f s%s"), *Car->GetName(), *Best->GetName(),
			Speed / 100.0f, (Len + 450.0f) / Speed, bAlarm ? TEXT(", alarme") : TEXT(""));
		CarCrushStart(Car, Best->GetActorForwardVector() * 100.0, (Len + 450.0f) / Speed, uint32(CrushTestSeen.Num()) * 7919u, bAlarm);
	}
}

void UBlastFxDirector::WireArc(const FVector& PointCm, int32 Kind, uint32 Variant)
{
	if (!ShotFxEnabled() || !GetWorld())
	{
		return;
	}
	const uint32 V = Variant ^ (++ShotSerial) * 0x9E3779B9u;
	if (ABlastDebris* A = GetWorld()->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
	{
		A->GlowScale = ArcGlow;
		A->PlateTint = FLinearColor(0.85f, 0.84f, 0.8f, 1.0f);   // as lascas do isolador: porcelana
		A->PlateRoughNow = 0.35f;
		A->StartArc(PointCm, Kind, int32(V));
	}
	if (Kind == 2)
	{
		const double GroundZ = BlastFxGround::HeightAtCached(GetWorld(), PointCm.X, PointCm.Y);
		SpawnDust(FVector(PointCm.X, PointCm.Y, GroundZ), ArcSmokeScale, ArcSmokeTime, ArcSmokeOpacity, 0, V ^ 0xA3Cu, ArcSmokeColor);
	}
}

void UBlastFxDirector::UpdateArcTest()
{
	// -ArcFxTestX= -ArcFxTestY= (m): a sequência do poste derrubado num ponto, sem o jogo chamar: o fio arrebenta a 8,5 m (4,0 s), a
	// ponta encosta na areia 1,5 m ao lado (4,5 s) e o religador tenta de novo a +2 s e +2,5 s (6,5 s e 9,0 s)
	static const FVector2D At = []
	{
		float X = 1.0e9f, Y = 1.0e9f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ArcFxTestX="), X) && FParse::Value(FCommandLine::Get(), TEXT("ArcFxTestY="), Y))
		{
			return FVector2D(X * 100.0, Y * 100.0);
		}
		return FVector2D(1.0e9, 1.0e9);
	}();
	if (At.X > 1.0e8 || !GetWorld())
	{
		return;
	}
	static const float Times[] = { 6.0f, 6.5f, 8.5f, 11.0f };   // (o vídeo do teste começa ~4,5 s depois da partida)
	static const int32 Kinds[] = { 0, 1, 2, 2 };
	const double Now = GetWorld()->GetTimeSeconds();
	while (ArcTestStep < 4 && Now >= Times[ArcTestStep])
	{
		const int32 K = Kinds[ArcTestStep];
		const double Gz = BlastFxGround::HeightAtCached(GetWorld(), At.X, At.Y);
		const FVector P = K == 0 ? FVector(At.X, At.Y, Gz + 850.0) : FVector(At.X + 150.0, At.Y, Gz + 5.0);
		UE_LOG(LogTemp, Display, TEXT("BlastFx arco: tipo %d em %.0f s"), K, Now);
		WireArc(P, K, 0x5EEDu + uint32(ArcTestStep));
		++ArcTestStep;
	}
}

void UBlastFxDirector::MarkHull(UStaticMeshComponent* Part, const FVector& LocalCm, int32 Kind)
{
	static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("HullMarks="), V); return V != 0; }();
	if (!bOn || !Part || !GetWorld())
	{
		return;
	}
	const float Now = FMath::Max(float(GetWorld()->GetTimeSeconds()), 0.01f);
	const TArray<float>& D = Part->GetCustomPrimitiveData().Data;
	auto At = [&D](int32 I) { return D.IsValidIndex(I) ? D[I] : 0.0f; };
	int32 Same = -1, Empty = -1, Oldest = -1;
	float OldestBorn = TNumericLimits<float>::Max();
	for (int32 k = 0; k < HullMarkSlots; ++k)
	{
		const int32 B = HullMarkBase + 4 * k;
		const float W = At(B + 3);
		const float K = FMath::FloorToFloat(W / 1.0e5f);
		const float Born = W - 1.0e5f * K;
		if (W <= 0.0f || Born > Now)
		{
			Empty = Empty < 0 ? k : Empty;          // vaga livre (ou de outra partida: o relógio voltou)
			continue;
		}
		if (int32(K) == Kind && FVector::Dist(FVector(At(B), At(B + 1), At(B + 2)), LocalCm) < HullMarkMergeCm)
		{
			Same = k;
			break;
		}
		if (Born < OldestBorn)
		{
			OldestBorn = Born;
			Oldest = k;
		}
	}
	const int32 Slot = Same >= 0 ? Same : (Empty >= 0 ? Empty : Oldest);
	if (Slot < 0)
	{
		return;
	}
	const int32 B = HullMarkBase + 4 * Slot;
	// o reforço fica no lugar da marca (a mesma cicatriz, de novo quente)
	const FVector P = Same >= 0 ? FVector(At(B), At(B + 1), At(B + 2)) : LocalCm;
	Part->SetCustomPrimitiveDataVector4(B, FVector4(P.X, P.Y, P.Z, Now + 1.0e5f * float(Kind)));
}

namespace
{
	// o alcance das luzes pras fumaças (17/10): -SmokeLightReach= (1 = o raio da Unreal, o de antes)
	float SmokeLightReach()
	{
		static const float V = [] { float R = 3.0f; FParse::Value(FCommandLine::Get(), TEXT("SmokeLightReach="), R); return R; }();
		return V;
	}
}

void UBlastFxDirector::GatherSceneLights()
{
	// as luzes pontuais acesas da cena (as das explosões, do fogo da carcaça, dos tiros: o que se move ou acende), as 32
	// mais fortes até 250 m da câmera, em candelas (a Unreal converte as unidades), pra toda fumaça ler (RTSSceneLights)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVector CamM = FVector::ZeroVector;
	if (const APlayerController* Pc = World->GetFirstPlayerController(); Pc && Pc->PlayerCameraManager)
	{
		CamM = Pc->PlayerCameraManager->GetCameraLocation() / 100.0;
	}
	struct FCand { RTSSceneLights::FLight L; float Score; };
	TArray<FCand> Cands;
	for (TObjectIterator<UPointLightComponent> It; It; ++It)
	{
		const UPointLightComponent* P = *It;
		if (!P || P->GetWorld() != World || !P->IsRegistered() || !P->IsVisible() || P->Intensity <= 0.0f || P->Mobility == EComponentMobility::Static)
		{
			continue;
		}
		const float Cd = P->Intensity * ULocalLightComponent::GetUnitsConversionFactor(P->IntensityUnits, ELightUnits::Candelas);
		if (Cd <= 1.0f)
		{
			continue;
		}
		const FVector PosM = P->GetComponentLocation() / 100.0;
		const float D2 = float(FVector::DistSquared(PosM, CamM));
		if (D2 > 250.0f * 250.0f)
		{
			continue;
		}
		FCand& C = Cands.AddDefaulted_GetRef();
		C.L.PosM = PosM;
		// o alcance pras fumaças (17/10): o raio da Unreal é corte de desempenho (7–9 m nas luzes das explosões); a luz física
		// não acaba, e a fumaça a 6–20 m de uma morte à noite ficava sem o laranja. × 3: a janela (1 − (d/R)⁴)² fica ~1 até ~2 R
		C.L.RadiusM = P->AttenuationRadius / 100.0f * SmokeLightReach();
		C.L.Candela = P->GetLightColor() * Cd;
		C.Score = Cd / FMath::Max(D2, 100.0f);
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.Score > B.Score; });
	// (17/10, pro A/B da noite) -SmokeLightUncomp=1: as fumaças recebem a luz sem a compensação da exposição dos destroços
	// (ExpoComp, presa a 1/16 de noite: a lua não leva nenhuma). Só a cópia das fumaças; o chão e as unidades seguem iguais
	static const bool bUncomp = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("SmokeLightUncomp="), V); return V != 0; }();
	if (bUncomp)
	{
		static const float RefExpo = [] { float V = 6.95e-5f; FParse::Value(FCommandLine::Get(), TEXT("DebrisRefExpo="), V); return V; }();
		const float E = BlastRaymarch::LastViewExposure();
		const float Comp = E > 0.0f ? FMath::Clamp(RefExpo / E, 1.0f / 16.0f, 4.0f) : 1.0f;
		for (FCand& C : Cands)
		{
			C.L.Candela /= Comp;
		}
	}
	TArray<RTSSceneLights::FLight> Out;
	for (int32 k = 0; k < Cands.Num() && k < RTSSceneLights::MaxLights; ++k)
	{
		Out.Add(Cands[k].L);
	}
	// -SceneLightsLog=1 (17/10, a prova da poeira acesa pela carcaça): a lista, uma vez por segundo
	static const bool bLightsLog = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("SceneLightsLog="), V); return V != 0; }();
	static double LastLightsLog = -10.0;
	if (bLightsLog && World->GetTimeSeconds() - LastLightsLog >= 1.0)
	{
		LastLightsLog = World->GetTimeSeconds();
		FString Txt;
		for (int32 k = 0; k < Out.Num() && k < 6; ++k)
		{
			Txt += FString::Printf(TEXT(" [(%.1f, %.1f, %.1f) m, raio %.1f m, cd (%.0f, %.0f, %.0f)]"), Out[k].PosM.X, Out[k].PosM.Y, Out[k].PosM.Z, Out[k].RadiusM,
				Out[k].Candela.R, Out[k].Candela.G, Out[k].Candela.B);
		}
		UE_LOG(LogTemp, Display, TEXT("BlastFx luzes da cena aos %.1f s: %d de %d candidatas;%s"), World->GetTimeSeconds(), Out.Num(), Cands.Num(), *Txt);
	}
	RTSSceneLights::Set_GameThread(MoveTemp(Out));
}

void UBlastFxDirector::UpdateShadowTest()
{
	// -ShadowFxTest=1 (16/10 noite, a prova da regra do CLAUDE.md: a fumaça recebe a sombra do mapa): tiros na areia em volta
	// da casa 22 da vila do World_TestSalt2 (-14, 32), pela direção do sol: dois no lado da sombra (a 9 e 16 m do meio da
	// casa) e um no lado do sol (a 12 m); o log escreve a visibilidade do sol do mapa comum (a consulta da Arte) em cada um,
	// a 0,5, 2 e 4 m. Com -RmDebug=4 a fumaça mostra a mesma visibilidade (branco ao sol, preto na sombra do mapa)
	// -ShadowFxTest=2 (a fase 2, uma fumaça na sombra da outra): uma morte de tanque 45 m da casa na direção do sol, aos
	// 7 s (o vídeo começa ~4,5 s depois da partida), e três tiros na areia na sombra da coluna dela (12, 20 e 30 m pro lado
	// da casa), aos 7,6, 8,2 e 8,8 s, enquanto a coluna está densa (a gravação eg_tq_v2 dura ~3 s). Com
	// -RmDebug=5 a fumaça mostra o sol pelas outras explosões; -RmSmokeShadow=0 é o A/B
	static const int32 Mode = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShadowFxTest="), V); return V; }();
	UWorld* World = GetWorld();
	if (Mode == 0 || !World)
	{
		return;
	}
	FVector ToSun = FVector(0.3, 0.3, 0.9).GetSafeNormal();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (const UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()); L && L->IsUsedAsAtmosphereSunLight())
		{
			ToSun = -L->GetDirection();
		}
	}
	const FVector2D Away = -FVector2D(ToSun.X, ToSun.Y).GetSafeNormal();
	const FVector2D C(-14.0, 32.0);
	const FVector2D Points[] = { C + Away * 9.0, C + Away * 16.0, C - Away * 12.0 };
	const double Now = World->GetTimeSeconds();
	if (Mode == 4)
	{
		// -ShadowFxTest=4 (17/10, a bancada da carcaça em volume): a coluna sim_carcaca solta 30 m ao norte da casa 22 (do lado
		// do sol, em campo aberto), aos 5 s, por 30 s, girada pro vento, com a luz do fogo dela (os destroços sem as chamas)
		if (ShadowTestStep == 0 && Now >= 5.0)
		{
			++ShadowTestStep;
			const FVector2D P = (C - Away * 30.0) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			if (AExplosionVolume* Col = SpawnDust(FVector(P.X, P.Y, Gz), 1.0f, 1.0f, WreckColumnOpacity, 1, 0xCA2CA5u, FVector4f(-1.0f, 0.0f, 0.0f, 0.0f),
				WreckColumnSource, WreckColumnThickness, WreckColumnSoot))
			{
				Dusts.RemoveAll([Col](const FDust& D) { return D.E.Get() == Col; });
				Col->HoldAge = 30.0f;
				Col->RaymarchEmit = WreckColumnEmit;
				Col->RaymarchYawRad = WindYawRad;
			}
			if (ABlastDebris* Fx = World->SpawnActor<ABlastDebris>(ABlastDebris::StaticClass(), FTransform::Identity))
			{
				Fx->GlowScale = MuzzleGlow;
				Fx->StartWreckFire(30.0f, false);
				Fx->SetFirePos(FVector(P.X, P.Y, Gz + RingUpCm - 20.0));
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx carcaça (bancada): a coluna em (%.1f, %.1f), vento %.0f°"), P.X / 100.0, P.Y / 100.0, FMath::RadiansToDegrees(WindYawRad));
		}
		return;
	}
	if (Mode == 3)
	{
		// -ShadowFxTest=3 (a prova direta da fase 2): duas mortes de tanque na mesma reta do sol, a 8 m uma da outra, aos 7,0
		// e 7,3 s: a de baixo do sol (a 2ª) fica na sombra da 1ª. Com -RmDebug=5 a 2ª tem de sair escura do lado do sol
		const FVector2D B = C - Away * 45.0;
		while (ShadowTestStep < 2 && Now >= 7.0 + 0.3 * double(ShadowTestStep))
		{
			const FVector2D P = (B + Away * (8.0 * double(ShadowTestStep))) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			TankDeath(FVector(P.X, P.Y, Gz), 0xDEADu + uint32(ShadowTestStep));
			UE_LOG(LogTemp, Display, TEXT("BlastFx sombra (fase 2, direta): morte %d em (%.1f, %.1f) aos %.1f s"), ShadowTestStep, P.X / 100.0, P.Y / 100.0, Now);
			++ShadowTestStep;
		}
		return;
	}
	if (Mode == 2)
	{
		const FVector2D B = C - Away * 45.0;
		const FVector2D Hits[] = { B + Away * 12.0, B + Away * 20.0, B + Away * 30.0 };
		while (ShadowTestStep < 4 && Now >= 7.0 + 0.6 * double(ShadowTestStep))
		{
			const FVector2D P = (ShadowTestStep == 0 ? B : Hits[ShadowTestStep - 1]) * 100.0;
			const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
			if (ShadowTestStep == 0)
			{
				TankDeath(FVector(P.X, P.Y, Gz), 0xDEADu);
			}
			else
			{
				GroundHit(FVector(P.X, P.Y, Gz), FVector(P.X, P.Y - 4000.0, Gz + 300.0), 0xC0FFEEu + uint32(ShadowTestStep));
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx sombra (fase 2): %s em (%.1f, %.1f) aos %.1f s"), ShadowTestStep == 0 ? TEXT("a morte") : TEXT("tiro"),
				P.X / 100.0, P.Y / 100.0, Now);
			++ShadowTestStep;
		}
		return;
	}
	while (ShadowTestStep < 3 && Now >= 4.0 + 3.0 * ShadowTestStep)
	{
		const FVector2D P = Points[ShadowTestStep] * 100.0;
		const double Gz = BlastFxGround::HeightAtCached(World, P.X, P.Y);
		FString Vis;
		for (const double Hm : { 0.5, 2.0, 4.0 })
		{
			Vis += FString::Printf(TEXT(" %.0f m: %.2f"), Hm, RTSSunShadow::SunVisibilityCPU(World, FVector(P.X, P.Y, Gz) / 100.0 + FVector(0.0, 0.0, Hm), 0.2));
		}
		UE_LOG(LogTemp, Display, TEXT("BlastFx sombra: tiro %d em (%.1f, %.1f) aos %.1f s, sol pra (%.2f, %.2f, %.2f); a visibilidade do sol no mapa:%s"),
			ShadowTestStep, P.X / 100.0, P.Y / 100.0, Now, ToSun.X, ToSun.Y, ToSun.Z, *Vis);
		GroundHit(FVector(P.X, P.Y, Gz), FVector(P.X, P.Y - 4000.0, Gz + 300.0), 0xC0FFEEu + uint32(ShadowTestStep));
		++ShadowTestStep;
	}
}
