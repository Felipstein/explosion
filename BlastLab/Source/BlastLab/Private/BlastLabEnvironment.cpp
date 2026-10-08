#include "BlastLabEnvironment.h"

#include "Celestial.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

namespace
{
	// reference/webgl/js/celestial.js
	constexpr float SunLux = 128000.0f;          // no topo da atmosfera
	constexpr float MoonRender = 2.2e-3f;        // lua do render / sol (a física é 2.5e-6)
	// luz do sol refletida pelo regolito: (1.165, 0.976, 0.753) com luminância 1;
	// a cor da luz guarda no máximo 1 por canal, então vai normalizada pelo
	// vermelho e a intensidade leva o 1.165 de volta
	const FLinearColor MoonTint(1.0f, 0.976f / 1.165f, 0.753f / 1.165f);
	constexpr float MoonTintPeak = 1.165f;
	// quanto a adaptação às explosões pode fechar (js/exposure.js): de dia
	// 0.3 stop (§4d A4: "exposição da RTS varia < 0.3 EV", 07/10; era meio stop
	// acima da exposição do meio-dia, aeExpDay = 11.4 por unidade do render),
	// até 8 stops à noite (~6.5: uma cena iluminada pelo fogo fica tão clara
	// quanto o dia; a noite vai em A/B em movimento pro Felipe)
	// 07/10 (painel da sessão de arte, rodada f): às 16 h a morte de tanque
	// fechava −0.6 EV (céu 212 → 170) — a parcela da hora somava ~0.3 aos 0.3 do
	// dia; sem o halo, a tela ESCURECIA no clarão. Teto de 0.25 EV de dia e no fim
	// da tarde pra explosão do tamanho de unidade; à noite segue abrindo
	constexpr float ExposureDay = 11.4f;
	constexpr float DayDarkenEV = -0.05f, MinDarkenEV = 0.25f, MaxDarkenEV = 8.0f;
	// limiar do bloom do WebGL (js/main.js bloomThreshold), na radiância do render (sol = 1)
	constexpr float BloomThresholdRender = 1.7f;
	// O céu iluminado pela lua, na tela: com o luar 880× o físico ele saía o
	// azul do dia, 1.7× mais claro que o chão (medido 06/10, câmera baixa às
	// 22:30). Nas noites aprovadas da arte conceitual o céu é 0.30× (Gemini) e
	// 0.80× (GPT) o chão iluminado (captures/concept/11_night_tilt): a média
	// geométrica, 0.49, pede 0.49/1.70 = 0.29.
	constexpr float NightSkyScale = 0.29f;
	constexpr float MoonBoost = 880.0f;          // lua do render / física (celestial.js)

	// liga ou tira a luz dos canais dos volumes, mantendo o 0 (cenário): o 1 (as
	// explosões, que também ouvem a luz do fogo de dentro) e o 2 (a poeira, que só
	// ouve o céu: acesa pelo fogo de dentro ela virava a "panqueca creme", 07/10)
	void SetVolumeChannel(ULightComponent* L, bool bOn)
	{
		if (L && (L->LightingChannels.bChannel1 != bOn || L->LightingChannels.bChannel2 != bOn))
		{
			L->SetLightingChannels(L->LightingChannels.bChannel0, bOn, bOn);
		}
	}
}

ABlastLabEnvironment::ABlastLabEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

ABlastLabEnvironment* ABlastLabEnvironment::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABlastLabEnvironment> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ABlastLabEnvironment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AppliedTime = -1.0f;
	Apply();
}

void ABlastLabEnvironment::BeginPlay()
{
	Super::BeginPlay();
	if (PostProcess)
	{
		FPostProcessSettings& P = PostProcess->Settings;
		// calibração da medição sem remontar o mapa (Tools/blast.ps1 view -Cvars não alcança o pós-processo)
		float Value;
		if (FParse::Value(FCommandLine::Get(), TEXT("-AELow="), Value)) { P.bOverride_AutoExposureLowPercent = true; P.AutoExposureLowPercent = Value; }
		if (FParse::Value(FCommandLine::Get(), TEXT("-AEHigh="), Value)) { P.bOverride_AutoExposureHighPercent = true; P.AutoExposureHighPercent = Value; }
		if (FParse::Value(FCommandLine::Get(), TEXT("-AEBias="), Value)) { P.bOverride_AutoExposureBias = true; P.AutoExposureBias = Value; }
		if (ExposureMask && !FParse::Param(FCommandLine::Get(), TEXT("NoMeterMask")))
		{
			// textura pequena: a medição lê ela em coordenadas de tela, sem detalhe fino
			MeterMask = UKismetRenderingLibrary::CreateRenderTarget2D(this, 128, 72, RTF_R8, FLinearColor::White);
			MeterMaskMaterial = UMaterialInstanceDynamic::Create(ExposureMask, this);
			P.bOverride_AutoExposureMeterMask = true;
			P.AutoExposureMeterMask = MeterMask;
		}
	}
	FString Air;
	if (FParse::Value(FCommandLine::Get(), TEXT("Air="), Air) && Air == TEXT("desert"))
	{
		bDesertAir = true;
	}
	bBacklight = FParse::Value(FCommandLine::Get(), TEXT("Backlight="), BacklightDeg);
	ApplyAir();
	AppliedTime = -1.0f;
	KeyLight = -1;
	Apply();
}

void ABlastLabEnvironment::UpdateMeterMask()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!MeterMask || !MeterMaskMaterial || !PC || !PC->PlayerCameraManager || !Viewport)
	{
		return;
	}
	FVector2D Size;
	Viewport->GetViewportSize(Size);
	if (Size.X < 1.0 || Size.Y < 1.0)
	{
		return;
	}
	const FRotationMatrix View(PC->PlayerCameraManager->GetCameraRotation());
	// a câmera guarda o fov horizontal de 16:9 com o vertical fixo (BlastLabCamera)
	const float TanY = FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()) * 0.5f) * 9.0f / 16.0f;
	MeterMaskMaterial->SetVectorParameterValue(TEXT("Forward"), FLinearColor(View.GetScaledAxis(EAxis::X)));
	MeterMaskMaterial->SetVectorParameterValue(TEXT("Right"), FLinearColor(View.GetScaledAxis(EAxis::Y)));
	MeterMaskMaterial->SetVectorParameterValue(TEXT("Up"), FLinearColor(View.GetScaledAxis(EAxis::Z)));
	MeterMaskMaterial->SetScalarParameterValue(TEXT("TanX"), TanY * float(Size.X / Size.Y));
	MeterMaskMaterial->SetScalarParameterValue(TEXT("TanY"), TanY);
	MeterMaskMaterial->SetScalarParameterValue(TEXT("Height"), FMath::Max(PC->PlayerCameraManager->GetCameraLocation().Z * 0.01f, 0.01f));
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, MeterMask, MeterMaskMaterial);
}

namespace
{
	// Ar de deserto (poeira mineral; AERONET no Saara: AOD 0.3–0.5 de média,
	// expoente de Ångström ~0.2–0.5, quase cinza; albedo simples ~0.90 no azul e
	// 0.97 no vermelho; camada de 1–3 km; assimetria g ~0.7). A atmosfera da
	// Unreal vem com o aerossol limpo do artigo do Hillaire (β 0.004/km, 1.2 km:
	// AOD ~0.005); 40× com a camada a 2 km dá AOD ~0.3, a névoa de uma tarde
	// de deserto (não tempestade). A absorção vai junto (mesma razão 0.11) e a
	// cor do espalhamento puxa pro quente (a poeira come o azul).
	constexpr float DesertMieScale = 40.0f, DesertMieHeightKm = 2.0f, DesertMieAnisotropy = 0.7f;
	// (1 : 0.9 : 0.76 deixava o céu perto do sol bege-lavanda; com mais absorção no
	// azul ele fica pêssego-dourado como no vídeo: A/B captures/ue/_ab_backlight.jpg)
	const FLinearColor DesertMieTint(1.0f, 0.85f, 0.65f);
	// a névoa de altura (a do chão até o horizonte) mais densa e quente
	constexpr float DesertFogDensity = 2.5f;
	const FLinearColor DesertFogTint(1.0f, 0.86f, 0.66f);
}

void ABlastLabEnvironment::SetDesertAir(bool bOn)
{
	bDesertAir = bOn;
	ApplyAir();
	AppliedTime = -1.0f;            // a névoa da hora também muda de cor
	Apply();
}

void ABlastLabEnvironment::ApplyAir()
{
	USkyAtmosphereComponent* A = Atmosphere ? Atmosphere->GetComponent() : nullptr;
	UExponentialHeightFogComponent* F = Fog ? Fog->GetComponent() : nullptr;
	if (!bAirDefaultsRead && A && F)
	{
		bAirDefaultsRead = true;
		DefaultMieScale = A->MieScatteringScale;
		DefaultMieAbsorption = A->MieAbsorptionScale;
		DefaultMieHeight = A->MieExponentialDistribution;
		DefaultMieAnisotropy = A->MieAnisotropy;
		DefaultMieColor = A->MieScattering;
		DefaultFogDensity = F->FogDensity;
	}
	if (!bAirDefaultsRead)
	{
		return;
	}
	A->SetMieScatteringScale(DefaultMieScale * (bDesertAir ? DesertMieScale : 1.0f));
	A->SetMieAbsorptionScale(DefaultMieAbsorption * (bDesertAir ? DesertMieScale : 1.0f));
	A->SetMieExponentialDistribution(bDesertAir ? DesertMieHeightKm : DefaultMieHeight);
	A->SetMieAnisotropy(bDesertAir ? DesertMieAnisotropy : DefaultMieAnisotropy);
	// -AirTint=r,g,b troca o tom da poeira pra A/B (mais absorção no azul: mais laranja perto do sol)
	static const FLinearColor Tint = []
	{
		FLinearColor T = DesertMieTint;
		FString S;
		TArray<FString> P;
		if (FParse::Value(FCommandLine::Get(), TEXT("AirTint="), S, false) && S.ParseIntoArray(P, TEXT(",")) == 3)
		{
			T = FLinearColor(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
		}
		return T;
	}();
	A->SetMieScattering(bDesertAir ? DefaultMieColor * Tint : DefaultMieColor);
	F->SetFogDensity(DefaultFogDensity * (bDesertAir ? DesertFogDensity : 1.0f));
}

void ABlastLabEnvironment::SetTimeOfDay(float Hours)
{
	TimeOfDay = FMath::Fmod(FMath::Fmod(Hours, 24.0f) + 24.0f, 24.0f);
	Apply();
}

void ABlastLabEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAutoCycle)
	{
		SetTimeOfDay(TimeOfDay + DeltaSeconds * CycleSpeed);     // tempo real, como no WebGL
	}
	UpdateMeterMask();
	UpdateBloomThreshold();
	if (bBacklight && GetWorld() && GetWorld()->GetTimeSeconds() > 1.5)
	{
		bBacklight = false;          // travado na câmera do começo: orbitando, o sol não gira junto
	}
	if (bBacklight)
	{
		// o sol na frente da câmera: o azimute dele vira o da direção dela + BacklightDeg
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			const FCelestial S = FCelestial::Sun(TimeOfDay, LatitudeDeg, DayOfYear);
			const float CamYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
			const float SunYaw = S.Direction.Rotation().Yaw;
			const float Want = FRotator::NormalizeAxis(CamYaw + BacklightDeg - SunYaw);
			if (FMath::Abs(FRotator::NormalizeAxis(Want - SunAzimuthOffsetDeg)) > 0.5f)
			{
				SunAzimuthOffsetDeg = Want;
				AppliedTime = -1.0f;
				Apply();
			}
		}
	}
}

void ABlastLabEnvironment::UpdateBloomThreshold()
{
	// O bloom do WebGL (js/post.js) é feito na radiância do render ANTES da
	// exposição, com limiar 1.7 (× 128 000 cd/m²): só o fogo e o traçante
	// passam, de dia e de noite. O da Unreal compara a luminância já exposta
	// (PostProcessBloom.usf: Luminance·ExposureScale − BloomThreshold), então o
	// limiar segue a exposição (a do quadro anterior). Fixo em 1.7 ele pegava
	// coisa 18× mais fraca de dia e quase tudo à noite.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!PostProcess || !LP || !LP->ViewportClient || !LP->ViewportClient->Viewport || !GetWorld()->Scene)
	{
		return;
	}
	FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(LP->ViewportClient->Viewport, GetWorld()->Scene,
		LP->ViewportClient->EngineShowFlags).SetRealtimeUpdate(true));
	FVector Location;
	FRotator Rotation;
	const FSceneView* View = LP->CalcSceneView(&Family, Location, Rotation, LP->ViewportClient->Viewport);
	const float Exposure = View ? View->GetLastEyeAdaptationExposure() : 0.0f;
	if (Exposure > 0.0f)
	{
		FPostProcessSettings& P = PostProcess->Settings;
		P.bOverride_BloomThreshold = true;
		P.BloomThreshold = BloomThresholdRender * SunLux * Exposure;
	}
}

bool ABlastLabEnvironment::TablePosition(int32 Count, int32& I0, float& Frac) const
{
	const int32 N = FMath::Min(Count, CurveHours.Num());
	if (N < 2)
	{
		return false;
	}
	// a tabela vai de 0 a 24 h em passos iguais
	const float Step = (CurveHours[N - 1] - CurveHours[0]) / float(N - 1);
	const float X = FMath::Clamp((TimeOfDay - CurveHours[0]) / Step, 0.0f, float(N - 1));
	I0 = FMath::Min(FMath::FloorToInt32(X), N - 2);
	Frac = X - I0;
	return true;
}

float ABlastLabEnvironment::Sample(const TArray<float>& Curve, float Fallback) const
{
	int32 I0;
	float F;
	return TablePosition(Curve.Num(), I0, F) ? FMath::Lerp(Curve[I0], Curve[I0 + 1], F) : Fallback;
}

FLinearColor ABlastLabEnvironment::Sample(const TArray<FLinearColor>& Curve, const FLinearColor& Fallback) const
{
	int32 I0;
	float F;
	return TablePosition(Curve.Num(), I0, F) ? FMath::Lerp(Curve[I0], Curve[I0 + 1], F) : Fallback;
}

void ABlastLabEnvironment::Apply()
{
	if (FMath::IsNearlyEqual(TimeOfDay, AppliedTime, 1e-4f))
	{
		return;
	}
	AppliedTime = TimeOfDay;
	FCelestial S = FCelestial::Sun(TimeOfDay, LatitudeDeg, DayOfYear);
	const FCelestial M = FCelestial::Moon(TimeOfDay, LatitudeDeg, DayOfYear);
	if (SunAzimuthOffsetDeg != 0.0f)
	{
		S.Direction = FRotator(0.0f, SunAzimuthOffsetDeg, 0.0f).RotateVector(S.Direction);     // contraluz (-Backlight=)
	}
	SunAltDeg = S.AltitudeDeg;
	// a luz principal: o sol enquanto estiver acima do horizonte, depois a lua (main.js keyIsSun)
	const int32 Key = S.AltitudeDeg > 0.0f ? 0 : 1;
	KeyDir = Key == 0 ? S.Direction : M.Direction;
	KeyAltDeg = Key == 0 ? S.AltitudeDeg : M.AltitudeDeg;

	// a luz anda no sentido oposto ao do astro
	if (Sun)
	{
		Sun->SetActorRotation((-S.Direction).Rotation());
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(SunLux);
			// aceso até o fim do crepúsculo astronômico: o céu da tarde e a faixa roxa
			// vêm do sol abaixo do horizonte (o chão escurece sozinho, a atmosfera zera
			// a luz que chega); a sombra dele só custa enquanto ele ilumina
			L->SetVisibility(S.AltitudeDeg > -18.0f);
			if (L->CastShadows != (S.AltitudeDeg > -1.0f))
			{
				L->SetCastShadows(S.AltitudeDeg > -1.0f);
			}
			// As explosões (canal 1) só com a luz que ilumina de fato: o volume faz
			// uma passada de luz inteira por luz que alcança o canal dele, sem olhar
			// a intensidade (HeterogeneousVolumes.cpp GatherLights). Abaixo do
			// horizonte o sol só serve ao céu
			SetVolumeChannel(L, S.AltitudeDeg > -2.0f);
			if (Key != KeyLight)
			{
				L->SetForwardShadingPriority(Key == 0 ? 1 : 0);
			}
		}
	}
	if (Moon)
	{
		Moon->SetActorRotation((-M.Direction).Rotation());
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			L->SetIntensity(SunLux * MoonRender * MoonTintPeak);
			L->SetLightColor(MoonTint);
			L->SetVisibility(M.AltitudeDeg > -2.0f);
			if (L->CastShadows != (Key == 1))
			{
				L->SetCastShadows(Key == 1);
			}
			// de dia a lua no céu não acende nada perto do sol, mas custava nas
			// explosões uma passada de luz inteira (medido 06/10: 17–21 ms de volume)
			SetVolumeChannel(L, Key == 1);
			if (Key != KeyLight)
			{
				L->SetForwardShadingPriority(Key == 1 ? 1 : 0);
			}
		}
	}
	KeyLight = Key;

	// o céu do WebGL: espalhamento múltiplo por hora (Tools/sky_ms_table.py)
	LastMultiScattering = Sample(CurveMultiScattering, 1.0f);
	if (Atmosphere && Atmosphere->GetComponent())
	{
		Atmosphere->GetComponent()->SetMultiScatteringFactor(LastMultiScattering);
	}
	// Quanto da luz do céu é da lua: o ganho das estrelas é E_render/E_físico =
	// 1/(1 − m + m/880), com m a parte da lua. A parte da lua escurece na tela
	// (o céu e a névoa, que é a cor do céu abaixo do horizonte) e volta à escala
	// física pra visibilidade das estrelas (M_Sky)
	LastStarGain = Sample(CurveStarGain, 1.0f);
	const float MoonShare = FMath::Clamp((1.0f - 1.0f / FMath::Max(LastStarGain, 1.0f)) / (1.0f - 1.0f / MoonBoost), 0.0f, 1.0f);
	const float NightSky = FMath::Lerp(1.0f, NightSkyScale, MoonShare);
	const float SkyPhys = 1.0f - MoonShare + MoonShare / MoonBoost;
	// a névoa com a cor do céu do WebGL abaixo do horizonte. A parte direcional a
	// Unreal multiplica pela luminância do sol (FogRendering.cpp) e, com a Sky
	// Atmosphere no projeto, pela fase uniforme 1/4π (HeightFogCommon.ush)
	if (Fog && Fog->GetComponent())
	{
		const FLinearColor AirTint = bDesertAir ? DesertFogTint : FLinearColor::White;
		Fog->GetComponent()->SetFogInscatteringColor(Sample(CurveFog, FLinearColor::Black) * NightSky * AirTint);
		Fog->GetComponent()->SetDirectionalInscatteringColor(Sample(CurveFogDirectional, FLinearColor::Black) * (4.0f * PI / SunLux * NightSky) * AirTint);
		Fog->GetComponent()->SetDirectionalInscatteringExponent(Sample(CurveFogExponent, 4.0f));
	}

	// exposição analítica da hora (a do WebGL); a adaptação às explosões só
	// escurece a partir dela. O histograma leva a cena a 0.18·2^bias: com o piso
	// em EV + bias, o repouso é 2^−EV (build_scene.py AE_BIAS)
	TargetEV = Sample(CurveEV100, 12.85f);
	if (PostProcess)
	{
		FPostProcessSettings& P = PostProcess->Settings;
		const float Bias = P.bOverride_AutoExposureBias ? P.AutoExposureBias : 0.0f;
		const float WebGLExposure = SunLux * FMath::Exp2(-TargetEV);
		const float Darken = FMath::Clamp(DayDarkenEV + FMath::Log2(WebGLExposure / ExposureDay), MinDarkenEV, MaxDarkenEV);
		P.bOverride_AutoExposureMinBrightness = true;
		P.bOverride_AutoExposureMaxBrightness = true;
		P.AutoExposureMinBrightness = TargetEV + Bias;
		P.AutoExposureMaxBrightness = TargetEV + Bias + Darken;
	}
	LastPurkinje = Sample(CurvePurkinje, 1.0e5f);
	if (Params && GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Params, TEXT("Purkinje"), LastPurkinje);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Params, TEXT("StarGain"), LastStarGain);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Params, TEXT("NightSky"), NightSky);
		UKismetMaterialLibrary::SetScalarParameterValue(this, Params, TEXT("SkyPhys"), SkyPhys);
	}
}
