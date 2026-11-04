#pragma once

// O som das telas fora da partida (04/11, as telas nativas): a música do menu (as três trilhas do assgets em sequência), o clique,
// o abaixar da música nas telas de conversa (o mix SM_ShellDuck) e o que o radar do menu escuta da música (o analisador do
// navegador do mockup: o espectro pra cascata do ESM e o nível pro A-scope). Os assets saem de Tools/import_menu_audio.py.

#include "CoreMinimal.h"

namespace RTSShellAudio
{
	/** Menu: a música no volume dela; Duck: abaixada (skirmish, multiplayer, SCRIPT); Off: some em 1 s (a partida, as telas dela). */
	enum class EMusic : uint8 { Menu, Duck, Off };

	void SetMusic(EMusic Mode);
	/** O clique do menu (todo clique em algo que se clica). */
	void Click();
	/** Os outros sons das telas (04/11): o hover (o toque da tecla, com o tom variando um pouco e no máximo um a cada 45 ms), a entrada
	 *  no skirmish e no multiplayer (o golpe grave enquanto a música abaixa) e os três da abertura (o relé, a subida e a revelação). */
	enum class EUi : uint8 { Hover, Enter, BootRelay, BootRiser, BootReveal, Count };
	void Play(EUi Sound);
	/** O espectro como o getByteFrequencyData do mockup (fftSize 512: 256 valores de 0 a 255) e o pico da última janela (o
	 *  getByteTimeDomainData de 8 em 8). Falso quando a música não toca: o radar fica só com os emissores do teatro. */
	bool Analyse(TArray<uint8>& OutBytes, float& OutLevel);
	void Shutdown();
}
