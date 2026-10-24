#include "RTSGameData.h"

#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
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
		if (Name == TEXT("structure")) { Out = Sim::ArmorType::Structure; return true; }
		return false;
	}

	/** Campo numérico que pode faltar (economia e construção: a unidade de combate não tem). */
	double Opt(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, double Default = 0.0)
	{
		double V = Default;
		O->TryGetNumberField(Field, V);
		return V;
	}

	std::vector<std::string> Keys(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FString> In;
		O->TryGetStringArrayField(Field, In);
		std::vector<std::string> Out;
		for (const FString& K : In)
		{
			Out.push_back(TCHAR_TO_UTF8(*K));
		}
		return Out;
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

	/** [x, y, z] (ou mais) em metros → cm; o que faltar fica 0. */
	FVector4 VectorM(const TArray<TSharedPtr<FJsonValue>>& A, bool bLastIsDegrees = false)
	{
		FVector4 V(0.0, 0.0, 0.0, 0.0);
		for (int32 K = 0; K < FMath::Min(4, A.Num()); ++K)
		{
			V[K] = A[K]->AsNumber() * (bLastIsDegrees && K == 3 ? 1.0 : 100.0);
		}
		return V;
	}

	FVector FieldM(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O->TryGetArrayField(Field, A) ? FVector(VectorM(*A)) : FVector::ZeroVector;
	}

	TArray<FVector4> ListM(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, bool bLastIsDegrees = false)
	{
		TArray<FVector4> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(Field, A))
		{
			for (const TSharedPtr<FJsonValue>& Item : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>* Inner = nullptr;
				if (Item->TryGetArray(Inner))
				{
					Out.Add(VectorM(*Inner, bLastIsDegrees));
				}
			}
		}
		return Out;
	}

	/** (v4, 22/10) Um número ou um par [o de sempre, o crescido] (os poderes: "missiles": [6, 9]); falso se faltar. */
	bool NumberOrPair(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, double& OutBase, double& OutUpgraded)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (O->TryGetArrayField(Field, Pair) && Pair->Num() >= 1)
		{
			OutBase = (*Pair)[0]->AsNumber();
			OutUpgraded = Pair->Num() >= 2 ? (*Pair)[1]->AsNumber() : OutBase;
			return true;
		}
		if (O->TryGetNumberField(Field, OutBase))
		{
			OutUpgraded = OutBase;
			return true;
		}
		return false;
	}

	/** (v4) A malha existe no projeto? (o "…/wheel_*" confere a primeira, wheel_001). O modelo provisório pode chegar depois
	 *  dos dados: sem ele, o visual cai no "fallback" (um desenho que já existe) em vez de a unidade ficar invisível. */
	bool MeshExists(const FString& PackagePath)
	{
		const FString Path = PackagePath.EndsWith(TEXT("*")) ? PackagePath.LeftChop(1) + TEXT("001") : PackagePath;
		return FPackageName::IsValidLongPackageName(Path) && FPackageName::DoesPackageExist(Path);
	}

	/** As peças do prédio, do Dozer e do caminhão (Units.json, "visual"). */
	void ReadStructureVisual(const TSharedPtr<FJsonObject>& Visual, FRTSUnitVisual& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* Spin = nullptr;
		if (Visual->TryGetArrayField(TEXT("spin"), Spin))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Spin)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (Item->TryGetObject(O))
				{
					FRTSSpinPart Part;
					Part.Mesh = (*O)->GetStringField(TEXT("mesh"));
					Part.AtCm = FieldM(*O, TEXT("at_m"));
					Part.Rpm = float(Opt(*O, TEXT("rpm"), 30.0));
					V.SpinParts.Add(Part);
				}
			}
		}
		Visual->TryGetStringField(TEXT("scaffold"), V.ScaffoldMesh);
		const TSharedPtr<FJsonObject>* Blade = nullptr;
		if (Visual->TryGetObjectField(TEXT("blade"), Blade))
		{
			V.BladeMesh = (*Blade)->GetStringField(TEXT("mesh"));
			V.BladeAtCm = FieldM(*Blade, TEXT("at_m"));
		}
		const TSharedPtr<FJsonObject>* Cargo = nullptr;
		if (Visual->TryGetObjectField(TEXT("cargo"), Cargo))
		{
			V.CargoMesh = (*Cargo)->GetStringField(TEXT("mesh"));
			for (const FVector4& Slot : ListM(*Cargo, TEXT("slots_m")))
			{
				V.CargoSlotsCm.Add(FVector(Slot));
			}
		}
	}

	/**
	 * O depósito: as caixas em pilhas iguais em cada vaga. Somem as de cima primeiro e
	 * espalhado pelo pátio (o W3DSupplyDraw do ZH esconde as caixas do modelo na proporção do
	 * que sobrou; aqui a ordem salta de vaga em vaga, pra o pátio esvaziar por igual).
	 */
	void ReadDockVisual(const TSharedPtr<FJsonObject>& O, FRTSDockVisual& Out)
	{
		O->TryGetStringField(TEXT("mesh"), Out.Mesh);
		O->TryGetStringField(TEXT("crate"), Out.CrateMesh);
		const TArray<FVector4> Pads = ListM(O, TEXT("pads_m"));
		const TArray<FVector4> Stack = ListM(O, TEXT("stack_m"));
		const int32 N = Pads.Num();
		// um passo primo com o número de vagas: visita todas sem repetir, longe uma da outra
		int32 Stride = FMath::Max(1, N / 3 + 1);
		while (N > 1 && FMath::GreatestCommonDivisor(Stride, N) != 1)
		{
			++Stride;
		}
		for (int32 Level = Stack.Num() - 1; Level >= 0; --Level)
		{
			for (int32 K = 0; K < N; ++K)
			{
				const FVector4& Pad = Pads[(K * Stride + Level) % N];
				Out.CrateSlotsCm.Add(FVector(Pad.X + Stack[Level].X, Pad.Y + Stack[Level].Y, Stack[Level].Z));
			}
		}
		Out.PileSlotsCm = ListM(O, TEXT("pile_m"), true);
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
	Sim::ArmorTable ArmorTable;
	Sim::EconomyRules Economy;
	std::vector<Sim::UnitSpec> Specs;
	const TSharedPtr<FJsonObject>* EconomyJson = nullptr;
	if (UnitsJson->TryGetObjectField(TEXT("economy"), EconomyJson))
	{
		const TSharedPtr<FJsonObject>& Ej = *EconomyJson;
		Economy.StartingMoney = int32(Opt(Ej, TEXT("starting_money")));
		// (v4) as escolhas do dinheiro inicial (o menu e a sala): em ordem e com o padrão no meio; senão, só o padrão
		DefaultStartingMoney = Economy.StartingMoney;
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Ej->TryGetArrayField(TEXT("starting_money_choices"), Choices))
		{
			for (const TSharedPtr<FJsonValue>& C : *Choices)
			{
				StartingMoneyChoices.Add(int32(C->AsNumber()));
			}
		}
		bool bSorted = true;
		for (int32 K = 1; K < StartingMoneyChoices.Num(); ++K)
		{
			bSorted &= StartingMoneyChoices[K] > StartingMoneyChoices[K - 1];
		}
		if (!bSorted || !StartingMoneyChoices.Contains(DefaultStartingMoney) || StartingMoneyChoices[0] <= 0)
		{
			StartingMoneyChoices = { DefaultStartingMoney };
		}
		Economy.BoxValue = int32(Opt(Ej, TEXT("box_value")));
		Economy.TicksPerBox = Sim::TicksFromMs(RTS::Milli(Opt(Ej, TEXT("seconds_per_box"))));
		Economy.CenterTicks = Sim::TicksFromMs(RTS::Milli(Opt(Ej, TEXT("center_s"))));
		Economy.DockScanDistance = Sim::Fix::FromMilli(RTS::Milli(Opt(Ej, TEXT("dock_scan_m"))));
		Economy.MaxQueue = int32(Opt(Ej, TEXT("max_queue"), 9.0));
		Economy.LowPowerMinPermille = RTS::Milli(Opt(Ej, TEXT("low_power_min"), 0.5));
		Economy.LowPowerMaxPermille = RTS::Milli(Opt(Ej, TEXT("low_power_max"), 0.8));
		Economy.MaxFlatnessCm = int32(FMath::RoundToInt(Opt(Ej, TEXT("max_flatness_m")) * 100.0));
		Economy.EdgeMargin = Sim::Fix::FromMilli(RTS::Milli(Opt(Ej, TEXT("edge_margin_m"))));
		Economy.SupplyCenterDockGap = Sim::Fix::FromMilli(RTS::Milli(Opt(Ej, TEXT("supply_center_dock_gap_m"))));
		Economy.DockClearCells = int32(Opt(Ej, TEXT("dock_clear_cells"), 1.0));
		Economy.DropPileHalf = Sim::Fix::FromMilli(RTS::Milli(Opt(Ej, TEXT("drop_pile_m"), 3.0)) / 2);
		// (v4, 23/10) vender (o BuildAssistant do ZH): a % do custo que volta (SellPercentage 50%) e a desmontagem (6 s)
		Economy.SellPercent = int32(FMath::RoundToInt(Opt(Ej, TEXT("sell_percent"), 50.0)));
		Economy.SellTicks = FMath::Max(1, Sim::TicksFromMs(RTS::Milli(Opt(Ej, TEXT("sell_s"), 6.0))));
	}
	// (v4, 22/10) a patente do general: o XP de cada patente (as 5, a 1ª é 0) e o valor de cada XP (1 por $10 destruído, 1 por
	// $30 perdido); sem o bloco, os números do EconomyRules
	const TSharedPtr<FJsonObject>* RankJson = nullptr;
	if (UnitsJson->TryGetObjectField(TEXT("rank"), RankJson))
	{
		const TArray<TSharedPtr<FJsonValue>>* Xp = nullptr;
		if ((*RankJson)->TryGetArrayField(TEXT("xp"), Xp))
		{
			if (Xp->Num() != Sim::MaxRank)
			{
				OutError = FString::Printf(TEXT("Units.json: rank.xp tem de ter %d números (um por patente)"), Sim::MaxRank);
				return false;
			}
			for (int32 K = 0; K < Sim::MaxRank; ++K)
			{
				Economy.RankXp[K] = int32(FMath::RoundToInt((*Xp)[K]->AsNumber()));
			}
		}
		Economy.XpDealtDivisor = int32(Opt(*RankJson, TEXT("dealt_value_per_xp"), Economy.XpDealtDivisor));
		Economy.XpTakenDivisor = int32(Opt(*RankJson, TEXT("taken_value_per_xp"), Economy.XpTakenDivisor));
	}
	// (v4) os poderes do general, na ordem (o índice vai no comando UsePower): o nome e o ícone ficam do lado da Unreal
	std::vector<Sim::PowerSpec> PowerSpecs;
	const TArray<TSharedPtr<FJsonValue>>* PowersJson = nullptr;
	if (UnitsJson->TryGetArrayField(TEXT("powers"), PowersJson))
	{
		for (const TSharedPtr<FJsonValue>& Value : *PowersJson)
		{
			const TSharedPtr<FJsonObject> Pj = Value.IsValid() ? Value->AsObject() : TSharedPtr<FJsonObject>();
			FString Key, Munition;
			if (!Pj.IsValid() || !Pj->TryGetStringField(TEXT("key"), Key) || !Pj->TryGetStringField(TEXT("munition"), Munition))
			{
				OutError = TEXT("Units.json: poder sem \"key\" ou sem \"munition\"");
				return false;
			}
			bool bOk = true;
			Sim::PowerSpec P;
			P.Key = TCHAR_TO_UTF8(*Key);
			P.Munition = TCHAR_TO_UTF8(*Munition);
			P.RequiresRank = int32(Num(Pj, TEXT("requires_rank"), bOk));
			P.UpgradeRank = int32(Opt(Pj, TEXT("upgrade_rank")));
			P.CooldownMs = RTS::Milli(Num(Pj, TEXT("cooldown_s"), bOk));
			double Missiles = 0.0, MissilesUp = 0.0, Radius = 0.0, RadiusUp = 0.0;
			bOk &= NumberOrPair(Pj, TEXT("missiles"), Missiles, MissilesUp);
			bOk &= NumberOrPair(Pj, TEXT("pattern_radius_m"), Radius, RadiusUp);
			P.Missiles = int32(FMath::RoundToInt(Missiles));
			P.MissilesUpgraded = int32(FMath::RoundToInt(MissilesUp));
			P.PatternRadiusMm = RTS::Milli(Radius);
			P.PatternRadiusUpgradedMm = RTS::Milli(RadiusUp);
			P.IntervalMs = RTS::Milli(Opt(Pj, TEXT("interval_s")));
			P.SpawnBackMm = RTS::Milli(Num(Pj, TEXT("spawn_back_m"), bOk));
			P.SpawnUpMm = RTS::Milli(Num(Pj, TEXT("spawn_up_m"), bOk));
			if (!bOk)
			{
				OutError = FString::Printf(TEXT("Units.json: o poder \"%s\" com campo faltando ou inválido"), *Key);
				return false;
			}
			PowerSpecs.push_back(P);
			FRTSPowerVisual PV;
			PV.Key = Key;
			Pj->TryGetStringField(TEXT("name"), PV.DisplayName);
			Pj->TryGetStringField(TEXT("icon"), PV.Icon);
			if (PV.DisplayName.IsEmpty())
			{
				PV.DisplayName = Key;
			}
			PowerVisuals.Add(PV);
		}
	}
	const TSharedPtr<FJsonObject>* Armor = nullptr;
	if (UnitsJson->TryGetObjectField(TEXT("armor"), Armor))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& A : (*Armor)->Values)
		{
			if (A.Key.StartsWith(TEXT("_")))
			{
				continue;   // anotação (fonte, comentário)
			}
			Sim::ArmorType Type;
			if (!ArmorTypeFrom(A.Key, Type))
			{
				OutError = FString::Printf(TEXT("armadura desconhecida: %s"), *A.Key);
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& D : A.Value->AsObject()->Values)
			{
				if (D.Key.StartsWith(TEXT("_")))
				{
					continue;
				}
				Sim::DamageType Damage;
				if (!DamageTypeFrom(D.Key, Damage))
				{
					OutError = FString::Printf(TEXT("tipo de dano desconhecido: %s"), *D.Key);
					return false;
				}
				ArmorTable.Set(Type, Damage, int32(FMath::RoundToInt(D.Value->AsNumber())));
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
		if (!U.IsValid() || !U->TryGetObjectField(TEXT("visual"), Visual))
		{
			OutError = TEXT("unidade sem \"visual\"");
			return false;
		}
		const bool bArmed = U->TryGetObjectField(TEXT("weapon"), Weapon);   // Dozer, caminhão e prédio vêm sem arma
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
		if (bArmed)
		{
			S.Weapon.RangeMm = RTS::Milli(Num(*Weapon, TEXT("range_m"), bOk));
			S.Weapon.Damage = int32(Num(*Weapon, TEXT("damage"), bOk));
			bOk &= DamageTypeFrom((*Weapon)->GetStringField(TEXT("damage_type")), S.Weapon.Type);
			S.Weapon.ReloadMs = RTS::Milli(Num(*Weapon, TEXT("reload_s"), bOk));
			S.Weapon.AimToleranceMilliDeg = RTS::Milli(Num(*Weapon, TEXT("aim_tolerance_deg"), bOk));
			S.Weapon.ProjectileSpeedMmPerS = RTS::Milli(Num(*Weapon, TEXT("projectile_mps"), bOk));
			// a rajada (opcional): clip_size tiros a reload_s, e clip_reload_s depois do último
			double ClipSize = 0.0, ClipReload = 0.0;
			if ((*Weapon)->TryGetNumberField(TEXT("clip_size"), ClipSize))
			{
				S.Weapon.ClipSize = int32(ClipSize);
				(*Weapon)->TryGetNumberField(TEXT("clip_reload_s"), ClipReload);
				S.Weapon.ClipReloadMs = RTS::Milli(ClipReload);
			}
			// (16/10) o míssil (opcional): sai inclinado da caixa, acelera, guia e explode no contato (a Patriot)
			const TSharedPtr<FJsonObject>* Missile = nullptr;
			if ((*Weapon)->TryGetObjectField(TEXT("missile"), Missile))
			{
				S.Weapon.MissileLaunchSpeedMmPerS = RTS::Milli(Num(*Missile, TEXT("launch_mps"), bOk));
				S.Weapon.MissileAccelMmPerS2 = RTS::Milli(Num(*Missile, TEXT("accel_mps2"), bOk));
				S.Weapon.MissileTurnMilliDegPerS = RTS::Milli(Num(*Missile, TEXT("turn_dps"), bOk));
				S.Weapon.MissileBoostMs = RTS::Milli(Num(*Missile, TEXT("boost_s"), bOk));
				S.Weapon.MissileLaunchPitchMilliDeg = RTS::Milli(Num(*Missile, TEXT("launch_pitch_deg"), bOk));
				S.Weapon.MissileLaunchHeightMm = RTS::Milli(Num(*Missile, TEXT("launch_height_m"), bOk));
				S.Weapon.MissileSpreadMilliDeg = RTS::Milli(Num(*Missile, TEXT("spread_deg"), bOk));
				S.Weapon.MissileLifeMs = RTS::Milli(Num(*Missile, TEXT("life_s"), bOk));
			}
			// (v4, 22/10) o alcance mínimo da artilharia e o dano em área: splash_m = [cheio até, zero além] (m, da borda do casco
			// ou do lote) e splash_edge_pct = a % que ainda chega na borda de fora (opcionais)
			S.Weapon.MinRangeMm = RTS::Milli(Opt(*Weapon, TEXT("min_range_m")));
			const TArray<TSharedPtr<FJsonValue>>* Splash = nullptr;
			if ((*Weapon)->TryGetArrayField(TEXT("splash_m"), Splash))
			{
				bOk &= Splash->Num() == 2;
				if (Splash->Num() == 2)
				{
					S.Weapon.SplashFullMm = RTS::Milli((*Splash)[0]->AsNumber());
					S.Weapon.SplashMm = RTS::Milli((*Splash)[1]->AsNumber());
				}
				S.Weapon.SplashEdgePct = int32(FMath::RoundToInt(Opt(*Weapon, TEXT("splash_edge_pct"))));
			}
			// (v4, 23/10) a mira antes do tiro (o AIM do TurretAI com o PreAttackDelay do ZH) e a boca do tubo: launch_offset_m = [pra
			// frente, pro lado (os tubos alternam), a altura do chão] a partir do pivô da torre (o turret_pivot_m do visual: a
			// simulação lê o principal, nunca o de reserva, pra a regra não depender de qual malha existe no PC)
			S.Weapon.PreAttackMs = RTS::Milli(Opt(*Weapon, TEXT("pre_attack_s")));
			const TArray<TSharedPtr<FJsonValue>>* Mouth = nullptr;
			if ((*Weapon)->TryGetArrayField(TEXT("launch_offset_m"), Mouth))
			{
				bOk &= Mouth->Num() == 3;
				if (Mouth->Num() == 3)
				{
					S.Weapon.LaunchForwardMm = RTS::Milli((*Mouth)[0]->AsNumber());
					S.Weapon.LaunchSideMm = RTS::Milli((*Mouth)[1]->AsNumber());
					S.Weapon.LaunchUpMm = RTS::Milli((*Mouth)[2]->AsNumber());
				}
				const TArray<TSharedPtr<FJsonValue>>* MouthPivot = nullptr;
				if ((*Visual)->TryGetArrayField(TEXT("turret_pivot_m"), MouthPivot) && MouthPivot->Num() >= 2)
				{
					S.Weapon.LaunchPivotXMm = RTS::Milli((*MouthPivot)[0]->AsNumber());
					S.Weapon.LaunchPivotYMm = RTS::Milli((*MouthPivot)[1]->AsNumber());
				}
			}
		}
		// (v4, 23/10) o conserto do Dozer (o RepairHealthPercentPerSecond do ZH, em % da vida cheia por segundo) e a massa do veículo
		// (o "mass_t" do visual principal; a simulação derruba a palmeira e o poste pelo choque, ½·m·v²)
		S.RepairMilliPctPerS = RTS::Milli(Opt(U, TEXT("repair_pct_per_s")));
		{
			double MassT = 0.0;
			(*Visual)->TryGetNumberField(TEXT("mass_t"), MassT);
			S.MassKg = int32(FMath::RoundToInt(MassT * 1000.0));
		}
		U->TryGetBoolField(TEXT("needs_power"), S.bNeedsPower);
		// (16/10) o esmagar do ZH (CrusherLevel) e a caixa do casco que pega o carro e a árvore
		double AssistRange = 0.0;
		if (U->TryGetNumberField(TEXT("assist_range_m"), AssistRange))
		{
			S.AssistRangeMm = RTS::Milli(AssistRange);   // (16/10) a ajuda entre as defesas
		}
		double Crusher = 0.0;
		if (U->TryGetNumberField(TEXT("crusher_level"), Crusher))
		{
			S.CrusherLevel = int32(Crusher);
		}
		const TArray<TSharedPtr<FJsonValue>>* Hull = nullptr;
		if (U->TryGetArrayField(TEXT("hull_m"), Hull) && Hull->Num() >= 2)
		{
			S.HullHalfLengthMm = RTS::Milli((*Hull)[0]->AsNumber() * 0.5);
			S.HullHalfWidthMm = RTS::Milli((*Hull)[1]->AsNumber() * 0.5);
		}
		// (22/10) a carcaça: sólida por wreck_life_s, afunda em wreck_sink_s e some (sem o campo, os 48 s e 7 s do UnitSpec)
		double WreckLife = 0.0, WreckSink = 0.0;
		if (U->TryGetNumberField(TEXT("wreck_life_s"), WreckLife))
		{
			S.WreckLifeMs = RTS::Milli(WreckLife);
		}
		if (U->TryGetNumberField(TEXT("wreck_sink_s"), WreckSink))
		{
			S.WreckSinkMs = RTS::Milli(WreckSink);
		}
		// economia e construção (todos opcionais)
		FString Kind;
		if (U->TryGetStringField(TEXT("kind"), Kind) && Kind == TEXT("structure"))
		{
			S.Kind = Sim::ObjectKind::Structure;
		}
		S.Cost = int32(Opt(U, TEXT("cost")));
		S.BuildTimeMs = RTS::Milli(Opt(U, TEXT("build_time_s")));
		S.Energy = int32(Opt(U, TEXT("energy")));
		const TArray<TSharedPtr<FJsonValue>>* SizeJson = nullptr;
		if (U->TryGetArrayField(TEXT("size_m"), SizeJson) && SizeJson->Num() == 2)
		{
			S.SizeXMm = RTS::Milli((*SizeJson)[0]->AsNumber());
			S.SizeYMm = RTS::Milli((*SizeJson)[1]->AsNumber());
		}
		S.Produces = Keys(U, TEXT("produces"));
		S.Constructs = Keys(U, TEXT("constructs"));
		S.FreeUnits = Keys(U, TEXT("free_units"));
		S.SupplyBoxes = int32(Opt(U, TEXT("supply_boxes")));
		U->TryGetBoolField(TEXT("supply_center"), S.bSupplyCenter);
		S.Requires = Keys(U, TEXT("requires"));
		U->TryGetBoolField(TEXT("counts_for_victory"), S.bCountsForVictory);
		S.ExitLaneMm = RTS::Milli(Opt(U, TEXT("exit_lane_m")));
		// (v4, 22/10) a patente que o tipo exige, o teto por jogador (fundações inclusive) e o prédio que liga os poderes
		S.RequiresRank = int32(Opt(U, TEXT("requires_rank")));
		S.MaxPerPlayer = int32(Opt(U, TEXT("max_per_player")));
		U->TryGetBoolField(TEXT("grants_powers"), S.bGrantsPowers);
		// (v4) o avião de carga da zona de lançamento: o 1º pouso, o intervalo, o aviso, os paletes [mín, máx] (a rota que
		// amadurece), as caixas por palete e a empilhadeira por palete; a altura do voo é só do desenho
		float AirliftAltitudeM = 70.0f;
		const TSharedPtr<FJsonObject>* Airlift = nullptr;
		if (U->TryGetObjectField(TEXT("airlift"), Airlift))
		{
			S.AirliftFirstMs = RTS::Milli(Num(*Airlift, TEXT("first_s"), bOk));
			S.AirliftPeriodMs = RTS::Milli(Num(*Airlift, TEXT("period_s"), bOk));
			S.AirliftLeadMs = RTS::Milli(Num(*Airlift, TEXT("lead_s"), bOk));
			S.AirliftBoxesPerPallet = int32(Num(*Airlift, TEXT("boxes_per_pallet"), bOk));
			S.AirliftUnloadMsPerPallet = RTS::Milli(Num(*Airlift, TEXT("unload_s_per_pallet"), bOk));
			double PalletsMin = 0.0, PalletsMax = 0.0;
			bOk &= NumberOrPair(*Airlift, TEXT("pallets"), PalletsMin, PalletsMax);
			S.AirliftPalletsMin = int32(FMath::RoundToInt(PalletsMin));
			S.AirliftPalletsMax = int32(FMath::RoundToInt(PalletsMax));
			(*Airlift)->TryGetNumberField(TEXT("altitude_m"), AirliftAltitudeM);
		}
		if (!bOk || S.Key.empty())
		{
			OutError = FString::Printf(TEXT("unidade \"%s\" com campo faltando ou inválido"), UTF8_TO_TCHAR(S.Key.c_str()));
			return false;
		}
		Specs.push_back(S);

		FRTSUnitVisual V;
		V.Key = UTF8_TO_TCHAR(S.Key.c_str());
		V.DisplayName = U->GetStringField(TEXT("name"));
		V.AirliftAltitudeM = AirliftAltitudeM;
		// (v4, 22/10) o modelo provisório pode chegar depois dos dados (a zona de lançamento, o Tomahawk): sem a malha do casco
		// no projeto, o visual inteiro vem do "fallback" (um desenho que já existe), em vez de a unidade ficar invisível
		TSharedPtr<FJsonObject> Vis = *Visual;
		const TSharedPtr<FJsonObject>* Fallback = nullptr;
		const TArray<FString> MainHull = Strings(Vis, TEXT("hull"));
		if (Vis->TryGetObjectField(TEXT("fallback"), Fallback) && Fallback->IsValid() && (MainHull.IsEmpty() || !MeshExists(MainHull[0])))
		{
			UE_LOG(LogTemp, Log, TEXT("RTS: %s sem a malha %s: o visual de reserva (fallback)"), *V.Key, MainHull.IsEmpty() ? TEXT("-") : *MainHull[0]);
			Vis = *Fallback;
			V.bFallbackVisual = true;
		}
		V.HullMeshes = Strings(Vis, TEXT("hull"));
		V.TurretMeshes = Strings(Vis, TEXT("turret"));
		Vis->TryGetNumberField(TEXT("muzzle_height_m"), V.MuzzleHeightM);
		Vis->TryGetNumberField(TEXT("turret_yaw_offset_deg"), V.TurretYawOffsetDeg);
		// (16/10) o diesel: as saídas e o motor
		const TSharedPtr<FJsonObject>* Exhaust = nullptr;
		if (Vis->TryGetObjectField(TEXT("exhaust"), Exhaust))
		{
			(*Exhaust)->TryGetNumberField(TEXT("power_kw"), V.ExhaustPowerKw);
			(*Exhaust)->TryGetNumberField(TEXT("mass_t"), V.ExhaustMassT);
			(*Exhaust)->TryGetNumberField(TEXT("roll"), V.ExhaustRoll);
			(*Exhaust)->TryGetNumberField(TEXT("work_load"), V.ExhaustWorkLoad);
			(*Exhaust)->TryGetNumberField(TEXT("gears"), V.ExhaustGears);
			(*Exhaust)->TryGetNumberField(TEXT("jet_sigma_m"), V.ExhaustJetSigmaM);
			(*Exhaust)->TryGetNumberField(TEXT("exit_ms"), V.ExhaustExitMs);
			U->TryGetNumberField(TEXT("speed_mps"), V.ExhaustMaxSpeedMs);
			U->TryGetNumberField(TEXT("turn_dps"), V.ExhaustTurnDps);
			const TArray<TSharedPtr<FJsonValue>>* Outs = nullptr;
			if ((*Exhaust)->TryGetArrayField(TEXT("outlets"), Outs))
			{
				for (const TSharedPtr<FJsonValue>& O : *Outs)
				{
					const TSharedPtr<FJsonObject>* Oo = nullptr;
					const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
					const TArray<TSharedPtr<FJsonValue>>* Dir = nullptr;
					if (O.IsValid() && O->TryGetObject(Oo) && (*Oo)->TryGetArrayField(TEXT("at_m"), At) && (*Oo)->TryGetArrayField(TEXT("dir"), Dir)
						&& At->Num() >= 3 && Dir->Num() >= 3)
					{
						V.ExhaustAtM.Add(FVector((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber()));
						V.ExhaustDir.Add(FVector((*Dir)[0]->AsNumber(), (*Dir)[1]->AsNumber(), (*Dir)[2]->AsNumber()).GetSafeNormal());
					}
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Pivot = nullptr;
		if (Vis->TryGetArrayField(TEXT("turret_pivot_m"), Pivot) && Pivot->Num() >= 2)
		{
			V.TurretPivotM = FVector2D((*Pivot)[0]->AsNumber(), (*Pivot)[1]->AsNumber());
		}
		// (v4, 23/10) o corpo da torre que voa na morte: [[x, y, z mín], [x, y, z máx]] em m no quadro das malhas
		const TArray<TSharedPtr<FJsonValue>>* TurretBox = nullptr;
		if (Vis->TryGetArrayField(TEXT("turret_box_m"), TurretBox) && TurretBox->Num() == 2)
		{
			const TArray<TSharedPtr<FJsonValue>>& Lo = (*TurretBox)[0]->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& Hi = (*TurretBox)[1]->AsArray();
			if (Lo.Num() >= 3 && Hi.Num() >= 3)
			{
				V.TurretBoxM = FBox(FVector(Lo[0]->AsNumber(), Lo[1]->AsNumber(), Lo[2]->AsNumber()), FVector(Hi[0]->AsNumber(), Hi[1]->AsNumber(), Hi[2]->AsNumber()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
		if (Vis->TryGetArrayField(TEXT("tracks_m"), Tracks) && Tracks->Num() >= 2)
		{
			V.TracksM = FVector2D((*Tracks)[0]->AsNumber(), (*Tracks)[1]->AsNumber());
		}
		FString Crater;
		if (Vis->TryGetStringField(TEXT("impact_crater"), Crater) && Crater.Len() == 1)
		{
			V.ImpactCrater = FMath::Clamp(int32(FChar::ToUpper(Crater[0]) - TEXT('A')), 0, 2);
		}
		Vis->TryGetNumberField(TEXT("near_miss"), V.NearMissShare);
		FString Blast;
		V.bHeavyImpact = Vis->TryGetStringField(TEXT("impact_blast"), Blast) && Blast == TEXT("classic");
		Vis->TryGetNumberField(TEXT("warhead_kg"), V.WarheadKg);   // (23/10) a ogiva do míssil (só desenho)
		FString WarheadLook;
		V.bWarheadClassicLook = Vis->TryGetStringField(TEXT("warhead_look"), WarheadLook) && WarheadLook == TEXT("classic");
		// (23/10, a explosão com vida) a munição e o alvo (só desenho; BlastFxHitLife.h)
		const TSharedPtr<FJsonObject>* HitLife = nullptr;
		if (Vis->TryGetObjectField(TEXT("hit_life"), HitLife))
		{
			(*HitLife)->TryGetNumberField(TEXT("fragment_tilt_deg"), V.FragmentTiltDeg);
			(*HitLife)->TryGetNumberField(TEXT("carry"), V.HitCarry);
			(*HitLife)->TryGetNumberField(TEXT("ejecta_kg"), V.EjectaKg);
			(*HitLife)->TryGetNumberField(TEXT("sand_kg"), V.SandKg);
			FString LifeCrater;
			if ((*HitLife)->TryGetStringField(TEXT("crater"), LifeCrater) && LifeCrater.Len() == 1)
			{
				V.LifeCrater = FMath::Clamp(int32(FChar::ToUpper(LifeCrater[0]) - TEXT('A')), 0, 2);
			}
			(*HitLife)->TryGetNumberField(TEXT("mark_m"), V.LifeMarkM);
			(*HitLife)->TryGetNumberField(TEXT("crater_depth"), V.LifeCraterDepth);
		}
		Vis->TryGetNumberField(TEXT("mass_t"), V.MassT);
		// (23/10, v4) as rodas e o lançador que levanta (só desenho; RTSUnitActor)
		const TSharedPtr<FJsonObject>* Wheels = nullptr;
		if (Vis->TryGetObjectField(TEXT("wheels"), Wheels))
		{
			(*Wheels)->TryGetNumberField(TEXT("tyre_w_m"), V.WheelTyreWidthM);
			(*Wheels)->TryGetNumberField(TEXT("dust_scale"), V.WheelDustScale);
			(*Wheels)->TryGetNumberField(TEXT("dust_amount"), V.WheelDustAmount);
			(*Wheels)->TryGetNumberField(TEXT("dust_spacing"), V.WheelDustSpacing);
			(*Wheels)->TryGetNumberField(TEXT("dust_trail_s"), V.WheelDustTrailS);
			(*Wheels)->TryGetNumberField(TEXT("mark_dark"), V.WheelMarkDark);
			(*Wheels)->TryGetNumberField(TEXT("mark_depth"), V.WheelMarkDepth);
			(*Wheels)->TryGetNumberField(TEXT("mark_lug"), V.WheelMarkLug);
			(*Wheels)->TryGetNumberField(TEXT("mark_segment_m"), V.WheelMarkSegmentM);
		}
		const TSharedPtr<FJsonObject>* Launcher = nullptr;
		if (Vis->TryGetObjectField(TEXT("launcher"), Launcher))
		{
			(*Launcher)->TryGetStringField(TEXT("mesh"), V.LauncherMesh);
			V.LauncherHingeCm = FieldM(*Launcher, TEXT("hinge_m"));
			(*Launcher)->TryGetNumberField(TEXT("fire_pitch_deg"), V.LauncherFirePitchDeg);
			(*Launcher)->TryGetNumberField(TEXT("raise_dps"), V.LauncherRaiseDps);
			(*Launcher)->TryGetNumberField(TEXT("accel_dps2"), V.LauncherAccelDps2);
			(*Launcher)->TryGetNumberField(TEXT("brake_dps2"), V.LauncherBrakeDps2);
			(*Launcher)->TryGetNumberField(TEXT("lower_dps"), V.LauncherLowerDps);
			(*Launcher)->TryGetNumberField(TEXT("lower_delay_s"), V.LauncherLowerDelayS);
			(*Launcher)->TryGetNumberField(TEXT("stop_hz"), V.LauncherStopHz);
			(*Launcher)->TryGetNumberField(TEXT("stop_zeta"), V.LauncherStopZeta);
			for (const FVector4& Tube : ListM(*Launcher, TEXT("tubes_m")))
			{
				V.LauncherTubesCm.Add(FVector(Tube));
			}
			(*Launcher)->TryGetStringArrayField(TEXT("noses"), V.LauncherNoseMeshes);
		}
		V.RadiusM = float(U->GetNumberField(TEXT("radius_m")));
		ReadStructureVisual(Vis, V);
		Visuals.Add(V);
	}
	std::string BuildError;
	// (17/10, o Felipe: "tinha que ter um modo fast pra teste, onde aumenta o dinheiro, diminui o tempo das construções e
	// unidades em 75% e aumenta a velocidade dos veículos em dobro") -RTSFast, só pros testes: as regras mudam (a versão da
	// sala difere: quem não tem a chave não entra)
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSFast")))
	{
		MoneyScale = 5;   // (v4) o dinheiro escolhido no menu também (a montagem leva o escolhido × MoneyScale)
		Economy.StartingMoney *= MoneyScale;
		for (Sim::UnitSpec& S : Specs)
		{
			S.BuildTimeMs /= 4;
			if (S.Kind != Sim::ObjectKind::Structure)
			{
				S.SpeedMmPerS *= 2;
				S.AccelMmPerS2 *= 2;
			}
		}
	}
	if (!Sim::Ruleset::Build(Specs, PowerSpecs, ArmorTable, Economy, Rules, &BuildError))
	{
		OutError = FString::Printf(TEXT("Units.json: %s"), UTF8_TO_TCHAR(BuildError.c_str()));
		return false;
	}
	const TSharedPtr<FJsonObject>* DockJson = nullptr;
	if (UnitsJson->TryGetObjectField(TEXT("supply_dock_visual"), DockJson))
	{
		ReadDockVisual(*DockJson, DockVisual);
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
		// (16/10) as árvores que o tanque derruba (a malha delas não fecha célula: vira objeto da simulação)
		Info.TreeMeshes = Strings(M, TEXT("tree_meshes"));
		if (Info.TreeMeshes.IsEmpty())
		{
			Info.TreeMeshes.Add(TEXT("DatePalm"));
		}
		M->TryGetStringField(TEXT("village"), Info.VillageFile);
		M->TryGetStringField(TEXT("surface"), Info.SurfaceFile);
		M->TryGetBoolField(TEXT("ground_v3"), Info.bGroundV3);
		M->TryGetStringField(TEXT("grid"), Info.GridFile);
		M->TryGetNumberField(TEXT("wind_yaw"), Info.WindYawDeg);
		M->TryGetNumberField(TEXT("wind_speed"), Info.WindSpeedMs);
		// o vento do WorldForge (o map.json do export, copiado pelo game.ps1 worlds) passa por cima: a areia v2 é
		// assada com ele
		FString WorldInfo;
		if (M->TryGetStringField(TEXT("world_info"), WorldInfo))
		{
			FString Text;
			TSharedPtr<FJsonObject> W;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), WorldInfo))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), W) && W.IsValid())
			{
				W->TryGetNumberField(TEXT("wind_yaw"), Info.WindYawDeg);
				W->TryGetNumberField(TEXT("wind_speed"), Info.WindSpeedMs);
			}
		}
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
		ReadPoints(TEXT("supply_docks"), Info.DocksM);
		const TArray<TSharedPtr<FJsonValue>>* DockSize = nullptr;
		if (M->TryGetArrayField(TEXT("dock_size_m"), DockSize) && DockSize->Num() == 2)
		{
			Info.DockSizeM = FVector2D((*DockSize)[0]->AsNumber(), (*DockSize)[1]->AsNumber());
		}
		M->TryGetNumberField(TEXT("dock_boxes"), Info.DockBoxes);
		M->TryGetNumberField(TEXT("skirmish_gap_m"), Info.SkirmishGapM);
		M->TryGetStringField(TEXT("look_recipe"), Info.LookRecipe);
		const TSharedPtr<FJsonObject>* Skirmish = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* PerPlayer = nullptr;
		if (M->TryGetObjectField(TEXT("skirmish"), Skirmish))
		{
			(*Skirmish)->TryGetStringField(TEXT("base"), Info.BaseStructure);
		}
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

bool FRTSGameData::LoadScenario(const FString& Name, FRTSScenario& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ReadJson(TEXT("Scenarios.json"), Root, OutError))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("scenarios"), List))
	{
		OutError = TEXT("Scenarios.json sem a lista \"scenarios\"");
		return false;
	}
	const auto Point = [](const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return A.Num() >= 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector;
	};
	for (const TSharedPtr<FJsonValue>& Value : *List)
	{
		const TSharedPtr<FJsonObject> S = Value->AsObject();
		if (!S.IsValid() || S->GetStringField(TEXT("name")) != Name)
		{
			continue;
		}
		Out = FRTSScenario();
		Out.Name = Name;
		S->TryGetStringField(TEXT("map"), Out.Map);
		S->TryGetNumberField(TEXT("seed"), Out.Seed);
		S->TryGetStringField(TEXT("fog"), Out.Fog);
		S->TryGetNumberField(TEXT("money"), Out.Money);
		S->TryGetNumberField(TEXT("end_s"), Out.EndS);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		// (v4, 22/10) a patente de largada: um número vale pra todos; a lista, um por jogador (presa em 1..5)
		double StartRank = 0.0;
		if (S->TryGetNumberField(TEXT("start_rank"), StartRank))
		{
			Out.StartRanks.Init(FMath::Clamp(int32(StartRank), 1, Sim::MaxRank), Sim::MaxPlayers);
		}
		else if (S->TryGetArrayField(TEXT("start_rank"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr) { Out.StartRanks.Add(FMath::Clamp(int32(V->AsNumber()), 1, Sim::MaxRank)); }
		}
		// (v4, 23/10) quem é a IA na cena (o cerco do Tomahawk, o ataque de mísseis da IA)
		if (S->TryGetArrayField(TEXT("ai"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr) { Out.AIPlayers.Add(V->AsBool()); }
		}
		if (S->TryGetArrayField(TEXT("starts_m"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr) { Out.StartsM.Add(Point(V)); }
		}
		if (S->TryGetArrayField(TEXT("units_per_player"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TArray<TSharedPtr<FJsonValue>>& Pair = V->AsArray();
				if (Pair.Num() == 2) { Out.UnitsPerPlayer.Emplace(Pair[0]->AsString(), int32(Pair[1]->AsNumber())); }
			}
		}
		if (S->TryGetArrayField(TEXT("orders"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FRTSScenarioOrder Order;
				O->TryGetNumberField(TEXT("t_s"), Order.AtS);
				O->TryGetNumberField(TEXT("player"), Order.Player);
				O->TryGetStringField(TEXT("type"), Order.Type);
				O->TryGetNumberField(TEXT("facing_deg"), Order.FacingDeg);
				O->TryGetNumberField(TEXT("width_m"), Order.WidthM);
				FString Size;
				if (O->TryGetStringField(TEXT("size"), Size) && Size.Len() == 1)
				{
					Order.MarkSize = FMath::Clamp(int32(FChar::ToUpper(Size[0]) - TEXT('A')), 0, 2);
				}
				O->TryGetBoolField(TEXT("formation"), Order.bFormation);
				O->TryGetNumberField(TEXT("unit"), Order.Unit);
				const TArray<TSharedPtr<FJsonValue>>* To = nullptr;
				if (O->TryGetArrayField(TEXT("to_m"), To) && To->Num() >= 2)
				{
					Order.ToM = FVector2D((*To)[0]->AsNumber(), (*To)[1]->AsNumber());
				}
				Out.Orders.Add(Order);
			}
			Out.Orders.StableSort([](const FRTSScenarioOrder& A, const FRTSScenarioOrder& B) { return A.AtS < B.AtS; });
		}
		if (S->TryGetArrayField(TEXT("structures"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
				FRTSScenarioStructure St;
				if (O.IsValid() && O->TryGetStringField(TEXT("key"), St.Key) && O->TryGetArrayField(TEXT("at_m"), At) && At->Num() >= 2)
				{
					O->TryGetNumberField(TEXT("player"), St.Player);
					St.AtM = FVector2D((*At)[0]->AsNumber(), (*At)[1]->AsNumber());
					Out.Structures.Add(St);
				}
			}
		}
		if (S->TryGetArrayField(TEXT("units"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
				FRTSScenarioUnit Un;
				if (O.IsValid() && O->TryGetStringField(TEXT("key"), Un.Key) && O->TryGetArrayField(TEXT("at_m"), At) && At->Num() >= 2)
				{
					O->TryGetNumberField(TEXT("player"), Un.Player);
					O->TryGetNumberField(TEXT("facing_deg"), Un.FacingDeg);
					Un.AtM = FVector2D((*At)[0]->AsNumber(), (*At)[1]->AsNumber());
					Out.Units.Add(Un);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Camera = nullptr;
		if (S->TryGetObjectField(TEXT("camera"), Camera))
		{
			const TArray<TSharedPtr<FJsonValue>>* Focus = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("focus_m"), Focus) && Focus->Num() >= 2)
			{
				Out.CameraFocusM = FVector2D((*Focus)[0]->AsNumber(), (*Focus)[1]->AsNumber());
			}
			(*Camera)->TryGetNumberField(TEXT("distance_m"), Out.CameraDistanceM);
			(*Camera)->TryGetNumberField(TEXT("yaw_deg"), Out.CameraYawDeg);
			(*Camera)->TryGetNumberField(TEXT("pitch_deg"), Out.CameraPitchDeg);
			(*Camera)->TryGetNumberField(TEXT("look_up_deg"), Out.CameraLookUpDeg);
			(*Camera)->TryGetNumberField(TEXT("follow_unit"), Out.FollowUnit);
			(*Camera)->TryGetNumberField(TEXT("follow_player"), Out.FollowPlayer);
			(*Camera)->TryGetNumberField(TEXT("follow_lag_s"), Out.FollowLagS);
			const TArray<TSharedPtr<FJsonValue>>* FollowScreen = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("follow_screen"), FollowScreen) && FollowScreen->Num() >= 2)
			{
				Out.FollowScreen = FVector2D((*FollowScreen)[0]->AsNumber(), (*FollowScreen)[1]->AsNumber());
			}
			Out.bSunRel = (*Camera)->TryGetNumberField(TEXT("sun_rel_deg"), Out.SunRelDeg);
			const TArray<TSharedPtr<FJsonValue>>* Pan = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("pan_m_s"), Pan) && Pan->Num() >= 2)
			{
				Out.CameraPanMS = FVector2D((*Pan)[0]->AsNumber(), (*Pan)[1]->AsNumber());
			}
			const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("keys"), Keys))
			{
				for (const TSharedPtr<FJsonValue>& KV : *Keys)
				{
					const TSharedPtr<FJsonObject> K = KV->AsObject();
					if (!K.IsValid())
					{
						continue;
					}
					FRTSScenarioCameraKey Key;
					Key.YawDeg = Out.CameraYawDeg;
					Key.DistanceM = Out.CameraDistanceM;
					Key.PitchDeg = Out.CameraPitchDeg;
					Key.LookUpDeg = Out.CameraLookUpDeg;
					K->TryGetNumberField(TEXT("t_s"), Key.AtS);
					K->TryGetNumberField(TEXT("yaw_deg"), Key.YawDeg);
					K->TryGetNumberField(TEXT("distance_m"), Key.DistanceM);
					K->TryGetNumberField(TEXT("pitch_deg"), Key.PitchDeg);
					K->TryGetNumberField(TEXT("look_up_deg"), Key.LookUpDeg);
					Out.CameraKeys.Add(Key);
				}
				Out.CameraKeys.StableSort([](const FRTSScenarioCameraKey& A, const FRTSScenarioCameraKey& B) { return A.AtS < B.AtS; });
			}
		}
		S->TryGetBoolField(TEXT("hud"), Out.bHud);
		if (S->TryGetArrayField(TEXT("shots_s"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr) { Out.ShotsS.Add(float(V->AsNumber())); }
		}
		return true;
	}
	OutError = FString::Printf(TEXT("o cenário %s não está no Scenarios.json"), *Name);
	return false;
}

const FRTSMapInfo* FRTSGameData::FindMap(const FString& Name) const
{
	return Maps.FindByPredicate([&Name](const FRTSMapInfo& M) { return M.Name == Name; });
}
