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
	/** (06/11) O mapa da próxima partida: a sala da LAN escolhe (o anfitrião) e o pacote da sala leva pros outros. */
	void SetMapOverride(const FString& Map);
	/** Do BeginPlay do controle do jogador: no menu põe as telas; na partida tira. */
	void OnPlayerBegin(ARTSPlayerController* PC, bool bMenu);
	/** As telas novas estão cobrindo o menu (o HUD não desenha o menu antigo). */
	bool IsCovering();
	/** (06/11) Uma tela da partida nativa está por cima (o HUD não desenha o LOADING, o aviso de rede, o resultado e o Esc velhos). */
	bool IsMatchOverlay();
	/** (06/11 à noite) O botão da tela da partida nativa pelo Id do OnButton ("menu", "lobby"...), em pixels do viewport: o teste
	 *  automático clica nele como clicava no do HUD velho. */
	bool MatchButtonCenter(FName Id, FVector2D& OutViewportPixel);
	/** O skirmish montado na sala nova, pra próxima partida local (nullptr = o 1 contra 1 de antes). */
	const TArray<FRTSLocalSlot>* LocalSlots();
	/** O mapa escolhido na sala nova ou o da cena do SCRIPT (vazio = o de sempre). */
	FString MapOverride();
}
