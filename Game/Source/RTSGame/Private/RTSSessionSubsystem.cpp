#include "RTSSessionSubsystem.h"

#include "RTSNetSession.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void URTSSessionSubsystem::StartSkirmish()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSSkirmishGameMode"));
}

void URTSSessionSubsystem::ReturnToMenu()
{
	if (URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>())
	{
		Net->Leave();   // a partida em rede acabou: sai da sala
	}
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));
}

void URTSSessionSubsystem::ReturnToLobby()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));
}

void URTSSessionSubsystem::Quit(APlayerController* PC)
{
	UKismetSystemLibrary::QuitGame(GetGameInstance(), PC, EQuitPreference::Quit, false);
}
