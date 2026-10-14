"""O mapa de teste com o visual do jogo: /Game/Maps/Look_Desert (pedido do Felipe, 12/10).

As explosões eram testadas nos campos do BlastLab (luz e chão próprios). Este mapa usa o que o jogo aprovou:
- a luz: o plugin RTSLook do jogo (BlastLab/Plugins/RTSLook = junções pro Source e o Content de
  Game/Plugins/RTSLook), um ALookDirector com a receita desert_golden_hour_v2_aprovada (a do World_TestSalt2);
- o chão: a areia v1 aprovada, o Custom node congelado (Game/Tools/look_materials/sand_desert_v1_ground.hlsl,
  SALB = 1) com as texturas do Poly Haven que o WorldForge baixou, e uma rua de asfalto rachado (as 3 escolhidas
  por ele, 12/10, são batalhas em rua de vila: road_town_battle, usa_vs_china, r1_raid_slabs);
- a cena: o kit civil da vila do Look (captures/look/civil_glb), o tanque USA provisório (captures/look/tank_glb,
  já importado por Tools/import_look_tank.py) e picapes do GLA.
O chão é um plano em z = 0 (o BlastLabGround cai no plano z = 0 sem Landscape: clique e destroços funcionam).

Rodar: blast.ps1 py Tools\\build_look_desert.py
"""
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_util import REPO  # noqa: E402

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

MAP = '/Game/Maps/Look_Desert'
ROOT = '/Game/Look'
ASSETS = os.path.join(REPO, 'WorldForge', 'Intermediate', 'Assets')
GEN = os.path.join(REPO, 'BlastLab', 'Intermediate', 'LookDesert')
HLSL = os.path.join(REPO, 'Game', 'Tools', 'look_materials', 'sand_desert_v1_ground.hlsl')
SAND = json.load(open(os.path.join(REPO, 'Game', 'Tools', 'look_materials', 'sand_desert_v1.json'), encoding='utf-8'))
RECIPE = '/RTSLook/Recipes/DA_Look_desert_golden_hour_v2_aprovada'
# a câmera inicial do BlastLab (-Cam=...,azim 0.8,...): da câmera pro alvo, giro de -134° na Unreal; o sol da receita
# é dado em relação a ela (LookDirector.InitialCameraYaw)
CAM_YAW = -134.0
# a planta da rua: 160 m em volta da origem, 1024 px (15,6 cm por pixel)
LX, LY, LS, LPX = -80.0, -80.0, 160.0, 1024


def log(*a):
    unreal.log('LOOKDESERT ' + ' '.join(str(x) for x in a))


# ---- as texturas --------------------------------------------------------------------------
def import_tex(path, folder, kind):
    name = 'T_' + os.path.splitext(os.path.basename(path))[0]
    asset = f'{folder}/{name}'
    if eal.does_asset_exist(asset):
        return unreal.load_asset(asset)
    t = unreal.AssetImportTask()
    for k, v in (('filename', path), ('destination_path', folder), ('destination_name', name),
                 ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(asset)
    if kind == 'normal':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif kind == 'linear':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property('srgb', False)
    elif kind == 'masks':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property('srgb', False)
    eal.save_loaded_asset(tex)
    log('textura', name)
    return tex


def surface(name, part, kind):
    return import_tex(os.path.join(ASSETS, name, f'{name}_{part}_4k.jpg'), f'{ROOT}/Surfaces/{name}', kind)


def sampler_for(tex):
    cs = tex.get_editor_property('compression_settings')
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if tex.get_editor_property('srgb'):
        return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


# ---- a planta da rua: gerada antes pelo Tools/look_desert_layout.py (Python do sistema) ---------
def layout_png():
    path = os.path.join(GEN, 'T_LookDesert_Layout.png')
    if not os.path.exists(path):
        raise RuntimeError('rode antes: python BlastLab/Tools/look_desert_layout.py')
    return path


# ---- o material do chão (a areia v1 congelada) ----------------------------------------------
def fresh_material(name, folder):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def ground_material():
    code = open(HLSL, encoding='utf-8').read()
    cracks = os.path.join(REPO, 'captures', 'look', 'textures')
    tex = {
        'SandD': surface('sandy_gravel_02', 'diff', 'color'), 'SandN': surface('sandy_gravel_02', 'nor_dx', 'normal'),
        'SandM': surface('sandy_gravel_02', 'arm', 'linear'), 'Beach': surface('aerial_beach_01', 'diff', 'color'),
        'BeachN': surface('aerial_beach_01', 'nor_dx', 'normal'),
        'GravD': surface('gravelly_sand', 'diff', 'color'), 'GravN': surface('gravelly_sand', 'nor_dx', 'normal'),
        'GravM': surface('gravelly_sand', 'arm', 'linear'),
        'AsphD': surface('road_damaged_2', 'diff', 'color'), 'AsphN': surface('road_damaged_2', 'nor_dx', 'normal'),
        'AsphM': surface('road_damaged_2', 'arm', 'linear'), 'AerAsph': surface('aerial_asphalt_01', 'diff', 'color'),
        'Layout': import_tex(layout_png(), f'{ROOT}/Textures', 'masks'),
        'Cracks': import_tex(os.path.join(cracks, 'T_LL_Cracks.png'), f'{ROOT}/Textures', 'masks'),
        'CracksN': import_tex(os.path.join(cracks, 'T_LL_Cracks_N.png'), f'{ROOT}/Textures', 'normal'),
    }
    sa = SAND['constants']['SAND_ALBEDO_linear']
    consts = [('LX', LX), ('LY', LY), ('LS', LS), ('SGR', 1.0), ('SGG', 1.0), ('SGB', 1.0), ('AGR', 1.0), ('AGG', 1.0), ('AGB', 1.0),
              ('SALB', 1.0), ('SAR', sa[0]), ('SAG', sa[1]), ('SAB', sa[2]), ('RIPPLE', float(SAND['constants']['SAND_RIPPLES']))]
    m = fresh_material('M_Look_Ground', f'{ROOT}/Materials')
    m.set_editor_property('tangent_space_normal', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', code)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'Look desert ground (sand_desert_v1)')
    names = ['WP', 'Depth', 'VN'] + list(tex) + [k for k, _ in consts]
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
    yy = -900
    for key, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('Depth', unreal.MaterialExpressionPixelDepth),
                     ('VN', unreal.MaterialExpressionVertexNormalWS)):
        e = mel.create_material_expression(m, cls, -700, yy)
        mel.connect_material_expressions(e, '', custom, key)
        yy += 120
    for key, t in tex.items():
        o = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -700, yy)
        o.set_editor_property('texture', t)
        o.set_editor_property('sampler_type', sampler_for(t))
        mel.connect_material_expressions(o, '', custom, key)
        yy += 120
    for key, v in consts:
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -700, yy)
        c.set_editor_property('r', v)
        mel.connect_material_expressions(c, '', custom, key)
        yy += 100
    mel.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(custom, 'AO', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(custom, 'Nrm', unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material do chão pronto')
    return m


def flat_material(name, color, rough, metal=0.0):
    m = fresh_material(name, f'{ROOT}/Materials')
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


def srgb_lin(c):
    return tuple(((v / 255.0 + 0.055) / 1.055) ** 2.4 if v / 255.0 > 0.04045 else v / 255.0 / 12.92 for v in c)


# o aspecto de cada slot (os do Game/Tools/look_lab.py: TANK_SLOTS e as casas)
SLOTS = {'Paint': ((126, 126, 119), 0.62, 0.0), 'Team': ((86, 108, 150), 0.66, 0.0), 'Dark': ((40, 40, 38), 0.7, 0.0),
         'Metal': ((95, 95, 92), 0.45, 0.7), 'Cloth': ((64, 86, 132), 0.92, 0.0), 'Lens': ((30, 60, 110), 0.15, 0.0),
         'Track': ((55, 52, 48), 0.6, 0.5), 'Char': ((26, 23, 20), 0.92, 0.0), 'Rust': ((74, 46, 30), 0.85, 0.15),
         'Blade': ((196, 160, 40), 0.6, 0.2), 'Plaster': ((178, 150, 112), 0.92, 0.0), 'Wood': ((92, 66, 42), 0.8, 0.0)}


def slot_materials():
    return {k: flat_material(f'M_Look_{k}', srgb_lin(c), r, mt) for k, (c, r, mt) in SLOTS.items()}


def apply_slots(comp, mesh, mats):
    for i, sm in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(sm.get_editor_property('material_slot_name'))
        key = next((k for k in mats if slot.lower().startswith(k.lower())), None)
        if key:
            comp.set_material(i, mats[key])


# ---- as malhas ------------------------------------------------------------------------------
def import_civil():
    out = {}
    folder = f'{ROOT}/Civil'
    for f in sorted(os.listdir(os.path.join(REPO, 'captures', 'look', 'civil_glb'))):
        if not f.endswith('.glb'):
            continue
        name = os.path.splitext(f)[0]
        path = f'{folder}/{name}/StaticMeshes/{name}'
        if not eal.does_asset_exist(path):
            t = unreal.AssetImportTask()
            for k, v in (('filename', os.path.join(REPO, 'captures', 'look', 'civil_glb', f)), ('destination_path', folder),
                         ('destination_name', name), ('automated', True), ('replace_existing', True), ('save', True)):
                t.set_editor_property(k, v)
            tools.import_asset_tasks([t])
            ms = [unreal.load_asset(p) for p in t.get_editor_property('imported_object_paths') or []]
            ms = [m for m in ms if isinstance(m, unreal.StaticMesh)]
            if ms:
                path = ms[0].get_path_name().split('.')[0]
        mesh = unreal.load_asset(path)
        if mesh:
            out[name[len('SM_civ_'):]] = mesh
            b = mesh.get_bounding_box()
            slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
            log(f'{name}: {(b.max.x - b.min.x) / 100:.1f} x {(b.max.y - b.min.y) / 100:.1f} x {(b.max.z - b.min.z) / 100:.1f} m, slots {slots}')
    return out


def put(mesh, x, y, yaw, mats, label, folder, z=0.0, scale=1.0):
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * 100, y * 100, z * 100),
                                      unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.static_mesh_component.set_static_mesh(mesh)
    if scale != 1.0:
        a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    apply_slots(a.static_mesh_component, mesh, mats)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def tank(x, y, yaw, mats, label):
    for part in ('Hull', 'Turret', 'Barrel', 'Track_l', 'Track_r', 'Wheels'):
        mesh = unreal.load_asset(f'/Game/Units/LL_USATank/SM_LL_USATank_{part}/StaticMeshes/SM_LL_USATank_{part}')
        if mesh:
            put(mesh, x, y, yaw, mats, f'{label}_{part}', 'Units')


# a vila em volta da rua (x ao longo da rua, y pra cada lado; as portas em +Y da casa viram pra rua)
HOUSES = [('house_2story', -18, 13, 180), ('house_1story', -5, 12, 180), ('house_long', 9, 14, 180), ('house_porch', 33, 13, 180),
          ('house_1story', 46, 12, 180), ('house_2story', 58, 14, 180), ('house_long', -40, 14, 180),
          ('house_1story', -14, -13, 0), ('house_porch', 0, -12, 0), ('house_2story', 14, -14, 0), ('house_long', 28, -13, 0),
          ('house_1story', -44, -12, 0), ('house_2story', -56, -14, 0), ('house_porch', 46, -13, 0),
          ('house_1story', 10, 30, 90), ('house_2story', 30, 32, 0), ('house_long', -10, -32, 90), ('house_1story', -26, 28, 0)]
WALLS = [('wall', -28, 9, 0), ('wall', 20, -9, 0), ('wall_gate', 22, 9, 0), ('wall', 52, -9, 0), ('wall', -50, 9, 0)]
PROPS = [('pole', -10, 6, 0), ('pole', 25, 6, 0), ('pole', 60, 6, 0), ('pole', -45, -6, 0), ('barrels', 4, 7.5, 20),
         ('crates', -24, -8, 0), ('sandbags', 16, -7.5, 0), ('car_burnt', -8, -7, 25)]
TANKS = [(-26, -1.8, 0), (-38, 1.8, 0), (-50, -1.8, 0), (-62, 1.8, 0)]
PICKUPS = [(16, 2.5, 100), (24, -2.0, 85), (31, 3.0, 110), (40, -1.5, 90)]   # a picape é comprida em Y: giro de 90° fica ao longo da rua


def build():
    mats = slot_materials()
    ground = ground_material()
    civil = import_civil()
    if eal.does_asset_exist(MAP):
        levels.load_level(MAP)
        for a in actors.get_all_level_actors():
            actors.destroy_actor(a)
    else:
        levels.new_level(MAP)
    # o chão: plano de 1,2 km (a névoa cobre a borda)
    plane = unreal.load_asset('/Engine/BasicShapes/Plane')
    g = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    g.static_mesh_component.set_static_mesh(plane)
    g.set_actor_scale3d(unreal.Vector(1200, 1200, 1))
    g.static_mesh_component.set_material(0, ground)
    g.set_actor_label('Ground')
    # a luz do jogo
    d = actors.spawn_actor_from_class(unreal.LookDirector, unreal.Vector(0, 0, 500))
    d.set_editor_property('recipe', unreal.load_asset(RECIPE))
    d.set_editor_property('initial_camera_yaw', CAM_YAW)
    d.set_actor_label('LookDirector')
    for name, x, y, yaw in HOUSES + WALLS:
        if name in civil:
            put(civil[name], x, y, yaw, mats, f'{name}_{x}_{y}', 'Village')
    for name, x, y, yaw in PROPS:
        if name in civil:
            put(civil[name], x, y, yaw, mats, f'{name}_{x}_{y}', 'Props')
    for i, (x, y, yaw) in enumerate(TANKS):
        tank(x, y, yaw, mats, f'USATank_{i}')
    for i, (x, y, yaw) in enumerate(PICKUPS):
        if 'pickup' in civil:
            put(civil['pickup'], x, y, yaw, mats, f'Pickup_{i}', 'Units')
    levels.save_current_level()
    log('mapa salvo', MAP)


build()
