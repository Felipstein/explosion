#include "RTSShellGame.h"
#include "RTSShell.h"
#include "RTSPlayerController.h"
#include "RTSSessionSubsystem.h"
#include "RTSNetSession.h"
#include "RTSLab.h"
#include "RTSGameData.h"
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

	bool Wanted()
	{
		FString Script;
		return !FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), Script) && !FParse::Param(FCommandLine::Get(), TEXT("RTSOldMenu"))
			&& FRTSShell::IsAvailable();
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
		// criar ou entrar: a sala ainda é a antiga (o HUD desenha quando a página do menu sai da principal)
		if (Command == TEXT("host"))
		{
			GPC->OnButton(TEXT("lan_host"));
			return GPC->GetMenuPage() != ARTSPlayerController::EMenuPage::Main;
		}
		if (Command.StartsWith(TEXT("join:")))
		{
			GPC->OnButton(FName(*(TEXT("lan_join_") + Command.Mid(5))));
			return GPC->GetMenuPage() != ARTSPlayerController::EMenuPage::Main;
		}
		return false;
	}

	bool Api(const FString& Path, FString& OutJson)
	{
		if (Path == TEXT("scenarios.json"))
		{
			// o arquivo do jogo como está (a página conta as unidades e as ordens)
			return FFileHelper::LoadFileToString(OutJson, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Scenarios.json")));
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
		FRTSShell::SetHandlers(MoveTemp(H));
		// a sala da LAN ainda é a antiga: quando a página do menu sai da principal, as telas saem; quando volta, voltam
		GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			ARTSPlayerController* PC = GPC.Get();
			if (!PC || !PC->IsMenu() || GLaunching)
			{
				return true;
			}
			const bool bMain = PC->GetMenuPage() == ARTSPlayerController::EMenuPage::Main;
			if (bMain && !GCovering)
			{
				GCovering = true;
				FRTSShell::Show(TEXT("menu.html"));
			}
			else if (!bMain && GCovering)
			{
				Leave();
			}
			return true;
		}), 0.25f);
	}
}

void RTSShellGame::OnPlayerBegin(ARTSPlayerController* PC, bool bMenu)
{
	if (!PC || !PC->IsLocalController() || !Wanted())
	{
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
		FRTSShell::Show(TEXT("menu.html"));
	}
	else
	{
		Leave();
	}
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
