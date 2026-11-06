#include "RTSSoftDraw.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "GlobalRenderResources.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/** A faixa e o disco (as pontas, as fichas) têm a largura + 1 px, com o alfa 0,5 na borda certa. Um par por largura (em
	 *  quarto de pixel), feito uma vez. */
	UTexture2D* SoftTexture(float WidthPx, bool bDisc)
	{
		static TMap<int32, TStrongObjectPtr<UTexture2D>> Cache;
		const int32 Key = FMath::RoundToInt32(WidthPx * 4.0f) * 2 + (bDisc ? 1 : 0);
		if (const TStrongObjectPtr<UTexture2D>* Have = Cache.Find(Key))
		{
			return Have->Get();
		}
		constexpr int32 N = 64;
		const int32 Rows = bDisc ? N : 1;
		UTexture2D* T = UTexture2D::CreateTransient(N, Rows, PF_B8G8R8A8);
		if (!T)
		{
			return nullptr;
		}
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->SRGB = true;
		const float Edge = 1.0f / FMath::Max(1.0f, WidthPx + 1.0f);   // um pixel de tela na escala da textura
		FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
		FColor* Px = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Rows; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float U = (X + 0.5f) / N - 0.5f;
				const float V = bDisc ? (Y + 0.5f) / N - 0.5f : 0.0f;
				const float R = bDisc ? FMath::Sqrt(U * U + V * V) : FMath::Abs(U);
				Px[Y * N + X] = FColor(255, 255, 255, uint8(FMath::RoundToInt(255.0f * FMath::Clamp((0.5f - R) / Edge, 0.0f, 1.0f))));
			}
		}
		Mip.BulkData.Unlock();
		T->UpdateResource();
		Cache.Add(Key, TStrongObjectPtr<UTexture2D>(T));
		return T;
	}

	void AddQuad(TArray<FCanvasUVTri>& Tris, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
		const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD, const FLinearColor& Color)
	{
		// A-B um lado, C-D o outro (A frente de C)
		FCanvasUVTri T;
		T.V0_Pos = A; T.V0_UV = UA; T.V0_Color = Color;
		T.V1_Pos = B; T.V1_UV = UB; T.V1_Color = Color;
		T.V2_Pos = C; T.V2_UV = UC; T.V2_Color = Color;
		Tris.Add(T);
		T.V0_Pos = C; T.V0_UV = UC;
		T.V1_Pos = B; T.V1_UV = UB;
		T.V2_Pos = D; T.V2_UV = UD;
		Tris.Add(T);
	}

	void Flush(UCanvas* Canvas, TArray<FCanvasUVTri>& Tris, const FTexture* Texture)
	{
		if (Tris.Num() && Texture)
		{
			FCanvasTriangleItem Item(Tris, Texture);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		Tris.Reset();
	}

	const FTexture* ResourceOf(UTexture2D* T)
	{
		return T ? T->GetResource() : nullptr;
	}
}

FLinearColor RTSSoftDraw::Mix(const FColor& A, const FColor& B, float T)
{
	const auto L = [T](uint8 X, uint8 Y) { return uint8(FMath::RoundToInt(FMath::Lerp(float(X), float(Y), T))); };
	return FLinearColor(FColor(L(A.R, B.R), L(A.G, B.G), L(A.B, B.B)));
}

void RTSSoftDraw::Stroke(UCanvas* Canvas, const TArray<FVector2D>& Pts, float Width, const FLinearColor& Color, bool bClosed)
{
	const int32 Num = Pts.Num();
	if (!Canvas || Num < 2)
	{
		return;
	}
	const float Half = (Width + 1.0f) * 0.5f;
	TArray<FVector2D> L, R;
	for (int32 K = 0; K < Num; ++K)
	{
		const bool bHasPrev = bClosed || K > 0;
		const bool bHasNext = bClosed || K + 1 < Num;
		const FVector2D Prev = Pts[(K + Num - 1) % Num], Next = Pts[(K + 1) % Num];
		const FVector2D In = (bHasPrev ? Pts[K] - Prev : Next - Pts[K]).GetSafeNormal();
		const FVector2D Out = bHasNext ? (Next - Pts[K]).GetSafeNormal() : In;
		FVector2D T = (In + Out).GetSafeNormal();
		if (T.IsNearlyZero())
		{
			T = Out;
		}
		const FVector2D N(-T.Y, T.X);
		const float Miter = Half / FMath::Max(0.5f, float(FVector2D::DotProduct(N, FVector2D(-In.Y, In.X))));
		L.Add(Pts[K] + N * Miter);
		R.Add(Pts[K] - N * Miter);
	}
	TArray<FCanvasUVTri> Tris;
	const FVector2D U0(0.0f, 0.5f), U1(1.0f, 0.5f);
	const int32 Segments = bClosed ? Num : Num - 1;
	for (int32 K = 0; K < Segments; ++K)
	{
		const int32 J = (K + 1) % Num;
		AddQuad(Tris, L[K], R[K], L[J], R[J], U0, U1, U0, U1, Color);
	}
	Flush(Canvas, Tris, ResourceOf(SoftTexture(Width, false)));
	if (bClosed)
	{
		return;
	}
	// as pontas: meio disco além de cada ponta
	const auto Cap = [&](const FVector2D& E, const FVector2D& D)
	{
		const FVector2D N(-D.Y, D.X);
		AddQuad(Tris, E + N * Half, E - N * Half, E + (D + N) * Half, E + (D - N) * Half,
			FVector2D(1.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f), Color);
	};
	Cap(Pts[0], (Pts[0] - Pts[1]).GetSafeNormal());
	Cap(Pts.Last(), (Pts.Last() - Pts[Num - 2]).GetSafeNormal());
	Flush(Canvas, Tris, ResourceOf(SoftTexture(Width, true)));
}

void RTSSoftDraw::Disc(UCanvas* Canvas, const FVector2D& At, float Radius, const FLinearColor& Color)
{
	if (!Canvas)
	{
		return;
	}
	const float Half = Radius + 0.5f;
	TArray<FCanvasUVTri> Tris;
	AddQuad(Tris, At + FVector2D(-Half, -Half), At + FVector2D(Half, -Half), At + FVector2D(-Half, Half), At + FVector2D(Half, Half),
		FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), Color);
	Flush(Canvas, Tris, ResourceOf(SoftTexture(Radius * 2.0f, true)));
}

void RTSSoftDraw::Triangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
{
	if (!Canvas)
	{
		return;
	}
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T;
	T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
	T.V0_Color = T.V1_Color = T.V2_Color = Color;
	Tris.Add(T);
	Flush(Canvas, Tris, GWhiteTexture);
}

void RTSSoftDraw::Fill(UCanvas* Canvas, const TArray<FVector2D>& Pts, const FLinearColor& Color)
{
	if (!Canvas || Pts.Num() < 3)
	{
		return;
	}
	TArray<FCanvasUVTri> Tris;
	for (int32 K = 1; K + 1 < Pts.Num(); ++K)
	{
		FCanvasUVTri T;
		T.V0_Pos = Pts[0]; T.V1_Pos = Pts[K]; T.V2_Pos = Pts[K + 1];
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
	}
	Flush(Canvas, Tris, GWhiteTexture);
	Outline(Canvas, Pts, 1.0f, Color);
}

void RTSSoftDraw::Outline(UCanvas* Canvas, const TArray<FVector2D>& Pts, float Width, const FLinearColor& Color)
{
	for (int32 K = 0; K < Pts.Num(); ++K)
	{
		Stroke(Canvas, { Pts[K], Pts[(K + 1) % Pts.Num()] }, Width, Color);
	}
}

void RTSSoftDraw::Band(UCanvas* Canvas, const TArray<FVector2D>& Inner, const TArray<FVector2D>& Outer, const FLinearColor& InnerColor, const FLinearColor& OuterColor)
{
	if (!Canvas || Inner.Num() < 2 || Inner.Num() != Outer.Num())
	{
		return;
	}
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Inner.Num() * 2);
	for (int32 K = 0; K < Inner.Num(); ++K)
	{
		const int32 N = (K + 1) % Inner.Num();
		FCanvasUVTri A;
		A.V0_Pos = Inner[K]; A.V1_Pos = Outer[K]; A.V2_Pos = Outer[N];
		A.V0_Color = InnerColor; A.V1_Color = A.V2_Color = OuterColor;
		FCanvasUVTri B;
		B.V0_Pos = Inner[K]; B.V1_Pos = Outer[N]; B.V2_Pos = Inner[N];
		B.V0_Color = B.V2_Color = InnerColor; B.V1_Color = OuterColor;
		Tris.Add(A);
		Tris.Add(B);
	}
	Flush(Canvas, Tris, GWhiteTexture);
}
