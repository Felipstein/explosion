#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldForgeEditorLib.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterial;
class UMaterialInterface;
class USkeletalMesh;
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
	 * @param LayerInfos       as camadas, por papel: base, manchas, trilha, rocha, molhado — e,
	 *                         com 6, o asfalto (estrada ao longo da trilha mais longa)
	 * @param RoadWidthM       largura da pista (m); só vale com a 6ª camada
	 * @return o Landscape (o nível ainda não foi salvo) ou nullptr com Error
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static ALandscape* BuildSliceLandscape(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
		int32 Components, int32 QuadsPerComponent, float QuadSizeM, UMaterialInterface* Material,
		const TArray<ULandscapeLayerInfoObject*>& LayerInfos, float RoadWidthM, FString& Error);

	/**
	 * Espalhamento no mesmo recorte (coordenadas do nível, cm), a partir das
	 * máscaras do gerador: Kind = "RockLarge", "RockSmall", "Pebble", "Tuft",
	 * "Shrub", "Branch", "Tree" (as árvores do gerador em grupos: mata
	 * fechada; Spacing = raio do grupo), "Forest" (bosques com borda e clareiras,
	 * pela vegetação do gerador e uma mancha larga; Spacing = grade), "Undergrowth" e "Deadwood"
	 * (sub-bosque e madeira morta dentro da mata), "Cliff" (malhas de paredão nos cortes do terreno,
	 * viradas pra baixo da encosta). Spacing = distância média entre
	 * instâncias (m); SinkM afunda no chão (× escala); YawOffset gira a malha
	 * (o "frente" de cada modelo). Determinístico.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FWorldForgeScatter SliceScatter(FName Kind, float SpacingM, float ScaleMin, float ScaleMax, float SinkM = 0.0f, float YawOffset = 0.0f);

	/** Linha central da estrada no recorte (cm, com a cota do asfalto), a cada StepM metros. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FVector> SliceRoad(float StepM);

	/** Distância (cm) de um ponto do nível (cm) até a linha central da estrada (1e9 sem estrada). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceRoadDistance(FVector2D PointCm);

	/** Metros até a ponta mais perto da estrada (onde o asfalto se desfaz), num ponto perto dela (cm). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceRoadEndDistance(FVector2D PointCm);

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
	 * Componentes instanciados (salvos com o nível) no ator, um por bloco de
	 * 64 m, com as instâncias dadas (coordenadas do nível). CullDistanceM 0 = sem corte. Materials
	 * troca os materiais da malha só neste componente (por espaço; vazio = os
	 * da malha): a mesma malha serve a fatias com tons diferentes.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddInstances(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCastShadow, float CullDistanceM,
		const TArray<UMaterialInterface*>& Materials);

	/**
	 * Árvore do Procedural Vegetation Editor (malha esquelética Nanite cujos
	 * raminhos e folhas são uma "assembly": peças instanciadas nos ossos) →
	 * malha estática instanciável, na pose de referência. Com bBakeParts as
	 * peças viram geometria da malha; sem, a malha estática guarda a assembly
	 * (precisa de r.Nanite.Foliage ou r.Nanite.AllowAssemblies no projeto).
	 * As peças estáticas vêm das cópias que o exportador do PVE deixa ao lado
	 * das esqueléticas ("SKM_X" → "X").
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static UStaticMesh* VegetationToStaticMesh(USkeletalMesh* Source, const FString& AssetPath, bool bBakeParts);

	/**
	 * Neve num material que usa "material attributes" (o mestre das árvores do
	 * PVE, já duplicado pro projeto): a cor e a rugosidade de saída viram neve
	 * no que olha pra cima, pelo parâmetro escalar "Snow" (0 = sem neve).
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddSnowToMaterial(UMaterial* Material);

	/** Dá nome à camada de um LandscapeLayerInfoObject (a propriedade é só leitura no Python). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void SetLayerInfoName(ULandscapeLayerInfoObject* LayerInfo, FName LayerName);

	/** Liga o Nanite numa malha importada e reconstrói (o subsistema de malha do editor não existe no commandlet). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool EnableNanite(UStaticMesh* Mesh);

	/** Altura do chão no recorte (cm) num ponto do nível (cm). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceHeightAt(FVector2D PointCm);

	/**
	 * Os controles do painel do app (JSON que o "Ver em qualidade real" grava):
	 * daí em diante BuildSliceLandscape e SuggestSliceCenter geram com eles, no
	 * lugar da semente/bioma/tamanho que recebem. Devolve bioma e tamanho do mapa.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool LoadWorldSettings(const FString& JsonPath, int32& OutBiome, float& OutMapSize);

	/** Cota d'água do mapa gerado (cm, no nível); muito negativa se o mapa não tem água. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceWaterLevel();
};
