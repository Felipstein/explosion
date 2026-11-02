# Os 4 prédios da GLA refeitos (itens 48 a 51 do `TAREFAS.md`, 01/11/2026)

A galeria (01/11, 15h20): o quartel, o Palace, o Stinger Site e o Supply Stash REPROVADOS; o Palace com a frase que vale pros quatro:
"sai um pouco dessa de tudo ser murado assim". A folha: `docs/aprovados/modelos/2026-11-01_galeria_elenco_17_refazer_e_rumo.jpg`.

## 1. Por que os quatro falharam

Os quatro são a mesma receita: o pátio quadrado de adobe com o muro em volta e a coisa dentro. De cima, quatro quadrados bege com
miolo diferente; nenhum tem uma forma que se lê sozinha. E a laje quadrada por baixo de tudo (a regra da galeria, item 44: "a base
acompanha a forma"). O que foi aprovado da GLA (o Command Center, o Arms Dealer com a rampa do túnel) tem uma peça dominante; os
quatro não.

## 2. A regra pros quatro

- **Sem muro em volta.** Uma forma por prédio, que lê de cima em 1 segundo e não repete a de outro prédio do jogo.
- **A base acompanha a forma** (o redondo pisa no redondo; a lona pisa na terra batida, sem laje).
- **O material da GLA** (`docs/CONCEITO_CONSTRUCOES.md`): adobe, chapa enferrujada, lona, rede de camuflagem, madeira; a cor do
  jogador é o verde (as bandeiras, os trapos e as faixas pintadas à mão). Nada de montanha de caixas (o veto dele).
- **O detalhe B** (`model_run_detail.py`), os 3 estados de dano (`model_damage.py`), o tiro (`elenco_hitmesh.py`), a bandeira com
  física (`model_lib.flag`, o `flags.json`), as luzes que apagam sem energia; o lote de hoje (`Units.json` `size_m`).

## 3. A palavra de cada um

| Prédio | A palavra | De cima |
|---|---|---|
| **Barracks** (o quartel) | **o acampamento da tenda beduína**: a tenda grande de pelo de cabra escura (o *beit al-sha'ar*), baixa e comprida, sustentada por varas e esticada por cordas que saem em leque até as estacas; o campo de treino na frente (os pneus, o arame de rastejar, os alvos de saco, o cavalete das armas) e a fogueira apagada | o retângulo escuro da lona com as cordas em raio (nenhuma peça do jogo é escura e mole assim) |
| **Palace** | **o palácio tomado, aberto**: o mesmo palácio de cúpula turquesa (a ficha `docs/elenco/gla/gla_palace.md`), mas solto no terreno, sobre o pódio em cruz com degraus (sem muro), a arcada nos 4 lados, as 4 cupulinhas dos cantos, os ninhos de saco no teto, os estandartes verdes pendurados na arcada, o carro de luxo roubado sob a lona | o círculo turquesa no meio da cruz |
| **Stinger Site** | **o ninho em trevo**: 3 covas redondas de sacos de areia ligadas em trevo em volta da cova da munição, cada uma com o lançador no tripé apontado pro céu, e a rede de camuflagem esticada em varas por cima, com os trapos verdes | o trevo de 3 círculos sob a rede |
| **Supply Stash** | **o mercado sob a rede**: a rede de camuflagem grande e irregular esticada em varas (o contorno orgânico é a silhueta), por baixo as pilhas de caixas, sacos e tambores em ruas, o carrinho de mão, a boca do túnel de onde sai a carga (a marca da GLA) e o pátio de terra batida onde a picape encosta | a mancha irregular da rede, com as pilhas aparecendo pelos furos |

O Worker (item 52: "parece que ele tá segurando uma arma") usa o boneco da infantaria (`T_Soldier_GLA_WORKER`, as peças): é da sessão
da infantaria, que o Felipe está vendo.

## 4. Fontes

- A tenda beduína (o pelo de cabra, as varas, as cordas em leque): https://en.wikipedia.org/wiki/Bedouin#Tents
- Os ninhos de MANPADS em sacos de areia (as covas redondas): https://en.wikipedia.org/wiki/Man-portable_air-defense_system
- A rede de camuflagem: https://en.wikipedia.org/wiki/Camouflage_netting
- Os prédios da GLA no ZH (o Stinger Site de 3 soldiers no ninho, o Supply Stash de caixas sob a lona): https://cnc.fandom.com/wiki/Stinger_Site_(Generals_1) ,
  https://cnc.fandom.com/wiki/Supply_Stash
