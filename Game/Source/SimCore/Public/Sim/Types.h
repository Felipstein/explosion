#pragma once

// Tipos básicos da simulação. C++ puro: nada aqui conhece a Unreal.

#include <cstdint>

namespace Sim
{
	/** Identificador estável de uma entidade na partida. Nunca é reutilizado; 0 = nenhuma. */
	using EntityId = uint32_t;
	constexpr EntityId NoEntity = 0;

	using PlayerIndex = uint8_t;
	/** (v4) Nenhum jogador (o dano sem dono, o tiro de antes do contrato). */
	constexpr PlayerIndex NoPlayer = 0xFF;
	using TickNumber = uint32_t;

	/** Passos da simulação por segundo (o mesmo do Zero Hour, do Spring e do Vanguard). */
	constexpr int32_t TicksPerSecond = 30;
	constexpr int32_t MaxPlayers = 8;
	/** Teto de unidades numa seleção/grupo (o comando fica pequeno e o pacote, limitado). */
	constexpr int32_t MaxGroupSize = 500;

	/** Versão das regras da simulação: replay de outra versão não reproduz. */
	constexpr uint32_t SimVersion = 20;   // 20 (23/10, v4): vender o prédio (Sell, o sellObject do ZH: 50% em 6 s desmontando), o conserto do Dozer (Repair, 2% da vida por segundo, de graça), a linha dos dois botões espalhada pelo comprimento puxado, o caminhão e o Tomahawk derrubando a palmeira pelo choque (½·m·v², a célula da árvore à parte da do carro) e a mira do lançador (o relógio pre_attack_s e o míssil nascendo na boca do tubo), e (a revisão da mesma noite) quem atira parado segurando o lugar (o moveAllies do ZH); 19 (22/10, v4): o contrato da v4 (a patente do general pela luta, os poderes, a zona de lançamento com o avião de carga, o dano em área e o alcance mínimo); 18 (22/10): a carcaça vive 48 s, afunda 7 s e some (os tetos de 40 sólidas e 64 no total); 17 (16/10): a defesa ajuda a vizinha (o AssistedTargeting do ZH) e não desperdiça tiro no alvo já condenado; 16 (16/10): o tanque esmaga o carro e derruba a árvore (a célula esmagável, o freio, a queda); 15 (16/10): o míssil de verdade (a Patriot: sai inclinado, acelera, guia e explode no contato); 14 (16/10): a IA expande quando os depósitos da base secam; 13 (14/10): a defesa de base (a Patriot): prédio que atira, rajada e POWERED; 12 (13/10): a carcaça com física (o vivo empurra, ela segura); 11: render-se e a queda em rede (Resign); 10: a água do WorldForge (grid.png: fundo bloqueia; margem, vau e gelo passam); 9: o tiro forçado (no chão e em qualquer unidade);   // 5 (11/10): o Dozer sai do lote pelo lado livre; 6: vaga livre no ponto de encontro; 7: área de reunião e chegada virada; 8: formação (F)
}
