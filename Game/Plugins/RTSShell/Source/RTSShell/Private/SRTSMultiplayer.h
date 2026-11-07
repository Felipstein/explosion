#pragma once

// A lista de salas do multiplayer nativa (06/11): o mockup aprovado (multiplayer.html) refeito em Slate, como o menu e o skirmish.
// À esquerda de onde jogar (ONLINE e DIRECT IP apagados até existirem; a LAN de verdade) e o HOST GAME; no meio a busca, os filtros
// (HIDE FULL, HIDE IN GAME, MY VERSION), o cabeçalho que ordena e as salas; à direita a sala escolhida (a prévia do mapa com as
// fichas, quem está nela e o JOIN). As salas vêm do jogo (api/lan.json a cada segundo) e os botões viram os comandos da LAN de hoje
// (browse, host, join:<n>, back). Medidas do Chrome: Tools/measure_multiplayer.js → multiplayer_layout.json.

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RTSShellUI.h"

class SRTSMultiplayer : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSMultiplayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** A lista voltou à tela: a busca vazia, a LAN procurando de novo. */
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
	/** Uma sala da lista (a ROOMS da página, montada do api/lan.json como o poll dela). */
	struct FRoom
	{
		FString Name, Host, Mode, Ver, Sw;
		int32 Map = 0, Players = 0, Max = 8, Ping = 2, Cash = 10000, Idx = -1;
		bool bGame = false;
	};
	struct FMapInfo { FString Real, Name; TArray<FVector2f> Starts; };
	enum class EHit : uint8 { None, Back, Src, Host, Search, Chip, Th, Row, Join };
	struct FHit
	{
		EHit Kind = EHit::None;
		int32 A = -1;
		bool operator==(const FHit& O) const { return Kind == O.Kind && A == O.A; }
	};

	FHit HitAt(FVector2f P) const;
	/** As salas que passam na busca e nos filtros, na ordem (os índices de Rooms). */
	TArray<int32> Visible() const;
	void Poll();
	void Join(int32 Room);
	void Back();
	bool Full(const FRoom& R) const { return R.Players >= R.Max; }
	bool Bad(const FRoom& R) const { return R.Ver != TEXT("0.5.2"); }
	float HoverOf(EHit Kind, int32 A = -1) const;
	float MaxScroll() const;

	void PaintSide(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintList(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 Room, float Top) const;
	void PaintDetail(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	TArray<FMapInfo> Maps;
	TArray<FRoom> Rooms;
	FString LastJson;
	int32 Sel = 0;
	FString Query;
	bool bSearchFocus = false;
	bool bFull = true, bInGame = false, bMyVer = true;   // os filtros da página (F)
	int32 SortK = 4;                                       // 0 ROOM, 1 MAP, 2 PLAYERS, 3 MODE, 4 PING
	bool bSortUp = true;
	float Scroll = 0.0f, ScrollTo = 0.0f;
	FHit Hover;
	TMap<uint32, float> HoverEase;
	double Now = 0.0, CaretT0 = 0.0, PollAt = 0.0;
	bool bTestHosted = false;   // -RTSMpTestHost: o teste da rede de verdade (hospeda sozinho)
	float T = 0.0f;
};
