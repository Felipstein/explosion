#include "RTSDockActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "RTSGameData.h"

namespace
{
	UStaticMesh* LoadDockMesh(const FString& PackagePath)
	{
		if (PackagePath.IsEmpty())
		{
			return nullptr;
		}
		const FString Name = FPaths::GetBaseFilename(PackagePath);
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

ARTSDockActor::ARTSDockActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Yard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Yard"));
	Yard->SetupAttachment(Root);
	Yard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Yard->SetCanEverAffectNavigation(false);
	Crates = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Crates"));
	Crates->SetupAttachment(Root);
	Crates->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crates->SetCanEverAffectNavigation(false);
}

void ARTSDockActor::Setup(const FRTSDockVisual& Visual, bool bInPile, uint32 Seed)
{
	bPile = bInPile;
	if (UStaticMesh* Mesh = bPile ? nullptr : LoadDockMesh(Visual.Mesh))
	{
		Yard->SetStaticMesh(Mesh);
	}
	else
	{
		Yard->SetVisibility(false);
	}
	if (UStaticMesh* Crate = LoadDockMesh(Visual.CrateMesh))
	{
		Crates->SetStaticMesh(Crate);
	}
	Slots.Reset();
	if (bPile)
	{
		// a pilha caída: as caixas tortas, cada pilha girada de um jeito
		const float Turn = float(Seed % 360);
		for (const FVector4& S : Visual.PileSlotsCm)
		{
			const FVector P = FRotator(0.0f, Turn, 0.0f).RotateVector(FVector(S.X, S.Y, S.Z));
			Slots.Add(FTransform(FRotator(0.0f, Turn + float(S.W), 0.0f), P));
		}
	}
	else
	{
		for (const FVector& P : Visual.CrateSlotsCm)
		{
			Slots.Add(FTransform(FRotator(0.0f, float((Seed + uint32(Slots.Num()) * 37u) % 7) - 3.0f, 0.0f), P));
		}
	}
}

void ARTSDockActor::SetBoxes(int32 Boxes, int32 StartBoxes)
{
	const int32 Total = Slots.Num();
	int32 Want = 0;
	if (Boxes > 0 && Total > 0)
	{
		// a pilha tem uma caixa por caixa; o depósito, a proporção do estoque (pra cima: a
		// última caixa só some quando acaba de verdade)
		Want = bPile ? FMath::Min(Boxes, Total) : FMath::Clamp(int32(FMath::DivideAndRoundUp(int64(Total) * Boxes, int64(FMath::Max(1, StartBoxes)))), 1, Total);
	}
	if (Want == Shown)
	{
		return;
	}
	Shown = Want;
	Crates->ClearInstances();
	// as primeiras da lista somem primeiro: mostra o fim dela
	for (int32 K = Total - Want; K < Total; ++K)
	{
		Crates->AddInstance(Slots[K], false);
	}
	SetActorHiddenInGame(bPile && Want == 0);
}
