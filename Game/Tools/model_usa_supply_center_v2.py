"""O Supply Center da USA (usa_supply_center), VERSÃO B no detalhe AAA (01/11/2026): a de antes era a caixa do script antigo
(Game/Tools/blender_provisional.py supply_center(), a do jogo hoje); esta é a refeita no nível do Strategy Center B.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_supply_center_v2.py C:/Users/luisf/explosion/captures/game/models/usa_supply_center_v2

A PALAVRA (docs/CONCEITO_CONSTRUCOES.md §3b): O HELIPONTO. O Chinook pousa e decola com a carga: de cima, o "H" branco no círculo
amarelo, no octógono escuro de chapa de aço com a borda branca e as 4 marcas azuis (a cor do jogador), em cima do depósito (o
heliponto de laje escura do alvo aprovado docs/aprovados/alvos/r1_raid_slabs_gemini.webp, levado pro teto: o helipad_m é a 4,864 m). O deque é UM objeto: a
plataforma suspensa nas 8 colunas por cima do teto do depósito (a fresta de sombra embaixo dela a separa do prédio), com a REDE DE
SEGURANÇA inclinada em volta (a moldura de tubo, as mãos-francesas), as luzes de borda, a escada com o patamar e a biruta.
  - o deque: a laje de chapa octogonal (apótema 5,0 m) com o topo da tinta em 4,864 m, o círculo de toque amarelo, o "H" branco com as
    pernas ao longo de X (de pé na câmera do jogo), o contorno azul, as 16 luzes de borda; embaixo, as vigas radiais, a viga de borda e
    as 8 colunas com as mãos-francesas;
  - o depósito (o pedestal do deque): as paredes de PLACAS com a fileira azul, os pilares redondos nas quinas cortadas, o parapeito com
    a capa, a laje de fundação na borda; na frente (+X) a BAÍA do caminhão: o vão fundo e escuro com a carga dentro e as lâmpadas do
    teto, a porta de enrolar recolhida (o tambor, as últimas lâminas), os trilhos, os batentes de perigo, a placa azul e os
    holofotes; do lado (+Y) a porta de serviço com a marquise; do outro (−Y) as venezianas; atrás a escada de marinheiro;
  - o teto do depósito: a escada até o deque, a caixa de ventiladores (a assinatura do teto da USA), o armário de rádio com as antenas
    e a sirene, o ar-condicionado, a biruta de manga listrada que cai;
  - o pátio da baía na frente (as lajes com a junta, as faixas amarelas da vaga do caminhão, a faixa de perigo na soleira) e a
    CARGA recebida dos dois lados: os caixotes em pilha com a LONA azul amarrada (a cor do jogador, o objeto do tanque); os postes
    de luz com 2 holofotes virados pra baía.
A USA limpa (13/10): sem desgaste pintado.

Lote 18,56 × 18,96 m (x −9,28..9,28, y −9,48..9,48), a frente em +X; o caminhão sai pela baía em +X (y = 0); nada sai do lote.
helipad_m MANTIDO: (−3,52; 0; 4,864) = o centro do deque e o topo da tinta do "H" (onde o Chinook pousa).
Peças: SM_USA_SupplyCenter_Base (tudo: nada gira; a vida é o Chinook indo e vindo, do jogo), SM_USA_SupplyCenter_Scaffold.
Slots (os do Strategy Center B): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate, Rubber.
O kit (CPart, Plan, poly_slab, plan_walls, ...) vem do model_usa_power_plant_v2.py.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import (CPart, Plan, chamfer_rect, poly_slab, poly_band, plan_core, plan_walls, plan_pillars,  # noqa: E402
                                      slab_tiles, louvers, scaffold_poly, check, preview_spec, X, Y, Z)

PREFIX = 'SM_USA_SupplyCenter'
COLORS.update({'Crate': (112, 100, 62), 'Rubber': (32, 32, 31)})

HX, HY = 9.28, 9.48               # o lote 18,56 × 18,96
FLOOR = 0.12
HELI = (-3.52, 0.0)               # o centro do deque (o helipad_m)
HELI_Z = 4.864                    # o topo da tinta do "H"
DECK_TOP = 4.85                   # o topo da chapa do deque (a tinta tem 1,4 cm)
DECK_T = 0.26
DECK = Plan(HELI, [(45 * k, 5.0) for k in range(8)])                  # o octógono do deque (a face 0 olha pra +X)
DEPOT = chamfer_rect((-3.3, 0.0), 4.9, 4.9, 8.3, 8.3, (0.6, 1.0, 1.0, 0.6))   # o depósito: x −8,2..1,6, y ±8,3
WALL_Z = 3.3                      # o topo das paredes do depósito
ROOF_Z = 3.35                     # o topo da laje do teto
BAY = (2.25, 3.0)                 # a baía: a meia largura do vão e a altura
STAIR_Y = (5.22, 6.12)            # a escada do teto até o deque (y)


def depot_face(normal):
    return [k for k in range(DEPOT.n) if DEPOT.nrm(k).dot(Vector(normal)) > 0.99][0]


# ---------------------------------------------------------------------------------- o depósito
def depot(p):
    # a borda de fundação (só o anel que aparece em volta do pé)
    poly_band(p, 'Concrete#pnl', DEPOT, 0.38, -0.3, -0.12, FLOOR)
    kf, ky, km, kb = depot_face((1, 0, 0)), depot_face((0, 1, 0)), depot_face((0, -1, 0)), depot_face((-1, 0, 0))
    _, _, _, wf = DEPOT.face(kf)
    _, _, _, wy = DEPOT.face(ky)
    _, _, _, wm = DEPOT.face(km)
    # os vãos no miolo (da quina de começo): a baía na frente, a porta de serviço em +Y
    core_holes = {kf: [(wf / 2 - BAY[0], wf / 2 + BAY[0], -0.1, BAY[1])], ky: [(wy / 2 + 2.0, wy / 2 + 3.2, -0.1, 2.25)]}
    plan_core(p, DEPOT, 0.0, FLOOR - 0.02, WALL_Z, holes=core_holes)
    # as placas (os mesmos vãos com a margem da parede) e a fileira azul
    m = 0.3
    wall_holes = {kf: [(wf / 2 - m - BAY[0] - 0.25, wf / 2 - m + BAY[0] + 0.25, -0.1, BAY[1] + 0.06)],
                  ky: [(wy / 2 - m + 1.9, wy / 2 - m + 3.3, -0.1, 2.32)],
                  km: [(wm / 2 - m - 4.9, wm / 2 - m - 2.1, 1.05, 2.15), (wm / 2 - m + 2.1, wm / 2 - m + 4.9, 1.05, 2.15)]}
    plan_walls(p, DEPOT, 0.0, FLOOR, WALL_Z - 0.05, rows=(0.75, 1.85, 2.35), blue=2, pw=2.45, holes=wall_holes)
    plan_pillars(p, DEPOT, 0.06, FLOOR, WALL_Z + 0.1, r=0.28)
    # o parapeito com a capa e a laje do teto
    poly_band(p, 'Body#pnl', DEPOT, 0.12, -0.24, WALL_Z - 0.12, WALL_Z + 0.34)
    poly_band(p, 'Trim#sm', DEPOT, 0.18, -0.3, WALL_Z + 0.34, WALL_Z + 0.42)
    poly_slab(p, 'Concrete#pnl', DEPOT, -0.22, WALL_Z - 0.2, ROOF_Z, cell=1.6)
    # as venezianas do lado −Y
    a, eu, en, w = DEPOT.face(km)
    for u in (w / 2 - 3.5, w / 2 + 3.5):
        louvers(p, a + eu * u + en * 0.09 + Vector((0, 0, FLOOR + 1.6)), tuple(en), tuple(eu), 2.4, 0.85, 9)
    # a porta de serviço (+Y) com a marquise e a luminária
    a, eu, en, w = DEPOT.face(ky)
    q = a + eu * (w / 2 + 2.6)
    ub.obox(p, 'Dark#vp', q - en * 0.1 + Vector((0, 0, (FLOOR + 2.2) / 2)), (1.24, 2.2 - FLOOR, 0.1), eu, Z, en)
    ub.obox(p, 'Trim#sm', q - en * 0.04 + Vector((0, 0, (FLOOR + 2.12) / 2 + 0.01)), (1.06, 2.12 - FLOOR - 0.04, 0.08), eu, Z, en)
    for k in range(3):
        ub.obox(p, 'Dark#sm', q + en * 0.01 + Vector((0, 0, FLOOR + 0.5 + k * 0.55)), (0.9, 0.09, 0.05), eu, Z, en)
    vp.grab_handle(p, 'Body', q + en * 0.0 + eu * 0.36 + Vector((0, 0, 0.95)), q + eu * 0.36 + Vector((0, 0, 1.4)), tuple(en), height=0.06,
                   r=0.02)
    ub.obox(p, 'Trim#sm', q + en * 0.45 + Vector((0, 0, 2.55)), (1.8, 0.08, 0.8), eu, Z, en)
    ub.lamp_head(p, q + en * 0.3 + Vector((0, 0, 2.38)), tuple(en * 1.0 + Vector((0, 0, -1.2))), w=0.4, h=0.22)
    ub.obox(p, 'Hazard#sm', q + en * 0.25 + Vector((0, 0, FLOOR + 0.012)), (1.4, 0.03, 0.42), eu, Z, en)
    # a escada de marinheiro (atrás) até o teto
    a, eu, en, w = DEPOT.face(kb)
    q = a + eu * (w / 2 - 3.0)
    ub.ladder(p, 'Trim', q.x + 0.08 * en.x, q.y, FLOOR, ROOF_Z, tuple(en), w=0.5, cage=True, wall=0.16, top_ext=1.0)


def bay(p):
    """A baía do caminhão (+X): o vão fundo e escuro, as paredes e o teto do vão, as lâmpadas, a carga lá dentro, a porta de enrolar
    recolhida (o tambor e as últimas lâminas), os trilhos, os batentes de perigo, a placa azul e os holofotes."""
    a, eu, en, w = DEPOT.face(depot_face((1, 0, 0)))
    xf = a.x                                  # a face da frente (x = 1,6)
    hw, ht = BAY
    xb = xf - 2.4                             # o fundo do vão
    ub.obox(p, 'Concrete#pnl', ((xb + xf) / 2 - 0.1, 0.0, 0.0), (xf - xb + 0.2, 2 * hw + 0.3, 0.24), X, Y, Z)
    ub.obox(p, 'Dark#vp', (xb, 0.0, (FLOOR + ht) / 2), (0.1, 2 * hw + 0.1, ht - FLOOR), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Body#sm', ((xb + xf) / 2, s * (hw + 0.05), (FLOOR + ht) / 2), (xf - xb, 0.1, ht - FLOOR), X, Y, Z)
    ub.obox(p, 'Body#sm', ((xb + xf) / 2, 0.0, ht + 0.05), (xf - xb, 2 * hw + 0.1, 0.1), X, Y, Z)
    for k in range(3):
        ub.obox(p, 'Light#vp', (xb + 0.5 + k * 0.7, 0.0, ht - 0.02), (0.14, 1.6, 0.03), X, Y, Z)
    for (cx, cy, h) in ((xb + 0.75, -1.25, 1.0), (xb + 0.75, -1.25 + 0.0, 2.0), (xb + 0.75, 1.15, 1.0), (xb + 1.75, 1.3, 0.85)):
        ub.obox(p, 'Crate#pnl', (cx, cy, FLOOR + h - 0.48), (0.95, 0.95, 0.92), X, Y, Z)
    # a porta de enrolar recolhida
    vp.lathe(p, 'Trim#vp', (xf + 0.22, 0.0, ht + 0.22), 'y', [(0.0, -hw - 0.2), (0.27, -hw - 0.2), (0.27, hw + 0.2), (0.0, hw + 0.2)], seg=20)
    for s in (-1, 1):
        vp.lathe(p, 'Dark#vp', (xf + 0.22, s * (hw + 0.2), ht + 0.22), (0, s, 0), [(0.0, 0.0), (0.3, 0.0), (0.3, 0.05), (0.0, 0.05)], seg=20)
    ub.obox(p, 'Body#sm', (xf + 0.05, 0.0, ht - 0.12), (0.06, 2 * hw - 0.05, 0.3), X, Y, Z)
    for z in (ht - 0.2, ht - 0.07):
        ub.obox(p, 'Dark#vp', (xf + 0.085, 0.0, z), (0.01, 2 * hw - 0.1, 0.025), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', (xf + 0.08, s * (hw + 0.06), (FLOOR + ht) / 2), (0.16, 0.14, ht - FLOOR), X, Y, Z)
        ub.obox(p, 'Hazard#sm', (xf + 0.13, s * (hw + 0.3), FLOOR + 0.6), (0.06, 0.3, 1.0), X, Y, Z)
        vp.lathe(p, 'Rubber#vp', (xf + 0.12, s * (hw - 0.3), FLOOR), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.5), (0.09, 0.56), (0.0, 0.57)], seg=12)
    # a placa azul e os holofotes no parapeito, por cima do tambor
    ub.obox(p, 'Player#sm', (xf + 0.2, 0.0, WALL_Z + 0.12), (0.04, 2.4, 0.36), X, Y, Z)
    for s in (-1, 1):
        ub.lamp_head(p, (xf + 0.32, s * 1.75, WALL_Z + 0.12), (1.0, 0.0, -0.9), w=0.5, h=0.28)
        ub.obox(p, 'Dark#sm', (xf + 0.21, s * 1.75, WALL_Z + 0.12), (0.12, 0.12, 0.22), X, Y, Z)


# ---------------------------------------------------------------------------------- o deque
def deck(p):
    cx, cy = HELI
    z0 = DECK_TOP - DECK_T
    # a laje de chapa (o topo em grade) e a viga de borda por baixo
    poly_slab(p, 'Trim#pnl', DECK, 0.0, z0, DECK_TOP, cell=1.6)
    poly_band(p, 'Trim#sm', DECK, -0.05, -0.45, z0 - 0.32, z0 + 0.02)
    # as vigas radiais embaixo, as 8 colunas com a base e as mãos-francesas
    rc = 4.2
    for k in range(8):
        a = math.radians(22.5 + 45 * k)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        e = Vector((-d.y, d.x, 0.0))
        C = Vector((cx, cy, 0.0))
        ub.obox(p, 'Trim#sm', C + d * ((0.45 + rc + 0.6) / 2) + Vector((0, 0, z0 - 0.17)), (rc + 0.6 - 0.45, 0.24, 0.34), d, e, Z)
        q = C + d * rc
        vp.lathe(p, 'Body#vp', (q.x, q.y, ROOF_Z), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.12), (0.2, 0.18), (0.18, z0 - 0.3 - ROOF_Z),
                                                         (0.0, z0 - 0.3 - ROOF_Z)], seg=16)
        vp.bolt_ring(p, 'Dark', (q.x, q.y, ROOF_Z + 0.12), 'z', 0.25, 6, r=0.03, h=0.02)
        for s in (-1, 1):
            vp.tube(p, 'Trim', [q + Vector((0, 0, ROOF_Z + 0.35)) + e * (0.05 * s), q - d * 0.9 + e * (0.35 * s) + Vector((0, 0, z0 - 0.33))],
                    0.045, seg=8)
    vp.lathe(p, 'Body#vp', (cx, cy, ROOF_Z), 'z', [(0.0, 0.0), (0.7, 0.0), (0.7, 0.1), (0.6, 0.16), (0.6, z0 - 0.3 - ROOF_Z),
                                                   (0.0, z0 - 0.3 - ROOF_Z)], seg=24)
    # a rede de segurança inclinada em volta (menos a face da escada), a moldura de tubo e as mãos-francesas
    k_stair = [k for k in range(DECK.n) if DECK.nrm(k).y > 0.99][0]
    for k in range(DECK.n):
        if k == k_stair:
            continue
        a, eu, en, w = DECK.face(k, 0.0)
        mid = a + eu * (w / 2)
        tilt = math.radians(8)
        ev = en * math.cos(tilt) + Vector((0, 0, math.sin(tilt)))
        nn = eu.cross(ev).normalized()
        if nn.z < 0:
            nn = -nn
        c = mid + ev * 0.38 + Vector((0, 0, DECK_TOP - 0.16))
        ub.obox(p, 'Dark#vp', c, (w - 0.1, 0.62, 0.025), eu, ev, nn)
        o0 = a + en * 0.7 + Vector((0, 0, DECK_TOP - 0.16 + 0.7 * math.tan(tilt)))
        vp.tube(p, 'Trim', [o0 + eu * 0.05, o0 + eu * (w - 0.05)], 0.035, seg=8)
        for t in (0.15, 0.5, 0.85):
            q = a + eu * (w * t)
            vp.tube(p, 'Trim', [q + Vector((0, 0, DECK_TOP - 0.2)), q + en * 0.7 + Vector((0, 0, DECK_TOP - 0.16 + 0.7 * math.tan(tilt)))],
                    0.028, seg=6)
            vp.tube(p, 'Trim', [q + Vector((0, 0, z0 - 0.25)), q + en * 0.55 + Vector((0, 0, DECK_TOP - 0.2))], 0.028, seg=6)
    # as luzes de borda
    for k in range(16):
        a = 2 * math.pi * (k + 0.5) / 16
        r = 4.55 / math.cos(((a + math.pi / 8) % (math.pi / 4)) - math.pi / 8)
        ub.runway_light(p, cx + r * math.cos(a), cy + r * math.sin(a), DECK_TOP - 0.01, lens='Light', r=0.11)
    # a tinta (o topo em HELI_Z): o contorno azul, o círculo de toque amarelo, o "H" branco com as pernas ao longo de X
    zp0, zp1 = DECK_TOP - 0.004, HELI_Z
    poly_band(p, 'Paint#vp', DECK, -0.3, -0.5, zp0, zp1)              # a borda branca (o alvo r1_raid_slabs)
    for k in (1, 3, 5, 7):                                           # as marcas azuis nas 4 faces de quina (a cor do jogador)
        a, eu, en, w = DECK.face(k, -0.85)
        mid = a + eu * (w / 2)
        q = [mid + eu * su * 0.85 + en * sv * 0.16 for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        ub.flat_paint(p, 'Player', [(v.x, v.y) for v in q], zp0, zp1)
    ub.ring_paint(p, 'Yellow', HELI, 3.25, 3.6, zp0, zp1, seg=64)
    hp = [(-1.5, -0.95), (1.5, -0.95), (1.5, -0.55), (0.2, -0.55), (0.2, 0.55), (1.5, 0.55), (1.5, 0.95), (-1.5, 0.95), (-1.5, 0.55),
          (-0.2, 0.55), (-0.2, -0.55), (-1.5, -0.55)]
    ub.flat_paint(p, 'Paint', [(cx + u, cy + v) for u, v in hp], zp0, zp1)


def stair(p):
    """A escada do teto do depósito até o deque (ao longo de −X, do lado +Y) com o patamar, as longarinas e os corrimãos."""
    y0, y1 = STAIR_Y
    rise = DECK_TOP - ROOF_Z
    n = 9
    rz, run = rise / n, 0.27
    xb = -1.55                                     # o pé da escada (no teto)
    pts = [(0.0, 0.0)]
    for i in range(n):
        pts += [(i * run, (i + 1) * rz), ((i + 1) * run, (i + 1) * rz)]
    pts += [(n * run, n * rz - 0.32), (0.5, 0.0)]
    vp.prism(p, 'Body#vp', pts, Vector((xb, y0, ROOF_Z)), Vector((-1, 0, 0)), Vector(Z), Vector(Y), y1 - y0)
    xt = xb - n * run                              # o topo da escada
    ub.obox(p, 'Trim#sm', ((xt - 1.0 + xt) / 2, (y0 + y1) / 2, DECK_TOP - 0.06), (1.0, y1 - y0 + 0.1, 0.12), X, Y, Z)
    for x in (xt - 0.95, xt - 0.05):
        vp.lathe(p, 'Trim#vp', (x, (y0 + y1) / 2, ROOF_Z), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, DECK_TOP - 0.12 - ROOF_Z),
                                                                 (0.0, DECK_TOP - 0.12 - ROOF_Z)], seg=8)
    for y in (y0 - 0.04, y1 + 0.04):
        ub.obox(p, 'Trim#sm', ((xb + xt) / 2, y, ROOF_Z + rise / 2 + 0.05), (math.hypot(n * run, rise) + 0.1, 0.06, 0.24),
                Vector((-n * run, 0, rise)).normalized(), Vector(Y), Vector((-n * run, 0, rise)).normalized().cross(Vector(Y)) * -1)
    # os corrimãos: o da escada (inclinado, do lado de fora) e o do patamar
    yo = y1 + 0.04
    top = [Vector((xb + 0.1, yo, ROOF_Z + 1.0)), Vector((xt, yo, DECK_TOP + 1.0)), Vector((xt - 1.0, yo, DECK_TOP + 1.0)),
           Vector((xt - 1.0, (y0 + y1) / 2 - 0.5, DECK_TOP + 1.0))]
    vp.tube(p, 'Trim', vp.bent_bar(top, R=0.12, n=4), 0.028, seg=8)
    for q, z in ((Vector((xb + 0.1, yo, 0)), ROOF_Z), (Vector(((xb + xt) / 2, yo, 0)), ROOF_Z + rise / 2), (Vector((xt, yo, 0)), DECK_TOP),
                 (Vector((xt - 1.0, yo, 0)), DECK_TOP)):
        vp.tube(p, 'Trim', [q + Vector((0, 0, z)), q + Vector((0, 0, ROOF_Z + 1.0 + (z - ROOF_Z) if z < DECK_TOP else DECK_TOP + 1.0))],
                0.028, seg=8)
    vp.tube(p, 'Trim', [Vector((xb + 0.1, yo, ROOF_Z + 0.5)), Vector((xt, yo, DECK_TOP + 0.5))], 0.022, seg=8)


def roof_kit(p):
    # a caixa de ventiladores (o +Y de trás): a assinatura do teto da USA
    ub.fan_box(p, (-6.9, 7.05), (2.0, 1.3, 0.9), fans=2, z0=ROOF_Z)
    # o armário de rádio com as antenas e a sirene (−Y de trás)
    ax_, ay_ = -7.0, -7.0
    ub.obox(p, 'Body#pnl', (ax_, ay_, ROOF_Z + 0.58), (1.1, 1.1, 1.1), X, Y, Z)
    louvers(p, (ax_ + 0.55, ay_, ROOF_Z + 0.6), (1, 0, 0), (0, 1, 0), 0.7, 0.55, 6)
    ub.obox(p, 'Player#sm', (ax_, ay_ - 0.56, ROOF_Z + 0.85), (0.9, 0.03, 0.22), X, Y, Z)
    ub.antenna(p, ax_ + 0.3, ay_ - 0.3, ROOF_Z + 1.13, 3.4, r=0.05)
    ub.antenna(p, ax_ - 0.3, ay_ + 0.3, ROOF_Z + 1.13, 2.4, r=0.05)
    ub.beacon(p, (ax_ + 0.3, ay_ - 0.3, ROOF_Z + 1.13 + 3.44), r=0.08, h=0.16, cage=False)
    # o ar-condicionado (−Y da frente) e o conduíte até o parapeito
    ub.fan_box(p, (-0.9, -6.9), (1.5, 1.2, 0.8), fans=1, z0=ROOF_Z)
    ub.pipe(p, 'Trim', [(-1.65, -6.6, ROOF_Z + 0.2), (-3.2, -6.6, ROOF_Z + 0.2), (-3.2, -7.9, ROOF_Z + 0.2)], 0.05)
    # a biruta (o +Y da frente) no teto
    windsock_at(p, 0.55, 7.4, ROOF_Z, h=3.2, length=1.7, wind=(-0.35, -1.0))
    # as balizas nos cantos de trás do parapeito
    for (x, y) in ((-8.0, 7.95), (-8.0, -7.95)):
        ub.beacon(p, (x, y, WALL_Z + 0.42), r=0.08, h=0.17)


def windsock_at(p, x, y, z0, h=3.2, length=1.7, wind=(0.55, 0.75)):
    """A biruta do kit (ub.windsock) com o pé numa altura qualquer (o kit assenta no chão)."""
    ub.obox(p, 'Concrete#pnl', (x, y, z0 + 0.1), (0.6, 0.6, 0.24), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (x, y, z0 + 0.22), 'z', [(0.0, 0.0), (0.14, 0.0), (0.14, 0.03), (0.07, 0.06), (0.055, 0.2), (0.045, h - 0.22),
                                                   (0.0, h - 0.22)], seg=12)
    vp.lathe(p, 'Dark#vp', (x, y, z0 + h - 0.02), 'z', [(0.0, 0.0), (0.08, 0.0), (0.08, 0.1), (0.0, 0.12)], seg=12)
    w = Vector((wind[0], wind[1], 0.0)).normalized()
    r0, r1, seg = 0.28, 0.12, 5
    top = Vector((x, y, z0 + h + 0.04))
    base = top + w * 0.14
    vp.lathe(p, 'Trim#vp', base, (w.x, w.y, 0.0), [(r0 - 0.005, -0.025), (r0 + 0.03, -0.025), (r0 + 0.03, 0.025), (r0 - 0.005, 0.025)],
             seg=20, closed=True)
    vp.tube(p, 'Trim', [top, base], 0.02, seg=6)
    n = seg * 4

    def at(t):
        return base + w * (length * t) + Vector((0, 0, -0.4 * t ** 1.8))
    for i in range(seg):
        pts = [at((i * 4 + j) / n) for j in range(5)]
        vp.tube(p, 'Yellow' if i % 2 == 0 else 'Paint', pts, r0, seg=16, caps=(i == seg - 1),
                r_fn=lambda j, i=i: r0 + (r1 - r0) * ((i * 4 + j) / n))


# ---------------------------------------------------------------------------------- o pátio da baía e a carga
def crate(p, c, size=(1.15, 1.15, 0.95), yaw=0.0):
    """O caixote militar no palete: o corpo de quinas largas, as 2 cintas de aço, o palete de tábuas."""
    a = math.radians(yaw)
    ex, ey = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
    C = Vector(c)
    sx, sy, sz = size
    ub.obox(p, 'Crate#sm', C + Vector((0, 0, 0.07)), (sx + 0.06, sy + 0.06, 0.14), ex, ey, Z)
    ub.obox(p, 'Crate#pnl', C + Vector((0, 0, 0.14 + sz / 2)), (sx, sy, sz), ex, ey, Z)
    for t in (-0.3, 0.3):
        ub.obox(p, 'Trim#sm', C + ex * (t * sx) + Vector((0, 0, 0.14 + sz / 2)), (0.07, sy + 0.03, sz + 0.03), ex, ey, Z)
    return 0.14 + sz


def yard(p):
    a, eu, en, w = DEPOT.face(depot_face((1, 0, 0)))
    xf = a.x
    # o pátio da baía: as lajes com a junta, a vaga do caminhão em amarelo, a faixa de perigo na soleira
    slab_tiles(p, xf + 0.15, 8.95, -4.2, 4.2, 3, 3)
    zp0, zp1 = FLOOR - 0.004, FLOOR + 0.012
    for s in (-1, 1):
        ub.flat_paint(p, 'Yellow', [(xf + 0.9, s * 2.05 - 0.07), (8.6, s * 2.05 - 0.07), (8.6, s * 2.05 + 0.07), (xf + 0.9, s * 2.05 + 0.07)], zp0, zp1)
    ub.flat_paint(p, 'Hazard', [(xf + 0.2, -BAY[0]), (xf + 0.75, -BAY[0]), (xf + 0.75, BAY[0]), (xf + 0.2, BAY[0])], zp0, zp1)
    for k in range(3):                                    # as setas de saída
        x = 4.6 + k * 1.3
        ub.flat_paint(p, 'Paint', [(x, -0.12), (x + 0.5, -0.12), (x + 0.5, -0.38), (x + 0.95, 0.0), (x + 0.5, 0.38), (x + 0.5, 0.12),
                                   (x, 0.12)], zp0, zp1)
    # a carga recebida: as pilhas com a lona azul amarrada (−Y) e os caixotes soltos (+Y)
    for (cx, cy) in ((4.15, -6.0), (5.45, -6.0), (4.15, -7.3), (5.45, -7.3)):
        crate(p, (cx, cy, 0.0))
    ub.obox(p, 'Crate#pnl', (4.8, -6.65, 1.09 + 0.48), (2.3, 2.35, 0.9), X, Y, Z)
    ub.tarp_cover_at(p, (4.8, -6.65), 2.75, 2.8, 0.0, 1.09 + 0.06, 0.98, m='Player', seed=0.7, ties=3, box=14)
    for (cx, cy, yaw) in ((7.3, -6.3, 12.0), (7.35, -7.75, -6.0)):
        crate(p, (cx, cy, 0.0), yaw=yaw)
    for (cx, cy) in ((4.4, 6.2), (5.7, 6.2), (4.4, 7.5)):
        crate(p, (cx, cy, 0.0))
    ub.obox(p, 'Crate#pnl', (5.05, 6.2, 1.09 + 0.48), (2.3, 1.15, 0.9), X, Y, Z)
    ub.tarp_cover_at(p, (5.05, 6.2), 2.7, 1.55, 0.0, 1.09 + 0.06, 0.98, m='Player', seed=2.1, ties=3, box=14)
    crate(p, (7.2, 6.9, 0.0), yaw=-15.0)
    # os postes de luz com 2 holofotes virados pra baía
    for s in (-1, 1):
        ub.lamp_post(p, 8.55, s * 8.7, h=6.0, aim=(4.0, 0.0), heads=2)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    depot(p)
    bay(p)
    deck(p)
    stair(p)
    roof_kit(p)
    yard(p)
    return ub.finish(p)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_supply_center_v2')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    pts = [tuple(DEPOT.corner(k, 0.06))[:2] for k in range(DEPOT.n)]
    sc = scaffold_poly(PREFIX + '_Scaffold', pts, WALL_Z + 0.1, out=0.95)
    check([b, sc], HX, HY)
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, sc)})
    total = export(b, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL helipad_m', [HELI[0], HELI[1], HELI_Z], '(o topo da tinta do H)')
    print('MODEL fire_points_m', [[-6.6, 6.4, ROOF_Z], [-1.2, -6.4, ROOF_Z], [-7.4, -4.6, ROOF_Z]])
    views = [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 800]],
             ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
             ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]]
    game = [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]]
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]]], views, (2.0, 13.0, 0), game, scaffold=PREFIX + '_Scaffold')
    # (PREVIEW_SPEC=preview_chinook.json) o Chinook B pousado no "H" (a origem dele no chão, no meio da fuselagem)
    import json
    ch = 'captures/game/models/usa_chinook_v2/SM_USA_Chinook_'
    hz = HELI_Z
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [ch + 'Body', [HELI[0], -HELI[1], hz]],
                      [ch + 'Ramp', [HELI[0] - 2.668, -HELI[1], hz + 0.435]], [ch + 'RotorFront', [HELI[0] + 3.463, -HELI[1], hz + 2.697]],
                      [ch + 'RotorRear', [HELI[0] - 3.463, -HELI[1], hz + 3.277]]],
            'views': [['chinook_three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95]], 'usa_at': [2.0, 13.0, 0],
            'game': [['chinook_game_cam', 37.5, 200.0, 50.0, 0.85]]}
    with open(os.path.join(out, 'preview_chinook.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


if __name__ == '__main__':
    main()
