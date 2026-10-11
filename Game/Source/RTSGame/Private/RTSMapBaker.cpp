#include "RTSMapBaker.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "LandscapeProxy.h"
#include "RTSGameData.h"

namespace
{
	constexpr float NoGround = -100000.0f;

	bool IsObstacleMesh(const UStaticMesh* Mesh, const TArray<FString>& Keywords)
	{
		if (!Mesh)
		{
			return false;
		}
		const FString Path = Mesh->GetPathName();
		for (const FString& K : Keywords)
		{
			if (Path.Contains(K))
			{
				return true;
			}
		}
		return false;
	}
}

float FRTSBakedMap::GroundZCm(const FVector2D& Cm) const
{
	if (Samples <= 1)
	{
		return 0.0f;
	}
	const double GX = FMath::Clamp((Cm.X / 100.0 - OriginM.X) / SampleM, 0.0, double(Samples - 1) - 1e-3);
	const double GY = FMath::Clamp((Cm.Y / 100.0 - OriginM.Y) / SampleM, 0.0, double(Samples - 1) - 1e-3);
	const int32 X = int32(GX);
	const int32 Y = int32(GY);
	const float FX = float(GX - X);
	const float FY = float(GY - Y);
	auto H = [this](int32 I, int32 J) { return HeightsM[J * Samples + I]; };
	const float Top = FMath::Lerp(H(X, Y), H(X + 1, Y), FX);
	const float Bottom = FMath::Lerp(H(X, Y + 1), H(X + 1, Y + 1), FX);
	return FMath::Lerp(Top, Bottom, FY) * 100.0f;
}

FVector FRTSBakedMap::GroundNormal(const FVector2D& Cm) const
{
	const float D = SampleM * 100.0f;
	const float DX = GroundZCm(Cm + FVector2D(D, 0)) - GroundZCm(Cm - FVector2D(D, 0));
	const float DY = GroundZCm(Cm + FVector2D(0, D)) - GroundZCm(Cm - FVector2D(0, D));
	return FVector(-DX, -DY, 2.0f * D).GetSafeNormal();
}

bool RTSMapBaker::Bake(UWorld* World, const FRTSMapInfo& Info, FRTSBakedMap& Out, FString& OutError)
{
	const double Start = FPlatformTime::Seconds();
	Out = FRTSBakedMap();

	TArray<ALandscapeProxy*> Landscapes;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Landscapes.Add(*It);
	}
	if (Landscapes.IsEmpty())
	{
		OutError = TEXT("o mapa não tem Landscape");
		return false;
	}

	// 1. alturas a cada SampleM (a mesma grade de 2 m do WorldForge)
	const float Half = Info.SizeM * 0.5f;
	Out.SampleM = Info.SampleM;
	Out.Samples = FMath::RoundToInt(Info.SizeM / Info.SampleM) + 1;
	Out.OriginM = FVector2D(-Half, -Half);
	Out.HeightsM.SetNumUninitialized(Out.Samples * Out.Samples);
	int32 Missing = 0;
	for (int32 J = 0; J < Out.Samples; ++J)
	{
		for (int32 I = 0; I < Out.Samples; ++I)
		{
			const FVector P((Out.OriginM.X + I * Info.SampleM) * 100.0, (Out.OriginM.Y + J * Info.SampleM) * 100.0, 0.0);
			float Z = NoGround;
			for (const ALandscapeProxy* L : Landscapes)
			{
				if (const TOptional<float> H = L->GetHeightAtLocation(P))
				{
					Z = H.GetValue() / 100.0f;
					break;
				}
			}
			Missing += Z == NoGround ? 1 : 0;
			Out.HeightsM[J * Out.Samples + I] = Z;
		}
	}

	// 2. células de 4 m: penhasco pela inclinação (diferença central, como o gerador), água pela altura
	const int32 CellsPerSide = FMath::RoundToInt(Info.SizeM / Info.CellM);
	const int32 Step = FMath::Max(1, FMath::RoundToInt(Info.CellM / Info.SampleM));
	Sim::SimMap& M = Out.Logic;
	M.Resize(CellsPerSide, CellsPerSide, Sim::Fix::FromRatio(FMath::RoundToInt(Info.CellM * 1000), 1000),
		Sim::Vec2::Make(Sim::Fix::FromInt(-FMath::RoundToInt(Half)), Sim::Fix::FromInt(-FMath::RoundToInt(Half))));
	auto H = [&Out](int32 I, int32 J)
	{
		I = FMath::Clamp(I, 0, Out.Samples - 1);
		J = FMath::Clamp(J, 0, Out.Samples - 1);
		return Out.HeightsM[J * Out.Samples + I];
	};
	auto IsCliff = [&](int32 I, int32 J)
	{
		const float GX = (H(I + 1, J) - H(I - 1, J)) / (2.0f * Info.SampleM);
		const float GY = (H(I, J + 1) - H(I, J - 1)) / (2.0f * Info.SampleM);
		return GX * GX + GY * GY > Info.MaxSlope * Info.MaxSlope || H(I, J) == NoGround;
	};
	// a menor e a maior altura de cada célula, em decímetros: a regra de construir no plano (ZH)
	M.MinDm.assign(M.Pass.size(), 0);
	M.MaxDm.assign(M.Pass.size(), 0);
	for (int32 CY = 0; CY < CellsPerSide; ++CY)
	{
		for (int32 CX = 0; CX < CellsPerSide; ++CX)
		{
			// as amostras dentro da célula (3×3 com célula de 4 m e amostra de 2 m)
			int32 Cliffs = 0;
			int32 Count = 0;
			float Lo = TNumericLimits<float>::Max();
			float Hi = TNumericLimits<float>::Lowest();
			for (int32 DJ = 0; DJ <= Step; ++DJ)
			{
				for (int32 DI = 0; DI <= Step; ++DI)
				{
					Cliffs += IsCliff(CX * Step + DI, CY * Step + DJ) ? 1 : 0;
					++Count;
					const float Z = H(CX * Step + DI, CY * Step + DJ);
					Lo = FMath::Min(Lo, Z);
					Hi = FMath::Max(Hi, Z);
				}
			}
			M.MinDm[size_t(M.Index(CX, CY))] = int16(FMath::Clamp(FMath::FloorToInt(Lo * 10.0f), -32000, 32000));
			M.MaxDm[size_t(M.Index(CX, CY))] = int16(FMath::Clamp(FMath::CeilToInt(Hi * 10.0f), -32000, 32000));
			const float Center = H(CX * Step + Step / 2, CY * Step + Step / 2);
			uint8 Pass = uint8(Sim::CellPass::Open);
			if (Center < Info.WaterBelowM)
			{
				Pass = uint8(Sim::CellPass::Water);
				++Out.WaterCells;
			}
			else if (Cliffs * 2 >= Count)
			{
				Pass = uint8(Sim::CellPass::Blocked);
				++Out.CliffCells;
			}
			M.Pass[M.Index(CX, CY)] = Pass;
		}
	}

	// 3. pedras grandes, paredões, palmeiras e barreiras viram célula bloqueada
	auto BlockFootprint = [&](const FBox& Bounds)
	{
		const FVector C = Bounds.GetCenter();
		const FVector E = Bounds.GetExtent() * 0.85f;   // a caixa é maior que a pedra
		if (E.X < 100.0f && E.Y < 100.0f)
		{
			return;   // menor que 2 m: o tanque passa por cima
		}
		const Sim::Cell Lo = M.CellOf(Sim::Vec2::Make(Sim::Fix::FromInt(FMath::FloorToInt((C.X - E.X) / 100.0f)), Sim::Fix::FromInt(FMath::FloorToInt((C.Y - E.Y) / 100.0f))));
		const Sim::Cell Hi = M.CellOf(Sim::Vec2::Make(Sim::Fix::FromInt(FMath::CeilToInt((C.X + E.X) / 100.0f)), Sim::Fix::FromInt(FMath::CeilToInt((C.Y + E.Y) / 100.0f))));
		for (int32 Y = FMath::Max(Lo.Y, 0); Y <= FMath::Min(Hi.Y, M.Height - 1); ++Y)
		{
			for (int32 X = FMath::Max(Lo.X, 0); X <= FMath::Min(Hi.X, M.Width - 1); ++X)
			{
				// dentro da elipse inscrita na caixa
				const Sim::Vec2 CellCenter = M.CellCenter(Sim::Cell{ X, Y });
				const float DX = (float(CellCenter.X.ToDouble()) * 100.0f - C.X) / E.X;
				const float DY = (float(CellCenter.Y.ToDouble()) * 100.0f - C.Y) / E.Y;
				if (DX * DX + DY * DY <= 1.0f && M.Pass[M.Index(X, Y)] == uint8(Sim::CellPass::Open))
				{
					M.Pass[M.Index(X, Y)] = uint8(Sim::CellPass::Blocked);
					++Out.ObstacleCells;
				}
			}
		}
	};
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* Comp : Meshes)
		{
			if (!IsObstacleMesh(Comp->GetStaticMesh(), Info.ObstacleMeshes))
			{
				continue;
			}
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				const FBox Local = ISM->GetStaticMesh()->GetBoundingBox();
				for (int32 K = 0; K < ISM->GetInstanceCount(); ++K)
				{
					FTransform T;
					if (ISM->GetInstanceTransform(K, T, /*bWorldSpace=*/true))
					{
						BlockFootprint(Local.TransformBy(T));
					}
				}
			}
			else
			{
				BlockFootprint(Comp->Bounds.GetBox());
			}
		}
	}

	M.ComputeClearance();
	Out.BakeSeconds = FPlatformTime::Seconds() - Start;
	UE_LOG(LogTemp, Log, TEXT("RTS mapa %s: %d×%d células de %.0f m; penhasco %d, água %d, pedras %d, sem chão %d amostras; %.2f s; hash %016llx"),
		*Info.Name, M.Width, M.Height, Info.CellM, Out.CliffCells, Out.WaterCells, Out.ObstacleCells, Missing, Out.BakeSeconds,
		(unsigned long long)M.Hash());
	return true;
}
