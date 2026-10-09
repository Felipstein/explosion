# Conceito: explosões (direção de arte)

Estudo da sessão de arte conceitual (06/10/2026). Complementa o
`docs/EXPLOSOES.md` (física, técnica e fases, da sessão das explosões): aqui é
**o que o jogador vê e sente**, os tipos, a assinatura de cada facção e os
prompts das artes-alvo. As artes aprovadas vão pra `docs/concept/explosions/`.

---

## 1. Por que a explosão de hoje não encanta (comparada com o alvo)

`captures/ue/_fase1_alvo.jpg` contra `docs/concept/explosions/target_ground_blast.webp`:

| | Alvo | Hoje |
|---|---|---|
| Núcleo | branco estourado, ofuscante | laranja translúcido, "gelatina" de contorno escuro |
| Forma | bolhas nítidas e opacas | mole, borrada |
| Fumaça | coluna preta subindo, separada do fogo | bolo marrom que vira névoa |
| Poeira | saia baixa, contida, com bolinhas | véu bege cobrindo meia tela |
| Destroços/faíscas | riscos e terra voando | nada |
| Contraste | branco, preto e bege | marrom-alaranjado médio |

**A lição:** explosão bonita é **pilha de camadas** com cor, tempo e nitidez
próprias (clarão, bola de fogo, coluna de fumaça, saia de poeira, faíscas,
destroços, terra, marca no chão, luz). A de hoje é praticamente uma camada
só em baixa resolução.

---

## 2. A ideia que faz isso ser icônico: a explosão é a assinatura da facção

No Zero Hour você sabia **quem** atacou só de olhar: o verde do GLA, o fogo
da China, o feixe da USA. Proposta: cada facção destrói de um jeito, e o
jogador lê isso de longe.

| Facção | Personalidade | Cor e forma | Exemplos |
|---|---|---|---|
| **USA** | precisão, tecnologia | clarão **branco-azulado**, bola de fogo compacta e limpa, pouca fumaça, faíscas brancas; ataques que **caem do céu** (míssil guiado, bomba inteligente, feixe orbital) | míssil guiado, ataque aéreo de precisão, **canhão de partículas/laser orbital** |
| **China** | força bruta, fogo | **laranja-vermelho**, bolas de fogo enormes, **paredes de napalm**, fumaça preta gorda; muita coisa ao mesmo tempo (barragem) | barragem de artilharia, **napalm**, Overlord, **bomba nuclear** |
| **GLA** | sujeira, improviso, veneno | **fumaça preta oleosa**, fogo sujo, **verde tóxico**, sucata voando; explosões "erradas" (carro-bomba, foguete torto) | carro-bomba, foguetes improvisados, **nuvem tóxica verde**, **Scud** |

Cada facção também morre de um jeito: tanque da USA explode com faíscas
brancas e a torre voando; o da China queima por muito tempo; o veículo do GLA
se desmancha em sucata e rodas.

---

## 3. Catálogo dos tipos (o que o jogo vai ter)

Organizado por tamanho, do mais frequente ao mais raro. Cada um tem um papel
na leitura da batalha.

| # | Tipo | O que o jogador sente | Camadas que fazem ser bonito |
|---|---|---|---|
| 1 | **Impacto de tiro de tanque na areia** | o pulso da batalha, acontece o tempo todo | clarão curto, **jato de terra em cone**, torrões, saia baixa, cratera pequena |
| 2 | **Impacto em blindagem** | "acertei" | **chuva de faíscas** brancas, clarão branco, pedaço de chapa |
| 3 | **Morte de tanque** | a recompensa | sub-explosões, **torre arremessada girando** com rastro de fogo, bola de combustível, destroços, carcaça queimando |
| 4 | **Veículo leve arremessado** | o espetáculo do GLA | picape girando no ar com rastro, rodas soltas, sucata, fumaça preta |
| 5 | **Prédio destruído** | conquista | colapso, **nuvem de poeira cinza de concreto**, vigas, faíscas, fogo nas janelas |
| 6 | **Barragem de artilharia** (China) | medo | várias colunas de terra em linha, uma atrás da outra |
| 7 | **Napalm** (China) | o chão vira inferno | parede de fogo baixa e larga, fumaça preta gorda, chão queimado |
| 8 | **Ataque de precisão** (USA) | "do céu" | risco do míssil, clarão branco-azulado, bola compacta e limpa |
| 9 | **Nuvem tóxica** (GLA) | ameaça que fica | névoa verde rasteira, unidades sumindo dentro |
| 10 | **Superarmas** | o momento do jogo | nuke (China), canhão de partículas (USA), Scud tóxico (GLA) |

**Variações por chão** (o mesmo tipo responde ao terreno): areia (terra
amarela, assenta rápido), terra seca (poeira marrom, dura), neve (cratera com
solo escuro, neve voando branca), grama (capim pega fogo), asfalto/cidade
(lascas e pó cinza), água (coluna de spray).
**À noite:** a explosão vira a luz da cena (o laranja do fogo ilumina tudo em
volta) e as faíscas brilham mais.

---

## 3b. Anatomia da morte de tanque que arrepia (vídeos do Veo, 06/10/2026)

O Felipe achou muito realista a morte de tanque dos vídeos de gameplay que
geramos no Gemini (Veo 3.1). Quadro a quadro em
`docs/concept/explosions/video_tank_kill_rts_cam_frames.jpg` (câmera de RTS,
12 quadros/s) e `video_tank_kill_low_cam_frames.jpg` (câmera deitada, 8
quadros/s); vídeos em `captures/concept/33_video/`.

| Tempo (do impacto) | Camada | O que faz arrepiar |
|---|---|---|
| 0–0,1 s | **Clarão** branco estourado | **ilumina tudo em volta**: prédios, chão, tanques vizinhos; raios de luz em estrela |
| 0,05–0,5 s | **Estrela de faíscas e destroços** | riscos radiais brilhantes, **chapas de metal escuras girando** (câmera deitada), algumas com rastro de fumaça |
| 0,1–0,6 s | **Anel de choque no chão** | disco claro que se espalha rente ao chão com riscos radiais; depois vira **saia de poeira baixa** |
| 0,3–1 s | **Bola de fogo** branco-amarela | bolhas nítidas ("couve-flor") subindo rápido |
| 0,8–1,5 s | **Esfriamento de fora pra dentro** | a casca vai de laranja → vermelho-escuro → **preto**, com **brasas brilhando pelas rachaduras**; forma de cogumelo (chapéu + caule de fogo) — o momento mais "real" |
| 1,5–3 s e depois | **Coluna preta + carcaça queimando** | o chapéu preto sobe; o caule de fogo continua (a carcaça queima); faíscas caem; destroços quicam; a poeira assenta baixa |

**O ritmo é rápido:** clarão em 2 quadros, pico em ~0,5 s, preto em ~1,5 s.
A nossa cresce devagar e fica marrom; a dele é um soco e depois esfria.

**Escala:** bola de fogo de ~2 comprimentos de tanque; chapéu a 3–4
comprimentos de altura. A física do `EXPLOSOES.md` dá bola de 34 m em 2,6 s
pro tanque: o vídeo comprime pra ~1,5 s (bom pra jogo).

**Realismo de verdade x cinema:**
- Na vida real (filmagens de drone de "cook-off" de T-72), primeiro sai uma
  **fonte de fogo pela escotilha** (o combustível), e segundos depois a munição
  detona e **arremessa a torre**. Bom roteiro pra variar: às vezes explode já,
  às vezes queima e depois explode.
- O **Broken Arrow** (RTS moderno de 2025) fez explosões "fiéis" (onda de
  pressão, quase sem fogo) e os jogadores pediram mais. Ele quebra cada tanque
  em ~100 pedaços e usa **explosão da munição atrasada** "pra dar tempo de o
  jogador notar e aumentar a satisfação".
- O anel claro no chão do vídeo é exagero de cinema (o choque real é
  invisível; só a poeira aparece), mas **comunica área e força** de longe.
  Manter, na cor do chão.
- **Direção:** realismo cinematográfico (o que o olho acredita), não física
  crua. Física dá os tamanhos e tempos; o cinema dá o contraste e o ritmo.

**Comparado com o BlastLab hoje:** falta o clarão que ilumina a cena, a
estrela de faíscas, as chapas voando, o anel, o esfriamento com casca preta e
brasas (a nossa é translúcida e marrom), o ritmo rápido e o fogo que fica na
carcaça.

---

## 4. Artes-alvo (primeira leva: 8)

Mesma linguagem das artes aprovadas: câmera de RTS a ~55°, deserto em golden
hour, unidades pequenas como no jogo, fotorrealista AAA. Gerar no **GPT**
(o melhor em explosão) e no **Gemini**. Os prompts completos estão abaixo.

### 4.1 Impacto de tiro de tanque na areia
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour with long shadows. Close-up moment of a tank shell hitting the sand next to a group of sand-yellow guerrilla vehicles: a short white-hot flash at the impact point, a sharp cone of sand and dirt thrown up and outward in a fan, clumps of earth flying in arcs, a low ring of dust spreading along the ground, a small dark crater left in the sand. The dust stays low and does not hide the vehicles. Crisp, high-contrast detail: bright flash, dark crater, golden dust. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.2 Impacto em blindagem (faíscas)
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour. A light grey US main battle tank with blue markings is hit on the turret armor by an anti-tank round: a burst of bright white and orange sparks spraying outward in a cone, a brief white flash on the armor, a few glowing metal fragments flying, a thin puff of grey smoke. The tank stays fully visible. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.3 Morte de tanque (torre arremessada)
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour with long shadows. A dark olive-green Chinese tank is destroyed by an ammunition explosion: its turret is blown straight up into the air, spinning, trailing fire and smoke; a white-hot core and an orange fireball burst out of the hull hatches; burning fragments and road wheels fly outward in arcs leaving thin smoke trails; thick black smoke starts rising in a column above the wreck; the sand around is scorched. Other tanks nearby stay readable. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.4 Veículo leve arremessado
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour. A sand-yellow guerrilla technical pickup truck with green flags is hit by a shell and thrown into the air, flipping upside down and spinning, trailing black smoke and fire; two wheels and pieces of scrap metal fly off in different directions; a burst of orange fire and black oily smoke at the impact point on the ground; sand thrown in a fan. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.5 Prédio destruído
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert town at golden hour. A two-story concrete building collapses after a heavy explosion: the roof and walls cave in, a huge grey concrete dust cloud billows outward and upward, broken beams and chunks of concrete fall, sparks and small fires burn in the windows, debris scatters across the street. The dust is light grey, not brown, and settles low around the ruin. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.6 Napalm (China)
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour. A Chinese napalm air strike: a long, low wall of rolling orange and red fire sweeps across the ground in a straight line, thick black smoke boiling up above the flames, the sand blackened and burning behind it, small burning puddles scattered along the edges. Guerrilla vehicles flee at the edge of the fire. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.7 Ataque de precisão (USA)
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour. A US precision-guided missile strikes a guerrilla vehicle: a thin bright white trail coming down from the sky, a sharp white-blue flash at the impact, a compact clean orange fireball, a ring of sand pushed outward, very little smoke, glowing white sparks. Clean, surgical, high-tech. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

### 4.8 Nuvem tóxica (GLA)
```
In-game screenshot of a modern real-time strategy game, 16:9, high camera looking down at about 55 degrees, desert at golden hour. A guerrilla toxin shell bursts in the middle of a group of light grey US tanks: a dirty yellow-green explosion releases a thick, low, glowing green toxic cloud that crawls along the ground, the sand under it stained green, a faint green haze drifting with the wind. The tanks are still visible inside the cloud. Menacing, poisonous. Photorealistic AAA game graphics, Unreal Engine 5 look, cinematic lighting. No HUD, no text.
```

**Próxima leva (depois desta):** as superarmas (nuke, canhão de partículas,
Scud tóxico), barragem de artilharia, helicóptero caindo, as variações por chão
(neve, grama, cidade, água) e a noite. E um **vídeo-alvo no Veo** da explosão
aprovada, pra ter o ritmo e o tempo, não só um quadro.

---

## 4b. O plano do arrepio (síntese de 5 pesquisas, 07/10/2026)

Relatórios completos em `captures/concept/34_explosion_study/`:
`01_jogos_aaa.md`, `02_fisica_matematica.md`, `03_desempenho_vento.md`,
`04_interacao_ambiente.md`, `05_arrepio_experiencia.md`.

### Por que a nossa fica marrom, mole e lenta (achado no código)

1. **A curva do brilho achata o fogo.** Na física, a 2000 K o fogo brilha 60×
   o laranja de 1500 K, e a fuligem a 1000 K quase não brilha (lei de Planck:
   o brilho visível cresce como T^16 a 1500 K). O material usa
   `pow(Y, 0.44–0.6)` (`build_explosion.py` linhas 259/290/811/1058, herdado do
   WebGL): o núcleo brilha só 3–12× e a fuligem fria brilha 24–90× demais.
   **É o "marrom".** A casca preta com brasas nas rachaduras sai de graça com
   a curva física (~1,0): o render já faz emissão por Kirchhoff. O nó
   BlackBody da Unreal também erra o brilho (usa T⁴).
2. **Poeira dentro da bola.** A Carga Pesada mistura poeira (albedo 0,78) com
   fuligem (0,30) = marrom. Morte de tanque é bola de combustível: só fuligem
   e fogo; a poeira fica na saia. E a poeira tem albedo baixo demais (real
   0,95–0,99).
3. **Tempo de física, não de cinema.** A bola de fogo do tanque dura
   0,078·D s (CCPS) — o "preto em 1,5 s" do vídeo é isso pra D ≈ 20 m. E
   escalar a gravação por W^⅓ deixa as grandes lentas (a subida vai por √D).
4. **A gravação é grossa pro zoom do jogo** (0,4 m por voxel contra 0,03–0,06
   m por pixel) e o render amacia (meia resolução + upsample suave).
5. **A fumaça de verdade é quase uma superfície** (fuligem: 8,7 m²/g; 1 m de
   casca deixa passar ~1%), não névoa. A nossa espalha a densidade e fica
   translúcida.

### O arrepio é multissensorial

O cérebro arrepia quando os sentidos são atingidos **na ordem certa**: o
clarão, a luz no mundo, e então o choque chegando (som, tremor e o mundo
reagindo juntos, no instante d/343 s). Mais a **antecipação** (aviso,
assobio, silêncio) e a **consequência** (o mapa guarda a marca). O Zero Hour
já organizava assim: cada evento no `FXList.ini` junta efeito, pulso de luz,
tremor, som e queimado. **O maior buraco hoje é o som: não existe.**

### Fases (ordem por arrepio ÷ esforço)

**A. O soco (dias, ~0 ms de placa)**
1. Emissão com contraste físico (curva ~1,0) + núcleo estourado pelo tonemap.
   **Decisão do Felipe** (afasta do ajuste aprovado do WebGL; A/B).
2. Retime: soco até 0,3–0,5 s, preto em ~1,5 s (relógio em duas partes:
   λ·t até o fim da bola, depois √λ).
3. Receita da morte de tanque só com fuligem e fogo (poeira só na saia).
4. Clarão que acende a cena: luz direta de 2–3 quadros sem rebote e sem
   névoa + estrela de raios + exposição presa na câmera de RTS.
5. Tremor por trauma² com ruído, chegando em d/343 s, medido no ponto de mira,
   com teto e botão de acessibilidade.

**B. O mundo sente (1–2 semanas, < 1 ms)**
6. Anel de choque rente ao chão, na cor do material, que vira a saia de poeira.
7. Chão por material: 8–16 jatos em cone de 45–50° (assimétricos pela direção
   do tiro), grãos quicando, chuva de areia (0,5–3 s), leque do disparo à
   frente do cano deitando o capim. Tabela arma × superfície (areia, terra,
   grama, asfalto, neve, lama, rocha) com o som junto.
8. Faíscas e chapas acopladas ao fluxo (o Niagara 5.8 lê a velocidade da
   gravação), torre arremessada girando com rastro, sub-explosões sorteadas
   ou atrasadas.
9. Onda de reação no instante d/343: árvores atingidas (até ~32, só elas)
   curvando e voltando, folhas arrancadas, capim deitando em anel, vidro
   estourando em sequência, poeira caindo dos telhados, tanques vizinhos
   balançando, bandeiras chicoteando, pássaros fugindo.
10. Memória do campo: queimado, tapete de ejeção, riscos e trilhas de esteira
    carimbados na textura virtual do terreno; carcaça queimando (1–3 focos,
    chama até a altura do casco, pulsando a 1,5/√D Hz) com coluna preta
    subindo acima das unidades.

**C. O som (começar já, em paralelo: os sons demoram)**
11. Camadas (estalo 0–30 ms, corpo grave 50–400 ms, destroços, cauda por
    ambiente), microfone no ponto de mira a ~12 m do chão (regra do ZH),
    atraso pela distância, mixagem que abaixa o resto (HDR barato), vozes das
    unidades e do anunciador nunca cortadas. GPU: zero.

**D. Nitidez e escala (semanas)**
12. Gravação herói mais fina pra morte de tanque (0,15–0,2 m por voxel) e
    render inteiro no ultra.
13. Flipbook 6-way + canal de temperatura + vetores de movimento, gravado de
    simulação fina, pros efeitos pequenos e frequentes (o que os AAA fazem:
    Battlefront II, Skull & Bones, Star Citizen). Volume pras grandes e
    raras e pra câmera baixa.
14. **Primeiro medir** (varredura de zoom: 1 Carga Pesada a 40/60/90/150/300 m)
    e decidir Heterogeneous Volume × marchador conjunto próprio (o do WebGL
    fazia 96 explosões em 5,6 ms). Segunda opinião antes.
15. Campo de vento único: motores analíticos (pulso de explosão, esteira de
    avião, rotor de helicóptero, veículo, pluma de fogo) + grade presa ao
    mundo. Toda fumaça, poeira, capim e explosão viva leem o mesmo vento.
    ~0,3–0,5 ms. (O MSFS 2024 nem faz a fumaça reagir ao rotor:
    oportunidade.)

**E. Espetáculo**
16. Superarma coreografada: aviso com contagem, assobio, mixagem quase muda,
    clarão, estrondo atrasado pela distância, tremor severo, ronco longo,
    marca permanente.
17. Câmera de cinema baixa + replay e diretor (fora do multiplayer).
18. Fogo no capim andando com o vento (linha de cinza), coluna d'água com
    anel escuro, prédio desabando com poeira cinza escorrendo pelas ruas.

### Decisões (respondidas pelo Felipe em 07/10/2026)
- **Regra dele:** fazer o que for necessário pra chegar no alvo (o plano e as
  artes); as escolhas técnicas são nossas, sem se prender ao ajuste antigo.
- Curva de emissão física: **liberada** (A/B só pra medir, não pra pedir licença).
- Som sob a névoa de guerra: o combate comum escondido continua mudo; os
  **eventos grandes** (superarma, nuke) são ouvidos pelo mapa inteiro como um
  estrondo distante e abafado, de arrepiar: todo mundo sabe que "deu merda"
  em algum lugar, sem ganhar a posição exata (direção no estéreo, no máximo).
- Cratera como cobertura (estilo Company of Heroes: infantaria dentro do buraco
  leva menos dano, veículo passa mais devagar): pendente, explicado a ele.
- Barragem acima do teto de volumes: as mais próximas da câmera e as maiores
  ficam com a versão completa; as excedentes nascem na versão barata (flipbook
  da mesma gravação). No meio de 20 ao mesmo tempo, ninguém percebe.

### O roteiro (tanque morre a ~55 m da fábrica, pôr do sol)
0 s clarão branco acende a fábrica e os tanques; 0,09 s a onda chega na
fábrica: poeira cai do telhado, uma vidraça estala, a bandeira chicoteia;
0,16 s o tranco da câmera e o "BUM" (estalo + soco grave; o resto abaixa);
0,2–0,4 s o tanque vizinho balança, pássaros saem da palmeira; 0,3–1 s bola
de fogo, torre subindo girando, chapas voando; 0,5–1,5 s sub-explosões e
pipocos; ~1 s soldados arremessados, rádio: "Alvo destruído!"; 1,5 s a casca
vira preto com brasas, o estrondo rola pelo deserto; 3,5 s a torre cai; 3–20 s
coluna preta sobe e a sombra dela passa sobre a fábrica, calor tremendo,
crepitar; minutos depois: carcaça queimando baixo, estrela de queimado,
cratera, pedaços e o rastro da última manobra.

---

## 4d. Ordem de serviço do arrepio (entregue à sessão das explosões, 07/10/2026)

Um item só está feito quando passa no critério, medido pela sessão de arte
com a folha quadro a quadro (`36_blastlab_vs_target/`). A captura usa sempre
os mesmos parâmetros (`cmp_rts` e `cmp_low`). "Fase" é a do plano de
`docs/EXPLOSOES.md` §6. **O teste final não é a bola de fogo: é a sequência
dos sentidos.**

**O princípio:** todo evento dispara junto, como o FXList do Zero Hour, as
mesmas cinco coisas, cada uma no seu instante: efeito, pulso de luz, tremor,
som e marca no chão. O clarão sai em t = 0; o som, o tremor e a reação do
mundo saem juntos em t = d/343 s, com d = distância até o ponto de mira.

| # | Item | Critério de aceite (medido) | Fonte |
|---|---|---|---|
| A1 | Curva de emissão física + núcleo estourado | p5 da luminância na bola ≤ 0,18 a partir de 1,3 s; fogo com matiz 52–90° < 1%; 0,1–0,4 s: mais da metade da bola com saturação ≤ 0,11 | 02 §curva, 01 §0 |
| A2 | Ritmo | o escurecer começa em 1,3–1,5 s (**já bate**, não mexer) | 02 |
| A3 | Morte de tanque só com fuligem e fogo | bola sem poeira (albedo ≤ 0,35, extinção de fuligem); poeira de albedo 0,95 só na saia | 01 §0, 02 |
| A4 | Clarão que acende o mundo | pico de luz em 2–3 quadros; o chão volta ao normal até 0,15 s (hoje > 0,4 s); estrela de raios visível 0–0,13 s; exposição da RTS varia < 0,3 EV | 01 §4, 05 |
| A5 | Tremor | começa em d/343 s; amplitude ∝ trauma²; teto global; decai em ~1 s; opção de acessibilidade | 05, 01 §4 |
| B6 | Anel de choque | anel nítido no chão de 0,3 a 1 s, crescendo, na cor do material, que vira a saia | 04, vídeo |
| B7 | Chão por material | 8–16 jatos em cone de 45–50°, chuva de areia de 0,5 a 3 s, tabela arma × chão | 04 |
| B8 | Faíscas, chapas e torre | 8 ou mais faíscas-cometa com rastro de fumaça; chapas com sombra no chão; torre girando com rastro e caindo em ~3,5 s | 01 §4, vídeo |
| B9 | Onda de reação em d/343 | árvores curvam e voltam, vidro estoura em sequência, poeira cai do telhado, bandeira chicoteia, tanque vizinho balança, pássaros fogem | 04, 05 |
| B10 | Memória do campo | queimado e cratera permanentes; carcaça com 1–3 focos (EXPLOSOES §7b); coluna preta opaca subindo de 3 a 20 s, acima das unidades | 04, EXPLOSOES §7b |
| C11 | Som | estalo 0–30 ms, corpo grave 50–400 ms, destroços, cauda; atraso d/343; o resto da mixagem abaixa; vozes nunca cortadas; superarma ouvida no mapa inteiro, abafada e sem posição | 05 |
| D12 | Nitidez de perto | bolhas nítidas na câmera baixa (0,15–0,2 m por voxel), sem granulado em movimento | 01 §6, 03 |
| D13–14 | Escala | medir antes (Carga Pesada a 40/60/90/150/300 m), depois decidir volume × flipbook 6-way × marcha conjunta, com segunda opinião | 03 |
| D15 | Vento único | explosão, avião, rotor e veículo empurram toda a fumaça, a poeira e o capim | 03 |
| E16–18 | Espetáculo | superarma coreografada, câmera de cinema com replay, fogo no capim, coluna d'água, prédio desabando | 05 |

**Ajustes de 07/10, depois da 1ª rodada da fase A** (`captures/ue/_fa2.jpg`, linha 3):
- **Método único:** as medidas saem de `BlastLab/Tools/blast_metrics.py`
  (janela de 24 m pela linha AUTOTEST), reportando p5, mediana e saturação,
  pra sombra no chão não se passar por fumaça.
- **A1, cor:** a fumaça escura tem que ser **cinza-preto neutro**. Na rodada,
  a parte escura saiu com saturação 0,9–1,0 (bronze); no alvo é ~0,4.
- **A1, brasas:** as brasas ficam **nas rachaduras entre os lóbulos** (onde a
  fuligem é fina e o núcleo aparece), com os topos pretos, como no vídeo. Na
  rodada o brilho saiu no topo das bolhas (bronze lustroso).
- **A3, saia:** a poeira ao sol fica igual ou mais clara que a areia em volta
  (≥ 0,9×). Uma saia mais escura que o chão vira a "panqueca".
- **Hora da captura:** passa a ser `-Hour 16` (sol ~15°, como as artes).
  Às 17 h o sol fica a 3,4° e pinta tudo de laranja (erro meu no parâmetro).
- **A4 à noite:** de dia, a exposição varia < 0,3 EV. À noite, em vez de
  fechar ~4,7 stops (some o campo de batalha), o fogo brilha em relação à
  exposição, como a lua. Vai em A/B em movimento pro Felipe, com o critério:
  as unidades continuam legíveis no clarão.

**Crítica do Felipe na rodada 1 ("parece slime, quadriculado de TV velha"),
confirmada a 3×:**
- **Slime:** os lóbulos são bolas lisas do mesmo tamanho, de miolo chapado e
  contorno escuro. O alvo é couve-flor em 3–4 escalas, com a parte quente
  turbulenta. Na fase branca (0,1–0,8 s) a bola não tem contorno escuro.
- **TV velha:** granulado colorido pixel a pixel na parte clara (ruído que não
  converge: meia resolução + upsample, provável jitter antes da cor do corpo
  negro). Critério: sem grão visível a 3× e sem cintilar no vídeo.

**Mudança de rumo (07/10):** antes de seguir pelas fases, **uma cena herói de
ponta a ponta**: a tomada da câmera baixa do vídeo do Veo, com um tanque
real como vítima, bola de ~2,5× a altura do tanque, faíscas-cometa, chapas,
anel, tronco de fogo, carcaça queimando e som. O aceite é vídeo contra vídeo.

**Teste final = o roteiro** (§4b): tanque morre a ~55 m da fábrica, ao pôr
do sol. É gravado como **vídeo com som** e comparado com o vídeo do Veo, não
só como fotos.

---

## 4c. Hoje × alvo (prints do Felipe, 07/10/2026)

Folha: `captures/concept/36_blastlab_vs_target/hoje_vs_alvo.jpg` (prints em
`hoje_230..235.webp`). Artes recebidas em `captures/concept/35_explosion_arts/`.
Medido em recortes da explosão (luminância sRGB 0–1):

| | parte mais escura (p5) | faixa p5→p95 | fogo puxando pro limão |
|---|---|---|---|
| Hoje, câmera de RTS | 0,41–0,51 | 0,33–0,42 | — |
| Hoje, de perto | — | — | 8–18% |
| Artes e vídeo | 0,09–0,18 | 0,6–0,9 | < 1% |

Por ordem de peso:
1. **Não existe preto.** O mais escuro da nossa é bege claro; nas artes é a
   fumaça preta. → fase A (curva física, receita só de fuligem).
2. **Forma única de cogumelo** (mini-nuke) e saia que vira **panqueca marrom
   chapada**. O alvo é bola + coluna preta de lado + terra em leque + anel.
   → receita por tipo (§3, §4).
3. **Fogo verde-limão de perto**: conflita com o tóxico, a assinatura do GLA.
   Hipótese: fogo amarelo visto através de casca marrom translúcida + tonemap
   por canal (vermelho bate no teto antes do verde). Medir o pixel antes do
   tonemap.
4. **Granulado de perto** (chiado de TV, vai piscar em movimento) e céu preto
   nas câmeras baixas (exposição?).
5. **Saia de poeira em bolas de algodão iguais**, sem escala menor nem jatos.
6. **Nada voa**: sem faísca, sem pedaço com rastro, sem estrela de clarão.
7. Quadro inteiro laranja (230) e crepúsculo roxo (231); listras na rocha
   (artefato de sombra, bug à parte).

O que já está bom: cratera na areia, luz do fogo no chão, pedras do chão, o
cenário.

**Quadro a quadro (capturado por mim, 07/10):** `quadro_a_quadro_rts.jpg` e
`quadro_a_quadro_low.jpg` na mesma pasta (Carga Pesada, Range_Desert, alta,
passo fixo de 1/30 s; vídeo do Veo no mesmo instante depois do clarão).
- **0–0,3 s:** no vídeo, estrela branca com raios e faíscas; na nossa, uma
  bola branca lisa e pequena, enquanto o chão fica aceso por mais de 0,4 s.
- **0,4–1 s:** no vídeo, anel branco de choque no chão, 8–10 faíscas-cometa
  com rastro de fumaça e chapas pretas voando; na nossa, nada voa, não há
  anel, e a saia marrom vira panqueca.
- **1–1,5 s:** no vídeo, casca preta com brasas nas rachaduras e um tronco de
  fogo ligando ao tanque; na nossa, uma bola dourada-marrom uniforme.
- **2–4 s:** no vídeo, coluna preta opaca e fogo contínuo na carcaça; na
  nossa, a nuvem vira marrom-oliva translúcida e some, sem nada queimando.
- **O ritmo já bate:** o escurecer começa em ~1,3–1,5 s nos dois. Falta o
  preto, não o tempo.
- O vídeo é morte de tanque; a nossa ainda é só a Carga Pesada (HE). "Igual
  ao vídeo" exige a receita de morte de tanque.

---

## 4e. Alvos aprovados por tipo (Felipe, 07/10/2026)

Aprovados "pode ser o que você escolheu". Os arquivos estão em
`docs/concept/explosions/type_4.x_*.webp`; a folha das 8 em
`captures/concept/35_explosion_arts/escolhas_por_tipo.jpg`. Cada arte vira
uma ficha de aceite: o que **tem que** aparecer, a cor e a regra de jogo.

- **4.1 Tiro na areia** (Gemini): clarão branco curto; um jato de terra escura
  em leque **pra um lado** (pela direção do tiro, não um cogumelo); anel de
  poeira clara rente ao chão; cratera escura. Do GPT: a massa de poeira
  rolando. Nada de fumaça preta: é HE no chão.
- **4.2 Faíscas na blindagem** (Gemini): leque de riscos finos branco-quentes
  saindo da placa, flash curto no metal, sopro de fumaça cinza. Do GPT: 3–5
  chapinhas voando. O tanque nunca some atrás.
- **4.3 Morte de tanque** (Gemini, + o vídeo do Veo): torre arremessada girando
  com rastro; bola saindo do anel da torre; coluna **preta opaca** de lado;
  pedaços em chamas em arco com rastro de fumaça; chão queimado com brasas.
- **4.4 Picape arremessada** (GPT): o veículo sobe girando, 2 rodas e chapas
  soltas, fogo + fumaça **preta oleosa** (assinatura do GLA), leque de areia.
  No jogo sobe menos que na arte.
- **4.5 Prédio** (Gemini): o teto afunda, poeira **cinza-clara** (não marrom)
  escorrendo pelas ruas e baixa, pedaços de concreto. Do GPT: fogo nas
  janelas. Depende dos prédios em 3D.
- **4.6 Napalm da China** (Gemini): **linha** de fogo (não um mar), 3 colunas
  pretas, chão preto queimado atrás, poças queimando. Não pode cobrir mais que
  a faixa atingida.
- **4.7 Precisão da USA** (Gemini): feixe/rastro branco-azulado, clarão
  branco-azul, bola laranja compacta, pouca fumaça, faíscas brancas em raio.
  **O anel de areia é o do GPT** (`type_4.7_ring_reference_gpt.webp`): a
  referência do anel de choque de todas as explosões.
- **4.8 Tóxico do GLA** (GPT): gás verde rasteiro, poças verdes brilhando no
  chão. Regra de jogo: o gás fica **abaixo da torre** dos tanques. O verde é
  exclusivo do GLA, por isso **fogo nunca pode puxar pro verde** (§4c).

---

## 4f. O recomeço (noite de 07/10/2026, Felipe dormindo, eu coordenando)

**Por quê.** Com tanques de verdade na cena (`Range_Desert_Units`), a
explosão estava 4× grande demais (escala 2,2 em vez de 0,55), virou "slime"
e não encaixava no ambiente. O Felipe pediu um passo atrás: refazer do zero
com estudo e segundas opiniões, em vez de remendar.

**As três opiniões** (`captures/concept/37_restart_study/`):
- **01, crítico imparcial:** jogar fora a gravação do WebGL como corpo e os
  remendos em cima dela, a poeira analítica dentro do volume e a estrela de
  lente. O padrão AAA é simulação offline de alta qualidade, só tocada no
  jogo.
- **02, pipeline offline:** o Mantaflow do Blender não tem expansão pela
  queima, então a couve-flor não sai dele sozinho. O EmberGen tem.
- **03, atmosfera e escala:** a luz do céu nos volumes estava a 0,175 (5,7×
  menos que o chão) e a poeira tinha a cor da arte, não a do chão.

**Proporção-alvo** (medida no vídeo pelo casco da vítima):
- bola com 1,0–1,2 tanque de largura no pico;
- coluna com 1,1–1,4 tanque, presa ao fogo da carcaça;
- poeira até 2,5 × 0,4 tanque, assentando em 2–2,5 s;
- anel de 2–3 tanques.

**O corpo novo:** a Gasoline Explosion da JangaFX (EmberGen, CC0, baixada
com o OK do Felipe), convertida por `Tools/convert_pyro_vdb.py` em
`Intermediate/BakeRaw/eg_gasoline` (192³, 17,1 m, 74 quadros, quadro do
EmberGen / 36). Na prévia (`37_restart_study/eg_gasoline/sheet.jpg`) ela tem
a anatomia do vídeo: couve-flor em várias escalas, cogumelo com tronco e
laranja → casca → preto. O EmberGen dissolve a fumaça depois de ~2 s; o
meta.json traz um ganho sugerido pra manter a coluna.

**Ferramentas novas:**
- `blender_pyro_tank.py` e `blender_pyro_preview.py`: Mantaflow e prévia.
- `blender_vdb_preview.py`: prévia de qualquer VDB.
- `convert_pyro_vdb.py`: VDB → bake bruto do BlastLab.
- `veo_sheet.py`: folha quadro a quadro contra o vídeo.
- `import_units.py` + `build_hero_scene.py`: tanque e cena herói.

**Divisão:**
- **Sessão das explosões:** pluga o corpo novo, faz a torre com rastro, as
  faíscas-cometa e as chapas, o anel, a poeira como camada própria, o fogo
  na carcaça e a coluna, e corrige o céu nos volumes (com oitavas × albedo)
  e a cor da poeira.
- **Sessão de arte:** escolhe, converte e mede as fontes contra o vídeo e
  revisa o WorldForge.

---

## 5. Depois das artes

1. O Felipe escolhe as artes de cada tipo.
2. Pra cada tipo: uma **folha de instantes** (clarão, 0,3 s, 1 s, 3 s) e o
   vídeo-alvo.
3. Comparar quadro a quadro com o BlastLab (mesma câmera e instante), medindo
   cor, contraste, nitidez e quanto da tela cobre.
4. Lista de correções por camada pra sessão das explosões.
