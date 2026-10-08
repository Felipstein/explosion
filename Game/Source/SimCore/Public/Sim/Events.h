#pragma once

// O que aconteceu num passo, pra o desenho reagir (tiro, impacto, morte, fim).
// Os eventos saem da simulação; nada volta por aqui.

#include <cstdint>

#include "Sim/FixedMath.h"
#include "Sim/Types.h"

namespace Sim
{
	enum class EventType : uint8_t
	{
		UnitSpawned,
		WeaponFired,      // Source atirou em Target; Point = posição do alvo no tiro
		ProjectileHit,    // tiro de Source chegou em Point (Target pode já estar morto)
		UnitDamaged,      // Target perdeu Value pontos de vida
		UnitDied,         // Target morreu em Point; Source = quem matou (pode ser NoEntity)
		PlayerDefeated,   // Value = jogador
		MatchOver,        // Value = time vencedor (−1 = empate)
	};

	struct SimEvent
	{
		EventType Type = EventType::UnitSpawned;
		EntityId Source = NoEntity;
		EntityId Target = NoEntity;
		Vec2 Point;
		int32_t Value = 0;
	};
}
