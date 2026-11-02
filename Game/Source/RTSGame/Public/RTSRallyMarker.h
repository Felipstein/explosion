// O ponto de encontro das fábricas (02/11, aprovado: docs/APROVADOS.md, o mestre art/cursors/rally/rally_gameplay.html).
// Com a fábrica selecionada, a reta rente ao chão sai da borda do prédio até o ponto: o contorno quase preto e o miolo na cor do
// jogador clareada, as setas no sentido da ida; no ponto, a bandeira 3D (o mastro, a placa redonda no chão e o pano que balança no
// vento do mapa, o mesmo SM_FlagCloth das bandeiras dos prédios), acesa pelo sol e com a sombra no chão. Com o ponto fora da tela, a
// ficha com a bandeirinha na borda aponta pra ele.
#pragma once

#include "CoreMinimal.h"

class ARTSPlayerController;
class UCanvas;
class URTSMatchSubsystem;

namespace RTSRallyMarker
{
	/** Desenha a reta e a ficha da borda no Canvas do HUD e põe as bandeiras 3D (some com a do prédio que saiu da seleção).
	 *  Teste: -RTSRallyAt=X,Y (m) marca o ponto de encontro dos prédios selecionados uma vez, como o clique no chão. */
	void Draw(UCanvas* Canvas, ARTSPlayerController* PC, URTSMatchSubsystem* Match);
}
