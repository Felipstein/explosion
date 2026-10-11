#pragma once

// Efeitos curtos do primeiro passo, feitos com malha emissiva + luz pontual:
// clarão de tiro, traçante (curto e evidente, nunca raio laser contínuo), impacto
// e o clarão da morte. A explosão de verdade do BlastLab entra no passo seguinte.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSEffect.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class RTSGAME_API ARTSEffect : public AActor
{
	GENERATED_BODY()

public:
	ARTSEffect();

	/** Traçante do projétil, de From até To na velocidade dada (cm/s). */
	static void SpawnTracer(UWorld* World, const FVector& From, const FVector& To, float SpeedCmPerS);
	/** Risco curto e fino (a faísca da solda): o traçante com comprimento, espessura e cor próprios, sem luz. */
	static void SpawnStreak(UWorld* World, const FVector& From, const FVector& To, float SpeedCmPerS, float LengthCm, float ThicknessCm, const FLinearColor& Radiance);
	/** Clarão: bola emissiva que cresce e some + luz. RadiusCm do brilho, CandelaPeak da luz. */
	static void SpawnFlash(UWorld* World, const FVector& At, float RadiusCm, float CandelaPeak, float LifeSeconds, const FLinearColor& Radiance);

	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EKind : uint8 { Flash, Tracer };

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;

	EKind Kind = EKind::Flash;
	float Age = 0.0f;
	float Life = 0.1f;
	float Radius = 100.0f;
	float Candela = 0.0f;
	FLinearColor Radiance = FLinearColor::White;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Speed = 10000.0f;
	float Length = 600.0f;
};
