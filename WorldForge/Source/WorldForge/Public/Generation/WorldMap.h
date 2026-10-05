#pragma once

#include "CoreMinimal.h"

/**
 * Dados do mapa, separados de como ele é desenhado (como no motor SAGE do
 * Generals: grade de altura + texturas + passabilidade + objetos num arquivo;
 * o renderizador só lê). O núcleo do gerador só depende de Core: o mesmo
 * código gera o mapa no jogo a partir da semente.
 *
 * Unidades: metros. Grade quadrada de N×N vértices centrada na origem; o
 * vértice (I, J) fica em X = (I − (N−1)/2)·Cell, Y = (J − (N−1)/2)·Cell
 * (X e Y da Unreal; Z pra cima). Índice linear = J·N + I.
 */

enum class EWorldBiome : uint8
{
	Temperate,
	Desert,
	Winter,
};

/** Os controles do painel. Todos os números de 0 a 1 são "quanto" de cada coisa. */
struct WORLDFORGE_API FWorldGenSettings
{
	int32 Seed = 1;
	/** Lado do mapa jogável em metros (pequeno 768, médio 1024, grande 1536). */
	float MapSize = 1024.0f;
	/** Jogadores: 2 (simetria de rotação de 180°) ou 4 (de 90°). */
	int32 Players = 2;
	EWorldBiome Biome = EWorldBiome::Temperate;
	float Relief = 0.5f;      // montanhas e morros
	float Plateaus = 0.5f;    // platôs com penhascos (o "degrau" de RTS)
	float Water = 0.35f;      // lagos
	float Erosion = 0.5f;     // erosão hidráulica (vales e sulcos)
	float Forests = 0.5f;     // árvores
	float Resources = 0.5f;   // campos de recurso

	/** Espaçamento da grade (m). 2 m: o detalhe de um penhasco de RTS (o Generals usa ~3 m). */
	static constexpr float CellSize = 2.0f;
	int32 GridSize() const { return FMath::RoundToInt(MapSize / CellSize) + 1; }
};

enum class EWorldPropType : uint8
{
	Tree,
	Rock,
};

struct FWorldProp
{
	EWorldPropType Type = EWorldPropType::Tree;
	FVector2f Pos = FVector2f::ZeroVector;   // m
	float Z = 0.0f;                          // m (altura do chão no ponto)
	float Scale = 1.0f;
	float Yaw = 0.0f;                        // graus
	uint8 Variant = 0;                       // sorteio de forma/cor
};

struct FWorldStart
{
	int32 Player = 0;
	FVector2f Pos = FVector2f::ZeroVector;
	float Z = 0.0f;
};

struct FWorldResource
{
	FVector2f Pos = FVector2f::ZeroVector;
	float Z = 0.0f;
	float Radius = 10.0f;    // m
	float Amount = 1.0f;     // riqueza relativa (1 = campo padrão)
	int32 Owner = -1;        // jogador mais próximo, −1 = disputado (meio do mapa)
};

/** Passabilidade por célula (unidades de chão). */
enum class EWorldPass : uint8
{
	Open = 0,
	Cliff = 1,     // inclinação demais
	Water = 2,     // fundo demais
};

struct WORLDFORGE_API FWorldMap
{
	FWorldGenSettings Settings;
	int32 N = 0;
	float Cell = 2.0f;
	float WaterLevel = -1000.0f;              // m; abaixo disso é água

	TArray<float> Height;                     // N×N, m
	/**
	 * Pesos das camadas do chão por vértice (o material mistura nesta ordem):
	 * R = terra batida (bases, caminhos, campos de recurso)
	 * G = vegetação (umidade: vales, fluxo d'água) — grama no temperado
	 * B = areia/margem (perto da água)
	 * A = rocha (penhascos e encostas)
	 */
	TArray<FColor> Layers;
	TArray<EWorldPass> Pass;                  // N×N
	TArray<float> Flow;                       // N×N, fluxo d'água da erosão (0–1)
	/** Região de cada jogador (célula → jogador mais próximo pelo caminho), −1 = fora. */
	TArray<int8> Territory;

	TArray<FWorldStart> Starts;
	TArray<FWorldResource> Resources;
	TArray<FWorldProp> Props;
	/** Trilhas esculpidas (base → centro, flancos, ligações), como linhas de pontos (m). */
	TArray<TArray<FVector2f>> Tracks;

	/** Tempos por etapa (ms) e resultado das verificações, pro painel e o log. */
	TArray<TPair<FString, double>> Timings;
	bool bConnected = false;     // todos os jogadores se alcançam por terra
	int32 RampsCarved = 0;
	float OpenFraction = 0.0f;   // fração do mapa transitável
	float RushMeters = 0.0f;     // caminho por terra da base 1 até a base inimiga mais perto
	float RushStraight = 0.0f;   // a mesma distância em linha reta

	FORCEINLINE int32 Index(int32 I, int32 J) const { return J * N + I; }
	FORCEINLINE float Half() const { return 0.5f * float(N - 1) * Cell; }
	FORCEINLINE FVector2f CellPos(int32 I, int32 J) const { return FVector2f(I * Cell - Half(), J * Cell - Half()); }
	FORCEINLINE FVector2f ToGrid(FVector2f P) const { return (P + FVector2f(Half(), Half())) / Cell; }

	/** Altura no ponto (m), bilinear; fora do mapa repete a borda. */
	float HeightAt(FVector2f P) const;
	/** Normal da superfície no ponto (diferenças centrais). */
	FVector3f NormalAt(FVector2f P) const;
	bool IsInside(FVector2f P) const { return FMath::Abs(P.X) <= Half() && FMath::Abs(P.Y) <= Half(); }
};
