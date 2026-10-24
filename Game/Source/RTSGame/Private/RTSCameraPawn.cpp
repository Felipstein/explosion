#include "RTSCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSUnitActor.h"

namespace
{
	/** O topo (cm) das unidades e prédios que cobrem o ponto (com 1,5 m de folga); sem nenhum, bem abaixo do chão. Eles não
	 *  têm colisão: vale a caixa das peças de cada um. */
	float UnitTopZCm(const UWorld* World, const FVector2D& Cm)
	{
		float Top = -1.0e9f;
		for (TActorIterator<ARTSUnitActor> It(World); It; ++It)
		{
			const FBox Box = It->GetComponentsBoundingBox(true);
			if (Box.IsValid && Cm.X >= Box.Min.X - 150.0 && Cm.X <= Box.Max.X + 150.0 && Cm.Y >= Box.Min.Y - 150.0 && Cm.Y <= Box.Max.Y + 150.0)
			{
				Top = FMath::Max(Top, float(Box.Max.Z));
			}
		}
		return Top;
	}
}

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

float ARTSCameraPawn::GetFieldOfViewDeg() const
{
	return Camera ? Camera->FieldOfView : FieldOfViewDeg;
}

void ARTSCameraPawn::JumpTo(const FVector2D& Cm)
{
	bGliding = false;
	Focus = FVector(FMath::Clamp(Cm.X, BoundsCm.Min.X, BoundsCm.Max.X), FMath::Clamp(Cm.Y, BoundsCm.Min.Y, BoundsCm.Max.Y), 0.0);
	SmoothedGroundZ = GroundZ(FVector2D(Focus.X, Focus.Y));
	UpdateCamera(0.0f);
}

void ARTSCameraPawn::AddShake(const FVector2D& AtCm, float StrengthCm)
{
	// só as explosões perto do foco (até 70 m), caindo com o quadrado da distância; a mais forte manda
	const float D = float(FVector2D::Distance(AtCm, FVector2D(Focus.X, Focus.Y))) / 100.0f;
	if (bMenuMode || D > 70.0f)
	{
		return;
	}
	const float Amp = StrengthCm * FMath::Square(1.0f - D / 70.0f);
	if (Amp > ShakeCm * FMath::Exp(-9.0f * ShakeAge))
	{
		ShakeCm = Amp;
		ShakeAge = 0.0f;
		ShakeSeed = float(int32(AtCm.X + AtCm.Y) % 997);
	}
}

void ARTSCameraPawn::SetSilhouetteColors(const TArray<FLinearColor>& Colors)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSNoSilhouette")))
	{
		return;   // o A/B
	}
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UnitSilhouette.M_UnitSilhouette"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!M)
	{
		return;   // sem o game.ps1 content: sem silhueta
	}
	if (!SilhouetteMID)
	{
		SilhouetteMID = UMaterialInstanceDynamic::Create(M, this);
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, SilhouetteMID));
		Camera->PostProcessBlendWeight = 1.0f;
	}
	for (int32 K = 0; K < Colors.Num() && K < 8; ++K)
	{
		SilhouetteMID->SetVectorParameterValue(FName(*FString::Printf(TEXT("Color%d"), K + 1)), Colors[K]);
	}
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
	const bool bAnyInput = !In.PanAxis.IsNearlyZero() || !In.DragPan.IsNearlyZero() || In.bReset || In.YawDelta != 0.0f || In.PitchDelta != 0.0f || In.ZoomSteps != 0.0f;
	if (bScenarioView)
	{
		if (!bAnyInput)
		{
			return;   // a vista da cena fica, mesmo mais perto que o zoom do jogador
		}
		bScenarioView = false;
	}
	if (In.bReset)
	{
		// volta o giro (pelo caminho mais curto), a inclinação e o zoom do padrão
		Yaw = YawShown + FMath::FindDeltaAngleDegrees(YawShown, DefaultYawDeg);
		FreePitch = 0.0f;
		TargetDistanceM = DefaultDistanceM;
	}
	Yaw += In.YawDelta;
	// o botão do meio deita por cima da curva; o total vai de quase de cima até olhar pro céu (abaixo da MinFreePitchDeg a
	// vista gira em volta do olho: a inclinação negativa é o quanto ela sobe acima do horizonte)
	const float Curve = CurvePitch(TargetDistanceM);
	FreePitch = FMath::Clamp(FreePitch + In.PitchDelta, -MaxLookUpDeg - Curve, MaxFreePitchDeg - Curve);
	// zoom em passos multiplicativos (cada clique da roda ~12%)
	// (16/10, o Felipe pelo Look) -RTSCameraHigh: só de teste, o zoom sobe até 20 km pra ver as nuvens de cima (2,5–11,5 km);
	// acima do zoom do jogo, cada clique anda 1,43× (umas 14 voltas da roda até o teto)
	static const bool bHighCamera = FParse::Param(FCommandLine::Get(), TEXT("RTSCameraHigh"));
	const float ZoomStep = bHighCamera && TargetDistanceM >= MaxDistanceM ? 0.7f : 0.88f;
	TargetDistanceM = FMath::Clamp(TargetDistanceM * FMath::Pow(ZoomStep, In.ZoomSteps), MinDistanceM, bHighCamera ? 20000.0f : MaxDistanceM);

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
	Pitch = FMath::Clamp(Curve + FreePitchShown, bMenuMode ? MinFreePitchDeg : -MaxLookUpDeg, MaxFreePitchDeg);
	const float Ground = GroundZ(FVector2D(Focus.X, Focus.Y));
	SmoothedGroundZ = DeltaSeconds > 0.0f ? FMath::FInterpTo(SmoothedGroundZ, Ground, DeltaSeconds, 4.0f) : Ground;
	// o olho orbita até a inclinação mínima; dali pra baixo ele fica e só a vista sobe (o FPS)
	const float OrbitPitch = FMath::Max(Pitch, MinFreePitchDeg);
	const FRotator View(-OrbitPitch, YawShown, 0.0f);
	const FVector Target(Focus.X, Focus.Y, SmoothedGroundZ);
	// a câmera gira em volta do ponto olhado na mesma distância: deitada, ela desce perto do chão
	// (a vista das artes); nunca abaixo do relevo embaixo dela
	FVector Eye = Target - View.Vector() * DistanceM * 100.0f;
	// (17/10 noite) e acima do topo do prédio ou do tanque embaixo dele (+2 m): de perto (10 m, "quase encostando no tanque")
	// o olho entrava na parede (a vitrine do teste via o prédio por dentro)
	const float Floor = FMath::Max(GroundZ(FVector2D(Eye.X, Eye.Y)) + 300.0f, bMenuMode ? -1.0e9f : UnitTopZCm(GetWorld(), FVector2D(Eye.X, Eye.Y)) + 200.0f);
	if (!bMenuMode && Eye.Z < Floor)
	{
		Eye.Z = Floor;
	}
	FRotator ViewRot = bMenuMode ? View : (Target - Eye).Rotation();
	if (!bMenuMode && Pitch < MinFreePitchDeg)
	{
		// a vista sobe a partir da mirada no ponto olhado (sem salto na passagem), até MaxLookUpDeg acima do horizonte
		ViewRot.Pitch = FMath::Min(ViewRot.Pitch + (MinFreePitchDeg - Pitch), MaxLookUpDeg);
	}
	// o tremor: ruído (não seno), ~25 Hz, apagando em ~0,35 s; no olho, pros lados e pra cima da vista.
	// (14/10, o "mergulho" que o Felipe e o BlastLab viram: a câmera é a raiz do ator, então o deslocamento "relativo"
	// dela era a posição no mundo: em toda explosão a vista ia pro centro do mapa, no chão e de lado, enquanto durava
	// o tremor)
	if (ShakeCm > 0.0f)
	{
		ShakeAge += DeltaSeconds;
		const float Env = FMath::Exp(-9.0f * ShakeAge);
		if (Env < 0.02f)
		{
			ShakeCm = 0.0f;
		}
		else
		{
			const float T = ShakeAge * 25.0f + ShakeSeed;
			const FRotationMatrix M(ViewRot);
			Eye += M.GetScaledAxis(EAxis::Y) * (FMath::PerlinNoise1D(T) * ShakeCm * Env) + M.GetScaledAxis(EAxis::Z) * (FMath::PerlinNoise1D(T + 37.1f) * ShakeCm * Env);
			ViewRot.Roll += FMath::PerlinNoise1D(T + 71.3f) * 0.15f * Env;
		}
	}
	SetActorLocationAndRotation(Eye, ViewRot);
	// o campo de visão: mais fechado de longe. Deitar não mexe nele (17/10, o Felipe: "tem que ser sem essa mecanica de
	// aumentar fov", liso como o WorldForge, que só gira em volta do ponto; a LowPitchFieldOfViewDeg ficou sem uso)
	const float Zoom = FMath::Clamp((DistanceM - DefaultDistanceM) / FMath::Max(MaxDistanceM - DefaultDistanceM, 1.0f), 0.0f, 1.0f);
	const float Fov = FMath::Lerp(FieldOfViewDeg, FarFieldOfViewDeg, Zoom);
	Camera->SetFieldOfView(bMenuMode ? FieldOfViewDeg : Fov);
}
