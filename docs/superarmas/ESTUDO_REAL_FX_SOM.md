# Estudo: superarmas — o mundo real, o efeito visual e o som (01/11/2026)

Pesquisa para as superarmas das 4 facções (USA, China, GLA, Rússia). Nada foi decidido; as ideias fracas também estão aqui.

**Convenções**
- **[E]** = estimativa ou conta minha (o script está em `scratchpad/calc/sw.py`).
- **[NC]** = número que circula mas **não está confirmado** (alegação oficial, imprensa, fonte única ou fonte fraca).
- **Blast:** Kingery-Bulmash/Swisdak, explosão no chão (hemisférica), o mesmo usado em `docs/ESTUDO_ONDA_DE_CHOQUE.md`. Conferi o script contra aquele estudo: 15 kg dá 5,4 m em 5 ms e 58,6 m em 150 ms, igual.
- **Nuclear:** metade da energia vira onda de choque ([Effects of nuclear explosions](https://en.wikipedia.org/wiki/Effects_of_nuclear_explosions): 50% blast, 35% térmico, 5% radiação inicial, 5–10% resíduo), então W_TNT = 0,5·W. Conferido com Glasstone 1957: "2 psi a 0,53 milha de 1 kt na superfície" ([extrato](https://archive.org/download/DnaEm1CapabilitiesOfNuclearWeapons/Glasstone%20Effects%20of%20Nuclear%20Weapons%201957%20edition%20EXTRACT.pdf)); o script dá W_TNT = 524 t, ou seja, 0,52 do nuclear.
- **Não repito** o que já está em `docs/EXPLOSOES.md` (fórmulas de Taylor–Sedov, a nuke do ZH, o FAF/BAR, o Fallout em resumo) nem em `docs/CONCEITO_SOM.md` (camadas estalo/corpo/cauda, d/343, microfone no ponto de mira, ducking). Aqui eu aprofundo e corrijo.

---

## 0. A régua: o que "destruição forte" quer dizer em metros

**Os anéis de pressão** (Glasstone e [Effects of nuclear explosions](https://en.wikipedia.org/wiki/Effects_of_nuclear_explosions)):

| Pressão | O que acontece | Vento atrás do choque ([Glasstone, Tab. 3.07](https://www.fourmilab.ch/etexts/www/effects/eonw_3.pdf)) |
|---|---|---|
| 1 psi (6,9 kPa) | vidros, dano moderado em casas | ~17 m/s [E] |
| 2 psi | dano leve a médio, infantaria derrubada | 31 m/s |
| 5 psi (34 kPa) | a maioria dos prédios civis destruída; postes caem (Glasstone, Fig. 5.104) | 73 m/s |
| 10 psi | limiar de "dano severo" | 131 m/s |
| 20 psi (140 kPa) | área urbana arrasada, concreto pesado demolido | 224 m/s |

**O raio de 5 psi em função da carga:**
- R(5 psi) = **5,7·W^(1/3)** m (W em kg de TNT), com a carga **no chão** [E, KB hemisférico].
- O **4,6** do simulador antigo é o mesmo 5 psi, só que **no ar livre** (KB esférico): 5,7 / 1,8^(1/3) ≈ 4,7.
- A nuke antiga (112 m) era, portanto, uma nuclear de **~14–15 t**. É o porte da Davy Crockett, já está na faixa certa.

**Para dar 60–120 m de "destruição forte" (5 psi):**

| R(5 psi) | TNT no chão | Nuclear no chão |
|---|---|---|
| 60 m | 1,2 t | 2,3 t |
| 90 m | 3,9 t | 7,8 t |
| 120 m | 9,3 t | 18,7 t |

- O 18 t é o **Little Feller I**, a W54 da Davy Crockett, 1962 ([DOE/NV-1200](https://nnss.gov/wp-content/uploads/2023/04/DOENV_1200.pdf)).
- **A MOAB (11 t de TNT) dá 5 psi a 127 m** [E]. A análise das fotos de satélite da Alcis achou **38 prédios e 69 árvores destruídos num raio de 150 m**, o choque fazendo estrago entre 135 e 450 m, e **nenhuma cratera identificável** ([Alcis via resumo](https://medium.com/alcis-stories/after-the-dust-settles-d167fa0e7599), [Fox](https://www.foxnews.com/world/analysts-downplay-effectiveness-of-moab-strike-in-afghanistan)). A conta e o mundo real batem.

**Proporção fixa entre os anéis (física, para qualquer arma):**
- 20 psi : 5 psi : 2 psi : 1 psi ≈ **1 : 2 : 3,8 : 6,5**.
- Explosão no ar na altura ótima: o anel de 5 psi no chão fica **~1,4×** maior que o da explosão no chão. A conta: 1 kt dá 630 m no ar contra 453 m no chão; 10 kt dá 1.350 m ([cálculo de ogivas táticas](https://archive.org/download/DnaEm1CapabilitiesOfNuclearWeapons/Neutron%20bomb%20calculations.pdf)) contra 977 m [E].

**Atenção na escala do jogo:**
- No zoom mais perto (tela de ~63 m de largura, ver `ESTUDO_ONDA_DE_CHOQUE.md`), um anel de 120 m é **4× a largura da tela**.
- A leitura então não é "ver o anel inteiro". É ver a varredura saindo da tela e o mundo sumindo. O anel completo aparece no minimapa e na marca no chão.

---

## 1. As armas reais

### 1.1 USA

**Tomahawk TLAM-D** (o míssil de cruzeiro com submunições) ([Wikipedia](https://en.wikipedia.org/wiki/Tomahawk_missile)):
- **Lança:** de célula vertical Mk 41 (navio) ou submarino. O canister é o próprio tubo; um booster sólido tira o míssil e acende o turbofan.
- **Voo:** subsônico (~Mach 0,74, ~920 km/h) a **30–50 m do chão**. TLAM-D Block III: 1.300 km de alcance.
- **Carga:**
  - 166 BLU-97/B em **24 canisters** (22 com 7 bombinhas, 2 com 6);
  - solta **dois canisters por vez, um de cada lado, de trás pra frente**;
  - pode atacar **até 5 alvos** no mesmo voo.
- **Como se vê:** um míssil baixo e rápido, com um "rosário" de pacotes saindo dos lados. Não há clarão alto: é chuva rasante em fila.

**A bombinha BLU-97** ([CBU-87 na Wikipedia](https://en.wikipedia.org/wiki/CBU-87_Combined_Effects_Munition)):
- **Tamanho e carga:** 20 × 6 cm, 1,5 kg, **287 g** de explosivo. Carga oca, casca estriada e **anel de zircônio incendiário** (as faíscas brancas).
- **Queda:** cai freada por um balão desacelerador.
- **Pegada da CBU-87 (202 bombinhas):** de **20×20 m** (solta baixa, giro lento) a **120×240 m** (alta, giro rápido).
- **Falhas:** 5% segundo o fabricante; **7–8%** no Kosovo (Landmine Action). As falhas viram minas.

**ATACMS Block I** ([designation-systems](https://designation-systems.net/dusrm/m-140.html), [CSIS](https://missilethreat.csis.org/missile/atacms/)):
- **950 bombinhas M74** espalhadas em **~33.000 m², um círculo de ~205 m**. Esse número público foi admitido até 2024.
- Cada M74 tem **raio letal de 15 m** contra pessoas (FM 6-60, [análise](https://www.generalstaff.org/BBOW/BalTerm/Coverage_Bomblets.htm)).
- Abrir mais alto espalha mais, mas mata menos.
- **É a régua da "chuva de submunições" sobre uma base: 205 m é exatamente o tamanho de uma base nossa.**

**MIRV e os riscos de Kwajalein:**
- **As fotos:** as famosas são **exposições longas**, por isso aparecem riscos, não pontos ([10 Peacekeeper em 1984](https://download.osmand.net/wiki/File:Ten_LGM-118_Peacekeeper_MIRVs_over_Kwajalein_in_1984.jpeg); [8 veículos atravessando nuvens, 20/12/1984](https://www.themilitarystandard.com/missile/peacekeeper/reentrypics/)).
- **O Peacekeeper:** 10 Mk-21 com W87 de 300 kt; um "ônibus" de 4º estágio solta as ogivas **no espaço**, manobrando entre uma e outra ([fact sheet USAF](https://www.warren.af.mil/About-Us/Fact-Sheets/Display/Article/2869085)).
- **O Minuteman III original:** 3 ogivas W62 ([MIRV](https://iiab.live/kiwix/content/wikipedia_en_top1m_maxi_2024-05/A/MIRV)).
- **A reentrada:**
  - entra a ~6–7 km/s ([IDA D-4792](https://www.ida.org/~/media/Corporate/Files/Publications/IDA_Documents/STD/D-4792.pdf));
  - **começa a brilhar a ~20 km** de altura (o Hwasong-14 visto pela câmera da NHK; aquecimento sério a 25–30 km) ([38 North](https://www.38north.org/2017/07/melleman073117/), [Space.com](https://www.space.com/37687-north-korea-icbm-test-re-entry-vehicle.html));
  - de 100 km ao chão leva ~15–25 s [E].
- **Importante:** a liberação das ogivas **não tem clarão nenhum**. O "estouro alto" com clarão da nossa ideia é licença de jogo (ver §3.1).

**Conventional Prompt Global Strike e Dark Eagle (LRHW):**
- **Dark Eagle:**
  - booster de 2 estágios num caminhão, que solta o planador C-HGB;
  - **~2.775 km**, **acima de Mach 5**, ogiva **convencional**;
  - o resto é sigiloso, e há números de velocidade contraditórios na imprensa [NC] ([Wikipedia](https://en.wikipedia.org/wiki/Long-Range_Hypersonic_Weapon), [designation-systems](https://www.designation-systems.net/dusrm/app4/lrhw.html)).
- **O conceito CSM:** reentra a ~Mach 24, plana a Mach 10–15 e **chega a ~Mach 4** ([GlobalSecurity](https://www.globalsecurity.org/wmd/systems/ctm.htm)). As National Academies citam a manobra final para **impacto vertical** contra o que está enterrado ([NAP](https://nap.nationalacademies.org/read/12061/chapter/6)).
- **Plasma:** acima de ~Mach 5 forma uma bainha de plasma que corta o rádio e o GPS. É um problema maior para o míssil balístico com guiagem terminal do que para o planador (Acton 2015, via [typeset](https://typeset.io/pdf/hypersonic-boost-glide-weapons-4xzolkpx67.pdf)).
- **A energia do impacto em si** [E], E = ½mv²: 500 kg a 1,5 km/s = 0,56 GJ ≈ **134 kg de TNT**; a 3 km/s ≈ **540 kg de TNT**.

**"Rods from God" / Project Thor:**
- O relatório da USAF de 2003 fala de uma vara de tungstênio de **6,1 × 0,3 m a Mach 10 ≈ 11,5 t de TNT** ([Kinetic bombardment](https://en.wikipedia.org/wiki/Kinetic_bombardment), [GlobalSecurity](https://www.globalsecurity.org/space/systems/rods-from-god.htm)).
- **[NC]:** nunca foi construído, e a velocidade real no chão seria menor por causa do arrasto.
- **No visual:** não tem bola de fogo química. É um **jato de terra** do buraco e um anel de poeira; no clarão do impacto, o metal vaporiza [E].

**B-2 com GBU-57 MOP:**
- **Peso:** 12,3–13,6 t, com **~2,4–2,7 t de explosivo** ([NPR](https://www.npr.org/2025/06/25/nx-s1-5444307/did-america-bunker-busting-bombs-fail-reach-iran-nuclear-target), [Forecast Intl](https://dsm.forecastinternational.com/2025/06/23/with-gbu-57s-u-s-finally-deploys-weapon-designed-specifically-for-irans-hardened-nuclear-sites/)).
- **Penetração [NC]:** "até 60 m" é a alegação; pela equação, a NPR chega a **7,9 m em rocha média**.
- **Fordow (junho de 2025):** **12 MOPs** de 6 B-2 (Forecast; a Wikipedia diz 7 B-2).
- **No visual:** quase nada na superfície. Um furo, o chão que estufa, poeira saindo pelas aberturas.

**GBU-43 MOAB** (ver §0):
- **Peso:** 9.850 kg, dos quais **8.480 kg de H6 ≈ 11 t de TNT** ([Business Insider](https://www.businessinsider.in/Heres-why-the-mother-of-all-bombs-exploded-6-feet-above-its-target-in-Afghanistan/articleshow/58173415.cms)).
- Explode **~2 m acima do chão**, de propósito, para não gastar energia em cratera.
- **O "1 milha de raio" da imprensa é mito.** O medido: destruição ~150 m, bola de fogo até ~140 m (Alcis).
- **Primeiro e único uso em combate:** 13/04/2017, Achin, Afeganistão.

### 1.2 China

**DF-17 com o planador DF-ZF** ([CSIS](https://missilethreat.csis.org/missile/df-17)):
- **Alcance:** 1.800–2.500 km (a parte planada é bem menor, segundo a [GlobalSecurity](https://www.globalsecurity.org/wmd/world/china/df-17.htm)).
- **Velocidade de planeio:** **Mach 5–10**.
- **Tamanho:** 11 m, 15 t, num caminhão lançador. Apareceu no desfile de 01/10/2019.
- **Ogiva:** não pública [NC].

**DF-21D e DF-26** (os "carrier killer" e "Guam express"):
- Ogivas manobráveis na reentrada (MaRV).
- **Alcance:** DF-21D ~1.500–2.150 km; DF-26 ~3.000–4.000 km. As fontes divergem [NC] ([Defense News](https://defensenews.com/global/asia-pacific/2016/05/12/china-puts-guam-within-missile-range), [National Interest](https://nationalinterest.org/node/207372)).

**Jiu Tian (SS-UAV, "Nove Céus"), o drone-mãe:**
- **Primeiro voo:** **11/12/2025** em Pucheng ([Aviation Week](https://aviationweek.com/defense/aircraft-propulsion/china-flies-nine-heavens-uas-mothership), [TWZ](https://www.twz.com/air/chinas-high-flying-swarm-mothership-drone-has-flown)).
- **Capacidade:** solta **~100 drones pequenos ou munições de espera**.
- **Carga e peso:** "até 6 t de carga" e "7.000 km" vêm da cobertura de antes do voo [NC]; peso máximo ~16 t.

**PHL-16 (PCL/PCH-191)** ([Wikipedia](https://en.wikipedia.org/wiki/PHL-16), [GlobalSecurity](https://www.globalsecurity.org/military/world/china/ar-3.htm), [Janes](https://www.janes.com/defence-intelligence-insights/defence-news/weapons/china-fires-airburst-munitions-from-phl-16-missile-launcher)):
- **Cada célula leva:** 5 foguetes de 300 mm, ou 4 de 370 mm (~280–350 km), ou 1 Fire Dragon 480 de 750 mm (são 2 por veículo).
- **Fire Dragon 480:** 300–500 km [NC]; **ogiva de 480 kg** segundo uma fonte só [NC].

**H-6K / H-20 e a "MOAB chinesa":**
- **O vídeo:** a NORINCO mostrou em dez/2018–2019 um H-6K soltando uma bomba de **~5–6 m** [NC]. Um representante disse que **não é termobárica** ([TWZ](https://www.twz.com/our-first-full-look-at-chinas-own-mother-of-all-bombs), [Newsweek](https://newsweek.com/china-mother-all-bombs-weapons-1278985)).
- **O H-20:** não achei apresentação pública dele nas fontes [NC].

**Micro-ondas, laser e antissatélite:**
- **Hurricane 3000:** micro-ondas num caminhão 8×8 contra enxames de drones, **"além de 3 km"**, alegação da NORINCO [NC] ([The Star/SCMP](https://www.thestar.com.my/aseanplus/aseanplus-news/2026/01/09/chinas-powerful-new-microwave-weapon-system-can-destroy-drone-swarms-within-3km)).
- **LY-1:** o laser naval apareceu no desfile de setembro de 2025, sem potência divulgada ([Maritime Executive](https://www.maritime-executive.com/article/china-unveils-large-shipboard-laser-system-for-maritime-attack)).
- **SC-19:** em jan/2007 destruiu o satélite **Fengyun-1C a ~865 km**; deixou **>3.000 fragmentos catalogados** ([Wikipedia](https://wikipedia.com/wiki/Fengyun-1C)).
- **No visual:** micro-ondas e laser são **invisíveis** (o laser só aparece na fumaça ou poeira). O ASAT acontece fora do mapa. Nenhum dos três é bom como superarma de chão.

### 1.3 GLA

**SCUD-B (R-17) no MAZ-543** ([Scud](https://en.wikipedia.org/wiki/Scud), [missilery](https://en.missilery.info/node/12755)):
- **Alcance:** ~300 km (270 km com ogiva cheia).
- **Carga útil:** 985 kg; uma fonte dá 770 kg de explosivo [NC].
- **Precisão (CEP):** 450 m segundo a URSS, ~900 m segundo o Ocidente.
- **Propelente:** querosene + ácido nítrico fumegante vermelho (IRFNA).
- **Ogiva:** podia ser convencional, química ou nuclear.
- **Lançamento:** levantar o míssil **leva ~4 min**; a sequência inteira ~1 h; o Iraque reduziu de 90 para ~30 min.
- **Voo:** ~5 min para 300 km [E].

**Al-Hussein (a versão iraquiana)** ([Wikipedia](https://en.wikipedia.org/wiki/Al-Husayn_(missile))):
- **650 km**, ogiva reduzida a **500 kg**, **CEP ~1.000 m**, apogeu ~150 km, ~8 min de voo.
- **A quebra:** **o corpo e a ogiva tendiam a se partir na reentrada**. Isso funcionou como chamariz contra o Patriot.
  - Postol: o rastro de destroços é o míssil se desfazendo ([depoimento de 1992](https://www.globalsecurity.org/space/library/congress/1992_h/h920407z.htm)).
  - O "saca-rolha/espiral" circula em relatos, mas não achei confirmação [NC].
- **1991:**
  - **88 lançados: 46 na Arábia Saudita, 42 em Israel**;
  - Dhahran, 25/02: **28 mortos**, 110 feridos;
  - o Patriot perdeu o rastreio ali por um desvio do relógio do radar depois de >100 h ligado.

**Patriot × SCUD:**
- A taxa de acerto anunciada foi rebaixada depois.
- Lewis e Postol: os vídeos **não mostram nenhuma ogiva claramente destruída** ([S&GS 1993](https://scienceandglobalsecurity.org/archive/1993/08/video_evidence_on_the_effectiv.bib)).
- O GAO: os vídeos não servem para contar acertos ([GAO NSIAD-93-22R](https://gao.gov/assets/nsiad-93-22r.pdf)).

**A caça aos SCUDs:**
- **"Atira e foge":** posição pré-levantada, monta, atira e some.
- **O ciclo:** de ~10 min a ~30 min, conforme a fonte [NC] ([National Interest](https://nationalinterest.org/node/131647), [SOF News](https://sof.news/history/desert-storm-sof-scud-hunting-mission-in-iraq/)).
- **O resultado:** os aviões disseram ter destruído ~80 lançadores e as forças especiais mais ~20. O Pentágono **não confirmou nenhum**; muitos eram chamarizes ([PBS Frontline](https://www.pbs.org/wgbh/pages/frontline/gulf/weapons/scud.html)).

**As ogivas químicas:**
- A contagem mais citada: **30 ogivas de sarin** destruídas sob a UNSCOM.
- O Iraque depois declarou até **50 químicas** (mistura sarin/ciclosarin) e **25 biológicas**. Não houve uso em 1991 ([UNSCOM 1999](https://www.globalsecurity.org/wmd/library/news/iraq/un/990125/dis-chem.htm), [Wisconsin Project](https://www.wisconsinproject.org/iraqs-chemical-weapons-program-profile/)).

### 1.4 Rússia

**Iskander-M (9K720)** ([Wikipedia](https://en.wikipedia.org/wiki/9K720_Iskander)):
- **Lançador:** caminhão com 2 mísseis.
- **Trajetória:** quase-balística e baixa, ~50 km de altura [NC, sem citação]; Mach 6–7 [NC]. Mergulho final íngreme; algumas versões levam chamarizes.
- **Ogiva:** 480–700 kg (as fontes divergem).
- **Nuclear:** opção estimada em **5–50 kt** [NC] ([globalmilitary](https://www.globalmilitary.net/missiles/ss-26-stone-iskander/)).

**Yars (RS-24) / Topol-M:**
- **Lançador:** MZKT-79221, **16×16**, ou silo ([Wikipedia](https://en.wikipedia.org/wiki/MZKT-79221)).
- **O abrigo Krona:** a "garagem" com **teto que corre**; o ministério já mostrou lançamentos de dentro dela ([russianforces.org](https://russianforces.org/blog/2017/09/training_launch_of_yars_icbm_f.shtml)).
- **Pronto em ~7 min** [NC].
- **Lançamento a frio:**
  - um gerador de gás expulsa o míssil do tubo e o motor acende no ar;
  - está documentado no RT-2PM Topol ([historyrise](https://historyrise.com/article/the-history-and-significance-of-the-soviet-rt-2pm-topol-icbm/));
  - no Topol-M e no Yars a fonte é fraca [NC].

**RS-28 Sarmat:**
- **208,1 t**, 35,3 × 3 m, em silo.
- Um teste **falhou em set/2024** ([Wikipedia](https://www.wikipedia.org/wiki/RS-28_Sarmat)).
- Levaria planadores Avangard: "Mach 20–25" é alegação [NC].

**A alternativa termobárica:**
- **ATBIP, o "Pai de todas as bombas":**
  - 7,1 t [NC, sem citação], **44 t de TNT** [NC], raio de 300 m [NC], teste de 2007;
  - analistas desconfiam do número e até do teste ([Wikipedia](https://en.wikipedia.org/wiki/Father_of_All_Bombs), [Military.com/Wired](https://www.military.com/defensetech/2007/09/12/the-father-of-all-bombs));
  - **pela conta, 44 t dariam 5 psi a ~200 m** [E].
- **TOS-1A Solntsepyok:**
  - **24 tubos de 220 mm**, ~6 km de alcance, salva em **6–12 s**;
  - cobre **200 × 400 m** ([Wikipedia](https://en.wikipedia.org/wiki/Thermobaric_weapon)), ou 40.000 m² na alegação russa [NC] ([GlobalSecurity](https://www.globalsecurity.org/military/world/russia/tos-1a.htm)).
  - Já é a artilharia pesada da Rússia no `docs/FACCAO_RUSSIA.md`.

---

## 2. A linha do tempo física

### 2a. Nuclear de baixa potência (0,01–10 kt), no ar e no chão

**As fórmulas** (Glasstone & Dolan 1977, [cap. II](https://www.fourmilab.ch/etexts/www/effects/eonw_2.pdf) e [cap. VII](https://www.fourmilab.ch/etexts/www/effects/eonw_7.pdf)):
- **O clarão duplo:** o 1º mínimo de brilho é aos **11 ms para 20 kt** e escala com ~W^0,4 (§2.125–2.126). O 2º máximo, onde sai quase todo o calor, é **t_max = 0,0417·W^0,44 s** (§7.85).
- **O raio da bola de fogo:**
  - no mínimo térmico: 90·W^0,4 pés (27 m·W^0,4);
  - na "separação": 110·W^0,4 pés (33,5 m) no ar e **145·W^0,4 pés (44 m) encostada no chão** (§2.127);
  - **o máximo é ~2× o da separação**.
  - Conferido em 20 kt: diâmetro máximo de ~1.550 pés ([Glasstone 1962 transcrito](https://www.abomb1.org/nukeffct/airburst.html)).
- **Temperatura da superfície da bola:** mínimo ~3.000 °C, máximo **~7.700 °C** (§2.125).
- **A cor da nuvem:** **primeiro vermelha ou marrom-avermelhada** (óxidos de nitrogênio), **depois branca** (água condensada) (§2.08). No deserto entra o marrom do pó.
- **Os ventos de volta (afterwinds)** sugam terra para dentro e para cima (§2.09); passam de "200 mph" nas grandes.
- **A nuvem de Wilson:**
  - **se forma 1–2 s depois e some em ~1 s** (Able, 23 kt), só em **ar úmido**;
  - nasce **à frente** da bola, onde a pressão já caiu;
  - vira domo, depois anel, e some (§2.48–2.50).
- **A subida e o tamanho final:**
  - **a nuvem para (estabiliza) em ~10 min** (§2.15);
  - 10 kt: topo **19.000 pés (5,8 km)**, base 3 km, largura 3 km;
  - **abaixo de ~20 kt, o caule tem metade do raio da nuvem** (§2.17);
  - em 20 kt, aos 30 s a nuvem já subiu 1,5 milha ([Glasstone 1962](https://www.abomb1.org/nukeffct/airburst.html)).
- **Little Feller I (18 t, a 3 pés do chão):** **topo da nuvem a 11.000 pés** ([DOE/NV-1200](https://nnss.gov/wp-content/uploads/2023/04/DOENV_1200.pdf)). Provavelmente acima do nível do mar: a Área 18 fica a ~1,5 km [NC], então **~1,8–2 km acima do chão** [E].
- **A cratera** (encostada no chão, solo seco):
  - 1 kt dá **raio aparente ~60 pés (18 m), profundidade ~30 pés (9 m)**, crista da borda +15 pés;
  - **escala com W^0,3**;
  - o material jogado em volta vai até **2,15× o raio** ([cap. VI §6.09, §6.74](https://www.fourmilab.ch/etexts/www/effects/eonw_6.pdf));
  - conferido no teste Jangle Surface (1,2 kt a 3,5 pés): profundidade real de 17–19 pés ([OSTI](https://www.osti.gov/biblio/4035254)).
- **A fase positiva do choque:** em 20 kt dura ~1–1,4 s a 1–2 milhas (§3.11). Escala com W^(1/3).
- **A radiação nas potências baixas é maior que o blast:**
  - na Davy Crockett, **100% de baixas num raio de >160 m** pelos nêutrons ([Wikipedia](https://en.wikipedia.org/wiki/Davy_Crockett_(nuclear_device))), contra 5 psi a ~120 m;
  - a chuva radioativa local só aparece se a bola encosta no chão ([Glasstone 1962](https://www.abomb1.org/nukeffct/airburst.html)).

**A tabela** [E, montada com as fórmulas acima]:

| W | 2º máx. térmico | Bola máx. (raio), no ar / no chão | 20 psi | 5 psi | 2 psi | Cratera (encostada) R × prof. | Topo da nuvem [E] |
|---|---|---|---|---|---|---|---|
| 0,01 kt | 5,5 ms | 11 / 14 m | 47 m | 98 m | 181 m | 4,6 × 2,3 m | ~1,7 km |
| **0,02 kt** | **7,5 ms** | **14 / 18 m** | **60 m** | **123 m** | **228 m** | **5,7 × 2,8 m** | **~1,8–2 km** (Little Feller) |
| 0,1 kt | 15 ms | 27 / 35 m | 102 m | 210 m | 390 m | 9,2 × 4,6 m | ~2,6 km |
| 1 kt | 42 ms | 67 / 88 m | 219 m | 453 m | 840 m | 18 × 9 m | ~3,9 km |
| 10 kt | 115 ms | 168 / 222 m | 472 m | 977 m | 1.809 m | 36 × 18 m | 5,8 km (Glasstone) |

- **O topo da nuvem** é uma interpolação W^0,18 entre o Little Feller I e os 10 kt do Glasstone [E].
- **No ar**, sem cratera e sem chuva radioativa local; o 5 psi fica ~1,4× maior.

**A frente de choque nas pequenas** (nuclear no chão) [E]:

| Raio | 20 m | 40 m | 60 m | 100 m | 150 m | 250 m | 400 m |
|---|---|---|---|---|---|---|---|
| **0,02 kt** | 7 ms | 30 ms | 65 ms | 157 ms | 286 ms | 561 ms | 986 ms |
| 0,1 kt | 3 ms | 17 ms | 39 ms | 107 ms | 217 ms | 472 ms | 882 ms |
| 1 kt | 1 ms | 5 ms | 16 ms | 50 ms | 114 ms | 301 ms | 656 ms |

**A sequência para 20 t na superfície** (a candidata da Rússia, ver §3.4):

| t | O que acontece |
|---|---|
| 0–1 ms | **Clarão branco**. Mínimo térmico ~0,7 ms [E]: o "duplo pulso" fica **dentro do 1º quadro a 60 qps** e não dá pra ver (só se vê a partir de ~5–10 kt, quando t_max ≥ 6 quadros) |
| 1–10 ms | A bola cresce até ~9 m (separação) [E]. Pico de calor aos 7,5 ms |
| 10–100 ms | Bola de **~18 m de raio** (hemisfério), branco-amarelada. O choque se solta: 60 m aos 65 ms |
| 0,1–0,3 s | O choque varre a base (100 m aos 157 ms, 150 m aos 286 ms). A areia sobe **no lugar** por onde ele passa. Domo de Wilson breve (0,1–0,2 s, só com ar úmido) [E] |
| 0,1–0,15 s | Fim da fase positiva perto do anel de 5 psi [E]. Vem a **fase negativa**: vento de volta para o centro, mais fraco e mais longo (0,3–0,5 s) [E] |
| 0,2–1 s | A bola perde o brilho (laranja → vermelho-escuro) e vira **nuvem marrom-avermelhada** (NOx), já subindo e virando toroide [E] |
| 1–10 s | **Os ventos de volta** sugam a poeira do anel para o centro e formam **o caule de terra** (raio ≈ ½ do da nuvem). A cabeça sobe a ~10–20 m/s [E] |
| 10 s – 3 min | A cabeça embranquece se o ar for úmido; no deserto fica marrom-acinzentada. Achata e para a ~1,8–2 km [E]. O caule se desfaz de baixo para cima |
| depois | **Cratera** de ~11 m de diâmetro e 3 m de fundo, borda baixa até ~15 m, material jogado até ~25 m. **Chão chamuscado** até ~60–90 m pela radiação térmica [E] (35% de 0,02 kt ≈ 29 GJ). **Zona de radiação** de ~150–400 m. Chuva radioativa a favor do vento |

### 2b. Termobárica (só a parte visível)

- **Dois estágios** ([Thermobaric weapon](https://en.wikipedia.org/wiki/Thermobaric_weapon)):
  1. **Uma nuvem de aerossol** se espalha e entra em tudo. Exemplo citado: **~18 m de diâmetro × 2,4 m de espessura** (CBU-72). De cima, aparece como uma "panqueca" cinza-clara por um instante [E: ~0,1–0,2 s].
  2. **A nuvem inteira acende** numa bola de fogo **muito maior que a de um explosivo comum do mesmo peso**. A KAB-1500S alega bola de 150 m de raio [NC].
- **A pressão é mais longa**, com uma **fase negativa forte**: o "vácuo" que dá o nome.
  - Em lugar fechado, os reflexos sustentam a bola por **10–50 ms** a mais.
  - Um estudo de 2024 mediu **+39% de pico e +51% de impulso** contra o TNT ([DOAJ](https://doaj.org/article/9d71eedf6c014acb8e569582b2c9015e)).
- **O que fica:** fumaça grossa e escura (o combustível que sobra) e o chão chamuscado sem cratera. A bola em campo aberto dura ~0,5–2 s [E].

### 2c. Reentrada a Mach 5+ (ogiva convencional)

- **O risco de luz:**
  - acende a ~20–30 km ([38 North](https://www.38north.org/2017/07/melleman073117/));
  - a 6 km/s e 30 km, um nariz de 50 cm irradia **~125 kW de luz visível** ([IDA](https://www.ida.org/~/media/Corporate/Files/Publications/IDA_Documents/STD/D-4792.pdf));
  - nas fotos, as ogivas americanas chegam **ainda brilhando** ao mar de Kwajalein (fórum citando fotos do Mk-12 [NC], [up-ship](https://up-ship.com/blog/?p=5379)).
- **O estrondo sônico e a "ordem invertida"** [E, física]:
  - O objeto anda muito mais rápido que o som. Quem está perto do alvo ouve **primeiro o impacto** e só **depois** o rugido do voo.
  - O som que ele emitiu a uma altura h chega ~h/343 s depois do impacto. Os últimos metros chegam antes; o ronco do alto chega por último.
  - O efeito é um **rasgo que "sobe"**: 10 km de trajetória audível dão ~30 s de cauda.
  - Os relatos de Kiev ("explosões antes do alarme") combinam com isso ([Ukrainska Pravda](https://www.pravda.com.ua/eng/news/2026/07/11/8043534/)). Não achei relato descrevendo o som invertido em si [NC].
- **O impacto:** a energia cinética é pequena perto do explosivo (§1.1). **Ogiva cinética** faz um furo fundo com **jato de terra vertical** e anel de poeira, sem bola de fogo. **Ogiva com explosivo** faz cratera e bola normais, só que **sem aviso sonoro antes**.

### 2d. Chuva de submunições

- **A abertura:** o casco se abre e os pacotes saem em pares (TLAM-D) ou de uma vez (ATACMS). De perto, um estalo seco e a "nuvem" de pontos escuros se abrindo em cone [E].
- **A queda:** com desacelerador, as bombinhas descem quase na vertical por alguns segundos.
- **O chão:** um **tapete de clarões pequenos**: cada um ~0,05–0,1 s, bola de 1–2 m, sopro de poeira de 3–5 m [E]. Ondula de um lado para o outro em **~1–3 s** [E]. A BLU-97 solta **faíscas brancas de zircônio**.
- **O som:** o jargão americano chama esses projéteis de **"firecracker" ou "popcorn"** pelos muitos estouros pequenos ([Cluster munition](https://cavac.at/cavacopedia/Cluster_munition)). É um **crepitar denso**, sem um estrondo único.
- **O depois:** **5–8% não explodem** e ficam no chão ([CBU-87](https://en.wikipedia.org/wiki/CBU-87_Combined_Effects_Munition)).

### 2e. SCUD com agente químico: a cor real e a cor dos jogos

**A cor real:**
- **Sarin:** **incolor** e sem cheiro.
- **Mostarda:** pura é incolor; a de guerra é **amarelo-marrom**, cheiro de alho ou raiz-forte, fica no chão.
- **Cloro:** **verde-amarelado**, visível.
- **Fosgênio:** branco a amarelo-pálido ([CompoundChem](https://www.compoundchem.com/wp-content/uploads/2023/12/Chemical-Warfare-WW1-2023.pdf), [WebMD](https://www.webmd.com/mental-health/news/20011012/americas-resilience)).
- **Halabja (1988, mostarda + agentes de nervos):** os sobreviventes descrevem **nuvens branco-amareladas que desciam para o chão**, com cheiro de **maçã podre ou alho** ([Springer](https://link.springer.com/chapter/10.1007/978-3-319-51664-6_18), [Halabja](https://en.wikipedia.org/wiki/Halabja_massacre)).

**O detalhe real que nos serve: o laranja do próprio SCUD.**
- Sobra **ácido nítrico** no míssil quando ele cai ou é interceptado.
- Ele solta uma **névoa amarelo-amarronzada ou laranja** de óxidos de nitrogênio (o N₂O₃ é marrom-avermelhado).
- Em 1991, isso foi confundido com ataque químico ([Gulflink/DIA, resumo da busca](https://gulflink.health.mil/irfna/irfna_refs/n28en112/950825_0151pgv_91d.html); a página principal do [Gulflink](https://gulflink.health.mil/story/scud_irfnajsp.shtml) recusou o acesso, então trato como [NC]).
- A chama do lançamento de um míssil desse tipo é **laranja** ([UCS](https://blog.ucs.org/david-wright/debris-from-north-koreas-launcher-what-it-shows/)).

**A cor dos jogos** (o `ParticleSystem.ini` do ZH, em `captures/fxlab/tread_study/aaa_research/zh_src/`):
- toxina **verde puro aditivo** (ToxinPuddle R4 G255 B4);
- Anthrax Beta **azul** (43, 51, 155);
- Anthrax Gamma **roxo** (128, 55, 217);
- todos com **shader ADDITIVE**: gás que **brilha**, fisicamente errado.

**O comportamento físico** [E, gás pesado]: a nuvem é mais densa que o ar. Abraça o chão em 1–3 m, **escorre para o baixo** (valas, trincheiras, crateras), anda com o vento a 1–3 m/s e se dilui em dezenas de segundos ao ar livre.

---

## 3. Escala do nosso jogo: números propostos por arma

**Regras para todas:**
- os anéis seguem a física (**1 : 2 : 3,8** para 20 : 5 : 2 psi);
- **a onda de choque anda na velocidade real** (não desacelerar; quem conta é o rastro de areia);
- as alturas são comprimidas por λ e os tempos por √λ (semelhança de Froude, como em `EXPLOSOES.md`);
- **tudo acima de ~3 m sai do nível das unidades em 3–5 s**.

### 3.1 USA: o míssil que estoura alto e solta as ogivas

**A física que limita a ideia:**
- A liberação real das ogivas acontece **no espaço e sem clarão**.
- **Da câmera de RTS o céu não aparece:** o alto da imagem ainda olha para o chão. Um estouro a 300 m de altura sobre o alvo fica **acima e atrás da câmera**.
- Então o "estouro alto" só se lê por **luz e sombra**:
  - o chão inteiro se acende;
  - as sombras dos prédios ficam curtas e caem **para fora do ponto de luz** e se mexem enquanto o clarão desce [E];
  - os **riscos entram pelo alto da tela**.

**A proposta A, "Prompt Strike" de 6 ogivas** (a mais próxima da ideia; recomendada):

| Item | Número | Por quê |
|---|---|---|
| Ogivas | **6** (anel de 5 + 1 no centro, ou anel de 6 com raio de 50–60 m) | cobre ~180–200 m = uma base |
| Carga de cada uma | **150–250 kg de TNT** (explosivo + impacto) | 5 psi = **30–36 m**, 20 psi = 15–17 m: um lote de prédio por ogiva |
| Impacto | real seria 1,5–3 km/s; **visual de 300–600 m/s nos últimos 150 m**, com um **risco que persiste** 0,5–1 s (a foto de exposição longa de Kwajalein) | a velocidade real cruza a tela em 1–3 quadros |
| Escalonamento | uma a cada 0,15–0,4 s | lê como "rajada", não como uma explosão só |
| Cratera | 6–12 m [E] | Mk-84 enterrada tem ~15 m; isto é menor |
| "Estouro alto" | clarão a 600–1.000 m virtuais: luz forte por 3–6 quadros, sombras andando; **trovão rolando 2–3 s depois** (d/343) | o "som tenebroso" vem do alto, depois do clarão, sobreposto às ogivas chegando |

**A proposta B, a chuva de submunições** (TLAM-D / ATACMS):
- **600–950 bombinhas** num círculo de **180–210 m** (o ATACMS real: 950 em 205 m).
- Cada uma é um clarão de 1–2 m e um sopro de 3–5 m, num tapete de 1,5–3 s.
- Bonita e fiel, mas **não "limpa" prédios** (o M74 mata gente, não derruba concreto). Exigiria uma carga central extra.

**A proposta C, "Thor"** (uma vara só):
- Pela alegação de 2003 (9 t a ~3 km/s ≈ 9,7–11,5 t de TNT) daria **5 psi a ~120 m** [E/NC], o mesmo anel da nuke.
- O visual é o mais diferente do jogo: **jato de terra** e anel de poeira, sem bola de fogo.

### 3.2 China (em aberto: as candidatas com número)

| Candidata | Proposta para o jogo | Leitura |
|---|---|---|
| **Jiu Tian, enxame** | **60–100** munições de 5–15 kg (5 psi = 10–14 m cada) em espiral, **15–30 s** de ataque; a **sombra do drone-mãe** passa pela base antes; dá pra derrubar com antiaéreo | nova, lê muito bem de cima, tem resposta; **é a única "moderna de 2025"** |
| **H-6K + bomba grande** ("MOAB chinesa", termobárica no visual) | **8–11 t de TNT**: 5 psi = 115–127 m, 2 psi = 215–235 m; bola de fogo de 40–70 m de raio [E] | bate com a identidade da China no `CONCEITO_EXPLOSOES` (fogo, força bruta), **mas fica parecida com a nuke russa** |
| **PHL-16, salva** | 8 foguetes de ~150 kg (5 psi = 30 m) ou 2 FD-480 de 480 kg (5 psi = 45 m) em 3–6 s | é artilharia, não superarma; repete a barragem |
| **DF-17** | 1 ogiva de ~500 kg [NC] chegando muda, a Mach 5+: 5 psi = 45 m | precisa demais para "limpar a área"; serve para "decapitar" 1 prédio |
| Micro-ondas, laser, ASAT | — | invisíveis ou fora do mapa: **descartar** como superarma |

**Minha leitura:**
- Para as facções ficarem distintas, a **termobárica fica com a Rússia** (já tem o TOS-1A) e a **China fica com o enxame**, ou com o DF-17 + enxame.
- A decisão é sua.

### 3.3 GLA: a tempestade de SCUDs

| Item | Proposta | Base real |
|---|---|---|
| Mísseis | **6–9** (o ZH usa 9) | — |
| Carga de cada um | **100–250 kg de TNT** (5 psi = 26–36 m) | o SCUD-B real (~800 kg) daria 53 m; reduzir |
| Imprecisão | dispersão **σ 25–40 m** em volta do alvo; trajetória com **bamboleio**; **1 em 9 se parte** na descida (cai em pedaços, blast menor); **1 falha** às vezes | CEP real de 450–1.000 m, comprimido; a quebra do al-Hussein |
| Química | cada impacto: **nuvem pesada de 20–30 m de raio** que **escorre** até **40–60 m** em 10–20 s; **1–2 m de espessura**; anda com o vento a 1–3 m/s; dura **30–60 s**; mancha no chão por 60–120 s | Halabja: "nuvens que desciam pro chão" |
| Cor | **amarelo-esverdeado pálido / ocre**, com **sombra e luz do mundo** (nunca aditivo); a **névoa laranja-marrom de NOx** em cada impacto é a assinatura "suja" | cloro e mostarda; o ácido nítrico do SCUD |
| Lançamento | MAZ-543 levanta o míssil em ~4 min reais; no jogo, **8–12 s** com o míssil subindo devagar e a **nuvem marrom-alaranjada** na ignição | — |
| Contra | Patriot pode interceptar alguns, mas **os pedaços caem** e a ogiva às vezes passa | 1991 |

### 3.4 Rússia: a ogiva nuclear

**A proposta: ~20 t (0,02 kt), porte W54/Little Feller**, explodindo encostada ou muito baixa (≤ 1–2 m):

| Item | Número |
|---|---|
| Anel total (20 psi) | **60 m** |
| Anel forte (5 psi) | **123 m** (o pedido era 60–120 m; com **10 t**, 98 m) |
| Anel leve (2 psi) | **228 m**, quase a base inteira |
| Bola de fogo | **raio de 18 m** (hemisfério), branco → amarelo em 0,1 s; **brilho útil de 0,5–1 s** (real); no jogo pode durar 1,5–2 s para ler [licença] |
| Choque | 60 m aos 65 ms, 123 m aos ~220 ms, 228 m aos ~500 ms: **varre a base em 1/3 de segundo** |
| Cratera | **~11 m de diâmetro, 3 m de fundo**, borda até ~15 m, material jogado até ~25 m. Para a cratera "que molda o mapa" (`VISAO_DO_JOGO`), 0,1 kt dá 18 m × 4,6 m, mas o anel forte vira 210 m (passa da base) |
| Nuvem | real ~1,8–2 km em minutos. **λ ≈ 0,12–0,15**: a cabeça para a **200–300 m** (acima da câmera, sai do quadro); tempos × ~0,37: **caule em 3–5 s, cabeça passa da câmera aos 8–12 s, estabiliza aos 30–60 s**, coluna residual por 2 min |
| Cor | a bola branca → laranja; **a nuvem marrom-avermelhada (NOx)**; o caule e o anel de **poeira da cor do chão**; ao entardecer, a cabeça pega o sol |
| Radiação | campo que fica, de **150–200 m** (mais largo que o blast, como na Davy Crockett), por 60–120 s |
| Vento de volta | a poeira do anel é **puxada para o centro** a 10–30 m/s [E] por 2–10 s e alimenta o caule |

**O que torna a nossa diferente do ZH:**
- o ZH faz ~25 s com fogo **verde-esbranquiçado aditivo** e poeira quase branca (`EXPLOSOES.md`);
- **a nossa ganha volume de verdade, sombra e a nuvem castanha do NOx**.

**A alternativa termobárica (ATBIP):**
- pela alegação de 44 t, **5 psi a ~200 m** (a base inteira e mais);
- reduzida a 10–15 t ela vira "uma MOAB", **e aí repete a candidata chinesa**.

---

## 4. Referências de efeito visual: o que cada uma fez de marcante

**Filmes:**
- **Oppenheimer (2023), o Trinity sem CGI:**
  - Scott Fisher (efeitos práticos) e Andrew Jackson (efeitos visuais) fizeram **miniaturas grandes** filmadas como se fossem enormes, e um **"rolo de demonstração de explosões"** do qual o Nolan escolheu os quadros;
  - Nolan: "É difícil fazer o CG ser ameaçador" ([TheWrap](https://www.thewrap.com/christopher-nolan-oppenheimer-practical-effects-nuclear-explosion/), [Task & Purpose](https://taskandpurpose.com/culture/christopher-nolan-oppenheimer-cgi/));
  - **a crítica:** não mostra a escala, é uma colagem ([Looper](https://www.looper.com/1348747/oppenheimer-practical-bomb-david-lynch-trinity-test-embraced-cgi-twin-peaks/));
  - **a lição:** textura real (turbulência, fuligem) vence a geometria limpa.
  - Os materiais (gasolina, propano, magnésio) circulam, mas não achei em fonte [NC].
- **Twin Peaks: The Return, parte 8 (2017):** o Trinity em **11 min de CG (BUF)**, com um empurrão lento para dentro da bola. A lição: dilatar o tempo **depois** do clarão, não antes.
- **Terminator 2 (1991):**
  - a 4-Ward (os irmãos Skotak) fez **Los Angeles em miniatura** destruída com **canhões de ar**, um cogumelo de acrílico e fibra ajustável, e pinturas de fundo ([Cartoon Brew](https://cartoonbrew.com/classic/making-nuclear-apocalypse-iconic-sequence-terminator-2-created-141209.html));
  - **a lição:** a onda é vista **pelo que ela arranca**, em sequência e de longe para perto.
- **Threads (1984):** ~£250–400 mil, efeitos de Peter Wragg, ritmo de documentário, o PEM primeiro ([Threads](https://wikipedia.com/wiki/Threads_(1984_film))). **A lição:** contenção e tom sério (o tom do nosso jogo, `VISAO_DO_JOGO`).
- **The Day After (1983):** não consegui fonte sobre como fizeram os efeitos [NC].

**Jogos:**
- **Call of Duty 4, "Shock and Awe":** depois da nuke, o jogador rasteja, com câmera lenta, cor lavada e som abafado. Não achei a análise de produção [NC]. **A lição:** o "depois" vale mais que o estouro.
- **Battlefield 3, "Comrades":** a nuke em Paris vista de longe pelo Dima ([resumo](https://Www.Wikipedia.org/wiki/Battlefield_3)).
- **Fallout 4, o "mushroom case"** ([Simon Schreibt](http://simonschreibt.de/gat/fallout-4-the-mushroom-case/)):
  - **planos de partícula** com um flipbook cinza **que repete**;
  - uma textura que **corre em direção ao centro** (o rolar);
  - uma **rampa de cor pela idade** (de fogo a preto) e o alfa remapeado no tempo.
  - **A lição:** a cor muda pela idade de cada pedaço, não pelo tempo global.
- **World in Conflict (2007), a nuke tática num RTS:**
  - a nuvem é um **objeto 3D**, ela **abre as nuvens do céu**, e **é o choque que comunica a força**;
  - os críticos disseram que as **crateras ficaram rasas** ([Tom's Hardware](https://www.tomshardware.com/picturestory/444-world-in-conflict.html), [PC Gamer](https://www.pcgamer.com/pc-gamings-most-noteworthy-nukes)).
  - **A lição (a mais útil para nós):** de cima, **choque + reação do mundo + marca funda**.
- **C&C Generals / ZH** (nosso `ParticleSystem.ini`):
  - a nuke é ~12 sistemas;
  - **o novo para nós:** o **NukeShockwaveInverted**, poeira que anda **para dentro** (o vento de volta), e o **NukeRadiationInitial**, a "poça" laranja de raio 200 unidades (~48 m) por 13 s;
  - os relógios das superarmas são **visíveis para todos** (nuke 6:00, canhão de partículas 4:00 com feixe de 10 s) ([C&C Wiki](https://cnc.fandom.com/wiki/Nuclear_missile_(Generals_1)), [CNCNZ](https://cncnz.com/games/zero-hour/leangs-structures/), [TV Tropes](https://www.tvtropes.org/pmwiki/pmwiki.php/YMMV/CommandAndConquerGenerals));
  - a tempestade de SCUDs dispara **9 mísseis** ([transcrição](https://cnc.fandom.com/wiki/Transcript:Final_Justice)).
- **C&C3 (Tiberium Wars), o Ion Cannon:**
  - **8 feixes secundários giram** em volta da área, convergem, uma pausa curta, e o feixe principal ([C&C Wiki](https://breezewiki.discard.no/cnc/wiki/Ion_cannon));
  - **a lição:** **desenhar a área antes do golpe** dá tempo de reagir e mostra o raio.
- **Supreme Commander / BAR:** a nuke é feita de emissores presos a projéteis invisíveis (`EXPLOSOES.md`). Existe o antimíssil como resposta, e a cratera e o queimado ficam para sempre.

**Filmagem real para conferir quadro a quadro:**
- **Os filmes desclassificados da LLNL** (Greg Spriggs, 2017): milhares de quadros por segundo; a velocidade de cada câmera teve que ser recalculada para medir o crescimento da bola ([LLNL](https://www.llnl.gov/news/physicist-declassifies-rescued-nuclear-test-films)).
- **Beirute, 04/08/2020: o porte de uma nuke pequena, com centenas de vídeos e o som real.**
  - **0,5 kt** (limite 1,12 kt) pela chegada do choque em 38 pontos ([Rigby et al.](https://core.ac.uk/download/345552407.pdf)); **~1 kt** por sismo e infrassom ([Pilger 2021](https://doaj.org/article/59ebe76a57554324a65c66d27c3dc016)); **637 t** ([Stennett](https://zendy.io/title/10.1002/prep.202000227)).
  - **O domo de condensação branco** e a **nuvem marrom-avermelhada** (NOx do nitrato de amônio) são a melhor referência de cor e tempo que temos.
- **Little Feller I:** o filme do Ivy Flats (1962).

**Palestras e técnica (volumes acesos pelo mundo):**
- **Guerrilla, Nubis:**
  - nuvens volumétricas em ray march ([2017](https://advances.realtimerendering.com/s2017/Nubis%20-%20Authoring%20Realtime%20Volumetric%20Cloudscapes%20with%20the%20Decima%20Engine%20-%20Final%20.pdf), [2022](https://www.guerrilla-games.com/read/nubis-evolved));
  - **"Nubis³" (SIGGRAPH 2023):** nuvens em voxel **modeladas por simulação de fluido**, aceleração por campos de distância comprimidos, bordas escuras e brilho interno ([Guerrilla](https://www.guerrilla-games.com/read/nubis-cubed), [PDF](https://advances.realtimerendering.com/s2023/Nubis%20Cubed%20(Advances%202023).pdf));
  - **"Real-Time Volumetric Superstorms" (GDC 2022):** tempestades com **relâmpago aceso por dentro** ([GDC Vault](https://www.gdcvault.com/play/1027688/The-Real-Time-Volumetric-Superstorms)).
  - **É o caminho certo para o cogumelo que recebe sombra e é aceso pelo fogo.**
- **Housemarque, "Can We Do It With Particles?":** névoa por simulação de fluido em tempo real ([GDC Vault](https://gdcvault.com/play/1027742/Visual-Effects-Summit-Can-We)).
- **Battlefront II em camadas** (Schreibt) e o resto já estão em `EXPLOSOES.md`.

---

## 5. Som

### 5.1 Como soa de verdade

**O atraso é d/343** ([Glasstone §3.09](https://www.fourmilab.ch/etexts/www/effects/eonw_3.pdf); a chegada do choque calculada [E]):

| Distância | Atraso | Pico com 20 t | Pico com 1 kt | O que se ouve [E] |
|---|---|---|---|---|
| ~150 m (a nossa câmera) | **0,3–0,5 s** | ~185 dB (5 psi) | — | clarão, **meio segundo de silêncio**, e um **soco** com pressão no peito |
| 1 km | **2,7 s** (20 t) / 2,3 s (1 kt) | ~160 dB | ~175 dB | **estalo seco** (onda em N, às vezes duplo), corpo grave, ronco |
| 5 km | **14–15 s** | ~140 dB | ~156 dB | estalo arredondado, **"tum"** grave, janelas vibrando, ronco longo |
| 20 km | **~58 s** | ~123 dB | ~139 dB | só grave: **trovão rolando** de vários segundos, eco no relevo |

**O ar come os agudos** (ISO 9613-1, ~20 °C e 70% de umidade; o mesmo 117 dB/km do `CONCEITO_SOM`): 125 Hz ~0,4 dB/km, 500 Hz ~2, 1 kHz ~4, 2 kHz ~10, 4 kHz ~33, 8 kHz ~117.
- **A 5 km** não sobra nada acima de ~2 kHz.
- **A 20 km**, só abaixo de ~250 Hz.
- No ar seco do deserto os médios morrem ainda mais [E].

**Os relatos reais:**
- **Trinity:** Fermi, no acampamento-base a ~10 milhas: "uns 40 s depois a onda de ar me alcançou". Outro físico: o som "muito seco, e ribombou, e todas as montanhas ribombaram junto" ([atomicarchive](https://www.atomicarchive.com/resources/documents/trinity/fermi.html), [AHF](https://ahf.nuclearmuseum.org/ahf/key-documents/trinity-test-eyewitnesses/)).
- **Quase todo filme de teste tem som falso:** não gravaram, ou dublaram em cima.
  - Um "som de bomba" clássico da CBS é **uma cachoeira invertida e desacelerada** (Robert L. Mott).
  - O **Annie (1953)** tem áudio original, e o som chega **32 s depois do clarão** ([USC, Seeing Crossroads](https://scalar.usc.edu/works/seeing-crossroads/the-bombs-elusive-sound), [Straight Dope](https://boards.straightdope.com/t/where-does-that-standard-reverberating-boom-atomic-explosion-sound-effect-come-from/674898)).
- **Submunições:** o "pipocar" contínuo (§2d).
- **Míssil balístico:** chega **antes do alarme** (Kiev). O som do voo chega **depois do impacto**, na ordem invertida (§2c).
- **SCUD em 1991:** a sirene de ataque aéreo, o rugido do Patriot subindo e o estampido no alto.

### 5.2 Como os filmes e jogos fizeram

- **Oppenheimer (Richard King):** o miolo da cena são os **30–40 s** que o choque leva até o bunker. O "silêncio" é **uma camada densa de ambiente e respiração**, não silêncio digital, e o estrondo chega **quando o público já parou de esperar** ([A Sound Effect](https://www.asoundeffect.com/oppenheimer-film-sound/), [Yahoo/IndieWire](https://ca.news.yahoo.com/boldest-sound-oppenheimer-silence-210000937.html)).
- **DICE, o áudio HDR** (Bad Company, GDC 2009, Anders Clerwall):
  - mede a sonoridade no ouvinte e escala tudo, numa faixa do quase inaudível até ~130 dB;
  - **não é compressão**: mexe em valores lógicos de sonoridade;
  - **a sonoridade decide também quem é cortado**, porque o mais alto é o mais importante;
  - tiros e explosões em estéreo ([Frostbite](https://www.ea.com/frostbite/news/how-hdr-audio-makes-battlefield-bad-company-go-boom), [slides](https://www.slideshare.net/slideshow/adaptive-mixing-in-frostbite/3128152)).
  - **Para a superarma:** ela é o som mais alto possível e "afunda" todo o resto.
- **C&C:**
  - o padrão do gênero é a **voz do anunciador** em três momentos: superarma detectada, pronta, lançada;
  - o Tiberian Sun tem uma fala cortada de aviso de míssil nuclear chegando ([TCRF](https://tcrf.net/Command_%26_Conquer:_Tiberian_Sun/Unused_Sounds));
  - as falas exatas do Generals e do RA2 eu **não consegui confirmar** [NC];
  - **não copiar frases-marca** (o "Nuclear launch detected" é do StarCraft); escrever as nossas, em inglês (o jogo é em inglês).

### 5.3 Roteiro de camadas de uma superarma (molde; t = 0 no impacto)

| Momento | Tempo | Quem ouve | Camadas |
|---|---|---|---|
| **Construída** | uma vez | todos | voz curta: "superarma inimiga detectada" + relógio público (como o ZH) |
| **Pronta** | uma vez | o dono | voz e som de travamento mecânico |
| **Lançamento** | T−15 a T−8 s | todos (abafado, sem posição) + o dono | **voz de alerta**. No local: a abertura do lançador (o teto do Krona correndo, o MAZ levantando), o **"pop" do lançamento a frio** ou a **ignição** (rugido de 20–80 Hz), a subida se afastando |
| **Alarme no alvo** | T−10 a 0 s | quem está na base atingida | **a sirene de ataque aéreo da própria base**, diegética, como em Israel em 1991; mais um bipe de alarme químico no caso dos SCUDs. É o que avisa sem pôr nada na tela |
| **Voo** | T−10 a 0 s | — | balístico chega **mudo** (é a física, e é o terror). O enxame chinês, ao contrário, **zumbe crescendo** por 5–10 s |
| **Clarão** | 0 | todos na tela | **o resto da mixagem cai −15 a −25 dB** em 50–100 ms, música junto; só um sopro de ar ou zumbido fica (a "respiração" do Oppenheimer). Vozes nunca são cortadas |
| **Chegada** | d/343 (0,3–0,6 s na câmera) | — | **estalo** (0–30 ms), **soco grave** (30–100 Hz, 50–400 ms), **pressão** de sub (20–40 Hz) junto com o **tranco da câmera** |
| **Ordem invertida** (USA e balísticos) | +0,5 a +5 s | — | o **rasgo supersônico** chegando depois do impacto, subindo |
| **Destroços** | +0,3 a +4 s | — | chapas, telhados, vidros, a torre de tanque caindo, cada som no lugar onde cai |
| **Vento de volta** | +1 a +10 s | — | um **sopro para o centro** e o rugido baixo da coluna subindo (cogumelo) |
| **Cauda** | +3 a +30 s | — | **trovão rolando** com eco no relevo e nos prédios, fogo crepitando, explosões secundárias (paióis), alarmes de veículos; a mixagem volta em 2–4 s |
| **Mapa inteiro** | atraso pela distância da câmera de cada jogador, comprimido [licença] | todos fora da tela | só o grave (< 250 Hz), com direção no estéreo no máximo: "aconteceu algo enorme ali" |

---

## 6. As lições: de arrepiar e legível de cima

1. **O céu não está na tela.** Tudo que acontece no alto se conta pela **luz e pela sombra no chão**: o clarão que vira as sombras, a sombra do drone-mãe, a sombra da cabeça do cogumelo escurecendo a base. Também pelos **riscos entrando pelo topo** do quadro.
2. **Na nossa escala, o silêncio é físico e curto.** A 100–200 m são **0,3–0,6 s** entre o clarão e o estrondo. Esse meio segundo, com a mixagem afundada, é o "Oppenheimer" do RTS. Esticar só com licença (o abafamento de quem foi cegado e ensurdecido), nunca atrasando o choque.
3. **Avisar sem cobrir:** a sirene **dentro** da base atingida, a voz e o relógio público. Desenhar a área antes do golpe por alguns segundos (o Ion Cannon do C&C3) é decisão de jogo sua. Nada de nuvem na tela antes do impacto.
4. **Camadas curtas e na ordem física:**
   1. clarão de 1–3 quadros (o duplo pulso não aparece abaixo de ~5 kt: um clarão só);
   2. bola de 0,5–2 s;
   3. **o choque na velocidade real** (a base varrida em ~0,3 s, a areia subindo no lugar);
   4. domo branco só com ar úmido;
   5. **o vento de volta puxando a poeira para dentro**;
   6. o caule e a cabeça **saindo do nível das unidades em 3–5 s**;
   7. a cauda longa.
5. **A área lida pelo chão:** a varredura da areia e o anel queimado mostram o raio por 0,3–1,7 s. Depois ficam a **marca, a cratera e o campo** (radiação ou gás) como informação de jogo. Além do anel de morte, a poeira fica **baixa (≤ 2–3 m) e translúcida**, com as unidades sempre visíveis.
6. **Volume aceso pelo mundo, sempre:** cogumelo, gás e poeira **recebem a sombra do mapa, fazem sombra e são acesos** pelo fogo e pela bola. Nunca aditivo, que é o erro do ZH. Técnica: volumes em voxel modelados por simulação (Nubis³).
7. **Comprimir a altura sem quebrar a física:** a nuvem real de 2 km vira 200–300 m (λ ≈ 0,12–0,15) e o tempo vai junto por √λ. A onda de choque **não** se comprime.
8. **Cada facção, uma assinatura:**
   - **USA:** precisão e luz (riscos persistentes, chegada muda, rasgo invertido);
   - **China:** massa (enxame ou fogo);
   - **GLA:** sujeira e imprecisão (bamboleio, quebra, laranja do NOx, gás pesado escorrendo);
   - **Rússia:** a nuke (clarão cegante curto, **nuvem castanha**, caule de terra, campo de radiação).
9. **O "depois" vende mais que o estouro** (CoD4, Threads, World in Conflict): a reação do mundo, o eco que rola, o vento que volta, a cratera **funda o bastante** para ler de cima.
10. **Não acreditar em número de propaganda:** MOAB de "1 milha" (foram 150 m), FOAB de 44 t, Patriot de 1991, Rods from God, Sarmat e Avangard. Os anéis saem da conta (KB e Glasstone), conferida contra o real (MOAB/Alcis, Beirute, Little Feller).
11. **Duração total do espetáculo: 20–45 s.** Os 3 s centrais (clarão, choque, vento de volta) são intocáveis. No multiplayer, **a câmera não é tomada** (só um sinal no mapa); o cinema fica fora do MP (`CONCEITO_EXPLOSOES` E17).
12. **Desempenho:** cogumelo e gás grandes em volume custam caro na 1660 (é evento raro; `EXPLOSOES.md` estima 5–15 ms enquanto enche a tela). Medir antes, com o volume compartilhado do Fluxo.

---

### Fontes principais (as demais estão no texto)
- Glasstone & Dolan 1977 (fourmilab): [cap. II](https://www.fourmilab.ch/etexts/www/effects/eonw_2.pdf), [III](https://www.fourmilab.ch/etexts/www/effects/eonw_3.pdf), [VI](https://www.fourmilab.ch/etexts/www/effects/eonw_6.pdf), [VII](https://www.fourmilab.ch/etexts/www/effects/eonw_7.pdf); [Glasstone 1962 transcrito](https://www.abomb1.org/nukeffct/airburst.html)
- Kingery-Bulmash/Swisdak: ver `docs/ESTUDO_ONDA_DE_CHOQUE.md`
- [DOE/NV-1200 (Little Feller)](https://nnss.gov/wp-content/uploads/2023/04/DOENV_1200.pdf), [Davy Crockett](https://en.wikipedia.org/wiki/Davy_Crockett_(nuclear_device)), [Little Feller](https://en.wikipedia.org/wiki/Little_Feller_(nuclear_tests))
- Vídeo e tempo real: [Rigby (Beirute)](https://core.ac.uk/download/345552407.pdf), [38 North (reentrada)](https://www.38north.org/2017/07/melleman073117/), [LLNL (filmes)](https://www.llnl.gov/news/physicist-declassifies-rescued-nuclear-test-films)
- Jogos: `captures/fxlab/tread_study/aaa_research/zh_src/ParticleSystem.ini` (o ZH original), [Schreibt FO4](http://simonschreibt.de/gat/fallout-4-the-mushroom-case/), [Frostbite HDR](https://www.ea.com/frostbite/news/how-hdr-audio-makes-battlefield-bad-company-go-boom), [Nubis³](https://www.guerrilla-games.com/read/nubis-cubed)
