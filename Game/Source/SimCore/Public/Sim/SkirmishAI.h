#pragma once

#include "Sim/SimApi.h"

// IA de skirmish v0: um jogador virtual. Lê o estado (como o desenho lê) e dá
// ordens como um humano daria, pela mesma fila de comandos. Não tem atalho:
// não mexe em nada da simulação, só emite comandos (docs/JOGO.md §3.5).
//
// Comportamento v0 (só tanques, sem base ainda):
//  - espera juntar o grupo inicial;
//  - de tempos em tempos, manda todo mundo em ataque-andando até o inimigo mais
//    perto do centro do próprio exército;
//  - o resto (mirar, reagir a tiro) é das próprias unidades.

#include <cstdint>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/World.h"

namespace Sim
{
	class SIMCORE_API SkirmishAI
	{
	public:
		explicit SkirmishAI(PlayerIndex InMe) : Me(InMe) {}

		/** Pensa no passo atual e devolve as ordens a emitir agora (pode ser nenhuma). */
		void Think(const World& State, std::vector<Command>& OutCommands);

		/** Espera antes do primeiro ataque, pra o jogador se orientar. */
		int32_t FirstWaveDelayTicks = 20 * TicksPerSecond;
		/** De quanto em quanto tempo ela reavalia o alvo do ataque. */
		int32_t RethinkTicks = 3 * TicksPerSecond;

	private:
		PlayerIndex Me = 0;
		TickNumber NextThink = 0;
		EntityId CurrentTarget = NoEntity;
	};
}
