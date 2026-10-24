// (17/10) A gravação de vídeo dos testes sem travar a placa (o Felipe: "6 minutos de intervalo entre os testes e cada
// teste levar 6 minutos"). Medido nos logs de hoje: sem gravar o teste roda a 0,5–0,7 s de relógio por s de jogo; gravando,
// a 7–12 s, porque o FScreenshotRequest de cada quadro espera a placa terminar, lê a tela de volta e comprime o PNG.
// Aqui, como o FFrameGrabber da Unreal (MovieSceneCapture), a tela pronta pra apresentar é copiada pra uma área de leitura
// da placa (4 em voo) e lida quadros depois, sem esperar; o JPG é comprimido nos núcleos livres.
#pragma once

#include "CoreMinimal.h"

namespace RTSFrameRecorder
{
	/** rts.Record.Fast (registrada quando o jogo carrega, pra o -ExecCmds alcançar): 1 = esta gravação, 0 = o print de antes. */
	bool WantFast();
	/** Liga (o aviso da Slate de tela pronta na janela do jogo). false sem Slate ou sem janela: aí fica o print de antes. */
	bool Start();
	bool IsActive();
	/** Pede o quadro que vai ser apresentado agora (o caminho do .jpg). Só no game thread. */
	void Capture(const FString& JpgPath);
	/** Os quadros pedidos que ainda não foram salvos. */
	int32 Outstanding();
	/** Desliga e espera os que faltam (bloqueia, até MaxWaitS). */
	void Stop(float MaxWaitS = 30.0f);
}
