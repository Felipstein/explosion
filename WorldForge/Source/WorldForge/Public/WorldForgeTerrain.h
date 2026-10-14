#pragma once

#include "GameFramework/Actor.h"
#include "Generation/WorldMap.h"
#include "Generation/WorldDetail.h"
#include "WorldForgeTerrain.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UStaticMesh;

/**
 * Desenha um FWorldMap: malha do terreno (pedaços de 64×64 células, pra a
 * câmera descartar o que está fora da tela), água, objetos instanciados
 * (árvores, rochas, cristais de recurso, marcadores de base) e as camadas de
 * depuração (passabilidade, territórios).
 *
 * As camadas do chão vão pro material numa textura (DataMap) do tamanho da
 * grade: o material mistura grama/terra/areia/rocha por ela.
 */
UCLASS()
class WORLDFORGE_API AWorldForgeTerrain : public AActor
{
	GENERATED_BODY()

public:
	AWorldForgeTerrain();

	/** Troca o mapa mostrado (no game thread; a geração em si roda fora). Com
	 *  InLive, o chão e a água de verdade (os do "qualidade real"). */
	void ShowMap(TSharedPtr<const FWorldMap> InMap, TSharedPtr<struct FWorldLive> InLive = nullptr);
	const FWorldMap* GetMap() const { return Map.Get(); }
	TSharedPtr<const FWorldMap> GetMapShared() const { return Map; }

	/** Raio (m) contra o terreno e a água; devolve o ponto (m). */
	bool RayHit(const FVector& OriginM, const FVector& DirM, FVector& OutPointM) const;

	/** Camadas de depuração: 0 nada, 1 passabilidade, 2 territórios. */
	void SetOverlay(int32 Mode);
	int32 GetOverlay() const { return Overlay; }

	/** Muda a cada mapa mostrado (a transparência das árvores varre de novo). */
	int32 GetSceneVersion() const { return SceneVersion; }

	/** Uma peça do espalhamento de verdade tirada da vista (pra desfazer). */
	struct FRemoved
	{
		int32 Component = INDEX_NONE;
		FTransform Xform;
		FString Kind;
		FVector2f Origin = FVector2f::ZeroVector;   // o ponto sorteado (m): a chave da edição
	};
	/** Tira a peça mais perto de P (m) dentro de RadiusM; as grandes (árvore,
	 *  pedra) ganham das miúdas quando o clique cai na copa ou na pedra. */
	bool RemoveNearest(FVector2f P, float RadiusM, FRemoved& Out);
	/** Devolve uma peça tirada (desfazer). */
	void Restore(const FRemoved& R);

	/** Uma peça pega pra mover: onde estava quando foi pega. */
	struct FPicked
	{
		int32 Component = INDEX_NONE;
		FString Kind;
		FVector2f Origin = FVector2f::ZeroVector;   // o ponto sorteado (a chave da edição)
		FTransform Start;
		FVector2f Pivot = FVector2f::ZeroVector;    // o lugar dela (m): o centro da pedra, o pé da árvore
		float RadiusCm = 100.0f;                    // o tamanho dela (o círculo que marca a peça)
	};
	/** Pega a peça mais perto de P (m), como o apagar. */
	bool PickPiece(FVector2f P, float RadiusM, FPicked& Out, const TCHAR* KindPrefix = nullptr) const;
	/** Põe a peça pega com o lugar em To (m), girada YawDeg desde que foi pega
	 *  (assenta pela diferença de altura do chão). */
	void PlacePicked(const FPicked& Pk, FVector2f To, float YawDeg);
	/** Volta a peça ao transform dado (desfazer). */
	void SetPieceTransform(int32 Component, FVector2f Origin, const FTransform& X);
	/** Troca as peças com o ponto sorteado dentro de Region (m) pelas de New
	 *  (FWorldLive::ScatterRegion): a vista depois de pintar ou mexer na altura. */
	void ReplaceLiveRegion(const FBox2f& Region, const TArray<FWorldLive::FKind>& New);
	/** Teste: a peça do tipo (prefixo) mais perto do centro do mapa (cm) e o total de peças. */
	bool FindLivePiece(const FString& KindPrefix, FVector& OutCm) const;
	/** O mundo de verdade em uso (a pintura ao vivo mexe nas edições dele). */
	struct FWorldLive* GetLive() const { return Live.Get(); }
	/** Refaz os pesos do chão de um retângulo (m) e manda pra textura (pintura ao vivo). */
	void RepaintRegion(FVector2f MinM, FVector2f MaxM);
	/** Refaz a malha do chão nos pedaços que tocam os vértices I0..I1, J0..J1 (altura à mão). */
	void UpdateGroundRegion(int32 I0, int32 J0, int32 I1, int32 J1);
	/** Sobe/desce as peças do espalhamento num retângulo (m) pela mudança de altura. */
	void LiftPieces(FVector2f MinM, FVector2f MaxM, TFunctionRef<float(FVector2f)> DeltaAtM);
	int32 LiveInstanceCount() const;

	/** Minimapa (cores do bioma + relevo + água + bases + recursos), pro painel. */
	UTexture2D* GetMinimap() const { return Minimap; }
	const TArray<FColor>& GetMinimapPixels() const { return MinimapPixels; }
	static constexpr int32 MinimapSize = 256;

	UPROPERTY(VisibleAnywhere, Category = "WorldForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "WorldForge")
	TObjectPtr<UProceduralMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Category = "WorldForge")
	TObjectPtr<UStaticMeshComponent> WaterPlane;

	/** A água só onde existe água (13/10, docs/WORLDFORGE.md §12.3, a P1): uma seção por corpo, a mesma
	 *  conta do "qualidade real" (FWorldDetail::WaterBodyCells); sem a vista ao vivo, o plano de antes. */
	UPROPERTY(VisibleAnywhere, Category = "WorldForge")
	TObjectPtr<UProceduralMeshComponent> WaterBodies;

private:
	void BuildGroundMesh();
	void BuildDataTextures();
	void BuildProps();
	void BuildMinimap();
	UHierarchicalInstancedStaticMeshComponent* MakeInstances(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, bool bShadows);

	/** O material do chão em uso: o de verdade (ao vivo) ou o rascunho. */
	UMaterialInterface* GroundMaterial() const;
	void BuildLiveMaterials();
	/** Pedras, árvores, palmeiras, capim e paredões de verdade (FWorldLive::Scatter). */
	void BuildLiveScatter();
	/** A vila (FWorldDetail::GVillage) em caixas provisórias: casa, muro, portão,
	 *  carro, poste, miudeza, cada uma do tamanho e da altura da peça do kit; as
	 *  malhas do Look entram no lugar quando existirem. */
	void BuildVillage();
	/** Os monólitos do kit de malhas (FWorldMap::Monoliths): a malha de /Game/Slice/Monoliths no lugar. */
	void BuildMonoliths();
	/** As seções d'água por corpo (WaterBodies); falso sem a vista ao vivo ou sem corpo (fica o plano). */
	bool BuildWaterBodies(UMaterialInterface* Material);
	bool HasLiveScatter() const;
	int32 SceneVersion = 0;

	TSharedPtr<const FWorldMap> Map;
	TSharedPtr<struct FWorldLive> Live;
	int32 Overlay = 0;
	TArray<FColor> MinimapPixels;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMID;

	// a vista ao vivo: o chão (M_LiveGround_<bioma>) e a água (M_Ocean_<bioma>)
	// de verdade, com as texturas do mapa feitas na geração (FWorldLive)
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiveGroundMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiveWaterMID;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WeightsATex;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WeightsBTex;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WaterInfoTex;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DataMap;       // camadas do chão (RGBA8)

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> OverlayMap;    // passabilidade / territórios

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Minimap;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PropComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PropMeshes;   // geradas em código (WorldForgeMeshes)

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> LiveComponents;   // o espalhamento de verdade, em blocos de 128 m

	/** A peça do espalhamento mais perto de P (componente e instância). */
	bool FindPiece(FVector2f P, float RadiusM, int32& OutComponent, int32& OutInstance, const TCHAR* KindPrefix = nullptr) const;

	/** Por componente de LiveComponents: o tipo, o raio da malha (cm) e o ponto sorteado de cada instância. */
	struct FLiveInfo
	{
		FString Kind;
		float Radius = 100.0f;
		TArray<FVector2f> Origins;
		int32 KindIndex = 0, MeshIndex = 0;   // em FWorldLive::Scatter
		FIntPoint Cell = FIntPoint::ZeroValue; // o bloco de 128 m
	};
	TArray<FLiveInfo> LiveInfo;
	TMap<FIntVector, int32> LiveCellIndex;   // (tipo·64 + malha, bloco) → componente
	static FIntPoint LiveCellOf(const FVector& Cm);
	/** Um componente vazio do tipo/malha no bloco; devolve o índice (INDEX_NONE sem a malha). */
	int32 MakeLiveComponent(int32 KindIndex, int32 MeshIndex, FIntPoint Cell);
	/** Põe estas peças no componente (troca as que havia). */
	void FillLiveComponent(int32 Component, const TArray<FTransform>& Xforms, TArray<FVector2f>&& Origins);
};

/** A peça pega pela ferramenta Mover (o modo de jogo só declara o nome). */
struct FMovePick
{
	AWorldForgeTerrain::FPicked Piece;
};
