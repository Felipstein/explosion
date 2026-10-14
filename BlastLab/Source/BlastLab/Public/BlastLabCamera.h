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

	/**
	 * O choque chegando (UBlastLabSimulation, em d/343 s): trauma que soma e
	 * satura em 1; o tremor é trauma², só de rotação, com ruído suave, e decai
	 * em ~1 s (Eiserloh, "Math for Game Programmers: Juicing Your Cameras",
	 * GDC 2016). Trauma 1 = 1° (~3% da tela, a nuclear).
	 */
	void AddTrauma(float Amount);

	/** Acessibilidade: o tremor do choque pode ser desligado (painel, -NoShake). */
	static bool bShakeEnabled;

	/**
	 * Tripé: sem a câmera na mão e sem tremor (as capturas de comparação do
	 * -AutoTest, quadro a quadro com o vídeo; -LiveCam devolve os dois).
	 */
	static bool bTripod;

	/** O ponto do chão pra onde a câmera olha (cm): o "microfone" do Zero Hour, no ponto de mira. */
	FVector AimPoint() const;

	/** Enquadramento direto (testes: a mesma câmera das referências do WebGL). */
	void SetView(float InDist, float InAzim, float InElev, float InTargetY, float InFovV)
	{
		Dist = InDist; Azim = InAzim; Elev = InElev; TargetY = InTargetY; BaseFov = InFovV;
	}
	/** O ponto do chão que a câmera olha (m, x/y da Unreal): a câmera do jogo da maquete do look olha (−6,1; −0,4). */
	void SetCenter(float X, float Y) { Center = FVector2D(X, Y); }

	float Distance() const { return Dist; }
	/** Fov vertical atual (rad). */
	float VerticalFov() const { return Fov; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// a rolagem fica dentro do terreno (m, x/y da Unreal); sem Landscape, ±500 m como no WebGL
	FVector2D MapMin = FVector2D(-500.0, -500.0);
	FVector2D MapMax = FVector2D(500.0, 500.0);
	// a altura do chão sob o centro (m), suavizada: a câmera sobe e desce com o relevo sem tremer
	float GroundM = 0.0f;
	bool bGroundReady = false;
	// estado (metros, radianos) — valores iniciais do WebGL
	FVector2D Center = FVector2D::ZeroVector;   // no chão: x leste, y (WebGL z)
	float Dist = 62.0f;
	float Azim = -0.62f;
	float Elev = 0.36f;
	float TargetY = 2.0f;
	float Fov = 0.58f;                          // vertical
	float BaseFov = 0.58f;
	float Shake = 0.0f;
	float Trauma = 0.0f;
	float QuakeAmp = 0.0f;
	float QuakeT = 0.0f;
	float T = 0.0f;

	// a névoa começando numa fração da distância ao foco, nos mapas do look (BlastLabCamera.cpp, FocusFog)
	void FocusFog(float DistCm);
	bool bFocusFogChecked = false;
	float LastFocusCm = -1.0f;
	TArray<TWeakObjectPtr<class UExponentialHeightFogComponent>> FocusFogs;
	TArray<TWeakObjectPtr<class USkyAtmosphereComponent>> FocusSkies;
};
