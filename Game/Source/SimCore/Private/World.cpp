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
		// as carcaças (13/10): o casco é dois círculos no eixo, a ±0,45 R e
		// de raio 0,6 R (~2,1 R × 1,2 R, a planta do tanque); ela pesa o mesmo que o vivo do mesmo tipo (a massa ∝
		// raio²), então o tanque que empurra anda à metade da velocidade (o Felipe: "ela segura o tanque também"); a
		// batida nunca é reta (±9° no empurrão, pelo par), e a carcaça gira e escorrega pro lado em vez de ir na frente
		// do tanque o caminho inteiro (o 1º teste: 60 m empurrada de frente); anda até R/4 e gira até 2° por passo;
		// travada, espera meio segundo pra tentar de novo.
		// (22/10) os tetos, pra morte em massa (o exército inteiro de quem perde): passou de 40 sólidas, a mais velha com
		// pelo menos 12 s começa a afundar já; passou de 64 no total (afundando inclusive), a mais velha sai na hora. Sem
		// eles o carcaça × carcaça (N²) e o desenho cresciam sem limite; na batalha comum (~20 mortes por minuto, ~55 s de
		// vida cada) ficam ~18 no mapa e os tetos nem entram
		constexpr int32_t WreckSolidCap = 40;
		constexpr size_t WreckHardCap = 64;
		constexpr TickNumber WreckMinForcedAgeTicks = TickNumber(12 * TicksPerSecond);
		constexpr Fix WreckSubOffset = Fix::FromRatio(9, 20);
		constexpr Fix WreckSubRadius = Fix::FromRatio(3, 5);
		constexpr int64_t WreckMaxTurnBam = 364;
		constexpr uint8_t WreckStuckTicks = 15;
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
		TurretRel.resize(N); Speed.resize(N); Hp.resize(N); Cooldown.resize(N); Clip.resize(N); Order.resize(N);
		OrderPoint.resize(N); OrderTarget.resize(N); ForcedAttack.resize(N); Engage.resize(N); FireTarget.resize(N);
		LastAttacker.resize(N); TurretIdle.resize(N); Path.resize(N); PathIndex.resize(N);
		PathGoal.resize(N); NeedsPath.resize(N); RepathWait.resize(N); StuckTicks.resize(N);
		StuckAnchor.resize(N); EngageAnchor.resize(N); Ignore.resize(N); IgnoreUntil.resize(N);
		LookCell.resize(N); BuildLeft.resize(N); Builder.resize(N); Queue.resize(N); QueueLeft.resize(N);
		Rally.resize(N); HasRally.resize(N); Task.resize(N); TaskTarget.resize(N); TaskDock.resize(N);
		TaskTimer.resize(N); Cargo.resize(N); AutoGather.resize(N); Blocking.resize(N); TaskPoint.resize(N);
		ArriveFacing.resize(N); HasArriveFacing.resize(N); SpeedCap.resize(N); IdleTurret.resize(N);
		CrushSlowLeft.resize(N); CrushSlowTotal.resize(N); CrushSlowFloor.resize(N);
		IncomeLeft.resize(N); IncomeCount.resize(N); UnloadLeft.resize(N);   // (v4) a zona de lançamento
		SellLeft.resize(N); RepairAcc.resize(N); AimTicks.resize(N); AimIdle.resize(N);   // (v4, 23/10) vender, consertar, a mira
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
		H.Add(uint64_t(Docks.size()));
		for (const SupplyDockSetup& D : Docks)
		{
			H.Add(D.Pos);
			H.Add(D.HalfX);
			H.Add(D.HalfY);
			H.Add(D.Boxes);
		}
		H.Add(uint64_t(Props.size()));
		for (const PropSetup& P : Props)
		{
			H.Add(uint8_t(P.Kind)); H.Add(P.Pos); H.Add(P.Facing); H.Add(P.HalfLength); H.Add(P.HalfWidth); H.Add(P.Height); H.Add(P.CrushableLevel);
		}
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
			H.Add(P.StartRank);   // (v4)
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
		MaxBodyRadius = Fix::Zero();
		for (const UnitDef& D : RulesData.Units)
		{
			if (!D.IsStructure())
			{
				MaxUnitRadius = Max(MaxUnitRadius, D.Radius);
			}
			MaxBodyRadius = Max(MaxBodyRadius, D.Radius);
		}
		int32_t Teams = 0;
		for (const PlayerSetup& P : Setup.Players)
		{
			if (P.Team >= MaxPlayers || P.StartRank > MaxRank)
			{
				return false;   // o time é índice da grade de visão: nunca maior que o número de vagas; (v4) a patente vai até 5
			}
			PlayerState S;
			S.Team = P.Team;
			S.Money = RulesData.Economy.StartingMoney;
			// (v4) a patente de largada (as cenas de teste) começa com o XP do limiar dela
			S.Rank = uint8_t(std::clamp<int32_t>(P.StartRank, 1, MaxRank));
			S.XpMilli = int64_t(RulesData.Economy.RankXp[S.Rank - 1]) * 1000;
			Players.push_back(S);
			Teams = std::max(Teams, int32_t(P.Team) + 1);
		}
		Vision.Init(MapData.Width, MapData.Height, Teams);
		bFogOfWar = Setup.bFogOfWar;
		bAllowDebug = Setup.bAllowDebugCommands;
		// os depósitos do mapa entram na grade como terreno (nunca saem); os prédios iniciais
		// entram prontos, antes das unidades: quem nasceria em cima deles vai pro lado
		for (const SupplyDockSetup& D : Setup.Docks)
		{
			SupplyDock Dock;
			Dock.Pos = D.Pos;
			Dock.HalfX = D.HalfX;
			Dock.HalfY = D.HalfY;
			Dock.Boxes = D.Boxes;
			Dock.StartBoxes = D.Boxes;
			Docks.push_back(Dock);
			const Vec2 Local = D.Pos - MapData.Origin;
			const int32_t X0 = int32_t(((Local.X - D.HalfX) / MapData.CellSize).Floor());
			const int32_t Y0 = int32_t(((Local.Y - D.HalfY) / MapData.CellSize).Floor());
			const int32_t X1 = int32_t(((Local.X + D.HalfX) / MapData.CellSize).Floor()) + 1;
			const int32_t Y1 = int32_t(((Local.Y + D.HalfY) / MapData.CellSize).Floor()) + 1;
			for (int32_t Y = Y0; Y < Y1; ++Y)
			{
				for (int32_t X = X0; X < X1; ++X)
				{
					if (MapData.InBounds(X, Y))
					{
						MapData.Pass[size_t(MapData.Index(X, Y))] = uint8_t(CellPass::Blocked);
					}
				}
			}
		}
		// (16/10) os carros e as árvores: a célula deles vira esmagável (antes da passagem do terreno: o prédio que cai
		// devolve a célula como ela está)
		InitProps(Setup.Props);
		TerrainPass = MapData.Pass;
		MapData.ComputeClearance();
		for (SupplyDock& Dock : Docks)
		{
			BuildDockField(Dock);
		}
		for (size_t P = 0; P < Setup.Players.size(); ++P)
		{
			for (const StartingUnit& S : Setup.Players[P].Units)
			{
				const int32_t DefIndex = RulesData.FindUnit(S.Key);
				if (DefIndex >= 0 && RulesData.Units[size_t(DefIndex)].IsStructure())
				{
					// o lado do prédio sai do giro de largada, arredondado pro quarto de volta mais perto
					const int32_t Dir = int32_t(((S.Facing.Bam + Angle::QuarterTurn / 2) & 0xFFFF) / Angle::QuarterTurn) & 3;
					PlaceStructure(PlayerIndex(P), uint16_t(DefIndex), Dir, ClampToMap(S.Pos), true);
				}
			}
		}
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
				if (RulesData.Units[size_t(DefIndex)].IsStructure())
				{
					continue;   // já está no mapa
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
			AddClearance(MobilityOfDef(D));   // (16/10) quem esmaga tem a região dele (passa no carro e na árvore)
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
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			Starts[U.Owner[I]] += U.Pos[I];
			StartCount[U.Owner[I]] += 1;
		}
		for (const Placement& Pl : Places)
		{
			const EntityId Id = Spawn(Pl.Owner, Pl.Def, Pl.Pos, Pl.Facing);
			U.AutoGather[size_t(IndexOf(Id))] = RulesData.Units[Pl.Def].SupplyBoxes > 0 ? 1 : 0;
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
		UpdatePower();
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (DefOf(I).bCountsForVictory && IsComplete(I))
			{
				Players[U.Owner[I]].bHasBase = true;
			}
		}
		bGridChanged = false;
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
		U.ArriveFacing[I] = Facing;
		U.HasArriveFacing[I] = 0;
		U.SpeedCap[I] = Fix::Zero();
		U.IdleTurret[I] = 0;
		U.CrushSlowLeft[I] = 0;
		U.CrushSlowTotal[I] = 0;
		U.CrushSlowFloor[I] = 1000;
		U.TurretRel[I] = 0;
		U.Speed[I] = Fix::Zero();
		U.Hp[I] = D.MaxHp;
		U.Cooldown[I] = 0;
		U.Clip[I] = D.Weapon.ClipSize;
		U.Order[I] = OrderType::Idle;
		U.OrderPoint[I] = Pos;
		U.OrderTarget[I] = NoEntity;
		U.ForcedAttack[I] = 0;
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
		U.BuildLeft[I] = 0;
		U.Builder[I] = NoEntity;
		U.Queue[I].clear();
		U.QueueLeft[I] = 0;
		U.Rally[I] = Pos;
		U.HasRally[I] = 0;
		U.Task[I] = UnitTask::None;
		U.TaskTarget[I] = NoEntity;
		U.TaskDock[I] = -1;
		U.TaskTimer[I] = 0;
		U.Cargo[I] = 0;
		U.AutoGather[I] = 0;
		U.Blocking[I] = 0;
		U.TaskPoint[I] = Pos;
		U.IncomeLeft[I] = 0;
		U.IncomeCount[I] = 0;
		U.UnloadLeft[I] = 0;
		U.SellLeft[I] = 0;
		U.RepairAcc[I] = 0;
		U.AimTicks[I] = 0;
		U.AimIdle[I] = 0;
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

	bool World::IsAliveUnit(EntityId Other) const
	{
		const int32_t J = IndexOf(Other);
		return J >= 0 && U.Hp[J] > 0;
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
		const UnitDef& D = DefOf(I);
		V.bStructure = D.IsStructure();
		V.BuildLeft = U.BuildLeft[I] > 0 ? (U.BuildLeft[I] + 999) / 1000 : 0;   // guardado em passos × 1000
		V.BuildTicks = D.BuildTicks;
		V.Dir = V.bStructure ? DirOf(I) : 0;
		V.QueueSize = int32_t(U.Queue[I].size());
		if (!U.Queue[I].empty())
		{
			const int64_t Total = int64_t(RulesData.Units[U.Queue[I].front()].BuildTicks) * 1000;
			V.QueueHead = U.Queue[I].front();
			V.QueuePermille = Total > 0 ? int32_t(1000 - int64_t(U.QueueLeft[I]) * 1000 / Total) : 1000;
		}
		V.bHasRally = U.HasRally[I] != 0;
		V.Rally = U.Rally[I];
		V.Task = U.Task[I];
		V.TaskTarget = U.TaskTarget[I];
		V.Cargo = U.Cargo[I];
		V.IncomeLeft = U.IncomeLeft[I];
		V.IncomeCount = U.IncomeCount[I];
		// (v4, 23/10) a venda e a mira antes do tiro
		V.SellLeft = std::max(U.SellLeft[I], 0);
		V.SellTicks = U.SellLeft[I] != 0 ? RulesData.Economy.SellTicks : 0;
		V.AimTicks = U.AimTicks[I];
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
		UpdatePower();
		UpdateConstruction();
		UpdateProduction();
		UpdateSupply();
		UpdateAirlift();
		UpdateRepair();   // (v4, 23/10) o conserto do Dozer e a venda
		UpdateSell();
		if (bGridChanged)
		{
			// prédio novo ou que caiu: os caminhos antigos podem cruzar a grade mudada
			for (int32_t I = 0; I < UnitCount(); ++I)
			{
				if (!U.Path[I].empty())
				{
					U.NeedsPath[I] = 1;
				}
			}
			bGridChanged = false;
		}
		BuildBins();
		UpdateTargets();
		UpdatePaths();
		PrePos = U.Pos;
		PreHull = U.Hull;
		UpdateMovement();
		BuildBins();
		SeparateUnits();
		AgeWrecks();
		PushWrecks();
		UpdateCrush();
		BuildBins();
		UpdateTurrets();
		UpdateProjectiles();
		UpdateWeapons();
		// (v4) o míssil do poder entra na lista no mesmo passo do WeaponFired dele, como o da Patriot
		UpdatePowers();
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
		if (C.Type == CommandType::Resign)
		{
			// render-se (13/10, N5 da rede; e a queda, que o anfitrião manda pela vaga de quem caiu): quem desiste
			// perde tudo, como no ZH (VictoryConditions::killPlayer); a vitória marca a derrota no fim deste passo
			for (int32_t I = 0; I < UnitCount(); ++I)
			{
				if (U.Owner[I] == C.Player)
				{
					U.Hp[I] = 0;
				}
			}
			return;
		}
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

		// prédio não anda: o grupo das ordens é só de unidades
		std::vector<int32_t> Members;
		for (const EntityId Id : P.Group)
		{
			const int32_t I = IndexOf(Id);
			if (I >= 0 && U.Owner[I] == C.Player && !DefOf(I).IsStructure())
			{
				Members.push_back(I);
			}
		}
		const bool bCombat = C.Type == CommandType::Attack || C.Type == CommandType::ForceAttack || C.Type == CommandType::AttackGround;
		if (C.Type >= CommandType::Produce && !bCombat)
		{
			if (C.Type == CommandType::UsePower)
			{
				ApplyPowerCommand(C);   // (v4) o poder do general não usa o grupo
				return;
			}
			ApplyEconomyCommand(C, Members);
			return;
		}
		if (bCombat)
		{
			// desarmado não ataca (o Dozer e o caminhão ficam onde estão)
			Members.erase(std::remove_if(Members.begin(), Members.end(), [this](int32_t I) { return !DefOf(I).HasWeapon(); }), Members.end());
		}
		if (C.Type == CommandType::Move || C.Type == CommandType::AttackMove)
		{
			// prédio não anda (14/10: a defesa de base selecionada junto com os tanques ficava com a ordem de andar)
			Members.erase(std::remove_if(Members.begin(), Members.end(), [this](int32_t I) { return DefOf(I).IsStructure(); }), Members.end());
		}
		if (Members.empty())
		{
			return;
		}
		if (C.Type == CommandType::Move || C.Type == CommandType::AttackMove || bCombat || C.Type == CommandType::Stop)
		{
			// ordem do jogador: o Dozer larga a obra (ela fica parada) ou o conserto e o caminhão para de coletar
			for (const int32_t I : Members)
			{
				if (U.Task[I] == UnitTask::Construct || U.Task[I] == UnitTask::Repair)
				{
					const int32_t S = IndexOf(U.TaskTarget[I]);
					if (S >= 0 && U.Builder[S] == U.Id[I])
					{
						U.Builder[S] = NoEntity;
					}
				}
				U.Task[I] = UnitTask::None;
				U.TaskTarget[I] = NoEntity;
				U.TaskDock[I] = -1;
				U.AutoGather[I] = 0;
			}
		}

		switch (C.Type)
		{
		case CommandType::Move:
		case CommandType::AttackMove:
		{
			const OrderType Order = C.Type == CommandType::Move ? OrderType::Move : OrderType::AttackMove;
			// em formação (F segurado, como no C&C): o desenho do grupo anda junto, no passo do mais lento;
			// sem a tecla, cada um vai na sua velocidade
			const bool bFormation = MoveFormationOf(C.Value) && Members.size() > 1;
			// o arraste do esquerdo (11/10, estudo AoE4, CoH, BAR): a linha de frente virada pra onde o jogador puxou
			const bool bDragFacing = MoveHasFacingOf(C.Value);
			const Angle DragFacing = Angle::FromBam(MoveFacingBamOf(C.Value));
			std::vector<Vec2> Slots;
			if (bDragFacing)
			{
				LineSlots(Members, ClampToMap(C.Point), DragFacing, Fix::FromInt(2 * MoveWidthHalfMOf(C.Value)), Slots);
			}
			else if (bFormation)
			{
				FormationSlots(Members, ClampToMap(C.Point), Slots);
			}
			else
			{
				GroupSlots(Members, ClampToMap(C.Point), Slots);
			}
			Fix Slowest = Fix::Zero();
			for (const int32_t M : Members)
			{
				if (!DefOf(M).IsStructure() && (Slowest == Fix::Zero() || DefOf(M).MaxSpeed < Slowest))
				{
					Slowest = DefOf(M).MaxSpeed;
				}
			}
			// o grupo chega virado pra direção da marcha, do centro do grupo pro ponto
			// (estudo de 11/10: CoH, AoE4, 0 A.D.; ordem curta, de menos de 3 células, não gira)
			Vec2 Center = Vec2::Zero();
			for (const int32_t M : Members)
			{
				Center += U.Pos[M] / Fix::FromInt(int64_t(Members.size()));
			}
			const bool bFace = bDragFacing || (!Members.empty() && DistSqRaw(Center, C.Point) > RadiusSqRaw(MapData.CellSize * 3));
			const Angle March = bDragFacing ? DragFacing : (C.Point - Center).Heading();
			for (size_t K = 0; K < Members.size(); ++K)
			{
				GiveMove(Members[K], Slots[K], Order);
				if (bFormation)
				{
					U.SpeedCap[Members[K]] = Slowest;
				}
				if (bFace && !DefOf(Members[K]).IsStructure())
				{
					U.ArriveFacing[Members[K]] = March;
					U.HasArriveFacing[Members[K]] = 1;
				}
			}
			if (bFace)
			{
				FanTurrets(Members, Slots, March);
			}
			break;
		}
		case CommandType::Attack:
		case CommandType::ForceAttack:
		{
			const bool bForced = C.Type == CommandType::ForceAttack;
			if (bForced ? !IsAliveUnit(C.Target) : !IsAliveEnemy(Members.front(), C.Target))
			{
				return;   // alvo morto, ou aliado sem o Ctrl: ordem inválida, ignora
			}

			for (const int32_t I : Members)
			{
				if (U.Id[I] == C.Target)
				{
					continue;   // ninguém atira em si mesmo
				}
				U.ForcedAttack[I] = bForced ? 1 : 0;
				U.Order[I] = OrderType::Attack;
				U.SpeedCap[I] = Fix::Zero();
				U.OrderTarget[I] = C.Target;
				U.Engage[I] = C.Target;
				U.Path[I].clear();
				U.NeedsPath[I] = 1;
				U.StuckTicks[I] = 0;
				U.StuckAnchor[I] = U.Pos[I];
			}
			break;
		}
		case CommandType::AttackGround:
			// o tiro forçado no chão (Ctrl, como no C&C): vai até o alcance e atira no ponto até mandarem outra coisa
			for (const int32_t I : Members)
			{
				U.Order[I] = OrderType::AttackGround;
				U.OrderPoint[I] = C.Point;
				U.OrderTarget[I] = NoEntity;
				U.ForcedAttack[I] = 0;
				U.Engage[I] = NoEntity;
				U.FireTarget[I] = NoEntity;
				U.SpeedCap[I] = Fix::Zero();
				U.Path[I].clear();
				U.NeedsPath[I] = 1;
				U.StuckTicks[I] = 0;
				U.StuckAnchor[I] = U.Pos[I];
			}
			break;
		case CommandType::Stop:
			for (const int32_t I : Members)
			{
				U.Order[I] = OrderType::Idle;
				U.OrderTarget[I] = NoEntity;
				U.Engage[I] = NoEntity;
				U.LastAttacker[I] = NoEntity;
				U.Path[I].clear();
				U.NeedsPath[I] = 0;
				U.SpeedCap[I] = Fix::Zero();
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
		U.HasArriveFacing[I] = 0;
		U.SpeedCap[I] = Fix::Zero();
		U.IdleTurret[I] = 0;
	}

	namespace
	{
		// o leque: as torres da ponta olham 25° pra fora (a vigia em todas as direções da doutrina de
		// coluna blindada: cada um cobre o seu setor), o meio olha pra frente
		constexpr int32_t TurretFanBam = Angle::FullTurn * 25 / 360;
	}

	void World::FanTurrets(const std::vector<int32_t>& Members, const std::vector<Vec2>& Slots, Angle March)
	{
		const int32_t N = int32_t(Members.size());
		if (N < 2 || Slots.size() != Members.size())
		{
			return;
		}
		const Vec2 F = Vec2::FromAngle(March);
		const Vec2 Left = Vec2::Make(-F.Y, F.X);
		Vec2 Center = Vec2::Zero();
		for (const Vec2& S : Slots)
		{
			Center += S / Fix::FromInt(int64_t(N));
		}
		std::vector<Fix> Lat(static_cast<size_t>(N));
		Fix HalfWidth = Fix::Zero();
		for (int32_t K = 0; K < N; ++K)
		{
			Lat[size_t(K)] = (Slots[size_t(K)] - Center).Dot(Left);
			HalfWidth = Max(HalfWidth, Abs(Lat[size_t(K)]));
		}
		if (HalfWidth > Fix::FromInt(2))
		{
			// em linha ou em bloco: pelo lado de cada um (positivo = esquerda, como o giro da torre)
			for (int32_t K = 0; K < N; ++K)
			{
				const int32_t I = Members[size_t(K)];
				if (!DefOf(I).IsStructure() && DefOf(I).HasWeapon())
				{
					U.IdleTurret[I] = int32_t((Lat[size_t(K)] / HalfWidth * Fix::FromInt(TurretFanBam)).Round());
				}
			}
			return;
		}
		// em fila indiana: o da frente olha pra frente e os de trás alternam esquerda e direita
		std::vector<int32_t> Order(static_cast<size_t>(N));
		for (int32_t K = 0; K < N; ++K)
		{
			Order[size_t(K)] = K;
		}
		std::sort(Order.begin(), Order.end(), [&](int32_t A, int32_t B)
		{
			const Fix FA = (Slots[size_t(A)] - Center).Dot(F);
			const Fix FB = (Slots[size_t(B)] - Center).Dot(F);
			return FA != FB ? FA > FB : A < B;
		});
		for (int32_t R = 0; R < N; ++R)
		{
			const int32_t I = Members[size_t(Order[size_t(R)])];
			if (!DefOf(I).IsStructure() && DefOf(I).HasWeapon())
			{
				U.IdleTurret[I] = R == 0 ? 0 : ((R & 1) != 0 ? TurretFanBam : -TurretFanBam);
			}
		}
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

	void World::Uncross(const std::vector<int32_t>& Members, std::vector<Vec2>& Slots) const
	{
		// troca dois de vaga quando a soma das distâncias cai: com a distância simples (não a ao
		// quadrado), a atribuição sem melhora por troca não tem caminhos que se cruzam (o "NoX" do
		// BAR, estudo de 11/10). Passadas e teto fixos, por N e nunca por tempo: o lockstep exige
		constexpr int32_t MaxUncross = 48;
		constexpr int32_t Passes = 3;
		const int32_t N = int32_t(Members.size());
		if (N < 2 || N > MaxUncross)
		{
			return;
		}
		for (int32_t Pass = 0; Pass < Passes; ++Pass)
		{
			bool bSwapped = false;
			for (int32_t A = 0; A < N; ++A)
			{
				for (int32_t B = A + 1; B < N; ++B)
				{
					const Vec2 PA = U.Pos[Members[size_t(A)]];
					const Vec2 PB = U.Pos[Members[size_t(B)]];
					const Fix Now = Distance(PA, Slots[size_t(A)]) + Distance(PB, Slots[size_t(B)]);
					const Fix Swapped = Distance(PA, Slots[size_t(B)]) + Distance(PB, Slots[size_t(A)]);
					if (Swapped + Fix::FromMilli(10) < Now)
					{
						std::swap(Slots[size_t(A)], Slots[size_t(B)]);
						bSwapped = true;
					}
				}
			}
			if (!bSwapped)
			{
				break;
			}
		}
	}

	void World::FormationSlots(const std::vector<int32_t>& Members, Vec2 Point, std::vector<Vec2>& Out)
	{
		// o "quadro mágico" do SC2 e a formação do C&C: o grupo é levado como está, só transladado
		// (sem girar: ninguém troca de lado, nada cruza). Grupo espalhado demais é apertado até o
		// tamanho de um bloco (raiz de N vagas de lado), pra o F não virar "cada um no seu canto do mapa".
		const int32_t N = int32_t(Members.size());
		Out.assign(size_t(N), Point);
		if (N <= 1)
		{
			return;
		}
		Vec2 Sum = Vec2::Zero();
		Fix MaxRadius = Fix::Zero();
		int32_t Clearance = 1;
		for (const int32_t I : Members)
		{
			Sum += U.Pos[I];
			MaxRadius = Max(MaxRadius, DefOf(I).Radius);
		}
		Clearance = std::max(Clearance, SimMap::ClearanceOf(GroupMobility(Members))) | (GroupMobility(Members) & (SimMap::CrusherBit | SimMap::ToppleBit));
		const Vec2 Center = Vec2::Make(Sum.X / int64_t(N), Sum.Y / int64_t(N));
		const Fix Spacing = MaxRadius * Fix::FromMilli(SlotSpacingPerRadiusMilli);
		const Fix Reach = Spacing * int64_t(ISqrt(uint64_t(N)) + 1);
		Fix Farthest = Fix::Zero();
		for (const int32_t I : Members)
		{
			Farthest = Max(Farthest, Distance(U.Pos[I], Center));
		}
		Cell PointCell = MapData.CellOf(Point);
		const int32_t Region = Pathfinder::NearestWalkable(MapData, PointCell, Clearance, 24, PointCell) ? RegionAt(Clearance, PointCell) : 0;
		for (int32_t K = 0; K < N; ++K)
		{
			Vec2 Rel = U.Pos[Members[size_t(K)]] - Center;
			if (Farthest > Reach)
			{
				Rel = Rel * (Reach / Farthest);   // aperta por igual: o desenho fica, só menor
			}
			const Vec2 Slot = ClampToMap(Point + Rel);
			Out[size_t(K)] = Region != 0 ? IntoRegion(Clearance, Region, Slot, Point) : Slot;
		}
	}

	void World::LineSlots(const std::vector<int32_t>& Members, Vec2 Point, Angle Facing, Fix Width, std::vector<Vec2>& Out) const
	{
		// a linha de frente: as fileiras são de través da direção (de frente pro inimigo, não em coluna), a da
		// frente no ponto apertado e as outras atrás. A largura vem do arraste (o BAR espalha pela linha
		// puxada); arraste curto, a do grupo: até 4 numa fileira só, senão metade em cada (no máximo 8).
		// (v4, 23/10) A linha puxada dá o passo, como no BAR (Custom Formations: "evenly spread along the line") e no Total
		// War: cabem floor(L / passo mínimo) + 1 na fileira; com menos unidades que isso, uma fileira só, de ponta a ponta da
		// linha (passo L / (N − 1)); com mais, as fileiras cheias vão de ponta a ponta (o passo nunca abaixo do mínimo, 2,34
		// raios: as 32 unidades do ZH) e a de trás, incompleta, fica no meio com o mesmo passo. Antes, com menos unidades que
		// a linha comportava, todas juntavam no meio no passo mínimo (3 tanques numa linha de 60 m ocupavam 17 m).
		// (revisão de 23/10) floor, não round: com round a fileira passava das pontas (6 tanques em 30 m faziam 33,7 m, 1,85 m
		// além de cada ponta); agora toda vaga fica dentro da linha puxada, a de trás desencontrada também.
		const int32_t N = int32_t(Members.size());
		Out.assign(size_t(N), Point);
		if (N <= 1)
		{
			return;
		}
		Vec2 Sum = Vec2::Zero();
		Fix MaxRadius = Fix::Zero();
		int32_t Clearance = 1;
		for (const int32_t I : Members)
		{
			Sum += U.Pos[I];
			MaxRadius = Max(MaxRadius, DefOf(I).Radius);
		}
		Clearance = std::max(Clearance, SimMap::ClearanceOf(GroupMobility(Members))) | (GroupMobility(Members) & (SimMap::CrusherBit | SimMap::ToppleBit));
		const Vec2 Center = Vec2::Make(Sum.X / int64_t(N), Sum.Y / int64_t(N));
		const Fix Spacing = MaxRadius * Fix::FromMilli(SlotSpacingPerRadiusMilli);
		int32_t Cols = N <= 4 ? N : std::min(8, (N + 1) / 2);
		// o passo de lado na fileira (Gap) e o de fundo entre as fileiras (Spacing, o mínimo)
		Fix Gap = Spacing;
		const bool bDrawn = Width > Spacing;   // a linha puxada manda na largura
		if (bDrawn)
		{
			Cols = int32_t((Width / Spacing).Floor()) + 1;   // a largura é de centro a centro dos tanques das pontas
			Cols = std::clamp(Cols, 1, N);
			Gap = Cols > 1 ? Max(Spacing, Width / int64_t(Cols - 1)) : Spacing;
		}
		Cols = std::clamp(Cols, 1, N);
		const Vec2 Dir = Vec2::FromAngle(Facing);
		const Vec2 Side = Vec2::Make(-Dir.Y, Dir.X);
		Cell PointCell = MapData.CellOf(Point);
		const int32_t Region = Pathfinder::NearestWalkable(MapData, PointCell, Clearance, 24, PointCell) ? RegionAt(Clearance, PointCell) : 0;
		const auto Settle = [&](Vec2 Slot) { return Region != 0 ? IntoRegion(Clearance, Region, ClampToMap(Slot), Point) : ClampToMap(Slot); };

		// quem está mais à frente (na direção nova) vai pra fileira da frente; na fileira, cada um pelo seu lado
		std::vector<int32_t> Order(static_cast<size_t>(N));
		std::vector<Fix> Forward(static_cast<size_t>(N));
		std::vector<Fix> Lateral(static_cast<size_t>(N));
		for (int32_t K = 0; K < N; ++K)
		{
			const Vec2 Rel = U.Pos[Members[size_t(K)]] - Center;
			Forward[size_t(K)] = Rel.Dot(Dir);
			Lateral[size_t(K)] = Rel.Dot(Side);
			Order[size_t(K)] = K;
		}
		std::sort(Order.begin(), Order.end(), [&](int32_t A, int32_t B)
		{
			return Forward[size_t(A)] != Forward[size_t(B)] ? Forward[size_t(A)] > Forward[size_t(B)] : A < B;
		});
		for (int32_t Row = 0; Row * Cols < N; ++Row)
		{
			const int32_t First = Row * Cols;
			const int32_t Count = std::min(Cols, N - First);
			std::sort(Order.begin() + First, Order.begin() + First + Count, [&](int32_t A, int32_t B)
			{
				return Lateral[size_t(A)] != Lateral[size_t(B)] ? Lateral[size_t(A)] < Lateral[size_t(B)] : A < B;
			});
			for (int32_t Col = 0; Col < Count; ++Col)
			{
				const Fix Lat = Gap * int64_t(2 * Col - (Count - 1)) / 2;
				Vec2 Slot = Point + Side * Lat - Dir * (Spacing * int64_t(Row));
				if ((Row & 1) != 0)
				{
					// a de trás meio desencontrada: cada um atira pelo vão da frente; na linha puxada, só até a ponta dela (a
					// fileira cheia de trás fica alinhada: desencontrada, passaria da ponta)
					Fix Shift = Gap * 5 / 16;
					if (bDrawn)
					{
						Shift = Min(Shift, Max(Width / 2 - Gap * int64_t(Count - 1) / 2, Fix::Zero()));
					}
					Slot += Side * Shift;
				}
				Out[size_t(Order[size_t(First + Col)])] = Settle(Slot);
			}
		}
		Uncross(Members, Out);
	}

	// ---------------------------------------------------------------- (v4, 23/10) a prévia da linha dos dois botões

	bool World::PreviewLineSlots(PlayerIndex P, const std::vector<EntityId>& Units, Vec2 Point, int32_t Value,
		std::vector<EntityId>& OutIds, std::vector<Vec2>& OutSlots) const
	{
		// a mesma conta do comando, passo a passo (ApplyCommand): as portas do Step e do ApplyCommand, o grupo como o SetGroup
		// monta (os MaxGroupSize primeiros, vivos e do jogador, em ordem de ID, sem repetição), sem os prédios como o Move, e
		// o LineSlots no ponto preso no mapa. Só lê: o compilador garante (const), e a vaga desenhada é a vaga da ordem
		OutIds.clear();
		OutSlots.clear();
		if (!MoveHasFacingOf(Value) || P >= Players.size() || Players[P].bDefeated || bMatchOver)
		{
			return false;
		}
		std::vector<EntityId> Group;
		for (size_t K = 0; K < Units.size() && K < size_t(MaxGroupSize); ++K)
		{
			const int32_t I = IndexOf(Units[K]);
			if (I >= 0 && U.Owner[I] == P)
			{
				Group.push_back(Units[K]);
			}
		}
		std::sort(Group.begin(), Group.end());
		Group.erase(std::unique(Group.begin(), Group.end()), Group.end());
		std::vector<int32_t> Members;
		for (const EntityId Id : Group)
		{
			const int32_t I = IndexOf(Id);
			if (I >= 0 && U.Owner[I] == P && !DefOf(I).IsStructure())
			{
				Members.push_back(I);
			}
		}
		if (Members.empty())
		{
			return false;
		}
		LineSlots(Members, ClampToMap(Point), Angle::FromBam(MoveFacingBamOf(Value)), Fix::FromInt(2 * MoveWidthHalfMOf(Value)), OutSlots);
		OutIds.reserve(Members.size());
		for (const int32_t I : Members)
		{
			OutIds.push_back(U.Id[I]);
		}
		return true;
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
		}
		Clearance = std::max(Clearance, SimMap::ClearanceOf(GroupMobility(Members))) | (GroupMobility(Members) & (SimMap::CrusherBit | SimMap::ToppleBit));
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
			Uncross(Members, Out);
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
			Uncross(Members, Out);
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

	bool World::IsDoomed(int32_t Target) const
	{
		int64_t Incoming = 0;
		for (const Projectile& P : Projectiles)
		{
			if (P.Target == U.Id[Target])
			{
				Incoming += int64_t(P.Damage) * RulesData.Armor.Get(DefOf(Target).Armor, P.Type) / 100;
			}
		}
		for (const DamageIntent& H : PendingDamage)
		{
			if (H.Target == U.Id[Target])
			{
				Incoming += int64_t(H.Amount) * RulesData.Armor.Get(DefOf(Target).Armor, H.Type) / 100;
			}
		}
		return Incoming > 0 && Incoming >= U.Hp[Target];
	}

	EntityId World::NearestEnemy(int32_t Self, Fix Radius, bool bSkipDoomed) const
	{
		EntityId Best = NoEntity;
		int64_t BestD2 = 0;
		bool bBestIsStructure = false;
		const bool bIgnoring = U.Ignore[Self] != NoEntity && CurrentTick < U.IgnoreUntil[Self];
		// (v4) a artilharia (a arma com alcance mínimo, o Tomahawk): não enxerga quem está dentro do mínimo (o centro a menos
		// dele) e prefere prédio a unidade: o lançador que só anda atacando com a onda escolhe a defesa, não o tanque que passa
		const Fix MinRange = DefOf(Self).Weapon.MinRange;
		const bool bArtillery = MinRange.Raw > 0;
		ForEachNear(U.Pos[Self], Radius + MaxBodyRadius, [&](int32_t J)
		{
			if (U.Hp[J] <= 0 || !AreEnemies(U.Owner[Self], U.Owner[J]) || (bIgnoring && U.Id[J] == U.Ignore[Self]) || !SeesUnit(Self, J)
				|| (bSkipDoomed && IsDoomed(J)))
			{
				return;
			}
			const int64_t D2 = DistSqRaw(U.Pos[Self], U.Pos[J]);
			if (D2 > RadiusSqRaw(Radius + DefOf(J).Radius) || (bArtillery && D2 < RadiusSqRaw(MinRange)))
			{
				return;
			}
			// o que atira primeiro no que se mexe: prédio só quando não há unidade no alcance (a artilharia, ao contrário)
			const bool bStructure = DefOf(J).IsStructure();
			const bool bBestWrongKind = bArtillery ? (!bBestIsStructure && bStructure) : (bBestIsStructure && !bStructure);
			const bool bBetter = Best == NoEntity
				|| bBestWrongKind
				|| (bBestIsStructure == bStructure && (D2 < BestD2 || (D2 == BestD2 && U.Id[J] < Best)));
			if (bBetter)
			{
				Best = U.Id[J];
				BestD2 = D2;
				bBestIsStructure = bStructure;
			}
		});
		return Best;
	}

	// ---------------------------------------------------------------- alvos

	void World::UpdateTargets()
	{
		// (v4) o alcance mínimo (o Tomahawk: 20 m, pelo centro): o alvo mais perto que isto sai da mira
		const auto TooClose = [this](int32_t Self, EntityId T)
		{
			const Fix MinRange = DefOf(Self).Weapon.MinRange;
			const int32_t J = IndexOf(T);
			return MinRange.Raw > 0 && J >= 0 && DistSqRaw(U.Pos[Self], U.Pos[J]) < RadiusSqRaw(MinRange);
		};
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			if (!D.HasWeapon())
			{
				// prédio, Dozer e caminhão: sem arma, não miram nem vão atrás de ninguém
				U.FireTarget[I] = NoEntity;
				U.Engage[I] = NoEntity;
				U.LastAttacker[I] = NoEntity;
				continue;
			}
			const bool bScan = ((CurrentTick + U.Id[I]) % TargetScanInterval) == 0;
			if (D.IsStructure())
			{
				// a defesa de base (a Patriot, 14/10): não anda nem persegue; mira o inimigo mais perto no alcance (ou o da
				// ordem de ataque). Em obra ou sem energia (o POWERED do ZH) fica muda
				U.Engage[I] = NoEntity;
				if (U.BuildLeft[I] != 0 || (D.bNeedsPower && Players[U.Owner[I]].IsLowPower()) || U.Order[I] == OrderType::AttackGround || IsSelling(I))
				{
					if (U.Order[I] != OrderType::AttackGround || IsSelling(I))
					{
						U.FireTarget[I] = NoEntity;   // (v4) a vendida também: desmontando, não atira
					}
					continue;
				}
				auto InReach = [&](EntityId T)
				{
					const int32_t J = IndexOf(T);
					return J >= 0 && DistSqRaw(U.Pos[I], U.Pos[J]) <= RadiusSqRaw(D.Weapon.Range + DefOf(J).Radius) && SeesUnit(I, J) && !TooClose(I, T);
				};
				const bool bForcedS = U.Order[I] == OrderType::Attack && U.ForcedAttack[I] != 0;
				if (U.Order[I] == OrderType::Attack)
				{
					const bool bAlive = bForcedS ? IsAliveUnit(U.OrderTarget[I]) && U.OrderTarget[I] != U.Id[I] : IsAliveEnemy(I, U.OrderTarget[I]);
					if (!bAlive)
					{
						U.Order[I] = OrderType::Idle;
						U.OrderTarget[I] = NoEntity;
					}
					else if (InReach(U.OrderTarget[I]))
					{
						U.FireTarget[I] = U.OrderTarget[I];
						continue;
					}
				}
				if (U.FireTarget[I] != NoEntity && (!IsAliveEnemy(I, U.FireTarget[I]) || !InReach(U.FireTarget[I])))
				{
					U.FireTarget[I] = NoEntity;
				}
				if (!IsAliveEnemy(I, U.LastAttacker[I]))
				{
					U.LastAttacker[I] = NoEntity;
				}
				// (16/10) sem desperdício: o alvo que os tiros no ar já vão matar fica pra eles; a defesa passa pro próximo
				if (U.FireTarget[I] != NoEntity && IsDoomed(IndexOf(U.FireTarget[I])))
				{
					const EntityId Next = NearestEnemy(I, D.Weapon.Range, true);
					if (Next != NoEntity)
					{
						U.FireTarget[I] = Next;
					}
				}
				if (U.FireTarget[I] == NoEntity && (bScan || U.LastAttacker[I] != NoEntity))
				{
					U.FireTarget[I] = NearestEnemy(I, D.Weapon.Range, true);
					if (U.FireTarget[I] == NoEntity)
					{
						U.FireTarget[I] = NearestEnemy(I, D.Weapon.Range);
					}
				}
				continue;
			}
			if (U.Order[I] == OrderType::AttackGround)
			{
				// atira no chão: nem persegue nem troca de alvo (a torre e a arma vão pelo OrderPoint)
				U.Engage[I] = NoEntity;
				U.FireTarget[I] = NoEntity;
				continue;
			}
			// o alvo vale: inimigo vivo; no ataque forçado, o da ordem, de qualquer dono
			const bool bForced = U.Order[I] == OrderType::Attack && U.ForcedAttack[I] != 0;
			auto Valid = [&](EntityId T) { return bForced && T == U.OrderTarget[I] ? IsAliveUnit(T) && T != U.Id[I] : IsAliveEnemy(I, T); };

			if (U.Order[I] == OrderType::Attack && !Valid(U.OrderTarget[I]))
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
			if (!Valid(U.Engage[I]) || (IndexOf(U.Engage[I]) >= 0 && !SeesUnit(I, IndexOf(U.Engage[I]))))
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
			else if (U.FireTarget[I] != NoEntity && (!Valid(U.FireTarget[I]) || !WithinRaw(U.FireTarget[I], D.Weapon.Range)
				|| !SeesUnit(I, IndexOf(U.FireTarget[I]))))
			{
				U.FireTarget[I] = NoEntity;
			}
			if (U.FireTarget[I] != NoEntity && TooClose(I, U.FireTarget[I]))
			{
				U.FireTarget[I] = NoEntity;   // (v4) dentro do alcance mínimo: a artilharia procura outro (o NearestEnemy pula este)
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
		if (U.Order[I] == OrderType::AttackGround)
		{
			const Fix Stop = DefOf(I).Weapon.Range * ChaseStopFraction;
			if (DistSqRaw(U.Pos[I], U.OrderPoint[I]) <= RadiusSqRaw(Stop))
			{
				return false;   // no alcance: fica e atira no chão
			}
			OutGoal = U.OrderPoint[I];
			OutStopDistance = Stop;
			return true;
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
				const int32_t Clearance = MobilityOf(I);
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
			if (D.IsStructure())
			{
				continue;
			}
			Vec2 Goal;
			Fix Stop;
			const bool bWantsToMove = MovementGoal(I, Goal, Stop) && !U.Path[I].empty();
			Fix TargetSpeed = Fix::Zero();
			Angle Desired = U.Hull[I];

			if (bWantsToMove)
			{
				std::vector<Vec2>& Path = U.Path[I];
				const Fix CellSize = MapData.CellSize;
				const int32_t PathClearance = MobilityOf(I);
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
					Fix Top = U.SpeedCap[I] > Fix::Zero() ? Min(D.MaxSpeed, U.SpeedCap[I]) : D.MaxSpeed;
					if (U.CrushSlowLeft[I] > 0 && U.CrushSlowTotal[I] > 0)
					{
						// (16/10) esmagando: cai ao piso e volta em rampa (o usa_mbt acelera 263 m/s²: um degrau seria um tranco seco)
						const int64_t Lost = int64_t(1000 - U.CrushSlowFloor[I]) * U.CrushSlowLeft[I] / U.CrushSlowTotal[I];
						Top = Top * Fix::FromRatio(1000 - Lost, 1000);
					}
					TargetSpeed = Off >= TurnInPlaceBam
						? Fix::Zero()
						: Top * Fix::FromRatio(TurnInPlaceBam - Off, TurnInPlaceBam);
					if (bLast)
					{
						// freia a tempo: v² = 2·a·d
						const Fix Remaining = Max(ToWaypoint - Stop, Fix::Zero());
						TargetSpeed = Min(TargetSpeed, Sqrt(D.Brake * Remaining * 2));
					}
				}
			}

			if (!bWantsToMove && U.HasArriveFacing[I] && U.Order[I] == OrderType::Idle && U.Engage[I] == NoEntity)
			{
				Desired = U.ArriveFacing[I];
				if (AbsInt(U.Hull[I].DeltaTo(Desired)) <= D.TurnBam)
				{
					U.HasArriveFacing[I] = 0;
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
				const int32_t Clearance = MobilityOf(I);
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
						if (bNear && U.Engage[I] == NoEntity && U.Order[I] != OrderType::Attack && U.Order[I] != OrderType::AttackGround)
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

	bool World::HoldsGround(int32_t I) const
	{
		// (v4, 23/10) atirando parado: a torre num alvo (ou o tiro no chão) e a unidade sem andar
		return U.Speed[size_t(I)] <= Fix::Zero() && (U.FireTarget[size_t(I)] != NoEntity || U.Order[size_t(I)] == OrderType::AttackGround);
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
			if (DefOf(I).IsStructure())
			{
				continue;
			}
			const Fix RI = DefOf(I).Radius;
			const bool bMovingI = U.Speed[I] > Fix::Zero();
			ForEachNear(U.Pos[I], RI + MaxUnitRadius, [&](int32_t J)
			{
				if (J <= I || DefOf(J).IsStructure())
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
				Vec2 Dir = Dist.Raw > 0
					? Delta / Dist
					: Vec2::FromAngle(Angle::FromBam(int32_t((U.Id[I] * 2654435761u) >> 16)));
				const Fix Overlap = Reach - Dist;
				const bool bMovingJ = U.Speed[J] > Fix::Zero();
				Fix ShareI = Fix::Half();
				if (bMovingI != bMovingJ)
				{
					ShareI = bMovingI ? Fix::FromRatio(1, 4) : Fix::FromRatio(3, 4);
				}
				// (v4, 23/10) quem atira parado não sai do lugar pro aliado (o Pathfinder::moveAllies do ZH: "Don't move units
				// that are attacking"): o outro leva o empurrão inteiro. Antes o reforço que chegava empurrava o tanque parado,
				// que empurrava o Tomahawk do cerco 6 m pra dentro do alcance da Patriot (o 24d da IA: morria aos 102 s)
				if (TeamOf(U.Owner[size_t(I)]) == TeamOf(U.Owner[size_t(J)]))
				{
					const bool bHoldI = HoldsGround(I);
					if (bHoldI != HoldsGround(J))
					{
						ShareI = bHoldI ? Fix::Zero() : Fix::One();
						// a batida torta, ±9° pelo par (como na carcaça presa, PushWrecks): quem leva o empurrão inteiro escorrega
						// pro lado e contorna, nem de frente fica preso atrás
						const int32_t Skew = ((U.Id[I] * 2654435761u) ^ (U.Id[J] * 40503u)) & 1u ? 1638 : -1638;
						Dir = Vec2::FromAngle(Angle::FromBam(int32_t(Dir.Heading().Bam) + Skew));
					}
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
			if (MapData.IsWalkable(C.X, C.Y, MobilityOf(I)))
			{
				U.Pos[I] = Next;
			}
		}
	}

	void World::PushWrecks()
	{
		if (Wrecks.empty())
		{
			return;
		}
		const Fix BamPerRadian = Fix::FromRatio(10430378, 1000);   // 65536 / 2π
		std::vector<Vec2> Move(Wrecks.size(), Vec2::Zero());
		std::vector<int64_t> Turn(Wrecks.size(), 0);
		std::vector<uint8_t> Contact(Wrecks.size(), 0);
		std::vector<Vec2> Push(U.Id.size(), Vec2::Zero());
		const auto Circles = [this](const WreckState& Wr, Vec2 (&C)[2], Fix& R)
		{
			const Fix Big = RulesData.Units[Wr.Def].Radius;
			R = Big * WreckSubRadius;
			const Vec2 Ax = Vec2::FromAngle(Wr.Hull) * (Big * WreckSubOffset);
			C[0] = Wr.Pos + Ax;
			C[1] = Wr.Pos - Ax;
		};
		// o empurrão fora do centro gira o casco: θ = (r × F) / (2·|r|²), r do centro ao círculo que levou o empurrão
		const auto AddTurn = [&](size_t K, Vec2 At, Vec2 F)
		{
			const Vec2 Lever = At - Wrecks[K].Pos;
			const Fix Den = Lever.Dot(Lever) * 2;
			if (Den.Raw > 0)
			{
				Turn[K] += (((Lever.X * F.Y - Lever.Y * F.X) / Den) * BamPerRadian).Round();
			}
		};
		// o vivo contra a carcaça: a sobreposição dividida pela massa
		for (size_t K = 0; K < Wrecks.size(); ++K)
		{
			const WreckState& Wr = Wrecks[K];
			Vec2 C[2];
			Fix R;
			Circles(Wr, C, R);
			const Fix Big = RulesData.Units[Wr.Def].Radius;
			const int64_t MassW = (Big * Big).Raw;
			// (22/10) afundando: não sai do lugar (o vivo leva o empurrão inteiro) e o encosto encolhe com o afundar, até o
			// tanque passar por cima do resto
			const bool bSinking = Wr.SinkStart > 0;
			const Fix Keep = Fix::One() - WreckSinkProgress(Wr);
			ForEachNear(Wr.Pos, Big + MaxUnitRadius, [&](int32_t J)
			{
				const UnitDef& DJ = DefOf(J);
				if (DJ.IsStructure())
				{
					return;
				}
				const int64_t MassU = (DJ.Radius * DJ.Radius).Raw;
				const Fix ShareW = Wr.Stuck > 0 || bSinking ? Fix::Zero() : Fix::FromRatio(MassU, MassU + MassW);
				for (int32_t S = 0; S < 2; ++S)
				{
					const Fix Reach = bSinking ? (DJ.Radius + R) * Keep : DJ.Radius + R;
					const Vec2 Delta = U.Pos[J] - C[S];
					if (Delta.LengthSqRaw() >= RadiusSqRaw(Reach))
					{
						continue;
					}
					const Fix Dist = Delta.Length();
					const Vec2 Straight = Dist.Raw > 0 ? Delta / Dist : Vec2::FromAngle(Angle::FromBam(int32_t(Wr.Hull.Bam) + Angle::QuarterTurn));
					// a batida torta: ±9° (1638 bam) pelo par carcaça × vivo, sempre o mesmo lado pro mesmo par
					const int32_t Skew = ((Wr.Id * 2654435761u) ^ (U.Id[J] * 40503u)) & 1u ? 1638 : -1638;
					const Vec2 Dir = Vec2::FromAngle(Angle::FromBam(int32_t(Straight.Heading().Bam) + Skew));
					const Fix Overlap = Reach - Dist;
					Push[size_t(J)] += Dir * (Overlap * (Fix::One() - ShareW));
					const Vec2 F = -(Dir * (Overlap * ShareW));
					Move[K] += F;
					AddTurn(K, C[S], F);
					Contact[K] = 1;
				}
			});
		}
		// carcaça com carcaça (duas mortes no mesmo lugar, ou uma empurrada contra a outra): meio a meio; (22/10) a que afunda
		// não sai do lugar (a outra leva tudo) e o encosto encolhe com o afundar dela; duas afundando não se mexem
		for (size_t K = 0; K < Wrecks.size(); ++K)
		{
			for (size_t L = K + 1; L < Wrecks.size(); ++L)
			{
				const bool bSinkK = Wrecks[K].SinkStart > 0;
				const bool bSinkL = Wrecks[L].SinkStart > 0;
				if (bSinkK && bSinkL)
				{
					continue;
				}
				const Fix BigK = RulesData.Units[Wrecks[K].Def].Radius;
				const Fix BigL = RulesData.Units[Wrecks[L].Def].Radius;
				if (DistSqRaw(Wrecks[K].Pos, Wrecks[L].Pos) >= RadiusSqRaw(BigK + BigL))
				{
					continue;
				}
				Vec2 CK[2], CL[2];
				Fix RK, RL;
				Circles(Wrecks[K], CK, RK);
				Circles(Wrecks[L], CL, RL);
				const Fix Keep = Fix::One() - WreckSinkProgress(bSinkK ? Wrecks[K] : Wrecks[L]);
				for (int32_t A = 0; A < 2; ++A)
				{
					for (int32_t B = 0; B < 2; ++B)
					{
						const Fix Reach = bSinkK || bSinkL ? (RK + RL) * Keep : RK + RL;
						const Vec2 Delta = CK[A] - CL[B];
						if (Delta.LengthSqRaw() >= RadiusSqRaw(Reach))
						{
							continue;
						}
						const Fix Dist = Delta.Length();
						const Vec2 Dir = Dist.Raw > 0 ? Delta / Dist : Vec2::FromAngle(Angle::FromBam(int32_t((Wrecks[K].Id * 2654435761u) >> 16)));
						if (bSinkK || bSinkL)
						{
							// a parada não anda: a outra sai inteira
							const Vec2 Full = Dir * (Reach - Dist);
							if (bSinkK)
							{
								Move[L] -= Full;
								AddTurn(L, CL[B], -Full);
							}
							else
							{
								Move[K] += Full;
								AddTurn(K, CK[A], Full);
							}
							continue;
						}
						const Vec2 F = Dir * ((Reach - Dist) / 2);
						Move[K] += F;
						Move[L] -= F;
						AddTurn(K, CK[A], F);
						AddTurn(L, CL[B], -F);
					}
				}
			}
		}
		// a carcaça anda e gira (até R/4 e 2° por passo); onde não cabe, trava e o vivo leva o empurrão inteiro
		for (size_t K = 0; K < Wrecks.size(); ++K)
		{
			WreckState& Wr = Wrecks[K];
			if (Wr.SinkStart > 0)
			{
				continue;   // (22/10) afundando: presa no chão
			}
			if (!Contact[K])
			{
				Wr.Stuck = 0;
			}
			else if (Wr.Stuck > 0)
			{
				--Wr.Stuck;
			}
			Vec2 M = Move[K];
			const int64_t T = Turn[K] > WreckMaxTurnBam ? WreckMaxTurnBam : (Turn[K] < -WreckMaxTurnBam ? -WreckMaxTurnBam : Turn[K]);
			if (M == Vec2::Zero() && T == 0)
			{
				continue;
			}
			const UnitDef& D = RulesData.Units[Wr.Def];
			const Fix MaxMove = D.Radius / 4;
			if (M.LengthSqRaw() > RadiusSqRaw(MaxMove))
			{
				M = M.Normalized() * MaxMove;
			}
			const Vec2 Next = Wr.Pos + M;
			const Cell C = MapData.CellOf(Next);
			if (MapData.IsWalkable(C.X, C.Y, MapData.ClearanceFor(D.PathRadius)))
			{
				Wr.Pos = Next;
				Wr.Hull = Angle::FromBam(int32_t(Wr.Hull.Bam) + int32_t(T));
			}
			else
			{
				Wr.Stuck = WreckStuckTicks;
			}
		}
		// o vivo segurado pela carcaça: a parte dele da sobreposição sai inteira no passo (até R/4), não o encosto suave
		// entre dois tanques, senão ele entra meio casco na carcaça antes de empurrar (carcaca_empurra2, 13/10)
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			Vec2 P = Push[size_t(I)];
			if (P == Vec2::Zero())
			{
				continue;
			}
			const Fix MaxPush = DefOf(I).Radius / 4;
			if (P.LengthSqRaw() > RadiusSqRaw(MaxPush))
			{
				P = P.Normalized() * MaxPush;
			}
			const Vec2 Next = U.Pos[I] + P;
			const Cell C = MapData.CellOf(Next);
			if (MapData.IsWalkable(C.X, C.Y, MobilityOf(I)))
			{
				U.Pos[I] = Next;
			}
		}
	}

	Fix World::WreckSinkProgress(const WreckState& Wr) const
	{
		if (Wr.SinkStart == 0)
		{
			return Fix::Zero();
		}
		const int32_t Ticks = RulesData.Units[Wr.Def].WreckSinkTicks;
		return Clamp(Fix::FromRatio(int64_t(CurrentTick - Wr.SinkStart), Ticks > 0 ? Ticks : 1), Fix::Zero(), Fix::One());
	}

	void World::SinkWreck(size_t K, int32_t Reason)
	{
		WreckState& Wr = Wrecks[K];
		Wr.SinkStart = CurrentTick;
		Wr.Stuck = 0;
		Events.push_back({ EventType::WreckSinking, NoEntity, Wr.Id, Wr.Pos, RulesData.Units[Wr.Def].WreckSinkTicks | (Reason << 16) });
	}

	void World::EraseWreck(size_t K, int32_t Reason)
	{
		Events.push_back({ EventType::WreckRemoved, NoEntity, Wrecks[K].Id, Wrecks[K].Pos, Reason });
		Wrecks.erase(Wrecks.begin() + std::ptrdiff_t(K));
	}

	void World::AgeWrecks()
	{
		if (Wrecks.empty())
		{
			return;
		}
		// o afundar acabou: sai do mapa (a que sai não pula a seguinte)
		for (size_t K = 0; K < Wrecks.size();)
		{
			const WreckState& Wr = Wrecks[K];
			if (Wr.SinkStart > 0 && CurrentTick - Wr.SinkStart >= TickNumber(RulesData.Units[Wr.Def].WreckSinkTicks))
			{
				EraseWreck(K, 0);
				continue;
			}
			++K;
		}
		// a vida acabou: começa a afundar
		int32_t Solid = 0;
		for (size_t K = 0; K < Wrecks.size(); ++K)
		{
			if (Wrecks[K].SinkStart > 0)
			{
				continue;
			}
			const int32_t Life = RulesData.Units[Wrecks[K].Def].WreckLifeTicks;
			if (Life > 0 && CurrentTick - Wrecks[K].Born >= TickNumber(Life))
			{
				SinkWreck(K, 0);
				continue;
			}
			++Solid;
		}
		// sólidas demais (a morte em massa): as mais velhas que já viveram 12 s afundam já, na ordem de nascimento (a
		// nova nunca some no meio da luta que a fez)
		for (size_t K = 0; K < Wrecks.size() && Solid > WreckSolidCap; ++K)
		{
			if (Wrecks[K].SinkStart == 0 && CurrentTick - Wrecks[K].Born >= WreckMinForcedAgeTicks)
			{
				SinkWreck(K, 1);
				--Solid;
			}
		}
	}

	// ---------------------------------------------------------------- torre e armas

	void World::UpdateTurrets()
	{
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			int32_t DesiredRel = U.IdleTurret[I];
			int32_t Rate = D.TurretTurnBam;
			const int32_t T = IndexOf(U.FireTarget[I]);
			if (U.Order[I] == OrderType::AttackGround && D.HasWeapon())
			{
				DesiredRel = U.Hull[I].DeltaTo((U.OrderPoint[I] - U.Pos[I]).Heading());
				U.TurretIdle[I] = 0;
			}
			else if (T >= 0)
			{
				const Angle Bearing = (U.Pos[T] - U.Pos[I]).Heading();
				DesiredRel = U.Hull[I].DeltaTo(Bearing);
				U.TurretIdle[I] = 0;
			}
			else
			{
				// sem alvo: depois de um tempo a torre volta pra frente (ou pro seu lado do leque), a meia velocidade (ZH)
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
		// (v4, 23/10) a mira antes do tiro (o AIM do TurretAI do ZH com o PreAttackDelay; o lançador do Tomahawk levanta o tubo
		// junto com o giro): o relógio sobe 1 por passo desde que há alvo no alcance (o da torre, ou o tiro no chão no alcance),
		// não desde o alinhamento, até o PreAttackTicks da arma; sem alvo, segura o TurretIdleReturn (o RecenterTime do ZH) e
		// desce à metade (1 a cada 2 passos) até 0; o alvo que volta sobe dali. Os tiros seguintes da rajada não esperam de novo
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			const WeaponDef& W = D.Weapon;
			if (W.PreAttackTicks <= 0 || !D.HasWeapon())
			{
				continue;
			}
			bool bAim = false;
			if (U.Order[I] == OrderType::AttackGround)
			{
				const int64_t D2 = DistSqRaw(U.Pos[I], U.OrderPoint[I]);
				bAim = D2 <= RadiusSqRaw(W.Range) && D2 >= RadiusSqRaw(W.MinRange);
			}
			else if (const int32_t T = IndexOf(U.FireTarget[I]); T >= 0)
			{
				const int64_t D2 = DistSqRaw(U.Pos[I], U.Pos[T]);
				bAim = D2 <= RadiusSqRaw(W.Range + DefOf(T).Radius) && D2 >= RadiusSqRaw(W.MinRange);
			}
			if (bAim)
			{
				U.AimTicks[I] = std::min(U.AimTicks[I] + 1, W.PreAttackTicks);
				U.AimIdle[I] = 0;
			}
			else if (U.AimTicks[I] > 0)
			{
				++U.AimIdle[I];
				if (U.AimIdle[I] > D.TurretIdleReturnTicks && ((U.AimIdle[I] - D.TurretIdleReturnTicks) & 1) == 0)
				{
					--U.AimTicks[I];
				}
			}
			else
			{
				U.AimIdle[I] = 0;
			}
		}
	}

	Fix World::GroundHeightAt(Vec2 P) const
	{
		const Cell C = MapData.CellOf(P);
		if (!MapData.HasHeights() || !MapData.InBounds(C.X, C.Y))
		{
			return Fix::Zero();
		}
		const size_t K = size_t(MapData.Index(C.X, C.Y));
		return Fix::FromRatio(int64_t(MapData.MinDm[K]) + int64_t(MapData.MaxDm[K]), 20);
	}

	bool World::FlyMissile(Projectile& P)
	{
		// (16/10, o Felipe: "ele atira mísseis, ... com explosão quando encosta no tanque; simular bem cada coisa")
		// O míssil guiado em 3D, como o do ZH (MissileAIUpdate: TryToFollowTarget), com a mira na frente: depois de sair
		// reto da caixa (o impulso), o rumo e a inclinação giram até MissileTurnBam por passo em direção ao ponto onde o
		// alvo vai estar quando ele chegar (a posição + a velocidade × os passos que faltam), e a velocidade sobe até o
		// ProjectileStep. Explode no contato (o passo dele passa a menos de 0,6 raio do meio do casco, a 1,2 m do chão),
		// no chão ou no fim da vida. Com o alvo morto, segue pro último ponto e explode lá.
		const WeaponDef& W = RulesData.Units[P.Def].Weapon;
		const int32_t T = IndexOf(P.Target);
		Fix HitRadius = Fix::FromMilli(1500);
		Vec2 HitAt = P.Aim;
		Fix HitZ = P.AimZ;
		if (T >= 0)
		{
			const UnitDef& DT = DefOf(T);
			const Fix Lift = DT.IsStructure() ? Fix::FromInt(3) : Fix::FromMilli(1200);
			const Fix Closing = Max(P.Speed, W.MissileLaunchStep);
			int64_t Ahead = Closing.Raw > 0 ? (Distance(P.Pos, U.Pos[T]) / Closing).Round() : 0;
			Ahead = Ahead < 0 ? 0 : (Ahead > 90 ? 90 : Ahead);
			HitAt = U.Pos[T];
			HitZ = GroundHeightAt(U.Pos[T]) + Lift;
			HitRadius = DT.IsStructure() ? DT.Radius : DT.Radius * Fix::FromMilli(600);
			P.Aim = U.Pos[T] + Vec2::FromAngle(U.Hull[T]) * (U.Speed[T] * Fix::FromInt(Ahead));
			P.AimZ = HitZ;
		}
		++P.Age;
		// (v4, 22/10) a velocidade que ainda deixa fazer a curva até o ponto: o ponto a D, θ fora do rumo, só é alcançável com o
		// raio de giro v ÷ ω ≤ D ÷ (2 sen θ) (fora dos dois círculos de giro, o caminho de Dubins). Mais perto ou mais de lado
		// que isso, o míssil não acelera além (e freia até lá, nunca abaixo da velocidade da caixa): a artilharia (o Tomahawk,
		// 100°/s) sobe e cai no ponto mesmo a 20 m. Sem isso ele orbitava o ponto a 20–30 m até o fim da vida e passava 7–25 m
		// dele a 40–50 m. Apontado pro ponto (θ ≈ 0) nada muda: a Patriot e o míssil do poder voam como antes
		Fix Top = W.ProjectileStep;
		if (W.MissileTurnBam > 0)
		{
			const Vec2 Flat = Vec2::FromAngle(P.Yaw) * Cos(P.Pitch);
			const Fix DirZ = Sin(P.Pitch);
			const Vec2 To = P.Aim - P.Pos;
			const Fix ToZ = P.AimZ - P.Z;
			const Fix CX = Flat.Y * ToZ - DirZ * To.Y;
			const Fix CY = DirZ * To.X - Flat.X * ToZ;
			const Fix CZ = Flat.X * To.Y - Flat.Y * To.X;
			const Fix Side = Sqrt(CX * CX + CY * CY + CZ * CZ);   // D × sen θ
			if (Side.Raw > 0)
			{
				const Fix Omega = Fix::FromRatio(int64_t(W.MissileTurnBam) * 6283185, int64_t(Angle::FullTurn) * 1000000);   // rad por passo
				const Fix Reach = Omega * (To.Dot(To) + ToZ * ToZ) / (Side * 2);
				Top = Min(Top, Max(Reach, W.MissileLaunchStep));
			}
		}
		const Fix Free = Min(P.Speed + W.MissileAccel, W.ProjectileStep);   // o de sempre: acelera até a velocidade da arma
		P.Speed = Free <= Top ? Free : Min(Free, Max(P.Speed - W.MissileAccel, Top));
		if (P.Age > W.MissileBoostTicks)
		{
			const Vec2 To = P.Aim - P.Pos;
			P.Yaw = P.Yaw.RotatedTowards(To.Heading(), W.MissileTurnBam);
			P.Pitch = P.Pitch.RotatedTowards(Atan2(P.AimZ - P.Z, To.Length()), W.MissileTurnBam);
		}
		const Vec2 From = P.Pos;
		const Fix FromZ = P.Z;
		P.Pos += Vec2::FromAngle(P.Yaw) * (P.Speed * Cos(P.Pitch));
		P.Z += P.Speed * Sin(P.Pitch);
		// o ponto do passo mais perto do alvo (o segmento From → Pos contra o meio do casco, em 3D)
		const Vec2 Seg = P.Pos - From;
		const Fix SegZ = P.Z - FromZ;
		const Vec2 Rel = HitAt - From;
		const Fix RelZ = HitZ - FromZ;
		const Fix SegLenSq = Seg.Dot(Seg) + SegZ * SegZ;
		const Fix Along = SegLenSq.Raw > 0 ? Clamp((Rel.Dot(Seg) + RelZ * SegZ) / SegLenSq, Fix::Zero(), Fix::One()) : Fix::Zero();
		const Vec2 Near = From + Seg * Along;
		const Fix NearZ = FromZ + SegZ * Along;
		const Vec2 Miss = HitAt - Near;
		const Fix MissZ = HitZ - NearZ;
		if (Miss.Dot(Miss) + MissZ * MissZ <= HitRadius * HitRadius)
		{
			if (T >= 0)
			{
				PendingDamage.push_back({ P.Shooter, P.Target, P.Damage, P.Type, P.Owner });
			}
			Events.push_back({ EventType::ProjectileHit, P.Shooter, T >= 0 ? P.Target : NoEntity, Near, int32_t(P.Id) });
			if (W.HasSplash())
			{
				ApplySplash(Near, P, T >= 0 ? P.Target : NoEntity);   // (v4) o alvo já levou o cheio; a área vai pros outros
			}
			return false;
		}
		if (P.Z <= GroundHeightAt(P.Pos) || P.Age >= W.MissileLifeTicks)
		{
			// no chão (errou, ou o tiro forçado no chão) ou no fim da vida: explode onde está
			Events.push_back({ EventType::ProjectileHit, P.Shooter, NoEntity, P.Pos, int32_t(P.Id) });
			if (W.HasSplash())
			{
				ApplySplash(P.Pos, P, NoEntity);
			}
			return false;
		}
		return true;
	}

	void World::ApplySplash(Vec2 At, const Projectile& P, EntityId Exclude)
	{
		// (v4, 22/10) o dano em área: a distância é da borda (o casco da unidade, o lote do prédio), a queda é em linha reta e
		// inteira (cheio até SplashFull, SplashEdgePct % em Splash, nada além: sem degrau no Full). Só os inimigos do dono do
		// tiro levam (sem fogo amigo por enquanto: a batalha lê limpa e a IA não precisa se cuidar); a fundação leva (tem 1 PV)
		// e não dá XP (o ResolveDamage confere). A soma do XP não depende da ordem, então a ordem das caixas da vizinhança vale
		const WeaponDef& W = RulesData.Units[P.Def].Weapon;
		if (P.Owner >= Players.size() || !W.HasSplash())
		{
			return;
		}
		// a busca cobre a quina do maior prédio (o centro dele pode estar a meia diagonal da borda que a explosão toca)
		Fix Reach = MaxBodyRadius;
		for (const UnitDef& D : RulesData.Units)
		{
			if (D.IsStructure())
			{
				Reach = Max(Reach, D.HalfX + D.HalfY);
			}
		}
		const Fix Span = W.Splash - W.SplashFull;
		ForEachNear(At, W.Splash + Reach, [&](int32_t J)
		{
			if (U.Hp[J] <= 0 || U.Id[J] == Exclude || !AreEnemies(P.Owner, U.Owner[J]))
			{
				return;
			}
			Fix D;
			if (DefOf(J).IsStructure())
			{
				Fix HX;
				Fix HY;
				HalfExtents(U.Def[J], DirOf(J), HX, HY);
				D = RectDistance(At, U.Pos[J], HX, HY);
			}
			else
			{
				D = Max(Distance(At, U.Pos[J]) - DefOf(J).Radius, Fix::Zero());
			}
			if (D > W.Splash)
			{
				return;
			}
			const int64_t Pct = D <= W.SplashFull ? 100
				: (Span.Raw > 0 ? 100 - int64_t(100 - W.SplashEdgePct) * (D - W.SplashFull).Raw / Span.Raw : W.SplashEdgePct);
			const int32_t Amount = int32_t(int64_t(P.Damage) * Pct / 100);
			if (Amount > 0)
			{
				PendingDamage.push_back({ P.Shooter, U.Id[J], Amount, P.Type, P.Owner });
			}
		});
	}

	void World::UpdateProjectiles()
	{
		std::vector<Projectile> Flying;
		Flying.reserve(Projectiles.size());
		for (Projectile& P : Projectiles)
		{
			if (P.bMissile)
			{
				if (FlyMissile(P))
				{
					Flying.push_back(P);
				}
				continue;
			}
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
					PendingDamage.push_back({ P.Shooter, P.Target, P.Damage, P.Type, P.Owner });
				}
				Events.push_back({ EventType::ProjectileHit, P.Shooter, P.Target, P.Aim, int32_t(P.Id) });
				if (RulesData.Units[P.Def].Weapon.HasSplash())
				{
					ApplySplash(P.Aim, P, T >= 0 ? P.Target : NoEntity);   // (v4) o projétil reto com área (nenhum hoje)
				}
				continue;
			}
			P.Pos += (P.Aim - P.Pos).Normalized() * P.Step;
			Flying.push_back(P);
		}
		Projectiles.swap(Flying);
	}

	void World::AfterShot(int32_t I, const WeaponDef& W)
	{
		// a rajada (o ClipSize do ZH, a Patriot: 4 mísseis a cada 250 ms e 2 s de recarga): o último tiro espera a recarga
		if (W.ClipSize > 0)
		{
			if (--U.Clip[I] <= 0)
			{
				U.Clip[I] = W.ClipSize;
				U.Cooldown[I] = W.ClipReloadTicks;
				return;
			}
		}
		U.Cooldown[I] = W.ReloadTicks;
	}

	void World::UpdateWeapons()
	{
		// o tiro sai: o do tanque da boca, reto; o míssil (16/10, a Patriot) de uma das 4 caixas (2 × 2), inclinado, com o
		// rumo aberto em leque pela caixa: os 4 da rajada saem separados e se juntam no alvo
		const auto Fire = [this](int32_t I, const WeaponDef& W, EntityId Target, Vec2 Aim)
		{
			const int32_t Tube = W.ClipSize > 0 ? (W.ClipSize - U.Clip[I]) & 3 : 0;
			AfterShot(I, W);
			Projectile P;
			P.Id = NextProjectileId++;
			P.Shooter = U.Id[I];
			P.Target = Target;
			P.Aim = Aim;
			P.Step = W.ProjectileStep;
			P.Damage = W.Damage;
			P.Type = W.Type;
			P.Def = U.Def[I];
			P.Owner = U.Owner[I];   // (v4) o XP vai pro dono mesmo com o atirador morto
			const Vec2 Fwd = Vec2::FromAngle(TurretWorld(I));
			if (!W.IsMissile())
			{
				P.Pos = U.Pos[I] + Fwd * DefOf(I).Radius;
			}
			else
			{
				const int32_t Col = (Tube & 1) ? 1 : -1;   // esquerda, direita
				const int32_t Row = Tube >> 1;            // baixo, cima
				const Vec2 Right = Vec2::Make(-Fwd.Y, Fwd.X);
				P.bMissile = 1;
				if (W.HasLaunchMouth())
				{
					// (v4, 23/10) da boca do tubo levantado (o Tomahawk): o pivô da torre no casco, a boca à frente dele no giro da
					// torre, os tubos alternando de lado, na altura da boca
					const Vec2 HullFwd = Vec2::FromAngle(U.Hull[I]);
					const Vec2 HullRight = Vec2::Make(-HullFwd.Y, HullFwd.X);
					P.Pos = U.Pos[I] + HullFwd * W.LaunchPivot.X + HullRight * W.LaunchPivot.Y + Fwd * W.LaunchForward + Right * (W.LaunchSide * int64_t(Col));
					P.Z = GroundHeightAt(U.Pos[I]) + W.LaunchUp;
				}
				else
				{
					P.Pos = U.Pos[I] + Fwd * (DefOf(I).Radius / 4) + Right * Fix::FromMilli(550 * Col);
					P.Z = GroundHeightAt(U.Pos[I]) + W.MissileLaunchHeight + Fix::FromMilli(500 * Row);
				}
				P.AimZ = GroundHeightAt(Aim);
				P.Yaw = Angle::FromBam(int32_t(TurretWorld(I).Bam) + Col * (W.MissileSpreadBam >> Row));
				P.Pitch = W.MissileLaunchPitch;
				P.Speed = W.MissileLaunchStep;
			}
			Projectiles.push_back(P);
			Events.push_back({ EventType::WeaponFired, U.Id[I], Target, Aim, int32_t(P.Id) });
			// (16/10) a ajuda entre as defesas (o AssistedTargetingUpdate do ZH): as do mesmo tipo e do mesmo dono a até
			// AssistRange, livres e que alcançam o alvo, passam a mirar nele (o laser no desenho)
			const UnitDef& DI = DefOf(I);
			const int32_t T = IndexOf(Target);
			if (DI.IsStructure() && DI.AssistRange.Raw > 0 && T >= 0)
			{
				ForEachNear(U.Pos[I], DI.AssistRange, [&](int32_t J)
				{
					if (J == I || U.Def[J] != U.Def[I] || U.Owner[J] != U.Owner[I] || U.FireTarget[J] != NoEntity || U.BuildLeft[J] != 0
						|| (DefOf(J).bNeedsPower && Players[U.Owner[J]].IsLowPower()) || U.Order[J] == OrderType::AttackGround
						|| DistSqRaw(U.Pos[I], U.Pos[J]) > RadiusSqRaw(DI.AssistRange)
						|| DistSqRaw(U.Pos[J], U.Pos[T]) > RadiusSqRaw(DefOf(J).Weapon.Range + DefOf(T).Radius) || !SeesUnit(J, T))
					{
						return;
					}
					U.FireTarget[J] = Target;
					Events.push_back({ EventType::DefenseAssist, U.Id[I], U.Id[J], U.Pos[T], int32_t(Target) });
				});
			}
		};
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Cooldown[I] > 0)
			{
				--U.Cooldown[I];
			}
			if (DefOf(I).IsStructure() && (U.BuildLeft[I] != 0 || (DefOf(I).bNeedsPower && Players[U.Owner[I]].IsLowPower()) || IsSelling(I)))
			{
				continue;   // a defesa em obra, sem energia ou (v4) sendo vendida não atira
			}
			// (v4, 23/10) com a mira antes do tiro, só depois do relógio cheio (UpdateTurrets)
			const bool bAimed = DefOf(I).Weapon.PreAttackTicks <= 0 || U.AimTicks[I] >= DefOf(I).Weapon.PreAttackTicks;
			if (U.Order[I] == OrderType::AttackGround && U.Cooldown[I] == 0 && DefOf(I).HasWeapon() && bAimed)
			{
				// o tiro forçado no chão: no alcance e com a torre alinhada, o projétil vai até o ponto e não acerta ninguém
				const WeaponDef& W = DefOf(I).Weapon;
				const Vec2 Aim = U.OrderPoint[I];
				const int64_t D2 = DistSqRaw(U.Pos[I], Aim);
				// (v4) dentro do alcance mínimo a artilharia não atira nem no chão
				if (D2 <= RadiusSqRaw(W.Range) && D2 >= RadiusSqRaw(W.MinRange) && AbsInt(TurretWorld(I).DeltaTo((Aim - U.Pos[I]).Heading())) <= W.AimToleranceBam)
				{
					Fire(I, W, NoEntity, Aim);
				}
				continue;
			}
			const int32_t T = IndexOf(U.FireTarget[I]);
			if (T < 0 || U.Cooldown[I] > 0 || !bAimed)
			{
				continue;
			}
			const WeaponDef& W = DefOf(I).Weapon;
			const int64_t D2 = DistSqRaw(U.Pos[I], U.Pos[T]);
			if (D2 > RadiusSqRaw(W.Range + DefOf(T).Radius) || D2 < RadiusSqRaw(W.MinRange))
			{
				continue;   // fora do alcance, ou (v4) dentro do mínimo da artilharia
			}
			const Angle Bearing = (U.Pos[T] - U.Pos[I]).Heading();
			if (AbsInt(TurretWorld(I).DeltaTo(Bearing)) > W.AimToleranceBam)
			{
				continue;   // a torre ainda está girando: só atira alinhada (Vanguard ADR-0031)
			}
			Fire(I, W, U.Id[T], U.Pos[T]);
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
			// (v4, 22/10) a patente do general, só pela luta: o acerto vale C = o dano até a vida que o alvo tinha (o golpe
			// que passa da morte não conta; a vida cai a cada acerto do mesmo passo), em valor C × custo ÷ vida cheia. Quem
			// acertou ganha 1 XP por XpDealtDivisor $ e o dono do alvo 1 por XpTakenDivisor $ (perder ensina menos). Obra
			// inacabada, alvo próprio ou aliado e o que não custa nada valem 0 (a fundação de 1 PV não vira fazenda de XP)
			const int32_t HpBefore = U.Hp[T];
			const int32_t Credit = std::min(Amount, std::max(HpBefore, 0));
			const UnitDef& DT = DefOf(T);
			if (Credit > 0 && U.BuildLeft[T] == 0 && H.SourcePlayer < Players.size() && AreEnemies(H.SourcePlayer, U.Owner[T]) && DT.Cost > 0 && DT.MaxHp > 0)
			{
				const EconomyRules& E = RulesData.Economy;
				const int64_t Value1000 = int64_t(Credit) * DT.Cost * 1000;
				if (E.XpDealtDivisor > 0)
				{
					Players[H.SourcePlayer].XpMilli += Value1000 / (int64_t(DT.MaxHp) * E.XpDealtDivisor);
				}
				if (E.XpTakenDivisor > 0)
				{
					Players[U.Owner[T]].XpMilli += Value1000 / (int64_t(DT.MaxHp) * E.XpTakenDivisor);
				}
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
						if (J != T && U.Owner[J] == U.Owner[T] && DefOf(J).HasWeapon() && U.Order[J] == OrderType::Idle && U.Engage[J] == NoEntity
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
		CheckRanks();
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Hp[I] <= 0)
			{
				// a obra cancelada (BuildLeft −1) some sem explosão; (v4) o prédio vendido (SellLeft −1) também, com o dinheiro que voltou
				if (U.SellLeft[I] < 0)
				{
					Events.push_back({ EventType::StructureSold, NoEntity, U.Id[I], U.Pos[I], SellRefund(U.Id[I]) | (int32_t(U.Owner[I]) << 24) });
					continue;
				}
				Events.push_back({ U.BuildLeft[I] < 0 ? EventType::StructureCancelled : EventType::UnitDied, U.LastAttacker[I], U.Id[I], U.Pos[I], U.Owner[I] });
			}
		}
	}

	void World::CheckRanks()
	{
		// (v4) uma subida por limiar, em ordem (um passo de muito XP sobe mais de uma, com um evento cada); quem caiu não sobe.
		// O limiar ≤ 0 acima da 1ª patente fecha a escada ali (as regras sem patente)
		const EconomyRules& E = RulesData.Economy;
		for (size_t P = 0; P < Players.size(); ++P)
		{
			PlayerState& S = Players[P];
			while (!S.bDefeated && S.Rank < MaxRank && E.RankXp[S.Rank] > 0 && S.XpMilli >= int64_t(E.RankXp[S.Rank]) * 1000)
			{
				++S.Rank;
				Events.push_back({ EventType::RankUp, NoEntity, NoEntity, PlayerStart(PlayerIndex(P)), int32_t(P) | (int32_t(S.Rank) << 8) });
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
			if (Keep[I])
			{
				continue;
			}
			const UnitDef& D = DefOf(int32_t(I));
			if (D.IsStructure())
			{
				SetFootprint(int32_t(I), false);   // a célula volta a ser o terreno
				// a fábrica que cai devolve o dinheiro da fila (ZH ProductionUpdate::onDie)
				for (const uint16_t K : U.Queue[I])
				{
					Players[U.Owner[I]].Money += RulesData.Units[K].Cost;
				}
				U.Queue[I].clear();
			}
			if (U.Cargo[I] > 0)
			{
				DropCargo(U.Pos[I], U.Cargo[I]);   // a carga que cai
				U.Cargo[I] = 0;
			}
			if (!D.IsStructure())
			{
				// o veículo vira carcaça no lugar, com o giro do casco (13/10)
				WreckState Wr;
				Wr.Id = U.Id[I];
				Wr.Def = U.Def[I];
				Wr.Pos = U.Pos[I];
				Wr.Hull = U.Hull[I];
				Wr.Born = CurrentTick;
				Wrecks.push_back(Wr);
				// (22/10) o teto de todas (morte em massa): a mais velha sai na hora, com o evento (o desenho a afunda depressa)
				while (Wrecks.size() > WreckHardCap)
				{
					EraseWreck(0, 1);
				}
			}
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
		Compact(U.OrderTarget, Keep); Compact(U.ForcedAttack, Keep); Compact(U.Engage, Keep); Compact(U.FireTarget, Keep);
		Compact(U.LastAttacker, Keep); Compact(U.TurretIdle, Keep); Compact(U.Path, Keep); Compact(U.Clip, Keep);
		Compact(U.PathIndex, Keep); Compact(U.PathGoal, Keep); Compact(U.NeedsPath, Keep);
		Compact(U.RepathWait, Keep); Compact(U.StuckTicks, Keep); Compact(U.StuckAnchor, Keep);
		Compact(U.EngageAnchor, Keep); Compact(U.Ignore, Keep); Compact(U.IgnoreUntil, Keep);
		Compact(U.LookCell, Keep); Compact(U.BuildLeft, Keep); Compact(U.Builder, Keep); Compact(U.Queue, Keep);
		Compact(U.QueueLeft, Keep); Compact(U.Rally, Keep); Compact(U.HasRally, Keep); Compact(U.Task, Keep);
		Compact(U.TaskTarget, Keep); Compact(U.TaskDock, Keep); Compact(U.TaskTimer, Keep); Compact(U.Cargo, Keep);
		Compact(U.AutoGather, Keep); Compact(U.Blocking, Keep); Compact(U.TaskPoint, Keep);
		Compact(U.ArriveFacing, Keep); Compact(U.HasArriveFacing, Keep); Compact(U.SpeedCap, Keep); Compact(U.IdleTurret, Keep);
		Compact(U.CrushSlowLeft, Keep); Compact(U.CrushSlowTotal, Keep); Compact(U.CrushSlowFloor, Keep);   // a freada de quem atropelou anda junto com a unidade
		Compact(U.IncomeLeft, Keep); Compact(U.IncomeCount, Keep); Compact(U.UnloadLeft, Keep);   // (v4)
		Compact(U.SellLeft, Keep); Compact(U.RepairAcc, Keep); Compact(U.AimTicks, Keep); Compact(U.AimIdle, Keep);   // (v4, 23/10)
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
		// com base, perde quem fica sem prédio pronto que conta (o CC e os outros da lista; ajuste ao
		// ZH, que conta até fundação: uma fundação escondida segurava o jogador vivo); sem base, como
		// no skirmish só de tanques, perde quem fica sem nada
		std::vector<int32_t> Alive(Players.size(), 0);
		std::vector<int32_t> Bases(Players.size(), 0);
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			++Alive[U.Owner[I]];
			if (DefOf(I).bCountsForVictory && IsComplete(I) && U.Hp[I] > 0)
			{
				++Bases[U.Owner[I]];
			}
		}
		for (size_t P = 0; P < Players.size(); ++P)
		{
			PlayerState& S = Players[P];
			S.bHasBase = S.bHasBase || Bases[P] > 0;
			if (!S.bDefeated && (S.bHasBase ? Bases[P] == 0 : Alive[P] == 0))
			{
				S.bDefeated = true;
				Events.push_back({ EventType::PlayerDefeated, NoEntity, NoEntity, Vec2::Zero(), int32_t(P) });
				// quem perde a base perde o resto (ZH VictoryConditions::killPlayer)
				for (int32_t I = 0; I < UnitCount(); ++I)
				{
					if (U.Owner[I] == PlayerIndex(P))
					{
						U.Hp[I] = 0;
					}
				}
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
			H.Add(P.Money);
			H.Add(P.PowerMade);
			H.Add(P.PowerUsed);
			H.Add(P.bHasBase);
			H.Add(uint64_t(P.Group.size()));
			for (const EntityId Id : P.Group)
			{
				H.Add(Id);
			}
			// (v4) a patente e os relógios dos poderes
			H.Add(P.XpMilli);
			H.Add(P.Rank);
			for (const TickNumber At : P.PowerReadyAt)
			{
				H.Add(At);
			}
		}
		H.Add(uint64_t(U.Id.size()));
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			H.Add(U.Id[I]); H.Add(U.Owner[I]); H.Add(U.Def[I]); H.Add(U.Pos[I]); H.Add(U.Hull[I]);
			H.Add(U.TurretRel[I]); H.Add(U.Speed[I]); H.Add(U.Hp[I]); H.Add(U.Cooldown[I]); H.Add(U.Clip[I]);
			H.Add(uint8_t(U.Order[I])); H.Add(U.OrderPoint[I]); H.Add(U.OrderTarget[I]); H.Add(U.ForcedAttack[I]); H.Add(U.Engage[I]);
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
			H.Add(U.BuildLeft[I]); H.Add(U.Builder[I]); H.Add(uint64_t(U.Queue[I].size()));
			for (const uint16_t K : U.Queue[I])
			{
				H.Add(K);
			}
			H.Add(U.QueueLeft[I]); H.Add(U.Rally[I]); H.Add(U.HasRally[I]); H.Add(uint8_t(U.Task[I]));
			H.Add(U.TaskTarget[I]); H.Add(U.TaskDock[I]); H.Add(U.TaskTimer[I]); H.Add(U.Cargo[I]);
			H.Add(U.AutoGather[I]); H.Add(U.Blocking[I]); H.Add(U.TaskPoint[I]);
			H.Add(U.ArriveFacing[I]); H.Add(U.HasArriveFacing[I]); H.Add(U.SpeedCap[I]); H.Add(U.IdleTurret[I]);
		}
		H.Add(uint64_t(Docks.size()));
		for (const SupplyDock& D : Docks)
		{
			H.Add(D.Pos); H.Add(D.Boxes); H.Add(D.bPile); H.Add(D.Occupant);
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
			H.Add(P.Id); H.Add(P.bMissile); H.Add(P.Z); H.Add(P.AimZ); H.Add(P.Yaw); H.Add(P.Pitch); H.Add(P.Speed); H.Add(P.Age); H.Add(P.Def);
			H.Add(P.Owner);   // (v4)
		}
		H.Add(NextProjectileId);
		H.Add(uint64_t(Props.size()));
		for (const PropState& P : Props)
		{
			H.Add(P.State); H.Add(P.Side); H.Add(P.FallDir); H.Add(P.FallLength); H.Add(P.At); H.Add(P.By);
		}
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			H.Add(U.CrushSlowLeft[I]); H.Add(U.CrushSlowTotal[I]); H.Add(U.CrushSlowFloor[I]);
		}
		H.Add(uint64_t(Wrecks.size()));
		for (const WreckState& Wr : Wrecks)
		{
			H.Add(Wr.Id); H.Add(Wr.Def); H.Add(Wr.Pos); H.Add(Wr.Hull); H.Add(Wr.Stuck); H.Add(Wr.Born); H.Add(Wr.SinkStart);
		}
		// (v4) a zona de lançamento (o avião e a empilhadeira) e os ataques de poder no ar
		for (size_t I = 0; I < U.Id.size(); ++I)
		{
			H.Add(U.IncomeLeft[I]); H.Add(U.IncomeCount[I]); H.Add(U.UnloadLeft[I]);
			H.Add(U.SellLeft[I]); H.Add(U.RepairAcc[I]); H.Add(U.AimTicks[I]); H.Add(U.AimIdle[I]);   // (v4, 23/10)
		}
		H.Add(uint64_t(Strikes.size()));
		for (const Strike& S : Strikes)
		{
			H.Add(S.Owner); H.Add(S.Power); H.Add(S.Total); H.Add(S.Fired); H.Add(S.Center); H.Add(S.From); H.Add(S.Radius); H.Add(S.Phase); H.Add(S.NextAt);
		}
		return H.Value();
	}

	// ---------------------------------------------------------------- o esmagar e o derrubar (16/10)

	bool KnocksOver(const UnitDef& D, Fix SpeedPerTick, int64_t EnergyJ)
	{
		// (v4, 23/10) ½·m·v² contra a energia que o objeto absorve até cair (World.h); a conta inteira em J, sem float
		if (D.MassKg <= 0 || SpeedPerTick.Raw <= 0)
		{
			return false;
		}
		const Fix V = SpeedPerTick * int64_t(TicksPerSecond);   // m/s
		return ((V * V) * int64_t(D.MassKg)).Floor() >= EnergyJ * 2;
	}

	namespace
	{
		// o freio (a 2ª opinião: o carro tira 5–10% da energia do tanque; 85% já exagera, pra ler): o piso e a volta
		constexpr int32_t CarSlowFloorPermille = 850;
		constexpr int32_t CarSlowTicks = 15;          // 0,5 s
		constexpr int32_t TreeSlowFloorPermille = 950;
		constexpr int32_t TreeSlowTicks = 6;          // 0,2 s
		constexpr int32_t RegionRelabelTicks = 30;
		// o objeto entra no casco: a caixa (meio comprimento × meia largura) no rumo do casco
		bool InHull(Vec2 P, Vec2 Center, Angle Hull, Fix HalfLength, Fix HalfWidth, Fix Pad, Fix& OutAlong, Fix& OutSide)
		{
			const Vec2 Fwd = Vec2::FromAngle(Hull);
			const Vec2 Rel = P - Center;
			OutAlong = Rel.Dot(Fwd);
			OutSide = Rel.X * (-Fwd.Y) + Rel.Y * Fwd.X;
			return Abs(OutAlong) <= HalfLength + Pad && Abs(OutSide) <= HalfWidth + Pad;
		}
	}

	int32_t World::MobilityOfDef(const UnitDef& D) const
	{
		// (v4, 23/10) quem não esmaga mas derruba a palmeira pelo choque na velocidade de cruzeiro (o caminhão, o Tomahawk) passa
		// pela célula da árvore (não pela do carro: a roda não sobe nele); o toque derruba a árvore em qualquer velocidade
		// (UpdateCrush: a mesma conta, nunca o casco dentro do tronco de pé)
		const int32_t Bits = D.CrusherLevel >= 2 ? SimMap::CrusherBit : (KnocksOver(D, D.MaxSpeed, TreeToppleEnergyJ) ? SimMap::ToppleBit : 0);
		return MapData.ClearanceFor(D.PathRadius) | Bits;
	}

	int32_t World::MobilityOf(int32_t Index) const
	{
		return MobilityOfDef(DefOf(Index));
	}

	int32_t World::GroupMobility(const std::vector<int32_t>& Members) const
	{
		int32_t Clearance = 1;
		bool bAllCrush = !Members.empty();
		bool bAllTopple = !Members.empty();   // (v4) a árvore só abre pro grupo se todos a esmagam ou derrubam
		for (const int32_t I : Members)
		{
			const int32_t M = MobilityOf(I);
			Clearance = std::max(Clearance, SimMap::ClearanceOf(M));
			bAllCrush = bAllCrush && (M & SimMap::CrusherBit) != 0;
			bAllTopple = bAllTopple && (M & (SimMap::CrusherBit | SimMap::ToppleBit)) != 0;
		}
		return Clearance | (bAllCrush ? SimMap::CrusherBit : (bAllTopple ? SimMap::ToppleBit : 0));
	}

	void World::InitProps(const std::vector<PropSetup>& Setup)
	{
		PropDefs = Setup;
		Props.assign(PropDefs.size(), PropState());
		PropCells.assign(PropDefs.size(), std::vector<int32_t>());
		PropStanding.assign(MapData.Pass.size(), 0);
		PropStandingCars.assign(MapData.Pass.size(), 0);
		const Fix Margin = Fix::FromMilli(500);
		for (size_t K = 0; K < PropDefs.size(); ++K)
		{
			const PropSetup& P = PropDefs[K];
			std::vector<int32_t>& Cells = PropCells[K];
			if (P.Kind == PropKind::Tree)
			{
				const Cell C = MapData.CellOf(P.Pos);
				if (MapData.InBounds(C.X, C.Y))
				{
					Cells.push_back(MapData.Index(C.X, C.Y));
				}
			}
			else
			{
				// o carro: as células com o centro dentro da caixa dele + 0,5 m (um carro de 4,6 × 2 m pega ~2 células de 4 m)
				const Fix Reach = Max(P.HalfLength, P.HalfWidth) + Margin;
				const Cell Lo = MapData.CellOf(P.Pos - Vec2::Make(Reach, Reach));
				const Cell Hi = MapData.CellOf(P.Pos + Vec2::Make(Reach, Reach));
				for (int32_t Y = Lo.Y; Y <= Hi.Y; ++Y)
				{
					for (int32_t X = Lo.X; X <= Hi.X; ++X)
					{
						Fix A, S;
						if (MapData.InBounds(X, Y) && InHull(MapData.CellCenter(Cell{ X, Y }), P.Pos, P.Facing, P.HalfLength, P.HalfWidth, Margin, A, S))
						{
							Cells.push_back(MapData.Index(X, Y));
						}
					}
				}
				if (Cells.empty())
				{
					const Cell C = MapData.CellOf(P.Pos);
					if (MapData.InBounds(C.X, C.Y))
					{
						Cells.push_back(MapData.Index(C.X, C.Y));
					}
				}
			}
			for (const int32_t Idx : Cells)
			{
				// (v4, 23/10) a célula do carro fica só de quem esmaga; a só da árvore, também de quem a derruba pelo choque
				const CellPass Was = CellPass(MapData.Pass[size_t(Idx)]);
				if (Was == CellPass::Open || Was == CellPass::Crushable || Was == CellPass::Toppleable)
				{
					PropStanding[size_t(Idx)] = uint8_t(std::min(int32_t(PropStanding[size_t(Idx)]) + 1, 255));
					if (P.Kind == PropKind::Car)
					{
						PropStandingCars[size_t(Idx)] = uint8_t(std::min(int32_t(PropStandingCars[size_t(Idx)]) + 1, 255));
					}
					MapData.Pass[size_t(Idx)] = uint8_t(PropStandingCars[size_t(Idx)] > 0 ? CellPass::Crushable : CellPass::Toppleable);
				}
			}
		}
	}

	void World::ReleaseProp(int32_t Prop)
	{
		bool bChanged = false;
		const bool bCar = PropDefs[size_t(Prop)].Kind == PropKind::Car;
		for (const int32_t Idx : PropCells[size_t(Prop)])
		{
			uint8_t& N = PropStanding[size_t(Idx)];
			if (N == 0)
			{
				continue;
			}
			--N;
			uint8_t& Cars = PropStandingCars[size_t(Idx)];
			if (bCar && Cars > 0)
			{
				--Cars;
			}
			// (v4) o que sobrou de pé diz a célula: carro, só árvore ou nada (o prédio em cima, Blocked, fica)
			const CellPass Now = N == 0 ? CellPass::Open : (Cars > 0 ? CellPass::Crushable : CellPass::Toppleable);
			for (uint8_t* Pass : { &TerrainPass[size_t(Idx)], &MapData.Pass[size_t(Idx)] })
			{
				const CellPass Was = CellPass(*Pass);
				if ((Was == CellPass::Crushable || Was == CellPass::Toppleable) && Was != Now)
				{
					*Pass = uint8_t(Now);
					bChanged = true;
				}
			}
		}
		if (bChanged)
		{
			// abrir célula só encurta caminho: ninguém precisa refazer o dele (a 2ª opinião: sem a tempestade de caminhos)
			MapData.ComputeClearance();
			bRegionsStale = true;
		}
	}

	Angle World::FreeFall(Vec2 From, Angle Want, Fix Height, Fix& OutLength) const
	{
		// o rumo pedido e, se bater em prédio, penhasco ou pedra, ±22,5°, ±45°, ±67,5° e ±90° (nessa ordem fixa); se todos
		// batem, o que vai mais longe (a árvore fica encostada)
		static constexpr int32_t Offsets[] = { 0, 4096, -4096, 8192, -8192, 12288, -12288, 16384, -16384 };
		const Fix Step = MapData.CellSize / 2;
		Angle Best = Want;
		Fix BestLen = Fix::FromInt(-1);
		for (const int32_t Off : Offsets)
		{
			const Angle Dir = Angle::FromBam(int32_t(Want.Bam) + Off);
			const Vec2 V = Vec2::FromAngle(Dir);
			Fix Len = Fix::Zero();
			while (Len < Height)
			{
				const Fix Next = Min(Len + Step, Height);
				const Cell C = MapData.CellOf(From + V * Next);
				const CellPass P = MapData.PassAt(C.X, C.Y);
				if (P == CellPass::Blocked)
				{
					break;
				}
				Len = Next;
			}
			if (Len >= Height)
			{
				OutLength = Height;
				return Dir;
			}
			if (Len > BestLen)
			{
				BestLen = Len;
				Best = Dir;
			}
		}
		OutLength = Max(BestLen, Fix::Zero());
		return Best;
	}

	void World::RelabelRegions()
	{
		for (size_t K = 0; K < RegionClearances.size(); ++K)
		{
			Pathfinder::LabelRegions(MapData, RegionClearances[K], RegionLabels[K]);
		}
		bRegionsStale = false;
		RegionsLabeledAt = CurrentTick;
	}

	void World::UpdateCrush()
	{
		if (!PropDefs.empty())
		{
			// o contato: o objeto de pé dentro da caixa do casco de quem esmaga e se mexeu neste passo (andou, girou ou foi
			// empurrado: o tanque que gira parado em cima do carro esmaga); o objeto pela ordem do índice, o esmagador de
			// menor Id ganha (o mesmo em todo PC)
			const Fix Pad = Fix::Zero();
			for (size_t K = 0; K < PropDefs.size(); ++K)
			{
				const PropSetup& P = PropDefs[K];
				PropState& St = Props[K];
				const bool bTree = P.Kind == PropKind::Tree;
				if ((bTree && St.State != 0) || (!bTree && St.State == 3))
				{
					continue;
				}
				const Fix Reach = (bTree ? P.HalfLength : P.HalfLength + P.HalfWidth) + MaxUnitRadius * 2;
				// o carro: os 3 pontos do ZH no eixo dele (frente, meio, traseira, a ±1/4 do comprimento)
				const Vec2 Axis = Vec2::FromAngle(P.Facing);
				const Vec2 Points[3] = { P.Pos + Axis * (P.HalfLength / 2), P.Pos, P.Pos - Axis * (P.HalfLength / 2) };
				EntityId PointBy[3] = { NoEntity, NoEntity, NoEntity };
				int32_t PointUnit[3] = { -1, -1, -1 };
				Fix PointSide[3];
				EntityId TreeBy = NoEntity;
				int32_t TreeUnit = -1;
				Fix TreeAlong, TreeSide;
				ForEachNear(P.Pos, Reach, [&](int32_t J)
				{
					const UnitDef& DJ = DefOf(J);
					// o carro só pra quem esmaga (o nível do ZH); (v4, 23/10) a árvore também pra quem bate com energia pra derrubar.
					// (revisão de 23/10) A energia é a da velocidade de cruzeiro, a mesma que abre a célula da árvore no caminho
					// (MobilityOfDef): quem tem massa e motor pra derrubá-la no choque também a empurra até cair num toque lento
					// (separado pelo aliado parado, ou freado na curva). Pela velocidade da hora, o caminhão a menos de 2,8 m/s
					// entrava na célula aberta e atravessava o tronco de pé
					const bool bCrushes = DJ.CrusherLevel > int32_t(P.CrushableLevel);
					if (DJ.IsStructure() || !(bCrushes || (bTree && KnocksOver(DJ, DJ.MaxSpeed, TreeToppleEnergyJ))) || size_t(J) >= PrePos.size())
					{
						return;
					}
					if (U.Pos[J] == PrePos[size_t(J)] && U.Hull[J] == PreHull[size_t(J)])
					{
						return;   // parado: não esmaga (o ZH também exige o esmagador andando)
					}
					if (bTree)
					{
						Fix A, S;
						if (InHull(P.Pos, U.Pos[J], U.Hull[J], DJ.HullHalfLength, DJ.HullHalfWidth, P.HalfLength, A, S) && (TreeBy == NoEntity || U.Id[J] < TreeBy))
						{
							TreeBy = U.Id[J];
							TreeUnit = J;
							TreeAlong = A;
							TreeSide = S;
						}
						return;
					}
					for (int32_t Pt = 0; Pt < 3; ++Pt)
					{
						Fix A, S;
						if (InHull(Points[Pt], U.Pos[J], U.Hull[J], DJ.HullHalfLength, DJ.HullHalfWidth, Pad, A, S) && (PointBy[Pt] == NoEntity || U.Id[J] < PointBy[Pt]))
						{
							PointBy[Pt] = U.Id[J];
							PointUnit[Pt] = J;
							PointSide[Pt] = S;
						}
					}
				});
				if (bTree)
				{
					if (TreeUnit < 0)
					{
						continue;
					}
					const int32_t J = TreeUnit;
					const UnitDef& DJ = DefOf(J);
					// a normal da face do casco que bateu + 0,5 × a frente: de frente cai pra frente, de raspão pro lado
					const Vec2 Fwd = Vec2::FromAngle(U.Hull[J]);
					const Vec2 Right = Vec2::Make(-Fwd.Y, Fwd.X);
					const bool bEnd = (Abs(TreeAlong) - DJ.HullHalfLength) > (Abs(TreeSide) - DJ.HullHalfWidth);
					const Vec2 Normal = bEnd ? (TreeAlong.Raw >= 0 ? Fwd : -Fwd) : (TreeSide.Raw >= 0 ? Right : -Right);
					const Vec2 Push = Normal + Fwd * Fix::Half();
					Fix Length;
					St.FallDir = FreeFall(P.Pos, Push == Vec2::Zero() ? U.Hull[J] : Push.Heading(), P.Height, Length);
					St.FallLength = Length;
					St.State = 1;
					St.At = CurrentTick;
					St.By = U.Id[J];
					ReleaseProp(int32_t(K));
					if (U.CrushSlowLeft[J] < TreeSlowTicks)
					{
						U.CrushSlowLeft[J] = U.CrushSlowTotal[J] = TreeSlowTicks;
						U.CrushSlowFloor[J] = U.CrushSlowLeft[J] > 0 ? std::min(U.CrushSlowFloor[J], TreeSlowFloorPermille) : TreeSlowFloorPermille;
					}
					const int64_t SpeedCm = (U.Speed[J] * Fix::FromInt(int64_t(TicksPerSecond) * 100)).Round();
					Events.push_back({ EventType::PropToppled, U.Id[J], EntityId(K), P.Pos,
						int32_t(St.FallDir.Bam) | int32_t((SpeedCm < 0 ? 0 : (SpeedCm > 32767 ? 32767 : SpeedCm)) << 16) });
					continue;
				}
				// o carro: o meio esmaga os dois lados; a frente e a traseira, o seu
				const uint8_t Before = St.State;
				for (int32_t Pt = 0; Pt < 3; ++Pt)
				{
					const int32_t J = PointUnit[Pt];
					if (J < 0)
					{
						continue;
					}
					const uint8_t Bits = Pt == 0 ? 1 : (Pt == 2 ? 2 : 3);
					const uint8_t New = uint8_t(Bits & ~St.State);
					if (New == 0)
					{
						continue;
					}
					St.State = uint8_t(St.State | New);
					St.Side = PointSide[Pt].Raw > 0 ? 1 : 0;
					St.At = CurrentTick;
					St.By = U.Id[J];
					U.CrushSlowLeft[J] = U.CrushSlowTotal[J] = CarSlowTicks;
					U.CrushSlowFloor[J] = CarSlowFloorPermille;
					Events.push_back({ EventType::PropCrushed, U.Id[J], EntityId(K), Points[Pt], int32_t(New) | (int32_t(St.Side) << 2) | (int32_t(U.Hull[J].Bam) << 16) });
				}
				if (Before == 0 && St.State != 0)
				{
					// achatou: a célula abre já no primeiro esmagar (a carcaça que morrer em cima não trava)
					ReleaseProp(int32_t(K));
				}
			}
		}
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.CrushSlowLeft[I] > 0 && --U.CrushSlowLeft[I] == 0)
			{
				U.CrushSlowFloor[I] = 1000;
			}
		}
		if (bRegionsStale && CurrentTick - RegionsLabeledAt >= TickNumber(RegionRelabelTicks))
		{
			RelabelRegions();
		}
	}
}
