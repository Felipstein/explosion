#pragma once

// O jogador: seleção, ordens e câmera, no esquema do Zero Hour (Vanguard ADR-0004):
//  - botão esquerdo faz tudo: clique seleciona; arrastar faz a caixa; com seleção,
//    clique no chão = andar, clique no inimigo = atacar; Shift soma/tira;
//    dois cliques = todos iguais na tela;
//  - botão direito: clique rápido desseleciona, arrastar rola a câmera;
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
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	bool IsAttackMoveArmed() const { return bAttackMoveArmed; }
	bool IsDragging(FVector2D& OutA, FVector2D& OutB) const;
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
	/** As unidades da seleção que constroem (Dozers) ou que coletam (caminhões). */
	void SelectedWith(bool bConstructors, TArray<uint32>& Out) const;

	/** O letreiro "Unidade sob ataque" (some em 3 s). */
	float AttackMessageAlpha() const { return FMath::Clamp(1.0f - (AttackMessageAge - 2.5f) / 0.5f, 0.0f, 1.0f) * (AttackMessageAge < 3.0f ? 1.0f : 0.0f); }

private:
	void TickMenu();
	void TickMatch(float DeltaTime);
	void TickCamera(float DeltaTime, ARTSCameraPawn* Cam);
	/** O minimapa pega o gesto inteiro quando o botão desce em cima dele (o ZH engole o clique). */
	void TickMinimap(float DeltaTime, ARTSCameraPawn* Cam, const FVector2D& Mouse, bool bHasMouse);
	void TickAlerts(float DeltaTime);
	void HandleLeftClick(const FVector2D& Mouse);
	void HandleBoxSelect(const FVector2D& A, const FVector2D& B);
	void HandleGroups();
	void SelectSameTypeOnScreen(uint32 Id);
	uint32 PickUnit(const FVector2D& Mouse) const;
	bool GroundUnderCursor(const FVector2D& Mouse, FVector& Out) const;
	void PruneSelection();
	void AddMarker(const FVector& At, bool bAttack);
	void ApplyQuality(int32 Level);
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
	bool bTestMouse = false;
	FVector2D TestMouse = FVector2D::ZeroVector;
	UPROPERTY() TObjectPtr<AActor> Ghost;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GhostMesh;
	int32 GhostDef = -1;
	URTSMatchSubsystem* Match() const;
	ARTSHUD* GetRTSHUD() const;

	bool bMenu = true;
	EMenuPage MenuPage = EMenuPage::Main;
	bool bNetAutoDone = false;
	double BotStartAt = 0.0;
	double BotExitAt = 0.0;
	bool bPauseMenu = false;
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
