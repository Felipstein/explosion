#pragma once

// A receita do look de um ambiente e hora (docs/GRAFICOS.md §7–8): o espelho de
// Game/Tools/look_recipes/<cena>.json, que continua sendo a fonte. O JSON vira este DataAsset
// por Game/Tools/look_recipe_import.py. Os valores são os da maquete (LookLab), medidos contra
// a arte da cena no ponto a ponto.

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LookRecipe.generated.h"

/** O sol (ou a lua): escolhido pela câmera inicial do mapa e fixo no mundo. */
USTRUCT(BlueprintType)
struct FLookSun
{
	GENERATED_BODY()

	/** Altura acima do horizonte (graus). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Elevation = 20.0f;
	/** Graus à esquerda da frente da câmera inicial (negativo = à direita). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FromCamera = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Lux = 120000.0f;
	/** Temperatura de corpo negro (K); 0 = usa Color. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Temperature = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor Color = FColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SourceAngle = 0.53f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpecularScale = 1.0f;
	/** (16/10, o gameplay_v3: os raios saindo de trás da nuvem) o "Light Shaft Bloom" da Unreal: em volta do sol na tela, o
	 *  claro acima do limiar é borrado em raios a partir do sol e somado (o espalhamento como pós-processo do GPU Gems 3).
	 *  Só aparece com o sol na tela (a câmera deitada); a câmera do RTS não muda. 0 = não mexe no que o mapa tem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShaftBloom = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShaftThreshold = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShaftMaxBrightness = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor ShaftTint = FColor::White;
};

USTRUCT(BlueprintType)
struct FLookAtmosphere
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RayleighScatteringScale = 0.0331f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MieScatteringScale = 0.003996f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor MieScattering = FLinearColor(0.577f, 0.577f, 0.577f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MieAbsorptionScale = 0.000444f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor MieAbsorption = FLinearColor(0.577f, 0.577f, 0.577f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MieAnisotropy = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MieExponentialDistribution = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor GroundAlbedo = FColor(170, 170, 170);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AerialDistanceScale = 1.0f;
	/** O começo da perspectiva aérea em proporção da distância da câmera ao foco (0 = não mexe). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AerialStartRatio = 0.0f;
	/** (16/10, "uma luz só") o brilho e a cor dos pixels de céu (SkyLuminanceFactor da Unreal): só o céu, igual em qualquer
	 *  ângulo; a areia, as unidades e as explosões não mudam (SkyAtmosphere.usf). Também entra na luz distante do céu (a névoa
	 *  e a captura): medir a areia junto. 1 = a física. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor SkyLuminanceFactor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	/** Quanto a luz do céu e do sol na atmosfera tinge a névoa de altura (HeightFogContribution; 1 = a física). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeightFogContribution = 1.0f;
};

USTRUCT(BlueprintType)
struct FLookSkyLight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Intensity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor Color = FColor::White;
};

USTRUCT(BlueprintType)
struct FLookFog
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Density = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Falloff = 0.2f;
	/** O começo da névoa em proporção da distância da câmera ao foco. Nunca 0: tinge o foco de perto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartRatio = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bVolumetric = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor VolumetricAlbedo = FColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float VolumetricScatteringDistribution = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float VolumetricExtinctionScale = 1.0f;
	/** Até onde a névoa volumétrica vai (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float VolumetricDistance = 6000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float VolumetricStartRatio = 0.75f;
	/** (16/10, "uma luz só"; o Felipe: "o céu sumiu a cor, sumiu as nuvens") o teto da névoa de altura (FogMaxOpacity): o véu
	 *  do céu e do horizonte com a câmera deitada para aqui; de cima a névoa (60–250 m) fica abaixo dele e não muda (medir). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxOpacity = 1.0f;
	/** (17/10, a faixa branca no horizonte da câmera deitada) a cor da névoa de altura além da volumétrica: a Unreal acende
	 *  essa névoa só com a luz ambiente do céu (SkyAtmosphereAmbientContributionColorScale, padrão branco), sem a cor da
	 *  poeira; vista de lado ela satura no brilho do céu. Com a cor da poeira (o albedo da volumétrica) as duas névoas casam,
	 *  como faz o r.SupportExpFogMatchesVolumetricFog do projeto. Branco = o padrão da Unreal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor SkyAmbientScale = FLinearColor::White;
};

/** O pós: exposição travada (EV100 fixo, mínimo = máximo), a curva e a cor. */
USTRUCT(BlueprintType)
struct FLookPost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EV100 = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Bias = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Bloom = 0.675f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LocalExposureShadow = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LocalExposureHighlight = 1.0f;
	/** 0 = não mexe no balanço de branco. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WhiteTemp = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Saturation = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Contrast = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Gain = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor ShadowGain = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor MidtoneGain = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor HighlightGain = FLinearColor::White;
	/** A curva do tonemapper; < 0 = o padrão da Unreal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FilmToe = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FilmShoulder = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Vignette = 0.4f;
	/** Onde começam as luzes e onde acabam as sombras na correção de cor; < 0 = padrão. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HighlightsMin = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowsMax = -1.0f;
};

/** O pó do mundo nas unidades (o M_UnitDust): a cor e quanto assenta. */
USTRUCT(BlueprintType)
struct FLookWorld
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor DustColor = FLinearColor(0.46f, 0.34f, 0.22f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float UnitDustTop = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float UnitDustLow = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float UnitDustHeight = 1.2f;
	/** (17/10, o "piso liso" da câmera deitada) o Specular da areia e da terra do chão: o parâmetro 'SandSpecular' do
	 *  material do mapa (o 4º dado do MF do chão; o asfalto fica no dele). Abaixo de 0,25 a Unreal trata o F0 < 2% como
	 *  sombra (BRDF.ush, F_Schlick: saturate(50·F0)), a sombra dos grãos no próprio brilho. −1 = o do material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GroundSandSpecular = -1.0f;
};

/** A água do mapa (os controles do M_Ocean_* do WorldForge): a cor própria do corpo d'água e o peso do reflexo do
 *  céu. Sem bOverride, a água fica como o mapa fez. A água tem de ler como água em qualquer hora (jogabilidade). */
USTRUCT(BlueprintType)
struct FLookWater
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOverride = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor BodyColor = FLinearColor(0.05f, 0.35f, 0.4f);
	/** 0..1: quanto da cor própria substitui o espalhamento do material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BodyStrength = 0.0f;
	/** 0..1: o peso do reflexo do céu e da névoa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SkyReflection = 1.0f;
};

/** O céu com a câmera deitada (14–16/10, docs/CEU_E_NUVENS.md §7.1, §8.2–8.3 e a 2ª opinião). Tudo pela elevação do topo da
 *  tela em relação ao horizonte (o campo de visão abre ao deitar): na câmera normal o topo fica a −23°, no zoom perto a
 *  −13°, e o horizonte entra no 0°. Abaixo de TiltStartTopDeg nada muda (a câmera normal idêntica à aprovada). Acima:
 *  - a perspectiva aérea do RTS (×40, véu nas distâncias curtas) desce pra a do céu, senão apaga as nuvens e as dunas;
 *  - a névoa de altura (a camada de poeira, ~57 m de escala) fica mais rasa: a queda vezes TiltFogFalloffScale. Sem degrau
 *    nem costura no horizonte, como o corte duro (FogCutoffDistance) faria. */
USTRUCT(BlueprintType)
struct FLookSky
{
	GENERATED_BODY()

	/** O corte duro da névoa de altura (cm; 0 = sem corte). Só depois do último terreno, se um dia precisar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float FogCutoffCm = 0.0f;
	/** A escala da perspectiva aérea com a câmera deitada (0 = não muda). A 2ª opinião: ×1–2, nunca mais (×4 já apaga). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltAerialDistanceScale = 0.0f;
	/** A queda da névoa de altura vezes isto com a câmera deitada (0 ou 1 = não muda). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltFogFalloffScale = 0.0f;
	/** A mistura, pela elevação do topo da tela (graus): começa em TiltStartTopDeg e completa em TiltFullTopDeg. */
	/** (16/10, o teste do sol na tela: o céu e o lago estouravam) a exposição com a câmera deitada, em EV somado ao Bias
	 *  da receita (negativo escurece): a exposição travada é a do chão visto de cima. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltExposureBias = 0.0f;
	/** O reflexo do céu na água com a câmera deitada, vezes o da receita (1 = não muda): em ângulo rasante o lago virava
	 *  um espelho branco do céu claro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltWaterReflection = 1.0f;
	/** (16/10, o sol na tela: o −1,6 EV ainda deixava 15% estourado e escurecia o chão de costas pro sol) o contraste
	 *  das luzes altas da exposição local com a câmera deitada (0 = não muda; a receita normal usa ~0,9): o céu claro é
	 *  uma área grande e clara, que a exposição local comprime sem mexer no meio-tom do chão (a doc da Unreal, Local
	 *  Exposure: "Highlight Contrast Scale"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltLocalExposureHighlight = 0.0f;
	/** (16/10, o gameplay_v3) a saturação da câmera deitada, vezes a da receita (0 = não muda): de frente pro sol baixo, a
	 *  areia de duna sob o sol laranja e o céu de poeira somavam saturação 0,67–0,78 contra 0,38–0,51 no alvo; a câmera do
	 *  RTS (a cor aprovada) não muda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltSaturation = 0.0f;
	/** (16/10, o gameplay_v3) o balanço de branco da câmera deitada (K, 0 = não muda): o alvo é dourado (matiz ~25) e a nossa
	 *  areia sob o sol baixo saía vermelha (~20); tirar saturação virava salmão. Subir o WhiteTemp (a receita usa 5600)
	 *  puxa pro amarelo. E o bloom da câmera deitada (0 = não muda): o clarão dourado em volta do sol. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltWhiteTemp = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltBloom = 0.0f;
	/** (16/10, o Felipe: "precisa desse escurecimento?") a janela da câmera (exposição, contraste das luzes, bloom, saturação,
	 *  balanço de branco), separada da janela da névoa: começa quando o horizonte encosta no topo da tela (0°) e completa
	 *  com o horizonte a ~1/3 do topo (+10°, a moldura do gameplay_v3). Antes ela andava junto da névoa (−8° → +2°) e a
	 *  imagem escurecia na descida antes do céu claro entrar no quadro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltExposureStartTopDeg = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltExposureFullTopDeg = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltStartTopDeg = -8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TiltFullTopDeg = 2.0f;
};

/** As nuvens volumétricas (14/10, docs/CEU_E_NUVENS.md §7): uma camada só (a Unreal desenha um VolumetricCloud por
 *  cena), a quilômetros de altura: nunca fica entre a câmera e as unidades. Ligada só com a câmera deitada. */
USTRUCT(BlueprintType)
struct FLookClouds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnable = false;
	/** (16/10, "uma luz só") a nuvem entra na captura da luz do céu em tempo real (o WorldForge liga a captura em tempo real):
	 *  como ela só liga com a câmera deitada, a luz ambiente mudava ao deitar. false = fora da captura. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bInSkyCapture = false;
	/** O material (caminho do asset); vazio = o m_SimpleVolumetricCloud da Unreal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Material;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float BottomKm = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float HeightKm = 8.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float TracingMaxKm = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ViewSampleScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowSampleScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowTracingKm = 15.0f;
	/** Cada amostra com a luz do sol da altitude dela (o topo alto mais branco que a base). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPerSampleTransmittance = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FColor GroundAlbedo = FColor(176, 140, 100);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SkyLightBottomOcclusion = 0.5f;
	/** Não traça antes disto da câmera nem começa se a camada estiver mais longe que StartMaxKm (a faixa de 0–2° entra
	 *  na camada a 60–200 km e paga o máximo de passos). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartFromCameraKm = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartMaxKm = 100.0f;
	/** A sombra da nuvem: no ar (os raios pelos buracos; começa em 0, só com A/B) e no chão (0 com o sol baixo: escurece a
	 *  tela inteira). Ajustada uma vez: mexer nela recria a luz e invalida o cache das sombras virtuais. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowOnAtmosphere = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShadowOnSurface = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor ScatteredLuminanceScale = FLinearColor::White;
	/** Liga quando o topo da tela passa desta elevação (graus; o horizonte entra no 0°, o zoom perto fica a −13°) e
	 *  desliga 2° abaixo: a câmera normal não paga nada e a reconstrução tem uns quadros pra convergir antes do céu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EnableTopDeg = -9.0f;
	/** Os parâmetros do material, por nome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, float> Scalars;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FLinearColor> Vectors;
};

/** A sombra das nuvens no chão, como o C&C Zero Hour (16/10, o pedido do Felipe). Não é a sombra física da nuvem: com o sol
 *  a 8° ela estica ~7× e escurece a tela inteira por minutos. É uma projeção suave no rumo do sol, do tamanho da tela do
 *  RTS, andando com o vento, como termo da função de luz do sol (a M_SmokeShadowLF do Fluxo, a única que o sol aceita). */
USTRUCT(BlueprintType)
struct FLookCloudShadow
{
	GENERATED_BODY()

	/** 0 = sem sombra de nuvem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Strength = 0.0f;
	/** O tamanho das manchas (m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ScaleM = 150.0f;
	/** Quanto do chão fica na sombra (0..1) e a maciez da borda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cover = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Soft = 0.15f;
	/** O vento das manchas (m/s, X e Y do mundo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor WindMS = FLinearColor(4.0f, 2.0f, 0.0f, 0.0f);
};

/** O que cada preset muda (1 baixa … 4 ultra, o mesmo nível do Scalability do jogo). */
USTRUCT(BlueprintType)
struct FLookPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bLumen = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bVolumetricFog = true;
	/** A luz do céu sem Lumen precisa de mais força pra a sombra não afundar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SkyLightScaleWithoutLumen = 1.0f;
	/** As nuvens neste preset (a baixa fica sem) e a escala das amostras delas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bClouds = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CloudSampleScale = 1.0f;
	/** Outro material de nuvem neste preset (as oitavas de dispersão são fixas na compilação); vazio = o da receita. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString CloudMaterial;
	/** O mapa de sombra das nuvens neste preset mesmo com as forças de sombra em 0 (o material que se sombreia por ele, o
	 *  _Med). Sem isso ele só existe quando alguma força passa de 0: com o sol a 8° custa ~2,9 ms (medido 16/10). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCloudShadowMap = false;
};

UCLASS(BlueprintType)
class RTSLOOK_API ULookRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookSun Sun;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookAtmosphere Atmosphere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookSkyLight SkyLight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookFog Fog;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookPost Post;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookWorld World;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookWater Water;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookSky Sky;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookClouds Clouds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLookCloudShadow CloudShadow;
	/** Índice 0..3 = preset 1..4 (baixa, média, alta, ultra). Vazio = tudo ligado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") TArray<FLookPreset> Presets;
};
