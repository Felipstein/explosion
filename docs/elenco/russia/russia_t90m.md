# T-90M (Rússia, o tanque da gaiola)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque básico: o corpo do exército da Rússia |
| Nome na tela | T-90M (a chave é `russia_t90m`) |
| Custo e tempo | $950, 11 s |
| Vida e blindagem | **560**, tanque (o tanque básico de mais vida) |
| Velocidade | **6,2 m/s** (o mais lento dos básicos), giro do casco e da torre 180°/s |
| Visão | 39,5 m |
| Arma | canhão: 60 perfurante a cada 2 s, alcance 39,5 m (o dos outros básicos), a granada a 105 m/s |
| Esmagador | nível 2: passa por cima de soldado, carro, poste e árvore |
| Requer | Tank Plant |
| Sai de | Tank Plant |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**A gaiola.** O tanque russo de hoje: a torre baixa soviética com a cunha de tijolos de blindagem reativa abrindo em V na frente, o
canhão comprido com o evacuador e, por cima de tudo, o telhado de ripas de aço (a gaiola contra o que cai do alto) com a rede e a
lona da cor do jogador. Pesado, baixo, teimoso.

- **A palavra de cima:** a GAIOLA (o telhado de ripas cobrindo a torre inteira), com a rede e a lona da cor do jogador.
- **Inspiração real:** o T-90M "Proryv" (o 2A46M-5 de 125 mm com carregador automático, a blindagem reativa Relikt, a mira
  Sosna-U, a estação remota com a metralhadora Kord) e a gaiola de ripas que os blindados russos ganharam. Na escala de verdade:
  casco de 6,9 m, 9,5 m com o cano. Sem ficha aprovada ainda (falta a paleta e a folha de ideias, estudo §6).
- **Por que existe:** a Rússia é pouca e pesada: o tanque dela tem mais vida e mais alcance que os outros básicos e anda mais
  devagar.

## 3. Como funciona

- **No jogo:** 60 perfurante a cada 2 s = **30/s** em tanque e prédio, **3/s** em soldado (perfurante faz 10%). Mata um tanque USA
  (480) em 8 tiros (14 s), um Battlemaster (400) em 7 (12 s), um Scorpion (370) em 7 (12 s), uma Technical (180) em 3 (4 s), um
  Overlord (1 100) em 19 (36 s).
- **O duelo com os básicos:** ganha do tanque USA no 1 contra 1 (14 s contra 18 s), por $50 a
  mais; ganha do Battlemaster (12 s contra 18 s), que custa $150 a menos. Pela conta de Lanchester (`docs/ELENCO.md` §8.1), 560 × 30
  ÷ 950² = **0,0186** (USA 0,0178, China 0,0188, GLA 0,0206): dentro dos ±15%. O alcance é o dos básicos (01/11: os 42 m do 1º
  rascunho saíram; o longe da Rússia é do T-14 e do Kornet).
- **A gaiola e o Relikt são só desenho hoje:** a mecânica do estudo (o 1º míssil antitanque perde 50%, uma vez a cada 20 s) não está
  nos dados (ideia, §9).
- **Esmaga:** o soldado que fica na frente morre (200%); é assim que ele mata infantaria.
- **Forte contra:** tanque, veículo leve, prédio, defesa de base no alcance dele.
- **Fraco contra:** soldado antitanque em grupo (3 RPGs, Javelins ou Tank Hunters tiram 120/s: ele cai em ~4–5 s), ar (não atira
  pra cima), infantaria espalhada (o canhão quase não fere soldado), artilharia, o Overlord (mata ele em ~7 s).
- **O contra dele:** o grupo antitanque, o Apache, o Rocket Buggy de longe, o Overlord. **Ele é o contra de:** o tanque básico de
  cada facção no 1 contra 1, a Technical, a Quad Cannon, o caminhão.

## 4. Em batalha

- **Temperamento: o veterano.** Pesado, paciente, teimoso. Não avança antes da hora, mas não recua.
- **O ritmo:** um tiro a cada 2 s, o passo lento; em linha, o trovão compassado (BUM ... BUM-BUM ... BUM) mais grave que o da China.
- **O jogador bom** faz a linha de 4 a 6, com 1 BMPT (a infantaria) e 1 Pantsir-S1 (o ar) atrás; segura a linha parada e deixa o
  inimigo entrar no fogo dela; manda esmagar o grupo de RPG em vez de atirar nele.
- **A IA** faz dele o 1º da fábrica (vale por 2 na mistura).
- **Sozinho e ferido:** não recua sozinho; abaixo de 70%, o fio de fumaça clara; abaixo de 35%, a chama e a fumaça grossa.

## 5. A vida

- **No jogo:** a torre gira 180°/s e volta pra frente 2 s depois de parar; o cano recua no tiro; as esteiras de elos andam
  (`track_step_m`) com a poeira das lagartas; a cratera A no chão onde a granada cai; o raspão (`near_miss` 0,2: o tiro que passa
  perto levanta a areia); os estados de dano (a fumaça abaixo de 70%, a chama e a fumaça grossa abaixo de 35%).
- **Ideia (parado):** o diesel em marcha lenta solta o pulso de fumaça escura pelo escape do lado esquerdo (como no T-72 de verdade);
  a rede da gaiola mexe no vento; as antenas balançam.
- **Ideia (andando):** a lona da gaiola bate no vento; a aba de borracha da saia balança; o casco inclina na arrancada e na freada.
- **Ideia (atirando):** o casco balança pra trás; o sopro da boca levanta a poeira na frente do cano; o evacuador solta o fio de
  fumaça; 2 s depois, o carrossel do carregador automático gira (o som).

## 6. O visual

- **De cima:** o retângulo de 7,2 × 3,9 m (9,5 m com o cano), a gaiola de ripas cobrindo a torre inteira com a rede vermelha na
  frente e a lona vermelha dobrada atrás, a cunha de tijolos em V aparecendo pela frente da gaiola, o cano comprido, o rolo vermelho
  no fim do deck do motor.
- **Assinatura:** a gaiola com a rede e a lona da cor do jogador (nenhuma outra facção tem grade cobrindo a torre).
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras (1,8 m; a paleta A do estudo, provisória até a escolha
  dele); a cor do jogador na rede, na lona da gaiola e no rolo do motor; a estrela vermelha pequena contornada de branco na lateral
  da torre (fixa, slots `Red` e `White`).
- **Nunca:** Z ou V pintados; a faixa branca; a gaiola fechada como caixa (a torre tem de ler girando: estudo §8); cano duplo (é do
  Overlord); a estrela no teto.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão laranja curto e grosso; o anel de fumaça cinza que sai da boca e abre (volume, aceso pelo clarão); a poeira
  levantada no chão na frente (o sopro).
- **O projétil:** o risco curto, quente, branco-amarelado, quase invisível (105 m/s no jogo).
- **O impacto:**
  - no tanque: a faísca forte, a chapa que salta, a marca preta; o raspão levanta a areia atrás do alvo;
  - no prédio: o pedaço de painel que voa e a poeira;
  - no chão: a cratera A e a areia que sobe;
  - no soldado: quase nada (10%); ele mata soldado passando por cima.
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; cobertura pro soldado e sucata ($142 em
  ~9 s) pro construtor.
- **A morte (ideia, a física do carregador automático):** a munição fica no carrossel embaixo da torre; quando ela cozinha, a torre
  salta alto (o defeito conhecido dos tanques soviéticos de carregador automático, documentado desde 1991) e a gaiola se desfaz no ar;
  a torre cai virada do lado e o casco queima aberto.

## 8. O som

- **Seleção (variantes):** o diesel V-12 acelerando com o assobio do turbo; o servo da torre girando curto; a gaiola chacoalhando
  (o "chac" das ripas); o rádio de onda curta chiando.
- **As falas** (inglês, com o sotaque russo leve; o comandante de tanque, voz grave, seca, veterana):
  - selecionado: "T-90 ready." / "Crew standing by." / "Your orders." / "Heavy armor here." / "Steady."
  - andando: "Moving out." / "Advancing." / "Rolling." / "On the move."
  - atacando: "Target." / "Fire!" / "Engaging armor." / "Load and fire."
  - sob fogo: "Taking hits!" / "Armor holding!" / "Watch the sky!" / "We need support!"
  - pronto: "T-90M, reporting."
- **O corpo:** o diesel de tanque em marcha lenta (graves pulsando, com o assobio do turbo por cima); as esteiras com o clank-clank
  médio; o servo da torre com o rangido do anel; as ripas da gaiola batendo nos buracos.
- **A arma:** o estrondo seco com o "tum" no peito; o eco nos prédios; depois, o carrossel do carregador automático (o motor
  elétrico girando e o "clank" da culatra).
- **A morte:** o estouro do combustível, a munição cozinhando em estalos, a torre caindo pesada no chão, o fogo crepitando.
- **A cara da Rússia:** o diesel grave com o turbo, o metal frio, o rádio de onda curta.

## 9. Ideias pra depois

- **A blindagem reativa e a gaiola** (estudo `docs/FACCAO_RUSSIA.md` §4): o 1º míssil antitanque a cada 20 s perde 50% do dano; de
  cima, o tijolo do Relikt que estoura (o clarão curto e o tijolo faltando na cunha). É o que compensa o tanque lento.
- **O míssil pelo cano:** o T-90 de verdade dispara míssil guiado pelo canhão; uma vez a cada 30 s, um tiro a 60 m (além de qualquer
  canhão de tanque) que acerta o veículo andando. O tiro de abertura da Rússia.
- **A berma:** o T-90M parado atrás da berma do IMR-3M só leva tiro na torre (metade do dano), a "posição de casco protegido" da
  doutrina; a Rússia que cava antes de lutar.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_t90m` (feito no fim, com o modelo provisório como anexo de forma).
