#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FXWreckFire.h"
#include "FXSceneLight.h"
#include "FXTread.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class AStaticMeshActor;

/** O tipo de chão: dá a cor, a claridade e a quantidade da poeira. */
enum class EFXGround : uint8 { Sand, Snow, Mud, Asphalt };

/** O ajuste por chão (fxtread.js: TREAD_DEFAULTS.sand / snow / mud / asphalt). */
struct FFXTreadGround
{
	double R = 1.0, G = 0.78, B = 0.52, Albedo = 4.0, Density = 0.9, Life = 1.0, Amount = 1.0, Alpha = 1.0, Size = 1.0;
};

/** O ajuste da poeira do andar: FXLab/tread.json (o mesmo do laboratório, ?fx=tread). */
struct FFXTreadPreset
{
	FString Bake = TEXT("dust_impact4");
	double Speed = 7.0, PathR = 26.0, Spacing = 0.4, FullSpeed = 6.0, Life = 2.6, Frame0 = 16.0, Frame1 = 50.0;
	double Size0 = 0.24, Size1 = 0.75, Kick = 1.5, Side = 1.8, Up = 0.35, Drag = 1.6, Inherit = 0.08;
	double WindX = 0.6, WindY = 0.3, Sink = 0.08, Alpha = 0.55, Erode = 0.35, RearX = -2.7, HalfW = 1.55, Along = 2.6, RotMax = 0.6, Orient = 0.0, SquashUp = 1.0;
	double TankYaw = 0.0;      // graus: o giro do modelo do tanque na Unreal em relação à frente (+x)
	FFXTreadGround Grounds[4];

	FFXTreadPreset();
	bool Load(const FString& Path);
};

/** Um veículo que levanta poeira: o jogo manda um por unidade em movimento, a cada quadro. */
struct FFXTreadSource
{
	int32 Id = 0;                          // o mesmo id de quadro em quadro (guarda os metros andados)
	FVector PosM = FVector::ZeroVector;    // m: o centro do veículo no chão
	FVector FwdM = FVector(1, 0, 0);       // a frente, no chão
	double Speed = 0.0;                    // m/s
	EFXGround Ground = EFXGround::Sand;
};

/**
 * A poeira do andar: o rastro atrás das lagartas. Repete o laboratório (FXLab/web/fxtread.js) linha a linha,
 * com a mesma semente: cartões 6-way da Dust Impact (JangaFX, CC0) que nascem por metro andado no pé da
 * metade de trás de cada lagarta, são jogados pra trás e pros lados, crescem, se espalham com o vento e se
 * desfazem tocando a própria simulação.
 *
 * Pro jogo: um ator só pra todas as unidades. A cada quadro, SetSources() com os veículos que andam (a ponte
 * sabe a posição, a frente, a velocidade e o chão); os cartões de todos saem numa malha por tipo de chão.
 * No laboratório (bLabDrive): o tanque do mapa dá voltas e a câmera segue, como no navegador.
 */
UCLASS()
class FXLAB_API AFXTreadDust : public AActor
{
	GENERATED_BODY()

public:
	AFXTreadDust();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<UMaterialInterface> CardMaterial;

	/** O laboratório: o tanque do mapa dá voltas (fxtread.js) e a câmera segue. */
	UPROPERTY(EditAnywhere, Category = "FX")
	bool bLabDrive = false;

	UPROPERTY(EditAnywhere, Category = "FX")
	int32 Seed = 5;

	/** Os veículos em movimento deste quadro (o jogo chama antes do Tick). */
	void SetSources(const TArray<FFXTreadSource>& InSources) { Sources = InSources; }

	bool bPaused = false;
	double TimeScale = 1.0;

private:
	struct FCard
	{
		FVector P, V;
		double Age = 0.0, Life = 1.0, S = 1.0, Rot = 0.0, Flip = 1.0, F0 = 0.0, A = 1.0, GA = 1.0, GS = 1.0, GroundZ = 0.0;
		FVector Back = FVector(-1, 0, 0);     // pra trás do tanque quando nasceu (o rastro da gravação aponta pra lá)
		EFXGround Ground = EFXGround::Sand;
	};
	struct FLayer
	{
		UProceduralMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
	};

	double Rand();
	double Rnd(double A, double B);
	void Step(double Dt);
	void Emit(const FFXTreadSource& Src, double Dt);
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);
	void LoadPreset();
	void ApplyPreset();
	void LabPose(double Dt);
	/** A altura do chão (m) embaixo de (x, y) em metros: um raio de cima pra baixo, sem o tanque. */
	double GroundAt(double X, double Y, double Fallback) const;
	/** O peso da camada "Asphalt" do terreno em (x, y) metros (0..1): a areia não levanta poeira no asfalto. */
	double AsphaltAt(double X, double Y) const;
	/** O tanque do jogo de hoje (Game/Config/RTS/Units.json: o tanque da maquete do Look) montado na origem do mapa. */
	void SpawnGameTank();
	/** Lê a camada "Asphalt" do terreno inteiro uma vez (as texturas de peso do Landscape, dado do editor). */
	void BuildAsphaltGrid();
	/** Acha o centro da volta: uma volta inteira no chão, cruzando um pedaço de asfalto (pra ver a poeira parar nele). */
	FVector FindPathCenter() const;

	FFXTreadPreset P;
	FFXPreset Scene;
	FFXSceneLight Light;
	FFXBakeMeta Meta;
	FLayer Layers[4];
	TArray<FCard> Cards;
	TArray<FFXTreadSource> Sources;
	TMap<int32, TPair<double, double>> Acc;    // metros andados desde o último cartão: esquerda, direita
	uint32 RngState = 5;

	// o laboratório: o tanque que dá voltas
	double Ang = 0.0, T = 0.0;
	FVector TankPos = FVector::ZeroVector, CamFollowed = FVector::ZeroVector;
	FVector TankFwd = FVector(1, 0, 0);
	EFXGround LabGround = EFXGround::Sand;
	TArray<TPair<TObjectPtr<AStaticMeshActor>, FTransform>> TankParts;   // a peça e onde ela fica em relação ao tanque
	FVector PathCenter = FVector::ZeroVector;      // m: o centro da volta (o tanque do grupo 1 no mapa do jogo)
	bool bCamAligned = false;
	// o peso do asfalto em cada vértice do terreno (0..255), em coordenadas de quadrado do Landscape
	TArray<uint8> AsphaltGrid;
	FIntPoint AsphaltMin = FIntPoint::ZeroValue, AsphaltSize = FIntPoint::ZeroValue;
	FTransform LandscapeXf;
	int32 Warm = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	FString ShotName;
	TArray<double> ShotAt;
	int32 ExitIn = -1;
	bool bCapture = false;
	bool bFixedStep = false;
};
