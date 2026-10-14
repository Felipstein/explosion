#include "RTSGameModes.h"

#include "Engine/World.h"
#include "RTSCameraPawn.h"
#include "RTSHUD.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSSessionSubsystem.h"
#include "RTSNetSession.h"
#include "RTSGameData.h"
#include "LookDirector.h"
#include "Scalability.h"
#include "LookRecipe.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** O look do mapa (plugin RTSLook, sessão de gráficos): o diretor monta sol, céu, névoa e pós pela
	 *  receita que o Maps.json diz. Sem a receita importada (ou com -RTSNoLook, pro A/B), fica a luz do mapa. */
	void SpawnLook(UWorld* World)
	{
		if (!World || FParse::Param(FCommandLine::Get(), TEXT("RTSNoLook")))
		{
			return;
		}
		FRTSGameData Data;
		FString Error;
		const FRTSMapInfo* Info = Data.Load(Error) ? Data.FindMap(URTSSessionSubsystem::SkirmishMapName()) : nullptr;
		if (!Info || Info->LookRecipe.IsEmpty())
		{
			return;
		}
		// -RTSLookRecipe=<nome> troca a receita (o A/B do Look na mesma cena do teste rápido)
		FString RecipeName = Info->LookRecipe;
		FParse::Value(FCommandLine::Get(), TEXT("-RTSLookRecipe="), RecipeName);
		const FString Path = FString::Printf(TEXT("/RTSLook/Recipes/DA_Look_%s.DA_Look_%s"), *RecipeName, *RecipeName);
		ULookRecipe* Recipe = LoadObject<ULookRecipe>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Recipe)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS look: a receita %s não existe (rode o look_recipe_import.py): fica a luz do mapa"), *Path);
			return;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.bDeferConstruction = true;
		if (ALookDirector* Director = World->SpawnActor<ALookDirector>(ALookDirector::StaticClass(), FTransform::Identity, P))
		{
			Director->Recipe = Recipe;
			Director->FinishSpawning(FTransform::Identity);
			// o preset da luz pelo nível da Unreal desde o começo (14/10: ele nascia no ultra, com Lumen, e só baixava no
			// F5–F8; no Média a luz pesava como no Ultra); 1 baixa … 4 ultra (o Cinematic)
			const int32 Level = FMath::Clamp(Scalability::GetQualityLevels().EffectsQuality, 1, 4);
			Director->ApplyPreset(Level);
			UE_LOG(LogTemp, Log, TEXT("RTS look: receita %s, preset %d"), *RecipeName, Level);
		}
	}
}

void ARTSMenuGameMode::StartPlay()
{
	SpawnLook(GetWorld());
	Super::StartPlay();
}

ARTSMenuGameMode::ARTSMenuGameMode()
{
	DefaultPawnClass = ARTSCameraPawn::StaticClass();
	PlayerControllerClass = ARTSPlayerController::StaticClass();
	HUDClass = ARTSHUD::StaticClass();
}

ARTSSkirmishGameMode::ARTSSkirmishGameMode()
{
	DefaultPawnClass = ARTSCameraPawn::StaticClass();
	PlayerControllerClass = ARTSPlayerController::StaticClass();
	HUDClass = ARTSHUD::StaticClass();
}

void ARTSSkirmishGameMode::StartPlay()
{
	SpawnLook(GetWorld());   // antes da partida: a foto do minimapa e a cena de aquecimento já saem com o look
	// o jogador e a câmera já existem aqui (o login acontece ao carregar o mapa)
	URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	ARTSPlayerController* PC = Cast<ARTSPlayerController>(GetWorld()->GetFirstPlayerController());
	FString Error;
	const URTSNetSession* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
	if (Net && Net->IsActive() && !Net->IsHost())
	{
		// cliente da LAN: a partida começa quando a montagem do anfitrião chegar (URTSMatchSubsystem::Tick)
		Match->AwaitNetSetup(URTSSessionSubsystem::SkirmishMapName());
	}
	else if (!Match->StartSkirmish(URTSSessionSubsystem::SkirmishMapName(), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("RTS: não deu pra montar a partida: %s"), *Error);
		if (PC)
		{
			PC->SetError(FText::Format(NSLOCTEXT("RTSHUD", "SetupFailed", "Couldn't set up the match: {0}"), FText::FromString(Error)).ToString());
		}
	}
	else if (PC)
	{
		if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn()))
		{
			// 50 m pra dentro da borda: na borda exata meia tela mostrava fora do mapa
			Cam->SetBounds(FBox2D(FVector2D(-46200.0, -46200.0), FVector2D(46200.0, 46200.0)));
			Cam->FocusOn(Match->LocalStartCm());
		}
	}
	Super::StartPlay();
}
