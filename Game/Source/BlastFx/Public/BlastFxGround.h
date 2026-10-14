// Cópia do BlastLab (BlastLabGround, 14/10/2026): o chão pra quem a explosão do jogo consulta (a sombra da
// fumaça, os destroços). A original continua sendo a referência.
#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * O chão do campo. Nos campos feitos das fatias do WorldForge (Range_*) é o
 * Landscape, com relevo de verdade; no campo de testes, que não tem Landscape,
 * o plano z = 0 de sempre. Rochas, árvores e destroços não contam: o clique,
 * a câmera e a sombra da fumaça seguem o terreno.
 */
namespace BlastFxGround
{
	/** O primeiro ponto do chão no raio (cm). Falso se ele não chega no chão até MaxCm. */
	BLASTFX_API bool Ray(const UWorld* World, const FVector& Origin, const FVector& Dir, double MaxCm, FVector& Out);

	/** A altura do chão (cm) na vertical de (X, Y) em cm; fora do terreno, 0. */
	BLASTFX_API double HeightAt(const UWorld* World, double X, double Y);

	/**
	 * A mesma altura, interpolada numa grade de 1 m guardada (o terreno não muda
	 * no jogo): pra quem consulta todo quadro, como os destroços. Cada nó da grade
	 * é traçado uma vez só.
	 */
	BLASTFX_API double HeightAtCached(const UWorld* World, double X, double Y);

	/** A caixa do terreno (cm). Falso sem Landscape (o campo de testes). */
	BLASTFX_API bool Bounds(const UWorld* World, FBox& Out);
}
