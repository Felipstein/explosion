#include "RTSSessionSubsystem.h"

#include "RTSGameData.h"
#include "RTSNetSession.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void URTSSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// (v4, 22/10) o dinheiro inicial: as escolhas do Units.json, o padrão, e o que o jogador guardou da última vez
	FRTSGameData Data;
	FString Error;
	const bool bData = Data.Load(Error);
	const int32 Default = bData && Data.DefaultStartingMoney > 0 ? Data.DefaultStartingMoney : 10000;
	MoneyChoices = bData && Data.StartingMoneyChoices.Num() > 0 ? Data.StartingMoneyChoices : TArray<int32>{ Default };
	MoneyScale = bData ? FMath::Max(1, Data.MoneyScale) : 1;
	StartingMoney = Default;
	// o teste automático e os robôs da LAN ficam no padrão (o 50 000 guardado do Felipe mudaria o teste)
	FString Script;
	bTestRun = FParse::Value(FCommandLine::Get(), TEXT("-RTSAutoTest="), Script) || FParse::Param(FCommandLine::Get(), TEXT("RTSNetBot"));
	int32 Saved = 0;
	if (!bTestRun && GConfig && GConfig->GetInt(TEXT("RTS"), TEXT("StartingMoney"), Saved, GGameUserSettingsIni))
	{
		SetStartingMoney(Saved, false);
	}
	int32 Forced = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-RTSStartMoney="), Forced) && Forced > 0)
	{
		SetStartingMoney(Forced, false);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS dinheiro inicial: %d (x%d), %d escolhas%s"), StartingMoney, MoneyScale, MoneyChoices.Num(), bTestRun ? TEXT(" (teste: sem o guardado)") : TEXT(""));
}

int32 URTSSessionSubsystem::StepMoney(int32 From, int32 Dir) const
{
	if (MoneyChoices.Num() == 0)
	{
		return From;
	}
	// a posição de agora (a primeira escolha que alcança From) e um passo, preso nas pontas
	int32 At = 0;
	while (At < MoneyChoices.Num() - 1 && MoneyChoices[At] < From)
	{
		++At;
	}
	if (MoneyChoices[At] != From && Dir != 0)
	{
		// From fora da lista: o + vai pra primeira acima, o - pra primeira abaixo (presos nas pontas)
		if (Dir > 0)
		{
			return MoneyChoices[At] > From ? MoneyChoices[At] : MoneyChoices.Last();
		}
		return MoneyChoices[At] < From ? MoneyChoices[At] : MoneyChoices[FMath::Max(0, At - 1)];
	}
	return MoneyChoices[FMath::Clamp(At + (Dir > 0 ? 1 : (Dir < 0 ? -1 : 0)), 0, MoneyChoices.Num() - 1)];
}

void URTSSessionSubsystem::SetStartingMoney(int32 Money, bool bSave)
{
	// o mais perto da lista (o .ini editado à mão, o -RTSStartMoney fora da lista)
	int32 Best = MoneyChoices.Num() > 0 ? MoneyChoices[0] : Money;
	for (const int32 C : MoneyChoices)
	{
		Best = FMath::Abs(C - Money) < FMath::Abs(Best - Money) ? C : Best;
	}
	StartingMoney = Best;
	if (bSave && !bTestRun && GConfig)
	{
		GConfig->SetInt(TEXT("RTS"), TEXT("StartingMoney"), StartingMoney, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

FString URTSSessionSubsystem::SkirmishMapName()
{
	// a cena lida uma vez (o nome dela não muda no processo); sem cena ou sem "map", o mapa do skirmish
	static const FString Map = []()
	{
		FString ScenarioName;
		FRTSScenario Scenario;
		FString Error;
		if (FParse::Value(FCommandLine::Get(), TEXT("-RTSScenario="), ScenarioName) && FRTSGameData::LoadScenario(ScenarioName, Scenario, Error)
			&& !Scenario.Map.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("RTS mapa da cena %s: %s"), *ScenarioName, *Scenario.Map);
			return Scenario.Map;
		}
		return FString(TEXT("World_TestSalt2"));
	}();
	return Map;
}

void URTSSessionSubsystem::StartSkirmish()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSSkirmishGameMode"));
}

void URTSSessionSubsystem::ReturnToMenu()
{
	if (URTSNetSession* Net = GetGameInstance()->GetSubsystem<URTSNetSession>())
	{
		Net->Leave();   // a partida em rede acabou: sai da sala
	}
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));
}

void URTSSessionSubsystem::ReturnToLobby()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SkirmishMapName()), true, TEXT("game=/Script/RTSGame.RTSMenuGameMode"));
}

void URTSSessionSubsystem::Quit(APlayerController* PC)
{
	UKismetSystemLibrary::QuitGame(GetGameInstance(), PC, EQuitPreference::Quit, false);
}
