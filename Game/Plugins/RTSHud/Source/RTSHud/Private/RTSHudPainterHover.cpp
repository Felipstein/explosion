#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GlobalRenderResources.h"

// O hover de todos os botões (01/11, aprovado: "o resto tá aprovado"; DECISOES §7; o protótipo docs/concept/hud/prototipo/index.html, .hs):
// o botão sobe 3 px e cresce 5% com a mola (0,13 s), ganha a sombra embaixo, acende por dentro na cor do tipo (a luz sobe da base, o fio e o
// halo) e uma luz passa no metal uma vez (0,55 s). Cada facção tem o seu jeito: a USA a luz branca na diagonal; a China a luz de lanterna
// e o brilho de ouro no friso; a GLA a lâmpada que pisca duas vezes e fica, sem luz correndo; a Rússia o scanner frio que desce. Apertado:
// afunda, escurece e pisca o aro branco. Bloqueado: não sobe, só treme. As cores do tipo: a da facção, verde na ação, dourado na melhoria e
// no poder pronto, cinza no bloqueado, vermelho no preço sem dinheiro.

float FRTSHudPainter::Bezier(float X1, float Y1, float X2, float Y2, float T)
{
	T = FMath::Clamp(T, 0.0f, 1.0f);
	auto Coord = [](float A, float B, float U) { const float V = 1.0f - U; return 3.0f * V * V * U * A + 3.0f * V * U * U * B + U * U * U; };
	float U = T;
	for (int32 I = 0; I < 8; ++I)
	{
		const float Xu = Coord(X1, X2, U) - T;
		const float D = 3.0f * (1 - U) * (1 - U) * X1 + 6.0f * (1 - U) * U * (X2 - X1) + 3.0f * U * U * (1 - X2);
		if (FMath::Abs(D) < 1e-5f) break;
		U = FMath::Clamp(U - Xu / D, 0.0f, 1.0f);
	}
	return Coord(Y1, Y2, U);
}

namespace
{
	FLinearColor WithAlpha(FLinearColor C, float A) { C.A = A; return C; }
}

FLinearColor FRTSHudPainter::FactionLight() const
{
	// o --lt do protótipo: a luz de cada facção
	const FName F = Cur ? Cur->SkinFaction : NAME_None;
	if (F == TEXT("china")) return FLinearColor(FColor(240, 72, 50));
	if (F == TEXT("gla")) return FLinearColor(FColor(214, 170, 96));
	if (F == TEXT("russia")) return FLinearColor(FColor(230, 240, 255));
	return FLinearColor(FColor(73, 170, 255));
}

void FRTSHudPainter::GridFill(float X, float Y, float W, float H, int32 Nx, int32 Ny, const FColorAt& ColorAt)
{
	// a cor ponto a ponto numa grade (os degradês que andam: a luz do hover), em triângulos com cor por vértice
	TArray<FCanvasUVTri> List;
	List.Reserve(Nx * Ny * 2);
	for (int32 J = 0; J < Ny; ++J)
	{
		for (int32 I = 0; I < Nx; ++I)
		{
			const FVector2D P00(X + W * I / Nx, Y + H * J / Ny), P10(X + W * (I + 1) / Nx, Y + H * J / Ny);
			const FVector2D P01(X + W * I / Nx, Y + H * (J + 1) / Ny), P11(X + W * (I + 1) / Nx, Y + H * (J + 1) / Ny);
			const FLinearColor C00 = ColorAt(P00), C10 = ColorAt(P10), C01 = ColorAt(P01), C11 = ColorAt(P11);
			if (C00.A + C10.A + C01.A + C11.A <= 0.001f) continue;
			FCanvasUVTri A, B;
			A.V0_Pos = P00; A.V1_Pos = P10; A.V2_Pos = P11; A.V0_Color = C00; A.V1_Color = C10; A.V2_Color = C11;
			B.V0_Pos = P00; B.V1_Pos = P11; B.V2_Pos = P01; B.V0_Color = C00; B.V1_Color = C11; B.V2_Color = C01;
			List.Add(A);
			List.Add(B);
		}
	}
	if (List.Num())
	{
		FCanvasTriangleItem Item(List, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

bool FRTSHudPainter::HoverXform(FName Id, EHoverKind Kind, float& X, float& Y, float& W, float& H, float Corner, bool bContained)
{
	if (!Cur || Id.IsNone() || (Cur->HoverId != Id && Cur->PressedId != Id) || Kind == EHoverKind::Empty)
	{
		return false;
	}
	const bool bPress = Cur->PressedId == Id;
	if (Kind == EHoverKind::Locked)
	{
		// bloqueado: não sobe, só treme (0,28 s) e clareia pouco
		const float T = FMath::Clamp(Cur->HoverAge / 0.28f, 0.0f, 1.0f);
		X += (T < 1.0f ? FMath::Sin(T * 2.0f * PI) * 2.0f * S : 0.0f);
		DrawTint = FLinearColor(1.06f, 1.06f, 1.06f, 1.0f);
		return true;
	}
	if (bContained)
	{
		// (05/11, ele: "as bordas dos cards com hover estão saindo do HUD") nas casas do painel e do cartão o hover fica dentro da casa: não
		// sobe, não cresce e não tem a sombra; só o brilho (e a lâmpada na GLA), a luz por dentro, o fio e a luz que passa (HoverFx)
		const bool bDown = Cur->PressedId == Id;
		const float K0 = FMath::Clamp(Cur->HoverAge / 0.13f, 0.0f, 1.0f);
		const float B0 = bDown ? 0.8f : FMath::Lerp(1.0f, Cur->SkinFaction == FName(TEXT("gla")) ? 1.18f : 1.2f, K0);
		DrawTint = FLinearColor(B0, B0, B0, 1.0f);
		if (Cur->SkinFaction == FName(TEXT("gla")) && !bDown)
		{
			static const float Steps0[4] = { 1.32f, 0.95f, 1.28f, 1.05f };
			const int32 Step0 = FMath::FloorToInt(Cur->HoverAge / 0.08f);
			if (Step0 < 4) DrawTint = FLinearColor(Steps0[Step0], Steps0[Step0] * 0.98f, Steps0[Step0] * 0.92f, 1.0f);
		}
		return true;
	}
	// sobe 3 px e cresce 5% com a mola do protótipo (cubic-bezier(.2, .9, .3, 1.3), 0,13 s); apertado: desce 1 px e encolhe a 97% (0,04 s)
	const float T = bPress ? FMath::Clamp(Cur->PressAge / 0.04f, 0.0f, 1.0f) : FMath::Clamp(Cur->HoverAge / 0.13f, 0.0f, 1.0f);
	const float K = Bezier(0.2f, 0.9f, 0.3f, 1.3f, T);
	const float Scale = bPress ? FMath::Lerp(1.05f, 0.97f, K) : FMath::Lerp(1.0f, 1.05f, K);
	const float Dy = bPress ? FMath::Lerp(-3.0f, 1.0f, K) * S : -3.0f * S * K;
	const float CX = X + W * 0.5f, CY = Y + H * 0.5f;
	W *= Scale; H *= Scale;
	X = CX - W * 0.5f; Y = CY - H * 0.5f + Dy;
	if (!bPress)
	{
		// a sombra embaixo (0 10 px 18 px, preto a 70%): camadas que se espalham
		for (int32 L = 0; L < 5; ++L)
		{
			const float Grow = (2.0f + L * 4.0f) * S;
			RoundRect(X - Grow, Y + 10.0f * S * K - Grow * 0.4f, W + 2 * Grow, H + 2 * Grow * 0.7f, Corner + Grow, FLinearColor(0, 0, 0, 0.14f * K));
		}
	}
	// o brilho do botão (brightness 1.2 no hover; 0.8 apertado): a cor das peças multiplicada
	const float B = bPress ? 0.8f : FMath::Lerp(1.0f, Cur->SkinFaction == FName(TEXT("gla")) ? 1.18f : 1.2f, K);
	DrawTint = FLinearColor(B, B, B, 1.0f);
	if (Cur->SkinFaction == FName(TEXT("gla")) && !bPress)
	{
		// a lâmpada: pisca duas vezes como incandescente (0,32 s em degraus) e fica
		static const float Steps[4] = { 1.32f, 0.95f, 1.28f, 1.05f };
		const int32 Step = FMath::FloorToInt(Cur->HoverAge / 0.08f);
		if (Step < 4) DrawTint = FLinearColor(Steps[Step], Steps[Step] * 0.98f, Steps[Step] * 0.92f, 1.0f);
	}
	return true;
}

void FRTSHudPainter::HoverFx(FName Id, EHoverKind Kind, float X, float Y, float W, float H, float Corner, bool bContained)
{
	DrawTint = FLinearColor::White;
	if (!Cur || Id.IsNone() || (Cur->HoverId != Id && Cur->PressedId != Id))
	{
		return;
	}
	const bool bPress = Cur->PressedId == Id;
	const FName F = Cur->SkinFaction;
	if (Kind == EHoverKind::Empty)
	{
		// a casa vazia só clareia
		RoundRect(X, Y, W, H, Corner, FLinearColor(1, 1, 1, 0.06f));
		return;
	}
	FLinearColor Rim = FactionLight();
	float RimA = 0.95f, Rim2A = 0.35f;
	switch (Kind)
	{
	case EHoverKind::Action: Rim = FLinearColor(FColor(90, 225, 120)); break;
	case EHoverKind::Gold: Rim = FLinearColor(FColor(246, 200, 80)); break;
	case EHoverKind::Locked: Rim = FLinearColor(FColor(150, 160, 175)); RimA = 0.6f; Rim2A = 0.25f; break;
	case EHoverKind::Poor: Rim = FLinearColor(FColor(255, 59, 44)); Rim2A = 0.4f; break;
	default: break;
	}
	const float In = FMath::Clamp(Cur->HoverAge / 0.13f, 0.0f, 1.0f);   // o ::before acende em 0,13 s
	if (bPress)
	{
		// apertado: o aro branco e a luz branca por dentro
		RoundRectRing(X, Y, W, H, Corner, 2.0f * S, FLinearColor(1, 1, 1, 0.9f));
		GridFill(X, Y, W, H, 8, 8, [=, this](const FVector2D& P)
		{
			const float D = FMath::Min(FMath::Min(P.X - X, X + W - P.X), FMath::Min(P.Y - Y, Y + H - P.Y));
			return FLinearColor(1, 1, 1, 0.35f * FMath::Clamp(1.0f - D / (16.0f * S), 0.0f, 1.0f));
		});
		return;
	}
	// o halo por fora (0 0 12 px, a cor a 35%); contido (as casas do painel), sem ele: nada sai da casa
	for (int32 L = 1; L <= (bContained ? 0 : 3); ++L)
	{
		const float G = L * 3.0f * S;
		RoundRectRing(X - G, Y - G, W + 2 * G, H + 2 * G, Corner + G, 3.0f * S, WithAlpha(Rim, Rim2A * In * (0.55f - L * 0.15f)));
	}
	// a luz que sobe da base (inset 0 -18 px 22 px -12 px): forte na beira de baixo, some uns 18 px acima; na GLA, a lâmpada acende o
	// miolo inteiro (inset 0 0 18 px, âmbar)
	if (F == TEXT("gla"))
	{
		const FLinearColor Warm = Kind == EHoverKind::Action ? FLinearColor(FColor(90, 225, 120)) : FLinearColor(FColor(255, 226, 160));
		GridFill(X, Y, W, H, 8, 8, [=, this](const FVector2D& P)
		{
			const float D = FMath::Min(FMath::Min(P.X - X, X + W - P.X), FMath::Min(P.Y - Y, Y + H - P.Y));
			return WithAlpha(Warm, 0.42f * In * FMath::Clamp(1.0f - D / (18.0f * S), 0.0f, 1.0f));
		});
	}
	else
	{
		GridFill(X, Y + H - 22.0f * S, W, 22.0f * S, 1, 6, [=, this](const FVector2D& P)
		{
			const float T = FMath::Clamp((Y + H - P.Y) / (22.0f * S), 0.0f, 1.0f);
			return WithAlpha(Rim, RimA * In * 0.75f * FMath::Square(1.0f - T));
		});
	}
	// o fio por dentro (1 px, a cor a 35%; na China o fio de ouro)
	RoundRectRing(X, Y, W, H, Corner, FMath::Max(1.0f, S), F == TEXT("china") ? FLinearColor(0.92f, 0.58f, 0.08f, 0.8f * In) : WithAlpha(Rim, Rim2A * In * 2.0f));
	// a luz que passa no metal uma vez: na diagonal (USA branca, China dourada, 0,55 s) ou de cima pra baixo (Rússia, 0,5 s); a GLA não tem
	if (F == TEXT("gla") || Kind == EHoverKind::Locked)
	{
		return;
	}
	const bool bScan = F == TEXT("russia");
	const float Dur = bScan ? 0.5f : 0.55f;
	const float T = FMath::Clamp(Cur->HoverAge / Dur, 0.0f, 1.0f);
	if (T >= 1.0f)
	{
		return;
	}
	const float E = Bezier(0.0f, 0.0f, 0.58f, 1.0f, T);   // ease-out
	const FLinearColor Glint = F == TEXT("china") ? WithAlpha(FLinearColor(FColor(255, 214, 120)), 0.55f) : bScan ? WithAlpha(FLinearColor(FColor(235, 245, 255)), 0.6f) : FLinearColor(1, 1, 1, 0.38f);
	if (bScan)
	{
		// o scanner (o gradiente de 300% de altura indo de -110% a 210%): pela conta do background-position, a faixa atravessa de baixo pra
		// cima, que é o que ele aprovou vendo
		const float Pp = FMath::Lerp(-1.1f, 2.1f, E);
		const float Cy = Y + H * (1.5f - 2.0f * Pp), Half = H * 0.24f;
		GridFill(X, Y, W, H, 1, 24, [=, this](const FVector2D& P)
		{
			return WithAlpha(Glint, Glint.A * FMath::Clamp(1.0f - FMath::Abs(P.Y - Cy) / Half, 0.0f, 1.0f));
		});
		return;
	}
	// a diagonal a 110° (o gradiente de 260% de largura de 130% a -30%): a faixa estreita atravessa da esquerda pra direita
	const float Ang = FMath::DegreesToRadians(110.0f);
	const FVector2D Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
	const float Wg = 2.6f * W, Pp = FMath::Lerp(1.3f, -0.3f, E);
	const FVector2D Cimg(X + W * (1.3f - 1.6f * Pp), Y + H * 0.5f);
	const float Len = FMath::Abs(Wg * FMath::Sin(Ang)) + FMath::Abs(H * FMath::Cos(Ang));
	GridFill(X, Y, W, H, 24, 4, [=, this](const FVector2D& P)
	{
		const float D = FVector2D::DotProduct(P - Cimg, Dir);
		return WithAlpha(Glint, Glint.A * FMath::Clamp(1.0f - FMath::Abs(D) / (0.12f * Len), 0.0f, 1.0f));
	});
}
