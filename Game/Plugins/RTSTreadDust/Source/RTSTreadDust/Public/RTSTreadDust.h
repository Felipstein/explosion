#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSTreadDust.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * Um veículo que levanta poeira (o contrato da ponte com o jogo, docs/POEIRA_DAS_LAGARTAS.md):
 * - a ponte manda TODOS os veículos perto da câmera a cada quadro, os parados também (Speed 0): o parado não solta poeira,
 *   mas continua fazendo sombra nela (sem ele na lista, a poeira na sombra dele acenderia num quadro);
 * - Id estável; Speed com sinal (negativa de ré); YawRate = a derivada do yaw da Unreal em rad/s (positivo virando pra +Y);
 * - Sand: o peso da areia (1 − max(asfalto, molhado) da grade do WorldForge, FRTSBakedMap::SurfaceAt) no pé de trás
 *   (x −2,40 m) e na frente (x +2,55 m) de cada lagarta; quem nasce no meio (a cortina) interpola entre os dois;
 * - bVisible: falso sob a névoa de guerra (a poeira do inimigo escondido não aparece, como no Zero Hour).
 */
USTRUCT(BlueprintType)
struct RTSTREADDUST_API FRTSTreadSource
{
	GENERATED_BODY()
	int32 Id = 0;
	FVector PosM = FVector::ZeroVector;        // m: o centro do veículo no chão
	FVector FwdM = FVector(1, 0, 0);           // a frente, no chão
	double Speed = 0.0;                        // m/s (negativa de ré)
	double YawRate = 0.0;                      // rad/s: d(yaw)/dt da Unreal (no giro no lugar cada lagarta anda ±ω·1,425)
	double Sand[2] = { 1.0, 1.0 };             // pé de trás: esquerda, direita
	double SandFront[2] = { 1.0, 1.0 };        // frente: esquerda, direita
	bool bVisible = true;
};

/** Uma camada da poeira (o jato K1, a nuvem K2, a cortina C): os mesmos campos do FXLab (FFXTreadLayer). */
struct FRTSTreadLayer
{
	FString Name;
	double XMin = -3.84, XMax = -3.48, YOff = 1.425, YJit = 0.16, Z0 = 0.0;
	double Spacing = 0.25, SpacingJit = 1.0, Life = 0.35, LifeJit = 0.0, MaskDense = 0.0, MaskEnd = 0.0;
	double BakeStart = 0.0, BakeStartJit = 0.0, BakeRate = 1.0, Scale = 1.0, ScaleJit = 0.15, GrowTo = 1.0;
	double Alpha = 0.8, Attack = 0.05, Decay = 0.5, Inherit = 0.3, Drag = 1.6, Kick = 0.0, Side = 0.0, Up = 0.0, Sink = 0.0;
	double WakeRise = 0.0, WakePull = 0.0, WakeGrow = 0.0, WakeLen = 1.0;
	double Orient = 0.0, Flip = 0.0, RotMax = 0.17, PivotBottom = 1.0, CamOffset = 0.0, Erode = 0.2;
	double V0 = 1.88, V1 = 2.5, FollowLoad = 0.0, LoadGain = 1.0, Shadow = 0.0, BaseShade = 1.0, ScaleSlow = 1.0, VFull = 7.9;
};

/** O perfil (Resources/tread_v12.json, a cópia do FXLab/tread_v12_ember.json aprovado como caminho). */
struct FRTSTreadProfile
{
	FString Bake;
	double SoftDist = 35.0, LoadTau = 0.27, HullLen = 7.93, WindX = 0.0, WindY = 0.0;
	double TintR = 1.0, TintG = 0.64, TintB = 0.38, Albedo = 1.2, Density = 0.9;
	TArray<FRTSTreadLayer> Layers;
	bool Load(const FString& Path);
};

/** O atlas do cozimento (Resources/<cozimento>.atlas.json): grade, tempos, pivô, caixa e octógono de cada quadro. */
struct FRTSTreadAtlas
{
	int32 Cols = 8, Rows = 6, Frames = 48;
	double SideM = 4.2;
	FVector2D Pivot = FVector2D(0.5, 0.5);
	TArray<double> Times, BallBottom;
	TArray<FVector4> BBox;
	TArray<TArray<FVector2D>> Cut8;
	FVector2D LightScale = FVector2D(1.0, 1.0);   // as folhas normalizadas pra 8 bits: a luz × ganho (bake_norm8.py)
	bool Load(const FString& Path);
	double FrameAt(double BakeT) const;
};

/**
 * A poeira das lagartas no jogo (a v12 do FXLab: o jato que nasce na roda motriz, a nuvem que abre em leque até a altura
 * da torre, a cortina ao longo da lagarta, a sombra do tanque na poeira e a sombra da nuvem no chão). Um ator só pra
 * todos os veículos: cartões 6-way virados pra câmera (o cozimento do EmberGen tread_kick_v2c), recortados por octógono,
 * numa malha procedural. Os presets: 0 baixa (só a nuvem), 1 média (+ jato), 2 alta (+ cortina), 3 ultra (+ sombra no chão).
 */
UCLASS()
class RTSTREADDUST_API ARTSTreadDust : public AActor
{
	GENERATED_BODY()

public:
	ARTSTreadDust();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Os veículos deste quadro (chamar antes do Tick do ator, ou no mesmo quadro). */
	void SetSources(const TArray<FRTSTreadSource>& InSources) { Sources = InSources; }

	/** 0 baixa, 1 média, 2 alta, 3 ultra. */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void SetQuality(int32 Level) { Quality = FMath::Clamp(Level, 0, 3); }

	/** Liga e desliga tudo (some na hora; o que estava no ar some junto). */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void SetDustEnabled(bool bOn) { bEnabled = bOn; if (!bOn) Cards.Reset(); }

	/**
	 * A sombra do cenário na poeira (casas, árvores, pedras, o relevo): um mapa de profundidade visto do sol, tirado uma
	 * vez (e de novo se o sol mudar); cada vértice da poeira compara a sua distância ao sol com a do mapa. Sem custo na
	 * placa por quadro. Chamar quando o cenário que faz sombra mudar (prédio construído ou destruído).
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Dust")
	void RefreshMapShadows() { bShadowDirty = true; }

	/** Atores com estas etiquetas (e os Pawns) não entram no mapa de sombra: as unidades andam (a sombra delas é a das caixas). */
	UPROPERTY(EditAnywhere, Category = "RTS|Dust")
	TArray<FName> MapShadowIgnoreTags = { TEXT("GameTank"), TEXT("RTSUnit") };

	/** A semente do sorteio (a mesma história a cada partida; o efeito não volta pra simulação). */
	UPROPERTY(EditAnywhere, Category = "RTS|Dust")
	int32 Seed = 5;

	int32 NumCards() const { return Cards.Num(); }

	/** O custo da última conta na CPU (ms: o passo + a montagem da malha), pra medir. */
	double LastMs = 0.0;

	/** Teste (o FXLab compara com a poeira dele): o relógio vem de fora, por Advance(); o Tick só desenha. */
	void SetExternalClock(bool bOn) { bExternalClock = bOn; }
	void Advance(double Dt);

private:
	struct FCard
	{
		FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Back = FVector(-1, 0, 0);
		double Age = 0.0, Life = 1.0, S = 1.0, Rot = 0.0, Flip = 1.0, A = 1.0, GroundZ = 0.0;
		double Odo0 = 0.0, Bake0 = 0.0, Wake = 1.0, Sun = -1.0;
		double Map[3] = { -1.0, -1.0, -1.0 };    // o sol que passa pelo cenário no pé, no meio e no topo do cartão
		int32 Layer = 0, Src = 0;
		bool bDouble = false;                  // a baixa: um cartão no lugar de dois
	};

	double Rand();
	double Rnd(double A, double B) { return A + Rand() * (B - A); }
	void Emit(const FRTSTreadSource& Src, double Dt);
	void Step(double Dt);
	void Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward);
	double SunVisAt(const FVector& Pm, const FVector& ToSun) const;
	void FindSun();
	bool Init();
	void CaptureMapShadows();
	double MapSunVis(const FVector& Pm) const;
	double ShadowDepthAt(double Fu, double Fv) const;
	void PushMesh(UProceduralMeshComponent* Mesh, int32 Slot, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& U0,
		TArray<FVector2D>& U1, TArray<FVector2D>& U2, TArray<FVector2D>& U3, TArray<FLinearColor>& Cl, int32 VertsPerCard);

	FRTSTreadProfile P;
	FRTSTreadAtlas Atlas;
	TArray<FCard> Cards;
	TArray<FRTSTreadSource> Sources;
	TMap<int64, double> LayerAcc, LayerNext;
	TMap<int32, double> Odo;
	TMap<int32, FVector2D> Load;
	TMap<int32, bool> Visible;
	uint32 RngState = 5, StepCount = 0;
	int32 Quality = 3;
	bool bEnabled = true, bExternalClock = false, bInit = false, bReady = false;
	double StepMs = 0.0, TanHalfH = 1.0, TanHalfV = 0.5625;
	// o mapa de sombra do cenário (cm): a profundidade vista do sol, a base da imagem e a calibração
	TArray<float> ShadowDepth;
	int32 ShadowRes = 4096;
	FVector ShadowO = FVector::ZeroVector, ShadowX = FVector::YAxisVector, ShadowY = FVector::ZAxisVector, ShadowF = FVector::XAxisVector;
	FVector ShadowSun = FVector::ZeroVector;
	double ShadowW = 1.0, ShadowCal = 0.0, ShadowSu = 1.0, ShadowSv = -1.0;
	bool bShadowDirty = true;
	FVector SunDir = FVector(0.3, 0.3, 0.9).GetSafeNormal();
	TWeakObjectPtr<class UDirectionalLightComponent> Sun;

	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> CardMesh;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ShadowMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CardMid;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShadowMid;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> ShadowCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> ShadowRT;
	int32 MeshCap[2] = { 0, 0 }, MeshVerts[2] = { 0, 0 };
	bool bMeshEmpty[2] = { false, false };
};
