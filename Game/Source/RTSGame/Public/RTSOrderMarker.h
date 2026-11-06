// A marca que a ordem deixa no chão no clique (03/11; o pedido dele: "o pointer efeito que mostra onde vai andar, não tem hoje
// né? o ZH tinha"). O ZH: InGameUI::createMoveHint + W3DInGameUI::drawMoveHints, um modelo animado deitado no relevo, ~1,3 s, um
// por ordem. Aqui a direção B da prancha art/cursors/marca_ordem.html (a escolhida: "gostei mais do B"): o anel cortado da mira
// encolhe até o ponto com as quatro setinhas nos cortes e o ponto fica até sumir; 1 s ao todo. As cores dos cursores: verde as suas
// tropas (andar, escoltar, entrar, patrulhar, o ponto de encontro), vermelho o hostil (atacar, o ataque andando, o tiro no chão
// com o X), âmbar o trabalho (construir, consertar, retomar a obra, a sucata, a doca). Com alvo, o anel fecha em volta dele.
#pragma once

#include "CoreMinimal.h"

class ARTSPlayerController;
class UCanvas;
class URTSMatchSubsystem;

namespace RTSOrderMarker
{
	/** Quanto a marca dura (o controle segura a marca esse tempo). */
	constexpr float Seconds = 1.0f;
	/** A marca no ponto World (no chão), com Age s de vida. Kind: 0 verde, 1 vermelho, 2 âmbar; RadiusCm > 0: o anel fecha em volta
	 *  do alvo desse raio; bCross: o X do tiro no chão no lugar do ponto. */
	void Draw(UCanvas* Canvas, ARTSPlayerController* PC, const FVector& World, float Age, uint8 Kind, float RadiusCm, bool bCross);
	/** O raio do alvo no chão: a planta do prédio ou as malhas visíveis da unidade (sem o modelo, 3 m). */
	float RadiusOf(const URTSMatchSubsystem* Match, uint32 Id);
	/** Teste: -RTSMarkerAt=X,Y (m) repete as marcas a cada 1,6 s: andar ali, o ataque andando 10 m ao lado e o tiro no chão a 20 m;
	 *  e as de alvo nas três unidades mais perto (atacar, consertar, entrar). A cena manda as ordens direto pra simulação, sem o
	 *  clique que deixa a marca. */
	void DrawTest(UCanvas* Canvas, ARTSPlayerController* PC);
}
