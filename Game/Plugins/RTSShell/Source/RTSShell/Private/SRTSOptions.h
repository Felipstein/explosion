#pragma once

// As opções nativas (06/11): o mockup aprovado (screens.html, a tela "opt") refeito em Slate. À esquerda as abas (GRAPHICS, SOUND,
// CONTROLS, GAME, ACCESSIBILITY); no meio, rolando, os presets (CLASSIC a ULTRA e o AUTO, com os quadros estimados pra GTX 1660) e os
// itens de cada aba em grupos; à direita o cartão do item com o mouse em cima (o que faz e quanto custa) e, nos gráficos, o medidor
// dos quadros e da memória de vídeo; embaixo RESET e APPLY. O APPLY manda o preset de qualidade pro jogo, grava as opções do HUD
// (GameUserSettings [RTS]) e os volumes (o SM_UserVolumes, também guardados no [RTS]). O layout segue o fluxo do CSS; as medidas
// do Chrome conferem (Tools/measure_options.js → options_layout.json).

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RTSShellUI.h"

class SRTSOptions : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSOptions) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** A tela voltou: os itens no padrão, a qualidade e o HUD como o jogo tem, a aba dos gráficos no topo. */
	void Restart();
	/** (06/11) Aberta do Esc da partida: o BACK volta pro Esc (só tira as opções), não pro menu. */
	void SetFromMatch(bool b) { bFromMatch = b; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(RTSShellUI::StageW, RTSShellUI::StageH); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** Um item das opções (os GFX/SND/CTL/GAMEO/ACC da página). */
	struct FItem
	{
		const TCHAR* Group;
		const TCHAR* Key;
		const TCHAR* Name;
		TArray<const TCHAR*> Opts;   // vazio = deslizante
		int32 Min = 0, Max = 100;    // o deslizante
		int32 Def = 0;               // o valor de fábrica (ou o do HIGH, nos que seguem o preset)
		TArray<int32> Preset;        // o valor em CLASSIC, LOW, MEDIUM, HIGH, ULTRA (vazio = não segue)
		bool bFixed = false;         // não muda com o preset (a tela, o modo...)
		TArray<int32> Na;            // as opções indisponíveis (riscadas)
		float W = 0.0f, M = 0.0f;    // o custo de placa (ms no máximo) e de memória de vídeo (GB no máximo)
		const TCHAR* Desc = TEXT("");
		const TCHAR* Ini = nullptr;  // a chave do GameUserSettings [RTS] que o jogo lê (o HUD); true = a 1ª opção
	};

private:
	enum class EHit : uint8 { None, Back, Tab, Preset, Seg, Slider, Row, Search, Reset, Apply };
	struct FHit
	{
		EHit Kind = EHit::None;
		int32 A = -1, B = -1;   // a aba / o preset / o item e a opção
		bool operator==(const FHit& O) const { return Kind == O.Kind && A == O.A && B == O.B; }
	};
	/** Uma peça do conteúdo que rola, no palco (já sem a rolagem): o que é, o item, a caixa. */
	struct FBlock
	{
		enum EType : uint8 { Presets, Custom, Group, Row, KeySearch, KeyRow } Type = Row;
		int32 Item = -1;       // o item (Row) ou a tecla (KeyRow)
		FString Text;          // o nome do grupo
		float Y = 0.0f, H = 0.0f;
	};

	const FItem* Find(const TCHAR* Key) const;
	int32 Val(const TCHAR* Key) const;
	void ApplyPreset(int32 K);
	TMap<FString, int32> PresetVals(int32 K) const;
	void Estimate(const TMap<FString, int32>& V, int32& OutFps, float& OutGb) const;
	/** O fluxo da aba (o CSS: os grupos, as linhas de 48 + 2, os presets e a faixa do CUSTOM), em y relativo ao topo do conteúdo. */
	TArray<FBlock> Layout() const;
	float ContentH() const;
	/** As caixas das opções de uma linha (os spans do .seg: o texto + 22 de respiro + a borda, 4 entre eles). */
	TArray<FVector4f> SegBoxes(const FItem& It, float RowY) const;
	FHit HitAt(FVector2f P) const;
	void SetSlider(int32 Item, float X);
	float HoverOf(EHit Kind, int32 A = -1, int32 B = -1) const;
	void Back();
	void Apply();
	/** Há mudança que o APPLY ainda não gravou (o APPLY só acende assim). */
	bool IsDirty() const;
	/** O estado aplicado (o que está gravado) passa a ser o da tela. */
	void Snapshot();
	void SetVolumes(bool bSaved) const;

	void PaintLeft(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintMain(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintPresets(FSlateWindowElementList& Out, int32& L, const FGeometry& S, float Y) const;
	void PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, const FItem& It, int32 Index, float Y) const;
	void PaintKeyRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 Key, float Y) const;
	void PaintInfo(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintFoot(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	TArray<FItem> Tabs[5];
	TMap<FString, int32> Opt;
	bool bFromMatch = false;
	int32 Tab = 0, Pre = 5;
	bool bCustom = false;
	// o que está aplicado (gravado): pra saber se há mudança pendente
	TMap<FString, int32> SavedOpt;
	int32 SavedPre = 5;
	bool bSavedCustom = false;
	float CustomH = 0.0f;
	int32 CardItem = -1;       // o item do cartão (−1 = a qualidade)
	FString KeyQuery;
	bool bKeyFocus = false;
	int32 DragItem = -1;
	float Scroll = 0.0f, ScrollTo = 0.0f;
	FHit Hover;
	TMap<uint32, float> HoverEase;
	double Now = 0.0, CaretT0 = 0.0;
};
