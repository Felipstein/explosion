#pragma once

#include "GameFramework/Actor.h"
#include "ExplosionDemo.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;

/**
 * Toca a sequência da explosão em laço quando o simulador roda (janela ou
 * Play no editor). Não faz nada nos renders do Movie Render Queue, que tocam
 * a sequência por conta própria.
 *
 * Com -DemoCapture=<pasta> na linha de comando, toca uma vez só, grava telas
 * em instantes fixos e o tempo médio de quadro no log, e fecha (é o teste
 * automático de que a janela funciona). -DemoDelay=<s> muda a espera antes
 * da detonação (padrão 2 s, pra exposição assentar).
 */
UCLASS()
class BLASTLAB_API AExplosionDemo : public AActor
{
	GENERATED_BODY()

public:
	AExplosionDemo();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<ULevelSequence> Sequence;

	/** Câmera da sequência: a vista já fica nela enquanto a exposição assenta, antes do clarão. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<AActor> Camera;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> Player;

	FString CaptureFolder;
	TArray<float> CaptureTimes = { 0.4f, 0.8f, 1.5f, 2.5f, 4.0f };
	int32 NextCapture = 0;
	double SumDt = 0.0;
	int32 NumDt = 0;
};
