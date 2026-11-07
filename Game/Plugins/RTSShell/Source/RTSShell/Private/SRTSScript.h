#pragma once

// O SCRIPT nativo (06/11; só no desenvolvimento): o mockup aprovado (script.html) refeito em Slate. As partidas de teste prontas do
// Scenarios.json (as que as sessões abrem com -RTSScenario), a mais nova primeiro, a busca por nome, mapa ou texto (o trecho achado em
// ciano), a ordem (NEWEST, NAME, MAP) e o LAB ONLY; a cena que veio pela linha de comando armada no topo; à direita a escolhida com o
// que ela testa e o RUN. Medidas do Chrome: Tools/measure_script.js → script_layout.json.

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RTSShellUI.h"

class SRTSScript : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSScript) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** A tela voltou: relê as cenas do jogo e a armada, escolhe a armada (ou a mais nova). */
	void Restart();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(RTSShellUI::StageW, RTSShellUI::StageH); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	/** Uma cena (o ALL da página). */
	struct FScene
	{
		FString Name, About, Map, MapTitle, Fog;
		int32 Units = 0, Structures = 0, Orders = 0, Index = 0;
		float EndS = 0.0f;
		bool bLab = false, bHud = true;
	};
	enum class EHit : uint8 { None, Back, Search, Chip, Armed, Row, Run };
	struct FHit
	{
		EHit Kind = EHit::None;
		int32 A = -1;
		bool operator==(const FHit& O) const { return Kind == O.Kind && A == O.A; }
	};

	/** As cenas que passam na busca e no LAB ONLY, na ordem escolhida (índices de All). */
	TArray<int32> List() const;
	void Pick(int32 Scene, bool bScroll);
	void Run();
	void Back();
	float RowsTop() const { return bArmed() ? 402.0f : 335.0f; }
	bool bArmed() const { return !Armed.IsEmpty(); }
	float MaxScroll() const;
	FHit HitAt(FVector2f P) const;
	float HoverOf(EHit Kind, int32 A = -1) const;

	void PaintHead(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintList(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintCard(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	TArray<FScene> All;
	FString Armed;
	int32 Sel = -1;
	FString Query;
	bool bSearchFocus = false;
	int32 SortK = 0;   // 0 NEWEST, 1 NAME, 2 MAP
	bool bLabOnly = false;
	float Scroll = 0.0f, ScrollTo = 0.0f;
	FHit Hover;
	TMap<uint32, float> HoverEase;
	double Now = 0.0, CaretT0 = 0.0;
};
