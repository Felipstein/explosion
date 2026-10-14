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
 * A interface do gerador (Slate em código, sem asset), em três partes sobre a
 * vista: à esquerda o painel de propriedades (cabeçalho fixo com a semente,
 * "Gerar mundo novo" e "Ver em qualidade real"; embaixo rolando o tipo de mapa,
 * o terreno, os mapas guardados e os atalhos); no alto à direita a barra da
 * vista (o que mostrar, qualidade); embaixo à direita o minimapa (clique leva a
 * câmera) com os números do mapa.
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
	/** Lado do minimapa (px), embaixo à direita. */
	static constexpr float MinimapSize = 230.0f;

	/** O mouse está em cima do painel (aí zoom e cliques não vão pra câmera). */
	bool IsCursorOver() const;

	/** Controles por nome, pro teste automático da interface (ForgeUITest). */
	TSharedPtr<SWidget> FindTarget(const FString& Id) const
	{
		const TSharedPtr<SWidget>* W = TestTargets.Find(Id);
		return W ? *W : TSharedPtr<SWidget>();
	}

	/** A lista de mapas guardados (refeita quando muda). */
	void RefreshSavedMaps();
	/** A aba do painel: 0 Mundo (tipo de mapa e terreno), 1 Editar (só a ferramenta escolhida), 2 Mapas
	 *  (os guardados e os do Zero Hour). Sair de Editar volta a ferramenta pra "Olhar". */
	void SetTab(int32 Tab);
	int32 GetTab() const { return PanelTab; }
	/** Rola o painel até o controle (teste: o clique tem de cair num botão visível). */
	void ScrollToTarget(const FString& Id);

private:
	TSharedRef<SWidget> Choice(const FString& Id, const FText& Label, TArray<TPair<FText, int32>> Options, TFunction<int32()> Get, TFunction<void(int32)> Set, bool bCompact = false);
	TSharedRef<SWidget> SliderRow(const FString& Id, const FText& Label, float FWorldGenSettings::* Field);
	/** As camadas do chão do bioma pro pincel. */
	TSharedRef<SWidget> BrushLayerRow();
	/** A ferramenta de estrada: o tipo, a dica do que fazer e os botões. */
	TSharedRef<SWidget> RoadRows();
	void Regenerate();
	void OnMapShown();
	FReply OnMinimapClick(const FGeometry& Geometry, const FPointerEvent& Event);

	TWeakObjectPtr<AWorldForgeGameMode> GameMode;
	TSharedPtr<SBorder> Body;      // o painel de propriedades (esquerda)
	TSharedPtr<SBorder> Toolbar;   // a barra da vista (alto, à direita)
	TSharedPtr<SBorder> MapBox;    // minimapa e números (embaixo, à direita)
	TSharedPtr<class SScrollBox> Scroll;
	bool bShowHelp = false;
	int32 PanelTab = 0;
	TSharedRef<SWidget> TabButton(int32 Tab, const FText& Label, const FText& Tip);
	TSharedPtr<SEditableTextBox> SeedBox;
	TSharedPtr<STextBlock> Stats;
	TSharedPtr<STextBlock> SaveStatus;
	TSharedPtr<class SVerticalBox> SavedList;
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
