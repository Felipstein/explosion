#pragma once

// Os estados de teste, os mesmos da planta (docs/concept/hud/wireframe/hud_wireframe.html):
// 1 tanques selecionados, 2 War Factory cheia, 3 Dozer cheio, 4 prédio com melhorias, 5 nada selecionado
// (8 jogadores), 6 o 1 com várias superarmas dos dois lados.
// MainColumns: as colunas da grade principal (7 ou 9); as teclas seguem as fileiras (QWERTYUIO / ZXCVBNM,.).

#include "CoreMinimal.h"
#include "RTSHudTypes.h"

FRTSHudState MakeHudLabState(int32 Index, int32 MainColumns = 7);
