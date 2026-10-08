#pragma once

#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "BlastLabController.generated.h"

class ABlastLabCamera;
class AExplosionVolume;
class SBlastLabPanel;
class UTexture2D;

/**
 * Entrada do simulador, como no WebGL (reference/webgl/js/main.js):
 * - botão esquerdo: clique no terreno detona a arma escolhida; arrastado orbita
 *   (passou de 5 px de arrasto, não é mais clique);
 * - roda: zoom; botão direito arrastado: rolagem estilo Generals;
 * - P pausa · [ ] câmera lenta/rápida · \ volta a 1× · , . hora −/+10 min ·
 *   T ciclo · 1–4 qualidade · H esconde o painel · espaço detona no centro ·
 *   A avião · M míssil · D disparo · B bombardeiro atravessando a fumaça da última explosão.
 *
 * -AutoTest=<pasta> na linha de comando (Tools/blast.ps1 view -Folder): espera
 * a exposição assentar, detona a arma -AutoWeapon (padrão Carga Pesada) no
 * centro, grava telas em instantes fixos da explosão e o tempo de quadro no
 * log, e fecha. É o teste do fluxo real sem ninguém clicando.
 */
UCLASS()
class BLASTLAB_API ABlastLabController : public APlayerController
{
	GENERATED_BODY()

public:
	ABlastLabController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Detona a arma escolhida no ponto do chão (com o tranco de câmera). */
	AExplosionVolume* DetonateAt(const FVector& Ground);

private:
	ABlastLabCamera* Cam() const;
	bool GroundUnderCursor(FVector& Out) const;
	bool GroundUnderScreenPoint(const FVector2D& Screen, FVector& Out) const;

	void LeftDown();
	void LeftUp();
	void RightDown();
	void RightUp();
	void Wheel(float Value);
	void TogglePause();
	void Slower();
	void Faster();
	void ResetTime();
	void EarlierHour();
	void LaterHour();
	void ToggleCycle();
	void TogglePanel();
	void DetonateCenter();
	void PassPlane();
	void PassMissile();
	void PassShell();
	void PassBomber();
	template <int32 Level> void Quality();

	void TickAutoTest(float DeltaTime);

	/** Miniatura dos cartões: a referência fica aqui (o pincel do Slate não segura a textura). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Thumbnail;

	TSharedPtr<SBlastLabPanel> Panel;

	bool bLeft = false, bRight = false;
	FVector2D LeftStart, LastMouse, Anchor;
	float DragPixels = 0.0f;

	// teste automático
	FString AutoFolder;
	int32 AutoWeapon = 2;
	float AutoClock = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<AExplosionVolume> AutoExplosion;
	TArray<float> AutoTimes = { 0.4f, 0.8f, 1.5f, 2.5f, 4.0f };
	float AutoT = 0.0f;          // relógio do teste (s da gravação)
	bool bAutoDetonated = false;
	float AutoScale = 1.0f;
	int32 AutoNext = 0;
	bool bPreShot = false;
	bool bAutoWatch = false;
	bool bReadyLogged = false;
	bool bAutoDone = false;
	bool bEnvLogged = false;
	TArray<float> PendingView;
	TArray<TPair<int32, float>> AutoPasses;     // (0 avião · 1 míssil · 2 disparo, instante)
	bool bAutoPassesParsed = false;
	TArray<float> AutoGpuDumps;                 // -AutoGpuDump=1.5,7: instantes da árvore de tempos de GPU
	double AutoSumDt = 0.0;
	TArray<float> AutoFrameTimes, AutoBaseTimes;
	int32 AutoFrames = 0;
};

UCLASS()
class BLASTLAB_API ABlastLabGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABlastLabGameMode();
};
