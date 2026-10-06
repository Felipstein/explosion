#include "BlastLabSimulation.h"

#include "BattleSmokeSubsystem.h"
#include "BlastLabEnvironment.h"
#include "BlastLabMover.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "Materials/MaterialInterface.h"
#include "ShaderCompiler.h"

namespace
{
	// As variantes gravadas (Tools/build_explosion.py): a mesma carga com sementes diferentes.
	const TCHAR* BakePaths[] = {
		TEXT("/Game/Explosions/DA_HeavyCharge_v0.DA_HeavyCharge_v0"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v1.DA_HeavyCharge_v1"),
		TEXT("/Game/Explosions/DA_HeavyCharge_v2.DA_HeavyCharge_v2"),
	};
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_Explosion.M_Explosion");

	// Variação de tamanho por detonação (instances.js spawn: jitter 0.14): uma
	// mesma arma não sai sempre igual. Tamanho e duração juntos, pela mesma lei.
	constexpr float SizeJitter = 0.14f;

	// quantas explosões na tela ficam com detalhe máximo
	constexpr int32 FullDetailBudget = 3;

	// a deformação pelo que passa: acima dessa escala a explosão não cabe no
	// campo de 96 m que dá o vento (o ataque aéreo, 2.2, cabe; a nuclear não)
	constexpr float MaxWarpScale = 3.0f;
}

UBlastLabSimulation* UBlastLabSimulation::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBlastLabSimulation>() : nullptr;
}

const TArray<FBlastWeapon>& UBlastLabSimulation::Arsenal()
{
	// Granada 0.05 = 1 kg de TNT, então 1 = 20 kg. Nuclear 729 = 9³ (~15 t,
	// a ordem do W54 do Davy Crockett): a coluna de ~38 m vira ~340 m e a
	// sequência de 6 s dura ~54 s.
	static const TArray<FBlastWeapon> Weapons = {
		{ TEXT("granada"),  TEXT("Granada"),       0.05f,  TEXT("HE · 1 kg"),                false },
		{ TEXT("morteiro"), TEXT("Morteiro"),      0.22f,  TEXT("HE · 5 kg"),                false },
		{ TEXT("tanque"),   TEXT("Carga Pesada"),  1.0f,   TEXT("HE · alto rendimento"),     false },
		{ TEXT("deposito"), TEXT("Paiol"),         4.0f,   TEXT("detonação secundária"),     false },
		{ TEXT("aereo"),    TEXT("Ataque Aéreo"),  11.0f,  TEXT("bomba de demolição"),       false },
		{ TEXT("nuclear"),  TEXT("Bomba Nuclear"), 729.0f, TEXT("nuclear tática · ~15 t"),   true  },
	};
	return Weapons;
}

bool UBlastLabSimulation::LoadAssets()
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
		UE_LOG(LogTemp, Error, TEXT("BlastLab: faltam as explosões gravadas (rode Tools/blast.ps1 build-explosion)"));
		return false;
	}
	return true;
}

AExplosionVolume* UBlastLabSimulation::Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale)
{
	AExplosionVolume* E = GetWorld()->SpawnActorDeferred<AExplosionVolume>(AExplosionVolume::StaticClass(), FTransform(Ground),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Bake = Bake;
	E->SizeScale = SizeScale;
	E->Seed = FMath::FRand();
	E->Age = 0.0f;
	E->bSelfAdvance = true;
	E->Volume->SetMaterial(0, VolumeMaterial);
	E->FinishSpawning(FTransform(Ground));
	return E;
}

AExplosionVolume* UBlastLabSimulation::Detonate(const FVector& Ground, int32 Weapon)
{
	if (!Arsenal().IsValidIndex(Weapon) || !LoadAssets())
	{
		return nullptr;
	}
	const FBlastWeapon& W = Arsenal()[Weapon];
	AExplosionVolume* E = Spawn(Ground, Bakes[FMath::RandRange(0, Bakes.Num() - 1)],
		W.SizeScale() * (1.0f + (FMath::FRand() - 0.5f) * SizeJitter));
	if (E)
	{
		Alive.Add(E);
		LastBlastGround = Ground;
		bHasLastBlast = true;
		// a expansão empurra a fumaça das outras pra fora (main.js: a 2 m·escala
		// do chão); as que ela alcança passam a ser deformadas pelo campo. A
		// própria não: a expansão dela já está na gravação.
		if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
		{
			const FVector Center = Ground + FVector(0.0f, 0.0f, 200.0f * E->SizeScale);
			Battle->AddBlast(Center, E->SizeScale);
			const float ReachCm = UBattleSmokeSubsystem::BlastRadiusM(E->SizeScale) * 100.0f;
			for (AExplosionVolume* O : Alive)
			{
				if (O != E && IsValid(O) && O->SizeScale <= MaxWarpScale && Battle->Covers(O->GetActorLocation())
					&& O->SmokeBoxWorld().ExpandBy(ReachCm).IsInside(Center))
				{
					Battle->AttachWarp(O);
				}
			}
		}
	}
	return E;
}

int32 UBlastLabSimulation::PendingShaders()
{
	return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
}

void UBlastLabSimulation::Prewarm(const FVector& At)
{
	if (!LoadAssets())
	{
		return;
	}
	MoverMaterials.Reset();
	for (const TCHAR* Path : { TEXT("/Game/Materials/M_Emissive.M_Emissive"), TEXT("/Game/Materials/M_Glow.M_Glow") })
	{
		MoverMaterials.Add(LoadObject<UMaterialInterface>(nullptr, Path));
	}
	for (UExplosionBake* B : Bakes)
	{
		if (AExplosionVolume* E = Spawn(At, B, 1.0f))
		{
			E->Opacity = 0.0f;
			E->Age = 0.3f;                 // já com bola de fogo: o material inteiro em uso
			E->bSelfAdvance = false;       // parada até os shaders ficarem prontos
			Warmups.Add(E);
		}
	}
	WarmupClock = 0.0f;
}

void UBlastLabSimulation::UpdateView()
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
	// a câmera guarda o fov horizontal de 16:9 com o vertical fixo (BlastLabCamera)
	TanHalfY = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f) * 9.0f / 16.0f;
	TanHalfX = TanHalfY * float(Size.X / Size.Y);
	ViewHeightPx = float(Size.Y);
	bHasView = true;
	if (const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
	{
		CachedAmbientLux = Env->RenderLux();
	}
}

float UBlastLabSimulation::RadiansPerPixel() const
{
	// o render é menor que a janela (o TSR amplia): o pixel que conta é o dele
	static const IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	const float Pct = ScreenPercentage ? FMath::Clamp(ScreenPercentage->GetFloat(), 10.0f, 200.0f) : 100.0f;
	return 2.0f * TanHalfY / FMath::Max(ViewHeightPx * Pct / 100.0f, 1.0f);
}

float UBlastLabSimulation::ScreenFraction(const FVector& Center, float Radius, bool& bInView) const
{
	bInView = true;
	if (!bHasView)
	{
		return 1.0f;
	}
	// esfera contra as 4 faces e o plano da câmera, nos eixos da vista
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
	// dentro dela (ou quase): ocupa a tela toda
	return Z <= Radius ? 1.0f : FMath::Min(Radius / (Z * TanHalfY), 1.0f);
}

void UBlastLabSimulation::Tick(float DeltaTime)
{
	UpdateView();
	// aquecimento: sai quando os shaders acabaram de compilar (e o volume já
	// desenhou alguns quadros), ou em 60 s de qualquer jeito
	if (Warmups.Num())
	{
		WarmupClock += DeltaTime;
		if ((PendingShaders() == 0 && WarmupClock > 2.0f) || WarmupClock > 60.0f)
		{
			for (AExplosionVolume* E : Warmups)
			{
				if (IsValid(E))
				{
					E->Destroy();
				}
			}
			Warmups.Reset();
		}
	}
	// a gravação acabou: a explosão sai de cena (o WebGL fazia o mesmo)
	for (int32 i = Alive.Num() - 1; i >= 0; --i)
	{
		AExplosionVolume* E = Alive[i];
		if (!IsValid(E) || E->IsFinished())
		{
			if (IsValid(E))
			{
				E->Destroy();
			}
			Alive.RemoveAtSwap(i);
		}
	}
	// Orçamento de detalhe (como o Significance Manager): as mais importantes
	// na tela (as maiores) ficam com tudo; passando delas, as seguintes descem
	// um nível e o resto dois. Medido 05/10 com 8 Cargas Pesadas na tela: o
	// volume custava 15–25 ms de GPU, e as que estão atrás das outras quase não
	// aparecem.
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
	for (int32 i = 0; i < Seen.Num(); ++i)
	{
		Seen[i]->LodFloor = i < FullDetailBudget ? 0 : (i < 2 * FullDetailBudget ? 1 : 2);
	}

	// Interação com o que passa: uma explosão que algo atravessa passa a ser
	// deformada pelo vento do campo até o fim dela (UBattleSmokeSubsystem
	// AttachWarp), sem trocar de fumaça. Fora do campo coberto pela grade, ou
	// grande demais pra ele (a nuclear), ela toca como foi gravada.
	if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
	{
		for (AExplosionVolume* E : Alive)
		{
			if (!E->IsWarped() && E->SizeScale <= MaxWarpScale && Battle->Covers(E->GetActorLocation()) && CrossedThisFrame(E))
			{
				Battle->AttachWarp(E);
			}
		}
	}
	MoverSegments.Reset();
	Movers.RemoveAll([](const TWeakObjectPtr<ABlastLabMover>& M) { return !M.IsValid(); });
}

void UBlastLabSimulation::AddMoverSegment(const FVector& FromCm, const FVector& ToCm, float RadiusCm)
{
	MoverSegments.Add({ FromCm, ToCm, RadiusCm });
}

ABlastLabMover* UBlastLabSimulation::SpawnMover(EBlastMover Kind)
{
	// o alvo: o pé da última explosão; sem nenhuma, o chão no centro da tela
	FVector Target = LastBlastGround;
	if (!bHasLastBlast)
	{
		Target = bHasView && ViewForward.Z < -1e-3f ? ViewLocation - ViewForward * (ViewLocation.Z / ViewForward.Z) : FVector::ZeroVector;
		Target.Z = 0.0f;
	}
	// cruza a tela da esquerda pra direita
	FVector Side = FVector(ViewRight.X, ViewRight.Y, 0.0f).GetSafeNormal();
	if (Side.IsNearlyZero())
	{
		Side = FVector::RightVector;
	}
	// Passa pela altura média da fuligem da explosão ali (gravada quadro a
	// quadro, Tools/export_bake_velocity.py; battlesmoke.js smokeHeightAt). O meio
	// da caixa não serve: a poeira rente ao chão a puxa pra baixo e o avião
	// passava por baixo da nuvem. Sem explosão ali, a fuligem do campo, se houver.
	float AltM = -1.0f;
	for (int32 i = Alive.Num() - 1; i >= 0; --i)
	{
		const AExplosionVolume* E = Alive[i];
		if (IsValid(E) && FVector::Dist2D(E->GetActorLocation(), Target) < 2000.0f * FMath::Max(E->SizeScale, 1.0f))
		{
			// onde a fuligem vai estar quando ele chegar (a nuvem sobe ~10 m/s no começo)
			const float Now = (E->SootHeightWorld() - Target.Z) / 100.0f;
			const float Ahead = ABlastLabMover::TimeToTarget(Kind, Now);
			AltM = (E->SootHeightWorld(Ahead) - Target.Z) / 100.0f;
			AltM = (E->SootHeightWorld(ABlastLabMover::TimeToTarget(Kind, AltM)) - Target.Z) / 100.0f;
			break;
		}
	}
	FBox Box;
	float SootCm = 0.0f;
	const UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this);
	if (AltM < 0.0f && Battle && Battle->SmokeBox(Box) && Box.ExpandBy(FVector(800.0f, 800.0f, 0.0f)).IsInsideXY(Target) && Battle->SootHeight(SootCm))
	{
		AltM = SootCm / 100.0f;
	}
	UE_LOG(LogTemp, Display, TEXT("BlastLab: passagem %d em %s, altura da fumaça %.1f m"), int32(Kind), *Target.ToString(), AltM);
	ABlastLabMover* M = ABlastLabMover::Launch(GetWorld(), Kind, Target, Side, AltM);
	if (M)
	{
		Movers.Add(M);
	}
	return M;
}

bool UBlastLabSimulation::IsMoverActive(EBlastMover Kind) const
{
	for (const TWeakObjectPtr<ABlastLabMover>& M : Movers)
	{
		if (M.IsValid() && M->GetKind() == Kind)
		{
			return true;
		}
	}
	return false;
}

bool UBlastLabSimulation::CrossedThisFrame(const AExplosionVolume* E) const
{
	// algum objeto deste quadro passou pela caixa da fumaça dela (com folga do raio)? (battlesmoke.js _crossed)
	if (!MoverSegments.Num())
	{
		return false;
	}
	const FBox Box = E->SmokeBoxWorld();
	for (const FMoverSegment& S : MoverSegments)
	{
		const FBox B = Box.ExpandBy(S.RadiusCm);
		FVector Hit, Normal;
		float Time;
		if (B.IsInside(S.From) || B.IsInside(S.To) || FMath::LineExtentBoxIntersection(B, S.From, S.To, FVector::ZeroVector, Hit, Normal, Time))
		{
			return true;
		}
	}
	return false;
}
