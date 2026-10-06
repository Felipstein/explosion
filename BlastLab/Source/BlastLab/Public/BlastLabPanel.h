#pragma once

#include "Widgets/SCompoundWidget.h"

class UTexture2D;

/** Os quatro níveis do painel (poucas opções pro jogador: os detalhes são ferramenta de depuração). */
enum class EBlastQuality : uint8 { Low, Medium, High, Ultra };

/**
 * Painel lateral do simulador, o "ARSENAL" do WebGL (reference/webgl/index.html):
 * armamento, passagem pela fumaça, ambiente (hora do dia) e sistema
 * (qualidade + leituras). Slate puro, desenhado em código: metal escuro e
 * âmbar, na linha dos HUDs de C&C.
 */
class BLASTLAB_API SBlastLabPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlastLabPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_ARGUMENT(UTexture2D*, Thumbnail)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Largura do painel em unidades de Slate (a área de clique no mundo é o resto). */
	static constexpr float Width = 252.0f;

	/** Pisca o cartão da arma detonada. */
	void Fired(int32 Weapon);

	static void ApplyQuality(EBlastQuality Q);
	static EBlastQuality CurrentQuality;

private:
	TSharedRef<SWidget> Section(const FText& Title, TSharedRef<SWidget> Content, TAttribute<FText> Status = FText());
	TSharedRef<SWidget> WeaponCard(int32 Index);
	TSharedRef<SWidget> Button(const FText& Label, TFunction<bool()> IsOn, TFunction<void()> OnClick, const FText& Tooltip = FText(), bool bEnabled = true);

	TWeakObjectPtr<UWorld> World;
	TSharedPtr<struct FSlateBrush> ThumbBrush;
	float FpsSmoothed = 0.0f;
	float FlashTime[8] = {};
	int32 FlashWeapon = -1;
	double FlashStart = -10.0;
};
