"""O destrutor de veículos: os estados de dano de um veículo (01/11/2026; o estudo: docs/ESTUDO_DANO_VEICULOS.md).

O irmão do model_damage.py (o dos prédios), pro casco e a torre de qualquer veículo do elenco, sem código por veículo:

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_damage_vehicle.py -- \\
      captures/game/models/usa_mbt SM_LL_USATank [semente] [pasta_de_saída] [--render pasta_das_imagens]

Lê <pasta>/<PREFIXO>_Hull.glb (e _Turret.glb, se houver) e escreve NUMA PASTA SEPARADA (padrão: <pasta>_damage; nunca a do modelo):
- <PREFIXO>_Hull_Damaged.glb / _Turret_Damaged.glb (abaixo de 70% da vida, o UnitDamagedThreshold do ZH);
- <PREFIXO>_Hull_Wrecked.glb / _Turret_Wrecked.glb (abaixo de 35%, o UnitReallyDamagedThreshold);
- <PREFIXO>_damage.json: os pontos de fogo e fumaça de cada estado (metros da Unreal; os da torre no espaço da torre), o motor, as
  chapas que saíram (pro jogo jogar o pedaço no quadro da troca) e o que foi feito.
Com --render, a prova na CPU (Cycles): os 3 estados lado a lado na câmera do jogo, o quadro ampliado e de perto.

Nada é pintado por cima (14/10: "camuflagem pintada por cima, um negócio porco"). O dano é geometria, e a fuligem é um campo que sai
da causa (a grelha do motor, o furo, a caixa que estourou), escuro na causa e sumindo suave, nunca placas:
- a SAIA/CHAPA de fora que sai (rasgada, sobra a tira de cima com a borda torta) e a vizinha PENDURADA por um parafuso: a silhueta
  muda e a rodagem escura aparece (0,8 × 0,8 m: ~16 px na câmera do jogo);
- o FURO de penetração: o recorte de ruído com a espessura da chapa, a cavidade escura por trás e a fuligem do estouro em raios em
  volta (o furo real de 3–5 cm sumiria: aqui Ø 20–36 cm de propósito, como a ferragem dos prédios);
- o PARALAMA amassado na quina da frente (o campo de deslocamento, sem booleano);
- a ANTENA dobrada (danificado) e quebrada (destruído); a CAIXA de fora arrancada (com a marca do estouro onde ela estava);
- no destruído, o MOTOR que pega fogo (o jogo acende o fogo do motor aos 35%): a lona e a carga em cima queimam, a fuligem sai de
  cada grelha, a tinta do compartimento do motor escurece de calor e a pluma suja o que fica em cima (a traseira da torre).
Onde fica o motor: pelas grelhas (a área escura/metálica de cima numa das pontas); sem grelha, atrás no de esteira e na frente no
de rodas. <pasta>/<PREFIXO>_damage_hints.json (opcional) força: {"engine": "rear"|"front", "side": "left"|"right"}.

A QUEIMA vai no alfa da cor de vértice (o canal livre: o model_detail.py grava R = quina, G = oclusão, B = fresta e A = 1): A = 1 − queima
(branco = neutro, como as máscaras). O material liga a queima numa rampa só: tinta → tinta queimada (cinza-marrom fosco) → fuligem.
Os slots continuam os do modelo (o jogo importa sem tabela nova): a cavidade e a borda do furo no Dark, a borda da chapa no Metal.
A malha que não foi tocada fica idêntica (o mesmo vértice, normal, UV e máscaras): a troca no jogo só muda o que quebrou.
"""
import glob
import json
import math
import os
import random
import struct
import sys
from collections import Counter, defaultdict

import bmesh
import bpy
from mathutils import Matrix, Vector, noise
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import model_lib  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:]
RENDER = None
if '--render' in argv:
    i = argv.index('--render')
    RENDER = os.path.abspath(argv[i + 1])
    argv = argv[:i] + argv[i + 2:]
DIR, PREFIX = os.path.abspath(argv[0]), argv[1]
SEED = int(argv[2]) if len(argv) > 2 else 7
OUT = os.path.abspath(argv[3]) if len(argv) > 3 else DIR.rstrip('\\/') + '_damage'
if os.path.normcase(OUT) == os.path.normcase(DIR):
    raise SystemExit('a saída não pode ser a pasta do modelo (os GLB aprovados ficam intactos)')

Q = 0.0005          # a solda por posição das ilhas (o GLB parte os vértices nas quinas duras)
# os slots que o estado pode precisar e a peça não tem (as cores de conferência, lineares; o jogo usa as instâncias pelo nome)
NEED_MATS = {'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3), 'Metal': ((0.114, 0.114, 0.107), 0.45, 0.6)}
SKIP_PANEL = ('Team', 'Lens', 'Dark', 'Cloth', 'Track', 'Rubber', 'Glass', 'Light', 'Beacon')


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


# ------------------------------------------------------------------------------------------------ carregar
def glb_colors(path):
    """O baseColorFactor de cada material do GLB (o importador do glTF liga a cor de vértice na cor: a máscara tingia o material)."""
    b = open(path, 'rb').read()
    n = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + n])
    return {m.get('name', ''): tuple(m.get('pbrMetallicRoughness', {}).get('baseColorFactor', (1, 1, 1, 1)))
            for m in j.get('materials', [])}


def canon_mat(name, rgba=None, rough=None, metal=None):
    """O material pelo nome-base (Paint, não Paint.001), um só pra todas as peças: o GLB sai com o nome do slot que o jogo lê."""
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
    b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if b is not None:
        for lk in list(b.inputs['Base Color'].links):
            m.node_tree.links.remove(lk)
        if rgba is not None:
            b.inputs['Base Color'].default_value = rgba
            m.diffuse_color = rgba
        if rough is not None:
            b.inputs['Roughness'].default_value = rough
        if metal is not None:
            b.inputs['Metallic'].default_value = metal
    return m


def load_part(kind):
    path = os.path.join(DIR, f'{PREFIX}_{kind}.glb')
    if not os.path.exists(path):
        return None
    cols = glb_colors(path)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    objs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    model_lib.select_only(objs[0])
    for o in objs:
        o.select_set(True)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for e in [x for x in bpy.data.objects if x not in before and x.type != 'MESH']:
        bpy.data.objects.remove(e, do_unlink=True)
    me = o.data
    # os slots pelo nome-base, com a cor do GLB (a 2ª peça importada vem com Paint.001: volta pro Paint da 1ª)
    for i, m in enumerate(me.materials):
        base = m.name.split('.')[0]
        keep = bpy.data.materials.get(base)
        if keep is None or keep == m:
            m.name = base
            me.materials[i] = canon_mat(m.name, cols.get(base))
        else:
            me.materials[i] = canon_mat(base)
    for nm, (rgb, r, mt) in NEED_MATS.items():
        if nm not in [m.name for m in me.materials]:
            ex = bpy.data.materials.get(nm)
            me.materials.append(ex if ex else canon_mat(nm, (*rgb, 1.0), r, mt))
    # as máscaras do model_detail.py voltam com o nome que o model_lib exporta
    if not me.color_attributes:
        me.color_attributes.new('Col', 'BYTE_COLOR', 'CORNER').data.foreach_set('color', [1.0] * (4 * len(me.loops)))
    me.color_attributes[0].name = 'Col'
    me.color_attributes.active_color = me.color_attributes['Col']
    o.name = f'{PREFIX}_{kind}_src'
    return o


# ------------------------------------------------------------------------------------------------ análise
class Island:
    pass


def analyse(o):
    """As peças soltas (ilhas pela posição soldada) com a caixa, a área por material e a normal média; grava a ilha de cada face no
    atributo 'isl' (sobrevive à subdivisão: o pedaço continua sabendo de quem é)."""
    me = o.data
    mats = [m.name for m in me.materials]
    key, vk = {}, []
    for v in me.vertices:
        k = (round(v.co.x / Q), round(v.co.y / Q), round(v.co.z / Q))
        vk.append(key.setdefault(k, len(key)))
    par = list(range(len(key)))

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for p in me.polygons:
        r0 = find(vk[p.vertices[0]])
        for i in p.vertices[1:]:
            r = find(vk[i])
            if r != r0:
                par[r] = r0
    groups = defaultdict(list)
    for p in me.polygons:
        groups[find(vk[p.vertices[0]])].append(p.index)
    isls = []
    fid = [0] * len(me.polygons)
    for gi, fs in enumerate(sorted(groups.values(), key=len, reverse=True)):
        I = Island()
        I.id, I.faces = gi, fs
        vs = {i for f in fs for i in me.polygons[f].vertices}
        cs = [me.vertices[i].co for i in vs]
        I.lo = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
        I.hi = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
        I.size, I.c = I.hi - I.lo, (I.lo + I.hi) / 2
        I.area = Counter()
        for f in fs:
            p = me.polygons[f]
            I.area[mats[p.material_index]] += p.area
            fid[f] = gi
        I.A = sum(I.area.values())
        I.dom = max(I.area, key=I.area.get)
        isls.append(I)
    at = me.attributes.get('isl') or me.attributes.new('isl', 'INT', 'FACE')
    at.data.foreach_set('value', fid)
    return isls, mats


def attachments(isls, I, pad=0.07):
    """As peças pequenas presas na peça (as faixas da cor do jogador, os parafusos, as venezianas): a caixa delas dentro da caixa dela."""
    lo, hi = I.lo - Vector((pad, pad, pad)), I.hi + Vector((pad, pad, pad))
    out = {I.id}
    for J in isls:
        if J.id != I.id and J.A < I.A and all(lo[k] <= J.lo[k] and J.hi[k] <= hi[k] for k in range(3)):
            out.add(J.id)
    return out


def part_bounds(o):
    cs = [v.co for v in o.data.vertices]
    return (Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))),
            Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs))))


def side_panels(isls, lo, hi):
    """As chapas de fora dos lados (a saia do tanque, a porta, o paralama): finas no Y, de 0,35–2,2 m, na borda do casco."""
    Y = max(abs(lo.y), abs(hi.y))
    out = {1: [], -1: []}
    for I in isls:
        s = I.size
        if s.y > 0.25 or not (0.35 <= s.x <= 2.2) or not (0.3 <= s.z <= 1.6) or I.A < 0.6 or I.dom in SKIP_PANEL:
            continue
        if abs(I.c.y) < 0.8 * Y:
            continue
        out[1 if I.c.y > 0 else -1].append(I)
    for k in out:
        out[k].sort(key=lambda I: I.c.x)
    return out


def thin_masts(isls, top_from):
    """As antenas e os mastros finos: a pegada de menos de 7 cm e mais de 0,6 m de altura, acima do teto."""
    return [I for I in isls if max(I.size.x, I.size.y) < 0.07 and I.size.z > 0.6 and I.hi.z > top_from]


def outboard_boxes(isls, lo, hi):
    """As caixas de fora (a defesa ativa, as caixas de ferramenta, os galões): pequenas, cheias, na borda do lado."""
    Y = max(abs(lo.y), abs(hi.y))
    return [I for I in isls if all(0.12 <= I.size[k] <= 0.7 for k in range(3)) and 0.2 <= I.A <= 1.6
            and abs(I.c.y) > 0.55 * Y and I.dom not in SKIP_PANEL]


def stowage(isls, lo, hi):
    """A carga no cesto de trás (caixas, sacos): pequenas, perto do meio, no terço de trás da peça."""
    Y = max(abs(lo.y), abs(hi.y))
    L = hi.x - lo.x
    return [I for I in isls if all(0.1 <= I.size[k] <= 0.75 for k in range(3)) and I.A >= 0.08
            and abs(I.c.y) <= 0.55 * Y and I.c.x < lo.x + 0.3 * L and I.dom not in ('Lens', 'Glass')]


def engine_source(o, isls, mats, lo, hi, tracked, hint):
    """O motor: a ponta com mais área escura/metálica no alto do casco (as grelhas). Devolve a ponta, o centro do deck do motor e as
    grelhas (o centro e o tamanho de cada uma: de onde sai o fogo e a fuligem)."""
    L = hi.x - lo.x
    zt = lo.z + 0.6 * (hi.z - lo.z)
    score, acc = {1: 0.0, -1: 0.0}, {1: Vector(), -1: Vector()}
    for p in o.data.polygons:
        if mats[p.material_index] not in ('Dark', 'Metal') or p.center.z < zt:
            continue
        u = (p.center.x - lo.x) / L
        end = -1 if u < 0.33 else (1 if u > 0.67 else 0)
        if end:
            score[end] += p.area
            acc[end] += p.center * p.area
    if hint in ('rear', 'front'):
        end = -1 if hint == 'rear' else 1
    elif max(score.values()) > 0.2 and max(score.values()) > 1.3 * min(score.values()):
        end = max(score, key=score.get)
    else:
        end = -1 if tracked else 1
    x = (acc[end] / score[end]).x if score[end] > 0 else (lo.x + 0.15 * L if end < 0 else hi.x - 0.15 * L)
    bvh = BVHTree.FromPolygons([v.co for v in o.data.vertices], [p.vertices for p in o.data.polygons
                                                                  if mats[p.material_index] != 'Cloth'])
    h = bvh.ray_cast(Vector((x, 0.0, hi.z + 3.0)), Vector((0, 0, -1)), hi.z + 6.0)
    src = Vector((x, 0.0, h[0].z if h[0] is not None else hi.z))
    # as grelhas: as peças escuras/metálicas da ponta do motor, largas (não as cintas nem os frisos), no alto
    grilles = []
    for I in isls:
        u = (I.c.x - lo.x) / L
        if I.dom in ('Dark', 'Metal') and I.c.z > zt - 0.1 and (u < 0.36 if end < 0 else u > 0.64) \
                and min(I.size.x, I.size.y) > 0.25 and I.A > 0.15:
            grilles.append((I.c.copy(), 0.45 + 0.5 * max(I.size.x, I.size.y)))
    grilles.sort(key=lambda g: -g[1])
    if not grilles:
        grilles = [(src.copy(), 0.9)]
    return ('rear' if end < 0 else 'front'), src, grilles[:6]


def hole_sites(o, isl_ok, k, rnd, sep, test, avoid=()):
    """Os lugares dos furos: faces das peças grandes que passam no teste (o lado, a normal) e que veem fora (o raio pela normal e pela
    diagonal de cima não bate no próprio modelo: nada atrás da saia), sorteadas pela área com a distância mínima."""
    me = o.data
    mats = [m.name for m in me.materials]
    isl = me.attributes['isl'].data
    bvh = BVHTree.FromPolygons([v.co for v in me.vertices], [p.vertices for p in me.polygons])
    cand = [p for p in me.polygons if isl[p.index].value in isl_ok and p.area > 0.002
            and mats[p.material_index] not in ('Lens', 'Cloth', 'Track') and test(p.center, p.normal)]
    rnd.shuffle(cand)
    out = []
    for p in sorted(cand[:4000], key=lambda p: -p.area * rnd.uniform(0.3, 1.0)):
        if len(out) >= k:
            break
        c, n = p.center.copy(), p.normal.copy()
        if any((c - a).length < sep for a in list(avoid) + [q[0] for q in out]):
            continue
        up = (n + Vector((0, 0, 0.9))).normalized()
        if bvh.ray_cast(c + n * 0.01, n, 2.5)[0] is not None or bvh.ray_cast(c + n * 0.01, up, 2.5)[0] is not None:
            continue
        out.append((c, n))
    return out


# ------------------------------------------------------------------------------------------------ a queima (o alfa da cor de vértice)
class Halo:
    """A fuligem de um estouro: preta até rs, sumindo até rsc, em raios (o respingo não é redondo), só nas faces que olham pro estouro."""

    def __init__(self, c, n, rs, rsc, seed, streak=True, min_dot=-0.1):
        self.c, self.n, self.rs, self.rsc, self.seed, self.streak, self.min_dot = c.copy(), n.normalized(), rs, rsc, seed, streak, min_dot
        self.t = self.n.orthogonal().normalized()
        self.b = self.n.cross(self.t)
        self.R = rsc * 1.45

    def __call__(self, p, fn):
        d = p - self.c
        if d.length_squared > self.R * self.R or fn.dot(self.n) < self.min_dot:
            return 0.0
        a = math.atan2(d.dot(self.b), d.dot(self.t))
        ca, sa = math.cos(a), math.sin(a)
        k = 0.82 + 0.25 * noise.noise(Vector((ca * 1.6, sa * 1.6, self.seed)))
        if self.streak:
            k += 0.3 * max(0.0, noise.noise(Vector((ca * 6.0, sa * 6.0, self.seed + 5.0))))
        x = d.length / max(0.3, k)
        if x <= self.rs:
            return 1.0
        return max(0.0, 1.0 - (x - self.rs) / (self.rsc - self.rs)) ** 1.4


class EngineBurn:
    """O motor que queimou: a fuligem sai de cada grelha (preta perto, sumindo em ~2× o tamanho dela) e o calor escurece a tinta do
    deck do compartimento inteiro (até 0,32: a tinta queimada, não preta). Sem ilhas: tudo é um campo liso com o ruído só na borda."""

    def __init__(self, grilles, src, R, seed):
        self.g, self.src, self.R, self.seed = grilles, src.copy(), R, seed
        self.Rmax = R * 1.3 + 2.0 * max(s for _, s in grilles)

    def __call__(self, p, fn):
        d = p - self.src
        if d.length > self.Rmax or p.z < self.src.z - 0.55:
            return 0.0
        wob = 0.12 * noise.noise(p * 1.1 + Vector((self.seed, 0, 0))) + 0.05 * noise.noise(p * 3.7 + Vector((0, self.seed, 0)))
        b = 0.0
        for c, s in self.g:
            dd = (p - c)
            t = math.sqrt(dd.x * dd.x + dd.y * dd.y + (dd.z * 1.6) ** 2) / s + wob
            b = max(b, 1.0 - smooth(0.55, 1.9, t))      # preta até ~meia grelha além da borda, sumindo até ~2× o tamanho dela
        if fn.z > 0.3 or p.z > self.src.z - 0.1:
            e = math.sqrt(d.x * d.x + (d.y / 0.85) ** 2) / self.R + wob
            b = max(b, 0.32 * (1.0 - smooth(0.55, 1.0, e)))
        return b


class Plume:
    """A pluma do fogo do motor sobe das grelhas e suja o que fica em cima (a traseira e a barriga da torre): mais larga subindo, mais
    fraca longe, só nas faces que olham pra ela ou pra baixo."""

    def __init__(self, grilles, seed):
        self.g, self.seed = grilles, seed

    def __call__(self, p, fn):
        b = 0.0
        for c, s in self.g:
            dz = p.z - c.z
            if dz < -0.1 or dz > 2.2:
                continue
            h = Vector((p.x - c.x, p.y - c.y, 0.0))
            w = 0.35 * s + 0.45 * max(0.0, dz)
            if h.length > w * 1.4:
                continue
            if fn.dot(-h.normalized()) < 0.0 and fn.z > -0.3 and h.length > 0.05:
                continue
            t = h.length / w + 0.15 * noise.noise(p * 2.0 + Vector((self.seed, 0, 0)))
            b = max(b, max(0.0, 1.0 - t) ** 1.2 * math.exp(-max(0.0, dz) / 1.6))
        return min(1.0, b * 1.3)


# ------------------------------------------------------------------------------------------------ as operações na malha (bmesh)
class Ctx:
    """A cópia de um estado: a bmesh, as camadas, os slots e os campos de queima."""

    def __init__(self, o):
        self.o = o
        self.bm = bmesh.new()
        self.bm.from_mesh(o.data)
        self.isl = self.bm.faces.layers.int['isl']
        self.brn = self.bm.faces.layers.int.new('brn')        # a queima mínima da face (×1000: a borda da lona, a carga queimada)
        self.col = self.bm.loops.layers.color.get('Col')
        self.uvs = list(self.bm.loops.layers.uv.values())
        self.mi = {m.name: i for i, m in enumerate(o.data.materials)}
        self.fields = []

    def faces_of(self, ids):
        return [f for f in self.bm.faces if f[self.isl] in ids]

    def verts_of(self, ids):
        return list({v for f in self.faces_of(ids) for v in f.verts})

    def init_new(self, faces, mat, burn=0.0, ao=1.0, edge=1.0, h=None):
        """A face nova: o slot, a sombra lisa, o UV em metros e a semente (como o model_detail.py), as máscaras e a queima."""
        h = h if h is not None else random.random()
        for f in faces:
            f[self.isl] = -1
            f[self.brn] = int(burn * 1000)
            f.material_index = self.mi[mat]
            f.smooth = False
            f.normal_update()
            ax = max(range(3), key=lambda k: abs(f.normal[k]))
            for lp in f.loops:
                c = lp.vert.co
                if self.uvs:
                    lp[self.uvs[0]].uv = ((c.y, c.z), (c.x, c.z), (c.x, c.y))[ax]
                if len(self.uvs) > 1:
                    lp[self.uvs[1]].uv = (h, 2.0 + h)
                if self.col is not None:
                    lp[self.col] = (edge, ao, 1.0, 1.0)

    def refine(self, faces, cuts=2):
        es = list({e for f in faces for e in f.edges if e.calc_length() > 0.03})
        if es:
            bmesh.ops.subdivide_edges(self.bm, edges=es, cuts=cuts, use_grid_fill=True)

    def kill(self, faces):
        """Apaga as faces e devolve a borda nova do corte (as arestas que tinham duas faces e ficaram com uma)."""
        kill = set(faces)
        if not kill:
            return []
        border = {e for f in kill for e in f.edges if len(e.link_faces) == 2 and any(g not in kill for g in e.link_faces)}
        bmesh.ops.delete(self.bm, geom=list(kill), context='FACES')
        return [e for e in border if e.is_valid and len(e.link_faces) == 1]

    def rim(self, border, offset, mat, facing, burn):
        """A espessura da chapa no corte: a borda puxada pra dentro (offset(p) → deslocamento), virada pra quem olha (facing(centro))."""
        if not border:
            return []
        r = bmesh.ops.extrude_edge_only(self.bm, edges=border)
        vs = [g for g in r['geom'] if isinstance(g, bmesh.types.BMVert)]
        fs = [g for g in r['geom'] if isinstance(g, bmesh.types.BMFace)]
        for v in vs:
            v.co += offset(v.co)
        for f in fs:
            f.normal_update()
            if f.normal.dot(facing(f.calc_center_median())) < 0:
                f.normal_flip()
        self.init_new(fs, mat, burn=burn, ao=0.6, edge=0.35)
        return fs

    def add_mesh(self, verts, faces, mat, burn=0.0, ao=1.0):
        vv = [self.bm.verts.new(p) for p in verts]
        fs = []
        for f in faces:
            try:
                fs.append(self.bm.faces.new([vv[i] for i in f]))
            except ValueError:
                pass
        self.init_new(fs, mat, burn=burn, ao=ao)
        return fs

    def bake_burn(self):
        """A queima no alfa (A = 1 − queima) de cada canto: o maior dos campos e a mínima da face. Fora dos campos, A fica como veio."""
        if self.col is None:
            return
        for f in self.bm.faces:
            fmin = f[self.brn] / 1000.0
            fc = f.calc_center_median()
            act = [g for g in self.fields if (fc - getattr(g, 'c', getattr(g, 'src', fc))).length < getattr(g, 'R', getattr(g, 'Rmax', 9.9)) + 0.3]
            if not act and fmin <= 0.0:
                continue
            fn = f.normal
            for lp in f.loops:
                b = fmin
                for g in act:
                    b = max(b, g(lp.vert.co, fn))
                if b > 0.0:
                    c = lp[self.col]
                    lp[self.col] = (c[0], c[1], c[2], min(c[3], 1.0 - min(1.0, b)))

    def finish(self, name):
        bm = self.bm
        self.bake_burn()
        loose = [e for e in bm.edges if not e.link_faces]
        if loose:
            bmesh.ops.delete(bm, geom=loose, context='EDGES')
        lv = [v for v in bm.verts if not v.link_edges]
        if lv:
            bmesh.ops.delete(bm, geom=lv, context='VERTS')
        c = self.o.copy()
        c.data = self.o.data.copy()
        c.name = name
        bpy.context.scene.collection.objects.link(c)
        bm.to_mesh(c.data)
        bm.free()
        for a in ('isl', 'brn'):
            if c.data.attributes.get(a):
                c.data.attributes.remove(c.data.attributes[a])
        c.data.update()
        print('STATE', name, 'normais próprias' if c.data.attributes.get('custom_normal') else 'SEM as normais próprias',
              len(c.data.polygons), 'faces')
        return c


def rigid(ctx, ids, M):
    for v in ctx.verts_of(ids):
        v.co = M @ v.co


def hang_matrix(I, side, free_dir, angle, tilt):
    """A chapa pendurada por um parafuso: gira no plano dela em volta da quina de cima do lado que segura (a ponta solta desce) e a
    barra abre pra fora (a dobradiça na borda de cima)."""
    px = I.hi.x if free_dir < 0 else I.lo.x
    piv = Vector((px, I.c.y + side * I.size.y / 2, I.hi.z))
    th = math.radians(angle) * (1 if free_dir > 0 else -1)
    ph = math.radians(tilt) * side
    return Matrix.Translation(piv) @ Matrix.Rotation(ph, 4, 'X') @ Matrix.Rotation(th, 4, 'Y') @ Matrix.Translation(-piv)


def tear(ctx, ids, I, side, rnd, band=(0.12, 0.2)):
    """A chapa arrancada: sobra a tira de cima presa nos parafusos, com a borda rasgada (o ruído, nunca reta) e a espessura de aço."""
    seed = rnd.uniform(0, 100)
    b0 = rnd.uniform(*band)

    def zcut(x):
        return I.hi.z - b0 - 0.07 * noise.noise(Vector((x * 2.7, seed, 0.3))) - 0.03 * noise.noise(Vector((x * 9.0, seed, 1.7)))
    fs = ctx.faces_of(ids)
    ctx.refine([f for f in fs if abs(f.calc_center_median().z - zcut(f.calc_center_median().x)) < 0.16])
    kill = [f for f in ctx.faces_of(ids) if f.calc_center_median().z < zcut(f.calc_center_median().x)]
    border = ctx.kill(kill)
    out = Vector((0, side, 0))
    border = [e for e in border if e.link_faces[0].normal.dot(out) > 0.5]
    t = max(0.03, min(0.15, I.size.y * 0.8))
    ctx.rim(border, lambda p: -out * t, 'Metal', lambda c: Vector((0, 0, -1)) + out * 0.3, burn=0.0)


def dent(ctx, c, R, d, dirv, seed):
    """O amassado: o campo de deslocamento suave (sem booleano: vale pra qualquer malha), com ruído pra não ser uma bola."""
    for v in ctx.bm.verts:
        r = (v.co - c).length
        if r < R:
            t = 1.0 - r / R
            k = t * t * (3 - 2 * t) * (0.75 + 0.35 * noise.noise(v.co * 3.0 + Vector((seed, 0, 0))))
            v.co += dirv * d * k


def droop(ctx, I, frac, angle, az):
    """A antena dobrada: acima do ponto de quebra ela gira em volta do eixo horizontal (a curva em 6 cm, não um canto)."""
    zb = I.lo.z + frac * I.size.z
    axis = Vector((math.cos(az), math.sin(az), 0.0))
    piv = Vector((I.c.x, I.c.y, zb))
    for v in ctx.verts_of({I.id}):
        if v.co.z > zb:
            t = min(1.0, (v.co.z - zb) / 0.06)
            v.co = piv + Matrix.Rotation(math.radians(angle) * t, 3, axis) @ (v.co - piv)


def snap(ctx, I, frac):
    zb = I.lo.z + frac * I.size.z
    ctx.kill([f for f in ctx.faces_of({I.id}) if f.calc_center_median().z > zb])


def hole(ctx, c, n, r, rs, rsc, rnd):
    """O furo de penetração: o recorte de ruído (a borda rasgada), a chapa afundada em volta (o lábio), a espessura de aço queimado na
    borda, a cavidade escura por trás (o casco é casca: sem ela se via o chão através) e a fuligem do estouro em raios."""
    seed = rnd.uniform(0, 100)
    t = n.orthogonal().normalized()
    b = n.cross(t)

    def R(p):
        d = p - c
        a = math.atan2(d.dot(b), d.dot(t))
        return r * (1.0 + 0.32 * noise.noise(Vector((math.cos(a) * 1.8, math.sin(a) * 1.8, seed)))
                    + 0.12 * noise.noise(Vector((math.cos(a) * 5.0, math.sin(a) * 5.0, seed + 3.0))))

    def tang(p):
        d = p - c
        return (d - n * d.dot(n)).length

    ctx.refine([f for f in ctx.bm.faces if (f.calc_center_median() - c).length < 2.6 * r
                and abs((f.calc_center_median() - c).dot(n)) < 0.25])
    for v in ctx.bm.verts:          # o lábio: só pela posição (os vértices partidos nas quinas andam juntos)
        d = v.co - c
        dn = d.dot(n)
        tt = (d - n * dn).length
        if abs(dn) < 0.12 and tt < 2.4 * r:
            k = 1.0 - tt / (2.4 * r)
            v.co -= n * 0.045 * k * k
    border = ctx.kill([f for f in ctx.bm.faces if abs((f.calc_center_median() - c).dot(n)) < 0.14
                       and tang(f.calc_center_median()) < R(f.calc_center_median())])
    ctx.rim(border, lambda p: -n * 0.07 + (c - p - n * (c - p).dot(n)) * 0.12, 'Dark',
            lambda fc: (c - fc) - n * (c - fc).dot(n), burn=1.0)
    # a cavidade: a meia esfera de ruído atrás da chapa (a boca passa da borda do furo), as faces viradas pra dentro (vistas pelo furo)
    tmp = bmesh.new()
    bmesh.ops.create_icosphere(tmp, subdivisions=2, radius=1.0)
    cc = c - n * (0.55 * r + 0.02)
    rr = 1.6 * r
    vs = []
    for v in tmp.verts:
        d = v.co.normalized()
        vs.append(cc + d * rr * (1.0 + 0.2 * noise.noise(d * 2.0 + Vector((seed, 0, 0)))))
    fs = [[v.index for v in f.verts][::-1] for f in tmp.faces if all((vs[v.index] - c).dot(n) < -0.015 for v in f.verts)]
    tmp.free()
    ctx.add_mesh(vs, fs, 'Dark', burn=1.0, ao=0.1)
    ctx.fields.append(Halo(c, n, rs, rsc, seed))


def blow_off(ctx, ids, I, rnd):
    """A caixa arrancada: some, e o que estava embaixo fica com a marca do estouro."""
    ctx.kill(ctx.faces_of(ids))
    s = max(I.size.x, I.size.y)
    ctx.fields.append(Halo(Vector((I.c.x, I.c.y, I.lo.z)), Vector((0, 0, 1)), 0.4 * s, 0.95 * s, rnd.uniform(0, 100), min_dot=0.5))


def cloth_burn(ctx, src, Rb, seed):
    """A lona em cima do motor queima: some perto do fogo, sobra pedaço longe, com a borda carbonizada."""
    cloth = ctx.mi.get('Cloth', -1)
    kill = []
    for f in ctx.bm.faces:
        if f.material_index != cloth:
            continue
        fc = f.calc_center_median()
        bb = 1.15 - (fc - src).length / Rb + 0.35 * noise.noise(fc * 2.5 + Vector((seed, 0, 0)))
        if bb > 0.45:
            kill.append(f)
        elif bb > 0.1:
            f[ctx.brn] = int(1000 * min(1.0, (bb - 0.1) / 0.25))
    ctx.kill(kill)


def cargo_burn(ctx, cargo, isls, rnd):
    """A carga do cesto queimou: 60% some, o resto fica carbonizado; o cesto (os trilhos e postes em volta) fica com a tinta queimada."""
    if not cargo:
        return
    gone = {I.id for I in cargo if rnd.random() < 0.6}
    ctx.kill(ctx.faces_of(gone))
    rest = {I.id for I in cargo} - gone
    for f in ctx.faces_of(rest):
        f[ctx.brn] = 950
    lo = Vector((min(I.lo.x for I in cargo), min(I.lo.y for I in cargo), min(I.lo.z for I in cargo))) - Vector((0.2, 0.2, 0.2))
    hi = Vector((max(I.hi.x for I in cargo), max(I.hi.y for I in cargo), max(I.hi.z for I in cargo))) + Vector((0.2, 0.2, 0.3))
    rack = {I.id for I in isls if I.A < 1.0 and all(lo[k] <= I.c[k] <= hi[k] for k in range(3))} - gone - rest
    for f in ctx.faces_of(rack):
        f[ctx.brn] = 550


# ------------------------------------------------------------------------------------------------ o plano
def U(p):
    """Do Blender pra Unreal (m): o Y troca de sinal."""
    return [round(p.x, 2), round(-p.y, 2), round(p.z, 2)]


def main():
    os.makedirs(OUT, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    hints = {}
    hp = os.path.join(DIR, PREFIX + '_damage_hints.json')
    if os.path.exists(hp):
        hints = json.load(open(hp, encoding='utf-8'))
    tracked = bool(glob.glob(os.path.join(DIR, PREFIX + '_Track_*.glb')))
    rnd = random.Random(SEED * 7919 + len(PREFIX))
    hull = load_part('Hull')
    turret = load_part('Turret')
    if hull is None:
        raise SystemExit(f'sem {PREFIX}_Hull.glb em {DIR}')
    hisl, hmats = analyse(hull)
    hlo, hhi = part_bounds(hull)
    L, W = hhi.x - hlo.x, hhi.y - hlo.y
    Yh = max(abs(hlo.y), abs(hhi.y))
    end, src, grilles = engine_source(hull, hisl, hmats, hlo, hhi, tracked, hints.get('engine'))
    # o lado que leva os tiros (Blender: +Y é a esquerda da Unreal)
    S = {'left': 1, 'right': -1}.get(hints.get('side'), rnd.choice((1, -1)))
    report = {'prefix': PREFIX, 'seed': SEED, 'tracked': tracked, 'engine': end, 'engine_m': U(src),
              'grilles_m': [U(c) for c, _ in grilles], 'side_hit': 'left' if S > 0 else 'right',
              'size_m': [round(L, 2), round(W, 2), round(hhi.z, 2)]}
    print('VEHICLE', PREFIX, 'motor', end, U(src), 'grelhas', len(grilles), 'lado', report['side_hit'])

    # -------- o casco: as chapas do lado
    panels = side_panels(hisl, hlo, hhi)
    P, Qo = panels[S], panels[-S]
    print('PANELS', len(P), len(Qo))
    plan = {'D': [], 'W': []}
    torn_off = {'D': [], 'W': []}
    if len(P) >= 3:
        k = rnd.randrange(1, len(P) - 1)
        torn = P[k]
        hk = k + 1 if k + 1 < len(P) - 1 or k - 1 < 1 else k - 1
        hang = P[hk]
        free = -1 if hang.c.x > torn.c.x else 1          # a ponta solta vai pro lado do buraco
        plan['D'] += [('tear', torn), ('hang', hang, free, rnd.uniform(26, 36), rnd.uniform(12, 18))]
        torn_off['D'].append(torn)
        # o destruído: a pendurada caiu, mais uma rasgada longe
        far = [i for i in range(len(P)) if abs(i - k) >= 2 and i != hk]
        plan['W'] += [('tear', torn), ('gone', hang)]
        torn_off['W'] += [torn, hang]
        if far:
            k2 = rnd.choice(far)
            plan['W'].append(('tear', P[k2]))
            torn_off['W'].append(P[k2])
    elif P:
        plan['D'].append(('tear', P[0]))
        plan['W'].append(('gone', P[0]))
        torn_off['D'].append(P[0])
        torn_off['W'].append(P[0])
    if len(Qo) >= 3:
        j = rnd.randrange(1, len(Qo) - 1)
        plan['W'].append(('tear', Qo[j]))
        torn_off['W'].append(Qo[j])
        plan['W'].append(('hang', Qo[-1], 1, rnd.uniform(8, 14), rnd.uniform(18, 26)))   # a da frente torta pra fora (o paralama)
    # o amassado na quina da frente do lado do tiro (o paralama pra baixo e pra dentro)
    corner = Vector((hhi.x - 0.15, S * (Yh - 0.1), hhi.z - 0.1))
    ddir = Vector((-0.4, -S * 0.5, -0.8)).normalized()
    plan['D'].append(('dent', corner, 0.75, 0.08, ddir))
    plan['W'].append(('dent', corner, 0.85, 0.14, ddir))
    # os furos do casco: um no lado do tiro já no danificado (a saia que ficou, o flanco), um na frente no destruído
    moved = {op[1].id for op in plan['W'] if op[0] in ('tear', 'gone', 'hang')}
    big = {I.id for I in hisl if I.A > 1.0 and I.id not in moved and I.dom != 'Cloth'}
    side_h = hole_sites(hull, big, 1, rnd, 1.2, lambda c, n: -0.2 < n.z < 0.6 and c.y * S > 0.6 * Yh and abs(c.x) < 0.42 * L
                        and c.z > hlo.z + 0.25)
    front_h = hole_sites(hull, big, 1, rnd, 1.2, lambda c, n: n.x > 0.4 and n.z > -0.1 and c.z > hlo.z + 0.3,
                         avoid=[s[0] for s in side_h])
    hholes = {'D': [(c, n, 0.12, 0.40, 0.75) for c, n in side_h]}
    hholes['W'] = hholes['D'] + [(c, n, 0.10, 0.30, 0.55) for c, n in front_h]
    plan['D'] += [('hole',) + h for h in hholes['D']]
    plan['W'] += [('hole',) + h for h in hholes['W']]
    plan['W'] += [('cloth', src), ('engine', src)]

    # -------- a torre
    tplan = {'D': [], 'W': []}
    tholes = {'D': [], 'W': []}
    tisl = []
    if turret is not None:
        tisl, _ = analyse(turret)
        tlo, thi = part_bounds(turret)
        TL = thi.x - tlo.x
        Yt = max(abs(tlo.y), abs(thi.y))
        main_t = max(tisl, key=lambda I: I.A)
        masts = sorted(thin_masts(tisl, main_t.hi.z - 0.05), key=lambda I: -I.size.z)
        if masts:
            tplan['D'].append(('droop', masts[0], rnd.uniform(0.3, 0.5), rnd.uniform(70, 105), rnd.uniform(0, 6.28)))
            tplan['W'].append(('snap', masts[0], rnd.uniform(0.25, 0.4)))
            for I in masts[1:]:
                if rnd.random() < 0.5:
                    tplan['W'].append(('droop', I, rnd.uniform(0.3, 0.6), rnd.uniform(60, 100), rnd.uniform(0, 6.28)))
                else:
                    tplan['W'].append(('snap', I, rnd.uniform(0.2, 0.5)))
        boxes = outboard_boxes(tisl, tlo, thi)

        def on_top(I):
            return not any(J is not I and abs(J.c.x - I.c.x) < 0.15 and abs(J.c.y - I.c.y) < 0.15 and J.c.z > I.c.z for J in boxes)
        bs = sorted([I for I in boxes if I.c.y * S > 0], key=lambda I: (-round(I.c.z, 1), rnd.random()))
        bo = [I for I in boxes if I.c.y * S < 0 and on_top(I)]
        if bs:
            tplan['D'].append(('blow', bs[0]))
            tplan['W'].append(('blow', bs[0]))
            stack = [I for I in bs[1:] if abs(I.c.x - bs[0].c.x) < 0.15 and abs(I.c.y - bs[0].c.y) < 0.15]
            other = [I for I in bs[1:] if I not in stack and abs(I.c.x - bs[0].c.x) > 0.6 and on_top(I)]
            for I in stack[:1] + other[:1]:
                tplan['W'].append(('blow', I))
        if bo:
            tplan['W'].append(('blow', rnd.choice(bo)))
        blown = {op[1].id for op in tplan['W'] if op[0] == 'blow'}
        cargo = [I for I in stowage(tisl, tlo, thi) if I.id not in blown]
        tplan['W'].append(('cargo', cargo))
        tbig = {I.id for I in tisl if I.A > 0.15 * main_t.A}      # o corpo da torre (não a mira do comandante nem o mantelete)
        s1 = hole_sites(turret, tbig, 1, rnd, 1.0, lambda c, n: -0.25 < n.z < 0.6 and c.y * S > 0.35 * Yt
                        and c.x > tlo.x + 0.35 * TL and c.z > tlo.z + 0.12)
        s2 = hole_sites(turret, tbig, 1, rnd, 1.0, lambda c, n: -0.25 < n.z < 0.6 and c.y * S > 0.35 * Yt
                        and c.x > tlo.x + 0.3 * TL and c.z > tlo.z + 0.12, avoid=[s[0] for s in s1])
        s3 = hole_sites(turret, tbig, 1, rnd, 1.0, lambda c, n: n.x > 0.45 and -0.2 < n.z < 0.7 and c.z > tlo.z + 0.12
                        and abs(c.y) > 0.5, avoid=[s[0] for s in s1 + s2])
        tholes['D'] = [(c, n, 0.12, 0.40, 0.70) for c, n in s1]
        tholes['W'] = tholes['D'] + [(c, n, 0.17, 0.5, 0.85) for c, n in s2] + [(c, n, 0.09, 0.26, 0.45) for c, n in s3]
        tplan['D'] += [('hole',) + h for h in tholes['D']]
        tplan['W'] += [('hole',) + h for h in tholes['W']]
        tplan['W'].append(('plume', grilles))

    # -------- executar
    def run(o, isls, ops, name):
        ctx = Ctx(o)
        groups = {}

        def grp(I):
            if I.id not in groups:
                groups[I.id] = attachments(isls, I)
            return groups[I.id]
        order = ['hang', 'droop', 'dent', 'gone', 'blow', 'snap', 'tear', 'hole', 'cargo', 'cloth', 'engine', 'plume']
        for kind in order:
            for op in [x for x in ops if x[0] == kind]:
                if kind == 'hang':
                    I = op[1]
                    rigid(ctx, grp(I), hang_matrix(I, 1 if I.c.y > 0 else -1, op[2], op[3], op[4]))
                elif kind == 'droop':
                    droop(ctx, op[1], op[2], op[3], op[4])
                elif kind == 'dent':
                    dent(ctx, op[1], op[2], op[3], op[4], SEED)
                elif kind == 'gone':
                    ctx.kill(ctx.faces_of(grp(op[1])))
                elif kind == 'blow':
                    blow_off(ctx, grp(op[1]), op[1], rnd)
                elif kind == 'snap':
                    snap(ctx, op[1], op[2])
                elif kind == 'tear':
                    I = op[1]
                    tear(ctx, grp(I), I, 1 if I.c.y > 0 else -1, rnd)
                elif kind == 'hole':
                    hole(ctx, op[1], op[2], op[3], op[4], op[5], rnd)
                elif kind == 'cargo':
                    cargo_burn(ctx, op[1], isls, rnd)
                elif kind == 'cloth':
                    cloth_burn(ctx, op[1], 2.2, SEED + 0.5)
                elif kind == 'engine':
                    ctx.fields.append(EngineBurn(grilles, op[1], 0.42 * W, SEED + 0.25))
                elif kind == 'plume':
                    ctx.fields.append(Plume(op[1], SEED + 0.75))
        c = ctx.finish(name)
        return c, model_lib.export(c, OUT)

    made = {}
    for st, label in (('D', 'Damaged'), ('W', 'Wrecked')):
        made['hull_' + label], report[f'hull_{label.lower()}_tris'] = run(hull, hisl, plan[st], f'{PREFIX}_Hull_{label}')
        if turret is not None:
            made['turret_' + label], report[f'turret_{label.lower()}_tris'] = run(turret, tisl, tplan[st], f'{PREFIX}_Turret_{label}')

    # -------- os pontos pro jogo (metros da Unreal; o casco no espaço do ator, a torre no espaço da torre, que gira)
    def pts(hs):
        return [U(c + n * 0.1) for c, n, *_ in hs]
    vents = [U(c + Vector((0, 0, 0.15))) for c, _ in grilles]
    sz = lambda I: [round(I.size.x, 2), round(I.size.y, 2), round(I.size.z, 2)]   # noqa: E731
    report['damage_states'] = {
        'damaged': f'{PREFIX}_Hull_Damaged', 'wrecked': f'{PREFIX}_Hull_Wrecked',
        'turret_damaged': f'{PREFIX}_Turret_Damaged' if turret else '', 'turret_wrecked': f'{PREFIX}_Turret_Wrecked' if turret else '',
        # o ZH: abaixo dos 35% a fumaça num osso Smoke sorteado (o TransitionDamageFX); aqui o danificado solta o fio claro do furo e
        # o destruído põe o fogo nas grelhas (o motor) e nos furos
        'smoke_damaged_m': pts(hholes['D']), 'fire_damaged_m': [],
        'turret_smoke_damaged_m': pts(tholes['D']),
        'fire_wrecked_m': vents[:2] + pts(hholes['W']), 'smoke_wrecked_m': [U(src + Vector((0, 0, 0.4)))],
        'turret_fire_wrecked_m': pts(tholes['W']),
        # as chapas que saem na troca (o jogo joga um pedaço do tamanho delas, como os destroços do prédio)
        'torn_off_damaged': [{'at_m': U(I.c), 'size_m': sz(I)} for I in torn_off['D']],
        'torn_off_wrecked': [{'at_m': U(I.c), 'size_m': sz(I)} for I in torn_off['W'] if I not in torn_off['D']],
    }
    report['ops_damaged'] = [op[0] for op in plan['D'] + tplan['D']]
    report['ops_wrecked'] = [op[0] for op in plan['W'] + tplan['W']]
    with open(os.path.join(OUT, PREFIX + '_damage.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1, ensure_ascii=False)
    print('DAMAGE', json.dumps(report, ensure_ascii=False))
    if RENDER:
        render_proof(report['side_hit'] == 'left')


# ------------------------------------------------------------------------------------------------ a prova (Cycles na CPU)
REND = {'Paint': ((0.40, 0.405, 0.38), 0.55, 0.0), 'Team': ((0.05, 0.09, 0.24), 0.5, 0.0), 'Cloth': ((0.06, 0.10, 0.24), 0.85, 0.0),
        'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3), 'Metal': ((0.114, 0.114, 0.107), 0.45, 0.6), 'Lens': ((0.008, 0.01, 0.013), 0.06, 0.0),
        'Track': ((0.036, 0.034, 0.030), 0.85, 0.3), 'Rubber': ((0.012, 0.012, 0.012), 0.92, 0.0)}
HEAT = (0.13, 0.105, 0.085)      # a tinta queimada de calor (cinza-marrom fosco)
SOOT = (0.016, 0.014, 0.012)     # a fuligem


def render_mat(name):
    """O material de conferência pelo slot, com as máscaras como o material do jogo (aproximado, o model_preview.py com
    PREVIEW_MASKS=game: a oclusão escurece, o pó assenta na fresta) e a queima do alfa: tinta → tinta queimada → fuligem, a
    rugosidade subindo junto. É a rampa que o material do jogo precisa ganhar (docs/ESTUDO_DANO_VEICULOS.md)."""
    key = 'R_' + name
    m = bpy.data.materials.get(key)
    if m:
        return m
    rgb, rough, metal = REND.get(name, ((0.3, 0.3, 0.3), 0.6, 0.0))
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes['Principled BSDF']
    b.inputs['Metallic'].default_value = metal
    ca = nt.nodes.new('ShaderNodeVertexColor')
    ca.layer_name = 'Col'
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(ca.outputs['Color'], sep.inputs['Color'])

    def node(kind, op=None, **kw):
        n = nt.nodes.new(kind)
        if op:
            n.operation = op
        return n

    def mix(a, bb, fac, blend='MIX'):
        n = nt.nodes.new('ShaderNodeMix')
        n.data_type = 'RGBA'
        n.blend_type = blend
        for sock, v in (('A', a), ('B', bb), ('Factor', fac)):
            if isinstance(v, (tuple, float, int)):
                n.inputs[sock].default_value = (*v, 1.0) if isinstance(v, tuple) and len(v) == 3 else v
            else:
                nt.links.new(v, n.inputs[sock])
        return n.outputs['Result']

    inv = node('ShaderNodeMath', 'SUBTRACT')
    inv.inputs[0].default_value = 1.0
    nt.links.new(sep.outputs['Blue'], inv.inputs[1])
    dk = node('ShaderNodeMath', 'MULTIPLY')
    dk.inputs[1].default_value = 0.5
    nt.links.new(inv.outputs[0], dk.inputs[0])
    col = mix(rgb, (0.40, 0.31, 0.21), dk.outputs[0])
    g = nt.nodes.new('ShaderNodeCombineColor')
    for k in ('Red', 'Green', 'Blue'):
        nt.links.new(sep.outputs['Green'], g.inputs[k])
    col = mix(col, g.outputs['Color'], 0.35, 'MULTIPLY')
    # a queima: b = 1 − alfa
    burn = node('ShaderNodeMath', 'SUBTRACT')
    burn.inputs[0].default_value = 1.0
    nt.links.new(ca.outputs['Alpha'], burn.inputs[1])
    h1 = nt.nodes.new('ShaderNodeMapRange')
    h1.interpolation_type = 'SMOOTHSTEP'
    h1.inputs['From Min'].default_value, h1.inputs['From Max'].default_value = 0.03, 0.45
    nt.links.new(burn.outputs[0], h1.inputs['Value'])
    h2 = nt.nodes.new('ShaderNodeMapRange')
    h2.interpolation_type = 'SMOOTHSTEP'
    h2.inputs['From Min'].default_value, h2.inputs['From Max'].default_value = 0.42, 0.95
    nt.links.new(burn.outputs[0], h2.inputs['Value'])
    col = mix(col, HEAT, h1.outputs['Result'])
    col = mix(col, SOOT, h2.outputs['Result'])
    nt.links.new(col, b.inputs['Base Color'])
    r = nt.nodes.new('ShaderNodeMapRange')
    r.inputs['To Min'].default_value, r.inputs['To Max'].default_value = rough, 0.95
    nt.links.new(burn.outputs[0], r.inputs['Value'])
    nt.links.new(r.outputs['Result'], b.inputs['Roughness'])
    return m


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.type == 'MESH':
            for i, m in enumerate(o.data.materials):
                o.data.materials[i] = render_mat(m.name.split('.')[0])
            if o.data.color_attributes:
                o.data.color_attributes[0].name = 'Col'
    return new


def render_proof(left):
    import numpy as np
    os.makedirs(RENDER, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'                      # nunca a placa (é de todas as sessões)
    sc.cycles.samples = 96
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = 'AgX'
    sc.render.image_settings.file_format = 'JPEG'
    sc.render.image_settings.quality = 90
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes['Background']
    bg.inputs['Color'].default_value = (0.36, 0.48, 0.66, 1)
    bg.inputs['Strength'].default_value = 0.55
    bpy.ops.mesh.primitive_plane_add(size=800, location=(0, 0, 0))
    gm = bpy.data.materials.new('ground')
    gm.use_nodes = True
    gb = gm.node_tree.nodes['Principled BSDF']
    gb.inputs['Base Color'].default_value = (0.42, 0.33, 0.21, 1)
    gb.inputs['Roughness'].default_value = 0.95
    bpy.context.active_object.data.materials.append(gm)
    # as 3 versões: o intacto da pasta do modelo; o danificado e o destruído com o casco e a torre novos e o resto igual
    others = [os.path.basename(p)[:-4] for p in glob.glob(os.path.join(DIR, PREFIX + '_*.glb'))
              if not p.endswith(('_Hull.glb', '_Turret.glb'))]
    versions = [('intacto', [os.path.join(DIR, PREFIX + '_Hull.glb'), os.path.join(DIR, PREFIX + '_Turret.glb')]),
                ('danificado', [os.path.join(OUT, PREFIX + '_Hull_Damaged.glb'), os.path.join(OUT, PREFIX + '_Turret_Damaged.glb')]),
                ('destruido', [os.path.join(OUT, PREFIX + '_Hull_Wrecked.glb'), os.path.join(OUT, PREFIX + '_Turret_Wrecked.glb')])]
    yaw_view = math.radians(90.0)                   # a câmera olha pro +Y do Blender; a direita da imagem é o +X
    right = Vector((1.0, 0.0, 0.0))
    roots = []
    for label, files in versions:
        objs = []
        for p in files + [os.path.join(DIR, n + '.glb') for n in others]:
            if os.path.exists(p):
                objs += import_glb(p)
        e = bpy.data.objects.new('v_' + label, None)
        sc.collection.objects.link(e)
        for o in objs:
            if o.parent is None:
                o.parent = e
        # o lado do tiro virado pra câmera (que fica no −Y olhando pro +Y), com a frente a 35° pra ela (a 3/4 da frente)
        e.rotation_euler.z = math.radians(215.0) if left else math.radians(-35.0)
        roots.append((label, e))
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.2
    sun.data.color = (1.0, 0.80, 0.58)
    sun.data.angle = math.radians(1.5)
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam

    def place(spacing):
        for i, (label, e) in enumerate(roots):
            e.location = right * (i - 1) * spacing

    def aim(target, pitch, dist, hfov, res, sun_el=20.0, sun_from=(-0.95, -0.3)):
        d = Vector((0.0, math.cos(math.radians(pitch)), -math.sin(math.radians(pitch))))
        cam.location = target - d * dist
        cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        cam.data.sensor_fit = 'HORIZONTAL'
        cam.data.angle = math.radians(hfov)
        cam.data.clip_end = 2000
        # o sol rasante e quente vindo de sun_from (no chão: a câmera fica no −Y; −X é a esquerda da imagem)
        t = -Vector((sun_from[0], sun_from[1], 0.0)).normalized()
        sun.rotation_euler = (math.radians(90 - sun_el), 0, math.atan2(-t.x, t.y))
        sc.render.resolution_x, sc.render.resolution_y = res

    def shoot(path):
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print('RENDER', path)
        return path

    def load_px(path):
        img = bpy.data.images.load(path)
        w_, h_ = img.size
        a = np.array(img.pixels[:], dtype=np.float32).reshape(h_, w_, 4)
        bpy.data.images.remove(img)
        return a

    def save_px(a, path):
        o = bpy.data.images.new('px', a.shape[1], a.shape[0])
        o.pixels = a.ravel()
        o.filepath_raw = path
        o.file_format = 'JPEG'
        try:
            o.save(quality=90)          # os bytes como estão (o save_render passaria a AgX de novo)
        except TypeError:
            o.save()
        bpy.data.images.remove(o)
        print('RENDER', path)

    # 1) a câmera do jogo: 40° a 88 m; o campo de visão faz o casco de 7,9 m ter ~150 px em 1024 (34°); os 3 a 13 m
    place(13.0)
    aim(Vector((0, 0, 1.2)), 40.0, 88.0, 34.1, (1024, 576))
    game = shoot(os.path.join(RENDER, 'dano_jogo.jpg'))
    # 2) o mesmo quadro ampliado 3× no pixel (vizinho mais perto): o que o jogador vê, só maior
    a = load_px(game)
    h_, w_ = a.shape[:2]
    crop = a[int(h_ * 0.33):int(h_ * 0.67), int(w_ * 0.14):int(w_ * 0.88)]
    save_px(np.repeat(np.repeat(crop, 2, axis=0), 2, axis=1), os.path.join(RENDER, 'dano_jogo_ampliado.jpg'))
    # 3) de perto e de cima-perto: cada um no mesmo enquadramento, os 3 colados lado a lado
    for name, pitch, dist in (('dano_perto', 30.0, 15.5), ('dano_perto_cima', 60.0, 16.0)):
        tiles = []
        for i, (label, e) in enumerate(roots):
            place(60.0)
            aim(e.location + Vector((0, 0, 1.3)), pitch, dist, 38.0, (900, 640))
            tiles.append(load_px(shoot(os.path.join(RENDER, f'_{name}_{i}.jpg'))))
            os.remove(os.path.join(RENDER, f'_{name}_{i}.jpg'))
        save_px(np.concatenate(tiles, axis=1), os.path.join(RENDER, name + '.jpg'))


main()
