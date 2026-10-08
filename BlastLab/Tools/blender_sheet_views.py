"""Fotografa o tanque montado nas vistas da ficha (cima, baixo, frente, trás,
lados e 3/4), com luz de estúdio parecida com a da ficha, pra comparar.

Uso: blender -b <tank.blend> -P blender_sheet_views.py -- <saida_dir>
Câmeras ortográficas (menos o 3/4), fundo transparente, enquadramento
justo no tanque. Eixos do tanque: X frente, Y esquerda, Z cima.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

OUT = sys.argv[sys.argv.index('--') + 1]
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
for o in sc.objects:
    if o.type == 'MESH' and o.name.startswith('Plane'):
        o.hide_render = True
for o in list(sc.objects):
    if o.type == 'LIGHT':
        bpy.data.objects.remove(o)
meshes = [o for o in sc.objects if o.type == 'MESH' and not o.hide_render]
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
c = (mn + mx) / 2

sc.render.engine = 'CYCLES'
sc.cycles.device = 'CPU'
sc.cycles.samples = 48
sc.cycles.use_denoising = True
sc.render.film_transparent = True
bg = sc.world.node_tree.nodes['Background']
bg.inputs[0].default_value = (0.55, 0.56, 0.58, 1)
bg.inputs[1].default_value = 1.0
sc.view_settings.view_transform = 'Standard'
cam = sc.camera
d = 40


def shoot(name, direction, up_roll, size, res, key_from):
    for o in list(sc.objects):
        if o.type == 'LIGHT':
            bpy.data.objects.remove(o)
    L = bpy.data.lights.new('key', 'AREA')
    L.energy = 2500
    L.size = 12
    k = bpy.data.objects.new('key', L)
    sc.collection.objects.link(k)
    k.location = c + Vector(key_from)
    k.rotation_euler = (c - k.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = size
    loc = c + Vector(direction) * d
    cam.location = loc
    q = (c - loc).to_track_quat('-Z', 'Y')
    cam.rotation_euler = q.to_euler()
    cam.rotation_euler.rotate_axis('Z', up_roll)
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


Lx, Ly, Lz = mx.x - mn.x, mx.y - mn.y, mx.z - mn.z
# cima: frente pra esquerda (como na ficha); câmera olhando pra baixo
shoot('top.png', (0, 0, 1), 0, Lx * 1.05, (1200, int(1200 * Ly / Lx * 1.05 / 1.05) + 40), (2, 6, 14))
# baixo: frente pra esquerda
shoot('bottom.png', (0, 0, -1), 0, Lx * 1.05, (1200, int(1200 * Ly / Lx) + 40), (2, 6, -14))
# lado esquerdo (+Y): frente pra esquerda; lado direito (-Y): frente pra direita
shoot('side_left.png', (0, 1, 0), 0, Lx * 1.05, (1200, int(1200 * Lz / Lx) + 60), (2, 14, 12))
shoot('side_right.png', (0, -1, 0), 0, Lx * 1.05, (1200, int(1200 * Lz / Lx) + 60), (2, -14, 12))
# frente e trás
shoot('front.png', (1, 0, 0), 0, max(Ly, Lz) * 1.08, (900, 900), (14, 4, 12))
shoot('back.png', (-1, 0, 0), 0, max(Ly, Lz) * 1.08, (900, 900), (-14, 4, 12))
# 3/4 da ficha: de cima, pela frente-esquerda
cam_dir = Vector((0.62, 0.55, 0.56)).normalized()
for o in list(sc.objects):
    if o.type == 'LIGHT':
        bpy.data.objects.remove(o)
shoot('three_quarter.png', tuple(cam_dir), 0, Lx * 0.95, (1200, 700), (6, 10, 12))
print(f'SHEETVIEWS size=({Lx:.2f},{Ly:.2f},{Lz:.2f})')
