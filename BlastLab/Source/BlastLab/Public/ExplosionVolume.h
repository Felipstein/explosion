#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "ExplosionVolume.generated.h"

class UHeterogeneousVolumeComponent;
class UMaterialInstanceDynamic;

/**
 * Transição contínua entre os quadros gravados de uma explosão.
 *
 * O Heterogeneous Volume liga ao material um quadro só do volume animado, o
 * floor(Frame) (UHeterogeneousVolumeComponent::TickComponent): a fumaça anda
 * aos degraus, um por quadro gravado (o bake tem 56 em 6 s, ~0.2 s entre os
 * últimos). Este componente liga também o quadro SEGUINTE e a fração entre os
 * dois — o caminho que a própria Epic deixou pra isso
 * (UAnimatedSparseVolumeTextureController::GetCurrentFramesForInterpolation) —
 * e o material mistura os dois levando cada um pelos vetores de movimento até
 * o instante pedido, como o WebGL fazia (reference/webgl/js/instances.js bakeAtMV).
 *
 * Ele também encolhe a caixa que o volume marcha pra caixa justa da fumaça
 * daquele quadro. O Heterogeneous Volume marcha a caixa inteira do volume
 * (a união de todos os quadros, 38 m) voxel a voxel, avaliando o material a
 * cada passo, sem pular o vazio: o custo é proporcional ao volume da caixa, e
 * a fumaça ocupa de ~1% a ~33% dela. O dado do volume não muda — o material
 * continua lendo nas coordenadas do domínio inteiro.
 *
 * Por isso este componente assume a reprodução que o volume fazia no próprio
 * tick (que fica desligado): pede os quadros ao streaming, escreve os
 * parâmetros e a caixa. Parâmetros do material: SparseVolumeTexture (quadro de
 * baixo), SparseVolumeTextureNext (quadro de cima), FrameAlpha (fração entre
 * eles), FrameInterval (segundos entre os dois), BoxMin e BoxSize (caixa
 * marchada, em voxels do domínio).
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class BLASTLAB_API UFrameBlendComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrameBlendComponent();

	/** Instante de cada quadro gravado, em segundos (o bake não é uniforme no tempo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TArray<float> FrameTimes;

	/** Caixa marchada de cada quadro (união da fumaça dele e do seguinte): canto mínimo, em voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TArray<FIntVector> BoxMin;

	/** Tamanho da caixa marchada de cada quadro, em voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TArray<FIntVector> BoxSize;

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UHeterogeneousVolumeComponent* Volume() const;
	void ApplyBox(UHeterogeneousVolumeComponent* V, UMaterialInstanceDynamic* MID, const FTransform& Base, int32 Frame);
};

/** Uma explosão gravada: o volume + a transição entre os quadros. */
UCLASS()
class BLASTLAB_API AExplosionVolume : public AActor
{
	GENERATED_BODY()

public:
	AExplosionVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UHeterogeneousVolumeComponent> Volume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UFrameBlendComponent> FrameBlend;
};
