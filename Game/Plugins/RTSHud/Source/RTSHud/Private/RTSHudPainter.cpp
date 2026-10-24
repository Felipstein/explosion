#include "RTSHudPainter.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GlobalRenderResources.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "TextureResource.h"

// As cores em sRGB (como na planta e nas peças aprovadas); FLinearColor(FColor) converte pra linear.
namespace RTSHudColors
{
	const FLinearColor KFill(FColor(13, 16, 21, 240));
	const FLinearColor KEdge(FColor(84, 94, 108));
	const FLinearColor KEdgeDark(FColor(4, 5, 7));
	const FLinearColor KHighlight(FColor(150, 160, 172, 150));
	const FLinearColor KRivet(FColor(118, 128, 142));
	const FLinearColor KSlotFill(FColor(24, 30, 38, 255));
	const FLinearColor KSlotEmpty(FColor(9, 12, 16, 215));
	const FLinearColor KSlotEmptyEdge(FColor(48, 56, 66));
	const FLinearColor KBlue(FColor(77, 143, 224));
	const FLinearColor KBlueFill(FColor(24, 46, 80, 242));
	const FLinearColor KRed(FColor(224, 68, 58));
	const FLinearColor KAmber(FColor(240, 160, 32));
	const FLinearColor KGreen(FColor(57, 196, 90));
	const FLinearColor KGold(FColor(217, 169, 58));
	const FLinearColor KGoldText(FColor(242, 209, 107));
	const FLinearColor KOrange(FColor(201, 115, 58));
	const FLinearColor KTextMain(FColor(233, 238, 244));
	const FLinearColor KTextSub(FColor(154, 166, 180));
	const FLinearColor KTextDim(FColor(94, 104, 116));
	const FLinearColor KKey(FColor(255, 215, 106));
	const FLinearColor KNavy(FColor(20, 36, 58, 250));
}
using namespace RTSHudColors;

// A pele: as molduras do kit (docs/concept/hud/kit/skin/, recortadas por art/tools/cut_hud_skin.py) e as
// margens de cada uma em pixels da textura (o que não estica: os cantos, os rebites, a ponta do aviso).
namespace RTSHudSkin
{
	const FName SkTopBar(TEXT("skin:top_bar_1v1"));             // 1087×140
	const FName SkSuperweaponBox(TEXT("skin:superweapon_box")); // 427×69
	const FName SkMinimapPod(TEXT("skin:minimap_pod"));         // 706×620, o losango vazado
	const FName SkUnitCard(TEXT("skin:unit_card"));             // 786×480, a janela do retrato vazada
	const FName SkSlotNormal(TEXT("skin:slot_normal"));         // 198×189
	const FName SkSlotAction(TEXT("skin:slot_action"));
	const FName SkSlotCaptain(TEXT("skin:slot_captain"));
	const FName SkSlotLocked(TEXT("skin:slot_locked"));
	const FName SkGroupOn(TEXT("skin:group_on"));               // 231×102
	const FName SkGroupOff(TEXT("skin:group_off"));             // 229×102
	const FName SkGroupSoon(TEXT("skin:group_soon"));           // a azul com o neon vermelho
	const FName SkAlert(TEXT("skin:alert"));                    // 932×134
	const FName SkAlertAmber(TEXT("skin:alert_amber"));
	const FName SkAlertIcon(TEXT("skin:alert_icon_red"));       // 128×112, liso (art/tools/make_alert_icons.py)
	const FName SkAlertIconAmber(TEXT("skin:alert_icon_amber"));
	const FName SkSpyRing(TEXT("skin:spy_badge_ring"));         // 64×64, liso (art/tools/make_hud_glyphs.py)
	const FName SkSpyEye(TEXT("skin:spy_badge_eye"));
	const FName SkDefeatedX(TEXT("skin:defeated_x"));
	const FName SkRadiation(TEXT("skin:radiation"));            // 64×64, o trevo (art/tools/make_hud_glyphs.py)
	const FName SkRadiationRed(TEXT("skin:radiation_red"));
	const FName SkPower(TEXT("skin:power"));                    // 201×274
	const FName SkPanelWide(TEXT("skin:panel_wide"));           // 928×223
	const FName SkPanelLong(TEXT("skin:panel_long"));           // 1626×214
	const FName SkTimerGlow(TEXT("skin:timer_glow"));           // 680×420, a luz azul atrás do medalhão do relógio (art/tools/make_timer_medallion.py)

	const FMargin SkMSlot(40.0f);
	const FMargin SkMGroup(34.0f, 30.0f);
	const FMargin SkMSuperweapon(40.0f, 16.0f);
	const FMargin SkMPanelWide(50.0f, 40.0f);
	const FMargin SkMPanelLong(100.0f, 36.0f);
	const FMargin SkMAlert(100.0f, 0.0f, 200.0f, 0.0f);
	// o encaixe da vida no cartão: o rebaixo que serve de fundo pra toda barra
	const FBox2D SkCardHealthSlot(FVector2D(37, 388), FVector2D(502, 435));
}
using namespace RTSHudSkin;

namespace
{
	FLinearColor WithA(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }
}

FRTSHudPainter::FRTSHudPainter(FTextureLookup InLookup, const FString& FontDir)
	: Lookup(MoveTemp(InLookup))
{
	const TCHAR* Files[3] = { TEXT("BarlowSemiCondensed-Medium.ttf"), TEXT("BarlowSemiCondensed-SemiBold.ttf"), TEXT("BarlowSemiCondensed-Bold.ttf") };
	for (int32 I = 0; I < 3; ++I)
	{
		Fonts[I] = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), FontDir / Files[I], EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		UE_CLOG(!FPaths::FileExists(FontDir / Files[I]), LogTemp, Error, TEXT("RTSHud: fonte que falta: %s"), *(FontDir / Files[I]));
	}
}

// ---------------------------------------------------------------------------------------------
// planta → tela

void FRTSHudPainter::Layout(float ClipX, float ClipY, float& OutS, float& OutLeft, float& OutWidth)
{
	OutS = FMath::Min(ClipY / 1080.0f, ClipX / 1920.0f);
	OutWidth = FMath::Min(ClipX, ClipY * 16.0f / 9.0f);
	OutWidth = FMath::Max(OutWidth, 1920.0f * OutS);
	OutLeft = (ClipX - OutWidth) * 0.5f;
}

void FRTSHudPainter::MinimapHole(float ClipX, float ClipY, FVector2D& OutCenter, float& OutHalfDiagonal)
{
	// as mesmas contas do DrawMinimap: a peça 706×620 em 0,534·S, presa no canto de baixo à esquerda
	float Sc, Left, Width;
	Layout(ClipX, ClipY, Sc, Left, Width);
	const float K = 0.534f * Sc;
	const float X = Left + 8 * Sc, Y = ClipY - 8 * Sc - 620 * K;
	OutCenter = FVector2D(X + 311.5f * K, Y + 302.5f * K);
	OutHalfDiagonal = 266.0f * K;
}

float FRTSHudPainter::SX(float X, EAnchor A) const
{
	switch (A)
	{
	case EAnchor::Center: return LayoutLeft + LayoutWidth * 0.5f + (X - 960.0f) * S;
	case EAnchor::Right:  return LayoutLeft + LayoutWidth - (1920.0f - X) * S;
	default:              return LayoutLeft + X * S;
	}
}

float FRTSHudPainter::SY(float Y, bool bBottom) const
{
	return bBottom ? Canvas->ClipY - (1080.0f - Y) * S : Y * S;
}

// ---------------------------------------------------------------------------------------------
// primitivas

void FRTSHudPainter::Rect(float X, float Y, float W, float H, const FLinearColor& C)
{
	FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), C);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::Border(float X, float Y, float W, float H, float T, const FLinearColor& C)
{
	Rect(X, Y, W, T, C);
	Rect(X, Y + H - T, W, T, C);
	Rect(X, Y + T, T, H - 2 * T, C);
	Rect(X + W - T, Y + T, T, H - 2 * T, C);
}

void FRTSHudPainter::DashedBorder(float X, float Y, float W, float H, float T, float Dash, const FLinearColor& C)
{
	for (float P = 0; P < W; P += Dash * 2)
	{
		const float L = FMath::Min(Dash, W - P);
		Rect(X + P, Y, L, T, C);
		Rect(X + P, Y + H - T, L, T, C);
	}
	for (float P = 0; P < H; P += Dash * 2)
	{
		const float L = FMath::Min(Dash, H - P);
		Rect(X, Y + P, T, L, C);
		Rect(X + W - T, Y + P, T, L, C);
	}
}

void FRTSHudPainter::Line(FVector2D A, FVector2D B, float T, const FLinearColor& C)
{
	FCanvasLineItem Item(A, B);
	Item.LineThickness = T;
	Item.SetColor(C);
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::Image(FName Name, float X, float Y, float W, float H, const FLinearColor& Tint, bool bCover)
{
	UTexture* Tex = Lookup ? Lookup(Name) : nullptr;
	if (!Tex || !Tex->GetResource())
	{
		Rect(X, Y, W, H, FLinearColor(FColor(40, 34, 28)));
		return;
	}
	FVector2D UV0(0, 0), UV1(1, 1);
	if (bCover)
	{
		// recorta a imagem pra encher a caixa sem esticar
		const float TA = Tex->GetSurfaceWidth() / FMath::Max(1.0f, Tex->GetSurfaceHeight());
		const float BA = W / FMath::Max(1.0f, H);
		if (TA > BA)
		{
			const float U = BA / TA;
			UV0.X = (1 - U) * 0.5f; UV1.X = 1 - UV0.X;
		}
		else if (TA < BA)
		{
			const float V = TA / BA;
			UV0.Y = (1 - V) * 0.5f; UV1.Y = 1 - UV0.Y;
		}
	}
	FCanvasTileItem Item(FVector2D(X, Y), Tex->GetResource(), FVector2D(W, H), UV0, UV1, Tint);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::ImageInset(FName Name, float X, float Y, float W, float H, float Inset, const FLinearColor& Tint)
{
	UTexture* Tex = Lookup ? Lookup(Name) : nullptr;
	if (!Tex || !Tex->GetResource())
	{
		Rect(X, Y, W, H, FLinearColor(FColor(40, 34, 28)));
		return;
	}
	FCanvasTileItem Item(FVector2D(X, Y), Tex->GetResource(), FVector2D(W, H), FVector2D(Inset, Inset), FVector2D(1 - Inset, 1 - Inset), Tint);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::Skin(FName Name, float X, float Y, float W, float H, const FMargin& M, float Scale, const FLinearColor& Tint, const FBox2D& SrcIn)
{
	UTexture* Tex = nullptr;
	if (Lookup && Cur && !Cur->SkinFaction.IsNone())
	{
		// a pele da facção (skin:<facção>/<peça>), quando a peça existe; senão a da USA
		FString Piece = Name.ToString();
		if (Piece.RemoveFromStart(TEXT("skin:")))
		{
			Tex = Lookup(FName(*FString::Printf(TEXT("skin:%s/%s"), *Cur->SkinFaction.ToString(), *Piece)));
		}
	}
	if (!Tex)
	{
		Tex = Lookup ? Lookup(Name) : nullptr;
	}
	if (!Tex || !Tex->GetResource() || W <= 0.0f || H <= 0.0f)
	{
		return;
	}
	const float TW = Tex->GetSurfaceWidth(), TH = Tex->GetSurfaceHeight();
	const FBox2D Src = SrcIn.bIsValid ? SrcIn : FBox2D(FVector2D(0, 0), FVector2D(TW, TH));
	// as margens na tela; se não couberem na caixa, encolhem juntas
	float ML = M.Left * Scale, MR = M.Right * Scale, MT = M.Top * Scale, MB = M.Bottom * Scale;
	if (ML + MR > W)
	{
		const float Fit = W / (ML + MR); ML *= Fit; MR *= Fit;
	}
	if (MT + MB > H)
	{
		const float Fit = H / (MT + MB); MT *= Fit; MB *= Fit;
	}
	const float Xs[4] = { X, X + ML, X + W - MR, X + W };
	const float Ys[4] = { Y, Y + MT, Y + H - MB, Y + H };
	const float Us[4] = { float(Src.Min.X) / TW, float(Src.Min.X + M.Left) / TW, float(Src.Max.X - M.Right) / TW, float(Src.Max.X) / TW };
	const float Vs[4] = { float(Src.Min.Y) / TH, float(Src.Min.Y + M.Top) / TH, float(Src.Max.Y - M.Bottom) / TH, float(Src.Max.Y) / TH };
	TArray<FCanvasUVTri> List;
	auto AddTri = [&](const FVector2D& P0, const FVector2D& U0, const FVector2D& P1, const FVector2D& U1, const FVector2D& P2, const FVector2D& U2)
	{
		FCanvasUVTri Tr;
		Tr.V0_Pos = P0; Tr.V0_UV = U0; Tr.V0_Color = Tint;
		Tr.V1_Pos = P1; Tr.V1_UV = U1; Tr.V1_Color = Tint;
		Tr.V2_Pos = P2; Tr.V2_UV = U2; Tr.V2_Color = Tint;
		List.Add(Tr);
	};
	for (int32 J = 0; J < 3; ++J)
	{
		for (int32 I = 0; I < 3; ++I)
		{
			if (Xs[I + 1] - Xs[I] < 0.01f || Ys[J + 1] - Ys[J] < 0.01f)
			{
				continue;
			}
			const FVector2D P00(Xs[I], Ys[J]), P10(Xs[I + 1], Ys[J]), P01(Xs[I], Ys[J + 1]), P11(Xs[I + 1], Ys[J + 1]);
			const FVector2D U00(Us[I], Vs[J]), U10(Us[I + 1], Vs[J]), U01(Us[I], Vs[J + 1]), U11(Us[I + 1], Vs[J + 1]);
			AddTri(P00, U00, P10, U10, P11, U11);
			AddTri(P00, U00, P11, U11, P01, U01);
		}
	}
	if (List.Num() > 0)
	{
		FCanvasTriangleItem Item(List, Tex->GetResource());
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

void FRTSHudPainter::BarFill(float X, float Y, float W, float H, const FLinearColor& C)
{
	if (W < 0.5f || H < 0.5f)
	{
		return;
	}
	const FLinearColor Top = FMath::Lerp(C, FLinearColor::White, 0.30f);
	const FLinearColor Bot = FLinearColor(C.R * 0.62f, C.G * 0.62f, C.B * 0.62f, 1.0f);
	FillPoly({ {X, Y}, {X + W, Y}, {X + W, Y + H}, {X, Y + H} }, [=](const FVector2D& P)
	{
		FLinearColor O = FMath::Lerp(Top, Bot, FMath::Clamp((P.Y - Y) / H, 0.0f, 1.0f));
		O.A = 1.0f;
		return O;
	});
	Rect(X, Y, W, FMath::Max(1.0f, H * 0.18f), FLinearColor(1, 1, 1, 0.22f));
}

void FRTSHudPainter::Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C)
{
	// o rebaixo é o encaixe da vida do cartão (a mesma pele), na altura da barra
	Skin(SkUnitCard, X, Y, W, H, FMargin(10.0f), H / 47.0f, FLinearColor::White, SkCardHealthSlot);
	const float In = FMath::Max(2.0f, H * 0.22f);
	BarFill(X + In, Y + In, (W - 2 * In) * FMath::Clamp(Fill, 0.0f, 1.0f), H - 2 * In, C);
}

void FRTSHudPainter::Radiation(FVector2D C, float R)
{
	// o símbolo radioativo: o disco amarelo com o miolo e as três divisões escuras
	FillCircle(C, R, KGold, KGold, 20);
	FillCircle(C, R * 0.3f, FLinearColor::Black, FLinearColor::Black, 12);
	for (int32 B = 0; B < 3; ++B)
	{
		const float A = -PI * 0.5f + B * 2 * PI / 3 + PI / 3;
		const FVector2D D(FMath::Cos(A), FMath::Sin(A));
		Line(C + D * R * 0.42f, C + D * R, R * 0.28f, FLinearColor::Black);
	}
}

void FRTSHudPainter::Tris(const TArray<FVector2D>& Pts, const FLinearColor& C)
{
	TArray<FCanvasUVTri> List;
	for (int32 I = 0; I + 2 < Pts.Num(); I += 3)
	{
		FCanvasUVTri T;
		T.V0_Pos = Pts[I]; T.V1_Pos = Pts[I + 1]; T.V2_Pos = Pts[I + 2];
		T.V0_Color = T.V1_Color = T.V2_Color = C;
		List.Add(T);
	}
	if (List.Num() == 0)
	{
		return;
	}
	FCanvasTriangleItem Item(List, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::Sweep(float X, float Y, float W, float H, float Done, float Alpha)
{
	// o carregamento do C&C: um fundo escuro por cima do ícone que some no sentido do relógio, a partir
	// das 12 horas; Done é o que já carregou (0..1)
	Done = FMath::Clamp(Done, 0.0f, 1.0f);
	if (Done >= 1.0f)
	{
		return;
	}
	const FVector2D C(X + W * 0.5f, Y + H * 0.5f);
	const float A0 = Done * 2 * PI;
	const int32 N = FMath::Max(2, FMath::CeilToInt((1 - Done) * 96));
	auto EdgePoint = [&](float A)
	{
		const FVector2D D(FMath::Sin(A), -FMath::Cos(A));
		const float T = FMath::Min(W * 0.5f / FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(D.X)), H * 0.5f / FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(D.Y)));
		return C + D * T;
	};
	TArray<FVector2D> P;
	for (int32 I = 0; I < N; ++I)
	{
		const float Ta = A0 + (2 * PI - A0) * I / N;
		const float Tb = A0 + (2 * PI - A0) * (I + 1) / N;
		P.Add(C); P.Add(EdgePoint(Ta)); P.Add(EdgePoint(Tb));
	}
	Tris(P, FLinearColor(0, 0, 0, Alpha));
}

void FRTSHudPainter::Star(FVector2D C, float R, const FLinearColor& Col)
{
	TArray<FVector2D> Pts;
	for (int32 I = 0; I < 10; ++I)
	{
		const float A0 = -PI * 0.5f + I * PI / 5;
		const float A1 = -PI * 0.5f + (I + 1) * PI / 5;
		const float R0 = (I % 2 == 0) ? R : R * 0.45f;
		const float R1 = (I % 2 == 0) ? R * 0.45f : R;
		Pts.Add(C);
		Pts.Add(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R0);
		Pts.Add(C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R1);
	}
	Tris(Pts, Col);
}

TArray<FVector2D> FRTSHudPainter::ChamferPoly(float X, float Y, float W, float H, float C)
{
	C = FMath::Min(C, FMath::Min(W, H) * 0.45f);
	return { {X + C, Y}, {X + W - C, Y}, {X + W, Y + C}, {X + W, Y + H - C}, {X + W - C, Y + H}, {X + C, Y + H}, {X, Y + H - C}, {X, Y + C} };
}

void FRTSHudPainter::FillPoly(const TArray<FVector2D>& Poly, const FColorAt& ColorAt)
{
	// leque a partir do centro (os polígonos do HUD são convexos), com cor por vértice: o degradê
	if (Poly.Num() < 3)
	{
		return;
	}
	FVector2D Ctr(0, 0);
	for (const FVector2D& P : Poly) Ctr += P;
	Ctr /= Poly.Num();
	const FLinearColor CC = ColorAt(Ctr);
	TArray<FCanvasUVTri> List;
	for (int32 I = 0; I < Poly.Num(); ++I)
	{
		const FVector2D& A = Poly[I];
		const FVector2D& B = Poly[(I + 1) % Poly.Num()];
		FCanvasUVTri T;
		T.V0_Pos = Ctr; T.V1_Pos = A; T.V2_Pos = B;
		T.V0_Color = CC; T.V1_Color = ColorAt(A); T.V2_Color = ColorAt(B);
		List.Add(T);
	}
	FCanvasTriangleItem Item(List, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::FillCircle(FVector2D C, float R, const FLinearColor& Inner, const FLinearColor& Outer, int32 N)
{
	TArray<FCanvasUVTri> List;
	for (int32 I = 0; I < N; ++I)
	{
		const float A0 = 2 * PI * I / N, A1 = 2 * PI * (I + 1) / N;
		FCanvasUVTri T;
		T.V0_Pos = C; T.V1_Pos = C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R; T.V2_Pos = C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R;
		T.V0_Color = Inner; T.V1_Color = Outer; T.V2_Color = Outer;
		List.Add(T);
	}
	FCanvasTriangleItem Item(List, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FRTSHudPainter::Plate(float X, float Y, float W, float H, float C, const FLinearColor& Rim, const FLinearColor& BodyTop, const FLinearColor& BodyBottom, bool bRivets)
{
	// 1. a linha escura de fora, 2. o aro de metal (claro em cima, escuro embaixo), 3. o corpo em degradê,
	// 4. o brilho fino logo abaixo do aro de cima, 5. os rebites
	const float T = FMath::Max(2.0f, 3.0f * S), O = FMath::Max(1.0f, 1.0f * S);
	FillPoly(ChamferPoly(X - O, Y - O, W + 2 * O, H + 2 * O, C + O), [](const FVector2D&) { return FLinearColor(0.0f, 0.0f, 0.0f, 0.85f); });
	const FLinearColor RimTop = FMath::Lerp(Rim, FLinearColor::White, 0.35f);
	const FLinearColor RimBot = Rim * 0.45f;
	FillPoly(ChamferPoly(X, Y, W, H, C), [=](const FVector2D& P)
	{
		const float K = FMath::Clamp((P.Y - Y) / FMath::Max(1.0f, H), 0.0f, 1.0f);
		FLinearColor Out = FMath::Lerp(RimTop, RimBot, K); Out.A = 1.0f; return Out;
	});
	FillPoly(ChamferPoly(X + T, Y + T, W - 2 * T, H - 2 * T, FMath::Max(0.0f, C - T * 0.6f)), [](const FVector2D&) { return FLinearColor(0.004f, 0.005f, 0.007f, 1.0f); });
	const float I2 = T + O;
	FillPoly(ChamferPoly(X + I2, Y + I2, W - 2 * I2, H - 2 * I2, FMath::Max(0.0f, C - I2 * 0.6f)), [=](const FVector2D& P)
	{
		const float K = FMath::Clamp((P.Y - Y) / FMath::Max(1.0f, H), 0.0f, 1.0f);
		return FMath::Lerp(BodyTop, BodyBottom, K);
	});
	Rect(X + I2 + C * 0.5f, Y + I2, W - 2 * I2 - C, O, FLinearColor(1, 1, 1, 0.10f));
	if (bRivets && W > 90 * S && H > 70 * S)
	{
		const float In = T + 5.0f * S, R = 2.2f * S;
		for (const FVector2D& P : { FVector2D(X + In + C * 0.4f, Y + In), FVector2D(X + W - In - C * 0.4f, Y + In), FVector2D(X + In + C * 0.4f, Y + H - In), FVector2D(X + W - In - C * 0.4f, Y + H - In) })
		{
			FillCircle(P, R + 0.8f * S, FLinearColor(0, 0, 0, 0.8f), FLinearColor(0, 0, 0, 0.8f), 12);
			FillCircle(P, R, FLinearColor(FColor(168, 178, 190)), FLinearColor(FColor(70, 78, 90)), 12);
		}
	}
}

void FRTSHudPainter::EmblemRing(FName Emblem, FVector2D C, float R, const FLinearColor& Rim)
{
	FillCircle(C, R + 2 * S, FLinearColor(0, 0, 0, 0.9f), FLinearColor(0, 0, 0, 0.9f));
	FillCircle(C, R, FMath::Lerp(Rim, FLinearColor::White, 0.45f), Rim * 0.5f);
	FillCircle(C, R - 4 * S, FLinearColor(FColor(30, 38, 52)), FLinearColor(FColor(8, 10, 14)));
	Image(Emblem, C.X - (R - 5 * S), C.Y - (R - 5 * S), 2 * (R - 5 * S), 2 * (R - 5 * S), FLinearColor::White, false);
}

void FRTSHudPainter::Frame(float X, float Y, float W, float H, const FLinearColor& EdgeColor, float FillAlpha)
{
	// o painel padrão: metal escuro chanfrado (o acabamento das peças aprovadas)
	Plate(X, Y, W, H, 7.0f * S, EdgeColor, FLinearColor(FColor(30, 37, 46, uint8(255 * FillAlpha))), FLinearColor(FColor(11, 14, 18, uint8(255 * FillAlpha))), true);
}

FSlateFontInfo FRTSHudPainter::Font(float Px, EWeight W) const
{
	// o tamanho da Slate é em pontos (96 dpi): 1 px = 0,75 pt
	return FSlateFontInfo(Fonts[int32(W)], FMath::Max(4.0f, Px * S * 0.75f), FName(TEXT("Regular")));
}

FVector2D FRTSHudPainter::Measure(const FString& Str, float Px, EWeight W) const
{
	if (!FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	// a medida do texto é o que mais pesa no HUD (o encolher pra caber mede várias vezes): guarda por quadro
	// e entre quadros; o tamanho na tela entra na chave, então trocar de resolução não confunde
	const float Dpi = Canvas ? Canvas->GetDPIScale() : 1.0f;
	const FString Key = FString::Printf(TEXT("%d|%d|%d|%s"), FMath::RoundToInt(Px * S * 100.0f), int32(W), FMath::RoundToInt(Dpi * 100.0f), *Str);
	if (const FVector2D* Found = MeasureCache.Find(Key))
	{
		return *Found;
	}
	if (MeasureCache.Num() > 4096)
	{
		MeasureCache.Reset();
	}
	const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D E = M->Measure(Str, Font(Px, W), Dpi) / Dpi;
	MeasureCache.Add(Key, E);
	return E;
}

FVector2D FRTSHudPainter::Text(const FString& Str, float X, float Y, float Px, const FLinearColor& C, EWeight W, EAlign Al, bool bShadow)
{
	const FVector2D E = Measure(Str, Px, W);
	float DX = X;
	if (Al == EAlign::Center) DX = X - E.X * 0.5f;
	if (Al == EAlign::Right) DX = X - E.X;
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(DX), FMath::RoundToFloat(Y)), FText::FromString(Str), Font(Px, W), C);
	// o Canvas só desenha texto com um UFont de cache em tempo real (CanvasItem.cpp: HasValidText,
	// GetFontCacheType); a fonte de verdade é a da FSlateFontInfo (a Barlow), que tem precedência
	Item.Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0, 0, 0, 0.7f * C.A), FVector2D(FMath::Max(1.0f, FMath::RoundToFloat(1.0f * S))));
	}
	Canvas->DrawItem(Item);
	return E;
}

FVector2D FRTSHudPainter::TextMid(const FString& Str, float X, float CY, float Px, const FLinearColor& C, EWeight W, EAlign Al, bool bShadow)
{
	return Text(Str, X, CY - Measure(Str, Px, W).Y * 0.5f, Px, C, W, Al, bShadow);
}

float FRTSHudPainter::PanelDrop(const FRTSHudState& State) const
{
	// o painel sobe da borda de baixo quando algo é selecionado (PanelIn 0 → 1, desacelerando)
	const float In = FMath::Clamp(State.PanelIn, 0.0f, 1.0f);
	return (1.0f - (1.0f - FMath::Pow(1.0f - In, 3.0f))) * (CommandPanelHeight(State) + 70.0f) * S;
}

FLinearColor FRTSHudPainter::Accent() const
{
	// a cor do neon da pele: azul na USA, vermelho na China, âmbar no GLA
	const FName F = Cur ? Cur->SkinFaction : NAME_None;
	if (F == TEXT("china")) return FLinearColor(FColor(232, 60, 52));
	if (F == TEXT("gla")) return FLinearColor(FColor(236, 168, 52));
	return KBlue;
}

float FRTSHudPainter::Pulse(float Hz) const
{
	return 0.5f + 0.5f * FMath::Sin((Cur ? Cur->Time : 0.0f) * Hz * 2.0f * PI);
}

void FRTSHudPainter::HoverPress(FName Id, float X, float Y, float W, float H, const FLinearColor& Rim)
{
	if (!Cur || Id.IsNone())
	{
		return;
	}
	if (Cur->PressedId == Id)
	{
		// apertado: afunda (sombra por dentro em cima e à esquerda) e escurece
		FillPoly(ChamferPoly(X, Y, W, H, 5 * S), [](const FVector2D&) { return FLinearColor(0.0f, 0.0f, 0.0f, 0.28f); });
		Rect(X + 3 * S, Y + 3 * S, W - 6 * S, 2 * S, FLinearColor(0, 0, 0, 0.55f));
		Rect(X + 3 * S, Y + 3 * S, 2 * S, H - 6 * S, FLinearColor(0, 0, 0, 0.55f));
	}
	else if (Cur->HoverId == Id)
	{
		// o mouse em cima: um véu claro e a moldura acesa na cor do aro
		FillPoly(ChamferPoly(X, Y, W, H, 5 * S), [](const FVector2D&) { return FLinearColor(1.0f, 1.0f, 1.0f, 0.07f); });
		const FLinearColor Lit = FMath::Lerp(Rim, FLinearColor::White, 0.45f);
		const TArray<FVector2D> Pts = ChamferPoly(X + 1 * S, Y + 1 * S, W - 2 * S, H - 2 * S, 5 * S);
		for (int32 K = 0; K < Pts.Num(); ++K)
		{
			Line(Pts[K], Pts[(K + 1) % Pts.Num()], 3.5f * S, WithA(Lit, 0.30f));
			Line(Pts[K], Pts[(K + 1) % Pts.Num()], 1.5f * S, WithA(Lit, 0.95f));
		}
	}
}

FLinearColor FRTSHudPainter::HealthColor(float H)
{
	return H > 0.6f ? KGreen : (H > 0.3f ? KAmber : KRed);
}

// ---------------------------------------------------------------------------------------------

void FRTSHudPainter::Draw(UCanvas* InCanvas, const FRTSHudState& State, TArray<FRTSHudButton>& OutButtons)
{
	if (!InCanvas)
	{
		return;
	}
	Canvas = InCanvas;
	Cur = &State;
	Buttons = &OutButtons;
	OutButtons.Reset();
	SlotBoxes.Reset();
	BgBoxes.Reset();
	Layout(Canvas->ClipX, Canvas->ClipY, S, LayoutLeft, LayoutWidth);

	DrawMinimap(State);
	DrawCommandPanel(State);
	DrawGroups(State);
	DrawCard(State);
	DrawAlerts(State);
	DrawPowers(State);
	DrawTop(State);
	DrawCommander(State);
	DrawTooltip(State);
	// o fundo de cada peça segura o clique (não vai pro mundo); vem no fim, depois dos botões de verdade:
	// quem procura o botão debaixo do mouse pega o primeiro da lista que contém o ponto
	for (const FBox2D& Box : BgBoxes)
	{
		OutButtons.Add({ TEXT("hud_bg"), Box });
	}
	Canvas = nullptr;
	Cur = nullptr;
	Buttons = nullptr;
}

// ---------------------------------------------------------------------------------------------
// topo: o relógio no centro, metade dos jogadores de cada lado; você e os aliados num bloco azul só

void FRTSHudPainter::DrawTop(const FRTSHudState& State)
{
	const int32 N = State.Players.Num();
	if (N == 0)
	{
		return;
	}
	if (N == 2)
	{
		DrawTop1v1(State);
		return;
	}
	const float CW = 136.0f, CH = 58.0f, TW = 104.0f, ClockX = 960.0f - TW * 0.5f;
	const int32 Half = (N + 1) / 2;
	const float LeftStart = ClockX - Half * CW;

	int32 TeamN = 0;
	for (int32 I = 0; I < Half; ++I)
	{
		if (State.Players[I].bMe || State.Players[I].bAlly) ++TeamN;
	}
	if (TeamN > 0)
	{
		// o seu time: uma tecla azul da pele (group_on) que abraça você e os aliados
		const float X = SX(LeftStart - 2, EAnchor::Center), Y = SY(2, false);
		Skin(SkGroupOn, X, Y, TeamN * CW * S, (CH + 4) * S, SkMGroup, 0.5f * S);
		Bg(X, Y, TeamN * CW * S, (CH + 4) * S);
		for (int32 I = 1; I < TeamN; ++I)
		{
			Rect(X + (I * CW - 3) * S, Y + 11 * S, FMath::Max(1.0f, S), (CH - 18) * S, WithA(KBlue, 0.55f));
		}
	}

	float SWPx = 12.0f;
	for (const FRTSHudPlayer& P : State.Players)
	{
		if (!P.bDead && P.Superweapons.Num() > 0)
		{
			SWPx = FMath::Min(SWPx, SuperweaponNamePx(P.Superweapons, (CW - 4) * S, 18 * S, 3, 12.0f));
		}
	}

	for (int32 I = 0; I < N; ++I)
	{
		const FRTSHudPlayer& P = State.Players[I];
		const float PX = I < Half ? LeftStart + I * CW : ClockX + TW + (I - Half) * CW;
		const float X = SX(PX, EAnchor::Center), Y = SY(4, false), W = (CW - 4) * S, H = CH * S;
		const bool bInTeam = P.bMe || P.bAlly;
		if (!bInTeam)
		{
			// os outros: a tecla escura (group_off); espionado fica laranja
			Skin(SkGroupOff, X, Y, W, H, SkMGroup, 0.5f * S, P.bSpied ? FLinearColor(1.0f, 0.70f, 0.32f) : FLinearColor::White);
			Bg(X, Y, W, H);
		}
		const bool bEco = P.bShowEco && !P.bDead && !P.bMe;
		// o emblema grande à esquerda, na altura do cartão (como a peça aprovada)
		const float ES = 36 * S, EX = X + 8 * S, EY = Y + (H - ES) * 0.5f;
		Image(P.Emblem, EX, EY, ES, ES, P.bDead ? FLinearColor(0.32f, 0.32f, 0.32f, 0.85f) : FLinearColor::White, false);
		// a coluna do texto, do emblema até a borda de dentro do cartão: nada passa dela
		const float TX = X + 50 * S, RX = X + W - 9 * S, TextW = RX - TX;
		const FLinearColor NameC = P.bDead ? KTextDim : FMath::Lerp(P.Color, FLinearColor::White, bInTeam ? 0.30f : 0.12f);
		auto FitPx = [&](const FString& Str, float Px, EWeight Wt)
		{
			while (Px > 10.0f && Measure(Str, Px, Wt).X > TextW)
			{
				Px -= 0.5f;
			}
			return Px;
		};
		if (!bEco)
		{
			TextMid(P.Name, TX, Y + H * 0.5f, FitPx(P.Name, 16.0f, EWeight::Bold), NameC, EWeight::Bold);
		}
		else
		{
			// o nome em cima; o dinheiro à esquerda e a energia em número à direita; a barra da energia embaixo
			TextMid(P.Name, TX, Y + 16 * S, FitPx(P.Name, 15.0f, EWeight::Bold), NameC, EWeight::Bold);
			TextMid(P.Money, TX, Y + 34 * S, 13.0f, KGoldText, EWeight::Bold);
			if (P.PowerMade > 0)
			{
				const bool bLow = P.PowerUsed > P.PowerMade;
				TextMid(FString::Printf(TEXT("%d/%d"), P.PowerUsed, P.PowerMade), RX, Y + 34 * S, 12.0f, bLow ? KRed : KTextMain, EWeight::SemiBold, EAlign::Right);
				Rect(TX, Y + 44 * S, TextW, 4 * S, FLinearColor(0, 0, 0, 0.85f));
				BarFill(TX + 1 * S, Y + 45 * S, (TextW - 2 * S) * FMath::Min(1.0f, float(P.PowerUsed) / P.PowerMade), 2 * S, bLow ? KRed : KGreen);
			}
		}
		if (P.bSpied)
		{
			// o selo do olho no canto do emblema; o tempo que resta da espionagem é o carregamento do C&C no selo
			const float BS = 24 * S, BX = EX + ES - BS + 7 * S, BY = EY + ES - BS + 6 * S;
			Image(SkSpyRing, BX, BY, BS, BS, FLinearColor::White, false);
			const FVector2D BC(BX + BS * 0.5f, BY + BS * 0.5f);
			const float R = BS * 0.5f - 1 * S, A0 = FMath::Clamp(P.SpyLeft, 0.0f, 1.0f) * 2 * PI;
			TArray<FVector2D> Pie;
			const int32 Seg = FMath::Max(2, FMath::CeilToInt((1 - P.SpyLeft) * 40));
			for (int32 K = 0; K < Seg; ++K)
			{
				const float Ta = A0 + (2 * PI - A0) * K / Seg, Tb = A0 + (2 * PI - A0) * (K + 1) / Seg;
				Pie.Add(BC);
				Pie.Add(BC + FVector2D(FMath::Sin(Ta), -FMath::Cos(Ta)) * R);
				Pie.Add(BC + FVector2D(FMath::Sin(Tb), -FMath::Cos(Tb)) * R);
			}
			if (P.SpyLeft < 1.0f)
			{
				Tris(Pie, FLinearColor(0, 0, 0, 0.70f));
			}
			Image(SkSpyEye, BX, BY, BS, BS, FLinearColor::White, false);
		}
		if (P.bDead)
		{
			Image(SkDefeatedX, EX + 3 * S, EY + 3 * S, ES - 6 * S, ES - 6 * S, FLinearColor::White, false);
		}

		// a lista das superarmas: caixa separada embaixo do dono, uma por linha, até 3 e "+n more"
		if (!P.bDead && P.Superweapons.Num() > 0)
		{
			DrawSuperweapons(P.Superweapons, X, Y + H + 4 * S, W, 18 * S, 3, SWPx, bInTeam);
		}
	}

	// o relógio
	const float CX = SX(ClockX + 2, EAnchor::Center);
	Skin(SkGroupOff, CX, SY(0, false), (TW - 4) * S, (CH + 8) * S, SkMGroup, 0.5f * S);
	Bg(CX, SY(0, false), (TW - 4) * S, (CH + 8) * S);
	TextMid(State.Clock, CX + (TW - 4) * 0.5f * S, SY(33, false), 32.0f, KTextMain, EWeight::Bold, EAlign::Center);
}

void FRTSHudPainter::OutlinePoly(const TArray<FVector2D>& Poly, const FLinearColor& Rim)
{
	for (int32 I = 0; I < Poly.Num(); ++I)
	{
		Line(Poly[I], Poly[(I + 1) % Poly.Num()], 5 * S, FLinearColor(0, 0, 0, 0.9f));
	}
	for (int32 I = 0; I < Poly.Num(); ++I)
	{
		Line(Poly[I], Poly[(I + 1) % Poly.Num()], 2.5f * S, Rim);
	}
}

// A caixa das superarmas (peça aprovada top_bar_8_players): uma mini tecla igual ao cartão de cima (azul no
// seu time, escura no inimigo, vermelha com brilho quando alguma falta menos de 1 minuto); por linha, o trevo,
// o nome e o tempo, com respiro nas pontas. A linha que está pra sair fica vermelha, sem faixa por trás.
namespace
{
	struct FSWLayout
	{
		float Pad, Icon, Gap, TimePx;
	};
	FSWLayout SWLayout(float RowH, float S)
	{
		return { 8 * S, FMath::Min(RowH * 0.74f, 18 * S), 5 * S, RowH >= 24 * S ? 16.0f : 13.0f };
	}
}

float FRTSHudPainter::SuperweaponNamePx(const TArray<FRTSHudSuperweapon>& List, float W, float RowH, int32 MaxRows, float NamePx) const
{
	const FSWLayout L = SWLayout(RowH, S);
	float Px = NamePx;
	for (int32 R = 0; R < FMath::Min(MaxRows, List.Num()); ++R)
	{
		const float Room = W - 2 * L.Pad - L.Icon - L.Gap - Measure(List[R].Time, L.TimePx, EWeight::Bold).X - 6 * S;
		while (Px > 8.0f && Measure(List[R].Name, Px, EWeight::SemiBold).X > Room)
		{
			Px -= 0.5f;
		}
	}
	return Px;
}

void FRTSHudPainter::DrawSuperweapons(const TArray<FRTSHudSuperweapon>& List, float X, float Y, float W, float RowH, int32 MaxRows, float NamePx, bool bTeam)
{
	const int32 Rows = FMath::Min(MaxRows, List.Num());
	const int32 More = List.Num() - Rows;
	bool bSoon = false;
	for (const FRTSHudSuperweapon& SW : List) bSoon |= SW.bSoon;
	const FSWLayout L = SWLayout(RowH, S);
	const float PadY = Rows == 1 && More == 0 ? 2 * S : 4 * S;
	const float MoreH = More > 0 ? 16 * S : 0.0f;
	const float H = Rows * RowH + MoreH + 2 * PadY;
	if (bSoon)
	{
		// falta menos de 1 minuto: o brilho vermelho pulsa
		const float P = 0.55f + 0.45f * Pulse(1.0f);
		FillPoly(ChamferPoly(X - 5 * S, Y - 5 * S, W + 10 * S, H + 10 * S, 9 * S), [P](const FVector2D&) { return FLinearColor(0.92f, 0.14f, 0.10f, 0.16f * P); });
		FillPoly(ChamferPoly(X - 2.5f * S, Y - 2.5f * S, W + 5 * S, H + 5 * S, 7 * S), [P](const FVector2D&) { return FLinearColor(0.92f, 0.16f, 0.12f, 0.30f * P); });
	}
	Skin(bSoon ? SkGroupSoon : (bTeam ? SkGroupOn : SkGroupOff), X, Y, W, H, SkMGroup, 0.2f * S);
	Bg(X, Y, W, H);
	const float Px = FMath::Min(NamePx, SuperweaponNamePx(List, W, RowH, MaxRows, NamePx));
	for (int32 R = 0; R < Rows; ++R)
	{
		const FRTSHudSuperweapon& SW = List[R];
		const float CY = Y + PadY + (R + 0.5f) * RowH;
		const FLinearColor TC = SW.bSoon ? FLinearColor(FColor(255, 92, 76)) : KTextMain;
		Image(SW.bSoon ? SkRadiationRed : SkRadiation, X + L.Pad, CY - L.Icon * 0.5f, L.Icon, L.Icon, FLinearColor::White, false);
		TextMid(SW.Name, X + L.Pad + L.Icon + L.Gap, CY, Px, TC, EWeight::SemiBold);
		TextMid(SW.Time, X + W - L.Pad, CY, L.TimePx, TC, EWeight::Bold, EAlign::Right);
	}
	if (More > 0)
	{
		TextMid(FString::Printf(TEXT("+ %d more"), More), X + L.Pad + 1 * S, Y + PadY + Rows * RowH + MoreH * 0.5f - 1 * S, 12.0f, KTextSub, EWeight::SemiBold);
	}
}

// o topo 1x1: a peça aprovada (você em azul, o relógio, o adversário em vermelho, os emblemas nos anéis
// das pontas) na largura do limpo; a superarma sai de baixo da barra, do lado do dono
void FRTSHudPainter::DrawTop1v1(const FRTSHudState& State)
{
	const FRTSHudPlayer& Me = State.Players[0];
	const FRTSHudPlayer& Foe = State.Players[1];
	const float K = 617.0f / 1087.0f * S;
	const float X = SX(651.5f, EAnchor::Center), Y = SY(0, false);
	auto A = [&](float U, float V) { return FVector2D(X + U * K, Y + V * K); };

	for (int32 I = 0; I < 2; ++I)
	{
		const FRTSHudPlayer& Pl = I == 0 ? Me : Foe;
		if (Pl.Superweapons.Num() > 0 && !Pl.bDead)
		{
			const bool bOne = Pl.Superweapons.Num() == 1;
			DrawSuperweapons(Pl.Superweapons, SX(I == 0 ? 668.5f : 1019.5f, EAnchor::Center), SY(73, false), 232 * S, (bOne ? 26 : 22) * S, 3, 13.0f, I == 0);
		}
	}
	Skin(SkTopBar, X, Y, 1087 * K, 140 * K);
	Bg(X, Y, 1087 * K, 140 * K);
	for (int32 I = 0; I < 2; ++I)
	{
		const FRTSHudPlayer& Pl = I == 0 ? Me : Foe;
		const FVector2D C = A(I == 0 ? 83.0f : 1006.0f, 65.0f);
		const float R = 39 * K;
		Image(Pl.Emblem, C.X - R, C.Y - R, 2 * R, 2 * R, Pl.bDead ? FLinearColor(0.3f, 0.3f, 0.3f) : FLinearColor::White, false);
	}
	const FVector2D MeC = A(272, 68), ClockC = A(542.5f, 68), FoeC = A(806, 68);
	TextMid(Me.Name, MeC.X, MeC.Y, 26.0f, FLinearColor(FColor(170, 210, 255)), EWeight::Bold, EAlign::Center);
	TextMid(State.Clock, ClockC.X, ClockC.Y, 36.0f, KTextMain, EWeight::Bold, EAlign::Center);
	TextMid(Foe.Name, FoeC.X, FoeC.Y, 26.0f, FLinearColor(FColor(255, 120, 108)), EWeight::Bold, EAlign::Center);
}

// ---------------------------------------------------------------------------------------------
// painel do comandante: dinheiro, energia, patente e o Promote (canto de cima à direita)

void FRTSHudPainter::DrawCommander(const FRTSHudState& State)
{
	if (State.Money.IsEmpty())
	{
		return;
	}
	// sem patente no jogo (Stars < 0), o painel fica numa linha só: o dinheiro e a energia, sem estrela vazia
	const bool bRank = State.Stars >= 0;
	const float X = SX(1558, EAnchor::Right), Y = SY(6, false), W = 354 * S, H = (bRank ? 97 : 60) * S;
	Skin(SkPanelWide, X, Y, W, H, SkMPanelWide, 0.42f * S);
	Bg(X, Y, W, H);
	const float R1 = Y + (bRank ? 31 : 30) * S, R2 = Y + 67 * S;
	TextMid(State.Money, X + 20 * S, R1, 32.0f, KGoldText, EWeight::Bold);
	if (State.PowerMade > 0)
	{
		const bool bLow = State.PowerUsed > State.PowerMade;
		const FString Nums = FString::Printf(TEXT("%d/%d"), State.PowerUsed, State.PowerMade);
		const float NX = X + W - 20 * S;
		const float BW = 56 * S, BX = NX - Measure(Nums, 15.0f, EWeight::SemiBold).X - 8 * S - BW;
		TextMid(TEXT("Power"), BX - 8 * S, R1, 15.0f, KTextSub, EWeight::Medium, EAlign::Right);
		// faltando energia, a barra vermelha pisca devagar (o aviso do ZH)
		Bar(BX, R1 - 7 * S, BW, 14 * S, float(State.PowerUsed) / State.PowerMade, bLow ? FMath::Lerp(KRed * 0.45f, KRed, Pulse(1.2f)) : KGreen);
		TextMid(Nums, NX, R1, 15.0f, bLow ? KRed : KTextMain, EWeight::SemiBold, EAlign::Right);
	}
	if (!bRank)
	{
		return;
	}
	for (int32 I = 0; I < 5; ++I)
	{
		Star(FVector2D(X + (28 + I * 20) * S, R2), 9 * S, I < State.Stars ? KGold : FLinearColor(FColor(80, 84, 90)));
	}
	Bar(X + 130 * S, R2 - 7 * S, 92 * S, 14 * S, State.Xp, KGold);
	if (State.Promote > 0)
	{
		// o botão dourado do limpo: aro escuro, ouro em degradê e o brilho em cima
		const FString L = FString::Printf(TEXT("Promote %d"), State.Promote);
		const float BW = Measure(L, 15.0f, EWeight::Bold).X + 18 * S, BH = 25 * S, BX = X + W - 20 * S - BW, BY = R2 - BH * 0.5f;
		FillPoly(ChamferPoly(BX - 1.5f * S, BY - 1.5f * S, BW + 3 * S, BH + 3 * S, 4.5f * S), [](const FVector2D&) { return FLinearColor(0.0f, 0.0f, 0.0f, 0.9f); });
		FillPoly(ChamferPoly(BX, BY, BW, BH, 3.5f * S), [=](const FVector2D& P)
		{
			return FMath::Lerp(FLinearColor(FColor(255, 216, 92)), FLinearColor(FColor(190, 132, 26)), FMath::Clamp((P.Y - BY) / BH, 0.0f, 1.0f));
		});
		Rect(BX + 4 * S, BY + 1.5f * S, BW - 8 * S, FMath::Max(1.0f, S), FLinearColor(1, 1, 1, 0.45f));
		TextMid(L, BX + BW * 0.5f, BY + BH * 0.5f, 15.0f, FLinearColor(FColor(24, 18, 6)), EWeight::Bold, EAlign::Center, false);
		HoverPress(TEXT("promote"), BX, BY, BW, BH, KGold);
		Buttons->Add({ TEXT("promote"), FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH)) });
	}
}

// ---------------------------------------------------------------------------------------------

void FRTSHudPainter::DrawAlerts(const FRTSHudState& State)
{
	for (int32 I = 0; I < State.Alerts.Num(); ++I)
	{
		const FRTSHudAlert& Al = State.Alerts[I];
		// o aviso do limpo: a ponta de metal vermelha fica, o miolo estica, o fim some no mundo
		// entra deslizando da esquerda (0,3 s, desacelerando)
		const float In = FMath::Clamp(Al.Age / 0.3f, 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Pow(1.0f - In, 3.0f);
		const float W = 340 * S, H = 48 * S;
		const float X = SX(12, EAnchor::Left) - (1.0f - Ease) * (W + 20 * S), Y = SY(12 + I * 54, false);
		const bool bAmber = Al.Color.G > 0.2f;
		Skin(bAmber ? SkAlertAmber : SkAlert, X, Y, W, H, SkMAlert, H / 134.0f);
		Bg(X, Y, W, H);
		const float IW = 32 * S, IH = 28 * S;
		Image(bAmber ? SkAlertIconAmber : SkAlertIcon, X + 40 * S, Y + (H - IH) * 0.5f - 1 * S, IW, IH, FLinearColor::White, false);
		const FLinearColor TextC = bAmber ? FLinearColor(FColor(255, 184, 52)) : FLinearColor(FColor(246, 70, 62));
		TextMid(Al.Text, X + 82 * S, Y + H * 0.5f, 23.0f, TextC, EWeight::Bold);
	}
	if (State.bOffscreenArrow)
	{
		// a seta aponta pra fora da TELA: fica na borda de verdade, mesmo no ultrawide (fora da faixa 16:9)
		const float X = Canvas->ClipX - 22 * S, Y = Canvas->ClipY * 0.46f;
		Line(FVector2D(X - 12 * S, Y - 20 * S), FVector2D(X + 6 * S, Y), 7 * S, KRed);
		Line(FVector2D(X + 6 * S, Y), FVector2D(X - 12 * S, Y + 20 * S), 7 * S, KRed);
	}
}

// coluna de poderes: só o ícone num botão quadrado e simples (a tecla escura da pele); carregando = o fundo
// escuro do C&C e o tempo em cima; pronto = filete dourado por dentro e READY. A coluna fica em cima da placa
// do minimapa, alinhada na borda esquerda dela, e cresce de baixo pra cima com o mesmo vão entre tudo.
void FRTSHudPainter::DrawPowers(const FRTSHudState& State)
{
	// a placa do minimapa (as mesmas contas do DrawMinimap): borda esquerda em 46 e topo em 120 da textura
	const float PodK = 0.534f * S;
	const float PodX = SX(8, EAnchor::Left), PodY = SY(1072, true) - 620 * PodK;
	const float W = 64 * S, Gap = 6 * S;
	const float X = PodX + 46 * PodK, Bottom = PodY + 120 * PodK - Gap;
	for (int32 I = 0; I < State.Powers.Num(); ++I)
	{
		const FRTSHudPower& P = State.Powers[I];
		const float Y = Bottom - W - I * (W + Gap);
		const bool bReady = P.Remaining <= 0.0f;
		Skin(SkGroupOff, X, Y, W, W, SkMGroup, 0.3f * S);
		const float In = 6 * S, IW = W - 2 * In;
		ImageInset(P.Icon, X + In, Y + In, IW, IW, 0.1f);
		if (!bReady)
		{
			Sweep(X + In, Y + In, IW, IW, 1.0f - P.Remaining, 0.62f);
		}
		else
		{
			// pronto: o filete dourado respira (quase 1 vez por segundo)
			const float Glow = Pulse(0.9f);
			Border(X + In - 1 * S, Y + In - 1 * S, IW + 2 * S, IW + 2 * S, FMath::Max(1.0f, 1.5f * S), WithA(KGold, 0.55f + 0.45f * Glow));
			FillPoly(ChamferPoly(X - 3 * S, Y - 3 * S, W + 6 * S, W + 6 * S, 8 * S), [Glow](const FVector2D&) { return FLinearColor(1.0f, 0.70f, 0.18f, 0.05f + 0.13f * Glow); });
		}
		// o tempo (ou READY) numa faixa escura na base do ícone
		Rect(X + In, Y + W - In - 18 * S, IW, 18 * S, FLinearColor(0, 0, 0, 0.62f));
		if (!bReady)
		{
			TextMid(P.Time, X + W * 0.5f, Y + W - In - 9 * S, 16.0f, KTextMain, EWeight::Bold, EAlign::Center);
		}
		else
		{
			TextMid(TEXT("READY"), X + W * 0.5f, Y + W - In - 9 * S, 14.0f, KGoldText, EWeight::Bold, EAlign::Center);
		}
		const FName PId(*FString::Printf(TEXT("power_%d"), I));
		HoverPress(PId, X, Y, W, W, bReady ? KGold : Accent());
		Buttons->Add({ PId, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + W)) });
	}
}

// ---------------------------------------------------------------------------------------------
// minimapa: a peça aprovada (o losango no painel e a coluna dos 4 botões), o mapa por baixo do buraco

void FRTSHudPainter::DrawMinimap(const FRTSHudState& State)
{
	// 0,534: a peça ocupa o mesmo canto que o minimapa e os botões do limpo (377 px de largura)
	const float K = 0.534f * S;
	const float W = 706 * K, H = 620 * K;
	const float X = SX(8, EAnchor::Left), Y = SY(1072, true) - H;
	const FVector2D C(X + 311.5f * K, Y + 302.5f * K);
	// o mapa é um quadrado girado 45°: a meia diagonal (264 da textura) passa um pouco do buraco
	const float Side = 266.0f * K * UE_SQRT_2;
	if (UTexture* Tex = Lookup ? Lookup(State.MinimapImage) : nullptr)
	{
		if (Tex->GetResource())
		{
			FCanvasTileItem Item(C - FVector2D(Side, Side) * 0.5f, Tex->GetResource(), FVector2D(Side, Side), FLinearColor::White);
			Item.Rotation = FRotator(0.0f, 45.0f, 0.0f);
			Item.PivotPoint = FVector2D(0.5f, 0.5f);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	}
	auto ToScreen = [&](const FVector2D& UV)
	{
		const FVector2D P((UV.X - 0.5f) * Side, (UV.Y - 0.5f) * Side);
		const float Cs = 0.70710678f;
		return C + FVector2D(P.X * Cs - P.Y * Cs, P.X * Cs + P.Y * Cs);
	};
	for (const FRTSHudMinimapDot& Dot : State.MinimapDots)
	{
		const FVector2D P = ToScreen(Dot.UV);
		FillCircle(P, 4.5f * S, FLinearColor(0, 0, 0, 0.7f), FLinearColor(0, 0, 0, 0.7f), 12);
		FillCircle(P, 3.5f * S, Dot.Color, Dot.Color, 12);
	}
	if (State.MinimapCamera.bIsValid)
	{
		const FVector2D A = ToScreen(State.MinimapCamera.Min), Bq = ToScreen(FVector2D(State.MinimapCamera.Max.X, State.MinimapCamera.Min.Y));
		const FVector2D Cq = ToScreen(State.MinimapCamera.Max), Dq = ToScreen(FVector2D(State.MinimapCamera.Min.X, State.MinimapCamera.Max.Y));
		Line(A, Bq, 2 * S, FLinearColor::White); Line(Bq, Cq, 2 * S, FLinearColor::White);
		Line(Cq, Dq, 2 * S, FLinearColor::White); Line(Dq, A, 2 * S, FLinearColor::White);
	}

	Skin(SkMinimapPod, X, Y, W, H);
	Bg(X, Y, W, H);
	// a bússola do clássico: uma letra dentro de cada ponta do losango
	TextMid(TEXT("N"), C.X, C.Y - 226 * K, 14.0f, KTextMain, EWeight::Bold, EAlign::Center);
	TextMid(TEXT("S"), C.X, C.Y + 226 * K, 14.0f, KTextMain, EWeight::Bold, EAlign::Center);
	TextMid(TEXT("W"), C.X - 222 * K, C.Y, 14.0f, KTextMain, EWeight::Bold, EAlign::Center);
	TextMid(TEXT("E"), C.X + 222 * K, C.Y, 14.0f, KTextMain, EWeight::Bold, EAlign::Center);

	// os botões nos encaixes da coluna (sem a moldura do ícone: o encaixe já é a moldura)
	const float Rows[4][2] = { { 176, 243 }, { 266, 332 }, { 355, 422 }, { 445, 511 } };
	const TCHAR* Icons[4] = { TEXT("icon:settings"), TEXT("icon:idle_units"), TEXT("icon:beacon"), TEXT("icon:chat") };
	const TCHAR* Ids[4] = { TEXT("mm_settings"), TEXT("mm_idle"), TEXT("mm_beacon"), TEXT("mm_chat") };
	for (int32 I = 0; I < 4; ++I)
	{
		const float BX = X + 611 * K, BY = Y + Rows[I][0] * K, BW = 64 * K, BH = (Rows[I][1] - Rows[I][0]) * K;
		ImageInset(FName(Icons[I]), BX + 4 * K, BY + 4 * K, BW - 8 * K, BH - 8 * K, 0.1f);
		HoverPress(FName(Ids[I]), BX, BY, BW, BH, Accent());
		Buttons->Add({ FName(Ids[I]), FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH)) });
	}
}

// ---------------------------------------------------------------------------------------------
// abas de grupo: o número grande é a tecla, a quantidade pequena ao lado, a vida somada embaixo

void FRTSHudPainter::DrawGroups(const FRTSHudState& State)
{
	const float GY = State.bGroupsAtBottom ? 1024.0f : 1072.0f - CommandPanelHeight(State) - 50.0f;
	for (int32 I = 0; I < 10 && I < State.Groups.Num(); ++I)
	{
		const FRTSHudGroup& G = State.Groups[I];
		const float X = SX(685 + I * 55, EAnchor::Center), Y = SY(GY, true) + (State.bGroupsAtBottom ? 0.0f : PanelDrop(State)), W = 50 * S, H = 46 * S;
		const FString Num = FString::FromInt((I + 1) % 10);
		const bool bOn = G.Count > 0;
		Skin(bOn ? SkGroupOn : SkGroupOff, X, Y, W, H, SkMGroup, 0.36f * S);
		if (bOn)
		{
			TextMid(Num, X + 11 * S, Y + 21 * S, 26.0f, KTextMain, EWeight::Bold);
			TextMid(FString::Printf(TEXT("×%d"), G.Count), X + W - 9 * S, Y + 25 * S, 14.0f, KTextSub, EWeight::SemiBold, EAlign::Right);
			Rect(X + 9 * S, Y + H - 13 * S, W - 18 * S, 5 * S, FLinearColor(0, 0, 0, 0.85f));
			BarFill(X + 10 * S, Y + H - 12 * S, (W - 20 * S) * G.Health, 3 * S, HealthColor(G.Health));
		}
		else
		{
			TextMid(Num, X + W * 0.5f, Y + H * 0.5f, 24.0f, KTextDim, EWeight::Bold, EAlign::Center);
		}
		const FName GId(*FString::Printf(TEXT("group_%d"), (I + 1) % 10));
		HoverPress(GId, X, Y, W, H, Accent());
		Buttons->Add({ GId, FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)) });
	}
}

// ---------------------------------------------------------------------------------------------
// painel central: do minimapa até o cartão; grade principal 2 × 9 e grade de ações 2 × 3

float FRTSHudPainter::CommandPanelHeight(const FRTSHudState& State)
{
	return State.MainColumns >= 9 ? 180.0f : 204.0f;
}

void FRTSHudPainter::DrawCommandPanel(const FRTSHudState& State)
{
	if (!State.bCommandPanel)
	{
		return;
	}
	// a peça comprida da pele: as pontas entram por baixo do minimapa e do cartão (encosta nos dois)
	const float PH = CommandPanelHeight(State), PTop = 1072.0f - PH;
	const float X0 = SX(378, EAnchor::Left), X1 = SX(1520, EAnchor::Right), Y = SY(PTop, true) + PanelDrop(State), H = PH * S;
	Skin(SkPanelLong, X0, Y, X1 - X0, H, SkMPanelLong, 0.22f * S);
	Bg(X0, Y, X1 - X0, H);
	// as casas: a grade principal (MC colunas) e a de ações (3), com 6 px de vão e um sulco entre as duas
	const int32 MC = FMath::Clamp(State.MainColumns, 5, 9);
	const float Left = 408, Right = 1492, Sep = 10, Gap = 6, PadY = 10, GapY = 8;
	const float Step = (Right - Left - Sep + Gap) / (MC + 3), SlotW = Step - Gap;
	const float SlotH = (PH - 2 * PadY - GapY) / 2, StepY = SlotH + GapY, MainY = PTop + PadY;
	const float ActX = Left + MC * Step + Sep;
	const float GX = SX(ActX - (Sep + Gap) * 0.5f, EAnchor::Left);
	Rect(GX - 1 * S, Y + 14 * S, 2 * S, H - 28 * S, FLinearColor(0, 0, 0, 0.7f));
	Rect(GX + 1 * S, Y + 14 * S, FMath::Max(1.0f, S), H - 28 * S, FLinearColor(1, 1, 1, 0.08f));

	auto Place = [&](const FRTSHudSlot& Slot, float PXv, float PYv)
	{
		const float X = SX(PXv, EAnchor::Left), SYv = SY(PYv, true) + PanelDrop(State), W = SlotW * S, SH = SlotH * S;
		DrawSlot(Slot, X, SYv, W, SH);
		if (!Slot.bEmpty && !Slot.Id.IsNone())
		{
			const FBox2D Box(FVector2D(X, SYv), FVector2D(X + W, SYv + SH));
			Buttons->Add({ Slot.Id, Box });
			SlotBoxes.Add(Slot.Id, Box);
		}
	};
	if (State.Timer.bShow)
	{
		// (v4, 23/10) o relógio no lugar da grade principal (o CB_CONTEXT_OCL_TIMER do ZH): a esquerda sem casas, a direita igual
		DrawTimer(State, Left, ActX - (Sep + Gap) * 0.5f, PTop, PH);
	}
	for (int32 I = 0; !State.Timer.bShow && I < 2 * MC; ++I)
	{
		Place(I < State.Main.Num() ? State.Main[I] : FRTSHudSlot(), Left + (I % MC) * Step, MainY + (I / MC) * StepY);
	}
	for (int32 I = 0; I < 6; ++I)
	{
		Place(I < State.Actions.Num() ? State.Actions[I] : FRTSHudSlot(), ActX + (I % 3) * Step, MainY + (I / 3) * StepY);
	}
}

// ---------------------------------------------------------------------------------------------
// (v4, 23/10) o relógio do prédio que trabalha sozinho (a Supply Drop Zone e o avião de carga): a esquerda do painel do meio
// sem a grade, na composição que o Felipe escolheu (a imagem 4 do GPT, docs/aprovados/jogo/
// 2026-10-23_timer_drop_zone_gpt_b_ESQUERDA_APROVADA.jpg): o medalhão de aço com a luz azul atrás; ao lado, o rótulo dourado
// espaçado, o tempo no tamanho do relógio do topo (nunca maior), a linha cinza e a barra no rebaixo da vida do cartão. É o
// CB_CONTEXT_OCL_TIMER do ZH (ControlBarOCLTimer.cpp: o texto e a barra só mudam quando o segundo mostrado muda; quem preenche
// já manda a barra em degraus de 1 s). As medidas saíram do alvo, na planta do painel de 7 colunas (204 de altura); noutra
// altura tudo encolhe junto.

FVector2D FRTSHudPainter::TextSpaced(const FString& Str, float X, float CY, float Px, const FLinearColor& C, float Tracking)
{
	// letra por letra, com Tracking × o tamanho entre elas (as maiúsculas espaçadas não pedem o kerning)
	float CX = X;
	for (int32 I = 0; I < Str.Len(); ++I)
	{
		const FString Ch = Str.Mid(I, 1);
		if (Ch != TEXT(" "))
		{
			TextMid(Ch, CX, CY, Px, C, EWeight::Bold);
		}
		CX += Measure(Ch, Px, EWeight::Bold).X + Tracking * Px * S;
	}
	return FVector2D(CX - X, Measure(Str, Px, EWeight::Bold).Y);
}

void FRTSHudPainter::DrawTimer(const FRTSHudState& State, float Left, float Right, float PTop, float PH)
{
	const FRTSHudTimer& T = State.Timer;
	const float Drop = PanelDrop(State);
	const float K = PH / 204.0f;
	// o medalhão quase na altura inteira do painel (no alvo ele encosta nas bordas), com o vão à esquerda pra luz
	const float R = 96.0f * K;
	const float CXp = Left + 148.0f * K;
	const FVector2D C(SX(CXp, EAnchor::Left), SY(PTop + PH * 0.5f, true) + Drop);
	const float Rs = R * S;

	// a luz azul atrás (a textura cobre 3,4 R × 2,1 R: o pico colado no aro, mais comprida do lado de fora). Sem energia ela
	// quase apaga; com o avião a caminho ela respira devagar (o READY da coluna de poderes respira assim)
	const float GlowA = T.bPaused ? 0.12f : (T.bInbound ? 0.55f + 0.45f * Pulse(0.9f) : 0.88f);
	Image(SkTimerGlow, C.X - 1.7f * Rs, C.Y - 1.05f * Rs, 3.4f * Rs, 2.1f * Rs, FLinearColor(1.0f, 1.0f, 1.0f, GlowA), false);
	// o medalhão: a peça de 512 px tem o aro no raio 250 (o resto é a sombra de fora); parado, mais escuro
	const float MD = 2.0f * Rs * 512.0f / 500.0f;
	const float Lit = T.bPaused ? 0.70f : 1.0f;
	Image(T.Medallion, C.X - MD * 0.5f, C.Y - MD * 0.5f, MD, MD, FLinearColor(Lit, Lit, Lit, 1.0f), false);

	// o texto: o rótulo (dourado; âmbar com o avião a caminho e na parada), o tempo (o tamanho do relógio do topo: 36 no 1x1,
	// 32 com mais jogadores) e a linha dos paletes
	const float ClockPx = State.Players.Num() == 2 ? 36.0f : 32.0f;
	const float TX = SX(CXp + R + 27.0f * K, EAnchor::Left);
	const auto Row = [&](float Y) { return SY(PTop + Y * K, true) + Drop; };
	TextSpaced(T.Title, TX, Row(42.0f), 20.0f * K, (T.bInbound || T.bPaused) ? KAmber : KGoldText, 0.40f);
	TextMid(T.Time, TX - 1.0f * S, Row(80.0f), ClockPx * K, KTextMain, EWeight::Bold);
	TextMid(T.Line, TX, Row(118.0f), 25.0f * K, KTextSub, EWeight::SemiBold);

	// a barra no rebaixo da vida do cartão (a mesma pele), da esquerda pra direita: o nosso azul vivo com o brilho em cima
	// (como no alvo); parada, em âmbar
	const float BX = TX, BY = Row(144.0f), BW = SX(Right - 26.0f * K, EAnchor::Left) - TX, BH = 32.0f * K * S;
	if (BW < 40.0f * S)
	{
		return;
	}
	Skin(SkUnitCard, BX, BY, BW, BH, FMargin(10.0f), BH / 47.0f, FLinearColor::White, SkCardHealthSlot);
	const float In = FMath::Max(2.0f, BH * 0.15f);
	const float FX = BX + In, FY = BY + In, FW = (BW - 2.0f * In) * FMath::Clamp(T.Fill, 0.0f, 1.0f), FH = BH - 2.0f * In;
	if (FW < 0.5f)
	{
		return;
	}
	const FLinearColor Body = T.bPaused ? FLinearColor(FColor(226, 146, 30)) : FLinearColor(FColor(52, 150, 248));
	const FLinearColor Hi = FMath::Lerp(Body, FLinearColor::White, 0.45f);
	const FLinearColor Lo(Body.R * 0.70f, Body.G * 0.70f, Body.B * 0.70f, 1.0f);
	const float Split = FY + FH * 0.35f;
	FillPoly({ {FX, FY}, {FX + FW, FY}, {FX + FW, Split}, {FX, Split} }, [=](const FVector2D& P)
	{
		return FMath::Lerp(Hi, Body, FMath::Clamp((P.Y - FY) / FMath::Max(1.0f, Split - FY), 0.0f, 1.0f));
	});
	FillPoly({ {FX, Split}, {FX + FW, Split}, {FX + FW, FY + FH}, {FX, FY + FH} }, [=](const FVector2D& P)
	{
		return FMath::Lerp(Body, Lo, FMath::Clamp((P.Y - Split) / FMath::Max(1.0f, FY + FH - Split), 0.0f, 1.0f));
	});
	Rect(FX, FY, FW, FMath::Max(1.0f, S), FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
}

void FRTSHudPainter::DrawSlot(const FRTSHudSlot& Slot, float X, float Y, float W, float H)
{
	// U: pixels da tela por pixel do botão grande (74 de altura); Z: o mesmo pros tamanhos de letra
	const float U = H / 74.0f, Z = U / S;
	const float Sc = 0.17f * U;          // a moldura da pele (198×189) fina, como as casas da peça aprovada
	const float In = 30.0f * Sc;         // a janela dentro da moldura
	if (Slot.bEmpty)
	{
		// a casa vazia é lisa (a peça aprovada multi_select_captain): fundo escuro e um fio cinza
		FillPoly(ChamferPoly(X + 1 * U, Y + 1 * U, W - 2 * U, H - 2 * U, 4 * U), [](const FVector2D&) { return FLinearColor(0.0f, 0.0f, 0.0f, 0.30f); });
		const FLinearColor Ln(FColor(96, 108, 122, 70));
		const TArray<FVector2D> Pts = ChamferPoly(X + 1 * U, Y + 1 * U, W - 2 * U, H - 2 * U, 4 * U);
		for (int32 K = 0; K < Pts.Num(); ++K)
		{
			Line(Pts[K], Pts[(K + 1) % Pts.Num()], FMath::Max(1.0f, U), Ln);
		}
		return;
	}
	const FName FrameSkin = Slot.bCaptain ? SkSlotCaptain : (Slot.bAction ? SkSlotAction : (Slot.bLocked ? SkSlotLocked : SkSlotNormal));
	Skin(FrameSkin, X, Y, W, H, SkMSlot, Sc);
	const float IX = X + In, IY = Y + In, IW = W - 2 * In, IH = H - 2 * In;
	const bool bShowPrice = !Slot.Price.IsEmpty() && Slot.Health < 0.0f && Slot.Progress < 0.0f;
	const float Band = bShowPrice ? 17 * U : 0.0f;
	if (!Slot.Image.IsNone())
	{
		if (Slot.Image.ToString().StartsWith(TEXT("icon:")))
		{
			// o ícone quadrado, sem a moldura dele, no meio da janela
			const float IS = FMath::Min(IW, IH - Band) * 0.82f;
			ImageInset(Slot.Image, IX + (IW - IS) * 0.5f, IY + (IH - Band - IS) * 0.5f + 2 * U, IS, IS, 0.1f);
		}
		else
		{
			Image(Slot.Image, IX, IY, IW, IH - Band);
		}
	}
	else if (!Slot.Label.IsEmpty())
	{
		TextMid(Slot.Label, X + W * 0.5f, IY + IH * 0.4f, 13.0f * Z, KTextMain, EWeight::SemiBold, EAlign::Center);
	}
	if (bShowPrice)
	{
		Rect(IX, IY + IH - Band, IW, Band, FLinearColor(0.0f, 0.0f, 0.0f, 0.62f));
	}
	if (Slot.bLocked)
	{
		Rect(IX, IY, IW, IH, FLinearColor(0, 0, 0, 0.55f));
		ImageInset(TEXT("icon:locked"), IX + IW - 21 * U, IY + 1 * U, 20 * U, 20 * U, 0.1f);
	}
	if (Slot.bDone)
	{
		Rect(IX, IY, IW, IH, FLinearColor(0, 0, 0, 0.45f));
		const FVector2D C(X + W * 0.5f, Y + H * 0.48f);
		Line(C + FVector2D(-12 * U, 0), C + FVector2D(-3 * U, 9 * U), 5 * U, KGreen);
		Line(C + FVector2D(-3 * U, 9 * U), C + FVector2D(14 * U, -10 * U), 5 * U, KGreen);
	}
	if (Slot.Progress >= 0.0f)
	{
		Sweep(IX, IY, IW, IH, Slot.Progress, 0.62f);
		TextMid(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Slot.Progress * 100)), X + W * 0.5f, Y + H * 0.5f, 19.0f * Z, KTextMain, EWeight::Bold, EAlign::Center);
	}
	if (Slot.Health >= 0.0f)
	{
		Rect(IX + 1 * U, IY + IH - 8 * U, IW - 2 * U, 7 * U, FLinearColor(0.01f, 0.01f, 0.01f, 0.95f));
		BarFill(IX + 2 * U, IY + IH - 7 * U, (IW - 4 * U) * Slot.Health, 5 * U, HealthColor(Slot.Health));
	}

	// a melhoria: moldura dourada tracejada na borda da janela
	if (Slot.bUpgrade && !Slot.bCaptain)
	{
		DashedBorder(IX, IY, IW, IH, 2 * U, 6 * U, KGold);
	}

	if (Slot.Key != 0)
	{
		Text(FString::Chr(Slot.Key), IX + 3 * U, IY - 1 * U, 17.0f * Z, KKey, EWeight::Bold);
	}
	if (Slot.Count > 0)
	{
		const FString Cn = FString::FromInt(Slot.Count);
		const FVector2D E = Measure(Cn, 15.0f * Z, EWeight::Bold);
		const float BW = E.X + 9 * U, BX = IX + IW - BW - 1 * U, BY = IY + 1 * U;
		Rect(BX, BY, BW, 19 * U, FLinearColor::White);
		TextMid(Cn, BX + BW * 0.5f, BY + 9.5f * U, 15.0f * Z, FLinearColor::Black, EWeight::Bold, EAlign::Center, false);
	}
	if (bShowPrice)
	{
		const FLinearColor PC = Slot.bNoMoney ? KRed : (Slot.bLocked ? KTextDim : KGoldText);
		TextMid(Slot.Price, X + W * 0.5f, IY + IH - Band * 0.5f, 15.0f * Z, PC, EWeight::Bold, EAlign::Center);
	}
	HoverPress(Slot.Id, X, Y, W, H, Slot.bCaptain ? KGold : (Slot.bAction ? KGreen : Accent()));
	if (Slot.bCancelMark)
	{
		const float CX = IX + IW - 17 * U, CY = IY + 1 * U;
		Rect(CX, CY, 16 * U, 16 * U, KRed);
		Line(FVector2D(CX + 4 * U, CY + 4 * U), FVector2D(CX + 12 * U, CY + 12 * U), 2.5f * U, FLinearColor::White);
		Line(FVector2D(CX + 12 * U, CY + 4 * U), FVector2D(CX + 4 * U, CY + 12 * U), 2.5f * U, FLinearColor::White);
	}
}

// ---------------------------------------------------------------------------------------------
// cartão da seleção: a peça do limpo (retrato, nome, vida e números); a fila numa tira em cima;
// ou a placa da facção quando não há nada selecionado

void FRTSHudPainter::DrawCard(const FRTSHudState& State)
{
	const FRTSHudCard& Cd = State.Card;
	if (!Cd.bShow)
	{
		return;
	}
	// a peça unit_card (786×480) com a largura do limpo (398 px), presa no canto de baixo à direita
	const float W = 398 * S, K = W / 786.0f, H = 480 * K;
	const bool bQueue = !Cd.bFactionPlate && Cd.Queue.Num() > 0;
	const float QH = 76 * S;
	const float X = SX(1514, EAnchor::Right), Y = SY(1072, true) - H - (bQueue ? QH - 6 * S : 0.0f);
	auto A = [&](float U, float V) { return FVector2D(X + U * K, Y + V * K); };

	if (Cd.bFactionPlate)
	{
		Skin(SkPanelWide, X, Y, W, H, SkMPanelWide, 0.45f * S);
		Bg(X, Y, W, H);
		const FVector2D EC = A(590, 236);
		FillCircle(EC, 210 * K, FLinearColor(0.16f, 0.32f, 0.62f, 0.40f), FLinearColor(0.16f, 0.32f, 0.62f, 0.0f));
		Image(Cd.FactionEmblem, EC.X - 165 * K, EC.Y - 165 * K, 330 * K, 330 * K, FLinearColor::White, false);
		Text(Cd.FactionName, X + 26 * S, Y + 40 * S, 26.0f, KTextMain, EWeight::Bold);
		Text(Cd.PlateLine, X + 26 * S, Y + 82 * S, 18.0f, KTextMain, EWeight::SemiBold);
		Text(Cd.PlateKills, X + 26 * S, Y + 190 * S, 15.0f, KTextSub, EWeight::SemiBold);
		return;
	}

	// a fila de produção: uma tira da pele embaixo do cartão, presa nele (o cartão fica igual sempre)
	if (bQueue)
	{
		const float QY = Y + H - 6 * S;
		Skin(SkPanelWide, X, QY, W, QH, SkMPanelWide, 0.3f * S);
		Bg(X, QY, W, QH);
		for (int32 I = 0; I < Cd.Queue.Num() && I < 5; ++I)
		{
			const float QX = X + (22 + I * 72) * S, QSY = QY + 9 * S, QW = 66 * S, QSH = 54 * S;
			DrawSlot(Cd.Queue[I], QX, QSY, QW, QSH);
			if (!Cd.Queue[I].bEmpty)
			{
				Buttons->Add({ FName(*FString::Printf(TEXT("queue_%d"), I)), FBox2D(FVector2D(QX, QSY), FVector2D(QX + QW, QSY + QSH)) });
			}
		}
	}

	// o retrato vai por baixo da peça: a janela dela é vazada e a moldura cobre a borda da foto
	const FVector2D W0 = A(34, 26), W1 = A(752, 306);
	if (!Cd.Portrait.IsNone())
	{
		Image(Cd.Portrait, W0.X, W0.Y, W1.X - W0.X, W1.Y - W0.Y);
	}
	else
	{
		const float Y0 = W0.Y, HH = W1.Y - W0.Y;
		FillPoly({ W0, FVector2D(W1.X, W0.Y), W1, FVector2D(W0.X, W1.Y) }, [=](const FVector2D& P)
		{
			return FMath::Lerp(FLinearColor(FColor(26, 40, 62)), FLinearColor(FColor(8, 12, 20)), FMath::Clamp((P.Y - Y0) / HH, 0.0f, 1.0f));
		});
	}
	Skin(SkUnitCard, X, Y, W, H);
	Bg(X, Y, W, H);

	// a insígnia num botão pequeno da pele, no canto do retrato; a patente em divisas douradas no outro
	if (!Cd.Insignia.IsNone())
	{
		const FVector2D P = A(56, 46);
		const float IS = 36 * S, Sc = IS / 78.0f * 0.28f, In = 30.0f * Sc;
		Skin(SkSlotNormal, P.X, P.Y, IS, IS, SkMSlot, Sc);
		ImageInset(Cd.Insignia, P.X + In, P.Y + In, IS - 2 * In, IS - 2 * In, 0.1f);
	}
	if (Cd.Rank > 0)
	{
		const FVector2D P = A(706, 50);
		for (int32 R = 0; R < Cd.Rank; ++R)
		{
			const float CY = P.Y + (7 + R * 9) * S;
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				const float Th = Pass == 0 ? 6.5f * S : 3.5f * S;
				const FLinearColor Cc = Pass == 0 ? FLinearColor(0, 0, 0, 0.8f) : KGold;
				Line(FVector2D(P.X - 15 * S, CY + 7 * S), FVector2D(P.X, CY - 2 * S), Th, Cc);
				Line(FVector2D(P.X, CY - 2 * S), FVector2D(P.X + 15 * S, CY + 7 * S), Th, Cc);
			}
		}
	}
	const FVector2D NameP = A(56, 347);
	TextMid(Cd.Name, NameP.X, NameP.Y, 22.0f, KTextMain, EWeight::Bold);
	const float HF = Cd.MaxHealth > 0 ? float(Cd.Health) / Cd.MaxHealth : 0.0f;
	const FVector2D H0 = A(47, 398), H1 = A(492, 425);
	BarFill(H0.X, H0.Y, (H1.X - H0.X) * HF, H1.Y - H0.Y, HealthColor(HF));
	const FVector2D NumP = A(627, 410);
	const FString Nums = FString::Printf(TEXT("%s / %s"), *FText::AsNumber(Cd.Health).ToString(), *FText::AsNumber(Cd.MaxHealth).ToString());
	float NumPx = 19.0f;
	while (NumPx > 12.0f && Measure(Nums, NumPx, EWeight::Bold).X > 170 * K)
	{
		NumPx -= 0.5f;
	}
	TextMid(Nums, NumP.X, NumP.Y, NumPx, KTextMain, EWeight::Bold, EAlign::Center);
}

// ---------------------------------------------------------------------------------------------
// a dica: no mouse, em cima do botão apontado

void FRTSHudPainter::DrawTooltip(const FRTSHudState& State)
{
	const FBox2D* Box = State.TooltipFor.IsNone() ? nullptr : SlotBoxes.Find(State.TooltipFor);
	if (!Box)
	{
		return;
	}
	const FString KeyStr = State.TooltipKey.IsEmpty() ? FString() : FString::Printf(TEXT("[%s]"), *State.TooltipKey);
	const FVector2D ET = Measure(State.TooltipTitle, 17.0f, EWeight::SemiBold);
	const FVector2D EK = Measure(KeyStr, 17.0f, EWeight::Bold);
	const FVector2D EC = Measure(State.TooltipCost, 16.0f, EWeight::SemiBold);
	const FVector2D EL = Measure(State.TooltipLine, 15.0f, EWeight::Medium);
	const float Pad = 14 * S;
	const float Line1 = ET.X + 6 * S + EK.X + (EC.X > 0 ? 12 * S + EC.X : 0);
	const float BW = FMath::Max(Line1, EL.X) + Pad * 2, BH = 58 * S;
	float BX = Box->Min.X + 26 * S;
	BX = FMath::Min(BX, Canvas->ClipX - BW - 8 * S);
	float BY = Box->Min.Y - BH - 6 * S;
	if (State.bCommandPanel && !State.bGroupsAtBottom)
	{
		BY = FMath::Min(BY, SY(1072.0f - CommandPanelHeight(State) - 50.0f, true) - BH - 4 * S);
	}
	Skin(SkPanelWide, BX, BY, BW, BH, SkMPanelWide, 0.2f * S);
	float TX = BX + Pad;
	Text(State.TooltipTitle, TX, BY + 8 * S, 17.0f, KTextMain, EWeight::SemiBold);
	TX += ET.X + 6 * S;
	Text(KeyStr, TX, BY + 8 * S, 17.0f, KKey, EWeight::Bold);
	TX += EK.X + 12 * S;
	if (EC.X > 0)
	{
		Text(State.TooltipCost, TX, BY + 9 * S, 16.0f, KGoldText, EWeight::SemiBold);
	}
	Text(State.TooltipLine, BX + Pad, BY + 32 * S, 15.0f, State.bTooltipLineRed ? KRed : KTextSub, EWeight::Medium);

	if (State.bDrawCursor)
	{
		// a seta do laboratório no canto de baixo da casa, sem cobrir o número do carregamento
		const FVector2D P(Box->Min.X + Box->GetSize().X * 0.74f, Box->Min.Y + Box->GetSize().Y * 0.58f);
		Tris({ P + FVector2D(-1.5f * S, -2 * S), P + FVector2D(-1.5f * S, 23 * S), P + FVector2D(17 * S, 17 * S) }, FLinearColor::Black);
		Tris({ P, P + FVector2D(0, 19 * S), P + FVector2D(13 * S, 14.5f * S) }, FLinearColor::White);
	}
}
