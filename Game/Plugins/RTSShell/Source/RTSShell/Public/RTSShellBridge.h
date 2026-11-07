#pragma once

// A ponte das páginas pro jogo: o navegador expõe as funções deste objeto como window.ue.shell.<nome em minúsculas>(...), e
// cada chamada devolve uma Promise com o retorno (RTSShell.h).

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RTSShellBridge.generated.h"

UCLASS()
class URTSShellBridge : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION() bool IsDev();
	UFUNCTION() FString ArmedScenario();
	UFUNCTION() void RunScenario(const FString& Name);
	UFUNCTION() void OpenLab();
	UFUNCTION() void StartSkirmish(const FString& SetupJson);
	UFUNCTION() void SetQuality(int32 Level);
	UFUNCTION() int32 GetQuality();
	UFUNCTION() void Quit();
	UFUNCTION() bool Lan(const FString& Command);
	/** As opções que o jogo lê do GameUserSettings, seção [RTS] (o HUD relê a cada segundo: HudCleanSlots, HudHideEmptySlots).
	 *  O valor em texto ("True"/"False" pros sim/não); vazio = nunca gravada (a página fica no padrão dela). */
	UFUNCTION() FString GetOption(const FString& Key);
	UFUNCTION() void SetOption(const FString& Key, const FString& Value);
	/** O console da página no log do jogo (o console.log não chega sozinho). */
	UFUNCTION() void Log(const FString& Text);
};
