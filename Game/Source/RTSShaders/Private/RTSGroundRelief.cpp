#include "RTSGroundRelief.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "SystemTextures.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

BEGIN_SHADER_PARAMETER_STRUCT(FRTSReliefShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProfileTex)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CraterTex)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InTex)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTex)
	SHADER_PARAMETER(FIntPoint, Origin)
	SHADER_PARAMETER(FIntPoint, Size)
	SHADER_PARAMETER(FVector2f, Texel)
	SHADER_PARAMETER(float, TexelCm)
	SHADER_PARAMETER(float, RangeCm)
	SHADER_PARAMETER(float, GrooveCm)
	SHADER_PARAMETER(float, BermCm)
	SHADER_PARAMETER(float, BermWidthCm)
	SHADER_PARAMETER(float, BandScale)
	SHADER_PARAMETER(float, BermScale)
	SHADER_PARAMETER(float, HaloCm)
	SHADER_PARAMETER(FVector2f, WorldMinCm)
	SHADER_PARAMETER(FVector2f, SunDir)
	SHADER_PARAMETER(float, SunTan)
	SHADER_PARAMETER(float, PenumbraCm)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SurfaceTex)
	SHADER_PARAMETER(FVector2f, SurfaceMinCm)
	SHADER_PARAMETER(float, SurfaceInvSizeCm)
	SHADER_PARAMETER(int32, UseSurface)
	SHADER_PARAMETER(FVector4f, SurfDepth)
	SHADER_PARAMETER(FVector4f, SurfBerm)
	SHADER_PARAMETER(FVector4f, SurfClaw)
	SHADER_PARAMETER(FVector3f, SurfWet)
END_SHADER_PARAMETER_STRUCT()

// as marcas (GroundCrater.usf): os campos comuns às duas passadas
BEGIN_SHADER_PARAMETER_STRUCT(FRTSMarkCommonParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasTex)
	SHADER_PARAMETER_SAMPLER(SamplerState, AtlasSampler)
	SHADER_PARAMETER(FIntPoint, Origin)
	SHADER_PARAMETER(FIntPoint, Size)
	SHADER_PARAMETER(FVector2f, SheetMinCm)
	SHADER_PARAMETER(float, SheetTexelCm)
	SHADER_PARAMETER(FVector2f, CenterCm)
	SHADER_PARAMETER(FVector2f, Axis)
	SHADER_PARAMETER(float, Scale)
	SHADER_PARAMETER(float, Stretch)
	SHADER_PARAMETER(int32, RampMode)
	SHADER_PARAMETER(float, Mirror)
	SHADER_PARAMETER(float, HeightMul)
	SHADER_PARAMETER(float, DarkMul)
	SHADER_PARAMETER(float, HeatMul)
	SHADER_PARAMETER(float, LightMul)
	SHADER_PARAMETER(float, Variant)
	SHADER_PARAMETER(float, Variants)
	SHADER_PARAMETER(float, SpanCm)
	SHADER_PARAMETER(float, AtlasTexels)
	SHADER_PARAMETER(float, AtlasRangeCm)
	SHADER_PARAMETER(int32, Taps)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRTSMarkTrackParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FRTSMarkCommonParameters, Common)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, TargetTex)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRTSMarkHeightParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FRTSMarkCommonParameters, Common)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SunTex)
	SHADER_PARAMETER(int32, UseSun)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, CraterOut)
END_SHADER_PARAMETER_STRUCT()

#define RTS_RELIEF_SHADER(Name, Entry)                                                                \
	class Name : public FGlobalShader                                                                 \
	{                                                                                                 \
	public:                                                                                           \
		DECLARE_GLOBAL_SHADER(Name);                                                                  \
		using FParameters = FRTSReliefShaderParameters;                                               \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)    \
		{                                                                                             \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);              \
		}                                                                                             \
	};                                                                                                \
	IMPLEMENT_GLOBAL_SHADER(Name, "/RTSGame/Private/GroundRelief.usf", Entry, SF_Compute);

RTS_RELIEF_SHADER(FRTSReliefHeightCS, "HeightCS")
RTS_RELIEF_SHADER(FRTSReliefSunCS, "SunCS")

#define RTS_MARK_SHADER(Name, Params, Entry)                                                          \
	class Name : public FGlobalShader                                                                 \
	{                                                                                                 \
	public:                                                                                           \
		DECLARE_GLOBAL_SHADER(Name);                                                                  \
		using FParameters = Params;                                                                   \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                             \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)    \
		{                                                                                             \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);              \
		}                                                                                             \
		/* as marcas leem e escrevem o caderno (RGBA8, R16F): leitura tipada de UAV, que o D3D11 só */ \
		/* aceita com a opção; o pacote cozinha pro SM5 também (a 1660 e o D3D12 têm) */              \
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment) \
		{                                                                                             \
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);                  \
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);                               \
		}                                                                                             \
	};                                                                                                \
	IMPLEMENT_GLOBAL_SHADER(Name, "/RTSGame/Private/GroundCrater.usf", Entry, SF_Compute);

RTS_MARK_SHADER(FRTSMarkTrackCS, FRTSMarkTrackParameters, "MarkCS")
RTS_MARK_SHADER(FRTSMarkHeightCS, FRTSMarkHeightParameters, "HeightCS")

// a luz do sol assada (16/10, GroundSunLit.usf): a varredura na grade fina e o resolve no caderno de luz
BEGIN_SHADER_PARAMETER_STRUCT(FRTSSunLitCommon, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReliefTex)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CraterTex)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
	SHADER_PARAMETER(float, RangeCm)
	SHADER_PARAMETER(FVector2f, SunDir)
	SHADER_PARAMETER(float, SunTan)
	SHADER_PARAMETER(float, SunSin)
	SHADER_PARAMETER(float, SunCos)
	SHADER_PARAMETER(float, PenumbraCm)
	SHADER_PARAMETER(float, MaxLit)
	// a garra (16/10): o perfil (R a faixa, B o frescor) e o andado da lagarta
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProfileTex)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AlongTex)
	SHADER_PARAMETER(int32, UseClaw)
	SHADER_PARAMETER(float, ClawCm)
	SHADER_PARAMETER(float, ClawPitchCm)
	SHADER_PARAMETER(float, AlongRangeCm)
	SHADER_PARAMETER(float, BandScale)
	SHADER_PARAMETER(FVector2f, SheetMinCm)
	SHADER_PARAMETER(float, SheetSizeCmF)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SurfaceTex)
	SHADER_PARAMETER(FVector2f, SurfaceMinCm)
	SHADER_PARAMETER(float, SurfaceInvSizeCm)
	SHADER_PARAMETER(int32, UseSurface)
	SHADER_PARAMETER(FVector4f, SurfDepth)
	SHADER_PARAMETER(FVector4f, SurfBerm)
	SHADER_PARAMETER(FVector4f, SurfClaw)
	SHADER_PARAMETER(FVector3f, SurfWet)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRTSSunLitSweepParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FRTSSunLitCommon, Common)
	SHADER_PARAMETER(FVector2f, FineTexelUV)
	SHADER_PARAMETER(float, FineTexelCm)
	SHADER_PARAMETER(FIntPoint, GridOrigin)
	SHADER_PARAMETER(FIntPoint, GridSize)
	SHADER_PARAMETER(FIntPoint, WriteMin)
	SHADER_PARAMETER(FIntPoint, WriteMax)
	SHADER_PARAMETER(int32, MajorY)
	SHADER_PARAMETER(int32, StepSign)
	SHADER_PARAMETER(float, Slope)
	SHADER_PARAMETER(int32, LineBase)
	SHADER_PARAMETER(int32, LineCount)
	SHADER_PARAMETER(float, StepCm)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, FineOut)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRTSSunLitResolveParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FRTSSunLitCommon, Common)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, FineIn)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, SunLitOut)
	SHADER_PARAMETER(int32, SS)
	SHADER_PARAMETER(FIntPoint, BaseMin)
	SHADER_PARAMETER(FIntPoint, BaseSize)
	SHADER_PARAMETER(FVector2f, BaseTexelUV)
	SHADER_PARAMETER(float, BaseTexelCm)
	SHADER_PARAMETER(float, AORadiusCm)
END_SHADER_PARAMETER_STRUCT()

#define RTS_SUNLIT_SHADER(Name, Params, Entry)                                                    \
	class Name : public FGlobalShader                                                                \
	{                                                                                                \
	public:                                                                                          \
		DECLARE_GLOBAL_SHADER(Name);                                                                    \
		using FParameters = Params;                                                                     \
		SHADER_USE_PARAMETER_STRUCT(Name, FGlobalShader);                                               \
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)      \
		{                                                                                               \
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);                    \
		}                                                                                               \
	};                                                                                               \
	IMPLEMENT_GLOBAL_SHADER(Name, "/RTSGame/Private/GroundSunLit.usf", Entry, SF_Compute);

RTS_SUNLIT_SHADER(FRTSSunLitSweepCS, FRTSSunLitSweepParameters, "SweepCS")
RTS_SUNLIT_SHADER(FRTSSunLitResolveCS, FRTSSunLitResolveParameters, "ResolveCS")

// as mips dos cadernos (16/10): a média de 2 × 2 da mip de cima
BEGIN_SHADER_PARAMETER_STRUCT(FRTSGroundMipParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, SrcMip)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DstMip)
	SHADER_PARAMETER(FIntPoint, DstMin)
	SHADER_PARAMETER(FIntPoint, DstSize)
	SHADER_PARAMETER(FIntPoint, SrcSize)
END_SHADER_PARAMETER_STRUCT()

class FRTSGroundMipCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSGroundMipCS);
	using FParameters = FRTSGroundMipParameters;
	SHADER_USE_PARAMETER_STRUCT(FRTSGroundMipCS, FGlobalShader);
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRTSGroundMipCS, "/RTSGame/Private/GroundMips.usf", "DownCS", SF_Compute);

// (16/10) a batalha no minimapa: a camada pintada do caderno de acúmulo
BEGIN_SHADER_PARAMETER_STRUCT(FRTSBattleOverlayParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, BattleTex)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTex)
	SHADER_PARAMETER(FIntPoint, OutSize)
	SHADER_PARAMETER(FIntPoint, InSize)
END_SHADER_PARAMETER_STRUCT()

class FRTSBattleOverlayCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSBattleOverlayCS);
	using FParameters = FRTSBattleOverlayParameters;
	SHADER_USE_PARAMETER_STRUCT(FRTSBattleOverlayCS, FGlobalShader);
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRTSBattleOverlayCS, "/RTSGame/Private/GroundBattle.usf", "OverlayCS", SF_Compute);

void RTSGroundRelief::BattleOverlay(UTextureRenderTarget2D* Battle, UTextureRenderTarget2D* Out)
{
	FTextureRenderTargetResource* InRes = Battle ? Battle->GameThread_GetRenderTargetResource() : nullptr;
	FTextureRenderTargetResource* OutRes = Out ? Out->GameThread_GetRenderTargetResource() : nullptr;
	if (!InRes || !OutRes)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSGroundBattle)([InRes, OutRes](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* InRHI = InRes->GetRenderTargetTexture();
		FRHITexture* OutRHI = OutRes->GetRenderTargetTexture();
		if (!InRHI || !OutRHI)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RTSGroundBattle"));
		FRDGTextureRef InT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InRHI, TEXT("RTSGround.Battle")));
		FRDGTextureRef OutT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutRHI, TEXT("RTSGround.BattleOverlay")));
		FRTSBattleOverlayParameters* P = GraphBuilder.AllocParameters<FRTSBattleOverlayParameters>();
		P->BattleTex = InT;
		P->OutTex = GraphBuilder.CreateUAV(OutT);
		P->OutSize = OutT->Desc.Extent;
		P->InSize = InT->Desc.Extent;
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround BattleOverlay"), TShaderMapRef<FRTSBattleOverlayCS>(GetGlobalShaderMap(GMaxRHIFeatureLevel)), P,
			FIntVector(FMath::DivideAndRoundUp(P->OutSize.X, 8), FMath::DivideAndRoundUp(P->OutSize.Y, 8), 1));
		GraphBuilder.Execute();
	});
}

void RTSGroundRelief::BuildMips(UTextureRenderTarget2D* Target, const FIntRect& InRect, int32 MaxMips)
{
	if (!Target || InRect.Width() <= 0 || InRect.Height() <= 0 || MaxMips <= 0)
	{
		return;
	}
	FTextureRenderTargetResource* Res = Target->GameThread_GetRenderTargetResource();
	if (!Res)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSGroundMips)([Res, InRect, MaxMips](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* RHI = Res->GetRenderTargetTexture();
		if (!RHI || RHI->GetDesc().NumMips < 2)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RTSGroundMips"));
		FRDGTextureRef T = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHI, TEXT("RTSGround.Mips")));
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const FIntPoint Extent = T->Desc.Extent;
		const int32 Last = FMath::Min(MaxMips, int32(T->Desc.NumMips) - 1);
		FIntRect R = InRect;
		R.Clip(FIntRect(0, 0, Extent.X, Extent.Y));
		for (int32 Mip = 1; Mip <= Last && R.Width() > 0 && R.Height() > 0; ++Mip)
		{
			const FIntPoint Src(FMath::Max(Extent.X >> (Mip - 1), 1), FMath::Max(Extent.Y >> (Mip - 1), 1));
			const FIntPoint Dst(FMath::Max(Extent.X >> Mip, 1), FMath::Max(Extent.Y >> Mip, 1));
			// o retângulo desta mip: a metade da de cima, arredondada pra fora
			R = FIntRect(FIntPoint(R.Min.X / 2, R.Min.Y / 2), FIntPoint((R.Max.X + 1) / 2, (R.Max.Y + 1) / 2));
			R.Clip(FIntRect(0, 0, Dst.X, Dst.Y));
			if (R.Width() <= 0 || R.Height() <= 0)
			{
				break;
			}
			FRTSGroundMipParameters* P = GraphBuilder.AllocParameters<FRTSGroundMipParameters>();
			P->SrcMip = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(T, Mip - 1));
			P->DstMip = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(T, uint8(Mip)));
			P->DstMin = R.Min;
			P->DstSize = R.Size();
			P->SrcSize = Src;
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Mip"), TShaderMapRef<FRTSGroundMipCS>(ShaderMap), P,
				FIntVector(FMath::DivideAndRoundUp(R.Width(), 8), FMath::DivideAndRoundUp(R.Height(), 8), 1));
		}
		GraphBuilder.Execute();
	});
}

void RTSGroundRelief::Compute(UTextureRenderTarget2D* Profile, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* Relief, const FRTSReliefParams& Params, const FIntRect& Rect)
{
	// (14/10) a largura e a altura, não a área: as duas negativas davam área positiva e o despacho de bilhões de grupos
	// que travou a placa no jogo do Felipe
	if (!Profile || !Crater || !Relief || Rect.Width() <= 0 || Rect.Height() <= 0)
	{
		return;
	}
	FTextureRenderTargetResource* ProfileRes = Profile->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* CraterRes = Crater->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* ReliefRes = Relief->GameThread_GetRenderTargetResource();
	if (!ProfileRes || !CraterRes || !ReliefRes)
	{
		return;
	}
	const FIntPoint Extent(Relief->SizeX, Relief->SizeY);
	// a 2ª passada lê os vizinhos da 1ª rumo ao sol (3,5 texels) e a meio texel do lado: a 1ª cobre o
	// retângulo com essa margem, a 2ª só o retângulo
	const int32 SunMargin = 5;
	const auto Grow = [&Extent](const FIntRect& R, int32 M)
	{
		return FIntRect(FMath::Max(R.Min.X - M, 0), FMath::Max(R.Min.Y - M, 0), FMath::Min(R.Max.X + M, Extent.X), FMath::Min(R.Max.Y + M, Extent.Y));
	};
	const FIntRect R2 = Grow(Rect, 0);
	const FIntRect R1 = Grow(Rect, SunMargin);
	if (R2.Width() <= 0 || R2.Height() <= 0)
	{
		return;   // o retângulo fora do caderno
	}
	FTextureResource* SurfaceRes = Params.Surface ? Params.Surface->GetResource() : nullptr;
	ENQUEUE_RENDER_COMMAND(RTSGroundRelief)([ProfileRes, CraterRes, ReliefRes, SurfaceRes, Params, Extent, R1, R2](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* ProfileRHI = ProfileRes->GetRenderTargetTexture();
		FRHITexture* CraterRHI = CraterRes->GetRenderTargetTexture();
		FRHITexture* ReliefRHI = ReliefRes->GetRenderTargetTexture();
		if (!ProfileRHI || !CraterRHI || !ReliefRHI)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RTSGroundRelief"));   // (o nome aparece nas migalhas de um travamento)
		FRDGTextureRef ProfileT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ProfileRHI, TEXT("RTSGround.Profile")));
		FRDGTextureRef CraterT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterRHI, TEXT("RTSGround.Crater")));
		FRDGTextureRef ReliefT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ReliefRHI, TEXT("RTSGround.Relief")));
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTextureRef HeightT = GraphBuilder.CreateTexture(Desc, TEXT("RTSGround.Height"));
		FRHITexture* SurfaceRHI = SurfaceRes ? SurfaceRes->TextureRHI.GetReference() : nullptr;
		const bool bSurface = SurfaceRHI != nullptr;
		FRDGTextureRef SurfaceT = bSurface ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SurfaceRHI, TEXT("RTSGround.Surface")))
			: GSystemTextures.GetBlackDummy(GraphBuilder);
		FRHISamplerState* Linear = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const auto Fill = [&](FRTSReliefShaderParameters* P, FRDGTextureRef In, FRDGTextureRef Out, const FIntRect& R)
		{
			P->ProfileTex = ProfileT;
			P->CraterTex = CraterT;
			P->InTex = In;
			P->LinearSampler = Linear;
			P->OutTex = GraphBuilder.CreateUAV(Out);
			P->Origin = R.Min;
			P->Size = R.Size();
			P->Texel = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);
			P->TexelCm = Params.TexelCm;
			P->RangeCm = Params.RangeCm;
			P->GrooveCm = Params.GrooveCm;
			P->BermCm = Params.BermCm;
			P->BermWidthCm = Params.BermWidthCm;
			P->BandScale = Params.BandScale;
			P->BermScale = Params.BermScale;
			P->HaloCm = Params.HaloCm;
			P->WorldMinCm = Params.WorldMinCm;
			P->SunDir = Params.SunDir;
			P->SunTan = Params.SunTan;
			P->PenumbraCm = Params.PenumbraCm;
			P->SurfaceTex = SurfaceT;
			P->SurfaceMinCm = Params.SurfaceMinCm;
			P->SurfaceInvSizeCm = 1.0f / FMath::Max(Params.SurfaceSizeCm, 1.0f);
			P->UseSurface = bSurface ? 1 : 0;
			P->SurfDepth = Params.SurfDepth;
			P->SurfBerm = Params.SurfBerm;
			P->SurfClaw = Params.SurfClaw;
			P->SurfWet = Params.SurfWet;
		};
		const auto Groups = [](const FIntRect& R) { return FIntVector(FMath::DivideAndRoundUp(R.Width(), 8), FMath::DivideAndRoundUp(R.Height(), 8), 1); };
		{
			FRTSReliefShaderParameters* P = GraphBuilder.AllocParameters<FRTSReliefShaderParameters>();
			Fill(P, ProfileT, HeightT, R1);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Height"), TShaderMapRef<FRTSReliefHeightCS>(ShaderMap), P, Groups(R1));
		}
		{
			FRTSReliefShaderParameters* P = GraphBuilder.AllocParameters<FRTSReliefShaderParameters>();
			Fill(P, HeightT, ReliefT, R2);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Sun"), TShaderMapRef<FRTSReliefSunCS>(ShaderMap), P, Groups(R2));
		}
		GraphBuilder.Execute();
	});
}

void RTSGroundRelief::StampMarks(UTextureRenderTarget2D* Track, UTextureRenderTarget2D* Crater, const FVector2D& SheetMinCm, const FVector2D& SheetSizeCm,
	const FRTSGroundMarkAtlas& CraterAtlas, const FRTSGroundMarkAtlas& WreckAtlas, const TArray<FRTSGroundMarkParams>& Marks)
{
	if (!Track || !Crater || Marks.IsEmpty())
	{
		return;
	}
	FTextureRenderTargetResource* TrackRes = Track->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* CraterRes = Crater->GameThread_GetRenderTargetResource();
	FTextureResource* CraterAtlasRes = CraterAtlas.Texture ? CraterAtlas.Texture->GetResource() : nullptr;
	FTextureResource* WreckAtlasRes = WreckAtlas.Texture ? WreckAtlas.Texture->GetResource() : nullptr;
	FTextureResource* CraterSunRes = CraterAtlas.SunTexture ? CraterAtlas.SunTexture->GetResource() : nullptr;
	FTextureResource* WreckSunRes = WreckAtlas.SunTexture ? WreckAtlas.SunTexture->GetResource() : nullptr;
	if (!TrackRes || !CraterRes)
	{
		return;
	}
	const FIntPoint TrackExtent(Track->SizeX, Track->SizeY);
	const FIntPoint CraterExtent(Crater->SizeX, Crater->SizeY);
	struct FJob
	{
		FRTSGroundMarkParams M;
		FIntRect TrackRect, CraterRect;
	};
	TArray<FJob> Jobs;
	for (const FRTSGroundMarkParams& M : Marks)
	{
		const FRTSGroundMarkAtlas& A = M.bWreck ? WreckAtlas : CraterAtlas;
		if (!(M.bWreck ? WreckAtlasRes : CraterAtlasRes))
		{
			continue;
		}
		// o retângulo do carimbo girado, em texels de cada caderno
		const double Half = A.SpanCm * 0.5 * M.Scale * FMath::Max(M.Stretch, 1.0f) * (FMath::Abs(M.Axis.X) + FMath::Abs(M.Axis.Y));
		const auto RectFor = [&](const FIntPoint& E)
		{
			const FVector2D S(E.X / SheetSizeCm.X, E.Y / SheetSizeCm.Y);
			const FVector2D C = (FVector2D(M.CenterCm) - SheetMinCm) * S;
			FIntRect R(FMath::FloorToInt(C.X - Half * S.X) - 1, FMath::FloorToInt(C.Y - Half * S.Y) - 1, FMath::CeilToInt(C.X + Half * S.X) + 1, FMath::CeilToInt(C.Y + Half * S.Y) + 1);
			R.Clip(FIntRect(0, 0, E.X, E.Y));
			return R;
		};
		FJob J{ M, RectFor(TrackExtent), RectFor(CraterExtent) };
		if ((J.TrackRect.Width() > 0 && J.TrackRect.Height() > 0) || (J.CraterRect.Width() > 0 && J.CraterRect.Height() > 0))
		{
			Jobs.Add(J);
		}
	}
	if (Jobs.IsEmpty())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSGroundMarks)([TrackRes, CraterRes, CraterAtlasRes, WreckAtlasRes, CraterSunRes, WreckSunRes, CraterAtlas, WreckAtlas, Jobs, SheetMinCm, SheetSizeCm, TrackExtent, CraterExtent](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* TrackRHI = TrackRes->GetRenderTargetTexture();
		FRHITexture* CraterRHI = CraterRes->GetRenderTargetTexture();
		FRHITexture* CraterAtlasRHI = CraterAtlasRes ? CraterAtlasRes->TextureRHI.GetReference() : nullptr;
		FRHITexture* WreckAtlasRHI = WreckAtlasRes ? WreckAtlasRes->TextureRHI.GetReference() : nullptr;
		if (!TrackRHI || !CraterRHI || (!CraterAtlasRHI && !WreckAtlasRHI))
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RTSGroundMarks"));
		FRDGTextureRef TrackT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TrackRHI, TEXT("RTSGround.Track")));
		FRDGTextureRef CraterT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterRHI, TEXT("RTSGround.Crater")));
		FRDGTextureRef CraterAtlasT = CraterAtlasRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterAtlasRHI, TEXT("RTSGround.CraterAtlas"))) : nullptr;
		FRDGTextureRef WreckAtlasT = WreckAtlasRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(WreckAtlasRHI, TEXT("RTSGround.WreckAtlas"))) : nullptr;
		FRHITexture* CraterSunRHI = CraterSunRes ? CraterSunRes->TextureRHI.GetReference() : nullptr;
		FRHITexture* WreckSunRHI = WreckSunRes ? WreckSunRes->TextureRHI.GetReference() : nullptr;
		FRDGTextureRef CraterSunT = CraterSunRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterSunRHI, TEXT("RTSGround.CraterSun"))) : nullptr;
		FRDGTextureRef WreckSunT = WreckSunRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(WreckSunRHI, TEXT("RTSGround.WreckSun"))) : nullptr;
		FRDGTextureUAVRef TrackUAV = GraphBuilder.CreateUAV(TrackT);
		FRDGTextureUAVRef CraterUAV = GraphBuilder.CreateUAV(CraterT);
		FRHISamplerState* Linear = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const auto Groups = [](const FIntRect& R) { return FIntVector(FMath::DivideAndRoundUp(R.Width(), 8), FMath::DivideAndRoundUp(R.Height(), 8), 1); };
		for (const FJob& J : Jobs)
		{
			const FRTSGroundMarkAtlas& A = J.M.bWreck ? WreckAtlas : CraterAtlas;
			FRDGTextureRef AtlasT = J.M.bWreck ? WreckAtlasT : CraterAtlasT;
			if (!AtlasT)
			{
				continue;
			}
			const auto Fill = [&](FRTSMarkCommonParameters* P, const FIntRect& R, const FIntPoint& E)
			{
				const float TexelCm = float(SheetSizeCm.X / E.X);
				P->AtlasTex = AtlasT;
				P->AtlasSampler = Linear;
				P->Origin = R.Min;
				P->Size = R.Size();
				P->SheetMinCm = FVector2f(SheetMinCm);
				P->SheetTexelCm = TexelCm;
				P->CenterCm = J.M.CenterCm;
				P->Axis = J.M.Axis;
				P->Scale = J.M.Scale;
				P->Stretch = J.M.Stretch;
				P->RampMode = A.bRamps ? 1 : 0;
				P->Mirror = J.M.Mirror;
				P->HeightMul = J.M.HeightMul;
				P->DarkMul = J.M.DarkMul;
				P->HeatMul = J.M.HeatMul;
				P->LightMul = J.M.LightMul;
				P->Variant = float(J.M.Variant % FMath::Max(A.Variants, 1));
				P->Variants = float(FMath::Max(A.Variants, 1));
				P->SpanCm = A.SpanCm;
				P->AtlasTexels = float(A.Size);
				P->AtlasRangeCm = A.RangeCm;
				// a caixa de filtro: quantos texels do atlas cabem num do caderno (o grosso tem ~11)
				P->Taps = FMath::Clamp(FMath::CeilToInt(TexelCm / (A.SpanCm * J.M.Scale / A.Size)), 1, 12);
			};
			if (J.TrackRect.Width() > 0 && J.TrackRect.Height() > 0)
			{
				FRTSMarkTrackParameters* P = GraphBuilder.AllocParameters<FRTSMarkTrackParameters>();
				Fill(&P->Common, J.TrackRect, TrackExtent);
				P->TargetTex = TrackUAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Mark"), TShaderMapRef<FRTSMarkTrackCS>(ShaderMap), P, Groups(J.TrackRect));
			}
			if (J.CraterRect.Width() > 0 && J.CraterRect.Height() > 0 && J.M.HeightMul > 0.0f)   // (a mancha do queimado não tem altura)
			{
				FRTSMarkHeightParameters* P = GraphBuilder.AllocParameters<FRTSMarkHeightParameters>();
				Fill(&P->Common, J.CraterRect, CraterExtent);
				FRDGTextureRef SunT = J.M.bWreck ? WreckSunT : CraterSunT;
				P->SunTex = SunT ? SunT : AtlasT;
				P->UseSun = SunT ? 1 : 0;
				P->CraterOut = CraterUAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Crater"), TShaderMapRef<FRTSMarkHeightCS>(ShaderMap), P, Groups(J.CraterRect));
			}
		}
		GraphBuilder.Execute();
	});
}

void RTSGroundRelief::ComputeSunLit(UTextureRenderTarget2D* Relief, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* SunLit, const FRTSSunLitParams& Params, const FIntRect& InRect,
	UTextureRenderTarget2D* Profile, UTextureRenderTarget2D* Along)
{
	if (!Relief || !Crater || !SunLit || InRect.Width() <= 0 || InRect.Height() <= 0)
	{
		return;
	}
	FTextureRenderTargetResource* ReliefRes = Relief->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* CraterRes = Crater->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* SunLitRes = SunLit->GameThread_GetRenderTargetResource();
	if (!ReliefRes || !CraterRes || !SunLitRes)
	{
		return;
	}
	// a garra só com o perfil e o andado (o caderno do detalhe da v3)
	FTextureRenderTargetResource* ProfileRes = Profile && Along && Params.ClawCm > 0.0f ? Profile->GameThread_GetRenderTargetResource() : nullptr;
	FTextureRenderTargetResource* AlongRes = ProfileRes ? Along->GameThread_GetRenderTargetResource() : nullptr;
	FTextureResource* SunSurfaceRes = Params.Surface ? Params.Surface->GetResource() : nullptr;
	const FIntPoint Extent(SunLit->SizeX, SunLit->SizeY);
	const int32 SS = FMath::Clamp(Params.Supersample, 1, 4);
	const double BaseTexelCm = double(Params.SheetSizeCm) / Extent.X;
	const float SunTan = FMath::Max(Params.SunTan, 0.03f);
	// a sombra mais longa, em texels do caderno de luz (a 8° e 1 m de queda: ~7 m)
	const int32 Reach = FMath::Min(FMath::CeilToInt(Params.MaxDropCm / SunTan / BaseTexelCm) + 2, 1024);
	const int32 WriteReach = FMath::Min(FMath::CeilToInt(FMath::Min(Params.WriteDropCm, Params.MaxDropCm) / SunTan / BaseTexelCm) + 2, Reach);
	// a varredura anda pra longe do sol
	const FVector2D Away = -FVector2D(Params.SunDir).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		return;
	}
	const FIntPoint Shift(FMath::RoundToInt(Away.X * Reach), FMath::RoundToInt(Away.Y * Reach));
	const FIntPoint WriteShift(FMath::RoundToInt(Away.X * WriteReach), FMath::RoundToInt(Away.Y * WriteReach));
	const FIntRect Sheet(0, 0, Extent.X, Extent.Y);
	struct FTile
	{
		FIntRect Write;
		FIntRect Grid;
	};
	TArray<FTile> Tiles;
	constexpr int32 TileSize = 512;
	for (int32 Y0 = InRect.Min.Y; Y0 < InRect.Max.Y; Y0 += TileSize)
	{
		for (int32 X0 = InRect.Min.X; X0 < InRect.Max.X; X0 += TileSize)
		{
			const FIntRect R(X0, Y0, FMath::Min(X0 + TileSize, InRect.Max.X), FMath::Min(Y0 + TileSize, InRect.Max.Y));
			// escreve o retângulo e a sombra que ele joga (pro lado de longe do sol); varre a partir de antes, do lado do sol
			FIntRect Write(FIntPoint(FMath::Min(R.Min.X, R.Min.X + WriteShift.X), FMath::Min(R.Min.Y, R.Min.Y + WriteShift.Y)),
				FIntPoint(FMath::Max(R.Max.X, R.Max.X + WriteShift.X), FMath::Max(R.Max.Y, R.Max.Y + WriteShift.Y)));
			Write.Clip(Sheet);
			FIntRect Grid(FIntPoint(FMath::Min(Write.Min.X, Write.Min.X - Shift.X) - 1, FMath::Min(Write.Min.Y, Write.Min.Y - Shift.Y) - 1),
				FIntPoint(FMath::Max(Write.Max.X, Write.Max.X - Shift.X) + 1, FMath::Max(Write.Max.Y, Write.Max.Y - Shift.Y) + 1));
			Grid.Clip(Sheet);
			if (Write.Width() > 0 && Write.Height() > 0 && Grid.Width() > 0 && Grid.Height() > 0)
			{
				Tiles.Add({ Write, Grid });
			}
		}
	}
	if (Tiles.IsEmpty())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSGroundSunLit)([ReliefRes, CraterRes, SunLitRes, ProfileRes, AlongRes, SunSurfaceRes, Params, Extent, SS, BaseTexelCm, SunTan, Away, Tiles](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* ReliefRHI = ReliefRes->GetRenderTargetTexture();
		FRHITexture* CraterRHI = CraterRes->GetRenderTargetTexture();
		FRHITexture* SunLitRHI = SunLitRes->GetRenderTargetTexture();
		if (!ReliefRHI || !CraterRHI || !SunLitRHI)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RTSGroundSunLit"));
		FRDGTextureRef ReliefT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ReliefRHI, TEXT("RTSGround.Relief")));
		FRDGTextureRef CraterT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterRHI, TEXT("RTSGround.Crater")));
		FRDGTextureRef SunLitT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SunLitRHI, TEXT("RTSGround.SunLit")));
		FRHITexture* ProfileRHI = ProfileRes ? ProfileRes->GetRenderTargetTexture() : nullptr;
		FRHITexture* AlongRHI = AlongRes ? AlongRes->GetRenderTargetTexture() : nullptr;
		const bool bClaw = ProfileRHI && AlongRHI;
		FRDGTextureRef ProfileT = bClaw ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ProfileRHI, TEXT("RTSGround.Profile"))) : ReliefT;
		FRDGTextureRef AlongT = bClaw ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(AlongRHI, TEXT("RTSGround.Along"))) : ReliefT;
		FRDGTextureUAVRef SunLitUAV = GraphBuilder.CreateUAV(SunLitT);
		FRHITexture* SurfaceRHI = SunSurfaceRes ? SunSurfaceRes->TextureRHI.GetReference() : nullptr;
		const bool bSurface = SurfaceRHI != nullptr;
		FRDGTextureRef SurfaceT = bSurface ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SurfaceRHI, TEXT("RTSGround.SunSurface")))
			: GSystemTextures.GetBlackDummy(GraphBuilder);
		FRHISamplerState* Linear = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		const float Elev = FMath::Atan(SunTan);
		const auto FillCommon = [&](FRTSSunLitCommon& C)
		{
			C.ReliefTex = ReliefT;
			C.CraterTex = CraterT;
			C.LinearSampler = Linear;
			C.RangeCm = Params.RangeCm;
			C.SunDir = Params.SunDir;
			C.SunTan = SunTan;
			C.SunSin = FMath::Sin(Elev);
			C.SunCos = FMath::Cos(Elev);
			C.PenumbraCm = Params.PenumbraCm;
			C.MaxLit = Params.MaxLit;
			C.ProfileTex = ProfileT;
			C.AlongTex = AlongT;
			C.UseClaw = bClaw ? 1 : 0;
			C.ClawCm = Params.ClawCm;
			C.ClawPitchCm = FMath::Max(Params.ClawPitchCm, 1.0f);
			C.AlongRangeCm = Params.AlongRangeCm;
			C.BandScale = Params.BandScale;
			C.SheetMinCm = Params.SheetMinCm;
			C.SheetSizeCmF = Params.SheetSizeCm;
			C.SurfaceTex = SurfaceT;
			C.SurfaceMinCm = Params.SurfaceMinCm;
			C.SurfaceInvSizeCm = 1.0f / FMath::Max(Params.SurfaceSizeCm, 1.0f);
			C.UseSurface = bSurface ? 1 : 0;
			C.SurfDepth = Params.SurfDepth;
			C.SurfBerm = Params.SurfBerm;
			C.SurfClaw = Params.SurfClaw;
			C.SurfWet = Params.SurfWet;
		};
		const bool bMajorY = FMath::Abs(Away.Y) > FMath::Abs(Away.X);
		const double Wm = bMajorY ? Away.Y : Away.X;
		const double Wn = bMajorY ? Away.X : Away.Y;
		const float FineTexelCm = float(BaseTexelCm / SS);
		for (const FTile& T : Tiles)
		{
			const FIntPoint FineSize = T.Write.Size() * SS;
			FRDGTextureRef FineT = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FineSize, PF_R16F, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("RTSGround.SunLitFine"));
			{
				FRTSSunLitSweepParameters* P = GraphBuilder.AllocParameters<FRTSSunLitSweepParameters>();
				FillCommon(P->Common);
				P->FineTexelUV = FVector2f(1.0f / (Extent.X * SS), 1.0f / (Extent.Y * SS));
				P->FineTexelCm = FineTexelCm;
				P->GridOrigin = T.Grid.Min * SS;
				P->GridSize = T.Grid.Size() * SS;
				P->WriteMin = T.Write.Min * SS;
				P->WriteMax = T.Write.Max * SS;
				P->MajorY = bMajorY ? 1 : 0;
				P->StepSign = Wm >= 0.0 ? 1 : -1;
				P->Slope = float(Wn / FMath::Abs(Wm));
				const int32 MajorLen = bMajorY ? P->GridSize.Y : P->GridSize.X;
				const int32 MinorLen = bMajorY ? P->GridSize.X : P->GridSize.Y;
				const int32 Span = FMath::CeilToInt(FMath::Abs(P->Slope) * (MajorLen - 1));
				P->LineBase = P->Slope >= 0.0f ? -Span : 0;
				P->LineCount = MinorLen + Span;
				P->StepCm = float(FineTexelCm / FMath::Abs(Wm));
				P->FineOut = GraphBuilder.CreateUAV(FineT);
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround SunLit sweep"), TShaderMapRef<FRTSSunLitSweepCS>(ShaderMap), P,
					FIntVector(FMath::DivideAndRoundUp(P->LineCount, 64), 1, 1));
			}
			{
				FRTSSunLitResolveParameters* P = GraphBuilder.AllocParameters<FRTSSunLitResolveParameters>();
				FillCommon(P->Common);
				P->FineIn = FineT;
				P->SunLitOut = SunLitUAV;
				P->SS = SS;
				P->BaseMin = T.Write.Min;
				P->BaseSize = T.Write.Size();
				P->BaseTexelUV = FVector2f(1.0f / Extent.X, 1.0f / Extent.Y);
				P->BaseTexelCm = float(BaseTexelCm);
				P->AORadiusCm = Params.AORadiusCm;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround SunLit resolve"), TShaderMapRef<FRTSSunLitResolveCS>(ShaderMap), P,
					FIntVector(FMath::DivideAndRoundUp(T.Write.Width(), 8), FMath::DivideAndRoundUp(T.Write.Height(), 8), 1));
			}
		}
		GraphBuilder.Execute();
	});
}
