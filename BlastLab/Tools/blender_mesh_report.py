"""Relatório de uma malha GLB pelo Blender (sem interface).

Uso: blender -b --factory-startup -P blender_mesh_report.py -- arquivo.glb
Imprime objetos, triângulos, tamanho e as partes soltas (pedaços de malha
sem ligação entre si), que é o que diz se dá pra separar rodas e esteira
sem cortar.
"""
import sys

import bmesh
import bpy

path = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=path)
for ob in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    tris = sum(len(f.verts) - 2 for f in bm.faces)
    xs = [v.co.x for v in bm.verts]; ys = [v.co.y for v in bm.verts]; zs = [v.co.z for v in bm.verts]
    print(f'REPORT {ob.name}: verts={len(bm.verts)} tris={tris} '
          f'size=({max(xs)-min(xs):.3f}, {max(ys)-min(ys):.3f}, {max(zs)-min(zs):.3f}) '
          f'materials={[m.name for m in me.materials]}')
    # partes soltas por vértices ligados por aresta; vértices na mesma posição
    # (costura de UV) contam como ligados
    key = {}
    for v in bm.verts:
        key.setdefault((round(v.co.x, 5), round(v.co.y, 5), round(v.co.z, 5)), []).append(v.index)
    parent = list(range(len(bm.verts)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for group in key.values():
        for i in group[1:]:
            union(group[0], i)
    for e in bm.edges:
        union(e.verts[0].index, e.verts[1].index)
    comps = {}
    for v in bm.verts:
        comps.setdefault(find(v.index), []).append(v)
    sizes = sorted(((len(c), c) for c in comps.values()), key=lambda t: -t[0])
    print(f'REPORT {ob.name}: partes soltas={len(sizes)}')
    for n, c in sizes[:25]:
        cx = [v.co.x for v in c]; cy = [v.co.y for v in c]; cz = [v.co.z for v in c]
        print(f'  parte verts={n} centro=({sum(cx)/n:.3f},{sum(cy)/n:.3f},{sum(cz)/n:.3f}) '
              f'tam=({max(cx)-min(cx):.3f},{max(cy)-min(cy):.3f},{max(cz)-min(cz):.3f})')
    bm.free()
