#include "ForgePlayerController.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ForgeCameraPawn.h"
#include "ForgeDebugView.h"
#include "WorldForgeTerrain.h"
#include "SForgePanel.h"
#include "Generation/WorldDetail.h"
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
	// edição à mão: com a ferramenta "apagar", o clique esquerdo no mapa (fora do
	// painel) tira a peça mais perto; Ctrl+Z desfaz
	if (AWorldForgeGameMode* GM = GetWorld()->GetAuthGameMode<AWorldForgeGameMode>())
	{
		const bool bOverPanel = Panel.IsValid() && Panel->IsCursorOver();
		if (GM->EditTool == 4)
		{
			RoadTool(GM, bOverPanel);
		}
		else if (RoadDrag != INDEX_NONE || GM->RoadDraft.Num())
		{
			RoadDrag = INDEX_NONE;   // trocou de ferramenta no meio do desenho
			GM->RoadDraft.Reset();
		}
		if (GM->EditTool == 5 || MovePick.IsValid())
		{
			MoveTool(GM, bOverPanel);
		}
		if (GM->EditTool == 1 && !bOverPanel && WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			if (AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>())
			{
				FVector P;
				if (Cam->CursorGroundPoint(P))
				{
					GM->EditDeleteAt(FVector2f(P.X, P.Y));
				}
			}
		}
		// pincel de chão: arrastando com o botão esquerdo, uma pincelada a cada
		// ~1/3 do raio andado
		if (GM->EditTool == 2 && IsInputKeyDown(EKeys::LeftMouseButton) && (!bOverPanel || bPainting))
		{
			if (AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>())
			{
				FVector P;
				if (Cam->CursorGroundPoint(P))
				{
					const FVector2f Q(P.X, P.Y);
					if (!bPainting || FVector2f::Distance(Q, LastDab) > 0.33f * GM->BrushRadius)
					{
						GM->EditPaint(Q, !bPainting);
						LastDab = Q;
						bPainting = true;
					}
				}
			}
		}
		else if (GM->EditTool == 3 && IsInputKeyDown(EKeys::LeftMouseButton) && (!bOverPanel || bPainting))
		{
			// pincel de altura: segurando, sobe/desce/aplaina contínuo
			if (AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>())
			{
				FVector P;
				if (Cam->CursorGroundPoint(P))
				{
					GM->EditHeight(FVector2f(P.X, P.Y), !bPainting, DeltaTime);
					bPainting = true;
				}
			}
		}
		else
		{
			if (bPainting)
			{
				GM->EditStrokeEnd();   // soltou o pincel: o pedaço é sorteado de novo
			}
			bPainting = false;
		}
		if ((IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl)) && WasInputKeyJustPressed(EKeys::Z))
		{
			GM->EditUndo();
		}
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

void AForgePlayerController::RoadTool(AWorldForgeGameMode* GM, bool bOverPanel)
{
	AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>();
	FWorldLive* L = GM->GetTerrain() ? GM->GetTerrain()->GetLive() : nullptr;
	if (!Cam || !L || !L->Display.IsValid())
	{
		return;
	}
	const FWorldMap& D = *L->Display;
	FVector Hit = FVector::ZeroVector;
	const bool bHit = !bOverPanel && Cam->CursorGroundPoint(Hit);
	const FVector2f Q(Hit.X, Hit.Y);
	auto At = [&D](FVector2f P, float Up) { return FVector(P.X * 100.0, P.Y * 100.0, (D.HeightAt(P) + Up) * 100.0); };
	// o ponto de controle sob o mouse (até 14 px na tela)
	auto HandleUnderMouse = [&]() -> int32
	{
		float MX, MY;
		if (bOverPanel || !GetMousePosition(MX, MY)) return INDEX_NONE;
		int32 Best = INDEX_NONE;
		double BestD = 14.0;
		const TArray<FVector2f>& H = GM->GetRoadHandles();
		for (int32 K = 0; K < H.Num(); ++K)
		{
			FVector2D S;
			if (!ProjectWorldLocationToScreen(At(H[K], 0.8f), S)) continue;
			const double Dist = FVector2D::Distance(S, FVector2D(MX, MY));
			if (Dist < BestD) { BestD = Dist; Best = K; }
		}
		return Best;
	};

	// enquanto o mundo é refeito (~4 s) o clique não mexe: a edição iria pro mundo de antes
	if (!GM->IsGenerating())
	{
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHit)
		{
			const double Now = GetWorld()->GetRealTimeSeconds();
			if (GM->RoadDraft.Num() > 0)
			{
				// desenhando: o clique põe um ponto; o duplo clique no último termina
				const bool bDouble = Now - LastRoadClick < 0.35 && FVector2f::Distance(Q, GM->RoadDraft.Last()) < 4.0f;
				if (bDouble) GM->RoadFinishDraft();
				else if (FVector2f::Distance(Q, GM->RoadDraft.Last()) > 1.0f) GM->RoadDraft.Add(Q);
			}
			else if ((RoadDrag = HandleUnderMouse()) != INDEX_NONE)
			{
				RoadDragPos = GM->GetRoadHandles()[RoadDrag];
			}
			else if (!GM->RoadSelectAt(Q))
			{
				// no chão: começa uma estrada nova
				GM->RoadDeselect();
				GM->RoadDraft.Add(Q);
			}
			LastRoadClick = Now;
		}
		if (RoadDrag != INDEX_NONE)
		{
			if (bHit) RoadDragPos = Q;
			if (!IsInputKeyDown(EKeys::LeftMouseButton))
			{
				const TArray<FVector2f>& H = GM->GetRoadHandles();
				if (H.IsValidIndex(RoadDrag) && FVector2f::Distance(RoadDragPos, H[RoadDrag]) > 0.5f)
				{
					GM->RoadMoveHandle(RoadDrag, RoadDragPos);
				}
				RoadDrag = INDEX_NONE;
			}
		}
		if (WasInputKeyJustPressed(EKeys::Enter)) GM->RoadFinishDraft();
		if (WasInputKeyJustPressed(EKeys::BackSpace) && GM->RoadDraft.Num()) GM->RoadDraft.Pop();
		if (WasInputKeyJustPressed(EKeys::Escape))
		{
			if (GM->RoadDraft.Num()) GM->RoadDraft.Reset(); else GM->RoadDeselect();
		}
		if (WasInputKeyJustPressed(EKeys::Delete) && GM->HasRoadSelection()) GM->RoadDeleteSelected();
	}

	// o desenho: por cima de tudo (SDPG_Foreground), largura que acompanha a distância
	const float Thick = FMath::Max(15.0f, 0.25f * Cam->GetDistance());
	auto Line = [&](const TArray<FVector2f>& Pts, const FColor& C, int32 Every)
	{
		for (int32 K = Every; K < Pts.Num() + Every - 1; K += Every)
		{
			const int32 B = FMath::Min(K, Pts.Num() - 1);
			DrawDebugLine(GetWorld(), At(Pts[K - Every], 0.6f), At(Pts[B], 0.6f), C, false, -1.0f, SDPG_Foreground, Thick);
		}
	};
	const FColor Selected(255, 205, 60), Dragging(255, 135, 40), Draft(90, 215, 255);
	if (GM->HasRoadSelection())
	{
		TArray<FVector2f> H = GM->GetRoadHandles();
		const bool bDragging = RoadDrag != INDEX_NONE && H.IsValidIndex(RoadDrag);
		if (bDragging)
		{
			// arrastando: o traçado novo (o mesmo suavizado que a estrada vai ter)
			H[RoadDrag] = RoadDragPos;
			Line(FWorldDetail::UserRoadLine(H), Dragging, 3);
		}
		else if (const TArray<FVector2f>* Real = GM->SelectedRoadLine())
		{
			Line(*Real, Selected, 3);
		}
		else
		{
			Line(FWorldDetail::UserRoadLine(H), Selected, 3);   // o mundo sendo refeito: a prévia
		}
		const int32 Hover = bDragging ? RoadDrag : HandleUnderMouse();
		for (int32 K = 0; K < H.Num(); ++K)
		{
			DrawDebugPoint(GetWorld(), At(H[K], 0.8f), K == Hover ? 20.0f : 13.0f, K == Hover ? Dragging : FColor::White, false, -1.0f, SDPG_Foreground);
		}
	}
	if (GM->RoadDraft.Num())
	{
		TArray<FVector2f> P = GM->RoadDraft;
		if (bHit && FVector2f::Distance(Q, P.Last()) > 1.0f) P.Add(Q);
		Line(FWorldDetail::UserRoadLine(P), Draft, 3);
		for (const FVector2f& X : GM->RoadDraft)
		{
			DrawDebugPoint(GetWorld(), At(X, 0.8f), 13.0f, Draft, false, -1.0f, SDPG_Foreground);
		}
	}
}

void AForgePlayerController::MoveTool(AWorldForgeGameMode* GM, bool bOverPanel)
{
	AForgeCameraPawn* Cam = GetPawn<AForgeCameraPawn>();
	AWorldForgeTerrain* T = GM->GetTerrain();
	if (!Cam || !T || !T->GetMap())
	{
		MovePick.Reset();
		return;
	}
	const FWorldMap& D = *T->GetMap();
	FVector Hit = FVector::ZeroVector;
	const bool bHit = !bOverPanel && Cam->CursorGroundPoint(Hit);
	const FVector2f Q(Hit.X, Hit.Y);
	auto Ring = [this, &D](FVector2f P, float RadiusCm, const FColor& C)
	{
		const FVector Center(P.X * 100.0, P.Y * 100.0, (D.HeightAt(P) + 0.3f) * 100.0);
		DrawDebugCircle(GetWorld(), Center, RadiusCm, 32, C, false, -1.0f, SDPG_Foreground, FMath::Max(8.0f, 0.004f * RadiusCm * 25.0f), FVector(1, 0, 0), FVector(0, 1, 0), false);
	};
	if (!MovePick.IsValid())
	{
		// a peça sob o mouse ganha um círculo; o clique pega
		if (GM->EditTool != 5 || !bHit || GM->IsGenerating()) return;
		FMovePick P;
		if (!T->PickPiece(Q, 2.0f, P.Piece)) return;
		Ring(P.Piece.Pivot, P.Piece.RadiusCm, FColor(255, 255, 255));
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			MovePick = MakeShared<FMovePick>(P);
			MoveGrab = P.Piece.Pivot - Q;
			MoveTo = P.Piece.Pivot;
			MoveYaw = 0.0f;
		}
		return;
	}
	// arrastando: a peça segue o mouse (com a mesma folga do clique), R gira 15°
	if (bHit) MoveTo = Q + MoveGrab;
	if (WasInputKeyJustPressed(EKeys::R))
	{
		MoveYaw += (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)) ? -15.0f : 15.0f;
	}
	T->PlacePicked(MovePick->Piece, MoveTo, MoveYaw);
	Ring(MoveTo, MovePick->Piece.RadiusCm, FColor(255, 135, 40));
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		if (FVector2f::Distance(MoveTo, MovePick->Piece.Pivot) > 0.05f || MoveYaw != 0.0f)
		{
			GM->EditMovePiece(*MovePick, MoveTo, MoveYaw);
		}
		MovePick.Reset();
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
