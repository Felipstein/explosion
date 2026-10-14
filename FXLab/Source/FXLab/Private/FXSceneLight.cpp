#include "FXSceneLight.h"

#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"

FFXSceneLight FFXSceneLight::Find(UWorld* World)
{
	FFXSceneLight S;
	if (!World)
	{
		return S;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("GameSun")) && It->GetLightComponent())
		{
			S.bGame = true;
			// a luz anda ao longo do X dela; pro sol é o contrário
			S.SunDir = -It->GetLightComponent()->GetComponentTransform().GetUnitAxis(EAxis::X);
			break;
		}
	}
	// o mapa do jogo (World_TestSalt2): o sol dele não tem a marca; com -GameLight vale o primeiro que houver
	if (!S.bGame && FParse::Param(FCommandLine::Get(), TEXT("GameLight")))
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (It->GetLightComponent())
			{
				S.bGame = true;
				S.SunDir = -It->GetLightComponent()->GetComponentTransform().GetUnitAxis(EAxis::X);
				break;
			}
		}
	}
	if (!S.bGame)
	{
		return S;
	}
	// A exposição travada do mapa, com a conta da Unreal (PostProcessEyeAdaptation.cpp, com
	// r.EyeAdaptation.LensAttenuation 0.78 e a faixa estendida): o branco fica em 2^EV100 e o
	// cinza em 18 % dele, então exposição = 2^(bias − EV100). Manual sem câmera física: 2^bias.
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (!It->bUnbound)
		{
			continue;
		}
		const FPostProcessSettings& Pp = It->Settings;
		S.Bias = Pp.bOverride_AutoExposureBias ? Pp.AutoExposureBias : 0.0;
		if (Pp.bOverride_AutoExposureMethod && Pp.AutoExposureMethod == AEM_Manual)
		{
			S.EmitScale = FMath::Pow(2.0, -S.Bias);
		}
		else if (Pp.bOverride_AutoExposureMinBrightness && Pp.bOverride_AutoExposureMaxBrightness)
		{
			if (!FMath::IsNearlyEqual(Pp.AutoExposureMinBrightness, Pp.AutoExposureMaxBrightness, 0.01f))
			{
				UE_LOG(LogTemp, Warning, TEXT("FXLab: a exposição do mapa não é travada (%.2f a %.2f): o fogo usa a média"),
					Pp.AutoExposureMinBrightness, Pp.AutoExposureMaxBrightness);
			}
			S.Ev100 = 0.5 * (Pp.AutoExposureMinBrightness + Pp.AutoExposureMaxBrightness);
			S.EmitScale = FMath::Pow(2.0, S.Ev100 - S.Bias);
		}
		break;
	}
	UE_LOG(LogTemp, Display, TEXT("FXLab: luz do jogo, EV100 %.2f, bias %.2f, fogo x%.0f, sol (%.2f, %.2f, %.2f)"),
		S.Ev100, S.Bias, S.EmitScale, S.SunDir.X, S.SunDir.Y, S.SunDir.Z);
	return S;
}
