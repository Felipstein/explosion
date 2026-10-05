#include "ForgeUITest.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "SForgePanel.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"
#include "WorldForgeGameMode.h"
#include "WorldForgeTerrain.h"

namespace
{
	float WaterFraction(const FWorldMap* M)
	{
		if (!M || M->Pass.Num() == 0) return 0.0f;
		int32 W = 0;
		for (EWorldPass P : M->Pass) W += P == EWorldPass::Water ? 1 : 0;
		return float(W) / M->Pass.Num();
	}
}

FForgeUITest::FForgeUITest(AWorldForgeGameMode* InGameMode, const FString& InFolder)
	: GameMode(InGameMode), Folder(InFolder)
{
	Build();
}

// ---- entrada sintética ----------------------------------------------------------

FVector2D FForgeUITest::Target(const FString& Id, FVector2D Frac) const
{
	AWorldForgeGameMode* GM = GameMode.Get();
	APlayerController* PC = GM ? GM->GetWorld()->GetFirstPlayerController() : nullptr;
	AForgePlayerController* FPC = Cast<AForgePlayerController>(PC);
	const TSharedPtr<SForgePanel> Panel = FPC ? FPC->GetPanel() : nullptr;
	const TSharedPtr<SWidget> W = Panel.IsValid() ? Panel->FindTarget(Id) : nullptr;
	if (!W.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE UI widget not found: %s"), *Id);
		return FVector2D::ZeroVector;
	}
	const FGeometry& G = W->GetTickSpaceGeometry();
	return FVector2D(G.LocalToAbsolute(FVector2D(G.GetLocalSize()) * Frac));
}

FVector2D FForgeUITest::SliderPoint(const FString& Id, float Value, float YFrac) const
{
	// a conta do SSlider (PositionToValue, com a bolinha recuada): x = T + v·(L − 2T)
	const FVector2D Left = Target(Id, FVector2D(0.0, YFrac)), Right = Target(Id, FVector2D(1.0, YFrac));
	const double W = Right.X - Left.X, T = SForgePanel::ThumbSize;
	return FVector2D(Left.X + T + Value * (W - 2.0 * T), Left.Y);
}

FVector2D FForgeUITest::ViewportPoint(FVector2D Frac) const
{
	const TSharedPtr<SViewport> V = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	if (!V.IsValid())
	{
		return FVector2D::ZeroVector;
	}
	const FGeometry& G = V->GetTickSpaceGeometry();
	return FVector2D(G.LocalToAbsolute(FVector2D(G.GetLocalSize()) * Frac));
}

void FForgeUITest::Move(FVector2D Pos)
{
	FSlateApplication& App = FSlateApplication::Get();
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastPos, Pressed, EKeys::Invalid, 0.0f, FModifierKeysState());
	App.ProcessMouseMoveEvent(E);
	LastPos = Pos;
}

void FForgeUITest::Down(FVector2D Pos, FKey Button)
{
	FSlateApplication& App = FSlateApplication::Get();
	// clique novo: ninguém pode estar segurando o mouse (a viewport do jogo
	// pega a captura com o mouse de verdade e engolia os cliques no painel)
	if (Pressed.Num() == 0 && App.HasUserMouseCapture(App.GetUserIndexForMouse()))
	{
		UE_LOG(LogTemp, Display, TEXT("FORGE UI soltando captura do mouse antes do clique"));
		App.ReleaseAllPointerCapture(App.GetUserIndexForMouse());
	}
	Pressed.Add(Button);
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastPos, Pressed, Button, 0.0f, FModifierKeysState());
	App.ProcessMouseButtonDownEvent(nullptr, E);
	LastPos = Pos;
}

void FForgeUITest::Up(FVector2D Pos, FKey Button)
{
	FSlateApplication& App = FSlateApplication::Get();
	Pressed.Remove(Button);
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastPos, Pressed, Button, 0.0f, FModifierKeysState());
	App.ProcessMouseButtonUpEvent(E);
	LastPos = Pos;
}

void FForgeUITest::Wheel(FVector2D Pos, float Delta)
{
	FSlateApplication& App = FSlateApplication::Get();
	Move(Pos);
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, Pos, Pressed, EKeys::Invalid, Delta, FModifierKeysState());
	App.ProcessMouseWheelOrGestureEvent(E, nullptr);
}

void FForgeUITest::Key(FKey K, bool bDown)
{
	FSlateApplication& App = FSlateApplication::Get();
	const FKeyEvent E(K, FModifierKeysState(), App.GetUserIndexForKeyboard(), false, 0, 0);
	if (bDown) App.ProcessKeyDownEvent(E);
	else App.ProcessKeyUpEvent(E);
}

bool FForgeUITest::RealCursorOverWindow() const
{
	// o Slate também recebe o mouse de verdade: se ele está sobre a janela,
	// os eventos sintéticos podem ser atropelados (hover, captura)
	const TSharedPtr<SViewport> V = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	if (!V.IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	const FGeometry& G = V->GetTickSpaceGeometry();
	const FVector2D Real = FSlateApplication::Get().GetPlatformCursor().IsValid() ? FVector2D(FSlateApplication::Get().GetPlatformCursor()->GetPosition()) : FVector2D(-1e6, -1e6);
	const FVector2D Local = FVector2D(G.AbsoluteToLocal(Real));
	return Local.X >= 0 && Local.Y >= 0 && Local.X <= G.GetLocalSize().X && Local.Y <= G.GetLocalSize().Y;
}

bool FForgeUITest::NewMapSince(int32 Ticket) const
{
	const AWorldForgeGameMode* GM = GameMode.Get();
	return GM && !GM->IsGenerating() && GM->GetShownTicket() > Ticket;
}

// ---- os passos -------------------------------------------------------------------

void FForgeUITest::Build()
{
	auto GM = [this]() { return GameMode.Get(); };
	auto Map = [GM]() { return GM() && GM()->GetTerrain() ? GM()->GetTerrain()->GetMap() : nullptr; };
	auto Pawn = [GM]() -> AForgeCameraPawn*
	{
		APlayerController* PC = GM() ? GM()->GetWorld()->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn<AForgeCameraPawn>() : nullptr;
	};
	auto Remember = [this, GM, Map, Pawn]()
	{
		TicketBefore = GM()->GetShownTicket();
		SeedBefore = GM()->GetSettings().Seed;
		FocusBefore = Pawn() ? Pawn()->GetFocus() : FVector2f::ZeroVector;
		DistanceBefore = Pawn() ? Pawn()->GetDistance() : 0.0f;
		WaterFractionBefore = WaterFraction(Map());
		YawBefore = Pawn() ? Pawn()->GetYaw() : 0.0f;
	};
	auto NewMap = [this]() { return NewMapSince(TicketBefore); };
	auto Always = []() { return true; };

	Steps.Add({ TEXT("dice"), [this, Remember]() { Remember(); Click(Target(TEXT("Dice"))); }, NewMap,
		[this, Map]() -> FString
		{
			const FWorldMap* M = Map();
			if (!M || M->Settings.Seed == SeedBefore) return TEXT("a semente não mudou");
			return M->bConnected ? FString() : TEXT("mapa novo sem caminho entre as bases");
		} });
	Steps.Add({ TEXT("players4"), [this, Remember]() { Remember(); Click(Target(TEXT("Players_4"))); }, NewMap,
		[Map]() -> FString { return Map() && Map()->Settings.Players == 4 && Map()->Starts.Num() == 4 ? FString() : TEXT("não ficou com 4 bases"); } });
	Steps.Add({ TEXT("desert"), [this, Remember]() { Remember(); Click(Target(TEXT("Biome_1"))); }, NewMap,
		[Map]() -> FString { return Map() && Map()->Settings.Biome == EWorldBiome::Desert ? FString() : TEXT("bioma não mudou"); } });
	// arrastar o controle de água de onde está até 90%
	Steps.Add({ TEXT("water90"), [this, Remember]()
		{
			Remember();
			const float Cur = GameMode->GetSettings().Water;
			const FVector2D From = SliderPoint(TEXT("Slider_Water"), Cur, 0.5f);
			const FVector2D To = SliderPoint(TEXT("Slider_Water"), 0.9f, 0.5f);
			Move(From);
			Down(From, EKeys::LeftMouseButton);
			Move(FMath::Lerp(From, To, 0.5));
			Move(To);
			Up(To, EKeys::LeftMouseButton);
		}, NewMap,
		[this, Map]() -> FString
		{
			const float W = GameMode->GetSettings().Water;
			if (FMath::Abs(W - 0.9f) > 0.03f) return FString::Printf(TEXT("água ficou em %.2f"), W);
			const float F = WaterFraction(Map());
			return F > WaterFractionBefore ? FString() : FString::Printf(TEXT("a água não aumentou (%.3f → %.3f)"), WaterFractionBefore, F);
		} });
	// clique solto na faixa do controle (longe da bolinha, na beirada de baixo): pula pro ponto
	Steps.Add({ TEXT("slider_click_track"), [this, Remember]() { Remember(); Click(SliderPoint(TEXT("Slider_Forests"), 0.2f, 0.85f)); }, NewMap,
		[this]() -> FString
		{
			const float F = GameMode->GetSettings().Forests;
			return FMath::Abs(F - 0.2f) < 0.03f ? FString() : FString::Printf(TEXT("florestas ficou em %.2f"), F);
		} });
	Steps.Add({ TEXT("overlay_pass"), [this, Remember]() { Remember(); Click(Target(TEXT("Overlay_1"))); }, Always,
		[GM]() -> FString { return GM()->GetTerrain()->GetOverlay() == 1 ? FString() : TEXT("camada de passagem não ligou"); }, 1.0f });
	Steps.Add({ TEXT("overlay_areas"), [this, Remember]() { Remember(); Click(Target(TEXT("Overlay_2"))); }, Always,
		[GM]() -> FString { return GM()->GetTerrain()->GetOverlay() == 2 ? FString() : TEXT("camada de áreas não ligou"); }, 1.0f });
	Steps.Add({ TEXT("overlay_off"), [this, Remember]() { Remember(); Click(Target(TEXT("Overlay_0"))); }, Always,
		[GM]() -> FString { return GM()->GetTerrain()->GetOverlay() == 0 ? FString() : TEXT("camada não desligou"); }, 1.0f });
	// clique no minimapa (canto de cima à direita) leva a câmera pra lá
	Steps.Add({ TEXT("minimap"), [this, Remember]() { Remember(); Click(Target(TEXT("Minimap"), FVector2D(0.85, 0.15))); }, Always,
		[Map, Pawn]() -> FString
		{
			const float Size = Map()->Half() * 2.0f;
			const FVector2f Want((0.5f - 0.15f) * Size, (0.85f - 0.5f) * Size);
			const FVector2f Got = Pawn()->GetFocus();
			return FVector2f::Distance(Want, Got) < 0.06f * Size ? FString()
				: FString::Printf(TEXT("câmera em (%.0f, %.0f), esperado (%.0f, %.0f)"), Got.X, Got.Y, Want.X, Want.Y);
		}, 2.0f });
	// teclado: W por ~1 s anda pra frente (+X com a câmera padrão)
	Steps.Add({ TEXT("key_w"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::W, true);
		}, Always,
		[this, Pawn]() -> FString
		{
			Key(EKeys::W, false);
			const float D = Pawn()->GetFocus().X - FocusBefore.X;
			return D > 20.0f ? FString() : FString::Printf(TEXT("andou só %.1f m"), D);
		}, 1.0f });
	// roda do mouse no meio da tela: aproxima
	Steps.Add({ TEXT("wheel_zoom"), [this, Remember]()
		{
			Remember();
			for (int32 K = 0; K < 3; ++K) Wheel(ViewportPoint(FVector2D(0.6, 0.5)), 1.0f);
		}, Always,
		[this, Pawn]() -> FString
		{
			const float D = Pawn()->GetDistance();
			return D < DistanceBefore * 0.8f ? FString() : FString::Printf(TEXT("distância %.0f → %.0f"), DistanceBefore, D);
		}, 1.0f });
	// roda em cima do painel: não pode dar zoom
	Steps.Add({ TEXT("wheel_over_panel"), [this, Remember]()
		{
			Remember();
			for (int32 K = 0; K < 3; ++K) Wheel(Target(TEXT("Minimap"), FVector2D(0.5, 0.5)), 1.0f);
		}, Always,
		[this, Pawn]() -> FString
		{
			const float D = Pawn()->GetDistance();
			return FMath::Abs(D - DistanceBefore) < 1.0f ? FString() : FString::Printf(TEXT("zoom vazou pelo painel: %.0f → %.0f"), DistanceBefore, D);
		}, 1.0f });
	// botão direito arrastando o chão pra direita: a câmera vai pra esquerda (−Y)
	auto Drag = [](FStep S, FKey Button, FVector2D From, FVector2D To, float Time)
	{
		S.DragButton = Button;
		S.DragFrom = From;
		S.DragTo = To;
		S.DragTime = Time;
		return S;
	};
	Steps.Add(Drag({ TEXT("rmb_scroll_right"), Remember, Always,
		[this, Pawn]() -> FString
		{
			// rolagem do C&C: mouse pra direita da âncora → câmera anda pra direita (+Y)
			const FVector2f Got = Pawn()->GetFocus();
			return Got.Y > FocusBefore.Y + 10.0f ? FString() : FString::Printf(TEXT("rolagem não andou pra direita (y %.1f → %.1f)"), FocusBefore.Y, Got.Y);
		}, 0.5f }, EKeys::RightMouseButton, FVector2D(0.55, 0.55), FVector2D(0.75, 0.55), 0.4f));
	// botão do meio arrastando pra cima e pro lado: gira e deita a câmera até a horizontal
	Steps.Add(Drag({ TEXT("tilt_horizontal"), Remember, Always,
		[this, Pawn]() -> FString
		{
			const float P = Pawn()->GetPitch(), Y = Pawn()->GetYaw();
			if (P > 1.0f) return FString::Printf(TEXT("inclinação parou em %.1f°"), P);
			return FMath::Abs(Y - YawBefore) > 20.0f ? FString() : TEXT("não girou");
		}, 1.5f }, EKeys::MiddleMouseButton, FVector2D(0.5, 0.8), FVector2D(0.7, 0.05), 0.6f));
	// dois cliques na roda: volta pro ângulo e rotação do início
	// (o 2º clique sai 0.15 s depois, no Tick: SecondClick)
	FStep DoubleClick{ TEXT("middle_double_click_reset"), [this, Remember]() { Remember(); Click(ViewportPoint(FVector2D(0.5, 0.5)), EKeys::MiddleMouseButton); }, Always,
		[Pawn]() -> FString
		{
			const float P = Pawn()->GetPitch(), Y = Pawn()->GetYaw();
			const float YawOff = FMath::Abs(Y - FMath::RoundToFloat(Y / 360.0f) * 360.0f);
			return FMath::Abs(P - AForgeCameraPawn::DefaultPitch) < 1.0f && YawOff < 1.0f ? FString()
				: FString::Printf(TEXT("não voltou: inclinação %.1f°, rotação %.1f°"), P, Y);
		}, 1.5f };
	DoubleClick.SecondClick = EKeys::MiddleMouseButton;
	Steps.Add(DoubleClick);
}

bool FForgeUITest::Tick(float DeltaSeconds)
{
	if (!GameMode.IsValid() || !FSlateApplication::IsInitialized())
	{
		return true;
	}
	if (Index >= Steps.Num())
	{
		UE_LOG(LogTemp, Display, TEXT("FORGE UI test done: %d of %d steps failed"), Failures, Steps.Num());
		return true;
	}
	FStep& S = Steps[Index];
	Timer += DeltaSeconds;
	if (Phase == 0)
	{
		S.Act();
		Phase = 1;
		Timer = 0.0f;
		return false;
	}
	// segundo clique de um duplo clique
	if (S.SecondClick.IsValid() && Phase == 1 && !bDragStarted && Timer >= 0.15f)
	{
		Click(ViewportPoint(FVector2D(0.5, 0.5)), S.SecondClick);
		bDragStarted = true;
		return false;
	}
	// arraste em vários quadros: aperta no começo, anda até o fim e solta
	if (S.DragButton.IsValid() && Phase == 1)
	{
		const FVector2D A = ViewportPoint(S.DragFrom), B = ViewportPoint(S.DragTo);
		if (!bDragStarted)
		{
			Move(A);
			Down(A, S.DragButton);
			bDragStarted = true;
			return false;
		}
		if (Pressed.Contains(S.DragButton))
		{
			const float T = FMath::Clamp(Timer / S.DragTime, 0.0f, 1.0f);
			Move(FMath::Lerp(A, B, T));
			if (T >= 1.0f) Up(B, S.DragButton);
			return false;
		}
	}
	if (Phase == 1 && Timer >= S.MinWait && S.Ready())
	{
		const FString Why = S.Check();
		if (Why.IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("FORGE UI ok %s"), *S.Name);
		}
		else
		{
			++Failures;
			UE_LOG(LogTemp, Warning, TEXT("FORGE UI FALHOU %s: %s%s"), *S.Name, *Why, RealCursorOverWindow() ? TEXT(" (o mouse de verdade estava em cima da janela)") : TEXT(""));
		}
		const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("ui_%02d_%s.png"), Index, *S.Name));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		Phase = 2;
		Timer = 0.0f;
		return false;
	}
	if (Phase == 1 && Timer > 30.0f)
	{
		++Failures;
		UE_LOG(LogTemp, Warning, TEXT("FORGE UI FALHOU %s: nada aconteceu em 30 s%s"), *S.Name, RealCursorOverWindow() ? TEXT(" (o mouse de verdade estava em cima da janela)") : TEXT(""));
		Phase = 2;
		Timer = 0.0f;
	}
	if (Phase == 2 && Timer > 0.3f)
	{
		++Index;
		Phase = 0;
		bDragStarted = false;
	}
	return false;
}
