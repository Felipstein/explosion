#pragma once

#include "CoreMinimal.h"

/**
 * A sombra das fumaças de puffs no mapa do sol (23/10; o Felipe: "as fumaças do Tomahawk, do Patriot e do carrinho não projetam
 * sombras bonitas e reais"; o sistema único de docs/ESTUDO_LUZ_NA_FUMACA.md, a fase 2b: o mapa composto). Um registro só, no
 * padrão do RTSSceneLights: o rastro do míssil (e o diesel, que é o mesmo sistema de puffs) manda a cada quadro os puffs que
 * fazem sombra; o render soma a coluna analítica de cada um (a gaussiana 3D vista do sol é uma gaussiana 2D) ao mapa de
 * espessura óptica das explosões e da poeira (BlastRaymarch, MainShadowMap), no mapa que a light function do sol
 * (M_SmokeShadowLF) lê: a sombra tira só o sol direto, cai no chão, nas unidades e nos prédios, e soma lisa com a das
 * explosões. Quem tem o mapa (o BlastFx, UBlastRaymarchSubsystem) avisa por SetMap_GameThread a cada quadro.
 */
namespace RTSSmokeShadow
{
	/** Um puff que faz sombra neste quadro (o mundo, m). */
	struct FPuff
	{
		FVector PosM = FVector::ZeroVector;            // o centro
		float SigmaM = 1.0f;                           // o σ da gaussiana
		float TauC = 0.0f;                             // a coluna no centro: extinção × parte visível / (2π σ²) × o apagamento
		float Cut = 0.0f;                              // o corte do contorno visto do sol, relativo ao centro (0 = sem corte; ≥ 1 não entra)
		float GroundZM = 0.0f;                         // o chão debaixo dele (a parte do puff abaixo do chão não faz sombra)
		FVector3f Axis = FVector3f::ZeroVector;        // o rumo do rastro aqui (unitária; zero = sem rumo, a nuvem da largada)
	};

	/** O lado do mapa final (texels): a janela de 192 m em texels de 0,375 m. O mapa das explosões continua em 256² (0,75 m). */
	constexpr int32 OutRes = 512;

	/** Troca a lista inteira (o rastro, uma vez por quadro). */
	RTSSHADERS_API void SetPuffs_GameThread(TArray<FPuff>&& Puffs);
	/** Quantos puffs o último Set mandou (0 se ele não veio nos últimos quadros: a partida acabou ou o rastro parou de mandar). */
	RTSSHADERS_API int32 NumPuffs_GameThread();
	RTSSHADERS_API bool HasPuffs_RenderThread();
	/** A cópia do render thread (a do último Set que chegou lá; vazia se velha). */
	RTSSHADERS_API const TArray<FPuff>& Get_RenderThread();

	/** Quem tem o mapa avisa a cada quadro que ele existe (bOn: a light function liga neste quadro) e a direção pro sol que ele usa
	 *  (já com o piso de elevação das sombras das fumaças). */
	RTSSHADERS_API void SetMap_GameThread(bool bOn, const FVector& ToSun);
	/** true se o mapa existe e foi alimentado nos últimos quadros (a direção pro sol dele em OutToSun); false sem o mapa (o
	 *  ambiente do BlastLab, -Raymarch=0, -RmShadow=0, ou nada vivo ainda). */
	RTSSHADERS_API bool GetMap_GameThread(FVector& OutToSun);
}
