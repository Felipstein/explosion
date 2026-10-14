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

/**
 * Uma camada da poeira nova (docs/POEIRA_DAS_LAGARTAS.md, B3): o jato da roda motriz (K1), a nuvem que fica (K2),
 * a cortina ao longo da lagarta (C). Todas tocam o mesmo cozimento do perfil, cada uma com a sua janela de tempo,
 * o seu lugar de nascer e a sua vida. Medidas em metros, no referencial do tanque (x pra frente, y pro lado da
 * lagarta, a partir do meio dela).
 */
struct FFXTreadLayer
{
	FString Name;
	double XMin = -3.84, XMax = -3.48, YOff = 1.425, YJit = 0.16, Z0 = 0.0;   // a caixa de nascer
	double Spacing = 0.25, SpacingJit = 1.0;      // m andados por cartão × U(1, SpacingJit)
	double Life = 0.35, LifeJit = 0.0;            // s, ± fração
	double MaskDense = 0.0, MaskEnd = 0.0;        // em cascos andados desde o nascer (0 = sem máscara)
	double BakeStart = 0.0, BakeStartJit = 0.0, BakeRate = 1.0;   // s de simulação: começo, sorteio, taxa
	double Scale = 1.0, ScaleJit = 0.15, GrowTo = 1.0;           // × side_m; cresce até GrowTo no fim da vida
	double Alpha = 0.8, Attack = 0.05, Decay = 0.5;              // frações da vida (a curva do WARNO)
	double Inherit = 0.3, Drag = 1.6, Kick = 0.0, Side = 0.0, Up = 0.0, Sink = 0.0;
	double WakeRise = 0.0, WakePull = 0.0, WakeGrow = 0.0, WakeLen = 1.0;   // a esteira do casco, × velocidade
	double Orient = 0.0, Flip = 0.0, RotMax = 0.17, PivotBottom = 1.0, CamOffset = 0.0, Erode = 0.2;
	double V0 = 1.88, V1 = 2.5;                   // m/s: rampa da quantidade pela velocidade da lagarta
	double FollowLoad = 0.0, LoadGain = 1.0;      // 1: segue a carga de areia presa (a areia pinga no asfalto)
	double Shadow = 0.0;                          // > 0: deita no chão a sombra do cartão (camada S), com essa força
	double BaseShade = 1.0;                       // o sol no pé do cartão (junto do chão) × isso, subindo até 1 no topo
	double ScaleSlow = 1.0, VFull = 7.9;          // o tamanho × ScaleSlow na velocidade V1, até × 1 em VFull (a esteira ∝ v)
};

/** O ajuste da poeira do andar: FXLab/tread.json (o mesmo do laboratório, ?fx=tread). */
struct FFXTreadPreset
{
	FString Bake = TEXT("dust_impact4");
	double Speed = 7.0, PathR = 26.0, Spacing = 0.4, FullSpeed = 6.0, Life = 2.6, Frame0 = 16.0, Frame1 = 50.0;
	double Size0 = 0.24, Size1 = 0.75, Kick = 1.5, Side = 1.8, Up = 0.35, Drag = 1.6, Inherit = 0.08;
	double WindX = 0.6, WindY = 0.3, Sink = 0.08, Alpha = 0.55, Erode = 0.35, RearX = -2.7, HalfW = 1.55, Along = 2.6, RotMax = 0.6, Orient = 0.0, SquashUp = 1.0;
	double TankYaw = 0.0;      // graus: o giro do modelo do tanque na Unreal em relação à frente (+x)
	double SoftDist = 100.0;   // cm: a partícula macia contra o chão
	double LoadTau = 0.27;     // s: a areia presa esvazia (abaixo de 5 % em 0,8 s)
	double HullLen = 7.93;     // m: o casco (a máscara de comprimento conta em cascos)
	FFXTreadGround Grounds[4];
	TArray<FFXTreadLayer> Layers;    // vazio: a poeira de 13/10 (uma camada só, os campos de cima)

	FFXTreadPreset();
	bool Load(const FString& Path);
};

/**
 * Um veículo que levanta poeira (o contrato da ponte com o jogo, docs/POEIRA_DAS_LAGARTAS.md):
 * - a ponte manda TODOS os veículos perto da câmera a cada quadro, os parados também (Speed 0): o parado não solta poeira,
 *   mas continua fazendo sombra nela (sem ele na lista, a poeira na sombra dele acenderia num quadro);
 * - Id estável; Speed com sinal (negativa de ré); YawRate em rad/s, positivo girando a frente pro lado de R = (F.Y, −F.X);
 * - Sand: o peso da areia (1 − asfalto) no pé de trás (x −2,40) e na frente (x +2,55) de cada lagarta; quem nasce no
 *   meio (a cortina) interpola entre os dois.
 */
struct FFXTreadSource
{
	int32 Id = 0;                          // o mesmo id de quadro em quadro (guarda os metros andados)
	FVector PosM = FVector::ZeroVector;    // m: o centro do veículo no chão
	FVector FwdM = FVector(1, 0, 0);       // a frente, no chão
	double Speed = 0.0;                    // m/s (negativa de ré)
	double YawRate = 0.0;                  // rad/s, d(yaw)/dt da Unreal: no giro cada lagarta anda v ± ω·1,425 (a de dentro, menos)
	EFXGround Ground = EFXGround::Sand;
	double Sand[2] = { 1.0, 1.0 };         // o peso da areia no pé de trás de cada lagarta (1 − asfalto): esquerda, direita
	double SandFront[2] = { 1.0, 1.0 };    // o mesmo na frente de cada lagarta
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

	/** A sombra da nuvem no chão (M_FXDustShadow, Modulate). */
	UPROPERTY(EditAnywhere, Category = "FX")
	TObjectPtr<UMaterialInterface> ShadowMaterial;

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
		// a poeira em camadas
		int32 Layer = -1, Src = 0;
		double Odo0 = 0.0, Bake0 = 0.0, Wake = 1.0;
		double Sun = -1.0;                    // o sol que chega no cartão (a sombra do tanque), suavizado no tempo
	};
	struct FLayer
	{
		UProceduralMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
		UMaterialInstanceDynamic* MidDust = nullptr;   // M_FXSixWayDust: a mesma conta sem ler a folha do fogo
	};

	double Rand();
	double Rnd(double A, double B);
	void Step(double Dt);
	void Emit(const FFXTreadSource& Src, double Dt);
	void EmitLayers(const FFXTreadSource& Src, double Dt);
	/** 0 = o próprio tanque tapa o sol nesse ponto (m), 1 = sol direto: a poeira nasce escura na sombra do tanque. */
	double SunVisAt(const FVector& Pm, const FVector& ToSun) const;
	/** O quadro (fracionário) do cozimento num tempo de simulação. */
	double FrameAt(double BakeT) const;
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);
	void LoadPreset();
	void ApplyPreset();
	void LabPose(double Dt);
	/** A altura do chão (m) embaixo de (x, y) em metros: um raio de cima pra baixo, sem o tanque. */
	double GroundAt(double X, double Y, double Fallback) const;
	/** O peso da camada "Asphalt" do terreno em (x, y) metros (0..1): a areia não levanta poeira no asfalto. */
	double AsphaltAt(double X, double Y) const;
	/** O mesmo pelo vértice mais próximo: a leitura da poeira de 13/10 (o caminho sem camadas fica igual ao guardado). */
	double AsphaltAtNearest(double X, double Y) const;
	/** O tanque do jogo de hoje (Game/Config/RTS/Units.json: o tanque da maquete do Look) montado na origem do mapa. */
	void SpawnGameTank();
	/** Lê a camada "Asphalt" do terreno inteiro uma vez (as texturas de peso do Landscape, dado do editor). */
	void BuildAsphaltGrid();
	/** Acha o centro da volta: uma volta inteira no chão, cruzando um pedaço de asfalto (pra ver a poeira parar nele). */
	FVector FindPathCenter() const;
	/** Uma reta só de areia (sem pedra, casa nem asfalto) de Len m, perto do centro; devolve o começo e o rumo. */
	bool FindPathLine(double Len, FVector& Start, FVector& Dir, bool bToAsphalt = false, bool bShade = false) const;
	/** A câmera presa ao tanque e o sol na mesma relação dos alvos (-Cam=r1 / v2 / v2close). */
	void ApplyRig();

	FFXTreadPreset P;
	FFXPreset Scene;
	FFXSceneLight Light;
	FFXBakeMeta Meta;
	FLayer Layers[4];
	UProceduralMeshComponent* ShadowMesh = nullptr;
	UMaterialInstanceDynamic* ShadowMid = nullptr;
	TArray<FCard> Cards;
	TArray<FFXTreadSource> Sources;
	TMap<int32, TPair<double, double>> Acc;    // metros andados desde o último cartão: esquerda, direita
	TMap<int64, double> LayerAcc;              // por veículo, camada e lagarta
	TMap<int64, double> LayerNext;             // o próximo espaçamento já sorteado (m, com quantidade 1)
	TMap<int32, double> Odo;                   // m andados por veículo (a máscara de comprimento)
	TMap<int32, FVector2D> Load;               // a areia presa em cada lagarta (0..1)
	uint32 RngState = 5;

	// o laboratório: o tanque que dá voltas
	double Ang = 0.0, T = 0.0;
	FVector TankPos = FVector::ZeroVector, CamFollowed = FVector::ZeroVector;
	FVector TankFwd = FVector(1, 0, 0);
	EFXGround LabGround = EFXGround::Sand;
	TArray<TPair<TObjectPtr<AStaticMeshActor>, FTransform>> TankParts;   // a peça e onde ela fica em relação ao tanque
	FVector PathCenter = FVector::ZeroVector;      // m: o centro da volta (o tanque do grupo 1 no mapa do jogo)
	bool bCamAligned = false;
	// -TreadPath=line: o tanque anda em reta (o começo é a pose em t = 0; o aquecimento vem de trás)
	bool bLine = false;
	FVector LineStart = FVector::ZeroVector, LineDir = FVector(1, 0, 0);
	// a câmera dos alvos: inclinação, distância (m), campo, rumo do tanque em relação à frente da câmera (+ = pra
	// direita), o sol em relação à frente da câmera (− = esquerda) e a altura dele; o casco ocupa RigFrac da largura
	bool bRig = false;
	double RigPitch = 28.0, RigDist = 53.0, RigFov = 50.0, RigRel = 80.0, SunRel = -41.0, SunEl = 25.0, RigFrac = 0.16;
	bool bRigDistSolved = false;
	// -TreadTanks=N: N tanques em formação (5 por fila, 12 m de lado, 15 m de fila) levantando poeira (o custo);
	// -TreadMeasure=nome: mede o quadro (mediana e p95, CPU e placa) por 10 s e sai
	int32 NumTanks = 1;
	// -TreadGrid=CxR: a formação em grade (C de lado a 10 m, R filas a 12 m), toda no quadro, cada tanque desencontrado
	// (hodômetro e velocidade sorteados): a régua de custo do plano de otimização
	int32 GridCols = 0, GridRows = 0;
	TArray<double> TankOdoOffset, TankSpeedMul;
	// -TreadAB=chave: mede no mesmo processo, trocando a chave a cada 2 s (ABAB), a diferença de placa e de CPU
	FString ABKey;
	bool bABState = false;
	double ABClock = 0.0;
	TArray<float> ABGpu[2], ABCpu[2];
	TArray<double> ABPairs;
	bool bShadowOn = true, bShadowModulate = false, bClip = true, bK2On = true;
	void ApplyAB(bool bState);
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShadowMidMod;
	FString MeasureName;
	TArray<float> MeasMs, MeasGpu, MeasCpu;
	double LastCpuMs = 0.0, LastStepMs = 0.0, LastDrawMs = 0.0, LastPushMs = 0.0;
	TArray<float> MeasStep, MeasDraw, MeasPush;
	bool bMeshEmpty[5] = { false, false, false, false, false };
	int32 MeshCap[5] = { 0, 0, 0, 0, 0 };      // cartões que a seção de cada malha comporta (atualiza sem recriar)
	int32 MeshVerts[5] = { 4, 4, 4, 4, 4 };    // vértices por cartão na seção (4 = caixa, 8 = octógono)
	bool bCut8 = true;                         // o recorte de 8 vértices (o A/B "cut8" desliga)
	bool bDustMat = true;                      // a poeira em camadas usa o M_FXSixWayDust (o A/B "dustmat" troca)
	uint32 StepCount = 0;
	/** Manda os quads pra malha: atualiza a seção se cabem (sem recriar o proxy), senão recria com folga. */
	void PushMesh(class UProceduralMeshComponent* Mesh, int32 Slot, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& U0,
		TArray<FVector2D>& U1, TArray<FVector2D>& U2, TArray<FVector2D>& U3, TArray<FLinearColor>& Cl, int32 VertsPerCard = 4);
	double MeasT = -1.0;
	// o peso do asfalto em cada vértice do terreno (0..255), em coordenadas de quadrado do Landscape
	TArray<uint8> AsphaltGrid;
	FIntPoint AsphaltMin = FIntPoint::ZeroValue, AsphaltSize = FIntPoint::ZeroValue;
	FTransform LandscapeXf;
	int32 Warm = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;

	// -TreadPlugin: a poeira desenhada pelo plugin do jogo (Plugins/RTSTreadDust), com as mesmas fontes desta volta
	UPROPERTY(Transient)
	TObjectPtr<class ARTSTreadDust> PluginDust;
	void StepDust(double Dt);

	FString ShotName;
	TArray<double> ShotAt;
	int32 ExitIn = -1;
	bool bCapture = false;
	bool bFixedStep = false;
};
