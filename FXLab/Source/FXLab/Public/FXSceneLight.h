#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * A luz do mapa do jogo. No deserto do WorldForge (World_Desert, fxlab.ps1 worlds) o sol vem
 * marcado "GameSun" e a cena é física: sol de ~120 mil lux, céu da SkyLight e exposição travada.
 * Os cartões então leem o sol e o céu do próprio mapa (no material: SceneLight = 1) e o fogo,
 * que tem luz própria, é multiplicado por 1/exposição: na tela ele sai com o mesmo valor do
 * laboratório aprovado (exposição 1), seja qual for a luz do dia.
 * Sem o "GameSun" é o laboratório (Lookdev_Fire): o sol e o céu vêm do preset.json.
 */
struct FFXSceneLight
{
	bool bGame = false;
	FVector SunDir = FVector(0.0, 0.0, 1.0);    // pro sol, eixos da Unreal
	double EmitScale = 1.0;                     // 1 / exposição do mapa
	double Ev100 = 0.0, Bias = 0.0;

	static FFXSceneLight Find(UWorld* World);
};
