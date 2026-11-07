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
			if (!IsInGameThread())
			{
				// (06/11) a tela de carregar no MoviePlayer pinta noutra thread, e o HarfBuzz da Unreal só cria a fonte na thread do jogo
				// (FontCacheHarfBuzz.cpp, check(IsInGameThread()) no CreateFont: a 1ª partida caiu no teste da rede). Lá vai o caminho do
				// STextBlock no latim (o kerning do FreeType, sem HarfBuzz) e sem a lista guardada (ela é da thread do jogo)
				return Cache->ShapeUnidirectionalText(S, F, Scale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::KerningOnly);
			}
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
			// (06/11) o HarfBuzz sempre (o kerning do GPOS das fontes do mockup; ver o Unspaced)
			const FShapedGlyphSequenceRef R = Cache->ShapeUnidirectionalText(S, F, Scale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::FullShaping);
			GShaped.Add(Key, R);
			return R;
		}
	}

	namespace
	{
		/** O espaçamento do CSS em pixels do palco (o Font guardou em milésimos do tamanho em pontos) e a fonte sem ele. */
		float SpacingOf(const FSlateFontInfo& F) { return F.LetterSpacing * F.Size / 1000.0f; }
		// (06/11) 1 milésimo e não 0: a Slate só liga o kerning do HarfBuzz quando a fonte tem a tabela kern antiga ou quando há
		// espaçamento (SlateTextShaper.cpp, bHasKerning); as fontes do mockup têm o kerning no GPOS. 1/1000 do tamanho vira 0 px no
		// arredondamento da Slate, então só liga o kerning ("BATTLE" com o A e o T encostados, como no Chrome)
		FSlateFontInfo Unspaced(const FSlateFontInfo& F) { FSlateFontInfo R = F; R.LetterSpacing = 1; return R; }
		/** (06/11) O avanço de cada letra em quartos de pixel: o texto é medido moldado em 4× a escala da tela, como o navegador que
		 *  põe as letras em posição fracionária. A Slate arredonda o avanço de cada letra pro pixel inteiro, e um parágrafo de 500 px
		 *  saía ~1% mais estreito que no Chrome (as quebras de linha do SCRIPT e das opções mudavam de lugar). */
		constexpr float FINE = 4.0f;
	}

	float TextWidth(const FString& S, const FSlateFontInfo& F, float Scale)
	{
		if (!FSlateApplication::IsInitialized() || S.IsEmpty())
		{
			return 0.0f;
		}
		return float(Shape(S, Unspaced(F), Scale * FINE)->GetMeasuredWidth()) / (Scale * FINE) + SpacingOf(F) * S.Len();
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
		const FShapedGlyphSequenceRef Fine = Shape(S, F0, Scale * FINE);   // os avanços fracionários (o FINE)
		const float W = float(Fine->GetMeasuredWidth()) / (Scale * FINE) + Sp * S.Len();
		const float MaxH = float(Sh->GetMaxTextHeight()) / Scale;
		const float Base = float(Sh->GetTextBaseline()) / Scale;   // negativo: a distância do fim da célula até a base
		const float Top = BaseY - (MaxH + Base);
		const float Left = Align == 1 ? X - W * 0.5f : (Align == 2 ? X - W : X);
		// cada letra desenhada na escala da tela (nítida), na posição fracionária do texto moldado em 4×
		float Pen = Left;
		for (const FShapedGlyphEntry& G : Fine->GetGlyphsToRender())
		{
			const int32 N = FMath::Max<int32>(1, G.NumCharactersInGlyph);
			if (G.bIsVisible)
			{
				const FShapedGlyphSequenceRef One = Shape(S.Mid(G.SourceIndex, N), F0, Scale);
				FSlateDrawElement::MakeShapedText(Out, Layer, Stage.ToPaintGeometry(FVector2f(float(One->GetMeasuredWidth()) / Scale + 4.0f, MaxH),
					FSlateLayoutTransform(FVector2f(Pen, Top))), One, ESlateDrawEffect::None, Color, FLinearColor::Transparent);
			}
			Pen += float(G.XAdvance) / (Scale * FINE) + Sp * N;
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

	FLinearColor Hex(uint32 H, float A) { return Css((H >> 16) & 255, (H >> 8) & 255, H & 255, A); }
	FColor HexV(uint32 H, float A, bool bPremul) { return CssV((H >> 16) & 255, (H >> 8) & 255, H & 255, A, bPremul); }
	FLinearColor Ac(float A) { return Css(216, 173, 78, A); }
	FColor AcV(float A) { return CssV(216, 173, 78, A); }
	bool In(const FVector4f& R, FVector2f P) { return P.X >= R.X && P.X <= R.X + R.Z && P.Y >= R.Y && P.Y <= R.Y + R.W; }

	void BoxRect(FMesh& M, const FVector4f& R, FColor Fill, FColor Border)
	{
		if (Fill.A) M.Rect(R.X, R.Y, R.Z, R.W, Fill);
		if (Border.A)
		{
			M.Rect(R.X, R.Y, R.Z, 1, Border); M.Rect(R.X, R.Y + R.W - 1, R.Z, 1, Border);
			M.Rect(R.X, R.Y + 1, 1, R.W - 2, Border); M.Rect(R.X + R.Z - 1, R.Y + 1, 1, R.W - 2, Border);
		}
	}

	/** A função erro (Abramowitz e Stegun 7.1.26, erro abaixo de 1,5e-7). */
	static float ErfA(float X)
	{
		const float Sg = X < 0.0f ? -1.0f : 1.0f, A = FMath::Abs(X), T = 1.0f / (1.0f + 0.3275911f * A);
		const float P = ((((1.061405429f * T - 1.453152027f) * T + 1.421413741f) * T - 0.284496736f) * T + 0.254829592f) * T;
		return Sg * (1.0f - P * FMath::Exp(-A * A));
	}

	void GlowRect(FMesh& M, const FVector4f& R, uint32 HexC, float Alpha, float Blur)
	{
		const float Sig = FMath::Max(0.5f, Blur * 0.5f), K = 1.0f / (Sig * 1.41421356f);
		const auto Axis = [&](float A, float B, TArray<float>& Xs, TArray<float>& Ps)
		{
			for (int32 I = 0; I <= 12; ++I) { Xs.Add(A - 3.0f * Sig + 0.5f * Sig * I); Xs.Add(B - 3.0f * Sig + 0.5f * Sig * I); }
			Xs.Sort();
			TArray<float> U;
			for (const float X : Xs) if (!U.Num() || X - U.Last() > 0.25f) U.Add(X);
			Xs = U;
			for (const float X : Xs) Ps.Add(0.5f * (ErfA((X - A) * K) - ErfA((X - B) * K)));
		};
		TArray<float> Xs, Px, Ys, Py;
		Axis(R.X, R.X + R.Z, Xs, Px);
		Axis(R.Y, R.Y + R.W, Ys, Py);
		const int32 Base = M.Verts.Num(), Nx = Xs.Num();
		for (int32 J = 0; J < Ys.Num(); ++J)
			for (int32 I = 0; I < Nx; ++I)
				M.Vert(FVector2f(Xs[I], Ys[J]), FVector2f(0, 0), HexV(HexC, Alpha * Px[I] * Py[J]));
		for (int32 J = 0; J + 1 < Ys.Num(); ++J)
			for (int32 I = 0; I + 1 < Nx; ++I)
			{
				const int32 A = Base + J * Nx + I;
				M.Tri(A, A + 1, A + Nx + 1); M.Tri(A, A + Nx + 1, A + Nx);
			}
	}

	void Disc(FMesh& M, FVector2f C, float R, FColor Col, int32 Seg) { M.Poly(Arc(C, R, 0.0f, 2.0f * PI, Seg), Col); }

	void Chev(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, const FLinearColor& Col, float Half)
	{
		Lines(Out, L, S, { FVector2f(C.X - Half, C.Y - Half * 0.5f), FVector2f(C.X, C.Y + Half * 0.5f), FVector2f(C.X + Half, C.Y - Half * 0.5f) }, Col, 1.5f);
	}

	FGeometry StageFit(const FGeometry& G)
	{
		const FVector2f Local = FVector2f(G.GetLocalSize());
		const float S = FMath::Min(Local.X / StageW, Local.Y / StageH);
		const FVector2f Offset = (Local - FVector2f(StageW, StageH) * S) * 0.5f;
		return G.MakeChild(FVector2f(StageW, StageH), FSlateLayoutTransform(S, Offset));
	}

	void Backdrop(FSlateWindowElementList& Out, int32& L, const FGeometry& Allotted, const FGeometry& S, const TCHAR* GlowImage)
	{
		FSlateDrawElement::MakeBox(Out, L++, Allotted.ToPaintGeometry(), White(), ESlateDrawEffect::None, Css(10, 13, 11));
		const FPaintGeometry Full = S.ToPaintGeometry(FVector2f(StageW, StageH), FSlateLayoutTransform());
		FSlateDrawElement::MakeBox(Out, L++, Full, Image(GlowImage, FVector2f(480, 270)), ESlateDrawEffect::None, FLinearColor::White);
		FMesh Grid(S);
		const FColor C = CssV(255, 255, 255, 0.02f);
		for (float X = 0.0f; X < StageW; X += 48.0f) Grid.Rect(X, 0.0f, 1.0f, StageH, C);
		for (float Y = 0.0f; Y < StageH; Y += 48.0f) Grid.Rect(0.0f, Y, StageW, 1.0f, C);
		Grid.Draw(Out, L++, White());
	}

	FVector4f BackRect() { return FVector4f(110, 30, 156.55f, 44); }

	void BackButton(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float Hover)
	{
		// o .backbtn: o fundo, a borda que vai pro âmbar no hover (.15 s), a seta, o BACK e a tecla ESC
		const FLinearColor Bc = FMath::Lerp(Css(238, 241, 244, 0.9f), Ac(), Hover);
		{
			FMesh M(S);
			const FLinearColor B0 = Css(230, 220, 190, 0.32f), B1 = Ac(0.8f);
			BoxRect(M, BackRect(), CssV(16, 21, 19, 0.85f), FMath::Lerp(B0, B1, Hover).ToFColorSRGB());
			M.Draw(Out, L++, White());
		}
		Lines(Out, L, S, { FVector2f(134.75f, 45.58f), FVector2f(128.33f, 52.0f), FVector2f(134.75f, 58.42f) }, Bc, 2.2f);
		Text(Out, L, S, TEXT("BACK"), Font(EFace::Shoulders800, 19, 3), 155, 59.5f, Bc);
		const FLinearColor Kc(Bc.R, Bc.G, Bc.B, Bc.A * 0.7f);
		Lines(Out, L, S, { FVector2f(213.23f, 42.5f), FVector2f(249.04f, 42.5f), FVector2f(249.04f, 61.5f), FVector2f(213.23f, 61.5f), FVector2f(213.23f, 42.5f) }, Kc, 1.0f);
		Text(Out, L, S, TEXT("ESC"), Font(EFace::Mono600, 11, 1), 219.73f, 56, Kc);
		++L;
	}

	void StageCorners(FMesh& M)
	{
		const FColor Cc = CssV(216, 173, 78, 0.45f);
		M.Rect(40, 40, 24, 2, Cc); M.Rect(40, 40, 2, 24, Cc);
		M.Rect(1856, 40, 24, 2, Cc); M.Rect(1878, 40, 2, 24, Cc);
		M.Rect(40, 1038, 24, 2, Cc); M.Rect(40, 1016, 2, 24, Cc);
		M.Rect(1856, 1038, 24, 2, Cc); M.Rect(1878, 1016, 2, 24, Cc);
	}
}


