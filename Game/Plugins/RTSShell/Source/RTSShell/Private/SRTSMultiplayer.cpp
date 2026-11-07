#include "SRTSMultiplayer.h"
#include "RTSShell.h"
#include "RTSShellAudio.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

using namespace RTSShellUI;

// As medidas do Chrome (Tools/multiplayer_layout.json). Num namespace com nome e sempre com o prefixo: na compilação unificada da
// Unreal os namespaces anônimos dos .cpp do módulo viram um só, e um nome repetido entre as telas colide.
namespace RTSMpScreen
{
	const float SRC_Y[3] = { 203.89f, 297.89f, 391.89f };
	const TCHAR* SRC_T[3] = { TEXT("ONLINE"), TEXT("LAN"), TEXT("DIRECT IP") };
	const TCHAR* SRC_S[3] = { TEXT("STEAM · FRIENDS FIRST"), TEXT("YOUR NETWORK"), TEXT("JOIN BY ADDRESS") };
	const FVector4f HOST(110, 495.89f, 400, 62);
	const FVector4f SEARCH(560, 88, 414.94f, 46);
	const FVector4f CHIP[3] = { FVector4f(984.94f, 88, 97.3f, 46), FVector4f(1092.23f, 88, 119.42f, 46), FVector4f(1221.66f, 88, 109.53f, 46) };
	const TCHAR* CHIP_T[3] = { TEXT("HIDE FULL"), TEXT("HIDE IN GAME"), TEXT("MY VERSION") };
	const float TH_X[5] = { 560, 920, 1130, 1250, 1330 };
	const float TH_W[5] = { 360, 210, 120, 80, 70 };
	const TCHAR* TH_T[5] = { TEXT("ROOM"), TEXT("MAP"), TEXT("PLAYERS"), TEXT("MODE"), TEXT("PING") };
	const float TH_CHEV[5] = { 625.41f, 975.11f, 1223.06f, 1315.13f, 1391.28f };   // o centro de cada setinha (depois do texto)
	constexpr float ROWS_Y = 178.0f, ROWS_B = 1030.0f, ROW_H = 64.0f, ROW_STEP = 70.0f, LIST_X = 560.0f, LIST_W = 840.0f;
	const FVector4f JOIN(1440, 958, 380, 72);
	constexpr float PV_X = 1440.0f, PV_Y = 88.0f, PV_S = 380.0f;
	// a paleta de 8 do multiplayer (COLORS da página)
	const uint32 PCOL[8] = { 0x3d7fe0, 0xd83a2e, 0xe3b23c, 0x4caf50, 0xef7f22, 0x3fc6d6, 0x9a5ad8, 0xe86aa8 };
	const TCHAR* VERSION = TEXT("0.5.2");

	FString Thousands(int32 V)
	{
		FString S = FString::FromInt(V), Out;
		for (int32 I = 0; I < S.Len(); ++I)
		{
			if (I && (S.Len() - I) % 3 == 0) Out += TEXT(",");
			Out += S[I];
		}
		return Out;
	}
	/** As cores com a opacidade do grupo (o .rm.off a 0,5, as fontes apagadas a 0,35). */
	FLinearColor Op(FLinearColor C, float A) { C.A *= A; return C; }
	FColor OpV(FColor C, float A) { C.A = uint8(FMath::RoundToInt(C.A * A)); return C; }
	/** A quebra de linha do CSS (palavra por palavra, na largura dada). */
	TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& F, float W, float Scale)
	{
		TArray<FString> Words, Lines;
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
	/** A coroa do anfitrião (o svg 16 × 12 da página), em triângulos (o polígono não é convexo). */
	void Crown(FMesh& M, FVector2f O, FColor C)
	{
		const auto P = [&](float X, float Y) { return M.Vert(O + FVector2f(X, Y), FVector2f(0, 0), C); };
		const int32 A = P(1, 11), B = P(15, 11), Cc = P(13, 3), D = P(10, 7), E = P(8, 1), F = P(6, 7), G = P(3, 3);
		M.Tri(B, Cc, D); M.Tri(D, E, F); M.Tri(F, G, A); M.Tri(A, B, D); M.Tri(A, D, F);
	}
}
namespace MpS = RTSMpScreen;

void SRTSMultiplayer::Construct(const FArguments& InArgs)
{
	// os mapas (o nome e as bases, pra prévia) da mesma ficha da sala do skirmish
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *(PluginDir() / TEXT("Resources/Skirmish/maps.json"))))
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FMapInfo M;
				M.Real = O->GetStringField(TEXT("real"));
				M.Name = O->GetStringField(TEXT("name"));
				for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("starts")))
				{
					const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
					M.Starts.Add(FVector2f(float(XY[0]->AsNumber()), float(XY[1]->AsNumber())));
				}
				Maps.Add(MoveTemp(M));
			}
		}
	}
	Restart();
}

void SRTSMultiplayer::Restart()
{
	Query.Reset();
	bSearchFocus = false;
	Scroll = ScrollTo = 0.0f;
	Hover = FHit();
	HoverEase.Reset();
	LastJson.Reset();
	Rooms.Reset();
	Sel = 0;
	// a página no jogo: a LAN procurando (o ONLINE e o IP direto ainda não existem)
	if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(TEXT("browse"));
	Poll();
	PollAt = Now + 1.0;
}

void SRTSMultiplayer::Poll()
{
	// o poll da página: o api/lan.json do jogo (as salas achadas na rede); -RTSLanFake=<arquivo.json> pras fotos de teste
	FString Json, Fake;
	bool bOk = false;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSLanFake="), Fake)) bOk = FFileHelper::LoadFileToString(Json, *Fake);
	else if (FRTSShell::Handlers().Api) bOk = FRTSShell::Handlers().Api(TEXT("lan.json"), Json);
	if (!bOk || Json == LastJson) return;
	LastJson = Json;
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* Games = nullptr;
	Rooms.Reset();
	if (O->TryGetArrayField(TEXT("games"), Games))
	{
		for (const TSharedPtr<FJsonValue>& V : *Games)
		{
			const TSharedPtr<FJsonObject> G = V->AsObject();
			if (!G.IsValid()) continue;
			FRoom R;
			FString Name;
			double Players = 0.0, Index = -1.0;
			bool bOpen = false;
			G->TryGetStringField(TEXT("name"), Name);
			G->TryGetNumberField(TEXT("players"), Players);
			G->TryGetNumberField(TEXT("index"), Index);
			G->TryGetBoolField(TEXT("open"), bOpen);
			R.Name = R.Host = Name.ToUpper();
			R.Players = int32(Players);
			R.Mode = FString::Printf(TEXT("%d IN"), R.Players);
			R.Max = 8;
			R.Ping = 2;   // a LAN de hoje não mede o ping (a página mostrava 2 MS)
			R.Ver = bOpen ? FString(MpS::VERSION) : FString(TEXT("?"));
			R.bGame = !bOpen;
			R.Cash = 10000;
			R.Sw = TEXT("UNLIMITED");
			R.Idx = int32(Index);
			Rooms.Add(R);
		}
	}
	if (!Rooms.IsValidIndex(Sel)) Sel = 0;
}

TArray<int32> SRTSMultiplayer::Visible() const
{
	// o rooms() da página: a busca no nome, no anfitrião e no mapa; os filtros; a ordem pela coluna escolhida (os amigos primeiro)
	const FString Q = Query.TrimStartAndEnd().ToUpper();
	TArray<int32> L;
	for (int32 I = 0; I < Rooms.Num(); ++I)
	{
		const FRoom& R = Rooms[I];
		const FString MapName = Maps.IsValidIndex(R.Map) ? Maps[R.Map].Name : FString();
		if (!Q.IsEmpty() && !(R.Name + TEXT(" ") + R.Host + TEXT(" ") + MapName).Contains(Q)) continue;
		if (bFull && Full(R)) continue;
		if (bInGame && R.bGame) continue;
		if (bMyVer && Bad(R)) continue;
		L.Add(I);
	}
	L.StableSort([&](int32 A, int32 B)
	{
		const FRoom& Ra = Rooms[A];
		const FRoom& Rb = Rooms[B];
		int32 C = 0;
		switch (SortK)
		{
		case 0: C = Ra.Name.Compare(Rb.Name, ESearchCase::CaseSensitive); break;
		case 1: C = (Maps.IsValidIndex(Ra.Map) ? Maps[Ra.Map].Name : FString()).Compare(Maps.IsValidIndex(Rb.Map) ? Maps[Rb.Map].Name : FString(), ESearchCase::CaseSensitive); break;
		case 2: C = (Ra.Max - Ra.Players) - (Rb.Max - Rb.Players); break;
		case 3: C = Ra.Mode.Compare(Rb.Mode, ESearchCase::CaseSensitive); break;
		default: C = Ra.Ping - Rb.Ping; break;
		}
		return bSortUp ? C < 0 : C > 0;
	});
	return L;
}

float SRTSMultiplayer::MaxScroll() const
{
	return FMath::Max(0.0f, Visible().Num() * MpS::ROW_STEP - (MpS::ROWS_B - MpS::ROWS_Y));
}

float SRTSMultiplayer::HoverOf(EHit Kind, int32 A) const
{
	const float* V = HoverEase.Find((uint32(Kind) << 16) | uint32(A + 1));
	return V ? FMath::Clamp(*V, 0.0f, 1.0f) : 0.0f;
}

void SRTSMultiplayer::Join(int32 Room)
{
	if (!Rooms.IsValidIndex(Room)) return;
	const FRoom& R = Rooms[Room];
	if (R.bGame) return;   // o join da página: a sala em jogo espera
	UE_LOG(LogTemp, Log, TEXT("RTS telas: multiplayer, JOIN %s (%d)"), *R.Name, R.Idx);
	if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(FString::Printf(TEXT("join:%d"), R.Idx));
}

void SRTSMultiplayer::Back()
{
	// sai da LAN no jogo (a página do menu do jogo volta pra principal) e volta pro menu
	if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(TEXT("back"));
	FRTSShell::Show(TEXT("menu.html"));
}

SRTSMultiplayer::FHit SRTSMultiplayer::HitAt(FVector2f P) const
{
	if (In(BackRect(), P)) return { EHit::Back, -1 };
	for (int32 K = 0; K < 3; ++K) if (In(FVector4f(110, MpS::SRC_Y[K], 400, 86), P)) return { EHit::Src, K };
	if (In(MpS::HOST, P)) return { EHit::Host, -1 };
	if (In(MpS::SEARCH, P)) return { EHit::Search, -1 };
	for (int32 K = 0; K < 3; ++K) if (In(MpS::CHIP[K], P)) return { EHit::Chip, K };
	for (int32 K = 0; K < 5; ++K) if (In(FVector4f(MpS::TH_X[K], 155, MpS::TH_W[K], 14), P)) return { EHit::Th, K };
	if (P.X >= MpS::LIST_X && P.X <= MpS::LIST_X + MpS::LIST_W && P.Y >= MpS::ROWS_Y && P.Y <= MpS::ROWS_B)
	{
		const TArray<int32> L = Visible();
		const float Y = P.Y - MpS::ROWS_Y + Scroll;
		const int32 I = FMath::FloorToInt(Y / MpS::ROW_STEP);
		if (L.IsValidIndex(I) && Y - I * MpS::ROW_STEP <= MpS::ROW_H) return { EHit::Row, L[I] };
	}
	if (In(MpS::JOIN, P)) return { EHit::Join, -1 };
	return FHit();
}

void SRTSMultiplayer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = InCurrentTime;
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	T += Dt;
	// as transições do CSS (.15 s) de cada peça com o mouse em cima
	const bool bEase = Hover.Kind != EHit::None && Hover.Kind != EHit::Search;
	const uint32 HKey = bEase ? (uint32(Hover.Kind) << 16) | uint32(Hover.A + 1) : 0u;
	if (HKey) HoverEase.FindOrAdd(HKey);
	for (auto It = HoverEase.CreateIterator(); It; ++It)
	{
		It.Value() = FMath::Clamp(It.Value() + (It.Key() == HKey ? Dt : -Dt) / 0.15f, 0.0f, 1.0f);
		if (It.Value() <= 0.0f && It.Key() != HKey) It.RemoveCurrent();
	}
	ScrollTo = FMath::Clamp(ScrollTo, 0.0f, MaxScroll());
	Scroll += (ScrollTo - Scroll) * (1.0f - FMath::Exp(-Dt * 16.0f));
	if (FMath::Abs(ScrollTo - Scroll) < 0.25f) Scroll = ScrollTo;
	// a lista se atualiza sozinha (o setInterval de 1 s da página)
	if (Now >= PollAt)
	{
		PollAt = Now + 1.0;
		Poll();
	}
	// o teste da rede de verdade: hospeda uma sala sozinho (a sala nativa da LAN abre com os dados reais do jogo)
	if (!bTestHosted && T > 1.5f && FParse::Param(FCommandLine::Get(), TEXT("RTSMpTestHost")))
	{
		bTestHosted = true;
		UE_LOG(LogTemp, Log, TEXT("RTS telas: teste da rede, HOST GAME"));
		if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(TEXT("host"));
	}
	// a sala escolhida sempre é uma das que aparecem (o renderList da página)
	const TArray<int32> L = Visible();
	if (L.Num() && !L.Contains(Sel)) Sel = L[0];
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SRTSMultiplayer::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return FReply::Handled();   // a foto de teste fica parada
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	const bool bLive = H.Kind != EHit::None && H.Kind != EHit::Search && !(H.Kind == EHit::Src && H.A != 1);
	if (!(H == Hover) && bLive) RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	Hover = (H.Kind == EHit::Src && H.A != 1) ? FHit() : H;   // o ONLINE e o IP direto não reagem (pointer-events: none)
	return FReply::Handled();
}

void SRTSMultiplayer::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScreenShot="), Shot)) return;
	Hover = FHit();
}

FReply SRTSMultiplayer::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	if (H.Kind != EHit::None && H.Kind != EHit::Search && !(H.Kind == EHit::Src && H.A != 1)) RTSShellAudio::Click();
	bSearchFocus = H.Kind == EHit::Search;
	if (bSearchFocus) CaretT0 = Now;
	switch (H.Kind)
	{
	case EHit::Back: Back(); break;
	case EHit::Host:
		UE_LOG(LogTemp, Log, TEXT("RTS telas: multiplayer, HOST GAME"));
		if (FRTSShell::Handlers().Lan) FRTSShell::Handlers().Lan(TEXT("host"));
		break;
	case EHit::Chip:
		if (H.A == 0) bFull = !bFull; else if (H.A == 1) bInGame = !bInGame; else bMyVer = !bMyVer;
		break;
	case EHit::Th:
		if (SortK == H.A) bSortUp = !bSortUp; else { SortK = H.A; bSortUp = true; }
		break;
	case EHit::Row: Sel = H.A; break;
	case EHit::Join:
		if (Rooms.IsValidIndex(Sel) && !Full(Rooms[Sel]) && !Bad(Rooms[Sel]) && !Rooms[Sel].bGame && Visible().Contains(Sel)) Join(Sel);
		break;
	default: break;
	}
	return FReply::Handled();
}

FReply SRTSMultiplayer::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// o duplo clique na sala entra (o ondblclick da página)
	const FHit H = HitAt(FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	if (H.Kind == EHit::Row) { Sel = H.A; Join(H.A); return FReply::Handled(); }
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SRTSMultiplayer::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ScrollTo = FMath::Clamp(ScrollTo - MouseEvent.GetWheelDelta() * 100.0f, 0.0f, MaxScroll());
	return FReply::Handled();
}

FReply SRTSMultiplayer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Escape)
	{
		// o Esc da página: com a busca escrita, limpa; senão volta pro menu
		if (bSearchFocus && !Query.IsEmpty()) { Query.Reset(); CaretT0 = Now; }
		else Back();
		return FReply::Handled();
	}
	if (!bSearchFocus) return FReply::Unhandled();
	if (K == EKeys::BackSpace)
	{
		if (InKeyEvent.IsControlDown())
		{
			Query.TrimEndInline();
			int32 Sp = INDEX_NONE;
			Query = Query.FindLastChar(TEXT(' '), Sp) ? Query.Left(Sp + 1) : FString();
		}
		else if (Query.Len()) Query.LeftChopInline(1);
		CaretT0 = Now;
	}
	else if (K == EKeys::V && InKeyEvent.IsControlDown())
	{
		FString Clip;
		FPlatformApplicationMisc::ClipboardPaste(Clip);
		Clip.ReplaceInline(TEXT("\r"), TEXT(" "));
		Clip.ReplaceInline(TEXT("\n"), TEXT(" "));
		Query = (Query + Clip).Left(40);
		CaretT0 = Now;
	}
	return FReply::Handled();
}

FReply SRTSMultiplayer::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR C = InCharacterEvent.GetCharacter();
	if (!bSearchFocus || C < 32 || C == 127 || (InCharacterEvent.IsControlDown() && !InCharacterEvent.IsAltDown())) return FReply::Unhandled();
	if (Query.Len() < 40) Query.AppendChar(C);
	CaretT0 = Now;
	return FReply::Handled();
}

FCursorReply SRTSMultiplayer::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (Hover.Kind == EHit::Search) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
	const bool bJoinOff = Hover.Kind == EHit::Join && (!Rooms.IsValidIndex(Sel) || Full(Rooms[Sel]) || Bad(Rooms[Sel]) || Rooms[Sel].bGame);
	const bool bHand = Hover.Kind != EHit::None && !bJoinOff;
	return FCursorReply::Cursor(bHand ? EMouseCursor::Hand : EMouseCursor::Default);
}

// ---------------------------------------------------------------------------------------------------------------------------- a pintura

int32 SRTSMultiplayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry S = StageFit(AllottedGeometry);
	int32 L = LayerId;
	Backdrop(Out, L, AllottedGeometry, S, TEXT("screens_glow.png"));
	PaintSide(Out, L, S);
	PaintList(Out, L, S);
	PaintDetail(Out, L, S);
	BackButton(Out, L, S, HoverOf(EHit::Back));
	{
		FMesh M(S);
		StageCorners(M);
		M.Draw(Out, L++, White());
	}
	return L;
}

void SRTSMultiplayer::PaintSide(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const FLinearColor Mute = Css(152, 161, 154);
	Text(Out, L, S, TEXT("MULTIPLAYER"), Font(EFace::Barlow600, 16, 8), 110, 104, Ac());
	Text(Out, L, S, TEXT("FIND A BATTLE"), Font(EFace::Shoulders900, 62, 2), 110, 172, FLinearColor::White);
	// de onde jogar: o ONLINE e o IP direto apagados (a página do jogo: opacity .35), a LAN escolhida com as cantoneiras
	{
		FMesh M(S);
		for (int32 K = 0; K < 3; ++K)
		{
			const FVector4f R(110, MpS::SRC_Y[K], 400, 86);
			const float H = K == 1 ? HoverOf(EHit::Src, K) : 0.0f;
			const FLinearColor Bg = FMath::Lerp(Css(16, 21, 19, 0.62f), Css(22, 28, 26, 0.78f), H);
			const FLinearColor Bd = K == 1 ? Ac(0.75f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.26f), H);
			BoxRect(M, R, Bg.ToFColorSRGB(), Bd.ToFColorSRGB());
			if (K == 1)
			{
				const FColor A = AcV();
				M.Rect(R.X - 4, R.Y - 4, 10, 2, A); M.Rect(R.X - 4, R.Y - 4, 2, 10, A);
				M.Rect(R.X + R.Z - 6, R.Y + R.W + 2, 10, 2, A); M.Rect(R.X + R.Z + 2, R.Y + R.W - 6, 2, 10, A);
			}
		}
		const float Hh = HoverOf(EHit::Host);
		BoxRect(M, MpS::HOST, FMath::Lerp(Css(216, 173, 78, 0.05f), Ac(), Hh).ToFColorSRGB(), AcV(0.8f));
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < 3; ++K)
	{
		const float Y = MpS::SRC_Y[K];
		Text(Out, L, S, MpS::SRC_T[K], Font(EFace::Shoulders800, 30, 3), 129, Y + 44, FLinearColor::White);
		Text(Out, L, S, MpS::SRC_S[K], Font(EFace::Barlow600, 13, 3), 129, Y + 66, Mute);
		if (K < 2) Text(Out, L, S, FString::Printf(TEXT("%d ROOMS"), K == 1 ? Rooms.Num() : 0), Font(EFace::Mono600, 13), 491, Y + 32, Css(230, 220, 190, 0.55f), 2);
	}
	++L;
	{
		// o .35 do ONLINE e do IP direto: o fundo do palco por cima, a 65%
		FMesh M(S);
		for (const int32 K : { 0, 2 }) M.Rect(110, MpS::SRC_Y[K], 400, 86, CssV(10, 13, 11, 0.65f));
		M.Draw(Out, L++, White());
	}
	{
		const float Hh = HoverOf(EHit::Host), Ls = 5.0f + 2.0f * Hh;
		Text(Out, L, S, TEXT("HOST GAME"), Font(EFace::Shoulders900, 28, Ls), 310, 537.89f, FMath::Lerp(Ac(), Css(10, 13, 11), Hh), 1);
	}
	// embaixo: a rede de hoje (o ponto verde com o brilho, LAN, a versão)
	{
		FMesh M(S);
		GlowRect(M, FVector4f(110, 1014, 8, 8), 0x78c878, 0.6f, 8.0f);
		Disc(M, FVector2f(114, 1018), 4, CssV(120, 200, 120));
		M.Draw(Out, L++, White());
	}
	const float Lw = Text(Out, L, S, TEXT("LAN"), Font(EFace::Shoulders800, 20, 2), 130, 1026, FLinearColor::White);
	Text(Out, L, S, TEXT("v0.5.2 · YOUR NETWORK"), Font(EFace::Barlow600, 14, 2), 130 + Lw + 12, 1023.5f, Mute);
	(void)Scale;
}

void SRTSMultiplayer::PaintList(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	// a busca, os filtros e o LIVE
	{
		FMesh M(S);
		BoxRect(M, MpS::SEARCH, CssV(16, 21, 19, 0.8f), CssV(230, 220, 190, 0.2f));
		for (int32 K = 0; K < 3; ++K)
		{
			const bool bOn = K == 0 ? bFull : K == 1 ? bInGame : bMyVer;
			const float H = HoverOf(EHit::Chip, K);
			BoxRect(M, MpS::CHIP[K], bOn ? CssV(216, 173, 78, 0.07f) : FColor(0, 0, 0, 0),
				bOn ? AcV(0.75f) : FMath::Lerp(Css(230, 220, 190, 0.14f), Css(230, 220, 190, 0.35f), H).ToFColorSRGB());
		}
		M.Draw(Out, L++, White());
	}
	{
		const FLinearColor Ic = Css(230, 220, 190, 0.6f);
		Lines(Out, L, S, Arc(FVector2f(582.875f, 109.875f), 4.875f, 0.0f, 2.0f * PI, 28), Ic, 1.5f);
		Lines(Out, L, S, { FVector2f(586.625f, 113.625f), FVector2f(590.75f, 117.75f) }, Ic, 1.5f);
	}
	const FSlateFontInfo Qf = Font(EFace::Barlow600, 17, 1);
	if (Query.IsEmpty()) Text(Out, L, S, TEXT("Search rooms, hosts, maps"), Qf, 603, 117, Css(230, 220, 190, 0.35f));
	else Text(Out, L, S, Query, Qf, 603, 117, FLinearColor::White);
	if (bSearchFocus && FMath::Fmod(float(Now - CaretT0), 1.0f) < 0.5f)
	{
		FMesh M(S);
		M.Rect(603 + (Query.IsEmpty() ? 0.0f : TextWidth(Query, Qf, Scale)), 101, 1, 21, CssV(255, 255, 255));
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < 3; ++K)
	{
		const bool bOn = K == 0 ? bFull : K == 1 ? bInGame : bMyVer;
		const FLinearColor C = bOn ? Ac() : FMath::Lerp(Css(238, 241, 244, 0.6f), FLinearColor::White, HoverOf(EHit::Chip, K));
		Text(Out, L, S, MpS::CHIP_T[K], Font(EFace::Barlow700, 13, 2), MpS::CHIP[K].X + 13, 116, C);
	}
	{
		// o LIVE: o anel e o ponteiro girando (3 s por volta)
		const FVector2f C(1354.19f, 111.0f);
		const float A = FMath::Fmod(T / 3.0f, 1.0f) * 2.0f * PI;
		Lines(Out, L, S, Arc(C, 6.75f, 0.0f, 2.0f * PI, 32), Css(216, 173, 78, 0.35f), 1.2f);
		Lines(Out, L, S, { C, C + FVector2f(FMath::Sin(A), -FMath::Cos(A)) * 6.75f }, Ac(), 1.2f);
		Text(Out, L, S, TEXT("LIVE"), Font(EFace::Mono600, 12), 1371.19f, 115.5f, Css(230, 220, 190, 0.55f));
	}
	// o cabeçalho que ordena: a coluna escolhida em âmbar, a setinha vira pra cima na ordem ao contrário
	for (int32 K = 0; K < 5; ++K)
	{
		const bool bOn = K == SortK;
		const FLinearColor C = bOn ? Ac() : FMath::Lerp(Css(230, 220, 190, 0.42f), Css(238, 241, 244, 0.8f), HoverOf(EHit::Th, K));
		Text(Out, L, S, MpS::TH_T[K], Font(EFace::Barlow600, 11.5f, 4), MpS::TH_X[K] + 14, 167, C);
		// o i de 6 px com as bordas da direita e de baixo, girado 45° (pra baixo) ou −135° e 3 px abaixo (pra cima)
		if (bOn && !bSortUp) Lines(Out, L, S, { FVector2f(MpS::TH_CHEV[K] - 4.2f, 166.6f), FVector2f(MpS::TH_CHEV[K], 162.4f), FVector2f(MpS::TH_CHEV[K] + 4.2f, 166.6f) }, C, 1.5f);
		else Chev(Out, L, S, FVector2f(MpS::TH_CHEV[K], 162.6f), C, 4.2f);
	}
	++L;
	// as salas (rolando dentro da área da lista)
	const TArray<int32> Vis = Visible();
	Out.PushClip(FSlateClippingZone(S.MakeChild(FVector2f(MpS::LIST_W + 2, MpS::ROWS_B - MpS::ROWS_Y), FSlateLayoutTransform(FVector2f(MpS::LIST_X - 1, MpS::ROWS_Y)))));
	if (!Vis.Num())
	{
		Text(Out, L, S, TEXT("NO ROOMS MATCH · HOST ONE"), Font(EFace::Barlow600, 15, 3), MpS::LIST_X + MpS::LIST_W * 0.5f, 253, Css(152, 161, 154), 1);
	}
	for (int32 I = 0; I < Vis.Num(); ++I)
	{
		const float Top = MpS::ROWS_Y + I * MpS::ROW_STEP - Scroll;
		if (Top + MpS::ROW_H < MpS::ROWS_Y || Top > MpS::ROWS_B) continue;
		PaintRow(Out, L, S, Vis[I], Top);
	}
	const float Content = Vis.Num() * MpS::ROW_STEP;
	if (Content > MpS::ROWS_B - MpS::ROWS_Y)
	{
		const float H = MpS::ROWS_B - MpS::ROWS_Y;
		FMesh M(S);
		M.Rect(MpS::LIST_X + MpS::LIST_W - 8.0f, MpS::ROWS_Y + Scroll / Content * H, 8.0f, H * H / Content, CssV(230, 220, 190, 0.18f));
		M.Draw(Out, L++, White());
	}
	Out.PopClip();
}

void SRTSMultiplayer::PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 Room, float Top) const
{
	const FRoom& R = Rooms[Room];
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const float A = Full(R) || Bad(R) ? 0.5f : 1.0f;   // o .rm.off
	const float H = HoverOf(EHit::Row, Room);
	const bool bOn = Room == Sel;
	const FString MapName = Maps.IsValidIndex(R.Map) ? Maps[R.Map].Name : FString();
	const FString Real = Maps.IsValidIndex(R.Map) ? Maps[R.Map].Real : FString(TEXT("World_TestSalt2"));
	{
		FMesh M(S);
		const FLinearColor Bg = FMath::Lerp(Css(16, 21, 19, 0.62f), Css(22, 28, 26, 0.8f), H);
		const FLinearColor Bd = bOn ? Ac(0.75f) : FMath::Lerp(Css(230, 220, 190, 0.10f), Css(230, 220, 190, 0.26f), H);
		BoxRect(M, FVector4f(MpS::LIST_X, Top, MpS::LIST_W, MpS::ROW_H), MpS::OpV(Bg.ToFColorSRGB(), A), MpS::OpV(Bd.ToFColorSRGB(), A));
		// os jogadores: 8 marquinhas (cheias até quantos estão), a escala apertada do flex (8,875 de largura)
		for (int32 K = 0; K < R.Max; ++K)
		{
			M.Rect(1143 + K * 11.875f, Top + 15, 8.875f, 14, MpS::OpV(K < R.Players ? AcV() : CssV(230, 220, 190, 0.16f), A));
		}
		// o ping: 4 barras de 5/8/11/15, cheias pela faixa, na cor da faixa
		const int32 N = R.Ping < 40 ? 4 : R.Ping < 80 ? 3 : R.Ping < 140 ? 2 : 1;
		const FColor Pc = R.Ping < 60 ? CssV(120, 200, 120) : R.Ping < 120 ? AcV() : CssV(240, 110, 90);
		const float Hs[4] = { 5, 8, 11, 15 };
		for (int32 K = 0; K < 4; ++K) M.Rect(1343 + K * 6.0f, Top + 31 - Hs[K], 4, Hs[K], MpS::OpV(K < N ? Pc : CssV(230, 220, 190, 0.18f), A));
		M.Draw(Out, L++, White());
	}
	// o mapa: o losango pequeno
	FSlateDrawElement::MakeBox(Out, L++, S.ToPaintGeometry(FVector2f(40, 40), FSlateLayoutTransform(FVector2f(933, Top + 12))),
		Image(FString::Printf(TEXT("../Skirmish/maps/%s_mpicon.png"), *Real), FVector2f(80, 80)), ESlateDrawEffect::None, FLinearColor(1, 1, 1, A));
	Text(Out, L, S, R.Name, Font(EFace::Shoulders800, 22, 2), 575, Top + 29.5f, MpS::Op(FLinearColor::White, A));
	const FSlateFontInfo Sf = Font(EFace::Barlow600, 12.5f, 2);
	const float Sw = Text(Out, L, S, FString::Printf(TEXT("HOST %s"), *R.Host), Sf, 575, Top + 50.5f, MpS::Op(Css(152, 161, 154), A));
	{
		// os selos: FULL, IN GAME, a versão diferente (a 8 px do texto, 2 px acima da linha)
		float Bx = 575 + Sw + 8;
		struct FBadge { FString T; FLinearColor C, B; };
		TArray<FBadge> Bs;
		if (Full(R)) Bs.Add({ TEXT("FULL"), Css(238, 241, 244, 0.6f), Css(238, 241, 244, 0.3f) });
		if (R.bGame) Bs.Add({ TEXT("IN GAME"), Ac(), Ac(0.5f) });
		if (Bad(R)) Bs.Add({ FString::Printf(TEXT("v%s"), *R.Ver), Css(240, 110, 90), Css(240, 110, 90, 0.6f) });
		const FSlateFontInfo Bf = Font(EFace::Barlow700, 11, 2);
		for (const FBadge& B : Bs)
		{
			const float W = TextWidth(B.T, Bf, Scale) + 12 + 2;
			const FVector4f Br(Bx, Top + 50.5f - 2 - 9.5f - 3, W, 17);
			Lines(Out, L, S, { FVector2f(Br.X + 0.5f, Br.Y + 0.5f), FVector2f(Br.X + Br.Z - 0.5f, Br.Y + 0.5f), FVector2f(Br.X + Br.Z - 0.5f, Br.Y + Br.W - 0.5f),
				FVector2f(Br.X + 0.5f, Br.Y + Br.W - 0.5f), FVector2f(Br.X + 0.5f, Br.Y + 0.5f) }, MpS::Op(B.B, A), 1.0f);
			Text(Out, L, S, B.T, Bf, Bx + 7, Top + 50.5f - 2, MpS::Op(B.C, A));
			Bx += W + 8;
		}
	}
	Text(Out, L, S, MapName, Font(EFace::Barlow700, 14, 2), 983, Top + 37.5f, MpS::Op(Css(238, 241, 244, 0.85f), A));
	Text(Out, L, S, FString::Printf(TEXT("%d / %d"), R.Players, R.Max), Font(EFace::Mono500, 12), 1143, Top + 46, MpS::Op(Css(230, 220, 190, 0.6f), A));
	Text(Out, L, S, R.Mode, Font(EFace::Shoulders800, 20, 2), 1264, Top + 40, MpS::Op(Css(238, 241, 244), A));
	Text(Out, L, S, FString::Printf(TEXT("%d MS"), R.Ping), Font(EFace::Mono500, 11), 1343, Top + 46, MpS::Op(Css(230, 220, 190, 0.55f), A));
	++L;
}

void SRTSMultiplayer::PaintDetail(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const TArray<int32> Vis = Visible();
	if (!Vis.Num() || !Rooms.IsValidIndex(Sel) || !Vis.Contains(Sel)) return;
	const FRoom& R = Rooms[Sel];
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const FMapInfo* Mi = Maps.IsValidIndex(R.Map) ? &Maps[R.Map] : nullptr;
	const FString Real = Mi ? Mi->Real : FString(TEXT("World_TestSalt2"));
	// a prévia do mapa (o losango em 86% com a moldura) e as bases: a ficha de quem está, o círculo tracejado das livres
	FSlateDrawElement::MakeBox(Out, L++, S.ToPaintGeometry(FVector2f(MpS::PV_S, MpS::PV_S), FSlateLayoutTransform(FVector2f(MpS::PV_X, MpS::PV_Y))),
		Image(FString::Printf(TEXT("../Skirmish/maps/%s_mppv.png"), *Real), FVector2f(760, 760)), ESlateDrawEffect::None, FLinearColor::White);
	if (Mi)
	{
		const float S2 = MpS::PV_S / UE_SQRT_2 * 0.86f, Sc = S2 / 1024.0f;
		const auto ToP = [&](FVector2f P)
		{
			const float U = P.X * Sc, V = P.Y * Sc;
			// o mesmo giro da sala e da partida (06/11)
			return FVector2f(MpS::PV_X + MpS::PV_S * 0.5f + (V - U) * UE_INV_SQRT_2, MpS::PV_Y + MpS::PV_S * 0.5f - (U + V) * UE_INV_SQRT_2);
		};
		FMesh M(S);
		for (int32 K = 0; K < FMath::Min(Mi->Starts.Num(), R.Max); ++K)
		{
			if (K < R.Players) Disc(M, ToP(Mi->Starts[K]), 13, HexV(MpS::PCOL[K % 8]), 40);
		}
		M.Draw(Out, L++, White());
		for (int32 K = 0; K < FMath::Min(Mi->Starts.Num(), R.Max); ++K)
		{
			const FVector2f C = ToP(Mi->Starts[K]);
			if (K < R.Players)
			{
				Lines(Out, L, S, Arc(C, 13, 0.0f, 2.0f * PI, 40), FLinearColor::White, 1.5f);
				FSlateDrawElement::MakeBox(Out, L + 1, S.ToPaintGeometry(FVector2f(18, 18), FSlateLayoutTransform(C - FVector2f(9, 9))),
					Image(TEXT("../Skirmish/glyph_usa.png"), FVector2f(64, 64)), ESlateDrawEffect::None, FLinearColor::White);
			}
			else
			{
				Dashed(Out, L, S, Arc(C, 11, 0.0f, 2.0f * PI, 40), Css(238, 241, 244, 0.7f), 1.4f, 3.0f);
			}
		}
		L += 2;
	}
	// o nome, a ficha (o mapa, o modo, o dinheiro, as superarmas; quebra como o CSS em 380 px) e quem está
	Text(Out, L, S, R.Name, Font(EFace::Shoulders900, 36, 2), 1440, 508, FLinearColor::White);
	{
		const FSlateFontInfo F = Font(EFace::Barlow600, 13, 3);
		const TArray<FString> Ln = MpS::Wrap(FString::Printf(TEXT("%s · %s · $%s · SUPERWEAPONS %s"), Mi ? *Mi->Name : TEXT(""), *R.Mode, *MpS::Thousands(R.Cash), *R.Sw), F, 380.0f, Scale);
		for (int32 I = 0; I < Ln.Num(); ++I) Text(Out, L, S, Ln[I], F, 1440, 532 + 16 * I, Css(152, 161, 154));
	}
	{
		FMesh M(S);
		for (int32 K = 0; K < R.Max; ++K)
		{
			const float Y = 567 + K * 48.0f;
			if (K < R.Players)
			{
				BoxRect(M, FVector4f(1440, Y, 380, 42), CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
				M.Rect(1440, Y, 3, 42, HexV(MpS::PCOL[K % 8]));   // o border-left de 3 px na cor do jogador
				// a ficha: o disco na cor, com o escuro de baixo (o inset 0 -2px 4px)
				const FVector2f C(1466, Y + 21);
				Disc(M, C, 13, HexV(MpS::PCOL[K % 8]), 32);
				for (int32 J = 0; J < 8; ++J)
				{
					// faixas da metade de baixo, do transparente no meio a 0,35 na borda (a placa interpola entre as faixas)
					const float Y0 = 13.0f * J / 8.0f, Y1 = 13.0f * (J + 1) / 8.0f;
					const float W0 = FMath::Sqrt(FMath::Max(0.0f, 169.0f - Y0 * Y0)), W1 = FMath::Sqrt(FMath::Max(0.0f, 169.0f - Y1 * Y1));
					const FColor A0 = CssV(0, 0, 0, 0.35f * FMath::Square(J / 8.0f)), A1 = CssV(0, 0, 0, 0.35f * FMath::Square((J + 1) / 8.0f));
					const int32 V0 = M.Vert(C + FVector2f(-W0, Y0), FVector2f(0, 0), A0), V1 = M.Vert(C + FVector2f(W0, Y0), FVector2f(0, 0), A0);
					const int32 V2 = M.Vert(C + FVector2f(W1, Y1), FVector2f(0, 0), A1), V3 = M.Vert(C + FVector2f(-W1, Y1), FVector2f(0, 0), A1);
					M.Tri(V0, V1, V2); M.Tri(V0, V2, V3);
				}
				if (K == 0) MpS::Crown(M, FVector2f(1489 + TextWidth(K == 0 ? R.Host : FString(TEXT("PLAYER")), Font(EFace::Shoulders800, 18, 2), Scale) + 10, Y + 15), AcV());
			}
			else
			{
				// a vaga livre: a borda tracejada (a da esquerda, de 3 px, apagada; desenhadas com o texto)
			}
		}
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < R.Max; ++K)
	{
		const float Y = 567 + K * 48.0f;
		if (K < R.Players)
		{
			FSlateDrawElement::MakeBox(Out, L, S.ToPaintGeometry(FVector2f(19, 19), FSlateLayoutTransform(FVector2f(1456.5f, Y + 11.5f))),
				Image(TEXT("../Skirmish/glyph_usa.png"), FVector2f(64, 64)), ESlateDrawEffect::None, FLinearColor::White);
			Text(Out, L, S, K == 0 ? R.Host : FString(TEXT("PLAYER")), Font(EFace::Shoulders800, 18, 2), 1489, Y + 28, FLinearColor::White);
			Text(Out, L, S, TEXT("USA"), Font(EFace::Barlow600, 12, 2), 1809, Y + 26, Css(152, 161, 154), 2);
		}
		else
		{
			const TArray<FVector2f> Box = { FVector2f(1443, Y + 0.5f), FVector2f(1819.5f, Y + 0.5f), FVector2f(1819.5f, Y + 41.5f), FVector2f(1443, Y + 41.5f) };
			Dashed(Out, L, S, Box, Css(230, 220, 190, 0.10f), 1.0f, 3.0f);
			Dashed(Out, L, S, { FVector2f(1441.5f, Y), FVector2f(1441.5f, Y + 42) }, Css(230, 220, 190, 0.15f), 3.0f, 6.0f);
			Text(Out, L, S, TEXT("OPEN SLOT"), Font(EFace::Shoulders800, 18, 2), 1453, Y + 28, Css(238, 241, 244, 0.4f));
		}
	}
	++L;
	// o JOIN: a faixa âmbar com o brilho (no hover o espaçamento abre); sem poder entrar, apagada com o motivo
	const bool bNo = Full(R) || Bad(R);
	const FString Label = Bad(R) ? FString::Printf(TEXT("NEEDS v%s"), *R.Ver) : Full(R) ? FString(TEXT("ROOM FULL")) : R.bGame ? FString(TEXT("IN GAME · WAIT")) : FString(TEXT("JOIN"));
	const float Hj = bNo ? 0.0f : HoverOf(EHit::Join);
	{
		FMesh M(S);
		if (bNo) M.Rect(MpS::JOIN.X, MpS::JOIN.Y, MpS::JOIN.Z, MpS::JOIN.W, CssV(230, 220, 190, 0.12f));
		else
		{
			GlowRect(M, MpS::JOIN, 0xd8ad4e, 0.25f + 0.2f * Hj, 40.0f + 20.0f * Hj);
			M.Rect(MpS::JOIN.X, MpS::JOIN.Y, MpS::JOIN.Z, MpS::JOIN.W, AcV());
		}
		M.Draw(Out, L++, White());
	}
	const float Ls = bNo ? 4.0f : 6.0f + 2.0f * Hj;
	Text(Out, L, S, Label, Font(EFace::Shoulders900, bNo ? 24.0f : 34.0f, Ls), MpS::JOIN.X + MpS::JOIN.Z * 0.5f, bNo ? 1003.5f : 1007.0f,
		bNo ? Css(238, 241, 244, 0.45f) : Css(10, 13, 11), 1);
	++L;
}
