#include "RTSPlayerController.h"

#include "RTSNetSession.h"

#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "RTSCameraPawn.h"
#include "RTSChat.h"
#include "LookDirector.h"
#include "EngineUtils.h"
#include "RTSGameData.h"
#include "RTSGameModes.h"
#include "RTSHUD.h"
#include "RTSLab.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSSessionSubsystem.h"
#include "Scalability.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

// (29/10) o chão de hoje de manhã (1) × o de agora (0): o F12 troca; a coleção MPC_Ground (GroundV) vai junto no PlayerTick
static IConsoleVariable* CVarGroundBefore = IConsoleManager::Get().RegisterConsoleVariable(TEXT("rts.Ground.Before"), 0,
	TEXT("O chão de hoje de manhã (1) ou o de agora (0); o F12 troca"), ECVF_Default);

namespace
{
	constexpr float DragThresholdPx = 6.0f;
	constexpr double DoubleClickSeconds = 0.3;
	// Câmera (valores do Vanguard, ADR-0003, que o Felipe testou e aprovou em 07/2026)
	constexpr float EdgeScrollPx = 16.0f;
	constexpr float EdgeScrollBoost = 1.6f;
	constexpr float VerticalScrollBias = 1.25f;
	constexpr float DragRefPx = 180.0f;          // arrasto do direito: 180 px = velocidade normal
	constexpr float DragMaxFactor = 2.5f;
	constexpr float RotateDegPerSec = 140.0f;    // setas e botão do meio
	constexpr float RotateRefPx = 180.0f;
	constexpr float RotateMaxFactor = 2.5f;
	constexpr float KeyboardZoomStepsPerSec = 6.0f;
	constexpr float MarkerSeconds = 0.7f;
	constexpr float PickHeightCm = 150.0f;
	// (v4, 23/10) a linha dos dois botões (o acorde): o segundo botão ainda forma o acorde até 0,2 s depois do esquerdo (a
	// caixa ainda não fez nada); soltar os dois tem 0,25 s de folga (a mão nunca solta no mesmo quadro: 1 a 5 quadros); a
	// linha mede de 1 m até o teto do MoveValue (63 passos de 2 m)
	constexpr double ChordPressGraceSeconds = 0.2;
	constexpr double ChordReleaseGraceSeconds = 0.25;
	constexpr float MinLineM = 1.0f;
	constexpr float MaxLineM = 126.0f;

	/** As teclas 1–9 (dentro de função: as FKey do InputCore são estáticas de outro módulo). */
	const FKey& GroupKey(int32 K)
	{
		static const FKey Keys[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
		return Keys[K];
	}
}

ARTSPlayerController::ARTSPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ARTSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bMenu = !GetWorld()->GetAuthGameMode() || GetWorld()->GetAuthGameMode()->IsA<ARTSMenuGameMode>();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	// na partida o cursor fica preso na janela, pra rolar pela borda (como no ZH)
	Mode.SetLockMouseToViewportBehavior(bMenu ? EMouseLockMode::DoNotLock : EMouseLockMode::LockAlways);
	SetInputMode(Mode);
	if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn()))
	{
		Cam->SetMenuMode(bMenu);
	}
	if (!bMenu)
	{
		RTSChat::Attach(this);   // (17/10) o chat da partida: o Enter abre a caixa (RTSChat.h)
	}
	// a primeira vez que o jogo abre num PC (14/10, o notebook do pai do Felipe): a Média, a que rodou bem na v1; depois,
	// o que o jogador escolheu no Esc fica no GameUserSettings.ini e a Unreal já abre nele (o teste, com o -ExecCmds
	// "Scalability N", fica de fora)
	{
		FString Exec;
		const bool bTestLevels = FParse::Value(FCommandLine::Get(), TEXT("ExecCmds="), Exec) && Exec.Contains(TEXT("Scalability"));
		int32 Saved = -1;
		if (!bTestLevels && !GConfig->GetInt(TEXT("RTS"), TEXT("QualityPreset"), Saved, GGameUserSettingsIni))
		{
			ApplyQuality(1);
		}
	}
	// a luz do Look no preset da Unreal de agora (o -ExecCmds "Scalability N" pode chegar depois do GameMode)
	const int32 Level = FMath::Clamp(Scalability::GetQualityLevels().EffectsQuality, 1, 4);
	for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)
	{
		It->ApplyPreset(Level);
	}
}

URTSMatchSubsystem* ARTSPlayerController::Match() const
{
	URTSMatchSubsystem* M = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	return M && M->IsRunning() ? M : nullptr;
}

ARTSHUD* ARTSPlayerController::GetRTSHUD() const
{
	return Cast<ARTSHUD>(GetHUD());
}

bool ARTSPlayerController::IsDragging(FVector2D& OutA, FVector2D& OutB) const
{
	float X = 0.0f;
	float Y = 0.0f;
	// (v4) o esquerdo arrastado é sempre a caixa (com tropa selecionada também); no acorde dos dois botões, não
	if (!bLeftDown || LineChord != ELineChord::None || !ReadMouse(X, Y))
	{
		return false;
	}
	OutA = LeftDownPos;
	OutB = FVector2D(X, Y);
	return FVector2D::Distance(OutA, OutB) > DragThresholdPx;
}

void ARTSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (bMenu)
	{
		// (01/11) a nossa seta já no menu (ela fica no lugar da do Windows em toda a tela)
		int32 ViewW = 0, ViewH = 0;
		GetViewportSize(ViewW, ViewH);
		CurrentMouseCursor = Cursors.Apply(ERTSCursor::Pointer, ViewH);
		TickMenu();
	}
	else
	{
		TickMatch(DeltaTime);
	}
	{
		// (17/10) o teste do chat: o robô da LAN manda -RTSTestChat=<texto> (o _ vira espaço) uma vez, com 5 s de partida
		static bool bTestChatSent = false;
		FString Say;
		const URTSMatchSubsystem* M = bMenu ? nullptr : Match();
		if (!bTestChatSent && M && M->IsRunning() && M->MatchSeconds() > 5.0f && FParse::Value(FCommandLine::Get(), TEXT("-RTSTestChat="), Say))
		{
			bTestChatSent = true;
			if (URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>())
			{
				Net->SendChat(Say.Replace(TEXT("_"), TEXT(" ")), false);
			}
		}
	}
	// (14/10, a vista que deitava no chão por ~1 s quando um tanque explodia: era o tremor movendo a câmera inteira) o
	// aviso, de guarda, quando a vista do jogo sai da câmera do jogador
	if (!bMenu && PlayerCameraManager)
	{
		const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn());
		const FVector ViewAt = PlayerCameraManager->GetCameraLocation();
		const FRotator ViewRot = PlayerCameraManager->GetCameraRotation();
		if (Cam && (GetViewTarget() != Cam || FVector::Dist(ViewAt, Cam->GetActorLocation()) > 2000.0))
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS vista fora da câmera: alvo %s, vista (%.0f, %.0f, %.0f) inclinação %.1f giro %.1f, câmera (%.0f, %.0f, %.0f) %s"),
				*GetNameSafe(GetViewTarget()), ViewAt.X, ViewAt.Y, ViewAt.Z, ViewRot.Pitch, ViewRot.Yaw,
				Cam->GetActorLocation().X, Cam->GetActorLocation().Y, Cam->GetActorLocation().Z, *Cam->GetActorRotation().ToString());
		}
	}
	if (Selected.Num() != LoggedSelection)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS seleção: %d → %d"), LoggedSelection, Selected.Num());
		LoggedSelection = Selected.Num();
	}
	{
		// (17/10 noite, o teste das faixas cinza no chão: o Felipe trocou Alta → Baixa → Alta no meio da partida) -RTSTestQualityAt=
		// <s>:<nível>,<s>:<nível> troca o preset nesses segundos de partida, como as teclas F5–F8 (0 baixa … 3 ultra)
		static TArray<FVector2D> QualityAt = []
		{
			TArray<FVector2D> Out;
			FString Arg;
			if (FParse::Value(FCommandLine::Get(), TEXT("-RTSTestQualityAt="), Arg, false))
			{
				TArray<FString> Items;
				Arg.ParseIntoArray(Items, TEXT(","));
				for (const FString& It : Items)
				{
					FString S, L;
					if (It.Split(TEXT(":"), &S, &L))
					{
						Out.Add(FVector2D(FCString::Atof(*S), FCString::Atof(*L)));
					}
				}
			}
			return Out;
		}();
		const URTSMatchSubsystem* QM = bMenu ? nullptr : Match();
		if (QM && QM->IsRunning() && QualityAt.Num() && QM->MatchSeconds() >= QualityAt[0].X)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS TEST qualidade: preset %d aos %.1f s"), int32(QualityAt[0].Y), QM->MatchSeconds());
			ApplyQuality(int32(QualityAt[0].Y), false);
			QualityAt.RemoveAt(0);
		}
	}
	// presets: as teclas (o atalho) e a linha QUALITY do Esc e do menu
	const FKey QualityKeys[4] = { EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8 };
	for (int32 Q = 0; Q < 4; ++Q)
	{
		if (WasInputKeyJustPressed(QualityKeys[Q]))
		{
			ApplyQuality(Q);
		}
	}
	if (WasInputKeyJustPressed(EKeys::F4))
	{
		// o HUD novo do HUDLab x o de hoje (o teste do Felipe, 13/10)
		if (ARTSHUD* HUD = GetRTSHUD())
		{
			HUD->ToggleNewHud();
			UE_LOG(LogTemp, Log, TEXT("RTS HUD: %s"), HUD->IsNewHud() ? TEXT("o novo (HUDLab)") : TEXT("o de hoje"));
		}
	}
	if (WasInputKeyJustPressed(EKeys::F10))
	{
		// (v5, 24/10) no laboratório o F10 mostra e esconde o painel (a linha de depuração fica na aba Debug)
		if (FRTSLab* Lab = bMenu ? nullptr : FRTSLab::Get(this))
		{
			Lab->TogglePanel();
		}
		else
		{
			bShowDebug = !bShowDebug;
		}
	}
}

int32 ARTSPlayerController::GetQualityPreset() const
{
	return FMath::Clamp(Scalability::GetQualityLevels().EffectsQuality - 1, 0, 3);
}

void ARTSPlayerController::ApplyQuality(int32 Level, bool bRemember)
{
	Level = FMath::Clamp(Level, 0, 3);
	// os mesmos níveis do BlastLab: baixa/média/alta/ultra = Médio/Alto/Épico/Cinematic do motor
	static const int32 EngineLevel[4] = { 1, 2, 3, 4 };
	static const float ResolutionPct[4] = { 50.0f, 58.0f, 67.0f, 100.0f };
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(EngineLevel[Level]);
	Levels.ResolutionQuality = ResolutionPct[Level];
	Scalability::SetQualityLevels(Levels);
	// o look (plugin RTSLook) tem o seu preset por nível: Lumen, névoa volumétrica
	for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)
	{
		It->ApplyPreset(Level + 1);
	}
	// o chão que afunda de verdade só no Alta e no Ultra; a poeira das lagartas pelo preset
	if (URTSMatchSubsystem* M = GetWorld()->GetSubsystem<URTSMatchSubsystem>())
	{
		M->SetGroundDisplacement(Level >= 2);
		M->SetDustQuality(Level);
	}
	if (bRemember)
	{
		GConfig->SetInt(TEXT("RTS"), TEXT("QualityPreset"), Level, GGameUserSettingsIni);
		Scalability::SaveState(GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS qualidade: preset %d%s"), Level, bRemember ? TEXT(" (guardado)") : TEXT(""));
}

void ARTSPlayerController::OnButton(FName Id)
{
	URTSSessionSubsystem* Session = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>();
	URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>();
	const FString IdText = Id.ToString();
	if (Id == TEXT("skirmish"))
	{
		Session->StartSkirmish();
	}
	else if (Id == TEXT("lab") && FRTSLab::IsAllowed())
	{
		FRTSLab::Open(this, FRTSLab::DefaultMap());   // (v5) o laboratório (o chão plano, se existir; o painel troca o mapa)
	}
	// a LAN: criar a sala, achar as da rede, a sala de espera
	else if (Id == TEXT("multiplayer"))
	{
		MenuPage = EMenuPage::Lan;
		Net->StartBrowsing();
	}
	else if (Id == TEXT("lan_ip"))
	{
		bTypingIp = true;
	}
	else if (Id == TEXT("lan_ip_join"))
	{
		JoinTypedIp();
	}
	else if (Id == TEXT("lan_back"))
	{
		Net->Leave();
		Net->StopBrowsing();
		MenuPage = EMenuPage::Main;
	}
	else if (Id == TEXT("lan_host"))
	{
		FString NetError;
		if (Net->Host(NetError))
		{
			MenuPage = EMenuPage::Lobby;
		}
	}
	else if (IdText.StartsWith(TEXT("lan_join_")))
	{
		const int32 K = FCString::Atoi(*IdText.Mid(9));
		if (Net->FoundGames().IsValidIndex(K))
		{
			const FRTSLanGame G = Net->FoundGames()[K];
			FString NetError;
			if (Net->Join(G.Ip, G.Port, NetError))
			{
				MenuPage = EMenuPage::Lobby;
			}
		}
	}
	else if (Id == TEXT("lan_start"))
	{
		Net->StartMatch();
	}
	else if (Id == TEXT("lan_color"))
	{
		Net->CycleColor();
	}
	// a sala: o time e a cor de cada linha (a sua; o anfitrião mexe em todas e nas IAs)
	else if (IdText.StartsWith(TEXT("lan_team_")))
	{
		Net->CycleTeam(uint8(FCString::Atoi(*IdText.Mid(9))));
	}
	else if (IdText.StartsWith(TEXT("lan_color_")))
	{
		const uint8 Slot = uint8(FCString::Atoi(*IdText.Mid(10)));
		if (Slot == Net->LocalSlot())
		{
			Net->CycleColor();
		}
		else
		{
			Net->CycleColorOf(Slot);
		}
	}
	else if (IdText.StartsWith(TEXT("lan_kick_")))
	{
		Net->RemoveAI(uint8(FCString::Atoi(*IdText.Mid(9))));
	}
	else if (Id == TEXT("lan_add_ai"))
	{
		Net->AddAI();
	}
	else if (Id == TEXT("lan_random"))
	{
		Net->RandomTeams();
	}
	else if (Id == TEXT("lan_fog"))
	{
		Net->SetFogMode(URTSSessionSubsystem::NextFogMode(Net->GetFogMode()));
	}
	else if (Id == TEXT("lobby"))
	{
		// a partida em rede acabou: todos voltam pra mesma sala (o anfitrião leva os outros junto)
		Net->BackToLobby();
		Session->ReturnToLobby();
	}
	else if (Id == TEXT("lan_leave"))
	{
		Net->Leave();
		MenuPage = EMenuPage::Lan;
	}
	else if (Id == TEXT("quit"))
	{
		Session->Quit(this);
	}
	else if (Id == TEXT("resume"))
	{
		bPauseMenu = false;
		bSurrenderArmed = false;
		if (URTSMatchSubsystem* M = Match())
		{
			M->SetPaused(false);
		}
	}
	// a pausa em rede, a volta e o voto pra derrubar quem sumiu (13/10)
	else if (Id == TEXT("net_pause"))
	{
		Net->RequestPause();
		bPauseMenu = false;
		bSurrenderArmed = false;
	}
	else if (Id == TEXT("net_resume"))
	{
		Net->RequestResume();
	}
	else if (Id == TEXT("net_vote"))
	{
		Net->VoteDrop(Net->GetPause().WaitingFor);
	}
	else if (Id == TEXT("surrender"))
	{
		if (!bSurrenderArmed)
		{
			bSurrenderArmed = true;
		}
		else if (URTSMatchSubsystem* M = Match())
		{
			bSurrenderArmed = false;
			bPauseMenu = false;
			M->SetPaused(false);
			M->OrderResign();
		}
	}
	else if (Id == TEXT("menu"))
	{
		Session->ReturnToMenu();
	}
	else if (Id == TEXT("fog"))
	{
		if (URTSMatchSubsystem* M = Match())
		{
			M->ToggleFogReveal();
		}
	}
	else if (IdText.StartsWith(TEXT("quality_")))
	{
		ApplyQuality(FCString::Atoi(*IdText.RightChop(8)));
	}
	else if (Id == TEXT("fogsetting"))
	{
		Session->FogMode = URTSSessionSubsystem::NextFogMode(Session->FogMode);
	}
	// (v4, 22/10) o dinheiro inicial: o do menu (fica guardado) e o da sala (o anfitrião)
	else if (Id == TEXT("money_up") || Id == TEXT("money_down"))
	{
		Session->SetStartingMoney(Session->StepMoney(Session->StartingMoney, Id == TEXT("money_up") ? 1 : -1));
	}
	else if (Id == TEXT("lan_money_up") || Id == TEXT("lan_money_down"))
	{
		Net->SetStartingMoney(Session->StepMoney(Net->GetStartingMoney(), Id == TEXT("lan_money_up") ? 1 : -1));
	}
	// (31/10, o elenco das 3 facções) a sua facção e a da IA no skirmish (ficam guardadas)
	else if (Id == TEXT("faction_up") || Id == TEXT("faction_down"))
	{
		Session->SetPlayerFaction(Session->StepFaction(Session->PlayerFaction, Id == TEXT("faction_up") ? 1 : -1));
	}
	else if (Id == TEXT("aifaction_up") || Id == TEXT("aifaction_down"))
	{
		Session->SetAIFaction(Session->StepFaction(Session->AIFaction, Id == TEXT("aifaction_up") ? 1 : -1));
	}
	else if (URTSMatchSubsystem* M = Match())
	{
		// o painel provisório da economia: eco_<ação>_<número>
		const FString S = Id.ToString();
		FString Action;
		FString Arg;
		// os botões do HUD novo (RTSHUDNew.cpp)
		if (S.StartsWith(TEXT("group_")))
		{
			const int32 N = FCString::Atoi(*S.Mid(6));
			if (const TArray<uint32>* Group = Groups.Find(N == 0 ? 9 : N - 1))
			{
				Selected = *Group;
				PruneSelection();
			}
			return;
		}
		if (S.StartsWith(TEXT("pass_")))
		{
			// (01/11) a casa de quem está dentro do transporte selecionado: só ele sai
			const uint32 One = uint32(FCString::Strtoui64(*S.Mid(5), nullptr, 10));
			if (Selected.Num() == 1)
			{
				M->OrderUnloadOne(Selected[0], One);
				++Orders;
			}
			return;
		}
		if (S.StartsWith(TEXT("seat_")))
		{
			return;   // (01/11) a vaga livre: nada
		}
		if (S.StartsWith(TEXT("unit_")))
		{
			// a casa de uma unidade selecionada: fica só ela
			const uint32 One = uint32(FCString::Strtoui64(*S.Mid(5), nullptr, 10));
			if (Selected.Contains(One))
			{
				Selected = { One };
			}
			return;
		}
		if (S.StartsWith(TEXT("queue_")))
		{
			// (v4) na zona de lançamento a casa da fila é o avião de carga: não cancela nada
			FRTSUnitState QU;
			FRTSDefInfo QD;
			const uint32 Structure = SelectedStructure();
			if (Structure != 0 && M->GetUnit(Structure, QU) && M->GetDef(QU.Def, QD) && !QD.Produces.IsEmpty())
			{
				M->OrderCancelProduce(Structure, FCString::Atoi(*S.Mid(6)));
			}
			return;
		}
		if (S.StartsWith(TEXT("power_")))
		{
			// (v4) a coluna de poderes: a casa N é o N-ésimo poder que a patente já ligou (a mesma ordem das teclas F1–F3)
			const int32 Power = M->PowerAtSlot(FCString::Atoi(*S.Mid(6)));
			if (Power >= 0)
			{
				BeginTargeting(Power);
			}
			return;
		}
		if (S == TEXT("cmd_attack_move"))
		{
			// (v4) o mesmo da tecla A: armar o ataque andando desiste da mira do poder (senão o próximo clique no chão era o poder)
			bAttackMoveArmed = !Selected.IsEmpty();
			if (bAttackMoveArmed)
			{
				TargetingPower = -1;
			}
			return;
		}
		if (S == TEXT("cmd_unload"))
		{
			if (!Selected.IsEmpty())
			{
				M->OrderUnload(Selected);   // (simulação 35)
				++Orders;
			}
			return;
		}
		if (S == TEXT("cmd_stop"))
		{
			if (!Selected.IsEmpty())
			{
				M->OrderStop(Selected);
				++Orders;
			}
			return;
		}
		// (v5, 24/10) o ar: Voltar à base já; a patrulha arma o próximo clique no chão
		if (S == TEXT("cmd_return_base"))
		{
			if (!Selected.IsEmpty())
			{
				M->OrderReturnToBase(Selected);
				++Orders;
			}
			return;
		}
		if (S == TEXT("cmd_patrol"))
		{
			bPatrolArmed = !Selected.IsEmpty();
			bAttackMoveArmed = false;
			bGuardArmed = false;
			TargetingPower = -1;
			return;
		}
		// (v5, 25/10) a guarda: arma o próximo clique (o chão guarda a área, o amigo é escoltado), como a tecla G
		if (S == TEXT("cmd_guard"))
		{
			bGuardArmed = !Selected.IsEmpty();
			bGuardPressed = false;
			bPatrolArmed = false;
			bAttackMoveArmed = false;
			TargetingPower = -1;
			return;
		}
		if (S == TEXT("cmd_sell"))
		{
			// (v4, 23/10) vender (o Sell do ZH: sem confirmação, todos os prédios prontos da seleção); o prédio sendo vendido sai
			// da seleção (o UNSELECTABLE do ZH, PruneSelection)
			TArray<uint32> Sell;
			for (const uint32 Building : Selected)
			{
				if (M->CanSell(Building))
				{
					Sell.Add(Building);
				}
			}
			if (!Sell.IsEmpty())
			{
				M->OrderSell(Sell);
				++Orders;
			}
			return;
		}
		if (S == TEXT("mm_worker"))
		{
			SelectIdleBuilders(IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl));
			return;
		}
		if (S == TEXT("mm_settings"))
		{
			bPauseMenu = true;
			M->SetPaused(true);
			return;
		}
		if (S == TEXT("hud_bg") || S.StartsWith(TEXT("mm_")) || S == TEXT("promote") || S.StartsWith(TEXT("cmd_")))
		{
			return;   // a peça do HUD segura o clique; os botões que ainda não existem na partida não fazem nada
		}
		if (S.StartsWith(TEXT("eco_")) && S.Mid(4).Split(TEXT("_"), &Action, &Arg))
		{
			const int32 N = FCString::Atoi(*Arg);
			const uint32 Structure = SelectedStructure();
			if (Action == TEXT("produce") && Structure != 0)
			{
				// Shift-click +5 (o clássico; a simulação recusa o que passar da fila)
				const int32 Count = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) ? 5 : 1;
				for (int32 K = 0; K < Count; ++K)
				{
					M->OrderProduce(Structure, uint16(N));
				}
				++Orders;
			}
			else if (Action == TEXT("cancelq") && Structure != 0)
			{
				M->OrderCancelProduce(Structure, N);
			}
			else if (Action == TEXT("build"))
			{
				TargetingPower = -1;   // (v4) posicionar e mirar o poder não andam juntos
				BeginPlacing(N);
			}
			else if (Action == TEXT("cancelbuild") && Structure != 0)
			{
				M->OrderCancelBuild(Structure);
			}
		}
	}
}

void ARTSPlayerController::JoinTypedIp()
{
	URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>();
	FString NetError;
	bTypingIp = false;
	if (Net && !IpEntry.IsEmpty() && Net->Join(IpEntry, URTSNetSession::GamePort, NetError))
	{
		MenuPage = EMenuPage::Lobby;
		// o último IP fica salvo: na próxima vez o campo já vem preenchido
		FFileHelper::SaveStringToFile(IpEntry, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LastLanIp.txt")));
	}
}

void ARTSPlayerController::TickIpEntry()
{
	if (!bTypingIp)
	{
		return;
	}
	// o campo do IP: números, ponto, apagar, Enter entra, Esc sai
	static const FKey Digits[10] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey Pad[10] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 K = 0; K < 10; ++K)
	{
		if ((WasInputKeyJustPressed(Digits[K]) || WasInputKeyJustPressed(Pad[K])) && IpEntry.Len() < 15)
		{
			IpEntry.AppendChar(TCHAR('0' + K));
		}
	}
	if ((WasInputKeyJustPressed(EKeys::Period) || WasInputKeyJustPressed(EKeys::Decimal) || WasInputKeyJustPressed(EKeys::Comma)) && IpEntry.Len() < 15)
	{
		IpEntry.AppendChar(TEXT('.'));
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace) && !IpEntry.IsEmpty())
	{
		IpEntry.LeftChopInline(1);
	}
	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		JoinTypedIp();
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		bTypingIp = false;
	}
}

void ARTSPlayerController::TickMenu()
{
	URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>();
	{
		// (v5, 24/10) -RTSLab: o jogo abre direto no laboratório, uma vez por processo (o teste automático e a LAN seguem o menu);
		// -RTSLabMenuShot=<arquivo>: a foto do menu com o botão LAB (0,6 s depois de ele aparecer)
		static bool bLabAutoDone = false;
		static double MenuSince = -1.0;
		FString Script;
		FString MenuShot;
		const bool bTest = FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), Script);
		if (!bLabAutoDone && !bTest && FParse::Param(FCommandLine::Get(), TEXT("RTSLab")) && !(Net && Net->IsActive()))
		{
			bLabAutoDone = true;
			FRTSLab::Open(this, FRTSLab::DefaultMap());
			return;
		}
		MenuSince = MenuSince < 0.0 ? FPlatformTime::Seconds() : MenuSince;
		if (MenuSince > 0.0 && FPlatformTime::Seconds() - MenuSince > 0.6 && FParse::Value(FCommandLine::Get(), TEXT("-RTSLabMenuShot="), MenuShot))
		{
			MenuSince = 0.0;   // uma foto só
			FScreenshotRequest::RequestScreenshot(MenuShot, true, false);
			UE_LOG(LogTemp, Log, TEXT("RTS laboratório: a foto do menu em %s"), *MenuShot);
		}
	}
	// de volta da partida em rede: o menu abre na mesma sala
	if (!bNetAutoDone && Net && Net->IsActive())
	{
		bNetAutoDone = true;
		MenuPage = EMenuPage::Lobby;
	}
	// o teste da LAN sem clicar: -RTSNetHost cria a sala; -RTSNetJoin=<ip> entra direto (depois do
	// arquivo -RTSNetJoinGate=, se tiver: o anfitrião do teste escolhe a cor antes dos outros entrarem)
	FString Gate;
	const bool bGateClosed = FParse::Value(FCommandLine::Get(), TEXT("-RTSNetJoinGate="), Gate) && !FPaths::FileExists(Gate);
	if (!bNetAutoDone && Net && !bGateClosed)
	{
		bNetAutoDone = true;
		FString JoinIp;
		FString NetError;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSNetJoin="), JoinIp) && Net->Join(JoinIp, URTSNetSession::GamePort, NetError))
		{
			MenuPage = EMenuPage::Lobby;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("RTSNetHost")) && Net->Host(NetError))
		{
			MenuPage = EMenuPage::Lobby;
			if (FParse::Param(FCommandLine::Get(), TEXT("RTSNetNoFog")))
			{
				Net->SetFogMode(ERTSFogMode::Off);
			}
		}
	}
	if (Net && Net->IsActive() && FParse::Param(FCommandLine::Get(), TEXT("RTSNetBot")))
	{
		// o robô do teste na sala: o anfitrião começa com -RTSNetAutoStart=<n> jogadores; depois de
		// uma partida, todos de volta na sala, sai (o teste conferiu que voltaram)
		int32 AutoStart = 0;
		const double Now = FPlatformTime::Seconds();
		if (Net->MatchesPlayed() == 0 && Net->IsHost() && FParse::Value(FCommandLine::Get(), TEXT("-RTSNetAutoStart="), AutoStart)
			&& Net->PlayerCount() >= AutoStart && Net->EveryoneInLobby() && Net->GetPhase() == ERTSNetPhase::Lobby)
		{
			if (BotStartAt == 0.0)
			{
				BotStartAt = Now + 3.0;   // os nomes e cores chegam em todos antes
			}
			else if (Now >= BotStartAt)
			{
				UE_LOG(LogTemp, Log, TEXT("RTS LAN robô: %d na sala, começando"), Net->PlayerCount());
				Net->StartMatch();
			}
		}
		if (Net->MatchesPlayed() > 0 && Net->GetPhase() == ERTSNetPhase::Lobby && (!Net->IsHost() || Net->EveryoneInLobby()))
		{
			if (BotExitAt == 0.0)
			{
				BotExitAt = Now + 10.0;
				UE_LOG(LogTemp, Log, TEXT("RTS LAN robô: de volta na sala com %d jogadores; saindo em 10 s"), Net->PlayerCount());
			}
			else if (Now >= BotExitAt)
			{
				FPlatformMisc::RequestExit(false);
			}
		}
	}
	if (MenuPage == EMenuPage::Lan)
	{
		if (IpEntry.IsEmpty() && !bTypingIp)
		{
			FFileHelper::LoadFileToString(IpEntry, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LastLanIp.txt")));
			IpEntry.TrimStartAndEndInline();
		}
		TickIpEntry();
	}
	else
	{
		bTypingIp = false;
	}
	// a sala caiu (o anfitrião saiu, recusou): volta pra lista
	if (MenuPage == EMenuPage::Lobby && Net && !Net->IsActive())
	{
		MenuPage = EMenuPage::Lan;
		Net->StartBrowsing();
	}
	float X = 0.0f;
	float Y = 0.0f;
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && ReadMouse(X, Y))
	{
		if (ARTSHUD* HUD = GetRTSHUD())
		{
			const FName Id = HUD->ButtonAt(FVector2D(X, Y));
			if (!Id.IsNone())
			{
				OnButton(Id);
			}
		}
	}
}

void ARTSPlayerController::TickMatch(float DeltaTime)
{
	URTSMatchSubsystem* M = Match();
	ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn());
	float MX = 0.0f;
	float MY = 0.0f;
	const bool bHasMouse = ReadMouse(MX, MY);
	const FVector2D Mouse(MX, MY);
	// (simulação 35, 01/11) o Alt segurado põe as ordens na fila (o waypoint do ZH, com atacar e as obras do Dozer)
	const bool bAltHeld = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt) || bLabAlt;   // (o da cena do laboratório também)
	if (M)
	{
		M->SetQueueOrders(bAltHeld);
	}

	// (v4) a linha dos dois botões que acabou (mandou ou desistiu) só volta ao normal com os dois soltos: até lá o botão que
	// ficou apertado não rola, não faz caixa, não clica nem desseleciona
	if (LineChord == ELineChord::Swallow && !IsInputKeyDown(EKeys::LeftMouseButton) && !IsInputKeyDown(EKeys::RightMouseButton))
	{
		LineChord = ELineChord::None;
		bLeftDown = false;
		bRightDown = false;
		bRightScrolling = false;
	}

	for (int32 I = Markers.Num() - 1; I >= 0; --I)
	{
		Markers[I].Age += DeltaTime;
		if (Markers[I].Age > MarkerSeconds)
		{
			Markers.RemoveAt(I);
		}
	}

	const bool bOver = M && M->Result() != ERTSMatchResult::Playing;
	if (M && M->IsLocalDefeated())
	{
		// quem caiu só assiste: nada selecionado, nada pra pôr no chão
		Selected.Reset();
		PlacingDef = -1;
		TargetingPower = -1;
	}
	if (bOver)
	{
		// (v4) acabou, com qualquer resultado: a mira do poder sai, e a cruz do cursor junto (o círculo vermelho não fica
		// seguindo o mouse na tela do resultado; o cursor só é escolhido depois do retorno lá embaixo)
		TargetingPower = -1;
		CurrentMouseCursor = EMouseCursor::Default;
	}
	if (WasInputKeyJustPressed(EKeys::F9) && M)
	{
		M->ToggleFogReveal();   // depuração: a névoa liga e desliga na hora
	}
	// (29/10, o Felipe: "uma tecla pra trocar de textura no meio da partida") o chão de hoje de manhã × o de agora: as duas versões
	// moram no mesmo material do chão (WorldForge, a chave GroundV da coleção MPC_Ground; 1 = agora). O F12 troca a rts.Ground.Before (o F11
	// a Unreal usa pra tela cheia e pega antes do jogo: InputSettings bF11TogglesFullscreen)
	// (o teste liga pela linha de comando: -Cvars 'rts.Ground.Before 1')
	if (WasInputKeyJustPressed(EKeys::F12))
	{
		CVarGroundBefore->Set(CVarGroundBefore->GetInt() ? 0 : 1, ECVF_SetByCode);
	}
	if (CVarGroundBefore && (CVarGroundBefore->GetInt() != 0) != bGroundBefore)
	{
		if (UMaterialParameterCollection* Ground = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Ground.MPC_Ground"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			bGroundBefore = CVarGroundBefore->GetInt() != 0;
			UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Ground, TEXT("GroundV"), bGroundBefore ? 0.0f : 1.0f);
			SetPowerMessage(FText::FromString(bGroundBefore ? TEXT("Ground: BEFORE (this morning)  -  F12 to switch") : TEXT("Ground: NOW  -  F12 to switch")));
			UE_LOG(LogTemp, Display, TEXT("RTS chão: %s"), bGroundBefore ? TEXT("o de antes") : TEXT("o de agora"));
		}
	}
	// o Esc desiste de posicionar o prédio (ou de mirar o poder) antes de abrir a pausa
	const bool bEscCancelsPlacement = (PlacingDef >= 0 || TargetingPower >= 0) && WasInputKeyJustPressed(EKeys::Escape);
	if (bEscCancelsPlacement)
	{
		PlacingDef = -1;
		TargetingPower = -1;
	}
	// (v4) e desiste da linha dos dois botões sendo puxada (esse Esc não abre a pausa)
	const bool bEscCancelsLine = (LineChord == ELineChord::Active || LineChord == ELineChord::Releasing) && WasInputKeyJustPressed(EKeys::Escape);
	if (bEscCancelsLine)
	{
		CancelLine(TEXT("o Esc"));
	}
	// (v5) e solta a ferramenta do cursor do laboratório
	FRTSLab* EscLab = M ? M->GetLab() : nullptr;
	const bool bEscCancelsLab = EscLab && EscLab->HasTool() && !bEscCancelsPlacement && !bEscCancelsLine && WasInputKeyJustPressed(EKeys::Escape);
	if (bEscCancelsLab)
	{
		EscLab->DropTool();   // (a estrada e a corrente de postes: o 1º Esc fecha a linha, o 2º solta)
	}
	if (WasInputKeyJustPressed(EKeys::Escape) && !bOver && !bEscCancelsPlacement && !bEscCancelsLine && !bEscCancelsLab)
	{
		bPauseMenu = !bPauseMenu;
		bSurrenderArmed = false;
		if (M)
		{
			M->SetPaused(bPauseMenu);
		}
	}
	// parado em rede, só os botões da tela respondem (a ordem não fica guardada pra depois da pausa)
	if (bPauseMenu || bOver || !M || M->IsNetPaused())
	{
		if (LineChord == ELineChord::Active || LineChord == ELineChord::Releasing)
		{
			CancelLine(TEXT("a pausa ou o fim da partida"));
		}
		bLeftDown = false;
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse)
		{
			if (ARTSHUD* HUD = GetRTSHUD())
			{
				const FName Id = HUD->ButtonAt(Mouse);
				if (!Id.IsNone())
				{
					OnButton(Id);
				}
			}
		}
		if (Cam && !bPauseMenu)
		{
			TickCamera(DeltaTime, Cam);   // no fim dá pra olhar o campo
		}
		return;
	}

	if (ARTSHUD* HUD = GetRTSHUD())
	{
		// (v4) um botão de verdade ganha da caixa do minimapa: a coluna de poderes desce pra dentro do quadrado em volta
		// do losango (o "hud_bg" da peça não conta: ele cobre o losango também)
		const FName UnderMouse = bHasMouse ? HUD->ButtonAt(Mouse) : NAME_None;
		const bool bOnButton = !UnderMouse.IsNone() && !UnderMouse.ToString().StartsWith(TEXT("hud_bg"));
		bOverMinimap = bHasMouse && !bOnButton && HUD->IsOverMinimap(Mouse);
	}
	TickMinimap(DeltaTime, Cam, Mouse, bHasMouse);
	TickAlerts(DeltaTime);
	if (Cam)
	{
		TickCamera(DeltaTime, Cam);
	}
	PruneSelection();
	const bool bPlacing = TickPlacement(Mouse, bHasMouse);
	UpdateGhost();
	if (bPlacing)
	{
		TargetingPower = -1;
	}
	// (v4) os poderes do general: F1, F2, F3 na ordem da coluna (de baixo pra cima), em qualquer HUD; a mesma tecla de novo
	// desiste. Sem poder ligado ainda, a tecla diz o que falta ("Requires: Rank 2")
	{
		static const FKey PowerKeys[3] = { EKeys::F1, EKeys::F2, EKeys::F3 };
		for (int32 K = 0; K < 3; ++K)
		{
			if (!WasInputKeyJustPressed(PowerKeys[K]))
			{
				continue;
			}
			const int32 Power = M->PowerAtSlot(K);
			if (Power >= 0)
			{
				BeginTargeting(Power);
			}
			else if (K < M->PowerCount())
			{
				SetPowerMessage(M->PowerRefusal(K));
			}
		}
	}
	const bool bTargeting = TickTargeting(Mouse, bHasMouse);
	// (v5, 24/10) o laboratório: os atalhos (P, ., [ ], Delete, H, G) e a ferramenta do cursor (pôr, matar, efeito, atirar no
	// chão), que pega o clique esquerdo no mapa antes da seleção e das ordens
	FRTSLab* Lab = M->GetLab();
	if (Lab)
	{
		Lab->TickKeys(*this);
	}
	const bool bLabTool = Lab && !bPlacing && !bTargeting && Lab->TickCursor(*this, Mouse, bHasMouse);

	if (WasInputKeyJustPressed(EKeys::A) && !Selected.IsEmpty() && !bPlacing)
	{
		bAttackMoveArmed = true;
		bGuardArmed = false;
		TargetingPower = -1;
	}
	// (v5, 25/10) a guarda (G, o pedido do Felipe): a mesma tecla de novo desiste
	if (WasInputKeyJustPressed(EKeys::G) && !Selected.IsEmpty() && !bPlacing && !bLabTool)
	{
		bGuardArmed = !bGuardArmed;
		bGuardPressed = false;
		bAttackMoveArmed = false;
		bPatrolArmed = false;
		TargetingPower = -1;
	}
	const bool bGuardTool = !bPlacing && !bTargeting && !bLabTool && TickGuard(Mouse, bHasMouse);
	if (WasInputKeyJustPressed(EKeys::S) && !Selected.IsEmpty())
	{
		M->OrderStop(Selected);
		++Orders;
	}
	// (simulação 35) D: os transportes selecionados desembarcam todos (o Evacuate do ZH)
	if (WasInputKeyJustPressed(EKeys::D) && !Selected.IsEmpty() && !bPlacing)
	{
		M->OrderUnload(Selected);
		++Orders;
	}
	// as teclas das casas do HUD novo (as do clássico: QWERTYU em cima, ZXCVBNM embaixo); o R, posicionando um
	// prédio, segue girando o prédio
	if (ARTSHUD* HUD = GetRTSHUD(); HUD && HUD->IsNewHud())
	{
		static const TPair<TCHAR, FKey> Letters[] = {
			{ 'Q', EKeys::Q }, { 'W', EKeys::W }, { 'E', EKeys::E }, { 'R', EKeys::R }, { 'T', EKeys::T }, { 'Y', EKeys::Y }, { 'U', EKeys::U },
			{ 'Z', EKeys::Z }, { 'X', EKeys::X }, { 'C', EKeys::C }, { 'V', EKeys::V }, { 'B', EKeys::B }, { 'N', EKeys::N }, { 'M', EKeys::M } };
		for (const TPair<TCHAR, FKey>& L : Letters)
		{
			if ((L.Key == 'R' && (bPlacing || (Lab && Lab->GetTool() == ERTSLabTool::PlaceProp))) || !WasInputKeyJustPressed(L.Value))   // (v5: o R gira a peça civil do laboratório)
			{
				continue;
			}
			if (const FName* Id = HUD->GetHotkeys().Find(L.Key))
			{
				OnButton(*Id);
			}
		}
	}
	HandleGroups();

	// o que está sob o mouse: o anel acende; o cursor diz o que o clique vai fazer (ZH). Os cursores são os nossos (01/11,
	// docs/CURSORES.md): a mesma ordem de decisão do clique (HandleLeftClick e HandleEconomyClick), pra nunca mentir
	const bool bMinimapMouse = bOverMinimap || MinimapGesture != EMinimapGesture::None;
	const bool bLineChord = LineChord == ELineChord::Active || LineChord == ELineChord::Releasing;
	HoverId = (bHasMouse && !bLeftDown && !bMinimapMouse && LineChord == ELineChord::None) ? PickUnit(Mouse) : 0;
	const bool bCtrlHeld = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	const bool bShiftHeld = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const ERTSCursor Walk = bAttackMoveArmed ? ERTSCursor::AttackMove : ERTSCursor::Move;   // o clique de andar (com o Alt, ganha a ficha da fila abaixo)
	int32 ViewW = 0, ViewH = 0;
	GetViewportSize(ViewW, ViewH);
	const float EdgeX = !bHasMouse || ViewW <= 0 ? 0.0f : (Mouse.X <= EdgeScrollPx ? -1.0f : (Mouse.X >= ViewW - 1 - EdgeScrollPx ? 1.0f : 0.0f));
	const float EdgeY = !bHasMouse || ViewH <= 0 ? 0.0f : (Mouse.Y <= EdgeScrollPx ? 1.0f : (Mouse.Y >= ViewH - 1 - EdgeScrollPx ? -1.0f : 0.0f));
	ERTSCursor Cursor = ERTSCursor::Pointer;
	FRTSUnitState Hovered;
	const bool bHovered = HoverId != 0 && M->GetUnit(HoverId, Hovered);
	TArray<uint32> Dozers;
	SelectedWith(true, Dozers);
	if (bRightScrolling || bMiddleDown)
	{
		Cursor = ERTSCursor::Pan;   // arrastando a vista
	}
	else if (bEdgeScroll && (EdgeX != 0.0f || EdgeY != 0.0f) && !bMinimapMouse && !bLineChord)
	{
		// rolando pela borda (a mesma regra do TickCamera): a seta na direção, com a ponta encostada na borda
		static const ERTSCursor Dirs[3][3] = {   // [linha: em cima, meio, embaixo][coluna: esquerda, meio, direita]
			{ ERTSCursor::ScrollNW, ERTSCursor::ScrollN, ERTSCursor::ScrollNE },
			{ ERTSCursor::ScrollW, ERTSCursor::Pointer, ERTSCursor::ScrollE },
			{ ERTSCursor::ScrollSW, ERTSCursor::ScrollS, ERTSCursor::ScrollSE } };
		Cursor = Dirs[EdgeY > 0.0f ? 0 : (EdgeY < 0.0f ? 2 : 1)][EdgeX < 0.0f ? 0 : (EdgeX > 0.0f ? 2 : 1)];
	}
	else if (bLineChord)
	{
		Cursor = bAttackMoveArmed ? ERTSCursor::AttackMove : ERTSCursor::Move;   // (v4) puxando a linha dos dois botões
	}
	else if (bMinimapMouse)
	{
		// sobre o minimapa: a seta sem seleção, o "andar" com seleção (o ataque andando, se armado)
		Cursor = Selected.IsEmpty() ? ERTSCursor::Pointer : Walk;
	}
	else if (bPlacing)
	{
		FVector Ground;
		FVector2D Center, Half;
		const bool bFits = GroundUnderCursor(Mouse, Ground)
			&& M->CanPlace(uint16(PlacingDef), PlacingDir, FVector2D(Ground.X, Ground.Y), Center, Half) == ERTSPlace::Ok;
		Cursor = bFits ? ERTSCursor::Build : ERTSCursor::NoBuild;   // o fantasma verde ou vermelho faz o resto
	}
	else if (bLabTool)
	{
		Cursor = ERTSCursor::Strike;   // (v5) a ferramenta do laboratório armada
	}
	else if (TargetingPower >= 0)
	{
		// (v4) a mira do poder: a de ataque se ele lança mísseis, a de apoio se não (o paraquedas, o drone)
		URTSMatchSubsystem::FPowerInfo Info;
		Cursor = M->GetPowerInfo(TargetingPower, Info) && Info.Missiles == 0 ? ERTSCursor::Support : ERTSCursor::Strike;
	}
	else if (bGuardArmed)
	{
		Cursor = bHovered && !M->IsEnemyOfLocal(Hovered.Owner) ? ERTSCursor::Escort : ERTSCursor::Guard;   // (v5) a escolta ou a área
	}
	else if (bPatrolArmed)
	{
		Cursor = ERTSCursor::Patrol;
	}
	else if (bAttackMoveArmed)
	{
		Cursor = ERTSCursor::AttackMove;
	}
	else if (bCtrlHeld && !Selected.IsEmpty())
	{
		// o tiro forçado (Ctrl): na unidade, ou no chão (a sua única selecionada vira tiro no chão, como no clique)
		const bool bOnUnit = bHovered && !(Selected.Num() == 1 && Selected.Contains(HoverId));
		Cursor = bOnUnit ? ERTSCursor::Force : ERTSCursor::Ground;
	}
	else if (bHovered)
	{
		const bool bEnemy = M->IsEnemyOfLocal(Hovered.Owner);
		const bool bMine = Hovered.Owner == M->LocalPlayer();
		if (bEnemy)
		{
			// o inimigo que ninguém da seleção acerta (o tanque e o avião no ar): o clique continua sendo andar
			Cursor = Selected.IsEmpty() ? ERTSCursor::Pointer : (M->CanAnyTarget(Selected, HoverId) ? ERTSCursor::Attack : ERTSCursor::Cant);
		}
		else if (!Dozers.IsEmpty() && bMine && Hovered.bStructure && Hovered.BuildFraction < 1.0f)
		{
			Cursor = ERTSCursor::Resume;   // a obra parada
		}
		else if (!Dozers.IsEmpty() && Hovered.bStructure && M->CanRepair(HoverId))
		{
			Cursor = ERTSCursor::Repair;   // (v4, 23/10) o prédio avariado, dele ou do aliado
		}
		else if (bMine && !bShiftHeld && !Selected.Contains(HoverId) && M->CanEnter(Selected, HoverId))
		{
			Cursor = ERTSCursor::Enter;   // (simulação 35) o soldado no transporte com vaga
		}
		else
		{
			Cursor = bMine ? ERTSCursor::Select : (Selected.IsEmpty() ? ERTSCursor::Pointer : Walk);   // o do aliado: o clique anda
		}
	}
	else if (!Selected.IsEmpty())
	{
		FVector Ground;
		if (!GroundUnderCursor(Mouse, Ground))
		{
			Cursor = ERTSCursor::Invalid;   // sem chão debaixo (a vista no céu)
		}
		else
		{
			TArray<uint32> Trucks;
			SelectedWith(false, Trucks);
			bool bOnlyStructures = true;
			for (const uint32 Id : Selected)
			{
				FRTSUnitState U;
				bOnlyStructures &= M->GetUnit(Id, U) && U.bStructure && U.Owner == M->LocalPlayer();
			}
			if (!Dozers.IsEmpty() && M->SalvageAt(FVector2D(Ground.X, Ground.Y)) != 0)
			{
				Cursor = ERTSCursor::Salvage;   // (simulação 30) a carcaça ou o entulho
			}
			else if (!Trucks.IsEmpty() && DockAt(Ground) >= 0)
			{
				Cursor = ERTSCursor::Gather;
			}
			else
			{
				Cursor = bOnlyStructures ? ERTSCursor::Rally : Walk;   // prédio não anda: o clique marca o ponto de encontro
			}
		}
	}
	bRepairCursor = Cursor == ERTSCursor::Repair || Cursor == ERTSCursor::Salvage;
	// (01/11, aprovado) a fila do Alt: a ordem ganha a ficha com o número da vez (a fila mais longa da seleção, mais 1)
	int32 QueueN = 0;
	if (bAltHeld && !Selected.IsEmpty() && FRTSCursors::IsQueueable(Cursor))
	{
		TArray<URTSMatchSubsystem::FRTSOrderStep> Steps;
		for (const uint32 Id : Selected)
		{
			QueueN = FMath::Max(QueueN, M->GetOrderPath(Id, Steps) ? Steps.Num() : 0);
		}
		QueueN += 1;
	}
	const ERTSCursor Before = Cursors.GetShown();
	CurrentMouseCursor = Cursors.Apply(Cursor, ViewH, QueueN);
	// -RTSCursorLog: cada troca no log, com o mouse (a prova dos testes: o cursor do Windows não sai na captura do jogo)
	static const bool bCursorLog = FParse::Param(FCommandLine::Get(), TEXT("RTSCursorLog"));
	if (bCursorLog && Cursors.GetShown() != Before)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS cursor: %s (fila %d) em (%.0f, %.0f), tela %dx%d"), FRTSCursors::Name(Cursor), QueueN, Mouse.X, Mouse.Y, ViewW, ViewH);
	}
	if (!Cursors.IsReady() && bRepairCursor && EnsureRepairCursor())
	{
		CurrentMouseCursor = EMouseCursor::Custom;   // sem os arquivos: a chave antiga
	}

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		UE_LOG(LogTemp, Log, TEXT("RTS clique: esquerdo desceu em (%.0f, %.0f), mouse %d, gesto do minimapa %d"), Mouse.X, Mouse.Y, int32(bHasMouse), int32(MinimapGesture));
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse && MinimapGesture == EMinimapGesture::None)
	{
		// o painel da economia vem antes do mundo: clicar num botão nunca vira ordem no chão
		const ARTSHUD* PanelHUD = GetRTSHUD();
		const FName Button = PanelHUD ? PanelHUD->ButtonAt(Mouse) : NAME_None;
		if (!Button.IsNone())
		{
			OnButton(Button);
		}
		else if (!bPlacing && !bTargeting && !bLabTool && !bGuardTool && LineChord == ELineChord::None)
		{
			// (v4, 23/10) o esquerdo arrastado é sempre a caixa de seleção, com tropa selecionada também (o Felipe: arrastar pra
			// pegar outros tanques virava a linha); a linha agora é o esquerdo + o direito juntos (TickLineChord)
			bLeftDown = true;
			LeftDownPos = Mouse;
			LeftDownAt = FPlatformTime::Seconds();
		}
	}
	// (v4) a linha dos dois botões: depois do esquerdo e do direito que descem (este quadro já sabe dos dois) e antes do
	// esquerdo que sobe (o acorde pega o soltar antes da caixa e do clique)
	TickLineChord(Mouse, bHasMouse);
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
	{
		UE_LOG(LogTemp, Log, TEXT("RTS clique: esquerdo subiu em (%.0f, %.0f), estava apertado %d"), Mouse.X, Mouse.Y, int32(bLeftDown));
	}
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton) && bLeftDown)
	{
		bLeftDown = false;
		if (LineChord != ELineChord::None)
		{
			// o esquerdo do acorde: a linha é do TickLineChord (nem caixa nem clique)
		}
		else if (FVector2D::Distance(LeftDownPos, Mouse) > DragThresholdPx)
		{
			HandleBoxSelect(LeftDownPos, Mouse);
		}
		else
		{
			HandleLeftClick(Mouse);
		}
	}
}

void ARTSPlayerController::TickCamera(float DeltaTime, ARTSCameraPawn* Cam)
{
	ARTSCameraPawn::FInput In;
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	float MX = 0.0f;
	float MY = 0.0f;
	const bool bHasMouse = ReadMouse(MX, MY);
	const FVector2D Mouse(MX, MY);

	// setas (ADR-0003): ↑↓ rolam, ←→ giram; A e S são ordens, então não há WASD
	In.PanAxis.Y = (IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);
	In.YawDelta = ((IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f)) * RotateDegPerSec * DeltaTime;
	if (bEdgeScroll && bHasMouse && W > 0 && H > 0 && !bRightScrolling && !bMiddleDown && !bOverMinimap && MinimapGesture == EMinimapGesture::None)
	{
		// a borda rola mais rápido que a tecla, e a vertical um pouco mais (ZH)
		const float EX = Mouse.X <= EdgeScrollPx ? -1.0f : (Mouse.X >= W - 1 - EdgeScrollPx ? 1.0f : 0.0f);
		const float EY = Mouse.Y <= EdgeScrollPx ? 1.0f : (Mouse.Y >= H - 1 - EdgeScrollPx ? -1.0f : 0.0f);
		In.PanAxis.X += EX * EdgeScrollBoost;
		In.PanAxis.Y += EY * EdgeScrollBoost * VerticalScrollBias;
	}
	In.PanAxis.X = FMath::Clamp(In.PanAxis.X, -EdgeScrollBoost, EdgeScrollBoost);
	In.PanAxis.Y = FMath::Clamp(In.PanAxis.Y, -EdgeScrollBoost * VerticalScrollBias, EdgeScrollBoost * VerticalScrollBias);

	// o HUD novo: o direito num botão de produzir tira um daquele tipo da fila (o clássico: Right-click −1)
	bool bRightOnHud = false;
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && bHasMouse)
	{
		ARTSHUD* HUD = GetRTSHUD();
		const FString B = HUD && HUD->IsNewHud() ? HUD->ButtonAt(Mouse).ToString() : FString();
		if (B.StartsWith(TEXT("eco_produce_")))
		{
			bRightOnHud = true;
			const uint32 Structure = SelectedStructure();
			const int32 Def = FCString::Atoi(*B.Mid(12));
			TArray<uint16> Queue;
			URTSMatchSubsystem* MM = Match();
			if (Structure != 0 && MM)
			{
				MM->GetQueue(Structure, Queue);
				for (int32 K = Queue.Num() - 1; K >= 0; --K)
				{
					if (Queue[K] == Def)
					{
						MM->OrderCancelProduce(Structure, K);
						break;
					}
				}
			}
		}
	}
	// botão direito: arrastar rola (estilo Generals: quanto mais longe do ponto, mais rápido); clique rápido desseleciona
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && bHasMouse && MinimapGesture == EMinimapGesture::None && !bRightOnHud)
	{
		bRightDown = true;
		bRightScrolling = false;
		RightAnchor = Mouse;
	}
	// (v4) no acorde da linha (esquerdo + direito) o direito não rola: o arraste é a linha; a borda, as setas e a roda seguem
	// (a linha fica presa no chão, dá pra puxar uma comprida rolando)
	if (bRightDown && bHasMouse && H > 0 && LineChord == ELineChord::None)
	{
		const FVector2D Offset = Mouse - RightAnchor;
		bRightScrolling |= Offset.Size() > DragThresholdPx;
		if (bRightScrolling)
		{
			// mais longe do ponto apertado = mais rápido: 180 px = 1×, teto de 2,5× (Vanguard)
			const float Factor = FMath::Min(float(Offset.Size()) / DragRefPx, DragMaxFactor);
			In.DragPan = FVector2D(Offset.X, -Offset.Y).GetSafeNormal() * Factor;
		}
	}
	if (WasInputKeyJustReleased(EKeys::RightMouseButton))
	{
		if (bRightDown && !bRightScrolling && LineChord == ELineChord::None)   // (o direito do acorde da linha não desseleciona)
		{
			if (PlacingDef >= 0)
			{
				// posicionando: o clique simples desiste do prédio e mantém a seleção (o arraste só rola)
				PlacingDef = -1;
			}
			else if (TargetingPower >= 0)
			{
				TargetingPower = -1;   // (v4) mirando o poder: o mesmo, desiste e mantém a seleção
			}
			else if (bGuardArmed)
			{
				bGuardArmed = false;   // (v5, 25/10) escolhendo a guarda: o mesmo
				bGuardPressed = false;
			}
			else if (FRTSLab* Lab = FRTSLab::Get(this); Lab && Lab->HasTool())
			{
				Lab->DropTool();   // (v5) a ferramenta do laboratório: o mesmo, solta e mantém a seleção (a estrada: fecha a linha antes)
			}
			else if ((IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt)) && Match() && !Selected.IsEmpty())
			{
				Match()->OrderTrimQueue(Selected);   // (simulação 35) Alt + o clique rápido do direito tira o último da fila (a seleção fica)
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("RTS clique: direito desfez a seleção em (%.0f, %.0f)"), Mouse.X, Mouse.Y);
				Selected.Reset();
				bAttackMoveArmed = false;
				bPatrolArmed = false;
			}
		}
		bRightDown = false;
		bRightScrolling = false;
	}

	// a roda como eixo: dois cliques no mesmo quadro contam dois (como tecla, contavam um)
	In.ZoomSteps += GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	// zoom pelo teclado (ADR-0003): = − PgUp PgDn
	const bool bZoomIn = IsInputKeyDown(EKeys::Equals) || IsInputKeyDown(EKeys::Add) || IsInputKeyDown(EKeys::PageUp);
	const bool bZoomOut = IsInputKeyDown(EKeys::Hyphen) || IsInputKeyDown(EKeys::Subtract) || IsInputKeyDown(EKeys::PageDown);
	In.ZoomSteps += ((bZoomIn ? 1.0f : 0.0f) - (bZoomOut ? 1.0f : 0.0f)) * KeyboardZoomStepsPerSec * DeltaTime;

	// botão do meio: pros lados gira, pra cima e pra baixo deita a câmera até a vista quase de lado das
	// artes (o Felipe, 10/10: "nossas gameplays podem ficar na horizontal também"); dois cliques voltam
	// ao padrão
	if (WasInputKeyJustPressed(EKeys::MiddleMouseButton) && bHasMouse)
	{
		const double Now = FPlatformTime::Seconds();
		In.bReset = Now - LastMiddleClickTime < DoubleClickSeconds;
		LastMiddleClickTime = Now;
		bMiddleDown = true;
		MiddleAnchor = Mouse;
	}
	if (bMiddleDown && bHasMouse)
	{
		// pelo movimento do mouse, como o WorldForge e o BlastLab (o Felipe, 10/10: "tem que ser algo
		// parecido"): 0,3° de giro e 0,2° de inclinação por pixel; pra cima deita até quase a horizontal.
		// O modo antigo (a velocidade pela distância do ponto apertado) era ruim de controlar
		const FVector2D Delta = Mouse - MiddleAnchor;
		In.YawDelta += float(Delta.X) * 0.3f;
		In.PitchDelta += float(Delta.Y) * 0.2f;
		MiddleAnchor = Mouse;
	}
	if (WasInputKeyJustReleased(EKeys::MiddleMouseButton))
	{
		bMiddleDown = false;
	}
	Cam->ApplyInput(In, DeltaTime);
}

void ARTSPlayerController::TickMinimap(float DeltaTime, ARTSCameraPawn* Cam, const FVector2D& Mouse, bool bHasMouse)
{
	for (int32 I = MinimapPulses.Num() - 1; I >= 0; --I)
	{
		MinimapPulses[I].Age += DeltaTime;
		if (MinimapPulses[I].Age > 0.7f)
		{
			MinimapPulses.RemoveAt(I);
		}
	}
	ARTSHUD* HUD = GetRTSHUD();
	URTSMatchSubsystem* M = Match();
	if (!HUD || !M || !bHasMouse)
	{
		return;
	}
	const bool bLeftPressed = WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bRightPressed = WasInputKeyJustPressed(EKeys::RightMouseButton);
	if (MinimapGesture == EMinimapGesture::None && bOverMinimap && (bLeftPressed || bRightPressed))
	{
		FVector2D Cm;
		const bool bOnMap = HUD->MinimapToWorld(Mouse, Cm);
		if (!bOnMap)
		{
			MinimapGesture = EMinimapGesture::Ignore;   // a moldura fora do losango: só engole o clique
		}
		else if (bLeftPressed && TargetingPower >= 0)
		{
			// (v4) mirando o poder, o esquerdo no minimapa usa ali (como no C&C); ponto que não vale: segue mirando, com o motivo
			TryUsePower(Cm);
			MinimapGesture = EMinimapGesture::LeftDone;
		}
		else if (bLeftPressed && !Selected.IsEmpty())
		{
			// com seleção, o esquerdo manda (andar, ou ataque andando se armado), como no ZH
			const FRTSBakedMap* Map = M->BakedMap();
			M->OrderMove(Selected, Cm, bAttackMoveArmed, IsInputKeyDown(EKeys::F));
			AddMarker(FVector(Cm.X, Cm.Y, Map ? Map->GroundZCm(Cm) : 0.0f), bAttackMoveArmed);
			MinimapPulses.Add({ Cm, 0.0f, bAttackMoveArmed });
			bAttackMoveArmed = false;
			++Orders;
			MinimapGesture = EMinimapGesture::LeftDone;
		}
		else
		{
			// sem seleção, ou o direito: a câmera vai pro ponto, e segurando arrasta
			if (Cam)
			{
				Cam->JumpTo(Cm);
			}
			bAttackMoveArmed = false;
			MinimapGesture = bLeftPressed ? EMinimapGesture::LeftDrag : EMinimapGesture::RightDrag;
		}
	}
	const bool bLeftHeld = IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRightHeld = IsInputKeyDown(EKeys::RightMouseButton);
	if ((MinimapGesture == EMinimapGesture::LeftDrag && bLeftHeld) || (MinimapGesture == EMinimapGesture::RightDrag && bRightHeld))
	{
		FVector2D Cm;
		if (Cam && HUD->MinimapToWorld(Mouse, Cm, true))
		{
			Cam->JumpTo(Cm);
		}
	}
	if (MinimapGesture != EMinimapGesture::None && !bLeftHeld && !bRightHeld)
	{
		MinimapGesture = EMinimapGesture::None;
	}
	// Espaço: a câmera vai pro último "sob ataque" (ZH: não precisa de radar pra isso)
	FVector2D LastActionCm;
	if (WasInputKeyJustPressed(EKeys::SpaceBar) && Cam && M && M->IsLocalDefeated() && M->LastActionCm(LastActionCm))
	{
		Cam->JumpTo(LastActionCm);   // o espectador: a última morte à vista (o "último evento" do ZH)
	}
	else if (WasInputKeyJustPressed(EKeys::SpaceBar) && bHasAlert && Cam)
	{
		Cam->JumpTo(LastAlertCm);
	}
}

void ARTSPlayerController::TickAlerts(float DeltaTime)
{
	AttackMessageAge += DeltaTime;
	for (int32 I = MinimapPings.Num() - 1; I >= 0; --I)
	{
		MinimapPings[I].Age += DeltaTime;
		if (MinimapPings[I].Age > 4.0f)
		{
			MinimapPings.RemoveAt(I);
		}
	}
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	TArray<FVector2D> Hits;
	M->TakeDamageAlerts(Hits);
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	ARTSHUD* HUD = GetRTSHUD();
	const FRTSBakedMap* Map = M->BakedMap();
	const double Now = FPlatformTime::Seconds();
	for (const FVector2D& Cm : Hits)
	{
		// só o que você não está vendo: fora da tela, ou embaixo do minimapa
		FVector2D S;
		const bool bOnScreen = ProjectWorldLocationToScreen(FVector(Cm.X, Cm.Y, Map ? Map->GroundZCm(Cm) : 0.0f), S)
			&& S.X >= 0 && S.Y >= 0 && S.X < W && S.Y < H && !(HUD && HUD->IsOverMinimap(S));
		if (bOnScreen)
		{
			continue;
		}
		// um aviso a cada 10 s num raio de ~66 m (250 unidades do ZH)
		bool bRecent = false;
		for (const FRTSMinimapPing& Old : MinimapPings)
		{
			bRecent |= FVector2D::Distance(Old.Cm, Cm) < 6600.0;
		}
		if (bRecent || (bHasAlert && FVector2D::Distance(LastAlertCm, Cm) < 6600.0 && Now - LastAlertTime < 10.0))
		{
			continue;
		}
		MinimapPings.Add({ Cm, 0.0f });
		LastAlertCm = Cm;
		LastAlertTime = Now;
		bHasAlert = true;
		AttackMessageAge = 0.0f;
	}
	// (v4) o ataque de mísseis chegando perto do que é seu: o ponto pisca no minimapa e o Espaço leva a câmera até lá.
	// Os avisos do mesmo quadro têm o mesmo At (dois inimigos no mesmo passo, dois passos num quadro): a chave de "já
	// posto" é o At e a ordem dentro dele (a lista só cresce no fim e sai inteira por idade)
	int32 SameAt = 0;
	for (const URTSMatchSubsystem::FHudNotice& N : M->HudNotices())
	{
		if (N.Kind != URTSMatchSubsystem::FHudNotice::EKind::StrikeIncoming || N.At < LastStrikeNoticeAt)
		{
			continue;
		}
		if (N.At > LastStrikeNoticeAt)
		{
			LastStrikeNoticeAt = N.At;
			LastStrikeNoticeCount = 0;
			SameAt = 0;
		}
		if (++SameAt > LastStrikeNoticeCount)
		{
			LastStrikeNoticeCount = SameAt;
			MinimapPings.Add({ N.Cm, 0.0f });
			LastAlertCm = N.Cm;
			LastAlertTime = Now;
			bHasAlert = true;
		}
	}
}

// ---------------------------------------------------------------- (v4, 23/10) a mira do poder do general

const FText& ARTSPlayerController::GetPowerMessage(float& OutAgeS) const
{
	OutAgeS = float(FPlatformTime::Seconds() - PowerMessageAt);
	return PowerMessage;
}

void ARTSPlayerController::SetPowerMessage(const FText& Text)
{
	PowerMessage = Text;
	PowerMessageAt = FPlatformTime::Seconds();
}

void ARTSPlayerController::BeginTargeting(int32 Power)
{
	URTSMatchSubsystem* M = Match();
	if (!M || M->IsLocalDefeated())
	{
		return;
	}
	if (TargetingPower == Power)
	{
		TargetingPower = -1;   // o mesmo botão (ou a tecla) de novo desiste
		return;
	}
	// sem estar pronto (a espera, sem o Command Center) nem entra na mira: o motivo aparece no mouse
	const FText Why = M->PowerRefusal(Power);
	if (!Why.IsEmpty())
	{
		SetPowerMessage(Why);
		UE_LOG(LogTemp, Log, TEXT("RTS poder: o poder %d recusado no botão (%s)"), Power, *Why.ToString());
		return;
	}
	TargetingPower = Power;
	PlacingDef = -1;
	bAttackMoveArmed = false;
	PowerMessage = FText::GetEmpty();
	UE_LOG(LogTemp, Log, TEXT("RTS poder: mirando o poder %d"), Power);
}

bool ARTSPlayerController::TickTargeting(const FVector2D& Mouse, bool bHasMouse)
{
	if (TargetingPower < 0)
	{
		return false;
	}
	URTSMatchSubsystem* M = Match();
	if (!M || M->IsLocalDefeated())
	{
		TargetingPower = -1;
		return false;
	}
	// o poder deixou de valer no meio da mira (o Command Center caiu): sai, com o motivo
	const FText Why = M->PowerRefusal(TargetingPower);
	if (!Why.IsEmpty())
	{
		SetPowerMessage(Why);
		TargetingPower = -1;
		return false;
	}
	const ARTSHUD* HUD = GetRTSHUD();
	if ((HUD && !HUD->ButtonAt(Mouse).IsNone()) || bOverMinimap)
	{
		return false;   // os botões do painel e o minimapa seguem valendo
	}
	FVector Ground;
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bHasMouse && GroundUnderCursor(Mouse, Ground))
	{
		TryUsePower(FVector2D(Ground.X, Ground.Y));
	}
	return true;   // mirando: o clique esquerdo é do poder (nem caixa nem ordem)
}

bool ARTSPlayerController::TryUsePower(const FVector2D& Cm)
{
	URTSMatchSubsystem* M = Match();
	if (!M || TargetingPower < 0)
	{
		return false;
	}
	const FText Why = M->PowerRefusal(TargetingPower, &Cm);
	if (!Why.IsEmpty())
	{
		SetPowerMessage(Why);
		UE_LOG(LogTemp, Log, TEXT("RTS poder: o ponto (%.0f, %.0f) m recusado (%s)"), Cm.X / 100.0, Cm.Y / 100.0, *Why.ToString());
		return false;
	}
	M->OrderUsePower(TargetingPower, Cm);
	const FRTSBakedMap* Map = M->BakedMap();
	AddMarker(FVector(Cm.X, Cm.Y, Map ? Map->GroundZCm(Cm) : 0.0f), true);
	++Orders;
	TargetingPower = -1;
	PowerMessage = FText::GetEmpty();
	return true;
}

bool ARTSPlayerController::GetGuardPreview(FVector2D& OutCenterCm, float& OutRadiusCm, uint32& OutTarget, bool& bOutDragging) const
{
	const URTSMatchSubsystem* M = Match();
	if (!bGuardArmed || !M)
	{
		return false;
	}
	bOutDragging = bGuardPressed && bGuardDragged;
	OutCenterCm = bGuardPressed ? GuardPressCm : GuardHoverCm;
	OutRadiusCm = bOutDragging ? GuardDragRadiusCm : M->GuardRadiusM(Selected) * 100.0f;
	OutTarget = bGuardPressed ? GuardPressTarget : GuardHoverTarget;
	return true;
}

bool ARTSPlayerController::TickGuard(const FVector2D& Mouse, bool bHasMouse)
{
	// (v5, 25/10) a guarda (o AIGuard do ZH, com o que o Felipe pediu): o esquerdo que desce no chão marca o centro; arrastado, a
	// distância até o mouse é o raio (entre o mínimo e o máximo da simulação); soltou: a ordem. O clique (sem arraste) num amigo, a
	// unidade ou o prédio, é a escolta dele
	if (!bGuardArmed)
	{
		return false;
	}
	URTSMatchSubsystem* M = Match();
	if (!M || Selected.IsEmpty() || M->IsLocalDefeated())
	{
		bGuardArmed = false;
		bGuardPressed = false;
		return false;
	}
	const ARTSHUD* HUD = GetRTSHUD();
	if (!bGuardPressed && ((HUD && !HUD->ButtonAt(Mouse).IsNone()) || bOverMinimap))
	{
		return false;   // os botões do painel e o minimapa seguem valendo
	}
	FVector Ground = FVector::ZeroVector;
	const bool bGround = bHasMouse && GroundUnderCursor(Mouse, Ground);
	if (bGround)
	{
		GuardHoverCm = FVector2D(Ground.X, Ground.Y);
	}
	GuardHoverTarget = 0;
	const uint32 Under = bHasMouse ? PickUnit(Mouse) : 0;
	FRTSUnitState U;
	if (Under != 0 && M->GetUnit(Under, U) && U.Hp > 0 && !M->IsEnemyOfLocal(U.Owner) && !(Selected.Num() == 1 && Selected.Contains(Under)))
	{
		GuardHoverTarget = Under;
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bGround)
	{
		bGuardPressed = true;
		bGuardDragged = false;
		GuardPressPx = Mouse;
		GuardPressCm = GuardHoverCm;
		GuardPressTarget = GuardHoverTarget;
		GuardDragRadiusCm = 0.0f;
	}
	if (bGuardPressed && IsInputKeyDown(EKeys::LeftMouseButton) && FVector2D::Distance(Mouse, GuardPressPx) > DragThresholdPx)
	{
		bGuardDragged = true;
		GuardPressTarget = 0;   // arrastar é sempre a área
		GuardDragRadiusCm = FMath::Clamp(float(FVector2D::Distance(GuardPressCm, GuardHoverCm)), float(Sim::GuardRadiusMinM) * 100.0f, float(Sim::GuardRadiusMaxM) * 100.0f);
	}
	if (bGuardPressed && WasInputKeyJustReleased(EKeys::LeftMouseButton))
	{
		const FRTSBakedMap* Map = M->BakedMap();
		if (GuardPressTarget != 0 && M->GetUnit(GuardPressTarget, U))
		{
			M->OrderGuard(Selected, U.PosCm, GuardPressTarget, 0.0f);
			AddMarker(FVector(U.PosCm.X, U.PosCm.Y, Map ? Map->GroundZCm(U.PosCm) : 0.0f), false);
			UE_LOG(LogTemp, Log, TEXT("RTS guarda: %d unidades escoltam %u"), Selected.Num(), GuardPressTarget);
		}
		else
		{
			const float Radius = bGuardDragged ? GuardDragRadiusCm : M->GuardRadiusM(Selected) * 100.0f;
			M->OrderGuard(Selected, GuardPressCm, 0, Radius);
			AddMarker(FVector(GuardPressCm.X, GuardPressCm.Y, Map ? Map->GroundZCm(GuardPressCm) : 0.0f), false);
			UE_LOG(LogTemp, Log, TEXT("RTS guarda: %d unidades guardam (%.0f, %.0f) m, raio %.0f m"), Selected.Num(), GuardPressCm.X / 100.0, GuardPressCm.Y / 100.0, Radius / 100.0);
		}
		++Orders;
		bGuardArmed = false;
		bGuardPressed = false;
	}
	return true;
}

void ARTSPlayerController::HandleLeftClick(const FVector2D& Mouse)
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (bPatrolArmed)
	{
		// (v5) a patrulha que volta: as aeronaves armadas do grupo patrulham o ponto (atacam o que entra, rearmam e voltam sozinhas)
		FVector Ground;
		if (!Selected.IsEmpty() && GroundUnderCursor(Mouse, Ground))
		{
			M->OrderPatrol(Selected, FVector2D(Ground.X, Ground.Y));
			AddMarker(Ground, true);
			++Orders;
		}
		bPatrolArmed = false;
		return;
	}
	if (bAttackMoveArmed)
	{
		FVector Ground;
		if (!Selected.IsEmpty() && GroundUnderCursor(Mouse, Ground))
		{
			M->OrderMove(Selected, FVector2D(Ground.X, Ground.Y), /*bAttackMove=*/true, IsInputKeyDown(EKeys::F));
			AddMarker(Ground, true);
			++Orders;
		}
		bAttackMoveArmed = false;
		return;
	}

	// o tiro forçado do C&C (Ctrl, pedido do Felipe em 13/10): ataca o que estiver embaixo do cursor, de qualquer dono
	// (a própria unidade, o aliado, o prédio), ou o chão, onde o tiro cava a cratera da arma
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bCtrl && !Selected.IsEmpty())
	{
		const uint32 Target = PickUnit(Mouse);
		FRTSUnitState TU;
		const FRTSBakedMap* Map = M->BakedMap();
		if (Target != 0 && M->GetUnit(Target, TU) && !(Selected.Num() == 1 && Selected.Contains(Target)))
		{
			M->OrderAttack(Selected, Target, /*bForced=*/true);
			AddMarker(FVector(TU.PosCm.X, TU.PosCm.Y, Map ? Map->GroundZCm(TU.PosCm) : 0.0f), true);
			++Orders;
			return;
		}
		FVector Ground;
		if (GroundUnderCursor(Mouse, Ground))
		{
			M->OrderAttackGround(Selected, FVector2D(Ground.X, Ground.Y));
			AddMarker(Ground, true);
			++Orders;
		}
		return;
	}

	const uint32 Hit = PickUnit(Mouse);
	if (HandleEconomyClick(Mouse, Hit))
	{
		return;   // ponto de encontro, coletar, retomar a obra
	}
	FRTSUnitState U;
	if (Hit != 0 && M->GetUnit(Hit, U))
	{
		// (simulação 35) com soldado selecionado, o clique no próprio transporte com vaga manda entrar (o ZH); com o Shift, soma à seleção
		if (U.Owner == M->LocalPlayer() && !bShift && !Selected.Contains(Hit) && M->CanEnter(Selected, Hit))
		{
			M->OrderEnter(Selected, Hit);
			++Orders;
			const FRTSBakedMap* Map = M->BakedMap();
			AddMarker(FVector(U.PosCm.X, U.PosCm.Y, Map ? Map->GroundZCm(U.PosCm) : 0.0f), false);
			return;
		}
		if (U.Owner == M->LocalPlayer())
		{
			const double Now = FPlatformTime::Seconds();
			if (Hit == LastClickedId && Now - LastClickTime < DoubleClickSeconds)
			{
				SelectSameTypeOnScreen(Hit);
			}
			else if (bShift)
			{
				if (Selected.Contains(Hit)) { Selected.Remove(Hit); } else { Selected.Add(Hit); }
			}
			else
			{
				Selected = { Hit };
			}
			LastClickedId = Hit;
			LastClickTime = Now;
			return;
		}
		if (M->IsEnemyOfLocal(U.Owner) && (Selected.IsEmpty() || M->CanAnyTarget(Selected, Hit)))   // (v5) o tanque não ataca a aeronave no ar: vira andar
		{
			if (!Selected.IsEmpty())
			{
				M->OrderAttack(Selected, Hit);
				++Orders;
				const FRTSBakedMap* Map = M->BakedMap();
				AddMarker(FVector(U.PosCm.X, U.PosCm.Y, Map ? Map->GroundZCm(U.PosCm) : 0.0f), true);
			}
			return;
		}
	}

	FVector Ground;
	if (!Selected.IsEmpty() && GroundUnderCursor(Mouse, Ground))
	{
		// segurando o F, o grupo vai em formação (C&C); sem, cada um na sua velocidade
		M->OrderMove(Selected, FVector2D(Ground.X, Ground.Y), /*bAttackMove=*/false, IsInputKeyDown(EKeys::F));
		AddMarker(Ground, false);
		++Orders;
	}
}

// ---------------------------------------------------------------- (v4, 23/10) a linha dos dois botões
//
// O pedido do Felipe (23/10): a linha de antes era o esquerdo arrastado com tropa selecionada, e arrastar pra pegar outros
// tanques na caixa virava a linha; só o direito também não dá (ele rola a câmera). Os jogos põem a linha no direito
// arrastado porque lá o direito dá as ordens (Total War, BAR com o Custom Formations, AoE4); no nosso esquema do ZH o esquerdo
// faz tudo e o direito é a câmera, e o gesto livre é o acorde dos dois, como o Supreme Commander (esquerdo com o direito
// apertado) e o campo minado do Windows. Puxar da esquerda pra direita deixa a frente pra longe da câmera; ao contrário, pra
// ela (a regra do lado do Total War). As vagas aparecem no chão enquanto puxa (ARTSHUD::DrawLinePreview), pela mesma conta da
// simulação (World::PreviewLineSlots): o anel fica onde o tanque vai parar.

bool ARTSPlayerController::HasOwnTroopsSelected() const
{
	// só com tropa: prédio não anda (o teste base de 13/10 virou "linha de frente" com a War Factory selecionada)
	const URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return false;
	}
	for (const uint32 Id : Selected)
	{
		FRTSUnitState S;
		if (M->GetUnit(Id, S) && !S.bStructure && S.Owner == M->LocalPlayer())
		{
			return true;
		}
	}
	return false;
}

bool ARTSPlayerController::GetLineDrag(FLineDrag& Out) const
{
	if ((LineChord != ELineChord::Active && LineChord != ELineChord::Releasing) || !bLineOpened)
	{
		return false;
	}
	const FVector2D D = LineEndCm - LineStartCm;
	const double Len = D.Size();
	if (Len < double(MinLineM) * 100.0)
	{
		return false;
	}
	// a linha mais comprida que o teto do comando fica no teto, a partir do ponto apertado
	const double L = FMath::Min(Len, double(MaxLineM) * 100.0);
	Out.StartCm = LineStartCm;
	Out.EndCm = LineStartCm + D / Len * L;
	// o ponto da ordem é o meio: a fileira da frente fica em cima da linha puxada, as outras atrás. A largura passa sempre (até
	// uma vaga de largura, a simulação já usa a do grupo)
	Out.CenterCm = (Out.StartCm + Out.EndCm) * 0.5;
	Out.WidthM = float(L / 100.0);
	// a frente é a linha girada −90° no giro: na vista de cima (Z pra cima), com a câmera no giro θ a direita da tela é
	// (−sen θ, cos θ), que vira (cos θ, sen θ), a frente da câmera. Da esquerda pra direita: de costas pra câmera
	Out.FacingDeg = float(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))) - 90.0f;
	Out.bAttack = bAttackMoveArmed;
	Out.bFormation = IsInputKeyDown(EKeys::F);
	return true;
}

void ARTSPlayerController::TickLineChord(const FVector2D& Mouse, bool bHasMouse)
{
	const bool bLeftHeld = IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRightHeld = IsInputKeyDown(EKeys::RightMouseButton);
	const double Now = FPlatformTime::Seconds();
	switch (LineChord)
	{
	case ELineChord::None:
	{
		// o acorde forma quando o segundo botão desce com o primeiro ainda "livre": o esquerdo que começou no mundo (fora de botão,
		// minimapa, prédio sendo posto e mira) e não virou caixa (a menos de 6 px, ou apertado há até 0,2 s), e o direito que
		// ainda não rolou. O primeiro que já virou caixa ou rolagem deixa o segundo com o significado de hoje
		const bool bLeftNew = WasInputKeyJustPressed(EKeys::LeftMouseButton);
		const bool bRightNew = WasInputKeyJustPressed(EKeys::RightMouseButton);
		if (!bHasMouse || !bLeftHeld || !bRightHeld || !(bLeftNew || bRightNew) || !bLeftDown || !bRightDown || bRightScrolling)
		{
			return;
		}
		const bool bLeftFree = FVector2D::Distance(LeftDownPos, Mouse) <= DragThresholdPx || Now - LeftDownAt <= ChordPressGraceSeconds;
		if (!bLeftFree || PlacingDef >= 0 || TargetingPower >= 0 || !HasOwnTroopsSelected())
		{
			return;
		}
		// a linha sai de onde o PRIMEIRO botão desceu (os dois no mesmo quadro: o mouse), guardada no chão
		const FVector2D StartPx = (bLeftNew && bRightNew) ? Mouse : (bRightNew ? LeftDownPos : RightAnchor);
		FVector Ground;
		if (!GroundUnderCursor(StartPx, Ground))
		{
			return;
		}
		LineChord = ELineChord::Active;
		LineStartCm = FVector2D(Ground.X, Ground.Y);
		LineEndCm = LineStartCm;
		bLineOpened = false;
		UE_LOG(LogTemp, Log, TEXT("RTS linha: os dois botões (%s antes) em (%.0f, %.0f) m"),
			(bLeftNew && bRightNew) ? TEXT("juntos") : (bRightNew ? TEXT("o esquerdo") : TEXT("o direito")), LineStartCm.X / 100.0, LineStartCm.Y / 100.0);
		return;
	}
	case ELineChord::Active:
	{
		if (PlacingDef >= 0 || TargetingPower >= 0 || !HasOwnTroopsSelected())
		{
			CancelLine(TEXT("a seleção ficou sem tropa, ou entrou o prédio ou a mira"));
			return;
		}
		// a ponta segue o chão sob o cursor (sem chão no raio, fica a última boa)
		FVector Ground;
		if (bHasMouse && GroundUnderCursor(Mouse, Ground))
		{
			LineEndCm = FVector2D(Ground.X, Ground.Y);
		}
		// a zona morta é na tela, do ponto apertado reprojetado (a câmera pode ter andado)
		const URTSMatchSubsystem* M = Match();
		const FRTSBakedMap* Map = M ? M->BakedMap() : nullptr;
		FVector2D StartPx;
		if (!bLineOpened && bHasMouse && ProjectWorldLocationToScreen(FVector(LineStartCm.X, LineStartCm.Y, Map ? Map->GroundZCm(LineStartCm) : 0.0f), StartPx)
			&& FVector2D::Distance(StartPx, Mouse) > DragThresholdPx)
		{
			bLineOpened = true;
		}
		if (!bLeftHeld && !bRightHeld)
		{
			CommitLine();
		}
		else if (!bLeftHeld || !bRightHeld)
		{
			// um subiu: a ponta congela, e o outro tem a folga pra subir junto (ninguém solta os dois no mesmo quadro)
			LineChord = ELineChord::Releasing;
			LineReleaseAt = Now;
		}
		return;
	}
	case ELineChord::Releasing:
	{
		if (!bLeftHeld && !bRightHeld)
		{
			CommitLine();   // o outro subiu dentro da folga: a linha congelada
		}
		else if (bLeftHeld && bRightHeld)
		{
			LineChord = ELineChord::Active;   // o dedo voltou: segue puxando
		}
		else if (Now - LineReleaseAt > ChordReleaseGraceSeconds)
		{
			CancelLine(TEXT("soltou um botão e segurou o outro"));
		}
		else if (!HasOwnTroopsSelected())
		{
			CancelLine(TEXT("a seleção ficou sem tropa"));
		}
		return;
	}
	case ELineChord::Swallow:
		return;
	}
}

void ARTSPlayerController::CommitLine()
{
	URTSMatchSubsystem* M = Match();
	FLineDrag L;
	if (!M || !GetLineDrag(L))
	{
		CancelLine(TEXT("a linha não saiu da zona morta"));
		return;
	}
	TArray<URTSMatchSubsystem::FLineSlot> Slots;
	M->PreviewLineMove(Selected, L.CenterCm, L.bFormation, L.FacingDeg, L.WidthM, Slots);   // (só pro registro: as vagas da ordem)
	M->OrderMove(Selected, L.CenterCm, L.bAttack, L.bFormation, true, L.FacingDeg, L.WidthM);
	const FRTSBakedMap* Map = M->BakedMap();
	AddMarker(FVector(L.CenterCm.X, L.CenterCm.Y, Map ? Map->GroundZCm(L.CenterCm) : 0.0f), L.bAttack);
	UE_LOG(LogTemp, Log, TEXT("RTS ordem: linha de (%.0f, %.0f) a (%.0f, %.0f) m, virada a %.0f°, %.0f m, %d vagas%s"),
		L.StartCm.X / 100.0, L.StartCm.Y / 100.0, L.EndCm.X / 100.0, L.EndCm.Y / 100.0, L.FacingDeg, L.WidthM, Slots.Num(),
		L.bAttack ? TEXT(", ataque andando") : TEXT(""));
	bAttackMoveArmed = false;
	++Orders;
	LineChord = ELineChord::Swallow;
}

void ARTSPlayerController::CancelLine(const TCHAR* Why)
{
	UE_LOG(LogTemp, Log, TEXT("RTS linha: desistiu (%s), nenhuma ordem"), Why);
	LineChord = ELineChord::Swallow;
}

void ARTSPlayerController::HandleBoxSelect(const FVector2D& A, const FVector2D& B)
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	const FBox2D Box(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	const FRTSBakedMap* Map = M->BakedMap();
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	TArray<uint32> Found;
	for (const FRTSUnitState& U : Units)
	{
		if (U.Owner != M->LocalPlayer() || U.bStructure)
		{
			continue;   // a caixa só pega unidade própria (ADR-0004), e prédio não (ZH)
		}
		FVector2D Screen;
		const FVector World(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + (U.bAir ? U.AltCm : 0.0f) + PickHeightCm);   // (v5) a aeronave no ar
		if (ProjectWorldLocationToScreen(World, Screen) && Box.IsInside(Screen))
		{
			Found.Add(U.Id);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RTS caixa: (%.0f, %.0f)-(%.0f, %.0f), pegou %d"), Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, Found.Num());
	if (Found.IsEmpty())
	{
		// caixa vazia não mexe na seleção (ZH, SelectionXlat::onMouseLeftClick): um clique que
		// escorrega uns pixels não pode desfazer o grupo, e caixa não dá ordem
		return;
	}
	if (!bShift)
	{
		Selected = Found;
		return;
	}
	for (const uint32 Id : Found)
	{
		Selected.AddUnique(Id);
	}
}

void ARTSPlayerController::SelectIdleBuilders(bool bAll)
{
	// (01/11, ele: "pra clicar pra ir pra próxima unidade de construção liberada") cada clique pega o parado seguinte (na ordem do Id, e
	// volta pro primeiro) e leva a câmera até ele; o Ctrl pega todos (o Ctrl+F1 do StarCraft II) e vai pro primeiro
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		return;
	}
	TArray<uint32> Idle;
	M->GetIdleBuilders(M->LocalPlayer(), Idle);
	if (Idle.IsEmpty())
	{
		return;
	}
	int32 K = 0;
	while (K < Idle.Num() && Idle[K] <= LastIdleBuilder)
	{
		++K;
	}
	const uint32 Next = bAll ? Idle[0] : Idle[K % Idle.Num()];
	LastIdleBuilder = Next;
	Selected = bAll ? Idle : TArray<uint32>{ Next };
	FRTSUnitState U;
	if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn()); Cam && M->GetUnit(Next, U))
	{
		Cam->JumpTo(U.PosCm);
	}
}

void ARTSPlayerController::SelectSameTypeOnScreen(uint32 Id)
{
	URTSMatchSubsystem* M = Match();
	FRTSUnitState Clicked;
	if (!M || !M->GetUnit(Id, Clicked))
	{
		return;
	}
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	const FBox2D Screen(FVector2D::ZeroVector, FVector2D(W, H));
	const FRTSBakedMap* Map = M->BakedMap();
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	Selected.Reset();
	for (const FRTSUnitState& U : Units)
	{
		FVector2D P;
		const FVector World(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + (U.bAir ? U.AltCm : 0.0f) + PickHeightCm);
		if (U.Owner == Clicked.Owner && U.Def == Clicked.Def && ProjectWorldLocationToScreen(World, P) && Screen.IsInside(P))
		{
			Selected.Add(U.Id);
		}
	}
}

void ARTSPlayerController::HandleGroups()
{
	URTSMatchSubsystem* M = Match();
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	for (int32 K = 0; K < 10; ++K)
	{
		if (!WasInputKeyJustPressed(GroupKey(K)))
		{
			continue;
		}
		if (bCtrl)
		{
			Groups.Add(K, Selected);
			continue;
		}
		if (bShift)
		{
			// Shift+número: a seleção atual entra no grupo (ADR-0004)
			TArray<uint32>& Group = Groups.FindOrAdd(K);
			for (const uint32 Id : Selected)
			{
				Group.AddUnique(Id);
			}
			continue;
		}
		const TArray<uint32>* Group = Groups.Find(K);
		if (!Group)
		{
			continue;
		}
		Selected = *Group;
		PruneSelection();
		const double Now = FPlatformTime::Seconds();
		if (LastGroupKey == K && Now - LastGroupKeyTime < 0.4 && M && !Selected.IsEmpty())
		{
			// dois toques: a câmera vai até o grupo
			FVector2D Sum = FVector2D::ZeroVector;
			int32 Count = 0;
			for (const uint32 Id : Selected)
			{
				FRTSUnitState U;
				if (M->GetUnit(Id, U))
				{
					Sum += U.PosCm;
					++Count;
				}
			}
			if (ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(GetPawn()); Cam && Count > 0)
			{
				Cam->GlideTo(Sum / Count);   // desliza mantendo o giro e o zoom
			}
		}
		LastGroupKey = K;
		LastGroupKeyTime = Now;
	}
}

uint32 ARTSPlayerController::PickUnit(const FVector2D& Mouse) const
{
	URTSMatchSubsystem* M = Match();
	if (!M || !PlayerCameraManager)
	{
		return 0;
	}
	const FRTSBakedMap* Map = M->BakedMap();
	const FVector CamRight = FRotationMatrix(PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	TArray<FRTSUnitState> Units;
	M->GetUnits(Units);
	uint32 Best = 0;
	float BestScore = TNumericLimits<float>::Max();
	for (const FRTSUnitState& U : Units)
	{
		if (!M->IsUnitShown(U.Id) || U.bStructure)
		{
			continue;   // na névoa não se clica (o ZH não deixa); prédio é pela área no chão, abaixo
		}
		const FRTSUnitVisual* Visual = M->VisualOf(U.Def);
		const float RadiusCm = Visual ? Visual->RadiusM * 100.0f : 300.0f;
		// (v5, 24/10) a aeronave: na tela onde ela está no ar, e na frente do que está no chão embaixo (o clique a pega primeiro)
		const FVector Center(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + (U.bAir ? U.AltCm : 0.0f) + PickHeightCm);
		FVector2D C;
		FVector2D Edge;
		if (!ProjectWorldLocationToScreen(Center, C) || !ProjectWorldLocationToScreen(Center + CamRight * RadiusCm, Edge))
		{
			continue;
		}
		const float RadiusPx = FMath::Max(14.0f, FVector2D::Distance(C, Edge));
		const float Dist = FVector2D::Distance(C, Mouse);
		const float Score = Dist / RadiusPx * (U.bAir && U.AltCm > 200.0f ? 0.6f : 1.0f);
		if (Dist <= RadiusPx && Score < BestScore)
		{
			BestScore = Score;
			Best = U.Id;
		}
	}
	FVector Ground;
	if (Best == 0 && GroundUnderCursor(Mouse, Ground))
	{
		Best = StructureAt(Ground);
	}
	return Best;
}

bool ARTSPlayerController::GroundUnderCursor(const FVector2D& Mouse, FVector& Out) const
{
	FVector Origin;
	FVector Dir;
	if (!DeprojectScreenPositionToWorld(Mouse.X, Mouse.Y, Origin, Dir))
	{
		return false;
	}
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * 500000.0, ECC_Visibility))
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	// (16/10) o plano do chão só na frente da câmera: com a vista no céu (a câmera que olha pra cima) não há chão no cursor
	if (Dir.Z < -KINDA_SMALL_NUMBER && -Origin.Z / Dir.Z > 0.0)
	{
		Out = Origin + Dir * (-Origin.Z / Dir.Z);
		return true;
	}
	return false;
}

void ARTSPlayerController::PruneSelection()
{
	URTSMatchSubsystem* M = Match();
	if (!M)
	{
		Selected.Reset();
		return;
	}
	Selected.RemoveAll([M](uint32 Id)
	{
		// (v4, 23/10) o prédio sendo vendido sai da seleção (o OBJECT_STATUS_UNSELECTABLE do ZH)
		FRTSUnitState U;
		return !M->GetUnit(Id, U) || U.bSelling || U.Inside != 0;   // (simulação 35) quem entrou no transporte sai da seleção
	});
}

void ARTSPlayerController::AddMarker(const FVector& At, bool bAttack)
{
	FRTSOrderMarker Marker;
	Marker.World = At;
	Marker.bAttack = bAttack;
	Markers.Add(Marker);
}

bool ARTSPlayerController::ReadMouse(float& X, float& Y) const
{
	if (bTestMouse)
	{
		X = float(TestMouse.X);
		Y = float(TestMouse.Y);
		return true;
	}
	return GetMousePosition(X, Y);
}
