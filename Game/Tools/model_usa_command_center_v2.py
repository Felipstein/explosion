"""O Command Center da USA (usa_command_center), VERSÃO B no detalhe AAA (01/11/2026): a de antes era a caixa do script antigo
(Game/Tools/blender_provisional.py command_center(), a do jogo hoje); esta é a refeita no nível do Strategy Center B.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_command_center_v2.py C:/Users/luisf/explosion/captures/game/models/usa_command_center_v2

A PALAVRA (docs/CONCEITO_CONSTRUCOES.md §3b): O RADAR. A antena de vigilância grande girando em cima do bloco blindado; o Dozer sai
da garagem. A antena é a de radar de aproximação (a "casca de laranja": o refletor largo de 12 m, curvo nos dois sentidos, feito de 8
chapas com a junta, com a BARRA do IFF em cima, a treliça atrás e a corneta embaixo), não o prato redondo (o prato é a palavra do
Strategy Center): girando, ela varre um círculo de ~12,6 m sobre o pedestal do BLOCO DE COMANDO (o 2º degrau da armadura, com a fenda
de vidro escuro), que sobe do meio do bloco baixo. A massa em degraus: o bloco baixo largo, o bloco de comando, o pedestal, o radar
(um objeto só, a armadura subindo até a antena). De cima: o círculo grande no meio do retângulo. É o maior e o mais alto da base (a
hierarquia: ~15 m no topo da barra).
  - o bloco blindado (17 × 25 m, as quinas cortadas): a saia inclinada de CHAPAS com os parafusos em volta toda (aberta só na
    garagem), as paredes de PLACAS com a junta funda e a fileira azul, os pilares redondos nas quinas, o parapeito com a capa, as
    luminárias com o corpo, as balizas;
  - a GARAGEM do Dozer (+X): o pórtico saliente de pilares de placas com a base de perigo, a verga com a placa azul, os holofotes e a
    capa; a porta de enrolar recolhida (as últimas lâminas, os trilhos); o vão FUNDO e escuro (4,6 m) com as lâmpadas do teto, as
    nervuras e as faixas de perigo no piso; o pátio de saída em lajes com as faixas e as setas; os frades;
  - o bloco de comando: a saia de chapas, as paredes de placas com a FENDA de vidro escuro e os montantes (a sala de operações), a
    fileira azul, os pilares, o parapeito, a eclusa da frente com a porta, a marquise e o holofote, o pedestal torneado do radar com o
    anel azul, 2 caixas de ventiladores debaixo do giro, a escotilha, a escada;
  - o teto baixo: as juntas das lajes, a caixa de ventiladores (a assinatura do teto da USA), o armário de rádio com as antenas e a
    sirene, 2 cargas com a LONA azul amarrada (a cor do jogador, o objeto do tanque), a escada de marinheiro atrás;
  - o chão: o mastro da bandeira torneado (o pano no vento é do jogo: flags.json), os 2 postes de luz virados pra saída, o tanque de
    combustível deitado no berço;
  - o radar (malha à parte, origem no eixo): a mesa giratória com os parafusos, a casa do giro de placas com a faixa azul e a
    veneziana, o contrapeso, a coluna, a casca de laranja branca com o aro, as 7 costelas, os 3 banzos e as diagonais da treliça, a
    corneta escura no braço, a guia de onda, a barra do IFF com os 16 dipolos e a faixa azul, a sirene.
A USA limpa (13/10): sem desgaste pintado no modelo (o desgaste vem do material do jogo, pelas juntas e chapas daqui).

Lote 25,28 × 29,52 m (x −12,64..12,64, y −14,76..14,76), a frente em +X; o Dozer sai pela garagem em +X (y = 0); nada sai do lote.
Pivô NOVO do radar: spin at_m = RADAR_AT (−3,4; 0,0; 9,2) (o eixo no topo do pedestal do bloco de comando), rpm 6 (o de hoje).
Peças: SM_USA_CommandCenter_Base, SM_USA_CommandCenter_Radar (origem no eixo), SM_USA_CommandCenter_Scaffold.
Slots (os do Strategy Center B): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime).
O kit (CPart, Plan, poly_slab, plan_walls, ...) vem do model_usa_power_plant_v2.py.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import (CPart, chamfer_rect, poly_slab, poly_band, plan_core, plan_walls, plan_pillars,  # noqa: E402
                                      slab_tiles, louvers, scaffold_poly, check, preview_spec, X, Y, Z)

PREFIX = 'SM_USA_CommandCenter'

HX, HY = 12.64, 14.76             # o lote 25,28 × 29,52
FLOOR = 0.12
BLOCK = chamfer_rect((-2.375, 0.0), 8.575, 8.575, 12.6, 12.6, (2.2, 2.2, 2.2, 2.2))   # x −10,95..6,2, y ±12,6
XF = 6.2                          # a face da frente
SKIRT = (0.6, 1.6)                # a saia: o avanço no pé e a altura
WALL_Z = 5.6
ROOF_Z = 5.65
GATE = (3.3, 4.72)                # a garagem: a meia largura do vão e a altura
PIL = (3.4, 4.95, 7.75)           # os pilares do pórtico: y de dentro, y de fora, x da frente
GX0 = 1.6                         # o fundo da garagem
UP = chamfer_rect((-3.4, 0.0), 5.2, 5.2, 6.6, 6.6, (1.6, 1.6, 1.6, 1.6))    # o bloco de comando: x −8,6..1,8, y ±6,6
UP_SKIRT = (0.45, 6.45)           # a saia do bloco de comando (o avanço no pé, o topo)
UP_WALL = 8.3
UP_ROOF = 8.35
SLIT = (7.0, 7.75)                # a fenda de vidro escuro (a sala de operações)
RADAR_AT = (-3.4, 0.0, 9.2)       # o eixo do radar (spin at_m), no topo do pedestal

# o radar (no referencial dele: origem no eixo, x pra onde ele olha)
RW, RH = 12.0, 3.8                # a casca de laranja: a largura e a altura
FH, FV = 9.0, 2.2                 # as distâncias focais (de lado e de pé)
TILT = math.radians(12)           # olha 12° pra cima
R0 = Vector((0.8, 0.0, 3.0))      # o vértice da casca
THK = 0.1
NSTRIP = 8                        # as chapas verticais da casca (a junta mostra as costelas escuras atrás)


def face_of(normal, plan=None):
    plan = plan or BLOCK
    return [k for k in range(plan.n) if plan.nrm(k).dot(Vector(normal)) > 0.99][0]


# ---------------------------------------------------------------------------------- o bloco
def skirt(p, plan, dl0, z0, z1, cuts=None, plate_w=2.4):
    """A saia inclinada em volta toda (o miolo em quadriláteros por face; cuts[k] = (sa, sb): o vão na coordenada ao longo da face,
    p.ex. o y na face da frente) com as chapas de blindagem e os parafusos."""
    b = p.bm('Body#pnl', ub._bev('Body#pnl'))
    cuts = cuts or {}
    for k in range(plan.n):
        a0, eu, en, w0 = plan.face(k, dl0)
        a1, _, _, w1 = plan.face(k, 0.0)
        s0a, s0b = a0.dot(eu), a0.dot(eu) + w0
        s1a, s1b = a1.dot(eu), a1.dot(eu) + w1
        segs = [((s0a, s0b), (s1a, s1b))]
        if k in cuts:
            ga, gb = cuts[k]
            segs = [((s0a, ga), (s1a, ga)), ((gb, s0b), (gb, s1b))]
        for (pa, pb), (qa, qb) in segs:
            pts = [a0 + eu * (pa - s0a), a0 + eu * (pb - s0a), a1 + eu * (qb - s1a), a1 + eu * (qa - s1a)]
            zs = (z0, z0, z1, z1)
            b.faces.new([b.verts.new(V(q.x, q.y, zz)) for q, zz in zip(pts, zs)])
        # as chapas no plano inclinado (uma a cada ~plate_w), menos onde o vão corta
        mid0 = a0 + eu * (w0 / 2) + Vector((0, 0, z0))
        mid1 = a1 + eu * (w1 / 2) + Vector((0, 0, z1))
        ev = (mid1 - mid0).normalized()
        nn = eu.cross(ev).normalized()
        if nn.dot(en) < 0:
            nn = -nn
        L = (mid1 - mid0).length
        mid = (mid0 + mid1) / 2
        w = min(w0, w1) - 0.3
        n = max(1, int(round(w / plate_w)))
        pw = w / n
        for i in range(n):
            sl = -w / 2 + pw * (i + 0.5)
            sc = mid.dot(eu) + sl
            if k in cuts and cuts[k][0] - pw / 2 < sc < cuts[k][1] + pw / 2:
                continue
            q = mid + eu * sl + nn * 0.02
            ub.obox(p, 'Body#pnl', q, (pw - 0.08, L - 0.16, 0.12), eu, ev, nn)
            for t in (-0.32, 0.0, 0.32):
                vp.nut(p, 'Dark', q + eu * (t * pw) + ev * (L / 2 - 0.2) + nn * 0.08, nn, r=0.035, h=0.025)


def block(p):
    poly_band(p, 'Concrete#pnl', BLOCK, SKIRT[0] + 0.32, SKIRT[0] - 0.12, -0.12, FLOOR)
    skirt(p, BLOCK, SKIRT[0], FLOOR - 0.02, SKIRT[1], cuts={face_of((1, 0, 0)): (-PIL[1], PIL[1])})
    kf = face_of((1, 0, 0))
    a, eu, en, w = BLOCK.face(kf)
    u_gate = (-GATE[0] - a.y, GATE[0] - a.y)
    plan_core(p, BLOCK, 0.0, SKIRT[1] - 0.25, WALL_Z, holes={kf: [(u_gate[0], u_gate[1], -0.3, GATE[1] - SKIRT[1] + 0.25)]})
    # os miolos ao lado do vão, do chão até a saia (atrás dos pilares do pórtico)
    for s in (-1, 1):
        yc = s * (GATE[0] + PIL[1]) / 2
        ub.obox(p, 'Body#pnl', (XF - 0.15, yc, (FLOOR + SKIRT[1]) / 2), (0.3, PIL[1] - GATE[0] + 0.1, SKIRT[1] - FLOOR + 0.1), X, Y, Z)
    m = 0.3
    plan_walls(p, BLOCK, 0.0, SKIRT[1], WALL_Z - 0.05, rows=(1.2, 2.55, 3.0), blue=2, pw=2.4,
               holes={kf: [(-PIL[1] - 0.05 - a.y - m, PIL[1] + 0.05 - a.y - m, -0.3, 5.0)]})
    plan_pillars(p, BLOCK, 0.08, SKIRT[1] - 0.3, WALL_Z + 0.25, r=0.34)
    poly_band(p, 'Body#pnl', BLOCK, 0.12, -0.26, WALL_Z - 0.12, WALL_Z + 0.4)
    poly_band(p, 'Trim#sm', BLOCK, 0.18, -0.32, WALL_Z + 0.4, WALL_Z + 0.48)
    up_in = [(UP.nrm(k), UP.f[k][1] - 0.5) for k in range(UP.n)]          # a laje debaixo do bloco de comando não aparece
    poly_slab(p, 'Concrete#pnl', BLOCK, -0.24, WALL_Z - 0.2, ROOF_Z, cell=1.6,
              hide=lambda q: all((q - UP.c).dot(n) < d for n, d in up_in))
    # as luminárias com o corpo nos lados e atrás
    for nrm, us in (((0, 1, 0), (0.3, 0.7)), ((0, -1, 0), (0.3, 0.7)), ((-1, 0, 0), (0.25, 0.75))):
        k = face_of(nrm)
        a, eu, en, w = BLOCK.face(k)
        for u in us:
            q = a + eu * (w * u) + Vector((0, 0, 3.6))
            ub.obox(p, 'Dark#sm', q + en * 0.17, (0.6, 0.18, 0.22), eu, en, Z)
            ub.obox(p, 'Light#vp', q + en * 0.27 - Vector((0, 0, 0.05)), (0.5, 0.012, 0.1), eu, en, Z)
            ub.obox(p, 'Dark#sm', q + en * 0.28 + Vector((0, 0, 0.13)), (0.64, 0.2, 0.025), eu, en, Z)
    # a escada de marinheiro atrás, até o teto
    k = face_of((-1, 0, 0))
    a, eu, en, w = BLOCK.face(k)
    q = a + eu * (w * 0.62)
    ub.ladder(p, 'Trim', q.x - 0.08, q.y, FLOOR, ROOF_Z, tuple(en), w=0.5, cage=True, wall=0.62, top_ext=1.0)
    # as balizas nos 4 cantos do parapeito
    for k in range(BLOCK.n):
        if abs(BLOCK.nrm(k).x) > 0.5 and abs(BLOCK.nrm(k).y) > 0.5:
            a, eu, en, w = BLOCK.face(k, -0.05)
            q = a + eu * (w / 2)
            ub.beacon(p, (q.x, q.y, WALL_Z + 0.48), r=0.09, h=0.18)


def garage(p):
    """A garagem do Dozer (+X): o pórtico, a porta recolhida, o vão fundo e escuro."""
    gw, gh = GATE
    yi, yo, xp = PIL
    # os pilares do pórtico (placas, a base de perigo, a capa) e a verga
    for s in (-1, 1):
        yc = s * (yi + yo) / 2
        ub.obox(p, 'Body#pnl', ((XF - 0.2 + xp) / 2, yc, (FLOOR + 6.3) / 2), (xp - XF + 0.2, yo - yi, 6.3 - FLOOR), X, Y, Z)
        ub.panel_wall(p, (xp, yc - s * (yo - yi) / 2, FLOOR + 1.3), (0, s, 0), Z, X, yo - yi, 4.9, pw=yo - yi, ph=1.25, m='Body#pnl',
                      gap=0.07, proud=0.07, embed=0.03)
        ub.obox(p, 'Hazard#sm', (xp + 0.02, yc, FLOOR + 0.62), (0.08, yo - yi + 0.04, 1.16), X, Y, Z)
        ub.obox(p, 'Hazard#sm', ((XF + xp) / 2 + 0.1, s * (yo + 0.02), FLOOR + 0.62), (xp - XF - 0.2, 0.06, 1.16), X, Y, Z)
        ub.obox(p, 'Trim#sm', ((XF + xp) / 2, yc, 6.36), (xp - XF + 0.5, yo - yi + 0.3, 0.14), X, Y, Z)
        ub.beacon(p, ((XF + xp) / 2 + 0.2, yc, 6.43), r=0.09, h=0.18)
    ub.obox(p, 'Body#pnl', ((XF - 0.2 + xp) / 2, 0.0, (gh + 6.2) / 2), (xp - XF + 0.2, 2 * yi + 0.1, 6.2 - gh), X, Y, Z)
    ub.obox(p, 'Trim#sm', ((XF + xp) / 2 + 0.05, 0.0, 6.27), (xp - XF + 0.3, 2 * yo + 0.1, 0.14), X, Y, Z)
    ub.obox(p, 'Player#sm', (xp + 0.04, 0.0, gh + 0.85), (0.05, 3.2, 0.62), X, Y, Z)
    for y in (-2.4, 2.4):
        ub.lamp_head(p, (xp + 0.22, y, gh + 0.6), (1.0, 0.0, -0.9), w=0.6, h=0.32)
        ub.obox(p, 'Dark#sm', (xp + 0.08, y, gh + 0.6), (0.14, 0.12, 0.22), X, Y, Z)
    # a porta de enrolar recolhida: as últimas lâminas, os trilhos
    ub.obox(p, 'Body#sm', (xp - 0.5, 0.0, gh - 0.16), (0.08, 2 * gw + 0.1, 0.36), X, Y, Z)
    for z in (gh - 0.27, gh - 0.12):
        ub.obox(p, 'Dark#vp', (xp - 0.455, 0.0, z), (0.012, 2 * gw, 0.03), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', (xp - 0.5, s * (gw + 0.06), (FLOOR + gh) / 2), (0.16, 0.14, gh - FLOOR), X, Y, Z)
    # o vão fundo: o piso, o fundo escuro, as paredes com as nervuras, o teto com as lâmpadas, as faixas de perigo
    ub.obox(p, 'Concrete#pnl', ((GX0 + xp) / 2, 0.0, -0.002), (xp - GX0 + 0.2, 2 * gw + 0.4, 0.236), X, Y, Z)
    ub.obox(p, 'Dark#vp', (GX0, 0.0, (FLOOR + gh) / 2), (0.1, 2 * gw + 0.1, gh - FLOOR), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Body#sm', ((GX0 + XF) / 2, s * (gw + 0.06), (FLOOR + gh) / 2), (XF - GX0, 0.12, gh - FLOOR), X, Y, Z)
        for k in range(4):
            ub.obox(p, 'Trim#sm', (GX0 + 0.6 + k * 1.15, s * (gw - 0.02), (FLOOR + gh) / 2), (0.14, 0.08, gh - FLOOR - 0.1), X, Y, Z)
        ub.flat_paint(p, 'Hazard', [(GX0 + 0.1, s * (gw - 0.45)), (xp - 0.6, s * (gw - 0.45)), (xp - 0.6, s * (gw - 0.15)),
                                    (GX0 + 0.1, s * (gw - 0.15))][::s], FLOOR - 0.004, FLOOR + 0.012)
    ub.obox(p, 'Body#sm', ((GX0 + XF) / 2, 0.0, gh + 0.06), (XF - GX0, 2 * gw + 0.12, 0.12), X, Y, Z)
    for k in range(3):
        ub.obox(p, 'Light#vp', (GX0 + 0.9 + k * 1.4, 0.0, gh - 0.015), (0.16, 2.6, 0.03), X, Y, Z)
    # as bancadas e o armário de ferramentas do fundo
    ub.obox(p, 'Trim#sm', (GX0 + 0.35, -1.6, FLOOR + 0.5), (0.5, 1.6, 1.0), X, Y, Z)
    ub.obox(p, 'Body#sm', (GX0 + 0.3, 1.5, FLOOR + 0.9), (0.45, 1.2, 1.8), X, Y, Z)
    ub.obox(p, 'Player#sm', (GX0 + 0.54, 1.5, FLOOR + 1.5), (0.03, 1.0, 0.18), X, Y, Z)


def apron(p):
    """O pátio de saída do Dozer: as lajes com a junta, as faixas amarelas, a faixa de perigo na soleira, as setas, os frades."""
    x0 = PIL[2] + 0.05
    slab_tiles(p, x0, 12.5, -4.9, 4.9, 2, 4)
    zp0, zp1 = FLOOR - 0.004, FLOOR + 0.012
    for s in (-1, 1):
        ub.flat_paint(p, 'Yellow', [(x0 + 0.3, s * 3.15 - 0.07), (12.2, s * 3.15 - 0.07), (12.2, s * 3.15 + 0.07), (x0 + 0.3, s * 3.15 + 0.07)],
                      zp0, zp1)
    ub.flat_paint(p, 'Hazard', [(x0 + 0.05, -GATE[0]), (x0 + 0.6, -GATE[0]), (x0 + 0.6, GATE[0]), (x0 + 0.05, GATE[0])], zp0, zp1)
    for k in range(2):
        x = 9.4 + k * 1.5
        ub.flat_paint(p, 'Paint', [(x, -0.15), (x + 0.6, -0.15), (x + 0.6, -0.45), (x + 1.15, 0.0), (x + 0.6, 0.45), (x + 0.6, 0.15),
                                   (x, 0.15)], zp0, zp1)
    for y in (-4.55, 4.55):
        vp.lathe(p, 'Hazard#vp', (x0 + 0.5, y, FLOOR), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.95), (0.13, 1.02), (0.0, 1.04)], seg=14)


# ---------------------------------------------------------------------------------- o bloco de comando e o teto
def upper(p):
    """O bloco de comando (o 2º degrau da armadura): a saia de chapas, as paredes de placas com a FENDA de vidro escuro e os montantes
    (a sala de operações), a fileira azul, os pilares, o parapeito, a eclusa da frente com a porta, o pedestal do radar."""
    kf = face_of((1, 0, 0), UP)
    skirt(p, UP, UP_SKIRT[0], ROOF_Z - 0.06, UP_SKIRT[1], cuts={kf: (-1.15, 1.15)}, plate_w=2.0)
    plan_core(p, UP, 0.0, UP_SKIRT[1] - 0.2, UP_WALL)
    for k in range(UP.n):                                  # a fenda: o vidro recuado e os montantes
        a, eu, en, w = UP.face(k)
        if w < 1.5:
            continue
        mid = a + eu * (w / 2)
        ww = w - 0.7
        if k == kf:
            for sgn in (-1, 1):
                wl = (ww - 2.6) / 2
                q = mid + eu * (sgn * (1.3 + wl / 2))
                ub.obox(p, 'Glass#vp', q + en * 0.01 + Vector((0, 0, sum(SLIT) / 2)), (wl, SLIT[1] - SLIT[0], 0.04), eu, Z, en)
        else:
            ub.obox(p, 'Glass#vp', mid + en * 0.01 + Vector((0, 0, sum(SLIT) / 2)), (ww, SLIT[1] - SLIT[0], 0.04), eu, Z, en)
        n = max(2, int(round(ww / 1.2)))
        for i in range(n + 1):
            u = -ww / 2 + ww * i / n
            if k == kf and abs(u) < 1.4:
                continue
            ub.obox(p, 'Trim#sm', mid + eu * u + en * 0.05 + Vector((0, 0, sum(SLIT) / 2)), (0.12, SLIT[1] - SLIT[0] + 0.04, 0.1), eu, Z, en)
    m = 0.3
    holes = {}
    for k in range(UP.n):
        a, eu, en, w = UP.face(k)
        sv = (SLIT[0] - UP_SKIRT[1] - 0.02, SLIT[1] - UP_SKIRT[1] + 0.02)
        holes[k] = [(-1.0, w + 1.0, sv[0], sv[1])]
        if k == kf:
            holes[k].append((w / 2 - m - 1.25, w / 2 - m + 1.25, -1.0, 2.0))
    plan_walls(p, UP, 0.0, UP_SKIRT[1], UP_WALL - 0.05, rows=(SLIT[0] - UP_SKIRT[1], SLIT[1] - UP_SKIRT[1] + 0.04), blue=2, pw=2.0,
               holes=holes)
    plan_pillars(p, UP, 0.08, UP_SKIRT[1] - 0.25, UP_WALL + 0.2, r=0.28)
    poly_band(p, 'Body#pnl', UP, 0.12, -0.24, UP_WALL - 0.12, UP_WALL + 0.36)
    poly_band(p, 'Trim#sm', UP, 0.18, -0.3, UP_WALL + 0.36, UP_WALL + 0.44)
    poly_slab(p, 'Concrete#pnl', UP, -0.22, UP_WALL - 0.2, UP_ROOF, cell=1.6)
    # a eclusa da frente: o bloco saliente, a porta recuada com as nervuras, a marquise, o holofote, a placa azul, os degraus
    a, eu, en, w = UP.face(kf)
    xf = a.x
    ub.obox(p, 'Body#pnl', (xf + 0.25, 0.0, (ROOF_Z + 8.1) / 2), (0.7, 2.3, 8.1 - ROOF_Z), X, Y, Z)
    ub.obox(p, 'Trim#sm', (xf + 0.3, 0.0, 8.16), (0.9, 2.5, 0.12), X, Y, Z)
    ub.obox(p, 'Dark#vp', (xf + 0.56, 0.0, ROOF_Z + 1.08), (0.06, 1.16, 2.16), X, Y, Z)
    ub.obox(p, 'Trim#sm', (xf + 0.6, 0.0, ROOF_Z + 1.06), (0.06, 1.02, 2.04), X, Y, Z)
    for k in range(3):
        ub.obox(p, 'Dark#sm', (xf + 0.635, 0.0, ROOF_Z + 0.5 + k * 0.55), (0.04, 0.88, 0.09), X, Y, Z)
    vp.grab_handle(p, 'Body', (xf + 0.63, 0.34, ROOF_Z + 0.9), (xf + 0.63, 0.34, ROOF_Z + 1.35), (1, 0, 0), height=0.06, r=0.02)
    ub.obox(p, 'Trim#sm', (xf + 1.0, 0.0, ROOF_Z + 2.38), (0.8, 1.9, 0.08), X, Y, Z)
    ub.lamp_head(p, (xf + 0.75, 0.0, ROOF_Z + 2.24), (1.0, 0.0, -1.2), w=0.4, h=0.2)
    ub.obox(p, 'Player#sm', (xf + 0.62, 0.0, ROOF_Z + 2.0 + 0.55), (0.04, 1.4, 0.28), X, Y, Z)
    ub.obox(p, 'Hazard#sm', (xf + 0.95, 0.0, ROOF_Z + 0.012), (0.6, 1.3, 0.03), X, Y, Z)
    # o pedestal do radar: o tronco torneado, o anel azul, os parafusos
    c = (UP.c.x, UP.c.y, UP_ROOF - 0.05)
    h = RADAR_AT[2] - c[2]
    vp.lathe(p, 'Body#vp', c, 'z', [(0.0, 0.0), (2.1, 0.0), (2.1, 0.22), (1.9, 0.32), (1.55, 0.4), (1.45, h - 0.2), (1.55, h - 0.12),
                                    (1.55, h), (0.0, h)], seg=48)
    vp.lathe(p, 'Player#vp', c, 'z', [(1.49, 0.48), (1.53, 0.48), (1.53, 0.66), (1.49, 0.66)], seg=48, closed=True)
    vp.bolt_ring(p, 'Dark', (c[0], c[1], c[2] + 0.22), 'z', 1.95, 24, r=0.03, h=0.02)
    # o teto do bloco de comando: 2 caixas de ventiladores (debaixo do giro), a escotilha
    ub.fan_box(p, (-7.0, 4.3), (2.2, 1.4, 0.95), fans=2, z0=UP_ROOF)
    ub.fan_box(p, (-7.0, -4.3), (2.2, 1.4, 0.95), fans=2, z0=UP_ROOF)
    hx_, hy_ = 0.6, 4.9
    ub.obox(p, 'Body#pnl', (hx_, hy_, UP_ROOF + 0.2), (0.9, 0.9, 0.4), X, Y, Z)
    ub.obox(p, 'Body#sm', (hx_ + 0.03, hy_, UP_ROOF + 0.43), (0.8, 0.82, 0.06), X, Y, Z)
    vp.hinge(p, 'Trim', (hx_ - 0.4, hy_, UP_ROOF + 0.44), 'y', 0.6, r=0.025, knuckles=3)
    # a escada de marinheiro do bloco de comando (atrás)
    kb = face_of((-1, 0, 0), UP)
    a, eu, en, w = UP.face(kb)
    q = a + eu * (w * 0.7)
    ub.ladder(p, 'Trim', q.x - 0.08, q.y, ROOF_Z, UP_ROOF, tuple(en), w=0.5, cage=False, wall=0.5, top_ext=1.0)


def roof_joints(p):
    """As juntas das lajes do teto baixo (a cada 3,2 m; o plano grande e liso lia como uma placa só)."""
    z = ROOF_Z + 0.007
    ux0, ux1 = UP.c.x - 5.2 - 0.6, UP.c.x + 5.2 + 0.6
    uy = 6.6 + 0.6
    X0, X1, Y0, Y1 = -10.55, 5.8, -12.2, 12.2
    for x in (-9.4, -6.2, -3.0, 0.2, 3.4):
        segs = [(Y0, -uy), (uy, Y1)] if ux0 < x < ux1 else [(Y0, Y1)]
        for y0, y1 in segs:
            ub.obox(p, 'Dark#vp', (x, (y0 + y1) / 2, z), (0.07, y1 - y0, 0.012), X, Y, Z)
    for y in (-9.6, -6.4 - 0.6, 6.4 + 0.6, 9.6, -3.2, 0.0, 3.2):
        segs = [(X0, ux0), (ux1, X1)] if abs(y) < uy else [(X0, X1)]
        for x0, x1 in segs:
            ub.obox(p, 'Dark#vp', ((x0 + x1) / 2, y, z), (x1 - x0, 0.07, 0.012), X, Y, Z)


def roof(p):
    roof_joints(p)
    # a caixa de ventiladores de trás e o armário de rádio com as antenas e a sirene (frente-esquerda, longe do giro)
    ub.fan_box(p, (-9.2, 9.4), (2.6, 1.6, 1.0), fans=2, z0=ROOF_Z)
    ax_, ay_ = 3.6, -9.6
    ub.obox(p, 'Body#pnl', (ax_, ay_, ROOF_Z + 0.62), (1.3, 1.2, 1.2), X, Y, Z)
    louvers(p, (ax_ + 0.65, ay_, ROOF_Z + 0.65), (1, 0, 0), (0, 1, 0), 0.8, 0.6, 6)
    ub.obox(p, 'Player#sm', (ax_, ay_ - 0.61, ROOF_Z + 0.9), (1.0, 0.03, 0.24), X, Y, Z)
    ub.antenna(p, ax_ + 0.35, ay_ - 0.3, ROOF_Z + 1.22, 4.2, r=0.06)
    ub.antenna(p, ax_ - 0.35, ay_ + 0.3, ROOF_Z + 1.22, 3.0, r=0.05)
    ub.beacon(p, (ax_ + 0.35, ay_ - 0.3, ROOF_Z + 1.22 + 4.24), r=0.08, h=0.16, cage=False)
    ub.pipe(p, 'Trim', [(ax_ - 0.66, ay_ + 0.3, ROOF_Z + 0.2), (1.2, ay_ + 0.3, ROOF_Z + 0.2), (1.2, -7.05, ROOF_Z + 0.2)], 0.06)
    # as 2 cargas com a lona azul amarrada (a cor do jogador)
    for (cx, cy, L, W, seed) in ((3.4, 9.2, 2.8, 1.7, 1.1), (-9.0, -9.4, 2.6, 1.6, 2.4)):
        ub.obox(p, 'Body#sm', (cx, cy, ROOF_Z + 0.27), (L - 0.3, W - 0.3, 0.54), X, Y, Z)
        ub.tarp_cover_at(p, (cx, cy), L, W, 0.0, ROOF_Z + 0.08, 0.56, m='Player', seed=seed, ties=3, box=14)


def ground(p):
    # o mastro da bandeira (frente, esquerda): a sapata, o poste torneado, a bola, o pano (o jogo desenha no vento)
    fx, fy = 11.3, -12.6
    ub.obox(p, 'Concrete#pnl', (fx, fy, 0.05), (1.1, 1.1, 0.5), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 0.3), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.05), (0.13, 0.1), (0.11, 0.4), (0.065, 10.4), (0.0, 10.4)],
             seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 10.7), 'z', [(0.0, 0.0), (0.07, 0.0), (0.13, 0.08), (0.13, 0.13), (0.07, 0.21), (0.0, 0.21)], seg=12)
    flag(p, (fx, fy), 10.6, 2.8, 1.7, m='Player')
    # os 2 postes de luz virados pra saída
    for s in (-1, 1):
        ub.lamp_post(p, 11.6, s * 6.6, h=6.5, aim=(9.0, 0.0), heads=2)
    # o tanque de combustível deitado no berço (frente, direita), com a escada e a linha até o bloco
    ub.obox(p, 'Concrete#pnl', (9.9, 10.6, 0.0), (3.6, 2.2, 0.4), X, Y, Z)
    ub.h_tank(p, (9.9, 10.6, 1.25), 'x', 0.72, 2.6, m='Body', band='Player', saddles=2, foot_z=0.2)
    ub.pipe(p, 'Trim', [(8.3, 10.2, 0.45), (7.0, 10.2, 0.45), (7.0, 9.4, 0.45)], 0.07)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    block(p)
    garage(p)
    apron(p)
    upper(p)
    roof(p)
    ground(p)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o radar (origem no eixo)
def _rot(x, z):
    return x * math.cos(TILT) - z * math.sin(TILT), x * math.sin(TILT) + z * math.cos(TILT)


def rpt(s, t, back=0.0):
    """O ponto da casca (s ao longo da largura, t de pé; back = pra trás da face da frente)."""
    xr, zr = _rot(s * s / (4 * FH) + t * t / (4 * FV) - back, t)
    return R0 + Vector((xr, s, zr))


def squircle(rho, th):
    c, sn = math.cos(th), math.sin(th)
    return RW / 2 * rho * math.copysign(abs(c) ** 0.5, c), RH / 2 * rho * math.copysign(abs(sn) ** 0.5, sn)


def t_max(s):
    return RH / 2 * max(0.0, 1 - (2 * abs(s) / RW) ** 4) ** 0.25


def seams():
    smax = RW / 2 * 0.985
    return smax, [-smax + 2 * smax * i / NSTRIP for i in range(NSTRIP + 1)]


def shell(p):
    """A casca de laranja em NSTRIP chapas verticais (cada uma um sólido: a frente, as costas e as 4 bordas), com a junta de 6 cm entre
    elas (a costela escura atrás aparece na junta), no contorno de superelipse; o aro de tubo em volta."""
    b = p.bm('Paint#dish', 0.0)
    smax, sj = seams()
    g = 0.03
    ns, nt = 4, 10
    for i in range(NSTRIP):
        s0 = sj[i] + (g if i else 0.0)
        s1 = sj[i + 1] - (g if i < NSTRIP - 1 else 0.0)
        fr, bk = [], []
        for a in range(ns + 1):
            sa = s0 + (s1 - s0) * a / ns
            tm = t_max(sa)
            fr.append([b.verts.new(V(*rpt(sa, -tm + 2 * tm * c / nt))) for c in range(nt + 1)])
            bk.append([b.verts.new(V(*rpt(sa, -tm + 2 * tm * c / nt, THK))) for c in range(nt + 1)])
        for a in range(ns):
            for c in range(nt):
                b.faces.new([fr[a][c], fr[a][c + 1], fr[a + 1][c + 1], fr[a + 1][c]]).smooth = True
                b.faces.new([bk[a + 1][c], bk[a + 1][c + 1], bk[a][c + 1], bk[a][c]]).smooth = True
        for c in range(nt):
            b.faces.new([fr[0][c], bk[0][c], bk[0][c + 1], fr[0][c + 1]])
            b.faces.new([fr[ns][c + 1], bk[ns][c + 1], bk[ns][c], fr[ns][c]])
        for a in range(ns):
            b.faces.new([fr[a][0], fr[a + 1][0], bk[a + 1][0], bk[a][0]])
            b.faces.new([fr[a + 1][nt], fr[a][nt], bk[a][nt], bk[a + 1][nt]])
    n = 48
    top = [rpt(-smax + 2 * smax * k / n, t_max(-smax + 2 * smax * k / n) + 0.02, THK / 2) for k in range(n + 1)]
    bot = [rpt(smax - 2 * smax * k / n, -t_max(smax - 2 * smax * k / n) - 0.02, THK / 2) for k in range(n + 1)]
    vp.tube(p, 'Trim', top + bot + [top[0]], 0.06, seg=8, caps=False)


def radar():
    p = CPart(PREFIX + '_Radar', cull=False)
    ub.keys(p)
    nrm = Vector((math.cos(TILT), 0.0, math.sin(TILT)))         # pra onde a casca olha
    up = Vector((-math.sin(TILT), 0.0, math.cos(TILT)))
    # a mesa giratória, a casa do giro (placas, a faixa azul, a veneziana), o contrapeso
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (1.5, 0.0), (1.5, 0.22), (1.4, 0.3), (0.0, 0.3)], seg=48)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.3), 'z', 1.3, 24, r=0.03, h=0.02)
    ub.obox(p, 'Body#pnl', (-0.35, 0.0, 0.86), (2.1, 1.8, 1.12), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Player#sm', (-0.35, s * 0.915, 0.95), (1.7, 0.04, 0.3), X, Y, Z)
    louvers(p, (-1.42, 0.0, 0.85), (-1, 0, 0), (0, 1, 0), 1.2, 0.6, 6)
    ub.obox(p, 'Dark#pnl', (-1.75, 0.0, 1.05), (0.7, 1.5, 0.9), X, Y, Z)
    # a coluna da casa até as costas da casca, os 2 braços
    ub.obox(p, 'Body#pnl', (0.18, 0.0, 2.4), (0.42, 0.9, 2.1), X, Y, Z)
    for s in (-1, 1):
        vp.tube(p, 'Trim', [Vector((-0.2, s * 0.7, 1.4)), rpt(s * 2.6, -0.95, THK + 0.32)], 0.08, seg=10)
        vp.tube(p, 'Trim', [Vector((0.1, s * 0.4, 3.4)), rpt(s * 3.2, 0.95, THK + 0.32)], 0.07, seg=10)
    shell(p)
    # as costelas atrás nas juntas das chapas (seguindo a curva), os 3 banzos e as diagonais da treliça
    for s in seams()[1][1:-1]:
        tm = t_max(s) * 0.94
        rings = []
        for i in range(9):
            t = -tm + 2 * tm * i / 8
            q = rpt(s, t, THK)
            h = 0.3 - 0.14 * abs(t) / RH * 2
            rings.append([tuple(q + Vector((0, -0.05, 0))), tuple(q + Vector((0, 0.05, 0))), tuple(q + Vector((0, 0.05, 0)) - nrm * h),
                          tuple(q + Vector((0, -0.05, 0)) - nrm * h)])
        p.loft('Trim#rib', rings, bevel=0.0)
    ss = [-5.2 + 10.4 * i / 14 for i in range(15)]
    for t in (-0.95, 0.0, 0.95):
        vp.tube(p, 'Trim', [rpt(s, t, THK + 0.3) for s in ss], 0.055, seg=8)
    for i in range(14):
        ta, tb = (-0.95, 0.0) if i % 2 == 0 else (0.0, -0.95)
        vp.tube(p, 'Trim', [rpt(ss[i], ta, THK + 0.3), rpt(ss[i + 1], tb, THK + 0.3)], 0.035, seg=6)
        ta, tb = (0.0, 0.95) if i % 2 == 0 else (0.95, 0.0)
        vp.tube(p, 'Trim', [rpt(ss[i], ta, THK + 0.3), rpt(ss[i + 1], tb, THK + 0.3)], 0.035, seg=6)
    # a corneta escura no braço (embaixo, na frente da casca) e a guia de onda até a casa
    hp = R0 + Vector((_rot(2.0, -1.05)[0], 0.0, _rot(2.0, -1.05)[1]))
    look = (rpt(0.0, 0.0) - hp).normalized()
    ex = look
    ey = Vector((0, 1, 0))
    ez = ex.cross(ey).normalized()
    rings = []
    for u, (a, b2) in ((-0.4, (0.18, 0.13)), (0.0, (0.22, 0.17)), (0.65, (0.7, 0.4))):
        c = hp + ex * u
        rings.append([tuple(c + ey * (sa * a) + ez * (sb * b2)) for sa, sb in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    p.loft('Dark#horn', rings, bevel=0.0)
    for s in (-1, 1):
        vp.tube(p, 'Trim', [rpt(s * 0.9, -t_max(0.9) * 0.97, THK / 2), hp + ey * (s * 0.18) - ez * 0.1], 0.04, seg=8)
    wg = [hp - ex * 0.35, hp - ex * 0.35 - ez * 0.3, Vector((1.8, 0.0, 0.9)), Vector((0.6, 0.0, 0.9))]
    vp.tube(p, 'Dark', vp.bent_bar(wg, R=0.2, n=4), 0.07, seg=8)
    # a barra do IFF em cima da casca: o corpo de placas, os 16 dipolos, a faixa azul atrás, os 3 apoios, a sirene
    bc = R0 + Vector((_rot(0.15, RH / 2 + 0.42)[0], 0.0, _rot(0.15, RH / 2 + 0.42)[1]))
    ub.obox(p, 'Body#pnl', bc, (0.34, 9.6, 0.6), nrm, Vector(Y), up)
    for k in range(20):
        s = -4.5 + 9.0 * k / 19
        ub.obox(p, 'Dark#sm', bc + nrm * 0.19 + Vector((0, s, 0)), (0.07, 0.14, 0.44), nrm, Vector(Y), up)
    ub.obox(p, 'Player#sm', bc - nrm * 0.18 + up * 0.08, (0.03, 7.2, 0.22), nrm, Vector(Y), up)
    for s in (-3.6, 0.0, 3.6):
        vp.tube(p, 'Trim', [rpt(s, t_max(s) * 0.98, THK / 2), bc + Vector((0, s, 0)) - up * 0.3], 0.055, seg=8)
    ub.beacon(p, tuple(bc + up * 0.3), r=0.09, h=0.16, cage=False)
    return ub.finish(p)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_command_center_v2')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    r = radar()
    pts = [tuple(BLOCK.corner(k, SKIRT[0] + 0.08))[:2] for k in range(BLOCK.n)]
    sc = scaffold_poly(PREFIX + '_Scaffold', pts, WALL_Z + 0.4, out=0.95, skip=(BLOCK.n - 1,))
    check([b, r, sc], HX, HY, at={r.name: RADAR_AT})
    # o raio que o radar varre (tem de caber sobre o tambor e longe das antenas do teto)
    sweep = max(math.hypot(v.co.x, v.co.y) for v in r.data.vertices)
    zlow = min(v.co.z for v in r.data.vertices if math.hypot(v.co.x, v.co.y) > 2.2)
    print('MODEL radar: varre r = %.2f m; o mais baixo fora do pedestal a z = %.2f m' % (sweep, zlow + RADAR_AT[2]))
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, r, sc)})
    r = bake_in_place(r, RADAR_AT)
    total = export(b, out)
    total += export(r, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL spin at_m', list(RADAR_AT), 'rpm 6')
    print('MODEL fire_points_m', [[3.6, 6.4, ROOF_Z], [-6.0, -9.6, ROOF_Z], [-6.6, 2.6, UP_ROOF]])
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]]],
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1400]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                  ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
                 (4.0, 19.5, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
