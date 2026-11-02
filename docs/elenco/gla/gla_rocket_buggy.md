# Rocket Buggy (GLA, o enxame de foguetes)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a artilharia rápida da GLA (o "pesado" dela é o enxame) |
| Custo e tempo | $900, 10 s |
| Vida e blindagem | **120**, tanque (a casca mais fina do jogo, junto com o Inferno) |
| Velocidade | **23,7 m/s** (o mais rápido do chão, junto com a Technical), giro 180°/s, o lançador 180°/s |
| Visão | 39,5 m (atira a 79: precisa de quem veja) |
| Arma | salva de **6 foguetes** de 20 explosivo, um a cada 0,25 s, depois 6 s pra recarregar; de 13,2 a 79 m; estilhaço cheio até 1,3 m, borda a 2,6 m |
| Esmagador | **nenhum** (não passa por cima nem de soldado) |
| Requer | Arms Dealer + Palace |

## 2. A ideia

**O buggy com os 2 casulos de foguete.** Um buggy de areia de tubo, o motor à mostra atrás, as rodas de trás grandes com as pás, e
em cima, num garfo giratório, dois casulos de 6 tubos. Chega a ~85 km/h, para, despeja 6 foguetes em 1,5 s e some antes do primeiro
cair. Sozinho é um mosquito; em 6, é uma tempestade.

- **A palavra de cima:** os DOIS CASULOS de tubos (3 × 2 cada) levantados no alto, atrás.
- **Inspiração real:** o buggy de areia de tubo (o "sand rail", com o motor boxer à mostra) e os lançadores de foguete de 107 mm de
  12 tubos que as guerrilhas soldam em cima de qualquer coisa que anda (o Type 63 chinês é o mais comum). Fictício, não cópia.
- **Por que existe:** a GLA não tem tanque pesado. O Rocket Buggy é o martelo dela: dano grande, de longe, com a velocidade pra
  escolher a hora.

## 3. Como funciona

- **No jogo:** 6 × 20 = **120 por salva**, a cada ~7,25 s (6 foguetes em 1,25 s + 6 s): ~16,6/s, **em tudo igual** (o explosivo faz
  100% em soldado, tanque e prédio). 4 buggies tiram 480 numa salva: **um tanque USA inteiro**. Contra soldado, o estilhaço de 1,3 a
  2,6 m pega o grupo.
- **O foguete:** sai a 25 m/s, acelera até 60, curva só 60°/s, espalha 8°, sobe a 40° (arco), vive 4,5 s: contra alvo parado
  (prédio, defesa, tanque parado) acerta quase tudo; contra alvo andando, erra muito. A 79 m, o voo leva ~2 s.
- **Não esmaga e não chega perto:** abaixo de 13,2 m não atira.
- **Forte contra:** defesa parada (Gattling Cannon, Patriot), prédio, tanque parado, infantaria agrupada, artilharia inimiga.
- **Fraco contra:** o que bate em veículo e o alcança (120 de vida: um tanque o mata em 2 tiros, a Quad Cannon em ~5 s, a Technical
  em ~10 s, o Stinger Site em 6 mísseis, o RPG em 3 foguetes), o ar, o alvo que anda. A blindagem de tanque o protege do gattling e
  do fuzil (a Gattling Cannon leva ~30 s, o par de Red Guards ~16 s).
- **O contra dele:** a Technical, a Quad Cannon, o tanque que o pega, o Raptor, o Apache. **Ele é o contra de:** as defesas, o
  Overlord parado, o Inferno Cannon, a base inteira quando em bando.

## 4. Em batalha

- **Temperamento: o vespeiro.** Nervoso, imprevisível, covarde e letal. Chega, pica e foge; nunca fica.
- **O ritmo:** a arrancada, a freada, o "fsht-fsht-fsht-fsht-fsht-fsht" em 1,5 s, a arrancada de novo; 6 s de silêncio de fuga; de
  novo.
- **O jogador bom** usa 4 a 8 juntos, com uma Technical na frente pra ver; atira e já manda andar (bate e corre); mira prédio e
  defesa, nunca o tanque que está se mexendo.
- **A IA** o trata como artilharia (tem alcance mínimo): 1 + as defesas inimigas que conhece, com 2 tanques de escolta por lançador.
- **Sozinho e ferido:** morre em segundos; a velocidade é a única defesa.

## 5. A vida

- **No jogo:** o lançador gira 180°/s pro alvo (peça `_Turret`, com a bandeirinha do casulo girando junto); os 6 foguetes modelados em
  voo com o rastro (`missile_model`); as rodas, a poeira (a mais alta: é rápido), a marca; a morte de gasolina (escala 0,35).
- **Ideia (parado):** o motor boxer à mostra trepidando; o chicote (a antena alta de buggy de duna) balançando com a flâmula.
- **Ideia (andando):** a suspensão de braços longos trabalhando à vista (as molas comprimem); nas dunas, as rodas da frente saem do
  chão; a poeira em leque das pás de trás.
- **Ideia (atirando):** o buggy freia de lado, os casulos sobem a 40°, os 6 foguetes saem alternando os casulos (esquerda, direita), o
  sopro levanta uma nuvem de poeira atrás e cobre o buggy (volume); ele sai da própria nuvem acelerando.
- **Ideia (recarregando):** os tubos vazios soltam fumaça fina pelos 6 s.

## 6. O visual

- **De cima:** o buggy de ~4,6 × 2,3 m, a gaiola de tubos, as rodas de trás grandes, os 2 casulos no alto atrás, o chicote.
- **Assinatura:** os 2 casulos de foguete sobre o buggy.
- **Cores:** os tubos amarelo-areia, o bico com as listras verde-escuras, o escape de ferrugem; o verde do jogador na flâmula do
  chicote, na bandeirinha do casulo, na faixa à mão no bico, nos rabiscos e no pano amarrado.
- **Nunca:** blindagem; carroceria fechada (é tubo e motor à mostra).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** 6 clarões curtos, a chama de cada foguete saindo do tubo, a nuvem de poeira e fumaça atrás (o sopro).
- **O projétil:** 6 foguetes com a chama laranja e o rastro de fumaça cinza fino (`trail_width` 0,2), em arco, espalhados, um pouco
  tortos (8°): o enxame se lê pelos 6 rastros curvos no céu.
- **O impacto:**
  - no prédio: 6 estouros em 1,5 s, cada um com o clarão, a poeira e o pedaço que voa: o "tapete";
  - no tanque: as bolas de fogo em volta e em cima, faísca;
  - no soldado: os estouros no chão levantando areia, o grupo inteiro atingido;
  - no chão: 6 marcas pretas pequenas espalhadas.
- **A morte (no jogo):** a morte de gasolina (escala 0,35, pequena); a carcaça 48 s; sucata ($135 em 9 s).
- **A morte (ideia):** os foguetes que sobraram nos tubos disparam soltos e tortos (rastros girando pelo chão por 1 s, sem dano); o
  buggy capota.

## 8. O som

- **Seleção (variantes):** o motor boxer acelerando (o ronco de "fusca" envenenado, agudo e rasgado); o garfo do lançador girando
  com o motorzinho elétrico; os foguetes batendo dentro dos tubos; o grito de euforia.
- **As falas** (inglês; o piloto, voz jovem, rápida, maníaca, rindo no meio da frase):
  - selecionado: "Rocket buggy!" / "Fast and loud!" / "Pick a target, any target!" / "Twelve tubes, six ready!" / "Let's go
    hunting!"
  - andando: "Floor it!" / "Too fast to hit!" / "Weee-ha, moving!" / "Shortcut through the dunes!"
  - atacando: "Rockets away!" / "Rain on them!" / "Fire, fire, fire!" / "Hit and run!"
  - sob fogo: "Too hot! Run!" / "I'm made of tubes, get me out!" / "Back, back, back!"
  - pronto: "Rocket buggy, ready to fly."
- **O corpo:** o boxer gritando; a suspensão batendo; as pás jogando areia.
- **A arma:** 6 "fsht" rápidos em 1,25 s (cada foguete um chiado com a ignição seca), os 6 rastros assobiando no ar; a 2 s, os 6
  estouros secos e médios, em cascata; depois, o silêncio dos 6 s de recarga com o clique dos tubos.
- **A morte:** a gasolina (curto, agudo), os foguetes soltos chiando.

## 9. Ideias pra depois

- **A salva concentrada:** 3 ou mais buggies mirando o mesmo ponto sincronizam a salva (o tapete sai junto): o estilhaço do centro
  sobe 30% (os foguetes se somam).
- **O foguete de sucata:** sem o Palace, o Arms Dealer faz uma versão pior (3 foguetes, sem curva) com os tubos tirados das carcaças:
  a GLA tem artilharia cedo, ruim e barata.
- **O salto da duna:** a 23 m/s, o buggy salta as dunas e o barranco (1 a 2 s no ar, sem atirar): o momento bonito da câmera deitada e
  uma fuga que o tanque não segue.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_rocket_buggy` (feito no fim, com o modelo provisório como anexo de forma).
