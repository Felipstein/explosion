#pragma once

// O estado do HUD: só dados, sem saber da simulação. O jogo preenche com a partida; o HUDLab, com os
// estados de teste. As regras de cada peça: docs/concept/hud/DECISOES.md; os nomes: NOMES.md.
//
// Imagens: um FName com prefixo, resolvido por quem desenha (FRTSHudPainter::FTextureLookup):
//   "icon:<nome>"   docs/concept/hud/kit/icons/<nome>.png
//   "btn:<unidade>" docs/concept/hud/kit/photos/buttons/<unidade>.png (foto do botão)
//   "card:<unidade>" docs/concept/hud/kit/photos/card/<unidade>.png (retrato largo do cartão)
//   "emblem:<facção>", "minimap", "bg"

#include "CoreMinimal.h"

/** Uma casa da grade principal ou da grade de ações (ou um item da fila). */
struct FRTSHudSlot
{
	bool bEmpty = true;
	FName Id;                 // o id do botão (o clique devolve este id)
	FName Image;              // foto ou ícone; vazio = só o nome
	FString Label;            // o nome (dica, ou texto quando não há imagem)
	FString Price;            // "$900", "✓", "40%"...
	TCHAR Key = 0;            // a tecla, no canto de cima à esquerda
	int32 Count = 0;          // quantos na fila (o selo branco)
	float Progress = -1.0f;   // 0..1 carregando (o fundo escuro do C&C some no sentido do relógio); <0 = parado
	float Health = -1.0f;     // 0..1, só nas casas de unidade selecionada
	bool bLocked = false;     // falta prédio: mais escuro, com o cadeado
	bool bUpgrade = false;    // melhoria: moldura dourada tracejada
	bool bDone = false;       // melhoria já comprada: apagada, com o check verde
	bool bAction = false;     // ordem (grade de ações): moldura verde
	bool bCaptain = false;    // a unidade principal da seleção: moldura dourada com brilho
	bool bNoMoney = false;    // preço vermelho
	bool bCancelMark = false; // fila: o X vermelho do cancelar (mouse em cima)
};

/** Uma superarma de um jogador (a lista C no canto e a coluna do placar juntam as do mesmo dono e tipo numa linha). */
struct FRTSHudSuperweapon
{
	FName Type;               // o tipo: junta as iguais do mesmo dono ("nuclear_missile"); a capturada tem o tipo dela
	FString Name;             // "Nuclear Missile"
	FName Glyph;              // o desenho da arma, branco (pintado na hora): "skin:sw_nuclear"
	int32 Seconds = 0;        // o que falta pra disparar; 0 = pronta
	int32 FullSeconds = 600;  // a carga inteira (o arco de carga em volta do desenho)
};

/** Um jogador: a ficha no topo, a linha do placar (Tab) e as superarmas dele. */
struct FRTSHudPlayer
{
	FString Name;
	FName Emblem;             // "emblem:usa" (a ficha usa a silhueta "emblem:glyph_usa")
	FLinearColor Color = FLinearColor::White;
	int32 Team = 0;           // o time (o seu é o 0); todos contra todos: cada um no seu
	bool bMe = false;
	bool bAlly = false;
	bool bDead = false;
	bool bSpied = false;
	float SpyLeft = 0.0f;     // 0..1 o tempo que resta da espionagem
	bool bShowEco = false;    // aliado ou espionado: dinheiro e energia
	FString Money;
	int32 PowerUsed = 0;
	int32 PowerMade = 0;
	TArray<FRTSHudSuperweapon> Superweapons;

	// o placar (Tab)
	FName RankIcon;           // a patente: "skin:rank_usa_3", "skin:china/rank_china_3"
	int32 Score = 0;
	int32 Destroyed = 0;
	int32 Lost = 0;
	int32 Army = 0;           // o valor do exército em $
	int32 Cash = 0;
	int32 Income = 0;         // por minuto
};

struct FRTSHudAlert
{
	FString Text;
	FLinearColor Color = FLinearColor::Red;
	float Age = 99.0f;        // segundos desde que apareceu: entra deslizando da esquerda nos primeiros 0,3 s
};

/** Um poder do general ou uma superarma sua, na coluna da esquerda. */
struct FRTSHudPower
{
	FName Icon;
	float Remaining = 0.0f;   // 0..1 o que falta da recarga (0 = pronto)
	FString Time;             // "2:31"
};

struct FRTSHudGroup
{
	int32 Count = 0;
	float Health = 1.0f;      // a vida somada do grupo, 0..1
};

struct FRTSHudMinimapDot
{
	FVector2D UV = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

/** O cartão da seleção (canto de baixo à direita), ou a placa da facção sem nada selecionado. */
struct FRTSHudCard
{
	bool bShow = false;
	FString Name;
	FName Portrait;           // "card:usa_battle_tank"
	int32 Health = 0;
	int32 MaxHealth = 0;
	FName Insignia;           // melhoria no canto de cima à esquerda do retrato
	int32 Rank = 0;           // divisas no canto de cima à direita (0 = nenhuma)
	TArray<FRTSHudSlot> Queue;// a fila da fábrica ou da pesquisa (5 casas)

	bool bFactionPlate = false;
	FString FactionName;      // "UNITED STATES"
	FName FactionEmblem;
	FString PlateLine;        // "Army 24 · Buildings 9"
	FString PlateKills;       // "KILLS  Units 31 · Buildings 6"
};

/**
 * (v4, 23/10) O relógio de um prédio que trabalha sozinho (a Supply Drop Zone e o avião de carga; o CB_CONTEXT_OCL_TIMER do
 * ZH): com ele, a esquerda do painel do meio fica sem a grade principal e mostra o medalhão, o rótulo, o tempo, a linha e a
 * barra (a imagem 4 do GPT que o Felipe escolheu); a grade de ações da direita fica igual.
 */
struct FRTSHudTimer
{
	bool bShow = false;
	FName Medallion;          // "skin:timer_medallion_airlift" (o medalhão redondo com o emblema)
	FString Title;            // "NEXT AIRLIFT" (maiúsculas: vai espaçado)
	FString Time;             // "0:42"
	FString Line;             // "4 pallets  ·  +$1,200"
	float Fill = 0.0f;        // 0..1, a barra (quem preenche já manda em degraus de 1 s, como o ZH)
	bool bInbound = false;    // o avião a caminho: o rótulo em âmbar e a luz respirando
	bool bPaused = false;     // sem energia: o rótulo em âmbar, a barra parada em âmbar, a luz apagada
};

/** (04/11) Um poder da árvore do general (Game/Config/RTS/GeneralTrees.json, exportado de docs/poderes/tela/<facção>.js). */
struct FRTSHudGenNode
{
	FName Id;
	FString Name;             // "Naval Gunfire"
	FString Level;            // "II" (vazio = sem níveis)
	FName Icon;               // "doc:poderes/tela/icones/usa_naval_esc"
	int32 Rank = 1;           // a patente que abre (1..5): a coluna
	int32 Row = 0;            // a linha (sem rótulo: o tipo se lê no ícone)
	int32 Cost = 1;
	FString Cooldown;         // "3:00", "Passive"
	FString Desc;
	FName Req;                // o nível anterior, que precisa estar comprado
};

/** Uma comenda da faixa de baixo da tela do general. */
struct FRTSHudGenMedal
{
	FString Name;             // "Tank Killer"
	FString Line;             // "10 vehicles · 7 / 10"
	float Progress = -1.0f;   // 0..1 a barra; < 0 sem barra
	bool bDone = false;
	int32 Xp = 0;             // "+300"
	FName Icon;               // "skin:medal_tank_killer" (a da facção, se houver)
};

/**
 * (04/11) A tela do general (General's Command, aprovada em 02 e 03/11: a moldura de cada facção, a linha do tempo das patentes, a árvore
 * calma sem as linhas por tipo, o detalhe à direita e as comendas embaixo). Abre pelo cubo da patente; a batalha não para.
 */
struct FRTSHudGeneral
{
	bool bOpen = false;
	bool bLegible = false;           // (05/11, ele: "não consigo decifrar e ler tudo") a versão B: ícone grande, nome em destaque, o custo em losangos
	int32 Rank = 1;                  // 1..5
	float NextFrac = 0.0f;           // o XP feito rumo à próxima patente (0..1)
	FString XpLine;                  // "1,750 / 5,000 XP"
	int32 PointsLeft = 0;
	int32 PointsPerRank[5] = { 1, 1, 2, 2, 3 };
	FString RankAbbr[5];             // "LTG"
	FString RankName[5];             // "Lieutenant General"
	int32 Rows = 4;
	TArray<FRTSHudGenNode> Nodes;
	TArray<FName> Owned;
	TArray<FName> Soon;              // (05/11) os poderes que o jogo ainda não tem (a simulação não conhece o nó): aparecem apagados e não compram
	TArray<FRTSHudGenMedal> Medals;
	FName Shake;                     // o poder que tentou comprar sem poder (treme)
	float ShakeAge = 99.0f;

	/** A árvore da facção, os nomes das patentes e os pontos por patente, de Game/Config/RTS/GeneralTrees.json. Falso se não achou. */
	RTSHUD_API bool LoadTree(const FString& JsonPath, const FString& Faction);
	/** Os pontos que sobram na patente Rank com os comprados (os pontos de cada patente somados, menos o custo do que já tem). */
	RTSHUD_API int32 PointsLeftFor(int32 InRank) const;
	/** Pode comprar agora (a patente, o nível de antes e os pontos)? */
	RTSHUD_API bool CanBuy(FName Id) const;
};

/** (05/11) O dinheiro que entra ou sai voando perto do mostrador: "+$1,200" (verde, voa pela curva até o número) ou "−$900" (vermelho, cai). */
struct FRTSHudMoneyChip
{
	FString Text;
	bool bIn = true;
	float Age = 0.0f;
};

/** (04/11) Uma linha do chat: quem fala (vazio no sistema), o texto, a cor do jogador, ALLIES e a idade (entra em 0,3 s, some aos 12 s). */
struct FRTSHudChatLine
{
	FString Who;
	FString Text;
	FLinearColor Color = FLinearColor(FColor(230, 236, 243));
	bool bAllies = false;
	bool bSystem = false;            // a linha do jogo (fulano foi derrotado): cinza, sem nome
	float Age = 0.0f;
};

/** (04/11) O chat: as linhas (a mais nova no fim; o pintor mostra as 6 últimas) e a caixa de digitar (Enter abre). */
struct FRTSHudChat
{
	TArray<FRTSHudChatLine> Lines;
	bool bTyping = false;
	bool bAllies = true;             // a aba escolhida na caixa
	FString Draft;
	float TypingAge = 0.0f;          // desde que a caixa abriu (a entrada e o cursor piscando)
	float PingAge = 99.0f;           // desde a última mensagem que chegou: o botão do chat no minimapa pisca (2 vezes, 1 s cada)
};

struct FRTSHudState
{
	// o tempo (segundos, qualquer origem): as pulsações (READY, superarma perto, energia faltando)
	float Time = 0.0f;
	// o botão debaixo do mouse e o que está sendo apertado (ids da lista de botões do último quadro)
	FName HoverId;
	FName PressedId;
	float HoverAge = 99.0f;          // (04/11) segundos desde que o mouse entrou no HoverId (a mola, a luz que passa)
	float PressAge = 99.0f;          // e desde que apertou
	float PanelIn = 1.0f;            // 0..1: o painel central e o cartão sobem da borda quando algo é selecionado
	FName SkinFaction;               // a pele do HUD: "china" ou "gla" (kit/skin/<facção>/); vazio = a da USA

	// topo
	FString Clock;
	TArray<FRTSHudPlayer> Players;   // você primeiro, depois os aliados, depois os outros time a time

	// o placar (Tab aberto): a janela no meio com a moldura da facção; a lista C das superarmas some (está toda nele)
	bool bScoreboard = false;
	FRTSHudGeneral General;          // (04/11) a tela do general
	bool bScoreboardPassive = false; // só pra olhar (a foto do teste): desenha, mas não segura o clique de quem joga
	FString ModeLabel;               // "3 vs 5 vs 8", "Free for all"
	int32 ScoreSwScroll = 0;         // a coluna das superarmas rolada (linhas; o jogo soma com a rodinha, o pintor limita)

	// painel do comandante
	FString Money;
	int32 PowerUsed = 0;
	int32 PowerMade = 0;
	int32 Stars = -1;                // a patente (0 a 5); -1 = o jogo ainda não tem patente: o painel fica numa linha só
	float Xp = 0.0f;
	int32 Promote = 0;               // pontos pra gastar (0 = sem botão)
	// (04/11) as plaquetas do console: a vermelha que nasce em cima do brasão (LOW POWER, INSUFFICIENT FUNDS: o título e a linha do
	// que falta) e a dourada do PROMOTED em cima do cubo (o nome da patente nova); Age = segundos desde que nasceu (a mola)
	FString PlateTitle;
	FString PlateLine;
	float PlateAge = 99.0f;
	FString PromotedRank;
	float PromotedAge = 99.0f;
	FString PromotedTitle;           // (05/11) vazio = PROMOTED; na patente 1 (o começo da partida, como no ZH) FIRST POWER, sem o nome
	bool bLowMoney = false;          // o dinheiro não paga o que foi pedido: o mostrador em vermelho
	bool bCleanSlots = false;        // (05/11, pedido dele: "o HUD muito sujo") as casas sem a letra do atalho e sem o preço: a dica mostra
	bool bHideEmptySlots = false;    // (05/11, pedido dele) as casas vazias somem: no painel só aparece o que existe
	float PlateLife = 0.0f;          // (05/11) > 0: a plaqueta vermelha sai aos PlateLife s (a mola ao contrário, 0,25 s); 0 = fica
	FName FlashId;                   // (05/11) a casa clicada sem dinheiro pisca em vermelho (0,9 s desde FlashAge = 0)
	float FlashAge = 99.0f;
	float CrestShakeAge = 99.0f;     // (05/11) e o brasão do dinheiro treme (0,45 s)
	float BrownAge = -1.0f;          // (05/11) >= 0: faltando energia há BrownAge s: o apagão do console (o tranco, o lembrete a cada 12 s) e o raio
	TArray<FRTSHudMoneyChip> MoneyChips;   // (05/11) o dinheiro voando (o jogo some com eles depois de 1,3 s)

	TArray<FRTSHudAlert> Alerts;
	bool bOffscreenArrow = false;
	TArray<FRTSHudPower> Powers;

	// minimapa
	FName MinimapImage;
	TArray<FRTSHudMinimapDot> MinimapDots;
	FBox2D MinimapCamera = FBox2D(ForceInit); // em UV

	// grupos 1–0
	TArray<FRTSHudGroup> Groups;     // 10
	bool bGroupsAtBottom = false;    // nada selecionado: descem pra borda
	int32 IdleWorkers = -1;          // (01/11) o botão do trabalhador: os construtores parados (−1 = o botão de antes, sem o jogo dizer)
	FName WorkerImage;               // e a foto do construtor da facção

	// painel central
	bool bCommandPanel = false;
	TArray<FRTSHudSlot> Main;        // 18 (2 × 9)
	/** Colunas da grade principal: 9 (a planta) ou menos, com as casas maiores do clássico (em teste, 13/10). */
	int32 MainColumns = 9;
	TArray<FRTSHudSlot> Actions;     // 6 (2 × 3)
	FRTSHudTimer Timer;              // (v4) com bShow, no lugar da grade principal
	FRTSHudChat Chat;                // (04/11) o chat da partida

	FRTSHudCard Card;

	// dica (no mouse, em cima do botão apontado)
	FName TooltipFor;                // id da casa apontada
	uint8 TopEmblem = 1;             // o emblema no topo: 1 a silhueta na cor do jogador (APROVADA 02/11), 0 o inteiro, 2 a ficha nas cores da facção
	FString TooltipTitle;
	FString TooltipKey;
	FString TooltipCost;
	FString TooltipLine;
	bool bTooltipLineRed = false;
	bool bDrawCursor = false;        // o laboratório desenha a seta do mouse na casa apontada
};

/** Um botão desenhado no último quadro (pro clique). */
struct FRTSHudButton
{
	FName Id;
	FBox2D Box;
};
