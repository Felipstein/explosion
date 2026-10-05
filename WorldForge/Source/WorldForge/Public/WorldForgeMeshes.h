#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

/**
 * Malhas dos objetos do mapa, geradas em código (low-poly facetado, cor no
 * vértice): sem assets importados, o gerador roda do zero. A cor do vértice
 * leva a cor de base (RGB) e a máscara de brilho (A: cristal, farol da base).
 * Assets de verdade (Megascans/Fab) entram depois no lugar destas.
 */
namespace WorldForgeMeshes
{
	enum class EKind : uint8
	{
		Conifer,     // pinheiro: tronco + 3 cones
		Broadleaf,   // copa redonda: tronco + 3 bolhas
		Rock,        // icosaedro deformado e achatado
		Crystal,     // aglomerado de prismas (campo de recurso)
		BasePad,     // marcador da posição inicial: laje octogonal + farol
		Num
	};

	/** Malha transitória (sem asset), com variações da semente Variant. */
	UStaticMesh* Build(EKind Kind, uint32 Variant, UMaterialInterface* Material, UObject* Outer);
}
