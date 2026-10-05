#include "WorldForgeGameMode.h"

#include "Async/Async.h"
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

	Terrain = GetWorld()->SpawnActor<AWorldForgeTerrain>();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			Pawn->Terrain = Terrain;
		}
	}

	if (FParse::Value(Cmd, TEXT("ForgeCapture="), CaptureFolder))
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

void AWorldForgeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
		FScreenshotRequest::RequestScreenshot(File, View == 0, false);
		UE_LOG(LogTemp, Display, TEXT("FORGE shot %s"), *File);
		CaptureWait = 0.3;
	}
	++CaptureShot;
}
