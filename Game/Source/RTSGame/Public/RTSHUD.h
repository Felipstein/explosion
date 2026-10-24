#pragma once

// HUD do primeiro passo, desenhado direto na tela (Vanguard ADR-0021), com as
// letras do Slate (nítidas em qualquer tamanho):
//  - anel no chão na cor do time em cada unidade (forte quando selecionada ou sob o mouse);
//  - barra de vida só quando a unidade está ferida, selecionada ou sob o mouse;
//  - caixa de seleção, marca de ordem no chão, seta pro inimigo fora da tela;
//  - menu principal, objetivo no começo, pausa e o resultado 3 s depois do golpe final.
// A reforma visual (vidro, ícones, minimapa) é um passo seguinte.

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RTSHudTypes.h"
#include "RTSHUD.generated.h"

class ARTSPlayerController; struct FCompositeFont;   // (v4: na mesma linha, o GENERATED_BODY abaixo não muda de linha)
class URTSMatchSubsystem;

enum class ERTSFogMode : uint8;

UCLASS()
class RTSGAME_API ARTSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Botão sob o ponto (pixels da tela) no último quadro desenhado; None se nenhum. */
	FName ButtonAt(const FVector2D& Screen) const;
	/** Onde está o botão no último quadro (pro teste automático clicar nele). */
	bool FindButton(FName Id, FBox2D& OutBox) const;
	/** Os botões do último quadro, pro log do teste quando um clique não acha o seu. */
	FString ButtonIdsForTest() const;

	// --- minimapa (o losango no canto de baixo à esquerda), na geometria do último quadro
	/** O ponto da tela está sobre a moldura do minimapa (o clique é dele, não do mundo). */
	bool IsOverMinimap(const FVector2D& Screen) const;
	/** O ponto da tela cai dentro do mapa do minimapa: o lugar no mundo (cm). bClamp prende na borda. */
	bool MinimapToWorld(const FVector2D& Screen, FVector2D& OutCm, bool bClamp = false) const;
	/** Onde um lugar do mundo aparece no minimapa (pro teste clicar). */
	bool WorldToMinimapScreen(const FVector2D& Cm, FVector2D& OutScreen) const;
	/** O HUD novo do HUDLab (13/10, o teste do Felipe jogando): -NewHud liga; F4 troca na partida. */
	bool IsNewHud() const { return bNewHud; }
	void ToggleNewHud() { bNewHud = !bNewHud; bNewHudChecked = true; }
	/** As teclas das casas do HUD novo (a letra → o id do botão), do último quadro. */
	const TMap<TCHAR, FName>& GetHotkeys() const { return NewHotkeys; }
	/** (v4) O dinheiro como na tela: "$20,000" (fora do namespace anônimo: o unity build junta os .cpp do HUD). */
	static FString MoneyText(int32 Money);

private:
	struct FButton
	{
		FName Id;
		FBox2D Box;
	};

	void DrawMenu(ARTSPlayerController* PC);
	void DrawMatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	void DrawUnits(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	void DrawMarkers(ARTSPlayerController* PC);
	void DrawTopBar(URTSMatchSubsystem* Match);
	void DrawObjective(URTSMatchSubsystem* Match);
	void DrawPause();
	/** A linha QUALITY (baixa … ultra) do Esc e do menu. */
	void DrawQualityRow(const class ARTSPlayerController* PC, float X, float Y, float W);
	/** (v4) A linha STARTING MONEY: [ - ] $20,000 [ + ] e um tracinho por escolha, o de agora aceso (ids <Prefix>_down e
	 *  <Prefix>_up). bCanChange falso: só o valor. bCompact: sem o título em cima (a sala), 34 px de alto. */
	void DrawMoneyRow(FName Prefix, int32 Money, float X, float Y, float W, bool bCanChange, bool bCompact = false);
	void DrawResult(URTSMatchSubsystem* Match);
	/** (16/10) O mapa da batalha na tela do fim: a foto do minimapa e as marcas, num losango de meia diagonal Half. */
	void DrawBattleMap(URTSMatchSubsystem* Match, const FVector2D& Center, float Half, float Alpha);
	void DrawMinimap(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	FVector2D MinimapUVToScreen(const FVector2D& UV) const;
	FVector2D MinimapScreenToUV(const FVector2D& Screen) const;
	void DrawClippedLine(FVector2D A, FVector2D B, const FLinearColor& Color, float Thickness);

	void DrawGroundRing(const FVector& Center, float RadiusCm, const FLinearColor& Color, float Thickness, int32 Segments = 28);
	/** (v4, 23/10) A prévia da linha dos dois botões (esquerdo + direito arrastando), nos dois HUDs: a linha puxada, fina, no
	 *  chão, e cada unidade como um anel com um risquinho pra frente no lugar exato onde vai parar (a conta da simulação). */
	void DrawLinePreview(ARTSPlayerController* PC, URTSMatchSubsystem* Match);

	// economia e construção, o HUD provisório (RTSHUDEconomy.cpp; o de verdade sai da arte: docs/HUD.md)
	/** O topo com base: dinheiro, energia, relógio e quem segue em campo. */
	void DrawEconomyTopBar(URTSMatchSubsystem* Match);
	/** O painel do canto de baixo à direita: botões do prédio ou do Dozer, a fila e a obra. */
	void DrawCommandPanel(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	/** Quem caiu assiste: a lista dos jogadores no lugar da barra de comando. */
	void DrawObserverPanel(URTSMatchSubsystem* Match);
	/** "Fulano foi derrotado" pra todos, e a faixa do espectador. */
	void DrawDefeats(URTSMatchSubsystem* Match);
	/** O fantasma do prédio sob o mouse (verde ou vermelho, com o motivo) e a faixa da porta. */
	void DrawPlacementGhost(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	/** A bandeira do ponto de encontro dos prédios selecionados. */
	void DrawRallyPoints(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	/** O "+$300" que sobe do centro de suprimentos. */
	void DrawMoneyPopups(URTSMatchSubsystem* Match, float DeltaSeconds);
	/** O valor que sobra no depósito sob o mouse (o ZH mostra a dica com o dinheiro). */
	void DrawDockHover(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	/** Retângulo no chão, seguindo o relevo. */
	void DrawGroundRect(const FVector2D& CenterCm, const FVector2D& HalfCm, const FLinearColor& Color, float Thickness);
	/** Botão pequeno do painel; apagado não reage ao mouse mas segue clicável (a simulação recusa). */
	void DrawPanelButton(FName Id, const FString& Label, const FString& Sub, float X, float Y, float W, float H, bool bEnabled);
	/** A partida tem base (economia): o topo e o painel mudam. */
	bool HasEconomy(URTSMatchSubsystem* Match) const;
	/** O jogador tem algum prédio: sem base não há o que produzir, e a energia 0/0 não é aviso (13/10). */
	bool OwnsStructure(URTSMatchSubsystem* Match, uint8 Player) const;
	void DrawLanMenu(ARTSPlayerController* PC, const class URTSNetSession& Net, float X, float Y, float BW);
	static FString FogModeText(ERTSFogMode Mode);
	static FString FogModeHint(ERTSFogMode Mode);
	void DrawNetStatus(URTSMatchSubsystem* Match);

	struct FPopup
	{
		FVector2D Cm = FVector2D::ZeroVector;
		int32 Amount = 0;
		float Age = 0.0f;
	};
	TArray<FPopup> Popups;
	double LastPopupTime = -1.0;
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color);
	/** Texto com sombra; Size em pontos do Slate. Devolve o tamanho desenhado (px). */
	FVector2D DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCenter = false, bool bBold = false);
	FVector2D MeasureLabel(const FString& Text, int32 Size, bool bBold) const;
	/** (17/10) O chat da partida: as linhas no canto de cima à esquerda (somem em 10 s) e a caixa de digitar. */
	void DrawChat(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	void DrawButton(FName Id, const FString& Label, float X, float Y, float W, float H);
	/** O botão pequeno da sala: o texto centrado e menor; com Swatch, o quadrado de cor dentro (o botão da cor). */
	void DrawSmallButton(FName Id, const FString& Label, float X, float Y, float W, float H, const FLinearColor* Swatch = nullptr);
	FVector2D MousePos() const;

	TArray<FButton> Buttons;
	// o HUD novo (RTSHUDNew.cpp; o plugin RTSHud)
	void DrawNewHud(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	void FillNewHud(ARTSPlayerController* PC, URTSMatchSubsystem* Match, FRTSHudState& S);
	TSharedPtr<class FRTSHudKit> NewKit;
	TSharedPtr<class FRTSHudPainter> NewPainter;
	TArray<FRTSHudButton> NewButtons;
	TMap<TCHAR, FName> NewHotkeys;
	/** A subida do painel (PanelIn) e a idade do aviso de energia, no HUD novo. */
	double NewSelectionShownAt = -1.0;
	double NewLowPowerSince = -1.0;

	// (v4, 23/10) a patente, os poderes do general e a zona de lançamento (RTSHUDNew.cpp). O plugin desenha as estrelas, a
	// barra de XP, a coluna de poderes, os avisos e a casa do avião no cartão; o que ele ainda não tem (a dica do poder, o
	// relógio do avião ao lado da casa) sai daqui, na mesma letra (a Barlow do HUDLab) e na mesma planta (Layout)
	/** A mira do poder: o círculo do raio no chão (azul vale, vermelho não, com o motivo no mouse), nos dois HUDs. */
	void DrawPowerTarget(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	/** A recusa do último pedido de poder, no mouse (some em 2 s). */
	void DrawPowerMessage(ARTSPlayerController* PC);
	/** "PROMOTED - RANK 2: Cruise Missile Strike" (o que a patente liberou, pelos dados). */
	static FString PromotionText(URTSMatchSubsystem* Match, int32 Rank);
	/** Depois do plugin: a dica do poder sob o mouse e o relógio do avião no cartão da zona. */
	void DrawNewHudExtras(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	FVector2D NewText(const FString& Text, float X, float Y, float Px, const FLinearColor& Color, bool bBold, bool bRight = false);
	FVector2D NewMeasure(const FString& Text, float Px, bool bBold) const;
	TSharedPtr<const FCompositeFont> NewFonts[2];
	/** A escala da planta neste quadro (FRTSHudPainter::Layout) e o lado esquerdo e a largura da faixa 16:9. */
	float NewS = 1.0f;
	float NewLeft = 0.0f;
	float NewWidth = 1920.0f;
	/** A casa da coluna de poderes sob o mouse (−1 = nenhuma) e a zona de lançamento do cartão (0 = nenhuma). */
	int32 NewHoverPowerSlot = -1;
	uint32 NewAirliftZone = 0;
	bool bNewHud = false;
	bool bNewHudChecked = false;
	/** O quadro (ms) suavizado, pro contador de FPS no canto (14/10, o Felipe: "durante o desenvolvimento"). */
	float FrameMsSmooth = 16.7f;
	/** O minimapa na geometria do losango da peça nova (sem a moldura velha). */
	bool bNewMinimapGeometry = false;
	bool bMinimapDrawn = false;
	FBox2D MinimapFrame = FBox2D(ForceInit);
	FVector2D MinimapCenter = FVector2D::ZeroVector;
	float MinimapSide = 0.0f;       // lado do quadrado do mapa (o losango é ele girado)
	float MinimapCos = 1.0f;
	float MinimapSin = 0.0f;
	FVector2D MinimapMinCm = FVector2D::ZeroVector;
	FVector2D MinimapSizeCm = FVector2D(1.0, 1.0);
};
