#include "RTSEffect.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// M_Glow: aditivo, macio na borda; M_Emissive: sólido. Radiância por primitiva
	// (índices 0–2), × 128000 cd/m² (Game/Tools/build_content.py, como no BlastLab).
	UMaterialInterface* GlowMaterial() { return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glow.M_Glow")); }
	UMaterialInterface* EmissiveMaterial() { return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Emissive.M_Emissive")); }
	UStaticMesh* Sphere() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
	UStaticMesh* Cylinder() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }

	void SetRadiance(UStaticMeshComponent* Mesh, const FLinearColor& R)
	{
		Mesh->SetCustomPrimitiveDataVector3(0, FVector(R.R, R.G, R.B));
	}

	constexpr float TracerLengthCm = 600.0f;
	constexpr float TracerThicknessCm = 14.0f;
}

ARTSEffect::ARTSEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCanEverAffectNavigation(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetCastShadows(false);
	Light->SetUsingAbsoluteScale(true);
}

void ARTSEffect::SpawnFlash(UWorld* World, const FVector& At, float RadiusCm, float CandelaPeak, float LifeSeconds, const FLinearColor& InRadiance)
{
	if (!World)
	{
		return;
	}
	ARTSEffect* E = World->SpawnActor<ARTSEffect>(At, FRotator::ZeroRotator);
	if (!E)
	{
		return;
	}
	E->Kind = EKind::Flash;
	E->Life = LifeSeconds;
	E->Radius = RadiusCm;
	E->Candela = CandelaPeak;
	E->Radiance = InRadiance;
	E->Mesh->SetStaticMesh(Sphere());
	E->Mesh->SetMaterial(0, GlowMaterial());
	E->Mesh->SetWorldScale3D(FVector(RadiusCm / 50.0f));
	SetRadiance(E->Mesh, InRadiance);
	E->Light->SetLightColor(FLinearColor(1.0f, 0.62f, 0.3f));
	E->Light->SetAttenuationRadius(RadiusCm * 12.0f);
	E->Light->SetIntensity(CandelaPeak);
}

void ARTSEffect::SpawnTracer(UWorld* World, const FVector& InFrom, const FVector& InTo, float SpeedCmPerS)
{
	if (!World || SpeedCmPerS <= 0.0f)
	{
		return;
	}
	const FVector Dir = (InTo - InFrom).GetSafeNormal();
	ARTSEffect* E = World->SpawnActor<ARTSEffect>(InFrom, FRotationMatrix::MakeFromZ(Dir).Rotator());
	if (!E)
	{
		return;
	}
	E->Kind = EKind::Tracer;
	E->From = InFrom;
	E->To = InTo;
	E->Speed = SpeedCmPerS;
	E->Life = FVector::Dist(InFrom, InTo) / SpeedCmPerS;
	E->Radiance = FLinearColor(4.0f, 2.3f, 0.8f);
	E->Mesh->SetStaticMesh(Cylinder());
	E->Mesh->SetMaterial(0, EmissiveMaterial());
	// o cilindro da engine tem 100 cm de altura e 100 de diâmetro, em pé no eixo Z
	E->Mesh->SetWorldScale3D(FVector(TracerThicknessCm / 100.0f, TracerThicknessCm / 100.0f, TracerLengthCm / 100.0f));
	SetRadiance(E->Mesh, E->Radiance);
	E->Light->SetLightColor(FLinearColor(1.0f, 0.6f, 0.25f));
	E->Light->SetAttenuationRadius(700.0f);
	E->Light->SetIntensity(2.0e5f);
}

void ARTSEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Life)
	{
		Destroy();
		return;
	}
	const float T = Age / Life;
	if (Kind == EKind::Flash)
	{
		const float Fade = (1.0f - T) * (1.0f - T);
		Mesh->SetWorldScale3D(FVector(Radius / 50.0f * (0.6f + 0.4f * T)));
		SetRadiance(Mesh, Radiance * Fade);
		Light->SetIntensity(Candela * Fade);
		return;
	}
	// traçante: anda até o alvo; o meio do risco fica meio comprimento atrás da ponta
	const FVector Dir = (To - From).GetSafeNormal();
	const FVector Tip = From + Dir * FMath::Min(Speed * Age, FVector::Dist(From, To));
	SetActorLocation(Tip - Dir * (TracerLengthCm * 0.5f));
}
