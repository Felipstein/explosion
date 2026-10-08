#pragma once

// Dados de design lidos dos JSON em Game/Config/RTS (relidos a cada partida, pra
// ajustar sem recompilar): as unidades viram Sim::Ruleset; o desenho de cada
// unidade e as informações do mapa ficam aqui do lado da Unreal.

#include "CoreMinimal.h"
#include "Sim/Defs.h"

struct FRTSUnitVisual
{
	FString Key;
	FString DisplayName;
	/** Malhas que andam com o casco (casco, esteiras, rodas). */
	TArray<FString> HullMeshes;
	/** Malhas que giram com a torre (torre, cano). */
	TArray<FString> TurretMeshes;
	float MuzzleHeightM = 2.0f;
	float RadiusM = 3.0f;
};

struct FRTSMapInfo
{
	FString Name;
	FString Title;
	float SizeM = 1024.0f;
	float CellM = 4.0f;
	float SampleM = 2.0f;
	float MaxSlope = 0.78f;
	float WaterBelowM = -1.5f;
	TArray<FVector2D> StartsM;
	/** O skirmish de só tanques começa no caminho entre as bases, a esta distância (pelo caminho) um do outro; 0 = nas bases. */
	float SkirmishGapM = 0.0f;
	TArray<FString> ObstacleMeshes;
	/** Unidades de cada jogador no começo do skirmish: (chave, quantidade). */
	TArray<TPair<FString, int32>> StartingUnits;
};

struct FRTSGameData
{
	Sim::Ruleset Rules;
	TArray<FRTSUnitVisual> Visuals;     // mesma ordem de Rules.Units
	TArray<FRTSMapInfo> Maps;

	/** Lê Units.json e Maps.json. Falso (com o motivo em OutError) se faltar arquivo ou campo. */
	bool Load(FString& OutError);

	const FRTSMapInfo* FindMap(const FString& Name) const;
	const FRTSUnitVisual* FindVisual(int32 DefIndex) const { return Visuals.IsValidIndex(DefIndex) ? &Visuals[DefIndex] : nullptr; }
};
