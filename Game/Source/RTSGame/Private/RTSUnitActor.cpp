#include "RTSUnitActor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "RTSEffect.h"
#include "RTSGameData.h"

namespace
{
	UStaticMesh* LoadMesh(const FString& PackagePath)
	{
		const FString Name = FPaths::GetBaseFilename(PackagePath);
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** "…/wheel_*" vira wheel_001, wheel_002… até não existir mais. */
	TArray<FString> ExpandPattern(const FString& Path)
	{
		TArray<FString> Out;
		if (!Path.EndsWith(TEXT("*")))
		{
			Out.Add(Path);
			return Out;
		}
		const FString Prefix = Path.LeftChop(1);
		for (int32 N = 1; N < 100; ++N)
		{
			const FString Candidate = FString::Printf(TEXT("%s%03d"), *Prefix, N);
			if (!LoadMesh(Candidate))
			{
				break;
			}
			Out.Add(Candidate);
		}
		return Out;
	}

	/** Sorteio só visual, fixo por unidade (não toca a simulação). */
	uint32 CosmeticHash(uint32 Id)
	{
		uint32 X = Id * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return X;
	}

	constexpr float GravityCm = 980.0f;

	// Suspensão do CrusaderLocomotor (Locomotor.ini do ZH 1.04): a mola e o amortecimento
	// por passo de 1/30 s, o teto de inclinação e os fatores de aceleração. A mola é só no
	// desenho: o ZH também balança o tanque sem mexer na lógica (Appearance = TREADS).
	constexpr float SuspStep = 1.0f / 30.0f;
	constexpr float AccelPitchLimitDeg = 5.0f;     // AccelerationPitchLimit / DecelerationPitchLimit
	constexpr float PitchStiffness = 0.05f;
	constexpr float PitchDamping = 0.8f;
	constexpr float RollStiffness = 0.025f;
	constexpr float RollDamping = 0.3f;
	constexpr float ForwardAccelPitchFactor = 0.3f;
	constexpr float LateralAccelRollFactor = 0.33f;
	/** Graus de impulso por m/s de velocidade ganha num passo (a escala é nossa: o fator é do ZH). */
	constexpr float ImpulseDegPerMps = 1.0f;
	/** O tranco do tiro: o canhão empurra o casco pra trás (graus por passo). */
	constexpr float FireKickDeg = 1.2f;
	constexpr float WreckFireSeconds = 30.0f;
}

ARTSUnitActor::ARTSUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(Body);
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Body);
	FireLight->SetRelativeLocation(FVector(0, 0, 180));
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetIntensity(0.0f);
	FireLight->SetAttenuationRadius(1600.0f);
	FireLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.15f));
	FireLight->SetCastShadows(false);
	FireLight->SetVisibility(false);
}

void ARTSUnitActor::AddParts(const TArray<FString>& Paths, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out)
{
	for (const FString& Pattern : Paths)
	{
		for (const FString& Path : ExpandPattern(Pattern))
		{
			UStaticMesh* Mesh = LoadMesh(Path);
			if (!Mesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS: malha não encontrada: %s"), *Path);
				continue;
			}
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
			USceneComponent* Attach = Parent;
			if (Mesh->GetName().StartsWith(TEXT("wheel")))
			{
				// a roda gira no centro dela: um pivô ali, e a malha deslocada de volta
				const FBox Box = Mesh->GetBoundingBox();
				USceneComponent* Pivot = NewObject<USceneComponent>(this);
				Pivot->SetupAttachment(Parent);
				Pivot->SetRelativeLocation(Box.GetCenter());
				Pivot->RegisterComponent();
				Part->SetRelativeLocation(-Box.GetCenter());
				Attach = Pivot;
				WheelPivots.Add(Pivot);
				WheelRadiusCm.Add(FMath::Max(Box.GetExtent().Z, 10.0f));
			}
			Part->SetupAttachment(Attach);
			Part->SetStaticMesh(Mesh);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetCanEverAffectNavigation(false);
			Part->RegisterComponent();
			Out.Add(Part);
		}
	}
}

void ARTSUnitActor::Setup(uint32 InSimId, const FRTSUnitVisual& Visual)
{
	SimId = InSimId;
	RadiusCm = Visual.RadiusM * 100.0f;
	MuzzleHeightCm = Visual.MuzzleHeightM * 100.0f;
	AddParts(Visual.HullMeshes, Body, HullParts);
	AddParts(Visual.TurretMeshes, TurretPivot, TurretParts);
	// a boca do cano: a ponta da malha do cano (as peças saem no lugar, com origem no centro do tanque)
	for (const UStaticMeshComponent* Part : TurretParts)
	{
		if (Part->GetStaticMesh() && Part->GetStaticMesh()->GetName().Contains(TEXT("Barrel")))
		{
			MuzzleForwardCm = Part->GetStaticMesh()->GetBoundingBox().Max.X;
		}
	}
}

void ARTSUnitActor::SetPose(const FVector& Location, float HullYawDeg, float TurretYawWorldDeg, const FVector& GroundNormal, float DeltaSeconds)
{
	if (bDead)
	{
		return;
	}
	// velocidade e aceleração do casco (do desenho interpolado), pra rodas e suspensão
	const FVector Forward = FRotator(0.0f, HullYawDeg, 0.0f).Vector();
	const FVector Right = FRotator(0.0f, HullYawDeg + 90.0f, 0.0f).Vector();
	float ForwardAccel = 0.0f;
	float LateralAccel = 0.0f;
	if (bHasLastPose && DeltaSeconds > 1e-4f)
	{
		const FVector Velocity = (Location - LastLocation) / DeltaSeconds;
		ForwardAccel = float(FVector::DotProduct(Velocity - LastVelocity, Forward)) / 100.0f;   // m/s ganhos neste quadro
		// na curva, o peso vai pra fora: aceleração centrípeta = v · ω
		const float Speed = float(FVector::DotProduct(Velocity, Forward)) / 100.0f;
		LateralAccel = Speed * FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(LastYawDeg, HullYawDeg) / DeltaSeconds);
		LastVelocity = Velocity;
		// rodas: giram pela distância andada no chão
		const float Travel = float(FVector::DotProduct(Location - LastLocation, Forward));
		if (WheelPivots.Num())
		{
			WheelAngleDeg = FMath::Fmod(WheelAngleDeg + FMath::RadiansToDegrees(Travel / WheelRadiusCm[0]), 360.0f);
			for (int32 K = 0; K < WheelPivots.Num(); ++K)
			{
				const float Scale = WheelRadiusCm[0] / WheelRadiusCm[K];
				WheelPivots[K]->SetRelativeRotation(FRotator(-WheelAngleDeg * Scale, 0.0f, 0.0f));
			}
		}
	}
	LastLocation = Location;
	LastYawDeg = HullYawDeg;
	bHasLastPose = true;
	StepSuspension(DeltaSeconds, ForwardAccel, LateralAccel);

	SetActorLocation(Location);
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal);
	TiltSmoothed = FQuat::Slerp(TiltSmoothed, Tilt, 1.0f - FMath::Exp(-DeltaSeconds * 8.0f));
	// arrancou: o nariz sobe (pitch +); freou: desce; na curva, rola pra fora
	const FQuat Yaw = FRotator(0.0f, HullYawDeg, 0.0f).Quaternion();
	const FQuat Sway = FRotator(SuspPitch, 0.0f, SuspRoll).Quaternion();
	Body->SetWorldRotation(TiltSmoothed * Yaw * Sway);
	TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg), 0.0f));
	TurretPivot->SetRelativeLocation(FVector(-RecoilCm, 0.0f, 0.0f));
}

FVector ARTSUnitActor::MuzzleLocation() const
{
	return TurretPivot->GetComponentTransform().TransformPosition(FVector(MuzzleForwardCm, 0.0f, MuzzleHeightCm));
}

FVector ARTSUnitActor::MuzzleDirection() const
{
	return TurretPivot->GetForwardVector();
}

void ARTSUnitActor::StepSuspension(float DeltaSeconds, float ForwardAccel, float LateralAccel)
{
	// a mola do ZH roda em passos de 1/30 s: ângulo-alvo pela aceleração, depois
	// velocidade = velocidade·amortecimento + (alvo − ângulo)·rigidez
	SuspClock += DeltaSeconds;
	// arrancada e freada são instantâneas (Acceleration 1000 no ZH): viram impulso na mola
	// (o nariz sobe ao arrancar, desce ao frear); a curva é contínua: vira ângulo-alvo de rolagem
	SuspPitchRate += FMath::Clamp(ForwardAccel * ForwardAccelPitchFactor * ImpulseDegPerMps, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	const float PitchTarget = 0.0f;
	const float RollTarget = FMath::Clamp(LateralAccel * LateralAccelRollFactor * 0.1f, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	int32 Steps = 0;
	while (SuspClock >= SuspStep && Steps < 4)
	{
		SuspClock -= SuspStep;
		++Steps;
		SuspPitchRate = SuspPitchRate * PitchDamping + (PitchTarget - SuspPitch) * PitchStiffness;
		SuspRollRate = SuspRollRate * RollDamping + (RollTarget - SuspRoll) * RollStiffness;
		SuspPitch = FMath::Clamp(SuspPitch + SuspPitchRate, -AccelPitchLimitDeg, AccelPitchLimitDeg);
		SuspRoll = FMath::Clamp(SuspRoll + SuspRollRate, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	}
	if (Steps == 4)
	{
		SuspClock = 0.0f;
	}
}

void ARTSUnitActor::PlayFire()
{
	RecoilCm = 30.0f;
	// o tranco do tiro: o canhão empurra o casco pra trás, pro lado de onde a torre aponta
	const float Rel = FMath::DegreesToRadians(TurretPivot->GetRelativeRotation().Yaw);
	SuspPitchRate += FireKickDeg * FMath::Cos(Rel);
	SuspRollRate -= FireKickDeg * FMath::Sin(Rel);
	ARTSEffect::SpawnFlash(GetWorld(), MuzzleLocation() + MuzzleDirection() * 60.0f, 90.0f, 1.5e6f, 0.09f, FLinearColor(3.0f, 2.0f, 0.9f));
}

void ARTSUnitActor::SetFogHidden(bool bHide)
{
	if (bHide != bFogHidden)
	{
		bFogHidden = bHide;
		SetActorHiddenInGame(bHide);
	}
}

void ARTSUnitActor::SetWreckLive(bool bLive)
{
	if (bLive != bWreckLive)
	{
		bWreckLive = bLive;
		if (bDead)
		{
			FireLight->SetVisibility(bLive);
		}
	}
}

void ARTSUnitActor::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathAge = 0.0f;
	Char();
	// o clarão e a bola são da explosão (BlastFx, a Carga Pesada aprovada); aqui só a carcaça
	FireLight->SetVisibility(bWreckLive);
	// a torre voa em ~1 de cada 3 mortes (munição detonando), não em toda (Vanguard ADR-0027)
	const uint32 H = CosmeticHash(SimId);
	if (H % 3 == 0)
	{
		bTurretFlying = true;
		const float Angle = float(H % 360);
		TurretVelocity = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * 150.0f, FMath::Sin(FMath::DegreesToRadians(Angle)) * 150.0f, 750.0f + float(H % 200));
		TurretSpinDegPerS = FVector(float(H % 120) - 60.0f, 180.0f + float(H % 140), float(H % 90) - 45.0f);
		TurretPivot->SetUsingAbsoluteLocation(true);
		TurretPivot->SetUsingAbsoluteRotation(true);
	}
}

void ARTSUnitActor::Char()
{
	// carcaça queimada: escurece a cor e tira o brilho (parâmetros do M_UnitPBR)
	auto Burn = [](UStaticMeshComponent* Part)
	{
		for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(Slot))
			{
				MID->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(0.07f, 0.064f, 0.058f, 1.0f));
				MID->SetScalarParameterValue(TEXT("RoughnessFactor"), 0.95f);
				MID->SetScalarParameterValue(TEXT("MetallicFactor"), 0.0f);
			}
		}
	};
	for (UStaticMeshComponent* Part : HullParts)
	{
		Burn(Part);
	}
	for (UStaticMeshComponent* Part : TurretParts)
	{
		Burn(Part);
	}
}

void ARTSUnitActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RecoilCm = FMath::Max(0.0f, RecoilCm - DeltaSeconds * 120.0f);
	if (!bDead)
	{
		return;
	}
	DeathAge += DeltaSeconds;
	if (bTurretFlying)
	{
		TurretVelocity.Z -= GravityCm * DeltaSeconds;
		const FVector P = TurretPivot->GetComponentLocation() + TurretVelocity * DeltaSeconds;
		TurretPivot->SetWorldLocationAndRotation(P, TurretPivot->GetComponentRotation() + FRotator::MakeFromEuler(TurretSpinDegPerS * DeltaSeconds));
		if (P.Z < GetActorLocation().Z + 60.0f && TurretVelocity.Z < 0.0f)
		{
			bTurretFlying = false;   // caiu
		}
	}
	// o fogo da carcaça tremula e se apaga
	const float Fade = FMath::Clamp(1.0f - DeathAge / WreckFireSeconds, 0.0f, 1.0f);
	const float Flicker = 0.75f + 0.18f * FMath::Sin(DeathAge * 13.0f) + 0.1f * FMath::Sin(DeathAge * 31.0f + 1.3f);
	FireLight->SetIntensity(4.0e5f * Fade * Flicker);
	if (Fade <= 0.0f)
	{
		FireLight->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}
