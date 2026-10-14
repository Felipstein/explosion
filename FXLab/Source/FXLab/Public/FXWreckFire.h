#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FXSceneLight.h"
#include "FXWreckFire.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;

/** O ajuste: FXLab/preset.json, o mesmo arquivo do laboratório no navegador (FXLab/web). */
struct FFXPreset
{
	FString Bake = TEXT("gas_puff2");
	double Interval = 0.69, Scale = 0.69, Speed = 0.85, Rise = 1.1, SmokeLife = 2.0, SmokeGrow = 0.26;
	double Density = 2.6, Erode = 0.25, EmberCut = 0.9, Albedo = 1.11, Shadow = 0.47, LickInterval = 0.22;
	double BaseR = 1.35, FireLight = 153.0, Emit = 13.7, Sun = 3.4, Sky = 1.85, SunElev = 10.0, SunAz = 35.0;
	double Wind = 1.0, Exposure = 1.0;
	double UeBloom = 0.15;      // brilho da Unreal, afinado pra o halo bater com o do laboratório

	bool Load(const FString& Path);
	/** Pro sol, eixos da Unreal (az 0 = na frente da câmera de RTS, contraluz). */
	FVector SunDir() const;
};

/** O que o cozimento mede: atlas.json (bake_sixway.py + atlas_meta.py). */
struct FFXBakeMeta
{
	int32 Cols = 8, Rows = 6, Frames = 48, FireEnd = 36;
	double SideM = 18.0, LastTime = 1.0;
	FVector2D Pivot = FVector2D(0.5, 0.5);
	TArray<double> BallBottom;
	TArray<int32> SimFrames;
	TArray<double> Times;      // s: o tempo da simulação de cada quadro (times_s)
	TArray<FVector4> BBox;     // a caixa do alfa de cada quadro (u0, v0, u1, v1; v pra cima), se o atlas_bbox.py rodou
	TArray<TArray<FVector2D>> Cut8;   // o octógono do alfa dos quadros f e f+1 (atlas_cut8.py), u e v de baixo

	bool Load(const FString& Path);
};

/** Um cartão: bolota (fogo que vira fumaça) ou lambida (a chama curta no anel da torre). */
struct FFXCard
{
	FVector P = FVector::ZeroVector;     // m, eixos da Unreal
	double Age = 0.0, Rot = 0.0, RotV = 0.0, Flip = 1.0, Size = 1.0, Rad = 0.0, T = 1.0;
	bool bLick = false;
};

/**
 * O fogo da carcaça do jeito dos jogos: cartões virados pra câmera que tocam uma
 * simulação gravada com luz de 6 direções. Fogo e fumaça são a mesma gravação.
 * A lógica repete a do laboratório (FXLab/web/fxfire.js) linha a linha, com a mesma
 * semente: as duas dão a mesma história quadro a quadro. O ator fica no anel da torre.
 */
UCLASS()
class FXLAB_API AFXWreckFire : public AActor
{
	GENERATED_BODY()

public:
	AFXWreckFire();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<UMaterialInterface> CardMaterial;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UProceduralMeshComponent> Cards;

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UPointLightComponent> FireLight;

	/** A semente (o laboratório usa 7); no mapa cada carcaça tem a sua, pra não saírem clonadas. */
	UPROPERTY(EditAnywhere, Category = "FX")
	int32 Seed = 7;

	bool bPaused = false;
	double TimeScale = 1.0;
	double SimTime = 0.0;
	FFXPreset P;

	/** Recomeça a história (mesma semente). */
	void Restart();

private:
	// lógica (fxfire.js)
	double Rand();
	double Rnd(double A, double B);
	double FireDur() const;
	void SpawnPuff();
	void SpawnLick();
	void Step(double Dt, const FVector& CamM);
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);

	void LoadPreset(bool bForce);
	void LoadBake();
	void ApplyPreset();
	void Capture();
	/** Pro sol: o do mapa do jogo ou o do ajuste. */
	FVector SunDirection() const { return Light.bGame ? Light.SunDir : P.SunDir(); }

	FFXBakeMeta Meta;
	FFXSceneLight Light;
	TArray<FFXCard> CardList;
	uint32 RngState = 7;
	double SpawnClock = 0.0, LickClock = 0.0, Fps = 1.0;
	int32 LickEnd = 14;
	FVector Hatch = FVector::ZeroVector;     // m
	FString LoadedBake;
	FDateTime PresetStamp;
	double PresetCheck = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Mid;

	// captura: -Shot=nome -At=1,2,3 (com -benchmark -fps=30: passo fixo, a mesma história do laboratório)
	FString ShotName;
	TArray<double> ShotAt;
	int32 ExitIn = -1;
};
