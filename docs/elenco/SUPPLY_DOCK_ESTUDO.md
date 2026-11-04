# Supply Dock: o estudo das variações (item 63, 03/11/2026)

O pedido (03/11): "refaça variações do supply dock, onde pegamos dinheiro, esse precisa ser muito bem legal, investido, porque o
outro foi feito como se fosse um jogo de lego, feio, sem criatividade [...] variações tops que fazem sentido, tanto pra mapa quanto
pra tamanhos".

## O que já foi reprovado (14/10, `docs/APROVADOS.md`)
- o monte de caixas dos game shots ("simples demais");
- a zona de lançamento aéreo e o avião que repõe ("vai fuder a mecânica": o depósito é finito e já começa no mapa);
- o pátio de contêineres ("ridículo, não lê como suprimento");
- a pirâmide de caixas ("já reprovei").
O que ele quer: "uma ideia poderosa, que encaixa bem com cenários, de arrepiar, pensar fora da caixinha", entendível e icônica como
no ZH, com o estoque finito e visível.

## O ZH (os arquivos, `docs/ECONOMIA.md` §4)
Quatro tamanhos, todos imortais:

| Objeto | Caixas | Some ao esvaziar? |
|---|---|---|
| SupplyDock | 400 (30 000 $) | não |
| SupplyWarehouse | 400 | não |
| SupplyPile | 150 (11 250 $) | sim |
| SupplyPileSmall | 50 (3 750 $) | sim |

A pilha encolhe no desenho (`W3DSupplyDraw.cpp`): o jogador conta o dinheiro pelas caixas. É isso que fica.

## Os outros RTS (a fonte finita que todo mundo lembra)
- **Warcraft III, a mina de ouro:** uma palavra (a boca da mina na rocha); finita; desaba em ruína quando acaba. O lugar conta a
  história do recurso.
- **StarCraft II, os cristais:** finitos; encolhem em 4 estágios à vista. A leitura do estoque é a forma.
- **Age of Empires, o ouro e a pedra:** o monte diminui. Sem história, só o monte (é o que ele reprovou em nós).
- **Red Alert / C&C 3:** o minério e o Tiberium voltam a crescer (proibido aqui: o nosso não repõe).

## A ideia: "a carga que a guerra deixou"
Cada depósito é o veículo ou o lugar que trazia suprimento e ficou parado no campo de batalha, cheio das caixas do C&C (olive, a
faixa amarela na tampa). Ele explica por que o dinheiro está ali (o mapa conta a história), muda com o tipo de mapa e nunca repõe:
o navio não volta, o trem não sai, o avião não decola. As caixas são o estoque e somem à vista, as de cima primeiro (o
`ARTSDockActor` de hoje faz isso com qualquer lista de vagas). Nada de contêiner: a carga é sempre a caixa.

**Por tipo de mapa (o grande, 400 caixas):**
1. **O cargueiro encalhado** (litoral): o navio pequeno de carga geral, enferrujado, encalhado na praia e adernado, os porões abertos
   cheios de caixas, as lanças de carga baixando um palete na areia. Real: os navios encalhados de Nouadhibou e do mar de Aral.
2. **O trem parado** (mapa com ferrovia): a locomotiva diesel velha e as pranchas carregadas, a lona meio puxada, a plataforma de
   concreto onde os caminhões encostam. Real: os trens de suprimento parados nas linhas da guerra (Ucrânia, Bálcãs).
3. **O cargueiro aéreo caído** (deserto aberto): o quadrimotor de transporte pousado de barriga, uma asa quebrada, a rampa aberta com
   as caixas espalhadas em leque, o sulco na areia atrás. Real: os C-130 e Il-76 perdidos em pistas de terra.
4. **O pátio do pórtico** (qualquer mapa, o padrão): o pátio militar de concreto com o pórtico amarelo sobre trilhos passando por
   cima de 3 fileiras de caixas, um palete no gancho. De cima: a barra amarela atravessando o monte. É o "armazém" do ZH.

**Os tamanhos (depois que ele escolher a família):** cada família em 3 tamanhos, como o ZH:
- pequeno (50 caixas, o SupplyPileSmall): o barco de pesca / o vagão solto / o helicóptero caído / o caminhão tombado;
- médio (150): o barco de desembarque / 1 locomotiva e 1 prancha / o bimotor / o comboio parado;
- grande (400): os 4 acima.

## As regras
- **Jogabilidade:** o que é alto não tapa unidade (o casco baixo, adernado e afundado na areia; o pórtico vazado); o lado da fila
  (a faixa de terra batida por onde os caminhões entram numa ponta e saem na outra) livre e visível.
- **Tamanho:** o depósito de hoje ocupa 26,33 × 32,12 m (`WorldGenerator.cpp`); a simulação já guarda a meia-largura de cada um
  (`SupplyDockSetup::HalfX/HalfY`), então cada variação pode ter o seu tamanho.
- **Neutro:** sem cor de facção, sem bandeira, sem logo; números de estêncil gastos.
- **Vivo:** o gancho que sobe e desce quando o caminhão carrega, a lona batendo, as gaivotas no navio, a fumaça fina do motor quebrado.

## A ordem
1. A folha das 4 famílias (GPT; o Gemini está sem cota até ele subir o teto), ele escolhe.
2. A folha dos tamanhos da família escolhida.
3. A ficha, o modelo, as vagas das caixas (`pile_m` por variação), o jogo e o WorldForge (qual variação por tipo de mapa).

## 03/11, o que ele disse da 1ª folha
"as ideias do supply são top, pode guardar elas, mas como nosso mapa não tem mar, nem trem, eles não vão ser usados dentro do mapa,
porém quero pro jogo, top demais, mas foca no que dá pra usar [...] temos vilas no nosso mapa, então também tem que ter outra
variação de supply mais básico, mais parecido com C&C ZH também, e mais parecido com outros rts".
- As 4 famílias ficam guardadas (`docs/APROVADOS.md`, a imagem em `docs/aprovados/modelos/2026-11-03_supply_dock_4_familias_GUARDADAS.jpg`)
  pros mapas de litoral e com ferrovia. O avião caído e o pórtico cabem no deserto de hoje, mas vêm depois do básico.
- Agora: o Supply básico (a folha 161), os 4 tamanhos do ZH na linguagem da vila: a pilha pequena (50), a pilha (150), o armazém da
  vila (400) e a doca clássica (400).

## 03/11 noite: no jogo (decisão minha, com a delegação dele)
- Os modelos (um agente): `Game/Tools/model_supply_pile_small.py`, `model_supply_pile.py`, `model_supply_dock_v2.py`,
  `model_supply_warehouse.py`, as peças em comum em `model_supply_basic_parts.py` e a caixa do estoque em `model_supply_stock_crate.py`.
  A folha: `captures/game/models/supply_folha.jpg`.
- **O formato:** cada depósito do `village.json` (o `supply_docks` do WorldForge) ganha `"kind"`: `pile_small` (50 caixas), `pile`
  (150), `dock` (400) ou `warehouse` (400, o armazém da vila); sem `kind`, a doca. O `Units.json` tem o desenho de cada uma em
  `supply_dock_visuals`:
  - `stock_m`: as caixas do estoque, com o giro, na ordem em que somem;
  - `sim_rect_m`: a área que a simulação bloqueia, deslocada no desenho; a faixa dos caminhões em +X fica livre;
  - `boxes`: as caixas quando o mapa não diz.
  O `yaw_deg` (em quartos de volta) gira o desenho e a área juntos. O código: `RTSGameData` (`DockVisuals`), `RTSDockActor`
  (o estoque modelado) e `RTSMatchSubsystem` (o `kind` e o giro de cada depósito; o cliente da LAN lê o mesmo arquivo).
- **Nos mapas:**
  - no `World_TestSalt2`: a doca nas bases (o do jogador 1 girado 180°, a faixa virada pra base) e o armazém nos 4 das mini vilas;
  - no `World_TestHills` (o de teste): uma de cada, a cena `supply_variacoes`.
- **Pro dono do WorldForge:** o gerador passa a escrever o `kind` (e as caixas de cada um): a doca perto das bases, o armazém
  dentro da mini vila, as pilhas soltas no campo como as do ZH (`SupplyPile` e `SupplyPileSmall`). Hoje o `kind` foi escrito à mão
  nos 2 arquivos; gerar a vila de novo apaga.
