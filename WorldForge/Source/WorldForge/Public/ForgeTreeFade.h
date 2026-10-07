#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ForgeTreeFade.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Árvore transparente perto do cursor, a parte da CPU. O material das árvores
 * (WorldForgeEditorLib::AddOcclusionFadeToMaterial) recorta os pixels perto da
 * linha câmera → cursor, mas o Nanite desenha a árvore de longe em voxels, e o
 * voxel só usa o material padrão (NaniteCullRaster.cpp: a permutação de voxels
 * só compila pra ele), então o recorte não vale ali. As árvores perto da linha
 * saem do seu componente (escala quase 0) e vão pra um componente "fantasma" com
 * a cópia da malha que nunca vira voxels (SM_x_Fade, build_slice.py tree_fade),
 * onde vale.
 * Só nos componentes instanciados simples das fatias e do mapa inteiro.
 */
UCLASS()
class UForgeTreeFade : public UObject
{
	GENERATED_BODY()

public:
	/** Câmera e cursor (cm) e o raio do recorte (cm; 0 devolve todas as árvores). */
	void Update(UWorld* World, const FVector& Camera, const FVector& Cursor, float Radius);

	/** Quantas árvores estão no fantasma agora. */
	int32 NumFaded() const;

private:
	struct FSource
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Ghost;
		TWeakObjectPtr<UStaticMesh> FadeMesh;
		TArray<FTransform> Transforms;   // das instâncias, no mundo
		FBox Bounds = FBox(ForceInit);
		float Height = 0.0f;             // topo da malha (cm, escala 1)
		float Crown = 0.0f;              // raio da copa (cm, escala 1)
		TArray<int32> Faded;             // instâncias no fantasma, em ordem
	};

	void Scan(UWorld* World);
	void SetFaded(FSource& Source, const TArray<int32>& Wanted);

	TArray<FSource> Sources;
	TWeakObjectPtr<UWorld> ScannedWorld;
	float MaxReach = 0.0f;               // maior copa + altura (cm), pra caixa da busca
};
