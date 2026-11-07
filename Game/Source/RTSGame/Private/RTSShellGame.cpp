#include "RTSShellGame.h"
#include "RTSShell.h"
#include "RTSPlayerController.h"
#include "RTSSessionSubsystem.h"
#include "RTSNetSession.h"
#include "RTSLab.h"
#include "RTSGameData.h"
#include "RTSMatchSubsystem.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TWeakObjectPtr<ARTSPlayerController> GPC;
	bool GInstalled = false;
	bool GCovering = false;
	bool GLaunching = false;               // um mapa está abrindo: as telas não voltam no intervalo
	FString GArmed;                       // a cena da linha de comando, lida uma vez (o SCRIPT mostra armada)
	FString GMap;                         // o mapa da sala nova ou da cena do SCRIPT
	bool GHasSlots = false;
	TArray<FRTSLocalSlot> GSlots;
	FTSTicker::FDelegateHandle GTick;
	int32 GLastPage = -1;                 // a página do menu do jogo que as telas mostram (a principal, a LAN, a sala)
	FString GLastScenario;                // (06/11) a cena do SCRIPT da partida (o RESTART joga de novo)
	FString GNextPage;                    // (06/11) a página do menu na volta (o BACK TO LOBBY do skirmish volta pra sala)

	bool Wanted()
	{
		// (06/11 à noite) o menu antigo do HUD saiu de vez: o teste automático também usa o nativo (o RTSAutoTest chama a função de cada
		// botão quando o velho não existe); o -RTSOldMenu deixou de existir
		return FRTSShell::IsAvailable();
	}

	URTSSessionSubsystem* Session()
	{
		return GPC.IsValid() && GPC->GetGameInstance() ? GPC->GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
	}

	URTSNetSession* Net()
	{
		return GPC.IsValid() && GPC->GetGameInstance() ? GPC->GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
	}

	/** Troca a cena da linha de comando (o jogo lê o -RTSScenario na hora de começar a partida); vazio = sem cena. */
	void SetScenarioArg(const FString& Name)
	{
		FString Cmd = FCommandLine::Get();
		for (int32 At = Cmd.Find(TEXT("-RTSScenario="), ESearchCase::IgnoreCase); At != INDEX_NONE; At = Cmd.Find(TEXT("-RTSScenario="), ESearchCase::IgnoreCase))
		{
			int32 End = At;
			while (End < Cmd.Len() && !FChar::IsWhitespace(Cmd[End]))
			{
				++End;
			}
			Cmd.RemoveAt(At, End - At);
		}
		if (!Name.IsEmpty())
		{
			Cmd += TEXT(" -RTSScenario=") + Name;
		}
		FCommandLine::Set(*Cmd);
	}

	/** Sai das telas antes de abrir o mapa: a tela de carregar do jogo aparece no lugar. */
	void Leave(bool bLaunch = false)
	{
		GCovering = false;
		GLaunching |= bLaunch;
		FRTSShell::Hide();
	}

	// o preset das telas (0 CLASSIC, 1 LOW, 2 MEDIUM, 3 HIGH, 4 ULTRA, 5 AUTO) no do jogo (0 baixa, 1 média, 2 alta, 3 ultra);
	// o visual CLASSIC ainda não existe no jogo: vai na média
	int32 GameQualityOf(int32 Ui) { static const int32 Map[6] = { 1, 0, 1, 2, 3, 2 }; return Map[FMath::Clamp(Ui, 0, 5)]; }
	int32 UiQualityOf(int32 Game) { return FMath::Clamp(Game, 0, 3) + 1; }

	FLinearColor ParseColor(const FString& Hex)
	{
		// como a paleta do jogo (RTSMatchSubsystem::PaletteColor): os números da tela direto, sem a curva do sRGB
		const FColor C = FColor::FromHex(Hex);
		return FLinearColor(C.R / 255.0f, C.G / 255.0f, C.B / 255.0f);
	}

	void StartSkirmish(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS telas: montagem do skirmish ilegível: %s"), *Json.Left(200));
			return;
		}
		GSlots.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		if (Root->TryGetArrayField(TEXT("slots"), Slots))
		{
			for (const TSharedPtr<FJsonValue>& V : *Slots)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid())
				{
					continue;
				}
				FRTSLocalSlot S;
				S.bAI = O->GetStringField(TEXT("kind")) == TEXT("ai");
				S.Team = uint8(FMath::Clamp(int32(O->GetNumberField(TEXT("team"))), 0, 8));
				S.Start = int32(O->GetNumberField(TEXT("base")));
				S.Faction = O->GetStringField(TEXT("fac"));
				FString Hex;
				S.bHasColor = O->TryGetStringField(TEXT("color"), Hex) && Hex.StartsWith(TEXT("#"));
				if (S.bHasColor)
				{
					S.Color = ParseColor(Hex);
				}
				S.Difficulty = uint8(FMath::Clamp(int32(O->GetNumberField(TEXT("ai"))), 0, 3));
				// você sempre na vaga 0 (o jogador local da partida)
				if (!S.bAI)
				{
					GSlots.Insert(S, 0);
				}
				else
				{
					GSlots.Add(S);
				}
			}
		}
		GSlots.SetNum(FMath::Min(GSlots.Num(), 8));
		GHasSlots = GSlots.Num() >= 2;
		GMap = Root->GetStringField(TEXT("map"));
		GLastScenario.Reset();
		SetScenarioArg(FString());   // a partida da sala, não a cena da linha de comando
		URTSSessionSubsystem* S = Session();
		if (!S)
		{
			return;
		}
		double Money = 0.0;
		if (Root->TryGetNumberField(TEXT("money"), Money) && Money > 0.0)
		{
			S->SetStartingMoney(int32(Money), true);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: skirmish em %s com %d vagas, $%d"), *GMap, GSlots.Num(), S->StartingMoney);
		Leave(true);
		S->StartSkirmish();
	}

	void RunScenario(const FString& Name)
	{
		FRTSScenario Scenario;
		FString Error;
		if (!FRTSGameData::LoadScenario(Name, Scenario, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS telas: a cena %s não abriu: %s"), *Name, *Error);
			return;
		}
		GHasSlots = false;
		GSlots.Reset();
		GMap = Scenario.Map;
		SetScenarioArg(Name);
		GLastScenario = Name;   // (06/11) o RESTART joga a mesma cena; a tela de carregar mostra o nome dela
		{
			FRTSMatchIntro In;
			In.Op = Name.ToUpper().Replace(TEXT("_"), TEXT(" "));
			In.MapReal = GMap;
			FRTSShell::SetIntro(In);
		}
		UE_LOG(LogTemp, Log, TEXT("RTS telas: SCRIPT %s em %s"), *Name, *GMap);
		if (URTSSessionSubsystem* S = Session())
		{
			Leave(true);
			S->StartSkirmish();
		}
	}

	bool Lan(const FString& Command)
	{
		URTSNetSession* N = Net();
		if (!N || !GPC.IsValid())
		{
			return false;
		}
		if (Command == TEXT("browse"))
		{
			N->StartBrowsing();
			return true;
		}
		if (Command == TEXT("stop"))
		{
			N->StopBrowsing();
			return true;
		}
		// os botões da sala (03/11, a sala nova da LAN): os mesmos do menu antigo, pela função do botão; a página do menu do jogo
		// (a principal, a LAN, a sala) é que decide a tela (o relógio de Install)
		static const TMap<FString, FString> Buttons = { { TEXT("host"), TEXT("lan_host") }, { TEXT("leave"), TEXT("lan_leave") },
			{ TEXT("back"), TEXT("lan_back") }, { TEXT("start"), TEXT("lan_start") }, { TEXT("addai"), TEXT("lan_add_ai") }, { TEXT("random"), TEXT("lan_random") } };
		if (const FString* Id = Buttons.Find(Command))
		{
			if (Command == TEXT("host"))
			{
				GMap.Reset();   // a rede abre o mapa de sempre em todos os PCs
				GHasSlots = false;
			}
			GPC->OnButton(FName(**Id));
			return true;
		}
		// (06/11, a sala nova) o valor exato de cada campo, como no skirmish: "team:2=1", "fac:2=china", "base:2=-1", "color:2=#c8323c"
		// (−1 = automática), "slot:2=open|closed|ai:1" (o nível), "kick:2" (quem entrou ou a IA), "map:World_TestHills", "chat:texto"
		if (Command.StartsWith(TEXT("chat:")))
		{
			N->SendChat(Command.Mid(5), false);
			return true;
		}
		if (Command.StartsWith(TEXT("map:")))
		{
			return N->SetMap(Command.Mid(4));
		}
		if (Command == TEXT("ready:1") || Command == TEXT("ready:0"))
		{
			N->SetLobbyReady(Command.EndsWith(TEXT("1")));
			return true;
		}
		if (Command.StartsWith(TEXT("countdown:")))
		{
			N->SetCountdown(FCString::Atoi(*Command.Mid(10)));
			return true;
		}
		if (Command.StartsWith(TEXT("kick:")))
		{
			N->Kick(uint8(FCString::Atoi(*Command.Mid(5))));
			return true;
		}
		{
			FString Field, Rest, SlotS, Value;
			if (Command.Split(TEXT(":"), &Field, &Rest) && Rest.Split(TEXT("="), &SlotS, &Value) && SlotS.IsNumeric())
			{
				const uint8 Slot = uint8(FMath::Clamp(FCString::Atoi(*SlotS), 0, URTSNetSession::MaxPlayers - 1));
				if (Field == TEXT("team"))
				{
					N->SetTeamOf(Slot, uint8(FMath::Clamp(FCString::Atoi(*Value), 0, 8)));
				}
				else if (Field == TEXT("fac"))
				{
					N->SetFactionOf(Slot, Value.ToLower());
				}
				else if (Field == TEXT("base"))
				{
					N->SetBaseOf(Slot, FCString::Atoi(*Value));
				}
				else if (Field == TEXT("color"))
				{
					N->SetColorOf(Slot, Value.StartsWith(TEXT("#")) ? (FParse::HexNumber(*Value.Mid(1)) & 0xFFFFFFu) : 0u);
				}
				else if (Field == TEXT("slot"))
				{
					if (Value == TEXT("open"))
					{
						N->SetSlotState(Slot, 0);
					}
					else if (Value == TEXT("closed"))
					{
						N->SetSlotState(Slot, 1);
					}
					else if (Value.StartsWith(TEXT("ai:")))
					{
						N->SetSlotState(Slot, 2, uint8(FMath::Clamp(FCString::Atoi(*Value.Mid(3)), 0, 2)));
					}
				}
				else
				{
					return false;
				}
				return true;
			}
		}
		static const TMap<FString, FString> SlotButtons = { { TEXT("join:"), TEXT("lan_join_") }, { TEXT("team:"), TEXT("lan_team_") },
			{ TEXT("color:"), TEXT("lan_color_") }, { TEXT("kick:"), TEXT("lan_kick_") } };
		for (const TPair<FString, FString>& B : SlotButtons)
		{
			if (Command.StartsWith(B.Key))
			{
				if (B.Key == TEXT("join:"))
				{
					GMap.Reset();
					GHasSlots = false;
				}
				GPC->OnButton(FName(*(B.Value + FString::FromInt(FCString::Atoi(*Command.Mid(B.Key.Len()))))));
				return true;
			}
		}
		if (Command.StartsWith(TEXT("money:")) && N->IsHost())
		{
			N->SetStartingMoney(FCString::Atoi(*Command.Mid(6)));
			return true;
		}
		return false;
	}

	bool MatchJson(FString& OutJson, bool bHistory);

	bool Api(const FString& Path, FString& OutJson)
	{
		if (Path.StartsWith(TEXT("match.json")))
		{
			return MatchJson(OutJson, Path.Contains(TEXT("h=1")));   // (06/11) as telas da partida (o histórico só pro resultado)
		}
		if (Path == TEXT("scenarios.json"))
		{
			// o arquivo do jogo como está (a página conta as unidades e as ordens)
			return FFileHelper::LoadFileToString(OutJson, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Scenarios.json")));
		}
		if (Path == TEXT("lobby.json"))
		{
			// a sala da rede como está (a página redesenha a cada meio segundo)
			TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Members;
			if (const URTSNetSession* N = Net())
			{
				for (const FRTSNetMember& M : N->Members())
				{
					const FLinearColor Pal = URTSMatchSubsystem::PaletteColor(M.Color);
					// (06/11) a cor da sala quando escolhida (das 24), senão a da paleta
					const FLinearColor C = M.Rgb ? FLinearColor(((M.Rgb >> 16) & 255) / 255.0f, ((M.Rgb >> 8) & 255) / 255.0f, (M.Rgb & 255) / 255.0f) : Pal;
					TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
					O->SetNumberField(TEXT("slot"), M.Slot);
					O->SetStringField(TEXT("name"), M.Name);
					O->SetBoolField(TEXT("ai"), M.bAI);
					O->SetNumberField(TEXT("team"), M.Team);
					O->SetNumberField(TEXT("color"), M.Color);
					O->SetStringField(TEXT("hex"), FString::Printf(TEXT("#%02x%02x%02x"), FMath::RoundToInt(FMath::Clamp(C.R, 0.0f, 1.0f) * 255.0f),
						FMath::RoundToInt(FMath::Clamp(C.G, 0.0f, 1.0f) * 255.0f), FMath::RoundToInt(FMath::Clamp(C.B, 0.0f, 1.0f) * 255.0f)));
					O->SetBoolField(TEXT("ready"), M.bLobbyReady);   // (06/11) o READY da sala
					O->SetBoolField(TEXT("inLobby"), M.bInLobby);
					O->SetBoolField(TEXT("colorAuto"), M.Rgb == 0);
					O->SetStringField(TEXT("fac"), M.Faction);
					O->SetNumberField(TEXT("base"), M.Base);
					O->SetNumberField(TEXT("diff"), M.Difficulty);
					// o ping: o anfitrião mede o de cada um; quem entrou só sabe o dele até o anfitrião (o mesmo pro anfitrião e pra ele)
					O->SetNumberField(TEXT("ping"), !N->IsHost() && (M.Slot == 0 || M.Slot == N->LocalSlot()) ? N->WorstRttMs() : M.PingMs);
					Members.Add(MakeShared<FJsonValueObject>(O));
				}
				TArray<TSharedPtr<FJsonValue>> Closed;
				for (int32 K = 0; K < URTSNetSession::MaxPlayers; ++K)
				{
					if (N->GetClosedMask() & (1u << K))
					{
						Closed.Add(MakeShared<FJsonValueNumber>(K));
					}
				}
				Root->SetArrayField(TEXT("closed"), Closed);
				Root->SetNumberField(TEXT("countdown"), N->GetCountdown());
				// o chat da sala: as últimas linhas (a vaga, o nome de quem é hoje, o texto, se foi pros aliados, quantos segundos faz)
				TArray<TSharedPtr<FJsonValue>> Lines;
				const TArray<URTSNetSession::FChatLine>& Chat = N->ChatLines();
				const double Now = FPlatformTime::Seconds();
				for (int32 K = FMath::Max(0, Chat.Num() - 30); K < Chat.Num(); ++K)
				{
					const URTSNetSession::FChatLine& L = Chat[K];
					const FRTSNetMember* Who = N->Members().FindByPredicate([&L](const FRTSNetMember& X) { return X.Slot == L.Slot; });
					TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
					O->SetNumberField(TEXT("slot"), L.Slot);
					O->SetStringField(TEXT("name"), Who ? Who->Name : FString());
					O->SetStringField(TEXT("text"), L.Text);
					O->SetBoolField(TEXT("team"), L.bTeam);
					O->SetNumberField(TEXT("ago"), Now - L.Time);
					Lines.Add(MakeShared<FJsonValueObject>(O));
				}
				Root->SetArrayField(TEXT("chat"), Lines);
				Root->SetNumberField(TEXT("chatTotal"), N->ChatTotalCount());
				Root->SetBoolField(TEXT("host"), N->IsHost());
				Root->SetNumberField(TEXT("me"), N->LocalSlot());
				Root->SetBoolField(TEXT("canStart"), N->IsHost() && N->CanStart());
				Root->SetStringField(TEXT("status"), N->StatusText());
				Root->SetNumberField(TEXT("money"), N->GetStartingMoney());
				Root->SetNumberField(TEXT("slotLimit"), N->SlotLimit());
			}
			{
				const URTSNetSession* N = Net();
				Root->SetStringField(TEXT("map"), N && !N->GetMapName().IsEmpty() ? N->GetMapName() : URTSSessionSubsystem::SkirmishMapName());
			}
			Root->SetArrayField(TEXT("members"), Members);
			const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutJson);
			return FJsonSerializer::Serialize(Root.ToSharedRef(), W);
		}
		if (Path == TEXT("lan.json"))
		{
			TArray<TSharedPtr<FJsonValue>> Games;
			if (const URTSNetSession* N = Net())
			{
				for (int32 K = 0; K < N->FoundGames().Num(); ++K)
				{
					const FRTSLanGame& G = N->FoundGames()[K];
					TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
					O->SetNumberField(TEXT("index"), K);
					O->SetStringField(TEXT("name"), G.Name);
					O->SetStringField(TEXT("ip"), G.Ip);
					O->SetNumberField(TEXT("port"), G.Port);
					O->SetNumberField(TEXT("players"), G.Players);
					O->SetBoolField(TEXT("open"), G.bOpen);
					Games.Add(MakeShared<FJsonValueObject>(O));
				}
			}
			TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetArrayField(TEXT("games"), Games);
			const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutJson);
			return FJsonSerializer::Serialize(Root.ToSharedRef(), W);
		}
		return false;
	}

	// ------------------------------------------------------------ (06/11) as telas da partida nativas (o plugin, SRTSMatch)
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
		if (!M->IsRunning() && !PC->GetError().IsEmpty())
		{
			O = ERTSMatchOverlay::Net;   // (06/11 à noite) a partida não montou: o aviso nativo com o motivo e o MAIN MENU
		}
		else if (!M->IsReady() || M->IsWaitingForPlayers())
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
			if (!M->IsRunning() && !PC->GetError().IsEmpty())
			{
				// (06/11 à noite) 7: a partida não montou (qualquer partida, não só a da rede); o motivo vai junto
				TSharedPtr<FJsonObject> Nj = MakeShared<FJsonObject>();
				Nj->SetNumberField(TEXT("case"), 7);
				Nj->SetStringField(TEXT("error"), PC->GetError());
				Root->SetObjectField(TEXT("net"), Nj);
			}
			else if (bNet)
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

	// (06/11 à noite) as telas da partida valem em todo modo: o teste automático e o -RTSOldMenu também (lá o menu antigo do HUD segue,
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
		GInstalled = true;
		FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), GArmed);
		FRTSShellHandlers H;
		H.IsDev = []() { return FRTSLab::IsAllowed(); };
		H.ArmedScenario = []() { return GArmed; };
		H.RunScenario = [](const FString& Name) { RunScenario(Name); };
		H.OpenLab = []() { if (GPC.IsValid()) { Leave(true); GPC->OnButton(TEXT("lab")); } };
		H.StartSkirmish = [](const FString& Json) { StartSkirmish(Json); };
		H.SetQuality = [](int32 Ui) { if (GPC.IsValid()) GPC->OnButton(FName(*FString::Printf(TEXT("quality_%d"), GameQualityOf(Ui)))); };
		H.GetQuality = []() { return GPC.IsValid() ? UiQualityOf(GPC->GetQualityPreset()) : 3; };
		H.Quit = []() { if (GPC.IsValid()) { Leave(true); GPC->OnButton(TEXT("quit")); } };
		H.Lan = [](const FString& Command) { return Lan(Command); };
		H.Api = [](const FString& Path, FString& OutJson) { return Api(Path, OutJson); };
		H.MatchButton = [](const FString& Id) { MatchButton(Id); };
		// (06/11) as telas da partida: qual por cima e as amostras do gráfico, a cada quadro (o relógio é do InstallMatch)
		FRTSShell::SetHandlers(MoveTemp(H));
		// a página do menu do jogo decide a tela: a principal = o menu, a LAN = a lista (depois de sair de uma sala), a sala = a sala nova
		// (03/11); só na troca de página, pra não atropelar a navegação dentro das telas
		GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			ARTSPlayerController* PC = GPC.Get();
			if (!PC || !PC->IsMenu() || GLaunching)
			{
				return true;
			}
			const int32 Page = int32(PC->GetMenuPage());
			if (Page == GLastPage && GCovering)
			{
				return true;
			}
			GLastPage = Page;
			GCovering = true;
			if (PC->GetMenuPage() == ARTSPlayerController::EMenuPage::Lobby)
			{
				const URTSNetSession* N = Net();
				FString Host = N && N->Members().Num() > 0 ? N->Members()[0].Name : FString(TEXT("HOST"));
				Host = Host.Replace(TEXT("&"), TEXT("")).Replace(TEXT("#"), TEXT("")).Replace(TEXT(" "), TEXT("+"));
				FRTSShell::Show(FString::Printf(TEXT("skirmish.html?mp=1&lan=1&src=lan&iamhost=%d&host=%s"), N && N->IsHost() ? 1 : 0, *Host));
			}
			else
			{
				FRTSShell::Show(PC->GetMenuPage() == ARTSPlayerController::EMenuPage::Lan ? TEXT("multiplayer.html") : TEXT("menu.html"));
			}
			return true;
		}), 0.25f);
	}
}

void RTSShellGame::OnPlayerBegin(ARTSPlayerController* PC, bool bMenu)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	GPC = PC;
	InstallMatch();   // (06/11 à noite) as telas da partida em todo modo
	if (!Wanted())
	{
		static bool bSaid = false;
		if (bMenu && !bSaid)
		{
			bSaid = true;
			UE_LOG(LogTemp, Log, TEXT("RTS telas: o menu antigo (teste automático, -RTSOldMenu, sem placa ou sem o navegador embutido; páginas em %s)"), *FRTSShell::UiDir());
		}
		return;
	}
	Install();
	GPC = PC;
	GLaunching = false;
	if (bMenu)
	{
		// de volta ao menu (o fim da partida, o Esc): a próxima partida local volta a ser montada pela sala
		GHasSlots = false;
		GCovering = true;
		GLastPage = int32(ARTSPlayerController::EMenuPage::Main);
		FRTSShell::Show(GNextPage.IsEmpty() ? FString(TEXT("menu.html")) : GNextPage);   // (06/11) o BACK TO LOBBY do skirmish volta pra sala
		GNextPage.Reset();
	}
	else
	{
		Leave();
	}
}

void RTSShellGame::SetMapOverride(const FString& Map)
{
	GMap = Map;
}

bool RTSShellGame::IsMatchOverlay()
{
	return FRTSShell::GetMatchOverlay() != ERTSMatchOverlay::None;
}

bool RTSShellGame::MatchButtonCenter(FName Id, FVector2D& OutViewportPixel)
{
	return FRTSShell::MatchButtonCenter(Id.ToString(), OutViewportPixel);
}

bool RTSShellGame::IsCovering()
{
	return GCovering && FRTSShell::IsShown();
}

const TArray<FRTSLocalSlot>* RTSShellGame::LocalSlots()
{
	return GHasSlots ? &GSlots : nullptr;
}

FString RTSShellGame::MapOverride()
{
	return GMap;
}
