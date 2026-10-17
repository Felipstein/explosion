// Cópia do BlastLab (13/10/2026, a morte de tanque da v2: a luz e a cor por explosão, a composição em duas camadas,
// a emissão compensada pela exposição): a ponte até o plugin comum. A original continua sendo a referência:
// mudanças de aparência nascem lá e são trazidas pra cá.
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
	/** A última exposição da vista (GetLastEyeAdaptationExposure, a do render do raymarch; 0 = ainda nenhuma): os destroços
	 *  (BlastDebris) compensam o brilho por ela, como o raymarch compensa o fogo (16/10). */
	RTSSHADERS_API float LastViewExposure();

	/** Blocos esparsos (Tools/brm2.py): 8³ voxels por bloco, com 1 de borda no atlas. */
	constexpr int32 Brick = 8;
	constexpr int32 BrickA = Brick + 2;            // 10³ RGBA8 por bloco
	constexpr int32 BrickF = Brick / 2 + 2;        // 6³ RGBA8 por bloco (meia resolução)

	/** Uma gravação na RAM, em blocos esparsos (Intermediate/Raymarch/<nome>.brm2: as do WebGL, Tools/build_raymarch_bake.py
	 *  + Tools/brm2.py; as de outros simuladores, Tools/build_raymarch_vdb.py). Imutável depois de carregada. */
	struct RTSSHADERS_API FBakeData
	{
		int32 Res = 0, FuelRes = 0, MacroRes = 0, Frames = 0;
		float Duration = 6.0f, Curve = 1.7f, Domain = 38.0f, VelMax = 40.0f;
		TArray<FBox3f> Bounds;          // caixa justa por quadro (m locais)
		TArray<FVector3f> LightPos;     // luz gravada por quadro (m locais)
		TArray<FVector3f> LightCol;     // unidades do WebGL (1 = iluminância do sol)
		TArray<int32> NumBricks;        // blocos com fumaça por quadro
		TArray<int64> Offsets;          // começo de cada quadro em Data
		int32 MaxBricks = 0;
		bool bLightPhysical = false;    // a curva de luz já é física (as de VDB); a do WebGL vem comprimida (expoente 0.44)
		bool bLoop = false;             // gravação em loop (o fogo da carcaça, 13/10): o quadro depois do último é o primeiro
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
		float FrameOfTime(float T) const
		{
			if (bLoop)
			{
				// em loop: quadros iguais no tempo, o ciclo tem um quadro a mais que a duração (o último emenda no primeiro)
				const float Period = LoopPeriod();
				return FMath::Fmod(FMath::Max(T, 0.0f), Period) / Period * float(Frames);
			}
			return FMath::Pow(FMath::Clamp(T / Duration, 0.0f, 1.0f), 1.0f / Curve) * float(Frames - 1);
		}
		float LoopPeriod() const { return Duration * float(Frames) / float(FMath::Max(Frames - 1, 1)); }
		/** O quadro depois de F (em loop, volta pro primeiro). */
		int32 NextFrame(int32 F) const { return bLoop ? (F + 1) % Frames : FMath::Min(F + 1, Frames - 1); }
	};
	using FBakeRef = TSharedPtr<const FBakeData, ESPMode::ThreadSafe>;

	/** Uma explosão neste quadro (no mundo, cm). */
	struct FInstance
	{
		FBakeRef Bake;
		FVector FootCm = FVector::ZeroVector;
		float Scale = 1.0f;
		float YawRad = 0.0f;           // o rumo (giro em z, rad): a gravação direcional aponta pra onde o fenômeno aponta (16/10 noite)
		float Frame = 0.0f;            // quadro contínuo da gravação
		int32 Slot0 = 0, Slot1 = 0;    // onde os dois quadros em volta estão no pool
		float Seed = 0.0f;
		float Keep = 1.0f;             // fração ainda visível (o sumiço no fim, o aquecimento)
		float Thickness = 1.0f;
		float NoiseScale = 1.0f;       // o ruído de detalhe do WebGL nesta (0 = só o detalhe da gravação; 13/10)
		// a luz e a cor desta explosão (13/10: a morte de tanque nova no jogo; as construções ficam com as globais).
		// Negativo / W ≤ 0 = a global (o padrão do BlastLab e as opções de linha de comando)
		FVector4f SootLook = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f);    // rgb, albedo da fuligem
		FVector4f DustLook = FVector4f(-1.0f, 0.0f, 0.0f, 0.0f);    // rgb, albedo da poeira
		float SunFull = -1.0f;                                       // 0 = o seno do WebGL, 1 = o sol inteiro
		float SkyGain = -1.0f;                                       // o ganho da luz do céu (1,1 = o do WebGL; 6,28 = físico)
		FVector4f GroundAlbedo = FVector4f(0.0f, 0.0f, 0.0f, 0.0f); // rgb: a areia; W > 0 = liga
		float EmitScale = 1.0f;        // a emissão desta (o fogo da carcaça: a chama baixa, a fumaça manda; 13/10)
		bool bNuclear = false;                       // a cor da nuvem nuclear (instances.js _nukeLook)
		float Flash = 0.0f;                          // o clarão neste quadro (0–1): a média de exp(−t/τ) no intervalo do quadro
		FTextureResource* Warp = nullptr;            // a deformação pelo que passa (UBattleSmokeSubsystem), ou nada
		TRefCountPtr<FRHITexture> WarpRHI;           // (render thread)
	};

	/** Um quadro pra subir pro slot antes de desenhar (ou, bEvict, só devolver os blocos do slot ao atlas). */
	struct FUpload
	{
		FBakeRef Bake;
		int32 Frame = 0;
		int32 Slot = 0;
		bool bEvict = false;
	};

	// Até 32 quadros na placa (dois por explosão: 16 explosões). Cada vaga tem a
	// tabela de blocos e a ocupação macro do quadro (pequenas, 2 lado a lado em y
	// e 16 empilhadas em z); os blocos de todas ficam num atlas só, do tamanho do
	// pior caso (o quadro mais cheio das gravações carregadas × 32).
	constexpr int32 SlotsY = 2;
	// (17/10 tarde) 64 vagas pras 32 explosões; o atlas continua no tamanho do pior caso de 32 (AtlasSlotBudget): os quadros
	// pequenos (o disparo, a areia) cabem de sobra, e o jogo tira os velhos quando os blocos não cabem
	constexpr int32 MaxSlots = 64;
	constexpr int32 AtlasSlotBudget = 32;
	// 500³ RGBA8 = 500 MB (+ 108 MB da meia resolução) no máximo: a Carga Pesada do EmberGen
	// (até 7 592 blocos por quadro) pediria 1,2 GB pro pior caso das 32 vagas cheias dela
	constexpr int32 MaxAtlasBricksPerAxis = 50;
	/** Blocos por eixo do atlas pro quadro mais cheio das gravações carregadas (o jogo e o render usam a mesma conta). */
	inline int32 AtlasSide(int32 MaxBricks)
	{
		return FMath::Clamp(FMath::CeilToInt32(FMath::Pow(float(FMath::Max(MaxBricks, 1)) * AtlasSlotBudget, 1.0f / 3.0f)), 4, MaxAtlasBricksPerAxis);
	}
	// (17/10 tarde, o Felipe: 6 tanques atirando, "some os tiros quando mais de 3 atira") cada tiro gasta 3–4 (o disparo, a
	// areia do sopro, o tiro na areia e a cortina): 16 lotava com 4 tanques
	constexpr int32 MaxInstances = 32;
	// 12 (13/10): as bolas do fogo da carcaça no raymarch (-WreckRm=1) são ~10 ao mesmo tempo; com 4 a maioria ficava
	// sem o sol de verdade (a transmitância do céu gravada) e saía com um contorno preto. 48³ × 4 B = 0,44 MB cada
	constexpr int32 MaxLightCaches = 12;
	constexpr int32 MaxWarps = 4;           // BattleSmoke::MaxWarps
	constexpr float WarpMaxLocalM = 8.0f;   // BattleSmoke::WarpMaxLocalM: a fumaça empurrada sai da caixa até isso
}

class RTSSHADERS_API FBlastRaymarchExtension : public FSceneViewExtensionBase
{
public:
	FBlastRaymarchExtension(const FAutoRegister& AutoRegister);

	/** Do jogo: a lista do quadro (da mais perto pra mais longe), os quadros pra subir e o ruído. */
	void Submit_GameThread(TArray<BlastRaymarch::FInstance>&& Instances, TArray<BlastRaymarch::FUpload>&& Uploads, FTextureResource* NoiseRes,
		FTextureResource* WorleyRes = nullptr, const FIntVector& PoolRes = FIntVector(96, 48, 24), int32 MaxBricks = 1024);

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	/** A névoa do mapa pra aplicar na explosão (13/10): a cena já tem a névoa e a explosão era desenhada limpa por cima ("parece um
	 *  PNG em cima do cenário", o Felipe). Densidade e queda da névoa de altura da Unreal, já em metros; a volumétrica com o albedo dela. */
	struct FFog
	{
		bool bOn = false;
		float DensityPerM = 0.0f;      // FogDensity/1000 por cm × 100 × VolumetricFogExtinctionScale
		float FalloffPerM = 0.0f;      // FogHeightFalloff/1000 por cm × 100 (base 2, como a Unreal)
		float HeightCm = 0.0f;         // a altura da névoa no mundo
		float StartM = 0.0f;           // onde ela começa (a regra de RTS: 0,739 × a distância ao foco)
		FVector3f Albedo = FVector3f(1.0f);
		float G = 0.6f;                // a fase da volumétrica
	};
	void SetFog_GameThread(const FFog& InFog);

	/** A sombra da explosão no chão, nas casas e nas unidades (13/10; "parece um PNG": a nuvem não fazia sombra): a espessura
	 *  óptica entre um plano do chão e o sol, num mapa visto de cima, que a light function do sol (M_SmokeShadowLF) lê. */
	struct FShadow
	{
		class FTextureRenderTargetResource* Out = nullptr;
		FVector2D OriginM = FVector2D::ZeroVector;   // canto do mapa no mundo (m)
		float TexelM = 0.75f;
		int32 Res = 256;
		float GroundZM = 0.0f;                       // o plano no mundo (m)
		FVector3f ToSun = FVector3f(0.0f, 0.0f, 1.0f);
	};
	void SetShadow_GameThread(const FShadow& InShadow);

private:
	FFog Fog;
	FShadow Shadow;
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
	TRefCountPtr<IPooledRenderTarget> History, HistoryNear;   // as duas camadas da marcha (a de trás e a da frente)
	FIntPoint HistorySize = FIntPoint::ZeroValue;
	FMatrix PrevWorldToClip = FMatrix::Identity;
	uint32 HistoryFrame = 0;
};
