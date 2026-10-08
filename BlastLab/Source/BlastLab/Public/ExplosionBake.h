#pragma once

#include "Engine/DataAsset.h"
#include "ExplosionBake.generated.h"

class UAnimatedSparseVolumeTexture;
class UVolumeTexture;

/**
 * Uma explosão gravada (uma variante): o volume animado e tudo o que a
 * reprodução precisa saber dele. Gerado por Tools/build_explosion.py a partir
 * da conversão (Tools/convert_bake.py) — não editar à mão.
 *
 * Tempos e tamanhos são os da gravação, na magnitude 1 (Carga Pesada); uma
 * explosão de outra magnitude escala espaço E tempo por W^(1/3).
 */
UCLASS(BlueprintType)
class BLASTLAB_API UExplosionBake : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UAnimatedSparseVolumeTexture> Volume;

	/** Aresta do domínio gravado, em cm (o volume é um cubo de Resolution³ voxels). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	float DomainCm = 3800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	int32 Resolution = 96;

	/** Instante de cada quadro gravado, em segundos (a amostragem não é uniforme no tempo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<float> FrameTimes;

	/** Caixa marchada de cada quadro (união da fumaça dele e do seguinte): canto mínimo, em voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<FIntVector> BoxMin;

	/** Tamanho da caixa marchada de cada quadro, em voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<FIntVector> BoxSize;

	/** Centro da luz do fogo em cada quadro, em cm, relativo ao pé da explosão (eixos da Unreal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<FVector> LightPosition;

	/** Cor × potência da luz do fogo em cada quadro, nas unidades do WebGL (1 = 128 000 lux de referência). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<FLinearColor> LightColor;

	/**
	 * Velocidade gravada em baixa resolução, todos os quadros empilhados em z
	 * (VelocityResolution³ cada; Tools/export_bake_velocity.py): xyz m/s, w
	 * fumaça. O compute shader da deformação lê esta (ele não lê a Sparse
	 * Volume Texture) pra o que passa empurrar a fumaça junto com a subida dela.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab|Interaction")
	TObjectPtr<UVolumeTexture> Velocity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab|Interaction")
	int32 VelocityResolution = 32;

	/**
	 * A normal pra fora da fumaça e a força da borda, na mesma grade da
	 * Velocity (rgb = normal·0.5 + 0.5, a = força): o deslocamento piroclástico
	 * do material (Tools/export_bake_velocity.py, build_explosion.py).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UVolumeTexture> Normal;

	/** Altura média da fuligem em cada quadro, em cm desde o pé (é por onde o avião e o míssil passam). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab|Interaction")
	TArray<float> SootHeight;

	/**
	 * Ganho da fuligem em cada quadro (vazio = 1). As simulações do EmberGen
	 * DISSOLVEM a fumaça (−50% aos 2.2 s na Gasoline): o ganho, S_pico / S(t)
	 * depois do pico (teto 6×), mantém a massa de fuligem que no vídeo fica no ar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TArray<float> SootGain;

	float Duration() const { return FrameTimes.Num() ? FrameTimes.Last() : 0.0f; }

	/** Quadro de baixo, o de cima e a fração entre eles (linear no tempo) no instante T da gravação. */
	void FramesAt(float T, int32& F0, int32& F1, float& Alpha) const;
};
