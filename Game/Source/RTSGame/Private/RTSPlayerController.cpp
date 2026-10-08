#include "RTSPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "RTSCameraPawn.h"
#include "RTSGameData.h"
#include "RTSGameModes.h"
#include "RTSHUD.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSSessionSubsystem.h"
#include "Scalability.h"

namespace
{
	constexpr float DragThresholdPx = 6.0f;
	constexpr double DoubleClickSeconds = 0.3;
	// Câmera (valores do Vanguard, ADR-0003, que o Felipe testou e aprovou em 07/2026)
	constexpr float EdgeScrollPx = 16.0f;
	constexpr float EdgeScrollBoost = 1.6f;
	constexpr float VerticalScrollBias = 1.25f;
	constexpr float DragRefPx = 180.0f;          // arrasto do direito: 180 px = velocidade normal
	constexpr float DragMaxFactor = 2.5f;
	constexpr float RotateDegPerSec = 140.0f;    // setas e botão do meio
	constexpr float RotateRefPx = 180.0f;
	constexpr float RotateMaxFactor = 2.5f;
	constexpr float KeyboardZoomStepsPerSec = 6.0f;
	constexpr float MarkerSeconds = 0.7f;
	constexpr float PickHeightCm = 150.0f;

	/** As teclas 1–9 (dentro de função: as FKey do InputCore são estáticas de outro módulo). */
	const FKey& GroupKey(int32 K)
	{
		static const FKey Keys[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
		return Keys[K];
	}
}

ARTSPlayerController::ARTSPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ARTSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bMenu = !GetWorld()->GetAuthGameMode() || GetWorld()->GetAuthGameMode()->IsA<ARTSMenuGameMode>();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	// na partida o cursor fica preso na janela, pra rolar pela borda (como no ZH)
	Mode.SetLockMouseToViewportBehavior(bMenu ? EMouseLockMode::DoNotLock : EMouseLockMode::LockAlways);
	SetInputMode(Mode);
	if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn()))
	{
		Cam->SetMenuMode(bMenu);
	}
}

URTSMatchSubsystem* ARTSPlayerController::Match() const
{
	URTSMatchSubsystem* M = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	return M && M->IsRunning() ? M : nullptr;
}

ARTSHUD* ARTSPlayerController::GetRTSHUD() const
{
	return Cast<ARTSHUD>(GetHUD());
}

bool ARTSPlayerController::IsDragging(FVector2D& OutA, FVector2D& OutB) const
{
	float X = 0.0f;
	float Y = 0.0f;
	if (!bLeftDown || !GetMousePosition(X, Y))
	{
		return false;
	}
	OutA = LeftDownPos;
	OutB = FVector2D(X, Y);
	return FVector2D::Distance(OutA, OutB) > DragThresholdPx;
}

void ARTSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (bMenu)
	{
		TickMenu();
	}
	else
	{
		TickMatch(DeltaTime);
	}
	// presets (ferramenta de teste; pro jogador eles vão pra tela de opções)
	const FKey QualityKeys[4] = { EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8 };
	for (int32 Q = 0; Q < 4; ++Q)
	{
		if (WasInputKeyJustPressed(QualityKeys[Q]))
		{
			ApplyQuality(Q);
		}
	}
	if (WasInputKeyJustPressed(EKeys::F10))
	{
		bShowDebug = !bShowDebug;
	}
}

void ARTSPlayerController::ApplyQuality(int32 Level)
{
	// os mesmos níveis do BlastLab: baixa/média/alta/ultra = Médio/Alto/Épico/Cinematic do motor
	static const int32 EngineLevel[4] = { 1, 2, 3, 4 };
	static const float ResolutionPct[4] = { 50.0f, 58.0f, 67.0f, 100.0f };
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(EngineLevel[Level]);
	Levels.ResolutionQuality = ResolutionPct[Level];
	Scalability::SetQualityLevels(Levels);
}

void ARTSPlayerController::OnButton(FName Id)
{
	URTSSessionSubsystem* Session = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>();
	if (Id == TEXT("skirmish"))
	{
		Session->StartSkirmish();
	}
	else if (Id == TEXT("quit"))
	{
		Session->Quit(this);
	}
	else if (Id == TEXT("resume"))
	{
		bPauseMenu = false;
		if (URTSMatchSubsystem* M = Match())
		{
			M->SetPaused(false);
		}
	}
	else if (Id == TEXT("menu"))
	{
		Session->ReturnToMenu();
	}
}

void ARTSPlayerController::TickMenu()
{
	float X = 0.0f;
	float Y = 0.0f;
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && GetMousePosition(X, Y))
	{
		if (ARTSHUD* HUD = GetRTSHUD())
		{
			const FName Id = HUD->ButtonAt(FVector2D(X, Y));
			if (!Id.IsNone())
			{
				OnButton(Id);
			}
		}
	}
}

void ARTSPlayerController::TickMatch(float DeltaTime)
{
	URTSMatchSubsystem* M = Match();
	ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn());
	float MX = 0.0f;
	float MY = 0.0f;
	const bool bHasMouse = GetMousePosition(MX, MY);
	const FVector2D Mouse(MX, MY);

	for (int32 I = Markers.Num() - 1; I >= 0; --I)
	{
		Markers[I].Age += DeltaTime;
		if (Markers[I].Age > MarkerSeconds)
		{
			Markers.RemoveAt(I);
		}
	}

	const bool bOver = M && M->Result() != ERTSMatchResult::Playing;
	if (WasInputKeyJustPressed(EKeys::Escape) && !bOver)
	{
		bPauseMenu = !bPauseMenu;
		if (M)
		{
			M->SetPaused(bPauseMenu);
		}
	}
	if (bPauseMenu || bOver || !M)
	{
		bLeftDown = false;
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse)
		{
			if (ARTSHUD* HUD = GetRTSHUD())
			{
				const FName Id = HUD->ButtonAt(Mouse);
				if (!Id.IsNone())
				{
					OnButton(Id);
				}
			}
		}
		if (Cam && !bPauseMenu)
		{
			TickCamera(DeltaTime, Cam);   // no fim dá pra olhar o campo
		}
		return;
	}

	if (Cam)
	{
		TickCamera(DeltaTime, Cam);
	}
	PruneSelection();

	if (WasInputKeyJustPressed(EKeys::A) && !Selected.IsEmpty())
	{
		bAttackMoveArmed = true;
	}
	if (WasInputKeyJustPressed(EKeys::S) && !Selected.IsEmpty())
	{
		M->OrderStop(Selected);
		++Orders;
	}
	HandleGroups();

	// o que está sob o mouse: o anel acende; o cursor diz o que o clique vai fazer (ZH)
	HoverId = (bHasMouse && !bLeftDown) ? PickUnit(Mouse) : 0;
	EMouseCursor::Type Cursor = EMouseCursor::Default;
	FRTSUnitState Hovered;
	if (bAttackMoveArmed)
	{
		Cursor = EMouseCursor::Crosshairs;
	}
	else if (HoverId != 0 && M->GetUnit(HoverId, Hovered))
	{
		const bool bEnemy = M->IsEnemyOfLocal(Hovered.Owner);
		Cursor = bEnemy ? (Selected.IsEmpty() ? EMouseCursor::Default : EMouseCursor::Crosshairs) : EMouseCursor::Hand;
	}
	CurrentMouseCursor = Cursor;

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse)
	{
		bLeftDown = true;
		LeftDownPos = Mouse;
	}
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton) && bLeftDown)
	{
		bLeftDown = false;
		if (FVector2D::Distance(LeftDownPos, Mouse) > DragThresholdPx)
		{
			HandleBoxSelect(LeftDownPos, Mouse);
		}
		else
		{
			HandleLeftClick(Mouse);
		}
	}
}

void ARTSPlayerController::TickCamera(float DeltaTime, ARTSCameraPawn* Cam)
{
	ARTSCameraPawn::FInput In;
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	float MX = 0.0f;
	float MY = 0.0f;
	const bool bHasMouse = GetMousePosition(MX, MY);
	const FVector2D Mouse(MX, MY);

	// setas (ADR-0003): ↑↓ rolam, ←→ giram; A e S são ordens, então não há WASD
	In.PanAxis.Y = (IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);
	In.YawDelta = ((IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f)) * RotateDegPerSec * DeltaTime;
	if (bEdgeScroll && bHasMouse && W > 0 && H > 0 && !bRightScrolling && !bMiddleDown)
	{
		// a borda rola mais rápido que a tecla, e a vertical um pouco mais (ZH)
		const float EX = Mouse.X <= EdgeScrollPx ? -1.0f : (Mouse.X >= W - 1 - EdgeScrollPx ? 1.0f : 0.0f);
		const float EY = Mouse.Y <= EdgeScrollPx ? 1.0f : (Mouse.Y >= H - 1 - EdgeScrollPx ? -1.0f : 0.0f);
		In.PanAxis.X += EX * EdgeScrollBoost;
		In.PanAxis.Y += EY * EdgeScrollBoost * VerticalScrollBias;
	}
	In.PanAxis.X = FMath::Clamp(In.PanAxis.X, -EdgeScrollBoost, EdgeScrollBoost);
	In.PanAxis.Y = FMath::Clamp(In.PanAxis.Y, -EdgeScrollBoost * VerticalScrollBias, EdgeScrollBoost * VerticalScrollBias);

	// botão direito: arrastar rola (estilo Generals: quanto mais longe do ponto, mais rápido); clique rápido desseleciona
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && bHasMouse)
	{
		bRightDown = true;
		bRightScrolling = false;
		RightAnchor = Mouse;
	}
	if (bRightDown && bHasMouse && H > 0)
	{
		const FVector2D Offset = Mouse - RightAnchor;
		bRightScrolling |= Offset.Size() > DragThresholdPx;
		if (bRightScrolling)
		{
			// mais longe do ponto apertado = mais rápido: 180 px = 1×, teto de 2,5× (Vanguard)
			const float Factor = FMath::Min(float(Offset.Size()) / DragRefPx, DragMaxFactor);
			In.DragPan = FVector2D(Offset.X, -Offset.Y).GetSafeNormal() * Factor;
		}
	}
	if (WasInputKeyJustReleased(EKeys::RightMouseButton))
	{
		if (!bRightScrolling)
		{
			Selected.Reset();
			bAttackMoveArmed = false;
		}
		bRightDown = false;
		bRightScrolling = false;
	}

	// a roda como eixo: dois cliques no mesmo quadro contam dois (como tecla, contavam um)
	In.ZoomSteps += GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	// zoom pelo teclado (ADR-0003): = − PgUp PgDn
	const bool bZoomIn = IsInputKeyDown(EKeys::Equals) || IsInputKeyDown(EKeys::Add) || IsInputKeyDown(EKeys::PageUp);
	const bool bZoomOut = IsInputKeyDown(EKeys::Hyphen) || IsInputKeyDown(EKeys::Subtract) || IsInputKeyDown(EKeys::PageDown);
	In.ZoomSteps += ((bZoomIn ? 1.0f : 0.0f) - (bZoomOut ? 1.0f : 0.0f)) * KeyboardZoomStepsPerSec * DeltaTime;

	// botão do meio: gira (com Alt, inclina); dois cliques voltam ao padrão
	if (WasInputKeyJustPressed(EKeys::MiddleMouseButton) && bHasMouse)
	{
		const double Now = FPlatformTime::Seconds();
		In.bReset = Now - LastMiddleClickTime < DoubleClickSeconds;
		LastMiddleClickTime = Now;
		bMiddleDown = true;
		MiddleAnchor = Mouse;
	}
	if (bMiddleDown && bHasMouse)
	{
		// gira mais rápido quanto mais longe do ponto apertado (ZH/Vanguard); com Alt, inclina
		// (o modo cinematográfico das artes, fora do jogo competitivo)
		const FVector2D Offset = Mouse - MiddleAnchor;
		if (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))
		{
			In.PitchDelta = -FMath::Clamp(float(Offset.Y) / RotateRefPx, -RotateMaxFactor, RotateMaxFactor) * 40.0f * DeltaTime;
		}
		else if (FMath::Abs(Offset.X) > DragThresholdPx)
		{
			In.YawDelta += FMath::Clamp(float(Offset.X) / RotateRefPx, -RotateMaxFactor, RotateMaxFactor) * RotateDegPerSec * DeltaTime;
		}
	}
	if (WasInputKeyJustReleased(EKeys::MiddleMouseButton))
	{
		bMiddleDown = false;
	}
	Cam->ApplyInput(In, DeltaTime);
}

void ARTSPlayerController::HandleLeftClick(const FVector2D& Mouse)
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (bAttackMoveArmed)
	{
		FVector Ground;
		if (!Selected.IsEmpty() && GroundUnderCursor(Mouse, Ground))
		{
			M->OrderMove(Selected, FVector2D(Ground.X, Ground.Y), /*bAttackMove=*/true);
			AddMarker(Ground, true);
			++Orders;
		}
		bAttackMoveArmed = false;
		return;
	}

	const uint32 Hit = PickUnit(Mouse);
	FRTSUnitState U;
	if (Hit != 0 && M->GetUnit(Hit, U))
	{
		if (U.Owner == M->LocalPlayer())
		{
			const double Now = FPlatformTime::Seconds();
			if (Hit == LastClickedId && Now - LastClickTime < DoubleClickSeconds)
			{
				SelectSameTypeOnScreen(Hit);
			}
			else if (bShift)
			{
				if (Selected.Contains(Hit)) { Selected.Remove(Hit); } else { Selected.Add(Hit); }
			}
			else
			{
				Selected = { Hit };
			}
			LastClickedId = Hit;
			LastClickTime = Now;
			return;
		}
		if (M->IsEnemyOfLocal(U.Owner))
		{
			if (!Selected.IsEmpty())
			{
				M->OrderAttack(Selected, Hit);
				++Orders;
				const FRTSBakedMap* Map = M->BakedMap();
				AddMarker(FVector(U.PosCm.X, U.PosCm.Y, Map ? Map->GroundZCm(U.PosCm) : 0.0f), true);
			}
			return;
		}
	}

	FVector Ground;
	if (!Selected.IsEmpty() && GroundUnderCursor(Mouse, Ground))
	{
		M->OrderMove(Selected, FVector2D(Ground.X, Ground.Y), /*bAttackMove=*/false);
		AddMarker(Ground, false);
		++Orders;
	}
}

void ARTSPlayerController::HandleBoxSelect(const FVector2D& A, const FVector2D& B)
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	const FBox2D Box(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const FRTSBakedMap* Map = M->BakedMap();
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	TArray<uint32> Found;
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner != M->LocalPlayer())
		{
			continue;   // a caixa só pega unidade própria (ADR-0004)
		}
		FVector2D Screen;
		const FVector World(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + PickHeightCm);
		if (ProjectWorldLocationToScreen(World, Screen) && Box.IsInside(Screen))
		{
			Found.Add(U.Id);
		}
	}
	if (!bShift)
	{
		Selected = Found;
		return;
	}
	for (const uint32 Id : Found)
	{
		Selected.AddUnique(Id);
	}
}

void ARTSPlayerController::SelectSameTypeOnScreen(uint32 Id)
{
	URTSMatchSubsystem* M = Match();
	FRTSUnitState Clicked;
	if (!M || !M->GetUnit(Id, Clicked))
	{
		return;
	}
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	const FBox2D Screen(FVector2D::ZeroVector, FVector2D(W, H));
	const FRTSBakedMap* Map = M->BakedMap();
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	Selected.Reset();
	for (const FRTSUnitState& U : Units)
	{
		FVector2D P;
		const FVector World(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + PickHeightCm);
		if (U.Owner == Clicked.Owner && U.Def == Clicked.Def && ProjectWorldLocationToScreen(World, P) && Screen.IsInside(P))
		{
			Selected.Add(U.Id);
		}
	}
}

void ARTSPlayerController::HandleGroups()
{
	URTSMatchSubsystem* M = Match();
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	for (int32 K = 0; K < 10; ++K)
	{
		if (!WasInputKeyJustPressed(GroupKey(K)))
		{
			continue;
		}
		if (bCtrl)
		{
			Groups.Add(K, Selected);
			continue;
		}
		if (bShift)
		{
			// Shift+número: a seleção atual entra no grupo (ADR-0004)
			TArray<uint32>& Group = Groups.FindOrAdd(K);
			for (const uint32 Id : Selected)
			{
				Group.AddUnique(Id);
			}
			continue;
		}
		const TArray<uint32>* Group = Groups.Find(K);
		if (!Group)
		{
			continue;
		}
		Selected = *Group;
		PruneSelection();
		const double Now = FPlatformTime::Seconds();
		if (LastGroupKey == K && Now - LastGroupKeyTime < 0.4 && M && !Selected.IsEmpty())
		{
			// dois toques: a câmera vai até o grupo
			FVector2D Sum = FVector2D::ZeroVector;
			int32 Count = 0;
			for (const uint32 Id : Selected)
			{
				FRTSUnitState U;
				if (M->GetUnit(Id, U))
				{
					Sum += U.PosCm;
					++Count;
				}
			}
			if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn()); Cam && Count > 0)
			{
				Cam->GlideTo(Sum / Count);   // desliza mantendo o giro e o zoom
			}
		}
		LastGroupKey = K;
		LastGroupKeyTime = Now;
	}
}

uint32 ARTSPlayerController::PickUnit(const FVector2D& Mouse) const
{
	URTSMatchSubsystem* M = Match();
	if (!M || !PlayerCameraManager)
	{
		return 0;
	}
	const FRTSBakedMap* Map = M->BakedMap();
	const FVector CamRight = FRotationMatrix(PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	uint32 Best = 0;
	float BestScore = TNumericLimits<float>::Max();
	for (const FRTSUnitState& U : Units)
	{
		const FRTSUnitVisual* Visual = M->VisualOf(U.Def);
		const float RadiusCm = Visual ? Visual->RadiusM * 100.0f : 300.0f;
		const FVector Center(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + PickHeightCm);
		FVector2D C;
		FVector2D Edge;
		if (!ProjectWorldLocationToScreen(Center, C) || !ProjectWorldLocationToScreen(Center + CamRight * RadiusCm, Edge))
		{
			continue;
		}
		const float RadiusPx = FMath::Max(14.0f, FVector2D::Distance(C, Edge));
		const float Dist = FVector2D::Distance(C, Mouse);
		if (Dist <= RadiusPx && Dist / RadiusPx < BestScore)
		{
			BestScore = Dist / RadiusPx;
			Best = U.Id;
		}
	}
	return Best;
}

bool ARTSPlayerController::GroundUnderCursor(const FVector2D& Mouse, FVector& Out) const
{
	FVector Origin;
	FVector Dir;
	if (!DeprojectScreenPositionToWorld(Mouse.X, Mouse.Y, Origin, Dir))
	{
		return false;
	}
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * 500000.0, ECC_Visibility))
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	if (FMath::Abs(Dir.Z) > KINDA_SMALL_NUMBER)
	{
		Out = Origin + Dir * (-Origin.Z / Dir.Z);
		return true;
	}
	return false;
}

void ARTSPlayerController::PruneSelection()
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		Selected.Reset();
		return;
	}
	Selected.RemoveAll([M](uint32 Id)
	{
		FRTSUnitState U;
		return !M->GetUnit(Id, U);
	});
}

void ARTSPlayerController::AddMarker(const FVector& At, bool bAttack)
{
	FRTSOrderMarker Marker;
	Marker.World = At;
	Marker.bAttack = bAttack;
	Markers.Add(Marker);
}
