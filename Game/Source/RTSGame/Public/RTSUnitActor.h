#pragma once

// O desenho de uma unidade. A posição, o casco e a torre vêm da simulação
// (interpolados entre dois passos); o resto é só visual: inclinar no relevo,
// recuo do tiro, a morte com a carcaça e às vezes a torre voando.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSUnitActor.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
struct FRTSUnitVisual;

UCLASS()
class RTSGAME_API ARTSUnitActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSUnitActor();

	void Setup(uint32 InSimId, const FRTSUnitVisual& Visual);
	/** Pose do quadro: Location no chão (cm), casco e torre em graus (torre no mundo), normal do chão. */
	void SetPose(const FVector& Location, float HullYawDeg, float TurretYawWorldDeg, const FVector& GroundNormal, float DeltaSeconds);

	FVector MuzzleLocation() const;
	FVector MuzzleDirection() const;
	void PlayFire();
	/** Morre: vira carcaça (e a torre pode voar). Fica no mapa. */
	void Die();

	uint32 GetSimId() const { return SimId; }
	bool IsDead() const { return bDead; }
	float GetRadiusCm() const { return RadiusCm; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void AddParts(const TArray<FString>& Paths, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out);
	/** A mola da suspensão (só no desenho), em passos fixos de 1/30 s como no ZH. */
	void StepSuspension(float DeltaSeconds, float ForwardDeltaV, float LateralAccel);
	void Char();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;          // inclina no relevo
	UPROPERTY() TObjectPtr<USceneComponent> TurretPivot;   // gira com a torre
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HullParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TurretParts;
	UPROPERTY() TObjectPtr<UPointLightComponent> FireLight;
	/** Cada roda num pivô no centro dela, pra girar no próprio eixo (as peças saem com a origem no centro do tanque). */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelPivots;
	TArray<float> WheelRadiusCm;
	float WheelAngleDeg = 0.0f;

	// suspensão: ângulo e velocidade angular (graus, graus por passo de 1/30 s)
	float SuspPitch = 0.0f;
	float SuspPitchRate = 0.0f;
	float SuspRoll = 0.0f;
	float SuspRollRate = 0.0f;
	float SuspClock = 0.0f;
	bool bHasLastPose = false;
	FVector LastLocation = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	float LastYawDeg = 0.0f;

	uint32 SimId = 0;
	float RadiusCm = 300.0f;
	float MuzzleForwardCm = 500.0f;
	float MuzzleHeightCm = 200.0f;
	float RecoilCm = 0.0f;
	FQuat TiltSmoothed = FQuat::Identity;

	bool bDead = false;
	float DeathAge = 0.0f;
	bool bTurretFlying = false;
	FVector TurretVelocity = FVector::ZeroVector;
	FVector TurretSpinDegPerS = FVector::ZeroVector;
};
