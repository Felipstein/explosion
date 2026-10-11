#pragma once

// O depósito de suprimentos do mapa (e a pilha de caixas que cai do caminhão destruído).
// Só desenho: o estoque vem da simulação. As caixas são instâncias de uma malha só e somem
// na proporção do que sobrou, as de cima primeiro (o W3DSupplyDraw do ZH esconde as caixas do
// modelo do mesmo jeito: caixas mostradas = total × estoque ÷ estoque inicial, arredondado pra cima).

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSDockActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
struct FRTSDockVisual;

UCLASS()
class RTSGAME_API ARTSDockActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSDockActor();

	/** bPile = a pilha caída (só as caixas, sem o pátio). */
	void Setup(const FRTSDockVisual& Visual, bool bInPile, uint32 Seed);
	/** O estoque agora e o do começo (caixas da simulação). */
	void SetBoxes(int32 Boxes, int32 StartBoxes);
	bool IsPile() const { return bPile; }

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Yard;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Crates;

	/** Onde fica cada caixa, na ordem em que somem. */
	TArray<FTransform> Slots;
	bool bPile = false;
	int32 Shown = -1;
};
