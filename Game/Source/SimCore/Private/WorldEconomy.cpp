#include "Sim/World.h"
#include "Sim/Pathfinder.h"

#include <algorithm>

// A economia e a construção do ZH dentro da simulação: prédio na grade, obra do Dozer,
// fila de produção, energia, o vaivém do caminhão de suprimentos e a carga que cai.
// Números e regras do ZH 1.04 (docs/ECONOMIA.md); as mesmas regras do World.cpp: só
// inteiro e ponto fixo, varredura em ordem de ID, desempate pelo ID.

namespace Sim
{
	namespace
	{
		/** O caminhão à toa procura depósito a cada 0,5 s, em turnos espalhados pelo ID. */
		constexpr int32_t GatherScanInterval = 15;
		/** Dozer ou caminhão que parou longe do destino tenta o caminho de novo depois de 1 s. */
		constexpr int32_t RetryTicks = 30;
		/** Sem ponto de encontro, a unidade nova para a esta distância além da frente do prédio: o
		 *  NaturalRallyPoint do ZH fica na borda (War Factory: X 53 = meio comprimento) e a unidade
		 *  anda mais 20 u na mesma direção (DefaultProductionExitUpdate) = 5,3 m. */
		constexpr Fix NaturalRallyDistance = Fix::FromMilli(5300);
		/** Sem depósito ao alcance, o caminhão espera a até isto do centro (ZH: 100 u = 26 m). */
		constexpr Fix IdleNearCenter = Fix::FromInt(26);
		/** Chegou no ponto de encaixe (entrada, saída, porta): a última célula do caminho fica a até ~2,8 m do ponto. */
		constexpr Fix LaneArrive = Fix::FromInt(3);
		/** Custo do passo no campo de distância dos depósitos: reto e diagonal (o mesmo do A*). */
		constexpr int32_t StraightCost = 10;
		constexpr int32_t DiagonalCost = 14;

		Vec2 DirVector(int32_t Dir)
		{
			switch (Dir & 3)
			{
			case 0: return Vec2::Make(Fix::One(), Fix::Zero());
			case 1: return Vec2::Make(Fix::Zero(), Fix::One());
			case 2: return Vec2::Make(-Fix::One(), Fix::Zero());
			default: return Vec2::Make(Fix::Zero(), -Fix::One());
			}
		}

		bool Contains(const std::vector<uint16_t>& List, uint16_t V)
		{
			return std::find(List.begin(), List.end(), V) != List.end();
		}

		/** Dois retângulos (centro e meio lado) se sobrepõem? Encostar não conta. */
		bool RectsOverlap(Vec2 A, Fix AX, Fix AY, Vec2 B, Fix BX, Fix BY)
		{
			return Abs(A.X - B.X) < AX + BX && Abs(A.Y - B.Y) < AY + BY;
		}

		/** A folga entre dois retângulos (0 se se tocam ou se sobrepõem). */
		Fix RectGap(Vec2 A, Fix AX, Fix AY, Vec2 B, Fix BX, Fix BY)
		{
			const Fix DX = Max(Abs(A.X - B.X) - (AX + BX), Fix::Zero());
			const Fix DY = Max(Abs(A.Y - B.Y) - (AY + BY), Fix::Zero());
			return Vec2::Make(DX, DY).Length();
		}
	}

	// ---------------------------------------------------------------- leitura

	void World::QueueOf(EntityId Structure, std::vector<uint16_t>& Out) const
	{
		const int32_t S = IndexOf(Structure);
		Out = S >= 0 ? U.Queue[S] : std::vector<uint16_t>();
	}

	DockView World::Dock(int32_t Index) const
	{
		DockView V;
		if (Index >= 0 && Index < int32_t(Docks.size()))
		{
			const SupplyDock& D = Docks[size_t(Index)];
			V.Pos = D.Pos;
			V.HalfX = D.HalfX;
			V.HalfY = D.HalfY;
			V.Boxes = D.Boxes;
			V.StartBoxes = D.StartBoxes;
			V.bPile = D.bPile;
			V.Occupant = D.Occupant;
		}
		return V;
	}

	int32_t World::PowerFactorPermille(PlayerIndex P) const
	{
		// ZH (ThingTemplate::calcTimeToBuild, Energy::getEnergySupplyRatio): r = gerada ÷ gasta;
		// r ≥ 1 normal; abaixo disso o ritmo é r, preso entre 0,5 e 0,8. Sem gasto, a razão é a
		// própria geração: sem usina nenhuma (0/0) dá metade, como no ZH (decisão do Felipe, 10/10)
		const PlayerState& S = Players[P];
		const EconomyRules& E = RulesData.Economy;
		int64_t Ratio = 0;
		if (S.PowerUsed == 0)
		{
			Ratio = S.PowerMade > 0 ? 1000 : 0;
		}
		else
		{
			Ratio = int64_t(S.PowerMade) * 1000 / S.PowerUsed;
		}
		if (Ratio >= 1000)
		{
			return 1000;
		}
		return int32_t(std::min<int64_t>(std::max<int64_t>(Ratio, E.LowPowerMinPermille), E.LowPowerMaxPermille));
	}

	bool World::HasPrerequisites(PlayerIndex P, uint16_t Def) const
	{
		for (const uint16_t R : RulesData.Units[Def].Requires)
		{
			bool bFound = false;
			for (int32_t I = 0; I < UnitCount() && !bFound; ++I)
			{
				bFound = U.Owner[I] == P && U.Def[I] == R && IsComplete(I) && U.Hp[I] > 0;
			}
			if (!bFound)
			{
				return false;
			}
		}
		return true;
	}

	// ---------------------------------------------------------------- o prédio na grade

	void World::HalfExtents(uint16_t Def, int32_t Dir, Fix& OutX, Fix& OutY) const
	{
		const UnitDef& D = RulesData.Units[Def];
		OutX = (Dir & 1) ? D.HalfY : D.HalfX;
		OutY = (Dir & 1) ? D.HalfX : D.HalfY;
	}

	void World::FootprintCells(uint16_t Def, int32_t Dir, Vec2 Center, int32_t& X0, int32_t& Y0, int32_t& X1, int32_t& Y1) const
	{
		Fix HX;
		Fix HY;
		HalfExtents(Def, Dir, HX, HY);
		// o prédio ocupa células inteiras: o lado arredonda pra cima e o canto cai na célula mais perto
		const int64_t Cell = MapData.CellSize.Raw;
		const int32_t NX = int32_t((HX.Raw * 2 + Cell - 1) / Cell);
		const int32_t NY = int32_t((HY.Raw * 2 + Cell - 1) / Cell);
		const Vec2 Local = Center - MapData.Origin;
		X0 = int32_t((Local.X / MapData.CellSize - Fix::FromRatio(NX, 2)).Round());
		Y0 = int32_t((Local.Y / MapData.CellSize - Fix::FromRatio(NY, 2)).Round());
		X1 = X0 + NX;
		Y1 = Y0 + NY;
	}

	Vec2 World::SnapStructure(uint16_t Def, int32_t Dir, Vec2 Center) const
	{
		if (Def >= RulesData.Units.size() || !RulesData.Units[Def].IsStructure())
		{
			return Center;
		}
		int32_t X0, Y0, X1, Y1;
		FootprintCells(Def, Dir, Center, X0, Y0, X1, Y1);
		return Vec2::Make(MapData.Origin.X + MapData.CellSize * X0 + MapData.CellSize * (X1 - X0) / 2,
			MapData.Origin.Y + MapData.CellSize * Y0 + MapData.CellSize * (Y1 - Y0) / 2);
	}

	Fix World::RectDistance(Vec2 P, Vec2 Center, Fix HalfX, Fix HalfY)
	{
		const Fix DX = Max(Abs(P.X - Center.X) - HalfX, Fix::Zero());
		const Fix DY = Max(Abs(P.Y - Center.Y) - HalfY, Fix::Zero());
		return Vec2::Make(DX, DY).Length();
	}

	bool World::ExitLaneRect(uint16_t Def, int32_t Dir, Vec2 Center, Vec2& OutCenter, Fix& OutHalfX, Fix& OutHalfY) const
	{
		const UnitDef& D = RulesData.Units[Def];
		if (D.ExitLane.Raw <= 0)
		{
			return false;
		}
		Fix HX;
		Fix HY;
		HalfExtents(Def, Dir, HX, HY);
		// a faixa cobre a frente inteira do prédio e tem a profundidade da saída (ZH FactoryExitWidth)
		const Fix Depth = (Dir & 1) ? HY : HX;
		const Fix Half = D.ExitLane / 2;
		OutCenter = Center + DirVector(Dir) * (Depth + Half);
		OutHalfX = (Dir & 1) ? HX : Half;
		OutHalfY = (Dir & 1) ? Half : HY;
		return true;
	}

	PlaceResult World::CanPlace(PlayerIndex P, uint16_t Def, int32_t Dir, Vec2 Center) const
	{
		// as regras do ZH (BuildAssistant::isLocationLegalToBuild), na ordem em que o fantasma explica
		if (P >= Players.size() || Def >= RulesData.Units.size() || !RulesData.Units[Def].IsStructure())
		{
			return PlaceResult::NotAStructure;
		}
		const UnitDef& D = RulesData.Units[Def];
		const EconomyRules& E = RulesData.Economy;
		const Vec2 C = SnapStructure(Def, Dir, Center);
		int32_t X0, Y0, X1, Y1;
		FootprintCells(Def, Dir, C, X0, Y0, X1, Y1);
		if (X0 < 0 || Y0 < 0 || X1 > MapData.Width || Y1 > MapData.Height)
		{
			return PlaceResult::OutOfMap;
		}
		const int32_t Margin = int32_t((E.EdgeMargin.Raw + MapData.CellSize.Raw - 1) / MapData.CellSize.Raw);
		if (X0 < Margin || Y0 < Margin || X1 > MapData.Width - Margin || Y1 > MapData.Height - Margin)
		{
			return PlaceResult::NearEdge;
		}
		Fix HX;
		Fix HY;
		HalfExtents(Def, Dir, HX, HY);
		for (int32_t Y = Y0; Y < Y1; ++Y)
		{
			for (int32_t X = X0; X < X1; ++X)
			{
				if (CellPass(MapData.Pass[size_t(MapData.Index(X, Y))]) != CellPass::Open)
				{
					return PlaceResult::Blocked;
				}
			}
		}
		// as fundações ainda não marcaram a grade: o retângulo delas conta igual
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (!DefOf(I).IsStructure())
			{
				continue;
			}
			Fix SX;
			Fix SY;
			HalfExtents(U.Def[I], DirOf(I), SX, SY);
			if (RectsOverlap(C, HX, HY, U.Pos[I], SX, SY))
			{
				return PlaceResult::Blocked;
			}
		}
		if (MapData.HasHeights() && E.MaxFlatnessCm > 0)
		{
			int32_t Lo = 0x7FFF;
			int32_t Hi = -0x7FFF;
			for (int32_t Y = Y0; Y < Y1; ++Y)
			{
				for (int32_t X = X0; X < X1; ++X)
				{
					const size_t K = size_t(MapData.Index(X, Y));
					Lo = std::min<int32_t>(Lo, MapData.MinDm[K]);
					Hi = std::max<int32_t>(Hi, MapData.MaxDm[K]);
				}
			}
			if ((Hi - Lo) * 10 > E.MaxFlatnessCm)
			{
				return PlaceResult::NotFlat;
			}
		}
		// a folga dos depósitos do mapa: nenhum prédio encosta (a doca não pode ser cercada) e o
		// centro de suprimentos fica a pelo menos SupplyCenterDockGap (ZH SupplyCenterCloseRadius)
		const Fix DockClear = MapData.CellSize * E.DockClearCells;
		for (const SupplyDock& Dk : Docks)
		{
			if (Dk.bPile)
			{
				continue;
			}
			const Fix Gap = RectGap(C, HX, HY, Dk.Pos, Dk.HalfX, Dk.HalfY);
			if (Gap < DockClear || (D.bSupplyCenter && Gap < E.SupplyCenterDockGap))
			{
				return PlaceResult::NearSupplies;
			}
		}
		// a faixa de saída: nenhum prédio em cima da saída de uma fábrica, nem a saída nova em cima de algo
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			Vec2 LC;
			Fix LX;
			Fix LY;
			if (DefOf(I).IsStructure() && ExitLaneRect(U.Def[I], DirOf(I), U.Pos[I], LC, LX, LY) && RectsOverlap(C, HX, HY, LC, LX, LY))
			{
				return PlaceResult::BlocksExit;
			}
		}
		{
			Vec2 LC;
			Fix LX;
			Fix LY;
			if (ExitLaneRect(Def, Dir, C, LC, LX, LY))
			{
				int32_t LX0, LY0, LX1, LY1;
				const Vec2 Local = LC - MapData.Origin;
				LX0 = int32_t(((Local.X - LX) / MapData.CellSize).Floor());
				LY0 = int32_t(((Local.Y - LY) / MapData.CellSize).Floor());
				LX1 = int32_t(((Local.X + LX - Fix::FromRaw(1)) / MapData.CellSize).Floor()) + 1;
				LY1 = int32_t(((Local.Y + LY - Fix::FromRaw(1)) / MapData.CellSize).Floor()) + 1;
				for (int32_t Y = LY0; Y < LY1; ++Y)
				{
					for (int32_t X = LX0; X < LX1; ++X)
					{
						if (!MapData.InBounds(X, Y) || CellPass(MapData.Pass[size_t(MapData.Index(X, Y))]) != CellPass::Open)
						{
							return PlaceResult::BlocksExit;
						}
					}
				}
				for (int32_t I = 0; I < UnitCount(); ++I)
				{
					Fix SX;
					Fix SY;
					if (DefOf(I).IsStructure())
					{
						HalfExtents(U.Def[I], DirOf(I), SX, SY);
						if (RectsOverlap(LC, LX, LY, U.Pos[I], SX, SY))
						{
							return PlaceResult::BlocksExit;
						}
					}
				}
			}
		}
		const uint8_t Team = Players[P].Team;
		const Cell CenterCell = MapData.CellOf(C);
		if (bFogOfWar && !Vision.IsVisible(Team, MapData.Index(CenterCell.X, CenterCell.Y)))
		{
			return PlaceResult::NotVisible;
		}
		// unidade inimiga em cima bloqueia (amiga sai da frente quando o Dozer começa). Se o
		// time não vê quem está ali, o motivo é a névoa: "ocupado" seria espiar (TheSuperHackers)
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (DefOf(I).IsStructure() || !AreEnemies(P, U.Owner[I]) || RectDistance(U.Pos[I], C, HX, HY) >= DefOf(I).Radius)
			{
				continue;
			}
			const bool bSeen = !bFogOfWar || (U.LookCell[I] >= 0 && Vision.IsVisible(Team, U.LookCell[I]));
			return bSeen ? PlaceResult::Occupied : PlaceResult::NotVisible;
		}
		if (!HasPrerequisites(P, Def))
		{
			return PlaceResult::MissingPrereq;
		}
		if (Players[P].Money < D.Cost)
		{
			return PlaceResult::NoMoney;
		}
		return PlaceResult::Ok;
	}

	void World::BlockRect(int32_t X0, int32_t Y0, int32_t X1, int32_t Y1, bool bBlock)
	{
		for (int32_t Y = Y0; Y < Y1; ++Y)
		{
			for (int32_t X = X0; X < X1; ++X)
			{
				if (MapData.InBounds(X, Y))
				{
					const size_t K = size_t(MapData.Index(X, Y));
					MapData.Pass[K] = bBlock ? uint8_t(CellPass::Blocked) : TerrainPass[K];
				}
			}
		}
		MapData.ComputeClearance();
		bGridChanged = true;
	}

	void World::SetFootprint(int32_t Index, bool bBlock)
	{
		if (U.Blocking[Index] == uint8_t(bBlock ? 1 : 0))
		{
			return;
		}
		int32_t X0, Y0, X1, Y1;
		FootprintCells(U.Def[Index], DirOf(Index), U.Pos[Index], X0, Y0, X1, Y1);
		BlockRect(X0, Y0, X1, Y1, bBlock);
		U.Blocking[Index] = bBlock ? 1 : 0;
	}

	Vec2 World::LotExit(Vec2 P, Vec2 Center, Fix HX, Fix HY, Fix Margin, int32_t MinClearance) const
	{
		// os 4 lados do retângulo, do mais perto pro mais longe (empate: esquerda, direita, baixo,
		// cima), e um pouco além dele; sai pelo primeiro com chão livre do outro lado. Antes era só o
		// mais perto: no lan2v2 (11/10) ele dava dentro do centro de suprimentos recém-construído, o
		// Dozer não achava caminho e ficava parado no lote da fábrica pra sempre
		struct FSide
		{
			Fix Dist;
			Vec2 Exit;
		};
		FSide Sides[4] = {
			{ P.X - (Center.X - HX), Vec2::Make(Center.X - HX - Margin, P.Y) },
			{ (Center.X + HX) - P.X, Vec2::Make(Center.X + HX + Margin, P.Y) },
			{ P.Y - (Center.Y - HY), Vec2::Make(P.X, Center.Y - HY - Margin) },
			{ (Center.Y + HY) - P.Y, Vec2::Make(P.X, Center.Y + HY + Margin) },
		};
		for (int32_t A = 1; A < 4; ++A)
		{
			for (int32_t B = A; B > 0 && Sides[B].Dist < Sides[B - 1].Dist; --B)
			{
				const FSide Swap = Sides[B];
				Sides[B] = Sides[B - 1];
				Sides[B - 1] = Swap;
			}
		}
		for (const FSide& Side : Sides)
		{
			const Vec2 Exit = ClampToMap(Side.Exit);
			const Cell C = MapData.CellOf(Exit);
			if (MapData.IsWalkable(C.X, C.Y, MinClearance))
			{
				return Exit;
			}
		}
		// nenhum lado livre: o chão andável mais perto da borda mais perto
		const Vec2 Nearest = ClampToMap(Sides[0].Exit);
		Cell Free;
		if (Pathfinder::NearestWalkable(MapData, MapData.CellOf(Nearest), MinClearance, 16, Free))
		{
			return MapData.CellCenter(Free);
		}
		return Nearest;
	}

	bool World::ClaimFootprint(int32_t S)
	{
		Fix HX;
		Fix HY;
		HalfExtents(U.Def[S], DirOf(S), HX, HY);
		bool bClear = true;
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (DefOf(I).IsStructure() || RectDistance(U.Pos[I], U.Pos[S], HX, HY) >= DefOf(I).Radius)
			{
				continue;
			}
			bClear = false;
			// amigo livre sai da frente (ZH): anda pra fora, na direção em que já está
			if (U.Owner[I] == U.Owner[S] && U.Order[I] == OrderType::Idle && U.Task[I] == UnitTask::None)
			{
				Vec2 Away = U.Pos[I] - U.Pos[S];
				if (Away == Vec2::Zero())
				{
					Away = Vec2::Make(Fix::One(), Fix::Zero());
				}
				const Fix Out = Max(HX, HY) + DefOf(I).Radius + MapData.CellSize * 2;
				GiveMove(I, ClampToMap(U.Pos[S] + Away.Normalized() * Out), OrderType::Move);
			}
		}
		if (bClear)
		{
			SetFootprint(S, true);
		}
		return bClear;
	}

	EntityId World::PlaceStructure(PlayerIndex Owner, uint16_t Def, int32_t Dir, Vec2 Center, bool bComplete)
	{
		const Vec2 C = SnapStructure(Def, Dir, Center);
		const EntityId Id = Spawn(Owner, Def, C, Angle::FromBam((Dir & 3) * Angle::QuarterTurn));
		const int32_t I = IndexOf(Id);
		const UnitDef& D = RulesData.Units[Def];
		if (bComplete || D.BuildTicks <= 0)
		{
			SetFootprint(I, true);   // prédio de largada: pronto e no chão
		}
		else
		{
			// a fundação nasce com 1 de vida e só ocupa a grade quando o Dozer começa (ajuste ao ZH:
			// senão a fundação de 100% de volta vira muro grátis na porta do inimigo, revisão de 10/10)
			U.BuildLeft[I] = std::max(1, D.BuildTicks) * 1000;
			U.Hp[I] = 1;
		}
		Events.push_back({ EventType::StructurePlaced, NoEntity, Id, C, Owner });
		return Id;
	}

	Vec2 World::ExitPoint(int32_t Structure, uint16_t UnitDefIndex) const
	{
		const int32_t Dir = DirOf(Structure);
		Fix HX;
		Fix HY;
		HalfExtents(U.Def[Structure], Dir, HX, HY);
		const Fix Depth = (Dir & 1) ? HY : HX;
		const UnitDef& UD = RulesData.Units[UnitDefIndex];
		Vec2 P = U.Pos[Structure] + DirVector(Dir) * (Depth + UD.Radius + MapData.CellSize / 2);
		const int32_t Clearance = MapData.ClearanceFor(UD.PathRadius);
		const Cell C = MapData.CellOf(P);
		Cell To;
		if (!MapData.IsWalkable(C.X, C.Y, Clearance) && Pathfinder::NearestWalkable(MapData, C, Clearance, 12, To))
		{
			P = MapData.CellCenter(To);
		}
		return ClampToMap(P);
	}

	EntityId World::SpawnFrom(int32_t Structure, uint16_t UnitDefIndex)
	{
		// tudo copiado antes: o Spawn aumenta os arrays
		const Vec2 Pos = ExitPoint(Structure, UnitDefIndex);
		const PlayerIndex Owner = U.Owner[Structure];
		const EntityId StructureId = U.Id[Structure];
		const int32_t Dir = DirOf(Structure);
		Fix HX;
		Fix HY;
		HalfExtents(U.Def[Structure], Dir, HX, HY);
		const Vec2 Natural = U.Pos[Structure] + DirVector(Dir) * (((Dir & 1) ? HY : HX) + NaturalRallyDistance);
		const bool bHasRally = U.HasRally[Structure] != 0;
		const Vec2 RallyPoint = bHasRally ? U.Rally[Structure] : Natural;
		const bool bTruck = RulesData.Units[UnitDefIndex].SupplyBoxes > 0;
		const EntityId Id = Spawn(Owner, UnitDefIndex, Pos, Angle::FromBam(Dir * Angle::QuarterTurn));
		const int32_t I = IndexOf(Id);
		U.AutoGather[I] = bTruck ? 1 : 0;
		if (!bTruck || bHasRally)
		{
			// o caminhão sem ponto de encontro vai direto trabalhar (ZH SupplyCenterProductionExitUpdate);
			// cada um vai pra uma vaga da área de reunião e chega virado pra longe da fábrica
			Angle Facing;
			int32_t LateralHalf = 0;
			const Vec2 Slot = StagingSlot(IndexOf(StructureId), I, ClampToMap(RallyPoint), Facing, &LateralHalf);
			GiveMove(I, Slot, OrderType::Move);
			U.ArriveFacing[I] = Facing;
			U.HasArriveFacing[I] = 1;
			if (DefOf(I).HasWeapon())
			{
				// o leque da área de reunião: as pontas da fileira olham pra fora (meio passo = 1/4 do leque)
				U.IdleTurret[I] = std::clamp(LateralHalf, -4, 4) * (Angle::FullTurn * 25 / 360) / 4;
			}
		}
		Events.push_back({ EventType::UnitProduced, StructureId, Id, Pos, Owner });
		return Id;
	}

	bool World::SpotFree(int32_t Unit, Vec2 Q) const
	{
		// o passo das vagas do grupo (2,34 raios: as 32 unidades do ZH pro Crusader de 30), com folga
		// de 15% pro tremido das vagas; conta quem já está parado ali e quem está indo pra lá (o
		// destino dele, como as células de objetivo do ZH)
		const UnitDef& D = DefOf(Unit);
		const Cell C = MapData.CellOf(Q);
		if (!MapData.IsWalkable(C.X, C.Y, MapData.ClearanceFor(D.PathRadius)))
		{
			return false;
		}
		const int64_t MinSq = RadiusSqRaw(D.Radius * Fix::FromMilli(2000));
		for (int32_t J = 0; J < UnitCount(); ++J)
		{
			if (J == Unit || U.Owner[J] != U.Owner[Unit] || U.Hp[J] <= 0 || DefOf(J).IsStructure())
			{
				continue;
			}
			const Vec2 Claim = U.Order[J] == OrderType::Move ? U.OrderPoint[J] : U.Pos[J];
			if (DistSqRaw(Claim, Q) < MinSq)
			{
				return false;
			}
		}
		return true;
	}

	Vec2 World::StagingSlot(int32_t Structure, int32_t Unit, Vec2 Rally, Angle& OutFacing, int32_t* OutLateralHalf) const
	{
		// a frente: da fábrica pro ponto; as fileiras de 4 enchem do ponto pra trás (o que chega
		// depois não precisa passar pelo meio dos que já estão lá), a de trás meio passo de lado
		const UnitDef& D = DefOf(Unit);
		const Vec2 FromFactory = Rally - U.Pos[Structure];
		const Angle Front = DistSqRaw(Rally, U.Pos[Structure]) > RadiusSqRaw(MapData.CellSize)
			? FromFactory.Heading() : Angle::FromBam(DirOf(Structure) * Angle::QuarterTurn);
		const Vec2 F = Vec2::FromAngle(Front);
		const Vec2 R = Vec2::Make(-F.Y, F.X);
		const Fix Step = D.Radius * Fix::FromMilli(2340);
		static const int32_t Lateral[4] = { -1, 1, -3, 3 };   // em meios passos: o meio primeiro, depois as pontas
		const uint32_t Seed = uint32_t(U.Id[Structure]) * 2654435761u;
		for (int32_t K = 0; K < 24; ++K)
		{
			const int32_t Row = K / 4;
			const int32_t Half = Lateral[K % 4] + (Row & 1);
			Vec2 P = Rally + R * (Step * Fix::FromRatio(Half, 2)) - F * (Step * Fix::FromInt(Row));
			// o tremido fixo da vaga (hash da fábrica e da vaga, sem estado de sorteio): ±0,3 m e ±4°
			// (com mais, duas vagas vizinhas chegavam a menos de 2 raios e uma era pulada)
			uint32_t H = Seed ^ (uint32_t(K + 1) * 40503u);
			H ^= H >> 15; H *= 2246822519u; H ^= H >> 13;
			P = P + F * Fix::FromMilli(int64_t(H % 601u) - 300) + R * Fix::FromMilli(int64_t((H >> 10) % 601u) - 300);
			P = ClampToMap(P);
			if (SpotFree(Unit, P))
			{
				OutFacing = Angle::FromBam(int32_t(Front.Bam) + int32_t((H >> 20) % 1457u) - 728);
				if (OutLateralHalf)
				{
					*OutLateralHalf = Half;
				}
				return P;
			}
		}
		OutFacing = Front;
		return FreeSpotNear(Unit, Rally);
	}

	Vec2 World::FreeSpotNear(int32_t Unit, Vec2 P) const
	{
		const auto IsFree = [&](Vec2 Q) { return SpotFree(Unit, Q); };
		if (IsFree(P))
		{
			return P;
		}
		// a espiral do ZH (direita, cima, esquerda, baixo; o lado cresce a cada duas voltas), em células
		const Cell Center = MapData.CellOf(P);
		int32_t X = Center.X;
		int32_t Y = Center.Y;
		int32_t Segment = 1;
		const int32_t Dirs[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
		for (int32_t Steps = 0; Steps < 24 * 24;)
		{
			for (int32_t K = 0; K < 4; ++K)
			{
				for (int32_t N = 0; N < Segment; ++N, ++Steps)
				{
					X += Dirs[K][0];
					Y += Dirs[K][1];
					if (MapData.InBounds(X, Y))
					{
						const Vec2 Q = MapData.CellCenter(Cell{ X, Y });
						if (IsFree(Q))
						{
							return Q;
						}
					}
				}
				if (K & 1)
				{
					++Segment;
				}
			}
		}
		return P;
	}

	void World::CompleteStructure(int32_t Index)
	{
		U.BuildLeft[Index] = 0;
		U.Builder[Index] = NoEntity;
		const EntityId Id = U.Id[Index];
		Events.push_back({ EventType::StructureCompleted, NoEntity, Id, U.Pos[Index], U.Owner[Index] });
		const std::vector<uint16_t> Free = DefOf(Index).FreeUnits;
		for (const uint16_t K : Free)
		{
			SpawnFrom(IndexOf(Id), K);
		}
	}

	// ---------------------------------------------------------------- depósitos

	void World::BuildDockField(SupplyDock& Dock) const
	{
		// Dijkstra inteiro a partir das células em volta do depósito, pela folga do maior
		// caminhão: "o mais perto" é pelo caminho (o ZH mede em linha reta e manda o caminhão
		// pro outro lado do morro; ajuste da revisão de 10/10). O terreno não muda, então uma vez só.
		Fix TruckRadius = Fix::Zero();
		for (const UnitDef& D : RulesData.Units)
		{
			if (D.SupplyBoxes > 0)
			{
				TruckRadius = Max(TruckRadius, D.PathRadius);
			}
		}
		const int32_t Clearance = MapData.ClearanceFor(TruckRadius);
		const int32_t N = MapData.Width * MapData.Height;
		Dock.Field.assign(size_t(N), -1);
		struct Entry
		{
			int32_t Cost;
			int32_t Index;
		};
		const auto Worse = [](const Entry& A, const Entry& B) { return A.Cost > B.Cost || (A.Cost == B.Cost && A.Index > B.Index); };
		std::vector<Entry> Heap;
		const Vec2 Local = Dock.Pos - MapData.Origin;
		const int32_t X0 = int32_t(((Local.X - Dock.HalfX) / MapData.CellSize).Floor()) - 1;
		const int32_t Y0 = int32_t(((Local.Y - Dock.HalfY) / MapData.CellSize).Floor()) - 1;
		const int32_t X1 = int32_t(((Local.X + Dock.HalfX) / MapData.CellSize).Floor()) + 2;
		const int32_t Y1 = int32_t(((Local.Y + Dock.HalfY) / MapData.CellSize).Floor()) + 2;
		for (int32_t Y = Y0; Y < Y1; ++Y)
		{
			for (int32_t X = X0; X < X1; ++X)
			{
				if (MapData.IsWalkable(X, Y, Clearance))
				{
					const int32_t K = MapData.Index(X, Y);
					Dock.Field[size_t(K)] = 0;
					Heap.push_back({ 0, K });
				}
			}
		}
		std::make_heap(Heap.begin(), Heap.end(), Worse);
		static constexpr int32_t DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static constexpr int32_t DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		while (!Heap.empty())
		{
			std::pop_heap(Heap.begin(), Heap.end(), Worse);
			const Entry E = Heap.back();
			Heap.pop_back();
			if (E.Cost > Dock.Field[size_t(E.Index)])
			{
				continue;
			}
			const int32_t CX = E.Index % MapData.Width;
			const int32_t CY = E.Index / MapData.Width;
			for (int32_t K = 0; K < 8; ++K)
			{
				const int32_t NX = CX + DX[K];
				const int32_t NY = CY + DY[K];
				if (!MapData.IsWalkable(NX, NY, Clearance))
				{
					continue;
				}
				if (K >= 4 && (!MapData.IsWalkable(CX + DX[K], CY, Clearance) || !MapData.IsWalkable(CX, CY + DY[K], Clearance)))
				{
					continue;   // não corta quina (como o caminho)
				}
				const int32_t NI = MapData.Index(NX, NY);
				const int32_t Cost = E.Cost + (K >= 4 ? DiagonalCost : StraightCost);
				if (Dock.Field[size_t(NI)] < 0 || Cost < Dock.Field[size_t(NI)])
				{
					Dock.Field[size_t(NI)] = Cost;
					Heap.push_back({ Cost, NI });
					std::push_heap(Heap.begin(), Heap.end(), Worse);
				}
			}
		}
	}

	Fix World::DockDistance(int32_t K, Vec2 From) const
	{
		const SupplyDock& D = Docks[size_t(K)];
		if (D.Field.empty())
		{
			return Distance(From, D.Pos);   // pilha solta: em linha reta
		}
		const Cell C = MapData.CellOf(From);
		if (!MapData.InBounds(C.X, C.Y))
		{
			return -Fix::One();
		}
		const int32_t Cost = D.Field[size_t(MapData.Index(C.X, C.Y))];
		return Cost < 0 ? -Fix::One() : MapData.CellSize * Cost / StraightCost;
	}

	void World::DockLane(int32_t K, int32_t Truck, Vec2& OutEnter, Vec2& OutExit) const
	{
		// O ZH não deixa o caminhão pegar e entregar parado: ele entra na vaga do depósito por uma
		// ponta, carrega e sai pela outra (os ossos DOCKSTART/DOCKACTION/DOCKEND do DockUpdate), e
		// entrega na porta do centro. Aqui: o lado do depósito mais perto do caminhão, por fora.
		const SupplyDock& D = Docks[size_t(K)];
		const Fix R = DefOf(Truck).Radius;
		const Fix Out = R + Fix::One();
		const Vec2 Rel = U.Pos[Truck] - D.Pos;
		Vec2 A;
		Vec2 B;
		if (Abs(Rel.X) * D.HalfY >= Abs(Rel.Y) * D.HalfX)
		{
			const Fix X = D.Pos.X + (Rel.X.Raw >= 0 ? D.HalfX + Out : -(D.HalfX + Out));
			const Fix Inset = Max(D.HalfY - R, Fix::Zero());
			A = Vec2::Make(X, D.Pos.Y - Inset);
			B = Vec2::Make(X, D.Pos.Y + Inset);
		}
		else
		{
			const Fix Y = D.Pos.Y + (Rel.Y.Raw >= 0 ? D.HalfY + Out : -(D.HalfY + Out));
			const Fix Inset = Max(D.HalfX - R, Fix::Zero());
			A = Vec2::Make(D.Pos.X - Inset, Y);
			B = Vec2::Make(D.Pos.X + Inset, Y);
		}
		const bool bAFirst = DistSqRaw(U.Pos[Truck], A) <= DistSqRaw(U.Pos[Truck], B);
		const int32_t Clearance = MapData.ClearanceFor(DefOf(Truck).PathRadius);
		const auto Walkable = [&](Vec2 P)
		{
			const Cell C = MapData.CellOf(P);
			Cell To;
			if (!MapData.IsWalkable(C.X, C.Y, Clearance) && Pathfinder::NearestWalkable(MapData, C, Clearance, 6, To))
			{
				return MapData.CellCenter(To);
			}
			return ClampToMap(P);
		};
		OutEnter = Walkable(bAFirst ? A : B);
		OutExit = Walkable(bAFirst ? B : A);
	}

	Vec2 World::CenterDoor(int32_t Center, int32_t Truck) const
	{
		const int32_t Dir = DirOf(Center);
		Fix HX;
		Fix HY;
		HalfExtents(U.Def[Center], Dir, HX, HY);
		const Fix Depth = (Dir & 1) ? HY : HX;
		Vec2 P = U.Pos[Center] + DirVector(Dir) * (Depth + DefOf(Truck).Radius + Fix::One());
		const int32_t Clearance = MapData.ClearanceFor(DefOf(Truck).PathRadius);
		const Cell C = MapData.CellOf(P);
		Cell To;
		if (!MapData.IsWalkable(C.X, C.Y, Clearance) && Pathfinder::NearestWalkable(MapData, C, Clearance, 8, To))
		{
			P = MapData.CellCenter(To);
		}
		return ClampToMap(P);
	}

	void World::DropCargo(Vec2 At, int32_t Boxes)
	{
		// a carga que cai (decisão do Felipe, 10/10): uma pilha pequena no chão que qualquer
		// caminhão pega, o de quem perdeu ou o do inimigo; some quando esvazia (SupplyPileSmall)
		SupplyDock Pile;
		Pile.Pos = ClampToMap(At);
		Pile.HalfX = RulesData.Economy.DropPileHalf;
		Pile.HalfY = RulesData.Economy.DropPileHalf;
		Pile.Boxes = Boxes;
		Pile.StartBoxes = Boxes;
		Pile.bPile = true;
		Docks.push_back(Pile);
		Events.push_back({ EventType::CargoDropped, NoEntity, NoEntity, Pile.Pos, int32_t(Docks.size()) - 1 });
	}

	int32_t World::NearestDock(Vec2 From) const
	{
		int32_t Best = -1;
		Fix BestD = Fix::Zero();
		for (int32_t K = 0; K < int32_t(Docks.size()); ++K)
		{
			if (Docks[size_t(K)].Boxes <= 0)
			{
				continue;
			}
			const Fix D = DockDistance(K, From);
			if (D < Fix::Zero() || D > RulesData.Economy.DockScanDistance)
			{
				continue;
			}
			if (Best < 0 || D < BestD)
			{
				Best = K;
				BestD = D;
			}
		}
		return Best;
	}

	int32_t World::NearestSupplyCenter(PlayerIndex Owner, Vec2 From) const
	{
		int32_t Best = -1;
		int64_t BestD2 = 0;
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			if (U.Owner[I] != Owner || !DefOf(I).bSupplyCenter || !IsComplete(I) || U.Hp[I] <= 0)
			{
				continue;
			}
			const int64_t D2 = DistSqRaw(From, U.Pos[I]);
			if (Best < 0 || D2 < BestD2)
			{
				Best = I;
				BestD2 = D2;
			}
		}
		return Best;
	}

	// ---------------------------------------------------------------- comandos

	void World::ApplyEconomyCommand(const Command& C, const std::vector<int32_t>& Members)
	{
		PlayerState& P = Players[C.Player];
		const EconomyRules& E = RulesData.Economy;
		const auto OwnStructure = [&](EntityId Id) -> int32_t
		{
			const int32_t S = IndexOf(Id);
			return S >= 0 && U.Owner[S] == C.Player && DefOf(S).IsStructure() && U.Hp[S] > 0 ? S : -1;
		};
		// o Dozer do grupo mais perto do ponto que sabe construir o tipo (desempate pelo ID)
		const auto NearestBuilder = [&](uint16_t Def, Vec2 At) -> int32_t
		{
			int32_t Best = -1;
			int64_t BestD2 = 0;
			for (const int32_t I : Members)
			{
				if (!Contains(DefOf(I).Constructs, Def))
				{
					continue;
				}
				const int64_t D2 = DistSqRaw(U.Pos[I], At);
				if (Best < 0 || D2 < BestD2 || (D2 == BestD2 && U.Id[I] < U.Id[Best]))
				{
					Best = I;
					BestD2 = D2;
				}
			}
			return Best;
		};
		const auto LeaveCurrentWork = [&](int32_t B)
		{
			if (U.Task[B] == UnitTask::Construct)
			{
				const int32_t Old = IndexOf(U.TaskTarget[B]);
				if (Old >= 0 && U.Builder[Old] == U.Id[B])
				{
					U.Builder[Old] = NoEntity;
				}
			}
		};

		switch (C.Type)
		{
		case CommandType::Produce:
		{
			const int32_t S = OwnStructure(C.Target);
			if (S < 0 || !IsComplete(S) || C.Value < 0 || C.Value >= int32_t(RulesData.Units.size()))
			{
				return;
			}
			const uint16_t Def = uint16_t(C.Value);
			const UnitDef& UD = RulesData.Units[Def];
			if (!Contains(DefOf(S).Produces, Def) || int32_t(U.Queue[S].size()) >= E.MaxQueue || P.Money < UD.Cost
				|| !HasPrerequisites(C.Player, Def))
			{
				return;
			}
			// o dinheiro sai inteiro quando entra na fila; cancelar devolve tudo (ZH ProductionUpdate)
			P.Money -= UD.Cost;
			if (U.Queue[S].empty())
			{
				U.QueueLeft[S] = std::max(1, UD.BuildTicks) * 1000;
			}
			U.Queue[S].push_back(Def);
			return;
		}
		case CommandType::CancelProduce:
		{
			const int32_t S = OwnStructure(C.Target);
			if (S < 0 || C.Value < 0 || C.Value >= int32_t(U.Queue[S].size()))
			{
				return;
			}
			P.Money += RulesData.Units[U.Queue[S][size_t(C.Value)]].Cost;
			U.Queue[S].erase(U.Queue[S].begin() + C.Value);
			if (C.Value == 0)
			{
				U.QueueLeft[S] = U.Queue[S].empty() ? 0 : std::max(1, RulesData.Units[U.Queue[S].front()].BuildTicks) * 1000;
			}
			return;
		}
		case CommandType::Build:
		{
			if (C.Target != NoEntity)
			{
				// retomar uma obra parada: outro Dozer (ou o mesmo) pega dela; só um trabalha por vez
				const int32_t S = OwnStructure(C.Target);
				if (S < 0 || IsComplete(S))
				{
					return;
				}
				const int32_t B = NearestBuilder(U.Def[S], U.Pos[S]);
				if (B < 0)
				{
					return;
				}
				LeaveCurrentWork(B);
				U.Task[B] = UnitTask::Construct;
				U.TaskTarget[B] = C.Target;
				U.TaskTimer[B] = 0;
				return;
			}
			const uint16_t Def = BuildDefOf(C.Value);
			const int32_t Dir = BuildDirOf(C.Value);
			if (Def >= RulesData.Units.size() || !RulesData.Units[Def].IsStructure())
			{
				return;
			}
			const Vec2 Center = SnapStructure(Def, Dir, ClampToMap(C.Point));
			const int32_t B = NearestBuilder(Def, Center);
			if (B < 0 || CanPlace(C.Player, Def, Dir, Center) != PlaceResult::Ok)
			{
				return;
			}
			// o Dozer precisa conseguir chegar: alguma célula em volta da obra na região dele (ZH CLEAR_PATH)
			const int32_t Clearance = MapData.ClearanceFor(DefOf(B).PathRadius);
			const int32_t Home = RegionAt(Clearance, MapData.CellOf(U.Pos[B]));
			if (Home != 0)
			{
				int32_t X0, Y0, X1, Y1;
				FootprintCells(Def, Dir, Center, X0, Y0, X1, Y1);
				bool bReach = false;
				for (int32_t Y = Y0 - 1; Y <= Y1 && !bReach; ++Y)
				{
					for (int32_t X = X0 - 1; X <= X1 && !bReach; ++X)
					{
						const bool bRing = X == X0 - 1 || X == X1 || Y == Y0 - 1 || Y == Y1;
						bReach = bRing && RegionAt(Clearance, Cell{ X, Y }) == Home;
					}
				}
				if (!bReach)
				{
					return;
				}
			}
			// o dinheiro sai quando a obra é marcada no chão (ZH DozerAIUpdate)
			P.Money -= RulesData.Units[Def].Cost;
			const EntityId BuilderId = U.Id[B];
			LeaveCurrentWork(B);
			const EntityId Id = PlaceStructure(C.Player, Def, Dir, Center, false);
			const int32_t BI = IndexOf(BuilderId);
			U.Task[BI] = UnitTask::Construct;
			U.TaskTarget[BI] = Id;
			U.TaskTimer[BI] = 0;
			U.Builder[IndexOf(Id)] = BuilderId;
			return;
		}
		case CommandType::CancelBuild:
		{
			// cancelar a obra devolve 100%, em qualquer fase (ZH GameLogicDispatch)
			const int32_t S = OwnStructure(C.Target);
			if (S < 0 || IsComplete(S))
			{
				return;
			}
			P.Money += DefOf(S).Cost;
			U.Hp[S] = 0;
			U.BuildLeft[S] = -1;   // sai no RemoveDead, sem explosão
			return;
		}
		case CommandType::SetRally:
		{
			const int32_t S = OwnStructure(C.Target);
			if (S < 0)
			{
				return;
			}
			U.Rally[S] = ClampToMap(C.Point);
			U.HasRally[S] = 1;
			return;
		}
		case CommandType::Gather:
		{
			if (C.Value < 0 || C.Value >= int32_t(Docks.size()) || Docks[size_t(C.Value)].Boxes <= 0)
			{
				return;
			}
			for (const int32_t I : Members)
			{
				if (DefOf(I).SupplyBoxes <= 0)
				{
					continue;
				}
				// o depósito clicado vira o preferido (ZH); carregado, entrega primeiro
				U.AutoGather[I] = 1;
				U.TaskDock[I] = C.Value;
				U.TaskTarget[I] = NoEntity;
				U.TaskTimer[I] = 0;
				U.Task[I] = U.Cargo[I] > 0 ? UnitTask::None : UnitTask::ToDock;
				if (U.Task[I] == UnitTask::ToDock)
				{
					Vec2 Exit;
					DockLane(C.Value, I, U.TaskPoint[I], Exit);
					GiveMove(I, U.TaskPoint[I], OrderType::Move);
				}
			}
			return;
		}
		default:
			return;
		}
	}

	// ---------------------------------------------------------------- sistemas

	void World::UpdatePower()
	{
		for (PlayerState& P : Players)
		{
			P.PowerMade = 0;
			P.PowerUsed = 0;
		}
		for (int32_t I = 0; I < UnitCount(); ++I)
		{
			const UnitDef& D = DefOf(I);
			if (!D.IsStructure() || !IsComplete(I) || D.Energy == 0 || U.Hp[I] <= 0)
			{
				continue;   // a obra não gera nem gasta (ZH Player.cpp)
			}
			PlayerState& P = Players[U.Owner[I]];
			if (D.Energy > 0)
			{
				P.PowerMade += D.Energy;
			}
			else
			{
				P.PowerUsed -= D.Energy;
			}
		}
	}

	void World::UpdateConstruction()
	{
		const int32_t N = UnitCount();
		for (int32_t I = 0; I < N; ++I)
		{
			if (U.Task[I] != UnitTask::Construct)
			{
				continue;
			}
			const int32_t S = IndexOf(U.TaskTarget[I]);
			if (S < 0 || U.Owner[S] != U.Owner[I] || IsComplete(S) || U.Hp[S] <= 0)
			{
				U.Task[I] = UnitTask::None;
				U.TaskTarget[I] = NoEntity;
				continue;
			}
			Fix HX;
			Fix HY;
			HalfExtents(U.Def[S], DirOf(S), HX, HY);
			// o alcance da obra: o raio + 2 células. Com 1 célula, a fundação que toma o chão empurrava
			// o Dozer pra fora da folga dela e ele ficava a 7,7 m com alcance de 7,6 m: parado do lado,
			// sem construir e sem poder chegar mais perto (teste base2, 10/10)
			const Fix Reach = DefOf(I).Radius + MapData.CellSize * 2;
			if (RectDistance(U.Pos[I], U.Pos[S], HX, HY) > Reach)
			{
				// ainda longe: anda até a obra (o caminho termina na célula livre mais perto do centro)
				const bool bGoing = U.Order[I] == OrderType::Move && U.OrderPoint[I] == U.Pos[S];
				if (!bGoing)
				{
					if (U.TaskTimer[I] > 0)
					{
						--U.TaskTimer[I];
					}
					else
					{
						GiveMove(I, U.Pos[S], OrderType::Move);
						U.TaskTimer[I] = RetryTicks;
					}
				}
				continue;
			}
			// o Dozer em cima do próprio lote (o jogador clicou onde ele estava): sai pela borda mais
			// perto antes de a fundação tomar o chão, e constrói dali (o ZH trabalha do lado da obra).
			// Sem isso ele esperava o lote vagar pra sempre (teste base1, 10/10)
			if (!U.Blocking[S] && RectDistance(U.Pos[I], U.Pos[S], HX, HY) < DefOf(I).Radius)
			{
				if (U.Order[I] != OrderType::Move)
				{
					GiveMove(I, LotExit(U.Pos[I], U.Pos[S], HX, HY, DefOf(I).Radius + MapData.CellSize, MapData.ClearanceFor(DefOf(I).PathRadius)), OrderType::Move);
				}
				continue;
			}
			if (U.Order[I] != OrderType::Idle)
			{
				U.Order[I] = OrderType::Idle;
				U.Path[I].clear();
				U.NeedsPath[I] = 0;
			}
			// um Dozer por obra: o que chegou primeiro constrói, o outro espera do lado
			const int32_t Current = IndexOf(U.Builder[S]);
			if (Current < 0 || U.Task[Current] != UnitTask::Construct || U.TaskTarget[Current] != U.Id[S])
			{
				U.Builder[S] = U.Id[I];
			}
			if (U.Builder[S] != U.Id[I])
			{
				continue;
			}
			// a fundação só toma o chão agora, com a área livre (os amigos saem da frente)
			if (!U.Blocking[S] && !ClaimFootprint(S))
			{
				continue;
			}
			// a obra anda no ritmo da energia (ZH DozerAIUpdate → calcTimeToBuild); a vida sobe junto
			const UnitDef& SD = DefOf(S);
			const int64_t Total = int64_t(std::max(1, SD.BuildTicks)) * 1000;
			const int64_t Done = Total - U.BuildLeft[S];
			const int64_t Next = std::min<int64_t>(Total, Done + PowerFactorPermille(U.Owner[S]));
			U.BuildLeft[S] = int32_t(Total - Next);
			const int64_t Gain = int64_t(SD.MaxHp) * Next / Total - int64_t(SD.MaxHp) * Done / Total;
			U.Hp[S] = int32_t(std::min<int64_t>(SD.MaxHp, U.Hp[S] + Gain));
			if (U.BuildLeft[S] <= 0)
			{
				const EntityId DozerId = U.Id[I];
				CompleteStructure(S);
				const int32_t D = IndexOf(DozerId);
				U.Task[D] = UnitTask::None;
				U.TaskTarget[D] = NoEntity;
			}
		}
	}

	void World::UpdateProduction()
	{
		const int32_t N = UnitCount();
		for (int32_t S = 0; S < N; ++S)
		{
			if (U.Queue[S].empty() || !DefOf(S).IsStructure() || !IsComplete(S) || U.Hp[S] <= 0)
			{
				continue;
			}
			// só o primeiro da fila anda, no ritmo da energia; o progresso só soma (sem o pulo do ZH #1390)
			U.QueueLeft[S] -= PowerFactorPermille(U.Owner[S]);
			if (U.QueueLeft[S] > 0)
			{
				continue;
			}
			const uint16_t Done = U.Queue[S].front();
			U.Queue[S].erase(U.Queue[S].begin());
			U.QueueLeft[S] = U.Queue[S].empty() ? 0 : std::max(1, RulesData.Units[U.Queue[S].front()].BuildTicks) * 1000;
			SpawnFrom(S, Done);
		}
	}

	void World::UpdateSupply()
	{
		const EconomyRules& E = RulesData.Economy;
		// o atendido de cada depósito tem de estar lá, carregando
		for (int32_t K = 0; K < int32_t(Docks.size()); ++K)
		{
			SupplyDock& Dk = Docks[size_t(K)];
			const int32_t T = IndexOf(Dk.Occupant);
			if (Dk.Occupant != NoEntity && (T < 0 || U.Task[T] != UnitTask::Loading || U.TaskDock[T] != K))
			{
				Dk.Occupant = NoEntity;
			}
		}
		const int32_t N = UnitCount();
		for (int32_t I = 0; I < N; ++I)
		{
			const UnitDef& D = DefOf(I);
			if (D.SupplyBoxes <= 0)
			{
				continue;
			}
			const auto Arrived = [&]()
			{
				// no ponto de encaixe, ou parado perto dele (o caminho acaba na célula, não no ponto)
				const int64_t D2 = DistSqRaw(U.Pos[I], U.TaskPoint[I]);
				return D2 <= RadiusSqRaw(LaneArrive) || (U.Order[I] == OrderType::Idle && D2 <= RadiusSqRaw(MapData.CellSize * 2));
			};
			const auto Stop = [&]()
			{
				U.Order[I] = OrderType::Idle;
				U.Path[I].clear();
				U.NeedsPath[I] = 0;
			};
			const auto GoTo = [&](Vec2 P)
			{
				U.TaskPoint[I] = P;
				U.TaskTimer[I] = 0;
				GiveMove(I, P, OrderType::Move);
			};
			const auto Retry = [&]()
			{
				// parou longe (empurrado, caminho cortado): refaz o caminho a cada 1 s
				if (U.Order[I] != OrderType::Idle)
				{
					return;
				}
				if (U.TaskTimer[I] > 0)
				{
					--U.TaskTimer[I];
					return;
				}
				GiveMove(I, U.TaskPoint[I], OrderType::Move);
				U.TaskTimer[I] = RetryTicks;
			};
			const auto GoDeliver = [&]()
			{
				const int32_t Center = NearestSupplyCenter(U.Owner[I], U.Pos[I]);
				if (Center < 0)
				{
					U.Task[I] = UnitTask::WaitCenter;
					U.TaskTimer[I] = RetryTicks;
					return;
				}
				U.Task[I] = UnitTask::ToCenter;
				U.TaskTarget[I] = U.Id[Center];
				GoTo(CenterDoor(Center, I));
			};
			const auto PickDock = [&]()
			{
				// o preferido do jogador, se ainda tem caixas; senão o mais perto pelo caminho, até o alcance
				int32_t K = U.TaskDock[I];
				if (K < 0 || K >= int32_t(Docks.size()) || Docks[size_t(K)].Boxes <= 0)
				{
					K = NearestDock(U.Pos[I]);
				}
				if (K >= 0)
				{
					U.TaskDock[I] = K;
					U.Task[I] = UnitTask::ToDock;
					Vec2 Enter;
					Vec2 Exit;
					DockLane(K, I, Enter, Exit);
					GoTo(Enter);
					return;
				}
				// nada ao alcance: volta pra perto do centro (ou de qualquer prédio dele) e espera (ZH)
				U.Task[I] = UnitTask::WaitDock;
				U.TaskTimer[I] = RetryTicks;
				int32_t Home = NearestSupplyCenter(U.Owner[I], U.Pos[I]);
				for (int32_t J = 0; J < UnitCount() && Home < 0; ++J)
				{
					Home = U.Owner[J] == U.Owner[I] && DefOf(J).IsStructure() && IsComplete(J) ? J : -1;
				}
				if (Home >= 0 && DistSqRaw(U.Pos[I], U.Pos[Home]) > RadiusSqRaw(IdleNearCenter + DefOf(Home).Radius))
				{
					GiveMove(I, U.Pos[Home], OrderType::Move);
				}
			};
			switch (U.Task[I])
			{
			case UnitTask::None:
				if (!U.AutoGather[I] || U.Order[I] != OrderType::Idle || ((CurrentTick + U.Id[I]) % GatherScanInterval) != 0)
				{
					break;
				}
				if (U.Cargo[I] > 0)
				{
					GoDeliver();   // com qualquer caixa a bordo, vai entregar (ZH)
				}
				else
				{
					PickDock();
				}
				break;
			case UnitTask::WaitDock:
				if (--U.TaskTimer[I] <= 0)
				{
					PickDock();
				}
				break;
			case UnitTask::ToDock:
			{
				const int32_t K = U.TaskDock[I];
				if (K < 0 || K >= int32_t(Docks.size()) || Docks[size_t(K)].Boxes <= 0)
				{
					U.Task[I] = UnitTask::None;
					U.TaskDock[I] = -1;
					break;
				}
				SupplyDock& Dk = Docks[size_t(K)];
				if (Arrived())
				{
					Stop();
					// um caminhão atendido por vez; os outros esperam na entrada (ZH DockUpdate)
					if (Dk.Occupant == NoEntity)
					{
						Dk.Occupant = U.Id[I];
						U.Task[I] = UnitTask::Loading;
						U.TaskTimer[I] = E.TicksPerBox;
					}
				}
				else
				{
					Retry();
				}
				break;
			}
			case UnitTask::Loading:
			{
				const int32_t K = U.TaskDock[I];
				if (K < 0 || K >= int32_t(Docks.size()))
				{
					U.Task[I] = UnitTask::None;
					break;
				}
				SupplyDock& Dk = Docks[size_t(K)];
				// a espera vem antes de cada caixa, inclusive da primeira (ZH AIDock)
				if (--U.TaskTimer[I] > 0)
				{
					break;
				}
				if (Dk.Boxes > 0 && U.Cargo[I] < D.SupplyBoxes)
				{
					--Dk.Boxes;
					++U.Cargo[I];
				}
				if (Dk.Boxes > 0 && U.Cargo[I] < D.SupplyBoxes)
				{
					U.TaskTimer[I] = E.TicksPerBox;
					break;
				}
				Dk.Occupant = NoEntity;
				if (U.Cargo[I] > 0)
				{
					// sai pela outra ponta da vaga antes de ir entregar
					Vec2 Enter;
					Vec2 Exit;
					DockLane(K, I, Enter, Exit);
					U.Task[I] = UnitTask::LeaveDock;
					GoTo(Exit);
				}
				else
				{
					U.Task[I] = UnitTask::None;
				}
				break;
			}
			case UnitTask::LeaveDock:
				if (Arrived())
				{
					GoDeliver();
				}
				else
				{
					Retry();
				}
				break;
			case UnitTask::WaitCenter:
				if (--U.TaskTimer[I] <= 0)
				{
					GoDeliver();
				}
				break;
			case UnitTask::ToCenter:
			{
				const int32_t Center = IndexOf(U.TaskTarget[I]);
				if (Center < 0 || !IsComplete(Center) || U.Hp[Center] <= 0 || U.Owner[Center] != U.Owner[I])
				{
					GoDeliver();   // o centro caiu: o mais perto que sobrou
					break;
				}
				if (Arrived())
				{
					Stop();
					U.Task[I] = UnitTask::Unloading;
					U.TaskTimer[I] = E.CenterTicks;
				}
				else
				{
					Retry();
				}
				break;
			}
			case UnitTask::Unloading:
			{
				if (--U.TaskTimer[I] > 0)
				{
					break;
				}
				const int32_t Amount = U.Cargo[I] * E.BoxValue;
				Players[U.Owner[I]].Money += Amount;
				Events.push_back({ EventType::SupplyDelivered, U.Id[I], U.TaskTarget[I], U.Pos[I], Amount });
				U.Cargo[I] = 0;
				U.Task[I] = UnitTask::None;
				U.TaskTarget[I] = NoEntity;
				break;
			}
			case UnitTask::Construct:
				break;
			}
		}
	}
}
