# Paladin (USA, o pesado)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque pesado: o fim de jogo da USA, o escudo da coluna |
| Custo e tempo | $1 100, 12 s |
| Vida e blindagem | 500, tanque, com ricochete |
| Velocidade | 6,6 m/s (mais lento que o Battle Tank), giro do casco e da torre 180°/s |
| Visão | 39,5 m |
| Arma | o mesmo canhão do Battle Tank: 60 perfurante a cada 2 s, 39,5 m |
| O laser | **no jogo, só a peça que gira** na torre (20 rpm); derrubar míssil é IDEIA (a simulação não tem) |
| Esmagador | nível 2 |
| Requer | War Factory + Strategy Center |
| Sai de | War Factory |

## 2. A ideia

**A cunha com o laser.** O tanque da USA maior e um pouco diferente: ~6% mais comprido, mais largo nas saias e mais alto na torre;
a torre em CUNHA (as duas bochechas em bico apontando pra frente, o mantelete recuado no vão entre elas: de cima, uma ponta de
flecha com o entalhe do canhão) e, no teto de trás, a torrinha redonda do laser de defesa de ponto num pedestal acima de tudo,
com a lente grande. É a unidade reservada (o design 3 de `docs/concept/units/usa_mbt_exploration.webp`).

- **A palavra de cima:** a CUNHA com o círculo do laser no meio das facetas.
- **Inspiração real:** o M1A2 SEPv3 (o casco, as saias, o cesto, a mira do comandante), a blindagem em cunha da torre do Leopard
  2A6 e os protótipos de defesa a laser em veículo (o diretor de feixe numa torrinha própria). **Atenção pro prompt:** "Paladin"
  na vida real é o obuseiro autopropulsado M109A6; o nosso é o tanque do ZH. Nunca deixar o gerador desenhar um obuseiro.
- **Por que existe:** a USA precisa de uma peça que segure a frente no fim de jogo, quando o inimigo tem Overlord e enxame de
  foguete. O laser é a ideia: o tanque que protege o grupo dos mísseis.

## 3. Como funciona

- **No jogo hoje:** é o Battle Tank com 500 de vida (contra 480), mais lento, mais caro e com a mesma arma. Lanchester 0,0124 contra
  0,0178 do Battle Tank (`docs/ELENCO.md` §8.1): **por dólar ele é ~30% pior**; o ganho é "mais casco por vaga de exército".
- **O alerta de designer:** sem o laser na simulação, o Paladin não se paga. No ZH o preço dele era o laser. Ou o laser entra (§9),
  ou o número muda (mais vida, ~650, mantendo o preço), ou ele perde o sentido. É a decisão a tomar antes de ele ir pro teste.
- **Forte contra (hoje):** o mesmo do Battle Tank: tanque, prédio, veículo leve, Tomahawk.
- **Fraco contra:** o ar, o Javelin/RPG/Caçador em grupo, o Overlord (que ganha dele com folga: 69,6/s contra 30/s).
- **O contra dele:** Apache, Overlord, o grupo antitanque. **Ele seria o contra de** (com o laser): o enxame de foguetes e mísseis.

## 4. Em batalha

- **Temperamento: o cavaleiro.** Solene, protetor, calmo como quem já viu muita guerra. Fala de proteger, não de matar.
- **O ritmo:** o do Battle Tank (BUM ... 2 s ... BUM), mais pesado: o casco balança menos no tiro (66 t contra 60).
- **O jogador bom** (com o laser): põe o Paladin na frente da coluna, os Battle Tanks atrás e do lado, o Linebacker colado; o grupo
  avança "debaixo" dele contra a infantaria antitanque.
- **A IA** treina pela mistura da fábrica, depois do Strategy Center.
- **Sozinho:** igual ao tanque; é lento demais pra fugir, então segura e atira.

## 5. A vida

- **Parado:** a turbina em marcha lenta; o laser gira devagar na torrinha (20 rpm), a lente azul refletindo o céu; o sensor do
  comandante varrendo.
- **Andando:** o mesmo pó em volume, a lagarta mais larga (0,68 m) e a marca mais funda.
- **Atirando:** o tiro do tanque; a torrinha do laser **não** acompanha o canhão: ela olha pro céu em volta (é quem vigia).
- **Ferido:** fumaça do motor; com 30% o laser trava parado e a lente apaga (o detalhe que conta que a proteção caiu).
- **Pronto na fábrica:** sai com o laser dando uma volta inteira rápida, como quem testa o sistema.

## 6. O visual

- **De cima:** a ponta de flecha da torre (lê diferente da torre larga e curta do Battle Tank), o disco do laser atrás dela, o cano
  mais comprido (a boca em +6,45 m contra +5,64 m).
- **Assinatura:** a torre em cunha + o laser redondo.
- **Cores:** as do tanque USA (o cinza-claro, a cor do jogador nos detalhes pequenos seguindo as placas, os 2 anéis do cano, a lona
  no motor); a lente do laser azul fosca, nunca acesa como neon.
- **Nunca:** um raio laser contínuo e colorido (a bíblia proíbe raio e neon); o laser maior que a torre; cara de obuseiro.

## 7. Tiro, projétil, impacto, destruição

- **A boca, o projétil e o impacto:** os do Battle Tank (o mesmo canhão, os mesmos aprovados), com a cratera A.
- **A morte:** a de tanque, um pouco maior (66 t: a bola pela raiz cúbica da massa): a torre em cunha voa, a torrinha do laser se
  solta dela e cai à parte com a lente estilhaçada (o detalhe que diz "era o Paladin"). A carcaça queima ~48 s.

## 8. O som

- **Seleção (variantes):** a turbina; o servo do laser (um zumbido fino e agudo, mais "eletrônico" que o da torre); o bipe de
  sistema armado; a escotilha.
- **As falas** (o comandante; voz grave, solene, sem pressa):
  - selecionado: "Paladin, on watch." / "Shield is up." / "Standing guard." / "Command me."
  - andando: "Advancing with the column." / "Moving to cover." / "Stay behind me." / "Forward."
  - atacando: "Engaging." / "Target in sight, firing." / "Hold the line." / "Break their armor."
  - sob fogo: "We hold." / "Armor's taking it." / "Keep them off the column." / "Is that all they've got?"
  - pronto: "Paladin, ready to serve." / "Heavy armor on the field." / "Shield and sword ready." / "Systems hot."
- **O corpo:** a turbina mais grave que a do Battle Tank, a esteira mais larga e lenta, o zumbido do laser girando por cima (fino,
  constante, o "som de tecnologia").
- **A arma:** a do Battle Tank. (Com o laser na simulação: um "tsss" curto e seco de capacitor, quase sem estrondo, e o estalo do
  míssil inimigo morrendo no ar.)
- **A morte:** a de tanque, mais o "estilhaço de vidro" da lente do laser e o zumbido fino caindo de tom até sumir.

## 9. Ideias pra depois

- **O laser de verdade (a razão de ele existir):** na simulação, a defesa de ponto derruba o míssil e o foguete inimigos que
  passarem a até ~15 m dele ou de um aliado perto (Javelin, RPG, Caçador de tanque, os mísseis do Apache e do Raptor, a Patriot,
  o Tomahawk, o ataque de mísseis), um a cada 1,5 s, com o calor limitando (4 seguidos e 6 s pra esfriar). O canhão de tanque passa
  (bala não se intercepta). Vira o contra do enxame, e o grupo que anda com ele muda de jeito.
- **O feixe que só aparece na poeira (o fenômeno real):** o laser de verdade é invisível no ar limpo; ele só aparece onde
  atravessa partícula (a poeira, a fumaça). No jogo, o feixe se desenha só dentro da poeira das lagartas e da fumaça em volume
  (que já existem, o Fluxo), como uma linha fina clara; no ar limpo se vê só o ponto quente no míssil, que brilha, tomba e apaga.
  Lindo, físico e nunca neon.
- **O escudo que se vê:** com o Paladin selecionado, o anel de 15 m da proteção no chão; os aliados dentro ganham um ícone pequeno.
- **A sobrecarga:** gastar o calor todo de uma vez numa rajada (8 interceptações em 3 s) e ficar 15 s sem laser: a jogada contra o
  ataque de mísseis anunciado.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_paladin` (feito no fim, com o modelo provisório como anexo de forma).
