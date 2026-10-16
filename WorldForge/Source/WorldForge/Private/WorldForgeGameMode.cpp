#include "WorldForgeGameMode.h"

#include "Generation/WorldDetail.h"
#include "Generation/WorldImport.h"
#include "Generation/WorldSettingsJson.h"

#include "Async/Async.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "Scalability.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "ForgeUITest.h"
#include "Generation/WorldGenerator.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Tasks/Task.h"
#include "UnrealClient.h"
#include "WorldForgeExport.h"
#include "WorldForgeTerrain.h"

struct FEditUndo
{
	AWorldForgeTerrain::FRemoved Removed;
	bool bPaint = false;
	TMap<FIntPoint, FWorldEdits::FPaint> Prev;   // pincelada: as células antes dela
	bool bHeight = false;
	TMap<FIntPoint, float> Applied;              // altura: o desvio somado por vértice no arrasto
	float FlatTarget = 0.0f;
	FVector2f Min = FVector2f(1e9f), Max = FVector2f(-1e9f);
	bool bRoads = false;                         // estrada: a lista de antes
	TArray<FWorldEdits::FRoadEdit> PrevRoads;
	TArray<FVector2f> PrevRemoved;
	bool bMove = false;                          // peça movida: onde estava e a edição de antes
	TOptional<FWorldEdits::FMoved> PrevMove;
	FWorldEdits::FMoved Move;
	int32 Stamp = 0;                             // o mapa mostrado quando a edição foi feita
};

AWorldForgeGameMode::AWorldForgeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AForgeCameraPawn::StaticClass();
	PlayerControllerClass = AForgePlayerController::StaticClass();
}

namespace
{
	FWorldGenSettings SettingsFrom(const FJsonObject& J);
}

void AWorldForgeGameMode::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("Seed="), Settings.Seed);
	FParse::Value(Cmd, TEXT("Players="), Settings.Players);
	FParse::Value(Cmd, TEXT("Size="), Settings.MapSize);
	int32 Biome = int32(Settings.Biome);
	FParse::Value(Cmd, TEXT("Biome="), Biome);
	Settings.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	FParse::Value(Cmd, TEXT("Forests="), Settings.Forests);
	FParse::Value(Cmd, TEXT("Frame="), Settings.Frame);   // tipo da borda (−1 sorteia)
	// voltando do "Ver em qualidade real": os mesmos controles de antes
	const bool bResumed = UGameplayStatics::HasOption(OptionsString, TEXT("resume"));
	if (UGameplayStatics::HasOption(OptionsString, TEXT("q")))
	{
		const int32 Q = UGameplayStatics::GetIntOption(OptionsString, TEXT("q"), -1);
		if (Q >= 0 && Q <= 3 && GEngine)
		{
			GEngine->Exec(GetWorld(), *FString::Printf(TEXT("Scalability %d"), Q));
		}
	}
	if (bResumed)
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		const FString Json = FPaths::ProjectSavedDir() / TEXT("WorldForge/world_settings.json");
		if (FFileHelper::LoadFileToString(Text, *Json) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
		{
			Settings.Seed = int32(J->GetNumberField(TEXT("seed")));
			Settings.Players = int32(J->GetNumberField(TEXT("players")));
			Settings.MapSize = float(J->GetNumberField(TEXT("size")));
			Settings.Biome = EWorldBiome(FMath::Clamp(int32(J->GetNumberField(TEXT("biome"))), 0, 2));
			Settings.Relief = float(J->GetNumberField(TEXT("relief")));
			Settings.Plateaus = float(J->GetNumberField(TEXT("plateaus")));
			Settings.Water = float(J->GetNumberField(TEXT("water")));
			Settings.Erosion = float(J->GetNumberField(TEXT("erosion")));
			Settings.Forests = float(J->GetNumberField(TEXT("forests")));
			Settings.Resources = float(J->GetNumberField(TEXT("resources")));
			Settings.Frame = J->HasField(TEXT("frame")) ? int32(J->GetNumberField(TEXT("frame"))) : -1;
			WorldSettingsJson::ReadVillage(*J, Settings);
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE resumed: seed %d, %d players, %.0f m, biome %d"), Settings.Seed, Settings.Players, Settings.MapSize, int32(Settings.Biome));
		if (FParse::Param(Cmd, TEXT("ForgeBack")))
		{
			FGenericPlatformMisc::RequestExit(false);   // teste do caminho de volta: chegou, fecha
			return;
		}
	}
	// teste do "Ver em qualidade real" (-ForgeRealQuality): o mesmo caminho do
	// botão, disparado quando o 1º mapa aparece; o -ForgeCapture fica pro mapa montado
	bAutoRealQuality = FParse::Param(Cmd, TEXT("ForgeRealQuality")) && !bResumed;

	// rascunho: as árvores de verdade com o Nanite bem mais grosso (triângulos
	// ~4× maiores na tela; de longe viram os voxels da folhagem mais cedo). O
	// mapa em qualidade real volta ao normal (EndPlay).
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))
	{
		V->Set(4.0f, ECVF_SetByCode);
	}

	Terrain = GetWorld()->SpawnActor<AWorldForgeTerrain>();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			Pawn->Terrain = Terrain;
		}
	}

	FString SettingsPath;
	if (FParse::Value(Cmd, TEXT("ForgeSettings="), SettingsPath))
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *SettingsPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
		{
			Settings = SettingsFrom(*J);
			UE_LOG(LogTemp, Display, TEXT("FORGE settings from %s: seed %d, %.0f m, biome %d, villages %d, %d keep-outs"),
				*SettingsPath, Settings.Seed, Settings.MapSize, int32(Settings.Biome), Settings.Villages, Settings.KeepOut.Num());
		}
	}
	if (!bAutoRealQuality && FParse::Value(Cmd, TEXT("ForgeCapture="), CaptureFolder))
	{
		FString Seeds;
		if (FParse::Value(Cmd, TEXT("ForgeSeeds="), Seeds, false))   // false: a vírgula faz parte do valor
		{
			TArray<FString> Parts;
			Seeds.ParseIntoArray(Parts, TEXT(","));
			for (const FString& P : Parts) CaptureSeeds.Add(FCString::Atoi(*P));
		}
		if (CaptureSeeds.Num() == 0) CaptureSeeds.Add(Settings.Seed);
		if (FParse::Param(Cmd, TEXT("ForgeUITest")))
		{
			UITest = MakeShared<FForgeUITest>(this, CaptureFolder);
		}
		Settings.Seed = CaptureSeeds[0];
		IFileManager::Get().MakeDirectory(*CaptureFolder, true);
	}
	Generate(Settings);
}

namespace
{
	bool SameSettings(const FWorldGenSettings& A, const FWorldGenSettings& B)
	{
		auto Same = [](float X, float Y) { return FMath::Abs(X - Y) < 1e-4f; };
		return A.Seed == B.Seed && A.Players == B.Players && Same(A.MapSize, B.MapSize) && A.Biome == B.Biome && Same(A.Relief, B.Relief)
			&& Same(A.Plateaus, B.Plateaus) && Same(A.Water, B.Water) && Same(A.Erosion, B.Erosion) && Same(A.Forests, B.Forests)
			&& Same(A.Resources, B.Resources) && A.Frame == B.Frame && A.Villages == B.Villages && A.KeepOut == B.KeepOut && A.ReliefType == B.ReliefType;
	}
}

void AWorldForgeGameMode::Generate(const FWorldGenSettings& NewSettings)
{
	// mapa importado: vale enquanto os controles forem os dele
	if (ImportedMap.IsValid() && !SameSettings(NewSettings, ImportedSettings))
	{
		ImportedMap.Reset();
		ImportFolder.Reset();
		ImportTitle.Reset();
		ImportNote.Reset();
	}
	const TSharedPtr<const FWorldMap> Source = ImportedMap;
	// semente nova = mundo novo: as edições à mão eram do mundo de antes. Os
	// outros controles mantêm as edições (o que não achar mais o alvo fica de fora)
	if (!Edits.IsValid() || NewSettings.Seed != Settings.Seed || NewSettings.Biome != Settings.Biome || NewSettings.MapSize != Settings.MapSize)
	{
		Edits = MakeShared<FWorldEdits>();
		UndoStack.Reset();
		RoadDeselect();
		RoadDraft.Reset();
	}
	Settings = NewSettings;
	const int32 Ticket = ++Pending;
	const FWorldGenSettings S = Settings;
	TSharedPtr<const FWorldEdits> EditsCopy = MakeShared<FWorldEdits>(*Edits);
	TWeakObjectPtr<AWorldForgeGameMode> Weak(this);
	// a geração é só dados (núcleo sem UObject): roda numa thread de trabalho
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [S, Ticket, Weak, EditsCopy, Source]()
	{
		TSharedPtr<FWorldMap> Map = Source.IsValid() ? MakeShared<FWorldMap>(*Source) : FWorldGenerator::Generate(S);
		// o mundo de verdade (o mesmo do "qualidade real"): pesos do chão, água e
		// a altura final, pra vista ao vivo mostrar o que vai sair (07/10)
		const double T0 = FPlatformTime::Seconds();
		TSharedPtr<FWorldLive> Live = FWorldLive::Build(Map, EditsCopy);
		Map->Timings.Add(TPair<FString, double>(TEXT("live"), 1000.0 * (FPlatformTime::Seconds() - T0)));
		AsyncTask(ENamedThreads::GameThread, [Weak, Ticket, Map, Live]()
		{
			if (AWorldForgeGameMode* GM = Weak.Get())
			{
				GM->OnGenerated(Ticket, Map, Live);
			}
		});
	});
}

void AWorldForgeGameMode::OnGenerated(int32 Ticket, TSharedPtr<FWorldMap> Map, TSharedPtr<FWorldLive> Live)
{
	// um pedido mais novo já está a caminho: este fica de fora (o painel não pisca)
	if (Ticket < Pending || Ticket <= Shown || !Terrain)
	{
		return;
	}
	const float OldSize = Terrain->GetMap() ? Terrain->GetMap()->Settings.MapSize : -1.0f;
	Shown = Ticket;
	Terrain->ShowMap(Map, Live);
	FString Times;
	for (const TPair<FString, double>& T : Map->Timings)
	{
		Times += FString::Printf(TEXT(" %s %.0f"), *T.Key, T.Value);
	}
	static const TCHAR* FrameNames[] = { TEXT("serra"), TEXT("colinas"), TEXT("costa"), TEXT("ilha") };
	UE_LOG(LogTemp, Display, TEXT("FORGE map seed %d players %d size %.0f: borda %s (lados %d%d%d%d), connected %d, rush %.0f m (%.2fx straight), ramps %d, open %.0f%%, props %d, resources %d · ms:%s"),
		Map->Settings.Seed, Map->Settings.Players, Map->Settings.MapSize, FrameNames[FMath::Min(int32(Map->Frame), 3)],
		Map->FrameSides[0], Map->FrameSides[1], Map->FrameSides[2], Map->FrameSides[3],
		Map->bConnected ? 1 : 0, Map->RushMeters, Map->RushMeters / FMath::Max(Map->RushStraight, 1.0f), Map->RampsCarved,
		100.0f * Map->OpenFraction, Map->Props.Num(), Map->Resources.Num(), *Times);

	if (bFirstMap || OldSize != Map->Settings.MapSize)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
			{
				Pawn->FrameWholeMap(true);
			}
		}
		bFirstMap = false;
	}
	OnMapShown.Broadcast();
	if (bAutoRealQuality)
	{
		AutoRealAt = FPlatformTime::Seconds() + 5.0;
		bAutoRealQuality = false;
	}
	if (!CaptureFolder.IsEmpty())
	{
		CaptureShot = 0;
		CaptureWait = 2.0;    // sombras virtuais, Lumen e TSR assentarem
	}
}

FString AWorldForgeGameMode::SaveCurrentMap()
{
	if (!Terrain || !Terrain->GetMap())
	{
		return FString();
	}
	const FWorldMap& M = *Terrain->GetMap();
	const FString Name = FString::Printf(TEXT("seed%d_%dp_%dm"), M.Settings.Seed, M.Settings.Players, FMath::RoundToInt(M.Settings.MapSize));
	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Maps"), Name);
	if (!WorldForgeExport::Save(M, Terrain->GetMinimapPixels(), Folder))
	{
		return FString();
	}
	if (Edits.IsValid() && !Edits->IsEmpty())
	{
		FFileHelper::SaveStringToFile(Edits->ToJson(), *(Folder / TEXT("edits.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	return FPaths::ConvertRelativePathToFull(Folder);
}

void AWorldForgeGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))
	{
		V->Set(1.0f, ECVF_SetByCode);
	}
	Super::EndPlay(Reason);
}

namespace
{
	/**
	 * Versão do gerador e da montagem: entra na chave das prévias guardadas.
	 * Subir quando o gerador (WorldGenerator) ou as regras do editor
	 * (WorldForgeEditorLib, WorldTrees.h) mudarem o mapa — o script de
	 * acabamento já entra pelo próprio conteúdo.
	 */
	const TCHAR* PipelineVersion = TEXT("2026-10-07b");   // d: estradas, campos, paredões, árvores transparentes; e: pedras assentadas; 07a: bordas por tipo, corpo dos platôs, leitos secos; 07b: drenagem (Priority-Flood), leitos largos, salina, gelo da costa, mar claro

	TSharedRef<FJsonObject> SettingsJson(const FWorldGenSettings& S)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetNumberField(TEXT("seed"), S.Seed);
		J->SetNumberField(TEXT("players"), S.Players);
		J->SetNumberField(TEXT("size"), S.MapSize);
		J->SetNumberField(TEXT("biome"), int32(S.Biome));
		J->SetNumberField(TEXT("relief"), S.Relief);
		J->SetNumberField(TEXT("plateaus"), S.Plateaus);
		J->SetNumberField(TEXT("water"), S.Water);
		J->SetNumberField(TEXT("erosion"), S.Erosion);
		J->SetNumberField(TEXT("forests"), S.Forests);
		J->SetNumberField(TEXT("resources"), S.Resources);
		J->SetNumberField(TEXT("frame"), S.Frame);
		WorldSettingsJson::WriteVillage(*J, S);
		return J;
	}

	FWorldGenSettings SettingsFrom(const FJsonObject& J)
	{
		FWorldGenSettings S;
		S.Seed = int32(J.GetNumberField(TEXT("seed")));
		S.Players = int32(J.GetNumberField(TEXT("players")));
		S.MapSize = float(J.GetNumberField(TEXT("size")));
		S.Biome = EWorldBiome(FMath::Clamp(int32(J.GetNumberField(TEXT("biome"))), 0, 2));
		S.Relief = float(J.GetNumberField(TEXT("relief")));
		S.Plateaus = float(J.GetNumberField(TEXT("plateaus")));
		S.Water = float(J.GetNumberField(TEXT("water")));
		S.Erosion = float(J.GetNumberField(TEXT("erosion")));
		S.Forests = float(J.GetNumberField(TEXT("forests")));
		S.Resources = float(J.GetNumberField(TEXT("resources")));
		S.Frame = J.HasField(TEXT("frame")) ? int32(J.GetNumberField(TEXT("frame"))) : -1;
		WorldSettingsJson::ReadVillage(J, S);
		return S;
	}
}

/** Opções do mapa em qualidade real: "real" (abre no máximo) e o preset pra devolver na volta. */
static FString RealOptions()
{
	return FString::Printf(TEXT("real?q=%d"), Scalability::GetQualityLevels().GetSingleQualityLevel());
}

/**
 * Os 3 mundos de exemplo (Tools/build_slice.py: SLICES, com os mesmos controles):
 * a prévia deles é o mundo montado à mão, um pedaço de 250 m com estrada,
 * paredões e tudo, não o mapa inteiro.
 */
static FString ExampleMapOf(const FWorldGenSettings& S)
{
	struct FExample { int32 Seed; EWorldBiome Biome; float Forests; const TCHAR* Map; };
	static const FExample Examples[] = {
		{ 3, EWorldBiome::Desert, 0.5f, TEXT("/Game/Maps/Slice_Desert") },
		{ 5, EWorldBiome::Winter, 0.7f, TEXT("/Game/Maps/Slice_Snow") },
		{ 42, EWorldBiome::Temperate, 0.85f, TEXT("/Game/Maps/Slice_Forest") },
	};
	const FWorldGenSettings D;   // os outros controles nos padrões
	auto Same = [](float A, float B) { return FMath::Abs(A - B) < 0.005f; };
	for (const FExample& E : Examples)
	{
		if (S.Seed == E.Seed && S.Biome == E.Biome && Same(S.Forests, E.Forests) && S.Players == D.Players && Same(S.MapSize, D.MapSize)
			&& Same(S.Relief, D.Relief) && Same(S.Plateaus, D.Plateaus) && Same(S.Water, D.Water) && Same(S.Erosion, D.Erosion)
			&& Same(S.Resources, D.Resources) && FPackageName::DoesPackageExist(E.Map))
		{
			return E.Map;
		}
	}
	return FString();
}

FString AWorldForgeGameMode::PreviewMapOf(const FWorldGenSettings& S)
{
	const FString Example = ExampleMapOf(S);
	if (!Example.IsEmpty())
	{
		return Example;
	}
	// os controles em texto fixo (3 casas: o arrasto do controle não muda a chave à toa)
	const FString Key = FString::Printf(TEXT("%s|%d|%d|%.0f|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%d|"), PipelineVersion, S.Seed, S.Players, S.MapSize,
		int32(S.Biome), S.Relief, S.Plateaus, S.Water, S.Erosion, S.Forests, S.Resources, S.Frame) + WorldSettingsJson::VillageKey(S);
	FMD5 Md5;
	const FTCHARToUTF8 Utf8(*Key);
	Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	TArray<uint8> Script;
	if (FFileHelper::LoadFileToArray(Script, *(FPaths::ProjectDir() / TEXT("Tools/build_slice.py"))))
	{
		Md5.Update(Script.GetData(), Script.Num());
	}
	uint8 Digest[16];
	Md5.Final(Digest);
	return FString::Printf(TEXT("/Game/Maps/Worlds/W_%s"), *BytesToHex(Digest, 5).ToLower());
}

bool AWorldForgeGameMode::HasPreview(const FWorldGenSettings& S)
{
	return FPackageName::DoesPackageExist(PreviewMapOf(S));
}

const TArray<AWorldForgeGameMode::FSavedMap>& AWorldForgeGameMode::GetSavedMaps()
{
	if (bSavedLoaded)
	{
		return SavedMaps;
	}
	bSavedLoaded = true;
	ON_SCOPE_EXIT
	{
		// os mapas trazidos de fora (Tools/zh_import.py), no fim da lista
		for (const WorldImport::FEntry& E : WorldImport::List())
		{
			FSavedMap& M = SavedMaps.AddDefaulted_GetRef();
			M.Name = E.Title + TEXT(" (Zero Hour)");
			M.ImportFolder = E.Folder;
		}
	};
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectSavedDir() / TEXT("WorldForge/maps.json");
	if (FFileHelper::LoadFileToString(Text, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
	{
		for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("maps")))
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (V->TryGetObject(O))
			{
				FSavedMap& M = SavedMaps.Add_GetRef({ (*O)->GetStringField(TEXT("name")), SettingsFrom(**O) });
				(*O)->TryGetStringField(TEXT("edits"), M.EditsFile);
				(*O)->TryGetStringField(TEXT("import"), M.ImportFolder);
			}
		}
		return SavedMaps;
	}
	// a 1ª vez: os 3 mapas das fatias de exemplo (as mesmas sementes e controles)
	auto Example = [this](const TCHAR* Name, int32 Seed, EWorldBiome Biome, float Forests)
	{
		FSavedMap M;
		M.Name = Name;
		M.Settings.Seed = Seed;
		M.Settings.Biome = Biome;
		M.Settings.Forests = Forests;
		SavedMaps.Add(M);
	};
	Example(TEXT("Deserto com estrada (exemplo)"), 3, EWorldBiome::Desert, 0.5f);
	Example(TEXT("Neve (exemplo)"), 5, EWorldBiome::Winter, 0.7f);
	Example(TEXT("Floresta (exemplo)"), 42, EWorldBiome::Temperate, 0.85f);
	return SavedMaps;
}

void AWorldForgeGameMode::AddSavedMap()
{
	GetSavedMaps();
	static const TCHAR* Biomes[] = { TEXT("Floresta"), TEXT("Deserto"), TEXT("Neve") };
	// (antes dos importados da pasta, que ficam no fim)
	int32 At = SavedMaps.IndexOfByPredicate([](const FSavedMap& M) { return !M.ImportFolder.IsEmpty() && M.EditsFile.IsEmpty() && M.Name.EndsWith(TEXT("(Zero Hour)")); });
	if (At == INDEX_NONE) At = SavedMaps.Num();
	FSavedMap& New = SavedMaps.InsertDefaulted_GetRef(At);
	New.Name = FString::Printf(TEXT("%s, semente %d"), Biomes[FMath::Clamp(int32(Settings.Biome), 0, 2)], Settings.Seed);
	New.Settings = Settings;
	if (ImportedMap.IsValid())
	{
		New.Name = ImportTitle + TEXT(" (Zero Hour)");
		New.ImportFolder = ImportFolder;
	}
	// as edições à mão vão junto, num arquivo dele (o mapa é semente + controles + edições)
	if (Edits.IsValid() && !Edits->IsEmpty())
	{
		New.EditsFile = FString::Printf(TEXT("edits/%s_%s.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *FString::FromInt(Settings.Seed));
		FFileHelper::SaveStringToFile(Edits->ToJson(), *(FPaths::ProjectSavedDir() / TEXT("WorldForge") / New.EditsFile), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		New.Name += FString::Printf(TEXT(" · %d edições"), EditCount());
	}
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FSavedMap& M : SavedMaps)
	{
		if (!M.ImportFolder.IsEmpty() && M.EditsFile.IsEmpty()) continue;   // o importado puro vem da pasta
		TSharedRef<FJsonObject> J = SettingsJson(M.Settings);
		J->SetStringField(TEXT("name"), M.Name);
		if (!M.EditsFile.IsEmpty()) J->SetStringField(TEXT("edits"), M.EditsFile);
		if (!M.ImportFolder.IsEmpty()) J->SetStringField(TEXT("import"), M.ImportFolder);
		Arr.Add(MakeShared<FJsonValueObject>(J));
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("maps"), Arr);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("WorldForge/maps.json")));
}

bool AWorldForgeGameMode::OpenImported(const FString& Folder, const FString& EditsFile)
{
	TSharedPtr<FWorldEdits> E;
	FString Note;
	TSharedPtr<FWorldMap> M = WorldImport::Load(Folder, E, Note);
	if (!M.IsValid())
	{
		return false;
	}
	ImportedMap = M;
	ImportedSettings = M->Settings;
	ImportFolder = Folder;
	ImportNote = Note;
	ImportTitle.Reset();
	for (const WorldImport::FEntry& Entry : WorldImport::List())
	{
		if (FPaths::IsSamePath(Entry.Folder, Folder)) ImportTitle = Entry.Title;
	}
	if (ImportTitle.IsEmpty()) ImportTitle = FPaths::GetCleanFilename(Folder);
	// as estradas e o concreto do mapa entram como edições à mão (dá pra mexer);
	// guardado com edições: as dele
	Edits = E.IsValid() ? E : MakeShared<FWorldEdits>();
	FString Text;
	if (!EditsFile.IsEmpty() && FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("WorldForge") / EditsFile)))
	{
		Edits->FromJson(Text);
	}
	UndoStack.Reset();
	RoadDeselect();
	RoadDraft.Reset();
	Settings = ImportedSettings;
	Generate(Settings);
	return true;
}

void AWorldForgeGameMode::OpenSavedMap(int32 Index)
{
	GetSavedMaps();
	if (!SavedMaps.IsValidIndex(Index))
	{
		return;
	}
	const FSavedMap& M = SavedMaps[Index];
	if (!M.ImportFolder.IsEmpty())
	{
		OpenImported(M.ImportFolder, M.EditsFile);
		return;
	}
	TSharedPtr<FWorldEdits> E = MakeShared<FWorldEdits>();
	FString Text;
	if (!M.EditsFile.IsEmpty() && FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("WorldForge") / M.EditsFile)))
	{
		E->FromJson(Text);
	}
	// as edições do guardado (e não as do mundo de agora): troca antes de gerar,
	// com os controles já iguais (o Generate não zera)
	Edits = E;
	UndoStack.Reset();
	RoadDeselect();
	RoadDraft.Reset();
	Settings = M.Settings;
	Generate(M.Settings);
}

bool AWorldForgeGameMode::StartRealQuality()
{
	if (RealState == 1)
	{
		return false;
	}
	RealMap = PreviewMapOf(Settings);
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldForge"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	// as edições à mão vão num arquivo ao lado; a prévia guardada ganha a marca
	// delas no nome (o mesmo mapa com edições diferentes é outra prévia)
	FString EditsPath;
	uint32 EditsHash = 0;
	if (Edits.IsValid() && !Edits->IsEmpty())
	{
		const FString EditsText = Edits->ToJson();
		EditsHash = FCrc::StrCrc32(*EditsText);
		EditsPath = Dir / TEXT("world_edits.json");
		FFileHelper::SaveStringToFile(EditsText, *EditsPath);
		RealMap += FString::Printf(TEXT("_e%08x"), EditsHash);
	}
	const FString Json = Dir / TEXT("world_settings.json");
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("seed"), Settings.Seed);
	J->SetNumberField(TEXT("players"), Settings.Players);
	J->SetNumberField(TEXT("size"), Settings.MapSize);
	J->SetNumberField(TEXT("biome"), int32(Settings.Biome));
	J->SetNumberField(TEXT("relief"), Settings.Relief);
	J->SetNumberField(TEXT("plateaus"), Settings.Plateaus);
	J->SetNumberField(TEXT("water"), Settings.Water);
	J->SetNumberField(TEXT("erosion"), Settings.Erosion);
	J->SetNumberField(TEXT("forests"), Settings.Forests);
	J->SetNumberField(TEXT("resources"), Settings.Resources);
	J->SetNumberField(TEXT("frame"), Settings.Frame);
	WorldSettingsJson::WriteVillage(*J, Settings);
	J->SetStringField(TEXT("map"), RealMap);
	if (!EditsPath.IsEmpty())
	{
		J->SetStringField(TEXT("edits"), EditsPath);
	}
	if (ImportedMap.IsValid())
	{
		J->SetStringField(TEXT("imported"), ImportFolder);   // o mapa trazido de fora no lugar do gerador
	}
	FString Text;
	FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text));
	if (!FFileHelper::SaveStringToFile(Text, *Json))
	{
		RealState = -1;
		return false;
	}
	// já montado com estes controles, estas edições e este acabamento: abre na hora
	if (EditsHash == 0 ? HasPreview(Settings) : FPackageName::DoesPackageExist(RealMap))
	{
		RealStart = RealEnd = FPlatformTime::Seconds();
		RealState = 2;
		UE_LOG(LogTemp, Display, TEXT("FORGE real quality: cached %s"), *RealMap);
		UGameplayStatics::OpenLevel(this, FName(*RealMap), true, RealOptions());
		return true;
	}
	// o editor sem janela (o mesmo binário do app) monta o nível
	const FString Exe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe"));
	const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/build_slice.py"));
	const FString Log = Dir / TEXT("world_build.log");
	FPlatformMisc::SetEnvironmentVar(TEXT("SLICE"), TEXT("world"));
	FPlatformMisc::SetEnvironmentVar(TEXT("WORLD_SETTINGS"), *Json);
	const FString Args = FString::Printf(TEXT("\"%s\" -run=pythonscript -script=\"%s\" -nullrhi -unattended -nosplash -abslog=\"%s\""), *Project, *Script, *Log);
	RealProc = FPlatformProcess::CreateProc(*Exe, *Args, true, true, true, nullptr, -1, nullptr, nullptr);
	if (!RealProc.IsValid())
	{
		RealState = -1;
		return false;
	}
	RealStart = FPlatformTime::Seconds();
	RealState = 1;
	UE_LOG(LogTemp, Display, TEXT("FORGE real quality: building (%s)"), *Log);
	return true;
}

int32 AWorldForgeGameMode::GetRealQualityState(double& OutSeconds) const
{
	OutSeconds = (RealState == 1 ? FPlatformTime::Seconds() : RealEnd) - RealStart;
	return RealState;
}

void AWorldForgeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AutoRealAt > 0.0 && FPlatformTime::Seconds() > AutoRealAt)
	{
		AutoRealAt = 0.0;
		StartRealQuality();
	}
	if (RealState == 1 && !FPlatformProcess::IsProcRunning(RealProc))
	{
		int32 Code = -1;
		FPlatformProcess::GetProcReturnCode(RealProc, &Code);
		FPlatformProcess::CloseProc(RealProc);
		RealEnd = FPlatformTime::Seconds();
		RealState = Code == 0 ? 2 : -1;
		UE_LOG(LogTemp, Display, TEXT("FORGE real quality: done, code %d, %.0f s"), Code, RealEnd - RealStart);
		if (RealState == 2)
		{
			UGameplayStatics::OpenLevel(this, FName(*RealMap), true, RealOptions());
		}
	}
	if (!CaptureFolder.IsEmpty() && Shown == Pending && Shown > 0)
	{
		FrameSum += DeltaSeconds;
		++FrameCount;
		CaptureWait -= DeltaSeconds;
		if (UITest.IsValid())
		{
			// teste da interface no lugar das vistas fixas
			if (CaptureWait <= 0.0 && UITest->Tick(DeltaSeconds))
			{
				UITest.Reset();
				CaptureFolder.Empty();
				FGenericPlatformMisc::RequestExit(false);
			}
			return;
		}
		if (CaptureWait <= 0.0)
		{
			CaptureStep();
		}
	}
}

void AWorldForgeGameMode::CaptureStep()
{
	// vistas fixas por semente: o mapa todo (com o painel), a base do jogador 1,
	// o meio do mapa e uma vista baixa de lado. Cada vista em duas fases: põe
	// a câmera e espera assentar; depois fotografa (a foto sai no fim do quadro,
	// então a câmera não pode mudar no mesmo quadro)
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AForgeCameraPawn* Pawn = PC ? PC->GetPawn<AForgeCameraPawn>() : nullptr;
	const FWorldMap* M = Terrain ? Terrain->GetMap() : nullptr;
	if (!Pawn || !M)
	{
		return;
	}
	Pawn->EdgePan = false;
	const int32 Seed = M->Settings.Seed;
	const FVector2f Start = M->Starts.Num() ? M->Starts[0].Pos : FVector2f::ZeroVector;
	const FString Prefix = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("seed%d"), Seed));
	static const TCHAR* MapNames[] = { TEXT("overview"), TEXT("base"), TEXT("center"), TEXT("low"), TEXT("horizon"), TEXT("pass"), TEXT("areas") };
	static const TCHAR* VillageNames[] = { TEXT("overview"), TEXT("village_top"), TEXT("village_game"), TEXT("village_game_north"), TEXT("village_close"), TEXT("village_low") };
	// os monólitos: o miolo de cima, a câmera do jogo no marco e num pilar do anel, de lado
	static const TCHAR* MonolithNames[] = { TEXT("overview"), TEXT("mono_top"), TEXT("mono_game"), TEXT("mono_ring"), TEXT("mono_side"), TEXT("mono_far") };
	FString Shots;
	FParse::Value(FCommandLine::Get(), TEXT("ForgeShots="), Shots);
	const bool bVillageShots = Shots == TEXT("village");
	const bool bMonoShots = Shots == TEXT("monoliths");
	const TCHAR* const* Names = bVillageShots ? VillageNames : (bMonoShots ? MonolithNames : MapNames);
	const int32 Views = bVillageShots ? UE_ARRAY_COUNT(VillageNames) : (bMonoShots ? UE_ARRAY_COUNT(MonolithNames) : UE_ARRAY_COUNT(MapNames));
	const int32 View = CaptureShot / 2;
	if (View >= Views)
	{
		Terrain->SetOverlay(0);
		// próxima semente ou fim
		if (++CaptureSeedIndex < CaptureSeeds.Num())
		{
			FWorldGenSettings S = Settings;
			S.Seed = CaptureSeeds[CaptureSeedIndex];
			CaptureShot = 0;
			CaptureWait = 1e9;
			Generate(S);
			return;
		}
		const double Mean = FrameSum / FMath::Max(FrameCount, 1);
		UE_LOG(LogTemp, Display, TEXT("FORGE mean frame %.1f ms (%.0f fps) over %d frames"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), FrameCount);
		UE_LOG(LogTemp, Display, TEXT("FORGE capture done"));
		CaptureFolder.Empty();
		FGenericPlatformMisc::RequestExit(false);
		return;
	}
	if (CaptureShot % 2 == 0)
	{
		// -ForgeDebugToggle=1,2: liga/desliga o F2 no começo dessas vistas (teste)
		FString Toggles;
		if (FParse::Value(FCommandLine::Get(), TEXT("ForgeDebugToggle="), Toggles, false)
			&& (TEXT(",") + Toggles + TEXT(",")).Contains(FString::Printf(TEXT(",%d,"), View)))
		{
			if (AForgePlayerController* FPC = Cast<AForgePlayerController>(PC))
			{
				FPC->ToggleDebugView();
			}
		}
		// a vila: o meio das peças da 1ª vila; a câmera do jogo (37,5°, 61 m de
		// altura = 100 m de distância, nasce girada 45°: Game RTSCameraPawn)
		FVector2f Village = FVector2f::ZeroVector;
		if (bVillageShots && Terrain->GetLive() && Terrain->GetLive()->Detail->GVillage.Num())
		{
			int32 N = 0;
			for (const FWorldDetail::FVillagePiece& V : Terrain->GetLive()->Detail->GVillage)
			{
				if (V.Village == 0) { Village += V.Center; ++N; }
			}
			Village /= float(FMath::Max(N, 1));
		}
		if (bVillageShots && View > 0)
		{
			switch (View)
			{
			case 1: Pawn->SetView(Village, 0.0f, 85.0f, 300.0f); break;
			case 2: Pawn->SetView(Village, 45.0f, 37.5f, 100.0f); break;
			case 3: Pawn->SetView(Village + FVector2f(-25.0f, 55.0f), 45.0f, 37.5f, 100.0f); break;
			case 4: Pawn->SetView(Village + FVector2f(-20.0f, 40.0f), 45.0f, 37.5f, 55.0f); break;
			default: Pawn->SetView(Village, 225.0f, 22.0f, 160.0f); break;
			}
			CaptureWait = 1.5;
			++CaptureShot;
			return;
		}
		if (bMonoShots && View > 0)
		{
			// a direção da base 0 pro centro (o anel fica dos lados dessa rota)
			const FVector2f Dir = Start.GetSafeNormal();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Dir.Y, -Dir.X));
			switch (View)
			{
			case 1: Pawn->SetView(FVector2f::ZeroVector, 0.0f, 85.0f, 520.0f); break;
			case 2: Pawn->SetView(FVector2f::ZeroVector, 45.0f, 37.5f, 160.0f); break;
			case 3: Pawn->SetView(Start * 0.45f, 45.0f, 37.5f, 160.0f); break;
			case 4: Pawn->SetView(Start * 0.2f, Yaw, 14.0f, 260.0f); break;
			default: Pawn->SetView(Start * 0.6f, Yaw, 30.0f, 520.0f); break;
			}
			CaptureWait = 1.5;
			++CaptureShot;
			return;
		}
		switch (View)
		{
		case 0:
			Pawn->FrameWholeMap(true);
			WorldForgeExport::Save(*M, Terrain->GetMinimapPixels(), Prefix + TEXT("_data"));
			break;
		case 1: Pawn->SetView(Start * 0.92f, 0.0f, 55.0f, 170.0f); break;
		case 2: Pawn->SetView(FVector2f::ZeroVector, 0.0f, 55.0f, 330.0f); break;
		case 3: Pawn->SetView(Start * 0.45f, 35.0f, 24.0f, 420.0f); break;
		case 4: Pawn->SetView(Start * 0.3f, 60.0f, 7.0f, 260.0f); break;   // céu e horizonte
		default:
			// camadas de depuração: passabilidade e territórios, no mapa todo
			Terrain->SetOverlay(View == 5 ? 1 : 2);
			Pawn->FrameWholeMap(true);
			break;
		}
		CaptureWait = 1.5;
	}
	else
	{
		const FString File = FString::Printf(TEXT("%s_%d_%s.png"), *Prefix, View, Names[View]);
		// janela minimizada (a tela vira 8×8): a Unreal quebra ao copiar a foto (D3D12Texture.cpp)
		FVector2D ShotSize(0, 0);
		if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ShotSize);
		if (ShotSize.X >= 64 && ShotSize.Y >= 64)
			FScreenshotRequest::RequestScreenshot(File, View == 0, false);
		{
			FTextureMemoryStats Mem;
			RHIGetTextureMemoryStats(Mem);
			UE_LOG(LogTemp, Display, TEXT("FORGE vram %s: %.0f/%.0f MB, estourado %.0f MB"), Names[View],
				(Mem.StreamingMemorySize + Mem.NonStreamingMemorySize) / 1048576.0, Mem.TotalGraphicsMemory / 1048576.0, GDemotedLocalMemorySize / 1048576.0);
			if (View == 0 && FParse::Param(FCommandLine::Get(), TEXT("ForgeDumpMemory")) && GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpMemory"));
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpResourceMemory 40"));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE shot %s"), *File);
		CaptureWait = 0.3;
	}
	++CaptureShot;
}


bool AWorldForgeGameMode::EditDeleteAt(FVector2f P)
{
	if (!Terrain || !Edits.IsValid())
	{
		return false;
	}
	AWorldForgeTerrain::FRemoved R;
	if (!Terrain->RemoveNearest(P, 2.0f, R))
	{
		return false;
	}
	Edits->Add(R.Kind, R.Origin);
	TSharedPtr<FEditUndo> U = MakeShared<FEditUndo>();
	U->Removed = R;
	U->Stamp = Shown;
	UndoStack.Add(U);
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: apagou %s em (%.1f, %.1f); %d edições"), *R.Kind, R.Origin.X, R.Origin.Y, Edits->Deleted.Num());
	return true;
}

bool AWorldForgeGameMode::EditUndo()
{
	if (UndoStack.Num() == 0 || !Terrain || !Edits.IsValid())
	{
		return false;
	}
	if (IsGenerating())
	{
		return false;   // o mundo está sendo refeito com as edições de agora
	}
	const TSharedPtr<FEditUndo> U = UndoStack.Pop();
	if (U->bRoads)
	{
		Edits->Roads = U->PrevRoads;
		Edits->RemovedRoads = U->PrevRemoved;
		RoadDeselect();
		Generate(Settings);
		return true;
	}
	if (U->bMove)
	{
		Edits->RemoveMoved(U->Move.Kind, U->Move.From);
		if (U->PrevMove.IsSet()) Edits->SetMoved(U->PrevMove.GetValue());
		if (U->Stamp != Shown)
		{
			Generate(Settings);
			return true;
		}
		if (FWorldLive* L = Terrain->GetLive())
		{
			L->Edits->RemoveMoved(U->Move.Kind, U->Move.From);
			if (U->PrevMove.IsSet()) L->Edits->SetMoved(U->PrevMove.GetValue());
		}
		Terrain->SetPieceTransform(U->Removed.Component, U->Move.From, U->Removed.Xform);
		return true;
	}
	if (U->Stamp != Shown)
	{
		// o mundo foi refeito depois desta edição (uma estrada, por exemplo): as
		// peças e a malha de agora são outras; volta nos dados e refaz
		if (U->bHeight)
		{
			for (const TPair<FIntPoint, float>& K : U->Applied)
			{
				float& H = Edits->Height.FindOrAdd(K.Key);
				H -= K.Value;
				if (FMath::Abs(H) < 1e-4f) Edits->Height.Remove(K.Key);
			}
		}
		else if (U->bPaint)
		{
			Edits->Unpaint(U->Prev);
		}
		else
		{
			Edits->RemoveLast();
		}
		Generate(Settings);
		return true;
	}
	if (U->bHeight)
	{
		TMap<FIntPoint, float> Back;
		for (const TPair<FIntPoint, float>& K : U->Applied) Back.Add(K.Key, -K.Value);
		ApplyHeightDeltas(Back);
		RefreshRegion(U->Min, U->Max);
		return true;
	}
	if (U->bPaint)
	{
		Edits->Unpaint(U->Prev);
		if (FWorldLive* L = Terrain->GetLive())
		{
			L->Edits->Unpaint(U->Prev);
		}
		Terrain->RepaintRegion(U->Min, U->Max);
		RefreshRegion(U->Min, U->Max);
		return true;
	}
	Terrain->Restore(U->Removed);
	Edits->RemoveLast();
	return true;
}

void AWorldForgeGameMode::ApplyHeightDeltas(const TMap<FIntPoint, float>& Deltas)
{
	FWorldLive* L = Terrain ? Terrain->GetLive() : nullptr;
	if (!L || !L->Display.IsValid() || Deltas.Num() == 0)
	{
		return;
	}
	FWorldMap& D = *L->Detail->GMap;       // o relevo do gerador (com as edições)
	FWorldMap& Disp = *L->Display;         // a altura final mostrada
	int32 I0 = D.N, J0 = D.N, I1 = -1, J1 = -1;
	for (const TPair<FIntPoint, float>& K : Deltas)
	{
		const FIntPoint V = K.Key;
		if (V.X < 0 || V.Y < 0 || V.X >= D.N || V.Y >= D.N) continue;
		D.Height[D.Index(V.X, V.Y)] += K.Value;
		Edits->Height.FindOrAdd(V) += K.Value;
		L->Edits->Height.FindOrAdd(V) += K.Value;
		I0 = FMath::Min(I0, V.X); J0 = FMath::Min(J0, V.Y); I1 = FMath::Max(I1, V.X); J1 = FMath::Max(J1, V.Y);
	}
	if (I1 < 0)
	{
		return;
	}
	// a altura final nos vértices tocados (+1 de margem: a normal usa os vizinhos)
	I0 = FMath::Max(I0 - 1, 0); J0 = FMath::Max(J0 - 1, 0); I1 = FMath::Min(I1 + 1, D.N - 1); J1 = FMath::Min(J1 + 1, D.N - 1);
	TMap<FIntPoint, float> Moved;
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			const int32 V = Disp.Index(I, J);
			const float Old = Disp.Height[V];
			Disp.Height[V] = L->Detail->SliceHeight(D, Disp.CellPos(I, J));
			Moved.Add(FIntPoint(I, J), Disp.Height[V] - Old);
		}
	}
	Terrain->UpdateGroundRegion(I0, J0, I1, J1);
	const FVector2f MinM = Disp.CellPos(I0, J0), MaxM = Disp.CellPos(I1, J1);
	Terrain->LiftPieces(MinM, MaxM, [&Disp, &Moved](FVector2f Q)
	{
		const FVector2f G = Disp.ToGrid(Q);
		const int32 X = FMath::FloorToInt(G.X), Y = FMath::FloorToInt(G.Y);
		const float FX = G.X - X, FY = G.Y - Y;
		auto At = [&Moved](int32 A, int32 B) { const float* V = Moved.Find(FIntPoint(A, B)); return V ? *V : 0.0f; };
		return FMath::Lerp(FMath::Lerp(At(X, Y), At(X + 1, Y), FX), FMath::Lerp(At(X, Y + 1), At(X + 1, Y + 1), FX), FY);
	});
	Terrain->RepaintRegion(MinM - FVector2f(2.0f), MaxM + FVector2f(2.0f));   // encosta nova: rocha onde ficou íngreme
}

void AWorldForgeGameMode::EditHeight(FVector2f P, bool bStart, float DeltaSeconds)
{
	FWorldLive* L = Terrain ? Terrain->GetLive() : nullptr;
	if (!L || !Edits.IsValid())
	{
		return;
	}
	const FWorldMap& D = *L->Detail->GMap;
	if (bStart || UndoStack.Num() == 0 || !UndoStack.Last()->bHeight)
	{
		TSharedPtr<FEditUndo> U = MakeShared<FEditUndo>();
		U->bHeight = true;
		U->Stamp = Shown;
		U->FlatTarget = D.HeightAt(P);
		UndoStack.Add(U);
	}
	FEditUndo& U = *UndoStack.Last();
	// subir/baixar: ~1,5 m por segundo segurando no meio do pincel; aplainar:
	// puxa ~2× por segundo pra altura de onde o arrasto começou
	const float R = BrushRadius;
	U.Min = FVector2f(FMath::Min(U.Min.X, P.X - R - 2.0f), FMath::Min(U.Min.Y, P.Y - R - 2.0f));
	U.Max = FVector2f(FMath::Max(U.Max.X, P.X + R + 2.0f), FMath::Max(U.Max.Y, P.Y + R + 2.0f));
	const FVector2f G = D.ToGrid(P);
	const int32 Rc = FMath::CeilToInt(R / D.Cell) + 1;
	TMap<FIntPoint, float> Deltas;
	for (int32 J = FMath::FloorToInt(G.Y) - Rc; J <= FMath::CeilToInt(G.Y) + Rc; ++J)
	{
		for (int32 I = FMath::FloorToInt(G.X) - Rc; I <= FMath::CeilToInt(G.X) + Rc; ++I)
		{
			if (I < 0 || J < 0 || I >= D.N || J >= D.N) continue;
			const float Dist = FVector2f::Distance(D.CellPos(I, J), P);
			const float F = 1.0f - FMath::SmoothStep(0.45f * R, R, Dist);
			if (F <= 0.0f) continue;
			float Delta;
			if (HeightMode == 2) Delta = (U.FlatTarget - D.Height[D.Index(I, J)]) * FMath::Min(1.0f, 2.0f * DeltaSeconds) * F;
			else Delta = (HeightMode == 0 ? 1.5f : -1.5f) * DeltaSeconds * F;
			if (FMath::Abs(Delta) < 1e-4f) continue;
			Deltas.Add(FIntPoint(I, J), Delta);
			U.Applied.FindOrAdd(FIntPoint(I, J)) += Delta;
		}
	}
	ApplyHeightDeltas(Deltas);
}

void AWorldForgeGameMode::EditPaint(FVector2f P, bool bStart)
{
	if (!Terrain || !Edits.IsValid() || !Terrain->GetLive())
	{
		return;
	}
	if (bStart || UndoStack.Num() == 0 || !UndoStack.Last()->bPaint)
	{
		TSharedPtr<FEditUndo> U = MakeShared<FEditUndo>();
		U->bPaint = true;
		U->Stamp = Shown;
		UndoStack.Add(U);
	}
	FEditUndo& U = *UndoStack.Last();
	TMap<FIntPoint, FWorldEdits::FPaint> Ignored;
	Edits->PaintDisc(P, BrushRadius, BrushLayer, 1.0f, U.Prev);
	Terrain->GetLive()->Edits->PaintDisc(P, BrushRadius, BrushLayer, 1.0f, Ignored);
	const FVector2f Ext(BrushRadius + 1.0f, BrushRadius + 1.0f);
	U.Min = FVector2f(FMath::Min(U.Min.X, P.X - Ext.X), FMath::Min(U.Min.Y, P.Y - Ext.Y));
	U.Max = FVector2f(FMath::Max(U.Max.X, P.X + Ext.X), FMath::Max(U.Max.Y, P.Y + Ext.Y));
	Terrain->RepaintRegion(P - Ext, P + Ext);
}

int32 AWorldForgeGameMode::EditCount() const
{
	return Edits.IsValid() ? Edits->Deleted.Num() + (Edits->Paint.Num() ? 1 : 0) + (Edits->Height.Num() ? 1 : 0)
		+ Edits->Roads.Num() + Edits->RemovedRoads.Num() + Edits->Moved.Num() : 0;
}

void AWorldForgeGameMode::EditMovePiece(const FMovePick& Pick, FVector2f To, float YawDeg)
{
	if (!Terrain || !Edits.IsValid() || IsGenerating())
	{
		return;
	}
	const AWorldForgeTerrain::FPicked& Pk = Pick.Piece;
	TSharedPtr<FEditUndo> U = MakeShared<FEditUndo>();
	U->bMove = true;
	U->Stamp = Shown;
	U->Removed.Component = Pk.Component;
	U->Removed.Xform = Pk.Start;
	FWorldEdits::FMoved M;
	M.Kind = Pk.Kind;
	M.From = Pk.Origin;
	M.To = To;
	M.Yaw = YawDeg;
	if (const FWorldEdits::FMoved* Prev = Edits->FindMoved(Pk.Kind, Pk.Origin))
	{
		U->PrevMove = *Prev;
		M.Yaw += Prev->Yaw;
	}
	U->Move = M;
	Edits->SetMoved(M);
	if (FWorldLive* L = Terrain->GetLive())
	{
		L->Edits->SetMoved(M);
	}
	UndoStack.Add(U);
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: moveu %s de (%.1f, %.1f) para (%.1f, %.1f), giro %.0f°"), *M.Kind, Pk.Pivot.X, Pk.Pivot.Y, To.X, To.Y, M.Yaw);
}

void AWorldForgeGameMode::EditStrokeEnd()
{
	if (UndoStack.Num() && (UndoStack.Last()->bPaint || UndoStack.Last()->bHeight) && UndoStack.Last()->Stamp == Shown)
	{
		const FEditUndo& U = *UndoStack.Last();
		RefreshRegion(U.Min, U.Max);
	}
}

void AWorldForgeGameMode::RefreshRegion(FVector2f MinM, FVector2f MaxM)
{
	FWorldLive* L = Terrain ? Terrain->GetLive() : nullptr;
	if (!L || !L->Display.IsValid() || IsGenerating() || MinM.X > MaxM.X)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	// a altura final do pedaço (a pintura muda a ondulação da areia; a altura, o resto)
	FWorldMap& Disp = *L->Display;
	const FWorldMap& D = *L->Detail->GMap;
	const FVector2f G0 = Disp.ToGrid(MinM), G1 = Disp.ToGrid(MaxM);
	const int32 I0 = FMath::Clamp(FMath::FloorToInt(G0.X) - 1, 0, Disp.N - 1), I1 = FMath::Clamp(FMath::CeilToInt(G1.X) + 1, 0, Disp.N - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt(G0.Y) - 1, 0, Disp.N - 1), J1 = FMath::Clamp(FMath::CeilToInt(G1.Y) + 1, 0, Disp.N - 1);
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			Disp.Height[Disp.Index(I, J)] = L->Detail->SliceHeight(D, Disp.CellPos(I, J));
		}
	}
	Terrain->UpdateGroundRegion(I0, J0, I1, J1);
	// as peças: o sorteio do pedaço com o chão de agora (o mesmo do mapa inteiro)
	const FBox2f Region(MinM, MaxM);
	TArray<FWorldLive::FKind> Fresh;
	L->ScatterRegion(Region, Fresh);
	Terrain->ReplaceLiveRegion(Region, Fresh);
	++RegionRefreshes;
	UE_LOG(LogTemp, Display, TEXT("FORGE pedaço refeito (%.0f×%.0f m) em %.0f ms"), MaxM.X - MinM.X, MaxM.Y - MinM.Y, 1000.0 * (FPlatformTime::Seconds() - T0));
}

// ---- estradas à mão ----------------------------------------------------------

bool AWorldForgeGameMode::BiomeHasPavedRoads() const
{
	float Half;
	bool bPaved;
	FWorldDetail::MainRoadOf(Settings.Biome, Half, bPaved);
	return bPaved;
}

int32 AWorldForgeGameMode::FindSelectedRoad() const
{
	const FWorldLive* L = Terrain ? Terrain->GetLive() : nullptr;
	if (!bRoadSelected || !L)
	{
		return INDEX_NONE;
	}
	const FWorldDetail& D = *L->Detail;
	if (RoadSelUser != INDEX_NONE)
	{
		return D.UserRoadIndex.IsValidIndex(RoadSelUser) ? D.UserRoadIndex[RoadSelUser] : INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	float BestD = 3.0f;
	for (int32 R = 0; R < D.NumGenRoads; ++R)
	{
		int32 Idx;
		const float Dist = FWorldDetail::NearestOn(D.GRoads[R].P, RoadSelProbe, Idx);
		if (Dist < BestD) { BestD = Dist; Best = R; }
	}
	return Best;
}

const TArray<FVector2f>* AWorldForgeGameMode::SelectedRoadLine() const
{
	if (IsGenerating())
	{
		return nullptr;
	}
	const int32 G = FindSelectedRoad();
	return G != INDEX_NONE ? &Terrain->GetLive()->Detail->GRoads[G].P : nullptr;
}

bool AWorldForgeGameMode::IsSelectedRoadPaved() const
{
	if (!bRoadSelected)
	{
		return RoadPaved;
	}
	if (RoadSelUser != INDEX_NONE && Edits->Roads.IsValidIndex(RoadSelUser))
	{
		return Edits->Roads[RoadSelUser].bPaved && BiomeHasPavedRoads();
	}
	const int32 G = FindSelectedRoad();
	return G != INDEX_NONE ? Terrain->GetLive()->Detail->GRoads[G].bPaved : RoadPaved;
}

bool AWorldForgeGameMode::RoadSelectAt(FVector2f P)
{
	FWorldLive* L = Terrain ? Terrain->GetLive() : nullptr;
	if (!L || !Edits.IsValid())
	{
		return false;
	}
	const FWorldDetail& D = *L->Detail;
	int32 Best = INDEX_NONE, BestIdx = INDEX_NONE;
	float BestD = 3.0f;   // até 3 m fora da beira
	for (int32 R = 0; R < D.GRoads.Num(); ++R)
	{
		int32 Idx;
		const float Dist = FWorldDetail::NearestOn(D.GRoads[R].P, P, Idx) - D.GRoads[R].Half;
		if (Dist < BestD) { BestD = Dist; Best = R; BestIdx = Idx; }
	}
	if (Best == INDEX_NONE)
	{
		return false;
	}
	bRoadSelected = true;
	const FWorldDetail::FRoad& Road = D.GRoads[Best];
	const int32 U = D.UserRoadIndex.Find(Best);
	if (U != INDEX_NONE && Edits->Roads.IsValidIndex(U))
	{
		RoadSelUser = U;
		RoadHandles = Edits->Roads[U].Points;
	}
	else
	{
		// a do gerador: ela é achada de novo depois de refazer o mundo por um ponto
		// dela longe das outras (no cruzamento o ponto acharia as duas)
		RoadSelUser = INDEX_NONE;
		RoadSelProbe = Road.P[BestIdx];
		for (int32 Off = 0; Off < Road.P.Num(); Off += 3)
		{
			bool bFound = false;
			for (const int32 K : { BestIdx + Off, BestIdx - Off })
			{
				if (!Road.P.IsValidIndex(K)) continue;
				float Other = 1e9f;
				for (int32 R = 0; R < D.NumGenRoads && Other > 8.0f; ++R)
				{
					if (R == Best) continue;
					int32 Idx;
					Other = FMath::Min(Other, FWorldDetail::NearestOn(D.GRoads[R].P, Road.P[K], Idx));
				}
				if (Other > 8.0f) { RoadSelProbe = Road.P[K]; bFound = true; break; }
			}
			if (bFound) break;
		}
		RoadHandles = FWorldDetail::Decimate(Road.P, 1.5f);
	}
	RoadPaved = Road.bPaved;
	return true;
}

void AWorldForgeGameMode::RoadDeselect()
{
	bRoadSelected = false;
	RoadSelUser = INDEX_NONE;
	RoadHandles.Reset();
}

void AWorldForgeGameMode::RoadPushUndo()
{
	TSharedPtr<FEditUndo> U = MakeShared<FEditUndo>();
	U->bRoads = true;
	U->PrevRoads = Edits->Roads;
	U->PrevRemoved = Edits->RemovedRoads;
	U->Stamp = Shown;
	UndoStack.Add(U);
}

void AWorldForgeGameMode::RoadMakeUser()
{
	if (RoadSelUser != INDEX_NONE)
	{
		return;
	}
	const bool bWasPaved = IsSelectedRoadPaved();
	const int32 G = FindSelectedRoad();
	Edits->RemovedRoads.Add(RoadSelProbe);
	FWorldEdits::FRoadEdit& E = Edits->Roads.AddDefaulted_GetRef();
	E.Points = RoadHandles;
	E.bPaved = bWasPaved;
	if (G != INDEX_NONE)
	{
		// a ponta que entrava na base continua se desfazendo como antes
		const FWorldDetail::FRoad& Road = Terrain->GetLive()->Detail->GRoads[G];
		E.bFade[0] = Road.bOpen[0];
		E.bFade[1] = Road.bOpen[1];
	}
	RoadSelUser = Edits->Roads.Num() - 1;
}

void AWorldForgeGameMode::RoadFinishDraft()
{
	if (RoadDraft.Num() >= 2 && Edits.IsValid() && !IsGenerating())
	{
		RoadPushUndo();
		FWorldEdits::FRoadEdit& E = Edits->Roads.AddDefaulted_GetRef();
		E.Points = RoadDraft;
		E.bPaved = RoadPaved && BiomeHasPavedRoads();
		bRoadSelected = true;
		RoadSelUser = Edits->Roads.Num() - 1;
		RoadHandles = RoadDraft;
		UE_LOG(LogTemp, Display, TEXT("FORGE edição: estrada nova de %s com %d pontos"), E.bPaved ? TEXT("asfalto") : TEXT("terra"), E.Points.Num());
		Generate(Settings);
	}
	RoadDraft.Reset();
}

void AWorldForgeGameMode::RoadMoveHandle(int32 Handle, FVector2f To)
{
	if (!bRoadSelected || !RoadHandles.IsValidIndex(Handle) || IsGenerating())
	{
		return;
	}
	RoadPushUndo();
	RoadMakeUser();
	Edits->Roads[RoadSelUser].Points[Handle] = To;
	RoadHandles = Edits->Roads[RoadSelUser].Points;
	Generate(Settings);
}

void AWorldForgeGameMode::RoadSetPaved(bool bPaved)
{
	RoadPaved = bPaved && BiomeHasPavedRoads();
	if (!bRoadSelected || IsSelectedRoadPaved() == RoadPaved || IsGenerating())
	{
		return;
	}
	RoadPushUndo();
	RoadMakeUser();
	Edits->Roads[RoadSelUser].bPaved = RoadPaved;
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: estrada virou %s"), RoadPaved ? TEXT("asfalto") : TEXT("terra"));
	Generate(Settings);
}

int32 AWorldForgeGameMode::GetSelectedRoadStyle() const
{
	if (bRoadSelected && RoadSelUser != INDEX_NONE && Edits.IsValid() && Edits->Roads.IsValidIndex(RoadSelUser))
	{
		return Edits->Roads[RoadSelUser].Style;
	}
	return 0;
}

void AWorldForgeGameMode::RoadSetStyle(int32 Style)
{
	if (!bRoadSelected || IsGenerating() || !IsSelectedRoadPaved() || GetSelectedRoadStyle() == Style)
	{
		return;
	}
	RoadPushUndo();
	RoadMakeUser();
	Edits->Roads[RoadSelUser].Style = Style;
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: acabamento do asfalto %d"), Style);
	Generate(Settings);
}

int32 AWorldForgeGameMode::GetSelectedRoadDirt() const
{
	if (bRoadSelected && RoadSelUser != INDEX_NONE && Edits.IsValid() && Edits->Roads.IsValidIndex(RoadSelUser))
	{
		return Edits->Roads[RoadSelUser].Dirt;
	}
	return 0;
}

void AWorldForgeGameMode::RoadSetDirt(int32 Dirt)
{
	if (!bRoadSelected || IsGenerating() || !IsSelectedRoadPaved() || GetSelectedRoadDirt() == Dirt)
	{
		return;
	}
	RoadPushUndo();
	RoadMakeUser();
	Edits->Roads[RoadSelUser].Dirt = Dirt;
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: sujeira na beira %d"), Dirt);
	Generate(Settings);
}

void AWorldForgeGameMode::RoadDeleteSelected()
{
	if (!bRoadSelected || IsGenerating())
	{
		return;
	}
	RoadPushUndo();
	if (RoadSelUser != INDEX_NONE)
	{
		Edits->Roads.RemoveAt(RoadSelUser);
	}
	else
	{
		Edits->RemovedRoads.Add(RoadSelProbe);
	}
	RoadDeselect();
	UE_LOG(LogTemp, Display, TEXT("FORGE edição: estrada apagada"));
	Generate(Settings);
}
