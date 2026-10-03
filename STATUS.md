# Estado atual — 02/10/2026

Rodar: `python3 -m http.server 8129` na raiz e abrir http://localhost:8129
(ou o preview `explosion` do `.claude/launch.json`).

## O que já está funcionando

Pipeline completo de ponta a ponta, sem dependências, WebGL2 puro:

- **Solver de fluido 3D na GPU** (`js/fluid.js`) — advecção MacCormack com
  limitador, combustão por coordenada de reação, resfriamento radiativo T⁴,
  empuxo, vorticity confinement, turbulência curl-noise (textura 3D tileável
  pré-computada), anel de vórtice toroidal na detonação, surto de base com
  escala Taylor–Sedov (R ∝ t^0.4), projeção de pressão por Jacobi com warm
  start e fronteiras abertas (Dirichlet nas laterais/topo, Neumann no chão).
- **Emissão de corpo negro física** (`js/blackbody.js`) — LUT integrando
  Planck × CIE (fit gaussiano de Wyman/JCGT 2013) → sRGB linear. Verificada:
  1400K sai vermelho-profundo, 3500K branco-amarelado, intensidade caindo 5
  ordens de grandeza. Nenhum gradiente pintado à mão.
- **Raymarch volumétrico** (`js/volumeRender.js`) — integração
  energia-conservante por passo, espalhamento múltiplo em 3 octaves, phase
  Henyey–Greenstein dupla, meia resolução com jitter + upsample de
  profundidade-mais-próxima, distorção por calor.
- **Empty-space skipping com DDA exato** por blocos macro — salta até a
  fronteira do bloco, nunca atravessa detalhe fino.
- **A bola de fogo é uma luz real** — redução GPU do volume de emissão em
  cadeia 4×4 até 1×1, lida via PBO + fenceSync (assíncrono, sem stall).
- **Cena PBR** (`js/scene.js`, `js/layout.js`) — 376 props instanciados,
  materiais procedurais, shadow map do sol + shadow map dinâmico do fogo +
  sombra volumétrica marchada pelo volume de fumaça, céu procedural e névoa
  analítica de altura que usa a MESMA função do céu.
- **Post** (`js/post.js`) — bloom progressivo com média de Karis, streak
  anamórfico, ACES, aberração cromática, vinheta, grain, dither.
- **Hora do dia completa** (`js/atmosphere.js`, `js/celestial.js`) — modelo
  atmosférico de Hillaire 2020 em 4 LUTs, posição solar real por
  latitude/dia/hora, luz-chave única sol→lua, exposição analítica,
  estrelas/Via Láctea/lua. Slider + presets + ciclo automático na UI,
  teclas `,` `.` `t`. Validado de meia-noite a meio-dia.
- **Ferramentas**: `__app.prof.totals()` (timer queries de GPU, uma por
  seção por frame), `__app.bench()`, `__app.renderAt(t)`.

## Onde parei — os dois problemas abertos

### 1. Performance — 31 → 22 ms/frame (45 fps) no preset `alta`

Resolvido o suficiente. Detalhes e tabela em `ARCHITECTURE.md` §4b.

Principal armadilha, registrada pra não se repetir: **os timer queries de GPU
estavam mentindo** (apontavam `post` e `luzFogo` como gargalos; ambos custam
~0). A medição confiável é `App.ablate()`, que usa `readPixels` no
framebuffer **padrão** como barreira de sincronização.

Se precisar de mais: `sim` (7.8 ms) e `cena` (7.6 ms) são os maiores. Para
`sim`, o caminho é desacoplar a taxa da simulação da taxa de render (fumaça
não precisa de 60 Hz). Para `cena`, assar também o material dos props.

### 1b. Realismo da transição explosão → fumaça — reescrito em 01/10

A crítica era que fumaça e fogo estavam volumosos mas "a explosão em si
virando fumaça" não convencia. Diagnóstico feito com as ferramentas novas
(`__capture` e `__fields`, ver abaixo): os CAMPOS estavam lisos — não era o
renderizador escondendo estrutura.

Mudanças, em ordem de impacto:
1. Combustão limitada por mistura (antes: por temperatura). Ver ARCHITECTURE §1.
2. Fuligem com formação no lado rico e oxidação no lado pobre.
3. Detonação como fonte de pressão em vez de velocidade imposta.
4. Turbulência concentrada na interface de densidade.
5. Estrutura na condição inicial (borda nítida + mosqueado volumétrico):
   nos primeiros 200ms a bola tem ~12 células e o solver não gera
   turbulência sozinho nessa escala.
6. Empuxo rebalanceado — `sootWeight` 16 quase anulava `buoyancy` 52 e a
   coluna não subia. A massa de fuligem é ínfima; foi pra 3.4.
7. Resfriamento com auto-absorção (o fogo apagava em 0.5s).
8. Faixa dinâmica da emissão: a superfície estava ~10× acima do joelho do
   ACES, então toda variação de temperatura virava o mesmo branco.

Ainda imperfeito: os primeiros ~0.3 s continuam saturando em branco, e a
coluna de fumaça tardia é macia demais. Próximos passos seriam detalhe
sub-grid mais agressivo na fumaça e, se houver orçamento, resolução maior
só durante a fase quente.

### Ferramentas de inspeção

- `__capture('x.png', {times:[...]})` — folha de contato da evolução
  temporal com a câmera TRAVADA. Grava via POST pro `devserver.py`, então
  funciona mesmo com a janela do app minimizada.
- `__fields('x.png', {t:0.2})` — o mesmo instante em vários CAMPOS
  (temperatura, fuligem, combustível, frente de chama, emissão). Distingue
  "a simulação está lisa" de "o renderizador está escondendo a estrutura".
  Cuidado: o raymarch corta por opacidade, então nos modos de debug o
  early-out é desligado — senão só se vê a casca externa.

### 2. Escala física do evento — RESOLVIDO em 01/10

Aplicado: `blastSpeed` 36→19 com `drag` 0.06→0.55, `cooling` 1.75→0.72,
`burnRate` 2.6→1.45, `surgeSpeed` 16→6.5, `DOMAIN` 27→31 m, shot da câmera
reenquadrado pra ~40–52 m, `domainFade` estreitado. A explosão não transborda
mais o quadro e a bola de fogo sobrevive alguns segundos.

Ainda dá pra melhorar: a fase inicial ainda lê como um disco achatado por
causa do surto de base; vale separar melhor a bola de fogo (que sobe) da
poeira rasteira (que espalha).

## Qualidade de imagem / integração — rev. 3 (01/10)

Crítica: fogo e fumaça pararam de projetar sombras bonitas e a explosão
parecia "um PNG colado", não ambientada.

Causa raiz: a intensidade da LUZ da bola de fogo era derivada do mesmo
`emissionGain` que controla o RENDER. Ao baixar o ganho pra conter o estouro
de branco, a luz da cena caiu junto — `fireColor` tinha ido a 0.03 contra
~0.7 de um raio de sol. Desacoplado (a redução de momentos usa ganho fixo).

Também entrou:
- Sombra volumétrica mais profunda (`fireTauCap` 1.6 → 3.2).
- Luz de preenchimento do fogo com difusa envolvente (ver ARCHITECTURE §3).
- Erosão de borda modulada por ruído, ATENUADA pela densidade — a primeira
  versão subtraía valor absoluto e apagava a pluma tardia inteira.
- **Sistema de brasas** (`js/particles.js`), que faltava desde o começo.
- Grain reduzido e uniforme (pesar pras sombras destruía a cena noturna).

Performance mantida: ~70 fps no preset `alta`.

Ainda aberto:
- Os primeiros ~0.3 s continuam saturando em branco.
- Sem SSAO — o MRT de normal + fração-de-ambiente existe e segue sem
  consumidor; é o que daria sombra de contato nos props.
- Volume ainda em meia resolução; reprojeção temporal nunca feita.
- A fumaça tardia rarefaz bastante depois de ~4 s.

## Qualidade de imagem — rev. 4 (01/10)

Crítica: "parece 2012, parece um PNG, não faz parte do ambiente". Quatro bugs
distintos, nenhum deles no simulador:

1. **Tempestade de aliasing.** O chão assado não tinha mipmap, e o ruído
   procedural dos props não tinha LOD nenhum. Em ângulo rasante cada pixel
   cobria dezenas de texels → a cena inteira salpicada. Corrigido com
   `enableMipAniso()` (mipmap + anisotropia 16×) e `detailLOD()`, que atenua
   cada termo de alta frequência pelo tamanho do pixel em metros via
   `fwidth`.
2. **Normais aleatórias nos props** (o padrão "dálmata"/isopor). O bump por
   diferenças finitas divide a amplitude por `e = 0.03`, então o fator 0.30
   produzia perturbação de ~0.9 — da mesma ordem da própria normal. Além
   disso a atenuação por LOD entrava como `if (ld > 0.02)`, um limiar duro
   sobre valor contínuo: pixels vizinhos alternavam entre perturbado e liso.
   Agora amplitude 0.02 e LOD por multiplicação suave.
3. **Contorno escuro na bola de fogo** — o sinal real de "adesivo". O canal
   de poeira ABSORVE mas não emite, e a poeira colada na bola recebia
   transmitância ~0 em direção ao fogo (mesma quebra da aproximação de luz
   pontual já corrigida na cena, esquecida no volume de luz). Resolvido com
   `tauCap = 2.2` no canal de fogo do volume de luz.
   Também corrigido um bug no upsample: `abs(hd - min(dFull,1e8))` clampava
   só um lado, então céu-contra-céu dava 9e8 e o teste caía em point-sampling
   bloquiado exatamente na silhueta.
4. **Fuligem sendo consumida.** `sootOxid` 1.25 comia quase toda ela — a bola
   de fogo ficava sem o que emite e sobrava pouca fumaça. Baixado pra 0.30.

Também: perspectiva aérea aplicada ao volume no composite. A radiância do
volume era somada crua, sem atravessar ar nenhum, enquanto tudo atrás dela
atravessava 60 m — contribuía pro efeito de "colado na frente".

Performance: ~82 fps no preset `alta`.

Ainda aberto: sem SSAO (sem sombra de contato); volume em meia resolução;
primeiros ~0.3 s ainda saturam; a cena ainda é "limpa" demais (falta sujeira
e variação de material).

## rev. 5 (01/10) — pendências fechadas

- **SSAO** construído (`js/ssao.js`) e consumindo o MRT de normal +
  fração-de-ambiente que existia sem uso desde o início. Ver ARCHITECTURE §4d.
- **Resolução do volume** virou parâmetro de qualidade (`volScale`):
  alta = 0.72, ultra = 1.0. Era fixo em 0.5 e era o maior responsável pela
  maciez da fumaça.
- **Saturação dos primeiros 0.3 s** — ganho de emissão reduzido, brilho
  devolvido pelo bloom, e preservação de matiz no tonemap (ARCHITECTURE §4e).
- **Sujeira e variação de material** — manchas de óleo e areia no chão em
  escalas diferentes da do tile (evita repetição visível); pó assentado nas
  faces viradas pra cima, encardido acumulado na base dos props e escorrido
  vertical nas laterais.

Performance: ~47 fps no preset `alta` (era 82 antes destes quatro).

Pendências reais que restam: reprojeção temporal no volume; a cena ainda não
tem decalques nem variação de geometria; os embers não iluminam a cena.

## Bug corrigido — sombra fantasma (01/10)

Sintoma: depois da fumaça sumir, a sombra dela continuava no chão e ia
desaparecendo devagar.

Causa: inconsistência entre render e sombra. `sampleMedium()` aplica erosão
modulada por ruído na densidade antes de renderizar; `volumeShadow()`
marchava o campo CRU. Fumaça residual já apagada da imagem continuava
projetando sombra, e sumia junto com o decaimento lento da fuligem.

Correção em duas camadas:
1. `effExtinction()` em `glsl.js` — uma única definição de "densidade
   efetiva", usada pelo render, pela sombra E pelo volume macro. A sombra usa
   a MÉDIA da erosão (o ruído tem média zero), que basta: ela não precisa do
   detalhe de alta frequência, só precisa concordar em quanta densidade há.
2. Corte duro de resíduo no solver (`minDensity`). Decaimento exponencial tem
   cauda infinita — sobrava densidade baixa demais pra ver e alta o bastante
   pra sombrear. Num RTS isso acumularia a cada explosão.

Lição geral: qualquer efeito aplicado no render e não na simulação cria
divergência entre o que se vê e o que o resto do pipeline acredita existir.

## HUD + detonação por clique (01/10)

Sidebar no estilo C&C (`index.html`), com painéis em metal biselado e acentos
âmbar:
- **Armamento** — cartão com thumbnail. O ícone é um RENDER DO PRÓPRIO MOTOR
  (preset ultra, crepúsculo, enquadramento fechado, recorte 4:3 → 256×192),
  não arte externa: ele sempre reflete como a explosão realmente parece.
  Pra regerar depois de mudar o look, ver a receita no fim desta seção.
- **Ambiente** — slider de hora com gradiente dia/noite, readout em LED verde
  com hora + elevação solar + fase, presets e ciclo automático.
- **Sistema** — qualidade e telemetria ao vivo.

**Clique no terreno detona ali.** O que isso exigiu: o domínio da simulação
era cravado como `#define` na origem do mundo. Virou `uDomainOrigin`,
declarado uma única vez em `COMMON` (várias grades compartilham o domínio e
seriam declarações duplicadas no prelude sufixado) e propagado pros 8 shaders
que convertem mundo↔voxel. A caixa de simulação agora acompanha a detonação.

Arrasto continua orbitando: o clique só detona se o cursor andou menos de 5px.

**A câmera NÃO acompanha a detonação** — num RTS ela fica onde o jogador
deixou e a explosão acontece onde acontecer, inclusive fora de quadro.

Receita do thumbnail (console):
```js
const {heroShot, saveSheet} = await import('/js/contactsheet.js');
__app.setQuality('ultra'); __app.setTimeOfDay(18.1);
const big = heroShot(__app, {t:0.95, w:1024, seed:4242,
  view:{dist:26, azim:-0.55, elev:0.055, targetY:5.2}});
// recorta 4:3 em 256x192 e salva como assets/thumb_blast.png
```

## Múltiplas explosões simultâneas (01/10)

Havia UMA simulação e `detonate()` a resetava: clicar num segundo ponto
apagava o primeiro. `js/blastpool.js` resolve com um pool de slots, cada um
com sua simulação e suas partículas.

Todos os slots compartilham resolução e tamanho de domínio e diferem só na
ORIGEM — por isso os shaders são idênticos e basta trocar texturas e
`uDomainOrigin` entre as passagens.

**Política de passo.** Simular N fluidos por frame seria N× o custo. Só a
fase quente (primeiro ~1.2 s) precisa de 60 Hz; depois a fumaça é lenta e um
passo a cada K frames com dt K× maior é visualmente equivalente — advecção
semi-Lagrangiana é incondicionalmente estável, então dt maior não
desestabiliza. Resultado medido: 3 explosões custam 1.7× uma, não 3×
(13.6 ms → 23.4 ms; 73 → 43 fps).

**Composição volumétrica front-to-back.** Um alvo só, limpo em (0,0,0,1), e
cada volume desenhado do mais próximo ao mais distante com
`blendFuncSeparate(DST_ALPHA, ONE, ZERO, SRC_ALPHA)`:
RGB acumula radiância atenuada pelo que está à frente, A é o produto das
transmitâncias. No alvo auxiliar o alpha fica 1, então o mesmo blend vira
soma pura (profundidade ponderada por cobertura e calor somam).

**Cena.** Sol e cada bola de fogo são ocluídos por TODAS as explosões. O
teste de caixa rejeita na hora quem não está no caminho, então volume
inativo custa quase nada. O shadow map do fogo cobre só a explosão principal.

Armadilha: **array de sampler exige índice constante** neste driver — índice
de laço não conta, apesar do que a spec ES 3.0 sugere. A consulta por
explosão é desenrolada em JS na montagem do shader.

## Escalabilidade (01/10) — bake + instâncias

O pool de simulações resolvia "mais de uma explosão" mas nunca escalaria pra
RTS. Arquitetura correta implementada: o solver vira BAKER, o runtime
instancia. Detalhes em ARCHITECTURE §4f.

Medido: 96 instâncias simultâneas a 5,6 ms. O custo escala com área em tela.

O pool ao vivo continua existindo (tecla `espaço`) como caminho "hero" e como
fonte do bake. Clique no terreno usa o caminho instanciado.

Pendências desta camada:
- Instâncias não projetam sombra volumétrica no terreno (só luz pontual).
- Qualidade do bake é 64³ contra 128³ do ao vivo; dá pra subir se a memória
  permitir (96³ seriam ~177 MB).
- Falta variação: hoje todas instanciam a MESMA sequência, variando só
  escala, seed de ruído e velocidade. Assar 2-3 sequências diferentes
  removeria a repetição.
- Múltiplas magnitudes (granada/tanque/depósito) ainda não existem.

## Camada final entregue (01/10)

Tudo que estava listado como pendente desta fase foi fechado:

- **3 variantes** assadas no mesmo texture array (layer = variante×quadros +
  quadro), um sampler só. 56 quadros cada, 168 camadas, **168 MB**. Sem
  variantes uma barragem repete o mesmo desenho visivelmente.
- **5 magnitudes** (`MAGNITUDES` em `instances.js`) com leis de escala
  corretas: comprimentos E tempos vão com W^(1/3), então magnitude =
  {escala espacial, velocidade de reprodução} e a física sai certa de graça.
  Intensidade da luz com a área, W^(2/3).
- **Sombra volumétrica das instâncias** no terreno. Como todas compartilham o
  texture array, custa 2 samplers no total — independente de quantas houver.
- **HUD com 5 cartões**, cada thumbnail renderizado pelo próprio motor a
  partir da sequência assada (se o visual mudar, o ícone muda junto).

Armadilha que custou tempo: a luz de uma explosão estava sendo ocluída pelo
PRÓPRIO volume dela. A aproximação de luz pontual coloca a fonte no centro da
bola de fogo, então o raio saía atravessando ela inteira; com 4 oclusores
empilhando um teto de 4%, 0.04⁴ ≈ 2.6e-6 e o terreno ficava preto absoluto.
Luzes e oclusores agora carregam identidade e o sombreador pula a si mesmo.

## Bug "o chão parou de brilhar depois de algumas explosões" (02/10)

Não reproduzia com passos síncronos — só no loop real. Causa: várias portas
abertas que se somavam numa sessão de uso normal.

1. **Câmera em `cine` por padrão.** O shot é indexado por `this.time`, que o
   clique nunca reseta; em segundos o alvo sobe até 22 m e a câmera passa a
   olhar pro céu. Padrão agora é `free` (estática, RTS), elevação limitada
   entre 0.06 e 1.35 rad.
2. **Clique perto do horizonte**: raio quase paralelo ao chão, t = −y/dy
   explode e a explosão nascia a 1.4e12 m. Agora há `maxClickRange` (260 m).
3. **`realDt` sem piso** + shake multiplicado por exp(−dt·k): um único dt
   negativo faz o shake crescer exponencialmente (câmera chegou a 1e10 m).
   Piso em 0 no loop e no `update` da câmera, shake limitado a 1.5.
4. **`autoReplay` ligado** detonava a simulação ao vivo no centro a cada 15 s
   — resto da fase de explosão única. Desligado.
5. **Queimado global** cravado na origem e amarrado ao tempo global. Agora uma
   marca persistente por explosão (até 16).
6. **As 5 thumbnails deixavam 5 queimados na origem** no startup.
7. **`safeLoop` só protegia o 1º frame** (o loop re-agendava a si mesmo).
8. **HUD transbordando**: 5 cartões empurravam Ambiente/Sistema pra fora da
   tela. Grade de 2–3 colunas como a barra do C&C, `minmax(0,1fr)`.

Lição: teste síncrono não substitui sessão real. Três desses bugs só existem
com tempo global avançando, input do mouse e o rAF de verdade.

## Boot lento + "parou de emitir luz" (02/10, tarde)

- **Tela preta ~16 s no boot**: o bake rodava síncrono no construtor (3
  variantes × 720 passos de 128³). Virou asset em `assets/bake/<hash>.bin`
  (gzip, 4,4 MB) carregado em ~0,26 s; sem asset, bake incremental com o HUD
  vivo e o "relógio de construção" do C&C nos cartões. Detalhes em
  ARCHITECTURE.md §4f.
- **Explosões sem luz depois de trocar a qualidade**: `_buildSim` recriava o
  bake vazio e nunca rodava, e não liberava nada (~600 MB por troca). Agora o
  bake e as instâncias sobrevivem à troca, e a geração anterior é liberada por
  `trackGL`. A troca também não detona mais nada no centro.
- **Solver dependente do passo**: fade de borda (×0.955) e dissipação de
  velocidade (×0.999) eram por PASSO; agora são por segundo (ref. 1/120,
  idêntico nesse passo). Afetava explosões velhas do pool, que avançam com dt
  maior.
- Verificado: troca de qualidade 10× seguidas sem erro GL, instâncias e
  simulação ao vivo iluminando o chão à noite, asset carregando do zero.

## "Às vezes não dispara luz" / chão some (02/10, noite)

Não era a luz. Depois que uma explosão AO VIVO (tecla espaço, clique antes do
arsenal ficar pronto, ou a antiga troca de qualidade) expirava, os samplers
`uFieldsArr[i]`/`uMacroArr[i]` da cena continuavam apontando pra unidade de
textura que o bake (sampler2DArray) passa a usar. Dois tipos de sampler na
mesma unidade → `INVALID_OPERATION` → os 3 draws da cena (chão, prédios,
cilindros) descartados em todo frame, pra sempre. Só sobrava céu + explosão.

- Fix: slots sem explosão apontam pra uma textura 1×1 vazia (scene.js).
- O loop agora checa `gl.getError()` a cada 120 frames e loga no console —
  erro de GL não lança exceção e era invisível.
- Teste: sessão de ~2000 frames com instâncias de todos os tipos a cada
  0,4 s, explosões ao vivo a cada 3 s, troca de qualidade e de horário:
  0 erros de draw (antes: quebrava assim que a 1ª ao vivo expirava).

## Explosão do clique com a qualidade da tecla espaço (02/10, noite)

Comparação lado a lado no mesmo instante (variante 0 = mesma semente da ao
vivo). Diferenças achadas e resolvidas: passo do bake (1/120 → 1/60), média de
temperatura apagando a chama (agora conserva energia), sem combustível, sem
volume de luz direcional (cache de luz por instância), 1 oitava de
espalhamento (agora 3), sem faíscas (sparks.js, 12288 por explosão — a ao vivo
tem 12288, não 4096: o pool ajusta `count` depois da textura criada), luz na
cena pela metade, e o salto de espaço vazio das variantes 1 e 2 usando o macro
da variante 0. A diferença que sobra é resolução (64³ assado contra 128³ ao
vivo): detalhe fino um pouco mais suave.

Ferramenta: `app.hold = true` congela o loop real, pra capturar quadro a
quadro sem o rAF avançar a simulação no meio.

## Configuração gráfica (02/10, noite)

Presets + gaveta "⚙ personalizar" no painel Sistema, 18 opções em 4 grupos,
salvas no navegador. Detalhe das explosões vai de 48³ a 96³ com assets
prontos pras quatro resoluções. Fumaça-bloqueia-luz-de-outras saiu do padrão
(era ~20 ms numa barragem). Luz fraca das explosões pequenas no fim da vida é
física (luz ∝ área, vida ∝ tamanho), não bug: a Carga Pesada do clique tem a
mesma curva de luz da ao vivo; "Brilho das explosões" ajusta.

## Explosões sobrepostas (02/10, noite)

Várias pequenas + uma grande em cima: a fumaça das pequenas aparecia colada
por cima da bola de fogo. Agora volumes que se interpenetram são marchados
juntos (ARCHITECTURE.md §4f2). Caminho percorrido, pra não repetir: a luz
cruzada sem auto-sombra deixava a fumaça das pequenas "algodão branco"; a
aproximação pelo céu olhava pra cima com o fogo embaixo; e a linha reta era a
face dos blocos macro da grande (luz pulada junto com a densidade vazia).
Ferramentas: `inst.params.joint` e `inst.params.debug`.

Git: `v1-antes-composicao-conjunta` (estado anterior) e
`v2-composicao-conjunta`.

## Noite (Ghost of Tsushima) + sombras da luz do fogo (02/10, noite) — tag v4-noite-sombras

Aprovado pelo usuário. Detalhes em ARCHITECTURE.md §2c
e §3.

- Sombra "atrasada": o mapa de 145° pra baixo só cobria ~7 m em volta de uma
  bola de fogo baixa → 2 cubos de distância radial. Presente desde o quadro 1.
- Noite invisível: o céu não espalhava a Lua, a exposição da noite deixava o
  chão ~6.5 stops abaixo do dia e o "lift" azul era um Purkinje falso. Agora:
  céu de luar na mesma atmosfera, exposição por iluminância física
  (lua cheia alta ≈ −2.75 stops na tela), Purkinje shift do Ghost calibrado
  nos slides, luar fisicamente avermelhado.
- A noite visível expôs o fogo: o chão perto da explosão (~900× o luar)
  estourava a tela de branco por >1 s. O medidor da adaptação passou a ver as
  superfícies (não só o volume) e pode fechar até a exposição do dia.
- Granulado/malha nas sombras: IGN sem TAA + penumbra enorme. VSM testado e
  descartado (perde a sombra); ficou PCSS + PCF bilinear + filtro bilateral
  em tela (`js/shadowdenoise.js`).
- Sombra sumindo à noite quando o fogo fica vermelho (achado pelo usuário):
  o critério de quem ganha cubo era potência > 2, absoluto. Agora é o
  contraste da sombra contra a luz ambiente da hora (≥ 3%). À noite o cubo
  fica ~4.5 s em vez de ~1.9 s (o custo dura mais, junto).
- Asset r64 re-assado (o hash cobre glsl.js/volumeRender.js); r48/r80/r96
  re-assam ao trocar de qualidade.

Ferramentas: `captures/_harness.js` (importar no console) — `__runSeq`,
`__sheet`, `__still`, `__env4` (texel 4: exposição, lux, E_render, escala do
Purkinje), `__ae`, `__hdr`. `env.purkinje` liga/desliga o shift.

Em aberto, visto nos testes:
- Faíscas brancas na face dos pilares sob luz do fogo: relevo procedural do
  material (aparece com o cubo desligado) — aliasing de normal, não sombra.
- Crepúsculo (17.3–18h) bem saturado laranja → magenta: é o modelo
  atmosférico (ozônio) agora visível; falta adaptação cromática (white
  balance por hora, como o Ghost faz).
- Os 4 primeiros quadros de uma explosão perto são o clarão branco (ataque
  da adaptação 0.08 s).

## Item 3 — vetores de movimento no bake (03/10) — tag v5-vetores-movimento

Aprovado pelo usuário. Detalhes em
ARCHITECTURE.md §4f1. Velocidade gravada no bake, interpolação com movimento
em todas as instâncias (as pequenas não pulam mais de quadro), macro dos dois
quadros, cache de luz interpolado, ruído de detalhe advectado. +26% no cenário
de 12 explosões. Assets das 4 resoluções re-assados no formato novo.

Visto nos testes e NÃO tratado: uma linha horizontal cortando a fumaça na
altura do horizonte (composição/névoa sobre o volume).

## Ainda não construído

- SSAO — o MRT de normal + fração-de-ambiente já existe em `sceneT.texs[1]`
  e está sendo escrito, mas ninguém consome. Era pra ancorar os props.
- Reprojeção temporal no volume (hoje só jitter espacial).
- Painel de parâmetros pra tuning ao vivo.
- `renderAt()` não atualiza a luz do fogo: as fences de PBO não sinalizam
  dentro de uma rajada síncrona de JS. Precisa de um caminho de leitura
  bloqueante só pra captura determinística.
