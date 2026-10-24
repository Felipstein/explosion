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
		StructurePlaced,  // obra nova Target em Point (Source = o Dozer)
		StructureCompleted, // a obra Target ficou pronta
		StructureCancelled, // a obra Target foi cancelada (some sem explodir)
		UnitProduced,     // o prédio Source soltou a unidade Target
		SupplyDelivered,  // o caminhão Source entregou Value $ no centro Target
		CargoDropped,     // o caminhão destruído soltou a carga em Point: a pilha é o depósito Value
		// (16/10) o tanque esmagou o carro Target (o índice do objeto, World::PropAt): Point = o ponto esmagado; Value = as
		// partes novas (1 frente, 2 traseira) | (1 << 2 se o ponto ficou do lado direito do tanque) | (o casco do tanque << 16)
		PropCrushed,
		// (16/10) o tanque derrubou a árvore Target: Point = o tronco; Value = o rumo da queda (bam) | (a velocidade do
		// tanque em cm/s << 16); o comprimento livre está no PropAt
		PropToppled,
		// (16/10) a defesa Source pediu ajuda e a Target (do mesmo tipo) passou a mirar no alvo dela (Value = o Id do alvo,
		// Point = a posição dele): o laser do desenho (o LaserFromAssisted e o LaserToTarget da Patriot do ZH)
		DefenseAssist,
		// (22/10) a carcaça Target começa a afundar em Point (fica imóvel, e o casco de colisão encolhe até sumir): Value = os
		// passos do afundar | (o motivo << 16): 0 a vida acabou, 1 sólidas demais no mapa (a mais velha afunda antes)
		WreckSinking,
		// (22/10) a carcaça Target saiu do mapa em Point: Value = o motivo, 0 afundou inteira, 1 o teto de todas (a mais velha
		// sai na hora, a morte em massa)
		WreckRemoved,
		// (v4, 22/10) a patente do general: o jogador subiu (uma vez em cada limiar); Value = o jogador | (a patente nova << 8)
		RankUp,
		// (v4) o poder do general foi usado em Point: Value = o jogador | (o poder << 8) | (os mísseis << 16)
		PowerUsed,
		// (v4) o avião de carga da zona Source vem aí (o aviso, Lead antes do pouso): Point = a zona; Value = os paletes
		AirliftInbound,
		// (v4) os paletes pousaram no pátio da zona Source: Point = a zona; Value = as caixas que chegaram
		AirliftLanded,
		// (v4, 23/10) o prédio Target começou a ser vendido em Point (a desmontagem do ZH): Value = os passos dela
		StructureSelling,
		// (v4, 23/10) a venda do prédio Target acabou: ele sai do mapa sem explodir e o dono recebe o dinheiro; Point = o prédio,
		// Value = o dinheiro devolvido | (o dono << 24)
		StructureSold,
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
