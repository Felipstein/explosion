#include "Sim/Defs.h"

#include "Sim/Hash.h"

namespace Sim
{
	UnitDef UnitDef::FromSpec(const UnitSpec& S)
	{
		UnitDef D;
		D.Key = S.Key;
		D.MaxHp = S.MaxHp;
		D.Armor = S.Armor;
		D.Kind = S.Kind;
		D.HalfX = Fix::FromMilli(S.SizeXMm) / 2;
		D.HalfY = Fix::FromMilli(S.SizeYMm) / 2;
		// prédio sem raio no dado: o círculo inscrito no lado menor (é o que a arma mede até a parede)
		const int32_t StructureRadiusMm = (S.SizeXMm < S.SizeYMm ? S.SizeXMm : S.SizeYMm) / 2;
		D.Radius = Fix::FromMilli(S.RadiusMm > 0 || S.Kind != ObjectKind::Structure ? S.RadiusMm : StructureRadiusMm);
		D.PathRadius = Fix::FromMilli(S.PathRadiusMm > 0 ? S.PathRadiusMm : S.RadiusMm);
		D.MaxSpeed = PerTickFromMilliPerSecond(S.SpeedMmPerS);
		D.Accel = PerTick2FromMilliPerSecond2(S.AccelMmPerS2);
		D.Brake = PerTick2FromMilliPerSecond2(S.BrakeMmPerS2);
		D.TurnBam = BamPerTickFromMilliDegPerSecond(S.TurnMilliDegPerS);
		D.TurretTurnBam = BamPerTickFromMilliDegPerSecond(S.TurretTurnMilliDegPerS);
		D.TurretIdleReturnTicks = TicksFromMs(S.TurretIdleReturnMs);
		D.Sight = Fix::FromMilli(S.SightMm);
		D.Weapon.Range = Fix::FromMilli(S.Weapon.RangeMm);
		D.Weapon.Damage = S.Weapon.Damage;
		D.Weapon.Type = S.Weapon.Type;
		D.Weapon.ReloadTicks = TicksFromMs(S.Weapon.ReloadMs);
		D.Weapon.AimToleranceBam = Angle::FromMilliDeg(S.Weapon.AimToleranceMilliDeg).Bam;
		D.Weapon.ProjectileStep = PerTickFromMilliPerSecond(S.Weapon.ProjectileSpeedMmPerS);
		D.Weapon.ClipSize = S.Weapon.ClipSize;
		D.Weapon.ClipReloadTicks = TicksFromMs(S.Weapon.ClipReloadMs);
		D.Weapon.MissileLaunchStep = PerTickFromMilliPerSecond(S.Weapon.MissileLaunchSpeedMmPerS);
		D.Weapon.MissileAccel = PerTick2FromMilliPerSecond2(S.Weapon.MissileAccelMmPerS2);
		D.Weapon.MissileTurnBam = BamPerTickFromMilliDegPerSecond(S.Weapon.MissileTurnMilliDegPerS);
		D.Weapon.MissileBoostTicks = TicksFromMs(S.Weapon.MissileBoostMs);
		D.Weapon.MissileLaunchPitch = Angle::FromMilliDeg(S.Weapon.MissileLaunchPitchMilliDeg);
		D.Weapon.MissileLaunchHeight = Fix::FromMilli(S.Weapon.MissileLaunchHeightMm);
		D.Weapon.MissileSpreadBam = Angle::FromMilliDeg(S.Weapon.MissileSpreadMilliDeg).Bam;
		D.Weapon.MissileLifeTicks = TicksFromMs(S.Weapon.MissileLifeMs);
		D.CrusherLevel = S.CrusherLevel;
		D.AssistRange = Fix::FromMilli(S.AssistRangeMm);
		D.HullHalfLength = S.HullHalfLengthMm > 0 ? Fix::FromMilli(S.HullHalfLengthMm) : D.Radius * Fix::FromMilli(1080);
		D.HullHalfWidth = S.HullHalfWidthMm > 0 ? Fix::FromMilli(S.HullHalfWidthMm) : D.Radius * Fix::FromMilli(510);
		D.WreckLifeTicks = S.WreckLifeMs > 0 ? TicksFromMs(S.WreckLifeMs) : 0;
		D.WreckSinkTicks = S.WreckSinkMs > 0 ? TicksFromMs(S.WreckSinkMs) : 1;
		D.Cost = S.Cost;
		D.BuildTicks = TicksFromMs(S.BuildTimeMs);
		D.Energy = S.Energy;
		D.SupplyBoxes = S.SupplyBoxes;
		D.bSupplyCenter = S.bSupplyCenter;
		D.bCountsForVictory = S.bCountsForVictory;
		D.ExitLane = Fix::FromMilli(S.ExitLaneMm);
		D.bNeedsPower = S.bNeedsPower;
		// (v4, 22/10) o alcance mínimo, o dano em área, a patente, o teto por jogador, os poderes e o avião de carga
		D.Weapon.MinRange = Fix::FromMilli(S.Weapon.MinRangeMm);
		D.Weapon.SplashFull = Fix::FromMilli(S.Weapon.SplashFullMm);
		D.Weapon.Splash = Fix::FromMilli(S.Weapon.SplashMm);
		D.Weapon.SplashEdgePct = S.Weapon.SplashEdgePct;
		D.RequiresRank = S.RequiresRank;
		D.MaxPerPlayer = S.MaxPerPlayer;
		D.bGrantsPowers = S.bGrantsPowers;
		D.AirliftFirstTicks = TicksFromMs(S.AirliftFirstMs);
		D.AirliftPeriodTicks = TicksFromMs(S.AirliftPeriodMs);
		D.AirliftLeadTicks = TicksFromMs(S.AirliftLeadMs);
		D.AirliftPalletsMin = S.AirliftPalletsMin;
		D.AirliftPalletsMax = S.AirliftPalletsMax;
		D.AirliftBoxesPerPallet = S.AirliftBoxesPerPallet;
		D.AirliftUnloadTicks = TicksFromMs(S.AirliftUnloadMsPerPallet);
		// (v4, 23/10) a mira antes do tiro, a boca do tubo, o conserto do Dozer e a massa
		D.Weapon.PreAttackTicks = S.Weapon.PreAttackMs > 0 ? TicksFromMs(S.Weapon.PreAttackMs) : 0;
		D.Weapon.LaunchPivot = Vec2::Make(Fix::FromMilli(S.Weapon.LaunchPivotXMm), Fix::FromMilli(S.Weapon.LaunchPivotYMm));
		D.Weapon.LaunchForward = Fix::FromMilli(S.Weapon.LaunchForwardMm);
		D.Weapon.LaunchSide = Fix::FromMilli(S.Weapon.LaunchSideMm);
		D.Weapon.LaunchUp = Fix::FromMilli(S.Weapon.LaunchUpMm);
		D.RepairMilliPctPerS = S.RepairMilliPctPerS;
		D.MassKg = S.MassKg;
		return D;
	}

	PowerDef PowerDef::FromSpec(const PowerSpec& S)
	{
		PowerDef P;
		P.Key = S.Key;
		P.RequiresRank = S.RequiresRank;
		P.UpgradeRank = S.UpgradeRank;
		P.CooldownTicks = TicksFromMs(S.CooldownMs);
		P.Missiles = S.Missiles;
		// sem o número crescido, fica o de sempre
		P.MissilesUpgraded = S.MissilesUpgraded > 0 ? S.MissilesUpgraded : S.Missiles;
		P.PatternRadius = Fix::FromMilli(S.PatternRadiusMm);
		P.PatternRadiusUpgraded = Fix::FromMilli(S.PatternRadiusUpgradedMm > 0 ? S.PatternRadiusUpgradedMm : S.PatternRadiusMm);
		P.IntervalTicks = TicksFromMs(S.IntervalMs);
		P.SpawnBack = Fix::FromMilli(S.SpawnBackMm);
		P.SpawnUp = Fix::FromMilli(S.SpawnUpMm);
		return P;
	}

	int32_t Ruleset::FindUnit(const std::string& Key) const
	{
		for (size_t I = 0; I < Units.size(); ++I)
		{
			if (Units[I].Key == Key)
			{
				return int32_t(I);
			}
		}
		return -1;
	}

	bool Ruleset::Build(const std::vector<UnitSpec>& Specs, const ArmorTable& InArmor, const EconomyRules& InEconomy, Ruleset& Out, std::string* OutError)
	{
		return Build(Specs, {}, InArmor, InEconomy, Out, OutError);
	}

	bool Ruleset::Build(const std::vector<UnitSpec>& Specs, const std::vector<PowerSpec>& InPowers, const ArmorTable& InArmor, const EconomyRules& InEconomy, Ruleset& Out, std::string* OutError)
	{
		Out = Ruleset();
		Out.Armor = InArmor;
		Out.Economy = InEconomy;
		for (const UnitSpec& S : Specs)
		{
			Out.Units.push_back(UnitDef::FromSpec(S));
		}
		const auto Resolve = [&](const UnitSpec& S, const std::vector<std::string>& Keys, std::vector<uint16_t>& Into, bool bWantStructure)
		{
			for (const std::string& K : Keys)
			{
				const int32_t Index = Out.FindUnit(K);
				if (Index < 0 || Out.Units[size_t(Index)].IsStructure() != bWantStructure)
				{
					if (OutError)
					{
						*OutError = S.Key + ": " + (Index < 0 ? "chave desconhecida " : "tipo errado em ") + K;
					}
					return false;
				}
				Into.push_back(uint16_t(Index));
			}
			return true;
		};
		for (size_t I = 0; I < Specs.size(); ++I)
		{
			UnitDef& D = Out.Units[I];
			// prédio treina unidade, o Dozer constrói prédio e o prédio pronto solta unidade
			if (!Resolve(Specs[I], Specs[I].Produces, D.Produces, false) || !Resolve(Specs[I], Specs[I].Constructs, D.Constructs, true)
				|| !Resolve(Specs[I], Specs[I].FreeUnits, D.FreeUnits, false) || !Resolve(Specs[I], Specs[I].Requires, D.Requires, true))
			{
				return false;
			}
			if (D.IsStructure() && (D.HalfX.Raw <= 0 || D.HalfY.Raw <= 0))
			{
				if (OutError)
				{
					*OutError = D.Key + ": prédio sem tamanho no chão";
				}
				return false;
			}
			// (v4) a patente vai de 1 a MaxRank; o avião de carga precisa de paletes, caixas e um intervalo maior que o aviso
			const bool bAirliftOk = !D.HasAirlift() || (D.IsStructure() && D.AirliftPalletsMin > 0 && D.AirliftPalletsMax >= D.AirliftPalletsMin
				&& D.AirliftBoxesPerPallet > 0 && D.AirliftLeadTicks >= 0 && D.AirliftLeadTicks < D.AirliftPeriodTicks && D.AirliftFirstTicks > D.AirliftLeadTicks);
			if (D.RequiresRank < 0 || D.RequiresRank > MaxRank || D.MaxPerPlayer < 0 || !bAirliftOk
				|| D.Weapon.SplashEdgePct < 0 || D.Weapon.SplashEdgePct > 100 || D.Weapon.Splash < D.Weapon.SplashFull || D.Weapon.MinRange.Raw < 0)
			{
				if (OutError)
				{
					*OutError = D.Key + ": patente, teto, dano em área ou avião de carga inválido";
				}
				return false;
			}
			// (v4, 23/10) a mira, o conserto e a massa não são negativos; a boca do tubo fica acima do chão
			if (D.Weapon.PreAttackTicks < 0 || D.RepairMilliPctPerS < 0 || D.MassKg < 0 || D.Weapon.LaunchUp.Raw < 0)
			{
				if (OutError)
				{
					*OutError = D.Key + ": mira antes do tiro, boca do tubo, conserto ou massa inválidos";
				}
				return false;
			}
		}
		// (v4) vender: a % e a desmontagem (pelo menos 1 passo)
		if (InEconomy.SellPercent < 0 || InEconomy.SellPercent > 100 || InEconomy.SellTicks < 1)
		{
			if (OutError)
			{
				*OutError = "economia: venda inválida (sell_percent 0..100, sell_s > 0)";
			}
			return false;
		}
		// (v4) os poderes do general: a munição é uma unidade (não prédio) com arma de míssil
		if (InPowers.size() > size_t(MaxPowers))
		{
			if (OutError)
			{
				*OutError = "poderes demais (o máximo é " + std::to_string(MaxPowers) + ")";
			}
			return false;
		}
		for (const PowerSpec& S : InPowers)
		{
			PowerDef P = PowerDef::FromSpec(S);
			const int32_t Munition = Out.FindUnit(S.Munition);
			const char* Why = nullptr;
			if (Munition < 0)
			{
				Why = "munição desconhecida ";
			}
			else if (Out.Units[size_t(Munition)].IsStructure() || !Out.Units[size_t(Munition)].Weapon.IsMissile() || Out.Units[size_t(Munition)].Weapon.Damage <= 0)
			{
				Why = "a munição não é unidade com arma de míssil: ";
			}
			else if (P.Missiles < 1 || P.MissilesUpgraded < 1 || P.Missiles > MaxStrikeMissiles || P.MissilesUpgraded > MaxStrikeMissiles
				|| P.RequiresRank < 0 || P.RequiresRank > MaxRank || P.UpgradeRank < 0 || P.UpgradeRank > MaxRank || P.CooldownTicks < 0
				|| P.IntervalTicks < 0 || S.Key.empty() || S.Key.size() > 64)
			{
				Why = "números do poder inválidos: ";
			}
			if (Why)
			{
				if (OutError)
				{
					*OutError = S.Key + ": " + Why + S.Munition;
				}
				return false;
			}
			P.Munition = uint16_t(Munition);
			Out.Powers.push_back(P);
		}
		return true;
	}

	uint64_t Ruleset::Hash() const
	{
		Hasher H;
		H.Add(uint64_t(Units.size()));
		for (const UnitDef& D : Units)
		{
			H.Add(D.Key);
			H.Add(D.MaxHp);
			H.Add(uint8_t(D.Armor));
			H.Add(D.Radius);
			H.Add(D.PathRadius);
			H.Add(D.MaxSpeed);
			H.Add(D.Accel);
			H.Add(D.Brake);
			H.Add(D.TurnBam);
			H.Add(D.TurretTurnBam);
			H.Add(D.TurretIdleReturnTicks);
			H.Add(D.Sight);
			H.Add(D.Weapon.Range);
			H.Add(D.Weapon.Damage);
			H.Add(uint8_t(D.Weapon.Type));
			H.Add(D.Weapon.ReloadTicks);
			H.Add(D.Weapon.AimToleranceBam);
			H.Add(D.Weapon.ProjectileStep);
			H.Add(D.Weapon.ClipSize);
			H.Add(D.Weapon.ClipReloadTicks);
			H.Add(D.Weapon.MissileLaunchStep);
			H.Add(D.Weapon.MissileAccel);
			H.Add(D.Weapon.MissileTurnBam);
			H.Add(D.Weapon.MissileBoostTicks);
			H.Add(D.Weapon.MissileLaunchPitch.Bam);
			H.Add(D.Weapon.MissileLaunchHeight);
			H.Add(D.Weapon.MissileSpreadBam);
			H.Add(D.Weapon.MissileLifeTicks);
			H.Add(uint8_t(D.bNeedsPower ? 1 : 0));
			H.Add(D.CrusherLevel);
			H.Add(D.HullHalfLength);
			H.Add(D.HullHalfWidth);
			H.Add(D.AssistRange);
			H.Add(D.WreckLifeTicks);
			H.Add(D.WreckSinkTicks);
			H.Add(uint8_t(D.Kind));
			H.Add(D.Cost);
			H.Add(D.BuildTicks);
			H.Add(D.Energy);
			H.Add(D.HalfX);
			H.Add(D.HalfY);
			for (const std::vector<uint16_t>* List : { &D.Produces, &D.Constructs, &D.FreeUnits, &D.Requires })
			{
				H.Add(uint64_t(List->size()));
				for (const uint16_t K : *List)
				{
					H.Add(K);
				}
			}
			H.Add(D.SupplyBoxes);
			H.Add(D.bSupplyCenter);
			H.Add(D.bCountsForVictory);
			H.Add(D.ExitLane);
			// (v4) o alcance mínimo, o dano em área, a patente, o teto, os poderes e o avião de carga
			H.Add(D.Weapon.MinRange);
			H.Add(D.Weapon.SplashFull);
			H.Add(D.Weapon.Splash);
			H.Add(D.Weapon.SplashEdgePct);
			H.Add(D.RequiresRank);
			H.Add(D.MaxPerPlayer);
			H.Add(D.bGrantsPowers);
			H.Add(D.AirliftFirstTicks);
			H.Add(D.AirliftPeriodTicks);
			H.Add(D.AirliftLeadTicks);
			H.Add(D.AirliftPalletsMin);
			H.Add(D.AirliftPalletsMax);
			H.Add(D.AirliftBoxesPerPallet);
			H.Add(D.AirliftUnloadTicks);
			// (v4, 23/10) a mira antes do tiro, a boca do tubo, o conserto e a massa
			H.Add(D.Weapon.PreAttackTicks);
			H.Add(D.Weapon.LaunchPivot);
			H.Add(D.Weapon.LaunchForward);
			H.Add(D.Weapon.LaunchSide);
			H.Add(D.Weapon.LaunchUp);
			H.Add(D.RepairMilliPctPerS);
			H.Add(D.MassKg);
		}
		for (int A = 0; A < int(ArmorType::Count); ++A)
		{
			for (int T = 0; T < int(DamageType::Count); ++T)
			{
				H.Add(Armor.Percent[A][T]);
			}
		}
		H.Add(Economy.StartingMoney);
		H.Add(Economy.BoxValue);
		H.Add(Economy.TicksPerBox);
		H.Add(Economy.CenterTicks);
		H.Add(Economy.DockScanDistance);
		H.Add(Economy.MaxQueue);
		H.Add(Economy.LowPowerMinPermille);
		H.Add(Economy.LowPowerMaxPermille);
		H.Add(Economy.MaxFlatnessCm);
		H.Add(Economy.EdgeMargin);
		H.Add(Economy.SupplyCenterDockGap);
		H.Add(Economy.DockClearCells);
		H.Add(Economy.DropPileHalf);
		// (v4) a patente e os poderes do general
		for (const int32_t Xp : Economy.RankXp)
		{
			H.Add(Xp);
		}
		H.Add(Economy.XpDealtDivisor);
		H.Add(Economy.XpTakenDivisor);
		H.Add(Economy.SellPercent);   // (v4, 23/10) vender
		H.Add(Economy.SellTicks);
		H.Add(uint64_t(Powers.size()));
		for (const PowerDef& P : Powers)
		{
			H.Add(P.Key);
			H.Add(P.RequiresRank);
			H.Add(P.UpgradeRank);
			H.Add(P.CooldownTicks);
			H.Add(P.Missiles);
			H.Add(P.MissilesUpgraded);
			H.Add(P.PatternRadius);
			H.Add(P.PatternRadiusUpgraded);
			H.Add(P.IntervalTicks);
			H.Add(P.SpawnBack);
			H.Add(P.SpawnUp);
			H.Add(P.Munition);
		}
		return H.Value();
	}
}
