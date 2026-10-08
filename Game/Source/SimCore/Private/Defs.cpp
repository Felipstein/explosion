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
		D.Radius = Fix::FromMilli(S.RadiusMm);
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
		}
		for (int A = 0; A < int(ArmorType::Count); ++A)
		{
			for (int T = 0; T < int(DamageType::Count); ++T)
			{
				H.Add(Armor.Percent[A][T]);
			}
		}
		return H.Value();
	}
}
