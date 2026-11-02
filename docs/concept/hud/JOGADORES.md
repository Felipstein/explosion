# Os jogadores no HUD: estudo (02/11)

Feito por um agente depois da v1 reprovada. Fontes conferidas: código do Zero Hour (`InGameUI.cpp`, EA), AoE II e IV, CS2, Overwatch 2, BAR,
guia de acessibilidade do Xbox (XAG 101 e 102). SC2, Dota/Valorant e SupCom por conhecimento geral.

## O que as referências fazem
- **Zero Hour:** o timer de superarma é só texto na cor do dono, com sombra, sem caixa e sem o nome do jogador ("Arma: 4:32", os dois-pontos
  numa coluna); pronto = negrito piscando; passou de 82% da altura = "...".
- **AoE II:** só a maravilha mais próxima de cada jogador aparece na tela. **CS2:** dinheiro do inimigo simplesmente não aparece (sem traço).
- **Overwatch 2:** trocou medalhas por números fáceis de ler; o placar cobrindo a tela foi reclamado e reduzido. **AoE IV:** pontuação opcional.
- **SC2:** sem placar comparativo pro jogador; a comparação fica na tela de resultado. **Dota/Valorant:** relógio no meio do topo, os retratos
  de cada time de um lado, o morto apagado: o padrão pra "quem está vivo" sem lista lateral.

## Regras (1080p)
- Corpo >= 20 px (Barlow; na UMG é ponto a 96 DPI: px = tamanho × 1,33). Números 22–24 px SemiBold, dígitos de largura igual, abreviados (12,4k).
- Rótulos de coluna 15–16 px, maiúsculas, 60% de opacidade, uma vez só. Linha 34–36 px. Entre times, 24–28 px de vazio, sem traço.
- No máximo ~6–7 colunas; o resto vai pra tela de resultado. Contraste >= 4,5:1 (texto): no Tab, painel escuro 80–85%; na tela de jogo,
  nenhuma caixa, só texto com sombra. Cor escura de jogador clareada (~40% de branco) no texto.
- Informação escondida: célula vazia com um único olho fechado, nunca vários "—".

## A recomendação
1. **Placar (Tab):** seu time primeiro, os outros por pontuação; número do time na margem; colunas Jogador (emblema + patente + nome),
   Pontuação, Destruídos/Perdidos, Exército, Dinheiro, Renda/min, Energia (ícone). Saem medalhas, superarma e estado (o derrotado vira a
   linha a 40% com a palavra).
2. **Lista lateral:** sai; "quem está vivo" vai pra barra do topo; o dinheiro dos aliados no mouse na ficha e no Tab.
3. **Superarmas:** o do Zero Hour modernizado, sem caixa, a mais próxima de cada jogador, no máximo 4 linhas e "+N".
4. **Relógio com 3+ times:** peça irmã da barra do duelo, nunca um recorte: seu time à esquerda, os inimigos em grupos de fichas de 28 px
   (12–16 px entre grupos) à direita, o derrotado apagado; em todos contra todos, só as fichas.
