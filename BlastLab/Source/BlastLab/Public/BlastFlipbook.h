#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlastFlipbook.generated.h"

class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UTexture;

/**
 * A bola de fogo pré-renderizada (teste de 07/10, a pedido do Felipe: "parece
 * jogo de 2003"; a prévia no Cycles da mesma simulação era muito melhor que o
 * volume ao vivo). Como os AAA fazem: a simulação é renderizada antes, com a
 * luz da cena (Tools/blender_flipbook.py, da sessão de arte), e o jogo só toca
 * o atlas num cartão virado pra câmera. -BallFlipbook=1 na Morte de Tanque.
 *
 * O atlas (Tools/build_flipbook.py): Cols × Rows quadros a Fps, RGBA em float
 * com alfa pré-multiplicado, quadro de SizeM × SizeM com o chão a GroundM da
 * borda de baixo. M_FlipbookCard mistura os dois quadros em volta do instante.
 */
UCLASS()
class BLASTLAB_API ABlastFlipbook : public AActor
{
	GENERATED_BODY()

public:
	ABlastFlipbook();

	/**
	 * Começa no pé (cm) com o atlas escolhido (e o do fogo, se vier separado).
	 * bDisplay: o atlas já vem em cor de tela (o AgX da prévia, M_FlipbookCardDisplay).
	 */
	void Start(UTexture* Atlas, UTexture* Emission, int32 InCols, int32 InRows, int32 InFrames, float InFps, float InSizeM, float InGroundM,
		bool bDisplay = false);

	/**
	 * O flipbook com luz de 6 direções (M_SixWay, Tools/build_sixway.py): a fumaça
	 * reacendida pela luz da cena. A, B e MV são os atlas; o centro da célula fica
	 * CenterM acima do pé (o alvo da câmera de bake).
	 */
	void StartSixWay(UTexture* A, UTexture* B, UTexture* MV, int32 InCols, int32 InRows, int32 InFrames, float InFps, float InSizeM, float InCenterM);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMaterialBillboardComponent> Card;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	int32 Cols = 8, Rows = 6, Frames = 42;
	float Fps = 15.0f, SizeM = 18.0f, GroundM = 0.5f;
	float Age = 0.0f;
};
