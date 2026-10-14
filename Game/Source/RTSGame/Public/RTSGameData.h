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
	/** O eixo da torre no casco (m, x e y): as malhas vêm com a origem do tanque, a torre gira aqui. */
	FVector2D TurretPivotM = FVector2D::ZeroVector;
	/** Lagarta (m): a bitola (centro a centro) e a largura de cada uma; zero = sem lagarta (não risca o chão). */
	FVector2D TracksM = FVector2D::ZeroVector;
	/** A cratera do tiro dela (13/10): 0 = A, 1 = B, 2 = C, −1 = nenhuma; e a parte dos tiros que passa raspando
	 *  e cava o chão além do alvo (só desenho, pelo hash do evento). */
	int32 ImpactCrater = -1;
	float NearMissShare = 0.0f;
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

/** Uma ordem do cenário: no segundo AtS de simulação, o jogador manda todas as unidades dele. */
struct FRTSScenarioOrder
{
	float AtS = 0.0f;
	int32 Player = 0;
	/** "line" (o arraste com direção), "move" ou "attack_move". */
	FString Type;
	FVector2D ToM = FVector2D::ZeroVector;
	float FacingDeg = 0.0f;
	float WidthM = 0.0f;
	/** A cratera da ordem "scar": 0 = A, 1 = B, 2 = C. */
	int32 MarkSize = 1;
	/** O grupo em formação, no passo do mais lento (o F segurado). */
	bool bFormation = false;
};

/** O teste rápido (12/10): a partida pronta, com semente fixa, ordens em passos fixos e câmera parada. */
struct FRTSScenario
{
	FString Name;
	FString Map;
	int32 Seed = 1;
	/** "off", "black" ou "explored". */
	FString Fog = TEXT("off");
	TArray<FVector2D> StartsM;
	TArray<TPair<FString, int32>> UnitsPerPlayer;
	TArray<FRTSScenarioOrder> Orders;
	FVector2D CameraFocusM = FVector2D::ZeroVector;
	float CameraDistanceM = 95.0f;
	float CameraYawDeg = 45.0f;
	/** A câmera desliza nesta velocidade (m/s) a partir do foco: o teste do cintilado em movimento (13/10). */
	FVector2D CameraPanMS = FVector2D::ZeroVector;
	/** A inclinação forçada (graus; < 0 = a curva do zoom do jogo): só pra foto lado a lado com a maquete do Look,
	 *  numa câmera que o jogador não tem (13/10). */
	float CameraPitchDeg = -1.0f;
	/** Falso: as fotos sem o HUD (a comparação com a maquete). */
	bool bHud = true;
	TArray<float> ShotsS;
	float EndS = 30.0f;
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
	/** A receita do look (plugin RTSLook, /RTSLook/Recipes/DA_Look_<nome>): o ambiente e a hora. Vazio = a luz do mapa. */
	FString LookRecipe;
	/** O skirmish de só tanques começa no caminho entre as bases, a esta distância (pelo caminho) um do outro; 0 = nas bases. */
	float SkirmishGapM = 0.0f;
	TArray<FString> ObstacleMeshes;
	/** A vila do WorldForge (Config/RTS/<arquivo>, o village.json que ele exporta ao montar o mapa): as peças que
	 *  bloqueiam viram células fechadas na grade. Vazio = mapa sem vila. */
	FString VillageFile;
	/** O tipo de chão do WorldForge (Config/RTS/<arquivo>.png, o surface.png que ele exporta ao montar o mapa, com o
	 *  .json ao lado): R asfalto, G terra batida, B rocha, a 1 m. Só desenho (a poeira, o sulco). Vazio = tudo areia. */
	FString SurfaceFile;
	/** A grade da simulação do WorldForge (Config/RTS/<arquivo>.png, o grid.png com o .json ao lado; 13/10): a água por
	 *  célula de 4 m (R 0 seco, 1 margem, 2 vau, 3 fundo, 4 gelo; G o corpo) e os corpos. Vazio = a água pela altura. */
	FString GridFile;
	/** Pra onde o vento sopra (graus; 0 = +X): enche o rastro do lado de onde ele vem e é a mesma direção das
	 *  ondulações da areia do Look (que lê daqui). */
	float WindYawDeg = 0.0f;
	/** A velocidade do vento (m/s, "wind_speed"): a fumaça, a poeira, as bandeiras e as palmeiras (MPC_Wind). */
	float WindSpeedMs = 4.0f;
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

	/** Um cenário de teste rápido (Config/RTS/Scenarios.json). Falso se não existir. */
	static bool LoadScenario(const FString& Name, struct FRTSScenario& Out, FString& OutError);

	/** Lê Units.json e Maps.json. Falso (com o motivo em OutError) se faltar arquivo ou campo. */
	bool Load(FString& OutError);

	const FRTSMapInfo* FindMap(const FString& Name) const;
	const FRTSUnitVisual* FindVisual(int32 DefIndex) const { return Visuals.IsValidIndex(DefIndex) ? &Visuals[DefIndex] : nullptr; }
};
