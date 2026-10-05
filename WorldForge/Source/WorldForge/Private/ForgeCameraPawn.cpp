#include "ForgeCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "WorldForgeTerrain.h"

AForgeCameraPawn::AForgeCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	// Campo de visão estreito, como nos RTS (menos distorção de perspectiva:
	// as unidades têm o mesmo tamanho no alto e no pé da tela).
	Camera->FieldOfView = 50.0f;
	Camera->bConstrainAspectRatio = false;
}

float AForgeCameraPawn::GroundAt(FVector2f P) const
{
	if (!Terrain || !Terrain->GetMap())
	{
		// mapa feito no editor (a fatia): o chão é o Landscape, pela colisão
		FHitResult Hit;
		const FVector A(P.X * 100.0, P.Y * 100.0, 1e6), B(P.X * 100.0, P.Y * 100.0, -1e6);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ForgeGround), false, this);
		return GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Q) ? float(Hit.ImpactPoint.Z / 100.0) : 0.0f;
	}
	const FWorldMap& M = *Terrain->GetMap();
	return FMath::Max(M.HeightAt(P), M.WaterLevel);
}

void AForgeCameraPawn::FrameWholeMap(bool bInstant)
{
	const float Size = (Terrain && Terrain->GetMap()) ? Terrain->GetMap()->Settings.MapSize : 1024.0f;
	ResetAngles();        // +X pra cima na tela, como no minimapa
	// distância que cabe o mapa (inclinado) na vertical da tela
	LookAt(FVector2f(0.0f, -0.06f * Size), Size * 1.05f, bInstant);
	if (bInstant)
	{
		Yaw = YawTarget;
		Pitch = PitchTarget;
		ApplyTransform();
	}
}

void AForgeCameraPawn::ResetAngles()
{
	// volta pro "norte" mais perto (não desenrola as voltas já dadas)
	YawTarget = FMath::RoundToFloat(Yaw / 360.0f) * 360.0f;
	PitchTarget = DefaultPitch;
}

void AForgeCameraPawn::LookAt(FVector2f PointM, float DistanceM, bool bInstant)
{
	FocusTarget = PointM;
	DistanceTarget = FMath::Clamp(DistanceM, MinDistance, MaxDistance);
	if (bInstant)
	{
		Focus = FocusTarget;
		Distance = DistanceTarget;
		FocusZ = GroundAt(Focus);
		ApplyTransform();
	}
}

void AForgeCameraPawn::SetView(FVector2f FocusM, float YawDeg, float PitchDeg, float DistanceM)
{
	Yaw = YawTarget = YawDeg;
	Pitch = PitchTarget = PitchDeg;
	LookAt(FocusM, DistanceM, true);
}

void AForgeCameraPawn::ApplyTransform()
{
	const FRotator Rot(-Pitch, Yaw, 0.0f);
	const FVector Fwd = Rot.Vector();
	const FVector FocusCm(Focus.X * 100.0, Focus.Y * 100.0, FocusZ * 100.0);
	FVector Loc = FocusCm - Fwd * (Distance * 100.0);
	// nunca abaixo do relevo (montanhas entre a câmera e o ponto olhado)
	const float Ground = GroundAt(FVector2f(Loc.X / 100.0, Loc.Y / 100.0));
	Loc.Z = FMath::Max(Loc.Z, (Ground + 8.0f) * 100.0);
	SetActorLocationAndRotation(Loc, Rot);
}

bool AForgeCameraPawn::ScreenRay(FVector2D Pixel, FVector& Origin, FVector& Dir) const
{
	// Raio calculado da transformação atual da câmera (a deprojeção do
	// controlador usa a vista do quadro anterior e o "arrastar o chão" escorrega).
	if (!GEngine || !GEngine->GameViewport)
	{
		return false;
	}
	FVector2D Size;
	GEngine->GameViewport->GetViewportSize(Size);
	if (Size.X < 1 || Size.Y < 1)
	{
		return false;
	}
	const float TanX = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
	const float TanY = TanX * Size.Y / Size.X;
	const float NX = 2.0f * Pixel.X / Size.X - 1.0f;
	const float NY = 1.0f - 2.0f * Pixel.Y / Size.Y;
	const FRotator Rot = GetActorRotation();
	const FRotationMatrix R(Rot);
	Dir = (R.GetUnitAxis(EAxis::X) + R.GetUnitAxis(EAxis::Y) * (NX * TanX) + R.GetUnitAxis(EAxis::Z) * (NY * TanY)).GetSafeNormal();
	Origin = GetActorLocation() / 100.0;   // m
	return true;
}

bool AForgeCameraPawn::CursorGroundPoint(FVector& OutPointM) const
{
	const APlayerController* PC = GetController<APlayerController>();
	float MX, MY;
	FVector O, D;
	if (!PC || !PC->GetMousePosition(MX, MY) || !ScreenRay(FVector2D(MX, MY), O, D))
	{
		return false;
	}
	if (Terrain && Terrain->GetMap())
	{
		return Terrain->RayHit(O, D, OutPointM);
	}
	// sem terreno: o plano z = 0
	if (D.Z > -1e-4)
	{
		return false;
	}
	OutPointM = O + D * (-O.Z / D.Z);
	return true;
}

void AForgeCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetController<APlayerController>();
	if (!PC)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const bool bOverUI = FSlateApplication::IsInitialized() && IsOverUI && IsOverUI();
	const bool bFast = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	// digitando a semente: as teclas são do campo de texto
	bool bTyping = false;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		bTyping = Focused.IsValid() && Focused->GetType() == FName(TEXT("SEditableText"));
	}

	// ---- teclado e borda da tela: andar no plano, na direção da câmera ----
	FVector2f Move(0.0f, 0.0f);
	if (!bTyping)
	{
		if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up)) Move.X += 1.0f;
		if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) Move.X -= 1.0f;
		if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Move.Y += 1.0f;
		if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) Move.Y -= 1.0f;
	}
	float MX = 0.0f, MY = 0.0f;
	const bool bMouse = PC->GetMousePosition(MX, MY);
	FVector2D ViewSize(0, 0);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewSize);
	}
	if (bMouse && !bScrolling && !bRotating && ViewSize.X > 0 && EdgePan)
	{
		const float Edge = 3.0f;
		if (MX <= Edge) Move.Y -= 1.0f;
		if (MX >= ViewSize.X - 1 - Edge) Move.Y += 1.0f;
		if (MY <= Edge) Move.X += 1.0f;
		if (MY >= ViewSize.Y - 1 - Edge) Move.X -= 1.0f;
	}
	if (!Move.IsNearlyZero())
	{
		Move.Normalize();
		const float Speed = Distance * 0.9f * (bFast ? 2.5f : 1.0f);   // m/s: proporcional à altura
		const float YawRad = FMath::DegreesToRadians(Yaw);
		const FVector2f Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
		const FVector2f Right(-Fwd.Y, Fwd.X);
		FocusTarget += (Fwd * Move.X + Right * Move.Y) * Speed * Dt;
	}

	// ---- rotação: Q / E e botão do meio ----
	if (!bTyping && PC->IsInputKeyDown(EKeys::Q)) YawTarget += 90.0f * Dt;
	if (!bTyping && PC->IsInputKeyDown(EKeys::E)) YawTarget -= 90.0f * Dt;
	const double Now = FPlatformTime::Seconds();
	if (PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton) && !bOverUI)
	{
		bRotating = true;
		LastMouse = FVector2D(MX, MY);
		MiddlePressTime = Now;
		MiddlePressPos = LastMouse;
	}
	// dois cliques na roda (sem arrastar): ângulo e rotação voltam ao início
	if (PC->WasInputKeyJustReleased(EKeys::MiddleMouseButton))
	{
		const bool bClick = Now - MiddlePressTime < 0.35 && FVector2D::Distance(FVector2D(MX, MY), MiddlePressPos) < 6.0;
		if (bClick && Now - LastMiddleClick < 0.45)
		{
			ResetAngles();
			LastMiddleClick = -1.0;
		}
		else if (bClick)
		{
			LastMiddleClick = Now;
		}
	}
	if (bRotating)
	{
		if (!PC->IsInputKeyDown(EKeys::MiddleMouseButton))
		{
			bRotating = false;
		}
		else if (bMouse)
		{
			const FVector2D Delta = FVector2D(MX, MY) - LastMouse;
			YawTarget += Delta.X * 0.3f;
			// até 0°: olhando na horizontal
			PitchTarget = FMath::Clamp(PitchTarget + Delta.Y * 0.2f, MinPitch, MaxPitch);
			LastMouse = FVector2D(MX, MY);
		}
	}

	// ---- zoom: roda do mouse, em passos proporcionais ----
	if (!bOverUI)
	{
		// o eixo da roda soma os "dentes" do quadro (girar rápido manda vários
		// num quadro só; o "acabou de apertar" contava um)
		const float Notches = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
		if (Notches != 0.0f) DistanceTarget *= FMath::Pow(0.82f, Notches);
	}
	DistanceTarget = FMath::Clamp(DistanceTarget, MinDistance, MaxDistance);
	if (PC->WasInputKeyJustPressed(EKeys::Home))
	{
		FrameWholeMap(false);
	}

	// limites: o ponto olhado fica dentro do mapa
	if (Terrain && Terrain->GetMap())
	{
		const float H = Terrain->GetMap()->Half();
		FocusTarget.X = FMath::Clamp(FocusTarget.X, -H, H);
		FocusTarget.Y = FMath::Clamp(FocusTarget.Y, -H, H);
	}

	// ---- suavização (exponencial, independente da taxa de quadros) ----
	const float K = 1.0f - FMath::Exp(-12.0f * Dt);
	Focus += (FocusTarget - Focus) * K;
	Yaw += (YawTarget - Yaw) * K;
	Pitch += (PitchTarget - Pitch) * K;
	Distance += (DistanceTarget - Distance) * K;
	// a altura do ponto olhado acompanha o chão devagar (sem solavanco em penhasco)
	const float Ground = GroundAt(Focus);
	FocusZ += (Ground - FocusZ) * (1.0f - FMath::Exp(-4.0f * Dt));

	// ---- rolagem do C&C Generals / Zero Hour (botão direito arrastado) ----
	// A âncora é onde o botão desceu; a câmera anda na direção do cursor, mais
	// rápido quanto mais longe dela, e a âncora é puxada junto quando o cursor
	// passa de meia tela. Porte do WebGL aprovado (reference/webgl/js/main.js,
	// camera.js scrollRMB), os mesmos números do BlastLab (BlastLabCamera.cpp).
	if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton) && !bOverUI && bMouse)
	{
		bScrolling = true;
		ScrollAnchor = FVector2D(MX, MY);
	}
	if (bScrolling && !PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		bScrolling = false;
	}
	if (bScrolling && bMouse && ViewSize.Y > 0)
	{
		constexpr float RmbDead = 0.02f, RmbMin = 0.15f, RmbGain = 2.0f;   // zona morta, mínimo e ganho (telas/s)
		ScrollAnchor.X = FMath::Clamp(ScrollAnchor.X, MX - ViewSize.X * 0.5, MX + ViewSize.X * 0.5);
		ScrollAnchor.Y = FMath::Clamp(ScrollAnchor.Y, MY - ViewSize.Y * 0.5, MY + ViewSize.Y * 0.5);
		const float HalfH = FMath::Max(float(ViewSize.Y) * 0.5f, 1.0f);
		const float Ox = (MX - ScrollAnchor.X) / HalfH, Oy = (MY - ScrollAnchor.Y) / HalfH;
		const float R = FMath::Sqrt(Ox * Ox + Oy * Oy);
		if (R >= RmbDead)
		{
			// altura de chão enquadrada (campo vertical): 2·distância·tan(fov/2)
			const float TanV = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f)) * float(ViewSize.Y / ViewSize.X);
			const float View = 2.0f * Distance * TanV;
			const float Step = (RmbMin + RmbGain * (R - RmbDead)) * View * Dt / R;
			const float YawRad = FMath::DegreesToRadians(Yaw);
			const FVector2f Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
			const FVector2f Right(-Fwd.Y, Fwd.X);
			// cursor pra cima da âncora → anda pra frente; pra direita → pra direita
			FocusTarget += (Right * Ox - Fwd * Oy) * Step;
		}
	}
	ApplyTransform();
}
