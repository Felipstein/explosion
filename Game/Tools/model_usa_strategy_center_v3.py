"""O Strategy Center da USA (usa_strategy_center), VERSÃO 3: o PENTÁGONO como CENTRO DE INTELIGÊNCIA (01/11/2026).

O pedido (o dono, 01/11 17h55, vendo o Pentágono do Command Center v3 no jogo): "o pentágono com a bola ficou muito bom, dá pra usar
como o centro de inteligência, mas tem que melhorar ainda, tirar o portão, e dar uma melhorada, mudar algumas coisas, deixar mais
legal, mais icônico". Nasce do Game/Tools/model_usa_command_center_v3.py (o Pentágono do outro agente: as funções de parede, anel,
saia, poço e telhado copiadas de lá, com as medidas deste lote) e do campo de radomos (a geodésica de
Game/Tools/model_usa_strategy_center_v3_radomos.py, a versão guardada).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV, as máscaras e a bandeira no flags.json):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_strategy_center_v3.py C:/Users/luisf/explosion/captures/game/models/usa_strategy_center_v3

A PALAVRA: O PENTÁGONO COM A BOLA. De cima, em 1 segundo: o contorno de cinco lados, as linhas concêntricas (a fachada, o poço de luz
escuro, o anel de dentro claro com a capa AZUL, o pátio) e no meio a BOLA BRANCA geodésica de 7,5 m (as faces triangulares com a junta
escura), a maior coisa redonda da base; nas 2 alas de trás, as 2 radomos menores (3,1 m): o "campo de radomos" dentro do Pentágono.
O que muda do Command Center (a regra: cada prédio ICÔNICO e diferente):
  - SEM o portão blindado: a face da frente fechada como as outras (as placas, a fenda de vidro, a fileira azul), com a ENTRADA DE GENTE
    pequena no meio: a porta dupla recuada, a placa da cor do jogador, a marquise nos 2 pilares, as luminárias, os frades;
  - a torre do radar SPY sai: o fuste octogonal fica como a BASE DA BOLA (o colar em cone com a faixa azul e o friso escuro onde a
    esfera assenta), a bola com a luz de obstáculo (Beacon, pisca) e o para-raios no topo; sem o heliponto;
  - o anel de dentro MAIS MARCADO: 0,6 m mais alto (7,4 m), a fileira azul larga em volta, a capa do parapeito AZUL (de cima, o
    pentágono azul em volta da bola) e o teto claro (Paint) contra o poço escuro;
  - as ANTENAS DE ESCUTA nas 5 quinas: o mastro de 4,8 m sobre o pilar redondo, com as 4 cruzes de dipolos (de cima, um X em cada
    quina) e a luz de obstáculo no topo;
  - a SALA DE GUERRA: a claraboia no teto da ala da frente (o meio-fio, os vidros escuros inclinados, a grade aberta) com a MESA DO
    MAPA acesa em azul (UsCore, a luz azul do jogo), o terreno (Player), as setas e os marcadores do plano de batalha (Light);
  - no chão (sem laje quadrada: a borda de fundação segue o pentágono): o mastro da bandeira (o pano no vento é do jogo: flags.json),
    os postes de luz, o gerador e o tanque de combustível nos cantos de trás, os sacos de areia.
A USA limpa (13/10): sem desgaste pintado.

Lote (Units.json usa_strategy_center size_m): 20 × 20 m, a frente em +X. O Pentágono do Command Center (9,8 m do centro à face) não
cabe: este é o mesmo desenho com 7,9 m (80%), com as medidas de gente iguais (portas, janelas, corrimãos, antenas): o Command Center
continua o maior. O centro do pentágono em (0,55; 0): a fachada da frente em x = 8,45, a ponta de trás em −9,3.
Peças: SM_USA_StrategyCenter_Base (tudo; nada gira: a vida é a luz da mesa, as luzes de obstáculo e as sirenes piscando) e
SM_USA_StrategyCenter_Scaffold (o andaime da obra).
Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime), CivCloth (os sacos) e UsCore
(a luz azul da mesa do mapa: a instância do núcleo da usina da USA, que o import_provisional já cria).
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import CPart, Plan, poly_slab, poly_band, plan_pillars, louvers, check, preview_spec, X, Y, Z  # noqa: E402

PREFIX = 'SM_USA_StrategyCenter'
COLORS.update({'CivCloth': (158, 132, 98), 'Rubber': (32, 32, 31), 'UsCore': (90, 160, 255)})

# ---------------------------------------------------------------------------------- as medidas
HX = HY = 10.0                   # o lote 20 × 20
FLOOR = 0.12                     # o topo da laje
C0 = (0.55, 0.0)                 # o centro do pentágono
A_O = 7.9                        # do centro à fachada de fora (o Command Center: 9,8)
D_OI = -2.4                      # (recuo) a face de dentro do anel de fora: o poço de luz
D_IO = -3.4                      # a face de fora do anel de dentro
D_II = -5.2                      # a face do pátio (o pátio: 2,7 m do centro à face)
PENT = Plan(C0, [(72 * k, A_O) for k in range(5)])          # a face 0 olha pra +X; a quina 2 é a ponta de trás (−X)
SKIRT = (0.45, 1.5)              # a saia: o avanço no pé e a altura
SKIRTED = (1, 2, 3, 4)           # a frente (face 0) é reta (a entrada)
Z_O, Z_I = 5.4, 7.4              # os tetos dos anéis (o de dentro 0,6 m mais alto que no Command Center: o anel marcado)
XF = C0[0] + A_O                 # a fachada da frente (x = 8,45)
DOOR = (0.95, 2.45)              # a entrada de gente: a meia largura e o topo
O_ROWS = (2.6, 3.35, 4.3, 4.85)  # anel de fora: a fenda 2,6–3,35, o azul 4,3–4,85
I_ROWS = (5.85, 6.45)            # anel de dentro: a fenda 5,85–6,45, o azul de 6,45 até o topo (0,9 m)
# a bola grande (no pátio, sobre o fuste) e as 2 pequenas (nas alas 2 e 3, no teto de fora)
SHAFT_TOP = 7.8
BALL = {'c': C0, 'r': 3.75, 'rc': 2.45, 'zcut': 8.15, 'freq': 4}
SMALL_K = (2, 3)
SMALL = {'r': 1.55, 'rc': 0.85, 'RC': 1.0, 'zcut': 6.0, 'freq': 2}
for _d in (BALL, SMALL):
    _d['zc'] = _d['zcut'] + math.sqrt(_d['r'] ** 2 - _d['rc'] ** 2)
# a sala de guerra: a claraboia no teto da ala da frente (o centro, o comprimento ao longo da face e a profundidade)
SKY = (C0[0] + A_O + D_OI / 2 - 0.05, 0.0, 4.0, 1.6)


def oct_plan(a, c):
    """O octógono do fuste: as faces grandes (45°, 135°, ...) a 'a' do centro, as das quinas (0°, 90°, ...) a 'c'."""
    return Plan(C0, [(0, c), (45, a), (90, c), (135, a), (180, c), (225, a), (270, c), (315, a)])


SHAFT = oct_plan(1.9, 2.15)


# ---------------------------------------------------------------------------------- ajudantes (copiados do model_usa_command_center_v3.py)
def xcorner(plan, ki, di, kj, dj):
    """A quina entre a face ki recuada di e a face kj recuada dj (a saia que acaba contra a face reta da frente)."""
    n1, n2 = plan.nrm(ki), plan.nrm(kj)
    d1, d2 = plan.f[ki % plan.n][1] + di, plan.f[kj % plan.n][1] + dj
    det = n1.x * n2.y - n1.y * n2.x
    return plan.c + Vector(((d1 * n2.y - d2 * n1.y) / det, (n1.x * d2 - n2.x * d1) / det, 0.0))


def wing(k, d, u=0.0):
    """O ponto da ala k a 'd' do centro (pela normal da face) e 'u' ao longo dela (anti-horário); devolve (ponto, normal, tangente)."""
    n = PENT.nrm(k)
    t = Vector((-n.y, n.x, 0.0))
    return PENT.c + n * d + t * u, n, t


CELL = 0.39


def nseg(L):
    return max(1, math.ceil(L / CELL - 1e-6))


def gpatch(b, cache, P00, P10, P11, P01, nu, nv, smooth=False):
    """O quadrilátero (P00→P10 é u, P00→P01 é v) em nu × nv células; 'cache' liga as bordas com as faces vizinhas da mesma peça."""
    def vv(q):
        key = (round(q.x, 4), round(q.y, 4), round(q.z, 4))
        if key not in cache:
            cache[key] = b.verts.new(V(q.x, q.y, q.z))
        return cache[key]
    g = []
    for j in range(nv + 1):
        t = j / nv
        a, c = P00 + (P01 - P00) * t, P10 + (P11 - P10) * t
        g.append([vv(a + (c - a) * (i / nu)) for i in range(nu + 1)])
    for j in range(nv):
        for i in range(nu):
            f = b.faces.new([g[j][i], g[j][i + 1], g[j + 1][i + 1], g[j + 1][i]])
            f.smooth = smooth


def gbox(p, m, c, size, ex, ey, ez, open_back=True):
    """A caixa do ub.obox já quadriculada (sem a face de trás −ez quando open_back: o fundo enterrado no miolo)."""
    b = p.bm(m, ub._bev(m))
    c, ex, ey, ez = Vector(c), Vector(ex), Vector(ey), Vector(ez)
    n = [nseg(size[0]), nseg(size[1]), nseg(size[2])]

    def P(i, j, k):
        return c + ex * (size[0] / 2 * i) + ey * (size[1] / 2 * j) + ez * (size[2] / 2 * k)
    cache = {}
    gpatch(b, cache, P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), n[0], n[1])
    if not open_back:
        gpatch(b, cache, P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1), n[0], n[1])
    gpatch(b, cache, P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), n[1], n[2])
    gpatch(b, cache, P(-1, 1, -1), P(-1, -1, -1), P(-1, -1, 1), P(-1, 1, 1), n[1], n[2])
    gpatch(b, cache, P(1, 1, -1), P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), n[0], n[2])
    gpatch(b, cache, P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), n[0], n[2])


def ring_band(p, m, plan, dl_out, dl_in, z0, z1):
    """O anel no contorno (a laje do teto, o parapeito, a capa) já quadriculado, sólido fechado."""
    b = p.bm(m, ub._bev(m))
    cache = {}
    for k in range(plan.n):
        o0, o1 = plan.corner(k - 1, dl_out), plan.corner(k, dl_out)
        i0, i1 = plan.corner(k - 1, dl_in), plan.corner(k, dl_in)
        nu, nw, nh = nseg((o1 - o0).length), nseg((i0 - o0).length), nseg(z1 - z0)

        def P(q, z):
            return Vector((q.x, q.y, z))
        gpatch(b, cache, P(o0, z1), P(o1, z1), P(i1, z1), P(i0, z1), nu, nw)
        gpatch(b, cache, P(i0, z0), P(i1, z0), P(o1, z0), P(o0, z0), nu, nw)
        gpatch(b, cache, P(o0, z0), P(o1, z0), P(o1, z1), P(o0, z1), nu, nh)
        gpatch(b, cache, P(i0, z1), P(i1, z1), P(i1, z0), P(i0, z0), nu, nh)


def gcore(p, plan, dl, z0, z1, holes=None, skip=(), m='Body#vp', t=0.25, inward=False):
    """O miolo das paredes (com os vãos) já quadriculado; inward = a face visível pro centro (o poço, o pátio)."""
    e = 0.002
    for k in range(plan.n):
        if k in skip:
            continue
        a, eu, en, w = plan.face(k, dl)
        for (u0, u1, v0, v1) in ub._rect_minus(0.0, w, 0.0, z1 - z0, (holes or {}).get(k, ()), 0.0):
            c = a + eu * ((u0 + u1) / 2) + Vector((0, 0, z0 + (v0 + v1) / 2)) + en * ((t / 2) * (1 if inward else -1))
            gbox(p, m, c, (u1 - u0 + 2 * e, v1 - v0 + 2 * e, t), eu, Z, -en if inward else en)


def gwalls(p, plan, dl, z0, z1, skip=(), rows=(0.55, 1.65, 2.3), blue=2, pw=2.4, holes=None, m='Body#pnl', margin=0.3):
    """As placas com a junta funda e a fileira azul, já quadriculadas (os vãos 'holes' em u, v a partir do canto + margem)."""
    for k in range(plan.n):
        if k in skip:
            continue
        a, eu, en, w = plan.face(k, dl)
        if w - 2 * margin < 0.6:
            continue
        O = a + eu * margin + Vector((0, 0, z0))
        ww, h = w - 2 * margin, z1 - z0
        nu = max(1, round(ww / pw))
        us = [ww * i / nu for i in range(nu + 1)]
        vs = [0.0] + [r for r in rows if 0 < r < h] + [h]
        g = 0.035
        for i in range(len(us) - 1):
            for j in range(len(vs) - 1):
                a0 = us[i] + (g if i > 0 else 0.0)
                b0 = us[i + 1] - (g if i < len(us) - 2 else 0.0)
                c0 = vs[j] + (g if j > 0 else 0.0)
                d0 = vs[j + 1] - (g if j < len(vs) - 2 else 0.0)
                for (a2, b2, c2, d2) in ub._rect_minus(a0, b0, c0, d0, (holes or {}).get(k, ()), g):
                    if b2 - a2 < 0.3 or d2 - c2 < 0.3:
                        continue
                    cc = O + eu * ((a2 + b2) / 2) + Vector((0, 0, (c2 + d2) / 2)) + en * 0.02
                    gbox(p, 'Player#pnl' if (blue is not None and j == blue) else m, cc, (b2 - a2, d2 - c2, 0.12), eu, Z, en)


def stamp(p, build, at, yaw):
    """Monta peças alinhadas aos eixos numa peça de rascunho e carimba na p girada 'yaw' graus e posta em 'at' (x, y, z)."""
    q = Part('_stamp')
    ub.keys(q)
    build(q)
    M = Matrix.Translation(V(*at)) @ Matrix.Rotation(math.radians(-yaw), 4, 'Z')
    for key, src in q.bms.items():
        if len(src.faces):
            dst = p.bm(key, q.bevel[key])
            vmap = {v: dst.verts.new(M @ v.co) for v in src.verts}
            for f in src.faces:
                nf = dst.faces.new([vmap[v] for v in f.verts])
                nf.smooth = f.smooth
            for e in src.edges:
                if not e.smooth:
                    ne = dst.edges.get([vmap[e.verts[0]], vmap[e.verts[1]]])
                    if ne is not None:
                        ne.smooth = False
        src.free()


def walls_in(p, plan, dl, z0, z1, holes=None, skip=(), m='Body#vp', t=0.25):
    gcore(p, plan, dl, z0, z1, holes=holes, skip=skip, m=m, t=t, inward=True)


def slit(p, plan, dl, k, z0, z1, margin=0.85, step=1.15):
    """A fenda de vidro escuro na face k (o vidro no miolo, recuado atrás das placas) com os montantes e o peitoril."""
    a, eu, en, w = plan.face(k, dl)
    ww = w - 2 * margin
    mid = a + eu * (w / 2)
    ub.obox(p, 'Glass#vp', mid + en * 0.01 + Vector((0, 0, (z0 + z1) / 2)), (ww, z1 - z0, 0.04), eu, Z, en)
    n = max(2, int(round(ww / step)))
    for i in range(n + 1):
        u = -ww / 2 + ww * i / n
        ub.obox(p, 'Trim#vp', mid + eu * u + en * 0.05 + Vector((0, 0, (z0 + z1) / 2)), (0.12, z1 - z0 + 0.04, 0.1), eu, Z, en)
    gbox(p, 'Trim#sm', mid + en * 0.1 + Vector((0, 0, z0 - 0.04)), (ww + 0.1, 0.08, 0.14), eu, Z, en, open_back=False)


def slit_span(p, plan, dl, k, u0, u1, z0, z1, step=1.15):
    """A fenda de vidro só entre u0 e u1 (a partir do canto da face k): a frente, interrompida no meio pela entrada."""
    a, eu, en, w = plan.face(k, dl)
    ww = u1 - u0
    mid = a + eu * ((u0 + u1) / 2)
    ub.obox(p, 'Glass#vp', mid + en * 0.01 + Vector((0, 0, (z0 + z1) / 2)), (ww, z1 - z0, 0.04), eu, Z, en)
    n = max(2, int(round(ww / step)))
    for i in range(n + 1):
        u = -ww / 2 + ww * i / n
        ub.obox(p, 'Trim#vp', mid + eu * u + en * 0.05 + Vector((0, 0, (z0 + z1) / 2)), (0.12, z1 - z0 + 0.04, 0.1), eu, Z, en)
    gbox(p, 'Trim#sm', mid + en * 0.1 + Vector((0, 0, z0 - 0.04)), (ww + 0.1, 0.08, 0.14), eu, Z, en, open_back=False)


def wall_lamp(p, q, eu, en):
    ub.obox(p, 'Dark#sm', q + en * 0.17, (0.6, 0.18, 0.22), eu, en, Z)
    ub.obox(p, 'Light#vp', q + en * 0.27 - Vector((0, 0, 0.05)), (0.5, 0.012, 0.1), eu, en, Z)
    ub.obox(p, 'Dark#sm', q + en * 0.28 + Vector((0, 0, 0.13)), (0.64, 0.2, 0.025), eu, en, Z)


def sandbag_arc(p, c, R, a0, a1, layers=3):
    """O ninho de sacos de areia: o arco de raio R de a0 a a1 graus, em fiadas desencontradas."""
    for j in range(layers):
        rr = R - j * 0.04
        n = max(2, int(abs(math.radians(a1 - a0)) * rr / 0.56))
        for i in range(n):
            t = (i + (0.5 if j % 2 else 0.0) + 0.25) / n
            if t > 1.0:
                continue
            a = math.radians(a0 + (a1 - a0) * t)
            q = (c[0] + rr * math.cos(a), c[1] + rr * math.sin(a))
            vp.rbox(p, 'CivCloth#vp', (q[0], q[1], 0.09 + j * 0.17), (0.56, 0.34, 0.17), yaw=math.degrees(a) + 90, r=0.08, k=2)


# ---------------------------------------------------------------------------------- a geodésica (do campo de radomos)
def geodesic(freq):
    """Os triângulos da esfera geodésica (vetores unitários, um vértice no topo: a estrela de 5 que lê como bola de futebol)."""
    t = (1 + 5 ** 0.5) / 2
    v0 = [Vector(q).normalized() for q in ((-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t),
                                           (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1))]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
             (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    R = v0[0].rotation_difference(Vector((0, 0, 1))).to_matrix()
    v0 = [R @ q for q in v0]
    out = []
    for (a, b, c) in faces:
        A, B, C = v0[a], v0[b], v0[c]
        P = {}
        for i in range(freq + 1):
            for j in range(freq + 1 - i):
                P[i, j] = (A + (B - A) * (i / freq) + (C - A) * (j / freq)).normalized()
        for i in range(freq):
            for j in range(freq - i):
                out.append((P[i, j], P[i + 1, j], P[i, j + 1]))
                if i + j < freq - 1:
                    out.append((P[i + 1, j], P[i + 1, j + 1], P[i, j + 1]))
    return out


def ball(p, c, d, rod=0.9):
    """A radomo: o miolo escuro (o poliedro 7 cm pra dentro: a junta funda) e os PAINÉIS brancos em tronco de pirâmide (o topo 7,5 cm
    pra dentro de cada lado, o pé 3 cm: a borda inclinada pega a luz), só acima do corte (o pé dos de baixo fica dentro do colar); no
    topo a tampa, a luz de obstáculo (Beacon) e o para-raios."""
    C = Vector((c[0], c[1], d['zc']))
    r, z_cut = d['r'], d['zcut']
    pan = p.bm('Paint#panel', 0.0)
    core = p.bm('Dark#core', 0.0)
    seen = {}

    def cv(q):
        key = (round(q.x, 4), round(q.y, 4), round(q.z, 4))
        if key not in seen:
            seen[key] = core.verts.new(V(*q))
        return seen[key]
    n_pan = 0
    for tri in geodesic(d['freq']):
        pts = [C + q * r for q in tri]
        if max(q.z for q in pts) < z_cut + 0.05:
            continue
        core.faces.new([cv(C + q * (r - 0.07)) for q in tri])
        g = (pts[0] + pts[1] + pts[2]) / 3
        n = (pts[1] - pts[0]).cross(pts[2] - pts[0]).normalized()
        if n.dot(g - C) < 0:
            n = -n
        ins = 0.075 if r > 2.0 else 0.05
        top = [pan.verts.new(V(*(q + (g - q).normalized() * ins))) for q in pts]
        bot = [pan.verts.new(V(*(q + (g - q).normalized() * 0.03 - n * 0.09))) for q in pts]
        pan.faces.new(top)
        pan.faces.new(bot[::-1])
        for i in range(3):
            j = (i + 1) % 3
            pan.faces.new([top[i], bot[i], bot[j], top[j]])
        n_pan += 1
    T = C + Vector((0, 0, r - 0.02))
    rr = 0.34 if r > 2.0 else 0.22
    vp.lathe(p, 'Paint', T, 'z', [(0.0, 0.0), (rr, 0.0), (rr, 0.05), (rr - 0.04, 0.08), (0.0, 0.08)], seg=24)
    ub.beacon(p, tuple(T + Vector((0, 0, 0.08))), r=0.11 if r > 2.0 else 0.08, h=0.22 if r > 2.0 else 0.16, cage=r > 2.0)
    vp.tube(p, 'Trim', [T + Vector((rr * 0.55, 0.0, 0.08)), T + Vector((rr * 0.55, 0.0, rod))], 0.02, seg=6)
    return n_pan


# ---------------------------------------------------------------------------------- o anel de fora
def skirt(p):
    """A saia blindada nas 4 faces de trás e dos lados (a frente é reta): a superfície inclinada, as chapas e os parafusos."""
    b = p.bm('Body#pnl', ub._bev('Body#pnl'))
    z0, z1 = FLOOR - 0.02, SKIRT[1]
    for k in SKIRTED:
        k0, k1 = (k - 1) % 5, (k + 1) % 5
        f0 = xcorner(PENT, k0, SKIRT[0] if k0 in SKIRTED else 0.0, k, SKIRT[0])
        f1 = xcorner(PENT, k, SKIRT[0], k1, SKIRT[0] if k1 in SKIRTED else 0.0)
        t0, t1 = PENT.corner(k - 1, 0.0), PENT.corner(k, 0.0)
        vs = [b.verts.new(V(q.x, q.y, z)) for q, z in ((f0, z0), (f1, z0), (t1, z1), (t0, z1))]
        b.faces.new(vs)
        if k0 not in SKIRTED:
            b.faces.new([b.verts.new(V(t0.x, t0.y, z0)), b.verts.new(V(f0.x, f0.y, z0)), b.verts.new(V(t0.x, t0.y, z1))])
        if k1 not in SKIRTED:
            b.faces.new([b.verts.new(V(f1.x, f1.y, z0)), b.verts.new(V(t1.x, t1.y, z0)), b.verts.new(V(t1.x, t1.y, z1))])
        n = PENT.nrm(k)
        eu = (t1 - t0).normalized()
        ev = (Vector((0, 0, z1 - z0)) - n * SKIRT[0]).normalized()
        nn = eu.cross(ev).normalized()
        if nn.dot(n) < 0:
            nn = -nn
        L = math.hypot(SKIRT[0], z1 - z0)
        top = (t0 + t1) / 2 + Vector((0, 0, z1))
        w = (t1 - t0).length - 1.2
        cnt = max(1, int(round(w / 3.3)))
        pw = w / cnt
        for i in range(cnt):
            q = top + eu * (-w / 2 + pw * (i + 0.5)) - ev * (L / 2) + nn * 0.02
            ub.obox(p, 'Body#pnl', q, (pw - 0.08, L - 0.16, 0.12), eu, ev, nn)
            for tt in (-0.32, 0.0, 0.32):
                vp.nut(p, 'Dark', q + eu * (tt * pw) + ev * (L / 2 - 0.2) + nn * 0.08, nn, r=0.035, h=0.025)


def listening_mast(p, q, out):
    """A antena de escuta da quina: o pé com o isolador, o mastro de 4,8 m, as 4 cruzes de dipolos (de cima, o X na quina: uma perna
    na bissetriz, a outra de través) com as pontas, a luz de obstáculo no topo e o cabo descendo."""
    z0 = Z_O + 0.9
    vp.lathe(p, 'Trim#vp', (q.x, q.y, z0), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.1), (0.1, 0.16), (0.09, 0.34), (0.0, 0.34)], seg=12)
    top = z0 + 4.8
    vp.tube(p, 'Trim', [Vector((q.x, q.y, z0 + 0.3)), Vector((q.x, q.y, top))], 0.055, seg=8)
    side = Vector((-out.y, out.x, 0.0))
    e1, e2 = (out + side).normalized(), (out - side).normalized()     # o X a 45° da bissetriz: as pontas não saem do lote
    for i, (zz, hl) in enumerate(((z0 + 1.5, 0.6), (z0 + 2.55, 0.5), (z0 + 3.5, 0.42), (z0 + 4.3, 0.32))):
        c = Vector((q.x, q.y, zz))
        ub.obox(p, 'Dark#sm', c, (0.16, 0.16, 0.14), out, side, Z)
        for e in (e1, e2):
            vp.tube(p, 'Trim', [c - e * hl, c + e * hl], 0.026, seg=6)
            for s in (-1, 1):
                vp.lathe(p, 'Dark#vp', c + e * (s * hl), e * s, [(0.0, -0.02), (0.045, -0.02), (0.045, 0.05), (0.0, 0.06)], seg=8)
    ub.beacon(p, (q.x, q.y, top), r=0.08, h=0.16, cage=False)
    vp.tube(p, 'Dark', [Vector((q.x, q.y, z0 + 0.4)) + side * 0.09, Vector((q.x, q.y, top - 0.3)) + side * 0.09], 0.016, seg=5)


def outer_ring(p):
    # a borda de fundação em volta do pé (segue o pentágono: sem laje quadrada)
    ring_band(p, 'Concrete#pnl', PENT, SKIRT[0] + 0.15, SKIRT[0] - 0.2, -0.12, FLOOR - 0.006)
    skirt(p)
    _, _, _, w0 = PENT.face(0)
    door = {0: [(w0 / 2 - DOOR[0], w0 / 2 + DOOR[0], -0.3, DOOR[1] - FLOOR + 0.02)]}
    # o miolo: a frente do chão (com a porta), as outras de trás da saia
    gcore(p, PENT, 0.0, FLOOR - 0.02, Z_O, holes=door, skip=SKIRTED, m='Body#pnl')
    gcore(p, PENT, 0.0, SKIRT[1] - 0.25, Z_O, skip=(0,))
    # as placas: nas 4 faces com a saia a partir do topo dela; na da frente a partir do chão (a fileira de baixo a mais, a porta)
    z0 = SKIRT[1]
    rows = tuple(r - z0 for r in O_ROWS)
    holes = {}
    for k in SKIRTED:
        _, _, _, w = PENT.face(k)
        holes[k] = [(0.55, w - 0.6 - 0.55, rows[0] + 0.005, rows[1] - 0.005)]
    gwalls(p, PENT, 0.0, z0, Z_O - 0.05, skip=(0,), rows=rows, blue=3, pw=2.8, holes=holes)
    rows0 = tuple(r - FLOOR for r in (SKIRT[1],) + O_ROWS)
    m = 0.3
    GAP = 2.2                                                   # a meia largura do meio sem a fenda (a marquise e a placa)
    holes0 = {0: [(0.55, w0 / 2 - m - GAP, rows0[1] + 0.005, rows0[2] - 0.005),
                  (w0 / 2 - m + GAP, w0 - 0.6 - 0.55, rows0[1] + 0.005, rows0[2] - 0.005),
                  (w0 / 2 - m - DOOR[0] - 0.04, w0 / 2 - m + DOOR[0] + 0.04, -0.1, DOOR[1] - FLOOR + 0.05)]}
    gwalls(p, PENT, 0.0, FLOOR, Z_O - 0.05, skip=SKIRTED, rows=rows0, blue=4, pw=2.8, holes=holes0)
    for k in range(5):
        if k == 0:
            slit_span(p, PENT, 0.0, 0, 0.85, w0 / 2 - GAP - 0.25, O_ROWS[0], O_ROWS[1])
            slit_span(p, PENT, 0.0, 0, w0 / 2 + GAP + 0.25, w0 - 0.85, O_ROWS[0], O_ROWS[1])
        else:
            slit(p, PENT, 0.0, k, O_ROWS[0], O_ROWS[1])
        a, eu, en, w = PENT.face(k)
        for u in ((0.22, 0.78) if k == 0 else (0.3, 0.7)):
            wall_lamp(p, a + eu * (w * u) + Vector((0, 0, 3.85)), eu, en)
    # os pilares redondos nas 5 quinas (passam do parapeito) com as ANTENAS DE ESCUTA
    plan_pillars(p, PENT, 0.08, FLOOR, Z_O + 0.78, r=0.38)
    for k in range(5):
        q = PENT.corner(k, 0.08)
        vp.lathe(p, 'Trim#vp', (q.x, q.y, Z_O + 0.78), 'z', [(0.0, 0.0), (0.46, 0.0), (0.46, 0.08), (0.36, 0.12), (0.0, 0.12)], seg=16)
        out = (q - PENT.c).normalized()
        listening_mast(p, q, out)
    # o parapeito com a capa, a laje do teto, a borda do poço
    ring_band(p, 'Body#pnl', PENT, 0.12, -0.26, Z_O - 0.12, Z_O + 0.4)
    ring_band(p, 'Trim#sm', PENT, 0.18, -0.32, Z_O + 0.4, Z_O + 0.48)
    ring_band(p, 'Concrete#pnl', PENT, -0.24, D_OI + 0.003, Z_O - 0.2, Z_O)
    ring_band(p, 'Trim#sm', PENT, D_OI + 0.16, D_OI - 0.05, Z_O - 0.06, Z_O + 0.1)
    # a parede do anel de fora virada pro poço
    walls_in(p, PENT, D_OI, FLOOR - 0.02, Z_O - 0.18)


def entrance(p):
    """A entrada de gente na face da frente: o vão recuado escuro, a porta dupla com os visores, o batente, a PLACA da cor do jogador,
    a marquise nos 2 pilares (a faixa azul na testa), as luminárias, o patamar e os frades."""
    a, eu, en, w = PENT.face(0)
    c = a + eu * (w / 2)                                         # o meio da fachada no chão
    dz0, dz1 = FLOOR, DOOR[1]
    ub.obox(p, 'Dark#vp', c - en * 0.27 + Vector((0, 0, (dz0 + dz1) / 2)), (2 * DOOR[0] + 0.1, dz1 - dz0, 0.04), eu, Z, en)
    for s in (-1, 1):
        ub.obox(p, 'Body#sm', c + eu * (s * (DOOR[0] - 0.04)) - en * 0.12 + Vector((0, 0, (dz0 + dz1) / 2)), (0.1, dz1 - dz0, 0.3),
                eu, Z, en)
        yc = s * (DOOR[0] - 0.1) / 2
        ub.obox(p, 'Trim#sm', c + eu * yc - en * 0.2 + Vector((0, 0, FLOOR + 1.08)), ((DOOR[0] - 0.12), 2.12, 0.08), eu, Z, en)
        ub.obox(p, 'Glass#vp', c + eu * yc - en * 0.155 + Vector((0, 0, FLOOR + 1.55)), (0.4, 0.5, 0.02), eu, Z, en)
        vp.grab_handle(p, 'Dark', c + eu * (s * 0.12) - en * 0.16 + Vector((0, 0, 1.0)), c + eu * (s * 0.12) - en * 0.16 + Vector((0, 0, 1.4)),
                       en, height=0.05, r=0.014)
    ub.obox(p, 'Trim#sm', c + en * 0.06 + Vector((0, 0, dz1 + 0.08)), (2 * DOOR[0] + 0.4, 0.16, 0.16), eu, Z, en)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', c + eu * (s * (DOOR[0] + 0.12)) + en * 0.06 + Vector((0, 0, (dz0 + dz1 + 0.03) / 2)), (0.16, dz1 - dz0 + 0.03, 0.16),
                eu, Z, en)
    # a placa da cor do jogador sobre a porta (o "centro de inteligência"): a chapa, a moldura e as 2 luzes de cima
    ub.obox(p, 'Player#sm', c + en * 0.11 + Vector((0, 0, 3.62)), (1.9, 0.6, 0.06), eu, Z, en)
    ub.obox(p, 'Trim#sm', c + en * 0.09 + Vector((0, 0, 3.62)), (2.04, 0.74, 0.04), eu, Z, en)
    # a marquise: a laje com a faixa azul na testa, os 2 pilares redondos, a luminária de baixo
    zc0, zc1 = 2.95, 3.17
    depth = 1.45
    ub.obox(p, 'Body#pnl', c + en * (depth / 2) + Vector((0, 0, (zc0 + zc1) / 2)), (3.6, (zc1 - zc0), depth), eu, Z, en)
    ub.obox(p, 'Player#sm', c + en * (depth + 0.01) + Vector((0, 0, (zc0 + zc1) / 2)), (3.4, 0.14, 0.03), eu, Z, en)
    for s in (-1, 1):
        q = c + eu * (s * 1.55) + en * (depth - 0.18)
        vp.lathe(p, 'Trim#vp', (q.x, q.y, FLOOR), 'z', [(0.0, 0.0), (0.14, 0.0), (0.14, 0.06), (0.08, 0.1), (0.075, zc0 - FLOOR), (0.0, zc0 - FLOOR)],
                 seg=12)
        ub.obox(p, 'Light#vp', c + eu * (s * 0.7) + en * (depth * 0.55) + Vector((0, 0, zc0 - 0.012)), (0.5, 0.12, 0.02), eu, en, Z)
    # o patamar (um pouco acima da borda de fundação) e os frades
    ub.obox(p, 'Concrete#pnl', c + en * 0.75 + Vector((0, 0, FLOOR - 0.06)), (4.0, 0.2, 1.5), eu, Z, en)
    for s in (-1, 1):
        q = c + eu * (s * 2.35) + en * 1.35
        vp.lathe(p, 'Hazard#vp', (q.x, q.y, 0.0), 'z', [(0.0, 0.0), (0.15, 0.0), (0.15, 0.9), (0.11, 0.97), (0.0, 0.99)], seg=14)


# ---------------------------------------------------------------------------------- o poço e o anel de dentro
def roof_joints(p, d0, d1, z, step=2.9):
    for k in range(5):
        a0, eu, en, w0 = PENT.face(k, d0)
        a1, _, _, w1 = PENT.face(k, d1)
        mid0, mid1 = a0 + eu * (w0 / 2), a1 + eu * (w1 / 2)
        half = min(w0, w1) / 2 - 0.4
        n = int(half // step)
        for i in range(-n, n + 1):
            q0, q1 = mid0 + eu * (i * step), mid1 + eu * (i * step)
            ub.obox(p, 'Dark#vp', (q0 + q1) / 2 + Vector((0, 0, z)), (0.07, (q1 - q0).length, 0.012), eu, en, Z)
        c0, c1 = PENT.corner(k, d0), PENT.corner(k, d1)
        d = (c1 - c0).normalized()
        ub.obox(p, 'Dark#vp', (c0 + c1) / 2 + Vector((0, 0, z)), ((c1 - c0).length, 0.07, 0.012), d, Vector((-d.y, d.x, 0)), Z)


def well_and_inner(p):
    # o chão do poço e do pátio
    ring_band(p, 'Concrete#pnl', PENT, D_OI + 0.02, D_IO - 0.02, -0.1, FLOOR)
    poly_slab(p, 'Concrete#pnl', PENT, D_II + 0.02, -0.1, FLOOR - 0.002, cell=1.4)
    # o anel de dentro: o miolo, as placas acima do teto de fora (a fenda e a fileira AZUL larga)
    gcore(p, PENT, D_IO, FLOOR - 0.02, Z_I)
    z0 = 5.1
    rows = tuple(r - z0 for r in I_ROWS)
    holes = {}
    for k in range(5):
        _, _, _, w = PENT.face(k, D_IO)
        holes[k] = [(0.4, w - 0.6 - 0.4, rows[0] + 0.005, rows[1] - 0.005)]
    gwalls(p, PENT, D_IO, z0, Z_I - 0.05, rows=rows, blue=2, pw=2.6, holes=holes)
    for k in range(5):
        slit(p, PENT, D_IO, k, I_ROWS[0], I_ROWS[1], margin=0.7)
    plan_pillars(p, PENT, D_IO + 0.06, Z_O - 0.3, Z_I + 0.62, r=0.3)
    # o parapeito, a CAPA AZUL (de cima: o pentágono azul em volta da bola) e o teto claro
    ring_band(p, 'Body#pnl', PENT, D_IO + 0.12, D_IO - 0.26, Z_I - 0.12, Z_I + 0.4)
    ring_band(p, 'Player#sm', PENT, D_IO + 0.2, D_IO - 0.4, Z_I + 0.4, Z_I + 0.5)
    ring_band(p, 'Paint#pnl', PENT, D_IO - 0.24, D_II + 0.003, Z_I - 0.2, Z_I)
    # a mureta do pátio e a parede do pátio
    ring_band(p, 'Body#pnl', PENT, D_II + 0.24, D_II - 0.04, Z_I - 0.1, Z_I + 0.3)
    walls_in(p, PENT, D_II, FLOOR - 0.02, Z_I - 0.15)
    # as 5 pontes nas quinas do poço (os corredores): o bloco com a fenda de vidro dos 2 lados e a capa
    dm = (D_OI + D_IO) / 2
    for k in range(5):
        q = PENT.corner(k, dm)
        r = (q - PENT.c).normalized()
        t = Vector((-r.y, r.x, 0.0))
        L = (D_OI - D_IO) / math.cos(math.radians(36)) + 0.5
        gbox(p, 'Body#pnl', q + Vector((0, 0, (FLOOR + Z_O - 0.02) / 2)), (L, 1.9, Z_O - 0.02 - FLOOR), r, t, Z, open_back=False)
        for s in (-1, 1):
            ub.obox(p, 'Glass#vp', q + t * (s * 0.955) + Vector((0, 0, 2.95)), (L - 0.5, 0.04, 0.7), r, t, Z)
            ub.obox(p, 'Trim#sm', q + t * (s * 0.99) + Vector((0, 0, 2.55)), (L - 0.4, 0.1, 0.08), r, t, Z)
        ub.obox(p, 'Trim#sm', q + Vector((0, 0, Z_O + 0.04)), (L - 0.3, 1.7, 0.08), r, t, Z)


# ---------------------------------------------------------------------------------- a sala de guerra
def war_room(p):
    """A claraboia da sala de guerra no teto da ala da frente: o meio-fio de placas com o forro escuro por dentro, a MESA DO MAPA acesa
    em azul (UsCore) com o terreno (Player), a grade escura, as setas e os marcadores do plano (Light); os 4 vidros escuros inclinados,
    as barras das quinas e do meio, o caixilho de cima e as travessas da grade aberta."""
    cx, cy, L, D = SKY
    eu, ev = Vector(Y), Vector(X)                               # eu ao longo da face (y), ev na profundidade (x)
    C = Vector((cx, cy, 0.0))
    zr = Z_O
    hc = 0.5                                                    # a altura do meio-fio
    t = 0.18
    for s in (-1, 1):
        ub.obox(p, 'Body#sm', C + ev * (s * (D / 2 - t / 2)) + Vector((0, 0, zr + hc / 2)), (L, t, hc), eu, ev, Z)
        ub.obox(p, 'Body#sm', C + eu * (s * (L / 2 - t / 2)) + Vector((0, 0, zr + hc / 2)), (t, D - 2 * t - 0.006, hc), eu, ev, Z)
        ub.obox(p, 'Dark#vp', C + ev * (s * (D / 2 - t - 0.01)) + Vector((0, 0, zr + hc / 2)), (L - 2 * t, 0.02, hc - 0.02), eu, ev, Z)
        ub.obox(p, 'Dark#vp', C + eu * (s * (L / 2 - t - 0.01)) + Vector((0, 0, zr + hc / 2)), (0.02, D - 2 * t, hc - 0.02), eu, ev, Z)
    iw, idp = L - 2 * t - 0.06, D - 2 * t - 0.06               # a mesa (dentro do meio-fio)
    ub.obox(p, 'Dark#sm', C + Vector((0, 0, zr + 0.07)), (iw, idp, 0.14), eu, ev, Z)
    ub.obox(p, 'UsCore#vp', C + Vector((0, 0, zr + 0.155)), (iw - 0.06, idp - 0.06, 0.03), eu, ev, Z)
    zt = zr + 0.17
    for (pu, pv, su, sv) in ((-1.05, 0.15, 0.7, 0.42), (0.25, -0.2, 0.9, 0.36), (1.15, 0.22, 0.5, 0.3), (-0.3, 0.3, 0.45, 0.22)):
        vp.prism(p, 'Player#vp', vp.rounded_rect(su, sv, 0.08, 3), C + eu * pu + ev * pv + Vector((0, 0, zt)), eu, ev, Vector(Z), 0.008)
    for k in range(1, 6):
        ub.obox(p, 'Dark#vp', C + eu * (-iw / 2 + iw * k / 6) + Vector((0, 0, zt + 0.004)), (0.035, idp - 0.06, 0.008), eu, ev, Z)
    ub.obox(p, 'Dark#vp', C + Vector((0, 0, zt + 0.004)), (iw - 0.06, 0.035, 0.008), eu, ev, Z)
    # o plano de batalha: 2 setas e os marcadores
    for (u0, v0, u1, v1) in ((-1.45, -0.32, 0.1, 0.1), (-1.2, 0.36, 0.75, 0.12)):
        A, B = C + eu * u0 + ev * v0 + Vector((0, 0, zt + 0.012)), C + eu * u1 + ev * v1 + Vector((0, 0, zt + 0.012))
        e = (B - A).normalized()
        f = Vector((-e.y, e.x, 0.0))
        ub.obox(p, 'Light#vp', (A + B) / 2 - e * 0.1, ((B - A).length - 0.2, 0.07, 0.01), e, f, Z)
        tip = [B + e * 0.08, B - e * 0.2 + f * 0.14, B - e * 0.2 - f * 0.14]
        vp.prism(p, 'Light#vp', [(q.x, q.y) for q in tip], Vector((0, 0, zt + 0.007)), Vector(X), Vector(Y), Vector(Z), 0.01)
    for (pu, pv) in ((0.4, 0.05), (0.62, -0.2), (0.85, 0.18), (1.3, -0.3), (-0.2, -0.3), (1.6, 0.05), (-1.6, 0.25)):
        ub.obox(p, 'Light#vp', C + eu * pu + ev * pv + Vector((0, 0, zt + 0.015)), (0.09, 0.09, 0.02), eu, ev, Z)
    # os vidros inclinados (do meio-fio pra dentro e pra cima) e o caixilho
    zg0, zg1 = zr + hc, zr + hc + 0.28
    ou, ov, iu, iv = L / 2, D / 2, L / 2 - t - 0.02, D / 2 - t - 0.02
    corners_o = [C + eu * (su * ou) + ev * (sv * ov) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    corners_i = [C + eu * (su * iu) + ev * (sv * iv) for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    bm = p.bm('Glass#vp', 0.0)
    for i in range(4):
        j = (i + 1) % 4
        lo = [corners_o[i] + Vector((0, 0, zg0)), corners_o[j] + Vector((0, 0, zg0)), corners_i[j] + Vector((0, 0, zg1)),
              corners_i[i] + Vector((0, 0, zg1))]
        n = (lo[1] - lo[0]).cross(lo[3] - lo[0]).normalized()
        if n.z < 0:
            n = -n
        vs = [bm.verts.new(V(*q)) for q in lo] + [bm.verts.new(V(*(q - n * 0.04))) for q in lo]
        bm.faces.new(vs[:4])
        bm.faces.new(vs[4:][::-1])
        for a_ in range(4):
            b_ = (a_ + 1) % 4
            bm.faces.new([vs[a_], vs[4 + a_], vs[4 + b_], vs[b_]])
        vp.tube(p, 'Trim', [lo[0], lo[3]], 0.035, seg=6)
        vp.tube(p, 'Trim', [(lo[0] + lo[1]) / 2, (lo[2] + lo[3]) / 2], 0.03, seg=6)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', C + ev * (s * iv) + Vector((0, 0, zg1 + 0.03)), (2 * iu + 0.08, 0.08, 0.08), eu, ev, Z)
        ub.obox(p, 'Trim#sm', C + eu * (s * iu) + Vector((0, 0, zg1 + 0.03)), (0.08, 2 * iv, 0.08), eu, ev, Z)
    for k in range(1, 6):
        ub.obox(p, 'Trim#vp', C + eu * (-iu + 2 * iu * k / 6) + Vector((0, 0, zg1 + 0.03)), (0.06, 2 * iv, 0.06), eu, ev, Z)
    ub.obox(p, 'Trim#vp', C + Vector((0, 0, zg1 + 0.03)), (2 * iu, 0.06, 0.06), eu, ev, Z)


# ---------------------------------------------------------------------------------- os tetos
def roofs(p):
    """As juntas, as caixas de ventiladores (giradas com a ala), o armário de rádio, a escotilha, a escada de marinheiro, a sala de
    guerra e as 2 radomos pequenas nas alas de trás (o colar de concreto com a faixa azul)."""
    roof_joints(p, -0.3, D_OI + 0.08, Z_O + 0.006)
    roof_joints(p, D_IO - 0.3, D_II + 0.28, Z_I + 0.006)
    for k, u in ((1, 1.7), (4, -1.7)):
        q, n, t = wing(k, A_O + D_OI / 2 - 0.05, u)
        yaw = math.degrees(math.atan2(t.y, t.x))
        stamp(p, lambda s: ub.fan_box(s, (0.0, 0.0), (2.4, 1.25, 0.95), fans=2), (q.x, q.y, Z_O), yaw)
    q, n, t = wing(2, A_O + D_OI / 2 - 0.05, 3.2)
    yaw = math.degrees(math.atan2(t.y, t.x))

    def cabinet(s):
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 0.62), (1.3, 1.1, 1.2), X, Y, Z)
        louvers(s, (0.0, -0.56, 0.65), (0, -1, 0), (1, 0, 0), 0.8, 0.6, 6)
        ub.obox(s, 'Player#sm', (0.0, 0.56, 0.9), (1.0, 0.03, 0.24), X, Y, Z)
        ub.antenna(s, 0.35, 0.25, 1.22, 3.2, r=0.065)
    stamp(p, cabinet, (q.x, q.y, Z_O), yaw)
    q, n, t = wing(1, A_O + D_OI / 2 - 0.05, -2.8)
    yaw = math.degrees(math.atan2(t.y, t.x))

    def hatch(s):
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 0.2), (0.9, 0.9, 0.4), X, Y, Z)
        ub.obox(s, 'Body#sm', (0.03, 0.0, 0.43), (0.8, 0.82, 0.06), X, Y, Z)
        vp.hinge(s, 'Trim', (-0.4, 0.0, 0.44), 'y', 0.6, r=0.025, knuckles=3)
    stamp(p, hatch, (q.x, q.y, Z_O), yaw)
    a, eu, en, w = PENT.face(2)
    q = a + eu * (w * 0.78)
    ub.ladder(p, 'Trim', q.x, q.y, FLOOR, Z_O, tuple(en), w=0.5, cage=True, wall=0.62, top_ext=1.0)
    war_room(p)
    # as 2 radomos pequenas nas alas de trás
    n_small = 0
    for k in SMALL_K:
        q, n, t = wing(k, A_O + D_OI / 2 - 0.05, 0.0)
        RC, zc = SMALL['RC'], SMALL['zcut']
        vp.lathe(p, 'Concrete', (q.x, q.y, 0.0), 'z', [(0.0, Z_O - 0.05), (RC + 0.12, Z_O - 0.05), (RC + 0.1, Z_O + 0.06), (RC, Z_O + 0.12),
                                                       (RC, zc - 0.06), (RC - 0.06, zc), (0.0, zc)], seg=40)
        vp.lathe(p, 'Player', (q.x, q.y, 0.0), 'z', [(RC - 0.01, zc - 0.36), (RC + 0.015, zc - 0.36), (RC + 0.015, zc - 0.16),
                                                     (RC - 0.01, zc - 0.16)], seg=40, closed=True)
        vp.lathe(p, 'Dark', (q.x, q.y, 0.0), 'z', [(SMALL['rc'] - 0.08, zc - 0.01), (SMALL['rc'] + 0.05, zc - 0.01), (SMALL['rc'] + 0.05, zc + 0.03),
                                                   (SMALL['rc'] - 0.08, zc + 0.03)], seg=40, closed=True)
        n_small += ball(p, (q.x, q.y), SMALL, rod=0.6)
    return n_small


# ---------------------------------------------------------------------------------- a bola grande no pátio
def big_ball(p):
    """O fuste octogonal no pátio (a base da bola; a torre do radar saiu): o pé de concreto, as placas, as faixas de aço, a porta do
    pátio; o colar em cone (a faixa azul, o friso escuro onde a esfera assenta); a bola geodésica de 7,5 m; o anel amarelo no pátio."""
    cx, cy = C0
    poly_band(p, 'Concrete#pnl', SHAFT, 0.3, -0.2, -0.1, FLOOR + 0.2)
    gcore(p, SHAFT, 0.0, FLOOR + 0.18, SHAFT_TOP, m='Body#pnl')
    for zb in (2.6, 5.2):
        poly_band(p, 'Trim#sm', SHAFT, 0.05, -0.05, zb, zb + 0.22)
    d = Vector((1.0, 0.0, 0.0))
    e = Vector((0.0, 1.0, 0.0))
    q = PENT.c + d * 2.15
    ub.obox(p, 'Dark#vp', q + d * 0.02 + Vector((0, 0, FLOOR + 1.2)), (0.95, 2.0, 0.06), e, Z, d)
    ub.obox(p, 'Trim#sm', q + d * 0.05 + Vector((0, 0, FLOOR + 1.17)), (0.82, 1.9, 0.06), e, Z, d)
    ub.ring_paint(p, 'Yellow', C0, 2.45, 2.6, FLOOR - 0.004, FLOOR + 0.012, seg=56)
    # o colar em cone (sai do fuste e alarga por cima da mureta do pátio)
    zc = BALL['zcut']
    R1 = BALL['rc'] + 0.37
    vp.lathe(p, 'Body', (cx, cy, 0.0), 'z', [(0.0, SHAFT_TOP - 0.25), (2.0, SHAFT_TOP - 0.25), (R1, zc - 0.2), (R1, zc - 0.05),
                                             (R1 - 0.05, zc), (0.0, zc)], seg=64)
    vp.lathe(p, 'Player', (cx, cy, 0.0), 'z', [(R1 - 0.01, zc - 0.19), (R1 + 0.018, zc - 0.19), (R1 + 0.018, zc - 0.02),
                                               (R1 - 0.01, zc - 0.02)], seg=64, closed=True)
    vp.lathe(p, 'Dark', (cx, cy, 0.0), 'z', [(BALL['rc'] - 0.12, zc - 0.01), (BALL['rc'] + 0.07, zc - 0.01), (BALL['rc'] + 0.07, zc + 0.04),
                                             (BALL['rc'] - 0.12, zc + 0.04)], seg=64, closed=True)
    vp.bolt_ring(p, 'Dark', (cx, cy, zc), 'z', R1 - 0.15, 28, r=0.03, h=0.02)
    return ball(p, C0, BALL)


# ---------------------------------------------------------------------------------- o chão
def ground(p):
    # o mastro da bandeira (frente, +Y)
    fx, fy = 9.35, 7.5
    ub.obox(p, 'Concrete#pnl', (fx, fy, 0.05), (0.9, 0.9, 0.4), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 0.25), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.11, 0.1), (0.09, 0.4), (0.055, 8.6), (0.0, 8.6)], seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 8.85), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), 8.75, 2.4, 1.45, m='Player')
    # os postes de luz dos 2 lados da entrada, virados pra porta
    for s in (-1, 1):
        ub.lamp_post(p, 9.45, s * 3.5, h=5.4, aim=(XF, 0.0))
    # os sacos de areia na quina da frente-esquerda
    sandbag_arc(p, (9.2, -7.9), 0.95, 60, 300)
    # o gerador (trás, +Y) e o tanque de combustível (trás, −Y), paralelos às alas, longe do andaime

    def generator(s):
        ub.obox(s, 'Concrete#pnl', (0.0, 0.0, 0.0), (2.6, 1.4, 0.4), X, Y, Z)
        ub.obox(s, 'Trim#sm', (0.0, 0.0, 0.26), (2.3, 1.1, 0.12), X, Y, Z)
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 1.2), (2.2, 1.0, 1.8), X, Y, Z)
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 2.14), (2.32, 1.12, 0.12), X, Y, Z)
        for sg in (-1, 1):
            ub.obox(s, 'Player#sm', (0.0, sg * 0.52, 1.4), (1.9, 0.04, 0.4), X, Y, Z)
        ub.louvers(s, (1.105, 0.0, 1.15), (1, 0, 0), (0, 1, 0), 0.7, 1.2, 9)
        ex = Vector((-0.75, 0.25, 2.2))
        vp.lathe(s, 'Dark#vp', ex, 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.05), (0.11, 0.08), (0.11, 0.7), (0.0, 0.7)], seg=16)
        vp.tube(s, 'Dark', vp.bent_bar([ex + Vector((0, 0, 0.66)), ex + Vector((0, 0, 0.8)), ex + Vector((-0.2, 0.2, 0.86))], R=0.1, n=4),
                0.11, seg=14)
    stamp(p, generator, (-7.55, 8.35, 0.0), 54.0)

    def tank(s):
        ub.obox(s, 'Concrete#pnl', (0.0, 0.0, 0.0), (2.7, 1.6, 0.4), X, Y, Z)
        ub.h_tank(s, (0.0, 0.0, 1.1), 'x', 0.6, 2.1, m='Body', band='Player', saddles=2, foot_z=0.2)
    stamp(p, tank, (-7.55, -8.35, 0.0), -54.0)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    outer_ring(p)
    entrance(p)
    well_and_inner(p)
    n_small = roofs(p)
    n_big = big_ball(p)
    ground(p)
    print('MODEL paineis: a bola grande %d, as 2 pequenas %d' % (n_big, n_small))
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o andaime
def scaffold_runs(name, runs, h, out=0.85, post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra em trechos [(a, b)]; 'out' pra fora do centro do pentágono."""
    p = Part(name)
    top = h + 1.0
    levels = max(2, int((top - z0) / lift))
    cx, cy = C0
    for (ax, ay), (bx, by) in runs:
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
            dd = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (dd.x, dd.y, dd.z), 0.035, dd.length, seg=6, bevel=0.0)
    return p.finish()


def scaffold():
    """O andaime nas 5 faces (na da frente, dos 2 lados da entrada), cortado onde o lado de fora sairia do lote."""
    runs = []
    for k in range(5):
        dl = SKIRT[0] + 0.2 if k in SKIRTED else 0.2
        a = PENT.corner(k - 1, dl)
        b = PENT.corner(k, dl)
        n = PENT.nrm(k)

        def ok(q):
            o = q + n * 0.85
            return abs(o.x) < HX - 0.1 and abs(o.y) < HY - 0.1
        ts = [i / 40 for i in range(41)]
        good = [t for t in ts if ok(a + (b - a) * t)]
        if not good:
            continue
        t0, t1 = min(good), max(good)
        if k == 0:
            runs.append((tuple(a + (b - a) * t0)[:2], tuple(a + (b - a) * 0.3)[:2]))
            runs.append((tuple(a + (b - a) * 0.7)[:2], tuple(a + (b - a) * t1)[:2]))
        else:
            runs.append((tuple(a + (b - a) * t0)[:2], tuple(a + (b - a) * t1)[:2]))
    return scaffold_runs(PREFIX + '_Scaffold', runs, Z_O + 0.4)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_strategy_center_v3')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    sc = scaffold()
    check([b, sc], HX, HY)
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, sc)})
    total = export(b, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL a bola: centro (%.2f, %.2f, %.2f), diametro %.1f, topo %.2f; as pequenas: diametro %.1f, topo %.2f' %
          (C0[0], C0[1], BALL['zc'], 2 * BALL['r'], BALL['zc'] + BALL['r'], 2 * SMALL['r'], SMALL['zc'] + SMALL['r']))
    q1, _, _ = wing(1, A_O + D_OI / 2, -1.0)
    q4, _, _ = wing(4, A_O + D_OI / 2, 1.0)
    q0, _, _ = wing(0, A_O + (D_IO + D_II) / 2, 2.6)
    q2, _, _ = wing(2, A_O + D_OI / 2, -3.0)
    print('MODEL fire_points_m', [[round(q1.x, 2), round(q1.y, 2), Z_O], [round(q4.x, 2), round(q4.y, 2), Z_O],
                                  [round(q0.x, 2), round(q0.y, 2), Z_I], [round(q2.x, 2), round(q2.y, 2), Z_O]])
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]]],
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 900]],
                  ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1400, 900]],
                  ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
                 (2.0, 14.5, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
