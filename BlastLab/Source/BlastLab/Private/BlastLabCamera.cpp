#include "BlastLabCamera.h"

#include "BlastLabGround.h"
#include "Camera/CameraComponent.h"

namespace
{
	// reference/webgl/js/camera.js
	constexpr float RmbDead = 0.02f, RmbMin = 0.15f, RmbGain = 2.0f;   // rolagem: zona morta, mínimo e ganho (telas/s)
	constexpr float ZoomRts = 220.0f, ZoomMax = 800.0f, ZoomLift = 0.5f, LiftMax = 175.0f;
	constexpr float ZoomPerNotch = 1.127f;      // 1.0012^100: um "clique" de roda do navegador (deltaY 100)
	// o tremor do choque: trauma 1 = 1° (~3% da altura da tela, a nuclear), decai 1/s, ruído a 18 Hz
	constexpr float MaxShakeDeg = 1.0f, TraumaDecay = 1.0f, ShakeHz = 18.0f;

	// WebGL (x, y pra cima, z) em metros → Unreal em cm
	FVector ToUE(float X, float Y, float Z) { return FVector(X, Z, Y) * 100.0f; }
}

ABlastLabCamera::ABlastLabCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	// fov vertical fixo (o WebGL guarda o vertical e alarga em tela estreita)
	Camera->bConstrainAspectRatio = false;
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
}

void ABlastLabCamera::BeginPlay()
{
	Super::BeginPlay();
	FBox Terrain;
	if (BlastLabGround::Bounds(GetWorld(), Terrain))
	{
		MapMin = FVector2D(Terrain.Min.X, Terrain.Min.Y) / 100.0;
		MapMax = FVector2D(Terrain.Max.X, Terrain.Max.Y) / 100.0;
	}
}

void ABlastLabCamera::Orbit(float Dx, float Dy)
{
	Azim -= Dx * 0.005f;
	Elev = FMath::Clamp(Elev + Dy * 0.004f, 0.06f, 1.35f);
}

void ABlastLabCamera::Zoom(float Notches)
{
	Dist = FMath::Clamp(Dist * FMath::Pow(ZoomPerNotch, -Notches), 8.0f, ZoomMax);
}

void ABlastLabCamera::ScrollRMB(float Ox, float Oy, float RealDt)
{
	const float R = FMath::Sqrt(Ox * Ox + Oy * Oy);
	if (R < RmbDead || !(RealDt > 0.0f))
	{
		return;
	}
	const float View = 2.0f * Dist * FMath::Tan(Fov * 0.5f);
	const float Step = (RmbMin + RmbGain * (R - RmbDead)) * View * FMath::Min(RealDt, 0.1f) / R;
	const float Fx = -FMath::Cos(Azim), Fz = -FMath::Sin(Azim);       // frente no chão (da câmera pro alvo)
	const float Rx = -Fz, Rz = Fx;
	Center.X = FMath::Clamp(Center.X + (Rx * Ox - Fx * Oy) * Step, MapMin.X, MapMax.X);
	Center.Y = FMath::Clamp(Center.Y + (Rz * Ox - Fz * Oy) * Step, MapMin.Y, MapMax.Y);
}

void ABlastLabCamera::Kick(float Amount)
{
	if (FMath::IsFinite(Amount))
	{
		Shake = FMath::Min(FMath::Max(Shake, Amount), 1.5f);
	}
}

bool ABlastLabCamera::bShakeEnabled = true;
bool ABlastLabCamera::bTripod = false;

void ABlastLabCamera::AddTrauma(float Amount)
{
	if (FMath::IsFinite(Amount) && Amount > 0.0f)
	{
		Trauma = FMath::Min(Trauma + Amount, 1.0f);      // teto global: dez explosões juntas não sacodem o mapa (o bug do BF1)
	}
}

FVector ABlastLabCamera::AimPoint() const
{
	return FVector(Center.X * 100.0, Center.Y * 100.0, GroundM * 100.0);
}

void ABlastLabCamera::Quake(float Amp)
{
	if (!FMath::IsFinite(Amp) || Amp <= 0.0f)
	{
		return;
	}
	if (Amp > QuakeAmp)
	{
		QuakeT = 0.0f;
	}
	QuakeAmp = FMath::Min(FMath::Max(QuakeAmp, Amp), 0.05f);
}

void ABlastLabCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Clamp(DeltaSeconds, 0.0f, 0.25f) : 0.0f;
	T += Dt;

	float A = Azim, E = Elev, Ty = TargetY, F = BaseFov;
	Ty += FMath::Min(ZoomLift * FMath::Max(0.0f, Dist - ZoomRts), LiftMax);
	// o relevo: o alvo fica TargetY acima do chão sob o centro (no campo de testes, plano, 0)
	const float Ground = float(BlastLabGround::HeightAt(GetWorld(), Center.X * 100.0, Center.Y * 100.0) / 100.0);
	GroundM = bGroundReady ? FMath::Lerp(GroundM, Ground, 1.0f - FMath::Exp(-Dt / 0.25f)) : Ground;
	bGroundReady = true;
	Ty += GroundM;
	// câmera na mão: três senos incomensuráveis, amplitude de décimos de grau
	auto Hn = [this](float P, float S)
	{
		return (FMath::Sin(T * 0.73f + P) * 0.55f + FMath::Sin(T * 1.31f + P * 2.1f) * 0.3f + FMath::Sin(T * 2.17f + P * 3.7f) * 0.15f) * S;
	};
	if (!bTripod)
	{
		A += Hn(0.0f, 0.0075f);
		E += Hn(1.7f, 0.0042f);
		Ty += Hn(3.4f, 0.045f);
	}

	Shake *= FMath::Exp(-Dt * 3.1f);
	if (!FMath::IsFinite(Shake) || Shake < 1e-5f) Shake = 0.0f;
	const float Sh = FMath::Min(Shake, 1.5f);
	float Shx = 0.0f, Shy = 0.0f, Shz = 0.0f;
	if (Sh > 1e-4f)
	{
		const float W = T * 47.0f;
		Shx = (FMath::Sin(W * 1.00f) + FMath::Sin(W * 2.31f) * 0.5f) * Sh * 0.055f;
		Shy = (FMath::Sin(W * 1.37f + 2.1f) + FMath::Sin(W * 3.11f) * 0.5f) * Sh * 0.055f;
		Shz = FMath::Sin(W * 0.83f + 1.3f) * Sh * 0.03f;
		F *= 1.0f + Sh * 0.012f;
	}
	QuakeAmp *= FMath::Exp(-Dt / 0.8f);
	if (!FMath::IsFinite(QuakeAmp) || QuakeAmp < 1e-5f) QuakeAmp = 0.0f;
	if (QuakeAmp > 0.0f)
	{
		QuakeT += Dt;
		const float Q = QuakeT * PI * 2.0f;
		A += QuakeAmp * (FMath::Sin(Q * 4.3f + 1.1f) * 0.6f + FMath::Sin(Q * 9.7f + 0.3f) * 0.3f + FMath::Sin(Q * 12.1f + 2.0f) * 0.1f);
		E += QuakeAmp * (FMath::Sin(Q * 5.1f + 0.4f) * 0.6f + FMath::Sin(Q * 8.3f + 2.6f) * 0.3f + FMath::Sin(Q * 11.3f + 1.2f) * 0.1f);
		F *= 1.0f + QuakeAmp * 1.5f;
	}
	Fov = F;

	const float Ce = FMath::Cos(E), Se = FMath::Sin(E);
	const FVector Target = ToUE(Center.X + Shx * 0.4f, Ty + Shy * 0.4f, Center.Y + Shz * 0.4f);
	float Px = Center.X + Shx * 0.4f + FMath::Cos(A) * Ce * Dist + Shx;
	float Py = Ty + Shy * 0.4f + Se * Dist + Shy;
	float Pz = Center.Y + Shz * 0.4f + FMath::Sin(A) * Ce * Dist + Shz;
	Py = FMath::Max(Py, float(BlastLabGround::HeightAt(GetWorld(), Px * 100.0, Pz * 100.0) / 100.0) + 0.45f);
	const FVector Pos = ToUE(Px, Py, Pz);
	// o tremor do choque: trauma², só rotação (a câmera não sai do lugar), ruído suave
	Trauma = FMath::Max(0.0f, Trauma - Dt * TraumaDecay);
	FRotator ShakeRot = FRotator::ZeroRotator;
	if (Trauma > 0.0f && bShakeEnabled && !bTripod)
	{
		const float S = Trauma * Trauma * MaxShakeDeg;
		ShakeRot = FRotator(S * FMath::PerlinNoise1D(T * ShakeHz + 11.3f), S * FMath::PerlinNoise1D(T * ShakeHz + 47.1f),
			0.5f * S * FMath::PerlinNoise1D(T * ShakeHz + 91.7f));
	}
	SetActorLocationAndRotation(Pos, (Target - Pos).Rotation() + ShakeRot);
	// a Unreal pede o fov HORIZONTAL; com MaintainYFOV ele vale pra 16:9 e o vertical fica fixo
	Camera->SetFieldOfView(FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(F * 0.5f) * 16.0f / 9.0f)));
}
