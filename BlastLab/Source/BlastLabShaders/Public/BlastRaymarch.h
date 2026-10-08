#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class FRHITexture;
class FTextureResource;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;

/**
 * O raymarch das explosões gravadas, portado do WebGL (Shaders/Private/BlastRaymarch.usf,
 * reference/webgl/js/instances.js). Substitui o Heterogeneous Volume da Unreal,
 * que na GTX 1660 saía borrado (buffer de 536×304 em alta) e caro.
 *
 * O jogo (UBlastRaymarchSubsystem) carrega a gravação na RAM, decide quais
 * quadros ficam na placa (um pool de slots) e manda por quadro a lista de
 * explosões; o render marcha depois do TSR, em meia resolução, e compõe na cena
 * antes do bloom e do tonemapper.
 */
namespace BlastRaymarch
{
	/** Uma gravação na RAM (Tools/build_raymarch_bake.py, Intermediate/Raymarch/r96_v<n>.bin). Imutável depois de carregada. */
	struct BLASTLABSHADERS_API FBakeData
	{
		int32 Res = 0, FuelRes = 0, MacroRes = 0, Frames = 0;
		float Duration = 6.0f, Curve = 1.7f, Domain = 38.0f, VelMax = 40.0f;
		TArray<FBox3f> Bounds;          // caixa justa por quadro (m locais)
		TArray<FVector3f> LightPos;     // luz gravada por quadro (m locais)
		TArray<FVector3f> LightCol;     // unidades do WebGL (1 = iluminância do sol)
		TArray64<uint8> Data;
		int64 FrameBytes = 0, BytesA = 0, BytesF = 0, BytesM = 0;

		static TSharedPtr<FBakeData, ESPMode::ThreadSafe> Load(const FString& Path, FString& OutError);

		const uint8* FrameA(int32 F) const { return Data.GetData() + F * FrameBytes; }
		const uint8* FrameF(int32 F) const { return FrameA(F) + BytesA; }
		const uint8* FrameM(int32 F) const { return FrameF(F) + BytesF; }

		/** js/bake.js timeOfFrame / frameOfTime: os quadros são densos no começo. */
		float TimeOfFrame(float F) const { return Duration * FMath::Pow(FMath::Clamp(F / float(Frames - 1), 0.0f, 1.0f), Curve); }
		float FrameOfTime(float T) const { return FMath::Pow(FMath::Clamp(T / Duration, 0.0f, 1.0f), 1.0f / Curve) * float(Frames - 1); }
	};
	using FBakeRef = TSharedPtr<const FBakeData, ESPMode::ThreadSafe>;

	/** Uma explosão neste quadro (no mundo, cm). */
	struct FInstance
	{
		FBakeRef Bake;
		FVector FootCm = FVector::ZeroVector;
		float Scale = 1.0f;
		float Frame = 0.0f;            // quadro contínuo da gravação
		int32 Slot0 = 0, Slot1 = 0;    // onde os dois quadros em volta estão no pool
		float Seed = 0.0f;
		float Keep = 1.0f;             // fração ainda visível (o sumiço no fim, o aquecimento)
		float Thickness = 1.0f;
		bool bNuclear = false;                       // a cor da nuvem nuclear (instances.js _nukeLook)
		FTextureResource* Warp = nullptr;            // a deformação pelo que passa (UBattleSmokeSubsystem), ou nada
		TRefCountPtr<FRHITexture> WarpRHI;           // (render thread)
	};

	/** Um quadro pra subir pro slot antes de desenhar. */
	struct FUpload
	{
		FBakeRef Bake;
		int32 Frame = 0;
		int32 Slot = 0;
	};

	// 32 quadros, 2 lado a lado em y e 16 empilhados em z (a textura 3D vai até 2048
	// por eixo): 96 × 192 × 1536 RGBA8 = 113 MB. Duas por explosão: 16 explosões.
	constexpr int32 SlotsY = 2;
	constexpr int32 MaxSlots = 32;
	constexpr int32 MaxInstances = 16;
	constexpr int32 MaxLightCaches = 4;
	constexpr int32 MaxWarps = 4;           // BattleSmoke::MaxWarps
	constexpr float WarpMaxLocalM = 8.0f;   // BattleSmoke::WarpMaxLocalM: a fumaça empurrada sai da caixa até isso
}

class BLASTLABSHADERS_API FBlastRaymarchExtension : public FSceneViewExtensionBase
{
public:
	FBlastRaymarchExtension(const FAutoRegister& AutoRegister);

	/** Do jogo: a lista do quadro (da mais perto pra mais longe), os quadros pra subir e o ruído. */
	void Submit_GameThread(TArray<BlastRaymarch::FInstance>&& Instances, TArray<BlastRaymarch::FUpload>&& Uploads, FTextureResource* NoiseRes);

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

private:
	void Submit_RenderThread(FRHICommandListImmediate& RHICmdList, TArray<BlastRaymarch::FInstance>&& List, const TArray<BlastRaymarch::FUpload>& Ups, FTextureResource* NoiseRes);
	FScreenPassTexture Render_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
	void EnsurePools_RenderThread(FRHICommandListImmediate& RHICmdList, const BlastRaymarch::FBakeData& Bake);

	// render thread
	TArray<BlastRaymarch::FInstance> Instances;
	TRefCountPtr<FRHITexture> Noise;      // a referência segura a textura mesmo se o recurso do UTexture for trocado
	TRefCountPtr<FRHITexture> PoolA, PoolF, PoolM, BlackBody;
	FIntVector PoolDims = FIntVector::ZeroValue;     // Res, FuelRes, MacroRes do pool
	uint32 FrameCounter = 0;

	// acúmulo temporal: o volume resolvido do quadro anterior e a câmera dele
	TRefCountPtr<IPooledRenderTarget> History;
	FIntPoint HistorySize = FIntPoint::ZeroValue;
	FMatrix PrevWorldToClip = FMatrix::Identity;
	uint32 HistoryFrame = 0;
};
