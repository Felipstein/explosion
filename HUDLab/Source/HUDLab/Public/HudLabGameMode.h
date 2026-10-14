#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HudLabGameMode.generated.h"

/** Só põe o HUD do laboratório; sem peão, sem mundo (o fundo é a cena aprovada, uma imagem). */
UCLASS()
class AHudLabGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHudLabGameMode();
};
