# Toxin Tractor (GLA, o pulverizador de veneno)

## 1. Ficha rápida

| | |
|---|---|
| Papel | anti-infantaria em área: limpa soldado agrupado, na rua e atrás da cobertura |
| Custo e tempo | $600, 5 s |
| Vida e blindagem | 240, tanque |
| Velocidade | 7,9 m/s (lento: é trator), giro 120°/s, a barra 180°/s |
| Visão | 39,5 m |
| Arma | jato de veneno: 10 a cada 0,25 s em área de 2,6 m, alcance 26,3 m (só no chão) |
| Esmagador | nível 1 |
| Requer | Arms Dealer |

## 2. A ideia

**O tanque de veneno com a caveira, no trator.** Um trator agrícola velho dos anos 60, o pulverizador de lavoura nos 3 pontos, a
barra de bicos levantada na frente num mastro giratório com o canhão de jato no meio. A coisa mais feia e mais temida da GLA: não
tem blindagem nem pressa, mas onde ela passa ninguém fica de pé.

- **A palavra de cima:** o TANQUE com o losango da caveira pintado em cima + a barra de pulverização aberta.
- **Inspiração real:** os tratores de roda dos anos 60 (o Belarus MTZ e o Massey Ferguson 35: o capô estreito, as rodas de trás
  enormes com as garras em V) com o pulverizador de lavoura. O veneno é ficção do jogo; o desenho da nuvem segue a física de um gás
  agressivo real: o cloro, por exemplo, é amarelo-esverdeado e **mais pesado que o ar** (desce e se junta no baixo).
- **Por que existe:** a GLA vence na emboscada e na vila, mas a infantaria inimiga escondida na ruína e na carcaça (a cobertura
  corta a bala pela metade) é dura de tirar. O veneno não respeita cobertura de bala.

## 3. Como funciona

- **No jogo:** 10 a cada 0,25 s = **40/s em cada soldado** a até 2,6 m do ponto (veneno 100% no soldado), **10/s em tanque** (25%),
  **0,4/s em prédio** (1%), **nada no ar** (0%). Mata um Red Guard (120) em ~3 s, e o par inteiro junto (os dois ficam no raio).
- **A cobertura (no jogo):** atrás da carcaça ou do entulho, a bala faz 50%; o veneno, como todo dano que não é bala, faz 70%: ele
  entra bem atrás da cobertura.
- **Forte contra:** infantaria agrupada (o par de Red Guard, o grupo de RPG, os Rangers), soldado na cobertura, o Worker e o Dozer
  (10/s: o Dozer em 25 s).
- **Fraco contra:** tudo com canhão (um tanque o mata em 6 s), a Technical e a Quad (atiram de 39,5 m: mais longe que os 26 m dele),
  ar, prédio (0,4/s: não serve contra prédio).
- **O contra dele:** os veículos com alcance maior e o tanque. **Ele é o contra de:** a horda de infantaria da China, os Rangers
  entrincheirados, o soldado antitanque escondido.

## 4. Em batalha

- **Temperamento: o sinistro.** Lento, silencioso, frio. Não grita; chega devagar e o silêncio é o aviso.
- **O ritmo:** o chiado contínuo da bomba, o jato que varre de um lado pro outro, a tosse dos soldados.
- **O jogador bom** o leva atrás dos Scorpions até a infantaria inimiga se juntar (no entulho, na carcaça, em volta dos tanques) e
  então entra; nunca na frente de um tanque.
- **A IA** o põe na mistura da fábrica pelo que falta.
- **Sozinho e ferido:** devagar demais pra fugir; abaixo de 35%, o tanque de veneno começa a vazar (ideia).

## 5. A vida

- **No jogo:** a barra de pulverização gira 180°/s pro alvo (peça `_Turret`, com a flâmula no alto do mastro); as 4 rodas (as de trás
  grandes, garras em V), a poeira, a marca; a morte de gasolina (escala 0,5).
- **Ideia (parado):** o escapamento em pé solta fumaça escura com a tampinha batendo; a bomba do pulverizador tique-taqueia; os panos
  amarrados nas pontas da barra balançam.
- **Ideia (atirando):** o jato verde-amarelado sai do canhão do meio e a névoa fina dos bicos da barra; a nuvem (volume, na luz do
  mundo, recebendo e fazendo sombra) desce, rasteja e se acumula no baixo e dentro das ruínas, some em ~4 s; o chão fica com uma mancha
  amarelada por ~10 s.
- **Ideia (ferido):** um furo no tanque solta um fio de névoa atrás dele (o rastro mostra por onde ele passou).

## 6. O visual

- **De cima:** o trator de ~5,4 m com as rodas de trás grandes, o tanque deitado com o losango da caveira e dos ossos pintado em cima,
  a barra de 3 m aberta na frente.
- **Assinatura:** o tanque com o símbolo de veneno.
- **Cores:** o amarelo-areia fosco com a listra verde-escura no capô e nos para-lamas, o tanque verde-oliva amarelado, o tambor de
  ferrugem; o verde do jogador na bandeira do berço, na faixa torta em volta do tanque, nos rabiscos e nos panos da barra.
- **Nunca:** gosma verde neon brilhando (o gás não brilha); caveira enorme de desenho animado; cruz vermelha.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** sem clarão; o jato de pressão (o "pssssh") e a névoa que abre em leque.
- **O projétil:** a nuvem: não é bala, é área. Verde-amarelada pálida, densa no meio, rasgada pelo vento do mapa, pesada (desce).
- **O impacto:**
  - no soldado: ele leva a mão ao rosto, tosse, cai de joelhos (a morte animada); a nuvem fica em volta;
  - no tanque: a névoa escorre pela chapa (25%);
  - no prédio: nada (1%);
  - no chão: a mancha amarelada, o capim que amarela e morre (o campo que muda, ideia).
- **A morte (no jogo):** a morte de gasolina (escala 0,5), a carcaça 48 s, sucata ($90 em 7 s).
- **A morte (ideia):** o tanque rompe e solta a nuvem grande de veneno (2 s de dano no raio de 5 m, de qualquer dono: a GLA também
  morre perto do próprio trator); depois o fogo da gasolina.

## 8. O som

- **Seleção (variantes):** a bomba do pulverizador dando pressão (o "tchk-tchk" de bomba de pistão); o motor de trator velho de um
  cilindro grande (o "pop-pop-pop" lento); a válvula soltando ar; a respiração dentro de uma máscara de gás.
- **As falas** (inglês; o operador, voz abafada pela máscara de gás, lenta, calma demais, sinistra):
  - selecionado: "Toxin tractor." / "Breathe deep." / "The air is mine." / "Nothing grows where I pass." / "Tank is full."
  - andando: "Slow and sure." / "Moving in." / "Downwind." / "Approaching."
  - atacando: "Spray them." / "Let the wind carry it." / "Clear the streets." / "Into the ruins."
  - sob fogo: "Don't hit the tank!" / "We're leaking!" / "Pull back, pull back."
  - pronto: "Toxin tractor, ready."
- **O corpo:** o motor de trator (lento, grave, cada explosão do pistão separada); a bomba de pistão; as rodas de garra na areia.
- **A arma:** o jato de pressão (o chiado alto e contínuo), a névoa sibilando; ao longe, a tosse.
- **A morte:** o tanque rompendo (o "pfoooosh" do gás saindo), depois o estouro da gasolina.

## 9. Ideias pra depois

- **O gás que fica:** a nuvem fica 6 s no chão e escorre pro baixo (a física do gás pesado): o vale e a trincheira viram armadilha; o
  vento do mapa empurra a nuvem (a mesma direção da bandeira da Technical).
- **O veneno na água:** o trator parado num poço ou num depósito de suprimento do mapa envenena ele: o coletor inimigo que carrega ali
  perde 25% da vida por viagem por 60 s.
- **O filtro:** a resposta ao veneno: o upgrade de máscara no quartel da China e da USA corta o veneno pela metade nos soldados (o
  contra do contra).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_toxin_tractor` (feito no fim, com o modelo provisório como anexo de forma).
