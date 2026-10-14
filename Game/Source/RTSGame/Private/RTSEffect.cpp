#include "RTSEffect.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

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

	// o traçante (12/10, o defeito 5 da 1ª gameplay: "traçante branco comprido"; BASE_JOGAVEL: curto e
	// laranja, espessura constante na tela, nasce à frente da boca, some no impacto, aditivo, como o Tracer
	// do ZH com Speed/Length/Width): 2,5 m de risco a ~105 m/s são 24 ms de voo, a leitura do tiro de canhão
	// nos jogos de tanque (War Thunder, Battlefield); a espessura sai da distância da câmera (~2,5 px a
	// 5 cm por pixel) e a cor só passa do branco da exposição no vermelho (não estoura pra branco)
	constexpr float TracerLengthCm = 250.0f;
	constexpr float TracerThicknessCm = 14.0f;
	constexpr float TracerThicknessPerCm = 0.0013f;   // espessura (cm) por cm de distância da câmera
	constexpr float TracerMuzzleGapCm = 150.0f;       // nasce à frente da boca (o clarão do tiro é da sessão de explosões)
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
	const float Span = FVector::Dist(InFrom, InTo);
	const FVector Start = InFrom + Dir * FMath::Min(TracerMuzzleGapCm, Span * 0.5f);
	ARTSEffect* E = World->SpawnActor<ARTSEffect>(Start, FRotationMatrix::MakeFromZ(Dir).Rotator());
	if (!E)
	{
		return;
	}
	E->Kind = EKind::Tracer;
	E->From = Start;
	E->To = InTo;
	E->Speed = SpeedCmPerS;
	E->Length = TracerLengthCm;
	E->Thickness = -1.0f;   // pela câmera, a cada quadro
	E->Life = FVector::Dist(Start, InTo) / SpeedCmPerS;
	// na exposição do look (EV ~13,7, o branco perto de 10 mil nits) só o vermelho passa do branco: fica
	// laranja; com 0,9/0,32/0,07 (115/41/9 mil nits) os três passavam e o tonemapper desbotava pra creme (vila_tiro1)
	E->Radiance = FLinearColor(0.16f, 0.05f, 0.009f);
	E->Mesh->SetStaticMesh(Cylinder());
	E->Mesh->SetMaterial(0, GlowMaterial());   // aditivo, o miolo forte e a borda macia
	// o cilindro da engine tem 100 cm de altura e 100 de diâmetro, em pé no eixo Z
	E->Mesh->SetWorldScale3D(FVector(TracerThicknessCm / 100.0f, TracerThicknessCm / 100.0f, TracerLengthCm / 100.0f));
	SetRadiance(E->Mesh, E->Radiance);
	E->Light->SetLightColor(FLinearColor(1.0f, 0.5f, 0.18f));
	E->Light->SetAttenuationRadius(600.0f);
	E->Light->SetIntensity(6.0e4f);
}

void ARTSEffect::SpawnStreak(UWorld* World, const FVector& InFrom, const FVector& InTo, float SpeedCmPerS, float LengthCm, float ThicknessCm, const FLinearColor& InRadiance)
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
	E->Length = LengthCm;
	E->Life = FVector::Dist(InFrom, InTo) / SpeedCmPerS;
	E->Radiance = InRadiance;
	E->Mesh->SetStaticMesh(Cylinder());
	E->Mesh->SetMaterial(0, EmissiveMaterial());
	E->Mesh->SetWorldScale3D(FVector(ThicknessCm / 100.0f, ThicknessCm / 100.0f, LengthCm / 100.0f));
	SetRadiance(E->Mesh, InRadiance);
	E->Light->SetVisibility(false);
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
	// traçante: anda até o alvo; o meio do risco fica meio comprimento atrás da ponta (no começo, o risco
	// ainda está saindo: não passa pra trás do ponto de nascer)
	const FVector Dir = (To - From).GetSafeNormal();
	const float Travel = FMath::Min(Speed * Age, float(FVector::Dist(From, To)));
	const float Shown = Thickness < 0.0f ? FMath::Min(Length, FMath::Max(Travel, 1.0f)) : Length;   // os riscos de antes (SpawnStreak) seguem iguais
	const FVector Tip = From + Dir * Travel;
	SetActorLocation(Tip - Dir * (Shown * 0.5f));
	if (Thickness < 0.0f)
	{
		// espessura constante na tela: cresce com a distância da câmera
		float Thick = TracerThicknessCm;
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				Thick = FMath::Clamp(float(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Tip)) * TracerThicknessPerCm, 6.0f, 30.0f);
			}
		}
		Mesh->SetWorldScale3D(FVector(Thick / 100.0f, Thick / 100.0f, Shown / 100.0f));
	}
}
