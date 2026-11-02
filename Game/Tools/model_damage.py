"""O destrutor: os estados de dano de um prédio (31/10/2026; o estudo e o plano: docs/ESTUDO_DEGRADACAO_PREDIOS.md §2a).

Roda sobre o GLB pronto do prédio (o provisório por script ou o modelo final feito à mão), sem código por prédio:

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_damage.py -- \\
      captures/game/models/china_war_factory SM_China_WarFactory china [semente]

Saem, ao lado do SM_X_Base.glb (o import_provisional.py já pega SM_X_*.glb da pasta):
- SM_X_Damaged.glb (abaixo de 70% da vida): 3–4 mordidas na platibanda, nas quinas e na borda do telhado. O corte é um booleano com um
  cortador de ruído (nunca um círculo) e a face do corte é o miolo da parede (o slot Broken: o concreto claro e áspero; no adobe, o
  barro); na borda do concreto sai a ferragem torta (4–6 cm, de propósito: a de verdade some abaixo de 1 px), no adobe os caibros;
- SM_X_Wrecked.glb (abaixo de 35%): mais 2 mordidas grandes, UMA QUINA INTEIRA caída (as duas paredes e o canto do telhado, numa célula
  só: nunca por parede), a laje do telhado pendurada pela borda que segura, o interior que aparece (os pilares e a laje do andar) e o
  talude de entulho no pé da quina, na cor do prédio;
- SM_X_Ruin.glb (o prédio morto): a planta inteira em muretas de 0,6–1,5 m de topo quebrado, 1–3 pilares em pé até ~4 m (os ossos:
  vazados, não tapam unidade) e o monte de entulho dentro da pegada + 3 m (a cobertura que a simulação dá);
- SM_X_damage.json: os pontos de fogo de cada estado (as mordidas, em metros da Unreal) e o que foi feito.
Cada estado leva a grade, o UV e as máscaras do model_detail.py (a quina, o contato, a fresta), como o prédio inteiro.

A facção escolhe o que quebra: china/usa = concreto com ferragem; gla = adobe com caibros de madeira e torrões redondos.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from model_detail import CLASS, bake_masks, grid, uv_meters  # noqa: E402
import model_lib  # noqa: E402

args = sys.argv[sys.argv.index('--') + 1:]
DIR, PREFIX, FACTION = os.path.abspath(args[0]), args[1], args[2].lower()
SEED = int(args[3]) if len(args) > 3 else 7
ADOBE = FACTION == 'gla'


def mat(name, rgb, rough=0.9):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get('Principled BSDF')
    b.inputs['Base Color'].default_value = (*rgb, 1.0)
    b.inputs['Roughness'].default_value = rough
    return m


BROKEN = mat('Broken', (0.40, 0.30, 0.18) if ADOBE else (0.30, 0.29, 0.26), 0.95)
REBAR = mat('CivWood' if ADOBE else 'CivRust', (0.11, 0.05, 0.02) if ADOBE else (0.07, 0.03, 0.013), 0.85)


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def load_base():
    path = os.path.join(DIR, PREFIX + '_Base.glb')
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    model_lib.select_only(objs[0])
    for o in objs:
        o.select_set(True)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = o.data
    # as máscaras e o UV antigos saem (o corte muda a malha: tudo é refeito no fim)
    for ca in list(me.color_attributes):
        me.color_attributes.remove(ca)
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    o.name = PREFIX + '_Base_src'
    return o


def bounds(o):
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), max(0.0, min(v.z for v in vs))))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def roof_z(o, x, y, lo, hi):
    """A altura do telhado em (x, y) (o raio de cima; o chão se nada)."""
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromObject(o, bpy.context.evaluated_depsgraph_get())
    hit = bvh.ray_cast(Vector((x, y, hi.z + 5.0)), Vector((0, 0, -1)), hi.z + 10.0)
    return hit[0].z if hit[0] is not None else 0.0


def cutter(name, c, r, rough=0.38, seed=0.0, stretch=(1.0, 1.0, 1.0)):
    """O cortador: a esfera de ruído (a borda do furo rasgada, não um círculo), com a face no slot Broken (o miolo que o corte mostra)."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
    for v in bm.verts:
        d = v.co.normalized()
        n = noise.noise(d * 1.7 + Vector((seed, seed * 1.3, seed * 0.7)))
        n2 = noise.noise(d * 4.1 + Vector((seed * 2.1, 3.3, seed)))
        k = 1.0 + rough * n + 0.12 * n2
        v.co = Vector((d.x * stretch[0], d.y * stretch[1], d.z * stretch[2])) * r * k + c
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(BROKEN)
    return o


def box_cutter(name, lo, hi, rough=0.25, seed=0.0, cells=6):
    """A caixa de ruído (a quina que cai): os lados quebrados em degraus irregulares."""
    bm = bmesh.new()
    r = bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.subdivide_edges(bm, edges=list(bm.edges), cuts=cells, use_grid_fill=True)
    size = hi - lo
    for v in bm.verts:
        p = Vector((v.co.x + 0.5, v.co.y + 0.5, v.co.z + 0.5))
        q = Vector((lo.x + p.x * size.x, lo.y + p.y * size.y, lo.z + p.z * size.z))
        q += Vector((noise.noise(q * 0.45 + Vector((seed, 0, 0))), noise.noise(q * 0.45 + Vector((0, seed, 0))),
                     noise.noise(q * 0.45 + Vector((0, 0, seed))))) * rough * min(size.x, size.y)
        v.co = q
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(BROKEN)
    return o


def boolean(o, cut, op='DIFFERENCE'):
    model_lib.select_only(o)
    m = o.modifiers.new('cut', 'BOOLEAN')
    m.operation = op
    m.object = cut
    m.solver = 'EXACT'
    m.use_self = False
    m.use_hole_tolerant = True
    try:
        m.material_mode = 'TRANSFER'   # a face do corte fica com o material do cortador (o miolo Broken)
    except Exception:
        pass
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(cut, do_unlink=True)


def copy_obj(o, name):
    c = o.copy()
    c.data = o.data.copy()
    c.name = name
    bpy.context.scene.collection.objects.link(c)
    return c


def rebar(p_list, normal_out, rnd, n=5):
    """A ferragem torta na borda do corte (concreto) ou os caibros (adobe), como peça junta ao estado."""
    bm = bmesh.new()
    for c in p_list:
        for _ in range(n):
            d = (normal_out + Vector((rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), rnd.uniform(-0.2, 0.5)))).normalized()
            L = rnd.uniform(0.3, 0.8) if not ADOBE else rnd.uniform(0.5, 1.2)
            rad = 0.025 if not ADOBE else 0.07
            p0 = c + Vector((rnd.uniform(-0.8, 0.8), rnd.uniform(-0.8, 0.8), rnd.uniform(-0.5, 0.3)))
            bend = Vector((rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3), -rnd.uniform(0.1, 0.4))) * (0.0 if ADOBE else 1.0)
            pts = [p0, p0 + d * L * 0.55, p0 + d * L + bend]
            ring_prev = None
            for k, p in enumerate(pts):
                t = (pts[min(k + 1, 2)] - pts[max(k - 1, 0)]).normalized()
                a = t.orthogonal().normalized()
                b = t.cross(a)
                ring = [bm.verts.new(p + (a * math.cos(j * math.pi / 3) + b * math.sin(j * math.pi / 3)) * rad) for j in range(6)]
                if ring_prev:
                    for j in range(6):
                        bm.faces.new([ring_prev[j], ring_prev[(j + 1) % 6], ring[(j + 1) % 6], ring[j]])
                ring_prev = ring
    me = bpy.data.meshes.new('rebar')
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new('rebar', me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(REBAR)
    return o


def chunks(center, spread, n, size, mats, rnd, zmax=None, name='chunks', lo=None, hi=None):
    """Os pedaços do entulho, QUEBRADOS (01/11: as bolas de icosfera liam como pedra de rio no jogo, nada a ver com o prédio). No
    concreto, a laje quebrada: o contorno de 5–7 lados com a espessura de parede, as faces de fora na cor do prédio (parede ou
    telhado, os mats menos o último) e a quebra no miolo (o último, o Broken); deitada, encostada ou de pé. No adobe, o torrão
    anguloso (o casco convexo de pontos), na cor da parede com as quebras claras. lo/hi seguram tudo dentro da pegada (+0,6 m)."""
    bm = bmesh.new()
    core = len(mats) - 1
    for k in range(n):
        s = size * (rnd.random() ** 1.6 * 0.85 + 0.15)
        c = center + Vector((rnd.gauss(0, spread.x), rnd.gauss(0, spread.y), 0.0))
        if lo is not None:
            c.x = min(max(c.x, lo.x - 0.6 + s * 0.5), hi.x + 0.6 - s * 0.5)
            c.y = min(max(c.y, lo.y - 0.6 + s * 0.5), hi.y + 0.6 - s * 0.5)
        surf = rnd.randrange(max(1, core))
        if ADOBE:
            pts = [bm.verts.new((rnd.uniform(-1, 1) * s, rnd.uniform(-0.8, 0.8) * s, rnd.uniform(0, 0.75) * s)) for _ in range(10)]
            r = bmesh.ops.convex_hull(bm, input=pts)
            junk = list({g for g in r['geom_interior'] + r['geom_unused'] if isinstance(g, bmesh.types.BMVert) and g.is_valid})
            if junk:
                bmesh.ops.delete(bm, geom=junk, context='VERTS')
            faces = [g for g in r['geom'] if isinstance(g, bmesh.types.BMFace) and g.is_valid]
            for f in faces:
                f.material_index = core if rnd.random() < 0.3 else surf
            verts = list({v for f in faces for v in f.verts})
            tilt = Matrix.Rotation(rnd.uniform(-0.3, 0.3), 4, 'X')
        else:
            m = rnd.randint(5, 7)
            angs = sorted(rnd.uniform(0, 2 * math.pi) for _ in range(m))
            out = [(math.cos(a) * s * rnd.uniform(0.55, 1.0) * 1.25, math.sin(a) * s * rnd.uniform(0.55, 1.0)) for a in angs]
            t = max(0.08, s * rnd.uniform(0.16, 0.28))
            bot = [bm.verts.new((x, y, 0.0)) for x, y in out]
            top = [bm.verts.new((x, y, t)) for x, y in out]
            faces = [bm.faces.new(top), bm.faces.new(list(reversed(bot)))]
            faces[0].material_index = faces[1].material_index = surf
            for i in range(m):
                j = (i + 1) % m
                f = bm.faces.new([bot[i], bot[j], top[j], top[i]])
                f.material_index = core
                faces.append(f)
            verts = bot + top
            # 70% deitada (inclinada no monte), 30% encostada ou de pé
            lean = rnd.uniform(0.0, 0.4) if rnd.random() < 0.7 else rnd.uniform(0.7, 1.35)
            tilt = Matrix.Rotation(lean, 4, 'X')
        M = Matrix.Translation(c) @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, 'Z') @ tilt
        bmesh.ops.transform(bm, matrix=M, verts=verts)
        dz = -min(v.co.z for v in verts) - rnd.uniform(0.02, 0.12)   # assentada: um pouco enterrada no chão ou no monte
        for v in verts:
            v.co.z += dz
            if zmax is not None:
                v.co.z = min(v.co.z, zmax)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    for m in mats:
        o.data.materials.append(m)
    for p in o.data.polygons:
        p.use_smooth = False
    return o


def mound(lo, hi, height, mats, rnd, name='mound'):
    """O monte de entulho da ruína: a colina baixa dentro da pegada, com o topo quebrado (ruído), na cor do prédio."""
    bm = bmesh.new()
    cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
    ax, ay = (hi.x - lo.x) * 0.36, (hi.y - lo.y) * 0.36
    n = 24
    verts = {}
    for i in range(n + 1):
        for j in range(n + 1):
            u, v = i / n * 2 - 1, j / n * 2 - 1
            r = math.sqrt(u * u + v * v)
            h = max(0.0, 1.0 - r) ** 0.8 * height
            p = Vector((cx + u * ax * 1.15, cy + v * ay * 1.15, 0.0))
            h *= 0.75 + 0.5 * (noise.noise(p * 0.35 + Vector((SEED, 0, 0))) * 0.5 + 0.5)
            p.z = h - 0.05
            verts[i, j] = bm.verts.new(p)
    for i in range(n):
        for j in range(n):
            bm.faces.new([verts[i, j], verts[i + 1, j], verts[i + 1, j + 1], verts[i, j + 1]])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    for m in mats:
        o.data.materials.append(m)
    for i, p in enumerate(o.data.polygons):
        p.material_index = 0 if noise.noise(p.center * 0.8) > -0.45 else min(1, len(mats) - 1)
    return o


def join(objs, name):
    model_lib.select_only(objs[0])
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name
    return o


def body_height(o, lo, hi):
    """A altura do corpo do prédio (o percentil 75 do telhado visto de cima numa grade de 1 m): o mastro, a bandeira e a antena
    (finos) quase não entram; com a caixa inteira a laje do andar boiava acima do telhado do quartel (a bandeira de 13 m)."""
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromObject(o, bpy.context.evaluated_depsgraph_get())
    zs = []
    x = lo.x + 0.5
    while x < hi.x - 0.5:
        y = lo.y + 0.5
        while y < hi.y - 0.5:
            h = bvh.ray_cast(Vector((x, y, hi.z + 5.0)), Vector((0, 0, -1)), hi.z + 10.0)
            if h[0] is not None and h[0].z > 0.5:
                zs.append(h[0].z)
            y += 1.0
        x += 1.0
    zs.sort()
    return zs[int(len(zs) * 0.75)] if zs else hi.z


def material_at(o, x, y, top):
    """O material do que se vê de cima em (x, y) (o telhado que a laje leva junto)."""
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromPolygons([v.co for v in o.data.vertices], [p.vertices for p in o.data.polygons], all_triangles=False)
    h = bvh.ray_cast(Vector((x, y, top + 5.0)), Vector((0, 0, -1)), top + 10.0)
    if h[2] is None:
        return None
    mi = o.data.polygons[h[2]].material_index
    return o.data.materials[mi] if mi < len(o.data.materials) else None


def roof_edges(o, lo, hi, H, step=1.0):
    """As células da borda da massa alta (o telhado do corpo, ≥ 70% da altura, com um vizinho bem mais baixo ou vazio): onde a
    mordida lê de cima. (Mordida na caixa do lote caía nos muros baixos do pátio e não se via.)"""
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromObject(o, bpy.context.evaluated_depsgraph_get())
    nx, ny = int((hi.x - lo.x) / step) + 1, int((hi.y - lo.y) / step) + 1
    z = {}
    for i in range(nx):
        for j in range(ny):
            h = bvh.ray_cast(Vector((lo.x + i * step, lo.y + j * step, hi.z + 5.0)), Vector((0, 0, -1)), hi.z + 10.0)
            z[i, j] = h[0].z if h[0] is not None else 0.0
    out = []
    for (i, j), zz in z.items():
        if zz < 0.7 * H or zz > 1.25 * H:
            continue
        if any(z.get((i + a, j + b), 0.0) < 0.6 * zz for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            out.append(Vector((lo.x + i * step, lo.y + j * step, zz)))
    return out


def dominant_material(o, up=None):
    """O material de mais área (up=True: o que olha pra cima, o telhado; False: o de pé, a parede)."""
    area = {}
    for p in o.data.polygons:
        if up is not None and (p.normal.z > 0.6) != up:
            continue
        if p.material_index < len(o.data.materials):
            m = o.data.materials[p.material_index]
            if m and m.name.split('.')[0] not in ('Glass', 'Light', 'Beacon', 'Fire', 'Core', 'Dark', 'Team', 'Player'):
                area[m] = area.get(m, 0.0) + p.area
    return max(area, key=area.get) if area else BROKEN


def U(p):
    """Do Blender pra Unreal (m): o Y troca de sinal."""
    return [round(p.x, 2), round(-p.y, 2), round(p.z, 2)]


def finish(o, name):
    o.name = name
    grid(o, CLASS['building']['cell'])
    uv_meters(o, SEED)
    bake_masks([o], 'building')
    model_lib.export(o, DIR)


def tall_cover(o, lo, hi, H):
    """(01/11, o Airfield e a Drop Zone B da USA destruídos: ~30 pilares nascendo no meio da pista e na areia, e a laje do 'telhado'
    pendurada por cima do pátio) onde há prédio de verdade: as células de 1 m cujo topo passa de 40% do corpo (e de 1,5 m). Devolve a
    fração do lote coberta, a caixa do coberto e o teste por ponto. No lote que é quase todo pátio (fração < 0,55), a quina, a laje
    pendurada, os pilares, os ossos e o monte vão só pro que é prédio (fração < 0,65); no prédio cheio nada muda."""
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromObject(o, bpy.context.evaluated_depsgraph_get())
    zmin = max(1.5, 0.4 * H)
    cov, n = set(), 0
    tops = {}
    x = lo.x + 0.5
    while x < hi.x:
        y = lo.y + 0.5
        while y < hi.y:
            n += 1
            h = bvh.ray_cast(Vector((x, y, hi.z + 5.0)), Vector((0, 0, -1)), hi.z + 10.0)
            if h[0] is not None:
                tops[(int(math.floor(x - lo.x)), int(math.floor(y - lo.y)))] = h[0].z
            if h[0] is not None and h[0].z > zmin:
                cov.add((int(math.floor(x - lo.x)), int(math.floor(y - lo.y))))
            y += 1.0
        x += 1.0
    frac = len(cov) / max(n, 1)

    def top(px, py):
        return tops.get((int(math.floor(px - lo.x)), int(math.floor(py - lo.y))), 0.0)
    if not cov:
        return 1.0, lo, hi, lambda px, py: True, top
    xs = [i for i, _ in cov]
    ys = [j for _, j in cov]
    tlo = Vector((lo.x + min(xs), lo.y + min(ys), lo.z))
    thi = Vector((lo.x + max(xs) + 1.0, lo.y + max(ys) + 1.0, hi.z))

    def covered(px, py):
        return (int(math.floor(px - lo.x)), int(math.floor(py - lo.y))) in cov
    return frac, tlo, thi, covered, top


def main():
    clear()
    rnd = random.Random(SEED * 7919 + len(PREFIX))
    base = load_base()
    lo, hi = bounds(base)
    H = body_height(base, lo, hi)
    # (01/11) o lote que é pátio (a pista, a laje do pouso): o dano grande só no que é prédio
    frac, tlo, thi, covered, top = tall_cover(base, lo, hi, H)
    # (01/11) 0,55 → 0,65: o Supply Center B (0,57 do lote coberto, o pátio de carga na frente) deixava 3 pilares soltos no pátio
    yard = frac < 0.65
    lot_lo, lot_hi = lo, hi
    if yard:
        lo, hi = tlo, thi
    W, D = hi.x - lo.x, hi.y - lo.y
    k = max(0.6, min(1.6, H / 9.0))
    dom = dominant_material(base, up=False)
    roofm = dominant_material(base, up=True)
    report = {'prefix': PREFIX, 'faction': FACTION, 'seed': SEED, 'size_m': [round(W, 2), round(D, 2), round(H, 2)],
              'wall': dom.name, 'roof': roofm.name, 'cover': round(frac, 2), 'yard': yard}

    # ---------------- o danificado (70%): 3–4 mordidas na borda do telhado e nas quinas
    dmg = copy_obj(base, 'dmg')
    bites = []
    corners = [(lo.x, lo.y), (hi.x, lo.y), (lo.x, hi.y), (hi.x, hi.y)]
    edges = roof_edges(base, lo, hi, H)
    rnd.shuffle(edges)
    for e in edges:
        if len(bites) >= 4:
            break
        if any((e - b).length < 4.0 * k for b in bites):
            continue
        r = rnd.uniform(1.3, 2.1) * k
        c = Vector((e.x, e.y, e.z - 0.2 * r))
        boolean(dmg, cutter(f'bite{len(bites)}', c, r, seed=rnd.uniform(0, 50)))
        bites.append(c)
    parts = [dmg]
    if bites:
        ctr = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, 0))
        parts.append(rebar(bites, Vector((0, 0, 0.6)), rnd, n=4))
    dmg = join(parts, 'dmg')
    report['fire_damaged_m'] = [U(b + Vector((0, 0, 0.3))) for b in bites]

    # ---------------- o muito danificado (35%): mais mordidas, a quina que cai, a laje pendurada, o interior e o talude
    wr = copy_obj(dmg, 'wr')
    extra = []
    for e in edges:
        if len(extra) >= 2:
            break
        if any((e - b).length < 5.0 * k for b in bites + extra):
            continue
        r = rnd.uniform(2.0, 2.8) * k
        c = Vector((e.x, e.y, e.z - 0.2 * r))
        boolean(wr, cutter(f'big{len(extra)}', c, r, seed=rnd.uniform(0, 50), stretch=(1.2, 1.0, 0.9)))
        extra.append(c)
    # a quina que cai: a célula de ~35% × 35% da planta, de ~30% da altura pra cima (uma célula só: as duas paredes e o canto do telhado)
    cx, cy = corners[rnd.randrange(4)]
    fx = 1 if cx == lo.x else -1
    fy = 1 if cy == lo.y else -1
    c_lo = Vector((min(cx, cx + fx * 0.38 * W), min(cy, cy + fy * 0.38 * D), H * 0.3))
    c_hi = Vector((max(cx, cx + fx * 0.38 * W), max(cy, cy + fy * 0.38 * D), H + 2.0))
    c_lo.x -= 1.0 if fx > 0 else 0.0
    c_hi.x += 1.0 if fx < 0 else 0.0
    c_lo.y -= 1.0 if fy > 0 else 0.0
    c_hi.y += 1.0 if fy < 0 else 0.0
    # a laje pendurada: o pedaço do telhado vizinho ao canto, recortado e girado pela borda que segura (a dobradiça da ferragem)
    sl_lo = Vector((min(cx + fx * 0.38 * W, cx + fx * 0.62 * W), min(cy, cy + fy * 0.30 * D), H * 0.55))
    sl_hi = Vector((max(cx + fx * 0.38 * W, cx + fx * 0.62 * W), max(cy, cy + fy * 0.30 * D), H + 2.0))
    # (01/11) no pátio, a laje só se a área dela for teto de verdade (senão sobrava a chapa enorme inclinada por cima da pista)
    samples = [(sl_lo.x + (sl_hi.x - sl_lo.x) * (i + 0.5) / 4, sl_lo.y + (sl_hi.y - sl_lo.y) * (j + 0.5) / 4) for i in range(4) for j in range(4)]
    # (01/11, o Strategy Center v3: o pentágono com o anel de dentro mais alto) e só se ali o telhado está na altura H: a laje é do
    # telhado que está lá (no pentágono a caixa do lote tem os cantos vazios e o anel de fora é mais baixo; a chapa flutuava no ar)
    use_slab = sum(covered(px, py) and abs(top(px, py) - H) < 1.2 for px, py in samples) >= 11
    report['slab'] = use_slab
    if use_slab:
        boolean(wr, box_cutter('slab_hole', sl_lo, sl_hi, rough=0.05, seed=3.0, cells=3))
    # a laje que cai pendurada: a chapa do telhado (0,3 m) com a borda solta quebrada, na cor do telhado e o miolo por baixo
    slab_src = box_cutter('slab', Vector((sl_lo.x + 0.1, sl_lo.y + 0.1, H - 0.3)), Vector((sl_hi.x - 0.1, sl_hi.y - 0.1, H)),
                          rough=0.04, seed=5.0, cells=5)
    slab_src.data.materials.clear()
    slab_src.data.materials.append(material_at(base, (sl_lo.x + sl_hi.x) / 2, (sl_lo.y + sl_hi.y) / 2, hi.z) or roofm)
    slab_src.data.materials.append(BROKEN)
    for poly in slab_src.data.polygons:
        poly.material_index = 0 if poly.normal.z > 0.5 else 1
    boolean(wr, box_cutter('corner', c_lo, c_hi, rough=0.18, seed=rnd.uniform(0, 30), cells=6))
    hinge_x = cx + fx * 0.62 * W
    ang = math.radians(rnd.uniform(22, 34)) * (fx)
    piv = Vector((hinge_x, (sl_lo.y + sl_hi.y) / 2, H))
    slab_src.matrix_world = Matrix.Translation(piv) @ Matrix.Rotation(ang, 4, 'Y') @ Matrix.Translation(-piv) @ slab_src.matrix_world
    bpy.context.view_layer.update()
    model_lib.select_only(slab_src)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # o interior que aparece: pilares a cada ~4,5 m e a laje do andar (só se vê pelos buracos)
    bm = bmesh.new()
    nx, ny = max(2, int(W / 4.5)), max(2, int(D / 4.5))
    for i in range(1, nx):
        for j in range(1, ny):
            x, y = lo.x + W * i / nx, lo.y + D * j / ny
            top = H * rnd.uniform(0.55, 0.85)
            if yard and not covered(x, y):
                continue
            r = bmesh.ops.create_cube(bm, size=1.0)
            bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((x, y, top / 2))) @ Matrix.Diagonal((0.45, 0.45, top, 1.0)), verts=r['verts'])
    if H > 7.0 and not yard:
        r = bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, H * 0.48)))
                            @ Matrix.Diagonal((W * 0.9, D * 0.9, 0.25, 1.0)), verts=r['verts'])
    me = bpy.data.meshes.new('inside')
    bm.to_mesh(me)
    bm.free()
    inside = bpy.data.objects.new('inside', me)
    bpy.context.scene.collection.objects.link(inside)
    inside.data.materials.append(BROKEN)
    foot = Vector((cx + fx * 0.15 * W, cy + fy * 0.15 * D, 0.0))
    talus = chunks(foot, Vector((0.14 * W + 1.0, 0.14 * D + 1.0, 0)), 34, 1.1 * k, [dom, dom, roofm, BROKEN], rnd, name='talus',
                   lo=lo, hi=hi)
    if not use_slab:
        bpy.data.objects.remove(slab_src, do_unlink=True)
    wr = join([wr] + ([slab_src] if use_slab else []) + [inside, talus, rebar(extra + [Vector(((c_lo.x + c_hi.x) / 2, (c_lo.y + c_hi.y) / 2, H * 0.6))],
                                                   Vector((0, 0, 0.3)), rnd, n=6)], 'wr')
    report['fire_wrecked_m'] = [U(b + Vector((0, 0, 0.3))) for b in bites + extra] + [U(Vector((foot.x, foot.y, H * 0.5)))]

    # ---------------- a ruína: o corte de topo quebrado (0,6–1,5 m), os ossos e o monte
    ru = copy_obj(wr, 'ru')
    top = box_cutter('ruin_top', Vector((min(lo.x, lot_lo.x) - 4, min(lo.y, lot_lo.y) - 4, 1.05)),
                     Vector((max(hi.x, lot_hi.x) + 4, max(hi.y, lot_hi.y) + 4, H + 8)), rough=0.0, seed=1.0, cells=1)
    # o fundo do cortador em ondas de 0,6–1,5 m (a altura quebrada das muretas), e (01/11) duas quinas que ficam de pé até ~35–55% da
    # altura (a ruína de AAA não é um tapete: o canto da parede amarrado pelas duas faces é o que resiste; nunca a quina que caiu)
    others = [q for q in corners if q != (cx, cy)]
    rnd.shuffle(others)
    stand = [(Vector((qx, qy, 0.0)), H * rnd.uniform(0.35, 0.55)) for qx, qy in others[:2]]
    R = 0.2 * min(W, D) + 1.0

    def ruin_z(x, y):
        z = 1.05 + 0.45 * noise.noise(Vector((x * 0.35, y * 0.35, SEED + 0.5)))
        for q, A in stand:
            d2 = (x - q.x) ** 2 + (y - q.y) ** 2
            z = max(z, 1.05 + A * math.exp(-d2 / (R * R)) * (0.85 + 0.3 * noise.noise(Vector((x * 0.6, y * 0.6, SEED + 2.0)))))
        return min(z, H * 0.6)
    me = top.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bottom = [f for f in bm.faces if f.normal.z < -0.9]
    bmesh.ops.subdivide_edges(bm, edges=list({e for f in bottom for e in f.edges}), cuts=28, use_grid_fill=True)
    for v in bm.verts:
        if v.co.z < 2.0:
            v.co.z = ruin_z(v.co.x, v.co.y)
    bm.to_mesh(me)
    bm.free()
    boolean(ru, top)
    bones = []
    bm = bmesh.new()
    for i in range(rnd.randint(1, 3)):
        x, y = rnd.uniform(lo.x + 0.25 * W, hi.x - 0.25 * W), rnd.uniform(lo.y + 0.25 * D, hi.y - 0.25 * D)
        if yard and not covered(x, y):
            continue
        h = rnd.uniform(2.5, 4.0)
        r = bmesh.ops.create_cube(bm, size=1.0)
        tilt = Matrix.Rotation(rnd.uniform(-0.15, 0.15), 4, 'X') @ Matrix.Rotation(rnd.uniform(-0.15, 0.15), 4, 'Y')
        bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector((x, y, 0))) @ tilt @ Matrix.Translation(Vector((0, 0, h / 2)))
                            @ Matrix.Diagonal((0.45, 0.45, h, 1.0)), verts=r['verts'])
        bones.append(Vector((x, y, h)))
    me = bpy.data.meshes.new('bones')
    bm.to_mesh(me)
    bm.free()
    bo = bpy.data.objects.new('bones', me)
    bpy.context.scene.collection.objects.link(bo)
    bo.data.materials.append(BROKEN)
    pile = mound(lo, hi, min(2.2, max(0.7, H / 5.0)), [dom, BROKEN], rnd)
    scatter = chunks(Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, 0)), Vector((W * 0.32, D * 0.32, 0)), 70, 0.9 * k,
                     [dom, dom, roofm, BROKEN], rnd, name='scatter', lo=lo, hi=hi)
    ru = join([ru, bo, pile, scatter, rebar(bones, Vector((0, 0, 1.0)), rnd, n=3)], 'ru')
    report['bones_m'] = [U(b) for b in bones]

    # ---------------- as máscaras e os GLB
    bpy.data.objects.remove(base, do_unlink=True)
    finish(dmg, PREFIX + '_Damaged')
    finish(wr, PREFIX + '_Wrecked')
    finish(ru, PREFIX + '_Ruin')
    with open(os.path.join(DIR, PREFIX + '_damage.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1, ensure_ascii=False)
    print('DAMAGE', json.dumps(report, ensure_ascii=False))


main()
