"""O toco e o tronco quebrado de cada palmeira (16/10, o pedido do Felipe pelo Look: o tanque derruba a palmeira; o
estudo em docs/WORLDFORGE.md §12.8). A palmeira leve (Intermediate/Assets/date_palm, BlastLab/Tools/blender_palm_lite.py)
é cortada na altura da quebra:
- SM_<palmeira>_Stump: o que fica de pé (o ZH: o toco, StumpName);
- SM_<palmeira>_Fallen: o resto, com o pivô (a origem) no centro da quebra; o jogo gira por ele na queda.
A quebra de um tronco de palmeira é fibrosa (o caule é um feixe de fibras, sem cerne): o miolo rasgado e as lascas
compridas em volta, claras. Aqui: a tampa da seção com o miolo puxado por ruído, e lascas finas (cunhas de 3–6 cm de
base e 10–45 cm de comprimento) nascendo da borda, inclinadas pra fora; as do toco pra cima, as do tronco caído pra
baixo. A tampa e as lascas vão no material 2 (PalmBreak); 0 folhagem e 1 tronco ficam como na palmeira.

  blender -b -P blender_palm_break.py -- <entrada.glb> <pasta de saída> [altura da quebra em m, 0,8]
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
src, out_dir = argv[0], argv[1]
cut = float(argv[2]) if len(argv) > 2 else 0.8
name = os.path.splitext(os.path.basename(src))[0]
os.makedirs(out_dir, exist_ok=True)


def fresh():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    # sem pai (o glTF põe um vazio às vezes) e com as transformações aplicadas
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for x in list(bpy.context.scene.objects):
        if x.type != 'MESH':
            bpy.data.objects.remove(x, do_unlink=True)
    return o


def trunk_index(o):
    for i, m in enumerate(o.data.materials):
        if m and 'trunk' in m.name.lower():
            return i
    return min(1, len(o.data.materials) - 1)


def break_material(o):
    m = bpy.data.materials.get('M_PalmBreak') or bpy.data.materials.new('M_PalmBreak')
    m.use_nodes = True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.42, 0.31, 0.19, 1.0)   # fibra seca, marrom-clara
    m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.9
    while len(o.data.materials) < 2:
        o.data.materials.append(None)
    o.data.materials.append(m)
    return len(o.data.materials) - 1


def rings(cut_edges):
    """As bordas da quebra, uma por tronco (o cacho tem vários): os vértices ligados pelas arestas do corte."""
    left = set(cut_edges)
    out = []
    while left:
        e = left.pop()
        comp, stack = {e.verts[0], e.verts[1]}, [e.verts[0], e.verts[1]]
        while stack:
            v = stack.pop()
            for le in v.link_edges:
                if le in left:
                    left.discard(le)
                    for w in le.verts:
                        if w not in comp:
                            comp.add(w)
                            stack.append(w)
        if len(comp) >= 6:
            out.append(list(comp))
    return out


def split(keep_below):
    o = fresh()
    bi = break_material(o)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    # o glTF separa os vértices na costura da textura: soldados, a borda do corte fecha (senão a tampa não nasce e o
    # tronco fica oco); o UV fica nos cantos das faces, então a costura não perde nada
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.0005)
    lay = bm.faces.layers.int.new('cap')   # (antes de guardar referências: criar a camada realoca os elementos)
    ti = trunk_index(o)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, cut), plane_no=(0, 0, 1),
                               clear_outer=keep_below, clear_inner=not keep_below)
    # só o corte do tronco (no cacho, folhas a 0,8 m também são cortadas: essas ficam abertas, são finas)
    cut_edges = [e for e in r['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_valid
                 and any(f.material_index == ti for f in e.link_faces)]
    sgn = 1.0 if keep_below else -1.0
    rnd = random.Random((hash(name) & 0xffff) * 2 + (1 if keep_below else 0))
    # só as bordas do tronco (a folhagem cortada na altura da quebra não fecha: no toco não tem folha a 0,8 m)
    loops = [rg for rg in rings(cut_edges)]
    centers = []
    for rg in loops:
        c = sum((v.co for v in rg), Vector()) / len(rg)
        rad = sum(((v.co - c).xy.length for v in rg)) / len(rg)
        centers.append((c, rad))
    # o miolo: um disco denso no tamanho do tronco (polar, 10 anéis × 56 raios), no lugar da tampa preenchida (que
    # saía em placas grandes e lisas, e o serrilhado da borda esticava a casca); as fibras em relevo por faixas radiais
    for (c, rad) in centers:
        NR, NA = 10, 56
        grid = []
        for i in range(NR + 1):
            rr = i / NR
            row = []
            for j in range(NA if i else 1):
                a = 2 * math.pi * j / NA
                fib = 0.5 + 0.5 * math.sin(11 * a + 3.0 * rr) * math.sin(5 * a - 2.0)
                z = sgn * (0.02 + 0.16 * fib * (0.4 + 0.6 * rr) + rnd.uniform(0.0, 0.04)) if i else sgn * 0.02
                row.append(bm.verts.new(Vector((c.x + math.cos(a) * rad * 1.03 * rr, c.y + math.sin(a) * rad * 1.03 * rr, cut + z))))
            grid.append(row)
        for i in range(NR):
            for j in range(NA):
                j1 = (j + 1) % NA
                vs = (grid[0][0], grid[1][j], grid[1][j1]) if i == 0 else (grid[i][j], grid[i + 1][j], grid[i + 1][j1], grid[i][j1])
                try:
                    f = bm.faces.new(vs if keep_below else tuple(reversed(vs)))
                    f.material_index = bi
                except ValueError:
                    pass
    # as fibras: o tufo rasgado em volta (mais finas e mais curtas que lascas de madeira: a palmeira é feixe de fibras),
    # abrindo pra fora e curvando (cada fibra em dois trechos), umas poucas compridas
    for (c, rad) in centers:
        n = max(16, int(2 * math.pi * rad / 0.028))
        for k in range(n):
            if rnd.random() < 0.2:
                continue
            a = 2 * math.pi * (k + rnd.uniform(-0.4, 0.4)) / n
            od = Vector((math.cos(a), math.sin(a), 0.0))
            side = Vector((-od.y, od.x, 0.0))
            base = c + od * rad * rnd.uniform(0.7, 0.97)
            base.z = cut + sgn * rnd.uniform(0.0, 0.08)
            h = rnd.uniform(0.04, 0.18) if rnd.random() < 0.85 else rnd.uniform(0.2, 0.34)
            w = rnd.uniform(0.015, 0.035)
            lean = rnd.uniform(0.25, 0.8)
            mid = base + Vector((0, 0, sgn * h * 0.55)) + od * h * lean * 0.35
            tip = base + Vector((0, 0, sgn * h * (0.75 + rnd.uniform(-0.1, 0.1)))) + od * h * lean + side * rnd.uniform(-0.03, 0.03)
            b0 = bm.verts.new(base - side * w * 0.5)
            b1 = bm.verts.new(base + side * w * 0.5)
            m0 = bm.verts.new(mid - side * w * 0.3)
            m1 = bm.verts.new(mid + side * w * 0.3)
            tv = bm.verts.new(tip)
            for quad in ((b0, b1, m1, m0),):
                try:
                    bm.faces.new(quad).material_index = bi
                except ValueError:
                    pass
            try:
                bm.faces.new((m0, m1, tv)).material_index = bi
            except ValueError:
                pass
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(o.data)
    bm.free()
    o.data.update()
    if not keep_below:
        # o pivô no centro da quebra (o jogo gira o tronco caído por ele): o centro das bordas, na altura do corte
        c = sum((cr[0] for cr in centers), Vector()) / max(len(centers), 1) if centers else Vector((0, 0, cut))
        c.z = cut
        for v in o.data.vertices:
            v.co -= c
    tag = 'Stump' if keep_below else 'Fallen'
    o.name = '%s_%s' % (name, tag)
    path = os.path.join(out_dir, '%s_%s.glb' % (name, tag))
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False)
    tri = sum(len(p.vertices) - 2 for p in o.data.polygons)
    zs = [v.co.z for v in o.data.vertices]
    print('PALMBREAK', tag, path, '%d triangulos' % tri, 'troncos %d' % len(centers), 'z %.2f..%.2f' % (min(zs), max(zs)))


split(True)
split(False)
