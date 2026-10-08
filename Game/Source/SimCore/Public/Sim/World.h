#pragma once

#include "Sim/SimApi.h"

// A simulação da partida (o "juiz"). Recebe comandos, anda um passo de 1/30 s
// e conta o que aconteceu. Todo PC roda a mesma, com os mesmos comandos, e
// chega no mesmo estado bit por bit (StateHash).
//
// Regras do código aqui dentro (docs/JOGO.md §3.2):
//  - só inteiros e ponto fixo; nada de float, relógio, threads ou ponteiro como chave;
//  - toda varredura em ordem de ID; empate sempre desempatado pelo ID;
//  - quem vem de fora (desenho, IA) só lê; a única entrada é Step(comandos).

#include <cstdint>
#include <string>
#include <vector>

#include "Sim/Commands.h"
#include "Sim/Defs.h"
#include "Sim/Events.h"
#include "Sim/FixedMath.h"
#include "Sim/Pathfinder.h"
#include "Sim/Rng.h"
#include "Sim/SimMap.h"
#include "Sim/Types.h"

namespace Sim
{
	struct StartingUnit
	{
		std::string Key;
		Vec2 Pos;
		Angle Facing;
	};

	struct PlayerSetup
	{
		uint8_t Team = 0;
		bool bIsAI = false;
		std::vector<StartingUnit> Units;
	};

	struct SIMCORE_API MatchSetup
	{
		uint64_t Seed = 1;
		SimMap Map;
		Ruleset Rules;
		std::vector<PlayerSetup> Players;

		uint64_t Hash() const;
	};

	enum class OrderType : uint8_t
	{
		Idle,
		Move,
		Attack,
		AttackMove,
	};

	struct PlayerState
	{
		uint8_t Team = 0;
		bool bDefeated = false;
		/** O grupo de comando atual (SetGroup), em ordem de ID. */
		std::vector<EntityId> Group;
	};

	/** Leitura de uma unidade, pro desenho e pra IA. */
	struct UnitView
	{
		EntityId Id = NoEntity;
		PlayerIndex Owner = 0;
		uint16_t Def = 0;
		Vec2 Pos;
		Angle Hull;
		/** Direção da torre no mundo. */
		Angle Turret;
		Fix Speed;
		int32_t Hp = 0;
		int32_t MaxHp = 0;
		OrderType Order = OrderType::Idle;
		Vec2 OrderPoint;
		/** Em quem a torre está mirando agora (NoEntity = ninguém). */
		EntityId FireTarget = NoEntity;
		/** Atrás de quem a unidade está indo (ordem de ataque, ataque andando ou reação). */
		EntityId Engage = NoEntity;
		/** Pontos que faltam no caminho atual (0 = parada ou sem caminho). */
		int32_t PathLeft = 0;
		int32_t Cooldown = 0;
	};

	class SIMCORE_API World
	{
	public:
		/** Falso se a montagem é inválida (unidade desconhecida, jogador demais, mapa vazio). */
		bool Init(const MatchSetup& Setup);
		/** Anda um passo. Commands = os comandos marcados para este passo, já em ordem (jogador, sequência). */
		void Step(const std::vector<Command>& Commands);

		TickNumber Tick() const { return CurrentTick; }
		uint64_t StateHash() const;
		const std::vector<SimEvent>& LastEvents() const { return Events; }

		int32_t UnitCount() const { return int32_t(U.Id.size()); }
		UnitView Unit(int32_t Index) const;
		bool FindUnit(EntityId Id, UnitView& Out) const;

		int32_t PlayerCount() const { return int32_t(Players.size()); }
		const PlayerState& Player(PlayerIndex P) const { return Players[P]; }
		bool AreEnemies(PlayerIndex A, PlayerIndex B) const { return Players[A].Team != Players[B].Team; }

		bool IsMatchOver() const { return bMatchOver; }
		/** Time vencedor; −1 = empate ou partida em andamento. */
		int32_t WinningTeam() const { return Winner; }
		/** Passo em que a partida acabou (o relógio do resultado para aqui). */
		TickNumber EndTick() const { return MatchEndTick; }

		const SimMap& Map() const { return MapData; }
		const Ruleset& Rules() const { return RulesData; }

	private:
		/** Estado das unidades em arrays paralelos (SoA), sempre em ordem crescente de ID. */
		struct UnitArrays
		{
			std::vector<EntityId> Id;
			std::vector<PlayerIndex> Owner;
			std::vector<uint16_t> Def;
			std::vector<Vec2> Pos;
			std::vector<Angle> Hull;
			std::vector<int32_t> TurretRel;      // torre relativa ao casco (bam, com sinal)
			std::vector<Fix> Speed;
			std::vector<int32_t> Hp;
			std::vector<int32_t> Cooldown;
			std::vector<OrderType> Order;
			std::vector<Vec2> OrderPoint;
			std::vector<EntityId> OrderTarget;   // alvo da ordem de ataque
			std::vector<EntityId> Engage;        // em quem a unidade está indo atrás (ordem ou reação)
			std::vector<EntityId> FireTarget;    // em quem a torre mira
			std::vector<EntityId> LastAttacker;
			std::vector<int32_t> TurretIdle;     // passos sem alvo (a torre volta pra frente)
			std::vector<std::vector<Vec2>> Path;
			std::vector<uint16_t> PathIndex;
			std::vector<Vec2> PathGoal;          // pra onde o caminho atual leva
			std::vector<uint8_t> NeedsPath;
			std::vector<int32_t> RepathWait;     // passos até poder refazer o caminho
			std::vector<int32_t> StuckTicks;
			std::vector<Vec2> StuckAnchor;
			std::vector<Vec2> EngageAnchor;      // onde a perseguição começou (a coleira mede daqui)
			std::vector<EntityId> Ignore;        // alvo inalcançável, ignorado até IgnoreUntil
			std::vector<TickNumber> IgnoreUntil;

			void Resize(size_t N);
		};

		struct Projectile
		{
			EntityId Shooter = NoEntity;
			EntityId Target = NoEntity;
			Vec2 Pos;
			Vec2 Aim;              // última posição conhecida do alvo
			Fix Step;              // m por passo; 0 = chega no passo seguinte
			int32_t Damage = 0;
			DamageType Type = DamageType::ArmorPiercing;
		};

		struct DamageIntent
		{
			EntityId Source = NoEntity;
			EntityId Target = NoEntity;
			int32_t Amount = 0;
			DamageType Type = DamageType::ArmorPiercing;
		};

		// sistemas, na ordem do passo
		void ApplyCommand(const Command& C);
		void BuildBins();
		void UpdateTargets();
		void UpdatePaths();
		void UpdateMovement();
		void SeparateUnits();
		void UpdateTurrets();
		void UpdateWeapons();
		void UpdateProjectiles();
		void ResolveDamage();
		void RemoveDead();
		void CheckVictory();

		// ajudantes
		EntityId Spawn(PlayerIndex Owner, uint16_t DefIndex, Vec2 Pos, Angle Facing);
		int32_t IndexOf(EntityId Id) const;
		bool IsAliveEnemy(int32_t Self, EntityId Other) const;
		const UnitDef& DefOf(int32_t Index) const { return RulesData.Units[U.Def[Index]]; }
		Angle TurretWorld(int32_t Index) const { return Angle::FromBam(int32_t(U.Hull[Index].Bam) + U.TurretRel[Index]); }
		EntityId NearestEnemy(int32_t Self, Fix Radius) const;
		/** Começa a perseguir Target (guarda de onde saiu, pra coleira). */
		void StartEngage(int32_t Index, EntityId Target);
		Vec2 ClampToMap(Vec2 P) const;
		void GiveMove(int32_t Index, Vec2 Point, OrderType Order);
		/** As vagas de destino de um grupo, uma por membro (na ordem de Members). */
		void GroupSlots(const std::vector<int32_t>& Members, Vec2 Point, std::vector<Vec2>& Out);
		/** Região ligada da célula C pra uma folga (0 = não andável, ou folga sem mapa de regiões). */
		int32_t RegionAt(int32_t Clearance, Cell C) const;
		/** P se ele já está na região; senão a célula da região mais perto; senão Fallback. */
		Vec2 IntoRegion(int32_t Clearance, int32_t Region, Vec2 P, Vec2 Fallback) const;
		/** Pra onde a unidade quer ir neste passo; falso = ficar parada. */
		bool MovementGoal(int32_t Index, Vec2& OutGoal, Fix& OutStopDistance) const;

		uint64_t ComputeHash() const;

		UnitArrays U;
		std::vector<int32_t> IdToIndex;     // −1 = morta ou nunca existiu
		EntityId NextId = 1;
		std::vector<PlayerState> Players;
		SimMap MapData;
		Ruleset RulesData;
		Rng Random;
		Pathfinder Paths;
		TickNumber CurrentTick = 0;
		bool bMatchOver = false;
		int32_t Winner = -1;
		TickNumber MatchEndTick = 0;
		/** Onde a cota de caminhos do passo começa (dá a volta: ninguém fica sempre por último). */
		EntityId PathCursor = NoEntity;
		std::vector<SimEvent> Events;
		std::vector<Projectile> Projectiles;
		std::vector<DamageIntent> PendingDamage;
		Fix MaxUnitRadius;
		uint64_t CachedHash = 0;
		/** Regiões ligadas do mapa, uma grade por folga das unidades (sai do mapa: não entra no hash). */
		std::vector<int32_t> RegionClearances;
		std::vector<std::vector<int32_t>> RegionLabels;

		// grade de vizinhança (reconstruída quando as posições mudam)
		Fix BinSize = Fix::FromInt(16);
		int32_t BinsW = 0;
		int32_t BinsH = 0;
		std::vector<int32_t> BinStart;      // BinsW·BinsH + 1
		std::vector<int32_t> BinUnits;      // índices de unidade, por célula, em ordem de índice
		template <typename Fn> void ForEachNear(Vec2 P, Fix Radius, Fn&& Visit) const;
	};
}
