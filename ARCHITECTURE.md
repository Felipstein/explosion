# Explosão volumétrica — arquitetura e algoritmos

Renderizador WebGL2 sem dependências. Simulação de fluido Euleriana 3D na GPU,
renderizada por raymarching volumétrico com espalhamento múltiplo, iluminando
uma cena PBR rasterizada.

## 1. Simulação (Eulerian grid, GPU)

Grade 3D armazenada como *flattened volume atlas* numa textura 2D (tiles de
`nx*ny`, `nz` tiles). Amostragem trilinear manual = 2 fetches bilineares (HW) +
lerp em Z. Uma única draw call fullscreen por passo do solver.

Campos:
- `vel`     RGBA16F — velocidade (m/s)
- `fields`  RGBA16F — r: fuligem (soot), g: temperatura, b: combustível, a: poeira
- `press`   R16F    — pressão (ping-pong, warm start do frame anterior)
- `div`     R16F    — divergência
- `curl`    RGBA16F — vorticidade ω = ∇×u
- `lightVol` RG16F  — transmitância acumulada (sol, céu)

Pipeline por frame:
1. **Advecção** — semi-Lagrangiana + correção **MacCormack** de 2ª ordem com
   limitador de min/max dos 8 vizinhos (evita overshoot). Reduz drasticamente a
   difusão numérica → detalhe fino preservado.
2. **Combustão limitada por MISTURA**, não por temperatura. Afterburning de
   explosivo é controlado por mistura: o combustível só queima onde já
   encontrou oxigênio, o que confina a reação a uma frente fina na interface
   e deixa o núcleo rico sem queimar. Gatilhar só por temperatura (a versão
   anterior) acende a bola inteira de uma vez e produz um borrão uniforme —
   era a causa principal da explosão não parecer real.
   O oxidante não é um campo advectado: é aproximado pelo ar fresco ainda não
   deslocado pelos produtos, `oxy = 1 − k·(combustível + fuligem·0.45)`, o
   que dá o comportamento certo a custo zero.
2b. **Fuligem com dois lados** — nasce no lado RICO (pirólise) e é oxidada no
   lado POBRE e quente. É por isso que fogo real tem chama limpa de um lado e
   fumaça preta do outro, em vez de produzir fuligem uniformemente.
3. **Resfriamento radiativo com auto-absorção** — dT/dt ∝ −T⁴/(1+k·fuligem).
   O T⁴ puro vale no limite opticamente FINO; uma bola de fogo carregada de
   fuligem é opticamente espessa, reabsorve a própria radiação e esfria bem
   mais devagar. Sem esse denominador o fogo apagava em ~0.5 s.
4. **Empuxo** — F = α(T−T₀)·ŷ − β·ρ_soot·ŷ (gás quente sobe, fuligem pesa).
5. **Vorticity confinement** (Fedkiw et al., *Visual Simulation of Smoke*,
   SIGGRAPH 2001) — F = ε·h·(N̂ × ω), reinjeta a rotação destruída pela
   dissipação numérica. É a fonte dos rolos de fumaça.
6. **Turbulência curl-noise concentrada na INTERFACE** — campo
   divergence-free ∇×ψ (Bridson et al., SIGGRAPH 2007), mas com amplitude
   proporcional a 4·d·(1−d), que tem pico no meio da transição de densidade.
   Rayleigh–Taylor e Kelvin–Helmholtz geram vorticidade na descontinuidade,
   não no volume inteiro; perturbar uniformemente deixa tudo igualmente
   difuso, perturbar a interface é o que produz a superfície em couve-flor.
6b. **Detonação como fonte de PRESSÃO, não de velocidade.** Impor um campo
   radial produz uma casca dura que translada com velocidade constante. A
   detonação real é uma bolha de alta pressão: injetar uma fonte de
   divergência e deixar a projeção resolver dá o escoamento radial com
   decaimento 1/r² e desaceleração natural.
6c. **Estrutura na condição inicial.** Nos primeiros 200 ms a bola de fogo
   tem ~12 células de diâmetro — nessa escala o solver não consegue gerar
   turbulência sozinho. Então a estrutura entra na injeção: deformação do
   raio em duas escalas, borda nítida (um ramp suave sobre 70% do raio é uma
   bola *sem superfície*, e sem interface não há o que instabilizar) e
   mosqueado volumétrico de combustível e temperatura.
7. **Projeção de pressão** — ∇²p = ∇·u por Jacobi, warm-started; depois
   u ← u − ∇p. Fronteira inferior com no-penetration + atrito.

## 2. Renderização volumétrica

- **Emissão por corpo negro**: LUT 1D pré-computada em JS integrando a lei de
  Planck contra as funções de casamento de cor CIE XYZ (aproximação gaussiana
  multi-lobo de Wyman/Sloan/Shirley, JCGT 2013) → sRGB linear. Intensidade por
  Stefan–Boltzmann (T⁴). É por isso que o núcleo vira branco-azulado e as
  bordas caem pra laranja profundo sozinhas — não é gradiente pintado à mão.
- **Integração energy-conserving**: por passo, S = (L − L·e^(−σΔ))/σ
  (Hillaire, *Physically Based and Unified Volumetric Rendering*, Frostbite).
- **Phase function** Henyey–Greenstein anisotrópica.
- **Espalhamento múltiplo** por octaves (Wrenninge et al.,
  *Art-Directable Multiple Volumetric Scattering*): extinção a^i, albedo b^i,
  anisotropia c^i. Dá o "glow" interno de fumaça densa iluminada.
- **Light attenuation volume**: transmitância até a luz pré-calculada por voxel
  → auto-sombreamento da fumaça a 1 tap por passo do raymarch.
- Half-res + jitter com blue-noise + reprojeção temporal + upsample bilateral
  com o depth da cena.

## 2b. Atmosfera e hora do dia

Nada de cor de céu pintada à mão: o ambiente inteiro sai de um modelo de
espalhamento atmosférico, seguindo Hillaire, *A Scalable and Production Ready
Sky and Atmosphere Rendering Technique*, EGSR 2020 (o esquema de Frostbite e
UE5), sobre o meio de Bruneton & Neyret 2008.

Quatro LUTs (`js/atmosphere.js`):
1. **Transmitância** 256×64 — e^(-τ) de uma altitude numa direção até o topo
   da atmosfera. Parametrização de Bruneton. Calculada uma vez.
2. **Multi-espalhamento** 32×32 — colapsa a série infinita de ordens de
   espalhamento numa geométrica, L₂⁺ = L_f/(1−f_ms), integrando sobre uma
   esfera de Fibonacci de 48 direções. Sem ela o céu diurno fica escuro e
   saturado demais (espalhamento simples perde ~40% da energia). Uma vez.
3. **Sky-view** 192×128 — o céu inteiro pra direção solar atual, azimute
   parametrizado *relativo ao sol* (a atmosfera é simétrica em volta dele) e
   zênite com distorção sqrt pra concentrar resolução no horizonte.
   Recalculada só quando o sol se move.
4. **Ambiente** 4×1 — irradiância da luz-chave, irradiância hemisférica do
   céu (cima e rebote do chão) e cor do disco solar, integradas na GPU.
   Fica em textura justamente pra nunca passar pela CPU: um `readPixels` de
   um único pixel custava 21 ms de stall.

O meio inclui ozônio (perfil tenda em 25 km), que é o que faz o crepúsculo
ficar azul-violeta em vez de marrom.

A posição do Sol vem de um modelo solar padrão (`js/celestial.js`):
declinação pelo dia do ano, ângulo horário pela hora, elevação e azimute pela
latitude — então o arco muda com a estação e o hemisfério. A Lua é uma lua
cheia simplificada, oposta ao Sol.

**Uma única luz-chave**: o Sol enquanto está acima do horizonte, a Lua depois.
A troca acontece onde ambas as irradiâncias já são ~0, então não aparece. O
shadow map direcional segue a luz-chave e só é re-renderizado quando ela
realmente muda de direção.

**Exposição analítica**, calculada na GPU (texel 4 da LUT de ambiente) a
partir da iluminância da hora — nunca da imagem, que pulsaria junto com a
explosão. Ver §2c.

## 2c. Noite: luar, visão noturna e exposição

Seguindo Ghost of Tsushima (Patry, *Real-Time Samurai Cinema*, SIGGRAPH 2021
Advances) — a noite tem que *parecer* noite e ainda assim dar pra jogar.

- **Céu de luar físico**: a sky-view espalha também a luz da Lua, na mesma
  atmosfera, com a fase dela. A Lua é a antípoda do Sol (lua cheia), então
  fica no mesmo plano vertical e a simetria azimutal da LUT continua valendo.
  O alfa da LUT guarda a parte lunar (luminância) — o ambiente usa pra
  separar luz solar de luar.
- **Duas escalas**: o render usa uma Lua ~880× mais forte que a real
  (`MOON_ILLUMINANCE`), por precisão de half-float e pra conter o contraste
  fogo/noite. Estrelas, airglow, Via Láctea e o disco lunar são definidos em
  cd/m² físicos e entram com o MESMO reforço, então entre si as razões são as
  reais; o céu solar do crepúsculo não é reforçado, e as estrelas são
  atenuadas pela razão céu-render/céu-físico pra aparecerem na hora certa.
  A exposição e a visão noturna enxergam a escala física: lua cheia
  2.5e-6 do Sol (magnitudes −12.74/−26.74) ≈ 0.29 lux; céu sem lua 0.002 lux.
- **Cor do luar**: luz solar refletida por um solo levemente avermelhado
  (B−V 0.92 contra 0.656 do Sol) → [1.165, 0.976, 0.753]. O azul que o olho vê
  não é do luar, é do Purkinje (Jensen et al. 2001 faz o mesmo).
- **Exposição**: `BASE · (E_ref/E_render) · (E_lux/E_ref_lux)^α`. O primeiro
  fator é compensação total (medidor de luz incidente); o segundo devolve
  parte da diferença — o brilho exibido cai como E^α (papel da Exposure
  Compensation Curve do Unreal / day-for-night do cinema). E_ref fixa o
  meio-dia igual à curva antiga; α = 0.1265 põe o chão sob lua cheia alta
  ~2.75 stops abaixo do meio-dia NA TELA (o pé do ACES escurece os escuros
  além da cena; o Purkinje devolve ~0.35 stop). Ghost fica ~4 stops (medido
  nos slides); num RTS a leitura vem antes.
- **Purkinje shift** (`post.js`, antes da exposição): no escuro os bastonetes
  somam um sinal azul-esverdeado pelas vias dos cones — a cena clareia nos
  escuros, azula e dessatura; com luz o ganho dos cones cai e o efeito some
  sozinho (≥400 lux: nada). Modelo de Cao et al. 2008 como o Patry implementa;
  matrizes geradas por `tools/purkinje.py` a partir de dados espectrais
  (cones Smith-Pokorny, V'(λ) CIE 1951, D65, espectros de Smits/pbrt). As
  constantes m do slide batem com os picos de Smith-Pokorny normalizados em
  trolands (0.6372/0.3920), o que fixou a normalização. A escala absoluta foi
  calibrada contra as capturas do slide 173 a 0.05 lux: direção do desvio
  [−0.03, 0.26, 0.97] contra [0.01, 0.16, 0.99] medido; ganhos cinza 1.61 =
  1.61, folhagem amarela 1.39 ≈ 1.42, grama 1.85 ≈ 1.75.
- **Adaptação às explosões** (`exposure.js`): o medidor vê as superfícies
  (com a luz do fogo nelas) e o volume, nunca céu/sol/lua — o "luminância só
  quando há altas luzes" do Ghost. Média de potência p=0.5 (entre a
  logarítmica do medidor de câmera e a aritmética: explosão pequena e longe
  quase não pesa); alvo 0.45, acima de qualquer cena sem explosão (0.03–0.29
  medido). À noite ela pode fechar até a exposição do dia (+0.5 EV): o chão a
  8 m de uma bola de fogo recebe ~160 klux, mais que o meio-dia — com o
  limite antigo de 2.5 EV a tela inteira estourava de branco por >1 s.

## 3. Cena e integração de luz

- Geometria procedural instanciada (concreto, tambores, tubos, escombros),
  PBR GGX, shadow map direcional com PCF poisson rotacionado.
- **A bola de fogo é uma luz real**: redução GPU do volume de emissão em 4
  passos → centróide + potência + cor, lida assíncrona (1 pixel).
  A redução usa ganho de emissão FIXO (1.0), de propósito: amarrá-la ao
  `emissionGain` do render acoplava a intensidade da LUZ ao ajuste de faixa
  dinâmica do TONEMAP — baixar o ganho pra conter o estouro apagava a
  iluminação da cena junto, e a explosão virava um adesivo flutuante.
- **Luz de preenchimento do fogo** com difusa envolvente, sem sombra
  volumétrica: aproxima o espalhamento múltiplo no ar e na poeira. Sem ela a
  região sombreada vira um buraco preto chapado, que o olho lê como falha de
  render em vez de sombra. Precisa ser envolvente e respeitar o shadow map da
  geometria — a versão uniforme acendia todas as faces e os props pareciam
  emissivos.
- **Sombra dos props pra luz do fogo**: 2 cubos de distância radial (R32F
  512²) compartilhados entre as luzes mais fortes — a bola de fogo nasce rente
  ao chão e precisa enxergar o horizonte (o mapa de 145° pra baixo cobria só
  ~7 m e a sombra só aparecia quando o fogo subia). PCSS de fonte extensa
  (raio da bola de fogo) com PCF bilinear manual (o R32F não filtra). A
  penumbra é larguíssima e sem TAA o PCSS granula; VSM pré-filtrado foi
  testado e não serve (com kernel desse tamanho o chão distante puxa a média
  dos momentos pra trás do receptor e a sombra some). Então o fator é filtrado
  em espaço de tela (`shadowdenoise.js`, bilateral por profundidade e normal,
  como a light attenuation buffer do Unreal): a cena grava por cubo a luz que
  ele multiplica (RGB) e o fator ruidoso (A), e o composite faz
  `cena += L·(filtrado − ruidoso)` — exato na cor. 8+8 amostras; ~1.9 ms com
  uma luz a 983×597.
  Quem ganha cubo: as luzes mais fortes cuja sombra ainda é VISÍVEL contra o
  ambiente — contraste E_f/(E_f + E_amb) ≥ ~3% (E_f a 5 m, E_amb lida da LUT
  de ambiente por PBO assíncrono). O limiar absoluto antigo (potência > 2)
  desligava a sombra à noite assim que o fogo ficava vermelho, com ele ainda
  ~100× mais forte que o luar.
- **Sombra volumétrica na cena**: cada pixel da cena marcha em direção ao fogo
  através do volume de densidade → sombra suave, gigante e em movimento no
  chão. Idem para o sol via `lightVol`.

## 4. Partículas

Simulação GPU (pos/vel em texturas float, ping-pong), 65k partículas advectadas
**pelo campo de velocidade do fluido**, com drag, gravidade e colisão com o
chão. Faíscas coloridas por corpo negro com temperatura própria; detritos
sólidos iluminados pelo fogo; motion blur por esticamento no eixo da velocidade.

## 4b. Orçamento de performance

O custo real por estágio só apareceu depois de três tentativas de medição
falharem, e o registro disso importa mais que os números:

- **Timer queries de GPU mentem** aqui. Elas reportavam `post` a 36 ms e
  `luzFogo` a 26 ms; ambos custam ~0. O que elas mediam era drenagem de fila,
  não trabalho.
- **`gl.finish()` é no-op** neste runtime, e `clientWaitSync` não tem
  permissão de bloquear no Chrome.
- **`readPixels` de um FBO auxiliar não sincroniza nada** — o driver só
  precisa terminar o trabalho daquele FBO.
- O que funciona: `readPixels` de 1 pixel do **framebuffer padrão**. Todo o
  frame desemboca nele, então a espera é real. `App._gpuSync()`.

Com essa barreira, `App.ablate()` desliga um estágio por vez e compara.
Medição em sequência dava uma rampa monótona (clock da GPU subindo), então
as medidas são intercaladas em round-robin com mínimo por estágio.

Custo medido no preset `alta` (112³, render 1382×1037), em ms por frame:

| estágio    | antes | depois |
|------------|-------|--------|
| sim        | 11.4  | 7.8    |
| cena       |  9.9  | 7.6    |
| volume     | 11.1  | 4.6    |
| resto      |  ~0   | ~0     |
| **total**  | **~31** | **22** |

O que mudou:
- **Multigrid de pressão de 2 níveis** — 18 iterações de Jacobi na grade fina
  viraram 16 no nível grosso (8× menos pixels, dobro de alcance por iteração)
  + 3 de suavização no fino.
- **Advecção de velocidade em 1ª ordem** fora do ultra: o limitador
  MacCormack custa 8 `texelFetch` por fragmento, e no campo de velocidade o
  vorticity confinement repõe o detalhe. Nos campos visíveis
  (fuligem/temperatura) o MacCormack fica.
- **Empty-space skipping no volume de luz** (reusa o DDA de blocos macro).
- **Material do chão pré-assado** numa textura tileável — eram ~20 `snoise`
  por pixel, com o campo de altura avaliado 3× pra tirar a normal. Usa
  value-noise periódico, porque simplex não tilea e uma emenda no chão seria
  pior que o custo economizado.
- PCF de 12 → 8 taps, em duas luzes.
- Canvas e resolução de render desacoplados, com **resolução dinâmica**
  mirando um orçamento de frame.

**Nota sobre o painel de preview**: o loop de rAF marca ~250 ms/frame quando
o painel está oculto, contra 22 ms de render medido. Isso é o navegador
congelando uma aba invisível, não custo de renderização — a adaptação de
resolução ignora frames acima de `adaptCeiling` justamente por isso.

## 4c. Armadilhas de filtragem (aprendidas na marra)

Material procedural não tem mipmap. Três defeitos vieram daí, e todos liam
como "renderizador quebrado":

- **Textura assada sem mipmap** → em ângulo rasante um pixel cobre dezenas de
  texels e o chão vira ruído salpicado. `enableMipAniso()` em `gl.js`.
- **Ruído procedural sem LOD** → mesma coisa, mas sem textura pra mipmapear.
  `detailLOD(w, freq)` usa `fwidth` pra medir o pixel em metros e atenua o
  termo quando a feature deixa de caber nele.
- **Atenuação por LOD aplicada com `if`** → um limiar duro sobre valor
  contínuo faz pixels vizinhos alternarem entre dois resultados diferentes,
  o que produz exatamente o salpicado que se queria evitar. A atenuação tem
  que entrar como multiplicação suave.

E uma de escala: bump por diferenças finitas divide por `e`, então o fator de
amplitude não é adimensional. Um "0.30" com `e = 0.03` vira perturbação de
~0.9 — da ordem da própria normal, e a superfície vira normal aleatória.

## 4d. Oclusão de ambiente (`js/ssao.js`)

Estimador tipo Alchemy/SAO (McGuire et al., HPG 2011/2012): espiral de 12
amostras no hemisfério orientado pela normal, reprojetadas em tela, medindo
o quanto cada vizinho se eleva acima do plano tangente. Desfoque bilateral
guiado por profundidade.

Duas decisões que só ficaram claras medindo:

- **Normal GEOMÉTRICA derivada da profundidade**, não a do G-buffer. A normal
  mapeada descasa da geometria que o teste de profundidade enxerga, e o AO
  marcava cada rachadura do normal map do chão como oclusão real.
- **Raio pequeno (~0.7 m)**. Com 2.6 m quase toda amostra cai em chão vazio
  numa cena aberta e o buffer fica branco (média medida: 0.96). Oclusão de
  contato — o sinal que o olho usa pra apoiar o objeto no chão — vive nos
  primeiros centímetros do encontro.

Aplicada só sobre a fração INDIRETA da cor (a cena escreve essa fração no
MRT), com um piso: puramente indireto é mais correto, mas em sol pleno a
fração indireta é pequena e os objetos voltam a flutuar.

## 4e. Tonemap com preservação de matiz

ACES aplicado por canal dessatura highlights em direção ao branco. Com a
exposição alta do crepúsculo a bola de fogo virava uma mancha branca. A
crominância original é reaplicada sobre a luminância comprimida e misturada
de volta — o fogo continua laranja mesmo estourado.

## 4f. Escalabilidade: bake + instâncias

Um RTS dispara explosões várias vezes por segundo. Rodar um solver euleriano
por instância é inviável, e **nenhum jogo faz isso** — o padrão da indústria
(EmberGen, Houdini → engine, e os RTS open-source) é simular OFFLINE e
instanciar o resultado. O simulador é ferramenta de autoria, não de runtime.

Aqui o próprio solver do projeto é o baker (`js/bake.js`):

- Grava 64 quadros num `TEXTURE_2D_ARRAY` — uma camada por quadro, cada
  camada é o mesmo atlas de volume achatado que o resto do código já sabe
  amostrar. 64³, RGBA8, **64 MB**.
- **Amostragem temporal não-uniforme** (t = T·(f/F)^1.7): a bola de fogo muda
  tudo nos primeiros 300 ms, a fumaça tardia quase não muda. Uniforme
  gastaria metade da memória em quadros quase idênticos.
- **Densidade em gamma** (sqrt). Em 8 bits lineares o menor passo seria ~2.6×
  o corte de densidade do solver e a fumaça fina ficaria em degraus.
- O alpha guarda a **transmitância do céu**, que é view-independent: é a
  auto-sombra das instâncias, que não têm volume de luz próprio.
- Grava também a **curva de luz** (centróide, cor, por quadro). Em runtime a
  luz da instância é um lookup interpolado — sem redução de GPU, sem readback.
- Volume **macro** por quadro (16³, R8, 260 KB) pro salto de espaço vazio.

**O bake é um asset, não um passo do boot.** Antes ele rodava síncrono no
construtor: ~16 s de página travada, tela preta e HUD morto. Agora:

- A chave do asset é um hash das fontes que determinam o resultado
  (`fluid.js`, `glsl.js`, `volume.js`, `bake.js`, `volumeRender.js`,
  `blackbody.js`) mais os parâmetros. Mudou o solver, a chave muda sozinha.
- Boot: busca `assets/bake/<hash>.bin` (gzip, ~4 MB contra 168 MB crus) e sobe
  pra GPU. Arsenal pronto ~0,8 s depois de abrir a página.
- Sem asset: bake **incremental** (`begin`/`advance`) com solver e redutor de
  luz DEDICADOS, num escopo de recursos que é liberado no fim. O loop dá
  1–10 passos por frame conforme o frame time; a cena e o HUD funcionam desde
  o primeiro frame. Cada variante fica utilizável assim que termina. Clique
  antes da primeira variante cai na simulação ao vivo. A curva de luz é lida
  por PBO + fence (readPixels síncrono custava ~12 ms por quadro assado). Com
  a aba oculta o rAF para, e um timer assume com blocos grandes.
- No fim, as camadas são lidas em fatias e o asset vai pro devserver
  (`POST /asset`), que apaga os de hash antigo. `?rebake` força o caminho do
  bake.
- Passo fixo de 1/120. Passo adaptativo foi testado lado a lado: idêntico até
  2,6 s, mas a difusão numérica depende do número de passos e a fumaça tardia
  durava ~2 s a mais (e sumia de estalo no fim da sequência).

O bake também independe da qualidade: trocar baixa/média/alta/ultra
reconstrói pool, cena e volume dentro de um `trackGL` (registra todo objeto
GL criado e apaga a geração anterior inteira). Antes cada troca recriava o
bake VAZIO (as explosões sumiam e paravam de iluminar) e vazava ~600 MB de
VRAM.

Runtime (`js/instances.js`): cada instância é só
`{posição, t0, escala, seed}`. Três coisas fazem a conta fechar:

1. **Scissor por instância.** O AABB da caixa é projetado na CPU; o fragment
   shader só roda onde a explosão aparece. Sem isso cada instância custaria a
   tela inteira mesmo ocupando 2% dela.
2. **Empty-space skipping** pelo macro assado. A caixa tem 38 m mas a bola de
   fogo ocupa uma fração dela na maior parte da sequência. Medido: **11 ms
   por instância sem o salto, 0,24 ms com**.
3. **LOD por área em tela**: passos de marcha e interpolação temporal caem
   com o tamanho projetado.

Custo medido (preset `alta`): 12 instâncias 6,7 ms · 48 instâncias 6,6 ms ·
**96 instâncias 5,6 ms**. O custo escala com área em tela, não com contagem.

**Mesma qualidade da simulação ao vivo.** A explosão da tecla espaço é a
referência visual, e a instância foi comparada lado a lado com ela no mesmo
instante (a variante 0 usa a mesma semente e o mesmo passo, então só o render
difere). O que a instância ganhou:

- **Passo do bake = 1/60**, o da ao vivo. A combustão depende do passo: com
  1/120 a instância queimava menos e subia menos.
- **Temperatura que conserva energia** no downsample 128³→64³: a temperatura
  assada é a que reproduz a emissão média do bloco (bisseção na LUT), não a
  média de T, que apagava as frentes de chama finas.
- **Combustível assado** (32³, R8) pro mesmo reforço de frente de chama.
- **Cache de luz por instância** (48³, RG8): as 6 instâncias de maior área em
  tela ganham, por frame, a transmitância até o sol e até a bola de fogo,
  marchada no quadro assado — o mesmo `lightVolume` da simulação. As demais
  usam a transmitância do céu assada. Custo ~1.5 ms pras 6.
- **3 oitavas de espalhamento múltiplo**, como no render ao vivo (custo ~0: a
  marcha é limitada por leitura de textura).
- **Faíscas** (`js/sparks.js`): um estado de GPU com 12 rajadas de 12288
  partículas, escoamento analítico calibrado no campo de velocidade da
  simulação (expulsão radial ~30 m/s que decai em ~0.1 s, pluma só depois de
  ~0.4 s).
- **Luz na cena igual à da ao vivo** (antes era 0.5×), com fonte EXTENSA,
  E = P/(s² + 0.02d²): perto, o brilho é limitado pela radiância da bola de
  fogo; longe, a maior ilumina mais área. E compressão local de barragem:
  cada luz perde (1−κ) do que as vizinhas jogam no ponto dela.

A cena recebe até 8 luzes de instância como pontuais sem sombra volumétrica
(todas compartilham a mesma sequência, então não há volume individual pra
sombrear); as explosões SIMULADAS ao vivo mantêm o caminho completo com
sombra marchada.

## 4f2. Explosões sobrepostas: marcha conjunta

Antes cada instância era marchada sozinha e o resultado inteiro composto na
frente ou atrás da outra pela distância do centro. Com volumes que se
interpenetram (várias pequenas e uma grande em cima) isso põe a fumaça escura
da pequena — que está dentro/atrás da bola de fogo — colada por cima dela.

- **Caixa justa por quadro** (`ExplosionBake.computeBounds`, guardada no
  cabeçalho do asset): onde a fumaça existe de fato. No começo é ~1/12 do
  domínio. Encolhe o recorte em tela e o trecho de marcha de TODA instância.
- **Grupos por interseção 3D** das caixas justas. Instância sozinha (o caso
  comum) vai pelo shader enxuto. Grupo vai pelo shader conjunto: um raio só,
  e em cada passo cada membro que ocupa o ponto entra com seu meio —
  transporte de MISTURA (extinções e emissões somam, espalhamento ponderado
  por σ). Os itens (sozinhas e grupos) são desenhados em ordem de
  profundidade.
- **Luz compartilhada no ponto**: sol = produto das transmitâncias de todos
  os membros (raios paralelos, exato); fogo k atenuado pelo cache de k e, nos
  demais, pela transmitância até k (o cache guarda, além de sol e fogo
  próprio, a transmitância até o fogo VIZINHO dominante) ou pela do céu.
  Fatorado como céu × correções: O(N) por passo.
- O cache de luz de quem está num grupo cobre TODAS as células (a fumaça da
  vizinha pode estar onde esta está vazia); sozinha, pula as vazias. A
  consulta de luz é separada da de densidade — juntas, criavam uma aresta
  reta na face dos blocos macro.
- `boxFade`: a fumaça tardia encosta no teto do domínio da simulação e fica
  cortada reta; uma faixa de 20% no teto afina antes do corte, com a MESMA
  função no cache e na marcha.
- Dither de meio LSB nos valores de 8 bits do bake (curvas de nível na
  superfície do fogo).
- Duas variantes do shader conjunto (até 4 e até 8 membros): a pressão de
  registradores dos arrays por membro custa ~25% no de 8. Grupo maior que 8
  vira pedaços de 4 em ordem de profundidade (aproximado, e barato).

Custo (alta): cenário de 5 pequenas + 1 grande, 14 ms conjunta contra 11,6 ms
uma por vez. Barragem extrema de 15 gigantes interpenetradas: 49 ms contra
25 ms. `inst.params.joint = 1` reproduz a composição antiga pra comparar;
`inst.params.debug` (1 fogo · 2 sol · 3 atenuações · 4 nº de membros) mostra
os termos da luz.

## 4g. Configuração gráfica

`js/settings.js` é a fonte única: esquema das opções (com o que cada uma
exige — `live`, `rebuild` da simulação ao vivo, ou `rebake`), os presets
baixa/média/alta/ultra e a persistência em localStorage. A gaveta do HUD é
montada a partir do esquema. `App.applySettings()` faz o mínimo de trabalho
pra cada mudança; mexer em qualquer opção vira "personalizado".

- **Detalhe das explosões** = resolução do bake (48³/64³/80³/96³, 74–587 MB de
  vídeo). Cada resolução tem seu asset `assets/bake/r<res>-<hash>.bin`; o
  devserver mantém o último de cada uma. A redução da simulação (128³) pra
  grade assada aceita razão fracionária (subamostras trilineares na pegada do
  voxel; em 64³ é idêntica à média de blocos).
- **Custo medido** (cena com 15 explosões, ablação intercalada): o dominante
  é a marcha das instâncias (56→32 passos: −7 ms; volume 72%→50%: −3 ms). O
  bloqueio da luz de uma explosão pela fumaça de outra custava ~20 ms (luzes ×
  oclusores marchas por pixel) e virou opção, ligada só no ultra.

## 5. Post-processing

Bloom progressivo down/upsample com filtro de 13 taps e média de Karis
(Jimenez, *Next Generation Post Processing in Call of Duty: Advanced Warfare*),
streak anamórfico, tonemap ACES (fit de Stephen Hill), aberração cromática
radial, distorção por calor (refração pelo gradiente do volume), grain e
vinheta. Câmera cinemática com shake de choque amortecido.
