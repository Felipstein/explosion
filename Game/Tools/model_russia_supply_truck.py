"""O caminhão de suprimento da Rússia (o KamAZ 6×6) provisório, modelado no Blender por script (01/11/2026; docs/FACCAO_RUSSIA.md §5.2:
"o caminhão pesado: carrega o dobro e anda devagar").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_russia_supply_truck.py C:/Users/luisf/explosion/captures/game/models/russia_supply_truck

Fictício inspirado no KamAZ-5350 (o Mustang 6×6 do exército), não cópia. A palavra: a CABINE ALTA E A CARROCERIA ABERTA COM AS 5 CAIXAS.
De cima: a cabine quadrada e alta na frente, o estepe em pé e o snorkel atrás dela, a carroceria comprida com os 3 arcos da lona e a
lona da cor do jogador ENROLADA na frente (aberta pra carga: as caixas aparecem de cima), as 5 caixas em 2 + 2 + 1.
Medidas (apoio, na escala dos caminhões do jogo: ~0,75 do KamAZ de verdade, que tem 8 m; maior que o da China, 5,4 m, porque
carrega 5 caixas e não 4): 6,0 × 2,36 m, a cabine com o teto a 2,35 m, a carroceria com o piso a 1,22 m e as laterais a 1,72 m, os
arcos a 2,6 m; 6 rodas de 0,92 m (pneus de 0,36 m), o eixo da frente em x 2,05 e o tandem em −0,9 / −2,25, a bitola de 2,0 m.
A carga: 5 caixas pequenas (SM_USA_SupplyCrate_Small, 1,08 × 0,94 × 0,72 m, a mesma da USA e da China) nas vagas SLOTS
(cargo.slots_m: o centro da base da caixa no piso).
Cor do jogador (slot Team): a lona enrolada na frente da carroceria, o rolo da lateral e a lona dobrada no teto da cabine.

Peças (origem no chão, no centro do chassi, frente em +X):
  SM_Russia_SupplyTruck_Hull    tudo que não gira;
  SM_Russia_SupplyTruck_Wheels  6 malhas wheel_001..006 (uma por roda, no lugar dela: o jogo gira cada uma pelo centro da caixa).
Slots: Paint, Metal, Team, Lens, Cloth, Rubber, Dark, Light.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import bounds, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_supply_truck')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_SupplyTruck'

FRONT, REAR = 3.0, -3.0
WHEEL_R, WHEEL_W, WHEEL_Y = 0.46, 0.36, 1.0
AXLES_X = [2.05, -0.9, -2.25]
FRAME_Z = (0.74, 0.94)
CAB_X1, CAB_D, CAB_HW, CAB_ZB, CAB_ZR = 2.92, 1.72, 1.15, 1.08, 2.35
BED_X = (-2.97, 0.78)            # a carroceria, da tampa de trás à cabeceira
BED_Z = 1.22                     # o piso (= z das vagas)
BED_HW = 1.15
SIDE_H = 0.5
BOW_Z = 2.6
SLOTS = [(0.16, -0.5, BED_Z), (0.16, 0.5, BED_Z), (-1.0, -0.5, BED_Z), (-1.0, 0.5, BED_Z), (-2.16, 0.0, BED_Z)]


def hull():
    p = rk.new_part(PREFIX + '_Hull')
    # ---------------------------------------------------------------- o chassi: as longarinas, as travessas, os eixos, os feixes
    for y in (-0.46, 0.46):
        vp.rbox(p, 'Metal', ((REAR + 0.06 + FRONT - 0.15) / 2, y, sum(FRAME_Z) / 2), (FRONT - 0.15 - REAR - 0.06, 0.12, FRAME_Z[1] - FRAME_Z[0]),
                r=0.02, k=1)
    for x in (-2.75, -1.6, -0.2, 0.7, 1.45, 2.6):
        p.box('Metal#vp', x - 0.04, x + 0.04, -0.52, 0.52, FRAME_Z[0] + 0.03, FRAME_Z[1] - 0.03, bevel=0.0)
    for x in AXLES_X:
        p.cyl('Dark#vp', (x, 0.0, WHEEL_R), 'y', 0.08, 2 * WHEEL_Y - WHEEL_W, seg=12, bevel=0.0)                # o eixo
        vp.lathe(p, 'Dark', (x, 0.0, WHEEL_R), 'x', [(0.0, -0.18), (0.12, -0.18), (0.17, -0.1), (0.17, 0.1), (0.12, 0.18), (0.0, 0.18)],
                 seg=16)                                                                                    # o diferencial
        for s in (-1, 1):
            for k in range(4):                                                                              # o feixe de molas
                L = 1.1 - k * 0.18
                vp.rbox(p, 'Metal', (x, 0.46 * s, WHEEL_R + 0.12 + k * 0.03), (L, 0.09, 0.025), r=0.01, k=1)
    # o balancim do tandem (a viga entre os 2 eixos de trás)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', ((AXLES_X[1] + AXLES_X[2]) / 2, 0.62 * s, WHEEL_R + 0.12), (1.5, 0.1, 0.12), r=0.03, k=2)
        p.cyl('Metal#vp', ((AXLES_X[1] + AXLES_X[2]) / 2, 0.62 * s, WHEEL_R + 0.12), 'y', 0.09, 0.16, seg=14, bevel=0.0)
    # o tanque de combustível (esquerda) com as cintas e o bocal; a caixa das baterias e os tanques de ar (direita)
    vp.rbox(p, 'Paint', (0.55, -0.92, 0.78), (1.1, 0.42, 0.44), roll=90, r=0.12, k=3)
    for dx in (-0.32, 0.32):
        vp.rbox(p, 'Metal', (0.55 + dx, -0.92, 0.78), (0.05, 0.45, 0.47), roll=90, r=0.13, k=3)
    vp.lathe(p, 'Metal', (0.25, -0.92, 1.0), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.04), (0.05, 0.05), (0.0, 0.05)], seg=12)
    vp.rbox(p, 'Paint#plate', (0.75, 0.9, 0.82), (0.7, 0.36, 0.38), r=0.03, k=2)
    vp.rbox(p, 'Paint#plate', (0.75, 0.9, 1.02), (0.74, 0.4, 0.035), r=0.035, k=2)
    for dx in (-0.2, 0.2):
        vp.rbox(p, 'Metal', (0.75 + dx, 1.09, 0.95), (0.05, 0.02, 0.07), r=0.008, k=1)
    for z in (0.62,):
        vp.lathe(p, 'Paint', (0.1, 0.86, z), 'x', [(0.0, -0.3), (0.1, -0.3), (0.12, -0.27), (0.12, 0.27), (0.1, 0.3), (0.0, 0.3)], seg=16)
    # ---------------------------------------------------------------- a cabine (a do KamAZ, no kit)
    rk.kamaz_cab(p, CAB_X1, CAB_HW, CAB_ZB, CAB_ZR, depth=CAB_D)
    # os para-lamas da frente (o arco por cima da roda da frente, preso embaixo da cabine) com a aba de borracha
    for s in (-1, 1):
        y0, y1 = sorted(((WHEEL_Y - WHEEL_W / 2 - 0.04) * s, (WHEEL_Y + WHEEL_W / 2 + 0.06) * s))
        rk.arc_fender(p, 'Paint#vp', AXLES_X[0], WHEEL_R, y0, y1, WHEEL_R + 0.1, a0=20, a1=160, t=0.03)
        vp.mudflap(p, (AXLES_X[0] - 0.6, WHEEL_Y * s, WHEEL_R + 0.2), WHEEL_W + 0.06, 0.42, m='Rubber')
    # a lona dobrada da cor do jogador no teto da cabine (presa pelas cintas no bagageiro de tubo)
    cx0 = CAB_X1 - CAB_D
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(cx0 + 0.15, 0.95 * s, CAB_ZR), (cx0 + 0.15, 0.95 * s, CAB_ZR + 0.08), (cx0 + 0.95, 0.95 * s, CAB_ZR + 0.08),
                             (cx0 + 0.95, 0.95 * s, CAB_ZR)], 0.018, seg=6)
    vp.tarp_bundle(p, (cx0 + 0.55, 0.38, CAB_ZR + 0.01), (0.7, 0.9, 0.16), m='Team', strap_m='Cloth', buckle_m='Metal', straps=2,
                   layers=2, seed=7.0)
    # ---------------------------------------------------------------- atrás da cabine: o estepe em pé, o snorkel, o escape
    vp.tyre(p, (1.0, 0.0, FRAME_Z[1] + WHEEL_R + 0.04), WHEEL_R, WHEEL_W, 1, rim_r=0.29, rubber='Rubber', rim='Paint', hub='Metal',
            seg=28, holes_m='Dark', axis='x', n_lugs=24)
    for y in (-0.42, 0.42):
        vp.rbox(p, 'Metal', (1.0, y, FRAME_Z[1] + 0.3), (0.08, 0.06, 0.6), r=0.02, k=1)
    # o snorkel do ar (o cano alto da direita com o chapéu) e o escape (esquerda, com a proteção de calor furada)
    vp.tube(p, 'Paint', vp.bent_bar([(1.08, 1.02, 0.95), (1.08, 1.02, 2.2), (1.08, 0.9, 2.45)], R=0.12, n=3), 0.08, seg=14)
    vp.lathe(p, 'Paint', (1.08, 0.88, 2.5), 'z', [(0.0, -0.06), (0.1, -0.06), (0.13, -0.02), (0.13, 0.06), (0.1, 0.09), (0.0, 0.1)], seg=16)
    vp.lathe(p, 'Dark', (1.06, -1.0, 0.95), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 1.35), (0.055, 1.38), (0.0, 1.38)], seg=12)
    vp.lathe(p, 'Metal', (1.06, -1.0, 1.4), 'z', [(0.08, 0.0), (0.09, 0.0), (0.09, 0.62), (0.08, 0.62)], seg=14, closed=True)
    # ---------------------------------------------------------------- a carroceria: o piso, as laterais de tábua de aço, a cabeceira com
    # a grade, a tampa de trás, os arcos da lona, a lona enrolada
    bx0, bx1 = BED_X
    vp.rbox(p, 'Paint#plate', ((bx0 + bx1) / 2, 0.0, BED_Z - 0.09), (bx1 - bx0, 2 * BED_HW, 0.18), r=0.03, k=2)
    for k in range(9):                                                                        # as travessas de baixo
        x = bx0 + 0.2 + k * (bx1 - bx0 - 0.4) / 8
        p.box('Metal#vp', x - 0.04, x + 0.04, -BED_HW + 0.05, BED_HW - 0.05, BED_Z - 0.3, BED_Z - 0.18, bevel=0.0)
    for k in range(11):                                                                       # as frestas do piso de tábua
        y = -BED_HW + 0.1 + k * (2 * BED_HW - 0.2) / 10
        p.box('Dark', bx0 + 0.06, bx1 - 0.06, y - 0.012, y + 0.012, BED_Z - 0.004, BED_Z + 0.002, bevel=0.0)
    for s in (-1, 1):
        # a lateral de 3 tábuas de aço com as nervuras, os postes (os bolsos das estacas) e os trincos
        for j in range(3):
            z0 = BED_Z + j * SIDE_H / 3
            vp.rbox(p, 'Paint#plate', ((bx0 + bx1) / 2, (BED_HW - 0.03) * s, z0 + SIDE_H / 6), (bx1 - bx0 - 0.02, SIDE_H / 3 - 0.02, 0.05),
                    roll=90, r=0.02, k=2)
        for x in (bx0 + 0.04, bx0 + 1.28, bx0 + 2.52, bx1 - 0.04):
            vp.rbox(p, 'Paint#plate', (x, (BED_HW + 0.0) * s, BED_Z + SIDE_H / 2 - 0.08), (0.1, 0.06, SIDE_H + 0.16), r=0.02, k=1)
        for x in (bx0 + 0.66, bx0 + 1.9, bx0 + 3.14):
            vp.rbox(p, 'Metal', (x, (BED_HW + 0.02) * s, BED_Z + SIDE_H - 0.08), (0.08, 0.03, 0.12), r=0.01, k=1)
        # o para-lama do tandem (a chapa reta por cima das 2 rodas) e a aba de borracha atrás
        y0, y1 = sorted(((WHEEL_Y - WHEEL_W / 2 - 0.05) * s, (WHEEL_Y + WHEEL_W / 2 + 0.06) * s))
        vp.rbox(p, 'Paint#thin', ((AXLES_X[1] + AXLES_X[2]) / 2, (y0 + y1) / 2, WHEEL_R + 0.58), (2.25, y1 - y0, 0.03), r=0.03, k=2)
        for x in (AXLES_X[1] + 0.62, AXLES_X[2] - 0.62):
            vp.rbox(p, 'Paint#thin', (x, (y0 + y1) / 2, WHEEL_R + 0.44), (0.03, y1 - y0, 0.3), r=0.02, k=1)
        vp.mudflap(p, (AXLES_X[2] - 0.66, WHEEL_Y * s, WHEEL_R + 0.3), WHEEL_W + 0.08, 0.6, m='Rubber')
        # as lanternas de trás (a caixa com a grade)
        vp.rbox(p, 'Dark', (REAR + 0.05, 0.86 * s, 0.82), (0.08, 0.3, 0.14), r=0.03, k=2)
        vp.rbox(p, 'Lens', (REAR + 0.005, 0.86 * s, 0.82), (0.012, 0.25, 0.1), r=0.03, k=2)
    # a cabeceira alta com a grade (protege a cabine) e a tampa de trás com os trincos e as dobradiças
    vp.rbox(p, 'Paint#plate', (bx1 - 0.03, 0.0, BED_Z + SIDE_H / 2), (0.06, 2 * BED_HW, SIDE_H), r=0.02, k=1)
    for k in range(9):
        y = -BED_HW + 0.1 + k * (2 * BED_HW - 0.2) / 8
        vp.tube(p, 'Metal', [(bx1 - 0.03, y, BED_Z + SIDE_H), (bx1 - 0.03, y, BED_Z + SIDE_H + 0.42)], 0.018, seg=6)
    vp.tube(p, 'Metal', [(bx1 - 0.03, -BED_HW, BED_Z + SIDE_H + 0.42), (bx1 - 0.03, BED_HW, BED_Z + SIDE_H + 0.42)], 0.026, seg=8)
    for j in range(3):
        vp.rbox(p, 'Paint#plate', (bx0 + 0.03, 0.0, BED_Z + j * SIDE_H / 3 + SIDE_H / 6), (0.05, 2 * BED_HW - 0.04, SIDE_H / 3 - 0.02),
                r=0.02, k=1)
    for y in (-0.72, 0.72):
        vp.rbox(p, 'Metal', (bx0 - 0.02, y, BED_Z + SIDE_H - 0.1), (0.04, 0.1, 0.14), r=0.01, k=1)
        vp.hinge(p, 'Metal', (bx0 - 0.02, y, BED_Z + 0.03), 'y', 0.3, r=0.022, knuckles=3)
    # os 3 arcos da lona (o U invertido de tubo), a cumeeira e as 2 réguas de cima
    bows = [bx1 - 0.18, (bx0 + bx1) / 2, bx0 + 0.16]
    for x in bows:
        vp.tube(p, 'Metal', vp.bent_bar([(x, -BED_HW + 0.02, BED_Z + SIDE_H - 0.05), (x, -BED_HW + 0.02, BOW_Z - 0.1),
                                         (x, -BED_HW + 0.4, BOW_Z), (x, BED_HW - 0.4, BOW_Z), (x, BED_HW - 0.02, BOW_Z - 0.1),
                                         (x, BED_HW - 0.02, BED_Z + SIDE_H - 0.05)], R=0.22, n=4), 0.03, seg=8)
    vp.tube(p, 'Metal', [(bows[0], 0.0, BOW_Z + 0.03), (bows[-1], 0.0, BOW_Z + 0.03)], 0.025, seg=8)
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(bows[0], (BED_HW - 0.02) * s, BOW_Z - 0.14), (bows[-1], (BED_HW - 0.02) * s, BOW_Z - 0.14)], 0.022, seg=8)
    # a lona ENROLADA (a cor do jogador) na frente, deitada no arco da frente e amarrada (a carga aberta pra cima), e o rolo da
    # lateral da direita preso na régua de cima
    vp.tarp_roll(p, (bows[0] + 0.02, 0.0, BOW_Z + 0.17), 'y', 2 * BED_HW - 0.7, 0.15, m='Team', straps=3, strap_m='Cloth',
                 buckle_m='Metal')
    vp.tarp_roll(p, ((bows[0] + bows[-1]) / 2, BED_HW + 0.06, BOW_Z - 0.26), 'x', bows[0] - bows[-1] - 0.3, 0.09, m='Team', straps=4,
                 strap_m='Cloth', buckle_m='Metal')
    for x in (bows[0] + 0.15, bows[0] - 0.12):
        vp.rope(p, 'Cloth', [(x, -BED_HW + 0.05, BOW_Z - 0.1), (x, -0.4, BOW_Z + 0.3), (x, 0.4, BOW_Z + 0.3), (x, BED_HW - 0.05, BOW_Z - 0.1)],
                r=0.012, sag=0.0)
    # o para-choque de trás com o engate e a placa lisa, as argolas de amarração
    vp.rbox(p, 'Metal', (REAR + 0.08, 0.0, 0.66), (0.12, 2.0, 0.12), r=0.03, k=2)
    vp.rbox(p, 'Dark', (REAR + 0.02, 0.0, 0.66), (0.14, 0.22, 0.2), r=0.03, k=2)
    vp.lathe(p, 'Metal', (REAR - 0.06, 0.0, 0.66), 'z', [(0.0, -0.06), (0.06, -0.06), (0.07, 0.0), (0.06, 0.06), (0.0, 0.06)], seg=12)
    vp.rbox(p, 'Paint#thin', (REAR + 0.01, -0.45, 0.86), (0.02, 0.5, 0.12), r=0.02, k=1)
    for x in (bx0 + 0.4, bx1 - 0.4):
        for s in (-1, 1):
            p.torus('Metal#vp', (x, (BED_HW - 0.12) * s, BED_Z + 0.03), 'x', 0.045, 0.012, seg=12, rseg=6)
    return p.finish()


def wheels():
    return rk.tyre_wheels('wheel_%03d', AXLES_X, WHEEL_Y, WHEEL_R, WHEEL_W, 0.29, style='mud')


def main():
    clear_scene()
    h = hull()
    total = export(h, OUT)
    ws = wheels()
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h] + ws)
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), largura %.2f m, altura %.2f m (os arcos %.2f, a cabine %.2f)'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, BOW_Z, CAB_ZR))
    print('MODEL cargo.slots_m', [list(sl) for sl in SLOTS], '(o centro da base da caixa; SM_USA_SupplyCrate_Small)')
    print('MODEL pneu: raio %.2f, largura %.2f (tyre_w_m); eixos x %s; o centro da roda em y +-%.2f' % (WHEEL_R, WHEEL_W, AXLES_X, WHEEL_Y))
    print('MODEL tris por peca', {h.name: tris(h), 'wheels': sum(tris(w) for w in ws)})
    crate = 'C:/Users/luisf/explosion/captures/game/models/usa_supply_truck/SM_USA_SupplyCrate_Small'
    rk.write_spec(OUT, [(PREFIX + '_Hull', (0, 0, 0)), (PREFIX + '_Wheels', (0, 0, 0))] + [(crate, (x, -y, z)) for (x, y, z) in SLOTS],
                  usa_at=(0, 6.0, 0))


main()
