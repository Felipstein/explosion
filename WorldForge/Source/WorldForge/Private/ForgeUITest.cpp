#include "ForgeUITest.h"
#include "ForgeDebugView.h"
#include "EngineUtils.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SForgePanel.h"
#include "Generation/WorldDetail.h"
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

/**
 * O portão da entrada: enquanto o teste roda, o movimento do mouse de verdade
 * (o Felipe usando o PC, o mouse em cima da janela) não chega ao app; só o que
 * o teste injeta (bInjecting). Sem ele os cliques sintéticos
 * iam pro lugar do mouse real e o teste falhava sem motivo (11–16 passos, 12/10).
 */
class FForgeInputGate : public IInputProcessor
{
public:
	bool bInjecting = false;
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	// o movimento (leva o cursor e o "em cima de" pro lugar do mouse real) e o
	// teclado (a janela do teste pega o foco ao abrir: o Enter do Felipe escrevendo
	// noutra janela terminava ou apagava a estrada do teste). Os botões do mouse
	// passam: barrar eles travava os cliques do teste nos botões (12/10)
	virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent&) override { return !bInjecting; }
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent&) override { return !bInjecting; }
	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent&) override { return !bInjecting; }
	virtual const TCHAR* GetDebugName() const override { return TEXT("ForgeUITestGate"); }
};

void FForgeUITest::Inject(TFunctionRef<void()> Fn)
{
	if (Gate.IsValid()) Gate->bInjecting = true;
	Fn();
	if (Gate.IsValid()) Gate->bInjecting = false;
}

FForgeUITest::FForgeUITest(AWorldForgeGameMode* InGameMode, const FString& InFolder)
	: GameMode(InGameMode), Folder(InFolder)
{
	if (FSlateApplication::IsInitialized())
	{
		Gate = MakeShared<FForgeInputGate>();
		FSlateApplication::Get().RegisterInputPreProcessor(Gate, 0);
	}
	// a Slate repõe o cursor no lugar do mouse de verdade a cada quadro (o
	// "movimento sintético"): com o mouse do Felipe parado em cima da janela, os
	// cliques do teste iam pro lugar dele. Desligado só durante o teste
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.EnableSyntheticCursorMoves")))
	{
		V->Set(0, ECVF_SetByCode);
	}
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

FVector2D FForgeUITest::WorldPoint(const FVector& Cm) const
{
	AWorldForgeGameMode* GM = GameMode.Get();
	APlayerController* PC = GM ? GM->GetWorld()->GetFirstPlayerController() : nullptr;
	FVector2D Screen(0, 0), Size(1, 1);
	if (!PC || !PC->ProjectWorldLocationToScreen(Cm, Screen) || !GEngine || !GEngine->GameViewport)
	{
		return FVector2D::ZeroVector;
	}
	GEngine->GameViewport->GetViewportSize(Size);
	return ViewportPoint(Screen / Size);
}

void FForgeUITest::Move(FVector2D Pos)
{
	FSlateApplication& App = FSlateApplication::Get();
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastPos, Pressed, EKeys::Invalid, 0.0f, FModifierKeysState());
	Inject([&]() { App.ProcessMouseMoveEvent(E); });
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
	Inject([&]() { App.ProcessMouseButtonDownEvent(nullptr, E); });
	LastPos = Pos;
}

void FForgeUITest::Up(FVector2D Pos, FKey Button)
{
	FSlateApplication& App = FSlateApplication::Get();
	Pressed.Remove(Button);
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastPos, Pressed, Button, 0.0f, FModifierKeysState());
	Inject([&]() { App.ProcessMouseButtonUpEvent(E); });
	LastPos = Pos;
}

void FForgeUITest::Wheel(FVector2D Pos, float Delta)
{
	FSlateApplication& App = FSlateApplication::Get();
	Move(Pos);
	const FPointerEvent E(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, Pos, Pressed, EKeys::Invalid, Delta, FModifierKeysState());
	Inject([&]() { App.ProcessMouseWheelOrGestureEvent(E, nullptr); });
}

void FForgeUITest::Key(FKey K, bool bDown)
{
	FSlateApplication& App = FSlateApplication::Get();
	const FKeyEvent E(K, FModifierKeysState(), App.GetUserIndexForKeyboard(), false, 0, 0);
	Inject([&]() { if (bDown) App.ProcessKeyDownEvent(E); else App.ProcessKeyUpEvent(E); });
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
	Steps.Add({ TEXT("tab_world"), [this]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GameMode->GetWorld()->GetFirstPlayerController())) PC->GetPanel()->SetTab(0);
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("players4"), [this, Remember]() { Remember(); Click(Target(TEXT("Players_4"))); }, NewMap,
		[Map]() -> FString { return Map() && Map()->Settings.Players == 4 && Map()->Starts.Num() == 4 ? FString() : TEXT("não ficou com 4 bases"); } });
	Steps.Add({ TEXT("desert"), [this, Remember]() { Remember(); Click(Target(TEXT("Biome_1"))); }, NewMap,
		[Map]() -> FString { return Map() && Map()->Settings.Biome == EWorldBiome::Desert ? FString() : TEXT("bioma não mudou"); } });
	// (16/10) a areia do acabamento (só no deserto): a amarela e de volta a de duna; só guarda, não gera o mapa de novo
	Steps.Add({ TEXT("scroll_sand"), [this]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GameMode->GetWorld()->GetFirstPlayerController())) PC->GetPanel()->ScrollToTarget(TEXT("Sand_2"));
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("sand_yellow"), [this]() { Click(Target(TEXT("Sand_2"))); }, Always,
		[this]() -> FString { return GameMode->GetSettings().Sand == 2 ? FString() : FString::Printf(TEXT("a areia ficou em %d"), GameMode->GetSettings().Sand); }, 0.4f });
	Steps.Add({ TEXT("sand_dune"), [this]() { Click(Target(TEXT("Sand_3"))); }, Always,
		[this]() -> FString { return GameMode->GetSettings().Sand == 3 ? FString() : FString::Printf(TEXT("a areia ficou em %d"), GameMode->GetSettings().Sand); }, 0.4f });
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
	// F2 liga o teste de qualidade e NÃO troca o modo de visão da Unreal (o atalho
	// de depuração dela, "viewmode unlit", vinha junto: tela sem sol nem sombra)
	Steps.Add({ TEXT("key_f2"), [this]()
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::F2, true);
			Key(EKeys::F2, false);
		}, Always,
		[this, GM]() -> FString
		{
			const UGameViewportClient* VC = GEngine ? GEngine->GameViewport : nullptr;
			const int32 Mode = VC ? VC->ViewModeIndex : -1;
			bool bDebug = false;
			for (TActorIterator<AForgeDebugView> It(GM()->GetWorld()); It; ++It) bDebug = true;
			Key(EKeys::F2, true);   // fecha de novo
			Key(EKeys::F2, false);
			if (Mode != VMI_Lit) return FString::Printf(TEXT("o modo de visão mudou pra %d (o atalho da Unreal ainda está ativo)"), Mode);
			return bDebug ? FString() : TEXT("o teste de qualidade não abriu");
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

	// edição à mão: com a ferramenta "apagar", o clique numa árvore (ou pedra)
	// de verdade tira a peça; Ctrl+Z devolve
	Steps.Add({ TEXT("tab_edit"), [this]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GameMode->GetWorld()->GetFirstPlayerController())) PC->GetPanel()->SetTab(1);
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("edit_frame"), [this, GM, Pawn]()
		{
			GM()->EditTool = 1;
			AWorldForgeTerrain* T = GM()->GetTerrain();
			if (T && (T->FindLivePiece(TEXT("Tree"), EditPieceCm) || T->FindLivePiece(TEXT("RockLarge"), EditPieceCm)))
			{
				Pawn()->SetView(FVector2f(EditPieceCm.X / 100.0, EditPieceCm.Y / 100.0), 0.0f, 55.0f, 90.0f);
			}
		}, Always, [this]() -> FString { return EditPieceCm.IsZero() ? TEXT("sem peça de verdade no mapa (falta a tabela live_<bioma>.json?)") : FString(); }, 1.5f });
	Steps.Add({ TEXT("edit_delete"), [this, GM]()
		{
			PiecesBefore = GM()->GetTerrain()->LiveInstanceCount();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Click(WorldPoint(EditPieceCm + FVector(0, 0, 150)));
		}, Always,
		[this, GM]() -> FString
		{
			const int32 N = GM()->GetTerrain()->LiveInstanceCount();
			return GM()->EditCount() == 1 && N == PiecesBefore - 1 ? FString()
				: FString::Printf(TEXT("edições %d, peças %d → %d"), GM()->EditCount(), PiecesBefore, N);
		}, 1.0f });
	Steps.Add({ TEXT("edit_undo"), [this]()
		{
			// o Ctrl fica apertado até a conferência (como a mão: o jogo lê o Ctrl
			// apertado no quadro em que o Z desce)
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, Always,
		[this, GM]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			const int32 N = GM()->GetTerrain()->LiveInstanceCount();
			GM()->EditTool = 0;
			return GM()->EditCount() == 0 && N == PiecesBefore ? FString()
				: FString::Printf(TEXT("edições %d, peças %d (antes %d)"), GM()->EditCount(), N, PiecesBefore);
		}, 1.0f });

	// pincel de chão: uma pincelada de asfalto (camada 5 no deserto) arrastada
	// no meio da tela; o peso do asfalto no meio da pincelada sobe e o Ctrl+Z volta
	Steps.Add(Drag({ TEXT("edit_paint"), [this, GM]()
		{
			GM()->EditTool = 2;
			GM()->BrushLayer = 5;
			GM()->BrushRadius = 6.0f;
			PiecesBefore = GM()->GetTerrain()->LiveInstanceCount();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}, Always,
		[this, GM]() -> FString
		{
			FWorldLive* L = GM()->GetTerrain()->GetLive();
			if (!L || !L->Edits.IsValid()) return TEXT("sem o mundo de verdade");
			int32 Strong = 0;
			for (const FColor& C : L->WeightsB) Strong += C.G > 200 ? 1 : 0;
			return L->Edits->Paint.Num() > 20 && Strong > 20 && GM()->GetRegionRefreshes() > 0 ? FString()
				: FString::Printf(TEXT("células pintadas %d, texels de asfalto %d, pedaços refeitos %d"), L->Edits->Paint.Num(), Strong, GM()->GetRegionRefreshes());
		}, 1.0f }, EKeys::LeftMouseButton, FVector2D(0.55, 0.5), FVector2D(0.7, 0.55), 0.6f));
	Steps.Add({ TEXT("edit_paint_undo"), [this]()
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, Always,
		[this, GM]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			GM()->EditTool = 0;
			FWorldLive* L = GM()->GetTerrain()->GetLive();
			// desfeita a pincelada, o pedaço sorteado de novo tem as mesmas peças de antes
			const int32 N = GM()->GetTerrain()->LiveInstanceCount();
			return L && L->Edits->Paint.Num() == 0 && N == PiecesBefore ? FString()
				: FString::Printf(TEXT("ainda há %d células pintadas; peças %d (antes %d)"), L ? L->Edits->Paint.Num() : -1, N, PiecesBefore);
		}, 1.0f });

	// pincel de altura: segurar "subir" ~1 s no meio da tela levanta o chão ali; Ctrl+Z volta
	Steps.Add(Drag({ TEXT("edit_height"), [this, GM]()
		{
			GM()->EditTool = 3;
			GM()->HeightMode = 0;
			GM()->BrushRadius = 6.0f;
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			FVector P;
			AForgeCameraPawn* Cam = GM()->GetWorld()->GetFirstPlayerController()->GetPawn<AForgeCameraPawn>();
			HeightProbe = Cam && Cam->PixelGroundPoint(ViewportPixel(FVector2D(0.6, 0.5)), P) ? FVector2f(P.X, P.Y) : FVector2f::ZeroVector;
			FWorldLive* L = GM()->GetTerrain()->GetLive();
			HeightBefore = L ? L->Display->HeightAt(HeightProbe) : 0.0f;
		}, Always,
		[this, GM]() -> FString
		{
			FWorldLive* L = GM()->GetTerrain()->GetLive();
			const float H = L ? L->Display->HeightAt(HeightProbe) : 0.0f;
			return H > HeightBefore + 0.6f ? FString() : FString::Printf(TEXT("altura %.2f → %.2f m"), HeightBefore, H);
		}, 0.5f }, EKeys::LeftMouseButton, FVector2D(0.6, 0.5), FVector2D(0.6, 0.5), 1.0f));
	Steps.Add({ TEXT("edit_height_undo"), [this]()
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, Always,
		[this, GM]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			GM()->EditTool = 0;
			FWorldLive* L = GM()->GetTerrain()->GetLive();
			const float H = L ? L->Display->HeightAt(HeightProbe) : 0.0f;
			return FMath::Abs(H - HeightBefore) < 0.05f ? FString() : FString::Printf(TEXT("ficou em %.2f m (antes %.2f)"), H, HeightBefore);
		}, 1.0f });

	// estrada à mão: três cliques no chão e Enter desenham uma de terra; o mundo
	// é refeito com ela. Depois: vira asfalto pelo painel, Delete apaga, Ctrl+Z volta
	auto Live = [GM]() -> FWorldLive* { return GM() && GM()->GetTerrain() ? GM()->GetTerrain()->GetLive() : nullptr; };
	// (num areal aberto, longe de estrada: na beira do oásis a de terra some no
	// chão de terra úmida e a foto não mostra nada)
	Steps.Add({ TEXT("road_frame"), [this, Pawn, Live]()
		{
			FWorldLive* L = Live();
			if (!L) return;
			const int32 R = L->WeightRes;
			auto Sand = [L, R](FVector2f P)
			{
				const int32 I = FMath::Clamp(FMath::FloorToInt(P.X / L->SizeM * R + 0.5f * R), 0, R - 1);
				const int32 J = FMath::Clamp(FMath::FloorToInt(P.Y / L->SizeM * R + 0.5f * R), 0, R - 1);
				return L->WeightsA[J * R + I].R;
			};
			// o lugar com mais areia no pior ponto (o chão do teste muda com os
			// controles: o limiar fixo de antes às vezes não achava nenhum)
			int32 Best = -1;
			for (float Y = -0.35f * L->SizeM; Y <= 0.35f * L->SizeM; Y += 24.0f)
			{
				for (float X = -0.35f * L->SizeM; X <= 0.35f * L->SizeM; X += 24.0f)
				{
					const FVector2f C(X, Y);
					if (L->Detail->RoadAt(C).D < 45.0f || L->Detail->InVillage(C, 60.0f)) continue;
					int32 Worst = 255;
					for (int32 K = 0; K < 25; ++K) Worst = FMath::Min<int32>(Worst, Sand(C + FVector2f((K % 5 - 2) * 20.0f, (K / 5 - 2) * 12.0f)));
					if (Worst > Best) { Best = Worst; RoadProbe = C; }
				}
			}
			if (Best >= 0) Pawn()->SetView(RoadProbe, 0.0f, 55.0f, 90.0f);
		}, Always, [this]() -> FString { return RoadProbe.IsZero() ? TEXT("sem areal aberto no mapa") : FString(); }, 1.5f });
	const FVector2D RoadClicks[] = { FVector2D(0.45, 0.62), FVector2D(0.58, 0.5), FVector2D(0.72, 0.58) };
	for (int32 K = 0; K < 3; ++K)
	{
		const FVector2D At = RoadClicks[K];
		Steps.Add({ FString::Printf(TEXT("road_point%d"), K + 1), [this, GM, At, K]()
			{
				if (K == 0)
				{
					GM()->EditTool = 4;
					GM()->RoadDeselect();
					GM()->RoadDraft.Reset();
					GM()->RoadPaved = false;
				}
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
				Click(ViewportPoint(At));
			}, Always,
			[GM, K]() -> FString
			{
				return GM()->RoadDraft.Num() == K + 1 ? FString()
					: FString::Printf(TEXT("%d pontos no rascunho (escolheu uma estrada? %d)"), GM()->RoadDraft.Num(), GM()->HasRoadSelection() ? 1 : 0);
			}, 0.5f });
	}
	auto UserRoads = [Live]() -> FString
	{
		FWorldLive* L = Live();
		if (!L) return TEXT("sem o mundo de verdade");
		FString S = FString::Printf(TEXT("desenhadas %d:"), L->Edits->Roads.Num());
		for (int32 U = 0; U < L->Detail->UserRoadIndex.Num(); ++U)
		{
			const int32 G = L->Detail->UserRoadIndex[U];
			S += G == INDEX_NONE ? TEXT(" fora") : FString::Printf(TEXT(" %d m %s"), L->Detail->GRoads[G].P.Num(), L->Detail->GRoads[G].bPaved ? TEXT("asfalto") : TEXT("terra"));
		}
		return S;
	};
	auto RoadIs = [Live](int32 Count, int32 Paved) -> bool
	{
		FWorldLive* L = Live();
		if (!L || L->Edits->Roads.Num() != Count || L->Detail->UserRoadIndex.Num() != Count) return false;
		for (const int32 G : L->Detail->UserRoadIndex)
		{
			if (G == INDEX_NONE || L->Detail->GRoads[G].P.Num() < 20) return false;
			if (Paved >= 0 && L->Detail->GRoads[G].bPaved != (Paved == 1)) return false;
		}
		return true;
	};
	Steps.Add({ TEXT("road_finish"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::Enter, true);
		}, NewMap,
		[this, GM, RoadIs, UserRoads]() -> FString
		{
			Key(EKeys::Enter, false);
			return RoadIs(1, 0) && GM()->HasRoadSelection() ? FString() : UserRoads();
		}, 1.0f });
	auto ScrollTo = [this, GM](const FString& Id)
	{
		return FStep{ TEXT("scroll_") + Id, [GM, Id]()
			{
				if (AForgePlayerController* PC = Cast<AForgePlayerController>(GM()->GetWorld()->GetFirstPlayerController()))
				{
					PC->GetPanel()->ScrollToTarget(Id);
				}
			}, []() { return true; }, []() { return FString(); }, 0.4f };
	};
	Steps.Add(ScrollTo(TEXT("RoadType_1")));
	Steps.Add({ TEXT("road_paved"), [this, Remember]() { Remember(); Click(Target(TEXT("RoadType_1"))); }, NewMap,
		[RoadIs, UserRoads]() -> FString { return RoadIs(1, 1) ? FString() : UserRoads(); }, 1.0f });
	// (16/10) o acabamento do asfalto da escolhida: Gasta (vale no "qualidade real"; aqui confere que guardou)
	Steps.Add(ScrollTo(TEXT("RoadStyle_3")));
	Steps.Add({ TEXT("road_style_worn"), [this, Remember]() { Remember(); Click(Target(TEXT("RoadStyle_3"))); }, NewMap,
		[GM]() -> FString { return GM()->GetSelectedRoadStyle() == 3 ? FString() : FString::Printf(TEXT("o acabamento ficou em %d"), GM()->GetSelectedRoadStyle()); }, 1.0f });
	// (16/10) a sujeira na beira da escolhida: Muita
	Steps.Add(ScrollTo(TEXT("RoadDirt_4")));
	Steps.Add({ TEXT("road_dirt_high"), [this, Remember]() { Remember(); Click(Target(TEXT("RoadDirt_4"))); }, NewMap,
		[GM]() -> FString { return GM()->GetSelectedRoadDirt() == 4 ? FString() : FString::Printf(TEXT("a sujeira ficou em %d"), GM()->GetSelectedRoadDirt()); }, 1.0f });
	Steps.Add({ TEXT("road_delete"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::Delete, true);
		}, NewMap,
		[this, GM, RoadIs, UserRoads]() -> FString
		{
			Key(EKeys::Delete, false);
			return RoadIs(0, -1) && !GM()->HasRoadSelection() ? FString() : UserRoads();
		}, 1.0f });
	Steps.Add({ TEXT("road_delete_undo"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, NewMap,
		[this, RoadIs, UserRoads]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			return RoadIs(1, 1) ? FString() : UserRoads();
		}, 1.0f });

	// a estrada do gerador: clicar nela escolhe; Delete tira; Ctrl+Z devolve
	Steps.Add({ TEXT("road_gen_frame"), [this, GM, Pawn, Live]()
		{
			GM()->RoadDeselect();
			FWorldLive* L = Live();
			if (!L || L->Detail->NumGenRoads == 0) return;
			const TArray<FVector2f>& P = L->Detail->GRoads[0].P;
			RoadProbe = P[P.Num() / 2];
			RoadsBefore = L->Detail->GRoads.Num();
			Pawn()->SetView(RoadProbe, 0.0f, 55.0f, 90.0f);
		}, Always, [this]() -> FString { return RoadsBefore > 0 ? FString() : TEXT("o mapa não tem estrada"); }, 1.5f });
	Steps.Add({ TEXT("road_gen_select"), [this, Live]()
		{
			FWorldLive* L = Live();
			const float Z = L ? L->Display->HeightAt(RoadProbe) : 0.0f;
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Click(WorldPoint(FVector(RoadProbe.X * 100.0, RoadProbe.Y * 100.0, Z * 100.0 + 30.0)));
		}, Always,
		[GM]() -> FString
		{
			return GM()->HasRoadSelection() && GM()->SelectedRoadLine() && GM()->GetRoadHandles().Num() >= 2 ? FString()
				: FString::Printf(TEXT("escolhida %d, pontos %d"), GM()->HasRoadSelection() ? 1 : 0, GM()->GetRoadHandles().Num());
		}, 0.5f });
	Steps.Add({ TEXT("road_gen_delete"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::Delete, true);
		}, NewMap,
		[this, Live]() -> FString
		{
			Key(EKeys::Delete, false);
			FWorldLive* L = Live();
			float Near = 1e9f;
			for (int32 R = 0; L && R < L->Detail->NumGenRoads; ++R) { int32 I; Near = FMath::Min(Near, FWorldDetail::NearestOn(L->Detail->GRoads[R].P, RoadProbe, I)); }
			return L && L->Detail->GRoads.Num() == RoadsBefore - 1 && L->Edits->RemovedRoads.Num() == 1 && Near > 3.0f ? FString()
				: FString::Printf(TEXT("estradas %d (antes %d), tiradas %d, a mais perto do ponto a %.1f m"), L ? L->Detail->GRoads.Num() : -1, RoadsBefore, L ? L->Edits->RemovedRoads.Num() : -1, Near);
		}, 1.0f });
	Steps.Add({ TEXT("road_gen_undo"), [this, Remember]()
		{
			Remember();
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, NewMap,
		[this, GM, Live]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			GM()->EditTool = 0;
			FWorldLive* L = Live();
			return L && L->Detail->GRoads.Num() == RoadsBefore && L->Edits->RemovedRoads.Num() == 0 ? FString()
				: FString::Printf(TEXT("estradas %d (antes %d)"), L ? L->Detail->GRoads.Num() : -1, RoadsBefore);
		}, 1.0f });

	// mover: arrastar uma pedra com a ferramenta Mover leva ela; Ctrl+Z volta; e
	// o mundo refeito põe a pedra no lugar novo (o mesmo caminho do "qualidade real")
	Steps.Add({ TEXT("move_frame"), [this, GM, Pawn]()
		{
			GM()->EditTool = 5;
			AWorldForgeTerrain* T = GM()->GetTerrain();
			FVector Cm;
			AWorldForgeTerrain::FPicked Pk;
			if (T && T->FindLivePiece(TEXT("Rock"), Cm) && T->PickPiece(FVector2f(Cm.X / 100.0, Cm.Y / 100.0), 4.0f, Pk, TEXT("Rock")))
			{
				MovePivot = Pk.Pivot;
				Pawn()->SetView(MovePivot, 0.0f, 55.0f, 60.0f);
			}
		}, Always, [this]() -> FString { return MovePivot.IsZero() ? TEXT("sem pedra no mapa") : FString(); }, 1.5f });
	auto MovedCheck = [this, GM, Live](int32 Want, float MinDist) -> FString
	{
		FWorldLive* L = Live();
		if (!L) return TEXT("sem o mundo de verdade");
		if (L->Edits->Moved.Num() != Want) return FString::Printf(TEXT("movidas %d (queria %d)"), L->Edits->Moved.Num(), Want);
		if (Want == 0) return FString();
		const FWorldEdits::FMoved& M = L->Edits->Moved[0];
		const float D = FVector2f::Distance(M.To, MovePivot);
		return D > MinDist ? FString() : FString::Printf(TEXT("%s andou só %.1f m"), *M.Kind, D);
	};
	auto MoveDrag = [this, GM, Drag](const TCHAR* Name, TFunction<FString()> Check)
	{
		return Drag({ Name, [this, GM]()
			{
				GM()->EditTool = 5;
				PiecesBefore = GM()->GetTerrain()->LiveInstanceCount();
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
			}, [](){ return true; }, Check, 1.0f }, EKeys::LeftMouseButton, FVector2D(0.5, 0.5), FVector2D(0.62, 0.5), 0.6f);
	};
	Steps.Add(MoveDrag(TEXT("move_drag"), [this, GM, MovedCheck]() -> FString
		{
			const FString E = MovedCheck(1, 3.0f);
			const int32 N = GM()->GetTerrain()->LiveInstanceCount();
			return !E.IsEmpty() ? E : (N == PiecesBefore ? FString() : FString::Printf(TEXT("peças %d → %d"), PiecesBefore, N));
		}));
	Steps.Add({ TEXT("move_undo"), [this]()
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			Key(EKeys::LeftControl, true);
			Key(EKeys::Z, true);
		}, Always,
		[this, GM, MovedCheck]() -> FString
		{
			Key(EKeys::Z, false);
			Key(EKeys::LeftControl, false);
			const FString E = MovedCheck(0, 0.0f);
			AWorldForgeTerrain::FPicked Pk;
			const bool bBack = GM()->GetTerrain()->PickPiece(MovePivot, 1.0f, Pk, TEXT("Rock")) && FVector2f::Distance(Pk.Pivot, MovePivot) < 0.1f;
			return !E.IsEmpty() ? E : (bBack ? FString() : TEXT("a pedra não voltou pro lugar"));
		}, 1.0f });
	Steps.Add(MoveDrag(TEXT("move_drag_again"), [MovedCheck]() { return MovedCheck(1, 3.0f); }));
	Steps.Add({ TEXT("move_rebuild"), [this, GM, Remember]()
		{
			Remember();
			GM()->Generate(GM()->GetSettings());
		}, NewMap,
		[this, GM, Live]() -> FString
		{
			GM()->EditTool = 0;
			FWorldLive* L = Live();
			if (!L || L->Edits->Moved.Num() != 1) return TEXT("a edição sumiu ao refazer");
			const FWorldEdits::FMoved& M = L->Edits->Moved[0];
			AWorldForgeTerrain::FPicked Pk;
			if (!GM()->GetTerrain()->PickPiece(M.To, 1.5f, Pk)) return TEXT("nada no lugar novo");
			return Pk.Kind == M.Kind && FVector2f::Distance(Pk.Origin, M.From) < 0.1f && FVector2f::Distance(Pk.Pivot, M.To) < 0.3f ? FString()
				: FString::Printf(TEXT("no lugar novo: %s a %.2f m (queria %s de (%.1f, %.1f))"), *Pk.Kind, FVector2f::Distance(Pk.Pivot, M.To), *M.Kind, M.From.X, M.From.Y);
		}, 1.0f });

	// guardar na lista leva as edições: guarda, sorteia outro mundo (zera) e
	// abre o guardado pelo botão da lista; a lista do Felipe volta como estava
	const FString MapsJson = FPaths::ProjectSavedDir() / TEXT("WorldForge/maps.json");
	Steps.Add(ScrollTo(TEXT("Dice")));
	Steps.Add({ TEXT("tab_maps"), [this]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GameMode->GetWorld()->GetFirstPlayerController())) PC->GetPanel()->SetTab(2);
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("saved_with_edits"), [this, GM, Remember, MapsJson]()
		{
			bHadSavedList = FFileHelper::LoadFileToString(SavedListBefore, *MapsJson);
			GM()->AddSavedMap();
			Remember();
			Click(Target(TEXT("Dice")));
		}, NewMap,
		[Live]() -> FString
		{
			FWorldLive* L = Live();
			return L && L->Edits->IsEmpty() ? FString() : TEXT("semente nova não zerou as edições");
		}, 1.0f });
	Steps.Add({ TEXT("saved_refresh"), [GM]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GM()->GetWorld()->GetFirstPlayerController()))
			{
				PC->GetPanel()->RefreshSavedMaps();
			}
		}, Always, []() { return FString(); }, 0.3f });
	Steps.Add({ TEXT("scroll_saved"), [GM]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GM()->GetWorld()->GetFirstPlayerController()))
			{
				// o guardado com edição fica antes dos importados (o último sem pasta de importação)
				const TArray<AWorldForgeGameMode::FSavedMap>& Maps = GM()->GetSavedMaps();
				int32 I = Maps.Num() - 1;
				while (I > 0 && !Maps[I].ImportFolder.IsEmpty() && Maps[I].EditsFile.IsEmpty()) --I;
				PC->GetPanel()->ScrollToTarget(FString::Printf(TEXT("Saved_%d"), I));
			}
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("saved_open"), [this, GM, Remember]()
		{
			Remember();
			const TArray<AWorldForgeGameMode::FSavedMap>& Maps = GM()->GetSavedMaps();
			int32 I = Maps.Num() - 1;
			while (I > 0 && !Maps[I].ImportFolder.IsEmpty() && Maps[I].EditsFile.IsEmpty()) --I;
			Click(Target(FString::Printf(TEXT("Saved_%d"), I)));
		}, NewMap,
		[Live]() -> FString
		{
			FWorldLive* L = Live();
			return L && L->Edits->Moved.Num() == 1 ? FString()
				: FString::Printf(TEXT("abriu com %d peças movidas (queria 1)"), L ? L->Edits->Moved.Num() : -1);
		}, 1.0f });
	// devolve a lista do Felipe como estava e apaga os arquivos de edição do teste
	// (passo próprio: roda mesmo se abrir o guardado falhou; os testes que caíam no
	// meio deixavam "Deserto, semente N · 2 edições" na lista dele, 12/10)
	Steps.Add({ TEXT("saved_cleanup"), [this, GM, MapsJson]()
		{
			for (const AWorldForgeGameMode::FSavedMap& M : GM()->GetSavedMaps())
			{
				if (!M.EditsFile.IsEmpty() && !SavedListBefore.Contains(M.EditsFile))
				{
					IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("WorldForge") / M.EditsFile));
				}
			}
			if (bHadSavedList) FFileHelper::SaveStringToFile(SavedListBefore, *MapsJson);
			else IFileManager::Get().Delete(*MapsJson);
			GM()->ReloadSavedMaps();
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GM()->GetWorld()->GetFirstPlayerController()))
			{
				PC->GetPanel()->RefreshSavedMaps();
			}
		}, Always, []() { return FString(); }, 0.3f });

	// mapa do Zero Hour (Tools/zh_import.py): abre pela lista, com as estradas
	// de lá como edições, e as vistas de cima e de perto da base
	Steps.Add({ TEXT("tab_maps_import"), [this]()
		{
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GameMode->GetWorld()->GetFirstPlayerController())) PC->GetPanel()->SetTab(2);
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("scroll_import"), [GM]()
		{
			const TArray<AWorldForgeGameMode::FSavedMap>& Maps = GM()->GetSavedMaps();
			const int32 I = Maps.IndexOfByPredicate([](const AWorldForgeGameMode::FSavedMap& M) { return !M.ImportFolder.IsEmpty() && M.Name.Contains(TEXT("Desert Fury")); });
			if (AForgePlayerController* PC = Cast<AForgePlayerController>(GM()->GetWorld()->GetFirstPlayerController()))
			{
				PC->GetPanel()->ScrollToTarget(FString::Printf(TEXT("Saved_%d"), I));
			}
		}, Always, []() { return FString(); }, 0.4f });
	Steps.Add({ TEXT("import_open"), [this, GM, Remember]()
		{
			Remember();
			const TArray<AWorldForgeGameMode::FSavedMap>& Maps = GM()->GetSavedMaps();
			const int32 I = Maps.IndexOfByPredicate([](const AWorldForgeGameMode::FSavedMap& M) { return !M.ImportFolder.IsEmpty() && M.Name.Contains(TEXT("Desert Fury")); });
			if (I != INDEX_NONE) Click(Target(FString::Printf(TEXT("Saved_%d"), I)));
		}, NewMap,
		[GM, Live]() -> FString
		{
			FWorldLive* L = Live();
			const FWorldMap* M = GM()->GetTerrain()->GetMap();
			const int32 Roads = L ? L->Detail->GRoads.Num() - L->Detail->NumGenRoads : 0;
			return GM()->GetImportTitle() == TEXT("Desert Fury") && M && M->Starts.Num() == 2 && Roads >= 20 ? FString()
				: FString::Printf(TEXT("título '%s', bases %d, estradas %d (falta rodar Tools/zh_import.py?)"), *GM()->GetImportTitle(), M ? M->Starts.Num() : -1, Roads);
		}, 2.0f });
	Steps.Add({ TEXT("import_whole"), [Pawn]() { Pawn()->FrameWholeMap(true); }, Always, []() { return FString(); }, 2.0f });
	Steps.Add({ TEXT("import_base"), [GM, Pawn]()
		{
			const FWorldMap* M = GM()->GetTerrain()->GetMap();
			if (M && M->Starts.Num()) Pawn()->SetView(M->Starts[0].Pos, 30.0f, 55.0f, 160.0f);
		}, Always, []() { return FString(); }, 2.0f });
	Steps.Add({ TEXT("import_center"), [Pawn]() { Pawn()->SetView(FVector2f::ZeroVector, 30.0f, 50.0f, 220.0f); }, Always, []() { return FString(); }, 2.0f });
}

FVector2D FForgeUITest::ViewportPixel(FVector2D Frac) const
{
	FVector2D Size(1, 1);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
	return Size * Frac;
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
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.EnableSyntheticCursorMoves")))
		{
			V->Set(1, ECVF_SetByCode);
		}
		if (Gate.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Gate);
			Gate.Reset();
		}
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
