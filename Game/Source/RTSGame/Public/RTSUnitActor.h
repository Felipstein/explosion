#pragma once

// O desenho de uma unidade. A posição, o casco e a torre vêm da simulação
// (interpolados entre dois passos); o resto é só visual: inclinar no relevo,
// recuo do tiro, a morte com a carcaça e às vezes a torre voando.
// Prédio também é unidade (o ZH: tudo é Object): a obra subindo com o andaime e a solda,
// as luzes que acendem na inauguração, o que gira (ventilador, radar), a cor do dono.

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

	/** A cor do dono nas faixas e lonas (dado de primitiva 0–3 do M_Structure). */
	void SetPlayerColor(const FLinearColor& Color);
	/** Prédio: quanto da obra está feito (0..1; 1 = pronto) e se tem energia (luzes, giro). */
	void SetStructureState(float InBuildFraction, bool bInPowered);
	/** Caminhão: quantas caixas leva. */
	void SetCargo(int32 Boxes);
	/** Dozer: trabalhando na obra (a lâmina mexe). */
	void SetWorking(bool bInWorking);
	bool IsStructure() const { return bStructure; }
	/** Altura do prédio (cm), pra a explosão e o fogo da morte. */
	float GetHeightCm() const { return HeightCm; }

	uint32 GetSimId() const { return SimId; }
	/** Névoa: some (com sombra e luz) ou volta. Só troca quando muda (recriar as peças custa). */
	void SetFogHidden(bool bHide);
	/** As marcas de dano no material (0 = inteiro, 1 = quase morto). */
	void SetDamage(float Damage);
	/** A silhueta atrás de prédio (M_UnitSilhouette): o stencil é 1 + o dono; 0 desliga (carcaça, prédio). */
	void SetSilhouette(int32 Stencil);
	bool IsFogHidden() const { return bFogHidden; }
	/** Carcaça na névoa: aparece como foi vista, sem o fogo "ao vivo" enquanto ninguém olha. */
	void SetWreckLive(bool bLive);
	/** A carcaça já foi vista pelo jogador local (a de inimigo morto na névoa só aparece quando a área é revista). */
	bool bWreckKnown = false;
	/** Quando o jogador local viu esta unidade pela última vez (segundos de mundo). */
	double LastSeenSeconds = -1.0e9;
	bool IsDead() const { return bDead; }
	float GetRadiusCm() const { return RadiusCm; }
	/** A lagarta (cm): bitola e largura; zero = não risca o chão. */
	FVector2D GetTracksCm() const { return TracksCm; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void AddParts(const TArray<FString>& Paths, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out);
	/** A mola da suspensão (só no desenho), em passos fixos de 1/30 s como no ZH. */
	void StepSuspension(float DeltaSeconds, float ForwardDeltaV, float LateralAccel);
	void Char();
	/** Troca os materiais de todas as peças pelos da obra (MI_X_Build) ou de volta. */
	void SetBuildLook(bool bBuild);
	void SetPartsFloat(int32 Index, float Value);
	void TickStructure(float DeltaSeconds);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, USceneComponent* Parent);

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
	/** O eixo da torre no casco (cm): o pivô fica ali e as malhas da torre voltam pra origem do tanque. */
	FVector TurretPivotCm = FVector::ZeroVector;
	FVector2D TracksCm = FVector2D::ZeroVector;
	float MuzzleHeightCm = 200.0f;
	float RecoilCm = 0.0f;
	FQuat TiltSmoothed = FQuat::Identity;

	bool bDead = false;
	bool bFogHidden = false;
	bool bWreckLive = true;
	float DeathAge = 0.0f;
	float DamageShown = 0.0f;
	bool bTurretFlying = false;
	FVector TurretVelocity = FVector::ZeroVector;
	/** O giro da torre voando (eixo × rad/s) e a pose; pousada, assenta em 0,3 s de pé ou de ponta-cabeça. */
	FVector TurretSpin = FVector::ZeroVector;
	FQuat TurretQ = FQuat::Identity;
	bool bTurretSettling = false;
	float TurretSettle = 0.0f;
	FQuat TurretRestQ = FQuat::Identity;

	// prédio, Dozer e caminhão
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> SpinPivots;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SpinParts;
	TArray<float> SpinRpm;
	TArray<float> SpinAngleDeg;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scaffold;
	UPROPERTY() TObjectPtr<USceneComponent> BladePivot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CargoParts;
	bool bStructure = false;
	bool bBuildLook = false;
	float BuildFraction = 1.0f;
	bool bPowered = true;
	/** A luz mostrada (0..1): pisca ao acender como lâmpada de galpão, apaga em 0,3 s. */
	float PowerShown = 1.0f;
	float PowerOnClock = 1.0f;
	float LastPowerSent = -1.0f;
	float LastCutSent = -1.0f;
	float HeightCm = 1000.0f;
	FBox2D FootprintCm = FBox2D(FVector2D(-500.0), FVector2D(500.0));
	float SparkClock = 0.0f;
	uint32 SparkSeed = 1;
	bool bWorking = false;
	float BladeClock = 0.0f;
	float BladeAmount = 0.0f;
	int32 CargoShown = -1;
	float SinkCm = 0.0f;
};
