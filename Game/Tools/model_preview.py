"""Folha de conferência de um modelo provisório (Cycles na CPU: não usa a placa).

Uso: blender -b --factory-startup -P Game/Tools/model_preview.py -- <pasta_glb> <pasta_saida> <modelo> [tanque_usa_dir]
Sai com as vistas da ficha de cada modelo (ortográficas em fundo cinza e as 3/4 em perspectiva) mais a câmera
do jogo (37,5°) no chão de areia com o tanque USA do lado, pra medir a proporção. As peças que se mexem entram
no lugar delas (a mesma origem do modelo). Convenção: Unreal X frente, Y direita; no Blender a direita é −Y.
"""
import math
import os
import sys

import bpy
from mathutils import Quaternion, Vector

args = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, MODEL = args[0], args[1], args[2]
TANK = args[3] if len(args) > 3 else ''
os.makedirs(OUT, exist_ok=True)

USA_PARTS = ['SM_LL_USATank_Hull', 'SM_LL_USATank_Turret', 'SM_LL_USATank_Barrel', 'SM_LL_USATank_Wheels',
             'SM_LL_USATank_Track_l', 'SM_LL_USATank_Track_r']

# por modelo: as peças (GLB, posição), as vistas e onde o tanque USA fica na câmera do jogo
# vista ortográfica: ('ortho', direção do olhar em Blender, o "cima" da imagem, (largura, altura))
# vista em perspectiva: ('persp', inclinação, giro, (largura, altura), campo de visão, margem)
MODELS = {
    'gla_tank': {
        'parts': [('SM_GLA_MBT_Hull', (0, 0, 0)), ('SM_GLA_MBT_Turret', (0, 0, 0)), ('SM_GLA_MBT_Barrel', (0, 0, 0)),
                  ('SM_GLA_MBT_Wheels', (0, 0, 0)), ('SM_GLA_MBT_Track_l', (0, 0, 0)), ('SM_GLA_MBT_Track_r', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 32.0, 215.0, (1600, 900), 35.0, 0.95),
                  ('three_quarter_right', 'persp', 32.0, 145.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, -6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    'usa_dozer': {
        'parts': [('SM_USA_Dozer_Hull', (0, 0, 0)), ('SM_USA_Dozer_Blade', (-0.352, 0.0, 0.66)), ('SM_USA_Dozer_Beacon', (0.396, -0.264, 2.86)), ('SM_USA_Dozer_Wheels', (0, 0, 0)),
                  ('SM_USA_Dozer_Track_l', (0, 0, 0)), ('SM_USA_Dozer_Track_r', (0, 0, 0))],
        'views': [('front', 'ortho', (-1, 0, 0), (0, 0, 1), (900, 900)),
                  ('back', 'ortho', (1, 0, 0), (0, 0, 1), (900, 900)),
                  ('left', 'ortho', (0, -1, 0), (0, 0, 1), (1800, 820)),
                  ('top', 'ortho', (0, 0, -1), (0, -1, 0), (1800, 1000)),
                  ('three_quarter', 'persp', 27.0, 232.0, (1600, 1000), 30.0, 0.95),
                  ('three_quarter_right', 'persp', 30.0, 40.0, (1600, 900), 35.0, 0.95)],
        'usa_at': (0, 6.0, 0),
        'game': [('game_cam', 37.5, 35.0, 50.0, 0.8), ('game_cam_close', 37.5, 35.0, 28.0, 0.75)],
    },
    'gla_war_factory': {
        'parts': [('gla_war_factory', (0, 0, 0))],
        'views': [('top', 'ortho', (0, 0, -1), (-1, 0, 0), (1500, 1200)),           # a planta: frente embaixo, direita (+Y) à esquerda
                  ('front', 'ortho', (-1, 0, 0), (0, 0, 1), (1800, 700)),
                  ('side', 'ortho', (0, 1, 0), (0, 0, 1), (1800, 700)),              # a face da direita (+Y Unreal)
                  ('three_quarter', 'persp', 30.0, 145.0, (1600, 900), 32.0, 0.95),  # pela frente-direita, como a ficha
                  ('back_three_quarter', 'persp', 30.0, 35.0, (1600, 900), 32.0, 0.95)],
        'usa_at': (2.0, 0.0, 0),                                                  # o tanque saindo da rampa
        'game': [('game_cam', 37.5, 20.0, 50.0, 0.9), ('game_cam_close', 37.5, 20.0, 30.0, 0.8)],
        'scaffold': 'gla_war_factory_scaffold',
    },
}


def load(path, at):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location = Vector(at) + o.location
    return new


EMISSIVE = {'Beacon': ((1.0, 0.55, 0.1), 12.0), 'Light': ((1.0, 0.85, 0.55), 20.0)}


def fix_materials():
    for m in bpy.data.materials:
        base = m.name.split('.')[0]
        if base in EMISSIVE and m.node_tree:
            (r, g, b), strength = EMISSIVE[base]
            bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if bsdf:
                bsdf.inputs['Emission Color'].default_value = (r, g, b, 1)
                bsdf.inputs['Emission Strength'].default_value = strength


def scene_setup():
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 64
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = 'AgX'
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.45, 0.45, 0.46, 1)
    bg.inputs['Strength'].default_value = 1.0
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(3.0)
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam, sun, bg


def bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == 'MESH' for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def look(cam, center, direction, up, dist):
    """A câmera a 'dist' do centro, olhando ao longo de 'direction' (Blender), com 'up' pra cima na imagem."""
    d = Vector(direction).normalized()
    cam.location = Vector(center) - d * dist
    q = d.to_track_quat('-Z', 'Y')
    cam_up = q @ Vector((0, 1, 0))
    want = Vector(up) - d * Vector(up).dot(d)
    if want.length > 1e-6:
        want.normalize()
        ang = cam_up.angle(want)
        if cam_up.cross(want).dot(d) < 0:
            ang = -ang
        q = Quaternion(d, ang) @ q
    cam.rotation_euler = q.to_euler()


def ortho(cam, objs, direction, up, res, margin=1.12, floor=True):
    lo, hi = bounds(objs)
    if floor:
        lo.z = max(lo.z, 0.0)          # a saia do lote (abaixo do chão) não entra na vista
    center = (lo + hi) / 2
    cam.data.type = 'ORTHO'
    cam.data.sensor_fit = 'HORIZONTAL'
    d = Vector(direction).normalized()
    u = Vector(up) - d * Vector(up).dot(d)
    u.normalize()
    r = d.cross(u)
    pts = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    w = max(p.dot(r) for p in pts) - min(p.dot(r) for p in pts)
    h = max(p.dot(u) for p in pts) - min(p.dot(u) for p in pts)
    cam.data.ortho_scale = max(w, h * res[0] / res[1]) * margin
    cam.data.clip_end = 500
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, direction, up, 120.0)


def persp(cam, objs, pitch_deg, yaw_deg, res, fov=50.0, margin=0.9):
    lo, hi = bounds(objs)
    lo.z = max(lo.z, 0)
    center = (lo + hi) / 2
    radius = (hi - lo).length / 2
    cam.data.type = 'PERSP'
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(fov)
    cam.data.clip_end = 1000
    f = min(cam.data.angle, cam.data.angle * res[1] / res[0])
    dist = radius * margin / math.sin(f / 2)
    p, y = math.radians(pitch_deg), math.radians(yaw_deg)
    d = Vector((math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p)))     # pra onde a câmera olha
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, d, (0, 0, 1), dist)


def render(name):
    bpy.context.scene.render.filepath = os.path.join(OUT, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('PREVIEW', name)


def ground(bg, sun):
    bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
    g = bpy.context.active_object
    gm = bpy.data.materials.new('ground')
    gm.use_nodes = True
    gb = gm.node_tree.nodes['Principled BSDF']
    gb.inputs['Base Color'].default_value = (0.42, 0.33, 0.21, 1)
    gb.inputs['Roughness'].default_value = 0.95
    g.data.materials.append(gm)
    bg.inputs['Color'].default_value = (0.36, 0.48, 0.66, 1)
    sun.data.energy = 4.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    return g


spec = MODELS[MODEL]
cam, sun, bg = scene_setup()
objs = []
for name, at in spec['parts']:
    objs += load(os.path.join(SRC, name + '.glb'), at)
fix_materials()
bpy.context.view_layer.update()
lo, hi = bounds(objs)
print(f'MEASURE {MODEL}: comprimento {hi.x - lo.x:.2f} m (x {lo.x:.2f}..{hi.x:.2f}), largura {hi.y - lo.y:.2f} m, altura {hi.z:.2f} m')

# luz da ficha: de cima, da frente e um pouco da esquerda
sun.rotation_euler = (math.radians(35), math.radians(20), math.radians(-150))
for view in spec['views']:
    tag, kind = view[0], view[1]
    if kind == 'ortho':
        ortho(cam, objs, view[2], view[3], view[4])
    else:
        persp(cam, objs, view[2], view[3], view[4], fov=view[5], margin=view[6])
    render(tag)

# a câmera do jogo no chão de areia, com o tanque USA como régua
ground(bg, sun)
if TANK:
    tank_objs = []
    for n in USA_PARTS:
        tank_objs += load(os.path.join(TANK, n + '.glb'), (0, 0, 0))
    bpy.context.view_layer.update()
    tlo, thi = bounds(tank_objs)
    print(f'MEASURE tanque USA: comprimento {thi.x - tlo.x:.2f} m (casco {thi.x - tlo.x - 1.6:.2f} sem o cano), largura {thi.y - tlo.y:.2f} m, altura {thi.z:.2f} m')
    for o in tank_objs:
        if o.parent is None:
            o.location = o.location + Vector((spec['usa_at'][0], -spec['usa_at'][1], spec['usa_at'][2]))
    objs += tank_objs
for tag, pitch, yaw, fov, margin in spec['game']:
    persp(cam, objs, pitch, yaw, (1600, 900), fov=fov, margin=margin)
    render(tag)
# o prédio em obra: o andaime por cima, na câmera do jogo
if spec.get('scaffold') and os.path.exists(os.path.join(SRC, spec['scaffold'] + '.glb')):
    objs += load(os.path.join(SRC, spec['scaffold'] + '.glb'), (0, 0, 0))
    tag, pitch, yaw, fov, margin = spec['game'][0]
    persp(cam, objs, pitch, yaw, (1600, 900), fov=fov, margin=margin)
    render('game_cam_scaffold')
print('PREVIEW fim')
