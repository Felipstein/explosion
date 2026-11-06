// A resposta ao selecionar (04/11; ele: "na hora do clique que seleciona, o hover da cor, igual o C&C ZH, porém aqui eu quero bem
// feito"). O ZH pisca a unidade na cor do time (Drawable::onSelected → flashAsSelected: o tinte sobe na hora e cai em 4 quadros).
// Aqui as peças da unidade marcam o bit 64 do stencil durante o clarão e o pós-processo M_SelectFlash (Tools/make_select_flash.py)
// soma a cor do jogador por cima delas, mais forte no contorno; apaga em 0,28 s. Funciona com Nanite (o material por cima das peças,
// a 1ª versão, não aparece em malha Nanite) e não acende o chão.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace RTSSelectFlash
{
	/** O clarão no modelo da unidade (Unit), na cor do jogador. Selecionar de novo durante o clarão recomeça o mesmo. */
	void Spawn(AActor* Unit, const FLinearColor& Team);
	/** Apaga os clarões com o tempo (o HUD chama a cada quadro). */
	void Tick(UWorld* World);
	/** Quanto dura (s). */
	constexpr float Seconds = 0.28f;
	/** O bit do stencil (a silhueta usa 1–63 e a poeira o 128). */
	constexpr int32 StencilBit = 64;
}
