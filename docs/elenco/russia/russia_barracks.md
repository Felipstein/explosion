# Barracks (Rússia, a Kazarma)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o quartel: onde nasce a infantaria da Rússia |
| Nome na tela | Barracks (a Kazarma, a Казарма) |
| Custo e tempo | $500, 10 s |
| Vida e blindagem | 1 000, prédio |
| Energia | 0 |
| Visão | 52,7 m |
| Lote | 16 × 20 m |
| Arma | nenhuma |
| Requer | nada |
| Produz | Motostrelok ($200, 8 s), Kornet Team ($300, 6 s) |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O pátio de formatura com o mastro, na frente do bloco comprido.** A caserna de painel do exército russo: o bloco de 2 andares com
o telhado baixo de chapa verde, o corpo da escada saliente no meio com a estrela contornada no oitão e a porta de 2 folhas sob a
marquise; na frente, o plaç (o pátio de formatura) com as linhas brancas, a tribuna vermelha, o mastro alto e, no canto, a barra
fixa e as paralelas.

- **A palavra de cima:** o PÁTIO DE FORMATURA (o quadriculado branco das posições) com o mastro, na frente do bloco de telhado verde.
- **Inspiração real:** as казармы das unidades russas (o alojamento de painel dos anos 70–80) e o plaç, onde a unidade forma todo
  dia; a tribuna de onde o comandante fala.
- **Por que existe:** a infantaria da Rússia é pouca e firme (o Motostrelok) e tem o antitanque de mais longe do jogo (o Kornet); o
  quartel é barato e não pede nada, então a Rússia tem soldado desde o primeiro minuto.

## 3. Como funciona

- **No jogo:** $500, 10 s, sem energia, sem requisito. Produz o **Motostrelok** (um por clique, $200, 8 s) e o **Kornet Team** ($300,
  6 s). Os soldados saem pela porta da escada (`exit_lane_m` 6,6 m).
- **Forte contra:** nada sozinho; é a fonte da infantaria.
- **Fraco contra:** tudo que mira prédio; tem só 1 000 de vida (um tanque derruba em ~33 s).
- **O contra dele:** o raide de tanque. **Ele é o contra de:** o tanque inimigo que chega sem escolta (o Kornet o pega a 55 m).

## 4. Em batalha

- **Temperamento: o sargento antigo** (o praporshchik). Seco, cansado, rigoroso. Não grita: manda uma vez.
- **O ritmo:** a cada 8 s, um fuzileiro; a contagem curta no pátio.
- **O jogador bom** mistura 3 Motostreloks pra cada Kornet e põe o ponto de encontro atrás dos tanques (o Kornet atira de longe e
  precisa de quem veja por ele).
- **A IA** treina pelo que tem menos, com o Motostrelok (o 1º da lista) valendo por 2.
- **Sozinho e ferido:** a bandeira segue no mastro até o fim.

## 5. A vida

- **No jogo:** o andaime de aço na obra (peça `_Scaffold`); a bandeira lisa na cor do jogador no mastro de 9 m (`flags`); abaixo de
  35%, o fogo nos 3 pontos (`fire_points_m`).
- **Ideia (parado):** de manhã, 4 soldados pequenos em formatura no pátio (só desenho); as janelas acesas em fileira à noite; a
  sentinela no cogumelo; a corda do mastro batendo no vento.
- **Ideia (produzindo):** o soldado sai pela porta de 2 folhas, cruza o pátio em passo de marcha nos 3 primeiros metros e depois
  corre.
- **Ideia (ferido):** o bloco racha pelas juntas dos painéis; a tribuna vermelha segue de pé.

## 6. O visual

- **De cima:** o retângulo do pátio com o quadriculado branco e as cruzes das posições, o bloco comprido de telhado verde atrás, o
  corpo da escada no meio com o oitão e a estrela vermelha, a tribuna vermelha num canto do pátio com o mastro alto, a barra fixa no
  outro canto.
- **Assinatura:** o pátio de formatura com o mastro.
- **Cores:** o painel cinza com as juntas escuras, a chapa verde; o vermelho fixo na tribuna e na estrela do oitão; a cor do jogador
  na bandeira.
- **Provisório:** de cima, o bloco é liso (a Kazarma é a mais simples dos 7 prédios: `docs/elenco/TAREFAS.md` 32).
- **Nunca:** arame farpado de prisão (é quartel); Z ou V; a estrela no telhado (só no oitão e na tribuna).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o canhão arranca um painel da fachada (a janela fica pendurada); a bala lasca a tribuna e risca o concreto.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $50 em ~6,5 s pro construtor).
- **A morte (ideia):** o bloco desaba pelos painéis, o corpo da escada fica de pé um instante e cai pra frente sobre o pátio; o
  mastro cai por último e a bandeira fica no chão, queimando na ponta.

## 8. O som

- **Seleção (variantes):** o apito curto do sargento; as botas batendo juntas (o "tum" da formatura); a contagem curta em russo
  ("raz, dva!"); a corda do mastro batendo no aço.
- **As falas** (inglês, com o sotaque russo leve; o sargento antigo, voz rouca, baixa, sem paciência):
  - selecionado: "Barracks." / "Platoon, attention." / "Who's next for the line?" / "We train, they fight." / "Form up!"
  - produzindo: "Rifleman, out!" / "Kornet crew, move!" / "To the front, quick march."
  - sob ataque: "Barracks under fire!" / "Everyone to the windows!" / "They are shooting at the recruits!"
  - pronto: "Barracks ready." / "First formation at dawn."
- **O corpo:** ao fundo, a contagem distante no pátio, o estalo abafado do estande de tiro, o rádio do posto, o rangido da barra
  fixa.
- **A construção:** os painéis assentados pelo guindaste, a chapa do telhado parafusada; no fim, a bandeira subindo no mastro com a
  corda chiando e o toque curto de corneta.
- **A morte:** os estouros; os painéis caindo em placas; o mastro caindo de lado com o "clang" do aço.
- **A cara da Rússia:** a voz baixa e seca, o concreto grave, a corneta ao longe.

## 9. Ideias pra depois

- **O transporte (o "motorizado" de verdade):** o Motostrelok sem veículo é meio soldado. O BTR (o blindado de rodas de 8 lugares)
  sai da fábrica e leva o pelotão, que atira das escotilhas (o transporte da simulação 35 já existe no Stryker).
- **O Spetsnaz** (estudo `docs/FACCAO_RUSSIA.md` §5.3): no fim da árvore, o atirador com o fuzil longo e o traje ghillie, invisível
  parado; sai daqui.
- **A formatura:** o pátio com 4 soldados parados por 20 s dá a todos a primeira patente (a ordem unida que vira disciplina), e o
  jogador vê o pelotão formado de cima.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_barracks` (feito no fim, com o modelo provisório como anexo de forma).
