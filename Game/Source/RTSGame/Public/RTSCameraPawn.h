#pragma once

// A câmera do jogo, com os valores do Zero Hour (Vanguard ADR-0003, Config/DefaultGame.ini):
// inclinação fixa de 37,5°, vista diagonal de 45°, zoom de 2,6× em altura.
//  - rola com o botão direito arrastado (mais longe do ponto apertado = mais rápido),
//    com a borda da tela e com WASD/setas;
//  - gira só no eixo vertical: botão do meio arrastado ou Q/E; dois cliques no meio voltam;
//  - Alt + botão do meio inclina (modo cinematográfico, fora do jogo competitivo).
// No menu ela passeia devagar sobre o mapa.

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTSCameraPawn.generated.h"

class UCameraComponent;

UCLASS(Config = Game)
class RTSGAME_API ARTSCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARTSCameraPawn();

	/** Entrada já traduzida pelo controle, por quadro. */
	struct FInput
	{
		FVector2D PanAxis = FVector2D::ZeroVector;   // teclado/borda, −1..1 (X = direita, Y = frente)
		FVector2D DragPan = FVector2D::ZeroVector;   // arrasto do botão direito, em fração da tela por segundo
		float ZoomSteps = 0.0f;                      // roda: + aproxima
		float YawDelta = 0.0f;                       // graus
		float PitchDelta = 0.0f;                     // graus (só no modo cinematográfico)
		bool bReset = false;
	};

	void ApplyInput(const FInput& In, float DeltaSeconds);
	void FocusOn(const FVector2D& Cm);
	/** Vai deslizando até o ponto (gravação: sem pulos de câmera). */
	void GlideTo(const FVector2D& Cm) { GlideTarget = Cm; bGliding = true; }
	/** Pula pro ponto na hora, mantendo giro e zoom (o clique no minimapa: userLookAt do ZH). */
	void JumpTo(const FVector2D& Cm);
	/**
	 * Com névoa, a exposição local da Unreal não pode misturar a luminância borrada da tela (60%
	 * por padrão): a névoa escura ao lado clarearia o que está à vista. Fica só a grade bilateral,
	 * que separa por faixa de luminância (revisão de 08/10).
	 */
	void SetFogExposureFix(bool bOn);
	void SetMenuMode(bool bMenu) { bMenuMode = bMenu; }
	void SetBounds(const FBox2D& Cm) { BoundsCm = Cm; }
	FVector2D GetFocusCm() const { return FVector2D(Focus.X, Focus.Y); }
	float GetYawDeg() const { return Yaw; }

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * O zoom inclina a câmera (Red Alert 3): de perto ela deita um pouco, sem mostrar o horizonte;
	 * de longe olha mais de cima. A inclinação do padrão é a do ZH (37,5°, a SAGE desde o Generals).
	 * O zoom é a distância da câmera ao ponto olhado; os três pontos da curva são (distância, graus).
	 */
	UPROPERTY(Config) float DefaultPitchDeg = 37.5f;
	UPROPERTY(Config) float DefaultYawDeg = 45.0f;
	UPROPERTY(Config) float MinDistanceM = 67.0f;
	UPROPERTY(Config) float MaxDistanceM = 118.0f;
	UPROPERTY(Config) float DefaultDistanceM = 100.4f;   // = os 61,1 m de altura de antes a 37,5°
	UPROPERTY(Config) float NearPitchDeg = 28.0f;
	UPROPERTY(Config) float FarPitchDeg = 44.0f;
	/** O botão do meio deita a câmera por cima da curva (como o WorldForge), até quase a horizontal. */
	UPROPERTY(Config) float MinFreePitchDeg = 3.0f;
	UPROPERTY(Config) float MaxFreePitchDeg = 80.0f;
	/** O campo de visão: um pouco mais fechado de longe (menos distorção na borda), mais aberto com a câmera deitada (a vista das artes). */
	UPROPERTY(Config) float FieldOfViewDeg = 50.0f;
	UPROPERTY(Config) float FarFieldOfViewDeg = 46.0f;
	UPROPERTY(Config) float LowPitchFieldOfViewDeg = 58.0f;
	/** Velocidade de rolagem: fração da largura visível por segundo. */
	UPROPERTY(Config) float PanScreensPerSecond = 1.1f;

private:
	void UpdateCamera(float DeltaSeconds);
	float GroundZ(const FVector2D& Cm) const;

	UPROPERTY() TObjectPtr<UCameraComponent> Camera;

	FVector Focus = FVector::ZeroVector;
	/** A inclinação da curva do zoom pra esta distância. */
	float CurvePitch(float DistanceM) const;

	float Yaw = 45.0f;
	float YawShown = 45.0f;
	/** O quanto o botão do meio deitou (ou levantou) a câmera por cima da curva do zoom. */
	float FreePitch = 0.0f;
	float FreePitchShown = 0.0f;
	/** A inclinação de fato (a curva + o botão do meio), a que o quadro usa. */
	float Pitch = 37.5f;
	float DistanceM = 100.4f;
	float TargetDistanceM = 100.4f;
	float SmoothedGroundZ = 0.0f;
	bool bMenuMode = false;
	bool bGliding = false;
	FVector2D GlideTarget = FVector2D::ZeroVector;
	float MenuTime = 0.0f;
	FBox2D BoundsCm = FBox2D(FVector2D(-51200.0, -51200.0), FVector2D(51200.0, 51200.0));
};
