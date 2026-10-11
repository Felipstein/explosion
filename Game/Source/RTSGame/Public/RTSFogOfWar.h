#pragma once

// A névoa de guerra no desenho e o minimapa (docs/BASE_JOGAVEL.md §0).
//
// A visão vem pronta da simulação (célula a célula, do time local: 0 nunca vista, 1 já vista,
// 2 à vista). Aqui ela vira o nível da tela do Zero Hour (0 nunca visto, 0,5 já visto, 1 à
// vista; GlobalData ShroudAlpha 0 / FogAlpha 127 / ClearAlpha 255), andando 1 nível por segundo
// como o W3DShroud (preto → claro em 1 s, explorado → claro em 0,5 s). Esse nível alimenta a
// textura da passada RTSFogPass (que escurece o opaco) e o fundo do minimapa.
//
// O minimapa é uma foto de cima do mapa pronto, tirada uma vez na tela de carregar (ortográfica,
// em HDR, sem as unidades nem a névoa), com a exposição do próprio mapa aplicada aqui.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include <vector>
#include "RTSFogOfWar.generated.h"

class ASceneCapture2D;
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS()
class RTSGAME_API URTSFogOfWar : public UObject
{
	GENERATED_BODY()

public:
	/** A grade do mapa (células) e o retângulo dela no mundo (cm). */
	void Init(int32 InWidth, int32 InHeight, const FVector2D& InMinCm, const FVector2D& InSizeCm);
	/** Visão do time local (0/1/2, Y·W + X); bReveal = mostrar tudo (depuração, fim da partida). */
	void Update(const std::vector<uint8>& InVision, bool bReveal, float DeltaSeconds);

	/** Nível suavizado da tela (0..1) no ponto. */
	float LevelAt(const FVector2D& Cm) const;
	/** A célula do ponto está à vista agora (sem suavização; com bReveal, tudo está). */
	bool IsVisibleNow(const FVector2D& Cm) const;
	bool IsExplored(const FVector2D& Cm) const;
	/** A névoa "explorada" (o C&C em skirmish e multiplayer): o que nunca foi visto aparece como já
	 *  explorado, escurecido; as unidades fora da visão continuam sumindo. Só desenho. */
	void SetStartExplored(bool bOn) { bStartExplored = bOn; bHasLevels = false; }
	UTexture2D* VisionTexture() const { return Vision; }
	FVector2D MapMinCm() const { return MinCm; }
	FVector2D MapSizeCm() const { return SizeCm; }

	// --- minimapa
	/** Pede a foto de cima (sem os atores de Hidden). Leva uns quadros: chamar TickCapture até IsMinimapReady. */
	void BeginMinimapCapture(UWorld* World, const TArray<AActor*>& Hidden);
	void TickMinimapCapture();
	bool IsMinimapReady() const { return bMinimapReady; }
	bool IsMinimapCapturing() const { return CaptureStage > 0; }
	UTexture2D* MinimapTexture() const { return Minimap; }

	/** Mundo (cm) ↔ fração do minimapa (u = +Y do mundo pra direita, v = +X do mundo pra cima: v cresce pra baixo). */
	FVector2D WorldToMinimapUV(const FVector2D& Cm) const;
	FVector2D MinimapUVToWorld(const FVector2D& UV) const;

private:
	int32 CellIndex(const FVector2D& Cm) const;
	void UploadVision();
	void ComposeMinimap();
	void FinishCapture();

	UPROPERTY() TObjectPtr<UTexture2D> Vision;
	UPROPERTY() TObjectPtr<UTexture2D> Minimap;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CaptureTarget;
	UPROPERTY() TObjectPtr<ASceneCapture2D> CaptureActor;

	int32 W = 0;
	int32 H = 0;
	FVector2D MinCm = FVector2D::ZeroVector;
	FVector2D SizeCm = FVector2D(1.0, 1.0);
	TArray<float> Level;          // nível da tela por célula (0..1)
	TArray<uint8> Raw;            // 0/1/2 da simulação
	bool bRevealAll = false;
	bool bStartExplored = false;
	bool bHasLevels = false;
	bool bMinimapDirty = true;
	double LastCompose = -1.0;

	// fundo do minimapa: cor da tela (0..1, já com exposição e curva), MinimapSide² pixels
	static constexpr int32 MinimapSide = 256;
	static constexpr int32 CaptureSide = 1024;
	TArray<FLinearColor> MinimapBase;
	bool bMinimapReady = false;
	int32 CaptureStage = 0;       // 0 nada, 1 esperando o quadro do corte de distância, 2 capturado
	int32 CaptureFrames = 0;
	float SavedViewDistanceScale = 1.0f;
	float ExposureScale = 1.0f;
};
