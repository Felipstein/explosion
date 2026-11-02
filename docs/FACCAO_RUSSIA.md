# A facção Rússia (estudo, 01/11/2026)

O pedido do Felipe (01/11, ao dormir): "quando terminar tudo, já vai estudando a facção da RÚSSIA: como fazer, que paleta é, como
deixar icônico também igual o resto, o que eles têm, o que focar, já montar algumas unidades principais e modelos principais e
construções principais".

Este é o estudo. As escolhas de gosto são dele: a paleta, a assinatura e os nomes. As propostas abaixo dizem qual eu recomendo e por quê, e
como decidir com imagem, no mesmo caminho das outras 3 facções (`docs/CONCEITO_UNIDADES.md` §5).
Regras de base:
- a visão do jogo (`docs/VISAO_DO_JOGO.md`): nomes perto do real, tom sério como o ZH, a nostalgia na frente;
- a regra do equilíbrio (`docs/ELENCO.md` §2).

**Cuidado de tom:** a Rússia de hoje está numa guerra real. O jogo usa os veículos e a doutrina, nunca os símbolos dessa guerra:
- nada de Z ou V pintados;
- nada da faixa branca de identificação de 2022;
- nada de bandeira de grupo militar privado;
- nada de nome de batalha real.

A referência emocional é a nostalgia do C&C: os soviéticos do Red Alert (pesados, de aço, de fogo) no exército de hoje.

---

## 1. A identidade

| | USA | China | GLA | **Rússia (proposta)** |
|---|---|---|---|---|
| **Uma frase** | Vê tudo, acerta primeiro | Arrasa em massa | Vive da ruína | **Cava e queima: segura o chão e esmaga de longe** |
| **Como joga** | Pouco, caro e preciso; o ar; a informação | Muito, barato e pesado; a horda; o fogo | Rápido, barato, esperto; a emboscada | **Devagar e pesado; a artilharia e os foguetes; a defesa antiaérea; a trincheira** |
| **A relação com o campo que muda** | Mira o ponto | Queima a área | Recicla a carcaça | **Molda o terreno: o trator cava trincheira e berma (cobertura de verdade), a artilharia deixa cratera que atrasa quem passa** |
| **A energia** | Fusão (+5) | Nuclear (+10, derrete quando cai) | Nenhuma | **Usina a gás (+7); a chaminé listrada vermelha e branca; cai com a bola de fogo do gás** |
| **A economia** | Caminhão, Chinook, Drop Zone | Caminhão, reator | Picape da carga, o Worker | **O caminhão pesado (KamAZ): carrega o dobro e anda devagar; o depósito ferroviário (o trem que chega com carga, no fim da árvore)** |
| **A tecnologia** | Strategy Center (o prato) | Propaganda Center (os alto-falantes) | Palace (a cúpula) | **O Instituto (o NII): o radar e as antenas de guerra eletrônica** |
| **O pesado** | Paladin | Overlord | Rocket Buggy | **TOS-1A (o lançador termobárico) e o T-14 Armata** |
| **A cor** | cinza-claro, azul | oliva escuro, vermelho | areia, ferrugem, verde | **aço frio azulado, manchas pretas, a cor do jogador nas lonas e redes da gaiola** |
| **De cima** | prédios claros, quinas retas, pratos | blocos escuros, telhados em camadas | pátios de adobe, lonas, redes | **pavilhões de telhado-serra, concreto de painel, a chaminé listrada, as grades por cima das torres** |

### Por que essa identidade
- **Ela não pisa em ninguém:**
  - a China é a horda barata;
  - a Rússia é pouca e cara como a USA, mas pesada e de fogo de área, onde a USA é precisa;
  - a GLA já é a "soviética velha" (T-55, T-62): a Rússia é a soviética de hoje (T-90M, T-14), outra geração e outra linguagem.
- **A doutrina real é essa:** a artilharia e os foguetes primeiro (a "deusa da guerra"), a defesa aérea em camadas (Pantsir, Tor,
  S-400), a guerra eletrônica (Krasukha, Leer-3), a engenharia de campo (IMR-3M, UR-77) e a fortificação.
- **Ela conversa com o pilar do jogo,** o campo que muda: a Rússia é a facção que mais mexe no chão de propósito.

---

## 2. A paleta (o que decide com imagem)

**DECIDIDO (01/11, 19h): a A, a de hoje** ("eu fico com a A de hoje"). A "A média" fica pronta no `ru_paint_set.py` caso a leitura de cima em batalha peça.

As 3 facções já ocupam:
- a USA, o claro (L ≈ 0,55, cinza quase branco, azul);
- a China, o escuro verde (L ≈ 0,30, oliva em camuflagem digital miúda, de 16 cm);
- a GLA, o quente (areia, ferrugem).

O espaço livre é **o frio de luminância média**.

| Opção | Pintura | Prós | Contras |
|---|---|---|---|
| **A (recomendada): aço frio** | Cinza-azulado médio (≈ L 0,42, matiz 200–215°, saturação baixa ~0,10), com **manchas grandes pretas e cinza-claro** (1–2 m, as 3 cores da camuflagem russa de inverno e de cidade), o aço gasto nas quinas | O único frio do jogo: de cima não confunde com nenhuma. A mancha grande separa do pixel miúdo da China na mesma distância. Lembra o aço do Red Alert | Precisa provar que não some no deserto (o céu e a sombra azulados do mapa) |
| B: verde de 3 tons | O verde-oliva russo médio com manchas pretas e areia grandes | O mais real (a pintura de verão do exército) | Verde de novo: briga com a China de longe (oliva) e com o verde da GLA |
| C: cinza-verde liso | O "4BO" liso, sem camuflagem | O mais limpo e barato de pintar | Sem personalidade; de longe vira um cinza qualquer |

**Medido no jogo (01/11, a cena `elenco_4_tanques`: os 4 tanques lado a lado, perto, na câmera do jogo, longe e de cima; as fotos em
`captures/game/elenco_4_tanques/`):** a luminância de cima (L, a mediana do casco) é **areia 0,55, USA 0,34, GLA 0,30, China 0,24,
Rússia 0,17**. O T-90M da paleta A separa de todos, mas sai quase preto, e não no "frio médio" do plano: as manchas pretas grandes e o
aço escuro das grades pesam mais do que na prévia do Blender. **Testado (a A clareada, `captures/game/elenco_4_tanques_B`): a base +40% e a
mancha preta virando cinza-escuro levaram o T-90M só de 0,17 a 0,19.** O escuro de cima não é a tinta: é a gaiola anti-drone, as grades e
as saias em aço escuro (`slat_cage`, `slat_screen`, `grille_deck` do `model_russia_vehicle_kit.py`, o slot `Metal`), que cobrem quase todo
o topo. Pra chegar perto de 0,30: pintar a gaiola e as grades na cor do veículo (o slot `Paint`), e aí a "grade preta" de assinatura vira
grade da cor do tanque. Decisão dele junto com a paleta (a tinta voltou pra A).

**O estudo das 3 paletas no jogo (01/11, 11h; a folha `captures/game/elenco_4_tanques/paletas_russia.jpg`):** as opções da tabela
acima aplicadas no T-90M de verdade (só os parâmetros da tinta e do aço: nada remodelado; tudo voltou pra A no fim), medido SÓ o
tanque de cima (a areia e a sombra fora): **A 0,10; A clareada 0,12; A com a gaiola pintada 0,12; A clareada + gaiola pintada 0,13;
B verde de 3 tons 0,09; C cinza-verde liso 0,11** (as outras na mesma medida: USA 0,30, GLA 0,22, China 0,17).
- Em qualquer opção a Rússia é a mais escura do jogo e toma o lugar do escuro, que é da China; só tinta e gaiola não chegam ao meio:
  pra subir, mais área do casco precisa ser tinta (hoje os blocos de blindagem reativa e as saias também são aço escuro).
- A A é a única fria: de longe separa de todas. A B e a C puxam pro oliva e brigam com a China.
- A minha recomendação: **a A, clareada, com a gaiola e os blocos na cor do tanque** (o casco remodelado com mais área de tinta),
  mirando ~0,20 (entre a China e a GLA). Decisão dele.
- **Testada a "A média"** (`captures/game/elenco_4_tanques/paleta_media.jpg`): a mesma tinta fria com a base mais clara (0,20/0,225/0,25
  linear), as manchas cinza-escuro (0,075) e cinza-claro (0,40) em vez do preto, e a gaiola e as grades na cor da base: **0,14**, o
  cinza-azulado frio que de perto lê como aço e de longe separa da China pela matiz (ainda 0,03 abaixo dela). Os parâmetros prontos
  no scratchpad (`ru_paint_set.py`, os conjuntos MEDIA e GAIOLA_MEDIA); no jogo ficou a A de sempre.

**Medir antes de decidir** (como nas outras: `docs/CONCEITO_UNIDADES.md` §7):
- a luminância e a matiz da vista de cima nas 4 facções lado a lado;
- a separação de longe, na câmera do jogo;
- a cor do jogador em 8 cores.

### A cor do jogador (proposta)
A lona e a rede **por cima da gaiola anti-drone** da torre (a "cope cage", o telhado de grade que os tanques russos ganharam) e a
lona enrolada no motor.
- **De cima é a maior área colorida limpa do jogo** (~8–10% da vista de cima: mais que a China, 3,8%, e menos que um lençol).
- **É só dela:**
  - a USA tem a faixa segmentada e a lona VS-17;
  - a China tem as lonas dobradas;
  - a GLA tem os rabiscos e as bandeiras.
- **Nunca:** a faixa branca pintada, letras ou o símbolo da guerra real.

### A estrela
A estrela vermelha de 5 pontas com o contorno branco, pequena na torre (a tradição do exército). Ela troca com a cor do jogador, como a
da China.
- A China já tem a estrela.
- Pra separar, a da Rússia é **contornada** (a borda branca e o miolo na cor) e fica na lateral da torre, nunca no teto.

---

## 3. A assinatura icônica (o que lê de cima e é só dela)

| Peça | O elemento-assinatura | Por que funciona no RTS |
|---|---|---|
| **Todo blindado** | **A gaiola de grade por cima da torre** (o telhado de ripas de aço) **com a rede e a lona da cor do jogador** | É o que a câmera de cima mais vê. Nenhuma outra facção tem grade cobrindo a torre: separa sem cor (a torre USA larga, a China escudo, a GLA cúpula, **a Rússia gradeada**) |
| T-90M / T-14 | **O mosaico de tijolos de blindagem reativa** (a cunha de blocos na frente da torre, o Relikt) | A textura de "escamas" na frente; o T-14 tem a torre sem tripulação, baixa e retangular, com os sensores |
| TOS-1A | **O lançador de 24 tubos grossos** num chassi de tanque, inclinado | A silhueta que ninguém tem: um bloco de canos apontando pro céu |
| Ka-52 | **Os dois rotores um em cima do outro** (o coaxial) e as asas curtas | Visto de cima, dois discos girando ao contrário: inconfundível |
| Pantsir | **O radar redondo e os 2 canhões com os mísseis** no caminhão | O prato giratório e o "rosto" de canos |
| Prédios | **O telhado-serra** (shed roof) dos pavilhões, **a chaminé listrada vermelha e branca**, o concreto de painel com as juntas, os letreiros em cirílico estêncil, a estrela no frontão | Telhado-serra e chaminé listrada leem de cima a 100 m; o painel com juntas é a textura certa pro detalhe (as juntas fundas do kit) |
| A base | **A trincheira e o dente de dragão** (os blocos de concreto antitanque) | O pilar "a base com fortificação que dá bônus" (decisão 5 de 24/10) nasce nela |

---

## 4. O que focar (o jeito de jogar)

1. **A artilharia e os foguetes:**
   - a Msta-S (o canhão autopropulsado de 152 mm), de alcance longo, com o tiro que cai do alto e a cratera;
   - o TOS-1A, a área termobárica: o fogo que tira a infantaria de prédio e de trincheira.
   - A fraqueza: precisa de quem veja (o drone Orlan ou o radar) e é presa fácil de perto.
2. **A defesa aérea em camadas:** o Pantsir (canhão e míssil) e, no fim da árvore, a bateria S-400 (o alcance mais longo do jogo contra
   avião). É a resposta ao ar da USA.
3. **A guerra eletrônica:** a antena do Instituto e o caminhão Krasukha. Numa área, os mísseis guiados do inimigo (o Javelin, o Tomahawk, o
   Patriot) erram mais e os drones caem. É a resposta à precisão da USA.
4. **A trincheira e o trator:**
   - o IMR-3M cava a trincheira (a infantaria dentro ganha cobertura) e a berma (o tanque atrás dela ganha casco protegido);
   - o UR-77 lança a corda de explosivo que limpa minas e abre caminho (a explosão em linha, o efeito mais bonito do jogo pra fazer).
5. **O ritmo:** lenta no começo, a mais forte parada no meio, a artilharia decide o fim. Perde pra emboscada rápida (GLA), pra precisão
   que chega antes (USA) e pra horda que passa da linha (China).

**O equilíbrio (a mesma régua do `docs/ELENCO.md` §8.1):** o tanque básico, o T-90M, $1 000, 520 de vida, 30 de dano por segundo, 6,2 m/s. A força de Lanchester dá 0,0156: a mais baixa por dólar, compensada pela blindagem reativa e pela gaiola (o primeiro míssil antitanque perde 50% do dano, uma vez a cada 20 s) e pelo apoio de fogo. Os números finais saem da conta, como nas outras.

**Os números que entraram no jogo (01/11, `Game/Tools/elenco_units.py`, a função `russia()`; as fichas em `docs/elenco/russia/`):**
a blindagem reativa não existe nos dados ainda, então o T-90M entrou na faixa dos outros básicos: $950, 560 de vida, força 0,0186, o
alcance dos básicos (39,5 m). O T-14 tem 1 050 de vida (0,0162, a da Paladin). O Kornet custa $300, com 70 de dano a 55 m, sem tiro no ar
(0,039, 12% abaixo dos antitanques a pé). O KamAZ leva 5 caixas (+25%; não o dobro), mais devagar. A estrela segue vermelha fixa (a
assinatura); a cor do jogador fica na rede, nas lonas e nas faixas.

---

## 5. O elenco mínimo (o mesmo tamanho do das outras)

### 5.1 Construções
| Peça | O papel no ZH | O que é | A assinatura |
|---|---|---|---|
| Command Center (o Shtab) | Command Center | O quartel-general em bloco de concreto de painel, com a torre de comunicações treliçada | O frontão com a estrela e a torre de rádio |
| Usina a gás | Power Plant | As turbinas a gás num pavilhão, a tubulação e a chaminé listrada (+7) | A chaminé vermelha e branca; ao cair, o fogo do gás (a coluna alta) |
| Depósito de suprimentos | Supply Center | O galpão de carga com a doca e a ponte rolante | O pórtico amarelo da ponte rolante |
| Quartel (a Kazarma) | Barracks | O bloco de alojamento de painel com o pátio de formatura | O pátio com o mastro e a placa |
| Fábrica de tanques | War Factory | O pavilhão de telhado-serra (o Uralvagonzavod), a porta de correr alta, os trilhos saindo | O telhado-serra |
| Heliporto | Airfield | O heliporto com os hangares em arco (Ka-52, Mi-28) | Os hangares em arco |
| Instituto (o NII) | Tech | O prédio de pesquisa com o domo do radar e o mastro de antenas de guerra eletrônica | O domo e o mastro |
| Pantsir (defesa) | Patriot / Gattling Cannon | O Pantsir no reparo fixo, com o radar giratório | O radar e os canos |
| Posto Kornet (defesa) | Stinger Site / bunker | O bunker de sacos e concreto com o lançador antitanque | A seteira e o tubo |
| Trincheira e dente de dragão | (novo) | Feitos pelo trator, não comprados: a cobertura | A linha de blocos |

### 5.2 Veículos
| Peça | O papel | Real (com a escapada) | A assinatura |
|---|---|---|---|
| IMR-3M (o trator de engenharia) | Dozer | O trator blindado sobre o T-90, com a lâmina e o braço | O braço e a lâmina em V |
| KamAZ (suprimento) | Caminhão | O KamAZ 6×6 de carroceria com lona | Carrega o dobro |
| T-90M | Tanque básico | O T-90M com a gaiola e o Relikt | A gaiola com a lona da cor |
| BMPT Terminator | Contra infantaria | O chassi de tanque com os 2 canhões de 30 mm e os mísseis | Os canos gêmeos dos lados da torre |
| TOS-1A | Artilharia de área | O lançador termobárico de 24 tubos | O bloco de canos |
| 2S19 Msta-S | Artilharia de longe | O canhão de 152 mm na torre grande | O cano comprido e a torre-caixa |
| Pantsir-S1 | Antiaéreo | O caminhão com o radar, os canhões e os mísseis | O radar redondo |
| T-14 Armata | O pesado (fim de árvore) | A torre sem tripulação e o casco longo de 7 rodas | A torre baixa e retangular |
| Ka-52 | O ar | O helicóptero de rotores coaxiais | Os dois discos |

### 5.3 Infantaria
| Peça | O papel | O que é |
|---|---|---|
| Motostrelok | Fuzileiro | O fuzileiro motorizado (o AK-12, o capacete 6B47 e o colete 6B45: a silhueta do capacete com a capa de camuflagem) |
| Equipe Kornet | Antitanque | A dupla com o lançador no tripé: lenta pra montar, o tiro mais longo da infantaria |
| Spetsnaz | Especial (fim de árvore) | O sniper com o fuzil longo (o Orsis) e o traje ghillie: invisível parado |

### 5.4 Ideias além do ZH (pra discutir)
- **O trem de suprimento:** o depósito ferroviário, no fim da árvore. Um trem passa pelos trilhos do mapa e entrega a carga: é caro, mas não
  morre na estrada como o caminhão. Pede trilho no mapa (os tipos de mapa do WorldForge).
- **A corda explosiva (o UR-77):** a linha de 90 m de explosões que abre campo minado e derruba muro, o efeito-assinatura.
- **A neve como cenário da casa:** os mapas de inverno (`docs/concept/scenes/snow_*`) são a casa natural dela, e o aço frio fica bonito
  na neve. O mapa de neve com a facção de neve.
- **O Lancet:** o drone suicida, lançado do caminhão. Cai no alvo marcado, e a guerra eletrônica da USA ou da própria Rússia derruba.

---

## 6. A produção (a ordem, no mesmo caminho das outras)

1. **A paleta:** os prompts da §7 (A, B, C no GPT, numa folha só, com o tanque das 3 facções anexado pra comparar). Ele escolhe; eu meço.
2. **A ficha do tanque básico** (o T-90M com a gaiola): a folha de ideias (4 designs), ele escolhe, o teste em gameplay, a ficha de 5
   vistas, a cor do jogador.
3. **O modelo provisório por script** do tanque, no esquema de detalhe do elenco (a esteira de elos que anda, as máscaras, a gaiola de
   ripas de verdade, os tijolos do Relikt em relevo), e o Command Center, a fábrica e a usina, pela ficha de cada um.
4. **O elenco inteiro** no `Units.json` (os números pela conta da §4), a IA (o `SkirmishAI` com a árvore dela), as fichas de texto em
   `docs/elenco/russia/`.

---

## 7. Os prompts (prontos pra colar, em chat novo)

### 7.1 A paleta (GPT; anexos: 1 a ficha do tanque USA `docs/concept/units/usa_mbt_player_color.webp`, 2 a da China `china_mbt_player_color.webp`, 3 a da GLA `gla_mbt_sheet.webp`)

```
Faction paint study for a modern real-time strategy game, 16:9, one sheet. A fourth faction, the Russian army, needs a paint scheme that reads clearly against the three existing factions shown in images 1 to 3 (the light grey US tank, the dark olive Chinese tank with fine digital camouflage, the sand-yellow insurgent tank). Show the same modern Russian main battle tank (inspired by the T-90M with reactive armor bricks on the turret front and a slatted steel cage roof over the turret) three times, labeled A, B and C, each from the game's high camera (about 55 degrees looking down) on plain dusty ground at golden hour, with the three existing tanks small in a row at the bottom for comparison. A: cold blue-grey steel, medium brightness, with large black and light grey camouflage blotches about two meters wide. B: medium Russian olive green with large black and sand camouflage blotches. C: plain grey-green with no camouflage. On all three, the team color is a red canvas tarp and net stretched over the slatted cage roof of the turret and a rolled red tarp on the rear engine deck; a small red star with a white outline on the turret side. No letters, no Z or V symbols, no white stripes. Photorealistic AAA game asset renders, battle-worn. No other text than the labels A, B, C.
```

### 7.2 A folha de ideias do tanque básico (GPT; anexo: a paleta escolhida)

```
Concept art design exploration sheet for a modern real-time strategy game, 16:9. Four different designs for the main battle tank of the Russian faction, fictional near-future vehicles inspired by the T-90M but not copies of it, labeled 1 to 4. Design language: heavy, low and wide, modern Soviet lineage: a low turret covered at the front by a wedge-shaped mosaic of reactive armor bricks, a slatted steel cage roof over the turret (the faction's signature, seen clearly from above), side skirts with rubber flaps, a long gun with a fume extractor, smoke grenade launchers. Paint: the scheme from the attached image. Team color: a red canvas tarp and net stretched over the cage roof and a rolled red tarp on the rear deck. Stylized realism like classic real-time strategy games: realistic materials, bold chunky shapes, a strong silhouette and one memorable signature element per design. Each vehicle is shown from the game's high camera (about 55 degrees looking down) on plain dusty ground at golden hour, so its silhouette from above reads clearly. Photorealistic AAA game asset renders, battle-worn. No letters, no Z or V symbols. No other text.
```

### 7.3 As construções (GPT; anexo: a paleta escolhida e a ficha da War Factory da USA `docs/concept/buildings/usa_war_factory*.webp` pra escala)

```
Concept art sheet for a modern real-time strategy game, 16:9: four buildings of the Russian faction base, labeled 1 to 4, seen from the game's high camera (about 55 degrees looking down) on plain dusty ground at golden hour. 1: the headquarters, a block of Soviet precast concrete panels with deep joints, a lattice radio tower and a red star on the front gable. 2: the tank factory, a long industrial hall with a sawtooth roof, a tall sliding door and rails coming out of it. 3: the gas power plant, turbine halls with thick pipes and one tall red-and-white striped chimney. 4: the research institute, a concrete building with a white radar dome and a mast full of electronic warfare antennas. Shared language: grey precast concrete with stains, green-painted corrugated steel, red accents, stenciled Cyrillic signs that are not readable words, blue-grey steel details matching the attached tank paint, small team-color red tarps and banners. Realistic materials, bold shapes that read clearly from above. Photorealistic AAA game asset renders. No other text than the labels.
```

---

## 8. O que eu ainda não sei (e como fecho)
- **Se o aço frio lê no deserto:** a paleta da §7.1, medida contra as 3.
- **O tamanho da gaiola:** cobrir a torre inteira esconde a torre girando. Pode ser só a metade de trás, deixando a torre ler.
  Decide no teste de gameplay.
- **O trem:** depende de o WorldForge pôr trilho nos mapas (o tipo de mapa). Fica como ideia até lá.
