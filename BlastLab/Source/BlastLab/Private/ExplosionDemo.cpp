#include "ExplosionDemo.h"

#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

AExplosionDemo::AExplosionDemo()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AExplosionDemo::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* CommandLine = FCommandLine::Get();
	if (!Sequence || FCString::Strifind(CommandLine, TEXT("MoviePipelineConfig")))
	{
		return;   // render do Movie Render Queue: ele mesmo toca a sequência
	}
	FParse::Value(CommandLine, TEXT("DemoCapture="), CaptureFolder);

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.LoopCount.Value = CaptureFolder.IsEmpty() ? -1 : 0;   // laço infinito, ou uma vez no teste
	ALevelSequenceActor* SequenceActor = nullptr;
	Player = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, SequenceActor);
	if (Player)
	{
		// a exposição automática precisa assentar na cena antes do clarão
		// (os renders do Movie Render Queue têm 60 quadros de aquecimento)
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (Camera && PC)
		{
			PC->SetViewTarget(Camera);
		}
		float Delay = 2.0f;
		FParse::Value(CommandLine, TEXT("DemoDelay="), Delay);
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			Player->Play();
			UE_LOG(LogTemp, Display, TEXT("DEMO playing %s"), *Sequence->GetName());
		}), FMath::Max(Delay, 0.01f), false);
	}
}

void AExplosionDemo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Player || CaptureFolder.IsEmpty())
	{
		return;
	}
	const float T = Player->GetCurrentTime().AsSeconds();
	if (T > 0.2f)
	{
		SumDt += DeltaSeconds;
		++NumDt;
	}
	if (CaptureTimes.IsValidIndex(NextCapture) && T >= CaptureTimes[NextCapture])
	{
		const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("demo_%.2f.png"), CaptureTimes[NextCapture]));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		UE_LOG(LogTemp, Display, TEXT("DEMO shot %s (t = %.2f s)"), *File, T);
		++NextCapture;
	}
	if (!CaptureTimes.IsValidIndex(NextCapture) && T >= CaptureTimes.Last() + 0.5f)
	{
		const double Mean = SumDt / FMath::Max(NumDt, 1);
		UE_LOG(LogTemp, Display, TEXT("DEMO mean frame %.1f ms (%.1f fps) over %d frames"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), NumDt);
		CaptureFolder.Empty();
		FGenericPlatformMisc::RequestExit(false);
	}
}
