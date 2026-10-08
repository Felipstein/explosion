"""Teste de quanto dá pra reduzir os triângulos do tanque sem perder o visual.

Uso: blender -b <tank_v2.blend> -P blender_lod_test.py -- <saida_dir>

Pra cada nível (o atual e três reduções) reduz as peças com o modificador
Decimate (colapso, mantém as UVs e a textura) e fotografa nas duas câmeras que
importam (regra de detalhe do guia de unidades: o detalhe se mede pela câmera
deitada, a mais perto):
- a do jogo (55°, tanque com ~260 px, a 1024×576) e
- a deitada próxima (a de cinema).
Imprime os triângulos e vértices de cada nível, contando cada roda.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

OUT = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
os.makedirs(OUT, exist_ok=True)
BLEND = bpy.data.filepath
LEVELS = {
    # o que está no jogo hoje (os alvos do blender_export_unit.py)
    'a_atual': {'hull': 150000, 'turret': 150000, 'barrel': 20000, 'wheel': 6000},
    'b_medio': {'hull': 40000, 'turret': 40000, 'barrel': 4000, 'wheel': 1500},
    'c_leve': {'hull': 15000, 'turret': 15000, 'barrel': 1500, 'wheel': 600},
    'd_muito_leve': {'hull': 6000, 'turret': 6000, 'barrel': 800, 'wheel': 300},
}


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def clean(ob):
    """o glTF chega com os vértices separados nas costuras de UV (e o gerador deixa
    pedaços soltos): sem juntar, o colapso trava (a roda parava em 3,4 mil triângulos).
    Juntar por distância não perde a UV (ela é por canto de face)."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.to_mesh(ob.data)
    bm.free()


def decimate(ob, target):
    if ob.data.users > 1:
        ob.data = ob.data.copy()
    clean(ob)
    n = tris(ob)
    if n <= target:
        return
    m = ob.modifiers.new('dec', 'DECIMATE')
    m.ratio = target / n
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=m.name, single_user=True)


def shoot(path, loc, look, lens):
    sc = bpy.context.scene
    cam = sc.camera
    sc.render.resolution_x, sc.render.resolution_y = 1024, 576
    cam.data.type = 'PERSP'
    cam.data.lens = lens
    cam.location = loc
    cam.rotation_euler = (Vector(look) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)


for name, targets in LEVELS.items():
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    sc = bpy.context.scene
    for ob in sc.objects:
        if ob.type == 'MESH' and ob.name in ('ground', 'Ground', 'Plane'):
            ob.hide_render = False
    meshes = [o for o in sc.objects if o.type == 'MESH' and not o.hide_render]
    if targets:
        reduced = {}                   # malha original → reduzida (as rodas dividem a mesma malha)
        for ob in meshes:
            key = 'wheel' if ob.name.startswith('wheel') else ob.name if ob.name in targets else None
            if not key:
                continue
            src = ob.data.name
            if src in reduced:
                ob.data = reduced[src]
            else:
                decimate(ob, targets[key])
                reduced[src] = ob.data
    parts = [o for o in meshes if o.name.startswith(('hull', 'turret', 'barrel', 'track', 'wheel'))]
    t = sum(tris(o) for o in parts)
    v = sum(len(o.data.vertices) for o in parts)
    print(f'LOD {name}: {t} triângulos, {v} vértices ({len(parts)} peças)')
    for o in parts[:6]:
        print(f'  PART {o.name}: {tris(o)} tri, {len(o.data.vertices)} vert, malha {o.data.name}')
    d, el, az, look = 30.0, math.radians(55), math.radians(35), (0.3, 0, 1.0)
    loc = (look[0] + d * math.cos(el) * math.cos(az), look[1] + d * math.cos(el) * math.sin(az), look[2] + d * math.sin(el))
    shoot(os.path.join(OUT, f'{name}_game.png'), loc, look, 35)
    shoot(os.path.join(OUT, f'{name}_tilt.png'), (14, 9, 3.2), (0, 0, 1.2), 45)
