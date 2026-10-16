#include "RTSWireSystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, RTSWorldProps);

namespace
{
	constexpr float StepS = 1.0f / 120.0f;            // o passo fixo (a corrente estável com o tanque a 60 qps)
	constexpr float Gravity = -980.0f;                 // cm/s²
	constexpr float Damping = 0.995f;                  // o ar
	constexpr int32 Iterations = 16;                   // as passadas do comprimento por passo
	constexpr float SnapStretch = 1.5f;                // arrebenta acima de 1,5× o comprimento (o estudo, §12.8)
	constexpr float SleepAfterS = 1.5f;                // parado tanto tempo, dorme
	constexpr float StillCm = 0.02f;                   // "parado": cada ponto anda menos que isso por passo
}

bool URTSWireSystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URTSWireSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSWireSystem, STATGROUP_Tickables);
}

void URTSWireSystem::Deinitialize()
{
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	Comps.Reset();
	Spans.Reset();
	Super::Deinitialize();
}

UInstancedStaticMeshComponent* URTSWireSystem::CompFor(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
	if (!Mesh)
	{
		return nullptr;
	}
	const FString Key = Mesh->GetPathName() + TEXT("|") + (Material ? Material->GetPathName() : FString());
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Comps.Find(Key))
	{
		return *Found;
	}
	if (!Holder)
	{
		FActorSpawnParameters P;
		P.ObjectFlags = RF_Transient;
		Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Holder);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	if (Material)
	{
		for (int32 I = 0; I < Mesh->GetStaticMaterials().Num(); ++I)
		{
			C->SetMaterial(I, Material);
		}
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	C->SetupAttachment(Holder->GetRootComponent());
	C->RegisterComponent();
	Comps.Add(Key, C);
	return C;
}

int32 URTSWireSystem::ReleaseSpan(const FVector& ACm, const FVector& BCm, float SagM, float RadiusCm, UStaticMesh* Mesh, UMaterialInterface* Material, int32 FollowEnd)
{
	UInstancedStaticMeshComponent* C = CompFor(Mesh, Material);
	const float Span = FVector::Dist(ACm, BCm);
	if (!C || Span < 1.0f)
	{
		return INDEX_NONE;
	}
	FSpan S;
	// um ponto a cada ~1,2 m (8 a 40): curva lisa de cima e barata
	const int32 N = FMath::Clamp(FMath::CeilToInt(Span / 120.0f) + 1, 8, 40);
	S.P.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		const float T = float(I) / float(N - 1);
		// a flecha da catenária como parábola (a mesma forma da camada de fios a 1% de erro com a flecha de ~1–3% do vão)
		S.P[I] = FMath::Lerp(ACm, BCm, T) - FVector(0.0f, 0.0f, 4.0f * SagM * 100.0f * T * (1.0f - T));
	}
	S.Prev = S.P;
	for (int32 I = 0; I + 1 < N; ++I)
	{
		S.Total += FVector::Dist(S.P[I], S.P[I + 1]);
	}
	S.Rest = S.Total / float(N - 1);
	S.Radius = FMath::Max(RadiusCm, 0.3f);
	S.EndPos[0] = ACm;
	S.EndPos[1] = BCm;
	S.Follow = FollowEnd ? 1 : 0;
	S.Comp = C;
	S.FirstInstance = C->GetInstanceCount();
	TArray<FTransform> Init;
	Init.Init(FTransform::Identity, N - 1);
	C->AddInstances(Init, false, true);
	const int32 Handle = Spans.Add(MoveTemp(S));
	Draw(Spans[Handle]);
	return Handle;
}

void URTSWireSystem::MoveSpanEnd(int32 Handle, int32 End, const FVector& PosCm)
{
	if (!Spans.IsValidIndex(Handle))
	{
		return;
	}
	FSpan& S = Spans[Handle];
	S.EndPos[End ? 1 : 0] = PosCm;
	S.bAsleep = false;
	S.Still = 0.0f;
}

void URTSWireSystem::SetGroundHeight(TFunction<float(float XCm, float YCm)> InGround)
{
	Ground = MoveTemp(InGround);
}

int32 URTSWireSystem::NumActiveSpans() const
{
	int32 N = 0;
	for (const FSpan& S : Spans)
	{
		N += S.bAsleep ? 0 : 1;
	}
	return N;
}

float URTSWireSystem::GroundAt(const FSpan& S, float X, float Y) const
{
	return Ground ? Ground(X, Y) : FMath::Min(S.EndPos[0].Z, S.EndPos[1].Z) - 800.0f;
}

void URTSWireSystem::Step(FSpan& S, int32 Handle, float Dt)
{
	const int32 N = S.P.Num();
	S.bDirty = true;
	const FVector G(0.0f, 0.0f, Gravity * Dt * Dt);
	float MaxMove = 0.0f;
	// Verlet: a velocidade é o passo anterior; as pontas presas seguem os postes
	for (int32 I = 0; I < N; ++I)
	{
		const bool bPin = (I == 0 && S.bPinned[0]) || (I == N - 1 && S.bPinned[1]);
		if (bPin)
		{
			S.Prev[I] = S.P[I];
			S.P[I] = S.EndPos[I == 0 ? 0 : 1];
			MaxMove = FMath::Max(MaxMove, FVector::Dist(S.P[I], S.Prev[I]));
			continue;
		}
		const FVector V = (S.P[I] - S.Prev[I]) * Damping;
		S.Prev[I] = S.P[I];
		S.P[I] += V + G;
	}
	// o comprimento de cada trecho (o fio não estica): as pontas presas não andam
	for (int32 It = 0; It < Iterations; ++It)
	{
		for (int32 I = 0; I + 1 < N; ++I)
		{
			const bool bA = (I == 0 && S.bPinned[0]);
			const bool bB = (I + 1 == N - 1 && S.bPinned[1]);
			const FVector D = S.P[I + 1] - S.P[I];
			const float L = D.Size();
			if (L < 1e-3f || (bA && bB))
			{
				continue;
			}
			const FVector Fix = D * ((L - S.Rest) / L);
			if (bA) S.P[I + 1] -= Fix;
			else if (bB) S.P[I] += Fix;
			else { S.P[I] += Fix * 0.5f; S.P[I + 1] -= Fix * 0.5f; }
		}
	}
	// o chão: o fio deita, com atrito (perde a velocidade de lado) e quase sem quique
	for (int32 I = 0; I < N; ++I)
	{
		const float Gz = GroundAt(S, S.P[I].X, S.P[I].Y) + S.Radius;
		if (S.P[I].Z < Gz)
		{
			S.P[I].Z = Gz;
			S.Prev[I].Z = Gz;
			S.Prev[I].X = FMath::Lerp(S.Prev[I].X, S.P[I].X, 0.3f);
			S.Prev[I].Y = FMath::Lerp(S.Prev[I].Y, S.P[I].Y, 0.3f);
		}
		MaxMove = FMath::Max(MaxMove, FVector::Dist(S.P[I], S.Prev[I]));
	}
	// arrebenta: as pontas presas longe demais (o poste caindo puxa); a ponta do poste que cai solta
	if (!S.bSnapped && S.bPinned[0] && S.bPinned[1] && FVector::Dist(S.EndPos[0], S.EndPos[1]) > SnapStretch * S.Total)
	{
		S.bSnapped = true;
		S.bPinned[S.Follow] = false;
		OnWireSnap.Broadcast(Handle, S.EndPos[S.Follow]);
	}
	// a ponta viva no chão (a solta, se mexendo): o arco e a faísca são do jogo
	S.GroundCooldown -= Dt;
	if (S.bSnapped)
	{
		const int32 E = S.Follow ? N - 1 : 0;
		const float Speed = FVector::Dist(S.P[E], S.Prev[E]) / Dt;
		const float Gz = GroundAt(S, S.P[E].X, S.P[E].Y) + S.Radius;
		if (S.GroundCooldown <= 0.0f && S.P[E].Z <= Gz + 2.0f && Speed > 50.0f)
		{
			S.GroundCooldown = 0.2f;
			OnWireGround.Broadcast(Handle, S.P[E]);
		}
	}
	// dorme quando assenta (fica desenhado como parou)
	S.Still = MaxMove < StillCm ? S.Still + Dt : 0.0f;
	if (S.Still > SleepAfterS)
	{
		S.bAsleep = true;
	}
}

void URTSWireSystem::Draw(FSpan& S)
{
	if (!S.Comp)
	{
		return;
	}
	// cada trecho é um pedaço do fio: o cilindro de pé no Z (100 cm de altura e de diâmetro), esticado e girado
	const int32 N = S.P.Num();
	TArray<FTransform> X;
	X.Reserve(N - 1);
	const float W = 2.0f * S.Radius / 100.0f;
	for (int32 I = 0; I + 1 < N; ++I)
	{
		const FVector A = S.P[I], B = S.P[I + 1];
		const FVector D = B - A;
		const float L = FMath::Max(D.Size(), 0.1f);
		X.Add(FTransform(FQuat::FindBetweenNormals(FVector::UpVector, D / L), (A + B) * 0.5f, FVector(W, W, L / 100.0f)));
	}
	S.Comp->BatchUpdateInstancesTransforms(S.FirstInstance, X, true, true, false);
}

void URTSWireSystem::Tick(float DeltaTime)
{
	Accum = FMath::Min(Accum + DeltaTime, 0.1f);
	bool bAny = false;
	for (const FSpan& S : Spans)
	{
		bAny |= !S.bAsleep;
	}
	if (!bAny)
	{
		Accum = 0.0f;
		return;
	}
	while (Accum >= StepS)
	{
		for (int32 H = 0; H < Spans.Num(); ++H)
		{
			if (!Spans[H].bAsleep)
			{
				Step(Spans[H], H, StepS);
			}
		}
		Accum -= StepS;
	}
	for (FSpan& S : Spans)
	{
		if (S.bDirty)
		{
			Draw(S);
			S.bDirty = false;
		}
	}
}
