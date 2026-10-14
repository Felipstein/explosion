"""A função de material do chão com memória (o plugin RTSLook): MF_LookGroundMemory.

Rodar na Unreal do jogo (sem janela; o look.ps1 mf chama):
  UnrealEditor-Cmd Game/RTSGame.uproject -run=pythonscript -script=Game/Tools/look_mf.py -nullrhi

A divisão (12/10, combinada com a sessão do jogo): o jogo escreve ONDE e QUANDO o chão foi mexido (os cadernos de relevo
em volta da câmera: R altura com sinal, G idade, B sol, A revolvido; e o caderno da esteira com a garra fina); esta função
diz COMO o chão fica. O material do chão (o do WorldForge, build_slice.py ground_material) chama a função com a cor, a
normal (espaço tangente) e a rugosidade dele, e usa as saídas. Os parâmetros (os nomes que o jogo põe por
SetLandscapeMaterial*ParameterValue): GroundRelief, GroundReliefMid, GroundTrack (texturas), DetailBounds, MidBounds
(cm: mín X, mín Y, largura, altura) e ReliefRangeCm, ReliefTexel. Sem os cadernos (ReliefRangeCm = 0), a função não muda nada.

Como fica (o estudo e a 2ª opinião de 12/10; a marca na areia lê pela LUZ, não pela profundidade):
- a normal da altura pela diferença finita (o sulco e a bordinha pegando o sol rasante), mais a garra fina da esteira;
- a sombra do sol já calculada no carimbo (B), macia;
- a areia revolvida fresca um pouco mais escura e quente (a umidade de baixo), secando com a idade; a compactada mais lisa;
- a saída RippleMask: onde a ondulação da areia some (a esteira fresca apagou; volta com a idade);
- detalhe → meio → nada pelo tamanho do pixel no chão.
"""
import unreal

FOLDER = '/RTSLook/Materials'
NAME = 'MF_LookGroundMemory'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

CODE = r"""
float3 bc = BC; float3 nT = N; float rough = Rg;
Mask = 0.0; Emi = float3(0, 0, 0);
if (Range <= 0.0) { Nout = nT; Rout = rough; return bc; }
float2 p = WP.xy;
float2 uvD = (p - DB.xy) / max(DB.zw, 1.0);
float2 uvM = (p - MB.xy) / max(MB.zw, 1.0);
float inD = step(0.0, uvD.x) * step(uvD.x, 1.0) * step(0.0, uvD.y) * step(uvD.y, 1.0);
float inM = step(0.0, uvM.x) * step(uvM.x, 1.0) * step(0.0, uvM.y) * step(uvM.y, 1.0);
// o maior eixo do pixel no chão, em cm (13/10, medido pelo jogo: o length(fwidth(p)) soma |ddx|+|ddy| por eixo e, com a
// câmera girada a 45°, dava ~13–15 cm onde o pixel tem ~5×8 cm; o detalhe nunca entrava na vista do RTS e a cratera virava
// mancha do caderno do meio)
float fw = max(length(ddx(p)), length(ddy(p)));
float wD = inD * (1.0 - smoothstep(10.0, 16.0, fw));             // o detalhe em toda a faixa do zoom do RTS (o mais longe: ~7,5 cm)
float wM = inM * (1.0 - smoothstep(20.0, 40.0, fw)) * (1.0 - wD);
float w = wD + wM;
// (13/10, com o jogo: na câmera do RTS o pixel tem 10–20 cm e o wD zerava; a cor do caderno do meio não some com a
//  distância, só a normal)
float wMc = inM * (1.0 - wD);
float wc = wD + wMc;
if (wc <= 0.001) { Nout = nT; Rout = rough; return bc; }
float wn = max(w, 1e-4);
// (13/10 à noite, as lagartas que viravam "arame": 2ª opinião) os cadernos não têm mip: o passo da diferença finita é
// max(1 texel, o pixel no chão) e o B, a tinta e a máscara são a média de 4 amostras na área do pixel (o que o mip fazia
// na maquete). Sem isso a bordinha de 9 cm vira um degrau de 1–2 px preto (o "fio").
float tD = max(Texel, fw / max(DB.z, 1.0)), tM = max(Texel, fw / max(MB.z, 1.0));
float cmD = DB.z * Texel, cmM = MB.z * Texel;                    // cm por texel
float aD = 0.35 * tD, aM = 0.35 * tM;
float4 Rd = 0.25 * (Relief.Sample(ReliefSampler, uvD + float2(aD, aD)) + Relief.Sample(ReliefSampler, uvD + float2(-aD, aD))
                  + Relief.Sample(ReliefSampler, uvD + float2(aD, -aD)) + Relief.Sample(ReliefSampler, uvD + float2(-aD, -aD)));
float4 Rm = 0.25 * (ReliefMid.Sample(ReliefMidSampler, uvM + float2(aM, aM)) + ReliefMid.Sample(ReliefMidSampler, uvM + float2(-aM, aM))
                  + ReliefMid.Sample(ReliefMidSampler, uvM + float2(aM, -aM)) + ReliefMid.Sample(ReliefMidSampler, uvM + float2(-aM, -aM)));
float4 Rr = (Rd * wD + Rm * wMc) / wc;
float h = (Rr.r - 0.5) * 2.0 * Range;                            // a altura média no pixel, cm (o sulco < 0, a bordinha > 0)
float band = saturate(-h / 1.2);                                 // o miolo da faixa (o sulco), sem a bordinha
// a normal da altura (diferença finita no caderno de cada nível)
float hxD = (Relief.Sample(ReliefSampler, uvD + float2(tD, 0)).r - Relief.Sample(ReliefSampler, uvD - float2(tD, 0)).r) * 2.0 * Range / (2.0 * tD * DB.z);
float hyD = (Relief.Sample(ReliefSampler, uvD + float2(0, tD)).r - Relief.Sample(ReliefSampler, uvD - float2(0, tD)).r) * 2.0 * Range / (2.0 * tD * DB.z);
float hxM = (ReliefMid.Sample(ReliefMidSampler, uvM + float2(tM, 0)).r - ReliefMid.Sample(ReliefMidSampler, uvM - float2(tM, 0)).r) * 2.0 * Range / (2.0 * tM * MB.z);
float hyM = (ReliefMid.Sample(ReliefMidSampler, uvM + float2(0, tM)).r - ReliefMid.Sample(ReliefMidSampler, uvM - float2(0, tM)).r) * 2.0 * Range / (2.0 * tM * MB.z);
float2 g = (float2(hxD, hyD) * wD + float2(hxM, hyM) * wM) / wn;
// a garra fina da esteira (o caderno de 4,7 cm, R = a fita com as garras): ~0,5 cm de relevo, só no detalhe
float tT = Texel * 0.5;
float gx = (Track.Sample(TrackSampler, uvD + float2(tT, 0)).r - Track.Sample(TrackSampler, uvD - float2(tT, 0)).r);
float gy = (Track.Sample(TrackSampler, uvD + float2(0, tT)).r - Track.Sample(TrackSampler, uvD - float2(0, tT)).r);
// (13/10, combinado com o jogo) o relevo do caderno leva só a base do sulco; a garra é daqui: +45% do sulco de 2,2 cm
// a partir do R do GroundTrack (4,7 cm, a garra só no miolo da fita), sumindo com a idade
float grouserCm = 2.2 * 0.45 * (1.0 - 0.8 * Rr.g);
float fadeG = 1.0 - smoothstep(5.7, 8.5, fw);                  // a listra da garra até a Nyquist; depois some (média)
g += float2(gx, gy) * (-grouserCm) / (2.0 * cmD * 0.5) * wD * saturate((-h - 0.6) / 1.0) * fadeG;
// a cratera e o queimado (13/10, o alvo street_battle_usa_china_gemini; o formato final do jogo): a altura numa textura
// própria em cm (GroundCrater/GroundCraterMid, R16F), e o GroundTrack G = o escuro (o halo revirado, a fuligem, o
// combustível), B = o calor (a brasa), A = o claro (os torrões, a borda, os raios de areia)
float hc = 0.0, ej = 0.0, dark = 0.0, heat = 0.0, hideW = 0.0;
if (UseCrater > 0.5) {
    float hcD = Crater.Sample(CraterSampler, uvD).r, hcM = CraterMid.Sample(CraterMidSampler, uvM).r;
    hc = (hcD * wD + hcM * wMc) / wc;
    float cxD = (Crater.Sample(CraterSampler, uvD + float2(tD, 0)).r - Crater.Sample(CraterSampler, uvD - float2(tD, 0)).r) / (2.0 * tD * DB.z);
    float cyD = (Crater.Sample(CraterSampler, uvD + float2(0, tD)).r - Crater.Sample(CraterSampler, uvD - float2(0, tD)).r) / (2.0 * tD * DB.z);
    float cxM = (CraterMid.Sample(CraterMidSampler, uvM + float2(tM, 0)).r - CraterMid.Sample(CraterMidSampler, uvM - float2(tM, 0)).r) / (2.0 * tM * MB.z);
    float cyM = (CraterMid.Sample(CraterMidSampler, uvM + float2(0, tM)).r - CraterMid.Sample(CraterMidSampler, uvM - float2(0, tM)).r) / (2.0 * tM * MB.z);
    float2 gC = (float2(cxD, cyD) * wD + float2(cxM, cyM) * wM) / wn;
    // (14/10) sem relevo de verdade, a parede de dentro virada pro sol (a de perto da câmera) aparecia inteira e acesa: numa
    // cratera real a borda a esconde da câmera baixa. Onde a normal da cratera foge da câmera, achata e vira fundo da bacia
    float facing = dot(normalize(float3(-gC.x, -gC.y, 1.0)), Parameters.CameraVector);
    hideW = 1.0 - smoothstep(0.05, 0.2, facing);
    g += gC * (1.0 - hideW);
    // o escuro, o calor e o claro: o caderno de detalhe perto e o do meio (GroundTrackMid, 19 cm) na câmera do jogo
    float4 Tk = (Track.Sample(TrackSampler, uvD) * wD + TrackMid.Sample(TrackMidSampler, uvM) * wMc) / wc;
    // (13/10, a cratera v2, combinado com o jogo) MarkRamps = 1: o G e o A vêm em rampa de distância (< 0,5 fora; dentro,
    // 0,5 + 0,5·intensidade): a borda cortada no meio da rampa com antisserrilhado pelo fwidth, nítida em qualquer zoom.
    // MarkRamps = 0: lineares, como antes. O B (calor) é sempre linear.
    if (MarkRamps > 0.5) {
        float covG = saturate((Tk.g - 0.5) / max(fwidth(Tk.g), 1e-4) + 0.5);
        float covA = saturate((Tk.a - 0.5) / max(fwidth(Tk.a), 1e-4) + 0.5);
        dark = covG * saturate((Tk.g - 0.5) * 2.0) * wc;
        ej = covA * smoothstep(0.6, 0.8, saturate((Tk.a - 0.5) * 2.0)) * wc;   // (14/10) só os torrões: o claro largo virava mancha branca
    } else {
        dark = Tk.g * wc; ej = Tk.a * wc;
    }
    heat = Tk.b * wc;
}
float k = w;                                                     // some no longe
nT = normalize(nT + float3(-g.x, -g.y, 0.0) * k);
// a luz e a areia mexida
float age = Rr.g, stir = Rr.a, vis = Rr.b;
float wall = smoothstep(0.08, 0.2, length(g));                  // a parede do sulco/bordinha: o N·L já escurece
bc *= lerp(1.0, vis, 0.85 * k * (1.0 - wall));
if (UseCrater > 0.5) {
    // cada chão quebra de um jeito (13/10, a cratera v2; Surf = o peso da camada de asfalto do WorldForge, 0 = areia/terra):
    // a areia escorre e o claro é areia; o asfalto quebra em placas pretas e o fundo mostra a base de terra e brita
    float surf = saturate(Surf);
    float3 bc0 = bc;                                                              // a areia antes da bacia e do escuro
    // o fundo da bacia (medido na street_battle: o miolo ~0,09 de brilho na tela contra ~0,46 do chão; o mf15 dava 0,23):
    // na areia, terra revirada e úmida bem mais escura; no asfalto, a base de terra e brita à mostra
    float bowl = saturate(-hc / 25.0);
    bc = lerp(bc, lerp(bc * float3(0.5, 0.45, 0.4), float3(0.17, 0.15, 0.12), surf), bowl);
    bc *= lerp(1.0, lerp(1.1, 1.0, surf), saturate(hc / 6.0));                      // a borda levantada: areia clara (no asfalto, a placa)
    float lum = dot(bc, float3(0.3, 0.59, 0.11));
    // o escuro: na areia, terra revirada e fuligem, marrom-carvão e nunca preto chapado (medido 13/10: ~0,15–0,2× em albedo);
    // no asfalto, o asfalto quebrado e queimado, quase preto
    float3 darkC = lerp(lerp(bc, lum.xxx, 0.5) * 0.14, float3(0.03, 0.028, 0.026), surf);
    // (14/10) o manto mosqueado no mundo (o do atlas não chega à câmera do jogo: lá só vale o caderno do meio, 19 cm)
    float2 q1 = p / 60.0, q2 = p / 120.0 + 17.3;
    float2 i1 = floor(q1), f1 = frac(q1); f1 = f1 * f1 * (3.0 - 2.0 * f1);
    float2 i2 = floor(q2), f2 = frac(q2); f2 = f2 * f2 * (3.0 - 2.0 * f2);
    float n1 = lerp(lerp(frac(sin(dot(i1, float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i1 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453), f1.x),
                    lerp(frac(sin(dot(i1 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i1 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453), f1.x), f1.y);
    float n2 = lerp(lerp(frac(sin(dot(i2, float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i2 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453), f2.x),
                    lerp(frac(sin(dot(i2 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i2 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453), f2.x), f2.y);
    float mott = 0.6 * n1 + 0.4 * n2;
    bc = lerp(bc, darkC, dark * 0.95 * (0.55 + 0.45 * mott));
    // a parede escondida vira o fundo da bacia
    bc = lerp(bc, lerp(bc0 * float3(0.5, 0.45, 0.4), float3(0.17, 0.15, 0.12), surf), hideW * saturate(-hc / 8.0));
    // o claro: os torrões e a areia jogada por cima (o +13% de antes lia dois tons, não três): um albedo-alvo, areia clara
    // de baixo na areia e cascalho cinza no asfalto
    // (14/10) o claro = a areia de entrada ×1,15 (o piso fixo 0,55/0,46/0,33 tinha B/R 0,6 contra 0,2 da areia: saía cinza-branco)
    float3 lightC = lerp(bc0 * 1.15, float3(0.40, 0.38, 0.35), surf);
    bc = lerp(bc, lightC, ej * 0.9);
    rough = lerp(rough, 0.95, ej * 0.5);
    Emi = float3(1.0, 0.35, 0.08) * heat * heat * HeatNits;                        // a brasa (some em ~30 s, quem apaga é o jogo)
}
float fresh = band * (1.0 - age) * wc;
bc *= lerp(float3(1.0, 1.0, 1.0), float3(0.84, 0.8, 0.76), fresh * 0.8);   // revolvida fresca: mais escura e quente; seca com a idade
rough = lerp(rough, rough * 0.85, saturate(-h / 2.0) * wD);                  // o fundo compactado mais liso, só de perto
Mask = saturate(fresh + saturate(-hc / 3.0) + ej + dark * 0.5);                            // a ondulação some na esteira fresca e na cratera
Nout = nT; Rout = rough;
return bc;
"""


# O deslocamento do chão (13/10, a cratera do jogo igual à prova, proposta da sessão do jogo: a tesselação Nanite do próprio
# Landscape, sem malha nem buraco). Num Custom separado, ligado ao pino Displacement do material do chão com
# bEnableTessellation e DisplacementScaling (magnitude 200 cm, centro 0,5): roda no estágio da tesselação, então nada de
# ddx/fwidth e só SampleLevel. A altura é a do caderno (a cratera em cm + o sulco da esteira) e, dentro da cratera, o relevo
# fino da terra revirada (a altura da Farm Soil, Poly Haven CC0, em ladrilho de 1,6 m girado contra a repetição).
# Entradas: WP, Relief, ReliefMid, Crater, CraterMid, FarmH (farm_soil_disp, linear), DB, MB, Range, UseCrater, DispOn.
DISP_CODE = r"""
if (DispOn < 0.5 || Range <= 0.0) return 0.5;
float2 p = WP.xy;
float2 uvD = (p - DB.xy) / max(DB.zw, 1.0);
float2 uvM = (p - MB.xy) / max(MB.zw, 1.0);
float inD = step(0.0, uvD.x) * step(uvD.x, 1.0) * step(0.0, uvD.y) * step(uvD.y, 1.0);
float inM = step(0.0, uvM.x) * step(uvM.x, 1.0) * step(0.0, uvM.y) * step(uvM.y, 1.0);
// a borda dos cadernos: a altura vai a zero nos últimos 4% (sem degrau quando o caderno anda)
float2 eD = min(uvD, 1.0 - uvD), eM = min(uvM, 1.0 - uvM);
float fD = inD * saturate(min(eD.x, eD.y) / 0.04), fM = inM * saturate(min(eM.x, eM.y) / 0.04);
float wD = fD, wM = fM * (1.0 - fD);
float w = wD + wM;
if (w <= 0.001) return 0.5;
float hRel = ((Relief.SampleLevel(ReliefSampler, uvD, 0.0).r - 0.5) * wD + (ReliefMid.SampleLevel(ReliefMidSampler, uvM, 0.0).r - 0.5) * wM)
             * 2.0 * Range / w;                                                        // o sulco da esteira (cm)
float hc = 0.0;
if (UseCrater > 0.5)
    hc = (Crater.SampleLevel(CraterSampler, uvD, 0.0).r * wD + CraterMid.SampleLevel(CraterMidSampler, uvM, 0.0).r * wM) / w;   // cm
// a terra revirada dentro da cratera: ±2 cm, onde o caderno da cratera mexeu (a bacia, a borda, o manto)
float churn = saturate(abs(hc) / 4.0);
float2 q = p * 0.01 / 1.6;
float2 q2 = float2(0.8 * q.x - 0.6 * q.y, 0.6 * q.x + 0.8 * q.y) * 1.37 + 0.41;
float fh = lerp(FarmH.SampleLevel(FarmHSampler, q, 0.0).r, FarmH.SampleLevel(FarmHSampler, q2, 0.0).r, 0.5);
float hFine = (fh - 0.5) * 4.0 * churn;
float h = (hRel + hc + hFine) * w;                                                     // cm; some fora dos cadernos
return saturate(0.5 + h / 200.0);
"""


def main():
    path = f'{FOLDER}/{NAME}'
    # (o registro do commandlet nem sempre conhece o conteúdo do plugin: reaproveita a função e limpa os nós)
    mf = unreal.load_asset(path)
    if mf is not None:
        mel.delete_all_material_expressions_in_function(mf)
    else:
        mf = tools.create_asset(NAME, FOLDER, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    mf.set_editor_property('description', 'O chão com memória (RTSLook): lê os cadernos de relevo do jogo e muda cor, normal e rugosidade.')
    mf.set_editor_property('expose_to_library', True)
    E = lambda cls, x, y: mel.create_material_expression_in_function(mf, cls, x, y)

    def fin(name, typ, y, prio):
        e = E(unreal.MaterialExpressionFunctionInput, -1400, y)
        e.set_editor_property('input_name', name)
        e.set_editor_property('input_type', typ)
        e.set_editor_property('sort_priority', prio)
        return e

    bc_in = fin('BaseColor', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3, -400, 0)
    n_in = fin('Normal', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3, -250, 1)
    r_in = fin('Roughness', unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -100, 2)
    s_in = fin('Surface', unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, 50, 3)      # 0 areia/terra, 1 asfalto (o peso da camada)
    s_in.set_editor_property('use_preview_value_as_default', True)
    c = E(unreal.MaterialExpressionCustom, -600, 0)
    c.set_editor_property('code', CODE)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('description', 'RTSLook ground memory')
    names = ['WP', 'BC', 'N', 'Rg', 'Surf', 'Relief', 'ReliefMid', 'Track', 'TrackMid', 'Crater', 'CraterMid', 'DB', 'MB', 'Range', 'Texel', 'UseCrater', 'HeatNits', 'MarkRamps']
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Nout', unreal.CustomMaterialOutputType.CMOT_FLOAT3), ('Rout', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                 ('Mask', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('Emi', unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    c.set_editor_property('additional_outputs', outs)
    wp = E(unreal.MaterialExpressionWorldPosition, -1100, 100)
    mel.connect_material_expressions(wp, '', c, 'WP')
    mel.connect_material_expressions(bc_in, '', c, 'BC')
    mel.connect_material_expressions(n_in, '', c, 'N')
    mel.connect_material_expressions(r_in, '', c, 'Rg')
    mel.connect_material_expressions(s_in, '', c, 'Surf')
    black = unreal.load_asset('/Engine/EngineResources/Black')
    y = 200
    for pname, pin in (('GroundRelief', 'Relief'), ('GroundReliefMid', 'ReliefMid'), ('GroundTrack', 'Track'), ('GroundTrackMid', 'TrackMid'),
                       ('GroundCrater', 'Crater'), ('GroundCraterMid', 'CraterMid')):
        t = E(unreal.MaterialExpressionTextureObjectParameter, -1100, y)
        t.set_editor_property('parameter_name', pname)
        if black:
            t.set_editor_property('texture', black)
        t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        t.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)   # (o Landscape tem limite de 16 amostradores)
        mel.connect_material_expressions(t, '', c, pin)
        y += 120
    for pname, pin in (('DetailBounds', 'DB'), ('MidBounds', 'MB')):
        v = E(unreal.MaterialExpressionVectorParameter, -1100, y)
        v.set_editor_property('parameter_name', pname)
        v.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
        # (13/10: a saída '' do VectorParameter entrega só o RGB (float3) e o .zw não compilava no SM6, o chão do jogo saiu
        #  xadrez; a 'RGBA' entrega os quatro)
        ok = mel.connect_material_expressions(v, 'RGBA', c, pin)
        if not ok:      # (sem a saída 'RGBA' nesta versão: junta o RGB com o A)
            ap = E(unreal.MaterialExpressionAppendVector, -900, y)
            mel.connect_material_expressions(v, '', ap, 'A')
            mel.connect_material_expressions(v, 'A', ap, 'B')
            ok = mel.connect_material_expressions(ap, '', c, pin)
            unreal.log(f'LOOKMF {pname}: RGB + A pelo AppendVector ({ok})')
        else:
            unreal.log(f'LOOKMF {pname}: saída RGBA ligada')
        y += 120
    for pname, pin, dv in (('ReliefRangeCm', 'Range', 0.0), ('ReliefTexel', 'Texel', 1.0 / 2048.0), ('UseCrater', 'UseCrater', 0.0),
                           ('HeatNits', 'HeatNits', 3000.0), ('MarkRamps', 'MarkRamps', 0.0)):
        sp = E(unreal.MaterialExpressionScalarParameter, -1100, y)
        sp.set_editor_property('parameter_name', pname)
        sp.set_editor_property('default_value', dv)
        mel.connect_material_expressions(sp, '', c, pin)
        y += 100

    def fout(name, src, pin, yy, prio):
        o = E(unreal.MaterialExpressionFunctionOutput, -200, yy)
        o.set_editor_property('output_name', name)
        o.set_editor_property('sort_priority', prio)
        mel.connect_material_expressions(src, pin, o, '')
    fout('BaseColor', c, '', -200, 0)
    fout('Normal', c, 'Nout', -50, 1)
    fout('Roughness', c, 'Rout', 100, 2)
    fout('RippleMask', c, 'Mask', 250, 3)
    fout('Emissive', c, 'Emi', 400, 4)
    # (o deslocamento, DISP_CODE, vai só inline pelo WorldForge: a função do plugin não é chamada no chão hoje)
    mel.update_material_function(mf)
    eal.save_loaded_asset(mf)
    unreal.log(f'LOOKMF {path} pronta')


main()
