"""Fotos de uma peça GLB nos mesmos ângulos das vistas do Gemini, pra comparar.

Uso: blender -b --factory-startup -P blender_part_views.py -- <peca.glb> <saida_dir> <frente>
<frente>: eixo pra onde a frente da peça aponta no Blender (+X, -X, +Y, -Y).
Grava front/left/back/right (câmera na altura da peça, ortográfica), top (de
cima, frente pra baixo) e three_quarter, com luz de estúdio clara (Eevee).
"left" = lado esquerdo da peça (a frente aparece apontando pra esquerda).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

glb, out_dir, front = sys.argv[sys.argv.index('--') + 1:][:3]
os.makedirs(out_dir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=glb)
meshes = [o for o in sc.objects if o.type == 'MESH']

# gira a peça pra frente ficar em +X (convenção das outras ferramentas)
yaw = {'+X': 0, '-X': math.pi, '+Y': -math.pi / 2, '-Y': math.pi / 2}[front]
root = bpy.data.objects.new('root', None)
sc.collection.objects.link(root)
for o in sc.objects:
    if o.parent is None and o is not root:
        o.parent = root
root.rotation_euler = (0, 0, yaw)
bpy.context.view_layer.update()
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
c = (mn + mx) / 2
size = max(mx - mn) * 1.12

sc.render.engine = 'BLENDER_EEVEE'
sc.render.resolution_x = sc.render.resolution_y = 1024
sc.render.film_transparent = False
w = bpy.data.worlds.new('w')
w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (1, 1, 1, 1)
w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
sc.world = w
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
sun.data.energy = 2.5
sun.rotation_euler = (math.radians(40), 0, math.radians(30))
sc.collection.objects.link(sun)
sc.view_settings.view_transform = 'Standard'

cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
d = 20


def shoot(name, loc, rot, ortho=True):
    cam.data.type = 'ORTHO' if ortho else 'PERSP'
    cam.data.ortho_scale = size
    cam.data.lens = 50
    cam.location = loc
    cam.rotation_euler = rot
    sc.render.filepath = os.path.join(out_dir, name)
    bpy.ops.render.render(write_still=True)


h = math.pi / 2
shoot('front.png', c + Vector((d, 0, 0)), (h, 0, h))           # olhando pra -X
shoot('back.png', c + Vector((-d, 0, 0)), (h, 0, -h))
shoot('left.png', c + Vector((0, d, 0)), (h, 0, math.pi))      # lado esquerdo (+Y): frente aponta pra esquerda
shoot('right.png', c + Vector((0, -d, 0)), (h, 0, 0))
shoot('top.png', c + Vector((0, 0, d)), (0, 0, h))             # de cima, frente pra baixo
p = c + Vector((size * 1.6, size * 1.3, size * 1.2))
rot = (c - p).to_track_quat('-Z', 'Y').to_euler()
shoot('three_quarter.png', p, rot, ortho=False)
print(f'PARTVIEWS size=({mx.x - mn.x:.3f},{mx.y - mn.y:.3f},{mx.z - mn.z:.3f})')
