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

struct FRTSHudSuperweapon
{
	FString Name;             // "NUCLEAR MISSILE"
	FString Time;             // "0:42"
	bool bSoon = false;       // falta menos de 1 minuto: vermelho e brilhando
};

/** Um jogador no topo. */
struct FRTSHudPlayer
{
	FString Name;
	FName Emblem;             // "emblem:usa"
	FLinearColor Color = FLinearColor::White;
	bool bMe = false;
	bool bAlly = false;
	bool bDead = false;
	bool bSpied = false;
	float SpyLeft = 0.0f;     // 0..1 o tempo que resta da espionagem
	bool bShowEco = false;    // aliado ou espionado: dinheiro e energia
	FString Money;
	int32 PowerUsed = 0;
	int32 PowerMade = 0;
	TArray<FRTSHudSuperweapon> Superweapons; // a mais próxima primeiro
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

struct FRTSHudState
{
	// o tempo (segundos, qualquer origem): as pulsações (READY, superarma perto, energia faltando)
	float Time = 0.0f;
	// o botão debaixo do mouse e o que está sendo apertado (ids da lista de botões do último quadro)
	FName HoverId;
	FName PressedId;
	float PanelIn = 1.0f;            // 0..1: o painel central e o cartão sobem da borda quando algo é selecionado
	FName SkinFaction;               // a pele do HUD: "china" ou "gla" (kit/skin/<facção>/); vazio = a da USA

	// topo
	FString Clock;
	TArray<FRTSHudPlayer> Players;   // você e os aliados primeiro, depois os outros

	// painel do comandante
	FString Money;
	int32 PowerUsed = 0;
	int32 PowerMade = 0;
	int32 Stars = -1;                // a patente (0 a 5); -1 = o jogo ainda não tem patente: o painel fica numa linha só
	float Xp = 0.0f;
	int32 Promote = 0;               // pontos pra gastar (0 = sem botão)

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

	// painel central
	bool bCommandPanel = false;
	TArray<FRTSHudSlot> Main;        // 18 (2 × 9)
	/** Colunas da grade principal: 9 (a planta) ou menos, com as casas maiores do clássico (em teste, 13/10). */
	int32 MainColumns = 9;
	TArray<FRTSHudSlot> Actions;     // 6 (2 × 3)

	FRTSHudCard Card;

	// dica (no mouse, em cima do botão apontado)
	FName TooltipFor;                // id da casa apontada
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
