// A areia do deserto v1 (aprovada pelo Felipe em 12/10: "uma das melhores até agora"), congelada pro WorldForge.
// É o Custom node do material do chão da maquete (Game/Tools/look_lab.py, GROUND_CODE), com o caminho novo da areia
// ligado (SALB = 1). As entradas e os números estão em sand_desert_v1.json.

float2 p = WP.xy * 0.01;
float2 luv = (p - float2(LX, LY)) / LS;
float LANEX = (abs(p.x) > 12.0) ? 1.0 : 0.0;        // fora do cruzamento a rua corre em X; no meio, em Y
float4 L = Layout.Sample(LayoutSampler, luv);
if (any(luv < 0.0) || any(luv > 1.0)) L = 0.0;

// --- areia ---
float3 macroS = Beach.Sample(BeachSampler, p / 150.0 + 0.13).rgb;
float macro = lerp(0.85, 1.15, saturate((macroS.r - 0.255) / 0.06));   // a foto de praia: R vai de ~0,26 a ~0,31
float3 sA = SandD.Sample(SandDSampler, p / 2.5).rgb;
float3 sB = SandD.Sample(SandDSampler, p / 9.25 + 0.37).rgb;
float3 sand = lerp(sA, sB, 0.35);
float3 sFar = Beach.Sample(BeachSampler, p / 45.0 + 0.21).rgb * float3(1.32, 1.02, 0.72);
sand = lerp(sand, sFar, 0.45 + 0.3 * saturate((Depth - 1000.0) / 14000.0)) * macro * float3(1.21, 1.12, 1.03) * float3(SGR, SGG, SGB);   // medido: a areia da arte é mais vermelha e um pouco mais clara; × o ajuste da cena
if (SALB > 0.5) {
    // o albedo da areia pela física (a tabela PBR da Epic: areia de deserto ~0,36–0,44; estudo 12/10): a textura só dá o
    // desenho (claro/escuro em volta de 1); a cor é a da cena. A cor da arte já vem acesa pelo sol laranja: não copiar.
    // o desenho: a areia fina pisada (gravelly_sand a 1,3 m) e a foto de praia a 6 m (manchas de pisada e montinhos)
    float3 g1 = GravD.Sample(GravDSampler, p / 1.3).rgb;
    float3 g2 = GravD.Sample(GravDSampler, float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 4.1 + 0.37).rgb;
    float sl = dot(lerp(g1, g2, 0.4), float3(0.2126, 0.7152, 0.0722)) / 0.2;
    float mv8 = saturate((Beach.Sample(BeachSampler, p / 8.0 + 0.71).g - 0.22) / 0.04);
    float mv40 = saturate((Beach.Sample(BeachSampler, p / 40.0 + 0.29).r - 0.26) / 0.05);
    float mv200 = saturate((Beach.Sample(BeachSampler, p / 200.0 + 0.53).g - 0.22) / 0.04);
    float macro3 = 1.0 + 0.2 * ((mv8 - 0.5) + (mv40 - 0.5) + (mv200 - 0.5)) / 1.5;   // ±10% em 3 escalas (8, 40, 200 m)
    sand = float3(SAR, SAG, SAB) * lerp(1.0, sl, 0.25) * macro3;   // o desenho da textura fraco: forte, a areia lia como concreto sujo
    // o acabamento fino (o Felipe, 12/10: "um leve acabamento melhor" no alvo): as frestinhas entre os grãos escurecem
    float gao = GravM.Sample(GravMSampler, p / 1.3).r;
    sand *= lerp(1.0, gao * gao, 0.7);
}
float2 sNxy = SandN.Sample(SandNSampler, p / 2.5).rg * 2.0 - 1.0;
if (SALB > 0.5) {
    // o relevo fino: os grãos e as pisadas (gravelly_sand) e os montinhos macios (a foto aérea de praia a 6 m), que o sol
    // baixo marca de claro/escuro (a arte de perto é cheia deles; as ondulações regulares quase não aparecem)
    float2 n1 = GravN.Sample(GravNSampler, p / 1.3).rg * 2.0 - 1.0;
    n1 *= 1.35;                                    // o grão mais marcado (o acabamento do alvo de perto)
    float2 n2 = BeachN.Sample(BeachNSampler, p / 6.0 + 0.19).rg * 2.0 - 1.0;
    float2 n3 = BeachN.Sample(BeachNSampler, float2(0.6 * p.x + 0.8 * p.y, -0.8 * p.x + 0.6 * p.y) / 2.3 + 0.61).rg * 2.0 - 1.0;
    float rp = smoothstep(0.35, 0.75, saturate((Beach.Sample(BeachSampler, p / 25.0 + 0.83).g - 0.22) / 0.04));   // ondulação só em manchas
    sNxy = n1 * 1.0 + n2 * lerp(0.25, 0.9, rp) + n3 * 0.45;
}
// as ondulações de vento (Journey, GDC 2013, reconstruído pelo Zucconi; estudo 12/10): duas bandas (11 cm e 35 cm),
// a crista assimétrica (o lado de sotavento íngreme: o sol baixo risca claro/escuro), entortadas por ruído, com
// bifurcação, e apagadas quando a onda fica menor que ~4 px (fwidth). Só na normal: o relevo de cm não dá paralaxe.
if (RIPPLE > 0.0) {
    float2 wd = normalize(float2(0.8, 0.6));
    float ang = (saturate((Beach.Sample(BeachSampler, p / 600.0 + 0.11).r - 0.26) / 0.05) - 0.5) * 0.52;   // ±15°
    wd = float2(wd.x * cos(ang) - wd.y * sin(ang), wd.x * sin(ang) + wd.y * cos(ang));
    float rmask = smoothstep(0.3, 0.55, saturate((Beach.Sample(BeachSampler, p / 180.0 + 0.47).g - 0.22) / 0.04));
    float2 rip = float2(0.0, 0.0);
    [unroll] for (int b = 0; b < 3; b++) {
        float lam = (b == 0) ? 0.11 : ((b == 1) ? 0.35 : 0.8);           // + a ondulação larga (dunazinhas de 80 cm)
        float A = (b == 0) ? 0.005 : ((b == 1) ? 0.02 : 0.03);
        float nW = saturate((Beach.Sample(BeachSampler, p / (120.0 * lam) + 0.3 * b).r - 0.26) / 0.05);
        float2 q = p + 0.4 * lam * (2.0 * nW - 1.0) * float2(1.0, 0.7);
        float nB = saturate((Beach.Sample(BeachSampler, p / 60.0 + 0.6 + 0.2 * b).g - 0.22) / 0.04);
        float phi = 6.2832 * dot(q, wd) / lam + 9.4 * nB;
        float psi = phi - 0.55 * sin(phi);
        float fade = 1.0 - smoothstep(0.8, 1.6, fwidth(phi));
        float dh = A * fade * cos(psi) * (1.0 - 0.55 * cos(phi)) * 6.2832 / lam;
        rip += -dh * wd;
    }
    sNxy += rip * rmask * RIPPLE / 0.8;      // (a normal do chão é vn + nxy·0,8)
}
float3 sM = SandM.Sample(SandMSampler, p / 2.5).rgb;

// --- asfalto ---
float3 aA = AsphD.Sample(AsphDSampler, p / 2.2).rgb;
float3 aB = AsphD.Sample(AsphDSampler, p / 8.14 + 0.37).rgb;
float3 asph = lerp(aA, aB, 0.35) * 1.3;    // medido (ponto a ponto, 11/10): o asfalto da arte é bem mais claro
float3 aFar = AerAsph.Sample(AerAsphSampler, p / 30.0 + 0.21).rgb * 0.9;
asph = lerp(asph, aFar, 0.45 + 0.3 * saturate((Depth - 1000.0) / 14000.0)) * macro * 1.72 * float3(AGR, AGG, AGB);   // o ganho depois da foto aérea (medido 11/10); × o ajuste da cena
float2 aNxy = AsphN.Sample(AsphNSampler, p / 2.2).rg * 2.0 - 1.0;
float3 aM = AsphM.Sample(AsphMSampler, p / 2.2).rgb;
float3 dust = float3(0.30, 0.20, 0.12);
asph = lerp(asph, dust * 1.3, 0.10);                       // o pó quente do deserto em tudo

// as rachaduras: duas amostras (12 m e 16,4 m girada 37°), trocando por manchas
float2 pr = float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 16.4 + 0.37;
float4 C1 = Cracks.Sample(CracksSampler, p / 12.0);
float4 C2 = Cracks.Sample(CracksSampler, pr);
float sel = smoothstep(0.228, 0.244, Beach.Sample(BeachSampler, p / 40.0 + 0.19).g);   // G vai de ~0,22 a ~0,26
float4 C = lerp(C1, C2, sel);
// trechos muito rachados e trechos quase lisos (manchas de ~60 m e ~20 m), como nas artes
float zone = saturate((Beach.Sample(BeachSampler, p / 60.0 + 0.61).r - 0.262) / 0.04) * 0.7
           + saturate((Beach.Sample(BeachSampler, p / 20.0 + 0.27).g - 0.22) / 0.035) * 0.3;
float crackAmt = lerp(0.0, 0.8, smoothstep(0.45, 0.85, zone));   // rara: a arte tem trechos lisos e a fresta só aqui e ali (crítica 11/10)
C.r *= crackAmt;
C.g *= lerp(0.5, 1.0, crackAmt);
float2 n1 = CracksN.Sample(CracksNSampler, p / 12.0).rg * 2.0 - 1.0;
float2 t2 = CracksN.Sample(CracksNSampler, pr).rg * 2.0 - 1.0;
float2 n2 = float2(0.8 * t2.x + 0.6 * t2.y, -0.6 * t2.x + 0.8 * t2.y);   // de volta pro mundo
float2 cN = lerp(n1, n2, sel) * crackAmt;
asph *= 0.88 + 0.24 * C.b;                                  // cada placa envelheceu de um jeito
// a vida do asfalto (crítica 11/10): remendos retangulares mais escuros, manchas de óleo e marcas de pneu
float2 cell = floor(p / 3.5);
float rnd = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float rnd2 = frac(sin(dot(cell, float2(39.346, 11.135))) * 24634.6345);
float2 fp = frac(p / 3.5);
float patchBox = step(0.82, rnd) * step(0.12, fp.x) * step(fp.x, 0.12 + 0.5 + 0.35 * rnd2) * step(0.2, fp.y) * step(fp.y, 0.75);
asph *= lerp(1.0, 0.72, patchBox);
float oil = Beach.Sample(BeachSampler, p / 2.3 + 0.83).r;
asph *= lerp(1.0, 0.6, smoothstep(0.293, 0.303, oil));
float tireX = Beach.Sample(BeachSampler, float2(p.x / 40.0, p.y / 0.55) + 0.15).g;
float tireY = Beach.Sample(BeachSampler, float2(p.x / 0.55, p.y / 40.0) + 0.65).g;
float tire = max(smoothstep(0.247, 0.256, tireX) * LANEX, smoothstep(0.247, 0.256, tireY) * (1.0 - LANEX));
asph *= lerp(1.0, 0.8, tire);
asph = lerp(asph, asph * 0.3, saturate(C.r * 0.85));   // a fresta: fina (o miolo da textura), não a borda alargada
asph = lerp(asph, dust * 1.2, C.g * 0.6);                   // areia nas frestas e as faixas de vento
float aRough = saturate(aM.g + 0.1 * C.r + 0.1 * C.g);

// --- faixa pintada (gasta) ---
float wear = smoothstep(0.215, 0.245, Beach.Sample(BeachSampler, p / 1.7 + 0.31).g);
float paint = L.a * wear * (1.0 - C.r) * (1.0 - C.g);
asph = lerp(asph, float3(0.62, 0.60, 0.55), paint);

// --- terra batida ---
float3 dirt = sand * float3(0.63, 0.57, 0.50);

// --- mistura ---
float3 col = sand;
float2 nxy = sNxy;
float r = sM.g, ao = sM.r;
col = lerp(col, asph, L.r);
nxy = lerp(nxy, aNxy * 0.6 + cN, L.r);
r = lerp(r, lerp(aRough, 0.55, paint), L.r);
ao = lerp(ao, aM.r, L.r);
col = lerp(col, dirt, L.b);
col = lerp(col, sand * 1.04, L.g);
nxy = lerp(nxy, sNxy, L.g);
r = lerp(r, sM.g, L.g);
Rough = r;
AO = ao;
// o detalhe por cima da normal da malha (o relevo de verdade)
float3 vn = normalize(VN);
Nrm = normalize(vn + float3(nxy, 0.0) * 0.8);
return col;
