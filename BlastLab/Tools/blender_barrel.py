"""Cano do tanque USA por script (substitui o do AssetHub/Tripo, que não permite uso comercial).

Uso: blender -b --factory-startup -P blender_barrel.py -- <saida_dir>

Um torno (perfil girado) em metros reais, ao longo de +X, da base (x = 0,
escondida no mantelete) até a boca (x = LEN). O perfil segue a ficha aprovada
(docs/concept/units/usa_mbt_player_color.webp): colar grosso na base, luva
térmica em segmentos com juntas, evacuador de fumaça (o bojo do meio), tubo
fino e o lábio da boca; tubo de 0,19 m de diâmetro. A cor do jogador são os
dois anéis azuis (no evacuador e perto da boca), como na ficha e na "foto do
jogo". UV: U dá a volta, V corre ao longo (0 base → 1 boca); a textura é uma
faixa gerada aqui (tinta cinza gasta, anéis, fuligem na boca).
Grava barrel.glb (escala real; a montagem só posiciona) e barrel_side.png.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np

OUT = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
os.makedirs(OUT, exist_ok=True)
LEN = 4.75                      # base escondida (~0,8 m dentro do mantelete) até a boca
SEG = 32                        # lados do torno
# (fração do comprimento, raio em m)
PROFILE = [
    (0.000, 0.000), (0.000, 0.140), (0.095, 0.140), (0.135, 0.106),
    (0.250, 0.106), (0.253, 0.100), (0.262, 0.100), (0.265, 0.106),     # junta da luva
    (0.355, 0.106), (0.358, 0.100), (0.367, 0.100), (0.370, 0.106),     # junta
    (0.450, 0.106), (0.490, 0.126), (0.610, 0.126), (0.650, 0.097),     # evacuador
    (0.800, 0.097), (0.803, 0.093), (0.810, 0.093), (0.813, 0.097),     # junta
    (0.955, 0.097), (0.962, 0.103), (1.000, 0.103),                     # lábio da boca
    (1.000, 0.062), (0.985, 0.062),                                      # alma (o furo)
]
RINGS = [(0.520, 0.585), (0.870, 0.905)]        # cor do jogador (fração do comprimento)

# textura em faixa: V ao longo do cano
TW, TH = 64, 1024
rng = np.random.default_rng(5)
v = (np.arange(TH) + 0.5) / TH
paint = np.array([0.60, 0.61, 0.58])
blue = np.array([0.26, 0.34, 0.60])         # o azul apagado da ficha
col = np.tile(paint, (TH, TW, 1)).astype(np.float32)


def smooth(ny, nx):
    """ruído suave (bilinear) de ny × nx células esticado pra TH × TW"""
    c = rng.random((ny + 1, nx + 1))
    y = np.linspace(0, ny, TH)[:, None]
    x = np.linspace(0, nx, TW)[None, :]
    y0, x0 = np.minimum(y.astype(int), ny - 1), np.minimum(x.astype(int), nx - 1)
    fy, fx = y - y0, x - x0
    return (c[y0, x0] * (1 - fy) * (1 - fx) + c[y0 + 1, x0] * fy * (1 - fx) +
            c[y0, x0 + 1] * (1 - fy) * fx + c[y0 + 1, x0 + 1] * fy * fx)


# desgaste em riscos ao longo do cano (o ruído é fino na volta e longo no comprimento)
grain = 0.6 * smooth(48, 16) + 0.4 * smooth(160, 32)
col *= (0.86 + 0.2 * grain)[..., None]
for a, b in RINGS:
    m = (v >= a) & (v <= b)
    col[m] = blue * (0.9 + 0.15 * grain[m][..., None])
soot = np.clip((v - 0.93) / 0.07, 0, 1) ** 1.5                 # fuligem na boca
col *= (1 - 0.75 * soot)[:, None, None]
col[v > 0.985] = (0.04, 0.04, 0.04)                             # alma escura
img = bpy.data.images.new('T_Barrel', TW, TH)
rgba = np.concatenate([np.clip(col, 0, 1), np.ones((TH, TW, 1))], -1)
img.pixels = rgba.ravel().tolist()
img.filepath_raw = os.path.join(OUT, 'barrel_albedo.png')
img.file_format = 'PNG'
img.save()

bpy.ops.wm.read_factory_settings(use_empty=True)
img = bpy.data.images.load(os.path.join(OUT, 'barrel_albedo.png'))
bm = bmesh.new()
uvl = bm.loops.layers.uv.new('UVMap')
rings = []
for f, r in PROFILE:
    ring = []
    for k in range(SEG):
        a = 2 * math.pi * k / SEG
        ring.append(bm.verts.new((f * LEN, r * math.cos(a), r * math.sin(a))))
    rings.append(ring)
for i in range(len(rings) - 1):
    va, vb = PROFILE[i][0], PROFILE[i + 1][0]
    if PROFILE[i][1] == 0.0 and PROFILE[i + 1][1] == 0.0:
        continue
    for k in range(SEG):
        k2 = (k + 1) % SEG
        face = bm.faces.new((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]))
        u0, u1 = k / SEG, (k + 1) / SEG
        for loop, uv in zip(face.loops, ((u0, va), (u1, va), (u1, vb), (u0, vb))):
            loop[uvl].uv = uv
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new('barrel')
bm.to_mesh(me)
for p in me.polygons:
    p.use_smooth = True
ob = bpy.data.objects.new('barrel', me)
bpy.context.scene.collection.objects.link(ob)
mat = bpy.data.materials.new('M_Barrel')
mat.use_nodes = True
nt = mat.node_tree
nt.nodes.clear()
bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
out = nt.nodes.new('ShaderNodeOutputMaterial')
tex = nt.nodes.new('ShaderNodeTexImage')
tex.image = img
nt.links.new(tex.outputs['Color'], bs.inputs['Base Color'])
nt.links.new(bs.outputs[0], out.inputs['Surface'])
bs.inputs['Metallic'].default_value = 0.0
bs.inputs['Roughness'].default_value = 0.7
me.materials.append(mat)
tris = sum(len(p.vertices) - 2 for p in me.polygons)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'barrel.glb'), export_format='GLB')
print(f'BARREL {LEN} m, {tris} triângulos, {len(me.vertices)} vértices -> {OUT}')

# conferência de lado
sc = bpy.context.scene
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = LEN * 1.05
cam.location = (LEN / 2, -10, 0)
cam.rotation_euler = (math.pi / 2, 0, 0)
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
sun.rotation_euler = (math.radians(50), 0, math.radians(20))
sc.collection.objects.link(sun)
sc.render.engine = 'BLENDER_EEVEE'
sc.render.resolution_x, sc.render.resolution_y = 1200, 200
sc.render.filepath = os.path.join(OUT, 'barrel_side.png')
bpy.ops.render.render(write_still=True)
