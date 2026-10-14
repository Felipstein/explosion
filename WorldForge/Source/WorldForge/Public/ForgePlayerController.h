#pragma once

#include "GameFramework/PlayerController.h"
#include "ForgePlayerController.generated.h"

class SForgePanel;

/** Cursor visível, entrada de jogo + interface, e o painel do gerador. */
UCLASS()
class WORLDFORGE_API AForgePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AForgePlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PlayerTick(float DeltaTime) override;

	TSharedPtr<SForgePanel> GetPanel() const { return Panel; }

private:
	bool bPainting = false;           // arrastando o pincel de chão
	FVector2f LastDab = FVector2f::ZeroVector;
	// ferramenta de estrada: o ponto de controle sendo arrastado e o último clique
	int32 RoadDrag = INDEX_NONE;
	FVector2f RoadDragPos = FVector2f::ZeroVector;
	double LastRoadClick = -1.0;
	/** Cliques, teclas e o desenho (traçado, pontos) da ferramenta de estrada. */
	void RoadTool(class AWorldForgeGameMode* GM, bool bOverPanel);
	// ferramenta mover: a peça pega, o quanto o clique ficou fora do lugar dela,
	// o lugar novo e o giro (R)
	TSharedPtr<struct FMovePick> MovePick;
	FVector2f MoveGrab = FVector2f::ZeroVector;
	FVector2f MoveTo = FVector2f::ZeroVector;
	float MoveYaw = 0.0f;
	void MoveTool(class AWorldForgeGameMode* GM, bool bOverPanel);

public:
	/** Liga/desliga o modo teste de qualidade no ponto que a câmera olha (F2). */
	void ToggleDebugView();

private:
	TSharedPtr<SForgePanel> Panel;
	UPROPERTY()
	TObjectPtr<class AForgeDebugView> DebugView;   // F2: modo teste de qualidade
};
