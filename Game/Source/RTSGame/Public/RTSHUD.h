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
#include "RTSHUD.generated.h"

class ARTSPlayerController;
class URTSMatchSubsystem;

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

	// --- minimapa (o losango no canto de baixo à esquerda), na geometria do último quadro
	/** O ponto da tela está sobre a moldura do minimapa (o clique é dele, não do mundo). */
	bool IsOverMinimap(const FVector2D& Screen) const;
	/** O ponto da tela cai dentro do mapa do minimapa: o lugar no mundo (cm). bClamp prende na borda. */
	bool MinimapToWorld(const FVector2D& Screen, FVector2D& OutCm, bool bClamp = false) const;
	/** Onde um lugar do mundo aparece no minimapa (pro teste clicar). */
	bool WorldToMinimapScreen(const FVector2D& Cm, FVector2D& OutScreen) const;

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
	void DrawEnemyArrow(URTSMatchSubsystem* Match);
	void DrawPause();
	void DrawResult(URTSMatchSubsystem* Match);
	void DrawMinimap(ARTSPlayerController* PC, URTSMatchSubsystem* Match);
	FVector2D MinimapUVToScreen(const FVector2D& UV) const;
	FVector2D MinimapScreenToUV(const FVector2D& Screen) const;
	void DrawClippedLine(FVector2D A, FVector2D B, const FLinearColor& Color, float Thickness);

	void DrawGroundRing(const FVector& Center, float RadiusCm, const FLinearColor& Color, float Thickness);
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color);
	/** Texto com sombra; Size em pontos do Slate. Devolve o tamanho desenhado (px). */
	FVector2D DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCenter = false, bool bBold = false);
	FVector2D MeasureLabel(const FString& Text, int32 Size, bool bBold) const;
	void DrawButton(FName Id, const FString& Label, float X, float Y, float W, float H);
	FVector2D MousePos() const;

	TArray<FButton> Buttons;

	bool bMinimapDrawn = false;
	FBox2D MinimapFrame = FBox2D(ForceInit);
	FVector2D MinimapCenter = FVector2D::ZeroVector;
	float MinimapSide = 0.0f;       // lado do quadrado do mapa (o losango é ele girado)
	float MinimapCos = 1.0f;
	float MinimapSin = 0.0f;
	FVector2D MinimapMinCm = FVector2D::ZeroVector;
	FVector2D MinimapSizeCm = FVector2D(1.0, 1.0);
};
