#pragma once

#include "GameFramework/Actor.h"
#include "ExplosionVolume.generated.h"

class UExplosionBake;
class UHeterogeneousVolumeComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UTexture;

/**
 * Uma explosão: a gravação (UExplosionBake) tocada num Heterogeneous Volume,
 * com o relógio, o tamanho e as luzes dela. A origem do ator é o pé da
 * explosão, no chão.
 *
 * Tamanho: as grandezas de uma explosão escalam com W^(1/3) em comprimento E
 * em tempo (velocidade característica constante; reference/webgl/js/instances.js
 * MAGNITUDES), então uma explosão de outra magnitude é a mesma gravação com o
 * ator escalado por SizeScale e o tempo da gravação = Age / SizeScale. A luz
 * vai com a área (SizeScale²).
 *
 * Reprodução (o que o Heterogeneous Volume faria sozinho, feito aqui):
 * - liga ao material os DOIS quadros gravados em volta do instante e a
 *   fração entre eles — o volume da Unreal mostra um só, floor(Frame), e a
 *   fumaça andaria aos degraus; o material mistura os dois levando cada um
 *   pelos vetores de movimento (como o WebGL, js/instances.js bakeAtMV);
 * - encolhe a caixa que o volume marcha pra caixa justa da fumaça naquele
 *   quadro: o volume marcha a caixa inteira voxel a voxel avaliando o
 *   material em cada passo, sem pular o vazio, e a fumaça ocupa de ~1% a
 *   ~33% do domínio gravado (custo 15.5 → 1.5–4.3 ms na GTX 1660). O dado não
 *   muda: o material lê nas coordenadas do domínio inteiro.
 * Parâmetros do material: SparseVolumeTexture / SparseVolumeTextureNext
 * (quadros), FrameAlpha, FrameInterval (s), BoxMin / BoxSize (voxels), Time
 * (s da gravação, pro ruído de detalhe), Seed, DensityScale, EmissionScale.
 */
UCLASS()
class BLASTLAB_API AExplosionVolume : public AActor
{
	GENERATED_BODY()

public:
	AExplosionVolume();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<UExplosionBake> Bake;

	/** Segundos desde a detonação (tempo do mundo). Negativo = ainda não detonou. As sequências de teste animam isto. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "BlastLab")
	float Age = 0.0f;

	/** W^(1/3): 1 = Carga Pesada. Escala o tamanho e a duração. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float SizeScale = 1.0f;

	/** Desloca o ruído de detalhe: duas explosões da mesma gravação não ficam idênticas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float Seed = 0.0f;

	/** 0 = invisível (o aquecimento de shaders na abertura do simulador: desenha, mas não mostra nada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float Opacity = 1.0f;

	/** No simulador a explosão avança a própria idade (relógio do simulador, com pausa e câmera lenta). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bSelfAdvance = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<USceneComponent> Foot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UHeterogeneousVolumeComponent> Volume;

	/** Luz do fogo no cenário (com sombra); não toca o volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UPointLightComponent> FireLight;

	/** Luz do fogo dentro da fumaça: canal de iluminação 1, que só as explosões escutam. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UPointLightComponent> FireLightVolume;

	/** Tempo da gravação que está sendo mostrado. */
	float BakeTime() const { return Age / FMath::Max(SizeScale, 0.01f); }

	/** A gravação acabou (o simulador remove a explosão). */
	bool IsFinished() const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }   // o Sequencer no editor também atualiza

	/** Nível de detalhe atual (0 perto · 1 médio · 2 longe) e se está na tela (teste/depuração). */
	int32 DetailLevel() const { return Lod; }
	bool IsOnScreen() const { return bOnScreen; }
	float ScreenFraction() const { return ScreenFrac; }

	/** Detalhe mínimo pelo orçamento (UBlastLabSimulation): com muitas na tela, as menos importantes descem. */
	int32 LodFloor = 0;

	/**
	 * A deformação pelo que passa (UBattleSmokeSubsystem): o material lê a
	 * gravação deslocada por ela e a caixa marchada ganha folga pra fumaça
	 * empurrada pra fora. Nula: a gravação como é.
	 */
	void SetWarp(UTexture* Warp);
	bool IsWarped() const { return WarpTexture != nullptr; }

	/** Caixa da fumaça no quadro gravado atual, no mundo (cm). */
	FBox SmokeBoxWorld() const;

	/** Altura média da fuligem daqui a AheadS segundos, no mundo (cm): por onde o avião e o míssil passam. */
	float SootHeightWorld(float AheadS = 0.0f) const;

private:
	UMaterialInstanceDynamic* VolumeMaterial() const;
	void UpdateSignificance(float T);
	void UpdateVolume(float T);
	void UpdateLights(float T);

	// Importância (UpdateSignificance): fora da tela o volume não é desenhado
	// nem atualizado, só os quadros seguem chegando; pequena na tela, menos
	// passos e iluminação mais grossa. A luz do cenário continua acesa (o
	// clarão de uma explosão fora do quadro ilumina o chão que se vê), mas
	// sem sombra quando o chão perto dela não aparece.
	bool bOnScreen = true;
	bool bShadowRegionOnScreen = true;
	float ScreenFrac = 1.0f;
	int32 NaturalLod = 0;      // só pelo tamanho na tela (com histerese)
	int32 Lod = 0;             // o aplicado: o natural ou o piso do orçamento

	UPROPERTY(Transient)
	TObjectPtr<UTexture> WarpTexture;
	bool bWarpDirty = false;
};
