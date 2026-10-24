#pragma once

// Os destroços que ficam no chão em volta da carcaça (13/10, a revisão da cratera: "4–8 peças por morte").
// A roda arrancada, o pedaço de lagarta, a chapa torta, a escotilha, a caixa lateral e as lascas
// (game.ps1 debris: Tools/blender_provisional.py, /Game/Structures/debris), espalhados pra longe de quem
// atirou, fora do casco, meio enterrados. Só desenho, como a memória do chão:
// - sem colisão (não roubam o clique nem o traço do cursor);
// - aparecem quando pousam (~1–1,6 s depois da morte, por baixo da bola de fogo);
// - em instâncias por célula de 64 m (uma só no mapa inteiro invalidaria a sombra virtual do mapa a cada morte);
// - baixos e chapados: de cima não podem ler como infantaria; as lascas pequenas não fazem sombra;
// - a posição sai de um sorteio pelo lugar da morte (a mesma em todo PC e toda partida).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSDebris.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class RTSGAME_API ARTSDebrisField : public AActor
{
	GENERATED_BODY()

public:
	ARTSDebrisField();
	/** Os destroços de uma morte de tanque. GroundZ: a altura do chão (cm) num ponto (cm). WreckId (22/10): a carcaça dona
	 *  das peças grandes (0 = nenhuma: ficam pra sempre, como as lascas). */
	void AddWreck(const FVector2D& AtCm, float HullYawDeg, const FVector2D& FromCm, TFunctionRef<float(const FVector2D&)> GroundZ, uint32 WreckId = 0);
	/** (22/10) As peças grandes da carcaça (a roda, a lagarta, a chapa, a escotilha, a caixa) afundam junto com ela (P01, o
	 *  mesmo progresso do casco) e somem quando ela sai do mapa; as lascas pequenas ficam no chão, como o queimado. */
	void SinkWreck(uint32 WreckId, float P01);
	void RemoveWreck(uint32 WreckId);
	/** As pedras de uma cratera (13/10, a prova aprovada do Look): os torrões do atlas viram pedra escaneada no mesmo lugar,
	 *  deitadas. Large/Small: x, y e a largura (cm), já no mundo; as miúdas só aparecem no zoom mais perto e não fazem
	 *  sombra. Seed: o sorteio da forma e do giro (o mesmo em todo PC). */
	void AddStones(const TArray<FVector>& Large, const TArray<FVector>& Small, uint32 Seed, TFunctionRef<float(const FVector2D&)> GroundZ);
	virtual void Tick(float DeltaSeconds) override;
	int32 NumPieces() const { return Placed; }

private:
	struct FPending
	{
		double AtSeconds = 0.0;
		int32 Mesh = 0;
		FTransform Transform;
		bool bSmall = false;   // a pedra miúda: num componente à parte, que some com a distância
		uint32 WreckId = 0;    // (22/10) a peça grande vai embora com esta carcaça (0 = fica)
	};
	/** (22/10) Uma peça grande pousada: onde está (componente e instância), a pose do pouso, quanto desce e quanto já desceu. */
	struct FWreckPiece
	{
		int32 Component = 0;
		int32 Instance = 0;
		FTransform Base;
		float DepthCm = 0.0f;
		float ShownCm = 0.0f;
	};
	TMap<uint32, TArray<FWreckPiece>> WreckPieces;
	/** As vagas das peças que sumiram (escondidas no lugar), por componente: a próxima peça dali usa a vaga, sem mexer no
	 *  índice das outras instâncias. */
	TMap<int32, TArray<int32>> FreeSlots;
	/** A carcaça dona de cada peça do Batch (na mesma ordem). */
	TMap<int32, TArray<uint32>> BatchOwners;
	FTransform StoneTransform(int32 Mesh, const FVector& Stone, FRandomStream& R, TFunctionRef<float(const FVector2D&)> GroundZ) const;
	void Place(const FPending& P);
	void LoadMeshes();

	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Meshes;
	/** O material de cada malha (nulo = o dela) e se faz sombra; as pedras começam em StoneFirst. */
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> MeshMaterials;
	TArray<bool> MeshShadow;
	/** A largura de cada malha no XY (cm), pra escalar a pedra pela largura do torrão. */
	TArray<float> MeshWidthCm;
	/** A caixa de cada malha (meia medida e o centro, cm, no espaço dela): pra deitar a pedra pela face maior. */
	TArray<FVector> MeshExtent;
	TArray<FVector> MeshOrigin;
	int32 StoneFirst = 0;
	int32 StonesPlaced = 0;
	/** A pedra menor que isto (cm) não vira malha (o Config/RTS/Ground/stones.json muda). */
	float MinStoneCm = 9.0f;
	/** As que pousam no quadro, por componente: entram juntas. */
	TMap<int32, TArray<FTransform>> Batch;
	int32 SmallStonesPlaced = 0;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
	/** Por célula de 64 m e malha: o índice em Components. */
	TMap<FIntVector, int32> CellComponents;
	TArray<FPending> Pending;
	int32 Placed = 0;
	bool bLoaded = false;
};
