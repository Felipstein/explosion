#include "ForgePlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ForgeCameraPawn.h"
#include "ForgeDebugView.h"
#include "WorldForgeTerrain.h"
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
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.Visualize")))
	{
		V->Set(TEXT("off"), ECVF_SetByConsole);   // não leva a visualização pro próximo mapa
	}
	if (DebugView)
	{
		DebugView->Destroy();
		DebugView = nullptr;
	}
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
	// F2: modo teste de qualidade (ForgeDebugView.h) no ponto que a câmera olha
	if (WasInputKeyJustPressed(EKeys::F2))
	{
		ToggleDebugView();
	}
	// F3: os grupos de triângulos do Nanite (cada cor ~128 triângulos): de perto um
	// galho é um grupo, a 300 m uma árvore inteira é meia dúzia — é isso que a
	// Unreal reduz de longe (Clusters lê melhor que Triangles, testado em 06/10)
	if (WasInputKeyJustPressed(EKeys::F3))
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.Visualize")))
		{
			const bool bOn = V->GetString() == TEXT("Clusters");
			V->Set(bOn ? TEXT("off") : TEXT("Clusters"), ECVF_SetByConsole);
		}
	}
}

void AForgePlayerController::ToggleDebugView()
{
	if (DebugView)
	{
		DebugView->Destroy();
		DebugView = nullptr;
	}
	else if (AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>())
	{
		FBox Box(ForceInit);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const FString Class = It->GetClass()->GetName();
			if (Class.Contains(TEXT("Landscape")) || It->IsA<AWorldForgeTerrain>())
			{
				Box += It->GetComponentsBoundingBox();
			}
		}
		if (!Box.IsValid) Box = FBox(FVector(-50000), FVector(50000));
		const FVector2f F = Cam->GetFocus();
		DebugView = GetWorld()->SpawnActor<AForgeDebugView>();
		DebugView->Setup(FVector(F.X * 100.0, F.Y * 100.0, Cam->GetFocusZ() * 100.0), Cam->GetYaw(),
			FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y)));
	}
}
