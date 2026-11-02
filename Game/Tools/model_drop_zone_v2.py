"""O Supply Drop Zone da USA (usa_drop_zone), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_drop_zone.py,
aprovada em 23/10 com o avião de carga: "ta tudo top") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_drop_zone_v2.py C:/Users/luisf/explosion/captures/game/models/usa_drop_zone_v2

O mesmo desenho do A (a palavra é O ALVO DE LANÇAMENTO: o pátio claro com o X branco no anel amarelo, o meio livre pras caixas, a torre
de controle num canto de trás com o radar girando, a pilha das caixas recebidas, a carga coberta com a lona azul, o mastro de luz e a
biruta; lote 16 × 16 m) e as mesmas medidas, refeito peça por peça (docs/ESTUDO_DETALHE_AAA.md; a linguagem da War Factory da USA):
  - o pátio em 4 lajes com as juntas de dilatação e a borda redonda; as luzes de pista torneadas; a tinta do alvo como no A;
  - a torre: as 4 pernas de tubo nas sapatas, o contraventamento em X com as chapas de ligação, a escada de marinheiro com a gaiola,
    a passarela com o guarda-corpo em volta da cabine, a cabine de placas com a faixa de janelas (vidro recuado, montantes, peitoril),
    a faixa azul, a aba do teto, o pedestal do radar, a sirene e a antena;
  - a pilha: as caixas B (model_supply_crate_v2.crate_into) e a carga coberta com a LONA azul de verdade (o pano que cai, as cintas);
  - o mastro de luz com os 2 holofotes (model_usa_building_parts.lamp_post) e a biruta de manga listrada que cai com o peso;
  - o radar de aproximação: o refletor curvo de verdade (a casca em arco), as costelas, a treliça de apoio e a corneta.
A USA limpa (13/10): sem desgaste pintado.

Pivôs e pontos MANTIDOS (Units.json usa_drop_zone): spin do radar at_m (−6,3; −6,3; 6,4) = RADAR_AT (a origem da malha do radar); o
topo da laje do pátio em 0,14 (as vagas cargo.slots_m pousam nele); o alvo no centro do lote; size_m 16 × 16.
Peças (os NOMES do A): SM_USA_DropZone_Base, SM_USA_DropZone_Radar (origem no eixo), SM_USA_DropZone_Scaffold.
Slots (os do A): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate, CivCloth.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, clear_scene, export, scaffold  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_supply_crate_v2 import LID_TOP, crate_into  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_drop_zone_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_DropZone'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (96, 98, 62),
               'CivCloth': (150, 128, 96)})

# ---------------------------------------------------------------------------------- as medidas do A (mantidas)
HX = HY = 8.0                   # o lote 16 × 16
PAD = 5.0                       # o pátio: meia largura (10 × 10 m)
Z0 = 0.14                       # o topo da laje do pátio (as caixas pousam aqui)
MARK = 0.02                     # a espessura da tinta
RING = (3.15, 3.55)             # o anel amarelo do alvo
TC = (-6.3, -6.3)               # a torre de controle (canto de trás, esquerda)
CAB_Z0, CAB_Z1 = 4.25, 6.05     # a cabine da torre
RADAR_AT = (TC[0], TC[1], 6.4)  # o eixo do radar, em cima do teto da torre

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


# ---------------------------------------------------------------------------------- o pátio
def pad(p):
    """As 4 lajes com as juntas, a tinta do alvo (a borda branca, as cantoneiras de perigo, o anel amarelo, o X) e as luzes de pista."""
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * PAD / 2, sy * PAD / 2
            ub.slab(p, 'Concrete#pnl', cx - (PAD - 0.06) / 2, cx + (PAD - 0.06) / 2, cy - (PAD - 0.06) / 2, cy + (PAD - 0.06) / 2, -0.12, Z0)
    for (a, b) in (((-PAD, -0.04), (PAD, 0.04)), ((-0.04, -PAD), (0.04, PAD))):          # o fundo escuro das juntas
        ub.obox(p, 'Dark#vp', ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, Z0 - 0.05), (b[0] - a[0], b[1] - a[1], 0.06), X, Y, Z)
    zp0, zp1 = Z0 - 0.01, Z0 + MARK
    e0, e1 = PAD - 0.47, PAD - 0.25
    for (x0, x1, y0, y1) in ((-e1, e1, -e1, -e0), (-e1, e1, e0, e1), (-e1, -e0, -e0, e0), (e0, e1, -e0, e0)):
        ub.flat_paint(p, 'Paint', [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], zp0, zp1)
    for sx in (-1, 1):
        for sy in (-1, 1):
            # a cantoneira de perigo em L (um polígono só, o canto chanfrado)
            pts = [(2.55, 3.9), (3.9, 3.9), (3.9, 2.55), (4.35, 2.55), (4.35, 4.15), (4.15, 4.35), (2.55, 4.35)]
            pts = [(sx * x, sy * y) for x, y in pts]
            if sx * sy < 0:
                pts = pts[::-1]
            ub.flat_paint(p, 'Hazard', pts, zp0, zp1)
    ub.ring_paint(p, 'Yellow', (0.0, 0.0), RING[0], RING[1], zp0, zp1)
    A, w = 4.0, 0.275
    plus = [(A, w), (w, w), (w, A), (-w, A), (-w, w), (-A, w), (-A, -w), (-w, -w), (-w, -A), (w, -A), (w, -w), (A, -w)]
    c45 = math.cos(math.radians(45))
    ub.flat_paint(p, 'Paint', [((u - v) * c45, (u + v) * c45) for u, v in plus], zp0, Z0 + 2 * MARK)
    for t in (-2.5, 0.0, 2.5):
        for (x, y) in ((PAD - 0.14, t), (-PAD + 0.14, t), (t, PAD - 0.14), (t, -PAD + 0.14)):
            ub.runway_light(p, x, y, Z0 - 0.005)


# ---------------------------------------------------------------------------------- a torre de controle
def tower(p):
    cx, cy = TC
    h = 0.7
    legs = [(cx + sx * h, cy + sy * h) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for (x, y) in legs:
        ub.obox(p, 'Concrete#pnl', (x, y, 0.02), (0.56, 0.56, 0.44), X, Y, Z)
        vp.lathe(p, 'Trim#vp', (x, y, 0.24), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.03), (0.12, 0.05), (0.1, 0.12), (0.1, CAB_Z0 - 0.4),
                                                    (0.0, CAB_Z0 - 0.4)], seg=14)
        vp.bolt_ring(p, 'Dark', (x, y, 0.27), 'z', 0.16, 4, r=0.02, h=0.016, phase=0.785)
    for i in range(4):                                     # o contraventamento em X de cada face, as travessas e as chapas
        (ax, ay), (bx, by) = legs[i], legs[(i + 1) % 4]
        for (za, zb) in ((0.5, 2.3), (2.3, 4.1)):
            for (z0, z1) in ((za, zb), (zb, za)):
                vp.tube(p, 'Trim', [(ax, ay, z0), (bx, by, z1)], 0.04, seg=8)
            vp.tube(p, 'Trim', [(ax, ay, zb), (bx, by, zb)], 0.05, seg=8)
            mx, my = (ax + bx) / 2, (ay + by) / 2
            d = Vector((bx - ax, by - ay, 0.0)).normalized()
            ub.obox(p, 'Trim#sm', (mx, my, (za + zb) / 2), (0.2, 0.03, 0.2), d, Vector((-d.y, d.x, 0.0)), Z)
    # a escada de marinheiro com a gaiola (lado do pátio, +X) até a passarela
    ub.ladder(p, 'Trim', cx + h + 0.02, cy, 0.24, CAB_Z0 - 0.12, (1, 0, 0), w=0.5, cage=True, wall=0.9, top_ext=1.0)
    # a passarela em volta da cabine (a grade escura na moldura) e o guarda-corpo
    ub.obox(p, 'Trim#pnl', (cx, cy, CAB_Z0 - 0.08), (3.1, 3.1, 0.16), X, Y, Z)
    ub.obox(p, 'Dark#vp', (cx, cy, CAB_Z0 + 0.003), (2.95, 2.95, 0.006), X, Y, Z)
    r = 1.5
    ub.rail(p, 'Trim', [(cx + r, cy + 0.35), (cx + r, cy + r), (cx - r, cy + r), (cx - r, cy - r), (cx + r, cy - r), (cx + r, cy - 0.35)],
            CAB_Z0, h=1.0, r=0.024, step=1.1, toe='Trim')
    # a cabine: o miolo, a parede de placas embaixo das janelas, a faixa de janelas, a faixa azul, a aba do teto
    hw = 1.15
    w0, w1 = CAB_Z0 + 0.8, CAB_Z1 - 0.25
    ub.obox(p, 'Body#pnl', (cx, cy, (CAB_Z0 + CAB_Z1) / 2), (2 * hw - 0.1, 2 * hw - 0.1, CAB_Z1 - CAB_Z0), X, Y, Z)
    for (n, e) in (((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, -1, 0)), ((0, 1, 0), (-1, 0, 0)), ((0, -1, 0), (1, 0, 0))):
        en, eu = Vector(n), Vector(e)
        c0 = Vector((cx, cy, 0.0)) + en * (hw - 0.05)
        ub.panel_wall(p, c0 - eu * hw + Vector((0, 0, CAB_Z0)), eu, Z, en, 2 * hw, 0.78, pw=hw, m='Body#pnl', gap=0.05, proud=0.06,
                      embed=0.03)
        ub.panel_wall(p, c0 - eu * hw + Vector((0, 0, w1)), eu, Z, en, 2 * hw, CAB_Z1 - w1, pw=hw, m='Body#pnl', gap=0.05, proud=0.06,
                      embed=0.03, min_piece=0.2)
        ub.obox(p, 'Player#sm', c0 + en * 0.085 + Vector((0, 0, CAB_Z0 + 0.45)), (2 * hw - 0.3, 0.3, 0.03), eu, Z, en)
        # a faixa de janelas: o vidro recuado, o peitoril, os montantes, a verga
        ub.obox(p, 'Glass#vp', c0 + en * 0.01 + Vector((0, 0, (w0 + w1) / 2)), (2 * hw - 0.2, w1 - w0, 0.02), eu, Z, en)
        ub.obox(p, 'Body#sm', c0 + en * 0.08 + Vector((0, 0, w0 - 0.02)), (2 * hw, 0.06, 0.16), eu, Z, en)
        for t in (-0.98, -0.33, 0.33, 0.98):
            ub.obox(p, 'Trim#sm', c0 + en * 0.04 + eu * t + Vector((0, 0, (w0 + w1) / 2)), (0.07, w1 - w0, 0.08), eu, Z, en)
    ub.obox(p, 'Trim#pnl', (cx, cy, CAB_Z1 + 0.1), (2 * hw + 0.4, 2 * hw + 0.4, 0.2), X, Y, Z)        # a aba do teto
    ub.obox(p, 'Body#pnl', (cx, cy, CAB_Z1 + 0.24), (2 * hw - 0.1, 2 * hw - 0.1, 0.1), X, Y, Z)
    # o pedestal do radar (o eixo em RADAR_AT), a sirene no canto de trás e a antena
    hp = RADAR_AT[2] - CAB_Z1 - 0.24
    vp.lathe(p, 'Dark#vp', (RADAR_AT[0], RADAR_AT[1], CAB_Z1 + 0.24), 'z', [(0.0, 0.0), (0.42, 0.0), (0.42, 0.03), (0.3, 0.06), (0.28, hp),
                                                                             (0.0, hp)], seg=24)
    vp.bolt_ring(p, 'Trim', (RADAR_AT[0], RADAR_AT[1], CAB_Z1 + 0.31), 'z', 0.36, 10, r=0.018, h=0.012)
    bx, by = cx - 1.0, cy - 1.0
    vp.lathe(p, 'Trim#vp', (bx, by, CAB_Z1 + 0.29), 'z', [(0.0, 0.0), (0.08, 0.0), (0.05, 0.04), (0.04, 0.9), (0.0, 0.9)], seg=10)
    ub.beacon(p, (bx, by, CAB_Z1 + 1.19), r=0.09, h=0.18)
    ub.antenna(p, cx + 1.0, cy - 1.0, CAB_Z1 + 0.29, 2.2)
    # a caixa de comando no pé da torre e o conduíte até ela
    ub.obox(p, 'Dark#pnl', (cx + 1.25, cy + 0.9, 0.62), (0.5, 0.7, 1.0), X, Y, Z)
    ub.louvers(p, (cx + 1.51, cy + 0.9, 0.7), (1, 0, 0), (0, 1, 0), 0.4, 0.36, 4)
    ub.pipe(p, 'Trim', [(cx + 1.25, cy + 0.9, 1.12), (cx + 1.25, cy + 0.9, 1.5), (cx + 1.0, cy + 1.05, 1.5), (cx + 1.0, cy + 1.05, CAB_Z0 - 0.2)],
            0.045)


# ---------------------------------------------------------------------------------- a pilha das caixas e a carga coberta
def stacks(p):
    ax, ay = -6.4, 6.3
    jit = [2.0, -3.0, 1.5, -1.0, 4.0, -2.5]
    k = 0
    for dx in (-0.53, 0.53):
        for dy in (-0.63, 0.63):
            crate_into(p, (ax + dx, ay + dy, 0.0, 90.0 + jit[k]), bag=False)
            k += 1
    for dy in (-0.55, 0.55):
        crate_into(p, (ax, ay + dy, LID_TOP + 0.005, jit[k]), bag=False)
        k += 1
    bxc, byc = -6.4, 3.3
    for dx in (-0.53, 0.53):
        crate_into(p, (bxc + dx, byc, 0.0, 90.0), bag=False)
    ub.tarp_cover(p, bxc - 1.22, bxc + 1.22, byc - 0.76, byc + 0.76, 0.42, LID_TOP + 0.08 - 0.42, m='Player', seed=1.3, ties=2, box=20)


# ---------------------------------------------------------------------------------- a biruta
def windsock(p, x, y, h=5.0, length=1.9):
    """A biruta: a sapata, o poste torneado, o pivô com o aro e a manga de 5 gomos (amarelo e branco) que afina e CAI com o peso."""
    ub.obox(p, 'Concrete#pnl', (x, y, 0.02), (0.7, 0.7, 0.44), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (x, y, 0.24), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.03), (0.08, 0.06), (0.06, 0.2), (0.05, h - 0.24),
                                               (0.0, h - 0.24)], seg=12)
    vp.lathe(p, 'Dark#vp', (x, y, h - 0.02), 'z', [(0.0, 0.0), (0.08, 0.0), (0.08, 0.1), (0.0, 0.12)], seg=12)
    w = Vector((0.55, 0.75, 0.0)).normalized()
    r0, r1, seg = 0.3, 0.13, 5
    base = Vector((x, y, h + 0.04)) + w * 0.14
    vp.lathe(p, 'Trim#vp', base, (w.x, w.y, 0.0), [(r0 - 0.005, -0.025), (r0 + 0.03, -0.025), (r0 + 0.03, 0.025), (r0 - 0.005, 0.025)],
             seg=20, closed=True)
    vp.tube(p, 'Trim', [Vector((x, y, h + 0.04)), base], 0.02, seg=6)
    n = seg * 4

    def at(t):
        return base + w * (length * t) + Vector((0, 0, -0.42 * t ** 1.8))
    for i in range(seg):
        pts = [at((i * 4 + j) / n) for j in range(5)]
        vp.tube(p, 'Yellow' if i % 2 == 0 else 'Paint', pts, r0, seg=16, caps=(i == seg - 1),
                r_fn=lambda j, i=i: r0 + (r1 - r0) * ((i * 4 + j) / n))


# ---------------------------------------------------------------------------------- a base (casco)
def base():
    p = Part(PREFIX + '_Base')
    ub.keys(p)
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.35), (-HX, HX, HY - 0.35, HY), (-HX, -HX + 0.35, -HY + 0.35, HY - 0.35),
                             (HX - 0.35, HX, -HY + 0.35, HY - 0.35)):
        ub.obox(p, 'Concrete#pnl', ((x0 + x1) / 2, (y0 + y1) / 2, -0.09), (x1 - x0, y1 - y0, 0.42), X, Y, Z)
    pad(p)
    tower(p)
    stacks(p)
    ub.lamp_post(p, 6.4, -6.4, h=7.0, aim=(0.0, 0.0), heads=2, head=(0.72, 0.44))
    windsock(p, 6.3, 6.2)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o radar (origem no eixo)
def radar():
    """O radar de aproximação: o pedestal, a caixa giratória, o refletor curvo em arco (a casca com espessura, inclinada 12°), as
    costelas, a treliça de apoio e a corneta no braço. Origem no eixo (RADAR_AT); o jogo gira em Z (~15 rpm). O envelope do A."""
    p = Part(PREFIX + '_Radar')
    ub.keys(p)
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (0.26, 0.0), (0.26, 0.05), (0.2, 0.08), (0.18, 0.16), (0.0, 0.16)], seg=20)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.05), 'z', 0.22, 8, r=0.016, h=0.012)
    ub.obox(p, 'Body#sm', (0.0, 0.0, 0.39), (0.32, 0.3, 0.46), X, Y, Z)
    ub.obox(p, 'Trim#sm', (0.0, 0.0, 0.66), (0.14, 2.3, 0.1), X, Y, Z)                  # a travessa de trás
    tilt = math.radians(12)
    up = Vector((-math.sin(tilt), 0.0, math.cos(tilt)))
    fx = Vector((math.cos(tilt), 0.0, math.sin(tilt)))
    rings = []
    ny = 18
    for i in range(ny + 1):
        yy = -1.25 + 2.5 * i / ny
        xx = 0.1 + 0.11 * (yy / 1.25) ** 2                     # o arco em planta (as pontas viram pra frente)
        c = Vector((xx, yy, 0.92))
        rings.append([tuple(c + up * (s * 0.26) + fx * f) for s, f in ((-1, 0.0), (1, 0.0), (1, -0.025), (-1, -0.025))])
    p.loft('Paint#vp', rings, bevel=0.0, smooth=True)
    # as costas escuras do refletor (o A lia pelas costas escuras na câmera do jogo; o branco sozinho sumia na areia)
    p.loft('Dark#vp', [[tuple(Vector(q) - fx * 0.03) for q in r] for r in rings], bevel=0.0, smooth=True)
    for yy in (-0.9, -0.3, 0.3, 0.9):                          # as costelas atrás
        xx = 0.1 + 0.11 * (yy / 1.25) ** 2
        c = Vector((xx, yy, 0.92)) - fx * 0.07
        ub.obox(p, 'Dark#sm', c, (0.05, 0.05, 0.5), fx, Y, up)
        vp.tube(p, 'Trim', [Vector((0.0, yy * 0.8, 0.66)), c - up * 0.2], 0.018, seg=6)
    d = Vector((0.62, 0.0, 0.18))
    vp.tube(p, 'Trim', [Vector((0.12, 0.0, 0.64)), Vector((0.12, 0.0, 0.64)) + d], 0.03, seg=8)     # o braço da corneta
    vp.lathe(p, 'Dark#vp', Vector((0.12, 0.0, 0.64)) + d, (-1, 0, 0.35), [(0.0, 0.0), (0.05, 0.0), (0.1, 0.12), (0.11, 0.16), (0.0, 0.16)],
             seg=12)
    return ub.finish(p)


def write_spec():
    import json
    ra = [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', ra]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 800]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [0.0, 11.5, 0], 'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
    sd = 'captures/game/models/supply_drop_v2/SM_SupplyDrop_'
    drop = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', ra], [sd + 'Crate', [0.6, 0.4, 0.14]],
                      [sd + 'Crate', [-0.9, -0.7, 0.14]], [sd + 'Crate', [1.2, -1.5, 9.0]], [sd + 'Canopy', [1.2, -1.5, 9.0]]],
            'views': [], 'usa_at': [0.0, 11.5, 0], 'game': [['game_cam', 37.5, 200.0, 50.0, 0.85]]}
    with open(os.path.join(OUT, 'preview_drop.json'), 'w', encoding='utf-8') as f:
        json.dump(drop, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    r = bake_in_place(radar(), RADAR_AT)
    total = export(b, OUT)
    total += export(r, OUT)
    total += export(scaffold(PREFIX + '_Scaffold', TC[0] - 0.85, TC[0] + 0.85, TC[1] - 0.85, TC[1] + 0.85, CAB_Z1, gap=0.3), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, r)})
    print('MODEL lote 16 x 16 m; patio 10 x 10, laje em', Z0, '; alvo [0, 0]; spin radar at_m', RADAR_AT, 'rpm 15')
    write_spec()


main()
