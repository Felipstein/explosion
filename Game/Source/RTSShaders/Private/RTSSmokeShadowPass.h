#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

class FGlobalShaderMap;

/** O passe da composição do mapa da sombra (RTSSmokeShadow.cpp, SmokeShadowSplat.usf), pro BlastRaymarch. */
namespace RTSSmokeShadow
{
	/** A janela do mapa neste quadro (a mesma do FBlastRaymarchExtension::FShadow). */
	struct FMap
	{
		FVector2D OriginM = FVector2D::ZeroVector;     // o canto da janela no mundo (m)
		float WindowM = 192.0f;                        // o lado da janela (m)
		float GroundZM = 0.0f;                         // o plano do mapa (m)
		FVector3f ToSun = FVector3f(0.0f, 0.0f, 1.0f); // pro sol, com o piso de elevação (a mesma direção que a light function usa)
		float SunHalfAngleRad = 0.004675f;             // o raio angular do disco do sol (a penumbra)
	};

	/**
	 * Escreve o mapa final (Out, o alvo que a light function lê): o mapa de passagem das explosões e da poeira (VolTau, na mesma
	 * janela; nullptr = sem explosão nem poeira) amostrado em bilinear, mais a coluna de cada puff do rastro (os do último
	 * SetPuffs_GameThread), agrupados por ladrilho de 8 × 8 texels aqui no render thread.
	 */
	void AddComposePass(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGTextureRef VolTau, FRDGTextureRef Out, const FMap& Map);
}
