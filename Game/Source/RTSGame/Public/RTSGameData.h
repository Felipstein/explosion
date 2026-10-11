#pragma once

// Dados de design lidos dos JSON em Game/Config/RTS (relidos a cada partida, pra
// ajustar sem recompilar): as unidades viram Sim::Ruleset; o desenho de cada
// unidade e as informações do mapa ficam aqui do lado da Unreal.

#include "CoreMinimal.h"
#include "Sim/Defs.h"

/** Peça que gira sozinha (ventilador, antena de radar): a origem da malha é o eixo (Z). */
struct FRTSSpinPart
{
	FString Mesh;
	FVector AtCm = FVector::ZeroVector;
	float Rpm = 30.0f;
};

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
	/** Prédio: as peças que giram, e o andaime da obra (só aparece enquanto constrói). */
	TArray<FRTSSpinPart> SpinParts;
	FString ScaffoldMesh;
	/** Dozer: a lâmina, que sobe e desce quando ele trabalha (gira no Y do pivô). */
	FString BladeMesh;
	FVector BladeAtCm = FVector::ZeroVector;
	/** Caminhão: uma caixa em cada vaga; mostra quantas ele leva. */
	FString CargoMesh;
	TArray<FVector> CargoSlotsCm;
};

/** O desenho do depósito de suprimentos (o mesmo em todo mapa) e da pilha de caixas caídas. */
struct FRTSDockVisual
{
	FString Mesh;
	FString CrateMesh;
	/** Onde ficam as caixas do depósito, já na ordem em que somem (as de cima primeiro, espalhado). */
	TArray<FVector> CrateSlotsCm;
	/** A pilha que cai do caminhão destruído: caixa por caixa (posição e giro em graus no W). */
	TArray<FVector4> PileSlotsCm;
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
	/** Com base (ZH): o prédio de largada (o Command Center pronto); vazio = skirmish de só tanques. */
	FString BaseStructure;
	/** Os depósitos de suprimento (até o WorldForge exportar os dele): centro (m), tamanho e caixas. */
	TArray<FVector2D> DocksM;
	FVector2D DockSizeM = FVector2D(26.33, 32.12);
	int32 DockBoxes = 400;
};

struct FRTSGameData
{
	Sim::Ruleset Rules;
	TArray<FRTSUnitVisual> Visuals;     // mesma ordem de Rules.Units
	TArray<FRTSMapInfo> Maps;
	FRTSDockVisual DockVisual;

	/** Lê Units.json e Maps.json. Falso (com o motivo em OutError) se faltar arquivo ou campo. */
	bool Load(FString& OutError);

	const FRTSMapInfo* FindMap(const FString& Name) const;
	const FRTSUnitVisual* FindVisual(int32 DefIndex) const { return Visuals.IsValidIndex(DefIndex) ? &Visuals[DefIndex] : nullptr; }
};
