# War Factory (USA, a fábrica de veículos)

## 1. Ficha rápida

| | |
|---|---|
| Papel | a fábrica: todo veículo de chão da USA sai daqui |
| Custo e tempo | $2 000, 15 s |
| Vida e blindagem | 2 000, prédio |
| Energia | −1 |
| Visão | 52,7 m |
| Lote | 22,3 × 25,3 m |
| Requer | Supply Center |
| Produz | Battle Tank, Linebacker, Stryker, Tomahawk Launcher (patente 3), Paladin (com o Strategy Center) |
| Libera | Airfield, Supply Drop Zone, Strategy Center |
| Conta pra vitória | sim |

## 2. A ideia

**O hangar com o pórtico.** O galpão blindado com o vão da porta e a ponte de pórtico amarela por cima da saída, dois
ventiladores grandes no teto, o domo de radar na torre treliçada, as chaminés gêmeas soltando vapor branco e, do lado, o pátio
do braço robótico pintado com a faixa de perigo. É a arte aprovada (`docs/concept/buildings/usa_war_factory.webp`): o primeiro
prédio da USA que o dono aprovou.

- **A palavra de cima:** o PÓRTICO amarelo sobre a porta, com os dois ventiladores girando no teto.
- **Inspiração real:** as oficinas de manutenção pesada do exército (o hangar de blindados, a ponte rolante, os robôs de
  solda da indústria); a fórmula do ZH (o diorama compacto, a função na forma, o prédio que trabalha).
- **Por que existe:** é o degrau do meio da árvore. Sem ela não há tanque, nem Airfield, nem tecnologia. Perder a fábrica no meio
  da partida é perder o ritmo.

## 3. Como funciona

- **No jogo:** fila de até 9; o veículo sai pela faixa de 6,6 m da porta. Sem energia, a produção cai pra 50–80% da velocidade.
- **Os tempos:** Battle Tank 10 s, Linebacker 10 s, Stryker 10 s, Paladin 12 s, Tomahawk 15 s.
- **Forte contra:** nada (não atira). **Fraca contra:** o cerco de Tomahawk (a artilharia prefere prédio), o ataque de mísseis
  (~900 de 2 000, medido), os tanques: 6 Battle Tanks levam ~11 s.
- **Vender:** $1 000 em 6 s; a fila devolve o dinheiro na hora.
- **A IA** treina pela mistura: o que tem menos, com o tanque valendo por 2; o Tomahawk quando a patente deixa (1 + 1 por defesa
  inimiga conhecida, até 4); não constrói o Stryker.

## 4. Em batalha

- **Temperamento: o chefe de oficina.** Barulhento, orgulhoso, sem tempo pra conversa. Mede o mundo em veículos por minuto.
- **O ritmo:** o compasso de 10 s, a porta, o veículo, a porta. Quando a fila está cheia, a fábrica nunca fica em silêncio.
- **O jogador bom** tem duas, longe uma da outra; põe o ponto de reunião atrás da Patriot, nunca na porta (o ataque de mísseis
  pune o bolo parado na saída).
- **Sozinha e ferida:** segue produzindo até cair; o veículo que sai da fábrica em chamas sai inteiro.

## 5. A vida

- **Parada:** os 2 ventiladores do teto giram (38 e 44 rpm, desencontrados pra não parecer um só) e **param sem energia**; o
  vapor branco das chaminés; o domo de radar parado na torre; as luzes de serviço quentes no vão da porta; a bandeira.
- **Produzindo:** a ponte de pórtico corre dentro do vão, a solda pisca lá dentro; a porta sobe e o veículo sai com a poeira; o
  braço robótico do pátio se mexe (o gesto de "apertar" uma peça).
- **Na obra:** o andaime, a solda em rajadas (aprovada); o pórtico sobe por último.
- **Ferida:** com 70% a baforada no telhado; com 30% o estouro e o fogo volumétrico no telhado (aprovados em 30/10).
- **Sem energia:** os ventiladores param, o vapor some, a solda lá dentro fica mais espaçada (a produção mais lenta se vê).

## 6. O visual

- **De cima:** o retângulo claro com a fenda escura da porta na frente, a barra amarela do pórtico atravessada, os dois discos
  dos ventiladores, as duas chaminés redondas num canto, o quadrado de faixa de perigo do braço do lado.
- **Assinatura:** o pórtico amarelo + os ventiladores.
- **Cores:** painéis cinza-claro e concreto claro com desgaste e poeira; amarelo industrial no pórtico e nas faixas de perigo; a
  cor do jogador na faixa azul das paredes e nas lonas do teto.
- **Nunca:** neon, porta brilhando, cara de nave; o prédio pintado de azul.

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão no painel solta o pedaço de chapa e a poeira; nos ventiladores, a pá entorta e o giro fica torto (só
  desenho).
- **A morte:** o pórtico perde um pé e cai atravessado na porta (a saída fica bloqueada pelo entulho); os ventiladores voam do
  teto; o domo cai da torre e rola; as chaminés tombam pro lado contrário do impacto. Fogo grande dentro do vão (o óleo, a
  tinta), fumaça preta saindo pela porta.
- **O que fica:** a ruína com o pórtico caído; o Dozer recolhe $200 em 14 s.

## 8. O som

- **Seleção (variantes):** o rádio limpo + a sirene curta de pátio industrial; o motor do pórtico; a chave de impacto
  pneumática ("brrt-brrt"); o portão destravando.
- **As falas** (o chefe de oficina; voz rouca, alta, competente, com o barulho da fábrica atrás):
  - selecionado: "Factory floor, talk to me." / "Line's running." / "What are we building?" / "Motor pool, go."
  - produzindo: "Fresh off the line." / "Armor rolling out." / "One more for the front." / "She's fueled and ready."
  - sob ataque: "They're shelling the factory!" / "Fire on the line!" / "We're losing the floor!" / "Keep the doors open!"
  - pronto: "Factory's open." / "Assembly line online." / "Motor pool operational." / "Let's make some armor."
- **O corpo:** os ventiladores (o "vum-vum" grave, desencontrado); a fábrica lá dentro: prensas, solda, a ponte rolante, as
  correntes, abafados pelas paredes; a porta de aço subindo (o motor e o rangido).
- **A obra:** o andaime, o pórtico assentando com o "clang" grande; no fim os ventiladores começam a girar (a subida do giro).
- **A morte:** o pórtico caindo (o metal mais pesado da base), os ventiladores soltos batendo, o fogo de óleo com os estouros
  pequenos de lata de tinta e cilindro de gás.

## 9. Ideias pra depois

- **A linha de montagem dupla** (o pedido dele, `docs/IDEIAS_JOGABILIDADE.md` §3): duas linhas ao mesmo tempo pros leves e médios,
  ~15% mais rápido, o pesado (o Paladin) ocupa as duas; pede a patente. A fábrica ganha o anexo robotizado com os braços e o 2º
  portão.
- **O pátio de conserto:** o veículo parado no quadrado de faixa de perigo do braço robótico se conserta (2% da vida por segundo,
  o ritmo do Dozer); o braço faz o movimento sobre o casco e a solda pisca. Dá função à peça que a arte já tem.
- **O tanque que sai pintado:** a cor do jogador e o número do veículo pintados na saída, de leve (o número sobe a cada um que
  sai: o "12º tanque" da partida).
- **A porta que trava:** inimigo encostado na porta impede a saída; o veículo pronto espera dentro (e lê na tela pela porta
  fechada com a luz âmbar piscando).

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_war_factory` (feito no fim, com o modelo provisório como anexo de forma).
