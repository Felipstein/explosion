#include "SRTSMatch.h"
#include "RTSShellAudio.h"
#include "Rendering/DrawElements.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace RTSShellUI;

// as medidas do mockup (screens.html, medidas no Chrome em 06/11): o painel do Esc (650 de largura, o miolo de 560 a 44 da borda), o
// do aviso de rede (850 / 760), o resultado (a tabela de 1000 a partir de 110, 330; o gráfico em 1170, 330; os botões em 956), o
// carregando (a coluna de 760 em 110, 110; o mapa de 820 em 1010, 110)
namespace RTSMatchScreen
{
	constexpr float PAUSE_W = 650.0f, NET_W = 850.0f;
	const TCHAR* FacName(const FString& F)
	{
		return F == TEXT("usa") ? TEXT("UNITED STATES") : F == TEXT("russia") ? TEXT("RUSSIA") : F == TEXT("china") ? TEXT("CHINA") : F == TEXT("gla") ? TEXT("GLA") : TEXT("RANDOM ARMY");
	}
	// o lema de cada exército: o da USA é o do mockup aprovado; os outros três são provisórios no mesmo tom (docs/TELAS_NO_JOGO.md)
	const TCHAR* FacMotto(const FString& F)
	{
		return F == TEXT("usa") ? TEXT("PRECISION OVER NUMBERS") : F == TEXT("russia") ? TEXT("STEEL AND ARTILLERY") : F == TEXT("china") ? TEXT("MASS AND RESOLVE")
			: F == TEXT("gla") ? TEXT("STRIKE AND VANISH") : TEXT("THE ARMY IS DRAWN AT THE START");
	}
	uint32 ParseHex(const FString& S) { return S.StartsWith(TEXT("#")) ? (FParse::HexNumber(*S.Mid(1)) & 0xFFFFFFu) : 0xffffffu; }
	/** As bases de cada mapa (Resources/Skirmish/maps.json, em metros do mapa −512..512), lidas uma vez. */
	TMap<FString, FString>& Names()
	{
		static TMap<FString, FString> N;
		return N;
	}
	const TArray<FVector2f>* StartsOf(const FString& Real)
	{
		static TMap<FString, TArray<FVector2f>> Cache;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Text;
			TArray<TSharedPtr<FJsonValue>> Arr;
			if (FFileHelper::LoadFileToString(Text, *(PluginDir() / TEXT("Resources/Skirmish/maps.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : Arr)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					const TArray<TSharedPtr<FJsonValue>>* St = nullptr;
					if (!O.IsValid() || !O->TryGetArrayField(TEXT("starts"), St)) continue;
					TArray<FVector2f>& L = Cache.Add(O->GetStringField(TEXT("real")));
					Names().Add(O->GetStringField(TEXT("real")), O->GetStringField(TEXT("name")));
					for (const TSharedPtr<FJsonValue>& P : *St)
					{
						const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
						if (XY.Num() >= 2) L.Add(FVector2f(float(XY[0]->AsNumber()), float(XY[1]->AsNumber())));
					}
				}
			}
		}
		return Cache.Find(Real);
	}
	/** O nome do mapa na tela (SALT FLATS) pelo do jogo (World_TestSalt2); o do jogo se não achar. */
	FString NameOf(const FString& Real)
	{
		StartsOf(Real);
		const FString* N = Names().Find(Real);
		return N ? *N : Real;
	}
	/** A base do mapa no losango da tela (o giro da sala e da partida, 06/11: a frente da tela é +X+Y). */
	FVector2f ToDia(FVector2f M, FVector2f C, float Side)
	{
		const float K = Side / 1024.0f * UE_INV_SQRT_2;
		return FVector2f(C.X + (M.Y - M.X) * K, C.Y - (M.X + M.Y) * K);
	}
}
using namespace RTSMatchScreen;

double& SRTSMatch::LoadStart()
{
	static double T = 0.0;
	return T;
}

void SRTSMatch::Warm(const FRTSMatchIntro& In)
{
	for (const FRTSMatchIntro::FPlayer& P : In.Players)
	{
		if (P.Fac == TEXT("usa") || P.Fac == TEXT("russia") || P.Fac == TEXT("china") || P.Fac == TEXT("gla"))
		{
			Image(FString::Printf(TEXT("../Skirmish/glyph_%s.png"), *P.Fac), FVector2f(64, 64));
			if (P.bYou) Image(FString::Printf(TEXT("emblem_%s.png"), *P.Fac), FVector2f(400, 400));
		}
	}
	if (!In.MapReal.IsEmpty())
	{
		Image(FString::Printf(TEXT("../Skirmish/maps/%s_diamond.png"), *In.MapReal), FVector2f(1920, 1920));
		StartsOf(In.MapReal);
	}
	Image(TEXT("screens_glow.png"), FVector2f(480, 270));
	// cada face carregada aqui (a 1ª moldagem abre o arquivo da fonte): o filme só desenha, nunca abre fonte
	for (int32 F = 0; F < int32(EFace::Count); ++F) TextWidth(TEXT("AW09"), Font(EFace(F), 16));
}

void SRTSMatch::Construct(const FArguments& InArgs)
{
	bMovie = InArgs._Movie;
	Intro = FRTSShell::GetIntro();
	Now = FPlatformTime::Seconds();
	if (LoadStart() <= 0.0) LoadStart() = Now;
	SetMode(EMode::Loading);
}

void SRTSMatch::SetMode(EMode InMode)
{
	if (InMode != Mode || Btns.Num() == 0)
	{
		Mode = InMode;
		ModeT0 = FPlatformTime::Seconds();
		bConfirm = false;
		bViewMap = false;
		HoverBtn = HoverTab = -1;
	}
	Intro = FRTSShell::GetIntro();
	PollAt = 0.0;
	if (!bMovie) Poll();
	Build();
}

FString SRTSMatch::TimeText(float S) const
{
	const int32 T = FMath::Max(0, FMath::FloorToInt(S));
	return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60);
}

FString SRTSMatch::Matchup() const
{
	// "2 V 2", "1 V 1 V 1"; todos sozinhos com mais de 2 = FREE FOR ALL
	TMap<int32, int32> Teams;
	int32 Alone = 0;
	const int32 N = Players.Num() ? Players.Num() : Intro.Players.Num();
	TArray<int32> Sizes;
	for (int32 I = 0; I < N; ++I)
	{
		const int32 T = Players.Num() ? Players[I].Team : Intro.Players[I].Team;
		if (T <= 0) { Sizes.Add(1); ++Alone; }
		else Teams.FindOrAdd(T)++;
	}
	for (const TPair<int32, int32>& P : Teams) Sizes.Add(P.Value);
	if (Sizes.Num() > 2 && Alone == N) return TEXT("FREE FOR ALL");
	Sizes.Sort([](int32 A, int32 B) { return A > B; });
	FString Out;
	for (int32 K = 0; K < Sizes.Num(); ++K) Out += (K ? TEXT(" V ") : TEXT("")) + FString::FromInt(Sizes[K]);
	return Out;
}

void SRTSMatch::Poll()
{
	FString Json, Fake;
	if (bMovie) return;
	const bool bFake = FParse::Value(FCommandLine::Get(), TEXT("-RTSMatchFake="), Fake);
	if (bFake) { if (!FFileHelper::LoadFileToString(Json, *Fake)) return; }
	else if (!FRTSShell::Handlers().Api || !FRTSShell::Handlers().Api(Mode == EMode::Result ? TEXT("match.json?h=1") : TEXT("match.json"), Json)) return;
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return;
	const TSharedPtr<FJsonObject>* In = nullptr;
	if (bFake && O->TryGetObjectField(TEXT("intro"), In))
	{
		// a largada da foto (a sala de verdade preenche pelo FRTSShell::SetIntro)
		FRTSMatchIntro X;
		(*In)->TryGetStringField(TEXT("op"), X.Op);
		(*In)->TryGetStringField(TEXT("map"), X.Map);
		(*In)->TryGetStringField(TEXT("mapReal"), X.MapReal);
		const TArray<TSharedPtr<FJsonValue>>* Ps = nullptr;
		if ((*In)->TryGetArrayField(TEXT("players"), Ps))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ps)
			{
				const TSharedPtr<FJsonObject> P = V->AsObject();
				FRTSMatchIntro::FPlayer Y;
				FString HexS;
				double Dv = 0.0;
				P->TryGetStringField(TEXT("name"), Y.Name);
				P->TryGetStringField(TEXT("fac"), Y.Fac);
				P->TryGetStringField(TEXT("hex"), HexS);
				Y.Hex = ParseHex(HexS);
				if (P->TryGetNumberField(TEXT("team"), Dv)) Y.Team = int32(Dv);
				if (P->TryGetNumberField(TEXT("base"), Dv)) Y.Base = int32(Dv);
				P->TryGetBoolField(TEXT("ai"), Y.bAi);
				P->TryGetBoolField(TEXT("you"), Y.bYou);
				X.Players.Add(Y);
			}
		}
		Intro = X;
	}
	O->TryGetStringField(TEXT("op"), Op);
	O->TryGetStringField(TEXT("map"), MapName);
	O->TryGetStringField(TEXT("mapReal"), MapReal);
	O->TryGetStringField(TEXT("kind"), Kind);
	O->TryGetStringField(TEXT("result"), Result);
	double D = 0.0;
	if (O->TryGetNumberField(TEXT("time"), D)) MatchTime = float(D);
	if (O->TryGetNumberField(TEXT("local"), D)) LocalSlot = int32(D);
	O->TryGetBoolField(TEXT("ready"), bReady);
	O->TryGetBoolField(TEXT("canSurrender"), bCanSurrender);
	O->TryGetBoolField(TEXT("canRestart"), bCanRestart);
	O->TryGetBoolField(TEXT("canNetPause"), bCanNetPause);
	bLan = Kind == TEXT("lan");
	Players.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Ps = nullptr;
	if (O->TryGetArrayField(TEXT("players"), Ps))
	{
		for (const TSharedPtr<FJsonValue>& V : *Ps)
		{
			const TSharedPtr<FJsonObject> P = V->AsObject();
			if (!P.IsValid()) continue;
			FPlayer X;
			FString HexS;
			P->TryGetStringField(TEXT("name"), X.Name);
			P->TryGetStringField(TEXT("fac"), X.Fac);
			P->TryGetStringField(TEXT("hex"), HexS);
			X.Hex = ParseHex(HexS);
			if (P->TryGetNumberField(TEXT("team"), D)) X.Team = int32(D);
			if (P->TryGetNumberField(TEXT("diff"), D)) X.Diff = int32(D);
			P->TryGetBoolField(TEXT("ai"), X.bAi);
			P->TryGetBoolField(TEXT("you"), X.bYou);
			P->TryGetBoolField(TEXT("defeated"), X.bDefeated);
			P->TryGetBoolField(TEXT("loaded"), X.bLoaded);
			const TSharedPtr<FJsonObject>* St = nullptr;
			if (P->TryGetObjectField(TEXT("stats"), St))
			{
				const auto I = [&](const TCHAR* K) { double V2 = 0.0; (*St)->TryGetNumberField(K, V2); return V2; };
				X.Ub = int32(I(TEXT("ub"))); X.Bb = int32(I(TEXT("bb"))); X.Ud = int32(I(TEXT("ud"))); X.Bd = int32(I(TEXT("bd")));
				X.Ul = int32(I(TEXT("ul"))); X.Bl = int32(I(TEXT("bl"))); X.Money = int64(I(TEXT("money"))); X.Score = int64(I(TEXT("score")));
			}
			Players.Add(X);
		}
	}
	// o histórico do gráfico: os tempos e, por série, um vetor por jogador
	const TSharedPtr<FJsonObject>* H = nullptr;
	if (O->TryGetObjectField(TEXT("history"), H))
	{
		HistT.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Ts = nullptr;
		if ((*H)->TryGetArrayField(TEXT("t"), Ts)) for (const TSharedPtr<FJsonValue>& V : *Ts) HistT.Add(float(V->AsNumber()));
		const TCHAR* Keys[3] = { TEXT("army"), TEXT("econ"), TEXT("kills") };
		for (int32 S = 0; S < 3; ++S)
		{
			Hist[S].Reset();
			const TArray<TSharedPtr<FJsonValue>>* Series = nullptr;
			if (!(*H)->TryGetArrayField(Keys[S], Series)) continue;
			for (const TSharedPtr<FJsonValue>& Row : *Series)
			{
				TArray<float>& R = Hist[S].AddDefaulted_GetRef();
				for (const TSharedPtr<FJsonValue>& V : Row->AsArray()) R.Add(float(V->AsNumber()));
			}
		}
	}
	// a rede
	NetCase = 0;
	NetError.Reset();
	const TSharedPtr<FJsonObject>* N = nullptr;
	if (O->TryGetObjectField(TEXT("net"), N))
	{
		(*N)->TryGetStringField(TEXT("error"), NetError);
		const auto I = [&](const TCHAR* K) { double V2 = 0.0; (*N)->TryGetNumberField(K, V2); return V2; };
		NetCase = int32(I(TEXT("case")));
		NetSlot = int32(I(TEXT("slot")));
		NetLeft = float(I(TEXT("left")));
		NetSilent = float(I(TEXT("silent")));
		ResumeIn = float(I(TEXT("resumeIn")));
		Votes = int32(I(TEXT("votes")));
		VotesNeeded = int32(I(TEXT("needed")));
		PingMs = int32(I(TEXT("ping")));
		DesyncTick = int64(I(TEXT("desyncTick")));
		(*N)->TryGetBoolField(TEXT("voted"), bVoted);
		(*N)->TryGetBoolField(TEXT("host"), bNetHost);
	}
	if (Op.IsEmpty()) Op = Intro.Op;
	if (MapName.IsEmpty()) MapName = Intro.Map;
	if (MapReal.IsEmpty()) MapReal = Intro.MapReal;
	if (MapName.IsEmpty()) MapName = NameOf(MapReal);   // o SCRIPT: a largada só tem o mapa do jogo
}

void SRTSMatch::Build()
{
	// os botões de cada tela, nas medidas do mockup
	TArray<FBtn> B;
	const auto Add = [&](float X, float Y, float W, const TCHAR* Label, const TCHAR* Id, int32 Style, const TCHAR* Kbd = TEXT(""))
	{
		FBtn X0;
		X0.R = FVector4f(X, Y, W, 64);
		X0.Label = Label;
		X0.Id = Id;
		X0.Style = Style;
		X0.Kbd = Kbd;
		B.Add(X0);
	};
	if (Mode == EMode::Pause)
	{
		// o painel: 40 de respiro, o kicker (19), 6, o título (64), 8, a linha (17), 30, os botões de 64 com 10 entre eles, 44
		TArray<TTuple<FString, FString, int32, FString>> L;
		L.Add(MakeTuple(FString(TEXT("RESUME")), FString(TEXT("resume")), 1, FString(TEXT("ESC"))));
		if (bLan && bCanNetPause) L.Add(MakeTuple(FString(TEXT("PAUSE FOR EVERYONE")), FString(TEXT("net_pause")), 0, FString()));
		L.Add(MakeTuple(FString(TEXT("OPTIONS")), FString(TEXT("options")), 0, FString()));
		if (!bLan && bCanRestart) L.Add(MakeTuple(FString(TEXT("RESTART")), FString(TEXT("restart")), 0, FString()));
		if (bLan && bCanSurrender && !bConfirm) L.Add(MakeTuple(FString(TEXT("SURRENDER")), FString(TEXT("confirm")), 2, FString()));
		const int32 NumB = L.Num() + 1 + (bConfirm ? 1 : 0);
		const float H = 2 + 40 + 19 + 6 + 64 + 8 + 17 + 30 + NumB * 64 + (NumB - 1) * 10 + (bConfirm ? 26 + 10 : 0) + 44;
		const float Y0 = FMath::RoundToFloat(540.0f - H * 0.5f);
		float Y = Y0 + 1 + 40 + 19 + 6 + 64 + 8 + 17 + 30;
		for (const auto& T : L) { Add(680, Y, 560, *T.Get<0>(), *T.Get<1>(), T.Get<2>(), *T.Get<3>()); Y += 74; }
		if (bConfirm)
		{
			Y += 26 + 10;   // a pergunta (o .q: 15 px com 6 e 2 de margem) no lugar do SURRENDER, e o vão de 10
			Add(680, Y, 330, TEXT("YES, SURRENDER"), TEXT("yes"), 2);
			Add(1020, Y, 220, TEXT("NO"), TEXT("no"), 0);
			Y += 74;
		}
		Add(680, Y, 560, TEXT("QUIT TO MENU"), TEXT("menu"), 0);
	}
	else if (Mode == EMode::Result)
	{
		if (bViewMap) Add(752, 956, 416, TEXT("BACK TO RESULTS"), TEXT("results"), 1);
		else
		{
			TArray<TTuple<const TCHAR*, const TCHAR*, int32>> L;
			L.Add(MakeTuple(TEXT("VIEW MAP"), TEXT("view_map"), 0));
			if (bLan) L.Add(MakeTuple(TEXT("BACK TO LOBBY"), TEXT("lobby"), 1));
			else
			{
				L.Add(MakeTuple(TEXT("BACK TO LOBBY"), TEXT("back_to_room"), 0));
				if (bCanRestart) L.Add(MakeTuple(TEXT("REMATCH"), TEXT("restart"), 1));
			}
			L.Add(MakeTuple(TEXT("MAIN MENU"), TEXT("menu"), 0));
			const float W = (1700.0f - 12.0f * (L.Num() - 1)) / L.Num();
			for (int32 K = 0; K < L.Num(); ++K) Add(110 + K * (W + 12), 956, W, L[K].Get<0>(), L[K].Get<1>(), L[K].Get<2>());
		}
	}
	else if (Mode == EMode::Net)
	{
		// os botões do aviso: no fim do painel (a altura sai do caso)
		const float H = NetCase == 1 || NetCase == 4 ? 515.0f : NetCase == 2 ? 230.0f : 330.0f;
		const float Y0 = FMath::RoundToFloat(540.0f - H * 0.5f), Yb = Y0 + H - 1 - 40 - 64;
		if (NetCase == 1)
		{
			const FString Who = Players.IsValidIndex(NetSlot) ? Players[NetSlot].Name : FString(TEXT("THEM"));
			if (!bVoted) Add(580, Yb, 388, *FString::Printf(TEXT("DROP %s · %d / %d"), *Who, Votes, FMath::Max(1, VotesNeeded)), TEXT("net_vote"), 2);
			Add(bVoted ? 580 : 978, Yb, bVoted ? 760 : 362, TEXT("KEEP WAITING"), TEXT("keep"), 0);
		}
		else if (NetCase == 3) Add(580, Yb, 760, TEXT("RESUME"), TEXT("net_resume"), 1);
		else if (NetCase >= 4) Add(580, Yb, 760, TEXT("MAIN MENU"), TEXT("menu"), NetCase >= 5 ? 1 : 0);
	}
	Btns = MoveTemp(B);
	BtnHover.SetNumZeroed(Btns.Num());
	if (HoverBtn >= Btns.Num()) HoverBtn = -1;
}

void SRTSMatch::Press(const FString& Id)
{
	RTSShellAudio::Click();
	if (Id == TEXT("options")) { FRTSShell::Show(TEXT("screens.html?s=opt&from=esc")); return; }
	if (Id == TEXT("confirm")) { bConfirm = true; Build(); return; }
	if (Id == TEXT("no")) { bConfirm = false; Build(); return; }
	if (Id == TEXT("view_map")) { bViewMap = true; Build(); return; }
	if (Id == TEXT("results")) { bViewMap = false; Build(); return; }
	if (Id == TEXT("keep")) return;
	const auto& Fn = FRTSShell::Handlers().MatchButton;
	if (!Fn) return;
	if (Id == TEXT("yes"))
	{
		// o render-se do jogo pede dois cliques (o primeiro arma): a confirmação já foi aqui
		Fn(TEXT("surrender"));
		Fn(TEXT("surrender"));
		bConfirm = false;
		Build();
		return;
	}
	Fn(Id);
}

void SRTSMatch::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = FPlatformTime::Seconds();
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	if (!bMovie && Now >= PollAt)
	{
		PollAt = Now + 0.25;
		const int32 OldCase = NetCase;
		const bool bOldVoted = bVoted;
		Poll();
		if (OldCase != NetCase || bOldVoted != bVoted || Mode == EMode::Net) Build();
	}
	for (int32 K = 0; K < BtnHover.Num(); ++K) BtnHover[K] = FMath::Clamp(BtnHover[K] + (K == HoverBtn ? Dt : -Dt) / 0.15f, 0.0f, 1.0f);
}

FReply SRTSMatch::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2f P = FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	int32 H = -1;
	for (int32 K = 0; K < Btns.Num(); ++K) if (In(Btns[K].R, P)) H = K;
	if (H != HoverBtn && H >= 0) RTSShellAudio::Play(RTSShellAudio::EUi::Hover);
	HoverBtn = H;
	HoverTab = -1;
	if (Mode == EMode::Result && !bViewMap)
	{
		const FVector4f Tabs[3] = { FVector4f(1600.7f, 341, 53.6f, 24), FVector4f(1658.2f, 341, 78.9f, 24), FVector4f(1741.1f, 341, 55.9f, 24) };
		for (int32 K = 0; K < 3; ++K) if (In(Tabs[K], P)) HoverTab = K;
	}
	return FReply::Handled();
}

void SRTSMatch::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HoverBtn = HoverTab = -1;
}

FReply SRTSMatch::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Mode == EMode::Result && bViewMap)
	{
		// olhando o mapa: só o botão de voltar pega o clique; o resto vai pro jogo (a câmera)
		const FVector2f P = FVector2f(StageFit(MyGeometry).AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		if (!(Btns.Num() && In(Btns[0].R, P))) return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	if (HoverBtn >= 0 && Btns.IsValidIndex(HoverBtn)) { const FString Id = Btns[HoverBtn].Id; Press(Id); }
	else if (HoverTab >= 0) { RTSShellAudio::Click(); GraphTab = HoverTab; }
	return FReply::Handled();
}

FCursorReply SRTSMatch::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(HoverBtn >= 0 || HoverTab >= 0 ? EMouseCursor::Hand : EMouseCursor::Default);
}

// ---------------------------------------------------------------- a pintura

void SRTSMatch::PaintTok(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, float Size, const FString& Fac, uint32 Hex) const
{
	// o .tok: o disco na cor do jogador, a borda escura de 1 px e o brasão branco a 74%
	FMesh M(S);
	Disc(M, C, Size * 0.5f + 1.0f, CssV(0, 0, 0, 0.4f), 40);
	Disc(M, C, Size * 0.5f, HexV(Hex), 40);
	M.Draw(Out, L, White());
	if (Fac == TEXT("usa") || Fac == TEXT("russia") || Fac == TEXT("china") || Fac == TEXT("gla"))
	{
		const float G = Size * 0.74f;
		FSlateDrawElement::MakeBox(Out, L + 1, S.ToPaintGeometry(FVector2f(G, G), FSlateLayoutTransform(C - FVector2f(G, G) * 0.5f)),
			Image(FString::Printf(TEXT("../Skirmish/glyph_%s.png"), *Fac), FVector2f(64, 64)), ESlateDrawEffect::None, FLinearColor::White);
	}
	else Text(Out, L + 1, S, TEXT("?"), Font(EFace::Shoulders800, Size * 0.5f), C.X, C.Y + Size * 0.18f, FLinearColor::White, 1);
}

void SRTSMatch::PaintCells(FSlateWindowElementList& Out, int32 L, const FGeometry& S, float X, float Y, int32 N, int32 K, float W, float H, float Gap, FColor On) const
{
	// as células do .cells (inclinadas 12°, as cheias com o brilho de 8 px na cor)
	const float Sk = FMath::Tan(FMath::DegreesToRadians(12.0f)) * H * 0.5f;
	FMesh M(S);
	for (int32 I = 0; I < K; ++I)
	{
		const float Xi = X + I * (W + Gap);
		GlowRect(M, FVector4f(Xi, Y, W, H), (uint32(On.R) << 16) | (uint32(On.G) << 8) | On.B, 0.45f, 8.0f);
	}
	for (int32 I = 0; I < N; ++I)
	{
		const float Xi = X + I * (W + Gap);
		M.Poly({ FVector2f(Xi + Sk, Y), FVector2f(Xi + W + Sk, Y), FVector2f(Xi + W - Sk, Y + H), FVector2f(Xi - Sk, Y + H) }, I < K ? On : CssV(230, 220, 190, 0.1f));
	}
	M.Draw(Out, L, White());
}

void SRTSMatch::PaintPanel(FSlateWindowElementList& Out, int32& L, const FGeometry& S, const FVector4f& R, bool bBad, bool bCorners) const
{
	// o .panel: a sombra (0 30px 80px, 0,6), o fundo a 92%, a borda; os colchetes âmbar nos cantos (o Esc)
	FMesh M(S);
	GlowRect(M, FVector4f(R.X, R.Y + 30, R.Z, R.W), 0x000000, 0.6f, 80.0f);
	BoxRect(M, R, CssV(12, 16, 15, bCorners ? 0.92f : 0.94f), bBad ? CssV(240, 110, 90, 0.45f) : CssV(230, 220, 190, 0.16f));
	if (bCorners)
	{
		const FColor C = AcV(0.75f);
		M.Rect(R.X - 2, R.Y - 2, 16, 2, C); M.Rect(R.X - 2, R.Y - 2, 2, 16, C);
		M.Rect(R.X + R.Z - 14, R.Y + R.W, 16, 2, C); M.Rect(R.X + R.Z, R.Y + R.W - 14, 2, 16, C);
	}
	M.Draw(Out, L++, White());
}

void SRTSMatch::PaintBtns(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const bool bNetFont = Mode == EMode::Net;
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	{
		FMesh M(S);
		for (int32 K = 0; K < Btns.Num(); ++K)
		{
			const FBtn& B = Btns[K];
			const float H = BtnHover.IsValidIndex(K) ? BtnHover[K] : 0.0f;
			if (B.Style == 1)
			{
				GlowRect(M, B.R, 0xd8ad4e, 0.25f + 0.2f * H, 40.0f + 20.0f * H);
				BoxRect(M, B.R, AcV(), AcV());
			}
			else if (B.Style == 2) BoxRect(M, B.R, CssV(240, 110, 90, 0.1f * H), CssV(240, 110, 90, 0.55f));
			else BoxRect(M, B.R, CssV(16, 21, 19, 0.6f), CssV(230, 220, 190, 0.22f + 0.23f * H));
		}
		M.Draw(Out, L++, White());
	}
	for (int32 K = 0; K < Btns.Num(); ++K)
	{
		const FBtn& B = Btns[K];
		const float H = BtnHover.IsValidIndex(K) ? BtnHover[K] : 0.0f;
		const FLinearColor C = B.Style == 1 ? Css(10, 13, 11) : B.Style == 2 ? Css(240, 110, 90) : FMath::Lerp(Css(238, 241, 244, 0.9f), FLinearColor::White, H);
		const FSlateFontInfo F = bNetFont ? Font(EFace::Shoulders900, 24, 4) : Font(EFace::Shoulders900, 28, 5.0f + 2.0f * H);
		const FSlateFontInfo Kf = Font(EFace::Mono600, 12, 1);
		const float Tw = TextWidth(B.Label + TEXT(" "), F, Scale) - TextWidth(TEXT(" "), F, Scale);
		const float Kw = B.Kbd.IsEmpty() ? 0.0f : FMath::RoundToFloat(TextWidth(B.Kbd, Kf, Scale) + 14.0f);
		const float Total = Tw + (Kw > 0 ? 12.0f + Kw : 0.0f);
		const float X0 = B.R.X + (B.R.Z - Total) * 0.5f, Cy = B.R.Y + 32.0f;
		Text(Out, L, S, B.Label, F, X0, Cy + (bNetFont ? 8.6f : 10.0f), C);
		if (Kw > 0)
		{
			const float Kx = X0 + Tw + 12.0f;
			const FLinearColor Kc(C.R, C.G, C.B, C.A * 0.6f);
			Lines(Out, L, S, { FVector2f(Kx + 0.5f, Cy - 11.0f), FVector2f(Kx + Kw - 0.5f, Cy - 11.0f), FVector2f(Kx + Kw - 0.5f, Cy + 11.5f), FVector2f(Kx + 0.5f, Cy - 11.0f + 22.5f), FVector2f(Kx + 0.5f, Cy - 11.0f) }, Kc, 1.0f);
			Text(Out, L, S, B.Kbd, Kf, Kx + 7, Cy + 4.5f, Kc);
		}
	}
	++L;
}

void SRTSMatch::PaintLoading(FSlateWindowElementList& Out, int32& L, const FGeometry& Allotted, const FGeometry& S) const
{
	Backdrop(Out, L, Allotted, S, TEXT("screens_glow.png"));
	const FRTSMatchIntro& I = Intro;
	const FString OpS = I.Op.IsEmpty() ? (Op.IsEmpty() ? FString(TEXT("OPERATION")) : Op) : I.Op;
	const FString RealS = I.MapReal.IsEmpty() ? MapReal : I.MapReal;
	const FString MapS = !I.Map.IsEmpty() ? I.Map : !MapName.IsEmpty() ? MapName : NameOf(RealS);
	const FRTSMatchIntro::FPlayer* Me = I.Players.FindByPredicate([](const FRTSMatchIntro::FPlayer& P) { return P.bYou; });
	const FString MyFac = Me ? Me->Fac : FString(TEXT("random"));
	// a coluna: o kicker, a operação, a sua facção grande com o nome e o lema
	Text(Out, L, S, FString::Printf(TEXT("OPERATION · %s · %s"), *MapS.ToUpper(), *Matchup()), Font(EFace::Barlow600, 16, 8), 110, 125.1f, Ac());
	Text(Out, L, S, OpS, Font(EFace::Shoulders900, 88, 2), 110, 212.3f, FLinearColor::White);
	const bool bPeople = I.Players.Num() > 0;   // a cena do SCRIPT não tem a largada da sala: só a operação e o mapa
	if (!bPeople) {}
	else if (MyFac == TEXT("random"))
	{
		Dashed(Out, L, S, Arc(FVector2f(210, 348), 80, 0.0f, 2.0f * PI, 64), Css(230, 220, 190, 0.45f), 2.0f, 5.0f);
		Text(Out, L, S, TEXT("?"), Font(EFace::Shoulders800, 80), 210, 376, Css(230, 220, 190, 0.7f), 1);
	}
	else
	{
		FMesh M(S);
		GlowRect(M, FVector4f(140, 298, 140, 140), 0x000000, 0.45f, 40.0f);   // o drop-shadow (0 20px 40px)
		M.Draw(Out, L++, White());
		FSlateDrawElement::MakeBox(Out, L++, S.ToPaintGeometry(FVector2f(200, 200), FSlateLayoutTransform(FVector2f(110, 248))),
			Image(FString::Printf(TEXT("emblem_%s.png"), *MyFac), FVector2f(400, 400)), ESlateDrawEffect::None, FLinearColor::White);
	}
	if (bPeople)
	{
		Text(Out, L, S, FacName(MyFac), Font(EFace::Shoulders900, 64, 3), 336, 356.9f, FLinearColor::White);
		Text(Out, L, S, FacMotto(MyFac), Font(EFace::Barlow600, 16, 5), 336, 389.6f, Css(152, 161, 154));
	}
	// as linhas dos jogadores: as células enchendo (a IA já pronta, como no ZH); o seu carregando segue o tempo, e cheio quando o mapa abriu
	const float T = float(Now - LoadStart());
	const float Mine = bMovie ? FMath::Min(0.96f, 1.0f - FMath::Exp(-T / 7.0f)) : 1.0f;
	float Sum = 0.0f;
	int32 Count = 0;
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	for (int32 K = 0; K < I.Players.Num(); ++K)
	{
		const FRTSMatchIntro::FPlayer& P = I.Players[K];
		const float Top = 484.0f + K * 68.0f, Cy = Top + 30.0f;
		{
			FMesh M(S);
			BoxRect(M, FVector4f(110, Top, 760, 60), CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
			GlowRect(M, FVector4f(111, Top + 1, 6, 58), P.Hex, 0.8f, 12.0f);
			M.Rect(111, Top + 1, 6, 58, HexV(P.Hex));
			M.Draw(Out, L++, White());
		}
		PaintTok(Out, L, S, FVector2f(146, Cy), 34, P.Fac, P.Hex);
		L += 2;
		const float Nw = Text(Out, L, S, P.Name, Font(EFace::Shoulders800, 22, 2), 179, Cy + 7.7f, FLinearColor::White);
		const FString Sub = P.Team > 0 ? FString::Printf(TEXT("%s · TEAM %d"), *FString(P.Fac).ToUpper(), P.Team) : FString(P.Fac).ToUpper();
		Text(Out, L, S, Sub, Font(EFace::Barlow600, 12, 3), 179 + Nw + 10, Cy + 7.7f, Css(152, 161, 154));
		if (P.bAi)
		{
			Text(Out, L, S, TEXT("READY"), Font(EFace::Barlow700, 14, 3), 856, Cy + 4.9f, Css(120, 200, 120), 2);
			Sum += 1.0f;
		}
		else
		{
			const bool bLoaded = !bMovie && (P.bYou || (Players.IsValidIndex(K) && Players[K].bLoaded));
			const float Pr = P.bYou ? Mine : (bLoaded ? 1.0f : FMath::Min(0.9f, 1.0f - FMath::Exp(-T / 11.0f)));
			const FString Pct = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Pr * 100.0f));
			const FSlateFontInfo Pf = Font(EFace::Mono600, 13);
			const float Pw = TextWidth(Pct, Pf, Scale);
			Text(Out, L, S, Pct, Pf, 856 - Pw, Cy + 4.5f, Css(230, 220, 190, 0.6f));
			const float Right = 856 - Pw - 12;
			PaintCells(Out, L, S, Right - 357, Top + 19, 24, FMath::RoundToInt(Pr * 24), 12, 22, 3, HexV(P.Hex));
			++L;
			Sum += Pr;
		}
		++Count;
	}
	// o total embaixo: 48 células âmbar (sem jogadores na largada, o tempo do carregar)
	Text(Out, L, S, TEXT("ALL PLAYERS"), Font(EFace::Barlow600, 11.5f, 4), 110, 987.0f, Css(230, 220, 190, 0.45f));
	PaintCells(Out, L, S, 110, 1000, 48, FMath::RoundToInt((Count ? Sum / Count : Mine) * 48), 12.9f, 10, 3, AcV());
	++L;
	// o mapa: o losango da sala (a mesma imagem, no tamanho do mockup: 560 de lado) e a ficha de cada base escolhida, com o nome
	const FVector2f C(1420, 520);
	const float Side = 560.0f, Img = 960.0f * Side / (420.0f * UE_SQRT_2);
	if (!RealS.IsEmpty())
	{
		FSlateDrawElement::MakeBox(Out, L++, S.ToPaintGeometry(FVector2f(Img, Img), FSlateLayoutTransform(C - FVector2f(Img, Img) * 0.5f)),
			Image(FString::Printf(TEXT("../Skirmish/maps/%s_diamond.png"), *RealS), FVector2f(1920, 1920)), ESlateDrawEffect::None, FLinearColor::White);
		const TArray<FVector2f>* Starts = StartsOf(RealS);
		for (const FRTSMatchIntro::FPlayer& P : I.Players)
		{
			if (!Starts || !Starts->IsValidIndex(P.Base)) continue;
			const FVector2f D = ToDia((*Starts)[P.Base], C, Side);
			{
				FMesh M(S);
				Disc(M, D + FVector2f(2, 4), 27, CssV(0, 0, 0, 0.45f), 40);
				Disc(M, D, 26, CssV(255, 255, 255), 40);
				M.Draw(Out, L++, White());
			}
			PaintTok(Out, L, S, D, 48, P.Fac, P.Hex);
			L += 2;
			const FSlateFontInfo Lf = Font(EFace::Shoulders800, 16);
			const float Lw = TextWidth(P.Name, Lf, Scale) + 16.0f;
			{
				FMesh M(S);
				M.Rect(D.X - Lw * 0.5f, D.Y + 31, Lw, 22, CssV(8, 10, 9, 0.88f));
				M.Rect(D.X - Lw * 0.5f, D.Y + 31, 3, 22, HexV(P.Hex));
				M.Draw(Out, L++, White());
			}
			Text(Out, L, S, P.Name, Lf, D.X + 1, D.Y + 47.5f, FLinearColor::White, 1);
			++L;
		}
	}
}

void SRTSMatch::PaintPause(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	if (!Btns.Num()) return;
	const float Bottom = Btns.Last().R.Y + 64 + 44 + 1;
	const float Y0 = Btns[0].R.Y - (1 + 40 + 19 + 6 + 64 + 8 + 17 + 30);
	PaintPanel(Out, L, S, FVector4f(635, Y0, PAUSE_W, Bottom - Y0), false, true);
	Text(Out, L, S, bLan ? TEXT("MULTIPLAYER · THE MATCH GOES ON") : TEXT("SKIRMISH · PAUSED"), Font(EFace::Barlow600, 16, 8), 680, Y0 + 41 + 15.1f, Ac());
	Text(Out, L, S, Op.IsEmpty() ? FString(TEXT("PAUSED")) : Op, Font(EFace::Shoulders900, 64, 3), 680, Y0 + 66 + 54.4f, FLinearColor::White);
	// a linha: o tempo, o mapa e com quem
	int32 Ai = 0;
	TArray<FString> With;
	for (const FPlayer& P : Players)
	{
		if (P.bAi) ++Ai;
		else if (!P.bYou) With.Add(P.Name);
	}
	FString Sub = FString::Printf(TEXT("%s · %s"), *TimeText(MatchTime), *MapName.ToUpper());
	if (With.Num()) Sub += TEXT(" · WITH ") + FString::Join(With, TEXT(", "));
	if (Ai) Sub += FString::Printf(TEXT(" · VS %d AI"), Ai);
	Text(Out, L, S, Sub, Font(EFace::Barlow600, 14, 3), 680, Y0 + 138 + 13.4f, Css(152, 161, 154));
	if (bConfirm)
	{
		// a pergunta no lugar do SURRENDER (o .q: 15 px, 3 de espaçamento, vermelho)
		const int32 Yes = Btns.IndexOfByPredicate([](const FBtn& B) { return B.Id == TEXT("yes"); });
		if (Yes >= 0) Text(Out, L, S, TEXT("SURRENDER THE MATCH?"), Font(EFace::Barlow700, 15, 3), 960, Btns[Yes].R.Y - 10 - 2 - 4.5f, Css(240, 110, 90), 1);
	}
}

void SRTSMatch::PaintResult(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const bool bWin = Result == TEXT("victory"), bDef = Result == TEXT("defeat");
	const FLinearColor Big = bDef ? Css(240, 110, 90) : Ac();
	// o cabeçalho: a operação e o mapa, VICTORY/DEFEAT com o brilho, a linha do tempo e do placar
	Text(Out, L, S, FString::Printf(TEXT("OPERATION %s · %s"), *Op, *MapName.ToUpper()), Font(EFace::Barlow600, 16, 8), 110, 106.1f, Ac());   // (o vão de 4 px do mockup até o título)
	const FSlateFontInfo Vf = Font(EFace::Shoulders900, 150, 6);
	const FString Title = bWin ? TEXT("VICTORY") : bDef ? TEXT("DEFEAT") : TEXT("DRAW");
	{
		FMesh M(S);
		const float Vw = TextWidth(Title, Vf, Scale);
		GlowRect(M, FVector4f(110, 130, Vw, 95), bDef ? 0xf06e5a : 0xd8ad4e, 0.12f, 60.0f);   // o text-shadow (0 0 60px, ~0,35) espalhado
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, Title, Vf, 110, 230.3f, Big);   // a base medida no Chrome (o título de 109 a 230)
	bool bAllies = false;
	const FPlayer* MeP = Players.FindByPredicate([](const FPlayer& P) { return P.bYou; });
	for (const FPlayer& P : Players) if (MeP && !P.bYou && P.Team > 0 && P.Team == MeP->Team) bAllies = true;
	const FString How = bWin ? (bAllies ? TEXT("YOUR TEAM WON") : TEXT("YOU WON")) : bDef ? (bAllies ? TEXT("YOUR TEAM WAS DEFEATED") : TEXT("YOU WERE DEFEATED")) : TEXT("NO WINNER");
	{
		const FSlateFontInfo Mf = Font(EFace::Barlow600, 16, 4), Mb = Font(EFace::Barlow700, 16, 4);
		const float W0 = Text(Out, L, S, TimeText(MatchTime), Mb, 110, 265.1f, FLinearColor::White);
		Text(Out, L, S, FString::Printf(TEXT(" · %s · %s"), *Matchup(), *How), Mf, 110 + W0, 265.1f, Css(152, 161, 154));
	}
	// a tabela: o cabeçalho em duas linhas e uma linha por jogador (a borda esquerda na cor, os melhores em âmbar, os derrotados apagados)
	const float ColR[6] = { 0, 618.3f, 787.0f, 932.4f, 1006.8f, 1096.0f };
	const FSlateFontInfo Th = Font(EFace::Barlow600, 11.5f, 4);
	const FLinearColor ThC = Css(230, 220, 190, 0.45f);
	Text(Out, L, S, TEXT("PLAYER"), Th, 124, 356.0f, ThC);
	const TCHAR* H1[5] = { TEXT("UNITS BUILT ·"), TEXT("BUILDINGS ·"), TEXT("MONEY"), TEXT("KILLS"), TEXT("SCORE") };
	const TCHAR* H2[5] = { TEXT("LOST"), TEXT("LOST"), TEXT("EARNED"), TEXT(""), TEXT("") };
	for (int32 K = 0; K < 5; ++K)
	{
		const bool bTwo = H2[K][0] != 0;
		Text(Out, L, S, H1[K], Th, ColR[K + 1] + 4, bTwo ? 348.0f : 356.0f, ThC, 2);
		if (bTwo) Text(Out, L, S, H2[K], Th, ColR[K + 1] + 4, 364.0f, ThC, 2);
	}
	int64 BestU = 0, BestM = 0, BestK = 0, BestS = 0;
	for (const FPlayer& P : Players) { BestU = FMath::Max<int64>(BestU, P.Ub); BestM = FMath::Max(BestM, P.Money); BestK = FMath::Max<int64>(BestK, P.Ud + P.Bd); BestS = FMath::Max(BestS, P.Score); }
	for (int32 R = 0; R < Players.Num(); ++R)
	{
		const FPlayer& P = Players[R];
		const float Top = 374.0f + R * 64.0f, Cy = Top + 29.0f;
		const float A = P.bDefeated ? 0.55f : 1.0f;
		{
			FMesh M(S);
			M.Rect(110, Top, 1000, 58, CssV(16, 21, 19, 0.62f * A));
			M.Rect(110, Top, 1000, 1, CssV(230, 220, 190, 0.10f * A));
			M.Rect(110, Top + 57, 1000, 1, CssV(230, 220, 190, 0.10f * A));
			M.Rect(110, Top, 4, 58, HexV(P.Hex, A));
			M.Draw(Out, L++, White());
		}
		PaintTok(Out, L, S, FVector2f(145, Cy), 34, P.Fac, P.Hex);
		L += 2;
		const float Nw = Text(Out, L, S, P.Name, Font(EFace::Shoulders800, 22, 2), 174, Cy + 7.7f, FLinearColor(1, 1, 1, A));
		FString Sub = P.Team > 0 ? FString::Printf(TEXT("TEAM %d"), P.Team) : FString();
		if (P.bDefeated) Sub += Sub.IsEmpty() ? TEXT("DEFEATED") : TEXT(" · DEFEATED");
		Text(Out, L, S, Sub, Font(EFace::Barlow600, 12, 3), 174 + Nw + 12, Cy + 7.7f, Css(152, 161, 154, A));
		const FSlateFontInfo Nf = Font(EFace::Mono600, 17);
		const FLinearColor Nc = Css(238, 241, 244, 0.85f * A), Bc = Css(216, 173, 78, A);
		const auto Num = [&](int32 Col, const FString& Tx, bool bBest) { Text(Out, L, S, Tx, Nf, ColR[Col], Cy + 6.0f, bBest ? Bc : Nc, 2); };
		const int32 Kills = P.Ud + P.Bd;
		Num(1, FString::Printf(TEXT("%d · %d"), P.Ub, P.Ul), Players.Num() > 1 && P.Ub == BestU && BestU > 0);
		Num(2, FString::Printf(TEXT("%d · %d"), P.Bb, P.Bl), false);
		Num(3, FString::Printf(TEXT("$%s"), *FText::AsNumber(P.Money).ToString()), Players.Num() > 1 && P.Money == BestM && BestM > 0);
		Num(4, FString::FromInt(Kills), Players.Num() > 1 && Kills == BestK && BestK > 0);
		Num(5, FText::AsNumber(P.Score).ToString(), Players.Num() > 1 && P.Score == BestS && BestS > 0);
	}
	// o gráfico: o cartão, o nome da série, as abas e as linhas de cada jogador no tempo
	const FVector4f G(1170, 330, 642, 402);
	const FVector4f Tabs[3] = { FVector4f(1600.7f, 341, 53.6f, 24), FVector4f(1658.2f, 341, 78.9f, 24), FVector4f(1741.1f, 341, 55.9f, 24) };
	const TCHAR* TabN[3] = { TEXT("ARMY"), TEXT("ECONOMY"), TEXT("KILLS") };
	const TCHAR* Lab[3] = { TEXT("ARMY VALUE"), TEXT("MONEY EARNED"), TEXT("KILLS") };
	{
		FMesh M(S);
		BoxRect(M, G, CssV(16, 21, 19, 0.62f), CssV(230, 220, 190, 0.10f));
		for (int32 K = 0; K < 3; ++K) BoxRect(M, Tabs[K], FColor(0, 0, 0, 0), K == GraphTab ? AcV(0.65f) : (K == HoverTab ? CssV(230, 220, 190, 0.3f) : CssV(230, 220, 190, 0.12f)));
		// a grade (5 linhas) da área do canvas (1170, 374, 640 × 356, 34 de margem)
		for (int32 K = 0; K <= 4; ++K) M.Rect(1170 + 34, 374 + 34 + (356 - 68) * K / 4.0f, 640 - 34 - 16, 1, CssV(230, 220, 190, 0.06f));
		M.Draw(Out, L++, White());
	}
	Text(Out, L, S, Lab[GraphTab], Font(EFace::Barlow600, 11.5f, 4), 1187, 356.0f, ThC);
	for (int32 K = 0; K < 3; ++K)
	{
		Text(Out, L, S, TabN[K], Font(EFace::Barlow700, 12, 2), Tabs[K].X + Tabs[K].Z * 0.5f + 1, Tabs[K].Y + 16.5f,
			K == GraphTab ? Ac() : (K == HoverTab ? FLinearColor::White : Css(238, 241, 244, 0.55f)), 1);
	}
	const float GX = 1170 + 34, GW = 640 - 34 - 30, GY0 = 374 + 34, GH = 356 - 68;
	const float TEnd = HistT.Num() ? FMath::Max(60.0f, HistT.Last()) : FMath::Max(60.0f, MatchTime);
	// as marcas do tempo: em minutos (o mockup); abaixo de 4 min em m:ss (em minutos redondos elas se repetiam: "0 0 1 1 1 MIN"); sem
	// gráfico, nenhuma
	for (int32 K = 0; K <= 4 && HistT.Num() >= 4; ++K)
	{
		const float T = TEnd * K / 4.0f;
		const int32 Min = FMath::RoundToInt(T / 60.0f);
		const FString Lb = TEnd < 240.0f ? TimeText(T) : K == 4 ? FString::Printf(TEXT("%d MIN"), Min) : FString::FromInt(Min);
		Text(Out, L, S, Lb, Font(EFace::Mono500, 11), GX + GW * K / 4.0f - 4, 374 + 356 - 10, ThC);
	}
	const TArray<TArray<float>>& Ser = Hist[GraphTab];
	float Max = 1.0f;
	for (const TArray<float>& R : Ser) for (const float V : R) Max = FMath::Max(Max, V);
	if (HistT.Num() < 4)   // menos de ~20 s de partida: as amostras não fazem um gráfico
	{
		Text(Out, L, S, TEXT("THE MATCH WAS TOO SHORT FOR A GRAPH"), Font(EFace::Barlow600, 13, 3), 1170 + 321, 374 + 178, ThC, 1);
	}
	else
	{
		for (int32 P = 0; P < Ser.Num() && P < Players.Num(); ++P)
		{
			TArray<FVector2f> Pts;
			for (int32 K = 0; K < Ser[P].Num() && K < HistT.Num(); ++K)
			{
				Pts.Add(FVector2f(GX + GW * HistT[K] / TEnd, GY0 + GH - GH * 0.95f * Ser[P][K] / Max));
			}
			if (Pts.Num() >= 2) Lines(Out, L, S, Pts, Hex(Players[P].Hex), 2.2f);
		}
	}
	++L;
}

void SRTSMatch::PaintNet(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const
{
	const float H = NetCase == 1 || NetCase == 4 ? 515.0f : NetCase == 2 ? 230.0f : 330.0f;
	const float Y0 = FMath::RoundToFloat(540.0f - H * 0.5f);
	const bool bBad = NetCase >= 5;
	PaintPanel(Out, L, S, FVector4f(535, Y0, NET_W, H), bBad, false);
	const FString Who = Players.IsValidIndex(NetSlot) ? Players[NetSlot].Name : FString(TEXT("A PLAYER"));
	FString Kick = TEXT("MATCH PAUSED FOR EVERYONE"), Head;
	switch (NetCase)
	{
	case 1: Head = FString::Printf(TEXT("WAITING FOR %s"), *Who); break;
	case 2: Head = FString::Printf(TEXT("RESUMING IN %d"), FMath::Max(1, FMath::CeilToInt(ResumeIn))); break;
	case 3: Head = NetSlot == LocalSlot ? FString(TEXT("PAUSED BY YOU")) : FString::Printf(TEXT("PAUSED BY %s"), *Who); break;
	case 4: Head = TEXT("WAITING FOR THE HOST"); break;
	case 5: Kick = TEXT("CONNECTION LOST"); Head = bNetHost ? TEXT("A PLAYER IS GONE") : TEXT("THE HOST IS GONE"); break;
	case 7: Kick = TEXT("MATCH STOPPED"); Head = TEXT("COULDN'T START"); break;
	default: Kick = TEXT("MATCH STOPPED"); Head = TEXT("OUT OF SYNC"); break;
	}
	Text(Out, L, S, Kick, Font(EFace::Barlow600, 16, 8), 580, Y0 + 37 + 15.1f, bBad ? Css(240, 110, 90) : Ac());
	Text(Out, L, S, Head, Font(EFace::Shoulders900, 52, 3), 580, Y0 + 62 + 44.2f, FLinearColor::White);
	// as linhas de informação (15 px, 1,5 de entrelinha; os números em mono branco), centradas
	const float Scale = S.GetAccumulatedLayoutTransform().GetScale();
	const auto Rich = [&](const TArray<TPair<FString, bool>>& Parts, float Base)
	{
		const FSlateFontInfo Fm = Font(EFace::Barlow600, 15, 2), Fb = Font(EFace::Mono600, 15);
		float W = 0.0f;
		for (const auto& P : Parts) W += TextWidth(P.Key, P.Value ? Fb : Fm, Scale);
		float X = 960.0f - W * 0.5f;
		for (const auto& P : Parts) X += Text(Out, L, S, P.Key, P.Value ? Fb : Fm, X, Base, P.Value ? FLinearColor::White : Css(152, 161, 154));
	};
	if (NetCase == 1 || NetCase == 4)
	{
		// o anel da contagem (r 52, 6 de traço, de cima no sentido do relógio) e o tempo no meio
		const float Left = NetCase == 1 ? NetLeft : FMath::Max(0.0f, 60.0f - NetSilent);
		const FVector2f C(960, Y0 + 138 + 60);
		Lines(Out, L, S, Arc(C, 52, 0.0f, 2.0f * PI, 72), Css(230, 220, 190, 0.12f), 6.0f);
		const float F = FMath::Clamp(Left / 60.0f, 0.0f, 1.0f);
		if (F > 0.0f) Lines(Out, L + 1, S, Arc(C, 52, -0.5f * PI, -0.5f * PI + 2.0f * PI * F, FMath::Max(4, FMath::RoundToInt(72 * F))), Ac(), 6.0f);
		L += 2;
		Text(Out, L, S, TimeText(Left), Font(EFace::Mono600, 30), C.X, C.Y + 10.5f, FLinearColor::White, 1);
		// quem: a ficha e o nome
		const FPlayer* P = Players.IsValidIndex(NetSlot) ? &Players[NetSlot] : nullptr;
		const FString Name = NetCase == 4 ? (Players.Num() ? Players[0].Name : FString(TEXT("HOST"))) : Who;
		const FPlayer* Pw = NetCase == 4 ? (Players.Num() ? &Players[0] : nullptr) : P;
		const FSlateFontInfo Wf = Font(EFace::Shoulders800, 26, 2);
		const float Nw = TextWidth(Name, Wf, Scale), Wx = 960.0f - (30 + 12 + Nw) * 0.5f;
		if (Pw) { PaintTok(Out, L, S, FVector2f(Wx + 15, Y0 + 272 + 16), 30, Pw->Fac, Pw->Hex); L += 2; }
		Text(Out, L, S, Name, Wf, Wx + 42, Y0 + 272 + 16 + 9.1f, FLinearColor::White);
		const float Silent = NetCase == 4 ? NetSilent : FMath::Max(0.0f, 60.0f - NetLeft);
		TArray<TPair<FString, bool>> L1 = { { TEXT("NO SIGNAL FOR "), false }, { FString::Printf(TEXT("%d S"), FMath::FloorToInt(Silent)), true } };
		if (PingMs > 0) { L1.Add({ TEXT(" · LAST PING "), false }); L1.Add({ FString::Printf(TEXT("%d MS"), PingMs), true }); }
		Rich(L1, Y0 + 314 + 16.5f);
		Rich({ { NetCase == 1 ? TEXT("THE MATCH GOES ON IF THEY COME BACK; AT 0:00 THEY ARE DROPPED") : TEXT("THE MATCH WAITS FOR THE HOST UP TO 1:00"), false } }, Y0 + 314 + 39.0f);
		if (NetCase == 1)
		{
			// os votos: uma barrinha por voto preciso, as dadas em vermelho
			FMesh M(S);
			const int32 N = FMath::Max(1, VotesNeeded);
			const float Vx = 960.0f - (N * 28 + (N - 1) * 8) * 0.5f;
			for (int32 K = 0; K < N; ++K) M.Rect(Vx + K * 36, Y0 + 378, 28, 6, K < Votes ? CssV(240, 110, 90) : CssV(230, 220, 190, 0.15f));
			M.Draw(Out, L++, White());
		}
	}
	else if (NetCase == 3) Rich({ { TEXT("THE GAME IS STOPPED FOR EVERYONE"), false } }, Y0 + 130 + 16.5f);
	else if (NetCase == 5) Rich({ { TEXT("STOPPED ANSWERING AT "), false }, { TimeText(MatchTime), true } }, Y0 + 130 + 16.5f);
	else if (NetCase == 6)
	{
		Rich({ { TEXT("THE GAMES DISAGREED AT STEP "), false }, { FText::AsNumber(DesyncTick).ToString(), true } }, Y0 + 130 + 16.5f);
		Rich({ { TEXT("THE STEP IS IN THE LOG FOR THE DEVELOPERS"), false } }, Y0 + 130 + 39.0f);
	}
	else if (NetCase == 7)
	{
		// (06/11 à noite) a partida não montou (o mapa sem posições, um cenário com unidade que não existe): o motivo, cortado na
		// largura dos botões; antes era o texto velho do HUD, escondido embaixo do carregando
		Rich({ { TEXT("THE MATCH COULD NOT BE SET UP"), false } }, Y0 + 130 + 16.5f);
		FString Why = FText::FromString(NetError).ToUpper().ToString();   // (o FString::ToUpper não sobe os acentos: "POSIçõES")
		Why.RemoveFromStart(TEXT("COULDN'T SET UP THE MATCH: "));
		const FSlateFontInfo Fw = Font(EFace::Barlow600, 15, 2);
		bool bCut = false;
		while (Why.Len() > 4 && TextWidth(bCut ? Why + TEXT("…") : Why, Fw, Scale) > 760.0f)
		{
			Why.LeftChopInline(1);
			bCut = true;
		}
		Rich({ { bCut ? Why.TrimEnd() + TEXT("…") : Why, false } }, Y0 + 130 + 39.0f);
	}
}

int32 SRTSMatch::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FGeometry S = StageFit(AllottedGeometry);
	int32 L = LayerId;
	if (Mode == EMode::Loading)
	{
		PaintLoading(OutDrawElements, L, AllottedGeometry, S);
	}
	else
	{
		// o .dim do mockup sobre o jogo (o borrão é o SBackgroundBlur de quem hospeda); olhando o mapa, nada por cima
		if (!(Mode == EMode::Result && bViewMap))
		{
			FMesh M(AllottedGeometry);
			const FVector2f Sz = FVector2f(AllottedGeometry.GetLocalSize());
			M.Rect(0, 0, Sz.X, Sz.Y, CssV(6, 8, 7, Mode == EMode::Result ? 0.8f : 0.72f));
			M.Draw(OutDrawElements, L++, White());
		}
		if (Mode == EMode::Pause) PaintPause(OutDrawElements, L, S);
		else if (Mode == EMode::Result && !bViewMap) PaintResult(OutDrawElements, L, S);
		else if (Mode == EMode::Net) PaintNet(OutDrawElements, L, S);
		PaintBtns(OutDrawElements, L, S);
	}
	if (!(Mode == EMode::Result && bViewMap))
	{
		FMesh M(S);
		StageCorners(M);
		M.Draw(OutDrawElements, L++, White());
	}
	return L;
}
