# Battle Tank (USA, o tanque de batalha)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o tanque básico: o corpo do exército da USA |
| Custo e tempo | $900, 10 s |
| Vida e blindagem | 480, tanque, **com ricochete** (o tiro de raspão de outro tanque tira só 25%) |
| Velocidade | 7,9 m/s; giro do casco e da torre 180°/s (o mais ágil dos tanques) |
| Visão | 39,5 m |
| Arma | canhão: 60 perfurante a cada 2 s, alcance 39,5 m, o projétil a 105 m/s |
| Esmagador | nível 2: passa por cima de soldado, carro civil e palmeira |
| Requer | War Factory |
| Sai de | War Factory |

## 2. A ideia

**O tanque claro de torre facetada.** Casco comprido e baixo, a torre larga e curta em placas anguladas, a caixa do sensor com a
lente azul em cima, os lançadores de defesa ativa nas bochechas, as antenas, o cesto atrás da torre com a lona azul amarrada no
motor e a faixa azul segmentada nas saias. É a ficha aprovada (`docs/concept/units/usa_mbt_player_color.webp`): a primeira peça
do jogo e a régua de todas.

- **A palavra de cima:** a TORRE larga e facetada (a silhueta que separa a USA sem cor: a China tem a torre comprida em escudo, a
  GLA a cúpula redonda).
- **Inspiração real:** o M1A2 Abrams (o casco, a torre de blindagem plana, a turbina a gás, o canhão de 120 mm de alma lisa, os 4
  tripulantes com o municiador, a munição atrás da torre com os painéis de alívio no teto). O ZH chamava de Crusader; os números
  são dele.
- **Por que existe:** a USA é "pouco, caro e preciso". O tanque dela é o mais caro e o mais ágil do jogo: gira rápido, ricocheteia,
  e vive mais por saber mirar primeiro do que por aguentar.

## 3. Como funciona

- **No jogo:** 30 de dano por segundo no tanque e no prédio, 3/s no soldado (o perfurante entra 10%). Mata outro Battle Tank em 8
  tiros (~14 s), um Type 99 (400) em 7 (~12 s), um Scorpion (370) em 7.
- **O ricochete (decidido em 30/10):** só entre tanques; a chance pelo ângulo do tiro na face (de frente, ~1 em 8–12); tira 25% do
  dano e tem de se ver (o leque raso de faíscas e o traçante subindo torto).
- **O esmagamento:** o soldado inimigo no caminho dele andando morre na hora (200%: o esmagamento é o que o tanque tem contra a
  infantaria, já que o canhão quase não fere).
- **Lanchester** (`docs/ELENCO.md` §8.1): 0,0178, dentro dos ±15% do Type 99 (0,0188) e do Scorpion (0,0206).
- **Forte contra:** outro tanque, o Tomahawk (2 tanques matam um em 2,6 s; ele não atira a menos de 20 m), o Stryker, o Linebacker
  no chão (o Linebacker só atira no ar), a Technical, os prédios.
- **Fraco contra:** o ar (não atira pra cima: o Apache mata um a cada ~17 s sem levar tiro), o Javelin em grupo (3 matam em 4 s), a
  Patriot (2,45× mais forte por dólar), o Overlord (um ganha de dois Battle Tanks por pouco, ~14 s).
- **O contra dele:** Apache, Raptor, Javelin, RPG, Patriot. **Ele é o contra de:** Tomahawk, veículo leve, prédio.

## 4. Em batalha

- **Temperamento: o profissional.** Confiante, preciso, de poucas palavras. Ele não esmaga a porta como o Overlord: ele chega
  primeiro, atira primeiro e sai antes do troco.
- **O ritmo:** BUM ... dois segundos ... BUM. O recuo do cano (55 cm pra dentro e volta em ~0,5 s) marca o compasso.
- **O jogador bom** usa o giro: ataca de lado (o ricochete de frente protege; a lateral e a traseira não), anda junto do Linebacker
  (o ar) e de Rangers (a infantaria), e espalha quando aparece o anel do ataque de mísseis.
- **A IA** faz o tanque como o "1º da lista" (vale por 2 na mistura) e escolta cada Tomahawk com 2 tanques.
- **Sozinho e ferido:** atira até o fim; é rápido o bastante pra recuar, e o jogador bom recua.

## 5. A vida

- **Parado:** a turbina em marcha lenta (o assovio agudo e o calor tremendo no escape de trás, sem fumaça preta: a turbina queima
  limpo); a torre volta pra frente 2 s depois do último alvo; a caixa do sensor com a lente azul gira devagar; as antenas balançam.
- **Andando:** as esteiras com a poeira em volume (aprovada), a marca da lagarta no chão; o casco balança na suspensão ao frear.
- **Atirando:** o clarão e a bola de ~1,5 m por ~0,2 s, a fumaça rala até ~0,7 s (aprovado em 17/10); o cano recua; o casco joga
  pra trás com inércia (o pedido aberto: igualar ao recuo do tanque da GLA, aprovado 100%).
- **Ferido:** fumaça cinza do motor; abaixo de 30%, o fogo pequeno no cesto e as saias faltando placas.
- **Pronto na fábrica:** sai pela porta com a torre pra frente e a poeira do pátio.

## 6. O visual

- **De cima:** o retângulo comprido e claro (7,9 × 3,7 m de casco), a torre larga quase do tamanho da largura, o cano comprido pra
  frente com os 2 anéis azuis, a lona azul atrás.
- **Assinatura:** a torre facetada com a caixa do sensor de lente azul.
- **Cores:** cinza-claro quase branco com poeira (lê ~0,55 de luminância de cima); a cor do jogador em detalhes pequenos seguindo
  as placas (a faixa segmentada nas saias, os retalhos na torre, os anéis do cano, a lona no motor).
- **Nunca:** camuflagem de areia (some no deserto, medido); neon; placa grande pintada; contorno fino nas bordas; o V invertido.

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão laranja-branco curto, a bola simulada, o anel de fumaça rala; a areia levantada na frente pelo sopro (hoje
  um bolo; o certo é a saia rente ao chão que enrola e paira: melhoria futura aprovada).
- **O projétil:** o risco curto e quente (105 m/s no jogo; o dardo real é muitas vezes mais rápido: o jogo deixa ler).
- **O impacto:**
  - no tanque: o estalo, o leque de faíscas, a chama pequena e a fumaça saindo do furo (aprovado em 30/10, sem a bola bege);
    o ricochete com o jorro raso e o traçante indo embora;
  - no prédio: o pedaço de concreto e a poeira, no ponto do modelo;
  - no chão: a cratera A; os que passam raspando cavam o chão 6–12 m além do alvo;
  - no soldado: o tiro estoura no chão do pé dele (quase não fere).
- **A morte (a de tanque da v2, aprovada em 13/10):** a bola de fuligem cinza-carvão (nunca preta, nunca amarela: o amarelo é só a
  areia que a onda levanta), a torre voando com a física de corpo rígido (aprovada em 23/10: quica 4–7 vezes e assenta), os
  pedaços grossos cor de aço queimado (aprovados em 30/10). **No rumo:** o jato de fogo dos painéis de alívio saindo pelo teto do
  cesto (a munição queimando pra cima, como no tanque de verdade). A carcaça queima ~48 s e afunda em 7 s.

## 8. O som

- **Seleção (variantes):** a turbina subindo (o assovio que vira ronco); o servo elétrico da torre (o "zzzum" curto e limpo); a
  escotilha batendo; o rádio digital com o bipe.
- **As falas** (o comandante do tanque; voz firme, média, rádio limpo, frases curtas):
  - selecionado: "Armor, ready." / "Tank one, go ahead." / "Gun's up." / "Awaiting orders."
  - andando: "Moving." / "Rolling out." / "Copy, advancing." / "On the way."
  - atacando: "Target acquired." / "Sabot, up!" / "Fire!" / "Engaging armor."
  - sob fogo: "Taking hits!" / "Armor's holding." / "Bounced it!" / "Driver, back us up!"
  - pronto: "Battle tank ready." / "Crew's aboard, all systems green." / "Tank on the line." / "Ready to roll."
- **O corpo:** a turbina a gás (o assovio agudo que deu ao M1 o apelido "whispering death" + o ronco grave embaixo), as esteiras
  com o clank mais rápido e leve que o do Overlord, o servo da torre elétrico.
- **A arma:** o estrondo seco com o "tum" no peito, o assovio do projétil saindo, o "clang" da culatra e o municiador (a mão, não o
  automático: o som humano do carregar) durante os 2 s.
- **A morte:** o estalo do acerto, o estouro grave do corpo, o jato da munição (o "fuuush" longo dos painéis), a torre batendo no
  chão (o baque mais pesado), o fogo crepitando com pipocos.
- **A cara da USA:** turbina, servo elétrico, rádio limpo.

## 9. Ideias pra depois

- **A mira do comandante:** enquanto o canhão recarrega (2 s), a torre já vira pro próximo alvo que o comandante achou (a mira
  independente do M1A2 de verdade, o "caçador-matador"). O 2º tiro sai mais cedo depois de matar; lê pela torre que gira antes do
  alvo morrer.
- **A tripulação que salta:** na morte que não é catastrófica, 1–3 tripulantes saltam da escotilha com fumaça e correm pra base
  (`docs/ESTUDO_INFANTARIA_DESIGN.md`, ideia 6); levam a veterania pro próximo tanque.
- **O aterro:** atrás do aterro do Dozer (a "mureta"), só a torre fica exposta: menos dano de frente, o tanque lê como afundado.
- **As pétalas do dardo:** no zoom, as três pétalas do sabot se abrindo na frente da boca e caindo no chão a uns metros (o detalhe
  que só quem conhece vê).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_mbt` (feito no fim, com o modelo provisório como anexo de forma).
