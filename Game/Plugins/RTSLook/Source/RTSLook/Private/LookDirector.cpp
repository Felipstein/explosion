#include "LookDirector.h"

#include "LookCamera.h"
#include "LookRecipe.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSLook, Log, All);

ALookDirector::ALookDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;     // depois da câmera andar no quadro
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->bUnbound = true;
	Post->Priority = 10.0f;
	RootComponent = Post;
}

void ALookDirector::BeginPlay()
{
	Super::BeginPlay();
	AdoptOrSpawn();
	ApplyRecipe();
}

// ---- o que o mapa já tem (o WorldForge põe sol, céu, luz do céu e névoa) ---------------------------

template <typename TActor, typename TComp>
static TComp* FindOrSpawn(UWorld* World, const TCHAR* What)
{
	for (TActorIterator<TActor> It(World); It; ++It)
	{
		if (TComp* C = It->template FindComponentByClass<TComp>())
		{
			return C;
		}
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TActor* A = World->SpawnActor<TActor>(FVector(0.0, 0.0, 500.0), FRotator::ZeroRotator, P);
	UE_LOG(LogRTSLook, Log, TEXT("o mapa não tinha %s: criei um"), What);
	return A ? A->template FindComponentByClass<TComp>() : nullptr;
}

void ALookDirector::AdoptOrSpawn()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// o sol: o que ilumina a atmosfera, se houver; senão o primeiro
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		UDirectionalLightComponent* C = It->FindComponentByClass<UDirectionalLightComponent>();
		if (C && (!Sun || C->IsUsedAsAtmosphereSunLight()))
		{
			Sun = C;
		}
	}
	if (!Sun)
	{
		Sun = FindOrSpawn<ADirectionalLight, UDirectionalLightComponent>(World, TEXT("sol"));
	}
	Sky = FindOrSpawn<ASkyAtmosphere, USkyAtmosphereComponent>(World, TEXT("céu"));
	SkyLight = FindOrSpawn<ASkyLight, USkyLightComponent>(World, TEXT("luz do céu"));
	Fog = FindOrSpawn<AExponentialHeightFog, UExponentialHeightFogComponent>(World, TEXT("névoa"));
	for (USceneComponent* C : TArray<USceneComponent*>{Sun.Get(), SkyLight.Get()})
	{
		if (C && C->Mobility != EComponentMobility::Movable)
		{
			C->SetMobility(EComponentMobility::Movable);     // a receita mexe neles em jogo
		}
	}
}

// ---- a receita --------------------------------------------------------------------------------------

void ALookDirector::ApplyRecipe()
{
	if (!Recipe)
	{
		UE_LOG(LogRTSLook, Warning, TEXT("diretor do look sem receita"));
		return;
	}
	float Dist = 0.0f, Yaw = InitialCameraYaw;
	if (ReadCamera(Dist, Yaw))
	{
		bSunPlaced = true;
	}
	ApplySun(Yaw);
	ApplyAtmosphere();
	ApplySkyLight();
	ApplyFog();
	ApplyPost();
	ApplyWater();
	ApplyPreset(PresetLevel);
	LastFocusCm = -1.0f;
	if (Dist > 0.0f)
	{
		UpdateFocus(Dist);
	}
}

void ALookDirector::ApplySun(float CameraYaw)
{
	if (!Sun)
	{
		return;
	}
	const FLookSun& S = Recipe->Sun;
	// a direção pro sol é a frente da câmera girada FromCamera graus pra esquerda; a luz anda ao contrário
	const float LightYaw = CameraYaw - S.FromCamera + 180.0f;
	Sun->SetWorldRotation(FRotator(-S.Elevation, LightYaw, 0.0f));
	Sun->SetIntensity(S.Lux);
	Sun->SetUseTemperature(S.Temperature > 0.0f);
	if (S.Temperature > 0.0f)
	{
		Sun->SetTemperature(S.Temperature);
	}
	else
	{
		Sun->SetLightColor(FLinearColor(S.Color));
	}
	Sun->SetLightSourceAngle(S.SourceAngle);
	Sun->SetSpecularScale(S.SpecularScale);
	Sun->SetAtmosphereSunLight(true);
}

void ALookDirector::ApplyAtmosphere()
{
	if (!Sky)
	{
		return;
	}
	const FLookAtmosphere& A = Recipe->Atmosphere;
	Sky->SetRayleighScatteringScale(A.RayleighScatteringScale);
	Sky->SetMieScatteringScale(A.MieScatteringScale);
	Sky->SetMieScattering(A.MieScattering);
	Sky->SetMieAbsorptionScale(A.MieAbsorptionScale);
	Sky->SetMieAbsorption(A.MieAbsorption);
	Sky->SetMieAnisotropy(A.MieAnisotropy);
	Sky->SetMieExponentialDistribution(A.MieExponentialDistribution);
	Sky->SetGroundAlbedo(A.GroundAlbedo);
	Sky->SetAerialPespectiveViewDistanceScale(A.AerialDistanceScale);     // (o nome tem o erro de digitação da Unreal)
}

void ALookDirector::ApplySkyLight()
{
	if (!SkyLight)
	{
		return;
	}
	const FLookSkyLight& K = Recipe->SkyLight;
	float Scale = 1.0f;
	if (Recipe->Presets.IsValidIndex(PresetLevel - 1) && !Recipe->Presets[PresetLevel - 1].bLumen)
	{
		Scale = Recipe->Presets[PresetLevel - 1].SkyLightScaleWithoutLumen;
	}
	SkyLight->SetIntensity(K.Intensity * Scale);
	SkyLight->SetLightColor(FLinearColor(K.Color));
	if (!SkyLight->IsRealTimeCaptureEnabled())
	{
		SkyLight->RecaptureSky();
	}
}

void ALookDirector::ApplyFog()
{
	if (!Fog)
	{
		return;
	}
	const FLookFog& F = Recipe->Fog;
	Fog->SetFogDensity(F.Density);
	Fog->SetFogHeightFalloff(F.Falloff);
	Fog->SetVolumetricFogAlbedo(F.VolumetricAlbedo);
	Fog->SetVolumetricFogScatteringDistribution(F.VolumetricScatteringDistribution);
	Fog->SetVolumetricFogExtinctionScale(F.VolumetricExtinctionScale);
	Fog->SetVolumetricFogDistance(F.VolumetricDistance);
}

void ALookDirector::ApplyPost()
{
	const FLookPost& P = Recipe->Post;
	FPostProcessSettings& S = Post->Settings;
	// a exposição travada: o histograma com mínimo = máximo
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMinBrightness = S.AutoExposureMaxBrightness = P.EV100;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = 0;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = P.Bias;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = P.Bloom;
	S.bOverride_LocalExposureShadowContrastScale = S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureShadowContrastScale = P.LocalExposureShadow;
	S.LocalExposureHighlightContrastScale = P.LocalExposureHighlight;
	if (P.WhiteTemp > 0.0f)
	{
		S.bOverride_WhiteTemp = true;
		S.WhiteTemp = P.WhiteTemp;
	}
	auto V4 = [](const FLinearColor& C) { return FVector4(C.R, C.G, C.B, 1.0); };
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(P.Saturation, P.Saturation, P.Saturation, 1.0);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(P.Contrast, P.Contrast, P.Contrast, 1.0);
	S.bOverride_ColorGain = true;
	S.ColorGain = V4(P.Gain);
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = V4(P.ShadowGain);
	S.bOverride_ColorGainMidtones = true;
	S.ColorGainMidtones = V4(P.MidtoneGain);
	S.bOverride_ColorGainHighlights = true;
	S.ColorGainHighlights = V4(P.HighlightGain);
	if (P.FilmToe >= 0.0f)
	{
		S.bOverride_FilmToe = true;
		S.FilmToe = P.FilmToe;
	}
	if (P.FilmShoulder >= 0.0f)
	{
		S.bOverride_FilmShoulder = true;
		S.FilmShoulder = P.FilmShoulder;
	}
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = P.Vignette;
	if (P.HighlightsMin >= 0.0f)
	{
		S.bOverride_ColorCorrectionHighlightsMin = true;
		S.ColorCorrectionHighlightsMin = P.HighlightsMin;
	}
	if (P.ShadowsMax >= 0.0f)
	{
		S.bOverride_ColorCorrectionShadowsMax = true;
		S.ColorCorrectionShadowsMax = P.ShadowsMax;
	}
}

void ALookDirector::ApplyWater()
{
	const FLookWater& Wt = Recipe->Water;
	UWorld* World = GetWorld();
	if (!Wt.bOverride || !World)
	{
		return;
	}
	// a água do WorldForge: qualquer peça com um material cujo pai é o M_Ocean_<bioma> (/Game/Slice/Materials)
	int32 N = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Comps(*It);
		for (UPrimitiveComponent* C : Comps)
		{
			for (int32 i = 0; i < C->GetNumMaterials(); ++i)
			{
				UMaterialInterface* M = C->GetMaterial(i);
				const UMaterial* Base = M ? M->GetBaseMaterial() : nullptr;
				if (!Base || !Base->GetName().StartsWith(TEXT("M_Ocean")))
				{
					continue;
				}
				UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(M);
				if (!D)
				{
					D = C->CreateAndSetMaterialInstanceDynamic(i);
				}
				if (D)
				{
					D->SetVectorParameterValue(TEXT("WaterBodyColor"), Wt.BodyColor);
					D->SetScalarParameterValue(TEXT("WaterBodyStrength"), Wt.BodyStrength);
					D->SetScalarParameterValue(TEXT("SkyReflection"), Wt.SkyReflection);
					++N;
				}
			}
		}
	}
	UE_LOG(LogRTSLook, Log, TEXT("a água do mapa: %d materiais com a cor da receita"), N);
}

void ALookDirector::ApplyPreset(int32 Level)
{
	PresetLevel = FMath::Clamp(Level, 1, 4);
	if (!Recipe)
	{
		return;
	}
	FLookPreset Q;
	if (Recipe->Presets.IsValidIndex(PresetLevel - 1))
	{
		Q = Recipe->Presets[PresetLevel - 1];
	}
	FPostProcessSettings& S = Post->Settings;
	S.bOverride_DynamicGlobalIlluminationMethod = S.bOverride_ReflectionMethod = true;
	S.DynamicGlobalIlluminationMethod = Q.bLumen ? EDynamicGlobalIlluminationMethod::Lumen : EDynamicGlobalIlluminationMethod::None;
	S.ReflectionMethod = Q.bLumen ? EReflectionMethod::Lumen : EReflectionMethod::ScreenSpace;
	if (Fog)
	{
		Fog->SetVolumetricFog(Recipe->Fog.bVolumetric && Q.bVolumetricFog);
	}
	ApplySkyLight();
}

// ---- a câmera do RTS: a névoa começa numa proporção da distância até o foco ------------------------

bool ALookDirector::ReadCamera(float& OutDistanceCm, float& OutYaw) const
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	if (const ILookCamera* Cam = Cast<ILookCamera>(PC->GetPawn()))
	{
		OutDistanceCm = Cam->GetLookFocusDistance();
		OutYaw = Cam->GetLookYaw();
		return true;
	}
	if (const APlayerCameraManager* M = PC->PlayerCameraManager)
	{
		// sem a câmera do jogo: a distância até o chão (z = 0) na direção que a câmera olha
		const FVector Loc = M->GetCameraLocation();
		const FRotator Rot = M->GetCameraRotation();
		const float Down = FMath::Max(FMath::Sin(FMath::DegreesToRadians(static_cast<float>(-Rot.Pitch))), 0.1f);
		OutDistanceCm = static_cast<float>(FMath::Max(Loc.Z, 100.0)) / Down;
		OutYaw = static_cast<float>(Rot.Yaw);
		return true;
	}
	return false;
}

void ALookDirector::UpdateFocus(float FocusDistanceCm)
{
	// só quando muda de verdade (1%): mexer na névoa todo quadro invalida o cache dela à toa
	if (LastFocusCm > 0.0f && FMath::Abs(FocusDistanceCm - LastFocusCm) < 0.01f * LastFocusCm)
	{
		return;
	}
	LastFocusCm = FocusDistanceCm;
	const FLookFog& F = Recipe->Fog;
	if (Fog)
	{
		Fog->SetStartDistance(FMath::Max(F.StartRatio, 0.05f) * FocusDistanceCm);
		Fog->VolumetricFogStartDistance = F.VolumetricStartRatio * FocusDistanceCm;
		Fog->MarkRenderStateDirty();
	}
	if (Sky && Recipe->Atmosphere.AerialStartRatio > 0.0f)
	{
		Sky->AerialPerspectiveStartDepth = Recipe->Atmosphere.AerialStartRatio * FocusDistanceCm / 100000.0f;   // km
		Sky->MarkRenderStateDirty();
	}
}

void ALookDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Recipe)
	{
		return;
	}
	float Dist = 0.0f, Yaw = InitialCameraYaw;
	if (!ReadCamera(Dist, Yaw))
	{
		return;
	}
	if (!bSunPlaced)
	{
		// o sol é escolhido uma vez, pela câmera inicial; depois fica fixo no mundo
		ApplySun(Yaw);
		bSunPlaced = true;
	}
	UpdateFocus(Dist);
}
