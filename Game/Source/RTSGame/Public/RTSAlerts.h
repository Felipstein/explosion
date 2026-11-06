// Os alertas (03/11, aprovados: docs/APROVADOS.md; a prancha art/cursors/alertas/alertas.html), na língua dos cursores: o contorno
// escuro, a mira do atacar, os cantos do prédio; a cor diz de quem é, a forma diz o quê. No minimapa: a unidade e a base sob ataque,
// a superarma inimiga lançada (o círculo do estrago com o relógio), o seu poder pronto (a estrela dourada), a doca que acabou, a
// carga que cai, o primeiro contato, a unidade perdida, o calor da batalha e o radar que cai sem energia. Na borda da tela: a seta
// vermelha (a B) com o brilho da borda (o da C) apontando o ataque fora da vista. A obra e a unidade prontas não têm alerta ("não
// precisa"); o sinal do aliado e o invisível descoberto esperam o sinalizador do multiplayer e a camuflagem existirem.
#pragma once

#include "CoreMinimal.h"

class ARTSPlayerController;
class UCanvas;
class URTSMatchSubsystem;
struct FRTSMinimapPing;

namespace RTSAlerts
{
	enum EKind : uint8 { UnitAttacked = 0, BaseAttacked, StrikeIncoming, PowerReady, DockEmpty, Airlift, FirstContact, UnitLost };

	/** Quanto o alerta dura no minimapa (s). */
	float LifeOf(uint8 Kind);

	/** O minimapa na tela: o centro, o lado do quadrado do mapa (px), os 4 cantos do losango e o mundo → tela. */
	struct FView
	{
		FVector2D Center = FVector2D::ZeroVector;
		float Side = 0.0f;
		float PxPerCm = 0.0f;
		FVector2D Corners[4];
		TFunction<FVector2D(const FVector2D&)> ToScreen;
	};

	/** Os eventos que a simulação não avisa sozinha, vistos a cada passo do controle: o poder pronto, a doca que acabou, a carga a
	 *  caminho, o primeiro contato e a unidade perdida. Saem em Out (o controle junta aos avisos dele). */
	void Watch(URTSMatchSubsystem* Match, TArray<FRTSMinimapPing>& Out);
	/** O tiro em algo seu esquenta o minimapa ali (o calor da batalha, que esfria em 20 s). */
	void FeedHeat(const FVector2D& Cm);

	void DrawHeat(UCanvas* Canvas, const FView& View, float DeltaS);
	void DrawPing(UCanvas* Canvas, const FView& View, const FRTSMinimapPing& Ping);
	/** Sem energia: o chuvisco por cima do mapa; Label escreve o "RADAR OFFLINE" no meio. */
	void DrawRadarOffline(UCanvas* Canvas, const FView& View, double Now, TFunctionRef<void(const FString&, const FVector2D&)> Label);
	/** A seta vermelha com o brilho da borda, pro ataque fora da tela (some quando a câmera chega). */
	void DrawEdgeArrows(UCanvas* Canvas, ARTSPlayerController* PC, const TArray<FRTSMinimapPing>& Pings);
	/** Teste: -RTSAlertTest solta um de cada alerta a cada 8 s em volta da sua base (a cena de teste não tem ataque de verdade);
	 *  -RTSAlertTest=radar derruba o radar. */
	bool TestRadarOff();
}
