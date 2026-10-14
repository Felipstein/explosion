"""A maquete do look (LookLab): a arte day_game_cam montada dentro do jogo.

Rodar: Game/Tools/look.ps1 build   (Unreal sem janela; não usa a placa)

O que monta, tudo em /Game/LookLab (fora do git, refeito por este script):
- o chão: um material só, com o cruzamento desenhado pela posição no mundo
  (asfalto rachado, areia, faixas pintadas, areia soprada nas bordas). A receita
  do asfalto e da areia é a do deserto do WorldForge (build_slice.py, 'desert'),
  pra o "antes" ter o chão de hoje do jogo;
- cinco tanques USA na posição dos blindados da arte (o jogo ainda não tem os
  blindados de rodas, as picapes, o tanque da China nem as casas);
- palmeiras, mato seco e um poste nos lugares da arte;
- as câmeras: a da arte (medida pela perspectiva: 29° de inclinação, 65° de
  campo horizontal, 21 m de altura, giro de 45° em relação às ruas) e a do
  jogo (37,5°, 50°, 61,1 m e 31,6 m, Game/Config/DefaultGame.ini), todas
  olhando o mesmo ponto do chão;
- um mapa por luz:
    Before  = a luz do mapa do jogo hoje (World_TestSalt2, lida do mapa em
              10/10), com o sol na mesma posição em relação à câmera do jogo;
    ArtSun  = a mesma luz com o sol só girado pra posição da arte (contraluz
              45° à esquerda da câmera).
- as sequências e a configuração do Movie Render Queue pro look.ps1 render.

A planta (metros, mundo da Unreal: X ao longo da rua dos tanques, Y à direita)
saiu da arte desentortada pra vista de cima com a câmera medida.
"""
import glob
import math
import os

import unreal

ROOT = '/Game/LookLab'
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT_DIR = os.path.join(REPO, 'captures', 'look', '{sequence_name}').replace('\\', '/')
RES = (2048, 1144)            # 2× a arte (1024×572): mesma proporção
FPS = 30

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(*a):
    unreal.log('LOOK ' + ' '.join(str(x) for x in a))


# ---- a planta (metros): Tools/look_layout.py (lida também pelas casas e pelo chão) --------
import sys  # noqa: E402
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_scene  # noqa: E402
from look_scene import (HOUSES as HOUSE_DATA, YARDS, WALLS, TANKS, PALMS, SHRUBS, POLES,  # noqa: E402
                        SCATTER, SCATTER_MI, EXPLOSIONS, CAM as ART_CAM, SUB, SUFFIX, WRECKS, LIGHTS)
# a cena (LOOK_SCENE, Tools/look_scene.py): o deserto dourado é a primeira maquete
SCENE = look_scene.SCENE
MAP_PREFIX = getattr(look_scene, 'MAP_PREFIX', 'LL_DesertDay')

ART_FOCUS_M = ART_CAM['z'] / math.sin(math.radians(-ART_CAM['pitch']))   # da câmera da arte ao ponto que ela olha

SHRUB_MI = {
    'searsia_lucida': 'MI_searsia_lucida_desert',
    'wild_rooibos_bush': 'MI_wild_rooibos_bush_desert',
    'dry_branches_medium_01': 'MI_dry_branches_medium_01_desert',
    'namaqualand_stones_01': 'MI_namaqualand_stones_01_desert',
}

# as casas e os quintais (Tools/look_houses.py gera as malhas; a planta está no look_layout)
HOUSES = [(n, d['x'], d['y'], d.get('yaw', 0.0)) for n, d in list(HOUSE_DATA.items()) + list(YARDS.items())]
HOUSE_GLB = os.path.join(REPO, 'captures', 'look', SUB, 'houses_glb')
GROUND_DIR = os.path.join(REPO, 'captures', 'look', SUB, 'ground')
TEX_DIR = os.path.join(REPO, 'captures', 'look', 'textures')

UNIT = '/Game/Units/USA_MBT'
TANK_PARTS = (
    [f'{UNIT}/SM_USA_MBT_Hull/StaticMeshes/SM_USA_MBT_Hull',
     f'{UNIT}/SM_USA_MBT_Turret/StaticMeshes/SM_USA_MBT_Turret',
     f'{UNIT}/SM_USA_MBT_Barrel/StaticMeshes/SM_USA_MBT_Barrel',
     f'{UNIT}/SM_USA_MBT_Tracks/StaticMeshes/track_l',
     f'{UNIT}/SM_USA_MBT_Tracks/StaticMeshes/track_r']
    + [f'{UNIT}/SM_USA_MBT_Wheels/StaticMeshes/wheel_{i:03d}' for i in range(1, 19)])

# ---- a luz: receitas por ambiente (Tools/look_recipes/<nome>.json) -------------------
# Cada receita é um ambiente com hora fixa (deserto dourado, noite, neve, floresta...): sol,
# atmosfera, luz do céu, névoa e cor. A mesma receita vai pro plugin comum (jogo, WorldForge
# "ver na versão final" e BlastLab). O sol é dado em relação à câmera inicial do mapa:
# "from_camera" = graus à esquerda da frente da câmera (o sol fica fixo no mundo depois).
RECIPES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'look_recipes')
# mapa da maquete -> receita (o nome da receita = a pasta das cenas, docs/concept/scenes/<nome>/)
VARIANTS = getattr(look_scene, 'VARIANTS', {'Before': 'game_today', 'Golden': 'desert_golden_hour'})


def recipe(name):
    import json
    with open(os.path.join(RECIPES, name + '.json'), encoding='utf-8') as f:
        return json.load(f)



# ---- utilidades ----------------------------------------------------------------

def load(path):
    a = unreal.load_asset(path)
    if not a:
        unreal.log_error('LOOK falta ' + path)
    return a


def fresh(name, folder, cls, factory):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    a = asset_tools.create_asset(name, folder, cls, factory)
    if cls is unreal.Material:
        # as malhas da maquete são Nanite: sem a marca o -game desenha o material padrão
        a.set_editor_property('used_with_nanite', True)
    return a


def sampler_for(tex):
    cs = tex.get_editor_property('compression_settings')
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if tex.get_editor_property('srgb'):
        return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


# ---- o material do chão ----------------------------------------------------------

SURF = '/Game/Slice/Surfaces'
TEX = {
    'SandD': f'{SURF}/sandy_gravel_02/T_sandy_gravel_02_diff_4k',
    'SandN': f'{SURF}/sandy_gravel_02/T_sandy_gravel_02_nor_dx_4k',
    'SandM': f'{SURF}/sandy_gravel_02/T_sandy_gravel_02_arm_4k',
    'BeachD': f'{SURF}/aerial_beach_01/T_aerial_beach_01_diff_4k',
    'AsphD': f'{SURF}/road_damaged_2/T_road_damaged_2_diff_4k',
    'AsphN': f'{SURF}/road_damaged_2/T_road_damaged_2_nor_dx_4k',
    'AsphM': f'{SURF}/road_damaged_2/T_road_damaged_2_arm_4k',
    'AerAsphD': f'{SURF}/aerial_asphalt_01/T_aerial_asphalt_01_diff_4k',
    'GravD': f'{SURF}/gravelly_sand/T_gravelly_sand_diff_4k',        # a areia fina pisada (o caminho novo, 12/10)
    'GravN': f'{SURF}/gravelly_sand/T_gravelly_sand_nor_dx_4k',
    'BeachN': f'{SURF}/aerial_beach_01/T_aerial_beach_01_nor_dx_4k',   # os montinhos e as marcas de pisada (a foto aérea em 6 m)
    'GravM': f'{SURF}/gravelly_sand/T_gravelly_sand_arm_4k',          # a oclusão das frestinhas entre os grãos
}


def import_texture(path, folder, kind):
    """PNG gerado -> textura (reimporta só se o PNG mudou). kind: masks (RGBA linear, BC7) ou normal."""
    name = os.path.splitext(os.path.basename(path))[0]
    asset = f'{folder}/{name}'
    pkg = os.path.join(REPO, 'Game', 'Content', folder[len('/Game/'):].replace('/', os.sep), name + '.uasset')
    if eal.does_asset_exist(asset) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(path):
        return load(asset)
    t = unreal.AssetImportTask()
    for k, v in (('filename', path), ('destination_path', folder), ('destination_name', name),
                 ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    asset_tools.import_asset_tasks([t])
    tex = load(asset)
    if kind == 'normal':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    else:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property('srgb', False)
    eal.save_loaded_asset(tex)
    log('textura', name)
    return tex


# Tudo numa conta só (HLSL). A planta vem pintada (T_LL_Layout, look_ground.py): R asfalto,
# G areia por cima, B estrada de terra, A faixa pintada; fora dela é areia. O relevo é a
# malha (SM_LL_Ground); aqui só o detalhe fino.
#   areia   = sandy_gravel_02 a 2,5 m (+ 9,25 m, 35%) com a foto aérea de praia a 45 m por cima
#             (a receita do deserto do WorldForge, build_slice.py);
#   asfalto = road_damaged_2 a 2,2 m com a foto aérea a 30 m, o pó quente do deserto por cima
#             e o asfalto rachado das artes (T_LL_Cracks, look_textures.py: placas de 1–2 m,
#             jacaré em manchas, areia nas frestas e em faixas de vento) em duas escalas
#             giradas, trocando por manchas de 40 m (a repetição de 12 m não aparece);
#   terra   = a areia batida, mais escura.
GROUND_CODE = r"""
float2 p = WP.xy * 0.01;
float2 luv = (p - float2(LX, LY)) / LS;
float LANEX = (abs(p.x) > 12.0) ? 1.0 : 0.0;        // fora do cruzamento a rua corre em X; no meio, em Y
float4 L = Layout.Sample(LayoutSampler, luv);
float sandOcc = 1.0;   // a oclusão das frestas da areia (SANDAO)
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
    float4 Z = (ZON > 0.5) ? Zones.Sample(ZonesSampler, luv) : float4(0, 0, 0, 0);   // liso, ondulado, pedregoso, revirado
    float sOcc = lerp(1.0, gao * gao, 0.7 * (1.0 - 0.6 * Z.r) + 0.3 * Z.b);           // o liso tem menos fresta; o pedregoso mais
    // 13/10 (a cartela de cores): a fresta é oclusão, não cor. Escurecendo o albedo ela tirava ~35% da areia ao sol e a
    // exposição subia pra compensar (o resto estourava). SANDAO 1: vai pra saída AO (só a luz do céu escurece, a regra PBR)
    sand *= lerp(sOcc, 1.0, SANDAO);
    sandOcc = sOcc;
    sand *= lerp(1.0, 1.03, Z.r);                                                     // o compactado mais claro (pouco: a diferença é no grão, não em mancha)
    sand *= lerp(float3(1, 1, 1), float3(0.96, 0.95, 0.93), Z.b);                     // o pedregoso um pouco mais escuro e frio
    sand *= lerp(float3(1, 1, 1), float3(0.94, 0.92, 0.89), Z.a);                     // a areia revirada (solta, um pouco úmida)
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
    if (ZON > 0.5) {
        float4 Zn = Zones.Sample(ZonesSampler, luv);
        sNxy = n1 * lerp(1.0, 0.35, Zn.r) * lerp(1.0, 1.4, Zn.b) + (n2 * lerp(0.25, 0.9, rp) + n3 * 0.45) * lerp(1.0, 0.25, Zn.r);
    }
}
// as ondulações SIMULADAS (look_ripples.py, Nishimori & Ouchi 1993; 2ª opinião de 12/10): o ladrilho de 10 m alinhado
// ao vento do mapa, embaralhado em hexágonos sem girar (Mikkelsen 2022: 3 amostras com deslocamento aleatório por
// vértice, pesos com contraste) pra não repetir na tela; só em manchas (o vento expõe uma parte), com força que varia,
// e sumindo quando a onda fica menor que ~4 px (o tamanho do pixel no chão), pra não cintilar.
if (RIPTEX > 0.0) {
    float cw = cos(WINDYAW), sw = sin(WINDYAW);
    float2 q = float2(cw * p.x + sw * p.y, -sw * p.x + cw * p.y) / RTILE;        // o mundo no espaço do vento
    float2 dqx = ddx(q), dqy = ddy(q);
    float2 sk = float2(q.x + q.y * 0.57735, q.y * 1.1547) * 2.2;                 // a grade triangular (~4,5 m por hexágono)
    float2 bs = floor(sk), fr = frac(sk);
    float2 v1, v2, v3; float3 w;
    if (fr.x + fr.y < 1.0) { v1 = bs; v2 = bs + float2(1, 0); v3 = bs + float2(0, 1); w = float3(1.0 - fr.x - fr.y, fr.x, fr.y); }
    else { v1 = bs + float2(1, 1); v2 = bs + float2(0, 1); v3 = bs + float2(1, 0); w = float3(fr.x + fr.y - 1.0, 1.0 - fr.x, 1.0 - fr.y); }
    w = w * w * w; w /= (w.x + w.y + w.z);
    float2 o1 = frac(sin(float2(dot(v1, float2(127.1, 311.7)), dot(v1, float2(269.5, 183.3)))) * 43758.5453);
    float2 o2 = frac(sin(float2(dot(v2, float2(127.1, 311.7)), dot(v2, float2(269.5, 183.3)))) * 43758.5453);
    float2 o3 = frac(sin(float2(dot(v3, float2(127.1, 311.7)), dot(v3, float2(269.5, 183.3)))) * 43758.5453);
    float4 R1 = Ripples.SampleGrad(RipplesSampler, q + o1, dqx, dqy);
    float4 R2 = Ripples.SampleGrad(RipplesSampler, q + o2, dqx, dqy);
    float4 R3 = Ripples.SampleGrad(RipplesSampler, q + o3, dqx, dqy);
    float4 Rp = R1 * w.x + R2 * w.y + R3 * w.z;
    float2 tn = Rp.gb * 2.0 - 1.0;
    tn.y = -tn.y;                                                                  // (a imagem desce em +Y)
    float2 wn = float2(cw * tn.x - sw * tn.y, sw * tn.x + cw * tn.y);              // de volta pro mundo
    // as manchas: onde o vento expõe a areia (ruído grande) e a força varia de lugar pra lugar
    float m1 = saturate((Beach.Sample(BeachSampler, p / 34.0 + 0.41).g - 0.225) / 0.03);
    float m2 = saturate((Beach.Sample(BeachSampler, p / 9.0 + 0.77).r - 0.262) / 0.04);
    float rmaskT = smoothstep(0.25, 0.6, m1) * lerp(0.45, 1.0, m2);
    if (ZON > 0.5) rmaskT = Zones.Sample(ZonesSampler, luv).g * lerp(0.6, 1.0, m2);   // a ondulação só nas zonas dela (raras)
    float fw = max(length(fwidth(p)), 1e-4);                                       // metros por pixel no chão
    float fadeT = 1.0 - smoothstep(0.03, 0.06, fw);
    float kT = rmaskT * fadeT * RIPTEX;
    if (TRK > 0.5) kT *= 1.0 - Tracks.SampleLevel(TracksSampler, luv, 0).a;      // a esteira fresca apagou a ondulação
    sNxy += wn / 0.8 * 2.2 * kT;                                                   // (a normal do chão é vn + nxy·0,8; ×2,2: a escala real)
    sand *= lerp(1.0, Rp.a, kT);                                                    // a sombra das cristas, assada
}
// a trilha da esteira (look_ground.track_relief): o relevo raso assado (a normal e a sombra do sol) e a areia revirada
// fresca um pouco mais escura e quente (a umidade de baixo; seca com a idade). Lê pela luz, não pela profundidade.
if (TRK > 0.5) {
    float4 Tk = Tracks.Sample(TracksSampler, luv);
    float2 tkn = Tk.rg * 2.0 - 1.0;
    sNxy += tkn / 0.8;
    sand *= lerp(1.0, Tk.b, 0.85);
    sand *= lerp(float3(1.0, 1.0, 1.0), float3(0.84, 0.8, 0.76), Tk.a * 0.8);
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
// (13/10, o Felipe: "as marcas de pneu viram um PADRÃO na estrada toda") a foto aérea é um estacionamento com arcos de pneu e
// uma trinca reta; repetida a cada 30 m, os mesmos arcos voltam ao longo da rua. AERBLUR = 1: só a mancha de cor dela (o mip
// de ~1 m, os arcos somem), em duas amostras giradas e de escalas diferentes (a repetição se desfaz). O detalhe fino vem da
// foto de perto e dos defeitos, que não mudam.
if (AERBLUR > 0.5) {
    float2 pa2 = float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y);
    aFar = lerp(AerAsph.SampleLevel(AerAsphSampler, p / 30.0 + 0.21, 7.0).rgb,
                AerAsph.SampleLevel(AerAsphSampler, pa2 / 47.0 + 0.63, 7.0).rgb, 0.5) * 0.9;
}
asph = lerp(asph, aFar, 0.45 + 0.3 * saturate((Depth - 1000.0) / 14000.0)) * macro * 1.72 * float3(AGR, AGG, AGB);   // o ganho depois da foto aérea (medido 11/10); × o ajuste da cena
float2 aNxy = AsphN.Sample(AsphNSampler, p / 2.2).rg * 2.0 - 1.0;
float3 aM = AsphM.Sample(AsphMSampler, p / 2.2).rgb;
float3 dust = float3(0.30, 0.20, 0.12);
asph = lerp(asph, dust * 1.3, 0.10);                       // o pó quente do deserto em tudo
// a película de areia por cima (volta 15, o Felipe: "o gameshot está mais sujo de areia, mais claro"), em manchas largas
// (13/10: lida da foto de praia em tamanho normal, os pedregulhos dela viravam bolhas de 20–30 cm, "uma onça no chão";
//  agora só a mancha larga, num mip borrado: manchas de 2–5 m)
float film = 0.12 + 0.16 * smoothstep(0.228, 0.246, Beach.SampleLevel(BeachSampler, p / 14.0 + 0.71, 8.0).g);
asph = lerp(asph, sand * 0.92, film);

// as rachaduras: duas amostras (12 m e 16,4 m girada 37°), trocando por manchas
float2 pr = float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 16.4 + 0.37;
float4 C1 = Cracks.Sample(CracksSampler, p / 12.0);
float4 C2 = Cracks.Sample(CracksSampler, pr);
// (volta 15: a fresta tinha ~2 cm e sumia no pixel de 5–8 cm da câmera do jogo; nas favoritas ela lê como linha de ~10 cm:
//  engrossada ~3,5 cm pra cada lado pelo máximo de 4 vizinhos)
float dl1 = 0.035 / 12.0, dl2 = 0.035 / 16.4;
C1.r = max(C1.r, max(max(Cracks.Sample(CracksSampler, p / 12.0 + float2(dl1, 0)).r, Cracks.Sample(CracksSampler, p / 12.0 - float2(dl1, 0)).r),
                     max(Cracks.Sample(CracksSampler, p / 12.0 + float2(0, dl1)).r, Cracks.Sample(CracksSampler, p / 12.0 - float2(0, dl1)).r)));
C2.r = max(C2.r, max(max(Cracks.Sample(CracksSampler, pr + float2(dl2, 0)).r, Cracks.Sample(CracksSampler, pr - float2(dl2, 0)).r),
                     max(Cracks.Sample(CracksSampler, pr + float2(0, dl2)).r, Cracks.Sample(CracksSampler, pr - float2(0, dl2)).r)));
float sel = smoothstep(0.228, 0.244, Beach.SampleLevel(BeachSampler, p / 40.0 + 0.19, 8.0).g);   // (13/10: mip borrado, sem trocar em bolhas)   // G vai de ~0,22 a ~0,26
float4 C = lerp(C1, C2, sel);
// trechos muito rachados e trechos quase lisos (manchas de ~60 m e ~20 m), como nas artes
float zone = saturate((Beach.SampleLevel(BeachSampler, p / 60.0 + 0.61, 8.0).r - 0.262) / 0.04) * 0.7
           + saturate((Beach.SampleLevel(BeachSampler, p / 20.0 + 0.27, 7.0).g - 0.22) / 0.035) * 0.3;
float crackAmt = lerp(0.15, 0.9, smoothstep(0.35, 0.8, zone));   // (13/10: as favoritas mostram o asfalto rachado de longe)   // rara: a arte tem trechos lisos e a fresta só aqui e ali (crítica 11/10)
C.r *= crackAmt;
C.g *= lerp(0.5, 1.0, crackAmt);
float2 n1 = CracksN.Sample(CracksNSampler, p / 12.0).rg * 2.0 - 1.0;
float2 t2 = CracksN.Sample(CracksNSampler, pr).rg * 2.0 - 1.0;
float2 n2 = float2(0.8 * t2.x + 0.6 * t2.y, -0.6 * t2.x + 0.8 * t2.y);   // de volta pro mundo
float2 cN = lerp(n1, n2, sel) * crackAmt;
asph *= 0.88 + 0.24 * C.b;                                  // cada placa envelheceu de um jeito
// a vida do asfalto (crítica 11/10): remendos retangulares mais escuros, manchas de óleo e marcas de pneu
// os remendos (13/10: a grade de 3,5 m com 18% das células aparecia de longe como quadriculado): células de 9 m deslocadas
// por linha, 6% delas com um remendo de tamanho, posição e tom sorteados, alinhado à rua
float2 pc = p / 9.0;
pc.x += floor(pc.y) * 0.37;
float2 cell = floor(pc);
float rnd = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float rnd2 = frac(sin(dot(cell, float2(39.346, 11.135))) * 24634.6345);
float rnd3 = frac(sin(dot(cell, float2(73.156, 52.235))) * 15731.7431);
float2 fp = frac(pc);
float2 pc0 = float2(0.1 + 0.45 * rnd2, 0.15 + 0.4 * rnd3);
float2 psz = float2(0.18 + 0.3 * rnd3, 0.1 + 0.22 * rnd2);
float2 fpl = (LANEX > 0.5) ? fp : fp.yx;
float patchBox = step(0.94, rnd) * step(pc0.x, fpl.x) * step(fpl.x, pc0.x + psz.x) * step(pc0.y, fpl.y) * step(fpl.y, pc0.y + psz.y);
asph *= lerp(1.0, 0.78 + 0.12 * rnd2, patchBox * (1.0 - ASPHV2));   // (com o v2, os remendos são os dele)
float oil = Beach.Sample(BeachSampler, p / 2.3 + 0.83).r;
asph *= lerp(1.0, 0.6, smoothstep(0.293, 0.303, oil) * (1.0 - ASPHV2));
float tireX = Beach.Sample(BeachSampler, float2(p.x / 40.0, p.y / 0.55) + 0.15).g;
float tireY = Beach.Sample(BeachSampler, float2(p.x / 0.55, p.y / 40.0) + 0.65).g;
float tire = max(smoothstep(0.247, 0.256, tireX) * LANEX, smoothstep(0.247, 0.256, tireY) * (1.0 - LANEX));
asph *= lerp(1.0, 0.8, tire);
asph = lerp(asph, asph * 0.22, saturate(C.r * 0.95));   // a fresta: fina (o miolo da textura), não a borda alargada
asph = lerp(asph, dust * 1.2, C.g * 0.6);                   // areia nas frestas e as faixas de vento
float aRough = saturate(aM.g + 0.1 * C.r + 0.1 * C.g);
float2 v2Nxy = float2(0.0, 0.0);
ASPHALT_V2_BLOCK

// --- a faixa pintada ---
float paint;
if (PAINTV2 > 0.5) {
    // (13/10, o estudo nas favoritas: a road_town_battle e a close_game_cam) contínua e nítida, ~13 cm, creme empoeirada
    // (não branco); a trinca passa por cima e a corta, faltam lasquinhas ao longo, uns trechos mais apagados, e a areia que
    // invade a pista a cobre (a mistura da areia vem depois daqui)
    // a idade da tinta muda pela estrada, em trechos de 20–40 m (as favoritas vão da quase nova, a road_town_battle, à quase
    // sumida, a three_factions e a street_battle): a velha descasca em placas de 10–50 cm, ficando só o "fantasma" (o
    // asfalto mais claro onde a tinta esteve), e encarde (mais parda, menos contraste)
    float pline = saturate((L.a - 0.5) / max(fwidth(L.a), 1e-3) + 0.5);
    float life = lerp(0.12, 1.0, saturate((Beach.Sample(BeachSampler, p / 30.0 + 0.47).r - 0.262) / 0.035));
    // (v2: descascava demais e em pedaços curtos, a faixa lia tracejada; agora as falhas são maiores e mais raras: a maior
    //  parte da estrada tem a faixa quase inteira, como a road_town_battle, e só os trechos velhos ficam no fantasma)
    float flakeN = Beach.Sample(BeachSampler, p / 2.4 + 0.13).g;
    float thr = lerp(0.248, 0.205, life);
    float keep = smoothstep(thr - 0.003, thr + 0.003, flakeN);
    float chips = smoothstep(0.229, 0.222, Beach.Sample(BeachSampler, p / 0.6 + 0.71).g);
    float crackCut = 1.0 - saturate(C.r * 1.6);
    paint = pline * keep * (1.0 - 0.9 * chips) * (0.55 + 0.45 * life) * crackCut * (1.0 - 0.7 * C.g);
    float ghost = pline * (1.0 - keep) * 0.18 * crackCut;                     // o fantasma da tinta que saiu
    float3 paintC = lerp(float3(0.56, 0.53, 0.46), sand * 0.95, 0.2 + 0.35 * (1.0 - life));
    asph = lerp(asph, asph * 1.25 + 0.02, ghost);
    asph = lerp(asph, paintC, paint);
} else {
    float wear = smoothstep(0.215, 0.245, Beach.Sample(BeachSampler, p / 1.7 + 0.31).g);
    paint = L.a * wear * (1.0 - C.r) * (1.0 - C.g);
    asph = lerp(asph, float3(0.62, 0.60, 0.55), paint);
}

// --- terra batida ---
float3 dirt = sand * float3(0.63, 0.57, 0.50);

// --- mistura ---
float3 col = sand;
float2 nxy = sNxy;
float r = sM.g, ao = sM.r;
col = lerp(col, asph, L.r);
nxy = lerp(nxy, aNxy * 0.6 + cN + v2Nxy, L.r);
r = lerp(r, lerp(aRough, 0.55, paint), L.r);
ao = lerp(ao, aM.r, L.r);
col = lerp(col, dirt, L.b);
col = lerp(col, sand * 0.97, smoothstep(0.0, 1.0, L.g) * 0.9);   // (volta 11: a areia na pista é a mesma areia, num degradê; era sand × 1,04 de borda dura)
nxy = lerp(nxy, sNxy, L.g);
r = lerp(r, sM.g, L.g);
// --- a cratera v2 (13/10, a prova): a cor do crater() do jogo a 2 cm, com as cores da MF v2 (look_mf.py); o relevo é a
// malha (look_ground.py). A textura tem as crateras lado a lado: R altura, G escuro, B calor, A claro.
if (CRN > 0.5) {
    for (int k = 0; k < 2; k++) {
        if (k >= CRN) break;
        float cx = (k == 0) ? C1X : C2X, cy = (k == 0) ? C1Y : C2Y, ya = (k == 0) ? C1YAW : C2YAW;
        float2 dd = p - float2(cx, cy);
        float2 uvl = float2(dd.x * cos(ya) + dd.y * sin(ya), -dd.x * sin(ya) + dd.y * cos(ya)) / CSPAN + 0.5;
        if (all(uvl > 0.0) && all(uvl < 1.0)) {
            float4 Cr = Crater.Sample(CraterSampler, float2((uvl.x + k) / CRN, uvl.y));
            float hc = (Cr.r - 0.5) * 2.0 * CRANGE * 100.0;         // cm
            float dark = Cr.g, ej = Cr.a;
            float surf = L.r;                                        // 0 areia, 1 asfalto
            float bowl = saturate(-hc / 25.0);
            col = lerp(col, lerp(col * float3(0.5, 0.45, 0.4), float3(0.17, 0.15, 0.12), surf), bowl);
            col *= lerp(1.0, lerp(1.1, 1.0, surf), saturate(hc / 6.0));
            float lum = dot(col, float3(0.3, 0.59, 0.11));
            float3 darkC = lerp(lerp(col, lum.xxx, 0.5) * 0.14, float3(0.03, 0.028, 0.026), surf);
            col = lerp(col, darkC, dark * 0.95);
            float3 lightC = lerp(min(max(col * 1.35, float3(0.55, 0.46, 0.33)), 0.8), float3(0.40, 0.38, 0.35), surf);
            col = lerp(col, lightC, ej * 0.9);
            float crMask = saturate(dark + ej + bowl);
            r = lerp(r, 0.95, crMask);
            nxy *= 1.0 - 0.35 * crMask;                              // a ondulação some na terra revirada; o grão fica (volta 5: 0,7 alisava o fundo)
        }
    }
}
Rough = r;
AO = ao * lerp(1.0, sandOcc, SANDAO * saturate(1.0 - L.r));
// o detalhe por cima da normal da malha (o relevo de verdade)
float3 vn = normalize(VN);
// a face da quebra da laje da estrada (a estrada com volume, 13/10): a parede do degrau é íngreme; o asfalto quebrado
// mostra a brita e a base, mais clara e cinza que o topo (nos game shots a borda lê como um lábio claro esfarelado)
float lip = (1.0 - smoothstep(0.55, 0.85, vn.z)) * saturate(L.r * 2.0);
// (v3: a face clara lia linha branca nas frestas; nas imagens 14 e 12 a quebra é asfalto escuro com sombra, e o claro é
//  a areia em volta)
col = lerp(col, col * float3(0.45, 0.43, 0.41), lip);
Rough = lerp(Rough, 0.95, lip);
Nrm = normalize(vn + float3(nxy, 0.0) * 0.8);
return col;
"""


def asphalt_v2_block():
    """O asfalto v2 do WorldForge (look_asphalt.ASPHALT_V2_CODE, aprovado em 13/10) por cima do asfalto da maquete, num
    escopo próprio: a entrada Base é o asfalto da maquete; o referencial da rua vem do T_LL_RoadFrame."""
    if not getattr(look_scene, 'ASPHALT_V2', False):
        return ''
    import importlib.util as iu
    sp = iu.spec_from_file_location('look_asphalt', os.path.join(os.path.dirname(__file__), 'look_asphalt.py'))
    la = iu.module_from_spec(sp); sp.loader.exec_module(la)
    code = la.ASPHALT_V2_CODE
    for a, b in (('return col;', 'asph = col;'), ('Nout = normalize(N + float3(nxy, 0.0));', 'v2Nxy = nxy;'),
                 ('Rout = saturate(rough);', 'aRough = saturate(rough);')):
        assert a in code, a
        code = code.replace(a, b)
    head = """{
    float4 RFt = RoadFrame.SampleLevel(RoadFrameSampler, luv, 0.0);
    float Across = (RFt.r - 0.5) * 25.6;
    float Width = RFt.g * 32.0;
    float Edge = Width * 0.5 - abs(Across);
    float2 RD = float2(cos(RFt.b * 3.14159265), sin(RFt.b * 3.14159265));
    float2 WindDir = float2(cos(WINDYAW), sin(WINDYAW));
    float3 Base = asph;
    float Rough = aRough;
    float3 N = float3(0.0, 0.0, 1.0);
"""
    return head + code + chr(10) + "}" + chr(10)


def ground_material():
    name, folder = 'M_LL_Ground' + SUFFIX, ROOT + '/Materials'
    tfold = ROOT + '/Textures'
    lay = import_texture(os.path.join(GROUND_DIR, f'T_LL_Layout{SUFFIX}.png'), tfold, 'masks')
    crk = import_texture(os.path.join(TEX_DIR, 'T_LL_Cracks.png'), tfold, 'masks')
    crn = import_texture(os.path.join(TEX_DIR, 'T_LL_Cracks_N.png'), tfold, 'normal')
    rip = import_texture(os.path.join(TEX_DIR, 'T_LL_Ripples.png'), tfold, 'masks')     # as ondulações simuladas (look_ripples.py)
    zon_png = os.path.join(GROUND_DIR, f'T_LL_Zones{SUFFIX}.png')
    zon = import_texture(zon_png, tfold, 'masks') if os.path.exists(zon_png) else load('/Engine/EngineResources/Black')
    trk_png = os.path.join(GROUND_DIR, f'T_LL_Tracks{SUFFIX}.png')
    trk = import_texture(trk_png, tfold, 'masks') if os.path.exists(trk_png) else load('/Engine/EngineResources/Black')
    cr_png = os.path.join(GROUND_DIR, f'T_LL_Crater{SUFFIX}.png')        # a cratera v2 (look_ground.py)
    crt = import_texture(cr_png, tfold, 'masks') if os.path.exists(cr_png) else load('/Engine/EngineResources/Black')
    import json
    G = json.load(open(os.path.join(GROUND_DIR, 'ground.json')))
    m = fresh(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', GROUND_CODE.replace('ASPHALT_V2_BLOCK', asphalt_v2_block()))
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'LookLab ground')
    v2 = bool(getattr(look_scene, 'ASPHALT_V2', False))
    rf_png = os.path.join(GROUND_DIR, f'T_LL_RoadFrame{SUFFIX}.png')
    texs_v2 = [('RoadFrame', import_texture(rf_png, tfold, 'masks')),
               ('OilAtlas', import_texture(os.path.join(TEX_DIR, 'T_LL_Oil.png'), tfold, 'masks'))] if v2 else []
    texs = [('SandD', load(TEX['SandD'])), ('SandN', load(TEX['SandN'])), ('SandM', load(TEX['SandM'])),
            ('Beach', load(TEX['BeachD'])), ('AsphD', load(TEX['AsphD'])), ('AsphN', load(TEX['AsphN'])),
            ('AsphM', load(TEX['AsphM'])), ('AerAsph', load(TEX['AerAsphD'])),
            ('Layout', lay), ('Cracks', crk), ('CracksN', crn),
            ('GravD', load(TEX['GravD'])), ('GravN', load(TEX['GravN'])), ('BeachN', load(TEX['BeachN'])),
            ('GravM', load(TEX['GravM'])), ('Ripples', rip), ('Tracks', trk), ('Zones', zon), ('Crater', crt)] + texs_v2
    sg = getattr(look_scene, 'GROUND_TINT', {}).get('sand', (1.0, 1.0, 1.0))       # o ajuste da areia por cena (a noite: mais clara e cinza)
    ag = getattr(look_scene, 'GROUND_TINT', {}).get('asphalt', (1.0, 1.0, 1.0))
    sa = getattr(look_scene, 'SAND_ALBEDO', None)        # o albedo da areia em linear (o caminho novo, 12/10)
    if sa and os.environ.get('LOOK_SAND_SCALE'):          # o teste da escada de brilhos (13/10): a mesma cor, mais clara
        sa = tuple(v * float(os.environ['LOOK_SAND_SCALE']) for v in sa)
    consts = [('LX', G['x0']), ('LY', G['y0']), ('LS', G['size']), ('SGR', sg[0]), ('SGG', sg[1]), ('SGB', sg[2]),
              ('AGR', ag[0]), ('AGG', ag[1]), ('AGB', ag[2]), ('SALB', 1.0 if sa else 0.0),
              ('SAR', (sa or (0, 0, 0))[0]), ('SAG', (sa or (0, 0, 0))[1]), ('SAB', (sa or (0, 0, 0))[2]),
              ('RIPPLE', float(getattr(look_scene, 'SAND_RIPPLES', 1.0))),
              ('RIPTEX', float(getattr(look_scene, 'SAND_RIPPLE_TEX', 0.0))),
              ('WINDYAW', math.radians(getattr(look_scene, 'WIND_YAW', 0.0))), ('RTILE', 10.0),
              ('TRK', 1.0 if getattr(look_scene, 'TRACK_RELIEF', False) else 0.0),
              ('ZON', 1.0 if getattr(look_scene, 'ZONES', None) is not None else 0.0),
              ('AERBLUR', 1.0 if getattr(look_scene, 'AER_BLUR', False) else 0.0),
              ('PAINTV2', 1.0 if getattr(look_scene, 'ROAD', {}).get('paint_v2', False) else 0.0),
              ('ASPHV2', 1.0 if getattr(look_scene, 'ASPHALT_V2', False) else 0.0), ('OilPuddle', 0.07),
              ('SANDAO', 1.0 if (os.environ.get('LOOK_SAND_AO') or getattr(look_scene, 'SAND_AO', False)) else 0.0)]
    crs = G.get('craters', [])[:2] if os.path.exists(cr_png) else []
    cpad = crs + [dict(x=0.0, y=0.0, yaw=0.0)] * (2 - len(crs))
    consts += [('CRN', float(len(crs))), ('CSPAN', float(crs[0]['span']) if crs else 14.0),
               ('CRANGE', float(crs[0]['range']) if crs else 0.8)]
    for k, c in enumerate(cpad):
        consts += [(f'C{k + 1}X', float(c['x'])), (f'C{k + 1}Y', float(c['y'])), (f'C{k + 1}YAW', math.radians(c.get('yaw', 0.0)))]
    names = ['WP', 'Depth', 'VN'] + [k for k, _ in texs] + [k for k, _ in consts]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Rough', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('AO', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                 ('Nrm', unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)
    y = -900
    for key, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('Depth', unreal.MaterialExpressionPixelDepth),
                     ('VN', unreal.MaterialExpressionVertexNormalWS)):
        e = mel.create_material_expression(m, cls, -700, y)
        mel.connect_material_expressions(e, '', custom, key)
        y += 120
    for key, tex in texs:
        obj = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -700, y)
        obj.set_editor_property('texture', tex)
        obj.set_editor_property('sampler_type', sampler_for(tex))
        mel.connect_material_expressions(obj, '', custom, key)
        y += 120
    for key, value in consts:
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -700, y)
        c.set_editor_property('r', value)
        mel.connect_material_expressions(c, '', custom, key)
        y += 100
    mel.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(custom, 'AO', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(custom, 'Nrm', unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material', name)
    return m


def import_ground():
    """A malha do chão em relevo (look_ground.py), com Nanite; reimporta só se o GLB mudou."""
    folder, name = ROOT + '/Ground', 'SM_LL_Ground' + SUFFIX
    path = os.path.join(GROUND_DIR, name + '.glb')
    asset = f'{folder}/{name}/StaticMeshes/{name}'
    pkg = os.path.join(REPO, 'Game', 'Content', 'LookLab', 'Ground', name, 'StaticMeshes', name + '.uasset')
    if os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(path):
        mesh = load(asset)
        if mesh:
            return mesh
    t = unreal.AssetImportTask()
    for k, v in (('filename', path), ('destination_path', folder), ('destination_name', name),
                 ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    asset_tools.import_asset_tasks([t])
    meshes = [unreal.load_asset(x) for x in t.get_editor_property('imported_object_paths') or []]
    meshes = [x for x in meshes if isinstance(x, unreal.StaticMesh)]
    if not meshes:
        unreal.log_error('LOOK chão sem malha: ' + path)
        return None
    mesh = meshes[0]
    ns = mesh.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', True)
    mesh.set_editor_property('nanite_settings', ns)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    log(f'chão {(b.max.x - b.min.x) / 100:.0f} × {(b.max.y - b.min.y) / 100:.0f} m, altura {b.min.z / 100:.2f}..{b.max.z / 100:.2f} m')
    return mesh


# ---- as casas: materiais, importação e lugar --------------------------------------

# Reboco de adobe: o concreto claro escaneado (asphalt_04) projetado nos três planos do mundo
# (as malhas do Blender não têm UV) e puxado pra cor do adobe da arte (sRGB ~186/160/124),
# com manchas largas, uma variação de tom por casa, terra no pé da parede e pó no que olha
# pra cima (telhado e topo da mureta).
PLASTER_CODE = r"""
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float3 q = WP * 0.01;
float T = 2.4;
float3 c = Conc.Sample(ConcSampler, q.yz / T).rgb * w.x + Conc.Sample(ConcSampler, q.xz / T + 0.31).rgb * w.y
         + Conc.Sample(ConcSampler, q.xy / T + 0.57).rgb * w.z;
float stain = Beach.Sample(BeachSampler, q.yz / 9.0).g * w.x + Beach.Sample(BeachSampler, q.xz / 9.0 + 0.2).g * w.y
            + Beach.Sample(BeachSampler, q.xy / 9.0 + 0.4).g * w.z;
float3 col = c / 0.26 * float3(0.40, 0.31, 0.22);   // reboco cinza-terra (medido: b* 18 na arte)
float hsh = frac(sin(dot(Obj.xy * 0.01, float2(12.9898, 78.233))) * 43758.5453);
col *= lerp(float3(0.93, 0.95, 0.98), float3(1.07, 1.03, 0.97), hsh);
// cada casa um tom (13/10, crítica da arte: "umas caiadas, outras ocre"; todas tinham a cor do chão)
float hsh2 = frac(hsh * 7.31 + 0.17);
col = lerp(col, float3(0.52, 0.45, 0.35), 0.55 * step(0.62, hsh2));          // caiada (creme; o 0,50/0,46/0,40 lia cinza-frio)
col *= lerp(float3(1.0, 1.0, 1.0), float3(1.08, 0.94, 0.74), step(hsh2, 0.3));   // ocre
col *= lerp(0.88, 1.08, saturate((stain - 0.215) / 0.045));
float zf = (WP.z - Obj.z) * 0.01;
float sn = saturate((stain - 0.215) / 0.045);
// a barra de sujeira no pé, borda irregular (volta 12, 2ª crítica da arte: "paredes lisas, sem sujeira subindo da base":
// ~1,5 m e mais escura; era ~1 m e 0,62/0,55/0,47)
float base = 1.0 - smoothstep(0.8 + 0.5 * sn, 1.6 + 0.6 * sn, zf);
col = lerp(col, col * float3(0.55, 0.48, 0.40), 0.9 * base);
float zTop = Bnd.z * 0.01;
// paredes: escorridos de sujeira (ruído esticado na vertical, escorre da mureta) e o reboco
// caído em manchas (aparece o barro mais escuro e avermelhado por baixo)
float wall = 1.0 - smoothstep(0.3, 0.6, abs(n.z));
float2 wq = (w.x > w.y) ? q.yz : q.xz;
float streak = Beach.Sample(BeachSampler, float2(wq.x / 1.1, wq.y / 9.0) + 0.43).r;
streak = smoothstep(0.275, 0.3, streak);
float fromTop = 1.0 - smoothstep(0.3, 2.4, zTop - zf);                       // o escorrido nasce na beira do teto e some descendo
col *= lerp(1.0, 0.6, streak * wall * fromTop);
col *= lerp(1.0, 0.82, wall * (1.0 - smoothstep(0.0, 0.35, zTop - zf)));      // a beira do teto mais escura
float peel = Beach.Sample(BeachSampler, wq / 5.0 + 0.11).g;
peel = smoothstep(0.236, 0.246, peel + 0.004 * (c.r / 0.26 - 1.0));
col = lerp(col, col * float3(0.66, 0.5, 0.38), peel * wall);      // (volta 12: manchas maiores, o barro avermelhado aparecendo)
float up = smoothstep(0.55, 0.9, n.z);
col = lerp(col, float3(0.32, 0.28, 0.23), ROOFDARK * up);
col *= lerp(1.0, 0.78, up * step(zTop - 0.12, zf));                         // o topo da mureta (a quina do teto) mais escuro
float2 nX = ConcN.Sample(ConcNSampler, q.yz / T).rg * 2.0 - 1.0;
float2 nY = ConcN.Sample(ConcNSampler, q.xz / T + 0.31).rg * 2.0 - 1.0;
float2 nZ = ConcN.Sample(ConcNSampler, q.xy / T + 0.57).rg * 2.0 - 1.0;
Nrm = normalize(n + 0.6 * (w.x * float3(0.0, nX.x, nX.y) + w.y * float3(nY.x, 0.0, nY.y) + w.z * float3(nZ.x, nZ.y, 0.0)));
Rough = lerp(0.88, 0.95, peel * wall);
return min(col, 0.85);
"""


def custom_material(name, code, inputs, extra_out):
    folder = ROOT + '/Materials'
    m = fresh(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', code)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for n, t in extra_out:
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)
    y = -600
    for n, src in inputs:
        y += 120
        if isinstance(src, str):
            tex = load(src)
            e = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -700, y)
            e.set_editor_property('texture', tex)
            e.set_editor_property('sampler_type', sampler_for(tex))
        else:
            e = mel.create_material_expression(m, src, -700, y)
        mel.connect_material_expressions(e, '', custom, n)
    return m, custom


def plaster_material():
    F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
    # um reboco por cena (o telhado da noite é mais escuro): o material vai no ator, não na malha
    amt, rc = getattr(look_scene, 'HOUSE_ROOF', (0.35, (0.32, 0.28, 0.23)))
    code = PLASTER_CODE.replace('ROOFDARK', str(amt)).replace('float3(0.32, 0.28, 0.23), ' + str(amt), f'float3({rc[0]}, {rc[1]}, {rc[2]}), {amt}')
    ht = getattr(look_scene, 'HOUSE_TINT', None)
    if ht:     # o tom da cena no reboco (a noite: cinza-azulado), na cor final
        assert '\nreturn min(col, 0.85);' in code      # (o gancho antigo procurava "return col;" e nunca pegava)
        code = code.replace('\nreturn min(col, 0.85);', f'\nreturn min(col * float3({ht[0]}, {ht[1]}, {ht[2]}), 0.85);')
    m, c = custom_material(f'M_LL_Plaster{SUFFIX}', code,
                           [('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                            ('Obj', unreal.MaterialExpressionObjectPositionWS), ('Bnd', unreal.MaterialExpressionObjectBounds),
                            ('Conc', f'{SURF}/asphalt_04/T_asphalt_04_diff_4k'), ('ConcN', f'{SURF}/asphalt_04/T_asphalt_04_nor_dx_4k'),
                            ('Beach', TEX['BeachD'])],
                           [('Rough', F1), ('Nrm', F3)])
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(c, 'Nrm', unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def flat_material(name, color, rough, metal=0.0, two_sided=False):
    m = fresh(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', two_sided)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    c.set_editor_property('constant', unreal.LinearColor(*color, 1.0))
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 150)
    r.set_editor_property('r', rough)
    mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if metal:
        mt = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250)
        mt.set_editor_property('r', metal)
        mel.connect_material_property(mt, '', unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def sand_family(ratio, fallback):
    """A poeira e a sujeira seguem a areia do chão (13/10 volta 33): o albedo da areia da cena × a razão de cada uma (a
    poeira das unidades mais clara e menos saturada que a areia, a fração fina; a sujeira das miudezas um pouco mais
    escura). Sem SAND_ALBEDO na cena, o valor antigo."""
    sa = getattr(look_scene, 'SAND_ALBEDO', None)
    if sa and os.environ.get('LOOK_SAND_SCALE'):
        sa = tuple(v * float(os.environ['LOOK_SAND_SCALE']) for v in sa)
    return [sa[i] * ratio[i] for i in range(3)] if sa else list(fallback)


HOUSE_MATS = {}
PROP_MATS = {}

# O material mestre das miudezas (13/10, o estudo: o que tira o "bloco de computação gráfica" vista do alto é o material,
# não o modelo): a cor do slot, ±12% de valor e um pouco de matiz por peça (pela posição), a sujeira cor de areia subindo
# do chão com a borda quebrada por ruído, e o pó nas faces de cima. Fontes: CoH3 (hierarquia por tom e valor), Klevestav
# (Blizzard: pouco contraste entre materiais), a máscara de sujeira pela altura no mundo.
PROP_CODE = r"""
float3 n = normalize(N);
float hsh = frac(sin(dot(Obj.xy * 0.01, float2(12.9898, 78.233))) * 43758.5453);
float3 col = C * lerp(0.88, 1.12, hsh);
col *= lerp(float3(1.0, 1.0, 1.0), float3(1.04, 1.0, 0.94), frac(hsh * 7.31));
float3 q = WP * 0.01;
float nz = Beach.Sample(BeachSampler, q.xy / 1.7 + q.z * 0.3).g;
float zf = (WP.z - Obj.z) * 0.01;
float dirt = 1.0 - smoothstep(0.04, 0.35 + 6.0 * (nz - 0.237), zf);
float3 dust = DUSTRGB;
col = lerp(col, dust, 0.7 * saturate(dirt));
float up = smoothstep(0.6, 0.95, n.z);
col = lerp(col, dust * 1.15, 0.22 * up);
Rough = lerp(R, 0.95, 0.6 * saturate(dirt + 0.5 * up));
return col;
"""


def prop_material(name, color, rough, metal=0.0, two_sided=False):
    F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    code = PROP_CODE.replace('C * lerp', 'float3({:.4f}, {:.4f}, {:.4f}) * lerp'.format(*color)).replace('lerp(R, 0.95', 'lerp({:.3f}, 0.95'.format(rough))
    code = code.replace('DUSTRGB', 'float3({:.4f}, {:.4f}, {:.4f})'.format(*sand_family((0.816, 0.80, 0.78), (0.40, 0.32, 0.21))))
    m, c = custom_material(name, code,
                           [('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                            ('Obj', unreal.MaterialExpressionObjectPositionWS), ('Beach', TEX['BeachD'])],
                           [('Rough', F1)])
    m.set_editor_property('two_sided', two_sided)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    if metal:
        mt = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 250)
        mt.set_editor_property('r', metal)
        mel.connect_material_property(mt, '', unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def prop_materials():
    """Os materiais das miudezas da vila (o vestir): o mestre com a cor de cada slot; o reboco continua o das casas."""
    if not PROP_MATS:
        PROP_MATS.update({
            'Plaster': HOUSE_MATS.get('Plaster'),
            'Dark': prop_material('M_LL_Prop_Dark', (0.02, 0.018, 0.016), 0.9),
            'Wood': prop_material('M_LL_Prop_Wood', (0.11, 0.065, 0.035), 0.8),
            'Metal': prop_material('M_LL_Prop_Metal', (0.20, 0.19, 0.17), 0.55, 0.6),
            'Cloth': prop_material('M_LL_Prop_Cloth', (0.30, 0.22, 0.13), 0.95, two_sided=True),
            'Rust': prop_material('M_LL_Prop_Rust', (0.14, 0.06, 0.025), 0.85, 0.15),
        })
    return PROP_MATS


def house_materials():
    HOUSE_MATS.update({
        'Plaster': plaster_material(),
        'Dark': flat_material('M_LL_Dark', (0.012, 0.010, 0.009), 0.95),
        'Wood': flat_material('M_LL_Wood', (0.11, 0.065, 0.035), 0.8),
        'Metal': flat_material('M_LL_Metal', (0.20, 0.19, 0.17), 0.55, 0.6),
        'Cloth': flat_material('M_LL_Cloth', (0.30, 0.22, 0.13), 0.95, two_sided=True),
        'Rust': flat_material('M_LL_Rust', (0.14, 0.06, 0.025), 0.85, 0.15),
    })
    return HOUSE_MATS


def import_houses(mats):
    """Os GLB do Blender (look.ps1 houses) -> /Game/LookLab/Buildings, com Nanite e os nossos materiais."""
    folder = ROOT + '/Buildings'
    out = {}
    for f in sorted(glob.glob(os.path.join(HOUSE_GLB, 'SM_LL_*.glb'))):
        name = os.path.splitext(os.path.basename(f))[0]
        # já importada e o GLB não mudou: usa a que existe (a importação com Nanite leva ~1 min por casa)
        pkg = os.path.join(REPO, 'Game', 'Content', 'LookLab', 'Buildings', name, 'StaticMeshes', name + '.uasset')
        if os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(f):
            mesh = unreal.load_asset(f'{folder}/{name}/StaticMeshes/{name}')
            if mesh:
                out[name[len('SM_LL_'):]] = mesh
                continue
        task = unreal.AssetImportTask()
        for k, v in (('filename', f), ('destination_path', folder), ('destination_name', name),
                     ('automated', True), ('replace_existing', True), ('save', False)):
            task.set_editor_property(k, v)
        asset_tools.import_asset_tasks([task])
        meshes = [unreal.load_asset(p) for p in task.get_editor_property('imported_object_paths') or []]
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
        if not meshes:
            unreal.log_error('LOOK casa sem malha: ' + f)
            continue
        mesh = meshes[0]
        ns = mesh.get_editor_property('nanite_settings')
        ns.set_editor_property('enabled', True)
        mesh.set_editor_property('nanite_settings', ns)
        slots = mesh.get_editor_property('static_materials')
        for i, sm in enumerate(slots):
            slot = str(sm.get_editor_property('material_slot_name'))
            key = next((k for k in mats if slot.lower().startswith(k.lower())), None)
            if key:
                mesh.set_material(i, mats[key])
            else:
                log('aviso: slot sem material', name, slot)
        eal.save_loaded_asset(mesh)
        b = mesh.get_bounding_box()
        log(f'casa {name}: {(b.max.x - b.min.x) / 100:.1f} x {(b.max.y - b.min.y) / 100:.1f} x {(b.max.z - b.min.z) / 100:.1f} m, '
            f'{len(slots)} materiais')
        out[name[len('SM_LL_'):]] = mesh
    return out


def place_houses(meshes):
    for name, x, y, yaw in HOUSES:
        m = meshes.get(name)
        if not m:
            continue
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, 0),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(m)
        for i, sm in enumerate(m.get_editor_property('static_materials')):
            slot = str(sm.get_editor_property('material_slot_name'))
            if slot.lower().startswith('plaster'):
                a.static_mesh_component.set_material(i, HOUSE_MATS['Plaster'])
        a.set_actor_label('House_' + name)
        a.set_folder_path('Buildings')
    for name, x, y, yaw, sc in WALLS:
        m = meshes.get(name)
        if not m:
            continue
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, 0),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(m)
        for i, sm in enumerate(m.get_editor_property('static_materials')):
            if str(sm.get_editor_property('material_slot_name')).lower().startswith('plaster'):
                a.static_mesh_component.set_material(i, HOUSE_MATS['Plaster'])
        a.set_actor_scale3d(unreal.Vector(sc, 1.0, 1.0))
        a.set_actor_label('Wall_' + name)
        a.set_folder_path('Buildings')


# ---- o pó do mundo nas unidades (o padrão de material das unidades) ------------------
# A mesma unidade "mora" em cada mapa sem mexer no modelo: o pó assenta no que olha pra cima
# e a terra sobe pela parte de baixo (esteira, saia, rodas). A cor e a quantidade vêm da
# receita do ambiente ("world"): areia no deserto, lama no temperado, neve na neve.
# Os parâmetros do M_UnitPBR do BlastLab (cor, normal, rugosidade, metal) são copiados; o
# material original fica intocado (a maquete só troca nas peças dela).
UNIT_DUST_CODE = r"""
float3 vn = normalize(VN);
if (Camo > 0.5) {      // a camuflagem (China): manchas pela posição no objeto (as malhas não têm UV)
    float3 q = (WP - Obj) * 0.01;
    float c1 = Beach.Sample(BeachSampler, q.xy / 16.0 + q.z * 0.05 + 0.31).r;
    float c2 = Beach.Sample(BeachSampler, q.yx / 11.0 + q.z * 0.07 + 0.77).g;
    Base = lerp(CamoA.rgb, CamoB.rgb, smoothstep(0.276, 0.284, c1));
    Base = lerp(Base, CamoC.rgb, 0.85 * smoothstep(0.240, 0.246, c2));
}
float n = saturate((Beach.Sample(BeachSampler, WP.xy * 0.01 / 1.3 + WP.z * 0.003).g - 0.215) / 0.045);
float up = saturate((vn.z - 0.3) / 0.55);
float top = up * DustTop * lerp(0.45, 1.0, n);
float zl = (WP.z - Obj.z) * 0.01;
float low = (1.0 - smoothstep(0.15, DustH, zl)) * DustLow * lerp(0.55, 1.0, n);
float d = saturate(max(top, low));
Rough = lerp(R, 0.95, d);
Metal = M * (1.0 - d);
return lerp(Base, DustColor.rgb, d);
"""


def unit_dust_parent():
    m = fresh('M_LL_UnitDust', ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_nanite', True)

    def node(cls, x, y, **kw):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in kw.items():
            e.set_editor_property(k, v)
        return e
    bc = node(unreal.MaterialExpressionTextureSampleParameter2D, -1100, -300, parameter_name='BaseColorTexture')
    bf = node(unreal.MaterialExpressionVectorParameter, -1100, -100, parameter_name='BaseColorFactor',
              default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = node(unreal.MaterialExpressionMultiply, -800, -200)
    mel.connect_material_expressions(bc, 'RGB', mul, 'A')
    mel.connect_material_expressions(bf, '', mul, 'B')
    nm = node(unreal.MaterialExpressionTextureSampleParameter2D, -1100, 600, parameter_name='NormalTexture',
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
              texture=unreal.load_asset('/Engine/EngineMaterials/DefaultNormal'))
    mel.connect_material_property(nm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    c = node(unreal.MaterialExpressionCustom, -400, 0, code=UNIT_DUST_CODE,
             output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description='LookLab unit dust')
    ins = []
    for n in ('Base', 'R', 'M', 'VN', 'WP', 'Obj', 'Beach', 'DustColor', 'DustTop', 'DustLow', 'DustH', 'Camo', 'CamoA', 'CamoB', 'CamoC'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n in ('Rough', 'Metal'):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        outs.append(co)
    c.set_editor_property('additional_outputs', outs)
    mel.connect_material_expressions(mul, '', c, 'Base')
    y = 0
    for name, default in (('RoughnessFactor', 0.72), ('MetallicFactor', 0.0), ('DustTop', 0.4), ('DustLow', 0.6), ('DustHeight', 1.2)):
        e = node(unreal.MaterialExpressionScalarParameter, -800, y, parameter_name=name, default_value=default)
        pin = {'RoughnessFactor': 'R', 'MetallicFactor': 'M', 'DustTop': 'DustTop', 'DustLow': 'DustLow', 'DustHeight': 'DustH'}[name]
        mel.connect_material_expressions(e, '', c, pin)
        y += 100
    sp = node(unreal.MaterialExpressionScalarParameter, -400, 300, parameter_name='SpecularFactor', default_value=0.5)
    mel.connect_material_property(sp, '', unreal.MaterialProperty.MP_SPECULAR)
    dc = node(unreal.MaterialExpressionVectorParameter, -800, y, parameter_name='DustColor',
              default_value=unreal.LinearColor(0.5, 0.33, 0.18, 1))
    mel.connect_material_expressions(dc, '', c, 'DustColor')
    y += 100
    cm = node(unreal.MaterialExpressionScalarParameter, -800, y, parameter_name='Camo', default_value=0.0)
    mel.connect_material_expressions(cm, '', c, 'Camo')
    for nm in ('CamoA', 'CamoB', 'CamoC'):
        y += 100
        e = node(unreal.MaterialExpressionVectorParameter, -800, y, parameter_name=nm, default_value=unreal.LinearColor(0.1, 0.1, 0.1, 1))
        mel.connect_material_expressions(e, '', c, nm)
    for pin, cls in (('VN', unreal.MaterialExpressionVertexNormalWS), ('WP', unreal.MaterialExpressionWorldPosition),
                     ('Obj', unreal.MaterialExpressionObjectPositionWS)):
        y += 100
        mel.connect_material_expressions(node(cls, -800, y), '', c, pin)
    y += 100
    bt = node(unreal.MaterialExpressionTextureObject, -800, y, texture=load(TEX['BeachD']))
    bt.set_editor_property('sampler_type', sampler_for(load(TEX['BeachD'])))
    mel.connect_material_expressions(bt, '', c, 'Beach')
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(c, 'Metal', unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


_DUST_PARENT = None

# A carcaça queimada (13/10, medida na carcaça da street_battle ampliada; a mesma receita foi pro jogo): não é preta, é
# marrom-carvão com variação forte. Tinta queimada (92,74,60 sRGB) em ~55%, fuligem (34,30,27) embaixo e em manchas,
# ferrugem de calor (118,70,42) em manchas de ~1,5 m com borda dura, cinza (165,155,142) em placas nas faces de cima.
# Na tela o alvo dá p10/p50/p90 ≈ 0,03/0,33/0,60 (o bloco chapado de antes dava uma cor só).
BURNT_CODE = r"""
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float3 q = WP * 0.01;
// (13/10 volta 2: a foto da praia tem pedrinhas; amostrada sem desfoque o limiar virava pontilhado preto. O SampleBias
// pega um mip borrado: só as manchas largas ficam)
// (volta 4: o SampleBias somava ao mip da distância e o ruído virava uma cor só; o mip fixo dá a mesma mancha a qualquer distância)
float nA = Beach.SampleLevel(BeachSampler, q.yz / 1.5 + 0.13, 5.0).g * w.x + Beach.SampleLevel(BeachSampler, q.xz / 1.5 + 0.41, 5.0).g * w.y
         + Beach.SampleLevel(BeachSampler, q.xy / 1.5 + 0.77, 5.0).g * w.z;
float nB = Beach.SampleLevel(BeachSampler, q.yz / 0.8 + 0.29, 4.0).r * w.x + Beach.SampleLevel(BeachSampler, q.xz / 0.8 + 0.53, 4.0).r * w.y
         + Beach.SampleLevel(BeachSampler, q.xy / 0.8 + 0.91, 4.0).r * w.z;
// a faixa do ruído borrado (medida na foto em linear: mip 5 do G p5/p50/p95 0,221/0,237/0,253; mip 4 do R 0,257/0,284/0,309)
nA = saturate((nA - 0.221) / 0.032);
nB = saturate((nB - 0.257) / 0.052);
float zf = (WP.z - Obj.z) * 0.01;                                   // a altura sobre o chão (a origem do tanque)
// (volta 2: a fuligem cobria quase tudo, a carcaça deu p50 0,15 contra 0,33 no alvo: embaixo só até 0,6 m, e as manchas
// em ~25% do corpo)
float soot = saturate((1.0 - smoothstep(0.2, 0.6, zf)) * 0.8 + (1.0 - smoothstep(0.22, 0.32, nA)) * 0.9);   // ~25% em manchas
float rust = smoothstep(0.78, 0.86, nA) * (1.0 - 0.7 * soot);                                             // ~15%
float up = smoothstep(0.6, 0.9, n.z);
float ash = up * smoothstep(0.7, 0.8, nB) * (1.0 - soot);                                                 // ~20% do teto
float3 col = float3(0.076, 0.061, 0.051);                            // a tinta queimada (volta 4: 78,70,64, mais cinza: o sol baixo puxa pro laranja)
col = lerp(col, float3(0.140, 0.062, 0.030), rust);                  // a ferrugem de calor (volta 2: menos vermelha, 104,70,48)
col = lerp(col, float3(0.016, 0.013, 0.011), soot);                  // a fuligem
col = lerp(col, float3(0.376, 0.328, 0.270), ash);                   // a cinza
col *= 0.9 + 0.2 * nB;
Rough = 0.93;
return col;
"""
_BURNT = None


def burnt_material():
    global _BURNT
    if _BURNT is None:
        F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
        m, c = custom_material('M_LL_Burnt', BURNT_CODE,
                               [('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                                ('Obj', unreal.MaterialExpressionObjectPositionWS), ('Beach', TEX['BeachD'])],
                               [('Rough', F1)])
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
        _BURNT = m
    return _BURNT


def unit_dust_materials(R):
    """Para cada peça do tanque: as instâncias do pó com os parâmetros da peça e da receita."""
    global _DUST_PARENT
    W = R.get('world')
    if not W:
        return {}
    if _DUST_PARENT is None:
        _DUST_PARENT = unit_dust_parent()
    out = {}
    for path in TANK_PARTS:
        mesh = load(path)
        if not mesh:
            continue
        mats = []
        for i, sm in enumerate(mesh.get_editor_property('static_materials')):
            src = sm.get_editor_property('material_interface')
            name = f'MI_LL_{mesh.get_name()}_{i}_{R["name"]}'
            mi = fresh(name, ROOT + '/Materials/Units', unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, _DUST_PARENT)
            if isinstance(src, unreal.MaterialInstanceConstant):
                for tname in ('BaseColorTexture', 'NormalTexture'):
                    t = mel.get_material_instance_texture_parameter_value(src, tname)
                    if t:
                        mel.set_material_instance_texture_parameter_value(mi, tname, t)
                for sname in ('RoughnessFactor', 'MetallicFactor'):
                    mel.set_material_instance_scalar_parameter_value(mi, sname, mel.get_material_instance_scalar_parameter_value(src, sname))
                mel.set_material_instance_vector_parameter_value(mi, 'BaseColorFactor', mel.get_material_instance_vector_parameter_value(src, 'BaseColorFactor'))
            d = sand_family((1.0, 1.05, 1.15), W['dust'])
            mel.set_material_instance_vector_parameter_value(mi, 'DustColor', unreal.LinearColor(d[0], d[1], d[2], 1))
            mel.set_material_instance_scalar_parameter_value(mi, 'DustTop', W.get('unit_dust_top', 0.4))
            mel.set_material_instance_scalar_parameter_value(mi, 'DustLow', W.get('unit_dust_low', 0.6))
            mel.set_material_instance_scalar_parameter_value(mi, 'DustHeight', W.get('unit_dust_height', 1.2))
            eal.save_loaded_asset(mi)
            mats.append(mi)
        out[path] = mats
    log(f'pó do mundo nas unidades: {R["name"]}, {sum(len(v) for v in out.values())} materiais')
    return out


# ---- o tanque USA provisório detalhado (Tools/look_tank.py) -----------------------------
TANK_GLB = os.path.join(REPO, 'captures', 'look', 'tank_glb')
# o aspecto de cada material do tanque (cor sRGB, rugosidade, metal); o pó vem da receita
# a tinta é cinza-claro de tinta (albedo ~0,38), não branco
TANK_SLOTS = {'Paint': ((126, 126, 119), 0.62, 0.0), 'Team': ((86, 108, 150), 0.85, 0.0), 'Dark': ((40, 40, 38), 0.7, 0.0),
              'Metal': ((95, 95, 92), 0.45, 0.7), 'Cloth': ((64, 86, 132), 0.92, 0.0), 'Lens': ((30, 60, 110), 0.15, 0.0),
              'Track': ((55, 52, 48), 0.6, 0.5), 'Char': ((26, 23, 20), 0.92, 0.0), 'Rust': ((74, 46, 30), 0.85, 0.15),
              'Blade': ((196, 160, 40), 0.6, 0.2)}     # o amarelo de máquina (a lâmina do Dozer)


def srgb_lin(c):
    f = lambda v: (v / 255) / 12.92 if v / 255 <= 0.04045 else ((v / 255 + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(f(c[0]), f(c[1]), f(c[2]), 1.0)


SPLIT = False            # a luz do fogo separada nas unidades (desligada: o Felipe preferiu o fogo pintando tudo, 11/10)
R_SCENE = {}             # a receita do mapa sendo montado (a picape pega os materiais por ela)
EXTRA_MESHES = {}        # o que vem com o tanque e não é peça dele (a picape do GLA)
KIT_MATS = {}            # (tinta, receita) -> {slot: material}: a picape usa os mesmos materiais do tanque


def import_tank():
    folder = ROOT + '/Units/USATank'
    out = []
    for f in sorted(glob.glob(os.path.join(TANK_GLB, 'SM_LL_USATank_*.glb')) + glob.glob(os.path.join(TANK_GLB, 'SM_LL_Wreck.glb'))
                    + glob.glob(os.path.join(TANK_GLB, 'SM_LL_Chute.glb')) + glob.glob(os.path.join(TANK_GLB, 'SM_LL_Pickup.glb'))
                    + glob.glob(os.path.join(TANK_GLB, 'SM_LL_Prop_*.glb')) + glob.glob(os.path.join(TANK_GLB, 'SM_LL_Dozer.glb'))):
        name = os.path.splitext(os.path.basename(f))[0]
        pkg = os.path.join(REPO, 'Game', 'Content', 'LookLab', 'Units', 'USATank', name, 'StaticMeshes', name + '.uasset')
        mesh = None
        if os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(f):
            mesh = load(f'{folder}/{name}/StaticMeshes/{name}')
        if not mesh:
            t = unreal.AssetImportTask()
            for k, v in (('filename', f), ('destination_path', folder), ('destination_name', name),
                         ('automated', True), ('replace_existing', True), ('save', False)):
                t.set_editor_property(k, v)
            asset_tools.import_asset_tasks([t])
            ms = [unreal.load_asset(x) for x in t.get_editor_property('imported_object_paths') or []]
            ms = [x for x in ms if isinstance(x, unreal.StaticMesh)]
            if not ms:
                unreal.log_error('LOOK tanque sem malha: ' + f)
                continue
            mesh = ms[0]
            ns = mesh.get_editor_property('nanite_settings')
            ns.set_editor_property('enabled', True)
            mesh.set_editor_property('nanite_settings', ns)
            eal.save_loaded_asset(mesh)
            log('tanque', name)
        if name == 'SM_LL_Pickup':
            EXTRA_MESHES['pickup'] = mesh
        if name == 'SM_LL_Dozer':
            EXTRA_MESHES['dozer'] = mesh
        if name.startswith('SM_LL_Prop_'):
            EXTRA_MESHES[name[len('SM_LL_Prop_'):]] = mesh
        if not name.startswith(('SM_LL_Wreck', 'SM_LL_Chute', 'SM_LL_Pickup', 'SM_LL_Prop_', 'SM_LL_Dozer')):   # carcaça, paraquedas e picape vêm junto; não são peça do tanque
            out.append(mesh)
    return out


PAINTS = {'usa': (126, 126, 119), 'usa_tan': (176, 150, 112), 'china': (64, 72, 50), 'gla': (178, 152, 110), 'usa_night': (150, 128, 92), 'china_night': (40, 46, 32), 'olive': (66, 72, 50), 'burnt': (34, 30, 27),
          'gla_white': (196, 190, 176), 'gla_red': (128, 44, 34), 'gla_rust': (118, 74, 46)}   # as técnicas da road_town_battle (13/10)   # usa_night medida 11/10: o lado L* 36, quase sem amarelo


def wreck_materials(R):
    out = {}
    for key in ('Char', 'Rust', 'Metal', 'Dark'):
        col, rough, metal = TANK_SLOTS[key]
        out[key] = flat_material(f'M_LL_Wreck_{key}', tuple((c / 255.0) ** 2.2 for c in col), rough, metal)
    return out


# a varredura da cor do time (13/10, volta 37): LOOK_TEAM_SWEEP="r,g,b;r,g,b;..." põe um azul candidato em cada tanque
# da USA, sob a mesma luz, num render só (a cor escura e fosca recebe o reflexo laranja da areia: escolher medindo)
TEAM_SWEEP = [tuple(float(v) for v in c.split(',')) for c in os.environ.get('LOOK_TEAM_SWEEP', '').split(';') if c.strip()]   # r,g,b[,specular]


def kit_mat(R, paint, slot, cache):
    """O material de um slot da unidade (tinta da facção, pó do mundo, camuflagem), um por (slot, tinta, receita)."""
    global _DUST_PARENT
    key = next((k for k in TANK_SLOTS if slot.lower().startswith(k.lower())), 'Paint')
    if paint == 'burnt':
        return burnt_material()          # a carcaça: um material só, a variação vem do ruído (BURNT_CODE)
    if key in cache:
        return cache[key]
    if _DUST_PARENT is None:
        _DUST_PARENT = unit_dust_parent()
    W = R.get('world', {})
    white = load('/Engine/EngineResources/WhiteSquareTexture')
    col, rough, metal = TANK_SLOTS[key]
    if key == 'Paint':
        col = PAINTS.get(paint, col)
        col = tuple(W.get('paints', {}).get(paint, col))     # a tinta da facção pode vir da receita (o alvo de cada ambiente)
    if key == 'Team':
        col = tuple(W.get('team', {}).get(paint, col))       # a cor do time também (13/10: o azul das favoritas é forte)
        if paint.startswith('usa_sw'):
            col = TEAM_SWEEP[int(paint[6:])][:3]
    if key == 'Paint' and paint.startswith('usa_sw'):
        col = tuple(W.get('paints', {}).get('usa', PAINTS['usa']))
    if paint == 'olive' and key in ('Team', 'Cloth'):
        col = (120, 40, 34) if key == 'Team' else (58, 62, 46)        # a estrela vermelha e a lona oliva
    if paint.startswith('china') and key in ('Team', 'Cloth'):
        col = (120, 40, 34) if key == 'Team' else (62, 70, 50)      # o vermelho da China e a lona oliva
        if key == 'Team' and paint == 'china_night':
            col = (70, 52, 36)      # a arte da noite não mostra cor de time na China: só um tom de lona escura
    if paint.startswith('gla') and key in ('Team', 'Cloth'):
        col = (130, 36, 28) if key == 'Team' else (92, 74, 50)      # o vermelho do GLA e a lona marrom
    mi = fresh(f'MI_LL_USATank_{key}_{paint}_{R["name"]}', ROOT + '/Materials/Units', unreal.MaterialInstanceConstant,
               unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, _DUST_PARENT)
    if white:
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTexture', white)
    mel.set_material_instance_vector_parameter_value(mi, 'BaseColorFactor', srgb_lin(col))
    if key == 'Paint' and 'unit_paint_roughness' in W:
        rough = W['unit_paint_roughness']
    mel.set_material_instance_scalar_parameter_value(mi, 'RoughnessFactor', rough)
    mel.set_material_instance_scalar_parameter_value(mi, 'MetallicFactor', metal)
    if key == 'Team':
        spec = W.get('team_specular', 0.5)
        if paint.startswith('usa_sw') and len(TEAM_SWEEP[int(paint[6:])]) > 3:
            spec = TEAM_SWEEP[int(paint[6:])][3]
        mel.set_material_instance_scalar_parameter_value(mi, 'SpecularFactor', spec)
    TEAM_DUST = None
    if key == 'Team' and paint.startswith('usa_sw') and len(TEAM_SWEEP[int(paint[6:])]) > 4:
        TEAM_DUST = TEAM_SWEEP[int(paint[6:])][4] if TEAM_SWEEP[int(paint[6:])][4] >= 0 else None   # o pó de cima na peça do time (5º número; < 0 = o da receita)
    d = sand_family((1.0, 1.05, 1.15), W.get('dust', [0.5, 0.35, 0.2]))
    mel.set_material_instance_vector_parameter_value(mi, 'DustColor', unreal.LinearColor(d[0], d[1], d[2], 1))
    k = 0.0 if key in ('Lens',) else 1.0
    mel.set_material_instance_scalar_parameter_value(mi, 'DustTop', (W.get('unit_dust_top', 0.0) if TEAM_DUST is None else TEAM_DUST) * k)
    mel.set_material_instance_scalar_parameter_value(mi, 'DustLow', W.get('unit_dust_low', 0.0) * k)
    log(f'kit {paint} {key}: cor {tuple(round(c) for c in col)} pó de cima {(W.get("unit_dust_top", 0.0) if TEAM_DUST is None else TEAM_DUST) * k}') if key in ('Team', 'Paint') else None
    mel.set_material_instance_scalar_parameter_value(mi, 'DustHeight', W.get('unit_dust_height', 1.2))
    camo = None
    if key == 'Paint' and paint.startswith('china'):
        # a camuflagem verde da China (a arte: verde-escuro, verde-médio e um marrom-oliva)
        camo = ((46, 64, 36), (84, 108, 56), (78, 70, 44))
        if paint == 'china_night':      # à noite a luz azul puxa o verde pro menta: mais oliva e mais escuro (11/10)
            camo = ((44, 50, 26), (76, 82, 40), (96, 84, 52))
    if key == 'Paint' and paint == 'gla':
        # as picapes da arte: areia-amarelado com manchas marrons e oliva (a pintura de deserto feita à mão)
        camo = ((176, 148, 96), (112, 90, 54), (146, 132, 82))
    if camo:
        mel.set_material_instance_scalar_parameter_value(mi, 'Camo', 1.0)
        for nm, cc in zip(('CamoA', 'CamoB', 'CamoC'), camo):
            mel.set_material_instance_vector_parameter_value(mi, nm, srgb_lin(cc))
    eal.save_loaded_asset(mi)
    cache[key] = mi
    return mi


def tank_kit(R, paint='usa'):
    """As peças do tanque com os materiais do pó do mundo: o provisório detalhado (se existir)
    ou o tanque do BlastLab."""
    meshes = import_tank()
    if not meshes:
        dust = unit_dust_materials(R)
        return [(load(p), dust.get(p, [])) for p in TANK_PARTS if load(p)]
    cache, kit = KIT_MATS.setdefault((paint, R['name']), {}), []
    for mesh in meshes:
        mats = []
        for sm in mesh.get_editor_property('static_materials'):
            mats.append(kit_mat(R, paint, str(sm.get_editor_property('material_slot_name')), cache))
        kit.append((mesh, mats))
    log(f'tanque detalhado: {len(kit)} peças, {len(cache)} materiais ({R["name"]})')
    return kit


# ---- a cena --------------------------------------------------------------------------

def ground(mat, mesh):
    import json
    G = json.load(open(os.path.join(GROUND_DIR, 'ground.json')))
    if mesh:
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_material(0, mat)
        a.set_actor_label('Ground_Relief')
        a.set_folder_path('Ground')
    plane = load('/Engine/BasicShapes/Plane.Plane')      # 1 m × 1 m
    tile, n = 50.0, 12                                    # 600 m de chão em volta, em placas de 50 m
    x0, y0, size = G['x0'], G['y0'], G['size']
    for i in range(n):
        for j in range(n):
            x = (i - n / 2 + 0.5) * tile
            y = (j - n / 2 + 0.5) * tile
            if mesh and x - tile / 2 >= x0 and x + tile / 2 <= x0 + size and y - tile / 2 >= y0 and y + tile / 2 <= y0 + size:
                continue                                  # inteira dentro do relevo
            a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, G['outside_z'] * 100),
                                              unreal.Rotator(0, 0, 0))
            a.set_actor_scale3d(unreal.Vector(tile, tile, 1))
            a.static_mesh_component.set_static_mesh(plane)
            a.static_mesh_component.set_material(0, mat)
            a.set_actor_label(f'Ground_{i:02d}_{j:02d}')
            a.set_folder_path('Ground')


def placements():
    import json
    path = os.path.join(GROUND_DIR, 'placements.json')
    return json.load(open(path)) if os.path.exists(path) else {}


_BLOB = None


def blob_material():
    """A sombra de contato: escura no meio, some na borda (oval no plano de 1 × 1 m)."""
    global _BLOB
    if _BLOB:
        return _BLOB
    m = fresh('M_LL_ContactShadow', ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('code', 'float2 d = (UV - 0.5) * 2.0; float r = length(d); return 0.62 * (1.0 - smoothstep(0.35, 1.0, r));')
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', 'UV')
    c.set_editor_property('inputs', [ci])
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    mel.connect_material_expressions(uv, '', c, 'UV')
    black = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -150)
    black.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(black, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    _BLOB = m
    return m


def contact_shadow(x, y, z, yaw, length, width, label):
    plane = load('/Engine/BasicShapes/Plane.Plane')
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100 + 4),
                                      unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.static_mesh_component.set_static_mesh(plane)
    a.set_actor_scale3d(unreal.Vector(length, width, 1.0))
    a.static_mesh_component.set_material(0, blob_material())
    a.static_mesh_component.set_editor_property('cast_shadow', False)
    a.set_actor_label(label)
    a.set_folder_path('Units')


def tanks(P, kits):
    zs = P.get('tanks', [0.0] * len(TANKS))
    for k, t in enumerate(TANKS):
        x, y = t[0], t[1]
        yaw = t[2] if len(t) > 2 else 0.0
        paint = t[3] if len(t) > 3 else 'usa'
        if paint == 'usa' and k < len(TEAM_SWEEP):      # a varredura da cor do time: um azul candidato por tanque
            paint = f'usa_sw{k}'
        kit = kits(paint)
        loc = unreal.Vector(x * 100, y * 100, zs[k] * 100)
        rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
        if paint.startswith('gla') and EXTRA_MESHES.get('pickup'):
            # o GLA da arte anda de picape técnica (look_tank.py), 10% maior (a arte exagera os veículos)
            mesh = EXTRA_MESHES['pickup']
            cache = KIT_MATS[(paint, R_SCENE['name'])]
            a = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            a.static_mesh_component.set_static_mesh(mesh)
            a.set_actor_scale3d(unreal.Vector(1.1, 1.1, 1.1))
            for i, sm in enumerate(mesh.get_editor_property('static_materials')):
                a.static_mesh_component.set_material(i, kit_mat(R_SCENE, paint, str(sm.get_editor_property('material_slot_name')), cache))
            a.set_actor_label(f'Pickup{k}')
            a.set_folder_path('Units')
            contact_shadow(x, y, zs[k], yaw, 6.6, 2.9, f'Pickup{k}_ContactShadow')
            continue
        contact_shadow(x, y, zs[k], yaw, 9.4, 4.6, f'Tank{k}_ContactShadow')
        hull = None
        for mesh, mats in kit:
            a = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            a.static_mesh_component.set_static_mesh(mesh)
            for i, mi in enumerate(mats):
                a.static_mesh_component.set_material(i, mi)
            a.set_actor_label(f'Tank{k}_{mesh.get_name()}')
            a.set_folder_path(f'Units/Tank{k}')
            if hull is None:
                hull = a
            else:
                a.attach_to_actor(hull, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                                  unreal.AttachmentRule.KEEP_WORLD, False)

def dozers(P):
    """O Dozer da cena de perto (look_layout_ground): os materiais das unidades (tinta USA, pó do mundo)."""
    mesh = EXTRA_MESHES.get('dozer')
    if not mesh:
        return
    cache = KIT_MATS.setdefault(('usa', R_SCENE['name']), {})
    for k, (x, y, yaw) in enumerate(getattr(look_scene, 'DOZERS', [])):
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, 0), unreal.Rotator(0, 0, yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        for i, sm in enumerate(mesh.get_editor_property('static_materials')):
            a.static_mesh_component.set_material(i, kit_mat(R_SCENE, 'usa', str(sm.get_editor_property('material_slot_name')), cache))
        a.set_actor_label(f'Dozer{k}')
        a.set_folder_path('Units')
        contact_shadow(x, y, 0.0, yaw, 7.6, 4.0, f'Dozer{k}_ContactShadow')


def props(P):
    def put(mesh, x, y, z, yaw, sc, label, folder, mi=None):
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(unreal.Vector(sc, sc, sc))
        if mi:
            for i in range(a.static_mesh_component.get_num_materials()):
                a.static_mesh_component.set_material(i, mi)
        a.set_actor_label(label)
        a.set_folder_path(folder)
        return a

    zp = P.get('palms', [0.0] * len(PALMS))
    for k, (x, y, mesh, sc, yaw) in enumerate(PALMS):
        m = load(f'/Game/Slice/Palms/{mesh}/{mesh}')
        if m:
            pa = put(m, x, y, zp[k], yaw, sc, 'Palm_' + mesh, 'Props')
            # os materiais da palmeira do WorldForge (folha com a luz atravessando, cor da vértice, vento): na fatia
            # eles vão no componente instanciado, então a malha sozinha vinha com o material cru do glTF (copa escura)
            mats = [unreal.load_asset(f'/Game/Slice/Materials/M_{mesh}_Foliage'), unreal.load_asset(f'/Game/Slice/Materials/M_{mesh}_Trunk')]
            for i in range(min(2, pa.static_mesh_component.get_num_materials())):
                if mats[i]:
                    pa.static_mesh_component.set_material(i, mats[i])
    zs = P.get('shrubs', [0.0] * len(SHRUBS))
    for k, (x, y, mesh, sc, yaw) in enumerate(SHRUBS):
        m = load(f'/Game/Slice/Models/{mesh}')
        if m:
            fam = mesh.split('/')[0]
            put(m, x, y, zs[k], yaw, sc, 'Shrub_' + mesh.split('/')[-1], 'Props', load(f'/Game/Slice/Models/{fam}/{SHRUB_MI[fam]}'))
    # as miudezas (caixotes, tambores, munição, sacos de areia): materiais das casas pelo nome do slot
    zq = P.get('props', [])
    for k, (nm, x, y, yaw) in enumerate(getattr(look_scene, 'PROPS', [])):
        m = EXTRA_MESHES.get(nm)
        if not m:
            continue
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, (zq[k] if k < len(zq) else 0.0) * 100),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(m)
        for i, sm in enumerate(m.get_editor_property('static_materials')):
            slot = str(sm.get_editor_property('material_slot_name'))
            key = next((kk for kk in HOUSE_MATS if slot.lower().startswith(kk.lower())), None)
            if key:
                a.static_mesh_component.set_material(i, HOUSE_MATS[key])
        a.set_actor_label(f'Prop_{nm}_{k}')
        a.set_folder_path('Props')
    # o vestir da vila (look_ground.dressing): as peças do kit de entulho pelas regras da planta
    for k, (nm, x, y, z, yaw) in enumerate(P.get('dressing', [])):
        m = EXTRA_MESHES.get(nm)
        if not m:
            continue
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100),
                                          unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.static_mesh_component.set_static_mesh(m)
        PM = prop_materials()
        for i, sm in enumerate(m.get_editor_property('static_materials')):
            slot = str(sm.get_editor_property('material_slot_name'))
            key = next((kk for kk in PM if slot.lower().startswith(kk.lower())), None)
            if key and PM.get(key):
                a.static_mesh_component.set_material(i, PM[key])
        a.set_actor_label(f'Dress_{nm}_{k}')
        a.set_folder_path('Dressing')
    pole = load('/Game/Slice/Models/modular_electricity_poles/preset_02_pole')
    zo = P.get('poles', [0.0] * len(POLES))
    if pole:
        for k, (x, y, yaw) in enumerate(POLES):
            put(pole, x, y, zo[k], yaw, 1.0, 'Pole', 'Props')
    # o espalhamento (look_ground.py): tufos secos e pedrinhas onde o vento deixa
    meshes = {k: [load(f'/Game/Slice/Models/{m}') for m in v] for k, v in SCATTER.items()}
    mis = {fam: load(f'/Game/Slice/Models/{fam}/{mi}') for fam, mi in SCATTER_MI.items()}
    # o tom de cada planta na cena (o capim seco da arte de perto é oliva-amarelado claro; o do deserto saía quase preto)
    for fam, tint in getattr(look_scene, 'SCATTER_TINT', {}).items():
        if mis.get(fam):
            mi = fresh(f'MI_LL_{fam}{SUFFIX}', ROOT + '/Materials', unreal.MaterialInstanceConstant,
                       unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, mis[fam])
            mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
            if len(tint) > 3:
                mel.set_material_instance_scalar_parameter_value(mi, 'Desat', tint[3])
            eal.save_loaded_asset(mi)
            mis[fam] = mi
    for i, it in enumerate(P.get('scatter', [])):
        opts = SCATTER[it['kind']]
        j = it['v'] % len(opts)
        m = meshes[it['kind']][j]
        if m:
            put(m, it['x'], it['y'], it['z'] - 0.03, it['yaw'], it['s'], f'{it["kind"]}_{i}', 'Scatter',
                mis.get(opts[j].split('/')[0]))

def explosions():
    """A explosão parada no instante da arte (o ator do jogo, BlastFx: o raymarch desenha)."""
    cls = getattr(unreal, 'ExplosionVolume', None)
    mat = load('/Game/Explosions/M_Explosion')
    if not cls:
        unreal.log_warning('LOOK sem a classe ExplosionVolume (módulo BlastFx do jogo)')
        return
    for x, y, bake, age, size, seed in EXPLOSIONS:
        age = float(os.environ.get('LOOK_EXPLOSION_AGE', age))     # outro instante (a comparação das sessões de explosão)
        b = load(f'/Game/Explosions/{bake}')
        if not b:
            continue
        e = actors.spawn_actor_from_class(cls, unreal.Vector(x * 100, y * 100, 0), unreal.Rotator(0, 0, 0))
        for k, v in (('bake', b), ('age', age), ('size_scale', size), ('seed', seed), ('self_advance', False)):
            setp(e, k, v)
        vol = e.get_editor_property('volume')
        if vol and mat:
            vol.set_material(0, mat)
        e.set_actor_label('Explosion_' + bake)
        e.set_folder_path('Explosions')
        log(f'explosão {bake} em ({x}, {y}), {age} s')


# A água de teste (13/10, o plano da água do WorldForge, WORLDFORGE.md §12.3): só pra medir na 1660 o custo do Single
# Layer Water contra a água translúcida comum, com a mesma lagoa e a mesma câmera do jogo (look.ps1 perf). A receita da
# variante traz "water": {"mode": "slw" | "translucent", "rect": [cx, cy, w, h] em m, "z": m}. Os dois materiais têm o
# mesmo custo de base (duas amostras de normal correndo, a mesma conta): a diferença medida é a do caminho de render.
WATER_CODE = r"""
float2 p = WP.xy * 0.01;
float t = Tsec;
float2 n1 = WaterN.Sample(WaterNSampler, p / 7.0 + float2(0.013, 0.007) * t).rg * 2.0 - 1.0;
float2 n2 = WaterN.Sample(WaterNSampler, p / 3.1 + float2(-0.009, 0.016) * t + 0.37).rg * 2.0 - 1.0;
float2 nxy = (n1 + n2) * 0.12;
Nrm = normalize(float3(nxy, 1.0));
return float3(0.015, 0.045, 0.042);
"""


def water_material(mode):
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    name = 'M_LL_Water_' + ('SLW' if mode == 'slw' else 'Trans')
    m, c = custom_material(name, WATER_CODE, [('WP', unreal.MaterialExpressionWorldPosition),
                                              ('Tsec', unreal.MaterialExpressionTime), ('WaterN', TEX['BeachN'])],
                           [('Nrm', F3)])
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(c, 'Nrm', unreal.MaterialProperty.MP_NORMAL)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 250)
    r.set_editor_property('r', 0.04)
    mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if mode == 'slw':
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
        out = mel.create_material_expression(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 200, 300)
        vals = {'ScatteringCoefficients': (0.01, 0.025, 0.02), 'AbsorptionCoefficients': (0.45, 0.09, 0.06),
                'PhaseG': 0.1, 'ColorScaleBehindWater': 1.0}
        y = 300
        for k, v in vals.items():
            if isinstance(v, tuple):
                e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -100, y)
                e.set_editor_property('constant', unreal.LinearColor(v[0], v[1], v[2], 1.0))
            else:
                e = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -100, y)
                e.set_editor_property('r', v)
            ok = mel.connect_material_expressions(e, '', out, k)
            log(f'água SLW {k}: {"ligado" if ok else "NÃO ligou"}')
            y += 90
    else:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        df = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -300, 350)
        df.set_editor_property('opacity_default', 0.85)
        df.set_editor_property('fade_distance_default', 60.0)
        mel.connect_material_property(df, '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def water(rec):
    W = rec.get('water')
    if not W:
        return
    cx, cy, w, h = W['rect']
    if os.environ.get('LOOK_WATER_RECT'):      # o tamanho da medida (o oásis 15% da tela, o lago 40%) sem outra receita
        cx, cy, w, h = (float(v) for v in os.environ['LOOK_WATER_RECT'].split(','))
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx * 100, cy * 100, W.get('z', 0.4) * 100),
                                      unreal.Rotator(0, 0, 0))
    smc = a.static_mesh_component
    smc.set_static_mesh(load('/Engine/BasicShapes/Plane'))
    a.set_actor_scale3d(unreal.Vector(w, h, 1.0))       # o plano da Unreal tem 1 m
    smc.set_material(0, water_material(W['mode']))
    smc.set_editor_property('cast_shadow', False)
    a.set_actor_label('Water_' + W['mode'])
    a.set_folder_path('Water')
    log(f'água {W["mode"]}: {w}×{h} m em ({cx}, {cy}), z {W.get("z", 0.4)} m')


# A cartela de cores (13/10, a look-dev dos estúdios): LOOK_CHECKER=1 põe a ColorChecker de 24 cores (placas de 2 m, foscas)
# na praça, no centro da câmera do jogo, pra medir o que a luz e a gradação fazem com cada cor conhecida (o azul do time
# saía lavanda e nada no material explicava). Só nas montagens de teste (a vaga LookTest).
CHECKER = [(115, 82, 68), (194, 150, 130), (98, 122, 157), (87, 108, 67), (133, 128, 177), (103, 189, 170),
           (214, 126, 44), (80, 91, 166), (193, 90, 99), (94, 60, 108), (157, 188, 64), (224, 163, 46),
           (56, 61, 150), (70, 148, 73), (175, 54, 60), (231, 199, 31), (187, 86, 149), (8, 133, 161),
           (243, 243, 242), (200, 200, 200), (160, 160, 160), (122, 122, 121), (85, 85, 85), (52, 52, 52)]
CHECKER_CENTER = (7.3, 1.7)


def color_checker():
    if not os.environ.get('LOOK_CHECKER'):
        return
    m = fresh('M_LL_Checker', ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    c = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
    c.set_editor_property('parameter_name', 'C')
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 150)
    r.set_editor_property('r', 0.85)
    mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    cube = load('/Engine/BasicShapes/Cube')
    for k, col in enumerate(CHECKER):
        mi = fresh(f'MI_LL_Checker_{k:02d}', ROOT + '/Materials', unreal.MaterialInstanceConstant,
                   unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, m)
        mel.set_material_instance_vector_parameter_value(mi, 'C', srgb_lin(col))
        eal.save_loaded_asset(mi)
        i, j = k % 6, k // 6
        x = CHECKER_CENTER[0] + (i - 2.5) * 2.4
        y = CHECKER_CENTER[1] + (j - 1.5) * 2.4
        a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, 16.0), unreal.Rotator(0, 0, 0))
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector(2.0, 2.0, 0.04))
        a.static_mesh_component.set_material(0, mi)
        a.set_actor_label(f'Checker_{k:02d}')
        a.set_folder_path('Checker')
    log(f'cartela de cores: 24 placas em {CHECKER_CENTER}')


def scene_lights():
    """As luzes pontuais da cena (o fogo das carcaças, o sinalizador): candelas, raio em metros."""
    for i, (x, y, z, col, cd, radius, shadow) in enumerate(LIGHTS):
        a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * 100, y * 100, z * 100), unreal.Rotator(0, 0, 0))
        c = a.get_component_by_class(unreal.PointLightComponent)
        if getattr(look_scene, 'FIRES', None) and SPLIT:
            setp(c, 'lighting_channels', channels(False, True, True))
        setp(c, 'intensity_units', unreal.LightUnits.CANDELAS)
        setp(c, 'intensity', cd)
        setp(c, 'light_color', unreal.Color(r=col[0], g=col[1], b=col[2], a=255))
        setp(c, 'attenuation_radius', radius * 100.0)
        setp(c, 'cast_shadows', shadow)
        setp(c, 'mobility', unreal.ComponentMobility.MOVABLE)
        setp(c, 'source_radius', 30.0)
        a.set_actor_label(f'Light_{i}')
        a.set_folder_path('Lights')
    flare = getattr(look_scene, 'FLARE', None)
    if flare:
        # o paraquedas (look_tank.py: cúpula de gomos, cordas, a lata) aceso por baixo pela chama: o pano brilha
        # (a luz atravessa o náilon), mais claro na barra e com as costuras marcadas; a chama é um ponto quase branco
        mesh = load(f'{ROOT}/Units/USATank/SM_LL_Chute/StaticMeshes/SM_LL_Chute')
        if mesh:
            a = put_mesh(mesh, flare[0], flare[1], flare[2], unreal.Rotator(0, 0, 0), (1.0, 1.0, 1.0), None, 'Flare_Chute', 'Lights')
            cm, cc = custom_material('M_LL_Chute', CHUTE_CODE, [('WP', unreal.MaterialExpressionWorldPosition),
                                                                ('Obj', unreal.MaterialExpressionObjectPositionWS)], [])
            cm.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
            cm.set_editor_property('two_sided', True)
            br = mel.create_material_expression(cm, unreal.MaterialExpressionScalarParameter, -700, 300)
            br.set_editor_property('parameter_name', 'Bright')
            br.set_editor_property('default_value', 17000.0)
            ci = list(cc.get_editor_property('inputs'))
            x = unreal.CustomInput()
            x.set_editor_property('input_name', 'Bright')
            cc.set_editor_property('inputs', ci + [x])
            mel.connect_material_expressions(br, '', cc, 'Bright')
            mel.connect_material_property(cc, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.recompile_material(cm)
            eal.save_loaded_asset(cm)
            cord = emissive_material('M_LL_Cord', (5200.0, 5300.0, 5600.0))
            dark = flat_material('M_LL_FlareCan', (0.03, 0.03, 0.03), 0.6)
            for i, sm in enumerate(mesh.get_editor_property('static_materials')):
                slot = str(sm.get_editor_property('material_slot_name'))
                a.static_mesh_component.set_material(i, cm if 'Chute' in slot else cord if 'Cord' in slot else dark)
        sphere = load('/Engine/BasicShapes/Sphere.Sphere')
        core = put_mesh(sphere, flare[0], flare[1], flare[2] - 0.25, unreal.Rotator(0, 0, 0), (0.16, 0.16, 0.16),
                        emissive_material('M_LL_FlareCore', (600000.0, 620000.0, 660000.0)), 'Flare_Core', 'Lights')


def emissive_material(name, rgb):
    m = fresh(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    c = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    c.set_editor_property('constant', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def wrecks(P, kits):
    """As carcaças queimando: o carro queimado (look_tank.py) e o fogo em loop por cima."""
    if not WRECKS:
        return
    mesh = load(f'{ROOT}/Units/USATank/SM_LL_Wreck/StaticMeshes/SM_LL_Wreck')
    mats = {}
    if mesh:
        for i, sm in enumerate(mesh.get_editor_property('static_materials')):
            slot = str(sm.get_editor_property('material_slot_name'))
            mats[i] = slot
    # o fogo: uma chama pequena da Carga Pesada parada no fogo (o raymarch; o volume em loop
    # DA_WreckFireLoop com o material da Carga Pesada derrubou a placa em 11/10)
    zs = P.get('wrecks', [0.0] * len(WRECKS))
    for k, (x, y, yaw, sc) in enumerate(WRECKS):
        z = zs[k] if k < len(zs) else 0.0
        if mesh:
            a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            a.static_mesh_component.set_static_mesh(mesh)
            for i, slot in mats.items():      # (13/10: o carvão chapado lia "borrão preto": o material da carcaça)
                a.static_mesh_component.set_material(i, burnt_material())
            a.set_actor_scale3d(unreal.Vector(sc, sc, sc))
            a.set_actor_label(f'Wreck_{k}')
            a.set_folder_path('Wrecks')
    log(f'carcaças: {len(WRECKS)}')


WRECK_MATS = {}


# ---- os efeitos de look-dev (a noite): fogo, fumaça, queimado, destroços, faíscas, traçantes ------------
# Da sessão de gráficos, só pra a maquete ter a luz e a cor certas; o efeito final é das sessões de explosão.
FLAME_CODE = r"""
float h = saturate((WP.z - Obj.z) / max(Bnd.z, 1.0) + 0.5);      // 0 na base, 1 no topo do cartão
float x = abs(UV.x - 0.5) * 2.0;
float n1 = saturate((Beach.Sample(BeachSampler, float2(UV.x * 1.4 + Seed, h * 1.1 - Seed * 0.7)).r - 0.258) / 0.05);
float n2 = saturate((Beach.Sample(BeachSampler, float2(UV.x * 3.1 + Seed * 2.0, h * 2.6 + Seed)).g - 0.222) / 0.04);
float width = pow(saturate(1.0 - h), 0.55) * (0.65 + 0.5 * n1);
float shape = saturate(1.0 - x / max(width, 0.02));
shape *= saturate((1.0 - h) * 1.6 - 0.15 + (n2 - 0.5) * 0.8);
shape = saturate(shape * 1.6);
float core = pow(shape, 3.0);
float3 col = lerp(float3(0.75, 0.08, 0.01), float3(1.0, 0.42, 0.06), shape);
col = lerp(col, float3(1.0, 0.85, 0.5), core);
return col * shape * Intensity;
"""

SMOKE_CODE = r"""
float3 v = normalize(CV);        // o vetor da câmera da Unreal já aponta da superfície pra câmera
float rim = saturate(dot(normalize(N), v));
float n = saturate((Beach.Sample(BeachSampler, WP.xy * 0.004 + WP.z * 0.003 + Seed).r - 0.256) / 0.05);
float n2 = saturate((Beach.Sample(BeachSampler, WP.yz * 0.009 + Seed * 1.7).g - 0.222) / 0.04);
return saturate(saturate(rim * 1.7 - 0.1) * (0.6 + 0.7 * n * n2 + 0.3 * n) * Density * 1.25);   // densa no meio, rala na borda
"""

SCORCH_CODE = r"""
float2 d = (UV - 0.5) * 2.0;
float r = length(d);
float n = saturate((Beach.Sample(BeachSampler, UV * 2.3 + Seed).r - 0.256) / 0.05);
float n2 = saturate((Beach.Sample(BeachSampler, UV * 6.1 + Seed * 3.0).g - 0.222) / 0.04);
float edge = r + (n - 0.5) * 0.55 + (n2 - 0.5) * 0.25;
return 0.62 * (1.0 - smoothstep(0.45, 0.95, edge));   // queimado, não buraco preto (a arte: marrom-escuro com destroços)
"""

# Os cartões de fogo e fumaça (refeitos em 11/10, o jeito dos jogos): um quadrado virado pra câmera da arte
# com a textura desenhada no plano da câmera (a conta usa a direita e o alto da tela, não o UV da malha:
# o desenho e a luz não dependem de como o cartão foi girado). A fumaça guarda o relevo do tufo e é acesa
# de verdade pela luz da cena (translúcida com luz por pixel: o fogo embaixo acende o lado de baixo);
# o fogo é a temperatura pintada pela rampa do corpo negro (aditivo).
CARD_UV = r"""
float3 V = normalize(CV);                                   // da superfície pra câmera
float3 up = normalize(float3(0.0, 0.0, 1.0) - V * V.z);    // o alto da tela
float3 rt = cross(V, up);                                   // a direita da tela (a Unreal é mão esquerda)
float3 d = WP - Obj;
float2 q = float2(dot(d, rt), dot(d, up)) / Size;          // −0,5..0,5 no cartão
float cs = cos(Spin), sn = sin(Spin);
float2 r = float2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);
float inside = step(abs(r.x), 0.495) * step(abs(r.y), 0.495);
float2 cell = float2(fmod(Var, 2.0), floor(Var * 0.5));
float2 uv = (float2(r.x, -r.y) + 0.5) * 0.5 + cell * 0.5;
"""

PUFF_CODE = CARD_UV + r"""
float4 T = Puff.Sample(PuffSampler, uv);
float2 nt = T.gb * 2.0 - 1.0;
float2 n2 = float2(cs * nt.x + sn * nt.y, -sn * nt.x + cs * nt.y);
float nz = sqrt(saturate(1.0 - dot(n2, n2)));
Nrm = normalize(n2.x * rt + n2.y * up + nz * V);
float dens = T.r * inside;
Op = saturate(dens * Density) * DF;
float low = saturate(0.5 - r.y * 1.6);                      // a parte de baixo do tufo (a que olha pro fogo)
Emi = (float3(1.0, 0.36, 0.09) * Glow * (0.3 + 0.7 * low) + float3(0.62, 0.64, 0.7) * Amb) * dens;
return float3(1.0, 0.94, 0.86) * Albedo;
"""

FIRE_CODE = CARD_UV + r"""
float4 T = Flame.Sample(FlameSampler, uv);
float t = saturate(lerp(T.r, T.g, Mode) * inside * Heat);
float3 c = float3(0.9, 0.16, 0.02) * smoothstep(0.02, 0.3, t);
c = lerp(c, float3(1.0, 0.45, 0.08), smoothstep(0.25, 0.55, t));
c = lerp(c, float3(1.0, 0.78, 0.42), smoothstep(0.55, 0.85, t));
c = lerp(c, float3(1.0, 0.95, 0.85), smoothstep(0.85, 1.0, t));
return c * t * Intensity * DF;
"""

CHUTE_CODE = r"""
float3 d = WP - Obj;                                        // origem no sinalizador (cm)
float h = saturate((d.z - 600.0) / 190.0);                  // 0 na barra, 1 no topo
float g = abs(frac(atan2(d.y, d.x) / 6.2831853 * 16.0) - 0.5) * 2.0;   // 1 na costura, 0 no meio do gomo
float seam = smoothstep(0.8, 1.0, g);
float glow = lerp(1.0, 0.5, h * h);                         // a barra, mais perto da chama, mais clara
return float3(1.0, 1.0, 1.03) * glow * (1.0 - 0.4 * seam) * Bright;
"""


def card_material(name, code, kind, params, tex_key):
    """kind 'puff' (translúcido aceso por pixel) ou 'fire' (aditivo). params: (nome, padrão)."""
    if name in _FX:
        return _FX[name]
    m = fresh(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    if kind == 'fire':
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    else:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        m.set_editor_property('tangent_space_normal', False)
        setp(m, 'translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    tex_name = 'Puff' if kind == 'puff' else 'Flame'
    names = ['WP', 'Obj', 'CV', 'DF'] + [n for n, _ in params] + [tex_name]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    if kind == 'puff':
        outs = []
        for n, t in (('Op', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('Nrm', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                     ('Emi', unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
            co = unreal.CustomOutput()
            co.set_editor_property('output_name', n)
            co.set_editor_property('output_type', t)
            outs.append(co)
        c.set_editor_property('additional_outputs', outs)
    y = -600
    for n, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('Obj', unreal.MaterialExpressionObjectPositionWS),
                   ('CV', unreal.MaterialExpressionCameraVectorWS)):
        e = mel.create_material_expression(m, cls, -700, y)
        mel.connect_material_expressions(e, '', c, n)
        y += 100
    df = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -700, y)     # some macio onde corta o chão
    setp(df, 'fade_distance_default', 120.0)
    mel.connect_material_expressions(df, '', c, 'DF')
    y += 100
    for n, default in params:
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
        e.set_editor_property('parameter_name', n)
        e.set_editor_property('default_value', default)
        mel.connect_material_expressions(e, '', c, n)
        y += 100
    tex = import_texture(os.path.join(TEX_DIR, f'T_LL_{tex_key}.png'), ROOT + '/Textures', 'masks')
    # (13/10: no -game com o pool de texturas estourado a chama caía no último mip, uma média sem forma, e o cartão inteiro
    #  brilhava: o "quadrado branco" no BlastLab. O MRQ roda sem streaming e não mostrava. Textura de efeito não entra no streaming)
    if not tex.get_editor_property('never_stream'):
        tex.set_editor_property('never_stream', True)
        eal.save_loaded_asset(tex)
    bt = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -700, y)
    bt.set_editor_property('texture', tex)
    bt.set_editor_property('sampler_type', sampler_for(tex))
    mel.connect_material_expressions(bt, '', c, tex_name)
    if kind == 'fire':
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(c, 'Op', unreal.MaterialProperty.MP_OPACITY)
        mel.connect_material_property(c, 'Nrm', unreal.MaterialProperty.MP_NORMAL)
        mel.connect_material_property(c, 'Emi', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 200)
        r.set_editor_property('r', 1.0)
        mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    _FX[name] = m
    return m


_FX = {}


def fx_material(name, code, kind, extra_inputs=(), params=()):
    """kind: 'add' (aditivo, só brilho), 'smoke' (translúcido aceso pela luz da cena), 'scorch' (translúcido preto)."""
    if name in _FX:
        return _FX[name]
    m = fresh(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    if kind == 'add':
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        out_type = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    elif kind == 'smoke':
        # fumaça opaca com máscara pontilhada (o TSR alisa em semitransparência): acesa de verdade pelo fogo
        # (a translúcida não aparecia no render do Movie Render Queue)
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        setp(m, 'dither_opacity_mask', True)
        out_type = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    else:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        out_type = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', out_type)
    names = [n for n, _ in extra_inputs] + [n for n, _ in params] + ['Beach']
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    y = -500
    for n, cls in extra_inputs:
        e = mel.create_material_expression(m, cls, -700, y)
        mel.connect_material_expressions(e, '', c, n)
        y += 100
    for n, default in params:
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
        e.set_editor_property('parameter_name', n)
        e.set_editor_property('default_value', default)
        mel.connect_material_expressions(e, '', c, n)
        y += 100
    bt = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -700, y)
    bt.set_editor_property('texture', load(TEX['BeachD']))
    bt.set_editor_property('sampler_type', sampler_for(load(TEX['BeachD'])))
    mel.connect_material_expressions(bt, '', c, 'Beach')
    if kind == 'add':
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    elif kind == 'scorch':
        k = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -200)
        k.set_editor_property('constant', unreal.LinearColor(0.004, 0.003, 0.002, 1.0))
        mel.connect_material_property(k, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)
    else:
        k = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -200)
        k.set_editor_property('constant', unreal.LinearColor(0.05, 0.045, 0.04, 1.0))     # fuligem: escura, a luz do fogo acende
        mel.connect_material_property(k, '', unreal.MaterialProperty.MP_BASE_COLOR)
        r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, -100)
        r.set_editor_property('r', 1.0)
        mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    _FX[name] = m
    return m


def fx_instance(parent, name, **vals):
    mi = fresh(name, ROOT + '/Materials/FX', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in vals.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    eal.save_loaded_asset(mi)
    return mi


def put_mesh(mesh, x, y, z, rot, scale, mat, label, folder, shadow=False):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100), rot)
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(*scale))
    if mat:
        for i in range(a.static_mesh_component.get_num_materials()):
            a.static_mesh_component.set_material(i, mat)
    a.static_mesh_component.set_editor_property('cast_shadow', shadow)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def channels(c0, c1, c2):
    ch = unreal.LightingChannels()
    ch.set_editor_property('channel0', c0)
    ch.set_editor_property('channel1', c1)
    ch.set_editor_property('channel2', c2)
    return ch


def point_light(x, y, z, col, cd, radius, shadow, label, folder='Lights', source=40.0, chan=None):
    a = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * 100, y * 100, z * 100), unreal.Rotator(0, 0, 0))
    c = a.get_component_by_class(unreal.PointLightComponent)
    if chan:
        setp(c, 'lighting_channels', channels(*chan))
    setp(c, 'intensity_units', unreal.LightUnits.CANDELAS)
    setp(c, 'intensity', cd)
    setp(c, 'light_color', unreal.Color(r=col[0], g=col[1], b=col[2], a=255))
    setp(c, 'attenuation_radius', radius * 100.0)
    setp(c, 'cast_shadows', shadow)
    setp(c, 'mobility', unreal.ComponentMobility.MOVABLE)
    setp(c, 'source_radius', source)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def night_fx(P):
    fires = getattr(look_scene, 'FIRES', [])
    if not fires:
        return
    import random
    plane = load('/Engine/BasicShapes/Plane.Plane')
    cube = load('/Engine/BasicShapes/Cube.Cube')
    scorch_m = fx_material('M_LL_FX_Scorch', SCORCH_CODE, 'scorch', [('UV', unreal.MaterialExpressionTextureCoordinate)], (('Seed', 0.0),))
    CARD = (('Size', 300.0), ('Spin', 0.0), ('Var', 0.0))
    fire_m = card_material('M_LL_FX_Fire', FIRE_CODE, 'fire', CARD + (('Mode', 0.0), ('Heat', 1.0), ('Intensity', 6.0e4)), 'Flame')
    puff_m = card_material('M_LL_FX_Puff', PUFF_CODE, 'puff', CARD + (('Density', 1.0), ('Albedo', 0.1), ('Glow', 0.0), ('Amb', 300.0)), 'Puff')
    spark_m = emissive_material('M_LL_FX_Spark', (90000.0, 42000.0, 9000.0))
    tracer_m = emissive_material('M_LL_FX_Tracer', (160000.0, 120000.0, 50000.0))
    char_m = flat_material('M_LL_FX_Char', (0.022, 0.018, 0.015), 0.95)
    # o cartão olha pra câmera da arte (a do jogo tem o mesmo giro e quase a mesma inclinação)
    rot = unreal.Rotator(roll=0.0, pitch=90.0 + ART_CAM['pitch'], yaw=ART_CAM['yaw'])
    yaw = math.radians(ART_CAM['yaw'])
    right = (-math.sin(yaw), math.cos(yaw))              # a direita da tela, no chão
    n_card = [0]

    def card(parent, x, y, z, size, label, folder, **vals):
        n_card[0] += 1
        mi = fx_instance(parent, f'MI_LL_Card_{n_card[0]}', Size=size * 100.0, **vals)
        return put_mesh(plane, x, y, z, rot, (size, size, 1.0), mi, label, folder)

    rr = random.Random(5)

    def flames(x, y, z, size, k, heat=1.0, inten=9.0e4):
        # as línguas: três cartões lado a lado na tela, de tamanhos diferentes, e o brilho quente da base
        for j, (off, sc) in enumerate(((0.0, 2.4), (-0.35, 1.9), (0.4, 1.7))):
            s_ = size * sc
            card(fire_m, x + right[0] * off * size, y + right[1] * off * size, z + s_ * 0.42, s_, f'Flame_{k}_{j}', 'FX/Fire',
                 Var=float((k + j) % 4), Mode=0.0, Heat=heat * (1.0 - 0.12 * j), Intensity=inten)
        card(fire_m, x, y, z + size * 0.35, size * 2.0, f'FlameBase_{k}', 'FX/Fire', Var=float(k % 4), Mode=1.0,
             Spin=rr.uniform(0, 6.28), Heat=0.75 * heat, Intensity=inten * 0.6)

    # o fogo das carcaças, da casa e da brasa: chamas, luz quente
    for k, (x, y, z, size, cd, rad) in enumerate(fires):
        flames(x, y, z, size, k)
        if SPLIT:
            point_light(x, y, z + 2.4 + size * 0.5, (255, 150, 60), cd * 1.7, rad * 1.2, k < 3, f'FireLight_{k}', source=90.0,
                        chan=(False, False, True))
            point_light(x, y, z + 0.6, (255, 150, 60), cd * 0.8, rad * 0.9, False, f'FireUnitLight_{k}', source=60.0,
                        chan=(False, True, False))
        else:       # o fogo pinta tudo, as unidades inteiras também (a escolha do Felipe, 11/10)
            point_light(x, y, z + 2.4 + size * 0.5, (255, 150, 60), cd * 2.5, rad * 1.2, k < 3, f'FireLight_{k}', source=90.0)
    # a explosão do tanque do meio: a bola de fogo (tufos quentes sobrepostos), línguas, a luz forte e as faíscas
    bx, by, bz, br, bcd, brad = getattr(look_scene, 'BLAST', (0, 0, 0, 0, 0, 0))
    if br:
        for j in range(8):
            ox, oy, oz = rr.uniform(-1, 1) * br * 0.4, rr.uniform(-1, 1) * br * 0.4, rr.uniform(-0.2, 0.9) * br * 0.5
            card(fire_m, bx + ox, by + oy, bz + oz, br * rr.uniform(2.0, 2.8), f'Blast_{j}', 'FX/Blast', Var=float(j % 4),
                 Mode=1.0, Spin=rr.uniform(0, 6.28), Heat=rr.uniform(1.2, 1.55), Intensity=1.4e5)
        flames(bx, by, bz - 0.6, br * 0.9, 90, heat=1.2, inten=1.2e5)
        if SPLIT:
            point_light(bx, by, bz + 3.5, (255, 150, 60), bcd * 1.9, brad * 1.2, True, 'BlastLight', source=200.0,
                        chan=(False, False, True))
            point_light(bx, by, bz + 0.8, (255, 150, 60), bcd * 0.5, brad * 0.9, False, 'BlastUnitLight', source=150.0,
                        chan=(False, True, False))
        else:
            point_light(bx, by, bz + 3.5, (255, 150, 60), bcd * 2.6, brad * 1.2, True, 'BlastLight', source=200.0)
        for j in range(46):   # as faíscas: riscos finos subindo em leque
            a = rr.uniform(0, 6.283)
            up = rr.uniform(25, 80)
            L = rr.uniform(0.4, 1.4)
            d = rr.uniform(0.5, 6.0)
            sx_, sy_ = bx + math.cos(a) * d * 0.6, by + math.sin(a) * d * 0.6
            sz_ = bz + rr.uniform(0.5, 7.0)
            put_mesh(cube, sx_, sy_, sz_, unreal.Rotator(roll=0.0, pitch=up - 90.0, yaw=math.degrees(a)),
                     (0.03, 0.03, L), spark_m, f'Spark_{j}', 'FX/Blast')
    # a fumaça: tufos subindo, alargando e ralando, derivando com o vento; a base acesa pelo fogo
    for k, (x, y, hgt, w, dx, dy) in enumerate(getattr(look_scene, 'SMOKE', [])):
        n = int(hgt / 0.7) + 5
        for j in range(n):
            t = j / (n - 1)
            jx, jy = rr.uniform(-1, 1) * w * 0.18 * (0.3 + t), rr.uniform(-1, 1) * w * 0.18 * (0.3 + t)
            z = 2.2 + hgt * t + rr.uniform(-0.3, 0.3)
            size = w * (1.3 + 2.8 * t) * rr.uniform(0.85, 1.15)
            card(puff_m, x + dx * t * t + jx, y + dy * t * t + jy, z, size, f'Smoke_{k}_{j}', 'FX/Smoke',
                 Var=float(rr.randint(0, 3)), Spin=rr.uniform(0, 6.28), Density=1.3 * min(1.0, 0.25 + 3.0 * t) * (1.0 - 0.7 * t),
                 Albedo=0.08, Glow=9000.0 * math.exp(-hgt * t / 2.6), Amb=200.0)
    # o queimado no chão
    for k, (x, y, rad) in enumerate(getattr(look_scene, 'SCORCH', [])):
        mi = fx_instance(scorch_m, f'MI_LL_Scorch_{k}', Seed=0.23 * k)
        put_mesh(plane, x, y, 0.06, unreal.Rotator(roll=0.0, pitch=0.0, yaw=37.0 * k), (rad * 2, rad * 2, 1.0), mi,
                 f'Scorch_{k}', 'FX/Ground')
    # os destroços: pedaços carbonizados
    stones = [load(f'/Game/Slice/Models/namaqualand_stones_01/namaqualand_stones_01_{v}_LOD0') for v in 'abcde']
    for k, (x, y, yaw, sc, v) in enumerate(getattr(look_scene, 'DEBRIS', [])):
        mesh = stones[v % 5]
        scale = (sc, sc, sc * 0.6)
        put_mesh(mesh, x, y, 0.0, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), scale, char_m, f'Debris_{k}', 'FX/Debris', shadow=True)
    # os traçantes
    for k, (x0, y0, x1, y1, z) in enumerate(getattr(look_scene, 'TRACERS', [])):
        yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
        for j, t in enumerate((0.3, 0.75)):          # dois riscos curtos por rajada
            put_mesh(cube, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                     (2.4, 0.045, 0.045), tracer_m, f'Tracer_{k}_{j}', 'FX/Tracers')
    log(f'efeitos da noite: {len(fires)} fogos, explosão, {len(getattr(look_scene, "SMOKE", []))} fumaças, '
        f'{len(getattr(look_scene, "DEBRIS", []))} destroços')


def look_at_target():
    """O ponto do chão no centro da imagem da arte."""
    c = ART_CAM
    d = c['z'] / math.tan(math.radians(-c['pitch']))
    yaw = math.radians(c['yaw'])
    return c['x'] + d * math.cos(yaw), c['y'] + d * math.sin(yaw)


def camera(label, height, pitch_deg, hfov, yaw_deg, target):
    d = height / math.tan(math.radians(pitch_deg))
    yaw = math.radians(yaw_deg)
    x, y = target[0] - d * math.cos(yaw), target[1] - d * math.sin(yaw)
    cam = actors.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(x * 100, y * 100, height * 100),
                                        unreal.Rotator(roll=0.0, pitch=-pitch_deg, yaw=yaw_deg))
    cam.set_actor_label(label)
    cam.set_folder_path('Cameras')
    cc = cam.get_cine_camera_component()
    fb = cc.get_editor_property('filmback')
    fb.set_editor_property('sensor_width', 36.0)
    fb.set_editor_property('sensor_height', 36.0 * RES[1] / RES[0])
    cc.set_editor_property('filmback', fb)
    cc.set_editor_property('current_focal_length', 18.0 / math.tan(math.radians(hfov / 2)))
    fs = cc.get_editor_property('focus_settings')
    fs.set_editor_property('focus_method', unreal.CameraFocusMethod.DISABLE)
    cc.set_editor_property('focus_settings', fs)
    log(f'camera {label}: ({x:.1f}, {y:.1f}, {height:.1f}) m, {pitch_deg}°, {hfov}°')
    return cam


def lc3(v):
    return unreal.LinearColor(v[0], v[1], v[2], 1.0)


def c8(v):
    return unreal.Color(r=int(v[0]), g=int(v[1]), b=int(v[2]), a=255)


def setp(obj, key, value):
    try:
        obj.set_editor_property(key, value)
    except Exception as e:
        unreal.log_warning(f'LOOK propriedade {key}: {e}')


def environment(R, cam_yaw):
    S = R['sun']
    # direção pro sol = frente da câmera girada pra esquerda (giro negativo na Unreal);
    # a luz anda ao contrário
    light_yaw = cam_yaw - S['from_camera'] + 180.0
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000),
                                        unreal.Rotator(roll=0.0, pitch=-S['elevation'], yaw=light_yaw))
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    for k, v in (('intensity', S['lux']), ('atmosphere_sun_light', True), ('mobility', unreal.ComponentMobility.MOVABLE),
                 ('light_source_angle', S.get('source_angle', 0.53)), ('contact_shadow_length', 0.03),
                 ('dynamic_shadow_distance_movable_light', 40000.0), ('dynamic_shadow_cascades', 4),
                 ('cascade_distribution_exponent', 3.0)):
        setp(lc, k, v)
    if S.get('temperature'):
        setp(lc, 'use_temperature', True)
        setp(lc, 'temperature', S['temperature'])
    if S.get('color'):          # a lua: azul, na mão (sem temperatura de corpo negro)
        setp(lc, 'light_color', c8(S['color']))
    if 'specular_scale' in S:
        setp(lc, 'specular_scale', S['specular_scale'])

    A = R.get('atmosphere', {})
    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    for k in ('rayleigh_scattering_scale', 'mie_scattering_scale', 'mie_absorption_scale', 'mie_anisotropy',
              'mie_exponential_distribution', 'multi_scattering_factor'):
        if k in A:
            setp(sc, k, A[k])
    for k in ('mie_scattering', 'mie_absorption'):
        if k in A:
            setp(sc, k, lc3(A[k]))
    if 'ground_albedo' in A:
        setp(sc, 'ground_albedo', c8(A['ground_albedo']))
    if 'aerial_distance_scale' in A:     # (o nome da propriedade tem o erro de digitação da Unreal)
        setp(sc, 'aerial_pespective_view_distance_scale', A['aerial_distance_scale'])
    if 'aerial_start_km' in A:
        setp(sc, 'aerial_perspective_start_depth', A['aerial_start_km'])
    if 'aerial_start_ratio' in A:
        setp(sc, 'aerial_perspective_start_depth', A['aerial_start_ratio'] * ART_FOCUS_M / 1000.0)

    K = R.get('skylight', {})
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    setp(slc, 'real_time_capture', True)
    setp(slc, 'mobility', unreal.ComponentMobility.MOVABLE)
    setp(slc, 'intensity', K.get('intensity', 1.0))
    setp(slc, 'lower_hemisphere_is_black', True)
    if 'color' in K:
        setp(slc, 'light_color', c8(K['color']))

    F = R.get('fog', {})
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label('HeightFog')
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    setp(fc, 'fog_density', F.get('density', 0.0))
    setp(fc, 'fog_height_falloff', F.get('falloff', 0.2))
    if 'start_cm' in F:
        setp(fc, 'start_distance', F['start_cm'])
    if 'start_ratio' in F:
        setp(fc, 'start_distance', F['start_ratio'] * ART_FOCUS_M * 100.0)
    if 'inscattering' in F:
        setp(fc, 'fog_inscattering_luminance', lc3(F['inscattering']))
    if 'directional' in F:
        d = F['directional']
        setp(fc, 'directional_inscattering_luminance', lc3(d['luminance']))
        setp(fc, 'directional_inscattering_exponent', d.get('exponent', 4.0))
        setp(fc, 'directional_inscattering_start_distance', d.get('start_cm', 0.0))
    V = F.get('volumetric')
    if V:
        setp(fc, 'enable_volumetric_fog', True)
        setp(fc, 'volumetric_fog_start_distance', V.get('start_ratio', 0.0) * ART_FOCUS_M * 100.0 if 'start_ratio' in V
             else V.get('start_cm', 0.0))
        setp(fc, 'volumetric_fog_albedo', c8(V.get('albedo', [255, 255, 255])))
        setp(fc, 'volumetric_fog_scattering_distribution', V.get('scattering_distribution', 0.2))
        setp(fc, 'volumetric_fog_extinction_scale', V.get('extinction_scale', 1.0))
        setp(fc, 'volumetric_fog_distance', V.get('distance_cm', 6000.0))

    P = R.get('post', {})
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    st = pp.get_editor_property('settings')
    vals = [('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
            ('auto_exposure_min_brightness', P['ev100']), ('auto_exposure_max_brightness', P['ev100']),
            ('auto_exposure_apply_physical_camera_exposure', False), ('auto_exposure_bias', P.get('bias', 0.0)),
            ('bloom_intensity', P.get('bloom', 0.675)),
            ('local_exposure_shadow_contrast_scale', P.get('local_exposure_shadow', 1.0)),
            ('local_exposure_highlight_contrast_scale', P.get('local_exposure_highlight', 1.0))]
    if 'white_temp' in P:
        vals.append(('white_temp', P['white_temp']))
    if 'white_tint' in P:
        vals.append(('white_tint', P['white_tint']))
    for key, field in (('saturation', 'color_saturation'), ('contrast', 'color_contrast'), ('gamma', 'color_gamma'),
                       ('gain', 'color_gain'), ('shadow_saturation', 'color_saturation_shadows'),
                       ('shadow_gain', 'color_gain_shadows'), ('highlight_gain', 'color_gain_highlights'),
                       ('midtone_gain', 'color_gain_midtones'), ('shadow_contrast', 'color_contrast_shadows'),
                       ('midtone_saturation', 'color_saturation_midtones'), ('highlight_saturation', 'color_saturation_highlights')):
        if key in P:
            v = P[key]
            v4 = unreal.Vector4(v, v, v, 1.0) if not isinstance(v, list) else unreal.Vector4(v[0], v[1], v[2], 1.0)
            vals.append((field, v4))
    for key in ('film_slope', 'film_toe', 'film_shoulder', 'film_black_clip', 'film_white_clip', 'vignette_intensity'):
        if key in P:
            vals.append((key, P[key]))
    # qualquer outro ajuste do pós pelo nome da Unreal (ex.: local_exposure_detail_strength)
    for key, v in P.get('raw', {}).items():
        vals.append((key, v))
    for prop, value in vals:
        try:
            st.set_editor_property('override_' + prop, True)
            st.set_editor_property(prop, value)
        except Exception as e:
            unreal.log_warning(f'LOOK pós {prop}: {e}')
    pp.set_editor_property('settings', st)
    for a_ in (sun, sky, sl, fog, pp):
        a_.set_folder_path('Environment')
    return fc, sc
    log(f'environment {R["name"]}: sol {S["elevation"]}° de altura, {S["from_camera"]}° à esquerda da câmera, '
        f'EV100 {P["ev100"]:.2f}')


def key_float(seq, component, prop, keys):
    """Uma trilha de número numa propriedade do componente, com degraus (um valor por corte)."""
    b = seq.add_possessable(component)
    tr = b.add_track(unreal.MovieSceneFloatTrack)
    tr.set_property_name_and_path(prop, prop)
    sec = tr.add_section()
    sec.set_range(0, keys[-1][0] + 45)
    ch = sec.get_all_channels()[0]
    for frame, value in keys:
        ch.add_key(unreal.FrameNumber(frame), value, interpolation=unreal.MovieSceneKeyInterpolation.CONSTANT)


def sequence(name, cams, focus=None, rec=None):
    """Uma sequência com os cortes de câmera: 45 quadros por câmera (o Lumen e o TSR
    assentam depois de cada corte; a foto é o último quadro de cada trecho). Com a
    receita, a poeira de cada corte começa na distância do foco daquela câmera."""
    folder = ROOT + '/Sequences'
    seq = fresh(name, folder, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    per = 45
    seq.set_playback_start(0)
    seq.set_playback_end(per * len(cams))
    cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
    for i, cam in enumerate(cams):
        b = seq.add_possessable(cam)
        sec = cut.add_section()
        sec.set_range(i * per, (i + 1) * per)
        sec.set_camera_binding_id(seq.get_binding_id(b))
    if focus and rec:
        fc, sc = focus['fog'], focus['sky']
        F, A = rec.get('fog', {}), rec.get('atmosphere', {})
        d = focus['dist']
        try:
            if 'start_ratio' in F:
                key_float(seq, fc, 'StartDistance', [(i * per, F['start_ratio'] * d[i] * 100.0) for i in range(len(cams))])
            if 'start_ratio' in F.get('volumetric', {}):
                r = F['volumetric']['start_ratio']
                key_float(seq, fc, 'VolumetricFogStartDistance', [(i * per, r * d[i] * 100.0) for i in range(len(cams))])
            if 'aerial_start_ratio' in A:
                key_float(seq, sc, 'AerialPerspectiveStartDepth',
                          [(i * per, A['aerial_start_ratio'] * d[i] / 1000.0) for i in range(len(cams))])
        except Exception as e:
            unreal.log_warning(f'LOOK poeira por câmera não animada: {e}')
    eal.save_loaded_asset(seq)
    log('sequence', name, per * len(cams), 'quadros')
    return seq


def render_config():
    if not hasattr(unreal, 'MoviePipelinePrimaryConfig'):
        unreal.log_warning('LOOK sem o plugin MovieRenderPipeline no projeto: configuração de render não criada')
        return
    folder = ROOT + '/Render'
    cfg = fresh('Config_Look', folder, unreal.MoviePipelinePrimaryConfig, unreal.MoviePipelinePrimaryConfigFactory())
    out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    out.set_editor_property('output_directory', unreal.DirectoryPath(OUT_DIR))
    out.set_editor_property('file_name_format', '{sequence_name}.{frame_number}')
    out.set_editor_property('output_resolution', unreal.IntPoint(RES[0], RES[1]))
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.set_editor_property('engine_warm_up_count', 90)
    aa.set_editor_property('render_warm_up_frames', True)
    # o modo de jogo do Movie Render Queue (sem menu, sem peão) e o visual máximo (ultra)
    go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    go.set_editor_property('cinematic_quality_settings', True)
    try:      # um realce leve de nitidez (o grão nítido do alvo de perto, 12/10); o jogo leva o mesmo pelo plugin
        cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
        cv.add_or_update_console_variable('r.Tonemapper.Sharpen', 0.8)
    except Exception as e:
        unreal.log_warning(f'LOOK nitidez: {e}')
    eal.save_loaded_asset(cfg)
    log('render config', OUT_DIR)
    # o desempenho: a qualidade de jogo (o preset vem do -ExecCmds "Scalability N"), 1920×1080, só o tempo de GPU
    perf = fresh('Config_LookPerf', folder, unreal.MoviePipelinePrimaryConfig, unreal.MoviePipelinePrimaryConfigFactory())
    out = perf.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    out.set_editor_property('output_directory', unreal.DirectoryPath(OUT_DIR + '_perf'))
    out.set_editor_property('file_name_format', '{sequence_name}.{frame_number}')
    out.set_editor_property('output_resolution', unreal.IntPoint(1920, 1080))
    perf.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    perf.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_JPG)
    aa = perf.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.set_editor_property('engine_warm_up_count', 60)
    go = perf.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
    go.set_editor_property('cinematic_quality_settings', False)
    eal.save_loaded_asset(perf)


def night_channels(rec):
    """A leitura das facções à noite (crítica da arte, 11/10): a facção se lê pelo valor em qualquer luz.
    O fogo forte só no cenário; nas unidades, a luz baixa e fraca do fogo (o lado virado pra ele) e a lua
    das unidades, neutra e sem sombra (o topo frio e o bege da USA longe do fogo). A lua azul de sombra fica
    no canal 0 (todos), pra as unidades continuarem com sombra no chão."""
    unit_prefix = ('Tank', 'Pickup')
    n_u = n_s = 0
    for a in actors.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        label = a.get_actor_label()
        if label.startswith('Flame') or label.startswith('Blast_') or label.startswith('Spark') or label.startswith('Tracer'):
            continue
        comp = a.static_mesh_component
        if label.startswith(unit_prefix) and 'ContactShadow' not in label:
            comp.set_editor_property('lighting_channels', channels(True, True, False))
            n_u += 1
        else:
            comp.set_editor_property('lighting_channels', channels(True, False, True))
            n_s += 1
    W = rec.get('world', {})
    um = W.get('unit_moon')
    if um:
        S = rec['sun']
        light_yaw = ART_CAM['yaw'] - S['from_camera'] + 180.0
        m = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5200),
                                          unreal.Rotator(roll=0.0, pitch=-S['elevation'], yaw=light_yaw))
        lc = m.get_component_by_class(unreal.DirectionalLightComponent)
        for k, v in (('intensity', um['lux']), ('light_color', c8(um['color'])), ('cast_shadows', False),
                     ('mobility', unreal.ComponentMobility.MOVABLE), ('atmosphere_sun_light', False),
                     ('specular_scale', um.get('specular_scale', 1.0)), ('lighting_channels', channels(False, True, False))):
            setp(lc, k, v)
        m.set_actor_label('UnitMoon')
        m.set_folder_path('Environment')
    log(f'canais de luz da noite: {n_u} peças de unidade, {n_s} do cenário')


def build_map(variant, rec, mat, houses, gmesh):
    path = f'{ROOT}/Maps/{MAP_PREFIX}_{variant}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    levels.new_level(path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    ws = world.get_world_settings()
    try:
        ws.set_editor_property('default_game_mode', unreal.GameModeBase.static_class())
    except Exception as e:
        log('aviso: modo de jogo do mapa não trocado', e)
    ground(mat, gmesh)
    P = placements()
    _kits = {}

    def kits(paint):
        if paint not in _kits:          # (setdefault avaliaria tank_kit toda vez e recriaria os materiais)
            _kits[paint] = tank_kit(rec, paint)
        return _kits[paint]
    global R_SCENE, SPLIT
    R_SCENE = rec
    SPLIT = bool(rec.get('world', {}).get('unit_light_split', False))
    tanks(P, kits)
    dozers(P)
    if WRECKS:
        WRECK_MATS.clear()
        WRECK_MATS.update(wreck_materials(rec))
        wrecks(P, kits)
    scene_lights()
    night_fx(P)
    props(P)
    place_houses(houses)
    explosions()
    water(rec)
    color_checker()
    RC = rec.get('camera')          # a receita pode trazer a própria câmera (a comparação de perto)
    # o sol: o da câmera da receita (a comparação com o sol da arte de perto) ou o do mapa (sun_from_map: só a câmera gira)
    fog_c, sky_c = environment(rec, RC['yaw'] if RC and not RC.get('sun_from_map') else ART_CAM['yaw'])
    target = look_at_target()
    first = (camera('CAM_Art', RC['height'], RC['pitch'], RC['hfov'], RC['yaw'], RC['target']) if RC else
             camera('CAM_Art', ART_CAM['z'], -ART_CAM['pitch'], ART_CAM['hfov'], ART_CAM['yaw'], target))
    cams = [first,
            camera('CAM_GameDefault', 61.1, 37.5, 50.0, ART_CAM['yaw'], target),
            camera('CAM_GameNear', 31.6, 37.5, 50.0, ART_CAM['yaw'], target)]
    # no -game (look.ps1 perf) a câmera do jogo é a vista do jogador desde o começo
    try:
        cams[1].set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)
    except Exception as e:
        log('aviso: câmera do jogo não ativada', e)
    if getattr(look_scene, 'FIRES', None) and SPLIT:
        night_channels(rec)
    # o peão padrão longe da cena
    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-200000, -200000, 1000), unreal.Rotator(0, 0, 0))
    ps.set_folder_path('Cameras')
    levels.save_current_level()
    dists = [RC['height'] / math.sin(math.radians(RC['pitch'])) if RC else ART_FOCUS_M,
             61.1 / math.sin(math.radians(37.5)), 31.6 / math.sin(math.radians(37.5))]
    focus = {'fog': fog_c, 'sky': sky_c, 'dist': dists}
    sequence(f'Seq_{variant}', cams, focus, rec)
    sequence(f'Seq_{variant}_Q', cams[:1], focus, rec)     # a rápida: só a câmera da arte (pra iterar a luz)
    # o desempenho: 10 s na câmera do jogo (a padrão, 61 m), na mesma luz
    sequence(f'Seq_{variant}_Perf', [cams[1]] * 7, {'fog': fog_c, 'sky': sky_c, 'dist': [dists[1]] * 7}, rec)
    levels.save_current_level()
    log('map', path)


def main():
    mat = ground_material()
    gmesh = import_ground()
    houses = import_houses(house_materials())
    render_config()
    only = os.environ.get('LOOK_VARIANTS')
    for variant, rname in VARIANTS.items():
        if not only or variant in only.split(','):
            build_map(variant, recipe(rname), mat, houses, gmesh)
    log('pronto')


main()
