#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "BlastLabSimulation.generated.h"

class ABlastLabMover;
class AExplosionVolume;
class UExplosionBake;
enum class EBlastMover : uint8;
class UMaterialInterface;

/** Uma arma do arsenal (reference/webgl/js/instances.js MAGNITUDES). */
struct FBlastWeapon
{
	const TCHAR* Id;
	const TCHAR* Name;      // como aparece no painel
	float Yield;            // 1 = Carga Pesada (≈ 20 kg de TNT)
	const TCHAR* Meta;
	bool bNuclear;

	/** Tamanho e duração escalam com W^(1/3). */
	float SizeScale() const { return FMath::Pow(Yield, 1.0f / 3.0f); }
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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UExplosionBake>> Bakes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> VolumeMaterial;

	/** O emissivo e o brilho do míssil e do disparo: carregados no aquecimento, compilam junto com o resto. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> MoverMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AExplosionVolume>> Alive;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AExplosionVolume>> Warmups;

	float WarmupClock = 0.0f;

	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale);
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

	// a vista do quadro (UpdateView)
	bool bHasView = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector, ViewRight = FVector::RightVector, ViewUp = FVector::UpVector;
	float TanHalfX = 1.0f, TanHalfY = 0.5f;
	float ViewHeightPx = 900.0f;
	float CachedAmbientLux = 52000.0f;
};
