#include "RTSAutoTest.h"
#include "EngineUtils.h"
#include "LookDirector.h"
#include "LookRecipe.h"

#include "RTSFogOfWar.h"
#include "RTSFrameRecorder.h"
#include "HAL/FileManager.h"
#include "RTSHUD.h"

#include "Dom/JsonObject.h"
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
#include "RTSChat.h"
#include "RTSHUD.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSNetSession.h"
#include "RTSPlayerController.h"
#include "RTSUnitActor.h"
#include "Sim/World.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
	bAIvsAI = ScriptName == TEXT("aivsai");
	FString ScenarioArg;
	bScenario = FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), ScenarioArg);
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetPlayers="), NetPlayers);
	NetPlayers = FMath::Clamp(NetPlayers, 2, 8);
	bExpectFog = !FParse::Param(FCommandLine::Get(), TEXT("RTSTestNoFog"));
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetGate="), GateFile);
	FParse::Value(FCommandLine::Get(), TEXT("-RTSNetAI="), NetAIs);
	bTeams = FParse::Param(FCommandLine::Get(), TEXT("RTSTestTeams"));
	FParse::Value(FCommandLine::Get(), TEXT("-RTSTestMoney="), TestMoney);
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

void URTSAutoTest::QueueChord(const FVector2D& From, const FVector2D& To, bool bRightFirst, bool bRelease)
{
	const FKey First = bRightFirst ? EKeys::RightMouseButton : EKeys::LeftMouseButton;
	const FKey Second = bRightFirst ? EKeys::LeftMouseButton : EKeys::RightMouseButton;
	Pending.Add({ FInputEvent::EType::Move, From, EKeys::Invalid });
	Pending.Add({ FInputEvent::EType::Down, From, First });
	Pending.Add({ FInputEvent::EType::Down, From, Second });
	for (int32 K = 1; K <= 8; ++K)
	{
		Pending.Add({ FInputEvent::EType::Move, FMath::Lerp(From, To, K / 8.0), EKeys::Invalid });
	}
	if (bRelease)
	{
		QueueUp(To, EKeys::LeftMouseButton);
		QueueUp(To, EKeys::RightMouseButton);
	}
}

void URTSAutoTest::QueueUp(const FVector2D& At, FKey Button)
{
	Pending.Add({ FInputEvent::EType::Up, At, Button });
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

	Steps.Add({ TEXT("menu"), nullptr, IsMenu, nullptr, bScenario ? 0.5f : 6.0f, 90.0f, !bScenario });

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
		// (v4) o dinheiro inicial da sala pelos botões + e - (antes da névoa: a porta dos robôs abre depois)
		if (TestMoney > 0)
		{
			Steps.Add({ TEXT("lan_dinheiro"),
				[this]() { RetryAt = 0.0f; },
				[this]()
				{
					const URTSNetSession* N = NetSession();
					if (N && N->GetStartingMoney() == TestMoney)
					{
						return true;
					}
					if (N && Timer >= RetryAt && Pending.IsEmpty())
					{
						RetryAt = Timer + 0.4f;
						QueueClick(ButtonCenter(N->GetStartingMoney() < TestMoney ? TEXT("lan_money_up") : TEXT("lan_money_down")));
					}
					return false;
				},
				[this]()
				{
					const URTSNetSession* N = NetSession();
					UE_LOG(LogTemp, Display, TEXT("RTS TEST dinheiro inicial da sala: %d (pedido %d)"), N ? N->GetStartingMoney() : -1, TestMoney);
					return N && N->GetStartingMoney() == TestMoney ? FString() : FString::Printf(TEXT("o botão da sala não chegou em %d"), TestMoney);
				},
				0.5f, 20.0f });
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
		// (17/10) o chat da partida, pelo teclado como o Felipe: o Enter abre, as letras (com acento, e o A e o S, que são
		// ordens) vão pra caixa e não pro jogo, o Enter manda; com um robô na sala, a linha dele chega aqui pela rede
		static const FString ChatLine = TEXT("ataque às 5, ação já!");
		static int32 ChatOrdersBefore = 0;
		Steps.Add({ TEXT("chat_abrir"),
			[this]() { QueueKey(EKeys::Enter); },
			[]() { return RTSChat::IsTyping(); },
			[]() { return RTSChat::IsTyping() ? FString() : TEXT("o Enter não abriu o chat"); },
			0.3f, 5.0f, false });
		Steps.Add({ TEXT("chat_digitar"),
			[this]()
			{
				ChatOrdersBefore = PC() ? PC()->OrdersGiven() : 0;
				TGuardValue<bool> Injecting(Gate.IsValid() ? Gate->bInjecting : bDummyInjecting, true);
				RTSChat::SimulateTyping(ChatLine);
			},
			[]() { return RTSChat::Draft() == ChatLine; },
			[this]()
			{
				UE_LOG(LogTemp, Display, TEXT("RTS TEST chat: rascunho \"%s\", ordens %d → %d"), *RTSChat::Draft(), ChatOrdersBefore, PC() ? PC()->OrdersGiven() : -1);
				if (RTSChat::Draft() != ChatLine)
				{
					return FString::Printf(TEXT("a caixa ficou com \"%s\""), *RTSChat::Draft());
				}
				return PC() && PC()->OrdersGiven() == ChatOrdersBefore ? FString() : TEXT("as letras do chat viraram ordem no jogo");
			},
			0.5f, 5.0f });
		Steps.Add({ TEXT("chat_enviar"),
			[this]() { QueueKey(EKeys::Enter); },
			[this]()
			{
				const URTSNetSession* N = NetSession();
				return !RTSChat::IsTyping() && N && N->ChatLines().Num() > 0 && N->ChatLines().Last().Text == ChatLine;
			},
			[this]()
			{
				const URTSNetSession* N = NetSession();
				return N && N->ChatLines().Num() > 0 && N->ChatLines().Last().Text == ChatLine ? FString() : TEXT("a linha não foi");
			},
			1.0f, 5.0f });
		if (NetPlayers - NetAIs >= 2)
		{
			// o robô manda a dele (-RTSTestChat=) com 5 s de partida
			Steps.Add({ TEXT("chat_receber"), nullptr,
				[this]()
				{
					const URTSNetSession* N = NetSession();
					return N && N->ChatLines().ContainsByPredicate([N](const URTSNetSession::FChatLine& L) { return L.Slot != N->LocalSlot(); });
				},
				[this]()
				{
					const URTSNetSession* N = NetSession();
					return N && N->ChatLines().ContainsByPredicate([N](const URTSNetSession::FChatLine& L) { return L.Slot != N->LocalSlot(); })
						? FString() : TEXT("a linha do outro jogador não chegou");
				},
				1.0f, 60.0f });
		}
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
	if (!bLan && TestMoney > 0)
	{
		// (v4) o dinheiro inicial pelos botões + e - do menu, como o Felipe faria
		Steps.Add({ TEXT("dinheiro"),
			[this]() { RetryAt = 0.0f; },
			[this]()
			{
				const URTSSessionSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
				if (S && S->StartingMoney == TestMoney)
				{
					return true;
				}
				if (S && Timer >= RetryAt && Pending.IsEmpty())
				{
					RetryAt = Timer + 0.4f;
					QueueClick(ButtonCenter(S->StartingMoney < TestMoney ? TEXT("money_up") : TEXT("money_down")));
				}
				return false;
			},
			[this]()
			{
				const URTSSessionSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
				UE_LOG(LogTemp, Display, TEXT("RTS TEST dinheiro inicial no menu: %d (pedido %d)"), S ? S->StartingMoney : -1, TestMoney);
				return S && S->StartingMoney == TestMoney ? FString() : FString::Printf(TEXT("o botão do menu não chegou em %d"), TestMoney);
			},
			0.5f, 20.0f });
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
	// (v4) com a IA jogando pelo jogador local (aivsai, -RTSSelfAI: o mesmo teste do RTSMatchSubsystem) a conferência na partida
	// não vale: a IA põe a usina e o Dozer na fila no passo 0, e o dinheiro já baixou quando ela olha (~0,7 s depois). Fica a do
	// menu (o passo dinheiro)
	const bool bSelfAI = bAIvsAI || (!bLan && !bScenario && FParse::Param(FCommandLine::Get(), TEXT("RTSSelfAI")));
	if (TestMoney > 0 && bSelfAI)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS TEST dinheiro na partida: não confere (a IA joga pelo jogador local e gasta no 1º passo)"));
	}
	if (TestMoney > 0 && !bSelfAI)
	{
		// (v4) a partida começou com o escolhido (× 5 no -RTSFast), antes de qualquer obra
		Steps.Add({ TEXT("dinheiro_na_partida"), nullptr,
			[this]() { return Match() != nullptr; },
			[this]()
			{
				const URTSSessionSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
				const int32 Want = TestMoney * (S ? S->MoneyScale : 1);
				const int32 Have = Match() ? Match()->Money(Match()->LocalPlayer()) : -1;
				UE_LOG(LogTemp, Display, TEXT("RTS TEST dinheiro na partida: %d (esperado %d)"), Have, Want);
				return Have == Want ? FString() : FString::Printf(TEXT("o jogador começou com %d, não %d"), Have, Want);
			},
			0.2f, 10.0f, false });
	}

	if (bScenario)
	{
		BuildScenario();
		return;
	}
	if (bAIvsAI)
	{
		BuildAIvsAI();
		return;
	}
	if (bBase)
	{
		BuildBase();
	}

	// a caixa em volta de todos os seus tanques, como o jogador faria (no começo e de novo depois da linha dos dois botões, v4)
	auto QueueArmyBox = [this]()
	{
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
	};
	auto CheckArmySelected = [this]()
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
	};
	Steps.Add({ TEXT("caixa_de_selecao"), QueueArmyBox, []() { return true; }, CheckArmySelected, 1.0f });

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

	// (v4, 23/10) a linha dos dois botões, clique a clique como o Felipe: o esquerdo + o direito arrastando puxa a linha e mostra a
	// vaga de cada um no chão; o esquerdo arrastado sozinho é sempre a caixa (o defeito que ele contou: com tanques selecionados,
	// arrastar pra pegar outros virava a linha) e o direito sozinho rola. A foto da prévia (os anéis) e a da chegada (os tanques
	// em cima deles) saem com a mesma câmera: o A/B
	const FVector2D LineFrom(0.35, 0.45);
	const FVector2D LineTo(0.65, 0.45);
	const FVector2D LineFrom2(0.35, 0.55);
	const FVector2D LineTo2(0.65, 0.55);
	const FVector2D StillAt(0.5, 0.62);
	auto CamFocus = [this]()
	{
		const ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
		return Cam ? Cam->GetFocusCm() : FVector2D(1e9, 1e9);
	};
	auto CamYaw = [this]()
	{
		const ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
		return Cam ? Cam->GetViewYawDeg() : 0.0f;
	};
	// o estado antes de cada passo: a câmera, as ordens, a seleção e o destino de cada um da seleção
	auto LineSnapshot = [this, CamFocus]()
	{
		LineCamCm = CamFocus();
		LineOrders = PC() ? PC()->OrdersGiven() : 0;
		LineSelection = PC() ? PC()->GetSelection() : TArray<uint32>();
		LineOrderPoints.Reset();
		for (const uint32 Id : LineSelection)
		{
			FVector2D At;
			if (Match() && Match()->GetOrderPointCm(Id, At))
			{
				LineOrderPoints.Add(Id, At);
			}
		}
	};
	// nada mudou: nenhuma ordem, a mesma seleção, nenhum destino mexido, sem a pausa e sem linha aberta (bCamera: e a câmera parada)
	auto LineUnchanged = [this, CamFocus](bool bCamera) -> FString
	{
		ARTSPlayerController* P = PC();
		URTSMatchSubsystem* M = Match();
		if (!P || !M)
		{
			return TEXT("sem partida");
		}
		TArray<uint32> Now = P->GetSelection();
		TArray<uint32> Was = LineSelection;
		Now.Sort();
		Was.Sort();
		int32 Moved = 0;
		for (const TPair<uint32, FVector2D>& It : LineOrderPoints)
		{
			FVector2D At;
			Moved += M->GetOrderPointCm(It.Key, At) && FVector2D::Distance(At, It.Value) > 1.0 ? 1 : 0;
		}
		const double CamCm = FVector2D::Distance(CamFocus(), LineCamCm);
		ARTSPlayerController::FLineDrag L;
		UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: ordens %d → %d, seleção %d → %d, %d destinos mexidos, câmera andou %.1f cm, pausa %d"),
			LineOrders, P->OrdersGiven(), Was.Num(), Now.Num(), Moved, CamCm, int32(P->IsPauseMenuOpen()));
		if (P->OrdersGiven() != LineOrders || Moved > 0)
		{
			return FString::Printf(TEXT("saiu ordem (%d ordens novas, %d destinos mexidos)"), P->OrdersGiven() - LineOrders, Moved);
		}
		if (Now != Was)
		{
			return FString::Printf(TEXT("a seleção mudou (%d → %d)"), Was.Num(), Now.Num());
		}
		if (P->IsPauseMenuOpen() || P->GetLineDrag(L))
		{
			return TEXT("abriu a pausa, ou a linha ficou aberta");
		}
		if (bCamera && CamCm > 1.0)
		{
			return FString::Printf(TEXT("a câmera andou %.0f cm"), CamCm);
		}
		return FString();
	};
	// o controle já leu o último evento da fila (ele chega no quadro seguinte ao da entrega)
	auto InputDone = [this]() { return InputQuietFrames >= 3; };
	auto MedianMs = [this](int32 From, int32 To)
	{
		TArray<float> V;
		for (int32 K = FMath::Max(0, From); K < FMath::Min(To, FrameMs.Num()); ++K)
		{
			V.Add(FrameMs[K]);
		}
		V.Sort();
		return V.IsEmpty() ? 0.0f : V[V.Num() / 2];
	};
	// os destinos que a simulação deu contra as vagas da prévia, como conjunto a 1 cm (a linha congelada dá o mesmo ponto e o
	// mesmo valor, e o conjunto das vagas não depende de onde os tanques estão: só quem fica com qual)
	auto LineOrdersMatch = [this](double& OutWorstCm)
	{
		OutWorstCm = 0.0;
		URTSMatchSubsystem* M = Match();
		int32 Matched = 0;
		TArray<FVector2D> Free = LineSlotsCm;
		for (const uint32 Id : LineIds)
		{
			FVector2D At;
			if (!M || !M->GetOrderPointCm(Id, At))
			{
				continue;
			}
			int32 Best = INDEX_NONE;
			double BestCm = 1e12;
			for (int32 K = 0; K < Free.Num(); ++K)
			{
				const double D = FVector2D::Distance(At, Free[K]);
				if (D < BestCm)
				{
					BestCm = D;
					Best = K;
				}
			}
			OutWorstCm = FMath::Max(OutWorstCm, BestCm);
			if (Best != INDEX_NONE && BestCm <= 1.0)
			{
				++Matched;
				Free.RemoveAtSwap(Best);
			}
		}
		return Matched;
	};

	Steps.Add({ TEXT("linha_previa"),
		[this, LineSnapshot, LineFrom, LineTo]()
		{
			LineSnapshot();
			LineFrameStart = FrameMs.Num();
			QueueChord(ViewportFrac(LineFrom), ViewportFrac(LineTo), false, false);   // o esquerdo antes, da esquerda pra direita, segurando
		},
		InputDone,
		[this, CamFocus, CamYaw, MedianMs]()
		{
			ARTSPlayerController* P = PC();
			URTSMatchSubsystem* M = Match();
			ARTSPlayerController::FLineDrag L;
			if (!P || !M || !P->GetLineDrag(L))
			{
				return FString(TEXT("o esquerdo + o direito arrastando não puxaram a linha"));
			}
			TArray<URTSMatchSubsystem::FLineSlot> Slots;
			M->PreviewLineMove(P->GetSelection(), L.CenterCm, L.bFormation, L.FacingDeg, L.WidthM, Slots);
			int32 Troops = 0;
			for (const uint32 Id : P->GetSelection())
			{
				FRTSUnitState U;
				Troops += M->GetUnit(Id, U) && !U.bStructure && U.Owner == M->LocalPlayer() ? 1 : 0;
			}
			LineIds.Reset();
			LineSlotsCm.Reset();
			double GapM = 1e9;
			for (int32 A = 0; A < Slots.Num(); ++A)
			{
				LineIds.Add(Slots[A].Id);
				LineSlotsCm.Add(Slots[A].Cm);
				for (int32 B = A + 1; B < Slots.Num(); ++B)
				{
					GapM = FMath::Min(GapM, FVector2D::Distance(Slots[A].Cm, Slots[B].Cm) / 100.0);
				}
			}
			LineFacingDeg = L.FacingDeg;
			const double CamCm = FVector2D::Distance(CamFocus(), LineCamCm);
			const float FaceOff = FMath::Abs(FMath::FindDeltaAngleDegrees(CamYaw(), L.FacingDeg));
			UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: %d vagas pra %d unidades, %.0f m de (%.0f, %.0f) a (%.0f, %.0f), frente a %.1f° da câmera, par mais perto a %.1f m, câmera andou %.1f cm"),
				Slots.Num(), Troops, L.WidthM, L.StartCm.X / 100.0, L.StartCm.Y / 100.0, L.EndCm.X / 100.0, L.EndCm.Y / 100.0, FaceOff, Slots.Num() > 1 ? GapM : 0.0, CamCm);
			UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: o custo da prévia, quadro mediano %.1f ms segurando os dois botões contra %.1f ms antes"),
				MedianMs(LineFrameStart, FrameMs.Num()), MedianMs(LineFrameStart - 90, LineFrameStart));
			if (Troops == 0 || Slots.Num() != Troops)
			{
				return FString::Printf(TEXT("a prévia deu %d vagas pra %d unidades"), Slots.Num(), Troops);
			}
			if (CamCm > 1.0)
			{
				return FString::Printf(TEXT("a câmera andou %.0f cm segurando os dois botões (o direito rolou)"), CamCm);
			}
			if (FaceOff > 10.0f)
			{
				return FString::Printf(TEXT("da esquerda pra direita a frente devia ficar de costas pra câmera (%.0f° fora)"), FaceOff);
			}
			if (Slots.Num() > 1 && GapM < 7.0)
			{
				return FString::Printf(TEXT("vagas encostadas (%.1f m)"), GapM);
			}
			return FString();
		},
		1.5f, 10.0f });

	Steps.Add({ TEXT("linha_ordem"),
		[this, LineTo]()
		{
			// soltar: o esquerdo e, no quadro seguinte, o direito (a mão nunca solta os dois no mesmo quadro)
			QueueUp(ViewportFrac(LineTo), EKeys::LeftMouseButton);
			QueueUp(ViewportFrac(LineTo), EKeys::RightMouseButton);
		},
		[this, InputDone, LineOrdersMatch]()
		{
			double WorstCm = 0.0;
			return InputDone() && !LineIds.IsEmpty() && LineOrdersMatch(WorstCm) == LineIds.Num();
		},
		[this, LineOrdersMatch]()
		{
			ARTSPlayerController* P = PC();
			if (!P)
			{
				return FString(TEXT("sem jogador"));
			}
			double WorstCm = 0.0;
			const int32 Matched = LineOrdersMatch(WorstCm);
			TArray<uint32> Now = P->GetSelection();
			TArray<uint32> Was = LineSelection;
			Now.Sort();
			Was.Sort();
			ARTSPlayerController::FLineDrag L;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: %d de %d destinos nas vagas da prévia (o mais longe a %.2f cm), ordens %d → %d, seleção %d → %d"),
				Matched, LineIds.Num(), WorstCm, LineOrders, P->OrdersGiven(), Was.Num(), Now.Num());
			if (LineIds.IsEmpty() || Matched != LineIds.Num())
			{
				return FString::Printf(TEXT("só %d de %d destinos caíram nas vagas da prévia (o mais longe a %.0f cm)"), Matched, LineIds.Num(), WorstCm);
			}
			if (P->OrdersGiven() != LineOrders + 1)
			{
				return FString::Printf(TEXT("%d ordens em vez de 1"), P->OrdersGiven() - LineOrders);
			}
			if (Now != Was)
			{
				return FString::Printf(TEXT("soltar o direito mexeu na seleção (%d → %d)"), Was.Num(), Now.Num());
			}
			if (P->IsPauseMenuOpen() || P->GetLineDrag(L))
			{
				return FString(TEXT("abriu a pausa, ou a linha ficou aberta"));
			}
			return FString();
		},
		0.3f, 8.0f, false });

	Steps.Add({ TEXT("linha_chegada"),
		[this]() { LineStillFor = 0.0f; LineLastTimer = -1.0f; },
		[this]()
		{
			// todos parados por 1,5 s seguidos (o giro final no lugar), até 24 s
			URTSMatchSubsystem* M = Match();
			bool bAllStill = M != nullptr;
			for (const uint32 Id : LineIds)
			{
				FRTSUnitState U;
				bAllStill = bAllStill && !(M->GetUnit(Id, U) && U.bMoving);
			}
			LineStillFor = bAllStill ? LineStillFor + (LineLastTimer < 0.0f ? 0.0f : Timer - LineLastTimer) : 0.0f;
			LineLastTimer = Timer;
			return LineStillFor >= 1.5f || Timer >= 24.0f;
		},
		[this]()
		{
			// a chegada é da simulação (o TestLineDrag cobra); aqui ficam o registro e a foto, com a câmera da prévia
			URTSMatchSubsystem* M = Match();
			int32 Alive = 0;
			int32 Still = 0;
			int32 Near = 0;
			int32 Facing = 0;
			for (const uint32 Id : LineIds)
			{
				FRTSUnitState U;
				FVector2D Goal;
				if (!M || !M->GetUnit(Id, U) || !M->GetOrderPointCm(Id, Goal))
				{
					continue;
				}
				++Alive;
				const double OffM = FVector2D::Distance(U.PosCm, Goal) / 100.0;
				const float TurnOff = FMath::Abs(FMath::FindDeltaAngleDegrees(LineFacingDeg, U.HullYawDeg));
				Still += U.bMoving ? 0 : 1;
				Near += OffM < 3.0 ? 1 : 0;
				Facing += TurnOff < 10.0f ? 1 : 0;
				UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: #%u a %.1f m da vaga, o casco a %.0f° da frente%s"), Id, OffM, TurnOff, U.bMoving ? TEXT(", andando") : TEXT(""));
			}
			UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: a chegada em %.1f s, %d de %d parados, %d a menos de 3 m da vaga, %d virados pra frente da linha"),
				Timer, Still, Alive, Near, Facing);
			return FString();
		},
		2.0f, 30.0f });

	Steps.Add({ TEXT("linha_esc"),
		[this, LineSnapshot, LineFrom2, LineTo2]()
		{
			// puxa, o Esc desiste (sem abrir a pausa), e depois solta os dois: nenhuma ordem
			LineSnapshot();
			QueueChord(ViewportFrac(LineFrom2), ViewportFrac(LineTo2), false, false);
			QueueKey(EKeys::Escape);
			QueueUp(ViewportFrac(LineTo2), EKeys::LeftMouseButton);
			QueueUp(ViewportFrac(LineTo2), EKeys::RightMouseButton);
		},
		InputDone,
		[LineUnchanged]() { return LineUnchanged(true); },
		0.5f, 10.0f, false });

	Steps.Add({ TEXT("linha_solta_um"),
		[this, LineSnapshot, LineFrom2, LineTo2]()
		{
			// o direito antes, da direita pra esquerda, segurando: a frente vira pra câmera (a foto mostra os risquinhos)
			LineSnapshot();
			QueueChord(ViewportFrac(LineTo2), ViewportFrac(LineFrom2), true, false);
		},
		InputDone,
		[this, CamYaw]()
		{
			ARTSPlayerController::FLineDrag L;
			if (!PC() || !PC()->GetLineDrag(L))
			{
				return FString(TEXT("com o direito antes, os dois botões não puxaram a linha"));
			}
			const float FaceOff = FMath::Abs(FMath::FindDeltaAngleDegrees(CamYaw() + 180.0f, L.FacingDeg));
			UE_LOG(LogTemp, Display, TEXT("RTS TEST linha: da direita pra esquerda, a frente a %.1f° de olhar pra câmera"), FaceOff);
			return FaceOff <= 10.0f ? FString() : FString::Printf(TEXT("da direita pra esquerda a frente devia virar pra câmera (%.0f° fora)"), FaceOff);
		},
		0.8f, 10.0f });

	Steps.Add({ TEXT("linha_solta_um_fim"),
		[this, LineFrom2]()
		{
			bLineQueued = false;
			QueueUp(ViewportFrac(LineFrom2), EKeys::LeftMouseButton);
		},
		[this, LineFrom2]()
		{
			// o direito segue apertado 0,6 s depois do esquerdo subir (passou da folga de 0,25 s): desiste, sem ordem nem rolagem
			if (!bLineQueued)
			{
				if (Timer >= 0.7f)
				{
					bLineQueued = true;
					QueueUp(ViewportFrac(LineFrom2), EKeys::RightMouseButton);
				}
				return false;
			}
			return InputQuietFrames >= 3;
		},
		[LineUnchanged]() { return LineUnchanged(true); },
		0.1f, 10.0f, false });

	Steps.Add({ TEXT("acorde_parado"),
		[this, LineSnapshot, StillAt]()
		{
			// os dois apertados e soltos sem mexer: nada (nem ordem, nem desselecionar, nem clique)
			LineSnapshot();
			QueueChord(ViewportFrac(StillAt), ViewportFrac(StillAt), false, true);
		},
		InputDone,
		[LineUnchanged]() { return LineUnchanged(true); },
		0.5f, 10.0f, false });

	Steps.Add({ TEXT("direito_rola"),
		[this, LineSnapshot, StillAt]()
		{
			// só o direito, arrastado 200 px por 0,6 s: rola a câmera, como sempre
			LineSnapshot();
			bLineQueued = false;
			const FVector2D P = ViewportFrac(StillAt);
			Pending.Add({ FInputEvent::EType::Move, P, EKeys::Invalid });
			Pending.Add({ FInputEvent::EType::Down, P, EKeys::RightMouseButton });
			for (int32 K = 1; K <= 8; ++K)
			{
				Pending.Add({ FInputEvent::EType::Move, P + FVector2D(25.0 * K, 0.0), EKeys::Invalid });
			}
		},
		[this, StillAt]()
		{
			if (!bLineQueued)
			{
				if (Timer >= 0.6f)
				{
					bLineQueued = true;
					QueueUp(ViewportFrac(StillAt) + FVector2D(200.0, 0.0), EKeys::RightMouseButton);
				}
				return false;
			}
			return InputQuietFrames >= 3;
		},
		[this, CamFocus, LineUnchanged]()
		{
			const double MovedM = FVector2D::Distance(CamFocus(), LineCamCm) / 100.0;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST direito: a câmera andou %.1f m com o direito arrastado 200 px"), MovedM);
			const FString Why = LineUnchanged(false);
			if (!Why.IsEmpty())
			{
				return Why;
			}
			return MovedM > 1.0 ? FString() : FString::Printf(TEXT("o direito arrastado não rolou a câmera (%.2f m)"), MovedM);
		},
		0.1f, 10.0f, false });

	Steps.Add({ TEXT("camera_volta"),
		[this]()
		{
			// a câmera de antes da rolagem, pra caixa achar os tanques
			if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
			{
				Cam->JumpTo(LineCamCm);
			}
		},
		nullptr, nullptr, 0.6f, 5.0f, false });

	Steps.Add({ TEXT("caixa_com_tropa"),
		[this, LineSnapshot]()
		{
			// o defeito que o Felipe contou: com todos selecionados, o esquerdo arrastado em volta só da metade da esquerda (na tela)
			// seleciona só eles, e caixa não dá ordem
			LineSnapshot();
			LineIds.Reset();
			URTSMatchSubsystem* M = Match();
			ARTSPlayerController* P = PC();
			const FRTSBakedMap* Map = M ? M->BakedMap() : nullptr;
			if (!M || !P || !Map)
			{
				return;
			}
			struct FOnScreen
			{
				uint32 Id = 0;
				FVector2D S = FVector2D::ZeroVector;
			};
			TArray<FOnScreen> Sel;
			for (const uint32 Id : LineSelection)
			{
				FRTSUnitState U;
				FVector2D S;
				if (M->GetUnit(Id, U) && !U.bStructure && ScreenOf(U.PosCm, S, 150.0f))
				{
					Sel.Add({ Id, S });
				}
			}
			Sel.Sort([](const FOnScreen& A, const FOnScreen& B) { return A.S.X < B.S.X; });
			if (Sel.Num() < 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS TEST caixa com tropa: só %d da seleção na tela"), Sel.Num());
				return;
			}
			const int32 Half = Sel.Num() / 2;
			const double Margin = FMath::Clamp((Sel[Half].S.X - Sel[Half - 1].S.X) * 0.4, 4.0, 20.0);
			FBox2D Box(ForceInit);
			for (int32 K = 0; K < Half; ++K)
			{
				Box += Sel[K].S;
			}
			Box = Box.ExpandBy(Margin);
			// o que a caixa deve pegar, pela regra dela (HandleBoxSelect): toda unidade nossa que anda, com o ponto a 1,5 m do chão dentro
			TArray<FRTSUnitState> All;
			M->GetUnits(All);
			for (const FRTSUnitState& U : All)
			{
				FVector2D S;
				if (U.Owner == M->LocalPlayer() && !U.bStructure && P->ProjectWorldLocationToScreen(FVector(U.PosCm.X, U.PosCm.Y, Map->GroundZCm(U.PosCm) + 150.0f), S) && Box.IsInside(S))
				{
					LineIds.Add(U.Id);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("RTS TEST caixa com tropa: %d selecionados na tela, a caixa de (%.0f, %.0f) a (%.0f, %.0f) deve pegar %d"),
				Sel.Num(), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, LineIds.Num());
			QueueDrag(Box.Min, Box.Max);
		},
		InputDone,
		[this]()
		{
			ARTSPlayerController* P = PC();
			if (!P)
			{
				return FString(TEXT("sem jogador"));
			}
			TArray<uint32> Now = P->GetSelection();
			TArray<uint32> Want = LineIds;
			Now.Sort();
			Want.Sort();
			UE_LOG(LogTemp, Display, TEXT("RTS TEST caixa com tropa: selecionou %d (queria %d, de %d antes), ordens %d → %d"),
				Now.Num(), Want.Num(), LineSelection.Num(), LineOrders, P->OrdersGiven());
			if (P->OrdersGiven() != LineOrders)
			{
				return FString::Printf(TEXT("a caixa deu ordem (%d)"), P->OrdersGiven() - LineOrders);
			}
			if (Want.IsEmpty() || Want.Num() >= LineSelection.Num())
			{
				return FString::Printf(TEXT("não deu pra montar a caixa de metade (%d de %d)"), Want.Num(), LineSelection.Num());
			}
			return Now == Want ? FString() : FString::Printf(TEXT("a caixa com tropa selecionada pegou %d em vez de %d"), Now.Num(), Want.Num());
		},
		0.5f, 10.0f });

	// todos de novo, pros passos seguintes (o minimapa manda o exército inteiro)
	Steps.Add({ TEXT("caixa_todos"), QueueArmyBox, InputDone, CheckArmySelected, 1.0f });

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
				const FName Id(*FString::Printf(TEXT("eco_build_%d"), DefIndex(Key)));
				const FVector2D At = ButtonCenter(Id);
				UE_LOG(LogTemp, Warning, TEXT("RTS TEST o clique no botão de %s não pegou; de novo em (%.0f, %.0f)"), *Key, At.X, At.Y);
				if (At.X < 0.0f)
				{
					// o botão nem está no painel: o que está selecionado e o que o painel mostra
					const ARTSHUD* HUD = PC() ? Cast<ARTSHUD>(PC()->GetHUD()) : nullptr;
					UE_LOG(LogTemp, Warning, TEXT("RTS TEST   %s não está no painel: %d selecionados, prédio %u, posicionando %d; botões: %s"), *Id.ToString(),
						PC() ? PC()->GetSelection().Num() : -1, PC() ? PC()->SelectedStructure() : 0u, PC() ? PC()->GetPlacingDef() : -2, HUD ? *HUD->ButtonIdsForTest() : TEXT("(sem HUD)"));
				}
				QueueClick(At);
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

void URTSAutoTest::BuildScenario()
{
	// a câmera parada no foco do cenário, e as fotos nos segundos de simulação (não nos do relógio: a
	// mesma foto em toda rodada)
	Steps.Add({ TEXT("cena_camera"),
		[this]()
		{
			const FRTSScenario* Sc = Match() ? Match()->GetScenario() : nullptr;
			ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
			if (Sc && Cam)
			{
				Cam->SetView(Sc->CameraYawDeg, Sc->CameraDistanceM, Sc->CameraPitchDeg, Sc->CameraLookUpDeg);
				Cam->JumpTo(Sc->CameraFocusM * 100.0);
				if (!Sc->bHud && PC()->GetHUD())
				{
					PC()->GetHUD()->bShowHUD = false;   // (a foto sem o HUD, lado a lado com a maquete)
				}
				// o sol é posto pela câmera do começo: com a câmera do cenário girada, a receita de novo, pra o sol
				// cair onde o jogador o vê na câmera dele (o Look, 13/10: a foto saía com o sol 90° fora)
				for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)
				{
					if (Sc->bSunRel && It->Recipe)
					{
						// (16/10) o sol a sun_rel_deg da frente da câmera da cena: o diretor põe o sol pela câmera (a frente
						// girada FromCamera pra esquerda), então ele lê um giro de mentira e a câmera volta pro dela
						Cam->SetView(Sc->CameraYawDeg + Sc->SunRelDeg + It->Recipe->Sun.FromCamera, Sc->CameraDistanceM, Sc->CameraPitchDeg, Sc->CameraLookUpDeg);
						It->ApplyRecipe();
						Cam->SetView(Sc->CameraYawDeg, Sc->CameraDistanceM, Sc->CameraPitchDeg, Sc->CameraLookUpDeg);
						Cam->JumpTo(Sc->CameraFocusM * 100.0);
					}
					else
					{
						It->ApplyRecipe();
					}
				}
				bSceneCameraSet = true;
			}
		},
		[this]() { return Match() && Match()->IsReady() && Match()->GetScenario(); },
		[this]() { return Match() && Match()->GetScenario() ? FString() : TEXT("o cenário não montou"); },
		0.2f, 60.0f, false });
	// (v4, 23/10) "select" no cenário: o prédio do jogador local daquele tipo selecionado por um clique de verdade, antes da
	// 1ª foto (o HUD da zona de lançamento selecionada: o relógio do avião). O campo é lido aqui, só pro teste.
	// (revisão de 23/10) "clicks": "repair_sell" = o conserto e o vender pelos cliques (AddRepairAndSellSteps) antes das
	// fotos da cena: saíram da base (vendiam a única usina dela e a vitrine, a energia, a batalha e a numeração das fotos
	// mudavam pros outros donos)
	FString SelectKey;
	FString ClicksFlow;
	{
		FString Name;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), Name)
			&& FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Scenarios.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()
			&& Root->TryGetArrayField(TEXT("scenarios"), List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
				FString Each;
				if (O.IsValid() && O->TryGetStringField(TEXT("name"), Each) && Each == Name)
				{
					O->TryGetStringField(TEXT("select"), SelectKey);
					O->TryGetStringField(TEXT("clicks"), ClicksFlow);
				}
			}
		}
	}
	if (!SelectKey.IsEmpty())
	{
		Steps.Add({ TEXT("cena_selecionar"),
			[this]() { RetryAt = 0.0f; },
			[this, SelectKey]()
			{
				TArray<FRTSUnitState> F;
				if (!OwnOf(DefIndex(SelectKey), F) || F.IsEmpty() || !PC())
				{
					return false;
				}
				if (PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == F[0].Id)
				{
					return true;
				}
				FVector2D At;
				if (Timer >= RetryAt && Pending.IsEmpty() && ScreenOf(F[0].PosCm, At, 60.0f))
				{
					RetryAt = Timer + 0.8f;
					QueueClick(At);
				}
				return false;
			},
			[this, SelectKey]()
			{
				TArray<FRTSUnitState> F;
				return OwnOf(DefIndex(SelectKey), F) && !F.IsEmpty() && PC() && PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == F[0].Id
					? FString() : FString::Printf(TEXT("o clique não selecionou %s"), *SelectKey);
			},
			0.5f, 10.0f, false });
	}
	if (ClicksFlow == TEXT("repair_sell"))
	{
		AddRepairAndSellSteps();
	}
	const FRTSScenario* Sc = nullptr;
	FRTSScenario Loaded;
	FString ScenarioName;
	FString Error;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), ScenarioName) && FRTSGameData::LoadScenario(ScenarioName, Loaded, Error))
	{
		Sc = &Loaded;
	}
	TArray<float> Shots = Sc ? Sc->ShotsS : TArray<float>();
	const float EndS = Sc ? Sc->EndS : 30.0f;
	for (const float T : Shots)
	{
		const uint32 Tick = uint32(FMath::RoundToInt(T * 30.0f));
		Steps.Add({ FString::Printf(TEXT("cena_%03ds"), FMath::RoundToInt(T)),
			nullptr,
			[this, Tick]() { return Match() && Match()->SimTick() >= Tick; },
			nullptr,
			0.0f, 120.0f });
	}
	Steps.Add({ TEXT("cena_fim"),
		[this]()
		{
			if (Match())
			{
				Match()->DumpGroundMemory(Folder);
			}
		},
		[this, EndS]() { return Match() && Match()->SimTick() >= uint32(FMath::RoundToInt(EndS * 30.0f)); },
		nullptr,
		0.0f, 120.0f, false });
}

void URTSAutoTest::BuildAIvsAI()
{
	// (v4, 23/10) a prova da v4 numa partida de verdade, sem ninguém: a IA joga pelos dois lados (-RTSSelfAI). Uma foto a cada
	// 15 s de partida onde houve tiro por último (senão as bases e o meio, em rodízio); a cada ataque de mísseis novo a câmera
	// vai até ele e fotografa a 1,5, 3 e 5 s. Acaba em -RTSTestMinutes= (padrão 20) ou no fim da partida
	float Minutes = 20.0f;
	FParse::Value(FCommandLine::Get(), TEXT("-RTSTestMinutes="), Minutes);
	Steps.Add({ TEXT("ia_x_ia"),
		[this]()
		{
			WatchNextShot = 10.0f;
			WatchShotAt = -1.0f;
			WatchStrikeSerial = Match() ? Match()->GetStrikeSerial() : 0;
			WatchBurst.Reset();
			bWatchStarted = false;
		},
		[this, Minutes]() { return WatchTick() || (Match() && Match()->MatchSeconds() >= Minutes * 60.0f); },
		[this]()
		{
			const URTSMatchSubsystem* M = Match();
			UE_LOG(LogTemp, Display, TEXT("RTS TEST IA x IA: fim aos %.0f s, resultado %d, ataques de mísseis %d"),
				M ? M->MatchSeconds() : -1.0f, M ? int32(M->Result()) : -1, M ? M->GetStrikeSerial() : -1);
			return M ? FString() : TEXT("a partida sumiu");
		},
		0.0f, Minutes * 60.0f + 300.0f, false });
}

bool URTSAutoTest::WatchTick()
{
	URTSMatchSubsystem* M = Match();
	ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
	if (!M || !Cam)
	{
		return bWatchStarted;   // a partida acabou (ou ainda não começou)
	}
	bWatchStarted = true;
	if (M->Result() != ERTSMatchResult::Playing)
	{
		Shot(TEXT("fim"));
		return true;
	}
	const float T = M->MatchSeconds();
	// a foto marcada (meio segundo depois de a câmera chegar)
	if (WatchShotAt >= 0.0f && T >= WatchShotAt)
	{
		Shot(WatchShotName);
		WatchShotAt = -1.0f;
		return false;
	}
	auto Stamp = [T](const TCHAR* What) { return FString::Printf(TEXT("%s_%02dm%02ds"), What, int32(T) / 60, int32(T) % 60); };
	// o ataque de mísseis novo: a câmera vai até o ponto e a rajada de fotos
	FVector2D Spot;
	float At = 0.0f;
	if (M->GetStrikeSerial() != WatchStrikeSerial && M->GetLastStrike(Spot, At))
	{
		WatchStrikeSerial = M->GetStrikeSerial();
		// a foto do rodízio que esperava a câmera chegar (base, meio, luta) não sai mais: a câmera foi pro ataque, e ela
		// gravaria o ataque com o nome do outro lugar
		WatchShotAt = -1.0f;
		WatchShotName.Reset();
		Cam->JumpTo(Spot);
		WatchBurst = { T + 1.5f, T + 3.0f, T + 5.0f };
		WatchNextShot = T + 8.0f;
		UE_LOG(LogTemp, Display, TEXT("RTS TEST IA x IA: ataque de mísseis em (%.0f, %.0f) m aos %.0f s"), Spot.X / 100.0, Spot.Y / 100.0, T);
		return false;
	}
	if (WatchBurst.Num() && T >= WatchBurst[0])
	{
		WatchBurst.RemoveAt(0);
		Shot(Stamp(TEXT("ataque")));
		return false;
	}
	if (WatchBurst.IsEmpty() && T >= WatchNextShot && WatchShotAt < 0.0f)
	{
		WatchNextShot = T + 15.0f;
		// onde houve tiro nos últimos 5 s; senão o rodízio: a base de cada um e o meio
		if (M->GetLastHit(Spot, At) && T - At < 5.0f)
		{
			Cam->JumpTo(Spot);
			WatchShotName = Stamp(TEXT("luta"));
		}
		else
		{
			const FVector2D Mine = M->PlayerStartCm(M->LocalPlayer());
			const FVector2D Theirs = M->PlayerStartCm(M->LocalPlayer() == 0 ? 1 : 0);
			const int32 K = WatchCycle++ % 3;
			Cam->JumpTo(K == 0 ? Mine : K == 1 ? (Mine + Theirs) * 0.5 : Theirs);
			WatchShotName = Stamp(K == 0 ? TEXT("base_azul") : K == 1 ? TEXT("meio") : TEXT("base_vermelha"));
		}
		WatchShotAt = T + 0.5f;
	}
	return false;
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
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSTestPatriot")))
	{
		// (17/10 noite, o Felipe: "vc nem se quer construiu a porra do patriot na partida") o Patriot pelo Dozer, como ele faz, e
		// a troca de qualidade dele no meio da partida (Alta → Baixa → Alta, as teclas F5 e F7): as faixas cinza no chão
		AddBuildSteps(TEXT("patriot"), TEXT("usa_patriot"), ESpot::Front);
		Steps.Add({ TEXT("qualidade_baixa"), [this]() { QueueKey(EKeys::F5); }, []() { return true; }, []() { return FString(); }, 4.0f, 10.0f });
		Steps.Add({ TEXT("qualidade_alta"), [this]() { QueueKey(EKeys::F7); }, []() { return true; }, []() { return FString(); }, 4.0f, 10.0f });
		Steps.Add({ TEXT("qualidade_alta_depois"), nullptr, []() { return true; }, []() { return FString(); }, 6.0f, 10.0f });
		// e fica olhando o Patriot na vista do jogo durante o ataque da IA (no modo rápido ela chega em ~1:30; 17/10: "eu vi a
		// patriot bugando, vc nem viu pq vc saiu da tela"), uma foto a cada 2 s
		Steps.Add({ TEXT("patriot_guarda"),
			[this]()
			{
				TArray<FRTSUnitState> Pat;
				ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
				if (Cam && OwnOf(DefIndex(TEXT("usa_patriot")), Pat))
				{
					Cam->FocusOn(Pat[0].PosCm);
				}
			},
			[]() { return true; }, []() { return FString(); }, 1.0f, 5.0f });
		for (int32 K = 0; K < 24; ++K)
		{
			Steps.Add({ FString::Printf(TEXT("patriot_guarda_%02d"), K), nullptr, []() { return true; }, []() { return FString(); }, 2.0f, 5.0f });
		}
	}

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
	// zera também quando o evento entregue agora era o único da fila (senão o passo confere antes do controle ler)
	const bool bPumped = !Pending.IsEmpty();
	PumpInput();
	InputQuietFrames = bPumped ? 0 : InputQuietFrames + 1;
	// o cenário com a câmera deslizando (pan_m_s): o foco anda com o relógio da simulação, depois da foto do começo
	if (bScenario && Index > 0)
	{
		const FRTSScenario* Sc = Match() ? Match()->GetScenario() : nullptr;
		ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr;
		if (Sc && Cam && !Sc->CameraPanMS.IsZero() && Match()->IsReady())
		{
			Cam->JumpTo((Sc->CameraFocusM + Sc->CameraPanMS * Match()->MatchSeconds()) * 100.0);
		}
		// (16/10) a câmera que segue uma unidade (camera.follow_unit): o foco alcança o tanque desenhado em follow_lag_s
		if (Sc && Cam && Sc->FollowUnit >= 0 && Match()->IsReady())
		{
			if (FollowId == 0)
			{
				// a N-ésima unidade (não prédio) do jogador, na ordem da criação: a mesma conta da ordem "unit"
				TArray<FRTSUnitState> All;
				Match()->GetUnits(All);
				int32 K = 0;
				for (const FRTSUnitState& Un : All)
				{
					if (Un.Owner == Sc->FollowPlayer && !Un.bStructure && K++ == Sc->FollowUnit)
					{
						FollowId = Un.Id;
						break;
					}
				}
			}
			if (const ARTSUnitActor* A = FollowId != 0 ? Match()->ViewOf(FollowId) : nullptr)
			{
				const FVector2D At(A->GetActorLocation().X, A->GetActorLocation().Y);
				FollowCm = bFollowInit ? FollowCm + (At - FollowCm) * (1.0f - FMath::Exp(-DeltaTime / FMath::Max(Sc->FollowLagS, 0.01f))) : At;
				bFollowInit = true;
				// (16/10) o chão do tanque no ponto da tela pedido (follow_screen): o foco anda pra frente pela inclinação
				// (o raio desse ponto da tela desce mais) e pro lado pelo campo de visão, na profundidade do tanque
				FVector2D FocusCm = FollowCm;
				if (!Sc->FollowScreen.Equals(FVector2D(0.5, 0.5), 0.001))
				{
					const float P = FMath::DegreesToRadians(FMath::Max(Cam->GetViewPitchDeg(), 0.5f));
					const float H = Cam->GetViewDistanceM() * 100.0f * FMath::Sin(P);
					const float TanH = FMath::Tan(FMath::DegreesToRadians(Cam->GetFieldOfViewDeg() * 0.5f));
					FVector2D Vp(16.0, 9.0);
					if (GEngine && GEngine->GameViewport)
					{
						GEngine->GameViewport->GetViewportSize(Vp);
					}
					const float TanV = TanH * float(Vp.Y / FMath::Max(Vp.X, 1.0));
					const float Below = FMath::Atan(float(Sc->FollowScreen.Y - 0.5) * 2.0f * TanV);
					const float Dep = FMath::Max(P + Below, FMath::DegreesToRadians(0.5f));
					const float Lt = H / FMath::Tan(Dep), Lf = H / FMath::Tan(P);
					const float Depth = FMath::Sqrt(H * H + Lt * Lt) * FMath::Cos(Below);
					const float Dr = float(Sc->FollowScreen.X - 0.5) * 2.0f * TanH * Depth;
					const float Yaw = FMath::DegreesToRadians(Cam->GetViewYawDeg());
					const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw)), Right(-FMath::Sin(Yaw), FMath::Cos(Yaw));
					FocusCm = FollowCm + Fwd * double(Lf - Lt) - Right * double(Dr);
				}
				Cam->JumpTo(FocusCm);
			}
		}
		// a câmera que anda (camera.keys): giro, distância e inclinação pelos segundos da simulação, entre os pontos
		if (Sc && Cam && Sc->CameraKeys.Num() > 0 && Match()->IsReady())
		{
			const float T = Match()->MatchSeconds();
			const TArray<FRTSScenarioCameraKey>& K = Sc->CameraKeys;
			int32 B = 0;
			while (B < K.Num() && K[B].AtS <= T)
			{
				++B;
			}
			if (B > 0)
			{
				const FRTSScenarioCameraKey& P0 = K[B - 1];
				const FRTSScenarioCameraKey& P1 = K[FMath::Min(B, K.Num() - 1)];
				const float A = P1.AtS > P0.AtS ? FMath::Clamp((T - P0.AtS) / (P1.AtS - P0.AtS), 0.0f, 1.0f) : 1.0f;
				const float S = FMath::SmoothStep(0.0f, 1.0f, A);
				const float Yaw = P0.YawDeg + FMath::FindDeltaAngleDegrees(P0.YawDeg, P1.YawDeg) * S;
				// a inclinação: com as duas na curva do zoom, fica na curva; senão, a da curva entra pelo valor que ela dá. (16/10)
				// O olhar pra cima (look_up_deg) entra como inclinação negativa: deitar até o horizonte e subir é uma linha só
				const auto Eff = [Cam](const FRTSScenarioCameraKey& Kk)
				{
					return Kk.LookUpDeg > 0.0f ? -Kk.LookUpDeg : (Kk.PitchDeg < 0.0f ? Cam->CurvePitchAt(Kk.DistanceM) : Kk.PitchDeg);
				};
				const bool bCurve = P0.PitchDeg < 0.0f && P1.PitchDeg < 0.0f && P0.LookUpDeg <= 0.0f && P1.LookUpDeg <= 0.0f;
				const float Pitch = bCurve ? -1.0f : FMath::Lerp(Eff(P0), Eff(P1), S);
				Cam->SetView(Yaw, FMath::Lerp(P0.DistanceM, P1.DistanceM, S), Pitch < 0.0f ? -1.0f : Pitch, bCurve || Pitch >= 0.0f ? 0.0f : -Pitch);
			}
		}
	}
	if (bRecord && !bRecordDone)
	{
		URTSMatchSubsystem* M = Match();
		// (a gravação começa no 1º tiro; com a câmera que anda, já no 1º ponto dela: o vídeo do céu não tem tiro)
		const FRTSScenario* RecSc = M ? M->GetScenario() : nullptr;
		// (16/10) a cena sem tiro nem câmera que anda (os vídeos da poeira): grava com a cena pronta, até o end_s
		const bool bCamRec = RecSc && M->IsReady() && bSceneCameraSet && (RecSc->CameraKeys.Num() == 0 || M->MatchSeconds() >= RecSc->CameraKeys[0].AtS);
		if (M && (M->GetShotsFired() > 0 || bCamRec) && RecordFrames < 2400 && !FScreenshotRequest::IsScreenshotRequested())
		{
			// (17/10) a gravação sem travar a placa (RTSFrameRecorder): o teste gravado rodava a 7–12 s de relógio por s de jogo
			const FString Frames = FPaths::Combine(Folder, TEXT("frames"));
			if (RecordFrames == 0 && RTSFrameRecorder::WantFast())
			{
				IFileManager::Get().MakeDirectory(*Frames, true);
				RTSFrameRecorder::Start();
			}
			if (RTSFrameRecorder::IsActive())
			{
				RTSFrameRecorder::Capture(FPaths::Combine(Frames, FString::Printf(TEXT("f_%05d.jpg"), RecordFrames++)));
			}
			else
			{
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(Frames, FString::Printf(TEXT("f_%05d.png"), RecordFrames++)), true, false);
			}
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
			// com base, sem a base de pé o resto do teste não diz nada: encerra logo e libera a placa (o chat não é a base)
			const int32 ArmyStep = Steps.IndexOfByPredicate([](const FStep& X) { return X.Name == TEXT("caixa_de_selecao"); });
			if (bBase && Index < ArmyStep && !S.Name.StartsWith(TEXT("chat_")))
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
	RTSFrameRecorder::Stop();   // (17/10) os quadros que ainda estão em voo ou comprimindo
	FPlatformMisc::RequestExit(false);
}

void URTSAutoTest::AddRepairAndSellSteps()
{
	// (v4, 23/10) o conserto do Dozer e o vender pelo fluxo de verdade (cliques): os tanques avariam a própria usina (o Ctrl; a
	// ordem vai direto, é o preparo do teste), o Dozer é selecionado no clique, o cursor vira a chave sobre a usina avariada, o
	// clique manda consertar e a vida sobe; depois a usina é selecionada e vendida pelo botão Sell do painel: ela sai da seleção,
	// desmonta e some, e o dinheiro sobe
	const auto Camera = [this](const FVector2D& Cm)
	{
		if (ARTSCameraPawn* Cam = PC() ? Cast<ARTSCameraPawn>(PC()->GetPawn()) : nullptr)
		{
			Cam->JumpTo(Cm);
		}
	};
	const auto Plant = [this](FRTSUnitState& Out)
	{
		return Match() && RepairPlant != 0 && Match()->GetUnit(RepairPlant, Out);
	};
	Steps.Add({ TEXT("conserto_avaria"),
		[this, Camera]()
		{
			TArray<FRTSUnitState> P, T;
			RepairPlant = OwnOf(DefIndex(TEXT("usa_power_plant")), P) ? P[0].Id : 0;
			if (RepairPlant != 0 && OwnOf(DefIndex(TEXT("usa_mbt")), T) && Match())
			{
				Camera(P[0].PosCm);
				TArray<uint32> Ids;
				for (const FRTSUnitState& U : T)
				{
					Ids.Add(U.Id);
				}
				Match()->OrderAttack(Ids, RepairPlant, /*bForced=*/true);
			}
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			if (!Plant(U) || U.Hp > U.MaxHp * 6 / 10)
			{
				return false;
			}
			TArray<FRTSUnitState> T;
			TArray<uint32> Ids;
			OwnOf(DefIndex(TEXT("usa_mbt")), T);
			for (const FRTSUnitState& Tank : T)
			{
				Ids.Add(Tank.Id);
			}
			Match()->OrderStop(Ids);
			return true;
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			return Plant(U) && U.Hp < U.MaxHp ? FString() : TEXT("a usina não avariou");
		},
		1.0f, 40.0f });
	Steps.Add({ TEXT("conserto_dozer"),
		[this]() { RetryAt = 0.0f; },
		[this]()
		{
			TArray<FRTSUnitState> D;
			if (!OwnOf(DefIndex(TEXT("usa_dozer")), D) || !PC())
			{
				return false;
			}
			RepairDozer = D[0].Id;
			if (PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == RepairDozer)
			{
				return true;
			}
			FVector2D S;
			if (Timer >= RetryAt && Pending.IsEmpty())
			{
				RetryAt = Timer + 0.8f;
				if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC()->GetPawn()))
				{
					Cam->JumpTo(D[0].PosCm);
				}
				if (ScreenOf(D[0].PosCm, S, 150.0f))
				{
					QueueClick(S);
				}
			}
			return false;
		},
		[this]() { return PC() && PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == RepairDozer ? FString() : TEXT("o clique não selecionou o Dozer"); },
		0.8f, 8.0f });
	Steps.Add({ TEXT("conserto_cursor"),
		[this, Camera, Plant]()
		{
			FRTSUnitState U;
			if (Plant(U))
			{
				Camera(U.PosCm);
			}
			RetryAt = 0.5f;
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			FVector2D S;
			if (Timer >= RetryAt && Pending.IsEmpty() && Plant(U) && ScreenOf(U.PosCm, S, 300.0f))
			{
				RetryAt = Timer + 0.5f;
				Pending.Add({ FInputEvent::EType::Move, S, EKeys::Invalid });   // só passa o mouse por cima
			}
			return PC() && PC()->IsRepairCursorShown();
		},
		[this]() { return PC() && PC()->IsRepairCursorShown() ? FString() : TEXT("o cursor não virou a chave do conserto sobre a usina avariada"); },
		0.5f, 6.0f });
	Steps.Add({ TEXT("conserto_clique"),
		[this, Plant]()
		{
			FRTSUnitState U;
			FVector2D S;
			if (Plant(U) && ScreenOf(U.PosCm, S, 300.0f))
			{
				RepairHpStart = U.Hp;
				QueueClick(S);
			}
		},
		[this]()
		{
			FRTSUnitState D;
			return Match() && Match()->GetUnit(RepairDozer, D) && D.Task == uint8(Sim::UnitTask::Repair);
		},
		[this]()
		{
			FRTSUnitState D;
			return Match() && Match()->GetUnit(RepairDozer, D) && D.Task == uint8(Sim::UnitTask::Repair) ? FString() : TEXT("o clique não mandou o Dozer consertar");
		},
		0.5f, 5.0f });
	Steps.Add({ TEXT("conserto_sobe"), nullptr,
		[this, Plant]()
		{
			FRTSUnitState U;
			return Plant(U) && U.Hp >= RepairHpStart + 50;
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			const bool bOk = Plant(U) && U.Hp >= RepairHpStart + 50;
			UE_LOG(LogTemp, Display, TEXT("RTS TEST conserto: a usina de %d a %d de %d PV"), RepairHpStart, bOk ? U.Hp : -1, bOk ? U.MaxHp : -1);
			return bOk ? FString() : TEXT("a vida da usina não subiu com o Dozer");
		},
		1.0f, 60.0f });
	// vender: o direito desfaz a seleção (o Dozer), o clique pega a usina e o botão Sell do painel vende
	Steps.Add({ TEXT("vender_selecionar"),
		[this]()
		{
			RetryAt = 0.0f;
			if (PC())
			{
				QueueClick(ViewportFrac(FVector2D(0.5f, 0.12f)), EKeys::RightMouseButton);
			}
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			if (!PC() || !Plant(U))
			{
				return false;
			}
			if (PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == RepairPlant)
			{
				return true;
			}
			FVector2D S;
			if (Timer >= RetryAt + 0.4f && Pending.IsEmpty() && PC()->GetSelection().IsEmpty() && ScreenOf(U.PosCm, S, 300.0f))
			{
				RetryAt = Timer + 0.8f;
				QueueClick(S);
			}
			else if (Timer >= RetryAt + 0.4f && Pending.IsEmpty() && !PC()->GetSelection().IsEmpty() && PC()->GetSelection()[0] != RepairPlant)
			{
				RetryAt = Timer + 0.8f;
				QueueClick(ViewportFrac(FVector2D(0.5f, 0.12f)), EKeys::RightMouseButton);
			}
			return false;
		},
		[this]() { return PC() && PC()->GetSelection().Num() == 1 && PC()->GetSelection()[0] == RepairPlant ? FString() : TEXT("o clique não selecionou a usina"); },
		0.8f, 8.0f });
	Steps.Add({ TEXT("vender_botao"),
		[this]()
		{
			SellMoneyStart = Match() ? Match()->Money(Match()->LocalPlayer()) : 0;
			QueueClick(ButtonCenter(TEXT("cmd_sell")));
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			return !Plant(U) || U.bSelling;
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			const bool bSelling = Plant(U) && U.bSelling;
			const bool bDeselected = PC() && !PC()->GetSelection().Contains(RepairPlant);
			return bSelling && bDeselected ? FString() : FString::Printf(TEXT("o botão Sell não vendeu (vendendo %d, fora da seleção %d)"), int32(bSelling), int32(bDeselected));
		},
		0.5f, 5.0f });
	Steps.Add({ TEXT("vender_desmonta"), nullptr, []() { return true; }, nullptr, 3.0f, 5.0f });
	Steps.Add({ TEXT("vender_fim"), nullptr,
		[this, Plant]()
		{
			FRTSUnitState U;
			return !Plant(U);
		},
		[this, Plant]()
		{
			FRTSUnitState U;
			FRTSUnitState D;
			const int32 Now = Match() ? Match()->Money(Match()->LocalPlayer()) : 0;
			// o Dozer que consertava larga a usina vendida (o canAcceptNewRepair do ZH: o prédio vendido sai do conserto)
			const bool bDozerFree = !Match() || !Match()->GetUnit(RepairDozer, D) || D.Task != uint8(Sim::UnitTask::Repair);
			UE_LOG(LogTemp, Display, TEXT("RTS TEST vender: a usina saiu; o dinheiro de %d pra %d; o Dozer largou o conserto: %d"), SellMoneyStart, Now, int32(bDozerFree));
			return !Plant(U) && Now >= SellMoneyStart + 400 && bDozerFree ? FString() : TEXT("a usina vendida não saiu, o dinheiro não voltou ou o Dozer seguiu consertando");
		},
		0.5f, 10.0f });
}
