# Scorpion (GLA, o tanque velho com o kit)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque da GLA: barato, rápido, em bando |
| Nome na tela | Scorpion (a chave é `gla_tank`) |
| Custo e tempo | $600, 7 s |
| Vida e blindagem | 370, tanque, **com ricochete** |
| Velocidade | 10,5 m/s (o tanque mais rápido), giro do casco 180°/s, a torre **100°/s** (a mais lenta dos tanques leves) |
| Visão | **32,9 m** (menos que o alcance: precisa de quem veja por ele) |
| Arma | canhão: 20 perfurante a cada 1 s, alcance 39,5 m, a granada a 105 m/s |
| Esmagador | nível 2 |
| Requer | Arms Dealer |

## 2. A ideia

**O tanque soviético velho com o kit da guerrilha.** Um T-55 de 60 anos remendado: sacos de areia no glacis e na torre, gaiola de
barras nas laterais, chapas soldadas que não combinam, galões e caixas nos para-lamas, o pneu de estepe amarrado atrás, a
metralhadora pesada na torre e as bandeiras verdes. Ninguém o fabricou assim; a GLA o montou.

- **A palavra de cima:** a TORRE REDONDA de cúpula (a silhueta soviética) com a meia-lua de sacos de areia.
- **Inspiração real:** o T-55 (e a cópia chinesa, o Type 59): canhão raiado de 100 mm, 4 de tripulação, carregamento à mão, ~36 t.
  Fictício, não cópia: a ficha aprovada `docs/concept/units/gla_mbt_sheet.webp` e o provisório por script aprovado em 13/10
  (`Game/Tools/model_gla_tank.py`).
- **Por que existe:** a GLA precisa de um tanque pra segurar a linha, mas não paga o preço de um. O Scorpion é o mais eficiente por
  dólar e o mais frágil sozinho.

## 3. Como funciona

- **No jogo:** 20 a cada 1 s = **20/s em tanque e prédio**, 2/s em soldado. Mata uma Technical (180) em 9 tiros (8 s), um Battlemaster
  (400) em 20 (19 s), um tanque USA (480) em 24 (23 s).
- **A conta (Lanchester):** 370 × 20 ÷ 600² = **0,0206**, o melhor do jogo (o tanque USA 0,0178, o Battlemaster 0,0188). Sozinho
  perde (o tanque USA o mata em 12 s); 3 Scorpions ($1 800) contra 2 tanques USA ($1 800) ganham.
- **O ricochete (no jogo):** o tiro de canhão de tanque que bate de raspão desvia (a rampa de 74° a 80° da normal; 10% dos tiros de
  frente batem no glacis inclinado e desviam sempre). Só vale entre os cascos que têm o ricochete: o tanque USA, o Paladin e ele.
- **Forte contra:** veículo leve (Technical, Quad, Stryker, coletores), artilharia, prédio, em bando.
- **Fraco contra:** o tanque USA e o Battlemaster no 1 contra 1, o Overlord, a infantaria antitanque (3 RPGs em ~3 s), o ar, ser
  pego de lado com a torre lenta.
- **O contra dele:** o tanque pesado, o Apache, o Tank Hunter. **Ele é o contra de:** os veículos leves e o tanque sozinho cercado
  por 3.

## 4. Em batalha

- **Temperamento: o veterano casca-grossa.** Velho, teimoso, sem frescura. Já viu muita guerra e ainda está aqui.
- **O ritmo:** um tiro por segundo, seco e rápido (o canhão pequeno pra um tanque: ritmo de metralhadora lenta), BUM ... BUM ... BUM.
- **O jogador bom** usa em bando de 5 a 8, cercando (a velocidade de 10,5 m/s deixa ele flanquear antes da torre inimiga girar), com
  uma Technical na frente pra ver (a visão dele é curta).
- **A IA** faz dele o 1º da fábrica (vale por 2 na mistura).
- **Sozinho e ferido:** foge mal (a torre lenta não acompanha); abaixo de 35%, a chama pelo anel da torre.

## 5. A vida

- **No jogo:** a torre gira 100°/s, o cano recua; as esteiras e a poeira das lagartas; a cratera A; os estados de dano (o fio de
  fumaça abaixo de 70%, a chama pelo anel da torre e a fumaça grossa abaixo de 35%); a morte com o jato de fogo pelo anel (`death_jet`).
- **Ideia (parado):** o diesel velho em marcha lenta solta fumaça azulada e preta, irregular; as duas bandeiras (a alta na torre e a
  baixa no para-lama) batem no vento; o holofote grande ao lado do mantelete reflete o sol.
- **Ideia (andando):** os galões e as caixas dos para-lamas balançam e batem; a gaiola de barras chacoalha; o estepe pula.
- **Ideia (atirando):** o casco dá um tranco curto; o pano amarrado no cano chicoteia com o sopro.

## 6. O visual

- **De cima:** o casco de 7,1 × 3,6 m, a cúpula redonda da torre a ~51% do casco com a meia-lua de sacos, o canhão comprido, a gaiola
  nas laterais, o pneu atrás.
- **Assinatura:** a torre redonda soviética com os sacos de areia.
- **Cores:** areia-cáqui fosco com as listras verde-escuras gastas, chapas teal e ferrugem soldadas, sacos de juta; o verde do jogador
  nas 3 faixas (glacis, torre, diagonal), no anel do cano e nas 2 bandeiras.
- **Nunca:** pintura branca ou cinza (confunde com a USA), lataria verde inteira (puxa pra China), torre angulosa.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão médio e curto; a nuvem de fumaça cinza saindo da boca; a poeira levantada na frente.
- **O projétil:** o risco curto e quente, um por segundo.
- **O impacto:**
  - no tanque: a faísca, a marca preta pequena (o tiro dele faz 1/3 do tanque USA: 20 contra 60, e o efeito
    tem de ser menor); o ricochete, a faísca
    que desvia e o "pling";
  - no veículo leve: o furo e a chama;
  - no prédio: o pedaço de reboco;
  - no chão: a cratera A pequena.
- **A morte (no jogo):** o jato de fogo pelo anel da torre por ~3 s (a gravação do EmberGen, `eg_jato_municao`): a munição guardada
  no casco cozinha, a torre salta e o fogo sai em jato vertical; a carcaça fica 48 s queimando e vira sucata ($90 em 7 s).
- **A física conferida:** o T-55 é carregado à mão (não tem carregador automático nem carrossel); a munição fica em estantes no casco
  e na torre, sem painel de alívio: quando o fogo chega nela, a torre pode voar. É isso que o jato conta.

## 8. O som

- **Seleção (variantes):** o diesel V12 velho tentando pegar e pegando com a fumaça (a tossida); a torre girando no manual, com o
  rangido do anel seco; a escotilha de ferro batendo; o rádio chiado com a música no fundo.
- **As falas** (inglês; o comandante veterano, voz rouca, cansada, irônica, de quem já viu tudo):
  - selecionado: "Scorpion." / "Old but angry." / "This tank has seen three wars." / "Still running." / "What now?"
  - andando: "Moving." / "She's slow to start, fast to run." / "Rolling out." / "Flank them."
  - atacando: "Fire!" / "Load! Fire!" / "Hit them again." / "Keep shooting, don't stop."
  - sob fogo: "Armor's old, move!" / "That one went off the side!" (no ricochete) / "We're hit, keep going!"
  - pronto: "Scorpion, rolling."
- **O corpo:** o diesel velho (grave, irregular, com estalos); as esteiras soltas batendo (o clank mais desencontrado do jogo); a
  carga dos para-lamas chacoalhando.
- **A arma:** o estampido médio e seco, mais curto que o do Battlemaster; o carregador manual (a culatra abrindo, o cartucho
  vazio caindo no piso com o "tang" metálico, a nova granada empurrada, o "clack") a cada segundo.
- **A morte:** o estouro, o rugido do jato de fogo (3 s, como um maçarico gigante), a torre caindo de lado.

## 9. Ideias pra depois

- **O kit que cresce:** o Scorpion que desmancha (passa por cima) uma carcaça ganha uma peça a mais: +40 de vida e uma chapa
  soldada a mais na torre, até 3 vezes (lê de perto). O tanque da GLA melhora com a ruína.
- **O foguete na torre:** upgrade no Arms Dealer ($800): o lançador de foguete pequeno em cima da torre (+15 por tiro, a cada 4 s),
  como o kit das guerrilhas reais.
- **O tanque enterrado:** o Scorpion parado pode cavar (o Worker ajuda): o casco fica na terra, só a torre de fora; ele não anda,
  leva 30% menos dano e a visão sobe. A GLA vira defensora com o tanque velho.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_tank` (feito no fim, com o modelo provisório como anexo de forma).
