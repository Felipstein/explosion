#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "BlastLabSimulation.generated.h"

class ABlastDebris;
class ABlastLabMover;
class AExplosionVolume;
class UExplosionBake;
enum class EBlastMover : uint8;
class UMaterialInterface;
class UTexture;

/** Uma arma do arsenal (reference/webgl/js/instances.js MAGNITUDES). */
struct FBlastWeapon
{
	const TCHAR* Id;
	const TCHAR* Name;      // como aparece no painel
	float Yield;            // 1 = Carga Pesada (≈ 20 kg de TNT)
	const TCHAR* Meta;
	bool bNuclear;
	/**
	 * O relógio, se não for o do tamanho (0 = W^(1/3), a regra das explosões
	 * HE). A bola de combustível tem tempo próprio (CCPS: t = 0.078·D); na morte
	 * de tanque a gravação toca no ritmo do vídeo do Veo, sem esticar.
	 */
	float TimeYield = 0.0f;
	/** Morte de tanque: arremessa a torre e o cano da vítima e deixa a carcaça queimando. */
	bool bTankDeath = false;
	/**
	 * A bola gravada, se não for a do rendimento (0 = W^(1/3)). O choque, o
	 * tremor e o empurrão na fumaça seguem o Yield; a bola segue o alvo medido.
	 */
	float BallScale = 0.0f;
	/** A saia de poeira: altura e vida relativas às da Carga Pesada (estouro acima do chão levanta menos; 0 = sem). */
	float SkirtHeight = 1.0f;
	float SkirtLife = 1.0f;
	/**
	 * O corpo, se não for a Carga Pesada gravada no WebGL: DA_<Body> e
	 * M_Explosion_<Body> (build_explosion.py com BLAST_BAKE). Faltando, cai na Carga Pesada.
	 */
	const TCHAR* Body = nullptr;
	/** A poeira levantada, um segundo volume no mesmo ponto (DA_/M_Explosion_<DustBody>, cor do chão). */
	const TCHAR* DustBody = nullptr;

	/** Tamanho e duração escalam com W^(1/3). */
	float SizeScale() const { return FMath::Pow(Yield, 1.0f / 3.0f); }
	float TimeScale() const { return TimeYield > 0.0f ? FMath::Pow(TimeYield, 1.0f / 3.0f) : SizeScale(); }
};

/**
 * O simulador: o relógio (pausa e câmera lenta valem pras explosões; a câmera
 * e a interface andam no tempo real), o arsenal e as explosões vivas.
 */
UCLASS()
class BLASTLAB_API UBlastLabSimulation : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Multiplicador do tempo da simulação (o WebGL: [ e ] dividem/multiplicam por 1.5, entre 0.05 e 4). */
	UPROPERTY(BlueprintReadWrite, Category = "BlastLab")
	float TimeScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "BlastLab")
	bool bPaused = false;

	/** Arma escolhida no painel (índice do arsenal); padrão: Carga Pesada. */
	UPROPERTY(BlueprintReadWrite, Category = "BlastLab")
	int32 SelectedWeapon = 2;

	/** Segundos de simulação que correspondem a DeltaSeconds de tempo real. */
	float SimDelta(float DeltaSeconds) const { return bPaused ? 0.0f : DeltaSeconds * TimeScale; }

	static UBlastLabSimulation* Get(const UObject* WorldContext);

	static const TArray<FBlastWeapon>& Arsenal();

	/** Detona a arma (índice do arsenal) com o pé em Ground. */
	AExplosionVolume* Detonate(const FVector& Ground, int32 Weapon);

	/**
	 * Aquecimento: detona as variantes invisíveis em At (na frente da câmera).
	 * Sem os assets pré-compilados a Unreal compila os shaders do volume na
	 * primeira vez que ele aparece, e a primeira explosão saía sem bola de fogo;
	 * assim isso acontece na abertura, com o painel avisando.
	 */
	void Prewarm(const FVector& At);

	/** Ainda preparando (shaders compilando ou aquecimento rodando). */
	bool IsPreparing() const { return Warmups.Num() > 0 || PendingShaders() > 0; }
	static int32 PendingShaders();

	/** Explosões vivas (as que o simulador criou). */
	int32 NumAlive() const { return Alive.Num(); }
	const TArray<TObjectPtr<AExplosionVolume>>& AliveExplosions() const { return Alive; }

	/**
	 * Importância na tela (o papel do Significance Manager da Unreal, usado no
	 * Fortnite): quanto da meia altura da tela uma esfera ocupa, e se ela está
	 * dentro da vista. Calculado pela câmera do quadro anterior (um quadro de
	 * atraso não se vê). Sem câmera: tudo visível e grande.
	 */
	float ScreenFraction(const FVector& Center, float Radius, bool& bInView) const;

	/** Onde está a câmera (cm) e o ângulo de um pixel do render (já com a resolução de render do preset). */
	FVector ViewPoint() const { return ViewLocation; }
	float RadiansPerPixel() const;

	/** Iluminância do ambiente agora (lux do render: sol ou lua + céu), da tabela do WebGL. */
	float AmbientLux() const { return CachedAmbientLux; }

	/** Trecho que um avião/míssil/projétil varreu neste quadro (a entrega antecipada olha isso). */
	void AddMoverSegment(const FVector& FromCm, const FVector& ToCm, float RadiusCm);

	/** A explosão mais recente (o avião, o míssil e o disparo miram a fumaça dela). */
	AExplosionVolume* LastExplosion() const { return Alive.Num() ? Alive.Last().Get() : nullptr; }

	/**
	 * Avião, míssil ou disparo atravessando a fumaça da última explosão,
	 * cruzando a tela da esquerda pra direita, na altura em que ela está
	 * (main.js spawnMover).
	 */
	ABlastLabMover* SpawnMover(EBlastMover Kind);

	/** Algum desse tipo no ar agora (o botão do painel fica aceso). */
	bool IsMoverActive(EBlastMover Kind) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBlastLabSimulation, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	bool LoadAssets();
	/** As camadas em volta da explosão (clarão, anel, destroços, marca), pela receita da arma e pelo chão. */
	void SpawnEffects(const FVector& Ground, const FBlastWeapon& W);
	float RecipeClock = 0.0f;
	/** As camadas da fase 1 (rejeitadas em 06/10): desligadas até serem refeitas. */
	bool bLayersEnabled = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UExplosionBake>> Bakes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> VolumeMaterial;

	/** Os corpos próprios das armas (FBlastWeapon::Body), carregados uma vez. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UExplosionBake>> BodyBakes;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMaterialInterface>> BodyMaterials;
	bool LoadBody(const TCHAR* Body, UExplosionBake*& OutBake, UMaterialInterface*& OutMaterial);

	/** O emissivo e o brilho do míssil e do disparo: carregados no aquecimento, compilam junto com o resto. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> MoverMaterials;

	/** Os atlas do teste do flipbook, carregados no aquecimento. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture>> FlipbookAtlases;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AExplosionVolume>> Alive;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AExplosionVolume>> Warmups;

	float WarmupClock = 0.0f;

	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, UMaterialInterface* Material = nullptr);
	void UpdateView();
	bool CrossedThisFrame(const AExplosionVolume* E) const;

	struct FMoverSegment
	{
		FVector From, To;
		float RadiusCm = 0.0f;
	};
	TArray<FMoverSegment> MoverSegments;
	TArray<TWeakObjectPtr<ABlastLabMover>> Movers;

	// pé da última detonação (o alvo das passagens, mesmo depois que ela sai de cena)
	FVector LastBlastGround = FVector::ZeroVector;
	bool bHasLastBlast = false;

	/**
	 * O evento único de cada detonação (o FXList do Zero Hour; docs/CONCEITO_
	 * EXPLOSOES.md §4d): o clarão sai em t = 0 (a explosão), e o choque chega ao
	 * ponto de mira em t = d/343 s, quando saem juntos o tremor, o som e a
	 * reação do mundo. Relógio do simulador (a câmera lenta atrasa o estrondo).
	 */
	struct FShock
	{
		FVector Ground = FVector::ZeroVector;
		float SizeScale = 1.0f;
		float DistanceM = 0.0f;
		float Delay = 0.0f;
		int32 Weapon = 0;
	};
	TArray<FShock> Shocks;
	void ShockArrived(const FShock& S);

	/**
	 * Morte de tanque: a torre e o cano da vítima (atores com as tags HeroVictim
	 * + Turret / Barrel, Tools/build_hero_scene.py) voam juntos, como um corpo
	 * só, e a carcaça fica queimando (fogo contínuo no fluido do campo).
	 */
	void TankDeath(const FVector& Ground);

	/**
	 * Teste do flipbook (-BallFlipbook=1): a bola pré-renderizada no Cycles num
	 * cartão (ABlastFlipbook) no lugar do volume da EG, que fica invisível mas
	 * segue com as luzes e o choque. O atlas pela inclinação da câmera: abaixo de
	 * 25° o da câmera herói, acima o da de RTS.
	 */
	void SpawnFlipbook(AExplosionVolume* E, const FVector& Ground);
	struct FThrownPart
	{
		TWeakObjectPtr<AActor> Actor;
		FTransform Local;              // em relação ao pivô do corpo arremessado
	};
	struct FThrown
	{
		TArray<FThrownPart> Parts;
		FVector P = FVector::ZeroVector;     // pivô (cm)
		FVector V = FVector::ZeroVector;     // cm/s
		FQuat Q = FQuat::Identity;
		FVector Spin = FVector::ZeroVector;  // eixo · rad/s
		bool bRest = false;
		float Delay = 0.0f;                  // s até soltar do casco
		TWeakObjectPtr<ABlastDebris> Debris; // o rastro de fumaça e fogo atrás dela
		int32 Trail = -1;
	};
	TArray<FThrown> Thrown;
	struct FWreckFire
	{
		FVector FootCm = FVector::ZeroVector;
		float Age = 0.0f;
		TWeakObjectPtr<AExplosionVolume> Plume;    // a coluna preta em loop
		FVector RoofCm = FVector::ZeroVector;      // o teto do casco, de onde saem as bolas do fogo
		float NextBallS = 0.0f;                    // quando sai a próxima bola (s de Age)
		TArray<TWeakObjectPtr<AExplosionVolume>> Balls;
	};
	TArray<FWreckFire> Wrecks;
	void UpdateTankDeaths(float Dt);
	/** Uma bola pequena do fogo da carcaça (a bola de gasolina da EG em miniatura). */
	void SpawnWreckBall(FWreckFire& W);

	// a vista do quadro (UpdateView)
	bool bHasView = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector, ViewRight = FVector::RightVector, ViewUp = FVector::UpVector;
	float TanHalfX = 1.0f, TanHalfY = 0.5f;
	float ViewHeightPx = 900.0f;
	float CachedAmbientLux = 52000.0f;
};
