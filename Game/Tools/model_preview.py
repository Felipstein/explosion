"""Folha de conferência de um modelo provisório (Cycles na CPU: não usa a placa).

Uso: blender -b --factory-startup -P Game/Tools/model_preview.py -- <pasta_glb> <pasta_saida> <modelo> [tanque_usa_dir]
Sai com as 5 vistas ortográficas da ficha (frente, cima, lado esquerdo, trás, 3/4 de frente-esquerda) em fundo
cinza, mais a câmera do jogo (37,5°) no chão de areia com o tanque USA do lado, pra medir a proporção.
As peças que se mexem entram no lugar delas (a mesma origem do tanque).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, MODEL = args[0], args[1], args[2]
TANK = args[3] if len(args) > 3 else ''
os.makedirs(OUT, exist_ok=True)

MODELS = {
    'gla_tank': [('SM_GLA_MBT_Hull', (0, 0, 0)), ('SM_GLA_MBT_Turret', (0, 0, 0)), ('SM_GLA_MBT_Barrel', (0, 0, 0)),
                 ('SM_GLA_MBT_Wheels', (0, 0, 0)), ('SM_GLA_MBT_Track_l', (0, 0, 0)), ('SM_GLA_MBT_Track_r', (0, 0, 0))],
}
USA_PARTS = ['SM_LL_USATank_Hull', 'SM_LL_USATank_Turret', 'SM_LL_USATank_Barrel', 'SM_LL_USATank_Wheels',
             'SM_LL_USATank_Track_l', 'SM_LL_USATank_Track_r']


def load(path, at):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location = Vector(at) + o.location
    return new


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
    """A câmera a 'dist' do centro, olhando ao longo de 'direction' (vetores em Blender), com 'up' pra cima na imagem."""
    d = Vector(direction).normalized()
    cam.location = Vector(center) - d * dist
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    # força o 'up': gira em volta do eixo de visão até o up pedido ficar pra cima
    q = d.to_track_quat('-Z', 'Y')
    cam_up = q @ Vector((0, 1, 0))
    want = Vector(up) - d * Vector(up).dot(d)
    if want.length > 1e-6:
        want.normalize()
        ang = cam_up.angle(want)
        if cam_up.cross(want).dot(d) < 0:
            ang = -ang
        from mathutils import Quaternion
        q = Quaternion(d, ang) @ q
    cam.rotation_euler = q.to_euler()


def ortho(cam, objs, direction, up, res, margin=1.12):
    lo, hi = bounds(objs)
    center = (lo + hi) / 2
    cam.data.type = 'ORTHO'
    cam.data.sensor_fit = 'HORIZONTAL'
    # a largura vista: a projeção da caixa no plano da imagem
    d = Vector(direction).normalized()
    u = Vector(up) - d * Vector(up).dot(d)
    u.normalize()
    r = d.cross(u)
    pts = [o.matrix_world @ Vector(c) for o in objs if o.type == 'MESH' for c in o.bound_box]
    w = max(p.dot(r) for p in pts) - min(p.dot(r) for p in pts)
    h = max(p.dot(u) for p in pts) - min(p.dot(u) for p in pts)
    cam.data.ortho_scale = max(w, h * res[0] / res[1]) * margin
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, direction, up, 60.0)


def persp(cam, objs, pitch_deg, yaw_deg, res, fov=50.0, margin=0.9):
    lo, hi = bounds(objs)
    lo.z = max(lo.z, 0)
    center = (lo + hi) / 2
    radius = (hi - lo).length / 2
    cam.data.type = 'PERSP'
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(fov)
    f = min(cam.data.angle, cam.data.angle * res[1] / res[0])
    dist = radius * margin / math.sin(f / 2)
    p, y = math.radians(pitch_deg), math.radians(yaw_deg)
    d = Vector((math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p)))     # de onde a câmera olha
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = res
    look(cam, center, d, (0, 0, 1), dist)


def render(name):
    bpy.context.scene.render.filepath = os.path.join(OUT, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('PREVIEW', name)


cam, sun, bg = scene_setup()
objs = []
for name, at in MODELS[MODEL]:
    objs += load(os.path.join(SRC, name + '.glb'), at)

# as vistas da ficha, em Blender: a frente do tanque é +X; a direita do tanque é −Y do Blender
# luz da ficha: de cima, da frente e um pouco da esquerda
sun.rotation_euler = (math.radians(35), math.radians(20), math.radians(-150))
views = {
    'front': ((-1, 0, 0), (0, 0, 1), (900, 900)),            # olhando a frente
    'back': ((1, 0, 0), (0, 0, 1), (900, 900)),
    'left': ((0, -1, 0), (0, 0, 1), (1800, 820)),             # o lado esquerdo do tanque (Blender +Y), frente pra esquerda da imagem
    'top': ((0, 0, -1), (0, -1, 0), (1800, 1000)),            # de cima, frente pra esquerda, lado direito do tanque pra cima
}
for tag, (direction, up, res) in views.items():
    ortho(cam, objs, direction, up, res)
    render(tag)
# a 3/4 da ficha (de cima, pela frente-esquerda) em perspectiva, e a de frente-direita pra conferir o outro lado
persp(cam, objs, 32.0, 215.0, (1600, 900), fov=35.0, margin=0.95)
render('three_quarter')
persp(cam, objs, 32.0, 145.0, (1600, 900), fov=35.0, margin=0.95)
render('three_quarter_right')

# a câmera do jogo no chão de areia, com o tanque USA ao lado (3 m à direita)
bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, 0))
ground = bpy.context.active_object
gm = bpy.data.materials.new('ground')
gm.use_nodes = True
gb = gm.node_tree.nodes['Principled BSDF']
gb.inputs['Base Color'].default_value = (0.42, 0.33, 0.21, 1)
gb.inputs['Roughness'].default_value = 0.95
ground.data.materials.append(gm)
bg.inputs['Color'].default_value = (0.36, 0.48, 0.66, 1)
sun.data.energy = 4.5
sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
if TANK:
    for n in USA_PARTS:
        objs += load(os.path.join(TANK, n + '.glb'), (0, -6.0, 0))       # (Blender −Y = a direita do tanque)
# o jogo olha pra +X: câmera atrás (−X), 37,5°, um pouco pela direita, como o jogador vê a tropa andando
persp(cam, objs, 37.5, 35.0, (1600, 900), fov=50.0, margin=0.8)
render('game_cam')
persp(cam, objs, 37.5, 35.0, (1600, 900), fov=28.0, margin=0.75)
render('game_cam_close')
print('PREVIEW fim')
