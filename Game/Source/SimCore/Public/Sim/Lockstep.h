#pragma once

// A fila de comandos do lockstep. Um comando dado no passo T executa no passo
// T + atraso, em todos os PCs, na mesma ordem (jogador, sequência).
//
// No skirmish todos os jogadores (você e a IA) estão no mesmo PC: esta fila é o
// "transporte" local. No online, o transporte de rede preenche a mesma fila com
// os comandos que chegam dos outros, e o passo só anda quando todos chegaram.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/Types.h"

namespace Sim
{
	class CommandScheduler
	{
	public:
		explicit CommandScheduler(int32_t InDelayTicks = 1) : DelayTicks(InDelayTicks < 1 ? 1 : InDelayTicks) {}

		int32_t Delay() const { return DelayTicks; }

		/** Agenda um comando dado agora (no passo Now) pra executar em Now + atraso. Devolve o passo marcado. */
		TickNumber Issue(Command C, TickNumber Now)
		{
			C.Tick = Now + TickNumber(DelayTicks);
			Pending.push_back({ C, NextSequence++ });
			return C.Tick;
		}

		/** Agenda com o passo já marcado (replay, rede). */
		void Insert(const Command& C)
		{
			Pending.push_back({ C, NextSequence++ });
		}

		/** Tira os comandos do passo Tick, na ordem (jogador, sequência). */
		std::vector<Command> TakeFor(TickNumber Tick)
		{
			std::vector<Entry> Due;
			std::vector<Entry> Later;
			for (Entry& E : Pending)
			{
				(E.Cmd.Tick <= Tick ? Due : Later).push_back(std::move(E));
			}
			Pending.swap(Later);
			std::sort(Due.begin(), Due.end(), [](const Entry& A, const Entry& B)
			{
				if (A.Cmd.Player != B.Cmd.Player) { return A.Cmd.Player < B.Cmd.Player; }
				return A.Sequence < B.Sequence;
			});
			std::vector<Command> Out;
			Out.reserve(Due.size());
			for (Entry& E : Due)
			{
				E.Cmd.Tick = Tick;   // atrasado (não deveria acontecer) executa agora, marcado com o passo real
				Out.push_back(std::move(E.Cmd));
			}
			return Out;
		}

	private:
		struct Entry
		{
			Command Cmd;
			uint64_t Sequence = 0;
		};

		int32_t DelayTicks = 1;
		uint64_t NextSequence = 0;
		std::vector<Entry> Pending;
	};
}
