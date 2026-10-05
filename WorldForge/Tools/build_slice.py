"""Monta a fatia de qualidade (deserto, ~250 m): /Game/Maps/Slice_Desert.

Rodar sem abrir o editor: powershell -File WorldForge/Tools/forge.ps1 slice
(antes: python WorldForge/Tools/fetch_assets.py, que baixa os assets CC0).

O terreno sai do gerador do WorldForge (o mesmo núcleo do app): um recorte do
mapa vira Landscape da Unreal com 5 camadas pintadas pelas máscaras do
gerador (areia, cascalho, terra, rocha, terra rachada). Rochas, pedregulhos,
capim e arbustos são espalhados pelas mesmas máscaras e posições; marcas de
pneu vão ao longo das trilhas que o gerador abriu.

Pesquisa e escolhas (docs/WORLDFORGE.md, 05/10/2026): mistura por altura
(Mishkinis, "Advanced Terrain Texture Splatting"), duas escalas + variação
larga contra repetição, areia de longe e de perto, splats/decalques como no
Age of Empires IV e Company of Heroes, espalhamento por mapas de densidade como
no Horizon Zero Dawn, sol de fim de tarde a ~20° (sombra legível pro RTS).
"""
import math
import os
import sys

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
ASSETS = os.path.join(PROJECT, 'Intermediate', 'Assets')
DEST = '/Game/Slice'
MAP = '/Game/Maps/Slice_Desert'

# o mapa gerado de onde sai o recorte (deserto, 2 jogadores, 1 km)
SEED, PLAYERS, BIOME, MAP_SIZE = 3, 2, 1, 1024.0
COMPONENTS, QUADS, QUAD_M = 8, 63, 0.5          # 505×505 vértices, 252 m

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
lib = unreal.WorldForgeEditorLib


def log(*a):
    unreal.log('FORGE ' + ' '.join(str(x) for x in a))


# ---- importação ----------------------------------------------------------------

def import_file(path, folder, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', path)
    t.set_editor_property('destination_path', folder)
    if name:
        t.set_editor_property('destination_name', name)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    return [unreal.load_asset(p) for p in t.get_editor_property('imported_object_paths')]


def texture(path, folder, kind):
    """kind: color (sRGB), normal, masks (linear: ARM, altura, alfa)."""
    name = 'T_' + os.path.splitext(os.path.basename(path))[0]
    objs = import_file(path, folder, name)
    tex = unreal.load_asset(folder + '/' + name) if not objs else objs[0]
    if kind == 'normal':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif kind == 'masks':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property('srgb', False)
    eal.save_loaded_asset(tex)
    return tex


def find(folder_files, key):
    for f in folder_files:
        if key in os.path.basename(f):
            return f
    return None


def surface(aid):
    src = os.path.join(ASSETS, aid)
    files = [os.path.join(src, f) for f in os.listdir(src)]
    folder = DEST + '/Surfaces/' + aid
    return {
        'diff': texture(find(files, '_diff_'), folder, 'color'),
        'nor': texture(find(files, '_nor_dx_'), folder, 'normal'),
        'arm': texture(find(files, '_arm_'), folder, 'masks'),
        'disp': texture(find(files, '_disp_'), folder, 'masks'),
    }


# ---- materiais ----------------------------------------------------------------

class Graph:
    def __init__(self, name, folder=DEST + '/Materials'):
        path = folder + '/' + name
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        self.m = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        self.failed = []
        self.y = 0

    def node(self, cls, x=0, y=None, **props):
        if y is None:
            self.y += 120
            y = self.y
        e = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, out, b, inp):
        if not mel.connect_material_expressions(a, out, b, inp):
            self.failed.append('%s.%s -> %s.%s' % (a.get_name(), out, b.get_name(), inp))

    def out(self, a, pin, prop):
        if not mel.connect_material_property(a, pin, prop):
            self.failed.append('%s.%s -> %s' % (a.get_name(), pin, prop))

    def mul(self, a, a_pin, b, b_pin='', x=0):
        n = self.node(unreal.MaterialExpressionMultiply, x)
        self.link(a, a_pin, n, 'A')
        self.link(b, b_pin, n, 'B')
        return n

    def const3(self, rgb, x=0):
        return self.node(unreal.MaterialExpressionConstant3Vector, x, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))

    def sample(self, tex, uv, kind, x=-600):
        st = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
              'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
              'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        s = self.node(unreal.MaterialExpressionTextureSample, x, texture=tex, sampler_type=st,
                      # amostrador compartilhado: o terreno usa dezenas de texturas
                      sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        if uv is not None:
            self.link(uv, '', s, 'UVs')
        return s

    def finish(self, label):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        log(label, 'failed links:', self.failed or 'none')
        return self.m


def ground_material(S):
    """Landscape: 5 camadas misturadas pela altura da textura (LB_HeightBlend)."""
    g = Graph('M_SliceGround')
    # Uma mistura por canal (e não "atributos de material"): na 5.8 o
    # LandscapeLayerBlend em atributos falhava em parte das combinações de
    # camadas ("Arithmetic between types float3 and float2") e o componente
    # caía no material padrão quadriculado.
    wp = g.node(unreal.MaterialExpressionWorldPosition, -1800)
    xy = g.node(unreal.MaterialExpressionComponentMask, -1650, r=True, g=True, b=False, a=False)
    g.link(wp, '', xy, '')
    depth = g.node(unreal.MaterialExpressionPixelDepth, -1800)

    def uv(tile_m, offset=0.0, rot=False):
        d = g.node(unreal.MaterialExpressionDivide, -1450, const_b=tile_m * 100.0)
        g.link(xy, '', d, 'A')
        if offset:
            a = g.node(unreal.MaterialExpressionAdd, -1300, const_b=offset)
            g.link(d, '', a, 'A')
            return a
        return d

    # variação larga de tom (manchas de ~150 m) com a foto aérea: quebra a
    # repetição de todas as camadas vista de cima
    macro_s = g.sample(S['aerial_beach_01']['diff'], uv(150.0, 0.13), 'color', -1200)
    macro_r = g.node(unreal.MaterialExpressionComponentMask, -1050, r=True, g=False, b=False, a=False)
    g.link(macro_s, 'RGB', macro_r, '')
    macro = g.node(unreal.MaterialExpressionCustom, -900)
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', 'R')
    macro.set_editor_property('inputs', [ci])
    macro.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    macro.set_editor_property('code', 'return lerp(0.82, 1.18, saturate((R - 0.45) / 0.25));')
    g.link(macro_r, '', macro, 'R')

    WAT = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture')
    WAN = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal')

    # (ComponentMask nasce com R e G ligados: sempre dizer os quatro canais)
    def triplanar(tex, tile_m, normal=False):
        obj = g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                     (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
        f = g.node(unreal.MaterialExpressionMaterialFunctionCall, -500)
        f.set_material_function(WAN if normal else WAT)
        size = g.const3((tile_m * 100.0, tile_m * 100.0, tile_m * 100.0), -700)
        g.link(obj, '', f, 'TextureObject')
        g.link(size, '', f, 'TextureSize')
        return f

    def rock_layer(name, s, tile_m, tint):
        diff = triplanar(s['diff'], tile_m)
        nor = triplanar(s['nor'], tile_m, normal=True)
        arm = triplanar(s['arm'], tile_m)
        disp = triplanar(s['disp'], tile_m)
        base = g.mul(diff, 'XYZ Texture', g.const3(tint, -300), '', -200)
        base = g.mul(base, '', macro, '', 0)
        armr = g.node(unreal.MaterialExpressionComponentMask, -100, r=True, g=False, b=False, a=False)
        armg = g.node(unreal.MaterialExpressionComponentMask, -100, r=False, g=True, b=False, a=False)
        g.link(arm, 'XYZ Texture', armr, '')
        g.link(arm, 'XYZ Texture', armg, '')
        dispr = g.node(unreal.MaterialExpressionComponentMask, -100, r=True, g=False, b=False, a=False)
        g.link(disp, 'XYZ Texture', dispr, '')
        return {'name': name, 'base': (base, ''), 'nor': (nor, 'XYZ Texture'), 'rough': (armg, ''), 'ao': (armr, ''),
                'disp': (dispr, '')}

    def layer(name, s, tile_m, tint, far=None, far_tile=30.0, far_tint=(1, 1, 1)):
        a = g.sample(s['diff'], uv(tile_m), 'color')
        b = g.sample(s['diff'], uv(tile_m * 3.7, 0.37), 'color')
        diff = g.node(unreal.MaterialExpressionLinearInterpolate, -400, const_alpha=0.35)
        g.link(a, 'RGB', diff, 'A')
        g.link(b, 'RGB', diff, 'B')
        nor = g.sample(s['nor'], uv(tile_m), 'normal')
        arm = g.sample(s['arm'], uv(tile_m), 'masks')
        disp = g.sample(s['disp'], uv(tile_m), 'masks')
        base = g.mul(diff, '', g.const3(tint, -300), '', -200)
        if far is not None:
            # longe da câmera: a foto aérea de 30 m (o grão de perto vira ruído)
            fa = g.sample(far['diff'], uv(far_tile, 0.21), 'color')
            fb = g.mul(fa, 'RGB', g.const3(far_tint, -300), '', -200)
            t = g.node(unreal.MaterialExpressionCustom, -300)
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', 'D')
            t.set_editor_property('inputs', [ci])
            t.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
            t.set_editor_property('code', 'return saturate((D - 2500.0) / 6000.0);')
            g.link(depth, '', t, 'D')
            mix = g.node(unreal.MaterialExpressionLinearInterpolate, -100)
            g.link(base, '', mix, 'A')
            g.link(fb, '', mix, 'B')
            g.link(t, '', mix, 'Alpha')
            base = mix
        base = g.mul(base, '', macro, '', 0)
        return {'name': name, 'base': (base, ''), 'nor': (nor, 'RGB'), 'rough': (arm, 'G'), 'ao': (arm, 'R'), 'disp': (disp, 'R')}

    layers = [
        layer('Sand', S['sandy_gravel_02'], 2.5, (1.0, 1.0, 1.0), far=S['aerial_beach_01'], far_tile=45.0, far_tint=(1.32, 1.02, 0.72)),
        layer('Gravel', S['gravelly_sand'], 2.5, (0.95, 0.95, 0.95)),
        layer('Dirt', S['dry_ground_01'], 4.0, (1.18, 0.98, 0.78)),
        rock_layer('Rock', S['cliff_side'], 3.6, (0.95, 0.95, 0.95)),
        layer('Cracked', S['mud_cracked_dry_03'], 3.0, (0.72, 0.68, 0.64)),
    ]
    for channel, prop, y in (('base', unreal.MaterialProperty.MP_BASE_COLOR, 0),
                             ('nor', unreal.MaterialProperty.MP_NORMAL, 600),
                             ('rough', unreal.MaterialProperty.MP_ROUGHNESS, 1200),
                             ('ao', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 1800)):
        blend = g.node(unreal.MaterialExpressionLandscapeLayerBlend, 600, y)
        entries = []
        for L in layers:
            e = unreal.LayerBlendInput()
            e.set_editor_property('layer_name', L['name'])
            e.set_editor_property('blend_type', unreal.LandscapeLayerBlendType.LB_HEIGHT_BLEND)
            e.set_editor_property('preview_weight', 1.0 if L['name'] == 'Sand' else 0.0)
            entries.append(e)
        blend.set_editor_property('layers', entries)
        for L in layers:
            g.link(L[channel][0], L[channel][1], blend, 'Layer ' + L['name'])
            g.link(L['disp'][0], L['disp'][1], blend, 'Height ' + L['name'])
        g.out(blend, '', prop)
    return g.finish('ground material')


def prop_material(name, plant):
    """Mestre dos modelos escaneados: cor, normal, ARM (e alfa nas plantas)."""
    g = Graph(name)
    if plant:
        g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        g.m.set_editor_property('two_sided', True)
        # luz atravessando a folha no contraluz do fim de tarde
        g.m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    def param(cls, pname, default, kind):
        p = g.node(cls, -600, parameter_name=pname, texture=default,
                   sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                                 'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                                 'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind])
        return p
    defaults = DEFAULT_TEX
    col = param(unreal.MaterialExpressionTextureSampleParameter2D, 'BaseColorTex', defaults['color'], 'color')
    nor = param(unreal.MaterialExpressionTextureSampleParameter2D, 'NormalTex', defaults['normal'], 'normal')
    arm = param(unreal.MaterialExpressionTextureSampleParameter2D, 'ARMTex', defaults['masks'], 'masks')
    tint = g.node(unreal.MaterialExpressionVectorParameter, -600, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    base = g.mul(col, 'RGB', tint, '', -300)
    g.out(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(nor, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    g.out(arm, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(arm, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if plant:
        alpha = param(unreal.MaterialExpressionTextureSampleParameter2D, 'AlphaTex', defaults['masks'], 'masks')
        g.out(alpha, 'R', unreal.MaterialProperty.MP_OPACITY_MASK)
        sss = g.mul(base, '', g.const3((0.6, 0.55, 0.3), -300), '', -150)
        g.out(sss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    return g.finish(name)


def decal_material(tex):
    """Marca de pneu: o sulco escurece e ganha relevo; o resto do retângulo some."""
    g = Graph('M_TireTrack')
    g.m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc0 = g.node(unreal.MaterialExpressionTextureCoordinate, -1000)
    # a caixa do decalque fica ao longo da trilha, mas a foto entra girada 90°
    # nela (o comprimento da marca virava a largura): troca u↔v
    tc = g.node(unreal.MaterialExpressionCustom, -900)
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', 'UV')
    tc.set_editor_property('inputs', [ci])
    tc.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    tc.set_editor_property('code', 'return UV.yx;')
    g.link(tc0, '', tc, 'UV')
    col = g.sample(tex['color'], tc, 'color')
    nor = g.sample(tex['normal'], tc, 'normal')
    rough = g.sample(tex['rough'], tc, 'masks')
    disp = g.sample(tex['disp'], tc, 'masks')
    c = g.node(unreal.MaterialExpressionCustom, -300)
    ins = []
    for n in ('D', 'UV', 'C'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    # opacidade: a faixa inteira do pneu afunda e escurece a areia; a banda de
    # rodagem (altura da foto) varia por dentro. Só pegar o fundo dos sulcos
    # deixava a marca tracejada (A/B de 05/10).
    c.set_editor_property('code', '\n'.join([
        'float band = smoothstep(0.08, 0.28, UV.y) * smoothstep(0.92, 0.72, UV.y);',
        'float ends = smoothstep(0.0, 0.02, UV.x) * smoothstep(1.0, 0.98, UV.x);',
        'float tread = saturate((0.75 - D) * 1.5);',
        'return band * ends * (0.62 + 0.3 * tread);']))
    g.link(disp, 'R', c, 'D')
    g.link(tc, '', c, 'UV')
    g.link(col, 'RGB', c, 'C')
    # Cor: areia compactada um pouco mais escura que a do chão (a foto da
    # marca é de areia cinza-clara e aparecia como retângulo claro); o
    # desenho do sulco vem da altura da foto. A normal da foto deixava a marca
    # preta (A/B de 05/10: sem ela a marca aparece) — fica de fora por ora.
    lum = g.node(unreal.MaterialExpressionComponentMask, -400, r=False, g=True, b=False, a=False)
    g.link(col, 'RGB', lum, '')
    sand = g.mul(lum, '', g.const3((0.22, 0.135, 0.075), -400), '', -150)
    g.out(sand, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(rough, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(c, '', unreal.MaterialProperty.MP_OPACITY)
    return g.finish('tire track decal')


# ---- modelos ------------------------------------------------------------------

def model(aid, plant=False, nanite=True, tint=(1.0, 1.0, 1.0)):
    src = os.path.join(ASSETS, aid)
    files = [os.path.join(src, f) for f in os.listdir(src)]
    folder = DEST + '/Models/' + aid
    fbx = find(files, '.fbx')
    objs = import_file(fbx, folder)
    meshes = [o for o in objs if isinstance(o, unreal.StaticMesh)]
    if not meshes:
        meshes = [unreal.load_asset(a) for a in eal.list_assets(folder, recursive=False)
                  if isinstance(unreal.load_asset(a), unreal.StaticMesh)]
    mesh = meshes[0]
    tex = {'color': texture(find(files, '_diff_') if not find(files, '_dry_diff_') else find(files, '_dry_diff_'), folder, 'color'),
           'normal': texture(find(files, '_nor_dx_'), folder, 'normal'),
           'masks': texture(find(files, '_arm_'), folder, 'masks')}
    mi_path = folder + '/MI_' + aid
    if eal.does_asset_exist(mi_path):
        eal.delete_asset(mi_path)
    mi = asset_tools.create_asset('MI_' + aid, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    # galho seco sem mapa de alfa é geometria cheia: material de rocha (opaco)
    plant = plant and find(files, '_alpha_') is not None
    mi.set_editor_property('parent', PLANT_MASTER if plant else ROCK_MASTER)
    mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', tex['color'])
    mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', tex['normal'])
    mel.set_material_instance_texture_parameter_value(mi, 'ARMTex', tex['masks'])
    # tom: as rochas da Namaqualand são cinza-acastanhadas; o paredão do
    # terreno (cliff_side) e a areia daqui são alaranjados — casar os dois
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    if plant and find(files, '_alpha_'):
        mel.set_material_instance_texture_parameter_value(mi, 'AlphaTex', texture(find(files, '_alpha_'), folder, 'masks'))
    eal.save_loaded_asset(mi)
    for i in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(i, mi)
    if nanite:
        nanite = lib.enable_nanite(mesh)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    log('model', aid, 'size cm', round(b.max.x - b.min.x), round(b.max.y - b.min.y), round(b.max.z - b.min.z), 'nanite', nanite)
    return mesh


def layer_info(name):
    path = DEST + '/Layers/LI_' + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    li = asset_tools.create_asset('LI_' + name, DEST + '/Layers', unreal.LandscapeLayerInfoObject, None)
    lib.set_layer_info_name(li, name)
    eal.save_loaded_asset(li)
    return li


# ---- luz de fim de tarde ---------------------------------------------------------

def physical_ev100(sun_elevation_deg, sun_lux, sky_lux, transmittance):
    e = sun_lux * math.sin(math.radians(sun_elevation_deg)) * transmittance + sky_lux
    return math.log2(e * 100.0 / 250.0)


def environment():
    # Sol a 20°: sombra de ~2,7× a altura (um capim de 40 cm faz ~1,1 m), longa
    # o bastante pra desenhar o relevo e curta pra não cobrir unidades.
    elev, az = 20.0, math.radians(-60.0)
    to_sun = unreal.Vector(math.cos(az) * math.cos(math.radians(elev)), math.sin(az) * math.cos(math.radians(elev)),
                           math.sin(math.radians(elev)))
    rot = unreal.MathLibrary.make_rot_from_x(unreal.Vector(-to_sun.x, -to_sun.y, -to_sun.z))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), rot)
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property('intensity', 120000.0)
    lc.set_editor_property('atmosphere_sun_light', True)
    lc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property('light_source_angle', 0.53)          # diâmetro do sol: sombra dura no contato
    lc.set_editor_property('contact_shadow_length', 0.03)       # capim e pedrinhas
    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    # poeira de deserto: mais Mie (halo quente em volta do sol, névoa no horizonte)
    sc.set_editor_property('mie_scattering_scale', 0.012)
    sc.set_editor_property('mie_absorption_scale', 0.0015)
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property('intensity', 1.3)   # céu do deserto claro: sombra azulada mais aberta
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -500))
    fog.set_actor_label('HeightFog')
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.008)
    fc.set_editor_property('fog_height_falloff', 0.35)
    # exposição física travada pro sol a 20° (a atmosfera tira ~45% da luz)
    ev = physical_ev100(elev, 120000.0, 9000.0, 0.55)
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ('auto_exposure_min_brightness', ev),
                        ('auto_exposure_max_brightness', ev),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        ('auto_exposure_bias', 0.3),
                        ('bloom_intensity', 0.25),
                        ('local_exposure_shadow_contrast_scale', 0.6),
                        ('local_exposure_highlight_contrast_scale', 0.9)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    pp.set_editor_property('settings', s)
    for a in (sun, sky, sl, fog, pp):
        a.set_folder_path('Environment')
    log('environment EV100 %.2f, sun %.0f°' % (ev, elev))


# ---- montagem -------------------------------------------------------------------

def debug_decal():
    """Teste de orientação (DECAL_DEBUG=1): um decalque gigante no centro,
    com yaw 0 (trilha apontando pra +X), pintado com as coordenadas da
    textura: vermelho = U, verde = V (as mesmas que a marca usa)."""
    g = Graph('M_DecalDebug')
    g.m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc0 = g.node(unreal.MaterialExpressionTextureCoordinate, -1000)
    c = g.node(unreal.MaterialExpressionCustom, -500)
    ci = unreal.CustomInput()
    ci.set_editor_property('input_name', 'UV')
    c.set_editor_property('inputs', [ci])
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('code', 'float2 t = UV.yx; return float3(t.x, t.y, 0.0) * 0.6;')
    g.link(tc0, '', c, 'UV')
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    one = g.node(unreal.MaterialExpressionConstant, -300, r=0.9)
    g.out(one, '', unreal.MaterialProperty.MP_OPACITY)
    m = g.finish('decal debug')
    z = lib.slice_height_at(unreal.Vector2D(0, 0))
    da = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(0, 0, z), unreal.Rotator(-90.0, 0.0, 0.0))
    dc = da.get_component_by_class(unreal.DecalComponent)
    dc.set_editor_property('decal_material', m)
    dc.set_editor_property('decal_size', unreal.Vector(500.0, 720.0, 192.0))
    log('debug decal at z', z)


def tire_tracks(decal_mat):
    """Duas marcas paralelas (bitola ~1,8 m) ao longo das trilhas do gerador."""
    pts = lib.slice_tracks(1.6)
    n = 0
    line = []
    for p in list(pts) + [unreal.Vector(0, 0, -1e9)]:
        if p.z < -1e8:
            for i in range(1, len(line) - 1):
                a, b = line[i - 1], line[i + 1]
                d = unreal.Vector(b.x - a.x, b.y - a.y, 0)
                ln = math.hypot(d.x, d.y) or 1.0
                d = unreal.Vector(d.x / ln, d.y / ln, 0)
                side = unreal.Vector(-d.y, d.x, 0)
                yaw = math.degrees(math.atan2(d.y, d.x))
                for off in (-90.0, 90.0):
                    loc = unreal.Vector(line[i].x + side.x * off, line[i].y + side.y * off, line[i].z)
                    da = actors.spawn_actor_from_class(unreal.DecalActor, loc, unreal.Rotator(-90.0, yaw, 0.0))
                    dc = da.get_component_by_class(unreal.DecalComponent)
                    dc.set_editor_property('decal_material', decal_mat)
                    # caixa (meias medidas): 0,6 m de fundo, 0,48 de largura, 1,8 de comprimento (a foto)
                    dc.set_editor_property('decal_size', unreal.Vector(60.0, 90.0, 24.0))   # meias medidas: X fundo; o Y local fica ao longo da trilha (medido nas fotos)
                    da.set_folder_path('Decals')
                    n += 1
            line = []
        else:
            line.append(p)
    log('tire track decals', n)


def scatter(meshes):
    group = lib.spawn_group_actor('Scatter')
    plan = [
        # (tipo, espaçamento m, escala, malhas, sombra, corte m)
        ('RockLarge', 1.0, (0.6, 1.4), meshes['boulders_big'], True, 0),
        ('RockSmall', 1.3, (0.5, 1.5), meshes['boulders_small'], True, 260),
        ('Pebble', 1.4, (1.0, 2.2), meshes['pebbles'], True, 120),
        ('Tuft', 0.55, (2.5, 5.0), meshes['tufts'], True, 180),
        ('Branch', 4.0, (0.8, 1.4), meshes['branches'], True, 120),
        ('Shrub', 2.2, (1.0, 2.2), meshes['shrubs'], True, 300),
    ]
    for kind, spacing, (smin, smax), variants, shadow, cull in plan:
        xs = lib.slice_scatter(kind, spacing, smin, smax, 0.0, 0.0).transforms
        buckets = [[] for _ in variants]
        for i, x in enumerate(xs):
            buckets[(i * 2654435761) % len(variants)].append(x)
        for mesh, b in zip(variants, buckets):
            if b:
                lib.add_instances(group, mesh, b, shadow, cull)
        log('scatter', kind, len(xs))
    # paredões escaneados cobrindo os cortes do terreno (cada malha tem a sua frente)
    for mesh, yaw_off, sink in ((meshes['cliffs'][0], 0.0, 1.5), (meshes['cliffs'][1], 0.0, 2.0)):
        xs = lib.slice_scatter('Cliff', 9.0 if mesh == meshes['cliffs'][0] else 13.0, 0.9, 1.25, sink, yaw_off).transforms
        lib.add_instances(group, mesh, xs, True, 0)
        log('scatter Cliff', mesh.get_name(), len(xs))


def build():
    global DEFAULT_TEX, ROCK_MASTER, PLANT_MASTER
    S = {aid: surface(aid) for aid in ('aerial_beach_01', 'sandy_gravel_02', 'dry_ground_01', 'gravelly_sand',
                                       'mud_cracked_dry_03', 'asphalt_04', 'cliff_side')}
    DEFAULT_TEX = {'color': S['sandy_gravel_02']['diff'], 'normal': S['sandy_gravel_02']['nor'], 'masks': S['sandy_gravel_02']['arm']}
    ROCK_MASTER = prop_material('M_ScanRock', plant=False)
    PLANT_MASTER = prop_material('M_ScanPlant', plant=True)
    ground = ground_material(S)
    tt = os.path.join(ASSETS, 'TireTracks001')
    tfiles = [os.path.join(tt, f) for f in os.listdir(tt)]
    decal = decal_material({'color': texture(find(tfiles, '_Color'), DEST + '/Decals', 'color'),
                            'normal': texture(find(tfiles, '_NormalDX'), DEST + '/Decals', 'normal'),
                            'rough': texture(find(tfiles, '_Roughness'), DEST + '/Decals', 'masks'),
                            'disp': texture(find(tfiles, '_Displacement'), DEST + '/Decals', 'masks')})
    meshes = {
        'boulders_big': [model('namaqualand_boulder_02', tint=(1.18, 0.97, 0.80)), model('namaqualand_boulder_03', tint=(1.18, 0.97, 0.80)), model('namaqualand_boulder_04', tint=(1.18, 0.97, 0.80))],
        'boulders_small': [model('namaqualand_boulder_05', tint=(1.18, 0.97, 0.80)), model('namaqualand_boulder_06', tint=(1.18, 0.97, 0.80)), model('namaqualand_boulders_01', tint=(1.18, 0.97, 0.80)),
                           model('namaqualand_rocks_01', tint=(1.18, 0.97, 0.80))],
        'pebbles': [model('namaqualand_stones_01', tint=(1.18, 0.97, 0.80))],
        'tufts': [model('grass_medium_01', plant=True, nanite=False), model('grass_medium_02', plant=True, nanite=False)],
        'branches': [model('dry_branches_medium_01', plant=True, nanite=False)],
        'cliffs': [model('namaqualand_cliff_01', tint=(1.40, 1.00, 0.68)), model('namaqualand_cliff_02', tint=(1.40, 1.00, 0.68))],
        'shrubs': [model('wild_rooibos_bush', plant=True, nanite=False), model('searsia_lucida', plant=True, nanite=False)],
    }
    layers = [layer_info(n) for n in ('Sand', 'Gravel', 'Dirt', 'Rock', 'Cracked')]

    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    levels.new_level(MAP)
    environment()
    landscape, err = lib.build_slice_landscape(SEED, PLAYERS, BIOME, MAP_SIZE, unreal.Vector2D(1e9, 1e9),
                                               COMPONENTS, QUADS, QUAD_M, ground, layers)
    if err:
        log('landscape error:', err)
    scatter(meshes)
    tire_tracks(decal)
    if os.environ.get('DECAL_DEBUG') == '1':
        debug_decal()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.ForgeSliceGameMode)
    levels.save_current_level()
    eal.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('slice saved')


build()
