#pragma once

#include "Generation/WorldMap.h"

/**
 * O gerador de mundo. Mesma semente + mesmos controles = mesmo mapa.
 *
 * Pipeline (pesquisa em docs/WORLDFORGE.md: Age of Empires RMS, OpenRA,
 * StarCraft II, SAGE/Generals; Quilez, Musgrave, Beyer/Lague):
 *  1. Bases: posições iniciais na fatia do jogador 1, copiadas por rotação.
 *  2. Níveis (grade lógica de 4 m): água · baixada · platô · planalto ·
 *     montanha, por cortes do ruído em frações-alvo (como o SliceElevation do
 *     OpenRA). Bases forçadas planas.
 *  3. Limpeza: filtro de maioria e ilhas pequenas absorvidas.
 *  4. Caminhos: Dijkstra base → centro (e um flanco); onde o caminho cruza um
 *     desnível vira rampa, onde cruza água vira passagem rasa. Regiões grandes
 *     isoladas ganham rampa. Tudo replicado pela simetria.
 *  5. Distância de caminho de cada base → territórios e recursos
 *     (principal, expansão e disputados), na mesma distância pra todos.
 *  6. Relevo visível (grade de 2 m): penhascos suaves nas bordas dos níveis,
 *     margens rasas, montanhas "ridged", rampas, erosão nas encostas.
 *  7. Camadas do chão (terra, vegetação, areia, rocha), fluxo d'água D8.
 *  8. Árvores e rochas (grade com sorteio por célula canônica: simétrico).
 * Toda etapa usa uma sub-semente própria: mexer num controle não embaralha as
 * outras etapas.
 */
class WORLDFORGE_API FWorldGenerator
{
public:
	static TSharedPtr<FWorldMap> Generate(const FWorldGenSettings& Settings);
};
