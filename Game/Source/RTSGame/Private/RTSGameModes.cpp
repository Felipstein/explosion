#include "RTSGameModes.h"

#include "Engine/World.h"
#include "RTSCameraPawn.h"
#include "RTSHUD.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSSessionSubsystem.h"
#include "RTSNetSession.h"

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
