"""Roda um modelo provisório com o detalhe AAA (31/10/2026; docs/ESTUDO_DETALHE_AAA.md): o chanfro redondo, a camuflagem no material e
as máscaras na cor de vértice (model_detail.py), sem mexer no script do modelo.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_china_overlord.py captures/game/models/china_overlord

O 1º argumento é a classe (vehicle ou building); depois vêm o script do modelo e os argumentos dele. As exportações do script ficam
seguras até ele acabar; aí as peças (na posição de repouso, como o script as montou) levam a grade, o UV e as máscaras todas juntas (a
sombra de contato de uma peça na outra), e saem os GLB. O andaime da obra (…_Scaffold) não faz sombra nas outras: leva as máscaras
sozinho.
"""
import os
import runpy
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import model_lib  # noqa: E402
from model_detail import CLASS, bake_masks, grid, uv_meters  # noqa: E402

args = sys.argv[sys.argv.index('--') + 1:]
KIND, SCRIPT, REST = args[0], os.path.abspath(args[1]), args[2:]
model_lib.detail_mode(KIND)
model_lib.DETAIL['defer'] = []
sys.argv = [SCRIPT, '--'] + REST
runpy.run_path(SCRIPT, run_name='__main__')
pending = model_lib.DETAIL['defer']
model_lib.DETAIL['defer'] = None
objs = [o for _, os_, _, _ in pending for o in os_]


def take_flags(o):
    """(01/11) Tira da malha o pano das bandeiras (o material <m>Flag do model_lib.flag) e devolve o mastro e o tamanho de cada uma, no
    quadro da Unreal (x, −y do Blender, z; m): o jogo desenha o pano no vento. O pano do model_lib vai pra trás (−X): o mastro é o x máximo."""
    me = o.data
    idx = [i for i, m in enumerate(me.materials) if m and m.name.split('.')[0].endswith('Flag')]
    if not idx:
        return []
    bm = bmesh.new()
    bm.from_mesh(me)
    flagged = [f for f in bm.faces if f.material_index in idx]
    out = []
    seen = set()
    for f0 in flagged:
        if f0.index in seen:
            continue
        stack, isl = [f0], []
        seen.add(f0.index)
        while stack:
            g = stack.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen and h.material_index in idx:
                        seen.add(h.index)
                        stack.append(h)
        vs = [o.matrix_world @ v.co for f in isl for v in f.verts]
        xs, ys, zs = [v.x for v in vs], [-v.y for v in vs], [v.z for v in vs]   # (y na Unreal = −y do Blender)
        # o pano corre pelo eixo mais comprido (x no model_lib.flag, y no quartel da China); o mastro é a ponta mais alta (o pano
        # afina pra ponta solta)
        ax = 0 if max(xs) - min(xs) >= max(ys) - min(ys) else 1
        cs = xs if ax == 0 else ys
        lo, hi = min(cs), max(cs)
        span = max(hi - lo, 1e-3)

        def end_h(at):
            zz = [z for c, z in zip(cs, zs) if abs(c - at) < span * 0.06]
            return (max(zz) - min(zz)) if zz else 0.0
        mast = hi if end_h(hi) >= end_h(lo) else lo
        mx, my = (mast, (min(ys) + max(ys)) / 2) if ax == 0 else ((min(xs) + max(xs)) / 2, mast)
        out.append([round(mx, 3), round(my, 3), round(max(zs), 3), round(span, 3), round(max(zs) - min(zs), 3)])
    bmesh.ops.delete(bm, geom=flagged, context='FACES')
    bm.to_mesh(me)
    bm.free()
    me.update()
    return out


flags = {}
for _, os_, out, name in pending:
    for o in os_:
        fl = take_flags(o)
        if fl:
            flags.setdefault(name or o.name, []).extend(fl)
# o andaime pelo nome da exportação (as peças dele nem sempre levam o nome)
is_scaf = {o.as_pointer() for _, os_, out, name in pending for o in os_ if 'Scaffold' in (name or '') + os.path.basename(str(out))}
main = [o for o in objs if o.as_pointer() not in is_scaf and 'Scaffold' not in o.name]
scaf = [o for o in objs if o not in main]
before = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)
for o in objs:
    # (31/10) o andaime sem a grade: os tubos compridos viravam 105–175 mil triângulos; ele só aparece na obra, a sombra de contato
    # nos vértices dele basta
    if o in main:
        grid(o, CLASS[KIND]['cell'])
    uv_meters(o)
if main:
    bake_masks(main, KIND)
if scaf:
    bake_masks(scaf, KIND)
after = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)


def islands(bm):
    """As ilhas de faces ligadas (cada primitiva do script: o pneu, o disco, um parafuso)."""
    bm.faces.ensure_lookup_table()
    seen, out = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, isl = [f], []
        seen.add(f.index)
        while stack:
            g = stack.pop()
            isl.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        out.append(isl)
    return out


def track_uv(o):
    """(01/11, a esteira que anda) o UV2.x e o UV3.x de cada vértice = a tangente (x, z) × o passo do elo dele (a ilha inteira vai com o
    elo mais perto do centro dela: o elo não se rasga). O material desliza o vértice por ela × a fase que o jogo manda."""
    from mathutils import kdtree
    fr, st = o.get('track_frames'), o.get('track_step')
    if not fr or not st:
        return None
    fr = list(fr)
    n = len(fr) // 5
    kd = kdtree.KDTree(n)
    for i in range(n):
        kd.insert((fr[5 * i], fr[5 * i + 1], fr[5 * i + 2]), i)
    kd.balance()
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    ux = bm.loops.layers.uv.new('TrackX')
    uz = bm.loops.layers.uv.new('TrackZ')
    for isl in islands(bm):
        vs = {v for f in isl for v in f.verts}
        c = sum((v.co for v in vs), Vector()) / len(vs)
        _, i, _ = kd.find(o.matrix_world @ c)
        ex, ez = fr[5 * i + 3] * st, fr[5 * i + 4] * st
        for f in isl:
            for lp in f.loops:
                lp[ux].uv = (ex, 0.0)
                lp[uz].uv = (ez, 0.0)
    bm.to_mesh(me)
    bm.free()
    me.update()
    return st


def split_wheels(o):
    """(01/11) a rodagem do veículo de lagarta numa malha por roda (wheel_001...): o jogo gira cada uma pelo centro da caixa dela (o
    mesmo das rodas do caminhão). Cada ilha vai pra roda cujo disco contém o centro dela (do mesmo lado); a maior abre a roda."""
    bm = bmesh.new()
    bm.from_mesh(o.data)
    info = []
    for isl in islands(bm):
        vs = {v for f in isl for v in f.verts}
        xs, ys, zs = [v.co.x for v in vs], [v.co.y for v in vs], [v.co.z for v in vs]
        info.append(([f.index for f in isl], (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2, (min(ys) + max(ys)) / 2,
                     max(max(xs) - min(xs), max(zs) - min(zs)) / 2))
    bm.free()
    info.sort(key=lambda t: -t[4])
    wheels = []
    for faces, x, z, y, r in info:
        for w in wheels:
            if (y > 0) == (w[2] > 0) and (x - w[0]) ** 2 + (z - w[1]) ** 2 < (w[3] * 0.95) ** 2:
                w[4].extend(faces)
                break
        else:
            wheels.append([x, z, y, r, list(faces)])
    wheels.sort(key=lambda w: (w[2] > 0, w[0], w[1]))
    out = []
    for k, w in enumerate(wheels):
        keep = set(w[4])
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context='FACES')
        me = bpy.data.meshes.new('wheel_%03d' % (k + 1))
        bm.to_mesh(me)
        bm.free()
        for m in o.data.materials:
            me.materials.append(m)
        ca = me.color_attributes.get('Col')
        if ca is not None:
            me.color_attributes.active_color = ca
            me.color_attributes.render_color_index = me.color_attributes.find('Col')
        w_o = bpy.data.objects.new('wheel_%03d' % (k + 1), me)
        w_o.matrix_world = o.matrix_world.copy()
        bpy.context.scene.collection.objects.link(w_o)
        out.append(w_o)
    return out


tracked = any(o.get('track_frames') for o in objs)
steps = {}
total = 0
for kind, os_, out, name in pending:
    for o in os_:
        st = track_uv(o)
        if st:
            steps[o.name] = round(st, 5)
    if tracked and kind == 'one' and name.endswith('_Wheels'):
        ws = split_wheels(os_[0])
        print('DETAIL %s: %d rodas separadas (wheel_*)' % (name, len(ws)))
        total += model_lib.export_group(ws, out, name)
        continue
    total += model_lib.export(os_[0], out) if kind == 'one' else model_lib.export_group(os_, out, name)
if flags:
    import json
    with open(os.path.join(pending[0][2], 'flags.json'), 'w', encoding='utf-8') as f:
        json.dump(flags, f, indent=1)
    print('DETAIL bandeiras:', {k: len(v) for k, v in flags.items()})
if steps:
    # o passo de cada esteira (o jogo anda a fase por ele: Units.json visual.track_step_m, pelo elenco_units.py)
    import json
    outdir = pending[0][2]
    with open(os.path.join(outdir, 'tracks.json'), 'w', encoding='utf-8') as f:
        json.dump(steps, f, indent=1)
    print('DETAIL esteiras:', steps)
print('DETAIL %s: triângulos %d -> %d (exportados %d)' % (os.path.basename(SCRIPT), before, after, total))
