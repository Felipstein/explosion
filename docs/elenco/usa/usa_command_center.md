# Command Center (USA, o coração da base)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o quartel-general: começa a partida pronto, faz o Dozer e liga o poder do general |
| Custo e tempo | $2 000, 45 s (o 2º CC; o 1º já nasce pronto com 1 Dozer) |
| Vida e blindagem | 5 000, prédio (fuzil 50%, metralhadora rotativa 10%, fogo 100%, veneno 1%) |
| Energia | 0 (não gasta nem dá) |
| Visão | 79 m |
| Lote | 25,3 × 29,5 m (o maior prédio da base, menos o Airfield) |
| Requer | nada |
| Produz | Dozer |
| Liga | o Cruise Missile Strike (na patente 2, com o CC pronto e vivo) |
| Conta pra vitória | sim |

## 2. A ideia

**O radar.** Um bloco blindado baixo de comando com a antena de radar grande girando numa torre treliçada, a bandeira no mastro
e a garagem do Dozer na frente com o pórtico amarelo. É a primeira coisa que o jogador vê e a última que ele quer perder.

- **A palavra de cima:** a ANTENA girando (o círculo que se mexe sobre o retângulo, a regra 2 do estudo de prédios,
  `docs/CONCEITO_CONSTRUCOES.md` §3b).
- **Inspiração real:** o posto de comando de brigada de hoje (os abrigos de comando com antenas, os radares de vigilância de
  campanha, a garagem de manutenção). A antena de busca que gira é do mundo real (radar de vigilância aérea); a escala e o
  bloco são de jogo.
- **Por que existe:** é o "eu ainda estou no jogo". Faz o único construtor, liga o poder e ancora a base. Perder o CC não perde
  a partida (a vitória conta todos os prédios), mas tira o Dozer novo e o ataque de mísseis.

## 3. Como funciona

- **No jogo:** 5 000 de vida (2,5× a War Factory). Seis Battle Tanks (30/s cada no prédio) levam ~28 s; Rangers fazem 8,3/s
  cada (o fuzil entra 50%): vinte soldados levam ~30 s.
- **O poder:** com o CC pronto e vivo, a patente 2 acende o Cruise Missile Strike (`usa_cruise_missile.md`). Sem CC, sem poder.
- **Vender:** volta 50% ($1 000) em 6 s de desmontagem; o poder para já no começo.
- **A ruína:** o entulho do CC morto fica no chão e o Dozer pode recolher: 10% do custo ($200) em 14 s.
- **Forte contra:** nada (não atira). É o prédio que mais aguenta.
- **Fraco contra:** cerco de tanques, artilharia (o Tomahawk prefere prédio), o ataque de mísseis inimigo.
- **Quem defende ele:** a Patriot (−3 de energia) e o exército. A IA põe a Drop Zone atrás dele, longe do inimigo.

## 4. Em batalha

- **Temperamento: o veterano no rádio.** Sereno, firme, nunca grita. Ele não luta; ele coordena.
- **O ritmo:** o radar a 6 voltas por minuto, uma volta a cada 10 s, como um relógio de parede. Quando a base apanha, o ritmo do
  radar não muda: a calma é a cara da USA.
- **O jogador bom** põe o CC no fundo, com a Patriot na frente e o Airfield do lado; usa o 2º CC como base avançada (o Dozer
  sai perto da frente).
- **A IA** guarda o CC pelo raio de defesa (inimigo armado a 90 m de um prédio chama quem está a até 200 m) e decide o ataque de
  mísseis a cada 3 s pela nota do alvo.
- **Sozinho e ferido:** segue fazendo Dozer e mantendo o poder até cair. O último CC é o último Dozer.

## 5. A vida

- **Parado:** a antena gira (6 rpm) e **para sem energia**; as luzes de serviço quentes acesas na garagem; a bandeira no vento do
  mapa; vapor branco fino saindo da exaustão do gerador.
- **Fazendo Dozer:** o pórtico da garagem acende a luz âmbar, a porta sobe, o Dozer sai pela faixa de 6,6 m.
- **Na obra:** o andaime sobe com o prédio; a solda em rajadas de estalos, ponto a ponto (aprovada em 30/10).
- **Ferido:** com 70% a baforada de fumaça do telhado; com 30% o estouro e o fogo volumétrico no telhado (os dois aprovados em
  30/10, "top demais"). O conserto do Dozer apaga.
- **Sem energia:** a antena para, as luzes de serviço caem pra metade, o rádio do operador some.

## 6. O visual

- **De cima:** o retângulo grande e claro com o disco da antena em cima, de um lado; o pórtico amarelo da garagem na frente; a
  bandeira. O maior volume da base.
- **Assinatura:** a antena de radar na torre treliçada.
- **Cores:** painel cinza-claro e concreto claro com poeira; o amarelo industrial do pórtico e das faixas de perigo; a cor do
  jogador (azul) nas faixas das paredes e nas lonas amarradas no teto.
- **Estado:** o provisório por script foi **reprovado** (13/10: "tá horrível aquilo, mas depois a gente vê isso"). O desenho
  final começa por ele, uma palavra só (o radar), sem "um prédio com muitas coisas" (a lição da base v1).
- **Nunca:** neon, portas brilhando, cara de nave; prédio pintado de azul; caixa genérica sem a antena.

## 7. Tiro, projétil, impacto, destruição

- **Impacto:** o canhão no prédio faz o pedaço de concreto e a poeira (o tiro tem de bater nos triângulos do modelo, o
  problema aberto de 30/10); o fuzil faz faísca e pó no painel.
- **A morte (o pedido dele, 30/10):** nada de afundar e brotar quadradinhos. A ordem: a antena trava e tomba da torre, o andar de
  comando estoura pelas janelas (o clarão sai pela faixa de vidro), o teto cede no meio, a torre treliçada dobra e cai pro lado
  do impacto, o entulho fica com o fogo baixo e a fumaça preta subindo por ~30 s.
- **O que fica:** a ruína que muda a rua (cobertura, caminho fechado), até um Dozer recolher.

## 8. O som

- **Seleção (variantes):** o clique do rádio digital limpo + o bipe curto de dois tons; o motor da antena com o rolamento; o
  zumbido grave dos geradores; a porta da garagem destravando.
- **As falas** (a operadora da base no rádio, a EVA da USA, a mesma voz dos avisos (04/11, `docs/elenco/SONS_UNIDADES.md`); em inglês; calma, seca, com o leve chiado digital e o "roger" seco):
  - selecionado: "Command, go ahead." / "Headquarters on the line." / "Command post, listening." / "All stations, standing ready."
  - produzindo: "Dozer rolling out." / "Engineering detail dispatched." / "Builder on the pad." / "Another hand for the base."
  - sob ataque: "Hostiles at the gate." / "We're taking fire, Command." / "Perimeter breach, respond." / "Hold them off the HQ."
  - pronto (obra): "Command post established." / "Headquarters operational." / "Comms are up." / "We have a new home."
  - poder pronto: "Strike package available." / "Missile authority granted."
- **O corpo:** o zumbido do gerador (50–120 Hz, constante, baixo na mixagem); o motor elétrico da antena com um tique leve a cada
  volta; o ar-condicionado das salas; o rádio de fundo com tráfego ininteligível bem baixo.
- **A obra:** do andaime ao pronto: o guindaste, os parafusos de impacto, a solda em rajadas, a chapa assentando; no fim o
  gerador "pega" e sobe o tom e a antena dá a 1ª volta.
- **A morte:** o estalo do vidro, o ronco grave do teto caindo, o metal da torre dobrando (o rangido longo) e a pancada da
  antena no chão; depois o fogo baixo e o estalar do concreto quente.
- **A cara da USA no som:** rádio limpo e digital, servo elétrico, disciplina.

## 9. Ideias pra depois

- **O radar de verdade (do ZH, melhor):** o minimapa só mostra o terreno e as unidades com um CC vivo e energia; o traço da
  varredura gira no minimapa no mesmo ritmo da antena do prédio. Sem energia, o minimapa vira chuvisco.
- **O mastro da bandeira conta a vida:** a bandeira desce a meio mastro abaixo de 30%; volta a subir com o conserto.
- **A sala de guerra:** com o CC selecionado, o jogador vê os últimos 10 s de onde o inimigo foi visto (os "rastros" do radar),
  como um quadro de situação. Informação é a arma da USA.
- **A evacuação:** quando o último CC cai, sai do entulho um veículo de comando blindado (o posto móvel) que mantém o poder vivo
  por 60 s e pode montar um CC novo pela metade do preço se chegar a um lugar seguro. Dá ao "quase perdi" uma última jogada.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#usa_command_center` (feito no fim, com o modelo provisório como anexo de forma).
