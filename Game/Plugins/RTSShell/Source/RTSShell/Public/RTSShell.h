#pragma once

// As telas do jogo fora da partida (01/11, docs/concept/ui/TELAS.md): o menu, o skirmish, o multiplayer, as opções e o SCRIPT
// são as páginas HTML aprovadas, rodando no navegador embutido da Unreal por cima do jogo. As páginas moram em Resources/UI
// (copiadas do protótipo por captures/concept/ui/shell/tools/export_game.py) e são servidas em https://rts.ui/; o que vem do
// jogo (os mapas, as cenas, as salas da LAN) sai em https://rts.ui/api/<nome>. As páginas chamam o jogo pelo window.ue.shell
// (URTSShellBridge); o plugin não sabe da simulação: o RTSGame liga as ações (RTSShellGame.cpp).

#include "CoreMinimal.h"

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
};

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
};
