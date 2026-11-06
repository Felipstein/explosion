#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"

// O chat da partida (01/11, aprovado: "o resto tá aprovado"; DECISOES §7; o protótipo docs/concept/hud/prototipo/index.html, #chat): as
// linhas perto do minimapa (left 112, bottom 336, 600 de largura), empilhando pra cima; cada uma numa placa escura que some pra direita,
// com a cor do jogador na borda, letra de 17 px, o nome na cor dele, ALLIES num selo verde, o sistema em cinza; entram com o tranco das
// plaquetas (clIn: deslizam 26 px da esquerda acendendo 2,2×) e somem aos 12 s (1,2 s, recuando 14 px). No máximo 6. Enter abre a caixa
// com as abas ALL / ALLIES e a dica das teclas. Não segura o clique (pointer-events: none no protótipo). O ping do botão: DrawMinimap.

namespace
{
	FLinearColor Hex(uint32 RGB, float A = 1.0f)
	{
		FLinearColor C(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255));
		C.A = A;
		return C;
	}

	FLinearColor Fade(FLinearColor C, float A) { C.A *= A; return C; }

	/** O clIn do protótipo no tempo T (0..1) com a curva da animação: o quanto já entrou (passa de 1 na mola) e o brilho (2,2 → 1,3 aos 60% → 1). */
	void ClIn(float T, float X1, float Y1, float X2, float Y2, float& Move, float& Bright)
	{
		Move = FRTSHudPainter::Bezier(X1, Y1, X2, Y2, T);
		Bright = T < 0.6f ? FMath::Lerp(2.2f, 1.3f, FRTSHudPainter::Bezier(X1, Y1, X2, Y2, T / 0.6f))
			: FMath::Lerp(1.3f, 1.0f, FRTSHudPainter::Bezier(X1, Y1, X2, Y2, (T - 0.6f) / 0.4f));
	}

	FLinearColor Lit(const FLinearColor& C, float Bright, float A)
	{
		return FLinearColor(FMath::Min(1.0f, C.R * Bright), FMath::Min(1.0f, C.G * Bright), FMath::Min(1.0f, C.B * Bright), C.A * A);
	}
}

void FRTSHudPainter::DrawChat(const FRTSHudState& State)
{
	const FRTSHudChat& Ch = State.Chat;
	const float BoxX = SX(112.0f, EAnchor::Left), MaxW = 600.0f * S;
	float Y = SY(744.0f, true);
	constexpr float Life = 12.0f, OutDur = 1.2f;

	// a caixa de digitar (embaixo das linhas; o order 99): entra com o clIn em 0,25 s (ease), o aro de 1 px preto + 1 px #3a4757 e a
	// luz azul de 16 px; as abas, o texto com o cursor e a dica à direita
	if (Ch.bTyping)
	{
		float Move, Bright;
		ClIn(FMath::Clamp(Ch.TypingAge / 0.25f, 0.0f, 1.0f), 0.25f, 0.1f, 0.25f, 1.0f, Move, Bright);
		const float A = FMath::Clamp(Move, 0.0f, 1.0f);
		const float H = 36.0f * S, BY = Y - H, BX = BoxX - (1.0f - Move) * 26.0f * S, R = 4.0f * S;
		const FLinearColor Blue(FColor(73, 170, 255));
		for (int32 L = 1; L <= 4; ++L)
		{
			// o box-shadow 0 0 16px: a luz cai até sumir a 16 px (4 anéis de 4 px)
			const float G = L * 4.0f * S;
			RoundRectRing(BX - 2 * S - G, BY - 2 * S - G, MaxW + 4 * S + 2 * G, H + 4 * S + 2 * G, R + 2 * S + G, 4.0f * S,
				FLinearColor(Blue.R, Blue.G, Blue.B, 0.25f * A * FMath::Square(1.0f - (L - 0.5f) / 4.0f)));
		}
		RoundRect(BX - 2 * S, BY - 2 * S, MaxW + 4 * S, H + 4 * S, R + 2 * S, Lit(Hex(0x3a4757), Bright, A));
		RoundRect(BX - 1 * S, BY - 1 * S, MaxW + 2 * S, H + 2 * S, R + 1 * S, Fade(Hex(0x000000), A));
		const FLinearColor BodyTop = Lit(Hex(0x0f1720), Bright, A), BodyBot = Lit(Hex(0x0a0f15), Bright, A);
		FillPoly(ChamferPoly(BX, BY, MaxW, H, R * 0.6f), [=](const FVector2D& P) { return FMath::Lerp(BodyTop, BodyBot, FMath::Clamp((P.Y - BY) / H, 0.0f, 1.0f)); });
		float Pen = BX;
		for (int32 K = 0; K < 2; ++K)
		{
			const bool bAllies = K == 1, bOn = bAllies == Ch.bAllies;
			const FString Tab = bAllies ? TEXT("ALLIES") : TEXT("ALL");
			const float TW = MeasureSpaced(Tab, 14.0f, EWeight::Bold, 1.0f) + 24.0f * S;
			if (bOn)
			{
				// a aba escolhida: o degradê azul (todos) ou verde (aliados); a primeira acompanha o canto da caixa
				const FLinearColor Top = Lit(bAllies ? Hex(0x1f6b3a) : Hex(0x1d4f80), Bright, A), Bot = Lit(bAllies ? Hex(0x154c29) : Hex(0x143a5f), Bright, A);
				const float C = K == 0 ? R * 0.6f : 0.0f;
				FillPoly({ { Pen + C, BY }, { Pen + TW, BY }, { Pen + TW, BY + H }, { Pen + C, BY + H }, { Pen, BY + H - C }, { Pen, BY + C } },
					[=](const FVector2D& P) { return FMath::Lerp(Top, Bot, FMath::Clamp((P.Y - BY) / H, 0.0f, 1.0f)); });
			}
			TextSpaced(Tab, Pen + 12.0f * S, BY + H * 0.5f, 14.0f, Fade(bOn ? FLinearColor::White : Hex(0x6f7b88), A), EWeight::Bold, 1.0f);
			Pen += TW;
			Rect(Pen, BY, FMath::Max(1.0f, S), H, Fade(Hex(0x222c37), A));
			Pen += FMath::Max(1.0f, S);
		}
		const FString Hint = TEXT("Enter send · Tab all/allies · Esc cancel");
		const float HintW = Measure(Hint, 13.0f, EWeight::Medium).X;
		TextMid(Hint, BX + MaxW - 12.0f * S, BY + H * 0.5f, 13.0f, Fade(Hex(0x6f7b88), A), EWeight::Medium, EAlign::Right, false);
		const float TxtX = Pen + 12.0f * S, TxtW = BX + MaxW - 24.0f * S - HintW - TxtX;
		// o texto mais longo que a caixa: mostra o fim (o que está sendo digitado)
		FString Draft = Ch.Draft;
		while (!Draft.IsEmpty() && Measure(Draft, 17.0f, EWeight::Medium).X > TxtW - 4.0f * S)
		{
			Draft.RightChopInline(1);
		}
		const float DW = Draft.IsEmpty() ? 0.0f : TextMid(Draft, TxtX, BY + H * 0.5f, 17.0f, Fade(Hex(0xeef2f6), A), EWeight::Medium, EAlign::Left, false).X;
		// o cursor: o blink 1 s steps(2) do protótipo (1 → 0,58 → 0,15 → 0,58, 0,25 s cada)
		static const float Blink[4] = { 1.0f, 0.575f, 0.15f, 0.575f };
		const float CA = Blink[int32(FMath::Fmod(Ch.TypingAge, 1.0f) * 4.0f) & 3];
		Rect(TxtX + DW + 1.0f * S, BY + H * 0.5f - 7.5f * S, 2.0f * S, 18.0f * S, Fade(Hex(0xcfe9ff), A * CA));
		Y = BY - 2.0f * S - 4.0f * S - 5.0f * S;   // o aro, o margin-top 4 e o gap 5
	}

	// as linhas, a mais nova embaixo; no máximo as 6 últimas (o protótipo apaga a mais velha). Com a caixa aberta, as recentes voltam
	// (como no StarCraft II): quem vai responder lê a conversa
	int32 Shown = 0;
	for (int32 I = Ch.Lines.Num() - 1; I >= 0 && Shown < 6; --I)
	{
		const FRTSHudChatLine& L = Ch.Lines[I];
		if (!Ch.bTyping && L.Age > Life + OutDur)
		{
			continue;
		}
		++Shown;
		float Move, Bright;
		ClIn(FMath::Clamp(L.Age / 0.3f, 0.0f, 1.0f), 0.2f, 1.2f, 0.4f, 1.0f, Move, Bright);
		const float Out = Ch.bTyping || L.Age < Life ? 0.0f : Bezier(0.25f, 0.1f, 0.25f, 1.0f, (L.Age - Life) / OutDur);
		const float A = FMath::Clamp(Move, 0.0f, 1.0f) * (1.0f - Out);
		const float Dx = -(1.0f - Move) * 26.0f * S - Out * 14.0f * S;
		const FLinearColor Pc = L.bSystem ? Hex(0x6b7684) : L.Color;
		// o conteúdo (flex, gap 8): [ALLIES] Nome: texto; o sistema, o texto só, em cinza
		const float Gap = 8.0f * S;
		const float TagW = L.bAllies ? MeasureSpaced(TEXT("ALLIES"), 12.0f, EWeight::Bold, 1.0f) + 10.0f * S : 0.0f;
		const FString Who = L.bSystem || L.Who.IsEmpty() ? FString() : L.Who + TEXT(":");
		const float WhoW = Who.IsEmpty() ? 0.0f : Measure(Who, 17.0f, EWeight::Bold).X;
		const float Lead = (TagW > 0 ? TagW + Gap : 0.0f) + (WhoW > 0 ? WhoW + Gap : 0.0f);
		const FString Text = Ellipsize(L.Text, 17.0f, EWeight::Medium, MaxW - 35.0f * S - Lead);
		const float TextW = Measure(Text, 17.0f, EWeight::Medium).X;
		const float W = 13.0f * S + Lead + TextW + 22.0f * S;
		const float H = 31.0f * S, PX = BoxX + Dx, PY = Y - H;
		// a placa: escura à esquerda, some pra direita (o 70% ainda forte), a borda de 3 px na cor do jogador
		const TPair<float, FLinearColor> Plate[3] = { { 0.0f, Hex(0x060a0e, 0.92f * A) }, { 0.7f, Hex(0x060a0e, 0.8f * A) }, { 1.0f, Hex(0x060a0e, 0.0f) } };
		RoundRect(PX, PY, W, H, 3.0f * S, Plate);
		Rect(PX, PY, 3.0f * S, H, Lit(Pc, Bright, A));
		const float CY = PY + H * 0.5f;
		float Pen = PX + 13.0f * S;
		if (TagW > 0)
		{
			RoundRectRing(Pen, CY - 8.0f * S, TagW, 16.0f * S, 2.0f * S, FMath::Max(1.0f, S), Fade(Hex(0x7fe39a, 0.5f), A));
			TextSpaced(TEXT("ALLIES"), Pen + 5.0f * S, CY, 12.0f, Lit(Hex(0x7fe39a), Bright, A), EWeight::Bold, 1.0f, EAlign::Left, true);
			Pen += TagW + Gap;
		}
		if (WhoW > 0)
		{
			TextMid(Who, Pen, CY, 17.0f, Lit(Pc, Bright, A), EWeight::Bold);
			Pen += WhoW + Gap;
		}
		TextMid(Text, Pen, CY, 17.0f, Lit(L.bSystem ? Hex(0xaab4c0) : Hex(0xeef2f6), Bright, A), EWeight::Medium);
		Y = PY - 5.0f * S;
	}
}
