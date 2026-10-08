#include "RTSGameModes.h"

#include "Engine/World.h"
#include "RTSCameraPawn.h"
#include "RTSHUD.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSSessionSubsystem.h"

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
	if (!Match->StartSkirmish(URTSSessionSubsystem::SkirmishMapName(), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("RTS: não deu pra montar a partida: %s"), *Error);
		if (PC)
		{
			PC->SetError(FString::Printf(TEXT("Não deu pra montar a partida: %s"), *Error));
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
