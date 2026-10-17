#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * As luzes da cena pras fumaças (16/10 noite; o estudo: docs/ESTUDO_LUZ_NA_FUMACA.md; a regra do CLAUDE.md: "toda
 * fumaça, fogo e poeira ... é acesa pelo fogo, pelas explosões e pelas luzes perto"). Um registro só: o BlastFx junta a
 * cada quadro as luzes pontuais acesas da cena (as das explosões, do fogo, dos tiros), as 32 mais fortes em volta da
 * câmera, e quem desenha fumaça lê daqui:
 * - o raymarch das explosões e poeiras, no render thread (as 4 que mais iluminam cada explosão, por amostra);
 * - o rastro do míssil, o diesel e a poeira das lagartas, no game thread (IlluminanceAt somado por puff).
 * Sem sombra pela cena (a luz atravessa as casas); a sombra pela própria fumaça é de quem desenha.
 */
namespace RTSSceneLights
{
	struct FLight
	{
		FVector PosM = FVector::ZeroVector;           // m, mundo
		float RadiusM = 10.0f;                         // o raio de atenuação
		FLinearColor Candela = FLinearColor::Black;    // a cor linear × a intensidade (cd)
	};

	constexpr int32 MaxLights = 32;

	/** Troca a lista inteira (o BlastFx, uma vez por quadro). */
	RTSSHADERS_API void Set_GameThread(TArray<FLight>&& Lights);
	RTSSHADERS_API const TArray<FLight>& Get_GameThread();
	/** A cópia do render thread (a do último Set que chegou lá). */
	RTSSHADERS_API const TArray<FLight>& Get_RenderThread();

	/**
	 * A iluminância (lux, RGB) num ponto do mundo (m): Σ I/d² × a janela da Unreal (1 − (d/R)⁴)², com d ≥ 0,5 m
	 * (DeferredLightingCommon.ush). Sem sombra. Pra quem soma por puff no processador.
	 */
	RTSSHADERS_API FLinearColor IlluminanceAt(const FVector& PosM);

	/**
	 * A mesma lista numa textura pra material (Custom), 32×2 RGBA32F, sem filtro: a linha 0 é a posição no mundo (m) e o
	 * raio (m; 0 = vaga vazia), a linha 1 é a cor linear × cd. Refeita a cada Set (o game thread). Pra a poeira das
	 * lagartas e quem mais for material.
	 */
	RTSSHADERS_API UTexture2D* GetTexture();
}
