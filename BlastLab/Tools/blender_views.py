"""Fotos ortográficas de peças GLB pelo Blender (sem interface).

Uso: blender -b --factory-startup -P blender_views.py -- <saida_dir> <a.glb> [b.glb ...]
Pra cada peça: top (de cima; direita = +X, cima = +Y), side (do lado -Y;
direita = +X, cima = +Z) e end (da ponta +X; direita = +Y, cima = +Z).
Serve pra descobrir pra onde cada peça gerada por IA está virada.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
out_dir, files = args[0], args[1:]
os.makedirs(out_dir, exist_ok=True)


def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'TEXTURE'
    sc.render.resolution_x = 900
    sc.render.resolution_y = 900
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new('w')
    sc.world.color = (0.35, 0.35, 0.37)
    return sc


def bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return mn, mx


def shoot(sc, cam, name, loc, rot, scale):
    cam.location = loc
    cam.rotation_euler = rot
    cam.data.ortho_scale = scale
    sc.render.filepath = os.path.join(out_dir, name)
    bpy.ops.render.render(write_still=True)


for f in files:
    sc = setup_scene()
    bpy.ops.import_scene.gltf(filepath=f)
    meshes = [o for o in sc.objects if o.type == 'MESH']
    mn, mx = bounds(meshes)
    c = (mn + mx) / 2
    size = max(mx - mn) * 1.15
    cam_data = bpy.data.cameras.new('cam')
    cam_data.type = 'ORTHO'
    cam = bpy.data.objects.new('cam', cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    base = os.path.splitext(os.path.basename(f))[0]
    d = 10.0
    shoot(sc, cam, f'{base}_top.png', c + Vector((0, 0, d)), (0, 0, 0), size)
    shoot(sc, cam, f'{base}_side.png', c + Vector((0, -d, 0)), (math.pi / 2, 0, 0), size)
    shoot(sc, cam, f'{base}_end.png', c + Vector((d, 0, 0)), (math.pi / 2, 0, math.pi / 2), size)
    print(f'VIEWS {base}: min={tuple(round(v, 3) for v in mn)} max={tuple(round(v, 3) for v in mx)}')
