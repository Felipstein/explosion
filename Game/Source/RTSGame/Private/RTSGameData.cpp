#include "RTSGameData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTSConvert.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJson(const FString& File, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("não achei %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			OutError = FString::Printf(TEXT("%s não é um JSON válido: %s"), *Path, *Reader->GetErrorMessage());
			return false;
		}
		return true;
	}

	bool DamageTypeFrom(const FString& Name, Sim::DamageType& Out)
	{
		if (Name == TEXT("armor_piercing")) { Out = Sim::DamageType::ArmorPiercing; return true; }
		if (Name == TEXT("small_arms")) { Out = Sim::DamageType::SmallArms; return true; }
		if (Name == TEXT("explosive")) { Out = Sim::DamageType::Explosive; return true; }
		return false;
	}

	bool ArmorTypeFrom(const FString& Name, Sim::ArmorType& Out)
	{
		if (Name == TEXT("tank")) { Out = Sim::ArmorType::Tank; return true; }
		return false;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, bool& bOk)
	{
		double V = 0.0;
		if (!O->TryGetNumberField(Field, V))
		{
			bOk = false;
		}
		return V;
	}

	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FString> Out;
		O->TryGetStringArrayField(Field, Out);
		return Out;
	}
}

bool FRTSGameData::Load(FString& OutError)
{
	*this = FRTSGameData();

	TSharedPtr<FJsonObject> UnitsJson;
	if (!ReadJson(TEXT("Units.json"), UnitsJson, OutError))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Armor = nullptr;
	if (UnitsJson->TryGetObjectField(TEXT("armor"), Armor))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& A : (*Armor)->Values)
		{
			Sim::ArmorType Type;
			if (!ArmorTypeFrom(A.Key, Type))
			{
				OutError = FString::Printf(TEXT("armadura desconhecida: %s"), *A.Key);
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& D : A.Value->AsObject()->Values)
			{
				Sim::DamageType Damage;
				if (!DamageTypeFrom(D.Key, Damage))
				{
					OutError = FString::Printf(TEXT("tipo de dano desconhecido: %s"), *D.Key);
					return false;
				}
				Rules.Armor.Set(Type, Damage, int32(FMath::RoundToInt(D.Value->AsNumber())));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Units = nullptr;
	if (!UnitsJson->TryGetArrayField(TEXT("units"), Units))
	{
		OutError = TEXT("Units.json sem a lista \"units\"");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Units)
	{
		const TSharedPtr<FJsonObject> U = Value->AsObject();
		const TSharedPtr<FJsonObject>* Weapon = nullptr;
		const TSharedPtr<FJsonObject>* Visual = nullptr;
		if (!U.IsValid() || !U->TryGetObjectField(TEXT("weapon"), Weapon) || !U->TryGetObjectField(TEXT("visual"), Visual))
		{
			OutError = TEXT("unidade sem \"weapon\" ou \"visual\"");
			return false;
		}
		bool bOk = true;
		Sim::UnitSpec S;
		S.Key = TCHAR_TO_UTF8(*U->GetStringField(TEXT("key")));
		S.MaxHp = int32(Num(U, TEXT("max_hp"), bOk));
		bOk &= ArmorTypeFrom(U->GetStringField(TEXT("armor")), S.Armor);
		S.RadiusMm = RTS::Milli(Num(U, TEXT("radius_m"), bOk));
		S.PathRadiusMm = RTS::Milli(Num(U, TEXT("path_radius_m"), bOk));
		S.SpeedMmPerS = RTS::Milli(Num(U, TEXT("speed_mps"), bOk));
		S.AccelMmPerS2 = RTS::Milli(Num(U, TEXT("accel_mps2"), bOk));
		S.BrakeMmPerS2 = RTS::Milli(Num(U, TEXT("brake_mps2"), bOk));
		S.TurnMilliDegPerS = RTS::Milli(Num(U, TEXT("turn_dps"), bOk));
		S.TurretTurnMilliDegPerS = RTS::Milli(Num(U, TEXT("turret_turn_dps"), bOk));
		S.TurretIdleReturnMs = RTS::Milli(Num(U, TEXT("turret_idle_return_s"), bOk));
		S.SightMm = RTS::Milli(Num(U, TEXT("sight_m"), bOk));
		S.Weapon.RangeMm = RTS::Milli(Num(*Weapon, TEXT("range_m"), bOk));
		S.Weapon.Damage = int32(Num(*Weapon, TEXT("damage"), bOk));
		bOk &= DamageTypeFrom((*Weapon)->GetStringField(TEXT("damage_type")), S.Weapon.Type);
		S.Weapon.ReloadMs = RTS::Milli(Num(*Weapon, TEXT("reload_s"), bOk));
		S.Weapon.AimToleranceMilliDeg = RTS::Milli(Num(*Weapon, TEXT("aim_tolerance_deg"), bOk));
		S.Weapon.ProjectileSpeedMmPerS = RTS::Milli(Num(*Weapon, TEXT("projectile_mps"), bOk));
		if (!bOk || S.Key.empty())
		{
			OutError = FString::Printf(TEXT("unidade \"%s\" com campo faltando ou inválido"), UTF8_TO_TCHAR(S.Key.c_str()));
			return false;
		}
		Rules.Units.push_back(Sim::UnitDef::FromSpec(S));

		FRTSUnitVisual V;
		V.Key = UTF8_TO_TCHAR(S.Key.c_str());
		V.DisplayName = U->GetStringField(TEXT("name"));
		V.HullMeshes = Strings(*Visual, TEXT("hull"));
		V.TurretMeshes = Strings(*Visual, TEXT("turret"));
		(*Visual)->TryGetNumberField(TEXT("muzzle_height_m"), V.MuzzleHeightM);
		V.RadiusM = float(U->GetNumberField(TEXT("radius_m")));
		Visuals.Add(V);
	}

	TSharedPtr<FJsonObject> MapsJson;
	if (!ReadJson(TEXT("Maps.json"), MapsJson, OutError))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* MapList = nullptr;
	if (!MapsJson->TryGetArrayField(TEXT("maps"), MapList))
	{
		OutError = TEXT("Maps.json sem a lista \"maps\"");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *MapList)
	{
		const TSharedPtr<FJsonObject> M = Value->AsObject();
		FRTSMapInfo Info;
		Info.Name = M->GetStringField(TEXT("name"));
		Info.Title = M->GetStringField(TEXT("title"));
		M->TryGetNumberField(TEXT("size_m"), Info.SizeM);
		M->TryGetNumberField(TEXT("cell_m"), Info.CellM);
		M->TryGetNumberField(TEXT("sample_m"), Info.SampleM);
		M->TryGetNumberField(TEXT("max_slope"), Info.MaxSlope);
		M->TryGetNumberField(TEXT("water_below_m"), Info.WaterBelowM);
		Info.ObstacleMeshes = Strings(M, TEXT("obstacle_meshes"));
		auto ReadPoints = [&M](const TCHAR* Field, TArray<FVector2D>& Out)
		{
			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (M->TryGetArrayField(Field, Points))
			{
				for (const TSharedPtr<FJsonValue>& S : *Points)
				{
					const TArray<TSharedPtr<FJsonValue>>& XY = S->AsArray();
					if (XY.Num() == 2)
					{
						Out.Add(FVector2D(XY[0]->AsNumber(), XY[1]->AsNumber()));
					}
				}
			}
		};
		ReadPoints(TEXT("starts"), Info.StartsM);
		M->TryGetNumberField(TEXT("skirmish_gap_m"), Info.SkirmishGapM);
		const TSharedPtr<FJsonObject>* Skirmish = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* PerPlayer = nullptr;
		if (M->TryGetObjectField(TEXT("skirmish"), Skirmish) && (*Skirmish)->TryGetArrayField(TEXT("units_per_player"), PerPlayer))
		{
			for (const TSharedPtr<FJsonValue>& Entry : *PerPlayer)
			{
				const TArray<TSharedPtr<FJsonValue>>& Pair = Entry->AsArray();
				if (Pair.Num() == 2)
				{
					Info.StartingUnits.Emplace(Pair[0]->AsString(), int32(Pair[1]->AsNumber()));
				}
			}
		}
		Maps.Add(Info);
	}
	return true;
}

const FRTSMapInfo* FRTSGameData::FindMap(const FString& Name) const
{
	return Maps.FindByPredicate([&Name](const FRTSMapInfo& M) { return M.Name == Name; });
}
