#pragma once

// As telas do jogo fora da partida (01/11, docs/concept/ui/TELAS.md): o menu, o skirmish, o multiplayer, as opções e o SCRIPT
// são as páginas HTML aprovadas, rodando no navegador embutido da Unreal por cima do jogo. As páginas moram em Resources/UI
// (copiadas do protótipo por captures/concept/ui/shell/tools/export_game.py) e são servidas em https://rts.ui/; o que vem do
// jogo (os mapas, as cenas, as salas da LAN) sai em https://rts.ui/api/<nome>. As páginas chamam o jogo pelo window.ue.shell
// (URTSShellBridge); o plugin não sabe da simulação: o RTSGame liga as ações (RTSShellGame.cpp).

#include "CoreMinimal.h"

class SWidget;

/** (06/11) A largada da próxima partida, pra tela de carregar (o filme lê numa thread própria: só estes dados, nada do jogo). A sala
 *  (o skirmish e a LAN) preenche ao começar; o SCRIPT, com o nome da cena. */
struct FRTSMatchIntro
{
	struct FPlayer
	{
		FString Name, Fac;               // o nome na tela ("FE", "AI · HARD") e a facção ("usa", "random"...)
		int32 Team = 0, Base = -1, Diff = 1;
		uint32 Hex = 0xffffff;           // a cor (0xRRGGBB)
		bool bAi = false, bYou = false;
	};
	FString Op, Map, MapReal;            // a operação, o nome do mapa na tela e o do jogo
	bool bLan = false;
	TArray<FPlayer> Players;
};

/** O que o jogo responde às páginas. Cada um pode ficar vazio (a página trata como "não tem"). */
struct FRTSShellHandlers
{
	/** O LAB e o SCRIPT aparecem (o jogo do desenvolvimento ou -RTSLab). */
	TFunction<bool()> IsDev;
	/** A cena de teste da linha de comando (-RTSScenario), pra vir armada no SCRIPT. */
	TFunction<FString()> ArmedScenario;
	TFunction<void(const FString& /*Name*/)> RunScenario;
	TFunction<void()> OpenLab;
	/** O START MATCH do skirmish: a montagem da sala em JSON (o mapa, as vagas, o dinheiro, as superarmas). */
	TFunction<void(const FString& /*SetupJson*/)> StartSkirmish;
	/** O preset de qualidade (0 CLASSIC, 1 LOW, 2 MEDIUM, 3 HIGH, 4 ULTRA) e o atual. */
	TFunction<void(int32)> SetQuality;
	TFunction<int32()> GetQuality;
	TFunction<void()> Quit;
	/** A LAN: "browse", "stop", "host", "join:<ip>:<porta>", "ip:<ip>". Devolve se deu certo. */
	TFunction<bool(const FString& /*Command*/)> Lan;
	/** api/<Path>: o JSON que o jogo devolve (false = 404). */
	TFunction<bool(const FString& /*Path*/, FString& /*OutJson*/)> Api;
	/** (06/11) Os botões das telas da partida: "resume", "menu", "lobby", "restart", "surrender", "net_pause", "net_resume",
	 *  "net_vote", "back_to_room" (o skirmish: o menu e a sala de novo). */
	TFunction<void(const FString& /*Id*/)> MatchButton;
};

/** (06/11) A camada das telas da partida, por cima do jogo (o HUD não desenha as velhas enquanto ela está). */
enum class ERTSMatchOverlay : uint8 { None, Loading, Pause, Result, Net };

class RTSSHELL_API FRTSShell
{
public:
	static void SetHandlers(FRTSShellHandlers In);
	static const FRTSShellHandlers& Handlers();
	/** Põe as telas por cima do jogo, na página pedida (na 1ª vez cria o navegador; depois só troca a página). */
	static void Show(const FString& Page = TEXT("menu.html"));
	/** Tira da tela (o navegador fica vivo, pra voltar na hora). */
	static void Hide();
	static bool IsShown();
	/** O navegador embutido existe neste executável (o CEF) e as páginas estão no disco. */
	static bool IsAvailable();
	/** A pasta das páginas (Resources/UI do plugin). */
	static FString UiDir();

	// (06/11) as telas da partida
	static void SetIntro(const FRTSMatchIntro& In);
	static FRTSMatchIntro GetIntro();
	/** A tela de carregar aprovada, pro MoviePlayer (o RTSGameModule chama no PreLoadMap, na thread do jogo). */
	static TSharedRef<SWidget> MakeLoadingScreen();
	/** Põe (ou tira, com None) a tela da partida por cima do jogo; o RTSShellGame decide qual pelo estado da partida. */
	static void SetMatchOverlay(ERTSMatchOverlay Overlay);
	static ERTSMatchOverlay GetMatchOverlay();
	/** O centro de um botão da tela da partida (o Id do OnButton: "menu", "lobby", "resume"...) em pixels do viewport do jogo, pro teste
	 *  automático clicar como clicava nos botões do HUD velho. Falso se a tela não está ou não tem esse botão. */
	static bool MatchButtonCenter(const FString& Id, FVector2D& OutViewportPixel);
};
