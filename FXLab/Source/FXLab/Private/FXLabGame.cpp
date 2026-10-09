#include "FXLabGame.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FXImpact.h"
#include "FXSceneLight.h"
#include "FXWreckFire.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "Scalability.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// as câmeras do laboratório (three.js, metros, y pra cima) nos eixos da Unreal (cm, z pra cima):
	// Unreal (x, y, z) = three (x, z, y). No mapa do jogo, relativas ao grupo principal.
	struct FCamPreset { FVector Pos, Target; };
	FCamPreset CamPreset(const FString& Name)
	{
		if (Name == TEXT("low"))
		{
			return { FVector(-400.0, 2600.0, 300.0), FVector(0.0, 0.0, 550.0) };
		}
		return { FVector(0.0, 2600.0, 3600.0), FVector(0.0, 0.0, 500.0) };
	}
	constexpr double VerticalFovDeg = 32.0;

	// a câmera do jogo (Game/Config/DefaultGame.ini, RTSCameraPawn): inclinação 37,5°, giro 45°,
	// campo horizontal de 50°, altura 31,6–81,6 m (padrão 61,1)
	constexpr double GamePitch = 37.5, GameYaw = 45.0, GameFovDeg = 50.0;
	double GameHeight(const FString& Name)
	{
		return Name == TEXT("game_close") ? 31.6 : Name == TEXT("game_far") ? 81.6 : 61.1;
	}

	FString RepoDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
	}

	// mediana e p95 (a média leva junto os quadros de travada)
	void Stats(TArray<float> V, float& Median, float& P95)
	{
		Median = P95 = 0.0f;
		if (!V.Num())
		{
			return;
		}
		V.Sort();
		Median = V[V.Num() / 2];
		P95 = V[FMath::Min(V.Num() - 1, int32(V.Num() * 0.95))];
	}

	// a medida: aquece (shaders e Lumen), depois cada carga um tempo
	struct FPhase { const TCHAR* Name; int32 Load; double Seconds; };
	const FPhase Phases[] = {
		{ TEXT("aquecendo"), 3, 8.0 },
		{ TEXT("sem efeito"), 0, 8.0 },
		{ TEXT("1 fogo + 1 tiro"), 1, 10.0 },
		{ TEXT("3 fogos + 3 tiros"), 3, 10.0 },
	};
}

AFXLabGameMode::AFXLabGameMode()
{
	PlayerControllerClass = AFXLabController::StaticClass();
	DefaultPawnClass = nullptr;
}

void AFXLabController::BeginPlay()
{
	Super::BeginPlay();
	// o grupo principal primeiro (a câmera mira nele)
	for (TActorIterator<AFXWreckFire> It(GetWorld()); It; ++It)
	{
		Fires.Insert(*It, It->ActorHasTag(TEXT("Load1")) ? 0 : Fires.Num());
	}
	for (TActorIterator<AFXImpact> It(GetWorld()); It; ++It)
	{
		Impacts.Insert(*It, It->ActorHasTag(TEXT("Load1")) ? 0 : Impacts.Num());
	}
	bGame = FFXSceneLight::Find(GetWorld()).bGame;
	if (bGame && Fires.Num() && Impacts.Num())
	{
		// entre a carcaça e o tiro, no chão
		const FVector F = Fires[0]->GetActorLocation(), I = Impacts[0]->GetActorLocation();
		Focus = FVector((F.X + I.X) * 0.5, (F.Y + I.Y) * 0.5, I.Z);
	}

	FActorSpawnParameters Sp;
	Sp.Name = TEXT("FXLabViewCam");
	ViewCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Sp);
	ViewCam->GetCameraComponent()->SetConstraintAspectRatio(false);
	SetViewTarget(ViewCam);
	FString Cam = bGame ? TEXT("game") : TEXT("rts");
	FParse::Value(FCommandLine::Get(), TEXT("-Cam="), Cam);
	SetCam(Cam);

	if (!bGame)
	{
		// exposição 1, sem adaptação do olho: a mesma conta de tela do laboratório
		if (UGameViewportClient* Vp = GetWorld()->GetGameViewport())
		{
			Vp->EngineShowFlags.SetEyeAdaptation(false);
		}
	}
	else
	{
		// a exposição é a travada do mapa (o pós dele); o campo é o do jogo
		ViewCam->GetCameraComponent()->SetFieldOfView(float(GameFovDeg));
		FParse::Value(FCommandLine::Get(), TEXT("-Load="), Load);
		SetLoad(Load);
	}

	FString At;
	if (FParse::Value(FCommandLine::Get(), TEXT("-Shot="), ShotName))
	{
		if (bGame && FParse::Value(FCommandLine::Get(), TEXT("-At="), At, false))
		{
			TArray<FString> Items;
			At.ParseIntoArray(Items, TEXT(","));
			for (const FString& S : Items)
			{
				ShotAt.Add(FCString::Atod(*S));
			}
			bWarming = true;
			SetPausedAll(true);
		}
		return;                        // captura: sem painel nem cursor
	}
	if (bGame && FParse::Value(FCommandLine::Get(), TEXT("-Measure="), MeasureName))
	{
		MeasurePhase = 0;
		PhaseClock = 0.0;
		LastTime = FPlatformTime::Seconds();
		SetLoad(Phases[0].Load);
		return;
	}
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	auto Button = [this](const FString& Label, TFunction<void()> Act)
	{
		return SNew(SButton)
			.OnClicked_Lambda([Act]() { Act(); return FReply::Handled(); })
			[SNew(STextBlock).Text(FText::FromString(Label))];
	};
	auto ToggleFire = [this]()
	{
		for (AFXWreckFire* F : Fires)
		{
			const bool bHide = !F->IsHidden();
			F->SetActorHiddenInGame(bHide);
			F->SetActorTickEnabled(!bHide);
		}
	};
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[SNew(STextBlock).Text(FText::FromString(bGame ? TEXT("Deserto do WorldForge · luz do jogo · 6-way (Unreal)")
			: TEXT("Fogo da carcaça · 6-way (Unreal)")))];
	if (bGame)
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("câmera do jogo"), [this]() { SetCam(TEXT("game")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("zoom perto"), [this]() { SetCam(TEXT("game_close")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("zoom longe"), [this]() { SetCam(TEXT("game_far")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("de perto (laboratório)"), [this]() { SetCam(TEXT("rts")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("baixa"), [this]() { SetCam(TEXT("low")); })]
		];
		Box->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("1 fogo + 1 tiro"), [this]() { SetLoad(1); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("3 fogos + 3 tiros"), [this]() { SetLoad(3); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("sem efeito"), [this]() { SetLoad(0); })]
		];
	}
	else
	{
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("câmera RTS"), [this]() { SetCam(TEXT("rts")); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("baixa"), [this]() { SetCam(TEXT("low")); })]
		];
	}
	Box->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("pausar"), [this]() { TogglePause(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("0.25×"), [this]() { ToggleSlow(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("recomeçar o fogo"), [this]() { for (AFXWreckFire* F : Fires) F->Restart(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("tiro no chão"), [this]() { for (AFXImpact* I : Impacts) if (!I->IsHidden()) I->Fire(); })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(TEXT("fogo da carcaça liga/desliga"), ToggleFire)]
	];
	Box->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
	[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Readout); })];
	Box->AddSlot().AutoHeight().Padding(0, 2, 0, 0)
	[SNew(STextBlock).Text(FText::FromString(bGame ? TEXT("botão direito: girar · rodinha: aproximar")
		: TEXT("botão direito: girar · rodinha: aproximar · o ajuste vem do FXLab/preset.json")))];
	Panel = SNew(SBorder)
		.Padding(10.0f)
		.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.85f))
		[Box];
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(10.0f))[Panel.ToSharedRef()]);
	}
}

void AFXLabController::EndPlay(const EEndPlayReason::Type Reason)
{
	Panel.Reset();
	Super::EndPlay(Reason);
}

void AFXLabController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AFXLabController::ZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AFXLabController::ZoomOut);
}

void AFXLabController::SetCam(const FString& Name)
{
	CamName = Name;
	if (bGame && Name.StartsWith(TEXT("game")))
	{
		// a do RTSCameraPawn: alvo no chão, recuada ao longo da vista até a altura
		Target = Focus;
		Pitch = GamePitch;
		Yaw = GameYaw + 180.0;
		Dist = GameHeight(Name) * 100.0 / FMath::Sin(FMath::DegreesToRadians(GamePitch));
		ApplyOrbit();
		return;
	}
	const FCamPreset C = CamPreset(Name);
	Target = C.Target + Focus;
	const FVector D = C.Pos - C.Target;
	Dist = D.Size();
	Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Pitch = FMath::RadiansToDegrees(FMath::Asin(D.Z / Dist));
	ApplyOrbit();
}

void AFXLabController::ApplyOrbit()
{
	if (!ViewCam)
	{
		return;
	}
	const double Cy = FMath::Cos(FMath::DegreesToRadians(Yaw)), Sy = FMath::Sin(FMath::DegreesToRadians(Yaw));
	const double Cp = FMath::Cos(FMath::DegreesToRadians(Pitch)), Spp = FMath::Sin(FMath::DegreesToRadians(Pitch));
	const FVector Pos = Target + FVector(Cp * Cy, Cp * Sy, Spp) * Dist;
	ViewCam->SetActorLocationAndRotation(Pos, (Target - Pos).Rotation());
}

void AFXLabController::SetLoad(int32 N)
{
	Load = N;
	auto Show = [N](AActor* A)
	{
		const bool bOn = N >= 3 || (N >= 1 && A->ActorHasTag(TEXT("Load1")));
		A->SetActorHiddenInGame(!bOn);
		A->SetActorTickEnabled(bOn);
	};
	for (AFXWreckFire* F : Fires)
	{
		Show(F);
	}
	for (AFXImpact* I : Impacts)
	{
		Show(I);
	}
}

void AFXLabController::SetPausedAll(bool bPause)
{
	for (AFXWreckFire* F : Fires)
	{
		F->bPaused = bPause;
	}
	for (AFXImpact* I : Impacts)
	{
		I->bPaused = bPause;
	}
}

bool AFXLabController::ShadersBusy() const
{
	return GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
}

void AFXLabController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (ViewCam && GEngine && GEngine->GameViewport && !bGame)
	{
		// o campo vertical fixo (32°, o do laboratório); a Unreal guarda o horizontal
		FVector2D Size;
		GEngine->GameViewport->GetViewportSize(Size);
		if (Size.Y > 0)
		{
			const double H = 2.0 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalFovDeg * 0.5)) * Size.X / Size.Y);
			ViewCam->GetCameraComponent()->SetFieldOfView(float(FMath::RadiansToDegrees(H)));
		}
	}
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		float Dx = 0.0f, Dy = 0.0f;
		GetInputMouseDelta(Dx, Dy);
		Yaw += Dx * 0.4;
		Pitch = FMath::Clamp(Pitch - Dy * 0.4, -5.0, 85.0);
		ApplyOrbit();
	}
	// o que o painel mostra: quadro e placa (mediana do último segundo)
	RecentMs.Add(DeltaTime * 1000.0f);
	RecentGpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	if (RecentMs.Num() >= 60)
	{
		float M, P, Mg, Pg;
		Stats(RecentMs, M, P);
		Stats(RecentGpu, Mg, Pg);
		Readout = FString::Printf(TEXT("quadro %.1f ms (%.0f qps) · placa %.1f ms"), M, 1000.0f / FMath::Max(M, 0.1f), Mg);
		RecentMs.Reset();
		RecentGpu.Reset();
	}
	TickShots();
	TickMeasure(DeltaTime);
}

void AFXLabController::TickShots()
{
	if (!ShotAt.Num() && ExitIn < 0)
	{
		return;
	}
	if (bWarming)
	{
		// os efeitos ficam parados (mas desenhados) até a Unreal acabar de compilar o que eles e o mapa usam
		if (++Warm < 30 || ShadersBusy())
		{
			return;
		}
		bWarming = false;
		SetPausedAll(false);           // os efeitos já dão o 1º passo neste quadro
		UE_LOG(LogTemp, Display, TEXT("FXLab: aquecido em %d quadros; o relógio começa"), Warm);
	}
	// o quadro que anda agora: os efeitos dão o passo de 1/30 s depois desta função (TG_PostUpdateWork),
	// e a foto sai do quadro já desenhado com eles
	++Frame;
	const double T = Frame / 30.0;
	if (ShotAt.Num() && T >= ShotAt[0] - 1e-6)
	{
		const FString Path = RepoDir() / TEXT("captures/fxlab/shots_ue") / FString::Printf(TEXT("%s_%.2fs.png"), *ShotName, ShotAt[0]);
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		ShotAt.RemoveAt(0);
		if (!ShotAt.Num())
		{
			ExitIn = 6;
		}
	}
	if (ExitIn > 0 && --ExitIn == 0)
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}

void AFXLabController::TickMeasure(float DeltaTime)
{
	if (MeasurePhase < 0)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const float RealMs = float((Now - LastTime) * 1000.0);
	LastTime = Now;
	if (MeasurePhase == 0 && ShadersBusy())
	{
		return;                        // aquece só depois de compilar
	}
	PhaseClock += RealMs / 1000.0;
	if (MeasurePhase > 0)
	{
		FrameMs.Add(RealMs);
		GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
		RenderMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	}
	if (PhaseClock < Phases[MeasurePhase].Seconds)
	{
		return;
	}
	if (MeasurePhase > 0)
	{
		float M, P, Mg, Pg, Mt, Pt, Mr, Pr;
		Stats(FrameMs, M, P);
		Stats(GpuMs, Mg, Pg);
		Stats(GameMs, Mt, Pt);
		Stats(RenderMs, Mr, Pr);
		const FString Line = FString::Printf(TEXT("%-18s quadro %5.1f ms (p95 %5.1f) · placa %5.1f ms (p95 %5.1f) · jogo %4.1f ms · render %4.1f ms · %d quadros"),
			Phases[MeasurePhase].Name, M, P, Mg, Pg, Mt, Mr, FrameMs.Num());
		Report.Add(Line);
		UE_LOG(LogTemp, Display, TEXT("FXLab medida: %s"), *Line);
	}
	FrameMs.Reset();
	GpuMs.Reset();
	GameMs.Reset();
	RenderMs.Reset();
	PhaseClock = 0.0;
	if (++MeasurePhase < UE_ARRAY_COUNT(Phases))
	{
		SetLoad(Phases[MeasurePhase].Load);
		return;
	}
	MeasurePhase = -1;
	FIntPoint Res(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		Res = GEngine->GameViewport->Viewport->GetSizeXY();
	}
	const Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
	Report.Insert(FString::Printf(TEXT("FXLab, medida %s: %s, %dx%d, qualidade %d (0 baixa … 3 épica, -1 misturada), câmera %s, %s"),
		*MeasureName, *GetWorld()->GetMapName(), Res.X, Res.Y, Q.GetSingleQualityLevel(), *CamName, *FDateTime::Now().ToString()), 0);
	FFileHelper::SaveStringArrayToFile(Report, *(RepoDir() / TEXT("captures/fxlab") / FString::Printf(TEXT("measure_%s.txt"), *MeasureName)),
		FFileHelper::EEncodingOptions::ForceUTF8);
	FGenericPlatformMisc::RequestExit(false);
}

void AFXLabController::ZoomIn()
{
	Dist = FMath::Max(300.0, Dist * 0.9);
	ApplyOrbit();
}

void AFXLabController::ZoomOut()
{
	Dist = FMath::Min(20000.0, Dist / 0.9);
	ApplyOrbit();
}

void AFXLabController::TogglePause()
{
	for (AFXWreckFire* F : Fires)
	{
		F->bPaused = !F->bPaused;
	}
	for (AFXImpact* I : Impacts)
	{
		I->bPaused = !I->bPaused;
	}
}

void AFXLabController::ToggleSlow()
{
	bSlow = !bSlow;
	for (AFXWreckFire* F : Fires)
	{
		F->TimeScale = bSlow ? 0.25 : 1.0;
	}
	for (AFXImpact* I : Impacts)
	{
		I->TimeScale = bSlow ? 0.25 : 1.0;
	}
}
