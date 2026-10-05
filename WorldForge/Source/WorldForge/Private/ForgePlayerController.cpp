#include "ForgePlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "ForgeCameraPawn.h"
#include "SForgePanel.h"
#include "WorldForgeGameMode.h"

AForgePlayerController::AForgePlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AForgePlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	AWorldForgeGameMode* GM = GetWorld()->GetAuthGameMode<AWorldForgeGameMode>();
	if (!GM || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	Panel = SNew(SForgePanel).GameMode(GM);
	GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(), 10);
	if (AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>())
	{
		TWeakPtr<SForgePanel> Weak = Panel;
		Cam->IsOverUI = [Weak]()
		{
			const TSharedPtr<SForgePanel> P = Weak.Pin();
			return P.IsValid() && P->IsCursorOver();
		};
	}
}

void AForgePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Panel.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	}
	Panel.Reset();
	Super::EndPlay(Reason);
}

void AForgePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// F1 esconde/mostra o painel (pra olhar o mapa limpo)
	if (Panel.IsValid() && WasInputKeyJustPressed(EKeys::F1))
	{
		Panel->SetVisibility(Panel->GetVisibility() == EVisibility::Collapsed ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	}
}
