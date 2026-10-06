#include "WorldForgeGameMode.h"

#include "Async/Async.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "Scalability.h"
#include "Misc/PackageName.h"
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

AWorldForgeGameMode::AWorldForgeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AForgeCameraPawn::StaticClass();
	PlayerControllerClass = AForgePlayerController::StaticClass();
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

void AWorldForgeGameMode::Generate(const FWorldGenSettings& NewSettings)
{
	Settings = NewSettings;
	const int32 Ticket = ++Pending;
	const FWorldGenSettings S = Settings;
	TWeakObjectPtr<AWorldForgeGameMode> Weak(this);
	// a geração é só dados (núcleo sem UObject): roda numa thread de trabalho
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [S, Ticket, Weak]()
	{
		TSharedPtr<FWorldMap> Map = FWorldGenerator::Generate(S);
		AsyncTask(ENamedThreads::GameThread, [Weak, Ticket, Map]()
		{
			if (AWorldForgeGameMode* GM = Weak.Get())
			{
				GM->OnGenerated(Ticket, Map);
			}
		});
	});
}

void AWorldForgeGameMode::OnGenerated(int32 Ticket, TSharedPtr<FWorldMap> Map)
{
	// um pedido mais novo já está a caminho: este fica de fora (o painel não pisca)
	if (Ticket < Pending || Ticket <= Shown || !Terrain)
	{
		return;
	}
	const float OldSize = Terrain->GetMap() ? Terrain->GetMap()->Settings.MapSize : -1.0f;
	Shown = Ticket;
	Terrain->ShowMap(Map);
	FString Times;
	for (const TPair<FString, double>& T : Map->Timings)
	{
		Times += FString::Printf(TEXT(" %s %.0f"), *T.Key, T.Value);
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE map seed %d players %d size %.0f: connected %d, rush %.0f m (%.2fx straight), ramps %d, open %.0f%%, props %d, resources %d · ms:%s"),
		Map->Settings.Seed, Map->Settings.Players, Map->Settings.MapSize, Map->bConnected ? 1 : 0, Map->RushMeters, Map->RushMeters / FMath::Max(Map->RushStraight, 1.0f), Map->RampsCarved,
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
	return WorldForgeExport::Save(M, Terrain->GetMinimapPixels(), Folder) ? FPaths::ConvertRelativePathToFull(Folder) : FString();
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
	const TCHAR* PipelineVersion = TEXT("2026-10-06c");

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
	const FString Key = FString::Printf(TEXT("%s|%d|%d|%.0f|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|"), PipelineVersion, S.Seed, S.Players, S.MapSize,
		int32(S.Biome), S.Relief, S.Plateaus, S.Water, S.Erosion, S.Forests, S.Resources);
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
				SavedMaps.Add({ (*O)->GetStringField(TEXT("name")), SettingsFrom(**O) });
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
	SavedMaps.Add({ FString::Printf(TEXT("%s, semente %d"), Biomes[FMath::Clamp(int32(Settings.Biome), 0, 2)], Settings.Seed), Settings });
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FSavedMap& M : SavedMaps)
	{
		TSharedRef<FJsonObject> J = SettingsJson(M.Settings);
		J->SetStringField(TEXT("name"), M.Name);
		Arr.Add(MakeShared<FJsonValueObject>(J));
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("maps"), Arr);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("WorldForge/maps.json")));
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
	J->SetStringField(TEXT("map"), RealMap);
	FString Text;
	FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text));
	if (!FFileHelper::SaveStringToFile(Text, *Json))
	{
		RealState = -1;
		return false;
	}
	// já montado com estes controles e este acabamento: abre na hora
	if (HasPreview(Settings))
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
	static const TCHAR* Names[] = { TEXT("overview"), TEXT("base"), TEXT("center"), TEXT("low"), TEXT("horizon"), TEXT("pass"), TEXT("areas") };
	constexpr int32 Views = UE_ARRAY_COUNT(Names);
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
