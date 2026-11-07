#include "SRTSScript.h"
#include "RTSShell.h"
#include "RTSShellAudio.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

using namespace RTSShellUI;

// As medidas do Chrome (Tools/script_layout.json). Num namespace com nome e sempre com o prefixo (a compilação unificada da Unreal
// junta os .cpp do módulo e os nomes anônimos colidem).
namespace RTSScriptScreen
{
	// o ciano de planta do desenvolvimento (--dev)
	FLinearColor Dev(float A = 1.0f) { return Css(120, 190, 220, A); }
	FColor DevV(float A = 1.0f) { return CssV(120, 190, 220, A); }
	FLinearColor Mute() { return Css(152, 161, 154); }
	const FVector4f SEARCH(110, 250, 709.05f, 50);
	const FVector4f CHIP[4] = { FVector4f(829.05f, 250, 86.61f, 50), FVector4f(925.66f, 250, 68.03f, 50), FVector4f(1003.69f, 250, 58.61f, 50), FVector4f(1072.3f, 250, 97.7f, 50) };
	const TCHAR* CHIP_T[4] = { TEXT("NEWEST"), TEXT("NAME"), TEXT("MAP"), TEXT("LAB ONLY") };
	const FVector4f ARMED(110, 314, 1060, 53);
	const FVector4f CARD(1250, 250, 560, 676);
	const FVector4f RUN(1250, 938, 560, 72);
	constexpr float ROWS_B = 1010.0f, ROW_W = 1052.0f, ROW_H = 58.0f, ROW_STEP = 62.0f;

	/** O dur() da página: 7.5 s, 15 s, 1:05. */
	FString Dur(float S)
	{
		if (S >= 60.0f) return FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(S / 60.0f), FMath::RoundToInt(FMath::Fmod(S, 60.0f)));
		return S < 10.0f ? FString::Printf(TEXT("%.1f s"), S) : FString::Printf(TEXT("%d s"), FMath::RoundToInt(S));
	}
	/** O texto cortado com reticências na largura (o text-overflow: ellipsis). */
	FString Fit(const FString& S, const FSlateFontInfo& F, float W, float Scale)
	{
		if (TextWidth(S, F, Scale) <= W) return S;
		FString T = S;
		while (T.Len() > 1 && TextWidth(T + TEXT("…"), F, Scale) > W) T.LeftChopInline(1);
		return T.TrimEnd() + TEXT("…");
	}
	/** O texto com o trecho buscado em ciano (o <mark> da página): os pedaços e as cores, até a largura (com reticências). */
	float Marked(FSlateWindowElementList& Out, int32 L, const FGeometry& S, const FString& Full, const FString& Q, const FSlateFontInfo& F, float X, float Base, float W,
		const FLinearColor& C, float Scale)
	{
		const FString Shown = Fit(Full, F, W, Scale);
		const int32 At = Q.IsEmpty() ? INDEX_NONE : Shown.Find(Q, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE) return Text(Out, L, S, Shown, F, X, Base, C);
		const FString A = Shown.Left(At), B = Shown.Mid(At, Q.Len()), Cc = Shown.Mid(At + Q.Len());
		float Xx = X;
		Xx += Text(Out, L, S, A, F, Xx, Base, C);
		Xx += Text(Out, L, S, B, F, Xx, Base, Dev());
		Xx += Text(Out, L, S, Cc, F, Xx, Base, C);
		return Xx - X;
	}
	/** A quebra de linha do CSS (palavra por palavra) e a do break-all (letra por letra). */
	TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& F, float W, float Scale, bool bAnyChar)
	{
		TArray<FString> Lines;
		if (bAnyChar)
		{
			FString Cur;
			for (const TCHAR Ch : Text)
			{
				if (!Cur.IsEmpty() && TextWidth(Cur + Ch, F, Scale) > W) { Lines.Add(Cur); Cur.Reset(); }
				Cur.AppendChar(Ch);
			}
			if (!Cur.IsEmpty()) Lines.Add(Cur);
			return Lines;
		}
		TArray<FString> Words;
		Text.ParseIntoArray(Words, TEXT(" "));
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && TextWidth(Try, F, Scale) > W) { Lines.Add(Cur); Cur = Wd; }
			else Cur = Try;
		}
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		return Lines;
	}
	FString FirstLine(const FString& About)
	{
		// o firstLine da página: tira o "(01/11, itens 41/57)" do começo
		FString S = About;
		if (S.StartsWith(TEXT("(")))
		{
			int32 E = INDEX_NONE;
			if (S.FindChar(TEXT(')'), E)) S = S.Mid(E + 1).TrimStart();
		}
		return S;
	}
}
namespace ScS = RTSScriptScreen;

void SRTSScript::Construct(const FArguments& InArgs)
{
	Restart();
}

void SRTSScript::Restart()
{
	// as cenas como o jogo tem (api/scenarios.json = o Scenarios.json), a armada da linha de comando, a escolhida
	All.Reset();
	FString Json;
	if (FRTSShell::Handlers().Api && FRTSShell::Handlers().Api(TEXT("scenarios.json"), Json))
	{
		TSharedPtr<FJsonObject> O;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) && O.IsValid() && O->TryGetArrayField(TEXT("scenarios"), Arr))
		{
			const auto Cnt = [](const TSharedPtr<FJsonObject>& J, const TCHAR* K)
			{
				const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
				double N = 0.0;
				if (J->TryGetArrayField(K, A)) return A->Num();
				return J->TryGetNumberField(K, N) ? int32(N) : 0;
			};
			for (int32 I = 0; I < Arr->Num(); ++I)
			{
				const TSharedPtr<FJsonObject> J = (*Arr)[I]->AsObject();
				if (!J.IsValid()) continue;
				FScene S;
				J->TryGetStringField(TEXT("name"), S.Name);
				J->TryGetStringField(TEXT("about"), S.About);
				J->TryGetStringField(TEXT("map"), S.Map);
				if (!J->TryGetStringField(TEXT("map_title"), S.MapTitle) || S.MapTitle.IsEmpty()) S.MapTitle = S.Map;
				J->TryGetStringField(TEXT("fog"), S.Fog);
				S.Units = Cnt(J, TEXT("units"));
				S.Structures = Cnt(J, TEXT("structures"));
				S.Orders = Cnt(J, TEXT("orders"));
				double E = 0.0;
				J->TryGetNumberField(TEXT("end_s"), E);
				S.EndS = float(E);
				J->TryGetBoolField(TEXT("lab"), S.bLab);
				bool bHud = true;
				if (J->TryGetBoolField(TEXT("hud"), bHud)) S.bHud = bHud;
				S.Index = I;
				All.Add(MoveTemp(S));
			}
		}
	}
	Armed = FRTSShell::Handlers().ArmedScenario ? FRTSShell::Handlers().ArmedScenario() : FString();
	FParse::Value(FCommandLine::Get(), TEXT("-RTSScriptArmed="), Armed);   // a foto de teste com uma cena armada (sem rodar a cena)
	Query.Reset();
	bSearchFocus = false;
	Hover = FHit();
	HoverEase.Reset();
	Scroll = ScrollTo = 0.0f;
	Sel = All.Num() ? All.Num() - 1 : -1;
	for (int32 I = 0; I < All.Num(); ++I) if (!Armed.IsEmpty() && All[I].Name == Armed) Sel = I;
	Pick(Sel, true);
	Scroll = ScrollTo;
	UE_LOG(LogTemp, Log, TEXT("RTS telas: SCRIPT, %d cenas%s"), All.Num(), Armed.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", armada %s"), *Armed));
}

TArray<int32> SRTSScript::List() const
{
	const FString Q = Query.TrimStartAndEnd().ToLower();
	TArray<int32> L;
	for (int32 I = 0; I < All.Num(); ++I)
	{
		const FScene& S = All[I];
		if (bLabOnly && !S.bLab) continue;
		if (!Q.IsEmpty() && !(S.Name + TEXT(" ") + S.Map + TEXT(" ") + S.MapTitle + TEXT(" ") + S.About).ToLower().Contains(Q)) continue;
		L.Add(I);
	}
	if (SortK == 1) L.StableSort([&](int32 A, int32 B) { return All[A].Name.Compare(All[B].Name, ESearchCase::IgnoreCase) < 0; });
	else if (SortK == 2) L.StableSort([&](int32 A, int32 B) { const int32 C = All[A].Map.Compare(All[B].Map, ESearchCase::IgnoreCase); return C != 0 ? C < 0 : All[A].Index > All[B].Index; });
	else L.StableSort([&](int32 A, int32 B) { return All[A].Index > All[B].Index; });
	return L;
}

float SRTSScript::MaxScroll() const
{
	return FMath::Max(0.0f, List().Num() * ScS::ROW_STEP - 4.0f - (ScS::ROWS_B - RowsTop()));
}

void SRTSScript::Pick(int32 Scene, bool bScroll)
{
	if (!All.IsValidIndex(Scene)) return;
	Sel = Scene;
	if (!bScroll) return;
	// o scrollIntoView({ block: 'nearest' }) da página
	const int32 I = List().IndexOfByKey(Scene);
	if (I == INDEX_NONE) return;
	const float Top = I * ScS::ROW_STEP, Bottom = Top + ScS::ROW_H, View = ScS::ROWS_B - RowsTop();
	if (Top < ScrollTo) ScrollTo = Top;
	else if (Bottom > ScrollTo + View) ScrollTo = Bottom - View;
	ScrollTo = FMath::Clamp(ScrollTo, 0.0f, MaxScroll());
}

void SRTSScript::Run()
{
	if (!All.IsValidIndex(Sel)) return;
	UE_LOG(LogTemp, Log, TEXT("RTS telas: SCRIPT, RUN %s"), *All[Sel].Name);
	if (FRTSShell::Handlers().RunScenario) FRTSShell::Handlers().RunScenario(All[Sel].Name);
}

void SRTSScript::Back()
{
	FRTSShell::Show(TEXT("menu.html"));
}

float SRTSScript::HoverOf(EHit Kind, int32 A) const
{
	const float* V = HoverEase.Find((uint32(Kind) << 16) | uint32(A + 1));
	return V ? FMath::Clamp(*V, 0.0f, 1.0f) : 0.0f;
}

SRTSScript::FHit SRTSScript::HitAt(FVector2f P) const
{
	if (In(BackRect(), P)) return { EHit::Back, -1 };
	if (In(ScS::SEARCH, P)) return { EHit::Search, -1 };
	for (int32 K = 0; K < 4; ++K) if (In(ScS::CHIP[K], P)) return { EHit::Chip, K };
	if (bArmed() && In(ScS::ARMED, P)) return { EHit::Armed, -1 };
	if (In(ScS::RUN, P)) return { EHit::Run, -1 };
	if (P.X >= 110 && P.X <= 110 + ScS::ROW_W && P.Y >= RowsTop() && P.Y <= ScS::ROWS_B)
	{
		const TArray<int32> L = List();
		const float Y = P.Y - RowsTop() + Scroll;
		const int32 I = FMath::FloorToInt(Y / ScS::ROW_STEP);
		if (L.IsValidIndex(I) && Y - I * ScS::ROW_STEP <= ScS::ROW_H) return { EHit::Row, L[I] };
	}
	return FHit();
}

void SRTSScript::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	{
		// (06/11) o teste de ponta a ponta: a cena armada roda sozinha 2 s depois do SCRIPT abrir (uma vez por processo)
		static double AutoAt = -1.0;
		static bool bAutoDone = false;
		if (!bAutoDone && !Armed.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("RTSScriptAutoRun")))
		{
			if (AutoAt < 0.0) AutoAt = Now + 2.0;
			else if (Now >= AutoAt) { bAutoDone = true; UE_LOG(LogTemp, Log, TEXT("RTS telas: SCRIPT, o teste roda %s"), *Armed); Run(); }
		}
	}
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	const uint32 HKey = Hover.Kind != EHit::None && Hover.Kind != EHit::Search ? (uint32(Hover.Kind) << 16) | uint32(Hover.A + 1) : 0u;
	if (HKey) HoverEase.FindOrAdd(HKey);
	for (auto It = HoverEase.CreateIterator(); It; ++It)
	{
		It.Value() = FMath::Clamp(It.Value() + (It.Key() == HKey ? Dt : -Dt) / 0.12f, 0.0f, 1.0f);
		if (It.Value() <= 0.0f && It.Key() != HKey) It.RemoveCurrent();
	}
	ScrollTo = FMath::Clamp(ScrollTo, 0.0f, MaxScroll());
	Scroll += (ScrollTo - Scroll) * (1.0f - FMath::Exp(-Dt * 16.0f));
	if (FMath::Abs(ScrollTo - Scroll) < 0.25f) Scroll = ScrollTo;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SRTSScript::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return FReply::Handled();   // a foto de teste fica parada
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	if (!(H == Hover) && H.Kind != EHit::None && H.Kind != EHit::Search) RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	Hover = H;
	return FReply::Handled();
}

void SRTSScript::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return;
	Hover = FHit();
}

FReply SRTSScript::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	if (H.Kind != EHit::None && H.Kind != EHit::Search) RTSShellAudio::Click();
	bSearchFocus = H.Kind == EHit::Search;
	if (bSearchFocus) CaretT0 = Now;
	switch (H.Kind)
	{
	case EHit::Back: Back(); break;
	case EHit::Chip:
		if (H.A < 3) SortK = H.A; else bLabOnly = !bLabOnly;
		break;
	case EHit::Armed:
		for (int32 I = 0; I < All.Num(); ++I) if (All[I].Name == Armed) Pick(I, true);
		break;
	case EHit::Row: Pick(H.A, true); break;
	case EHit::Run: Run(); break;
	default: break;
	}
	return FReply::Handled();
}

FReply SRTSScript::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	if (H.Kind == EHit::Row) { Pick(H.A, true); Run(); return FReply::Handled(); }
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SRTSScript::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ScrollTo = FMath::Clamp(ScrollTo - MouseEvent.GetWheelDelta() * 100.0f, 0.0f, MaxScroll());
	return FReply::Handled();
}

FReply SRTSScript::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// as teclas da página: o Esc limpa a busca ou volta, o Enter roda, as setas andam na lista; as letras vão pra busca
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Escape)
	{
		if (!Query.IsEmpty()) Query.Reset();
		else Back();
		return FReply::Handled();
	}
	if (K == EKeys::Enter) { Run(); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::Up)
	{
		const TArray<int32> L = List();
		const int32 I = L.IndexOfByKey(Sel);
		const int32 N = FMath::Clamp(I + (K == EKeys::Down ? 1 : -1), 0, L.Num() - 1);
		if (L.IsValidIndex(N)) Pick(L[N], true);
		return FReply::Handled();
	}
	if (K == EKeys::BackSpace && bSearchFocus)
	{
		if (InKeyEvent.IsControlDown())
		{
			Query.TrimEndInline();
			int32 Sp = INDEX_NONE;
			Query = Query.FindLastChar(TEXT(' '), Sp) ? Query.Left(Sp + 1) : FString();
		}
		else if (Query.Len()) Query.LeftChopInline(1);
		CaretT0 = Now;
		return FReply::Handled();
	}
	if (K == EKeys::V && InKeyEvent.IsControlDown() && bSearchFocus)
	{
		FString Clip;
		FPlatformApplicationMisc::ClipboardPaste(Clip);
		Clip.ReplaceInline(TEXT("\r"), TEXT(" "));
		Clip.ReplaceInline(TEXT("\n"), TEXT(" "));
		Query = (Query + Clip).Left(60);
		CaretT0 = Now;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SRTSScript::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR C = InCharacterEvent.GetCharacter();
	if (C < 32 || C == 127 || (InCharacterEvent.IsControlDown() && !InCharacterEvent.IsAltDown())) return FReply::Unhandled();
	bSearchFocus = true;   // a letra fora da busca leva pra ela (a página)
	if (Query.Len() < 60) Query.AppendChar(C);
	CaretT0 = Now;
	// a busca nova escolhe a 1ª da lista se a escolhida saiu (o oninput)
	const TArray<int32> L = List();
	if (L.Num() && !L.Contains(Sel)) Pick(L[0], true);
	ScrollTo = FMath::Clamp(ScrollTo, 0.0f, MaxScroll());
	return FReply::Handled();
}

FCursorReply SRTSScript::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (Hover.Kind == EHit::Search) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
	return FCursorReply::Cursor(Hover.Kind != EHit::None ? EMouseCursor::Hand : EMouseCursor::Default);
}

// ---------------------------------------------------------------------------------------------------------------------------- a pintura

int32 SRTSScript::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry S = StageFit(AllottedGeometry);
	int32 L = LayerId;
	Backdrop(Out, L, AllottedGeometry, S, TEXT("script_glow.png"));
	PaintHead(Out, L, S);
	PaintList(Out, L, S);
	PaintCard(Out, L, S);
	BackButton(Out, L, S, HoverOf(EHit::Back));
	{
		// as cantoneiras em ciano (a tela do desenvolvimento)
		FMesh M(S);
		const FColor Cc = ScS::DevV(0.45f);
		M.Rect(40, 40, 24, 2, Cc); M.Rect(40, 40, 2, 24, Cc);
		M.Rect(1856, 40, 24, 2, Cc); M.Rect(1878, 40, 2, 24, Cc);
		M.Rect(40, 1038, 24, 2, Cc); M.Rect(40, 1016, 2, 24, Cc);
		M.Rect(1856, 1038, 24, 2, Cc); M.Rect(1878, 1016, 2, 24, Cc);
		M.Draw(Out, L++, White());
	}
	return L;
}

void SRTSScript::PaintHead(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	Text(Out, L, S, TEXT("DEVELOPMENT"), Font(EFace::Barlow600, 16, 8), 110, 105, ScS::Dev());
	Dashed(Out, L, S, { FVector2f(307.08f, 88.5f), FVector2f(469.72f, 88.5f), FVector2f(469.72f, 108.5f), FVector2f(307.08f, 108.5f), FVector2f(307.08f, 88.5f) }, ScS::Dev(0.6f), 1.0f, 3.0f);
	Text(Out, L, S, TEXT("NOT IN THE RELEASE"), Font(EFace::Barlow700, 11, 3), 315.58f, 103, ScS::Dev());
	Text(Out, L, S, TEXT("SCRIPT · TEST SCENES"), Font(EFace::Shoulders900, 62, 2), 110, 174, FLinearColor::White);
	const float W = Text(Out, L, S, TEXT("Ready-made matches with a fixed seed, camera and orders. The same as "), Font(EFace::Barlow500, 17, 1), 110, 206.89f, ScS::Mute());
	const float W2 = Text(Out, L, S, TEXT("game.ps1 view -Scenario <name>"), Font(EFace::Mono600, 14, 1), 110 + W, 206.89f, ScS::Dev(0.95f));
	Text(Out, L, S, TEXT("."), Font(EFace::Barlow500, 17, 1), 110 + W + W2, 206.89f, ScS::Mute());
	++L;
}

void SRTSScript::PaintList(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const TArray<int32> Ls = List();
	// a busca (a borda em ciano com o foco), o contador e os filtros
	{
		FMesh M(S);
		BoxRect(M, ScS::SEARCH, CssV(16, 21, 19, 0.8f), bSearchFocus ? ScS::DevV(0.7f) : CssV(230, 220, 190, 0.2f));
		for (int32 K = 0; K < 4; ++K)
		{
			const bool bOn = K < 3 ? SortK == K : bLabOnly;
			const float H = HoverOf(EHit::Chip, K);
			BoxRect(M, ScS::CHIP[K], bOn ? CssV(120, 190, 220, 0.07f) : FColor(0, 0, 0, 0),
				bOn ? ScS::DevV(0.7f) : FMath::Lerp(Css(230, 220, 190, 0.14f), Css(230, 220, 190, 0.3f), H).ToFColorSRGB());
		}
		if (bArmed())
		{
			M.Rect(ScS::ARMED.X, ScS::ARMED.Y, ScS::ARMED.Z, ScS::ARMED.W, CssV(216, 173, 78, 0.06f));
		}
		M.Draw(Out, L++, White());
	}
	{
		const FLinearColor Ic = Css(230, 220, 190, 0.6f);
		Lines(Out, L, S, Arc(FVector2f(132.875f, 273.875f), 4.875f, 0.0f, 2.0f * PI, 28), Ic, 1.5f);
		Lines(Out, L, S, { FVector2f(136.625f, 277.625f), FVector2f(140.75f, 281.75f) }, Ic, 1.5f);
	}
	const FSlateFontInfo Qf = Font(EFace::Barlow600, 18, 1);
	if (Query.IsEmpty()) Text(Out, L, S, TEXT("Search by name, map or what it tests"), Qf, 153, 281, Css(238, 241, 244, 0.35f));
	else Text(Out, L, S, ScS::Fit(Query, Qf, 560.0f, Scale), Qf, 153, 281, FLinearColor::White);
	if (bSearchFocus && FMath::Fmod(float(Now - CaretT0), 1.0f) < 0.5f)
	{
		FMesh M(S);
		M.Rect(153 + (Query.IsEmpty() ? 0.0f : TextWidth(ScS::Fit(Query, Qf, 560.0f, Scale), Qf, Scale)), 264, 1, 22, CssV(255, 255, 255));
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, FString::Printf(TEXT("%d / %d"), Ls.Num(), All.Num()), Font(EFace::Mono600, 13), 804.04f, 279.5f, Css(230, 220, 190, 0.5f), 2);
	for (int32 K = 0; K < 4; ++K)
	{
		const bool bOn = K < 3 ? SortK == K : bLabOnly;
		Text(Out, L, S, ScS::CHIP_T[K], Font(EFace::Barlow700, 13, 2), ScS::CHIP[K].X + 15, 280,
			bOn ? ScS::Dev() : FMath::Lerp(Css(238, 241, 244, 0.6f), FLinearColor::White, HoverOf(EHit::Chip, K)));
	}
	// a cena que veio pela linha de comando (clicar escolhe; o Enter roda)
	if (bArmed())
	{
		const FVector4f A = ScS::ARMED;
		Dashed(Out, L, S, { FVector2f(A.X + 0.5f, A.Y + 0.5f), FVector2f(A.X + A.Z - 0.5f, A.Y + 0.5f), FVector2f(A.X + A.Z - 0.5f, A.Y + A.W - 0.5f),
			FVector2f(A.X + 0.5f, A.Y + A.W - 0.5f), FVector2f(A.X + 0.5f, A.Y + 0.5f) }, Ac(0.7f), 1.0f, 3.0f);
		const float W = Text(Out, L, S, TEXT("FROM THE COMMAND LINE"), Font(EFace::Barlow700, 12, 3), 127, 345.5f, Ac());
		Text(Out, L, S, Armed, Font(EFace::Shoulders800, 22, 2), 127 + W + 16, 349, FLinearColor::White);
		Text(Out, L, S, TEXT("ENTER RUNS IT"), Font(EFace::Mono600, 13), 1153, 345, Css(238, 241, 244, 0.6f), 2);
	}
	// o cabeçalho
	const float Th = RowsTop() - 10.0f;
	const FSlateFontInfo Hf = Font(EFace::Barlow600, 11, 4);
	Text(Out, L, S, TEXT("SCENE"), Hf, 126, Th, Css(230, 220, 190, 0.45f));
	Text(Out, L, S, TEXT("WHAT IT TESTS"), Hf, 426, Th, Css(230, 220, 190, 0.45f));
	Text(Out, L, S, TEXT("MAP"), Hf, 874, Th, Css(230, 220, 190, 0.45f));
	Text(Out, L, S, TEXT("LENGTH"), Hf, 1154, Th, Css(230, 220, 190, 0.45f), 2);
	++L;
	// as cenas (rolando)
	const float Top0 = RowsTop();
	Out.PushClip(FSlateClippingZone(S.MakeChild(FVector2f(1062, ScS::ROWS_B - Top0), FSlateLayoutTransform(FVector2f(109, Top0)))));
	if (!Ls.Num())
	{
		Text(Out, L, S, TEXT("NO SCENE MATCHES"), Font(EFace::Barlow700, 15, 4), 110 + ScS::ROW_W * 0.5f, Top0 + 58, ScS::Mute(), 1);
	}
	const FString Q = Query.TrimStartAndEnd();
	for (int32 I = 0; I < Ls.Num(); ++I)
	{
		const float Y = Top0 + I * ScS::ROW_STEP - Scroll;
		if (Y + ScS::ROW_H < Top0 || Y > ScS::ROWS_B) continue;
		const FScene& Sc = All[Ls[I]];
		const bool bOn = Ls[I] == Sel;
		const float H = HoverOf(EHit::Row, Ls[I]);
		{
			FMesh M(S);
			BoxRect(M, FVector4f(110, Y, ScS::ROW_W, ScS::ROW_H), bOn ? CssV(120, 190, 220, 0.06f) : CssV(16, 21, 19, 0.62f),
				bOn ? ScS::DevV(0.8f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.28f), H).ToFColorSRGB());
			if (bOn)
			{
				// a barra da escolhida, com o brilho (0 0 12px)
				GlowRect(M, FVector4f(109, Y - 1 + 1, 4, ScS::ROW_H), 0x78bedc, 0.7f, 12.0f);
				M.Rect(109, Y, 4, ScS::ROW_H, ScS::DevV());
			}
			M.Draw(Out, L++, White());
		}
		ScS::Marked(Out, L, S, Sc.Name, Q, Font(EFace::Mono600, 17), 127, Y + 35, 286, FLinearColor::White, Scale);
		ScS::Marked(Out, L, S, ScS::FirstLine(Sc.About), Q, Font(EFace::Barlow500, 14), 427, Y + 34.5f, 424, Css(238, 241, 244, 0.62f), Scale);
		ScS::Marked(Out, L, S, Sc.MapTitle.ToUpper(), Q, Font(EFace::Barlow600, 13, 1.5f), 865, Y + 34, 190, Css(230, 220, 190, 0.7f), Scale);
		Text(Out, L, S, ScS::Dur(Sc.EndS), Font(EFace::Mono600, 13), 1145, Y + 33.5f, Css(238, 241, 244, 0.55f), 2);
		++L;
	}
	const float Content = Ls.Num() * ScS::ROW_STEP - 4.0f, View = ScS::ROWS_B - Top0;
	if (Content > View)
	{
		FMesh M(S);
		M.Rect(1162, Top0 + Scroll / Content * View, 8, View * View / Content, CssV(230, 220, 190, 0.18f));
		M.Draw(Out, L++, White());
	}
	Out.PopClip();
}

void SRTSScript::PaintCard(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const float X = 1279.0f, W = 502.0f;
	const FScene* Sc = All.IsValidIndex(Sel) ? &All[Sel] : nullptr;
	// o RUN: a faixa ciano com o brilho; o espaçamento abre no hover
	const float Hr = HoverOf(EHit::Run);
	{
		FMesh M(S);
		BoxRect(M, ScS::CARD, CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
		GlowRect(M, ScS::RUN, 0x78bedc, 0.25f + 0.2f * Hr, 40.0f + 20.0f * Hr);
		M.Rect(ScS::RUN.X, ScS::RUN.Y, ScS::RUN.Z, ScS::RUN.W, ScS::DevV());
		M.Draw(Out, L++, White());
	}
	{
		const float Ls = 6.0f + 2.0f * Hr;
		const FSlateFontInfo F = Font(EFace::Shoulders900, 30, Ls);
		const float Tw = TextWidth(TEXT("RUN SCENE"), F, Scale), Total = Tw + 14.0f + 55.0f;
		const float X0 = ScS::RUN.X + (ScS::RUN.Z - Total) * 0.5f;
		const FLinearColor Ink = Css(8, 16, 15);
		Text(Out, L, S, TEXT("RUN SCENE"), F, X0, 986, Ink);
		const float Kx = X0 + Tw + 14.0f;
		const FLinearColor Kc(Ink.R, Ink.G, Ink.B, 0.6f);
		Lines(Out, L, S, { FVector2f(Kx + 0.5f, 963.0f), FVector2f(Kx + 54.5f, 963.0f), FVector2f(Kx + 54.5f, 985.0f), FVector2f(Kx + 0.5f, 985.0f), FVector2f(Kx + 0.5f, 963.0f) }, Kc, 1.0f);
		Text(Out, L, S, TEXT("ENTER"), Font(EFace::Mono600, 12, 1), Kx + 7, 978.5f, Kc);
	}
	if (!Sc)
	{
		Text(Out, L, S, TEXT("NO SCENE"), Font(EFace::Barlow600, 12, 5), X, 289, ScS::Dev(0.8f));
		++L;
		return;
	}
	Out.PushClip(FSlateClippingZone(S.MakeChild(FVector2f(ScS::CARD.Z - 2, ScS::CARD.W - 2), FSlateLayoutTransform(FVector2f(ScS::CARD.X + 1, ScS::CARD.Y + 1)))));
	Text(Out, L, S, TEXT("SCENE"), Font(EFace::Barlow600, 12, 5), X, 289, ScS::Dev(0.8f));
	// o nome (quebra em qualquer letra, 30,8 por linha), o mapa, os números, o texto, as etiquetas e o comando
	const FSlateFontInfo Nf = Font(EFace::Mono600, 28);   // o 700 do CSS cai no 600 (a página só carrega até o 600)
	const TArray<FString> Nl = ScS::Wrap(Sc->Name, Nf, W, Scale, true);
	for (int32 I = 0; I < Nl.Num(); ++I) Text(Out, L, S, Nl[I], Nf, X, 324 + 30.8f * I, FLinearColor::White);
	float Y = 299 + 30.8f * FMath::Max(1, Nl.Num()) + 14;
	const float Mw = Text(Out, L, S, Sc->MapTitle.ToUpper(), Font(EFace::Barlow700, 15, 3), X, Y + 15, Css(230, 220, 190, 0.8f));
	Text(Out, L, S, Sc->Map, Font(EFace::Mono500, 13), X + Mw + 8, Y + 15, ScS::Mute());
	Y += 19 + 18;
	{
		FMesh M(S);
		for (int32 K = 0; K < 4; ++K) BoxRect(M, FVector4f(X + K * 127.5f, Y, 119.5f, 70), FColor(0, 0, 0, 0), CssV(230, 220, 190, 0.12f));
		M.Draw(Out, L++, White());
	}
	const FString Nums[4] = { FString::FromInt(Sc->Units), FString::FromInt(Sc->Structures), FString::FromInt(Sc->Orders), ScS::Dur(Sc->EndS) };
	const TCHAR* Labs[4] = { TEXT("UNITS"), TEXT("BUILDINGS"), TEXT("ORDERS"), TEXT("LENGTH") };
	for (int32 K = 0; K < 4; ++K)
	{
		Text(Out, L, S, Nums[K], Font(EFace::Shoulders800, 24, 1), X + K * 127.5f + 13, Y + 35, FLinearColor::White);
		Text(Out, L, S, Labs[K], Font(EFace::Barlow600, 11, 3), X + K * 127.5f + 13, Y + 56, ScS::Mute());
	}
	Y += 70 + 18;
	const FSlateFontInfo Pf = Font(EFace::Barlow500, 17);
	const TArray<FString> Pl = ScS::Wrap(Sc->About, Pf, W, Scale, false);
	for (int32 I = 0; I < Pl.Num(); ++I) Text(Out, L, S, Pl[I], Pf, X, Y + 19 + 25.5f * I, Css(238, 241, 244, 0.82f));
	Y += 25.5f * Pl.Num() + 16;
	// as etiquetas: LAB, HUD ou NO HUD, a névoa, ARMED
	TArray<FString> Tags;
	if (Sc->bLab) Tags.Add(TEXT("LAB"));
	Tags.Add(Sc->bHud ? TEXT("HUD") : TEXT("NO HUD"));
	if (!Sc->Fog.IsEmpty()) Tags.Add(TEXT("FOG ") + Sc->Fog.ToUpper());
	if (Sc->Name == Armed) Tags.Add(TEXT("ARMED"));
	{
		const FSlateFontInfo Tf = Font(EFace::Barlow700, 11, 2);
		float Tx = X;
		FMesh M(S);
		TArray<TPair<FString, FVector2f>> Put;
		for (const FString& T : Tags)
		{
			const float Tw = TextWidth(T, Tf, Scale) + 18.0f;
			if (Tx + Tw > X + W && Tx > X) { Tx = X; Y += 23 + 6; }
			BoxRect(M, FVector4f(Tx, Y, Tw, 23), FColor(0, 0, 0, 0), CssV(230, 220, 190, 0.18f));
			Put.Add({ T, FVector2f(Tx + 9, Y + 16) });
			Tx += Tw + 6;
		}
		M.Draw(Out, L++, White());
		for (const TPair<FString, FVector2f>& P : Put) Text(Out, L, S, P.Key, Tf, P.Value.X, P.Value.Y, Css(238, 241, 244, 0.7f));
	}
	Y += 23 + 18;
	{
		const FSlateFontInfo Cf = Font(EFace::Mono500, 13);
		const TArray<FString> Cl = ScS::Wrap(TEXT("game.ps1 view -Scenario ") + Sc->Name, Cf, W - 26, Scale, true);
		const float H = 19.0f + 20.0f * Cl.Num();
		FMesh M(S);
		BoxRect(M, FVector4f(X, Y, W, H), CssV(0, 0, 0, 0.35f), ScS::DevV(0.2f));
		M.Draw(Out, L++, White());
		for (int32 I = 0; I < Cl.Num(); ++I) Text(Out, L, S, Cl[I], Cf, X + 13, Y + 24 + 20.0f * I, ScS::Dev(0.9f));
	}
	Out.PopClip();
	++L;
}
