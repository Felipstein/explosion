#include "BlastEffects.h"

#include "BlastLabSimulation.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr float G = 9.81f;
	// os pedaços chamuscados: a cor de M_Base (rgb + rugosidade)
	const FLinearColor DebrisColor(0.045f, 0.040f, 0.036f, 0.85f);
	// a faísca: fragmento a ~1800 K (cor do corpo negro, unidade do render)
	const FLinearColor SparkColor(1.0f, 0.42f, 0.10f, 0.0f);
	constexpr float SparkRadiance = 10.0f;
	// a faísca se vê como um risco: o caminho de 1/30 s (o olho integra uns 30 ms)
	constexpr float StreakSeconds = 1.0f / 30.0f;
	// o plano do anel vai um pouco além do raio do dano (a frente tem largura)
	constexpr float RingPlaneMargin = 1.15f;
	// o decal projeta pra baixo até essa profundidade (cm)
	constexpr float ScorchDepthCm = 300.0f;

	UStaticMesh* Mesh(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); }
	UMaterialInterface* Material(const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); }

	void NoCollision(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
	}

	void SetColor(UPrimitiveComponent* C, const FLinearColor& Color, int32 First = 0)
	{
		C->SetCustomPrimitiveDataFloat(First + 0, Color.R);
		C->SetCustomPrimitiveDataFloat(First + 1, Color.G);
		C->SetCustomPrimitiveDataFloat(First + 2, Color.B);
		C->SetCustomPrimitiveDataFloat(First + 3, Color.A);
	}
}

ABlastEffects::ABlastEffects()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	Flash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	DebrisMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Debris"));
	ClodMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Clods"));
	SparkMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));
	for (UPrimitiveComponent* C : { (UPrimitiveComponent*)Flash, (UPrimitiveComponent*)Ring, (UPrimitiveComponent*)DebrisMesh,
		(UPrimitiveComponent*)ClodMesh, (UPrimitiveComponent*)SparkMesh })
	{
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		NoCollision(C);
	}
	// o que brilha não faz sombra
	Flash->SetCastShadow(false);
	Ring->SetCastShadow(false);
	SparkMesh->SetCastShadow(false);
	Flash->SetVisibility(false);
	Ring->SetVisibility(false);

	Scorch = CreateDefaultSubobject<UDecalComponent>(TEXT("Scorch"));
	Scorch->SetupAttachment(RootComponent);
	Scorch->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));      // projeta pra baixo
	Scorch->SetVisibility(false);
}

float ABlastEffects::MinSizeM(const FVector& PM) const
{
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	if (!Sim)
	{
		return 0.0f;
	}
	const float DistM = float(FVector::Dist(PM * 100.0, Sim->ViewPoint())) / 100.0f;
	return Recipe.MinPixels * DistM * Sim->RadiansPerPixel();
}

void ABlastEffects::Emit(TArray<FPiece>& Out, int32 Count, const FBlastRange& Speed, const FBlastRange& Elev,
	const FBlastRange& Size, const FVector& SizeShape, float Up)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FPiece P;
		const float Az = Rand.FRandRange(0.0f, 2.0f * PI);
		const float El = FMath::DegreesToRadians(Elev.Sample(Rand));
		const FVector Dir(FMath::Cos(El) * FMath::Cos(Az), FMath::Cos(El) * FMath::Sin(Az), FMath::Sin(El));
		P.V = Dir * Speed.Sample(Rand);
		P.P = GroundM + FVector(0.0f, 0.0f, Up) + FVector(Dir.X, Dir.Y, 0.0f) * Rand.FRandRange(0.0f, 0.6f);
		const float S = Size.Sample(Rand);
		P.Size = S * SizeShape * FVector(Rand.FRandRange(0.6f, 1.4f), Rand.FRandRange(0.6f, 1.4f), Rand.FRandRange(0.5f, 1.0f));
		P.Q = FQuat(FRotator(Rand.FRandRange(0.0f, 360.0f), Rand.FRandRange(0.0f, 360.0f), Rand.FRandRange(0.0f, 360.0f)));
		P.Spin = Rand.GetUnitVector() * Rand.FRandRange(2.0f, 14.0f);
		// arrasto quadrático: velocidade terminal ∝ √tamanho (massa/área), em torno da da receita
		const float Vt = FMath::Max(Recipe.TerminalSpeed * FMath::Sqrt(FMath::Max(S, 0.01f) / 0.4f), 5.0f);
		P.Drag = G / (Vt * Vt);
		Out.Add(P);
	}
}

void ABlastEffects::Start(const FBlastRecipe& InRecipe, const FBlastSurface& InSurface, int32 InSeed)
{
	Recipe = InRecipe;
	Surface = InSurface;
	Rand.Initialize(InSeed);
	GroundM = GetActorLocation() / 100.0;
	T = 0.0f;
	bStarted = true;

	UStaticMesh* Sphere = Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Plane = Mesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Base = Material(TEXT("/Game/Materials/M_Base.M_Base"));

	Flash->SetStaticMesh(Sphere);
	Flash->SetMaterial(0, Material(TEXT("/Game/Materials/M_Glow.M_Glow")));
	Flash->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f * Recipe.FlashRadiusM * 0.5f));

	Ring->SetStaticMesh(Plane);
	Ring->SetMaterial(0, Material(TEXT("/Game/Materials/M_GroundRing.M_GroundRing")));
	Ring->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
	Ring->SetRelativeScale3D(FVector(2.0f * Recipe.RingRadiusM * RingPlaneMargin));    // o plano tem 1 m
	SetColor(Ring, Surface.RingColor);
	Ring->SetCustomPrimitiveDataFloat(5, Rand.FRand());

	DebrisMesh->SetStaticMesh(Cube);
	DebrisMesh->SetMaterial(0, Base);
	SetColor(DebrisMesh, DebrisColor);
	ClodMesh->SetStaticMesh(Sphere);
	ClodMesh->SetMaterial(0, Base);
	SetColor(ClodMesh, Surface.ClodColor);
	SparkMesh->SetStaticMesh(Cube);
	SparkMesh->SetMaterial(0, Material(TEXT("/Game/Materials/M_Emissive.M_Emissive")));
	SetColor(SparkMesh, SparkColor * SparkRadiance);

	// o cubo e a esfera da Unreal têm 1 m: a escala é o tamanho em m
	Emit(Debris, Recipe.Debris, Recipe.DebrisSpeed, Recipe.DebrisElevDeg, Recipe.DebrisSizeM, FVector(1.0f, 0.7f, 0.35f), 0.5f);
	Emit(Clods, FMath::RoundToInt32(Recipe.Clods * Surface.Clods), Recipe.ClodSpeed, Recipe.ClodElevDeg, Recipe.ClodSizeM,
		FVector(1.0f, 0.85f, 0.7f), 0.2f);
	Emit(Sparks, Recipe.Sparks, Recipe.SparkSpeed, Recipe.SparkElevDeg, FBlastRange{ 1.0f, 1.0f }, FVector(1.0f), 0.8f);
	for (FPiece& S : Sparks)
	{
		S.Life = Recipe.SparkLifeMs.Sample(Rand) / 1000.0f;
		S.Drag = G / (60.0f * 60.0f);        // fragmento pequeno e rápido
	}
	TArray<FTransform> Zero;
	for (TPair<UInstancedStaticMeshComponent*, int32> M : { TPair<UInstancedStaticMeshComponent*, int32>(DebrisMesh, Debris.Num()),
		TPair<UInstancedStaticMeshComponent*, int32>(ClodMesh, Clods.Num()), TPair<UInstancedStaticMeshComponent*, int32>(SparkMesh, Sparks.Num()) })
	{
		Zero.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector::ZeroVector), M.Value);
		M.Key->ClearInstances();
		M.Key->AddInstances(Zero, false, true);
	}

	Scorch->SetDecalMaterial(Material(TEXT("/Game/Materials/M_Scorch.M_Scorch")));
	Scorch->DecalSize = FVector(ScorchDepthCm, Recipe.ScorchRadiusM * 100.0f, Recipe.ScorchRadiusM * 100.0f);
	Scorch->SetRelativeRotation(FRotator(-90.0f, Rand.FRandRange(0.0f, 360.0f), 0.0f));
	// semente, opacidade, crescimento e a luminância da fuligem no chão do ponto (M_Scorch)
	ScorchParams = FLinearColor(Rand.FRand(), 0.0f, 0.0f, Surface.ScorchColor.GetLuminance());
	Scorch->SetDecalColor(ScorchParams);
}

void ABlastEffects::Integrate(TArray<FPiece>& Pieces, float Dt, float Restitution)
{
	for (FPiece& P : Pieces)
	{
		if (!P.bAlive)
		{
			continue;
		}
		P.Life -= Dt;
		if (P.Life <= 0.0f)
		{
			P.bAlive = false;
			continue;
		}
		const float Bottom = GroundM.Z + 0.5f * P.Size.Z;
		if (P.RestT >= 0.0f)
		{
			// no chão: depois de um tempo afunda (o ZH afunda as carcaças) e some
			if (T - P.RestT > Recipe.RestS)
			{
				P.P.Z -= 0.3f * Dt;
				if (P.P.Z < GroundM.Z - P.Size.GetMax())
				{
					P.bAlive = false;
				}
			}
			continue;
		}
		const float Speed = P.V.Size();
		P.V += (FVector(0.0f, 0.0f, -G) - P.Drag * Speed * P.V) * Dt;
		P.P += P.V * Dt;
		P.Q = FQuat(P.Spin.GetSafeNormal(), P.Spin.Size() * Dt) * P.Q;
		if (P.P.Z < Bottom)
		{
			P.P.Z = Bottom;
			if (FMath::Abs(P.V.Z) < 1.5f)
			{
				P.V = FVector::ZeroVector;
				P.Spin = FVector::ZeroVector;
				P.RestT = T;
			}
			else
			{
				P.V.Z = -P.V.Z * Restitution;
				P.V.X *= 0.55f;
				P.V.Y *= 0.55f;
				P.Spin *= 0.5f;
			}
		}
	}
}

void ABlastEffects::UpdateInstances(UInstancedStaticMeshComponent* Mesh, const TArray<FPiece>& Pieces, bool bStreak)
{
	if (!Mesh || Pieces.Num() == 0)
	{
		return;
	}
	TArray<FTransform> X;
	X.Reserve(Pieces.Num());
	for (const FPiece& P : Pieces)
	{
		if (!P.bAlive)
		{
			X.Add(FTransform(FQuat::Identity, P.P * 100.0, FVector::ZeroVector));
			continue;
		}
		const float MinM = MinSizeM(P.P);
		if (bStreak)
		{
			const float Speed = P.V.Size();
			const FVector Dir = Speed > 0.1f ? P.V / Speed : FVector::UpVector;
			const float Width = FMath::Max(0.05f, MinM * 0.6f);
			const float Length = FMath::Max(Speed * StreakSeconds, Width);
			// ela esfria no fim da vida: some estreitando
			const float Fade = FMath::Clamp(P.Life / 0.15f, 0.0f, 1.0f);
			X.Add(FTransform(FRotationMatrix::MakeFromX(Dir).ToQuat(), (P.P - Dir * Length * 0.5f) * 100.0,
				FVector(Length, Width * Fade, Width * Fade)));
		}
		else
		{
			// tamanho mínimo na tela (como o traçante): escala uniforme pra cima
			const float Grow = FMath::Max(1.0f, MinM / FMath::Max(P.Size.GetMax(), 0.01f));
			X.Add(FTransform(P.Q, P.P * 100.0, P.Size * Grow));
		}
	}
	Mesh->BatchUpdateInstancesTransforms(0, X, true, true, true);
}

void ABlastEffects::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted)
	{
		return;
	}
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	const float Dt = Sim ? Sim->SimDelta(DeltaSeconds) : DeltaSeconds;
	if (Dt <= 0.0f)
	{
		return;
	}
	T += Dt;
	const float Ms = T * 1000.0f;

	// clarão: cresce um pouco e apaga em k² (2–3 quadros do ZH a 30 qps)
	if (Ms < Recipe.FlashMs)
	{
		const float U = Ms / Recipe.FlashMs;
		const float K = (1.0f - U) * (1.0f - U);
		Flash->SetVisibility(true);
		Flash->SetRelativeScale3D(FVector(2.0f * Recipe.FlashRadiusM * (0.7f + 0.3f * U)));
		SetColor(Flash, FLinearColor(1.0f, 0.93f, 0.82f, 0.0f) * (Recipe.FlashRadiance * K));
	}
	else if (Flash->IsVisible())
	{
		Flash->SetVisibility(false);
	}

	// anel de poeira: a frente vai até o raio do dano saindo rápido e freando
	// (1 − (1 − u)²), inteira; depois a poeira assenta em 2× esse tempo. Apagando
	// junto com a expansão ele sumia antes de ser lido (06/10)
	const float RingU = (Ms - Recipe.RingDelayMs) / FMath::Max(Recipe.RingMs, 1.0f);
	if (RingU >= 0.0f && RingU < 3.0f)
	{
		Ring->SetVisibility(true);
		const float U = FMath::Min(RingU, 1.0f);
		const float Front = 1.0f - (1.0f - U) * (1.0f - U);
		const float Settle = RingU < 1.0f ? 1.0f : FMath::Pow(1.0f - (RingU - 1.0f) / 2.0f, 1.5f);
		Ring->SetCustomPrimitiveDataFloat(3, Recipe.RingOpacity * Surface.Ring * Settle);
		Ring->SetCustomPrimitiveDataFloat(4, Front / RingPlaneMargin);
	}
	else if (Ring->IsVisible())
	{
		Ring->SetVisibility(false);
	}

	// marca queimada: cresce até o raio e, no fim da vida, apaga em 5 s. Os
	// shaders dela (e do anel) compilam no aquecimento da abertura (Prewarm): na
	// primeira vez a Unreal desenha o decal padrão, que pinta a caixa inteira
	// (quadrados escuros na primeira explosão, 06/10)
	if (!Scorch->IsVisible())
	{
		Scorch->SetVisibility(true);
	}
	const float Grow = FMath::Clamp(Ms / FMath::Max(Recipe.ScorchMs, 1.0f), 0.0f, 1.0f);
	const float Fade = FMath::Clamp((Recipe.ScorchLifeS - T) / 5.0f, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(ScorchParams.G, Fade, 1e-3f) || !FMath::IsNearlyEqual(ScorchParams.B, Grow, 1e-3f))
	{
		ScorchParams.G = Fade;
		ScorchParams.B = Grow;
		Scorch->SetDecalColor(ScorchParams);
	}

	// pedaços
	LastDt = Dt;
	Integrate(Debris, Dt, 0.3f);
	Integrate(Clods, Dt, 0.2f);
	Integrate(Sparks, Dt, 0.4f);
	UpdateInstances(DebrisMesh, Debris, false);
	UpdateInstances(ClodMesh, Clods, false);
	UpdateInstances(SparkMesh, Sparks, true);

	if (T > Recipe.ScorchLifeS)
	{
		bool bAny = false;
		for (const TArray<FPiece>* A : { &Debris, &Clods, &Sparks })
		{
			for (const FPiece& P : *A)
			{
				bAny |= P.bAlive;
			}
		}
		if (!bAny)
		{
			Destroy();
		}
	}
}
