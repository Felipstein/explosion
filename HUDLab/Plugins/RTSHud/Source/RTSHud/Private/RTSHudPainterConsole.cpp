#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "GlobalRenderResources.h"

// O console que vive da energia da base (aprovado em 31/10–01/11, docs/concept/hud/DECISOES.md §7 e §9; o protótipo
// docs/concept/hud/prototipo/index.html): a fileira das abas dos grupos dos dois lados do brasão do dinheiro (o mostrador), a espinha
// de energia na borda de cima do painel do meio (uma célula por ponto de energia: verde, amarela quando sobra menos de 3, vermelha
// faltando) e, na emenda do painel com o cartão, o cubo com a patente e o tubo dos 5 degraus. As medidas são as do protótipo (o
// painel de 7 colunas, de topo em 868); com outro painel, tudo acompanha o topo dele. Sem nada selecionado, a fileira desce 199.

namespace
{
	FLinearColor Hex(uint32 RGB, float A = 1.0f)
	{
		FLinearColor C(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255));
		C.A = A;
		return C;
	}

	struct FFacConsole
	{
		FBox2D Odo;    // a janela do mostrador, na planta
		FBox2D Hub;    // a peça do cubo (cada facção tem o desenho num tamanho)
	};

	FFacConsole FacConsole(FName F)
	{
		auto B = [](float X, float Y, float W, float H) { return FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)); };
		if (F == TEXT("china")) return { B(880.0f, 799.8f, 160.0f, 44.6f), B(-1.2f, -2.2f, 55.1f, 61.8f) };
		if (F == TEXT("gla")) return { B(877.5f, 804.2f, 165.0f, 42.5f), B(-1.6f, -1.3f, 61.3f, 63.5f) };
		if (F == TEXT("russia")) return { B(878.8f, 804.4f, 162.5f, 39.4f), B(-4.7f, 3.4f, 60.3f, 50.2f) };
		return { B(881.0f, 805.7f, 158.0f, 40.2f), B(0.0f, 0.1f, 52.0f, 57.6f) };
	}
}

FName FRTSHudPainter::FacSkin(const TCHAR* Piece) const
{
	// a peça da facção (skin:<facção>/<peça>) quando existe; senão a da USA (skin:<peça>)
	if (Cur && !Cur->SkinFaction.IsNone() && Lookup)
	{
		const FName N(*FString::Printf(TEXT("skin:%s/%s"), *Cur->SkinFaction.ToString(), Piece));
		if (Lookup(N))
		{
			return N;
		}
	}
	return FName(*FString::Printf(TEXT("skin:%s"), Piece));
}

void FRTSHudPainter::DrawConsole(const FRTSHudState& State)
{
	const bool bNone = State.bGroupsAtBottom;
	const float PTop = 1072.0f - CommandPanelHeight(State);
	// a fileira: presa ao topo do painel (e sobe junto com ele); sem seleção, 199 abaixo da posição do painel de 7 colunas
	const float RowDY = bNone ? 199.0f : PTop - 868.0f;
	const float Drop = bNone ? 0.0f : PanelDrop(State);
	auto Y = [&](float V) { return SY(V + RowDY, true) + Drop; };

	// ---- a espinha de energia (a GLA sem usina nenhuma: a barra some)
	const int32 Used = State.PowerUsed, Made = State.PowerMade;
	const bool bNoPower = Cur->SkinFaction == FName(TEXT("gla")) && Used == 0 && Made == 0;
	if (!bNoPower)
	{
		const float SXL = SX(392.0f, EAnchor::Left), SXR = SX(1492.0f, EAnchor::Right);
		Skin(TEXT("skin:power_conduit"), SXL, Y(854.0f), SXR - SXL, 24.0f * S, FMargin(72.0f, 0.0f), 22.0f / 72.0f * S);
		const bool bOver = Used > Made, bNear = !bOver && Made - Used < 3;
		const int32 N = FMath::Max(Made, Used);
		const float CX0 = SXL + 22.0f * S, CW0 = SXR - SXL - 44.0f * S, CY = Y(861.0f), CH = 10.0f * S, Gap = 4.0f * S;
		const float CellW = N > 0 ? FMath::Min(44.0f * S, (CW0 - Gap * (N - 1)) / N) : 0.0f;
		// as cores do protótipo: o verde normal, o âmbar quase sem, o vermelho faltando (DECISOES §9)
		const FLinearColor UTop = bOver ? Hex(0xff9e8f) : bNear ? Hex(0xffe0a8) : Hex(0xc9f7cf);
		const FLinearColor UMid = bOver ? Hex(0xc8281b) : bNear ? Hex(0xffab2e) : Hex(0x4fd36a);
		const FLinearColor UBot = bOver ? Hex(0x6a0e08) : bNear ? Hex(0xb06a0c) : Hex(0x1f8a34);
		const float Blink = (FMath::Fmod(State.Time, 1.0f) < 0.5f) ? 1.0f : 0.35f;
		for (int32 I = 0; I < N; ++I)
		{
			const float X = CX0 + I * (CellW + Gap);
			if (I < FMath::Min(Used, Made))
			{
				// aceso: o degradê de cima pra baixo e o brilho em volta
				FillPoly(ChamferPoly(X - 3 * S, CY - 3 * S, CellW + 6 * S, CH + 6 * S, 3 * S), [&](const FVector2D&) { return FLinearColor(UMid.R, UMid.G, UMid.B, 0.16f); });
				FillPoly({ { X, CY }, { X + CellW, CY }, { X + CellW, CY + CH * 0.4f }, { X, CY + CH * 0.4f } }, [&](const FVector2D& P) { return FMath::Lerp(UTop, UMid, FMath::Clamp((P.Y - CY) / (CH * 0.4f), 0.0f, 1.0f)); });
				FillPoly({ { X, CY + CH * 0.4f }, { X + CellW, CY + CH * 0.4f }, { X + CellW, CY + CH }, { X, CY + CH } }, [&](const FVector2D& P) { return FMath::Lerp(UMid, UBot, FMath::Clamp((P.Y - CY - CH * 0.4f) / (CH * 0.6f), 0.0f, 1.0f)); });
			}
			else if (I >= Made && I < Used)
			{
				// o que falta: vermelho pulsando depois do fim da produção
				const float A = 0.45f + 0.55f * Pulse(0.83f);
				FillPoly(ChamferPoly(X - 3 * S, CY - 3 * S, CellW + 6 * S, CH + 6 * S, 3 * S), [&](const FVector2D&) { return Hex(0xff3b2c, 0.3f * A); });
				Rect(X, CY, CellW, CH, Hex(0xff3b2c, A));
			}
			else
			{
				// a sobra: apagada; quase sem energia, âmbar piscando
				const FLinearColor Free = bNear ? Hex(0xffab2e, 0.6f * Blink) : Hex(0x4fd36a, 0.13f);
				Rect(X, CY, CellW, CH, Free);
				Border(X, CY, CellW, CH, FMath::Max(1.0f, S), bNear ? Hex(0xffab2e, 0.5f * Blink) : Hex(0x4fd36a, 0.32f));
			}
		}
		// a luz que corre nas acesas a cada 7 s (some quase sem e faltando)
		const float Lit = FMath::Min(Used, Made) * (CellW + Gap);
		const float T = FMath::Fmod(State.Time, 7.0f) / 2.1f;
		if (!bNear && !bOver && Lit > 0.0f && T < 1.0f)
		{
			const float GX = CX0 - 100.0f * S + (Lit + 100.0f * S) * T, GW = 90.0f * S;
			const TPair<float, FLinearColor> Stops[3] = { { 0.0f, FLinearColor(1, 1, 1, 0) }, { 0.5f, FLinearColor(1, 1, 1, 0.55f) }, { 1.0f, FLinearColor(1, 1, 1, 0) } };
			RoundRect(GX, CY, GW, CH, 0.0f, Stops, CX0, CX0 + Lit);
		}
		// o número: o raio e "usado / feito" (vermelho faltando, amarelo quase sem)
		const FLinearColor BoltC = bOver ? Hex(0xff3b2c) : bNear ? Hex(0xffd23a) : Hex(0x4fd36a);
		const FLinearColor NumC = bOver ? Hex(0xff7b66) : bNear ? Hex(0xffe27a) : Hex(0xe9edf2);
		Image(TEXT("skin:bolt"), SX(394.0f, EAnchor::Left), Y(830.0f), 19.0f * S, 19.0f * S, BoltC, false);
		TextMid(FString::Printf(TEXT("%d / %d"), Used, Made), SX(416.0f, EAnchor::Left), Y(839.5f), 19.0f, NumC, EWeight::Bold);
		Bg(SXL, Y(854.0f), SXR - SXL, 24.0f * S);
	}

	// ---- as abas dos grupos: 1–5 à esquerda do brasão, 6–0 à direita
	for (int32 I = 0; I < 10 && I < State.Groups.Num(); ++I)
	{
		const FRTSHudGroup& G = State.Groups[I];
		const bool bOn = G.Count > 0;
		const float PX = I < 5 ? 561.0f + I * 55.0f : 1091.0f + (I - 5) * 55.0f;
		const float X = SX(PX, EAnchor::Center), TY = Y(806.0f), W = 48.0f * S, H = 46.0f * S;
		const FName GId(*FString::Printf(TEXT("group_%d"), (I + 1) % 10));
		const float Lift = Cur->HoverId == GId ? -3.0f * S : (Cur->PressedId == GId ? 1.0f * S : 0.0f);
		Image(FacSkin(bOn ? TEXT("tab_on") : TEXT("tab_off")), X, TY + Lift, W, H, FLinearColor::White, false);
		const FString Num = FString::FromInt((I + 1) % 10);
		if (bOn)
		{
			TextMid(Num, X + 11.0f * S, TY + Lift + 21.5f * S, 23.0f, FLinearColor::White, EWeight::Bold);
			if (G.Count > 1)
			{
				TextMid(FString::Printf(TEXT("×%d"), G.Count), X + 41.0f * S, TY + Lift + 11.0f * S, 10.0f, Hex(0xcfd9e4), EWeight::Bold, EAlign::Right, false);
			}
			// a vida do grupo: verde inteira; ferida, laranja até onde vai e o resto apagado
			const float BX = X + 7.0f * S, BY = TY + Lift + 37.0f * S, BW = 34.0f * S, BH = 3.0f * S;
			const float H01 = FMath::Clamp(G.Health, 0.0f, 1.0f);
			if (H01 < 0.999f)
			{
				Rect(BX, BY, BW, BH, FLinearColor(1, 1, 1, 0.12f));
			}
			Rect(BX, BY, BW * H01, BH, H01 > 0.6f ? Hex(0x4fd36a) : (H01 > 0.3f ? Hex(0xff9a2e) : Hex(0xff3b2c)));
		}
		else
		{
			TextMid(Num, X + W * 0.5f, TY + Lift + H * 0.5f, 23.0f, Cur->HoverId == GId ? FLinearColor::White : Hex(0x8995a3), EWeight::Bold, EAlign::Center);
		}
		Buttons->Add({ GId, FBox2D(FVector2D(X, TY), FVector2D(X + W, TY + H)) });
	}

	// ---- o brasão do dinheiro com o mostrador (o "$4,250" numa janela escura; 01/11 o aprovado, sem os tambores)
	if (!State.Money.IsEmpty())
	{
		const FName Crest = FacSkin(TEXT("crest_money"));
		UTexture* CT = Lookup ? Lookup(Crest) : nullptr;
		const float CW = 250.0f, CH = CT ? CW * CT->GetSurfaceHeight() / FMath::Max(1.0f, float(CT->GetSurfaceWidth())) : 71.4f;
		const float CX = SX(835.0f, EAnchor::Center), CYt = Y(860.0f - CH);
		Image(Crest, CX, CYt, CW * S, CH * S, FLinearColor::White, false);
		const FBox2D O = FacConsole(Cur->SkinFaction).Odo;
		const float OX = SX(O.Min.X, EAnchor::Center), OY = Y(O.Min.Y), OW = (O.Max.X - O.Min.X) * S, OH = (O.Max.Y - O.Min.Y) * S;
		FillPoly(ChamferPoly(OX - 1 * S, OY - 1 * S, OW + 2 * S, OH + 2 * S, 3.5f * S), [](const FVector2D&) { return FLinearColor(1, 1, 1, 0.06f); });
		FillPoly(ChamferPoly(OX, OY, OW, OH, 3.0f * S), [&](const FVector2D& P)
		{
			const float K = FMath::Clamp((P.Y - OY) / FMath::Max(1.0f, OH), 0.0f, 1.0f);
			return K < 0.48f ? FMath::Lerp(Hex(0x030507), Hex(0x0b1118), K / 0.48f) : FMath::Lerp(Hex(0x0b1118), Hex(0x05080b), (K - 0.48f) / 0.52f);
		});
		// a sombra de dentro em cima (o vidro afundado)
		const TPair<float, FLinearColor> Shade[2] = { { 0.0f, FLinearColor(0, 0, 0, 0.55f) }, { 1.0f, FLinearColor(0, 0, 0, 0.55f) } };
		RoundRect(OX + 1 * S, OY + 1 * S, OW - 2 * S, 4 * S, 1.0f * S, Shade);
		FString Money = State.Money.Replace(TEXT(" "), TEXT(""));
		TextMid(Money, OX + OW * 0.5f, OY + OH * 0.5f + 0.5f * S, 33.0f, State.bLowMoney ? Hex(0xff3b2c) : Hex(0xffdc78), EWeight::Bold, EAlign::Center);
		Bg(CX, CYt, CW * S, CH * S);
	}

	// ---- a patente: o cubo na emenda do painel com o cartão (o desenho da patente da facção dentro) e o tubo dos 5 degraus
	if (State.Stars >= 1)
	{
		const int32 Rank = FMath::Clamp(State.Stars, 1, 5);
		const float HX0 = bNone ? 1470.0f : 1486.0f, HY0 = bNone ? 806.0f : 840.0f + (PTop - 868.0f);
		const auto HY = [&](float V) { return SY(V, true) + Drop; };
		const FBox2D HB = FacConsole(Cur->SkinFaction).Hub;
		// o tubo (a peça de metal com a janela; os 5 degraus de baixo pra cima: os feitos cheios de ouro, o atual enchendo com o XP)
		const float TX = SX(HX0 + 9.0f, EAnchor::Right), TY = HY(HY0 + (bNone ? 50.0f : 56.0f)), TW = 34.0f * S, TH = (bNone ? 196.0f : 166.0f) * S;
		Skin(FacSkin(TEXT("rank_tube")), TX, TY, TW, TH, FMargin(0.0f, 45.0f, 0.0f, 54.0f), 0.2f * S);
		const float Gap = 2.0f * S, InX = TX + 6.6f * S, InW = 20.8f * S, InTop = TY + 10.0f * S, InH = TH - 22.0f * S;
		const float SegH = (InH - 4.0f * Gap) / 5.0f;
		const TPair<float, FLinearColor> Gold[3] = { { 0.0f, Hex(0x8d5f12) }, { 0.5f, Hex(0xffe39a) }, { 1.0f, Hex(0xc58e25) } };
		for (int32 K = 0; K < 5; ++K)
		{
			const float SY0 = InTop + InH - SegH - K * (SegH + Gap);
			const bool bDone = K < Rank - 1, bCurSeg = K == Rank - 1;
			Rect(InX, SY0, InW, SegH, Hex(0xf6c850, 0.06f));
			Border(InX, SY0, InW, SegH, FMath::Max(1.0f, S), bCurSeg ? Hex(0xffe08a, 0.7f) : Hex(0xf6c850, 0.28f));
			const float Fill = bDone ? 1.0f : (bCurSeg ? (Rank >= 5 ? 1.0f : FMath::Clamp(State.Xp, 0.0f, 1.0f)) : 0.0f);
			if (Fill > 0.0f)
			{
				const float FH = SegH * Fill;
				FillPoly(ChamferPoly(InX - 2 * S, SY0 + SegH - FH - 2 * S, InW + 4 * S, FH + 4 * S, 2 * S), [](const FVector2D&) { return Hex(0xf6c850, 0.14f); });
				const TPair<float, FLinearColor> G2[3] = { { 0.0f, Gold[0].Value * FLinearColor(1, 1, 1, bDone ? 0.92f : 0.85f) },
					{ 0.5f, Gold[1].Value * FLinearColor(1, 1, 1, bDone ? 0.92f : 0.85f) }, { 1.0f, Gold[2].Value * FLinearColor(1, 1, 1, bDone ? 0.92f : 0.85f) } };
				RoundRect(InX, SY0 + SegH - FH, InW, FH, 0.0f, G2);
			}
			if (bCurSeg)
			{
				// a seta "você está aqui", à esquerda do degrau atual
				const float AX = TX - 12.0f * S, AY = SY0 + SegH * 0.5f;
				FillCircle(FVector2D(AX + 4 * S, AY), 6 * S, Hex(0xffc83c, 0.35f), Hex(0xffc83c, 0.0f), 16);
				Tris({ FVector2D(AX, AY - 6 * S), FVector2D(AX + 8 * S, AY), FVector2D(AX, AY + 6 * S) }, Hex(0xf6c850));
			}
		}
		Bg(TX, TY, TW, TH);
		// o cubo por cima do tubo: a peça da facção e o desenho da patente dentro
		const float BX = SX(HX0, EAnchor::Right), BY = HY(HY0);
		Image(FacSkin(TEXT("hub_general")), BX + HB.Min.X * S, BY + HB.Min.Y * S, (HB.Max.X - HB.Min.X) * S, (HB.Max.Y - HB.Min.Y) * S, FLinearColor::White, false);
		const FString Fac = Cur->SkinFaction.IsNone() ? FString(TEXT("usa")) : Cur->SkinFaction.ToString();
		const FName RankIcon = Fac == TEXT("usa") ? FName(*FString::Printf(TEXT("skin:rank_usa_%d"), Rank)) : FName(*FString::Printf(TEXT("skin:%s/rank_%s_%d"), *Fac, *Fac, Rank));
		Image(RankIcon, BX + 4.7f * S, BY + 4.3f * S, 42.6f * S, 43.5f * S, FLinearColor::White, false);
		HoverPress(TEXT("promote"), BX, BY, 52.0f * S, 52.0f * S, Hex(0x49aaff));
		Buttons->Add({ TEXT("promote"), FBox2D(FVector2D(BX, BY), FVector2D(BX + 52.0f * S, BY + 52.0f * S)) });
	}

	// ---- as plaquetas (01/11, aprovadas: "top"): nascem com a mola (achatadas, esticam passando um pouco e assentam) e ficam
	auto Spring = [](float Age) { const float T = FMath::Clamp(Age / 0.45f, 0.0f, 1.0f); return 1.0f + FMath::Sin(T * PI * 1.5f) * (1.0f - T) * 0.25f - (1.0f - T) * 0.65f * (1.0f - T); };
	if (!State.PlateTitle.IsEmpty())
	{
		// a vermelha em cima do brasão: o triângulo do aviso, o título espaçado e a linha do que falta
		const float K = FMath::Clamp(Spring(State.PlateAge), 0.2f, 1.2f), A = FMath::Clamp(State.PlateAge / 0.15f, 0.0f, 1.0f);
		const float TW = MeasureSpaced(State.PlateTitle, 18.0f, EWeight::Bold, 0.6f), LW = Measure(State.PlateLine, 15.0f, EWeight::SemiBold).X;
		const float W = 16.0f * S + 22.0f * S + 9.0f * S + TW + (LW > 0.0f ? 9.0f * S + LW : 0.0f) + 16.0f * S, H = 34.0f * S * K;
		const float X = SX(960.0f, EAnchor::Center) - W * 0.5f, PY = Y(747.0f) + 34.0f * S * (1.0f - K) * 0.5f;
		FillPoly(ChamferPoly(X - 8 * S, PY - 8 * S, W + 16 * S, H + 16 * S, 12 * S), [A](const FVector2D&) { return Hex(0xff3b2c, 0.14f * A); });
		FillPoly(ChamferPoly(X - 2 * S, PY - 2 * S, W + 4 * S, H + 4 * S, 5 * S), [A](const FVector2D&) { return Hex(0xa8281d, A); });
		FillPoly(ChamferPoly(X - 1 * S, PY - 1 * S, W + 2 * S, H + 2 * S, 4.5f * S), [A](const FVector2D&) { return FLinearColor(0, 0, 0, A); });
		FillPoly(ChamferPoly(X, PY, W, H, 4 * S), [&](const FVector2D& P) { return FMath::Lerp(Hex(0x3a0b08, A), Hex(0x1b0605, A), FMath::Clamp((P.Y - PY) / FMath::Max(1.0f, H), 0.0f, 1.0f)); });
		if (K > 0.7f)
		{
			const float CY = PY + H * 0.5f;
			Image(TEXT("skin:alert_icon_red"), X + 16.0f * S, CY - 9.5f * S, 22.0f * S, 19.0f * S, FLinearColor(1, 1, 1, A), false);
			const float TX = X + 47.0f * S;
			TextSpaced(State.PlateTitle, TX, CY, 18.0f, Hex(0xff7b66, A), EWeight::Bold, 0.6f, EAlign::Left, true);
			TextMid(State.PlateLine, TX + TW + 9.0f * S, CY + 1.0f * S, 15.0f, Hex(0xff7b66, A), EWeight::SemiBold);
		}
	}
	if (!State.PromotedRank.IsEmpty() && State.Stars >= 1)
	{
		// a dourada em cima do cubo: a ficha de ouro com a estrela, PROMOTED espaçado e o nome da patente nova
		const float K = FMath::Clamp(Spring(State.PromotedAge), 0.2f, 1.2f), A = FMath::Clamp(State.PromotedAge / 0.15f, 0.0f, 1.0f);
		const float PW = MeasureSpaced(TEXT("PROMOTED"), 14.0f, EWeight::Bold, 2.5f), RW = Measure(State.PromotedRank, 14.0f, EWeight::SemiBold).X;
		const float W = 8.0f * S + 30.0f * S + 9.0f * S + PW + 12.0f * S + RW + 18.0f * S, H = 32.0f * S * K;
		const float HX0 = bNone ? 1470.0f : 1486.0f, HY0 = bNone ? 806.0f : 840.0f + (PTop - 868.0f);
		const float X = SX(HX0 + 26.0f, EAnchor::Right) - W * 0.5f, PY = SY(HY0 - 44.0f, true) + Drop + 32.0f * S * (1.0f - K) * 0.5f;
		FillPoly(ChamferPoly(X - 7 * S, PY - 7 * S, W + 14 * S, H + 14 * S, 11 * S), [A](const FVector2D&) { return Hex(0xffc83c, 0.12f * A); });
		FillPoly(ChamferPoly(X - 2 * S, PY - 2 * S, W + 4 * S, H + 4 * S, 5 * S), [A](const FVector2D&) { return Hex(0xc9971f, A); });
		FillPoly(ChamferPoly(X - 1 * S, PY - 1 * S, W + 2 * S, H + 2 * S, 4.5f * S), [A](const FVector2D&) { return FLinearColor(0, 0, 0, A); });
		FillPoly(ChamferPoly(X, PY, W, H, 4 * S), [&](const FVector2D& P) { return FMath::Lerp(Hex(0x231a07, A), Hex(0x0f0b04, A), FMath::Clamp((P.Y - PY) / FMath::Max(1.0f, H), 0.0f, 1.0f)); });
		Rect(X + 4 * S, PY + 1 * S, W - 8 * S, FMath::Max(1.0f, S), Hex(0xffe696, 0.22f * A));
		if (K > 0.7f)
		{
			const float CY = PY + H * 0.5f;
			Image(TEXT("skin:token_gold"), X + 8.0f * S, CY - 13.5f * S, 30.0f * S, 27.0f * S, FLinearColor(1, 1, 1, A), false);
			Star(FVector2D(X + 23.0f * S, CY), 6.0f * S, Hex(0x2a1d05, A));
			const float TX = X + 47.0f * S;
			TextSpaced(TEXT("PROMOTED"), TX, CY, 14.0f, Hex(0xf6c850, A), EWeight::Bold, 2.5f, EAlign::Left, true);
			TextMid(State.PromotedRank, TX + PW + 12.0f * S, CY, 14.0f, Hex(0xe6ecf3, A), EWeight::SemiBold);
		}
	}
}
