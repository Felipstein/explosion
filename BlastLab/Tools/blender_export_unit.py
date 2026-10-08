"""Exporta as peças do tanque montado (tank_v2.blend) prontas pra Unreal.

Uso: blender -b <tank_v2.blend> -P blender_export_unit.py -- <saida_dir>

Cada peça sai num GLB próprio, já no lugar em relação à origem do tanque
(chão, centro do casco; X frente, Z cima, metros), então na Unreal basta pôr
todas no mesmo ponto. Antes de exportar:
- reduz os triângulos pra meta da peça (o tanque tem ~260 px na câmera mais
  próxima do jogo: 1 milhão de triângulos não aparece, só pesa na memória);
- texturas no máximo 2048 px;
- tinta sem metal (o gerador marca a pintura como metal; tinta é dielétrica).
Rodas: as cópias viram uma malha só por lado (pra animar depois, separar).
"""
import os
import sys

import bpy

OUT = sys.argv[sys.argv.index('--') + 1]
os.makedirs(OUT, exist_ok=True)
# 07/10/2026: 'leve' do blender_lod_test.py (~49 mil triângulos no tanque, contra 415 mil): igual
# na câmera do jogo e na deitada; com 30 mil o casco começa a amolecer de perto
TARGET_TRIS = {'hull': 15_000, 'turret': 15_000, 'barrel': 1_500, 'wheel': 600}
MAX_TEX = 2048

sc = bpy.context.scene
for img in bpy.data.images:
    if img.size[0] > MAX_TEX:
        img.scale(MAX_TEX, MAX_TEX)


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def clean(ob):
    """o glTF chega com os vértices separados nas costuras de UV e pedaços soltos:
    junta por distância (a UV é por canto de face, não se perde) e tira vértice
    sem face. Sem isso o tanque tinha 785 mil vértices pra 415 mil triângulos e a
    redução travava (blender_lod_test.py, 07/10/2026)."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(ob.data)
    bm.free()


def decimate(ob, target):
    clean(ob)
    n = tris(ob)
    if n <= target:
        return
    m = ob.modifiers.new('dec', 'DECIMATE')
    m.ratio = target / n
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=m.name)


def export(obs, name):
    for o in bpy.data.objects:
        o.select_set(o in obs)
    bpy.context.view_layer.objects.active = obs[0]
    path = os.path.join(OUT, name + '.glb')
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True,
                              export_apply=True, export_image_format='AUTO')
    print(f'EXPORT {name}: {sum(tris(o) for o in obs)} tris, {os.path.getsize(path) / 1e6:.1f} MB')


objs = {o.name: o for o in bpy.data.objects if o.type == 'MESH'}
for key in ('hull', 'turret', 'barrel'):
    ob = objs[key]
    decimate(ob, TARGET_TRIS[key])
    export([ob], 'SM_USA_MBT_' + key.capitalize())

export([o for n, o in objs.items() if n.startswith('track')], 'SM_USA_MBT_Tracks')

# rodas visíveis (o protótipo fica escondido)
wheels = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('wheel') and not o.hide_render]
if wheels:
    tmp = wheels[0]
    tmp.data = tmp.data.copy()          # a malha é dividida pelas 18 rodas (e o protótipo)
    decimate(tmp, TARGET_TRIS['wheel'])
    for w in wheels[1:]:
        w.data = tmp.data
    export(wheels, 'SM_USA_MBT_Wheels')
