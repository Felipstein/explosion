#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldForgeEditorLib.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterialInterface;
class UStaticMesh;

/** Uma categoria de espalhamento na fatia (rochas grandes, seixos, tufos...). */
USTRUCT(BlueprintType)
struct FWorldForgeScatter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "WorldForge")
	TArray<FTransform> Transforms;
};

/**
 * Ferramentas de editor do WorldForge, chamadas pelo Python (Tools/build_slice.py).
 * O jogo final usa o Landscape da Unreal (LOD, Nanite, sombras virtuais, RVT,
 * PCG) — que só pode ser criado no editor. Estas funções levam o resultado do
 * gerador (o mesmo núcleo do app) pra dentro de um mapa de verdade.
 */
UCLASS()
class WORLDFORGEEDITOR_API UWorldForgeEditorLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gera o mapa (semente e controles do painel) e cria um Landscape com um
	 * recorte quadrado dele, centrado na origem do nível.
	 *
	 * @param CenterM          centro do recorte no mapa gerado (m)
	 * @param Components       componentes por lado (cada um com QuadsPerComponent quads)
	 * @param QuadSizeM        tamanho de um quad (m); o gerador tem 2 m, o recorte reamostra
	 * @param LayerInfos       as camadas do material, nesta ordem: Sand, Gravel, Dirt, Rock, Cracked
	 * @return o Landscape (o nível ainda não foi salvo) ou nullptr com Error
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static ALandscape* BuildSliceLandscape(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
		int32 Components, int32 QuadsPerComponent, float QuadSizeM, UMaterialInterface* Material,
		const TArray<ULandscapeLayerInfoObject*>& LayerInfos, FString& Error);

	/**
	 * Espalhamento no mesmo recorte (coordenadas do nível, cm), a partir das
	 * máscaras do gerador: Kind = "RockLarge", "RockSmall", "Pebble", "Tuft",
	 * "Shrub", "Branch", "Cliff" (malhas de paredão nos cortes do terreno,
	 * viradas pra baixo da encosta). Spacing = distância média entre
	 * instâncias (m); SinkM afunda no chão (× escala); YawOffset gira a malha
	 * (o "frente" de cada modelo). Determinístico.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FWorldForgeScatter SliceScatter(FName Kind, float SpacingM, float ScaleMin, float ScaleMax, float SinkM = 0.0f, float YawOffset = 0.0f);

	/** Pontos das trilhas no recorte (cm), na ordem, pra marcas de pneu. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FVector> SliceTracks(float StepM);

	/**
	 * Centro de recorte mais variado (m): percorre o mapa em passos de 32 m e
	 * pontua a mistura de camadas (areia, cascalho, terra, rocha, rachada),
	 * trilha e rampa dentro de um quadrado de SizeM. Chamar depois de
	 * BuildSliceLandscape não faz sentido: ele mesmo usa isto se CenterM.X > 1e8.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FVector2D SuggestSliceCenter(int32 Seed, int32 Players, int32 Biome, float MapSize, float SizeM);

	/** Ator vazio (com raiz) pra guardar componentes instanciados no nível. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static AActor* SpawnGroupActor(const FString& Label);

	/**
	 * Um componente instanciado (salvo com o nível) no ator, com as instâncias
	 * dadas (coordenadas do nível). CullDistanceM 0 = sem corte.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddInstances(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCastShadow, float CullDistanceM);

	/** Dá nome à camada de um LandscapeLayerInfoObject (a propriedade é só leitura no Python). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void SetLayerInfoName(ULandscapeLayerInfoObject* LayerInfo, FName LayerName);

	/** Liga o Nanite numa malha importada e reconstrói (o subsistema de malha do editor não existe no commandlet). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool EnableNanite(UStaticMesh* Mesh);

	/** Altura do chão no recorte (cm) num ponto do nível (cm). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceHeightAt(FVector2D PointCm);
};
