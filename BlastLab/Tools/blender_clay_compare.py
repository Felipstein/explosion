"""Compara a GEOMETRIA de duas ou mais peças (sem textura, em "argila").

Uso: blender -b --factory-startup -P blender_clay_compare.py -- <saida_dir> <a.glb>:<frente> [<b.glb>:<frente> ...]
<frente>: eixo pra onde a frente aponta (+X, -X, +Y, -Y). Cada peça é girada
pra frente em +X e escalada pra comprimento 1, com o mesmo material cinza e a
mesma luz rasante (que realça quinas e superfícies tortas). Grava, por peça,
<nome>_q34.png (3/4 de perto), <nome>_side.png e <nome>_top.png.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index('--') + 1:]
out_dir, items = args[0], args[1:]
os.makedirs(out_dir, exist_ok=True)
for it in items:
    path, front = it.rsplit(':', 1)
    name = os.path.splitext(os.path.basename(path))[0]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    bpy.ops.import_scene.gltf(filepath=path)
    obs = [o for o in sc.objects if o.type == 'MESH']
    yaw = {'+X': 0, '-X': math.pi, '+Y': -math.pi / 2, '-Y': math.pi / 2}[front]
    clay = bpy.data.materials.new('clay')
    clay.use_nodes = True
    b = clay.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (0.6, 0.6, 0.6, 1)
    b.inputs['Roughness'].default_value = 0.6
    for o in obs:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = Matrix.Rotation(yaw, 4, 'Z') @ mw
        o.data.materials.clear()
        o.data.materials.append(clay)
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in obs for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    s = 1.0 / (mx.x - mn.x)
    c = (mn + mx) / 2
    for o in obs:
        o.matrix_world = Matrix.Scale(s, 4) @ Matrix.Translation(-c) @ o.matrix_world
    print(f'CLAY {name}: comprimento/largura={(mx.x - mn.x) / (mx.y - mn.y):.2f} '
          f'comprimento/altura={(mx.x - mn.x) / (mx.z - mn.z):.2f}')
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1200, 800
    w = bpy.data.worlds.new('w')
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.35, 0.36, 0.38, 1)
    w.node_tree.nodes['Background'].inputs[1].default_value = 0.5
    sc.world = w
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(62), 0, math.radians(-40))
    sc.collection.objects.link(sun)
    sc.view_settings.view_transform = 'Standard'
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    for tag, loc, ortho, lens in (('q34', (0.55, 0.45, 0.32), None, 50),
                                  ('side', (0, 5, 0), 1.1, None),
                                  ('top', (0, 0, 5), 1.1, None)):
        if ortho:
            cam.data.type = 'ORTHO'
            cam.data.ortho_scale = ortho
        else:
            cam.data.type = 'PERSP'
            cam.data.lens = lens
        cam.location = loc
        cam.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        if tag == 'top':
            cam.rotation_euler = (0, 0, 0)
        sc.render.filepath = os.path.join(out_dir, f'{name}_{tag}.png')
        bpy.ops.render.render(write_still=True)
