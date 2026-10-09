#include "Sim/World.h"

#include <algorithm>

#include "Sim/Hash.h"

namespace Sim
{
	namespace
	{
		/** Cada unidade procura alvo sozinha a cada 0,5 s, em turnos espalhados pelo ID. */
		constexpr int32_t TargetScanInterval = 15;
		/** Teto de buscas de caminho por passo (o resto espera o próximo, em ordem de ID). */
		constexpr int32_t MaxPathsPerTick = 16;
		/** Intervalo mínimo entre caminhos da mesma unidade quando o destino anda (perseguição). */
		constexpr int32_t RepathTicks = 15;
		/** Desvio acima do qual o tanque gira parado em vez de andar fazendo curva (60°). */
		constexpr int32_t TurnInPlaceBam = Angle::FullTurn / 6;
		/** Janela da checagem de "empacou": 1,5 s. */
		constexpr int32_t StuckWindowTicks = 45;
		/** Perseguição: para a 85% do alcance (o Vanguard mediu que parar no limite faz o alvo escapar). */
		constexpr Fix ChaseStopFraction = Fix::FromRatio(85, 100);
		/** Reação: só vai atrás de quem atirou se estiver a até 1,5× a visão. */
		constexpr Fix LeashFraction = Fix::FromRatio(3, 2);
		constexpr Fix ArriveDistance = Fix::FromInt(2);
		constexpr Fix StuckMinTravel = Fix::FromInt(1);
		/** Mudança de destino que obriga a refazer o caminho. */
		constexpr Fix RepathGoalShift = Fix::FromInt(4);
		/** Empurrão máximo entre unidades por passo (0,13 m = 3,9 m/s, metade da velocidade do tanque). */
		constexpr Fix MaxPushPerTick = Fix::FromRatio(13, 100);
		/** Quem está parado a até isto de um aliado que leva tiro vai ajudar. */
		constexpr Fix HelpRadius = Fix::FromInt(20);
		/** A visão de quem morreu fica 1 s (no SC2 ela não some no mesmo instante). */
		constexpr int32_t VisionLingerTicks = TicksPerSecond;
		/** Quem atira de dentro da névoa aparece num disco de 1 célula em volta dele. */
		constexpr int32_t ShotRevealCells = 1;

		template <typename T>
		void Compact(std::vector<T>& V, const std::vector<uint8_t>& Keep)
		{
			size_t W = 0;
			for (size_t R = 0; R < V.size(); ++R)
			{
				if (Keep[R])
				{
					if (W != R)
					{
						V[W] = std::move(V[R]);
					}
					++W;
				}
			}
			V.resize(W);
		}

		int32_t SignedBam(int32_t V)
		{
			return int32_t(int16_t(uint16_t(V & 0xFFFF)));
		}

		Fix ClampFix(Fix V, Fix Lo, Fix Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	}

	// ---------------------------------------------------------------- arrays

	void World::UnitArrays::Resize(size_t N)
	{
		Id.resize(N); Owner.resize(N); Def.resize(N); Pos.resize(N); Hull.resize(N);
		TurretRel.resize(N); Speed.resize(N); Hp.resize(N); Cooldown.resize(N); Order.resize(N);
		OrderPoint.resize(N); OrderTarget.resize(N); Engage.resize(N); FireTarget.resize(N);
		LastAttacker.resize(N); TurretIdle.resize(N); Path.resize(N); PathIndex.resize(N);
		PathGoal.resize(N); NeedsPath.resize(N); RepathWait.resize(N); StuckTicks.resize(N);
		StuckAnchor.resize(N); EngageAnchor.resize(N); Ignore.resize(N); IgnoreUntil.resize(N);
		LookCell.resize(N);
	}

	// ---------------------------------------------------------------- montagem

	uint64_t MatchSetup::Hash() const
	{
		Hasher H;
		H.Add(Seed);
		H.Add(Map.Hash());
		H.Add(Rules.Hash());
		H.Add(bFogOfWar);
		H.Add(bAllowDebugCommands);
		H.Add(uint64_t(Players.size()));
		for (const PlayerSetup& P : Players)
		{
			H.Add(P.Team);
			H.Add(P.bIsAI);
			H.Add(uint64_t(P.Units.size()));
			for (const StartingUnit& S : P.Units)
			{
				H.Add(S.Key);
				H.Add(S.Pos);
				H.Add(S.Facing);
			}
		}
		return H.Value();
	}

	bool World::Init(const MatchSetup& Setup)
	{
		*this = World();
		if (Setup.Players.empty() || Setup.Players.size() > size_t(MaxPlayers) || Setup.Map.Width <= 0 || Setup.Map.Height <= 0)
		{
			return false;
		}
		MapData = Setup.Map;
		MapData.ComputeClearance();
		RulesData = Setup.Rules;
		Random = Rng(Setup.Seed);
		// antes de nascer: o raio de visão de cada um depende do maior raio (SightCells)
		MaxUnitRadius = Fix::Zero();
		for (const UnitDef& D : RulesData.Units)
		{
			MaxUnitRadius = Max(MaxUnitRadius, D.Radius);
		}
		int32_t Teams = 0;
		for (const PlayerSetup& P : Setup.Players)
		{
			if (P.Team >= MaxPlayers)
			{
				return false;   // o time é índice da grade de visão: nunca maior que o número de vagas
			}
			PlayerState S;
			S.Team = P.Team;
			Players.push_back(S);
			Teams = std::max(Teams, int32_t(P.Team) + 1);
		}
		Vision.Init(MapData.Width, MapData.Height, Teams);
		bFogOfWar = Setup.bFogOfWar;
		bAllowDebug = Setup.bAllowDebugCommands;
		// Onde cada unidade nasce. A montagem vem de fora (formação em volta da largada) e pode
		// cair em cima de pedra, penhasco ou num platô isolado; quem nasce num lugar de onde não
		// sai fica presa a partida toda e ninguém vence (teste t4, 08/10). Então: a célula
		// andável mais perto e, depois, a região do exército dela (onde nasce a maioria do
		// jogador); quem ficou fora vai pra célula dessa região mais perto.
		struct Placement
		{
			PlayerIndex Owner;
			uint16_t Def;
			int32_t Clearance;
			Cell C;
			Vec2 Pos;
			Angle Facing;
		};
		std::vector<Placement> Places;
		for (size_t P = 0; P < Setup.Players.size(); ++P)
		{
			for (const StartingUnit& S : Setup.Players[P].Units)
			{
				const int32_t DefIndex = RulesData.FindUnit(S.Key);
				if (DefIndex < 0)
				{
					return false;
				}
				Placement Pl{ PlayerIndex(P), uint16_t(DefIndex), MapData.ClearanceFor(RulesData.Units[DefIndex].PathRadius), Cell{}, ClampToMap(S.Pos), S.Facing };
				Pl.C = MapData.CellOf(Pl.Pos);
				if (!MapData.IsWalkable(Pl.C.X, Pl.C.Y, Pl.Clearance) && Pathfinder::NearestWalkable(MapData, Pl.C, Pl.Clearance, 16, Pl.C))
				{
					Pl.Pos = MapData.CellCenter(Pl.C);
				}
				Places.push_back(Pl);
			}
		}
		// uma grade de regiões por folga: as das unidades do jogo (pras vagas de grupo) e as de quem nasce
		std::vector<int32_t>& LabelClearances = RegionClearances;
		std::vector<std::vector<int32_t>>& Labels = RegionLabels;
		std::vector<std::vector<int32_t>> RegionSizes;
		const auto AddClearance = [&](int32_t Clearance)
		{
			if (std::find(LabelClearances.begin(), LabelClearances.end(), Clearance) == LabelClearances.end())
			{
				LabelClearances.push_back(Clearance);
				Labels.emplace_back();
				RegionSizes.push_back(Pathfinder::LabelRegions(MapData, Clearance, Labels.back()));
			}
		};
		for (const UnitDef& D : RulesData.Units)
		{
			AddClearance(MapData.ClearanceFor(D.PathRadius));
		}
		for (const Placement& Pl : Places)
		{
			AddClearance(Pl.Clearance);
		}
		for (size_t P = 0; P < Setup.Players.size(); ++P)
		{
			for (size_t K = 0; K < LabelClearances.size(); ++K)
			{
				const std::vector<int32_t>& L = Labels[K];
				const std::vector<int32_t>& Sizes = RegionSizes[K];
				std::vector<int32_t> Votes(Sizes.size(), 0);
				for (const Placement& Pl : Places)
				{
					if (Pl.Owner == P && Pl.Clearance == LabelClearances[K])
					{
						Votes[size_t(L[size_t(MapData.Index(Pl.C.X, Pl.C.Y))])] += 1;
					}
				}
				// a região com mais unidades; empate = a maior região; depois o menor rótulo
				int32_t Home = 0;
				for (int32_t R = 1; R < int32_t(Votes.size()); ++R)
				{
					const int32_t V = Votes[size_t(R)];
					const int32_t Best = Votes[size_t(Home)];
					if (V > 0 && (V > Best || (V == Best && Sizes[size_t(R)] > Sizes[size_t(Home)])))
					{
						Home = R;
					}
				}
				if (Home == 0)
				{
					continue;
				}
				for (Placement& Pl : Places)
				{
					Cell To;
					if (Pl.Owner == P && Pl.Clearance == LabelClearances[K] && L[size_t(MapData.Index(Pl.C.X, Pl.C.Y))] != Home
						&& Pathfinder::NearestInRegion(MapData, L, Home, Pl.C, 64, To))
					{
						Pl.C = To;
						Pl.Pos = MapData.CellCenter(To);
					}
				}
			}
		}
		Starts.assign(Setup.Players.size(), Vec2::Zero());
		std::vector<int64_t> StartCount(Setup.Players.size(), 0);
		for (const Placement& Pl : Places)
		{
			Spawn(Pl.Owner, Pl.Def, Pl.Pos, Pl.Facing);
			Starts[Pl.Owner] += Pl.Pos;
			StartCount[Pl.Owner] += 1;
		}
		for (size_t P = 0; P < Starts.size(); ++P)
		{
			if (StartCount[P] > 0)
			{
				Starts[P] = Vec2::Make(Starts[P].X / StartCount[P], Starts[P].Y / StartCount[P]);
			}
		}
		const Vec2 Extent = MapData.MaxCorner() - MapData.MinCorner();
		BinsW = int32_t((Extent.X / BinSize).Floor()) + 1;
		BinsH = int32_t((Extent.Y / BinSize).Floor()) + 1;
		CachedHash = ComputeHash();
		return true;
	}

	EntityId World::Spawn(PlayerIndex Owner, uint16_t DefIndex, Vec2 Pos, Angle Facing)
	{
		const EntityId Id = NextId++;
		const size_t I = U.Id.size();
		U.Resize(I + 1);
		const UnitDef& D = RulesData.Units[DefIndex];
		U.Id[I] = Id;
		U.Owner[I] = Owner;
		U.Def[I] = DefIndex;
		U.Pos[I] = Pos;
		U.Hull[I] = Facing;
		U.TurretRel[I] = 0;
		U.Speed[I] = Fix::Zero();
		U.Hp[I] = D.MaxHp;
		U.Cooldown[I] = 0;
		U.Order[I] = OrderType::Idle;
		U.OrderPoint[I] = Pos;
		U.OrderTarget[I] = NoEntity;
		U.Engage[I] = NoEntity;
		U.FireTarget[I] = NoEntity;
		U.LastAttacker[I] = NoEntity;
		U.TurretIdle[I] = 0;
		U.Path[I].clear();
		U.PathIndex[I] = 0;
		U.PathGoal[I] = Pos;
		U.NeedsPath[I] = 0;
		U.RepathWait[I] = 0;
		U.StuckTicks[I] = 0;
		U.StuckAnchor[I] = Pos;
		U.EngageAnchor[I] = Pos;
		U.Ignore[I] = NoEntity;
		U.IgnoreUntil[I] = 0;
		const Cell C = MapData.CellOf(Pos);
		U.LookCell[I] = MapData.InBounds(C.X, C.Y) ? MapData.Index(C.X, C.Y) : -1;
		LookAt(Players[Owner].Team, DefIndex, U.LookCell[I], +1);
		if (IdToIndex.size() <= Id)
		{
			IdToIndex.resize(size_t(Id) + 1, -1);
		}
		IdToIndex[Id] = int32_t(I);
		Events.push_back({ EventType::UnitSpawned, NoEntity, Id, Pos, Owner });
		return Id;
	}

	// ---------------------------------------------------------------- leitura

	int32_t World::IndexOf(EntityId Id) const
	{
		return Id < IdToIndex.size() ? IdToIndex[Id] : -1;
	}

	bool World::IsAliveEnemy(int32_t Self, EntityId Other) const
	{
		const int32_t J = IndexOf(Other);
		return J >= 0 && U.Hp[J] > 0 && AreEnemies(U.Owner[Self], U.Owner[J]);
	}

	UnitView World::Unit(int32_t I) const
	{
		UnitView V;
		V.Id = U.Id[I];
		V.Owner = U.Owner[I];
		V.Def = U.Def[I];
		V.Pos = U.Pos[I];
		V.Hull = U.Hull[I];
		V.Turret = TurretWorld(I);
		V.Speed = U.Speed[I];
		V.Hp = U.Hp[I];
		V.MaxHp = DefOf(I).MaxHp;
		V.Order = U.Order[I];
		V.OrderPoint = U.OrderPoint[I];
		V.FireTarget = U.FireTarget[I];
		V.Engage = U.Engage[I];
		V.PathLeft = int32_t(U.Path[I].size()) - int32_t(U.PathIndex[I]);
		V.Cooldown = U.Cooldown[I];
		return V;
	}

	bool World::FindUnit(EntityId Id, UnitView& Out) const
	{
		const int32_t I = IndexOf(Id);
		if (I < 0)
		{
			return false;
		}
		Out = Unit(I);
		return true;
	}

	// ---------------------------------------------------------------- passo

	void World::Step(const std::vector<Command>& Commands)
	{
		Events.clear();
		// depuração (só se a montagem deixar): vale até depois do fim, pra olhar o mapa
		for (const Command& C : Commands)
		{
			if (C.Tick == CurrentTick && C.Type == CommandType::SetFog && bAllowDebug && C.Player < Players.size())
			{
				bFogOfWar = C.Target != 0;
			}
		}
		// depois do fim a partida segue rodando (os tanques terminam a curva, a fumaça sobe),
		// só sem ordens novas e sem conferir a vitória de novo
		if (!bMatchOver)
		{
			for (const Command& C : Commands)
			{
				if (C.Tick == CurrentTick && C.Type != CommandType::SetFog)
				{
					ApplyCommand(C);
				}
			}
		}
		BuildBins();
		UpdateTargets();
		UpdatePaths();
		UpdateMovement();
		BuildBins();
		SeparateUnits();
		BuildBins();
		UpdateTurrets();
		UpdateProjectiles();
		UpdateWeapons();
		ResolveDamage();
		RemoveDead();
		UpdateVision();
		if (!bMatchOver)
		{
			CheckVictory();
		}
		++CurrentTick;
		CachedHash = ComputeHash();
	}

	// ---------------------------------------------------------------- comandos

	void World::ApplyCommand(const Command& C)
	{
		if (C.Player >= Players.size() || Players[C.Player].bDefeated)
		{
			return;
		}
		PlayerState& P = Players[C.Player];
		if (C.Type == CommandType::SetGroup)
		{
			// só entra o que é do jogador e está vivo; ordem de ID, sem repetição
			P.Group.clear();
			for (size_t K = 0; K < C.Units.size() && K < size_t(MaxGroupSize); ++K)
			{
				const int32_t I = IndexOf(C.Units[K]);
				if (I >= 0 && U.Owner[I] == C.Player)
				{
					P.Group.push_back(C.Units[K]);
				}
			}
			std::sort(P.Group.begin(), P.Group.end());
			P.Group.erase(std::unique(P.Group.begin(), P.Group.end()), P.Group.end());
			return;
		}

		std::vector<int32_t> Members;
		for (const EntityId Id : P.Group)
		{
			const int32_t I = IndexOf(Id);
			if (I >= 0 && U.Owner[I] == C.Player)
			{
				Members.push_back(I);
			}
		}
		if (Members.empty())
		{
			return;
		}

		switch (C.Type)
		{
		case CommandType::Move:
		case CommandType::AttackMove:
		{
			const OrderType Order = C.Type == CommandType::Move ? OrderType::Move : OrderType::AttackMove;
			std::vector<Vec2> Slots;
			GroupSlots(Members, ClampToMap(C.Point), Slots);
			for (size_t K = 0; K < Members.size(); ++K)
			{
				GiveMove(Members[K], Slots[K], Order);
			}
			break;
		}
		case CommandType::Attack:
		{
			if (!IsAliveEnemy(Members.front(), C.Target))
			{
				return;   // alvo morto ou aliado: ordem inválida, ignora (ataque forçado vem depois)
			}

			for (const int32_t I : Members)
			{
				U.Order[I] = OrderType::Attack;
				U.OrderTarget[I] = C.Target;
				U.Engage[I] = C.Target;
				U.Path[I].clear();
				U.NeedsPath[I] = 1;
				U.StuckTicks[I] = 0;
				U.StuckAnchor[I] = U.Pos[I];
			}
			break;
		}
		case CommandType::Stop:
			for (const int32_t I : Members)
			{
				U.Order[I] = OrderType::Idle;
				U.OrderTarget[I] = NoEntity;
				U.Engage[I] = NoEntity;
				U.LastAttacker[I] = NoEntity;
				U.Path[I].clear();
				U.NeedsPath[I] = 0;
			}
			break;
		case CommandType::SetGroup:
			break;
		}
	}

	void World::GiveMove(int32_t I, Vec2 Point, OrderType Order)
	{
		U.Order[I] = Order;
		U.OrderPoint[I] = Point;
		U.OrderTarget[I] = NoEntity;
		U.Engage[I] = NoEntity;
		U.LastAttacker[I] = NoEntity;
		U.Path[I].clear();
		U.NeedsPath[I] = 1;
		U.RepathWait[I] = 0;
		U.StuckTicks[I] = 0;
		U.StuckAnchor[I] = U.Pos[I];
	}

	// ---------------------------------------------------------------- vagas do grupo
	//
	// Como o Zero Hour arruma um grupo de veículos (AIGroup::groupMoveToPosition, lido como
	// referência, não copiado):
	// - clique dentro do grupo: juntar em volta do ponto;
	// - 4 ou mais veículos (MinVehiclesForGroup): colunas alinhadas com o fim do caminho, a
	//   frente no ponto, cada um com a sua vaga (2 colunas com menos de 5, senão 3);
	// - grupo pequeno: cada um guarda a distância que tinha do centro, até 6 raios.
	// As vagas ficam a 3,2 células de 10 unidades = 32 unidades do ZH (8,43 m) pro Crusader de
	// 30 unidades: 2,34 raios do nosso tanque de 3,6 m. Diferenças nossas: as fileiras também
	// usam esse passo (no ZH, com menos de 5, elas encostam e o ajuste de destino separa) e
	// grupo grande vira um bloco quase quadrado em vez de 3 colunas de centenas de metros.
	namespace
	{
		constexpr int64_t SlotSpacingPerRadiusMilli = 2340;
		constexpr int32_t MinVehiclesForColumns = 4;
		constexpr int32_t MaxThreeColumnGroup = 12;
	}

	int32_t World::RegionAt(int32_t Clearance, Cell C) const
	{
		for (size_t K = 0; K < RegionClearances.size(); ++K)
		{
			if (RegionClearances[K] == Clearance)
			{
				return MapData.InBounds(C.X, C.Y) ? RegionLabels[K][size_t(MapData.Index(C.X, C.Y))] : 0;
			}
		}
		return 0;
	}

	Vec2 World::IntoRegion(int32_t Clearance, int32_t Region, Vec2 P, Vec2 Fallback) const
	{
		for (size_t K = 0; K < RegionClearances.size(); ++K)
		{
			if (RegionClearances[K] != Clearance)
			{
				continue;
			}
			const Cell C = MapData.CellOf(P);
			Cell To;
			if (MapData.InBounds(C.X, C.Y) && RegionLabels[K][size_t(MapData.Index(C.X, C.Y))] == Region)
			{
				return P;
			}
			return Pathfinder::NearestInRegion(MapData, RegionLabels[K], Region, C, 6, To) ? MapData.CellCenter(To) : Fallback;
		}
		return P;
	}

	void World::GroupSlots(const std::vector<int32_t>& Members, Vec2 Point, std::vector<Vec2>& Out)
	{
		const int32_t N = int32_t(Members.size());
		Out.assign(size_t(N), Point);
		if (N <= 1)
		{
			return;
		}
		Vec2 Sum = Vec2::Zero();
		Vec2 Lo = U.Pos[Members[0]];
		Vec2 Hi = Lo;
		Fix MaxRadius = Fix::Zero();
		int32_t Clearance = 1;
		for (const int32_t I : Members)
		{
			const Vec2 P = U.Pos[I];
			Sum += P;
			Lo = Vec2::Make(Min(Lo.X, P.X), Min(Lo.Y, P.Y));
			Hi = Vec2::Make(Max(Hi.X, P.X), Max(Hi.Y, P.Y));
			MaxRadius = Max(MaxRadius, DefOf(I).Radius);
			Clearance = std::max(Clearance, MapData.ClearanceFor(DefOf(I).PathRadius));
		}
		const Vec2 Center = Vec2::Make(Sum.X / int64_t(N), Sum.Y / int64_t(N));
		const Fix Spacing = MaxRadius * Fix::FromMilli(SlotSpacingPerRadiusMilli);

		// a região do ponto: a vaga não pode cair do outro lado de um penhasco nem na água
		Cell PointCell = MapData.CellOf(Point);
		const int32_t Region = Pathfinder::NearestWalkable(MapData, PointCell, Clearance, 24, PointCell) ? RegionAt(Clearance, PointCell) : 0;
		const auto Settle = [&](Vec2 Slot) { return Region != 0 ? IntoRegion(Clearance, Region, ClampToMap(Slot), Point) : ClampToMap(Slot); };

		const bool bInside = Point.X >= Lo.X && Point.X <= Hi.X && Point.Y >= Lo.Y && Point.Y <= Hi.Y;
		if (bInside)
		{
			// juntar: vagas em favo em volta do ponto, das mais perto pras mais longe; cada vaga
			// fica com o membro livre mais perto dela
			const int32_t Rings = int32_t(ISqrt(uint64_t(N))) + 2;
			struct Candidate
			{
				int64_t D2;
				int32_t GY;
				int32_t GX;
				Vec2 Pos;
			};
			std::vector<Candidate> Candidates;
			const Fix RowStep = Spacing * Fix::FromMilli(866);   // √3/2: fileiras do favo
			for (int32_t GY = -Rings; GY <= Rings; ++GY)
			{
				for (int32_t GX = -Rings; GX <= Rings; ++GX)
				{
					const Fix X = Spacing * GX + ((GY & 1) != 0 ? Spacing / 2 : Fix::Zero());
					const Vec2 P = ClampToMap(Point + Vec2::Make(X, RowStep * GY));
					if (Region != 0 && RegionAt(Clearance, MapData.CellOf(P)) != Region)
					{
						continue;
					}
					Candidates.push_back({ DistSqRaw(P, Point), GY, GX, P });
				}
			}
			std::sort(Candidates.begin(), Candidates.end(), [](const Candidate& A, const Candidate& B)
			{
				return A.D2 != B.D2 ? A.D2 < B.D2 : (A.GY != B.GY ? A.GY < B.GY : A.GX < B.GX);
			});
			std::vector<uint8_t> Taken(size_t(N), 0);
			for (size_t S = 0; S < Candidates.size() && S < size_t(N); ++S)
			{
				int32_t Best = -1;
				for (int32_t K = 0; K < N; ++K)
				{
					if (!Taken[size_t(K)] && (Best < 0 || DistSqRaw(U.Pos[Members[K]], Candidates[S].Pos) < DistSqRaw(U.Pos[Members[Best]], Candidates[S].Pos)))
					{
						Best = K;
					}
				}
				Taken[size_t(Best)] = 1;
				Out[size_t(Best)] = Candidates[S].Pos;
			}
			return;
		}

		if (N >= MinVehiclesForColumns)
		{
			// a direção de chegada: o último trecho do caminho do centro do grupo até o ponto
			// (o ZH usa o fim do caminho do grupo, a 6 células de 10 unidades ~ 2 vagas)
			Vec2 Dir = Point - Center;
			std::vector<Vec2> Way;
			if (Paths.FindPath(MapData, Center, Point, Clearance, Way) && !Way.empty())
			{
				const Vec2 End = Way.back();
				Vec2 From = Center;
				for (size_t K = Way.size(); K-- > 0;)
				{
					if (DistSqRaw(Way[K], End) > RadiusSqRaw(Spacing * 2))
					{
						From = Way[K];
						break;
					}
				}
				if (DistSqRaw(End, From) > 0)
				{
					Dir = End - From;
				}
			}
			Dir = Dir.LengthSqRaw() > 0 ? Dir.Normalized() : Vec2::Make(Fix::One(), Fix::Zero());
			const Vec2 Side = Vec2::Make(-Dir.Y, Dir.X);
			const int32_t Cols = N < 5 ? 2 : (N <= MaxThreeColumnGroup ? 3 : int32_t(ISqrt(uint64_t(N))));

			// coluna pela posição de lado (ninguém cruza o caminho do vizinho); na coluna,
			// quem está mais à frente fica na frente
			std::vector<Fix> Lateral(static_cast<size_t>(N));
			std::vector<Fix> Forward(static_cast<size_t>(N));
			std::vector<int32_t> Order(static_cast<size_t>(N));
			for (int32_t K = 0; K < N; ++K)
			{
				const Vec2 Rel = U.Pos[Members[K]] - Center;
				Lateral[size_t(K)] = Rel.Dot(Side);
				Forward[size_t(K)] = Rel.Dot(Dir);
				Order[size_t(K)] = K;
			}
			std::sort(Order.begin(), Order.end(), [&](int32_t A, int32_t B)
			{
				return Lateral[size_t(A)] != Lateral[size_t(B)] ? Lateral[size_t(A)] < Lateral[size_t(B)] : A < B;
			});
			std::vector<std::vector<int32_t>> Columns(static_cast<size_t>(Cols));
			for (int32_t R = 0; R < N; ++R)
			{
				Columns[size_t(int64_t(R) * Cols / N)].push_back(Order[size_t(R)]);
			}
			for (int32_t Col = 0; Col < Cols; ++Col)
			{
				std::vector<int32_t>& Members2 = Columns[size_t(Col)];
				std::sort(Members2.begin(), Members2.end(), [&](int32_t A, int32_t B)
				{
					return Forward[size_t(A)] != Forward[size_t(B)] ? Forward[size_t(A)] > Forward[size_t(B)] : A < B;
				});
				const Fix Lat = Spacing * int64_t(2 * Col - (Cols - 1)) / 2;
				for (size_t Row = 0; Row < Members2.size(); ++Row)
				{
					Vec2 Slot = Point + Side * Lat - Dir * (Spacing * int64_t(Row));
					if ((Row & 1) != 0)
					{
						Slot += Side * (Spacing * 5 / 16);   // o zigue-zague do ZH: 1 célula em 3,2
					}
					Out[size_t(Members2[Row])] = Settle(Slot);
				}
			}
			return;
		}

		// grupo pequeno: cada um guarda a distância do centro, até 6 raios
		for (int32_t K = 0; K < N; ++K)
		{
			const int32_t I = Members[size_t(K)];
			Vec2 Offset = U.Pos[I] - Center;
			const Fix Limit = DefOf(I).Radius * 6;
			if (Offset.LengthSqRaw() > RadiusSqRaw(Limit))
			{
				Offset = Offset.Normalized() * Limit;
			}
			Out[size_t(K)] = Settle(Point + Offset);
		}
	}

	Vec2 World::ClampToMap(Vec2 P) const
	{
		const Vec2 Lo = MapData.MinCorner();
		const Vec2 Hi = MapData.MaxCorner();
		return Vec2::Make(ClampFix(P.X, Lo.X, Hi.X - Fix::One()), ClampFix(P.Y, Lo.Y, Hi.Y - Fix::One()));
	}

	void World::StartEngage(int32_t I, EntityId Target)
	{
		if (U.Engage[I] != Target)
		{
			U.Engage[I] = Target;
			U.EngageAnchor[I] = U.Pos[I];
			U.NeedsPath[I] = 1;
		}
	}

	// ---------------------------------------------------------------- vizinhança

	void World::BuildBins()
	{
		const int32_t NumBins = BinsW * BinsH;
		BinStart.assign(size_t(NumBins) + 1, 0);
		BinUnits.assign(U.Id.size(), 0);
		std::vector<int32_t> BinOf(U.Id.size());
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			const Vec2 Local = U.Pos[I] - MapData.MinCorner();
			int32_t BX = int32_t((Local.X / BinSize).Floor());
			int32_t BY = int32_t((Local.Y / BinSize).Floor());
			BX = BX < 0 ? 0 : (BX >= BinsW ? BinsW - 1 : BX);
			BY = BY < 0 ? 0 : (BY >= BinsH ? BinsH - 1 : BY);
			BinOf[I] = BY * BinsW + BX;
			++BinStart[size_t(BinOf[I]) + 1];
		}
		for (int32_t B = 0; B < NumBins; ++B)
		{
			BinStart[size_t(B) + 1] += BinStart[size_t(B)];
		}
		std::vector<int32_t> Fill(BinStart.begin(), BinStart.end() - 1);
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			BinUnits[size_t(Fill[size_t(BinOf[I])]++)] = int32_t(I);
		}
	}

	template <typename Fn>
	void World::ForEachNear(Vec2 P, Fix Radius, Fn&& Visit) const
	{
		const Vec2 Local = P - MapData.MinCorner();
		auto ClampBin = [](int32_t V, int32_t N) { return V < 0 ? 0 : (V >= N ? N - 1 : V); };
		const int32_t X0 = ClampBin(int32_t(((Local.X - Radius) / BinSize).Floor()), BinsW);
		const int32_t X1 = ClampBin(int32_t(((Local.X + Radius) / BinSize).Floor()), BinsW);
		const int32_t Y0 = ClampBin(int32_t(((Local.Y - Radius) / BinSize).Floor()), BinsH);
		const int32_t Y1 = ClampBin(int32_t(((Local.Y + Radius) / BinSize).Floor()), BinsH);
		for (int32_t BY = Y0; BY <= Y1; ++BY)
		{
			for (int32_t BX = X0; BX <= X1; ++BX)
			{
				const int32_t B = BY * BinsW + BX;
				for (int32_t K = BinStart[size_t(B)]; K < BinStart[size_t(B) + 1]; ++K)
				{
					Visit(BinUnits[size_t(K)]);
				}
			}
		}
	}

	EntityId World::NearestEnemy(int32_t Self, Fix Radius) const
	{
		EntityId Best = NoEntity;
		int64_t BestD2 = 0;
		const bool bIgnoring = U.Ignore[Self] != NoEntity && CurrentTick < U.IgnoreUntil[Self];
		ForEachNear(U.Pos[Self], Radius + MaxUnitRadius, [&](int32_t J)
		{
			if (U.Hp[J] <= 0 || !AreEnemies(U.Owner[Self], U.Owner[J]) || (bIgnoring && U.Id[J] == U.Ignore[Self]) || !SeesUnit(Self, J))
			{
				return;
			}
			const int64_t D2 = DistSqRaw(U.Pos[Self], U.Pos[J]);
			if (D2 > RadiusSqRaw(Radius + DefOf(J).Radius))
			{
				return;
			}
			if (Best == NoEntity || D2 < BestD2 || (D2 == BestD2 && U.Id[J] < Best))
			{
				Best = U.Id[J];
				BestD2 = D2;
			}
		});
		return Best;
	}

	// ---------------------------------------------------------------- alvos

	void World::UpdateTargets()
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			const bool bScan = ((CurrentTick + U.Id[I]) % TargetScanInterval) == 0;

			if (U.Order[I] == OrderType::Attack && !IsAliveEnemy(I, U.OrderTarget[I]))
			{
				// alvo morreu: para onde está (o Vanguard tinha o bug de ir até o cadáver)
				U.Order[I] = OrderType::Idle;
				U.OrderTarget[I] = NoEntity;
				U.Path[I].clear();
			}
			if (U.Order[I] == OrderType::Attack && bFogOfWar)
			{
				// o alvo da ordem sumiu na névoa: vai até onde ele foi visto, atacando o que aparecer (ZH)
				const int32_t J = IndexOf(U.OrderTarget[I]);
				if (J >= 0 && !SeesUnit(I, J))
				{
					GiveMove(I, U.Pos[J], OrderType::AttackMove);
				}
			}
			if (!IsAliveEnemy(I, U.Engage[I]) || (IndexOf(U.Engage[I]) >= 0 && !SeesUnit(I, IndexOf(U.Engage[I]))))
			{
				U.Engage[I] = NoEntity;   // morto, ou sumiu na névoa
			}
			if (!IsAliveEnemy(I, U.LastAttacker[I]))
			{
				U.LastAttacker[I] = NoEntity;
			}

			auto WithinRaw = [&](EntityId Other, Fix Radius)
			{
				const int32_t J = IndexOf(Other);
				return J >= 0 && DistSqRaw(U.Pos[I], U.Pos[J]) <= RadiusSqRaw(Radius + DefOf(J).Radius);
			};

			switch (U.Order[I])
			{
			case OrderType::Attack:
				U.Engage[I] = U.OrderTarget[I];
				break;
			case OrderType::AttackMove:
				// a coleira mede de onde a perseguição começou: um alvo que recua não arrasta o tanque pelo mapa
				if (U.Engage[I] != NoEntity && DistSqRaw(U.Pos[I], U.EngageAnchor[I]) > RadiusSqRaw(D.Sight * LeashFraction))
				{
					U.Engage[I] = NoEntity;   // volta pro caminho do ataque andando
				}
				if (U.Engage[I] == NoEntity && bScan)
				{
					const EntityId Found = NearestEnemy(I, D.Sight);
					if (Found != NoEntity)
					{
						StartEngage(I, Found);
					}
				}
				if (U.Engage[I] == NoEntity && U.LastAttacker[I] != NoEntity && WithinRaw(U.LastAttacker[I], D.Sight * LeashFraction))
				{
					StartEngage(I, U.LastAttacker[I]);
				}
				break;
			case OrderType::Idle:
				// parado: atira no que entra no alcance; se levar tiro de longe (ou um vizinho levar), vai
				// atrás até a coleira e depois volta pra onde estava
				if (U.Engage[I] != NoEntity && DistSqRaw(U.Pos[I], U.EngageAnchor[I]) > RadiusSqRaw(D.Sight * LeashFraction))
				{
					const Vec2 Home = U.EngageAnchor[I];
					U.Engage[I] = NoEntity;
					U.LastAttacker[I] = NoEntity;
					GiveMove(I, Home, OrderType::Move);
					break;
				}
				if (U.Engage[I] == NoEntity && U.LastAttacker[I] != NoEntity && !WithinRaw(U.LastAttacker[I], D.Weapon.Range)
					&& WithinRaw(U.LastAttacker[I], D.Sight * LeashFraction))
				{
					StartEngage(I, U.LastAttacker[I]);
				}
				break;
			case OrderType::Move:
				U.Engage[I] = NoEntity;   // ordem de andar é andar: a torre atira de oportunidade
				break;
			}

			// torre: o alvo da perseguição se já está no alcance; senão o mais perto no alcance
			if (U.Engage[I] != NoEntity && WithinRaw(U.Engage[I], D.Weapon.Range))
			{
				U.FireTarget[I] = U.Engage[I];
			}
			else if (U.FireTarget[I] != NoEntity && (!IsAliveEnemy(I, U.FireTarget[I]) || !WithinRaw(U.FireTarget[I], D.Weapon.Range)
				|| !SeesUnit(I, IndexOf(U.FireTarget[I]))))
			{
				U.FireTarget[I] = NoEntity;
			}
			if (U.FireTarget[I] == NoEntity && (bScan || U.LastAttacker[I] != NoEntity))
			{
				U.FireTarget[I] = NearestEnemy(I, D.Weapon.Range);
			}
		}
	}

	// ---------------------------------------------------------------- caminho e movimento

	bool World::MovementGoal(int32_t I, Vec2& OutGoal, Fix& OutStopDistance) const
	{
		OutStopDistance = Fix::Zero();
		if (U.Engage[I] != NoEntity && U.Order[I] != OrderType::Move)
		{
			const int32_t J = IndexOf(U.Engage[I]);
			if (J >= 0)
			{
				const Fix Stop = DefOf(I).Weapon.Range * ChaseStopFraction + DefOf(J).Radius;
				if (DistSqRaw(U.Pos[I], U.Pos[J]) <= RadiusSqRaw(Stop))
				{
					return false;   // no alcance: fica e atira
				}
				OutGoal = U.Pos[J];
				OutStopDistance = Stop;
				return true;
			}
		}
		if (U.Order[I] == OrderType::Move || U.Order[I] == OrderType::AttackMove)
		{
			OutGoal = U.OrderPoint[I];
			return true;
		}
		return false;
	}

	void World::UpdatePaths()
	{
		const int32_t N = UnitCount();
		std::vector<uint8_t> Wants(size_t(N), 0);   // 0 nada · 1 refazer (o destino andou) · 2 ordem nova
		for (int32_t I = 0; I < N; ++I)
		{
			if (U.RepathWait[I] > 0)
			{
				--U.RepathWait[I];
			}
			Vec2 Goal;
			Fix Stop;
			if (!MovementGoal(I, Goal, Stop))
			{
				U.Path[I].clear();
				U.NeedsPath[I] = 0;
				continue;
			}
			// caminho vazio com o destino parado não refaz: senão quem chegou no fim de um caminho
			// parcial (alvo do outro lado do rio) buscava de novo pra sempre
			const bool bGoalShifted = DistSqRaw(Goal, U.PathGoal[I]) > RadiusSqRaw(RepathGoalShift);
			// sem caminho e ainda longe de onde quer chegar (o caminho acabou antes, um tanque na
			// frente): tenta de novo, no ritmo do RepathWait. Sem isso ela ficava parada com a ordem
			// presa pra sempre (impasse do teste t3, 08/10)
			const bool bStranded = U.Path[I].empty() && DistSqRaw(U.Pos[I], Goal) > RadiusSqRaw(Max(ArriveDistance, Stop) + MapData.CellSize);
			if (U.NeedsPath[I])
			{
				Wants[size_t(I)] = 2;
			}
			else if ((bGoalShifted || bStranded) && U.RepathWait[I] == 0)
			{
				Wants[size_t(I)] = 1;
			}
		}
		// a cota começa depois de quem foi atendido por último e dá a volta (ninguém fica sempre
		// por último); primeiro as ordens novas, depois as perseguições
		int32_t Start = 0;
		while (Start < N && U.Id[Start] <= PathCursor)
		{
			++Start;
		}
		int32_t Budget = MaxPathsPerTick;
		for (uint8_t Level = 2; Level >= 1 && Budget > 0; --Level)
		{
			for (int32_t K = 0; K < N && Budget > 0; ++K)
			{
				const int32_t I = (Start + K) % N;
				if (Wants[size_t(I)] != Level)
				{
					continue;
				}
				Vec2 Goal;
				Fix Stop;
				MovementGoal(I, Goal, Stop);
				--Budget;
				bool bReached = true;
				const int32_t Clearance = MapData.ClearanceFor(DefOf(I).PathRadius);
				if (!Paths.FindPath(MapData, U.Pos[I], Goal, Clearance, U.Path[I], &bReached))
				{
					U.Path[I].clear();
				}
				// o caminho novo não sai do lugar: daqui não se chega lá
				const bool bNoProgress = U.Path[I].empty()
					|| (DistSqRaw(U.Path[I].back(), U.Pos[I]) <= RadiusSqRaw(MapData.CellSize * 2)
						&& DistSqRaw(U.Pos[I], Goal) > RadiusSqRaw(Max(ArriveDistance, Stop) + MapData.CellSize * 2));
				if ((!bReached || bNoProgress) && U.Engage[I] != NoEntity && U.Order[I] != OrderType::Attack)
				{
					// não dá pra chegar no alvo (água, penhasco): solta ele por 3 s e segue a ordem
					U.Ignore[I] = U.Engage[I];
					U.IgnoreUntil[I] = CurrentTick + TickNumber(3 * TicksPerSecond);
					U.Engage[I] = NoEntity;
					U.Path[I].clear();
				}
				else if (bNoProgress)
				{
					// ordem que não leva a lugar nenhum: desiste e fica parada (a IA ou o jogador mandam de novo)
					U.Order[I] = OrderType::Idle;
					U.OrderTarget[I] = NoEntity;
					U.Engage[I] = NoEntity;
					U.Path[I].clear();
				}
				U.PathIndex[I] = 0;
				U.PathGoal[I] = Goal;
				U.NeedsPath[I] = 0;
				U.RepathWait[I] = RepathTicks;
				PathCursor = U.Id[I];
			}
		}
	}

	void World::UpdateMovement()
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			Vec2 Goal;
			Fix Stop;
			const bool bWantsToMove = MovementGoal(I, Goal, Stop) && !U.Path[I].empty();
			Fix TargetSpeed = Fix::Zero();
			Angle Desired = U.Hull[I];

			if (bWantsToMove)
			{
				std::vector<Vec2>& Path = U.Path[I];
				const Fix CellSize = MapData.CellSize;
				const int32_t PathClearance = MapData.ClearanceFor(D.PathRadius);
				while (U.PathIndex[I] + 1 < Path.size()
					&& (DistSqRaw(U.Pos[I], Path[U.PathIndex[I]]) <= RadiusSqRaw(ArriveDistance)
						|| (DistSqRaw(U.Pos[I], Path[U.PathIndex[I]]) <= RadiusSqRaw(CellSize)
							&& Pathfinder::HasLineOfMovement(MapData, MapData.CellOf(U.Pos[I]), MapData.CellOf(Path[U.PathIndex[I] + 1]), PathClearance))))
				{
					++U.PathIndex[I];
				}
				const Vec2 Waypoint = Path[U.PathIndex[I]];
				const bool bLast = U.PathIndex[I] + 1 == Path.size();
				const Fix ToWaypoint = Distance(U.Pos[I], Waypoint);

				if (bLast && ToWaypoint <= Max(ArriveDistance, Stop))
				{
					// chegou
					if (U.Engage[I] == NoEntity && (U.Order[I] == OrderType::Move || U.Order[I] == OrderType::AttackMove))
					{
						U.Order[I] = OrderType::Idle;
					}
					Path.clear();
				}
				else
				{
					Desired = (Waypoint - U.Pos[I]).Heading();
					const int32_t Off = AbsInt(U.Hull[I].DeltaTo(Desired));
					TargetSpeed = Off >= TurnInPlaceBam
						? Fix::Zero()
						: D.MaxSpeed * Fix::FromRatio(TurnInPlaceBam - Off, TurnInPlaceBam);
					if (bLast)
					{
						// freia a tempo: v² = 2·a·d
						const Fix Remaining = Max(ToWaypoint - Stop, Fix::Zero());
						TargetSpeed = Min(TargetSpeed, Sqrt(D.Brake * Remaining * 2));
					}
				}
			}

			U.Hull[I] = U.Hull[I].RotatedTowards(Desired, D.TurnBam);
			if (U.Speed[I] < TargetSpeed)
			{
				U.Speed[I] = Min(U.Speed[I] + D.Accel, TargetSpeed);
			}
			else
			{
				U.Speed[I] = Max(U.Speed[I] - D.Brake, TargetSpeed);
			}
			if (U.Speed[I] <= Fix::Zero())
			{
				U.Speed[I] = Fix::Zero();
			}
			else
			{
				const Vec2 Advance = Vec2::FromAngle(U.Hull[I]) * U.Speed[I];
				const int32_t Clearance = MapData.ClearanceFor(D.PathRadius);
				const Cell Here = MapData.CellOf(U.Pos[I]);
				const bool bStuckInside = !MapData.IsWalkable(Here.X, Here.Y, Clearance);
				auto Walkable = [&](Vec2 P)
				{
					const Cell C = MapData.CellOf(P);
					return bStuckInside || MapData.IsWalkable(C.X, C.Y, Clearance);
				};
				const Vec2 Pos = U.Pos[I];
				if (Walkable(Pos + Advance))
				{
					U.Pos[I] = Pos + Advance;
				}
				else if (Walkable(Pos + Vec2::Make(Advance.X, Fix::Zero())))
				{
					U.Pos[I] = Pos + Vec2::Make(Advance.X, Fix::Zero());
				}
				else if (Walkable(Pos + Vec2::Make(Fix::Zero(), Advance.Y)))
				{
					U.Pos[I] = Pos + Vec2::Make(Fix::Zero(), Advance.Y);
				}
				else
				{
					U.Speed[I] = Fix::Zero();
					U.NeedsPath[I] = U.RepathWait[I] == 0 ? 1 : U.NeedsPath[I];
				}
			}

			// empacou? (preso atrás de outro tanque, na quina)
			if (bWantsToMove)
			{
				if (++U.StuckTicks[I] >= StuckWindowTicks)
				{
					const bool bMoved = DistSqRaw(U.Pos[I], U.StuckAnchor[I]) > RadiusSqRaw(StuckMinTravel);
					if (!bMoved)
					{
						const bool bNear = DistSqRaw(U.Pos[I], Goal) <= RadiusSqRaw(D.Radius * 3 + Stop);
						if (bNear && U.Engage[I] == NoEntity && U.Order[I] != OrderType::Attack)
						{
							U.Order[I] = OrderType::Idle;   // perto o bastante: o grupo encostou
							U.Path[I].clear();
						}
						else
						{
							U.NeedsPath[I] = 1;
						}
					}
					U.StuckTicks[I] = 0;
					U.StuckAnchor[I] = U.Pos[I];
				}
			}
			else
			{
				U.StuckTicks[I] = 0;
				U.StuckAnchor[I] = U.Pos[I];
			}
		}
	}

	void World::SeparateUnits()
	{
		// Unidades não se atravessam: cada par que se sobrepõe se afasta. Quem está
		// parado cede mais que quem anda. Os empurrões somam e só depois se aplicam
		// (não depende da ordem de visita).
		std::vector<Vec2> Push(U.Id.size(), Vec2::Zero());
		std::vector<Fix> Deepest(U.Id.size(), Fix::Zero());   // a maior sobreposição de cada um
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const Fix RI = DefOf(I).Radius;
			const bool bMovingI = U.Speed[I] > Fix::Zero();
			ForEachNear(U.Pos[I], RI + MaxUnitRadius, [&](int32_t J)
			{
				if (J <= I)
				{
					return;
				}
				const Fix Reach = RI + DefOf(J).Radius;
				const Vec2 Delta = U.Pos[I] - U.Pos[J];
				if (Delta.LengthSqRaw() >= RadiusSqRaw(Reach))
				{
					return;
				}
				const Fix Dist = Delta.Length();
				const Vec2 Dir = Dist.Raw > 0
					? Delta / Dist
					: Vec2::FromAngle(Angle::FromBam(int32_t((U.Id[I] * 2654435761u) >> 16)));
				const Fix Overlap = Reach - Dist;
				const bool bMovingJ = U.Speed[J] > Fix::Zero();
				Fix ShareI = Fix::Half();
				if (bMovingI != bMovingJ)
				{
					ShareI = bMovingI ? Fix::FromRatio(1, 4) : Fix::FromRatio(3, 4);
				}
				Push[size_t(I)] += Dir * (Overlap * ShareI);
				Push[size_t(J)] -= Dir * (Overlap * (Fix::One() - ShareI));
				Deepest[size_t(I)] = Max(Deepest[size_t(I)], Overlap);
				Deepest[size_t(J)] = Max(Deepest[size_t(J)], Overlap);
			});
		}
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			Vec2 P = Push[size_t(I)];
			if (P == Vec2::Zero())
			{
				continue;
			}
			// encosto leve: empurrão suave (o tanque parado não sai deslizando como disco de hóquei);
			// um dentro do outro (vários chegando no mesmo ponto): separa rápido (teste t3, 08/10)
			const bool bDeep = Deepest[size_t(I)] > DefOf(I).Radius / 2;
			const Fix MaxPush = bDeep ? DefOf(I).Radius / 4 : Min(DefOf(I).Radius / 4, MaxPushPerTick);
			if (P.LengthSqRaw() > RadiusSqRaw(MaxPush))
			{
				P = P.Normalized() * MaxPush;
			}
			const Vec2 Next = U.Pos[I] + P;
			const Cell C = MapData.CellOf(Next);
			if (MapData.IsWalkable(C.X, C.Y, MapData.ClearanceFor(DefOf(I).PathRadius)))
			{
				U.Pos[I] = Next;
			}
		}
	}

	// ---------------------------------------------------------------- torre e armas

	void World::UpdateTurrets()
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			int32_t DesiredRel = 0;
			int32_t Rate = D.TurretTurnBam;
			const int32_t T = IndexOf(U.FireTarget[I]);
			if (T >= 0)
			{
				const Angle Bearing = (U.Pos[T] - U.Pos[I]).Heading();
				DesiredRel = U.Hull[I].DeltaTo(Bearing);
				U.TurretIdle[I] = 0;
			}
			else
			{
				// sem alvo: depois de um tempo a torre volta pra frente, a meia velocidade (ZH)
				if (U.TurretIdle[I] < D.TurretIdleReturnTicks)
				{
					++U.TurretIdle[I];
					continue;
				}
				Rate = Rate / 2 > 0 ? Rate / 2 : 1;
			}
			const int32_t Diff = SignedBam(DesiredRel - U.TurretRel[I]);
			const int32_t Turn = Diff > Rate ? Rate : (Diff < -Rate ? -Rate : Diff);
			U.TurretRel[I] = SignedBam(U.TurretRel[I] + Turn);
		}
	}

	void World::UpdateProjectiles()
	{
		std::vector<Projectile> Flying;
		Flying.reserve(Projectiles.size());
		for (Projectile& P : Projectiles)
		{
			const int32_t T = IndexOf(P.Target);
			if (T >= 0)
			{
				P.Aim = U.Pos[T];
			}
			const Fix Remaining = Distance(P.Pos, P.Aim);
			if (P.Step.Raw == 0 || Remaining <= P.Step)
			{
				if (T >= 0)
				{
					PendingDamage.push_back({ P.Shooter, P.Target, P.Damage, P.Type });
				}
				Events.push_back({ EventType::ProjectileHit, P.Shooter, P.Target, P.Aim, 0 });
				continue;
			}
			P.Pos += (P.Aim - P.Pos).Normalized() * P.Step;
			Flying.push_back(P);
		}
		Projectiles.swap(Flying);
	}

	void World::UpdateWeapons()
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Cooldown[I] > 0)
			{
				--U.Cooldown[I];
			}
			const int32_t T = IndexOf(U.FireTarget[I]);
			if (T < 0 || U.Cooldown[I] > 0)
			{
				continue;
			}
			const WeaponDef& W = DefOf(I).Weapon;
			if (DistSqRaw(U.Pos[I], U.Pos[T]) > RadiusSqRaw(W.Range + DefOf(T).Radius))
			{
				continue;
			}
			const Angle Bearing = (U.Pos[T] - U.Pos[I]).Heading();
			if (AbsInt(TurretWorld(I).DeltaTo(Bearing)) > W.AimToleranceBam)
			{
				continue;   // a torre ainda está girando: só atira alinhada (Vanguard ADR-0031)
			}
			U.Cooldown[I] = W.ReloadTicks;
			Projectile P;
			P.Shooter = U.Id[I];
			P.Target = U.Id[T];
			P.Pos = U.Pos[I] + Vec2::FromAngle(TurretWorld(I)) * DefOf(I).Radius;
			P.Aim = U.Pos[T];
			P.Step = W.ProjectileStep;
			P.Damage = W.Damage;
			P.Type = W.Type;
			Projectiles.push_back(P);
			Events.push_back({ EventType::WeaponFired, U.Id[I], U.Id[T], U.Pos[T], 0 });
		}
	}

	// ---------------------------------------------------------------- dano, mortes, vitória

	void World::ResolveDamage()
	{
		// Duas fases: todos os acertos do passo valem, mesmo de quem morreu no mesmo passo
		// (dois tanques que se matam juntos morrem juntos, sem depender da ordem).
		for (const DamageIntent& H : PendingDamage)
		{
			const int32_t T = IndexOf(H.Target);
			if (T < 0)
			{
				continue;
			}
			const int32_t Amount = H.Amount * RulesData.Armor.Get(DefOf(T).Armor, H.Type) / 100;
			if (Amount <= 0)
			{
				continue;
			}
			U.Hp[T] -= Amount;
			if (H.Source != NoEntity)
			{
				U.LastAttacker[T] = H.Source;
				const int32_t Shooter = IndexOf(H.Source);
				if (bFogOfWar && Shooter >= 0 && U.LookCell[Shooter] >= 0)
				{
					// quem atira de fora da visão aparece 1 s (o clarão denuncia), pra reação ter alvo
					AddLingering(Players[U.Owner[T]].Team, U.LookCell[Shooter], ShotRevealCells, int64_t(ShotRevealCells) * ShotRevealCells, TickNumber(VisionLingerTicks));
				}
				// os aliados parados perto escutam o tiro e vão atrás de quem atirou (o resto fica assistindo)
				if (IndexOf(H.Source) >= 0)
				{
					ForEachNear(U.Pos[T], HelpRadius + MaxUnitRadius, [&](int32_t J)
					{
						if (J != T && U.Owner[J] == U.Owner[T] && U.Order[J] == OrderType::Idle && U.Engage[J] == NoEntity
							&& U.LastAttacker[J] == NoEntity && DistSqRaw(U.Pos[J], U.Pos[T]) <= RadiusSqRaw(HelpRadius))
						{
							U.LastAttacker[J] = H.Source;
						}
					});
				}
			}
			Events.push_back({ EventType::UnitDamaged, H.Source, H.Target, U.Pos[T], Amount });
		}
		PendingDamage.clear();
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Hp[I] <= 0)
			{
				Events.push_back({ EventType::UnitDied, U.LastAttacker[I], U.Id[I], U.Pos[I], U.Owner[I] });
			}
		}
	}

	void World::RemoveDead()
	{
		std::vector<uint8_t> Keep(U.Id.size());
		bool bAnyDead = false;
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			Keep[I] = U.Hp[I] > 0 ? 1 : 0;
			bAnyDead |= Keep[I] == 0;
		}
		if (!bAnyDead)
		{
			return;
		}
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			if (!Keep[I])
			{
				IdToIndex[U.Id[I]] = -1;
				if (U.LookCell[I] >= 0)
				{
					// o disco já está carimbado: passa pra lista e sai quando o tempo acabar
					LingeringLook L;
					L.Team = Players[U.Owner[I]].Team;
					L.Cell = U.LookCell[I];
					SightCells(U.Def[I], L.RadiusCells, L.RadiusSq);
					L.Until = CurrentTick + TickNumber(VisionLingerTicks);
					Lingering.push_back(L);
				}
			}
		}
		Compact(U.Id, Keep); Compact(U.Owner, Keep); Compact(U.Def, Keep); Compact(U.Pos, Keep);
		Compact(U.Hull, Keep); Compact(U.TurretRel, Keep); Compact(U.Speed, Keep); Compact(U.Hp, Keep);
		Compact(U.Cooldown, Keep); Compact(U.Order, Keep); Compact(U.OrderPoint, Keep);
		Compact(U.OrderTarget, Keep); Compact(U.Engage, Keep); Compact(U.FireTarget, Keep);
		Compact(U.LastAttacker, Keep); Compact(U.TurretIdle, Keep); Compact(U.Path, Keep);
		Compact(U.PathIndex, Keep); Compact(U.PathGoal, Keep); Compact(U.NeedsPath, Keep);
		Compact(U.RepathWait, Keep); Compact(U.StuckTicks, Keep); Compact(U.StuckAnchor, Keep);
		Compact(U.EngageAnchor, Keep); Compact(U.Ignore, Keep); Compact(U.IgnoreUntil, Keep);
		Compact(U.LookCell, Keep);
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			IdToIndex[U.Id[I]] = int32_t(I);
		}
		for (PlayerState& P : Players)
		{
			P.Group.erase(std::remove_if(P.Group.begin(), P.Group.end(),
				[this](EntityId Id) { return IndexOf(Id) < 0; }), P.Group.end());
		}
	}

	// ---------------------------------------------------------------- visão

	void World::SightCells(uint16_t Def, int32_t& OutRadius, int64_t& OutRadiusSq) const
	{
		// revelar vai além de mirar (o ZH separa ShroudClearingRange de VisionRange): o alvo
		// automático mede até Sight + o raio do alvo (39,5 + 3,6 m), e a grade de 4 m arredonda as
		// duas pontas (até 2,8 m cada). O disco pega as células com o centro a até
		// Sight + maior raio + 1 célula: o tanque nunca escolhe sozinho um alvo que o time não vê
		const Fix RadiusCells = (RulesData.Units[Def].Sight + MaxUnitRadius) / MapData.CellSize + Fix::One();
		OutRadius = int32_t(RadiusCells.Floor()) + 1;
		OutRadiusSq = (RadiusCells * RadiusCells).Floor();
	}

	void World::LookAt(uint8_t Team, uint16_t Def, int32_t Cell, int32_t Delta)
	{
		if (Cell < 0)
		{
			return;
		}
		int32_t Radius = 0;
		int64_t RadiusSq = 0;
		SightCells(Def, Radius, RadiusSq);
		Vision.Look(Team, Cell, Radius, RadiusSq, Delta);
	}

	void World::AddLingering(uint8_t Team, int32_t Cell, int32_t RadiusCells, int64_t RadiusSq, TickNumber Ticks)
	{
		if (Cell < 0)
		{
			return;
		}
		Vision.Look(Team, Cell, RadiusCells, RadiusSq, +1);
		LingeringLook L;
		L.Team = Team;
		L.Cell = Cell;
		L.RadiusCells = RadiusCells;
		L.RadiusSq = RadiusSq;
		L.Until = CurrentTick + Ticks;
		Lingering.push_back(L);
	}

	bool World::SeesUnit(int32_t Self, int32_t J) const
	{
		if (!bFogOfWar)
		{
			return true;
		}
		if (Self < 0 || J < 0)
		{
			return false;
		}
		return Vision.IsVisible(Players[U.Owner[Self]].Team, U.LookCell[J]);
	}

	bool World::VisionSelfCheck() const
	{
		VisionGrid Fresh;
		Fresh.Init(Vision.Width(), Vision.Height(), Vision.Teams());
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			int32_t Radius = 0;
			int64_t RadiusSq = 0;
			SightCells(U.Def[I], Radius, RadiusSq);
			Fresh.Look(Players[U.Owner[I]].Team, U.LookCell[I], Radius, RadiusSq, +1);
		}
		for (const LingeringLook& L : Lingering)
		{
			Fresh.Look(L.Team, L.Cell, L.RadiusCells, L.RadiusSq, +1);
		}
		return Fresh.SameCounts(Vision);
	}

	void World::UpdateVision()
	{
		// os olhos de quem morreu saem quando o tempo deles acaba (em ordem de morte)
		size_t Kept = 0;
		for (size_t K = 0; K < Lingering.size(); ++K)
		{
			if (Lingering[K].Until <= CurrentTick)
			{
				Vision.Look(Lingering[K].Team, Lingering[K].Cell, Lingering[K].RadiusCells, Lingering[K].RadiusSq, -1);
			}
			else
			{
				Lingering[Kept++] = Lingering[K];
			}
		}
		Lingering.resize(Kept);
		// quem mudou de célula carimba o disco novo; o velho fica 1 s e sai pela lista (o ZH faz
		// igual: UnlookPersistDuration de 30 quadros), senão quem anda na borda da visão pisca
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			const Cell C = MapData.CellOf(U.Pos[I]);
			const int32_t Now = MapData.InBounds(C.X, C.Y) ? MapData.Index(C.X, C.Y) : -1;
			if (Now == U.LookCell[I])
			{
				continue;
			}
			const uint8_t Team = Players[U.Owner[I]].Team;
			LookAt(Team, U.Def[I], Now, +1);
			if (U.LookCell[I] >= 0)
			{
				LingeringLook L;
				L.Team = Team;
				L.Cell = U.LookCell[I];
				SightCells(U.Def[I], L.RadiusCells, L.RadiusSq);
				L.Until = CurrentTick + TickNumber(VisionLingerTicks);
				Lingering.push_back(L);
			}
			U.LookCell[I] = Now;
		}
	}

	bool World::IsCellVisible(uint8_t Team, int32_t X, int32_t Y) const
	{
		return MapData.InBounds(X, Y) && Vision.IsVisible(Team, MapData.Index(X, Y));
	}

	bool World::IsCellExplored(uint8_t Team, int32_t X, int32_t Y) const
	{
		return MapData.InBounds(X, Y) && Vision.IsExplored(Team, MapData.Index(X, Y));
	}

	bool World::IsVisibleTo(PlayerIndex P, Vec2 Pos) const
	{
		if (!bFogOfWar)
		{
			return true;
		}
		if (P >= Players.size())
		{
			return false;
		}
		const Cell C = MapData.CellOf(Pos);
		return IsCellVisible(Players[P].Team, C.X, C.Y);
	}

	void World::CopyVisibility(uint8_t Team, std::vector<uint8_t>& Out) const
	{
		const int32_t N = MapData.Width * MapData.Height;
		Out.assign(size_t(N), 0);
		for (int32_t I = 0; I < N; ++I)
		{
			Out[size_t(I)] = Vision.IsVisible(Team, I) ? 2 : (Vision.IsExplored(Team, I) ? 1 : 0);
		}
	}

	void World::CheckVictory()
	{
		std::vector<int32_t> Alive(Players.size(), 0);
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			++Alive[U.Owner[I]];
		}
		for (size_t P = 0; P < Players.size(); ++P)
		{
			if (!Players[P].bDefeated && Alive[P] == 0)
			{
				Players[P].bDefeated = true;
				Events.push_back({ EventType::PlayerDefeated, NoEntity, NoEntity, Vec2::Zero(), int32_t(P) });
			}
		}
		std::vector<uint8_t> TeamsAlive;
		for (const PlayerState& P : Players)
		{
			if (!P.bDefeated && std::find(TeamsAlive.begin(), TeamsAlive.end(), P.Team) == TeamsAlive.end())
			{
				TeamsAlive.push_back(P.Team);
			}
		}
		const int32_t TeamsLeft = int32_t(TeamsAlive.size());
		const int32_t LastTeam = TeamsLeft == 1 ? int32_t(TeamsAlive[0]) : -1;
		if (TeamsLeft <= 1)
		{
			bMatchOver = true;
			Winner = TeamsLeft == 1 ? LastTeam : -1;
			MatchEndTick = CurrentTick;
			Events.push_back({ EventType::MatchOver, NoEntity, NoEntity, Vec2::Zero(), Winner });
		}
	}

	// ---------------------------------------------------------------- impressão digital

	uint64_t World::StateHash() const
	{
		return CachedHash;
	}

	uint64_t World::ComputeHash() const
	{
		Hasher H;
		H.Add(CurrentTick);
		H.Add(Random.StateForHash());
		H.Add(NextId);
		H.Add(bMatchOver);
		H.Add(Winner);
		H.Add(MatchEndTick);
		H.Add(PathCursor);
		H.Add(bFogOfWar);
		for (const PlayerState& P : Players)
		{
			H.Add(P.Team);
			H.Add(P.bDefeated);
			H.Add(uint64_t(P.Group.size()));
			for (const EntityId Id : P.Group)
			{
				H.Add(Id);
			}
		}
		H.Add(uint64_t(U.Id.size()));
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			H.Add(U.Id[I]); H.Add(U.Owner[I]); H.Add(U.Def[I]); H.Add(U.Pos[I]); H.Add(U.Hull[I]);
			H.Add(U.TurretRel[I]); H.Add(U.Speed[I]); H.Add(U.Hp[I]); H.Add(U.Cooldown[I]);
			H.Add(uint8_t(U.Order[I])); H.Add(U.OrderPoint[I]); H.Add(U.OrderTarget[I]); H.Add(U.Engage[I]);
			H.Add(U.FireTarget[I]); H.Add(U.LastAttacker[I]); H.Add(U.TurretIdle[I]);
			H.Add(uint64_t(U.Path[I].size()));
			for (const Vec2& P : U.Path[I])
			{
				H.Add(P);
			}
			H.Add(U.PathIndex[I]); H.Add(U.PathGoal[I]); H.Add(U.NeedsPath[I]); H.Add(U.RepathWait[I]);
			H.Add(U.StuckTicks[I]); H.Add(U.StuckAnchor[I]);
			H.Add(U.EngageAnchor[I]); H.Add(U.Ignore[I]); H.Add(U.IgnoreUntil[I]);
			H.Add(U.LookCell[I]);
		}
		H.Add(uint64_t(Lingering.size()));
		for (const LingeringLook& L : Lingering)
		{
			H.Add(L.Team); H.Add(L.Cell); H.Add(L.RadiusCells); H.Add(L.RadiusSq); H.Add(L.Until);
		}
		H.Add(uint64_t(Projectiles.size()));
		for (const Projectile& P : Projectiles)
		{
			H.Add(P.Shooter); H.Add(P.Target); H.Add(P.Pos); H.Add(P.Aim); H.Add(P.Step);
			H.Add(P.Damage); H.Add(uint8_t(P.Type));
		}
		return H.Value();
	}
}
