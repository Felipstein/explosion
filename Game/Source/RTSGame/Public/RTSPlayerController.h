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

class ARTSCameraPawn;
class ARTSHUD;
class URTSMatchSubsystem;

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

private:
	void TickMenu();
	void TickMatch(float DeltaTime);
	void TickCamera(float DeltaTime, ARTSCameraPawn* Cam);
	void HandleLeftClick(const FVector2D& Mouse);
	void HandleBoxSelect(const FVector2D& A, const FVector2D& B);
	void HandleGroups();
	void SelectSameTypeOnScreen(uint32 Id);
	uint32 PickUnit(const FVector2D& Mouse) const;
	bool GroundUnderCursor(const FVector2D& Mouse, FVector& Out) const;
	void PruneSelection();
	void AddMarker(const FVector& At, bool bAttack);
	void ApplyQuality(int32 Level);
	URTSMatchSubsystem* Match() const;
	ARTSHUD* GetRTSHUD() const;

	bool bMenu = true;
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

	bool bRightDown = false;
	bool bRightScrolling = false;
	FVector2D RightAnchor = FVector2D::ZeroVector;

	bool bMiddleDown = false;
	FVector2D MiddleAnchor = FVector2D::ZeroVector;
	double LastMiddleClickTime = 0.0;

	int32 LastGroupKey = -1;
	double LastGroupKeyTime = 0.0;
};
