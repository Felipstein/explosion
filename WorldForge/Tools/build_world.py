"""Monta (ou remonta) os assets do WorldForge: texturas, materiais e o mapa.

Rodar sem abrir o editor: powershell -File WorldForge/Tools/forge.ps1 build
(roda make_textures.py antes). Idempotente: apaga e recria.

O mapa /Game/Maps/Forge só tem o ambiente (sol, céu, névoa, exposição); o
terreno, a água e os objetos são criados pelo C++ (AWorldForgeGameMode) a
cada mapa gerado. Os materiais recebem do C++ as texturas geradas em tempo de
execução (DataMap, OverlayMap) e as cores do bioma (FWorldPalette).
"""
import math
import os

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
TEXTURES = os.path.join(PROJECT, 'Intermediate', 'Textures')
MAP = '/Game/Maps/Forge'

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(*a):
    unreal.log('FORGE ' + ' '.join(str(x) for x in a))


def import_texture(file, name, normal=False):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(TEXTURES, file))
    t.set_editor_property('destination_path', '/Game/Textures')
    t.set_editor_property('destination_name', name)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    tex = unreal.load_asset('/Game/Textures/' + name)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings',
                            unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_BC7)
    eal.save_loaded_asset(tex)
    return tex


class Graph:
    """Atalhos pra montar um material por código."""

    def __init__(self, name, folder='/Game/Materials'):
        path = folder + '/' + name
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        self.m = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        self.failed = []

    def node(self, cls, x, y, **props):
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

    def custom(self, x, y, desc, code, inputs, out_type, extra_outputs=()):
        c = self.node(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property('description', desc)
        c.set_editor_property('code', code)
        c.set_editor_property('output_type', out_type)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', n)
            ins.append(ci)
        c.set_editor_property('inputs', ins)
        outs = []
        for n, t in extra_outputs:
            co = unreal.CustomOutput()
            co.set_editor_property('output_name', n)
            co.set_editor_property('output_type', t)
            outs.append(co)
        c.set_editor_property('additional_outputs', outs)
        return c

    def finish(self, label):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        log(label, 'failed links:', self.failed or 'none')
        return self.m


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

# ---- terreno -----------------------------------------------------------------
# Tudo num nó de código: camadas planas (grama, terra, areia) projetadas no XY
# do mundo em duas escalas (quebra a repetição), rocha triplanar (o penhasco
# não estica), misturas pela "altura" da textura (transição recortada, como o
# height blend dos materiais de terreno de jogo), variação larga de tom.
# A posição vem do nó Local Position (float comum: o terreno está na origem),
# não da posição de mundo de precisão dupla — num nó de código ela chegava
# errada no BlastLab (build_scene.py).
TERRAIN_HLSL = r'''
float3 p = LP * 0.01;
float2 muv = p.xy / MapSize + 0.5;
float4 d = DataMap.SampleLevel(DataMapSampler, muv, 0);
float3 n = normalize(VN);

float4 G = lerp(GrassT.Sample(GrassTSampler, p.xy * 0.25), GrassT.Sample(GrassTSampler, p.xy * 0.103 + 0.37), 0.45);
float4 D = lerp(DirtT.Sample(DirtTSampler, p.xy * 0.1667), DirtT.Sample(DirtTSampler, p.xy * 0.07 + 0.21), 0.4);
float4 S = lerp(SandT.Sample(SandTSampler, p.xy * 0.125), SandT.Sample(SandTSampler, p.xy * 0.052 + 0.53), 0.4);
float3 bw = pow(abs(n), 4.0);
bw /= (bw.x + bw.y + bw.z);
float4 Rx = RockT.Sample(RockTSampler, p.yz * 0.1);
float4 Ry = RockT.Sample(RockTSampler, p.xz * 0.1 + 0.5);
float4 Rz = RockT.Sample(RockTSampler, p.xy * 0.1 + 0.25);
float4 R = Rx * bw.x + Ry * bw.y + Rz * bw.z;
float3 rn = float3(0.0, Rx.g - 0.5, Rx.b - 0.5) * bw.x + float3(Ry.g - 0.5, 0.0, Ry.b - 0.5) * bw.y + float3(Rz.g - 0.5, Rz.b - 0.5, 0.0) * bw.z;

float macro = GrassT.Sample(GrassTSampler, p.xy * 0.024).r * 0.6 + GrassT.Sample(GrassTSampler, p.xy * 0.0065 + 0.1).r * 0.4;
macro = 0.75 + 0.5 * macro;

float aDirt = saturate((d.r - 0.5 + (D.r - 0.5) * 0.6) * 3.0 + 0.5);
float aSand = saturate((d.b - 0.5 + (S.r - 0.5) * 0.6) * 3.0 + 0.5);
float aRock = saturate((d.a - 0.5 + (R.r - 0.5) * 0.8) * 3.5 + 0.5);

float3 c = lerp(Dry, Lush, d.g) * (2.0 * G.r);
c = lerp(c, DirtC * (2.0 * D.r), aDirt);
c = lerp(c, SandC * (2.0 * S.r), aSand);
c = lerp(c, RockC * (2.0 * R.r), aRock);
c *= macro;

float rough = lerp(lerp(lerp(G.a, D.a, aDirt), S.a, aSand), R.a, aRock);
float3 flat = float3(G.g - 0.5, G.b - 0.5, 0.0);
flat = lerp(flat, float3(D.g - 0.5, D.b - 0.5, 0.0), aDirt);
flat = lerp(flat, float3(S.g - 0.5, S.b - 0.5, 0.0), aSand);
float3 N = normalize(n + flat * (1.0 - aRock) * 2.0 + rn * aRock * 2.5);

float4 ov = OverlayMap.SampleLevel(OverlayMapSampler, muv, 0);
c = lerp(c, ov.rgb * 0.6, ov.a * OverlayOpacity);

Rough = rough;
WNormal = N;
return c;
'''


def terrain_material(tex):
    g = Graph('M_Terrain')
    g.m.set_editor_property('tangent_space_normal', False)
    lp = g.node(unreal.MaterialExpressionLocalPosition, -900, -300)
    vn = g.node(unreal.MaterialExpressionVertexNormalWS, -900, -200)

    def texobj(name, default, y):
        e = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('texture', default)
        e.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        return e

    def color(name, y, v=(0.1, 0.1, 0.1)):
        e = g.node(unreal.MaterialExpressionVectorParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', unreal.LinearColor(v[0], v[1], v[2], 1.0))
        return e

    def scalar(name, y, v):
        e = g.node(unreal.MaterialExpressionScalarParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', v)
        return e

    ins = {
        'LP': lp, 'VN': vn,
        'DataMap': texobj('DataMap', tex['dirt'], -100),
        'OverlayMap': texobj('OverlayMap', tex['dirt'], 0),
        'GrassT': texobj('GrassT', tex['grass'], 100),
        'DirtT': texobj('DirtT', tex['dirt'], 200),
        'SandT': texobj('SandT', tex['sand'], 300),
        'RockT': texobj('RockT', tex['rock'], 400),
        'Lush': color('Lush', 500, (0.045, 0.085, 0.02)),
        'Dry': color('Dry', 600, (0.12, 0.11, 0.05)),
        'DirtC': color('Dirt', 700, (0.13, 0.095, 0.065)),
        'SandC': color('Sand', 800, (0.32, 0.27, 0.18)),
        'RockC': color('Rock', 900, (0.17, 0.16, 0.15)),
        'MapSize': scalar('MapSize', 1000, 1026.0),
        'OverlayOpacity': scalar('OverlayOpacity', 1100, 0.0),
    }
    c = g.custom(-400, 0, 'TerrainLayers', TERRAIN_HLSL, list(ins.keys()), F3, [('Rough', F1), ('WNormal', F3)])
    for k, e in ins.items():
        g.link(e, '', c, k)
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(c, 'WNormal', unreal.MaterialProperty.MP_NORMAL)
    return g.finish('terrain material')


# ---- água ---------------------------------------------------------------------
# Superfície translúcida com cor e opacidade pela espessura d'água vista (fundo
# da cena − superfície), a técnica clássica de água de jogo: margem rasa
# transparente mostrando a areia, fundo azul-esverdeado, reflexo do céu
# (Lumen na frente da translucidez, ligado no pós-processo).
# Testado antes: o Single Layer Water da Unreal 5.8 neste projeto calculava a
# luz que entra na água como ~0 (espalhamento 0.003 ou 0.05/cm davam a mesma
# imagem quase preta; com r.Water.SingleLayer 0 a imagem mudava) — fica pra
# investigar quando a água virar o plugin Water.
WATER_HLSL = r'''
float t = max(SceneD - PixelD, 0.0) * 0.01;          // m de água na linha de visão
float a = 1.0 - exp(-t / 1.2);
float3 c = lerp(Shallow, Deep, 1.0 - exp(-t / 2.5));
Opacity = clamp(a, 0.0, 0.96);
return c;
'''


def water_material(normal_tex):
    g = Graph('M_Water')
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    g.m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    wp = g.node(unreal.MaterialExpressionWorldPosition, -1200, 0)
    xy = g.node(unreal.MaterialExpressionComponentMask, -1050, 0, r=True, g=True)
    g.link(wp, '', xy, '')

    def ripple(tile_cm, sx, sy, y):
        dv = g.node(unreal.MaterialExpressionDivide, -900, y, const_b=tile_cm)
        g.link(xy, '', dv, 'A')
        pan = g.node(unreal.MaterialExpressionPanner, -750, y, speed_x=sx, speed_y=sy)
        g.link(dv, '', pan, 'Coordinate')
        s = g.node(unreal.MaterialExpressionTextureSample, -550, y, texture=normal_tex,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        g.link(pan, '', s, 'UVs')
        return s

    a = ripple(900.0, 0.012, 0.005, -150)
    b = ripple(1700.0, -0.006, 0.009, 100)
    mix = g.node(unreal.MaterialExpressionLinearInterpolate, -300, 0, const_alpha=0.5)
    g.link(a, 'RGB', mix, 'A')
    g.link(b, 'RGB', mix, 'B')
    flat = g.node(unreal.MaterialExpressionMultiply, -150, 0)
    g.link(mix, '', flat, 'A')
    k = g.node(unreal.MaterialExpressionConstant3Vector, -300, 120, constant=unreal.LinearColor(0.45, 0.45, 1.0, 1.0))
    g.link(k, '', flat, 'B')
    g.out(flat, '', unreal.MaterialProperty.MP_NORMAL)
    rough = g.node(unreal.MaterialExpressionConstant, -150, 200, r=0.04)
    g.out(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    sd = g.node(unreal.MaterialExpressionSceneDepth, -700, 400)
    pd = g.node(unreal.MaterialExpressionPixelDepth, -700, 500)
    shallow = g.node(unreal.MaterialExpressionVectorParameter, -700, 600, parameter_name='Shallow',
                     default_value=unreal.LinearColor(0.05, 0.16, 0.15, 1))
    deep = g.node(unreal.MaterialExpressionVectorParameter, -700, 700, parameter_name='Deep',
                  default_value=unreal.LinearColor(0.008, 0.035, 0.05, 1))
    c = g.custom(-300, 450, 'WaterDepthColor', WATER_HLSL, ['SceneD', 'PixelD', 'Shallow', 'Deep'], F3, [('Opacity', F1)])
    g.link(sd, '', c, 'SceneD')
    g.link(pd, '', c, 'PixelD')
    g.link(shallow, '', c, 'Shallow')
    g.link(deep, '', c, 'Deep')
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, 'Opacity', unreal.MaterialProperty.MP_OPACITY)
    return g.finish('water material')


# ---- objetos (árvores, rochas, cristais, marcadores de base) ------------------
# Cor no vértice: a Unreal grava em sRGB 8 bits e o nó devolve o valor
# codificado; o código decodifica pra linear. Alfa do vértice = brilho
# (cristal, faixa e farol da base). Tinta por instância (3 floats: cor do
# jogador, variação das árvores). Neve no que olha pra cima no inverno.
PROP_HLSL = r'''
float3 v = VC.rgb;
float3 lin = (v <= 0.04045) ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
float3 col = lin * Tint;
float snow = Snow * saturate((N.z - 0.45) * 4.0) * (1.0 - VA);
col = lerp(col, float3(0.75, 0.78, 0.82), snow);
Emissive = col * VA * Glow;
Rough = lerp(0.85, 0.3, VA);
return col;
'''


def prop_material():
    g = Graph('M_Prop')
    vc = g.node(unreal.MaterialExpressionVertexColor, -700, -100)
    tint = g.node(unreal.MaterialExpressionPerInstanceCustomData3Vector, -700, 0)
    tint.set_editor_property('data_index', 0)
    tint.set_editor_property('const_default_value', unreal.LinearColor(1, 1, 1, 1))
    vn = g.node(unreal.MaterialExpressionVertexNormalWS, -700, 100)
    snow = g.node(unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name='Snow', default_value=0.0)
    # brilho físico: com a exposição de dia (EV100 ~15) um emissivo precisa
    # de ~milhares de cd/m² pra parecer aceso
    glow = g.node(unreal.MaterialExpressionScalarParameter, -700, 300, parameter_name='Glow', default_value=6000.0)
    c = g.custom(-300, 0, 'PropShade', PROP_HLSL, ['VC', 'VA', 'Tint', 'N', 'Snow', 'Glow'], F3, [('Emissive', F3), ('Rough', F1)])
    g.link(vc, '', c, 'VC')
    g.link(vc, 'A', c, 'VA')
    g.link(tint, '', c, 'Tint')
    g.link(vn, '', c, 'N')
    g.link(snow, '', c, 'Snow')
    g.link(glow, '', c, 'Glow')
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, 'Emissive', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    # sem esta marca, no jogo a malha instanciada cai no material padrão
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    return g.finish('prop material')


def table_material():
    """Mesa escura sob a maquete (o mapa "flutua" sobre ela)."""
    g = Graph('M_Table')
    col = g.node(unreal.MaterialExpressionConstant3Vector, -300, 0, constant=unreal.LinearColor(0.025, 0.026, 0.028, 1))
    g.out(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    r = g.node(unreal.MaterialExpressionConstant, -300, 150, r=0.9)
    g.out(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    return g.finish('table material')


def physical_ev100(sun_elevation_deg, sun_lux, sky_lux=15000.0, transmittance=0.85):
    """EV100 de fotômetro incidente (C = 250) pela iluminância horizontal (o
    mesmo do BlastLab, build_scene.py; Lagarde & de Rousiers 2014)."""
    e = sun_lux * math.sin(math.radians(sun_elevation_deg)) * transmittance + sky_lux
    return math.log2(e * 100.0 / 250.0)


def build():
    tex = {k: import_texture(k + '.png', 'T_' + k.capitalize()) for k in ('grass', 'dirt', 'sand', 'rock')}
    water_n = import_texture('water_n.png', 'T_WaterN', normal=True)
    terrain_material(tex)
    water_material(water_n)
    prop_material()
    table = table_material()

    if eal.does_asset_exist(MAP):
        levels.load_level(MAP)
        for a in actors.get_all_level_actors():
            actors.destroy_actor(a)
        log('map emptied')
    else:
        levels.new_level(MAP)
        log('map created')

    # Sol: 42° de elevação, vindo da esquerda e um pouco de trás da câmera
    # padrão (que olha pra +X): sombras caem pra direita, o relevo lê bem.
    elev = 42.0
    sun_lux = 120000.0
    az = math.radians(-70.0)       # direção (no plano) de onde vem a luz
    to_sun = unreal.Vector(math.cos(az) * math.cos(math.radians(elev)),
                           math.sin(az) * math.cos(math.radians(elev)),
                           math.sin(math.radians(elev)))
    rot = unreal.MathLibrary.make_rot_from_x(unreal.Vector(-to_sun.x, -to_sun.y, -to_sun.z))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), rot)
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property('intensity', sun_lux)
    lc.set_editor_property('atmosphere_sun_light', True)
    lc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label('HeightFog')
    # névoa bem leve: a câmera de RTS olha de ~1 km; o padrão apagava o mapa
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.0025)
    fc.set_editor_property('fog_height_falloff', 0.25)

    # Exposição física travada (mínimo = máximo): o mapa não muda de brilho
    # quando a câmera passa por cima de água, neve ou da mesa escura.
    ev = physical_ev100(elev, sun_lux)
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ('auto_exposure_min_brightness', ev),
                        ('auto_exposure_max_brightness', ev),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        ('bloom_intensity', 0.2),
                        # Exposição local (Unreal 5, tone mapping local por grade
                        # bilateral): levanta as sombras sem estourar o sol. Câmera
                        # de RTS precisa ler o que está na sombra do penhasco; a
                        # curva padrão esmagava o chão sombreado (albedo ~7%) no preto.
                        ('local_exposure_shadow_contrast_scale', 0.6),
                        ('local_exposure_highlight_contrast_scale', 0.9),
                        ('auto_exposure_bias', 0.4),
                        # reflexo do Lumen na superfície da água (translúcida)
                        ('lumen_front_layer_translucency_reflections', True)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    pp.set_editor_property('settings', s)

    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
    t = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -9000), unreal.Rotator(0, 0, 0))
    t.set_actor_label('Table')
    t.set_actor_scale3d(unreal.Vector(400, 400, 1))     # 40 km
    t.static_mesh_component.set_static_mesh(plane)
    t.static_mesh_component.set_material(0, table)
    for a in (sun, sky, sl, fog, pp, t):
        a.set_folder_path('Environment')
    levels.save_current_level()
    unreal.EditorAssetLibrary.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('map saved, EV100 %.2f' % ev)


build()
