#pragma once

#include "CoreMinimal.h"
#include "Math/Float16.h"
#include "Math/Float16Color.h"
#include "RHIFwd.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSSunShadowRender.h"
#include "RTSSunShadow.generated.h"

class UPrimitiveComponent;
class USceneCaptureComponent2D;
class UTexture2D;
class UTextureRenderTarget2D;

/**
 * O mapa de sombra do sol de TODAS as fumaças, fogos e poeiras (16/10, a regra do CLAUDE.md: um sistema só;
 * docs/ESTUDO_LUZ_NA_FUMACA.md, o Fluxo). Duas texturas numa grade girada pro azimute do sol, com o mesmo texel:
 * - a estática (o mapa inteiro, RGBA16F, 0,5 m): o relevo, os prédios, os muros, as palmeiras (a copa como uma faixa solta
 *   no ar). Sai de 4 fotos ortográficas em blocos de 256 m, lidas sem travar, e de uma varredura ao longo do sol numa thread
 *   de fundo (~1 s no começo da partida). Refeita só numa área quando algo muda (AddShadowCaster, RefreshShadowArea);
 * - a dos veículos (R16F, uma janela em volta do foco): o topo da sombra das duas caixas de cada veículo, refeita quando
 *   algum anda.
 * Quem lê: o SunShadow.ush (RTSSunVisibility), num material (a poeira) ou num shader global (o raymarch, pelo
 * FRTSSunShadowParameters). A conta e as medidas: FXLab/Plugins/RTSTreadDust/INTEGRACAO.md, "A sombra por pixel".
 */

/** Um veículo que faz sombra (o jogo manda todos perto do foco a cada quadro: os de lagarta, os de roda, as carcaças). */
struct RTSSUNSHADOW_API FRTSShadowVehicle
{
	int32 Id = 0;
	FVector PosM = FVector::ZeroVector;        // m: o centro do veículo no chão
	FVector FwdM = FVector(1, 0, 0);           // a frente do casco, no chão
	FVector TurretFwdM = FVector::ZeroVector;  // a frente da torre (zero = a do casco)
	double HalfLenM = 3.95, HalfWidthM = 1.88, HullTopM = 1.55, HeightM = 2.48;   // as caixas (o padrão: o tanque USA)
	bool bVisible = true;                      // falso sob a névoa de guerra: não faz sombra (não entrega a posição)
};

namespace RTSSunShadow
{
	/** Na thread do jogo: o sol num ponto do mundo (m), 0..1 (o URTSSunShadowSubsystem::SunVisibilityAt do mundo). */
	RTSSUNSHADOW_API double SunVisibilityCPU(const UWorld* World, const FVector& PosWorldM, double SoftM = 0.2);
}

UCLASS()
class RTSSUNSHADOW_API URTSSunShadowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;

	/**
	 * Os veículos deste quadro (todos perto do foco, os parados também). bAuthoritative: o jogo, com todos (lagarta, roda,
	 * carcaças); falso: uma reserva (a poeira repassa os dela), ignorada se o jogo mandou no último segundo.
	 */
	void SetVehicles(const TArray<FRTSShadowVehicle>& InVehicles, bool bAuthoritative = true)
	{
		if (bAuthoritative || TickCount - LastAuthTick > 60)
		{
			Vehicles = InVehicles;
			if (bAuthoritative)
			{
				LastAuthTick = TickCount;
			}
		}
	}

	/** Um ator com etiqueta de unidade que faz sombra como cenário (o prédio pronto, os do começo da partida). */
	void AddShadowCaster(AActor* Actor);
	/** O prédio destruído ou vendido (chamar antes de destruir o ator). */
	void RemoveShadowCaster(AActor* Actor);
	/** O cenário mudou numa área (palmeira, carro ou poste derrubado): refaz a sombra dela, sem travar (m, no chão). */
	void RefreshShadowArea(const FVector& CenterM, double RadiusM);
	/** Refaz o mapa inteiro (sem travar; o de agora vale até o novo chegar). */
	void RefreshAll() { bHMapDirty = true; }

	/** O mapa chegou (a poeira e o FXLab esperam por ele). */
	bool IsReady() const { return HMapTex != nullptr; }
	/** Desistiu (3 falhas): quem lê volta pro seu jeito de antes. */
	bool HasGivenUp() const { return bHMapGaveUp; }
	UTexture2D* GetMapTexture() const { return HMapTex; }
	UTexture2D* GetVehicleTexture() const { return TMapTex; }
	/** Os parâmetros de material pra uma câmera (cm, mundo): CamToMap (xyz), Axis (xy o eixo, z 1/largura), Window, TanE. */
	void GetMaterialParams(const FVector& CamCm, FLinearColor& OutCamToMap, FLinearColor& OutAxis, FLinearColor& OutWindow, float& OutTanE) const;
	/** Na CPU: o sol num ponto (m) a AboveM m do chão desenhado (só a sombra presa ao chão, sem a corda). */
	double SunVisibility(const FVector& Pm, double AboveM) const;
	/**
	 * Na CPU, a mesma conta do SunShadow.ush num ponto do mundo (m): a sombra presa ao chão, a faixa da copa e a dos
	 * veículos, com a borda SoftM (m). Pras fitas de fumaça, os puffs do míssil e o que mais for calculado no processador.
	 */
	double SunVisibilityAt(const FVector& PosWorldM, double SoftM = 0.2) const;
	/** O chão desenhado (cm) num ponto (m); −1e30 fora do mapa. */
	double GroundZ(const FVector& Pm) const;

	/** Atores com estas etiquetas (e os Pawns) não entram no mapa estático: as unidades andam (vão na camada dos veículos). */
	UPROPERTY(EditAnywhere, Category = "RTS|SunShadow")
	TArray<FName> IgnoreTags = { TEXT("GameTank"), TEXT("RTSUnit") };
	/** As malhas de árvore (pedaço do nome): a copa faz uma faixa solta no ar, com sol por baixo dela. */
	UPROPERTY(EditAnywhere, Category = "RTS|SunShadow")
	TArray<FString> TreeMeshes = { TEXT("Palm"), TEXT("Tree"), TEXT("Pine"), TEXT("Oak"), TEXT("Birch"), TEXT("Spruce") };
	/** As malhas que não entram (o poste: a caixa dele é a da cruzeta; o fio). */
	UPROPERTY(EditAnywhere, Category = "RTS|SunShadow")
	TArray<FString> SkipMeshes = { TEXT("pole"), TEXT("wire"), TEXT("cable") };
	/** O texel do mapa (m). */
	double TexelM = 0.5;

public:
	// a grade (público pra varredura, que roda fora da classe)
	struct FHFrame
	{
		FVector O = FVector::ZeroVector;
		FVector2D U = FVector2D(1, 0), V = FVector2D(0, 1);
		double Texel = 50.0, Tan = 0.14, ZLo = 0.0, ZHi = 0.0;
		int32 Res = 0, Id = 0;
	};
	struct FHMaps
	{
		TArray<float> G;
		TArray<FFloat16> T, TT, B, Top;
		TArray<FFloat16> SB, SA;                 // a faixa da copa depois da varredura (fundo e topo acima do chão; iguais = sem)
	};

private:
	void GatherCasters(TArray<TWeakObjectPtr<UPrimitiveComponent>>& Terrain, TArray<TWeakObjectPtr<UPrimitiveComponent>>& Obst,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& Trees, FBox& Bounds, const AActor* OnlyActor = nullptr) const;
	void TickHeightMap();
	bool StartJob(bool bFull, const FBox& Area);
	void ShootJob(int32 Tile);
	void ApplyTile(const struct FRTSHeightRead& Rd);
	void StartFullSweep();
	void FinishFullJob();
	void FinishPatchJob();
	void FailJob(const TCHAR* Why);
	void UploadRows(int32 J0, int32 J1, TArray<FFloat16Color>&& Rows);
	void DumpMap(const TArray<FFloat16Color>& Out);
	void UpdateVehicleMap();
	void PushRenderData();
	bool FindSun(FVector& OutToSun);
	FVector FocusCm() const;

	TArray<FRTSShadowVehicle> Vehicles;
	TWeakObjectPtr<class UDirectionalLightComponent> SunLight;
	FVector SunDir = FVector::ZeroVector;
	int32 TickCount = 0, LastAuthTick = -1000;
	FHFrame HCur, HJobFrame;
	FHMaps HM;
	int32 HMapTries = 0, HMapWaitTicks = 0, HMapFails = 0, HFrameIds = 0;
	double HCalP75 = 1e9, HCalMed = 0.0, HPrevP75 = 1e9;
	FVector HSun = FVector::ZeroVector;
	bool bHMapDirty = true, bHMapGaveUp = false;
	int32 HJobKind = 0;                        // 0 nada; 1 o mapa inteiro, fotografando; 2 uma área; 3 o mapa inteiro, varrendo
	int32 HJobNb = 0;
	double HJobT0 = 0.0, HJobTShots = 0.0;
	TArray<FVector> HJobGround;
	TArray<FIntPoint> HJobTiles;
	int32 HJobNext = 0;
	FIntRect HJobPatch;
	TArray<TSharedPtr<struct FRTSHeightRead, ESPMode::ThreadSafe>> HJobInFlight;
	TSharedPtr<struct FRTSHeightBuilt, ESPMode::ThreadSafe> HJobBuilt;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HCasters[3];   // o terreno, os obstáculos (sem as árvores), as árvores
	TArray<FBox> HPatches;
	int32 HPatchWait = 0;
	TSet<TWeakObjectPtr<AActor>> ShadowCasters;
	// a janela dos veículos: com o texel e o giro do mapa (o chão de cada texel é um texel do HM.G)
	TArray<FFloat16> TOut;
	int32 TRes = 1024;
	FIntPoint TOrg = FIntPoint(0, 0);
	FIntRect TDirty = FIntRect(0, 0, 0, 0);
	uint32 TLastHash = 0;
	int32 TFrameId = -1;
	TArray<TArray<int32>> TRowSpans;

	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> HeightCapture;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> HJobRT;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> HMapTex;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> TMapTex;
};
