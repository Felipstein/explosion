#pragma once

#include "GameFramework/Actor.h"
#include "BlastLabEnvironment.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class APostProcessVolume;
class ASkyAtmosphere;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UTextureRenderTarget2D;

/**
 * Hora do dia, como no WebGL (reference/webgl/js/celestial.js, atmosphere.js,
 * post.js): move o sol e a lua (porte em Celestial.h) e, pela tabela gravada
 * do próprio WebGL (Tools/webgl_time_of_day.json), acerta pra cada hora:
 *
 * - a exposição analítica: exposição = BASE·(E_ref/E_render)·(E_lux/E_ref)^α,
 *   α = 0.1265 — o brilho exibido cai como E^α com a iluminância física, então
 *   a noite de lua sai ~2.2 stops abaixo do dia na tela (entre o "day-for-night"
 *   do cinema e Ghost of Tsushima). A adaptação às explosões (histograma) só
 *   pode escurecer a partir dela: ~1 stop de dia, ~6.5 à noite (exposure.js);
 * - o espalhamento múltiplo do céu (o céu aprovado do WebGL tem mais que o de
 *   Hillaire, ver Tools/sky_ms_table.py);
 * - a lua do render 2.2e-3 do sol (880× a física, 0.32 lux): com o luar físico
 *   a explosão seria ~10⁵× o ambiente e o campo sumiria a cada clarão (o mesmo
 *   que Far Cry 5 concluiu); a exposição e o Purkinje enxergam a física;
 * - o Purkinje (visão de bastonetes no escuro, Patry 2021) e o ganho das
 *   estrelas, que vão pro pós-processo e pro céu por uma Material Parameter
 *   Collection.
 * O sol continua aceso até −18° (a faixa roxa e o brilho do crepúsculo vêm
 * dele, abaixo do horizonte) e passa o posto de luz principal pra lua quando se
 * põe (ForwardShadingPriority: névoa volumétrica, translúcidos).
 */
UCLASS()
class BLASTLAB_API ABlastLabEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ABlastLabEnvironment();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<ADirectionalLight> Moon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<ASkyAtmosphere> Atmosphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<APostProcessVolume> PostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<AExponentialHeightFog> Fog;

	/** Purkinje e ganho das estrelas, lidos pelo tonemapper e pelo céu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<UMaterialParameterCollection> Params;

	/** Recorte do céu pra medição da exposição (build_scene.py exposure_mask_material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	TObjectPtr<UMaterialInterface> ExposureMask;

	/** Hora solar local, 0–24. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float TimeOfDay = 15.0f;

	/** Trópico de Capricórnio no solstício de junho, como o WebGL (main.js env). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float LatitudeDeg = -23.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float DayOfYear = 172.0f;

	/** Ciclo automático: horas de jogo por segundo real (o WebGL: 0.35). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	bool bAutoCycle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab")
	float CycleSpeed = 0.35f;

	// ---- tabela do WebGL, por hora (Tools/build_scene.py) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveHours;

	/** EV100 da exposição analítica naquela hora (15h = 14.61, a calibração da cena). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveEV100;

	/** Escala do Purkinje em cd/m⁻² (texel 4 do WebGL / 128 000). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurvePurkinje;

	/** Reforço das fontes da noite (E_render/E_lux): 1 de dia, ~880 com a lua. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveStarGain;

	/** MultiScatteringFactor da atmosfera: o céu do WebGL tem ~2–5× o espalhamento
	 *  múltiplo de Hillaire (Tools/sky_ms_table.py explica o porquê). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveMultiScattering;

	/** Cor da névoa em cd/m² (o céu do WebGL abaixo do horizonte, Tools/webgl_fog.json):
	 *  é ela que deixa o chão roxo no crepúsculo e neutro à noite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<FLinearColor> CurveFog;

	/** O excesso dela na direção do sol (o laranja do poente), em cd/m² com o
	 *  lobo cos^n da névoa direcional da Unreal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<FLinearColor> CurveFogDirectional;

	/** Largura desse lobo (o n do cos^n): ~1 no poente, quando o laranja toma o quadro, ~10 de dia. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveFogExponent;

	/** Iluminância horizontal do render (lux: luz principal + céu), E_render do WebGL × 128 000. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlastLab|Table")
	TArray<float> CurveRenderLux;

	void SetTimeOfDay(float Hours);
	float SunAltitudeDeg() const { return SunAltDeg; }
	/** EV100 da exposição analítica agora. */
	float TargetEV100() const { return TargetEV; }
	/** Iluminância do ambiente agora (lux do render). */
	float RenderLux() const { return Sample(CurveRenderLux, 52000.0f); }

	/** Últimos valores mandados aos materiais (leitura de teste). */
	float LastPurkinje = 0.0f;
	float LastStarGain = 0.0f;
	float LastMultiScattering = 0.0f;

	static ABlastLabEnvironment* Find(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void Apply();
	/** Desenha o recorte do céu com a câmera do jogador (a medição só vê chão e explosões). */
	void UpdateMeterMask();
	void UpdateBloomThreshold();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MeterMask;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeterMaskMaterial;
	float Sample(const TArray<float>& Curve, float Fallback) const;
	FLinearColor Sample(const TArray<FLinearColor>& Curve, const FLinearColor& Fallback) const;
	/** Posição na tabela (0..N−1) da hora atual; falso se a tabela não existe. */
	bool TablePosition(int32 Count, int32& I0, float& Frac) const;

	float SunAltDeg = 0.0f;
	float TargetEV = 14.61f;
	float AppliedTime = -1.0f;
	int32 KeyLight = -1;                 // 0 = sol, 1 = lua (a prioridade só muda na troca)
};
