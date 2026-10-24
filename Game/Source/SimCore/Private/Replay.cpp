#include "Sim/Replay.h"

namespace Sim
{
	namespace
	{
		constexpr uint32_t Magic = 0x52535452;   // "RTSR"
		constexpr uint32_t FormatVersion = 1;
		constexpr int32_t MaxMapSide = 4096;
		constexpr size_t MaxKeyLength = 64;
		constexpr uint16_t MaxUnitDefs = 1024;
		constexpr uint16_t MaxStartingUnits = 10000;
		constexpr uint16_t MaxListLength = 64;
		constexpr uint16_t MaxDocks = 1024;
		constexpr uint32_t MaxProps = 65536;

		enum class Record : uint8_t
		{
			Commands = 1,
			Hash = 2,
			End = 3,
		};

		void WriteString(ByteWriter& W, const std::string& S)
		{
			W.U16(uint16_t(S.size()));
			W.Raw(reinterpret_cast<const uint8_t*>(S.data()), S.size());
		}

		bool ReadString(ByteReader& R, std::string& Out)
		{
			uint16_t Len = 0;
			if (!R.U16(Len) || Len > MaxKeyLength)
			{
				return false;
			}
			Out.resize(Len);
			return Len == 0 || R.Raw(reinterpret_cast<uint8_t*>(&Out[0]), Len);
		}

		void WriteFix(ByteWriter& W, Fix F) { W.I64(F.Raw); }
		bool ReadFix(ByteReader& R, Fix& F) { return R.I64(F.Raw); }
		void WriteI32(ByteWriter& W, int32_t V) { W.U32(uint32_t(V)); }
		bool ReadI32(ByteReader& R, int32_t& V) { uint32_t U = 0; if (!R.U32(U)) { return false; } V = int32_t(U); return true; }

		void WriteDef(ByteWriter& W, const UnitDef& D)
		{
			WriteString(W, D.Key);
			WriteI32(W, D.MaxHp);
			W.U8(uint8_t(D.Armor));
			WriteFix(W, D.Radius);
			WriteFix(W, D.PathRadius);
			WriteFix(W, D.MaxSpeed);
			WriteFix(W, D.Accel);
			WriteFix(W, D.Brake);
			WriteI32(W, D.TurnBam);
			WriteI32(W, D.TurretTurnBam);
			WriteI32(W, D.TurretIdleReturnTicks);
			WriteFix(W, D.Sight);
			WriteFix(W, D.Weapon.Range);
			WriteI32(W, D.Weapon.Damage);
			W.U8(uint8_t(D.Weapon.Type));
			WriteI32(W, D.Weapon.ReloadTicks);
			WriteI32(W, D.Weapon.AimToleranceBam);
			WriteFix(W, D.Weapon.ProjectileStep);
			W.U8(uint8_t(D.Kind));
			WriteI32(W, D.Cost);
			WriteI32(W, D.BuildTicks);
			WriteI32(W, D.Energy);
			WriteFix(W, D.HalfX);
			WriteFix(W, D.HalfY);
			for (const std::vector<uint16_t>* List : { &D.Produces, &D.Constructs, &D.FreeUnits, &D.Requires })
			{
				W.U16(uint16_t(List->size()));
				for (const uint16_t K : *List)
				{
					W.U16(K);
				}
			}
			WriteI32(W, D.SupplyBoxes);
			W.U8(D.bSupplyCenter ? 1 : 0);
			W.U8(D.bCountsForVictory ? 1 : 0);
			WriteFix(W, D.ExitLane);
			WriteI32(W, D.Weapon.ClipSize);
			WriteI32(W, D.Weapon.ClipReloadTicks);
			W.U8(D.bNeedsPower ? 1 : 0);
			// (simulação 16, 16/10) o esmagar
			WriteI32(W, D.CrusherLevel);
			WriteFix(W, D.HullHalfLength);
			WriteFix(W, D.HullHalfWidth);
			WriteFix(W, D.AssistRange);   // (simulação 17)
			// (simulação 15, 16/10) o míssil da Patriot
			WriteFix(W, D.Weapon.MissileLaunchStep);
			WriteFix(W, D.Weapon.MissileAccel);
			WriteI32(W, D.Weapon.MissileTurnBam);
			WriteI32(W, D.Weapon.MissileBoostTicks);
			W.U16(D.Weapon.MissileLaunchPitch.Bam);
			WriteFix(W, D.Weapon.MissileLaunchHeight);
			WriteI32(W, D.Weapon.MissileSpreadBam);
			WriteI32(W, D.Weapon.MissileLifeTicks);
			// (simulação 18, 22/10) a vida e o afundar da carcaça
			WriteI32(W, D.WreckLifeTicks);
			WriteI32(W, D.WreckSinkTicks);
			// (simulação 19, 22/10) o alcance mínimo, o dano em área, a patente, o teto, os poderes e o avião de carga
			WriteFix(W, D.Weapon.MinRange);
			WriteFix(W, D.Weapon.SplashFull);
			WriteFix(W, D.Weapon.Splash);
			WriteI32(W, D.Weapon.SplashEdgePct);
			WriteI32(W, D.RequiresRank);
			WriteI32(W, D.MaxPerPlayer);
			W.U8(D.bGrantsPowers ? 1 : 0);
			for (const int32_t V : { D.AirliftFirstTicks, D.AirliftPeriodTicks, D.AirliftLeadTicks, D.AirliftPalletsMin, D.AirliftPalletsMax,
				D.AirliftBoxesPerPallet, D.AirliftUnloadTicks })
			{
				WriteI32(W, V);
			}
			// (simulação 20, 23/10) a mira antes do tiro, a boca do tubo, o conserto do Dozer e a massa
			WriteI32(W, D.Weapon.PreAttackTicks);
			for (const Fix V : { D.Weapon.LaunchPivot.X, D.Weapon.LaunchPivot.Y, D.Weapon.LaunchForward, D.Weapon.LaunchSide, D.Weapon.LaunchUp })
			{
				WriteFix(W, V);
			}
			WriteI32(W, D.RepairMilliPctPerS);
			WriteI32(W, D.MassKg);
		}

		bool ReadDef(ByteReader& R, UnitDef& D)
		{
			uint8_t Armor = 0;
			uint8_t Damage = 0;
			const bool bOk = ReadString(R, D.Key) && ReadI32(R, D.MaxHp) && R.U8(Armor)
				&& ReadFix(R, D.Radius) && ReadFix(R, D.PathRadius) && ReadFix(R, D.MaxSpeed)
				&& ReadFix(R, D.Accel) && ReadFix(R, D.Brake) && ReadI32(R, D.TurnBam)
				&& ReadI32(R, D.TurretTurnBam) && ReadI32(R, D.TurretIdleReturnTicks) && ReadFix(R, D.Sight)
				&& ReadFix(R, D.Weapon.Range) && ReadI32(R, D.Weapon.Damage) && R.U8(Damage)
				&& ReadI32(R, D.Weapon.ReloadTicks) && ReadI32(R, D.Weapon.AimToleranceBam)
				&& ReadFix(R, D.Weapon.ProjectileStep);
			if (!bOk || Armor >= uint8_t(ArmorType::Count) || Damage >= uint8_t(DamageType::Count))
			{
				return false;
			}
			D.Armor = ArmorType(Armor);
			D.Weapon.Type = DamageType(Damage);
			uint8_t Kind = 0;
			uint8_t Center = 0;
			if (!R.U8(Kind) || Kind >= uint8_t(ObjectKind::Count) || !ReadI32(R, D.Cost) || !ReadI32(R, D.BuildTicks)
				|| !ReadI32(R, D.Energy) || !ReadFix(R, D.HalfX) || !ReadFix(R, D.HalfY))
			{
				return false;
			}
			D.Kind = ObjectKind(Kind);
			for (std::vector<uint16_t>* List : { &D.Produces, &D.Constructs, &D.FreeUnits, &D.Requires })
			{
				uint16_t Count = 0;
				if (!R.U16(Count) || Count > MaxListLength)
				{
					return false;
				}
				List->resize(Count);
				for (uint16_t& K : *List)
				{
					if (!R.U16(K) || K >= MaxUnitDefs)
					{
						return false;
					}
				}
			}
			uint8_t Victory = 0;
			if (!ReadI32(R, D.SupplyBoxes) || !R.U8(Center) || Center > 1 || !R.U8(Victory) || Victory > 1 || !ReadFix(R, D.ExitLane))
			{
				return false;
			}
			D.bSupplyCenter = Center != 0;
			D.bCountsForVictory = Victory != 0;
			// (simulação 13, 14/10) a rajada e a energia da defesa de base
			uint8_t Powered = 0;
			if (!ReadI32(R, D.Weapon.ClipSize) || !ReadI32(R, D.Weapon.ClipReloadTicks) || !R.U8(Powered) || Powered > 1)
			{
				return false;
			}
			D.bNeedsPower = Powered != 0;
			// (simulação 16, 16/10) o esmagar
			if (!ReadI32(R, D.CrusherLevel) || !ReadFix(R, D.HullHalfLength) || !ReadFix(R, D.HullHalfWidth) || !ReadFix(R, D.AssistRange))
			{
				return false;
			}
			// (simulação 15, 16/10) o míssil da Patriot
			uint16_t LaunchPitch = 0;
			if (!ReadFix(R, D.Weapon.MissileLaunchStep) || !ReadFix(R, D.Weapon.MissileAccel) || !ReadI32(R, D.Weapon.MissileTurnBam)
				|| !ReadI32(R, D.Weapon.MissileBoostTicks) || !R.U16(LaunchPitch) || !ReadFix(R, D.Weapon.MissileLaunchHeight)
				|| !ReadI32(R, D.Weapon.MissileSpreadBam) || !ReadI32(R, D.Weapon.MissileLifeTicks))
			{
				return false;
			}
			D.Weapon.MissileLaunchPitch = Angle::FromBam(LaunchPitch);
			// (simulação 18, 22/10) a vida e o afundar da carcaça (o afundar tem pelo menos 1 passo: o desenho divide por ele)
			if (!ReadI32(R, D.WreckLifeTicks) || !ReadI32(R, D.WreckSinkTicks) || D.WreckLifeTicks < 0 || D.WreckSinkTicks < 1)
			{
				return false;
			}
			// (simulação 19, 22/10) o alcance mínimo, o dano em área, a patente, o teto, os poderes e o avião de carga (os números
			// fora da faixa recusam: o avião sem palete ou sem caixa travaria a conta)
			uint8_t Grants = 0;
			if (!ReadFix(R, D.Weapon.MinRange) || !ReadFix(R, D.Weapon.SplashFull) || !ReadFix(R, D.Weapon.Splash) || !ReadI32(R, D.Weapon.SplashEdgePct)
				|| !ReadI32(R, D.RequiresRank) || !ReadI32(R, D.MaxPerPlayer) || !R.U8(Grants) || Grants > 1)
			{
				return false;
			}
			D.bGrantsPowers = Grants != 0;
			for (int32_t* V : { &D.AirliftFirstTicks, &D.AirliftPeriodTicks, &D.AirliftLeadTicks, &D.AirliftPalletsMin, &D.AirliftPalletsMax,
				&D.AirliftBoxesPerPallet, &D.AirliftUnloadTicks })
			{
				if (!ReadI32(R, *V) || *V < 0)
				{
					return false;
				}
			}
			if (D.Weapon.MinRange.Raw < 0 || D.Weapon.Splash < D.Weapon.SplashFull || D.Weapon.SplashEdgePct < 0 || D.Weapon.SplashEdgePct > 100
				|| D.RequiresRank < 0 || D.RequiresRank > MaxRank || D.MaxPerPlayer < 0
				|| (D.HasAirlift() && (D.AirliftPalletsMin < 1 || D.AirliftPalletsMax < D.AirliftPalletsMin || D.AirliftBoxesPerPallet < 1
					|| D.AirliftLeadTicks >= D.AirliftPeriodTicks || D.AirliftFirstTicks <= D.AirliftLeadTicks)))
			{
				return false;
			}
			// (simulação 20, 23/10) a mira antes do tiro, a boca do tubo, o conserto do Dozer e a massa (nada negativo)
			if (!ReadI32(R, D.Weapon.PreAttackTicks))
			{
				return false;
			}
			for (Fix* V : { &D.Weapon.LaunchPivot.X, &D.Weapon.LaunchPivot.Y, &D.Weapon.LaunchForward, &D.Weapon.LaunchSide, &D.Weapon.LaunchUp })
			{
				if (!ReadFix(R, *V))
				{
					return false;
				}
			}
			return ReadI32(R, D.RepairMilliPctPerS) && ReadI32(R, D.MassKg) && D.Weapon.PreAttackTicks >= 0 && D.Weapon.LaunchUp.Raw >= 0
				&& D.RepairMilliPctPerS >= 0 && D.MassKg >= 0;
		}

		/** (simulação 19) Um poder do general; a munição o ReadSetup confere contra as unidades lidas. */
		void WritePower(ByteWriter& W, const PowerDef& P)
		{
			WriteString(W, P.Key);
			for (const int32_t V : { P.RequiresRank, P.UpgradeRank, P.CooldownTicks, P.Missiles, P.MissilesUpgraded, P.IntervalTicks })
			{
				WriteI32(W, V);
			}
			for (const Fix V : { P.PatternRadius, P.PatternRadiusUpgraded, P.SpawnBack, P.SpawnUp })
			{
				WriteFix(W, V);
			}
			W.U16(P.Munition);
		}

		bool ReadPower(ByteReader& R, PowerDef& P)
		{
			if (!ReadString(R, P.Key))
			{
				return false;
			}
			for (int32_t* V : { &P.RequiresRank, &P.UpgradeRank, &P.CooldownTicks, &P.Missiles, &P.MissilesUpgraded, &P.IntervalTicks })
			{
				if (!ReadI32(R, *V))
				{
					return false;
				}
			}
			for (Fix* V : { &P.PatternRadius, &P.PatternRadiusUpgraded, &P.SpawnBack, &P.SpawnUp })
			{
				if (!ReadFix(R, *V))
				{
					return false;
				}
			}
			return R.U16(P.Munition) && P.RequiresRank >= 0 && P.RequiresRank <= MaxRank && P.UpgradeRank >= 0 && P.UpgradeRank <= MaxRank
				&& P.CooldownTicks >= 0 && P.IntervalTicks >= 0 && P.Missiles >= 1 && P.Missiles <= MaxStrikeMissiles
				&& P.MissilesUpgraded >= 1 && P.MissilesUpgraded <= MaxStrikeMissiles;
		}
	}

	void WriteSetup(ByteWriter& W, const MatchSetup& S)
	{
		W.U64(S.Seed);
		W.U32(uint32_t(S.Map.Width));
		W.U32(uint32_t(S.Map.Height));
		WriteFix(W, S.Map.CellSize);
		WriteFix(W, S.Map.Origin.X);
		WriteFix(W, S.Map.Origin.Y);
		W.Raw(S.Map.Pass.data(), S.Map.Pass.size());
		W.U8(S.Map.HasHeights() ? 1 : 0);
		if (S.Map.HasHeights())
		{
			for (size_t I = 0; I < S.Map.Pass.size(); ++I)
			{
				W.U16(uint16_t(S.Map.MinDm[I]));
				W.U16(uint16_t(S.Map.MaxDm[I]));
			}
		}

		W.U16(uint16_t(S.Rules.Units.size()));
		for (const UnitDef& D : S.Rules.Units)
		{
			WriteDef(W, D);
		}
		for (int A = 0; A < int(ArmorType::Count); ++A)
		{
			for (int T = 0; T < int(DamageType::Count); ++T)
			{
				WriteI32(W, S.Rules.Armor.Percent[A][T]);
			}
		}
		const EconomyRules& E = S.Rules.Economy;
		for (const int32_t V : { E.StartingMoney, E.BoxValue, E.TicksPerBox, E.CenterTicks, E.MaxQueue, E.LowPowerMinPermille,
			E.LowPowerMaxPermille, E.MaxFlatnessCm, E.DockClearCells })
		{
			WriteI32(W, V);
		}
		for (const Fix V : { E.DockScanDistance, E.EdgeMargin, E.SupplyCenterDockGap, E.DropPileHalf })
		{
			WriteFix(W, V);
		}
		W.U16(uint16_t(S.Docks.size()));
		for (const SupplyDockSetup& D : S.Docks)
		{
			WriteFix(W, D.Pos.X);
			WriteFix(W, D.Pos.Y);
			WriteFix(W, D.HalfX);
			WriteFix(W, D.HalfY);
			WriteI32(W, D.Boxes);
		}

		W.U8(uint8_t(S.Players.size()));
		for (const PlayerSetup& P : S.Players)
		{
			W.U8(P.Team);
			W.U8(P.bIsAI ? 1 : 0);
			W.U16(uint16_t(P.Units.size()));
			for (const StartingUnit& U : P.Units)
			{
				WriteString(W, U.Key);
				WriteFix(W, U.Pos.X);
				WriteFix(W, U.Pos.Y);
				W.U16(U.Facing.Bam);
			}
		}
		W.U8(S.bFogOfWar ? 1 : 0);
		W.U8(S.bAllowDebugCommands ? 1 : 0);
		// (simulação 16) os carros e as árvores do mapa
		W.U32(uint32_t(S.Props.size()));
		for (const PropSetup& P : S.Props)
		{
			W.U8(uint8_t(P.Kind));
			WriteFix(W, P.Pos.X);
			WriteFix(W, P.Pos.Y);
			W.U16(P.Facing.Bam);
			WriteFix(W, P.HalfLength);
			WriteFix(W, P.HalfWidth);
			WriteFix(W, P.Height);
			W.U8(P.CrushableLevel);
		}
		// (simulação 19, 22/10) a patente e os poderes do general, e a patente de largada de cada jogador
		for (const int32_t V : E.RankXp)
		{
			WriteI32(W, V);
		}
		WriteI32(W, E.XpDealtDivisor);
		WriteI32(W, E.XpTakenDivisor);
		W.U8(uint8_t(S.Rules.Powers.size()));
		for (const PowerDef& P : S.Rules.Powers)
		{
			WritePower(W, P);
		}
		for (const PlayerSetup& P : S.Players)
		{
			W.U8(P.StartRank);
		}
		// (simulação 20, 23/10) vender: a % e a desmontagem
		WriteI32(W, E.SellPercent);
		WriteI32(W, E.SellTicks);
	}

	bool ReadSetup(ByteReader& R, MatchSetup& Out)
	{
		Out = MatchSetup();
		uint32_t Width = 0;
		uint32_t Height = 0;
		Fix CellSize;
		Vec2 Origin;
		if (!R.U64(Out.Seed) || !R.U32(Width) || !R.U32(Height) || Width == 0 || Height == 0
			|| Width > uint32_t(MaxMapSide) || Height > uint32_t(MaxMapSide)
			|| !ReadFix(R, CellSize) || CellSize.Raw <= 0 || !ReadFix(R, Origin.X) || !ReadFix(R, Origin.Y))
		{
			return false;
		}
		Out.Map.Resize(int32_t(Width), int32_t(Height), CellSize, Origin);
		if (!R.Raw(Out.Map.Pass.data(), Out.Map.Pass.size()))
		{
			return false;
		}
		for (const uint8_t P : Out.Map.Pass)
		{
			if (P > uint8_t(CellPass::Toppleable))
			{
				return false;
			}
		}
		uint8_t bHeights = 0;
		if (!R.U8(bHeights) || bHeights > 1)
		{
			return false;
		}
		if (bHeights)
		{
			Out.Map.MinDm.resize(Out.Map.Pass.size());
			Out.Map.MaxDm.resize(Out.Map.Pass.size());
			for (size_t I = 0; I < Out.Map.Pass.size(); ++I)
			{
				uint16_t Lo = 0;
				uint16_t Hi = 0;
				if (!R.U16(Lo) || !R.U16(Hi))
				{
					return false;
				}
				Out.Map.MinDm[I] = int16_t(Lo);
				Out.Map.MaxDm[I] = int16_t(Hi);
			}
		}

		uint16_t DefCount = 0;
		if (!R.U16(DefCount) || DefCount > MaxUnitDefs)
		{
			return false;
		}
		Out.Rules.Units.resize(DefCount);
		for (UnitDef& D : Out.Rules.Units)
		{
			if (!ReadDef(R, D))
			{
				return false;
			}
		}
		for (int A = 0; A < int(ArmorType::Count); ++A)
		{
			for (int T = 0; T < int(DamageType::Count); ++T)
			{
				if (!ReadI32(R, Out.Rules.Armor.Percent[A][T]))
				{
					return false;
				}
			}
		}
		EconomyRules& E = Out.Rules.Economy;
		for (int32_t* V : { &E.StartingMoney, &E.BoxValue, &E.TicksPerBox, &E.CenterTicks, &E.MaxQueue, &E.LowPowerMinPermille,
			&E.LowPowerMaxPermille, &E.MaxFlatnessCm, &E.DockClearCells })
		{
			if (!ReadI32(R, *V))
			{
				return false;
			}
		}
		for (Fix* V : { &E.DockScanDistance, &E.EdgeMargin, &E.SupplyCenterDockGap, &E.DropPileHalf })
		{
			if (!ReadFix(R, *V))
			{
				return false;
			}
		}
		uint16_t DockCount = 0;
		if (!R.U16(DockCount) || DockCount > MaxDocks)
		{
			return false;
		}
		Out.Docks.resize(DockCount);
		for (SupplyDockSetup& D : Out.Docks)
		{
			if (!ReadFix(R, D.Pos.X) || !ReadFix(R, D.Pos.Y) || !ReadFix(R, D.HalfX) || !ReadFix(R, D.HalfY) || !ReadI32(R, D.Boxes))
			{
				return false;
			}
		}

		uint8_t PlayerCount = 0;
		if (!R.U8(PlayerCount) || PlayerCount == 0 || PlayerCount > MaxPlayers)
		{
			return false;
		}
		Out.Players.resize(PlayerCount);
		for (PlayerSetup& P : Out.Players)
		{
			uint8_t AI = 0;
			uint16_t UnitCount = 0;
			if (!R.U8(P.Team) || P.Team >= MaxPlayers || !R.U8(AI) || !R.U16(UnitCount) || UnitCount > MaxStartingUnits)
			{
				return false;
			}
			P.bIsAI = AI != 0;
			P.Units.resize(UnitCount);
			for (StartingUnit& U : P.Units)
			{
				if (!ReadString(R, U.Key) || !ReadFix(R, U.Pos.X) || !ReadFix(R, U.Pos.Y) || !R.U16(U.Facing.Bam))
				{
					return false;
				}
			}
		}
		uint8_t Fog = 0;
		uint8_t Debug = 0;
		if (!R.U8(Fog) || Fog > 1 || !R.U8(Debug) || Debug > 1)
		{
			return false;
		}
		Out.bFogOfWar = Fog != 0;
		Out.bAllowDebugCommands = Debug != 0;
		// (simulação 16) os carros e as árvores do mapa
		uint32_t PropCount = 0;
		if (!R.U32(PropCount) || PropCount > MaxProps)
		{
			return false;
		}
		Out.Props.resize(PropCount);
		for (PropSetup& P : Out.Props)
		{
			uint8_t Kind = 0;
			if (!R.U8(Kind) || Kind > 1 || !ReadFix(R, P.Pos.X) || !ReadFix(R, P.Pos.Y) || !R.U16(P.Facing.Bam) || !ReadFix(R, P.HalfLength)
				|| !ReadFix(R, P.HalfWidth) || !ReadFix(R, P.Height) || !R.U8(P.CrushableLevel))
			{
				return false;
			}
			P.Kind = PropKind(Kind);
		}
		// (simulação 19, 22/10) a patente e os poderes do general (a munição tem de ser uma unidade lida com arma de míssil), e a
		// patente de largada de cada jogador
		for (int32_t& V : E.RankXp)
		{
			if (!ReadI32(R, V))
			{
				return false;
			}
		}
		uint8_t PowerCount = 0;
		if (!ReadI32(R, E.XpDealtDivisor) || !ReadI32(R, E.XpTakenDivisor) || E.XpDealtDivisor < 0 || E.XpTakenDivisor < 0
			|| !R.U8(PowerCount) || PowerCount > MaxPowers)
		{
			return false;
		}
		Out.Rules.Powers.resize(PowerCount);
		for (PowerDef& P : Out.Rules.Powers)
		{
			if (!ReadPower(R, P) || P.Munition >= Out.Rules.Units.size())
			{
				return false;
			}
			const UnitDef& M = Out.Rules.Units[P.Munition];
			if (M.IsStructure() || !M.Weapon.IsMissile() || M.Weapon.Damage <= 0)
			{
				return false;
			}
		}
		for (PlayerSetup& P : Out.Players)
		{
			if (!R.U8(P.StartRank) || P.StartRank > MaxRank)   // (o 0 o Init sobe pra 1)
			{
				return false;
			}
		}
		// (simulação 20, 23/10) vender: a % (0..100) e a desmontagem (pelo menos 1 passo)
		return ReadI32(R, E.SellPercent) && ReadI32(R, E.SellTicks) && E.SellPercent >= 0 && E.SellPercent <= 100 && E.SellTicks >= 1;
	}

	void ReplayWriter::Begin(const MatchSetup& Setup)
	{
		Header = ByteWriter();
		Body = ByteWriter();
		Header.U32(Magic);
		Header.U32(FormatVersion);
		Header.U32(SimVersion);
		WriteSetup(Header, Setup);
		LastTick = 0;
		LastHash = 0;
	}

	void ReplayWriter::AddTick(TickNumber Tick, const std::vector<Command>& Commands, uint64_t HashAfter)
	{
		if (!Commands.empty())
		{
			Body.U8(uint8_t(Record::Commands));
			Body.U32(Tick);
			Body.U16(uint16_t(Commands.size()));
			for (const Command& C : Commands)
			{
				WriteCommand(Body, C);
			}
		}
		if (Tick % ReplayHashInterval == 0)
		{
			Body.U8(uint8_t(Record::Hash));
			Body.U32(Tick);
			Body.U64(HashAfter);
		}
		LastTick = Tick;
		LastHash = HashAfter;
	}

	std::vector<uint8_t> ReplayWriter::Finish() const
	{
		ByteWriter Out;
		Out.Raw(Header.Bytes.data(), Header.Bytes.size());
		Out.Raw(Body.Bytes.data(), Body.Bytes.size());
		Out.U8(uint8_t(Record::End));
		Out.U32(LastTick);
		Out.U64(LastHash);
		return Out.Bytes;
	}

	bool ReplayReader::Load(const std::vector<uint8_t>& Bytes)
	{
		*this = ReplayReader();
		ByteReader R(Bytes.data(), Bytes.size());
		uint32_t FileMagic = 0;
		uint32_t Format = 0;
		uint32_t Version = 0;
		if (!R.U32(FileMagic) || FileMagic != Magic || !R.U32(Format) || Format != FormatVersion
			|| !R.U32(Version) || Version != SimVersion || !ReadSetup(R, SetupData))
		{
			return false;
		}
		while (true)
		{
			uint8_t Type = 0;
			if (!R.U8(Type))
			{
				return false;   // acabou sem o registro de fim
			}
			if (Type == uint8_t(Record::Commands))
			{
				TickRecord T;
				uint16_t Count = 0;
				if (!R.U32(T.Tick) || !R.U16(Count))
				{
					return false;
				}
				T.Commands.resize(Count);
				for (Command& C : T.Commands)
				{
					if (!ReadCommand(R, C) || C.Tick != T.Tick)
					{
						return false;
					}
				}
				Ticks.push_back(std::move(T));
			}
			else if (Type == uint8_t(Record::Hash))
			{
				HashRecord H;
				if (!R.U32(H.Tick) || !R.U64(H.Hash))
				{
					return false;
				}
				Hashes.push_back(H);
			}
			else if (Type == uint8_t(Record::End))
			{
				HashRecord H;
				if (!R.U32(FinalTick) || !R.U64(H.Hash))
				{
					return false;
				}
				H.Tick = FinalTick;
				Hashes.push_back(H);
				return R.AtEnd();
			}
			else
			{
				return false;
			}
		}
	}

	std::vector<Command> ReplayReader::CommandsFor(TickNumber Tick) const
	{
		// registros em ordem crescente de passo: busca binária
		size_t Lo = 0;
		size_t Hi = Ticks.size();
		while (Lo < Hi)
		{
			const size_t Mid = (Lo + Hi) / 2;
			if (Ticks[Mid].Tick < Tick) { Lo = Mid + 1; } else { Hi = Mid; }
		}
		if (Lo < Ticks.size() && Ticks[Lo].Tick == Tick)
		{
			return Ticks[Lo].Commands;
		}
		return {};
	}

	bool ReplayReader::ExpectedHash(TickNumber Tick, uint64_t& Out) const
	{
		// gravados em ordem de passo (o do fim pode repetir o último: vale o último)
		size_t Lo = 0;
		size_t Hi = Hashes.size();
		while (Lo < Hi)
		{
			const size_t Mid = (Lo + Hi) / 2;
			if (Hashes[Mid].Tick <= Tick) { Lo = Mid + 1; } else { Hi = Mid; }
		}
		if (Lo > 0 && Hashes[Lo - 1].Tick == Tick)
		{
			Out = Hashes[Lo - 1].Hash;
			return true;
		}
		return false;
	}
}
