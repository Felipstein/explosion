#include "BlastRaymarch.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Dom/JsonObject.h"
#include "GlobalShader.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

// tempos de GPU (blast.ps1 view -ProfileGpu → Tools/gpu_profile.py --live)
DECLARE_GPU_STAT_NAMED(BlastRaymarch, TEXT("BlastRaymarch"));

using namespace BlastRaymarch;

// O clarão (10/10): o do próprio fogo (o Felipe testou ao vivo os dois jeitos e ficou com esse; o
// brilho curto separado cortava no chão), τ 100 ms no relógio da gravação e força 2×. Depuração no app:
// G liga e desliga, J a duração, K a força (BlastLabController).
static TAutoConsoleVariable<int32> CVarFlashMode(TEXT("blast.FlashMode"), 0,
	TEXT("O clarão do fogo: 0 = ligado, 1 = desligado"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> CVarFlashTau(TEXT("blast.FlashTau"), 0.10f,
	TEXT("A queda do clarão (s da gravação)"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> CVarFlashStrength(TEXT("blast.FlashStrength"), 2.0f,
	TEXT("A força do clarão (1, 2, 4)"), ECVF_RenderThreadSafe);

namespace
{
	// reference/webgl/js/instances.js params e VOL_DEFAULTS (os comentários de lá
	// explicam cada um). Mesmos números do material do Heterogeneous Volume
	// (Tools/build_explosion.py), que já eram os do WebGL.
	constexpr float SootExt = 13.5f, DustExt = 5.5f;
	constexpr float SootAlbedo = 0.30f, DustAlbedo = 0.78f;
	const FVector3f SootColor(0.55f, 0.545f, 0.535f), DustColor(0.82f, 0.755f, 0.655f);
	constexpr float EmissionGain = 0.62f, EmissionCurve = 0.44f, TempScale = 0.92f, FlameBoost = 1.5f;
	constexpr float PhaseG = 0.42f, PhaseBack = 0.22f, PhaseMix = 0.32f, SunOcclude = 1.8f;
	constexpr float DetailAmp = 0.42f, DetailScale = 0.85f, DetailDens = 0.34f, Erode = 0.042f, FlowPeriod = 0.5f;
	constexpr float SkyGain = 1.1f, FireGain = 1.0f;
	constexpr int32 MsOctaves = 3;
	constexpr float MsExt = 0.52f, MsScatter = 0.52f, MsPhase = 0.55f;
	constexpr int32 LightSteps = 20, FireSteps = 16;
	constexpr int32 LCRes = 48;                 // ~0.8 m por célula na Carga Pesada
	// passos: 56 na explosão grande na tela, até 16 na pequena (instances.js _lod)
	constexpr int32 Steps = 56, StepsMin = 16, MaxShaderSteps = 96;
	constexpr float RefArea = 0.16f;
	// js/celestial.js SUN_LUX: 1 unidade de iluminância do WebGL = o sol no topo da atmosfera
	constexpr float LuxPerUnit = 128000.0f;
	constexpr float NearM = 0.1f;

	// blackbody.js: Planck × CIE 1931 (aproximação de Wyman, Sloan & Shirley 2013) → sRGB linear
	constexpr int32 BlackBodyN = 1024;
	constexpr double MaxKelvin = 3600.0;

	double Planck(double LambdaNm, double T)
	{
		const double L = LambdaNm * 1e-9;
		const double L5 = L * L * L * L * L;
		return 1.1910429723971884e-16 / (L5 * (FMath::Exp(1.4387768775039337e-2 / (L * T)) - 1.0));
	}
	double G(double X, double Mu, double S1, double S2)
	{
		const double T = (X - Mu) / (X < Mu ? S1 : S2);
		return FMath::Exp(-0.5 * T * T);
	}
	void Spectrum(double T, double& X, double& Y, double& Z)
	{
		X = Y = Z = 0.0;
		for (double L = 360.0; L <= 830.0; L += 2.0)
		{
			const double B = Planck(L, T);
			X += B * (1.056 * G(L, 599.8, 37.9, 31.0) + 0.362 * G(L, 442.0, 16.0, 26.7) - 0.065 * G(L, 501.1, 20.4, 26.2)) * 2.0;
			Y += B * (0.821 * G(L, 568.8, 46.9, 40.5) + 0.286 * G(L, 530.9, 16.3, 31.1)) * 2.0;
			Z += B * (1.217 * G(L, 437.0, 11.8, 36.0) + 0.681 * G(L, 459.0, 26.0, 13.8)) * 2.0;
		}
	}
	TArray<FVector4f> BuildBlackBody()
	{
		TArray<FVector4f> Out;
		Out.SetNum(BlackBodyN);
		double X, Yref, Z;
		Spectrum(MaxKelvin, X, Yref, Z);
		for (int32 i = 0; i < BlackBodyN; ++i)
		{
			const double T = FMath::Max(80.0, (i + 0.5) / BlackBodyN * MaxKelvin);
			double Xs, Ys, Zs;
			Spectrum(T, Xs, Ys, Zs);
			const double x = Xs / Ys, z = Zs / Ys;
			double R = 3.2404542 * x - 1.5371385 - 0.4985314 * z;
			double Gg = -0.9692660 * x + 1.8760108 + 0.0415560 * z;
			double B = 0.0556434 * x - 0.2040259 + 1.0572252 * z;
			const double M = FMath::Min3(R, Gg, B);
			if (M < 0.0)
			{
				R -= M; Gg -= M; B -= M;
			}
			double Lum = 0.2126 * R + 0.7152 * Gg + 0.0722 * B;
			Lum = Lum != 0.0 ? Lum : 1.0;
			Out[i] = FVector4f(float(R / Lum), float(Gg / Lum), float(B / Lum), float(Ys / Yref));
		}
		return Out;
	}

	float ParamOr(const TCHAR* Key, float Default)
	{
		float V = Default;
		FParse::Value(FCommandLine::Get(), Key, V);
		return V;
	}
}

// ---- a gravação na RAM ------------------------------------------------------------

TSharedPtr<FBakeData, ESPMode::ThreadSafe> FBakeData::Load(const FString& Path, FString& OutError)
{
	TArray64<uint8> Raw;
	if (!FFileHelper::LoadFileToArray(Raw, *Path))
	{
		OutError = FString::Printf(TEXT("não li %s (rode Tools/build_raymarch_bake.py)"), *Path);
		return nullptr;
	}
	if (Raw.Num() < 8 || FMemory::Memcmp(Raw.GetData(), "BRM2", 4) != 0)
	{
		OutError = FString::Printf(TEXT("%s: formato desconhecido (as densas BRM1 viram BRM2 com Tools/brm2.py)"), *Path);
		return nullptr;
	}
	const uint32 HeadLen = *reinterpret_cast<const uint32*>(Raw.GetData() + 4);
	if (Raw.Num() < 8 + int64(HeadLen))
	{
		OutError = TEXT("arquivo cortado");
		return nullptr;
	}
	TArray<ANSICHAR> HeadBytes(reinterpret_cast<const ANSICHAR*>(Raw.GetData() + 8), int32(HeadLen));
	HeadBytes.Add(0);
	const FString Head(UTF8_TO_TCHAR(HeadBytes.GetData()));
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Head), J) || !J.IsValid())
	{
		OutError = TEXT("cabeçalho inválido");
		return nullptr;
	}
	TSharedPtr<FBakeData, ESPMode::ThreadSafe> B = MakeShared<FBakeData, ESPMode::ThreadSafe>();
	B->Res = J->GetIntegerField(TEXT("res"));
	B->FuelRes = J->GetIntegerField(TEXT("fuelRes"));
	B->MacroRes = J->GetIntegerField(TEXT("macroRes"));
	B->Frames = J->GetIntegerField(TEXT("frames"));
	B->Duration = float(J->GetNumberField(TEXT("duration")));
	B->Curve = float(J->GetNumberField(TEXT("curve")));
	B->Domain = float(J->GetNumberField(TEXT("domain")));
	B->VelMax = float(J->GetNumberField(TEXT("velMax")));
	auto Vec = [](const TSharedPtr<FJsonValue>& V, int32 Offset) {
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return FVector3f(float(A[Offset]->AsNumber()), float(A[Offset + 1]->AsNumber()), float(A[Offset + 2]->AsNumber()));
	};
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("bounds")))
	{
		B->Bounds.Add(FBox3f(Vec(V, 0), Vec(V, 3)));
	}
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("lightPos")))
	{
		B->LightPos.Add(Vec(V, 0));
	}
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("lightCol")))
	{
		B->LightCol.Add(Vec(V, 0));
	}
	// a curva de luz: a do WebGL saiu comprimida pelo expoente 0.44 do material de lá (ExplosionVolume.cpp
	// LightCurveK); as dos conversores de VDB são físicas (Σ combustível·T⁴). As antigas sem o campo: pela origem
	bool bPhysical = false;
	B->bLightPhysical = J->TryGetBoolField(TEXT("lightPhysical"), bPhysical) ? bPhysical : !J->GetStringField(TEXT("source")).Contains(TEXT("reference/webgl"));
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("frameBricks")))
	{
		B->NumBricks.Add(int32(V->AsNumber()));
		B->MaxBricks = FMath::Max(B->MaxBricks, B->NumBricks.Last());
	}
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("frameOffsets")))
	{
		B->Offsets.Add(int64(V->AsNumber()));
	}
	const int64 Offset = 8 + int64(HeadLen);
	const int64 Body = Raw.Num() - Offset;
	bool bOk = B->Frames >= 2 && J->GetIntegerField(TEXT("brick")) == Brick && B->Res % Brick == 0 && B->FuelRes * 2 == B->Res
		&& B->Bounds.Num() == B->Frames && B->LightPos.Num() == B->Frames && B->LightCol.Num() == B->Frames
		&& B->NumBricks.Num() == B->Frames && B->Offsets.Num() == B->Frames;
	// cada quadro: tabela, macro, blocos A, blocos F, um atrás do outro
	for (int32 F = 0; bOk && F < B->Frames; ++F)
	{
		const int64 Size = int64(B->PTRes()) * B->PTRes() * B->PTRes() * 4 + int64(B->MacroRes) * B->MacroRes * B->MacroRes
			+ int64(B->NumBricks[F]) * (BrickA * BrickA * BrickA + BrickF * BrickF * BrickF) * 4;
		const int64 End = F + 1 < B->Frames ? B->Offsets[F + 1] : Body;
		bOk = B->Offsets[F] + Size == End;
	}
	if (!bOk)
	{
		OutError = FString::Printf(TEXT("%s: tamanho inesperado"), *Path);
		return nullptr;
	}
	B->Data.SetNumUninitialized(Body);
	FMemory::Memcpy(B->Data.GetData(), Raw.GetData() + Offset, Body);
	return B;
}

// ---- shaders ---------------------------------------------------------------------

BEGIN_SHADER_PARAMETER_STRUCT(FBlastRaymarchParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(float, Res)
	SHADER_PARAMETER(float, FuelRes)
	SHADER_PARAMETER(float, MacroRes)
	SHADER_PARAMETER(float, PTRes)
	SHADER_PARAMETER(FVector3f, AtlasASize)
	SHADER_PARAMETER(FVector3f, AtlasFSize)
	SHADER_PARAMETER(float, LCRes)
	SHADER_PARAMETER(float, Slots)
	SHADER_PARAMETER(float, SlotsY)
	SHADER_PARAMETER(float, LCSlots)
	SHADER_PARAMETER(float, VelMax)
	SHADER_PARAMETER(float, SootExt)
	SHADER_PARAMETER(float, DustExt)
	SHADER_PARAMETER(float, SootAlbedo)
	SHADER_PARAMETER(float, DustAlbedo)
	SHADER_PARAMETER(FVector3f, SootColor)
	SHADER_PARAMETER(FVector3f, DustColor)
	SHADER_PARAMETER(float, EmissionGain)
	SHADER_PARAMETER(float, EmissionCurve)
	SHADER_PARAMETER(float, TempScale)
	SHADER_PARAMETER(float, FlameBoost)
	SHADER_PARAMETER(float, PhaseG)
	SHADER_PARAMETER(float, PhaseBack)
	SHADER_PARAMETER(float, PhaseMix)
	SHADER_PARAMETER(float, SunOcclude)
	SHADER_PARAMETER(float, DetailAmp)
	SHADER_PARAMETER(float, DetailScale)
	SHADER_PARAMETER(float, DetailDens)
	SHADER_PARAMETER(float, Erode)
	SHADER_PARAMETER(float, FlowPeriod)
	SHADER_PARAMETER(float, DetailAmp2)
	SHADER_PARAMETER(float, SkyGain)
	SHADER_PARAMETER(float, FireGain)
	SHADER_PARAMETER(float, MsExt)
	SHADER_PARAMETER(float, MsScatter)
	SHADER_PARAMETER(float, MsPhase)
	SHADER_PARAMETER(int32, MsOctaves)
	SHADER_PARAMETER(int32, SunSteps)
	SHADER_PARAMETER(int32, FireSteps)
	SHADER_PARAMETER(float, RadianceScale)
	SHADER_PARAMETER(FVector4f, TermGain)
	SHADER_PARAMETER(int32, DebugView)
	SHADER_PARAMETER(int32, NumInstances)
	SHADER_PARAMETER_ARRAY(FVector4f, InstFoot, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstFrame, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstMisc, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstLight, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstBoxMin, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstBoxMax, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstFirePos, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstFireCol, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstLook, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstDetail, [MaxInstances])
	SHADER_PARAMETER_ARRAY(FVector4f, InstRes, [MaxInstances])
	SHADER_PARAMETER(float, DetailErode)
	SHADER_PARAMETER(float, DetailTemp)
	SHADER_PARAMETER(float, DetailPeriod)
	SHADER_PARAMETER(float, SharpPow)
	SHADER_PARAMETER(float, SharpRef)
	SHADER_PARAMETER(float, FlashGain)
	SHADER_PARAMETER(float, FlashDT)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Worley)
	SHADER_PARAMETER(FMatrix44f, ClipToCamera)
	SHADER_PARAMETER(FVector3f, CameraForward)
	SHADER_PARAMETER(float, NearM)
	SHADER_PARAMETER(FUintVector2, HalfSize)
	SHADER_PARAMETER(FUintVector2, OutMin)
	SHADER_PARAMETER(FUintVector2, OutSize)
	SHADER_PARAMETER(FUintVector2, OutExtent)
	SHADER_PARAMETER(float, Jitter)
	SHADER_PARAMETER(int32, LCInstance)
	SHADER_PARAMETER(int32, LCSlot)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<uint>, PoolPT)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, AtlasA)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, AtlasF)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float>, PoolM)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Noise)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, BlackBody)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Warp0)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Warp1)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Warp2)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float4>, Warp3)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SceneColorIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, VolIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float2>, VolDepthIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, HistIn)
	SHADER_PARAMETER_RDG_TEXTURE(Texture3D<float2>, PoolLC)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, VolOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, VolDepthOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, HistOut)
	SHADER_PARAMETER(FMatrix44f, CamToPrevClip)
	SHADER_PARAMETER(float, HistValid)
	SHADER_PARAMETER(float, HistAlpha)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorOut)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float2>, PoolLCOut)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearClamp)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearWrap)
END_SHADER_PARAMETER_STRUCT()

#define BLAST_RAYMARCH_SHADER(Name, Entry)                                                            \
	class Name : public FGlobalShader                                                                 \
	{                                                                                                 \
	public:                                                                                           \
		DECLARE_GLOBAL_SHADER(Name);                                                                  \
		using FParameters = FBlastRaymarchParameters;                                                 \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)    \
		{                                                                                             \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);              \
		}                                                                                             \
	};                                                                                                \
	IMPLEMENT_GLOBAL_SHADER(Name, "/BlastLab/Private/BlastRaymarch.usf", Entry, SF_Compute);

// a subida dos blocos pro atlas: um grupo por bloco
BEGIN_SHADER_PARAMETER_STRUCT(FBlastScatterParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BrickData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, BrickCoords)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, AtlasOut)
END_SHADER_PARAMETER_STRUCT()

#define BLAST_SCATTER_SHADER(Name, Entry)                                                             \
	class Name : public FGlobalShader                                                                 \
	{                                                                                                 \
	public:                                                                                           \
		DECLARE_GLOBAL_SHADER(Name);                                                                  \
		using FParameters = FBlastScatterParameters;                                                  \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)    \
		{                                                                                             \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);              \
		}                                                                                             \
	};                                                                                                \
	IMPLEMENT_GLOBAL_SHADER(Name, "/BlastLab/Private/BlastRaymarch.usf", Entry, SF_Compute);

BLAST_SCATTER_SHADER(FBlastScatterACS, "MainScatterA")
BLAST_SCATTER_SHADER(FBlastScatterFCS, "MainScatterF")
BLAST_RAYMARCH_SHADER(FBlastRaymarchLightCacheCS, "MainLightCache")
BLAST_RAYMARCH_SHADER(FBlastRaymarchMarchCS, "MainMarch")
BLAST_RAYMARCH_SHADER(FBlastRaymarchTemporalCS, "MainTemporal")
BLAST_RAYMARCH_SHADER(FBlastRaymarchCompositeCS, "MainComposite")

namespace
{
	FRDGTextureRef External(FRDGBuilder& GraphBuilder, FRHITexture* Texture, const TCHAR* Name)
	{
		return GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Texture, Name));
	}

	/** Pixels por metro a 1 m da câmera, na altura da imagem de saída (o voxel na tela, pro detalhe de perto). */
	float OutRectHeightForDetail(const FSceneView& View, const FIntRect& OutRect)
	{
		return float(OutRect.Height() * 0.5 * View.ViewMatrices.GetViewToClipNoAA().M[1][1]);
	}

	/** Os 8 cantos da caixa na tela: a fração da tela que ela cobre (0 se atrás da câmera ou fora). */
	float ScreenFraction(const FSceneView& View, const FBox& Box)
	{
		const FMatrix& VP = View.ViewMatrices.GetWorldToClip();
		FVector2D Min(1e9), Max(-1e9);
		for (int32 c = 0; c < 8; ++c)
		{
			const FVector P((c & 1) ? Box.Max.X : Box.Min.X, (c & 2) ? Box.Max.Y : Box.Min.Y, (c & 4) ? Box.Max.Z : Box.Min.Z);
			const FVector4 H = VP.TransformFVector4(FVector4(P, 1.0));
			if (H.W <= 1e-4)
			{
				return 1.0f;            // atravessa a câmera: a tela toda
			}
			const FVector2D N(H.X / H.W, H.Y / H.W);
			Min = FVector2D::Min(Min, N);
			Max = FVector2D::Max(Max, N);
		}
		Min = FVector2D::Max(Min, FVector2D(-1.0));
		Max = FVector2D::Min(Max, FVector2D(1.0));
		if (Max.X <= Min.X || Max.Y <= Min.Y)
		{
			return 0.0f;
		}
		return float((Max.X - Min.X) * (Max.Y - Min.Y) * 0.25);
	}
}

// ---- a extensão ------------------------------------------------------------------

FBlastRaymarchExtension::FBlastRaymarchExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FBlastRaymarchExtension::Submit_GameThread(TArray<FInstance>&& InInstances, TArray<FUpload>&& Uploads, FTextureResource* NoiseRes,
	FTextureResource* WorleyRes, const FIntVector& PoolRes, int32 MaxBricks)
{
	// a referência mantém a extensão viva até o comando rodar (o mundo pode fechar antes)
	ENQUEUE_RENDER_COMMAND(BlastRaymarchSubmit)(
		[Self = StaticCastSharedRef<FBlastRaymarchExtension>(AsShared()), List = MoveTemp(InInstances), Ups = MoveTemp(Uploads), NoiseRes, WorleyRes, PoolRes, MaxBricks]
		(FRHICommandListImmediate& RHICmdList) mutable
		{
			Self->Submit_RenderThread(RHICmdList, MoveTemp(List), Ups, NoiseRes, WorleyRes, PoolRes, MaxBricks);
		});
}

void FBlastRaymarchExtension::Submit_RenderThread(FRHICommandListImmediate& RHICmdList, TArray<FInstance>&& List, const TArray<FUpload>& Ups, FTextureResource* NoiseRes,
	FTextureResource* WorleyRes, const FIntVector& PoolRes, int32 MaxBricks)
{
	if (List.Num() || Ups.Num())
	{
		EnsurePools_RenderThread(RHICmdList, PoolRes, MaxBricks);
	}
	// primeiro devolve os blocos dos quadros que saem, depois reserva os dos que entram
	for (const FUpload& U : Ups)
	{
		if (U.Slot >= 0 && U.Slot < MaxSlots)
		{
			FreeBricks.Append(SlotBricks[U.Slot]);
			SlotBricks[U.Slot].Reset();
		}
	}
	const int32 PT = PoolDims.X / Brick;
	TArray<uint32> Table;
	for (const FUpload& U : Ups)
	{
		if (U.bEvict || !U.Bake)
		{
			continue;       // só devolveu os blocos (o jogo tirou o quadro velho da vaga)
		}
		const FBakeData& B = *U.Bake;
		if (B.Res > PoolDims.X || B.FuelRes > PoolDims.Y || B.MacroRes > PoolDims.Z || U.Slot < 0 || U.Slot >= MaxSlots)
		{
			continue;       // maior que a vaga: não cabe
		}
		const int32 K = B.NumBricks[U.Frame];
		const int32 Pt = B.PTRes();
		FScatter S;
		if (K > FreeBricks.Num())
		{
			// não deveria acontecer (o atlas é do pior caso): o quadro fica vazio em vez de ler blocos de outro
			UE_LOG(LogTemp, Warning, TEXT("BlastRaymarch: atlas cheio (%d blocos pedidos, %d livres)"), K, FreeBricks.Num());
		}
		else
		{
			SlotBricks[U.Slot].Append(FreeBricks.GetData() + FreeBricks.Num() - K, K);
			FreeBricks.SetNum(FreeBricks.Num() - K, EAllowShrinking::No);
			S.Coords.SetNumUninitialized(K);
			for (int32 k = 0; k < K; ++k)
			{
				const int32 L = SlotBricks[U.Slot][k];
				const int32 X = L % AtlasBricks.X, Y = (L / AtlasBricks.X) % AtlasBricks.Y, Z = L / (AtlasBricks.X * AtlasBricks.Y);
				S.Coords[k] = uint32(X) | (uint32(Y) << 10) | (uint32(Z) << 20);
			}
		}
		// a tabela do quadro com os endereços no atlas
		const int32* Src = B.FramePT(U.Frame);
		Table.SetNumUninitialized(Pt * Pt * Pt);
		for (int32 i = 0; i < Table.Num(); ++i)
		{
			Table[i] = Src[i] >= 0 && Src[i] < S.Coords.Num() ? S.Coords[Src[i]] : 0xFFFFFFFFu;
		}
		// o slot s fica em y = (s % SlotsY)·N, z = (s / SlotsY)·N da VAGA (BlastRaymarch.usf SlotBase);
		// uma gravação menor que a vaga ocupa o canto dela
		const int32 Sy = U.Slot % SlotsY, Sz = U.Slot / SlotsY;
		RHICmdList.UpdateTexture3D(PoolPT.GetReference(), 0, FUpdateTextureRegion3D(0, Sy * PT, Sz * PT, 0, 0, 0, Pt, Pt, Pt),
			Pt * 4, Pt * Pt * 4, reinterpret_cast<const uint8*>(Table.GetData()));
		RHICmdList.UpdateTexture3D(PoolM.GetReference(), 0, FUpdateTextureRegion3D(0, Sy * PoolDims.Z, Sz * PoolDims.Z, 0, 0, 0, B.MacroRes, B.MacroRes, B.MacroRes),
			B.MacroRes, B.MacroRes * B.MacroRes, B.FrameM(U.Frame));
		if (S.Coords.Num())
		{
			S.Bake = U.Bake;
			S.Frame = U.Frame;
			Pending.Add(MoveTemp(S));
		}
	}
	for (FInstance& I : List)
	{
		I.WarpRHI = I.Warp ? I.Warp->TextureRHI : nullptr;
	}
	Instances = MoveTemp(List);
	Noise = NoiseRes ? NoiseRes->TextureRHI : nullptr;
	Worley = WorleyRes ? WorleyRes->TextureRHI : nullptr;
}

void FBlastRaymarchExtension::ScatterPending(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef AtlasAT, FRDGTextureRef AtlasFT)
{
	for (const FScatter& S : Pending)
	{
		const FBakeData& B = *S.Bake;
		const int32 K = S.Coords.Num();
		FRDGBufferRef Coords = CreateStructuredBuffer(GraphBuilder, TEXT("BlastRaymarch.BrickCoords"), sizeof(uint32), K, S.Coords.GetData(), K * sizeof(uint32));
		const struct { const uint8* Data; int32 Side; FRDGTextureRef Atlas; } Parts[2] = {
			{ B.FrameBricksA(S.Frame), BrickA, AtlasAT }, { B.FrameBricksF(S.Frame), BrickF, AtlasFT } };
		for (int32 p = 0; p < 2; ++p)
		{
			const uint32 Texels = uint32(K) * Parts[p].Side * Parts[p].Side * Parts[p].Side;
			FRDGBufferRef Data = CreateStructuredBuffer(GraphBuilder, TEXT("BlastRaymarch.BrickData"), sizeof(uint32), Texels, Parts[p].Data, uint64(Texels) * 4);
			FBlastScatterParameters* P = GraphBuilder.AllocParameters<FBlastScatterParameters>();
			P->BrickData = GraphBuilder.CreateSRV(Data);
			P->BrickCoords = GraphBuilder.CreateSRV(Coords);
			P->AtlasOut = GraphBuilder.CreateUAV(Parts[p].Atlas);
			if (p == 0)
			{
				TShaderMapRef<FBlastScatterACS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch ScatterA %d", K), Shader, P, FIntVector(K, 1, 1));
			}
			else
			{
				TShaderMapRef<FBlastScatterFCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch ScatterF %d", K), Shader, P, FIntVector(K, 1, 1));
			}
		}
	}
	Pending.Reset();
}

void FBlastRaymarchExtension::EnsurePools_RenderThread(FRHICommandListImmediate& RHICmdList, const FIntVector& Dims, int32 MaxBricks)
{
	// o atlas pro pior caso: todas as vagas com o quadro mais cheio das gravações carregadas (com teto;
	// o jogo tira das vagas os quadros velhos quando os blocos não cabem, UBlastRaymarchSubsystem::Tick)
	const int32 Side = AtlasSide(MaxBricks);
	if (PoolPT.IsValid() && PoolDims == Dims && AtlasBricks.X >= Side)
	{
		return;
	}
	PoolDims = Dims;
	auto Make = [&](const TCHAR* Name, int32 N, EPixelFormat Format) {
		return RHICmdList.CreateTexture(FRHITextureCreateDesc::Create3D(Name, N, N * SlotsY, N * FMath::DivideAndRoundUp(MaxSlots, SlotsY), Format)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask));
	};
	PoolPT = Make(TEXT("BlastRaymarch.PoolPT"), Dims.X / Brick, PF_R32_UINT);
	PoolM = Make(TEXT("BlastRaymarch.PoolM"), Dims.Z, PF_G8);
	AtlasBricks = FIntVector(Side);
	auto MakeAtlas = [&](const TCHAR* Name, int32 N) {
		return RHICmdList.CreateTexture(FRHITextureCreateDesc::Create3D(Name, Side * N, Side * N, Side * N, PF_R8G8B8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV)
			.SetInitialState(ERHIAccess::SRVMask));
	};
	AtlasA = MakeAtlas(TEXT("BlastRaymarch.AtlasA"), BrickA);
	AtlasF = MakeAtlas(TEXT("BlastRaymarch.AtlasF"), BrickF);
	// as vagas antigas apontavam pro atlas anterior: todas recomeçam vazias (o jogo sobe de novo o que precisar)
	FreeBricks.Reset(Side * Side * Side);
	for (int32 i = Side * Side * Side - 1; i >= 0; --i)
	{
		FreeBricks.Add(i);
	}
	for (TArray<int32>& S : SlotBricks)
	{
		S.Reset();
	}
	Pending.Reset();
	UE_LOG(LogTemp, Display, TEXT("BlastRaymarch: atlas de %d³ blocos (%d MB), vagas de %d³"), Side,
		int32((int64(Side * BrickA) * Side * BrickA * Side * BrickA + int64(Side * BrickF) * Side * BrickF * Side * BrickF) * 4 >> 20), Dims.X);
	if (!BlackBody.IsValid())
	{
		const TArray<FVector4f> Lut = BuildBlackBody();
		BlackBody = RHICmdList.CreateTexture(FRHITextureCreateDesc::Create2D(TEXT("BlastRaymarch.BlackBody"), BlackBodyN, 1, PF_A32B32G32R32F)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask));
		RHICmdList.UpdateTexture2D(BlackBody.GetReference(), 0, FUpdateTextureRegion2D(0, 0, 0, 0, BlackBodyN, 1),
			BlackBodyN * sizeof(FVector4f), reinterpret_cast<const uint8*>(Lut.GetData()));
	}
}

void FBlastRaymarchExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	// depois do TSR e antes do bloom (a mesma saída que o "motion blur" entrega):
	// a imagem já está na resolução da tela, sem o jitter, e o fogo ainda entra no bloom e na exposição
	if (Pass == EPostProcessingPass::MotionBlur && Instances.Num() && !View.bIsSceneCapture && !View.bIsReflectionCapture && !View.bIsPlanarReflection)
	{
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FBlastRaymarchExtension::Render_RenderThread));
	}
}

FScreenPassTexture FBlastRaymarchExtension::Render_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	const FRDGTextureRef Depth = Inputs.SceneTextures.SceneTextures ? Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture : nullptr;
	if (!Instances.Num() || !PoolPT.IsValid() || !AtlasA.IsValid() || !Noise.IsValid() || !Depth || !SceneColor.IsValid())
	{
		return SceneColor;
	}
	RDG_EVENT_SCOPE_STAT(GraphBuilder, BlastRaymarch, "BlastRaymarch");

	const FVector CamCm = View.ViewMatrices.GetViewOrigin();
	auto Rel = [&](const FVector& Cm) { return FVector3f((Cm - CamCm) * 0.01); };
	const FBakeData& B0 = *Instances[0].Bake;
	static const int32 StepsCfg = int32(ParamOr(TEXT("RmSteps="), float(Steps)));
	static const int32 LightCachesCfg = FMath::Clamp(int32(ParamOr(TEXT("RmLightCaches="), float(MaxLightCaches))), 0, MaxLightCaches);
	static const float MvScale = ParamOr(TEXT("RmMV="), 1.0f);
	// a resolução da marcha em relação à tela (o WebGL marchava em meia) e o peso
	// do quadro novo no acúmulo (1 = sem acúmulo)
	static const float MarchRes = FMath::Clamp(ParamOr(TEXT("RmRes="), 0.5f), 0.25f, 1.0f);
	static const float HistAlpha = FMath::Clamp(ParamOr(TEXT("RmTemporal="), 0.2f), 0.02f, 1.0f);

	FBlastRaymarchParameters Base;
	FMemory::Memzero(Base);
	Base.View = View.ViewUniformBuffer;
	Base.Res = float(PoolDims.X);
	Base.FuelRes = float(PoolDims.Y);
	Base.MacroRes = float(PoolDims.Z);
	Base.LCRes = float(LCRes);
	Base.PTRes = float(PoolDims.X / Brick);
	Base.AtlasASize = FVector3f(FIntVector(AtlasBricks * BrickA));
	Base.AtlasFSize = FVector3f(FIntVector(AtlasBricks * BrickF));
	Base.Slots = float(MaxSlots);
	Base.SlotsY = float(SlotsY);
	Base.LCSlots = float(MaxLightCaches);
	Base.VelMax = B0.VelMax;
	Base.SootExt = SootExt;
	Base.DustExt = DustExt;
	Base.SootAlbedo = SootAlbedo;
	Base.DustAlbedo = DustAlbedo;
	Base.SootColor = SootColor;
	Base.DustColor = DustColor;
	Base.EmissionGain = EmissionGain;
	Base.EmissionCurve = EmissionCurve;
	Base.TempScale = TempScale;
	Base.FlameBoost = FlameBoost;
	Base.PhaseG = PhaseG;
	Base.PhaseBack = PhaseBack;
	Base.PhaseMix = PhaseMix;
	Base.SunOcclude = SunOcclude;
	Base.DetailAmp = DetailAmp;
	Base.DetailScale = DetailScale;
	Base.DetailDens = DetailDens;
	Base.Erode = Erode;
	Base.FlowPeriod = FlowPeriod;
	static const float Detail2 = ParamOr(TEXT("RmDetail2="), 0.0f);
	Base.DetailAmp2 = Detail2;
	Base.SkyGain = SkyGain;
	Base.FireGain = FireGain;
	Base.MsExt = MsExt;
	Base.MsScatter = MsScatter;
	Base.MsPhase = MsPhase;
	Base.MsOctaves = MsOctaves;
	Base.SunSteps = LightSteps;
	Base.FireSteps = FireSteps;
	Base.RadianceScale = LuxPerUnit;
	// depuração: -RmTerms=sol,céu,fogo,emissão (ex. 0,1,0,0 = só a luz do céu)
	static const FVector4f Terms = []
	{
		FVector4f T(1.0f, 1.0f, 1.0f, 1.0f);
		FString S;
		TArray<FString> P;
		if (FParse::Value(FCommandLine::Get(), TEXT("RmTerms="), S, false) && S.ParseIntoArray(P, TEXT(",")) == 4)
		{
			T = FVector4f(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3]));
		}
		return T;
	}();
	Base.TermGain = Terms;
	// o detalhe fino de perto: -RmDetail=0 desliga (comparação), -RmDetailTemp= a parte do fogo
	static const float DetailErodeCfg = ParamOr(TEXT("RmDetail="), 0.5f);
	static const float DetailTempCfg = ParamOr(TEXT("RmDetailTemp="), 0.04f);
	// com 1 s as bolhas esticavam em riscos na bola que sobe rápido (o ruído é
	// levado pela velocidade e a distorção cresce com |∇v|·ciclo; 08/10)
	static const float DetailPeriodCfg = FMath::Max(ParamOr(TEXT("RmDetailPeriod="), 0.2f), 0.05f);
	Base.DetailPeriod = DetailPeriodCfg;
	static const float SharpPowCfg = ParamOr(TEXT("RmSharp="), 0.0f);
	static const float SharpRefCfg = FMath::Max(ParamOr(TEXT("RmSharpRef="), 0.25f), 1e-3f);
	Base.SharpPow = SharpPowCfg;
	Base.SharpRef = SharpRefCfg;
	// o clarão (10/10; Jimenez, SIGGRAPH 2014: o halo é o bloom do próprio fogo, sem sprite): nos primeiros
	// ~0.1–0.2 s da gravação (τ 100 ms, blast.FlashTau) o miolo quente esquenta +500 K (passa por amarelo-branco: 2000 → 2500 K é ~×5 de
	// luminância de corpo negro, ~×2 depois da curva da emissão) e a emissão × (1 + 3·força·clarão), força 2× (o Felipe escolheu ao vivo, 10/10). -RmFlashGain=0 -RmFlashDT=0 desligam (comparação)
	// a força (blast.FlashStrength 1/2/4) sobe o ganho e o calor juntos
	static const float FlashGainCfg = ParamOr(TEXT("RmFlashGain="), 3.0f);
	static const float FlashDTCfg = ParamOr(TEXT("RmFlashDT="), 500.0f / 3600.0f / TempScale);
	const int32 FlashMode = CVarFlashMode.GetValueOnRenderThread();
	const float FlashStrength = FMath::Max(CVarFlashStrength.GetValueOnRenderThread(), 0.0f);
	const bool bFireFlash = FlashMode == 0;
	Base.FlashGain = bFireFlash ? FlashGainCfg * FlashStrength : 0.0f;
	Base.FlashDT = bFireFlash ? FlashDTCfg * FMath::Sqrt(FlashStrength) : 0.0f;
	const bool bDetail = Worley.IsValid() && DetailErodeCfg > 0.0f;
	Base.DetailErode = DetailErodeCfg;
	Base.DetailTemp = DetailTempCfg;
	const float PixelsPerMeterAt1M = OutRectHeightForDetail(View, SceneColor.ViewRect);
	static const int32 Debug = int32(ParamOr(TEXT("RmDebug="), 0.0f));
	Base.DebugView = Debug;
	Base.NearM = NearM;
	Base.Jitter = float(FrameCounter++ % 1024);

	// as explosões: caixa justa, passos pelo tamanho na tela, cache de luz pras maiores
	struct FVis { int32 Index; float Frac; };
	float MaxFlash = 0.0f;
	TArray<FVis> Vis;
	int32 N = 0;
	FRHITexture* WarpTex[MaxWarps] = {};
	int32 NumWarps = 0;
	for (int32 i = 0; i < Instances.Num() && N < MaxInstances; ++i)
	{
		const FInstance& I = Instances[i];
		const FBakeData& B = *I.Bake;
		const int32 F0 = FMath::Clamp(FMath::FloorToInt32(I.Frame), 0, B.Frames - 1);
		const int32 F1 = FMath::Min(F0 + 1, B.Frames - 1);
		const float A = FMath::Clamp(I.Frame - F0, 0.0f, 1.0f);
		// a caixa dos dois quadros, alargada pelo deslocamento do ruído (instances.js _box)
		FBox3f Local(ForceInit);
		for (const int32 F : { F0, F1 })
		{
			if (B.Bounds[F].Max.X > B.Bounds[F].Min.X)
			{
				Local += B.Bounds[F];
			}
		}
		if (!Local.IsValid)
		{
			continue;
		}
		const FVector3f DMin(-0.5f * B.Domain, -0.5f * B.Domain, 0.0f), DMax(0.5f * B.Domain, 0.5f * B.Domain, B.Domain);
		// deformada: a fumaça empurrada sai da caixa gravada até o |D| máximo
		int32 WarpSlot = -1;
		if (I.WarpRHI.IsValid() && NumWarps < MaxWarps)
		{
			WarpSlot = NumWarps;
			WarpTex[NumWarps++] = I.WarpRHI.GetReference();
		}
		const float Pad = DetailAmp + Base.DetailAmp2 + 2.0f * B.Domain / B.Res + (WarpSlot >= 0 ? WarpMaxLocalM : 0.0f);
		Local.Min = FVector3f::Max(DMin, Local.Min - Pad);
		Local.Max = FVector3f::Min(DMax, Local.Max + Pad);
		const FBox World(I.FootCm + FVector(Local.Min) * (I.Scale * 100.0), I.FootCm + FVector(Local.Max) * (I.Scale * 100.0));
		const float Frac = ScreenFraction(View, World);
		if (Frac <= 0.0f)
		{
			continue;
		}
		const float Lod = FMath::Sqrt(FMath::Min(Frac / RefArea, 1.0f));
		const int32 S = FMath::Clamp(FMath::Max(StepsMin, FMath::RoundToInt32(StepsCfg * Lod)), 1, MaxShaderSteps);
		// a luz gravada (instances.js lightOf): posição × escala, cor × escala²
		const FVector3f LP = FMath::Lerp(B.LightPos[F0], B.LightPos[F1], A);
		const FVector3f LC = FMath::Lerp(B.LightCol[F0], B.LightCol[F1], A) * (I.Scale * I.Scale);
		const FVector3f Foot = Rel(I.FootCm);
		Base.InstFoot[N] = FVector4f(Foot, I.Scale);
		Base.InstFrame[N] = FVector4f(float(I.Slot0), float(I.Slot1), A,
			(B.TimeOfFrame(float(F1)) - B.TimeOfFrame(float(F0))) * (B.Res / B.Domain) * MvScale);
		Base.InstMisc[N] = FVector4f(B.TimeOfFrame(I.Frame), I.Seed, I.Keep, float(S));
		// A nuvem nuclear quase não tem fuligem: logo depois da bola de fogo ela é
		// marrom-avermelhada (óxidos de nitrogênio do ar quente) e, quando esfria e o
		// vapor condensa, fica branca (Glasstone & Dolan 1977, cap. II). A emissão é
		// ∝ σ·(1−albedo) (Kirchhoff): o ganho compensa pra bola de fogo brilhar igual.
		float Albedo = SootAlbedo, EmitGain = 1.0f;
		FVector3f Look = SootColor;
		if (I.bNuclear)
		{
			const float K = FMath::Clamp((B.TimeOfFrame(I.Frame) - 1.0f) / 1.8f, 0.0f, 1.0f), Wn = K * K * (3.0f - 2.0f * K);
			Look = FMath::Lerp(FVector3f(0.66f, 0.46f, 0.34f), FVector3f(0.80f, 0.79f, 0.77f), Wn);
			Albedo = FMath::Lerp(0.48f, 0.70f, Wn);
			EmitGain = (1.0f - SootAlbedo) / (1.0f - Albedo);
		}
		Base.InstLook[N] = FVector4f(Look, Albedo);
		Base.InstRes[N] = FVector4f(float(B.Res), float(B.FuelRes), float(B.MacroRes), B.Domain);
		// cada oitava das bolhas entra quando o voxel da gravação (0.4 m na Carga
		// Pesada) cobre pixels bastante pra ela aparecer sem chuviscar na marcha de
		// meia resolução: 1.2 m a partir de ~3 px por voxel, 0.6 m de ~5, 0.3 m de
		// ~11. Da câmera do RTS (< 2.5 px) fica desligado e não custa nada.
		// Segunda opinião de 08/10 (agente), números do Nubis (Schneider 2015/2023).
		if (bDetail)
		{
			const double DistM = FMath::Max(1.0, FMath::Sqrt(World.ComputeSquaredDistanceToPoint(CamCm)) * 0.01);
			const float Px = float(B.Domain / B.Res * I.Scale * PixelsPerMeterAt1M / DistM);
			const float Hot = 1.0f - FMath::SmoothStep(2.0f, 4.0f, B.TimeOfFrame(I.Frame));
			Base.InstDetail[N] = FVector4f(FMath::SmoothStep(2.7f, 5.3f, Px), FMath::SmoothStep(5.3f, 10.7f, Px), FMath::SmoothStep(10.7f, 21.0f, Px), Hot);
		}
		Base.InstLight[N] = FVector4f(-1.0f, I.Thickness, EmitGain, float(WarpSlot));
		Base.InstBoxMin[N] = FVector4f(Rel(World.Min), 0.0f);
		Base.InstBoxMax[N] = FVector4f(Rel(World.Max), 0.0f);
		Base.InstFirePos[N] = FVector4f(Foot + LP * I.Scale, I.Flash);
		MaxFlash = FMath::Max(MaxFlash, I.Flash);
		Base.InstFireCol[N] = FVector4f(LC, 0.0f);
		Vis.Add({ N, Frac });
		++N;
	}
	if (!N)
	{
		return SceneColor;
	}
	Base.NumInstances = N;
	Vis.Sort([](const FVis& A, const FVis& B) { return A.Frac > B.Frac; });
	const int32 NumLC = FMath::Min(Vis.Num(), LightCachesCfg);
	for (int32 k = 0; k < NumLC; ++k)
	{
		Base.InstLight[Vis[k].Index].X = float(k);
	}

	// a tela: raio sem o jitter do TSR (a imagem aqui já é a reconstruída)
	const FMatrix InvProj = View.ViewMatrices.GetViewToClipNoAA().Inverse();
	const FMatrix ViewRot = View.ViewMatrices.GetWorldToView().RemoveTranslation().GetTransposed();
	Base.ClipToCamera = FMatrix44f(InvProj * ViewRot);
	Base.CameraForward = FVector3f(View.GetViewDirection());
	const FIntRect OutRect = SceneColor.ViewRect;
	const FIntPoint Half(FMath::Max(1, FMath::CeilToInt32(OutRect.Width() * MarchRes)), FMath::Max(1, FMath::CeilToInt32(OutRect.Height() * MarchRes)));
	Base.HalfSize = FUintVector2(Half.X, Half.Y);
	Base.OutMin = FUintVector2(OutRect.Min.X, OutRect.Min.Y);
	Base.OutSize = FUintVector2(OutRect.Width(), OutRect.Height());
	const FIntPoint Extent = SceneColor.Texture->Desc.Extent;
	Base.OutExtent = FUintVector2(Extent.X, Extent.Y);
	Base.LinearClamp = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Base.LinearWrap = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	FRDGTextureRef PoolPTT = External(GraphBuilder, PoolPT.GetReference(), TEXT("BlastRaymarch.PoolPT"));
	FRDGTextureRef AtlasAT = External(GraphBuilder, AtlasA.GetReference(), TEXT("BlastRaymarch.AtlasA"));
	FRDGTextureRef AtlasFT = External(GraphBuilder, AtlasF.GetReference(), TEXT("BlastRaymarch.AtlasF"));
	// os blocos dos quadros que acabaram de subir, antes de qualquer leitura
	ScatterPending(GraphBuilder, View, AtlasAT, AtlasFT);
	FRDGTextureRef PoolMT = External(GraphBuilder, PoolM.GetReference(), TEXT("BlastRaymarch.PoolM"));
	FRDGTextureRef NoiseT = External(GraphBuilder, Noise.GetReference(), TEXT("BlastRaymarch.Noise"));
	FRDGTextureRef BlackBodyT = External(GraphBuilder, BlackBody.GetReference(), TEXT("BlastRaymarch.BlackBody"));
	FRDGTextureRef WorleyT = Worley.IsValid() ? External(GraphBuilder, Worley.GetReference(), TEXT("BlastRaymarch.Worley")) : NoiseT;
	// as que não têm deformação nunca leem o slot: o ruído fica no lugar
	FRDGTextureRef WarpT[MaxWarps];
	for (int32 w = 0; w < MaxWarps; ++w)
	{
		WarpT[w] = WarpTex[w] ? External(GraphBuilder, WarpTex[w], TEXT("BlastRaymarch.Warp")) : NoiseT;
	}
	FRDGTextureRef LC = GraphBuilder.CreateTexture(FRDGTextureDesc::Create3D(FIntVector(LCRes, LCRes, LCRes * MaxLightCaches), PF_G16R16F,
		FClearValueBinding::White, TexCreate_ShaderResource | TexCreate_UAV), TEXT("BlastRaymarch.LightCache"));
	auto Common = [&](FBlastRaymarchParameters* P)
	{
		*P = Base;
		P->PoolPT = PoolPTT;
		P->AtlasA = AtlasAT;
		P->AtlasF = AtlasFT;
		P->PoolM = PoolMT;
		P->Noise = NoiseT;
		P->BlackBody = BlackBodyT;
		P->Worley = WorleyT;
		P->Warp0 = WarpT[0];
		P->Warp1 = WarpT[1];
		P->Warp2 = WarpT[2];
		P->Warp3 = WarpT[3];
		P->SceneDepth = Depth;
	};

	// 1) cache de luz das maiores na tela
	if (NumLC == 0)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LC), FVector4f(1.0f, 1.0f, 1.0f, 1.0f));
	}
	for (int32 k = 0; k < NumLC; ++k)
	{
		FBlastRaymarchParameters* P = GraphBuilder.AllocParameters<FBlastRaymarchParameters>();
		Common(P);
		P->LCInstance = Vis[k].Index;
		P->LCSlot = k;
		P->PoolLCOut = GraphBuilder.CreateUAV(LC);
		TShaderMapRef<FBlastRaymarchLightCacheCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch LightCache"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			Shader, P, FComputeShaderUtils::GetGroupCount(FIntVector(LCRes), FIntVector(4)));
	}

	// 2) a marcha, meia resolução
	FRDGTextureRef Vol = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Half, PF_FloatRGBA, FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV), TEXT("BlastRaymarch.Volume"));
	FRDGTextureRef VolDepth = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Half, PF_G32R32F, FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV), TEXT("BlastRaymarch.VolumeDepth"));
	{
		FBlastRaymarchParameters* P = GraphBuilder.AllocParameters<FBlastRaymarchParameters>();
		Common(P);
		P->PoolLC = LC;
		P->VolOut = GraphBuilder.CreateUAV(Vol);
		P->VolDepthOut = GraphBuilder.CreateUAV(VolDepth);
		TShaderMapRef<FBlastRaymarchMarchCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch March %dx%d", Half.X, Half.Y), Shader, P,
			FComputeShaderUtils::GetGroupCount(Half, FIntPoint(8, 8)));
	}

	// 2b) acúmulo temporal, levado pela câmera do quadro anterior
	const FMatrix WorldToClip = View.ViewMatrices.GetWorldToView() * View.ViewMatrices.GetViewToClipNoAA();
	const uint32 Frame = View.Family ? View.Family->FrameNumber : 0;
	const bool bHistory = HistAlpha < 1.0f && History.IsValid() && HistorySize == Half && Frame == HistoryFrame + 1;
	FRDGTextureRef Resolved = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(Half, PF_FloatRGBA, FClearValueBinding::Black,
		TexCreate_ShaderResource | TexCreate_UAV), TEXT("BlastRaymarch.Resolved"));
	{
		FBlastRaymarchParameters* P = GraphBuilder.AllocParameters<FBlastRaymarchParameters>();
		Common(P);
		P->VolIn = Vol;
		P->VolDepthIn = VolDepth;
		P->HistIn = bHistory ? GraphBuilder.RegisterExternalTexture(History, TEXT("BlastRaymarch.History")) : Vol;
		P->HistOut = GraphBuilder.CreateUAV(Resolved);
		// metros relativos à câmera de agora → mundo (cm) → clip do quadro anterior (em double, depois float)
		P->CamToPrevClip = FMatrix44f(FScaleMatrix(100.0) * FTranslationMatrix(CamCm) * PrevWorldToClip);
		P->HistValid = bHistory ? 1.0f : 0.0f;
		// no clarão o quadro novo pesa mais: o acúmulo atrasaria e achataria o pico (revisor de 10/10);
		// pela metade, a granulação da marcha não aparece nas outras explosões durante os ~0.1 s
		P->HistAlpha = FMath::Max(HistAlpha, FMath::Min(0.5f * MaxFlash * (Base.FlashGain > 0.0f ? 1.0f : 0.0f), 1.0f));
		TShaderMapRef<FBlastRaymarchTemporalCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch Temporal"), Shader, P,
			FComputeShaderUtils::GetGroupCount(Half, FIntPoint(8, 8)));
	}
	GraphBuilder.QueueTextureExtraction(Resolved, &History);
	HistorySize = Half;
	HistoryFrame = Frame;
	PrevWorldToClip = WorldToClip;

	// 3) composição na cena, numa textura nova (a de entrada pode ser a história do TSR)
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	FRDGTextureRef OutTex = Output.IsValid() ? Output.Texture : nullptr;
	if (!OutTex || !EnumHasAnyFlags(OutTex->Desc.Flags, TexCreate_UAV))
	{
		FRDGTextureDesc Desc = SceneColor.Texture->Desc;
		Desc.Flags = (Desc.Flags | TexCreate_UAV | TexCreate_ShaderResource) & ~(TexCreate_FastVRAM | TexCreate_DepthStencilTargetable);
		OutTex = GraphBuilder.CreateTexture(Desc, TEXT("BlastRaymarch.SceneColor"));
	}
	{
		FBlastRaymarchParameters* P = GraphBuilder.AllocParameters<FBlastRaymarchParameters>();
		Common(P);
		P->SceneColorIn = SceneColor.Texture;
		P->VolIn = Resolved;
		P->VolDepthIn = VolDepth;
		P->SceneColorOut = GraphBuilder.CreateUAV(OutTex);
		TShaderMapRef<FBlastRaymarchCompositeCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlastRaymarch Composite"), Shader, P,
			FComputeShaderUtils::GetGroupCount(Extent, FIntPoint(8, 8)));
	}
	if (Output.IsValid() && Output.Texture != OutTex)
	{
		AddCopyTexturePass(GraphBuilder, OutTex, Output.Texture);
		return FScreenPassTexture(Output);
	}
	return FScreenPassTexture(OutTex, OutRect);
}
