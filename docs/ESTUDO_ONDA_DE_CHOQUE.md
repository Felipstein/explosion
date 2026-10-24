# Estudo: a onda de choque na areia (o disparo e a explosão do tanque)

Estudo da sessão Fluxo (16/10), com fontes, para os itens 8 e 10 da fila em `docs/TIRO_E_ACERTO.md`. **[E]** = estimativa ou conta.
- As contas de onda de choque são do Kingery-Bulmash (coeficientes de Swisdak 1994, explosão na superfície). O script está no scratchpad da sessão Fluxo (`kb.py`).
- Os vídeos em câmera lenta não foram vistos quadro a quadro.
- 1 m ≈ 30 px na câmera do jogo.

## A) O disparo do canhão de 120 mm ("o que acontece quando um tanque atira?")

**A sequência** (t = 0 quando o projétil sai da boca):

| t | O que acontece | Onde, cor |
|---|---|---|
| −0,5 a 0 ms [E] | O gás precursor (o ar do cano empurrado na frente do projétil) | na boca; não se vê de cima |
| 0 a 1 ms | **O clarão primário:** o gás quente logo atrás do projétil | 0,3–0,6 m, branco-amarelo |
| 0 a 3 ms | **O jato e o "mach disk"** (o choque parado dentro do jato), com o clarão intermediário avermelhado atrás. Pela fórmula de Ashkenas-Sherman, o disco fica a 1,5–2,5 m da boca [E] | 45–75 px |
| 1 a 5 ms | A onda principal, mais forte pra frente; o gás sai todo em poucos ms | |
| 3 a 50 ms | **A onda bate no chão e levanta a areia** | |
| 5 a 50 ms, às vezes | **O clarão secundário:** o gás rico em CO/H₂ reacende misturado com o ar | bola laranja de 2–5 m [E]; de dia quase não se vê |
| 0,1 a 3 s | **A fumaça cinza-clara** se abre e vai com o vento. Entre 0,3 e 1,5 s, um sopro pequeno pela boca, do evacuador [E] | |

**O que isso derruba no jogo de hoje**
- **Nenhum desses canhões tem freio de boca.** Não têm o M256 (M1), o Leopard 2, o 2A46 (T-72/T-90) nem o ZPT-98 (ZTZ-96/99): o sabot da munição flecha se despedaçaria nele. Todos têm evacuador. O sopro sai pra frente, no eixo, e não pros lados.
- **A fumaça é cinza-clara a branca** (sais de potássio da carga). O marrom e o ocre dos vídeos de deserto é areia do chão [E]. A nuvem tem 3–5 m e dura 1–3 s [E].

**A sobrepressão no chão** [E], tratando a boca como 1–3 kg de TNT a 2 m de altura:

| Distância à frente | Pressão | Chegada |
|---|---|---|
| 2 m | 90–190 kPa | 3 ms |
| 5 m | 28–51 kPa | 9 ms |
| 10 m | 11–18 kPa | 22 ms |
| 20 m | 5–7 kPa | 50 ms |

**A areia: dois motores**
1. O choque levanta a poeira **no lugar**: o ar só anda 0,1–0,3 m.
2. O jato de gás varre o chão pra frente. Uma carta de 1942 já dizia que a poeira vem dos gases batendo no chão em frente à boca, e uma patente diz que quanto mais rasante o tiro, mais poeira.

O resultado é um **leque baixo pra frente**. Ele começa a ~2 m da boca, onde o choque refletido forma o "Mach stem" rente ao chão, e cresce depois. O atirador fica cego por 3,4–3,7 s (teste do Firefly), e no Golfo a poeira entregava a posição.

**A receita do disparo** [E]:

| Camada | Tempo | Tamanho | Cor |
|---|---|---|---|
| Clarão (primário + mach disk) | 1 quadro | núcleo de 0,6–1 m, cone no eixo até 1,5–2,5 m | branco-amarelo; luz de 8–10 m por 1–2 quadros |
| Bola secundária (opcional) | quadros 1–4 | 2–4 m, 2–4 m à frente | laranja ×0,3 de dia |
| Jato cinza | sai a 150–300 m/s no eixo, freia (τ ≈ 60 ms), para a 4–8 m; raio de 0,5 a 2,5 m; 1,5–3 s | | cinza-claro levemente quente, opacidade 0,6 → 0 |
| Saia de areia | nasce a ~2 m em 3–5 ms e varre a ~400 m/s | leque de ±50–60°, de 2 a 12 m; 0,2–0,5 m de altura ao nascer, 1–2 m em 0,5–1 s; some em 2–4 s | a cor do chão +10–20% de luz, opacidade ≤ 0,3 |
| Sopro do evacuador | 0,3–1,5 s | 0,5–1 m | opacidade 0,3 |

- **Leitura:** nada acima de ~2 m por mais de 0,5 s, e a poeira à frente do casco, não por cima dele.

## B) A explosão do tanque (10–20 kg de TNT equivalente [E])

**O raio da frente de choque** (10, 15 e 20 kg diferem menos de 10%):

| Tempo | 5 ms | 10 ms | 20 ms | 30 ms | 50 ms | 75 ms | 100 ms | 150 ms |
|---|---|---|---|---|---|---|---|---|
| Raio | 5,4 m | 8,0 m | 12,2 m | 16,0 m | 23,4 m | 32,4 m | 41,3 m | 58,6 m |

A 60 qps, o quadro 1 está em ~11 m, o 2 em ~17 m, o 3 em ~23 m e o 6 em ~41 m. A frente cruza a tela (63 m) em ~10 quadros.

**No chão, para 15 kg:**

| Distância | Pressão | Vento atrás do choque | Quanto o ar anda |
|---|---|---|---|
| 5 m | 275 kPa | 361 m/s | 0,7 m |
| 10 m | 63 kPa | 122 m/s | 0,4 m |
| 20 m | 20 kPa | 44 m/s | 0,2 m |
| 30 m | 11,5 kPa | 26 m/s | 0,1 m |

A fase positiva dura 5–13 ms.

**O que muda tudo: a areia não voa pra fora, ela sobe no lugar quando o choque passa.** O anel que o olho vê é a frente onde a areia começa a subir, andando na velocidade do choque.
- **Como sobe:** pelo cisalhamento atrás do choque e pela força de Saffman (Fedorov), com alguns ms de atraso. Areia solta solta ~6× mais poeira que o solo de Nevada (NRC 1985).
- **A altura:** em Suffield (500 t), a poeira foi a 15 m. Pela raiz cúbica, ~0,5 m para 15 kg [E].
- **As zonas** [E]: densa até ~12 m, média até ~20 m, tênue até ~30 m, nada além de ~40 m.
- **Depois do choque:** os gases e o vento que vem atrás empurram uma saia densa de 2–4 m de altura até 12–15 m em ~1 s. A fase negativa e a coluna subindo puxam a parte de dentro.
- **O anel de condensação** (nuvem de Wilson) precisa de ar úmido: no deserto quase não acontece. O que se vê é a refração contra o chão texturizado, forte só onde a pressão passa de dezenas de kPa (≤ 15–20 m), e a própria poeira.
- **A ressalva:** munição que deflagra em vez de detonar tem pico menor e mais longo [E]. O Abrams solta a explosão pra cima, pelos painéis de alívio.

**A receita da explosão** [E]:

| Camada | Tempo | Tamanho | Cor |
|---|---|---|---|
| Anel de distorção | quadros 1–4 (raio 11 / 17 / 23 / 29 m) | 1–2 m de largura, plano a +0,1 m | deslocamento de 4 px até 0, pela pressão |
| Varredura de areia | a fonte segue o raio da tabela, 2–5 ms atrás | densa ≤ 12 m, média ≤ 20 m, tênue ≤ 30 m; 0,3–0,6 m de altura | opacidade 0,5 / 0,25 / 0,1; sem velocidade pra fora, subindo a 1–4 m/s |
| Saia densa (o anel rolando) | 0,05–1,5 s | 2–4 m de altura, rola de 15 a 2 m/s e para em 12–15 m | a cor do chão, a base na sombra |
| Sucção pra coluna | 0,3–3 s | a parte de dentro do anel | |
| Névoa que fica | até 4–8 s | anel de 10–25 m, ≤ 0,8 m de altura | some com o vento |
| Calor sobre o fogo | enquanto queimar | 4–6 m | 1–2 px de distorção |
| Decal | fica | 3–5 m queimado, até 10–12 m varrido | junto com o avental do Look |

- **Não desacelerar o choque** (fica falso): quem conta a história é o rastro que ele deixa.
- **Leitura:** fora de 12 m, a poeira fica abaixo de 0,6 m e com opacidade ≤ 0,25 por menos de 1 s.
- **Custo** [E]: 0,2–0,5 ms na 1660. Precisa medir.

## Os jogos
- **War Thunder:** o clarão em partículas pelo calibre, com brilho fotométrico; nuvens de gás, fumaça e poeira por tipo de chão. O update 1.47 pôs distorção no tiro.
- **World of Tanks:** o 2.4 juntou os efeitos num sistema só e criou a opção de reduzir efeitos em volta da mira (a nossa regra da leitura).
- **Men of War AS2:** poeira no corpo do veículo ao atirar; efeitos de calibre grande por chão.
- **Arma 3:** o mod Blastcore Murr põe onda e poeira no tiro de canhão (a poeira cega a IA).
- **Frostbite (Battlefront II):** muitas camadas pequenas, flipbooks e 6 mapas de luz num atlas (a mesma ideia das nossas texturas de 6 direções).
- **A técnica comum do anel** (Polycount/RealtimeVFX): malha de anel com depth fade, partículas na borda e decal.
- **A distorção na Unreal:** Refraction com Pixel Normal Offset; o custo cresce com a área da tela.

## Fontes
- Kingery-Bulmash/Swisdak: [DTIC ADA526744](https://apps.dtic.mil/sti/pdfs/ADA526744.pdf), [coeficientes](https://github.com/fcento100/kingery-bulmash)
- [Muzzle flash](https://en.wikipedia.org/wiki/Muzzle_flash), [Muzzle brake](https://en.wikipedia.org/wiki/Muzzle_brake), [M829](https://en.wikipedia.org/wiki/M829), [2A46](https://en.wikipedia.org/wiki/2A46_125_mm_gun)
- [Klingenberg & Heimerl (Fraunhofer)](https://publica.fraunhofer.de/entities/publication/c8aa90ff-0ee2-4f9b-8d4a-af48a3cf4978), [NDIA/ARDEC: M256 sem freio](https://ndia.dtic.mil/wp-content/uploads/2006/garm/tuesday/smith.pdf), [Ashkenas-Sherman](https://gsa.confex.com/gsa/2009AM/webprogram/Paper166129.html)
- Patentes: [US6308608 (poeira)](https://patents.google.com/patent/US6308608), [US4315785 (fumaça do potássio)](https://patents.justia.com/patent/4315785), [USH18](https://patents.google.com/patent/USH18H/en), [US4260384](https://patents.google.com/patent/US4260384)
- [Carta de 1942 (UEA)](https://archivecollections.uea.ac.uk/sz.oemu.2.8.2.17), [Gulflink: a poeira na Arábia Saudita](https://gulflink.health.mil/declassdocs/centcom/19960424/DOC_87_DUST_CONTROL_IN_SAUDI_ARABIA_007.html), [o teste do Firefly](https://worldoftanks.com/en/news/history/The_Chieftains_Hatch_Firefly2/)
- [Suffield 1964 (DTIC)](https://apps.dtic.mil/sti/pdfs/AD0619787.pdf), [NRC 1985, Dust](https://nap.nationalacademies.org/read/540/chapter/5), [Fedorov](https://mathnet.ru/eng/fgv1692), [Shock Waves 2016](https://link.springer.com/article/10.1007/s00193-016-0661-0)
- [Condensation cloud](https://en.wikipedia.org/wiki/Condensation_cloud), [Hargather 2013 (BOS)](https://archive.aps.org/dfd/2013/m24/4)
- Jogos: [War Thunder](https://warthunder.com/en/news/6984-development-visual-effects-for-ground-vehicles-en), [WoT 2.4](https://worldoftanks.eu/en/news/general-news/2-4-upgraded-vfx-sfx/), [Battlefront II (Simon Schreibt)](https://simonschreibt.de/gat/battlefront-ii-layered-explosion/), [Polycount: anel de poeira](https://polycount.com/discussion/comment/2400917), [Pixel Normal Offset](https://dev.epicgames.com/documentation/en-us/unreal-engine/refraction-using-pixel-normal-offset-in-unreal-engine)
- Vídeos pra conferir quadro a quadro: [Slow Mo Guys, Sherman](https://www.youtube.com/watch?v=xpJ8EoGmLuE), [T-90 em câmera rápida](https://www.military.com/video/guns/tank-guns/t90-firing-caught-on-high-speed/1009222225001), [M1A2C em câmera lenta](https://taskandpurpose.com/news/army-m1-abrams-tank-video.md)
