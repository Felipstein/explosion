#include "Sim/SkirmishAI.h"

namespace Sim
{
	void SkirmishAI::Think(const World& State, std::vector<Command>& OutCommands)
	{
		const TickNumber Now = State.Tick();
		if (State.IsMatchOver() || Me >= State.PlayerCount() || State.Player(Me).bDefeated || Now < NextThink)
		{
			return;
		}
		if (Now < TickNumber(FirstWaveDelayTicks))
		{
			NextThink = TickNumber(FirstWaveDelayTicks);
			return;
		}
		NextThink = Now + TickNumber(RethinkTicks);

		std::vector<EntityId> Army;
		std::vector<EntityId> Idle;
		Vec2 Sum = Vec2::Zero();
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner != Me)
			{
				continue;
			}
			Army.push_back(V.Id);
			Sum += V.Pos;
			if (V.FireTarget == NoEntity && (V.Order == OrderType::Idle || CurrentTarget == NoEntity))
			{
				Idle.push_back(V.Id);
			}
		}
		if (Army.empty())
		{
			return;
		}
		const Vec2 Center = Vec2::Make(Sum.X / int64_t(Army.size()), Sum.Y / int64_t(Army.size()));

		// o inimigo mais perto do centro do exército (desempate pelo ID)
		EntityId Target = NoEntity;
		Vec2 TargetPos;
		int64_t BestD2 = 0;
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (!State.AreEnemies(Me, V.Owner))
			{
				continue;
			}
			const int64_t D2 = DistSqRaw(Center, V.Pos);
			if (Target == NoEntity || D2 < BestD2)
			{
				Target = V.Id;
				TargetPos = V.Pos;
				BestD2 = D2;
			}
		}
		if (Target == NoEntity)
		{
			return;
		}

		// só quem está sem alvo recebe a ordem: reenviar pro exército inteiro apagava o alvo de
		// quem já estava atirando (o "tranco" a cada morte); a primeira onda vai inteira
		std::vector<EntityId> Who = Idle;
		if (CurrentTarget != Target)
		{
			for (int32_t I = 0; I < State.UnitCount(); ++I)
			{
				const UnitView V = State.Unit(I);
				if (V.Owner == Me && V.FireTarget == NoEntity && V.Order != OrderType::Idle && V.Order != OrderType::AttackMove)
				{
					Who.push_back(V.Id);
				}
			}
		}
		if (Who.empty())
		{
			CurrentTarget = Target;
			return;
		}
		if (Who.size() > size_t(MaxGroupSize))
		{
			Who.resize(size_t(MaxGroupSize));
		}
		CurrentTarget = Target;

		Command Group;
		Group.Player = Me;
		Group.Type = CommandType::SetGroup;
		Group.Units = Who;
		OutCommands.push_back(Group);

		Command Go;
		Go.Player = Me;
		Go.Type = CommandType::AttackMove;
		Go.Point = TargetPos;
		OutCommands.push_back(Go);
	}
}
