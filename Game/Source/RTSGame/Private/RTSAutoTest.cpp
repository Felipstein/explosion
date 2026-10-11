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
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTSCameraPawn.h"
#include "RTSHUD.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSNetSession.h"
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
	FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), ScriptName);
	bLanClient = ScriptName == TEXT("lanclient");
	bLan = ScriptName == TEXT("lanhost") || bLanClient;
	bBase = ScriptName == TEXT("base") || bLan;
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetPlayers="), NetPlayers);
	NetPlayers = FMath::Clamp(NetPlayers, 2, 8);
	bExpectFog = !FParse::Param(FCommandLine::Get(), TEXT("RTSTestNoFog"));
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetGate="), GateFile);
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetAI="), NetAIs);
	bTeams = FParse::Param(FCommandLine::Get(), TEXT("RTSTestTeams"));
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

URTSNetSession* URTSAutoTest::NetSession() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
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
		if (ARTSPlayerController* P = PC())
		{
			P->SetTestMouse(E.Pos);   // o jogo lê este, não o cursor do Windows
		}
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
		// com base, o centro é o do exército (prédio não anda, e o Dozer e o caminhão têm a vida deles)
		FRTSDefInfo Info;
		const bool bArmy = !U.bStructure && (!bBase || (M->GetDef(uint16(U.Def), Info) && Info.bArmed));
		if (U.Owner == M->LocalPlayer() && bArmy)
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

	if (bLan)
	{
		// a LAN pelos botões, como o Felipe faria: Multiplayer → Criar (ou entrar) → a sala → Começar
		Steps.Add({ TEXT("lan_menu"),
			[this]() { QueueClick(ButtonCenter(TEXT("multiplayer"))); },
			[this]() { return PC() && PC()->GetMenuPage() == ARTSPlayerController::EMenuPage::Lan; },
			nullptr, 0.5f, 10.0f });
	}
	if (bLan && !bLanClient)
	{
		Steps.Add({ TEXT("lan_criar"),
			[this]() { QueueClick(ButtonCenter(TEXT("lan_host"))); },
			[this]() { const URTSNetSession* N = NetSession(); return N && N->IsHost(); },
			[this]() { const URTSNetSession* N = NetSession(); return N && N->IsHost() ? FString() : TEXT("a sala não foi criada"); },
			0.5f, 10.0f });
		// eu de vermelho: o botão da cor na sala, até chegar no vermelho (antes dos outros entrarem)
		Steps.Add({ TEXT("lan_cor"),
			[this]() { RetryAt = 0.0f; ColorBefore = MyLobbyColor(); },
			[this]()
			{
				if (MyLobbyColor() == 1)
				{
					return true;
				}
				if (Timer >= RetryAt && Pending.IsEmpty())
				{
					RetryAt = Timer + 0.6f;
					QueueClick(ButtonCenter(MyColorButton()));
				}
				return false;
			},
			[this]()
			{
				UE_LOG(LogTemp, Display, TEXT("RTS TEST cor na sala: de %d pra %d"), ColorBefore, MyLobbyColor());
				return MyLobbyColor() == 1 ? FString() : TEXT("a cor não chegou no vermelho");
			},
			0.5f, 20.0f });
		// as IAs da sala (antes dos outros: ficam nas primeiras vagas, ao meu lado no 2 contra 2)
		if (NetAIs > 0)
		{
			Steps.Add({ TEXT("lan_ia"),
				[this]() { RetryAt = 0.0f; },
				[this]()
				{
					const URTSNetSession* N = NetSession();
					int32 Ais = 0;
					for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
					{
						Ais += M.bAI ? 1 : 0;
					}
					if (Ais >= NetAIs)
					{
						return true;
					}
					if (Timer >= RetryAt && Pending.IsEmpty())
					{
						RetryAt = Timer + 0.6f;
						QueueClick(ButtonCenter(TEXT("lan_add_ai")));
					}
					return false;
				},
				[this]()
				{
					const URTSNetSession* N = NetSession();
					int32 Ais = 0;
					for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
					{
						Ais += M.bAI ? 1 : 0;
					}
					return Ais == NetAIs ? FString() : FString::Printf(TEXT("%d IAs na sala, queria %d"), Ais, NetAIs);
				},
				0.5f, 15.0f });
		}
		// a névoa da sala: o botão passa por explorada → preta → desligada
		Steps.Add({ TEXT("lan_nevoa"),
			[this]() { RetryAt = 0.0f; },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				const ERTSFogMode Want = bExpectFog ? ERTSFogMode::Explored : ERTSFogMode::Off;
				if (N && N->GetFogMode() == Want)
				{
					return true;
				}
				if (Timer >= RetryAt && Pending.IsEmpty())
				{
					RetryAt = Timer + 0.6f;
					QueueClick(ButtonCenter(TEXT("lan_fog")));
				}
				return false;
			},
			[this]()
			{
				// a porta pros outros jogos do teste (a cor e as IAs já escolhidas)
				if (!GateFile.IsEmpty())
				{
					FFileHelper::SaveStringToFile(TEXT("ok"), *GateFile);
				}
				const URTSNetSession* N = NetSession();
				UE_LOG(LogTemp, Display, TEXT("RTS TEST névoa da sala: %d (0 desligada, 1 preta, 2 explorada)"), N ? int32(N->GetFogMode()) : -1);
				return N && N->GetFogMode() == (bExpectFog ? ERTSFogMode::Explored : ERTSFogMode::Off) ? FString() : TEXT("o botão não chegou na névoa pedida");
			},
			0.5f, 15.0f });
		Steps.Add({ TEXT("lan_espera"), nullptr,
			[this]() { const URTSNetSession* N = NetSession(); return N && N->PlayerCount() >= NetPlayers; },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
				{
					UE_LOG(LogTemp, Display, TEXT("RTS TEST na sala: vaga %d, %s, cor %d"), M.Slot, *M.Name, M.Color);
				}
				return N && N->PlayerCount() >= NetPlayers ? FString() : FString::Printf(TEXT("só %d de %d entraram"), N ? N->PlayerCount() : 0, NetPlayers);
			},
			1.0f, 240.0f });
		if (bTeams)
		{
			Steps.Add({ TEXT("lan_times"),
				[this]() { RetryAt = 0.0f; },
				[this]()
				{
					const URTSNetSession* N = NetSession();
					if (!N)
					{
						return false;
					}
					for (const FRTSNetMember& M : N->Members())
					{
						const uint8 Want = (M.Slot == 0 || M.bAI) ? 1 : 2;
						if (M.Team != Want)
						{
							if (Timer >= RetryAt && Pending.IsEmpty())
							{
								RetryAt = Timer + 0.4f;
								QueueClick(ButtonCenter(FName(*FString::Printf(TEXT("lan_team_%d"), M.Slot))));
							}
							return false;
						}
					}
					return true;
				},
				[this]()
				{
					const URTSNetSession* N = NetSession();
					for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
					{
						UE_LOG(LogTemp, Display, TEXT("RTS TEST time na sala: vaga %d, %s%s, time %d"), M.Slot, *M.Name, M.bAI ? TEXT(" (IA)") : TEXT(""), M.Team);
					}
					return N && N->CanStart() ? FString() : TEXT("a sala não deixa começar com esses times");
				},
				0.5f, 30.0f });
		}
		Steps.Add({ TEXT("lan_comecar"),
			[this]() { QueueClick(ButtonCenter(TEXT("lan_start"))); },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				return Match() && Match()->IsReady() && PC() && !PC()->IsMenu() && N && N->CanStartTicking();
			},
			[this]() { return Match() ? FString() : TEXT("a partida em rede não começou"); },
			3.0f, 300.0f });
	}
	if (bLanClient)
	{
		// a nossa tela não é o anfitrião: acha a sala na lista da rede e entra, como a família faria
		Steps.Add({ TEXT("lan_entrar"),
			[this]() { RetryAt = 0.0f; },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				if (N && N->GetRole() == ERTSNetRole::Client && N->LocalSlot() > 0 && N->Members().Num() > 0)
				{
					return true;
				}
				if (N && !N->IsActive() && Timer >= RetryAt && Pending.IsEmpty() && N->FoundGames().Num() > 0 && N->FoundGames()[0].bOpen)
				{
					RetryAt = Timer + 3.0f;
					QueueClick(ButtonCenter(TEXT("lan_join_0")));
				}
				return false;
			},
			[this]()
			{
				const URTSNetSession* N = NetSession();
				UE_LOG(LogTemp, Display, TEXT("RTS TEST entrei na sala: vaga %d, %d na sala, cor %d"), N ? N->LocalSlot() : -1, N ? N->PlayerCount() : 0, MyLobbyColor());
				return N && N->GetRole() == ERTSNetRole::Client && N->LocalSlot() > 0 ? FString() : TEXT("não entrou na sala do anfitrião");
			},
			0.5f, 180.0f });
		Steps.Add({ TEXT("lan_cor"),
			[this]() { RetryAt = 2.0f; ColorBefore = MyLobbyColor(); QueueClick(ButtonCenter(MyColorButton())); },
			[this]()
			{
				if (MyLobbyColor() != ColorBefore)
				{
					return true;
				}
				if (Timer >= RetryAt && Pending.IsEmpty())
				{
					RetryAt = Timer + 2.0f;
					QueueClick(ButtonCenter(MyColorButton()));
				}
				return false;
			},
			[this]()
			{
				UE_LOG(LogTemp, Display, TEXT("RTS TEST cor na sala (cliente): de %d pra %d"), ColorBefore, MyLobbyColor());
				return MyLobbyColor() != ColorBefore ? FString() : TEXT("o anfitrião não trocou a minha cor");
			},
			0.3f, 8.0f });
		Steps.Add({ TEXT("lan_espera_inicio"), nullptr,
			[this]()
			{
				const URTSNetSession* N = NetSession();
				return Match() && Match()->IsReady() && PC() && !PC()->IsMenu() && N && N->CanStartTicking();
			},
			[this]() { return Match() ? FString() : TEXT("o anfitrião não começou a partida"); },
			1.0f, 400.0f });
	}
	if (!bLan)
	{
		Steps.Add({ TEXT("jogar_skirmish"),
			[this]() { RetryAt = 6.0f; QueueClick(ButtonCenter(TEXT("skirmish"))); },
			[this]()
			{
				if (Match() != nullptr && Match()->IsReady() && PC() && !PC()->IsMenu())
				{
					return true;
				}
				// o clique às vezes não pega (base4, 10/10: o menu ficou parado 120 s): repete enquanto
				// a partida nem começou a carregar
				if (Match() == nullptr && PC() && PC()->IsMenu() && Timer >= RetryAt)
				{
					RetryAt = Timer + 6.0f;
					const FVector2D B = ButtonCenter(TEXT("skirmish"));
					UE_LOG(LogTemp, Warning, TEXT("RTS TEST o clique no Play Skirmish não pegou; de novo em (%.0f, %.0f)"), B.X, B.Y);
					QueueClick(B);
				}
				return false;
			},
			[this]() { return Match() ? FString() : TEXT("a partida não começou"); },
			5.0f, 120.0f });
	}

	if (bBase)
	{
		BuildBase();
	}

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
				FRTSDefInfo Info;
				const bool bArmy = !U.bStructure && (!bBase || (M->GetDef(uint16(U.Def), Info) && Info.bArmed));
				if (U.Owner == M->LocalPlayer() && bArmy && P->ProjectWorldLocationToScreen(FVector(U.PosCm.X, U.PosCm.Y, Map->GroundZCm(U.PosCm) + 150.0f), S))
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
			FRTSDefInfo Info;
			for (const FRTSUnitState& U : Units)
			{
				const bool bArmy = !U.bStructure && (!bBase || (M->GetDef(uint16(U.Def), Info) && Info.bArmed));
				Own += U.Owner == (M ? M->LocalPlayer() : 0) && bArmy ? 1 : 0;
			}
			const int32 Sel = PC() ? PC()->GetSelection().Num() : 0;
			// com base, a caixa pode pegar junto um caminhão que passava (o ZH também pega)
			return (bBase ? Sel >= Own : Sel == Own) && Own > 0 ? FString() : FString::Printf(TEXT("selecionou %d de %d"), Sel, Own);
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
		[this]()
		{
			if (!Match())
			{
				return FString(TEXT("sem partida"));
			}
			UE_LOG(LogTemp, Display, TEXT("RTS TEST névoa na partida: %s (esperada %s)"), Match()->IsFogShown() ? TEXT("sim") : TEXT("não"), bExpectFog ? TEXT("sim") : TEXT("não"));
			return Match()->IsFogShown() == bExpectFog ? FString() : (bExpectFog ? TEXT("a névoa não está ligada") : TEXT("a névoa está ligada numa sala sem névoa"));
		},
		1.0f, 20.0f });

	if (!bLan)
	{
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
	}

	Steps.Add({ TEXT("minimapa_camera"),
		[this, Hud]()
		{
			// botão direito no minimapa, na largada do inimigo: a câmera vai lá e a seleção fica
			SelectionBefore = PC() ? PC()->GetSelection().Num() : 0;
			MinimapTarget = Match() ? EnemyBaseCm() : FVector2D::ZeroVector;
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
			const FVector2D Enemy = Match() ? EnemyBaseCm() : Own;
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
		[this]() { NextBattleShot = 0.0f; NextFollow = 0.0f; NextPush = 12.0f; BattleShots = 0; bStallLogged = false; NextReinforce = 0.0f; NextWave = 20.0f; },
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
			if (bBase && Timer >= NextReinforce)
			{
				// o robô joga o resto: fila de tanques sempre com 2 (o clique no painel já foi
				// testado antes), e a cada 25 s todo o exército em ataque andando na base inimiga
				NextReinforce = Timer + 5.0f;
				const int32 Tank = DefIndex(TEXT("usa_mbt"));
				TArray<FRTSUnitState> Factories;
				if (Tank >= 0 && OwnOf(DefIndex(TEXT("usa_war_factory")), Factories))
				{
					for (const FRTSUnitState& F : Factories)
					{
						if (F.BuildFraction >= 1.0f && F.QueueSize < 2 && M->Money(M->LocalPlayer()) >= 900)
						{
							M->OrderProduce(F.Id, uint16(Tank));
						}
					}
				}
				if (Timer >= NextWave)
				{
					NextWave = Timer + 25.0f;
					TArray<FRTSUnitState> Army;
					OwnOf(Tank, Army);
					TArray<uint32> Ids;
					for (const FRTSUnitState& U : Army)
					{
						Ids.Add(U.Id);
					}
					if (Ids.Num())
					{
						M->OrderMove(Ids, EnemyBaseCm(), true);
						UE_LOG(LogTemp, Display, TEXT("RTS TEST onda: %d tanques em ataque andando na base inimiga"), Ids.Num());
					}
				}
			}
			if (!bSpectatorLogged && M->IsLocalDefeated())
			{
				bSpectatorLogged = true;
				const bool bAll = M->IsSpectatingAll();
				const bool bFogShown = M->IsFogShown();
				UE_LOG(LogTemp, Display, TEXT("RTS TEST espectador: caí no %.0f s; vê o mapa todo: %s; névoa desenhada: %s"), M->MatchSeconds(),
					bAll ? TEXT("sim (o time todo caiu)") : TEXT("não (aliado vivo: a visão do time)"), bFogShown ? TEXT("sim") : TEXT("não"));
				if (bExpectFog && bAll == bFogShown)
				{
					UE_LOG(LogTemp, Warning, TEXT("RTS TEST espectador: a névoa não seguiu a regra (vê tudo só quando o time todo cai)"));
					++Failures;
				}
				Shot(TEXT("espectador"));
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
		1.0f, bBase ? (NetPlayers > 2 ? 2400.0f : 900.0f) : 420.0f, false });

	// o painel entra 3 s depois do golpe final e leva 0,6 s pra aparecer: espera ele inteiro (a
	// foto mostra o resultado e o próximo passo acha o botão; no t6 o clique saiu antes do botão)
	Steps.Add({ TEXT("resultado"), nullptr,
		[this]() { return Match() && Match()->SecondsSinceEnd() > 3.8f && ButtonCenter(bLan ? TEXT("lobby") : TEXT("menu")).X >= 0.0; },
		[this]()
		{
			URTSMatchSubsystem* M = Match();
			const ERTSMatchResult R = M ? M->Result() : ERTSMatchResult::Playing;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST resultado: %s em %.0f s"),
				R == ERTSMatchResult::Victory ? TEXT("vitória") : (R == ERTSMatchResult::Defeat ? TEXT("derrota") : TEXT("empate/nenhum")), M ? M->MatchSeconds() : 0.0f);
			for (int32 P = 0; M && P < M->PlayerCount(); ++P)
			{
				UE_LOG(LogTemp, Display, TEXT("RTS TEST jogador %d (%s): time %d, %s"), P, *URTSMatchSubsystem::PlayerColorName(uint8(P)).ToString(),
					int32(M->TeamOf(uint8(P))), M->IsDefeated(uint8(P)) ? TEXT("derrotado") : TEXT("em pé"));
			}
			return FString();
		},
		2.0f });

	if (bLan)
	{
		Steps.Add({ TEXT("lan_sincronia"), nullptr, nullptr,
			[this]()
			{
				const URTSNetSession* N = NetSession();
				if (!N)
				{
					return FString(TEXT("sem sessão de rede"));
				}
				UE_LOG(LogTemp, Display, TEXT("RTS TEST LAN: %d jogos, dessincronia %s, conexão %s, %lld impressões digitais iguais"), N->PlayerCount(),
					N->IsDesynced() ? TEXT("SIM") : TEXT("não"), N->IsPeerLost() ? TEXT("perdida") : TEXT("ok"), N->HashMatches());
				return N->IsDesynced() ? FString::Printf(TEXT("dessincronia no passo %lld"), N->GetDesyncTick()) : FString();
			},
			0.5f, 5.0f, false });
	}
	if (bLan)
	{
		// anfitrião: o botão "voltar pra sala" leva todos; cliente: o anfitrião (o robô) leva a gente
		auto InLobby = [this, IsMenu]() { return IsMenu() && PC()->GetMenuPage() == ARTSPlayerController::EMenuPage::Lobby && NetSession() && NetSession()->IsActive(); };
		Steps.Add({ TEXT("lan_volta_sala"),
			bLanClient ? TFunction<void()>() : TFunction<void()>([this]() { QueueClick(ButtonCenter(TEXT("lobby"))); }),
			InLobby,
			[InLobby]() { return InLobby() ? FString() : TEXT("não voltou pra sala"); },
			bLanClient ? 0.5f : 2.0f, 120.0f });
		Steps.Add({ TEXT("lan_todos_na_sala"), nullptr,
			[this]() { const URTSNetSession* N = NetSession(); return N && N->PlayerCount() >= NetPlayers && N->EveryoneInLobby(); },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
				{
					UE_LOG(LogTemp, Display, TEXT("RTS TEST de volta na sala: vaga %d, %s, cor %d, %s"), M.Slot, *M.Name, M.Color, M.bInLobby ? TEXT("na sala") : TEXT("ainda no resultado"));
				}
				return N && N->PlayerCount() >= NetPlayers && N->EveryoneInLobby() ? FString()
					: FString::Printf(TEXT("na sala %d de %d"), N ? N->PlayerCount() : 0, NetPlayers);
			},
			1.0f, 60.0f });
	}
	else
	{
		Steps.Add({ TEXT("voltar_ao_menu"),
			[this]() { QueueClick(ButtonCenter(TEXT("menu"))); },
			[this, IsMenu]() { return IsMenu() && Match() == nullptr; },
			[this, IsMenu]() { return IsMenu() ? FString() : TEXT("não voltou pro menu"); },
			4.0f, 120.0f });
	}
}

FVector2D URTSAutoTest::EnemyBaseCm() const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return FVector2D::ZeroVector;
	}
	// quem ainda tem peça no mapa, e desses o mais perto da minha largada
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	TSet<uint8> Alive;
	for (const FRTSUnitState& U : Units)
	{
		if (M->IsEnemyOfLocal(U.Owner))
		{
			Alive.Add(U.Owner);
		}
	}
	const FVector2D Home = M->PlayerStartCm(M->LocalPlayer());
	FVector2D Best = M->PlayerStartCm(M->LocalPlayer() == 0 ? 1 : 0);
	double BestD = DBL_MAX;
	for (uint8 P : Alive)
	{
		const double D = FVector2D::Distance(M->PlayerStartCm(P), Home);
		if (D < BestD)
		{
			BestD = D;
			Best = M->PlayerStartCm(P);
		}
	}
	return Best;
}

FName URTSAutoTest::MyColorButton() const
{
	const URTSNetSession* N = NetSession();
	return FName(*FString::Printf(TEXT("lan_color_%d"), N ? int32(N->LocalSlot()) : 0));
}

int32 URTSAutoTest::MyLobbyColor() const
{
	const URTSNetSession* N = NetSession();
	for (const FRTSNetMember& M : N ? N->Members() : TArray<FRTSNetMember>())
	{
		if (M.Slot == N->LocalSlot())
		{
			return M.Color;
		}
	}
	return -1;
}

// ---------------------------------------------------------------- a base (ZH)

int32 URTSAutoTest::DefIndex(const FString& Key) const
{
	URTSMatchSubsystem* M = Match();
	FRTSDefInfo Info;
	for (int32 K = 0; M && K < M->DefCount(); ++K)
	{
		if (M->GetDef(uint16(K), Info) && Info.Key == Key)
		{
			return K;
		}
	}
	return -1;
}

bool URTSAutoTest::OwnOf(int32 Def, TArray<FRTSUnitState>& Out) const
{
	Out.Reset();
	URTSMatchSubsystem* M = Match();
	if (!M || Def < 0)
	{
		return false;
	}
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner == M->LocalPlayer() && U.Def == Def)
		{
			Out.Add(U);
		}
	}
	return Out.Num() > 0;
}

bool URTSAutoTest::ScreenOf(const FVector2D& Cm, FVector2D& Out, float UpCm) const
{
	URTSMatchSubsystem* M = Match();
	ARTSPlayerController* P = PC();
	const FRTSBakedMap* Map = M ? M->BakedMap() : nullptr;
	if (!Map || !P || !P->ProjectWorldLocationToScreen(FVector(Cm.X, Cm.Y, Map->GroundZCm(Cm) + UpCm), Out))
	{
		return false;
	}
	// fora das bordas e do painel de baixo (a barra de comando ocupa ~30% da altura)
	const FVector2D Size = ViewportFrac(FVector2D(1.0, 1.0));
	return Out.X > 60.0 && Out.Y > 70.0 && Out.X < Size.X - 60.0 && Out.Y < Size.Y * 0.66;
}

int32 URTSAutoTest::NearestDockToBase() const
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return -1;
	}
	const FVector2D Base = M->PlayerStartCm(M->LocalPlayer());
	int32 Best = -1;
	double BestD = 1e18;
	for (int32 K = 0; K < M->DockCount(); ++K)
	{
		FRTSDockState D;
		if (M->GetDock(K, D) && !D.bPile && FVector2D::Distance(D.PosCm, Base) < BestD)
		{
			BestD = FVector2D::Distance(D.PosCm, Base);
			Best = K;
		}
	}
	return Best;
}

FVector2D URTSAutoTest::SpotAnchor(ESpot Spot, float& OutMinRadiusCm, float& OutAngle) const
{
	URTSMatchSubsystem* M = Match();
	const FVector2D Base = M ? M->PlayerStartCm(M->LocalPlayer()) : FVector2D::ZeroVector;
	const FVector2D Enemy = M ? EnemyBaseCm() : FVector2D(1.0, 0.0);
	const FVector2D ToEnemy = (Enemy - Base).GetSafeNormal();
	const float EnemyAngle = FMath::Atan2(float(ToEnemy.Y), float(ToEnemy.X));
	TArray<FRTSUnitState> Centers;
	const FVector2D CC = OwnOf(DefIndex(TEXT("usa_command_center")), Centers) ? Centers[0].PosCm : Base;
	switch (Spot)
	{
	case ESpot::NearDock:
	{
		FRTSDockState D;
		if (M && M->GetDock(NearestDockToBase(), D))
		{
			// do depósito pro lado do CC (o caminhão anda pouco), fora da folga de 5,3 m
			const FVector2D ToCC = (CC - D.PosCm).GetSafeNormal();
			OutMinRadiusCm = float(D.HalfCm.Size()) + 1500.0f;
			OutAngle = FMath::Atan2(float(ToCC.Y), float(ToCC.X));
			return D.PosCm;
		}
		break;
	}
	case ESpot::Front:
		OutMinRadiusCm = 4000.0f;
		OutAngle = EnemyAngle;
		return CC;
	default:
		break;
	}
	// a usina atrás do CC (longe do inimigo)
	OutMinRadiusCm = 3200.0f;
	OutAngle = EnemyAngle + PI;
	return CC;
}

bool URTSAutoTest::FindSpot(int32 Def, int32 Dir, const FVector2D& Anchor, float MinRadiusCm, float StartAngle, FVector2D& OutCm) const
{
	URTSMatchSubsystem* M = Match();
	if (!M || Def < 0)
	{
		return false;
	}
	for (float R = MinRadiusCm; R < MinRadiusCm + 9000.0f; R += 400.0f)
	{
		for (int32 K = 0; K < 24; ++K)
		{
			// alterna dos dois lados do ângulo preferido: o mais perto dele primeiro
			const float Off = float((K + 1) / 2) * (2.0f * PI / 24.0f) * (K % 2 ? 1.0f : -1.0f);
			const FVector2D P = Anchor + FVector2D(FMath::Cos(StartAngle + Off), FMath::Sin(StartAngle + Off)) * R;
			FVector2D Center, Half, Screen;
			if (M->CanPlace(uint16(Def), Dir, P, Center, Half) == ERTSPlace::Ok && ScreenOf(Center, Screen, 0.0f))
			{
				OutCm = Center;
				return true;
			}
		}
	}
	return false;
}

void URTSAutoTest::AddBuildSteps(const FString& Label, const FString& Key, ESpot Spot)
{
	// a câmera em cima do lugar (como o jogador olha pra onde vai construir)
	Steps.Add({ Label + TEXT("_camera"),
		[this, Spot]()
		{
			float MinR = 0.0f, Angle = 0.0f;
			const FVector2D Anchor = SpotAnchor(Spot, MinR, Angle);
			TArray<FRTSUnitState> Centers;
			const FVector2D CC = OwnOf(DefIndex(TEXT("usa_command_center")), Centers) ? Centers[0].PosCm : Anchor;
			const FVector2D Look = Spot == ESpot::NearDock ? (Anchor + CC) * 0.5 : Anchor + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * MinR * 0.6;
			if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
			{
				Cam->JumpTo(Look);
			}
		},
		nullptr, nullptr, 0.6f, 5.0f, false });

	// o Dozer: clique nele (se andou pra fora da tela, a câmera vai até ele antes)
	Steps.Add({ Label + TEXT("_dozer"),
		[this]() { RetryAt = 0.0f; },
		[this]()
		{
			TArray<FRTSUnitState> Dozers;
			if (!OwnOf(DefIndex(TEXT("usa_dozer")), Dozers) || !PC())
			{
				return false;
			}
			if (PC()->GetSelection().Contains(Dozers[0].Id))
			{
				return true;
			}
			if (Timer >= RetryAt && Pending.IsEmpty())
			{
				RetryAt = Timer + 0.4f;
				FVector2D S;
				if (ScreenOf(Dozers[0].PosCm, S))
				{
					QueueClick(S);
				}
				else if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC()->GetPawn()))
				{
					// saiu da tela (foi construir longe): a câmera vai até ele, como o jogador faria
					UE_LOG(LogTemp, Display, TEXT("RTS TEST o Dozer está fora da tela: a câmera vai até ele"));
					Cam->JumpTo(Dozers[0].PosCm);
				}
			}
			return false;
		},
		[this]()
		{
			TArray<FRTSUnitState> Dozers;
			return OwnOf(DefIndex(TEXT("usa_dozer")), Dozers) && PC() && PC()->GetSelection().Contains(Dozers[0].Id) ? FString() : TEXT("o clique não selecionou o Dozer");
		},
		0.3f, 6.0f, false });

	// o botão do prédio no painel
	Steps.Add({ Label + TEXT("_botao"),
		[this, Key]() { RetryAt = 0.8f; QueueClick(ButtonCenter(FName(*FString::Printf(TEXT("eco_build_%d"), DefIndex(Key))))); },
		[this, Key]()
		{
			if (PC() && PC()->GetPlacingDef() == DefIndex(Key))
			{
				return true;
			}
			if (Timer >= RetryAt && Pending.IsEmpty())
			{
				RetryAt = Timer + 0.8f;
				UE_LOG(LogTemp, Warning, TEXT("RTS TEST o clique no botão de %s não pegou; de novo"), *Key);
				QueueClick(ButtonCenter(FName(*FString::Printf(TEXT("eco_build_%d"), DefIndex(Key)))));
			}
			return false;
		},
		[this, Key]() { return PC() && PC()->GetPlacingDef() == DefIndex(Key) ? FString() : TEXT("o botão não começou a posicionar o prédio"); },
		0.3f, 4.0f });

	// o lugar: onde a simulação diz que cabe, no chão da tela
	Steps.Add({ Label + TEXT("_lugar"),
		[this, Spot]()
		{
			// a câmera volta pro lugar da obra (o Dozer pode ter levado ela pra longe)
			float MinR = 0.0f, Angle = 0.0f;
			const FVector2D Anchor = SpotAnchor(Spot, MinR, Angle);
			TArray<FRTSUnitState> Centers;
			const FVector2D CC = OwnOf(DefIndex(TEXT("usa_command_center")), Centers) ? Centers[0].PosCm : Anchor;
			const FVector2D Look = Spot == ESpot::NearDock ? (Anchor + CC) * 0.5 : Anchor + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * MinR * 0.6;
			if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
			{
				Cam->JumpTo(Look);
			}
			RetryAt = 0.4f;   // o clique no chão no quadro em que a câmera já chegou
		},
		[this, Key, Spot]()
		{
			TArray<FRTSUnitState> Out;
			if (OwnOf(DefIndex(Key), Out))
			{
				return true;
			}
			if (Timer >= RetryAt && Pending.IsEmpty())
			{
				RetryAt = Timer + 2.0f;
				float MinR = 0.0f, Angle = 0.0f;
				const FVector2D Anchor = SpotAnchor(Spot, MinR, Angle);
				FVector2D Cm, S;
				if (PC() && FindSpot(DefIndex(Key), PC()->GetPlacingDir(), Anchor, MinR, Angle, Cm) && ScreenOf(Cm, S, 0.0f))
				{
					UE_LOG(LogTemp, Display, TEXT("RTS TEST %s: lugar em (%.0f, %.0f) m"), *Key, Cm.X / 100.0, Cm.Y / 100.0);
					QueueClick(S);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("RTS TEST %s: nenhum lugar livre na tela"), *Key);
				}
			}
			return false;
		},
		[this, Key]()
		{
			TArray<FRTSUnitState> Out;
			return OwnOf(DefIndex(Key), Out) ? FString() : TEXT("o clique no chão não começou a obra");
		},
		1.0f, 8.0f });

	// a obra no meio: o andaime, a parede subindo e a solda
	Steps.Add({ Label + TEXT("_obra"), nullptr,
		[this, Key]() { TArray<FRTSUnitState> Out; return OwnOf(DefIndex(Key), Out) && Out[0].BuildFraction >= 0.5f; },
		[this, Key]() { TArray<FRTSUnitState> Out; return OwnOf(DefIndex(Key), Out) ? FString() : TEXT("a obra sumiu"); },
		0.5f, 120.0f });

	Steps.Add({ Label + TEXT("_pronta"), nullptr,
		[this, Key]() { TArray<FRTSUnitState> Out; return OwnOf(DefIndex(Key), Out) && Out[0].BuildFraction >= 1.0f; },
		[this, Key]()
		{
			TArray<FRTSUnitState> Out;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST %s pronto em %.0f s de partida"), *Key, Match() ? Match()->MatchSeconds() : 0.0f);
			return OwnOf(DefIndex(Key), Out) && Out[0].BuildFraction >= 1.0f ? FString() : TEXT("a obra não terminou");
		},
		1.5f, 150.0f });
}

void URTSAutoTest::BuildBase()
{
	Steps.Add({ TEXT("base_inicio"), nullptr,
		[this]()
		{
			TArray<FRTSUnitState> A, B;
			return OwnOf(DefIndex(TEXT("usa_command_center")), A) && OwnOf(DefIndex(TEXT("usa_dozer")), B);
		},
		[this]()
		{
			TArray<FRTSUnitState> A, B;
			return OwnOf(DefIndex(TEXT("usa_command_center")), A) && OwnOf(DefIndex(TEXT("usa_dozer")), B) ? FString() : TEXT("sem o CC e o Dozer na largada");
		},
		1.0f, 10.0f });

	AddBuildSteps(TEXT("usina"), TEXT("usa_power_plant"), ESpot::Behind);
	AddBuildSteps(TEXT("centro"), TEXT("usa_supply_center"), ESpot::NearDock);

	// o caminhão que vem com o centro coleta sozinho: a 1ª entrega ("+$" no centro)
	Steps.Add({ TEXT("primeira_entrega"),
		[this]() { MoneyBefore = Match() ? Match()->Money(Match()->LocalPlayer()) : 0; },
		[this]() { return Match() && Match()->Money(Match()->LocalPlayer()) > MoneyBefore; },
		[this]()
		{
			const int32 Now = Match() ? Match()->Money(Match()->LocalPlayer()) : 0;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST 1ª entrega: +%d $ (%.0f s de partida)"), Now - MoneyBefore, Match() ? Match()->MatchSeconds() : 0.0f);
			return Now > MoneyBefore ? FString() : TEXT("o caminhão não entregou");
		},
		0.5f, 90.0f });

	AddBuildSteps(TEXT("fabrica"), TEXT("usa_war_factory"), ESpot::Front);

	Steps.Add({ TEXT("selecionar_fabrica"),
		[this]() { RetryAt = 0.0f; },
		[this]()
		{
			TArray<FRTSUnitState> F;
			if (!OwnOf(DefIndex(TEXT("usa_war_factory")), F) || !PC())
			{
				return false;
			}
			if (PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == F[0].Id)
			{
				return true;
			}
			FVector2D S;
			if (Timer >= RetryAt && Pending.IsEmpty() && ScreenOf(F[0].PosCm, S, 500.0f))
			{
				RetryAt = Timer + 0.8f;
				QueueClick(S);
			}
			return false;
		},
		[this]()
		{
			TArray<FRTSUnitState> F;
			return OwnOf(DefIndex(TEXT("usa_war_factory")), F) && PC() && PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == F[0].Id
				? FString() : TEXT("o clique não selecionou a fábrica");
		},
		0.8f, 6.0f });

	Steps.Add({ TEXT("produzir_tanques"),
		[this]()
		{
			const FVector2D B = ButtonCenter(FName(*FString::Printf(TEXT("eco_produce_%d"), DefIndex(TEXT("usa_mbt")))));
			for (int32 K = 0; K < 5; ++K)
			{
				QueueClick(B);
			}
		},
		[this]()
		{
			TArray<FRTSUnitState> F;
			if (OwnOf(DefIndex(TEXT("usa_war_factory")), F) && F[0].QueueSize >= 4)
			{
				return true;
			}
			if (Timer >= 1.0f && Pending.IsEmpty())
			{
				QueueClick(ButtonCenter(FName(*FString::Printf(TEXT("eco_produce_%d"), DefIndex(TEXT("usa_mbt"))))));
			}
			return false;
		},
		[this]()
		{
			TArray<FRTSUnitState> F;
			const int32 Q = OwnOf(DefIndex(TEXT("usa_war_factory")), F) ? F[0].QueueSize : 0;
			return Q >= 4 ? FString() : FString::Printf(TEXT("a fila tem %d tanques"), Q);
		},
		0.5f, 6.0f });

	// o clique no chão com a fábrica selecionada: os tanques saem e vão pra lá (ZH). O chão tem
	// que estar livre: um prédio embaixo seleciona o prédio, e um botão por cima pega o clique
	// (lan4c, 11/10: com a base no sudeste, o ponto rumo ao inimigo caía no centro de comando)
	auto ClickRally = [this](int32 Attempt)
	{
		TArray<FRTSUnitState> F;
		URTSMatchSubsystem* M = Match();
		if (!M || !PC() || !OwnOf(DefIndex(TEXT("usa_war_factory")), F))
		{
			return;
		}
		const FVector2D Dir = (EnemyBaseCm() - F[0].PosCm).GetSafeNormal();
		static const float Turns[] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };
		int32 Found = 0;
		for (const float Turn : Turns)
		{
			const FVector2D D2 = Dir.GetRotated(Turn);
			for (float D = 5000.0f; D >= 2500.0f; D -= 500.0f)
			{
				FVector2D S;
				const ARTSHUD* H = Cast<ARTSHUD>(PC()->GetHUD());
				if (ScreenOf(F[0].PosCm + D2 * D, S, 0.0f) && PC()->UnitUnderScreen(S) == 0 && (!H || (H->ButtonAt(S).IsNone() && !H->IsOverMinimap(S))) && Found++ == Attempt)
				{
					UE_LOG(LogTemp, Display, TEXT("RTS TEST ponto de encontro: tentativa %d em (%.0f, %.0f) na tela"), Attempt, S.X, S.Y);
					QueueClick(S);
					return;
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("RTS TEST ponto de encontro: sem chão livre na tela (tentativa %d)"), Attempt);
	};
	Steps.Add({ TEXT("ponto_de_encontro"),
		[this, ClickRally]() { RetryAt = 1.2f; RallyTry = 0; ClickRally(RallyTry); },
		[this, ClickRally]()
		{
			TArray<FRTSUnitState> F;
			if (OwnOf(DefIndex(TEXT("usa_war_factory")), F) && F[0].bHasRally)
			{
				return true;
			}
			if (Timer >= RetryAt && Pending.IsEmpty())
			{
				RetryAt = Timer + 1.2f;
				ClickRally(++RallyTry);
			}
			return false;
		},
		[this]()
		{
			TArray<FRTSUnitState> F;
			return OwnOf(DefIndex(TEXT("usa_war_factory")), F) && F[0].bHasRally ? FString() : TEXT("o ponto de encontro não ficou marcado");
		},
		0.8f, 8.0f });

	Steps.Add({ TEXT("tanques_prontos"), nullptr,
		[this]() { TArray<FRTSUnitState> T; OwnOf(DefIndex(TEXT("usa_mbt")), T); return T.Num() >= 4; },
		[this]()
		{
			TArray<FRTSUnitState> T;
			OwnOf(DefIndex(TEXT("usa_mbt")), T);
			UE_LOG(LogTemp, Display, TEXT("RTS TEST %d tanques prontos em %.0f s de partida"), T.Num(), Match() ? Match()->MatchSeconds() : 0.0f);
			return T.Num() >= 4 ? FString() : FString::Printf(TEXT("só %d tanques"), T.Num());
		},
		1.0f, 120.0f });

	// a vitrine: a câmera de perto em cada prédio e 3 fotos em quadros seguidos, pra achar o que
	// pisca (as faces no mesmo plano do portão e do anexo da fábrica, 10/10: a diferença entre os
	// quadros mostra; o ventilador e o radar giram de propósito)
	for (const TCHAR* Key : { TEXT("usa_command_center"), TEXT("usa_power_plant"), TEXT("usa_supply_center"), TEXT("usa_war_factory") })
	{
		const FString K = Key;
		const FString Short = K.RightChop(4);
		Steps.Add({ TEXT("vitrine_") + Short,
			[this, K]()
			{
				VitrineShots = 0;
				TArray<FRTSUnitState> S;
				ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
				if (Cam && OwnOf(DefIndex(K), S))
				{
					Cam->JumpTo(S[0].PosCm);
					ARTSCameraPawn::FInput In;
					In.ZoomSteps = 12.0f;   // o mais perto (31,6 m de altura)
					Cam->ApplyInput(In, 0.0f);
				}
			},
			[this, Short]()
			{
				if (Timer < 1.8f)
				{
					return false;   // o zoom chega e o borrão do movimento some
				}
				if (!FScreenshotRequest::IsScreenshotRequested() && VitrineShots < 3)
				{
					Shot(FString::Printf(TEXT("vitrine_%s_%d"), *Short, VitrineShots++));
				}
				return VitrineShots >= 3 && !FScreenshotRequest::IsScreenshotRequested();
			},
			nullptr, 0.1f, 8.0f, false });
	}
	Steps.Add({ TEXT("vitrine_fim"),
		[this]()
		{
			if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
			{
				ARTSCameraPawn::FInput In;
				In.bReset = true;   // volta a altura e o giro padrão
				Cam->ApplyInput(In, 0.0f);
			}
		},
		nullptr, nullptr, 1.5f, 5.0f, false });

	// a câmera no exército pra caixa de seleção
	Steps.Add({ TEXT("camera_exercito"),
		[this]()
		{
			TArray<FRTSUnitState> T;
			if (OwnOf(DefIndex(TEXT("usa_mbt")), T))
			{
				FVector2D Sum = FVector2D::ZeroVector;
				for (const FRTSUnitState& U : T)
				{
					Sum += U.PosCm;
				}
				if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
				{
					Cam->JumpTo(Sum / T.Num());
				}
			}
		},
		nullptr, nullptr, 0.8f, 5.0f, false });
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
	// a partida acabou antes da hora (perdeu a base, por exemplo): pula pro resultado em vez de
	// esperar cada passo estourar o tempo segurando a placa (teste base1, 10/10)
	if (Phase == 0 && Match() && Match()->Result() != ERTSMatchResult::Playing)
	{
		const int32 ResultStep = Steps.IndexOfByPredicate([](const FStep& X) { return X.Name == TEXT("resultado"); });
		if (ResultStep > Index)
		{
			++Failures;
			UE_LOG(LogTemp, Warning, TEXT("RTS TEST FALHOU %s: a partida acabou antes (pula pro resultado)"), *Steps[Index].Name);
			Index = ResultStep;
		}
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
			// com base, sem a base de pé o resto do teste não diz nada: encerra logo e libera a placa
			const int32 ArmyStep = Steps.IndexOfByPredicate([](const FStep& X) { return X.Name == TEXT("caixa_de_selecao"); });
			if (bBase && Index < ArmyStep)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS TEST abortado: a base não ficou de pé (%s)"), *S.Name);
				Shot(S.Name);
				Finish();
				return;
			}
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
