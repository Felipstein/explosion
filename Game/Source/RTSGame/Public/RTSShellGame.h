#pragma once

// As telas novas no jogo (01/11, plugin RTSShell; docs/concept/ui/TELAS.md): este lado liga os botões das páginas às ações do
// jogo (o skirmish montado na sala nova, o laboratório, as cenas de teste do SCRIPT, a qualidade, a LAN) e decide quando as
// telas cobrem o jogo: no menu, fora do teste automático (-RTSAutoTest, que clica no menu antigo), sem -RTSOldMenu e com o
// navegador embutido disponível. A sala da LAN e as telas da partida (Esc, resultado) ainda são as antigas.

#include "CoreMinimal.h"

class ARTSPlayerController;

/** Uma vaga do skirmish montado na sala nova (a ordem é a dos jogadores na partida; a 0 é você). */
struct FRTSLocalSlot
{
	bool bAI = false;
	uint8 Team = 0;            // 0 = sem time; 1..8 = o time da sala
	int32 Start = -1;          // a base do mapa escolhida (-1 = sorteada entre as livres)
	FString Faction = TEXT("random");
	bool bHasColor = false;
	FLinearColor Color = FLinearColor::White;
	uint8 Difficulty = 1;      // guardada; a IA de hoje tem um nível só
};

namespace RTSShellGame
{
	/** Do BeginPlay do controle do jogador: no menu põe as telas; na partida tira. */
	void OnPlayerBegin(ARTSPlayerController* PC, bool bMenu);
	/** As telas novas estão cobrindo o menu (o HUD não desenha o menu antigo). */
	bool IsCovering();
	/** O skirmish montado na sala nova, pra próxima partida local (nullptr = o 1 contra 1 de antes). */
	const TArray<FRTSLocalSlot>* LocalSlots();
	/** O mapa escolhido na sala nova ou o da cena do SCRIPT (vazio = o de sempre). */
	FString MapOverride();
}
