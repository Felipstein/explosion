#include "ForgeTreeFade.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

namespace
{
	// a malha sem voxels ao lado da árvore: SM_broadleaf → SM_broadleaf_Fade
	UStaticMesh* FadeMeshOf(UStaticMesh* Mesh)
	{
		static TMap<TWeakObjectPtr<UStaticMesh>, TWeakObjectPtr<UStaticMesh>> Cache;
		if (const TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(Mesh))
		{
			return Found->Get();
		}
		UStaticMesh* Fade = nullptr;
		if (Mesh->GetName().StartsWith(TEXT("SM_")) && !Mesh->GetName().EndsWith(TEXT("_Fade")))
		{
			const FString Path = Mesh->GetPathName();   // /Game/Slice/Trees/SM_x.SM_x
			const FString Package = FPackageName::ObjectPathToPackageName(Path);
			const FString Name = Mesh->GetName() + TEXT("_Fade");
			Fade = LoadObject<UStaticMesh>(nullptr, *(FPackageName::GetLongPackagePath(Package) / Name + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		Cache.Add(Mesh, Fade);
		return Fade;
	}
}

void UForgeTreeFade::Scan(UWorld* World)
{
	Sources.Reset();
	MaxReach = 0.0f;
	ScannedWorld = World;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* C = *It;
		// o gerador usa componentes hierárquicos enormes (um por malha): fora daqui
		if (C->GetWorld() != World || C->IsA<UHierarchicalInstancedStaticMeshComponent>() || !C->GetStaticMesh() || !C->IsRegistered())
		{
			continue;
		}
		UStaticMesh* Fade = FadeMeshOf(C->GetStaticMesh());
		if (!Fade)
		{
			continue;
		}
		FSource& S = Sources.AddDefaulted_GetRef();
		S.Component = C;
		S.FadeMesh = Fade;
		S.Bounds = C->Bounds.GetBox();
		const FBox Local = C->GetStaticMesh()->GetBoundingBox();
		S.Height = Local.Max.Z;
		S.Crown = 0.5f * FMath::Max(Local.GetSize().X, Local.GetSize().Y);
		const int32 N = C->GetInstanceCount();
		S.Transforms.SetNum(N);
		float MaxScale = 0.0f;
		for (int32 I = 0; I < N; ++I)
		{
			C->GetInstanceTransform(I, S.Transforms[I], true);
			MaxScale = FMath::Max(MaxScale, float(S.Transforms[I].GetMaximumAxisScale()));
		}
		MaxReach = FMath::Max(MaxReach, MaxScale * FMath::Max(S.Height, S.Crown));
	}
	const FSource* Densest = nullptr;
	for (const FSource& S : Sources)
	{
		Densest = !Densest || S.Transforms.Num() > Densest->Transforms.Num() ? &S : Densest;
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE tree fade: %d componentes de árvore; o mais cheio: %d árvores em (%.0f, %.0f) m"), Sources.Num(),
		Densest ? Densest->Transforms.Num() : 0, Densest ? Densest->Bounds.GetCenter().X / 100.0 : 0.0, Densest ? Densest->Bounds.GetCenter().Y / 100.0 : 0.0);
}

void UForgeTreeFade::Update(UWorld* World, const FVector& Camera, const FVector& Cursor, float Radius)
{
	if (!World)
	{
		return;
	}
	if (ScannedWorld.Get() != World)
	{
		Scan(World);
	}
	static const TArray<int32> None;
	if (Radius <= 0.0f)
	{
		for (FSource& S : Sources)
		{
			SetFaded(S, None);
		}
		return;
	}
	// a busca: a caixa da linha câmera → cursor, crescida do raio e da maior árvore
	const FBox Line = FBox(Camera.ComponentMin(Cursor), Camera.ComponentMax(Cursor)).ExpandBy(Radius + MaxReach);
	TArray<int32> Wanted;
	for (FSource& S : Sources)
	{
		if (!S.Component.IsValid())
		{
			continue;
		}
		Wanted.Reset();
		if (S.Bounds.Intersect(Line))
		{
			for (int32 I = 0; I < S.Transforms.Num(); ++I)
			{
				const FTransform& T = S.Transforms[I];
				const FVector Base = T.GetLocation();
				const FVector Top = Base + T.GetUnitAxis(EAxis::Z) * (S.Height * T.GetScale3D().Z);
				const float Crown = S.Crown * float(T.GetScale3D().X);
				if (!FBox(Base.ComponentMin(Top), Base.ComponentMax(Top)).ExpandBy(Crown).Intersect(Line))
				{
					continue;
				}
				// a árvore como cápsula (tronco ao topo, raio da copa) contra a linha:
				// entra no fantasma um pouco antes de o recorte chegar nela
				FVector P, Q;
				FMath::SegmentDistToSegmentSafe(Base, Top, Camera, Cursor, P, Q);
				if (FVector::Dist(P, Q) < Radius + Crown)
				{
					Wanted.Add(I);
				}
			}
		}
		SetFaded(S, Wanted);
	}
}

void UForgeTreeFade::SetFaded(FSource& S, const TArray<int32>& Wanted)
{
	if (S.Faded == Wanted)
	{
		return;
	}
	UInstancedStaticMeshComponent* C = S.Component.Get();
	if (!C)
	{
		S.Faded.Reset();
		return;
	}
	for (int32 I : S.Faded)
	{
		if (!Wanted.Contains(I))
		{
			C->UpdateInstanceTransform(I, S.Transforms[I], true, true, true);
		}
	}
	for (int32 I : Wanted)
	{
		if (!S.Faded.Contains(I))
		{
			// escondida no lugar (escala quase 0: um milímetro de árvore)
			FTransform Hidden = S.Transforms[I];
			Hidden.SetScale3D(FVector(1e-3));
			C->UpdateInstanceTransform(I, Hidden, true, true, true);
		}
	}
	S.Faded = Wanted;

	UInstancedStaticMeshComponent* Ghost = S.Ghost.Get();
	if (!Ghost && Wanted.Num() > 0 && S.FadeMesh.IsValid() && C->GetOwner())
	{
		Ghost = NewObject<UInstancedStaticMeshComponent>(C->GetOwner(), NAME_None, RF_Transient);
		Ghost->SetMobility(C->Mobility);
		Ghost->SetStaticMesh(S.FadeMesh.Get());
		for (int32 M = 0; M < C->GetNumMaterials(); ++M)
		{
			Ghost->SetMaterial(M, C->GetMaterial(M));
		}
		Ghost->SetCastShadow(C->CastShadow);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// entra e sai o tempo todo: fora da cena do Lumen (a árvore escondida no
		// componente original já sai dela)
		Ghost->bAffectDistanceFieldLighting = false;
		Ghost->bAffectDynamicIndirectLighting = false;
		Ghost->SetWorldTransform(FTransform::Identity);
		Ghost->RegisterComponent();
		S.Ghost = Ghost;
	}
	if (Ghost)
	{
		Ghost->ClearInstances();
		TArray<FTransform> Ts;
		for (int32 I : Wanted)
		{
			Ts.Add(S.Transforms[I]);
		}
		if (Ts.Num() > 0)
		{
			Ghost->AddInstances(Ts, false, true, false);
		}
	}
}

int32 UForgeTreeFade::NumFaded() const
{
	int32 N = 0;
	for (const FSource& S : Sources)
	{
		N += S.Faded.Num();
	}
	return N;
}
