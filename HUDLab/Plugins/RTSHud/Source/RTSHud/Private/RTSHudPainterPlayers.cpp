#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "GlobalRenderResources.h"

// Os jogadores no HUD (03/11, aprovados no protótipo docs/concept/hud/prototipo/jogadores.html; o estudo em docs/concept/hud/JOGADORES.md):
// a barra dos times no topo, a lista C das superarmas no canto de cima à direita e o placar do Tab com a coluna de todas as superarmas.
// As medidas são as do protótipo, em px da planta (1920×1080); as cores em sRGB, como lá.

namespace
{
	FLinearColor Hex(uint32 RGB, float A = 1.0f)
	{
		FLinearColor C(FColor((RGB >> 16) & 255, (RGB >> 8) & 255, RGB & 255));
		C.A = A;
		return C;
	}

	FLinearColor Alpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	/** A cor do jogador clareada pra texto e silhueta: misturada com o branco em sRGB (a light() do protótipo). */
	FLinearColor Light(const FLinearColor& C, float K)
	{
		const FColor Src = C.ToFColor(true);
		auto Mix = [K](uint8 V) { return uint8(FMath::RoundToInt(V + (255 - V) * K)); };
		return FLinearColor(FColor(Mix(Src.R), Mix(Src.G), Mix(Src.B)));
	}

	/** A cor do jogador acende o vidro da barra: a escura sobe até um brilho mínimo, o tom fica (a mesma conta do topo 1x1). */
	FLinearColor Glow(FLinearColor C)
	{
		const float M = FMath::Max3(C.R, C.G, C.B);
		if (M > 0.001f && M < 0.7f)
		{
			C *= 0.7f / M;
		}
		C.A = 1.0f;
		return C;
	}

	FString Commas(int64 V)
	{
		FString D = FString::Printf(TEXT("%lld"), V < 0 ? -V : V);
		for (int32 I = D.Len() - 3; I > 0; I -= 3)
		{
			D.InsertAt(I, TEXT(','));
		}
		return V < 0 ? TEXT("-") + D : D;
	}

	FString Thousands(double V)
	{
		FString T = FString::Printf(TEXT("%.1f"), V / 1000.0);
		T.RemoveFromEnd(TEXT(".0"));
		return T + TEXT("k");
	}

	/** O número curto: 9,999 por extenso, depois 12.3k. */
	FString Short(int64 V)
	{
		return V >= 10000 ? Thousands(double(V)) : Commas(V);
	}

	FString Clock(int32 Seconds)
	{
		return FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
	}

	/** As superarmas de um dono do mesmo tipo numa linha: quantas, quantas prontas, a próxima e a de depois. */
	struct FSwGroup
	{
		int32 Player = 0;
		const FRTSHudSuperweapon* First = nullptr; // o nome e o desenho
		int32 N = 0;
		int32 Ready = 0;
		int32 Next = 0;   // 0 com alguma pronta
		int32 After = -1; // a próxima depois da mostrada: com prontas, a primeira que carrega; sem prontas, a segunda
		int32 Full = 600;
	};

	TArray<FSwGroup> GroupSuperweapons(const TArray<FRTSHudPlayer>& Players, TFunctionRef<bool(const FRTSHudPlayer&)> Keep)
	{
		TArray<FSwGroup> Out;
		for (int32 I = 0; I < Players.Num(); ++I)
		{
			const FRTSHudPlayer& P = Players[I];
			if (!Keep(P))
			{
				continue;
			}
			TArray<FName> Types;
			for (const FRTSHudSuperweapon& W : P.Superweapons)
			{
				Types.AddUnique(W.Type);
			}
			for (const FName T : Types)
			{
				FSwGroup G;
				G.Player = I;
				TArray<int32> Rest;
				for (const FRTSHudSuperweapon& W : P.Superweapons)
				{
					if (W.Type != T)
					{
						continue;
					}
					if (!G.First)
					{
						G.First = &W;
						G.Full = FMath::Max(1, W.FullSeconds);
					}
					++G.N;
					if (W.Seconds <= 0)
					{
						++G.Ready;
					}
					else
					{
						Rest.Add(W.Seconds);
					}
				}
				Rest.Sort();
				G.Next = G.Ready ? 0 : Rest[0];
				const int32 AfterIndex = G.Ready ? 0 : 1;
				G.After = Rest.IsValidIndex(AfterIndex) ? Rest[AfterIndex] : -1;
				Out.Add(G);
			}
		}
		// a mais perto de disparar em cima; empatadas, a de mais armas
		Out.StableSort([](const FSwGroup& A, const FSwGroup& B) { return A.Next != B.Next ? A.Next < B.Next : A.N > B.N; });
		return Out;
	}

	/** A linha fina de cima do painel das superarmas, na cor da pele (o --lt do protótipo). */
	FLinearColor FactionLine(FName F, float A)
	{
		if (F == TEXT("china")) return Hex(0xf04832, A);
		if (F == TEXT("gla")) return Hex(0xd6aa60, A);
		if (F == TEXT("russia")) return Hex(0xe6f0ff, A);
		return Hex(0x49aaff, A);
	}
}

// ---------------------------------------------------------------------------------------------
// primitivas

void FRTSHudPainter::Arc(FVector2D C, float R0, float R1, float A0, float A1, const FLinearColor& C0, const FLinearColor& C1)
{
	if (A1 <= A0 || R1 <= R0)
	{
		return;
	}
	const int32 N = FMath::Clamp(FMath::CeilToInt((A1 - A0) * 2.0f * PI * R1 / 3.0f), 6, 96);
	auto At = [&](float Turn, float R) { const float A = Turn * 2.0f * PI; return C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R; };
	TArray<FCanvasUVTri> List;
	List.Reserve(N * 2);
	for (int32 K = 0; K < N; ++K)
	{
		const float Ta = A0 + (A1 - A0) * K / N, Tb = A0 + (A1 - A0) * (K + 1) / N;
		FCanvasUVTri T1, T2;
		T1.V0_Pos = At(Ta, R0); T1.V1_Pos = At(Ta, R1); T1.V2_Pos = At(Tb, R1);
		T1.V0_Color = C0; T1.V1_Color = C1; T1.V2_Color = C1;
		T2.V0_Pos = At(Ta, R0); T2.V1_Pos = At(Tb, R1); T2.V2_Pos = At(Tb, R0);
		T2.V0_Color = C0; T2.V1_Color = C1; T2.V2_Color = C0;
		List.Add(T1);
		List.Add(T2);
	}
	FCanvasTriangleItem Item(List, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::SoftArc(FVector2D C, float R0, float R1, float A0, float A1, const FLinearColor& Col)
{
	// o Canvas não suaviza triângulo: o anel fino sai serrilhado. As duas beiras ganham meio pixel esfumado pra cada lado
	const float H = 0.5f;
	const FLinearColor Clear = Alpha(Col, 0.0f);
	Arc(C, R0 + H, FMath::Max(R0 + H, R1 - H), A0, A1, Col, Col);
	Arc(C, FMath::Max(0.0f, R0 - H), R0 + H, A0, A1, Clear, Col);
	Arc(C, R1 - H, R1 + H, A0, A1, Col, Clear);
}

void FRTSHudPainter::ChargeRing(FVector2D C, float R, float T, float Frac, const FLinearColor& Track, const FLinearColor& Lit)
{
	Frac = FMath::Clamp(Frac, 0.0f, 1.0f);
	// o trilho só onde a carga ainda não chegou (o aceso por cima dele somaria as duas transparências)
	SoftArc(C, R - T * 0.5f, R + T * 0.5f, Frac, 1.0f, Track);
	if (Frac <= 0.0f)
	{
		return;
	}
	SoftArc(C, R - T * 0.5f, R + T * 0.5f, 0.0f, Frac, Lit);
	if (Frac < 1.0f)
	{
		// as pontas redondas (o stroke-linecap do protótipo)
		for (const float Turn : { 0.0f, Frac })
		{
			const float A = Turn * 2.0f * PI;
			const FVector2D P = C + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * R;
			FillCircle(P, FMath::Max(0.1f, T * 0.5f - 0.5f), Lit, Lit, 12);
			Arc(P, FMath::Max(0.0f, T * 0.5f - 0.5f), T * 0.5f + 0.5f, 0.0f, 1.0f, Lit, Alpha(Lit, 0.0f));
		}
	}
}

void FRTSHudPainter::RoundRect(float X, float Y, float W, float H, float R, TArrayView<const TPair<float, FLinearColor>> Stops, float X0, float X1)
{
	R = FMath::Min(R, FMath::Min(W, H) * 0.5f);
	X0 = FMath::Max(X0, X);
	X1 = FMath::Min(X1, X + W);
	if (X1 <= X0 || H <= 0.0f || Stops.Num() == 0)
	{
		return;
	}
	// a faixa é monótona em x: colunas nos cantos (8 passos), nas paradas do degradê e nas pontas do pedaço; entre duas colunas, um quadrado
	TArray<float> Xs;
	for (int32 I = 0; I <= 8; ++I)
	{
		const float D = R * FMath::Cos(0.5f * PI * I / 8);
		Xs.Add(X + R - D);
		Xs.Add(X + W - R + D);
	}
	for (const TPair<float, FLinearColor>& St : Stops)
	{
		Xs.Add(X + St.Key * W);
	}
	Xs.Add(X0);
	Xs.Add(X1);
	Xs.Sort();
	auto Extent = [&](float Px, float& Top, float& Bottom)
	{
		float D = 0.0f;
		if (Px < X + R) D = X + R - Px;
		else if (Px > X + W - R) D = Px - (X + W - R);
		const float Dy = R - FMath::Sqrt(FMath::Max(0.0f, R * R - D * D));
		Top = Y + Dy;
		Bottom = Y + H - Dy;
	};
	auto ColorAt = [&](float Px)
	{
		const float T = (Px - X) / W;
		if (T <= Stops[0].Key) return Stops[0].Value;
		for (int32 I = 1; I < Stops.Num(); ++I)
		{
			if (T <= Stops[I].Key)
			{
				const float K = (T - Stops[I - 1].Key) / FMath::Max(1e-4f, Stops[I].Key - Stops[I - 1].Key);
				return FMath::Lerp(Stops[I - 1].Value, Stops[I].Value, K);
			}
		}
		return Stops.Last().Value;
	};
	TArray<FCanvasUVTri> List;
	float Prev = X0;
	for (const float Cx : Xs)
	{
		if (Cx <= Prev + 0.01f || Cx > X1)
		{
			continue;
		}
		float T0, B0, T1, B1;
		Extent(Prev, T0, B0);
		Extent(Cx, T1, B1);
		const FLinearColor Ca = ColorAt(Prev), Cb = ColorAt(Cx);
		FCanvasUVTri A, B;
		A.V0_Pos = FVector2D(Prev, T0); A.V1_Pos = FVector2D(Cx, T1); A.V2_Pos = FVector2D(Cx, B1);
		A.V0_Color = Ca; A.V1_Color = Cb; A.V2_Color = Cb;
		B.V0_Pos = FVector2D(Prev, T0); B.V1_Pos = FVector2D(Cx, B1); B.V2_Pos = FVector2D(Prev, B0);
		B.V0_Color = Ca; B.V1_Color = Cb; B.V2_Color = Ca;
		List.Add(A);
		List.Add(B);
		Prev = Cx;
	}
	if (List.Num())
	{
		FCanvasTriangleItem Item(List, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

void FRTSHudPainter::RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& C)
{
	const TPair<float, FLinearColor> Stops[2] = { { 0.0f, C }, { 1.0f, C } };
	RoundRect(X, Y, W, H, R, Stops);
}

void FRTSHudPainter::RoundRectRing(float X, float Y, float W, float H, float R, float T, const FLinearColor& C)
{
	auto Contour = [](float CX, float CY, float CW, float CH, float CR, TArray<FVector2D>& Out)
	{
		CR = FMath::Max(0.0f, FMath::Min(CR, FMath::Min(CW, CH) * 0.5f));
		const FVector2D Ctr[4] = { { CX + CW - CR, CY + CR }, { CX + CW - CR, CY + CH - CR }, { CX + CR, CY + CH - CR }, { CX + CR, CY + CR } };
		for (int32 K = 0; K < 4; ++K)
		{
			for (int32 I = 0; I <= 6; ++I)
			{
				const float A = (K * 0.25f + 0.25f * I / 6) * 2.0f * PI;
				Out.Add(Ctr[K] + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * CR);
			}
		}
	};
	TArray<FVector2D> O, I;
	Contour(X, Y, W, H, R, O);
	Contour(X + T, Y + T, W - 2 * T, H - 2 * T, R - T, I);
	TArray<FCanvasUVTri> List;
	for (int32 K = 0; K < O.Num(); ++K)
	{
		const int32 N = (K + 1) % O.Num();
		FCanvasUVTri A, B;
		A.V0_Pos = O[K]; A.V1_Pos = O[N]; A.V2_Pos = I[N];
		B.V0_Pos = O[K]; B.V1_Pos = I[N]; B.V2_Pos = I[K];
		A.V0_Color = A.V1_Color = A.V2_Color = C;
		B.V0_Color = B.V1_Color = B.V2_Color = C;
		List.Add(A);
		List.Add(B);
	}
	FCanvasTriangleItem Item(List, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

float FRTSHudPainter::MeasureSpaced(const FString& Str, float Px, EWeight W, float Spacing) const
{
	// como o letter-spacing do CSS: o espaço vai depois de cada letra, a última também
	float Sum = 0.0f;
	for (int32 I = 0; I < Str.Len(); ++I)
	{
		Sum += Measure(Str.Mid(I, 1), Px, W).X + Spacing * S;
	}
	return Sum;
}

FVector2D FRTSHudPainter::TextSpaced(const FString& Str, float X, float CY, float Px, const FLinearColor& C, EWeight W, float Spacing, EAlign Al, bool bShadow)
{
	const float Total = MeasureSpaced(Str, Px, W, Spacing);
	float Pen = Al == EAlign::Center ? X - Total * 0.5f : Al == EAlign::Right ? X - Total : X;
	for (int32 I = 0; I < Str.Len(); ++I)
	{
		const FString Ch = Str.Mid(I, 1);
		if (Ch != TEXT(" "))
		{
			TextMid(Ch, Pen, CY, Px, C, W, EAlign::Left, bShadow);
		}
		Pen += Measure(Ch, Px, W).X + Spacing * S;
	}
	return FVector2D(Total, Measure(Str, Px, W).Y);
}

FString FRTSHudPainter::Ellipsize(const FString& Str, float Px, EWeight W, float MaxW) const
{
	if (Measure(Str, Px, W).X <= MaxW)
	{
		return Str;
	}
	FString Cut = Str;
	while (Cut.Len() > 1)
	{
		Cut.LeftChopInline(1);
		const FString Try = Cut.TrimEnd() + TEXT("…");
		if (Measure(Try, Px, W).X <= MaxW)
		{
			return Try;
		}
	}
	return TEXT("…");
}

void FRTSHudPainter::Chip(const FRTSHudPlayer& P, FVector2D C, float D, float A)
{
	// a ficha do protótipo: o encaixe escuro, o aro de 2 px na cor do jogador e a sombra em volta; a silhueta do emblema dentro, na cor
	// dele clareada (no escuro o azul e o roxo puros não passam no contraste). Derrotado: tudo cinza e o X.
	const float R = D * 0.5f;
	Arc(C, R + 1.5f * S, R + 7 * S, 0.0f, 1.0f, FLinearColor(0, 0, 0, 0.55f * A), FLinearColor(0, 0, 0, 0));
	FillCircle(C, R, Hex(0x0b0f14, A), Hex(0x0b0f14, A), 32);
	SoftArc(C, R, R + 2 * S, 0.0f, 1.0f, Alpha(P.bDead ? Hex(0x3a3f45) : P.Color, A));
	FString Fac = P.Emblem.ToString();
	Fac.RemoveFromStart(TEXT("emblem:"));
	const float G = R * 0.68f;
	Image(FName(*FString::Printf(TEXT("emblem:glyph_%s"), *Fac)), C.X - G, C.Y - G, 2 * G, 2 * G, Alpha(P.bDead ? Hex(0x4a5058) : Light(P.Color, 0.25f), A), false);
	if (P.bDead)
	{
		const float X = R * 0.56f;
		Image(TEXT("skin:defeated_x"), C.X - X, C.Y - X, 2 * X, 2 * X, FLinearColor(1, 1, 1, 0.9f * A), false);
	}
}

// ---------------------------------------------------------------------------------------------
// a barra do topo com mais de 2 jogadores (03/11; o 2 × 2 equilibrado "fica perfeito", o desequilíbrio "não vai funcionar"): montada
// com a arte do duelo da facção, cortada em pedaços; só a faixa lisa do meio de cada painel estica, e os dois painéis têm sempre o
// comprimento do maior (o relógio fica no centro da barra). Dois times: o painel da esquerda na sua cor é o seu time, o da direita
// vermelho o outro. Três ou mais times e todos contra todos: o vidro neutro e as fichas divididas por igual entre os dois lados (o seu
// time colado em você, com o traço na sua cor embaixo, depois os outros times, separados por um respiro). Você no anel da esquerda; no
// da direita, quantos inimigos ainda estão vivos.

void FRTSHudPainter::DrawTopTeams(const FRTSHudState& State)
{
	const FName F = Cur->SkinFaction;
	const FString Fac = F.IsNone() ? FString(TEXT("usa")) : F.ToString();
	const FName Base(*FString::Printf(TEXT("skin:%s/top_bar_1v1"), *Fac));
	const FName MeLayer(*FString::Printf(TEXT("skin:%s/top_bar_1v1_me"), *Fac)), FoeLayer(*FString::Printf(TEXT("skin:%s/top_bar_1v1_foe"), *Fac));
	UTexture* BaseTex = Lookup ? Lookup(Base) : nullptr;
	if (!BaseTex)
	{
		return;
	}
	const float TW = BaseTex->GetSurfaceWidth();
	const float MX = (TW - 1087.0f) * 0.5f;          // a margem da arte (as pontas que saem da caixa)
	const float K = 617.0f / 1087.0f;                // px da arte → px da planta (a escala do topo 1x1)
	const float BarH = BaseTex->GetSurfaceHeight() * K;

	// quem vai em cada painel: listas de grupos (um grupo = um time)
	int32 MeIdx = 0;
	TArray<int32> Allies;
	TArray<int32> EnemyTeams;
	TArray<TArray<int32>> Groups;
	for (int32 I = 0; I < State.Players.Num(); ++I)
	{
		const FRTSHudPlayer& P = State.Players[I];
		if (P.bMe)
		{
			MeIdx = I;
		}
		else if (P.bAlly)
		{
			Allies.Add(I);
		}
		else
		{
			int32 G = EnemyTeams.Find(P.Team);
			if (G == INDEX_NONE)
			{
				G = EnemyTeams.Add(P.Team);
				Groups.AddDefaulted();
			}
			Groups[G].Add(I);
		}
	}
	const FRTSHudPlayer& Me = State.Players[MeIdx];
	const bool bMany = EnemyTeams.Num() >= 2;
	TArray<TArray<int32>> LeftG, RightG;
	if (!bMany)
	{
		if (Allies.Num())
		{
			LeftG.Add(Allies);
		}
		RightG = Groups;
	}
	else
	{
		// a fila inteira (o seu time, depois os inimigos time a time), cortada no meio; o lado esquerdo fica com a metade de cima
		TArray<TPair<int32, int32>> Seq;
		for (const int32 I : Allies) Seq.Add({ I, 0 });
		for (int32 G = 0; G < Groups.Num(); ++G)
		{
			for (const int32 I : Groups[G]) Seq.Add({ I, G + 1 });
		}
		const int32 Half = (Seq.Num() + 1) / 2;
		for (int32 J = 0; J < Seq.Num(); ++J)
		{
			TArray<TArray<int32>>& Side = J < Half ? LeftG : RightG;
			if (J == 0 || J == Half || Seq[J].Value != Seq[J - 1].Value)
			{
				Side.AddDefaulted();
			}
			Side.Last().Add(Seq[J].Key);
		}
	}
	const float D = 28.0f, Gap = 6.0f, TeamGap = 14.0f;
	auto Span = [&](const TArray<TArray<int32>>& Gs)
	{
		float W = FMath::Max(0, Gs.Num() - 1) * TeamGap;
		for (const TArray<int32>& G : Gs) W += G.Num() * D + (G.Num() - 1) * Gap;
		return W;
	};
	const float NeedL = Span(LeftG), NeedR = Span(RightG), Need = FMath::Max(NeedL, NeedR);

	// os cortes (em unidades da peça de 1087): a ponta esquerda com o anel e o começo do painel | a faixa lisa (estica) | o fim do painel |
	// o relógio | o espelho do outro lado
	const float Stretch = FMath::Max(0.0f, Need + 40.0f - (196.0f + 4.0f + 60.0f) * K) + 4.0f * K;
	struct FSeg { float A, B, W; };
	const FSeg Segs[11] = {
		{ -110, 170, -1 }, { 170, 240, -1 }, { 240, 244, Stretch }, { 244, 370, -1 }, { 370, 430, -1 }, { 430, 657, -1 },
		{ 657, 717, -1 }, { 717, 843, -1 }, { 843, 847, Stretch }, { 847, 917, -1 }, { 917, 1197, -1 } };
	float Pos[11];
	float LeftW = 0.0f;
	for (int32 I = 0; I < 5; ++I)
	{
		LeftW += Segs[I].W >= 0 ? Segs[I].W : (Segs[I].B - Segs[I].A) * K;
	}
	const FLinearColor MeC = Glow(Me.Color), FoeC = Glow(Hex(0xe0443a));
	float X = 960.0f - 227.0f * K * 0.5f - LeftW;
	for (int32 I = 0; I < 11; ++I)
	{
		const FSeg& Sg = Segs[I];
		const float W = Sg.W >= 0 ? Sg.W : (Sg.B - Sg.A) * K;
		Pos[I] = X;
		const FVector2D UV0((MX + Sg.A) / TW, 0.0f), UV1((MX + Sg.B) / TW, 1.0f);
		const float DX = SX(X, EAnchor::Center), DW = (W + 0.6f) * S, DH = BarH * S;
		ImageUV(Base, DX, SY(0, false), DW, DH, UV0, UV1, FLinearColor::White);
		if (!bMany)
		{
			ImageUV(MeLayer, DX, SY(0, false), DW, DH, UV0, UV1, MeC);
			ImageUV(FoeLayer, DX, SY(0, false), DW, DH, UV0, UV1, FoeC);
		}
		X += W;
	}
	Bg(SX(Pos[0] + 110.0f * K, EAnchor::Center), SY(0, false), (X - Pos[0] - 220.0f * K) * S, 140.0f * K * S);

	const float CY = 68.0f * K;
	const float RingL = Pos[0] + 193.0f * K, RingR = Pos[10] + 89.0f * K;
	const float PL0 = Pos[0] + 260.0f * K, PL1 = Pos[4] + 30.0f * K, PR0 = Pos[6] + 30.0f * K, PR1 = Pos[10] + 20.0f * K;
	auto PlaceChip = [&](int32 I, float Cx, float Size)
	{
		const FVector2D C(SX(Cx, EAnchor::Center), SY(CY, false));
		Chip(State.Players[I], C, Size * S);
		const float H = Size * 0.5f * S;
		Buttons->Add({ FName(*FString::Printf(TEXT("player_%d"), I)), FBox2D(C - FVector2D(H, H), C + FVector2D(H, H)) });
	};
	PlaceChip(MeIdx, RingL, 40.0f);
	TextMid(State.Clock, SX(960.0f, EAnchor::Center), SY(CY, false), 34.0f, Hex(0xe9eef4), EWeight::Bold, EAlign::Center);
	auto Place = [&](const TArray<TArray<int32>>& Gs, float C0, float C1, float NeedSide)
	{
		float Cx = (C0 + C1) * 0.5f - NeedSide * 0.5f + D * 0.5f;
		for (const TArray<int32>& G : Gs)
		{
			const float X0 = Cx;
			for (const int32 I : G)
			{
				PlaceChip(I, Cx, D);
				Cx += D + Gap;
			}
			if (bMany && State.Players[G[0]].bAlly)
			{
				// o traço do seu time embaixo dos aliados, na sua cor (com o brilho)
				const float LX = SX(X0 - D * 0.5f, EAnchor::Center), LW = (Cx - Gap - X0) * S, LY = SY(CY + D * 0.5f + 4.0f, false);
				RoundRect(LX - 2 * S, LY - 2 * S, LW + 4 * S, 7 * S, 3.5f * S, Alpha(Glow(Me.Color), 0.25f));
				RoundRect(LX, LY, LW, 3 * S, 1.5f * S, Light(Me.Color, 0.3f));
			}
			Cx += TeamGap - Gap;
		}
	};
	if (LeftG.Num())
	{
		Place(LeftG, PL0, PL1, NeedL);
	}
	else
	{
		TextMid(Me.Name, SX((PL0 + PL1) * 0.5f, EAnchor::Center), SY(CY, false), 22.0f, Light(Me.Color, 0.4f), EWeight::Bold, EAlign::Center);
	}
	Place(RightG, PR0, PR1, NeedR);
	int32 Alive = 0;
	for (const TArray<int32>& G : Groups)
	{
		for (const int32 I : G) Alive += State.Players[I].bDead ? 0 : 1;
	}
	TextMid(FString::FromInt(Alive), SX(RingR, EAnchor::Center), SY(CY, false), 30.0f, Hex(0xff7c6c), EWeight::Bold, EAlign::Center);
}

// ---------------------------------------------------------------------------------------------
// a lista C das superarmas (03/11, "vamos ficar com o C [...] sujar menos a tela, diminuir [...] e jogar pro topo"): um painel escuro
// translúcido no canto de cima à direita com o fio da cor da pele em cima; uma linha por dono e tipo (as várias iguais = "×N", as
// capturadas e os outros tipos em linhas próprias): o desenho da arma no arco de carga na cor do dono, o nome e o tempo (ou "N READY"),
// a mais perto de disparar em cima. Até 8 linhas; o resto, "+N · TAB" (o placar tem todas). Some com o placar aberto.

void FRTSHudPainter::DrawSuperweaponList(const FRTSHudState& State)
{
	if (State.bScoreboard)
	{
		return;
	}
	const TArray<FSwGroup> Gs = GroupSuperweapons(State.Players, [](const FRTSHudPlayer& P) { return !P.bMe && !P.bDead; });
	if (Gs.Num() == 0)
	{
		return;
	}
	const int32 Rows = FMath::Min(8, Gs.Num()), More = Gs.Num() - Rows;
	// no canto de cima (o dinheiro mora no console desde 04/11: o canto ficou livre)
	const float Top = 8.0f;
	const float X = SX(1676.0f, EAnchor::Right), Y = SY(Top, false), W = 232.0f * S;
	const float H = (3.0f + Rows * 24.0f + (More > 0 ? 15.0f : 0.0f) + 5.0f) * S;
	RoundRect(X - 3 * S, Y + 1 * S, W + 6 * S, H + 5 * S, 8 * S, FLinearColor(0, 0, 0, 0.10f));
	RoundRect(X - 1 * S, Y + 1.5f * S, W + 2 * S, H + 2.5f * S, 6 * S, FLinearColor(0, 0, 0, 0.14f));
	RoundRect(X, Y, W, H, 5 * S, Hex(0x080b0f, 0.62f));
	Rect(X + 3 * S, Y, W - 6 * S, 1.5f * S, FactionLine(Cur->SkinFaction, 0.6f));
	Bg(X, Y, W, H);
	for (int32 R = 0; R < Rows; ++R)
	{
		const FSwGroup& G = Gs[R];
		const FRTSHudPlayer& P = State.Players[G.Player];
		const float CY = Y + (3.0f + R * 24.0f + 12.0f) * S;
		const bool bHot = !G.Ready && G.Next < 60;
		const FLinearColor Own = Light(P.Color, 0.2f);
		const FVector2D IC(X + 19.0f * S, CY);
		ChargeRing(IC, 7.875f * S, 1.35f * S, G.Ready ? 1.0f : FMath::Max(0.03f, 1.0f - float(G.Next) / G.Full),
			FLinearColor(1, 1, 1, 0.12f), G.Ready || bHot ? Hex(0xff5a48) : Own);
		Image(G.First->Glyph, IC.X - 4.5f * S, CY - 4.5f * S, 9 * S, 9 * S, Hex(0xeef2f6), false);
		// da direita pra esquerda: o tempo, o "×N", e o nome no que sobra
		const FString Time = G.Ready ? (G.Ready > 1 ? FString::Printf(TEXT("%s READY"), *Short(G.Ready)) : FString(TEXT("READY"))) : Clock(G.Next);
		const float TimeR = X + W - 10.0f * S;
		const FVector2D TE = TextMid(Time, TimeR, CY, 14.5f, G.Ready || bHot ? Hex(0xff6a5a) : FLinearColor::White, EWeight::Bold, EAlign::Right);
		float NameR = TimeR - TE.X - 7.0f * S;
		if (G.N > 1)
		{
			const FString Xn = FString::Printf(TEXT("×%s"), *Short(G.N));
			const float PW = Measure(Xn, 10.5f, EWeight::Bold).X + 8.0f * S, PH = 13.0f * S;
			RoundRect(NameR - PW, CY - PH * 0.5f, PW, PH, 3.0f * S, Hex(0xffd257));
			TextMid(Xn, NameR - PW * 0.5f, CY, 10.5f, Hex(0x0b0f14), EWeight::Bold, EAlign::Center, false);
			NameR -= PW + 5.0f * S;
		}
		const float NameX = X + 35.0f * S;
		TextMid(Ellipsize(G.First->Name, 12.5f, EWeight::SemiBold, NameR - NameX), NameX, CY, 12.5f, Hex(0xd9e0e7), EWeight::SemiBold);
	}
	if (More > 0)
	{
		TextMid(FString::Printf(TEXT("+%d · TAB"), More), X + W - 10.0f * S, Y + (3.0f + Rows * 24.0f + 8.5f) * S, 11.5f, Hex(0x8fa0b2), EWeight::SemiBold, EAlign::Right);
	}
}

// ---------------------------------------------------------------------------------------------
// o placar (Tab, 03/11 aprovado): a janela da tela de poderes da facção (a moldura por fora, o metal por dentro, escurecido). Cada jogador
// numa faixa arredondada no tom da cor dele (forte à esquerda, sumindo pra direita), o seu time primeiro, os outros pela pontuação e os
// times derrotados por último, um respiro entre times; 7 colunas; a economia escondida dos inimigos com um olho só, a espionada em
// laranja. À direita, a coluna de todas as superarmas (as suas e as dos aliados também): uma faixa por dono e tipo na cor do dono, o
// desenho no anel de carga, a arma e o dono (YOU/ALLY), a plaquinha vermelha com o número de prontas e a âmbar com quantas carregam e
// quando vem a próxima; as que têm prontas em cima. Mais linhas que cabem: a coluna rola (ScoreSwScroll), a última aparece apagada.

void FRTSHudPainter::DrawScoreboard(const FRTSHudState& State)
{
	if (!State.bScoreboard || State.Players.Num() == 0)
	{
		return;
	}
	const TArray<FRTSHudPlayer>& Ps = State.Players;
	int32 MeIdx = 0;
	TArray<int32> Teams;
	for (int32 I = 0; I < Ps.Num(); ++I)
	{
		if (Ps[I].bMe) MeIdx = I;
		Teams.AddUnique(Ps[I].Team);
	}
	const bool bFfa = Ps.Num() > 2 && Teams.Num() == Ps.Num();
	auto RowLess = [&](int32 A, int32 B)
	{
		const FRTSHudPlayer& Pa = Ps[A];
		const FRTSHudPlayer& Pb = Ps[B];
		if (Pa.bMe != Pb.bMe) return Pa.bMe;
		if (Pa.bDead != Pb.bDead) return !Pa.bDead;
		return Pa.Score > Pb.Score;
	};
	// os blocos: o seu time primeiro; os outros pela pontuação somada, os já derrotados por último
	struct FBlock { int32 Team; TArray<int32> Rows; };
	TArray<FBlock> Blocks;
	if (bFfa)
	{
		FBlock B{ -1, {} };
		for (int32 I = 0; I < Ps.Num(); ++I) B.Rows.Add(I);
		B.Rows.StableSort(RowLess);
		Blocks.Add(B);
	}
	else
	{
		TArray<int32> Order = Teams;
		Order.Remove(Ps[MeIdx].Team);
		auto TScore = [&](int32 T) { int64 Sum = 0; for (const FRTSHudPlayer& P : Ps) if (P.Team == T) Sum += P.Score; return Sum; };
		auto TDead = [&](int32 T) { for (const FRTSHudPlayer& P : Ps) if (P.Team == T && !P.bDead) return false; return true; };
		Order.StableSort([&](int32 A, int32 B) { return TDead(A) != TDead(B) ? !TDead(A) : TScore(A) > TScore(B); });
		Order.Insert(Ps[MeIdx].Team, 0);
		for (const int32 T : Order)
		{
			FBlock B{ T, {} };
			for (int32 I = 0; I < Ps.Num(); ++I) if (Ps[I].Team == T) B.Rows.Add(I);
			B.Rows.StableSort(RowLess);
			Blocks.Add(B);
		}
	}

	// as superarmas de todos (as suas e as dos aliados também), as que têm prontas em cima. Nenhuma na partida (03/11, ele: "não mostra
	// nada mesmo"): a coluna não existe e a janela fica só com os jogadores
	TArray<FSwGroup> Gs = GroupSuperweapons(Ps, [](const FRTSHudPlayer& P) { return !P.bDead; });
	Gs.StableSort([](const FSwGroup& A, const FSwGroup& B)
	{
		if ((A.Ready > 0) != (B.Ready > 0)) return A.Ready > 0;
		if (A.Ready != B.Ready) return A.Ready > B.Ready;
		return A.Next < B.Next;
	});
	const bool bSide = Gs.Num() > 0;

	// a janela: 1540 de largura (1172 sem a coluna); a altura sai do conteúdo; o meio da tela um pouco acima (o translate(-50%, -48%) do protótipo)
	float GridH = 24.0f;
	for (const FBlock& B : Blocks) GridH += 22.0f + B.Rows.Num() * 42.0f + (B.Rows.Num() - 1) * 4.0f;
	const float BodyH = bSide ? FMath::Max(GridH, 300.0f) : GridH;   // a altura mínima é da coluna (a lista que rola)
	const float BoxW = bSide ? 1540.0f : 1172.0f, BoxH = 34.0f + 34.0f + 22.0f + BodyH + 36.0f;
	const float PX0 = 960.0f - BoxW * 0.5f, PY0 = 540.0f - BoxH * 0.48f;
	auto PX = [&](float V) { return SX(PX0 + V, EAnchor::Center); };
	auto PY = [&](float V) { return SY(PY0 + V, false); };

	Rect(0, 0, Canvas->ClipX, Canvas->ClipY, Hex(0x020407, 0.6f));
	{
		// o fundo de metal da facção (repete a cada 512), escurecido; a moldura por fora da janela, os cantos inteiros
		const FName F = Cur->SkinFaction;
		const FString Fac = F.IsNone() ? FString() : F.ToString() + TEXT("/");
		const FName BgTex(*FString::Printf(TEXT("skin:%sgen_bg"), *Fac)), FrameTex(*FString::Printf(TEXT("skin:%sgen_frame"), *Fac));
		const float BX = PX(0), BY = PY(0), BW = BoxW * S, BH = BoxH * S;
		ImageUV(BgTex, BX, BY, BW, BH, FVector2D(0, 0), FVector2D(BoxW / 512.0f, BoxH / 512.0f), FLinearColor::White);
		Rect(BX, BY, BW, BH, Hex(0x040609, 0.45f));
		// a borda de cada facção (cima, baixo, esquerda, direita) e o canto inteiro, em px da arte (art/tools/make_gen_frame.py); no placar a
		// borda sai 0,64 dela pra fora da janela
		float T = 23, Bo = 18, L = 22, Ri = 20, CW = 40, CH = 40;
		if (F == TEXT("china")) { T = 32; Bo = 32; L = 46; Ri = 46; CW = 112; CH = 72; }
		else if (F == TEXT("gla")) { T = 35; Bo = 32; L = 39; Ri = 38; CW = 74; CH = 74; }
		else if (F == TEXT("russia")) { T = 32; Bo = 30; L = 28; Ri = 28; CW = 206; CH = 44; }
		if (UTexture* FT = Lookup ? Lookup(FrameTex) : nullptr)
		{
			const float FW = (BoxW + (L + Ri) * 0.64f) * S, FH = (BoxH + (T + Bo) * 0.64f) * S;
			Skin(FrameTex, BX - L * 0.64f * S, BY - T * 0.64f * S, FW, FH, FMargin(CW, CH), FW / FT->GetSurfaceWidth());
		}
		if (!State.bScoreboardPassive)
		{
			Bg(BX, BY, BW, BH);
		}
	}

	// o cabeçalho: o título e a linha do formato
	const FVector2D TitleE = TextSpaced(TEXT("SCOREBOARD"), PX(44), PY(51), 28.0f, Hex(0xe9eef4), EWeight::Bold, 3.0f);
	{
		const FString Parts[5] = { State.ModeLabel + TEXT(" · "), FString::Printf(TEXT("%d players"), Ps.Num()), TEXT(" · "), State.Clock, FString() };
		float Pen = PX(44) + TitleE.X + 18.0f * S;
		for (int32 I = 0; I < 4; ++I)
		{
			Pen += TextMid(Parts[I], Pen, PY(55), 17.0f, I % 2 ? Hex(0xe9eef4) : Hex(0x9aa9b8), I % 2 ? EWeight::Bold : EWeight::SemiBold, EAlign::Left, false).X;
		}
	}

	// as colunas (da direita pra esquerda a partir da borda de dentro das faixas)
	const float BodyY = 90.0f;
	const float RowsX = 84.0f, RowsW = 1044.0f;
	const float ColR[7] = { 372.0f, 482.0f, 646.0f, 764.0f, 896.0f, 1038.0f, 1112.0f }; // o fim de cada coluna (a do jogador começa em 100)
	const FLinearColor Lab = Hex(0xc8d4e0, 0.6f);
	TextSpaced(TEXT("PLAYER"), PX(100), PY(BodyY + 9), 15.0f, Lab, EWeight::SemiBold, 1.5f);
	const TCHAR* Heads[6] = { TEXT("SCORE"), TEXT("DESTROYED / LOST"), TEXT("ARMY"), TEXT("MONEY"), TEXT("INCOME / MIN"), TEXT("POWER") };
	for (int32 I = 0; I < 6; ++I)
	{
		TextSpaced(Heads[I], PX(ColR[I + 1]), PY(BodyY + 9), 15.0f, Lab, EWeight::SemiBold, 1.5f, EAlign::Right);
	}

	float RowY = BodyY + 24.0f;
	for (const FBlock& B : Blocks)
	{
		RowY += 22.0f;
		if (!bFfa)
		{
			const bool bMine = B.Team == Ps[MeIdx].Team;
			TextMid(FString::FromInt(Teams.Find(B.Team) + 1), PX(44 + 15), PY(RowY + 22), 24.0f, bMine ? Hex(0x8fc6ff, 0.8f) : Hex(0xc8d4e0, 0.3f), EWeight::Bold, EAlign::Center, false);
		}
		for (const int32 I : B.Rows)
		{
			const FRTSHudPlayer& P = Ps[I];
			const float X = PX(RowsX), Y = PY(RowY), W = RowsW * S, H = 42.0f * S, CY = Y + H * 0.5f;
			const float A = P.bDead ? 0.5f : 1.0f;
			if (P.bDead)
			{
				RoundRect(X, Y, W, H, 9 * S, Hex(0x78828c, 0.07f));
				const TPair<float, FLinearColor> Edge[1] = { { 0.0f, Hex(0x78828c, 0.35f) } };
				RoundRect(X, Y, W, H, 9 * S, Edge, X, X + 4 * S);
			}
			else
			{
				const TPair<float, FLinearColor> Stops[3] = { { 0.0f, Alpha(P.Color, 0.36f) }, { 0.38f, Alpha(P.Color, 0.13f) }, { 1.0f, Alpha(P.Color, 0.05f) } };
				RoundRect(X, Y, W, H, 9 * S, Stops);
				const TPair<float, FLinearColor> Edge[1] = { { 0.0f, Alpha(P.Color, 0.95f) } };
				RoundRect(X, Y, W, H, 9 * S, Edge, X, X + 4 * S);
				RoundRectRing(X, Y, W, H, 9 * S, 1.0f * S, Alpha(P.Color, 0.14f));
			}
			// o jogador: a ficha, a patente, o nome, o YOU e o olho do espionado
			Chip(P, FVector2D(PX(100 + 13), CY), 26.0f * S, A);
			if (!P.RankIcon.IsNone())
			{
				Image(P.RankIcon, PX(136), CY - 12 * S, 24 * S, 24 * S, FLinearColor(1, 1, 1, A), false);
			}
			const bool bSpied = P.bSpied && !P.bDead;
			float NameMax = PX(ColR[0]) - PX(170) - (P.bMe ? 42.0f * S : 0.0f) - (bSpied ? 36.0f * S : 0.0f);
			const FVector2D NE = TextMid(Ellipsize(P.Name, 20.0f, EWeight::SemiBold, NameMax), PX(170), CY, 20.0f, Hex(0xf4f7fa, A), EWeight::SemiBold, EAlign::Left, false);
			float Pen = PX(170) + NE.X + 10.0f * S;
			if (P.bMe)
			{
				const float YW = MeasureSpaced(TEXT("YOU"), 12.0f, EWeight::Bold, 1.0f) + 10.0f * S;
				RoundRect(Pen, CY - 8 * S, YW, 16 * S, 4 * S, Hex(0xcfe3f5, A));
				TextSpaced(TEXT("YOU"), Pen + 5 * S, CY, 12.0f, Hex(0x0b0f14, A), EWeight::Bold, 1.0f);
				Pen += YW + 10.0f * S;
			}
			if (bSpied)
			{
				Image(TEXT("skin:spy_badge_eye"), Pen + 2 * S, CY - 12 * S, 24 * S, 24 * S, FLinearColor::White, false);
			}
			if (P.bDead)
			{
				TextSpaced(TEXT("DEFEATED"), PX(ColR[6]), CY, 16.0f, Hex(0x9aa6b2, A), EWeight::SemiBold, 1.5f, EAlign::Right);
				RowY += 46.0f;
				continue;
			}
			const FLinearColor Num = Hex(0xf4f7fa), Spy = Hex(0xffc27a);
			TextMid(Commas(P.Score), PX(ColR[1]), CY, 22.0f, Num, EWeight::SemiBold, EAlign::Right, false);
			{
				const FString Lost = FString::Printf(TEXT("/ %d"), P.Lost);
				const float LW = TextMid(Lost, PX(ColR[2]), CY + 2 * S, 16.0f, Hex(0xdce4ec, 0.6f), EWeight::SemiBold, EAlign::Right, false).X;
				TextMid(FString::FromInt(P.Destroyed), PX(ColR[2]) - LW - 5.0f * S, CY, 22.0f, Num, EWeight::SemiBold, EAlign::Right, false);
			}
			if (P.bMe || P.bAlly || bSpied)
			{
				const FLinearColor C = bSpied ? Spy : Num;
				TextMid(Thousands(P.Army), PX(ColR[3]), CY, 22.0f, C, EWeight::SemiBold, EAlign::Right, false);
				TextMid(TEXT("$") + Short(P.Cash), PX(ColR[4]), CY, 22.0f, C, EWeight::SemiBold, EAlign::Right, false);
				TextMid(TEXT("+") + Short(P.Income), PX(ColR[5]), CY, 22.0f, C, EWeight::SemiBold, EAlign::Right, false);
				const FLinearColor Bolt = P.PowerUsed > P.PowerMade ? Hex(0xff3b2c) : (P.PowerMade - P.PowerUsed < P.PowerMade * 0.1f ? Hex(0xffd23a) : Hex(0x4fd36a));
				Image(TEXT("skin:bolt"), PX(ColR[6]) - 16.7f * S, CY - 10 * S, 20 * S, 20 * S, Bolt, false);
			}
			else
			{
				// a economia escondida: um olho cortado só, no meio das três colunas
				const float EC = PX((ColR[2] + 14.0f + ColR[5]) * 0.5f);
				Image(TEXT("skin:eye_hidden"), EC - 11 * S, CY - 11 * S, 22 * S, 22 * S, Hex(0xc8d4e0, 0.5f), false);
			}
			RowY += 46.0f;
		}
		RowY -= 4.0f;
	}

	// a coluna das superarmas
	if (!bSide)
	{
		return;
	}
	const float SideX = 84.0f - 40.0f + 1084.0f + 26.0f;   // o fio da coluna (a grade termina em 44 + 1084)
	const float ListX = SideX + 24.0f, ListW = 318.0f;
	Rect(PX(SideX), PY(BodyY), FMath::Max(1.0f, S), BodyH * S, Hex(0xc8d4e0, 0.12f));
	int32 Total = 0, TotalReady = 0;
	for (const FSwGroup& G : Gs)
	{
		Total += G.N;
		TotalReady += G.Ready;
	}
	TextSpaced(TEXT("SUPERWEAPONS"), PX(ListX), PY(BodyY + 9), 15.0f, Lab, EWeight::SemiBold, 1.5f);
	{
		float Right = PX(ListX + ListW);
		auto Pill = [&](const FString& Str, const FLinearColor& Fill, const FLinearColor& Ink)
		{
			const float W = Measure(Str, 13.0f, EWeight::Bold).X + 16.0f * S;
			RoundRect(Right - W, PY(BodyY + 1), W, 16.0f * S, 8.0f * S, Fill);
			TextMid(Str, Right - W * 0.5f, PY(BodyY + 9), 13.0f, Ink, EWeight::Bold, EAlign::Center, false);
			Right -= W + 6.0f * S;
		};
		Pill(Short(Total), FLinearColor(1, 1, 1, 0.08f), Hex(0xe9eef4));
		if (TotalReady)
		{
			Pill(FString::Printf(TEXT("%s ready"), *Short(TotalReady)), Hex(0xc8281b), FLinearColor::White);
		}
	}
	const float ListY = BodyY + 34.0f, ListH = BodyH - 34.0f;
	const int32 Fits = FMath::Max(1, FMath::FloorToInt((ListH + 4.0f) / 54.0f));
	const bool bScroll = Gs.Num() > Fits;
	const int32 First = FMath::Clamp(State.ScoreSwScroll, 0, FMath::Max(0, Gs.Num() - Fits));
	const float RowW = bScroll ? 306.0f : 312.0f;
	if (bScroll)
	{
		// a barrinha da rolagem: o trilho e o pedaço que está à vista
		const float TX = PX(ListX + ListW - 3), TY = PY(ListY), TH = ListH * S;
		RoundRect(TX, TY, 3 * S, TH, 1.5f * S, Hex(0xc8d4e0, 0.10f));
		RoundRect(TX, TY + TH * First / Gs.Num(), 3 * S, TH * Fits / Gs.Num(), 1.5f * S, Hex(0xc8d4e0, 0.35f));
	}
	for (int32 R = 0; R < Fits && First + R < Gs.Num(); ++R)
	{
		const FSwGroup& G = Gs[First + R];
		const FRTSHudPlayer& P = Ps[G.Player];
		// a última que cabe sai apagada quando ainda há mais embaixo (o fim esmaecido da lista)
		const float A = bScroll && R == Fits - 1 && First + Fits < Gs.Num() ? 0.35f : 1.0f;
		const float X = PX(ListX), Y = PY(ListY + R * 54.0f), W = RowW * S, H = 50.0f * S, CY = Y + H * 0.5f;
		const bool bHot = !G.Ready && G.Next < 60;
		const TPair<float, FLinearColor> Stops[3] = { { 0.0f, Alpha(P.Color, 0.36f * A) }, { 0.45f, Alpha(P.Color, 0.13f * A) }, { 1.0f, Alpha(P.Color, 0.05f * A) } };
		RoundRect(X, Y, W, H, 9 * S, Stops);
		const TPair<float, FLinearColor> Edge[1] = { { 0.0f, Alpha(P.Color, 0.95f * A) } };
		RoundRect(X, Y, W, H, 9 * S, Edge, X, X + 4 * S);
		RoundRectRing(X, Y, W, H, 9 * S, 1.0f * S, G.Ready ? Hex(0xff4a3a, 0.55f * A) : Alpha(P.Color, 0.14f * A));
		// o desenho no anel de carga
		const FVector2D IC(X + 26.0f * S, CY);
		FillCircle(IC, 15.0f * S, Hex(0x0a0f15, A), Hex(0x0a0f15, A), 32);
		ChargeRing(IC, 15.0f * S, 2.5f * S, G.Ready ? 1.0f : FMath::Max(0.05f, 1.0f - float(G.Next) / G.Full),
			FLinearColor(1, 1, 1, 0.14f * A), Alpha(G.Ready || bHot ? Hex(0xff4a3a) : Light(P.Color, 0.2f), A));
		Image(G.First->Glyph, IC.X - 7.8f * S, CY - 7.8f * S, 15.6f * S, 15.6f * S, Hex(0xeef2f6, A), false);
		// as plaquinhas à direita, uma em cima da outra: a vermelha com o número de prontas, a âmbar com quantas carregam e a próxima
		const int32 Charging = G.N - G.Ready;
		const float PR = X + W - 12.0f * S;
		const float PillsH = (G.Ready ? 19.0f : 0.0f) + (Charging ? 18.0f : 0.0f) + (G.Ready && Charging ? 3.0f : 0.0f);
		float PillY = CY - PillsH * 0.5f * S;
		float PillsW = 0.0f;
		if (G.Ready)
		{
			const FString Nr = Short(G.Ready);
			const float NW = Measure(Nr, 16.0f, EWeight::Bold).X, RW = MeasureSpaced(TEXT("READY"), 10.0f, EWeight::Bold, 1.0f);
			const float W2 = NW + 4.0f * S + RW + 16.0f * S, H2 = 19.0f * S;
			RoundRect(PR - W2 - 3 * S, PillY - 2 * S, W2 + 6 * S, H2 + 4 * S, 8 * S, Hex(0xff3c28, 0.18f * A));
			RoundRect(PR - W2, PillY, W2, H2, 6 * S, Hex(0xc8281b, A));
			TextMid(Nr, PR - W2 + 8 * S, PillY + H2 * 0.5f, 16.0f, Alpha(FLinearColor::White, A), EWeight::Bold, EAlign::Left, false);
			TextSpaced(TEXT("READY"), PR - W2 + 8 * S + NW + 4 * S, PillY + H2 * 0.5f + 1.5f * S, 10.0f, Alpha(FLinearColor::White, A), EWeight::Bold, 1.0f);
			PillY += H2 + 3.0f * S;
			PillsW = W2;
		}
		if (Charging)
		{
			const int32 NextT = G.Ready ? G.After : G.Next;
			const FString Tm = Clock(FMath::Max(0, NextT));
			const FString Cnt = Charging > 1 ? Short(Charging) : FString();
			const float TmW = Measure(Tm, 15.0f, EWeight::Bold).X, CntW = Cnt.IsEmpty() ? 0.0f : Measure(Cnt, 12.0f, EWeight::Bold).X + 8.0f * S;
			const float W2 = TmW + (CntW > 0 ? CntW + 5.0f * S : 0.0f) + 16.0f * S, H2 = 18.0f * S;
			RoundRect(PR - W2, PillY, W2, H2, 6 * S, Hex(0x06090c, 0.75f * A));
			RoundRectRing(PR - W2, PillY, W2, H2, 6 * S, 1.0f * S, bHot ? Hex(0xff503c, 0.6f * A) : Hex(0xffd257, 0.4f * A));
			float Pen = PR - W2 + 8.0f * S;
			if (CntW > 0)
			{
				RoundRect(Pen, PillY + H2 * 0.5f - 7.0f * S, CntW, 14.0f * S, 3.0f * S, Hex(0xffd257, A));
				TextMid(Cnt, Pen + CntW * 0.5f, PillY + H2 * 0.5f, 12.0f, Hex(0x0b0f14, A), EWeight::Bold, EAlign::Center, false);
				Pen += CntW + 5.0f * S;
			}
			TextMid(Tm, Pen, PillY + H2 * 0.5f, 15.0f, bHot ? Hex(0xff8f80, A) : Hex(0xffe08a, A), EWeight::Bold, EAlign::Left, false);
			PillsW = FMath::Max(PillsW, W2);
		}
		// a arma em cima, o dono embaixo na cor dele (com o YOU / ALLY)
		const float TX = X + 54.0f * S, TMax = PR - PillsW - 10.0f * S - TX;
		TextMid(Ellipsize(G.First->Name, 16.0f, EWeight::SemiBold, TMax), TX, CY - 8.5f * S, 16.0f, Hex(0xf4f7fa, A), EWeight::SemiBold, EAlign::Left, false);
		const FString Tag = P.bMe ? TEXT("YOU") : P.bAlly ? TEXT("ALLY") : FString();
		const float TagW = Tag.IsEmpty() ? 0.0f : MeasureSpaced(Tag, 10.0f, EWeight::Bold, 1.0f) + 8.0f * S;
		const FVector2D OE = TextMid(Ellipsize(P.Name, 13.0f, EWeight::SemiBold, TMax - (TagW > 0 ? TagW + 6.0f * S : 0.0f)), TX, CY + 9.0f * S, 13.0f,
			Alpha(Light(P.Color, 0.45f), A), EWeight::SemiBold, EAlign::Left, false);
		if (TagW > 0)
		{
			const float GX = TX + OE.X + 6.0f * S;
			RoundRect(GX, CY + 9.0f * S - 6.5f * S, TagW, 13.0f * S, 3.0f * S, Hex(0xcfe3f5, A));
			TextSpaced(Tag, GX + 4.0f * S, CY + 9.0f * S, 10.0f, Hex(0x0b0f14, A), EWeight::Bold, 1.0f);
		}
	}
}
