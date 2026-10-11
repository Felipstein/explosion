#pragma once

#include "GameFramework/Actor.h"
#include "ExplosionVolume.generated.h"

class UExplosionBake;
class UHeterogeneousVolumeComponent;
class UMaterialBillboardComponent;
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

	/**
	 * O clarão da detonação (2–3 quadros): luz própria sem sombra, sem rebote no
	 * Lumen e sem espalhamento na névoa (o Lumen e a névoa guardam a luz por
	 * segundos e o chão ficava aceso depois do clarão).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UPointLightComponent> FlashLight;

	/** A estrela de raios do clarão (0–0.13 s), virada pra câmera (M_FlashStar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlastLab")
	TObjectPtr<UMaterialBillboardComponent> FlashStar;

	/** O relógio da gravação (0 = o do tamanho, SizeScale): a morte de tanque toca sem esticar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float TimeScale = 0.0f;

	/** Acende as luzes (fogo, clarão, ofuscamento). A poeira que acompanha uma explosão não tem luz própria. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bLights = true;

	/** Multiplica a fuligem e o fogo por metro (a bola menor que o rendimento continua opaca). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float Thickness = 1.0f;

	/** A saia de poeira relativa à da Carga Pesada: altura e vida (início e τ do assentamento). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float SkirtHeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float SkirtLife = 1.0f;

	/**
	 * Gravação em loop (o fogo da carcaça, BakeRaw/wreck_fire_loop): toca em
	 * módulo de LoopPeriod s (o último quadro emenda no primeiro) até LoopLife s.
	 * 0 = toca uma vez.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float LoopPeriod = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float LoopLife = 0.0f;

	/**
	 * O clarão da detonação (um fogo contínuo não tem): a luz curta que acende o
	 * chão (FlashLight) e, no raymarch, o fogo mais quente e mais forte nos
	 * primeiros ~20 ms (BlastRaymarch.cpp, o clarão). O brilho em volta é o bloom
	 * do próprio fogo: os discos virados pra câmera (M_FireGlare de 45 m e o
	 * núcleo de 14 m da morte de tanque) saíram em 10/10 ("gambiarra", lavavam a
	 * tela e tapavam as unidades; Jimenez, SIGGRAPH 2014).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bFlash = true;

	/**
	 * Mais fuligem e fogo por metro só no começo (× EarlyThickness em t = 0,
	 * voltando a 1 entre 0.7·EarlyEnd e EarlyEnd s da gravação): a bola da EG
	 * saía pálida e translúcida até ~1 s sem mexer na batida de 1.3–1.5 s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float EarlyThickness = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float EarlyEnd = 0.0f;

	/**
	 * A temperatura no mapa do corpo negro (TempScale do material) × TempDip só
	 * de 0.6 a 1.2 s da gravação (entra de 0.5 a 0.7, sai de 1.1 a 1.3). 0 = a do
	 * material. A bola da EG saía creme (quente demais) aos 0.8–1.0 s, onde o
	 * vídeo é laranja-amarelo, sem mexer na casca com brasas de 1.3–1.5 s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float TempBase = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float TempDip = 1.0f;
	/** × TempLate depois da casca com brasas (entra de 1.5 a 2.0 s): a fumaça fria não brilha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float TempLate = 1.0f;
	/** O mesmo pro ganho da emissão (EmissionBoost × EmissionDip na mesma janela; 0 = o do material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float EmissionBase = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float EmissionDip = 1.0f;

	/** A queda da luz do clarão (s; 0 = a padrão, 35 ms) e a altura dela (cm; 0 = a padrão, 3 m): baixa, ela acende o chão em volta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float FlashTauS = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float FlashHeightCm = 0.0f;

	/**
	 * O volume não é desenhado (as luzes, o clarão e o choque continuam): o teste
	 * do flipbook (ABlastFlipbook) põe a bola pré-renderizada no lugar dele.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bHideVolume = false;

	/** A gravação que o raymarch toca: vazio = a padrão (a do WebGL); na comparação lado a lado
	 *  (-RmCompare=<nome>) a gêmea toca Intermediate/Raymarch/<nome>.bin (r128_v0, eg_gasoline...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	FName RaymarchSource;

	/** A bomba nuclear: a nuvem muda de marrom pra branca (o raymarch, BlastRaymarch.cpp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bNuclear = false;

	/** Achata (< 1) a gravação na vertical: o anel de poeira rente ao chão. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float HeightScale = 1.0f;

	/** Parâmetros do material só desta (ex. BaseClearM: a base da coluna mais rala). */
	TMap<FName, float> MaterialParams;

	/**
	 * Multiplica, além do LightScale, só a luz de DENTRO da fumaça (canal 1: a
	 * fumaça própria e a das outras explosões). O fogo da carcaça a 0.1 acendia a
	 * coluna de marrom (rodada l); a bola da EG acendia a própria fumaça rala de
	 * creme aos 0.8–1.0 s. A luz no casco e no chão fica.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float VolumeLightScale = 1.0f;

	/** Multiplica a luz gravada (a curva vem normalizada no pico da Carga Pesada). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float LightScale = 1.0f;

	/** A luz do cenário pode fazer sombra (as bolas pequenas do fogo da carcaça não: várias vivas ao mesmo tempo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bSceneShadow = true;

	/**
	 * O mip da gravação que o streaming traz (0 = a resolução inteira). Uma bola
	 * tocada a 0.45 da escala no mip 1 tem o mesmo voxel no mundo que a bola
	 * inteira no mip 0, com 1/8 dos dados: várias cabem no pool de streaming.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float StreamMip = 0.0f;

	/** Tempo da gravação que está sendo mostrado. */
	float BakeTime() const
	{
		const float Raw = Age / FMath::Max(TimeScale > 0.0f ? TimeScale : SizeScale, 0.01f);
		return LoopPeriod > 0.0f ? FMath::Fmod(FMath::Max(Raw, 0.0f), LoopPeriod) : Raw;
	}

	/** Os dois quadros em volta de T e a fração (com a emenda do loop). */
	void FramesAt(float T, int32& F0, int32& F1, float& Alpha) const;

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
	UTexture* GetWarp() const { return WarpTexture; }

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
	bool bDustTintSet = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StarMaterial;
	/** O pico da luz gravada (o maior canal de LightColor), pra levar a curva à física. */
	float BakeLightPeak = -1.0f;
	virtual void BeginPlay() override;
};
