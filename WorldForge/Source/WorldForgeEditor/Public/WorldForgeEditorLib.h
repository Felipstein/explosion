#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldForgeEditorLib.generated.h"

class ALandscape;
class ULandscapeLayerInfoObject;
class UMaterial;
class UMaterialInterface;
class UMaterialExpression;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

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
	 *                         com 6, o asfalto (a estrada principal de cada jogador)
	 * @param RoadWidthM       largura da estrada principal de asfalto (m); só vale com a 6ª
	 *                         camada (sem ela, a principal é de terra). A rede de estradas
	 *                         (principal, terra, ramais até os campos) é do mapa inteiro
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

	/**
	 * O mesmo sorteio de SliceScatter, mas cada lugar vai pra uma das malhas
	 * (sorteada) e é assentado pela caixa dela (o ponto de origem de peças de
	 * conjunto fica longe da pedra). Uma lista de transformações por malha.
	 * Pedras: chão medido em 7 pontos da base, inclinadas pela encosta, ~25%
	 * enterradas; fora onde o chão é desnivelado demais. "Cliff": em pé, ao
	 * longo da parede, face (+Y) morro abaixo, costas na encosta, altura pelo
	 * desnível.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FWorldForgeScatter> SliceScatterMeshes(FName Kind, float SpacingM, float ScaleMin, float ScaleMax,
		const TArray<UStaticMesh*>& Meshes, float SinkM = 0.0f);

	/**
	 * Linhas centrais das estradas de asfalto no recorte (cm, com a cota do
	 * asfalto), a cada StepM metros, com (0,0,−1e9) separando os trechos.
	 * bMarksOnly: só onde cabe pintura e mureta (fora dos entroncamentos e
	 * longe das pontas que se desfazem).
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FVector> SliceRoad(float StepM, bool bMarksOnly = false);

	/** Distância (cm) de um ponto do nível (cm) até a linha central da estrada de asfalto mais perto (1e9 sem). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceRoadDistance(FVector2D PointCm);

	/** A meia largura (cm) da estrada de asfalto mais perto de um ponto do nível (cm): a rua da vila alarga
	 *  a principal (13/10), a pintura da borda acompanha. 0 sem estrada. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceRoadHalf(FVector2D PointCm);

	/** Liga uma expressão no pino Displacement do material (o MP_Displacement é escondido no Python): o
	 *  deslocamento Nanite da cratera do jogo (13/10, o Look e o jogo). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool ConnectMaterialDisplacement(UMaterialExpression* From, const FString& FromOutputName);

	/** A grade do tipo de chão pro jogo (13/10, pedido do jogo: no empacotado a camada do Landscape some; a poeira
	 *  não levanta areia do asfalto e a esteira não cava a rua): PNG RGBA de 8 bits, R = asfalto, G = terra batida
	 *  (trilha, chão pisado da vila), B = rocha, A = 255; o mapa inteiro a CellM por pixel, a linha 0 no −Y. Devolve
	 *  o JSON do lado (min_m, size_m, cell_m, res, canais); vazio se não deu. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString MakeSurfacePng(const FString& PngPath, float CellM);

	/** A água por corpo (13/10, docs/WORLDFORGE.md §12.3, a P0): na pasta OutDir, a grade da simulação
	 *  (grid.png a 4 m, R = seco/margem/vau/fundo/gelo, G = o id do corpo, e grid.json com os corpos e o
	 *  hash FNV-1a 64) e o campo do material (water.exr a 1 m, RGBA16F: R = a distância com sinal até a
	 *  margem, G = a profundidade, BA = a correnteza); e a textura T_Water em TextureAssetPath. Devolve um
	 *  resumo pro log; vazio se não deu. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString MakeWaterExports(const FString& OutDir, const FString& TextureAssetPath, const FString& MapName);

	/** A água só onde existe água (13/10, docs/WORLDFORGE.md §12.3, a P1): uma malha estática por corpo
	 *  (AssetDir/SM_Water_<mapa>_<id>, grade de 2 m; o mar a 4 m e com a saia até o horizonte), na cota do
	 *  corpo, com o material e o Custom Primitive Data do Look (0 = cota em m, 1 = tipo, 2 = turbidez,
	 *  3 = escala da espuma). Devolve os caminhos das malhas (pro Python salvar); vazio se não há corpo. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FString> SpawnWaterBodies(UMaterialInterface* Material, const FString& AssetDir, const FString& MapName);

	/** Metros até a ponta mais perto do asfalto (onde ele se desfaz), num ponto perto dele (cm). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceRoadEndDistance(FVector2D PointCm);

	/** A vila do mapa montado (o JSON que o jogo lê: uma linha por peça; vazio sem vila). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString SliceVillageJson(const FString& MapName);

	/** A impressão digital da vila (hex; vazio sem vila): a etiqueta do ator WorldForgeVillage no mapa. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString SliceVillageHash();

	/** O vento do mapa montado (graus, pra onde sopra): a etiqueta wind_yaw: e o map.json da exportação. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceWindYaw();

	/** Os monólitos do kit de malhas no mapa montado: o nome da malha e o lugar (cm, girado, na escala). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void SliceMonoliths(TArray<FString>& OutMeshes, TArray<FTransform>& OutTransforms);

	/** A velocidade do vento do mapa montado (m/s). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static float SliceWindSpeed();

	/** Pontos das estradas de terra no recorte (cm, no chão), pra marcas de pneu; (0,0,−1e9) separa os trechos. */
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
	 * das esqueléticas ("SKM_X" → "X"). bNoVoxels: a cópia que nunca vira
	 * voxels de longe (forma "área"): o recorte perto do cursor não vale em voxel.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static UStaticMesh* VegetationToStaticMesh(USkeletalMesh* Source, const FString& AssetPath, bool bBakeParts, bool bNoVoxels = false);

	/**
	 * Neve num material que usa "material attributes" (o mestre das árvores do
	 * PVE, já duplicado pro projeto): a cor e a rugosidade de saída viram neve
	 * no que olha pra cima, pelo parâmetro escalar "Snow" (0 = sem neve).
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddSnowToMaterial(UMaterial* Material);

	/**
	 * Árvore transparente perto do cursor (material com "material attributes"):
	 * os pixels perto da linha câmera → ponto do chão sob o cursor somem em
	 * pontilhado (o TSR suaviza), só entre a câmera e o ponto. A sombra fica
	 * (ShadowPassSwitch). Câmera e cursor vêm da coleção MPC (TreeFadeCamera,
	 * TreeFadeCursor com o raio em cm no w; raio 0 desliga), que o jogo atualiza.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddOcclusionFadeToMaterial(UMaterial* Material, class UMaterialParameterCollection* Collection);

	/** O mesmo recorte num material comum sem máscara (as palmeiras): liga a máscara de opacidade e o modo "masked". */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool AddOcclusionFadeToOpacityMask(UMaterial* Material, class UMaterialParameterCollection* Collection);

	/** Dá nome à camada de um LandscapeLayerInfoObject (a propriedade é só leitura no Python). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void SetLayerInfoName(ULandscapeLayerInfoObject* LayerInfo, FName LayerName);

	/** Liga o Nanite numa malha importada e reconstrói (o subsistema de malha do editor não existe no commandlet).
	 *  bFoliage: preservar a área (a técnica de folhagem do Nanite): sem ela, folha
	 *  fina em fita some ao simplificar de longe (as palmeiras viravam palito na
	 *  vista do app, 07/10). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool EnableNanite(UStaticMesh* Mesh, bool bFoliage = false);

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

	/** O mapa gerado tem borda de mar (costa ou ilha): a água segue até o horizonte. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool SliceHasSea();

	/**
	 * Textura da água do recorte (RGBA8 sem compressão, 1 texel = 2 m, cobrindo
	 * SizeM metros centrados no recorte): R = distância com sinal até a linha
	 * d'água (0,5 + d/64; d > 0 na água, < 0 na terra, ±32 m), G = profundidade
	 * (m/50). A espuma da água e a areia molhada do chão leem dela. Fora do mapa
	 * gerado conta como mar fundo se o mapa tem borda de mar, terra se não.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static UTexture2D* MakeWaterInfoTexture(const FString& AssetPath, float SizeM);

	/** A direção da pista de asfalto (o asfalto do Look: as faixas de pneu ao longo dela), na grade da
	 *  textura de informação (FWorldDetail::RoadDirPixels). Sem mapa, 16×16 de θ = 0. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static UTexture2D* MakeRoadDirTexture(const FString& AssetPath, float SizeM);

	/** Esquece o mapa gerado do recorte anterior (antes de montar outro). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void ResetSliceMap();

	/** Depuração: gera o mapa dos controles carregados (LoadWorldSettings), sem
	 *  montar nada, e grava em Folder imagens de 1 m por pixel do mapa inteiro:
	 *  relevo sombreado, bacias, leitos secos, pesos das camadas do chão. Pra
	 *  saber o que é cada coisa vista nas fotos. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static bool DebugSliceMaps(const FString& Folder);

	/** Depuração: os pontos sorteados (m) de um tipo do espalhamento no mapa em
	 *  curso (depois de DebugSliceMaps), já sem os apagados à mão. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static TArray<FVector2D> SliceCandidates(FName Kind, float SpacingM);

	/** Liga o Nanite do Landscape e monta a malha (chamar depois do material final). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void BuildLandscapeNanite(ALandscape* Landscape);

	/** Troca o material do Landscape (a instância com a textura da água deste mapa). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static void SetLandscapeMaterial(ALandscape* Landscape, UMaterialInterface* Material);

	/** A medida das camadas do chão por componente (14/10, a otimização): quantas camadas cada componente carrega
	 *  no shader e quantas têm peso máximo desprezível ali (a camada alocada entra inteira no material). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString LandscapeLayerReport(ALandscape* Landscape);

	/** A estrada com volume (14/10, v3): a laje de asfalto de 12 cm das estradas de asfalto, em ladrilhos Nanite de
	 *  ~31 m (SM_RoadSlab_<mapa>_<n>), com a borda quebrada em placas onde a estrada é gasta ou destruída (o kit do
	 *  Look, look_ground.road_slab). Só com a laje ligada no Setup (ROAD_SLAB=1). Devolve o resumo. */
	/** A laje no ponto (cm): (o desgaste 0..1, metros até a beira com sinal, a areia encostada em m); X < 0 fora do
	 *  asfalto. Pra escolher as vistas das fotos (a inteira, a gasta, a destruída). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FVector SliceSlabAt(FVector2D PointCm);

	/** A medida das dunas (o estudo do WORLDFORGE.md §12.7): a inclinação a cada 0,5 m nas barcanas. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString SliceDuneStats();

	/** A posição na pista em 16 bits (RoadFramePixels): pra faixa pintada nova na laje. */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static UTexture2D* MakeRoadFrameTexture(const FString& AssetPath, float SizeM);

	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString BuildRoadSlabs(UMaterialInterface* Material, const FString& AssetDir, const FString& MapName);

	/** A mesma medida sem montar o Landscape: os pesos da montagem (o mesmo Setup), por componente, e quantas
	 *  camadas sobram com cada limite da limpeza (LAYER_CULL na montagem). */
	/** O HLSL que a Unreal gera do material (o Material.ush, sem compilar): pra copiar uma conta da engine
	 *  idêntica num nó Custom (14/10, a pedra no desvio dinâmico). */
	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString MaterialHlsl(UMaterialInterface* Material);

	UFUNCTION(BlueprintCallable, Category = "WorldForge")
	static FString SliceLayerStats(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
		int32 Components, int32 QuadsPerComponent, float QuadSizeM, int32 NumLayers, float RoadWidthM);
};
