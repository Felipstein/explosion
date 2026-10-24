# Estudo: o míssil da Patriot (a defesa de base USA)

Estudo da sessão Fluxo (16/10), com fontes, para o item 4 da fila em `docs/TIRO_E_ACERTO.md`. Ainda não implementado.
**[E]** = estimativa, sem fonte direta.

**A simulação (sessão do jogo, 16/10):** o míssil sai de uma das 4 caixas inclinado 35°, a 20 m/s. Acelera a 120 m/s² até 110 m/s, guia a 200°/s e explode no contato. Voa ~0,7 s subindo ~5 m. A rajada de 4 sai em leque.

**A interface proposta à sessão do jogo (a confirmar):**
- `UBlastFxDirector::MissileLaunch(Id, PosCm, Dir, Variant)`
- `MissileMove(Id, PosCm, Dir, SpeedCmPerS)`, a cada quadro
- `MissileHit(Id, PointCm, Victim, GroundZCm, bKill)`

## O real

**A saída**
- **Lançamento quente (Patriot):** o motor sai aceso da caixa. Fica uma nuvem no chão cor de terra, acesa de laranja por dentro, com torrões. [E] ~24 × 10 m, medida nas fotos de Balikatan 2023 e Creta 2025.
- **A tampa da caixa:** sai em pedaços previstos (patente US5993921).

**A chama e a fumaça**
- Propelente com alumínio: fumaça branca (óxido de alumínio) e chama branca curta.
- Base dupla: quase sem fumaça e chama laranja.
- **A largura do rastro, [E] pelas fotos:**
  - ~4–5 diâmetros logo atrás da chama;
  - a fumaça por metro é a vazão dividida pela velocidade, então o rastro do nosso míssil (20 → 110 m/s) sai ~5× mais denso perto da caixa do que no fim.
- **O guiamento:** a navegação proporcional faz curva lisa. A espiral é do giro do corpo (5–15 voltas/s em mísseis de corpo giratório), e é opcional no A/B.

**A ogiva no tanque contra o tiro de canhão**
- Bola de fogo de ~1 m de raio por kg^⅓ (LANL).
- 3–5 kg de explosivo dão ~3–3,4 m de diâmetro, com brilho de 50–150 ms [E].
- Faíscas radiais e fumaça cinza-escura (o TNT solta fuligem).
- A ogiva explode do lado de fora da blindagem.

## Os jogos
- **Zero Hour:**
  - **Voo** (`PatriotMissileLocomotor`): 105 m/s, aceleração 118 m/s², giro 300°/s. É quase o nosso míssil.
  - **Disparo** (`FX_BuggyMissileIgnition`): 2 clarões aditivos que somem em 0,33 s e 1–2 nuvens cor de areia que vão de 1,3 a ~10 m em 1 s.
  - **Rastro** (`MissileExhaust`): fita creme que engrossa até ~0,39 m e apaga em 1,0 s.
  - **Acerto:** 3 bolas aditivas de ~4 m e 2 fumaças cinza que vão a ~11 m e somem em 2,5 s.
  - **Prioridade:** o rastro fica e a explosão de arma é a primeira a sair.
- **DCS:** um puff forte só na saída, depois um fio fino.
- **Arma 3:** densidade pela soma de camadas fracas.
- **Beyond All Reason:** rastro de 0,5–0,9 s, sem sombra.
- **O que se repete:** rastro de 0,5–2 s; saída densa e curta, cruzeiro fino; brilho na ponta; sopro de ré com nuvem cor do chão de ~1 s.

## A receita [E] (calibrar no jogo contra a arte 4.7, `docs/concept/explosions/type_4.7_precision_strike.webp`)

**Escala e cadência**
- O míssil tem ~3,4 × 0,4 m.
- O ciclo da torre é 2,75 s. Com rastro de no máximo 1,4 s, o céu limpa entre uma rajada e outra.

**Saída, por míssil**
- **Clarão:** branco-amarelo de 1,2 m por 2 quadros, mais uma língua de 1,5 m.
- **Jato de ré:** 6–10 riscos laranja de 1,5–3 m pela grelha de trás.
- **Tampa:** 2–3 pedaços de 0,3–0,45 m.
- **Luz:** 6·10⁵–1·10⁶ cd, τ 60 ms, raio 8 m.

**Poeira do lançamento, uma por rajada**
- A areia gravada atrás da caixa, ~5–6 m, abaixo de 2,5 m de altura.
- Some em menos de 2 s; acesa de laranja nos primeiros 0,15 s.

**Voo**
- **Chama:** miolo branco de 0,35 × 1,0–1,4 m (nunca menos de 3 px), língua até 2 m, tremendo ±20% a 20–30 Hz, e um halo fraco.
- **Luz que anda:** 1–2·10⁵ cd, raio de 5–6 m, sem sombra.
- **Fita do puff de saída** (primeiros 0,15 s): largura 1,0, engorda 2,0, vida 1,2 s, opacidade 0,6.
- **Fio de cruzeiro:** largura 0,5, engorda 1,2 m/s, vida 1,4 s, albedo 0,8, opacidade 0,5, tinta (0,94; 0,94; 0,95). Deriva com a brisa.
- **No código:** o fogo do rastro atrás da peça cai em 0,25 s fixos. A 100 m/s isso acende 25 m de fita. No míssil precisa de τ ≈ 0,025 s.

**Acerto no tanque**
- **Clarão:** branco-azulado de 2,5 m.
- **Bola compacta de ~3 m:** o fogo passa em ~0,15 s, a fumaça é cinza-escura e dura no máximo 1,8 s. Só o 1º acerto da rajada leva o volume.
- **Faíscas:** 40–60 radiais.
- **Lascas:** 8–12.
- **Luz:** 6–8·10⁵ cd, τ 120 ms, raio de 10 m.

**Lembrete (16/10, a bronca do disparo):** a bola da explosão não pode ser a morte de tanque em miniatura sem pensar. Compor pelo fenômeno real de uma ogiva de ~3–5 kg (`feedback-fenomeno-real-primeiro`).

## A explosão com vida (23/10, EM TESTE: nada aprovado ainda)

O Felipe, sobre a Patriot, o lançador Tomahawk e o Cruise Missile Strike: "é spawnado ali e pronto, sem interação, sem natureza, sem
vida"; "a explosão nasce de onde bateu e sai torta (mas é física, não regra: no chão sobe do chão plano)"; e no ataque "falta a
interação no chão: areia grossa voando, um impacto e uma cratera bem maiores". O plano do diretor das explosões (fases 1 a 14; as
palmeiras e a luz herói ficam pra fase 2) virou código; os números e as fontes estão nos comentários e no plano.

**A arquitetura:** um evento, muitas reações (os "nuggets" do FXList do ZH).
- O BlastFx desenha o acerto com vida (`UBlastFxDirector::MissileHit` com o pulso → `MissileHitLife`) e devolve o ponto, a normal, d,
  ℓ, o pé da bola e os relógios (`BlastFxHitLife::FHitOut`).
- O jogo distribui as reações que moram nele (`FRTSHitLife`, `RTSHitLife.h/.cpp`): o tranco, a areia, a fumaça do furo, o tremor, os
  vizinhos.
- Só desenho: nada volta pra simulação.

**As chaves:**
- `-HitLifeV=1` = a de hoje, exata (sem nenhuma areia nova: a da torre solta também fica de fora); `-HitLifeV=2` (o padrão) = com vida.
  `-MissileBlastV=1` continua a ogiva de antes, sem vida.
- Uma variável por camada, ao vivo, pro A/B camada por camada: `blast.HitLife.<camada>` 0/1.

| Camada | O que faz |
|---|---|
| `Dir` | ℓ = normalize(n + 0,3·cima·[face de pé] + w·t), d em 3D (o mergulho fica): a bola nasce em P + ℓ·0,35·R e anda mais 0,7·R por ℓ em 1 − e^(−t/τ) (τ = 0,25 × o fim da bola acesa); o giro, as faíscas, as lascas, a luz e o anel por ℓ; o raio do míssil em 3D acha o teto ou o costado. No teto (sem a `warhead_teto`) o pé da Carga Pesada fica no plano do teto: a carga em cima do casco é uma explosão de superfície nesse plano, e a bola fica em cima do casco, nunca dentro (com o pé no chão ela nascia dentro da planta do tanque). Ali a saia baixa da gravação (o canal da poeira, a areia que o choque levanta no chão dela) sai na cor e no albedo da fuligem da própria bola: aço e telhado não levantam areia, o que corre rente ao teto é o jato de parede do gás da carga; o anel de areia segue no chão |
| `Glints` | os brilhos dos estilhaços (1–2 quadros, 0,15–0,3 m) nas faces viradas pra bola: 6·K² no alvo (teto 16), 3–4 nos vizinhos dentro dos 50 kPa (7,3·K m) |
| `Sparks` | as faíscas em 3–7 jatos (55%), rentes à face (30%) e voltando (15%), 50·K² (teto 90), a vida sem o ×K, o leque inclinado pra frente (`fragment_tilt_deg`) |
| `Specks` | os pontinhos dos estilhaços na areia: 10·K², numa elipse à frente (3 a 20·K m, ±25°), 1,5 m e A 2 (um leque por lugar a cada 1,5 s). Só na explosão acima do chão (no casco, no prédio): no chão o que desce entra na cratera e o resto cai a centenas de metros, quem diz o rumo são os torrões. Nada embaixo da planta do alvo (+ 0,5 m) nem atrás dela no acerto do lado (a blindagem barra) |
| `Veil` | o véu de areia do choque, a onda pequena que empurra a areia do chão pro lado: a lei de Kinney-Graham (ver "A areia do acerto", caso 1). Uma representação só dessa areia: com ele o BlastFx não põe o anel de areia da ogiva (`FHitIn::bGameVeil`); com a gravação dedicada (`warhead_*`), ele nasce no esmaecer dela. `-VeilV=1` = o de antes (dois anéis ralos, 0,2, com o anel) |
| ~~`Under`~~ | **saiu (23/10).** Era a poeira saindo de baixo do casco. O Felipe: "não vai ter muita areia saindo do casco, isso não tem nada a ver com física nem a realidade; ele vai ter uma onda de impacto pequena que empurra as areias do chão pro lado". O chão embaixo do casco fica tapado pelo próprio casco; a areia do acerto no veículo é só o `Veil` |
| `Kick` | o tranco × (W/4 kg)·(60 t/M) (Units.json `mass_t`), até o teto de 5° da mola |
| `Neighbors` | o vizinho do dano em área: o tranco do alvo × (dano no vizinho ÷ dano no alvo, os dois depois da blindagem; no chão, ÷ o dano da arma) e a marca na face virada pra explosão |
| `Cloud` | a nuvem continua viva: o último quadro parado (o ruído andando), sobe e alarga como térmica (× √(t/t₁)), dilui (espessura ÷ o cubo), vai com o vento; 3·G s no Alta/Ultra, 1,5·G na Média, nenhuma na Baixa |
| `Smolder` | a fumaça do furo (a fuligem do diesel no rastro do míssil), presa ao casco, 4 puffs/s caindo em e^(−t/τ), τ = 2,5·K s; no chão, o fiapo claro da cratera |
| `Embers` | os pedaços em brasa (2·K², teto 4): voam com o fio de fumaça (a fuligem em volume do `RTSMissileSmoke`, um puff a cada 0,8 m do caminho, já nascendo com σ 0,28 m, que se dilui em e^(−t/0,35 s) e sai em 1,05 s: ~16 puffs vivos por pedaço; com a camada `Smolder`; 1 pedaço com fio por acerto, 2 a partir de 12 kg, teto 8/4/2/0 pedaços fumegando: no pior caso ~130 puffs a mais no Ultra), caem, ficam 2–4 s piscando, levantam areia e fumegam 3–4 s |
| `Ground` | o chamuscado no chão sob a bola (raio W^⅓ m; no teto, o fraco de sempre); o míssil no chão com a terra e os torrões na escala K (os 8/6/3/0 maiores levantam o respingo de areia no 1º toque, pela energia da batida); a terra escura no 2º míssil no mesmo buraco |
| `TrailRise` | a subida que puxa a ponta do rastro dura o que a nuvem viva dura |
| `Shake` | o tremor de 6 cm atrasado d/340 s (a partir de 12 kg) |
| `Strike` | o chão do ataque: a cratera C a 1,6× e mais funda, a terra e os torrões da carga de 20 kg (velocidades × √K), a `strike_chao` em todo míssil (com a `Bake`) e, sem ela, a onda de base de reserva |
| `Bake` | as gravações dedicadas (`warhead_lado`, `warhead_teto`, `warhead_chao`, `strike_chao` em `Intermediate/Raymarch`), quando existem: o pé no chão sob o ponto do acerto e o +X dela pela normal da face (de lado), pelo eixo do casco no sentido de ℓ (no teto) ou pelo rumo do míssil (no chão), sem andar; o fim do volume vai pela idade dele (o relógio da dedicada), com ou sem a nuvem viva. Só carregam com a vida (`-HitLifeV=2`, a ogiva v2, a camada ligada) e fora da Baixa. (23/10) A `strike_chao` é só a areia do ataque: toca à parte, na escala 1 (`-StrikeBakeScale=`), em todo míssil (as duas metades), no rumo do míssil, com a bola clássica ou a ogiva em cima; a ogiva da metade leve no chão é a `warhead_chao` (ou a Carga Pesada) |
| `Linger` | (23/10) a poeira que fica do ataque (e do míssil no chão): rola, abre e deriva no vento do mapa por ~20 s depois da gravação (ver "A areia do acerto", caso 2) |

**A areia** vai pra poeira das lagartas em volume (`ARTSTreadDust::AddImpactPuffs`, as camadas `impact`, `impact_surge` e
`impact_linger`): a sombra do mapa, a cor do chão, o vento e as luzes de graça. **A fumaça** vai pro `RTSMissileSmoke` (o fio do furo, os fiapos, o fio dos pedaços em brasa em
voo), com a sombra no mapa do sol; sob a névoa de guerra nenhum ponto solta fumaça.

**Os tetos por preset (Ultra/Alta/Média/Baixa):** cartões de impacto 384/256/64/0 (eram 256/192/64/0; ver "O orçamento"), fiapos 8/6/3/0, pedaços em brasa
32/16/8/4, a nuvem viva 3·G/3·G/1,5·G/não; a areia, em "A areia do acerto".

**As cenas da prova:** `vida_perto` (a Patriot de perto, o vizinho, a poeira velha), `vida_sombra` (na sombra da War Factory, ao lado
de uma morte) e `areia_strike_sombra` (o ataque na sombra da War Factory, ao lado de uma morte); e as de antes (`patriot_video_v4`,
`v4_siege`, `v4_strike`, `v4_strike_r4`, `torre_fisica`, `fumaca_predio2`).

## A areia do acerto: os 3 casos (23/10, EM TESTE: nada aprovado ainda)

O Felipe: a Patriot no tanque "quase não muda"; o ataque é bem melhor, mas parece chão duro (se até os tanques levantam poeira, uma
explosão na areia tem de levantar muito mais: densa, pesada, igual o game shot); a torre solta (aprovada) tem de levantar areia ao bater.
E a correção: "no tanque a explosão NÃO vai ter muita areia saindo do casco [...] ela vai ter uma onda de impacto pequena que empurra as
areias do chão pro lado, obviamente; a questão é os tomahawk quando cai na areia em si". Código: `RTSHitLife.cpp` (as leis e as
constantes, com as fontes), o plugin `RTSTreadDust` (a massa, a 3ª camada, o vento) e o BlastFx (a `strike_chao`, o respingo no 1º toque).

**O diagnóstico (medido na gravação `dust_kick_v3_30hz`):** τ do miolo de um puff = 0,43·A·a coluna da gravação. Com A = 1 a parte
visível (τ > 0,3) é ~0,3 do lado; o véu de antes (A 0,2, 2,5–3 m) nascia quase invisível, e a onda de base (A 0,15) também. A lagarta lê
porque 10–20 cartões se empilham em cada ponto; o acerto não empilha. **A correção:** cada puff de impacto leva a massa de vários
(`FImpactPuff::Mass`, A até 16, linear no volume; nos cartões da Média corta em 1). A régua é a poeira das lagartas aprovada (densidade
0,08); `rts.TreadDust.ImpactGain` multiplica a massa de todos ao vivo, pra calibrar.

**Caso 1, a ogiva no blindado (4–12 kg a 1–2,4 m, encostada no aço).** O grão de 200 µm só ganha ≥ 0,5 m/s onde o vento atrás do
choque passa de 100 m/s (a fase positiva dura 3–9 ms; Kinney-Graham com Rankine-Hugoniot); os finos seguem o ar até ~50 m/s. A carga
efetiva: 1,8·W do lado atingido (o casco e o chão), 1,0·W no teto (o centro é o do alvo), 0,5·W atrás do casco (a onda que contorna);
embaixo do casco nada.
- A faixa densa, da bola (1·W^⅓) até o vento de 100 m/s (Z = 3,80 m/kg^⅓; o de 50 m/s a Z = 5,76): 7,3 | 6,0 | 4,8 m com 4 kg (lado,
  teto, sombra), 10,6 | 8,7 | 6,9 m com 12 kg, no ar; no chão, com a altura do acerto, 7,2 do lado e 5,5 no teto com 4 kg. A franja até o
  de 50 m/s (11 e 16 m do lado).
- N = 0,47·(R² − r²) puffs com o lado de 5 m, ou seja 0,15 por m² de anel (24 puffs com 4 kg do lado, 35 com 12 kg; o texto de antes
  dizia 0,47 por m², faltava o π), lado 5·G m, A 4 (a franja, ¼ da densidade e A 1,5), massa patchy ±30%, nasce na chegada do choque
  (r/400 s) com a gravação em 0,5 s, sai a 2–4 m/s e para em 0,5–1 m, some em 4·G s. O miolo visível de um puff de A 4 (τ > 0,3) tem
  ~2,1 m: a cobertura do anel fica ~0,5 (patchy, com falhas entre os puffs; não os ~0,85 de antes).
- O puff cujo segmento do ponto do acerto até ele cruza a planta do alvo (a caixa das peças) vai pra faixa da sombra; dentro da planta
  + 0,5 m não nasce. A face da torre em cima do convés vale como o teto.
- Os raios são da distância inclinada √(r² + h²), com h a altura do ponto do acerto sobre o chão: no chão o raio de cada vento é
  √(R² − h²) e a chegada √(r² + h²)/400 s. No tanque (h 1–2,4 m) quase não muda; o acerto alto na parede do prédio (`v4_siege`) deixa no
  chão só o que o raio alcança (acima do raio do vento de 100 m/s, nada).
- No tiro que mata, sem véu (a morte tem a areia dela); a fusão de 1,5 s por lugar fica.
- **Uma representação só** (a revisão): o anel de areia da ogiva (a `sim_chao_anel` a 1,15 na Patriot e 1,66 no Tomahawk: até ~8 e
  ~11,6 m) é a mesma areia que o véu. Com o véu ligado (o de agora, o preset com cartões de impacto) o jogo avisa o BlastFx
  (`FHitIn::bGameVeil`) e o anel não nasce: fica o véu, que tem o vazio atrás do casco e nada embaixo dele. Com a gravação dedicada
  (`warhead_lado`, `_teto`, `_chao`, que já tem essa areia) o véu nasce no esmaecer dela (`FHitOut::bOwnSand`, `OwnSandFadeS`), no
  estado que teria nessa idade (parado, a gravação dele adiantada, a vida que sobra); a massa casa com a gravação quando ela for medida.
  `blast.HitLife.Veil 0` ou `-VeilV=1` trazem o anel de volta.

**Caso 2, o míssil na areia.** O Tomahawk real (~150 kg TNT eq.): a cratera de 2,7–4,3 m, 3,6 t lançadas, 100–360 kg de finos; a
cortina em dedos a 50–58° desaba em 2–4× a escavação (0,47 s) e vira a onda de base, uma corrente de gravidade cuja frente para em
R_L ≈ 0,55·R100 (~13–15 m). `hit_life.sand_kg` no Units.json (o ataque: 150; o padrão é o `warhead_kg`); os torrões seguem com
`ejecta_kg` 20.
- **Com a `strike_chao`** (a cortina, a onda de base, os anéis do véu, o jato escuro, só areia, 5 s): o campo não põe o perto; põe a
  **poeira que fica** (`impact_linger`): nasce sob o esmaecer da gravação (`FHitOut::SandFadeS`; o ataque de 1 s da camada é a emenda),
  em células de 6 m dentro de R_L em volta de cada míssil (o ataque não repete célula por 4 s; teto 64/48/16/0 por ataque), lado 7–9 m,
  A 6 (τ ~0,7 rente ao chão: 50% de transmitância), a gravação 5× mais devagar, sai a 1,5–3 m/s (arrasto 0,3), abre 1,6×, vive 20 s e
  deriva no vento do mapa.
- **Sem ela** (sem o arquivo, sem vaga, `Bake` desligada, o míssil fora do ataque): o véu do caso 1 com 1,8·sand_kg (sem franja acima de
  40 kg; até 40 puffs, o lado cresce), a onda de base de reserva (`impact_surge`: nasce em 2,15·Ra com 2,5·t_esc ±30% de atraso, sai a
  0,8·(R_L − r0), lado min(0,3·R_L/0,45; 8 m), ~18 puffs com 150 kg, uma por R_L a cada 3 s) e a poeira que fica 1,5 s depois. A massa da
  carga menor: A ∝ (W/150)^⅓ (os finos ∝ W numa área ∝ W^⅔).
- **Os respingos dos torrões:** no 1º toque, com a velocidade (antes no repouso, já sem energia): Dc = 0,92·[ρb/(ρg·μ²)]^¼·s^¾·H^¼
  (Uehara et al. 2003), o lado ≈ 4,4·Dc (2–5 m), A 4, 2,5 s; 8/6/3/0 por míssil.
- A `strike_chao` toca agora em **todo** míssil do ataque, na escala 1, com a bola clássica ou a ogiva em cima (antes só na metade leve, a
  0,84, como a fonte da ogiva). R_L e o τ do chão no esmaecer saem da fórmula até a gravação final ser medida.

**Caso 3, a torre (15 t) batendo e deslizando.** A 1ª batida a 13–16 m/s, as seguintes a 2–7 m/s. O corpo que bate pelo espalhamento
dos pontos (a quina 1,2 m, a face 2,7 m), Dc por Uehara (ρ 0,9, μ 0,67): o anel visível 2,2·Dc (4,8 m de quina e 9 m de face na 1ª
batida), N = 2,2·Dc/1,2 m puffs (3–8; teto 8/6/3/0) mais 1 no meio, lado 0,55·W, A = 6·Dc/2,2 (1,5–8), saída 0,25·v_n, a jusante na
batida oblíqua (o peso 1 + β·cos, β = 0,6·min(v_t/v_n; 1): Gault e Wedekind). Deslizando ou rolando a mais de 1 m/s: o solver só informa
um ponto a cada 0,5 m de contato (`FRTSLooseTurret::Scrapes`, no máximo 12; a física aprovada não muda), um puff de 2,4 m, A 0,5–2. O
empurrão na poeira que já está no ar e a marca ficam. `rts.Turret.Sand 0` desliga; com `-HitLifeV=1` ela também não sai.

**Pelo chão (`FRTSHitLife::DustYieldOf`, a revisão):** uma tabela só, a mesma pra lagarta e pro acerto: a da poeira das lagartas
aprovada, 1 − max(asfalto, molhado). O que sobe, da lagarta ou do choque, é a camada solta por cima: no deserto a terra batida e a rocha
têm a mesma areia solta e sobem como ela; o asfalto varrido e o molhado, não. Antes o acerto dava 0,17 na terra batida e 0,05 na rocha,
e ali um tanque passando levantaria mais que o véu do míssil. Hoje só existe areia; quando houver terra batida ou rocha de verdade (a
areia solta levanta ~6× mais, DNA ADA237024), as duas mudam juntas ali.

**O orçamento (a revisão 3).** Duas contas no `FRTSHitLife::SendPuffs`, cada classe da areia (a poeira que fica, os pousos, a torre,
o perto) com a sua conta:
- **Cartões de impacto** vivos e por nascer: 384/256/64/0 (Ultra/Alta/Média/Baixa; eram 256/192/64/0). Na Alta e no Ultra o custo é o dos
  tijolos (abaixo); só os cartões da Média custam por cartão. Acima do lugar, uma amostra espalhada.
- **Tijolos do campo** (4.096 no atlas; medido: 40 tanques ≈ 4.000 e +4,1 ms no Ultra, ~1 µs por tijolo): os de impacto até 1.600 na Alta e
  2.400 no Ultra, o campo inteiro (as lagartas mais os de impacto vivos e por nascer) abaixo de 3.600, como antes. O ataque de 9 com a
  poeira que fica inteira pede ~2.150; o pior caso do campo continua o das lagartas (3.600). A união das colunas de 2 m que os puffs ocupam
  (um mapa coluna → de quando a quando e a classe dona, a que a segura até mais tarde); sem lugar, saem os puffs de fora pra dentro (pela
  distância do centro do evento; nos pousos, os respingos menores primeiro), cada um pelas colunas novas que acrescenta: o miolo fica denso.
- **A cota não depende da ordem de chegada.** A reserva de antes só barrava quem chegava depois: no ataque, o perto e a poeira que fica
  (mandados no acerto) enchiam o teto, e os pousos dos torrões (1,5–3 s depois) não entravam (1 de 72 no Ultra com 9 mísseis, 0 na Alta;
  a torre caindo junto, 0 na Alta). Agora quem chega depois tem a cota guardada: os pousos 24% dos cartões e 20% dos tijolos (o pico deles
  no ataque de 9 no Ultra, ~90 cartões) e a torre 8% e 5% (~30 cartões). Ninguém mais pega a parte livre dela. A poeira que fica e o perto
  dividem o resto, na ordem: no mesmo acerto a poeira que fica vai antes, no miolo, e o perto reaproveita as colunas dela e põe a borda de
  fora. Guardar cota pra eles também prendia o perto no caso comum, só acertos em veículos (no Ultra, 219 dos 660 puffs pedidos contra 347).
- **O pior caso, simulado** (`scratchpad/sand3/final.py`: o ataque de 9 e de 6 mísseis no girassol de 14 e 12 m, um a cada 0,2 s, com as
  fusões de verdade, os torrões 8/6/3, os pedaços em brasa, a torre caindo junto, a Patriot atirando ao lado, 0 e 2.000 tijolos das
  lagartas). Ultra, 9, sem a gravação: os pousos 104/104 (os torrões 72/72), a torre 30/30, a poeira que fica 58/64. O perto entra 218 de
  364 (o véu de 150 kg vai até 24,6 m: a borda de fora sai). Com 20 tanques: os pousos 95/104, a poeira que fica 45/64. Com a gravação:
  64/64 e 104/104. Alta: os pousos 62–65/70, a torre 24/24, a poeira que fica 37/48. Média: os pousos 20/35, a torre 5/16 (o teto de 64).
  O caso comum (6 Tomahawk e 6 Patriot em 3,5 s): o perto 336/660 no Ultra (antes 324), 215 na Alta (antes 243: os 30% guardados) e os
  pousos 18/18 (antes 3/18).

**As chaves:** `-HitLifeV=1` (a de hoje, exata), `-VeilV=1` (o véu de antes), `blast.HitLife.Veil/Specks/Ground/Strike/Bake/Linger`,
`rts.Turret.Sand`, `rts.TreadDust.ImpactGain`, `-StrikeBakeScale=`.

**A prova:** A, B, A na mesma corrente, em vídeo, ampliado, na câmera do jogo e na do game shot 4.1: `vida_perto` (o anel e o vazio
atrás), `v4_siege` (a sombra do prédio), `v4_strike` e `v4_strike_r4` (com e sem a `strike_chao`, o custo), `torre_fisica` e
`areia_strike_sombra`. As medidas: os tijolos e "sem lugar no atlas" = 0 no log do `RTSDustField`, `rts.DustField.Debug 9`, a silhueta das
unidades ≥ 50% de transmitância 3 s depois do acerto, a emenda gravação → poeira que fica sem salto (±20%).

## As gravações dedicadas no jogo (23/10 noite, EM TESTE: nada aprovado ainda)

A rodada do Mantaflow (strike_chao v2/v3, warhead_lado/teto/chao) passou pela revisão (ok = falso) e pela prova no jogo. Só uma ganhou da
reserva e ficou ligada: **a areia do ataque de mísseis (`strike_chao`, a v4d, na escala 0,7)**. As outras ficam no BlastLab, fora do pacote.

**O que mudou nas gravações** (as ferramentas em `captures/game/noite_bk_provas/tools`; os arquivos em `BlastLab/Intermediate/Raymarch`):
- `strike_chao_v4d.brm2` (175 MB, até 875 blocos: o atlas não cresce): só areia, como o jogo de hoje toca (SpawnDust, sem bola, sem calor,
  sem luz). Os grãos novos (`ejecta_strike3.py`: 420 mil parcelas, jatos de 2–5° em vez de 0,8–2,4°, grumos com corpo de ~0,5 m) e a
  mistura (`assemble_sand.py`): o grosso a 0,6 da física, o borrão de 0,3 m antes do contraste (sem os degraus de 0,31 m da v3), a
  cortina alta rala (a folha do cone se abre subindo, 1/(1 + (z/6 m)²)), o gás do Mantaflow com a massa no pé (× 1 + 1,2·e^(−z/2 m)) e a
  terra escura (o jato central) a 0,7, na cor da cortina de terra do tiro na areia aprovado.
- `warhead_*_k.brm2`: a fuligem afinada dentro dos dados (`soot_keep.py`: só a fuligem fria, no relógio do fogo de cada uma, pro keep do
  Tomahawk 0,104); o jogo toca a dedicada com SootKeep 1 (a opacidade do volume multiplica fuligem e areia juntas, BlastRaymarch.usf). A
  `warhead_lado_km` é a espelhada (o lobo da lado puxa pro −Y dela, medido). A `warhead_chao_sk` é só o fogo e a fuligem do chão.

**A prova** (v4_strike gravada, Alta, A/B na mesma corrente; as folhas em `captures/game/noite_bk_provas`):
- Na escala 1 (as v4a/b/c/d) as 6 areias juntas, de 12 a 14 m de raio, **enterravam as bolas de fogo aprovadas** por ~1 s e cobriam as
  Patriots por ~3 s; a cortina lia como trigo (`ab_strike_4.jpg`, `ab_strike_fine.jpg`). A 0,55 a areia some atrás das bolas.
- **A 0,7 (`ab_strike_scales.jpg`, `ab_strike_s07.jpg`)**: as bolas inteiras, a areia densa explodindo em volta e embaixo delas, a onda
  de base rolando e as Patriots legíveis ~2 s depois do impacto; aos ~4 s a poeira que fica (o jogo) assume. Na sombra da War Factory, ao
  lado da morte de tanque (`areia_strike_sombra`, `sombra07_amp.jpg`): a areia escurece na sombra e fica de pé com a fumaça da carcaça.
- Por que 0,7 e não 1: a gravação é a areia de ~150 kg por míssil e as bolas que se veem são de 12–20 kg; a 0,7 (~50 kg, o relógio × 0,7)
  a areia é 2–3× a bola, como no game shot (motivação).

**As dedicadas no casco e no chão: desligadas** (não ganharam da reserva):
- O ajuste do casco (o casco gravado 7,9 × 3,6 m, o acerto no meio do costado ou no teto da torre) medido no espaço do Body (o ator do
  veículo não gira, quem gira é o Body; a caixa no espaço do ator saía 3,9 × 8,2 m no tanque de lado). A Patriot acerta a ponta do casco
  virada pra ela (x = 2,6–4,3 m do meio, a face da frente ou o convés inclinado): nenhum dos 9 acertos da `vida_perto` bate com a gravação,
  e fica a Carga Pesada em escala. O Tomahawk no tanque de lado bate (o meio do costado, a espelhada), mas a bola sai branca e a fuligem
  cinza abraça o casco: a reserva (a bola laranja aprovada e o véu) é mais bonita (`ab_toma_lado.jpg`).
- O tiro forçado na areia (Ctrl, `warhead_chao_k`): a cortina de grãos lê como uma vassoura branca e a bola some; a reserva é melhor
  (`ab_toma_chao3.jpg`).
- O mesmo achado corrigiu a planta do alvo do véu (`HullBox` e `RTSHitLife` `PlanOf`): ela era a caixa no espaço do ator, que não gira;
  num tanque a 45° o "embaixo do casco" e o "atrás do casco" viravam um quadrado de ~8,5 m. Agora é no rumo do Body (`FHitOut::HullYawDeg`).
  Nos tanques a 0/90/180° o véu sai igual (`vida_perto`, conferido em vídeo).

**O custo** (v4_strike_r4, 9 mísseis, sem gravar, 1600×900; a máquina dividida com as outras sessões, então o tempo de quadro é ruidoso):
- Alta: o quadro durante o ataque ficou igual (76 ms em média nos 6 s, com e sem); a mediana da partida 43,2 → 41,7 ms; o p95 66 → 82 ms
  (os picos de subida). A subida pra placa: 83–171 MB/s com a areia (24–68 sem), o pior quadro 36 MB (16 sem); até 17 explosões vivas.
- Ultra: inválido nesta noite (a mediana de 210–240 ms com e sem; a placa ocupada).
- RAM: 175 MB (a strike_chao). Placa: o atlas segue 35³ (199 MB), a maior gravação continua a eg_tq_v2.

**As chaves:** `-StrikeBakeScale=` (0,7), `-HitBakes=none` (a reserva exata) ou `all`, `-HitBakeFile=nome:caminho,...` (o A/B direto do
arquivo do BlastLab, sem link a mais), `-StrikeWarhead=1` (a `warhead_chao_s` na metade leve do ataque, não testada no jogo),
`-MissileBlastLog=1` (cada acerto: qual dedicada e as medidas do ajuste), `-RmUploadLog=1` (a subida e o quadro por segundo). A cena nova
`bake_tomahawk` (o Tomahawk no tanque de lado e o tiro forçado na areia).

**Pro EmberGen, se o Felipe quiser além disto:** a areia do ataque já lê densa; o que o Mantaflow não deu foi o casco e o chão de 12 kg com
fogo laranja e fuligem que não abraça o alvo. Simular no EmberGen (fila própria) o acerto no casco de verdade (a malha do tanque, nas
pontas: a frente e o convés, onde a Patriot e o Tomahawk acertam no jogo) e no chão, com o fogo calibrado pela bola aprovada (a
r96_v0 × λ), em 256³, exportado por `build_raymarch_vdb.py`; e a cortina do ataque em partículas de areia do próprio EmberGen (os jatos
largos com a física de pressão, sem o borrão de fios), pra comparar com a v4d no mesmo A/B.

## Fontes
- **Lançamento:** [Soft launch](https://en.wikipedia.org/wiki/Soft_launch_(missile)), [Starstreak](https://en.wikipedia.org/wiki/Starstreak), [TOW](https://designation-systems.net/dusrm/m-71.html), [fotos da Patriot no Commons](https://commons.wikimedia.org/wiki/File:Patriot_missile_firing_during_Balikatan_2023.jpg), [patente US5993921](https://patents.justia.com/patent/5993921).
- **Propelente e fumaça:** [patente US4798636](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/4798636), [DTIC ADA114222](https://apps.dtic.mil/sti/tr/pdf/ADA114222.pdf), [patente US6126763 (STANAG 6016)](https://patents.google.com/patent/US6126763), [patente US4054254A](https://patents.google.com/patent/US4054254A).
- **Ogiva:** [Rae & McAfee, LANL](https://www.osti.gov/biblio/1469550), [IATG 01.80](https://data.unsaferguard.org/iatg/en/IATG-01.80-Formulae-ammunition-management-IATG-V.3.pdf).
- **A areia do acerto:** Kinney e Graham, *Explosive Shocks in Air*; [a explosão na superfície, 1,8×](https://arxiv.org/pdf/2009.05674); [DNA ADA237024](https://apps.dtic.mil/sti/pdfs/ADA237024.pdf); [Klemens et al.](https://link.springer.com/article/10.1007/s00193-016-0661-0); [Houim et al.](https://meetings-archive.aps.org/dfd/2016/h5/1/); [Uehara et al. 2003](https://arxiv.org/pdf/cond-mat/0301549); [Walsh et al. 2003](https://physics.uwo.ca/~debruyn/pdfs/walsh_prl_03.pdf); [Glasstone e Dolan, cap. 6](https://atomicarchive.com/resources/documents/effects/glasstone-dolan/chapter6.html); [Housen e Holsapple](https://arxiv.org/pdf/2105.01474); [Church 1969](https://digital.library.unt.edu/ark:/67531/metadc1055670/m2/1/high_res_d/4798257.pdf); Gault e Wedekind 1978; [Kuhns et al.](https://gaftp.epa.gov/Air/nei/ei_conference/EI12/fugdust/present/kuhns.pdf); [Frost et al.](https://arxiv.org/abs/1110.3090).
- **Jogos:** [INIs do Zero Hour](https://github.com/TheSuperHackers/GeneralsGamePatch/tree/main/Patch104pZH/GameFilesOriginalZH/Data/INI), [ParticleSys.h](https://github.com/electronicarts/CnC_Generals_Zero_Hour/blob/main/GeneralsMD/Code/GameEngine/Include/GameClient/ParticleSys.h), [DCS](https://github.com/Quaggles/dcs-lua-datamine), [Beyond All Reason](https://github.com/beyond-all-reason/Beyond-All-Reason/blob/master/units/ArmBots/armrock.lua), [War Thunder datamine](https://github.com/gszabi99/War-Thunder-Datamine).
