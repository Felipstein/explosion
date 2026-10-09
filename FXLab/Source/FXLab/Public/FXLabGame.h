#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "FXLabGame.generated.h"

class ACameraActor;
class AFXImpact;
class AFXWreckFire;
class SWidget;

/** Sem boneco: só a câmera do laboratório. */
UCLASS()
class FXLAB_API AFXLabGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AFXLabGameMode();
};

/**
 * A câmera e o painel do laboratório: câmera de RTS e baixa (as mesmas do navegador:
 * campo vertical de 32°), girar com o botão direito, aproximar com a rodinha, pausar e
 * 0.25×. Exposição fixa em 1 (sem adaptação do olho), como o laboratório.
 *
 * No mapa do jogo (World_Desert, sol "GameSun"): a câmera do jogo (Game/Config/DefaultGame.ini:
 * 37,5° de inclinação, 45° de giro, 50° de campo, 61,1 m de altura), a exposição travada do
 * mapa, vários efeitos ao mesmo tempo (carga 1+1 ou 3+3), as fotos (-Shot) e a medida do
 * quadro (-Measure).
 */
UCLASS()
class FXLAB_API AFXLabController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	void SetCam(const FString& Name);
	void TogglePause();
	void ToggleSlow();
	/** Quantos efeitos ligados no mapa do jogo: 0 (nenhum), 1 (1 fogo + 1 tiro) ou 3 (3 + 3). */
	void SetLoad(int32 N);

private:
	void ZoomIn();
	void ZoomOut();
	void ApplyOrbit();
	void SetPausedAll(bool bPause);
	bool ShadersBusy() const;
	void TickShots();
	void TickMeasure(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ViewCam;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFXWreckFire>> Fires;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFXImpact>> Impacts;

	TSharedPtr<SWidget> Panel;
	FString CamName;
	FVector Target = FVector::ZeroVector;
	FVector Focus = FVector::ZeroVector;     // cm: o grupo principal (carcaça e tiro) no mapa do jogo
	double Yaw = 0.0, Pitch = 0.0, Dist = 1000.0;
	bool bSlow = false;
	bool bGame = false;
	int32 Load = 1;

	// fotos no mapa do jogo: -Shot=nome -At=1,2,3 (com -benchmark -fps=30); o relógio só anda
	// depois dos shaders compilarem (efeitos parados até lá)
	FString ShotName;
	TArray<double> ShotAt;
	int32 Frame = 0, Warm = 0, ExitIn = -1;
	bool bWarming = false;

	// medida: -Measure=nome
	FString MeasureName;
	int32 MeasurePhase = -1;
	double PhaseClock = 0.0, LastTime = 0.0;
	TArray<float> FrameMs, GpuMs, GameMs, RenderMs;
	TArray<FString> Report;

	// o que o painel mostra: os últimos quadros
	TArray<float> RecentMs, RecentGpu;
	FString Readout;
};
