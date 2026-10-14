#include "HudLabGameMode.h"

#include "HudLabHUD.h"

AHudLabGameMode::AHudLabGameMode()
{
	HUDClass = AHudLabHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
