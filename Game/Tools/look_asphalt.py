"""O asfalto da volta 15 (13/10, aprovado pelo Felipe: "ficou bem melhor a estrada") como código solto, pra o chão do
WorldForge (e do jogo) ligar na camada Asphalt, do mesmo jeito que a memória do chão (look_mf.py): o WorldForge lê a
string ASPHALT_CODE na hora de montar e põe num nó Custom (float3).

É o mesmo asfalto do chão da maquete (look_lab.py, GROUND_CODE, "--- asfalto ---"), separado da maquete:
  - a foto do asfalto danificado a 2,2 m e 8,14 m, com a foto aérea de asfalto no longe;
  - o tom (AG), a película de areia em manchas largas (a areia de baixo), o pó quente;
  - as rachaduras do T_LL_Cracks (duas amostras giradas, trechos muito e pouco rachados), engrossadas pra ler a 5–8 cm por
    pixel, com areia dentro;
  - os remendos (células de 9 m, 6%), as manchas de óleo e as faixas de pneu ao longo da pista.

Entradas do Custom (nesta ordem):
  WP       float3  a posição no mundo (cm), MaterialExpressionWorldPosition
  Depth    float   a profundidade do pixel (cm), MaterialExpressionPixelDepth
  Sand     float3  a cor da areia em volta (linear): a película e o pó puxam pra ela
  RD       float2  a direção da pista no mundo (normalizada); sem traçado, (1, 0)
  AG       float3  o tom do asfalto: (0,216, 0,396, 0,648) na volta 15
  AsphD    tex     /Game/Slice/Surfaces/road_damaged_2/T_road_damaged_2_diff_4k
  AsphN    tex     /Game/Slice/Surfaces/road_damaged_2/T_road_damaged_2_nor_dx_4k
  AsphM    tex     /Game/Slice/Surfaces/road_damaged_2/T_road_damaged_2_arm_4k
  AerAsph  tex     /Game/Slice/Surfaces/aerial_asphalt_01/T_aerial_asphalt_01_diff_4k
  Beach    tex     /Game/Slice/Surfaces/aerial_beach_01/T_aerial_beach_01_diff_4k (o ruído)
  Cracks   tex     captures/look/textures/T_LL_Cracks.png (importar linear, sRGB desligado, compressão BC7/máscara)
  CracksN  tex     captures/look/textures/T_LL_Cracks_N.png (importar como normal)
Saídas extras: Nxy (float2, a perturbação da normal no plano, somar como vn + float3(Nxy, 0)·0,8), Rough (float1),
AO (float1).
"""

ASPHALT_CODE = r"""
float2 p = WP.xy * 0.01;
float2 rd = normalize(RD + float2(1e-5, 0.0));
float2 q = float2(dot(p, rd), dot(p, float2(-rd.y, rd.x)));          // ao longo e através da pista (m)
float3 macroS = Beach.Sample(BeachSampler, p / 150.0 + 0.13).rgb;
float macro = lerp(0.85, 1.15, saturate((macroS.r - 0.255) / 0.06));
float3 aA = AsphD.Sample(AsphDSampler, p / 2.2).rgb;
float3 aB = AsphD.Sample(AsphDSampler, p / 8.14 + 0.37).rgb;
float3 asph = lerp(aA, aB, 0.35) * 1.3;
float3 aFar = AerAsph.Sample(AerAsphSampler, p / 30.0 + 0.21).rgb * 0.9;
asph = lerp(asph, aFar, 0.45 + 0.3 * saturate((Depth - 1000.0) / 14000.0)) * macro * 1.72 * AG;
float2 aNxy = AsphN.Sample(AsphNSampler, p / 2.2).rg * 2.0 - 1.0;
float3 aM = AsphM.Sample(AsphMSampler, p / 2.2).rgb;
float3 dust = float3(0.30, 0.20, 0.12);
asph = lerp(asph, dust * 1.3, 0.10);
float film = 0.12 + 0.16 * smoothstep(0.228, 0.246, Beach.SampleLevel(BeachSampler, p / 14.0 + 0.71, 8.0).g);   // (13/10: sem a onça)
asph = lerp(asph, Sand * 0.92, film);
float2 pr = float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 16.4 + 0.37;
float4 C1 = Cracks.Sample(CracksSampler, p / 12.0);
float4 C2 = Cracks.Sample(CracksSampler, pr);
float dl1 = 0.035 / 12.0, dl2 = 0.035 / 16.4;
C1.r = max(C1.r, max(max(Cracks.Sample(CracksSampler, p / 12.0 + float2(dl1, 0)).r, Cracks.Sample(CracksSampler, p / 12.0 - float2(dl1, 0)).r),
                     max(Cracks.Sample(CracksSampler, p / 12.0 + float2(0, dl1)).r, Cracks.Sample(CracksSampler, p / 12.0 - float2(0, dl1)).r)));
C2.r = max(C2.r, max(max(Cracks.Sample(CracksSampler, pr + float2(dl2, 0)).r, Cracks.Sample(CracksSampler, pr - float2(dl2, 0)).r),
                     max(Cracks.Sample(CracksSampler, pr + float2(0, dl2)).r, Cracks.Sample(CracksSampler, pr - float2(0, dl2)).r)));
float sel = smoothstep(0.228, 0.244, Beach.Sample(BeachSampler, p / 40.0 + 0.19).g);
float4 C = lerp(C1, C2, sel);
float zone = saturate((Beach.Sample(BeachSampler, p / 60.0 + 0.61).r - 0.262) / 0.04) * 0.7
           + saturate((Beach.Sample(BeachSampler, p / 20.0 + 0.27).g - 0.22) / 0.035) * 0.3;
float crackAmt = lerp(0.15, 0.9, smoothstep(0.35, 0.8, zone));
C.r *= crackAmt;
C.g *= lerp(0.5, 1.0, crackAmt);
float2 n1 = CracksN.Sample(CracksNSampler, p / 12.0).rg * 2.0 - 1.0;
float2 t2 = CracksN.Sample(CracksNSampler, pr).rg * 2.0 - 1.0;
float2 n2 = float2(0.8 * t2.x + 0.6 * t2.y, -0.6 * t2.x + 0.8 * t2.y);
float2 cN = lerp(n1, n2, sel) * crackAmt;
asph *= 0.88 + 0.24 * C.b;
float2 pc = q / 9.0;
pc.x += floor(pc.y) * 0.37;
float2 cell = floor(pc);
float rnd = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float rnd2 = frac(sin(dot(cell, float2(39.346, 11.135))) * 24634.6345);
float rnd3 = frac(sin(dot(cell, float2(73.156, 52.235))) * 15731.7431);
float2 fp = frac(pc);
float2 pc0 = float2(0.1 + 0.45 * rnd2, 0.15 + 0.4 * rnd3);
float2 psz = float2(0.18 + 0.3 * rnd3, 0.1 + 0.22 * rnd2);
float patchBox = step(0.94, rnd) * step(pc0.x, fp.x) * step(fp.x, pc0.x + psz.x) * step(pc0.y, fp.y) * step(fp.y, pc0.y + psz.y);
asph *= lerp(1.0, 0.78 + 0.12 * rnd2, patchBox);
float oil = Beach.Sample(BeachSampler, p / 2.3 + 0.83).r;
asph *= lerp(1.0, 0.6, smoothstep(0.293, 0.303, oil));
float tire = smoothstep(0.247, 0.256, Beach.Sample(BeachSampler, float2(q.x / 40.0, q.y / 0.55) + 0.15).g);
asph *= lerp(1.0, 0.8, tire);
asph = lerp(asph, asph * 0.22, saturate(C.r * 0.95));
asph = lerp(asph, dust * 1.2, C.g * 0.6);
Rough = saturate(aM.g + 0.1 * C.r + 0.1 * C.g);
AO = aM.r;
Nxy = aNxy * 0.6 + cN;
return asph;
"""


# ---------------------------------------------------------------------------------------------------------------------
# O asfalto v2 (13/10, pedido do Felipe: "no espírito da areia v2, juntar vários padrões de forma inteligente"), por cima
# do asfalto de ANTES do WorldForge (o que ele aprovou na luz do jogo). A cor não muda: tudo é relativo (multiplica a cor
# de antes perto de 1), pra continuar calibrado na v2_aprovada.
#
# Os padrões vêm do catálogo de defeitos de pavimento do FHWA (Distress Identification Manual, FHWA-HRT-13-092): cada
# defeito nasce num lugar da pista, e é isso que lê "estrada de verdade" em vez de ruído:
#   1. (a trilha de roda polida com o jacaré: tirada no A/B do Felipe, 13/10; virava 4 listras claras a 61–100 m);
#   2. o meio da pista: a linha de óleo/pingo, escura, com falhas;
#   3. as trincas em bloco (placas de 1–3 m), mais fortes no trecho abandonado;
#   4. a trinca longitudinal na emenda das pistas (o eixo) e as transversais a cada 5–15 m;
#   5. a desagregação na beira (0–1,5 m): o agregado aparece, mais claro e áspero;
#   6. os remendos: retângulos alinhados à pista, mais escuros e lisos, com a costura;
#   7. a areia na beira, mais do lado de onde vem o vento;
#   8. as zonas grandes (15–40 m): o trecho bom, o cansado e o abandonado (o que tira a repetição, como as zonas da areia).
#
# Entradas (o WorldForge assa RD/Across/Width/Edge na textura do RoadDir, já decodificados):
#   WP float3 (cm), Depth float (cm), RD float2 (a direção da pista), Across float (m, com sinal, + à esquerda do RD),
#   Width float (m), Edge float (m, = Width/2 − |Across|, + dentro), Base float3 (a cor do asfalto de antes),
#   Rough float, N float3 (a normal tangente de antes), WindDir float2 (pra onde o vento sopra),
#   Beach tex (aerial_beach_01 diff), Cracks tex (T_LL_Cracks), CracksN tex (T_LL_Cracks_N),
#   OilAtlas tex (captures/look/textures/T_LL_Oil.png, look_oil.py: linear, sRGB desligado, máscara; lido com SampleLevel 0),
#   OilPuddle float (a chance da poça viscosa por célula de 14 m: 0,07; 1 = todas viram poça, pra fotografar).
# Saídas: a cor (float3), Nout (float3, normal tangente), Rout (float).
ASPHALT_V2_CODE = r"""
float2 p = WP.xy * 0.01;
float2 rd = normalize(RD + float2(1e-5, 0.0));
float2 rn = float2(-rd.y, rd.x);                                     // + à esquerda (o lado do Across positivo)
float along = dot(p, rd);
float ac = Across;
float aa = abs(ac);
float3 col = Base;
float rough = Rough;
float2 nxy = float2(0.0, 0.0);
float lum0 = dot(Base, float3(0.3, 0.59, 0.11));

// 8. as zonas grandes: bom, cansado e abandonado (manchas de 15–40 m pelo ruído da foto de praia)
float zA = saturate((Beach.Sample(BeachSampler, p / 35.0 + 0.13).r - 0.262) / 0.04);
float zB = saturate((Beach.Sample(BeachSampler, p / 55.0 + 0.47).g - 0.226) / 0.03);
float tired = smoothstep(0.4, 0.75, zA);
float aband = smoothstep(0.5, 0.85, zB) * (1.0 - 0.4 * tired);

// as pistas: duas, uma por sentido; o meio da pista a ±min(W/4; 2,2 m)
float laneC = min(Width * 0.25, 2.2);
float dLane = aa - laneC;                                             // + pra fora da pista, − pro eixo

// 1. (13/10, o Felipe no A/B: "essas trilhas aí não sei não, você não vê isso no dia a dia nem no game shot": a 61 e a 100 m
//    as trilhas de roda polidas viravam 4 listras claras paralelas. Saíram, junto com o jacaré preso nelas.)

// 2. o meio da pista: a linha de óleo, escura, com falhas ao longo
// o óleo v4 (13/10, pelas referências que o Felipe escolheu: o estacionamento visto de cima e a mancha acumulada): carimbos
// do atlas T_LL_Oil (look_oil.py: 0 a mancha fosca, 1 o grupinho separado, 2 os pingos, 3 a poça viscosa), RAROS e
// aleatórios: células de 14 m da rua, cada uma com uma chance de ter um carimbo, sorteado em tipo, lugar (a largura toda,
// mais perto do meio das pistas e do acostamento), giro e tamanho. Nada de fila nem de faixa.
float oDark = 0.0, oWet = 0.0, oRim = 0.0, oHalo = 0.0;
[unroll] for (int oy = -1; oy <= 1; oy++) {
    [unroll] for (int ox = -1; ox <= 1; ox++) {
        float2 oc = floor(float2(along / 14.0, ac / 7.0)) + float2(ox, oy);
        float hA = frac(sin(dot(oc, float2(12.9898, 78.233))) * 43758.5453);
        float hB = frac(sin(dot(oc, float2(39.346, 11.135))) * 24634.6345);
        float hC = frac(sin(dot(oc, float2(73.156, 52.235))) * 15731.7431);
        float hD = frac(sin(dot(oc, float2(26.651, 91.733))) * 32462.3197);
        // o tipo pela chance: a poça primeiro (OilPuddle, padrão 0,07; 1 = todas as manchas viram poça, pro teste), depois
        // 12% a fosca, 7% o grupinho, 6% os pingos (o resto: nada)
        float pP = saturate(OilPuddle);
        float kind = (hA < pP) ? 3.0 : (hA < pP + 0.12) ? 0.0 : (hA < pP + 0.19) ? 1.0 : (hA < pP + 0.25) ? 2.0 : -1.0;
        if (pP >= 0.999) kind = 3.0;
        if (kind < 0.0) continue;
        // o lugar: ao longo da célula inteira; através, o meio de uma das pistas ou o acostamento
        float side = (hC < 0.5) ? -1.0 : 1.0;
        float tgt = (hD < 0.65) ? laneC : Width * 0.5 - 1.2;
        float2 ctr = float2((oc.x + 0.1 + 0.8 * hB) * 14.0, side * tgt + (hD - 0.5) * 0.8);
        if (abs(ctr.y) > Width * 0.5 - 0.5) continue;
        float sc = 0.7 + 0.7 * frac(hB * 7.13);
        float an = hC * 6.2832;
        float2 dd = (float2(along, ac) - ctr) / (2.4 * sc);
        float2 uv = float2(cos(an) * dd.x - sin(an) * dd.y, sin(an) * dd.x + cos(an) * dd.y) + 0.5;
        if (any(uv < 0.0) || any(uv > 1.0)) continue;
        float4 O = OilAtlas.SampleLevel(OilAtlasSampler, (uv + float2(fmod(kind, 2.0), floor(kind / 2.0))) * 0.5, 0.0);
        oDark = max(oDark, O.r); oWet = max(oWet, O.g); oRim = max(oRim, O.b); oHalo = max(oHalo, saturate(O.a - O.r));
    }
}
col *= 1.0 - 0.2 * oHalo;                          // a auréola encharcada
col *= 1.0 - 0.82 * oDark;                         // o óleo
col *= 1.0 - 0.25 * oRim;                          // a beirada que seca por último
col *= lerp(float3(1.0, 1.0, 1.0), float3(1.0, 0.95, 0.88), oDark);   // um pouco mais marrom
rough = lerp(rough, 0.18, oWet);                   // a poça viscosa: o reflexo na beira e nas faixas (o resto, fosco; 0,18 pelo estudo GGX)

// 3. as trincas em bloco (placas de 1–3 m), no trecho abandonado
float blk = Cracks.Sample(CracksSampler, p / 6.5 + 0.53).r * aband;
col *= 1.0 - 0.6 * saturate(blk * 1.3);
float2 bN = (CracksN.Sample(CracksNSampler, p / 6.5 + 0.53).rg * 2.0 - 1.0) * aband;

// 4. a trinca longitudinal no eixo (sinuosa, com falhas) e as transversais a cada 5–15 m (nunca mais finas que ~6 cm:
//    abaixo disso cintila no pixel de 5–8 cm da câmera)
float wob = (Beach.Sample(BeachSampler, float2(along / 9.0, 0.37)).r - 0.285) * 3.0;
float lng = (1.0 - smoothstep(0.03, 0.07, abs(ac + wob))) * smoothstep(0.24, 0.25, Beach.Sample(BeachSampler, float2(along / 5.0, 0.81)).g);
float cellL = 9.0;
float ci = floor(along / cellL);
float h1 = frac(sin(ci * 12.9898) * 43758.5453), h2 = frac(sin(ci * 78.233) * 24634.6345);
float tpos = (ci + 0.2 + 0.6 * h1) * cellL + (Beach.Sample(BeachSampler, float2(ac / 4.0, ci * 0.13)).r - 0.285) * 2.5;
float trn = (1.0 - smoothstep(0.03, 0.07, abs(along - tpos))) * step(h2, 0.75) * step(aa, Width * 0.5 * (0.4 + 0.6 * h2));
float lin = saturate(lng + trn) * (0.35 + 0.65 * saturate(tired + aband));
col *= 1.0 - 0.6 * lin;

// 5. a desagregação na beira: o fino some e o agregado aparece (mais claro, cinza e áspero), mais no abandonado
float rav = (1.0 - smoothstep(0.15, 1.5, Edge)) * (0.35 + 0.65 * aband) * smoothstep(0.23, 0.25, Beach.Sample(BeachSampler, p / 1.3 + 0.29).g + 0.01);
col = lerp(col, lerp(col, lum0.xxx, 0.5) * 1.3, rav * 0.6);
rough = lerp(rough, min(rough + 0.12, 1.0), rav);

// 6. os remendos: retângulos alinhados à pista (células de 7 m ao longo × uma pista), mais escuros e lisos, com a costura
float2 q = float2(along / 7.0, (ac + Width * 0.5) / max(Width * 0.5, 1.0));
float2 cell = floor(q);
float r1 = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float r2 = frac(sin(dot(cell, float2(39.346, 11.135))) * 24634.6345);
float r3 = frac(sin(dot(cell, float2(73.156, 52.235))) * 15731.7431);
float2 f = frac(q);
// (v2d: os remendos liam "quadrados claros do nada": mais lisos, refletiam o sol baixo; agora foscos, mais escuros, e a borda
//  irregular, recortada à mão como remendo de verdade)
f += (float2(Beach.Sample(BeachSampler, p / 0.9 + 0.11).r, Beach.Sample(BeachSampler, p / 0.9 + 0.57).g) - float2(0.285, 0.237)) * float2(0.9, 0.5);
float2 c0 = float2(0.1 + 0.4 * r2, 0.15 + 0.3 * r3);
float2 sz = float2(0.15 + 0.4 * r3, 0.3 + 0.35 * r2);
float inPatch = step(c0.x, f.x) * step(f.x, c0.x + sz.x) * step(c0.y, f.y) * step(f.y, c0.y + sz.y);
float patchOn = inPatch * step(r1, 0.12 + 0.25 * tired);
float2 de = min(f - c0, c0 + sz - f) * float2(7.0, max(Width * 0.5, 1.0));           // a distância até a borda do remendo (m)
float seam = patchOn * (1.0 - smoothstep(0.02, 0.06, min(de.x, de.y)));
col *= lerp(1.0, 0.72 + 0.08 * r2, patchOn);
col *= 1.0 - 0.45 * seam;

// 7. a areia na beira, mais do lado de onde vem o vento (a borda que olha pra trás do vento)
float upwind = saturate(dot(rn * sign(ac), -normalize(WindDir + float2(1e-5, 0.0))));
float sand = (1.0 - smoothstep(0.0, 2.2, Edge)) * (0.35 + 0.65 * upwind) * smoothstep(0.228, 0.25, Beach.Sample(BeachSampler, p / 6.0 + 0.61).g);
col = lerp(col, float3(0.30, 0.20, 0.12) * 1.35, sand * 0.5);
rough = lerp(rough, 0.95, sand * 0.5);

nxy = bN * 0.4;
Nout = normalize(N + float3(nxy, 0.0));
Rout = saturate(rough);
return col;
"""


# ---------------------------------------------------------------------------------------------------------------------
# A faixa pintada v2 no jogo (16/10, RECEITAS_DO_LOOK §7; a maquete de 13/10, PAINTV2 no look_lab.py). O Felipe procurou na
# vila do jogo e não achou: ficou só na maquete. O que ele pediu: "areia em cima da faixa branca", "erosão e rachaduras",
# "às vezes ela tá apagada". O estudo nas favoritas (captures/look/faixa_branca_estudo.jpg): a da beira a ~0,5 m da borda,
# contínua, acompanhando a curva; creme empoeirada (nunca branco: ~0,55–0,6 de brilho contra ~0,3 do asfalto); a idade muda
# em trechos (quase nova, gasta com falhas, só o fantasma); a velha descasca em placas de 10–50 cm; a trinca passa por cima.
# Pro pixel do jogo (5–7 cm na tela, 7–12 cm por dentro do TSR): a faixa com 15 cm (13 cm virava sub-pixel e piscava) e o
# corte antisserrilhado pelo fwidth; a descascada em placas, não em pó.
# A areia que invade a pista vem DEPOIS deste código (o WorldForge mistura a areia por cima): a areia cobre a faixa.
#
# Entradas: WP float3 (cm); RF float4 = o T_RoadFrame do WorldForge já amostrado (R a distância com sinal até o eixo em m,
# G a meia largura em m, 0 = não pinta; B/A = sen/cos(2*pi*s/9 m) ao longo da estrada); Base float3 (a cor do asfalto que
# chega); Rough float; N float3 (a normal tangente); Dust float3 (a cor da areia do mapa, pro encardido); Cracks tex
# (T_LL_Cracks, o mesmo do asfalto v2).
# Saídas: a cor (float3), Nout (float3), Rout (float).
PAINT_V2_CODE = r"""
float2 p = WP.xy * 0.01;                                     // m
float3 col = Base;
float rough = Rough;
float3 nO = N;
float halfW = RF.g;
if (halfW > 0.5) {
    float a = abs(RF.r);
    // a da beira: contínua, a 0,55 m da borda, 15 cm; o eixo: tracejado, 3 m de cada 9 m, 12 cm
    float edgeD = halfW - a;
    float lineE = saturate((0.075 - abs(edgeD - 0.55)) / max(fwidth(edgeD), 1e-3) + 0.5);
    float ph = frac(atan2(RF.b, RF.a) / 6.2831853 + 1.0);
    float fwP = max(fwidth(ph), 1e-4);
    float dash = saturate((0.3333 - ph) / fwP + 0.5) * saturate(ph / fwP + 0.5);
    float lineC = saturate((0.06 - a) / max(fwidth(a), 1e-3) + 0.5) * dash;
    float pline = max(lineE, lineC);
    if (pline > 0.001) {
        // três ruídos de valor no mundo: a idade por trecho (30 m), as placas que descascam (2,4 m) e as lascas (0,6 m)
        float nz[3];
        float sc[3] = { 30.0, 2.4, 0.6 };
        for (int k = 0; k < 3; k++) {
            float2 q = p / sc[k] + float2(17.3 * k, 5.1 * k);
            float2 i = floor(q), f = frac(q);
            f = f * f * (3.0 - 2.0 * f);
            float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
            float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
            float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
            float h11 = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
            nz[k] = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
        }
        float life = lerp(0.12, 1.0, smoothstep(0.25, 0.75, nz[0]));          // 0,12 = só o fantasma; 1 = quase nova
        float thr = lerp(0.8, 0.12, life);
        float keep = smoothstep(thr - 0.04, thr + 0.04, nz[1]);                // a tinta velha sai em placas
        float chips = smoothstep(0.84, 0.92, nz[2]);                           // as lasquinhas ao longo
        float crackCut = 1.0 - saturate(Cracks.Sample(CracksSampler, p / 12.0).r * 1.6);   // a trinca passa por cima
        float paint = pline * keep * (1.0 - 0.9 * chips) * (0.55 + 0.45 * life) * crackCut;
        float ghost = pline * (1.0 - keep) * 0.18 * crackCut;                  // o fantasma: o asfalto mais claro onde a tinta esteve
        float3 paintC = lerp(float3(0.56, 0.53, 0.46), Dust * 0.95, 0.2 + 0.35 * (1.0 - life));   // creme empoeirada, encarde com a idade
        col = lerp(col, col * 1.25 + 0.02, ghost);
        col = lerp(col, paintC, paint);
        rough = lerp(rough, 0.55, paint);
        nO = normalize(lerp(N, float3(0.0, 0.0, 1.0), 0.5 * paint));           // a tinta alisa o agregado
    }
}
Nout = nO;
Rout = saturate(rough);
return col;
"""
