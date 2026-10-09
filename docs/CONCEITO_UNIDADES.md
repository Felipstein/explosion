# Conceito: unidades

Guia pra criar unidades novas do RTS (o "Zero Hour moderno") seguindo a mesma
identidade de cada facção. Vale pra mim e pra qualquer sessão futura: como
gerar os prompts, o que medir, o que o Felipe aprovou e o que ele rejeitou.
Construções vão ter o guia delas (`CONCEITO_CONSTRUCOES.md`) quando chegarem
no ponto.

As imagens aprovadas estão copiadas em `docs/concept/` (no git): `units/`,
`palettes/`, `scenes/`, `buildings/`. O histórico completo, com tudo que foi
testado e rejeitado, fica em `captures/concept/` (fora do git, só nesta
máquina), uma pasta numerada por rodada.

---

## 1. Ferramentas e como usar cada uma

| Ferramenta | Pra quê | Observações |
|---|---|---|
| **GPT** (ChatGPT, GPT Image) | modelos de unidade, folhas de ideias, explosões, construções | O mais bonito e detalhado. Tende a câmera baixa e unidades grandes (cara de trailer) e a puxar cores pra bege na luz dourada. |
| **Gemini Pro** (site do Gemini) | composição da cena de jogo, fichas técnicas limpas | Acerta a câmera alta e a escala de RTS. Ruim pra construções (sai casinha low poly). |
| Gemini Flash-Lite | cenas rápidas | Bom pra gerar, ruim pra editar. |

Regras que aprendemos na prática:

- **Um chat novo por edição, uma mudança por prompt.** No mesmo chat o
  Gemini devolvia a imagem igual (medido: 93% do azul ficou); em chat novo
  obedeceu na hora. Pedido com duas mudanças, ele faz uma.
- **Recriar em vez de editar.** Edição em cima de edição degrada a imagem, e
  o GPT, quando edita, cai de 1672 pra 1024 px de largura. Pra uma mudança
  pequena: "Recreate the design of the attached image as a new image, ..."
- **"Tirar um pouco" não funciona** no GPT: ele muda quase nada. Diga
  números ("keep at most three crates", "only two lamp posts").
- **Não misturar estilos de gerador numa imagem.** O Gemini colou veículos
  do GPT sem ajustar escala e perspectiva.
- **Anexar a ficha aprovada** sempre que a unidade aparecer (cena, cor,
  variação). Sem a referência, cada gerador inventa outro tanque.
- **Prompt em inglês.** Nunca escrever C&C, Command & Conquer, Zero Hour,
  Generals ou EA: descrever o estilo.
- Limites: Gemini grátis dá ~20 imagens por dia; o Google AI Pro (R$ 96,99)
  dá 100. Codex CLI está instalado (gera imagem pela conta do ChatGPT), falta
  o `codex login` do Felipe. Gemini pela API é pago desde a primeira imagem.
- **Mostrar todos os resultados pro Felipe**, com a minha avaliação. Ele
  escolhe. Não filtrar "só os bons".

---

## 2. Princípios (o que ele aprovou e o que ele rejeitou)

1. **Icônico antes do realismo.** Material realista, forma de RTS clássico:
   silhueta forte vista de cima e um elemento-assinatura memorável. "Inspirado
   no real" sozinho gera coisa genérica, e ele rejeita na hora.
2. **Fictício inspirado no real.** Nunca o veículo real copiado. No prompt:
   "fictional, inspired by the X but not a copy". Também evita processo de
   fabricante (já aconteceu com o Humvee).
3. **Jogável primeiro.** A câmera padrão (e a do competitivo) é alta, a
   55–60°. O jogador pode deitar a câmera até o horizonte (15–20°). Toda
   unidade tem de ser legível de cima e bonita deitada.
4. **Nível de detalhe:** menos que o máximo do GPT, mais que low poly. A
   ficha da USA do Gemini é o ponto certo: formas grandes e claras de cima,
   detalhe fino só pro zoom.
   **Regra (06/10/2026): o detalhe se mede pela câmera deitada (a mais
   perto que o jogador chega); o que vira peça separada se mede pelo que se
   mexe no jogo.** Antes de pedir qualquer detalhe ou peça nova, perguntar:
   isso aparece na câmera? Isso se mexe? Na "foto do jogo" o tanque tem ~150
   px (um elo de esteira ~2 px, uma roda ~15 px); na câmera deitada ~250 px.
   Peças separadas de um tanque: casco, torre (gira), cano (recua), lagarta
   de cada lado; no modelo final, rodas separadas (giram) e esteira com
   textura correndo.
5. **Pintura grande vira brinquedo; a cor do jogador vai em detalhes
   pequenos espalhados, seguindo as placas do tanque** (faixa segmentada,
   algumas placas, anéis no cano) mais um objeto real (lona). Uma peça grande
   só (placa, contorno, listra) ficou feia em todas as tentativas. O GLA é a
   exceção: rabisco de tinta é a identidade dele.
6. **Cores desbotadas e empoeiradas, nunca "bold".** "Bold" no prompt deixa
   tudo com cara de brinquedo.
7. **A facção se lê pelo claro e escuro** da pintura principal (USA clara,
   China escura). A cor do jogador vai nos detalhes; de longe quem ajuda é a
   interface (círculo de seleção, barra de vida).

---

## 3. A bíblia das facções

### USA: clara, fria, tecnológica

- **Pintura:** cinza-claro (quase branco), com detalhes azuis desbotados.
  Referência: `docs/concept/palettes/usa_palette_study.webp` (opção B).
- **Formas:** limpas, anguladas, high-tech. Torre facetada, caixa de sensor e
  laser antidrone com lente azul em cima, lançadores de defesa ativa nas
  laterais, antenas e mastros.
- **Cor do jogador (versão atual, 06/10/2026):** **detalhes azuis pequenos
  espalhados seguindo as placas**: faixa segmentada ao longo do casco e das
  saias, retalhos nas laterais e bordas da torre, dois anéis no cano e uma
  **lona azul amarrada no motor** (o painel de identificação aérea, tipo
  VS-17). Nasceu da "foto do jogo": o GPT pintou assim e o Felipe adorou;
  depois virou ficha no Gemini (`units/usa_mbt_player_color.webp`). A versão
  anterior (dois painéis de lona no teto e no motor, ~9,5% da vista de cima)
  está em `units/usa_mbt_player_color_v1_air_panels.webp`.
- **Nunca:** camuflagem de areia (some no deserto, medido); neon e ficção
  científica; placas grandes pintadas; contorno fino nas bordas (ele achou
  "borocoxó"); V invertido pintado.

### China: escura, massiva, disciplinada

- **Pintura:** verde-oliva escuro em camuflagem digital, **sempre escura**
  (vista de cima mede L ≈ 0,30 contra 0,55 da USA). A China clara do GPT se
  confunde com a USA de longe.
- **Formas:** pesada, quadrada, brutal. Torre angulosa comprida como um
  escudo, gaiola de carga atrás da torre, um canhão só no tanque básico.
- **Estrela:** vermelha por padrão, mas **também troca com a cor do jogador**
  (decisão do Felipe, 05/10/2026).
- **Cor do jogador:** **lonas pequenas, dobradas e presas rente**, uma na
  gaiola atrás da torre e outra atrás do motor, mais a faixa no cano. Mede
  ~3,8% da vista de cima: de perto lê, afastado fica fraco (a interface
  compensa).
- **Nunca:** faixas pintadas (viraram listra de carro de corrida); lona
  grande caída (lençol); canhão duplo (é exclusivo do Overlord); cruz vermelha
  (emblema protegido).

### GLA: improvisada, sucata, guerrilha

- **Pintura padrão:** amarelo-areia fosco com listras verde-escuras gastas
  (`docs/concept/palettes/gla_palette_yellow.webp`). **Alternativa** aprovada: ferrugem escura
  com manchas azul-esverdeadas e listras verde-limão (`docs/concept/palettes/gla_palette_e.webp`). A
  ficha aprovada mistura as duas, e ele quer assim.
- **Formas:** soviética velha (T-55, T-62) com torre redonda de cúpula,
  chapas soldadas, gaiola improvisada, sacos de areia, pneu de estepe,
  metralhadora de picape em cima, peças que não combinam.
- **Cor do jogador:** **rabiscos e listras pintados à mão, bandeirinhas e
  panos amarrados** (o pano no cano é marca da facção). Bandeira é exclusiva
  do GLA: USA e China não usam.
- **Nunca:** pintura branca ou cinza (confunde com a USA); lataria verde
  inteira (puxa pra China).
- **Ponto fraco aceito:** no deserto, o GLA amarelo some de longe, e a
  ferrugem escura fica parecida com a China. Fica pro jogo (interface,
  paleta por mapa, rabisco verde-limão mais forte se precisar).

### Silhueta vista de cima (o que separa sem cor)

| Facção | Torre vista de cima |
|---|---|
| USA | larga e curta, facetada |
| China | comprida, pontuda na frente, como um escudo, com gaiola atrás |
| GLA | redonda, cúpula soviética |

### Unidades reservadas (não usar a ideia em outra)

- **Paladin (USA):** a torre em cunha do design 3 de `docs/concept/units/usa_mbt_exploration.webp`,
  maior e um pouco diferente.
- **Overlord (China):** canhão duplo (ideia em `docs/concept/units/china_mbt_exploration.webp`,
  design 2).
- **GLA, frota planejada:** a picape com metralhadora (tipo Technical), o
  tanque básico (feito), a picape com metralhadora quádrupla antiaérea (tipo
  Quad Cannon) e o trator tóxico.

---

## 4. O que já está aprovado

Caminhos relativos a `docs/concept/`.

| O quê | Arquivo |
|---|---|
| Tanque básico USA (ficha) | `units/usa_mbt_sheet.webp` |
| … com a cor do jogador | `units/usa_mbt_player_color.webp` |
| Tanque básico China (ficha) | `units/china_mbt_sheet.webp` |
| … com a cor do jogador | `units/china_mbt_player_color.webp` |
| Tanque básico GLA (ficha, já com a cor do jogador) | `units/gla_mbt_sheet.webp` |
| Cena oficial de dia, câmera de jogo | `scenes/day_game_cam.webp` |
| Dia, câmera deitada | `scenes/day_tilt_cam.webp` |
| Noite, câmera de jogo | `scenes/night_game_cam.webp` |
| Noite, câmera deitada | `scenes/night_tilt_cam_gemini.webp` e `_gpt.webp` (o chinês favorito dele) |
| Os três tanques em cena | `scenes/usa_vs_china_*.webp`, `scenes/china_vs_gla_*.webp` |
| **Foto do jogo** (tudo junto: fábricas, tanques, cor do jogador, explosão) | `scenes/game_shot_usa_vs_gla.webp` |
| Teste da cor do jogador no tamanho do jogo | `units/usa_player_color_game_scale_test.jpg` |

---

## 5. Como fazer uma unidade nova

1. **Folha de ideias (GPT):** 4 designs lado a lado, na câmera do jogo, com
   a ficha aprovada de outra unidade da mesma facção anexada (pra manter
   pintura e linguagem). Cada design com uma ideia diferente, todos dentro da
   bíblia da facção.
2. **O Felipe escolhe** (às vezes mistura dois). Pergunte o que ele gostou de
   cada um; não decida por ele.
3. **Ficha técnica:** a escolhida em 5 vistas (de cima, câmera do jogo,
   frente, lado, trás), fundo cinza neutro. Confira se as vistas batem entre si
   e se a vista de cima está limpa.
4. **Cor do jogador:** edição em chat novo, em cima da ficha **sem** cor,
   usando o jeito da facção (seção 3). Uma mudança só.
5. **Medir** (seção 7): quanto da vista de cima é cor do jogador, 8 cores,
   tamanho do jogo, visão de longe.
6. **Passada de beleza:** a unidade nova numa cena curta (dois exércitos e
   uma explosão), com as fichas anexadas. Folha de perto e de longe.

---

## 6. Prompts que funcionaram (modelos pra copiar)

Troque o que está entre `< >`. Sempre em chat novo.

### 6.1 Folha de ideias

```
Concept art design exploration sheet for a modern real-time strategy game, 16:9. Four different designs for the <unit role> of the <faction> faction, fictional near-future vehicles inspired by the <real vehicle> but not copies of it, labeled 1 to 4. Design language: <faction shape language from the bible>. Paint: <faction paint>. Stylized realism like classic real-time strategy games: realistic materials, bold chunky shapes, a strong silhouette and one memorable signature element per design. Each vehicle is shown from the game's high camera (about 55 degrees looking down) on plain dusty ground at golden hour, so its silhouette from above reads clearly. The attached <faction> tank is shown next to them for scale and style. Photorealistic AAA game asset renders, battle-worn. No other text.
```

### 6.2 Ficha técnica

```
Model sheet for a real-time strategy game unit: <unit>. It is design number <N> from the attached sheet: same design and same paint (<paint>). Show the same vehicle in 5 views on a plain neutral grey background with even studio lighting: straight top-down view (as the strategy camera sees it), game camera view (about 55 degrees looking down, three-quarter), front view, side view and rear view. Photorealistic AAA game asset render, battle-worn. No text.
```

### 6.3 Cor do jogador (em cima da ficha sem cor)

USA (anexar 1: a ficha sem cor; 2: `units/usa_player_color_reference_from_scene.png`):
```
Edit image 1, the model sheet of the US tank. Keep the tank design, shapes, views and lighting identical. Repaint only the team color, copying the color distribution of the tanks in image 2: the light grey body with many small blue details that follow the armor panel lines: a thin continuous blue band along the whole hull side and side skirts, a few individual side-skirt panels painted blue, two blue rings on the gun barrel, small blue patches on the turret sides, and a blue fabric panel strapped on the rear engine deck. Worn, chipped and dusty paint, like real military markings. The blue details must be visible in every view, including the top-down view. Change nothing else.
```

Lição: a melhor distribuição de cor saiu de uma **cena** (o gerador, livre,
pintou melhor do que nas fichas pedidas peça por peça). Quando uma cena
acertar um detalhe, recortar, ampliar e usar como referência da ficha.

China:
```
Edit the attached model sheet. Keep the tank design, shapes, views, camouflage, red stars and lighting identical. Add the team color as a small, neatly folded red canvas tarp strapped flat on top of the stowage cage at the rear of the turret, about the size of a hatch, not draped over the sides, with straps, dust and worn edges, and a second small folded red tarp strapped flat on the rear engine deck. Keep the existing red band on the gun barrel. Seen from directly above, the two small red tarps are clearly visible. No painted stripes on the turret or hull. Change nothing else.
```

GLA: a cor já nasce no design (rabiscos verdes, bandeiras, pano no cano).
Pra outra cor, pedir o mesmo trocando "green" pela cor.

### 6.4 Passada de beleza (cena)

```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees. Golden hour in a desert town: low warm sun, long shadows, dust in the air. A column of the <faction A> <units> from image 1 (<short paint description>) advances up a cracked road against the <faction B> <units> from image 2 (<short paint description>). Between them, a huge artillery explosion: white-hot fireball, rolling dark smoke, dust and debris. The units match the attached model sheets and are small like in a real strategy game. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

Câmera deitada (anexar a cena de cima aprovada):
```
Same battle as the attached image: same place, same units, same light and the same realistic style. Only the camera changes: lowered to about 15-20 degrees above the ground, just behind the <faction A> units, looking across the battlefield toward the horizon. <horizon details>. The explosion's smoke column rises against the sky. Still an in-game screenshot of the same strategy game, not a movie poster. No HUD, no text, no logos.
```

---

## 7. Como avaliar

### Medições (Python com Pillow e numpy)

- **Visão de longe:** reduzir a imagem 10× (filtro BOX) e ampliar de volta.
  Pergunta: dá pra separar as facções? A cor do jogador aparece?
- **Diferença de cor:** a cor mediana de cada unidade, na imagem reduzida
  10×, convertida pra CIELAB; distância (ΔE) até o chão e até as outras
  facções. ΔE > 20 é bem distinto, < 10 é parecido. O claro e escuro (L*) é o
  que mais separa.
- **Cor do jogador na vista de cima:** recortar a vista de cima da ficha,
  tirar o fundo (flood fill a partir das bordas) e contar os pixels da cor
  (faixa de matiz e saturação). Referências: 0,7% some; 3,8% lê de perto;
  5% lê no zoom padrão; 9,5–15% lê afastado.
- **Trocar a cor sem gerar:** pintar os pixels da cor do jogador com outra
  cor, mantendo a luz (cor × luminância relativa), e montar 8 cores no chão
  da cena oficial, no tamanho do jogo (o tanque da cena oficial tem ~110–150
  px de comprimento numa imagem de 1024 px).

```python
from PIL import Image
im = Image.open('cena.webp').convert('RGB')
w, h = im.size
longe = im.resize((w // 10, h // 10), Image.BOX).resize((w, h), Image.BILINEAR)
```

### Checklist de olho

- A silhueta vista de cima é única da facção (seção 3)?
- As cores batem com a bíblia? O GPT costuma puxar o cinza da USA pra bege e
  clarear a China: confira sempre.
- Nada de facção "contaminando" outra (verde na lataria do GLA, cinza no GLA).
- Parece equipamento militar, não brinquedo?
- As vistas da ficha batem entre si?
- Traçante curto e bem visível, nunca raio laser contínuo.
- Defeitos que se repetem: helicóptero pousado ou caído (Flash-Lite), dois
  veículos voando na explosão, paraquedas de sinalizador gigante, unidades
  enfileiradas como em desfile.

---

## 8. Cenas e atmosfera (pra mostrar as unidades)

- **Imagem de teste é diferente de imagem bonita.** Folha de cor ou cena com
  três facções serve pra decidir e não encanta (ele "broxou" numa delas). Pra
  beleza: prompt curto, uma história só (dois exércitos e uma explosão), cores
  desbotadas, chão e prédios mais calmos que as unidades.
- **A luz depende destas palavras:** "golden hour, cinematic lighting, Unreal
  Engine 5 look". Trocar por "natural colors, not oversaturated" achatou a luz
  das três (contraste −20%, medido).
- **Noite aprovada:** luar azul forte deixando o campo todo legível, mais
  poças de luz laranja dos fogos. A noite "realista" escura engole as
  unidades.
- **Realismo de campo de batalha** (quando pedir): unidades espalhadas usando
  cobertura, crateras, marcas de queimado e de esteira, carcaças queimando.
- A luz bonita dos tiros e do fogo (o forte do GPT) o Felipe considera fácil
  de fazer no motor: não perseguir isso nas imagens.

---

## 9. No jogo (pra quando chegar no Unreal)

- **Cor do jogador:** por padrão a cor da facção (azul, vermelho, verde);
  cada jogador pode trocar. Toda paleta liberada pro jogador tem de passar no
  teste de visão de longe, senão vira vantagem no competitivo.
- **Interface na cor do jogador** (círculo de seleção, barra de vida): é o
  que garante a leitura afastado, como em todo RTS.
- **Paleta por mapa e variações da mesma unidade:** ideia do Felipe pro
  futuro (escura, amarela, ferrugem, E no GLA).
- **Custo:** o detalhe do modelo a Nanite resolve. O que pesa num RTS é a
  quantidade: sombra de unidade que se mexe, infantaria animada, muitas
  explosões juntas (no BlastLab, 1,5–4,3 ms cada uma na 1660 a 720p) e
  memória de textura (6 GB na 1660).
- O gargalo real é a arte 3D: modelo nesse nível é comprado, encomendado ou
  modelado. A IA de 3D, com o tanque inteiro de uma vez, não chega lá; em
  peças (seção 10) chega bem mais perto.

---

## 10. Do conceito ao modelo 3D (AssetHub, 06/10/2026)

O que testamos, o que falhou e o caminho que funcionou. Primeiro modelo:
o tanque básico da USA (`captures/concept/30_to_3d/usa_tank/`).

### O que falhou (não repetir)

- **Tanque inteiro de uma imagem** (Rodin, Tripo): sai derretido, com
  detalhe em bolha e arestas moles. A textura saiu cinza lisa, sem as
  faixas azuis. No Rodin, gerar é grátis mas **baixar exige assinatura**.
- **Multiview automático do AssetHub numa peça separada:** inventa o que não
  vê. Na torre, pôs escapamento, grade de motor e escada atrás, e casco com
  lagarta embaixo. Duas rodadas, as duas ruins, e **custaram ~56 créditos**
  (o contador de créditos atualiza com atraso; não confiar que "foi grátis").
- **Separar peça demais:** separar rodas, roda dentada e elo de esteira é
  detalhe que não aparece (seção 2, regra do detalhe). Paramos na lagarta
  completa.

### O caminho que funcionou: peças

AssetHub (assethub.io) é um quadro de nós que usa Tripo, Hunyuan3D, Meshy,
Hi3D, Trellis e Rodin por dentro. Conta grátis: 4.200 créditos por ano;
pacote extra: US$ 12 por 400. Plano grátis é só pra avaliação (sem uso
comercial).

1. **Workflow** → arrastar a imagem 3/4 da ficha aprovada →
   **Add 3D Production** → **Start 3D Production** (2 créditos) →
   **Confirm & Start** (50). Deixar **Pre-process desligado**. Em "Allowed
   models", desmarcar os de baixa qualidade (Tripo P1.0, Meshy T2 Low Poly,
   Tripo P2.0).
2. Ele separa em peças (cada uma vira um cartão com imagem). **Conferir cada
   imagem ampliada** antes de aceitar. Aceitar é grátis; gerar 3D custa.
3. **Corrigir a imagem da peça antes do 3D**, nunca depois. No cartão:
   **Regenerate/Edit** → texto + **Annotate** (circular a região) →
   **Remove selected region**. Os botões "Extract/Remove" só escrevem uma
   instrução no texto; não criam peça nova. Peça nova: **Add part → From
   text** no nó do 3D Production. Rodar **3D Production numa peça** (pelo
   "+" da saída da imagem) separa ela em sub-peças (foi assim com a lagarta).
4. Erros que pegamos nas imagens: a torre veio com um **toco do cano**
   (virava cano duplo com a peça do cano); o casco veio com as lagartas. As
   duas se corrigem com Edit + texto.
5. **Generate 3D** em cada peça: High poly, **Tripo 3.1**, Parts
   Segmentation Off, Polygon Limit 1.000.000, Smart Low Poly **desligado**,
   Triangle, **Texture Standard (2K) + Geometry Standard = 25 créditos**.
   4K e Ultra não aparecem na câmera do jogo (seção 2) e gastam o dobro.
6. **AI Retopology** (75 créditos): não usar. Serve pra malha que dobra
   (personagem) ou jogo sem Nanite (seção 11).
7. **Download em GLB** → `captures/concept/30_to_3d/<unidade>/parts/<peça>.glb`.
8. Importar: `blast.ps1 py Tools\import_units.py` (cria
   `/Game/Units/<Unidade>/SM_<peça>` com Nanite e imprime o tamanho de cada
   peça). O GLB entra na escala e de pé certos (testado).

Peças do tanque: **casco** (sem lagarta e sem torre), **torre** (sem cano;
só o mantelete com o furo), **cano**, **lagarta completa com as rodas** (uma,
espelhada pro outro lado). Antenas finas: não gerar (derretem); fazer na
Unreal.

Créditos do tanque: separação 52 + edições de imagem ~24 + multiview ~56
(desperdício) + 3D 4 × 25 = ~230.

**Resultado da montagem (06/10/2026): reprovado.** As 4 peças montadas por
medida no Blender (`BlastLab/Tools/blender_assemble_tank.py`, comparação em
`captures/concept/30_to_3d/usa_tank/compare/compare_sheet.jpg`) ficaram no
lugar certo, mas **não parecem o tanque da ficha**: material escuro e sem o
cinza-claro empoeirado, faixas azuis quase sumidas, detalhes diferentes, cesto
de trás da torre virou placa, e a lagarta do Tripo tem proporção errada (no
tamanho certo fura o teto do casco; achatada, as rodas ficam ovais; 6 rodas em
vez de 7). O Felipe: "ficou um lixo, não tem quase nada a ver com as imagens
originais". Eu tinha dito que casco, torre e cano "batiam": julguei só a
posição, não a aparência. **Conclusão: IA de imagem pra 3D (2026) não chega
no nosso conceito, nem em peças.** Pro modelo final, artista 3D usando as
fichas; pra testar luz, escala e explosão no motor, um modelo pronto de
qualidade serve de substituto.

**Ideia pra próxima unidade:** em vez de deixar a IA inventar vistas,
montar as vistas da peça a partir da nossa ficha (o Gemini faz a folha de
vistas da peça sozinha, que já tem a traseira certa) e subir no 3D Studio
nos espaços Front / Left / Back / Right.

---

## 11. Modelos 3D no jogo: dano, polígonos e detalhe por distância

- **Dano não precisa de retopologia.** No ZH o tanque troca de visual por
  estado (inteiro, danificado, muito danificado: textura mais destruída,
  fogo e fumaça) e vira carcaça; a malha não deforma tiro a tiro. O nosso,
  mais bonito: **marcas de tiro e queimado pintadas por cima conforme a vida
  cai** (o material mistura a textura limpa com a de dano), **versão
  destruída** (a carcaça) e **fogo e fumaça do BlastLab** saindo do tanque.
  Amassar o metal de verdade seria destruição da Unreal, muito depois.
  Retopologia serve pra malha que dobra (personagem, tecido).
- **Quantos triângulos:** com o Nanite, o custo de desenhar depende dos
  pixels na tela, não do total do modelo, e 100 tanques iguais usam o mesmo
  modelo na memória. O que pesa é **tamanho e memória**: o cano veio com
  958 mil triângulos (28 MB), desperdício num tubo. Meta: cano ~20 mil,
  casco e torre 100–150 mil. Reduzir na importação (de graça) e comparar
  antes e depois na tela; se não der pra ver diferença, fica a leve.
- **Detalhe por distância (LOD):** o Nanite faz sozinho e de forma contínua
  (de perto tudo, de longe só o que cabe nos pixels). Texturas também: a
  Unreal carrega a resolução alta só do que está perto. Pra batalhas
  enormes, truques de RTS (versões simplificadas e animação mais leve muito
  longe) ficam pra otimização depois.
- **Peças separadas = o que se mexe:** torre (gira), cano (recua no tiro),
  lagartas; no modelo final, rodas (giram) e esteira com textura correndo.
