#include "LookDirector.h"

#include "LookCamera.h"
#include "LookRecipe.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSLook, Log, All);

// (16/10) teste: as nuvens ligadas sempre (a vista acima das nuvens, com a câmera de teste da sessão do jogo)
static TAutoConsoleVariable<int32> CVarCloudsAlways(TEXT("rts.Look.CloudsAlways"), 0,
	TEXT("1 = as nuvens da receita ficam ligadas em qualquer inclinação (teste; o jogo normal liga só com a câmera deitada)."));

// (17/10, a névoa de guerra com a câmera deitada) os dois testes do Felipe; o jogo lê a mesma CVar pra não multiplicar
static TAutoConsoleVariable<int32> CVarFogStyle(TEXT("rts.FogStyle"), 0,
	TEXT("A névoa de guerra: 0 = o passe do jogo (multiplica a tela, o ZH); 1 = a sombra de nuvem (o sol some fora da visão: o termo fow da light function do sol, mais o tom frio do AoE IV); 2 = o desfoque (o pós M_FowBlur do Look); 3 = a 1 de cima e o desfoque entrando ao deitar a câmera."),
	ECVF_RenderThreadSafe);

namespace
{
	// o que o jogo passou (ALookDirector::SetFogOfWar): a textura de visão e o retângulo do mapa
	TWeakObjectPtr<UTexture2D> GFowVision;
	FLinearColor GFowMapMinInvSize(0.0f, 0.0f, 0.0f, 0.0f);
	FLinearColor GFowTexel(0.0f, 0.0f, 0.0f, 0.0f);
	int32 GFowSerial = 0;
}

void ALookDirector::SetFogOfWar(UTexture2D* Vision, FVector2D MapMinCm, FVector2D MapSizeCm)
{
	GFowVision = Vision;
	GFowMapMinInvSize = FLinearColor(float(MapMinCm.X), float(MapMinCm.Y),
		MapSizeCm.X > 0.0 ? float(1.0 / MapSizeCm.X) : 0.0f, MapSizeCm.Y > 0.0 ? float(1.0 / MapSizeCm.Y) : 0.0f);
	GFowTexel = Vision ? FLinearColor(1.0f / float(FMath::Max(Vision->GetSizeX(), 1)), 1.0f / float(FMath::Max(Vision->GetSizeY(), 1)), 0.0f, 0.0f)
		: FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	++GFowSerial;
}

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
	for (USceneComponent* C : TArray<USceneComponent*>{Sun.Get(), SkyLight.Get(), Sky.Get(), Fog.Get()})
	{
		if (C && C->Mobility != EComponentMobility::Movable)
		{
			C->SetMobility(EComponentMobility::Movable);     // a receita mexe neles em jogo
		}
	}
}

// ---- a receita --------------------------------------------------------------------------------------

// (16/10) o relógio do -RTSLookCycle: a hora do mundo da última aplicação da receita que não veio do próprio ciclo (o teste
// rápido aplica a receita quando põe a câmera da cena, o zero dos tempos da cena; o relógio do mundo começa antes e a folga
// variava de 2 a 2,6 s entre rodadas, as trocas caíam fora das fotos)
static double GLookCycleZero = 0.0;
static bool GLookCycleApplying = false;

void ALookDirector::ApplyRecipe()
{
	if (!Recipe)
	{
		UE_LOG(LogRTSLook, Warning, TEXT("diretor do look sem receita"));
		return;
	}
	if (!GLookCycleApplying && GetWorld())
	{
		GLookCycleZero = GetWorld()->GetTimeSeconds();
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
	ApplyGround();
	if (Recipe->World.FogStyle >= 0)
	{
		CVarFogStyle->Set(Recipe->World.FogStyle, ECVF_SetByCode);
		UE_LOG(LogRTSLook, Log, TEXT("a receita põe a névoa de guerra no estilo %d"), Recipe->World.FogStyle);
	}
	ApplyPreset(PresetLevel);
	LastFocusCm = -1.0f;
	LastAerialScale = -1.0f;
	LastFalloff = -1.0f;
	CloudShadowMID = nullptr;
	CloudAbove = -1;
	LastTiltT = -1.0f;
	LastExpT = -1.0f;
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
	if (S.ShaftBloom > 0.0f)
	{
		Sun->SetEnableLightShaftBloom(true);
		Sun->SetBloomScale(S.ShaftBloom);
		Sun->SetBloomThreshold(S.ShaftThreshold);
		Sun->SetBloomMaxBrightness(S.ShaftMaxBrightness);
		Sun->SetBloomTint(S.ShaftTint);
	}
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
	Sky->SetSkyLuminanceFactor(A.SkyLuminanceFactor);
	Sky->SetHeightFogContribution(A.HeightFogContribution);
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
		// (16/10, a 2ª opinião) a captura é um render da cena: com a câmera deitada ela gravaria a névoa rasa e as nuvens
		// na luz ambiente da partida inteira. Só captura no estado normal; se estiver deitada, espera voltar.
		if (SkyT > 0.0f)
		{
			bRecapturePending = true;
		}
		else
		{
			SkyLight->RecaptureSky();
			bRecapturePending = false;
		}
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
	// (14/10) o corte: além dele a névoa de altura não entra, e o céu e as nuvens da câmera deitada saem limpos dela;
	// a câmera normal só vê chão a 60–250 m (docs/CEU_E_NUVENS.md §8.2)
	Fog->SetFogCutoffDistance(Recipe->Sky.FogCutoffCm);
	Fog->SetFogMaxOpacity(F.MaxOpacity);
	Fog->SetSkyAtmosphereAmbientContributionColorScale(F.SkyAmbientScale);
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
	WaterMIDs.Reset();
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
					WaterMIDs.Add(D);
					++N;
				}
			}
		}
	}
	UE_LOG(LogRTSLook, Log, TEXT("a água do mapa: %d materiais com a cor da receita"), N);
}

void ALookDirector::ApplyGround()
{
	// o especular da areia do chão (o 'SandSpecular' do material do WorldForge); sem o parâmetro no material, não faz nada
	const float V = Recipe->World.GroundSandSpecular;
	UWorld* World = GetWorld();
	if (V < 0.0f || !World)
	{
		return;
	}
	int32 N = 0;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		It->SetLandscapeMaterialScalarParameterValue(TEXT("SandSpecular"), V);
		++N;
	}
	UE_LOG(LogRTSLook, Log, TEXT("o especular da areia do chão: %.3f em %d pedaços do Landscape"), V, N);
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
	ApplyClouds();
}

// ---- as nuvens (14/10, docs/CEU_E_NUVENS.md §7): uma camada a quilômetros de altura, só com a câmera deitada --------

void ALookDirector::ApplyClouds()
{
	const FLookClouds& C = Recipe->Clouds;
	FLookPreset Q;
	if (Recipe->Presets.IsValidIndex(PresetLevel - 1))
	{
		Q = Recipe->Presets[PresetLevel - 1];
	}
	bCloudsAllowed = C.bEnable && Q.bClouds;
	CloudShown = -1;          // o Tick decide pela inclinação
	CloudAbove = -1;          // e o começo do traçado pela altura da câmera
	if (!bCloudsAllowed)
	{
		if (Cloud)
		{
			Cloud->SetVisibility(false);
			CloudShown = 0;
		}
		if (Sun && Sun->bCastCloudShadows)
		{
			Sun->bCastCloudShadows = false;
			Sun->MarkRenderStateDirty();
		}
		return;
	}
	if (!Cloud)
	{
		Cloud = FindOrSpawn<AVolumetricCloud, UVolumetricCloudComponent>(GetWorld(), TEXT("nuvens"));
		if (!Cloud)
		{
			return;
		}
	}
	Cloud->SetVisibleInRealTimeSkyCaptures(C.bInSkyCapture);
	Cloud->SetLayerBottomAltitude(C.BottomKm);
	Cloud->SetLayerHeight(C.HeightKm);
	Cloud->SetTracingMaxDistance(C.TracingMaxKm);
	Cloud->SetTracingStartDistanceFromCamera(C.StartFromCameraKm);
	Cloud->SetTracingStartMaxDistance(C.StartMaxKm);
	Cloud->SetViewSampleCountScale(C.ViewSampleScale * Q.CloudSampleScale);
	Cloud->SetShadowViewSampleCountScale(C.ShadowSampleScale * Q.CloudSampleScale);
	Cloud->SetShadowTracingDistance(C.ShadowTracingKm);
	Cloud->SetbUsePerSampleAtmosphericLightTransmittance(C.bPerSampleTransmittance);
	Cloud->SetGroundAlbedo(C.GroundAlbedo);
	Cloud->SetSkyLightCloudBottomOcclusion(C.SkyLightBottomOcclusion);
	const FString& Path = Q.CloudMaterial.IsEmpty() ? C.Material : Q.CloudMaterial;
	if (!Path.IsEmpty())
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path);
		if (M)
		{
			CloudMID = UMaterialInstanceDynamic::Create(M, this);
			for (const TPair<FName, float>& P : C.Scalars)
			{
				CloudMID->SetScalarParameterValue(P.Key, P.Value);
			}
			for (const TPair<FName, FLinearColor>& P : C.Vectors)
			{
				CloudMID->SetVectorParameterValue(P.Key, P.Value);
			}
			Cloud->SetMaterial(CloudMID);
			UpdateCloudSun();
		}
		else
		{
			UE_LOG(LogRTSLook, Warning, TEXT("as nuvens: o material %s não carregou"), *Path);
		}
	}
	if (Sun)
	{
		// a sombra no ar faz os raios pelos buracos; no chão fica 0 com o sol baixo (§7.4). O mapa de sombra só existe
		// quando alguém usa: com o sol a 8° a Unreal dobra as amostras dele e ele custava ~2,9 ms à toa (16/10)
		Sun->bCastCloudShadows = C.ShadowOnAtmosphere > 0.0f || C.ShadowOnSurface > 0.0f || Q.bCloudShadowMap;
		Sun->CloudShadowOnAtmosphereStrength = C.ShadowOnAtmosphere;
		Sun->CloudShadowOnSurfaceStrength = C.ShadowOnSurface;
		Sun->CloudScatteredLuminanceScale = C.ScatteredLuminanceScale;
		Sun->MarkRenderStateDirty();
	}
	UE_LOG(LogRTSLook, Log, TEXT("as nuvens: camada %.1f–%.1f km, material %s"), C.BottomKm, C.BottomKm + C.HeightKm, Path.IsEmpty() ? TEXT("(o da Unreal)") : *Path);
}

void ALookDirector::UpdateTilt(float TopElevationDeg)
{
	const FLookSky& K = Recipe->Sky;
	const float Span = FMath::Max(K.TiltFullTopDeg - K.TiltStartTopDeg, 0.1f);
	SkyT = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((TopElevationDeg - K.TiltStartTopDeg) / Span, 0.0f, 1.0f));
	if (Sky && K.TiltAerialDistanceScale > 0.0f)
	{
		// a perspectiva aérea do RTS (×40) desce pra a do céu; a mistura é no logaritmo (40 → 1,5 é uma razão)
		const float Base = FMath::Max(Recipe->Atmosphere.AerialDistanceScale, 0.01f);
		const float Scale = FMath::Exp(FMath::Lerp(FMath::Loge(Base), FMath::Loge(K.TiltAerialDistanceScale), SkyT));
		if (LastAerialScale < 0.0f || FMath::Abs(Scale - LastAerialScale) > 0.005f * Scale)
		{
			LastAerialScale = Scale;
			Sky->SetAerialPespectiveViewDistanceScale(Scale);
		}
	}
	if (Fog && K.TiltFogFalloffScale > 0.0f)
	{
		// a camada de poeira mais rasa: o céu baixo e as nuvens saem da névoa, o chão longe continua enevoado (sem degrau)
		const float Falloff = Recipe->Fog.Falloff * FMath::Exp(FMath::Lerp(0.0f, FMath::Loge(K.TiltFogFalloffScale), SkyT));
		if (LastFalloff < 0.0f || FMath::Abs(Falloff - LastFalloff) > 0.01f * Falloff)
		{
			LastFalloff = Falloff;
			Fog->SetFogHeightFalloff(Falloff);
		}
	}
	// (16/10) a câmera deitada: a exposição desce e o reflexo do céu no lago diminui (o teste do sol na tela estourava)
	// (16/10) a câmera (exposição, luzes, bloom, cor) anda pela própria janela: só com o céu claro entrando no quadro
	const float ExpSpan = FMath::Max(K.TiltExposureFullTopDeg - K.TiltExposureStartTopDeg, 0.1f);
	const float ExpT = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((TopElevationDeg - K.TiltExposureStartTopDeg) / ExpSpan, 0.0f, 1.0f));
	if (FMath::Abs(SkyT - LastTiltT) > 0.01f || LastTiltT < 0.0f || FMath::Abs(ExpT - LastExpT) > 0.01f || LastExpT < 0.0f)
	{
		LastTiltT = SkyT;
		LastExpT = ExpT;
		if (Post && K.TiltExposureBias != 0.0f)
		{
			Post->Settings.bOverride_AutoExposureBias = true;
			Post->Settings.AutoExposureBias = Recipe->Post.Bias + ExpT * K.TiltExposureBias;
		}
		if (Post && K.TiltLocalExposureHighlight > 0.0f)
		{
			Post->Settings.bOverride_LocalExposureHighlightContrastScale = true;
			Post->Settings.LocalExposureHighlightContrastScale = FMath::Lerp(Recipe->Post.LocalExposureHighlight, K.TiltLocalExposureHighlight, ExpT);
		}
		if (Post && K.TiltSaturation > 0.0f)
		{
			const float Sat = Recipe->Post.Saturation * FMath::Lerp(1.0f, K.TiltSaturation, ExpT);
			Post->Settings.bOverride_ColorSaturation = true;
			Post->Settings.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0);
		}
		if (Post && K.TiltWhiteTemp > 0.0f)
		{
			const float Base = Recipe->Post.WhiteTemp > 0.0f ? Recipe->Post.WhiteTemp : 6500.0f;
			Post->Settings.bOverride_WhiteTemp = true;
			Post->Settings.WhiteTemp = FMath::Lerp(Base, K.TiltWhiteTemp, ExpT);
		}
		if (Post && K.TiltBloom > 0.0f)
		{
			Post->Settings.bOverride_BloomIntensity = true;
			Post->Settings.BloomIntensity = FMath::Lerp(Recipe->Post.Bloom, K.TiltBloom, ExpT);
		}
		if (Recipe->Water.bOverride && K.TiltWaterReflection != 1.0f)
		{
			const float R = Recipe->Water.SkyReflection * FMath::Lerp(1.0f, K.TiltWaterReflection, SkyT);
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& W : WaterMIDs)
			{
				if (UMaterialInstanceDynamic* D = W.Get())
				{
					D->SetScalarParameterValue(TEXT("SkyReflection"), R);
				}
			}
		}
	}
	if (bRecapturePending && SkyT <= 0.0f && SkyLight)
	{
		SkyLight->RecaptureSky();
		bRecapturePending = false;
	}
	if (Cloud && bCloudsAllowed)
	{
		// liga antes do horizonte entrar e desliga 2° abaixo (a folga contra o liga-desliga)
		const float Limit = Recipe->Clouds.EnableTopDeg - (CloudShown == 1 ? 2.0f : 0.0f);
		const int8 Want = (TopElevationDeg > Limit || CVarCloudsAlways.GetValueOnGameThread() != 0) ? 1 : 0;
		// a câmera acima da base da camada (a vista de teste): o traçado começa na câmera
		const APlayerController* PCc = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const double CamKm = (PCc && PCc->PlayerCameraManager) ? PCc->PlayerCameraManager->GetCameraLocation().Z * 1.0e-5 : 0.0;
		const int8 Above = CamKm > Recipe->Clouds.BottomKm * 0.5 ? 1 : 0;
		if (Above != CloudAbove)
		{
			CloudAbove = Above;
			Cloud->SetTracingStartDistanceFromCamera(Above ? 0.0f : Recipe->Clouds.StartFromCameraKm);
		}
		if (Want != CloudShown)
		{
			CloudShown = Want;
			Cloud->SetVisibility(Want == 1);
			UE_LOG(LogRTSLook, Log, TEXT("as nuvens %s (topo da tela a %.1f°)"), Want ? TEXT("ligadas") : TEXT("desligadas"), TopElevationDeg);
		}
	}
}

void ALookDirector::UpdateCloudShadow()
{
	// (16/10) a sombra das nuvens é um termo da função de luz do sol (a M_SmokeShadowLF, que o BlastFx põe no sol depois do
	// começo da partida): quando ela aparece, os parâmetros da receita vão pra ela uma vez; com Strength 0 ela fica igual
	if (!Sun || Recipe->CloudShadow.Strength <= 0.0f)
	{
		return;
	}
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Sun->LightFunctionMaterial);
	if (!MID || CloudShadowMID.Get() == MID)
	{
		return;
	}
	CloudShadowMID = MID;
	const FLookCloudShadow& S = Recipe->CloudShadow;
	MID->SetScalarParameterValue(TEXT("CloudShadowStrength"), S.Strength);
	MID->SetScalarParameterValue(TEXT("CloudShadowScaleM"), S.ScaleM);
	MID->SetScalarParameterValue(TEXT("CloudShadowCover"), S.Cover);
	MID->SetScalarParameterValue(TEXT("CloudShadowSoft"), S.Soft);
	MID->SetVectorParameterValue(TEXT("CloudShadowWindMS"), S.WindMS);
	const FVector ToSun = -Sun->GetComponentRotation().Vector();
	MID->SetVectorParameterValue(TEXT("CloudSunDir"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0.0f));
	UE_LOG(LogRTSLook, Log, TEXT("a sombra das nuvens na função de luz do sol: força %.2f, manchas de %.0f m"), S.Strength, S.ScaleM);
}

void ALookDirector::UpdateCloudSun()
{
	// o corredor sem torres na direção do sol (±15–20°): o sol é escolhido em jogo pela câmera inicial, então vai pro
	// material aqui (a 2ª opinião: torre no azimute do sol tapa o disco, sombreia o ar da batalha e estoura o clarão)
	if (CloudMID && Sun)
	{
		const FVector ToSun = -Sun->GetComponentRotation().Vector();
		const FVector2D D = FVector2D(ToSun.X, ToSun.Y).GetSafeNormal();
		CloudMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(D.X, D.Y, 0.0f, 0.0f));
	}
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

void ALookDirector::UpdateFogOfWar()
{
	UTexture2D* Vision = GFowVision.Get();
	const int32 Style = Vision ? FMath::Clamp(CVarFogStyle.GetValueOnGameThread(), 0, 3) : 0;
	// o raio do desfoque (fração da altura da tela): 0 na sombra, 0,006 no desfoque e, no 3, entrando com a câmera deitada
	const float WantBlur = Style == 2 ? 0.006f : (Style == 3 ? 0.006f * SkyT : 0.0f);
	// a função de luz do sol é do BlastFx e nasce depois do começo: quando ela troca, o termo vai de novo
	UMaterialInstanceDynamic* SunMID = Sun ? Cast<UMaterialInstanceDynamic>(Sun->LightFunctionMaterial) : nullptr;
	if (Style == FowStyleApplied && GFowSerial == FowSerialApplied && SunMID == FowSunMID.Get())
	{
		// só a câmera deitou ou levantou: o raio do desfoque acompanha
		if (UMaterialInstanceDynamic* B = FowBlurMID.Get(); B && Style == 3 && FMath::Abs(WantBlur - FowBlurApplied) > 0.0002f)
		{
			B->SetScalarParameterValue(TEXT("BlurFrac"), WantBlur);
			FowBlurApplied = WantBlur;
		}
		return;
	}
	FowStyleApplied = Style;
	FowSerialApplied = GFowSerial;
	FowSunMID = SunMID;
	// 1: a sombra de nuvem (o termo fow da M_SmokeShadowLF: fora da visão o sol some e fica a luz do céu)
	if (SunMID)
	{
		SunMID->SetScalarParameterValue(TEXT("FowOn"), Style == 1 || Style == 3 ? 1.0f : 0.0f);
		if (Style == 1 || Style == 3)
		{
			SunMID->SetTextureParameterValue(TEXT("FogVision"), Vision);
			SunMID->SetVectorParameterValue(TEXT("FowMapMinInvSize"), GFowMapMinInvSize);
			SunMID->SetVectorParameterValue(TEXT("FowTexel"), GFowTexel);
		}
	}
	// o pós do Look (vivo pela lista de blendables enquanto ligado), nos dois estilos: a gradação do fora da visão (a do
	// AoE IV: um tom frio, menos saturação e mais escuro, sumindo com a névoa da distância pra o horizonte ficar com a
	// poeira) e, no 2, o desfoque
	FPostProcessSettings& S = Post->Settings;
	UMaterialInstanceDynamic* Blur = FowBlurMID.Get();
	if (Blur)
	{
		S.WeightedBlendables.Array.RemoveAll([Blur](const FWeightedBlendable& B) { return B.Object == Blur; });
	}
	if (Style >= 1)
	{
		if (!Blur)
		{
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/RTSLook/Fow/M_FowBlur.M_FowBlur")))
			{
				Blur = UMaterialInstanceDynamic::Create(M, this);
				FowBlurMID = Blur;
			}
		}
		if (Blur)
		{
			Blur->SetTextureParameterValue(TEXT("FogVision"), Vision);
			Blur->SetVectorParameterValue(TEXT("FowMapMinInvSize"), GFowMapMinInvSize);
			Blur->SetVectorParameterValue(TEXT("FowTexel"), GFowTexel);
			// 1 e 3: a sombra de nuvem leva a gradação fria (o sol já saiu pela função de luz; no 3 o desfoque entra ao
			// deitar); 2: o desfoque, mais escuro e menos saturado, sem tom
			const bool bShade = Style != 2;
			Blur->SetScalarParameterValue(TEXT("BlurFrac"), WantBlur);
			FowBlurApplied = WantBlur;
			Blur->SetScalarParameterValue(TEXT("UnseenDark"), bShade ? 0.8f : 0.7f);
			// o nunca visto esconde o mapa (o preto do ZH; no passeio de 17/10 o 0,45 deixava ver o lago e o relevo)
			Blur->SetScalarParameterValue(TEXT("NeverDark"), 0.06f);
			Blur->SetScalarParameterValue(TEXT("UnseenDesat"), bShade ? 0.3f : 0.5f);
			// o tom do fora da visão do AoE IV (FOW Midtone Tint 8065737F: 101/115/127 → 0,80/0,91/1,0)
			Blur->SetVectorParameterValue(TEXT("Tint"), bShade ? FLinearColor(0.80f, 0.91f, 1.0f, 1.0f) : FLinearColor::White);
			S.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Blur));
		}
	}
	UE_LOG(LogRTSLook, Log, TEXT("a névoa de guerra do Look: estilo %d (0 o jogo, 1 a sombra, 2 o desfoque, 3 os dois), visão %dx%d, função de luz do sol %s, desfoque %s"),
		Style, Vision ? Vision->GetSizeX() : 0, Vision ? Vision->GetSizeY() : 0, SunMID ? TEXT("sim") : TEXT("não"), Blur ? TEXT("sim") : TEXT("não"));
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
		UpdateCloudSun();
	}
	UpdateFogOfWar();
	{
		// (16/10, o céu contra o gameplay_v3) -RTSLookCycle=a,b,c: as receitas em sequência na mesma partida, uma a cada
		// -RTSLookCycleStepS (3 s) a partir de -RTSLookCycleStartS (1,5 s), pra fotografar várias na mesma pose numa rodada
		// só da fila. O sol fica onde a câmera inicial pôs (só a altura, a cor e a força mudam) e a luz do céu recaptura na
		// hora (no jogo ela espera a câmera voltar ao normal): a receita escolhida passa depois por uma rodada sozinha.
		static TArray<FString> CycleNames;
		static float CycleStartS = 1.5f, CycleStepS = 3.0f;
		static int32 CycleIndex = -1;
		static bool bCycleParsed = false;
		if (!bCycleParsed)
		{
			bCycleParsed = true;
			FString List;
			// (sem parar na vírgula: o FParse::Value padrão corta a lista na 1ª)
			if (FParse::Value(FCommandLine::Get(), TEXT("-RTSLookCycle="), List, false))
			{
				List.ParseIntoArray(CycleNames, TEXT(","), true);
				FParse::Value(FCommandLine::Get(), TEXT("-RTSLookCycleStartS="), CycleStartS);
				FParse::Value(FCommandLine::Get(), TEXT("-RTSLookCycleStepS="), CycleStepS);
			}
		}
		const float Now = GetWorld() ? float(GetWorld()->GetTimeSeconds() - GLookCycleZero) : 0.0f;     // a hora da cena
		const int32 Want = (CycleNames.Num() > 0 && Now >= CycleStartS) ? FMath::FloorToInt32((Now - CycleStartS) / FMath::Max(CycleStepS, 0.5f)) : -1;
		if (Want >= 0 && Want < CycleNames.Num() && Want != CycleIndex && Sun)
		{
			CycleIndex = Want;
			const FString& Name = CycleNames[Want];
			const FString Path = FString::Printf(TEXT("/RTSLook/Recipes/DA_Look_%s.DA_Look_%s"), *Name, *Name);
			if (ULookRecipe* Next = LoadObject<ULookRecipe>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				const float SunYaw = Sun->GetComponentRotation().Yaw;
				Recipe = Next;
				GLookCycleApplying = true;
				ApplyRecipe();
				GLookCycleApplying = false;
				Sun->SetWorldRotation(FRotator(-Recipe->Sun.Elevation, SunYaw, 0.0f));
				UpdateCloudSun();
				if (SkyLight && !SkyLight->IsRealTimeCaptureEnabled())
				{
					SkyLight->RecaptureSky();
					bRecapturePending = false;
				}
				UE_LOG(LogRTSLook, Log, TEXT("ciclo: receita %s aos %.1f s"), *Name, Now);
			}
			else
			{
				UE_LOG(LogRTSLook, Warning, TEXT("ciclo: a receita %s não existe"), *Path);
			}
		}
	}
	UpdateFocus(Dist);
	UpdateCloudShadow();
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		// o topo da tela em relação ao horizonte: a inclinação (negativa olhando pra baixo) mais meio campo vertical
		int32 W = 16, H = 9;
		PC->GetViewportSize(W, H);
		const float Aspect = H > 0 ? float(W) / float(H) : 16.0f / 9.0f;
		const float HalfH = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f);
		const float HalfV = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(HalfH) / FMath::Max(Aspect, 0.1f)));
		UpdateTilt(static_cast<float>(PC->PlayerCameraManager->GetCameraRotation().Pitch) + HalfV);
	}
}
