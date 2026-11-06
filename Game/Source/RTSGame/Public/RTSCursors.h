#pragma once

// Os cursores do jogo (01/11, docs/CURSORES.md): os desenhos em art/cursors/cursors.html, os arquivos gerados por
// art/tools/make_cursors.py em Content/Cursors/<48|64|96>/ (.ani animado, .cur parado). O Windows desenha e anima: o cursor
// não espera o quadro do jogo (a 30 qps, um cursor da Unreal atrasaria 33 ms) e não custa nada na placa.

#include "CoreMinimal.h"
#include "GenericPlatform/ICursor.h"

/** Cada cursor do jogo; a ordem é a dos nomes dos arquivos em RTSCursors.cpp. */
enum class ERTSCursor : uint8
{
	Pointer, Select,
	ScrollN, ScrollNE, ScrollE, ScrollSE, ScrollS, ScrollSW, ScrollW, ScrollNW,
	Pan, Invalid, Move, Queue, Attack, Cant, Force, Ground, AttackMove,
	Guard, Escort, Patrol, Enter, Rally, Build, NoBuild,
	Repair, Resume, Salvage, Gather, Sell, Strike, Support,
	Rotate,   // (04/11) girar a obra: a volta gira com o arraste (rotate_00..23, de 15 em 15°)
	Count
};

class FRTSCursors
{
public:
	/** Põe o cursor C. Carrega os arquivos na 1ª vez e de novo quando a altura da tela muda de faixa (48 px até 1200
	 *  linhas, 64 até 1800, 96 acima: os 4,2% da altura do ZH). Devolve o tipo que vai pra Slate: a seta é o Default (trocado
	 *  pela nossa, vale também no HUD e nos menus), o resto vai pelo Custom. Sem os arquivos, os do Windows de antes. */
	EMouseCursor::Type Apply(ERTSCursor C, int32 ScreenHeight, int32 QueueN = 0, float RotateDeg = 0.0f);
	bool IsReady() const { return LoadedPx > 0; }
	ERTSCursor GetShown() const { return Shown; }
	static const TCHAR* Name(ERTSCursor C);
	/** (01/11) A ordem entra na fila do Alt (tem a ficha do número da vez). QueueN no Apply: a vez (1 a 9; 10 ou mais = "+"). */
	static bool IsQueueable(ERTSCursor C);
	/** (03/11) Esquece o que está na tela: o próximo Apply repõe as formas. O navegador das telas do menu (o plugin RTSShell)
	 *  põe o cursor dele no Custom da aplicação inteira; sem isso, o 1º cursor igual ao último da partida ficava o do menu. */
	void Invalidate();
	static constexpr int32 QueueOrderCount = 14, QueueNumbers = 10;

private:
	bool Load(int32 Px);
	void* Handles[int32(ERTSCursor::Count)] = {};
	void* QueueHandles[QueueOrderCount][QueueNumbers] = {};
	int32 ShownQueue = 0;
	static constexpr int32 RotateSteps = 24;
	void* RotateHandles[RotateSteps] = {};
	int32 ShownRotate = -1;
	int32 LoadedPx = 0;
	int32 FailedPx = 0;
	ERTSCursor Shown = ERTSCursor::Count;
};
