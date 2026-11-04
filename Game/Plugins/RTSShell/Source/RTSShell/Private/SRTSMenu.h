#pragma once

// O menu principal nativo (03/11): o mockup aprovado (docs/concept/ui/menu/menu.html) refeito em Slate, com as mesmas contas. À
// direita, o painel do radar de vigilância (o tubo PPI de fósforo com persistência, o eco do chão pré-calculado pela mesma conta,
// as trilhas dos quatro exércitos, os instrumentos); à esquerda, o título, a lista e o bloco do LAB e do SCRIPT. As posições do
// lado esquerdo são as medidas no Chrome (Tools/measure_menu.js → menu_layout.json).

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "RTSShellUI.h"

class SRTSMenu : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSMenu) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** O menu voltou à tela: o radar recomeça aquecido e a lista sem hover. */
	void Restart();
	/** A abertura (04/11): o console de comando ligando antes do menu. As linhas da checagem (a dos shaders é o progresso de
	 *  verdade), o tubo acendendo (o desligar do QUIT ao contrário), o golpe com a música entrando e o menu montando. bFirstEver: a
	 *  1ª vez neste PC (inteira, o golpe em 3,9 s); depois, a curta (o golpe em 2,5 s). Tecla ou clique pula pro golpe. */
	void StartBoot(bool bFirstEver);
	bool IsBooting() const { return BootT >= 0.0f; }
	float BootRevealAt() const { return BootReveal; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(RTSShellUI::StageW, RTSShellUI::StageH); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	enum EHit : int32 { HitNone = -1, HitSkirmish = 0, HitMulti, HitOptions, HitQuit, HitLab = 10, HitScript = 11 };

	FGeometry StageOf(const FGeometry& G) const;
	FVector2f StagePos(const FGeometry& G, const FPointerEvent& E) const;
	int32 HitAt(FVector2f P) const;
	void SetHover(int32 Hit);
	/** A altura de cada item agora (o hover abre a descrição: 0 → 30 px) e o topo de cada um. */
	void ItemLayout(float OutTop[4], float OutExtra[4]) const;

	// o radar
	float Since(float Brg) const;
	void PaintRadar(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S) const;
	void PaintStatic(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S) const;
	void PaintTracks(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S, float D) const;
	void PaintInstruments(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S, float D) const;
	void PaintLink(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S) const;
	void PaintSide(FSlateWindowElementList& Out, int32& Layer, const FGeometry& S) const;
	void UpdateWaterfall(float Dt);
	// a abertura
	void StepBoot(float Dt);
	void SkipBoot();
	void PaintSideReveal(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintBootCover(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintBootLines(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	double Now = 0.0;
	float T = 30.0f, Beam = 0.0f, Dim = 1.0f, DimT = 1.0f, Gain = 1.0f, GainT = 1.0f, Knob = 0.0f, KnobT = 0.0f, Off = 0.0f, OffT = 0.0f;
	int32 Impacts = 0;
	int32 StillFrames = -1;   // o teste com o instante fixo: os quadros dados (dois de 1/30 s e depois parado)
	int32 Hover = HitNone;
	TArray<FVector2f> Hist[4];
	TArray<FVector4f> Rain;        // x, y, s (em volta do centro da chuva)
	TArray<FVector3f> Crater;      // x, y, alfa (o eco do impacto que fica no chão)
	TArray<float> Ret;             // o eco polar (1440 × 300), pro A-scope
	TArray<FColor> WfPixels;       // a cascata do ESM (210 × 98)
	TArray<uint8> Spec;            // o que o radar ouve da música: o espectro (o analisador do mockup) e o pico
	bool bSpec = false;
	float Level = 0.0f;
	TStrongObjectPtr<UTexture2D> WfTexture;
	TSharedPtr<FSlateBrush> WfBrush;
	RTSShellUI::FTween ItemColor[4], ItemSlide[4], ItemOpen[4], ItemBar[4], ItemNum[4];
	RTSShellUI::FTween TileHover[2];
	bool bDev = false;
	FString Armed, Scenes;
	double OffUntil = 0.0;
	struct FPending { double At = 0.0; TFunction<void()> Run; };
	TArray<FPending> Pending;
	mutable float LinkPhase = 0.0f;
	// a abertura: o tempo dela (< 0 = sem abertura), os instantes (o golpe, o relé, a 1ª linha e o passo entre linhas), a espera dos
	// shaders e os sons já tocados
	float BootT = -1.0f, BootReveal = 3.9f, BootRelayAt = 0.8f, BootLine0 = 1.0f, BootLineDt = 0.27f, BootHeld = 0.0f;
	bool bBootRelay = false, bBootRiser = false, bBootRevealed = false;
	uint32 BootPso = 0, BootPsoMax = 0;
	// o custo medido (o log a cada 2 s): o desenho do menu no processador e o quadro inteiro (jogo, renderização, placa)
	mutable double PaintSeconds = 0.0;
	double CostFrom = 0.0, CostGame = 0.0, CostRender = 0.0, CostGpu = 0.0, CostFrame = 0.0;
	int32 CostFrames = 0;
};
