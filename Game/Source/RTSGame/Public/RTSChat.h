// (17/10, o Felipe: "da tempo de vc implementar sistema de chat na partida?") A caixa de digitar do chat da partida. Como nos
// RTS (o StarCraft II: o Enter abre o chat com todos; o StarCraft Remastered: Shift+Enter todos e Ctrl+Enter só os
// aliados; o Zero Hour: as linhas no canto de cima e somem sozinhas): Enter abre (com todos), Shift ou Ctrl+Enter abre só
// com os aliados, Tab troca enquanto digita, Enter manda, Esc cancela. Enquanto digita, as teclas não chegam no jogo (os
// atalhos ficam quietos). As linhas e a rede ficam no URTSNetSession (fora da simulação: não mexem no lockstep).
#pragma once

#include "CoreMinimal.h"

class ARTSPlayerController;

namespace RTSChat
{
	/** Liga a caixa pra este controle (o pré-processador da Slate é registrado uma vez só). */
	void Attach(ARTSPlayerController* PC);
	bool IsTyping();
	bool IsTeam();
	const FString& Draft();
	/** A hora em que a caixa abriu (o cursor pisca a partir dela). */
	double OpenedAt();
	/** O teste: digita o texto como o teclado (a tecla e o caractere) pela Slate. */
	void SimulateTyping(const FString& Text);
}
