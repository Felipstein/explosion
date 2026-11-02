"""O caminhão de suprimento da China (6×6, o coletor) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_supply_truck.py -- C:/Users/luisf/explosion/captures/game/models/china_supply_truck

Fictício inspirado nos caminhões Shaanxi 6×6 do ELP (a cabine avançada chata, a grade grande com a estrela, os pneus grandes),
não cópia; a estrutura e as vagas da carga são as do caminhão da USA (Game/Tools/model_usa_supply_truck.py). A palavra: a
CAÇAMBA COM AS 4 CAIXAS. De cima: a cabine curta e baixa na frente, o vão com o estepe e a lona dobrada, a caçamba aberta com
as 4 caixas em 2 × 2. Da família da China: o oliva escuro com a camuflagem digital, a estrela vermelha na grade, o aço escuro.
Medidas (apoio, ~67% do tanque USA, que tem 8,1 m de casco e a torre a 2,48 m):
  - 5,4 × 2,36 m (nos pneus), a cabine baixa com o teto a 2,02 m, a caçamba com o assoalho a 1,05 m e as laterais a 1,52 m;
  - 6 rodas de 0,94 m (pneus de 0,36 m), os eixos em x 1,9 / -0,9 / -2,0, a bitola de 2,0 m;
  - a carga: 4 caixas pequenas (SM_USA_SupplyCrate_Small, 1,08 × 0,94 × 0,72 m, a mesma da USA) nas vagas SLOTS (cargo.slots_m,
    o centro da base da caixa no assoalho).
Cor do jogador (slot Team): a estrela vermelha na grade (troca com a cor do jogador), a lona DOBRADA no suporte atrás da cabine e
as estrelas pequenas nas portas.

Peças (origem no chão, no centro do chassi, frente em +X):
  SM_China_SupplyTruck_Hull    tudo que não gira (cabine, chassi, caçamba, estepe, lona, para-lamas);
  SM_China_SupplyTruck_Wheels  6 malhas wheel_001..006 (uma por roda, no lugar dela: o jogo gira cada uma pelo centro da caixa; o
                               lado de dentro da caixa é a borracha, o cubo e o aro saem pra fora).
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Rubber, Team, Lens, Dark, Cloth, Light.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, bounds, camo_faces, camo_poly, hatch, headlight, solid, star, tarp_fold,  # noqa: E402
                                     tow_hook, tris, write_preview)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_supply_truck')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_SupplyTruck'

# ---------------------------------------------------------------------------------- medidas
FRONT, REAR = 2.72, -2.68
WHEEL_R, WHEEL_W, WHEEL_Y = 0.47, 0.36, 1.0
AXLES_X = [1.9, -0.9, -2.0]
FRAME_Z = (0.62, 0.84)
BED_Z = 1.05                     # o assoalho da caçamba (= z das vagas)
BED_X = (-2.62, -0.02)           # a caçamba, da tampa de trás à cabeceira
BED_HW = 1.1
SIDE_H = 0.47
CAB_X = (1.12, 2.58)
CAB_HW = 1.12
CAB_Z = (0.92, 1.95)
SLOTS = [(-0.72, -0.5, BED_Z), (-0.72, 0.5, BED_Z), (-1.88, -0.5, BED_Z), (-1.88, 0.5, BED_Z)]


def bar(p, a, b, r=0.02, m='CnTankMetal', seg=8):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=seg, bevel=0.0)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('CnTankPaint#shell', 0.06)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.006)
    # ---------------------------------------------------------------- o chassi: as 2 longarinas, as travessas, os eixos, os feixes
    for y in (-0.48, 0.48):
        p.box('Dark', REAR + 0.08, FRONT - 0.2, y - 0.08, y + 0.08, FRAME_Z[0], FRAME_Z[1], bevel=0.015)
    for x in (-2.45, -1.45, -0.4, 0.6, 1.4, 2.3):
        p.box('Dark', x - 0.05, x + 0.05, -0.56, 0.56, FRAME_Z[0] + 0.03, FRAME_Z[1] - 0.02, bevel=0.0)
    for x in AXLES_X:
        p.cyl('Dark', (x, 0.0, WHEEL_R), 'y', 0.09, 2 * WHEEL_Y - WHEEL_W, seg=12, bevel=0.0)         # o eixo
        p.box('Dark', x - 0.18, x + 0.18, -0.15, 0.15, WHEEL_R - 0.14, WHEEL_R + 0.14, bevel=0.01)   # o diferencial
        for s in (-1, 1):
            p.box('CnTankMetal', x - 0.5, x + 0.5, 0.48 * s - 0.06, 0.48 * s + 0.06, WHEEL_R + 0.1, WHEEL_R + 0.17, bevel=0.0)   # o feixe
    # o tanque de combustível (esquerda) e a caixa das baterias (direita), entre a cabine e as rodas de trás
    # (31/10, o detalhe AAA) o tanque torneado (as bordas dobradas), as 2 cintas de aço com o esticador, a tampa do bocal
    vp.lathe(p, 'CnTankPaint', (0.3, -0.82, 0.78), 'x', [(0.0, -0.5), (0.2, -0.5), (0.235, -0.49), (0.245, -0.46), (0.245, 0.46),
                                                       (0.235, 0.49), (0.2, 0.5), (0.0, 0.5)], seg=22)
    for dx in (-0.3, 0.3):
        vp.lathe(p, 'CnTankMetal', (0.3 + dx, -0.82, 0.78), 'x', [(0.24, -0.025), (0.258, -0.025), (0.258, 0.025), (0.24, 0.025)],
                 seg=22, closed=True)
        p.box('CnTankMetal#vp', 0.3 + dx - 0.03, 0.3 + dx + 0.03, -0.66, -0.56, 0.62, 1.0, bevel=0.0)
    vp.lathe(p, 'CnTankMetal', (0.62, -0.82, 1.02), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.04), (0.05, 0.05), (0.0, 0.05)], seg=12)
    vp.rbox(p, 'CnTankPaint', (0.35, 0.81, 0.77), (0.8, 0.38, 0.38), r=0.03, k=2)
    vp.rbox(p, 'CnTankPaint', (0.35, 0.81, 0.97), (0.84, 0.42, 0.04), r=0.035, k=2)
    for dx in (-0.25, 0.25):
        vp.rbox(p, 'CnTankMetal', (0.35 + dx, 1.005, 0.9), (0.05, 0.02, 0.07), r=0.008, k=1)
    # ---------------------------------------------------------------- a cabine avançada (chata, baixa), a grade com a estrela
    cx0, cx1 = CAB_X
    z0, z1 = CAB_Z
    fc = solid(p, 'CnTankPaint#shell', [(cx0, s * CAB_HW, z) for s in (-1, 1) for z in (z0, z1)] +
               [(cx1, s * CAB_HW, z0) for s in (-1, 1)] + [(cx1 + 0.04, s * CAB_HW, 1.36) for s in (-1, 1)] +
               [(cx1 - 0.18, s * (CAB_HW - 0.04), z1) for s in (-1, 1)])
    camo_faces(p, fc, seed=51, skip=lambda c, n: (n.x > 0.5) or (abs(n.y) > 0.9) or n.z > 0.9)
    # a camuflagem dos lados da cabine fica só no painel de baixo (o vidro da porta em cima)
    for s in (-1, 1):
        y = CAB_HW * s
        camo_poly(p, [(cx0 + 0.05, y, z0 + 0.05), (cx1 - 0.05, y, z0 + 0.05), (cx1 - 0.05, y, 1.3), (cx0 + 0.05, y, 1.3)], (0, s, 0),
                  seed=51, inset=0.03)
    # o teto: a aba e a escotilha
    p.box('CnTankPaint', cx0 - 0.04, cx1 - 0.14, -CAB_HW - 0.03, CAB_HW + 0.03, z1, z1 + 0.06, bevel=0.02)
    camo_poly(p, [(cx0 - 0.02, -CAB_HW, z1 + 0.06), (cx1 - 0.16, -CAB_HW, z1 + 0.06), (cx1 - 0.16, CAB_HW, z1 + 0.06),
                  (cx0 - 0.02, CAB_HW, z1 + 0.06)], (0, 0, 1), seed=51, inset=0.04)
    p.obox('CnTankPaint', (cx1 - 0.08, 0.0, z1 - 0.01), (0.26, 2 * CAB_HW - 0.1, 0.035), pitch=8, bevel=0.01)   # a pala do para-brisa
    hatch(p, cx0 + 0.55, 0.35, z1 + 0.06, r=0.28, hinge_side=-1)
    # o para-brisa em 2 folhas inclinado (a face de cima da frente) e as janelas das portas
    ang = math.degrees(math.atan2(0.18 + 0.04, z1 - 1.36))
    for y in (-0.53, 0.53):
        p.obox('Lens', (cx1 - 0.075, y, 1.66), (0.02, 0.95, 0.5), pitch=-ang, bevel=0.0)
    p.obox('Dark', (cx1 - 0.08, 0.0, 1.66), (0.025, 0.1, 0.54), pitch=-ang, bevel=0.0)
    for y in (-0.53, 0.53):                                                        # (31/10) os limpadores
        p.obox('Dark#vp', (cx1 - 0.045, y - 0.12, 1.5), (0.02, 0.025, 0.42), pitch=-ang, roll=55, bevel=0.0)
    for s in (-1, 1):
        yo = (CAB_HW + 0.004) * s
        p.box('Lens', cx0 + 0.55, cx1 - 0.3, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.4, 1.84, bevel=0.0)
        p.box('Dark', cx0 + 0.5, cx0 + 0.53, min(yo, yo + 0.008 * s), max(yo, yo + 0.008 * s), z0 + 0.06, 1.88, bevel=0.0)   # a linha da porta
        p.box('CnTankMetal', cx0 + 0.6, cx0 + 0.72, min(yo, yo + 0.03 * s), max(yo, yo + 0.03 * s), 1.3, 1.34, bevel=0.0)    # a maçaneta
        star(p, ((cx0 + 0.55 + cx1 - 0.3) / 2, yo, 1.12), (0, s, 0), 0.13)                                          # a estrela na porta
        # o retrovisor, o degrau, o para-lama da frente com a aba
        yy0, yy1 = sorted(((CAB_HW + 0.02) * s, (CAB_HW + 0.2) * s))
        root = Vector((cx1 - 0.23, CAB_HW * s, 1.72))
        vp.tube(p, 'CnTankMetal', vp.bent_bar([root, root + Vector((0.0, 0.14 * s, 0.02)), root + Vector((-0.02, 0.18 * s, -0.06))], R=0.04, n=2),
                0.014, seg=6)
        vp.rbox(p, 'Dark', (cx1 - 0.25, (CAB_HW + 0.19) * s, 1.64), (0.05, 0.12, 0.3), yaw=-10 * s, r=0.03, k=2)
        vp.grab_handle(p, 'CnTankMetal', (cx0 + 0.06, CAB_HW * s, 1.0), (cx0 + 0.06, CAB_HW * s, 1.5), (0, s, 0), height=0.06, r=0.015)
        p.box('CnTankMetal', cx0 + 0.15, cx0 + 0.6, (CAB_HW - 0.05) * s - 0.12, (CAB_HW - 0.05) * s + 0.12, 0.55, 0.6, bevel=0.0)
        yy0, yy1 = sorted(((WHEEL_Y - WHEEL_W / 2 - 0.04) * s, (WHEEL_Y + WHEEL_W / 2 + 0.06) * s))
        p.box('CnTankPaint', AXLES_X[0] - 0.55, AXLES_X[0] + 0.55, yy0, yy1, WHEEL_R + 0.5, WHEEL_R + 0.55, bevel=0.01)
        p.obox('CnTankPaint', (AXLES_X[0] + 0.6, (WHEEL_Y + 0.01) * s, WHEEL_R + 0.38), (0.06, WHEEL_W + 0.1, 0.32), pitch=25, bevel=0.01)
    # a frente: a moldura da grade, as aletas, a ESTRELA vermelha no alto da grade, os faróis, o para-choque com o guincho
    gx = cx1 + 0.02
    p.box('CnTankPaint', gx - 0.02, gx + 0.05, -0.82, 0.82, 0.98, 1.34, bevel=0.015)
    p.box('Dark', gx + 0.05, gx + 0.06, -0.72, 0.72, 1.02, 1.22, bevel=0.0)
    for k in range(5):
        p.box('CnTankMetal', gx + 0.05, gx + 0.08, -0.7, 0.7, 1.04 + k * 0.038, 1.055 + k * 0.038, bevel=0.0)
    star(p, (gx + 0.05, 0.0, 1.28), (1, 0, 0), 0.1)
    p.box('CnTankPaint', gx + 0.05, gx + 0.06, -0.14, 0.14, 1.22, 1.33, bevel=0.0)
    for s in (-1, 1):
        headlight(p, (gx - 0.02, 0.9 * s, 1.12), r=0.085, guard=False, lit=True)
        p.box('Dark', gx - 0.02, gx + 0.05, 0.9 * s - 0.13, 0.9 * s + 0.13, 0.98, 1.26, bevel=0.01)
        p.box('Lens', gx + 0.05, gx + 0.06, 0.66 * s - 0.04, 0.66 * s + 0.04, 0.99, 1.02, bevel=0.0)       # a seta
    p.box('CnTankMetal', FRONT - 0.14, FRONT, -1.15, 1.15, 0.62, 0.86, bevel=0.03)                         # o para-choque
    vp.rbox(p, 'Dark', (FRONT - 0.01, 0.0, 0.74), (0.18, 0.56, 0.2), r=0.04, k=2)                          # o guincho
    vp.lathe(p, 'CnTankMetal', (FRONT + 0.04, 0.0, 0.74), 'y', [(0.0, -0.22), (0.1, -0.22), (0.1, -0.2), (0.075, -0.19), (0.075, 0.19),
                                                              (0.1, 0.2), (0.1, 0.22), (0.0, 0.22)], seg=16)
    for k in range(5):                                                               # o cabo enrolado no tambor
        p.cyl('Dark#vp', (FRONT + 0.04, -0.16 + k * 0.08, 0.74), 'y', 0.088, 0.06, seg=16, bevel=0.0)
    for s in (-1, 1):
        tow_hook(p, FRONT, 0.6 * s, 0.7, s=1)
    # ---------------------------------------------------------------- o vão atrás da cabine: o estepe em pé, o suporte com a lona
    p.box('CnTankMetal', 0.05, cx0, -0.6, 0.6, FRAME_Z[1], FRAME_Z[1] + 0.04, bevel=0.0)
    vp.tyre(p, (0.58, -0.45, 1.36), 0.47, 0.3, -1, rim_r=0.28, rubber='Rubber', rim='CnTankPaint', hub='CnTankMetal', seg=28,
            holes_m='Dark', axis='x')
    p.box('CnTankMetal', 0.5, 0.66, -0.5, -0.4, FRAME_Z[1], 1.05, bevel=0.0)
    # o suporte de barras atrás da cabine e a lona DOBRADA (cor do jogador) presa nele
    for y in (0.1, 0.95):
        bar(p, (0.62, y, FRAME_Z[1]), (0.62, y, 1.5), r=0.03)
    bar(p, (0.62, 0.1, 1.5), (0.62, 0.95, 1.5), r=0.03)
    p.box('CnTankMetal', 0.3, 0.95, 0.12, 0.98, 1.12, 1.16, bevel=0.0)
    tarp_fold(p, (0.62, 0.53, 1.16), (0.58, 0.82, 0.24), m='Team', straps=2, seed=13.0)
    antenna(p, cx0 - 0.08, 0.95, 1.6, 1.1)
    vp.lathe(p, 'Dark', (1.0, 1.06, 0.9), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 1.12), (0.07, 1.14), (0.07, 1.18), (0.05, 1.18),
                                               (0.05, 1.0), (0.0, 1.0)], seg=12)
    vp.lathe(p, 'CnTankMetal', (1.0, 1.06, 1.25), 'z', [(0.072, 0.0), (0.082, 0.0), (0.082, 0.5), (0.072, 0.5)], seg=14, closed=True)
    for k in range(4):
        p.box('Dark#vp', 0.992, 1.008, 1.06 + 0.075, 1.06 + 0.086, 1.3 + k * 0.11, 1.36 + k * 0.11, bevel=0.0)
    # ---------------------------------------------------------------- a caçamba aberta: o assoalho, as laterais baixas com as nervuras,
    # a cabeceira com a grade de proteção, a tampa de trás, os para-lamas das rodas de trás, as lanternas
    bx0, bx1 = BED_X
    p.box('CnTankPaint', bx0, bx1, -BED_HW, BED_HW, BED_Z - 0.18, BED_Z, bevel=0.02)
    for k in range(7):
        p.box('Dark', bx0 + 0.2 + k * 0.36, bx0 + 0.22 + k * 0.36, -BED_HW + 0.12, BED_HW - 0.12, BED_Z, BED_Z + 0.004, bevel=0.0)
    for s in (-1, 1):
        y0, y1 = sorted((BED_HW * s, (BED_HW - 0.06) * s))
        fs = solid(p, 'CnTankPaint', [(x, y, z) for x in (bx0, bx1) for y in (y0, y1) for z in (BED_Z, BED_Z + SIDE_H)], bevel=0.012)
        camo_faces(p, fs, seed=51, skip=lambda c, n: abs(n.y) < 0.9 or n.y * s < 0, inset=0.03)
        for x in (bx0 + 0.05, bx0 + 0.9, bx0 + 1.75, bx1 - 0.05):
            yy0, yy1 = sorted((BED_HW * s, (BED_HW + 0.035) * s))
            p.box('CnTankPaint', x - 0.04, x + 0.04, yy0, yy1, BED_Z - 0.16, BED_Z + SIDE_H + 0.02, bevel=0.0)
        yy0, yy1 = sorted((BED_HW * s, (BED_HW + 0.012) * s))
        p.box('CnTankMetal', bx0 + 0.3, bx1 - 0.3, yy0, yy1, BED_Z + SIDE_H - 0.08, BED_Z + SIDE_H - 0.04, bevel=0.0)   # o trilho das cordas
        # os para-lamas das rodas de trás (um só, comprido, por cima das duas)
        yy0, yy1 = sorted(((WHEEL_Y - WHEEL_W / 2 - 0.04) * s, (WHEEL_Y + WHEEL_W / 2 + 0.05) * s))
        p.box('CnTankPaint', AXLES_X[2] - 0.62, AXLES_X[1] + 0.62, yy0, yy1, BED_Z - 0.22, BED_Z - 0.18, bevel=0.01)
        for x in (AXLES_X[2] - 0.62, AXLES_X[1] + 0.62):
            p.box('Dark', x - 0.02, x + 0.02, yy0, yy1, WHEEL_R + 0.02, BED_Z - 0.2, bevel=0.0)   # a aba de borracha
        vp.mudflap(p, (AXLES_X[2] - 0.66, WHEEL_Y * s, BED_Z - 0.2), WHEEL_W + 0.08, 0.55, m='Rubber')
        # as lanternas de trás
        p.box('Dark', REAR - 0.04, REAR + 0.02, 0.85 * s - 0.12, 0.85 * s + 0.12, 0.66, 0.8, bevel=0.0)
        p.box('Lens', REAR - 0.05, REAR - 0.03, 0.85 * s - 0.09, 0.85 * s + 0.09, 0.69, 0.77, bevel=0.0)
    # a cabeceira alta com a grade (protege a cabine) e a tampa de trás
    p.box('CnTankPaint', bx1 - 0.06, bx1, -BED_HW, BED_HW, BED_Z, BED_Z + SIDE_H, bevel=0.012)
    for k in range(9):
        y = -BED_HW + 0.08 + k * (2 * BED_HW - 0.16) / 8
        bar(p, (bx1 - 0.03, y, BED_Z + SIDE_H), (bx1 - 0.03, y, BED_Z + SIDE_H + 0.45), r=0.018)
    bar(p, (bx1 - 0.03, -BED_HW, BED_Z + SIDE_H + 0.45), (bx1 - 0.03, BED_HW, BED_Z + SIDE_H + 0.45), r=0.026)
    fr = solid(p, 'CnTankPaint', [(x, y, z) for x in (bx0, bx0 + 0.06) for y in (-BED_HW, BED_HW) for z in (BED_Z, BED_Z + SIDE_H)], bevel=0.012)
    camo_faces(p, fr, seed=51, skip=lambda c, n: n.x > -0.9, inset=0.03)
    for y in (-0.7, 0.7):
        p.box('CnTankMetal', bx0 - 0.04, bx0, y - 0.05, y + 0.05, BED_Z + 0.25, BED_Z + 0.4, bevel=0.0)    # os trincos
    p.box('CnTankMetal', REAR - 0.08, REAR + 0.06, -1.05, 1.05, 0.6, 0.72, bevel=0.02)                       # o para-choque de trás
    p.box('Dark', REAR - 0.16, REAR, -0.12, 0.12, 0.6, 0.76, bevel=0.01)                                     # o engate
    return p.finish()


def wheels():
    """6 rodas (31/10, o detalhe AAA: model_vehicle_parts.tyre): o pneu de verdade (o flanco abaulado, os blocos em V subindo no
    ombro), o aro de aço rebaixado com os furos, o cubo saliente e o anel de porcas. Os blocos são múltiplos de 4 e a roda é
    simétrica no giro: a caixa continua centrada no eixo (o jogo gira cada roda pelo centro da caixa)."""
    objs = []
    n = 0
    for x in AXLES_X:
        for s in (-1, 1):
            n += 1
            w = Part(f'wheel_{n:03d}')
            vp.tyre(w, (x, WHEEL_Y * s, WHEEL_R), WHEEL_R, WHEEL_W, s, rim_r=0.29, rubber='Rubber', rim='CnTankPaint', hub='CnTankMetal',
                    seg=32, holes_m='Dark')
            objs.append(w.finish())
    return objs


def main():
    clear_scene()
    h = hull()
    total = export(h, OUT)
    ws = wheels()
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h] + ws)
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), largura %.2f m (nos pneus), altura %.2f m (sem a antena: teto %.2f)'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1, CAB_Z[1] + 0.06))
    print('MODEL cargo.slots_m', [list(sl) for sl in SLOTS], '(o centro da base da caixa; a caixa SM_USA_SupplyCrate_Small)')
    print('MODEL pneu: raio %.2f, largura %.2f; eixos x %s; centro da roda y +-%.2f (face de dentro +-%.2f); tyre_w_m %.2f'
          % (WHEEL_R, WHEEL_W, AXLES_X, WHEEL_Y, WHEEL_Y - WHEEL_W / 2, WHEEL_W))
    print('MODEL tris por peca', {h.name: tris(h), 'wheels': sum(tris(w) for w in ws)})
    crate = 'C:/Users/luisf/explosion/captures/game/models/usa_supply_truck/SM_USA_SupplyCrate_Small'
    write_preview(OUT, [(PREFIX + '_Hull', (0, 0, 0)), (PREFIX + '_Wheels', (0, 0, 0))] +
                  [(crate, (x, -y, z)) for (x, y, z) in SLOTS],
                  extra={'tank_views': False, 'usa_at': [0, 6.0, 0]})


main()
