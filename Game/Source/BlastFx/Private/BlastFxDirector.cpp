#include "BlastFxDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "ShaderCompiler.h"

namespace
{
	const TCHAR* BakePaths[] = {
		TEXT("/Game/Explosions/DA_HeavyCharge_v0.DA_HeavyCharge_v0"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v1.DA_HeavyCharge_v1"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v2.DA_HeavyCharge_v2"),
	};
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_Explosion.M_Explosion");

	// A morte de tanque sem o corpo do EmberGen, na proporção medida contra o
	// vídeo da cena herói (BlastLabSimulation.cpp Detonate, 07/10): bola 0.55,
	// poeira com 40% da altura e metade da vida, a gravação no relógio dela.
	constexpr float TankBallScale = 0.55f;
	constexpr float TankSkirtHeight = 0.4f;
	constexpr float TankSkirtLife = 0.5f;
	constexpr float SizeJitter = 0.1f;
	/** As 3 maiores na tela em detalhe total; as 3 seguintes um nível abaixo; o resto dois (BlastLab). */
	constexpr int32 FullDetailBudget = 3;
	constexpr float WarmSeconds = 2.5f;

	float VariantFraction(uint32 V, uint32 Salt)
	{
		uint32 X = (V ^ Salt) * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return float(X & 0xFFFF) / 65535.0f;
	}
}

UBlastFxDirector* UBlastFxDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBlastFxDirector>() : nullptr;
}

TStatId UBlastFxDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlastFxDirector, STATGROUP_Tickables);
}

bool UBlastFxDirector::LoadAssets()
{
	if (Bakes.Num() && VolumeMaterial)
	{
		return true;
	}
	Bakes.Reset();
	for (const TCHAR* Path : BakePaths)
	{
		if (UExplosionBake* B = LoadObject<UExplosionBake>(nullptr, Path))
		{
			Bakes.Add(B);
		}
	}
	VolumeMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (!Bakes.Num() || !VolumeMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("BlastFx: faltam as explosões gravadas (Game/Tools/game.ps1 worlds traz do BlastLab)"));
		return false;
	}
	return true;
}

AExplosionVolume* UBlastFxDirector::Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed)
{
	AExplosionVolume* E = GetWorld()->SpawnActorDeferred<AExplosionVolume>(AExplosionVolume::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Bake = Bake;
	E->SizeScale = SizeScale;
	E->Seed = Seed;
	E->Age = 0.0f;
	E->bSelfAdvance = true;
	E->Volume->SetMaterial(0, VolumeMaterial.Get());
	E->FinishSpawning(FTransform(Ground));
	return E;
}

AExplosionVolume* UBlastFxDirector::TankDeath(const FVector& Ground, uint32 Variant)
{
	if (!LoadAssets())
	{
		return nullptr;
	}
	UExplosionBake* Bake = Bakes[Variant % uint32(Bakes.Num())];
	const float Jitter = 1.0f + (VariantFraction(Variant, 0x51ED) - 0.5f) * SizeJitter;
	AExplosionVolume* E = Spawn(Ground, Bake, TankBallScale * Jitter, VariantFraction(Variant, 0xA11CE));
	if (!E)
	{
		return nullptr;
	}
	E->TimeScale = Jitter;                    // a gravação no relógio dela, sem esticar pelo tamanho
	E->Thickness = 1.0f / TankBallScale;      // a bola menor tão opaca quanto a Carga Pesada aprovada
	E->SkirtHeight = TankSkirtHeight;
	E->SkirtLife = TankSkirtLife;
	// sem o halo de ofuscamento: é um cartaz virado pra câmera que, na câmera alta do jogo,
	// atravessa o chão e mostra uma borda reta (o BlastLab também o tirou da morte de tanque nova)
	Alive.Add(E);
	return E;
}

void UBlastFxDirector::Prewarm(const FVector& At)
{
	if (!LoadAssets() || bWarm || Warmups.Num())
	{
		return;
	}
	// no centro da vista (a Unreal só compila o shader no primeiro desenho: fora da tela não aquece)
	UpdateView();
	for (int32 I = 0; I < Bakes.Num(); ++I)
	{
		if (AExplosionVolume* E = Spawn(At + ViewRight * (I - 1) * 800.0f, Bakes[I], TankBallScale, 0.0f))
		{
			E->Opacity = 0.0f;       // desenha, mas não mostra nada
			E->Age = 0.3f;
			E->bSelfAdvance = false;
			E->bLights = false;
			Warmups.Add(E);
		}
	}
	WarmClock = 0.0f;
}

void UBlastFxDirector::UpdateView()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	bHasView = false;
	if (!PC || !PC->PlayerCameraManager || !Viewport)
	{
		return;
	}
	FVector2D Size;
	Viewport->GetViewportSize(Size);
	if (Size.X < 1.0 || Size.Y < 1.0)
	{
		return;
	}
	const FRotationMatrix M(PC->PlayerCameraManager->GetCameraRotation());
	ViewLocation = PC->PlayerCameraManager->GetCameraLocation();
	ViewForward = M.GetScaledAxis(EAxis::X);
	ViewRight = M.GetScaledAxis(EAxis::Y);
	ViewUp = M.GetScaledAxis(EAxis::Z);
	// o campo de visão da câmera do jogo é o horizontal (padrão da Unreal)
	TanHalfX = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f);
	TanHalfY = TanHalfX * float(Size.Y / Size.X);
	bHasView = true;
}

float UBlastFxDirector::ScreenFraction(const FVector& Center, float Radius, bool& bInView) const
{
	bInView = true;
	if (!bHasView)
	{
		return 1.0f;
	}
	// esfera contra as 4 faces e o plano da câmera, nos eixos da vista (como no BlastLab)
	const FVector P = Center - ViewLocation;
	const float Z = FVector::DotProduct(P, ViewForward);
	const float X = FVector::DotProduct(P, ViewRight);
	const float Y = FVector::DotProduct(P, ViewUp);
	const float KX = Radius * FMath::Sqrt(1.0f + TanHalfX * TanHalfX);
	const float KY = Radius * FMath::Sqrt(1.0f + TanHalfY * TanHalfY);
	bInView = Z > -Radius && FMath::Abs(X) - Z * TanHalfX <= KX && FMath::Abs(Y) - Z * TanHalfY <= KY;
	if (!bInView)
	{
		return 0.0f;
	}
	return Z <= Radius ? 1.0f : FMath::Min(Radius / (Z * TanHalfY), 1.0f);
}

void UBlastFxDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateView();
	if (Warmups.Num())
	{
		WarmClock += DeltaTime;
		const bool bShadersDone = !GShaderCompilingManager || GShaderCompilingManager->GetNumRemainingJobs() == 0;
		if ((WarmClock > WarmSeconds && bShadersDone) || WarmClock > 60.0f)
		{
			for (AExplosionVolume* E : Warmups)
			{
				if (IsValid(E))
				{
					E->Destroy();
				}
			}
			Warmups.Reset();
			bWarm = true;
		}
	}
	// a gravação acabou: a explosão sai de cena
	for (int32 I = Alive.Num() - 1; I >= 0; --I)
	{
		AExplosionVolume* E = Alive[I];
		if (!IsValid(E) || E->IsFinished())
		{
			if (IsValid(E))
			{
				E->Destroy();
			}
			Alive.RemoveAtSwap(I);
		}
	}
	// orçamento de detalhe: as maiores na tela ficam com tudo
	TArray<AExplosionVolume*, TInlineAllocator<32>> Seen;
	for (AExplosionVolume* E : Alive)
	{
		if (E->IsOnScreen())
		{
			Seen.Add(E);
		}
		else
		{
			E->LodFloor = 0;
		}
	}
	Seen.Sort([](const AExplosionVolume& A, const AExplosionVolume& B) { return A.ScreenFraction() > B.ScreenFraction(); });
	for (int32 I = 0; I < Seen.Num(); ++I)
	{
		Seen[I]->LodFloor = I < FullDetailBudget ? 0 : (I < 2 * FullDetailBudget ? 1 : 2);
	}
}
