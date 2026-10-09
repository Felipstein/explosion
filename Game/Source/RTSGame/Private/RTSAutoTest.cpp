#include "RTSAutoTest.h"

#include "RTSFogOfWar.h"
#include "RTSHUD.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTSCameraPawn.h"
#include "RTSHUD.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

// Durante o teste só a entrada sintética chega no jogo. O mouse e o teclado de verdade (o
// Felipe usando o PC enquanto a janela do teste abre na frente) entravam junto: um clique que
// escorregou virou caixa, um botão direito desfez a seleção (t8/t9, 08/10).
class FRTSTestInputGate : public IInputProcessor
{
public:
	bool bInjecting = false;

	virtual void Tick(const float DeltaTime, FSlateApplication& App, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent& E) override { return !bInjecting; }
	virtual bool HandleKeyUpEvent(FSlateApplication& App, const FKeyEvent& E) override { return !bInjecting; }
	virtual bool HandleMouseMoveEvent(FSlateApplication& App, const FPointerEvent& E) override { return !bInjecting; }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& E) override { return !bInjecting; }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& App, const FPointerEvent& E) override { return !bInjecting; }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App, const FPointerEvent& E) override { return !bInjecting; }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& App, const FPointerEvent& E, const FPointerEvent* Gesture) override { return !bInjecting; }
	virtual const TCHAR* GetDebugName() const override { return TEXT("RTSTestInputGate"); }
};

bool URTSAutoTest::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Script;
	return FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), Script);
}

void URTSAutoTest::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("-RTSCapture="), Folder);
	if (Folder.IsEmpty())
	{
		Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AutoTest"));
	}
	bRecord = FParse::Param(FCommandLine::Get(), TEXT("RTSRecord"));
	Build();
	if (FSlateApplication::IsInitialized())
	{
		Gate = MakeShared<FRTSTestInputGate>();
		FSlateApplication::Get().RegisterInputPreProcessor(Gate, 0);
	}
	bActive = true;
	UE_LOG(LogTemp, Display, TEXT("RTS TEST começou: %d passos, fotos em %s"), Steps.Num(), *Folder);
}

void URTSAutoTest::Deinitialize()
{
	if (Gate.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Gate);
	}
	Gate.Reset();
	Super::Deinitialize();
}

TStatId URTSAutoTest::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSAutoTest, STATGROUP_Tickables);
}

ARTSPlayerController* URTSAutoTest::PC() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? Cast<ARTSPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

URTSMatchSubsystem* URTSAutoTest::Match() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	URTSMatchSubsystem* M = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	return M && M->IsRunning() ? M : nullptr;
}

FVector2D URTSAutoTest::ToSlate(const FVector2D& Pixel) const
{
	const TSharedPtr<SViewport> V = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
	if (!V.IsValid() || !GEngine->GameViewport->Viewport)
	{
		return Pixel;
	}
	const FGeometry& G = V->GetTickSpaceGeometry();
	const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
	const FVector2D Frac(Pixel.X / FMath::Max(1, Size.X), Pixel.Y / FMath::Max(1, Size.Y));
	return FVector2D(G.LocalToAbsolute(FVector2D(G.GetLocalSize()) * Frac));
}

FVector2D URTSAutoTest::ViewportFrac(const FVector2D& Frac) const
{
	const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint(1600, 900);
	return FVector2D(Frac.X * Size.X, Frac.Y * Size.Y);
}

FVector2D URTSAutoTest::ButtonCenter(FName Id) const
{
	ARTSPlayerController* P = PC();
	const ARTSHUD* HUD = P ? Cast<ARTSHUD>(P->GetHUD()) : nullptr;
	FBox2D Box;
	if (HUD && HUD->FindButton(Id, Box))
	{
		return Box.GetCenter();
	}
	return FVector2D(-1, -1);
}

void URTSAutoTest::QueueClick(const FVector2D& Pixel, FKey Button)
{
	Pending.Add({ FInputEvent::EType::Move, Pixel, EKeys::Invalid });
	Pending.Add({ FInputEvent::EType::Down, Pixel, Button });
	Pending.Add({ FInputEvent::EType::Up, Pixel, Button });
}

void URTSAutoTest::QueueDrag(const FVector2D& From, const FVector2D& To)
{
	Pending.Add({ FInputEvent::EType::Move, From, EKeys::Invalid });
	Pending.Add({ FInputEvent::EType::Down, From, EKeys::LeftMouseButton });
	for (int32 K = 1; K <= 6; ++K)
	{
		Pending.Add({ FInputEvent::EType::Move, FMath::Lerp(From, To, K / 6.0), EKeys::Invalid });
	}
	Pending.Add({ FInputEvent::EType::Up, To, EKeys::LeftMouseButton });
}

void URTSAutoTest::QueueKey(FKey K)
{
	Pending.Add({ FInputEvent::EType::KeyDown, FVector2D::ZeroVector, K });
	Pending.Add({ FInputEvent::EType::KeyUp, FVector2D::ZeroVector, K });
}

void URTSAutoTest::PumpInput()
{
	if (Pending.IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const FInputEvent E = Pending[0];
	Pending.RemoveAt(0);
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Pos = ToSlate(E.Pos);
	TGuardValue<bool> Injecting(Gate.IsValid() ? Gate->bInjecting : bDummyInjecting, true);
	// o mouse de verdade em cima da janela atropelava os eventos sintéticos (a posição do
	// viewport vinha dele): leva o cursor junto, como uma mão faria
	if (E.Type == FInputEvent::EType::Move || E.Type == FInputEvent::EType::Down || E.Type == FInputEvent::EType::Up)
	{
		if (App.GetPlatformCursor().IsValid())
		{
			App.GetPlatformCursor()->SetPosition(FMath::RoundToInt(Pos.X), FMath::RoundToInt(Pos.Y));
		}
	}
	switch (E.Type)
	{
	case FInputEvent::EType::Move:
	{
		const FPointerEvent P(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastSlatePos, Pressed, EKeys::Invalid, 0.0f, FModifierKeysState());
		App.ProcessMouseMoveEvent(P);
		LastSlatePos = Pos;
		break;
	}
	case FInputEvent::EType::Down:
	{
		Pressed.Add(E.Key);
		const FPointerEvent P(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastSlatePos, Pressed, E.Key, 0.0f, FModifierKeysState());
		App.ProcessMouseButtonDownEvent(nullptr, P);
		LastSlatePos = Pos;
		break;
	}
	case FInputEvent::EType::Up:
	{
		Pressed.Remove(E.Key);
		const FPointerEvent P(App.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, Pos, LastSlatePos, Pressed, E.Key, 0.0f, FModifierKeysState());
		App.ProcessMouseButtonUpEvent(P);
		LastSlatePos = Pos;
		break;
	}
	case FInputEvent::EType::KeyDown:
	case FInputEvent::EType::KeyUp:
	{
		const FKeyEvent K(E.Key, FModifierKeysState(), App.GetUserIndexForKeyboard(), false, 0, 0);
		if (E.Type == FInputEvent::EType::KeyDown) { App.ProcessKeyDownEvent(K); } else { App.ProcessKeyUpEvent(K); }
		break;
	}
	}
}

bool URTSAutoTest::OwnCentroid(FVector2D& OutCm) const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return false;
	}
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	FVector2D Sum = FVector2D::ZeroVector;
	int32 N = 0;
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner == M->LocalPlayer())
		{
			Sum += U.PosCm;
			++N;
		}
	}
	if (N == 0)
	{
		return false;
	}
	OutCm = Sum / N;
	return true;
}

bool URTSAutoTest::BattleFocus(FVector2D& OutCm) const
{
	// o meio da briga: seu exército e os inimigos a até 200 m dele (senão, só o seu)
	URTSMatchSubsystem* M = Match();
	FVector2D Own;
	if (!M || !OwnCentroid(Own))
	{
		return false;
	}
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	FVector2D Sum = FVector2D::ZeroVector;
	int32 N = 0;
	for (const FRTSUnitState& U : Units)
	{
		if (FVector2D::Distance(U.PosCm, Own) < 20000.0 && M->IsUnitShown(U.Id))
		{
			Sum += U.PosCm;
			++N;
		}
	}
	OutCm = N > 0 ? Sum / N : Own;
	return true;
}

void URTSAutoTest::Shot(const FString& Name)
{
	if (bRecord && !bRecordDone && RecordFrames > 0)
	{
		return;   // gravando: o pedido de foto é um só por quadro, e o do vídeo vem primeiro
	}
	const FString File = FPaths::Combine(Folder, FString::Printf(TEXT("%02d_%s.png"), Index, *Name));
	FScreenshotRequest::RequestScreenshot(File, true, false);
}

void URTSAutoTest::Build()
{
	auto IsMenu = [this]() { return PC() && PC()->IsMenu() && PC()->GetHUD(); };

	Steps.Add({ TEXT("menu"), nullptr, IsMenu, nullptr, 6.0f, 90.0f });

	Steps.Add({ TEXT("jogar_skirmish"),
		[this]() { QueueClick(ButtonCenter(TEXT("skirmish"))); },
		[this]() { return Match() != nullptr && Match()->IsReady() && PC() && !PC()->IsMenu(); },
		[this]() { return Match() ? FString() : TEXT("a partida não começou"); },
		5.0f, 120.0f });

	Steps.Add({ TEXT("caixa_de_selecao"),
		[this]()
		{
			// a caixa em volta de todos os seus tanques, como o jogador faria
			URTSMatchSubsystem* M = Match();
			ARTSPlayerController* P = PC();
			if (!M || !P)
			{
				return;
			}
			const FRTSBakedMap* Map = M->BakedMap();
			TArray<FRTSUnitState> Units;
			M->GetUnits(Units);
			FBox2D Box(ForceInit);
			for (const FRTSUnitState& U : Units)
			{
				FVector2D S;
				if (U.Owner == M->LocalPlayer() && P->ProjectWorldLocationToScreen(FVector(U.PosCm.X, U.PosCm.Y, Map->GroundZCm(U.PosCm) + 150.0f), S))
				{
					Box += S;
				}
			}
			UE_LOG(LogTemp, Log, TEXT("RTS TEST caixa: tanques na tela de (%.0f, %.0f) a (%.0f, %.0f), válida %d"), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, int32(Box.bIsValid));
			if (Box.bIsValid)
			{
				QueueDrag(Box.Min - FVector2D(60, 60), Box.Max + FVector2D(60, 60));
			}
		},
		[]() { return true; },
		[this]()
		{
			URTSMatchSubsystem* M = Match();
			TArray<FRTSUnitState> Units;
			if (M) { M->GetUnits(Units); }
			int32 Own = 0;
			for (const FRTSUnitState& U : Units) { Own += U.Owner == (M ? M->LocalPlayer() : 0) ? 1 : 0; }
			const int32 Sel = PC() ? PC()->GetSelection().Num() : 0;
			return Sel == Own && Own > 0 ? FString() : FString::Printf(TEXT("selecionou %d de %d"), Sel, Own);
		},
		1.0f });

	Steps.Add({ TEXT("ordem_andar"),
		[this]()
		{
			OwnCentroid(CentroidBefore);
			QueueClick(ViewportFrac(FVector2D(0.55, 0.32)));
		},
		[]() { return true; },
		[this]()
		{
			FVector2D Now;
			const double Moved = OwnCentroid(Now) ? FVector2D::Distance(Now, CentroidBefore) / 100.0 : 0.0;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST o grupo andou %.1f m em 4 s"), Moved);
			return Moved > 5.0 ? FString() : FString::Printf(TEXT("o grupo andou só %.1f m"), Moved);
		},
		4.0f });

	Steps.Add({ TEXT("ataque_andando"),
		[this]()
		{
			QueueKey(EKeys::A);
			QueueClick(ViewportFrac(FVector2D(0.5, 0.25)));
		},
		[]() { return true; },
		[this]() { return PC() && !PC()->IsAttackMoveArmed() ? FString() : TEXT("o ataque andando ficou armado"); },
		2.0f });

	// a névoa e o minimapa (docs/BASE_JOGAVEL.md §0), clicando como o Felipe clica
	auto Hud = [this]() { return PC() ? Cast<ARTSHUD>(PC()->GetHUD()) : nullptr; };
	Steps.Add({ TEXT("nevoa"), nullptr,
		[this]() { return Match() && Match()->GetFog() && Match()->GetFog()->IsMinimapReady(); },
		[this]() { return Match() && Match()->IsFogShown() ? FString() : TEXT("a névoa não está ligada"); },
		1.0f, 20.0f });

	Steps.Add({ TEXT("nevoa_desligada"),
		[this]() { QueueKey(EKeys::F9); },
		[this]() { return Match() && !Match()->IsFogOn(); },
		[this]() { return Match() && !Match()->IsFogShown() ? FString() : TEXT("o F9 não desligou a névoa"); },
		1.6f, 10.0f });

	Steps.Add({ TEXT("nevoa_ligada"),
		[this]() { QueueKey(EKeys::F9); },
		[this]() { return Match() && Match()->IsFogOn(); },
		[this]() { return Match() && Match()->IsFogShown() ? FString() : TEXT("o F9 não religou a névoa"); },
		1.6f, 10.0f });

	Steps.Add({ TEXT("minimapa_camera"),
		[this, Hud]()
		{
			// botão direito no minimapa, na largada do inimigo: a câmera vai lá e a seleção fica
			SelectionBefore = PC() ? PC()->GetSelection().Num() : 0;
			MinimapTarget = Match() ? Match()->PlayerStartCm(1) : FVector2D::ZeroVector;
			FVector2D Px;
			if (Hud() && Hud()->WorldToMinimapScreen(MinimapTarget, Px))
			{
				QueueClick(Px, EKeys::RightMouseButton);
			}
		},
		[this]()
		{
			ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
			return Cam && FVector2D::Distance(Cam->GetFocusCm(), MinimapTarget) < 4000.0;
		},
		[this]()
		{
			ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
			const double Off = Cam ? FVector2D::Distance(Cam->GetFocusCm(), MinimapTarget) / 100.0 : 1e9;
			const int32 Sel = PC() ? PC()->GetSelection().Num() : 0;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST minimapa: câmera a %.1f m do ponto, seleção %d de %d"), Off, Sel, SelectionBefore);
			return Sel == SelectionBefore ? FString() : FString::Printf(TEXT("o clique direito no minimapa mudou a seleção (%d → %d)"), SelectionBefore, Sel);
		},
		1.0f, 10.0f });

	Steps.Add({ TEXT("minimapa_ordem"),
		[this, Hud]()
		{
			// esquerdo no minimapa com o exército selecionado: ordem de andar até lá (80 m à frente)
			FVector2D Own;
			OwnCentroid(Own);
			const FVector2D Enemy = Match() ? Match()->PlayerStartCm(1) : Own;
			MinimapTarget = Own + (Enemy - Own).GetSafeNormal() * 8000.0;
			MinimapOrderOffCm = 1e12;
			FVector2D Px;
			if (Hud() && Hud()->WorldToMinimapScreen(MinimapTarget, Px))
			{
				QueueClick(Px);
			}
		},
		[this]()
		{
			// a marca da ordem no chão, perto do ponto (a resolução do minimapa é de ~5 m por pixel);
			// ela vive 0,7 s, então confere enquanto existe
			double Best = 1e12;
			if (PC())
			{
				for (const FRTSOrderMarker& Marker : PC()->GetMarkers())
				{
					Best = FMath::Min(Best, FVector2D::Distance(FVector2D(Marker.World.X, Marker.World.Y), MinimapTarget));
				}
			}
			if (Best < MinimapOrderOffCm)
			{
				MinimapOrderOffCm = Best;
			}
			return MinimapOrderOffCm < 1500.0;
		},
		[this]()
		{
			UE_LOG(LogTemp, Display, TEXT("RTS TEST minimapa: ordem a %.1f m do ponto clicado"), MinimapOrderOffCm / 100.0);
			return MinimapOrderOffCm < 1500.0 ? FString() : TEXT("o clique esquerdo no minimapa não deu ordem perto do ponto");
		},
		0.1f, 5.0f });

	Steps.Add({ TEXT("batalha"),
		[this]() { NextBattleShot = 0.0f; NextFollow = 0.0f; NextPush = 12.0f; BattleShots = 0; bStallLogged = false; },
		[this]()
		{
			// segue o seu exército com a câmera e fotografa de tempos em tempos
			URTSMatchSubsystem* M = Match();
			if (!M)
			{
				return true;
			}
			if (Timer >= NextFollow)
			{
				NextFollow = Timer + 0.5f;
				FVector2D C;
				ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
				if (Cam && BattleFocus(C))
				{
					Cam->GlideTo(C);
				}
			}
			// como um jogador faria: de tempos em tempos, ataque andando no inimigo mais perto que aparece
			if (Timer >= NextPush && PC() && !PC()->GetSelection().IsEmpty())
			{
				NextPush = Timer + 12.0f;
				const FRTSBakedMap* Map = M->BakedMap();
				TArray<FRTSUnitState> Units;
				M->GetUnits(Units);
				FVector2D Own;
				if (Map && OwnCentroid(Own))
				{
					const FRTSUnitState* Best = nullptr;
					for (const FRTSUnitState& U : Units)
					{
						if (M->IsEnemyOfLocal(U.Owner) && M->IsUnitShown(U.Id) && (!Best || FVector2D::Distance(U.PosCm, Own) < FVector2D::Distance(Best->PosCm, Own)))
						{
							Best = &U;
						}
					}
					FVector2D S;
					const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
					if (Best && PC()->ProjectWorldLocationToScreen(FVector(Best->PosCm.X, Best->PosCm.Y, Map->GroundZCm(Best->PosCm)), S)
						&& S.X > 20 && S.Y > 60 && S.X < Size.X - 20 && S.Y < Size.Y - 20)
					{
						QueueKey(EKeys::A);
						QueueClick(S + FVector2D(0.0, 40.0));   // no chão logo antes dele
					}
				}
			}
			if (!bStallLogged && M->GetShotsFired() > 0 && M->SecondsSinceStart() > 60.0f && GetGameInstance()->GetWorld()->GetTimeSeconds() - M->LastDeathSeconds() > 25.0)
			{
				bStallLogged = true;
				UE_LOG(LogTemp, Warning, TEXT("RTS TEST impasse: 25 s sem ninguém morrer"));
				M->LogUnits(TEXT("impasse"));
			}
			if (Timer >= NextBattleShot)
			{
				NextBattleShot = Timer + 8.0f;
				Shot(FString::Printf(TEXT("batalha_%02d"), BattleShots++));
				UE_LOG(LogTemp, Display, TEXT("RTS TEST %s"), *M->DebugLine());
			}
			return M->Result() != ERTSMatchResult::Playing;
		},
		[this]() { return Match() && Match()->Result() != ERTSMatchResult::Playing ? FString() : TEXT("a partida não terminou"); },
		1.0f, 420.0f, false });

	// o painel entra 3 s depois do golpe final e leva 0,6 s pra aparecer: espera ele inteiro (a
	// foto mostra o resultado e o próximo passo acha o botão; no t6 o clique saiu antes do botão)
	Steps.Add({ TEXT("resultado"), nullptr,
		[this]() { return Match() && Match()->SecondsSinceEnd() > 3.8f && ButtonCenter(TEXT("menu")).X >= 0.0; },
		[this]()
		{
			URTSMatchSubsystem* M = Match();
			const ERTSMatchResult R = M ? M->Result() : ERTSMatchResult::Playing;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST resultado: %s em %.0f s"),
				R == ERTSMatchResult::Victory ? TEXT("vitória") : (R == ERTSMatchResult::Defeat ? TEXT("derrota") : TEXT("empate/nenhum")), M ? M->MatchSeconds() : 0.0f);
			return FString();
		},
		2.0f });

	Steps.Add({ TEXT("voltar_ao_menu"),
		[this]() { QueueClick(ButtonCenter(TEXT("menu"))); },
		[this, IsMenu]() { return IsMenu() && Match() == nullptr; },
		[this, IsMenu]() { return IsMenu() ? FString() : TEXT("não voltou pro menu"); },
		4.0f, 120.0f });
}

void URTSAutoTest::Tick(float DeltaTime)
{
	if (!bActive)
	{
		return;
	}
	TotalTime += DeltaTime;
	Timer += DeltaTime;
	if (Match() && Index >= 2)
	{
		FrameMs.Add(DeltaTime * 1000.0f);
	}
	if (ARTSPlayerController* P = PC())
	{
		P->SetEdgeScroll(false);
	}
	PumpInput();
	if (bRecord && !bRecordDone)
	{
		URTSMatchSubsystem* M = Match();
		if (M && M->GetShotsFired() > 0 && RecordFrames < 2400 && !FScreenshotRequest::IsScreenshotRequested())
		{
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder, TEXT("frames"), FString::Printf(TEXT("f_%05d.png"), RecordFrames++)), true, false);
			if (M->Result() != ERTSMatchResult::Playing && (RecordTail += DeltaTime) > 4.0f)
			{
				bRecordDone = true;
			}
		}
	}
	if (!Pending.IsEmpty())
	{
		return;   // espera a entrada sintética terminar
	}
	if (Index >= Steps.Num())
	{
		Finish();
		return;
	}
	FStep& S = Steps[Index];
	if (Phase == 0)
	{
		if (S.Act)
		{
			S.Act();
		}
		Phase = 1;
		Timer = 0.0f;
		return;
	}
	if (Phase == 1)
	{
		const bool bReady = Timer >= S.MinWait && (!S.Ready || S.Ready());
		if (!bReady && Timer < S.Timeout)
		{
			return;
		}
		const FString Why = !bReady ? FString::Printf(TEXT("nada em %.0f s"), S.Timeout) : (S.Check ? S.Check() : FString());
		if (Why.IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("RTS TEST ok %s (%.1f s)"), *S.Name, Timer);
		}
		else
		{
			++Failures;
			UE_LOG(LogTemp, Warning, TEXT("RTS TEST FALHOU %s: %s"), *S.Name, *Why);
		}
		if (S.bShot)
		{
			Shot(S.Name);
		}
		Phase = 2;
		Timer = 0.0f;
		return;
	}
	if (Timer > 0.5f)
	{
		++Index;
		Phase = 0;
		Timer = 0.0f;
	}
}

void URTSAutoTest::Finish()
{
	bActive = false;
	FrameMs.Sort();
	const float Median = FrameMs.IsEmpty() ? 0.0f : FrameMs[FrameMs.Num() / 2];
	const float P95 = FrameMs.IsEmpty() ? 0.0f : FrameMs[FMath::Min(FrameMs.Num() - 1, FrameMs.Num() * 95 / 100)];
	int32 Hitches = 0;
	for (const float Ms : FrameMs)
	{
		Hitches += Ms > 50.0f ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, TEXT("RTS TEST quadros na partida: mediana %.1f ms, p95 %.1f ms, %d acima de 50 ms (de %d)"), Median, P95, Hitches, FrameMs.Num());
	UE_LOG(LogTemp, Display, TEXT("RTS TEST fim: %d falha(s), %.0f s"), Failures, TotalTime);
	FPlatformMisc::RequestExit(false);
}
