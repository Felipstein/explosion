#pragma once

// Os dois modos do jogo no mesmo mapa: o menu (o mapa ao fundo, como o ShellMap
// do Zero Hour) e o skirmish (a partida). A troca é por OpenLevel com ?game=
// (URTSSessionSubsystem).

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RTSGameModes.generated.h"

UCLASS()
class RTSGAME_API ARTSMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSMenuGameMode();
};

UCLASS()
class RTSGAME_API ARTSSkirmishGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSSkirmishGameMode();
	virtual void StartPlay() override;
};
