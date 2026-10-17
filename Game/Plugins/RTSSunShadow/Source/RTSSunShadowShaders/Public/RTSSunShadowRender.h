#pragma once

#include "CoreMinimal.h"
#include "RHIFwd.h"
#include "RHIResources.h"
#include "ShaderParameterMacros.h"

class FTexture;

// A parte do mapa do sol que a thread de desenho lê (o módulo RTSSunShadowShaders, carregado cedo, junto com os shaders
// globais que incluem o SunShadow.ush). O subsistema que monta o mapa fica no RTSSunShadow (RTSSunShadow.h).

/** Os parâmetros de um shader global que lê o mapa (os nomes batem com o SunShadow.ush, RTS_SUN_SHADOW_GLOBALS 1). */
BEGIN_SHADER_PARAMETER_STRUCT(FRTSSunShadowParameters, RTSSUNSHADOWSHADERS_API)
	SHADER_PARAMETER_TEXTURE(Texture2D, RTSSunHMap)
	SHADER_PARAMETER_SAMPLER(SamplerState, RTSSunHMapSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, RTSSunTMap)
	SHADER_PARAMETER_SAMPLER(SamplerState, RTSSunTMapSampler)
	SHADER_PARAMETER(FVector3f, RTSSunCamToMapCm)
	SHADER_PARAMETER(FVector4f, RTSSunAxis)
	SHADER_PARAMETER(float, RTSSunTanE)
	SHADER_PARAMETER(FVector3f, RTSSunWindow)
	SHADER_PARAMETER(float, RTSSunValid)
END_SHADER_PARAMETER_STRUCT()

/** O mapa como a thread de desenho vê (uma cópia por quadro). */
struct RTSSUNSHADOWSHADERS_API FRTSSunShadowRenderData
{
	FTextureRHIRef Static, Dynamic;
	FVector OriginCm = FVector::ZeroVector;    // a origem do mapa (cm, mundo; z = a base das alturas)
	FVector2f Axis = FVector2f(1.0f, 0.0f);    // o eixo da luz no chão
	float InvWidthCm = 0.0f, TanE = 0.0f, Texels = 0.0f;
	FVector2f WindowCm = FVector2f::ZeroVector;   // o canto da janela dos veículos (cm, no quadro do mapa)
	float WindowInvWidthCm = 0.0f;
	bool bValid = false;
};

namespace RTSSunShadow
{
	/** Na thread de desenho: o mapa deste quadro (bValid falso enquanto não chegou). */
	RTSSUNSHADOWSHADERS_API FRTSSunShadowRenderData Get_RenderThread();
	/** Na thread de desenho: preenche os parâmetros pra uma vista (a origem dela em cm, mundo); sem mapa, tudo ao sol. */
	RTSSUNSHADOWSHADERS_API void SetShaderParameters(FRTSSunShadowParameters& Out, const FVector& ViewOriginCm);
	/** Na thread do jogo (o subsistema, a cada quadro): o mapa que a thread de desenho vê a partir do próximo comando. */
	RTSSUNSHADOWSHADERS_API void PushRenderData(const FRTSSunShadowRenderData& Data, FTexture* Static, FTexture* Dynamic);
	/** Na thread do jogo: a thread de desenho para de ver o mapa (o mundo fechou). */
	RTSSUNSHADOWSHADERS_API void ClearRenderData();
}
