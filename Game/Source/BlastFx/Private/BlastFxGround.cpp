// Cópia do BlastLab (BlastLabGround, 14/10/2026): o chão pra quem a explosão do jogo consulta (a sombra da
// fumaça, os destroços). A original continua sendo a referência.
#include "BlastFxGround.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"

namespace
{
	// O traço só contra as colisões do terreno (ActorLineTraceSingle de cada
	// Landscape): as rochas, o capim e as árvores espalhados por cima ficam de fora
	bool TraceLandscape(const UWorld* World, const FVector& Start, const FVector& End, FVector& Out, bool& bHasLandscape)
	{
		bHasLandscape = false;
		double Best = TNumericLimits<double>::Max();
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(BlastFxGround), true);
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			bHasLandscape = true;
			FHitResult Hit;
			if (It->ActorLineTraceSingle(Hit, Start, End, ECC_Visibility, Params))
			{
				const double D = FVector::DistSquared(Start, Hit.ImpactPoint);
				if (D < Best)
				{
					Best = D;
					Out = Hit.ImpactPoint;
				}
			}
		}
		return Best < TNumericLimits<double>::Max();
	}
}

bool BlastFxGround::Ray(const UWorld* World, const FVector& Origin, const FVector& Dir, double MaxCm, FVector& Out)
{
	const FVector D = Dir.GetSafeNormal();
	if (!World || D.IsNearlyZero())
	{
		return false;
	}
	bool bHasLandscape = false;
	if (TraceLandscape(World, Origin, Origin + D * MaxCm, Out, bHasLandscape))
	{
		return true;
	}
	if (bHasLandscape || D.Z > -1e-4)
	{
		return false;                                  // fora do terreno, ou acima do horizonte
	}
	const double T = -Origin.Z / D.Z;                  // o campo de testes é o plano z = 0
	Out = Origin + D * T;
	return T <= MaxCm;
}

double BlastFxGround::HeightAt(const UWorld* World, double X, double Y)
{
	if (!World)
	{
		return 0.0;
	}
	// A altura direto do heightfield de colisão do pedaço debaixo do ponto (14/10): o traço do ActorLineTraceSingle
	// percorre os 256 pedaços do Landscape do jogo, e com 11 mortes juntas os destroços pediam centenas de nós novos
	// por quadro: 17 ms de processador em média, picos de 67 ms (perfil CSV, carcaca). O traço fica de reserva.
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if (const TOptional<float> H = It->GetHeightAtLocation(FVector(X, Y, 0.0)); H.IsSet())
		{
			return H.GetValue();
		}
	}
	FVector Hit;
	bool bHasLandscape = false;
	return TraceLandscape(World, FVector(X, Y, 1.0e6), FVector(X, Y, -1.0e6), Hit, bHasLandscape) ? Hit.Z : 0.0;
}

double BlastFxGround::HeightAtCached(const UWorld* World, double X, double Y)
{
	// Os destroços da morte de tanque consultavam o chão ~250 vezes por quadro, e
	// cada consulta traça contra cada pedaço de Landscape do mapa: ~16 ms de CPU
	// no jogo enquanto voavam (medido 07/10). Nós de 1 m, guardados por mundo
	constexpr double CellCm = 100.0;
	static TWeakObjectPtr<const UWorld> CachedWorld;
	static TMap<FIntPoint, float> Nodes;
	if (CachedWorld.Get() != World || Nodes.Num() > 500000)
	{
		CachedWorld = World;
		Nodes.Reset();
	}
	const double FX = X / CellCm, FY = Y / CellCm;
	const int32 IX = FMath::FloorToInt32(FX), IY = FMath::FloorToInt32(FY);
	auto Node = [World](int32 NX, int32 NY)
	{
		const FIntPoint Key(NX, NY);
		if (const float* H = Nodes.Find(Key))
		{
			return double(*H);
		}
		const double H = HeightAt(World, NX * CellCm, NY * CellCm);
		Nodes.Add(Key, float(H));
		return H;
	};
	const double AX = FX - IX, AY = FY - IY;
	return FMath::Lerp(FMath::Lerp(Node(IX, IY), Node(IX + 1, IY), AX), FMath::Lerp(Node(IX, IY + 1), Node(IX + 1, IY + 1), AX), AY);
}

bool BlastFxGround::Bounds(const UWorld* World, FBox& Out)
{
	Out.Init();
	if (World)
	{
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			Out += It->GetComponentsBoundingBox(true);
		}
	}
	return Out.IsValid != 0;
}
