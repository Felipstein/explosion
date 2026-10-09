#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FXWreckFire.h"
#include "FXImpact.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;

/** O ajuste do tiro no chão: FXLab/impact.json (o mesmo do laboratório, ?fx=impact). */
struct FFXImpactPreset
{
	FString DustBake = TEXT("dust_impact4"), RingBake = TEXT("shockwave_top"), GeyserBake = TEXT("debris4");
	double DirAz = 30.0, FlashSize = 2.6, FlashTime = 0.1, FlashGain = 30.0, FlashLight = 600.0;
	double SparkCount = 40.0, SparkSpeed = 26.0, SparkLife = 0.45;
	double ChunkCount = 50.0, ChunkSpeed = 12.0, ChunkSize = 0.09;
	double DustScale = 1.2, DustSpeed = 1.0, DustLife = 4.0;
	double RingStretch = 1.3, RingShift = 1.2;
	double RingScale = 0.5, RingSpeed = 0.8, RingLife = 4.5, RingAlbedo = 5.0, GeyserScale = 1.0, GeyserSpeed = 1.0, GeyserFade = 0.35;
	double CraterRadius = 1.9, SandR = 0.85, SandG = 0.8, SandB = 0.72, DustDensity = 1.3, DustAlbedo = 4.0;
	double GeyserAlbedo = 0.35, SoftDist = 1.0, OriginX = 9.0, OriginY = 2.0, Gravity = 9.8;

	bool Load(const FString& Path);
};

/**
 * O tiro de tanque que acerta o chão (arte 4.1). Repete o laboratório (FXLab/web/fximpact.js)
 * linha a linha, com a mesma semente: nuvem de poeira (Dust Impact), anel (Dust Shockwave) e jato
 * de areia (grãos simulados) em cartões 6-way; clarão, faíscas, pedrinhas e cratera desenhados.
 */
UCLASS()
class FXLAB_API AFXImpact : public AActor
{
	GENERATED_BODY()

public:
	AFXImpact();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<UMaterialInterface> CardMaterial;

	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<UMaterialInterface> PrimMaterial;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UProceduralMeshComponent> RingCards;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UProceduralMeshComponent> DustCards;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UProceduralMeshComponent> GeyserCards;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UProceduralMeshComponent> Prims;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UPointLightComponent> FlashLight;

	/** No mapa do jogo o tiro fica onde o setup_world.py pôs (no chão); no laboratório vem do impact.json. */
	UPROPERTY(EditAnywhere, Category = "FX")
	bool bPlacedInWorld = false;

	/** A semente (o laboratório usa 11) e quando sai o 1º tiro (s): no mapa, tiros desencontrados. */
	UPROPERTY(EditAnywhere, Category = "FX")
	int32 Seed = 11;

	UPROPERTY(EditAnywhere, Category = "FX")
	double FirstShot = 0.0;

	/** A cor da poeira = a do chão do mapa (a poeira é o chão levantado). A = 0: a do impact.json (o laboratório). */
	UPROPERTY(EditAnywhere, Category = "FX")
	FLinearColor GroundTint = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/** Dispara o tiro (sorteia tudo de novo, continuando a semente). */
	void Fire();

	bool bPaused = false;
	double TimeScale = 1.0;
	bool bAutoRepeat = true;

private:
	struct FPart { bool bSpark = true; FVector P, V; double Life = 1.0, W = 0.1, Landed = -1.0; };
	struct FLayer
	{
		UProceduralMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
		FFXBakeMeta Meta;
		double Fps = 1.0;
		FString Bake;
	};

	double Rand();
	double Rnd(double A, double B);
	FVector Cone(const FVector& Fwd, double TiltDeg, double SpreadDeg);
	void Step(double Dt);
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);
	void LoadPreset(bool bForce);
	void LoadLayer(FLayer& L, const FString& Bake);
	void ApplyPreset();
	FVector SunDirection() const { return Light.bGame ? Light.SunDir : Scene.SunDir(); }

	FFXImpactPreset P;
	FFXPreset Scene;               // o sol e o céu vêm do preset.json (a mesma cena do fogo)
	FFXSceneLight Light;           // ou do mapa do jogo
	FLayer Ring, Dust, Geyser;
	TArray<FPart> Parts;
	FVector Origin = FVector::ZeroVector;     // m
	uint32 RngState = 11;
	double T = -1.0, Refire = 0.0, PresetCheck = 0.0;
	double DustRot = 0.0, DustFlip = 1.0, RingRot = 0.0;
	FDateTime PresetStamp;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PrimMid;

	FString ShotName;
	TArray<double> ShotAt;
	int32 ExitIn = -1;
	bool bCapture = false;         // o laboratório fotografa (-Fx=impact -Shot)
	bool bFixedStep = false;       // passo de 1/30 s (qualquer -Shot)
};
