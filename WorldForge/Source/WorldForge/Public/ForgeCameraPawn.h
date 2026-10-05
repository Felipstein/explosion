#pragma once

#include "GameFramework/Pawn.h"
#include "ForgeCameraPawn.generated.h"

class UCameraComponent;
class AWorldForgeTerrain;

/**
 * Câmera de RTS: olha um ponto do chão de cima, num ângulo fixo, e anda por
 * cima do terreno.
 *   WASD / setas / mouse na borda da tela: anda
 *   botão direito arrastando: rola o mapa como no C&C Generals (anda na
 *   direção do mouse, mais rápido quanto mais longe do ponto onde apertou)
 *   botão do meio arrastando, Q / E: gira e inclina (até a horizontal)
 *   dois cliques na roda: ângulo e rotação voltam ao início · roda: zoom
 *   Home: volta pra vista do mapa inteiro
 */
UCLASS()
class WORLDFORGE_API AForgeCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AForgeCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "WorldForge")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<AWorldForgeTerrain> Terrain;

	/** Enquadra o mapa inteiro (chamado a cada mapa novo). */
	void FrameWholeMap(bool bInstant);
	/** Põe a câmera olhando um ponto (m), mantendo distância e ângulo. */
	void LookAt(FVector2f PointM, float DistanceM, bool bInstant);
	/** Leva o ponto olhado até lá, mantendo zoom e ângulo (clique no minimapa). */
	void JumpTo(FVector2f PointM) { FocusTarget = PointM; }
	/** Volta inclinação e rotação pro padrão (suave). */
	void ResetAngles();
	/** Vista fixa usada nos testes automáticos. */
	void SetView(FVector2f FocusM, float YawDeg, float PitchDeg, float DistanceM);

	FVector2f GetFocus() const { return Focus; }
	float GetDistance() const { return Distance; }
	float GetYaw() const { return Yaw; }
	float GetPitch() const { return Pitch; }

	/** Raio do mouse na tela contra o terreno (m). */
	bool CursorGroundPoint(FVector& OutPointM) const;

	/** O painel diz se o mouse está em cima dele (aí o zoom e os cliques são dele). */
	TFunction<bool()> IsOverUI;
	/** Andar com o mouse na borda da tela (o teste automático desliga). */
	bool EdgePan = true;

	// ângulos e distâncias da câmera (graus, metros)
	static constexpr float DefaultPitch = 58.0f;   // graus abaixo do horizonte (a vista inicial)
	static constexpr float MinPitch = 0.0f;        // na horizontal
	static constexpr float MaxPitch = 85.0f;
	float Pitch = DefaultPitch, PitchTarget = DefaultPitch;
	float MinDistance = 25.0f;
	float MaxDistance = 1400.0f;

private:
	void ApplyTransform();
	float GroundAt(FVector2f P) const;
	bool ScreenRay(FVector2D Pixel, FVector& Origin, FVector& Dir) const;

	// estado alvo (o que o controle pede) e atual (suavizado)
	FVector2f Focus = FVector2f::ZeroVector, FocusTarget = FVector2f::ZeroVector;
	float Yaw = 0.0f, YawTarget = 0.0f;
	float Distance = 300.0f, DistanceTarget = 300.0f;
	float FocusZ = 0.0f;

	bool bScrolling = false;                      // botão direito: rolagem do C&C
	FVector2D ScrollAnchor = FVector2D::ZeroVector;
	bool bRotating = false;
	double MiddlePressTime = -1.0, LastMiddleClick = -1.0;
	FVector2D MiddlePressPos = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
