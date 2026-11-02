"""O Alojamento da Rússia (Barracks, a Казарма), v2 (01/11/2026): pelo veredito da galeria ("no rumo, mude algumas coisas, parece um
prédio normal"). O v1 (model_russia_barracks.py) fica como estava; aqui a base continua (o bloco de painel de 2 andares, o telhado verde,
o corpo da escada) e o lote ganha o que faz ler QUARTEL e não prédio.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_russia_barracks_v2.py C:/Users/luisf/explosion/captures/game/models/russia_barracks_v2

A palavra (o que lê de cima): O PLAÇ PINTADO COM O PORTÃO DO KPP. De cima, o retângulo ESCURO de asfalto com as linhas brancas fortes
(o contorno, as 4 linhas de formação com as marcas de cada soldado, o ponto do comandante), o meio-fio pintado em vermelho e branco em
volta (a mania de todo quartel russo), o portão verde do KPP na frente com a guarita e a CANCELA listrada, o mastro alto, e do lado a
PISTA DE OBSTÁCULOS em fila (os pneus, o muro de tábuas, o arame baixo, a escada horizontal) na terra.
O que mudou do v1 (o "prédio normal"):
- o plaç: de lajes cinza com cruzinhas (sumia de cima) pro asfalto escuro com as linhas brancas grossas e o meio-fio listrado;
- o KPP (a entrada controlada): os 2 pilares, o portão de 2 folhas verdes com a estrela, abertas pra dentro; a guarita de vidro com
  o telhado verde, a luz e a placa; a cancela listrada levantada; a grade de barras verdes só na frente (vazada: quartel, não prisão);
- o mastro de 11 m com a bandeira (o jogo desenha o pano no vento; o desenho dele é do material da bandeira, EMBLEMAS.md);
- a pista de obstáculos no lado −Y (onde ficava o canto da barra fixa);
- o emblema (o aprovado de 01/11, EMBLEMAS.md): a ESTRELA VERMELHA com as bordas branca, azul e vermelha (a insígnia das Forças
  Armadas russas; o ru_star) no oitão da escada, nas 2 folhas do portão e na frente da tribuna;
- sem a laje do lote inteiro (a base acompanha a forma): a sapata só embaixo do bloco; o plaç e a pista assentam no chão.

Lote 16 x 20 m (X x Y), frente em +X. Alturas: o bloco 7,4 m na cumeeira, o oitão da escada 8,1 m, o mastro 11 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_Barracks_Base: tudo (a bandeira sai no flags.json; o jogo desenha o pano no vento);
  SM_Russia_Barracks_Scaffold: o andaime da obra.
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim (o asfalto), Hazard, Wood,
Rubber; Navy (a borda azul da estrela do ru_star).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export, scaffold  # noqa: E402,F401
import model_vehicle_parts as vp  # noqa: E402
from model_faction_kit import lamp  # noqa: E402
from model_building_parts import WallFace, wbox, tube, splits  # noqa: E402
from model_russia_parts import (RuPart, ru_block, ru_star, stencil, sign_board, gable_roof, entry_door, prof_sheet, flag_pole,  # noqa: E402
                                gable_end, flashing, roof_fan, grid_box, boom_barrier)
OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_barracks_v2')
PREFIX = 'SM_Russia_Barracks'

HX, HY = 8.0, 10.0                  # o lote 16 x 20
Z0 = 0.14
FL = Z0 + 0.12
T = 0.35
BX0, BX1, BY0, BY1 = -7.4, -1.6, -9.4, 9.4
F2, EAVE, RISE = 3.34, 6.0, 1.4
STAIR = (-1.6, -0.4, -1.8, 1.8)     # o corpo da escada saliente (x0, x1, y0, y1)
DOOR = (-1.0, 1.0, FL + 0.3, FL + 2.6)
PLAC = (0.15, 5.85, -6.3, 6.3)      # o plaç (x0, x1, y0, y1)
GATE_X, GATE_HW = 7.65, 2.4         # a linha do portão (os pilares) e a meia-largura do vão
KPP = (6.15, 7.55, 3.2, 5.2)      # a guarita (x0, x1, y0, y1); (revisão) mais rasa: o telhado não passa do lote
COURSE = (0.2, 7.1, -9.7, -7.0)     # a pista de obstáculos (x0, x1, y0, y1)


# ===================================================================================================== o bloco (o do v1)
def block(p):
    """O bloco de painel de 2 andares (oco) com o telhado de duas águas de chapa verde (a cumeeira em Y)."""
    ent = (-1.6, 1.6, Z0, 3.04)
    ru_block(p, BX0, BX1, BY0, BY1, Z0, EAVE, t=T, pw=3.0, zs=[F2, EAVE], parapet=0.0, roof='slab', floors=[F2],
             holes={'+x': [ent]}, ukeep={'+x': [-1.6, 1.6]},
             win={'+x': dict(floors=[Z0, F2], skip=[(3, None)], lit=[(1, 1), (5, 0)]), '-x': dict(floors=[Z0, F2], lit=[(2, 1)]),
                  '-y': dict(floors=[Z0, F2]), '+y': dict(floors=[Z0, F2], lit=[(0, 0)])},
             cols=[(-4.5, y) for y in (-6.0, -3.0, 3.0, 6.0)], step=3.0, along='y')
    gable_roof(p, BX0, BX1, BY0, BY1, EAVE, RISE, along='y', over=0.35, eave=0.38, t=T, gutter=True)
    for y in (-6.3, 6.3):
        lamp(p, BX1 + 0.08, y, 2.9, face='+x')
    # a sapata do bloco (só embaixo dele e do corpo da escada: nada de laje no lote inteiro)
    grid_box(p, 'Concrete#found', BX0 - 0.3, BX1 + 0.05, BY0 - 0.3, BY1 + 0.3, -0.1, Z0, bevel=0.04)
    grid_box(p, 'Concrete#found2', BX1 + 0.05, STAIR[1] + 0.25, STAIR[2] - 0.3, STAIR[3] + 0.3, -0.1, Z0, bevel=0.04)


def stair(p):
    """O corpo da escada saliente no meio da frente (o do v1), com a estrela nova (as bordas branca, azul e vermelha) no oitão."""
    x0, x1, y0, y1 = STAIR
    zt = EAVE
    ru_block(p, x0, x1, y0, y1, Z0, zt, t=T, walls=('+x', '+y', '-y'), skin=('+x', '+y', '-y'), pw=1.8, zs=[F2, zt], parapet=0.0,
             roof='slab', interior=False, holes={'+x': [DOOR, (-0.75, 0.75, 3.75, 5.5)]}, ukeep={'+x': [-1.0, 1.0]})
    F = WallFace('+x', x1, 0.0)
    entry_door(p, F, DOOR[0], DOOR[1], DOOR[2], DOOR[3], t=T, canopy=1.5)
    wbox(p, 'Glass', F, -0.75, 0.75, 3.75, 5.5, -0.2, -0.18, bevel=0.0)
    for u in (-0.75, 0.0, 0.75):
        wbox(p, 'White#win', F, u - 0.05, u + 0.05, 3.72, 5.53, -0.24, -0.14, bevel=0.012)
    for z in splits(3.75, 5.5, 0.58):
        wbox(p, 'White#win', F, -0.75, 0.75, z - 0.04, z + 0.04, -0.24, -0.14, bevel=0.012)
    p.box('Concrete#int', x0 - 1.0, x1 - 0.4, -0.9, 0.9, 4.6, 4.76, bevel=0.0)
    p.box('Light', x1 - 0.8, x1 - 0.5, -0.25, 0.25, 5.4, 5.44, bevel=0.0)
    rise = EAVE + RISE - zt
    xr = (BX0 + BX1) / 2
    gable_end(p, F, y0, y1, zt, rise, t=T)
    # a estrela nova no oitão (a das Forças Armadas: as bordas branca, azul e vermelha)
    zc = zt + 0.55
    ru_star(p, F, 0.0, zc, 0.24, n=0.08, t=0.025)
    for s in (-1, 1):
        P0 = Vector((x1 + 0.35, 0.0, zt + rise + 0.03))
        Pa = Vector((x1 + 0.35, s * (y1 - y0) / 2 * 1.2, zt - rise * 0.2 + 0.03))
        Pb = Vector((xr, 0.0, zt + rise + 0.03))
        prof_sheet(p, 'Roof', P0, Pa, Pb, drip=0.05)
    flashing(p, 'Paint#ridge', [(x1 + 0.4, 0.0, zt + rise + 0.07), (xr, 0.0, zt + rise + 0.07)], w=0.5, h=0.12)
    for s in (-1, 1):
        tube(p, 'Paint#ridge', [(x1 + 0.35, 0.0, zt + rise + 0.05), (x1 + 0.35, s * (y1 - y0) / 2 * 1.2, zt - rise * 0.2 + 0.05)], 0.04, seg=6)
    stencil(p, F, 0.0, DOOR[3] + 0.75, 'ЖЦ-6', h=0.32, m='White', n=0.08)
    # os degraus da porta até o plaç (o caminho de concreto)
    p.box('Concrete#path', x1 + 0.2, PLAC[0] + 0.02, -1.3, 1.3, -0.04, 0.06, bevel=0.03)


def roof_z(x):
    xm, hw = (BX0 + BX1) / 2, (BX1 - BX0) / 2
    return EAVE + RISE - RISE * abs(x - xm) / hw + 0.05


def roof_kit(p):
    """O que o telhado de uma caserna russa tem (o do v1): as chaminés de ventilação, os exaustores, a antena e a parabólica."""
    xm = (BX0 + BX1) / 2
    for y in (-6.4, 5.6):
        x = xm - 0.75
        z0 = roof_z(x - 0.35) - 0.25
        p.box('Concrete#chim', x - 0.35, x + 0.35, y - 0.45, y + 0.45, z0, EAVE + RISE + 1.05, bevel=0.04)
        p.box('Concrete#chim', x - 0.42, x + 0.42, y - 0.52, y + 0.52, EAVE + RISE + 1.05, EAVE + RISE + 1.15, bevel=0.02)
        for dy in (-0.2, 0.2):
            p.box('Dark', x - 0.18, x + 0.18, y + dy - 0.13, y + dy + 0.13, EAVE + RISE + 1.13, EAVE + RISE + 1.16, bevel=0.0)
        for (dx, dyy) in ((-1, -1), (1, 1), (-1, 1), (1, -1)):
            p.box('Paint#cap', x + dx * 0.36 - 0.03, x + dx * 0.36 + 0.03, y + dyy * 0.46 - 0.03, y + dyy * 0.46 + 0.03,
                  EAVE + RISE + 1.15, EAVE + RISE + 1.42, bevel=0.0)
        p.box('Paint#cap', x - 0.5, x + 0.5, y - 0.6, y + 0.6, EAVE + RISE + 1.42, EAVE + RISE + 1.48, bevel=0.0)
    for y in (-3.6, 3.2, 7.6):
        x = xm + 1.5
        roof_fan(p, x, y, roof_z(x) + 0.12, r=0.34)
    ax, ay, az = xm, -8.2, EAVE + RISE + 0.05
    p.cyl('Metal#ant', (ax, ay, az + 1.6), 'z', 0.04, 3.2, seg=8, bevel=0.0)
    for k, w in enumerate((1.4, 1.1, 0.8)):
        zz = az + 1.9 + 0.45 * k
        p.box('Metal#ant', ax - 0.02, ax + 0.02, ay - w / 2, ay + w / 2, zz - 0.02, zz + 0.02, bevel=0.0)
        for yy in splits(ay - w / 2, ay + w / 2, 0.22):
            p.box('Metal#ant', ax - 0.13, ax + 0.13, yy - 0.012, yy + 0.012, zz - 0.012, zz + 0.012, bevel=0.0)
    for (dx, dy) in ((1.2, 0.0), (-1.2, 0.0), (0.0, 1.4)):
        tube(p, 'Metal#ant', [(ax, ay, az + 2.6), (ax + dx, ay + dy, roof_z(ax + dx) + 0.02)], 0.008, seg=4)
    dx_, dy_ = xm - 1.6, -2.4
    dz = roof_z(dx_)
    p.cyl('Metal#ant', (dx_, dy_, dz + 0.3), 'z', 0.03, 0.6, seg=6, bevel=0.0)
    p.cyl('White#dish', (dx_, dy_, dz + 0.68), 'z', 0.1, 0.14, seg=24, r2=0.5, bevel=0.0, smooth=True)
    p.cyl('Metal#ant', (dx_, dy_, dz + 0.95), 'z', 0.025, 0.4, seg=6, bevel=0.0)


# ===================================================================================================== o plaç
def curb_run(p, a, b, w=0.16, h=0.14, seg=0.5):
    """O meio-fio PINTADO em blocos vermelhos e brancos alternados (o quartel russo pinta tudo), de a até b (no chão)."""
    a, b = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    L = (b - a).length
    n = max(1, int(round(L / seg)))
    d = (b - a) / n
    side = Vector((-d.y, d.x, 0.0)).normalized() * (w / 2)
    for k in range(n):
        q0, q1 = a + d * k, a + d * (k + 1)
        xs = sorted((q0.x - side.x, q0.x + side.x, q1.x - side.x, q1.x + side.x))
        ys = sorted((q0.y - side.y, q0.y + side.y, q1.y - side.y, q1.y + side.y))
        m = 'White#curb' if k % 2 == 0 else 'Red#curb'
        p.box(m, xs[0] + 0.005, xs[-1] - 0.005, ys[0] + 0.005, ys[-1] - 0.005, -0.04, h, bevel=0.02)


def plac(p):
    """O plaç: o asfalto escuro, as linhas brancas grossas (o contorno, as 4 linhas de formação com a marca de cada soldado, o ponto do
    comandante na frente da tribuna), o meio-fio listrado em volta, a tribuna com a estrela, o mastro alto."""
    x0, x1, y0, y1 = PLAC
    grid_box(p, 'Trim#asphalt', x0, x1, y0, y1, -0.05, 0.06, bevel=0.0)
    zl = 0.062
    # o contorno (as do x inteiras; as do y entre elas, noutro balde)
    i = 0.3
    for (yy) in (y0 + i, y1 - i):
        p.box('White#linex', x0 + i - 0.08, x1 - i + 0.08, yy - 0.08, yy + 0.08, zl - 0.01, zl + 0.006, bevel=0.0)
    for xx in (x0 + i, x1 - i):
        p.box('White#liney', xx - 0.08, xx + 0.08, y0 + i + 0.085, y1 - i - 0.085, zl - 0.01, zl + 0.006, bevel=0.0)
    # as 4 linhas de formação (ao longo de y, viradas pra tribuna do lado +Y) com as marcas de cada soldado a cada 0,9 m
    for xx in (1.35, 2.45, 3.55, 4.65):
        p.box('White#rank', xx - 0.05, xx + 0.05, y0 + 1.0, y1 - 1.6, zl - 0.01, zl + 0.006, bevel=0.0)
        for yy in splits(y0 + 1.3, y1 - 1.9, 0.9):
            p.box('White#tick', xx - 0.24, xx - 0.055, yy - 0.05, yy + 0.05, zl - 0.01, zl + 0.006, bevel=0.0)
            p.box('White#tick2', xx + 0.055, xx + 0.24, yy - 0.05, yy + 0.05, zl - 0.01, zl + 0.006, bevel=0.0)
    # a faixa larga da "linha do comandante" na frente da tribuna e o ponto dele (o círculo)
    p.box('White#cmd', 1.0, 5.0, y1 - 1.05, y1 - 0.9, zl - 0.01, zl + 0.006, bevel=0.0)
    p.cyl('White#spot', (3.0, y1 - 0.62, zl), 'z', 0.24, 0.016, seg=24, bevel=0.0)
    # o meio-fio listrado em volta (menos o lado do bloco, onde chega o caminho)
    curb_run(p, (x0 - 0.1, y0 - 0.1), (x1 + 0.1, y0 - 0.1))
    curb_run(p, (x0 - 0.1, y1 + 0.1), (x1 + 0.1, y1 + 0.1))
    curb_run(p, (x1 + 0.1, y0 + 0.2), (x1 + 0.1, y1 - 0.2))
    curb_run(p, (x0 - 0.1, y0 + 0.2), (x0 - 0.1, -1.5))
    curb_run(p, (x0 - 0.1, 1.5), (x0 - 0.1, y1 - 0.2))
    # a TRIBUNA do lado +Y: o pódio de concreto, a frente vermelha virada pro plaç (−Y) com a estrela, o guarda-corpo, a escada
    tx0, tx1, ty0, ty1 = 1.7, 4.3, 6.75, 8.15
    th = 1.25
    p.box('Concrete#trib', tx0, tx1, ty0, ty1, -0.05, th, bevel=0.06)
    Ft = WallFace('-y', ty0, 0.0)
    wbox(p, 'Red#trib', Ft, tx0 + 0.15, tx1 - 0.15, 0.15, th - 0.12, 0.0, 0.04, bevel=0.0)
    ru_star(p, Ft, (tx0 + tx1) / 2, th * 0.5, 0.2, n=0.04, t=0.02)
    for (a, b) in (((tx0 + 0.1, ty0 + 0.1), (tx1 - 0.1, ty0 + 0.1)), ((tx0 + 0.1, ty0 + 0.1), (tx0 + 0.1, ty1 - 0.5)),
                   ((tx1 - 0.1, ty0 + 0.1), (tx1 - 0.1, ty1 - 0.5))):
        nseg = max(1, int(math.dist(a, b) / 0.8))
        for k in range(nseg + 1):
            t_ = k / nseg
            p.cyl('Paint#rail', (a[0] + (b[0] - a[0]) * t_, a[1] + (b[1] - a[1]) * t_, th + 0.5), 'z', 0.03, 1.0, seg=6, bevel=0.0)
        tube(p, 'Paint#rail', [(a[0], a[1], th + 1.0), (b[0], b[1], th + 1.0)], 0.035, seg=6)
    for k in range(4):                                                      # a escada de trás (os degraus)
        z = th - (k + 1) * th / 5
        p.box('Concrete#tstep%d' % (k % 2), tx1 - 1.1, tx1 - 0.1, ty1 + k * 0.3, ty1 + (k + 1) * 0.3, -0.05, z, bevel=0.03)
    # o MASTRO alto ao lado da tribuna (a bandeira mais alta do quartel) e as 2 bandeiras menores do outro lado
    flag_pole(p, 5.3, 7.5, 0.0, 11.0, cloth=(2.4, 1.6), phase=0.2)
    # o quadro de avisos (os 2 стенды vermelhos) atrás da tribuna, virados pro plaç
    for (u, txt) in ((1.2, 'ДЖЛЫ'), (5.9, 'ШБ-7')):
        for x in (u - 0.85, u + 0.85):
            p.cyl('Paint#post', (x, 9.3, 1.1), 'z', 0.05, 2.2, seg=8, bevel=0.0)
        sign_board(p, WallFace('-y', 9.25, 0.0), u, 1.15, txt, h=0.36, board='Red', ink='White', pad=0.18)


# ===================================================================================================== o KPP (a entrada)
def kpp(p):
    """A entrada controlada: os 2 pilares, o portão verde de 2 folhas com a estrela (abertas pra dentro), a guarita de vidro com o telhado
    verde, a cancela listrada levantada, a grade de barras verdes só na frente."""
    gx, hw = GATE_X, GATE_HW
    # os 2 pilares de alvenaria rebocada (branco) com o chapéu e a luz
    for s in (-1, 1):
        y = s * (hw + 0.3)
        p.box('White#gpil%d' % (s + 1), gx - 0.3, gx + 0.3, y - 0.3, y + 0.3, -0.05, 2.9, bevel=0.04)
        p.box('Concrete#gcap%d' % (s + 1), gx - 0.38, gx + 0.38, y - 0.38, y + 0.38, 2.9, 3.05, bevel=0.03)
        p.cyl('Dark#glamp', (gx, y, 3.15), 'z', 0.12, 0.2, seg=12, bevel=0.0)
        p.cyl('Light', (gx, y, 3.3), 'z', 0.1, 0.1, seg=12, bevel=0.0, smooth=True)
        p.box('Red#gband%d' % (s + 1), gx + 0.3, gx + 0.31, y - 0.25, y + 0.25, 0.3, 0.6, bevel=0.0)
    # as 2 folhas abertas pra dentro (−X) a 90° (encostadas na lateral do vão): a moldura de tubo, a chapa verde, a estrela no meio
    for s in (-1, 1):
        hy = s * hw
        ang = math.radians(90.0)
        d = Vector((-math.sin(ang), -s * math.cos(ang), 0.0))          # da dobradiça pra ponta, abrindo pra dentro
        n = Vector((-d.y, d.x, 0.0))
        if n.y * s < 0:
            n = -n
        L, H, z0 = hw - 0.08, 2.3, 0.22
        from model_russia_vehicle_kit import frame_box
        o = Vector((gx - 0.05, hy, 0.0))
        c = o + d * (L / 2)
        frame_box(p, 'Roof#gate', (c.x, c.y, z0 + H / 2), d, Vector((0, 0, 1)), n, L, H, 0.05, bevel=0.0)
        # (revisão: as barras no mesmo balde soldavam nos cantos, NONMANIFOLD; as de pé, as deitadas e a do meio em baldes separados)
        for (key, u0, u1, zz0, zz1) in (('h', 0.0, L, z0, z0 + 0.07), ('h', 0.0, L, z0 + H - 0.07, z0 + H), ('v', 0.0, 0.07, z0 + 0.072, z0 + H - 0.072),
                                        ('v', L - 0.07, L, z0 + 0.072, z0 + H - 0.072), ('m', 0.072, L - 0.072, z0 + H * 0.5 - 0.035, z0 + H * 0.5 + 0.035)):
            q = o + d * ((u0 + u1) / 2)
            frame_box(p, 'Paint#gframe' + key, (q.x, q.y, (zz0 + zz1) / 2), d, Vector((0, 0, 1)), n, u1 - u0, zz1 - zz0, 0.09, bevel=0.01)
        for zz in (z0 + 0.4, z0 + H - 0.4):
            p.cyl('Metal#ghinge', (o.x, o.y, zz), 'z', 0.05, 0.2, seg=10, bevel=0.0)
        # a estrela nas 2 faces da folha (a folha a 90° fica no plano xz: a face é ±y)
        for sg in (-1, 1):
            face = '+y' if n.y * sg > 0 else '-y'
            ru_star(p, WallFace(face, hy + n.y * sg * 0.025, 0.0), c.x, z0 + H * 0.74, 0.17, n=0.0, t=0.02)
    # a guarita (KPP): o embasamento, as paredes de painel com o vidro largo nas 3 faces, o telhado verde em balanço, a placa, a luz
    x0, x1, y0, y1 = KPP
    p.box('Concrete#kbase', x0 - 0.1, x1 + 0.1, y0 - 0.1, y1 + 0.1, -0.05, 0.3, bevel=0.03)
    p.box('Concrete#kwall', x0, x1, y0, y1, 0.3, 2.75, bevel=0.06)
    for face, at, (a, b) in (('+x', x1, (y0, y1)), ('-y', y0, (x0, x1)), ('-x', x0, (y0, y1))):
        F = WallFace(face, at, 0.0)
        wbox(p, 'Glass#k', F, a + 0.25, b - 0.25, 1.05, 2.3, 0.0, 0.02, bevel=0.0)
        for u in splits(a + 0.25, b - 0.25, 0.65):
            wbox(p, 'White#kf', F, u - 0.035, u + 0.035, 1.05, 2.3, 0.0, 0.06, bevel=0.0)
        for z in (1.05, 2.3):
            wbox(p, 'White#kf2', F, a + 0.22, b - 0.22, z - 0.04, z + 0.04, 0.0, 0.06, bevel=0.0)
        wbox(p, 'White#ksill', F, a + 0.2, b - 0.2, 0.95, 1.02, 0.0, 0.12, bevel=0.01)
    Fd = WallFace('+y', y1, 0.0)                                             # a porta do lado de dentro do lote (+Y)
    wbox(p, 'Paint#kdoor', Fd, x0 + 0.4, x0 + 1.25, 0.3, 2.3, 0.0, 0.04, bevel=0.01)
    p.box('Roof#kroof', x0 - 0.45, x1 + 0.45, y0 - 0.45, y1 + 0.45, 2.75, 2.9, bevel=0.03)
    p.box('Paint#kfasc', x0 - 0.47, x1 + 0.47, y0 - 0.47, y1 + 0.47, 2.68, 2.75, bevel=0.0)
    sign_board(p, WallFace('+x', x1 + 0.45, 0.0), (y0 + y1) / 2, 2.98, 'ПЛ-4', h=0.26, board='Red', ink='White', pad=0.1)
    lamp(p, x1 + 0.06, y0 + 0.35, 2.45, face='+x')
    # a cancela listrada (o pilar ao lado da guarita, o braço levantado sobre o vão do portão)
    boom_barrier(p, 6.9, -hw - 0.65, length=5.5, angle=70.0, yaw=90.0)
    # a grade da frente: os pilares brancos e os painéis de barras verdes (vazada), dos pilares do portão até os cantos
    for (ya, yb) in ((-hw - 0.6, -HY + 0.25), (y1 + 0.15, HY - 0.25)):
        posts = splits(min(ya, yb), max(ya, yb), 2.3)
        for k, y in enumerate(posts):
            p.box('White#fpost%d' % (k % 2), gx - 0.18, gx + 0.18, y - 0.18, y + 0.18, -0.05, 2.15, bevel=0.03)
            p.box('Concrete#fcap%d' % (k % 2), gx - 0.22, gx + 0.22, y - 0.22, y + 0.22, 2.15, 2.25, bevel=0.02)
        for a, b in zip(posts[:-1], posts[1:]):
            for z in (0.25, 1.85):
                p.box('Roof#frail', gx - 0.03, gx + 0.03, a + 0.18, b - 0.18, z - 0.03, z + 0.03, bevel=0.0)
            for y in splits(a + 0.3, b - 0.3, 0.16):
                p.cyl('Roof#fbar', (gx, y, 1.05), 'z', 0.018, 1.75, seg=6, bevel=0.0)
            p.box('Concrete#fsock', gx - 0.12, gx + 0.12, a + 0.18, b - 0.18, -0.05, 0.12, bevel=0.0)


# ===================================================================================================== a pista de obstáculos
def tyre(p, x, y, z=0.0, r=0.38, w=0.24):
    """O pneu velho deitado (a corrida dos pneus): o toro achatado."""
    p.torus('Rubber#tyre', (x, y, z + w / 2), 'z', r - w * 0.35, w * 0.5, seg=20, rseg=8)


def course(p):
    """A PISTA DE OBSTÁCULOS em fila no lado −Y (lida de cima como o pátio de treino): a placa da largada, os pneus em zigue-zague, o
    muro de tábuas com os escoras, o rastejo sob o arame baixo, a escada horizontal, a vala com as pranchas, as bordas de madeira da
    pista (a terra do mapa fica à mostra dentro)."""
    x0, x1, y0, y1 = COURSE
    yc = (y0 + y1) / 2
    # as bordas de madeira (os dormentes) dos 2 lados da pista
    for y in (y0 + 0.08, y1 - 0.08):
        p.box('Wood#edge%d' % (0 if y < yc else 1), x0, x1, y - 0.08, y + 0.08, -0.05, 0.1, bevel=0.02)
    # a largada: os 2 postes com a faixa vermelha e branca atravessada
    for y in (y0 + 0.3, y1 - 0.3):
        p.cyl('Paint#start', (x0 + 0.3, y, 0.9), 'z', 0.05, 1.8, seg=8, bevel=0.0)
    for k in range(6):
        ya = y0 + 0.3 + (y1 - y0 - 0.6) * k / 6
        yb = y0 + 0.3 + (y1 - y0 - 0.6) * (k + 1) / 6
        p.box(('Red' if k % 2 == 0 else 'White') + '#startb', x0 + 0.26, x0 + 0.34, ya + 0.003, yb - 0.003, 1.45, 1.7, bevel=0.0)
    # os pneus em 2 fileiras desencontradas
    for k in range(4):
        for j, dy in enumerate((-0.55, 0.55)):
            tyre(p, x0 + 1.0 + k * 0.72 + j * 0.36, yc + dy)
    # o muro de tábuas (2 m) com as escoras atrás e a plataforma de saída
    wx = x0 + 4.3
    p.box('Wood#wall', wx - 0.1, wx + 0.1, y0 + 0.35, y1 - 0.35, -0.05, 2.0, bevel=0.02)
    for z in splits(0.2, 1.9, 0.25):
        p.box('Dark#wallgap', wx + 0.1, wx + 0.105, y0 + 0.36, y1 - 0.36, z - 0.006, z + 0.006, bevel=0.0)
    for y in (y0 + 0.5, yc, y1 - 0.5):
        tube(p, 'Wood#strut', [(wx - 0.1, y, 1.6), (wx - 1.0, y, 0.0)], 0.06, seg=6)
        p.box('Wood#post', wx + 0.1, wx + 0.24, y - 0.07, y + 0.07, -0.05, 2.1, bevel=0.01)
    # o rastejo: o arame baixo nas estacas (a grade de 4 x 3) depois do muro
    ax0, ax1 = wx + 0.5, wx + 2.1
    for x in splits(ax0, ax1, 0.8):
        for y in splits(y0 + 0.45, y1 - 0.45, 0.85):
            p.cyl('Wood#stake', (x, y, 0.27), 'z', 0.035, 0.6, seg=6, bevel=0.0)
    for y in splits(y0 + 0.45, y1 - 0.45, 0.85):
        tube(p, 'Metal#wire', [(ax0, y, 0.55), (ax1, y, 0.55)], 0.012, seg=4)
    for x in splits(ax0, ax1, 0.8):
        tube(p, 'Metal#wire', [(x, y0 + 0.45, 0.55), (x, y1 - 0.45, 0.55)], 0.012, seg=4)
    # a escada horizontal (as barras) no fim, com os 2 pórticos
    lx0, lx1 = ax1 + 0.6, min(x1 - 0.1, ax1 + 2.2)
    for x in (lx0, lx1):
        for y in (yc - 0.5, yc + 0.5):
            p.cyl('Paint#lad', (x, y, 1.1), 'z', 0.05, 2.2, seg=8, bevel=0.0)
    for y in (yc - 0.5, yc + 0.5):
        tube(p, 'Paint#ladr', [(lx0, y, 2.15), (lx1, y, 2.15)], 0.04, seg=8)
    for x in splits(lx0 + 0.15, lx1 - 0.15, 0.32):
        tube(p, 'Metal#rung', [(x, yc - 0.5, 2.15), (x, yc + 0.5, 2.15)], 0.025, seg=6)


# ===================================================================================================== montagem
def base():
    p = RuPart(PREFIX + '_Base')
    block(p)
    roof_kit(p)
    stair(p)
    plac(p)
    kpp(p)
    course(p)
    return p.finish()


def write_preview():
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                      ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 900]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [11.5, 0.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, EAVE + RISE + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    write_preview()
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 20 m; porta da infantaria em X', STAIR[1], 'Y', DOOR[0], '..', DOOR[1], '; portão do KPP em X', GATE_X,
          'Y', -GATE_HW, '..', GATE_HW, '; cumeeira', EAVE + RISE, 'm; mastro 11 m')
    print('MODEL fire_points_m', [(-4.5, -5.5, EAVE + RISE + 0.1), (-4.5, 5.0, EAVE + RISE + 0.1), (-1.0, 0.0, EAVE + RISE + 0.05)])


if __name__ == '__main__':
    main()
