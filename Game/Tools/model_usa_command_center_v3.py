"""O Command Center da USA (usa_command_center), VERSÃO 3 (01/11/2026): o PENTÁGONO. O estudo e o porquê: docs/elenco/USA_CC_E_USINA.md.

As duas antes foram reprovadas (o Felipe, 01/11: "muito básico, parecido com o anterior, quero ideia nova, de arrepiar"): eram o
retângulo blindado com o radar em cima. Esta nasce de uma forma que o mundo inteiro reconhece de cima: o QG dos EUA, cinco alas em
dois anéis em volta do pátio, e no pátio a torre de comando com o radar de antena plana (as 4 faces do AN/SPY-6 / AN/TPY-2) e o domo
do link de satélite girando em cima. Nenhuma outra peça do jogo tem cinco lados.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_command_center_v3.py C:/Users/luisf/explosion/captures/game/models/usa_command_center_v3

A PALAVRA (CONCEITO_CONSTRUCOES.md §3b, "uma palavra por prédio"): O PENTÁGONO. De cima, em 1 segundo: o contorno de cinco lados, as
linhas concêntricas (a fachada, o poço de luz escuro entre os anéis, o anel de dentro, o pátio) com as duas faixas azuis da cor do
jogador, e no meio a torre com os 4 hexágonos escuros do radar em volta do domo branco. O que se mexe: o domo gira (com o braço da
antena de satélite e a faixa azul), a bandeira, as sirenes; o Dozer sai pelo portão blindado da frente.
  - o anel de FORA (a fachada): a saia blindada de chapas com os parafusos (nas 4 faces de trás e dos lados), as paredes de PLACAS
    com a junta funda, a fenda de vidro escuro com os montantes (a fileira de janelas do Pentágono), a fileira AZUL de placas logo
    abaixo do parapeito, os pilares redondos nas 5 quinas passando do parapeito com as antenas de chicote e as balizas, as
    luminárias;
  - o POÇO DE LUZ (1 m) entre os anéis, escuro, atravessado nas 5 quinas pelas pontes (os corredores do Pentágono);
  - o anel de DENTRO, 1,4 m mais alto: a fenda de vidro e a fileira azul por cima do teto de fora (o degrau que lê de fora), os
    pilares nas quinas, o parapeito;
  - a face da FRENTE (+X) reta, sem saia: o PORTÃO BLINDADO da garagem do Dozer, as 2 folhas de correr de 60 cm (as do abrigo de
    avião endurecido) paradas abertas na frente da fachada, com as chapas aparafusadas e a borda de perigo, penduradas na VIGA DO
    TRILHO (a faixa azul, as balizas, os holofotes), os trilhos no chão; o vão fundo e escuro com as luzes do teto, as nervuras, o
    portão de dentro e as faixas de perigo; o pátio de saída com as faixas e os frades;
  - a TORRE no pátio: o fuste de placas, o colar, a casa das antenas em tronco de pirâmide (11° pra trás, como o passadiço do
    destróier) com os 4 HEXÁGONOS do radar (a moldura clara, o fundo escuro e os módulos quadrados de 60 cm do SPY-6 por dentro), a
    faixa azul, o deque com o guarda-corpo, o pedestal; as 2 passarelas até o teto do anel de dentro; o anel amarelo no pátio;
  - o DOMO (malha à parte, origem no eixo: o _Radar) do link de satélite: a mesa giratória, o tambor com a faixa azul, o domo de
    painéis desencontrados com a junta escura (um painel azul por fileira), o para-raios com a luz de aviso, o braço com a antena de
    satélite de um lado e a caixa de equipamento com a sirene do outro;
  - o HELIPONTO no teto da ala de trás-direita (216°): o deque octogonal sobre as colunas, a rede de segurança, as luzes de borda, o
    "H" no círculo amarelo com o contorno branco e as marcas azuis, a escada;
  - nos tetos: as caixas de ventiladores (a assinatura do teto da USA), o armário de rádio com as antenas, 2 domos pequenos no anel de
    dentro, as juntas das lajes, a escada de marinheiro;
  - no chão: o mastro da bandeira (o pano no vento é do jogo: flags.json), os postes de luz, as fileiras de HESCO, os ninhos de sacos
    de areia, as barreiras de concreto do pátio de saída.
A USA limpa (13/10): sem desgaste pintado no modelo (o desgaste vem do material do jogo, pelas juntas, chapas e máscaras daqui).

Lote 25,28 × 29,52 m (x −12,64..12,64, y −14,76..14,76), a frente em +X; o Dozer sai pelo portão em +X, centrado em y = 0 (o vão de
6,2 m livres × 4,6 m; a faixa de saída do SimCore é 6,6 m). O pentágono: o centro em (0,3; 0), 9,8 m do centro a cada face (24,3 m
de y de ponta a ponta, 21,9 m em x), a ponta pra trás. Alturas: o anel de fora 5,4 m (parapeito 5,9), o de dentro 6,8 (7,3), o deque
da torre 11,7, o eixo do domo 12,35, o topo do domo 15,1, a ponta do para-raios 16,0 (a regra: a torre até ~16 m).
Peças: SM_USA_CommandCenter_Base, SM_USA_CommandCenter_Radar (origem no eixo: spin at_m = RADAR_AT), SM_USA_CommandCenter_Scaffold.
Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime), CivCloth (HESCO e sacos).
O kit (CPart, Plan, poly_slab, plan_walls, ...) vem do model_usa_power_plant_v2.py (só a interface, nada do desenho).
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_lib import Part  # noqa: E402
from model_usa_power_plant_v2 import (CPart, Plan, poly_slab, poly_band, plan_core, plan_walls, plan_pillars, obox_open,  # noqa: E402
                                      slab_tiles, louvers, check, preview_spec, X, Y, Z)

PREFIX = 'SM_USA_CommandCenter'
COLORS.update({'CivCloth': (158, 132, 98), 'Rubber': (32, 32, 31)})

# ---------------------------------------------------------------------------------- as medidas
HX, HY = 12.64, 14.76            # o lote 25,28 × 29,52
FLOOR = 0.12                     # o topo da laje
C0 = (0.3, 0.0)                  # o centro do pentágono
A_O = 9.8                        # do centro à fachada de fora
D_OI = -2.4                      # (recuo) a face de dentro do anel de fora: o poço de luz
D_IO = -3.4                      # a face de fora do anel de dentro
D_II = -5.2                      # a face do pátio
PENT = Plan(C0, [(72 * k, A_O) for k in range(5)])          # a face 0 olha pra +X; a quina 2 é a ponta de trás (−X)
SKIRT = (0.45, 1.5)              # a saia: o avanço no pé e a altura
SKIRTED = (1, 2, 3, 4)           # a frente (face 0) é reta
Z_O, Z_I = 5.4, 6.8              # os tetos dos anéis
XF = C0[0] + A_O                 # a fachada da frente (x = 10,1)
GATE_W, GATE_H = 3.1, 4.72       # o vão: a meia largura e o topo (4,6 m livres)
GX0 = C0[0] + A_O + D_IO         # o fundo da garagem (a face do anel de dentro)
LEAF = (3.25, 6.55, 0.1, 0.7, 4.85)      # as folhas do portão: |y| de dentro e de fora, x (recuo da fachada) de trás e da frente, topo
BEAM = (4.95, 5.75)              # a viga do trilho (z)
# as fileiras das placas (z absolutos): a fenda de vidro e a faixa azul
O_ROWS = (2.6, 3.35, 4.3, 4.85)  # anel de fora: a fenda 2,6–3,35, o azul 4,3–4,85
I_ROWS = (5.7, 6.25)             # anel de dentro (a partir de 5,1): a fenda 5,7–6,25, o azul 6,25–6,75
# a torre (o octógono: as faces grandes a 45°, onde ficam os radares)
H0, H1 = 8.15, 11.7              # a casa das antenas (o tronco de pirâmide)
HA0, HA1 = (2.55, 2.95), (1.8, 2.2)       # (apótema das faces grandes, das quinas) embaixo e em cima
RADAR_AT = (C0[0], 0.0, 12.35)   # o eixo do domo (spin at_m)
BR_Z = 5.0                       # as passarelas da torre até o anel de dentro (o piso)
# o heliponto na ala 3 (216°)
HELI_ANG, HELI_R = 216.0, 7.1
HELI = (C0[0] + HELI_R * math.cos(math.radians(HELI_ANG)), C0[1] + HELI_R * math.sin(math.radians(HELI_ANG)))
DECK_TOP, DECK_T, DECK_AP = 8.0, 0.24, 2.9
HELI_Z = DECK_TOP + 0.014        # o topo da tinta do "H" (o helipad_m)


def oct_plan(a, c):
    """O octógono da torre: as faces grandes (45°, 135°, ...) a 'a' do centro, as das quinas (0°, 90°, ...) a 'c'."""
    return Plan(C0, [(0, c), (45, a), (90, c), (135, a), (180, c), (225, a), (270, c), (315, a)])


SHAFT = oct_plan(2.2, 2.6)


def house_plan(z, dl=0.0):
    """O contorno da casa das antenas na altura z (o tronco de pirâmide: interpolado entre o de baixo e o de cima)."""
    t = (z - H0) / (H1 - H0)
    return oct_plan(HA0[0] + (HA1[0] - HA0[0]) * t + dl, HA0[1] + (HA1[1] - HA0[1]) * t + dl)


# ---------------------------------------------------------------------------------- ajudantes
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


CELL = 0.39      # a célula da grade do detalhe (o executor corta toda aresta > 0,4 m ao meio, em potências de 2: 14 m viravam 64
                 # pedaços de 22 cm); a superfície grande já sai quadriculada em ≤ 0,39 m e o executor não corta de novo


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
    """O poly_band do kit (o anel: a laje do teto, o parapeito, a capa) já quadriculado, sólido fechado."""
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
    """O plan_core do kit (o miolo das paredes, com os vãos) já quadriculado; inward = a face visível pro centro (o poço, o pátio)."""
    e = 0.002
    for k in range(plan.n):
        if k in skip:
            continue
        a, eu, en, w = plan.face(k, dl)
        for (u0, u1, v0, v1) in ub._rect_minus(0.0, w, 0.0, z1 - z0, (holes or {}).get(k, ()), 0.0):
            c = a + eu * ((u0 + u1) / 2) + Vector((0, 0, z0 + (v0 + v1) / 2)) + en * ((t / 2) * (1 if inward else -1))
            gbox(p, m, c, (u1 - u0 + 2 * e, v1 - v0 + 2 * e, t), eu, Z, -en if inward else en)


def gwalls(p, plan, dl, z0, z1, skip=(), rows=(0.55, 1.65, 2.3), blue=2, pw=2.4, holes=None, m='Body#pnl', margin=0.3):
    """O plan_walls do kit (as placas com a junta funda e a fileira azul) com as placas já quadriculadas."""
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
    """Monta peças do kit que só existem alinhadas aos eixos (a caixa de ventiladores, o armário) numa peça de rascunho em volta da
    origem e carimba na p girada 'yaw' graus (o giro da Unreal, +X→+Y) e posta em 'at' (x, y, z): os tetos das alas do pentágono
    correm em 5 direções."""
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
    """O miolo das paredes viradas pra DENTRO do contorno (o poço, o pátio), já quadriculado."""
    gcore(p, plan, dl, z0, z1, holes=holes, skip=skip, m=m, t=t, inward=True)


def slit(p, plan, dl, k, z0, z1, margin=0.85, step=1.15):
    """A fenda de vidro escuro na face k (o vidro no miolo, recuado atrás das placas) com os montantes."""
    a, eu, en, w = plan.face(k, dl)
    ww = w - 2 * margin
    mid = a + eu * (w / 2)
    ub.obox(p, 'Glass#vp', mid + en * 0.01 + Vector((0, 0, (z0 + z1) / 2)), (ww, z1 - z0, 0.04), eu, Z, en)
    n = max(2, int(round(ww / step)))
    for i in range(n + 1):
        u = -ww / 2 + ww * i / n
        ub.obox(p, 'Trim#vp', mid + eu * u + en * 0.05 + Vector((0, 0, (z0 + z1) / 2)), (0.12, z1 - z0 + 0.04, 0.1), eu, Z, en)
    gbox(p, 'Trim#sm', mid + en * 0.1 + Vector((0, 0, z0 - 0.04)), (ww + 0.1, 0.08, 0.14), eu, Z, en, open_back=False)       # o peitoril


def wall_lamp(p, q, eu, en):
    """A luminária de parede com o corpo, a lente acesa e a aba."""
    ub.obox(p, 'Dark#sm', q + en * 0.17, (0.6, 0.18, 0.22), eu, en, Z)
    ub.obox(p, 'Light#vp', q + en * 0.27 - Vector((0, 0, 0.05)), (0.5, 0.012, 0.1), eu, en, Z)
    ub.obox(p, 'Dark#sm', q + en * 0.28 + Vector((0, 0, 0.13)), (0.64, 0.2, 0.025), eu, en, Z)


# ---------------------------------------------------------------------------------- o anel de fora
def skirt(p):
    """A saia blindada nas 4 faces de trás e dos lados (a frente é reta): a superfície inclinada contínua (acaba contra o plano da
    frente num triângulo), as chapas e os parafusos."""
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
        # as chapas no plano inclinado (a régua é a borda de cima), com os parafusos na borda de cima
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


def outer_ring(p):
    # a borda de fundação em volta do pé
    ring_band(p, 'Concrete#pnl', PENT, SKIRT[0] + 0.15, SKIRT[0] - 0.2, -0.12, FLOOR - 0.006)
    skirt(p)
    _, _, _, w0 = PENT.face(0)
    gate = {0: [(w0 / 2 - GATE_W, w0 / 2 + GATE_W, -0.3, GATE_H - FLOOR + 0.02)]}
    # o miolo: a frente do chão (com o vão), as outras de trás da saia
    gcore(p, PENT, 0.0, FLOOR - 0.02, Z_O, holes=gate, skip=SKIRTED, m='Body#pnl')
    gcore(p, PENT, 0.0, SKIRT[1] - 0.25, Z_O, skip=(0,))
    # as placas nas 4 faces com a saia: a fenda (o vão de uma fileira inteira) e a fileira azul
    z0 = SKIRT[1]
    rows = tuple(r - z0 for r in O_ROWS)
    holes = {}
    for k in SKIRTED:
        _, _, _, w = PENT.face(k)
        holes[k] = [(0.55, w - 0.6 - 0.55, rows[0] + 0.005, rows[1] - 0.005)]     # dentro da fileira: não corta as vizinhas
    gwalls(p, PENT, 0.0, z0, Z_O - 0.05, skip=(0,), rows=rows, blue=3, pw=2.8, holes=holes)
    for k in SKIRTED:
        slit(p, PENT, 0.0, k, O_ROWS[0], O_ROWS[1])
        a, eu, en, w = PENT.face(k)
        for u in (0.3, 0.7):
            wall_lamp(p, a + eu * (w * u) + Vector((0, 0, 3.85)), eu, en)
    # os pilares redondos nas 5 quinas (passam do parapeito), as antenas de chicote e as balizas
    plan_pillars(p, PENT, 0.08, FLOOR, Z_O + 0.78, r=0.38)
    for k in range(5):
        q = PENT.corner(k, 0.08)
        vp.lathe(p, 'Trim#vp', (q.x, q.y, Z_O + 0.78), 'z', [(0.0, 0.0), (0.46, 0.0), (0.46, 0.08), (0.36, 0.12), (0.0, 0.12)], seg=16)
        ub.antenna(p, q.x + 0.12, q.y, Z_O + 0.9, 3.6 if k in (0, 4) else 2.8, r=0.065)
        ub.beacon(p, (q.x - 0.15, q.y, Z_O + 0.9), r=0.08, h=0.17, cage=False)
    # o parapeito com a capa, a laje do teto, a borda do poço
    ring_band(p, 'Body#pnl', PENT, 0.12, -0.26, Z_O - 0.12, Z_O + 0.4)
    ring_band(p, 'Trim#sm', PENT, 0.18, -0.32, Z_O + 0.4, Z_O + 0.48)
    ring_band(p, 'Concrete#pnl', PENT, -0.24, D_OI + 0.003, Z_O - 0.2, Z_O)
    ring_band(p, 'Trim#sm', PENT, D_OI + 0.16, D_OI - 0.05, Z_O - 0.06, Z_O + 0.1)
    # a parede do anel de fora virada pro poço (o vão da garagem atravessa)
    _, _, _, wi = PENT.face(0, D_OI)
    walls_in(p, PENT, D_OI, FLOOR - 0.02, Z_O - 0.18, holes={0: [(wi / 2 - GATE_W - 0.32, wi / 2 + GATE_W + 0.32, -0.1, Z_O)]})


def roof_joints(p, d0, d1, z, step=2.9):
    """As juntas das lajes de um anel (de d0 a d1 de recuo): as radiais a cada ~2,9 m em cada ala e as diagonais nas quinas."""
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


# ---------------------------------------------------------------------------------- o poço e o anel de dentro
def well_and_inner(p):
    # o chão do poço e do pátio
    ring_band(p, 'Concrete#pnl', PENT, D_OI + 0.02, D_IO - 0.02, -0.1, FLOOR)
    poly_slab(p, 'Concrete#pnl', PENT, D_II + 0.02, -0.1, FLOOR - 0.002, cell=1.4)
    # o anel de dentro: o miolo de fora (o fundo da garagem é ele), as placas acima do teto de fora, a fenda e a faixa azul
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
    ring_band(p, 'Body#pnl', PENT, D_IO + 0.12, D_IO - 0.26, Z_I - 0.12, Z_I + 0.4)
    ring_band(p, 'Trim#sm', PENT, D_IO + 0.18, D_IO - 0.32, Z_I + 0.4, Z_I + 0.48)
    ring_band(p, 'Concrete#pnl', PENT, D_IO - 0.24, D_II + 0.003, Z_I - 0.2, Z_I)
    # a mureta do pátio e a parede do pátio
    ring_band(p, 'Body#pnl', PENT, D_II + 0.24, D_II - 0.04, Z_I - 0.1, Z_I + 0.32)
    walls_in(p, PENT, D_II, FLOOR - 0.02, Z_I - 0.15)
    # as 5 pontes nas quinas do poço (os corredores do Pentágono): o bloco com a fenda de vidro dos 2 lados e a capa
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


def roofs(p):
    """Os tetos: as juntas, as caixas de ventiladores (giradas com a ala), o armário de rádio com as antenas, os domos pequenos, a
    escotilha e a escada de marinheiro de trás."""
    roof_joints(p, -0.3, D_OI + 0.08, Z_O + 0.006)
    roof_joints(p, D_IO - 0.3, D_II + 0.28, Z_I + 0.006)
    for k, u in ((1, 1.6), (4, -1.6)):
        q, n, t = wing(k, A_O + D_OI / 2 - 0.05, u)
        yaw = math.degrees(math.atan2(t.y, t.x))
        stamp(p, lambda s: ub.fan_box(s, (0.0, 0.0), (2.4, 1.25, 0.95), fans=2), (q.x, q.y, Z_O), yaw)
    # o armário de rádio com as antenas e a sirene (ala 2, perto da quina de trás)
    q, n, t = wing(2, A_O + D_OI / 2 - 0.05, 2.6)
    yaw = math.degrees(math.atan2(t.y, t.x))

    def cabinet(s):
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 0.62), (1.3, 1.1, 1.2), X, Y, Z)
        louvers(s, (0.0, -0.56, 0.65), (0, -1, 0), (1, 0, 0), 0.8, 0.6, 6)
        ub.obox(s, 'Player#sm', (0.0, 0.56, 0.9), (1.0, 0.03, 0.24), X, Y, Z)
        ub.antenna(s, 0.35, 0.25, 1.22, 4.4, r=0.065)
        ub.antenna(s, -0.35, -0.25, 1.22, 3.2, r=0.055)
        ub.beacon(s, (0.35, 0.25, 1.22 + 4.44), r=0.08, h=0.16, cage=False)
    stamp(p, cabinet, (q.x, q.y, Z_O), yaw)
    # a escotilha do teto de fora (ala 1, perto da quina da frente)
    q, n, t = wing(1, A_O + D_OI / 2 - 0.05, -3.4)
    yaw = math.degrees(math.atan2(t.y, t.x))

    def hatch(s):
        ub.obox(s, 'Body#pnl', (0.0, 0.0, 0.2), (0.9, 0.9, 0.4), X, Y, Z)
        ub.obox(s, 'Body#sm', (0.03, 0.0, 0.43), (0.8, 0.82, 0.06), X, Y, Z)
        vp.hinge(s, 'Trim', (-0.4, 0.0, 0.44), 'y', 0.6, r=0.025, knuckles=3)
    stamp(p, hatch, (q.x, q.y, Z_O), yaw)
    # 2 domos pequenos (as antenas de satélite fixas) no anel de dentro, alas 1 e 4
    for k in (1, 4):
        q, n, t = wing(k, A_O + (D_IO + D_II) / 2, 0.0)
        vp.lathe(p, 'Body#vp', (q.x, q.y, Z_I), 'z', [(0.0, 0.0), (0.42, 0.0), (0.42, 0.1), (0.3, 0.16), (0.3, 0.45), (0.0, 0.45)], seg=20)
        prof = [(0.5 * math.cos(math.radians(a)), 0.45 + 0.5 * math.sin(math.radians(a))) for a in range(-20, 91, 10)]
        prof[-1] = (0.0, prof[-1][1])
        vp.lathe(p, 'Paint#vp', (q.x, q.y, Z_I), 'z', [(0.3, 0.45)] + prof, seg=28)
        vp.lathe(p, 'Player#vp', (q.x, q.y, Z_I), 'z', [(0.47, 0.42), (0.505, 0.42), (0.505, 0.52), (0.47, 0.52)], seg=28, closed=True)
    # a escada de marinheiro da face 2 (de trás), do chão ao teto de fora
    a, eu, en, w = PENT.face(2)
    q = a + eu * (w * 0.7)
    ub.ladder(p, 'Trim', q.x, q.y, FLOOR, Z_O, tuple(en), w=0.5, cage=True, wall=0.62, top_ext=1.0)


# ---------------------------------------------------------------------------------- a garagem e o portão blindado
def garage(p):
    """O vão fundo e escuro (da fachada até o anel de dentro, atravessando o poço): o piso, as paredes com as nervuras, o teto com as
    luzes, o portão de dentro, as faixas de perigo; o bloco que fecha o vão no poço."""
    gw, gh = GATE_W, GATE_H
    ub.obox(p, 'Concrete#pnl', ((GX0 + XF) / 2, 0.0, -0.002), (XF - GX0 + 0.1, 2 * gw + 0.3, 0.236), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Body#sm', ((GX0 + XF) / 2, s * (gw + 0.06), (FLOOR + gh) / 2), (XF - GX0, 0.12, gh - FLOOR), X, Y, Z)
        for k in range(3):
            ub.obox(p, 'Trim#sm', (GX0 + 0.7 + k * 1.1, s * (gw - 0.02), (FLOOR + gh) / 2), (0.14, 0.08, gh - FLOOR - 0.1), X, Y, Z)
        ub.flat_paint(p, 'Hazard', [(GX0 + 0.2, s * (gw - 0.45)), (XF - 0.2, s * (gw - 0.45)), (XF - 0.2, s * (gw - 0.15)),
                                    (GX0 + 0.2, s * (gw - 0.15))][::s], FLOOR - 0.004, FLOOR + 0.012)
        # o bloco que fecha o vão no poço (as paredes de lado até o teto)
        ub.obox(p, 'Body#pnl', (GX0 + 0.5, s * (gw + 0.27), (FLOOR + Z_O - 0.02) / 2), (1.2, 0.42, Z_O - 0.02 - FLOOR), X, Y, Z)
    ub.obox(p, 'Body#sm', ((GX0 + XF) / 2, 0.0, gh + 0.06), (XF - GX0, 2 * gw + 0.12, 0.12), X, Y, Z)
    ub.obox(p, 'Concrete#pnl', (GX0 + 0.5, 0.0, (gh + 0.12 + Z_O - 0.02) / 2), (1.2, 2 * gw + 0.9, Z_O - 0.02 - gh - 0.12), X, Y, Z)
    for k in range(3):
        ub.obox(p, 'Light#vp', (GX0 + 0.7 + k * 1.1, 0.0, gh - 0.015), (0.16, 2.8, 0.03), X, Y, Z)
    # o portão de dentro (de enrolar, fechado): a moldura, as lâminas, a faixa azul
    ub.obox(p, 'Trim#sm', (GX0 + 0.06, 0.0, (FLOOR + gh) / 2), (0.1, 2 * gw - 0.1, gh - FLOOR), X, Y, Z)
    for k in range(13):
        ub.obox(p, 'Dark#vp', (GX0 + 0.115, 0.0, FLOOR + 0.3 + k * 0.33), (0.012, 2 * gw - 0.4, 0.035), X, Y, Z)
    ub.obox(p, 'Player#sm', (GX0 + 0.13, 0.0, gh - 0.5), (0.03, 2.6, 0.3), X, Y, Z)
    # a linha amarela tracejada no meio
    for k in range(3):
        x = GX0 + 0.5 + k * 1.1
        ub.flat_paint(p, 'Yellow', [(x, -0.07), (x + 0.6, -0.07), (x + 0.6, 0.07), (x, 0.07)], FLOOR - 0.004, FLOOR + 0.012)


def blast_doors(p):
    """O portão blindado: as 2 folhas de correr de 60 cm paradas abertas na frente da fachada (as chapas aparafusadas, a borda de
    perigo com a borracha, os carrinhos de baixo, os pendurais), a viga do trilho (a faixa azul, o perigo nas pontas, as mãos-
    francesas, os holofotes, as balizas), os trilhos no chão e a moldura de aço do vão."""
    yi, yo, dx0, dx1, zt = LEAF
    x0, x1 = XF + dx0, XF + dx1
    for s in (-1, 1):
        yc = s * (yi + yo) / 2
        ub.obox(p, 'Body#pnl', ((x0 + x1) / 2, yc, (FLOOR + 0.06 + zt) / 2), (x1 - x0, yo - yi, zt - FLOOR - 0.06), X, Y, Z)
        # as chapas de blindagem na face da frente (2 × 3) com as fileiras de parafusos
        cw, ch = (yo - yi - 0.55) / 2, (zt - FLOOR - 0.3) / 3
        for i in range(2):
            for j in range(3):
                yy = s * (yi + 0.45 + cw * (i + 0.5))
                zz = FLOOR + 0.2 + ch * (j + 0.5)
                ub.obox(p, 'Body#pnl', (x1 + 0.03, yy, zz), (0.08, cw - 0.08, ch - 0.08), X, Y, Z)
                vp.bolt_row(p, 'Dark', (x1 + 0.07, yy - cw / 2 + 0.14, zz + ch / 2 - 0.13), (x1 + 0.07, yy + cw / 2 - 0.14, zz + ch / 2 - 0.13),
                            6, (1, 0, 0), r=0.035, h=0.025)
        # a borda de perigo (a ponta que fecha o vão) e a borracha
        ub.obox(p, 'Hazard#sm', (x1 + 0.04, s * (yi + 0.2), (FLOOR + zt) / 2), (0.08, 0.36, zt - FLOOR - 0.1), X, Y, Z)
        ub.obox(p, 'Dark#sm', ((x0 + x1) / 2, s * (yi - 0.03), (FLOOR + zt) / 2), (x1 - x0 - 0.1, 0.08, zt - FLOOR - 0.2), X, Y, Z)
        # os carrinhos de baixo e os pendurais de cima
        for yy in (yi + 0.6, yo - 0.6):
            ub.obox(p, 'Trim#sm', ((x0 + x1) / 2, s * yy, FLOOR + 0.12), (x1 - x0 + 0.16, 0.5, 0.24), X, Y, Z)
            ub.obox(p, 'Trim#sm', ((x0 + x1) / 2, s * yy, (zt + BEAM[0]) / 2 + 0.02), (0.3, 0.3, BEAM[0] - zt + 0.24), X, Y, Z)
        # a alça de puxar
        vp.grab_handle(p, 'Trim', (x1 + 0.08, s * (yi + 0.75), 1.1), (x1 + 0.08, s * (yi + 0.75), 1.9), (1, 0, 0), height=0.1, r=0.03)
    # a viga do trilho
    xb0, xb1 = XF - 0.05, XF + 0.85
    zb0, zb1 = BEAM
    ub.obox(p, 'Body#pnl', ((xb0 + xb1) / 2, 0.0, (zb0 + zb1) / 2), (xb1 - xb0, 2 * (yo + 0.2), zb1 - zb0), X, Y, Z)
    ub.obox(p, 'Player#sm', (xb1 + 0.02, 0.0, (zb0 + zb1) / 2 + 0.02), (0.05, 2 * yo - 1.4, 0.3), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Hazard#sm', (xb1 + 0.02, s * (yo - 0.25), (zb0 + zb1) / 2), (0.05, 0.62, zb1 - zb0 - 0.12), X, Y, Z)
        ub.beacon(p, ((xb0 + xb1) / 2, s * (yo - 0.1), zb1 + 0.08), r=0.1, h=0.2)
        vp.prism(p, 'Trim#vp', [(0.0, 0.0), (0.0, -0.75), (0.7, 0.0)], Vector((XF, s * (yo + 0.13) - 0.06, zb0)), Vector(X), Vector(Z),
                 Vector(Y), 0.12)
    for y in (-2.2, 2.2):
        ub.obox(p, 'Dark#sm', (xb1 + 0.08, y, zb0 + 0.2), (0.14, 0.12, 0.22), X, Y, Z)
        ub.lamp_head(p, (xb1 + 0.22, y, zb0 + 0.2), (1.0, 0.0, -1.0), w=0.6, h=0.3)
    ub.obox(p, 'Trim#sm', ((xb0 + xb1) / 2, 0.0, zb1 + 0.04), (xb1 - xb0 + 0.1, 2 * (yo + 0.25), 0.08), X, Y, Z)
    # os 2 trilhos no chão (atravessam o vão: o Dozer passa por cima)
    for xr in (x0 + 0.15, x1 - 0.15):
        ub.obox(p, 'Trim#sm', (xr, 0.0, FLOOR + 0.01), (0.12, 2 * (yo + 0.1), 0.05), X, Y, Z)
    # a moldura de aço do vão na fachada (os batentes e a verga)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', (XF + 0.05, s * (GATE_W + 0.12), (FLOOR + GATE_H) / 2), (0.2, 0.24, GATE_H - FLOOR), X, Y, Z)
    ub.obox(p, 'Trim#sm', (XF + 0.05, 0.0, GATE_H + 0.12), (0.2, 2 * GATE_W + 0.54, 0.24), X, Y, Z)


def apron(p):
    """A saída do Dozer SEM a laje quadrada (a regra do Felipe, 01/11: "o bagulho é redondo", a base acompanha a forma): a soleira
    estreita ao longo da fachada da frente (onde as folhas correm nos trilhos), a rampa curta só na largura do portão até o chão, a
    faixa de perigo, as 2 faixas amarelas da pista na soleira e os frades no chão."""
    x0, x1 = XF + 0.02, XF + 1.0
    ub.slab(p, 'Concrete#pnl', x0, x1, -6.95, 6.95, -0.12, FLOOR)
    rw, xr = GATE_W + 0.4, XF + 2.3
    vp.prism(p, 'Concrete#vp', [(x1 - 0.02, -0.12), (xr, -0.12), (xr, 0.012), (x1 - 0.02, FLOOR)], Vector((0.0, -rw, 0.0)), Vector(X),
             Vector(Z), Vector(Y), 2 * rw)
    for s_ in (-1, 1):
        ub.obox(p, 'Trim#sm', ((x1 + xr) / 2, s_ * (rw - 0.04), (FLOOR + 0.012) / 2 + 0.02), (xr - x1 + 0.02, 0.08, 0.1), X, Y, Z)
        ub.flat_paint(p, 'Yellow', [(x0 + 0.85, s_ * GATE_W - 0.07), (x1 - 0.03, s_ * GATE_W - 0.07), (x1 - 0.03, s_ * GATE_W + 0.07),
                                    (x0 + 0.85, s_ * GATE_W + 0.07)], FLOOR - 0.004, FLOOR + 0.012)
        vp.lathe(p, 'Hazard#vp', (xr - 0.25, s_ * (rw + 0.45), 0.0), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.95), (0.13, 1.02), (0.0, 1.04)], seg=14)
    ub.flat_paint(p, 'Hazard', [(x0 + 0.62, -GATE_W), (x1 - 0.03, -GATE_W), (x1 - 0.03, GATE_W), (x0 + 0.62, GATE_W)], FLOOR - 0.004,
                  FLOOR + 0.012)


# ---------------------------------------------------------------------------------- a torre
def hex_pts(R, rot=90.0):
    return [(R * math.cos(math.radians(rot + 60 * i)), R * math.sin(math.radians(rot + 60 * i))) for i in range(6)]


def inside_hex(u, v, R):
    """(u, v) dentro do hexágono de ponta pra cima com raio R."""
    for i in range(6):
        a = math.radians(60 * i)                          # as normais das 6 faces (a ponta pra cima: faces a 0°, 60°, ...)
        if u * math.cos(a) + v * math.sin(a) > R * math.sqrt(3) / 2:
            return False
    return True


def hex_frame(p, m, C, eu, ev, en, R0, R1, d0, d1):
    """A moldura hexagonal sólida (um anel só, sem vértice em comum entre pedaços): de R1 (dentro) a R0 (fora), de d0 a d1 na normal."""
    b = p.bm(m, ub._bev(m))
    rings = []
    for R, d in ((R0, d0), (R0, d1), (R1, d1), (R1, d0)):
        rings.append([b.verts.new(V(*(C + eu * u + ev * v + en * d))) for u, v in hex_pts(R)])
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(6):
            b.faces.new([ra[k], ra[(k + 1) % 6], rb[(k + 1) % 6], rb[k]])


def radar_face(p, k):
    """Um dos 4 radares de antena plana (a face grande k da casa das antenas): a moldura hexagonal clara, o fundo escuro e os módulos
    quadrados (os RMA de 60 cm do SPY-6, aqui 0,29 m no passo de 0,33 pra ler na câmera)."""
    pa, pb = house_plan(H0), house_plan(H1)
    a0, eu, en, w0 = pa.face(k)
    a1, _, _, w1 = pb.face(k)
    mb = a0 + eu * (w0 / 2) + Vector((0, 0, H0))
    mt = a1 + eu * (w1 / 2) + Vector((0, 0, H1))
    ev = (mt - mb).normalized()
    nn = eu.cross(ev).normalized()
    if nn.dot(en) < 0:
        nn = -nn
    C = mb + ev * ((mt - mb).length * 0.53)
    hex_frame(p, 'Paint#sm', C, eu, ev, nn, 1.38, 1.19, -0.04, 0.16)
    pts = hex_pts(1.21)
    vp.prism(p, 'Dark#vp', pts, C - nn * 0.03, eu, ev, nn, 0.08)
    pitch, tile = 0.33, 0.29
    n = 5
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            u, v = i * pitch, j * pitch
            h = tile / 2 + 0.02
            if all(inside_hex(u + su * h, v + sv * h, 1.17) for su in (-1, 1) for sv in (-1, 1)):
                ub.obox(p, 'Trim#vp', C + eu * u + ev * v + nn * 0.07, (tile, tile, 0.06), eu, ev, nn)


def tapered_band(p, m, z0, z1, out, inn):
    """A faixa em volta da casa das antenas (o tronco de pirâmide): o anel entre z0 e z1, de 'inn' a 'out' de recuo."""
    b = p.bm(m, ub._bev(m))
    rings = [[b.verts.new(V(*q)) for q in house_plan(z, d).ring(0.0, z)] for z, d in ((z0, out), (z1, out), (z1, inn), (z0, inn))]
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(8):
            b.faces.new([ra[k], ra[(k + 1) % 8], rb[(k + 1) % 8], rb[k]])


def tower(p):
    """A torre de comando no pátio: o fuste de placas, o colar, a casa das antenas com os 4 radares e a faixa azul, o deque com o
    guarda-corpo, o pedestal do domo; as portas; as 2 passarelas até o anel de dentro; o anel amarelo no pátio."""
    cx, cy = C0
    # o pé de concreto e o fuste
    poly_band(p, 'Concrete#pnl', SHAFT, 0.35, -0.2, -0.1, FLOOR + 0.2)
    gcore(p, SHAFT, 0.0, FLOOR + 0.18, H0 - 0.15, m='Body#pnl')
    for zb in (2.6, H0 - 0.95):                     # as faixas de aço do fuste (o de cima aparece acima do anel de dentro)
        poly_band(p, 'Trim#sm', SHAFT, 0.05, -0.05, zb, zb + 0.22)
    # a porta do pátio (de frente pro portão) e as das passarelas: o fundo escuro, a folha, a marquise
    for ang, z0 in ((0, FLOOR + 0.2), (90, BR_Z), (270, BR_Z)):
        d = Vector((math.cos(math.radians(ang)), math.sin(math.radians(ang)), 0.0))
        e = Vector((-d.y, d.x, 0.0))
        q = PENT.c + d * 2.6
        ub.obox(p, 'Dark#vp', q + d * 0.02 + Vector((0, 0, z0 + 1.0)), (0.95, 2.0, 0.06), e, Z, d)
        ub.obox(p, 'Trim#sm', q + d * 0.05 + Vector((0, 0, z0 + 0.97)), (0.82, 1.9, 0.06), e, Z, d)
        if ang == 0:
            ub.obox(p, 'Trim#sm', q + d * 0.35 + Vector((0, 0, z0 + 2.2)), (1.3, 0.07, 0.6), e, Z, d)
    # o colar e a casa das antenas
    poly_slab(p, 'Body#pnl', oct_plan(2.62, 3.02), 0.0, H0 - 0.23, H0 + 0.01, cell=1.2)
    poly_band(p, 'Trim#sm', oct_plan(2.62, 3.02), 0.06, -0.06, H0 - 0.29, H0 - 0.21)
    p.loft('Body#pnl', [house_plan(H0).ring(0.0, H0), house_plan(H1).ring(0.0, H1)], bevel=ub._bev('Body#pnl'), caps=(False, False))
    tapered_band(p, 'Player#pnl', H0 + 0.07, H0 + 0.52, 0.05, -0.08)
    for k in (1, 3, 5, 7):
        radar_face(p, k)
    # as faces das quinas: a veneziana do ar e as luzes de serviço
    pa, pb = house_plan(H0 + 1.4), house_plan(H0 + 2.6)
    for k in (0, 2, 4, 6):
        a0, eu, en, w0 = pa.face(k)
        a1, _, _, w1 = pb.face(k)
        mb, mt = a0 + eu * (w0 / 2) + Vector((0, 0, H0 + 1.4)), a1 + eu * (w1 / 2) + Vector((0, 0, H0 + 2.6))
        ev = (mt - mb).normalized()
        nn = eu.cross(ev).normalized()
        if nn.dot(en) < 0:
            nn = -nn
        louvers(p, (mb + mt) / 2 + nn * 0.02, tuple(nn), tuple(eu), 0.6, 0.9, 7)
    # o deque com a borda e o guarda-corpo
    deck = oct_plan(1.94, 2.34)
    poly_slab(p, 'Concrete#pnl', deck, 0.0, H1 - 0.16, H1 + 0.04, cell=1.0)
    poly_band(p, 'Trim#sm', deck, 0.05, -0.08, H1 - 0.24, H1 + 0.07)
    rp = [tuple(deck.corner(k, -0.1))[:2] for k in range(8)]
    ub.rail(p, 'Trim', rp + [rp[0]], H1 + 0.04, h=0.95, r=0.03, step=1.1, toe='Trim')
    # o pedestal do domo
    vp.lathe(p, 'Body#vp', (cx, cy, H1 + 0.04), 'z', [(0.0, 0.0), (1.34, 0.0), (1.34, 0.12), (1.2, 0.2), (1.16, RADAR_AT[2] - H1 - 0.12),
                                                     (1.24, RADAR_AT[2] - H1 - 0.08), (1.24, RADAR_AT[2] - H1 - 0.04), (0.0, RADAR_AT[2] - H1 - 0.04)],
             seg=40)
    vp.bolt_ring(p, 'Dark', (cx, cy, H1 + 0.16), 'z', 1.27, 20, r=0.03, h=0.02)
    # a escada de marinheiro do deque (face de trás, 180°) até o teto do anel de dentro não existe: o acesso é pelas passarelas
    # as 2 passarelas (±Y) da torre até a parede do pátio no 2º andar, com a porta do lado do anel
    for s, k in ((1, 1), (-1, 4)):
        y0, y1 = s * 2.55, s * 5.05
        ub.obox(p, 'Trim#sm', (cx, (y0 + y1) / 2, BR_Z - 0.12), (1.3, abs(y1 - y0), 0.24), X, Y, Z)
        ub.obox(p, 'Body#sm', (cx, (y0 + y1) / 2, BR_Z - 0.42), (0.5, abs(y1 - y0), 0.36), X, Y, Z)
        for xx in (-0.62, 0.62):
            ub.rail(p, 'Trim', [(cx + xx, y0 + s * 0.05), (cx + xx, s * 4.6)], BR_Z, h=1.0, r=0.028, step=1.1)
        n = PENT.nrm(k)
        e = Vector((-n.y, n.x, 0.0))
        q = PENT.c + Vector((0.0, s * (A_O + D_II) / math.cos(math.radians(18)), 0.0))
        ub.obox(p, 'Dark#vp', q - n * 0.02 + Vector((0, 0, BR_Z + 0.85)), (1.0, 1.7, 0.06), e, Z, -n)
        ub.obox(p, 'Trim#sm', q - n * 0.05 + Vector((0, 0, BR_Z + 0.83)), (0.86, 1.6, 0.06), e, Z, -n)
    # o pátio: o anel amarelo em volta da torre, a caixa de cabos e o gerador
    ub.ring_paint(p, 'Yellow', C0, 3.05, 3.2, FLOOR - 0.004, FLOOR + 0.012, seg=56)
    ub.obox(p, 'Body#pnl', (cx - 3.55, 1.0, FLOOR + 0.55), (0.9, 1.6, 1.1), X, Y, Z)
    louvers(p, (cx - 3.09, 1.0, FLOOR + 0.6), (1, 0, 0), (0, 1, 0), 1.0, 0.55, 6)


# ---------------------------------------------------------------------------------- o heliponto
def helipad(p):
    """O heliponto na ala 3: o deque octogonal de chapa sobre as colunas (no teto de fora e no de dentro), as vigas, a rede de
    segurança inclinada, as luzes de borda, a tinta (o contorno branco, as marcas azuis, o círculo amarelo, o "H"), a escada."""
    hp = Plan(HELI, [(HELI_ANG + 45 * k, DECK_AP) for k in range(8)])     # a face 0 olha pra fora; a 1, a da escada
    z0 = DECK_TOP - DECK_T
    poly_slab(p, 'Trim#pnl', hp, 0.0, z0, DECK_TOP, cell=1.4)
    poly_band(p, 'Trim#sm', hp, -0.05, -0.4, z0 - 0.3, z0 + 0.02)
    # as colunas: 2 no teto de fora, 2 no de dentro (com a sapata e as mãos-francesas), e as vigas embaixo
    r = Vector((math.cos(math.radians(HELI_ANG)), math.sin(math.radians(HELI_ANG)), 0.0))
    t = Vector((-r.y, r.x, 0.0))
    H = Vector((HELI[0], HELI[1], 0.0))
    for dr, zb in ((1.55, Z_O), (-1.55, Z_I)):
        for dt in (-1.55, 1.55):
            q = H + r * dr + t * dt
            vp.lathe(p, 'Body#vp', (q.x, q.y, zb), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.12), (0.2, 0.18), (0.18, z0 - 0.3 - zb), (0.0, z0 - 0.3 - zb)],
                     seg=16)
            vp.bolt_ring(p, 'Dark', (q.x, q.y, zb + 0.12), 'z', 0.25, 6, r=0.03, h=0.02)
        ub.obox(p, 'Trim#sm', H + r * dr + Vector((0, 0, z0 - 0.17)), (0.26, 5.0, 0.34), r, t, Z)
    for dt in (-1.55, 1.55):
        ub.obox(p, 'Trim#sm', H + t * dt + Vector((0, 0, z0 - 0.17)), (5.0, 0.24, 0.34), r, t, Z)
    # a rede de segurança inclinada (menos na face da escada, a 1: em cima do teto de fora) e as mãos-francesas
    k_out = 1
    for k in range(8):
        a, eu, en, w = hp.face(k, 0.0)
        if k == k_out:
            continue
        tilt = math.radians(8)
        ev = en * math.cos(tilt) + Vector((0, 0, math.sin(tilt)))
        nn = eu.cross(ev).normalized()
        if nn.z < 0:
            nn = -nn
        mid = a + eu * (w / 2)
        ub.obox(p, 'Dark#vp', mid + ev * 0.34 + Vector((0, 0, DECK_TOP - 0.16)), (w - 0.1, 0.55, 0.025), eu, ev, nn)
        o0 = a + en * 0.62 + Vector((0, 0, DECK_TOP - 0.16 + 0.62 * math.tan(tilt)))
        vp.tube(p, 'Trim', [o0 + eu * 0.05, o0 + eu * (w - 0.05)], 0.035, seg=8)
        for tt in (0.2, 0.8):
            q = a + eu * (w * tt)
            vp.tube(p, 'Trim', [q + Vector((0, 0, z0 - 0.25)), q + en * 0.5 + Vector((0, 0, DECK_TOP - 0.18))], 0.028, seg=6)
    # a escada de marinheiro da face 1, do teto de fora até o deque
    a, eu, en, w = hp.face(k_out, 0.0)
    q = a + eu * (w / 2)
    ub.ladder(p, 'Trim', q.x, q.y, Z_O, DECK_TOP, tuple(en), w=0.5, cage=False, wall=0.06, top_ext=1.0)
    # as luzes de borda
    for k in range(16):
        ang = 2 * math.pi * (k + 0.5) / 16
        rr = (DECK_AP - 0.32) / math.cos(((ang - math.radians(HELI_ANG) + math.pi / 8) % (math.pi / 4)) - math.pi / 8)
        ub.runway_light(p, HELI[0] + rr * math.cos(ang), HELI[1] + rr * math.sin(ang), DECK_TOP - 0.01, lens='Light', r=0.1)
    # a tinta: o contorno branco, as 4 marcas azuis, o círculo amarelo, o "H" (as pernas ao longo do raio: de pé na câmera)
    zp0, zp1 = DECK_TOP - 0.004, HELI_Z
    poly_band(p, 'Paint#vp', hp, -0.5, -0.66, zp0, zp1)
    for k in (1, 3, 5, 7):
        a, eu, en, w = hp.face(k, -0.95)
        mid = a + eu * (w / 2)
        qs = [mid + eu * su * 0.5 + en * sv * 0.13 for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        ub.flat_paint(p, 'Player', [(v.x, v.y) for v in qs], zp0, zp1)
    ub.ring_paint(p, 'Yellow', HELI, 1.85, 2.1, zp0, zp1, seg=56)
    hh = [(-1.05, -0.68), (1.05, -0.68), (1.05, -0.38), (0.15, -0.38), (0.15, 0.38), (1.05, 0.38), (1.05, 0.68), (-1.05, 0.68),
          (-1.05, 0.38), (-0.15, 0.38), (-0.15, -0.38), (-1.05, -0.38)]
    ub.flat_paint(p, 'Paint', [tuple(H + r * u + t * v)[:2] for u, v in hh], zp0, zp1)


# ---------------------------------------------------------------------------------- o chão
def hesco_row(p, a, b, h=1.1, depth=1.0):
    """A fileira de HESCO (as caixas de arame com o saco de areia): as células com a quina redonda, o topo de areia abaulado e os
    montantes de arame nas juntas."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    L = (B - A).length
    d = (B - A).normalized()
    e = Vector((-d.y, d.x, 0.0))
    n = max(1, int(round(L / 1.0)))
    yaw = math.degrees(math.atan2(d.y, d.x))
    for i in range(n):
        c = A + d * (L * (i + 0.5) / n)
        vp.rbox(p, 'CivCloth#vp', (c.x, c.y, h / 2), (L / n - 0.05, depth, h), yaw=yaw, r=0.09, k=2)
        vp.rbox(p, 'CivCloth#vp', (c.x, c.y, h + 0.04), (L / n * 0.84, depth * 0.84, 0.1), yaw=yaw, r=0.14, k=2)
    for i in range(n + 1):
        c = A + d * (L * i / n)
        for s in (-1, 1):
            q = c + e * (s * depth / 2)
            vp.tube(p, 'Trim', [(q.x, q.y, 0.0), (q.x, q.y, h + 0.02)], 0.022, seg=6)


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


def ground(p):
    # o mastro da bandeira (frente, +Y): a sapata, o poste torneado, a bola, o pano (o jogo desenha no vento)
    fx, fy = 11.7, 10.4
    ub.obox(p, 'Concrete#pnl', (fx, fy, 0.05), (1.1, 1.1, 0.5), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 0.3), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.05), (0.13, 0.1), (0.11, 0.4), (0.065, 11.2), (0.0, 11.2)],
             seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 11.5), 'z', [(0.0, 0.0), (0.07, 0.0), (0.13, 0.08), (0.13, 0.13), (0.07, 0.21), (0.0, 0.21)], seg=12)
    flag(p, (fx, fy), 11.4, 2.8, 1.7, m='Player')
    # os postes de luz dos 2 lados do pátio de saída, virados pro portão
    for s in (-1, 1):
        ub.lamp_post(p, 11.75, s * 7.75, h=6.5, aim=(10.0, 0.0), heads=2)
    # os ninhos de sacos de areia nas 2 quinas da frente
    sandbag_arc(p, (11.2, -10.4), 1.15, 100, 340)
    sandbag_arc(p, (9.4, 12.9), 1.0, 200, 400)
    # as fileiras de HESCO nas faces dos lados e de trás (a 1,7 m da fachada)
    for k, u0, u1 in ((1, -3.2, 3.2), (4, -3.2, 3.2), (2, -0.2, 4.6), (3, -4.6, 0.2)):
        a, n, t = wing(k, A_O + 1.75, u0)
        b, _, _ = wing(k, A_O + 1.75, u1)
        hesco_row(p, tuple(a)[:2], tuple(b)[:2])


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    outer_ring(p)
    well_and_inner(p)
    roofs(p)
    garage(p)
    blast_doors(p)
    apron(p)
    tower(p)
    helipad(p)
    ground(p)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o domo (origem no eixo)
DOME_ZC, DOME_R, DRUM_TOP = 1.4, 1.32, 0.78


def sphere_patch(b, C, R, th0, th1, ph0, ph1, t, nu=3, nv=2):
    """O painel do domo: o pedaço de casca esférica (de th0 a th1 do polo, de ph0 a ph1 em volta), sólido com espessura t."""
    def P(th, ph, rr):
        return C + Vector((math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))) * rr
    fr, bk = [], []
    for i in range(nu + 1):
        ph = ph0 + (ph1 - ph0) * i / nu
        fr.append([b.verts.new(V(*P(th0 + (th1 - th0) * j / nv, ph, R))) for j in range(nv + 1)])
        bk.append([b.verts.new(V(*P(th0 + (th1 - th0) * j / nv, ph, R - t))) for j in range(nv + 1)])
    for i in range(nu):
        for j in range(nv):
            b.faces.new([fr[i][j], fr[i + 1][j], fr[i + 1][j + 1], fr[i][j + 1]]).smooth = True
            b.faces.new([bk[i][j + 1], bk[i + 1][j + 1], bk[i + 1][j], bk[i][j]]).smooth = True
    for j in range(nv):
        b.faces.new([fr[0][j], fr[0][j + 1], bk[0][j + 1], bk[0][j]])
        b.faces.new([fr[nu][j + 1], fr[nu][j], bk[nu][j], bk[nu][j + 1]])
    for i in range(nu):
        b.faces.new([fr[i + 1][0], fr[i][0], bk[i][0], bk[i + 1][0]])
        b.faces.new([fr[i][nv], fr[i + 1][nv], bk[i + 1][nv], bk[i][nv]])


def radar():
    """O domo do link de satélite que gira (origem no eixo, x local = o lado da antena): a mesa giratória, o tambor com a faixa azul e
    a portinhola, o domo de painéis desencontrados com a junta escura (o painel do lado da antena em azul, em cada fileira), o
    para-raios com a luz de aviso, o braço com a antena de satélite (o prato, o alimentador nas 3 hastes) e, do outro lado, a caixa de
    equipamento com a sirene."""
    p = CPart(PREFIX + '_Radar', cull=False)
    ub.keys(p)
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (1.3, 0.0), (1.3, 0.16), (1.22, 0.22), (0.0, 0.22)], seg=48)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.22), 'z', 1.12, 24, r=0.03, h=0.02)
    vp.lathe(p, 'Body#vp', (0, 0, 0.22), 'z', [(0.0, 0.0), (1.2, 0.0), (1.2, DRUM_TOP - 0.28), (1.16, DRUM_TOP - 0.22), (0.0, DRUM_TOP - 0.22)],
             seg=48)
    vp.lathe(p, 'Player#vp', (0, 0, 0.22), 'z', [(1.19, 0.1), (1.225, 0.1), (1.225, 0.32), (1.19, 0.32)], seg=48, closed=True)
    ub.obox(p, 'Trim#sm', (-0.02, -1.2, 0.45), (0.6, 0.06, 0.42), X, Y, Z)              # a portinhola
    # o domo: a esfera escura de baixo (aparece só nas juntas) e os painéis
    C = Vector((0.0, 0.0, DOME_ZC))
    thmax = math.acos((DRUM_TOP - 0.02 - DOME_ZC) / DOME_R)
    prof = [((DOME_R - 0.035) * math.sin(thmax * (1 - i / 12)), DOME_ZC + (DOME_R - 0.035) * math.cos(thmax * (1 - i / 12))) for i in range(13)]
    prof[-1] = (0.0, prof[-1][1])
    vp.lathe(p, 'Dark#vp', (0, 0, 0), 'z', prof, seg=40)
    cap = math.radians(20)
    prof = [((DOME_R + 0.005) * math.sin(cap * (1 - i / 4)), DOME_ZC + (DOME_R + 0.005) * math.cos(cap * (1 - i / 4))) for i in range(5)]
    prof[-1] = (0.0, prof[-1][1])
    vp.lathe(p, 'Paint#vp', (0, 0, 0), 'z', prof, seg=28)
    bands = [(cap + 0.02, math.radians(50)), (math.radians(50), math.radians(82)), (math.radians(82), thmax)]
    g = 0.035 / DOME_R
    for bi, (t0, t1) in enumerate(bands):
        n = (9, 13, 15)[bi]
        off = (0.0, 0.5, 0.0)[bi] * 2 * math.pi / n
        for i in range(n):
            ph0 = off + 2 * math.pi * i / n + g / 2
            ph1 = off + 2 * math.pi * (i + 1) / n - g / 2
            mid = (ph0 + ph1) / 2
            dist = abs((mid + math.pi) % (2 * math.pi) - math.pi)
            m = 'Player#dome' if dist < math.pi / n else 'Paint#dome'
            sphere_patch(p.bm(m, 0.0), C, DOME_R, t0 + g / 2, t1 - g / 2, ph0, ph1, 0.05, nu=3, nv=2)
    # o para-raios e a luz de aviso no topo
    zt = DOME_ZC + DOME_R
    vp.lathe(p, 'Trim#vp', (0, 0, zt - 0.03), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.06), (0.05, 0.1), (0.0, 0.1)], seg=12)
    vp.tube(p, 'Trim', [(0, 0, zt + 0.05), (0, 0, zt + 0.95)], 0.025, seg=6)
    ub.beacon(p, (0.0, 0.0, zt + 0.6), r=0.07, h=0.14, cage=False)
    # o braço da antena de satélite (+X local): a viga do tambor ao prato, a escora
    ub.obox(p, 'Trim#sm', (1.55, 0.0, 0.58), (0.8, 0.26, 0.2), X, Y, Z)
    vp.tube(p, 'Trim', [Vector((1.15, 0.0, 0.3)), Vector((1.85, 0.0, 0.62))], 0.05, seg=8)
    d = Vector((math.cos(math.radians(35)), 0.0, math.sin(math.radians(35))))
    e = Vector((-d.z, 0.0, d.x))
    P0 = Vector((2.0, 0.0, 0.98))
    vp.lathe(p, 'Trim#vp', P0 - d * 0.2, d, [(0.0, -0.1), (0.16, -0.1), (0.16, 0.12), (0.0, 0.12)], seg=12)
    vp.tube(p, 'Trim', [Vector((1.85, 0.0, 0.62)), P0 - d * 0.25], 0.06, seg=8)
    rim = 0.58
    dish = [(rr, 0.16 * (rr / rim) ** 2) for rr in [0.0, 0.15, 0.3, 0.42, 0.52, rim]] + [(rim + 0.03, 0.17), (rim + 0.03, 0.13)]
    vp.lathe(p, 'Paint#vp', P0, d, dish + [(rim - 0.02, 0.12)] + [(rr, 0.16 * (rr / rim) ** 2 - 0.04) for rr in (0.5, 0.35, 0.15)] + [(0.0, -0.04)],
             seg=28)
    F = P0 + d * 0.52
    for k in range(3):
        a = 2 * math.pi * k / 3
        q = P0 + d * 0.15 + (e * math.cos(a) + Vector((0, 1, 0)) * math.sin(a)) * (rim - 0.04)
        vp.tube(p, 'Trim', [q, F], 0.018, seg=5)
    ub.obox(p, 'Dark#sm', F, (0.16, 0.14, 0.14), d, Vector((0, 1, 0)), e)
    # a caixa de equipamento do outro lado (−X local) com a sirene
    ub.obox(p, 'Body#pnl', (-1.5, 0.0, 0.52), (0.62, 0.72, 0.52), X, Y, Z)
    ub.obox(p, 'Player#sm', (-1.82, 0.0, 0.58), (0.03, 0.56, 0.16), X, Y, Z)
    ub.obox(p, 'Trim#sm', (-1.15, 0.0, 0.4), (0.3, 0.4, 0.2), X, Y, Z)
    ub.beacon(p, (-1.5, 0.0, 0.78), r=0.08, h=0.16)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o andaime
def scaffold_runs(name, runs, h, out=0.85, post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra em trechos [(a, b)] (o do kit pelo contorno, aqui trecho a trecho: perto da ponta de trás o lote acaba e o
    trecho é cortado); 'out' pra fora do centro do pentágono."""
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
    """O andaime nas 4 faces com a saia (a frente fica livre: o portão), cortado perto da ponta de trás (o lote acaba em x = −12,64)."""
    dl = SKIRT[0] + 0.2
    runs = []
    for k in SKIRTED:
        a = PENT.corner(k - 1, dl)
        b = PENT.corner(k, dl)
        # o trecho termina antes de o lado de fora sair do lote
        n = PENT.nrm(k)
        def ok(q):
            o = q + n * 0.85
            return o.x > -HX + 0.1 and abs(o.y) < HY - 0.1 and q.x > -HX + 0.1
        ts = [i / 40 for i in range(41)]
        good = [t for t in ts if ok(a + (b - a) * t)]
        t0, t1 = min(good), max(good)
        if k == 1:
            t0 = max(t0, 0.06)
        if k == 4:
            t1 = min(t1, 0.94)
        runs.append((tuple(a + (b - a) * t0)[:2], tuple(a + (b - a) * t1)[:2]))
    return scaffold_runs(PREFIX + '_Scaffold', runs, Z_O + 0.4)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_command_center_v3')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    r = radar()
    sc = scaffold()
    check([b, r, sc], HX, HY, at={r.name: RADAR_AT})
    sweep = max(math.hypot(v.co.x, v.co.y) for v in r.data.vertices)
    zlow = min(v.co.z for v in r.data.vertices if math.hypot(v.co.x, v.co.y) > 1.9)
    print('MODEL domo: varre r = %.2f m; o mais baixo fora de r 1,9 m a z = %.2f m (o guarda-corpo do deque acaba em %.2f)' %
          (sweep, zlow + RADAR_AT[2], H1 + 0.04 + 0.95 + 0.03))
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, r, sc)})
    r = bake_in_place(r, RADAR_AT)
    total = export(b, out)
    total += export(r, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL spin at_m', [round(v, 3) for v in RADAR_AT], 'rpm 3')
    print('MODEL helipad_m', [round(HELI[0], 3), round(HELI[1], 3), round(HELI_Z, 3)])
    q1, _, _ = wing(1, A_O + D_OI / 2, -1.6)
    q4, _, _ = wing(4, A_O + D_OI / 2, 1.6)
    q2, _, _ = wing(2, A_O + (D_IO + D_II) / 2, 1.0)
    print('MODEL fire_points_m', [[round(q1.x, 2), round(q1.y, 2), Z_O], [round(q4.x, 2), round(q4.y, 2), Z_O],
                                  [round(q2.x, 2), round(q2.y, 2), Z_I]])
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]]],
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1400]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                  ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 900]],
                  ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
                 (4.0, 19.5, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
