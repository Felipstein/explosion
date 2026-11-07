#pragma once

// As peças de desenho das telas nativas (03/11, o refazer nativo das telas aprovadas; docs/TELAS_NO_JOGO.md): as fontes do mockup
// lidas dos TTF (como o HUD), as cores do CSS, o texto posto pela linha de base (como o fillText do canvas e as medidas tiradas do
// Chrome), as curvas de tempo do CSS e uma malha de vértices pra desenhar muito de uma vez (pontos, leques, linhas retas).
// Tudo em pixels do palco de 1920 × 1080 do mockup; a tela escala o palco inteiro.

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "Rendering/RenderingCommon.h"

class FSlateWindowElementList;
struct FSlateBrush;

namespace RTSShellUI
{
	constexpr float StageW = 1920.0f;
	constexpr float StageH = 1080.0f;

	/** As faces das três famílias do mockup (Big Shoulders Display, Barlow Semi Condensed, IBM Plex Mono), pelo peso do CSS. */
	enum class EFace : uint8
	{
		Shoulders700, Shoulders800, Shoulders900,
		Barlow400, Barlow500, Barlow600, Barlow700,
		Mono400, Mono500, Mono600,
		Count
	};

	FString PluginDir();

	/** A fonte no tamanho do CSS (px) e o espaçamento entre letras do CSS (px). A Slate mede em pontos a 96 DPI (px × 0,75) e o
	 *  espaçamento em milésimos do tamanho em pontos (SlateTextShaper.cpp): os dois convertidos aqui. */
	FSlateFontInfo Font(EFace Face, float Px, float SpacingPx = 0.0f);

	/** A cor do CSS (sRGB, 0–255, alfa 0–1) como a Slate quer: a FLinearColor convertida do sRGB. */
	FLinearColor Css(int32 R, int32 G, int32 B, float A = 1.0f);
	/** A mesma cor pros vértices (sRGB); Premul = a cor já multiplicada pelo alfa. */
	FColor CssV(int32 R, int32 G, int32 B, float A = 1.0f, bool bPremul = false);

	/** A largura do texto em unidades do palco, com o espaçamento entre letras (o texto moldado na escala dada). */
	float TextWidth(const FString& Text, const FSlateFontInfo& Font, float Scale = 1.0f);

	/** O texto com a linha de base em BaseY (o fillText do canvas, as bases medidas no Chrome); Align 0 = esquerda, 1 = meio,
	 *  2 = direita. Devolve a largura. */
	float Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const FString& Text, const FSlateFontInfo& Font,
		float X, float BaseY, const FLinearColor& Color, int32 Align = 0);

	/** A curva cubic-bezier do CSS (o x é o tempo; resolve o t por Newton e devolve o y, que pode passar de 1). */
	float Bezier(float X1, float Y1, float X2, float Y2, float T);
	inline float Ease(float T) { return Bezier(0.25f, 0.1f, 0.25f, 1.0f, T); }

	/** Uma imagem de Resources/Menu (carregada uma vez). */
	const FSlateBrush* Image(const FString& File, FVector2f Size);
	const FSlateBrush* White();

	/** Uma transição do CSS: do valor de agora até o alvo, na duração e na curva dadas. */
	struct FTween
	{
		float From = 0.0f, To = 0.0f;
		double Start = -1.0;
		float Duration = 0.2f;
		float X1 = 0.25f, Y1 = 0.1f, X2 = 0.25f, Y2 = 1.0f;
		void Set(float Target, double Now, float CurrentValue) { if (Target != To) { From = CurrentValue; To = Target; Start = Now; } }
		float Value(double Now) const
		{
			if (Start < 0.0) return To;
			const float K = FMath::Clamp(float((Now - Start) / Duration), 0.0f, 1.0f);
			return From + (To - From) * Bezier(X1, Y1, X2, Y2, K);
		}
	};

	/** Muitos triângulos de uma vez (uma só ordem de desenho): pontos, quadrados, leques do radar, linhas retas de 1 px. */
	struct FMesh
	{
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indexes;
		FSlateRenderTransform Transform;

		explicit FMesh(const FGeometry& Stage) : Transform(Stage.GetAccumulatedRenderTransform()) {}
		int32 Vert(FVector2f P, FVector2f UV, FColor C);
		void Tri(int32 A, int32 B, int32 C) { Indexes.Add(A); Indexes.Add(B); Indexes.Add(C); }
		void Rect(float X, float Y, float W, float H, FColor C);
		void Poly(const TArray<FVector2f>& Points, FColor C);   // um polígono convexo cheio
		void Draw(FSlateWindowElementList& Out, int32 Layer, const FSlateBrush* Brush, ESlateDrawEffect Effect = ESlateDrawEffect::None) const;
	};

	/** Uma linha (ou várias ligadas) com o serrilhado suavizado da Slate, em pixels do palco. */
	void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const TArray<FVector2f>& Points, const FLinearColor& Color, float Width = 1.0f);
	void Dashed(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Stage, const TArray<FVector2f>& Points, const FLinearColor& Color, float Width, float Dash);
	/** Um círculo (ou um arco) como linha. */
	TArray<FVector2f> Arc(FVector2f C, float R, float A0 = 0.0f, float A1 = 2.0f * PI, int32 Segments = 0);

	// (06/11) as peças comuns das telas nativas (vieram da sala do skirmish; o multiplayer, as opções e o SCRIPT usam as mesmas)
	/** A cor 0xRRGGBB como a Slate quer (Hex) e pros vértices (HexV). */
	FLinearColor Hex(uint32 H, float A = 1.0f);
	FColor HexV(uint32 H, float A = 1.0f, bool bPremul = false);
	/** O âmbar do mockup (rgb(216,173,78)). */
	FLinearColor Ac(float A = 1.0f);
	FColor AcV(float A = 1.0f);
	/** O ponto dentro do retângulo (x, y, largura, altura). */
	bool In(const FVector4f& R, FVector2f P);
	/** Um retângulo com fundo e borda de 1 px (as caixas do CSS); cor de alfa 0 = sem. */
	void BoxRect(FMesh& M, const FVector4f& R, FColor Fill, FColor Border);
	/** O box-shadow do CSS (0 0 Bpx cor; pra sombra, o retângulo já deslocado): o retângulo borrado por uma gaussiana de sigma B/2,
	 *  como o navegador faz. A gaussiana de um retângulo é separável (o perfil em x vezes o perfil em y), então a grade leva o alfa
	 *  exato em cada vértice, de meio sigma em meio sigma perto das bordas, e a placa interpola no meio. Sem degrau nenhum. */
	void GlowRect(FMesh& M, const FVector4f& R, uint32 HexC, float Alpha, float Blur);
	/** Um disco cheio. */
	void Disc(FMesh& M, FVector2f C, float R, FColor Col, int32 Seg = 32);
	/** A setinha das células (o quadrado com duas bordas girado 45°). */
	void Chev(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, const FLinearColor& Col, float Half = 4.24f);
	/** O palco de 1920 × 1080 dentro do widget (a escala que cabe, centrado), e o ponto do mouse no palco. */
	FGeometry StageFit(const FGeometry& Allotted);
	/** O fundo das telas (#stage): a cor, o brilho do alto (uma imagem 480 × 270 de Resources/Menu) e a grade de 48 px. */
	void Backdrop(FSlateWindowElementList& Out, int32& L, const FGeometry& Allotted, const FGeometry& S, const TCHAR* GlowImage);
	/** O BACK do canto (110, 30, com o ESC), no hover 0–1 (a cor vai pro âmbar). */
	void BackButton(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float Hover);
	/** O retângulo do BACK, pra acertar o mouse. */
	FVector4f BackRect();
	/** As quatro cantoneiras do palco (24 px, 2 de espessura, âmbar a 45%). */
	void StageCorners(FMesh& M);
}


