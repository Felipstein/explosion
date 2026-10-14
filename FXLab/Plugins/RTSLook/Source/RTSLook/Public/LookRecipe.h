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

/** O que cada preset muda (1 baixa … 4 ultra, o mesmo nível do Scalability do jogo). */
USTRUCT(BlueprintType)
struct FLookPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bLumen = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bVolumetricFog = true;
	/** A luz do céu sem Lumen precisa de mais força pra a sombra não afundar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SkyLightScaleWithoutLumen = 1.0f;
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
	/** Índice 0..3 = preset 1..4 (baixa, média, alta, ultra). Vazio = tudo ligado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") TArray<FLookPreset> Presets;
};
