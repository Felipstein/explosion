"""A pedra pequena quebrada pelo tanque (16/10, o pedido do Felipe pelo Look: "a pedra pequena quebrando e a grande ele
sobe"; o estudo em docs/WORLDFORGE.md §12.8). A pedra escaneada (Poly Haven) é partida por 2–3 planos ao acaso perto do
centro (4–8 pedaços); cada pedaço se abre pra fora do centro (o peso da lagarta esmaga), gira um pouco e assenta no
chão. A face nova da quebra (a tampa do corte) vai num material à parte, a pedra fresca, mais clara (material 1); a
casca fica com o da pedra (0). Sai <malha>_Broken.glb com o pivô onde estava o da pedra.

  blender -b -P blender_rock_break.py -- <entrada.fbx> <saída.glb> [semente]
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
src, out = argv[0], argv[1]
seed = int(argv[2]) if len(argv) > 2 else 7
rnd = random.Random(seed)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
# a malha de mais detalhe (o FBX traz as LODs): a de mais vértices
o = max(objs, key=lambda x: len(x.data.vertices))
for x in list(bpy.context.scene.objects):
    if x is not o:
        bpy.data.objects.remove(x, do_unlink=True)
bpy.context.view_layer.objects.active = o
o.select_set(True)
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
# a pedra fresca (a face da quebra)
fresh = bpy.data.materials.new('M_RockFresh')
fresh.use_nodes = True
fresh.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.55, 0.45, 0.36, 1.0)
fresh.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.85
if not o.data.materials:
    o.data.materials.append(bpy.data.materials.new('M_Rock'))
o.data.materials.append(fresh)
fi = len(o.data.materials) - 1

bb = [Vector(v.co) for v in o.data.vertices]
lo = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
hi = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
center = (lo + hi) * 0.5
size = hi - lo
print('ROCKBREAK tamanho %.2f x %.2f x %.2f' % tuple(size))


def split(bm_src, co, no):
    """Os dois lados de um plano, cada um com a tampa fechada no material da pedra fresca."""
    parts = []
    for keep_inner in (True, False):
        bm = bm_src.copy()
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_outer=keep_inner, clear_inner=not keep_inner)
        edges = [e for e in r['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
        filled = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
        caps = list(filled.get('faces', []))
        if caps:
            tri = bmesh.ops.triangulate(bm, faces=caps)
            caps = list(tri['faces'])
            res = bmesh.ops.subdivide_edges(bm, edges=list({e for f in caps for e in f.edges}), cuts=3, use_grid_fill=True)
            inner = [v for v in res['geom_inner'] if isinstance(v, bmesh.types.BMVert)]
            nrm = no if keep_inner else -no
            amp = 0.03 * max(size.x, size.y, size.z)
            for v in inner:
                # a quebra da pedra é em lascas: o ruído grosso pela posição e o miúdo ao acaso, ao longo da normal do corte
                s = math.sin(v.co.x * 9.0 + 1.3) * math.sin(v.co.y * 7.0 - 0.7) * math.sin(v.co.z * 8.0 + 2.1)
                v.co += nrm * amp * (0.8 * s + rnd.uniform(-0.15, 0.15))
        for f in bm.faces:
            if f.is_valid and abs((f.calc_center_median() - co).dot(no)) < 0.12 * max(size.x, size.y, size.z) and not f.material_index:
                pass
        for f in (filled.get('faces', []) if not caps else []):
            f.material_index = fi
        # a tampa (os triângulos novos e os filhos) no material da pedra fresca: as faces cuja normal acompanha a do corte
        for f in bm.faces:
            if f.is_valid and abs(f.normal.dot(no)) > 0.6 and abs((f.calc_center_median() - co).dot(no)) < 0.1 * max(size.x, size.y, size.z):
                f.material_index = fi
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        if len(bm.verts) > 8:
            parts.append(bm)
        else:
            bm.free()
    return parts


bm0 = bmesh.new()
bm0.from_mesh(o.data)
bmesh.ops.remove_doubles(bm0, verts=bm0.verts[:], dist=0.0005)
pieces = [bm0]
for k in range(rnd.choice((2, 3))):
    a = rnd.uniform(0, math.pi)
    tilt = rnd.uniform(-0.5, 0.5)
    no = Vector((math.cos(a), math.sin(a), tilt)).normalized()
    co = center + Vector((rnd.uniform(-0.15, 0.15) * size.x, rnd.uniform(-0.15, 0.15) * size.y, rnd.uniform(-0.1, 0.2) * size.z))
    nxt = []
    for p in pieces:
        nxt += split(p, co, no)
        p.free()
    pieces = nxt
print('ROCKBREAK pedaços', len(pieces))

# cada pedaço se abre pra fora do centro, gira um pouco e assenta no chão (a base no z mais baixo da pedra)
merged = bmesh.new()
for p in pieces:
    c = sum((v.co for v in p.verts), Vector()) / max(len(p.verts), 1)
    d = (c - center)
    d.z = 0.0
    d = d.normalized() if d.length > 1e-4 else Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), 0)).normalized()
    push = d * rnd.uniform(0.04, 0.16) * max(size.x, size.y)
    rot = Matrix.Rotation(rnd.uniform(-0.25, 0.25), 4, Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.3, 0.3))).normalized())
    for v in p.verts:
        v.co = rot @ (v.co - c) + c + push
    # assenta: o ponto mais baixo no chão, afundado um pouco (o peso)
    zmin = min(v.co.z for v in p.verts)
    sink = rnd.uniform(0.04, 0.15) * size.z
    for v in p.verts:
        v.co.z += lo.z - zmin - sink
    tmp = bpy.data.meshes.new('tmp')
    p.to_mesh(tmp)
    merged.from_mesh(tmp)
    bpy.data.meshes.remove(tmp)
    p.free()
# o cascalho: 14–22 lascas pequenas (2–8 cm) em volta, no material da pedra fresca
for k in range(rnd.randint(14, 22)):
    a = rnd.uniform(0, 2 * math.pi)
    r = rnd.uniform(0.35, 0.75) * max(size.x, size.y)
    c = Vector((center.x + math.cos(a) * r, center.y + math.sin(a) * r, lo.z))
    s = rnd.uniform(0.02, 0.08)
    vs = [merged.verts.new(c + Vector((rnd.uniform(-s, s), rnd.uniform(-s, s), rnd.uniform(0.0, s * 0.8)))) for _ in range(4)]
    for tri in ((0, 1, 2), (0, 2, 3), (0, 3, 1), (1, 3, 2)):
        try:
            merged.faces.new([vs[i] for i in tri]).material_index = fi
        except ValueError:
            pass
# esmagada: a pilha toda baixa a 65% da altura (o peso da lagarta), o chão no mesmo lugar
for v in merged.verts:
    v.co.z = lo.z + (v.co.z - lo.z) * 0.65
bmesh.ops.recalc_face_normals(merged, faces=merged.faces[:])
merged.to_mesh(o.data)
merged.free()
o.data.update()
name = os.path.splitext(os.path.basename(out))[0]
o.name = name
os.makedirs(os.path.dirname(out), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=False)
print('ROCKBREAK', out, len(o.data.polygons), 'faces')
