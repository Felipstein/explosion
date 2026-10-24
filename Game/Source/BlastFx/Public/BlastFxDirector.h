#pragma once

// O diretor das explosões no jogo: o que o UBlastLabSimulation faz pras
// explosões no simulador, sem o resto do laboratório (painel, armas de teste,
// passagens de avião). Dá às explosões o relógio, a vista da câmera (pra saber
// o quanto cada uma ocupa da tela) e o orçamento de detalhe; dispara a morte de
// tanque; aquece os shaders na abertura.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlastFxDirector.generated.h"

class AExplosionVolume; namespace BlastFxHitLife { struct FHitIn; struct FHitOut; struct FLanding; struct FEmberSmoke; }   // (numa linha só: o GENERATED_BODY não muda de linha, o cc_check usa o .generated.h da última compilação)
class UExplosionBake;
class UMaterialInterface; class UStaticMeshComponent;

namespace BlastFx
{
	/** O quanto a deformação pelo que passa empurra a fumaça (BattleSmokeSim.h no BlastLab). Sem campo no jogo ainda. */
	constexpr float WarpMaxLocalM = 8.0f;
}

UCLASS()
class BLASTFX_API UBlastFxDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBlastFxDirector* Get(const UObject* WorldContext);

	/** O relógio das explosões (para com a partida pausada). */
	float SimDelta(float DeltaSeconds) const { return bPaused ? 0.0f : DeltaSeconds; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	/** Fração da meia altura da tela que a esfera ocupa (0 fora da vista), como no BlastLab. */
	float ScreenFraction(const FVector& Center, float Radius, bool& bInView) const;
	float AmbientLux() const { return 52000.0f; }

	/** A cor da poeira que a explosão levanta (a areia do deserto). */
	FLinearColor DustTint = FLinearColor(0.80f, 0.68f, 0.55f);

	/**
	 * A morte de um tanque: a Carga Pesada aprovada (uma das 3 variantes) na
	 * proporção medida contra o vídeo da cena herói (bola 0.55, poeira com 40% da
	 * altura e metade da vida; BlastLabSimulation.cpp Detonate). Variant escolhe a
	 * gravação e o ruído (sorteio só visual).
	 */
	/** WreckOf (17/10): o ator do tanque morto; com ele a carcaça acende pela morte, qualquer que seja o caminho dela (antes
	 *  só o tiro que matava pelo ArmorHit acendia, e a morte por outro caminho ficava sem fogo). */
	AExplosionVolume* TankDeath(const FVector& Ground, uint32 Variant, bool bClassic = false, AActor* WreckOf = nullptr);

	/**
	 * O tiro (14/10, a v3; o Felipe: o acerto como a arte 4.2, a areia que o sopro do disparo levanta em volta da torre,
	 * a areia da onda de choque no tanque atingido, o projétil com luz). Chamados pelo jogo nos eventos da simulação
	 * (WeaponFired, o traçante, ProjectileHit). -ShotFx=0 desliga: o jogo volta pro provisório dele.
	 *  - MuzzleBlast: a boca do cano (cm) e a direção; o chão embaixo (cm) pra areia do sopro.
	 *  - Tracer: o projétil de From a To (cm) na velocidade da simulação (cm/s).
	 *  - ArmorHit: o acerto no tanque Target: o ponto do jogo (o centro) e de onde veio o tiro; o efeito nasce na caixa
	 *    do próprio tanque, no lado virado pro atirador. bKill: o tiro que matou (a morte vem junto: só o clarão e as faíscas).
	 *  - GroundHit: o tiro no chão (o Ctrl, e os que passam raspando).
	 */
	static bool ShotFxEnabled();
	void MuzzleBlast(const FVector& MuzzleCm, const FVector& Dir, float GroundZCm, uint32 Variant);
	void Tracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, uint32 Variant);
	void ArmorHit(const FVector& AtCm, const FVector& FromCm, AActor* Target, float GroundZCm, uint32 Variant, bool bKill);
	void GroundHit(const FVector& AtCm, const FVector& FromCm, uint32 Variant);

	/**
	 * O tanque ferido (16/10): o jogo passa a vida (0–1) de cada veículo vivo a cada quadro (ARTSUnitActor::SetDamage).
	 * Abaixo de 70% um fio de fumaça clara sobe do deck do motor; abaixo de 35%, uma vez, o jato de chama pelo anel da
	 * torre e a fumaça escura e grossa (os limiares do ZH, UnitDamagedThreshold/UnitReallyDamagedThreshold). Na morte
	 * a fumaça para (a morte aprovada assume).
	 */
	void VehicleDamage(AActor* Unit, float Health01);
	/** (22/10) A carcaça vai embora (começou a afundar, ou a simulação a tirou): o fogo dela, se ainda queima, se apaga em
	 *  FadeS (a luz, a fumaça e as chamas); 0 = já. A que nem acendeu não acende mais. */
	void WreckGone(AActor* Unit, float FadeS);

	/**
	 * A Patriot (16/10; o estudo: docs/ESTUDO_MISSIL_PATRIOT.md). O jogo chama:
	 *  - MissileLaunch no disparo (Id = o do tiro; Pos e Dir na boca da caixa);
	 *  - MissileMove a cada quadro com o míssil interpolado (a chama, a luz e o rastro andam junto);
	 *  - MissileHit no contato (a explosão da ogiva; bKill = o tiro que matou);
	 *  - DefenseAssist quando uma bateria ajuda outra (o laser de mira vermelho entre elas, o LaserFromAssisted do ZH).
	 */
	void MissileLaunch(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant);
	/** (v4, 23/10) O míssil que já nasce voando (o Cruise Missile Strike, sem atirador) ou que entra na vista vindo da névoa:
	 *  a chama, a luz e o rastro do MissileLaunch, sem o clarão da saída e sem a poeira do lançamento. Não faz nada se o míssil
	 *  Id já tem a chama (pode ser chamado a cada quadro). */
	void MissileInFlight(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant);
	void MissileMove(int32 Id, const FVector& PosCm, const FVector& Dir, float SpeedCmPerS);
	/** (23/10, a ogiva v2) FromCm: de onde o míssil vinha (o passo de antes, no ar): a ogiva explode do lado de fora da blindagem,
	 *  virada pra ele. WarheadKg: a carga da ogiva em kg de TNT (Units.json "warhead_kg", só desenho): a bola de fogo, a fuligem e
	 *  o anel de areia em volume, pela escala de Hopkinson-Cranz a partir da Carga Pesada (o anel, a partir do tiro de canhão na
	 *  areia); 0 = sem volume (só o clarão, as faíscas, a luz e a marca, sem o brilho em sprite: a metade pesada do ataque de mísseis,
	 *  que ganha a clássica logo depois). -MissileBlastV=1 volta a de antes, exata.
	 *  (23/10, a revisão) bClassicLook: a fuligem, o sol, o céu e o chão da clássica (Units.json "warhead_look": "classic"; a metade
	 *  leve do ataque de mísseis, ao lado da pesada clássica), em vez dos da morte de tanque e dos tiros. Devolve o centro da bola de
	 *  fogo que se vê (cm; o pé fora da blindagem, virado pro míssil, na altura do miolo quente), pro empurrão e o calor no rastro do
	 *  míssil; sem a ogiva v2, o PointCm. */
	FVector MissileHit(int32 Id, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill, float WarheadKg,
		bool bClassicLook = false, const BlastFxHitLife::FHitIn* LifeIn = nullptr, BlastFxHitLife::FHitOut* LifeOut = nullptr);
	/**
	 * (23/10, a explosão com vida; BlastFxHitLife.h) Com LifeIn e -HitLifeV=2 (o padrão) o MissileHit é o com vida: a direção ℓ (a bola nasce
	 * no ponto do casco ou do chão e sai torta pela física: a normal da face, o que o míssil traz ao longo dela, o mergulho guardado), a
	 * nuvem que continua viva depois da gravação, os brilhos no casco, as faíscas em jatos, os pedaços em brasa, a terra do míssil no chão
	 * na escala da carga, as gravações dedicadas quando existem. Cada camada atrás da sua variável blast.HitLife.<camada>. LifeOut (bValid)
	 * leva pro jogo o ponto, a normal, d, ℓ, o pé, a bola e os relógios (o pulso). Sem LifeIn, ou com -HitLifeV=1, o de hoje, exato.
	 */
	/** (23/10, a vida) Os brilhos dos estilhaços (1–2 quadros) nas faces das peças de Target viradas pra FromCm, a até RadiusCm de NearCm. */
	void HullGlints(AActor* Target, const FVector& FromCm, const FVector& NearCm, float RadiusCm, int32 Count, uint32 Seed);
	/** (23/10, a vida; a camada Neighbors) O vizinho do dano em área: a marca do míssil (tipo 1) na face virada pra explosão. */
	void NeighborMark(AActor* Neighbor, const FVector& BlastCm, uint32 Seed);
	/** (23/10, a vida) A fila de pousos (os pedaços em brasa e os torrões que levantam areia): os destroços põem, o jogo esvazia a cada quadro. */
	void PushLanding(const BlastFxHitLife::FLanding& L);
	void ConsumeLandings(TArray<BlastFxHitLife::FLanding>& Out);
	/** (a revisão da vida) Os pedaços em brasa queimando em voo: os destroços põem o ponto a cada quadro, o jogo esvazia (o fio de fumaça em
	 *  volume no RTSMissileSmoke, no lugar da fita chapada). */
	void PushEmberSmoke(uint32 Id, const FVector& PosCm);
	void ConsumeEmberSmoke(TArray<BlastFxHitLife::FEmberSmoke>& Out);
	/** (23/10) A escala da ogiva de Kg kg: λ = 0,55·(W/20 kg)^⅓. A âncora: a clássica do jogo (a Carga Pesada a 0,55, a TankBallScale)
	 *  é a carga de 20 kg (o nominal, instances.js MAGNITUDES; e a bola dela, 2,75 m, dá 20,8 kg pela LANL, ~1 m por kg^⅓). */
	static float WarheadScale(float Kg) { return 0.55f * FMath::Pow(FMath::Max(Kg, 0.01f) / 20.0f, 1.0f / 3.0f); }
	/** O raio da bola de fogo (m): os 5,0 m medidos na gravação da Carga Pesada (T > 0,5 de 0,28 a 0,9 s) × λ. */
	static float WarheadFireballM(float Kg) { return 5.0f * WarheadScale(Kg); }
	/** -MissileBlastV= (2, o padrão: a ogiva v2; 1: a de antes, exata, no BlastFx e no jogo). */
	static int32 MissileBlastVer();
	void DefenseAssist(const FVector& FromCm, const FVector& ToCm);

	/**
	 * O tanque esmagando o carro civil (16/10; o estudo: docs/ESTUDO_ESMAGAR_E_POSTE.md). CarCrushStart quando a lagarta
	 * encosta (o 1º PropCrushed): o efeito segue a fração T da passagem (PassSeconds = (comprimento do carro + 4,5 m) ÷
	 * velocidade), nos pontos da caixa do carro: lanternas, vidros que estouram e piscam ao sol, pneus, poeira, o raspão do
	 * casco. bAlarm: o pisca-alerta âmbar (o alarme do ZH em 50% dos carros). CarCrushEnd: o tanque parou ou morreu em cima.
	 */
	void CarCrushStart(AActor* Car, const FVector& TankDirCm, float PassSeconds, uint32 Variant, bool bAlarm = false);
	void CarCrushEnd(AActor* Car);

	/**
	 * O arco do fio de poste (16/10; o estudo: docs/ESTUDO_ESMAGAR_E_POSTE.md, parte B), no lugar do WireArc provisório do
	 * jogo. Kind 0: o fio arrebentou no ar (os fios se tocam: o clarão rápido, as gotas, as lascas do isolador); 1: a ponta
	 * viva encostou no chão (o clarão rápido rente à areia); 2: a religada (o arco de 0,6–1,0 s pulando pela areia, as gotas
	 * e a fumacinha). O religador do estudo (padrão do OpenDSS): o 1º a +0,5 s, depois +2 s e +2 s.
	 */
	void WireArc(const FVector& PointCm, int32 Kind, uint32 Variant = 0);
	/** O vento do mapa (17/10; o jogo chama no começo da partida, RTSGameData WindYawDeg/WindSpeedMs): pra onde ele sopra
	 *  (graus, 0 = +X). A coluna da carcaça deita pra esse lado. */
	void SetWind(float YawDeg, float SpeedMs);
	/** O jogo desenha a fumaça do rastro do míssil (RTSMissileSmoke): o BlastFx fica só com a chama, a luz, o lançamento e o acerto. */
	static void SetExternalMissileSmoke(bool bOn) { bExternalMissileSmoke = bOn; }
	static bool HasExternalMissileSmoke() { return bExternalMissileSmoke; }

	/** Desenha cada gravação invisível na frente da câmera por uns quadros: o shader compila antes da 1ª morte. */
	void Prewarm(const FVector& At);
	bool IsWarm() const { return bWarm; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	bool LoadAssets();
	/** bLit false: nasce sem a luz e sem o clarão (os volumes da ogiva v2: a luz dela é a dos destroços). As areias aprovadas do
	 *  tiro (SpawnDust) seguem nascendo como antes (true: o ator apaga a luz e o clarão logo depois, no SpawnDust). */
	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed, bool bLit = true);
	void UpdateView();

	// a areia do tiro: a poeira gravada (jg_dust_impact) no raymarch, com orçamento (as mortes de tanque vêm primeiro:
	// o raymarch desenha até 16 por quadro). Prioridade 0 = o sopro do disparo, 1 = o acerto e o tiro no chão
	struct FDust
	{
		TWeakObjectPtr<AExplosionVolume> E;
		float Duration = 3.0f, Opacity0 = 1.0f;
		int32 Prio = 0;
		// (17/10 tarde) a vida em segundos de verdade (0 = a da gravação): some pela opacidade de FadeFromS a LifeS
		float LifeS = 0.0f, FadeFromS = 0.0f, FadeInS = 0.25f;   // (23/10 noite) FadeInS: a entrada (s da gravação; a strike_chao nasce no impacto)
	};
	TArray<FDust> Dusts;
	AExplosionVolume* SpawnDust(const FVector& GroundCm, float Scale, float TimeScale, float Opacity, int32 Prio, uint32 Variant,
		const FVector4f& Color = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f), const TCHAR* Source = nullptr, float Thickness = 1.0f,
		const FVector4f& Soot = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f));
	// o último acerto de cada tanque (no máximo um efeito por vítima a cada 100 ms, como o ZH)
	TMap<TWeakObjectPtr<AActor>, double> LastHitAt;
	uint32 ShotSerial = 0;                          // misturado na variante: cada tiro com o seu desenho
	// o ponto do acerto escolhido no disparo (o traçante voa até ele) e os acertos esperando o traçante chegar
	struct FShotPoint
	{
		FVector Point = FVector::ZeroVector, Normal = FVector::ZeroVector, ShellDir = FVector::ZeroVector;
		double Arrive = 0.0, Born = 0.0;
		uint32 Variant = 0;
		float GroundZCm = 0.0f;
		bool bSpawned = false;                      // as faíscas já nasceram na chegada do traçante
		TWeakObjectPtr<UStaticMeshComponent> Part;  // a peça que levou o tiro e o ponto no espaço da malha (a marca)
		FVector LocalCm = FVector::ZeroVector;
	};
	TMap<TWeakObjectPtr<AActor>, FShotPoint> PendingShots;
	struct FDelayedHit
	{
		FShotPoint Shot;
		TWeakObjectPtr<AActor> Target;
		float GroundZCm = 0.0f;
		bool bKill = false;
	};
	TArray<FDelayedHit> DelayedHits;
	// a Patriot: os mísseis em voo (o ator da chama e do rastro, e quando foi movido pela última vez), a última poeira de
	// lançamento e a última bola de ogiva por alvo
	struct FMissileFx
	{
		TWeakObjectPtr<class ABlastDebris> Fx;
		double Seen = 0.0;
	};
	TMap<int32, FMissileFx> Missiles;
	double LastLaunchDust = -10.0;
	TMap<TWeakObjectPtr<AActor>, double> LastWarheadBall;   // (a de antes, -MissileBlastV=1)
	/**
	 * (23/10) A ogiva v2: cada volume vivo de uma ogiva (o corpo: a Carga Pesada a λ; o anel: a sim_chao_anel), a vítima e o pé
	 * (a fusão dos acertos no mesmo alvo), os relógios em s de idade e a aposentadoria (o esmaecer curto quando falta vaga no
	 * raymarch ou uma morte nasce em cima). Fora da lista das areias: o teto, a prioridade e a vida são daqui.
	 */
	struct FWarhead
	{
		TWeakObjectPtr<AExplosionVolume> E;
		TWeakObjectPtr<AActor> Victim;
		FVector FootCm = FVector::ZeroVector;
		bool bRing = false;
		float RadiusCm = 0.0f;                      // a bola (o corpo) ou o R90 do anel
		float BallCm = 0.0f;                        // a bola da ogiva (nas duas: as mortes por cima tiram as duas)
		float FireEndS = 0.0f;                      // o fim da fase A (a bola acesa); o anel: a janela da fusão dos anéis
		float FadeFromS = 0.0f, LifeS = 0.0f;       // o corpo: o esmaecer natural (s de idade)
		float ThinFromS = 0.0f, SootKeep = 1.0f;    // o corpo: a fuligem afina de ThinFromS até FireEndS pra SootKeep (a leitura do alvo)
		float Duration = 0.0f, Opacity0 = 1.0f;     // o anel: até onde ele toca (s da gravação: o fim da parte que se vê) e a opacidade
		float RetireAt = -1.0f;                     // a idade em que começou a sair (< 0: não está saindo)
		// (23/10, a vida; BlastFxHitLife.h) o corpo com vida: a bola anda ao longo de ℓ (FootBase + MoveCm·(1 − e^(−t/MoveTauS))), a nuvem
		// segura o último quadro válido a partir de HoldAgeS e sobe e alarga como térmica (o tamanho × √(t/HoldAgeS), a espessura ÷ o cubo)
		// até CloudEndS, indo com o vento depois da bola acesa (DriftCm)
		bool bMove = false, bCloud = false;
		FVector FootBase = FVector::ZeroVector, MoveCm = FVector::ZeroVector, DriftCm = FVector::ZeroVector;
		float MoveTauS = 0.1f, HoldAgeS = 0.0f, Scale0 = 1.0f, Thick0 = 1.0f, LastAge = 0.0f;
		// (a revisão da vida) o λ da carga (a escala de Hopkinson-Cranz na Carga Pesada): a altura do meio da nuvem pro vento (12·λ m) é a da
		// carga, não a do tamanho em que a gravação toca (a dedicada toca a (W/W0)^⅓, outra régua)
		float CloudLambda = 1.0f;
	};
	TArray<FWarhead> Warheads;
	/** (23/10, a vida) O que o MissileHit com vida passa pro SpawnWarhead (nulo = a ogiva v2 de hoje, exata). */
	struct FWarheadLife
	{
		bool bCloud = false;                        // a nuvem viva (a camada Cloud; o preset diz quanto)
		float CloudExtraS = 0.0f;                   // quanto ela dura depois da gravação (s; 3·G no Alta/Ultra, 1,5·G na Média)
		FVector MoveCm = FVector::ZeroVector;       // a bola anda isto ao longo de ℓ (no chão)
		float MoveTauS = 0.1f;                      // τ = 0,25 × o fim da bola acesa
		bool bYaw = false;                          // o giro pelo rumo de ℓ (senão o sorteio de hoje)
		float YawRad = 0.0f;
		bool bRingFoot = false;                     // o anel na projeção do ponto, ~0,3 R pra fora
		FVector RingFootCm = FVector::ZeroVector;
		FName Source = NAME_None;                   // a gravação dedicada (warhead_lado, _teto, _chao), quando existe (a strike_chao, a areia do ataque, toca à parte)
		float SourceScale = 1.0f;                   // o tamanho dela pra esta carga ((W/W da gravação)^⅓)
		bool bGasSkirt = false;                     // o pé no teto do casco (sem a warhead_teto): a saia baixa é o gás da carga, na cor da fuligem
		bool bNoRing = false;                       // (a revisão) sem o anel de areia: o jogo desenha o véu, ou a strike_chao tem a areia (uma representação só)
		float OutCloudEndS = 0.0f;                  // (de volta) o fim da nuvem viva do corpo que nasceu (s de idade; 0 = sem ela)
		bool bOutOwn = false;                       // (de volta, a revisão) nasceu a gravação dedicada (ela tem a areia que o choque levanta)
		float OutOwnFadeS = 0.0f;                   // (de volta) o esmaecer dela (s de idade): a continuação do jogo nasce aí
	};
	/** (23/10, a vida) O MissileHit com vida (o pulso; BlastFxHitLife.h). */
	FVector MissileHitLife(uint32 Variant, const FVector& PointCm, const FVector& FromCm, AActor* Victim, float GroundZCm, bool bKill, float WarheadKg,
		bool bClassicLook, const BlastFxHitLife::FHitIn& In, BlastFxHitLife::FHitOut* Out);
	/** (23/10, a vida) O 1º toque do raio From → To nas caixas das peças do alvo (a mesma conta do ArmorPoint, com o raio de verdade em 3D). */
	bool HullRay(AActor* Target, const FVector& FromCm, const FVector& ToCm, FShotPoint& Out) const;
	/** (a revisão da vida) A caixa do alvo no espaço dele, sem escala: a soma das caixas das peças que o HullRay enxerga. */
	bool HullBox(AActor* Target, FBox& Out) const;
	/** (23/10, a vida) A fila de pousos (FLanding), num tipo daqui (o cabeçalho de fora só é declarado). */
	struct FLandingRec
	{
		FVector PosCm = FVector::ZeroVector;
		float SizeM = 0.3f;
		uint8 Kind = 0;
		bool bHot = false;
		FVector VelMs = FVector::ZeroVector;        // (a areia do acerto) a velocidade no 1º toque (m/s)
	};
	TArray<FLandingRec> Landings;
	/** (23/10, a vida) Até quando cada pedaço em brasa vive (o teto do preset: 32 Ultra, 16 Alta, 8 Média, 4 Baixa). */
	TArray<double> EmberUntil;
	/** (a revisão da vida) Os pedaços que soltam o fio de fumaça em voo: até quando cada um queima e o fio dele sai (o teto do preset: 8 Ultra,
	 *  4 Alta, 2 Média, nenhum na Baixa; 1 por acerto, 2 a partir de 12 kg) e os pontos deste quadro (o Id e onde está), que o jogo esvazia. */
	TArray<double> EmberSmokeUntil;
	struct FEmberSmokeRec
	{
		uint32 Id = 0;
		FVector PosCm = FVector::ZeroVector;
	};
	TArray<FEmberSmokeRec> EmberSmoke;
	TArray<TPair<FVector2D, double>> RecentBigBlasts;   // as mortes e as clássicas do último 1 s (o anel não nasce em cima delas)
	void UpdateWarheads();
	/** Devolve se a bola de fogo deste acerto é de uma ogiva: o corpo que nasceu agora, ou o aceso de antes em que ele se fundiu (a luz
	 *  dos destroços segue a curva dela). false: a bola é da morte ou da clássica recente em cima (elas acendem a própria luz), ou não
	 *  havia vaga no raymarch (os destroços ficam com a queda do clarão). Os destroços da ogiva v2 nunca têm o brilho em sprite. */
	bool SpawnWarhead(const FVector& FootCm, AActor* Victim, float Kg, uint32 Variant, bool bClassicLook, FWarheadLife* LifeOpt = nullptr);
	/** Quantos volumes o raymarch desenha agora na tela (o mesmo filtro e o mesmo cone dele; mais os anéis de ogiva que nasceram com
	 *  opacidade 0 e entram no próximo quadro). */
	int32 RaymarchOnScreen() const;
	/** Tira Count ogivas: já (bNow; só as que contam no RaymarchOnScreen, que liberam vaga) ou esmaecendo. Primeiro o anel que nasceu
	 *  com opacidade 0 e ainda não foi desenhado (sai já nos dois modos, sem estalo), depois os anéis, depois os corpos na fase B (a
	 *  fumaça), a mais velha primeiro; bPhaseBOnly deixa a bola acesa em paz. Devolve quantas saíram. */
	int32 RetireWarheads(int32 Count, bool bNow, bool bPhaseBOnly = false);
	void RetireWarheadsNear(const FVector& AtCm, float RadiusCm);
	/** Antes de nascer um volume que vem primeiro (a morte, a areia do tiro) com o pé em FootCm na escala Scale: as ogivas cedem. */
	void MakeRoomForPriority(const FVector& FootCm, float Scale);
	TArray<double> RecentDeaths;                    // quando nasceram as últimas mortes (o orçamento da morte em massa)
	// os carros sendo esmagados: o eixo e o tamanho do carro, a direção do tanque, quando começou, a passagem e o que já
	// aconteceu (um bit por etapa)
	struct FCrush
	{
		TWeakObjectPtr<AActor> Car;
		FVector Center = FVector::ZeroVector, Fwd = FVector::ForwardVector, Right = FVector::RightVector, TankDir = FVector::ForwardVector;
		float HalfLen = 220.0f, HalfWid = 90.0f, Height = 150.0f, GroundZ = 0.0f;
		double T0 = 0.0;
		float Pass = 2.0f;
		uint32 Variant = 0;
		int32 Done = 0;
		bool bEnded = false, bBurnt = false;        // bBurnt: a carcaça queimada (sem vidro, lanterna nem pneu)
	};
	TArray<FCrush> Crushes;
	void UpdateCrushes();
	// o teste do carro sem o jogo (-CrushFxTest): o proxy que o jogo cria ao esmagar (a malha civ_pickup/civ_car_burnt) e
	// os tanques (a velocidade, pra passagem)
	TSet<TWeakObjectPtr<AActor>> CrushTestSeen;
	TMap<TWeakObjectPtr<AActor>, FVector> CrushTestTankPos;
	TMap<TWeakObjectPtr<AActor>, float> CrushTestTankSpeed;
	void UpdateCrushTest();
	void UpdateArcTest();
	void GatherSceneLights();                       // as luzes pontuais acesas → RTSSceneLights (as fumaças leem)
	void UpdateShadowTest();                        // -ShadowFxTest=1: tiros na areia dos 4 lados da casa 22 (a prova da sombra do mapa)
	int32 ShadowTestStep = 0;
	/**
	 * A marca do tiro no casco (16/10; o estudo: docs/ESTUDO_MARCA_QUENTE.md): grava o acerto nos dados de primitiva
	 * 12–35 da peça (6 vagas: x, y, z no espaço da malha em cm; a hora + 100 000 × o tipo, 0 canhão, 1 míssil). Um acerto
	 * a menos de 25 cm de uma marca do mesmo tipo reforça ela (volta a esquentar); cheia, sai a mais velha. Quem desenha é
	 * a MF_HullMarks (Game/Tools/hull_marks.py) no material das unidades. -HullMarks=0 desliga.
	 */
	void MarkHull(UStaticMeshComponent* Part, const FVector& LocalCm, int32 Kind);                           // -ArcFxTestX/Y: a sequência do poste num ponto, sem o jogo
	int32 ArcTestStep = 0;
	static inline bool bExternalMissileSmoke = false;
	bool ArmorPoint(AActor* Target, const FVector& FromCm, const FVector& AtCm, float GroundZCm, uint32 Variant, FShotPoint& Out) const;
	void SpawnArmorImpact(const FShotPoint& Shot, AActor* Target, float GroundZCm, bool bKill);
	// os tanques feridos: a fumaça (um ator de destroços com 1–2 rastros que sobem) e o estágio (1 danificado, 2 muito)
	struct FDamaged
	{
		TWeakObjectPtr<class ABlastDebris> Fx;
		int32 Light = -1, Heavy = -1, Stage = 0;
		int32 Dbg[3] = { -1, -1, -1 };
		// a carcaça: a fumaça escura e o fogo pequeno começam quando a explosão da morte já se desfez e duram ~30 s
		double WreckStart = -1.0, WreckUntil = -1.0;
		double WreckLightStart = -1.0;   // (17/10) a luz do fogo de baixo acende logo depois da bola, antes da fumaça e das chamas
		bool bWreckLightOn = false;
		// (17/10) a coluna em volume da carcaça (sim_carcaca) e se já acendeu
		TWeakObjectPtr<class AExplosionVolume> Column;
		bool bWreckLit = false;
	};
	void StartWreck(AActor* Unit);
	TArray<FVector2D> GroundCraters;   // (17/10) onde já caiu tiro na areia (cm): ali a terra escura sobe
	float WindYawRad = -0.785f;   // o padrão: o vento da fumaça dos destroços (BlastDebris SmokeWind, (0,55; −0,55))
	float WindSpeedMs = 4.0f;
	TMap<TWeakObjectPtr<AActor>, FDamaged> Damaged;
	void UpdateDamaged();
	void EndDamaged(AActor* Unit);
	void UpdateDusts();

	UPROPERTY() TArray<TObjectPtr<UExplosionBake>> Bakes;
	UPROPERTY() TObjectPtr<UMaterialInterface> VolumeMaterial;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Alive;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Warmups;

	bool bPaused = false;
	bool bWarm = false;
	float WarmClock = 0.0f;

	bool bHasView = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	FVector ViewRight = FVector::RightVector;
	FVector ViewUp = FVector::UpVector;
	float TanHalfX = 1.0f;
	float TanHalfY = 1.0f;
};
