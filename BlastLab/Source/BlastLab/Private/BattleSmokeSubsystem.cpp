#include "BattleSmokeSubsystem.h"

#include "BlastLabSimulation.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "TextureResource.h"

namespace
{
	const TCHAR* MaterialPath = TEXT("/Game/Explosions/M_BattleSmoke.M_BattleSmoke");
	const TCHAR* NoisePath = TEXT("/Game/Explosions/T_CurlNoise.T_CurlNoise");
	// battlesmoke.js: dissipação (o mesmo sootTau do fluido) e o campo que conta como vazio
	constexpr float SootTau = 10.0f;
	constexpr float IdleMass = 0.15f;
	// a margem da caixa marchada em volta da fumaça (células finas): o detalhe
	// do material desloca ~0.6 m e o transporte anda até ~2 células por quadro
	constexpr int32 BoxMargin = 3;

	FLightingChannels VolumeChannels()
	{
		// como as explosões: só o canal 1 (sol e lua estão no 0 e no 1, a luz do fogo de dentro no 1)
		FLightingChannels C;
		C.bChannel0 = false;
		C.bChannel1 = true;
		C.bChannel2 = false;
		return C;
	}
}

UBattleSmokeSubsystem* UBattleSmokeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBattleSmokeSubsystem>() : nullptr;
}

void UBattleSmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	Noise = LoadObject<UVolumeTexture>(nullptr, NoisePath);
	if (!Base || !Noise)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlastLab: sem M_BattleSmoke/T_CurlNoise (rode Tools/blast.ps1 build-explosion): sem fumaça no campo"));
		return;
	}
	using namespace BattleSmoke;
	auto MakeTarget = [this](const FIntVector& Dim, const TCHAR* Name)
	{
		UTextureRenderTargetVolume* T = NewObject<UTextureRenderTargetVolume>(this, Name);
		T->bCanCreateUAV = true;
		T->ClearColor = FLinearColor::Transparent;
		T->Init(Dim.X, Dim.Y, Dim.Z, PF_FloatRGBA);
		T->UpdateResourceImmediate(true);
		return T;
	};
	FieldTarget = MakeTarget(FineDim, TEXT("BattleSmokeField"));
	VelTarget = MakeTarget(VelDim, TEXT("BattleSmokeVel"));
	for (int32 i = 0; i < MaxWarps; ++i)
	{
		WarpTargets.Add(MakeTarget(WarpDim, *FString::Printf(TEXT("ExplosionWarp%d"), i)));
	}

	// o volume que desenha: a caixa marchada encolhe pra fumaça (UpdateBox)
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	FieldActor = InWorld.SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(DomainMinM) * 100.0), Params);
	Volume = NewObject<UHeterogeneousVolumeComponent>(FieldActor, TEXT("BattleSmoke"));
	Volume->PrimaryComponentTick.bStartWithTickEnabled = false;     // sem quadros de Sparse Volume Texture pra tocar
	Volume->LightingChannels = VolumeChannels();
	Volume->bIssueBlockingRequests = false;
	FieldActor->SetRootComponent(Volume);
	Volume->SetWorldLocation(FVector(DomainMinM) * 100.0);
	Volume->RegisterComponent();
	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("Field"), FieldTarget);
	Material->SetTextureParameterValue(TEXT("Vel"), VelTarget);
	Material->SetTextureParameterValue(TEXT("Noise"), Noise);
	Volume->SetMaterial(0, Material);
	Volume->SetVisibility(false);
	UpdateBox(FIntVector(0), FineDim - FIntVector(1));
	Sim = MakeShared<FBattleSmokeSim, ESPMode::ThreadSafe>();
}

void UBattleSmokeSubsystem::Deinitialize()
{
	// o fluido mora no render thread: some depois dos comandos que já estão na fila
	if (Sim.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(BattleSmokeRelease)([Old = MoveTemp(Sim)](FRHICommandListImmediate&) mutable { Old.Reset(); });
	}
	Super::Deinitialize();
}

bool UBattleSmokeSubsystem::Covers(const FVector& PositionCm) const
{
	using namespace BattleSmoke;
	const FVector3f P = FVector3f(PositionCm / 100.0) - DomainMinM;
	return P.X >= 0.0f && P.Y >= 0.0f && P.X <= FineDim.X * FineCellM && P.Y <= FineDim.Y * FineCellM;
}

bool UBattleSmokeSubsystem::AttachWarp(AExplosionVolume* E)
{
	const UExplosionBake* B = E ? E->Bake.Get() : nullptr;
	if (!Sim.IsValid() || !B || !B->Velocity || WarpTargets.Num() < BattleSmoke::MaxWarps)
	{
		return false;
	}
	int32 Free = INDEX_NONE;
	for (int32 i = 0; i < BattleSmoke::MaxWarps; ++i)
	{
		if (WarpOwners[i].Get() == E)
		{
			return true;
		}
		if (Free == INDEX_NONE && (!WarpOwners[i].IsValid() || WarpOwners[i]->IsFinished()))
		{
			Free = i;
		}
	}
	if (Free == INDEX_NONE)
	{
		return false;
	}
	WarpOwners[Free] = E;
	WarpResetMask |= 1u << Free;
	E->SetWarp(WarpTargets[Free]);
	return true;
}
void UBattleSmokeSubsystem::AddDisturber(const FBattleSmokeDisturber& D)
{
	if (Disturbers.Num() < BattleSmoke::MaxDisturbers)
	{
		Disturbers.Add(D);
		// fumaça de motor entra no campo: conta pro "não está vazio"
		if (FMath::RoundToInt32(D.B.W) == 5)
		{
			MassEstimate += 0.02f;
		}
	}
}

void UBattleSmokeSubsystem::AddBlast(const FVector& PositionCm, float Scale)
{
	// ∇·u = S numa bola de raio R dá u_r(R) = S·R/3: ~18 m/s na borda de 9 m por
	// ~0.15 s, o pulso da onda de choque deslocando o ar uns metros (battlesmoke.js)
	const float R = BlastRadiusM(Scale);
	FBlast B;
	B.Blast.PosRadius = FVector4f(FVector3f(PositionCm / 100.0), R);
	B.Blast.Source = 3.0f * 18.0f / R;
	B.Until = Time + 0.15f;
	Blasts.Add(B);
}

int32 UBattleSmokeSubsystem::BeginWake(const FVector& StartM, const FVector& Dir, float Speed, const FVector& Right, float B0, float Gamma)
{
	FWakeRecord R;
	R.Id = NextWakeId++;
	// O par desce sozinho a w0 = Γ/(2π·b0); o núcleo tem ~0.05·b0 (Holzäpfel 2003)
	const float W0 = Gamma / (2.0f * PI * FMath::Max(B0, 0.1f));
	R.Wake.A = FVector4f(FVector3f(StartM), Time);
	R.Wake.B = FVector4f(FVector3f(Dir), Speed);
	R.Wake.C = FVector4f(FVector3f(Right), B0);
	R.Wake.D = FVector4f(Gamma, W0, 0.05f * B0, 0.0f);
	// só as mais novas: a mais velha já desceu e saiu do caminho
	if (Wakes.Num() >= BattleSmoke::MaxWakes)
	{
		Wakes.RemoveAt(0);
	}
	Wakes.Add(R);
	return R.Id;
}

void UBattleSmokeSubsystem::ExtendWake(int32 Id, float LengthM)
{
	for (FWakeRecord& R : Wakes)
	{
		if (R.Id == Id)
		{
			R.Wake.D.W = LengthM;
		}
	}
}

bool UBattleSmokeSubsystem::SmokeBox(FBox& Out) const
{
	using namespace BattleSmoke;
	if (bIdle || BoxMax.X < BoxMin.X)
	{
		return false;
	}
	const FVector Base = FVector(DomainMinM) * 100.0;
	Out = FBox(Base + FVector(BoxMin) * FineCellM * 100.0, Base + FVector(BoxMax + FIntVector(1)) * FineCellM * 100.0);
	return true;
}

bool UBattleSmokeSubsystem::SootHeight(float& OutCm) const
{
	using namespace BattleSmoke;
	float Cells = 0.0f;
	if (bIdle || !Sim.IsValid() || !Sim->GetSootHeight(Cells))
	{
		return false;
	}
	OutCm = (DomainMinM.Z + Cells * FineCellM) * 100.0f;
	return true;
}

void UBattleSmokeSubsystem::UpdateBox(const FIntVector& InMin, const FIntVector& InMax)
{
	using namespace BattleSmoke;
	const FIntVector Min(FMath::Clamp(InMin.X, 0, FineDim.X - 1), FMath::Clamp(InMin.Y, 0, FineDim.Y - 1), FMath::Clamp(InMin.Z, 0, FineDim.Z - 1));
	const FIntVector Max(FMath::Clamp(InMax.X, Min.X, FineDim.X - 1), FMath::Clamp(InMax.Y, Min.Y, FineDim.Y - 1), FMath::Clamp(InMax.Z, Min.Z, FineDim.Z - 1));
	BoxMin = Min;
	BoxMax = Max;
	if (!Volume || !Material)
	{
		return;
	}
	const FIntVector Size = Max - Min + FIntVector(1);
	FTransform Box = FTransform::Identity;
	Box.SetScale3D(FVector(FineCellM * 100.0f));                 // 1 voxel do volume = 1 célula fina
	Box.SetTranslation(FVector(Min) * FineCellM * 100.0f);
	Material->SetVectorParameterValue(TEXT("BoxMin"), FLinearColor(Min.X, Min.Y, Min.Z, 0.0f));
	Material->SetVectorParameterValue(TEXT("BoxSize"), FLinearColor(Size.X, Size.Y, Size.Z, 0.0f));
	if (Volume->VolumeResolution != Size || !Volume->FrameTransform.Equals(Box))
	{
		Volume->VolumeResolution = Size;
		Volume->FrameTransform = Box;
		Volume->UpdateBounds();
		Volume->MarkRenderStateDirty();
	}
}

void UBattleSmokeSubsystem::Tick(float DeltaTime)
{
	if (!Sim.IsValid() || !FieldTarget || !VelTarget || !Noise)
	{
		return;
	}
	const UBlastLabSimulation* BlastSim = UBlastLabSimulation::Get(this);
	const float Dt = BlastSim ? BlastSim->SimDelta(DeltaTime) : DeltaTime;
	Time += Dt;
	MassEstimate *= FMath::Exp(-Dt / SootTau);
	Blasts.RemoveAll([this](const FBlast& B) { return B.Until <= Time; });
	// a esteira some quando até o trecho mais novo já perdeu ~95% da força
	// (fase de difusão t0 = b0/w0, depois e^−(t−t0)/t0: 4·t0)
	Wakes.RemoveAll([this](const FWakeRecord& R)
	{
		const float T0 = R.Wake.C.W / FMath::Max(R.Wake.D.Y, 1e-3f);
		const float NewestBirth = R.Wake.A.W + R.Wake.D.W / FMath::Max(R.Wake.B.W, 1.0f);
		return Time - NewestBirth > 4.0f * T0;
	});

	// as explosões deformadas; o slot de uma que acabou fica livre
	TArray<FBattleSmokeWarp> Warps;
	for (int32 i = 0; i < BattleSmoke::MaxWarps; ++i)
	{
		AExplosionVolume* E = WarpOwners[i].Get();
		const UExplosionBake* B = E ? E->Bake.Get() : nullptr;
		if (!E || !B || !B->Velocity || E->IsFinished())
		{
			WarpOwners[i] = nullptr;
			continue;
		}
		FBattleSmokeWarp& W = Warps.AddDefaulted_GetRef();
		W.Slot = i;
		W.bReset = (WarpResetMask & (1u << i)) != 0;
		W.FootM = FVector3f(E->GetActorLocation() / 100.0);
		W.Scale = E->SizeScale;
		W.DomainLocalM = B->DomainCm / 100.0f;
		B->FramesAt(E->BakeTime(), W.Frame0, W.Frame1, W.Alpha);
		W.VelocityResolution = B->VelocityResolution;
		W.VelocityFrames = B->FrameTimes.Num();
		W.Velocity = B->Velocity->GetResource();
		W.Out = WarpTargets[i]->GameThread_GetRenderTargetResource();
	}
	WarpResetMask = 0;
	const bool bBusy = MassEstimate > IdleMass || Disturbers.Num() > 0 || Warps.Num() > 0;
	// fumaça própria no ar (desde o primeiro quadro do rastro de um míssil)
	const bool bSmoke = MassEstimate > 0.01f;
	// no aquecimento da abertura o volume (vazio) é desenhado: o material compila
	// junto com o das explosões, e não na primeira fumaça entregue
	if (BlastSim && BlastSim->IsPreparing() && bIdle)
	{
		// uma caixa mínima no meio do campo (a origem, onde as explosões aquecem):
		// fora da tela o volume não desenha e o material não compila
		const FIntVector Mid(BattleSmoke::FineDim.X / 2, BattleSmoke::FineDim.Y / 2, 0);
		UpdateBox(Mid - FIntVector(2, 2, 0), Mid + FIntVector(1, 1, 3));
		Volume->SetVisibility(true);
		return;
	}
	if (!bBusy)
	{
		if (!bIdle)
		{
			// o resto (invisível pela erosão) é zerado de uma vez e o campo para de custar
			bIdle = true;
			bNeedReset = true;
			MassEstimate = 0.0f;
		}
		Volume->SetVisibility(false);
		Disturbers.Reset();
		return;
	}
	bIdle = false;
	// só a deformação das explosões, sem fumaça própria no ar: nada pra desenhar
	Volume->SetVisibility(bSmoke);

	FBattleSmokeStepInputs In;
	In.Dt = Dt;
	In.Time = Time;
	In.bReset = bNeedReset;
	In.Warps = MoveTemp(Warps);
	In.Disturbers = MoveTemp(Disturbers);
	for (const FBlast& B : Blasts)
	{
		In.Blasts.Add(B.Blast);
	}
	for (const FWakeRecord& R : Wakes)
	{
		In.Wakes.Add(R.Wake);
	}
	In.Noise = Noise->GetResource();
	In.FieldOut = FieldTarget->GameThread_GetRenderTargetResource();
	In.VelOut = VelTarget->GameThread_GetRenderTargetResource();
	Disturbers.Reset();
	bNeedReset = false;
	ENQUEUE_RENDER_COMMAND(BattleSmokeStep)([S = Sim, Inputs = MoveTemp(In)](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		S->Step(GraphBuilder, Inputs);
		GraphBuilder.Execute();
	});

	// a caixa marchada acompanha a fumaça (lida da GPU com alguns quadros de atraso)
	FIntVector Lo, Hi;
	if (Sim->GetBounds(Lo, Hi))
	{
		Lo -= FIntVector(BoxMargin);
		Hi += FIntVector(BoxMargin);
		// só troca quando muda de verdade: cada troca recria o volume no render
		const bool bGrow = Lo.X < BoxMin.X || Lo.Y < BoxMin.Y || Lo.Z < BoxMin.Z || Hi.X > BoxMax.X || Hi.Y > BoxMax.Y || Hi.Z > BoxMax.Z;
		const bool bShrink = (Lo - BoxMin).GetMax() > 2 * BoxMargin || (BoxMax - Hi).GetMax() > 2 * BoxMargin;
		if (bGrow || bShrink)
		{
			UpdateBox(Lo, Hi);
		}
	}
	Material->SetScalarParameterValue(TEXT("Time"), Time);
}
