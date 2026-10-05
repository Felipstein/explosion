#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "CoreMinimal.h"
#include "Generation/WorldMap.h"
#include "Widgets/SCompoundWidget.h"

class AWorldForgeGameMode;
class SBorder;
class STextBlock;
class SEditableTextBox;

/**
 * Painel do gerador (Slate em código, sem asset): semente, tamanho,
 * jogadores, bioma, os controles de 0 a 100%, o minimapa (clique leva a
 * câmera), as camadas de depuração e "Salvar mapa".
 * Cada mudança gera o mapa de novo (os controles deslizantes, ao soltar).
 */
class SForgePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SForgePanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AWorldForgeGameMode>, GameMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SForgePanel() override;

	/** Lado da bolinha dos controles deslizantes (px). */
	static constexpr float ThumbSize = 20.0f;

	/** O mouse está em cima do painel (aí zoom e cliques não vão pra câmera). */
	bool IsCursorOver() const;

	/** Controles por nome, pro teste automático da interface (ForgeUITest). */
	TSharedPtr<SWidget> FindTarget(const FString& Id) const
	{
		const TSharedPtr<SWidget>* W = TestTargets.Find(Id);
		return W ? *W : TSharedPtr<SWidget>();
	}

private:
	TSharedRef<SWidget> Choice(const FString& Id, const FText& Label, TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set);
	TSharedRef<SWidget> SliderRow(const FString& Id, const FText& Label, float FWorldGenSettings::* Field);
	void Regenerate();
	void OnMapShown();
	FReply OnMinimapClick(const FGeometry& Geometry, const FPointerEvent& Event);

	TWeakObjectPtr<AWorldForgeGameMode> GameMode;
	TSharedPtr<SBorder> Body;
	TSharedPtr<SEditableTextBox> SeedBox;
	TSharedPtr<STextBlock> Stats;
	TSharedPtr<STextBlock> SaveStatus;
	FSlateBrush MinimapBrush;
	FSliderStyle SliderStyle;
	FSlateRoundedBoxBrush Background = FSlateRoundedBoxBrush(FLinearColor(0.012f, 0.016f, 0.02f, 0.86f), 10.0f);
	FSlateRoundedBoxBrush MinimapFrame = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), 4.0f);
	FDelegateHandle MapShownHandle;
	int32 OverlayMode = 0;
	/** Cópia local dos controles (o que o painel mostra; vai pro gerador ao mudar). */
	FWorldGenSettings Edit;
	TMap<FString, TSharedPtr<SWidget>> TestTargets;
};
