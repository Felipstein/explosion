#pragma once

#include "GameFramework/Actor.h"
#include "ForgeDebugView.generated.h"

class SWidget;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;

/**
 * Modo teste de qualidade (F2, no gerador e nos mapas): mostra o que a Unreal
 * economiza conforme a câmera se afasta.
 *
 * - Dois quadros no canto, da mesma câmera fixa perto do chão (no ponto que
 *   a câmera principal olhava ao ligar):
 *   "perto": o detalhe que aquele ponto tem visto de perto;
 *   "da sua câmera": o mesmo enquadramento com os pixels que a câmera PRINCIPAL
 *   gasta naquele ponto (resolução ÷ distância da principal ÷ distância do
 *   quadro), ampliado — e com o LOD dessa distância (LODDistanceFactor). É o que
 *   a Unreal precisa desenhar de longe: o Nanite e o streaming de texturas
 *   cortam o que cabe em menos de um pixel, por isso ninguém percebe.
 * - O que a câmera principal NÃO desenha some dos quadros: cada bloco de
 *   terreno e cada bloco de 64 m de árvores/pedras/capim é testado contra a
 *   vista principal (fora do campo de visão, ou além da distância de corte ×
 *   r.ViewDistanceScale) e escondido nos quadros (HiddenComponents). De costas
 *   pro ponto, ele some. É o descarte por bloco (CPU); o Nanite ainda descarta
 *   instância por instância e o que fica atrás de outra coisa, na placa.
 * - Mapa pequeno: o mapa todo de cima, os blocos de terreno em verde
 *   (desenhados) e cinza (não), o trapézio da vista e a posição do quadro.
 */
UCLASS()
class WORLDFORGE_API AForgeDebugView : public AActor
{
	GENERATED_BODY()

public:
	AForgeDebugView();

	/** Liga: o quadro no ponto (cm) que a câmera olha, virado como ela; o mapa com os limites dados (cm). */
	void Setup(const FVector& Target, float YawDeg, const FBox2D& MapBounds);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Probe;
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> ProbeFar;
	UPROPERTY()
	TArray<TObjectPtr<UTextureRenderTarget2D>> FarTargets;   // 480, 240, 120, 60, 30 de largura
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Overview;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ProbeTarget;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> OverviewTarget;

	TSharedPtr<SWidget> Overlay;
	TSharedPtr<struct FSlateBrush> ProbeBrush;
	TSharedPtr<struct FSlateBrush> FarBrush;
	TSharedPtr<struct FSlateBrush> OverviewBrush;
	FVector TargetPoint = FVector::ZeroVector;
	FBox2D Bounds = FBox2D(ForceInit);
	TArray<FVector2D> Frustum;     // trapézio no mapa pequeno (0–1)
	FVector2D ProbeMark = FVector2D::ZeroVector;
	FString Status;
	double OverviewWait = 0.0;
	double ProbeWait = 0.0;

	// os blocos da cena: componente, se é terreno, distância de corte (cm, 0 = nenhuma)
	struct FBlock
	{
		TWeakObjectPtr<class UPrimitiveComponent> Comp;
		bool bTerrain = false;
		float CullCm = 0.0f;
	};
	TArray<FBlock> Blocks;
	TArray<FBox2D> TerrainBoxes;   // no mapa pequeno (0–1)
	TArray<bool> TerrainDrawn;
	int32 NumDrawn = 0;
};
