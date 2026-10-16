"""O toco e o tronco quebrado de cada palmeira (16/10, o pedido do Felipe pelo Look: o tanque derruba a palmeira; o
estudo em docs/WORLDFORGE.md §12.8). A palmeira leve (Intermediate/Assets/date_palm, BlastLab/Tools/blender_palm_lite.py)
é cortada na altura da quebra:
- SM_<palmeira>_Stump: o que fica de pé (o ZH: o toco, StumpName);
- SM_<palmeira>_Fallen: o resto, com o pivô (a origem) no centro da quebra; o jogo gira por ele na queda.

A 1ª versão (a tampa reta com dezenas de fibras em estrela) foi reprovada: "parece só um monte de espinhos esticados sem
nenhum embasamento ou sentido". A 2ª segue a quebra real (o estudo de 16/10: Hodel e Wood, a falha do tronco de palmeira;
a anatomia do coqueiro, González e Nguyen 2016; a FAO; o CryEngine, as árvores quebráveis). A palmeira não tem lenho: são
feixes de fibra duros soltos num miolo mole, mais densos na borda, correndo ao longo do eixo. Ela quebra RASGANDO:
- o corte é torto: um plano inclinado de 20–35°, alto no lado que rasgou (o tracionado), com ondulação larga de 3–6 cm;
  a borda dura 2–5 cm acima do miolo, o miolo afundado 1–3 cm;
- 2–4 lascas grandes de casca em pé, só num arco de 90–120° do lado tracionado, paralelas ao eixo (abrem no máximo
  10–15°): 15–40 cm de comprimento, 4–10 cm de largura, 1–3 cm de espessura, afinando até a ponta;
- um tufo de fibra mole caído do lado tracionado (no toco desce pela casca; no tronco caído sai da ponta, de vassoura);
- nada em raio, nada em estrela, nada mais fino que um pixel do jogo (a 80 m o tronco tem ~8 px).
A tampa, o lado de dentro das lascas e o tufo vão no material 2 (PalmBreak). A cor sai do UV 1: x = a distância ao centro
(0 no meio, 1 na borda: o anel marrom e os pontos dos feixes mais densos na borda), y = o que é (0 a tampa, 1 a lasca, 2 o
tufo). O lado de fora das lascas é a casca, com o UV da casca de verdade (o ponto mais perto do tronco original).
O toco e o tronco caído são cortados pela MESMA superfície (as peças encaixam).

  blender -b -P blender_palm_break.py -- <entrada.glb> <pasta de saída> [altura da quebra em m, 0,8]
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.interpolate import poly_3d_calc

argv = sys.argv[sys.argv.index('--') + 1:]
src, out_dir = argv[0], argv[1]
cut = float(argv[2]) if len(argv) > 2 else 0.8
name = os.path.splitext(os.path.basename(src))[0]
os.makedirs(out_dir, exist_ok=True)
SEED = sum(ord(ch) * (i + 1) for i, ch in enumerate(name))   # (o hash() do Python muda a cada execução)


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
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.50, 0.40, 0.24, 1.0)   # palha (a cor de verdade é do UE)
    m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.85
    while len(o.data.materials) < 2:
        o.data.materials.append(None)
    o.data.materials.append(m)
    return len(o.data.materials) - 1


def smooth(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def hsh(i, j, s):
    n = (i * 73856093) ^ (j * 19349663) ^ (s * 83492791)
    n &= 0xffffffff
    n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
    return ((n ^ (n >> 16)) & 0xffffff) / 16777216.0


def voronoi(x, y, cs, s):
    """O Voronoi de células de cs metros: F1, F2 (em células) e a altura sorteada da célula mais perto."""
    gx, gy = x / cs, y / cs
    ix, iy = math.floor(gx), math.floor(gy)
    f1 = f2 = 9.0
    h = 0.0
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            ci, cj = ix + di, iy + dj
            px = ci + hsh(ci, cj, s)
            py = cj + hsh(ci, cj, s + 1)
            d = math.hypot(gx - px, gy - py)
            if d < f1:
                f2 = f1
                f1 = d
                h = hsh(ci, cj, s + 2)
            elif d < f2:
                f2 = d
    return f1, f2, h


class Trunk:
    """Um tronco que passa pela altura da quebra: o centro e o raio ali, e a superfície da quebra dele."""

    def __init__(self, k, c, r):
        self.c, self.r = c, r
        rnd = random.Random(SEED * 31 + k * 7)
        self.dang = rnd.uniform(0.0, 2.0 * math.pi)                 # o lado tracionado (o que rasga)
        self.d = Vector((math.cos(self.dang), math.sin(self.dang), 0.0))
        self.tan = math.tan(math.radians(rnd.uniform(20.0, 35.0)))
        self.ph = [rnd.uniform(0.0, 2.0 * math.pi) for _ in range(4)]
        self.rnd = rnd

    def plane(self, x, y):
        return cut + self.tan * ((x - self.c.x) * self.d.x + (y - self.c.y) * self.d.y)

    def z(self, rr, a, jitter=0.0):
        """A altura da quebra no ponto polar (rr: 0 no centro, 1 na borda; a: o ângulo): o plano inclinado, a borda dura
        acima do miolo afundado, a ondulação larga e o miúdo da borda (só pelo ângulo: o anel e a tampa batem)."""
        x = self.c.x + math.cos(a) * self.r * rr
        y = self.c.y + math.sin(a) * self.r * rr
        rim = -0.02 + 0.05 * smooth(0.5, 0.95, rr)                  # o miolo −2 cm, a borda +3 cm
        broad = 0.035 * (0.6 * math.sin(2 * a + self.ph[0]) + 0.4 * math.sin(3 * a + self.ph[1])) * (0.3 + 0.7 * rr)
        fine = 0.006 * math.sin(23 * a + self.ph[2]) * math.sin(7 * a + self.ph[3]) * smooth(0.6, 1.0, rr)
        return self.plane(x, y) + rim + broad + fine + jitter


def islands(bm, ti):
    """Os troncos: os pedaços ligados de faces de tronco que passam pela altura do corte."""
    faces = [f for f in bm.faces if f.material_index == ti]
    left = set(faces)
    out = []
    while left:
        f0 = left.pop()
        comp, stack = [f0], [f0]
        while stack:
            f = stack.pop()
            for e in f.edges:
                for g in e.link_faces:
                    if g in left:
                        left.discard(g)
                        comp.append(g)
                        stack.append(g)
        zs = [v.co.z for f in comp for v in f.verts]
        if min(zs) < cut - 0.05 and max(zs) > cut + 0.4:
            out.append(comp)
    return out


def section(comp):
    """O centro e o raio do tronco na altura do corte (as arestas que cruzam z = corte)."""
    pts = []
    for f in comp:
        for e in f.edges:
            a, b = e.verts[0].co, e.verts[1].co
            if (a.z - cut) * (b.z - cut) < 0.0:
                t = (cut - a.z) / (b.z - a.z)
                pts.append(a.lerp(b, t))
    if len(pts) < 6:
        return None
    c = sum(pts, Vector()) / len(pts)
    c.z = cut
    r = sum((p - c).xy.length for p in pts) / len(pts)
    return c, r


def orient(f, want):
    """Vira a face pro lado pedido (a normal com o produto escalar positivo com want)."""
    f.normal_update()
    if f.normal.dot(want) < 0.0:
        f.normal_flip()


def split(keep_below):
    o = fresh()
    bi = break_material(o)
    ti = trunk_index(o)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    # o glTF separa os vértices na costura da textura: soldados, a borda do corte fecha (o UV fica nos cantos das faces)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.0005)
    # o UV 0 é o da casca (o material do tronco no UE lê o 0); o da quebra vai no último (a palmeira já traz 3: é o 3)
    uv0 = bm.loops.layers.uv[0] if len(bm.loops.layers.uv) else bm.loops.layers.uv.new('UVMap')
    uv1 = bm.loops.layers.uv.new('BreakUV')
    # a casca de verdade pro lado de fora das lascas: o tronco inteiro, antes do corte
    bark = bm.copy()
    bmesh.ops.delete(bark, geom=[f for f in bark.faces if f.material_index != ti], context='FACES')
    bark.faces.ensure_lookup_table()
    bark_uv = bark.loops.layers.uv[0]
    bvh = BVHTree.FromBMesh(bark)

    def bark_uv_at(p):
        loc, nrm, idx, dist = bvh.find_nearest(p)
        if idx is None:
            return (0.0, 0.0)
        f = bark.faces[idx]
        w = poly_3d_calc([l.vert.co for l in f.loops], loc)
        u = sum(wi * l[bark_uv].uv.x for wi, l in zip(w, f.loops))
        v = sum(wi * l[bark_uv].uv.y for wi, l in zip(w, f.loops))
        return (u, v)

    trunks = []
    for k, comp in enumerate(islands(bm, ti)):
        s = section(comp)
        if s:
            trunks.append((Trunk(k, s[0], s[1]), comp))
    # o resto (as folhas baixas do cacho) cortado reto na altura da quebra, como antes: são finas e ficam abertas
    trunk_faces = set(f for _, comp in trunks for f in comp)
    rest = [f for f in bm.faces if f not in trunk_faces]
    rest_geom = list({v for f in rest for v in f.verts}) + list({e for f in rest for e in f.edges}) + rest
    if rest_geom:
        bmesh.ops.bisect_plane(bm, geom=rest_geom, plane_co=(0, 0, cut), plane_no=(0, 0, 1), clear_outer=keep_below, clear_inner=not keep_below)
    sgn = 1.0 if keep_below else -1.0
    new_faces = []   # (face, a direção de fora, o tipo) pra virar no fim

    def face(vs, mat, want, uvs0=None, uvs1=None):
        try:
            f = bm.faces.new(vs)
        except ValueError:
            return None
        f.material_index = mat
        for i, l in enumerate(f.loops):
            if uvs0:
                l[uv0].uv = uvs0[i]
            if uvs1:
                l[uv1].uv = uvs1[i]
        new_faces.append((f, want))
        return f

    for T, comp in trunks:
        # o corte do tronco pelo plano inclinado (a ondulação vem depois, nos vértices do anel)
        verts = list({v for f in comp if f.is_valid for v in f.verts})
        edges = list({e for f in comp if f.is_valid for e in f.edges})
        geom = verts + edges + [f for f in comp if f.is_valid]
        no = Vector((-T.tan * T.d.x, -T.tan * T.d.y, 1.0)).normalized()
        r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(T.c.x, T.c.y, cut), plane_no=no, clear_outer=keep_below, clear_inner=not keep_below)
        ring = list({v for e in r['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_valid for v in e.verts})
        # o anel sobe e desce com a borda da quebra (a borda dura, a ondulação larga)
        for v in ring:
            a = math.atan2(v.co.y - T.c.y, v.co.x - T.c.x)
            v.co.z = T.z(1.0, a)
        # a tampa (16/10, a 2ª volta: o disco liso lia como tronco serrado): a fibra rasgada em tufos. A quebra de um feixe
        # de fibras é a ponta de uma corda desfiada: tufos de ~3,5 cm (as células de um Voronoi) arrancados em alturas
        # diferentes, com frestas entre eles; mais altos do lado tracionado (até ~9 cm), alguns buracos (o feixe que ficou
        # na outra peça) e o miúdo dos fios (células de 1,2 cm). Nada em raio. Cada peça tem os tufos dela (as fibras saem
        # das duas), o plano e a borda são os mesmos (encaixam). Polar denso (24 anéis × 160 raios, ~0,8 cm na borda)
        NR, NA = 24, 160
        pseed = SEED * 3 + (1 if keep_below else 2)

        def cap_z(rr, a):
            x = math.cos(a) * T.r * rr
            y = math.sin(a) * T.r * rr
            ten = smooth(-0.6, 1.0, (x * T.d.x + y * T.d.y) / T.r)            # 0 no lado comprimido, 1 no tracionado
            fade = 1.0 - smooth(0.88, 1.0, rr)                                   # na borda, a borda dura (o anel da casca)
            f1, f2, hc = voronoi(x, y, 0.04, pseed)
            crack = 1.0 - smooth(0.0, 0.12, f2 - f1)
            hc = hc * 1.4 - 0.4                                                  # −0,4..1: uns buracos, a maioria pra fora
            amp = 0.03 + 0.09 * ten
            h = (max(hc, 0.0) ** 1.6 * amp + min(hc, 0.0) * 0.035) * (1.0 - 0.8 * crack) - 0.012 * crack
            g1, g2, hf = voronoi(x, y, 0.012, pseed + 7)
            h += (hf - 0.5) * 0.012 * (1.0 - smooth(0.0, 0.2, g2 - g1) * 0.5)
            cap_z.crack = crack * fade
            return T.z(rr, a) + sgn * h * fade

        grid = []
        for i in range(NR + 1):
            rr = i / NR
            row = []
            for j in range(NA if i else 1):
                a = 2 * math.pi * j / NA
                rad = T.r * rr * (1.03 if i == NR else 1.0)
                cap_z.crack = 0.0
                z = T.z(1.0, a) if i == NR else cap_z(rr, a)
                row.append((bm.verts.new(Vector((T.c.x + math.cos(a) * rad, T.c.y + math.sin(a) * rad, z))), rr, cap_z.crack))
            grid.append(row)
        up = Vector((0, 0, sgn))
        for i in range(NR):
            for j in range(NA):
                j1 = (j + 1) % NA
                cells = [grid[0][0], grid[1][j], grid[1][j1]] if i == 0 else [grid[i][j], grid[i + 1][j], grid[i + 1][j1], grid[i][j1]]
                vs = [x[0] for x in cells]
                f = face(vs, bi, up, [((x[0].co.x - T.c.x), (x[0].co.y - T.c.y)) for x in cells], [(x[1], 0.4 * x[2]) for x in cells])
                if f:
                    f.smooth = True

        # as lascas da casca dura, em pé do lado tracionado (no tronco caído, 1–2 do lado de baixo, pra fora dele). Grossas
        # (1,5–3 cm: a casca e a camada de fibra de baixo), afinando só até ~55%, e a ponta em 2–3 dentes de alturas
        # diferentes (a casca rasga, não termina em agulha)
        def splinter(a0, L, w0, th0, lean, out_dir_sign):
            nseg, ncol = 6, 5
            rows = []
            curl = T.rnd.uniform(-0.2, 0.2)
            teeth = [T.rnd.uniform(0.0, 0.07) for _ in range(ncol)]
            teeth[T.rnd.randrange(1, ncol - 1)] += 0.03
            teeth[0] *= 0.3
            teeth[-1] *= 0.3
            base_z = T.z(1.0, a0) - out_dir_sign * 0.03   # enterrada 3 cm na borda (sem fresta)
            for si in range(nseg + 1):
                s = si / nseg
                w = w0 * (1.0 - 0.45 * s)
                th = th0 * (1.0 - 0.35 * s)
                da = (w / T.r) * 0.5
                ac = a0 + curl * s * s * (w0 / T.r)
                radial = T.r * 1.004 + math.sin(lean) * (s ** 1.5) * L
                z = base_z + out_dir_sign * s * L
                row = []
                for k3 in range(ncol):
                    u = k3 / (ncol - 1)
                    aa = ac - da + 2 * da * u
                    zz = z + (out_dir_sign * teeth[k3] * s ** 3 if si == nseg else out_dir_sign * teeth[k3] * s ** 3)
                    po = Vector((T.c.x + math.cos(aa) * radial, T.c.y + math.sin(aa) * radial, zz))
                    pin = Vector((T.c.x + math.cos(aa) * (radial - th), T.c.y + math.sin(aa) * (radial - th), zz))
                    row.append((bm.verts.new(po), bm.verts.new(pin), s, u))
                rows.append(row)
            out_v = Vector((math.cos(a0), math.sin(a0), 0.0))
            for si in range(nseg):
                for k3 in range(ncol - 1):
                    q = [rows[si][k3], rows[si + 1][k3], rows[si + 1][k3 + 1], rows[si][k3 + 1]]
                    face([x[0] for x in q], ti, out_v, [bark_uv_at(x[0].co) for x in q], [(1.0, 1.0)] * 4)
                    face([x[1] for x in reversed(q)], bi, -out_v, [(x[3] * w0, x[2] * L) for x in reversed(q)], [(1.0, 1.0)] * 4)
                for k3, sd in ((0, -1.0), (ncol - 1, 1.0)):
                    q = [rows[si][k3][0], rows[si + 1][k3][0], rows[si + 1][k3][1], rows[si][k3][1]]
                    aa = a0 + sd * (w0 / T.r) * 0.5
                    face(q, bi, Vector((-math.sin(aa), math.cos(aa), 0.0)) * sd,
                         [(0.0, rows[si][k3][2] * L), (0.0, rows[si + 1][k3][2] * L), (th0, rows[si + 1][k3][2] * L), (th0, rows[si][k3][2] * L)], [(1.0, 1.0)] * 4)
            # o topo rasgado (a espessura na ponta)
            for k3 in range(ncol - 1):
                A, B = rows[nseg][k3], rows[nseg][k3 + 1]
                face([A[0], B[0], B[1], A[1]], bi, Vector((0, 0, out_dir_sign)), [(A[3] * w0, 0.0), (B[3] * w0, 0.0), (B[3] * w0, th0), (A[3] * w0, th0)], [(1.0, 1.0)] * 4)

        rnd = T.rnd
        if keep_below:
            n = rnd.choice((2, 3, 3, 4))
            spread = math.radians(rnd.uniform(45.0, 60.0))   # o arco de 90–120°
            slots = []
            for _ in range(n * 6):
                x = rnd.uniform(-spread, spread)
                if all(abs(x - y) > 0.5 for y in slots):
                    slots.append(x)
                if len(slots) == n:
                    break
            for off in sorted(slots):
                big = 1.0 - abs(off) / spread                  # as maiores no meio do lado tracionado
                L = rnd.uniform(0.10, 0.16) + 0.14 * big * rnd.uniform(0.6, 1.0)
                splinter(T.dang + off, L, rnd.uniform(0.07, 0.13), rnd.uniform(0.015, 0.03), math.radians(rnd.uniform(3.0, 12.0)), 1.0)
        else:
            for k2 in range(rnd.choice((1, 2))):
                off = math.pi + rnd.uniform(-0.6, 0.6)
                splinter(T.dang + off, rnd.uniform(0.10, 0.22), rnd.uniform(0.05, 0.09), rnd.uniform(0.015, 0.025), math.radians(rnd.uniform(3.0, 10.0)), -1.0)

        # o tufo de fibra mole do lado tracionado: fios (prismas de 1–1,6 cm), não uma lâmina. No toco saem da borda,
        # abrem uns centímetros e caem pela casca; no tronco caído saem da ponta, de vassoura, abrindo um pouco
        a0 = T.dang + rnd.uniform(-0.45, 0.45)
        nstr = rnd.randint(16, 24)
        for k2 in range(nstr):
            aa0 = a0 + rnd.gauss(0.0, 0.5) * (0.035 / T.r)
            L = rnd.uniform(0.06, 0.18) if keep_below else rnd.uniform(0.05, 0.14)
            wd = rnd.uniform(0.005, 0.009)
            sway = rnd.uniform(-0.03, 0.03)
            nseg = 6
            pts = []
            for si in range(nseg + 1):
                s = si / nseg
                if keep_below:
                    radial = T.r * 1.01 + 0.03 * math.sin(min(s * 2.5, 1.0) * math.pi * 0.5) + 0.01 * s
                    z = T.z(1.0, aa0) + 0.01 - s * L
                else:
                    radial = T.r * (0.7 + 0.25 * rnd.random() if si == 0 else 0.0) if False else T.r * 0.85 + 0.05 * s
                    z = T.z(0.9, aa0) - s * L
                aa = aa0 + sway * s / T.r
                pts.append((Vector((T.c.x + math.cos(aa) * radial, T.c.y + math.sin(aa) * radial, z)), aa, s))
            tri_rows = []
            for (p, aa, s) in pts:
                rad_v = Vector((math.cos(aa), math.sin(aa), 0.0))
                tan_v = Vector((-math.sin(aa), math.cos(aa), 0.0))
                w = wd * (1.0 - 0.7 * s)
                tri_rows.append([bm.verts.new(p + rad_v * w * 0.6), bm.verts.new(p - rad_v * w * 0.3 + tan_v * w * 0.5), bm.verts.new(p - rad_v * w * 0.3 - tan_v * w * 0.5)])
            for si in range(nseg):
                for e in range(3):
                    e1 = (e + 1) % 3
                    q = [tri_rows[si][e], tri_rows[si + 1][e], tri_rows[si + 1][e1], tri_rows[si][e1]]
                    mid = sum((v.co for v in q), Vector()) / 4.0
                    axis_c = (pts[si][0] + pts[si + 1][0]) * 0.5
                    f = face(q, bi, mid - axis_c, [(e * wd, si / nseg * L), (e * wd, (si + 1) / nseg * L), (e1 * wd, (si + 1) / nseg * L), (e1 * wd, si / nseg * L)], [(0.0, 2.0)] * 4)
                    if f:
                        f.smooth = True
            tip = tri_rows[-1]
            face(tip, bi, Vector((0, 0, -1.0)), [(0, 0), (wd, 0), (0, wd)], [(0.0, 2.0)] * 3)

    for f, want in new_faces:
        if f.is_valid:
            orient(f, want)
    bm.to_mesh(o.data)
    bm.free()
    bark.free()
    o.data.update()
    if not keep_below:
        # o pivô no centro da quebra (o jogo gira o tronco caído por ele): o centro dos troncos, na altura do corte
        c = sum((T.c for T, _ in trunks), Vector()) / max(len(trunks), 1) if trunks else Vector((0, 0, cut))
        c.z = cut
        for v in o.data.vertices:
            v.co -= c
    tag = 'Stump' if keep_below else 'Fallen'
    o.name = '%s_%s' % (name, tag)
    path = os.path.join(out_dir, '%s_%s.glb' % (name, tag))
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=False)
    tri = sum(len(p.vertices) - 2 for p in o.data.polygons)
    zs = [v.co.z for v in o.data.vertices]
    print('PALMBREAK', tag, path, '%d triangulos' % tri, 'troncos %d' % len(trunks), 'z %.2f..%.2f' % (min(zs), max(zs)))


split(True)
split(False)
