# Nuclear Reactor (China, a energia que explode)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a usina: a energia da base China (o dobro da USA), com o preço de explodir quando cai |
| Custo e tempo | $1 000, 10 s |
| Vida e blindagem | 1 500, prédio |
| Energia | **+10** (a usina da USA dá +5) |
| Visão | 52,7 m |
| Lote | 12 × 16 m |
| Arma | nenhuma em vida; **na morte, 400 explosivo** em volta (o `death_blast`) |
| Requer | nada |
| Produz | nada; destrava o Supply Center e a Gattling Cannon |

## 2. A ideia

**Duas torres de resfriamento soltando vapor, e o domo do reator no meio.** É o prédio mais poderoso e mais perigoso da base:
dá energia pra quase tudo, mas onde ele fica vira bomba. A China aceita o risco; é a cara dela.

- **A palavra de cima:** os DOIS CÍRCULOS FUMEGANTES (as bocas das torres em ampulheta).
- **Inspiração real:** as usinas nucleares com torres hiperbólicas de concreto (a cintura marcada, os pés em V por onde o ar
  entra) e o prédio de contenção cilíndrico com o domo. O que sai das torres de verdade é **vapor d'água**, branco, não fumaça.
- **A física da morte, conferida:** um reator de verdade não vira bomba atômica. O que explode é o vapor e o hidrogênio
  (Chernobyl em 1986: a explosão de vapor arrancou a tampa; Fukushima em 2011: as explosões de hidrogênio nos prédios). O desenho
  da morte segue isso: a tampa que voa e a nuvem branca, nunca o cogumelo.
- **Por que existe:** a escolha da China (`docs/ELENCO.md` §7.2): energia dobrada, mas cada reator é um ponto fraco na base.

## 3. Como funciona

- **No jogo:** +10 de energia; 1 500 de vida. Quando cai **pronto**, solta o dano de morte: **400 explosivo** até 13,2 m da borda
  do lote, caindo em linha reta até **12% (48)** a 26,3 m; em **todo mundo** (aliado, inimigo, neutro), sem XP pra ninguém. A obra
  inacabada e o reator vendido **não** explodem (`World.cpp`, `ApplyDeathBlast`).
- **O que 400 faz:** mata qualquer soldado, qualquer veículo do jogo (o Overlord tem 1 100: perde 36%), tira 8% de um CC, 27% de
  outro reator (que não estoura em cadeia sozinho: 1 500 de vida), 20% de uma War Factory.
- **Sem ele:** a energia baixa deixa a produção a 50–80% e desliga a Gattling Cannon (`needs_power`).
- **Forte contra:** nada em vida. Morto, é a arma: o inimigo que o derruba colado nele morre junto.
- **Fraco contra:** tudo que mira prédio (o tanque derruba em 50 s; o Overlord em ~22 s; 4 Rocket Buggies em 4 salvas, ~23 s).
- **O contra dele:** o raide de tanque ou de artilharia na retaguarda. **Ele é o contra de:** o raide preguiçoso que para do lado
  dele pra atirar.

## 4. Em batalha

- **Temperamento: o gigante nervoso.** Forte e instável. Trabalha calado, mas todo mundo anda com cuidado perto dele.
- **O ritmo:** a pulsação do núcleo; quando ferido, a pulsação acelera (ideia).
- **O jogador bom** o põe atrás da base, longe do CC e da fábrica, mas perto o bastante das defesas pra quem o atacar ficar no raio
  dele; ou, ousado, no caminho do inimigo como armadilha (o inimigo que o derruba de perto morre junto).
- **A IA** o constrói primeiro (a lista: usina, centro, fábrica, defesa) e a conta de energia dela pede o próximo quando precisa.
- **Sozinho e ferido:** abaixo de 35%, o fogo no alto das torres; aí é a hora de afastar as unidades.

## 5. A vida

- **No jogo:** o andaime de aço na obra; abaixo de 35%, o fogo nos 2 pontos do alto (`fire_points_m`) e os buracos.
- **Ideia (parado):** a coluna de vapor branco sobe das duas torres, alta e lenta, inclinada pelo vento do mapa (o vapor vive na
  luz do mundo: recebe a sombra, faz sombra, é volume); as balizas vermelhas do aro piscam alternadas; a janela redonda do domo
  pulsa num azul-branco fraco (o "brilho do núcleo", ficção legível).
- **Ideia (ferido):** acima de 50%, o vapor vai ficando irregular, em golfadas; abaixo de 35%, a sirene de alerta gira no bloco
  de controle e o domo solta vapor pelas rachaduras; a pulsação do núcleo acelera.
- **Ideia (vendido ou na obra):** sem vapor; o domo escuro.

## 6. O visual

- **De cima:** dois círculos grandes atrás (as bocas escuras das torres, com o vapor saindo), o círculo menor do domo na frente com
  o **trevo da radiação** amarelo e preto pintado em cima, o anel de perigo no chão em volta.
- **Assinatura:** as torres de resfriamento em ampulheta (proibidas na usina da USA: são da China).
- **Cores:** concreto claro nas torres (o único prédio China com concreto claro, por ser a torre de verdade), o resto oliva escuro;
  o vermelho do jogador na faixa das torres, no anel do domo, nas listras da chaminé, nos 2 estandartes e na estrela pintada nas
  torres.
- **Nunca:** cogumelo, verde neon "radioativo", gosma brilhando (é ficção barata; a radiação não brilha verde).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o canhão lasca o concreto das torres (o pedaço curvo cai pra dentro); o fuzil faz faísca no aço dos tubos.
- **A morte (no jogo):** o dano de 400 no raio + a morte de prédio (a explosão grande no meio e 3 menores em ~1,5 s, o chão
  queimado, o entulho).
- **A morte (ideia, em 3 tempos, conferida pela física):**
  1. o domo racha e a tampa salta (a explosão de vapor): o clarão branco-amarelo curto, a onda de choque que levanta a poeira em
     anel até os 26 m (o raio do dano, pro jogador ver de cima o tamanho do perigo);
  2. a nuvem branca de vapor e poeira sobe e abre (volume, iluminada pelo fogo de baixo);
  3. uma das torres racha na cintura e desaba pra dentro; a outra fica de pé, torta, sem vapor. O entulho fica fumegando branco
     por ~20 s.

## 8. O som

- **Seleção (variantes):** o zumbido grave das turbinas; o contador Geiger estalando curto (3 a 4 cliques) e o alto-falante; a
  válvula soltando vapor ("tsssh"); o bip de painel de controle.
- **As falas** (inglês; o técnico-chefe, voz calma, exata, um pouco tensa por baixo):
  - selecionado: "Reactor stable." / "Core temperature nominal." / "Power to the people." / "Ten units online." / "Do not linger
    near the core."
  - ferido: "Coolant failing!" / "Containment breach!" / "Evacuate the area!" / "Core temperature rising!"
  - pronto: "Reactor online." / "Full power."
- **O corpo:** o "uuuum" grave e contínuo das turbinas (o mais grave da base); o vapor rugindo baixo nas torres; o tic-tic do
  Geiger muito raro (1 a cada ~20 s). Ferido: a sirene de duas notas e o Geiger acelerando.
- **A construção:** as fôrmas da torre subindo (o vibrador de concreto, o guincho), as chapas do domo soldadas; no fim, as
  turbinas ganhando rotação (o "uuum" subindo de tom) e o alto-falante: "Reactor online."
- **A morte:** o estouro abafado e enorme (grave, longo, mais "whump" que "bang"), o rugido do vapor saindo, o concreto da torre
  rachando e caindo, o Geiger disparado por 2 s e depois o silêncio com o chiado do vapor.

## 9. Ideias pra depois

- **O SCRAM (desligar de emergência):** um botão no reator ferido: ele para de dar energia na hora e o dano de morte cai de 400 pra
  100 enquanto estiver desligado. Escolha de verdade no meio do ataque: a base no escuro ou a bomba armada.
- **A sobrecarga:** +50% de energia por 60 s, mas o reator perde 2% da vida por segundo enquanto estiver sobrecarregado (a China
  aposta).
- **A cicatriz:** onde o reator explodiu, o chão fica marcado (preto, rachado) pelo resto da partida e o capim não volta (o campo
  que muda, `docs/VISAO_DO_JOGO.md`).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_power_plant` (feito no fim, com o modelo provisório como anexo de forma).
