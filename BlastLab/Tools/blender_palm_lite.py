"""Palmeira da PlantCatalog (CC BY 4.0) leve pro jogo: uma malha, dois materiais, poucos triângulos.

Uso:
    blender -b --factory-startup -P blender_palm_lite.py -- <entrada.glb> <saida_dir> <nome> <tri_alvo> [render]

O modelo original tem 7–23 malhas, 7–15 materiais e 90 mil a 940 mil
triângulos, com folíolos em geometria (opacos, a borda em BLEND é só
suavização) e texturas pequenas (64–512 px). Aqui:
1. junta tudo numa malha, solda os vértices separados nas costuras de UV e
   tira os soltos (no tanque isso era metade do peso);
2. a folhagem (folíolos, talos, coroa, cortes, talos secos) perde a textura:
   a cor de cada canto vira cor de vértice amostrada da própria textura
   (as texturas dela têm 64–256 px, quase um degradê). Fica UM material de
   folhagem, opaco, sem máscara (o Nanite e o recorte do WorldForge pedem
   folha opaca);
3. o tronco mantém a textura dele (512 px: o desenho das bases das folhas
   velhas é o que se lê de perto) num segundo material;
4. reduz: o tronco pra ~6% do alvo, a folhagem pro resto;
5. origem no pé do tronco (z = 0 no chão), Z pra cima, metros.
Grava <nome>.glb e, com "render", fotos na câmera do jogo e na deitada.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, NAME, TARGET = os.path.abspath(argv[0]), os.path.abspath(argv[1]), argv[2], int(argv[3])
RENDER = len(argv) > 4 and argv[4] == 'render'
# como reduzir a folhagem: 'collapse' (padrão) ou 'planar:<graus>' — o folíolo é uma fita fina e
# quase plana; o colapso mata a largura dele (vira espinho), a dissolução planar mantém o contorno
FOLIAGE_MODE = argv[5] if len(argv) > 5 else 'collapse'
# fração dos folíolos (ilhas pequenas da folhagem) a tirar: rareia a copa sem afinar cada folíolo
THIN = float(argv[6]) if len(argv) > 6 else 0.0
TRUNK_MIN = 1500
os.makedirs(OUT, exist_ok=True)
TRUNK_SHARE = 0.06

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
src_tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)


def base_image(mat):
    """a textura de cor do material (a que liga no Base Color) e o fator de cor."""
    bsdf = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    factor = (1.0, 1.0, 1.0)
    if bsdf is None:
        return None, factor
    inp = bsdf.inputs['Base Color']
    if not inp.links:
        c = inp.default_value
        return None, (c[0], c[1], c[2])
    node = inp.links[0].from_node
    while node and node.type != 'TEX_IMAGE':          # passa por mix/multiply do glTF
        ins = [i for i in node.inputs if i.links]
        node = ins[0].links[0].from_node if ins else None
    return (node.image if node else None), factor


def is_trunk(mat):
    return 'trunk' in mat.name.lower()


# 2. cor de vértice amostrada da textura (só a folhagem)
cache = {}
for o in meshes:
    me = o.data
    attr = me.color_attributes.new('Col', 'BYTE_COLOR', 'CORNER')
    uv = me.uv_layers.active.data if me.uv_layers.active else None
    cols = np.ones((len(me.loops), 4), dtype=np.float32)
    for poly in me.polygons:
        mat = me.materials[poly.material_index] if me.materials else None
        if mat is None or is_trunk(mat):
            continue
        img, factor = base_image(mat)
        if img is None:
            cols[poly.loop_start:poly.loop_start + poly.loop_total, :3] = factor
            continue
        if img.name not in cache:
            w, h = img.size
            px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
            cache[img.name] = (px, w, h)
        px, w, h = cache[img.name]
        for li in range(poly.loop_start, poly.loop_start + poly.loop_total):
            u, v = (uv[li].uv if uv else (0.5, 0.5))
            x = int((u % 1.0) * (w - 1))
            y = int((v % 1.0) * (h - 1))
            cols[li, :3] = px[y, x, :3]
    attr.data.foreach_set('color', cols.ravel())

# junta numa malha só
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
ob.name = NAME
me = ob.data

# 3. dois materiais: tronco (textura) e folhagem (cor de vértice, opaca)
trunk_src = next((m for m in me.materials if is_trunk(m)), None)
foliage = bpy.data.materials.new('M_PalmFoliage')
foliage.use_nodes = True
nt = foliage.node_tree
nt.nodes.clear()
bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
outn = nt.nodes.new('ShaderNodeOutputMaterial')
vc = nt.nodes.new('ShaderNodeVertexColor')
vc.layer_name = 'Col'
nt.links.new(vc.outputs['Color'], bs.inputs['Base Color'])
nt.links.new(bs.outputs[0], outn.inputs['Surface'])
bs.inputs['Roughness'].default_value = 0.65
foliage.blend_method = 'OPAQUE' if hasattr(foliage, 'blend_method') else None
trunk = bpy.data.materials.new('M_PalmTrunk')
trunk.use_nodes = True
tn = trunk.node_tree
tn.nodes.clear()
tb = tn.nodes.new('ShaderNodeBsdfPrincipled')
to = tn.nodes.new('ShaderNodeOutputMaterial')
tn.links.new(tb.outputs[0], to.inputs['Surface'])
tb.inputs['Roughness'].default_value = 0.85
timg, _ = base_image(trunk_src) if trunk_src else (None, None)
if timg:
    ti = tn.nodes.new('ShaderNodeTexImage')
    ti.image = timg
    tn.links.new(ti.outputs['Color'], tb.inputs['Base Color'])
old = list(me.materials)
trunk_idx = {i for i, m in enumerate(old) if m and is_trunk(m)}
mat_index = np.zeros(len(me.polygons), dtype=np.int32)
me.polygons.foreach_get('material_index', mat_index)
new_index = np.where(np.isin(mat_index, list(trunk_idx)), 1, 0).astype(np.int32)
me.materials.clear()
me.materials.append(foliage)
me.materials.append(trunk)
me.polygons.foreach_set('material_index', new_index)

# 1. solda as costuras e tira os soltos
bm = bmesh.new()
bm.from_mesh(me)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(me)
bm.free()


def tris(m):
    return sum(len(p.vertices) - 2 for p in m.polygons)


# 4. reduz o tronco e a folhagem separados (o tronco é um cilindro: precisa de pouco)
def select_group(index):
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode='OBJECT')
    for p in me.polygons:
        p.select = p.material_index == index
    return sum(len(p.vertices) - 2 for p in me.polygons if p.material_index == index)


def reduce_group(index, target):
    n = select_group(index)
    if n <= target or n == 0:
        return
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.decimate(ratio=target / n)
    bpy.ops.object.mode_set(mode='OBJECT')


def planar_group(index, degrees):
    select_group(index)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.dissolve_limited(angle_limit=math.radians(degrees), delimit={'MATERIAL'})
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.ops.object.mode_set(mode='OBJECT')


def thin_leaflets(fraction):
    """tira 1 a cada N folíolos (ilhas pequenas da folhagem), de forma regular pra
    não abrir buraco num lado só; os talos e a coroa (ilhas grandes) ficam."""
    if fraction <= 0:
        return 0
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    seen, islands = set(), []
    for f in bm.faces:
        if f.index in seen or f.material_index != 0:
            continue
        stack, isl = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen and h.material_index == 0:
                        seen.add(h.index)
                        stack.append(h)
        islands.append(isl)
    areas = sorted(sum(f.calc_area() for f in isl) for isl in islands)
    small = areas[int(len(areas) * 0.9)] if areas else 0          # folíolos: as 90% menores
    leaf = [isl for isl in islands if sum(f.calc_area() for f in isl) <= small]
    drop = [isl for i, isl in enumerate(leaf) if (i * fraction) % 1.0 + fraction >= 1.0]
    bmesh.ops.delete(bm, geom=[f for isl in drop for f in isl], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(me)
    bm.free()
    print(f'PALM ilhas da folhagem {len(islands)}, folíolos {len(leaf)}, tirados {len(drop)}')
    return len(drop)


def rebuild_leaflets(segments):
    """Refaz cada folíolo como uma fita de `segments` trechos (2·segments triângulos),
    seguindo a curva e a largura medidas no original, com a cor média de cada trecho.
    O colapso comum mata a LARGURA do folíolo (vira espinho) e a dissolução planar funde
    os folíolos numa lâmina (vira agave); refazer mantém o desenho de pena da tamareira."""
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    col = bm.loops.layers.color.get('Col')
    seen, islands = set(), []
    for f in bm.faces:
        if f.index in seen or f.material_index != 0:
            continue
        stack, isl = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen and h.material_index == 0:
                        seen.add(h.index)
                        stack.append(h)
        islands.append(isl)
    areas = [sum(f.calc_area() for f in isl) for isl in islands]
    cut = sorted(areas)[int(len(areas) * 0.9)] if areas else 0
    new_faces, old = 0, []
    for isl, area in zip(islands, areas):
        if area > cut or len(isl) <= 2 * segments:
            continue                                   # talos, coroa e folíolos já mínimos ficam
        verts = list({v for f in isl for v in f.verts})
        P = np.array([v.co[:] for v in verts])
        C = np.zeros((len(verts), 3))
        cnt = np.zeros(len(verts))
        idx = {v: i for i, v in enumerate(verts)}
        if col:
            for f in isl:
                for l in f.loops:
                    C[idx[l.vert]] += l[col][:3]
                    cnt[idx[l.vert]] += 1
            C /= np.maximum(cnt, 1)[:, None]
        c = P.mean(0)
        _, _, vt = np.linalg.svd(P - c, full_matrices=False)
        e1, e2 = vt[0], vt[1]
        t = (P - c) @ e1
        w = (P - c) @ e2
        t0, t1 = t.min(), t.max()
        ring = []
        for k in range(segments + 1):
            tk = t0 + (t1 - t0) * k / segments
            near = np.abs(t - tk) <= (t1 - t0) / (2 * segments) + 1e-6
            if not near.any():
                near = np.argsort(np.abs(t - tk))[:3]
            cen = P[near].mean(0)
            half = max((w[near].max() - w[near].min()) / 2, 0.003)
            ring.append((bm.verts.new(cen - e2 * half), bm.verts.new(cen + e2 * half), C[near].mean(0)))
        for k in range(segments):
            a0, b0, c0 = ring[k]
            a1, b1, c1 = ring[k + 1]
            for tri, cs in (((a0, b0, b1), (c0, c0, c1)), ((a0, b1, a1), (c0, c1, c1))):
                face = bm.faces.new(tri)
                face.material_index = 0
                face.smooth = True
                if col:
                    for l, cc in zip(face.loops, cs):
                        l[col] = (cc[0], cc[1], cc[2], 1.0)
                new_faces += 1
        old.extend(isl)
    bmesh.ops.delete(bm, geom=old, context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(me)
    bm.free()
    print(f'PALM folíolos refeitos: {len(old)} faces → {new_faces}')


thin_leaflets(THIN)
if FOLIAGE_MODE.startswith('strip'):
    rebuild_leaflets(int(FOLIAGE_MODE.split(':')[1]))
reduce_group(1, max(TRUNK_MIN, int(TARGET * TRUNK_SHARE)))
if FOLIAGE_MODE.startswith('strip'):
    reduce_group(0, int(TARGET * (1 - TRUNK_SHARE)))      # só o que sobrou (talos, coroa) se passar do alvo
elif FOLIAGE_MODE.startswith('planar'):
    planar_group(0, float(FOLIAGE_MODE.split(':')[1]))
    reduce_group(0, int(TARGET * (1 - TRUNK_SHARE)))      # se ainda passar do alvo
else:
    reduce_group(0, int(TARGET * (1 - TRUNK_SHARE)))
bm = bmesh.new()
bm.from_mesh(me)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(me)
bm.free()

# 5. origem no pé do tronco
vs = np.array([v.co[:] for v in me.vertices])
trunk_v = vs[np.isin(np.arange(len(vs)), [v for p in me.polygons if p.material_index == 1 for v in p.vertices])] \
    if any(p.material_index == 1 for p in me.polygons) else vs
low = trunk_v[trunk_v[:, 2] < trunk_v[:, 2].min() + 0.5]
base = Vector((low[:, 0].mean(), low[:, 1].mean(), 0.0))
me.transform(__import__('mathutils').Matrix.Translation(-base))
for p in me.polygons:
    p.use_smooth = True
final = tris(me)
height = float(vs[:, 2].max())
path = os.path.join(OUT, NAME + '.glb')
bpy.ops.object.select_all(action='DESELECT')
ob.select_set(True)
bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True)
print(f'PALM {NAME}: {src_tris} → {final} triângulos, {len(me.vertices)} vértices, altura {height:.1f} m, '
      f'{os.path.getsize(path) / 1e6:.2f} MB')

if RENDER:
    sc = bpy.context.scene
    bpy.ops.mesh.primitive_plane_add(size=200)
    g = bpy.context.object
    gm = bpy.data.materials.new('sand')
    gm.use_nodes = True
    next(n for n in gm.node_tree.nodes if n.type == 'BSDF_PRINCIPLED').inputs['Base Color'].default_value = (0.62, 0.47, 0.32, 1)
    g.data.materials.append(gm)
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(55), 0, math.radians(-125))
    sc.collection.objects.link(sun)
    w = bpy.data.worlds.new('w')
    w.use_nodes = True
    bgn = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
    bgn.inputs[0].default_value = (0.55, 0.62, 0.72, 1)
    bgn.inputs[1].default_value = 0.7
    sc.world = w
    sc.render.engine = 'BLENDER_EEVEE'
    sc.view_settings.view_transform = 'AgX'
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = 800, 600
    look = Vector((0, 0, height * 0.55))
    for tag, loc, lens in (('game', look + Vector((30 * math.cos(math.radians(55)), 0, 30 * math.sin(math.radians(55)))), 35),
                           ('tilt', look + Vector((height * 1.4, -height * 0.4, -height * 0.25)), 40)):
        cam.location = loc
        cam.data.lens = lens
        cam.rotation_euler = (look - loc).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(OUT, f'{NAME}_{tag}.png')
        bpy.ops.render.render(write_still=True)
