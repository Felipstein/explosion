#include "RTSGroundRelief.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
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
	SHADER_PARAMETER(FVector2f, WorldMinCm)
	SHADER_PARAMETER(FVector2f, SunDir)
	SHADER_PARAMETER(float, SunTan)
	SHADER_PARAMETER(float, PenumbraCm)
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
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, CraterOut)
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

void RTSGroundRelief::Compute(UTextureRenderTarget2D* Profile, UTextureRenderTarget2D* Crater, UTextureRenderTarget2D* Relief, const FRTSReliefParams& Params, const FIntRect& Rect)
{
	if (!Profile || !Crater || !Relief || Rect.Area() <= 0)
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
	ENQUEUE_RENDER_COMMAND(RTSGroundRelief)([ProfileRes, CraterRes, ReliefRes, Params, Extent, R1, R2](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* ProfileRHI = ProfileRes->GetRenderTargetTexture();
		FRHITexture* CraterRHI = CraterRes->GetRenderTargetTexture();
		FRHITexture* ReliefRHI = ReliefRes->GetRenderTargetTexture();
		if (!ProfileRHI || !CraterRHI || !ReliefRHI)
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef ProfileT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ProfileRHI, TEXT("RTSGround.Profile")));
		FRDGTextureRef CraterT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterRHI, TEXT("RTSGround.Crater")));
		FRDGTextureRef ReliefT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(ReliefRHI, TEXT("RTSGround.Relief")));
		const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTextureRef HeightT = GraphBuilder.CreateTexture(Desc, TEXT("RTSGround.Height"));
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
			P->WorldMinCm = Params.WorldMinCm;
			P->SunDir = Params.SunDir;
			P->SunTan = Params.SunTan;
			P->PenumbraCm = Params.PenumbraCm;
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
		if (J.TrackRect.Area() > 0 || J.CraterRect.Area() > 0)
		{
			Jobs.Add(J);
		}
	}
	if (Jobs.IsEmpty())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSGroundMarks)([TrackRes, CraterRes, CraterAtlasRes, WreckAtlasRes, CraterAtlas, WreckAtlas, Jobs, SheetMinCm, SheetSizeCm, TrackExtent, CraterExtent](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture* TrackRHI = TrackRes->GetRenderTargetTexture();
		FRHITexture* CraterRHI = CraterRes->GetRenderTargetTexture();
		FRHITexture* CraterAtlasRHI = CraterAtlasRes ? CraterAtlasRes->TextureRHI.GetReference() : nullptr;
		FRHITexture* WreckAtlasRHI = WreckAtlasRes ? WreckAtlasRes->TextureRHI.GetReference() : nullptr;
		if (!TrackRHI || !CraterRHI || (!CraterAtlasRHI && !WreckAtlasRHI))
		{
			return;
		}
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef TrackT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TrackRHI, TEXT("RTSGround.Track")));
		FRDGTextureRef CraterT = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterRHI, TEXT("RTSGround.Crater")));
		FRDGTextureRef CraterAtlasT = CraterAtlasRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(CraterAtlasRHI, TEXT("RTSGround.CraterAtlas"))) : nullptr;
		FRDGTextureRef WreckAtlasT = WreckAtlasRHI ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(WreckAtlasRHI, TEXT("RTSGround.WreckAtlas"))) : nullptr;
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
			if (J.TrackRect.Area() > 0)
			{
				FRTSMarkTrackParameters* P = GraphBuilder.AllocParameters<FRTSMarkTrackParameters>();
				Fill(&P->Common, J.TrackRect, TrackExtent);
				P->TargetTex = TrackUAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Mark"), TShaderMapRef<FRTSMarkTrackCS>(ShaderMap), P, Groups(J.TrackRect));
			}
			if (J.CraterRect.Area() > 0 && J.M.HeightMul > 0.0f)   // (a mancha do queimado não tem altura)
			{
				FRTSMarkHeightParameters* P = GraphBuilder.AllocParameters<FRTSMarkHeightParameters>();
				Fill(&P->Common, J.CraterRect, CraterExtent);
				P->CraterOut = CraterUAV;
				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("RTSGround Crater"), TShaderMapRef<FRTSMarkHeightCS>(ShaderMap), P, Groups(J.CraterRect));
			}
		}
		GraphBuilder.Execute();
	});
}
