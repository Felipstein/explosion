"""Monta o FXLab do zero: o cozimento, o tanque, a areia, os materiais e o mapa.

Uso: fxlab.ps1 setup   (roda dentro da Unreal, sem janela)

Tudo do Content/ sai daqui: mude o script, não o asset. A cena repete a do
laboratório no navegador (FXLab/web/fxlab.js), com os mesmos números:
- o cozimento do preset.json (captures/fxlab/bake/<bake>/: six_pos, six_neg, emit) em /Game/FX/<bake>;
- M_FXSixWay: o mesmo cálculo do sombreador do laboratório (SIXWAY_HLSL), linha a linha;
- chão de 400 m com a areia aerial_beach_01 (Poly Haven, CC0) repetida a cada 10 m e o mesmo tom;
- céu: o mesmo degradê de fim de tarde (M_Sky), sol de 3.4 lux, luz do céu, névoa leve;
- o tanque USA (5 peças, montadas no lugar), com 7.9 m de comprimento contando o cano;
- o fogo (AFXWreckFire) no anel da torre.
Exposição 1 sem adaptação do olho (o controlador desliga), sem vinheta, sem borrão de
movimento, sem exposição local: o que muda entre os dois é só a curva de tela.
"""
import json
import math
import os

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
REPO = os.path.abspath(os.path.join(PROJECT, '..'))
MAP = '/Game/Maps/Lookdev_Fire'
TANK_DIR = os.path.join(REPO, 'captures', 'concept', '30_to_3d', 'usa_tank', 'parts_v2')
SAND = os.path.join(REPO, 'WorldForge', 'Intermediate', 'Assets', 'aerial_beach_01', 'aerial_beach_01_diff_4k.jpg')
TANK_LEN_CM = 790.0

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
preset = json.load(open(os.path.join(PROJECT, 'preset.json'), encoding='utf-8'))
BAKE = os.environ.get('FXLAB_BAKE') or preset['bake']


def log(*a):
    unreal.log('FXLAB ' + ' '.join(str(x) for x in a))


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def import_file(path, dest, name):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', path)
    t.set_editor_property('destination_path', dest)
    t.set_editor_property('destination_name', name)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    return list(t.get_editor_property('imported_object_paths') or [])


def import_bake(bake, root='/Game/FX', emit=True):
    """As três folhas do cozimento em ponto flutuante (RGBA16F, sem compressão: o alfa e o fogo HDR ficam).
    root: onde (o plugin da poeira do jogo usa /RTSTreadDust); emit=False pula a folha do fogo (a poeira não lê)."""
    src = os.path.join(REPO, 'captures', 'fxlab', 'bake', bake)
    out = {}
    files = (('six_pos.exr', 'T_SixPos'), ('six_neg.exr', 'T_SixNeg')) + ((('emit.exr', 'T_Emit'),) if emit else ())
    for file, name in files:
        import_file(os.path.join(src, file), f'{root}/{bake}', name)
        tex = unreal.load_asset(f'{root}/{bake}/{name}')
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('never_stream', True)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        tex.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
        eal.save_loaded_asset(tex)
        out[name] = tex
    log('bake', bake, 'importado')
    return out


def new_material(path):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    d, n = path.rsplit('/', 1)
    m = asset_tools.create_asset(n, d, unreal.Material, unreal.MaterialFactoryNew())
    if m is None:
        # o registro não viu o pacote (conteúdo de plugin num commandlet), mas ele existe: remonta o mesmo, do zero
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    return m


def linker(m, failed):
    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_class().get_name(), sa, b.get_class().get_name(), eb))
    return link


def custom(m, x, y, desc, code, inputs, out_type):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('description', desc)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', out_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    return c


# O mesmo cálculo do sombreador do laboratório (fxlab.js, fragmentShader), linha a linha. Diferenças
# só de convenção: a Unreal lê a textura de cima pra baixo (o EXRLoader do navegador, de baixo pra
# cima), e cada cartão traz no vértice o que lá vem por atributo:
#   UV0 = canto do cartão (y pra cima), UV1 = (quadro, opacidade), UV2 = (fogo, fundo da bola),
#   UV3 = (erosão, sol que passa pelos outros), VC.rgb = direção do sol no cartão × 0.5 + 0.5.
# A luz: a do ajuste (SunCol, SkyCol: o laboratório) ou, com SceneLight = 1, a do mapa do jogo, a
# mesma que acende o chão: o sol que chega no ponto depois da atmosfera (SkyAtmosphereLightIlluminance,
# lux) e o céu da SkyLight (SkyLightEnvMapSample pra cima, rugosidade 1: a radiância média do céu;
# × π = a iluminância num plano deitado, a unidade do SkyCol). EmitScale = 1/exposição do mapa:
# o fogo sai na tela com o valor aprovado no laboratório (exposição 1).
SCENE_LIGHT_HLSL = r'''
float3 SunC = lerp(SunCol.rgb, SceneSun, SceneLight);
float3 SkyC = lerp(SkyCol.rgb, SceneSky * 3.14159265, SceneLight);
'''
SIXWAY_HLSL = SCENE_LIGHT_HLSL + r'''
float cols = Grid.x, rows = Grid.y, frames = Grid.z;
float fr = UV1.x;
float f0 = floor(fr), f1 = min(f0 + 1.0, frames - 1.0), t = fr - f0;
float2 uv = UV0;
float2 a = float2((fmod(f0, cols) + uv.x) / cols, (floor(f0 / cols) + 1.0 - uv.y) / rows);
float2 b = float2((fmod(f1, cols) + uv.x) / cols, (floor(f1 / cols) + 1.0 - uv.y) / rows);
float4 Pp = lerp(Texture2DSample(SixPos, SixPosSampler, a), Texture2DSample(SixPos, SixPosSampler, b), t);
float4 Nn = lerp(Texture2DSample(SixNeg, SixNegSampler, a), Texture2DSample(SixNeg, SixNegSampler, b), t);
float3 E = lerp(Texture2DSample(EmitTex, EmitTexSampler, a).rgb, Texture2DSample(EmitTex, EmitTexSampler, b).rgb, t);
float bb = UV2.y;
float keep = smoothstep(bb - 0.08, bb, uv.y);
float erode = UV3.x;
float ae = saturate((Pp.a * keep - erode) / max(1.0 - erode, 1e-3));
keep *= Pp.a > 1e-4 ? ae / max(Pp.a * keep, 1e-4) : 0.0;
Pp *= keep; Nn *= keep; E *= keep;
float alpha = 1.0 - pow(max(1.0 - Pp.a, 0.0), Density);
float dk = Pp.a > 1e-4 ? alpha / Pp.a : 1.0;
float3 L = VC.rgb * 2.0 - 1.0;
float3 w = L * L;
float sunR = (L.x > 0.0 ? Pp.r : Nn.r) * w.x + (L.y > 0.0 ? Pp.g : Nn.g) * w.y + (L.z > 0.0 ? Pp.b : Nn.b) * w.z;
float skyR = 0.5 * Pp.g + 0.125 * (Pp.r + Nn.r + Pp.b + Nn.b) + 0.05 * Nn.g;
float eL = dot(E, float3(0.2126, 0.7152, 0.0722));
E *= smoothstep(EmberCut * 0.5, EmberCut * 1.5, eL * EmitGain);
float T = UV3.y;
float3 col = (SunC * sunR * T + SkyC * skyR * lerp(0.5, 1.0, T)) * dk * Albedo * Tint.rgb + E * EmitGain * UV2.x * EmitScale;
float va = UV1.y;
clip(alpha * va * Fade - ClipAlpha);       // a poeira descarta o pixel quase transparente; o padrão −1 nunca descarta
return float4(col * va, alpha * va) * Fade;
'''


def scene_light_inputs(m, c, link, x, y):
    """SceneSun, SceneSky (a luz do mapa), SceneLight (0 laboratório, 1 mapa) e EmitScale no Custom."""
    E = lambda cls, xx, yy: mel.create_material_expression(m, cls, xx, yy)
    sun = E(unreal.MaterialExpressionSkyAtmosphereLightIlluminance, x, y)
    sun.set_editor_property('light_index', 0)
    link(sun, '', c, 'SceneSun')
    up = E(unreal.MaterialExpressionConstant3Vector, x - 200, y + 100)
    up.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 0))
    rough = E(unreal.MaterialExpressionConstant, x - 200, y + 180)
    rough.set_editor_property('r', 1.0)
    sky = E(unreal.MaterialExpressionSkyLightEnvMapSample, x, y + 120)
    link(up, '', sky, 'Direction')
    link(rough, '', sky, 'Roughness')
    link(sky, '', c, 'SceneSky')
    for i, (pname, val) in enumerate((('SceneLight', 0.0), ('EmitScale', 1.0))):
        p = E(unreal.MaterialExpressionScalarParameter, x, y + 240 + i * 60)
        p.set_editor_property('parameter_name', pname)
        p.set_editor_property('default_value', val)
        link(p, '', c, pname)


def sixway_material(tex):
    m = new_material('/Game/FX/M_FXSixWay')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)     # pré-multiplicado
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('enable_responsive_aa', True)      # o TSR não arrasta o cartão que se mexe
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []
    link = linker(m, failed)
    names = ['UV0', 'UV1', 'UV2', 'UV3', 'VC', 'SixPos', 'SixNeg', 'EmitTex', 'Grid', 'SunCol', 'SkyCol',
             'EmitGain', 'Density', 'EmberCut', 'Albedo', 'Tint', 'Fade', 'SceneSun', 'SceneSky', 'SceneLight', 'EmitScale', 'ClipAlpha']
    c = custom(m, -300, 0, 'SixWay', SIXWAY_HLSL, names, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    scene_light_inputs(m, c, link, -900, 1000)
    for i in range(4):
        tc = E(unreal.MaterialExpressionTextureCoordinate, -900, -400 + i * 60)
        tc.set_editor_property('coordinate_index', i)
        link(tc, '', c, f'UV{i}')
    vc = E(unreal.MaterialExpressionVertexColor, -900, -120)
    link(vc, '', c, 'VC')
    for i, (pname, tname) in enumerate((('SixPos', 'T_SixPos'), ('SixNeg', 'T_SixNeg'), ('EmitTex', 'T_Emit'))):
        to = E(unreal.MaterialExpressionTextureObjectParameter, -900, -40 + i * 120)
        to.set_editor_property('parameter_name', pname)
        to.set_editor_property('texture', tex[tname])
        to.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        link(to, '', c, pname)
    for i, (pname, val) in enumerate((('Grid', unreal.LinearColor(8, 6, 48, 0)), ('SunCol', unreal.LinearColor(1, 1, 1, 0)),
                                       ('SkyCol', unreal.LinearColor(0.3, 0.4, 0.6, 0)), ('Tint', unreal.LinearColor(1, 1, 1, 1)))):
        v = E(unreal.MaterialExpressionVectorParameter, -900, 340 + i * 100)
        v.set_editor_property('parameter_name', pname)
        v.set_editor_property('default_value', val)
        link(v, '', c, pname)
    for i, (pname, val) in enumerate((('EmitGain', 13.7), ('Density', 2.6), ('EmberCut', 0.9), ('Albedo', 1.11), ('ClipAlpha', -1.0))):
        s = E(unreal.MaterialExpressionScalarParameter, -900, 640 + i * 60)
        s.set_editor_property('parameter_name', pname)
        s.set_editor_property('default_value', val)
        link(s, '', c, pname)
    # partícula macia: o cartão some a SoftDist cm de onde encosta (0 = desligado: o fogo aprovado)
    soft = E(unreal.MaterialExpressionScalarParameter, -900, 900)
    soft.set_editor_property('parameter_name', 'SoftDist')
    soft.set_editor_property('default_value', 0.0)
    mx = E(unreal.MaterialExpressionMax, -700, 900)
    mx.set_editor_property('const_b', 0.1)
    link(soft, '', mx, 'A')
    df = E(unreal.MaterialExpressionDepthFade, -500, 900)
    link(mx, '', df, 'FadeDistance')
    link(df, '', c, 'Fade')
    rgb = E(unreal.MaterialExpressionComponentMask, -100, -40)
    for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(ch, on)
    link(c, '', rgb, '')
    al = E(unreal.MaterialExpressionComponentMask, -100, 60)
    for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)):
        al.set_editor_property(ch, on)
    link(c, '', al, '')
    ok = [mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
          mel.connect_material_property(al, '', unreal.MaterialProperty.MP_OPACITY)]
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('M_FXSixWay', ok, 'falhas:', failed)
    return m


# O mesmo cálculo do laboratório (fxlab.js, primMat.fragmentShader). UV0 = canto (0–1), UV1 = (tipo, ganho),
# UV2.x = opacidade, VC.rgb = cor. Tipos: 0 clarão, 1 faísca, 2 jato de areia, 3 grão, 4 cratera.
PRIM_HLSL = SCENE_LIGHT_HLSL + r'''
float2 q = UV0 * 2.0 - 1.0;
float r2 = dot(q, q);
float kind = UV1.x, gain = UV1.y, va = UV2.x;
float4 o = 0;
// ruído de valor (o mesmo do laboratório): n(p) com hash(p) = frac(sin(dot(p, (127.1, 311.7))) * 43758.5453)
#define FXH(p) frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453)
#define FXN(p) lerp(lerp(FXH(floor(p)), FXH(floor(p) + float2(1, 0)), (frac(p) * frac(p) * (3.0 - 2.0 * frac(p))).x), lerp(FXH(floor(p) + float2(0, 1)), FXH(floor(p) + float2(1, 1)), (frac(p) * frac(p) * (3.0 - 2.0 * frac(p))).x), (frac(p) * frac(p) * (3.0 - 2.0 * frac(p))).y)
if (kind < 0.5) {
    float g = (exp(-r2 * 9.0) + 0.35 * exp(-r2 * 3.5)) * (1.0 - smoothstep(0.55, 1.0, sqrt(r2)));
    o = float4(float3(1.0, 0.86, 0.62) * g * gain * EmitScale, 0.0);
} else if (kind < 1.5) {
    float a = (1.0 - q.x * q.x) * smoothstep(0.0, 0.6, UV0.y);
    o = float4(float3(1.0, 0.68, 0.32) * a * gain * EmitScale, 0.0);
} else if (kind < 2.5) {
    float n = FXN(float2(q.x * 3.0, UV0.y * 9.0)) * 0.6 + FXN(float2(q.x * 7.0, UV0.y * 23.0)) * 0.4;
    float a = (1.0 - smoothstep(0.2, 1.0, abs(q.x) + 0.45 * n)) * smoothstep(0.0, 0.25, UV0.y) * (1.0 - smoothstep(0.7, 1.0, UV0.y));
    a *= va;
    float3 lit = VC.rgb * (SunC * 0.55 + SkyC * 0.35);
    o = float4(lit * a, a);
} else if (kind < 3.5) {
    float a = (1.0 - smoothstep(0.6, 1.0, r2)) * va;
    float3 lit = VC.rgb * (SunC * (0.5 + 0.4 * q.y) + SkyC * 0.35);
    o = float4(lit * a, a);
} else {
    float n = FXN(q * 4.0) * 0.5 + FXN(q * 11.0) * 0.5;
    float a = (1.0 - smoothstep(0.2, 1.0, sqrt(r2) + 0.25 * (n - 0.5))) * va;
    float3 c = lerp(float3(0.012, 0.011, 0.01), float3(0.04, 0.036, 0.032), smoothstep(0.0, 0.8, sqrt(r2)));
    o = float4(c * (SunC * 0.2 + SkyC * 0.5) * a, a * 0.85);
}
#undef FXN
#undef FXH
return kind < 3.5 ? o * Fade : o;
'''


def prim_material():
    m = new_material('/Game/FX/M_FXPrim')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('enable_responsive_aa', True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []
    link = linker(m, failed)
    c = custom(m, -300, 0, 'Prim', PRIM_HLSL, ['UV0', 'UV1', 'UV2', 'VC', 'SunCol', 'SkyCol', 'Fade',
                                               'SceneSun', 'SceneSky', 'SceneLight', 'EmitScale'],
               unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    scene_light_inputs(m, c, link, -900, 500)
    df = E(unreal.MaterialExpressionDepthFade, -600, 300)
    fd = E(unreal.MaterialExpressionConstant, -800, 300)
    fd.set_editor_property('r', 40.0)                   # 0.4 m, o uSoft das primitivas no laboratório
    link(fd, '', df, 'FadeDistance')
    link(df, '', c, 'Fade')
    for i in range(3):
        tc = E(unreal.MaterialExpressionTextureCoordinate, -900, -300 + i * 60)
        tc.set_editor_property('coordinate_index', i)
        link(tc, '', c, f'UV{i}')
    vc = E(unreal.MaterialExpressionVertexColor, -900, -100)
    link(vc, '', c, 'VC')
    for i, pname in enumerate(('SunCol', 'SkyCol')):
        v = E(unreal.MaterialExpressionVectorParameter, -900, 0 + i * 100)
        v.set_editor_property('parameter_name', pname)
        v.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 0))
        link(v, '', c, pname)
    rgb = E(unreal.MaterialExpressionComponentMask, -100, -40)
    for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(ch, on)
    link(c, '', rgb, '')
    al = E(unreal.MaterialExpressionComponentMask, -100, 60)
    for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)):
        al.set_editor_property(ch, on)
    link(c, '', al, '')
    ok = [mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
          mel.connect_material_property(al, '', unreal.MaterialProperty.MP_OPACITY)]
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('M_FXPrim', ok, 'falhas:', failed)
    return m


SKY_HLSL = r'''
float3 D = normalize(Dir);
float h = max(D.z, 0.0);
float3 haze = float3(0.78, 0.62, 0.48);
float3 zen = float3(0.30, 0.42, 0.62);
float3 c = lerp(haze, zen, pow(h, 0.55));
float g = max(dot(D, normalize(SunDir.xyz)), 0.0);
c += float3(1.0, 0.62, 0.32) * (0.9 * pow(g, 64.0) + 0.35 * pow(g, 6.0));
return c;
'''


def sky_material():
    """O céu do laboratório: o mesmo degradê, na esfera de 900 m (sem névoa por cima: é céu)."""
    m = new_material('/Game/Env/M_Sky')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('is_sky', True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []
    link = linker(m, failed)
    c = custom(m, -300, 0, 'Sky', SKY_HLSL, ['Dir', 'SunDir'], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    wp = E(unreal.MaterialExpressionWorldPosition, -800, -60)
    op = E(unreal.MaterialExpressionObjectPositionWS, -800, 40)
    d = E(unreal.MaterialExpressionSubtract, -600, 0)
    link(wp, '', d, 'A')
    link(op, '', d, 'B')
    link(d, '', c, 'Dir')
    sd = E(unreal.MaterialExpressionVectorParameter, -600, 120)
    sd.set_editor_property('parameter_name', 'SunDir')
    sd.set_editor_property('default_value', unreal.LinearColor(0.56, -0.8, 0.17, 0))
    link(sd, '', c, 'SunDir')
    ok = mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('M_Sky', ok, 'falhas:', failed)
    return m


def ground_material():
    """A areia do laboratório: a mesma foto, repetida a cada 10 m, vezes o tom 0xd8c0a0, rugosidade 1."""
    import_file(SAND, '/Game/Env', 'T_Sand')
    sand = unreal.load_asset('/Game/Env/T_Sand')
    eal.save_loaded_asset(sand)                # sem salvar, o M_Ground perde a textura (chão preto, 08/10)
    m = new_material('/Game/Env/M_Ground')
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []
    link = linker(m, failed)
    tc = E(unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tc.set_editor_property('u_tiling', 40.0)
    tc.set_editor_property('v_tiling', 40.0)
    s = E(unreal.MaterialExpressionTextureSample, -650, 0)
    s.set_editor_property('texture', sand)
    link(tc, '', s, 'UVs')
    tint = E(unreal.MaterialExpressionConstant3Vector, -650, 200)
    tint.set_editor_property('constant', unreal.LinearColor(srgb_to_linear(0xd8), srgb_to_linear(0xc0), srgb_to_linear(0xa0), 1))
    mul = E(unreal.MaterialExpressionMultiply, -400, 60)
    link(s, 'RGB', mul, 'A')
    link(tint, '', mul, 'B')
    rough = E(unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property('r', 1.0)
    ok = [mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)]
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('M_Ground', ok, 'falhas:', failed)
    return m


def import_tank():
    meshes = {}
    for part in ('Hull', 'Turret', 'Barrel', 'Tracks', 'Wheels'):
        paths = import_file(os.path.join(TANK_DIR, f'SM_USA_MBT_{part}.glb'), '/Game/Units/USA_MBT', f'SM_USA_MBT_{part}')
        ms = [unreal.load_asset(p) for p in paths]
        ms = [x for x in ms if isinstance(x, unreal.StaticMesh)]
        meshes[part] = ms                      # esteira e rodas vêm em várias malhas (track_l/r, wheel_001…018)
        for x in [unreal.load_asset(p) for p in paths]:
            eal.save_loaded_asset(x)
    log('tanque importado', {k: len(v) for k, v in meshes.items()})
    return meshes


def place_tank(meshes):
    """As peças já vêm montadas no lugar: mesma escala e posição pra todas (7.9 m com o cano, no chão,
    centrado). O cano aponta pra +X, como no laboratório. Devolve o anel da torre (cm)."""
    def box(m):
        b = m.get_bounding_box()
        return b.min, b.max
    every = [m for ms in meshes.values() for m in ms]
    mins = [box(m)[0] for m in every]
    maxs = [box(m)[1] for m in every]
    mn = unreal.Vector(min(v.x for v in mins), min(v.y for v in mins), min(v.z for v in mins))
    mx = unreal.Vector(max(v.x for v in maxs), max(v.y for v in maxs), max(v.z for v in maxs))
    along_x = (mx.x - mn.x) >= (mx.y - mn.y)
    s = TANK_LEN_CM / max(mx.x - mn.x, mx.y - mn.y)
    # o cano: de que lado do centro ele está, no eixo comprido
    bmn, bmx = box(meshes['Barrel'][0])
    cx, cy = (mn.x + mx.x) / 2, (mn.y + mx.y) / 2
    if along_x:
        yaw = 0.0 if (bmn.x + bmx.x) / 2 > cx else 180.0
    else:
        yaw = -90.0 if (bmn.y + bmx.y) / 2 > cy else 90.0
    rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
    for part, ms in meshes.items():
        for k, mesh in enumerate(ms):
            a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), rot)
            a.static_mesh_component.set_static_mesh(mesh)
            a.set_actor_scale3d(unreal.Vector(s, s, s))
            a.set_actor_label(f'Tank_{part}' if k == 0 else f'Tank_{part}_{k}')
            a.set_folder_path('Tank')
    # centra e assenta no chão com a caixa do conjunto já girado e escalado
    tank = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('Tank_')]
    lo = unreal.Vector(1e9, 1e9, 1e9)
    hi = unreal.Vector(-1e9, -1e9, -1e9)
    for a in tank:
        o, e = a.get_actor_bounds(False)
        lo = unreal.Vector(min(lo.x, o.x - e.x), min(lo.y, o.y - e.y), min(lo.z, o.z - e.z))
        hi = unreal.Vector(max(hi.x, o.x + e.x), max(hi.y, o.y + e.y), max(hi.z, o.z + e.z))
    shift = unreal.Vector(-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z)
    for a in tank:
        a.set_actor_location(a.get_actor_location() + shift, False, False)
    tur = next(a for a in tank if a.get_actor_label() == 'Tank_Turret')
    o, e = tur.get_actor_bounds(False)
    hatch = unreal.Vector(o.x, o.y, o.z + e.z - 25.0)
    log('tanque: escala', round(s, 4), 'giro', yaw, 'anel da torre (cm)', hatch)
    return hatch


def environment(sky_mat, ground_mat):
    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
    sphere = unreal.load_asset('/Engine/BasicShapes/Sphere.Sphere')
    g = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    g.static_mesh_component.set_static_mesh(plane)
    g.static_mesh_component.set_material(0, ground_mat)
    g.set_actor_scale3d(unreal.Vector(400, 400, 1))           # o Plane da engine tem 1 m: 400 m
    g.set_actor_label('Ground')
    sk = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    sk.static_mesh_component.set_static_mesh(sphere)
    sk.static_mesh_component.set_material(0, sky_mat)
    sk.static_mesh_component.set_cast_shadow(False)
    sk.set_actor_scale3d(unreal.Vector(1800, 1800, 1800))     # a Sphere tem 1 m de diâmetro: raio de 900 m
    sk.set_actor_label('Sky')
    sk.tags = ['Sky']
    # o sol do ajuste (sunElev, sunAz): a luz anda do sol pra cena. Móvel: o FXWreckFire gira ele
    # quando o preset.json muda (fixo, a Unreal ignorava o giro e o sol ficava rente ao chão, 08/10)
    el, az = math.radians(preset['sunElev']), math.radians(preset['sunAz'])
    to_sun = unreal.Vector(math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000),
                                        unreal.MathLibrary.make_rot_from_x(to_sun * -1.0))
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_intensity(float(preset['sun']))                    # lux; o FXWreckFire reaplica do preset.json
    lc.set_light_color(unreal.LinearColor(1.0, 198 / 255, 140 / 255, 1.0))
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('lower_hemisphere_is_black', False)
    slc.set_editor_property('lower_hemisphere_color',
                            unreal.LinearColor(srgb_to_linear(0xb0), srgb_to_linear(0x8a), srgb_to_linear(0x64), 1))
    slc.set_intensity(0.25)
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label('Fog')
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.0005)
    fc.set_editor_property('fog_height_falloff', 0.2)
    fc.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0.78, 0.62, 0.48, 1))
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        ('auto_exposure_bias', 0.0),
                        ('bloom_intensity', 0.15),
                        ('vignette_intensity', 0.0),
                        ('motion_blur_amount', 0.0),
                        ('film_grain_intensity', 0.0),
                        ('lens_flare_intensity', 0.0),
                        ('local_exposure_highlight_contrast_scale', 1.0),
                        ('local_exposure_shadow_contrast_scale', 1.0)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    pp.set_editor_property('settings', s)
    log('ambiente pronto')


def build():
    if eal.does_asset_exist(MAP):
        levels.load_level(MAP)
        for a in actors.get_all_level_actors():
            actors.destroy_actor(a)
    else:
        levels.new_level(MAP)
    tex = import_bake(BAKE)
    six = sixway_material(tex)
    environment(sky_material(), ground_material())
    hatch = place_tank(import_tank())
    fire = actors.spawn_actor_from_class(unreal.FXWreckFire, hatch)
    fire.set_actor_label('WreckFire')
    fire.set_editor_property('card_material', six)
    # o tiro no chão (impact.json): o cozimento da poeira e o material das primitivas
    imp_preset = json.load(open(os.path.join(PROJECT, 'impact.json'), encoding='utf-8'))
    for key in ('dustBake', 'ringBake', 'geyserBake'):
        import_bake(imp_preset[key])
    imp = actors.spawn_actor_from_class(unreal.FXImpact, unreal.Vector(imp_preset['originX'] * 100, imp_preset['originY'] * 100, 0))
    imp.set_actor_label('GroundImpact')
    imp.set_editor_property('card_material', six)
    imp.set_editor_property('prim_material', prim_material())
    levels.save_current_level()
    log('mapa salvo', MAP)


if not getattr(__import__('builtins'), 'FXLAB_NO_BUILD', False):
    build()
