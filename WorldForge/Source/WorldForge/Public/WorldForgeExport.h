#pragma once

#include "Generation/WorldMap.h"

/**
 * Salva um mapa gerado numa pasta:
 *   height16.png   alturas em 16 bits (o formato que o Landscape da Unreal
 *                  importa; a faixa em metros está no map.json)
 *   layers.png     camadas do chão (R terra, G vegetação, B areia, A rocha)
 *   minimap.png    o minimapa do painel
 *   map.json       controles, semente, bases, recursos, contagens, tempos
 */
namespace WorldForgeExport
{
	WORLDFORGE_API bool Save(const FWorldMap& Map, const TArray<FColor>& Minimap, const FString& Folder);
}
