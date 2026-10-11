#pragma once

// Tipos básicos da simulação. C++ puro: nada aqui conhece a Unreal.

#include <cstdint>

namespace Sim
{
	/** Identificador estável de uma entidade na partida. Nunca é reutilizado; 0 = nenhuma. */
	using EntityId = uint32_t;
	constexpr EntityId NoEntity = 0;

	using PlayerIndex = uint8_t;
	using TickNumber = uint32_t;

	/** Passos da simulação por segundo (o mesmo do Zero Hour, do Spring e do Vanguard). */
	constexpr int32_t TicksPerSecond = 30;
	constexpr int32_t MaxPlayers = 8;
	/** Teto de unidades numa seleção/grupo (o comando fica pequeno e o pacote, limitado). */
	constexpr int32_t MaxGroupSize = 500;

	/** Versão das regras da simulação: replay de outra versão não reproduz. */
	constexpr uint32_t SimVersion = 5;   // 5 (11/10): o Dozer sai do lote pelo lado livre
}
