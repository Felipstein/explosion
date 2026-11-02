"""O Airfield da USA (usa_airfield), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_usa_airfield.py, aprovada com
o sistema aéreo em 24/10: "meus parabéns, muita qualidade!") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_airfield_v2.py C:/Users/luisf/explosion/captures/game/models/usa_airfield_v2

O mesmo desenho do A (lote 40 × 32 m: a PISTA na frente, as QUATRO VAGAS em fila, o "H" do heliponto, o hangar de teto curvo e a torre
com o radar no fundo; docs/AEREO.md "Layout do Airfield (25/10)") e as mesmas medidas, refeito peça por peça na linguagem dos prédios
da USA (docs/ESTUDO_DETALHE_AAA.md):
  - o pátio em LAJES de 5 m com a junta funda e a borda redonda; a pista como a FAIXA ESCURA de asfalto (a ficha de texto,
    docs/elenco/usa/usa_airfield.md §6: "a faixa escura da pista na frente de ponta a ponta"; no A era o mesmo concreto do pátio) com
    as faixas brancas do A e as luzes de borda torneadas;
  - o hangar: as paredes de PLACAS com a faixa azul, a abóbada com as costuras em pé e os arcos, os exaustores redondos na cumeeira,
    o portão de 4 folhas com as nervuras e o trilho, o vão aberto fundo e escuro, as luminárias e as sirenes;
  - a torre: o prédio de baixo de placas com as janelas recuadas, a faixa azul, a porta, os condensadores com ventilador no teto, o
    fuste com os pilares redondos, a varanda octogonal com o guarda-corpo, a cabine de vidro inclinado com os montantes, a aba do teto
    com a faixa azul, o mastro do radar, as antenas e a sirene;
  - o tanque de combustível deitado (model_usa_building_parts.h_tank) na bacia de concreto, com a escada e a linha até o pátio;
  - os postes de luz com 2 holofotes virados pras vagas, a biruta de manga listrada que cai, o heliponto com as luzes torneadas;
  - o radar: a antena de vigilância com o refletor curvo, a treliça de trás, a faixa azul, a corneta e o contrapeso;
  - a vaga (a malha posta 4 vezes): as marcas como tinta de contorno liso e as 4 luzes de canto torneadas.
A USA limpa (13/10): sem desgaste pintado.

Pivôs e pontos MANTIDOS (Units.json usa_airfield): spin do radar at_m (12,6; 12,2; 12,45) = RADAR_AT (a origem da malha do radar); as
vagas pads_m (−14,4 / −4,8 / 4,8 / 14,4; 1,5; yaw −90) = a origem da malha da vaga (o centro); runways_m (−18; −12,5 → 19; −12,5);
helipad_m (−12; 11,8); size_m 40 × 32; o topo da laje em 0,12.
Peças (os NOMES do A): SM_USA_Airfield_Base, SM_USA_Airfield_Radar (origem no eixo), SM_USA_Airfield_Pad (origem no centro da vaga),
SM_USA_Airfield_Scaffold.
Slots (os do A): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, scaffold  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_airfield_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Airfield'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62)})

# ---------------------------------------------------------------------------------- as medidas do A (mantidas)
HX, HY = 20.0, 16.0              # o lote 40 × 32
FLOOR = 0.12                     # o topo da laje
PAINT_Z = (FLOOR + 0.004, FLOOR + 0.014)
RUNWAY = (-16.0, -9.0)           # y da pista
RUNWAY_HEAD, RUNWAY_END = -18.0, 19.0
HELIPAD = (-12.0, 11.8)
HELIPAD_R, HELIPAD_HALF = 3.9, 4.15
TAXI_Y = -8.0
PADS = [(-14.4, 1.5), (-4.8, 1.5), (4.8, 1.5), (14.4, 1.5)]
PAD_SIZE = (9.6, 12.4)
HANGAR = (-5.5, 5.5, 8.4, 15.8, 4.6, 6.4)      # x0, x1, y0, y1, altura da parede, altura da cumeeira
TOWER = (12.6, 12.2)
TOWER_H = (4.0, 8.8, 11.0)       # o topo do prédio de baixo, o piso da cabine de vidro, o teto
RADAR_AT = (TOWER[0], TOWER[1], TOWER_H[2] + 0.35 + 1.1)

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def paint(p, m, x0, x1, y0, y1, lift=0.0):
    p.box(m, min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1), PAINT_Z[0] + lift, PAINT_Z[1] + lift, bevel=0.0)


# ---------------------------------------------------------------------------------- o chão: as lajes, a pista
def ground(p):
    # as lajes do pátio (5 × 5 m, a junta de 6 cm com o fundo escuro), da pista até o fundo do lote
    xs = [-HX + 5.0 * i for i in range(9)]
    ys = [RUNWAY[1], -4.0, 1.0, 6.0, 11.0, HY]
    for i in range(8):
        for j in range(5):
            x0, x1, y0, y1 = xs[i] + 0.03, xs[i + 1] - 0.03, ys[j] + 0.03, ys[j + 1] - 0.03
            if i == 0:
                x0 = -HX
            if i == 7:
                x1 = HX
            if j == 4:
                y1 = HY
            ub.slab(p, 'Concrete#pnl', x0, x1, y0, y1, -0.12, FLOOR)
    for x in xs[1:-1]:
        ub.obox(p, 'Dark#vp', (x, (RUNWAY[1] + HY) / 2, FLOOR - 0.05), (0.08, HY - RUNWAY[1], 0.06), X, Y, Z)
    for y in ys[:-1]:
        ub.obox(p, 'Dark#vp', (0.0, y, FLOOR - 0.05), (2 * HX, 0.08, 0.06), X, Y, Z)
    # a pista: a faixa escura de asfalto com a borda redonda
    ub.slab(p, 'Trim#pnl', -HX, HX, -HY, RUNWAY[1] - 0.03, -0.12, FLOOR)
    runway(p, RUNWAY, -HX + 0.9)
    for k in range(11):
        xx = -HX + 1.0 + k * 3.8
        for y in (RUNWAY[0] + 0.2, RUNWAY[1] + 0.15):
            ub.runway_light(p, xx, y, FLOOR - 0.01)
    # a linha amarela de entrada de cada vaga até a borda da pista e a barra de espera dupla
    for xc, yc in PADS:
        paint(p, 'Yellow', xc - 0.1, xc + 0.1, RUNWAY[1] + 0.2, yc - PAD_SIZE[1] / 2 + 0.3)
        for yy in (RUNWAY[1] + 0.55, RUNWAY[1] + 0.85):
            paint(p, 'Yellow', xc - 1.6, xc + 1.6, yy - 0.06, yy + 0.06, 0.004)


def runway(p, band, head_x):
    """As marcas brancas da pista (as do A): as bordas, a linha do meio tracejada, as teclas de piano, as faixas de toque."""
    y0, y1 = band
    ym = (y0 + y1) / 2
    for y in (y0 + 0.35, y1 - 0.35):
        paint(p, 'Paint', -HX + 0.6, HX - 0.6, y - 0.12, y + 0.12)
    x = head_x + 3.3
    while x + 2.6 < HX - 4.0:
        paint(p, 'Paint', x, x + 2.6, ym - 0.14, ym + 0.14)
        x += 4.4
    hw = (y1 - y0) / 2 - 0.75
    for x0, x1 in ((head_x, head_x + 2.5), (HX - 3.4, HX - 0.9)):
        for k in range(6):
            yy = ym - hw + 0.22 + k * (2 * hw - 0.44) / 5
            if abs(yy - ym) < 0.3:
                continue
            paint(p, 'Paint', x0, x1, yy - 0.2, yy + 0.2)
    for yy in (ym - 0.36 * (y1 - y0) / 2 - 0.5, ym + 0.36 * (y1 - y0) / 2 + 0.5):
        paint(p, 'Paint', head_x + 8.0, head_x + 11.5, yy - 0.38, yy + 0.38)


def helipad(p):
    """O heliponto: a laje clara, o círculo amarelo de toque, o "H" branco, os cantos azuis e as luzes torneadas em volta."""
    hx, hy = HELIPAD
    h = HELIPAD_HALF
    ub.slab(p, 'Concrete#pnl', hx - h, hx + h, hy - h, hy + h, FLOOR - 0.02, FLOOR + 0.07)
    lift = 0.07
    ub.ring_paint(p, 'Yellow', (hx, hy), HELIPAD_R - 0.16, HELIPAD_R + 0.16, FLOOR + lift - 0.004, FLOOR + lift + 0.008)
    Hpts = [(-1.5, -1.4), (-0.9, -1.4), (-0.9, -0.3), (0.9, -0.3), (0.9, -1.4), (1.5, -1.4), (1.5, 1.4), (0.9, 1.4), (0.9, 0.3),
            (-0.9, 0.3), (-0.9, 1.4), (-1.5, 1.4)]
    ub.flat_paint(p, 'Paint', [(hx + v, hy + u) for u, v in Hpts], FLOOR + lift - 0.004, FLOOR + lift + 0.01)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = hx + sx * (h - 0.3), hy + sy * (h - 0.3)
            L = [(0, 0), (-1.6, 0), (-1.6, -0.3), (-0.3, -0.3), (-0.3, -1.6), (0, -1.6)]
            pts = [(cx + sx * a, cy + sy * b) for a, b in L]
            if sx * sy < 0:
                pts = pts[::-1]
            ub.flat_paint(p, 'Player', pts, FLOOR + lift - 0.004, FLOOR + lift + 0.01)
    for k in range(12):
        a = 2 * math.pi * k / 12
        ub.runway_light(p, hx + (HELIPAD_R + 0.3) * math.cos(a), hy + (HELIPAD_R + 0.3) * math.sin(a), FLOOR + lift, lens='Beacon', r=0.12)


# ---------------------------------------------------------------------------------- o hangar
def hangar(p):
    x0, x1, hy0, hy1, hw, hr = HANGAR
    ub.obox(p, 'Concrete#pnl', ((x0 + x1) / 2, (hy0 + hy1) / 2, FLOOR + 0.1), (x1 - x0 + 0.6, hy1 - hy0 + 0.6, 0.25), X, Y, Z)
    yf = hy0 + 0.4                                       # a parede da frente (a porta)
    dw = (x1 - x0 - 1.6) / 4
    xa, xb, zt, dn = x0 + 0.8, x0 + 0.8 + dw, hw - 0.62, 1.4      # o vão aberto: a folha 1 correu, o miolo recua dn (o fundo escuro)
    for (ax, bx, ay, by, az, bz) in ((x0, x1, yf + dn, hy1, FLOOR, hw), (x0, x1, yf, yf + dn, zt, hw), (x0, xa, yf, yf + dn, FLOOR, zt),
                                     (xb, x1, yf, yf + dn, FLOOR, zt)):
        ub.obox(p, 'Body#pnl', ((ax + bx) / 2, (ay + by) / 2, (az + bz) / 2), (bx - ax, by - ay, bz - az), X, Y, Z)
    ub.obox(p, 'Dark#vp', ((xa + xb) / 2, yf + dn - 0.03, (FLOOR + zt) / 2), (xb - xa, 0.06, zt - FLOOR), X, Y, Z)
    for xx in (xa + 0.03, xb - 0.03):
        ub.obox(p, 'Dark#vp', (xx, yf + dn / 2, (FLOOR + zt) / 2), (0.06, dn, zt - FLOOR), X, Y, Z)
    ub.obox(p, 'Light#vp', ((xa + xb) / 2, yf + dn * 0.5, zt - 0.05), (xb - xa - 0.4, 0.2, 0.04), X, Y, Z)
    rows = [1.3, 3.4, 4.1]
    # as paredes de placas: o fundo (+Y) e os lados (±X), a fileira azul (3,4..4,1) dos lados como no A
    ub.panel_wall(p, (x1, hy1, FLOOR + 0.05), (-1, 0, 0), Z, (0, 1, 0), x1 - x0, hw - FLOOR - 0.05, pw=1.85, m='Body#pnl', rows=rows)
    for s in (-1, 1):
        xf = x1 if s > 0 else x0
        ya, yb = (yf, hy1) if s > 0 else (hy1, yf)
        ub.panel_wall(p, (xf, ya, FLOOR + 0.05), (0, 1 if s > 0 else -1, 0), Z, (s, 0, 0), hy1 - yf, hw - FLOOR - 0.05, pw=1.75,
                      m='Body#pnl', rows=rows, row_mats={2: 'Player#pnl'})
    # a abóbada: a casca lisa (0,25 m), os arcos de tubo, as costuras em pé ao longo de x
    ny = 16
    arc = []
    for k in range(ny + 1):
        u = k / ny
        yy = (hy0 + 0.2) + (hy1 + 0.2 - hy0 - 0.2) * u
        arc.append((yy, hw + (hr - hw) * math.sin(math.pi * u)))
    rings = []
    for xx in (x0 - 0.25, x1 + 0.25):
        rings.append([(xx, yy, zz) for yy, zz in arc] + [(xx, yy, zz - 0.25) for yy, zz in reversed(arc)])
    p.loft('Body#roof', rings, bevel=0.0, smooth=True)
    for xx in [x0 + 0.1 + k * (x1 - x0 - 0.2) / 6 for k in range(7)]:
        vp.tube(p, 'Trim', [Vector((xx, yy, zz + 0.05)) for yy, zz in arc], 0.07, seg=8)
        ub.obox(p, 'Trim#sm', (xx, hy1 + 0.06, (FLOOR + hw) / 2), (0.24, 0.12, hw - FLOOR), X, Y, Z)
    for (yy, zz) in arc[1:-1:2]:
        vp.tube(p, 'Body', [(x0 - 0.2, yy, zz + 0.025), (x1 + 0.2, yy, zz + 0.025)], 0.03, seg=6)
    # as empenas (o arco fechado nas pontas) e a faixa de rufo
    for xx in (x0, x1 - 0.06):
        b = p.bm('Body#gable', 0.0)
        pts = [(hy0 + 0.4 + (hy1 - hy0 - 0.4) * k / ny, hw - 0.05 + (hr - hw - 0.15) * math.sin(math.pi * k / ny)) for k in range(ny + 1)]
        poly = list(reversed(pts)) + [(hy0 + 0.4, hw - 0.3), (hy1, hw - 0.3)]
        a = [b.verts.new(V(xx, y, z)) for y, z in poly]
        c = [b.verts.new(V(xx + 0.06, y, z)) for y, z in poly]
        b.faces.new(a)
        b.faces.new(list(reversed(c)))
        for i in range(len(poly)):
            j = (i + 1) % len(poly)
            b.faces.new([a[i], a[j], c[j], c[i]])
    # os exaustores redondos na cumeeira
    for xx in (x0 + 2.5, (x0 + x1) / 2, x1 - 2.5):
        ub.turbine_vent(p, xx, (hy0 + hy1) / 2 + 0.2, hr - 0.06, r=0.42)
    # o portão: o quadro, o trilho de cima, as 4 folhas de correr com as nervuras, o vão aberto fundo, a faixa azul, as luzes
    ub.obox(p, 'Trim#pnl', ((x0 + x1) / 2, yf - 0.05, hw - 0.42), (x1 - x0 - 0.6, 0.3, 0.3), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Trim#pnl', ((x0 + x1) / 2 + s * ((x1 - x0) / 2 - 0.45), yf - 0.05, (FLOOR + hw - 0.3) / 2), (0.3, 0.3, hw - 0.3 - FLOOR),
                X, Y, Z)
    ub.pipe(p, 'Trim', [(x0 + 0.5, yf - 0.24, hw - 0.62), (x1 - 0.5, yf - 0.24, hw - 0.62)], 0.05)
    for k in range(4):
        xa = x0 + 0.8 + k * dw
        if k == 0:
            xa += dw * 0.9                                 # a folha aberta: correu pra trás da segunda
        yk = yf - 0.12 - (0.1 if k % 2 else 0.0)
        # (as folhas de metal escuro com as nervuras: a porta clara não lia como porta na câmera do jogo)
        ub.obox(p, 'Trim#sm', (xa + dw / 2, yk, (FLOOR + 0.06 + hw - 0.62) / 2), (dw - 0.04, 0.1, hw - 0.68 - FLOOR), X, Y, Z)
        for zz in (1.0, 2.2, 3.4):
            ub.obox(p, 'Dark#sm', (xa + dw / 2, yk - 0.06, zz), (dw - 0.2, 0.05, 0.1), X, Y, Z)
        ub.obox(p, 'Dark#sm', (xa + dw / 2, yk - 0.06, (FLOOR + hw - 0.62) / 2), (0.08, 0.05, hw - 0.9 - FLOOR), X, Y, Z)
    ub.obox(p, 'Player#sm', ((x0 + x1) / 2, yf - 0.215, hw - 0.18), (x1 - x0 - 1.6, 0.03, 0.26), X, Y, Z)
    for xx in (x0 + 2.0, x1 - 2.0):
        ub.lamp_head(p, (xx, yf - 0.35, hw - 0.05), (0.0, -1.0, -0.9), w=0.6, h=0.26)
    for xx in (x0 + 0.3, x1 - 0.3):
        ub.beacon(p, (xx, hy0 + 0.6, hw + 0.05), r=0.09, h=0.2)
    paint(p, 'Yellow', (x0 + x1) / 2 - 0.1, (x0 + x1) / 2 + 0.1, 2.6, hy0 - 0.2)


# ---------------------------------------------------------------------------------- a torre
def tower(p):
    tx, ty = TOWER
    b0, f0, r0 = TOWER_H
    hw = 3.4
    # o prédio de baixo: o miolo, as placas com as janelas recuadas e a faixa azul, a porta
    ub.obox(p, 'Body#pnl', (tx, ty, (FLOOR + b0) / 2), (2 * hw - 0.1, 2 * hw - 0.1, b0 - FLOOR), X, Y, Z)
    for (n, e) in (((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, -1, 0)), ((0, 1, 0), (-1, 0, 0)), ((0, -1, 0), (1, 0, 0))):
        en, eu = Vector(n), Vector(e)
        c0 = Vector((tx, ty, 0.0)) + en * (hw - 0.05)
        org = c0 - eu * hw + Vector((0, 0, FLOOR))
        holes = [(hw - 2.8, hw + 2.8, 2.6 - FLOOR, 3.5 - FLOOR)]
        if n == (0, -1, 0):
            holes.append((hw - 1.0, hw + 0.2, -0.1, 2.3 - FLOOR))
        ub.panel_wall(p, org, eu, Z, en, 2 * hw, b0 - FLOOR, pw=1.7, m='Body#pnl', rows=[1.08, 1.58, 2.48 - FLOOR + 0.12, 3.38],
                      row_mats={1: 'Player#pnl'}, holes=holes, gap=0.06, proud=0.07, embed=0.04, min_piece=0.25)
        # a janela: o vidro recuado, os montantes, o peitoril
        ub.obox(p, 'Glass#vp', c0 + Vector((0, 0, 3.05)), (5.6, 0.9, 0.04), eu, Z, en)
        for t in (-2.1, -0.7, 0.7, 2.1):
            ub.obox(p, 'Trim#sm', c0 + eu * t + en * 0.03 + Vector((0, 0, 3.05)), (0.08, 0.9, 0.08), eu, Z, en)
        ub.obox(p, 'Body#sm', c0 + en * 0.09 + Vector((0, 0, 2.55)), (5.8, 0.08, 0.18), eu, Z, en)
    # a porta (−Y): o vão escuro, a folha, a marquise
    dc = Vector((tx - 0.4, ty - hw, 0.0))
    ub.obox(p, 'Dark#vp', dc + Vector((0, 0.02, (FLOOR + 2.3) / 2)), (1.2, 0.08, 2.3 - FLOOR), X, Y, Z)
    ub.obox(p, 'Body#sm', dc + Vector((0.25, -0.02, (FLOOR + 2.25) / 2)), (0.62, 0.05, 2.1 - FLOOR), X, Y, Z)
    ub.obox(p, 'Trim#sm', dc + Vector((0, -0.3, 2.45)), (1.6, 0.6, 0.08), X, Y, Z)
    # o teto do prédio de baixo: a laje com o rufo e os 2 condensadores com ventilador
    ub.obox(p, 'Body#pnl', (tx, ty, b0 + 0.12), (2 * hw + 0.2, 2 * hw + 0.2, 0.24), X, Y, Z)
    ub.obox(p, 'Trim#sm', (tx, ty, b0 + 0.02), (2 * hw + 0.28, 2 * hw + 0.28, 0.08), X, Y, Z)
    for (ax_, ay_) in ((-2.2, 2.0), (2.0, -2.2)):
        ub.fan_box(p, (tx + ax_, ty + ay_), (1.3, 1.0, 0.8), fans=1, z0=b0 + 0.24)
    # o fuste: as placas, os pilares redondos nos cantos, a janelinha da escada
    ub.obox(p, 'Body#pnl', (tx, ty, (b0 + f0) / 2), (2.5, 2.5, f0 - b0), X, Y, Z)
    for (n, e) in (((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, -1, 0)), ((0, 1, 0), (-1, 0, 0)), ((0, -1, 0), (1, 0, 0))):
        en, eu = Vector(n), Vector(e)
        c0 = Vector((tx, ty, 0.0)) + en * 1.25
        ub.panel_wall(p, c0 - eu * 1.0 + Vector((0, 0, b0 + 0.24)), eu, Z, en, 2.0, f0 - b0 - 0.4, pw=1.0, ph=1.5, m='Body#pnl',
                      gap=0.06, proud=0.06, embed=0.03)
    for sx in (-1, 1):
        for sy in (-1, 1):
            vp.lathe(p, 'Trim#vp', (tx + sx * 1.3, ty + sy * 1.3, b0 + 0.2), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.08), (0.15, 0.14),
                                                                                (0.15, f0 - b0 - 0.4), (0.0, f0 - b0 - 0.4)], seg=12)
    ub.obox(p, 'Glass#vp', (tx - 1.33, ty, (b0 + f0) / 2), (0.04, 0.5, 2.6), X, Y, Z)
    # a varanda octogonal (o piso com a borda redonda) e o guarda-corpo
    oct8 = lambda r, z, rot=22.5: [(tx + r * math.cos(math.radians(45 * k + rot)), ty + r * math.sin(math.radians(45 * k + rot)), z)
                                   for k in range(8)]
    p.loft('Body#pnl', [oct8(2.75, f0 - 0.42), oct8(2.75, f0 - 0.06), oct8(2.65, f0)])
    rp = [(q[0], q[1]) for q in oct8(2.62, 0)]
    ub.rail(p, 'Trim', rp + [rp[0]], f0, h=0.95, r=0.024, step=1.2, mid=True)
    # a cabine de vidro: o peitoril, o vidro inclinado pra fora, os montantes, a aba do teto com a faixa azul
    p.loft('Body#pnl', [oct8(2.2, f0 - 0.02), oct8(2.2, f0 + 0.35), oct8(2.26, f0 + 0.42)])
    p.loft('Glass#vp', [oct8(2.18, f0 + 0.4), oct8(2.5, r0 - 0.05)])
    for k in range(8):
        a = math.radians(45 * k + 22.5)
        q0 = Vector((tx + 2.24 * math.cos(a), ty + 2.24 * math.sin(a), f0 + 0.38))
        q1 = Vector((tx + 2.54 * math.cos(a), ty + 2.54 * math.sin(a), r0 - 0.04))
        vp.tube(p, 'Trim', [q0, q1], 0.055, seg=8)
    p.loft('Body#pnl', [oct8(2.55, r0 - 0.06), oct8(2.85, r0 + 0.04), oct8(2.85, r0 + 0.3), oct8(2.7, r0 + 0.36)])
    p.loft('Player#vp', [oct8(2.87, r0 + 0.08), oct8(2.87, r0 + 0.24)], caps=(False, False))
    # o mastro do radar, as antenas, a sirene
    vp.lathe(p, 'Trim#vp', (tx, ty, r0 + 0.34), 'z', [(0.0, 0.0), (0.4, 0.0), (0.4, 0.05), (0.2, 0.1), (0.17, 0.2), (0.15, RADAR_AT[2] - r0 - 0.46),
                                                     (0.42, RADAR_AT[2] - r0 - 0.4), (0.42, RADAR_AT[2] - r0 - 0.34), (0.0, RADAR_AT[2] - r0 - 0.34)],
             seg=20)
    ub.antenna(p, tx - 1.8, ty + 1.2, r0 + 0.36, 2.2)
    ub.antenna(p, tx + 1.6, ty - 1.6, r0 + 0.36, 1.6)
    ub.beacon(p, (tx + 2.2, ty, r0 + 0.36), r=0.11, h=0.22)


def fuel(p):
    """O tanque de combustível deitado na bacia de concreto (a mureta), a escada, a linha até o pátio, a faixa de perigo."""
    ub.slab(p, 'Concrete#pnl', 16.6, 19.8, 8.6, 15.8, FLOOR - 0.02, FLOOR + 0.13)
    for (cx, cy, sx, sy) in ((18.2, 8.75, 3.2, 0.3), (18.2, 15.65, 3.2, 0.3), (16.75, 12.2, 0.3, 7.2), (19.65, 12.2, 0.3, 7.2)):
        ub.obox(p, 'Concrete#pnl', (cx, cy, FLOOR + 0.3), (sx, sy, 0.6), X, Y, Z)
    ub.h_tank(p, (18.2, 12.2, 1.75), 'y', 1.05, 4.6, m='Body', band='Player', saddles=2, foot_z=FLOOR + 0.1)
    ub.ladder(p, 'Trim', 18.2 - 1.12, 12.2, FLOOR + 0.6, 2.8, (-1, 0, 0), w=0.45, cage=False, wall=0.12, top_ext=0.6)
    ub.pipe(p, 'Trim', [(18.2, 10.2, 0.62), (18.2, 8.3, 0.62), (16.2, 8.3, 0.62), (16.2, 8.3, FLOOR + 0.15), (15.0, 8.3, FLOOR + 0.15)], 0.07)
    vp.lathe(p, 'Yellow#vp', (16.2, 8.3, 0.6), 'y', [(0.0, -0.1), (0.12, -0.1), (0.12, 0.1), (0.0, 0.1)], seg=14)
    vp.tube(p, 'Yellow', [(16.2, 8.3, 0.72), (16.2, 8.3, 0.86), (16.45, 8.3, 0.86)], 0.025, seg=6)
    ub.obox(p, 'Hazard#sm', (18.2, 8.4, FLOOR + 0.01), (3.0, 0.3, 0.03), X, Y, Z)


def base():
    p = Part(PREFIX + '_Base')
    ub.keys(p)
    ground(p)
    hangar(p)
    fuel(p)
    helipad(p)
    tower(p)
    for (lx, ly) in ((19.6, 8.0), (-19.6, 8.0)):
        ub.lamp_post(p, lx, ly, h=9.0, aim=(lx * 0.6, 1.5), heads=2, footing=0.5)
    ub.windsock(p, 7.4, 15.2, wind=(-0.25, -1.0))
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o radar (origem no eixo)
def radar():
    """A antena de vigilância no pedestal: o refletor curvo comprido (3,4 m) inclinado 12° pra cima, a treliça de trás, a faixa azul,
    o braço da corneta, o contrapeso. Origem no eixo (RADAR_AT); o jogo gira em Z (~10 rpm). O envelope do A."""
    p = Part(PREFIX + '_Radar')
    ub.keys(p)
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (0.36, 0.0), (0.36, 0.06), (0.28, 0.1), (0.26, 0.3), (0.0, 0.3)], seg=20)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.06), 'z', 0.31, 10, r=0.018, h=0.012)
    ub.obox(p, 'Body#pnl', (0.0, 0.0, 0.56), (0.5, 0.5, 0.52), X, Y, Z)
    ub.obox(p, 'Player#sm', (-0.26, 0.0, 0.56), (0.03, 0.36, 0.2), X, Y, Z)
    ub.obox(p, 'Trim#sm', (-0.05, 0.0, 0.83), (0.2, 3.3, 0.16), X, Y, Z)          # o braço que segura o refletor
    tilt = math.radians(12)
    rings = []
    for yy in (-1.7, -0.85, 0.0, 0.85, 1.7):
        ring = []
        for k in range(11):
            t = -0.55 + 1.1 * k / 10
            x, z = 0.35 * math.cos(t) - 0.35, 0.95 * math.sin(t)
            ring.append((0.25 + x * math.cos(tilt) - z * math.sin(tilt), yy, 1.25 + x * math.sin(tilt) + z * math.cos(tilt)))
        for k in range(10, -1, -1):
            t = -0.55 + 1.1 * k / 10
            x, z = 0.35 * math.cos(t) - 0.35 - 0.06, 0.95 * math.sin(t)
            ring.append((0.25 + x * math.cos(tilt) - z * math.sin(tilt), yy, 1.25 + x * math.sin(tilt) + z * math.cos(tilt)))
        rings.append(ring)
    p.loft('Body#dish', rings, bevel=0.0, smooth=True)
    for yy in (-1.6, -0.8, 0.0, 0.8, 1.6):                  # as costelas de trás e a treliça
        ub.obox(p, 'Trim#sm', (0.05, yy, 1.25), (0.08, 0.06, 0.9), (math.cos(tilt), 0, -math.sin(tilt)), Y, (math.sin(tilt), 0, math.cos(tilt)))
        vp.tube(p, 'Trim', [(0.0, yy * 0.4, 0.86), (0.04, yy, 1.0)], 0.022, seg=6)
        vp.tube(p, 'Trim', [(0.0, yy * 0.4, 0.86), (0.08, yy, 1.62)], 0.018, seg=6)
    vp.tube(p, 'Trim', [(0.06, -1.6, 1.62), (0.06, 1.6, 1.62)], 0.022, seg=6)
    ub.obox(p, 'Player#sm', (0.215, 0.0, 1.07), (0.03, 2.0, 0.1), (math.cos(tilt), 0, -math.sin(tilt)), Y, (math.sin(tilt), 0, math.cos(tilt)))
    vp.tube(p, 'Trim', [(0.3, 0.0, 0.9), (1.1, 0.0, 1.1)], 0.035, seg=8)                  # o braço da corneta
    vp.lathe(p, 'Dark#vp', (1.08, 0.0, 1.1), (-1, 0, 0.3), [(0.0, -0.05), (0.08, -0.05), (0.14, 0.12), (0.15, 0.18), (0.0, 0.18)], seg=12)
    ub.obox(p, 'Dark#sm', (-0.45, 0.0, 0.9), (0.4, 0.5, 0.3), X, Y, Z)                     # o contrapeso
    return ub.finish(p)


# ---------------------------------------------------------------------------------- a vaga (origem no centro, nariz pra −Y)
def pad():
    """As marcas de uma vaga: o contorno amarelo, o círculo de pouso liso, a linha de entrada até a barra de parada branca, as setas de
    alinhamento, o quadro azul (cor do jogador) no fundo e as 4 luzes de canto torneadas."""
    p = Part(PREFIX + '_Pad')
    w, h = PAD_SIZE
    x0, x1, y0, y1 = -w / 2 + 0.3, w / 2 - 0.3, -h / 2 + 0.3, h / 2 - 0.3
    t = 0.14
    z0, z1 = PAINT_Z
    outer = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    for (a, b, c, d) in ((x0, x1, y0, y0 + t), (x0, x1, y1 - t, y1), (x0, x0 + t, y0 + t, y1 - t), (x1 - t, x1, y0 + t, y1 - t)):
        ub.flat_paint(p, 'Yellow', [(a, c), (b, c), (b, d), (a, d)], z0, z1)
    _ = outer
    ub.ring_paint(p, 'Paint', (0.0, 0.0), 3.4 - 0.07, 3.4 + 0.07, z0 + 0.005, z1 + 0.005, seg=56)
    ub.flat_paint(p, 'Yellow', [(-0.09, y0 + t), (0.09, y0 + t), (0.09, 3.0), (-0.09, 3.0)], z0 + 0.008, z1 + 0.008)
    ub.flat_paint(p, 'Paint', [(-1.6, -4.55), (1.6, -4.55), (1.6, -4.25), (-1.6, -4.25)], z0 + 0.01, z1 + 0.01)
    for s in (-1, 1):
        ub.flat_paint(p, 'Paint', [(s * 1.6 - 0.07, -0.6), (s * 1.6 + 0.07, -0.6), (s * 1.6 + 0.07, 1.3), (s * 1.6 + 0.2, 1.3),
                                   (s * 1.6, 1.7), (s * 1.6 - 0.2, 1.3), (s * 1.6 - 0.07, 1.3)][::(1 if s > 0 else 1)], z0, z1)
    ub.flat_paint(p, 'Player', [(-1.8, 3.6), (1.8, 3.6), (1.8, 5.0), (-1.8, 5.0)], z0, z1)
    ub.flat_paint(p, 'Dark', [(-1.6, 3.8), (1.6, 3.8), (1.6, 4.8), (-1.6, 4.8)], z0 + 0.005, z1 + 0.005)
    ub.flat_paint(p, 'Player', [(-1.3, 4.0), (1.3, 4.0), (1.3, 4.6), (-1.3, 4.6)], z0 + 0.01, z1 + 0.01)
    for sx in (-1, 1):
        for sy in (-1, 1):
            ub.runway_light(p, sx * (w / 2 - 0.45), sy * (h / 2 - 0.45), FLOOR - 0.005)
    return ub.finish(p)


def write_spec():
    import json
    ra = [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]
    parts = [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', ra]] + [[PREFIX + '_Pad', [x, -1.5, 0]] for x in (-14.4, -4.8, 4.8, 14.4)]
    for n in ('Body', 'Gear', 'Missiles'):
        for x in (-14.4, 4.8):
            parts.append(['captures/game/models/usa_raptor_v2/SM_USA_Raptor_' + n, [x, -1.5, 0], -90.0])
    for n in ('Body', 'Pods', 'Missiles'):
        parts.append(['captures/game/models/usa_comanche_v2/SM_USA_Comanche_' + n, [-12.0, -11.8, 0]])
    parts += [['captures/game/models/usa_comanche_v2/SM_USA_Comanche_Rotor', [-12.0, -11.8, 2.146]],
              ['captures/game/models/usa_comanche_v2/SM_USA_Comanche_TailFan', [-17.8, -11.603, 2.059]]]
    spec = {'parts': parts,
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 700]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [5.0, -4.0, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    r = bake_in_place(radar(), RADAR_AT)
    pd = bake_in_place(pad(), (PADS[2][0], PADS[2][1], 0.0))
    total = 0
    for o in (b, r, pd):
        total += export(o, OUT)
    x0, x1, hy0, hy1, hw, hr = HANGAR
    total += export(scaffold(PREFIX + '_Scaffold', x0, 18.0, hy0, hy1, hr), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, r, pd)})
    print('MODEL lote %.0f x %.0f m; radar spin at_m' % (2 * HX, 2 * HY), [round(c, 2) for c in RADAR_AT], 'rpm 10')
    print('MODEL pads_m', PADS, '; runways_m', [[RUNWAY_HEAD, (RUNWAY[0] + RUNWAY[1]) / 2, RUNWAY_END, (RUNWAY[0] + RUNWAY[1]) / 2]],
          '; helipad_m', list(HELIPAD))
    write_spec()


main()
