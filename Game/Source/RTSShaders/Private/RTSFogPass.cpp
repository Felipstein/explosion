#include "RTSFogPass.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Texture2D.h"
#include "EngineModule.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"

class FRTSFogOfWarPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRTSFogOfWarPS);
	SHADER_USE_PARAMETER_STRUCT(FRTSFogOfWarPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepth)
		SHADER_PARAMETER_TEXTURE(Texture2D, FogTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, FogSampler)
		SHADER_PARAMETER(FMatrix44f, ClipToWorld)
		SHADER_PARAMETER(FVector4f, ViewRect)
		SHADER_PARAMETER(FVector4f, MapMinInvSize)
		SHADER_PARAMETER(FVector2f, FogTexelSize)
		SHADER_PARAMETER(float, Strength)
		SHADER_PARAMETER(float, Time)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRTSFogOfWarPS, "/RTSGame/Private/FogOfWar.usf", "MainPS", SF_Pixel);

namespace
{
	/** O que a passada precisa, do lado do render thread (copiado por comando de renderização). */
	struct FFogRenderState
	{
		bool bEnabled = false;
		FTextureResource* Resource = nullptr;
		FVector2f MapMin = FVector2f::ZeroVector;
		FVector2f MapInvSize = FVector2f::ZeroVector;
		FVector2f TexelSize = FVector2f(1.0f / 256.0f, 1.0f / 256.0f);
		float Strength = 1.0f;
	};
	FFogRenderState GRenderState;     // só o render thread mexe
	FDelegateHandle GDelegate;        // só o game thread mexe
	const double GStartSeconds = FPlatformTime::Seconds();

	void RenderFog(FPostOpaqueRenderParameters& P)
	{
		const FFogRenderState& S = GRenderState;
		// (17/10, o Look) com rts.FogStyle 1 ou 2 a névoa de guerra é desenhada pelo Look (a sombra de nuvem na luz do sol ou
		// o desfoque no pós): este passe não multiplica. A CVar é do RTSLook; 0 = como antes.
		static TConsoleVariableData<int32>* StyleVar = nullptr;
		if (!StyleVar)
		{
			StyleVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rts.FogStyle"));
		}
		if (StyleVar && StyleVar->GetValueOnRenderThread() != 0)
		{
			return;
		}
		if (!S.bEnabled || !S.Resource || S.Strength <= 0.0f || !P.GraphBuilder || !P.ColorTexture || !P.DepthTexture)
		{
			return;
		}
		FRHITexture* FogRHI = S.Resource->TextureRHI;
		if (!FogRHI)
		{
			return;
		}
		FRDGBuilder& GraphBuilder = *P.GraphBuilder;
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FRTSFogOfWarPS> Shader(ShaderMap);

		FRTSFogOfWarPS::FParameters* Params = GraphBuilder.AllocParameters<FRTSFogOfWarPS::FParameters>();
		Params->SceneDepth = P.DepthTexture;
		Params->FogTexture = FogRHI;
		Params->FogSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		// do recorte (com o jitter do TSR, o mesmo do depth) pro mundo
		const FMatrix ClipToWorld = (P.ViewMatrix * P.ProjMatrix).Inverse();
		Params->ClipToWorld = FMatrix44f(ClipToWorld);
		const FIntRect R = P.ViewportRect;
		Params->ViewRect = FVector4f(float(R.Min.X), float(R.Min.Y), float(FMath::Max(R.Width(), 1)), float(FMath::Max(R.Height(), 1)));
		Params->MapMinInvSize = FVector4f(S.MapMin.X, S.MapMin.Y, S.MapInvSize.X, S.MapInvSize.Y);
		Params->FogTexelSize = S.TexelSize;
		Params->Strength = FMath::Clamp(S.Strength, 0.0f, 1.0f);
		Params->Time = float(FPlatformTime::Seconds() - GStartSeconds);
		Params->RenderTargets[0] = FRenderTargetBinding(P.ColorTexture, ERenderTargetLoadAction::ELoad);

		// cor da cena × nível (só RGB): o que já está na cena é o opaco iluminado
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, ShaderMap, RDG_EVENT_NAME("RTSFogOfWar"), Shader, Params, R,
			TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
	}
}

void RTSFogPass::Enable(UTexture2D* VisionTexture, const FVector2D& MapMinCm, const FVector2D& MapSizeCm)
{
	check(IsInGameThread());
	if (!VisionTexture || MapSizeCm.X <= 0.0 || MapSizeCm.Y <= 0.0)
	{
		Disable();
		return;
	}
	if (!GDelegate.IsValid())
	{
		GDelegate = GetRendererModule().RegisterPostOpaqueRenderDelegate(FPostOpaqueRenderDelegate::CreateStatic(&RenderFog));
	}
	FFogRenderState New;
	New.bEnabled = true;
	New.Resource = VisionTexture->GetResource();
	New.MapMin = FVector2f(MapMinCm);
	New.MapInvSize = FVector2f(float(1.0 / MapSizeCm.X), float(1.0 / MapSizeCm.Y));
	New.TexelSize = FVector2f(1.0f / float(FMath::Max(VisionTexture->GetSizeX(), 1)), 1.0f / float(FMath::Max(VisionTexture->GetSizeY(), 1)));
	ENQUEUE_RENDER_COMMAND(RTSFogEnable)([New](FRHICommandListImmediate&)
	{
		const float Strength = GRenderState.Strength;
		GRenderState = New;
		GRenderState.Strength = Strength;
	});
}

void RTSFogPass::SetStrength(float Strength)
{
	ENQUEUE_RENDER_COMMAND(RTSFogStrength)([Strength](FRHICommandListImmediate&)
	{
		GRenderState.Strength = Strength;
	});
}

void RTSFogPass::Disable()
{
	if (!IsInGameThread())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(RTSFogDisable)([](FRHICommandListImmediate&)
	{
		GRenderState = FFogRenderState();
	});
	if (GDelegate.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("Renderer")))
	{
		GetRendererModule().RemovePostOpaqueRenderDelegate(GDelegate);
	}
	GDelegate.Reset();
}
