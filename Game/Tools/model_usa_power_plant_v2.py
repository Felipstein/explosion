"""A usina da USA (usa_power_plant, o Cold Fusion Reactor), VERSÃO B no detalhe AAA (01/11/2026): a de antes era a caixa do script
antigo (Game/Tools/blender_provisional.py power_plant(), a do jogo hoje); esta é a refeita no nível do Strategy Center B.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_power_plant_v2.py C:/Users/luisf/explosion/captures/game/models/usa_power_plant_v2

O pedido (01/11, o Felipe): "refazer as construções iniciais da USA, ficaram todas quadradas, sem vida, sem qualidade [...] as novas que
você fez todas ficaram muito boas". A PALAVRA (docs/CONCEITO_CONSTRUCOES.md §3b e captures/concept/47_usa_power_plant/PROMPTS.md, o
design 1): O REATOR. O vaso branco e grosso subindo do meio do bloco blindado, abraçado por 4 ANÉIS de aço, a faixa estreita entre dois
anéis acesa (o núcleo vivo: o slot Core), o domo com a COROA de 12 mecanismos das barras de controle; de cima, círculos concêntricos (o
guarda-corpo da passarela, o anel, o domo com as nervuras, a coroa, o cubo) dentro do lote retangular. É o prédio mais repetido da base
(4 a 6 por partida): pequeno, uma silhueta só. Proibido: a torre de resfriamento em ampulheta (é da China).
  - o pedestal: a saia blindada de 12 faces com UMA chapa por face e os parafusos, a laje de concreto do pedestal com a borda;
  - o vaso: as placas curvas com a junta funda em 2 fileiras, a fileira azul de placas logo abaixo do anel de cima (a cor do
    jogador), os 4 anéis torneados com o anel de porcas, a faixa acesa com as 24 grades na frente, a passarela de cima com o
    guarda-corpo redondo e as mãos-francesas, a escada de marinheiro com a gaiola;
  - o domo elíptico com as 12 nervuras, o cubo da tampa com os parafusos, os 12 mecanismos (o tubo torneado com o colar e a tampa) e o
    aro que os amarra; a sirene no cubo;
  - a eclusa da frente (+X): o bloco de placas, a porta blindada recuada com as nervuras, a moldura de perigo, a placa azul, o
    holofote, a marquise, os frades;
  - a ala do vapor (−Y): o bloco baixo de placas com a faixa azul, a veneziana grande da tomada de ar, as 2 CHAMINÉS curtas de onde
    sai o vapor branco (o jogo), os 2 canos grossos até o vaso;
  - a ala da força (+Y): o bloco baixo, o TRANSFORMADOR no teto (o tanque com as aletas e os 3 isoladores de porcelana), os 3 cabos
    grossos com a barriga até o vaso, a carga com a LONA azul amarrada (a cor do jogador, o objeto do tanque);
  - a laje de fundação em placas com a junta; as luminárias com o corpo nas alas.
A USA limpa (13/10): sem desgaste pintado.

Lote 9,28 × 12,64 m (x −4,64..4,64, y −6,32..6,32), a frente em +X; nada sai do lote. Alturas: o pedestal 1,5 m, o vaso até 6,4 m, o
domo 7,5 m, a coroa 8,4 m (+ a sirene). Vapor: as 2 bocas em (0,05; ±1,2... ver VENTS) no topo das chaminés.
Peças: SM_USA_PowerPlant_Base (tudo: nada gira; a vida é o vapor, o núcleo aceso e a sirene, do jogo), SM_USA_PowerPlant_Scaffold.
Slots (os do Strategy Center B): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime), e o
Core (o núcleo aceso, o mesmo slot da usina antiga: import_provisional.py SLOTS['Core']).

O KIT DOS 3 PRÉDIOS (o Command Center e o Supply Center B importam daqui; o main() só roda como script):
  CPart (o BPart com o aviso NONMANIFOLD antes do chanfro), Plan (o contorno convexo pelas faces: os anéis, as faces, o recuo),
  poly_slab (a laje com o topo em grade de quadrados: o n-ágono grande borrava as máscaras), poly_band (o parapeito), glacis (a saia
  com as chapas), plan_walls (as paredes de placas com a fileira azul), plan_pillars (os pilares redondos nas quinas), curved_panels
  (as placas curvas do vaso), slab_tiles (o chão em lajes com a junta), scaffold_poly (o andaime pelo contorno), check (as medidas).
"""
import math
import os
import sys

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, Part  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import BPart  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402

PREFIX = 'SM_USA_PowerPlant'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62),
               'Core': (255, 236, 210), 'Rubber': (32, 32, 31)})

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


# ===================================================================================================== o kit dos 3 prédios
class CPart(BPart):
    """O BPart (o corte dos fundos escondidos) com o aviso do model_russia_parts: o balde chanfrado com aresta de 3+ faces depois da
    solda é peça encostando em peça do mesmo balde (o chanfro joga vértices pro infinito e a grade do detalhe não acaba)."""

    def finish(self):
        for m, b in self.bms.items():
            if self.bevel[m] <= 0 or not len(b.faces):
                continue
            c = b.copy()
            bmesh.ops.remove_doubles(c, verts=c.verts, dist=1e-5)
            bad = [e for e in c.edges if len(e.link_faces) > 2]
            if bad:
                q = bad[0].verts[0].co
                print('NONMANIFOLD', self.name, m, len(bad), 'arestas; a 1a em (%.2f, %.2f, %.2f)' % (q.x, -q.y, q.z))
            c.free()
        return super().finish()


class Plan:
    """O contorno convexo pelas faces: [(o ângulo da normal em graus, a distância do centro)], em ordem anti-horária; c = o centro
    (x, y). corner(k) = a quina entre a face k e a k+1; dl recua (−) ou avança (+) todas as faces juntas (a saia, o parapeito)."""

    def __init__(self, c, faces):
        self.c = Vector((c[0], c[1], 0.0))
        self.f = [(math.radians(a), d) for a, d in faces]
        self.n = len(faces)

    def nrm(self, k):
        a = self.f[k % self.n][0]
        return Vector((math.cos(a), math.sin(a), 0.0))

    def corner(self, k, dl=0.0):
        k %= self.n
        n1, n2 = self.nrm(k), self.nrm(k + 1)
        d1, d2 = self.f[k][1] + dl, self.f[(k + 1) % self.n][1] + dl
        det = n1.x * n2.y - n1.y * n2.x
        return self.c + Vector(((d1 * n2.y - d2 * n1.y) / det, (n1.x * d2 - n2.x * d1) / det, 0.0))

    def ring(self, dl, z):
        return [tuple(self.corner(k, dl) + Vector((0, 0, z))) for k in range(self.n)]

    def face(self, k, dl=0.0):
        """(a quina de começo, eu ao longo, en pra fora, a largura) da face k."""
        a, b = self.corner(k - 1, dl), self.corner(k, dl)
        return a, (b - a).normalized(), self.nrm(k), (b - a).length


def chamfer_rect(c, hx_p, hx_m, hy_p, hy_m, ch=(0.0, 0.0, 0.0, 0.0)):
    """O retângulo (as distâncias do centro pra +X, −X, +Y, −Y) com as quinas cortadas a 45° (ch: +X+Y, −X+Y, −X−Y, +X−Y; 0 = sem)."""
    faces = [(0, hx_p)]
    for ang, dd, cut in ((45, (hx_p, hy_p), ch[0]), (90, None, None), (135, (hx_m, hy_p), ch[1]), (180, None, None),
                         (225, (hx_m, hy_m), ch[2]), (270, None, None), (315, (hx_p, hy_m), ch[3])):
        if dd is None:
            faces.append((ang, {90: hy_p, 180: hx_m, 270: hy_m}[ang]))
        elif cut > 0:
            faces.append((ang, (dd[0] + dd[1] - cut) / math.sqrt(2)))
    return Plan(c, faces)


def poly_slab(p, m, plan, dl, z0, z1, cell=1.6, bottom=True, hide=None):
    """A laje no contorno (recuado dl) de z0 a z1, sólida, com o TOPO EM GRADE (setores de quadrados do centro até cada face, todos com
    o mesmo número de anéis: as arestas radiais batem): o n-ágono grande, sem pontos dentro, borrava as máscaras na grade do detalhe."""
    b = p.bm(m, ub._bev(m))
    C = plan.c
    cs = [plan.corner(k, dl) for k in range(plan.n)]
    ns = max(1, math.ceil(max((q - C).length for q in cs) / cell))
    vs = {}

    def vv(q, z):
        key = (round(q.x, 4), round(q.y, 4), round(z, 4))
        if key not in vs:
            vs[key] = b.verts.new(V(q.x, q.y, z))
        return vs[key]
    edge = []
    for k in range(plan.n):
        A, B = cs[k - 1], cs[k]
        nt = max(1, math.ceil((B - A).length / cell))
        g = [[vv(C + ((A + (B - A) * (j / nt)) - C) * (i / ns), z1) for j in range(nt + 1)] for i in range(ns + 1)]
        for i in range(ns):
            for j in range(nt):
                q = []
                for x in (g[i][j], g[i][j + 1], g[i + 1][j + 1], g[i + 1][j]):
                    if x not in q:
                        q.append(x)
                if len(q) >= 3:
                    if hide is not None:
                        cq = C + ((A + (B - A) * ((j + 0.5) / nt)) - C) * ((i + 0.5) / ns)
                        if hide(cq):
                            continue
                    b.faces.new(q)
        edge += [A + (B - A) * (j / nt) for j in range(nt)]
    top = [vv(q, z1) for q in edge]
    bot = [b.verts.new(V(q.x, q.y, z0)) for q in edge]
    n = len(edge)
    for i in range(n):
        k = (i + 1) % n
        b.faces.new([top[i], bot[i], bot[k], top[k]])
    if bottom:
        b.faces.new(bot)


def poly_band(p, m, plan, dl_out, dl_in, z0, z1):
    """O anel no contorno (o parapeito, a capa): as faces de fora, de cima, de dentro e de baixo."""
    b = p.bm(m, ub._bev(m))
    rings = [[b.verts.new(V(*q)) for q in plan.ring(d, z)] for d, z in ((dl_out, z0), (dl_out, z1), (dl_in, z1), (dl_in, z0))]
    n = plan.n
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(n):
            b.faces.new([ra[k], ra[(k + 1) % n], rb[(k + 1) % n], rb[k]])


def glacis(p, plan, dl0, dl1, z0, z1, m='Body#pnl', plates=1, skip=(), core='Body#pnl', bolts=3, plate_w=None):
    """A saia inclinada: o miolo (o loft do contorno dl0 em z0 até dl1 em z1, aberto em cima e embaixo) coberto de CHAPAS de blindagem
    no plano inclinado ('plates' por face, ou uma a cada plate_w m), com a fileira de parafusos na borda de cima."""
    p.loft(core, [plan.ring(dl0, z0), plan.ring(dl1, z1)], bevel=ub._bev(core), caps=(False, False))
    for k in range(plan.n):
        if k in skip:
            continue
        a0, eu, en, w0 = plan.face(k, dl0)
        a1, _, _, w1 = plan.face(k, dl1)
        mid0 = a0 + eu * (w0 / 2) + Vector((0, 0, z0))
        mid1 = a1 + eu * (w1 / 2) + Vector((0, 0, z1))
        ev = (mid1 - mid0).normalized()
        nn = eu.cross(ev).normalized()
        if nn.dot(en) < 0:
            nn = -nn
        L = (mid1 - mid0).length
        mid = (mid0 + mid1) / 2
        w = min(w0, w1) - 0.3
        np_ = plates if plate_w is None else max(1, int(round(w / plate_w)))
        pw = w / np_
        for i in range(np_):
            q = mid + eu * (-w / 2 + pw * (i + 0.5)) + nn * 0.02
            ub.obox(p, m, q, (pw - 0.08, L - 0.16, 0.12), eu, ev, nn)
            for t in ([-0.32, 0.0, 0.32] if bolts == 3 else [-0.3, 0.3]):
                vp.nut(p, 'Dark', q + eu * (t * pw) + ev * (L / 2 - 0.2) + nn * 0.08, nn, r=0.035, h=0.025)


def obox_open(p, m, c, size, ex, ey, ez):
    """A caixa do ub.obox SEM a face de trás (−ez): o fundo da placa enterrado no miolo e o lado de dentro do miolo nunca aparecem, e
    na grade do detalhe custavam ~25 triângulos/m² cada."""
    b = p.bm(m, ub._bev(m))
    c, ex, ey, ez = Vector(c), Vector(ex), Vector(ey), Vector(ez)
    vs = [b.verts.new(V(*(c + ex * (size[0] / 2 * i) + ey * (size[1] / 2 * j) + ez * (size[2] / 2 * k))))
          for k in (-1, 1) for j in (-1, 1) for i in (-1, 1)]
    for f in [(4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        b.faces.new([vs[i] for i in f])


def panel_wall_open(p, origin, eu, ev, en, w, h, pw=2.0, ph=None, m='Body#pnl', gap=0.06, proud=0.07, embed=0.05, holes=(),
                    min_piece=0.3, rows=None, row_mats=None):
    """O ub.panel_wall com as placas sem o fundo (obox_open): a mesma parede."""
    O, eu, ev, en = Vector(origin), Vector(eu), Vector(ev), Vector(en)
    nu = max(1, int(round(w / pw)))
    us = [w * i / nu for i in range(nu + 1)]
    if rows is None:
        ph = ph or h
        nv = max(1, int(round(h / ph)))
        vs = [h * j / nv for j in range(nv + 1)]
    else:
        vs = [0.0] + [r for r in rows if 0 < r < h] + [h]
    g = gap / 2
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            a = us[i] + (g if i > 0 else 0.0)
            b = us[i + 1] - (g if i < len(us) - 2 else 0.0)
            c = vs[j] + (g if j > 0 else 0.0)
            d = vs[j + 1] - (g if j < len(vs) - 2 else 0.0)
            for (a2, b2, c2, d2) in ub._rect_minus(a, b, c, d, holes, g):
                if b2 - a2 < min_piece or d2 - c2 < min_piece:
                    continue
                cc = O + eu * ((a2 + b2) / 2) + ev * ((c2 + d2) / 2) + en * ((proud - embed) / 2)
                obox_open(p, (row_mats or {}).get(j, m), cc, (b2 - a2, d2 - c2, proud + embed), eu, ev, en)


def plan_core(p, plan, dl, z0, z1, holes=None, skip=(), m='Body#pnl', t=0.25):
    """O miolo das paredes com os vãos recortados (a parede montada em volta do vão, sem booleano): cada face em retângulos inteiros
    (holes[k] = [(u0, u1, v0, v1)] a partir da quina de começo e de z0), cada retângulo um sólido de espessura t pra dentro, 2 mm maior
    em volta (as peças vizinhas se cruzam: nenhum vértice coincide, o remove_doubles não solda)."""
    e = 0.002
    for k in range(plan.n):
        if k in skip:
            continue
        a, eu, en, w = plan.face(k, dl)
        for (u0, u1, v0, v1) in ub._rect_minus(0.0, w, 0.0, z1 - z0, (holes or {}).get(k, ()), 0.0):
            c = a + eu * ((u0 + u1) / 2) + Vector((0, 0, z0 + (v0 + v1) / 2)) - en * (t / 2)
            obox_open(p, m, c, (u1 - u0 + 2 * e, v1 - v0 + 2 * e, t), eu, Z, en)


def plan_walls(p, plan, dl, z0, z1, skip=(), rows=(0.55, 1.65, 2.3), blue=2, pw=2.4, holes=None, m='Body#pnl', margin=0.3):
    """As paredes de PLACAS em cada face (a junta funda, a fileira 'blue' em azul: a faixa do jogador feita de placas). holes[k] = os
    vãos da face k [(u0, u1, v0, v1)] em metros a partir da quina de começo + margin e de z0."""
    for k in range(plan.n):
        if k in skip:
            continue
        a, eu, en, w = plan.face(k, dl)
        if w - 2 * margin < 0.6:
            continue
        org = a + eu * margin + Vector((0, 0, z0))
        ww = w - 2 * margin
        panel_wall_open(p, org, eu, Z, en, ww, z1 - z0, pw=ww / max(1, round(ww / pw)), m=m, gap=0.07, proud=0.08, embed=0.04,
                      rows=list(rows), row_mats={blue: 'Player#pnl'} if blue is not None else None,
                      holes=(holes or {}).get(k, ()))


def plan_pillars(p, plan, dl, z0, z1, r=0.3, skip=(), m='Body#vp'):
    """Os pilares redondos nas quinas (o ritmo de pé da War Factory): a base, o fuste, o capitel."""
    h = z1 - z0
    for k in range(plan.n):
        if k in skip:
            continue
        q = plan.corner(k, dl)
        vp.lathe(p, m, (q.x, q.y, z0), 'z', [(0.0, 0.0), (r + 0.06, 0.0), (r + 0.06, 0.1), (r, 0.16), (r, h - 0.2), (r + 0.04, h - 0.14),
                                             (r + 0.04, h), (0.0, h)], seg=16)


def curved_panels(p, m, c, r0, r1, z0, z1, n, gap=0.07, phase=0.0, k=4, skip=()):
    """As placas curvas (o vaso, o tambor): n setores de anel com a junta 'gap' entre eles, cada um um sólido (a face de fora lisa)."""
    C = Vector(c)
    ga = gap / r1
    b = p.bm(m, ub._bev(m))
    for i in range(n):
        if i in skip:
            continue
        a0 = phase + 2 * math.pi * i / n + ga / 2
        a1 = phase + 2 * math.pi * (i + 1) / n - ga / 2
        rings = []
        for j in range(k + 1):
            a = a0 + (a1 - a0) * j / k
            d = Vector((math.cos(a), math.sin(a), 0.0))
            rings.append([b.verts.new(V(*(C + d * r + Vector((0, 0, z))))) for (r, z) in ((r0, z0), (r1, z0), (r1, z1), (r0, z1))])
        for j in range(k):
            for e in range(4):
                f = b.faces.new([rings[j][e], rings[j][(e + 1) % 4], rings[j + 1][(e + 1) % 4], rings[j + 1][e]])
                f.smooth = e in (1, 3)
        b.faces.new(rings[0][::-1])
        b.faces.new(rings[-1])


def slab_tiles(p, x0, x1, y0, y1, nx, ny, z0=-0.12, z1=0.12, m='Concrete#pnl', joint=0.06):
    """O chão em lajes (nx × ny) com a junta funda escura entre elas (a do Airfield B)."""
    xs = [x0 + (x1 - x0) * i / nx for i in range(nx + 1)]
    ys = [y0 + (y1 - y0) * j / ny for j in range(ny + 1)]
    g = joint / 2
    for i in range(nx):
        for j in range(ny):
            ub.slab(p, m, xs[i] + (g if i else 0.0), xs[i + 1] - (g if i < nx - 1 else 0.0), ys[j] + (g if j else 0.0),
                    ys[j + 1] - (g if j < ny - 1 else 0.0), z0, z1)
    for x in xs[1:-1]:
        ub.obox(p, 'Dark#vp', (x, (y0 + y1) / 2, z1 - 0.05), (joint + 0.02, y1 - y0, 0.06), X, Y, Z)
    for y in ys[1:-1]:
        ub.obox(p, 'Dark#vp', ((x0 + x1) / 2, y, z1 - 0.05), (x1 - x0, joint + 0.02, 0.06), X, Y, Z)


def louvers(p, c, n_face, along, L, H, n, frame='Trim', slat='Trim', back='Dark', tilt=35.0):
    """A veneziana do kit (ub.louvers) com os montantes 4 mm mais curtos: no kit as quinas da moldura coincidem e o remove_doubles
    soldava as barras (NONMANIFOLD no Trim#sm). Troca a do kit também (o fan_box dela chama esta)."""
    en = Vector(n_face).normalized()
    eu = Vector(along).normalized()
    ev = en.cross(eu).normalized()
    if ev.z < 0:
        ev = -ev
    C = Vector(c)
    vp.prism(p, back + '#vp', vp.rounded_rect(L + 0.02, H + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.012)
    fw = 0.05
    for sgn in (-1, 1):
        ub.obox(p, frame + '#sm', C + ev * (sgn * (H / 2 + fw / 2)) + en * 0.025, (L + 2 * fw, fw, 0.05), eu, ev, en)
        ub.obox(p, frame + '#sm', C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.025, (fw, H - 0.004, 0.05), eu, ev, en)
    step = H / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        ub.obox(p, slat + '#sm', C + ev * (-H / 2 + step * (k + 0.5)) + en * 0.02, (L, step * 0.85, 0.014), eu, e2, e3)


ub.louvers = louvers


def fan_box(p, c, size, fans=1, m='Body', z0=0.0, R=None):
    """A caixa de ventiladores do kit (ub.fan_box) com as tampas dos 2 ventiladores 2 mm mais estreitas: no kit as duas tampas se
    encostavam com os vértices no mesmo lugar (NONMANIFOLD no Body#sm). Troca a do kit."""
    x, y = c
    sx, sy, sz = size
    t = 0.46
    ub.obox(p, m + '#pnl', (x, y, z0 + 0.08 + (sz - t + 0.02 - 0.08) / 2), (sx - 0.08, sy - 0.08, sz - t + 0.02 - 0.08), X, Y, Z)
    for sxx in (-1, 1):
        for syy in (-1, 1):
            ub.obox(p, 'Trim#sm', (x + sxx * (sx / 2 - 0.2), y + syy * (sy / 2 - 0.2), z0 + 0.05), (0.3, 0.3, 0.1), X, Y, Z)
    R = R or min(sy * 0.38, sx / (2 * fans) * 0.78)
    for k in range(fans):
        cx = x + (k - (fans - 1) / 2) * sx / fans
        ub.square_annulus(p, m + '#sm', (cx, y), sx / fans / 2 - 0.002, sy / 2, R, z0 + sz, t)
        ub.fan(p, (cx, y), R, z0 + sz, depth=t - 0.04)
    for s in (-1, 1):
        hb = sz - t - 0.08
        louvers(p, (x, y + s * (sy / 2 - 0.04), z0 + 0.08 + hb / 2), (0, s, 0), (1, 0, 0), sx * 0.7, hb * 0.6, max(3, int(hb * 0.6 / 0.09)))


ub.fan_box = fan_box


def ring_nuts(p, c, R, z, count, r=0.035, h=0.025, m='Dark', phase=0.0):
    """O anel de porcas na face de fora de um cilindro (os anéis do vaso, o tambor)."""
    for k in range(count):
        a = 2 * math.pi * k / count + phase
        d = Vector((math.cos(a), math.sin(a), 0.0))
        vp.nut(p, m, Vector(c) + d * R + Vector((0, 0, z)), d, r=r, h=h)


def scaffold_poly(name, pts, h, out=1.0, skip=(), post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra ao longo do contorno (o do Strategy Center B, copiado)."""
    p = Part(name)
    top = h + 1.0
    levels = max(2, int((top - z0) / lift))
    n = len(pts)
    cx, cy = sum(q[0] for q in pts) / n, sum(q[1] for q in pts) / n
    for i in range(n):
        if i in skip:
            continue
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        ox, oy = dy, -dx
        if ox * ((ax + bx) / 2 - cx) + oy * ((ay + by) / 2 - cy) < 0:
            ox, oy = -ox, -oy
        yaw = math.degrees(math.atan2(dy, dx))
        m = max(1, int(L / bay))
        for k in range(m + 1):
            t = k / m
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            for d in (0.0, out):
                p.cyl(post, (px + ox * d, py + oy * d, (z0 + top) / 2), 'z', 0.05, top - z0, seg=8, bevel=0.0)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            mx, my = (ax + bx) / 2, (ay + by) / 2
            for d in (0.0, out):
                p.cyl(post, (mx + ox * d, my + oy * d, z), (dx, dy, 0.0), 0.045, L, seg=8, bevel=0.0)
            p.obox(plank, (mx + ox * out / 2, my + oy * out / 2, z - 0.03), (L, abs(out) - 0.1, 0.06), yaw=yaw, bevel=0.01)
            p.obox(stripe, (mx + ox * out, my + oy * out, z + 0.09), (L, 0.04, 0.18), yaw=yaw, bevel=0.0)
        for k in range(m):
            t0, t1 = k / m, (k + 1) / m
            pa = Vector((ax + (bx - ax) * t0 + ox * out, ay + (by - ay) * t0 + oy * out, z0 + (top - z0) * ((k % levels) / levels)))
            pb = Vector((ax + (bx - ax) * t1 + ox * out, ay + (by - ay) * t1 + oy * out, z0 + (top - z0) * ((k % levels + 1) / levels)))
            d = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (d.x, d.y, d.z), 0.035, d.length, seg=6, bevel=0.0)
    return p.finish()


def check(objs, hx, hy, at=None):
    """As medidas de cada peça no quadro da Unreal (o y volta pro sinal dela); avisa o que sai do lote (o chanfro que explodiu também
    aparece aqui: vértice a quilômetros). 'at' = {nome: (x, y, z)} a posição de repouso das peças com a origem no eixo."""
    for o in objs:
        dx, dy, dz = (at or {}).get(o.name, (0.0, 0.0, 0.0))
        xs = [v.co.x + dx for v in o.data.vertices]
        ys = [-v.co.y + dy for v in o.data.vertices]
        zs = [v.co.z + dz for v in o.data.vertices]
        box = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
        ok = box[0] >= -hx - 1e-3 and box[1] <= hx + 1e-3 and box[2] >= -hy - 1e-3 and box[3] <= hy + 1e-3
        print('MODEL caixa %s: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f %s' % ((o.name,) + tuple(box) + ('(no lote)' if ok else
                                                                                                    'FORA DO LOTE',)))


def preview_spec(out, parts, views, usa_at, game, scaffold=None, tank_views=False):
    import json
    spec = {'parts': parts, 'views': views, 'usa_at': list(usa_at), 'game': game}
    if scaffold:
        spec['scaffold'] = scaffold
    if tank_views:
        spec['tank_views'] = True
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


# ===================================================================================================== a usina
HX, HY = 4.64, 6.32               # o lote 9,28 × 12,64
FLOOR = 0.12                      # o topo da laje de fundação
RC = Vector((0.0, 0.0, 0.0))      # o eixo do reator
R_V = 2.65                        # o vaso (o miolo)
R_RING = 2.94                     # o raio de fora dos anéis
PED = Plan((0.0, 0.0), [(30 * k, 3.55) for k in range(12)])     # a saia de 12 faces (a face 0 olha pra +X)
PED_Z = 1.5                       # o topo do pedestal
RINGS = [(1.5, 1.92), (3.55, 4.0), (4.45, 4.9), (5.95, 6.4)]      # os 4 anéis (z0, z1); a faixa acesa entre o 2º e o 3º
TOP_Z = 6.4                       # o topo do vaso (a passarela)
DOME_H = 1.08
WING_Y0, WING_Y1, WING_HX = 3.38, 5.0, 2.55        # as alas: |y| de 3,38 a 5,0 (a saia até 5,25), x ±2,55
WING_SKIRT = (0.25, 0.62)                          # a saia de chapas das alas: o avanço no pé e a altura
WING_Z = 2.3                      # o topo do teto das alas
VENTS = [(-1.15, -4.19), (1.15, -4.19)]            # as bocas de vapor (x, y)
VENT_TOP = WING_Z + 1.72


def foundation(p):
    slab_tiles(p, -4.3, 4.3, -5.9, 5.9, 3, 4)


def pedestal(p):
    glacis(p, PED, 0.0, -0.45, FLOOR - 0.02, PED_Z - 0.08, skip=(0,))
    poly_slab(p, 'Concrete#pnl', PED, -0.37, PED_Z - 0.2, PED_Z, cell=1.4)


def vessel(p):
    c = (RC.x, RC.y, 0.0)
    # o miolo escuro (aparece só nas juntas das placas e atrás das grades da faixa acesa)
    vp.lathe(p, 'Dark#vp', c, 'z', [(R_V, PED_Z - 0.1), (R_V, TOP_Z)], seg=48)
    # as placas curvas: a fileira de baixo, a do meio, a azul em cima (a cor do jogador)
    curved_panels(p, 'Body#pnl', c, R_V - 0.04, R_V + 0.1, RINGS[0][1] + 0.03, RINGS[1][0] - 0.03, 12, phase=math.radians(15))
    curved_panels(p, 'Body#pnl', c, R_V - 0.04, R_V + 0.1, RINGS[2][1] + 0.03, 5.38, 12, phase=math.radians(15))
    curved_panels(p, 'Player#pnl', c, R_V - 0.04, R_V + 0.1, 5.44, RINGS[3][0] - 0.03, 12, phase=math.radians(0))
    # os 4 anéis de aço torneados com o anel de porcas
    for z0, z1 in RINGS:
        vp.lathe(p, 'Trim#vp', c, 'z', [(R_V - 0.03, z0), (R_RING - 0.05, z0), (R_RING, z0 + 0.06), (R_RING, z1 - 0.06), (R_RING - 0.05, z1),
                                        (R_V - 0.03, z1)], seg=64, closed=True)
        ring_nuts(p, c, R_RING, (z0 + z1) / 2, 32, r=0.035, h=0.025)
    # a faixa acesa (o núcleo vivo) entre o 2º e o 3º anel, atrás das 24 grades
    vp.lathe(p, 'Core#vp', c, 'z', [(R_V + 0.02, RINGS[1][1] - 0.02), (R_V + 0.02, RINGS[2][0] + 0.02)], seg=64)
    for k in range(24):
        a = 2 * math.pi * (k + 0.5) / 24
        d = Vector((math.cos(a), math.sin(a), 0.0))
        ub.obox(p, 'Trim#sm', d * (R_V + 0.1) + Vector((0, 0, (RINGS[1][1] + RINGS[2][0]) / 2)), (0.2, 0.09, RINGS[2][0] - RINGS[1][1] + 0.04),
                d, Vector((-d.y, d.x, 0)), Z)
    # a passarela de cima (o piso de grade, as mãos-francesas, o guarda-corpo redondo)
    vp.lathe(p, 'Body#vp', c, 'z', [(R_RING - 0.02, TOP_Z - 0.08), (3.42, TOP_Z - 0.08), (3.42, TOP_Z), (R_RING - 0.02, TOP_Z)], seg=64,
             closed=True)
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        d = Vector((math.cos(a), math.sin(a), 0.0))
        e = Vector((-d.y, d.x, 0.0))
        vp.prism(p, 'Trim#vp', [(0.0, 0.0), (0.45, 0.0), (0.0, -0.5)], d * (R_RING - 0.02) + Vector((0, 0, TOP_Z - 0.08)) - e * 0.03, d, Vector(Z), e,
                 0.06)
    pts = [(3.34 * math.cos(2 * math.pi * k / 24 + 0.13), 3.34 * math.sin(2 * math.pi * k / 24 + 0.13)) for k in range(24)]
    ub.rail(p, 'Trim', pts + [pts[0]], TOP_Z, h=1.0, r=0.028, step=1.2, toe='Trim')
    # a escada de marinheiro com a gaiola (frente-esquerda), do pedestal até a passarela
    a = math.radians(-58)
    d = Vector((math.cos(a), math.sin(a), 0.0))
    q = d * R_RING
    ub.ladder(p, 'Trim', q.x, q.y, PED_Z, TOP_Z, (d.x, d.y, 0.0), w=0.5, cage=True, wall=0.16, top_ext=1.0)


def dome(p):
    c = (RC.x, RC.y, 0.0)
    # o domo elíptico (2:1) com os pontos a cada ~0,35 m (a grade fica de quadrados)
    prof = []
    for i in range(10):
        t = (math.pi / 2) * i / 9
        prof.append((R_V * math.cos(t), TOP_Z - 0.02 + DOME_H * math.sin(t)))
    prof[-1] = (0.0, prof[-1][1])
    vp.lathe(p, 'Body#vp', c, 'z', prof, seg=48)
    # as 12 nervuras do domo (de cima: os gomos)
    for k in range(12):
        a = 2 * math.pi * k / 12
        d = Vector((math.cos(a), math.sin(a), 0.0))
        rib = []
        for i in range(8):
            r = R_V - 0.02 - (R_V - 1.75) * i / 7
            zz = TOP_Z - 0.02 + DOME_H * math.sqrt(max(0.0, 1 - (r / R_V) ** 2))
            rib.append(Vector((c[0], c[1], 0)) + d * r + Vector((0, 0, zz + 0.03)))
        vp.tube(p, 'Trim', rib, 0.05, seg=8)
    # o cubo da tampa, a flange com os parafusos
    zc = TOP_Z - 0.02 + DOME_H
    vp.lathe(p, 'Trim#vp', c, 'z', [(0.0, zc - 0.3), (0.95, zc - 0.3), (0.95, zc - 0.12), (0.9, zc - 0.06), (0.62, zc - 0.06), (0.62, zc + 0.32),
                                    (0.56, zc + 0.38), (0.0, zc + 0.38)], seg=40)
    vp.bolt_ring(p, 'Dark', (c[0], c[1], zc - 0.06), 'z', 0.8, 16, r=0.03, h=0.02)
    # a coroa: os 12 mecanismos das barras de controle (o tubo torneado, o colar, a tampa escura) e o aro que os amarra
    Rc = 1.42
    zb = TOP_Z - 0.02 + DOME_H * math.sqrt(1 - (Rc / R_V) ** 2) - 0.12
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        x, y = c[0] + Rc * math.cos(a), c[1] + Rc * math.sin(a)
        vp.lathe(p, 'Body#vp', (x, y, zb), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.1), (0.17, 0.16), (0.17, 0.82), (0.21, 0.85), (0.21, 0.95),
                                                 (0.17, 0.98), (0.0, 0.98)], seg=16)
        vp.lathe(p, 'Dark#vp', (x, y, zb + 0.98), 'z', [(0.0, -0.01), (0.16, -0.01), (0.16, 0.06), (0.1, 0.12), (0.0, 0.13)], seg=12)
    p.torus('Trim#vp', (c[0], c[1], zb + 0.9), 'z', Rc, 0.06, seg=48, rseg=8)
    for k in range(6):                                   # os 6 braços do aro até o cubo
        a = 2 * math.pi * k / 6
        d = Vector((math.cos(a), math.sin(a), 0.0))
        vp.tube(p, 'Trim', [Vector((c[0], c[1], zc + 0.3)) + d * 0.5, Vector((c[0], c[1], zb + 0.9)) + d * (Rc - 0.05)], 0.04, seg=8)
    ub.beacon(p, (c[0], c[1], zc + 0.38), r=0.1, h=0.2)


def airlock(p):
    """A eclusa da frente: o bloco de placas, a porta recuada com as nervuras, a moldura de perigo, a placa azul, a marquise, o
    holofote, os frades."""
    x0, x1, hw, zt = 2.3, 3.62, 1.15, 2.95
    ub.obox(p, 'Body#pnl', ((x0 + x1) / 2 - 0.05, 0.0, (FLOOR + zt) / 2), (x1 - x0 - 0.1, 2 * hw - 0.1, zt - FLOOR), X, Y, Z)
    for s in (-1, 1):                                    # as paredes dos lados em placas
        ub.panel_wall(p, (x0, s * hw, FLOOR), (1, 0, 0), Z, (0, s, 0), x1 - x0 - 0.12, zt - FLOOR - 0.05, pw=1.2, m='Body#pnl', gap=0.07,
                      proud=0.08, embed=0.04, rows=[0.6, 1.6, 2.1], row_mats={2: 'Player#pnl'})
    # a frente: os 2 montantes e a verga de placas em volta do vão
    for s in (-1, 1):
        ub.obox(p, 'Body#pnl', (x1 + 0.02, s * 0.88, (FLOOR + zt) / 2), (0.16, 0.5, zt - FLOOR + 0.02), X, Y, Z)
    ub.obox(p, 'Body#pnl', (x1 + 0.02, 0.0, zt - 0.32), (0.16, 2 * hw + 0.02, 0.62), X, Y, Z)
    ub.obox(p, 'Dark#vp', (x1 - 0.12, 0.0, (FLOOR + 2.3) / 2), (0.1, 1.26, 2.3 - FLOOR), X, Y, Z)
    ub.obox(p, 'Trim#sm', (x1 - 0.06, 0.0, (FLOOR + 2.24) / 2 + 0.01), (0.08, 1.16, 2.24 - FLOOR - 0.04), X, Y, Z)
    for k in range(4):
        ub.obox(p, 'Dark#sm', (x1 - 0.005, 0.0, FLOOR + 0.4 + k * 0.48), (0.05, 1.0, 0.1), X, Y, Z)
    vp.grab_handle(p, 'Body', (x1 + 0.0, 0.42, 1.0), (x1 + 0.0, 0.42, 1.45), (1, 0, 0), height=0.06, r=0.02)
    for s in (-1, 1):                                    # a moldura de perigo
        ub.obox(p, 'Hazard#sm', (x1 + 0.11, s * 0.7, FLOOR + 0.5), (0.04, 0.14, 0.9), X, Y, Z)
    ub.obox(p, 'Hazard#sm', (x1 + 0.2, 0.0, FLOOR + 0.012), (0.42, 1.5, 0.03), X, Y, Z)
    ub.obox(p, 'Player#sm', (x1 + 0.11, 0.0, zt - 0.32), (0.04, 1.2, 0.36), X, Y, Z)
    # a marquise e o holofote
    ub.obox(p, 'Trim#sm', (x1 + 0.4, 0.0, 2.48), (0.75, 1.7, 0.08), X, Y, Z)
    for s in (-1, 1):
        vp.tube(p, 'Trim', [(x1 + 0.1, s * 0.75, 2.75), (x1 + 0.72, s * 0.75, 2.5)], 0.025, seg=6)
    ub.lamp_head(p, (x1 + 0.3, 0.0, 2.32), (1.0, 0.0, -1.2), w=0.4, h=0.22)
    # o teto da eclusa (a laje) e os frades
    ub.obox(p, 'Trim#sm', ((x0 + x1) / 2 + 0.05, 0.0, zt + 0.04), (x1 - x0 + 0.22, 2 * hw + 0.14, 0.1), X, Y, Z)
    for y in (-1.45, 1.45):
        vp.lathe(p, 'Hazard#vp', (4.2, y, FLOOR), 'z', [(0.0, 0.0), (0.15, 0.0), (0.15, 0.8), (0.11, 0.87), (0.0, 0.89)], seg=14)


def wing(s):
    """O contorno da ala do lado s (−1 = o vapor, +1 = a força): o retângulo com as quinas de fora cortadas."""
    yc = s * (WING_Y0 + WING_Y1) / 2
    hy = (WING_Y1 - WING_Y0) / 2
    ch = (0.5, 0.5, 0.0, 0.0) if s > 0 else (0.0, 0.0, 0.5, 0.5)
    return chamfer_rect((0.0, yc), WING_HX, WING_HX, hy, hy, ch)


def wings(p):
    for s in (-1, 1):
        pl = wing(s)
        inner = [k for k in range(pl.n) if (pl.nrm(k).y * s) < -0.9]         # a face de dentro (encosta no reator)
        # a saia blindada de chapas (a linguagem do pedestal) e o miolo da parede
        glacis(p, pl, WING_SKIRT[0], 0.0, FLOOR - 0.02, WING_SKIRT[1], skip=inner, plate_w=1.4, bolts=2)
        p.loft('Body#pnl', [pl.ring(0.0, WING_SKIRT[1] - 0.04), pl.ring(0.0, WING_Z - 0.1)], bevel=ub._bev('Body#pnl'), caps=(False, False))
        holes = {}
        if s < 0:
            out_k = [k for k in range(pl.n) if pl.nrm(k).y < -0.9][0]
            a, eu, en, w = pl.face(out_k)
            holes[out_k] = [((w - 0.6) / 2 - 1.3, (w - 0.6) / 2 + 1.3, 1.33 - 0.4 - WING_SKIRT[1] - 0.05, 1.33 + 0.4 - WING_SKIRT[1] + 0.05)]
        plan_walls(p, pl, 0.0, WING_SKIRT[1], WING_Z - 0.2, skip=inner, rows=(0.55, 0.92), blue=1, pw=1.4, holes=holes)
        poly_band(p, 'Body#pnl', pl, 0.1, -0.22, WING_Z - 0.3, WING_Z + 0.32)
        poly_band(p, 'Trim#sm', pl, 0.16, -0.28, WING_Z + 0.32, WING_Z + 0.4)
        poly_slab(p, 'Concrete#pnl', pl, -0.2, WING_Z - 0.3, WING_Z, cell=1.4)
        # as luminárias com o corpo na face da frente
        a, eu, en, w = pl.face(0)
        q = a + eu * (w / 2) + Vector((0.08, 0, 1.75))
        ub.obox(p, 'Dark#sm', q + en * 0.1, (0.18, 0.55, 0.2), en, eu, Z)
        ub.obox(p, 'Light#vp', q + en * 0.19 - Vector((0, 0, 0.05)), (0.012, 0.45, 0.09), en, eu, Z)
        ub.obox(p, 'Dark#sm', q + en * 0.2 + Vector((0, 0, 0.12)), (0.2, 0.6, 0.025), en, eu, Z)
    # a ala do vapor (−Y): a veneziana da tomada de ar, as 2 chaminés curtas, os 2 canos grossos até o vaso
    louvers(p, (0.0, -WING_Y1 - 0.06, 1.33), (0, -1, 0), (1, 0, 0), 2.4, 0.8, 9)
    for (vx, vy) in VENTS:
        vp.lathe(p, 'Body#vp', (vx, vy, WING_Z), 'z', [(0.0, 0.0), (0.53, 0.0), (0.53, 0.12), (0.46, 0.2), (0.44, 1.38), (0.52, 1.46),
                                                       (0.52, 1.68), (0.44, 1.72), (0.38, 1.66), (0.38, 1.1), (0.0, 1.1)], seg=28)
        vp.lathe(p, 'Dark#vp', (vx, vy, WING_Z), 'z', [(0.375, 1.67), (0.375, 1.12), (0.0, 1.12)], seg=28)
        vp.lathe(p, 'Player#vp', (vx, vy, WING_Z), 'z', [(0.43, 0.9), (0.465, 0.9), (0.465, 1.12), (0.43, 1.12)], seg=28, closed=True)
        vp.bolt_ring(p, 'Dark', (vx, vy, WING_Z + 0.12), 'z', 0.49, 10, r=0.03, h=0.02)
    for x in (-0.3, 0.3):
        ub.pipe(p, 'Trim', [(x, -4.45, WING_Z - 0.05), (x, -4.45, 2.95), (x, -(R_V - 0.02), 2.95)], 0.16, step=1.2)
    # a ala da força (+Y): o transformador no teto (o tanque, as aletas, os 3 isoladores) e os 3 cabos até o vaso
    tx, ty = -0.9, (WING_Y0 + WING_Y1) / 2
    ub.obox(p, 'Trim#sm', (tx, ty, WING_Z + 0.06), (1.7, 1.1, 0.12), X, Y, Z)
    ub.obox(p, 'Body#pnl', (tx, ty, WING_Z + 0.62), (1.5, 0.8, 1.0), X, Y, Z)
    for k in range(9):
        xx = tx - 0.64 + k * 0.16
        for sy in (-1, 1):
            ub.obox(p, 'Trim#sm', (xx, ty + sy * 0.47, WING_Z + 0.62), (0.05, 0.16, 0.82), X, Y, Z)
    ub.obox(p, 'Player#sm', (tx + 0.77, ty, WING_Z + 0.8), (0.03, 0.6, 0.18), X, Y, Z)
    for k in range(3):
        bx = tx - 0.5 + k * 0.5
        prof = [(0.0, 0.0), (0.16, 0.0), (0.16, 0.06)]
        for j in range(6):
            z = 0.1 + j * 0.13
            prof += [(0.11, z), (0.2, z + 0.03), (0.2, z + 0.07), (0.11, z + 0.1)]
        prof += [(0.09, 0.92), (0.09, 1.02), (0.0, 1.04)]
        vp.lathe(p, 'Paint#vp', (bx, ty, WING_Z + 1.12), 'z', prof, seg=16)
        top = Vector((bx, ty, WING_Z + 2.14))
        end = Vector((bx * 0.6, R_V + 0.12, 3.15))
        mid = (top + end) / 2 + Vector((0.0, 0.0, -0.35))
        vp.tube(p, 'Rubber', vp.bent_bar([top, top + Vector((0, -0.25, 0.12)), mid, end], R=0.25, n=4), 0.055, seg=8)
        vp.lathe(p, 'Trim#vp', end, (0, -1, 0), [(0.0, -0.05), (0.1, -0.05), (0.1, 0.12), (0.0, 0.12)], seg=10)
    ub.tarp_cover_at(p, (1.15, ty), 1.6, 1.0, 0.0, WING_Z + 0.06, 0.5, m='Player', seed=1.3, ties=2, box=12)
    ub.obox(p, 'Body#sm', (1.15, ty, WING_Z + 0.28), (1.4, 0.82, 0.44), X, Y, Z)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    foundation(p)
    pedestal(p)
    vessel(p)
    dome(p)
    airlock(p)
    wings(p)
    return ub.finish(p)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_power_plant_v2')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    sc = scaffold_poly(PREFIX + '_Scaffold', [(-3.58, -5.26), (3.58, -5.26), (3.58, 5.26), (-3.58, 5.26)], 7.0, out=1.0)
    check([b, sc], HX, HY)
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, sc)})
    total = export(b, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    zc = TOP_Z - 0.02 + DOME_H
    print('MODEL fire_points_m', [[0.0, 0.0, round(zc, 2)], [0.0, -4.2, WING_Z], [0.2, 4.2, WING_Z]])
    print('MODEL vapor (bocas)', [[vx, vy, round(VENT_TOP, 2)] for vx, vy in VENTS])
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]]],
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1300]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1300, 900]],
                  ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
                 (1.5, 9.5, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
