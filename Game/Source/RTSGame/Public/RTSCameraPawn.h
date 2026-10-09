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

	UPROPERTY(Config) float DefaultPitchDeg = 37.5f;
	UPROPERTY(Config) float DefaultYawDeg = 45.0f;
	UPROPERTY(Config) float MinHeightM = 31.6f;
	UPROPERTY(Config) float MaxHeightM = 81.6f;
	UPROPERTY(Config) float DefaultHeightM = 61.1f;
	UPROPERTY(Config) float FieldOfViewDeg = 50.0f;
	UPROPERTY(Config) float MinPitchDeg = 12.0f;
	UPROPERTY(Config) float MaxPitchDeg = 75.0f;
	/** Velocidade de rolagem: fração da largura visível por segundo. */
	UPROPERTY(Config) float PanScreensPerSecond = 1.1f;

private:
	void UpdateCamera(float DeltaSeconds);
	float GroundZ(const FVector2D& Cm) const;

	UPROPERTY() TObjectPtr<UCameraComponent> Camera;

	FVector Focus = FVector::ZeroVector;
	float Yaw = 45.0f;
	float Pitch = 37.5f;
	float HeightM = 61.1f;
	float TargetHeightM = 61.1f;
	float SmoothedGroundZ = 0.0f;
	bool bMenuMode = false;
	bool bGliding = false;
	FVector2D GlideTarget = FVector2D::ZeroVector;
	float MenuTime = 0.0f;
	FBox2D BoundsCm = FBox2D(FVector2D(-51200.0, -51200.0), FVector2D(51200.0, 51200.0));
};
