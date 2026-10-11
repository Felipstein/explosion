#include "RTSCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"

ARTSCameraPawn::ARTSCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->bConstrainAspectRatio = false;
	SetActorEnableCollision(false);
}

float ARTSCameraPawn::GroundZ(const FVector2D& Cm) const
{
	if (const URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>())
	{
		if (const FRTSBakedMap* Map = Match->BakedMap())
		{
			return Map->GroundZCm(Cm);
		}
	}
	// no menu ainda não há grade: raio de cima pra baixo
	FHitResult Hit;
	const FVector From(Cm.X, Cm.Y, 100000.0);
	const FVector To(Cm.X, Cm.Y, -100000.0);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility))
	{
		return float(Hit.ImpactPoint.Z);
	}
	return 0.0f;
}

void ARTSCameraPawn::FocusOn(const FVector2D& Cm)
{
	Focus = FVector(Cm.X, Cm.Y, 0.0);
	SmoothedGroundZ = GroundZ(Cm);
	Yaw = YawShown = DefaultYawDeg;
	FreePitch = FreePitchShown = 0.0f;
	DistanceM = TargetDistanceM = DefaultDistanceM;
	UpdateCamera(0.0f);
}

float ARTSCameraPawn::CurvePitch(float D) const
{
	// dois trechos suaves passando pelo padrão (o ZH): perto → padrão → longe
	if (D <= DefaultDistanceM)
	{
		const float T = FMath::Clamp((D - MinDistanceM) / FMath::Max(DefaultDistanceM - MinDistanceM, 1.0f), 0.0f, 1.0f);
		return FMath::Lerp(NearPitchDeg, DefaultPitchDeg, FMath::SmoothStep(0.0f, 1.0f, T));
	}
	const float T = FMath::Clamp((D - DefaultDistanceM) / FMath::Max(MaxDistanceM - DefaultDistanceM, 1.0f), 0.0f, 1.0f);
	return FMath::Lerp(DefaultPitchDeg, FarPitchDeg, FMath::SmoothStep(0.0f, 1.0f, T));
}

void ARTSCameraPawn::JumpTo(const FVector2D& Cm)
{
	bGliding = false;
	Focus = FVector(FMath::Clamp(Cm.X, BoundsCm.Min.X, BoundsCm.Max.X), FMath::Clamp(Cm.Y, BoundsCm.Min.Y, BoundsCm.Max.Y), 0.0);
	SmoothedGroundZ = GroundZ(FVector2D(Focus.X, Focus.Y));
	UpdateCamera(0.0f);
}

void ARTSCameraPawn::SetFogExposureFix(bool bOn)
{
	FPostProcessSettings& P = Camera->PostProcessSettings;
	P.bOverride_LocalExposureBlurredLuminanceBlend = bOn;
	float Blend = 0.0f;
	// A/B com o visual aprovado: -RTSLEBlend=0.6 volta o padrão da Unreal
	FParse::Value(FCommandLine::Get(), TEXT("-RTSLEBlend="), Blend);
	P.LocalExposureBlurredLuminanceBlend = Blend;
	Camera->PostProcessBlendWeight = 1.0f;
}

void ARTSCameraPawn::ApplyInput(const FInput& In, float DeltaSeconds)
{
	if (bMenuMode)
	{
		return;
	}
	if (!In.PanAxis.IsNearlyZero() || !In.DragPan.IsNearlyZero())
	{
		bGliding = false;   // o jogador pegou a câmera
	}
	if (In.bReset)
	{
		// volta o giro (pelo caminho mais curto), a inclinação e o zoom do padrão
		Yaw = YawShown + FMath::FindDeltaAngleDegrees(YawShown, DefaultYawDeg);
		FreePitch = 0.0f;
		TargetDistanceM = DefaultDistanceM;
	}
	Yaw += In.YawDelta;
	// o botão do meio deita por cima da curva; o total fica entre quase horizontal e quase de cima
	const float Curve = CurvePitch(TargetDistanceM);
	FreePitch = FMath::Clamp(FreePitch + In.PitchDelta, MinFreePitchDeg - Curve, MaxFreePitchDeg - Curve);
	// zoom em passos multiplicativos (cada clique da roda ~12%)
	TargetDistanceM = FMath::Clamp(TargetDistanceM * FMath::Pow(0.88f, In.ZoomSteps), MinDistanceM, MaxDistanceM);

	// a rolagem segue a vista: "frente" é pra onde a câmera olha, no chão
	const FVector Forward = FRotator(0.0f, Yaw, 0.0f).Vector();
	const FVector Right = FRotator(0.0f, Yaw + 90.0f, 0.0f).Vector();
	const float VisibleWidthCm = 2.0f * DistanceM * 100.0f * FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
	const FVector2D Axis = In.PanAxis + In.DragPan;
	const FVector Move = (Right * Axis.X + Forward * Axis.Y) * VisibleWidthCm * PanScreensPerSecond * DeltaSeconds;
	Focus += FVector(Move.X, Move.Y, 0.0);
	Focus.X = FMath::Clamp(Focus.X, BoundsCm.Min.X, BoundsCm.Max.X);
	Focus.Y = FMath::Clamp(Focus.Y, BoundsCm.Min.Y, BoundsCm.Max.Y);
}

void ARTSCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMenuMode)
	{
		// o fundo do menu: passeio lento e alto sobre o mapa, girando
		MenuTime += DeltaSeconds;
		const float Angle = MenuTime * 2.0f;
		Yaw = YawShown = FRotator::NormalizeAxis(DefaultYawDeg + Angle);
		FreePitch = FreePitchShown = 0.0f;
		TargetDistanceM = DistanceM = 95.0f / FMath::Sin(FMath::DegreesToRadians(24.0f));
		const FVector2D Orbit = FVector2D(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle))) * 9000.0;
		Focus = FVector(Orbit.X, Orbit.Y, 0.0);
	}
	else if (bGliding)
	{
		const FVector2D Now = FMath::Vector2DInterpTo(FVector2D(Focus.X, Focus.Y), GlideTarget, DeltaSeconds, 1.2f);
		Focus = FVector(Now.X, Now.Y, 0.0);
	}
	UpdateCamera(DeltaSeconds);
}

void ARTSCameraPawn::UpdateCamera(float DeltaSeconds)
{
	DistanceM = DeltaSeconds > 0.0f ? FMath::FInterpTo(DistanceM, TargetDistanceM, DeltaSeconds, 10.0f) : TargetDistanceM;
	// o giro e o deitar suaves, como o WorldForge (exponencial, independente da taxa de quadros)
	const float K = DeltaSeconds > 0.0f ? 1.0f - FMath::Exp(-12.0f * DeltaSeconds) : 1.0f;
	YawShown += (Yaw - YawShown) * K;
	FreePitchShown += (FreePitch - FreePitchShown) * K;
	const float Curve = bMenuMode ? 24.0f : CurvePitch(DistanceM);
	Pitch = FMath::Clamp(Curve + FreePitchShown, MinFreePitchDeg, MaxFreePitchDeg);
	const float Ground = GroundZ(FVector2D(Focus.X, Focus.Y));
	SmoothedGroundZ = DeltaSeconds > 0.0f ? FMath::FInterpTo(SmoothedGroundZ, Ground, DeltaSeconds, 4.0f) : Ground;
	const FRotator View(-Pitch, YawShown, 0.0f);
	const FVector Target(Focus.X, Focus.Y, SmoothedGroundZ);
	// a câmera gira em volta do ponto olhado na mesma distância: deitada, ela desce perto do chão
	// (a vista das artes); nunca abaixo do relevo embaixo dela
	FVector Eye = Target - View.Vector() * DistanceM * 100.0f;
	const float Floor = GroundZ(FVector2D(Eye.X, Eye.Y)) + 300.0f;
	if (!bMenuMode && Eye.Z < Floor)
	{
		Eye.Z = Floor;
	}
	SetActorLocationAndRotation(Eye, bMenuMode ? View : (Target - Eye).Rotation());
	// o campo de visão: mais fechado de longe, mais aberto com a câmera deitada
	const float Zoom = FMath::Clamp((DistanceM - DefaultDistanceM) / FMath::Max(MaxDistanceM - DefaultDistanceM, 1.0f), 0.0f, 1.0f);
	const float Low = FMath::Clamp((25.0f - Pitch) / 20.0f, 0.0f, 1.0f);
	const float Fov = FMath::Lerp(FMath::Lerp(FieldOfViewDeg, FarFieldOfViewDeg, Zoom), LowPitchFieldOfViewDeg, FMath::SmoothStep(0.0f, 1.0f, Low));
	Camera->SetFieldOfView(bMenuMode ? FieldOfViewDeg : Fov);
}
