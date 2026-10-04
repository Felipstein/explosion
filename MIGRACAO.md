# Migração WebGL → Unreal Engine

Decidida em 03/10/2026. Referência visual e técnica: tag `v6-fumaca-interacao`
deste repositório. Este documento diz **no que cada sistema vira na Unreal**,
**quais números levar** e **o que aprendemos**. Não é código: na Unreal, a
regra continua a mesma de sempre (pesquisar como os AAA fazem → plano →
implementar → medir e ver em movimento).

## Por que migrar agora

Os últimos problemas vieram de limites do WebGL, contornados um a um:

- **Fumaça "dura"**: sem compute shader, o fluido roda como passes de imagem
  num atlas 2D. A grade do vento fica com 2 m, grossa demais.
- **Fogo que muda na troca gravação → grade**: não dá pra compor duas nuvens
  que se interpenetram, e a grade de 1 m não guarda o detalhe da gravação
  (0.6 m).
- **Explosão que não reage nos primeiros ~3 s**: ela é uma gravação, porque
  simular cada explosão ao vivo não cabe no WebGL.

## Primeiro passo na Unreal: um laço de teste que o Claude consiga ver

No WebGL o Claude testava quadro a quadro sozinho no navegador. Na Unreal,
antes de qualquer efeito:

1. Anotar a versão da Unreal instalada. 5.4+ é o mínimo útil (Sparse Volume
   Textures e Heterogeneous Volumes maduros); 5.5+ traz MegaLights.
2. Ativar o plugin **Python Editor Script Plugin** (e **Niagara Fluids**).
3. Montar uma captura por linha de comando, que o Claude roda e depois lê as
   imagens: `UnrealEditor-Cmd.exe <projeto>.uproject -ExecutePythonScript=<script.py>`
   carregando um mapa de teste, posicionando a câmera, avançando o tempo e
   gravando quadros (`HighResShot` ou Movie Render Queue). Vídeo também, pra
   julgar movimento.
4. Mapa de teste fixo: o mesmo enquadramento das capturas do WebGL (câmera a
   ~55–85 m, elevação baixa, hora 15h e 22h30), pra comparar lado a lado.

## Sistema por sistema

| WebGL (arquivo) | Unreal | Notas |
|---|---|---|
| Solver da explosão ao vivo, 128³, combustão (`fluid.js`) | **Niagara Fluids** (Grid 3D Gas), ou simulação offline (EmberGen/Houdini) | Combustão limitada por mistura, fuligem rica/pobre, detonação como fonte de pressão (ARCHITECTURE §1). |
| Bake + instâncias (`bake.js`, `instances.js`) | **Sparse Volume Texture** (sequência OpenVDB) tocada num **Heterogeneous Volume** por explosão | Exportar nossas gravações pra `.vdb` (formato abaixo) ou refazer a simulação. Manter variantes e as leis de escala W^(1/3). |
| Vetores de movimento no bake | Interpolação entre quadros da SVT; se não houver, gravar mais quadros | Sem eles, as pequenas pulavam de quadro. |
| Fumaça no campo (`battlesmoke.js`) | **Niagara Fluids** num volume de campo, OU a própria simulação por explosão continuando | **Não repetir a troca gravação → grade.** Um único sistema desde o primeiro quadro (simulação Niagara que é a explosão), ou a gravação deformada pelo vento do Niagara. |
| Avião, míssil, disparo (`movers.js`) | Atores + módulos de Niagara que injetam velocidade; **Chaos Physics Fields** pra explosões | Modelos físicos abaixo. Colisão móvel nativa do Niagara Fluids pro corpo. |
| Sombreamento volumétrico (fase dupla de Henyey–Greenstein, espalhamento múltiplo em oitavas) | Heterogeneous Volumes / material de volume | Levar os parâmetros de aparência (abaixo). |
| Corpo negro (`blackbody.js`, LUT Planck × CIE) | Nó **Blackbody** do material, calibrado contra a nossa LUT | Conferir 1400 K vermelho profundo, 3500 K branco amarelado. |
| Atmosfera e hora do dia (`atmosphere.js`, `celestial.js`) | **Sky Atmosphere** + luz direcional sol/lua + SkyLight em tempo real | Hillaire 2020 é o mesmo modelo da Sky Atmosphere. |
| Noite (Ghost of Tsushima), Purkinje (`post.js`) | Exposição física (EV100) + material de pós-processo pro Purkinje | Matrizes LMSR em ARCHITECTURE §2c. |
| Exposição que reage (`exposure.js`) | Auto Exposure (histograma) com faixas de EV ajustadas | Medir superfícies + volume, sem o céu. |
| Luz das explosões + sombras (cubos PCSS) | Point lights com sombra; **MegaLights** + **Virtual Shadow Maps** | Lá não existe o limite de 2 cubos. |
| SSAO, bloom, tonemap | GTAO/Lumen, bloom nativo, tonemap filmic | — |
| Faíscas (`sparks.js`) | Partículas GPU do Niagara | — |
| Câmera RTS (`camera.js`) | Pawn com Enhanced Input | Rolagem pelo botão direito estilo Generals (abaixo). |
| Presets baixa/média/alta/ultra (`settings.js`) | Scalability Groups | Poucas opções pro jogador. |

### Formato das gravações (`assets/bake/r<res>-<hash>.bin`)

Para um exportador `.bin` → `.vdb` (por exemplo, rodando o Python do Blender,
que já traz `pyopenvdb`):

- `uint32` mágico `XBK1` (0x314b4258), `uint32` tamanho do cabeçalho, depois
  um cabeçalho JSON: `res`, `fuelRes`, `frames` (56), `variants` (3),
  `duration` (6 s), `curve`, `bounds` (caixa justa por quadro), `velMax` (40).
- Depois: posição e cor da luz por camada (float32 × 3 cada), os voxels e o
  combustível.
- Camadas = variantes × quadros, cada uma um atlas 2D de fatias z
  (`VolumeGrid`). Voxels em RGBA8: fuligem = r² × 4.0, temperatura = g × 1.6,
  poeira = b² × 2.5, a = transmitância do céu.
- Combustível em RGBA8 na metade da resolução: r² = combustível, gba =
  velocidade (±40 m/s).
- Tempo do quadro f: `duration × (f / (frames − 1))^curve`. Domínio de 38 m.

## Números calibrados pra levar

**Aparência da fumaça e do fogo** (`inst.params`):

| Parâmetro | Valor |
|---|---|
| Extinção | fuligem 13.5, poeira 5.5 |
| Albedo | fuligem 0.30, poeira 0.78 |
| Cor | fuligem (0.55, 0.545, 0.535), poeira (0.82, 0.755, 0.655) |
| Fase (Henyey–Greenstein dupla) | g 0.42, retro 0.22, mistura 0.32 |
| Espalhamento múltiplo | 3 oitavas: extinção 0.52, espalhamento 0.52, fase 0.55 |
| Emissão | ganho 0.62, curva 0.44, escala de T 0.92, reforço de chama 1.5 |
| Erosão das explosões | 0.042 |

**Magnitudes**:
- Granada 0.05, morteiro 0.22, carga pesada 1, paiol 4, ataque aéreo 11.
- Escala = W^(1/3) em comprimento e em tempo: as maiores são maiores e mais
  lentas.

**Exposição e noite**:

| Parâmetro | Valor |
|---|---|
| Base | 10 |
| Referência | 0.768 |
| α | 0.1265 |
| Sol | 128 000 lux |
| Lua / sol | 2.5e-6, cor (1.165, 0.976, 0.753) |
| Brilho do céu noturno | 0.002 lux |
| Medidor | p = 0.5, alvo 0.45, sem o céu |

**Fumaça no campo**:
- **Duração** (decisão de jogo, não física): densa até ~10 s, fiapos até
  ~15 s. Dissipação de 10 s + erosão que come primeiro o que está ralo.
- **Subida**: a nuvem para a ~35 m, porque a subida da pluma desacelera em
  1.2 s (Morton–Taylor–Turner).
- **Turbulência**: redemoinhos de ~10 m, σᵤ ≈ 0.4 m/s, vertical metade da
  horizontal (σ_w ≈ 0.5·σᵤ, Panofsky & Dutton). Espalhamento σ 6.6 → 10.5 m
  em 4 → 15 s (Pasquill–Gifford D).
- **Calor**: resfria com ~5 s de constante, o ritmo da gravação entre 3 e
  4.5 s.

**Objetos que atravessam**:
- **Avião** (classe A-10: 20 t, 16 m de envergadura, 140 m/s):
  - Esteira: par de vórtices com Γ = W/(ρ·U·b0) ≈ 91 m²/s, b0 = π/4·b.
    Desce ~1.1 m/s; o ar entre os vórtices desce ~4.6 m/s.
  - Jato: 2 × 40 kN. A velocidade no eixo é a menor entre a de saída e a que
    a conservação permite (empuxo/velocidade por metro, espalhado na seção;
    meia-largura ~0.1·x): ~9 m/s a 40 m.
- **Míssil**: 80 → 450 m/s, ~10 kN. Rastro de Al₂O₃ (albedo 0.93) que nasce
  com ~2.5 m. A luz da chama é 0.3% da luz de pico de uma explosão de tanque.
- **Disparo**: 120 mm a 1000 m/s, tubo de ~2 m que a turbulência fecha.
  Discreto: é o físico.
- **Explosão empurrando a fumaça**: ∇·u = S numa bola de 9 m·escala por
  0.15 s.

**Câmera RTS** (rolagem estilo Generals, `LookAtXlat.cpp` do código liberado
pela EA):
- Velocidade = (0.15 + 2·(r − 0.02)) telas/s, com r = afastamento do cursor
  em meias alturas de tela; zona morta 0.02.
- A âncora acompanha o cursor quando ele passa de meia tela.
- Tempo real: a câmera lenta não freia.
- Botão esquerdo arrastado gira a câmera, a roda dá zoom.

## Lições que valem pra Unreal

- **Medir antes de mexer** e comparar A/B com história idêntica (mesma
  semente, mesmo tempo da turbulência).
- **Ver em movimento**: a "dureza" da fumaça não aparecia em fotos.
- **Perturbador soma, não sobrescreve**: impor a velocidade apagava os
  vórtices e matava a subida da pluma. Corpo e jato mexem só na componente ao
  longo do voo.
- **Um freio vertical único** (pra nuvem parar de subir) matava metade da
  rotação. Na Unreal, a desaceleração da térmica tem que vir do modelo.
- **Duas nuvens no mesmo lugar** só se compõem direito num único volume (ou
  com Heterogeneous Volumes): evitar trocar uma representação por outra no
  meio da animação.
- **Efeito aplicado só no render** diverge do que o resto acredita existir
  (o bug da sombra fantasma).
- **Duração e cobertura de efeitos são decisões de jogabilidade.**

## Problemas em aberto (herdados)

- A troca gravação → grade é perceptível: o brilho muda e a subida
  desacelera de repente. Na Unreal, resolver com um sistema único.
- A explosão não reage nos primeiros ~3 s (gravação): simular por explosão
  no Niagara, ou deformar a gravação pelo vento.
- O disparo de tanque é discreto (físico); talvez valha um exagero de design.
- Linha do horizonte vista através da fumaça semitransparente (físico).
- Crepúsculo saturado (falta white balance por hora), faíscas brancas nos
  pilares (aliasing de normal).

## Ordem sugerida

0. Laço de captura automática + mapa de teste (acima).
1. Cena, céu, hora do dia e exposição, batendo com as referências do WebGL.
2. Explosão tocando a gravação (SVT + Heterogeneous Volume), com sombreamento
   e brilho iguais à referência.
3. Luz das explosões com sombra (MegaLights/VSM).
4. Fumaça e interação num sistema único (Niagara): avião, míssil, disparo,
   explosões.
5. Câmera RTS.
6. Desempenho e presets (Scalability).

Referências visuais: as capturas e os vídeos de `captures/` no Mac (fora do
git: `aviao_por_tras.mp4`, `aviao_missil_lado.mp4`, `noite_troca*.jpg`). A tag
`v6-fumaca-interacao` regenera qualquer um.
