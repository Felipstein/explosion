# WorldForge: ideias de direção de arte (TODO)

Recado da sessão de arte conceitual (06/10/2026) pra sessão do WorldForge.
**São ideias, não ordem.** O Felipe disse que você ainda vai baixar assets,
subir a qualidade e otimizar; use esta lista quando chegar nessa fase e
descarte o que não fizer sentido. Contexto completo da arte do jogo em
`docs/CONCEITO_UNIDADES.md` e `docs/CONCEITO_CONSTRUCOES.md`; imagens
aprovadas em `docs/concept/`.

Olhei as capturas `captures/worldforge/{desert,snow,forest}_final` (fotos e
passeio). A base técnica está forte: a luz dourada de sombra comprida, os
paredões de arenito em camadas (já parecem Wadi Rum), a terra rachada, as
sombras azuis dos pinheiros na neve. O que falta é criativo.

---

## A ideia que amarra tudo: o terreno conta a história

Hoje cada coisa está espalhada por sorteio, por igual. Na natureza, cada
coisa está onde está por um motivo: **vento, água, gravidade ou gente**.
Quando o espalhamento segue esses motivos, o mapa fica bonito sozinho e
desenha linhas que se leem de cima.

| Motivo | O que ele deixa no chão |
|---|---|
| Gravidade | pedra caída ao pé do penhasco (tálus), cascalho na base das encostas |
| Água | leitos secos (wadis) com cascalho, vegetação seguindo os leitos, oásis |
| Vento | ondulação da areia numa direção só, areia empilhada no lado protegido das rochas, duna |
| Gente | estrada gasta, marca de esteira e de pneu, areia cobrindo a beira da estrada, lixo de guerra perto das bases |

O gerador já tem as máscaras (encosta, altura, trilha). Dá pra derivar
"distância ao pé do penhasco", "linha de drenagem" (fluxo de água) e "sombra
de vento" e espalhar a partir delas.

---

## Alvo visual: o mapa AlUla

O conceito de mapa mais forte até agora: `docs/concept/maps/alula_1v1_candidate.webp`
(o desenho é o `alula_1v1_layout_gemini.webp`, refeito em qualidade AAA).
Não está aprovado como mapa oficial (o Felipe preferiu esperar o WorldForge),
mas é o alvo de beleza.

- **Planície de areia vermelha com poucos monólitos de arenito soltos**
  (inspiração: AlUla, na Arábia Saudita; a Elephant Rock tem 52 m). É a
  geografia de um mapa de RTS: muito chão aberto, obstáculos como ilhas.
  Cada monólito vira um ponto de referência e cria os gargalos.
- **Um arco natural de pedra** (Wadi Rum).
- **Um marco no centro:** uma fachada de tumba talhada num afloramento de
  arenito (inspiração: Hegra), com um **oásis** de palmeiras e água em volta.
  O verde e o azul contra o vermelho são o contraste que a direção de arte
  de Assassin's Creed Origins aponta como o que dá vida ao deserto.
- **Dunas douradas só na borda do mapa**, como moldura (Liwa, de Duna Parte 2).
- Bases em platô com uma rampa, poços de petróleo de cada lado.

Outra ideia guardada pra um segundo mapa: um rio em cânion cortando o mapa,
com duas pontes e uma cidade no meio do rio como travessias
(`docs/concept/maps/river_1v1_idea.webp`).

---

## TODO por bioma

### Deserto
- [ ] **Monólitos isolados** como tipo de relevo (além de mesa e paredão):
      altos, com a base mais fina (comida pela areia que o vento joga) e
      camadas horizontais. Um ou dois arcos por mapa.
- [ ] **Pedras pela gravidade:** concentrar ao pé dos penhascos e monólitos
      e nos leitos secos; tirar o "confete" uniforme da planície.
- [ ] **Leitos secos (wadis):** linhas de cascalho mais escuro seguindo a
      drenagem, com acácias e arbustos ao longo. Vistos de cima desenham o
      mapa.
- [ ] **Areia pelo vento:** ondulação numa direção só; montes de areia
      encostados no lado protegido das rochas; areia invadindo a beira da
      estrada.
- [ ] **Estrada gasta:** hoje está nova e cinza demais. Faixa apagada,
      remendos, areia por cima nas bordas, marca de esteira saindo dela.
- [ ] **Variação de cor da areia:** vermelha (Wadi Rum) na planície, dourada
      nas dunas, cascalho escuro (pavimento de deserto) nas áreas planas.
      Ajuda a separar o chão passável do obstáculo.
- [ ] **Oásis:** água, palmeiras, capim verde. É o contraste de cor.

### Neve
- [ ] As manchas de terra hoje parecem **mancha de café**: borda borrada e
      sem relação com o relevo. A terra deve aparecer **onde a neve derrete
      de verdade:** encosta voltada pro sol, em volta do tronco das árvores,
      junto das rochas, em cristas que o vento varre.
- [ ] **Neve acumulada** em montes atrás dos obstáculos (lado protegido do
      vento).
- [ ] Rastros de pneu e de esteira virando **lama cinza-marrom** (como o
      Company of Heroes 2), lago ou rio congelado com rachaduras.
- [ ] A superfície de neve está muito lisa e igual; um pouco de ondulação e
      variação de brilho.

### Floresta (a mais fraca das três)
- [ ] A grama parece um **tapete verde repetido**, e a divisa entre grama e
      terra é dura. Variação de altura e de tom, manchas de grama seca
      amarela, flores.
- [ ] **Transição na beira da mata:** arbusto, capim alto e mudas antes do
      bosque fechado (hoje passa de campo direto pra floresta).
- [ ] Trilhas gastas, riacho, clareiras.
- [ ] As vistas baixas ficam vazias (planície marrom e céu chapado).

---

## A ideia grande pro gerador: o marco desenhado à mão

Os AAA fazem assim (Horizon, Far Cry 5, Wildlands, já citados no
`docs/WORLDFORGE.md`): o artista coloca **o que tem significado** e o
procedural preenche o resto em volta.

- [ ] Suportar um **marco** (a tumba com oásis, uma vila, uma ponte, uma
      represa) colocado num ponto do mapa, com um raio próprio.
- [ ] O gerador **organiza o entorno** a partir dele: estradas convergindo,
      chão aberto em volta pra batalha, recursos perto, e respeitando a
      simetria entre jogadores.
- [ ] O mapa AlUla seria o primeiro teste perfeito.

É o que separa um mapa bonito de um mapa que se lembra: no Tempest Rising
(2025), o mapa de deserto "Overgrown Oasis" gira em torno de um templo
desenterrado no centro.

---

## Regras de jogo que vieram da arte conceitual

- **Nenhuma facção pode sumir no chão.** Medido na folha de biomas
  (`docs/concept/maps/biome_study_far_view.jpg`, a imagem reduzida 10× pra
  simular a visão de longe):
  - deserto apaga o GLA (areia-amarelo);
  - **cidade cinza apaga a USA** (cinza-claro);
  - neve lê as duas;
  - verde lê bem, mas a China (verde-oliva escuro) ali é risco, não testado.

  O Felipe quer **paleta por mapa** pras unidades. O gerador pode ajudar
  mantendo o chão passável num tom e brilho que não engolem as cores das
  facções (USA clara, China escura, GLA areia).
- **Chão passável diferente do obstáculo:** areia lisa e mais clara onde
  passa, rocha escura e áspera onde não passa. Os conceitos que funcionaram
  seguem essa regra; o que falhou virou "labirinto de cânion" do mesmo
  laranja.
- **Chão aberto pra batalha de tanque:** o Tournament Desert (o mapa mais
  jogado do ZH) é quase todo plano, com estradas, poucas cristas e poços de
  petróleo no meio. O teto de penhasco de ~6% que você já mediu bate com o
  que a arte quer.
- **Dois caminhos:** um curto e defensável pelo centro, outro longo pelo
  flanco (escola do StarCraft II).
- **Prédios e objetos baixos:** a câmera é a 55°; coisa alta esconde as
  unidades que passam atrás.

---

## Referências

- AlUla: [formações rochosas](https://gulfnews.com/photos/news/look-the-most-incredible-natural-rock-and-geographical-formations-in-saudi-arabias-alula-1.1660906076809)
- Wadi Rum: [ficha da IUCN](https://www.iucn.org/sites/default/files/import/downloads/fact_sheet_jordan.pdf)
- Duna (Wadi Rum e Liwa): [locações](https://cinapse.co/where-did-they-film-dune/) e
  [Unreal na produção do deserto](https://www.unrealengine.com/spotlights/unreal-engine-streamlines-desert-production-on-dune-part-two)
- Assassin's Creed Origins: [como a arte do mundo foi feita](https://gamesbeat.com/?p=150549)
- Tempest Rising: [Overgrown Oasis](https://tempestrising.wiki.gg/wiki/Overgrown_Oasis)
- StarCraft II: [princípios de mapa](https://news.blizzard.com/en-us/article/19956941/novye-karty-v-legacy-of-the-void)
- ZH: [Tournament Desert](https://cnc-central.fandom.com/wiki/Tournament_Desert)
