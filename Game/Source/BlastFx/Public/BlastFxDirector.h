#pragma once

// O diretor das explosões no jogo: o que o UBlastLabSimulation faz pras
// explosões no simulador, sem o resto do laboratório (painel, armas de teste,
// passagens de avião). Dá às explosões o relógio, a vista da câmera (pra saber
// o quanto cada uma ocupa da tela) e o orçamento de detalhe; dispara a morte de
// tanque; aquece os shaders na abertura.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlastFxDirector.generated.h"

class AExplosionVolume;
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

	/**
	 * A Patriot (16/10; o estudo: docs/ESTUDO_MISSIL_PATRIOT.md). O jogo chama:
	 *  - MissileLaunch no disparo (Id = o do tiro; Pos e Dir na boca da caixa);
	 *  - MissileMove a cada quadro com o míssil interpolado (a chama, a luz e o rastro andam junto);
	 *  - MissileHit no contato (a explosão da ogiva; bKill = o tiro que matou);
	 *  - DefenseAssist quando uma bateria ajuda outra (o laser de mira vermelho entre elas, o LaserFromAssisted do ZH).
	 */
	void MissileLaunch(int32 Id, const FVector& PosCm, const FVector& Dir, uint32 Variant);
	void MissileMove(int32 Id, const FVector& PosCm, const FVector& Dir, float SpeedCmPerS);
	void MissileHit(int32 Id, const FVector& PointCm, AActor* Victim, float GroundZCm, bool bKill);
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
	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed);
	void UpdateView();

	// a areia do tiro: a poeira gravada (jg_dust_impact) no raymarch, com orçamento (as mortes de tanque vêm primeiro:
	// o raymarch desenha até 16 por quadro). Prioridade 0 = o sopro do disparo, 1 = o acerto e o tiro no chão
	struct FDust
	{
		TWeakObjectPtr<AExplosionVolume> E;
		float Duration = 3.0f, Opacity0 = 1.0f;
		int32 Prio = 0;
		// (17/10 tarde) a vida em segundos de verdade (0 = a da gravação): some pela opacidade de FadeFromS a LifeS
		float LifeS = 0.0f, FadeFromS = 0.0f;
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
	TMap<TWeakObjectPtr<AActor>, double> LastWarheadBall;
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
