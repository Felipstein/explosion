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

	void DrawGroundRing(const FVector& Center, float RadiusCm, const FLinearColor& Color, float Thickness);
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color);
	/** Texto com sombra; Size em pontos do Slate. Devolve o tamanho desenhado (px). */
	FVector2D DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCenter = false, bool bBold = false);
	FVector2D MeasureLabel(const FString& Text, int32 Size, bool bBold) const;
	void DrawButton(FName Id, const FString& Label, float X, float Y, float W, float H);
	FVector2D MousePos() const;

	TArray<FButton> Buttons;
};
