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
		FVector2D Channel; // o canal da espinha de energia, de borda a borda do contorno de dentro (em px dos 24 da espinha), lido na régua da arte ampliada
		FVector2D ChannelX; // onde o canal começa e termina, a partir das pontas da espinha (px da planta: a arte × 22/72)
	};

	FFacConsole FacConsole(FName F)
	{
		auto B = [](float X, float Y, float W, float H) { return FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)); };
		if (F == TEXT("china")) return { B(880.0f, 799.8f, 160.0f, 44.6f), B(-1.2f, -2.2f, 55.1f, 61.8f), FVector2D(5.7f, 17.2f), FVector2D(7.3f, 7.9f) };
		if (F == TEXT("gla")) return { B(877.5f, 804.2f, 165.0f, 42.5f), B(-1.6f, -1.3f, 61.3f, 63.5f), FVector2D(6.6f, 16.8f), FVector2D(20.2f, 20.8f) };
		if (F == TEXT("russia")) return { B(878.8f, 804.4f, 162.5f, 39.4f), B(-4.7f, 3.4f, 60.3f, 50.2f), FVector2D(6.7f, 17.3f), FVector2D(20.8f, 20.8f) };
		return { B(881.0f, 805.7f, 158.0f, 40.2f), B(0.0f, 0.1f, 52.0f, 57.6f), FVector2D(6.5f, 16.5f), FVector2D(20.2f, 20.2f) };
	}
}

namespace
{
	/** Uma animação de CSS em quadros-chave: os tempos (0..1) e os valores, com a curva "ease" em cada trecho (o padrão do CSS). */
	float Keyframes(float T, std::initializer_list<TPair<float, float>> Keys)
	{
		const TPair<float, float>* Prev = Keys.begin();
		for (const TPair<float, float>& K : Keys)
		{
			if (T <= K.Key)
			{
				return K.Key <= Prev->Key ? K.Value : FMath::Lerp(Prev->Value, K.Value, FRTSHudPainter::Bezier(0.25f, 0.1f, 0.25f, 1.0f, (T - Prev->Key) / (K.Key - Prev->Key)));
			}
			Prev = &K;
		}
		return Prev->Value;
	}

	/** O ponto da curva cúbica P0..P3 no parâmetro U. */
	FVector2D Cubic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float U)
	{
		const float V = 1.0f - U;
		return P0 * (V * V * V) + P1 * (3.0f * V * V * U) + P2 * (3.0f * V * U * U) + P3 * (U * U * U);
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

	// ---- (05/11) sem energia (o "brown" do protótipo): só o HUD falha, nunca a batalha. Os pedaços do protótipo (#dimPod, #dimPod2,
	// #dimPanel, #dimCard) escurecem com o backdrop brightness(.42) (aqui o preto a 58%; o saturate(.35) do CSS não tem como no Canvas, que
	// não lê o que está embaixo) na animação brown de 12 s: o tranco forte quando cai (1 → .1 → 1 → .25 → 1), firme em .7, e o tranquinho de
	// lembrete no fim de cada volta (.15 → .8)
	const bool bBrown = State.BrownAge >= 0.0f;
	const float BrownT = bBrown ? FMath::Fmod(State.BrownAge, 12.0f) / 12.0f : 0.0f;
	if (bBrown)
	{
		const float Op = Keyframes(BrownT, { { 0.0f, 1.0f }, { 0.02f, 0.1f }, { 0.04f, 1.0f }, { 0.07f, 0.25f }, { 0.09f, 1.0f }, { 0.12f, 0.7f },
			{ 0.97f, 0.7f }, { 0.98f, 0.15f }, { 0.99f, 0.8f }, { 1.0f, 0.7f } });
		const FLinearColor Dark(0.0f, 0.0f, 0.0f, 0.58f * Op);
		Rect(SX(0.0f, EAnchor::Left), SY(806.0f, true), 388.0f * S, 274.0f * S, Dark);
		Rect(SX(100.0f, EAnchor::Left), SY(742.0f, true), 288.0f * S, 64.0f * S, Dark);
		if (!bNone)
		{
			// o painel acompanha o topo dele (o de 9 colunas é mais alto) e a subida; o cartão sobe junto
			Rect(SX(383.0f, EAnchor::Center), SY(PTop + 6.0f, true) + Drop, 1135.0f * S, (1074.0f - PTop) * S, Dark);
			Rect(SX(1515.0f, EAnchor::Right), SY(758.0f, true) + Drop, 405.0f * S, 322.0f * S, Dark);
		}
		else
		{
			Rect(SX(1515.0f, EAnchor::Right), SY(830.0f, true), 405.0f * S, 250.0f * S, Dark);
		}
	}
	// as células do tubo piscam com o tranco (a animação tube, a mesma volta de 12 s)
	const float TubeA = bBrown ? Keyframes(BrownT, { { 0.0f, 1.0f }, { 0.02f, 0.1f }, { 0.04f, 1.0f }, { 0.07f, 0.25f }, { 0.09f, 1.0f }, { 1.0f, 1.0f } }) : 1.0f;

	// ---- a espinha de energia (a GLA sem usina nenhuma: a barra some)
	const int32 Used = State.PowerUsed, Made = State.PowerMade;
	const bool bNoPower = Cur->SkinFaction == FName(TEXT("gla")) && Used == 0 && Made == 0;
	if (!bNoPower)
	{
		// sem seleção a espinha termina antes do tubo da patente (04/11, ele: "muito mal encaixado a barra da energia com a barra de xp")
		const float SXL = SX(392.0f, EAnchor::Left), SXR = SX(bNone ? 1469.0f : 1492.0f, EAnchor::Right);
		Skin(TEXT("skin:power_conduit"), SXL, Y(854.0f), SXR - SXL, 24.0f * S, FMargin(72.0f, 0.0f), 22.0f / 72.0f * S);
		const bool bOver = Used > Made, bNear = !bOver && Made - Used < 3;
		const int32 N = FMath::Max(Made, Used);
		// (04/11, ele: "os bloquinhos de energia são um pouco maiores do que a barra") as células dentro do canal escuro da arte de cada
		// facção, com 1 px de folga em cima e embaixo
		const FVector2D Ch = FacConsole(Cur->SkinFaction).Channel, ChX = FacConsole(Cur->SkinFaction).ChannelX;
		const float CX0 = SXL + (ChX.X + 2.0f) * S, CW0 = SXR - SXL - (ChX.X + ChX.Y + 4.0f) * S, CY = Y(854.0f + Ch.X + 1.0f), CH = (Ch.Y - Ch.X - 2.0f) * S, Gap = 4.0f * S;
		const float CellW = N > 0 ? FMath::Min(44.0f * S, (CW0 - Gap * (N - 1)) / N) : 0.0f;
		// as cores do protótipo: o verde normal, o âmbar quase sem, o vermelho faltando (DECISOES §9)
		const FLinearColor UTop = bOver ? Hex(0xff9e8f, TubeA) : bNear ? Hex(0xffe0a8) : Hex(0xc9f7cf);
		const FLinearColor UMid = bOver ? Hex(0xc8281b, TubeA) : bNear ? Hex(0xffab2e) : Hex(0x4fd36a);
		const FLinearColor UBot = bOver ? Hex(0x6a0e08, TubeA) : bNear ? Hex(0xb06a0c) : Hex(0x1f8a34);
		const float Blink = (FMath::Fmod(State.Time, 1.0f) < 0.5f) ? 1.0f : 0.35f;
		for (int32 I = 0; I < N; ++I)
		{
			const float X = CX0 + I * (CellW + Gap);
			if (I < FMath::Min(Used, Made))
			{
				// aceso: o degradê de cima pra baixo e o brilho em volta
				Rect(X - 2 * S, CY, CellW + 4 * S, CH, FLinearColor(UMid.R, UMid.G, UMid.B, 0.16f * UMid.A));
				FillPoly({ { X, CY }, { X + CellW, CY }, { X + CellW, CY + CH * 0.4f }, { X, CY + CH * 0.4f } }, [&](const FVector2D& P) { return FMath::Lerp(UTop, UMid, FMath::Clamp((P.Y - CY) / (CH * 0.4f), 0.0f, 1.0f)); });
				FillPoly({ { X, CY + CH * 0.4f }, { X + CellW, CY + CH * 0.4f }, { X + CellW, CY + CH }, { X, CY + CH } }, [&](const FVector2D& P) { return FMath::Lerp(UMid, UBot, FMath::Clamp((P.Y - CY - CH * 0.4f) / (CH * 0.6f), 0.0f, 1.0f)); });
			}
			else if (I >= Made && I < Used)
			{
				// o que falta: vermelho pulsando depois do fim da produção
				const float A = (0.45f + 0.55f * Pulse(0.83f)) * TubeA;
				Rect(X - 2 * S, CY, CellW + 4 * S, CH, Hex(0xff3b2c, 0.3f * A));
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
			// só por cima das células (nunca no vão), e acende e apaga devagar no começo e no fim da corrida: nada termina num corte seco
			const float GX = CX0 - 100.0f * S + (Lit + 100.0f * S) * T, GW = 90.0f * S;
			const float Env = FMath::Sin(PI * T);
			const TPair<float, FLinearColor> Stops[3] = { { 0.0f, FLinearColor(1, 1, 1, 0) }, { 0.5f, FLinearColor(1, 1, 1, 0.55f * Env) }, { 1.0f, FLinearColor(1, 1, 1, 0) } };
			for (int32 I = 0; I < FMath::Min(Used, Made); ++I)
			{
				const float X = CX0 + I * (CellW + Gap);
				RoundRect(GX, CY, GW, CH, 0.0f, Stops, X, X + CellW);
			}
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
		// o hover aprovado das abas: sobe 3 px com a mola, clareia (brightness 1.3; a lâmpada na GLA), a sombra embaixo e o halo na luz
		// da facção; apertada desce 1 px e escurece
		const bool bHot = Cur->HoverId == GId, bDown = Cur->PressedId == GId;
		const float K = bHot ? FMath::Clamp(Cur->HoverAge / 0.13f, 0.0f, 1.0f) : 0.0f;
		const float Lift = bDown ? 1.0f * S : -3.0f * S * K;
		if (bHot && !bDown)
		{
			const FLinearColor Lt = FactionLight();
			for (int32 L = 1; L <= 3; ++L)
			{
				const float Gr = L * 2.0f * S;
				RoundRect(X - Gr, TY + Lift + 6.0f * S - Gr * 0.5f, W + 2 * Gr, H + Gr, 6.0f * S + Gr, FLinearColor(0, 0, 0, 0.12f * K));
				RoundRectRing(X - Gr, TY + Lift - Gr, W + 2 * Gr, H + 2 * Gr, 6.0f * S + Gr, 2.0f * S, FLinearColor(Lt.R, Lt.G, Lt.B, 0.55f * K * (0.5f - L * 0.13f)));
			}
			float B = FMath::Lerp(1.0f, 1.3f, K);
			if (Cur->SkinFaction == FName(TEXT("gla")))
			{
				static const float Steps[4] = { 1.32f, 0.95f, 1.28f, 1.05f };
				const int32 Step = FMath::FloorToInt(Cur->HoverAge / 0.08f);
				B = Step < 4 ? Steps[Step] * 1.1f : 1.3f;
			}
			DrawTint = FLinearColor(B, B, B, 1.0f);
		}
		else if (bDown)
		{
			DrawTint = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
		}
		Image(FacSkin(bOn ? TEXT("tab_on") : TEXT("tab_off")), X, TY + Lift, W, H, FLinearColor::White, false);
		DrawTint = FLinearColor::White;
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
		// (05/11) clicou sem dinheiro: o brasão treme (o shake do protótipo: -7, 6, -4, 2 px em 0,45 s, a curva "ease" em cada trecho)
		float Shake = 0.0f;
		if (State.CrestShakeAge >= 0.0f && State.CrestShakeAge < 0.45f)
		{
			static const float Keys[6] = { 0.0f, -7.0f, 6.0f, -4.0f, 2.0f, 0.0f };
			const float T = State.CrestShakeAge / 0.45f * 5.0f;
			const int32 Seg = FMath::Min(4, int32(T));
			Shake = FMath::Lerp(Keys[Seg], Keys[Seg + 1], Bezier(0.25f, 0.1f, 0.25f, 1.0f, T - Seg)) * S;
		}
		const float CX = SX(835.0f, EAnchor::Center) + Shake, CYt = Y(860.0f - CH);
		Image(Crest, CX, CYt, CW * S, CH * S, FLinearColor::White, false);
		const FBox2D O = FacConsole(Cur->SkinFaction).Odo;
		const float OX = SX(O.Min.X, EAnchor::Center) + Shake, OY = Y(O.Min.Y), OW = (O.Max.X - O.Min.X) * S, OH = (O.Max.Y - O.Min.Y) * S;
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
		// (05/11) o dinheiro voando (o .chip do protótipo; a sombra 0 2px 3px #000, 0 0 8px #000 em 8 cópias pretas em volta)
		const auto ChipText = [&](const FString& T, float X, float CYc, float Px, const FLinearColor& C)
		{
			const float Off = 2.0f * S * Px / 24.0f;
			for (int32 K = 0; K < 8; ++K)
			{
				const float Ang = K * PI / 4.0f;
				TextMid(T, X + FMath::Cos(Ang) * Off, CYc + FMath::Sin(Ang) * Off + 1.0f * S, Px, FLinearColor(0, 0, 0, 0.32f * C.A), EWeight::Bold, EAlign::Center, false);
			}
			TextMid(T, X, CYc + 2.0f * S, Px, FLinearColor(0, 0, 0, 0.6f * C.A), EWeight::Bold, EAlign::Center, false);
			TextMid(T, X, CYc, Px, C, EWeight::Bold, EAlign::Center, false);
		};
		for (const FRTSHudMoneyChip& Chip : State.MoneyChips)
		{
			if (Chip.bIn && Chip.Age < 1.3f)
			{
				// entra: voa pela curva path('M 0 0 C -60 120, -200 280, -332 382') a partir de (1300, 440) até o meio do mostrador, em 1,3 s
				// cubic-bezier(.5,0,.6,1) na distância andada; aparece até 12%, cresce de 1,2 a 1 até 80% e encolhe a 0,6 sumindo no fim
				const float T = Chip.Age / 1.3f;
				const auto Ease = [](float V) { return Bezier(0.5f, 0.0f, 0.6f, 1.0f, V); };
				const float Dist = Ease(T);
				const FVector2D P0(0, 0), P1(-60, 120), P2(-200, 280), P3(-332, 382);
				// a distância andada é pelo comprimento da curva (o offset-distance do CSS), não pelo parâmetro
				float Len[33];
				Len[0] = 0.0f;
				for (int32 K = 1; K <= 32; ++K)
				{
					Len[K] = Len[K - 1] + FVector2D::Distance(Cubic(P0, P1, P2, P3, (K - 1) / 32.0f), Cubic(P0, P1, P2, P3, K / 32.0f));
				}
				const float Want = Dist * Len[32];
				int32 Seg = 1;
				while (Seg < 32 && Len[Seg] < Want) ++Seg;
				const float U = ((Seg - 1) + (Len[Seg] > Len[Seg - 1] ? (Want - Len[Seg - 1]) / (Len[Seg] - Len[Seg - 1]) : 0.0f)) / 32.0f;
				const FVector2D P = Cubic(P0, P1, P2, P3, FMath::Clamp(U, 0.0f, 1.0f));
				const float A = T < 0.12f ? Ease(T / 0.12f) : T < 0.8f ? 1.0f : 1.0f - Ease((T - 0.8f) / 0.2f);
				const float Sc = T < 0.8f ? FMath::Lerp(1.2f, 1.0f, Ease(T / 0.8f)) : FMath::Lerp(1.0f, 0.6f, Ease((T - 0.8f) / 0.2f));
				ChipText(Chip.Text, SX(1300.0f + P.X, EAnchor::Center), Y(440.0f + P.Y), 28.0f * Sc, Hex(0x6fe08a, A));
			}
			else if (!Chip.bIn && Chip.Age < 1.1f)
			{
				// sai: de (1050, 818) recua 90 e cai 40, apagando, em 1,1 s ease-in
				const float E = Bezier(0.42f, 0.0f, 1.0f, 1.0f, Chip.Age / 1.1f);
				const float W = Measure(Chip.Text, 24.0f, EWeight::Bold).X;
				ChipText(Chip.Text, SX(1050.0f - 90.0f * E, EAnchor::Center) + W * 0.5f, Y(818.0f + 14.0f + 40.0f * E), 24.0f, Hex(0xff6a55, 1.0f - E));
			}
		}
	}

	// ---- a patente: o cubo na emenda do painel com o cartão (o desenho da patente da facção dentro) e o tubo dos 5 degraus
	if (State.Stars >= 1)
	{
		const int32 Rank = FMath::Clamp(State.Stars, 1, 5);
		const float HX0 = bNone ? 1470.0f : 1486.0f, HY0 = bNone ? 806.0f : 840.0f + (PTop - 868.0f);
		const auto HY = [&](float V) { return SY(V, true) + Drop; };
		const FBox2D HB = FacConsole(Cur->SkinFaction).Hub;
		// o tubo (a peça de metal com a janela; os 5 degraus de baixo pra cima: os feitos cheios de ouro, o atual enchendo com o XP)
		// com seleção o tubo termina 10 acima da borda de baixo do painel (o de 9 colunas é mais baixo: o tubo encolhe junto)
		const float TX = SX(HX0 + 9.0f, EAnchor::Right), TY = HY(HY0 + (bNone ? 50.0f : 56.0f)), TW = 34.0f * S;
		const float TH = (bNone ? 221.0f : 166.0f - (PTop - 868.0f)) * S;
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
				// a seta "você está aqui" no metal do tubo, à esquerda do degrau atual (04/11: fora do tubo ela caía em cima da borda do último
				// botão e, sem seleção, da ponta da barra de energia)
				const float AX = TX + 0.5f * S, AY = SY0 + SegH * 0.5f;
				FillCircle(FVector2D(AX + 3 * S, AY), 5 * S, Hex(0xffc83c, 0.30f), Hex(0xffc83c, 0.0f), 16);
				Tris({ FVector2D(AX, AY - 4.5f * S), FVector2D(AX + 5.5f * S, AY), FVector2D(AX, AY + 4.5f * S) }, Hex(0xf6c850));
			}
		}
		Bg(TX, TY, TW, TH);
		// o cubo por cima do tubo: a peça da facção e o desenho da patente dentro. Com ponto pra gastar (o .ready do protótipo): o rglow
		// (1 s, ease-in-out: brilho 1,35 e a luz dourada de 8 px), o aro dourado borrado (r 20,6, traço 1,6, blur 2,5) e a moeda com o
		// número à esquerda (31×27 em -27, 13; a luz de 6 px). O hover do cubo é o dele (#hub:hover): cresce 8% com a mola, brilho 1,25 e
		// a luz azul; com ponto, a animação manda no brilho e o hover só cresce. Apertado: 95%, brilho 0,85
		const float BX = SX(HX0, EAnchor::Right), BY = HY(HY0);
		const bool bReady = State.Promote > 0;
		const bool bHot = Cur->HoverId == FName(TEXT("promote")), bDown = bHot && Cur->PressedId == FName(TEXT("promote"));
		const float HK = bDown ? Bezier(0.2f, 0.9f, 0.3f, 1.3f, Cur->PressAge / 0.04f) : bHot ? Bezier(0.2f, 0.9f, 0.3f, 1.3f, Cur->HoverAge / 0.13f) : 0.0f;
		const float Sc = bDown ? FMath::Lerp(1.08f, 0.95f, HK) : FMath::Lerp(1.0f, 1.08f, HK);
		const FVector2D HC(BX + 26.0f * S, BY + 26.0f * S);
		const auto M = [&](float X, float Yv) { return FVector2D(HC.X + (BX + X * S - HC.X) * Sc, HC.Y + (BY + Yv * S - HC.Y) * Sc); };
		const float GT = FMath::Fmod(State.Time, 1.0f);
		const float Glow = bReady ? (GT < 0.5f ? Bezier(0.42f, 0.0f, 0.58f, 1.0f, GT * 2.0f) : 1.0f - Bezier(0.42f, 0.0f, 0.58f, 1.0f, GT * 2.0f - 1.0f)) : 0.0f;
		const float Br = bReady ? 1.0f + 0.35f * Glow : bDown ? 0.85f : 1.0f + 0.25f * HK;
		// a luz em volta (o drop-shadow 8 px): dourada respirando com ponto; azul no hover sem ponto
		const FLinearColor GlowC = bReady ? FLinearColor(FColor(255, 205, 70)) : FLinearColor(FColor(73, 170, 255));
		const float GlowA = bReady ? 0.95f * Glow : (bDown ? 0.0f : 0.7f * HK);
		if (GlowA > 0.01f)
		{
			Arc(HC, 23.0f * S * Sc, 26.0f * S * Sc, 0.0f, 1.0f, FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.0f), FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.5f * GlowA));
			Arc(HC, 26.0f * S * Sc, 30.0f * S * Sc, 0.0f, 1.0f, FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.5f * GlowA), FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.18f * GlowA));
			Arc(HC, 30.0f * S * Sc, 35.0f * S * Sc, 0.0f, 1.0f, FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.18f * GlowA), FLinearColor(GlowC.R, GlowC.G, GlowC.B, 0.0f));
		}
		DrawTint = FLinearColor(Br, Br, Br, 1.0f);
		const FVector2D H0 = M(HB.Min.X, HB.Min.Y), H1 = M(HB.Max.X, HB.Max.Y);
		Image(FacSkin(TEXT("hub_general")), H0.X, H0.Y, H1.X - H0.X, H1.Y - H0.Y, FLinearColor::White, false);
		const FString Fac = Cur->SkinFaction.IsNone() ? FString(TEXT("usa")) : Cur->SkinFaction.ToString();
		const FName RankIcon = Fac == TEXT("usa") ? FName(*FString::Printf(TEXT("skin:rank_usa_%d"), Rank)) : FName(*FString::Printf(TEXT("skin:%s/rank_%s_%d"), *Fac, *Fac, Rank));
		const FVector2D I0 = M(4.7f, 4.3f), I1 = M(4.7f + 42.6f, 4.3f + 43.5f);
		Image(RankIcon, I0.X, I0.Y, I1.X - I0.X, I1.Y - I0.Y, FLinearColor::White, false);
		if (bReady)
		{
			// o aro: o traço de 1,6 borrado com desvio 2,5 (a curva de Gauss em 6 faixas; o pico ~0,26)
			const float Rr = 20.6f * S * Sc, G1 = 2.0f * S * Sc, G2 = 4.5f * S * Sc, G3 = 7.0f * S * Sc;
			const FLinearColor Au(FMath::Min(1.0f, 0.965f * Br), FMath::Min(1.0f, 0.784f * Br), FMath::Min(1.0f, 0.314f * Br), 1.0f);
			const auto A = [&](float V) { return FLinearColor(Au.R, Au.G, Au.B, V); };
			Arc(HC, Rr - G3, Rr - G2, 0.0f, 1.0f, A(0.0f), A(0.05f));
			Arc(HC, Rr - G2, Rr - G1, 0.0f, 1.0f, A(0.05f), A(0.185f));
			Arc(HC, Rr - G1, Rr, 0.0f, 1.0f, A(0.185f), A(0.255f));
			Arc(HC, Rr, Rr + G1, 0.0f, 1.0f, A(0.255f), A(0.185f));
			Arc(HC, Rr + G1, Rr + G2, 0.0f, 1.0f, A(0.185f), A(0.05f));
			Arc(HC, Rr + G2, Rr + G3, 0.0f, 1.0f, A(0.05f), A(0.0f));
			// a moeda com os pontos (a luz dourada de 6 px por baixo; o rglow também acende ela)
			const FVector2D T0 = M(-27.0f, 13.0f), T1 = M(4.0f, 40.0f), TC = (T0 + T1) * 0.5f;
			const FLinearColor Gd(FColor(255, 200, 60));
			Arc(TC, 11.0f * S * Sc, 15.0f * S * Sc, 0.0f, 1.0f, FLinearColor(Gd.R, Gd.G, Gd.B, 0.0f), FLinearColor(Gd.R, Gd.G, Gd.B, 0.38f));
			Arc(TC, 15.0f * S * Sc, 22.0f * S * Sc, 0.0f, 1.0f, FLinearColor(Gd.R, Gd.G, Gd.B, 0.38f), FLinearColor(Gd.R, Gd.G, Gd.B, 0.0f));
			Image(TEXT("skin:token_gold"), T0.X, T0.Y, T1.X - T0.X, T1.Y - T0.Y, FLinearColor::White, false);
			TextMid(FString::FromInt(State.Promote), TC.X, TC.Y + 0.5f * S, 16.0f * Sc, Hex(0x2a1d05), EWeight::Bold, EAlign::Center, false);
			Buttons->Add({ TEXT("promote"), FBox2D(T0, T1) });
		}
		DrawTint = FLinearColor::White;
		Buttons->Add({ TEXT("promote"), FBox2D(FVector2D(BX, BY), FVector2D(BX + 52.0f * S, BY + 52.0f * S)) });
	}

	// ---- as plaquetas (01/11, aprovadas: "top"): nascem com a mola (achatadas, esticam passando um pouco e assentam) e ficam
	auto Spring = [](float Age) { const float T = FMath::Clamp(Age / 0.45f, 0.0f, 1.0f); return 1.0f + FMath::Sin(T * PI * 1.5f) * (1.0f - T) * 0.25f - (1.0f - T) * 0.65f * (1.0f - T); };
	if (!State.PlateTitle.IsEmpty())
	{
		// a vermelha em cima do brasão: o triângulo do aviso, o título espaçado e a linha do que falta
		float K = FMath::Clamp(Spring(State.PlateAge), 0.2f, 1.2f), A = FMath::Clamp(State.PlateAge / 0.15f, 0.0f, 1.0f);
		if (State.PlateLife > 0.0f && State.PlateAge > State.PlateLife)
		{
			// (05/11) a saída: a .tag sem o .show (a transição de 0,25 s, cubic-bezier(.2,1.4,.4,1), achata pra 0,4 e apaga em 0,2 s)
			const float OutT = (State.PlateAge - State.PlateLife) / 0.25f;
			K = FMath::Lerp(1.0f, 0.4f, Bezier(0.2f, 1.4f, 0.4f, 1.0f, OutT));
			A = FMath::Clamp(1.0f - (State.PlateAge - State.PlateLife) / 0.2f, 0.0f, 1.0f);
		}
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
	if ((!State.PromotedRank.IsEmpty() || !State.PromotedTitle.IsEmpty()) && State.Stars >= 1)
	{
		// a dourada presa ao cubo pela ponta (v12/v20 do protótipo; a foto aprovada 01/11): a ficha de ouro com a estrela, PROMOTED (ou FIRST
		// POWER na patente 1) espaçado e o nome da patente nova; 39 de alto (6 + a ficha de 27 + 6), o topo 56 acima do cubo, a ponta 16×9
		// dourada com o miolo escuro. Nasce com o promoIn (0,38 s, cubic-bezier(.2,1.4,.4,1): sobe 20 e estica de 0,35 pela base) e respira
		// com o promoGlow (1,4 s: o aro clareia pra #f6c850 e a luz vai de 14 a 26 px). Some quando a tela do general é aberta (o jogo decide)
		const FString Title = State.PromotedTitle.IsEmpty() ? FString(TEXT("PROMOTED")) : State.PromotedTitle;
		const float PIn = Bezier(0.2f, 1.4f, 0.4f, 1.0f, State.PromotedAge / 0.38f);
		const float A = FMath::Clamp(PIn, 0.0f, 1.0f), SclY = FMath::Lerp(0.35f, 1.0f, PIn);
		const float GT = FMath::Max(0.0f, State.PromotedAge - 0.38f) / 1.4f, GF = FMath::Fmod(GT, 1.0f);
		const float Gl = State.PromotedAge < 0.38f ? 0.0f : (GF < 0.5f ? Bezier(0.42f, 0.0f, 0.58f, 1.0f, GF * 2.0f) : 1.0f - Bezier(0.42f, 0.0f, 0.58f, 1.0f, GF * 2.0f - 1.0f));
		const bool bHotP = Cur->HoverId == FName(TEXT("promote"));
		const float Br = bHotP ? 1.15f : 1.0f;
		const auto L = [&](uint32 RGB, float Al) { const FLinearColor C = Hex(RGB, Al); return FLinearColor(FMath::Min(1.0f, C.R * Br), FMath::Min(1.0f, C.G * Br), FMath::Min(1.0f, C.B * Br), C.A); };
		const float PW = MeasureSpaced(Title, 14.0f, EWeight::Bold, 2.5f), RW = State.PromotedRank.IsEmpty() ? 0.0f : Measure(State.PromotedRank, 14.0f, EWeight::SemiBold).X;
		const float W = 8.0f * S + 30.0f * S + 9.0f * S + PW + (RW > 0.0f ? 12.0f * S + RW : 0.0f) + 18.0f * S, H0 = 39.0f * S, H = H0 * SclY;
		const float HX0 = bNone ? 1470.0f : 1486.0f, HY0 = bNone ? 806.0f : 840.0f + (PTop - 868.0f);
		const float X = SX(HX0 + 26.0f, EAnchor::Right) - W * 0.5f, Bot = SY(HY0 - 56.0f, true) + Drop + H0 + 20.0f * S * (1.0f - PIn), PY = Bot - H;
		// a luz em volta (box-shadow 0 0 14→26 px): a borda do box-shadow em 6 anéis (a metade da cor na beira, a curva do erfc pra fora)
		{
			static const float Fall[6] = { 0.433f, 0.309f, 0.203f, 0.122f, 0.067f, 0.033f };
			const float Blur = FMath::Lerp(14.0f, 26.0f, Gl) * S, GA = FMath::Lerp(0.3f, 0.65f, Gl) * A, Step = Blur / 6.0f;
			for (int32 I = 0; I < 6; ++I)
			{
				const float D = I * Step;
				RoundRectRing(X - D - Step, PY - D - Step, W + 2 * (D + Step), H + 2 * (D + Step), 4.0f * S + D + Step, Step, Hex(0xffc83c, GA * Fall[I]));
			}
		}
		const FLinearColor Ring = FMath::Lerp(Hex(0xc9971f), Hex(0xf6c850), Gl);
		RoundRect(X - 2 * S, PY - 2 * S, W + 4 * S, H + 4 * S, 6.0f * S, FLinearColor(FMath::Min(1.0f, Ring.R * Br), FMath::Min(1.0f, Ring.G * Br), FMath::Min(1.0f, Ring.B * Br), A));
		RoundRect(X - 1 * S, PY - 1 * S, W + 2 * S, H + 2 * S, 5.0f * S, FLinearColor(0, 0, 0, A));
		const FLinearColor BgTop = L(0x231a07, A), BgBot = L(0x0f0b04, A);
		FillPoly(ChamferPoly(X, PY, W, H, 2.5f * S), [=](const FVector2D& P) { return FMath::Lerp(BgTop, BgBot, FMath::Clamp((P.Y - PY) / FMath::Max(1.0f, H), 0.0f, 1.0f)); });
		Rect(X + 3 * S, PY, W - 6 * S, FMath::Max(1.0f, S), Hex(0xffe696, FMath::Lerp(0.22f, 0.3f, Gl) * A));
		// a ponta: o triângulo dourado 16×9 e o miolo escuro 12×6, colados embaixo (esticam junto)
		const float CXp = X + W * 0.5f;
		Tris({ FVector2D(CXp - 8.0f * S, Bot + 1.0f * S), FVector2D(CXp + 8.0f * S, Bot + 1.0f * S), FVector2D(CXp, Bot + (1.0f + 9.0f * SclY) * S) }, Hex(0xc9971f, A));
		Tris({ FVector2D(CXp - 6.0f * S, Bot + 1.0f * S), FVector2D(CXp + 6.0f * S, Bot + 1.0f * S), FVector2D(CXp, Bot + (1.0f + 6.0f * SclY) * S) }, Hex(0x120d05, A));
		if (SclY > 0.8f)
		{
			const float TA = A * FMath::Clamp((SclY - 0.8f) / 0.15f, 0.0f, 1.0f);
			const float CY = PY + H * 0.5f;
			Image(TEXT("skin:token_gold"), X + 8.0f * S, CY - 13.5f * S, 30.0f * S, 27.0f * S, FLinearColor(Br, Br, Br, TA), false);
			Star(FVector2D(X + 23.0f * S, CY), 6.0f * S, Hex(0x2a1d05, TA));
			const float TX = X + 47.0f * S;
			TextSpaced(Title, TX, CY, 14.0f, L(0xf6c850, TA), EWeight::Bold, 2.5f, EAlign::Left, true);
			if (RW > 0.0f)
			{
				TextMid(State.PromotedRank, TX + PW + 12.0f * S, CY, 14.0f, L(0xe6ecf3, TA), EWeight::SemiBold);
			}
		}
		// clicar na plaqueta também abre a tela do general
		Buttons->Insert(FRTSHudButton{ TEXT("promote"), FBox2D(FVector2D(X - 2 * S, PY - 2 * S), FVector2D(X + W + 2 * S, Bot + 10.0f * S)) }, 0);
	}
}
