# Stinger Site (GLA, o ninho de mísseis)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a defesa de base da GLA: antiaérea e contra veículo leve |
| Custo e tempo | $900, 15 s (a defesa mais barata do jogo) |
| Vida e blindagem | 1 000, prédio |
| Energia | nenhuma (funciona sempre) |
| Visão | 94,8 m |
| Lote | 8 × 8 m |
| Arma (chão) | míssil: 20 explosivo a cada 2 s, alcance 59,2 m |
| Arma (ar) | míssil: 30 explosivo a cada 2 s, alcance 105 m, estilhaço de 1,3 m |
| Torre | gira 180°/s |
| Requer | Barracks |
| Conta pra vitória | não |

## 2. A ideia

**O ninho redondo de sacos de areia com os lança-mísseis.** Um buraco cavado na berma, o anel de sacos, a rede esticada em paus
por cima da metade de trás, e no meio o suporte giratório improvisado com 3 tubos, o assento de trator e o guidão.

- **A palavra de cima:** o ANEL de sacos (o círculo) com os 3 tubos verde-oliva apontados pra fora.
- **Inspiração real:** os ninhos de Stinger das guerrilhas afegãs dos anos 80 (o círculo de sacos numa berma de terra, a rede, as
  caixas compridas dos mísseis) e os suportes improvisados de vários tubos. O míssil de verdade (o FIM-92) é de ombro, guiado por
  calor; o tubo do jogo é maior que o real (2,45 m contra ~1,5 m) pra ler de cima.
- **Por que existe:** a GLA não tem ar. O Stinger Site é a resposta barata e sem energia ao helicóptero e ao avião, e segura a
  Technical inimiga.

## 3. Como funciona

- **No jogo, no ar:** 30 explosivo a cada 2 s, a 105 m: 15/s no avião, **19,5/s no helicóptero** (explosivo 130%), 7,5/s no Chinook
  (50%). O Raptor (160) e o Apache (220) caem com 6 mísseis (~10 s); o Chinook com 20.
- **No jogo, no chão:** 20 explosivo a cada 2 s, a 59 m: 10/s em tudo (o explosivo faz 100% em soldado, tanque e prédio). A
  Technical (180) cai em 9 mísseis (~16 s); ela faz 25/s no prédio e levaria 40 s pra derrubar o ninho.
- **O míssil:** sai a 20 m/s, acelera até 110 m/s, curva 200–240°/s (o mais ágil do jogo), sobe a 20° no chão e a 50° no ar.
- **Forte contra:** helicóptero, avião baixo, veículo leve, soldado sozinho (10/s), tudo isso sem energia.
- **Fraco contra:** tanque (um tanque USA o derruba em 33 s e leva 46 s pra morrer dele), artilharia (Inferno a 79 m, Tomahawk a 85
  m), grupo grande de infantaria (o tiro lento: 1 a cada 2 s).
- **O contra dele:** o tanque e a artilharia. **Ele é o contra de:** o Apache, o Raptor, a Technical e o Stryker no raide.

## 4. Em batalha

- **Temperamento: o atirador de tocaia.** Paciente, quieto, letal. Espera o helicóptero passar baixo; não perde a chance.
- **O ritmo:** o "pop" do lançamento, o rugido do motor, o estouro no alto; 2 s de silêncio; de novo.
- **O jogador bom** os espalha (são baratos e sem energia) cobrindo o Stash e o CC, e põe um na rota do helicóptero inimigo, longe da
  base, como surpresa.
- **A IA** espera o quartel (o pré-requisito) e põe a defesa na lista sem travar.
- **Sozinho e ferido:** atira até o fim; os sacos rasgam e a areia escorre.

## 5. A vida

- **No jogo:** o suporte gira 180°/s e os tubos apontam pro alvo (peça `_Turret`); o míssil modelado em voo com o rastro
  (`missile_model`, 1,52 m); o andaime de madeira na obra; abaixo de 35%, o fogo e os buracos.
- **Ideia (parado):** a rede de camuflagem ondula; a bandeira verde no muro de sacos bate; o suporte faz uma varredura lenta pro céu.
- **Ideia (travando no ar):** o tubo sobe a 50° e um tom agudo e crescente (o "growl" do buscador de calor que trava no alvo) toca
  antes do disparo.
- **Ideia (recarregando):** um tubo vazio é trocado por outro das caixas do chão (mãos, só desenho; o operador não existe no
  modelo).

## 6. O visual

- **De cima:** o círculo de sacos (1,6 m) com a mancha da rede na metade de trás, os 3 tubos apontados pra fora, a bandeira verde.
- **Assinatura:** os 3 tubos no suporte giratório.
- **Cores:** sacos de juta, terra, rede mosqueada, tubos verde-oliva; o verde do jogador na bandeira e no rabisco do escudo.
- **Nunca:** concreto (é terra e saco); radar.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o "pop" do motor de ejeção (a nuvem curta e branca na boca), o míssil cai um pouco e o motor de voo acende alguns
  metros depois (como no lançador de ombro de verdade): a chama branca e o rastro grosso; o sopro de trás levanta a areia atrás do
  ninho.
- **O projétil:** o rastro de fumaça branca grossa e curva (o míssil manobra forte), que fica no ar uns 2 s e mostra de onde veio.
- **O impacto:**
  - no ar: o estouro de estilhaço em volta da aeronave (bola laranja pequena e a nuvem de fragmentos), a fumaça preta do motor;
  - no veículo: o estouro e a faísca; na Technical, a bola de gasolina;
  - no soldado: o estouro no chão e a poeira;
  - no prédio: o buraco e o pó.
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho).
- **A morte (ideia):** os mísseis das caixas pegam fogo e saem tortos pra todo lado, girando no chão (uma cena de 2 s, sem dano); os
  sacos rasgam e a areia escorre.

## 8. O som

- **Seleção (variantes):** o tom agudo do buscador ligando ("bzzzt" fino); o tubo batendo no suporte; o rádio chiado; a lona da rede.
- **As falas** (inglês; o atirador, voz baixa, paciente, satisfeita no acerto):
  - selecionado: "Stinger ready." / "Watching the sky." / "Let them fly low." / "Eyes up." / "Nothing passes."
  - atacando: "Missile away!" / "Locked on!" / "Bird down!" / "Another one."
  - sob fogo: "They're shelling the nest!" / "Hold the position!" / "Sandbags won't last!"
  - pronto: "Stinger site ready."
- **O corpo:** o vento na rede; o rádio; o rangido do suporte girando.
- **A arma:** o tom do buscador subindo e travando (o "growl" contínuo), o "pop" do motor de ejeção, um instante de silêncio, o
  rugido do motor de voo indo embora com o efeito Doppler; o estouro seco no alto.
- **A construção:** as pás, os sacos jogados (o "fluff" grave), o suporte aparafusado; no fim, o buscador testado (o tom curto).
- **A morte:** os estouros; os mísseis chiando no chão; a areia escorrendo.

## 9. Ideias pra depois

- **O ninho que muda de lugar:** o Worker desmonta e remonta o ninho em outro ponto em 10 s (a defesa móvel da guerrilha), sem
  perder o dinheiro.
- **O ninho escondido:** parado sem atirar por 20 s, ele some da vista do inimigo até o próximo disparo (a rede e a berma), e o
  primeiro míssil da tocaia faz 50% a mais. O helicóptero inimigo aprende a ter medo de cada ruína.
- **O ninho na ruína:** construído em cima do entulho de um prédio, ganha +500 de vida (o concreto quebrado como muralha). A GLA usa a
  ruína do mapa como fortaleza.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_stinger_site` (feito no fim, com o modelo provisório como anexo de forma).
