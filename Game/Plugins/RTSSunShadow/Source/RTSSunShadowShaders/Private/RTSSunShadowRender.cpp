#include "RTSSunShadowRender.h"

#include "RenderingThread.h"
#include "RenderResource.h"
#include "GlobalRenderResources.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"

namespace
{
	// o mapa como a thread de desenho vê (só ela lê e escreve)
	FRTSSunShadowRenderData GSunShadowRT;
}

void RTSSunShadow::PushRenderData(const FRTSSunShadowRenderData& Data, FTexture* Static, FTexture* Dynamic)
{
	ENQUEUE_RENDER_COMMAND(RTSSunShadowPush)([D = Data, Static, Dynamic](FRHICommandListImmediate&) mutable
	{
		D.Static = Static ? Static->TextureRHI : nullptr;
		D.Dynamic = Dynamic ? Dynamic->TextureRHI : nullptr;
		D.bValid = D.Static.IsValid() && D.Dynamic.IsValid() && D.InvWidthCm > 0.0f;
		GSunShadowRT = D;
	});
}

void RTSSunShadow::ClearRenderData()
{
	ENQUEUE_RENDER_COMMAND(RTSSunShadowClear)([](FRHICommandListImmediate&) { GSunShadowRT = FRTSSunShadowRenderData(); });
}

FRTSSunShadowRenderData RTSSunShadow::Get_RenderThread()
{
	check(IsInRenderingThread());
	return GSunShadowRT;
}

void RTSSunShadow::SetShaderParameters(FRTSSunShadowParameters& Out, const FVector& ViewOriginCm)
{
	check(IsInRenderingThread());
	const FRTSSunShadowRenderData& D = GSunShadowRT;
	FRHISamplerState* Smp = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Out.RTSSunHMapSampler = Smp;
	Out.RTSSunTMapSampler = Smp;
	if (D.bValid)
	{
		Out.RTSSunHMap = D.Static;
		Out.RTSSunTMap = D.Dynamic;
		Out.RTSSunCamToMapCm = FVector3f(ViewOriginCm - D.OriginCm);
		Out.RTSSunAxis = FVector4f(D.Axis.X, D.Axis.Y, D.InvWidthCm, D.Texels);
		Out.RTSSunTanE = D.TanE;
		Out.RTSSunWindow = FVector3f(D.WindowCm.X, D.WindowCm.Y, D.WindowInvWidthCm);
		Out.RTSSunValid = 1.0f;
	}
	else
	{
		Out.RTSSunHMap = GBlackTexture->TextureRHI;
		Out.RTSSunTMap = GBlackTexture->TextureRHI;
		Out.RTSSunCamToMapCm = FVector3f::ZeroVector;
		Out.RTSSunAxis = FVector4f(1.0f, 0.0f, 0.0f, 1.0f);
		Out.RTSSunTanE = 0.0f;
		Out.RTSSunWindow = FVector3f::ZeroVector;
		Out.RTSSunValid = 0.0f;
	}
}
