#pragma once

// As telas da partida nativas (06/11; ele: "a tela de loading após o START MATCH continua com o loading de jogo de criança [...] o Esc
// dentro do jogo também, o feio ainda [...] nem defeat, nem victory estão com a nova"): o mockup aprovado (docs/concept/ui/menu/
// screens.html, as telas load, esc, res e net) refeito em Slate, um widget com quatro modos.
// - Loading: a operação, a sua facção grande, uma linha por jogador com as células do carregando (a IA pronta), o mapa com as bases e
//   o total embaixo. Também roda no MoviePlayer (a thread do filme: só os dados da largada, FRTSShell::GetIntro, nada do jogo).
// - Pause: o painel do Esc (RESUME, OPTIONS, RESTART, QUIT TO MENU; na rede, PAUSE FOR EVERYONE e SURRENDER com a confirmação).
// - Result: VICTORY/DEFEAT, a tabela de cada jogador (as contas do placar do Tab), o gráfico (o exército, o dinheiro, as baixas, pelas
//   amostras que o RTSShellGame tira durante a partida) e os botões.
// - Net: os avisos da rede num desenho só (esperando alguém, a pausa de alguém, o anfitrião mudo, a conexão perdida, a dessincronia).
// Os dados vêm do jogo pelo Handlers().Api("match.json"); os botões, pelo Handlers().MatchButton (o OnButton do controle).

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RTSShellUI.h"
#include "RTSShell.h"

class SRTSMatch : public SLeafWidget
{
public:
	enum class EMode : uint8 { Loading, Pause, Result, Net };

	SLATE_BEGIN_ARGS(SRTSMatch) : _Movie(false) {}
		/** A cópia do MoviePlayer: não lê nada do jogo (o jogo está carregando o mapa). */
		SLATE_ARGUMENT(bool, Movie)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetMode(EMode InMode);
	EMode GetMode() const { return Mode; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(RTSShellUI::StageW, RTSShellUI::StageH); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	/** O começo do carregando (a mesma conta no filme e no jogo: as células continuam de onde estavam). */
	static double& LoadStart();
	/** Na thread do jogo, antes do filme: as imagens, as fontes e as bases do mapa da largada ficam carregadas. */
	static void Warm(const FRTSMatchIntro& In);
	/** O borrão de quem hospeda (o backdrop-filter do .dim): nada no carregando (opaco) e olhando o mapa. */
	float BlurStrength() const { return Mode == EMode::Loading || (Mode == EMode::Result && bViewMap) ? 0.0f : 4.0f; }
	/** O centro do botão (o Id do OnButton) no palco de 1920 × 1080. */
	bool ButtonCenter(const FString& Id, FVector2f& Out) const
	{
		for (const FBtn& B : Btns) if (B.Id == Id) { Out = FVector2f(B.R.X + B.R.Z * 0.5f, B.R.Y + B.R.W * 0.5f); return true; }
		return false;
	}

private:
	struct FPlayer
	{
		FString Name, Fac;
		int32 Team = 0, Base = -1, Diff = 1;
		uint32 Hex = 0xffffff;
		bool bAi = false, bYou = false, bDefeated = false, bLoaded = false;
		int32 Ub = 0, Bb = 0, Ud = 0, Bd = 0, Ul = 0, Bl = 0;
		int64 Money = 0, Score = 0;
	};
	/** Um botão do .btn do mockup: o retângulo, o texto, a tecla, o que faz e o jeito (0 comum, 1 âmbar, 2 vermelho). */
	struct FBtn
	{
		FVector4f R;
		FString Label, Kbd, Id;
		int32 Style = 0;
	};

	void Poll();
	void Build();
	void Press(const FString& Id);
	FString Matchup() const;
	FString TimeText(float S) const;

	void PaintLoading(FSlateWindowElementList& Out, int32& L, const FGeometry& Allotted, const FGeometry& S) const;
	void PaintPause(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintResult(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintNet(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintPanel(FSlateWindowElementList& Out, int32& L, const FGeometry& S, const FVector4f& R, bool bBad, bool bCorners) const;
	void PaintBtns(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintTok(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, float Size, const FString& Fac, uint32 Hex) const;
	void PaintCells(FSlateWindowElementList& Out, int32 L, const FGeometry& S, float X, float Y, int32 N, int32 K, float W, float H, float Gap, FColor On) const;

	EMode Mode = EMode::Loading;
	bool bMovie = false;
	FRTSMatchIntro Intro;
	double Now = 0.0, PollAt = 0.0, ModeT0 = 0.0;
	// o que o jogo diz (match.json)
	FString Op, MapName, MapReal, Kind = TEXT("skirmish"), Result = TEXT("playing");
	float MatchTime = 0.0f;
	bool bReady = false, bCanSurrender = false, bCanRestart = false, bCanNetPause = false, bLan = false;
	int32 LocalSlot = 0;
	TArray<FPlayer> Players;
	TArray<float> HistT;
	TArray<TArray<float>> Hist[3];
	// a rede: o caso (0 nada, 1 esperando alguém, 2 voltando, 3 pausado por alguém, 4 o anfitrião mudo, 5 perdida, 6 dessincronia;
	// 7, de qualquer partida: ela não montou, com o motivo em NetError)
	int32 NetCase = 0, NetSlot = -1, Votes = 0, VotesNeeded = 0, PingMs = 0;
	float NetLeft = 0.0f, NetSilent = 0.0f, ResumeIn = 0.0f;
	bool bVoted = false, bNetHost = false;
	int64 DesyncTick = -1;
	FString NetError;
	// a tela
	bool bConfirm = false, bViewMap = false;
	int32 GraphTab = 0;
	TArray<FBtn> Btns;
	int32 HoverBtn = -1, HoverTab = -1;
	TArray<float> BtnHover;
};
