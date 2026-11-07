#pragma once

// A sala do skirmish nativa (04/11): o mockup aprovado (docs/concept/ui/menu/skirmish.html) refeito em Slate, como o menu. À esquerda
// a coluna (a operação, o mapa escolhido, o FILL/CLEAR AI e o TEAMS, as vagas com o exército, a cor, o time e a largada, o dinheiro e
// as superarmas); à direita o mapa no losango do minimapa do jogo, com as bases, as fichas que se arrastam e a bandeja da largada
// sorteada. As medidas são as do Chrome (Tools/measure_skirmish*.js → skirmish_layout.json e skirmish_pops.json); os losangos dos
// mapas saem do próprio mockup (Tools/export_skirmish_maps.py). O START manda o mesmo JSON que a página mandava pro jogo.

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "RTSShellUI.h"

class SRTSSkirmish : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSSkirmish) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/** A sala voltou à tela: a última configuração, sem janelinha aberta nem hover. */
	void Restart();
	/** A sala da LAN (05/11; o skirmish.html?mp=1&lan=1 da página): o mesmo desenho, com o estado vindo do jogo (api/lobby.json a cada
	 *  meio segundo) e os cliques virando os comandos da rede de hoje (a cor e o time em sequência, pôr e tirar IA, o dinheiro, começar
	 *  e sair). bOn falso volta pro skirmish comum. */
	void SetLan(bool bOn, bool bHost = false, const FString& HostName = FString());

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(RTSShellUI::StageW, RTSShellUI::StageH); }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** O jogador das vagas: você, a IA, aberta ou fechada, e o outro humano da sala da rede. */
	enum class EKind : uint8 { You, Ai, Open, Closed, Human };
	struct FSlot
	{
		EKind Kind = EKind::Open;
		int32 Fac = 0;      // 0 aleatório, 1 USA, 2 Rússia, 3 China, 4 GLA (a Rússia sempre depois dos EUA)
		int32 Color = -1;   // o índice das 24 cores; −1 = aleatória (vira uma cor livre na largada)
		int32 Team = 0;     // 0 = sem time
		int32 Base = -1;    // a base no mapa; −1 = sorteada
		int32 Ai = 1;       // 0 fácil, 1 médio, 2 difícil
		FString Name;       // o nome na rede (você e os outros humanos da sala da LAN)
		bool bReady = false;
		int32 Ping = 0;     // a sala da LAN: o ping (ms) e quem é o anfitrião (a coroa)
		bool bHost = false;
		uint32 NetHex = 0;  // a sala da LAN: a cor que o jogo dá (a automática, quando a vaga não escolheu)
	};
	struct FMapEntry
	{
		FString Real, Name, Size, Terr, Cond, Src;
		int32 Players = 0, Plays = 0, Date = 0;
		bool bNaval = false;
		TArray<FVector2f> Starts, Docks;   // em metros do mapa (−512..512), como o mockup
	};

private:
	/** As peças que o mouse acerta: cada uma com o retângulo no palco. */
	enum class EHit : uint8 { None, Back, Change, Fill, Diff, Clear, Teams, Cell, Fader, Seg, Start, Base, TrayToken, TrayReset, PopItem, Outside,
		BwBack, BwSearch, BwChip, BwCard, BwFav, BwNew, Kick, ChatInput };
	struct FHit
	{
		EHit Kind = EHit::None;
		int32 A = -1, B = -1;   // a vaga e o campo, o índice do item, a base…
		bool operator==(const FHit& O) const { return Kind == O.Kind && A == O.A && B == O.B; }
	};
	enum EField : int32 { FldWho = 0, FldFac, FldColor, FldTeam, FldBase, FldTeams };
	/** Um item da janelinha aberta: o retângulo, o valor e se é o atual. */
	struct FPopItem { FVector4f R; int32 Value = 0; bool bCur = false; bool bSep = false; bool bDisabled = false; };

	FGeometry StageOf(const FGeometry& G) const;
	FVector2f StagePos(const FGeometry& G, const FPointerEvent& E) const;
	FHit HitAt(FVector2f P) const;
	int32 Rows() const;
	float RowTop(int32 I) const;
	float RowH() const;
	bool Live(const FSlot& S) const { return S.Kind == EKind::You || S.Kind == EKind::Ai || S.Kind == EKind::Human; }
	/** O canEdit do mockup: no skirmish tudo; na rede, a sua vaga e (o anfitrião) as que não são de outro humano. */
	bool CanEdit(int32 I) const { return !bLan || Slots[I].Kind == EKind::You || (bLanHost && Slots[I].Kind != EKind::Human); }
	/** O nome fixo na rede (o .cell.fixed): você, os outros humanos e, pra quem não é o anfitrião, todas. */
	bool LanWhoFixed(int32 I) const { return bLan && (Slots[I].Kind == EKind::You || Slots[I].Kind == EKind::Human || !bLanHost); }
	/** O START aceso: no skirmish sempre; na sala da rede, o anfitrião com todos prontos (ou na contagem, pra cancelar) e o READY de quem entrou. */
	bool LanStartOn() const { return !bLan || !bLanHost || bLanCanStart || HostCd > 0; }
	FLinearColor SlotColor(const FSlot& S) const;
	FVector2f ToDiamond(FVector2f M) const;
	int32 Occupant(int32 Base) const;
	TArray<int32> TrayList() const;
	FVector2f TrayPos(int32 K) const;
	float TraySpacing() const;
	int32 BaseAt(FVector2f P) const;
	FString Compass(int32 Base) const;
	FString OpName() const;
	FString WhoText(const FSlot& S) const;

	// as ações
	void OpenPop(int32 Row, int32 Field);
	void BuildPop();
	void ChoosePop(int32 Item);
	void ApplyTeams(int32 Kind);
	void Place(int32 Slot, int32 Base);
	void ChooseMap(int32 Index);
	void StartMatch();
	void Back();
	void Save();
	bool Load();
	void SetCash(int32 V);
	// a sala da LAN
	void PollLobby();
	void LanAct(const FString& Command);
	/** (06/11) A sala da rede igual à do skirmish: o que mudou na tela desde a última leitura vira o comando da rede, campo a campo. */
	void LanSync();
	/** O campo F da vaga I mexe aqui? O anfitrião: a dele e as da IA (e as abertas e fechadas); quem entrou: só a dele. */
	bool LanMayEdit(int32 I, int32 F) const;
	/** A montagem (o mapa, as vagas, quem está pronto, o dinheiro): a contagem do anfitrião cancela se ela mudar. */
	FString LanSetupKey() const;
	/** Uma linha do chat da sala (bSys = a linha cinza do sistema: entrou, saiu, pronto, a contagem). */
	void ChatSay(const FString& Who, uint32 Hex, const FString& Text, bool bSys);
	/** (06/11) A largada pra tela de carregar (FRTSShell::SetIntro): a operação, o mapa e cada vaga viva com a cor resolvida. */
	void PushIntro(const TArray<uint32>& Hexes) const;
	float ChatTop() const;
	void PaintChat(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	// a pintura
	void PaintColumn(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintRow(FSlateWindowElementList& Out, int32& L, const FGeometry& S, int32 I) const;
	void PaintMap(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintToken(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f C, const FSlot& Slot, float R, float Alpha = 1.0f) const;
	void PaintFacToken(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f TopLeft, int32 Fac, float Size) const;
	void PaintMini(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f TopLeft, int32 Base) const;
	void PaintLabel(FSlateWindowElementList& Out, int32 L, const FGeometry& S, FVector2f At, const FSlot& Slot, float Alpha) const;
	void PaintTray(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintLink(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintPop(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	/** A dica da cor com o mouse em cima (o #tip): o nome, quem já usa e o aviso de cor que some no chão do mapa. */
	void PaintTip(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintButtons(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;

	// o navegador de mapas (o CHANGE MAP): a busca, os filtros, a ordem e a grade dos cartões
	void OpenBrowser();
	void CloseBrowser();
	void Refilter();
	float BwMaxScroll() const;
	FHit BrowserHitAt(FVector2f P) const;
	bool ChipOn(int32 Chip) const;
	void ToggleChip(int32 Chip);
	void LoadFavs();
	void SaveFavs() const;
	/** A transição do CSS de cada peça (o hover vai e volta em ~0,15 s): 0 parado, 1 com o mouse em cima. */
	float HoverOf(EHit Kind, int32 A = -1) const;
	void PaintBrowser(FSlateWindowElementList& Out, int32& L, const FGeometry& S) const;
	void PaintCard(FSlateWindowElementList& Out, int32& L, const FGeometry& G, int32 K, FVector2f At) const;

	TArray<FMapEntry> Maps;
	int32 MapIdx = 0;
	TArray<FSlot> Slots;
	int32 Cash = 0, Sw = 0, FillDiff = 1, Sel = 0;
	TSet<int32> SelSet;
	FHit Hover;
	int32 HoverRow = -1, HoverBase = -1;
	// a janelinha aberta: o campo, a vaga, a caixa e os itens
	int32 PopField = -1, PopRow = -1, PopHover = -1;
	FVector4f PopBox;
	TArray<FPopItem> PopItems;
	// a ficha arrastada
	int32 DragSlot = -1;
	bool bDragMoved = false, bFaderDrag = false;
	FVector2f DragPos, DragFrom;
	double Now = 0.0;
	float T = 0.0f, Fade = 0.0f, Launch = 0.0f, StartHover = 0.0f, BackHover = 0.0f;
	bool bStarting = false;
	// o navegador aberto: a busca digitada, a ordem, os filtros (os jogadores, o tamanho, o terreno, a condição e a origem), a lista
	// filtrada (índices de Maps), a rolagem da grade, os favoritos e a semente do mundo novo
	bool bBrowser = false;
	double BwT0 = 0.0, CaretT0 = 0.0;
	FString Query;
	int32 SortBy = 0;
	bool bFavOnly = false, bNavalOnly = false;
	TSet<FString> Filt[5];
	TArray<int32> List;
	float Scroll = 0.0f, ScrollTo = 0.0f;
	TSet<FString> Favs;
	int32 NewSeed = 48213;
	TMap<uint32, float> HoverEase;
	mutable float BwA = 1.0f;
	// a sala da LAN: o título (LAN ROOM · <ANFITRIÃO>'S ROOM), quem é o anfitrião, se dá pra começar, a próxima leitura e o último
	// dinheiro mandado
	bool bLan = false, bLanHost = false, bLanCanStart = false;
	bool bTestAi = false;   // -RTSMpTestAddAi: o teste da rede de verdade (o anfitrião põe uma IA)
	FString LanKicker;
	double LanPollAt = 0.0;
	// (06/11) a sala da última leitura (o que a tela mudou sai da diferença), o chat (as linhas, quantas do jogo já entraram, o que
	// está sendo digitado e se o campo tem o foco), quem está na sala e se está pronto (as linhas do sistema saem da diferença) e a
	// contagem (a que a sala mostra; a que o anfitrião roda, o próximo segundo e a montagem que ela protege)
	TArray<FSlot> LanSent;
	struct FChatMsg { FString Who; uint32 Hex = 0xffffff; FString Text; bool bSys = false; };
	TArray<FChatMsg> ChatLog;
	int32 ChatSeenTotal = -1;
	FString ChatDraft;
	bool bChatFocus = false;
	TMap<FString, bool> LanPeople;
	bool bLanFirstPoll = true;
	int32 LanCountdown = 0, HostCd = 0;
	// -RTSRoomTest=<arquivo>: o teste da sala da rede sem clicar (cada linha "segundos|comando"; o comando vai pelo LanAct, "start" é o
	// START/READY, "say:<texto>" o chat); a hora em que a sala abriu e quantas linhas já foram; a última sala escrita no log
	TArray<TPair<double, FString>> RoomTest;
	double RoomT0 = 0.0;
	int32 RoomNext = 0;
	FString RoomLogged;
	double CdNext = 0.0;
	FString CdKey;
	struct FPending { double At = 0.0; TFunction<void()> Run; };
	TArray<FPending> Pending;
};
