#include "RTSStrikeFx.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sim/Types.h"

namespace
{
	// o anel (o spec v4, power_cruise_strike: 4 s; o raio é o do girassol, 12 m e 14 m na patente 4)
	constexpr double RingLifeS = 4.0;
	constexpr double RingFadeInS = 0.15;
	constexpr double RingFadeOutS = 0.6;
	/** O piscar lento (o anel vivo, sem virar pisca-alerta): 1,25 vez por segundo, entre 75% e 100%. */
	constexpr double RingPulseHz = 1.25;
	constexpr float RingPulseDepth = 0.25f;
	/** A largura da fita: ~4 px na câmera do jogo a ~100 m (≈ 0,1 m por pixel), a leitura das linhas do HUD (1,5–2 px) com folga. */
	constexpr float RingWidthCm = 45.0f;
	/** Acima do chão (o relevo entre duas amostras não engole a fita). */
	constexpr float RingLiftCm = 25.0f;
	constexpr int32 RingSegments = 64;
	/** As 4 marcas pro centro; a do lado de quem lançou é mais comprida (os mísseis chegam de lá). */
	constexpr float TickLengthCm = 160.0f;
	constexpr float FromTickScale = 1.8f;
	/** O pico da radiância no M_Glow (× 128000 cd/m², Game/Tools/build_content.py): o nível do traçante (RTSEffect.cpp), que na
	 *  exposição do look só passa do branco no canal forte (o vermelho fica vermelho, sem estourar pra creme). */
	constexpr float RingPeakRadiance = 0.16f;

	FLinearColor RingRadiance(const FLinearColor& C)
	{
		const float Max = FMath::Max3(C.R, C.G, C.B);
		return Max > 1e-4f ? FLinearColor(C.R / Max, C.G / Max, C.B / Max) * RingPeakRadiance : FLinearColor(RingPeakRadiance, 0.0f, 0.0f);
	}

	/** Um quadrilátero (A, B no começo; C, D no fim) virado pra cima, nas duas faces (o M_Glow é de uma face só). */
	void AddQuad(TArray<FVector>& V, TArray<int32>& T, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 I = V.Num();
		V.Add(A);
		V.Add(B);
		V.Add(C);
		V.Add(D);
		T.Append({ I, I + 1, I + 2, I + 1, I + 3, I + 2 });
		T.Append({ I, I + 2, I + 1, I + 1, I + 2, I + 3 });
	}
}

FRTSStrikeRings::~FRTSStrikeRings()
{
	Shutdown();
}

void FRTSStrikeRings::Add(UWorld* World, const FVector2D& CenterCm, float RadiusCm, const FLinearColor& Color, float FromYawDeg, double NowTicks,
	TFunctionRef<float(const FVector2D&)> GroundZCm, const FVector* ViewCm)
{
	if (!World || RadiusCm <= 0.0f)
	{
		return;
	}
	if (!Material.IsValid())
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glow.M_Glow"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS anel do ataque: sem o /Game/Materials/M_Glow (game.ps1 content); o anel não aparece"));
			return;
		}
		Material.Reset(M);
	}
	const float Z0 = GroundZCm(CenterCm);
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!A)
	{
		return;
	}
	// (o ator sem raiz não guarda o lugar do SpawnActor: a fita é a raiz, posta no centro)
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(A, TEXT("StrikeRing"));
	A->SetRootComponent(Mesh);
	Mesh->SetWorldLocation(FVector(CenterCm.X, CenterCm.Y, Z0));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->bUseAsOccluder = false;
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetTranslucentSortPriority(3);
	Mesh->RegisterComponent();

	// a fita (no espaço do ator: o centro no chão), cada vértice na altura do chão dele
	const auto At = [&](double AngRad, double R) -> FVector
	{
		const FVector2D P = CenterCm + FVector2D(FMath::Cos(AngRad), FMath::Sin(AngRad)) * R;
		return FVector(P.X - CenterCm.X, P.Y - CenterCm.Y, GroundZCm(P) + RingLiftCm - Z0);
	};
	TArray<FVector> V;
	TArray<int32> T;
	const double In = FMath::Max(0.0, double(RadiusCm) - RingWidthCm * 0.5);
	const double Out = double(RadiusCm) + RingWidthCm * 0.5;
	for (int32 K = 0; K < RingSegments; ++K)
	{
		const double A0 = 2.0 * PI * double(K) / RingSegments;
		const double A1 = 2.0 * PI * double(K + 1) / RingSegments;
		AddQuad(V, T, At(A0, In), At(A0, Out), At(A1, In), At(A1, Out));
	}
	// as 4 marcas: a 1ª no lado de quem lançou (o rumo dele pro ponto + 180°)
	const double FromRad = FMath::DegreesToRadians(double(FromYawDeg) + 180.0);
	for (int32 J = 0; J < 4; ++J)
	{
		const double Ang = FromRad + double(J) * UE_HALF_PI;
		const FVector2D Rd(FMath::Cos(Ang), FMath::Sin(Ang));
		const FVector2D Tg(-Rd.Y, Rd.X);
		const double R1 = In;
		const double R0 = FMath::Max(0.0, R1 - TickLengthCm * (J == 0 ? FromTickScale : 1.0f));
		const auto P = [&](double R, double Side) -> FVector
		{
			const FVector2D Q = CenterCm + Rd * R + Tg * (Side * RingWidthCm * 0.5);
			return FVector(Q.X - CenterCm.X, Q.Y - CenterCm.Y, GroundZCm(Q) + RingLiftCm - Z0);
		};
		AddQuad(V, T, P(R0, -1.0), P(R0, 1.0), P(R1, -1.0), P(R1, 1.0));
	}
	// as normais pra câmera (o (N·V)² do M_Glow): a fita deitada acende inteira na vista do jogo
	TArray<FVector> N;
	N.Init(FVector::UpVector, V.Num());
	if (ViewCm)
	{
		const FVector Origin(CenterCm.X, CenterCm.Y, Z0);
		for (int32 K = 0; K < V.Num(); ++K)
		{
			N[K] = (*ViewCm - (Origin + V[K])).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		}
	}
	TArray<FVector2D> UV;
	UV.Init(FVector2D::ZeroVector, V.Num());
	TArray<FLinearColor> Colors;
	Colors.Init(FLinearColor::White, V.Num());
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), V.Num());
	Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, Colors, Tangents, false);
	Mesh->SetMaterial(0, Material.Get());
	Mesh->SetCustomPrimitiveDataVector3(0, FVector::ZeroVector);   // nasce apagado: acende no Tick

	FRing& R = Rings.AddDefaulted_GetRef();
	R.Actor = A;
	R.Mesh = Mesh;
	R.BornTicks = NowTicks;
	R.Radiance = RingRadiance(Color);
}

void FRTSStrikeRings::Tick(double NowTicks)
{
	for (int32 I = Rings.Num() - 1; I >= 0; --I)
	{
		FRing& R = Rings[I];
		const double Age = (NowTicks - R.BornTicks) / double(Sim::TicksPerSecond);
		UProceduralMeshComponent* Mesh = R.Mesh.Get();
		if (!Mesh || Age >= RingLifeS)
		{
			if (AActor* A = R.Actor.Get())
			{
				A->Destroy();
			}
			Rings.RemoveAtSwap(I);
			continue;
		}
		const double In = FMath::Clamp(Age / RingFadeInS, 0.0, 1.0);
		const double Out = FMath::Clamp((RingLifeS - Age) / RingFadeOutS, 0.0, 1.0);
		const double Pulse = 1.0 - RingPulseDepth * 0.5 * (1.0 - FMath::Cos(2.0 * PI * RingPulseHz * FMath::Max(0.0, Age)));
		const float Level = float(In * Out * Pulse);
		if (FMath::Abs(Level - R.LastLevel) > 0.005f)
		{
			R.LastLevel = Level;
			const FLinearColor C = R.Radiance * Level;
			Mesh->SetCustomPrimitiveDataVector3(0, FVector(C.R, C.G, C.B));
		}
	}
}

void FRTSStrikeRings::Shutdown()
{
	for (const FRing& R : Rings)
	{
		if (AActor* A = R.Actor.Get())
		{
			A->Destroy();
		}
	}
	Rings.Reset();
	Material.Reset();
}
