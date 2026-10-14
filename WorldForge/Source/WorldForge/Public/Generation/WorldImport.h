#pragma once

#include "CoreMinimal.h"
#include "Generation/WorldMap.h"

struct FWorldEdits;

/**
 * Mapas trazidos de fora (hoje: do C&C Generals Zero Hour, Tools/zh_import.py).
 * Cada um é uma pasta em Saved/WorldForge/imported/<nome>/ com o relevo, as
 * camadas do chão, as bases, os campos, as peças (map.json, height.bin,
 * layers.bin) e as edições à mão que vêm dele (edits.json: estradas, concreto).
 * O mapa entra no lugar do gerador; o acabamento (chão, espalhamento, estradas)
 * é o nosso.
 */
namespace WorldImport
{
	struct FEntry
	{
		FString Title;     // "Desert Fury"
		FString Folder;    // caminho completo da pasta
	};

	/** Os mapas importados que existem (a pasta tem map.json). */
	WORLDFORGE_API TArray<FEntry> List();

	/** Lê a pasta: o mapa pronto pro FWorldDetail e as edições dele. OutNote: o
	 *  que ficou de fora (prédios, água...), pro painel. */
	WORLDFORGE_API TSharedPtr<FWorldMap> Load(const FString& Folder, TSharedPtr<FWorldEdits>& OutEdits, FString& OutNote);
}
