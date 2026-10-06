#include "ForgeSliceGameMode.h"

#include "Engine/World.h"
#include "ForgeCameraPawn.h"
#include "ForgePlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStats.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// vistas da prova: as distâncias de câmera de RTS e duas de perto
	const FForgeSliceView DefaultViews[] = {
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
	// veio do botão "Ver em qualidade real": no máximo (Ultra = épico), seja qual
	// for o preset do gerador; o preset dele volta junto no "voltar"
	const int32 PrevQuality = UGameplayStatics::GetIntOption(OptionsString, TEXT("q"), -1);
	if (UGameplayStatics::HasOption(OptionsString, TEXT("real")) && GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("Scalability 3"));
		UE_LOG(LogTemp, Display, TEXT("FORGE real quality map: Ultra (gerador estava em %d)"), PrevQuality);
	}
	ResumeOptions = PrevQuality >= 0 ? FString::Printf(TEXT("resume?q=%d"), PrevQuality) : FString(TEXT("resume"));
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeCapture="), CaptureFolder))
	{
		IFileManager::Get().MakeDirectory(*CaptureFolder, true);
	}
	else if (GEngine && GEngine->GameViewport)
	{
		// volta pro gerador; "resume" traz os controles do último "Ver em qualidade real"
		TWeakObjectPtr<AForgeSliceGameMode> Weak(this);
		BackButton = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(14.0f))
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor(0.10f, 0.11f, 0.12f, 0.85f))
				.OnClicked_Lambda([Weak]()
				{
					if (Weak.IsValid())
					{
						UGameplayStatics::OpenLevel(Weak.Get(), FName(TEXT("/Game/Maps/Forge")), true, Weak->ResumeOptions);
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("WorldForge", "BackToForge", "← Voltar ao gerador     F2: teste de qualidade · F3: grupos de triângulos"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				]
			];
		// 1ª abertura de um mapa novo: os materiais compilam e a Unreal mostra o
		// quadriculado padrão até terminar — avisa em vez de parecer quebrado
		ShaderNote = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.0f, 18.0f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				.ShadowOffset(FVector2D(1.0, 1.0))
				.Visibility_Lambda([]() { return GShaderCompilingManager && GShaderCompilingManager->IsCompiling() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.Text_Lambda([]()
				{
					const int32 Left = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
					return FText::Format(NSLOCTEXT("WorldForge", "Compiling", "preparando os materiais (1ª vez): faltam {0}"), FText::AsNumber(Left));
				})
			];
		GEngine->GameViewport->AddViewportWidgetContent(ShaderNote.ToSharedRef(), 9);
		GEngine->GameViewport->AddViewportWidgetContent(BackButton.ToSharedRef(), 10);
	}
	// -ForgeViews=nome:x,y,yaw,pitch,dist;... troca as vistas (testes de cena);
	// -ForgeNoMotion pula a medição e o passeio
	Views.Append(DefaultViews, UE_ARRAY_COUNT(DefaultViews));
	FString Custom;
	if (FParse::Value(FCommandLine::Get(), TEXT("ForgeViews="), Custom, false))
	{
		TArray<FString> Items;
		Custom.ParseIntoArray(Items, TEXT(";"));
		TArray<FForgeSliceView> Parsed;
		for (const FString& Item : Items)
		{
			FString Name, Numbers;
			TArray<FString> N;
			if (Item.Split(TEXT(":"), &Name, &Numbers) && Numbers.ParseIntoArray(N, TEXT(",")) == 5)
			{
				Parsed.Add({ Name, FVector2f(FCString::Atof(*N[0]), FCString::Atof(*N[1])), FCString::Atof(*N[2]), FCString::Atof(*N[3]), FCString::Atof(*N[4]) });
			}
		}
		if (Parsed.Num())
		{
			Views = Parsed;
		}
	}
	bNoMotion = FParse::Param(FCommandLine::Get(), TEXT("ForgeNoMotion"));
}

void AForgeSliceGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const TSharedPtr<SWidget>& W : { BackButton, ShaderNote })
	{
		if (W.IsValid() && GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef());
		}
	}
	BackButton.Reset();
	ShaderNote.Reset();
	Super::EndPlay(Reason);
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
	Pawn->bLocked = true;   // o mouse de quem estiver no PC não mexe nas fotos
	const int32 NumViews = Views.Num();
	const int32 View = Step / 2;
	if (View < NumViews)
	{
		// duas fases por vista: põe a câmera e espera assentar; depois fotografa
		if (Step % 2 == 0)
		{
			const FForgeSliceView& V = Views[View];
			Pawn->SetView(V.Focus, V.Yaw, V.Pitch, V.Distance);
			// -ForgeDebugView: o modo teste (F2) no ponto da 1ª vista, e as fotos com a interface;
			// -ForgeDebugToggle=1,3: liga/desliga o F2 no começo dessas vistas (teste do liga-desliga)
			FString Toggles;
			const bool bToggleHere = (View == 0 && FParse::Param(FCommandLine::Get(), TEXT("ForgeDebugView")))
				|| (FParse::Value(FCommandLine::Get(), TEXT("ForgeDebugToggle="), Toggles, false)
					&& (TEXT(",") + Toggles + TEXT(",")).Contains(FString::Printf(TEXT(",%d,"), View)));
			if (bToggleHere)
			{
				if (AForgePlayerController* FPC = Cast<AForgePlayerController>(PC))
				{
					FPC->ToggleDebugView();
				}
			}
			Wait = 1.5;
		}
		else
		{
			const FString File = FPaths::Combine(CaptureFolder, FString::Printf(TEXT("%d_%s.png"), View, *Views[View].Name));
			const bool bUI = FParse::Param(FCommandLine::Get(), TEXT("ForgeDebugView")) || FParse::Param(FCommandLine::Get(), TEXT("ForgeShotUI"));
			// janela minimizada (a tela vira 8×8): a Unreal quebra ao copiar a foto (D3D12Texture.cpp)
			FVector2D ShotSize(0, 0);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ShotSize);
			if (ShotSize.X >= 64 && ShotSize.Y >= 64)
				FScreenshotRequest::RequestScreenshot(File, bUI, false);
			// -ForgeDumpMemory: o que ocupa a memória de vídeo, no log (na 1ª foto)
			FString DumpAt;
			if (FParse::Value(FCommandLine::Get(), TEXT("ForgeDumpMemory="), DumpAt) && (TEXT(",") + DumpAt + TEXT(",")).Contains(FString::Printf(TEXT(",%d,"), View)) && GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpMemory"));
				GEngine->Exec(GetWorld(), TEXT("rhi.DumpResourceMemory 60"));
			}
			FTextureMemoryStats Mem;
			RHIGetTextureMemoryStats(Mem);
			UE_LOG(LogTemp, Display, TEXT("FORGE shot %s · VRAM %.0f/%.0f MB, texturas %.0f MB (pool %.0f MB), estourado %.0f MB · %.1f ms"), *File,
				(Mem.StreamingMemorySize + Mem.NonStreamingMemorySize) / 1048576.0, Mem.TotalGraphicsMemory / 1048576.0,
				Mem.StreamingMemorySize / 1048576.0, Mem.TexturePoolSize / 1048576.0, GDemotedLocalMemorySize / 1048576.0,
				1000.0 * FrameSum / FMath::Max(FrameCount, 1));
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
			const FForgeSliceView& V = Views[FMath::Min(1, Views.Num() - 1)];
			Pawn->SetView(V.Focus, V.Yaw, V.Pitch, V.Distance);
			MeasureStart = GetWorld()->GetRealTimeSeconds();
			FrameSum = 0.0;
			FrameCount = 0;
			Wait = 4.0;
			// -ForgeProfile (com -csvGpuStats): tempos de GPU por passe nos mesmos 4 s
			// (Saved/Profiling/CSV; Tools/gpu_profile.py resume)
			if (FParse::Param(FCommandLine::Get(), TEXT("ForgeProfile")))
			{
				PC->ConsoleCommand(TEXT("csvprofile start"));
			}
			return;
		}
		const double Mean = FrameSum / FMath::Max(FrameCount, 1);
		UE_LOG(LogTemp, Display, TEXT("FORGE mean frame %.1f ms (%.0f fps) over %d frames (vista média, sem fotos)"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), FrameCount);
		bMeasured = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("ForgeProfile")))
		{
			PC->ConsoleCommand(TEXT("csvprofile stop"));
			Wait = 3.0;   // o CSV é gravado em segundo plano: fechar já deixava o arquivo vazio
			return;
		}
	}
	if (bNoMotion)
	{
		MotionFrame = MotionFrames;   // só as fotos e a medição
	}
	// passeio: a câmera de RTS anda 45 m em linha, um passo fixo por quadro
	// (o movimento não depende da taxa de quadros); uma foto por quadro
	if (MotionFrame < 0)
	{
		// põe a câmera no começo do passeio e espera: o salto da vista anterior
		// borrava o 1º quadro (desfoque de movimento)
		MotionFrame = 0;
		Pawn->SetView(FVector2f(-30.0f, -20.0f), 15.0f, 50.0f, 75.0f);
		Wait = 1.0;
		return;
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
	if (FParse::Param(FCommandLine::Get(), TEXT("ForgeBack")))
	{
		// teste do botão "voltar ao gerador": o mesmo caminho do clique
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/Forge")), true, ResumeOptions);
		return;
	}
	FGenericPlatformMisc::RequestExit(false);
}
