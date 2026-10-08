#pragma once

// A grade lógica do mapa, tirada do próprio nível ao carregar: alturas do
// Landscape a cada 2 m e as pedras grandes (malhas da lista do Maps.json). As
// regras de passagem são as do gerador do WorldForge (penhasco acima de 0,78 de
// inclinação, água abaixo de −1,5 m).
//
// É provisório: o certo é o WorldForge exportar a grade canônica junto com o
// mapa (docs/JOGO.md §3.6), pra todo PC ler o mesmo arquivo. Enquanto isso, no
// skirmish local o resultado é o mesmo a cada carga no mesmo PC.

#include "CoreMinimal.h"
#include "Sim/SimMap.h"

class UWorld;
struct FRTSMapInfo;

struct FRTSBakedMap
{
	Sim::SimMap Logic;

	/** Alturas (m) numa grade de SampleM, Samples×Samples, a partir de OriginM. Só pro desenho. */
	TArray<float> HeightsM;
	int32 Samples = 0;
	float SampleM = 2.0f;
	FVector2D OriginM = FVector2D::ZeroVector;

	int32 CliffCells = 0;
	int32 WaterCells = 0;
	int32 ObstacleCells = 0;
	double BakeSeconds = 0.0;

	/** Altura do chão (cm) no ponto (cm), bilinear na grade de amostras. */
	float GroundZCm(const FVector2D& Cm) const;
	/** Normal do chão no ponto (diferenças centrais), pra inclinar o tanque. */
	FVector GroundNormal(const FVector2D& Cm) const;
};

namespace RTSMapBaker
{
	bool Bake(UWorld* World, const FRTSMapInfo& Info, FRTSBakedMap& Out, FString& OutError);
}
