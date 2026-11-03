"""O detalhe AAA dos provisórios por script (31/10/2026; o estudo: docs/ESTUDO_DETALHE_AAA.md).

Na câmera do jogo 1 px vale 3–5 cm: o que tira a cara de "lego" não é parafuso, é a sombra de contato entre as peças, a sujeira
nas frestas, a quina arredondada que pega o sol e o desgaste nas quinas. Aqui, depois de o modelo estar montado (todas as peças
na posição de repouso, como no jogo):

- grid(o, cell): divide as arestas longas pra cor de vértice ter resolução (a forma fica idêntica: só corta, nada de remesh);
- bake_masks(objs, kind): as máscaras na cor de vértice (o atributo 'Col', por canto), com BRANCO = NEUTRO (o modelo sem máscara
  fica como sempre no material):
    R = 1 − quina convexa (o ângulo das arestas: com o chanfro de 3 segmentos, a faixa do chanfro),
    G = a oclusão de contato (raios contra o modelo inteiro + o chão em z = 0: a barriga, a caixa de roda, o anel da torre),
    B = 1 − fresta (aresta côncava + a oclusão curta: onde o pó assenta);
- uv_meters(o): UV0 em metros (projeção pela face) e UV1 = a semente de cada ilha (cada chapa um tom no material).

Uso no modelo: from model_lib import detail_mode; detail_mode('vehicle') antes de criar as peças; no fim, com as peças prontas:
    from model_detail import finish_detail; finish_detail([hull, turret, ...], 'vehicle')
e o export() de sempre (o model_lib leva a cor e o UV quando existem).
"""
import math
import time

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

CLASS = {
    # célula da grade (m), alcance da oclusão (m), alcance da fresta (m)
    'vehicle': {'cell': 0.14, 'ao': 1.1, 'short': 0.14},
    'building': {'cell': 0.40, 'ao': 3.2, 'short': 0.35},
}
RAYS = 40


def grid(o, cell):
    """Corta as arestas mais longas que a célula (em grupos do mesmo número de cortes, com o preenchimento em grade do Blender: o
    retângulo vira uma grade de quadrados; a forma não muda)."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    S = o.matrix_world.to_scale()
    s = max(abs(S.x), abs(S.y), abs(S.z))
    # (03/11, o Felipe na frente do tanque: "esse polígono feio de terra/areia aí sem sentido nenhum") a face de mais de 4 lados (a chapa
    # chanfrada, o recorte) não ganha pontos dentro com o preenchimento em grade: a exportação a abria em leque, com triângulos de até
    # 7,8 m no casco do M1, e a fresta de um canto pintava o triângulo inteiro (a mancha reta em diagonal). Ela vira triângulos antes
    # (os de forma boa, BEAUTY), e as passadas abaixo cortam as arestas longas deles como as do resto
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    if ngons:
        bmesh.ops.triangulate(bm, faces=ngons, quad_method='BEAUTY', ngon_method='BEAUTY')
    # ao meio, em passadas, todas as arestas longas juntas (o preenchimento em grade só acontece quando os lados da face são
    # cortados na mesma chamada: em grupos separados a face grande virava um polígono sem pontos dentro, e a quina pintava a face toda)
    for _ in range(9):
        es = [e for e in bm.edges if e.calc_length() * s > cell]
        if not es:
            break
        bmesh.ops.subdivide_edges(bm, edges=es, cuts=1, use_grid_fill=True)
    bm.to_mesh(me)
    bm.free()
    me.update()


def _dirs(n):
    """A semiesfera em cosseno (espiral de Fibonacci), fixa: o resultado não depende de sorteio."""
    out = []
    ga = math.pi * (3.0 - math.sqrt(5.0))
    for i in range(n):
        u = (i + 0.5) / n
        r = math.sqrt(u)
        a = i * ga
        out.append((r * math.cos(a), r * math.sin(a), math.sqrt(max(0.0, 1.0 - u))))
    return out


def _smooth(x, a, b):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def bake_masks(objs, kind='vehicle', ground=True, extra=()):
    """As máscaras de todas as peças (objs), com a oclusão contra todas elas juntas (+ o chão e os objetos de 'extra', que só
    fazem sombra). Grava o atributo 'Col' (FLOAT_COLOR, por canto) e deixa ativo."""
    t0 = time.time()
    C = CLASS[kind]
    verts, polys = [], []
    for o in list(objs) + list(extra):
        M = o.matrix_world
        base = len(verts)
        verts += [M @ v.co for v in o.data.vertices]
        polys += [[base + i for i in p.vertices] for p in o.data.polygons]
    if ground:
        g = 400.0
        base = len(verts)
        verts += [Vector((-g, -g, 0.0)), Vector((g, -g, 0.0)), Vector((g, g, 0.0)), Vector((-g, g, 0.0))]
        polys.append([base, base + 1, base + 2, base + 3])
    bvh = BVHTree.FromPolygons(verts, polys, all_triangles=False, epsilon=0.0)
    D = _dirs(RAYS)
    for o in objs:
        me = o.data
        bm = bmesh.new()
        bm.from_mesh(me)
        bm.normal_update()
        bm.verts.ensure_lookup_table()
        convex = [0.0] * len(bm.verts)
        concave = [0.0] * len(bm.verts)
        for e in bm.edges:
            if len(e.link_faces) != 2:
                continue
            a = math.degrees(e.calc_face_angle_signed(0.0))
            for v in e.verts:
                if a > convex[v.index]:
                    convex[v.index] = a
                if -a > concave[v.index]:
                    concave[v.index] = -a
        M = o.matrix_world
        R = M.to_3x3().normalized()
        ao = [1.0] * len(bm.verts)
        cav = [0.0] * len(bm.verts)
        for v in bm.verts:
            n = (R @ v.normal).normalized()
            if n.length < 0.5:
                continue
            p = M @ v.co + n * 0.006
            # a base do giro (a semente pelo índice: o mesmo modelo dá sempre a mesma máscara)
            t = n.orthogonal().normalized()
            ang = (v.index * 2.399963) % (2.0 * math.pi)
            t = (t * math.cos(ang) + n.cross(t) * math.sin(ang)).normalized()
            b = n.cross(t)
            occ = 0.0
            near = 0
            for dx, dy, dz in D:
                d = t * dx + b * dy + n * dz
                hit = bvh.ray_cast(p, d, C['ao'])
                if hit[0] is not None:
                    occ += 1.0 - (hit[3] / C['ao']) ** 0.7
                    near += hit[3] < C['short']
            ao[v.index] = 1.0 - occ / len(D)
            cav[v.index] = near / len(D)
        ca = me.color_attributes.get('Col') or me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
        me.color_attributes.active_color = ca
        cols = []
        for poly in me.polygons:
            for li in poly.loop_indices:
                vi = me.loops[li].vertex_index
                edge = _smooth(convex[vi], 12.0, 32.0)
                crease = max(_smooth(concave[vi], 8.0, 30.0), _smooth(cav[vi], 0.15, 0.55))
                cols += [1.0 - edge, ao[vi], 1.0 - crease, 1.0]
        ca.data.foreach_set('color', cols)
        bm.free()
        me.update()
        print('MASKS', o.name, len(me.vertices), 'vértices, %.1f s' % (time.time() - t0))


def uv_meters(o, seed=0):
    """UV0 em metros (projeção pelo eixo dominante de cada face, no espaço da peça) e UV1 = a semente da ilha (cada pedaço solto do
    modelo, que é uma primitiva do script: uma chapa, uma caixa, um cano)."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv0 = bm.loops.layers.uv.new('UVMeters')
    uv1 = bm.loops.layers.uv.new('Seed')
    seen = set()
    island = 0
    for f0 in bm.faces:
        if f0.index in seen:
            continue
        island += 1
        h = ((island * 2654435761 + seed * 97531) % 4294967296) / 4294967296.0
        stack = [f0]
        seen.add(f0.index)
        while stack:
            f = stack.pop()
            n = f.normal
            ax = max(range(3), key=lambda k: abs(n[k]))
            for lp in f.loops:
                c = lp.vert.co
                u, v = ((c.y, c.z), (c.x, c.z), (c.x, c.y))[ax]
                lp[uv0].uv = (u, v)
                # o V fora de [0, 1] marca o modelo com máscara (o UV de lightmap que a Unreal gera nos antigos fica dentro): o material
                # só lê a semente onde vê a marca. O glTF vira o V (1 − v = −1 − h), por isso o material aceita os dois lados
                lp[uv1].uv = (h, 2.0 + h)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
    bm.to_mesh(me)
    bm.free()
    me.update()


def finish_detail(objs, kind='vehicle', ground=True, extra=(), seed=0):
    """O passo final de um modelo montado: a grade, o UV e as máscaras (nessa ordem: a grade antes das máscaras)."""
    before = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)
    for o in objs:
        grid(o, CLASS[kind]['cell'])
        uv_meters(o, seed)
    bake_masks(objs, kind, ground, extra)
    after = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)
    print('DETAIL triângulos %d -> %d' % (before, after))
