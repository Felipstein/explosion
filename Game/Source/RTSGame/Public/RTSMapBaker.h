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
#include "Sim/World.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UInstancedStaticMeshComponent;

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
	/** O motivo de cada célula fechada (depuração, a imagem grade_motivos.png): 0 livre, 1 água, 2 penhasco pela
	 *  inclinação ou sem chão, 3 pedra, palmeira ou barreira (as malhas do Maps.json), 4 peça da vila. */
	TArray<uint8> CellReason;
	/** A água veio da grade do WorldForge (o hash dela conferido), e não da altura. */
	bool bWaterGrid = false;
	/** Células fechadas pela vila (casas, muros, carcaças, postes...). */
	int32 VillageCells = 0;
	/** O hash da vila no JSON e o do mapa montado (a etiqueta village_hash do ator WorldForgeVillage). */
	FString VillageHash;
	FString VillageHashInMap;
	double BakeSeconds = 0.0;

	/** O tipo de chão (13/10, o surface.png do WorldForge): RGBA por célula, a linha 0 no −Y. Só desenho. */
	TArray<FColor> Surface;
	int32 SurfaceRes = 0;
	float SurfaceCellM = 1.0f;
	FVector2D SurfaceMinM = FVector2D::ZeroVector;
	/** O A da grade é a margem molhada (só quando o .json lista o canal "a"; antes vinha 255 fixo). */
	bool bSurfaceWet = false;
	/** O peso de cada chão no ponto (cm), bilinear: X asfalto, Y terra batida, Z rocha, W molhado (0–1); sem a grade,
	 *  tudo zero (areia seca). Pra poeira (a areia não sobe no asfalto nem no molhado) e pra memória do chão (a esteira
	 *  não cava a rua). */
	FVector4f SurfaceAt(const FVector2D& Cm) const;
	/** Altura do chão (cm) no ponto (cm), bilinear na grade de amostras. */
	float GroundZCm(const FVector2D& Cm) const;
	/** Normal do chão no ponto (diferenças centrais), pra inclinar o tanque. */
	FVector GroundNormal(const FVector2D& Cm) const;

	/** (16/10) Os carros e as árvores que o tanque esmaga e derruba (docs/ESMAGAR_E_DERRUBAR.md), em ordem fixa (o índice
	 *  é o do evento), e de onde cada um é desenhado: a peça da vila (o id no village.json, −1 nenhuma) ou a instância da
	 *  árvore no mapa. */
	TArray<Sim::PropSetup> Props;
	TArray<int32> PropPiece;
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> PropIsm;
	TArray<int32> PropIsmIndex;
	int32 TreeProps = 0;
	int32 CarProps = 0;
};

namespace RTSMapBaker
{
	bool Bake(UWorld* World, const FRTSMapInfo& Info, FRTSBakedMap& Out, FString& OutError);
}
