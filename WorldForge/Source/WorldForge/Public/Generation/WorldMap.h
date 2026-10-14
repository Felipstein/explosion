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
	/**
	 * Tipo da borda do mapa (EWorldFrame), −1 = sorteado pela semente. Com a
	 * simetria de giro, lados opostos têm o mesmo tipo.
	 */
	int32 Frame = -1;
	/** Vila do kit civil do deserto (casas, muros, rua, miudezas): 0 nenhuma, 1 uma
	 *  por lado do mapa (a segunda é a primeira girada como o mapa). */
	int32 Villages = 0;
	/** Tipo de relevo (docs/WORLDFORGE.md §10, fase 2): 0 morros e platôs (o de
	 *  sempre), 1 monólitos (o AlUla: miolo de areia aberto, um anel de pilares
	 *  de rocha com passagens largas e um rochedo no centro quando não há vila). */
	int32 ReliefType = 0;
	/** Áreas que a vila não pode tapar, além das bases e dos campos do próprio mapa
	 *  (as do jogo: largadas extras e depósitos de suprimento): x, y, raio (m). */
	TArray<FVector3f> KeepOut;

	/** Espaçamento da grade (m). 2 m: o detalhe de um penhasco de RTS (o Generals usa ~3 m). */
	static constexpr float CellSize = 2.0f;
	int32 GridSize() const { return FMath::RoundToInt(MapSize / CellSize) + 1; }
};

/**
 * A borda do mapa: o que emoldura a área jogável, no lugar do muro. É tipo de
 * mapa, não regra: cada mapa sorteia o seu (feedback de 07/10).
 */
enum class EWorldFrame : uint8
{
	Range = 0,     // serra nos quatro lados
	Open = 1,      // colinas (dunas no deserto, mata no temperado) que seguem além da borda
	Coast = 2,     // mar em dois lados opostos; serra ou colinas nos outros dois
	Island = 3,    // mar em volta
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

/** Um monólito do kit de malhas (Tools/blender_monoliths.py, Config/WorldForge/Monoliths.json): o terreno
 *  faz só o monte do pé; a vista ao vivo, a versão final e o jogo põem a malha. */
struct FWorldMonolith
{
	FString Mesh;                            // SM_Mono_<nome>
	FVector2f Pos = FVector2f::ZeroVector;   // m
	float Z = 0.0f;                          // m (o chão no centro)
	float Yaw = 0.0f;                        // graus
	float Scale = 1.0f;
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

/** Papel de uma trilha do gerador (a rede de estradas segue isto). */
enum class EWorldTrack : uint8
{
	Main = 0,      // base → centro (o caminho principal de cada jogador)
	Flank = 1,     // rota alternativa base → centro, longe da principal
	Link = 2,      // ligação extra (bases que não se alcançavam, platôs ilhados)
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
	/** Pra onde o vento sopra (graus, 0 = +X): as ondulações da areia e, no jogo, o rastro que enche. */
	float WindYaw = -68.0f;
	float WindSpeed = 5.0f;                   // m/s (o vento do deserto de dia: 3–8)

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
	TArray<float> Bed;                        // N×N, fundo dos leitos (wadi, córrego, riacho congelado), 0–1
	/** Bacias por vértice: 0 nenhuma, 1 salina (bacia seca do deserto), 2 oásis (lago no deserto),
	 *  3 lagoa seca pequena (barreiro de argila). */
	TArray<uint8> Basin;
	/** Região de cada jogador (célula → jogador mais próximo pelo caminho), −1 = fora. */
	TArray<int8> Territory;

	/** A borda sorteada e o tipo de cada lado (+Y, −Y, +X, −X): 1 serra, 2 colinas, 3 mar. */
	EWorldFrame Frame = EWorldFrame::Range;
	uint8 FrameSides[4] = { 1, 1, 1, 1 };

	TArray<FWorldStart> Starts;
	float BaseRadius = 50.0f;                  // m, o platô de cada base
	TArray<FWorldResource> Resources;
	TArray<FWorldProp> Props;
	TArray<FWorldMonolith> Monoliths;
	/** Trilhas esculpidas (base → centro, flancos, ligações), como linhas de pontos (m). */
	TArray<TArray<FVector2f>> Tracks;
	/** O papel de cada trilha (mesma ordem de Tracks). */
	TArray<EWorldTrack> TrackKinds;

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
