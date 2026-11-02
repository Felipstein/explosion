"""As peças comuns dos prédios provisórios por script no detalhe AAA (o elenco da China e da GLA, 31/10/2026).

O pedido do Felipe: "isso não é um jogo de lego, é um RTS AAA [...] TEXTURIZADO, DETALHADO [...] curvas, nada de quinas quadradas".
A régua (docs/ESTUDO_DETALHE_AAA.md §0): na câmera padrão 1 px = 3–5 cm (2–3 cm deitada); forma menor que 6–10 cm não existe.
Então o detalhe aqui é de FORMA (≥ 6 cm): a placa pré-moldada com o chanfro largo e a junta funda, a pingadeira com espessura,
o vão recuado na parede de verdade, a telha em onda, a quina gasta e redonda do adobe, a dobra da lona, a rede com volume. O resto
(a textura, o escorrido, o pó na fresta) é do material, lido das máscaras do model_detail.py.

E o prédio PRONTO PRA DEGRADAR (docs/ESTUDO_DEGRADACAO_PREDIOS.md): parede com espessura (≥ 0,25 m no concreto, ≥ 0,35 m no
adobe), laje com espessura, e atrás das paredes grandes o interior mínimo (o piso, as vigas, um pilar, o escuro): o furo mostra um
prédio, não um papel.

Uso (coordenadas da Unreal: X frente, Y direita, Z cima, metros; o model_lib.V troca o Y pro Blender):
    from model_building_parts import BPart, WallFace, cn_block, cn_wall, ...
    p = BPart('SM_X_Base')        # o Part de sempre, mais o corte das faces de apoio escondidas (ver BPart)

O que tem aqui:
- BPart: tira, antes do chanfro, as faces viradas pra baixo que sentam em outra superfície (o fundo de cada caixa no chão, no
  pódio, no teto). A grade do detalhe (model_detail.grid) cobre TODA face de ~25 tri/m²: o fundo escondido era ~40% do prédio.
- WallFace, wbox, rect_minus: a parede no plano dela (u ao longo, z, n pra fora; pode ser inclinada) e o recorte dos vãos sem
  booleano (a parede montada em volta do vão: o lado do vão mostra a espessura toda).
- China (concreto): cn_wall (o miolo + a pele de placas), cn_window, cn_rollup, cn_block (o bloco oco: as 4 paredes, a laje, a
  cornija com a pingadeira, o parapeito com a capa, os pilares de quina, o interior), cornice_ring, coping_ring, interior_frame.
- GLA (adobe e sucata): adobe_wall (o muro de topo redondo, a base mais larga, a quina gasta), adobe_block (o bloco oco com as
  vigas que atravessam), adobe_window, adobe_door, merlons (as ameias redondas), lintel.
- A vida na parede: pipe_run (o cano com os cotovelos redondos, as braçadeiras e o pé até a parede), cable (a catenária),
  ac_unit (o ar-condicionado no suporte), wall_lamp (a luminária com o corpo).
- Pano: tarp (a lona com as dobras que saem das cordas e as cordas), net_tufts (o volume da rede de camuflagem).
- bake_in_place: a peça que gira (origem no eixo) fica no lugar dela durante o cozimento das máscaras e volta pra origem na
  exportação (senão a sombra de contato era cozida com ela enfiada no meio do prédio).
"""
import math

import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import model_lib
from model_lib import Part, V

FACES6 = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
EPS = 0.002          # as peças vizinhas se sobrepõem 2 mm (o remove_doubles do Part.finish não solda as quinas)


def _f(v, z):
    return v(z) if callable(v) else v


def _hash(i, j=0, s=0):
    n = (int(i) * 374761393 + int(j) * 668265263 + int(s) * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


# ===================================================================================================== a peça

def cull_down(bms, reach=0.025, down_only=False):
    """Apaga as faces que encostam em outra superfície em todos os pontos de prova (os cantos puxados pro centro, os meios das
    arestas e o centro, cada um a ≤ reach ao longo da normal): o fundo que senta, o topo da parede debaixo da laje, a face colada
    em outra. Só nas ilhas fechadas (o pano e a rede, abertos, ficam como estão). Devolve quantas faces saíram."""
    # as ilhas de cada malha (e se são fechadas), depois da solda e das normais pra fora
    info = {}
    nisl = 0
    for key, b in bms.items():
        bmesh.ops.remove_doubles(b, verts=b.verts, dist=1e-5)
        bmesh.ops.recalc_face_normals(b, faces=b.faces)
        b.faces.ensure_lookup_table()
        b.faces.index_update()
        isl_of = [0] * len(b.faces)
        closed = {}
        seen = set()
        for f0 in b.faces:
            if f0.index in seen:
                continue
            nisl += 1
            stack, ok = [f0], True
            seen.add(f0.index)
            while stack:
                f = stack.pop()
                isl_of[f.index] = nisl
                for e in f.edges:
                    if len(e.link_faces) != 2:
                        ok = False
                    for g in e.link_faces:
                        if g.index not in seen:
                            seen.add(g.index)
                            stack.append(g)
            closed[nisl] = ok
        info[key] = (isl_of, closed)
    verts, polys, owner = [], [], []
    for key, b in bms.items():
        b.verts.index_update()
        base = len(verts)
        verts += [v.co.copy() for v in b.verts]
        polys += [[base + v.index for v in f.verts] for f in b.faces]
        owner += info[key][0]
    if not polys:
        return 0
    bvh = BVHTree.FromPolygons(verts, polys, all_triangles=False, epsilon=0.0)

    def covered(q, n, isl):
        o = q + n * 0.002
        hit = bvh.ray_cast(o, n, reach)
        if hit[0] is not None and owner[hit[2]] != isl:
            return True
        # o apoio coplanar (o fundo exatamente no plano do piso): a face de outra ilha a ≤ 4 mm, virada contra esta
        for (_, nn, idx, _) in bvh.find_nearest_range(o, 0.004):
            if owner[idx] != isl and nn.dot(n) < -0.9:
                return True
        return False
    gone = 0
    for key, b in bms.items():
        isl_of, closed = info[key]
        kill = []
        for f in b.faces:
            n = f.normal
            isl = isl_of[f.index]
            if not closed.get(isl) or (down_only and n.z > -0.95):
                continue
            c = f.calc_center_median()
            cs = [v.co for v in f.verts]
            pts = [c] + [c + (q - c) * 0.85 for q in cs] + [c + ((cs[i] + cs[(i + 1) % len(cs)]) / 2 - c) * 0.85 for i in range(len(cs))]
            if all(covered(q, n, isl) for q in pts):
                kill.append(f)
        if kill:
            bmesh.ops.delete(b, geom=kill, context='FACES')
            gone += len(kill)
    return gone


class BPart(Part):
    """O Part do model_lib com o corte dos fundos escondidos antes do chanfro (cull_down). O contato fica com a quina viva (a aresta de
    borda não chanfra), que é o certo: o que senta não pega luz na quina de baixo. Pro destrutor (o estudo da degradação), o
    buraco que fica é um polígono plano no plano de apoio: fecha com um preenchimento simples."""

    def __init__(self, name, cull=True):
        super().__init__(name)
        self.cull = cull

    def finish(self):
        if self.cull:
            n = cull_down(self.bms)
            print('CULL', self.name, n, 'fundos escondidos')
        return super().finish()


# ===================================================================================================== a parede no plano dela

class WallFace:
    """O plano de fora de uma parede alinhada aos eixos: face '+x', '-x', '+y' ou '-y', em 'at' (a coordenada da face na altura
    z0); lean = quanto a face recua por metro de subida (a parede inclinada da China; 0 = a prumo). Coordenadas da parede: u ao
    longo (o Y do mundo nas faces ±x, o X nas faces ±y), z a altura, n pra fora a partir do plano (positivo = pra fora)."""

    def __init__(self, face, at, z0=0.0, lean=0.0):
        self.face, self.at, self.z0, self.lean = face, at, z0, lean
        self.s = 1 if face[0] == '+' else -1
        self.ax = face[1]

    def pt(self, u, z, n=0.0):
        d = self.at + self.s * (n - self.lean * (z - self.z0))
        return (d, u, z) if self.ax == 'x' else (u, d, z)

    def normal(self):
        return (self.s, 0.0, 0.0) if self.ax == 'x' else (0.0, self.s, 0.0)

    def yaw(self):
        """O giro (graus, Unreal) de uma peça cujo X local aponta pra fora da parede."""
        return {'+x': 0.0, '-x': 180.0, '+y': 90.0, '-y': -90.0}[self.face]


def hexa(p, m, P, bevel=None):
    """Hexaedro de 8 pontos (Unreal) na ordem do Part.box (índice = z*4 + n*2 + u)."""
    b = p.bm(m, bevel if bevel is not None else 0.03)
    vs = [b.verts.new(V(*q)) for q in P]
    for f in FACES6:
        b.faces.new([vs[i] for i in f])


def wbox(p, m, F, u0, u1, za, zb, n0, n1, bevel=None):
    """A caixa no plano da parede F (u0..u1, za..zb, n0..n1); u e n podem ser funções da altura (a quina que recua, o miolo)."""
    P = []
    for z in (za, zb):
        for n in (n0, n1):
            for u in (u0, u1):
                P.append(F.pt(_f(u, z), z, _f(n, z)))
    hexa(p, m, P, bevel)


def rect_minus(u0, u1, z0, z1, holes, eps=EPS):
    """O retângulo menos os vãos (retângulos dentro dele), em tiras: [(a, b, c, d, toca_u0, toca_u1)...]. As tiras vizinhas se
    sobrepõem eps (eps < 0: afastam, a junta)."""
    hs = [(max(a, u0), min(b, u1), max(c, z0), min(d, z1)) for (a, b, c, d) in holes if a < u1 and b > u0 and c < z1 and d > z0]
    us = sorted(set([u0, u1] + [h[0] for h in hs] + [h[1] for h in hs]))
    out = []
    for a, b in zip(us[:-1], us[1:]):
        if b - a < 1e-4:
            continue
        mid = (a + b) / 2
        cuts = sorted((h[2], h[3]) for h in hs if h[0] < mid < h[1])
        lo, hi = a <= u0 + 1e-6, b >= u1 - 1e-6
        a2 = a if lo else a - eps
        b2 = b if hi else b + eps
        z = z0
        for c, d in cuts:
            if c > z + 1e-4:
                out.append((a2, b2, z, c, lo, hi))
            z = max(z, d)
        if z1 > z + 1e-4:
            out.append((a2, b2, z, z1, lo, hi))
    return out


def open_box(p, m, x0, x1, y0, y1, z0, z1, bevel=None):
    """A caixa sem o fundo (a laje do lote, o pódio, a saia que entra no chão): o fundo enterrado nunca aparece e, coberto pela grade
    do detalhe, custava milhares de triângulos."""
    b = p.bm(m, bevel if bevel is not None else 0.03)
    vs = [b.verts.new(V(x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    for f in FACES6[1:]:
        b.faces.new([vs[i] for i in f])


def splits(a, b, step):
    n = max(1, int(round((b - a) / step)))
    return [a + (b - a) * k / n for k in range(n + 1)]


# ===================================================================================================== varreduras

def sweep(p, m, pts, prof, up=(0.0, 0.0, 1.0), caps=(True, True), smooth=False):
    """Varre o perfil 2D prof = [(lado, cima)...] (fechado) pela polilinha pts: o 'cima' é o up tirado da tangente (na rampa do
    telhado, o cima vira a normal da telha)."""
    P = [Vector(q) for q in pts]
    U = Vector(up)
    rings = []
    for i in range(len(P)):
        t = (P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized()
        h = U - t * U.dot(t)
        if h.length < 1e-6:
            h = Vector((1.0, 0.0, 0.0)) - t * t.x
        h.normalize()
        s = t.cross(h).normalized()
        rings.append([tuple(P[i] + s * a + h * c) for (a, c) in prof])
    p.loft(m, rings, bevel=0.0, smooth=smooth, caps=caps)


def round_prof(w, h, base=-0.03, n=7):
    """O perfil de topo redondo (a telha-canal, a cumeeira, o topo do muro): a base reta em 'base', o arco de largura w e altura h."""
    pr = [(-w / 2, base)]
    for k in range(n):
        a = math.pi * k / (n - 1)
        pr.append((-w / 2 * math.cos(a), h * math.sin(a) ** 0.85 if 0 < k < n - 1 else 0.0))
    pr.append((w / 2, base))
    return pr


def tube(p, m, pts, r, seg=16, smooth=True, caps=(True, True)):
    """Tubo redondo pela polilinha, com o quadro transportado (sem torcer)."""
    P = [Vector(q) for q in pts]
    T = [(P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized() for i in range(len(P))]
    ref = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    n = T[0].cross(ref).normalized()
    rings = []
    for i in range(len(P)):
        if i > 0:
            n = T[i - 1].rotation_difference(T[i]) @ n
        b2 = T[i].cross(n).normalized()
        rings.append([tuple(P[i] + (n * math.cos(2 * math.pi * k / seg) + b2 * math.sin(2 * math.pi * k / seg)) * r)
                      for k in range(seg)])
    p.loft(m, rings, bevel=0.0, smooth=smooth, caps=caps)


def fillet(pts, rad, n=5):
    """A polilinha com as quinas arredondadas (o cotovelo: raio rad, n segmentos)."""
    P = [Vector(q) for q in pts]
    if len(P) < 3 or rad <= 0:
        return [tuple(q) for q in P]
    out = [P[0]]
    for i in range(1, len(P) - 1):
        a, b, c = P[i - 1], P[i], P[i + 1]
        d0, d1 = b - a, c - b
        L0, L1 = d0.length, d1.length
        d0.normalize()
        d1.normalize()
        ang = d0.angle(d1) if L0 > 1e-6 and L1 > 1e-6 else 0.0
        if ang < 1e-3:
            out.append(b)
            continue
        tl = min(rad * math.tan(ang / 2), L0 * 0.45, L1 * 0.45)
        p0, p1 = b - d0 * tl, b + d1 * tl
        for k in range(n + 1):
            s = k / n
            out.append(p0 * (1 - s) ** 2 + b * (2 * (1 - s) * s) + p1 * s * s)
    out.append(P[-1])
    return [tuple(q) for q in out]


def catenary(a, b, sag, n=12):
    A, B = Vector(a), Vector(b)
    return [tuple(A + (B - A) * (k / n) - Vector((0, 0, sag * 4 * (k / n) * (1 - k / n)))) for k in range(n + 1)]


# ===================================================================================================== China: o concreto

def cn_wall(p, F, u0, u1, z0, z1, t=0.4, holes=(), m='CnConcrete', panels=True, pw=2.6, ph=2.6, zs=None, groove=0.11,
            proud=0.12, embed=0.10, pm='CnConcrete#pnl', min_piece=0.32):
    """A parede de concreto da China com a espessura de verdade e a pele de placas pré-moldadas.
    - o miolo: de n = −t até a face (n = 0), montado em volta dos vãos (holes = [(u0, u1, z0, z1)...]); u0/u1 podem ser funções
      da altura (a quina que recua com a parede vizinha inclinada);
    - as placas (panels): chapas de 'proud' pra fora (e 'embed' enterradas no miolo), separadas pela junta de 'groove': o sulco
      mostra o miolo no fundo; cada placa leva o chanfro largo e redondo (o 'CnConcrete#pnl', 10 cm, 3 segmentos no detalhe).
      Em volta do vão a placa recua meia junta: o miolo aparece como o degrau da moldura. zs = as juntas horizontais (senão a
      cada ~ph); pw = a largura da placa."""
    if panels:
        cn_wall_skin(p, F, u0, u1, z0, z1, holes, pw=pw, ph=ph, zs=zs, groove=groove, proud=proud, pm=pm,
                     min_piece=min_piece, t=t)
        return
    for (a, b, c, d, lo, hi) in rect_minus(_f(u0, z0), _f(u1, z0), z0, z1, holes):
        wbox(p, m, F, u0 if lo else a, u1 if hi else b, c, d, -t, 0.0)


def cn_window(p, F, u0, u1, z0, z1, t=0.4, frame='CnMetal', glass='Glass', mull=None, sill=True, proud=0.10, lit=False):
    """A janela no vão da parede (recuada na espessura, não pintada na face): o caixilho de aço no meio da parede, o vidro escuro
    atrás, os montantes, e o peitoril de concreto saindo pra fora com a pingadeira (a sombra fina embaixo dele). lit: o vidro
    aceso (a luz de dentro)."""
    d = t * 0.55
    fw = 0.09
    fm = frame + '#win'
    wbox(p, fm, F, u0, u1, z0, z0 + fw, -d - 0.05, -d + 0.07, bevel=0.012)
    wbox(p, fm, F, u0, u1, z1 - fw, z1, -d - 0.05, -d + 0.07, bevel=0.02)
    wbox(p, fm, F, u0, u0 + fw, z0 + fw - 0.01, z1 - fw + 0.01, -d - 0.05, -d + 0.07, bevel=0.02)
    wbox(p, fm, F, u1 - fw, u1, z0 + fw - 0.01, z1 - fw + 0.01, -d - 0.05, -d + 0.07, bevel=0.02)
    wbox(p, 'Light' if lit else glass, F, u0 + 0.02, u1 - 0.02, z0 + 0.02, z1 - 0.02, -d - 0.03, -d - 0.01, bevel=0.0)
    n = mull if mull is not None else max(0, int((u1 - u0) / 0.9))
    for k in range(1, n + 1):
        u = u0 + (u1 - u0) * k / (n + 1)
        wbox(p, fm, F, u - 0.035, u + 0.035, z0 + fw - 0.01, z1 - fw + 0.01, -d - 0.03, -d + 0.04, bevel=0.02)
    if z1 - z0 > 1.3:
        zm = z0 + (z1 - z0) * 0.62
        wbox(p, fm, F, u0 + fw - 0.01, u1 - fw + 0.01, zm - 0.035, zm + 0.035, -d - 0.03, -d + 0.04, bevel=0.02)
    if sill:
        # o peitoril: entra no vão até o caixilho e sai além da placa; a ponta mais fina (a pingadeira)
        wbox(p, 'CnPlinth#sill', F, u0 - 0.1, u1 + 0.1, z0 - 0.12, z0 + 0.02, -d + 0.06, proud + 0.1, bevel=0.03)


def cn_rollup(p, F, u0, u1, z0, z1, t=0.4, open_frac=0.0, m='CnMetal', rails='CnMetal'):
    """A porta de enrolar recuada no vão: as guias dos lados, a caixa do rolo em cima e as lâminas (o ritmo das frestas, ≥ 7 cm)."""
    d = t * 0.45
    zt = z1 - (z1 - z0) * open_frac
    wbox(p, rails + '#door', F, u0, u0 + 0.12, z0, z1, -d - 0.08, -d + 0.06, bevel=0.012)
    wbox(p, rails + '#door', F, u1 - 0.12, u1, z0, z1, -d - 0.08, -d + 0.06, bevel=0.02)
    wbox(p, rails + '#door', F, u0, u1, z1 - 0.32, z1, -d - 0.12, -d + 0.08, bevel=0.03)
    if open_frac > 0.02:
        wbox(p, 'Dark', F, u0 + 0.1, u1 - 0.1, z0, zt, -t + 0.02, -t + 0.04, bevel=0.0)
    k = 0
    z = zt
    if open_frac < 0.98:
        # a cortina: uma chapa com as frestas escuras a cada 24 cm (o ritmo das lâminas; a lâmina chanfrada custava demais)
        wbox(p, m + '#slat', F, u0 + 0.1, u1 - 0.1, z0, zt, -d - 0.04, -d, bevel=0.0)
        while z > z0 + 0.2:
            z -= 0.24
            wbox(p, 'Dark', F, u0 + 0.1, u1 - 0.1, z - 0.015, z + 0.015, -d, -d + 0.012, bevel=0.0)
            k += 1


def cornice_ring(p, x0, x1, y0, y1, z, courses=((0.22, 0.2), (0.12, 0.14)), m='CnConcrete#corn', lean=None, z0=0.0):
    """A cornija (a pingadeira em degraus, com espessura) em volta de um bloco, de z pra baixo: cada curso (sai, altura). O curso de
    cima sai mais: a sombra dele é a linha que desenha o topo do prédio de cima. lean = {face: recuo por metro} (o bloco inclinado)."""
    lean = lean or {}
    zz = z
    for (o, h) in courses:
        za = zz - h
        dx0 = lean.get('-x', 0.0) * (zz - z0)
        dx1 = lean.get('+x', 0.0) * (zz - z0)
        dy0 = lean.get('-y', 0.0) * (zz - z0)
        dy1 = lean.get('+y', 0.0) * (zz - z0)
        X0, X1, Y0, Y1 = x0 + dx0 - o, x1 - dx1 + o, y0 + dy0 - o, y1 - dy1 + o
        ring_boxes(p, m, X0, X1, Y0, Y1, za, zz, o + 0.25)
        zz = za


def ring_boxes(p, m, X0, X1, Y0, Y1, za, zb, w, bevel=None, skip=()):
    """Um anel retangular de 4 caixas de largura w pra dentro (as quinas sem vértice repetido)."""
    if 0 not in skip:
        p.box(m, X0, X1, Y0, Y0 + w, za, zb, bevel=bevel)
    if 2 not in skip:
        p.box(m, X0, X1, Y1 - w, Y1, za, zb, bevel=bevel)
    if 3 not in skip:
        p.box(m, X0, X0 + w, Y0 + w - EPS, Y1 - w + EPS, za, zb, bevel=bevel)
    if 1 not in skip:
        p.box(m, X1 - w, X1, Y0 + w - EPS, Y1 - w + EPS, za, zb, bevel=bevel)


def coping_ring(p, X0, X1, Y0, Y1, z, h=0.45, t=0.3, m='CnConcrete', cap='CnPlinth#cap', over=0.07, skip=()):
    """O parapeito do teto (a mureta de t de espessura, h de altura, por dentro do contorno) com a capa que sai 'over' dos dois
    lados (a pingadeira: a sombra fina embaixo dela lê de cima)."""
    ring_boxes(p, m, X0, X1, Y0, Y1, z, z + h, t, skip=skip)
    ring_boxes(p, cap, X0 - over, X1 + over, Y0 - over, Y1 + over, z + h, z + h + 0.1, t + 2 * over, bevel=0.03, skip=skip)


def interior_frame(p, x0, x1, y0, y1, z0, z1, m='CnConcrete#int', col=0.5, beam=(0.32, 0.5), step=3.0, cols=None, along='x',
                   floor=None):
    """O interior mínimo atrás das paredes grandes (o furo do dano mostra um prédio: docs/ESTUDO_DEGRADACAO_PREDIOS.md §(a)5): as
    vigas debaixo da laje do teto a cada ~step (cruzando o lado curto), os pilares (cols = [(x, y)...]; None: no meio do vão
    quando ele passa de 7 m) e o piso. along: a direção em que as vigas se repetem."""
    bw, bh = beam
    if along == 'x':
        for x in splits(x0 + step * 0.5, x1 - step * 0.5, step):
            p.box(m, x - bw / 2, x + bw / 2, y0 + 0.01, y1 - 0.01, z1 - bh, z1 + 0.01, bevel=0.04)
    else:
        for y in splits(y0 + step * 0.5, y1 - step * 0.5, step):
            p.box(m, x0 + 0.01, x1 - 0.01, y - bw / 2, y + bw / 2, z1 - bh, z1 + 0.01, bevel=0.04)
    if cols is None:
        cols = []
        if min(x1 - x0, y1 - y0) > 7.0:
            cols = [((x0 + x1) / 2, (y0 + y1) / 2)]
    for (x, y) in cols:
        p.box(m, x - col / 2, x + col / 2, y - col / 2, y + col / 2, z0, z1 - bh + 0.01, bevel=0.04)
        p.box(m, x - col / 2 - 0.1, x + col / 2 + 0.1, y - col / 2 - 0.1, y + col / 2 + 0.1, z1 - bh - 0.25, z1 - bh + 0.01, bevel=0.04)
    if floor:
        p.box(floor, x0, x1, y0, y1, z0, z0 + 0.12, bevel=0.0)


def cn_block(p, x0, x1, y0, y1, z0, z1, t=0.4, lean=None, holes=None, skin=('+x', '-x', '+y', '-y'),
             walls=('+x', '-x', '+y', '-y'), roof_t=0.35, parapet=0.4, corners=True, pier=0.8, cornice=True,
             interior=True, cols=None, pw=2.6, ph=2.6, zs=None, m='CnConcrete', along=None, plinth=0.9, roof_tiles=True):
    """O bloco oco de concreto da China (o hangar, as alas, a caldeira): as paredes com espessura (t) e as placas por fora (skin),
    os vãos (holes = {'+x': [(u0, u1, z0, z1)...]}; u = Y nas faces ±x e X nas faces ±y), a laje do teto com espessura, a
    cornija em degraus com a pingadeira, o parapeito com a capa, os pilares de quina chanfrados (corners: cobrem o encontro das
    placas, os 'contrafortes' da ficha), o talude inclinado no pé das faces de placas (plinth = a altura; o rodapé monumental da
    ficha), o teto em lajes pré-moldadas com o chanfro e a junta (roof_tiles: o que a câmera do RTS mais vê) e o interior mínimo.
    lean = {face: recuo por metro} (a parede inclinada).
    Devolve o contorno do topo (X0, X1, Y0, Y1) já com o recuo."""
    lean = lean or {}
    holes = holes or {}
    h = z1 - z0
    L = {f: lean.get(f, 0.0) for f in ('+x', '-x', '+y', '-y')}
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    # a quina recua com as 2 faces que se encontram: o alcance u de cada parede na altura z
    def urange(f):
        if f[1] == 'x':
            return (lambda z: y0 + L['-y'] * (z - z0)), (lambda z: y1 - L['+y'] * (z - z0))
        return (lambda z: x0 + L['-x'] * (z - z0)), (lambda z: x1 - L['+x'] * (z - z0))
    zt = z1 - roof_t
    for f in walls:
        F = WallFace(f, at[f], z0, L[f])
        a, b = urange(f)
        if f[1] == 'x':                                   # as paredes ±x por dentro das ±y (as quinas sem vértice repetido)
            a0, b0 = a, b
            a = (lambda z, q=a0: q(z) + EPS)
            b = (lambda z, q=b0: q(z) - EPS)
        pan = f in skin
        uin = 0.0
        if pan and corners:
            uin = pier / 2 + 0.02
        if pan:
            # as placas param no pilar de quina (o pilar cobre a ponta da parede: atrás dele, o miolo)
            aa = (lambda z, q=a: q(z) + uin) if uin else a
            bb = (lambda z, q=b: q(z) - uin) if uin else b
            cn_wall_skin(p, F, aa, bb, z0, zt, holes.get(f, ()), pw=pw, ph=ph, zs=zs, t=t)
            if uin:
                cn_wall(p, F, a, (lambda z, q=a: q(z) + uin + 0.012), z0, zt, t=t, m=m + '#core', panels=False)
                cn_wall(p, F, (lambda z, q=b: q(z) - uin - 0.012), b, z0, zt, t=t, m=m + '#core', panels=False)
        else:
            cn_wall(p, F, a, b, z0, zt, t=t, holes=holes.get(f, ()), m=m, panels=False)
    # a laje do teto (com espessura), no contorno recuado
    X0, X1 = x0 + L['-x'] * (zt - z0), x1 - L['+x'] * (zt - z0)
    Y0, Y1 = y0 + L['-y'] * (zt - z0), y1 - L['+y'] * (zt - z0)
    TX0, TX1 = x0 + L['-x'] * h, x1 - L['+x'] * h
    TY0, TY1 = y0 + L['-y'] * h, y1 - L['+y'] * h
    hexa(p, m + '#slab', [(X0, Y0, zt), (X1, Y0, zt), (X0, Y1, zt), (X1, Y1, zt),
                          (TX0, TY0, z1), (TX1, TY0, z1), (TX0, TY1, z1), (TX1, TY1, z1)], bevel=0.03)
    # os pilares de quina (inclinam com as 2 faces), chanfrados
    if corners:
        o = 0.12
        for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            fx, fy = ('+x' if sx > 0 else '-x'), ('+y' if sy > 0 else '-y')
            if fx not in skin and fy not in skin:
                continue
            P = []
            for z in (z0, z1 + 0.02):
                cx = at[fx] - sx * L[fx] * (z - z0)
                cy = at[fy] - sy * L[fy] * (z - z0)
                for yy in (cy - sy * pier / 2, cy + sy * o):
                    for xx in (cx - sx * pier / 2, cx + sx * o):
                        P.append((xx, yy, z))
            hexa(p, m + '#pier', P, bevel=0.12)
    if cornice:
        cornice_ring(p, x0, x1, y0, y1, z1 + 0.02, courses=((0.26, 0.3),), lean=lean, z0=z0)
    if parapet:
        coping_ring(p, TX0 - 0.2, TX1 + 0.2, TY0 - 0.2, TY1 + 0.2, z1 + 0.02, h=parapet, t=0.32)
        if roof_tiles:
            roof_slabs(p, TX0 + 0.12, TX1 - 0.12, TY0 + 0.12, TY1 - 0.12, z1)
        else:
            p.box('CnPlinth#roof', TX0, TX1, TY0, TY1, z1 + 0.0, z1 + 0.04, bevel=0.0)
    # o talude no pé das faces de placas (sai 0,35 m embaixo e morre na placa em 'plinth'), interrompido nas portas
    if plinth:
        for f in skin:
            if f not in walls:
                continue
            F = WallFace(f, at[f], z0, L[f])
            a, b = urange(f)
            ua, ub = a(z0) - 0.45, b(z0) + 0.45
            cuts = sorted((h[0] - 0.25, h[1] + 0.25) for h in holes.get(f, ()) if h[2] < z0 + plinth)
            segs, u = [], ua
            for (c0, c1) in cuts:
                if c0 > u + 0.3:
                    segs.append((u, c0 - 0.02))
                u = max(u, c1 + 0.02)
            if ub > u + 0.3:
                segs.append((u, ub))
            for (sa, sb) in segs:
                wbox(p, m + '#tal', F, sa, sb, z0, z0 + plinth, -0.2, (lambda z: 0.12 + 0.36 * max(0.0, 1.0 - (z - z0) / plinth) + 0.005),
                     bevel=0.1)
    if interior:
        ix0, ix1 = x0 + L['-x'] * h + t, x1 - L['+x'] * h - t
        iy0, iy1 = y0 + L['-y'] * h + t, y1 - L['+y'] * h - t
        if ix1 - ix0 > 1.0 and iy1 - iy0 > 1.0:
            interior_frame(p, ix0, ix1, iy0, iy1, z0, zt, cols=cols, along=along or ('x' if (x1 - x0) >= (y1 - y0) else 'y'))
    return TX0, TX1, TY0, TY1


def roof_slabs(p, x0, x1, y0, y1, z, size=3.0, gap=0.06, t=0.09, m='CnPlinth#rslab'):
    """O teto em lajes pré-moldadas (o que a câmera do RTS mais vê): as placas de ~size com o chanfro e a junta entre elas, em
    cima da laje (a vista de cima lê o quadriculado com a luz no chanfro, como o pátio de lajes)."""
    xs = splits(x0, x1, size)
    ys = splits(y0, y1, size)
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            p.box(m, xs[i] + gap / 2, xs[i + 1] - gap / 2, ys[j] + gap / 2, ys[j + 1] - gap / 2, z - 0.01, z + t, bevel=0.06)


def cn_wall_skin(p, F, u0, u1, z0, z1, holes=(), pw=2.6, ph=2.6, zs=None, groove=0.11, proud=0.12, embed=None,
                 pm='CnConcrete#pnl', min_piece=0.32, t=0.4, fill='CnConcrete#core'):
    """A parede de placas pré-moldadas (a pele É a parede: cada placa tem a espessura inteira, de n = −t até 'proud' pra fora,
    com o chanfro largo e redondo dos 2 lados; a face de dentro das placas é a parede de dentro). Entre as placas, a junta de
    'groove': o fundo dela é o preenchimento recuado (de −t até a face, n = 0), só onde não há placa nem vão; em volta do vão a
    placa recua e o preenchimento aparece como o degrau da moldura. Assim não há face escondida grande (a parede de 4 faces
    custava o dobro na grade do detalhe). u0/u1 podem ser funções da altura (a quina que recua)."""
    g = groove / 2
    U0, U1 = _f(u0, z0), _f(u1, z0)
    us = splits(U0, U1, pw)
    zs = zs or splits(z0, z1, ph)
    zs = [z for z in zs if z0 - 1e-6 <= z <= z1 + 1e-6]
    if zs[0] > z0 + 1e-6:
        zs = [z0] + zs
    if zs[-1] < z1 - 1e-6:
        zs = zs + [z1]
    if len(zs) > 2 and zs[-1] - zs[-2] < min_piece:
        zs = zs[:-2] + [zs[-1]]
    hh = [(h[0] - g, h[1] + g, h[2] - g, h[3] + g) for h in holes]
    pieces = []
    for i in range(len(us) - 1):
        for j in range(len(zs) - 1):
            a = us[i] + (g if i > 0 else 0.0)
            b = us[i + 1] - (g if i < len(us) - 2 else 0.0)
            c = zs[j] + (g if j > 0 else 0.0)
            d = zs[j + 1] - (g if j < len(zs) - 2 else 0.0)
            for (a2, b2, c2, d2, lo, hi) in rect_minus(a, b, c, d, hh, eps=-g):
                if b2 - a2 < min_piece or d2 - c2 < min_piece:
                    continue
                pieces.append((a2, b2, c2, d2))
                ua = (lambda z, o=u0: _f(o, z)) if (i == 0 and a2 <= U0 + 1e-6 and callable(u0)) else a2
                ub = (lambda z, o=u1: _f(o, z)) if (i == len(us) - 2 and b2 >= U1 - 1e-6 and callable(u1)) else b2
                wbox(p, pm, F, ua, ub, c2, d2, -t, proud, bevel=0.10)
    # o fundo das juntas: o complemento das placas e dos vãos, em células da grade das bordas (juntadas por fileira)
    eu = sorted(set([U0, U1] + [q for pc in pieces for q in pc[:2]] + [q for h in holes for q in h[:2]]))
    ez = sorted(set([z0, z1] + [q for pc in pieces for q in pc[2:]] + [q for h in holes for q in h[2:]]))
    eu = [u for u in eu if U0 - 1e-6 <= u <= U1 + 1e-6]
    ez = [z for z in ez if z0 - 1e-6 <= z <= z1 + 1e-6]

    def inside(u, z, rects):
        return any(r[0] - 1e-6 <= u <= r[1] + 1e-6 and r[2] - 1e-6 <= z <= r[3] + 1e-6 for r in rects)

    def emit(run, za, zb):
        ra = run[0] - (0.012 if run[0] > U0 + 1e-6 else 0.0)
        rb = run[1] + (0.012 if run[1] < U1 - 1e-6 else 0.0)
        rua = (lambda z, o=u0: _f(o, z)) if (run[0] <= U0 + 1e-6 and callable(u0)) else ra
        rub = (lambda z, o=u1: _f(o, z)) if (run[1] >= U1 - 1e-6 and callable(u1)) else rb
        wbox(p, fill, F, rua, rub, za - 0.012 if za > z0 + 1e-6 else za, zb + 0.012 if zb < z1 - 1e-6 else zb,
             -t + 0.012, 0.0, bevel=0.0)
    for zi in range(len(ez) - 1):
        za, zb = ez[zi], ez[zi + 1]
        if zb - za < 1e-4:
            continue
        zm = (za + zb) / 2
        run = None
        for ui in range(len(eu) - 1):
            ua, ub = eu[ui], eu[ui + 1]
            if ub - ua < 1e-4:
                continue
            um = (ua + ub) / 2
            if not inside(um, zm, pieces) and not inside(um, zm, holes):
                run = (run[0], ub) if run else (ua, ub)
            elif run:
                emit(run, za, zb)
                run = None
        if run:
            emit(run, za, zb)


def buttress(p, F, u, z0, h, w=0.75, d=0.6, top=0.5, m='CnConcrete#pier'):
    """O contraforte chanfrado encostado na parede F: a lâmina de d de fundo, o topo cortado em rampa (top = a altura da rampa)."""
    # o pé reto até h − top e a rampa: dois hexaedros
    zm = z0 + h - top
    P1, P2 = [], []
    for z in (z0, zm):
        for n in (-0.1, d):
            for uu in (u - w / 2, u + w / 2):
                P1.append(F.pt(uu, z, n))
    for (z, nn) in ((zm - 0.01, d), (z0 + h, 0.06)):
        for n in (-0.1, nn):
            for uu in (u - w / 2, u + w / 2):
                P2.append(F.pt(uu, z, n))
    hexa(p, m, P1, bevel=0.12)
    hexa(p, m, P2, bevel=0.12)


# ===================================================================================================== a vida na parede

def wall_lamp(p, F, u, z, m='Light', body='Dark#lamp', arm=0.22):
    """A luminária de parede com o corpo (os ~0,45 m que leem como objeto, não como ponto): a base na parede, o braço, a cúpula
    inclinada e a lente acesa virada pra baixo e pra fora (a luz cai no reboco embaixo dela)."""
    wbox(p, body, F, u - 0.13, u + 0.13, z - 0.2, z + 0.14, -0.02, 0.05, bevel=0.012)
    wbox(p, body, F, u - 0.035, u + 0.035, z + 0.02, z + 0.08, 0.04, arm, bevel=0.01)
    # a cúpula: mais larga embaixo, inclinada pra fora
    P = []
    for (z_, n0, n1, w) in ((z - 0.07, arm - 0.2, arm + 0.12, 0.2), (z + 0.1, arm - 0.12, arm + 0.02, 0.11)):
        for n in (n0, n1):
            for uu in (u - w, u + w):
                P.append(F.pt(uu, z_, n))
    hexa(p, body, P, bevel=0.02)
    wbox(p, m, F, u - 0.15, u + 0.15, z - 0.095, z - 0.065, arm - 0.17, arm + 0.09, bevel=0.0)


def pipe_run(p, pts, r, m='CnMetal#pipe', wall=None, step=1.8, clamp='Dark#clamp', bend=None, flanges=True, seg=16, feet=None):
    """O cano assentado: o tubo pela polilinha com os cotovelos redondos (raio bend, padrão 2,5 r), as flanges nas pontas e nas
    emendas, e as braçadeiras a cada 'step' com a mão-francesa até a parede (wall = (x, y) a normal da parede apontando PRO cano;
    o pé vai no sentido contrário até encostar) ou os pés até o chão (feet = a altura do chão)."""
    P = fillet(pts, bend if bend is not None else 2.5 * r, n=6)
    tube(p, m, P, r, seg=seg)
    if flanges:
        for (a, b) in ((pts[0], pts[1]), (pts[-1], pts[-2])):
            A, B = Vector(a), Vector(b)
            d = (B - A).normalized()
            c = A + d * 0.05
            p.cyl(m, tuple(c), tuple(d), r * 1.35, 0.07, seg=seg, bevel=0.0)
    for (a, b) in zip(pts[:-1], pts[1:]):
        A, B = Vector(a), Vector(b)
        Lr = (B - A).length
        if Lr < 0.6:
            continue
        d = (B - A).normalized()
        n = max(1, int(Lr / step))
        for k in range(n):
            q = A + d * (Lr * (k + 0.5) / n)
            p.cyl(clamp, tuple(q), tuple(d), r + 0.025, 0.07, seg=12, bevel=0.0)
            if wall is not None:
                w = Vector((-wall[0], -wall[1], 0.0))
                # a mão-francesa: da braçadeira até a parede (o comprimento: até achar a parede na distância do cano)
                L = wall[2] if len(wall) > 2 else 0.25
                c = q + w * (r + L / 2)
                p.cyl(clamp, tuple(c), tuple(w), 0.03, L, seg=6, bevel=0.0)
                e = q + w * (r + L)
                p.obox(clamp, tuple(e), (0.03, 0.2, 0.2), yaw=math.degrees(math.atan2(w.y, w.x)), bevel=0.0)    # a chapa na parede
            elif feet is not None and q.z - feet > 0.15:
                p.cyl(clamp, (q.x, q.y, (q.z - r + feet) / 2), 'z', 0.035, q.z - r - feet, seg=6, bevel=0.0)
                p.box(clamp, q.x - 0.12, q.x + 0.12, q.y - 0.12, q.y + 0.12, feet, feet + 0.03, bevel=0.0)


def cable(p, a, b, sag=0.25, r=0.02, m='Dark#cable', n=12):
    """O fio (a catenária, 8 lados): de a até b com a barriga."""
    tube(p, m, catenary(a, b, sag, n), r, seg=8)


def ac_unit(p, F, u, z, w=0.95, h=0.62, d=0.5, m='Metal#ac', bracket='Trim#ac'):
    """O ar-condicionado de janela na parede (a vida da GLA): a caixa com o chanfro, a grade do ventilador redonda (o anel, o
    escuro, a cruz), as aletas do lado, as 2 mãos-francesas em L por baixo, o dreno e o cabo que entra na parede."""
    n0, n1 = 0.02, 0.02 + d
    wbox(p, m, F, u - w / 2, u + w / 2, z, z + h, n0, n1, bevel=0.03)
    # a grade do ventilador na frente (a face de baixo-fora: o ventilador olha pra fora)
    c = F.pt(u - w * 0.12, z + h * 0.5, n1 + 0.005)
    nrm = F.normal()
    rr = h * 0.36
    p.cyl('Dark', c, nrm, rr, 0.03, seg=20, bevel=0.0)
    p.torus(bracket.split('#')[0] + '#acring', F.pt(u - w * 0.12, z + h * 0.5, n1 + 0.02), F.ax, rr, 0.02, seg=20, rseg=6)
    for ang in (0, 90):
        if F.ax == 'x':
            p.obox(bracket.split('#')[0] + '#acring', F.pt(u - w * 0.12, z + h * 0.5, n1 + 0.02), (0.02, rr * 2, 0.025), roll=ang, bevel=0.0)
        else:
            p.obox(bracket.split('#')[0] + '#acring', F.pt(u - w * 0.12, z + h * 0.5, n1 + 0.02), (rr * 2, 0.02, 0.025), pitch=ang, bevel=0.0)
    for k in range(4):                                                      # as aletas da direita
        zz = z + 0.12 + k * (h - 0.24) / 3
        wbox(p, 'Dark', F, u + w * 0.22, u + w * 0.45, zz - 0.02, zz + 0.02, n1, n1 + 0.012, bevel=0.0)
    for s in (-1, 1):                                                       # as mãos-francesas em L
        uu = u + s * w * 0.36
        wbox(p, bracket, F, uu - 0.025, uu + 0.025, z - 0.05, z, -0.02, n1 + 0.04, bevel=0.0)
        wbox(p, bracket, F, uu - 0.025, uu + 0.025, z - 0.45, z - 0.05, -0.02, 0.04, bevel=0.0)
        a = Vector(F.pt(uu, z - 0.42, 0.03))
        b = Vector(F.pt(uu, z - 0.03, n1 - 0.02))
        p.cyl(bracket, tuple((a + b) / 2), tuple(b - a), 0.022, (b - a).length, seg=6, bevel=0.0)
    # o dreno pingando e o cabo que entra na parede
    tube(p, 'Dark#cable', [F.pt(u + w * 0.4, z + 0.02, n1 - 0.1), F.pt(u + w * 0.4, z - 0.3, n1 - 0.1), F.pt(u + w * 0.42, z - 0.55, n1 + 0.02)], 0.018, seg=6)
    tube(p, 'Dark#cable', fillet([F.pt(u - w * 0.5 + 0.05, z + h - 0.1, 0.3), F.pt(u - w * 0.5 - 0.25, z + h - 0.1, 0.3),
                                  F.pt(u - w * 0.5 - 0.25, z + h + 0.6, 0.06), F.pt(u - w * 0.5 - 0.25, z + h + 0.9, 0.0)], 0.1), 0.022, seg=6)


# ===================================================================================================== GLA: o adobe

def adobe_wall(p, a, b, h, t=0.6, z0=0.2, m='Plaster#wall', batter=0.06, crown=0.5, band=None, ends=(True, True)):
    """O muro de adobe numa peça só, h = a altura total: a base mais larga (batter de cada lado), as faces em leve escarpa e o
    topo redondo (o pão: crown = a altura da coroa em relação à metade da espessura), a quina gasta do chanfro largo nas pontas (o 'Plaster#wall' tem
    o chanfro de 14 cm). band = (z_baixo, z_alto, material) a faixa verde pintada à mão nas 2 faces (a borda ondula)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    if L < 1e-3:
        return
    dx, dy = (bx - ax) / L, (by - ay) / L
    nx, ny = -dy, dx
    ht = t / 2
    hb = ht + batter
    zr = z0 + h - ht * crown
    prof = [(-hb, z0 - 0.04), (hb, z0 - 0.04), (ht, zr)]
    for k in range(1, 6):
        ang = math.pi * k / 6
        prof.append((ht * math.cos(ang), zr + ht * crown * math.sin(ang) ** 0.8))
    prof.append((-ht, zr))
    b_ = p.bm(m, 0.14)
    rings = []
    for (px, py) in ((ax, ay), (bx, by)):
        rings.append([b_.verts.new(V(px + nx * s, py + ny * s, z)) for (s, z) in prof])
    r0, r1 = rings
    b_.faces.new(list(reversed(r0)))
    b_.faces.new(r1)
    n = len(prof)
    for i in range(n):
        j = (i + 1) % n
        f = b_.faces.new([r0[i], r0[j], r1[j], r1[i]])
        f.smooth = 2 <= i <= n - 2
    if band:
        zl, zh, bm_ = band
        for s in (-1, 1):
            paint_band(p, (ax, ay), (bx, by), zl, zh, side=s, off=lambda z: ht + batter * (1 - (z - z0) / max(zr - z0, 0.01)), m=bm_,
                       seed=int(abs(ax * 13 + ay * 7 + s * 3)))


def paint_band(p, a, b, zl, zh, side=1, off=0.3, m='Player', seed=0, wob=0.06):
    """A faixa pintada à mão (a cor do jogador) colada numa face: a borda de baixo e a de cima ondulam (o pincel), a ponta não é
    reta. off = a distância do eixo do muro até a face (número ou função da altura)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    nx, ny = -dy * side, dx * side
    n = max(2, int(L / 0.35))
    bb = p.bm(m, 0.0)
    top, bot = [], []
    for k in range(n + 1):
        u = k / n
        x, y = ax + (bx - ax) * u, ay + (by - ay) * u
        lo = zl + wob * (_hash(k, seed, 3) - 0.5) * 2 + 0.03 * math.sin(u * 17 + seed)
        hi = zh + wob * 0.5 * (_hash(k, seed, 5) - 0.5) * 2
        for (z, lst) in ((lo, bot), (hi, top)):
            o = _f(off, z) + 0.012
            lst.append((bb.verts.new(V(x + nx * o, y + ny * o, z)), bb.verts.new(V(x + nx * (o - 0.03), y + ny * (o - 0.03), z))))
    for k in range(n):
        bb.faces.new([bot[k][0], bot[k + 1][0], top[k + 1][0], top[k][0]])
        bb.faces.new([bot[k][1], top[k][1], top[k + 1][1], bot[k + 1][1]])
        bb.faces.new([bot[k][0], bot[k][1], bot[k + 1][1], bot[k + 1][0]])
        bb.faces.new([top[k][0], top[k + 1][0], top[k + 1][1], top[k][1]])
    bb.faces.new([bot[0][0], top[0][0], top[0][1], bot[0][1]])
    bb.faces.new([bot[n][0], bot[n][1], top[n][1], top[n][0]])


def merlons(p, a, b, z, n=None, w=0.55, t=0.5, h=0.5, m='Plaster#wall', bevel=0.14):
    """As ameias de topo redondo (uma peça só cada: o perfil de arco extrudado na espessura, com a quina gasta)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    nx, ny = -dy, dx
    n = n or max(2, int(L / (w * 2.0)))
    prof = [(-w / 2, -0.05), (w / 2, -0.05), (w / 2, h - w / 2)]
    for k in range(1, 8):
        ang = math.pi * k / 8
        prof.append((w / 2 * math.cos(ang), h - w / 2 + w / 2 * math.sin(ang)))
    prof.append((-w / 2, h - w / 2))
    bb = p.bm(m, bevel)
    for k in range(n):
        u = (k + 0.5) / n
        cx, cy = ax + (bx - ax) * u, ay + (by - ay) * u
        rings = []
        for s in (-t / 2, t / 2):
            rings.append([bb.verts.new(V(cx + dx * q + nx * s, cy + dy * q + ny * s, z + zz)) for (q, zz) in prof])
        r0, r1 = rings
        bb.faces.new(list(reversed(r0)))
        bb.faces.new(r1)
        for i in range(len(prof)):
            j = (i + 1) % len(prof)
            f = bb.faces.new([r0[i], r0[j], r1[j], r1[i]])
            f.smooth = 2 <= i <= len(prof) - 2


def lintel(p, F, u0, u1, z, h=0.18, out=0.08, m='Wood#lintel', n_in=-0.25):
    """A verga de madeira sobre o vão do adobe (o tronco quadrado que passa da abertura dos 2 lados e sai um pouco da face)."""
    wbox(p, m, F, u0 - 0.3, u1 + 0.3, z, z + h, n_in, out, bevel=0.03)


def adobe_window(p, F, u0, u1, z0, z1, t=0.45, frame='Wood#win', pane='Dark', shutter=None, grille=False, sill='Plaster#sill',
                 lintel_m='Wood#lintel'):
    """A janela no vão fundo do adobe: o caixilho de madeira no meio da parede, o escuro atrás, a verga de madeira por cima saindo da
    face, o peitoril de reboco gasto, a grade (barras de 5 cm) e a veneziana aberta (shutter = o material)."""
    d = t * 0.5
    fw = 0.08
    wbox(p, frame, F, u0, u1, z0, z0 + fw, -d - 0.05, -d + 0.05, bevel=0.015)
    wbox(p, frame, F, u0, u1, z1 - fw, z1, -d - 0.05, -d + 0.05, bevel=0.015)
    wbox(p, frame, F, u0, u0 + fw, z0, z1, -d - 0.05, -d + 0.05, bevel=0.015)
    wbox(p, frame, F, u1 - fw, u1, z0, z1, -d - 0.05, -d + 0.05, bevel=0.015)
    wbox(p, frame, F, (u0 + u1) / 2 - 0.03, (u0 + u1) / 2 + 0.03, z0, z1, -d - 0.04, -d + 0.03, bevel=0.015)
    wbox(p, pane, F, u0 + 0.02, u1 - 0.02, z0 + 0.02, z1 - 0.02, -d - 0.04, -d - 0.02, bevel=0.0)
    lintel(p, F, u0, u1, z1 + 0.02, m=lintel_m)
    wbox(p, sill, F, u0 - 0.08, u1 + 0.08, z0 - 0.1, z0 + 0.01, -d + 0.04, 0.09, bevel=0.03)
    if grille:
        n = max(2, int((u1 - u0) / 0.2))
        for k in range(1, n):
            u = u0 + (u1 - u0) * k / n
            c = F.pt(u, (z0 + z1) / 2, -0.12)
            p.cyl('Trim#bar', c, 'z', 0.024, z1 - z0, seg=6, bevel=0.0)
    if shutter:
        for s in (-1, 1):
            ua = u0 - 0.06 if s < 0 else u1 + 0.06
            ub = ua + s * (u1 - u0) * 0.48
            wbox(p, shutter, F, min(ua, ub), max(ua, ub), z0 + 0.02, z1 - 0.02, 0.0, 0.05, bevel=0.015)
            for k in range(1, 5):                                             # as tábuas (o ritmo das frestas)
                uu = ua + (ub - ua) * k / 5
                wbox(p, 'Dark', F, uu - 0.012, uu + 0.012, z0 + 0.04, z1 - 0.04, 0.05, 0.056, bevel=0.0)


def adobe_door(p, F, u0, u1, z0, z1, t=0.45, m='Wood#door', lintel_m='Wood#lintel', boards=0.22, open_frac=0.0):
    """A porta de tábuas no vão fundo: as tábuas (o ritmo das frestas de 2 cm, lê como veio), as 2 travessas e a diagonal, o
    batente, a verga por cima. open_frac > 0: uma folha entreaberta (o escuro de dentro aparece)."""
    d = t * 0.55
    wbox(p, 'Dark', F, u0 + 0.01, u1 - 0.01, z0, z1 - 0.01, -t + 0.03, -t + 0.05, bevel=0.0)
    n = max(2, int((u1 - u0) / boards))
    for k in range(n):
        ua = u0 + 0.04 + (u1 - u0 - 0.08) * k / n
        ub = u0 + 0.04 + (u1 - u0 - 0.08) * (k + 1) / n - 0.02
        if open_frac and k >= n * (1 - open_frac):
            continue
        wbox(p, m, F, ua, ub, z0 + 0.02, z1 - 0.04, -d - 0.05, -d, bevel=0.012)
    for zz in (z0 + 0.35, z1 - 0.45):
        wbox(p, m, F, u0 + 0.1, u1 - 0.1, zz - 0.08, zz + 0.08, -d, -d + 0.04, bevel=0.012)
    wbox(p, m, F, u0, u0 + 0.1, z0, z1, -d - 0.06, -d + 0.06, bevel=0.015)
    wbox(p, m, F, u1 - 0.1, u1, z0, z1, -d - 0.06, -d + 0.06, bevel=0.015)
    lintel(p, F, u0, u1, z1 + 0.02, m=lintel_m)


def adobe_block(p, x0, x1, y0, y1, z0, z1, t=0.45, holes=None, m='Plaster', slab_m='Plaster#slab', parapet=0.55, par_t=0.4,
                beams=('+x', '-x'), beam_step=0.9, beam_out=0.38, beam_r=0.085, beam_z=None, interior=True, cols=(),
                skip_parapet=(), roof_t=0.32):
    """O bloco de adobe oco (a casa, o armazém): as 4 paredes com espessura (t ≥ 0,35) montadas em volta dos vãos (holes =
    {'+x': [(u0, u1, z0, z1)...]}), a laje do teto com espessura, o parapeito de topo redondo (com a quina gasta), e as vigas de
    madeira que atravessam de lado a lado e saem pela parede (beams = as faces por onde as pontas saem; as mesmas vigas seguram o
    teto por dentro: o adobe de verdade, e o que o furo do dano mostra). cols = os esteios de dentro [(x, y)]."""
    holes = holes or {}
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    zt = z1 - roof_t
    for f in ('+x', '-x', '+y', '-y'):
        F = WallFace(f, at[f], z0)
        if f[1] == 'x':
            u0, u1 = y0 + EPS, y1 - EPS
        else:
            u0, u1 = x0, x1
        for (a, b, c, d, lo, hi) in rect_minus(u0, u1, z0, z1, holes.get(f, ())):
            wbox(p, m, F, a, b, c, d, -t, 0.0)
    # a laje do teto (por dentro das paredes) e o piso de dentro
    p.box(slab_m, x0 + t - 0.05, x1 - t + 0.05, y0 + t - 0.05, y1 - t + 0.05, zt, z1 - 0.02, bevel=0.03)
    if interior:
        p.box(slab_m, x0 + t - 0.05, x1 - t + 0.05, y0 + t - 0.05, y1 - t + 0.05, z0 - 0.05, z0 + 0.06, bevel=0.0)
    # as vigas (os troncos) logo abaixo da laje, atravessando o lado curto, saindo pelas faces de 'beams'
    bz = beam_z if beam_z is not None else zt - beam_r - 0.02
    along_y = (x1 - x0) >= (y1 - y0)            # vigas cruzando o Y (o lado curto) e repetindo em X
    if beams:
        if along_y:
            ext0 = beam_out if '-y' in beams else -0.05
            ext1 = beam_out if '+y' in beams else -0.05
            for k, x in enumerate(splits(x0 + 0.6, x1 - 0.6, beam_step)):
                rr = beam_r * (0.88 + 0.24 * _hash(k, int(x0 * 10), 5))
                p.cyl('Wood#beam', (x, (y0 - ext0 + y1 + ext1) / 2, bz), 'y', rr, (y1 + ext1) - (y0 - ext0), seg=8, bevel=0.0)
        else:
            ext0 = beam_out if '-x' in beams else -0.05
            ext1 = beam_out if '+x' in beams else -0.05
            for k, y in enumerate(splits(y0 + 0.6, y1 - 0.6, beam_step)):
                rr = beam_r * (0.88 + 0.24 * _hash(k, int(y0 * 10), 5))
                p.cyl('Wood#beam', ((x0 - ext0 + x1 + ext1) / 2, y, bz), 'x', rr, (x1 + ext1) - (x0 - ext0), seg=8, bevel=0.0)
    for (cx, cy) in cols:
        p.cyl('Wood#beam', (cx, cy, (z0 + bz) / 2), 'z', 0.11, bz - z0, seg=8, bevel=0.0)
    # o parapeito de topo redondo em volta do teto
    if parapet:
        segs = {'-y': ((x0, y0 + par_t / 2), (x1, y0 + par_t / 2)), '+y': ((x0, y1 - par_t / 2), (x1, y1 - par_t / 2)),
                '-x': ((x0 + par_t / 2, y0 + par_t), (x0 + par_t / 2, y1 - par_t)), '+x': ((x1 - par_t / 2, y0 + par_t), (x1 - par_t / 2, y1 - par_t))}
        for f, (a, b) in segs.items():
            if f in skip_parapet:
                continue
            adobe_wall(p, a, b, parapet, t=par_t, z0=z1 - 0.03, batter=0.0, crown=0.6)


# ===================================================================================================== o pano

def tarp(p, corners, m='Cloth#tarp', sag=0.15, folds=4, amp=0.07, nx=None, ny=None, thick=0.03, ropes=None, rope_m='Trim#rope',
         seed=0):
    """A lona com as dobras de tensão: de cada canto amarrado saem 'folds' dobras em leque que morrem no meio (a lona puxada
    pela corda), mais a barriga. ropes = [(canto 0..3, (x, y, z) da ancoragem)...]: a corda do canto até a estaca/o gancho, com
    a laçada no canto."""
    c = [Vector(q) for q in corners]
    L = max((c[1] - c[0]).length, (c[2] - c[3]).length)
    W = max((c[3] - c[0]).length, (c[2] - c[1]).length)
    nx = nx or max(6, int(L / 0.18))
    ny = ny or max(4, int(W / 0.18))
    cuv = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))

    def zf(u, v, z):
        dz = -sag * math.sin(math.pi * u) * math.sin(math.pi * v)
        for k, (cu, cv) in enumerate(cuv):
            du, dv = (u - cu) * L, (v - cv) * W
            r = math.hypot(du, dv)
            if r < 1e-4:
                continue
            ang = math.atan2(dv, du)
            env = (r / 0.5) * math.exp(-r / (0.22 * (L + W) / 2 + 0.3))
            dz += amp * env * math.sin(folds * 2 * ang + _hash(k, seed, 1) * 6.28)
        return z + dz
    model_lib.sheet(p, m, corners, nx, ny, sag=0.0, z_fn=zf, thick=thick)
    for (k, anchor) in (ropes or ()):
        a = tuple(c[k])
        cable(p, a, anchor, sag=0.06, r=0.018, m=rope_m)
        p.torus(rope_m, a, 'z', 0.05, 0.016, seg=10, rseg=5)


def net_tufts(p, fn, nu, nv, keep, seed=1, density=4.0, size=(0.14, 0.32), mats=('Net#tuft', 'Cloth#tuft')):
    """O volume da rede de camuflagem: as tiras de pano amarradas na malha (os cachos alongados e achatados, 0,15–0,3 m, poucos
    cm de altura) espalhadas por cima de onde há rede (keep(u, v) → bool), em direções soltas: de cima a rede vira uma superfície
    fofa com sombra miúda, não um plano (e não pedras: o cacho é baixo e comprido). density = cachos por m² (aprox.)."""
    P0 = Vector(fn(0, 0))
    P1 = Vector(fn(1, 0))
    P3 = Vector(fn(0, 1))
    area = (P1 - P0).length * (P3 - P0).length
    n = int(area * density)
    for k in range(n):
        u, v = _hash(k, seed, 11), _hash(k, seed, 13)
        if not keep(u, v):
            continue
        q = Vector(fn(u, v))
        r = size[0] + (size[1] - size[0]) * _hash(k, seed, 17)
        m = mats[1] if _hash(k, seed, 19) > 0.78 else mats[0]
        b = p.bm(m, 0.0)
        rot = _hash(k, seed, 23) * 6.28
        ca, sa = math.cos(rot), math.sin(rot)
        hh = r * (0.16 + 0.12 * _hash(k, seed, 29))
        ring_n = 6
        top = b.verts.new(V(q.x, q.y, q.z + 0.02 + hh))
        ring, mid = [], []
        for i in range(ring_n):
            a = 2 * math.pi * i / ring_n
            rr = r * (0.75 + 0.4 * _hash(k * 7 + i, seed, 31))
            ex, ey = rr * math.cos(a), rr * 0.45 * math.sin(a)                   # o cacho comprido
            ring.append(b.verts.new(V(q.x + ex * ca - ey * sa, q.y + ex * sa + ey * ca, q.z + 0.025)))
            a2 = 2 * math.pi * (i + 0.5) / ring_n
            rr2 = r * 0.5
            ex, ey = rr2 * math.cos(a2), rr2 * 0.45 * math.sin(a2)
            mid.append(b.verts.new(V(q.x + ex * ca - ey * sa, q.y + ex * sa + ey * ca, q.z + 0.02 + hh * 0.75)))
        for i in range(ring_n):
            j = (i + 1) % ring_n
            b.faces.new([ring[i], ring[j], mid[i]]).smooth = True
            b.faces.new([mid[i], ring[j], mid[j]]).smooth = True
            b.faces.new([mid[i], mid[j], top]).smooth = True


# ===================================================================================================== a peça que gira

_MOVED = {}


def bake_in_place(o, at):
    """No executor do detalhe (model_run_detail.py): a peça que gira, modelada com a origem no eixo, fica no lugar dela (at, Unreal)
    durante a grade e o cozimento das máscaras (a sombra de contato certa, nela e no prédio) e volta pra origem na exportação. Fora
    do executor não faz nada."""
    if model_lib.DETAIL.get('defer') is None:
        return o
    o.location = (at[0], -at[1], at[2])
    _MOVED[o.as_pointer()] = o
    if not getattr(model_lib, '_bp_patched', False):
        one, group = model_lib.export, model_lib.export_group

        def _reset(objs):
            if model_lib.DETAIL.get('defer') is None:
                for q in objs:
                    if q.as_pointer() in _MOVED:
                        q.location = (0.0, 0.0, 0.0)

        def export(o_, out_dir):
            _reset([o_])
            return one(o_, out_dir)

        def export_group(objs, out_dir, name):
            _reset(objs)
            return group(objs, out_dir, name)
        model_lib.export, model_lib.export_group = export, export_group
        model_lib._bp_patched = True
    return o


# ===================================================================================================== o kit da GLA no detalhe
# (as trocas pras funções do "kit GLA do elenco" copiado nos 6 prédios da GLA: mesma assinatura; o prédio importa daqui por cima
# da cópia dele)

def _vnoise(x, y, s=0):
    i, j = math.floor(x), math.floor(y)
    fx, fy = x - i, y - j
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b, c, d = _hash(i, j, s), _hash(i + 1, j, s), _hash(i, j + 1, s), _hash(i + 1, j + 1, s)
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def _fbm(x, y, s=0):
    return _vnoise(x, y, s) * 0.65 + _vnoise(x * 2.13 + 5.3, y * 2.13 + 1.7, s + 1) * 0.35


def gla_wall_seg(p, a, b, h, t=0.6, z0=0.2, band=True, round_top=True, m='Plaster', band_hi=0.12, band_lo=0.55):
    """O muro do kit GLA (a mesma assinatura e o mesmo topo do wall_seg): uma peça só, a base mais larga, o topo de pão redondo,
    a quina gasta (o chanfro de 14 cm nas pontas) e a faixa verde pintada à mão (a borda ondula) nas 2 faces."""
    crown = 1.0 if round_top else 0.0
    total = h + (t / 2 * 0.97 if round_top else 0.0)
    adobe_wall(p, a, b, total, t=t, z0=z0, m=m.split('#')[0] + '#wall', batter=0.05 if t >= 0.3 else 0.0, crown=crown,
               band=(z0 + h - band_lo, z0 + h - band_hi, 'Player') if band else None)


def gla_round_merlons(p, a, b, z, n=None, w=0.5, t=0.45, h=0.45, m='Plaster'):
    """As ameias de topo redondo do kit GLA (a mesma assinatura): cada uma uma peça só com a quina gasta."""
    merlons(p, a, b, z, n=n, w=w, t=t, h=h + w / 2, m=m.split('#')[0] + '#merlon', bevel=0.012)


def gla_drift(p, a, b, nrm, h=0.4, w=0.6, seed=1, z=0.2):
    """A areia acumulada no pé do muro (do lado de fora), arredondada: o monte de perfil côncavo (a rampa de repouso da areia)
    com as pontas que morrem no chão, de a até b, a normal nrm (nunca passa de w: fica no lote)."""
    (ax, ay), (bx, by) = a, b
    L = math.hypot(bx - ax, by - ay)
    k = max(1, int(L / 3.0))
    for i in range(k):
        u0, u1 = i / k + 0.03, (i + 1) / k - 0.03
        hh = h * (0.6 + 0.6 * _hash(i, seed, 21))
        ww = w * (0.6 + 0.4 * _hash(i, seed, 23))
        n = 8
        rings = []
        for j in range(n + 1):
            t = j / n
            env = math.sin(math.pi * t) ** 0.6
            x = ax + (bx - ax) * (u0 + (u1 - u0) * t)
            y = ay + (by - ay) * (u0 + (u1 - u0) * t)
            ring = []
            for q in range(6):
                s = q / 5
                d = -0.08 + (ww * env + 0.08) * s
                zz = z - 0.05 + (hh * env) * (1 - s) ** 1.7
                ring.append((x + nrm[0] * d, y + nrm[1] * d, zz))
            ring.append((x + nrm[0] * (ww * env), y + nrm[1] * (ww * env), z - 0.06))
            ring.append((x - nrm[0] * 0.08, y - nrm[1] * 0.08, z - 0.06))
            rings.append(ring)
        p.loft('Earth#drift', rings, bevel=0.0, smooth=True)


def gla_net_surface(p, fn, nu, nv, seed=1, blotch=1.5, holes=0.3, light=0.62, ragged=0.35, thick=0.03, closed_u=False,
                    mats=('Net', 'Cloth#net')):
    """A rede de camuflagem do kit GLA (a mesma assinatura do net_surface) COM VOLUME: a rede mosqueada com os buracos e os
    retalhos de juta (o contorno pelo marching squares, como antes), agora mais grossa, ondulando entre os apoios (a barriga
    em gomos, não um plano), e os tufos de pano amarrados por cima (net_tufts): de cima, uma superfície fofa com sombra miúda."""
    def fn2(u, v):
        x, y, z = fn(u, v)
        e = min(u, 1 - u, v, 1 - v) if not closed_u else min(v, 1 - v)
        w = min(1.0, e * 6.0)
        return (x, y, z + w * 0.09 * (math.sin(u * 9.4 + seed) * math.sin(v * 7.1 + seed * 0.7)))
    P = [[fn2(i / nu, j / nv) for i in range(nu + 1)] for j in range(nv + 1)]
    F = []
    for j in range(nv + 1):
        row = []
        for i in range(nu + 1):
            u, v = i / nu, j / nv
            n = _fbm(P[j][i][0] / blotch + 0.37, P[j][i][1] / blotch + 0.71, seed)
            ec = (1.0 - v) * nv if closed_u else min(v * nv, (1.0 - v) * nv, u * nu, (1.0 - u) * nu)
            row.append(n - ragged * max(0.0, 1.0 - ec / 1.6) - (0.0 if ec > 0 else 0.6))
        F.append(row)
    th = max(thick, 0.05)
    for (m, t, lift) in ((mats[0], holes, 0.0), (mats[1], light, 0.02)):
        b = p.bm(m, 0.0)
        for j in range(nv):
            for i in range(nu):
                cs = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                poly = []
                for k in range(4):
                    (ia, ja), (ib, jb) = cs[k], cs[(k + 1) % 4]
                    fa, fb = F[ja][ia], F[jb][ib]
                    if fa >= t:
                        poly.append(P[ja][ia])
                    if (fa >= t) != (fb >= t):
                        s_ = (t - fa) / (fb - fa)
                        pa, pb = P[ja][ia], P[jb][ib]
                        poly.append((pa[0] + (pb[0] - pa[0]) * s_, pa[1] + (pb[1] - pa[1]) * s_, pa[2] + (pb[2] - pa[2]) * s_))
                if len(poly) < 3:
                    continue
                top = [b.verts.new(V(x, y, z + th / 2 + lift)) for (x, y, z) in poly]
                bot = [b.verts.new(V(x, y, z - th / 2 + lift)) for (x, y, z) in poly]
                b.faces.new(top).smooth = True
                b.faces.new(list(reversed(bot))).smooth = True

    def keep(u, v):
        i, j = min(nu, int(round(u * nu))), min(nv, int(round(v * nv)))
        return F[j][i] >= holes + 0.04
    net_tufts(p, fn2, nu, nv, keep, seed=seed, mats=(mats[0] + '#tuft' if '#' not in mats[0] else mats[0], 'Cloth#tuft'))


def gla_tarp_sheet(p, corners, sag=0.15, nx=8, ny=6, m='Cloth#tarp'):
    """A lona do kit GLA (a mesma assinatura): com as dobras de tensão que saem dos cantos amarrados e a barriga."""
    tarp(p, corners, m=m, sag=sag, folds=3, amp=0.05 + sag * 0.15, seed=int(abs(corners[0][0] * 7 + corners[0][1] * 3)))


def paint_band_block(p, x0, x1, y0, y1, zl, zh, faces='+x-x+y-y', m='Player', seed=0):
    """A faixa verde pintada à mão em volta de um bloco (as faces pedidas), com a borda ondulando (o pincel)."""
    if '+x' in faces:
        paint_band(p, (x1, y0), (x1, y1), zl, zh, side=-1, off=0.0, m=m, seed=seed + 1)
    if '-x' in faces:
        paint_band(p, (x0, y0), (x0, y1), zl, zh, side=1, off=0.0, m=m, seed=seed + 2)
    if '+y' in faces:
        paint_band(p, (x0, y1), (x1, y1), zl, zh, side=1, off=0.0, m=m, seed=seed + 3)
    if '-y' in faces:
        paint_band(p, (x0, y0), (x1, y0), zl, zh, side=-1, off=0.0, m=m, seed=seed + 4)


def barn_gate(p, F, u0, u1, z0, z1, t=0.45, m='Crate#gate', brace='Dark#gate', lintel_m='Wood#lintel'):
    """O portão de galpão no vão fundo da parede: as 2 folhas de tábua recuadas (o ritmo das tábuas, as 3 travessas e o Z de cada
    folha em relevo, a aldrava), o batente de madeira e a verga por cima saindo da face."""
    d = t * 0.45
    wbox(p, 'Dark', F, u0, u1, z0, z1, -t + 0.02, -t + 0.04, bevel=0.0)
    um = (u0 + u1) / 2
    for (a, b) in ((u0 + 0.06, um - 0.02), (um + 0.02, u1 - 0.06)):
        n = max(3, int((b - a) / 0.24))
        for k in range(n):
            ua = a + (b - a) * k / n
            ub = a + (b - a) * (k + 1) / n - 0.018
            wbox(p, m, F, ua, ub, z0 + 0.04, z1 - 0.06, -d - 0.05, -d, bevel=0.01)
        for zz in (z0 + 0.4, (z0 + z1) / 2, z1 - 0.45):
            wbox(p, brace, F, a + 0.06, b - 0.06, zz - 0.09, zz + 0.09, -d, -d + 0.04, bevel=0.0)
        for (za, zb) in ((z0 + 0.4, (z0 + z1) / 2), ((z0 + z1) / 2, z1 - 0.45)):
            Lr = math.hypot(b - a - 0.2, zb - za)
            c = F.pt((a + b) / 2, (za + zb) / 2, -d + 0.02)
            ang = math.degrees(math.atan2(zb - za, b - a - 0.2))
            if F.ax == 'x':
                p.obox(brace, c, (0.04, Lr, 0.12), roll=ang if F.s > 0 else -ang, bevel=0.0)
            else:
                p.obox(brace, c, (Lr, 0.04, 0.12), pitch=-ang if F.s > 0 else ang, bevel=0.0)
    wbox(p, brace, F, um - 0.3, um + 0.3, (z0 + z1) / 2 - 0.08, (z0 + z1) / 2 + 0.02, -d + 0.04, -d + 0.07, bevel=0.0)
    wbox(p, 'Wood#frame', F, u0, u0 + 0.12, z0, z1, -d - 0.08, -d + 0.08, bevel=0.015)
    wbox(p, 'Wood#frame', F, u1 - 0.12, u1, z0, z1, -d - 0.08, -d + 0.08, bevel=0.015)
    lintel(p, F, u0, u1, z1 + 0.02, h=0.24, m=lintel_m)
