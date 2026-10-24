# O diesel dos veículos

Estudo e plano da sessão do jogo, 16/10. O pedido do Felipe: "o diesel preto saindo atrás do tanque", como no vídeo de referência (`Downloads/gemini_generated_video_a0c04d8e.mp4`, os últimos takes do tanque de perto).

- **Regra:** a do CLAUDE.md. Toda fumaça tem volume, recebe a sombra do mapa, faz sombra e é acesa pelo fogo e pelas explosões.
- **[E]** marca uma estimativa ou uma conta minha, não um número da fonte.

## O fenômeno real

**A fumaça preta é o turbo atrasado.** Na arrancada o combustível sobe na hora, e a pressão do turbo demora.
- Por meio segundo a um segundo e meio falta ar, e sai a baforada.
- Na troca de marcha o motorista tira o pé e pisa de novo: sai outra, menor.
- Quando o turbo enche, some ([SAE 2010-01-1287](https://saemobilus.sae.org/content/2010-01-1287); [patente US4502437](https://patents.google.com/patent/US4502437A/en): um caminhão em regime faz cerca de 8% de opacidade, e na troca pula pra cerca de 70% por um instante).

**Também solta fumaça:**
- **Em carga alta com rotação baixa:** a subida, a lâmina do Dozer empurrando.
- **Na partida a frio:** fumaça branca, de combustível que condensa ([SAE 920032](https://saemobilus.sae.org/papers/diesel-engine-cold-starting-white-smoke-920032)).

**Em marcha lenta e em cruzeiro, com o motor quente, quase não sai nada.**

**A turbina do Abrams (AGT1500) não solta fuligem** ([Honeywell](https://www.honeywellaerospace.com/us/en/about-us/blogs/15-surprising-facts-about-honeywell-1500-horsepower-turbine-tank-engine)). O que se vê atrás dela é o ar quente tremendo e a poeira do chão soprada pela grade traseira.

## Os números

| O quê | Valor | Fonte |
|---|---|---|
| Albedo da fuligem fresca | 0,20–0,30; diesel medido 0,17–0,20 | [Bond e Bergstrom 2006](https://lweb.cfa.harvard.edu/hitran/HITRAN2012/Aerosols/papers/bond_aerscitech_2006.pdf) |
| Absorção | ∝ 1/λ: R/G/B = 0,85 / 1 / 1,22 (o azul absorve mais, a fuligem ao sol fica marrom-escura) | idem |
| Fase | fração pra trás 0,16–0,18, o que dá g ≈ 0,5 [E] | idem |
| Gás na saída, V12 diesel de ~950 kW a plena carga | 440–509 °C, 2,5–3,5 m³/s, 700–950 kW de calor | [Cat C32](https://v2.zeppelin.sk/userfiles/dokumenty/Cat%20C32%20-%201250%20kVA.pdf) |
| Tanque diesel de ~1100 kW (Leopard 2, Type 99, Merkava 4) | ~1,6 kg/s, ~0,9 MW no escape [E] | escala do Cat |
| Empuxo (Briggs) | F = g·Q/(π·cp·ρ·T) ≈ 8 m⁴/s³ a plena carga; Δh = 1,6·F^⅓·x^⅔/u | [fórmula](https://arxiv.org/pdf/2302.07416) |
| Opacidade de inspeção | k de 1,5–3 m⁻¹ (opacímetro, L = 0,43 m); aceleração livre de 1 a 4 s | [SAE J1667](https://saemobilus.sae.org/content/J1667_201802), [MOT](https://www.gov.uk/government/publications/mot-changes-from-may-2018-guidance-for-mot-testers/diesel-vehicle-emission-limits) |
| Diluição | ~1000:1 em 1–2 s; o gás fica a menos de 1 K do ar | [NREL](https://fischer-tropsch.org/DOE/DOE_reports/NREL/SR-570/SR-570-28521/sr_570_28521_attch25.pdf) |

**Onde fica o escape:**
- **T-72 e T-90:** atrás, do lado esquerdo, sai de lado.
- **Type 99:** na traseira.
- **Leopard 2:** atrás e pra baixo, espalhando no chão ([de.wikipedia](https://de.wikipedia.org/wiki/Leopard_2)).
- **Abrams:** a grade do meio da traseira.

**Como os jogos fazem:**
- **War Thunder:** a emissão segue o estado do motor. Os jogadores reclamam de fumaça demais em marcha lenta e da cor errada.
- **Unity, tutorial oficial:** a quantidade é a rotação vezes uma constante, e a fumaça vive 2,5 s.
- **Frostbite:** as partículas entram num volume iluminado e com sombra ([Hillaire 2015](https://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/)). É o padrão mais perto da nossa regra.
- **Não achei fonte técnica** de World of Tanks, Company of Heroes, Steel Division nem Battlefield.

## Como ficou no jogo

**Um sistema só.** A fuligem é um tipo de puff na fumaça do míssil (`Game/Source/RTSGame/Private/RTSMissileSmoke.cpp`, `TickExhausts`). Por isso ela já tem tudo o que a fumaça tem:
- o vento do mapa;
- os redemoinhos;
- o crescimento de Richardson–Obukhov;
- o empuxo de Briggs;
- o empurrão de quem passa, incluindo a esteira do próprio veículo, que puxa a fumaça;
- a sombra de um puff no outro;
- a sombra do mapa (o RTSSunShadow da Arte);
- as luzes das explosões e do fogo (o RTSSceneLights do Fluxo);
- a sombra no chão;
- o limite em cima das unidades.

**O motor de cada veículo, só no desenho:**
- **O veículo "virtual":** ganha velocidade pela potência, a = P/(m·v) − μg. Na simulação do RTS a arrancada é quase instantânea; um motor de verdade leva ~2–5 s.
- **O acelerador:** vai a fundo enquanto o veículo virtual não alcança a velocidade da simulação.
- **As marchas automáticas:** sobem a cada 1/N da velocidade máxima, tirando o pé por 0,25 s [E].
- **O turbo:** segue o acelerador com atraso, enchendo em 0,8 s e esvaziando em 0,3 s.

**A fuligem**, em m² de extinção por segundo: a opacidade do gás (k, m⁻¹) vezes a vazão (~3,5 m³/s a plena carga).
- **A fórmula:** (potência/1100 kW) × (10·max(0, acelerador − turbo) + 2,1·acelerador).
- **A arrancada do tanque:** perto do limite da aceleração livre de um diesel turbo de antes de 2008 (3,0 m⁻¹, o MOT), o que dá ~10 m²/s.
- **O cruzeiro carregado na areia:** k ~0,6 m⁻¹ [E], ~1 m²/s, um rastro escuro contínuo.
  - **Por que subiu:** a 1ª volta usava 0,3 m²/s, o número de um caminhão moderno, e não aparecia no jogo. O diesel militar sem filtro fuma bem mais, e o vídeo de referência mostra o rastro preto andando.
- **A marcha lenta:** quase nada.

**O puff:**
- nasce a cada 0,05 s na saída, com o jato a 25–30 m/s que freia em ~0,1 s e o σ do cano;
- o empuxo é B = F/u, a pluma inclinada de Briggs, com u = o ar que passa pelo veículo;
- a fuligem é toda sólida: não seca como a névoa do míssil.

**O material:** a "chama" negativa marca a fuligem.
- Albedo (0,282; 0,25; 0,215) e g 0,5.
- Cada oitava de espalhamento múltiplo leva o albedo de novo, então a fuligem quase não tem a 2ª.
- A do míssil continua branca: Al2O3, 0,97, g 0,7.

**As fichas** (`Game/Config/RTS/Units.json`, `visual.exhaust`):

| Unidade | Motor | Massa | Saída |
|---|---|---|---|
| Tanque | 1100 kW | 60 t | a placa de trás com as aletas, pra trás e um pouco pra cima |
| Dozer | 340 kW | 49 t | as duas chaminés com o cotovelo; a obra carrega o motor em 0,7 |
| Caminhão | 330 kW | 20 t | a chaminé atrás da cabine |

O Dozer é do tipo Cat D9 e o caminhão do tipo HEMTT.

**Leitura do RTS:**
- A fumaça na frente de uma unidade fica no máximo com τ 0,25 somado na linha da vista.
- O veículo sob a névoa não solta fumaça (a fumaça entregaria a posição).
- Só os veículos a menos de 150 m do foco da câmera soltam.

## Decisão (17/10, ~01h)

O Felipe: "já que os tanques tecnológicos e o trator da USA não têm diesel, então pode tirar; a gente foca nisso quando chega nos tanques mais pesados a diesel".
- Saíram as fichas `visual.exhaust` das três unidades dos EUA. O código fica, parado sem ficha.
- O que ele viu na última versão: "não estou vendo, cadê? só vejo poeira atrás". Ao sol baixo, a fuligem com o albedo medido some na poeira das lagartas.
- Na volta, com a China e a GLA, a fuligem tem de se ler de cima: mais preta perto do escape, saindo por cima da poeira e mais tempo inteira antes de diluir. Provar em vídeo de lado, com A/B.

## Pendente
- **Decisão do Felipe: o tanque dos EUA fica diesel (como no vídeo) ou vira turbina, como o Abrams de verdade?** A turbina não solta fuligem: solta ar tremendo e a poeira soprada atrás. Hoje está diesel, como ele pediu.
- **A partida a frio,** com fumaça branca no começo da partida: depois.
- **As facções novas:**
  - China (tipo Type 99): diesel pela traseira;
  - GLA (linhagem T-72): diesel pelo lado esquerdo, atrás;
  - a picape técnica: o cano curto embaixo.
