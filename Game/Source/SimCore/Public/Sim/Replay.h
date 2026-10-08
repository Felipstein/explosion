#pragma once

#include "Sim/SimApi.h"

// Replay = a montagem da partida + os comandos de cada passo + a impressão
// digital de tempos em tempos. Rodar de novo com os mesmos comandos tem de dar
// as mesmas impressões; se não der, achamos uma dessincronia (e o passo onde ela
// começou). Também é o teste automático do determinismo.

#include <cstdint>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/World.h"

namespace Sim
{
	/** Impressão digital gravada a cada este tanto de passos (0,5 s). */
	constexpr TickNumber ReplayHashInterval = 15;

	class SIMCORE_API ReplayWriter
	{
	public:
		void Begin(const MatchSetup& Setup);
		/** Chamado depois de cada passo, com os comandos que ele executou e o hash resultante. */
		void AddTick(TickNumber Tick, const std::vector<Command>& Commands, uint64_t HashAfter);
		std::vector<uint8_t> Finish() const;

	private:
		ByteWriter Header;
		ByteWriter Body;
		TickNumber LastTick = 0;
		uint64_t LastHash = 0;
	};

	class SIMCORE_API ReplayReader
	{
	public:
		/** Falso se o arquivo não é um replay desta versão ou está corrompido. */
		bool Load(const std::vector<uint8_t>& Bytes);

		const MatchSetup& Setup() const { return SetupData; }
		/** Comandos do passo Tick (vazio se não houve). */
		std::vector<Command> CommandsFor(TickNumber Tick) const;
		/** Hash esperado depois do passo Tick, se foi gravado. */
		bool ExpectedHash(TickNumber Tick, uint64_t& Out) const;
		TickNumber LastTick() const { return FinalTick; }

	private:
		struct TickRecord
		{
			TickNumber Tick = 0;
			std::vector<Command> Commands;
		};
		struct HashRecord
		{
			TickNumber Tick = 0;
			uint64_t Hash = 0;
		};

		MatchSetup SetupData;
		std::vector<TickRecord> Ticks;      // em ordem de passo
		std::vector<HashRecord> Hashes;     // em ordem de passo
		TickNumber FinalTick = 0;
	};

	/** Montagem ↔ bytes (também serve pra mandar a partida pela rede). */
	SIMCORE_API void WriteSetup(ByteWriter& W, const MatchSetup& Setup);
	SIMCORE_API bool ReadSetup(ByteReader& R, MatchSetup& Out);
}
