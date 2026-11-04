#include "RTSShellUI.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Hash/CityHash.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace RTSShellUI
{
	FString PluginDir()
	{
		static const FString Dir = []()
		{
			const TSharedPtr<IPlugin> P = IPluginManager::Get().FindPlugin(TEXT("RTSShell"));
			return P.IsValid() ? FPaths::ConvertRelativePathToFull(P->GetBaseDir()) : FString();
		}();
		return Dir;
	}

	FSlateFontInfo Font(EFace Face, float Px, float SpacingPx)
	{
		static const TCHAR* Files[int32(EFace::Count)] = {
			TEXT("BigShouldersDisplay-Bold.ttf"), TEXT("BigShouldersDisplay-ExtraBold.ttf"), TEXT("BigShouldersDisplay-Black.ttf"),
			TEXT("BarlowSemiCondensed-Regular.ttf"), TEXT("BarlowSemiCondensed-Medium.ttf"), TEXT("BarlowSemiCondensed-SemiBold.ttf"), TEXT("BarlowSemiCondensed-Bold.ttf"),
			TEXT("IBMPlexMono-Regular.ttf"), TEXT("IBMPlexMono-Medium.ttf"), TEXT("IBMPlexMono-SemiBold.ttf") };
		static TSharedPtr<const FCompositeFont> Faces[int32(EFace::Count)];
		TSharedPtr<const FCompositeFont>& F = Faces[int32(Face)];
		if (!F.IsValid())
		{
			// sem o hinting da fonte: o Chrome no Windows desenha sem ajustar o contorno à grade, e o título largo fica igual
			F = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), PluginDir() / TEXT("Resources/Fonts") / Files[int32(Face)],
				EFontHinting::None, EFontLoadingPolicy::LazyLoad);
		}
		const float Pt = Px * 0.75f;
		FSlateFontInfo Info(F, Pt, FName(TEXT("Regular")));
		Info.LetterSpacing = FMath::RoundToInt(SpacingPx * 1000.0f / Pt);
		return Info;
	}

	FLinearColor Css(int32 R, int32 G, int32 B, float A)
	{
		FLinearColor C = FLinearColor(FColor(uint8(R), uint8(G), uint8(B), 255));
		C.A = FMath::Clamp(A, 0.0f, 1.0f);
		return C;
	}

	FColor CssV(int32 R, int32 G, int32 B, float A, bool bPremul)
	{
		const float K = FMath::Clamp(A, 0.0f, 1.0f);
		const float M = bPremul ? K : 1.0f;
		return FColor(uint8(FMath::RoundToInt(R * M)), uint8(FMath::RoundToInt(G * M)), uint8(FMath::RoundToInt(B * M)), uint8(FMath::RoundToInt(K * 255.0f)));
	}

	namespace
	{
		// o texto moldado (o caminho do STextBlock), guardado por texto, fonte, tamanho e escala (moldar custa); a lista esvazia
		// quando a Slate solta as fontes. O espaçamento entre letras não passa pela Slate: ela só espaça em pixels inteiros por letra
		// (SlateTextShaper.cpp corta a fração), e o mockup tem 2,5 px e o espaçamento que anda no hover. Cada letra vai na posição
		// exata, como no navegador: o avanço dela (com o kerning) mais o espaçamento do CSS depois de cada letra.
		// a chave é o hash dos bytes (diferencia maiúscula de minúscula: um TMap de FString não diferencia, e o "t" saía com o "T" que
		// tinha sido moldado antes, 04/11)
		TMap<uint64, FShapedGlyphSequencePtr> GShaped;

		FShapedGlyphSequenceRef Shape(const FString& S, const FSlateFontInfo& F, float Scale)
		{
			const TSharedRef<FSlateFontCache> Cache = FSlateApplication::Get().GetRenderer()->GetFontCache();
			static bool bHooked = false;
			if (!bHooked)
			{
				Cache->OnReleaseResources().AddLambda([](const FSlateFontCache&) { GShaped.Reset(); });
				bHooked = true;
			}
			const FString Params = FString::Printf(TEXT("%p|%.3f|%d|%.4f|%s"), F.CompositeFont.Get(), F.Size, F.LetterSpacing, Scale, *S);
			const uint64 Key = CityHash64(reinterpret_cast<const char*>(*Params), uint32(Params.Len() * sizeof(TCHAR)));
			if (const FShapedGlyphSequencePtr* Found = GShaped.Find(Key))
			{
				return Found->ToSharedRef();
			}
			if (GShaped.Num() > 2048)
			{
				GShaped.Reset();
			}
			const FShapedGlyphSequenceRef R = Cache->ShapeUnidirectionalText(S, F, Scale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
			GShaped.Add(Key, R);
			return R;
		}
	}

	namespace
	{
		/** O espaçamento do CSS em pixels do palco (o Font guardou em milésimos do tamanho em pontos) e a fonte sem ele. */
		float SpacingOf(const FSlateFontInfo& F) { return F.LetterSpacing * F.Size / 1000.0f; }
		FSlateFontInfo Unspaced(const FSlateFontInfo& F) { FSlateFontInfo R = F; R.LetterSpacing = 0; return R; }
	}

	float TextWidth(const FString& S, const FSlateFontInfo& F, float Scale)
	{
		if (!FSlateApplication::IsInitialized() || S.IsEmpty())
		{
			return 0.0f;
		}
		return float(Shape(S, Unspaced(F), Scale)->GetMeasuredWidth()) / Scale + SpacingOf(F) * S.Len();
	}

	float Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const FString& S, const FSlateFontInfo& F,
		float X, float BaseY, const FLinearColor& Color, int32 Align)
	{
		if (S.IsEmpty() || Color.A <= 0.002f || !FSlateApplication::IsInitialized())
		{
			return 0.0f;
		}
		// moldado na escala da tela (a fonte sai nítida, sem esticar) e posto em unidades do palco
		const float Scale = Stage.GetAccumulatedLayoutTransform().GetScale();
		const FSlateFontInfo F0 = Unspaced(F);
		const float Sp = SpacingOf(F);
		const FShapedGlyphSequenceRef Sh = Shape(S, F0, Scale);
		const float W = float(Sh->GetMeasuredWidth()) / Scale + Sp * S.Len();
		const float MaxH = float(Sh->GetMaxTextHeight()) / Scale;
		const float Base = float(Sh->GetTextBaseline()) / Scale;   // negativo: a distância do fim da célula até a base
		const float Top = BaseY - (MaxH + Base);
		const float Left = Align == 1 ? X - W * 0.5f : (Align == 2 ? X - W : X);
		if (Sp == 0.0f)
		{
			FSlateDrawElement::MakeShapedText(Out, Layer, Stage.ToPaintGeometry(FVector2f(W + 4.0f, MaxH), FSlateLayoutTransform(FVector2f(Left, Top))), Sh,
				ESlateDrawEffect::None, Color, FLinearColor::Transparent);
			return W;
		}
		float Pen = Left;
		for (const FShapedGlyphEntry& G : Sh->GetGlyphsToRender())
		{
			const int32 N = FMath::Max<int32>(1, G.NumCharactersInGlyph);
			if (G.bIsVisible)
			{
				const FShapedGlyphSequenceRef One = Shape(S.Mid(G.SourceIndex, N), F0, Scale);
				FSlateDrawElement::MakeShapedText(Out, Layer, Stage.ToPaintGeometry(FVector2f(float(One->GetMeasuredWidth()) / Scale + 4.0f, MaxH),
					FSlateLayoutTransform(FVector2f(Pen, Top))), One, ESlateDrawEffect::None, Color, FLinearColor::Transparent);
			}
			Pen += float(G.XAdvance) / Scale + Sp * N;
		}
		return W;
	}

	float Bezier(float X1, float Y1, float X2, float Y2, float T)
	{
		if (T <= 0.0f) return 0.0f;
		if (T >= 1.0f) return 1.0f;
		const auto Cx = [&](float U) { return 3.0f * X1 * U * (1 - U) * (1 - U) + 3.0f * X2 * U * U * (1 - U) + U * U * U; };
		const auto Cy = [&](float U) { return 3.0f * Y1 * U * (1 - U) * (1 - U) + 3.0f * Y2 * U * U * (1 - U) + U * U * U; };
		float U = T;
		for (int32 I = 0; I < 8; ++I)
		{
			const float Dx = (Cx(U + 1e-4f) - Cx(U - 1e-4f)) / 2e-4f;
			if (FMath::Abs(Dx) < 1e-5f) break;
			U = FMath::Clamp(U - (Cx(U) - T) / Dx, 0.0f, 1.0f);
		}
		return Cy(U);
	}

	const FSlateBrush* Image(const FString& File, FVector2f Size)
	{
		static TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> Cache;
		TSharedPtr<FSlateDynamicImageBrush>& B = Cache.FindOrAdd(File);
		if (!B.IsValid())
		{
			B = MakeShared<FSlateDynamicImageBrush>(FName(*(PluginDir() / TEXT("Resources/Menu") / File)), FVector2D(Size));
		}
		return B.Get();
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	}

	int32 FMesh::Vert(FVector2f P, FVector2f UV, FColor C)
	{
		return Verts.Add(FSlateVertex::Make(Transform, P, UV, C));
	}

	void FMesh::Rect(float X, float Y, float W, float H, FColor C)
	{
		const int32 A = Vert(FVector2f(X, Y), FVector2f(0, 0), C), B = Vert(FVector2f(X + W, Y), FVector2f(1, 0), C);
		const int32 D = Vert(FVector2f(X + W, Y + H), FVector2f(1, 1), C), E = Vert(FVector2f(X, Y + H), FVector2f(0, 1), C);
		Tri(A, B, D);
		Tri(A, D, E);
	}

	void FMesh::Poly(const TArray<FVector2f>& Points, FColor C)
	{
		if (Points.Num() < 3) return;
		const int32 First = Vert(Points[0], FVector2f(0, 0), C);
		for (int32 I = 1; I + 1 < Points.Num(); ++I)
		{
			const int32 B = Vert(Points[I], FVector2f(0, 0), C);
			const int32 D = Vert(Points[I + 1], FVector2f(0, 0), C);
			Tri(First, B, D);
		}
	}

	void FMesh::Draw(FSlateWindowElementList& Out, int32 Layer, const FSlateBrush* Brush, ESlateDrawEffect Effect) const
	{
		if (Verts.Num() == 0 || Indexes.Num() == 0 || !Brush || !FSlateApplication::IsInitialized())
		{
			return;
		}
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Indexes, nullptr, 0, 0, Effect);
	}

	void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const TArray<FVector2f>& Points, const FLinearColor& Color, float Width)
	{
		if (Points.Num() >= 2 && Color.A > 0.002f)
		{
			FSlateDrawElement::MakeLines(Out, Layer, Stage.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Width);
		}
	}

	void Dashed(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const TArray<FVector2f>& Points, const FLinearColor& Color, float Width, float Dash)
	{
		if (Points.Num() >= 2 && Color.A > 0.002f)
		{
			// a Slate mede o traço em pixels da tela: o do mockup é em pixels do palco
			const float Scale = Stage.GetAccumulatedLayoutTransform().GetScale();
			TArray<FVector2f> P = Points;
			FSlateDrawElement::MakeDashedLines(Out, Layer, Stage.ToPaintGeometry(), MoveTemp(P), ESlateDrawEffect::None, Color, Width, Dash * Scale);
		}
	}

	TArray<FVector2f> Arc(FVector2f C, float R, float A0, float A1, int32 Segments)
	{
		const int32 N = Segments > 0 ? Segments : FMath::Clamp(int32(R * FMath::Abs(A1 - A0) / 4.0f), 12, 720);
		TArray<FVector2f> P;
		P.Reserve(N + 1);
		for (int32 I = 0; I <= N; ++I)
		{
			const float A = A0 + (A1 - A0) * I / N;
			P.Add(FVector2f(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R));
		}
		return P;
	}
}
