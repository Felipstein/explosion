#pragma once

#include "Sim/SimApi.h"

// IA de skirmish v0: um jogador virtual. Lê o estado (como o desenho lê) e dá
// ordens como um humano daria, pela mesma fila de comandos. Não tem atalho:
// não mexe em nada da simulação, só emite comandos (docs/JOGO.md §3.5).
//
// Comportamento v0:
//  - só tanques: espera juntar o grupo inicial e manda todo mundo em
//    ataque-andando até o inimigo mais perto do centro do próprio exército;
//  - com base (o robô de teste da economia, docs/ECONOMIA.md): uma lista fixa de
//    construção (usina → centro de suprimentos perto do depósito → War Factory),
//    2 caminhões, a fábrica sempre com fila, e ataque quando junta uma onda.
//    Reconstrói o que perder. Não é a IA de verdade (essa vem depois).

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
		PlayerIndex Player() const { return Me; }

		/** Pensa no passo atual e devolve as ordens a emitir agora (pode ser nenhuma). */
		void Think(const World& State, std::vector<Command>& OutCommands);

		/** Espera antes do primeiro ataque, pra o jogador se orientar. */
		int32_t FirstWaveDelayTicks = 20 * TicksPerSecond;
		/** De quanto em quanto tempo ela reavalia o alvo do ataque. */
		int32_t RethinkTicks = 3 * TicksPerSecond;
		/** Com base: quantos tanques juntam antes de atacar. */
		int32_t WaveSize = 6;
		/** Com base: caminhões que ela mantém. */
		int32_t TrucksWanted = 2;

	private:
		/** A parte da base: obras, caminhões e fila da fábrica. */
		void ThinkBase(const World& State, std::vector<Command>& OutCommands);
		/** O primeiro lugar que a regra deixa, numa espiral em volta de Anchor (determinístico). */
		bool FindSpot(const World& State, uint16_t Def, int32_t Dir, Vec2 Anchor, Fix MinRadius, Angle Start, Vec2& Out) const;
		/** O lado (0..3) que aponta de From pra To. */
		static int32_t DirToward(Vec2 From, Vec2 To);
		void Attack(const World& State, std::vector<Command>& OutCommands, bool bBase);

		PlayerIndex Me = 0;
		TickNumber NextThink = 0;
		EntityId CurrentTarget = NoEntity;
		/** A onda já saiu (com base): os tanques novos vão reforçar. */
		bool bWaveSent = false;
	};
}
