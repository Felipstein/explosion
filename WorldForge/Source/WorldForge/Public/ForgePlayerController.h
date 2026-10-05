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
	TSharedPtr<SForgePanel> Panel;
};
