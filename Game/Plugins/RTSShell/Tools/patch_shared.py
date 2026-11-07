# As linhas das Telas nos arquivos compartilhados (04/11, a abertura), aplicadas só na vez de compilar e por troca exata de trecho
# (nunca copiando o arquivo inteiro: as outras sessões mexem neles). "apply" troca o que ainda não foi trocado; "revert" desfaz.
#   - o jogo abre no mapa vazio do menu (DefaultEngine.ini) e volta pra ele no fim da partida (RTSSessionSubsystem.cpp);
#   - a tela de carregamento do mapa do menu é o escuro da abertura, sem o "LOADING" reprovado (RTSGameModule.cpp);
#   - (06/11) as telas da partida nativas: o RTSShellGame (o match.json, as amostras, qual tela por cima, os botões), o RTSHUD (não
#     desenha as velhas enquanto a nativa está) e o RTSGameModule (a tela de carregar do mapa de batalha é a aprovada).
import os, sys

G = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
PATCHES = [
    ('Config/DefaultEngine.ini',
     'GameDefaultMap=/Game/Maps/World_TestSalt2.World_TestSalt2',
     'GameDefaultMap=/Game/Maps/FrontEnd.FrontEnd'),
    ('Source/RTSGame/Private/RTSSessionSubsystem.cpp',
     'UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));',
     'UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("FrontEnd")), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));   // (04/11, Telas) o menu mora no mapa vazio'),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString&)
		{
			if (!GetMoviePlayer()->IsInitialized())
			{
				return;
			}
			FLoadingScreenAttributes Screen;
			Screen.bAutoCompleteWhenLoadingCompletes = true;
			Screen.MinimumLoadingScreenDisplayTime = 0.4f;
			Screen.WidgetLoadingScreen = MakeLoadingWidget();''',
     '''		PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString& Map)
		{
			if (!GetMoviePlayer()->IsInitialized())
			{
				return;
			}
			// (04/11, Telas) o mapa do menu (FrontEnd, vazio) abre em instantes: o escuro da abertura, sem o "LOADING" (reprovado)
			const bool bFrontEnd = Map.Contains(TEXT("FrontEnd"));
			FLoadingScreenAttributes Screen;
			Screen.bAutoCompleteWhenLoadingCompletes = true;
			Screen.MinimumLoadingScreenDisplayTime = bFrontEnd ? 0.0f : 0.4f;
			if (bFrontEnd)
			{
				TSharedRef<SBorder> Dark = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.003f, 0.004f, 0.0033f, 1.0f));
				Screen.WidgetLoadingScreen = Dark;
			}
			else
			{
				Screen.WidgetLoadingScreen = MakeLoadingWidget();
			}'''),
    # (06/11 à noite) a partida que não monta (o mapa sem posições, a cena com unidade que não existe): o aviso nativo com o motivo e o
    # MAIN MENU (o caso 7 do SRTSMatch), em vez do texto velho do HUD, que ficava escondido embaixo do carregando (o jogo parecia preso)
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		ERTSMatchOverlay O = ERTSMatchOverlay::None;
		if (!M->IsReady() || M->IsWaitingForPlayers())
		{''',
     '''		ERTSMatchOverlay O = ERTSMatchOverlay::None;
		if (!M->IsRunning() && !PC->GetError().IsEmpty())
		{
			O = ERTSMatchOverlay::Net;   // (06/11 à noite) a partida não montou: o aviso nativo com o motivo e o MAIN MENU
		}
		else if (!M->IsReady() || M->IsWaitingForPlayers())
		{'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''			if (bNet)
			{
				// o aviso da rede: 1 esperando quem sumiu''',
     '''			if (!M->IsRunning() && !PC->GetError().IsEmpty())
			{
				// (06/11 à noite) 7: a partida não montou (qualquer partida, não só a da rede); o motivo vai junto
				TSharedPtr<FJsonObject> Nj = MakeShared<FJsonObject>();
				Nj->SetNumberField(TEXT("case"), 7);
				Nj->SetStringField(TEXT("error"), PC->GetError());
				Root->SetObjectField(TEXT("net"), Nj);
			}
			else if (bNet)
			{
				// o aviso da rede: 1 esperando quem sumiu'''),
    ('Source/RTSGame/Private/RTSHUD.cpp',
     '''		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0, 0, 0, 0.6f));
		const bool bError = !PC->GetError().IsEmpty();''',
     '''		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0, 0, 0, 0.6f));
		if (RTSShellGame::IsMatchOverlay())
		{
			return;   // (06/11, Telas) o carregando e o erro de montar a partida são as telas nativas por cima
		}
		const bool bError = !PC->GetError().IsEmpty();'''),
    # (07/11) na sala da LAN você entra com a sua facção (a salva, a mesma com que a sala do skirmish começa), não no sorteio: o 0005
    # tinha posto "random" (antes dele, na rede todo humano era USA); o teste automático da LAN constrói a base de USA
    ('Source/RTSGame/Private/RTSNetSession.cpp',
     '''	Ais.Reset();
	if (const URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
	{
		FogMode = uint8(S->FogMode);
		StartingMoney = S->StartingMoney;
	}''',
     '''	Ais.Reset();
	if (const URTSSessionSubsystem* S = GetGameInstance()->GetSubsystem<URTSSessionSubsystem>())
	{
		FogMode = uint8(S->FogMode);
		StartingMoney = S->StartingMoney;
		// (07/11) você entra na sala com a sua facção (a salva, a mesma da sala do skirmish), não no sorteio
		MyFaction = IsFactionKey(S->PlayerFaction) ? S->PlayerFaction : FString(TEXT("random"));
	}'''),
    ('Source/RTSGame/Private/RTSNetSession.cpp',
     '''				{
					ClientSetField(FieldReady, 1, FString());
				}
			}
			return;
		}
		case EMsg::Reject:''',
     '''				{
					ClientSetField(FieldReady, 1, FString());
				}
				// (07/11) quem entra também chega com a sua facção (a salva, a mesma da sala do skirmish), não no sorteio
				const URTSSessionSubsystem* Ss = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
				if (Ss && Ss->PlayerFaction != TEXT("random") && IsFactionKey(Ss->PlayerFaction))
				{
					ClientSetField(FieldFaction, 0, Ss->PlayerFaction);
				}
			}
			return;
		}
		case EMsg::Reject:'''),
]

# (06/11) as telas da partida nativas, a caça às telas velhas: já aplicadas DE VEZ nos arquivos (o histórico do que entrou). O apply e o
# revert não passam mais por elas: uma troca mais nova que corta o texto de uma antiga fazia a antiga se reaplicar duplicada (23:28,
# o InstallMatch dentro do bloco das telas da partida: a compilação falhou e o deploy voltou a versão de antes).
DONE_0611 = [
    # (06/11) as telas da partida nativas
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''	int32 GLastPage = -1;                 // a página do menu do jogo que as telas mostram (a principal, a LAN, a sala)''',
     '''	int32 GLastPage = -1;                 // a página do menu do jogo que as telas mostram (a principal, a LAN, a sala)
	FString GLastScenario;                // (06/11) a cena do SCRIPT da partida (o RESTART joga de novo)
	FString GNextPage;                    // (06/11) a página do menu na volta (o BACK TO LOBBY do skirmish volta pra sala)'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''	void Install()
	{
		if (GInstalled)''',
     '''	// ------------------------------------------------------------ (06/11) as telas da partida nativas (o plugin, SRTSMatch)
	// o histórico do gráfico do resultado (o exército em dinheiro, o dinheiro ganho e as baixas, de 5 em 5 s de partida e no fim), a
	// cena do SCRIPT pra jogar de novo, a página do menu na volta (o BACK TO LOBBY do skirmish volta pra sala) e o preço de cada tipo
	TWeakObjectPtr<UWorld> GHistWorld;
	TArray<float> GHistT;
	TArray<TArray<float>> GHist[3];
	float GHistNext = 0.0f;
	bool GHistClosed = false;
	TMap<uint16, int32> GCost;

	URTSMatchSubsystem* MatchOf(const ARTSPlayerController* PC)
	{
		return PC && PC->GetWorld() ? PC->GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	}

	FString HexOf(const FLinearColor& C)
	{
		// como o lobby.json: a paleta do jogo são os números da tela direto (sem a curva do sRGB)
		return FString::Printf(TEXT("#%02x%02x%02x"), FMath::RoundToInt(FMath::Clamp(C.R, 0.0f, 1.0f) * 255.0f),
			FMath::RoundToInt(FMath::Clamp(C.G, 0.0f, 1.0f) * 255.0f), FMath::RoundToInt(FMath::Clamp(C.B, 0.0f, 1.0f) * 255.0f));
	}

	void SampleHistory(URTSMatchSubsystem* M)
	{
		const int32 N = M->PlayerCount();
		TArray<float> Army;
		Army.Init(0.0f, N);
		TArray<FRTSUnitState> Units;
		M->GetUnits(Units);
		for (const FRTSUnitState& U : Units)
		{
			if (U.bStructure || U.Hp <= 0 || U.Owner >= N)
			{
				continue;
			}
			int32* Cost = GCost.Find(U.Def);
			if (!Cost)
			{
				FRTSDefInfo Info;
				Cost = &GCost.Add(U.Def, M->GetDef(U.Def, Info) ? Info.Cost : 0);
			}
			Army[U.Owner] += float(*Cost);
		}
		GHistT.Add(M->MatchSeconds());
		for (TArray<TArray<float>>& H : GHist)
		{
			H.SetNum(N);
		}
		for (int32 P = 0; P < N; ++P)
		{
			const URTSMatchSubsystem::FPlayerStats St = M->StatsOf(uint8(P));
			GHist[0][P].Add(Army[P]);
			GHist[1][P].Add(float(St.MoneyEarned));
			GHist[2][P].Add(float(St.UnitsDestroyed + St.BuildingsDestroyed));
		}
	}

	void MatchTick()
	{
		FString Fake;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSMatchFake="), Fake))
		{
			return;   // a foto de teste das telas da partida (no menu, com os dados de mentira): quem põe a tela é a foto
		}
		ARTSPlayerController* PC = GPC.Get();
		URTSMatchSubsystem* M = MatchOf(PC);
		if (!PC || PC->IsMenu() || !M)
		{
			FRTSShell::SetMatchOverlay(ERTSMatchOverlay::None);
			return;
		}
		if (GHistWorld.Get() != PC->GetWorld())
		{
			GHistWorld = PC->GetWorld();
			GHistT.Reset();
			for (TArray<TArray<float>>& H : GHist)
			{
				H.Reset();
			}
			GHistNext = 0.0f;
			GHistClosed = false;
			GCost.Reset();
		}
		const bool bPlaying = M->Result() == ERTSMatchResult::Playing;
		if (M->IsReady() && bPlaying && M->MatchSeconds() >= GHistNext)
		{
			SampleHistory(M);
			GHistNext = M->MatchSeconds() + 5.0f;
		}
		else if (M->IsReady() && !bPlaying && !GHistClosed)
		{
			GHistClosed = true;
			SampleHistory(M);
		}
		// qual tela por cima (o que o HUD decidia): o carregando até a cena pronta e todos chegarem, o resultado 3 s depois do fim, os
		// avisos da rede, o Esc
		const URTSNetSession* N = Net();
		const bool bNetCase = M->IsNetMatch() && N && (N->IsDesynced() || N->IsPeerLost() || N->HostSilentSeconds() > 0.0f || N->IsNetPaused());
		ERTSMatchOverlay O = ERTSMatchOverlay::None;
		if (!M->IsReady() || M->IsWaitingForPlayers())
		{
			O = ERTSMatchOverlay::Loading;
		}
		else if (!bPlaying && M->SecondsSinceEnd() > 3.0f)
		{
			O = ERTSMatchOverlay::Result;
		}
		else if (bNetCase)
		{
			O = ERTSMatchOverlay::Net;
		}
		else if (PC->IsPauseMenuOpen())
		{
			O = ERTSMatchOverlay::Pause;
		}
		FRTSShell::SetMatchOverlay(O);
	}

	bool MatchJson(FString& OutJson, bool bHistory)
	{
		ARTSPlayerController* PC = GPC.Get();
		URTSMatchSubsystem* M = MatchOf(PC);
		const FRTSMatchIntro Intro = FRTSShell::GetIntro();
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("op"), Intro.Op);
		Root->SetStringField(TEXT("map"), Intro.Map);
		Root->SetStringField(TEXT("mapReal"), Intro.MapReal.IsEmpty() ? URTSSessionSubsystem::SkirmishMapName() : Intro.MapReal);
		if (M)
		{
			const URTSNetSession* N = Net();
			const bool bNet = M->IsNetMatch() && N;
			const uint8 Local = M->LocalPlayer();
			const ERTSMatchResult R = M->Result();
			Root->SetStringField(TEXT("kind"), bNet ? TEXT("lan") : TEXT("skirmish"));
			Root->SetStringField(TEXT("result"), R == ERTSMatchResult::Victory ? TEXT("victory") : R == ERTSMatchResult::Defeat ? TEXT("defeat")
				: R == ERTSMatchResult::Draw ? TEXT("draw") : TEXT("playing"));
			Root->SetNumberField(TEXT("time"), M->MatchSeconds());
			Root->SetNumberField(TEXT("local"), Local);
			Root->SetBoolField(TEXT("ready"), M->IsReady());
			Root->SetBoolField(TEXT("canSurrender"), R == ERTSMatchResult::Playing && !M->IsLocalDefeated());
			Root->SetBoolField(TEXT("canRestart"), !bNet && (GHasSlots || !GLastScenario.IsEmpty()));
			Root->SetBoolField(TEXT("canNetPause"), bNet && !N->IsNetPaused());
			static const TCHAR* Levels[3] = { TEXT("EASY"), TEXT("MEDIUM"), TEXT("HARD") };
			TArray<TSharedPtr<FJsonValue>> Players;
			for (int32 P = 0; P < M->PlayerCount(); ++P)
			{
				const FRTSNetMember* Mb = bNet ? N->Members().FindByPredicate([P](const FRTSNetMember& X) { return X.Slot == P; }) : nullptr;
				const FRTSLocalSlot* Ls = !bNet && GHasSlots && GSlots.IsValidIndex(P) ? &GSlots[P] : nullptr;
				const bool bAi = Mb ? Mb->bAI : Ls ? Ls->bAI : P != Local;
				const int32 Diff = Mb ? int32(Mb->Difficulty) : Ls ? int32(Ls->Difficulty) : 1;
				const FString Name = bAi ? FString::Printf(TEXT("AI · %s"), Levels[FMath::Clamp(Diff, 0, 2)])
					: Mb ? Mb->Name.ToUpper() : P == Local ? FString(FPlatformProcess::ComputerName()).ToUpper() : FString::Printf(TEXT("PLAYER %d"), P + 1);
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetStringField(TEXT("name"), Name);
				O->SetStringField(TEXT("fac"), M->FactionOf(uint8(P)));
				O->SetNumberField(TEXT("team"), M->TeamOf(uint8(P)));
				O->SetNumberField(TEXT("diff"), Diff);
				O->SetStringField(TEXT("hex"), HexOf(URTSMatchSubsystem::PlayerColor(uint8(P))));
				O->SetBoolField(TEXT("ai"), bAi);
				O->SetBoolField(TEXT("you"), P == Local);
				O->SetBoolField(TEXT("defeated"), M->IsDefeated(uint8(P)));
				O->SetBoolField(TEXT("loaded"), Mb ? Mb->bReady : true);
				const URTSMatchSubsystem::FPlayerStats St = M->StatsOf(uint8(P));
				TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
				S->SetNumberField(TEXT("ub"), St.UnitsBuilt);
				S->SetNumberField(TEXT("bb"), St.BuildingsBuilt);
				S->SetNumberField(TEXT("ud"), St.UnitsDestroyed);
				S->SetNumberField(TEXT("bd"), St.BuildingsDestroyed);
				S->SetNumberField(TEXT("ul"), St.UnitsLost);
				S->SetNumberField(TEXT("bl"), St.BuildingsLost);
				S->SetNumberField(TEXT("money"), double(St.MoneyEarned));
				S->SetNumberField(TEXT("score"), double(St.Score()));
				O->SetObjectField(TEXT("stats"), S);
				Players.Add(MakeShared<FJsonValueObject>(O));
			}
			Root->SetArrayField(TEXT("players"), Players);
			if (bHistory)
			{
				TSharedPtr<FJsonObject> H = MakeShared<FJsonObject>();
				TArray<TSharedPtr<FJsonValue>> Ts;
				for (const float T : GHistT)
				{
					Ts.Add(MakeShared<FJsonValueNumber>(T));
				}
				H->SetArrayField(TEXT("t"), Ts);
				static const TCHAR* Keys[3] = { TEXT("army"), TEXT("econ"), TEXT("kills") };
				for (int32 K = 0; K < 3; ++K)
				{
					TArray<TSharedPtr<FJsonValue>> Rows;
					for (const TArray<float>& Row : GHist[K])
					{
						TArray<TSharedPtr<FJsonValue>> Vs;
						for (const float V : Row)
						{
							Vs.Add(MakeShared<FJsonValueNumber>(V));
						}
						Rows.Add(MakeShared<FJsonValueArray>(Vs));
					}
					H->SetArrayField(Keys[K], Rows);
				}
				Root->SetObjectField(TEXT("history"), H);
			}
			if (bNet)
			{
				// o aviso da rede: 1 esperando quem sumiu, 2 voltando, 3 pausado por alguém, 4 o anfitrião mudo, 5 perdida, 6 dessincronia
				const URTSNetSession::FPauseState& Pz = N->GetPause();
				int32 Case = 0, Slot = -1;
				if (N->IsDesynced())
				{
					Case = 6;
				}
				else if (N->IsPeerLost())
				{
					Case = 5;
				}
				else if (N->HostSilentSeconds() > 0.0f)
				{
					Case = 4;
					Slot = 0;
				}
				else if (N->IsNetPaused())
				{
					Case = Pz.WaitingFor != 255 ? 1 : (Pz.ResumeIn >= 0.0f ? 2 : 3);
					Slot = Pz.WaitingFor != 255 ? Pz.WaitingFor : Pz.BySlot;
				}
				TSharedPtr<FJsonObject> Nj = MakeShared<FJsonObject>();
				Nj->SetNumberField(TEXT("case"), Case);
				Nj->SetNumberField(TEXT("slot"), Slot);
				Nj->SetNumberField(TEXT("left"), Pz.WaitLeft);
				Nj->SetNumberField(TEXT("silent"), N->HostSilentSeconds());
				Nj->SetNumberField(TEXT("resumeIn"), Pz.ResumeIn);
				Nj->SetNumberField(TEXT("votes"), FMath::CountBits(uint32(Pz.Votes)));
				Nj->SetNumberField(TEXT("needed"), Pz.VotesNeeded);
				Nj->SetBoolField(TEXT("voted"), (Pz.Votes & (1u << N->LocalSlot())) != 0);
				Nj->SetNumberField(TEXT("ping"), N->WorstRttMs());
				Nj->SetNumberField(TEXT("desyncTick"), double(N->GetDesyncTick()));
				Nj->SetBoolField(TEXT("host"), N->IsHost());
				Root->SetObjectField(TEXT("net"), Nj);
			}
		}
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutJson);
		return FJsonSerializer::Serialize(Root.ToSharedRef(), W);
	}

	void MatchButton(const FString& Id)
	{
		ARTSPlayerController* PC = GPC.Get();
		if (!PC)
		{
			return;
		}
		if (Id == TEXT("restart"))
		{
			// a mesma partida de novo: a mesma cena do SCRIPT, ou a montagem da sala (as vagas, o mapa e o dinheiro ficam guardados)
			if (!GLastScenario.IsEmpty())
			{
				RunScenario(GLastScenario);
			}
			else if (URTSSessionSubsystem* S = GHasSlots ? Session() : nullptr)
			{
				Leave(true);
				S->StartSkirmish();
			}
			return;
		}
		if (Id == TEXT("back_to_room"))
		{
			GNextPage = TEXT("skirmish.html");   // o menu abre já na sala do skirmish
			PC->OnButton(TEXT("menu"));
			return;
		}
		PC->OnButton(FName(*Id));
	}

	void Install()
	{
		if (GInstalled)'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''	bool Api(const FString& Path, FString& OutJson)
	{
		if (Path == TEXT("scenarios.json"))''',
     '''	bool MatchJson(FString& OutJson, bool bHistory);

	bool Api(const FString& Path, FString& OutJson)
	{
		if (Path.StartsWith(TEXT("match.json")))
		{
			return MatchJson(OutJson, Path.Contains(TEXT("h=1")));   // (06/11) as telas da partida (o histórico só pro resultado)
		}
		if (Path == TEXT("scenarios.json"))'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		H.Api = [](const FString& Path, FString& OutJson) { return Api(Path, OutJson); };''',
     '''		H.Api = [](const FString& Path, FString& OutJson) { return Api(Path, OutJson); };
		H.MatchButton = [](const FString& Id) { MatchButton(Id); };
		// (06/11) as telas da partida: qual por cima e as amostras do gráfico, a cada quadro
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { MatchTick(); return true; }), 0.0f);'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		GMap = Scenario.Map;
		SetScenarioArg(Name);''',
     '''		GMap = Scenario.Map;
		SetScenarioArg(Name);
		GLastScenario = Name;   // (06/11) o RESTART joga a mesma cena; a tela de carregar mostra o nome dela
		{
			FRTSMatchIntro In;
			In.Op = Name.ToUpper().Replace(TEXT("_"), TEXT(" "));
			In.MapReal = GMap;
			FRTSShell::SetIntro(In);
		}'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		GMap = Root->GetStringField(TEXT("map"));''',
     '''		GMap = Root->GetStringField(TEXT("map"));
		GLastScenario.Reset();'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		FRTSShell::Show(TEXT("menu.html"));
	}
	else
	{
		Leave();
	}''',
     '''		FRTSShell::Show(GNextPage.IsEmpty() ? FString(TEXT("menu.html")) : GNextPage);   // (06/11) o BACK TO LOBBY do skirmish volta pra sala
		GNextPage.Reset();
	}
	else
	{
		Leave();
	}'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''bool RTSShellGame::IsCovering()''',
     '''bool RTSShellGame::IsMatchOverlay()
{
	return FRTSShell::GetMatchOverlay() != ERTSMatchOverlay::None;
}

bool RTSShellGame::IsCovering()'''),
    ('Source/RTSGame/Public/RTSShellGame.h',
     '''	/** As telas novas estão cobrindo o menu (o HUD não desenha o menu antigo). */
	bool IsCovering();''',
     '''	/** As telas novas estão cobrindo o menu (o HUD não desenha o menu antigo). */
	bool IsCovering();
	/** (06/11) Uma tela da partida nativa está por cima (o HUD não desenha o LOADING, o aviso de rede, o resultado e o Esc velhos). */
	bool IsMatchOverlay();'''),
    ('Source/RTSGame/Private/RTSHUD.cpp',
     '''		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta''',
     '''		if (RTSShellGame::IsMatchOverlay())
		{
			return;   // (06/11, Telas) a tela de carregar nativa cobre
		}
		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta'''),
    ('Source/RTSGame/Private/RTSHUD.cpp',
     '''void ARTSHUD::DrawNetStatus(URTSMatchSubsystem* Match)
{''',
     '''void ARTSHUD::DrawNetStatus(URTSMatchSubsystem* Match)
{
	if (RTSShellGame::IsMatchOverlay())
	{
		return;   // (06/11, Telas) os avisos da rede nativos (longe do trecho do chat da sessão do HUD, no DrawMatch)
	}'''),
    ('Source/RTSGame/Private/RTSHUD.cpp',
     '''	if (!bPlaying && Match->SecondsSinceEnd() > ResultDelaySeconds)
	{
		DrawResult(Match);
	}
	else if (PC->IsPauseMenuOpen())''',
     '''	if (RTSShellGame::IsMatchOverlay())
	{
		// (06/11, Telas) o resultado e o Esc nativos (o plugin RTSShell, SRTSMatch)
	}
	else if (!bPlaying && Match->SecondsSinceEnd() > ResultDelaySeconds)
	{
		DrawResult(Match);
	}
	else if (PC->IsPauseMenuOpen())'''),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''#include "MoviePlayer.h"''',
     '''#include "MoviePlayer.h"
#include "Misc/CommandLine.h"
#include "RTSShell.h"'''),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''				Screen.WidgetLoadingScreen = MakeLoadingWidget();''',
     '''				// (06/11, Telas) a tela de carregar aprovada (o mockup screens.html, a "load"; o plugin RTSShell, SRTSMatch); a velha fica
				// pro menu antigo
				Screen.WidgetLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("RTSOldMenu")) ? MakeLoadingWidget() : FRTSShell::MakeLoadingScreen();'''),
    # (06/11 à noite) as telas da partida em todo modo, os botões nativos pro teste automático, o filme sempre o novo
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''	void Install()
	{
		if (GInstalled)
		{
			return;
		}
		GInstalled = true;''',
     '''	// (06/11 à noite) as telas da partida valem em todo modo: o teste automático e o -RTSOldMenu também (lá o menu antigo do HUD segue,
	// pros cliques do teste); só os dados, os botões e o relógio delas
	bool GMatchInstalled = false;
	void InstallMatch()
	{
		if (GMatchInstalled)
		{
			return;
		}
		GMatchInstalled = true;
		if (!GInstalled)
		{
			FRTSShellHandlers H;
			H.Api = [](const FString& Path, FString& OutJson) { return Api(Path, OutJson); };
			H.MatchButton = [](const FString& Id) { MatchButton(Id); };
			FRTSShell::SetHandlers(MoveTemp(H));
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { MatchTick(); return true; }), 0.0f);
	}

	void Install()
	{
		if (GInstalled)
		{
			return;
		}
		GInstalled = true;'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		H.MatchButton = [](const FString& Id) { MatchButton(Id); };
		// (06/11) as telas da partida: qual por cima e as amostras do gráfico, a cada quadro
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { MatchTick(); return true; }), 0.0f);''',
     '''		H.MatchButton = [](const FString& Id) { MatchButton(Id); };
		// (06/11) as telas da partida: qual por cima e as amostras do gráfico, a cada quadro (o relógio é do InstallMatch)'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (!Wanted())''',
     '''	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	GPC = PC;
	InstallMatch();   // (06/11 à noite) as telas da partida em todo modo
	if (!Wanted())'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''bool RTSShellGame::IsMatchOverlay()
{
	return FRTSShell::GetMatchOverlay() != ERTSMatchOverlay::None;
}''',
     '''bool RTSShellGame::IsMatchOverlay()
{
	return FRTSShell::GetMatchOverlay() != ERTSMatchOverlay::None;
}

bool RTSShellGame::MatchButtonCenter(FName Id, FVector2D& OutViewportPixel)
{
	return FRTSShell::MatchButtonCenter(Id.ToString(), OutViewportPixel);
}'''),
    ('Source/RTSGame/Public/RTSShellGame.h',
     '''	bool IsMatchOverlay();''',
     '''	bool IsMatchOverlay();
	/** (06/11 à noite) O botão da tela da partida nativa pelo Id do OnButton ("menu", "lobby"...), em pixels do viewport: o teste
	 *  automático clica nele como clicava no do HUD velho. */
	bool MatchButtonCenter(FName Id, FVector2D& OutViewportPixel);'''),
    ('Source/RTSGame/Private/RTSAutoTest.cpp',
     '''	if (HUD && HUD->FindButton(Id, Box))
	{
		return Box.GetCenter();
	}
	return FVector2D(-1, -1);''',
     '''	if (HUD && HUD->FindButton(Id, Box))
	{
		return Box.GetCenter();
	}
	FVector2D Native;
	if (RTSShellGame::MatchButtonCenter(Id, Native))
	{
		return Native;   // (06/11, Telas) o resultado e o Esc são as telas nativas: o botão de mesmo nome
	}
	return FVector2D(-1, -1);'''),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''FParse::Param(FCommandLine::Get(), TEXT("RTSOldMenu")) ? MakeLoadingWidget() : FRTSShell::MakeLoadingScreen();''',
     '''FParse::Param(FCommandLine::Get(), TEXT("RTSOldLoading")) ? MakeLoadingWidget() : FRTSShell::MakeLoadingScreen();'''),
    ('Source/RTSGame/Private/RTSAutoTest.cpp',
     '''#include "RTSAutoTest.h"''',
     '''#include "RTSAutoTest.h"
#include "RTSShellGame.h"   // (06/11, Telas) os botões das telas da partida nativas'''),
    ('Source/RTSGame/Private/RTSHUD.cpp',
     '''		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.012f, 0.014f, 0.017f, 1.0f));
		DrawLabel(LOCTEXT("Loading", "LOADING").ToString(), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f - 30.0f, 26, FLinearColor(0.92f, 0.93f, 0.95f), true, true);
		const int32 Dots = int32(FPlatformTime::Seconds() * 3.0) % 4;
		DrawLabel(FString::ChrN(Dots, TEXT('.')), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f + 12.0f, 20, FLinearColor(0.92f, 0.93f, 0.95f, 0.7f), true);
		return;''',
     '''		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta. (06/11, Telas) O "LOADING" velho saiu: a tela é a
		// nativa (o SRTSMatch); se ela ainda não estiver por cima, só o escuro dela (rgb 10, 13, 11), sem texto
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.003f, 0.004f, 0.0033f, 1.0f));
		return;'''),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''namespace
{
	TSharedRef<SWidget> MakeLoadingWidget()
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.012f, 0.014f, 0.017f, 1.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("RTSHUD", "Loading", "LOADING"))   // o jogo é em inglês por padrão (09/10)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
					.ColorAndOpacity(FLinearColor(0.92f, 0.93f, 0.95f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SThrobber)
				]
			];
	}
}''',
     '''// (06/11, Telas) o widget velho do carregar ("LOADING" com a rodinha) saiu: a tela é a aprovada, do plugin RTSShell (SRTSMatch)'''),
    ('Source/RTSGame/Private/RTSGameModule.cpp',
     '''				// (06/11, Telas) a tela de carregar aprovada (o mockup screens.html, a "load"; o plugin RTSShell, SRTSMatch); a velha fica
				// pro menu antigo
				Screen.WidgetLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("RTSOldLoading")) ? MakeLoadingWidget() : FRTSShell::MakeLoadingScreen();''',
     '''				// (06/11, Telas) a tela de carregar aprovada (o mockup screens.html, a "load"; o plugin RTSShell, SRTSMatch), em todo modo
				Screen.WidgetLoadingScreen = FRTSShell::MakeLoadingScreen();'''),
    # (06/11 à noite) o menu antigo do HUD sai de vez; o teste automático usa o nativo
    ('Source/RTSGame/Public/RTSAutoTest.h',
     '''	FVector2D ButtonCenter(FName Id) const;''',
     '''	FVector2D ButtonCenter(FName Id) const;
	/** (06/11, Telas) O botão do menu nativo que o ButtonCenter achou (o menu antigo do HUD saiu): o QueueClick chama a função dele. */
	mutable FName PendingButton;'''),
    ('Source/RTSGame/Private/RTSAutoTest.cpp',
     '''		return Native;   // (06/11, Telas) o resultado e o Esc são as telas nativas: o botão de mesmo nome
	}
	return FVector2D(-1, -1);''',
     '''		return Native;   // (06/11, Telas) o resultado e o Esc são as telas nativas: o botão de mesmo nome
	}
	if (P && P->IsMenu() && RTSShellGame::IsCovering())
	{
		PendingButton = Id;   // (06/11, Telas) o menu nativo cobre (o antigo do HUD saiu): o QueueClick chama a função do botão
		return FVector2D(-7777.0, -7777.0);
	}
	return FVector2D(-1, -1);'''),
    ('Source/RTSGame/Private/RTSAutoTest.cpp',
     '''void URTSAutoTest::QueueClick(const FVector2D& Pixel, FKey Button)
{''',
     '''void URTSAutoTest::QueueClick(const FVector2D& Pixel, FKey Button)
{
	if (Pixel.X == -7777.0 && Pixel.Y == -7777.0)
	{
		// (06/11, Telas) o botão do menu nativo: a função dele direto no controle (o ButtonCenter guardou qual), o que o clique fazia
		if (ARTSPlayerController* P = PC())
		{
			UE_LOG(LogTemp, Display, TEXT("RTS TEST %s pela função do botão (o menu nativo)"), *PendingButton.ToString());
			P->OnButton(PendingButton);
		}
		return;
	}'''),
    ('Source/RTSGame/Private/RTSShellGame.cpp',
     '''		FString Script;
		return !FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), Script) && !FParse::Param(FCommandLine::Get(), TEXT("RTSOldMenu"))
			&& FRTSShell::IsAvailable();''',
     '''		// (06/11 à noite) o menu antigo do HUD saiu de vez: o teste automático também usa o nativo (o RTSAutoTest chama a função de cada
		// botão quando o velho não existe); o -RTSOldMenu deixou de existir
		return FRTSShell::IsAvailable();'''),
]


STATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'patch_shared.applied')


def main(mode):
    # (06/11) o revert desfaz só o que o último apply trocou (as trocas de antes, já aprovadas, ficam), na ordem inversa (as trocas
    # encadeadas no mesmo arquivo voltam certo)
    if mode == 'apply':
        todo = list(enumerate(PATCHES))
        done = []
    else:
        keep = set(int(x) for x in open(STATE).read().split()) if os.path.exists(STATE) else set()
        todo = [(i, p) for i, p in reversed(list(enumerate(PATCHES))) if i in keep]
    for idx, (rel, old, new) in todo:
        path = os.path.join(G, rel)
        raw = open(path, 'rb').read()
        crlf = b'\r\n' in raw
        s = raw.decode('utf-8-sig').replace('\r\n', '\n')
        a, b = (old, new) if mode == 'apply' else (new, old)
        if mode == 'apply' and s.count(new):
            print(f'{rel}: nada a trocar (já aplicado)')   # (06/11) o trecho novo pode conter o velho: nunca aplicar duas vezes
            continue
        n = s.count(a)
        if n == 0:
            print(f'{rel}: nada a trocar ({"já aplicado" if s.count(b) else "trecho não encontrado"})')
            continue
        s = s.replace(a, b)
        bom = raw.startswith(b'\xef\xbb\xbf')
        out = s.replace('\n', '\r\n') if crlf else s
        open(path, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + out.encode('utf-8'))
        print(f'{rel}: {mode} ({n} trecho{"s" if n > 1 else ""})')
        if mode == 'apply':
            done.append(idx)
    if mode == 'apply':
        open(STATE, 'w').write(' '.join(str(i) for i in done))
    elif os.path.exists(STATE):
        os.remove(STATE)


main(sys.argv[1] if len(sys.argv) > 1 else 'apply')
