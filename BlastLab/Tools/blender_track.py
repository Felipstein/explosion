"""Esteira do tanque por script: uma faixa contornando as rodas.

Uso: blender -b --factory-startup -P blender_track.py -- <saida_dir> [cor_do_elo.png] [normal_do_elo.png]

As posições e raios das rodas vêm da ficha aprovada (vista de lado de
docs/concept/units/usa_mbt_player_color.webp, casco de 382 px = 7,9 m;
eixos do tanque: X frente, Z cima, chão em z = 0). O caminho da faixa é o
contorno convexo das rodas (como uma correia esticada): linhas tangentes e
arcos. A faixa tem UV reto: V corre ao longo da esteira (1 unidade = 1 elo),
U atravessa a largura, então a textura do elo se repete e pode correr no
material (anima a esteira andando).
Grava esteira.glb (uma lagarta, lado esquerdo; a direita é espelhada na
montagem) e uma foto de lado com as rodas provisórias pra conferir.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
OUT = args[0]
LINK_TEX = args[1] if len(args) > 1 else None
LINK_NRM = args[2] if len(args) > 2 else None   # normal do elo (espaço tangente, +Y do OpenGL)
os.makedirs(OUT, exist_ok=True)

M_PER_PX = 7.9 / 382.0


def px(xp, yp):
    """ponto da ficha (px) → metros no tanque (frente em +X)"""
    return 3.95 - (xp - 381) * M_PER_PX, (540 - yp) * M_PER_PX


ROAD_R = 0.31
ROAD_Z = 0.36
ROAD_X = [px(p, 0)[0] for p in (448.75, 488.75, 528.75, 567.5, 606.25, 645.0, 685.0)]
IDLER = (px(400, 507), 0.27)       # roda-guia (frente)
SPROCKET = (px(735, 506), 0.29)    # roda motriz (trás)
THICK = 0.06                       # espessura da esteira
WIDTH = 0.64                       # largura (Abrams: 635 mm)
PITCH = 0.19                       # passo do elo (Abrams: ~190 mm)
TRACK_Y = 1.45                     # centro da lagarta (lado esquerdo = +Y)

wheels = [((x, ROAD_Z), ROAD_R) for x in ROAD_X] + [IDLER, SPROCKET]

# contorno convexo dos círculos (raio + meia espessura = linha média da faixa)
pts = []
for (cx, cz), r in wheels:
    rr = r + THICK / 2
    for i in range(720):
        a = 2 * math.pi * i / 720
        pts.append((cx + rr * math.cos(a), cz + rr * math.sin(a)))
pts = sorted(set(pts))


def cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


lower, upper = [], []
for p in pts:
    while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
        lower.pop()
    lower.append(p)
for p in reversed(pts):
    while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
        upper.pop()
    upper.append(p)
hull = lower[:-1] + upper[:-1]          # anti-horário

# reamostra por comprimento: N elos inteiros
seg = [math.dist(hull[i], hull[(i + 1) % len(hull)]) for i in range(len(hull))]
perim = sum(seg)
n_links = round(perim / PITCH)
step = perim / (n_links * 4)            # 4 anéis por elo
ring, acc, i = [], 0.0, 0
target = 0.0
cum = [0.0]
for s in seg:
    cum.append(cum[-1] + s)
k = 0
while target < perim - 1e-9:
    while cum[k + 1] < target:
        k += 1
    t = (target - cum[k]) / seg[k] if seg[k] else 0
    a, b = hull[k], hull[(k + 1) % len(hull)]
    ring.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    target += step
print(f'TRACK perim={perim:.3f} m links={n_links} pitch={perim / n_links:.4f} rings={len(ring)}')

# malha: seção retangular varrida pelo caminho
bm = bmesh.new()
uvl = bm.loops.layers.uv.new('UVMap')
n = len(ring)
normals = []
for j in range(n):
    p0, p1 = ring[j - 1], ring[(j + 1) % n]
    tx, tz = p1[0] - p0[0], p1[1] - p0[1]
    ln = math.hypot(tx, tz)
    normals.append((tz / ln, -tx / ln))  # pra fora (caminho anti-horário)
ys = (TRACK_Y - WIDTH / 2, TRACK_Y + WIDTH / 2)
offs = (-THICK / 2, THICK / 2)          # dentro / fora
grid = {}
for j, (x, z) in enumerate(ring):
    nx, nz = normals[j]
    for oi, o in enumerate(offs):
        for yi, y in enumerate(ys):
            grid[j, oi, yi] = bm.verts.new((x + nx * o, y, z + nz * o))
bm.verts.ensure_lookup_table()


def quad(vs, uvs):
    f = bm.faces.new(vs)
    for loop, uv in zip(f.loops, uvs):
        loop[uvl].uv = uv


for j in range(n):
    j2 = (j + 1) % n
    v0, v1 = j / 4, (j + 1) / 4          # 1 unidade de V = 1 elo
    # face de fora (chão), de dentro (rodas), e as duas bordas
    quad([grid[j, 1, 0], grid[j2, 1, 0], grid[j2, 1, 1], grid[j, 1, 1]], [(0, v0), (0, v1), (1, v1), (1, v0)])
    quad([grid[j, 0, 1], grid[j2, 0, 1], grid[j2, 0, 0], grid[j, 0, 0]], [(1, v0), (1, v1), (0, v1), (0, v0)])
    quad([grid[j, 0, 0], grid[j2, 0, 0], grid[j2, 1, 0], grid[j, 1, 0]], [(0, v0), (0, v1), (0.05, v1), (0.05, v0)])
    quad([grid[j, 1, 1], grid[j2, 1, 1], grid[j2, 0, 1], grid[j, 0, 1]], [(0.95, v0), (0.95, v1), (1, v1), (1, v0)])
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
me = bpy.data.meshes.new('track_belt')
bm.to_mesh(me)
belt = bpy.data.objects.new('track_belt', me)
sc.collection.objects.link(belt)

mat = bpy.data.materials.new('M_TrackBelt')
mat.use_nodes = True
mat.node_tree.nodes.clear()
bsdf = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
mout = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
mat.node_tree.links.new(bsdf.outputs[0], mout.inputs['Surface'])
bsdf.inputs['Metallic'].default_value = 0.6
bsdf.inputs['Roughness'].default_value = 0.7
if LINK_TEX:
    tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(LINK_TEX)
    mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    if LINK_NRM:
        nt = mat.node_tree.nodes.new('ShaderNodeTexImage')
        nt.image = bpy.data.images.load(LINK_NRM)
        nt.image.colorspace_settings.name = 'Non-Color'
        nm = mat.node_tree.nodes.new('ShaderNodeNormalMap')
        mat.node_tree.links.new(nt.outputs['Color'], nm.inputs['Color'])
        mat.node_tree.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    bsdf.inputs['Metallic'].default_value = 0.0      # aço sujo de areia lê como dielétrico na câmera do jogo
    bsdf.inputs['Roughness'].default_value = 0.85
else:
    bsdf.inputs['Base Color'].default_value = (0.09, 0.09, 0.085, 1)
me.materials.append(mat)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'track_belt.glb'), export_format='GLB')

# rodas provisórias (cilindros) só pra conferir o desenho
for (cx, cz), r in wheels:
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=WIDTH * 0.9, location=(cx, TRACK_Y, cz),
                                        rotation=(math.pi / 2, 0, 0), vertices=48)
    bpy.context.object.color = (0.75, 0.75, 0.7, 1)

sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.color_type = 'OBJECT'
sc.render.resolution_x, sc.render.resolution_y = 1640, 320
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = 'ORTHO'
# enquadra igual ao recorte da ficha (px 370..780, 480..560)
x_l, z_t = px(370, 480)
x_r, z_b = px(780, 560)
cam.data.ortho_scale = abs(x_l - x_r)
cam.location = ((x_l + x_r) / 2, 20, (z_t + z_b) / 2)
cam.rotation_euler = (math.pi / 2, 0, math.pi)
sc.render.filepath = os.path.join(OUT, 'side_check.png')
bpy.ops.render.render(write_still=True)
