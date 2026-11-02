# Barracks (USA, a infantaria)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o quartel: treina os soldados |
| Custo e tempo | $600, 10 s |
| Vida e blindagem | 1 000, prédio |
| Energia | 0 |
| Visão | 52,7 m |
| Lote | 23,2 × 18,9 m |
| Requer | nada (o ZH 1.04 também não pede a usina) |
| Produz | Ranger ($225, 5 s); Javelin ($300, 5 s, pede também o Strategy Center) |
| Conta pra vitória | sim |

## 2. A ideia

**O hangar de meia-lua com o pátio de treino.** Um salão de teto curvo com nervuras, atrás; na frente, o pátio de areia batida
cercado por muretas de sacos de areia, com o mastro e a bandeira. De cima se lê "quartel" antes de qualquer detalhe.

- **A palavra de cima:** o TETO CURVO de nervuras (o meio-cilindro) com o pátio murado na frente. O provisório de hoje ainda é
  um salão de teto reto: a arte final volta pro arco (o estudo de prédios, `docs/CONCEITO_CONSTRUCOES.md` §3b).
- **Inspiração real:** os abrigos de arco metálico das bases de campanha (o hangar de chapa corrugada em meia-lua) e o pátio de
  treino com o percurso de obstáculos.
- **Por que existe:** a infantaria é o que o tanque não faz: ver primeiro, segurar a vila, o antitanque escondido. E é a peça mais
  barata da base: o primeiro prédio "de luta" que o jogador pode pôr sem energia.

## 3. Como funciona

- **No jogo:** um soldado por vez, 5 s cada; sai pela frente (a faixa de 6,6 m). Sem energia na base, o treino cai pra 50–80% da
  velocidade como toda produção.
- **Forte contra:** nada (não atira). **Fraco contra:** quase tudo: 1 000 de vida, 4 Battle Tanks levam ~8 s.
- **Vender:** $300 em 6 s.
- **A IA** usa o quartel pela mistura (o fuzil vale por 2); a construção do quartel pela IA da USA ainda é a de regras.

## 4. Em batalha

- **Temperamento: o sargento instrutor.** Duro, alto, sem paciência e orgulhoso dos seus. Cada soldado que sai é "um dos meus".
- **O ritmo:** o grito, a corrida, a porta. Um soldado a cada 5 s, o compasso mais rápido da base.
- **O jogador bom** põe o quartel perto da frente da base (o soldado anda devagar, 5,3 m/s) e atrás de uma defesa; o ponto de
  reunião na vila mais próxima.
- **Sozinho e ferido:** segue treinando até cair.

## 5. A vida

- **Parado:** a bandeira no vento do mapa; dois ou três soldados em exercício no pátio (flexão, corrida em volta da mureta) só
  quando a fila está vazia; a luz da porta acesa.
- **Treinando:** a porta abre, o soldado sai correndo e para no pátio; o próximo já aparece na porta. Os soldados de exercício
  param e olham quando um sai (o detalhe que conta a história).
- **Na obra:** as nervuras do arco sobem primeiro, a chapa vem por cima; os sacos de areia por último.
- **Ferido:** com 70% fumaça saindo de uma ponta do arco; com 30% o arco amassado e o fogo dentro.
- **Sem energia:** a luz da porta apaga (o quartel não gasta energia, mas a base em falta treina mais devagar).

## 6. O visual

- **De cima:** o retângulo de teto curvo com as nervuras em listras paralelas (a textura que lê de longe), o pátio mais claro na
  frente com a borda irregular dos sacos de areia, o ponto do mastro.
- **Assinatura:** o arco de nervuras + o pátio murado.
- **Cores:** chapa cinza-claro e concreto, sacos de areia cáqui; a cor do jogador na faixa do arco, na lona amarrada no teto e na
  bandeira do time.
- **Nunca:** quartel de tijolo com telhado de casa; o pátio vazio sem a mureta (vira estacionamento).

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão fura a chapa do arco (o furo com a borda virada pra dentro); o fuzil faz faísca e tira poeira dos sacos.
- **A morte:** o arco cede no meio e as duas pontas levantam (a chapa corrugada dobra como papelão); os sacos de areia estouram e
  espalham areia; o mastro cai. Pouco fogo (o prédio é chapa e lona), muita poeira.
- **O que fica:** o arco amassado no chão; o Dozer recolhe $60 em 7 s.

## 8. O som

- **Seleção (variantes):** o apito do sargento; botas batendo em cadência; o rádio limpo com o bipe; a porta de chapa batendo.
- **As falas** (o sargento instrutor; voz alta, rouca, ritmada):
  - selecionado: "Barracks! On your feet!" / "Drill yard, sound off!" / "Who's next?" / "Fresh boots ready."
  - treinando: "Move it, move it!" / "Fall in, soldier!" / "Out the door!" / "You're mine now, private."
  - sob ataque: "Barracks under fire!" / "Get down, get down!" / "They're hitting the yard!" / "Defend the barracks!"
  - pronto: "Barracks squared away." / "Training ground is open." / "Let's make some soldiers." / "Drill yard ready."
- **O corpo:** o vento na chapa corrugada (o zumbido metálico fino), a corda do mastro batendo no mastro (o "tin-tin" clássico de
  base), o grito de cadência ao longe, o rádio do quartel.
- **A obra:** o arco montando (as nervuras batendo), a chapa corrugada aparafusada, os sacos de areia caindo um em cima do outro.
- **A morte:** a chapa dobrando (o rangido largo), os sacos estourando (o "puf" de areia), o mastro batendo no chão.

## 9. Ideias pra depois

- **A retirada:** o botão "Fall back" manda o grupo selecionado pro quartel mais perto, correndo 25% mais rápido; chegando, cura e
  guarda a veterania (`docs/ESTUDO_INFANTARIA_DESIGN.md`, ideia 11). O soldado que volta vira personagem.
- **O pátio que treina:** com a fila vazia, o pátio dá um pouco de veterania aos soldados que estão nele (o "treino de tiro"),
  até um limite, e mostra isso com os alvos de tiro no fundo do pátio.
- **O alto-falante do quartel:** quando a base é atacada, a sirene toca e os soldados do pátio saem pra defesa sozinhos (até 4),
  como a reserva de prontidão.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_barracks` (feito no fim, com o modelo provisório como anexo de forma).
