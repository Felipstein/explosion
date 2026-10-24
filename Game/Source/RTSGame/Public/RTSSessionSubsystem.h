#pragma once

// O fluxo entre as telas (vive o jogo inteiro, entre um mapa e outro):
// menu → skirmish → menu → sair. Cada troca é um OpenLevel local com ?game=;
// a rede lockstep não usa a viagem de mapa da Unreal (docs/JOGO.md §3).

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RTSSessionSubsystem.generated.h"

class APlayerController;

/** A névoa da partida (o botão do menu e da sala):
 *  - Off: sem névoa;
 *  - Black: o que nunca foi visto fica preto (o "shroud");
 *  - Explored: o mapa aparece desde o começo, escurecido fora da visão; só as unidades fora da
 *    visão somem (o "fog of war" do C&C em skirmish e multiplayer; no AoE, "Explored"). */
enum class ERTSFogMode : uint8 { Off, Black, Explored };

UCLASS()
class RTSGAME_API URTSSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** O mapa do skirmish do primeiro passo (Maps.json). A cena de teste (-RTSScenario=) troca pelo "map" dela (16/10:
	 *  os mapas de teste do WorldForge, a laje e a areia v2). */
	static FString SkirmishMapName();

	void StartSkirmish();
	void ReturnToMenu();
	/** A partida em rede acabou: o menu, já na sala (a sessão continua). */
	void ReturnToLobby();
	void Quit(APlayerController* PC);

	/** A névoa da próxima partida (o botão do menu; na LAN, o do anfitrião). */
	ERTSFogMode FogMode = ERTSFogMode::Explored;
	static ERTSFogMode NextFogMode(ERTSFogMode M) { return M == ERTSFogMode::Explored ? ERTSFogMode::Black : (M == ERTSFogMode::Black ? ERTSFogMode::Off : ERTSFogMode::Explored); }

	// (v4, 22/10) o dinheiro inicial da próxima partida (o menu; na LAN, o do anfitrião), de 10 000 a 50 000
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** O escolhido (sem o × 5 do -RTSFast: a montagem multiplica por MoneyScale). Guardado no GameUserSettings.ini, menos
	 *  no teste automático e nos robôs da LAN (o 50 000 do Felipe não entra no teste); -RTSStartMoney=<n> troca sem guardar. */
	int32 StartingMoney = 10000;
	/** As escolhas, em ordem (Units.json "starting_money_choices"), e o fator do -RTSFast. */
	TArray<int32> MoneyChoices;
	int32 MoneyScale = 1;
	/** A escolha seguinte (Dir +1) ou a anterior (-1) na lista, parando nas pontas. */
	int32 StepMoney(int32 From, int32 Dir) const;
	/** Põe o mais perto da lista e guarda (bSave: no GameUserSettings.ini; nunca no teste automático nem nos robôs). */
	void SetStartingMoney(int32 Money, bool bSave = true);

private:
	/** -RTSAutoTest= ou -RTSNetBot: o dinheiro guardado não entra nem sai (o teste não muda o .ini do Felipe). */
	bool bTestRun = false;
};
