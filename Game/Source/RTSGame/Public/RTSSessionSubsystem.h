#pragma once

// O fluxo entre as telas (vive o jogo inteiro, entre um mapa e outro):
// menu → skirmish → menu → sair. Cada troca é um OpenLevel local com ?game=;
// a rede lockstep não usa a viagem de mapa da Unreal (docs/JOGO.md §3).

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RTSSessionSubsystem.generated.h"

class APlayerController;

UCLASS()
class RTSGAME_API URTSSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** O mapa do skirmish do primeiro passo (Maps.json). */
	static FString SkirmishMapName() { return TEXT("World_TestSalt2"); }

	void StartSkirmish();
	void ReturnToMenu();
	void Quit(APlayerController* PC);
};
