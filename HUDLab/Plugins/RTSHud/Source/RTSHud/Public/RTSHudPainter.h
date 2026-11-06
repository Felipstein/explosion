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
	/** (05/11) Quantas linhas a coluna das superarmas do placar rola no último quadro (0 = cabe tudo): o jogo limita a rodinha por ele. */
	int32 LastSwMaxScroll = 0;
	/** O cubic-bezier(x1, y1, x2, y2) do CSS no tempo T (0..1): as curvas do protótipo, iguais. */
	static float Bezier(float X1, float Y1, float X2, float Y2, float T);

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
	// os jogadores (RTSHudPainterPlayers.cpp, 03/11): a barra dos times, a lista C das superarmas e o placar do Tab
	/** A barra com mais de 2 jogadores: sempre simétrica; 2 times com o painel na sua cor e o do outro em vermelho, 3+ times em vidro neutro. */
	void DrawTopTeams(const FRTSHudState& State);
	/** As superarmas dos outros no canto de cima à direita: uma linha por dono e tipo, até 8, o resto no Tab. */
	void DrawSuperweaponList(const FRTSHudState& State);
	/** O placar (Tab): uma faixa na cor de cada jogador, os times separados, e a coluna de todas as superarmas. */
	void DrawScoreboard(const FRTSHudState& State);
	/** (RTSHudPainterGeneral.cpp, 04/11) A tela do general: a janela da facção, a trilha das patentes, a árvore, o detalhe e as comendas. */
	void DrawGeneral(const FRTSHudState& State);
	/** O texto quebrado em linhas de até MaxW (px da tela); MaxLines > 0 corta com "…" na última. */
	TArray<FString> WrapLines(const FString& Str, float Px, EWeight W, float MaxW, int32 MaxLines) const;
	/** O ✓ do protótipo, centrado em C, Size de largura e T de espessura (px da tela). */
	void CheckMark(FVector2D C, float Size, float T, const FLinearColor& Col);
	/** A ficha de ponto: 0 a dourada, 1 a escura, 2 a verde do comprado (com o ✓); Num no meio. */
	void GenToken(int32 Kind, float X, float Y, float W, float H, const FString& Num, float Px);
	/** O ⛓: dois elos, centrados em C. */
	void ChainGlyph(FVector2D C, float Size, const FLinearColor& Col);
	/** A ficha de um jogador: o encaixe escuro, o aro na cor dele e a silhueta do emblema na cor dele clareada. */
	void Chip(const FRTSHudPlayer& P, FVector2D C, float D, float Alpha = 1.0f);
	/** Um anel (ou arco) entre os raios R0 e R1, de A0 a A1 (em voltas: 0 = em cima, no sentido do relógio), a cor de dentro e a de fora. */
	void Arc(FVector2D C, float R0, float R1, float A0, float A1, const FLinearColor& C0, const FLinearColor& C1);
	/** O anel com as beiras suaves (meio pixel esfumado de cada lado): os anéis finos sem serrilhado. */
	void SoftArc(FVector2D C, float R0, float R1, float A0, float A1, const FLinearColor& Col);
	/** A carga: o aro de fundo (Track) e o arco aceso de 0 até Frac, com as pontas redondas. */
	void ChargeRing(FVector2D C, float R, float T, float Frac, const FLinearColor& Track, const FLinearColor& Lit);
	/** A faixa arredondada com degradê na horizontal (Stops: a posição 0..1 e a cor); só o pedaço entre X0 e X1, se pedido. */
	void RoundRect(float X, float Y, float W, float H, float R, TArrayView<const TPair<float, FLinearColor>> Stops, float X0 = -1e9f, float X1 = 1e9f);
	void RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& C);
	/** O contorno por dentro da faixa arredondada (a borda fina), T de espessura. */
	void RoundRectRing(float X, float Y, float W, float H, float R, float T, const FLinearColor& C);
	void DrawCommander(const FRTSHudState& State);
	/** (RTSHudPainterConsole.cpp) O console aprovado: as abas dos grupos, o brasão do dinheiro, a espinha de energia, o cubo e o tubo da patente. */
	void DrawConsole(const FRTSHudState& State);
	/** A peça da pele da facção (skin:<facção>/<peça>) quando existe; senão a da USA. */
	FName FacSkin(const TCHAR* Piece) const;
	void DrawAlerts(const FRTSHudState& State);
	void DrawPowers(const FRTSHudState& State);
	void DrawMinimap(const FRTSHudState& State);
	void DrawGroups(const FRTSHudState& State);
	void DrawCommandPanel(const FRTSHudState& State);
	/** A altura (na planta) do painel central: 180 com 9 colunas, mais alto com as casas maiores. */
	static float CommandPanelHeight(const FRTSHudState& State);
	/** (v4) O relógio (FRTSHudTimer) no lugar da grade principal: de Left até o sulco Right, na planta, no painel PTop/PH. */
	void DrawTimer(const FRTSHudState& State, float Left, float Right, float PTop, float PH);
	void DrawCard(const FRTSHudState& State);
	void DrawTooltip(const FRTSHudState& State);

	void DrawSlot(const FRTSHudSlot& Slot, float X, float Y, float W, float H);
	/** O mouse em cima e o aperto de um botão qualquer (o hover aprovado, sem subir: HoverFx com o tipo pela cor do aro). */
	void HoverPress(FName Id, float X, float Y, float W, float H, const FLinearColor& Rim);
	// (RTSHudPainterHover.cpp, 04/11) o hover aprovado: o tipo do botão dá a cor (a da facção, a ação, a melhoria, o bloqueado, sem dinheiro)
	enum class EHoverKind : uint8 { Normal, Action, Gold, Locked, Poor, Empty };
	/** Antes de desenhar o botão: sobe e cresce com a mola, a sombra embaixo e o brilho (DrawTint). Muda a caixa; falso se não está no mouse. */
	bool HoverXform(FName Id, EHoverKind Kind, float& X, float& Y, float& W, float& H, float Corner, bool bContained = false);
	/** Depois de desenhar: a luz por dentro, o fio, o halo e a luz que passa no metal (cada facção a sua); o aperto. Zera o DrawTint. */
	void HoverFx(FName Id, EHoverKind Kind, float X, float Y, float W, float H, float Corner, bool bContained = false);
	/** (05/11) A fila do cartão está sendo desenhada: as casas vazias dela ficam (a linha de fabricação mostra os espaços). */
	bool bDrawingQueue = false;
	/** A luz de cada facção (o --lt do protótipo). */
	FLinearColor FactionLight() const;
	/** (RTSHudPainterChat.cpp, 04/11) o chat da partida: as linhas perto do minimapa e a caixa de digitar. */
	void DrawChat(const FRTSHudState& State);
	/** A cor ponto a ponto numa grade Nx × Ny (os degradês que andam). */
	void GridFill(float X, float Y, float W, float H, int32 Nx, int32 Ny, const TFunction<FLinearColor(const FVector2D&)>& ColorAt);
	/** O brilho do botão no mouse: multiplica a cor das imagens e da pele enquanto ele é desenhado. */
	FLinearColor DrawTint = FLinearColor::White;
	/** 0..1, uma pulsação suave de Hz por segundo. */
	float Pulse(float Hz) const;
	/** A cor do neon da pele da facção (o mouse em cima acende nela). */
	FLinearColor Accent() const;
	UTexture* FactionTex(FName Name) const;
	/** Quanto o painel central ainda está abaixo do lugar (a subida do PanelIn), em pixels da tela. */
	float PanelDrop(const FRTSHudState& State) const;
	void Radiation(FVector2D C, float R);

	// primitivas (já em pixels da tela)
	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	void Border(float X, float Y, float W, float H, float T, const FLinearColor& C);
	void DashedBorder(float X, float Y, float W, float H, float T, float Dash, const FLinearColor& C);
	void Line(FVector2D A, FVector2D B, float T, const FLinearColor& C);
	void Image(FName Name, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White, bool bCover = true);
	/** O ícone sem a moldura dele (Inset = a fração cortada de cada lado): vai no encaixe da pele. */
	void ImageInset(FName Name, float X, float Y, float W, float H, float Inset, const FLinearColor& Tint = FLinearColor::White);
	/** Um pedaço da textura (UV0..UV1) esticado na caixa: as peças do topo de muitos jogadores saem da arte do topo 1x1. */
	void ImageUV(FName Name, float X, float Y, float W, float H, FVector2D UV0, FVector2D UV1, const FLinearColor& Tint);
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
	/** O texto com espaço entre as letras (os rótulos em caixa alta), centrado na altura CY; Spacing em px da planta. */
	FVector2D TextSpaced(const FString& Str, float X, float CY, float Px, const FLinearColor& C, EWeight W, float Spacing, EAlign Al = EAlign::Left, bool bShadow = false);
	float MeasureSpaced(const FString& Str, float Px, EWeight W, float Spacing) const;
	/** O texto cortado com "…" até caber em MaxW (px da tela). */
	FString Ellipsize(const FString& Str, float Px, EWeight W, float MaxW) const;
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
