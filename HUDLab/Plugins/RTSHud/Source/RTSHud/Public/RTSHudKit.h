#pragma once

// As imagens do HUD direto dos PNG do kit (docs/concept/hud/kit/), com mipmaps feitos na hora: serve o
// laboratório e o primeiro teste no jogo, sem importar asset nenhum. No jogo final elas viram assets em
// /Game/HUD/ (INTEGRACAO.md) e o lookup do painter aponta pra eles.
//
// Os nomes que o FRTSHudPainter pede:
//   skin:<peça>  → <Kit>/skin/<peça>.png       (as molduras)
//   icon:<nome>  → <Kit>/icons/<nome>.png
//   btn:<nome>   → <Kit>/photos/buttons/<nome>.png
//   card:<nome>  → <Kit>/photos/card/<nome>.png
//   emblem:<f>   → <Extra>/emblem_<f>.png      (o emblema da facção)
//   minimap      → <Extra>/minimap.png         (no jogo: a textura do minimapa de verdade)
//   bg           → <Extra>/background.png      (só o laboratório)

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UTexture2D;

class RTSHUD_API FRTSHudKit
{
public:
	/** KitDir: docs/concept/hud/kit; ExtraDir: a pasta dos emblemas e do minimapa (HUDLab/LabAssets). */
	FRTSHudKit(const FString& InKitDir, const FString& InExtraDir);

	/** A textura do nome (carrega na primeira vez; nome que falta dá nullptr e um aviso no log, uma vez). */
	UTexture2D* Get(FName Name);
	FString Resolve(FName Name) const;

	/** O PNG com a cadeia de mipmaps (a média de cada 2×2 pesa a cor pelo alfa). */
	static UTexture2D* LoadPngWithMips(const FString& Path);

private:
	FString KitDir, ExtraDir;
	TMap<FName, TStrongObjectPtr<UTexture2D>> Cache;
	TSet<FName> Missing;
};
