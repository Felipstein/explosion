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
		D.Cost = S.Cost;
		D.BuildTicks = TicksFromMs(S.BuildTimeMs);
		D.Energy = S.Energy;
		D.SupplyBoxes = S.SupplyBoxes;
		D.bSupplyCenter = S.bSupplyCenter;
		D.bCountsForVictory = S.bCountsForVictory;
		D.ExitLane = Fix::FromMilli(S.ExitLaneMm);
		return D;
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
		return H.Value();
	}
}
