#pragma once

// Os estados de teste, os mesmos da planta (docs/concept/hud/wireframe/hud_wireframe.html):
// 1 tanques selecionados, 2 War Factory cheia, 3 Dozer cheio, 4 prédio com melhorias, 5 nada selecionado
// (8 jogadores), 6 o 1 com várias superarmas dos dois lados.
// MainColumns: as colunas da grade principal (7 ou 9); as teclas seguem as fileiras (QWERTYUIO / ZXCVBNM,.).

#include "CoreMinimal.h"
#include "RTSHudTypes.h"

FRTSHudState MakeHudLabState(int32 Index, int32 MainColumns = 7);

/**
 * Troca os jogadores do estado pelos de um formato do protótipo aprovado (duel, 2v2, 3v5, 8v8, ffa6, ffa16, 4x4, 3v5v8), com os mesmos
 * números de lá; bAbuse dá 240 superarmas a um inimigo; MeHex troca a sua cor. Falso se o formato não existe.
 */
bool ApplyHudLabFormat(FRTSHudState& St, const FString& Format, const FString& MyFaction, bool bAbuse, const FString& MeHex);

/** (05/11) A dica de cada casa dos estados (o preço com o tempo e a linha de baixo; bRed = o motivo de não poder), pra janela mostrar a
 *  certa debaixo do mouse. Falso se a casa não tem dica própria. */
bool HudLabSlotTip(FName Id, FString& Cost, FString& Line, bool& bRed);
