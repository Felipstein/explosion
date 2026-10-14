#pragma once

// Desenha o FRTSHudState no Canvas, nas posições da planta (docs/concept/hud/wireframe/, 1920×1080):
// as medidas são da planta e escalam pela altura da tela; as peças de canto ficam presas no canto.

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "RTSHudTypes.h"

class UCanvas;
class UTexture;
struct FCompositeFont;

class RTSHUD_API FRTSHudPainter
{
public:
	/** O nome → a textura (UTexture: no jogo o "minimap" pode ser o render target do minimapa de verdade). */
	using FTextureLookup = TFunction<UTexture*(FName)>;

	/** FontDir: a pasta com as BarlowSemiCondensed-*.ttf. */
	FRTSHudPainter(FTextureLookup InLookup, const FString& FontDir);

	void Draw(UCanvas* InCanvas, const FRTSHudState& State, TArray<FRTSHudButton>& OutButtons);

	/**
	 * Onde fica o buraco do losango do minimapa numa tela ClipX × ClipY: o centro e a meia diagonal, em
	 * pixels da tela. O jogo desenha o minimapa de verdade ali, por baixo da moldura.
	 */
	static void MinimapHole(float ClipX, float ClipY, FVector2D& OutCenter, float& OutHalfDiagonal);

	/**
	 * A escala e a faixa da planta na tela: a planta (1920×1080) cabe inteira (a menor das duas escalas), e
	 * numa tela mais larga que 16:9 o HUD fica numa faixa 16:9 no meio, pro olho não atravessar a tela
	 * ultrawide pra achar o minimapa e o cartão.
	 */
	static void Layout(float ClipX, float ClipY, float& OutS, float& OutLeft, float& OutWidth);

private:
	enum class EAnchor : uint8 { Left, Center, Right };
	enum class EWeight : uint8 { Medium, SemiBold, Bold };
	enum class EAlign : uint8 { Left, Center, Right };

	// a planta em pixels de 1920×1080 → pixels da tela
	float S = 1.0f;
	float LayoutLeft = 0.0f, LayoutWidth = 1920.0f; // a faixa da planta na tela (Layout)
	float SX(float X, EAnchor A) const;
	float SY(float Y, bool bBottom) const;

	void DrawTop(const FRTSHudState& State);
	void DrawTop1v1(const FRTSHudState& State);
	void OutlinePoly(const TArray<FVector2D>& Poly, const FLinearColor& Rim);
	void DrawCommander(const FRTSHudState& State);
	void DrawAlerts(const FRTSHudState& State);
	void DrawPowers(const FRTSHudState& State);
	void DrawMinimap(const FRTSHudState& State);
	void DrawGroups(const FRTSHudState& State);
	void DrawCommandPanel(const FRTSHudState& State);
	/** A altura (na planta) do painel central: 180 com 9 colunas, mais alto com as casas maiores. */
	static float CommandPanelHeight(const FRTSHudState& State);
	void DrawCard(const FRTSHudState& State);
	void DrawTooltip(const FRTSHudState& State);

	void DrawSlot(const FRTSHudSlot& Slot, float X, float Y, float W, float H);
	/** O mouse em cima (moldura acesa) e o aperto (afunda e escurece) de um botão qualquer. */
	void HoverPress(FName Id, float X, float Y, float W, float H, const FLinearColor& Rim);
	/** 0..1, uma pulsação suave de Hz por segundo. */
	float Pulse(float Hz) const;
	/** A cor do neon da pele da facção (o mouse em cima acende nela). */
	FLinearColor Accent() const;
	/** Quanto o painel central ainda está abaixo do lugar (a subida do PanelIn), em pixels da tela. */
	float PanelDrop(const FRTSHudState& State) const;
	/** A caixa das superarmas (mini tecla azul no seu time, escura no inimigo, vermelha quando falta pouco). */
	void DrawSuperweapons(const TArray<FRTSHudSuperweapon>& List, float X, float Y, float W, float RowH, int32 MaxRows, float NamePx, bool bTeam);
	/** O maior tamanho de letra (até NamePx) em que todos os nomes da lista cabem junto do tempo. */
	float SuperweaponNamePx(const TArray<FRTSHudSuperweapon>& List, float W, float RowH, int32 MaxRows, float NamePx) const;
	void Radiation(FVector2D C, float R);

	// primitivas (já em pixels da tela)
	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	void Border(float X, float Y, float W, float H, float T, const FLinearColor& C);
	void DashedBorder(float X, float Y, float W, float H, float T, float Dash, const FLinearColor& C);
	void Line(FVector2D A, FVector2D B, float T, const FLinearColor& C);
	void Image(FName Name, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White, bool bCover = true);
	/** O ícone sem a moldura dele (Inset = a fração cortada de cada lado): vai no encaixe da pele. */
	void ImageInset(FName Name, float X, float Y, float W, float H, float Inset, const FLinearColor& Tint = FLinearColor::White);
	/**
	 * A pele (as molduras do kit, docs/concept/hud/kit/skin/) em 9 fatias: as margens (em pixels da textura)
	 * saem na tela vezes Scale e o miolo estica; Src recorta só um pedaço da textura (vazio = ela toda).
	 */
	void Skin(FName Name, float X, float Y, float W, float H, const FMargin& Margin = FMargin(0.0f), float Scale = 1.0f, const FLinearColor& Tint = FLinearColor::White, const FBox2D& Src = FBox2D(ForceInit));
	/** A barra num rebaixo da pele (o encaixe da vida do cartão), cheia até Fill. */
	void Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C);
	/** O enchimento da barra: a cor com brilho em cima e sombra embaixo. */
	void BarFill(float X, float Y, float W, float H, const FLinearColor& C);
	void Tris(const TArray<FVector2D>& Pts, const FLinearColor& C); // trios de pontos
	void Sweep(float X, float Y, float W, float H, float Done, float Alpha); // o fundo escuro do C&C
	void Star(FVector2D C, float R, const FLinearColor& Col);
	void Frame(float X, float Y, float W, float H, const FLinearColor& Edge, float FillAlpha = 0.94f);
	// o material das peças aprovadas: metal chanfrado com aro em degradê e corpo em degradê
	using FColorAt = TFunction<FLinearColor(const FVector2D&)>;
	static TArray<FVector2D> ChamferPoly(float X, float Y, float W, float H, float C);
	void FillPoly(const TArray<FVector2D>& Poly, const FColorAt& ColorAt);
	void FillCircle(FVector2D C, float R, const FLinearColor& Inner, const FLinearColor& Outer, int32 N = 40);
	/** O painel de metal: chanfro C, aro (Rim) e corpo em degradê de cima pra baixo; Tint pinta o corpo (azul, vermelho). */
	void Plate(float X, float Y, float W, float H, float C, const FLinearColor& Rim, const FLinearColor& BodyTop, const FLinearColor& BodyBottom, bool bRivets);
	/** O emblema no anel de metal (o topo 1x1). */
	void EmblemRing(FName Emblem, FVector2D C, float R, const FLinearColor& Rim);
	FVector2D Text(const FString& Str, float X, float Y, float Px, const FLinearColor& C, EWeight W = EWeight::SemiBold, EAlign Al = EAlign::Left, bool bShadow = true);
	/** O texto centrado na altura CY (pra caber no meio das faixas da pele). */
	FVector2D TextMid(const FString& Str, float X, float CY, float Px, const FLinearColor& C, EWeight W = EWeight::SemiBold, EAlign Al = EAlign::Left, bool bShadow = true);
	FVector2D Measure(const FString& Str, float Px, EWeight W) const;
	FSlateFontInfo Font(float Px, EWeight W) const;
	static FLinearColor HealthColor(float H);

	FTextureLookup Lookup;
	TSharedPtr<const FCompositeFont> Fonts[3];
	UCanvas* Canvas = nullptr;
	const FRTSHudState* Cur = nullptr;
	TArray<FRTSHudButton>* Buttons = nullptr;
	TMap<FName, FBox2D> SlotBoxes; // onde cada casa caiu no quadro (pra dica)
	mutable TMap<FString, FVector2D> MeasureCache; // as medidas de texto já feitas (Measure)
	/** As caixas das peças (o fundo de cada painel): vão no fim da lista de botões como "hud_bg". */
	TArray<FBox2D> BgBoxes;
	void Bg(float X, float Y, float W, float H) { BgBoxes.Add(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))); }
};
