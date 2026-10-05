#include "ForgeSliceGameMode.h"

#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

namespace
{
	struct FSliceView
	{
		const TCHAR* Name;
		FVector2f Focus;   // m
		float Yaw, Pitch, Distance;
	};
	// vistas da prova: as distâncias de câmera de RTS e duas de perto
	const FSliceView Views[] = {
		{ TEXT("rts_high"), FVector2f(0, 0), 0.0f, 55.0f, 170.0f },
		{ TEXT("rts_mid"), FVector2f(10, -15), 20.0f, 50.0f, 85.0f },
		{ TEXT("rts_close"), FVector2f(-20, 10), 35.0f, 45.0f, 38.0f },
		{ TEXT("ground"), FVector2f(15, 25), 110.0f, 16.0f, 22.0f },
		{ TEXT("against_sun"), FVector2f(-10, -30), 250.0f, 28.0f, 55.0f },
	};
	constexpr int32 MotionFrames = 120;   // 4 s a 30 qps
}

AForgeSliceGameMode::AForgeSliceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AForgeCameraPawn::StaticClass();
	PlayerControllerClass = AForgePlayerController::StaticClass();
}

void AForgeSliceGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AForgeCameraPawn* Pawn = PC->GetPawn<AForgeCameraPawn>())
		{
			Pawn->MaxDistance = 400.0f;
			Pawn->SetView(FVector2f(0, 0), 0.0f, 50.0f, 110.0f);
		}
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeCapture="), CaptureFolder))
	{
		IFileManager::Get().MakeDirectory(*CaptureFolder, true);
	}
}

void AForgeSliceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CaptureFolder.IsEmpty())
	{
		return;
	}
	FrameSum += DeltaSeconds;
	++FrameCount;
	// shader ainda compilando (1ª abertura): a Unreal mostra o material
	// padrão quadriculado — foto só depois que terminar
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		Wait = FMath::Max(Wait, 1.0);
		if (FrameCount % 120 == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("FORGE waiting shaders: %d"), GShaderCompilingManager->GetNumRemainingJobs());
		}
		return;
	}
	Wait -= DeltaSeconds;
	if (Wait <= 0.0)
	{
		CaptureStep();
	}
}

void AForgeSliceGameMode::CaptureStep()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AForgeCameraPawn* Pawn = PC ? PC->GetPawn<AForgeCameraPawn>() : nullptr;
	if (!Pawn)
	{
		return;
	}
	Pawn->EdgePan = false;
	constexpr int32 NumViews = UE_ARRAY_COUNT(Views);
	const int32 View = Step / 2;
	if (View < NumViews)
	{
		// duas fases por vista: põe a câmera e espera assentar; depois fotografa
		if (Step % 2 == 0)
		{
			const FSliceView& V = Views[View];
			Pawn->SetView(V.Focus, V.Yaw, V.Pitch, V.Distance);
			Wait = 1.5;
		}
		else
		{
			const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("%d_%s.png"), View, Views[View].Name));
			FScreenshotRequest::RequestScreenshot(File, false, false);
			UE_LOG(LogTemp, Display, TEXT("FORGE shot %s"), *File);
			Wait = 0.3;
		}
		++Step;
		return;
	}
	// medição limpa: 4 s parado na vista média, sem foto nenhuma
	if (MotionFrame < 0 && !bMeasured)
	{
		if (MeasureStart < 0.0)
		{
			Pawn->SetView(Views[1].Focus, Views[1].Yaw, Views[1].Pitch, Views[1].Distance);
			MeasureStart = GetWorld()->GetRealTimeSeconds();
			FrameSum = 0.0;
			FrameCount = 0;
			Wait = 4.0;
			return;
		}
		const double Mean = FrameSum / FMath::Max(FrameCount, 1);
		UE_LOG(LogTemp, Display, TEXT("FORGE mean frame %.1f ms (%.0f fps) over %d frames (vista média, sem fotos)"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), FrameCount);
		bMeasured = true;
	}
	// passeio: a câmera de RTS anda 45 m em linha, um passo fixo por quadro
	// (o movimento não depende da taxa de quadros); uma foto por quadro
	if (MotionFrame < 0)
	{
		MotionFrame = 0;
	}
	if (MotionFrame < MotionFrames)
	{
		const float T = float(MotionFrame) / (MotionFrames - 1);
		Pawn->SetView(FVector2f(-30.0f + 45.0f * T, -20.0f + 10.0f * T), 15.0f, 50.0f, 75.0f);
		const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("motion_%03d.png"), MotionFrame));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		++MotionFrame;
		Wait = 0.0;
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE capture done"));
	CaptureFolder.Empty();
	FGenericPlatformMisc::RequestExit(false);
}
