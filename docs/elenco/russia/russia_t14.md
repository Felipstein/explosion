# T-14 Armata (Rússia, o pesado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque pesado: o fim de jogo da Rússia, o atirador de longe |
| Nome na tela | T-14 Armata (a chave é `russia_t14`) |
| Custo e tempo | $1 800, 18 s |
| Vida e blindagem | 1 050, tanque |
| Velocidade | **7,5 m/s** (o pesado mais rápido: o Overlord 5,3, o Paladin 6,6), giro do casco e da torre 110°/s |
| Visão | 39,5 m |
| Arma | canhão: 100 perfurante a cada 2 s, alcance **52 m** (o canhão de tanque de mais longe do jogo), a granada a 105 m/s |
| Esmagador | nível 2 |
| Requer | Tank Plant + Research Institute |
| Sai de | Tank Plant |
| Estado | nos dados (`Units.json`) e no modelo provisório; fora do menu até os modelos da Rússia serem importados |

## 2. A ideia

**O casco longo e a torre baixa.** O tanque de nova geração: o casco comprido de 7 rodas por lado com a cápsula da tripulação na
frente (as 3 escotilhas em fila), a torre sem gente, baixa e retangular, mais pra trás, o canhão longo com a capa térmica e, só na
parte de trás da torre, a gaiola leve de ripas espaçadas com a rede.

- **A palavra de cima:** o CASCO LONGO com as 3 ESCOTILHAS em fila na frente + a torre-caixote mais pra trás.
- **Inspiração real:** o T-14 Armata (a torre sem tripulação, os 3 tripulantes numa cápsula blindada no casco, o canhão 2A82 de 125
  mm, a proteção ativa Afganit, a blindagem reativa Malakhit). Na escala de verdade: 9 × 3,8 m, 10,8 m com o cano. Maior que o
  T-90M, menor que o Overlord.
- **Por que existe:** o pesado da Rússia não é o martelo lento da China: é o atirador que chega rápido e bate de longe.

## 3. Como funciona

- **No jogo:** 100 perfurante a cada 2 s = **50/s** em tanque e prédio, 5/s em soldado. Mata um tanque USA (480) em 5 tiros (8 s), um
  Battlemaster (400) em 4 (6 s), um Paladin (650) em 7 (12 s), um Overlord (1 100) em 11 (20 s).
- **O duelo com o Overlord:** perde no 1 contra 1 (o Overlord o mata em ~14 s), mas atira de 52 m contra 46 e anda a 7,5 contra 5,3:
  o T-14 bem jogado não deixa o Overlord chegar.
- **A conta de Lanchester** (`docs/ELENCO.md` §8.1): 1 050 × 50 ÷ 1 800² = **0,0162**, a do Paladin (0,0161); o Overlord (0,0191) é o
  superpesado. O alcance e a velocidade são a vantagem dele (a conta não os vê).
- **Forte contra:** tanque e pesado de longe, prédio, a linha que fica parada no alcance dele.
- **Fraco contra:** soldado antitanque em grupo (3 juntos tiram 120/s: ele cai em ~8 s), ar (não atira pra cima), infantaria
  espalhada, artilharia, o Overlord colado. As defesas de base (55–59 m) alcançam mais que ele.
- **O contra dele:** o Apache, o grupo antitanque, o Overlord de perto. **Ele é o contra de:** a linha de tanques, o Paladin, o
  Battlemaster, o Scorpion.

## 4. Em batalha

- **Temperamento: o atirador de elite.** Frio, preciso, distante. Escolhe o alvo, atira e muda de lugar.
- **O ritmo:** um tiro a cada 2 s, mais pesado que o do T-90M (100 contra 60); entre os tiros, o motor manobrando.
- **O jogador bom** o deixa atrás da linha de T-90M, no limite dos 52 m, e recua quando o inimigo avança (o tiro e o passo pra trás);
  nunca deixa o Overlord nem o grupo de RPG chegar perto.
- **A IA** o põe na mistura da fábrica (peso 1) quando tem o Instituto e o dinheiro.
- **Sozinho e ferido:** segue atirando de longe; abaixo de 70%, a fumaça; abaixo de 35%, a chama.

## 5. A vida

- **No jogo:** a torre gira 110°/s e volta pra frente 2 s depois de parar; o cano recua; as esteiras de elos andam (`track_step_m`)
  com a poeira; a cratera A; o raspão (`near_miss` 0,2); os estados de dano do veículo.
- **Ideia (parado):** o motor quase silencioso em marcha lenta; a torre sem gente faz a varredura sozinha, em passos secos (o servo,
  não a mão); os 4 painéis do radar do Afganit nos cantos da torre com a luz fraca.
- **Ideia (andando):** as 7 rodas por lado, a saia de blocos grandes; o casco longo inclina pouco (a suspensão moderna).
- **Ideia (atirando):** o recuo forte; o carregador automático no bustle (o som); sem o fio de fumaça do evacuador (o canhão da torre
  sem gente não tem).

## 6. O visual

- **De cima:** o retângulo comprido (9 × 3,8 m), as 3 escotilhas redondas em fila na frente do casco, a torre-caixote mais pra trás
  com a gaiola leve só atrás, a rede vermelha e a lona vermelha dobrada na gaiola, o cano comprido, o rolo vermelho no fim do deck.
- **Assinatura:** o casco longo com as 3 escotilhas + a torre baixa.
- **Cores:** o aço frio azulado com as manchas grandes pretas e cinza-claras; a cor do jogador na rede, na lona e no rolo; a estrela
  vermelha pequena contornada de branco na lateral da torre (fixa).
- **Nunca:** cano duplo (é do Overlord); a gaiola cobrindo a torre inteira (é a do T-90M: a do T-14 é leve, só atrás); torre alta; Z
  ou V; a faixa branca.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão grande e curto do 125 mm novo; o anel de fumaça; a poeira levantada em leque na frente (o sopro, maior que o do
  T-90M).
- **O projétil:** o risco curto, quente, branco-amarelado (105 m/s no jogo), mais grosso que o dos outros.
- **O impacto:**
  - no tanque: a faísca forte, a chapa que salta, o buraco escuro; o raspão levanta a areia atrás do alvo;
  - no prédio: o pedaço de painel que voa e a poeira;
  - no chão: a cratera A;
  - no soldado: quase nada (10%).
- **A morte (no jogo):** a morte de veículo; a carcaça fica 48 s queimando e afunda em 7 s; sucata ($270 em 15 s) pro construtor.
- **A morte (ideia, a física da cápsula):** a munição fica na torre, longe da tripulação: a torre estoura e abre por cima, e o casco
  fica inteiro com as 3 escotilhas fechadas (a cápsula que segura). A carcaça mais "limpa" do jogo, de propósito.

## 8. O som

- **Seleção (variantes):** o motor diesel novo acelerando (o rosnado mais liso e moderno que o do T-90M); o servo elétrico da torre
  (agudo, preciso, sem rangido); o bip curto do sistema; o rádio digital (mais limpo que o dos outros russos).
- **As falas** (inglês, com o sotaque russo leve; o comandante na cápsula, voz calma, moderna, técnica):
  - selecionado: "Armata ready." / "Crew in the capsule." / "Systems green." / "Long gun ready." / "Give me a target."
  - andando: "Advancing." / "Moving fast." / "Repositioning." / "Keeping the range."
  - atacando: "Target locked." / "Fire." / "Engaging at range." / "Next target."
  - sob fogo: "Taking hits." / "Armor holding." / "Pulling back to range."
  - pronto: "T-14 Armata, ready for battle."
- **O corpo:** o motor grave e liso; as esteiras longas com o clank mais rápido; o servo elétrico da torre (o zumbido curto e preciso).
- **A arma:** o estrondo mais forte do elenco russo, seco, com o eco longo; o carregador automático no bustle (o "clack-zzz-clack"
  rápido).
- **A morte:** o estouro da torre (a munição), o metal se abrindo, o fogo; o casco silencioso.
- **A cara da Rússia:** o metal grave, o motor pesado, a voz baixa e seca.

## 9. Ideias pra depois

- **O Afganit** (a proteção ativa): derruba o 1º míssil antitanque a cada 15 s (o tubo do cinto dispara; de cima, o estouro a 2 m do
  tanque e o míssil caindo). O pesado que o RPG sozinho não abre.
- **A cápsula:** quando o T-14 morre, a tripulação sobrevive; o próximo T-14 sai com a patente do anterior (a Rússia guarda o
  veterano).
- **O radar do Afganit:** revela o soldado antitanque escondido a até 30 m (o radar de canto que vê o míssil chegar vê também quem
  atira).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#russia_t14` (feito no fim, com o modelo provisório como anexo de forma).
