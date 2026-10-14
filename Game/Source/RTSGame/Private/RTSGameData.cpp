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
		if (!bOk || S.Key.empty())
		{
			OutError = FString::Printf(TEXT("unidade \"%s\" com campo faltando ou inválido"), UTF8_TO_TCHAR(S.Key.c_str()));
			return false;
		}
		Specs.push_back(S);

		FRTSUnitVisual V;
		V.Key = UTF8_TO_TCHAR(S.Key.c_str());
		V.DisplayName = U->GetStringField(TEXT("name"));
		V.HullMeshes = Strings(*Visual, TEXT("hull"));
		V.TurretMeshes = Strings(*Visual, TEXT("turret"));
		(*Visual)->TryGetNumberField(TEXT("muzzle_height_m"), V.MuzzleHeightM);
		const TArray<TSharedPtr<FJsonValue>>* Pivot = nullptr;
		if ((*Visual)->TryGetArrayField(TEXT("turret_pivot_m"), Pivot) && Pivot->Num() >= 2)
		{
			V.TurretPivotM = FVector2D((*Pivot)[0]->AsNumber(), (*Pivot)[1]->AsNumber());
		}
		const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
		if ((*Visual)->TryGetArrayField(TEXT("tracks_m"), Tracks) && Tracks->Num() >= 2)
		{
			V.TracksM = FVector2D((*Tracks)[0]->AsNumber(), (*Tracks)[1]->AsNumber());
		}
		FString Crater;
		if ((*Visual)->TryGetStringField(TEXT("impact_crater"), Crater) && Crater.Len() == 1)
		{
			V.ImpactCrater = FMath::Clamp(int32(FChar::ToUpper(Crater[0]) - TEXT('A')), 0, 2);
		}
		(*Visual)->TryGetNumberField(TEXT("near_miss"), V.NearMissShare);
		V.RadiusM = float(U->GetNumberField(TEXT("radius_m")));
		ReadStructureVisual(*Visual, V);
		Visuals.Add(V);
	}
	std::string BuildError;
	if (!Sim::Ruleset::Build(Specs, ArmorTable, Economy, Rules, &BuildError))
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
		M->TryGetStringField(TEXT("village"), Info.VillageFile);
		M->TryGetStringField(TEXT("surface"), Info.SurfaceFile);
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
		S->TryGetNumberField(TEXT("end_s"), Out.EndS);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
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
				const TArray<TSharedPtr<FJsonValue>>* To = nullptr;
				if (O->TryGetArrayField(TEXT("to_m"), To) && To->Num() >= 2)
				{
					Order.ToM = FVector2D((*To)[0]->AsNumber(), (*To)[1]->AsNumber());
				}
				Out.Orders.Add(Order);
			}
			Out.Orders.StableSort([](const FRTSScenarioOrder& A, const FRTSScenarioOrder& B) { return A.AtS < B.AtS; });
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
			const TArray<TSharedPtr<FJsonValue>>* Pan = nullptr;
			if ((*Camera)->TryGetArrayField(TEXT("pan_m_s"), Pan) && Pan->Num() >= 2)
			{
				Out.CameraPanMS = FVector2D((*Pan)[0]->AsNumber(), (*Pan)[1]->AsNumber());
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
