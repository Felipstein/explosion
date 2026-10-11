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
	/** O mapa do skirmish do primeiro passo (Maps.json). */
	static FString SkirmishMapName() { return TEXT("World_TestSalt2"); }

	void StartSkirmish();
	void ReturnToMenu();
	/** A partida em rede acabou: o menu, já na sala (a sessão continua). */
	void ReturnToLobby();
	void Quit(APlayerController* PC);

	/** A névoa da próxima partida (o botão do menu; na LAN, o do anfitrião). */
	ERTSFogMode FogMode = ERTSFogMode::Explored;
	static ERTSFogMode NextFogMode(ERTSFogMode M) { return M == ERTSFogMode::Explored ? ERTSFogMode::Black : (M == ERTSFogMode::Black ? ERTSFogMode::Off : ERTSFogMode::Explored); }
};
