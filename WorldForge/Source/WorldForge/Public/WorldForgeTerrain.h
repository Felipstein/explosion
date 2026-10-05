#pragma once

#include "GameFramework/Actor.h"
#include "Generation/WorldMap.h"
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

	/** Troca o mapa mostrado (no game thread; a geração em si roda fora). */
	void ShowMap(TSharedPtr<const FWorldMap> InMap);
	const FWorldMap* GetMap() const { return Map.Get(); }
	TSharedPtr<const FWorldMap> GetMapShared() const { return Map; }

	/** Raio (m) contra o terreno e a água; devolve o ponto (m). */
	bool RayHit(const FVector& OriginM, const FVector& DirM, FVector& OutPointM) const;

	/** Camadas de depuração: 0 nada, 1 passabilidade, 2 territórios. */
	void SetOverlay(int32 Mode);
	int32 GetOverlay() const { return Overlay; }

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

private:
	void BuildGroundMesh();
	void BuildDataTextures();
	void BuildProps();
	void BuildMinimap();
	UHierarchicalInstancedStaticMeshComponent* MakeInstances(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, bool bShadows);

	TSharedPtr<const FWorldMap> Map;
	int32 Overlay = 0;
	TArray<FColor> MinimapPixels;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WaterMID;

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
};
