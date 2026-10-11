#include "Sim/SkirmishAI.h"

#include <algorithm>

namespace Sim
{
	namespace
	{
		/** Com base, ela reavalia a obra e a fila a cada 1 s (o ataque segue no ritmo do RethinkTicks). */
		constexpr int32_t BaseThinkTicks = TicksPerSecond;
		/** A espiral da procura de lugar: passo entre anéis e entre pontos, e até onde vai. */
		constexpr int32_t SpotRingStepM = 6;
		constexpr int32_t SpotPointStepM = 6;
		constexpr int32_t SpotMaxRingsM = 120;
		/** A folga a mais em volta do CC e do depósito quando ela começa a procurar. */
		constexpr int32_t SpotMarginM = 6;

		bool IsArmed(const World& State, const UnitView& V)
		{
			return State.Rules().Units[V.Def].HasWeapon();
		}
	}

	int32_t SkirmishAI::DirToward(Vec2 From, Vec2 To)
	{
		const Vec2 D = To - From;
		if (Abs(D.X) >= Abs(D.Y))
		{
			return D.X.Raw >= 0 ? 0 : 2;
		}
		return D.Y.Raw >= 0 ? 1 : 3;
	}

	bool SkirmishAI::FindSpot(const World& State, uint16_t Def, int32_t Dir, Vec2 Anchor, Fix MinRadius, Angle Start, Vec2& Out) const
	{
		// anéis a partir de MinRadius; em cada anel, pontos a cada ~6 m, sempre na mesma ordem, começando
		// virado pro inimigo (num mapa espelhado, os dois lados constroem no mesmo lugar relativo)
		if (State.CanPlace(Me, Def, Dir, Anchor) == PlaceResult::Ok && MinRadius.Raw <= 0)
		{
			Out = State.SnapStructure(Def, Dir, Anchor);
			return true;
		}
		for (int32_t RingM = 0; RingM <= SpotMaxRingsM; RingM += SpotRingStepM)
		{
			const Fix Radius = MinRadius + Fix::FromInt(RingM);
			if (Radius.Raw <= 0)
			{
				continue;
			}
			const int32_t Points = std::max<int32_t>(8, int32_t((Radius * 6).Floor() / SpotPointStepM));
			for (int32_t K = 0; K < Points; ++K)
			{
				const Angle A = Angle::FromBam(int32_t(Start.Bam) + int32_t(int64_t(K) * Angle::FullTurn / Points));
				const Vec2 P = Anchor + Vec2::FromAngle(A) * Radius;
				if (State.CanPlace(Me, Def, Dir, P) == PlaceResult::Ok)
				{
					Out = State.SnapStructure(Def, Dir, P);
					return true;
				}
			}
		}
		return false;
	}

	void SkirmishAI::Think(const World& State, std::vector<Command>& OutCommands)
	{
		const TickNumber Now = State.Tick();
		if (State.IsMatchOver() || Me >= State.PlayerCount() || State.Player(Me).bDefeated || Now < NextThink)
		{
			return;
		}
		bool bBase = false;
		for (int32_t I = 0; I < State.UnitCount() && !bBase; ++I)
		{
			const UnitView V = State.Unit(I);
			bBase = V.Owner == Me && V.bStructure;
		}
		if (bBase)
		{
			NextThink = Now + TickNumber(BaseThinkTicks);
			ThinkBase(State, OutCommands);
			if (Now % TickNumber(RethinkTicks) < TickNumber(BaseThinkTicks))
			{
				Attack(State, OutCommands, true);
			}
			return;
		}
		if (Now < TickNumber(FirstWaveDelayTicks))
		{
			NextThink = TickNumber(FirstWaveDelayTicks);
			return;
		}
		NextThink = Now + TickNumber(RethinkTicks);
		Attack(State, OutCommands, false);
	}

	void SkirmishAI::ThinkBase(const World& State, std::vector<Command>& OutCommands)
	{
		const Ruleset& R = State.Rules();
		const PlayerState& Self = State.Player(Me);
		// o inimigo: a largada do primeiro jogador inimigo
		Vec2 EnemyStart = State.PlayerStart(Me);
		for (int32_t P = 0; P < State.PlayerCount(); ++P)
		{
			if (State.AreEnemies(Me, PlayerIndex(P)))
			{
				EnemyStart = State.PlayerStart(PlayerIndex(P));
				break;
			}
		}
		const Vec2 Home = State.PlayerStart(Me);

		// os papéis saem dos dados, não do nome: usina gera energia, centro recebe caixas,
		// fábrica treina quem tem arma, CC treina quem constrói
		const auto ProducesArmed = [&](const UnitDef& D)
		{
			return std::any_of(D.Produces.begin(), D.Produces.end(), [&](uint16_t K) { return R.Units[K].HasWeapon(); });
		};
		const auto ProducesBuilder = [&](const UnitDef& D)
		{
			return std::any_of(D.Produces.begin(), D.Produces.end(), [&](uint16_t K) { return R.Units[K].CanConstruct(); });
		};
		enum Role { Power, Supply, Factory, RoleCount };
		const auto RoleOf = [&](const UnitDef& D) -> int32_t
		{
			if (!D.IsStructure())
			{
				return -1;
			}
			if (D.Energy > 0)
			{
				return Power;
			}
			if (D.bSupplyCenter)
			{
				return Supply;
			}
			return ProducesArmed(D) ? Factory : -1;
		};

		// o que ela tem
		int32_t Have[RoleCount] = {};
		EntityId Abandoned[RoleCount] = {};
		std::vector<UnitView> Dozers;
		int32_t Trucks = 0;
		EntityId Center = NoEntity;
		int32_t CenterQueue = 0;
		EntityId CommandCenter = NoEntity;
		std::vector<UnitView> Factories;
		std::vector<EntityId> Building;   // obras com Dozer trabalhando
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			if (V.Owner != Me)
			{
				continue;
			}
			const UnitDef& D = R.Units[V.Def];
			if (D.CanConstruct())
			{
				Dozers.push_back(V);
				if (V.Task == UnitTask::Construct)
				{
					Building.push_back(V.TaskTarget);
				}
			}
			Trucks += D.SupplyBoxes > 0 ? 1 : 0;
			const int32_t Role = RoleOf(D);
			if (Role >= 0)
			{
				++Have[Role];
				if (V.BuildLeft > 0)
				{
					Abandoned[Role] = V.Id;   // conferido abaixo: só é abandonada se nenhum Dozer está nela
				}
			}
			if (D.bSupplyCenter && V.BuildLeft == 0)
			{
				Center = V.Id;
				CenterQueue = V.QueueSize;
			}
			if (D.IsStructure() && V.BuildLeft == 0 && ProducesBuilder(D))
			{
				CommandCenter = V.Id;
			}
			if (Role == Factory && V.BuildLeft == 0)
			{
				Factories.push_back(V);
			}
		}
		for (EntityId& A : Abandoned)
		{
			if (A != NoEntity && std::find(Building.begin(), Building.end(), A) != Building.end())
			{
				A = NoEntity;
			}
		}

		// o Dozer à toa
		const UnitView* Free = nullptr;
		for (const UnitView& D : Dozers)
		{
			if (D.Task == UnitTask::None && D.Order == OrderType::Idle)
			{
				Free = &D;
				break;
			}
		}

		// a próxima obra da lista; uma usina a mais quando a energia não fecha
		int32_t Next = -1;
		for (int32_t Role = 0; Role < RoleCount && Next < 0; ++Role)
		{
			Next = Have[Role] == 0 ? Role : -1;
		}
		if (Next < 0 && Self.PowerUsed + 1 > Self.PowerMade && Abandoned[Power] == NoEntity && Building.empty())
		{
			Next = Power;
		}
		if (Free != nullptr && Next < 0)
		{
			for (int32_t Role = 0; Role < RoleCount; ++Role)
			{
				if (Abandoned[Role] != NoEntity)
				{
					// retoma a obra parada antes de começar outra
					Command G;
					G.Player = Me;
					G.Type = CommandType::SetGroup;
					G.Units = { Free->Id };
					OutCommands.push_back(G);
					Command B;
					B.Player = Me;
					B.Type = CommandType::Build;
					B.Target = Abandoned[Role];
					OutCommands.push_back(B);
					Free = nullptr;
					break;
				}
			}
		}
		if (Free != nullptr && Next >= 0)
		{
			int32_t Def = -1;
			for (const uint16_t K : R.Units[Free->Def].Constructs)
			{
				if (RoleOf(R.Units[K]) == Next && State.HasPrerequisites(Me, K))
				{
					Def = K;
					break;
				}
			}
			if (Def >= 0 && Self.Money >= R.Units[size_t(Def)].Cost)
			{
				const UnitDef& D = R.Units[size_t(Def)];
				Vec2 Anchor = Home;
				int32_t Dir = DirToward(Home, EnemyStart);
				Fix MinRadius = Fix::FromInt(30 + SpotMarginM);
				if (Next == Supply)
				{
					// o centro do lado de casa do depósito mais perto, com a porta virada pra ele
					int32_t Best = -1;
					int64_t BestD2 = 0;
					for (int32_t K = 0; K < State.DockCount(); ++K)
					{
						const DockView Dk = State.Dock(K);
						const int64_t D2 = DistSqRaw(Dk.Pos, Home);
						if (!Dk.bPile && Dk.Boxes > 0 && (Best < 0 || D2 < BestD2))
						{
							Best = K;
							BestD2 = D2;
						}
					}
					if (Best >= 0)
					{
						const DockView Dk = State.Dock(Best);
						const Fix Away = Max(Dk.HalfX, Dk.HalfY) + Max(D.HalfX, D.HalfY) + R.Economy.SupplyCenterDockGap + Fix::FromInt(SpotMarginM);
						Anchor = Dk.Pos + (Home - Dk.Pos).Normalized() * Away;
						Dir = DirToward(Anchor, Dk.Pos);
						MinRadius = Fix::Zero();
					}
				}
				Vec2 Spot;
				if (FindSpot(State, uint16_t(Def), Dir, Anchor, MinRadius, (EnemyStart - Home).Heading(), Spot))
				{
					Command G;
					G.Player = Me;
					G.Type = CommandType::SetGroup;
					G.Units = { Free->Id };
					OutCommands.push_back(G);
					Command B;
					B.Player = Me;
					B.Type = CommandType::Build;
					B.Value = BuildValue(uint16_t(Def), Dir);
					B.Point = Spot;
					OutCommands.push_back(B);
				}
				else if (Next == Supply)
				{
					// o depósito ainda está na névoa: o Dozer vai lá ver
					Command G;
					G.Player = Me;
					G.Type = CommandType::SetGroup;
					G.Units = { Free->Id };
					OutCommands.push_back(G);
					Command M;
					M.Player = Me;
					M.Type = CommandType::Move;
					M.Point = Anchor;
					OutCommands.push_back(M);
				}
			}
		}

		// produção: Dozer de reposição, caminhões e a fábrica sempre com fila
		int32_t Money = Self.Money;
		const auto Produce = [&](EntityId At, uint16_t Def)
		{
			if (Money < R.Units[Def].Cost || !State.HasPrerequisites(Me, Def))
			{
				return;
			}
			Money -= R.Units[Def].Cost;
			Command P;
			P.Player = Me;
			P.Type = CommandType::Produce;
			P.Target = At;
			P.Value = Def;
			OutCommands.push_back(P);
		};
		if (Dozers.empty() && CommandCenter != NoEntity)
		{
			UnitView CC;
			if (State.FindUnit(CommandCenter, CC) && CC.QueueSize == 0)
			{
				for (const uint16_t K : R.Units[CC.Def].Produces)
				{
					if (R.Units[K].CanConstruct())
					{
						Produce(CommandCenter, K);
						break;
					}
				}
			}
		}
		if (Center != NoEntity && Trucks < TrucksWanted && CenterQueue == 0)
		{
			UnitView C;
			State.FindUnit(Center, C);
			for (const uint16_t K : R.Units[C.Def].Produces)
			{
				if (R.Units[K].SupplyBoxes > 0)
				{
					Produce(Center, K);
					break;
				}
			}
		}
		for (const UnitView& F : Factories)
		{
			if (F.QueueSize >= 2)
			{
				continue;
			}
			for (const uint16_t K : R.Units[F.Def].Produces)
			{
				if (R.Units[K].HasWeapon())
				{
					Produce(F.Id, K);
					break;
				}
			}
		}
	}

	void SkirmishAI::Attack(const World& State, std::vector<Command>& OutCommands, bool bBase)
	{
		std::vector<EntityId> Army;
		std::vector<EntityId> Idle;
		Vec2 Sum = Vec2::Zero();
		for (int32_t I = 0; I < State.UnitCount(); ++I)
		{
			const UnitView V = State.Unit(I);
			// o exército é quem tem arma: prédio, Dozer e caminhão ficam fora da ofensiva
			if (V.Owner != Me || !IsArmed(State, V))
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
			bWaveSent = false;
			return;
		}
		if (bBase && !bWaveSent && int32_t(Army.size()) < WaveSize)
		{
			return;   // com base: junta a onda em casa antes de sair
		}
		bWaveSent = true;
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
				if (V.Owner == Me && IsArmed(State, V) && V.FireTarget == NoEntity && V.Order != OrderType::Idle && V.Order != OrderType::AttackMove)
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
