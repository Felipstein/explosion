# Explosões do RTS: variedade, chão e tipos novos (plano de 06/10/2026)

**Pedido do Felipe (06/10):** hoje toda arma do BlastLab é a mesma explosão em
escalas diferentes, e a nuclear está fraca. O jogo precisa de **variedade**:

- veículo que explode (a picape que capota da arte conceitual);
- tiro de tanque;
- fogo que não para de soltar fumaça;
- tanque lança-chamas;
- nuclear de verdade.

Acima de tudo, o **chão tem de reagir**: poeira, areia ou grama conforme o
terreno, o anel de poeira rente ao chão, as marcas.

A referência são as artes conceituais aprovadas (`docs/concept/scenes/` e as
cinco de 06/10). Nelas toda explosão tem:

- um clarão branco-amarelo pequeno;
- um anel de poeira marrom rente ao chão, bem maior que o fogo;
- destroços escuros voando em arco;
- uma coluna de fumaça preta que sobe;
- riscos radiais no chão.

Este documento junta três pesquisas feitas em paralelo:

- o mercado, incluindo os INI originais do Zero Hour, o código das nukes do
  Supreme Commander e do Beyond All Reason, e palestras de VFX;
- a física, com fórmulas e números;
- a técnica na Unreal 5.8, a partir do próprio fonte do motor.

Critérios de sempre:

- icônico e legível na câmera alta, bonito com a câmera deitada;
- não esconder as unidades;
- beleza máxima primeiro, presets depois;
- medir.

---

## 1. O que o mercado ensina

### Zero Hour (SAGE)

Os INI originais são públicos ([GeneralsGamePatch](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI)).
Tudo ali é sprite montado como **lista de camadas com atraso em ms**. O jogo
roda a 30 quadros/s, e um tanque Crusader mede ~30 unidades.

**Tiro de tanque**, tudo orientado pelo cano:

| Camada | Duração |
|---|---|
| chama aditiva | 0.8 s |
| 2 bolas de fumaça, do branco ao cinza | 1.7 s |
| flare | 0.8 s |
| onda de areia deitada no chão | 0.33 s |
| luz | 0.5 s |

E um tranco de câmera.

**Morte de tanque:**

- sorteio: 50% explode em 0.5 s, 50% pega fogo por 2 s e só depois explode;
- 2–4 "braços" invisíveis balísticos, cada um com rastro de fogo;
- 12 pedaços de malha;
- torre lançada quase na vertical (85–90°), girando 300°/s;
- 3 sub-explosões entre 167 e 667 ms;
- onda no chão de 1 s;
- marca queimada no chão;
- carcaça que afunda e some em 8 s.

**Picape Technical** (= a arte 18):

- arremessada a 70–90°, girando 120°/s;
- rastro de fumaça e poeira preso a ela;
- o atirador é jogado longe;
- explode no ar entre 0.5 e 2 s depois;
- solta 4 rodas e 8 sucatas.

**Nuke** (~25 s ao todo):

| Tempo | Camada |
|---|---|
| 0 s | flare + anel de fogo no chão |
| 0.2 s | bola de fogo branco-esverdeada + anel de poeira quase branca de raio ~7 tanques |
| 0.3 s | onda deitada + poeira **indo pro centro** (o vento de retorno) |
| 1 s | anel de fogo no alto |
| 2 s | tronco que converge e sobe + anel do chapéu, do pêssego ao branco |
| — | brilho no chão |
| — | queimado de raio 250 |

### O que deixa o efeito icônico e legível

- **Camadas curtas e defasadas.** O clarão dura 2–3 quadros; cada camada entra
  2–3 quadros depois da anterior.
- **O anel no chão comunica a área do dano.** Todo RTS tem um: brilho de 0.3–1.7 s;
  só a poeira dura mais.
- **A fumaça sobe e sai do nível das unidades.** No WARNO, a coluna preta na
  altura das unidades virou reclamação dos jogadores.
- **Peso visual = peso no jogo** (regra da Riot).
- **Variação por sorteio de roteiro, não só de escala:** atraso, posição,
  força e giro em faixas aleatórias.
- **Assinatura de cor por arma.** Nuke = fogo branco-esverdeado e poeira quase
  branca. MOAB = poeira marrom.

### Outros

- **Supreme Commander e BAR.** Nuke feita de emissores presos a projéteis
  invisíveis: anel interno, onda externa e "spinners" que sobem formando o
  toroide. Cratera e queimado ficam pra sempre.
- **Company of Heroes.**
  - Fumaça e destroços em jatos direcionais.
  - A carcaça fica como cobertura.
  - O lança-chamas é **geometria contínua deformada**: não quebra quando a
    torre gira.
  - As gotas batem no chão e acendem mini-fogos.
- **Battlefield 3/6.** Um mapa de material físico do terreno escolhe o efeito,
  o som e a cratera.
- **Ghost of Tsushima.** "Vortículas": partículas invisíveis de vento que o
  capim e as partículas somam; a coluna quente do fogo vira força.
- **Fallout 4.** O cogumelo é uma sequência cinza colorida por uma rampa
  segundo a idade.
- **Battlefront II.** Explosão em camadas, com fumaça "6-way": 6 mapas de luz
  num atlas.

Fontes: [Schreibt BF2](https://simonschreibt.de/gat/battlefront-ii-layered-explosion/), [Schreibt CoH](https://simonschreibt.de/gat/company-of-heroes-flamethrower/), [Schreibt FO4](http://simonschreibt.de/gat/fallout-4-the-mushroom-case/), [BF3 GDC12](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/gdc12-terrain-in-battlefield3.pdf), [Riot clarity](https://www.leagueoflegends.com/en-gb/news/dev/clarity-in-league/), [GoT vortículas](https://gamedeveloper.com/design/using-vorticles-to-simulate-wind-in-i-ghost-of-tsushima-i-), [FAF nuke](https://github.com/FAForever/fa/blob/develop/effects/Entities/UEFNukeEffectController01/UEFNukeEffectController01_script.lua), [BAR nukes](https://github.com/beyond-all-reason/Beyond-All-Reason/blob/master/effects/nukes.lua).

---

## 2. A física pra calibrar

**Legenda:**

- **[F]** física com fonte;
- **[E]** estimativa;
- **[J]** exagero de jogo aceito.

W é em kg de TNT; λ = W^(1/3).

### Veículo

**Bola de fogo do combustível** (CCPS/TNO) [F]:

- diâmetro D = 5.8·M^(1/3) m;
- duração t = 0.45·M^(1/3) s;
- M = f × combustível, com f ≈ 0.2–0.3 [J].

Exemplos:

| Veículo | D | Duração |
|---|---|---|
| jipe | 16 m | 1.2 s |
| tanque | 34 m | 2.6 s |
| caminhão-tanque | 98 m | 7.6 s |

**Destroços:**

- chapas e rodas a 10–60 m/s [E];
- massas segundo Mott, N(>m) = N₀·e^(−√(m/μ)) [F];
- arrasto: velocidade terminal de 20–45 m/s [F].

**Torre arremessada** (é real: o efeito "jack-in-the-box"):

- sobe 15–20 m: v₀ ≈ 17–20 m/s e 3.5–4 s no ar;
- gira 0.3–1 volta/s.

**Sequência de um tanque:**

| Tempo | O que acontece |
|---|---|
| 0.1–1 s | o propelente pega fogo |
| 0.5–3 s | jato pelas escotilhas, a torre sobe |
| depois | fogo contínuo com estalos de munição |
| horas | carcaça queimando a ~10 MW |

### Tiro de tanque

- **Clarão secundário:** 50–100 ms, bola de 3–8 m [F/E]. Esticar pra 150–200 ms [J].
- **Onda de boca:**
  - levanta poeira densa até ~14 m à frente e véu até 20–27 m, 2–5 m de altura;
  - obscurece 2–4 s (17-pdr: 2.1–3.7 s) [F];
  - pro jogo: não cobrir unidade por mais de 1–2 s [J].
- **Impacto HE:** equivale a 3–5 kg, a magnitude do morteiro. Cratera de 1.2–1.4 m.
- **Perfurante:** clarão branco de 1–10 ms e faíscas de 0.1–1 s; no chão, um sulco e um jato estreito de terra.

### Fogo persistente

Fórmulas:

- **Chama (Heskestad):** L = 0.235·Q^(2/5) − 1.02·D.
- **Pulsação:** f ≈ 1.5/√D Hz.
- **Pluma dobrada pelo vento (Briggs):** z = 1.6·F^(1/3)·x^(2/3)/U, com F = 8.8·Q_c[MW].

Exemplos:

| Fogo | Potência | Chama | Pulsação |
|---|---|---|---|
| carro | 4 MW | 4 m | 0.95 Hz |
| tanque | ~10 MW | 6 m | 0.8 Hz |
| poço de petróleo | ~500 MW | ≥ 40 m | — |

**Fuligem** por kg de combustível:

| Combustível | Fração |
|---|---|
| diesel em poça grande | 0.10–0.15 |
| pneu | ≥ 0.1 |
| madeira | 0.015 |

Madeira e capim dão fumaça cinza-clara.

**Duração:** real de 30 min a horas; no jogo, comprimir pra 1–3 min mantendo a forma [J].

### Lança-chamas

- Jato a 60–90 m/s.
- Alcance real < 150 m (o Crocodile alcançava 110 m).
- Gotas queimam pela lei d²: uma de 3 mm dura ~10 s.
- A poça no chão queima ~1 min.
- Fumaça preta densa.

### Nuclear (W em kt)

**Fórmulas** [F, Glasstone]:

- **Taylor–Sedov:** R = 332·W^(1/5)·t^(2/5) m.
- **Mínimo térmico:** em 3.3·W^0.4 ms (o duplo pulso do clarão).
- **Raio máximo da bola:** ≈ 55·W^0.4 m.
- **Topo da nuvem:** ≈ 3·W^0.3 km.
- **Raio do caule:** ½ do da nuvem até 20 kt.

**Visual:**

- cores branco → amarelo → laranja → vermelho, com a nuvem marrom-avermelhada (NO₂) que embranquece;
- anel de condensação de Wilson só em ar úmido, 1–2 s;
- base surge em cilindro oco;
- precursor de poeira sobre deserto quente;
- vento de retorno puxando a poeira pro centro.

**Escala de RTS** [J]:

- escolher o rendimento pelo anel de dano: 100–200 m ≈ **0.01–0.08 kt**, o porte de uma Davy Crockett;
- alturas por λ e tempos por √λ (semelhança de Froude);
- razão fixa de 1.5–2× entre os anéis.

**O que falta na nossa simulação:** atmosfera estratificada (N²), sem a qual
o chapéu não para nem achata (Morton–Taylor–Turner).

### Chão

**Vento atrás do choque:** u = 63 / 23 / 14 / 10 m/s em Z = R/W^(1/3) = 5 / 10 / 15 / 20 (Kinney–Graham) [F].

**Limiar pra levantar areia** (Shao–Lu) [F]:

- mínimo ~0.24 m/s em grãos de 75–100 µm;
- poeira fina (< 20 µm) só sai pelo impacto de grãos maiores.

**Anel de poeira:** denso até Z = 5–10, véu até Z = 15–20 [E]. Pra 20 kg: 14–27 m e até 40–55 m.

**Corrente de gravidade:** U = 1.2·√(g′h), com g′ pela carga de poeira [F]. Só corre rente ao chão se a poeira pesa mais que o calor a faz subir.

**Decantação (Stokes):** 100 µm cai em 1–3 s; 20 µm fica minutos; < 10 µm, horas.

**Cratera:** Ø ≈ 0.8·W^(1/3) m, profundidade ~0.3·Ø, lábio a 1.25·R, tapete de ejeção até 2.15·R [F].

**Por terreno** [F/E]:

| Terreno | Comportamento |
|---|---|
| areia | pouca poeira fina, grãos saltando rente, assenta rápido |
| terra seca | o máximo, dura minutos |
| grama | pouca poeira, o capim pega fogo |
| asfalto | lascas e pó cinza breve |
| neve | a cratera expõe solo escuro |
| água | spray, coluna e névoa |

**Fontes:** [Glasstone](https://www.fourmilab.ch/etexts/www/effects/), Kinney & Graham, [CCPS fireball](https://myworkspace.dnv.com/download/public/phast/technical_documentation/07_fire_models/Fireball%20Model%20Theory_Review_Validation.pdf), SFPE Handbook (Heskestad, McCaffrey, Tewarson), Briggs 1982, [Shao–Lu / Kok](https://arxiv.org/abs/1201.4353), Huppert & Simpson 1980, Housen & Holsapple 2011, [jack-in-the-box](https://en.wikipedia.org/wiki/Jack-in-the-box_effect), [Crocodile](https://en.wikipedia.org/wiki/Churchill_Crocodile).

---

## 3. O que a técnica permite (Unreal 5.8, GTX 1660)

**1. O Heterogeneous Volume desenha um volume por vez.** O custo cresce em linha reta.

- Cada explosão custa 1.5–4.3 ms (medido).
- Com 8 Cargas Pesadas na tela, o quadro foi de 10.6 a 33.5 ms.
- Dois volumes sobrepostos não se compõem direito (só com uma opção experimental).

Daí: **orçamento de volumes**, e as explosões que passam dele tocam um
**flipbook feito da MESMA gravação**. Uma explosão nunca troca de
representação no meio (a lição da "entrega").

**2. A Unreal não tem 6-way.** Uma função de material nossa resolve; o custo é o de um sprite iluminado.

**3. O WebGL está congelado.** Tipo volumétrico novo (fogo de combustível,
impacto no chão, fonte contínua, nuke) exige **portar o nosso solver**
(`fluid.js`) pra compute na Unreal. A base já existe no `BlastLabShaders`
(o fluido do campo). Seria uma ferramenta de editor que grava a Sparse Volume Texture.

**4. Terreno.**

- Cada camada do Landscape tem um material físico (`LandscapeLayerInfoObject`).
- Na CPU, uma consulta por evento: `GetPhysicalMaterialAtLocation`.
- Na GPU, por partícula, o Niagara lê do terreno: material, cor, altura e normal.

**5. Destroços.**

- Partículas de malha do Niagara na GPU, com colisão contra a altura do terreno (que funciona fora da tela).
- Chaos Destruction não: pesa demais na CPU com várias explosões.
- O veículo arremessado vira **um** corpo rígido: a carcaça (como no ZH).

**6. Niagara Fluids 3D por explosão: descartado** (384³ roda a 20 qps numa RTX 3500 Ada). A própria Epic indica o 2D pra jogo.

**Custos estimados na 1660:**

| Item | Custo |
|---|---|
| flipbook 6-way | 0.05–0.3 ms por efeito |
| destroços | < 0.2 ms por centenas |
| campo de poeira 2D (512²) | 0.1–0.3 ms de simulação + 1–2.5 ms de desenho |
| decals | < 0.2 ms |
| nuclear | 5–15 ms enquanto enche a tela (evento raro) |

---

## 4. A arquitetura proposta

(Revisada depois da segunda opinião: ver a seção 8.)

**Cada arma ou evento vira uma receita de camadas** (como o Battlefront II e
o próprio ZH), disparada por um evento só: posição, direção do tiro, tipo, W,
superfície e semente.

A receita é um **texto** (JSON, como os INI do ZH) que o simulador
**recarrega na hora**. Ele também ganha uma **linha do tempo pausável**:
o Felipe ajusta atrasos e tamanhos clicando.

A receita diz:

| Camada | O que é | Onde vive |
|---|---|---|
| clarão | malha/sprite emissivo de 2–5 quadros + pulso de luz | Niagara |
| corpo | a explosão gravada (volume) ou o flipbook dela | SVT + HV / Niagara |
| destroços | malhas com arco balístico e arrasto, rastro de fumaça, faíscas | Niagara GPU |
| torrões | do material do chão no ponto: terra, grama, lascas, areia | Niagara GPU + terreno |
| anel de poeira | o campo de poeira 2D (abaixo) | compute próprio |
| marcas | cratera + queimado (escala com W, como a física) | decal |
| sequência | sub-explosões, fogo depois, torre, carcaça | a receita (tempos sorteados) |
| câmera | tranco, atraso por d/c na nuke | já existe |

**O campo de poeira 2D é o maior ganho de leitura no RTS.**

- **Modelo:** corrente de gravidade (água rasa com gravidade reduzida pela carga de poeira).
- **Grade:** janela de 512² a 0.5–1 m que segue a câmera.
- **Comportamento:** a frente anda a Fr·√(g′h), freia sozinha, desce vales e se rasga em lóbulos. O anel de cada arma e o base surge da nuke **saem do modelo**, não de constante.
- **Quem alimenta:**
  - explosões;
  - onda de boca;
  - esteiras e rodas;
  - rotor de helicóptero;
  - os vórtices do avião rasante;
  - a nuke.
- **Desenho:** um volume rente ao chão, ralo de cima (espessura óptica ≤ 0.8), com o mesmo ruído advectado das explosões.
- **Cor:** a do terreno de onde a poeira saiu, e ela viaja junto: areia soprada sobre grama continua amarela.
- **Mais:** a mesma velocidade deita o capim.

**Variação:**

- **Sorteio de roteiro, como no ZH:** atraso, força, giro, posição e "explode já ou queima antes".
- **De graça:** semente, giro no eixo vertical, espelho, ±14% de tamanho e o desvio do ruído.
- **Mapa característico:**
  - o vento do mapa inclina todas as colunas pro mesmo lado (coerência);
  - um curl-noise estático por semente muda a silhueta em 10–15%.
- **Receitas diferentes no solver:** as únicas que mudam a física.

---

## 5. Catálogo dos tipos (primeira versão)

| Tipo | Camadas e tempos | Número físico | Exagero de jogo |
|---|---|---|---|
| **Tiro de tanque** | clarão 3 quadros + bola secundária 150 ms; leque de poeira no chão (campo 2D) 1–2 s; fumaça da boca 1.7 s; luz 0.1 s | onda de boca: ~2.6 kg à frente em ±60° | clarão 2× mais longo; poeira ≤ 2 s |
| **Impacto HE** (tanque/morteiro) | clarão, jato de terra em cone, torrões do chão, anel 2D, cratera 1.3 m | W 3–5 kg | — |
| **Impacto perfurante** | faíscas 0.1–1 s, sulco + jato estreito de terra, ricochete | — | faíscas visíveis de cima |
| **Veículo leve** (picape, jipe) | arremessado a 70–90° girando; rastro; explode no ar em 0.5–2 s; 4 rodas + 8 sucatas; fumaça preta; carcaça queimando | bola de 16 m / 1.2 s | carcaça 8 s ou fica (decisão) |
| **Tanque** | sorteio: explode já ou queima 2 s; braços de fogo; 12 destroços; torre a 15–20 m girando; 3 sub-explosões 0.17–0.67 s; bola de combustível; fogo contínuo + estalos | bola de 34 m / 2.6 s; torre a 17–20 m/s | fogo 1–3 min |
| **Fogo persistente** | chama em laço (período = pulsação) + coluna 6-way inclinada pelo vento + luz pulsando; rala nos primeiros ~10 m | Heskestad, Briggs, f = 1.5/√D | duração de minutos |
| **Lança-chamas** | jato de geometria contínua; gotas balísticas → poças em chamas + decal; fumaça preta | 60–90 m/s; poça ~1 min | alcance do jogo |
| **Paiol** | várias fontes defasadas na mesma gravação + faíscas + estalos | — | — |
| **Nuclear** | flash de duplo pulso; bola de fogo; domo do choque; Wilson; anel de poeira e base surge (2D) com vento de retorno; toroide; tronco de poeira; chapéu que para e achata (N²); queimado permanente | Glasstone, escala λ e √λ | 0.01–0.08 kt |

Tamanhos de cratera da física:

| Arma | Ø da cratera |
|---|---|
| granada | 0.8 m |
| morteiro | 1.3 m |
| carga | 2.2 m |
| paiol | 3.4 m |
| ataque aéreo | 4.8 m |

---

## 6. Fases (unificadas com a ordem de serviço do arrepio, 07/10)

Plano único: as fases abaixo + os itens A1–E18 de `docs/CONCEITO_EXPLOSOES.md`
§4d, cada um com o seu critério medido (a sessão de arte mede com a folha
quadro a quadro, captura `cmp_rts`/`cmp_low`). O teste final é o roteiro do
§4b gravado como **vídeo com som**, não a bola de fogo. Alvos por tipo: §4e
(`docs/concept/explosions/type_4.x_*.webp`; anel de referência: o do GPT 4.7;
fogo nunca puxa pro verde, que é do GLA).

| Fase | Itens do §4d |
|---|---|
| 1. Refação (em andamento) | A1 curva física, A2 ritmo (já bate), A3 bola só de fuligem, A4 clarão, A5 tremor + o **evento único** (efeito, luz, tremor, som e marca juntos; som, tremor e reação em d/343 s) |
| 2. Chão e vento | B6 anel de choque → saia, B7 chão por material (jatos, chuva de areia, tabela arma × chão), D15 campo de vento único, a poeira rente como campo 2.5D (tira os ~15 ms da saia do volume); antes, a varredura de zoom do D13 |
| 3. Morte de tanque e picape | B8 faíscas-cometa, chapas e torre; B10 memória (queimado, cratera, carcaça com 1–3 focos, coluna preta 3–20 s); a picape (4.4) |
| 3b. O mundo reage | B9 onda em d/343 (árvores atingidas, vidro, telhados, bandeiras, vizinhos, pássaros): pede os objetos do WorldForge e das bases |
| 4. Porte do solver | D12 gravação herói fina (0.15–0.2 m por voxel; antes, o teste com uma gravação CC0 de alta resolução) |
| 5. Fogo, lança-chamas, nuke | 4.6 napalm, 4.8 tóxico, E16 superarma coreografada |
| 6. Escala | D13–14 volume × flipbook 6-way × marcha conjunta (segunda opinião) |
| 7. Espetáculo | E17 câmera de cinema e replay, E18 fogo no capim, água, prédio (4.5) |
| C. Som (em paralelo) | C11: a sessão de arte especifica os sons e a mixagem; aqui, o gancho no evento (atraso d/343, ouvinte no ponto de mira a ~12 m, mixagem que abaixa o resto) e a reprodução. Downloads de som só com o ok do Felipe |

Cada fase termina testada no simulador, com A/B e vídeo. Os testes devem ser
feitos sobre uma **fatia de deserto do WorldForge**, não sobre retalhos no
campo de teste. Antes de começar, os dois projetos combinam o contrato de
superfície: um material físico por camada do terreno.

Histórico das fases como foram combinadas em 06/10:

1. **[FEITA em 06/10, ver STATUS] O que ele vê clicando, com a gravação atual:**
   - a receita em texto + linha do tempo pausável;
   - clarão em camadas (em ms);
   - destroços e faíscas com tamanho mínimo em pixels;
   - torrões do material do chão;
   - queimado radial carimbado na textura virtual do terreno, com o raio do dano;
   - a **sombra da fumaça no chão** (`r.HeterogeneousVolumes.Shadows`, medir);
   - a luz do fogo no chão;
   - Niagara compondo com o volume (`r.Translucency.HeterogeneousVolumes`, medir).
2. **O campo de poeira do chão:**
   - primeiro o desenho (campo de altura 2.5D próprio) com fonte analítica;
   - o teste de composição com os volumes (2–3 dias);
   - depois a física (jato radial rente com atrito e mistura);
   - a poeira acumulada nas unidades.
3. **A picape da arte 18 e o tiro de tanque completo**, que foi o que ele pediu.
4. **Porte do solver** (validado por curvas no tempo, ±10%) e as receitas novas:
   - veículo;
   - impacto no chão sem a poeira rente, que passa a ser do campo 2D;
   - fonte contínua;
   - 4 sementes por receita.
5. **Fogo persistente → lança-chamas → nuclear.**
6. **Orçamento de qualidade, flipbooks e marcha conjunta**, quando o teste de estresse (10–20 explosões) pedir.

---

## 7. Decisões do Felipe (respondidas em 06/10)

1. **A carcaça:** fica no mapa como cobertura (CoH) nos testes. No jogo de verdade vira configuração.
2. **O fogo persistente:** proposta de 2 min na carcaça, configurável, espalhando pouco e só no visual (10–15 m, ~1 min) onde houver capim. Ele perguntou o que é "espalhar"; falta a resposta final.
3. **Nuke:** "a grande". Estudar um bom tamanho (anel de dano, rendimento e escala).
4. **O teto da gravação:** fica como está (decisão de 06/10: "no jogo vai ser diferente o teto"). A coluna preta das artes vem do fogo persistente (partículas), não do domínio gravado.

---

## 7b. Inspirações: fogo na carcaça (opção b, "igual War Thunder")

| Referência | O que tem de bom |
|---|---|
| [War Thunder, fogo novo (Tusk Force)](https://warthunder.com/en/news/9710-development-new-effects-detailed-cockpits-coming-in-tusk-force-en), [clipes 0:18–0:28](https://www.youtube.com/watch?v=_KWW1jpzAt0&t=18) | chama saindo do deck do motor e das escotilhas (~1 altura do casco), **poça de combustível queimando** ao lado, fumaça preta gorda logo acima, a chama "balança e enrola" (nunca laço parado) |
| [WT × WoT, 5:02–6:12](https://www.youtube.com/watch?v=8BHgYqSsN6s&t=302) | WT: rodas rolando, várias línguas de fogo pelas aberturas, explosão da munição, fogo baixo na base. O WoT engole o tanque numa massa laranja: menos legível |
| WT [Drone Age](https://warthunder.com/en/news/7843-development-new-visual-effects-in-the-drone-age-update-en) / [Sons of Attila](https://warthunder.com/en/news/8451-development-new-visual-effects-in-the-sons-of-attila-update-en) | peças arrancadas com física e rastro, explosão de munição sorteada entre várias, combustível escorrendo em chamas até o chão |
| [CoH3 em câmera de RTS, 11:28](https://www.youtube.com/watch?v=Qx1SA0BPvKc&t=688) | a mais próxima da nossa câmera: fumaça cinza-escura baixa e translúcida, fogo pequeno, manchas queimadas no chão, unidades legíveis, sem coluna alta |
| [Type 69 queimando no deserto (1991)](https://commons.wikimedia.org/wiki/File:Type_69_Operation_Desert_Storm.jpg) | chama pequena (escotilha + poça embaixo), fumaça preta como piche que sobe em bolhas de 2–3 alturas do tanque |
| [NIST, carro queimando](https://www.nist.gov/node/1879391) | pico em 10–18% da duração, cauda longa só de fumaça |
| [Drone, de cima, 1:02](https://www.youtube.com/watch?v=ZksMWDhmQtU&t=62) | de cima e com sol a fumaça é cinza-clara translúcida, não preta; o chão aparece pelas bordas |
| [Skull & Bones 6-way](https://realtimevfx.com/t/smoke-lighting-and-texture-re-usability-in-skull-bones/5339), [Unity 6-way](https://unity.com/blog/engine-platform/realistic-smoke-with-6-way-lighting-in-vfx-graph) | 6 direções de luz + canal de calor: a mesma fumaça de dia e de noite, laranja por dentro em cima do fogo |

**O que copiar:**

1. **Focos, não a carcaça inteira.** Usar 1 a 3 focos (motor, escotilha, poça embaixo), com a chama no máximo da altura do casco.
2. **Pulsação física.** Tremor da chama e da luz a 1.5/√D: ~1.5 Hz na escotilha e ~0.8 Hz na poça.
3. **Cor por altura.** Preto-marrom denso na primeira altura de tanque, com o fundo laranja (canal de calor). Acima disso, cinza translúcido; o topo ao sol fica cinza-claro.
4. **Coluna sem vento.** Alarga em linha reta (~7° de meia-abertura, Morton–Taylor–Turner) e fica mais transparente subindo.
5. **Fases (NIST).**
   - Explosão.
   - Pico do fogo em ~10–18% da duração.
   - Cauda de fumaça fina com brasas.
   - A carcaça escura fica.
6. **Morte estilo WT.**
   - Torre e rodas com física, algumas com rastro.
   - Explosão de munição sorteada.
   - Combustível escorrendo e virando poça em chamas.
7. **Legível como o CoH3.** A fumaça densa fica baixa e o resto translúcido. Medir quanto de uma unidade a um tanque de distância fica escondido, visto de cima.

---

## 8. Segunda opinião (agente, 06/10) e o que mudou

**1. Volume ou flipbook se decide pelo TIPO de arma, nunca pela carga do momento.**

- A barragem mostraria duas qualidades lado a lado: a "entrega" no espaço.
- O flipbook não sente o vento dos aviões e não faz sombra.
- Por isso:
  - granada, tiro e morteiro são sempre Niagara/6-way;
  - carga, paiol, ataque aéreo e nuke são sempre volume;
  - com carga alta, o que cai é a qualidade do volume (passos, resolução, sombra).
- Flipbooks gravados no ângulo da câmera de RTS (impostores em 2–3 faixas).

**2. A poeira rente não é corrente de gravidade na fase que se vê.**

- Com 10 g/m³ de poeira, g′ ≈ 0.08 m/s² e a frente andaria a ~0.6 m/s. O anel real anda a dezenas de m/s, empurrado pelo vento do choque (Kinney–Graham: 63 m/s em Z = 5).
- Modelo certo: água rasa com fonte radial de **quantidade de movimento**, atrito e mistura; a gravidade só manda no fim.
- Calibrar pelo raio do anel no tempo em vídeos reais de impacto no deserto.
- A grade fica **presa ao mundo**, em blocos ativos. Uma janela que segue a câmera apagaria a poeira ao rolar o mapa.

**3. Composição do chão com as explosões.**

- Um volume do tamanho da janela marcha em tela cheia e se ordena mal pelo centroide.
- E a gravação já tem poeira no pé: duas poeiras, com costura.
- Por isso:
  - desenho próprio 2.5D (perfil vertical exponencial, integral analítica);
  - gravações novas sem poeira rente: o campo 2D é o único dono dela;
  - testar antes da fase. Se falhar, antecipar a marcha conjunta.

**4. Faltava, e está em todas as artes:**

- a sombra da fumaça no chão;
- riscos radiais (decal polar procedural na textura virtual do terreno);
- o queimado com o raio do **dano**, porque a cratera física do morteiro dá 2–3 pixels de cima;
- luz do fogo no chão e na poeira, com sombra só nas 2–3 luzes mais importantes (a 1660 não tem MegaLights);
- poeira e fuligem acumuladas nas unidades;
- destroços com tamanho mínimo em pixels;
- variedade:
  - 4 sementes;
  - velocidade de reprodução entre 0.9 e 1.1;
  - leque assimétrico pela direção do tiro;
  - teste com 10 explosões na câmera de RTS, ele aponta se vê repetição;
- a exposição num modo de jogo: teto e adaptação mais lenta, senão a tela pulsa na batalha;
- **contraste no deserto:** a poeira da cor do chão some na areia. A poeira fina é mais clara e menos saturada (medir em fotos), e o deserto das artes é terra seca/silte, com o máximo de poeira.

**5. Números a medir antes de usar:**

- ±14% de tamanho;
- espessura óptica ≤ 0.8 (deixa passar só 45%: definir pelo contraste mínimo das unidades, algo como ≤ 0.3 depois de 1 s);
- tamanho da grade;
- custo de 1.5–4.3 ms por volume (refazer a 1080p na câmera de jogo);
- f = 0.2–0.3 da bola de fogo;
- carga de poeira;
- memória de vídeo de 8 receitas × 4 sementes.

