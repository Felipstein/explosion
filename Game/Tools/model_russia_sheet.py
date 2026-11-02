"""A folha dos veículos da Rússia lado a lado com o tanque da USA e o Type 99 da China (01/11/2026; Cycles na CPU: não usa a placa).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_russia_sheet.py -- \
      <saida_sem_extensao> usa_mbt china_type99 russia_t90m [russia_t14 ...]

Cada argumento é uma pasta de captures/game/models (com o preview.json do modelo: as peças e onde ficam). Sai:
  <saida>_game.png  a câmera do jogo (37,5°, 50° de campo, ~3 cm/px como no jogo), todos virados 3/4 pra câmera, numa fileira;
  <saida>_top.png   de cima (ortográfica), a mesma fileira, todos de frente pra direita;
  <saida>.json      onde fica cada um na imagem (o rótulo, o x do centro em px) e a fração da cor do jogador de cima.
O material aproxima o do jogo (import_provisional.py): as cores lineares de cada instância (RuPaint, CnTankPaint, TankPaint...), a
camuflagem no material (a Rússia: Camo 2, as manchas de 1,8 m pretas e cinza-claras pelo mesmo corte do ruído; a China: o digital
de 16 cm), o desgaste da quina, o pó da fresta e a oclusão da cor de vértice (as máscaras do model_detail.py), a cor do jogador (a
USA azul, a China e a Rússia vermelhas) e o sol baixo de contraluz 3/4.
"""
import json
import math
import os
import sys

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
OUT, KEYS = args[0], args[1:]
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MODELS = os.path.join(REPO, 'captures', 'game', 'models')

PLAYER = {'usa': (0.05, 0.10, 0.32), 'cn': (0.42, 0.03, 0.025), 'ru': (0.42, 0.03, 0.025)}
# as instâncias do jogo (cor linear, rugosidade, metal) por facção e slot do GLB
SLOTS = {
    'ru': {'Paint': ((0.118, 0.140, 0.158), 0.6, 0.0), 'Metal': ((0.05, 0.052, 0.055), 0.45, 0.6), 'Red': ((0.30, 0.03, 0.025), 0.6, 0.0),
           'White': ((0.62, 0.62, 0.60), 0.6, 0.0)},
    'cn': {'CnTankPaint': ((0.075, 0.085, 0.048), 0.6, 0.0), 'CnTankCamo': ((0.040, 0.048, 0.028), 0.65, 0.0),
           'CnTankMetal': ((0.05, 0.05, 0.045), 0.45, 0.6)},
    'usa': {'Paint': ((0.40, 0.405, 0.38), 0.55, 0.0), 'Metal': ((0.114, 0.114, 0.107), 0.45, 0.6)},
}
COMMON = {'Track': ((0.036, 0.034, 0.030), 0.85, 0.3), 'Rubber': ((0.03, 0.03, 0.028), 0.92, 0.0), 'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3),
          'Lens': ((0.008, 0.010, 0.013), 0.06, 0.0), 'Cloth': ((0.305, 0.216, 0.122), 0.9, 0.0),
          'Light': ((0.8, 0.7, 0.55), 0.4, 0.0), 'Beacon': ((0.8, 0.4, 0.05), 0.4, 0.0), 'Yellow': ((0.525, 0.30, 0.019), 0.55, 0.0)}
CAMO = {'ru': ('blotch', 1.8), 'cn': ('digital', 1.1)}
TEAM_SLOTS = {'Team', 'Blue'}


def faction(key):
    return 'ru' if key.startswith('russia') else ('cn' if key.startswith('china') else 'usa')


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def load_model(key):
    folder = os.path.join(MODELS, key)
    spec = json.load(open(os.path.join(folder, 'preview.json'), encoding='utf-8'))
    objs = []
    for item in spec['parts']:
        name, at = item[0], item[1]
        if '/' in name:
            path = os.path.join(REPO, name + '.glb')
        else:
            path = os.path.join(folder, name + '.glb')
        if not os.path.exists(path):
            continue
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=path)
        new = [o for o in bpy.data.objects if o not in before]
        for o in new:
            if o.parent is None:
                o.location = Vector(at) + o.location
        objs += new
    bpy.context.view_layer.update()
    return objs


def make_material(fac, slot, cache={}):
    k = (fac, slot)
    if k in cache:
        return cache[k]
    m = bpy.data.materials.new('%s:%s' % (fac, slot))
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    col, rough, metal = SLOTS[fac].get(slot) or COMMON.get(slot) or ((0.3, 0.3, 0.3), 0.6, 0.0)
    if slot in TEAM_SLOTS:
        col, rough = PLAYER[fac], 0.6
    if slot == 'Cloth' and fac == 'usa':
        col = tuple(c * 0.8 for c in PLAYER['usa'])
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = metal
    if slot in ('Light', 'Beacon'):
        bsdf.inputs['Emission Color'].default_value = (1.0, 0.6 if slot == 'Beacon' else 0.85, 0.3, 1)
        bsdf.inputs['Emission Strength'].default_value = 8.0
    base_rgba = tuple(col) + (1.0,)
    src = None
    paint = slot in ('Paint', 'CnTankPaint') and fac in CAMO
    if paint:
        kind, scale = CAMO[fac]
        tc = nt.nodes.new('ShaderNodeTexCoord')
        nz = nt.nodes.new('ShaderNodeTexNoise')
        nz.inputs['Detail'].default_value = 2.0
        nz.inputs['Roughness'].default_value = 0.5
        if kind == 'digital':
            snap = nt.nodes.new('ShaderNodeVectorMath')
            snap.operation = 'SNAP'
            snap.inputs[1].default_value = (0.16, 0.16, 0.16)
            nt.links.new(tc.outputs['Object'], snap.inputs[0])
            nt.links.new(snap.outputs['Vector'], nz.inputs['Vector'])
            nz.inputs['Scale'].default_value = 0.9
        else:
            nt.links.new(tc.outputs['Object'], nz.inputs['Vector'])
            nz.inputs['Scale'].default_value = 1.0 / scale
        dark = nt.nodes.new('ShaderNodeMath')
        dark.operation = 'GREATER_THAN'
        dark.inputs[1].default_value = 0.53
        nt.links.new(nz.outputs['Fac'], dark.inputs[0])
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.inputs['A'].default_value = base_rgba
        mix.inputs['B'].default_value = (0.022, 0.024, 0.026, 1) if fac == 'ru' else (0.040, 0.048, 0.028, 1)
        nt.links.new(dark.outputs[0], mix.inputs['Factor'])
        src = mix
        if kind == 'blotch':
            light = nt.nodes.new('ShaderNodeMath')
            light.operation = 'LESS_THAN'
            light.inputs[1].default_value = 0.455
            nt.links.new(nz.outputs['Fac'], light.inputs[0])
            mix2 = nt.nodes.new('ShaderNodeMix')
            mix2.data_type = 'RGBA'
            nt.links.new(mix.outputs['Result'], mix2.inputs['A'])
            mix2.inputs['B'].default_value = (0.30, 0.32, 0.32, 1)
            nt.links.new(light.outputs[0], mix2.inputs['Factor'])
            src = mix2
    # as máscaras (a cor de vértice: R = 1 − quina, G = oclusão, B = 1 − fresta), como o PREVIEW_MASKS=game do model_preview.py
    ca = nt.nodes.new('ShaderNodeVertexColor')
    ca.layer_name = ''
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(ca.outputs['Color'], sep.inputs['Color'])
    wear = nt.nodes.new('ShaderNodeMath'); wear.operation = 'SUBTRACT'; wear.inputs[0].default_value = 1.0
    nt.links.new(sep.outputs['Red'], wear.inputs[1])
    wf = nt.nodes.new('ShaderNodeMath'); wf.operation = 'MULTIPLY'; wf.inputs[1].default_value = 0.5
    nt.links.new(wear.outputs[0], wf.inputs[0])
    m1 = nt.nodes.new('ShaderNodeMix'); m1.data_type = 'RGBA'
    if src:
        nt.links.new(src.outputs['Result'], m1.inputs['A'])
    else:
        m1.inputs['A'].default_value = base_rgba
    m1.inputs['B'].default_value = tuple(min(c * 1.6 + 0.035, 0.6) for c in col) + (1.0,)
    nt.links.new(wf.outputs[0], m1.inputs['Factor'])
    cav = nt.nodes.new('ShaderNodeMath'); cav.operation = 'SUBTRACT'; cav.inputs[0].default_value = 1.0
    nt.links.new(sep.outputs['Blue'], cav.inputs[1])
    cf = nt.nodes.new('ShaderNodeMath'); cf.operation = 'MULTIPLY'; cf.inputs[1].default_value = 0.6
    nt.links.new(cav.outputs[0], cf.inputs[0])
    m2 = nt.nodes.new('ShaderNodeMix'); m2.data_type = 'RGBA'
    nt.links.new(m1.outputs['Result'], m2.inputs['A'])
    m2.inputs['B'].default_value = (0.40, 0.31, 0.21, 1.0)
    nt.links.new(cf.outputs[0], m2.inputs['Factor'])
    ao = nt.nodes.new('ShaderNodeMix'); ao.data_type = 'RGBA'; ao.blend_type = 'MULTIPLY'
    ao.inputs['Factor'].default_value = 0.35
    nt.links.new(m2.outputs['Result'], ao.inputs['A'])
    gg = nt.nodes.new('ShaderNodeCombineColor')
    for c in ('Red', 'Green', 'Blue'):
        nt.links.new(sep.outputs['Green'], gg.inputs[c])
    nt.links.new(gg.outputs['Color'], ao.inputs['B'])
    nt.links.new(ao.outputs['Result'], bsdf.inputs['Base Color'])
    cache[k] = m
    return m


def assign(objs, fac):
    for o in objs:
        if o.type != 'MESH':
            continue
        for i, ms in enumerate(o.material_slots):
            if ms.material is None:
                continue
            slot = ms.material.name.split('.')[0]
            o.material_slots[i].material = make_material(fac, slot)


def group_bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == 'MESH' for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def place(objs, at, yaw_deg):
    """Gira o grupo em volta da origem (o centro do veículo no chão) e leva pra 'at' (Blender)."""
    from mathutils import Matrix
    M = Matrix.Translation(Vector(at)) @ Matrix.Rotation(math.radians(yaw_deg), 4, 'Z')
    for o in objs:
        if o.parent is None:
            o.matrix_world = M @ o.matrix_world
    bpy.context.view_layer.update()


def scene():
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 48
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = 'AgX'
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.36, 0.46, 0.62, 1)
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.9
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.2
    sun.data.angle = math.radians(2.5)
    sun.data.color = (1.0, 0.9, 0.78)
    # o sol de contraluz 3/4 dos game shots, ~35° de altura
    sun.rotation_euler = (math.radians(55), 0, math.radians(150))
    sc.collection.objects.link(sun)
    bpy.ops.mesh.primitive_plane_add(size=800, location=(0, 0, 0))
    g = bpy.context.active_object
    gm = bpy.data.materials.new('ground')
    gm.use_nodes = True
    gb = gm.node_tree.nodes['Principled BSDF']
    gb.inputs['Base Color'].default_value = (0.36, 0.27, 0.165, 1)          # o GlaEarth (a areia batida do jogo)
    gb.inputs['Roughness'].default_value = 0.95
    g.data.materials.append(gm)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


clear()
cam = scene()
groups = []
for key in KEYS:
    objs = load_model(key)
    assign(objs, faction(key))
    lo, hi = group_bounds(objs)
    groups.append((key, objs, lo, hi))

# a fileira: cada um com o vão pela largura (de cima: de frente pra direita, +X; o vão é no Y)
sc = bpy.context.scene
labels = {}
GAP = 1.6
y = 0.0
slots = []
for key, objs, lo, hi in groups:
    w = hi.y - lo.y
    cy = (lo.y + hi.y) / 2
    slots.append((key, y - w / 2 - cy, lo, hi))
    y -= w + GAP
for (key, objs, lo, hi), (_, yy, _, _) in zip(groups, slots):
    place(objs, (-(lo.x + hi.x) / 2, yy, 0.0), 0.0)
alll = [o for _, objs, _, _ in groups for o in objs]
LO, HI = group_bounds(alll)
C = (LO + HI) / 2
C.z = 0.0

# ---------------------------------------------------------------- de cima (ortográfica)
cam.data.type = 'ORTHO'
cam.data.ortho_scale = max(HI.y - LO.y, (HI.x - LO.x) * 3.0) * 1.04
sc.render.resolution_x, sc.render.resolution_y = 3000, 1000
cam.location = (C.x, C.y, 80.0)
cam.rotation_euler = (0.0, 0.0, math.radians(-90))       # o +X do veículo pra direita da imagem... (a frente pra cima da imagem)
cam.data.clip_end = 500
TOP_MPP = cam.data.ortho_scale / sc.render.resolution_x
sc.render.filepath = OUT + '_top.png'
bpy.ops.render.render(write_still=True)
top_labels = []
for (key, objs, lo, hi) in groups:
    lo2, hi2 = group_bounds(objs)
    c = (lo2 + hi2) / 2
    v = world_to_camera_view(sc, cam, c)
    top_labels.append((key, round(v.x * sc.render.resolution_x), round((1 - v.y) * sc.render.resolution_y)))

# ---------------------------------------------------------------- a câmera do jogo: cada um virado 3/4 (a frente pra câmera e pra
# esquerda), 37,5° de inclinação, 50° de campo
for (key, objs, lo, hi), (_, yy, _, _) in zip(groups, slots):
    lo2, hi2 = group_bounds(objs)
    c = Vector(((lo2.x + hi2.x) / 2, (lo2.y + hi2.y) / 2, 0.0))
    from mathutils import Matrix
    M = Matrix.Translation(c) @ Matrix.Rotation(math.radians(-150), 4, 'Z') @ Matrix.Translation(-c)
    for o in objs:
        if o.parent is None:
            o.matrix_world = M @ o.matrix_world
bpy.context.view_layer.update()
LO, HI = group_bounds(alll)
C = (LO + HI) / 2
C.z = 0.8
cam.data.type = 'PERSP'
cam.data.sensor_fit = 'HORIZONTAL'
cam.data.angle = math.radians(50.0)
pitch = math.radians(37.5)
width = (HI.y - LO.y) * 1.06
dist = width / 2 / math.tan(cam.data.angle / 2)
d = Vector((math.cos(pitch), 0.0, -math.sin(pitch)))          # a câmera olha pra +X, inclinada 37,5°
cam.location = C - d * dist
cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
sc.render.resolution_x, sc.render.resolution_y = 3000, 1100
sc.render.filepath = OUT + '_game.png'
bpy.ops.render.render(write_still=True)
game_labels = []
for (key, objs, lo, hi) in groups:
    lo2, hi2 = group_bounds(objs)
    c = Vector(((lo2.x + hi2.x) / 2, (lo2.y + hi2.y) / 2, 0.0))
    v = world_to_camera_view(sc, cam, c)
    game_labels.append((key, round(v.x * sc.render.resolution_x), round((1 - v.y) * sc.render.resolution_y)))
print('SHEET m/px no centro: %.3f' % (2 * dist * math.tan(cam.data.angle / 2) / sc.render.resolution_x))
json.dump({'top': top_labels, 'game': game_labels, 'top_m_per_px': TOP_MPP}, open(OUT + '.json', 'w', encoding='utf-8'), indent=1)
print('SHEET fim', OUT)
