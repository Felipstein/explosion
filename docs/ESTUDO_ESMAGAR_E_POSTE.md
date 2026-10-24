# Estudo: o tanque esmagando carro e o choque do poste derrubado

Estudo da sessão Fluxo (16/10), com fontes, para os itens 5 e 6 da fila em `docs/TIRO_E_ACERTO.md`. Ainda não implementado.
"(est.)" = estimativa ou conta, sem fonte direta.

**Escala na câmera do jogo** (`RTSCameraPawn.h`: campo de visão de 50° perto, 46° longe):
- 1 px ≈ 3,3 cm a 67 m e 4,4 cm a 100 m (em 1920 px);
- um cubinho de vidro ocupa 0,05–0,25 px, um carro 100–140 px, um arco de 0,5 m 11–15 px.

## A — o tanque esmagando carro

### O real

**Vidro**
- **Laterais e traseiro: temperado.**
  - Estilhaça de uma vez em cubinhos de ~2,5–8 mm (norma: 40–400 cacos em 5×5 cm, nenhum maior que 3,5 cm²).
  - Uma janela de ~0,35 m² dá 5.600–56.000 cacos.
  - Voa pouco: 1–2 m/s, alcance de 0,5–1,5 m, muito cai para dentro do carro (conta pela energia guardada, ~5,7 J/kg).
  - Muitas vezes cai em blocos ainda grudados (est.).
- **Para-brisa: laminado.** Racha em teia e fica preso ao PVB; visto de cima fica leitoso (est.).
- **Brilho ao sol.** Um caco alinhado sol→câmera deixa o pixel dezenas a milhares de vezes mais claro que a areia, mesmo cobrindo 1–2% dele. São poucos ao mesmo tempo: de alguns a dezenas por janela, piscando por milissegundos. O contraluz de três-quartos dos game shots favorece (ângulo rasante reflete 17–39%).

**Lataria e o resto**
- A lataria amassa "como uma lata" (Drive-a-Tank, Chieftain de 60 t).
- Pneus estouram (est.): ~40 L a 2,2 bar, um estalo e um sopro de poeira.
- O ar sai pelas janelas quebradas, levando poeira de dentro (est.).
- Fica mancha de óleo, arrefecimento e gasolina; fogo é raro (est.).

**Faísca:** rara e pouca.
- Sapatas com almofada de borracha (T-158).
- Aço contra aço dá pouca ou nenhuma faísca abaixo de ~6 m/s (van Wingerden 2019). Só a barriga do casco raspando o teto.
- De dia, quase invisível.

**Tempo:** a passagem dura (comprimento do carro + 4–5 m de esteira no chão) ÷ velocidade. A 4 m/s, são ~1,6–2,3 s.

### Os jogos

**Zero Hour (INIs do TheSuperHackers/GeneralsGamePatch)**
- 3 modelos do carro (frente amassada, traseira amassada, os dois); o som `CarMount`; o alarme em 50% das vezes.
- `FX_CarCrush`: 5 nuvens cinza que somem em 1,7 s e 8 sprites de terra (225, 186, 142) que duram 0,8 s.
- A carcaça fica 14 s e some aos 20 s.
- O `CarCrushGlassDebris` existe nos INIs, mas não é usado em nenhum efeito: o vidro ficou de fora, e não tem faísca.

**Outros**
- BeamNG: o vidro tem 2 estados (inteiro ou quebrado).
- World of Tanks: carros e postes destrutíveis, com prédios quebrando em Havok; sem detalhe publicado.
- Brilho menor que 1 px: a literatura de "glints" (Zirr & Kaplanyan 2016; Deliot & Belcour 2023).

### A receita (a calibrar no jogo)

**Linha do tempo, em fração da passagem T:**
1. 0: a frente amassa e caem lascas de lanterna.
2. 0,2T: as laterais estouram e o para-brisa fica leitoso.
3. 0,35T: os pneus da frente estouram.
4. 0,5T: o teto cede, o traseiro estoura e sai poeira pelas janelas.
5. 0,65T: o casco raspa no teto.
6. 0,8T: os pneus de trás estouram.
7. Depois: a poeira assenta em 2–4 s e a mancha cresce em 10–20 s.

| Camada | Números |
|---|---|
| Brilhos do vidro | Quads aditivos com normal sorteada, brilho pelo alinhamento sol→câmera. 30–80 lampejos por janela no voo (0,4–0,6 s), de 1–2 quadros cada, mínimo de 2 px com intensidade compensada (est.), pico de 3–6× o branco da areia |
| Blocos de vidro | 20–40 malhas de 3–10 cm a 1–2,5 m/s, restituição 0,1–0,2; no 1º toque viram 3–6 brilhos |
| Chão de vidro (o que mais lê) | Mancha com cintilação: raio de 1,5–2,5 m, mais densa junto às portas, células de 2–3 cm, 5–15% com caco, reflexo de 1–2° |
| Para-brisa | "Leitoso" no material da carcaça (com o Look) |
| Lanternas | 6–12 lascas vermelhas e âmbar de 5–15 cm (leem muito bem a 100 m) |
| Poeira | Volumes tingidos com a cor do chão: no contato, 1–2 de 2–3 m; nas janelas, 2–4 de 1–1,5 m; um anel baixo no pneu. Vida de 2–4 s, **teto de 1,5 m** |
| Faísca | 0–1 rajada só no raspão do casco: 5–12 faíscas de 0,1–0,3 s; de dia, quase nada |
| Pisca-alerta | Em 50% dos carros (o alarme do ZH): âmbar, 1,5 Hz, por 10–30 s |
| Fluido | Mancha escura de 0,3 → 1,2 m em 10–20 s |
| Luz pontual | Nenhuma de dia |

## B — o poste derrubado com choque

### O real

**Onde acontece o arco: é isto que deixa o efeito "bem colocado"**
- Fio caído na areia seca puxa ~0 A (molhada ~15 A, grama seca 25 A, concreto armado 75 A; ABB Review 2004). No deserto, o fio no chão quase não arqueia e fica vivo em silêncio.
- O arco forte vem quando:
  - uma fase toca outra fase;
  - uma fase toca o neutro ou o fio de baixa tensão (fios embolando na queda);
  - uma fase toca metal aterrado.
- **O arco fica nos pontos de contato, não espalhado pela areia.**
- Quando arqueia no chão, o fio "queima e pula" e deixa uma linha tracejada de fulguritos (areia vitrificada).

**Cor, forma e piscar**
- O arco chega a ~19.400 °C.
- Cor: halo azul-violeta (bandas do nitrogênio), linhas verdes do cobre, alumínio queimando em branco.
- Forma em espiral pelas forças magnéticas. No contato fio-fio tem 5–50 cm; se durar mais de 0,3 s, sobe e estica até 1–2 m (est.).
- Pisca a 120 Hz (a rede de 60 Hz zera e reacende a cada meio ciclo); a 30–60 qps isso vira brilho aleatório quadro a quadro.

**Religador**
- Em geral 4 disparos até bloquear: 2 rápidos e 2 temporizados.
- Intervalos de 0,5 / 2 / 2 s (padrão do OpenDSS); no campo, 0–5 s, 10–20 s e 10–30 s.
- O disparo rápido corta em ~46 ms: cada clarão dura ~3 quadros.

**O resto**
- **Chave fusível do transformador:** estampido, jato de gás para baixo, e o porta-fusível cai e fica pendurado (detalhe barato e muito real).
- **Baixa tensão (220/127 V):** crepitar laranja-branco por minutos.
- **Gotas de metal:** alumínio de ~1,5 mm queimando enquanto cai; cobre fundido laranja.
- **Transformador:** só explode em falha interna. O "azul" dos vídeos é arco, como o céu de Astoria em 2018. Num poste derrubado, o mais provável é o fusível abrir (est.).
- **Isoladores:** porcelana estilhaça em lascas brancas; vidro temperado vira cubinhos.
- **Fio:** cai de 9–10 m (~1,4 s) e chicoteia.

### Os jogos e a técnica
- **Zero Hour:** o poste só tomba. O único "fio vivo" são os `LiveWireSparks` (2–5 pontos por rajada, vida de 0,7–1 s, amarelo → laranja → vermelho), um bom molde pras gotas.
- **Epic, feixe no Niagara:** fita com ciclo de 0,1–0,2 s, posição refeita todo quadro, largura em rampa.
- **Raio:** deslocamento do ponto médio, com a amplitude caindo pela metade por nível; ramos ocasionais; dois raios alternando.

### A receita (a calibrar no jogo)

**Roteiro (~10 s)**
1. O tanque bate; o poste inclina e fica pendurado nos fios por 0,5–2 s; os isoladores estouram.
2. Os fios se tocam no ar: **disparo 1**, clarão de 3–4 quadros, chuveiro de gotas e a chave fusível estoura.
3. +0,5 s: **disparo 2**, com os fios embolados no chão, no neutro ou num veículo.
4. +2 s: **arco temporizado** de 0,6–1,0 s, com o fio pulando e deixando o fulgurito.
5. +5 s: o último arco, depois o bloqueio.
6. Em paralelo, o crepitar da baixa tensão por 3–8 s.

| Camada | Números |
|---|---|
| Arco (fitas aditivas refeitas todo quadro) | 0,1–0,6 m nos clarões rápidos, 0,3–1,5 m nos temporizados. Núcleo branco-azulado de 5–8 cm, halo azul-violeta de 25–40 cm a 15–25%, rastro do quadro anterior a 40%. 12–24 pontos por metro, 4 níveis de ponto médio, amplitude de 12–20% do comprimento; 0–3 ramos de 1–2 quadros. Intensidade de 0,35–1,0 por quadro, 10–15% dos quadros apagados, 1 em 8 com tom verde |
| Gotas | 40–80 por clarão rápido, 20–40/s nos temporizados. Vida de 0,6–1,5 s, 3–8 m/s com gravidade cheia, 1,5–2 px de largura, branco-amarelo → laranja → vermelho; quicam 1–2 vezes; 20% com fiozinho de fumaça (est.) |
| Luz pontual | No máximo 2, raio de 8–12 m, azul-branca. Clarão: 0,5–1× o sol a 3 m por 1–2 quadros. Temporizado: 0,15–0,4× piscando (est.). Com a compensação de exposição dos destroços |
| Fumaça | Volumes de 0,5–1,5 m branco-acinzentados por 2–4 s; cabo queimando, fita cinza-escura por 3–6 s |
| Pedaços sólidos | 6–12 lascas de porcelana; o porta-fusível pendurado (com o Look) |
| Fio | Catenária com física (WorldForge); recua 1–2 m ao romper; ponta incandescente por 0,5–1 s depois de cada arco |
| Chão | Manchas vitrificadas de 10–30 cm em linha tracejada, permanentes |

**Custo (est.):** 3 fitas de ~40 pontos, até 150 gotas, 1–2 luzes sem sombra, 2–4 volumes pequenos e decais.

**Decisões do Felipe (perguntar quando chegar nos itens):**
- religador realista (10–30 s) ou de jogo (2–5 s);
- fio na areia seca realista (nada visível) ou com chiado;
- transformador pega fogo ou não.

## Fontes
Zero Hour ([GeneralsGamePatch](https://github.com/TheSuperHackers/GeneralsGamePatch): `FXList.ini`, `ParticleSystem.ini`, `CivilianUnit.ini`, `CivilianProp.ini`, `DamageFX.ini`); vidro ([US4198463](https://patents.google.com/patent/US4198463), [UN WP.29](https://digitallibrary.un.org/record/287804/files/%5BE_ECE_%5DTRANS_WP.29_GRSG_1999_30-EN.pdf), [Springer](https://link.springer.com/doi/10.1007/s40940-018-00091-1)); [Drive A Tank](https://www.cigaraficionado.com/article/drive-a-tank); faíscas ([van Wingerden 2019](https://www.aidic.it/cet/19/77/023.pdf)); glints ([Zirr & Kaplanyan 2016](https://research.nvidia.com/sites/default/files/pubs/2016-02_Real-time-Rendering-of/ZirrKaplanyan_MultiscaleI3D2016.pdf), [Deliot & Belcour 2023](https://diglib.eg.org/handle/10.1111/cgf14866)); rede ([ABB Review](https://library.e.abb.com/public/b0a2eef7ff6d957ac1256e3f004d5c8d/28-31%20M843.pdf), [OpenDSS religador](https://opendss.epri.com/Recloser.html), [PAC Basics](https://pacbasics.org/how-do-reclosers-work-setting-and-operation/), [chave fusível](https://en.wikipedia.org/wiki/Fuse_cutout), [Arc flash](https://en.wikipedia.org/wiki/Arc_flash), [espectro com cobre](https://arxiv.org/pdf/physics/0410267), [Astoria](https://abc7ny.com/4977959/), [Dallas](https://www.nbcdfw.com/news/local/Video-Shows-Sparks-Arcing-on-Dallas-Power-Line-489102091.html)); técnica ([Epic, feixe no Niagara](https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-beam-effect-in-niagara-for-unreal-engine)).
