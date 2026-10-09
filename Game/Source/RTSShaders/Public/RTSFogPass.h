#pragma once

// A passada da névoa de guerra na renderização. Escurece só o que é opaco (chão, pedras,
// árvores, unidades) logo depois da luz e antes dos volumes e da translucência (explosões,
// traçantes, clarões): é o que o W3DShroud do Zero Hour faz (multiplica terreno e objetos, nunca
// as partículas). Um pós-processo comum pegaria a névoa do chão atrás da explosão e cortaria a
// bola ao meio na borda da visão (revisão de 08/10).
//
// A textura de visão tem 1 texel por célula do mapa, em BGRA8 sRGB: o canal R guarda o nível da
// névoa na escala da tela do ZH (0 nunca visto, 0,5 já visto, 1 à vista); a decodificação sRGB do
// hardware entrega direto o multiplicador linear (~nível^2,2), que é o que a tela vê como o ×0,5
// do ZH depois da curva de tom.

#include "CoreMinimal.h"

class UTexture2D;

namespace RTSFogPass
{
	/** Liga a passada com esta textura de visão e o retângulo do mapa (cm). Chamar no game thread. */
	RTSSHADERS_API void Enable(UTexture2D* VisionTexture, const FVector2D& MapMinCm, const FVector2D& MapSizeCm);
	/** 1 = névoa inteira; 0 = nada (revelar tudo). Pode mudar a cada quadro (transição). */
	RTSSHADERS_API void SetStrength(float Strength);
	/** Desliga (antes de destruir a textura). */
	RTSSHADERS_API void Disable();
}
