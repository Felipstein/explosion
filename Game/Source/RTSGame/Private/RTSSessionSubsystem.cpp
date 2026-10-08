#include "RTSSessionSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void URTSSessionSubsystem::StartSkirmish()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSSkirmishGameMode"));
}

void URTSSessionSubsystem::ReturnToMenu()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));
}

void URTSSessionSubsystem::Quit(APlayerController* PC)
{
	UKismetSystemLibrary::QuitGame(GetGameInstance(), PC, EQuitPreference::Quit, false);
}
