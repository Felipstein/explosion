# IMR-3M (Rússia, o trator de engenharia)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o construtor: ergue a base, conserta e desmancha carcaça em dinheiro |
| Nome na tela | IMR-3M (a chave é `russia_dozer`) |
| Custo e tempo | $1 000, 5 s |
| Vida e blindagem | **300**, tanque (o Dozer da USA e o da China têm 250) |
| Velocidade | **7,2 m/s** (os outros: 7,9), giro 90°/s, arranca devagar (7,9 m/s²) |
| Visão | 52,7 m |
| Arma | nenhuma |
| Esmagador | nível 2: passa por cima de soldado, carro, poste e árvore |
| Conserta | 2% da vida por segundo |
| Constrói | Command Center, Gas Power Plant, Supply Depot, Barracks, Tank Plant, Research Institute, Pantsir Site |
| Sai de | Command Center (a Rússia começa com 1) |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O braço com a garra e a lâmina em V.** O veículo de engenharia sobre o chassi de tanque: a lâmina em V na frente (as 2 asas
côncavas formando a ponta), a mesa giratória com a cabine blindada hexagonal do operador, a gaiola pequena com a rede e a sirene, e o
braço telescópico com a garra dobrado pra trás por cima do motor, passando da traseira. Não é um trator de obra: é um sapador
blindado.

- **A palavra de cima:** o BRAÇO COMPRIDO correndo pra trás com a garra passando da traseira + a PONTA da lâmina em V na frente.
- **Inspiração real:** o IMR-3M (o veículo de engenharia sobre o chassi do T-90: a lâmina que trabalha em V ou reta, o braço
  telescópico com a garra, a cabine blindada). Modelado no tamanho real (8,7 m) e posto na **escala de jogo 0,78** (6,8 m de ponta
  a ponta): o apoio fica menor que o tanque (`GAME_SCALE` no `model_russia_dozer.py`; `docs/elenco/TAREFAS.md` 31).
- **Por que existe:** sem ele não há base. E é a ponte pro pilar da Rússia (o chão moldado de propósito: a trincheira e a berma, que
  ainda são ideia, §9).

## 3. Como funciona

- **No jogo:** constrói os 7 prédios da Rússia; conserta prédio e veículo a 2% por segundo (um Shtab de 5 000 sobe 100 por segundo);
  **desmancha carcaça** (15% do custo de quem morreu, em 3 s + 1 s a cada $150: a carcaça de um T-14 dá $270 em 15 s) e **entulho
  de prédio** (10% em 4 s + 1 s a cada $200). A IA manda o trator parado recolher a sucata a até 90 m de casa.
- **O jeito da Rússia:** mais duro e mais lento que os outros construtores (300 contra 250 de vida, 7,2 contra 7,9 m/s): chega
  depois, aguenta mais.
- **Esmaga:** nível 2, como o tanque: o soldado que fica na frente dele morre (o esmagamento faz 200% no soldado).
- **Forte contra:** o tempo (a base), a sucata, o soldado distraído no caminho.
- **Fraco contra:** tudo que atira: 300 de vida (o RPG mata em ~7 s, a Technical em ~24 s, o tanque em 8 s).
- **O contra dele:** qualquer raide. **Ele é o contra de:** a base inimiga que perde a guerra de sucata.

## 4. Em batalha

- **Temperamento: o sapador.** Calado, cuidadoso, teimoso. Mede o chão antes de cavar e não larga o serviço com bala passando.
- **O ritmo:** lento e pesado: a lâmina empurra, o braço gira, o motor sobe e desce com a carga.
- **O jogador bom** tem 2 IMR-3M: um ergue, o outro conserta e recolhe a sucata atrás da linha depois de cada combate; nunca manda o
  trator na frente.
- **A IA** usa o trator pra lista da base e, parado, recolhe a carcaça mais perto até 90 m de casa.
- **Sozinho e ferido:** abaixo de 70%, o fio de fumaça clara; abaixo de 35%, a chama e a fumaça grossa. Não foge sozinho.

## 5. A vida

- **No jogo:** a sirene âmbar gira no canto da gaiola da cabine (peça `_Beacon`, 90 rpm); a lâmina em V gira no pivô do nariz
  quando trabalha (peça `_Blade`); as esteiras de elos andam (`track_step_m`) com a poeira das lagartas; os estados de dano do
  veículo (a fumaça abaixo de 70%, a chama abaixo de 35%).
- **Ideia (parado):** o diesel em marcha lenta solta o fio de fumaça escura; o braço descansa dobrado sobre o motor e a garra
  balança um pouco.
- **Ideia (construindo):** o braço se estende e a garra levanta os painéis pré-moldados da obra (a Rússia montada peça por peça,
  como o prédio de painel de verdade); a lâmina aplaina o lote; a poeira sobe na frente dela (volume, na luz do mundo).
- **Ideia (desmanchando sucata):** a garra agarra a torre da carcaça e arranca; a lâmina empurra o resto pro monte.
- **Ideia (consertando):** o braço segura a chapa enquanto a faísca de solda sai do lado da cabine.

## 6. O visual

- **De cima:** o casco comprido com a ponta da lâmina em V na frente (mais larga que as esteiras), a cabine hexagonal com a gaiola
  pequena e a rede vermelha, a sirene em cima, o braço comprido correndo pra trás e a garra aberta passando da traseira, as lonas
  vermelhas enroladas no deck do motor.
- **Assinatura:** o braço com a garra + a lâmina em V.
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras (a camuflagem da Rússia, no material; a paleta A do
  estudo, provisória até a escolha dele); a cor do jogador na rede da gaiola da cabine, na lona enrolada no motor e na lona dobrada
  na mesa; o âmbar da sirene.
- **Nunca:** arma; o amarelo de obra civil; a lâmina reta e curva de trator civil (é a do Dozer da China); Z ou V; a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** a bala de fuzil faz faísca na gaiola da cabine; o canhão amassa uma asa da lâmina (a lâmina segura o primeiro
  tiro de frente, só desenho).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; serve de cobertura pro soldado e de
  sucata pra outro construtor ($150 em ~10 s).
- **A morte (ideia):** o diesel estoura (a bola laranja suja e a fumaça preta), a cabine pega fogo, o braço desaba de lado e fica
  estendido no chão com a garra aberta (a peça que todo mundo reconhece de cima).

## 8. O som

- **Seleção (variantes):** o diesel V-12 acelerando com o assobio do turbo; a hidráulica do braço (o "zuuuu" e o "tchac" da trava);
  a lâmina batendo no chão; a sirene com o bip de ré.
- **As falas** (inglês, com o sotaque russo leve; o sapador veterano, voz grave, calma, de poucas palavras):
  - selecionado: "Engineer here." / "IMR, ready." / "Where do we dig?" / "Ground first, then steel." / "Show me the plan."
  - andando: "Moving." / "On my way." / "Clearing the road." / "Slow and heavy."
  - construindo: "Setting the panels." / "Foundation is in." / "Building."
  - sucata: "Scrap for the plant." / "Nothing is wasted." / "Pulling the wreck."
  - consertando: "Repairing." / "Hold still."
  - sob fogo: "I am not a tank!" / "Taking fire, need cover!" / "They are shooting the engineer!"
  - pronto: "IMR-3M ready."
- **O corpo:** o diesel pesado do chassi de tanque (rouco, com o assobio do turbo; mais grave que o Dozer da China); as esteiras com
  o clank lento; a hidráulica do braço; o bip de ré.
- **A construção:** a garra pegando e assentando os painéis (o "toc" do concreto na junta), a lâmina raspando, o motor sob carga.
- **A morte:** o estouro do diesel, o fogo crepitando, o braço caindo no chão com o metal longo.
- **A cara da Rússia:** o diesel grave, a hidráulica pesada, a voz baixa e seca.

## 9. Ideias pra depois

- **A trincheira e a berma** (estudo `docs/FACCAO_RUSSIA.md` §4.4, o pilar da Rússia): o IMR-3M cava a trincheira (o soldado dentro
  leva metade da bala, como na carcaça) e levanta a berma (o tanque atrás dela só leva tiro na torre). O chão que muda de
  propósito: a Rússia molda o campo.
- **O dente de dragão:** a linha de blocos de concreto antitanque que o braço assenta (bloqueia veículo, deixa o soldado passar); a
  base com fortificação que dá bônus nasce dela.
- **A corda explosiva** (o UR-77, estudo §5.4): outro veículo de engenharia, destravado pelo trator: a linha de 90 m de explosões
  que limpa campo minado e derruba muro.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_dozer` (feito no fim, com o modelo provisório como anexo de forma).
