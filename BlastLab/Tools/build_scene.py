"""Monta (ou remonta) o campo de teste a partir do layout do WebGL.

Rodar sem abrir o editor: powershell -File BlastLab/Tools/blast.ps1 build-scene
(roda make_ground.py antes, se a textura do chão ainda não existe).

Conversão WebGL → Unreal e câmeras: ue_util.py.
Idempotente: se o mapa existe, ele é esvaziado e repovoado.
"""
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_util import PROJECT, REPO, TOOLS, camera, ue_pos, ue_quat  # noqa: E402

MAP = '/Game/Maps/TestRange'
BASE_MATERIAL = '/Game/Materials/M_Base'
GROUND_TEXTURES = os.path.join(PROJECT, 'Intermediate', 'Ground')   # make_ground.py

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(*a):
    unreal.log('SCENE ' + ' '.join(str(x) for x in a))


def rotate(q, v):
    """v' = q v q* (q = x, y, z, w), nos eixos WebGL."""
    x, y, z, w = q
    tx = 2 * (y * v[2] - z * v[1])
    ty = 2 * (z * v[0] - x * v[2])
    tz = 2 * (x * v[1] - y * v[0])
    return [v[0] + w * tx + (y * tz - z * ty),
            v[1] + w * ty + (z * tx - x * tz),
            v[2] + w * tz + (x * ty - y * tx)]


# ---- material único: cor/rugosidade/metal por peça (Custom Primitive Data) --
def base_material():
    if unreal.EditorAssetLibrary.does_asset_exist(BASE_MATERIAL):
        return unreal.load_asset(BASE_MATERIAL)
    m = asset_tools.create_asset('M_Base', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('use_custom_primitive_data', True)
    color.set_editor_property('primitive_data_index', 0)       # 0, 1, 2 = rgb · 3 = rugosidade
    color.set_editor_property('default_value', unreal.LinearColor(0.08, 0.08, 0.08, 0.9))
    metal = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 250)
    metal.set_editor_property('parameter_name', 'Metal')
    metal.set_editor_property('use_custom_primitive_data', True)
    metal.set_editor_property('primitive_data_index', 4)
    ok = [mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(color, 'A', unreal.MaterialProperty.MP_ROUGHNESS),
          mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)]
    log('base material links', ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def import_texture(file, name, srgb, normal=False):
    """Importa Intermediate/Ground/<file> pra /Game/Textures (sempre: a fonte pode ter mudado)."""
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(GROUND_TEXTURES, file))
    t.set_editor_property('destination_path', '/Game/Textures')
    t.set_editor_property('destination_name', name)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    tex = unreal.load_asset('/Game/Textures/' + name)
    tex.set_editor_property('srgb', srgb)
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def ground_material():
    """O chão do WebGL: textura de 18 m amostrada em duas escalas e rotações
    (quebra a repetição) + variação larga de tom, em coordenadas de mundo.
    A normal da textura já está em espaço de mundo (make_ground.py)."""
    path = '/Game/Materials/M_Ground'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    ta = import_texture('ground_A.png', 'T_Ground_A', srgb=True)
    tn = import_texture('ground_N.png', 'T_Ground_N', srgb=False, normal=True)
    m = asset_tools.create_asset('M_Ground', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_class().get_name(), sa, b.get_class().get_name(), eb))

    # Coordenadas de mundo só por nós padrão: a Unreal 5 guarda a posição em
    # precisão dupla (LWC) e cuida da conversão neles; num nó de código
    # customizado a posição chegava errada e o chão virava um granulado.
    wp = E(unreal.MaterialExpressionWorldPosition, -1400, 0)
    xy = E(unreal.MaterialExpressionComponentMask, -1250, 0)
    xy.set_editor_property('r', True); xy.set_editor_property('g', True)
    link(wp, '', xy, '')
    # tile de 18 m (1800 cm); a segunda escala é 2.37× maior e deslocada
    uv1 = E(unreal.MaterialExpressionDivide, -1050, -80)
    uv1.set_editor_property('const_b', 1800.0)
    link(xy, '', uv1, 'A')
    uv2a = E(unreal.MaterialExpressionDivide, -1050, 80)
    uv2a.set_editor_property('const_b', 1800.0 * 2.37)
    link(xy, '', uv2a, 'A')
    uv2 = E(unreal.MaterialExpressionAdd, -950, 80)
    uv2.set_editor_property('const_b', 0.37)
    link(uv2a, '', uv2, 'A')

    def sample(tex, kind, y, uv):
        s = E(unreal.MaterialExpressionTextureSample, -700, y)
        s.set_editor_property('texture', tex)
        s.set_editor_property('sampler_type', kind)
        link(uv, '', s, 'UVs')
        return s

    a1 = sample(ta, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300, uv1)
    a2 = sample(ta, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -100, uv2)
    n1 = sample(tn, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 150, uv1)
    n2 = sample(tn, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 350, uv2)

    def blend(x, y, s1, s2):
        l = E(unreal.MaterialExpressionLinearInterpolate, x, y)
        l.set_editor_property('const_alpha', 0.42)
        link(s1, 'RGBA', l, 'A'); link(s2, 'RGBA', l, 'B')
        return l

    la = blend(-450, -200, a1, a2)
    ln = blend(-450, 250, n1, n2)

    # Variação de tom em larga escala (~20 m, o papel do fbm·0.042 do WebGL):
    # a própria textura lida 40× maior — o grão dela vira manchas de 20 m.
    # albedo × (0.72 + 0.62·m), m ∈ [0, 1] (o canal r vai de ~0.08 a ~0.22)
    uv3 = E(unreal.MaterialExpressionDivide, -1050, -500)
    uv3.set_editor_property('const_b', 1800.0 * 40.0)
    link(xy, '', uv3, 'A')
    large = sample(ta, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -500, uv3)
    tone = E(unreal.MaterialExpressionCustom, -450, -500)
    inp = unreal.CustomInput()
    inp.set_editor_property('input_name', 'R')
    tone.set_editor_property('inputs', [inp])
    tone.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    tone.set_editor_property('code', 'return 0.72 + 0.62 * saturate((R - 0.08) / 0.14);')
    link(large, 'R', tone, 'R')
    color = E(unreal.MaterialExpressionMultiply, -200, -300)
    rgb = E(unreal.MaterialExpressionComponentMask, -320, -260)
    rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True)
    link(la, '', rgb, '')
    link(rgb, '', color, 'A'); link(tone, '', color, 'B')
    rough = E(unreal.MaterialExpressionComponentMask, -200, -120)
    rough.set_editor_property('a', True)
    link(la, '', rough, '')
    nrm = E(unreal.MaterialExpressionComponentMask, -200, 250)
    nrm.set_editor_property('r', True); nrm.set_editor_property('g', True); nrm.set_editor_property('b', True)
    link(ln, '', nrm, '')

    ok = [mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS),
          mel.connect_material_property(nrm, '', unreal.MaterialProperty.MP_NORMAL)]
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('ground material', ok, 'failed links:', failed or 'none')
    return m


# metal por tipo (layout.js: 0 chão · 1 concreto · 2 ferrugem · 3 metal · 4 escombro)
METAL = {0: 0.0, 1: 0.0, 2: 0.45, 3: 0.6, 4: 0.0}

# Refletância. As cores do WebGL eram escuras demais pra um material real
# (concreto a 9%): a exposição de lá compensava. Aqui a exposição é física,
# então a refletância também precisa ser: cada tipo é levado à média medida
# do material real (tabelas de albedo de banda larga — solo nu 0.17, concreto
# envelhecido ~0.35; tinta e aço pela cor da superfície), mantendo a variação
# de cor de peça pra peça que o layout sorteou.
# fator = refletância real / média da cor no WebGL
ALBEDO = {
    0: 0.17 / 0.067,   # chão de terra batida
    1: 0.35 / 0.093,   # concreto (os galpões do fundo, mais escuros, ficam ~0.2)
    2: 1.6,            # tambores pintados: a tinta já tinha cor, só escurecida
    3: 0.50 / 0.072,   # aço (cor de reflexo do aço ~0.5)
    4: 0.25 / 0.085,   # entulho
}


def physical_albedo(mat_id, color):
    k = ALBEDO[mat_id]
    return [min(c * k, 0.9) for c in color]


def piece(mesh, mat, pos, scale, quat, color, rough, metal, folder, cylinder=False):
    center = list(pos)
    if cylinder:
        # cilindro do WebGL tem a base em y=0; o da Unreal é centrado
        half = rotate(quat, [0.0, 0.5 * scale[1], 0.0])
        center = [pos[i] + half[i] for i in range(3)]
    # spawn_actor_from_object passa pela seleção do editor, que não existe
    # sem janela (o commandlet trava); ator vazio + malha funciona
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, ue_pos(center), ue_quat(quat).rotator())
    a.set_actor_scale3d(unreal.Vector(scale[0], scale[2], scale[1]))
    c = a.static_mesh_component
    c.set_static_mesh(mesh)
    c.set_material(0, mat)
    c.set_default_custom_primitive_data_vector4(0, unreal.Vector4(color[0], color[1], color[2], rough))
    c.set_default_custom_primitive_data_float(4, metal)
    a.set_folder_path(folder)
    return a


def build():
    # mapa: cria ou esvazia
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        levels.load_level(MAP)
        for a in actors.get_all_level_actors():
            actors.destroy_actor(a)
        log('map emptied')
    else:
        levels.new_level(MAP)
        log('map created')

    mat = base_material()
    cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    cyl = unreal.load_asset('/Engine/BasicShapes/Cylinder.Cylinder')
    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')

    with open(os.path.join(TOOLS, 'layout.json'), encoding='utf-8') as f:
        L = json.load(f)
    for v in L['boxes']:
        t = int(v[7])
        piece(cube, mat, v[0:3], v[4:7], v[12:16], physical_albedo(t, v[8:11]), v[11], METAL[t], 'Scenery/Boxes')
    for v in L['cylinders']:
        t = int(v[7])
        piece(cyl, mat, v[0:3], v[4:7], v[12:16], physical_albedo(t, v[8:11]), v[11], METAL[t], 'Scenery/Cylinders',
              cylinder=True)
    # Chão até o horizonte: com os 1.4 km do WebGL, o "planeta" da Sky
    # Atmosphere aparecia como uma faixa azul-escura depois da borda.
    # 20 km some na névoa antes da borda. (o Plane da engine tem 1 m)
    g = L['ground']
    ground = piece(plane, ground_material(), [0, 0, 0], [20000, 1, 20000], [0, 0, 0, 1],
                   physical_albedo(0, g[8:11]), g[11], 0.0, 'Scenery')
    ground.set_actor_label('Ground')
    log('scenery', len(L['boxes']), 'boxes', len(L['cylinders']), 'cylinders')

    # ---- céu e luz -----------------------------------------------------------
    # Sol às 15h do protótipo (direção medida no WebGL, pra o sol):
    # (−0.649, 0.436, −0.623) → a luz anda no sentido oposto
    to_sun = ue_pos([-0.6488, 0.4363, -0.6235])
    light_dir = unreal.Vector(-to_sun.x, -to_sun.y, -to_sun.z)
    sun_rot = unreal.MathLibrary.make_rot_from_x(light_dir)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), sun_rot)
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property('intensity', 128000.0)          # lux no topo da atmosfera (SUN_LUX do WebGL)
    lc.set_editor_property('atmosphere_sun_light', True)
    lc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)

    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label('HeightFog')
    # A névoa de altura do WebGL: 0.0016/m no chão, caindo 0.09/m. A Unreal
    # divide os dois por 1000 por centímetro (SceneCore.cpp), ou seja, o valor
    # do editor é 10× o "por metro".
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.016)
    fc.set_editor_property('fog_height_falloff', 0.9)
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    exposure(pp, physical_ev100(sun_elevation_deg=math.degrees(math.asin(0.4363))))
    for a in (sun, sky, sl, fog, pp):
        a.set_folder_path('Environment')

    # ---- câmera: o enquadramento padrão do WebGL ------------------------------
    # dist 62 m, azim −0.62, elev 0.36, alvo a 2 m; fov vertical 0.58 rad em 16:9
    camera(label='DefaultCam', dist=62, azim=-0.62, elev=0.36, target_y=2.0)
    # a de lado, na altura da fumaça (a das folhas de teste do avião)
    camera(label='SideCam', dist=72, azim=-0.62, elev=0.10, target_y=30.0)

    levels.save_current_level()
    log('map saved')

    cams = {a.get_actor_label(): a for a in actors.get_all_level_actors()
            if isinstance(a, unreal.CineCameraActor)}
    sequence('Seq_Scene', cams['DefaultCam'], seconds=2.0)
    sequence('Seq_Side', cams['SideCam'], seconds=2.0)
    render_configs()
    unreal.EditorAssetLibrary.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('all saved')


def physical_ev100(sun_elevation_deg, sun_lux=128000.0, sky_lux=15000.0, transmittance=0.85):
    """EV100 de um fotômetro incidente (constante C = 250) pela iluminância
    horizontal: sol·sen(elevação)·transmitância da atmosfera + céu difuso.
    É a "sunny 16" dos fotógrafos (Lagarde & de Rousiers, Moving Frostbite to
    PBR, 2014): ~15 com sol a pino, ~14.3 às 15h."""
    e = sun_lux * math.sin(math.radians(sun_elevation_deg)) * transmittance + sky_lux
    return math.log2(e * 100.0 / 250.0)


def exposure(pp, ev100):
    """Exposição automática presa numa faixa em torno da física: pode abrir
    meio stop numa cena escura e fechar até 3.5 stops no clarão de uma
    explosão (o mesmo papel da adaptação do WebGL). Velocidades da adaptação
    do WebGL (js/main.js: ataque 0.08 s, soltura 1.6 s): fecha ~4 stops em
    ~0.1 s no clarão e reabre devagar."""
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ('auto_exposure_min_brightness', ev100 - 0.5),
                        ('auto_exposure_max_brightness', ev100 + 3.5),
                        ('auto_exposure_bias', 0.0),
                        ('auto_exposure_speed_up', 40.0),
                        ('auto_exposure_speed_down', 2.5),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        # bloom do WebGL (js/main.js): só acima de 1.7 (cena
                        # exposta), 5.2% — o padrão da Unreal (0.675, sem
                        # limiar) cobria o fogo com um véu e ele saía pálido.
                        # 0.052 / ~0.8 (soma dos pesos das 6 camadas da Unreal)
                        ('bloom_threshold', 1.7),
                        ('bloom_intensity', 0.065)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    wb = unreal.WeightedBlendable()
    wb.set_editor_property('weight', 1.0)
    wb.set_editor_property('object', tonemap_material())
    blend = unreal.WeightedBlendables()
    blend.set_editor_property('array', [wb])
    s.set_editor_property('weighted_blendables', blend)
    pp.set_editor_property('settings', s)
    log('exposure EV100', round(ev100, 2))


# ---- tonemapper do WebGL (js/post.js + js/glsl.js) -----------------------------
# A curva padrão da Unreal é aplicada por canal e dessatura o que estoura em
# direção ao branco: a bola de fogo saía branco-amarelada onde no WebGL é
# laranja (comparação lado a lado, 0.4–0.8 s). O WebGL devolve parte da
# crominância original sobre a luminância comprimida (ARCHITECTURE §4e) — o
# que o usuário aprovou. Material no lugar do tonemapper.
# MEDIDO (versão de depuração que escreve log2 dos valores na imagem, com
# TONEMAP_DEBUG=1): a cena chega em unidades físicas (chão ~1500 cd/m²), sem
# a pré-exposição; o nó de exposição dá 2^−14.2 (o EV100 da faixa); e a
# engine aplica a curva sRGB DEPOIS do material — os três só batem decodificando
# a imagem com a sRGB. Pelo fonte parecia o contrário (pré-exposição na
# entrada, saída direto na tela): a primeira versão estourou tudo pra branco.
TONEMAP = '/Game/Materials/M_TonemapWebGL'
# Sem fator extra de exposição: medido às 15h, a Unreal expõe 6.8 por unidade
# de radiância do render (128 000 × 2^−14.2) contra 17.4 do WebGL — e o chão
# aqui reflete 17% contra 6.7% lá. 6.8 × 17% ≈ 17.4 × 6.7%: o chão sai na tela
# com o mesmo brilho. (Uma primeira versão usava 4.05 e estourava a cena.)
GRADE = dict(hue=0.45, contrast=1.06, saturation=1.16, vignette=0.38)   # js/main.js env


def tonemap_material():
    if unreal.EditorAssetLibrary.does_asset_exist(TONEMAP):
        unreal.EditorAssetLibrary.delete_asset(TONEMAP)
    m = asset_tools.create_asset('M_TonemapWebGL', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property('blendable_location', unreal.BlendableLocation.BL_REPLACING_TONEMAPPER)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    scene = E(unreal.MaterialExpressionSceneTexture, -700, -100)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    bloom = E(unreal.MaterialExpressionSceneTexture, -700, 50)     # bloom combinado (PostProcessing.cpp)
    bloom.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT2)
    expo = E(unreal.MaterialExpressionEyeAdaptation, -700, 200)
    screen = E(unreal.MaterialExpressionScreenPosition, -700, 300)
    c = E(unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('description', 'TonemapWebGL')
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for n in ('Scene', 'Bloom', 'Exposure', 'UV'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    c.set_editor_property('inputs', inputs)
    p = GRADE
    if os.environ.get('TONEMAP_DEBUG'):
        # canais = log2 dos valores, codificados em [0, 1]
        c.set_editor_property('code', '\n'.join([
            'float lum = max(dot(Scene.rgb, float3(0.2126, 0.7152, 0.0722)), 1e-8);',
            'return float3(log2(max(Exposure, 1e-12)) / 64.0 + 0.5, log2(View.OneOverPreExposure) / 64.0 + 0.5, log2(lum) / 64.0 + 0.5);',
        ]))
    else:
        c.set_editor_property('code', '\n'.join([
            'float3 col = (Scene.rgb + Bloom.rgb) * Exposure;',
            'const float3 L709 = float3(0.2126, 0.7152, 0.0722);',
            '// ACES fit de Stephen Hill (RRT+ODT), como js/glsl.js tonemapACES',
            'const float3x3 RRT_IN = float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);',
            'const float3x3 ODT_OUT = float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);',
            'float3 v = mul(RRT_IN, col);',
            'v = (v * (v + 0.0245786) - 0.000090537) / (v * (0.983729 * v + 0.4329510) + 0.238081);',
            'float3 tm = saturate(mul(ODT_OUT, v));',
            '// preservação de matiz: crominância original sobre a luminância comprimida',
            'float lIn = max(dot(col, L709), 1e-4);',
            'float3 hueKept = saturate(col / lIn * dot(tm, L709));',
            'col = lerp(tm, hueKept, %r);' % p['hue'],
            '// grade: contraste em log, saturação',
            'col = pow(max(col, 0.0), %r);' % p['contrast'],
            'float l = dot(col, L709);',
            'col = lerp(l.xxx, col, %r);' % p['saturation'],
            '// vinheta natural (cos^4 suavizado)',
            'float2 d = UV - 0.5;',
            'col *= 1.0 - %r * smoothstep(0.05, 0.85, dot(d, d));' % p['vignette'],
            '// linear: a engine aplica a sRGB depois',
            'return saturate(col);',
        ]))
    link(scene, 'Color', c, 'Scene')
    link(bloom, 'Color', c, 'Bloom')
    link(expo, '', c, 'Exposure')
    link(screen, 'ViewportUV', c, 'UV')
    ok = mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if failed or not ok:
        raise RuntimeError('tonemap: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


FPS = 30


def sequence(name, cam, seconds):
    """Sequência com um corte de câmera, recriada do zero."""
    path = '/Game/Sequences/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    seq = asset_tools.create_asset(name, '/Game/Sequences', unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    end = int(round(seconds * FPS))
    seq.set_playback_start(0)
    seq.set_playback_end(end)
    binding = seq.add_possessable(cam)
    cut = seq.add_track(unreal.MovieSceneCameraCutTrack)
    sec = cut.add_section()
    sec.set_range(0, end)
    sec.set_camera_binding_id(seq.get_binding_id(binding))
    unreal.EditorAssetLibrary.save_loaded_asset(seq)
    log('sequence', name, end, 'frames')
    return seq


def render_config(name, game_quality):
    """Preset do Movie Render Queue: JPG 1280×720 em captures/ue/<sequência>/.
    game_quality: sem isto o Movie Render Queue sobe tudo pra Cinematic (o
    volume em resolução cheia com 1024 passos) — bom pra ver o máximo, errado
    pra medir o custo do que o jogador vê."""
    path = '/Game/Render/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    cfg = asset_tools.create_asset(name, '/Game/Render', unreal.MoviePipelinePrimaryConfig,
                                   unreal.MoviePipelinePrimaryConfigFactory())
    out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    folder = os.path.join(REPO, 'captures', 'ue', '{sequence_name}').replace('\\', '/')
    out.set_editor_property('output_directory', unreal.DirectoryPath(folder))
    out.set_editor_property('file_name_format', '{sequence_name}.{frame_number}')
    out.set_editor_property('output_resolution', unreal.IntPoint(1280, 720))
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_JPG)
    # quadros de aquecimento: a exposição automática e o TSR precisam de
    # histórico antes do primeiro quadro gravado
    aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
    aa.set_editor_property('engine_warm_up_count', 60)
    aa.set_editor_property('render_warm_up_frames', True)
    if game_quality:
        go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
        go.set_editor_property('cinematic_quality_settings', False)
    unreal.EditorAssetLibrary.save_loaded_asset(cfg)
    log('render config', name, folder)


def render_configs():
    render_config('Config_Test', game_quality=False)       # visual máximo
    render_config('Config_Profile', game_quality=True)     # custo na qualidade de jogo


if __name__ == '__main__':
    build()
