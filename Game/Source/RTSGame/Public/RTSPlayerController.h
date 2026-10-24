#pragma once

// O jogador: seleção, ordens e câmera, no esquema do Zero Hour (Vanguard ADR-0004):
//  - botão esquerdo faz tudo: clique seleciona; arrastar faz a caixa; com seleção,
//    clique no chão = andar, clique no inimigo = atacar; Shift soma/tira;
//    dois cliques = todos iguais na tela;
//  - botão direito: clique rápido desseleciona, arrastar rola a câmera; os dois juntos arrastando = a linha (v4, ELineChord);
//  - A = próxima ordem é ataque andando; S = parar; Ctrl+número grava grupo,
//    número chama, dois toques centralizam; Esc = pausa.
// Também cuida dos botões do menu e das telas de pausa e de fim.

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTSPlayerController.generated.h"

class UStaticMeshComponent;

class ARTSCameraPawn;
class ARTSHUD;
class URTSMatchSubsystem;

/** Pulso no minimapa onde uma ordem dada por ele chega. */
struct FRTSMinimapPulse
{
	FVector2D Cm = FVector2D::ZeroVector;
	float Age = 0.0f;
	bool bAttack = false;
};

/** "Sob ataque" no minimapa (ZH: 4 s de vida). */
struct FRTSMinimapPing
{
	FVector2D Cm = FVector2D::ZeroVector;
	float Age = 0.0f;
};

/** Marca de ordem no chão (verde = andar, vermelho = atacar), some sozinha. */
struct FRTSOrderMarker
{
	FVector World = FVector::ZeroVector;
	float Age = 0.0f;
	bool bAttack = false;
};

UCLASS()
class RTSGAME_API ARTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARTSPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	// leitura pro HUD
	bool IsMenu() const { return bMenu; }
	/** A página do menu: a principal, a da LAN (criar ou achar partida) e a sala de espera. */
	enum class EMenuPage : uint8 { Main, Lan, Lobby };
	EMenuPage GetMenuPage() const { return MenuPage; }
	/** O IP digitado na tela da LAN (entrar direto, quando a sala não aparece na lista). */
	const FString& GetIpEntry() const { return IpEntry; }
	bool IsTypingIp() const { return bTypingIp; }
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	/** O 1º clique no SURRENDER arma; o 2º rende (sem render-se sem querer). */
	bool IsSurrenderArmed() const { return bSurrenderArmed; }
	/** O preset de qualidade de agora (0 baixa … 3 ultra), pelo nível da Unreal. */
	int32 GetQualityPreset() const;
	bool IsAttackMoveArmed() const { return bAttackMoveArmed; }
	/** Os grupos 1–0 (a tecla K = 0..9: o 1 é o 0, o 0 é o 9), pro HUD novo. */
	const TMap<int32, TArray<uint32>>& GetGroups() const { return Groups; }
	bool IsDragging(FVector2D& OutA, FVector2D& OutB) const;
	/** (v4, 23/10) A linha dos dois botões sendo puxada: do ponto apertado (StartCm) até o cursor (EndCm), no chão; CenterCm é
	 *  o ponto da ordem (a fileira da frente fica em cima da linha), FacingDeg pra onde olham (puxar da esquerda pra direita na
	 *  tela = de costas pra câmera), WidthM a largura. bAttack: o ataque andando armado; bFormation: o F segurado. */
	struct FLineDrag
	{
		FVector2D StartCm = FVector2D::ZeroVector;
		FVector2D EndCm = FVector2D::ZeroVector;
		FVector2D CenterCm = FVector2D::ZeroVector;
		float FacingDeg = 0.0f;
		float WidthM = 0.0f;
		bool bAttack = false;
		bool bFormation = false;
	};
	/** Falso fora do acorde, ou ainda na zona morta (o cursor a menos de 6 px do ponto apertado). A prévia e a ordem usam esta conta. */
	bool GetLineDrag(FLineDrag& Out) const;
	const TArray<uint32>& GetSelection() const { return Selected; }
	const TArray<FRTSOrderMarker>& GetMarkers() const { return Markers; }
	bool ShowDebug() const { return bShowDebug; }
	/** A unidade sob o mouse (0 = nenhuma): o anel acende e a barra aparece, como no ZH. */
	uint32 GetHoverId() const { return HoverId; }
	/** Quantas ordens o jogador já deu (a linha de ajuda some depois das primeiras). */
	int32 OrdersGiven() const { return Orders; }
	const FString& GetError() const { return Error; }
	void SetError(const FString& InError) { Error = InError; }
	/** O teste automático desliga a rolagem pela borda (o mouse de verdade pode estar parado na borda). */
	void SetEdgeScroll(bool bOn) { bEdgeScroll = bOn; }

	/** Ação de um botão do HUD (menu, pausa, fim). */
	void OnButton(FName Id);

	const TArray<FRTSMinimapPulse>& GetMinimapPulses() const { return MinimapPulses; }
	const TArray<FRTSMinimapPing>& GetMinimapPings() const { return MinimapPings; }
	// economia e construção (HUD provisório, docs/ECONOMIA.md)
	/** Posicionando um prédio: o tipo (−1 = não) e o lado pra onde ele olha. */
	int32 GetPlacingDef() const { return PlacingDef; }
	/**
	 * O mouse que o jogo lê: o de verdade, ou o do teste automático. O teste leva o cursor do
	 * Windows junto, mas o Felipe usando o PC ao mesmo tempo tirava ele da janela entre o
	 * "mover" e o "apertar", e o clique saía sem posição (base6/base7, 10/10).
	 */
	bool ReadMouse(float& X, float& Y) const;
	void SetTestMouse(const FVector2D& Pixel) { bTestMouse = true; TestMouse = Pixel; }
	/** O teste: a unidade ou o prédio debaixo desse ponto da tela (0 = chão livre). */
	uint32 UnitUnderScreen(const FVector2D& Screen) const { return PickUnit(Screen); }
	int32 GetPlacingDir() const { return PlacingDir; }
	/** O chão sob o mouse (o fantasma do prédio). */
	bool GetCursorGround(FVector& Out) const;
	/** O primeiro prédio próprio da seleção (o painel mostra ele). */
	uint32 SelectedStructure() const;
	/** (v4, 23/10) O cursor agora é a chave do conserto (o Dozer selecionado sobre o prédio avariado)? Pro teste. */
	bool IsRepairCursorShown() const { return bRepairCursor; }
	/** As unidades da seleção que constroem (Dozers) ou que coletam (caminhões). */
	void SelectedWith(bool bConstructors, TArray<uint32>& Out) const;

	/** O letreiro "Unidade sob ataque" (some em 3 s). */
	float AttackMessageAlpha() const { return FMath::Clamp(1.0f - (AttackMessageAge - 2.5f) / 0.5f, 0.0f, 1.0f) * (AttackMessageAge < 3.0f ? 1.0f : 0.0f); }
	/** Os segundos desde o último "Unidade sob ataque" (o aviso do HUD novo entra deslizando nos primeiros 0,3 s). */
	float AttackMessageSeconds() const { return AttackMessageAge; }

	// (v4, 23/10) os poderes do general: o botão da coluna ou a tecla (F1–F3) entram no modo de mira, como o de posicionar
	// prédio; o esquerdo no chão (ou no minimapa) usa, o direito ou o Esc desistem
	/** O poder sendo mirado (o índice em Rules.Powers; −1 = nenhum): o HUD desenha o círculo do raio no chão. */
	int32 GetTargetingPower() const { return TargetingPower; }
	/** A recusa do último pedido de poder (o botão sem estar pronto, o ponto que não vale) e há quantos segundos. */
	const FText& GetPowerMessage(float& OutAgeS) const;

private:
	void TickMenu();
	void TickMatch(float DeltaTime);
	void TickCamera(float DeltaTime, ARTSCameraPawn* Cam);
	/** O minimapa pega o gesto inteiro quando o botão desce em cima dele (o ZH engole o clique). */
	void TickMinimap(float DeltaTime, ARTSCameraPawn* Cam, const FVector2D& Mouse, bool bHasMouse);
	void TickAlerts(float DeltaTime);
	void HandleLeftClick(const FVector2D& Mouse);
	void HandleBoxSelect(const FVector2D& A, const FVector2D& B);
	// (v4, 23/10) a linha dos dois botões (o acorde esquerdo + direito, como o campo minado do Windows): o estado e a máquina
	/** None: sem acorde. Active: os dois apertados, puxando. Releasing: um subiu, o outro tem o tempo de graça pra subir junto.
	 *  Swallow: acabou (mandou ou desistiu); os botões ainda apertados não fazem nada até os dois subirem. */
	enum class ELineChord : uint8 { None, Active, Releasing, Swallow };
	void TickLineChord(const FVector2D& Mouse, bool bHasMouse);
	void CommitLine();
	void CancelLine(const TCHAR* Why);
	bool HasOwnTroopsSelected() const;
	ELineChord LineChord = ELineChord::None;
	/** Os dois pontos da linha no chão (cm, o XY): a linha fica presa no mapa se a câmera andar. */
	FVector2D LineStartCm = FVector2D::ZeroVector;
	FVector2D LineEndCm = FVector2D::ZeroVector;
	/** A linha saiu da zona morta (o cursor passou de 6 px do ponto apertado, na tela). */
	bool bLineOpened = false;
	double LineReleaseAt = 0.0;
	double LeftDownAt = 0.0;
	void HandleGroups();
	void SelectSameTypeOnScreen(uint32 Id);
	uint32 PickUnit(const FVector2D& Mouse) const;
	bool GroundUnderCursor(const FVector2D& Mouse, FVector& Out) const;
	void PruneSelection();
	void AddMarker(const FVector& At, bool bAttack);
	/** bRemember: guarda no GameUserSettings.ini (o próximo jogo abre nele); o teste automático não guarda. */
	void ApplyQuality(int32 Level, bool bRemember = true);
	// economia e construção (RTSPlayerEconomy.cpp)
	void BeginPlacing(int32 Def);
	/** O modo de posicionar pega o clique e o Esc; true = a entrada foi dele neste quadro. */
	bool TickPlacement(const FVector2D& Mouse, bool bHasMouse);
	/** Clique esquerdo no chão com seleção: ponto de encontro, coletar, retomar a obra. true = tratou. */
	bool HandleEconomyClick(const FVector2D& Mouse, uint32 Hit);
	int32 DockAt(const FVector& Ground) const;
	uint32 StructureAt(const FVector& Ground) const;
	/** O fantasma do prédio no lugar: o modelo em holograma, verde se cabe, vermelho se não. */
	void UpdateGhost();
	int32 PlacingDef = -1;
	int32 PlacingDir = 0;
	/** (v4, 23/10) O cursor do conserto (a chave do kit, docs/concept/hud/kit/icons/repair.png, recortada da placa e com o contorno
	 *  escuro, 32 px): feito uma vez, no 1º uso; falso se a plataforma não deixa (fica a mão de sempre). */
	bool EnsureRepairCursor();
	void* RepairCursorHandle = nullptr;
	int8 RepairCursorState = 0;   // 0 ainda não tentou, 1 pronto, −1 não deu
	bool bRepairCursor = false;
	bool bTestMouse = false;
	FVector2D TestMouse = FVector2D::ZeroVector;
	UPROPERTY() TObjectPtr<AActor> Ghost;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GhostMesh;
	int32 GhostDef = -1;
	// (v4) o modo de mira do poder do general
	void BeginTargeting(int32 Power);
	/** O modo de mira pega o clique esquerdo no chão; true = a entrada foi dele neste quadro. */
	bool TickTargeting(const FVector2D& Mouse, bool bHasMouse);
	/** Usa o poder mirado no ponto (cm), se vale; senão guarda o motivo e segue mirando. */
	bool TryUsePower(const FVector2D& Cm);
	void SetPowerMessage(const FText& Text);
	int32 TargetingPower = -1;
	FText PowerMessage;
	double PowerMessageAt = -1000.0;
	/** O último "ataque chegando" já posto no minimapa: os segundos de mundo do aviso e quantos avisos com esse mesmo At
	 *  já foram postos (os do mesmo quadro têm o mesmo At). */
	double LastStrikeNoticeAt = -1.0;
	int32 LastStrikeNoticeCount = 0;
	URTSMatchSubsystem* Match() const;
	ARTSHUD* GetRTSHUD() const;

	bool bMenu = true;
	EMenuPage MenuPage = EMenuPage::Main;
	bool bNetAutoDone = false;
	FString IpEntry;
	bool bTypingIp = false;
	void TickIpEntry();
	void JoinTypedIp();
	double BotStartAt = 0.0;
	double BotExitAt = 0.0;
	bool bPauseMenu = false;
	bool bSurrenderArmed = false;
	bool bAttackMoveArmed = false;
	bool bShowDebug = false;
	bool bEdgeScroll = true;
	uint32 HoverId = 0;
	int32 Orders = 0;
	FString Error;

	TArray<uint32> Selected;
	TMap<int32, TArray<uint32>> Groups;
	TArray<FRTSOrderMarker> Markers;

	bool bLeftDown = false;
	FVector2D LeftDownPos = FVector2D::ZeroVector;
	uint32 LastClickedId = 0;
	double LastClickTime = 0.0;
	int32 LoggedSelection = 0;

	bool bRightDown = false;
	bool bRightScrolling = false;
	FVector2D RightAnchor = FVector2D::ZeroVector;

	bool bMiddleDown = false;
	FVector2D MiddleAnchor = FVector2D::ZeroVector;
	double LastMiddleClickTime = 0.0;

	int32 LastGroupKey = -1;
	double LastGroupKeyTime = 0.0;

	enum class EMinimapGesture : uint8 { None, LeftDrag, LeftDone, RightDrag, Ignore };
	EMinimapGesture MinimapGesture = EMinimapGesture::None;
	bool bOverMinimap = false;
	TArray<FRTSMinimapPulse> MinimapPulses;
	TArray<FRTSMinimapPing> MinimapPings;
	float AttackMessageAge = 1000.0f;
	bool bHasAlert = false;
	FVector2D LastAlertCm = FVector2D::ZeroVector;
	double LastAlertTime = -1000.0;
};
