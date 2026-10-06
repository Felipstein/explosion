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


def new_material(path):
    """Material vazio em path. Reaproveita o asset que já existe e só limpa os
    nós: apagar e recriar falha quando o simulador está aberto com ele
    carregado (05/10: 'M_Base already exists')."""
    folder, name = path.rsplit('/', 1)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


# ---- material único: cor/rugosidade/metal por peça (Custom Primitive Data) --
def base_material():
    m = new_material(BASE_MATERIAL)
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
    ta = import_texture('ground_A.png', 'T_Ground_A', srgb=True)
    tn = import_texture('ground_N.png', 'T_Ground_N', srgb=False, normal=True)
    m = new_material(path)
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
    for c, v in (('r', True), ('g', True), ('b', False), ('a', False)):
        xy.set_editor_property(c, v)
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
    # albedo × (0.72 + 0.62·m), m ∈ [0, 1] (o canal r vai de ~0.027 a ~0.075)
    uv3 = E(unreal.MaterialExpressionDivide, -1050, -500)
    uv3.set_editor_property('const_b', 1800.0 * 40.0)
    link(xy, '', uv3, 'A')
    large = sample(ta, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -500, uv3)
    tone = E(unreal.MaterialExpressionCustom, -450, -500)
    inp = unreal.CustomInput()
    inp.set_editor_property('input_name', 'R')
    tone.set_editor_property('inputs', [inp])
    tone.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    tone.set_editor_property('code', 'return 0.72 + 0.62 * saturate((R - 0.027) / 0.048);')
    link(large, 'R', tone, 'R')
    color = E(unreal.MaterialExpressionMultiply, -200, -300)
    rgb = E(unreal.MaterialExpressionComponentMask, -320, -260)
    rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True)
    link(la, '', rgb, '')
    link(rgb, '', color, 'A'); link(tone, '', color, 'B')
    # a máscara nasce com r e g ligados: só o alfa (a rugosidade). Ligado só o
    # 'a', a saída era (r, g, a) e a rugosidade virava o vermelho do albedo
    # (~0.05): o chão saiu quase espelhado até 05/10, refletindo o céu no
    # rasante (chão longe 2–4× mais claro e azul que o WebGL)
    rough = E(unreal.MaterialExpressionComponentMask, -200, -120)
    for c, v in (('r', False), ('g', False), ('b', False), ('a', True)):
        rough.set_editor_property(c, v)
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

# Refletância: as cores do próprio WebGL (terra escura a 6.7%, dentro da faixa
# de solo real, 5–15%). Uma versão anterior levava cada tipo à média "física"
# (solo 0.17, concreto 0.35) e escurecia a exposição 1.8 stop pra compensar —
# o chão saía igual, mas o céu, que não tem albedo, ficava 1.8 stop mais
# escuro que o aprovado (medido 05/10 com a câmera do WebGL). A exposição agora
# é a do WebGL (time_of_day_table), então as cores também são as dele.


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
        piece(cube, mat, v[0:3], v[4:7], v[12:16], v[8:11], v[11], METAL[t], 'Scenery/Boxes')
    for v in L['cylinders']:
        t = int(v[7])
        piece(cyl, mat, v[0:3], v[4:7], v[12:16], v[8:11], v[11], METAL[t], 'Scenery/Cylinders', cylinder=True)
    # Chão até o horizonte: com os 1.4 km do WebGL, o "planeta" da Sky
    # Atmosphere aparecia como uma faixa azul-escura depois da borda.
    # 20 km some na névoa antes da borda. (o Plane da engine tem 1 m)
    g = L['ground']
    ground = piece(plane, ground_material(), [0, 0, 0], [20000, 1, 20000], [0, 0, 0, 1],
                   g[8:11], g[11], 0.0, 'Scenery')
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
    # canais 0 (cenário) e 1 (explosões): as explosões escutam só o 1
    lc.set_editor_property('lighting_channels', channels(True, True))
    # a lua: segunda luz da atmosfera (o céu da noite vem dela), mexida pela
    # hora do dia (BlastLabEnvironment)
    moon = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1100), sun_rot)
    moon.set_actor_label('Moon')
    mc = moon.get_component_by_class(unreal.DirectionalLightComponent)
    mc.set_editor_property('intensity', 0.0)
    mc.set_editor_property('atmosphere_sun_light', True)
    mc.set_editor_property('atmosphere_sun_light_index', 1)
    mc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    mc.set_editor_property('lighting_channels', channels(True, True))

    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    # o meio é o padrão da Unreal, que é o do WebGL (atmosphere.js MEDIUM); só o
    # albedo do planeta difere: 0.12/0.11/0.10 lá (a cor é sRGB: 98/94/89).
    # O espalhamento múltiplo varia com a hora: BlastLabEnvironment
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    sc.set_editor_property('ground_albedo', unreal.Color(r=98, g=94, b=89, a=255))
    # Sem perspectiva aérea nas superfícies: no WebGL quem colore a distância é
    # só a névoa (com a cor do céu abaixo do horizonte); somada a ela, a da
    # atmosfera deixava o chão distante 0.6–1.2 stop mais claro e azul (medido
    # 05/10 contra as referências na mesma câmera). O céu não usa isso.
    sc.set_editor_property('aerial_pespective_view_distance_scale', 0.0)
    # e sem a luz da atmosfera somada à névoa (céu médio + sol no chão,
    # HeightFogCommon.ush): a névoa é só a do WebGL (BlastLabEnvironment)
    sc.set_editor_property('height_fog_contribution', 0.0)
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
    # A cor é a do WebGL, hora a hora (BlastLabEnvironment, Tools/webgl_fog.json):
    # lá a névoa pega o céu da LUT na direção do pixel, que abaixo do horizonte
    # é quase neutro à noite, roxo no crepúsculo e laranja do lado do sol. A da
    # Unreal somaria a média do céu (azul) — desligada.
    fc.set_editor_property('sky_atmosphere_ambient_contribution_color_scale', unreal.LinearColor(0, 0, 0, 1))
    fc.set_editor_property('directional_inscattering_start_distance', 0.0)
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    exposure(pp, webgl_ev100(15.0))
    # hora do dia: move sol e lua (Source/BlastLab/Public/BlastLabEnvironment.h);
    # começa às 15h, a hora em que a cena foi calibrada contra o WebGL
    env = actors.spawn_actor_from_class(unreal.BlastLabEnvironment, unreal.Vector(0, 0, 0))
    env.set_actor_label('Environment')
    for prop, value in (('sun', sun), ('moon', moon), ('atmosphere', sky), ('post_process', pp), ('fog', fog),
                        ('time_of_day', 15.0), ('params', environment_params()),
                        ('exposure_mask', exposure_mask_material())):
        env.set_editor_property(prop, value)
    time_of_day_table(env)
    dome = sky_dome()
    for a in (sun, moon, sky, sl, fog, pp, env, dome):
        a.set_folder_path('Environment')
    # o simulador (painel, câmera RTS, clique pra detonar); o modo do mapa
    # vence o padrão do projeto, então fica explícito aqui também
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.BlastLabGameMode)

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
    thumbnail()
    unreal.EditorAssetLibrary.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('all saved')


ENV_PARAMS = '/Game/Materials/MPC_Environment'
SUN_LUX = 128000.0


def webgl_fog():
    with open(os.path.join(TOOLS, 'webgl_fog.json'), encoding='utf-8') as f:
        return json.load(f)



def webgl_rows():
    with open(os.path.join(TOOLS, 'webgl_time_of_day.json'), encoding='utf-8') as f:
        return json.load(f)['rows']                    # hours, sunAlt, moonAlt, expo, lux, eRender, purkinje


def webgl_ev100(hours):
    """A exposição do WebGL naquela hora em EV100 da Unreal. O WebGL multiplica
    a radiância do render (sol = 1) por expo; aqui a radiância está em cd/m²
    (render × 128 000) e, com r.EyeAdaptation.LensAttenuation = 0.78 (padrão),
    a exposição da Unreal é 2^−EV100. Então EV100 = log2(128 000 / expo)."""
    r = min(webgl_rows(), key=lambda r: abs(r[0] - hours))
    return math.log2(SUN_LUX / r[3])


def environment_params():
    """Parâmetros da hora que o C++ (BlastLabEnvironment) escreve e os
    materiais leem: Purkinje (tonemapper) e StarGain (céu)."""
    if unreal.EditorAssetLibrary.does_asset_exist(ENV_PARAMS):
        return unreal.load_asset(ENV_PARAMS)
    c = asset_tools.create_asset('MPC_Environment', '/Game/Materials', unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    scalars = []
    for name, value in (('Purkinje', 1.0e5), ('StarGain', 1.0)):
        p = unreal.CollectionScalarParameter()
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        scalars.append(p)
    c.set_editor_property('scalar_parameters', scalars)
    unreal.EditorAssetLibrary.save_loaded_asset(c)
    return c


def time_of_day_table(env):
    """A exposição, o Purkinje, o reforço da noite e o céu, hora a hora, gravados
    do próprio WebGL (Tools/webgl_time_of_day.json: o texel 4 da LUT de
    ambiente de atmosphere.js em cada hora):
    - EV100(t) = log2(128 000 / exposição_WebGL(t)) (webgl_ev100);
    - Purkinje: o WebGL escala a radiância do render; aqui ela está em cd/m²
      (render × 128 000), então a escala vai dividida por 128 000;
    - ganho das estrelas = E_render·128 000 / E_lux: 1 de dia, ~880 com a lua
      (as fontes da noite reforçadas como ela, glsl.js skyRadiance);
    - espalhamento múltiplo do céu (Tools/webgl_sky_ms.json, sky_ms_table.py):
      o do WebGL é ~4× o de Hillaire por um desvio na LUT dele, e o céu
      aprovado é esse — medido 05/10 com a câmera do WebGL às 15h, o céu da
      Unreal saía 0.9–1.1 stop abaixo, igual ao Hillaire correto calculado à
      parte (11.44 contra 11.42 em log2 cd/m²)."""
    rows = webgl_rows()
    with open(os.path.join(TOOLS, 'webgl_sky_ms.json'), encoding='utf-8') as f:
        ms = json.load(f)['rows']                      # hours, multiScatteringFactor
    assert len(ms) == len(rows) and all(abs(a[0] - b[0]) < 1e-6 for a, b in zip(rows, ms))
    env.set_editor_property('curve_hours', [float(r[0]) for r in rows])
    env.set_editor_property('curve_ev100', [math.log2(SUN_LUX / r[3]) for r in rows])
    env.set_editor_property('curve_purkinje', [r[6] / SUN_LUX for r in rows])
    env.set_editor_property('curve_star_gain', [max(r[5] * SUN_LUX / max(r[4], 1e-9), 1.0) for r in rows])
    env.set_editor_property('curve_render_lux', [r[5] * SUN_LUX for r in rows])
    env.set_editor_property('curve_multi_scattering', [float(m[1]) for m in ms])
    fog = webgl_fog()['rows']                          # hours, fog rgb, directional rgb (cd/m²)
    assert len(fog) == len(rows) and all(abs(a[0] - b[0]) < 1e-6 for a, b in zip(rows, fog))
    env.set_editor_property('curve_fog', [unreal.LinearColor(r[1], r[2], r[3], 1.0) for r in fog])
    env.set_editor_property('curve_fog_directional', [unreal.LinearColor(r[4], r[5], r[6], 1.0) for r in fog])
    env.set_editor_property('curve_fog_exponent', [float(r[7]) for r in fog])
    log('time of day table', len(rows), 'rows')


def channels(c0, c1):
    lc = unreal.LightingChannels()
    lc.set_editor_property('channel0', c0)
    lc.set_editor_property('channel1', c1)
    lc.set_editor_property('channel2', False)
    return lc


def thumbnail():
    """Miniatura dos cartões do arsenal: o render do próprio WebGL
    (reference/webgl/assets/thumb_blast.png). Textura de interface: sem mipmap
    e sempre carregada (o pincel do painel não espera streaming)."""
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(REPO, 'reference', 'webgl', 'assets', 'thumb_blast.png'))
    t.set_editor_property('destination_path', '/Game/UI')
    t.set_editor_property('destination_name', 'T_ThumbBlast')
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    tex = unreal.load_asset('/Game/UI/T_ThumbBlast')
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    log('thumbnail', tex.get_path_name())


# Limiar da adaptação às explosões, como o alvo do WebGL (js/exposure.js
# aeTarget 0.45, "acima de qualquer cena sem explosão"). O histograma da
# Unreal leva a média exposta a 0.18·2^bias; com o piso da faixa em EV + bias,
# a exposição em repouso é exatamente a da hora (2^bias / 2^(EV+bias)) e só
# fecha quando a cena passa de 0.18·2^bias. O histograma, diferente do WebGL,
# também mede o céu, então o limiar vem medido nas vistas sem explosão.
# Calibrado 05/10 contra o medidor do WebGL (tanque no centro, câmera RTS e a
# baixa das referências, 5h21–22h30), com o céu fora da medição
# (exposure_mask_material) e a média da metade mais clara da tela (50–98%, o
# papel da média de potência p=0.5 do WebGL). O limiar é também onde a câmera
# assenta depois do clarão: alto demais e a explosão da noite fica estourada.
# A cena mais clara sem explosão mede 0.38 (manhã, câmera baixa) → limiar
# 0.18·2^1.5 = 0.51, 0.4 stop de folga; o tanque à noite na câmera RTS mede 13
# e fecha 4.7 stops, como o WebGL. (Com o chão espelhado do bug da rugosidade a
# cena parada media 1.06 e o limiar tinha subido pra 2.5: a noite estourava.)
AE_BIAS = 1.5
AE_LOW_PERCENT, AE_HIGH_PERCENT = 50.0, 98.0


def bloom_stages():
    """O bloom do WebGL (js/post.js) é uma pirâmide de 7 níveis somados com o
    MESMO peso (o upsample é aditivo): força 0.052 por nível, 0.364 no total. Até
    06/10 o porte usava 0.052 no total, ~7× mais fraco (o traçante e o fogo
    quase sem halo). Cada nível i é a imagem reduzida 2^(i+1)× pelo filtro de 13
    amostras (variância 1.5 texel² por eixo, texel da origem = 2^j px) e devolvida
    por tendas 1-2-1 de raio 1.15 texel (variância 0.5·1.15²): σ_i somando as
    variâncias. Na Unreal cada estágio é uma gaussiana exp(−16.7·(x/R)²),
    R = largura·tamanho%·½ (PostProcessWeightedSampleSum.cpp), σ = R/5.78: o
    tamanho% = σ/largura·1156. Seis estágios: os dois níveis menores juntos."""
    sig = []
    var = 0.0
    for i in range(7):
        var += 1.5 * 4.0 ** i                            # reduções até o nível i (px da tela cheia)
        up = sum(0.5 * (1.15 * 2.0 ** (k + 1)) ** 2 for k in range(1, i + 1))
        sig.append(math.sqrt(var + up))
    width = 1600.0                                       # a razão σ/largura não depende da resolução
    size = [s / width * 1156.0 for s in sig]
    merged = math.sqrt((sig[0] ** 2 + sig[1] ** 2) / 2.0) / width * 1156.0
    stages = [(merged, 2.0 / 7.0)] + [(size[i], 1.0 / 7.0) for i in range(2, 7)]
    return dict(intensity=0.052 * 7.0, stages=stages)


BLOOM = bloom_stages()


def exposure(pp, ev100):
    """Exposição automática: a da hora (BlastLabEnvironment escreve a faixa a
    cada mudança de hora) e a adaptação às explosões, que só escurece. Quanto
    ela pode fechar também é do WebGL (exposure.js): meio stop de dia, até a
    exposição do meio-dia à noite. Velocidades da adaptação do WebGL
    (js/main.js: ataque 0.08 s, soltura 1.6 s): fecha ~4 stops em ~0.1 s no
    clarão e reabre devagar."""
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ('auto_exposure_min_brightness', ev100 + AE_BIAS),
                        ('auto_exposure_max_brightness', ev100 + AE_BIAS + 1.0),
                        ('auto_exposure_bias', AE_BIAS),
                        ('auto_exposure_low_percent', AE_LOW_PERCENT),
                        ('auto_exposure_high_percent', AE_HIGH_PERCENT),
                        ('auto_exposure_speed_up', 40.0),
                        ('auto_exposure_speed_down', 2.5),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        # bloom: BLOOM (o limiar segue a exposição, BlastLabEnvironment)
                        ('bloom_method', unreal.BloomMethod.BM_SOG),
                        ('bloom_threshold', 1.7),
                        ('bloom_intensity', BLOOM['intensity']),
                        ('bloom_size_scale', 1.0)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    for i, (size, tint) in enumerate(BLOOM['stages'], 1):
        s.set_editor_property('override_bloom%d_size' % i, True)
        s.set_editor_property('bloom%d_size' % i, size)
        s.set_editor_property('override_bloom%d_tint' % i, True)
        s.set_editor_property('bloom%d_tint' % i, unreal.LinearColor(tint, tint, tint, 1.0))
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
# a pré-exposição; o nó de exposição dá 2^−14.2 (o EV100 da faixa). A saída
# vai pra tela como está — tem que sair em sRGB: quem codificava (e enganou a
# primeira medição) era a gravação de imagem do Movie Render Queue; na janela
# ao vivo a saída linear deixava tudo ~3 EV mais escuro (medido 05/10).
TONEMAP = '/Game/Materials/M_TonemapWebGL'
# Sem fator extra de exposição: medido às 15h, a Unreal expõe 6.8 por unidade
# de radiância do render (128 000 × 2^−14.2) contra 17.4 do WebGL — e o chão
# aqui reflete 17% contra 6.7% lá. 6.8 × 17% ≈ 17.4 × 6.7%: o chão sai na tela
# com o mesmo brilho. (Uma primeira versão usava 4.05 e estourava a cena.)
GRADE = dict(hue=0.45, contrast=1.06, saturation=1.16, vignette=0.38)   # js/main.js env


def tonemap_material():
    m = new_material(TONEMAP)
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
    purk = E(unreal.MaterialExpressionCollectionParameter, -700, 400)    # escrito pela hora do dia (C++)
    purk.set_editor_property('collection', environment_params())
    purk.set_editor_property('parameter_name', 'Purkinje')
    c = E(unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('description', 'TonemapWebGL')
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for n in ('Scene', 'Bloom', 'Exposure', 'UV', 'PurkS'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    c.set_editor_property('inputs', inputs)
    p = GRADE
    if os.environ.get('TONEMAP_DEBUG') == 'rgb':
        # canais = log2 da radiância (cd/m²) de cada canal, codificados em [0, 1]
        c.set_editor_property('code', 'return log2(max(Scene.rgb, 1e-8)) / 64.0 + 0.5;')
    elif os.environ.get('TONEMAP_DEBUG'):
        # canais = log2 dos valores, codificados em [0, 1]
        c.set_editor_property('code', '\n'.join([
            'float lum = max(dot(Scene.rgb, float3(0.2126, 0.7152, 0.0722)), 1e-8);',
            'return float3(log2(max(Exposure, 1e-12)) / 64.0 + 0.5, log2(View.OneOverPreExposure) / 64.0 + 0.5, log2(lum) / 64.0 + 0.5);',
        ]))
    else:
        c.set_editor_property('code', '\n'.join([
            '// Purkinje shift (post.js; Patry 2021 sobre Cao et al. 2008): no escuro os',
            '// bastonetes somam um sinal próprio — os escuros clareiam, azulados e',
            '// dessaturados; com luz o ganho dos cones cai e o efeito some. Na radiância',
            '// da cena, antes da exposição. Matrizes de tools/purkinje.py (GLSL mat3 é',
            '// por coluna: mul(v, M) com os mesmos 9 números dá o mesmo produto).',
            'const float3x3 PURK_G = float3x3(0.147629, 0.0751296, 0.00385728, 0.418795, 0.691496, 0.0537957, 0.0720858, 0.145298, 0.136669);',
            'const float3 PURK_R = float3(0.0107999, 0.622593, 0.39211);',
            'const float3x3 PURK_D = float3x3(8.2769, 2.28736, 16.2269, -9.45385, 7.05768, 16.0246, 0.241254, -0.233032, 1.50034);',
            'float3 c = max(Scene.rgb + Bloom.rgb, 0.0);',
            'float3 g = rsqrt(1.0 + PurkS * mul(c, PURK_G));',
            'c = max(c + mul(g, PURK_D) * dot(PURK_R, c), 0.0);',
            'float3 col = c * Exposure;',
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
            '// saída já codificada em sRGB (a tela): na janela ao vivo a engine',
            '// mostra o que o tonemapper entrega, sem codificar (medido 05/10: a',
            '// saída linear deixava tudo ~3 EV mais escuro). Quem codificava era',
            '// a gravação de imagem do Movie Render Queue.',
            'col = saturate(col);',
            'return (col <= 0.0031308) ? col * 12.92 : 1.055 * pow(col, 1.0 / 2.4) - 0.055;',
        ]))
    link(scene, 'Color', c, 'Scene')
    link(bloom, 'Color', c, 'Bloom')
    link(expo, '', c, 'Exposure')
    link(screen, 'ViewportUV', c, 'UV')
    link(purk, '', c, 'PurkS')
    ok = mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if failed or not ok:
        raise RuntimeError('tonemap: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


# ---- abóbada do céu: estrelas, Via Láctea, brilho do céu noturno, lua ----------
SKY = '/Game/Materials/M_Sky'
# Porte de js/glsl.js (ATMOS: starField, milkyWay, skyRadiance, celestialDisks).
# As fontes da noite estão em cd/m² físicos; o WebGL as reforça como a lua do
# render (880×) e as apaga sob o céu de sol pela razão céu-do-render/céu-físico
# por pixel. Aqui a mesma razão vem por hora (StarGain = E_render·128000/E_lux,
# BlastLabEnvironment), que é 880 com a lua e 1 de dia. Coordenadas do WebGL
# (y pra cima) dentro do código: d = (x, z, y) da Unreal.
SKY_HLSL = r'''
struct FNight
{
    float hash11(float p) { p = frac(p * 0.1031); p *= p + 33.33; p *= p + p; return frac(p); }
    float hash12(float2 p) { float3 p3 = frac(p.xyx * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.x + p3.y) * p3.z); }
    float3 hash33(float3 p)
    {
        p = float3(dot(p, float3(127.1, 311.7, 74.7)), dot(p, float3(269.5, 183.3, 246.1)), dot(p, float3(113.5, 271.9, 124.6)));
        return frac(sin(p) * 43758.5453123) * 2.0 - 1.0;
    }
    float3 mod289v3(float3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
    float4 mod289v4(float4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
    float4 permute(float4 x) { return mod289v4(((x * 34.0) + 1.0) * x); }
    // simplex 3D (Ashima Arts / Gustavson), como js/glsl.js
    float snoise(float3 v)
    {
        const float2 C = float2(1.0 / 6.0, 1.0 / 3.0);
        const float4 D = float4(0.0, 0.5, 1.0, 2.0);
        float3 i = floor(v + dot(v, C.yyy));
        float3 x0 = v - i + dot(i, C.xxx);
        float3 g_ = step(x0.yzx, x0.xyz);
        float3 l_ = 1.0 - g_;
        float3 i1 = min(g_.xyz, l_.zxy);
        float3 i2 = max(g_.xyz, l_.zxy);
        float3 x1 = x0 - i1 + C.xxx;
        float3 x2 = x0 - i2 + C.yyy;
        float3 x3 = x0 - D.yyy;
        i = mod289v3(i);
        float4 p = permute(permute(permute(i.z + float4(0.0, i1.z, i2.z, 1.0)) + i.y + float4(0.0, i1.y, i2.y, 1.0)) + i.x + float4(0.0, i1.x, i2.x, 1.0));
        float n_ = 0.142857142857;
        float3 ns = n_ * D.wyz - D.xzx;
        float4 j = p - 49.0 * floor(p * ns.z * ns.z);
        float4 x_ = floor(j * ns.z);
        float4 y_ = floor(j - 7.0 * x_);
        float4 x = x_ * ns.x + ns.yyyy;
        float4 y = y_ * ns.x + ns.yyyy;
        float4 h = 1.0 - abs(x) - abs(y);
        float4 b0 = float4(x.xy, y.xy);
        float4 b1 = float4(x.zw, y.zw);
        float4 s0 = floor(b0) * 2.0 + 1.0;
        float4 s1 = floor(b1) * 2.0 + 1.0;
        float4 sh = -step(h, 0.0);
        float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
        float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
        float3 p0 = float3(a0.xy, h.x);
        float3 p1 = float3(a0.zw, h.y);
        float3 p2 = float3(a1.xy, h.z);
        float3 p3 = float3(a1.zw, h.w);
        float4 nrm = 1.79284291400159 - 0.85373472095314 * float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3));
        p0 *= nrm.x; p1 *= nrm.y; p2 *= nrm.z; p3 *= nrm.w;
        float4 m = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
        m = m * m;
        return 42.0 * dot(m * m, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
    }
    float fbm(float3 p, int oct, float lac, float gain)
    {
        float s = 0.0, a = 0.5, n = 0.0;
        for (int i = 0; i < oct; i++) { s += a * snoise(p); n += a; p *= lac; a *= gain; }
        return s / max(n, 1e-4);
    }
    float billow(float3 p, int oct, float lac, float gain)
    {
        float s = 0.0, a = 0.5, n = 0.0;
        for (int i = 0; i < oct; i++) { s += a * abs(snoise(p)); n += a; p *= lac; a *= gain; }
        return s / max(n, 1e-4);
    }
    // estrelas: lei de potência numa grade de direções; a mais forte ≈ Sírius (~65 cd/m²)
    // pix: tamanho do pixel em unidades da grade. O núcleo do WebGL (0.085 da
    // célula, ~1') é menor que um pixel: o anti-aliasing temporal da Unreal
    // apagava as estrelas. Raio mínimo de ~1.5 pixel com a mesma energia
    // (o brilho cai com a área).
    float3 starField(float3 dir, float t, float pix)
    {
        float3 p = dir * 290.0;
        float3 id = floor(p), fp = frac(p) - 0.5;
        float3 h = hash33(id);
        float d = length(fp - h * 0.36);
        float mag = hash12(id.xy * 1.7 + id.z * 31.7);
        float bright = pow(mag, 16.0);
        float r0 = 0.085;
        float r = clamp(0.75 * pix, r0, 0.45);
        float core = smoothstep(r, 0.0, d) * bright * (r0 * r0) / (r * r);
        float tw = 0.70 + 0.30 * sin(t * 2.7 + mag * 97.0) * sin(t * 1.31 + mag * 41.0);
        float ct = hash11(mag * 53.0);
        float3 tint = lerp(float3(1.00, 0.78, 0.60), float3(0.72, 0.84, 1.00), smoothstep(0.25, 0.8, ct));
        return tint * core * tw * 65.0;
    }
    // Via Láctea: banda com faixas de poeira, ~4e-4 cd/m² no trecho mais claro
    float3 milkyWay(float3 dir)
    {
        float3 gn = normalize(float3(0.43, 0.56, -0.71));
        float b = 1.0 - abs(dot(dir, gn));
        float band = smoothstep(0.68, 0.98, b);
        if (band <= 0.0) return 0.0;
        float glow = 0.45 + 0.55 * fbm(dir * 5.5, 4, 2.3, 0.55);
        float dust = 1.0 - 0.75 * smoothstep(0.35, 0.75, billow(dir * 9.0 + 3.1, 4, 2.4, 0.5));
        return float3(0.94, 0.99, 1.24) * band * glow * dust * 4.0e-4;
    }
};
FNight Night;
const float3 MOONLIGHT = float3(1.165, 0.976, 0.753);    // luar, B−V 0.92
const float BOOST = 880.0;                               // lua do render / física (celestial.js)
float3 d = normalize(float3(Dir.x, Dir.z, Dir.y));
float3 md = normalize(float3(MoonDir.x, MoonDir.z, MoonDir.y));
float pix = (length(ddx(d)) + length(ddy(d))) * 0.5 * 290.0;
float3 col = 0.0;
// fontes da noite, só quando o céu é de lua (o WebGL: wash > 0.004)
if (Gain > BOOST * 0.004 && d.y > -0.06)
{
    float h = smoothstep(-0.06, 0.08, d.y);
    col += (Night.starField(d, T, pix) + Night.milkyWay(d)) * h * Gain;
    col += MOONLIGHT * 2.0e-4 * h * Gain;                  // airglow + luz zodiacal
}
// lua: disco com escurecimento de bordo e mares, ~2500 cd/m² na lua cheia.
// Fora da captura da Sky Light (como no WebGL, a luz ambiente é só o céu): o
// halo de dezenas de milhares de cd/m² virava uma luz pontual na iluminação
// global e manchava o chão.
float inView = 1.0 - View.RenderingReflectionCaptureMask;
float dm = dot(d, md);
float disk = smoothstep(0.999955, 0.999985, dm) * inView;
if (disk > 0.0)
{
    float3 tt = normalize(cross(md, float3(0.0, 1.0, 0.0)) + 1e-4);
    float3 bb = cross(md, tt);
    float2 uv = float2(dot(d, tt), dot(d, bb)) * 160.0;
    float rr = saturate(length(uv));
    float limb = sqrt(max(1.0 - rr * rr, 0.0)) * 0.45 + 0.55;
    float maria = 0.76 + 0.24 * Night.fbm(float3(uv * 2.2, 0.0), 4, 2.3, 0.55);
    col += MOONLIGHT * 2500.0 * BOOST * disk * limb * maria;
}
// auréola estreita (< 2°); a larga já vem do espalhamento da atmosfera
col += MOONLIGHT * 75.0 * BOOST * pow(max(dm, 0.0), 420.0) * inView;
// o disco do sol também fica fora: a luz direta dele já é a luz direcional
col += SunDisk * inView;
return col;
'''


def sky_material():
    """Céu da Unreal (SkyAtmosphereViewLuminance + disco do sol) + as fontes da
    noite do WebGL. Material de céu (IsSky): sem névoa, sem perspectiva aérea,
    e a captura em tempo real da Sky Light enxerga ele."""
    m = new_material(SKY)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('is_sky', True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    view_lum = E(unreal.MaterialExpressionSkyAtmosphereViewLuminance, -700, -300)
    sun_disk = E(unreal.MaterialExpressionSkyAtmosphereLightDiskLuminance, -700, -200)
    sun_disk.set_editor_property('light_index', 0)
    cam = E(unreal.MaterialExpressionCameraVectorWS, -900, 0)
    view_dir = E(unreal.MaterialExpressionMultiply, -700, 0)          # câmera → pixel
    view_dir.set_editor_property('const_b', -1.0)
    link(cam, '', view_dir, 'A')
    moon_dir = E(unreal.MaterialExpressionSkyAtmosphereLightDirection, -700, 120)
    moon_dir.set_editor_property('light_index', 1)
    time = E(unreal.MaterialExpressionTime, -700, 220)
    gain = E(unreal.MaterialExpressionCollectionParameter, -700, 320)    # escrito pela hora do dia (C++)
    gain.set_editor_property('collection', environment_params())
    gain.set_editor_property('parameter_name', 'StarGain')
    night = E(unreal.MaterialExpressionCustom, -400, 100)
    night.set_editor_property('description', 'NightSky')
    night.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for n in ('Dir', 'MoonDir', 'T', 'Gain', 'SunDisk'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    night.set_editor_property('inputs', inputs)
    night.set_editor_property('code', SKY_HLSL)
    link(view_dir, '', night, 'Dir')
    link(moon_dir, '', night, 'MoonDir')
    link(time, '', night, 'T')
    link(gain, '', night, 'Gain')
    link(sun_disk, '', night, 'SunDisk')
    add = E(unreal.MaterialExpressionAdd, -100, 0)
    link(view_lum, '', add, 'A')
    link(night, '', add, 'B')
    ok = mel.connect_material_property(add, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if failed or not ok:
        raise RuntimeError('sky: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


EXPOSURE_MASK = '/Game/Materials/M_ExposureMask'
MASK_REACH_M = 700.0      # o chão do WebGL: 1.4 km de lado (layout.js)


def exposure_mask_material():
    """Máscara da medição da exposição: 1 onde o raio da câmera bate no chão a
    menos de MASK_REACH_M, 0 no resto. O WebGL mede só superfícies e explosões
    (js/exposure.js: "nunca o céu, o sol ou a lua"), e o chão dele acaba em
    700 m: depois disso é céu. A Unreal mede a tela inteira, e um céu de poente,
    o halo da lua ou a névoa clara do horizonte (o chão daqui vai a 10 km)
    fechavam a câmera sem explosão nenhuma. O C++
    (BlastLabEnvironment) desenha isto numa textura a cada quadro com a câmera
    do jogador e entrega como AutoExposureMeterMask."""
    m = new_material(EXPOSURE_MASK)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    uv = E(unreal.MaterialExpressionTextureCoordinate, -700, -200)
    params = {}
    for i, n in enumerate(('Forward', 'Right', 'Up')):
        p = E(unreal.MaterialExpressionVectorParameter, -700, -100 + 100 * i)
        p.set_editor_property('parameter_name', n)
        params[n] = p
    for i, (n, v) in enumerate((('TanX', 0.7), ('TanY', 0.4), ('Height', 50.0), ('Reach', MASK_REACH_M))):
        p = E(unreal.MaterialExpressionScalarParameter, -700, 200 + 80 * i)
        p.set_editor_property('parameter_name', n)
        p.set_editor_property('default_value', v)
        params[n] = p
    c = E(unreal.MaterialExpressionCustom, -300, 0)
    c.set_editor_property('description', 'ExposureMask')
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = []
    for n in ('UV', 'Forward', 'Right', 'Up', 'TanX', 'TanY', 'Height', 'Reach'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    c.set_editor_property('inputs', inputs)
    c.set_editor_property('code', '\n'.join([
        'float3 d = normalize(Forward + (UV.x * 2.0 - 1.0) * TanX * Right - (UV.y * 2.0 - 1.0) * TanY * Up);',
        '// chão plano em z = 0 até Reach metros; acima disso é céu',
        'float t = d.z < -1e-5 ? Height / -d.z : 1e30;',
        'return t < Reach ? 1.0 : 0.0;',
    ]))
    link(uv, '', c, 'UV')
    for n in ('Forward', 'Right', 'Up', 'TanX', 'TanY', 'Height', 'Reach'):
        link(params[n], '', c, n)
    ok = mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if failed or not ok:
        raise RuntimeError('exposure mask: failed links %s %s' % (failed, ok))
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def sky_dome():
    """Esfera de 50 km em volta do campo (o chão vai a 10 km), vista por
    dentro. Não projeta sombra nem entra na iluminação global."""
    mesh = unreal.load_asset('/Engine/BasicShapes/Sphere')               # raio 50 cm
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    a.set_actor_label('SkyDome')
    a.set_actor_scale3d(unreal.Vector(1.0e5, 1.0e5, 1.0e5))
    c = a.static_mesh_component
    c.set_static_mesh(mesh)
    c.set_material(0, sky_material())
    for prop, value in (('cast_shadow', False), ('affect_distance_field_lighting', False),
                        ('affect_dynamic_indirect_lighting', False), ('visible_in_ray_tracing', False),
                        ('receives_decals', False), ('mobility', unreal.ComponentMobility.STATIC)):
        c.set_editor_property(prop, value)
    return a


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
