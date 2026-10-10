#include "BlastLabController.h"

#include "BlastRaymarchSubsystem.h"

#include "BlastLabCamera.h"
#include "BlastLabEnvironment.h"
#include "BlastLabGround.h"
#include "BlastLabMover.h"
#include "BlastLabPanel.h"
#include "BlastLabSimulation.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ExplosionBake.h"
#include "ExplosionVolume.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "SceneView.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/SWeakWidget.h"

namespace
{
	constexpr float ClickPixels = 5.0f;          // até isso de arrasto, ainda é clique (main.js dragDist)
	constexpr float ClickRangeM = 260.0f;        // alcance do clique (main.js maxClickRange)
	constexpr float ZoomRtsM = 220.0f;
	const TCHAR* ThumbnailPath = TEXT("/Game/UI/T_ThumbBlast.T_ThumbBlast");
}

ABlastLabController::ABlastLabController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	DefaultMouseCursor = EMouseCursor::Crosshairs;    // o WebGL: cursor de mira sobre o mundo
}

ABlastLabCamera* ABlastLabController::Cam() const
{
	return Cast<ABlastLabCamera>(GetPawn());
}

void ABlastLabController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	// teste automático com a mesma história em todo A/B: a variante gravada, o
	// tamanho e o ruído de cada explosão saem do mesmo sorteio (-AutoSeed=<n>)
	if (FCString::Strifind(FCommandLine::Get(), TEXT("AutoTest=")))
	{
		int32 Seed = 1;
		FParse::Value(FCommandLine::Get(), TEXT("AutoSeed="), Seed);
		FMath::RandInit(Seed);
		FMath::SRandInit(Seed);
	}
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);            // sem o cursor piscar a cada clique
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	// preset de qualidade: -Quality=baixa|media|alta|ultra (padrão alta). Só na
	// abertura: trocando de campo pelo painel, fica o que o jogador escolheu
	static bool bQualityApplied = false;
	if (!bQualityApplied)
	{
		bQualityApplied = true;
		ABlastLabCamera::bShakeEnabled = !FParse::Param(FCommandLine::Get(), TEXT("NoShake"));
		ABlastLabCamera::bTripod = FCString::Strifind(FCommandLine::Get(), TEXT("-AutoTest=")) && !FParse::Param(FCommandLine::Get(), TEXT("LiveCam"));
		FString Quality = TEXT("alta");
		FParse::Value(FCommandLine::Get(), TEXT("Quality="), Quality);
		SBlastLabPanel::ApplyQuality(Quality == TEXT("baixa") ? EBlastQuality::Low : Quality == TEXT("media") ? EBlastQuality::Medium
			: Quality == TEXT("ultra") ? EBlastQuality::Ultra : EBlastQuality::High);
	}

	Thumbnail = LoadObject<UTexture2D>(nullptr, ThumbnailPath);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Panel = SNew(SBlastLabPanel).World(GetWorld()).Thumbnail(Thumbnail);
		Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 10);
		// -NoPanel: começa escondido (o H), pras fotos de comparação com a tela inteira
		if (FParse::Param(FCommandLine::Get(), TEXT("NoPanel")))
		{
			Panel->SetVisibility(EVisibility::Collapsed);
		}
	}

	// -TimeOfDay=<h>: começa nessa hora (testes de crepúsculo e noite); trocando
	// de campo pelo painel, a hora vem na opção do mapa (?TimeOfDay=<h>)
	float StartHour = -1.0f;
	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (GameMode && UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("TimeOfDay")))
	{
		StartHour = FCString::Atof(*UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("TimeOfDay")));
	}
	if (StartHour >= 0.0f || FParse::Value(FCommandLine::Get(), TEXT("TimeOfDay="), StartHour))
	{
		if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
		{
			Env->SetTimeOfDay(StartHour);
		}
	}

	// -Cam=dist,azim,elev,alvoY,fovV: enquadramento fixo (testes contra as referências do WebGL)
	FString CamArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("Cam="), CamArg, false))
	{
		TArray<FString> P;
		CamArg.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 5)
		{
			// o pawn da câmera ainda pode não existir aqui: aplicado no primeiro PlayerTick
			PendingView = { FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3]), FCString::Atof(*P[4]) };
		}
	}

	// aquecimento dos shaders das explosões, bem na frente da câmera inicial
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->Prewarm(FVector(0.0, 0.0, BlastLabGround::HeightAt(GetWorld(), 0.0, 0.0)));
		// -Weapon=<índice>: abre com a arma escolhida no painel (6 = Morte de Tanque)
		int32 Weapon = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("-Weapon="), Weapon) && UBlastLabSimulation::Arsenal().IsValidIndex(Weapon))
		{
			Sim->SelectedWeapon = Weapon;
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("AutoTest="), AutoFolder);
	// -AutoWatch=<pasta>: o mesmo teste, mas quem detona é um clique de verdade
	// (o fluxo do usuário); o teste só espera a primeira explosão e fotografa
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoWatch="), AutoFolder))
	{
		bAutoWatch = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("AutoWeapon="), AutoWeapon);
	// -AutoShots=0.4,0.8,1.5,2.5,4,8,12: instantes das fotos (s da gravação)
	FString Shots;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoShots="), Shots, false))
	{
		TArray<FString> P;
		Shots.ParseIntoArray(P, TEXT(","));
		AutoTimes.Reset();
		for (const FString& S : P)
		{
			AutoTimes.Add(FCString::Atof(*S));
		}
	}
}

void ABlastLabController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Panel.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
		}
		Panel.Reset();
	}
	Super::EndPlay(Reason);
}

void ABlastLabController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ABlastLabController::LeftDown);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ABlastLabController::LeftUp);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABlastLabController::RightDown);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ABlastLabController::RightUp);
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ABlastLabController::Wheel);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ABlastLabController::TogglePause);
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ABlastLabController::Slower);
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ABlastLabController::Faster);
	InputComponent->BindKey(EKeys::Backslash, IE_Pressed, this, &ABlastLabController::ResetTime);
	InputComponent->BindKey(EKeys::Comma, IE_Pressed, this, &ABlastLabController::EarlierHour);
	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ABlastLabController::LaterHour);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ABlastLabController::ToggleCycle);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ABlastLabController::TogglePanel);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ABlastLabController::DetonateCenter);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ABlastLabController::PassPlane);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ABlastLabController::PassMissile);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ABlastLabController::PassShell);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ABlastLabController::PassBomber);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABlastLabController::Quality<0>);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABlastLabController::Quality<1>);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABlastLabController::Quality<2>);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ABlastLabController::Quality<3>);
}

// ---- terreno sob o cursor --------------------------------------------------------
bool ABlastLabController::GroundUnderScreenPoint(const FVector2D& Screen, FVector& Out) const
{
	FVector Origin, Dir;
	if (!DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Dir))
	{
		return false;
	}
	// longe demais: o clique perto do horizonte jogaria a explosão a quilômetros
	const ABlastLabCamera* C = Cam();
	const float Range = ClickRangeM * FMath::Max(1.0f, (C ? C->Distance() : 0.0f) / ZoomRtsM) * 100.0f;
	return BlastLabGround::Ray(GetWorld(), Origin, Dir, Range, Out);
}

bool ABlastLabController::GroundUnderCursor(FVector& Out) const
{
	float X, Y;
	return GetMousePosition(X, Y) && GroundUnderScreenPoint(FVector2D(X, Y), Out);
}

AExplosionVolume* ABlastLabController::DetonateAt(const FVector& Ground)
{
	UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	if (!Sim)
	{
		return nullptr;
	}
	AExplosionVolume* E = Sim->Detonate(Ground, Sim->SelectedWeapon);
	// a comparação lado a lado (-RmCompare=<gravação>[,...]): uma gêmea por
	// gravação, 40 m à direita na tela, a seguinte 40 m à esquerda (e assim por
	// diante), com o mesmo tamanho, relógio e semente; a do clique toca a gravação
	// de sempre (a do WebGL, 96), cada gêmea a sua (r128_v0, eg_gasoline_x15, mf_charge...)
	if (E && UBlastRaymarchSubsystem::CompareMode())
	{
		const FVector Right = PlayerCameraManager ? FRotationMatrix(PlayerCameraManager->GetCameraRotation()).GetScaledAxis(EAxis::Y) : FVector::RightVector;
		const TArray<FName>& Sources = UBlastRaymarchSubsystem::CompareSources();
		FString Msg = TEXT("COMPARACAO: onde voce clicou = a gravacao atual (WebGL 96)");
		for (int32 k = 0; k < Sources.Num(); ++k)
		{
			const double Side = (k % 2 ? -1.0 : 1.0) * (k / 2 + 1);
			FVector P = Ground + FVector(Right.X, Right.Y, 0.0).GetSafeNormal() * 4000.0 * Side * FMath::Max(E->SizeScale, 1.0f);
			P.Z = BlastLabGround::HeightAt(GetWorld(), P.X, P.Y);
			if (AExplosionVolume* Twin = Sim->Detonate(P, Sim->SelectedWeapon))
			{
				Twin->Bake = E->Bake;            // a mesma gravação da Unreal: a mesma luz do fogo e a mesma sombra no chão
				Twin->SizeScale = E->SizeScale;
				Twin->TimeScale = E->TimeScale;
				Twin->Seed = E->Seed;
				Twin->Thickness = E->Thickness;
				Twin->RaymarchSource = Sources[k];
			}
			Msg += FString::Printf(TEXT("   |   %d m a %s = %s"), int32(40 * FMath::Abs(Side)), Side > 0.0 ? TEXT("direita") : TEXT("esquerda"), *Sources[k].ToString());
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(424242, 30.0f, FColor::Yellow, Msg);
		}
	}
	if (E && bAutoWatch && !AutoExplosion)
	{
		AutoExplosion = E;
		bAutoDetonated = true;
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST click detonated weapon %d at %s"), Sim->SelectedWeapon, *Ground.ToString());
	}
	if (E)
	{
		// o tranco da câmera não sai mais aqui: o tremor chega com o choque, em
		// d/343 s (UBlastLabSimulation::ShockArrived)
		if (Panel.IsValid())
		{
			Panel->Fired(Sim->SelectedWeapon);
		}
	}
	return E;
}

// ---- mouse ------------------------------------------------------------------------
void ABlastLabController::LeftDown()
{
	float X, Y;
	if (GetMousePosition(X, Y))
	{
		bLeft = true;
		LeftStart = LastMouse = FVector2D(X, Y);
		DragPixels = 0.0f;
	}
}

void ABlastLabController::LeftUp()
{
	if (bLeft && DragPixels < ClickPixels)
	{
		FVector Ground;
		if (GroundUnderCursor(Ground))
		{
			DetonateAt(Ground);
		}
	}
	bLeft = false;
}

void ABlastLabController::RightDown()
{
	float X, Y;
	if (GetMousePosition(X, Y))
	{
		bRight = true;
		Anchor = FVector2D(X, Y);
	}
}

void ABlastLabController::RightUp()
{
	bRight = false;
}

void ABlastLabController::Wheel(float Value)
{
	if (Value != 0.0f)
	{
		if (ABlastLabCamera* C = Cam())
		{
			C->Zoom(Value);
		}
	}
}

void ABlastLabController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// o tempo da câmera é o real: a pausa e a câmera lenta são da simulação
	const float RealDt = FApp::GetDeltaTime();
	float X, Y;
	const bool bHaveMouse = GetMousePosition(X, Y);
	ABlastLabCamera* C = Cam();
	if (C && PendingView.Num() == 5)
	{
		C->SetView(PendingView[0], PendingView[1], PendingView[2], PendingView[3], PendingView[4]);
		PendingView.Reset();
	}
	if (C && bHaveMouse)
	{
		const FVector2D Mouse(X, Y);
		if (bLeft)
		{
			const FVector2D D = Mouse - LastMouse;
			DragPixels += FMath::Abs(D.X) + FMath::Abs(D.Y);
			if (DragPixels >= ClickPixels)
			{
				C->Orbit(D.X, D.Y);
			}
			LastMouse = Mouse;
		}
		if (bRight)
		{
			int32 W, H;
			GetViewportSize(W, H);
			// a âncora acompanha o cursor quando ele passa de meia tela (main.js)
			Anchor.X = FMath::Clamp(Anchor.X, Mouse.X - W * 0.5f, Mouse.X + W * 0.5f);
			Anchor.Y = FMath::Clamp(Anchor.Y, Mouse.Y - H * 0.5f, Mouse.Y + H * 0.5f);
			const float Half = FMath::Max(H * 0.5f, 1.0f);
			C->ScrollRMB((Mouse.X - Anchor.X) / Half, (Mouse.Y - Anchor.Y) / Half, RealDt);
		}
	}
	TickAutoTest(RealDt);
}

// ---- teclado ----------------------------------------------------------------------
void ABlastLabController::TogglePause()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->bPaused = !Sim->bPaused;
	}
}

void ABlastLabController::Slower()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->TimeScale = FMath::Clamp(Sim->TimeScale / 1.5f, 0.05f, 4.0f);
	}
}

void ABlastLabController::Faster()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->TimeScale = FMath::Clamp(Sim->TimeScale * 1.5f, 0.05f, 4.0f);
	}
}

void ABlastLabController::ResetTime()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->TimeScale = 1.0f;
	}
}

void ABlastLabController::EarlierHour()
{
	if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
	{
		Env->bAutoCycle = false;
		Env->SetTimeOfDay(Env->TimeOfDay - 1.0f / 6.0f);
	}
}

void ABlastLabController::LaterHour()
{
	if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
	{
		Env->bAutoCycle = false;
		Env->SetTimeOfDay(Env->TimeOfDay + 1.0f / 6.0f);
	}
}

void ABlastLabController::ToggleCycle()
{
	if (ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
	{
		Env->bAutoCycle = !Env->bAutoCycle;
	}
}

void ABlastLabController::TogglePanel()
{
	if (Panel.IsValid())
	{
		Panel->SetVisibility(Panel->GetVisibility() == EVisibility::Collapsed ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	}
}

void ABlastLabController::DetonateCenter()
{
	int32 W, H;
	GetViewportSize(W, H);
	FVector Ground;
	if (GroundUnderScreenPoint(FVector2D(W * 0.5f, H * 0.5f), Ground))
	{
		DetonateAt(Ground);
	}
}

void ABlastLabController::PassPlane()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->SpawnMover(EBlastMover::Plane);
	}
}

void ABlastLabController::PassMissile()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->SpawnMover(EBlastMover::Missile);
	}
}

void ABlastLabController::PassBomber()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->SpawnMover(EBlastMover::Bomber);
	}
}

void ABlastLabController::PassShell()
{
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		Sim->SpawnMover(EBlastMover::Shell);
	}
}

template <int32 Level>
void ABlastLabController::Quality()
{
	SBlastLabPanel::ApplyQuality(static_cast<EBlastQuality>(Level));
}

// ---- teste automático -------------------------------------------------------------
void ABlastLabController::TickAutoTest(float RealDt)
{
	if (AutoFolder.IsEmpty() || bAutoDone)
	{
		return;
	}
	AutoClock += RealDt;
	// a exposição que a Unreal está usando e o que o medidor dela leu (a cada
	// ~0.1 s): a curva da exposição ao longo da explosão, pra medir em vez de chutar
	static double LastExposureLog = -1.0;
	if (FPlatformTime::Seconds() - LastExposureLog > 0.1)
	{
		LastExposureLog = FPlatformTime::Seconds();
		if (ULocalPlayer* LP = GetLocalPlayer(); LP && LP->ViewportClient && LP->ViewportClient->Viewport)
		{
			FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(LP->ViewportClient->Viewport,
				GetWorld()->Scene, LP->ViewportClient->EngineShowFlags).SetRealtimeUpdate(true));
			FVector Loc;
			FRotator Rot;
			if (const FSceneView* View = LP->CalcSceneView(&Family, Loc, Rot, LP->ViewportClient->Viewport))
			{
				const float Exposure = View->GetLastEyeAdaptationExposure();
				const float Ev100 = Exposure > 0.0f ? -FMath::Log2(Exposure * 1.2f) : 0.0f;
				const float T = AutoExplosion ? AutoExplosion->Age : AutoClock - 3.0f;
				UE_LOG(LogTemp, Display, TEXT("AUTOTEST exposure t=%.2f EV100=%.2f avgLum=%.4g"), T, Ev100, View->GetLastAverageSceneLuminance());
			}
		}
	}
	if (!bAutoDetonated)
	{
		// Sem os assets pré-compilados (UnrealEditor -game), a Unreal desenha
		// com materiais provisórios enquanto compila shaders ("Preparing
		// Shaders"): o céu e a névoa saíam marrons. O teste só conta o relógio
		// depois que o simulador termina de preparar — e a exposição assenta
		// na cena antes do clarão (os renders do Movie Render Queue fazem o mesmo).
		const UBlastLabSimulation* SimNow = UBlastLabSimulation::Get(this);
		if (!SimNow || SimNow->IsPreparing())
		{
			AutoClock = 0.0f;
			return;
		}
		if (bAutoWatch && AutoClock > 2.0f && !bReadyLogged)
		{
			bReadyLogged = true;
			UE_LOG(LogTemp, Display, TEXT("AUTOTEST ready for click"));
		}
		if (AutoClock > 2.7f && !bEnvLogged)
		{
			bEnvLogged = true;
			if (const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
			{
				UE_LOG(LogTemp, Display, TEXT("AUTOTEST environment %.2fh sun %.1f deg EV %.2f purkinje %.4g stars %.4g sky multiscattering %.2f"), Env->TimeOfDay, Env->SunAltitudeDeg(), Env->TargetEV100(), Env->LastPurkinje, Env->LastStarGain, Env->LastMultiScattering);
			}
		}
		if (AutoClock > 0.5f && AutoClock < 2.7f)
		{
			AutoBaseTimes.Add(RealDt * 1000.0f);     // linha de base: a cena sem explosão
		}
		if (AutoClock > 2.8f && AutoNext == 0 && !bPreShot)
		{
			bPreShot = true;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(AutoFolder, TEXT("sim_-0.20.png")), true, false);
		}
		if (AutoClock > 3.0f && !bAutoWatch)
		{
			if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
			{
				Sim->SelectedWeapon = AutoWeapon;
			}
			// -AutoAim=x,y (fração da tela): detona pelo caminho do clique, o raio da
			// tela até o terreno (testa o clique no relevo); sem ele, no pé da origem
			FVector Ground(0.0, 0.0, BlastLabGround::HeightAt(GetWorld(), 0.0, 0.0));
			FString Aim;
			if (FParse::Value(FCommandLine::Get(), TEXT("AutoAim="), Aim, false))
			{
				FString AX, AY;
				int32 W, H;
				GetViewportSize(W, H);
				if (Aim.Split(TEXT(","), &AX, &AY) && !GroundUnderScreenPoint(FVector2D(W * FCString::Atof(*AX), H * FCString::Atof(*AY)), Ground))
				{
					UE_LOG(LogTemp, Display, TEXT("AUTOTEST aim %s missed the ground"), *Aim);
				}
				UE_LOG(LogTemp, Display, TEXT("AUTOTEST aim %s ground %s"), *Aim, *Ground.ToString());
			}
			// -AutoSeed=<n>: o mesmo sorteio (gravação, tamanho, semente do ruído) em
			// todas as rodadas: antes/depois com a mesma explosão
			int32 AutoSeed = 0;
			if (FParse::Value(FCommandLine::Get(), TEXT("AutoSeed="), AutoSeed))
			{
				FMath::RandInit(AutoSeed);
				FMath::SRandInit(AutoSeed);
			}
			AutoExplosion = DetonateAt(Ground);
			// onde ela cai na tela e quantos pixels valem 10 m ali (Tools/strip.py
			// recorta pela mesma largura em metros da arte, sem escolher no olho)
			if (AutoExplosion)
			{
				const FVector Mid = Ground + FVector(0.0, 0.0, 500.0 * AutoExplosion->SizeScale);
				const FVector Right = PlayerCameraManager ? FRotationMatrix(PlayerCameraManager->GetCameraRotation()).GetScaledAxis(EAxis::Y) : FVector::RightVector;
				FVector2D A, B;
				if (ProjectWorldLocationToScreen(Mid, A) && ProjectWorldLocationToScreen(Mid + Right * 1000.0, B))
				{
					UE_LOG(LogTemp, Display, TEXT("AUTOTEST screen %.1f %.1f px10m %.1f"), A.X, A.Y, (B - A).Size());
				}
			}
			bAutoDetonated = AutoExplosion != nullptr;
			UE_LOG(LogTemp, Display, TEXT("AUTOTEST detonated weapon %d scale %.2f"), AutoWeapon, AutoExplosion ? AutoExplosion->SizeScale : 0.0f);
			// -AutoMulti=<n> -AutoSpread=front|behind|far: mais explosões junto, pra
			// medir o custo das que estão na tela, atrás da câmera ou longe
			int32 Multi = 1;
			FParse::Value(FCommandLine::Get(), TEXT("AutoMulti="), Multi);
			FString Spread = TEXT("front");
			FParse::Value(FCommandLine::Get(), TEXT("AutoSpread="), Spread);
			const ABlastLabCamera* C = Cam();
			UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
			if (C && Sim && Multi > 1)
			{
				const FVector CamGround(C->GetActorLocation().X, C->GetActorLocation().Y, 0.0f);
				const FVector Back = CamGround.GetSafeNormal2D();          // do alvo (origem) pra câmera
				const FVector Side(-Back.Y, Back.X, 0.0f);
				for (int32 I = 1; I < Multi; ++I)
				{
					const float Sign = (I % 2) ? 1.0f : -1.0f;
					FVector P;
					if (Spread == TEXT("behind"))
					{
						P = CamGround + Back * (3000.0f + 1500.0f * I) + Side * (Sign * 1500.0f);
					}
					else if (Spread == TEXT("far"))
					{
						P = -Back * (12000.0f + 4000.0f * I) + Side * (Sign * 3000.0f * I);
					}
					else
					{
						const float A = 2.0f * PI * I / (Multi - 1);
						P = FVector(FMath::Cos(A), FMath::Sin(A), 0.0f) * 2200.0f;
					}
					P.Z = BlastLabGround::HeightAt(GetWorld(), P.X, P.Y);
					Sim->Detonate(P, AutoWeapon);
				}
				UE_LOG(LogTemp, Display, TEXT("AUTOTEST multi %d spread %s"), Multi, *Spread);
			}
			// -AutoProfile (com -csvGpuStats): tempos de GPU por passe durante a explosão (Tools/gpu_profile.py --live)
			if (FParse::Param(FCommandLine::Get(), TEXT("AutoProfile")))
			{
				ConsoleCommand(TEXT("csvprofile start"));
			}
			if (!AutoExplosion)
			{
				FGenericPlatformMisc::RequestExit(false);
			}
		}
		if (bAutoWatch && AutoClock > 120.0f)
		{
			UE_LOG(LogTemp, Display, TEXT("AUTOTEST no click in 120 s"));
			FGenericPlatformMisc::RequestExit(false);
		}
		return;
	}
	// instantes na escala da gravação; depois que ela acaba (e sai de cena) o relógio segue sozinho
	if (IsValid(AutoExplosion))
	{
		AutoScale = FMath::Max(AutoExplosion->SizeScale, 0.01f);
		AutoT = AutoExplosion->Age / AutoScale;
	}
	else
	{
		const UBlastLabSimulation* SimNow = UBlastLabSimulation::Get(this);
		AutoT += (SimNow ? SimNow->SimDelta(RealDt) : RealDt) / AutoScale;
	}
	const float T = AutoT;
	// -AutoPass=plane:5,missile:7,shell:9: as passagens pela fumaça em instantes da gravação
	if (AutoPasses.IsEmpty() && !bAutoPassesParsed)
	{
		bAutoPassesParsed = true;
		// -AutoGpuDump=1.5,7: a árvore de tempos de GPU de um quadro (profilegpu,
		// no log) nesses instantes: o custo de cada volume e de cada passe dele
		FString Dumps;
		if (FParse::Value(FCommandLine::Get(), TEXT("AutoGpuDump="), Dumps, false))
		{
			TArray<FString> P;
			Dumps.ParseIntoArray(P, TEXT(","));
			for (const FString& S : P)
			{
				AutoGpuDumps.Add(FCString::Atof(*S));
			}
		}
		FString Passes;
		if (FParse::Value(FCommandLine::Get(), TEXT("AutoPass="), Passes, false))
		{
			TArray<FString> P;
			Passes.ParseIntoArray(P, TEXT(","));
			for (const FString& S : P)
			{
				FString Kind, At;
				if (S.Split(TEXT(":"), &Kind, &At))
				{
					AutoPasses.Add({ Kind == TEXT("plane") ? 0 : Kind == TEXT("missile") ? 1 : Kind == TEXT("bomber") ? 3 : 2, FCString::Atof(*At) });
				}
			}
		}
	}
	for (int32 i = AutoPasses.Num() - 1; i >= 0; --i)
	{
		if (T >= AutoPasses[i].Value)
		{
			const int32 Kind = AutoPasses[i].Key;
			Kind == 0 ? PassPlane() : Kind == 1 ? PassMissile() : Kind == 3 ? PassBomber() : PassShell();
			UE_LOG(LogTemp, Display, TEXT("AUTOTEST pass %d at t=%.2f"), Kind, T);
			AutoPasses.RemoveAt(i);
		}
	}
	if (AutoGpuDumps.Num() && T >= AutoGpuDumps[0])
	{
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST profilegpu at t=%.2f"), T);
		ConsoleCommand(TEXT("profilegpu"));
		// e a memória de vídeo: o pool de render targets e os maiores recursos
		ConsoleCommand(TEXT("r.DumpRenderTargetPoolMemory"));
		ConsoleCommand(TEXT("rhi.DumpResourceMemory 60"));
		AutoGpuDumps.RemoveAt(0);
	}
	if (T > 0.2f)
	{
		AutoSumDt += RealDt;
		++AutoFrames;
		AutoFrameTimes.Add(RealDt * 1000.0f);
	}
	if (RealDt > 0.05f && IsValid(AutoExplosion) && AutoExplosion->Bake)
	{
		int32 F0, F1;
		float A;
		AutoExplosion->Bake->FramesAt(T, F0, F1, A);
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST hitch %.0f ms at t=%.2f (frames %d-%d)"), RealDt * 1000.0f, T, F0, F1);
	}
	else if (RealDt > 0.05f)
	{
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST hitch %.0f ms at t=%.2f"), RealDt * 1000.0f, T);
	}
	// -AutoOrbit=<rad/s>: a câmera gira devagar em volta do alvo depois da
	// detonação (o acúmulo temporal das explosões com a câmera andando)
	static const float OrbitRate = [] { float V = 0.0f; FParse::Value(FCommandLine::Get(), TEXT("AutoOrbit="), V); return V; }();
	if (OrbitRate != 0.0f)
	{
		if (ABlastLabCamera* C = Cam())
		{
			C->Orbit(-OrbitRate * RealDt / 0.005f, 0.0f);
		}
	}
	// -AutoVideo=de,até: uma foto por quadro nesse intervalo (s da gravação),
	// vid_NNNN.png. Com -UseFixedTimeStep -FPS=30 o relógio anda 1/30 s por quadro
	// e a pausa de cada foto não pula tempo: o vídeo sai na velocidade certa.
	static const FVector2f Video = []
	{
		FString V, A, B;
		return FParse::Value(FCommandLine::Get(), TEXT("AutoVideo="), V, false) && V.Split(TEXT(","), &A, &B)
			? FVector2f(FCString::Atof(*A), FCString::Atof(*B)) : FVector2f(1.0f, -1.0f);
	}();
	static int32 VideoFrame = 0;
	if (T >= Video.X && T <= Video.Y)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(AutoFolder, FString::Printf(TEXT("vid_%04d.png"), VideoFrame++)), true, false);
	}
	if (AutoTimes.IsValidIndex(AutoNext) && T >= AutoTimes[AutoNext])
	{
		const FString File = FPaths::Combine(AutoFolder, FString::Printf(TEXT("sim_%.2f.png"), AutoTimes[AutoNext]));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST shot %s (t = %.2f s)"), *File, T);
		++AutoNext;
	}
	if (!AutoTimes.IsValidIndex(AutoNext) && T >= AutoTimes.Last() + 0.5f)
	{
		const double Mean = AutoSumDt / FMath::Max(AutoFrames, 1);
		UE_LOG(LogTemp, Display, TEXT("AUTOTEST mean frame %.1f ms (%.1f fps) over %d frames"), 1000.0 * Mean, 1.0 / FMath::Max(Mean, 1e-6), AutoFrames);
		// mediana e p95: a média leva junto os quadros das fotos do teste (cada uma trava a leitura da tela)
		auto Stats = [](TArray<float> V, const TCHAR* What)
		{
			if (V.Num() < 4) return;
			V.Sort();
			const float Median = V[V.Num() / 2], P95 = V[FMath::Min(V.Num() - 1, int32(V.Num() * 0.95f))];
			UE_LOG(LogTemp, Display, TEXT("AUTOTEST %s frame median %.1f ms (%.1f fps) p95 %.1f ms"), What, Median, 1000.0f / Median, P95);
		};
		Stats(AutoBaseTimes, TEXT("before"));
		Stats(AutoFrameTimes, TEXT("explosion"));
		if (FParse::Param(FCommandLine::Get(), TEXT("AutoProfile")))
		{
			ConsoleCommand(TEXT("csvprofile stop"));
		}
		// o CSV é gravado numa thread: fecha um pouco depois
		FTimerHandle Exit;
		GetWorldTimerManager().SetTimer(Exit, [] { FGenericPlatformMisc::RequestExit(false); }, 2.0f, false);
		bAutoDone = true;
	}
}

ABlastLabGameMode::ABlastLabGameMode()
{
	DefaultPawnClass = ABlastLabCamera::StaticClass();
	PlayerControllerClass = ABlastLabController::StaticClass();
}
