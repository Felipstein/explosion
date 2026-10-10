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
	/** Blocos esparsos (Tools/brm2.py): 8³ voxels por bloco, com 1 de borda no atlas. */
	constexpr int32 Brick = 8;
	constexpr int32 BrickA = Brick + 2;            // 10³ RGBA8 por bloco
	constexpr int32 BrickF = Brick / 2 + 2;        // 6³ RGBA8 por bloco (meia resolução)

	/** Uma gravação na RAM, em blocos esparsos (Intermediate/Raymarch/<nome>.brm2: as do WebGL, Tools/build_raymarch_bake.py
	 *  + Tools/brm2.py; as de outros simuladores, Tools/build_raymarch_vdb.py). Imutável depois de carregada. */
	struct BLASTLABSHADERS_API FBakeData
	{
		int32 Res = 0, FuelRes = 0, MacroRes = 0, Frames = 0;
		float Duration = 6.0f, Curve = 1.7f, Domain = 38.0f, VelMax = 40.0f;
		TArray<FBox3f> Bounds;          // caixa justa por quadro (m locais)
		TArray<FVector3f> LightPos;     // luz gravada por quadro (m locais)
		TArray<FVector3f> LightCol;     // unidades do WebGL (1 = iluminância do sol)
		TArray<int32> NumBricks;        // blocos com fumaça por quadro
		TArray<int64> Offsets;          // começo de cada quadro em Data
		int32 MaxBricks = 0;
		TArray64<uint8> Data;

		static TSharedPtr<FBakeData, ESPMode::ThreadSafe> Load(const FString& Path, FString& OutError);

		int32 PTRes() const { return Res / Brick; }
		/** A tabela do quadro: (Res/8)³ int32, o índice do bloco na lista do quadro ou −1. */
		const int32* FramePT(int32 F) const { return reinterpret_cast<const int32*>(Data.GetData() + Offsets[F]); }
		const uint8* FrameM(int32 F) const { return Data.GetData() + Offsets[F] + int64(PTRes()) * PTRes() * PTRes() * 4; }
		const uint8* FrameBricksA(int32 F) const { return FrameM(F) + int64(MacroRes) * MacroRes * MacroRes; }
		const uint8* FrameBricksF(int32 F) const { return FrameBricksA(F) + int64(NumBricks[F]) * BrickA * BrickA * BrickA * 4; }

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

	// Até 32 quadros na placa (dois por explosão: 16 explosões). Cada vaga tem a
	// tabela de blocos e a ocupação macro do quadro (pequenas, 2 lado a lado em y
	// e 16 empilhadas em z); os blocos de todas ficam num atlas só, do tamanho do
	// pior caso (o quadro mais cheio das gravações carregadas × 32).
	constexpr int32 SlotsY = 2;
	constexpr int32 MaxSlots = 32;
	constexpr int32 MaxAtlasBricksPerAxis = 64;   // 640³ RGBA8 = 1 GB no máximo absoluto
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
	void Submit_GameThread(TArray<BlastRaymarch::FInstance>&& Instances, TArray<BlastRaymarch::FUpload>&& Uploads, FTextureResource* NoiseRes,
		FTextureResource* WorleyRes = nullptr, const FIntVector& PoolRes = FIntVector(96, 48, 24), int32 MaxBricks = 1024);

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

private:
	void Submit_RenderThread(FRHICommandListImmediate& RHICmdList, TArray<BlastRaymarch::FInstance>&& List, const TArray<BlastRaymarch::FUpload>& Ups, FTextureResource* NoiseRes,
		FTextureResource* WorleyRes, const FIntVector& PoolRes, int32 MaxBricks);
	FScreenPassTexture Render_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
	/** As vagas no tamanho da maior gravação carregada (voxels, combustível, macro) e o atlas pro pior caso de blocos. */
	void EnsurePools_RenderThread(FRHICommandListImmediate& RHICmdList, const FIntVector& Dims, int32 MaxBricks);
	/** Os blocos que subiram e ainda não foram copiados pro atlas (o passe da placa, no próximo desenho). */
	void ScatterPending(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef AtlasAT, FRDGTextureRef AtlasFT);

	// render thread
	TArray<BlastRaymarch::FInstance> Instances;
	TRefCountPtr<FRHITexture> Noise;      // a referência segura a textura mesmo se o recurso do UTexture for trocado
	TRefCountPtr<FRHITexture> Worley;     // as bolhas do detalhe de perto (T_DetailWorley)
	TRefCountPtr<FRHITexture> PoolPT, PoolM, AtlasA, AtlasF, BlackBody;
	FIntVector PoolDims = FIntVector::ZeroValue;     // Res, FuelRes, MacroRes do pool
	FIntVector AtlasBricks = FIntVector::ZeroValue;  // blocos por eixo no atlas
	TArray<int32> FreeBricks;                        // blocos livres do atlas (índice linear)
	TArray<int32> SlotBricks[BlastRaymarch::MaxSlots];
	struct FScatter
	{
		BlastRaymarch::FBakeRef Bake;
		int32 Frame = 0;
		TArray<uint32> Coords;                       // onde cada bloco do quadro vai no atlas
	};
	TArray<FScatter> Pending;
	uint32 FrameCounter = 0;

	// acúmulo temporal: o volume resolvido do quadro anterior e a câmera dele
	TRefCountPtr<IPooledRenderTarget> History;
	FIntPoint HistorySize = FIntPoint::ZeroValue;
	FMatrix PrevWorldToClip = FMatrix::Identity;
	uint32 HistoryFrame = 0;
};
