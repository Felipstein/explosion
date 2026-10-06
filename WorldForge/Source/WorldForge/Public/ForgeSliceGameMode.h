#pragma once

#include "GameFramework/GameModeBase.h"
#include "ForgeSliceGameMode.generated.h"

struct FForgeSliceView
{
	FString Name;
	FVector2f Focus;   // m
	float Yaw, Pitch, Distance;
};

/**
 * Modo de jogo da fatia de qualidade (mapas Slice_*, montados no editor
 * por Tools/build_slice.py): a mesma câmera de RTS, sem o gerador ao vivo
 * (o chão é o Landscape do mapa).
 *
 * Fora do teste, um botão no canto volta pro gerador (com os mesmos controles,
 * se veio do "Ver em qualidade real").
 *
 * -ForgeCapture=<pasta>: fotos em vistas fixas (alto, médio, perto, rasante,
 * contra o sol), depois um passeio de câmera quadro a quadro (motion_NNN.png,
 * vira animação no Tools/make_motion.py), tempo médio de quadro no log, e fecha.
 */
UCLASS()
class WORLDFORGE_API AForgeSliceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AForgeSliceGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void CaptureStep();

	FString CaptureFolder;
	FString ResumeOptions = TEXT("resume");   // a volta pro gerador (com o preset que ele tinha)
	TSharedPtr<class SWidget> BackButton;   // "voltar ao gerador" (fora das fotos automáticas)
	TSharedPtr<class SWidget> ShaderNote;   // aviso enquanto os materiais compilam
	TArray<FForgeSliceView> Views;
	bool bNoMotion = false;
	int32 Step = 0;
	double Wait = 2.5;
	int32 MotionFrame = -1;
	double FrameSum = 0.0;
	int32 FrameCount = 0;
	double MeasureStart = -1.0;
	bool bMeasured = false;
};
