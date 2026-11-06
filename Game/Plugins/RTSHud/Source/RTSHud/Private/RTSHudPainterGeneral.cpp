#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "GlobalRenderResources.h"

// A tela do general (General's Command), aprovada em 02/11 (a moldura de cada facção por fora, a janela e o conteúdo iguais nas quatro) e
// 03/11 (a árvore calma, sem as linhas por tipo, na ordem aprovada; a linha do tempo: a coluna da patente atual acesa, as que faltam
// apagadas). O protótipo é docs/concept/hud/prototipo/index.html (#gen); as medidas aqui são as dele em px da planta (a janela tem o zoom
// 1,25 de lá: 1575 × 850 em 172,5 / 75). A árvore vem de Game/Config/RTS/GeneralTrees.json (docs/poderes/tela/export_game.py).

namespace
{
	FLinearColor Hex(uint32 RGB, float A = 1.0f)
	{
		FLinearColor C(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255));
		C.A = A;
		return C;
	}

	FLinearColor Mul(FLinearColor C, float K, float A = 1.0f)
	{
		C.R *= K; C.G *= K; C.B *= K; C.A *= A;
		return C;
	}

	/** As cores das peças da tela em cada facção (as do protótipo: o cartão, a do cartão apagado, o cabeçalho da patente, o detalhe, a comenda). */
	struct FGenColors
	{
		uint32 TileTop, TileBot, TileRing, PoorTop, PoorBot, PoorRing, HeadBg, HeadRing, DetTop, DetBot, DetRing;
	};

	FGenColors GenColors(FName F)
	{
		if (F == TEXT("china")) return { 0x141010, 0x0a0808, 0x4a1d18, 0x141010, 0x0a0808, 0x4a1d18, 0x0f0c0c, 0x3e1814, 0x151111, 0x0a0808, 0x4a1d18 };
		if (F == TEXT("gla")) return { 0x22261a, 0x14170f, 0x4f5236, 0x22261a, 0x14170f, 0x4f5236, 0x171a11, 0x3e4029, 0x20231a, 0x12140e, 0x4f5236 };
		if (F == TEXT("russia")) return { 0x1c2126, 0x101317, 0x3c4652, 0x1c2126, 0x101317, 0x3c4652, 0x12161a, 0x2e3640, 0x1a1f24, 0x0f1215, 0x3c4652 };
		return { 0x17222e, 0x0d141b, 0x33414f, 0x121a23, 0x0b1118, 0x2a3540, 0x0d141b, 0x26313c, 0x141e29, 0x0b1118, 0x33414f };
	}

	// a borda de cada moldura (cima, baixo, esquerda, direita), em px da arte = px da planta (art/tools/make_gen_frame.py)
	FMargin FrameBorder(FName F)
	{
		if (F == TEXT("china")) return FMargin(46.0f, 32.0f, 46.0f, 32.0f);
		if (F == TEXT("gla")) return FMargin(39.0f, 35.0f, 38.0f, 32.0f);
		if (F == TEXT("russia")) return FMargin(28.0f, 32.0f, 28.0f, 30.0f);
		return FMargin(22.0f, 23.0f, 20.0f, 18.0f);
	}

	enum class EGenState : uint8 { Own, Buy, Poor, Req, Lock, Soon };   // (05/11) Soon: o jogo ainda não tem o poder
}

TArray<FString> FRTSHudPainter::WrapLines(const FString& Str, float Px, EWeight W, float MaxW, int32 MaxLines) const
{
	TArray<FString> Words;
	Str.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Line;
	for (const FString& Wd : Words)
	{
		const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
		if (!Line.IsEmpty() && Measure(Try, Px, W).X > MaxW)
		{
			Lines.Add(Line);
			Line = Wd;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	if (MaxLines > 0 && Lines.Num() > MaxLines)
	{
		FString Rest = Lines[MaxLines - 1];
		for (int32 I = MaxLines; I < Lines.Num(); ++I) Rest += TEXT(" ") + Lines[I];
		Lines.SetNum(MaxLines);
		Lines.Last() = Ellipsize(Rest, Px, W, MaxW);
	}
	return Lines;
}

void FRTSHudPainter::CheckMark(FVector2D C, float Size, float T, const FLinearColor& Col)
{
	// o ✓ do protótipo (o caminho M1.5 5.8 l3.4 3.2 L11.5 1.8 num quadro de 13 × 11)
	const float K = Size / 13.0f;
	const FVector2D O = C - FVector2D(6.5f, 5.5f) * K;
	const FVector2D A = O + FVector2D(1.5f, 5.8f) * K, B = O + FVector2D(4.9f, 9.0f) * K, D = O + FVector2D(11.5f, 1.8f) * K;
	Line(A, B, T, Col);
	Line(B, D, T, Col);
	FillCircle(B, T * 0.5f, Col, Col, 10);
}

void FRTSHudPainter::GenToken(int32 Kind, float X, float Y, float W, float H, const FString& Num, float Px)
{
	// as fichas de ponto (01/11, as 3 da arte aprovada): 0 a dourada, 1 a escura (falta ponto), 2 a verde do comprado (o hexágono com o ✓)
	if (Kind == 2)
	{
		auto Hexa = [](float HX, float HY, float HW, float HH) {
			return TArray<FVector2D>{ { HX + HW * 0.25f, HY }, { HX + HW * 0.75f, HY }, { HX + HW, HY + HH * 0.5f }, { HX + HW * 0.75f, HY + HH }, { HX + HW * 0.25f, HY + HH }, { HX, HY + HH * 0.5f } };
		};
		FillPoly(Hexa(X, Y, W, H), [=](const FVector2D& P) { return FMath::Lerp(Hex(0x2f7a3d), Hex(0x164a22), FMath::Clamp((P.Y - Y) / H, 0.0f, 1.0f)); });
		const float In = 1.875f * S;
		FillPoly(Hexa(X + In, Y + In, W - 2 * In, H - 2 * In), [=](const FVector2D& P)
		{
			// o degradê a 160°: quase de cima pra baixo, um pouco da esquerda pra direita
			const float T = FMath::Clamp(((P.Y - Y) / H) * 0.94f + ((P.X - X) / W) * 0.34f - 0.14f, 0.0f, 1.0f);
			return T < 0.45f ? FMath::Lerp(Hex(0xd9fbd4), Hex(0x7fdc78), T / 0.45f) : FMath::Lerp(Hex(0x7fdc78), Hex(0x3e9a48), FMath::Min(1.0f, (T - 0.45f) / 0.40f));
		});
		CheckMark(FVector2D(X + W * 0.5f, Y + H * 0.5f), 17.5f * S * W / (37.5f * S), 3.2f * S * W / (37.5f * S), Hex(0x0d2a12));
		return;
	}
	Image(Kind == 1 ? TEXT("skin:token_dark") : TEXT("skin:token_gold"), X, Y, W, H, FLinearColor::White, false);
	if (!Num.IsEmpty())
	{
		TextMid(Num, X + W * 0.5f, Y + H * 0.5f, Px, Kind == 1 ? Hex(0xe6ecf3) : Hex(0x2a1d05), EWeight::Bold, EAlign::Center, false);
	}
}

void FRTSHudPainter::ChainGlyph(FVector2D C, float Size, const FLinearColor& Col)
{
	// o ⛓ do protótipo: dois elos inclinados, um dentro do outro
	const float T = FMath::Max(1.0f, Size * 0.14f);
	for (const float Dx : { -0.22f, 0.22f })
	{
		const FVector2D O = C + FVector2D(Dx, -Dx) * Size;
		TArray<FVector2D> Ring;
		for (int32 I = 0; I <= 16; ++I)
		{
			const float A = 2.0f * PI * I / 16;
			const FVector2D P(FMath::Cos(A) * Size * 0.32f, FMath::Sin(A) * Size * 0.18f);
			Ring.Add(O + FVector2D(P.X * 0.707f - P.Y * 0.707f, P.X * 0.707f + P.Y * 0.707f));
		}
		for (int32 I = 0; I < 16; ++I) Line(Ring[I], Ring[I + 1], T, Col);
	}
}

void FRTSHudPainter::DrawGeneral(const FRTSHudState& State)
{
	const FRTSHudGeneral& G = State.General;
	if (!G.bOpen)
	{
		return;
	}
	const FName F = Cur->SkinFaction;
	const FGenColors Cl = GenColors(F);
	const FString Fac = F.IsNone() ? FString(TEXT("usa")) : F.ToString();
	auto X = [&](float V) { return SX(V, EAnchor::Center); };
	auto Y = [&](float V) { return SY(V, false); };
	auto RankIcon = [&](int32 R) { return Fac == TEXT("usa") ? FName(*FString::Printf(TEXT("skin:rank_usa_%d"), R)) : FName(*FString::Printf(TEXT("skin:%s/rank_%s_%d"), *Fac, *Fac, R)); };
	TArray<FRTSHudButton> Btn;   // os botões da tela vão na FRENTE da lista (ganham do HUD que fica por baixo)
	const int32 Rank = FMath::Clamp(G.Rank, 1, 5);

	// ---- o fundo da batalha escurece; a janela com o metal da facção e a moldura por fora
	Rect(0, 0, Canvas->ClipX, Canvas->ClipY, Hex(0x020407, 0.5f));
	const float WX = 172.5f, WY = 75.0f, WW = 1575.0f, WH = 850.0f;
	{
		const FName BgTex = FacSkin(TEXT("gen_bg"));
		const float Tile = 640.0f;   // o 512 do metal com o zoom 1,25 da janela
		ImageUV(BgTex, X(WX), Y(WY), WW * S, WH * S, FVector2D(0, 0), FVector2D(WW / Tile, WH / Tile), FLinearColor::White);
		const FMargin B = FrameBorder(F);
		Image(FacSkin(TEXT("gen_frame")), X(WX - B.Left), Y(WY - B.Top), (WW + B.Left + B.Right) * S, (WH + B.Top + B.Bottom) * S, FLinearColor::White, false);
	}

	// ---- o cabeçalho: o título, o general (a patente, o XP), a carteira de pontos e o fechar
	TextSpaced(TEXT("GENERAL'S COMMAND"), X(209.5f), Y(148.3f), 32.5f, FLinearColor::White, EWeight::Bold, 3.3f);
	Rect(X(209.5f), Y(185.5f), 1501.0f * S, FMath::Max(1.0f, S), Hex(0x2a3540));
	Rect(X(602.6f), Y(116.8f), FMath::Max(1.0f, S), 63.0f * S, Hex(0x2a3540));
	Image(FacSkin(TEXT("hub_general")), X(626.1f), Y(120.8f), 50.0f * S, 55.0f * S, FLinearColor::White, false);
	Image(RankIcon(Rank), X(630.6f), Y(124.9f), 41.0f * S, 41.8f * S, FLinearColor::White, false);
	{
		const FVector2D NE = TextMid(G.RankName[Rank - 1], X(688.6f), Y(128.8f), 20.0f, FLinearColor::White, EWeight::Bold, EAlign::Left, false);
		for (int32 I = 0; I < Rank; ++I)
		{
			Star(FVector2D(X(688.6f) + NE.X + (7.5f + 6.0f + I * 12.5f) * S, Y(129.6f)), 5.6f * S, Hex(0xf6c850));
		}
		Rect(X(687.6f), Y(144.8f), 264.5f * S, 8.3f * S, Hex(0x3a4552));
		Rect(X(688.6f), Y(145.8f), 262.5f * S, 6.3f * S, Hex(0x05080b));
		if (G.NextFrac > 0.0f)
		{
			const float FW = 262.5f * FMath::Clamp(G.NextFrac, 0.0f, 1.0f);
			const TPair<float, FLinearColor> Gold[3] = { { 0.0f, Hex(0x8d5f12) }, { 0.5f, Hex(0xffe39a) }, { 1.0f, Hex(0xc58e25) } };
			RoundRect(X(688.6f), Y(145.8f), FW * S, 6.3f * S, 0.0f, Gold);
		}
		FString Small = G.XpLine;
		if (Rank < 5)
		{
			const int32 Next = G.PointsPerRank[Rank];
			Small += FString::Printf(TEXT(" · next rank gives +%d %s"), Next, Next == 1 ? TEXT("point") : TEXT("points"));
		}
		TextMid(Small, X(688.6f), Y(169.8f), 15.0f, Hex(0x8fa3b8), EWeight::Medium, EAlign::Left, false);
	}
	GenToken(0, X(1231.8f), Y(117.0f), 72.5f * S, 62.5f * S, FString::FromInt(G.PointsLeft), 35.0f);
	TextSpaced(TEXT("POINTS"), X(1316.8f), Y(136.3f), 16.25f, Hex(0xf6c850), EWeight::Bold, 2.5f);
	TextMid(TEXT("to spend"), X(1316.8f), Y(159.8f), 15.0f, Hex(0x8fa3b8), EWeight::Medium, EAlign::Left, false);
	{
		const bool bHot = Cur->HoverId == FName(TEXT("gen_close"));
		const float BX = X(1668.0f), BY = Y(127.0f) - (bHot ? 2.5f * S : 0.0f), BW = 42.5f * S;
		RoundRect(BX, BY, BW, BW, 5.0f * S, bHot ? Hex(0x26323f) : Hex(0x141c25));
		RoundRectRing(BX, BY, BW, BW, 5.0f * S, FMath::Max(1.0f, S), Hex(0x3a4757));
		const FVector2D C(BX + BW * 0.5f, BY + BW * 0.5f);
		const float D = 7.0f * S;
		Line(C - FVector2D(D, D), C + FVector2D(D, D), 2.6f * S, Hex(0xcfd6de));
		Line(C + FVector2D(-D, D), C + FVector2D(D, -D), 2.6f * S, Hex(0xcfd6de));
		Btn.Add({ TEXT("gen_close"), FBox2D(FVector2D(BX, Y(127.0f)), FVector2D(BX + BW, Y(127.0f) + BW)) });
	}

	// ---- a trilha: as 5 patentes em colunas (a atual acesa embaixo, as que faltam apagadas), os poderes nas linhas
	const float TX0 = 209.5f, TY0 = 194.5f, ColW = 212.2f, ColStep = 223.45f, HeadH = 72.5f, Gap = 11.25f, TrackH = 578.5f;
	const int32 Rows = FMath::Max(1, G.Rows);
	const float RowH = (TrackH - HeadH - Rows * Gap) / Rows;
	auto ColX = [&](int32 R) { return TX0 + (R - 1) * ColStep; };
	auto RowY = [&](int32 Row) { return TY0 + HeadH + Gap + Row * (RowH + Gap); };
	const float FutureA = G.bLegible ? 0.62f : 0.38f;   // (05/11) na B as patentes que faltam ficam legíveis (é o que o jogador mira)
	for (int32 R = 1; R <= 5; ++R)
	{
		const float A = R > Rank ? FutureA : 1.0f;
		const float HX = X(ColX(R)), HY = Y(TY0), HW = ColW * S, HH = HeadH * S;
		RoundRect(HX, HY, HW, HH, 5.0f * S, Hex(Cl.HeadBg, A));
		if (R == Rank)
		{
			RoundRectRing(HX, HY, HW, HH, 5.0f * S, FMath::Max(1.0f, S), Hex(0x4a4027, A));
			Rect(HX + 1.0f * S, HY + HH - 5.0f * S, HW - 2.0f * S, 5.0f * S, Hex(0xf6c850));
		}
		else
		{
			RoundRectRing(HX, HY, HW, HH, 5.0f * S, FMath::Max(1.0f, S), Hex(Cl.HeadRing, A));
		}
		Image(RankIcon(R), HX + 8.7f * S, HY + 11.9f * S, 47.5f * S, 48.8f * S, FLinearColor(1, 1, 1, A), false);
		TextMid(Ellipsize(G.RankName[R - 1], 16.25f, EWeight::Bold, (ColW - 67.0f) * S), HX + 59.4f * S, HY + 19.35f * S, 16.25f, Hex(0xe6ecf3, A), EWeight::Bold, EAlign::Left, false);
		const float TokX = HX + (ColW - 8.75f - 27.5f) * S, TokY = HY + 42.5f * S;
		Image(TEXT("skin:token_gold"), TokX, TokY, 27.5f * S, 23.8f * S, FLinearColor(1, 1, 1, A), false);
		TextMid(FString::Printf(TEXT("+%d"), G.PointsPerRank[R - 1]), TokX - 3.75f * S, TokY + 11.9f * S, 13.75f, Hex(0xc9a85a, A), EWeight::Bold, EAlign::Right, false);
		if (R == Rank + 1)
		{
			// o XP rumo à próxima patente, embaixo do cabeçalho dela
			Rect(HX, HY + HH + 1.3f * S, HW, 3.8f * S, Hex(0x05080b));
			Rect(HX, HY + HH + 1.3f * S, HW * FMath::Clamp(G.NextFrac, 0.0f, 1.0f), 3.8f * S, Hex(0xf6c850));
		}
		if (R == Rank)
		{
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				RoundRect(HX, Y(RowY(Row)), HW, RowH * S, 5.0f * S, Hex(0xf6c850, 0.05f));
			}
		}
	}

	auto NodeById = [&](FName Id) -> const FRTSHudGenNode* { return G.Nodes.FindByPredicate([&](const FRTSHudGenNode& N) { return N.Id == Id; }); };
	auto StateOf = [&](const FRTSHudGenNode& N)
	{
		if (G.Owned.Contains(N.Id)) return EGenState::Own;
		if (Rank < N.Rank) return EGenState::Lock;
		if (G.Soon.Contains(N.Id)) return EGenState::Soon;
		if (!N.Req.IsNone() && !G.Owned.Contains(N.Req)) return EGenState::Req;
		return G.PointsLeft >= N.Cost ? EGenState::Buy : EGenState::Poor;
	};
	const float TileH = FMath::Min(107.5f, RowH);
	auto TileBox = [&](const FRTSHudGenNode& N) { return FBox2D(FVector2D(ColX(N.Rank), RowY(N.Row) + (RowH - TileH) * 0.5f), FVector2D(ColX(N.Rank) + ColW, RowY(N.Row) + (RowH + TileH) * 0.5f)); };

	// as linhas que ligam os níveis do mesmo poder (por baixo dos cartões)
	for (const FRTSHudGenNode& N : G.Nodes)
	{
		if (const FRTSHudGenNode* P = N.Req.IsNone() ? nullptr : NodeById(N.Req))
		{
			const FBox2D A = TileBox(*P), B = TileBox(N);
			const float LY = (B.Min.Y + B.Max.Y) * 0.5f;
			Rect(X(A.Max.X), Y(LY) - 1.25f * S, (B.Min.X - A.Max.X) * S, 2.5f * S, Hex(0x34414f));
		}
	}

	const FRTSHudGenNode* Hovered = nullptr;
	for (const FRTSHudGenNode& N : G.Nodes)
	{
		const FName BtnId(*(TEXT("gen_") + N.Id.ToString()));
		const EGenState St = StateOf(N);
		const bool bHover = Cur->HoverId == BtnId;
		if (bHover) Hovered = &N;
		if (G.bLegible)
		{
			// (05/11, a versão B; a segunda opinião e as árvores AAA: Diablo IV, PoE, CoH3, RA3) o ícone é o que se lê primeiro: 84 px (o
			// dobro da área de antes); o nome em 18 px bold branco; o nível numa plaquinha no canto do ícone; o custo em losangos pequenos
			// (cheio = ponto que você tem, vazado = o que falta) no lugar da moeda grande; um sinal só por estado: o fio dourado só em volta
			// do ícone que dá pra comprar, o check verde no comprado; o que falta (patente, nível, ainda não no jogo) fica sem cor mas legível
			const bool bGrey = St == EGenState::Lock || St == EGenState::Req || St == EGenState::Soon;
			const float CA = N.Rank > Rank ? FutureA : 1.0f;
			const FBox2D Bx = TileBox(N);
			float Shake = 0.0f;
			if (G.Shake == N.Id && G.ShakeAge < 0.28f)
			{
				Shake = FMath::Sin(G.ShakeAge / 0.28f * 3.0f * 2.0f * PI) * 4.0f * (1.0f - G.ShakeAge / 0.28f);
			}
			const float TX = X(Bx.Min.X + Shake), TY = Y(Bx.Min.Y), TW = ColW * S, TH = TileH * S;
			const float Lit = St == EGenState::Own ? 1.35f : 1.0f;   // o comprado: o fundo um tom mais claro
			const FLinearColor Top = Mul(Hex(bGrey ? Cl.PoorTop : Cl.TileTop), Lit, CA), Bot = Mul(Hex(bGrey ? Cl.PoorBot : Cl.TileBot), Lit, CA);
			FillPoly(ChamferPoly(TX, TY, TW, TH, 3.75f * S), [=](const FVector2D& P) { return FMath::Lerp(Top, Bot, FMath::Clamp((P.Y - TY) / TH, 0.0f, 1.0f)); });
			RoundRectRing(TX, TY, TW, TH, 6.25f * S, bHover ? 2.5f * S : FMath::Max(1.0f, S), bHover ? Hex(0xf6c850, CA) : Mul(Hex(bGrey ? Cl.PoorRing : Cl.TileRing), 1.0f, CA));
			// o ícone
			const float IS = 84.0f * S, IX = TX + 10.0f * S, IY = TY + (TH - IS) * 0.5f;
			RoundRect(IX - 1.0f * S, IY + 2.5f * S, IS + 2.0f * S, IS + 2.0f * S, 5.0f * S, FLinearColor(0, 0, 0, 0.4f * CA));
			Image(bGrey ? FName(*(TEXT("mono:") + N.Icon.ToString())) : N.Icon, IX, IY, IS, IS, FLinearColor(1, 1, 1, CA), false);
			if (St == EGenState::Buy)
			{
				const float Pu = 0.55f + 0.45f * Pulse(0.7f);
				RoundRectRing(IX - 2.0f * S, IY - 2.0f * S, IS + 4.0f * S, IS + 4.0f * S, 5.0f * S, 2.0f * S, Hex(0xf6c850, Pu));
			}
			// o nível na plaquinha do canto de baixo do ícone
			if (!N.Level.IsEmpty())
			{
				const float LW = Measure(N.Level, 15.0f, EWeight::Bold).X + 12.0f * S, LH = 22.0f * S;
				const float LX = IX + IS - LW + 2.0f * S, LY = IY + IS - LH + 2.0f * S;
				RoundRect(LX, LY, LW, LH, 3.5f * S, FLinearColor(0.02f, 0.03f, 0.04f, 0.88f * CA));
				RoundRectRing(LX, LY, LW, LH, 3.5f * S, FMath::Max(1.0f, S), Hex(bGrey ? 0x4a5562 : 0x8a7440, CA));
				TextMid(N.Level, LX + LW * 0.5f, LY + LH * 0.5f, 15.0f, Hex(bGrey ? 0xb4bdc6 : 0xf6d27a, CA), EWeight::Bold, EAlign::Center, false);
			}
			// o nome (até 2 linhas, 18 px; se não couber, 16) e embaixo o custo
			const float NX = IX + IS + 10.0f * S, NW = TX + TW - 8.0f * S - NX;
			float NPx = 18.0f;
			TArray<FString> NL = WrapLines(N.Name, NPx, EWeight::Bold, NW, 2);
			for (const FString& L : NL)
			{
				if (Measure(L, NPx, EWeight::Bold).X > NW) { NPx = 16.0f; NL = WrapLines(N.Name, NPx, EWeight::Bold, NW, 2); break; }
			}
			const FLinearColor NameC = Hex(bGrey ? 0x9aa5b0 : (St == EGenState::Own ? 0xe3f5e1 : 0xeef1f4), CA);
			const float LineH = (NPx + 3.0f) * S, PipH = 14.0f * S;
			float NY = TY + (TH - (NL.Num() * LineH + 7.0f * S + PipH)) * 0.5f;
			for (const FString& L : NL)
			{
				TextMid(L, NX, NY + LineH * 0.5f, NPx, NameC, EWeight::Bold, EAlign::Left, false);
				NY += LineH;
			}
			const float PY0 = NY + 7.0f * S + PipH * 0.5f;
			if (St == EGenState::Own)
			{
				CheckMark(FVector2D(NX + 9.0f * S, PY0), 15.0f * S, 2.8f * S, Hex(0x7fe39a, CA));
			}
			else
			{
				// os losangos do custo: cheios até os pontos que você tem (dourados), vazados os que faltam; sem cor no que não dá
				const FLinearColor Gold = Hex(0xf6c850, CA), Grey = Hex(0x7d8894, CA);
				for (int32 K = 0; K < N.Cost; ++K)
				{
					const FVector2D C(NX + 7.0f * S + K * 17.0f * S, PY0);
					const float D = 6.5f * S;
					const bool bHave = !bGrey && K < G.PointsLeft;
					const FLinearColor Col = bGrey ? Grey : Gold;
					FillPoly({ C + FVector2D(0, -D), C + FVector2D(D, 0), C + FVector2D(0, D), C + FVector2D(-D, 0) }, [Col](const FVector2D&) { return Col; });
					if (!bHave)
					{
						const float Di = D - 2.0f * S;
						const FLinearColor In = Bot;
						FillPoly({ C + FVector2D(0, -Di), C + FVector2D(Di, 0), C + FVector2D(0, Di), C + FVector2D(-Di, 0) }, [In](const FVector2D&) { return In; });
					}
				}
				if (St == EGenState::Soon)
				{
					TextSpaced(TEXT("IN DEV"), NX + N.Cost * 17.0f * S + 8.0f * S, PY0, 11.0f, Hex(0x7d8894, CA), EWeight::Bold, 1.0f);
				}
			}
			Btn.Add({ BtnId, FBox2D(FVector2D(TX, TY), FVector2D(TX + TW, TY + TH)) });
			continue;
		}
		const float ColA = N.Rank > Rank ? 0.38f : 1.0f;
		const float Dim = St == EGenState::Lock ? 0.5f : 1.0f;              // o trancado: brightness(.5)
		const FBox2D Bx = TileBox(N);
		float Shake = 0.0f;
		if (G.Shake == N.Id && G.ShakeAge < 0.28f)
		{
			Shake = FMath::Sin(G.ShakeAge / 0.28f * 3.0f * 2.0f * PI) * 4.0f * (1.0f - G.ShakeAge / 0.28f);
		}
		const float Lift = bHover && St == EGenState::Buy ? -3.75f : 0.0f;
		const float TX = X(Bx.Min.X + Shake), TY = Y(Bx.Min.Y + Lift), TW = ColW * S, TH = TileH * S;
		const bool bPoorLook = St == EGenState::Poor || St == EGenState::Req || St == EGenState::Soon;
		const FLinearColor Top = Mul(Hex(bPoorLook ? Cl.PoorTop : Cl.TileTop), Dim, ColA), Bot = Mul(Hex(bPoorLook ? Cl.PoorBot : Cl.TileBot), Dim, ColA);
		{
			const float R = 6.25f * S;
			// o degradê de cima pra baixo: duas faixas (o RoundRect pinta na horizontal), a de cima e a de baixo com os cantos
			FillPoly(ChamferPoly(TX, TY, TW, TH, R * 0.6f), [=](const FVector2D& P) { return FMath::Lerp(Top, Bot, FMath::Clamp((P.Y - TY) / TH, 0.0f, 1.0f)); });
		}
		if (bHover)
		{
			RoundRectRing(TX, TY, TW, TH, 6.25f * S, 2.5f * S, Mul(Hex(0xf6c850), Dim, ColA));
			if (St == EGenState::Buy) RoundRect(TX, TY, TW, TH, 6.25f * S, FLinearColor(1, 1, 1, 0.05f));
		}
		else
		{
			RoundRectRing(TX, TY, TW, TH, 6.25f * S, FMath::Max(1.0f, S), Mul(Hex(bPoorLook ? Cl.PoorRing : Cl.TileRing), Dim, ColA));
		}
		// o ícone, com a sombra embaixo; sem dinheiro e esperando o nível, apagado; trancado, quase cinza
		const float IS = 57.5f * S, IX = TX + 7.5f * S, IY = TY + (TH - IS) * 0.5f;
		RoundRect(IX - 1.0f * S, IY + 2.5f * S, IS + 2.0f * S, IS + 2.0f * S, 5.0f * S, FLinearColor(0, 0, 0, 0.35f * ColA));
		const FName Ic = St == EGenState::Lock ? FName(*(TEXT("dim90:") + N.Icon.ToString())) : bPoorLook ? FName(*(TEXT("dim35:") + N.Icon.ToString())) : N.Icon;
		Image(Ic, IX, IY, IS, IS, FLinearColor(1, 1, 1, ColA), false);
		if (St == EGenState::Own)
		{
			RoundRectRing(IX - 1.25f * S, IY - 1.25f * S, IS + 2.5f * S, IS + 2.5f * S, 5.0f * S, 1.25f * S, Hex(0x6fd36a, 0.35f));
		}
		// o nome (quebra em até 3 linhas) e o nível em romano à direita do bloco do nome, no meio da altura (o .nm do protótipo é um flex:
		// o nível divide a largura com o texto)
		const FLinearColor NameC = Mul(St == EGenState::Own ? Hex(0xe3f5e1) : bPoorLook ? Hex(0x8a95a2) : Hex(0xe6ecf3), Dim, ColA);
		const float NX = TX + 73.75f * S, NW = (ColW - 73.75f - 37.5f) * S;
		const float LvW = N.Level.IsEmpty() ? 0.0f : Measure(N.Level, 15.0f, EWeight::Bold).X + 11.25f * S;
		const TArray<FString> Lines = WrapLines(N.Name, 15.0f, EWeight::Bold, NW - LvW, 3);
		const float LineH = 16.5f * S, CY0 = TY + TH * 0.5f - (Lines.Num() - 1) * LineH * 0.5f;
		float BlockW = 0.0f;
		for (int32 L = 0; L < Lines.Num(); ++L)
		{
			BlockW = FMath::Max(BlockW, TextMid(Lines[L], NX, CY0 + L * LineH, 15.0f, NameC, EWeight::Bold, EAlign::Left, false).X);
		}
		if (LvW > 0.0f)
		{
			TextMid(N.Level, NX + BlockW + 8.75f * S, TY + TH * 0.5f, 15.0f, Mul(Hex(0x8c9aa8), Dim, ColA), EWeight::Bold, EAlign::Left, false);
		}
		// o canto: a ficha do custo (dourada; escura sem pontos), o ✓ verde do comprado, o elo do nível que falta ou o cadeado
		const float KX = TX + TW - (5.0f + 37.5f) * S, KY = TY + 5.0f * S;
		switch (St)
		{
		case EGenState::Own: GenToken(2, KX, KY, 37.5f * S, 32.5f * S, FString(), 0.0f); break;
		case EGenState::Buy: GenToken(0, KX, KY, 37.5f * S, 32.5f * S, FString::FromInt(N.Cost), 17.5f); break;
		case EGenState::Poor:
		case EGenState::Soon: GenToken(1, KX, KY, 37.5f * S, 32.5f * S, FString::FromInt(N.Cost), 17.5f); break;
		case EGenState::Req:
		{
			const FRTSHudGenNode* P = NodeById(N.Req);
			const FString Lv = P ? P->Level : FString();
			const float CW = 13.0f * S + Measure(Lv, 12.5f, EWeight::Bold).X + 10.0f * S, CX = TX + TW - 6.25f * S - CW, CYc = TY + 6.25f * S;
			RoundRect(CX, CYc, CW, 21.0f * S, 3.0f * S, Hex(0x0b1118, ColA));
			ChainGlyph(FVector2D(CX + 9.0f * S, CYc + 10.5f * S), 11.0f * S, Hex(0x8fa3b8, ColA));
			TextMid(Lv, CX + 17.0f * S, CYc + 10.5f * S, 12.5f, Hex(0x8fa3b8, ColA), EWeight::Bold, EAlign::Left, false);
			break;
		}
		case EGenState::Lock: Image(TEXT("icon:locked"), TX + TW - (7.5f + 20.0f) * S, TY + 5.0f * S, 20.0f * S, 20.0f * S, FLinearColor(0.5f, 0.5f, 0.5f, ColA), false); break;
		}
		Btn.Add({ BtnId, FBox2D(FVector2D(TX, TY), FVector2D(TX + TW, TY + TH)) });
	}

	// ---- o detalhe do poder sob o mouse (o painel fica, vazio, sem nada apontado)
	{
		const float DX = X(1335.5f), DY = Y(194.5f), DW = 375.0f * S, DH = 578.5f * S;
		const FLinearColor DT = Hex(Cl.DetTop), DB = Hex(Cl.DetBot);
		FillPoly(ChamferPoly(DX, DY, DW, DH, 4.0f * S), [=](const FVector2D& P) { return FMath::Lerp(DT, DB, FMath::Clamp((P.Y - DY) / DH, 0.0f, 1.0f)); });
		RoundRectRing(DX, DY, DW, DH, 7.5f * S, FMath::Max(1.0f, S), Hex(Cl.DetRing));
		const FRTSHudGenNode* Shown = Hovered;
		if (!Shown && G.bLegible)
		{
			// (05/11, a B) o painel nunca fica vazio: sem nada sob o mouse, o 1º poder que dá pra comprar agora (o foco sempre presente do
			// CoH3); sem nenhum, o 1º da patente de agora
			for (int32 Pass = 0; Pass < 2 && !Shown; ++Pass)
			{
				for (const FRTSHudGenNode& Nd : G.Nodes)
				{
					const EGenState St0 = StateOf(Nd);
					if ((Pass == 0 && St0 == EGenState::Buy) || (Pass == 1 && Nd.Rank == Rank && St0 != EGenState::Own))
					{
						if (!Shown || Nd.Rank < Shown->Rank || (Nd.Rank == Shown->Rank && Nd.Row < Shown->Row)) Shown = &Nd;
					}
				}
			}
		}
		if (Shown)
		{
			const FRTSHudGenNode& N = *Shown;
			const EGenState St = StateOf(N);
			const float PX = DX + 17.5f * S, PW = 340.0f * S;
			RoundRect(PX - 1.0f * S, DY + 21.0f * S, 97.0f * S, 97.0f * S, 6.0f * S, FLinearColor(0, 0, 0, 0.4f));
			Image(N.Icon, PX, DY + 17.5f * S, 95.0f * S, 95.0f * S, FLinearColor::White, false);
			// o título e a linha do custo, centrados ao lado do ícone
			const float TXd = PX + 110.0f * S, TWd = DX + DW - 17.5f * S - TXd;
			const FString Title = N.Name + (N.Level.IsEmpty() ? FString() : TEXT(" ") + N.Level);
			const TArray<FString> TL = WrapLines(Title, 26.25f, EWeight::Bold, TWd, 2);
			const FString A1 = TEXT(" cost · ") + G.RankAbbr[N.Rank - 1];
			const FString A2 = TEXT(" · cooldown ") + N.Cooldown;
			const float StarsW = N.Rank * 12.5f * S + 5.0f * S;
			const float Row1W = 27.5f * S + Measure(A1, 16.25f, EWeight::SemiBold).X + StarsW + Measure(A2, 16.25f, EWeight::SemiBold).X;
			const bool bMeta2 = Row1W > TWd;
			const float BlockH = TL.Num() * 28.9f * S + 4.0f * S + (bMeta2 ? 48.4f : 29.4f) * S;
			float Yc = DY + 17.5f * S + (95.0f * S - BlockH) * 0.5f;
			for (const FString& L : TL)
			{
				TextMid(L, TXd, Yc + 14.45f * S, 26.25f, FLinearColor::White, EWeight::Bold, EAlign::Left, false);
				Yc += 28.9f * S;
			}
			Yc += 4.0f * S;
			GenToken(0, TXd, Yc + 2.8f * S, 27.5f * S, 23.8f * S, FString::FromInt(N.Cost), 14.4f);
			float Pen = TXd + 27.5f * S;
			const float MY = Yc + 14.7f * S;
			Pen += TextMid(A1, Pen, MY, 16.25f, Hex(0x9fb0c2), EWeight::SemiBold, EAlign::Left, false).X;
			for (int32 I = 0; I < N.Rank; ++I) Star(FVector2D(Pen + (9.0f + I * 12.5f) * S, MY), 5.2f * S, Hex(0x9fb0c2));
			Pen += StarsW;
			if (bMeta2)
			{
				TextMid(A2.Mid(3), TXd, MY + 19.0f * S, 16.25f, Hex(0x9fb0c2), EWeight::SemiBold, EAlign::Left, false);
			}
			else
			{
				TextMid(A2, Pen, MY, 16.25f, Hex(0x9fb0c2), EWeight::SemiBold, EAlign::Left, false);
			}
			// por que dá ou não dá pra comprar
			float Y2 = DY + 132.0f * S;
			{
				FString Why;
				FLinearColor WC = Hex(0x9fb0c2);
				float Pre = 0.0f;
				switch (St)
				{
				case EGenState::Buy: Why = TEXT("You can buy it: click the power"); WC = Hex(0xf6c850); break;
				case EGenState::Soon: Why = TEXT("Not in the game yet"); break;
				case EGenState::Own: Why = TEXT("Owned"); WC = Hex(0x7fe39a); Pre = 20.0f * S; CheckMark(FVector2D(PX + 7.0f * S, Y2), 14.0f * S, 2.6f * S, WC); break;
				case EGenState::Lock: Why = TEXT("Reach ") + G.RankName[N.Rank - 1]; Pre = 22.0f * S; Image(TEXT("icon:locked"), PX - 1.0f * S, Y2 - 9.0f * S, 18.0f * S, 18.0f * S, FLinearColor::White, false); break;
				case EGenState::Req:
				{
					const FRTSHudGenNode* P = NodeById(N.Req);
					Why = TEXT("Buy ") + (P ? P->Name + TEXT(" ") + P->Level : FString()) + TEXT(" first");
					Pre = 20.0f * S;
					ChainGlyph(FVector2D(PX + 7.0f * S, Y2), 13.0f * S, WC);
					break;
				}
				case EGenState::Poor:
				{
					const int32 Need = N.Cost - G.PointsLeft;
					Why = FString::Printf(TEXT("Need %d more point%s"), Need, Need > 1 ? TEXT("s") : TEXT(""));
					break;
				}
				}
				TextSpaced(Why, PX + Pre, Y2, 16.25f, WC, EWeight::Bold, 0.625f);
			}
			// a descrição; nos poderes com níveis, a do primeiro e a lista dos níveis (o comprado em verde)
			TArray<const FRTSHudGenNode*> Chain;
			if (!N.Level.IsEmpty())
			{
				for (const FRTSHudGenNode& M : G.Nodes) if (M.Name == N.Name && !M.Level.IsEmpty()) Chain.Add(&M);
				Chain.Sort([](const FRTSHudGenNode& A, const FRTSHudGenNode& B) { return A.Rank < B.Rank; });
			}
			float PY = DY + 151.5f * S;
			for (const FString& L : WrapLines(Chain.Num() ? Chain[0]->Desc : N.Desc, 17.5f, EWeight::Medium, PW, 0))
			{
				TextMid(L, PX, PY + 12.7f * S, 17.5f, Hex(0xb9c7d6), EWeight::Medium, EAlign::Left, false);
				PY += 25.4f * S;
			}
			PY += 10.0f * S;
			for (const FRTSHudGenNode* M : Chain)
			{
				const bool bOn = G.Owned.Contains(M->Id);
				const FString Txt = M->Level + TEXT(" · ") + M->Desc;
				const FString Tag = TEXT(" (") + G.RankAbbr[M->Rank - 1] + TEXT(")");
				TArray<FString> LL = WrapLines(Txt + Tag, 16.25f, EWeight::Medium, PW, 0);
				for (int32 L = 0; L < LL.Num(); ++L)
				{
					// a sigla da patente no fim, mais apagada
					FString Body = LL[L];
					const bool bLast = L == LL.Num() - 1 && Body.EndsWith(Tag.TrimStart());
					if (bLast) Body.LeftChopInline(Tag.TrimStart().Len());
					const FVector2D E = TextMid(Body, PX, PY + 11.9f * S, 16.25f, bOn ? Hex(0x7fe39a) : Hex(0x8492a1), EWeight::Medium, EAlign::Left, false);
					if (bLast) TextMid(Tag.TrimStart(), PX + E.X, PY + 11.9f * S, 16.25f, Hex(0x6f7b88), EWeight::Medium, EAlign::Left, false);
					PY += 21.0f * S;
				}
				PY += 3.75f * S;
			}
		}
	}

	// ---- as comendas, na faixa de baixo
	for (int32 I = 0; I < G.Medals.Num() && I < 4; ++I)
	{
		const FRTSHudGenMedal& M = G.Medals[I];
		const float MX = X(209.5f + I * 279.6f), MY = Y(785.5f), MW = 267.1f * S, MH = 97.5f * S;
		RoundRect(MX, MY, MW, MH, 5.0f * S, Hex(Cl.HeadBg));
		RoundRectRing(MX, MY, MW, MH, 5.0f * S, FMath::Max(1.0f, S), Hex(Cl.HeadRing));
		const FName Ic = M.bDone ? M.Icon : FName(*(TEXT("mono:") + M.Icon.ToString()));
		Image(Ic, MX + 12.5f * S, MY + 16.25f * S, 42.5f * S, 65.0f * S, FLinearColor::White, false);
		const FString Xp = FString::Printf(TEXT("+%d"), M.Xp);
		const float XpW = Measure(Xp, 16.25f, EWeight::Bold).X + (M.bDone ? 20.0f * S : 0.0f);
		const FVector2D XE = TextMid(Xp, MX + MW - 12.5f * S, MY + MH * 0.5f, 16.25f, Hex(0xf6c850), EWeight::Bold, EAlign::Right, false);
		if (M.bDone)
		{
			CheckMark(FVector2D(MX + MW - 12.5f * S - XE.X - 11.0f * S, MY + MH * 0.5f), 13.0f * S, 2.4f * S, Hex(0xf6c850));
		}
		const float TXm = MX + 67.5f * S, TWm = MW - 67.5f * S - 12.5f * S - XpW - 12.5f * S;
		const TArray<FString> LL = WrapLines(M.Line, 15.0f, EWeight::Medium, TWm, 2);
		const bool bBar = M.Progress >= 0.0f && !M.bDone;
		const float BlockH = (27.0f + LL.Num() * 18.0f + (bBar ? 11.0f : 0.0f)) * S;
		float Ym = MY + (MH - BlockH) * 0.5f;
		TextMid(Ellipsize(M.Name, 17.5f, EWeight::Bold, TWm), TXm, Ym + 11.0f * S, 17.5f, Hex(0xe6ecf3), EWeight::Bold, EAlign::Left, false);
		Ym += 27.0f * S;
		for (const FString& L : LL)
		{
			TextMid(L, TXm, Ym + 9.0f * S, 15.0f, Hex(0x8492a1), EWeight::Medium, EAlign::Left, false);
			Ym += 18.0f * S;
		}
		if (bBar)
		{
			Ym += 6.0f * S;
			Rect(TXm, Ym, TWm, 5.0f * S, Hex(0x05080b));
			Rect(TXm, Ym, TWm * FMath::Clamp(M.Progress, 0.0f, 1.0f), 5.0f * S, Hex(0x8fc6ff));
		}
	}

	// a janela segura o clique; fora dela, fecha (o fundo escurecido)
	Btn.Add({ TEXT("gen_bg"), FBox2D(FVector2D(X(WX), Y(WY)), FVector2D(X(WX + WW), Y(WY + WH))) });
	Btn.Add({ TEXT("gen_back"), FBox2D(FVector2D(0, 0), FVector2D(Canvas->ClipX, Canvas->ClipY)) });
	Buttons->Insert(Btn, 0);
}
