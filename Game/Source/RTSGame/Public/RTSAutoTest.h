#pragma once

// Teste automático do fluxo inteiro, do jeito que o Felipe joga: cliques,
// arrastes e teclas sintéticos entregues ao Slate (o mesmo caminho do mouse de
// verdade, como o FForgeUITest do WorldForge). Menu → "Jogar Skirmish" → caixa
// de seleção → ordem de andar → ataque andando → batalha até o fim → "Voltar ao
// menu". Fotografa cada passo e confere no log ("RTS TEST ok/FALHOU ...").
//
// Linha de comando: -RTSAutoTest=skirmish -RTSCapture=<pasta>  (Game/Tools/game.ps1 view -Folder x)

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RTSAutoTest.generated.h"

class ARTSPlayerController;
class FRTSTestInputGate;
class URTSMatchSubsystem;

UCLASS()
class RTSGAME_API URTSAutoTest : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FStep
	{
		FString Name;
		TFunction<void()> Act;
		TFunction<bool()> Ready;
		TFunction<FString()> Check;    // vazio = ok
		float MinWait = 1.0f;
		float Timeout = 30.0f;
		bool bShot = true;
	};

	/** Entrada sintética em fila, um evento por quadro (o controle lê o mouse a cada quadro). */
	struct FInputEvent
	{
		enum class EType : uint8 { Move, Down, Up, KeyDown, KeyUp } Type = EType::Move;
		FVector2D Pos = FVector2D::ZeroVector;   // pixels da viewport
		FKey Key;
	};

	void Build();
	void Finish();
	void PumpInput();
	void QueueClick(const FVector2D& Pixel, FKey Button = EKeys::LeftMouseButton);
	void QueueDrag(const FVector2D& From, const FVector2D& To);
	void QueueKey(FKey K);
	FVector2D ToSlate(const FVector2D& Pixel) const;
	FVector2D ViewportFrac(const FVector2D& Frac) const;
	FVector2D ButtonCenter(FName Id) const;
	ARTSPlayerController* PC() const;
	URTSMatchSubsystem* Match() const;
	bool OwnCentroid(FVector2D& OutCm) const;
	void Shot(const FString& Name);

	FString Folder;
	TArray<FStep> Steps;
	int32 Index = 0;
	int32 Phase = 0;
	float Timer = 0.0f;
	float TotalTime = 0.0f;
	int32 Failures = 0;
	TArray<FInputEvent> Pending;
	TSet<FKey> Pressed;
	TSharedPtr<FRTSTestInputGate> Gate;
	bool bDummyInjecting = false;
	FVector2D LastSlatePos = FVector2D::ZeroVector;

	// medições
	TArray<float> FrameMs;
	FVector2D CentroidBefore = FVector2D::ZeroVector;
	FVector2D MinimapTarget = FVector2D::ZeroVector;
	int32 SelectionBefore = 0;
	double MinimapOrderOffCm = 1e12;
	float NextBattleShot = 0.0f;
	float NextPush = 0.0f;
	bool bStallLogged = false;
	float NextFollow = 0.0f;
	int32 BattleShots = 0;
	bool bActive = false;
	/** -RTSRecord: grava uma foto por quadro do combate (com -benchmark -fps=30, passo fixo) pra virar vídeo. */
	bool bRecord = false;
	bool bRecordDone = false;
	int32 RecordFrames = 0;
	float RecordTail = 0.0f;
	bool BattleFocus(FVector2D& OutCm) const;
};
