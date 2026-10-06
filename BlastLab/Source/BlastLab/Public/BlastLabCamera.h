#pragma once

#include "GameFramework/Pawn.h"
#include "BlastLabCamera.generated.h"

class UCameraComponent;

/**
 * Câmera do simulador: porte de reference/webgl/js/camera.js.
 *
 * Órbita em torno de um centro no chão (botão esquerdo arrastado), zoom pela
 * roda de 8 a 800 m (acima de 220 m o alvo sobe, pra caber a coluna nuclear),
 * rolagem pelo botão direito no estilo do C&C Generals (velocidade cresce com
 * o afastamento do cursor da âncora; LookAtXlat.cpp do código liberado pela
 * EA), um balanço leve de câmera na mão, o tranco das detonações e o tremor
 * da onda de choque nuclear. Anda no tempo REAL: pausa e câmera lenta não a
 * freiam. Estado em metros, nos eixos do WebGL convertidos pra Unreal.
 */
UCLASS()
class BLASTLAB_API ABlastLabCamera : public APawn
{
	GENERATED_BODY()

public:
	ABlastLabCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UCameraComponent> Camera;

	/** Arrasto em pixels (dy positivo = mouse pra baixo). */
	void Orbit(float DxPixels, float DyPixels);
	/** Roda: + = aproximar (um "clique" de roda = 1). */
	void Zoom(float WheelNotches);
	/** Rolagem pelo botão direito: afastamento do cursor da âncora, em meias alturas de tela (y pra baixo). */
	void ScrollRMB(float Ox, float Oy, float RealDt);
	/** Tranco curto (detonação). */
	void Kick(float Amount);
	/** Tremor longo (onda de choque). */
	void Quake(float Amplitude);

	/** Enquadramento direto (testes: a mesma câmera das referências do WebGL). */
	void SetView(float InDist, float InAzim, float InElev, float InTargetY, float InFovV)
	{
		Dist = InDist; Azim = InAzim; Elev = InElev; TargetY = InTargetY; BaseFov = InFovV;
	}

	float Distance() const { return Dist; }
	/** Fov vertical atual (rad). */
	float VerticalFov() const { return Fov; }

	virtual void Tick(float DeltaSeconds) override;

private:
	// estado (metros, radianos) — valores iniciais do WebGL
	FVector2D Center = FVector2D::ZeroVector;   // no chão: x leste, y (WebGL z)
	float Dist = 62.0f;
	float Azim = -0.62f;
	float Elev = 0.36f;
	float TargetY = 2.0f;
	float Fov = 0.58f;                          // vertical
	float BaseFov = 0.58f;
	float Shake = 0.0f;
	float QuakeAmp = 0.0f;
	float QuakeT = 0.0f;
	float T = 0.0f;
};
