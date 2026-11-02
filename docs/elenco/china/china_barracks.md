# Barracks (China, o pátio de parada)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o quartel: onde nasce a infantaria da China |
| Custo e tempo | $500, 10 s |
| Vida e blindagem | 1 000, prédio |
| Energia | 0 |
| Visão | 52,7 m |
| Lote | 16 × 20 m |
| Arma | nenhuma |
| Requer | nada |
| Produz | Red Guard (o par: $300, 10 s), Tank Hunter ($300, 5 s) |

## 2. A ideia

**O pátio de treino murado com o portão de pagode e o mastro da bandeira.** A disciplina da China em forma de prédio: o portão
cerimonial vermelho de 3 vãos na frente, o pátio de parada com o mastro no centro, o dormitório comprido no fundo, a pista de
obstáculos de um lado e os bonecos de palha do outro.

- **A palavra de cima:** o PÁTIO com o mastro e a bandeira vermelha no centro, emoldurado pelo muro de telha verde.
- **Inspiração real:** o paifang (o portão cerimonial de colunas vermelhas com os telhadinhos) e os pátios de parada dos quartéis
  do ELP (o mastro no centro, o dormitório no fundo).
- **Por que existe:** a infantaria barata é a massa da China; o quartel é barato e não pede nada, então a China tem soldado
  desde o primeiro minuto.

## 3. Como funciona

- **No jogo:** $500, 10 s, sem energia, sem requisito. Produz o **Red Guard em par** (`produce_count 2`: um clique, dois
  soldados, $300 o par) e o **Tank Hunter**. Os soldados saem pelo portão (`exit_lane_m` 6,6 m).
- **Forte contra:** nada sozinho; é a fonte da massa.
- **Fraco contra:** tudo que mira prédio; tem só 1 000 de vida (um tanque derruba em ~33 s).
- **O contra dele:** o raide de tanque. **Ele é o contra de:** a falta de infantaria (o par sai a $150 o soldado, mais barato
  que o Ranger da USA, $225).

## 4. Em batalha

- **Temperamento: o sargento.** Rígido, barulhento, pontual. Grita a ordem e manda o par marchando.
- **O ritmo:** o apito, a marcha, o portão; a cada 10 s, dois soldados.
- **O jogador bom** põe a fila de 5 pares e o ponto de encontro na frente da linha; mistura 2 pares de Red Guard pra cada Tank
  Hunter quando o inimigo tem tanque.
- **A IA** o põe cedo e mistura pelo que tem menos (o fuzil vale por 2; o Red Guard conta por par).
- **Sozinho e ferido:** a bandeira segue no mastro até o fim (ideia: ela é a última coisa a cair).

## 5. A vida

- **No jogo:** o andaime de aço na obra; abaixo de 35%, o fogo nos 2 pontos do telhado e os buracos.
- **Ideia (parado):** a bandeira grande do mastro tremula no vento do mapa; as lanternas vermelhas do portão balançam; de vez em
  quando, 4 soldados pequenos fazem a parada no pátio (só desenho); o boneco de palha balança depois do treino de baioneta.
- **Ideia (produzindo):** o apito curto; o par sai pelo vão do meio do portão, em passo de marcha nos 3 primeiros metros, depois
  corre.
- **Ideia (ferido):** o dormitório pega fogo primeiro; o portão segue de pé (o paifang é o último a cair).

## 6. O visual

- **De cima:** o quadrado do pátio claro com o mastro e a bandeira vermelha no centro, o portão de 3 telhadinhos na frente, o
  dormitório comprido de telhado verde no fundo, a pista de obstáculos e os bonecos nos lados.
- **Assinatura:** o portão vermelho de 3 vãos + a bandeira no mastro.
- **Cores:** concreto escuro oliva, telha verde; o vermelho do jogador nas colunas do portão, na borda dos telhados, na bandeira,
  nos 2 estandartes da varanda e na estrela.
- **Nunca:** mais de 4 estandartes; cruz vermelha; arame farpado de campo de prisioneiros (é quartel, não prisão).

## 7. Tiro, projétil, impacto, destruição

- **Impacto (ideia):** o canhão derruba um pedaço do muro de telha; a bala lasca as colunas vermelhas (a tinta salta).
- **A morte (no jogo):** a morte de prédio (a grande no meio, 3 menores, o chão queimado, o entulho: $50 em ~6,5 s pro Dozer).
- **A morte (ideia):** o dormitório desaba, o portão racha e tomba pra frente sobre o pátio; o mastro cai por último e a
  bandeira fica no chão, queimando na ponta.

## 8. O som

- **Seleção (variantes):** o apito do sargento; as botas batendo juntas no chão (o "tum" da parada); o coro curto contando o passo
  ("yi, er!"); o mastro rangendo com a corda da bandeira batendo.
- **As falas** (inglês; o sargento instrutor, voz rouca, alta, sem paciência):
  - selecionado: "Barracks, at attention!" / "Recruits ready!" / "Train them hard." / "Discipline wins wars." / "Who's next?"
  - produzindo: "Fall in, two by two!" / "Squad, move out!" / "Fresh soldiers for the front!"
  - sob ataque: "Barracks under attack!" / "Everyone to the walls!" / "They're shooting at the recruits!"
  - pronto: "Barracks complete." / "Training begins."
- **O corpo:** ao fundo, o coro distante do treino, a contagem em chinês, o estalo de tiro no estande (abafado), o apito de vez em
  quando.
- **A construção:** os blocos de concreto assentados, as colunas do portão içadas, o martelo nas telhas; no fim, a bandeira
  subindo no mastro com a corda chiando e o toque curto de corneta.
- **A morte:** os estouros; a madeira do portão estalando no fogo; o mastro caindo de lado com o "clang" do aço.

## 9. Ideias pra depois

- **O treino:** um upgrade ($800, 30 s) que faz os soldados saírem com a primeira patente (mais 10% de vida e mira); o pátio ganha
  os soldados treinando à vista.
- **O par que se cobre:** o Red Guard que perde o parceiro fica "furioso" por 10 s (recarga 20% mais rápida e não fica preso pela
  supressão), a disciplina do par.
- **A bandeira que reúne:** clicar na bandeira do pátio põe o ponto de encontro de todos os quartéis num toque só.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#china_barracks` (feito no fim, com o modelo provisório como anexo de forma).
