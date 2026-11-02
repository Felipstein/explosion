"""O Alojamento da Rússia (Barracks, a Казарма) provisório, modelado no Blender por script (a facção nova, 01/11/2026:
docs/FACCAO_RUSSIA.md §5.1: "o bloco de alojamento de painel com o pátio de formatura; o pátio com o mastro e a placa").

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_barracks.py C:/Users/luisf/explosion/captures/game/models/russia_barracks

A planta: a caserna de painel das unidades do exército russo (o bloco comprido de 2 andares com o telhado baixo de chapa verde, o corpo
da escada saliente no meio com o oitão e a estrela) e o PLAÇ (o pátio de formatura): o piso de lajes com as linhas brancas de formação, a
tribuna vermelha com a estrela, o mastro alto da bandeira, o quadro de avisos, a barra fixa e as paralelas, o cogumelo da sentinela.

A palavra (o que lê de cima): O PÁTIO DE FORMATURA COM O MASTRO na frente do bloco comprido de telhado verde. De cima, o retângulo do piso
com o quadriculado branco das formações, o mastro e a tribuna vermelha; de frente, o oitão da escada com a estrela contornada.

Peça por peça:
- o bloco (5,8 × 18,8 m, 2 andares, beiral a 6,0 m, cumeeira a 7,4 m): oco, painéis de 3 m com uma janela de caixilho branco em cada, as
  juntas fundas escuras, o soco, as lajes dos andares e os pilares por dentro; o telhado de duas águas de chapa verde (a cumeeira em Y);
- o corpo da escada saliente no meio da frente: a porta de 2 folhas com a marquise (por onde a infantaria sai), a fita vertical de
  janelas da escada, o oitão de painel com a ESTRELA VERMELHA CONTORNADA, o telhadinho de chapa;
- o plaç: as lajes, as linhas brancas (o contorno, as faixas de formação), a tribuna (o pódio de concreto com a frente vermelha, a estrela,
  o guarda-corpo, a escada), o mastro de 9 m com a bandeira da cor do jogador, o quadro de avisos vermelho com o letreiro estêncil;
- o canto da educação física (a barra fixa e as paralelas), as luminárias.

Lote 16 × 20 m (X × Y), frente em +X. Alturas: o bloco 7,4 m na cumeeira, o oitão da escada 8,1 m, o mastro 9,2 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_Barracks_Base: tudo (a bandeira sai no flags.json; o jogo desenha o pano no vento);
  SM_Russia_Barracks_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Wood, Rubber.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp  # noqa: E402
from model_building_parts import WallFace, wbox, tube, splits  # noqa: E402
from model_russia_parts import (ru_foundation, RuPart, ru_block, ru_star, stencil, sign_board, gable_roof, entry_door, prof_sheet, flag_pole,  # noqa: E402
                                sentry_mushroom, ac_box, face_prism, gable_end, flashing, stairs_x, roof_fan)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_barracks')
PREFIX = 'SM_Russia_Barracks'

HX, HY = 8.0, 10.0                  # o lote 16 × 20
Z0 = 0.14
FL = Z0 + 0.12
T = 0.35
BX0, BX1, BY0, BY1 = -7.4, -1.6, -9.4, 9.4
F2, EAVE, RISE = 3.34, 6.0, 1.4
STAIR = (-1.6, -0.4, -1.8, 1.8)     # o corpo da escada saliente (x0, x1, y0, y1)
DOOR = (-1.0, 1.0, FL + 0.3, FL + 2.6)


def block(p):
    """O bloco de painel de 2 andares (oco) com o telhado de duas águas de chapa verde (a cumeeira em Y)."""
    ent = (-1.6, 1.6, Z0, 3.04)
    ru_block(p, BX0, BX1, BY0, BY1, Z0, EAVE, t=T, pw=3.0, zs=[F2, EAVE], parapet=0.0, roof='slab', floors=[F2],
             holes={'+x': [ent]}, ukeep={'+x': [-1.6, 1.6]},
             win={'+x': dict(floors=[Z0, F2], skip=[(3, None)], lit=[(1, 1), (5, 0)]), '-x': dict(floors=[Z0, F2], lit=[(2, 1)]),
                  '-y': dict(floors=[Z0, F2]), '+y': dict(floors=[Z0, F2], lit=[(0, 0)])},
             cols=[(-4.5, y) for y in (-6.0, -3.0, 3.0, 6.0)], step=3.0, along='y')
    gable_roof(p, BX0, BX1, BY0, BY1, EAVE, RISE, along='y', over=0.35, eave=0.38, t=T, gutter=True)
    # as luminárias nas pontas e o condensador encostado no chão
    for y in (-6.3, 6.3):
        lamp(p, BX1 + 0.08, y, 2.9, face='+x')


def stair(p):
    """O corpo da escada saliente no meio da frente: a porta de 2 folhas com a marquise, a fita vertical de janelas da escada, o oitão
    de painel com a estrela contornada, o telhadinho de duas águas de chapa (a cumeeira em X)."""
    x0, x1, y0, y1 = STAIR
    zt = EAVE
    ru_block(p, x0, x1, y0, y1, Z0, zt, t=T, walls=('+x', '+y', '-y'), skin=('+x', '+y', '-y'), pw=1.8, zs=[F2, zt], parapet=0.0,
             roof='slab', interior=False, holes={'+x': [DOOR, (-0.75, 0.75, 3.75, 5.5)]}, ukeep={'+x': [-1.0, 1.0]})
    F = WallFace('+x', x1, 0.0)
    entry_door(p, F, DOOR[0], DOOR[1], DOOR[2], DOOR[3], t=T, canopy=1.5)
    # a fita da escada (o vidro alto com as travessas e os patamares que aparecem)
    wbox(p, 'Glass', F, -0.75, 0.75, 3.75, 5.5, -0.2, -0.18, bevel=0.0)
    for u in (-0.75, 0.0, 0.75):
        wbox(p, 'White#win', F, u - 0.05, u + 0.05, 3.72, 5.53, -0.24, -0.14, bevel=0.012)
    for z in splits(3.75, 5.5, 0.58):
        wbox(p, 'White#win', F, -0.75, 0.75, z - 0.04, z + 0.04, -0.24, -0.14, bevel=0.012)
    p.box('Concrete#int', x0 - 1.0, x1 - 0.4, -0.9, 0.9, 4.6, 4.76, bevel=0.0)
    p.box('Light', x1 - 0.8, x1 - 0.5, -0.25, 0.25, 5.4, 5.44, bevel=0.0)
    # o oitão (o triângulo de painel em cima do corpo) com a estrela, e o telhadinho de chapa que encosta no telhado grande
    rise = EAVE + RISE - zt                       # a cumeeira do corpo encontra a do telhado grande (o telhadinho fecha nele)
    xr = (BX0 + BX1) / 2
    gable_end(p, F, y0, y1, zt, rise, t=T)
    zc = zt + 0.5
    ru_star(p, F, 0.0, zc, 0.36, n=0.08, t=0.025, w=0.11)
    for s in (-1, 1):
        P0 = Vector((x1 + 0.35, 0.0, zt + rise + 0.03))
        Pa = Vector((x1 + 0.35, s * (y1 - y0) / 2 * 1.2, zt - rise * 0.2 + 0.03))
        Pb = Vector((xr, 0.0, zt + rise + 0.03))
        prof_sheet(p, 'Roof', P0, Pa, Pb, drip=0.05)
    flashing(p, 'Paint#ridge', [(x1 + 0.4, 0.0, zt + rise + 0.07), (xr, 0.0, zt + rise + 0.07)], w=0.5, h=0.12)
    for s in (-1, 1):
        tube(p, 'Paint#ridge', [(x1 + 0.35, 0.0, zt + rise + 0.05), (x1 + 0.35, s * (y1 - y0) / 2 * 1.2, zt - rise * 0.2 + 0.05)], 0.04, seg=6)
    # o oitão também atrás do corpo (fecha o telhadinho contra o telhado grande) e o letreiro em cima da porta
    stencil(p, F, 0.0, DOOR[3] + 0.75, 'ЖЦ-6', h=0.32, m='White', n=0.08)


def parade(p):
    """O plaç: as lajes, as linhas brancas de formação, a tribuna com a estrela, o mastro da bandeira, o quadro de avisos."""
    px0, px1, py0, py1 = -0.1, 7.7, -7.6, 7.6
    xs = splits(px0, px1, 2.6)
    ys = splits(py0, py1, 2.53)
    g = 0.02
    for (xa, xb) in zip(xs[:-1], xs[1:]):
        for (ya, yb) in zip(ys[:-1], ys[1:]):
            p.box('Concrete#slab', xa + g, xb - g, ya + g, yb - g, Z0, FL, bevel=0.04)
    # as linhas brancas: o contorno e os quadrados de formação (as 3 fileiras de quadros pros pelotões)
    zl = FL + 0.002
    for (a, b) in (((px0 + 0.4, py0 + 0.4), (px1 - 0.4, py0 + 0.4)), ((px0 + 0.4, py1 - 0.4), (px1 - 0.4, py1 - 0.4))):
        p.box('White#line', a[0], b[0], a[1] - 0.06, a[1] + 0.06, zl - 0.01, zl + 0.008, bevel=0.0)
    for x in (px0 + 0.4, px1 - 0.4):
        p.box('White#line', x - 0.06, x + 0.06, py0 + 0.4, py1 - 0.4, zl - 0.01, zl + 0.008, bevel=0.0)
    for x in (1.4, 3.4, 5.4):
        for y in splits(-5.6, 5.6, 1.4):
            p.box('White#line', x - 0.35, x + 0.35, y - 0.05, y + 0.05, zl - 0.01, zl + 0.008, bevel=0.0)
            p.box('White#line', x - 0.05, x + 0.05, y - 0.35, y + 0.35, zl - 0.01, zl + 0.008, bevel=0.0)
    # a tribuna (+Y): o pódio de concreto, a frente vermelha virada pro plaç (−Y) com a estrela, o guarda-corpo, a escada de trás
    tx0, tx1, ty0, ty1 = 2.4, 5.6, 8.2, 9.7
    th = 1.25
    p.box('Concrete#trib', tx0, tx1, ty0, ty1, Z0, Z0 + th, bevel=0.06)
    Ft = WallFace('-y', ty0, 0.0)
    wbox(p, 'Red#trib', Ft, tx0 + 0.15, tx1 - 0.15, Z0 + 0.15, Z0 + th - 0.12, 0.0, 0.04, bevel=0.0)
    ru_star(p, Ft, (tx0 + tx1) / 2, Z0 + th * 0.52, 0.34, n=0.04, t=0.02, w=0.1)
    for (a, b) in (((tx0 + 0.1, ty0 + 0.1), (tx1 - 0.1, ty0 + 0.1)), ((tx0 + 0.1, ty0 + 0.1), (tx0 + 0.1, ty1 - 0.6)),
                   ((tx1 - 0.1, ty0 + 0.1), (tx1 - 0.1, ty1 - 0.6))):
        for k in range(int(math.dist(a, b) / 0.8) + 1):
            t = k / int(math.dist(a, b) / 0.8)
            p.cyl('Paint#rail', (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, Z0 + th + 0.5), 'z', 0.03, 1.0, seg=6, bevel=0.0)
        tube(p, 'Paint#rail', [(a[0], a[1], Z0 + th + 1.0), (b[0], b[1], Z0 + th + 1.0)], 0.035, seg=6)
    stairs_x(p, 'Concrete#step', tx1 - 0.02, tx1 + 1.1, ty0 + 0.3, ty0 + 1.1, Z0, Z0 + th - 0.02, 4)                # a escada do lado
    # o mastro da bandeira ao lado da tribuna
    flag_pole(p, 6.6, 8.6, Z0, 9.0, cloth=(2.0, 1.3), phase=0.2)
    # o quadro de avisos (o стенд) do lado −Y, virado pro plaç (+Y)
    for x in (2.0, 4.6):
        p.cyl('Paint#post', (x, -8.6, Z0 + 1.1), 'z', 0.05, 2.2, seg=8, bevel=0.0)
    sign_board(p, WallFace('+y', -8.55, 0.0), 3.3, Z0 + 1.15, 'ДЖЛЫ', h=0.38, board='Red', ink='White', pad=0.18)
    ru_star(p, WallFace('+y', -8.48, 0.0), 3.3, Z0 + 1.95, 0.18, n=0.0, t=0.012, w=0.12)


def sports(p):
    """O canto da educação física (a barra fixa e as paralelas)."""
    bx = 6.6
    for y in (-9.4, -7.9):
        p.cyl('Paint#gym', (bx, y, Z0 + 1.25), 'z', 0.06, 2.5, seg=10, bevel=0.0)
    tube(p, 'Metal#gym', [(bx, -9.4, Z0 + 2.35), (bx, -7.9, Z0 + 2.35)], 0.035, seg=8)
    for y in (-9.6, -8.9):
        for x in (4.6, 5.9):
            p.cyl('Paint#gym', (x, y, Z0 + 0.6), 'z', 0.05, 1.2, seg=8, bevel=0.0)
    for y in (-9.6, -8.9):
        tube(p, 'Metal#gym', [(4.4, y, Z0 + 1.2), (6.1, y, Z0 + 1.2)], 0.035, seg=8)


def roof_z(x):
    """A altura da chapa do telhado grande em x (a cumeeira em Y no meio do bloco, as águas caindo até o pé da parede)."""
    xm, hw = (BX0 + BX1) / 2, (BX1 - BX0) / 2
    return EAVE + RISE - RISE * abs(x - xm) / hw + 0.05


def roof_kit(p):
    """(01/11, o quartel 'liso de cima': o telhado verde é 40% do lote na câmera do jogo) o que o telhado de uma caserna russa tem: as
    chaminés de ventilação de concreto saindo perto da cumeeira, com o chapéu de chapa e as bocas escuras; os exaustores redondos na
    água da frente (a da câmera); o mastro da antena com as travessas e a parabólica pequena na água de trás."""
    xm = (BX0 + BX1) / 2
    for y in (-6.4, 5.6):
        x = xm - 0.75
        z0 = roof_z(x - 0.35) - 0.25          # o pé abaixo do ponto mais baixo da chapa embaixo dela
        p.box('Concrete#chim', x - 0.35, x + 0.35, y - 0.45, y + 0.45, z0, EAVE + RISE + 1.05, bevel=0.04)
        p.box('Concrete#chim', x - 0.42, x + 0.42, y - 0.52, y + 0.52, EAVE + RISE + 1.05, EAVE + RISE + 1.15, bevel=0.02)   # a cinta
        for dy in (-0.2, 0.2):
            p.box('Dark', x - 0.18, x + 0.18, y + dy - 0.13, y + dy + 0.13, EAVE + RISE + 1.13, EAVE + RISE + 1.16, bevel=0.0)  # as bocas
        for (dx, dyy) in ((-1, -1), (1, 1), (-1, 1), (1, -1)):
            p.box('Paint#cap', x + dx * 0.36 - 0.03, x + dx * 0.36 + 0.03, y + dyy * 0.46 - 0.03, y + dyy * 0.46 + 0.03,
                  EAVE + RISE + 1.15, EAVE + RISE + 1.42, bevel=0.0)
        p.box('Paint#cap', x - 0.5, x + 0.5, y - 0.6, y + 0.6, EAVE + RISE + 1.42, EAVE + RISE + 1.48, bevel=0.0)              # o chapéu
    # os exaustores redondos na água da frente (+X, a da câmera)
    for y in (-3.6, 3.2, 7.6):
        x = xm + 1.5
        roof_fan(p, x, y, roof_z(x) + 0.12, r=0.34)
    # o mastro da antena na cumeeira, perto da ponta −Y, com as travessas (a antena de TV e rádio do alojamento) e os estais
    ax, ay, az = xm, -8.2, EAVE + RISE + 0.05
    p.cyl('Metal#ant', (ax, ay, az + 1.6), 'z', 0.04, 3.2, seg=8, bevel=0.0)
    for k, w in enumerate((1.4, 1.1, 0.8)):
        zz = az + 1.9 + 0.45 * k
        p.box('Metal#ant', ax - 0.02, ax + 0.02, ay - w / 2, ay + w / 2, zz - 0.02, zz + 0.02, bevel=0.0)
        for yy in splits(ay - w / 2, ay + w / 2, 0.22):
            p.box('Metal#ant', ax - 0.13, ax + 0.13, yy - 0.012, yy + 0.012, zz - 0.012, zz + 0.012, bevel=0.0)
    for (dx, dy) in ((1.2, 0.0), (-1.2, 0.0), (0.0, 1.4)):
        tube(p, 'Metal#ant', [(ax, ay, az + 2.6), (ax + dx, ay + dy, roof_z(ax + dx) + 0.02)], 0.008, seg=4)
    # a parabólica pequena na água de trás (−X), olhando pro céu do sul
    dx_, dy_ = xm - 1.6, -2.4
    dz = roof_z(dx_)
    p.cyl('Metal#ant', (dx_, dy_, dz + 0.3), 'z', 0.03, 0.6, seg=6, bevel=0.0)
    p.cyl('White#dish', (dx_, dy_, dz + 0.68), 'z', 0.1, 0.14, seg=24, r2=0.5, bevel=0.0, smooth=True)   # a boca larga pra cima
    p.cyl('Metal#ant', (dx_, dy_, dz + 0.95), 'z', 0.025, 0.4, seg=6, bevel=0.0)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    block(p)
    roof_kit(p)
    stair(p)
    parade(p)
    sports(p)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, EAVE + RISE + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 20 m; porta da infantaria em X', STAIR[1], 'Y', DOOR[0], '..', DOOR[1], '; cumeeira', EAVE + RISE, 'm')
    print('MODEL fire_points_m', [(-4.5, -5.5, EAVE + RISE + 0.1), (-4.5, 5.0, EAVE + RISE + 0.1), (-1.0, 0.0, EAVE + RISE + 0.05)])


if __name__ == '__main__':
    main()
