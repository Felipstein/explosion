"""A Usina a gás da Rússia (Power Plant) provisória, modelada no Blender por script (a facção nova, 01/11/2026: docs/FACCAO_RUSSIA.md
§1 e §5.1: "a usina a gás (+7); a chaminé listrada vermelha e branca; cai com a bola de fogo do gás").

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_power_plant.py C:/Users/luisf/explosion/captures/game/models/russia_power_plant

A planta: a central térmica a gás de bloco (a GTU-TETs russa: os pavilhões das turbinas de painel, a chaminé alta de concreto com as
faixas da sinalização de aviação, o duto grosso dos gases quentes nos cavaletes por cima dos telhados, o cano de gás amarelo que corre
por cima do chão com a lira de dilatação e as válvulas de volante vermelho, os transformadores no pátio com o pórtico de saída).

A palavra (o que lê de cima): A CHAMINÉ LISTRADA. De cima, o círculo vermelho e branco com a boca escura no canto; os 2 telhados verdes
de duas águas (os pavilhões) atravessados pelo duto grosso; o cano amarelo; os transformadores azul-acinzentados.

Peça por peça:
- os 2 pavilhões das turbinas (7,6 × 4,0 m, beiral a 5,0 m, cumeeira a 6,2 m): ocos, painéis industriais horizontais, as fitas de
  janela, os oitões de painel com a grelha, a porta de enrolar meio aberta, o número em estêncil, a turbina no skid por dentro (o furo
  do dano mostra a máquina); o telhado de chapa verde com o rufo, as calhas, os tubos de descida, os exaustores redondos;
- a chaminé (18 m, 7 faixas, a de cima vermelha): o pé largo, as 2 plataformas com o guarda-corpo e as luzes de obstáculo, a escada com
  a gaiola, a fuligem na boca;
- o duto dos gases (0,45 m de raio) que sobe de cada pavilhão, corre por cima dos telhados nos cavaletes e entra na chaminé;
- o cano de gás amarelo: entra pelo lote, faz a lira de dilatação (o П por cima da passagem), as válvulas de volante vermelho, o
  armário do regulador, e entra no pavilhão;
- o pátio de transformadores (2 transformadores com as aletas, as buchas brancas, o conservador) e o pórtico de saída treliçado com os
  isoladores e os cabos;
- a casa de controle (1 andar de painel, as janelas de caixilho branco, a porta com a marquise) com a bandeira da cor do jogador no teto.

Lote 14 × 16 m (X × Y), frente em +X. Alturas: os pavilhões 6,2 m, a casa de controle 4,1 m, o duto 8,1 m, o pórtico 7,1 m, a chaminé
18,1 m (a boca) e as luzes até ~18,4 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_PowerPlant_Base: tudo (nada se mexe; a bandeira sai no flags.json, o jogo desenha o pano no vento);
  SM_Russia_PowerPlant_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Rubber; Yellow (o cano de
gás) e Beacon (as luzes de obstáculo).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, splits, fillet, cn_rollup  # noqa: E402
from model_russia_parts import (ru_foundation, RuPart, ru_block, stencil, gable_roof, ribbon_glazing, entry_door, striped_chimney,  # noqa: E402
                                transformer, gas_pipe, valve, ladder_on, roof_fan, ac_box, lattice_mast, ru_star, tarp_stack, flag_pole)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_power_plant')
PREFIX = 'SM_Russia_PowerPlant'

HX, HY = 7.0, 8.0                   # o lote 14 × 16
Z0 = 0.14
FL = Z0 + 0.12
T = 0.3
PX0, PX1 = -6.6, 1.0                # os pavilhões (em X)
PAVS = [(-7.2, -3.2), (-2.2, 1.8)]  # os pavilhões (em Y)
EAVE, RISE = 5.0, 1.2
CHIM = (-4.2, 4.7)                  # o eixo da chaminé
CHIM_H = 18.0
DUCT_Z = 7.65                       # o eixo do duto dos gases
DUCT_R = 0.45


def pavilions(p):
    """Os 2 pavilhões das turbinas: os blocos de painel (ocos), as fitas de janela, a porta de enrolar meio aberta com a turbina acesa
    dentro, o telhado verde de duas águas com o oitão de painel e a grelha, os exaustores."""
    for k, (y0, y1) in enumerate(PAVS):
        ym = (y0 + y1) / 2
        door = (ym - 1.3, ym + 1.3, Z0, 3.4)
        holes = {'+x': [door], '-y': [(-5.9, 0.0, 3.16, 4.36)], '+y': [(-5.9, 0.0, 3.16, 4.36)]}
        ru_block(p, PX0, PX1, y0, y1, Z0, EAVE, t=T, pw=3.2, parapet=0.0, roof=None, zs=[1.96, 3.16, 4.36], holes=holes,
                 ukeep={'-y': [-5.9, 0.0], '+y': [-5.9, 0.0]}, cols=[], step=3.8, along='x')
        for (f, at) in (('-y', y0), ('+y', y1)):
            ribbon_glazing(p, WallFace(f, at, 0.0), -5.9, 0.0, 3.16, 4.36, t=T, step=1.5, rows=1)
        # a laje de forro com as tesouras e o telhado de duas águas
        gable_roof(p, PX0, PX1, y0, y1, EAVE, RISE, along="x", over=0.3, eave=0.35, t=T, gutter=True)
        # a porta de enrolar meio aberta (o escuro de dentro e a turbina acesa)
        F = WallFace('+x', PX1, 0.0)
        cn_rollup(p, F, door[0], door[1], FL, door[3], t=T, open_frac=0.55, m='Paint#door', rails='Metal#door')
        wbox(p, 'Hazard', F, door[0] - 0.12, door[0], FL, FL + 1.2, 0.08, 0.12, bevel=0.0)
        wbox(p, 'Hazard', F, door[1], door[1] + 0.12, FL, FL + 1.2, 0.08, 0.12, bevel=0.0)
        lamp(p, PX1 + 0.08, ym, door[3] + 0.55, face='+x')
        # o número do bloco em estêncil no oitão e a grelha de ventilação
        stencil(p, F, ym - 0.75, EAVE + 0.08, 'Б-' + ('1' if k == 0 else '4'), h=0.55, m='White', n=0.08)
        wbox(p, 'Metal#louv', F, ym + 0.3, ym + 1.3, EAVE + 0.1, EAVE + 0.7, -0.05, 0.12, bevel=0.012)
        for z in splits(EAVE + 0.18, EAVE + 0.62, 0.11):
            wbox(p, 'Dark', F, ym + 0.36, ym + 1.24, z - 0.02, z + 0.02, 0.12, 0.14, bevel=0.0)
        # a turbina lá dentro (o carretel no skid, o gerador, a luz)
        tx0 = PX1 - T - 6.4
        p.box('Metal#skid', tx0, PX1 - T - 0.6, ym - 1.0, ym + 1.0, FL, FL + 0.4, bevel=0.03)
        p.cyl('Paint#turb', ((tx0 + PX1 - T - 0.6) / 2 + 0.6, ym, FL + 1.3), 'x', 0.85, 3.6, seg=24, bevel=0.0, smooth=True)
        p.cyl('Paint#turb', (tx0 + 1.1, ym, FL + 1.35), 'x', 1.0, 1.4, seg=24, bevel=0.0, smooth=True)
        p.cyl('Yellow#gas', (PX1 - T - 1.2, ym + 0.5, FL + 1.0), 'z', 0.08, 1.4, seg=10, bevel=0.0)
        p.box('Light', PX1 - T - 3.0, PX1 - T - 1.2, ym - 0.3, ym + 0.3, EAVE - 0.35, EAVE - 0.3, bevel=0.0)
        # os exaustores no telhado e a escada de marinheiro
        for xx in (-5.2, -2.4):
            roof_fan(p, xx, ym, EAVE + RISE + 0.12, r=0.36)
    ladder_on(p, WallFace("-y", PAVS[0][0], 0.0), -2.0, Z0, EAVE, cage=False)


def chimney(p):
    """A chaminé listrada (a assinatura), na sapata de concreto, com o duto dos gases que chega dos pavilhões por cima dos telhados."""
    cx, cy = CHIM
    p.cyl('Concrete#chf', (cx, cy, Z0 + 0.15), 'z', 1.55, 0.3, seg=32, bevel=0.05)
    striped_chimney(p, CHIM, Z0 + 0.3, CHIM_H - 0.3, 1.0, 0.62, bands=7, platforms=(0.55, 0.88), ladder_side=-50.0, base_h=1.3)
    # o duto: sobe de cada pavilhão (o pescoço com a flange), corre em Y por cima dos telhados e entra na chaminé
    xd = -2.0
    yA, yB = (PAVS[0][0] + PAVS[0][1]) / 2, (PAVS[1][0] + PAVS[1][1]) / 2
    zr = EAVE + RISE * 0.55
    r_ch = 1.0 + (0.62 - 1.0) * (DUCT_Z - Z0 - 1.6) / (CHIM_H - 1.6)
    path = fillet([(xd, yA, zr), (xd, yA, DUCT_Z), (xd, cy - 1.6, DUCT_Z), (cx + r_ch + 0.05, cy, DUCT_Z)], 0.9, n=6)
    tube(p, 'Metal#duct', path, DUCT_R, seg=20)
    tube(p, 'Metal#duct', [(xd, yB, zr), (xd, yB, DUCT_Z - 0.2)], DUCT_R * 0.85, seg=20)
    for (y, z) in ((yA, zr + 0.4), (yB, zr + 0.4), (yA - 0.0, DUCT_Z - 0.9)):
        p.torus('Metal#flange', (xd, y, z), 'z', DUCT_R + 0.05, 0.06, seg=20, rseg=6)
    for y in (yA + 1.8, yB - 1.0, cy - 2.6):                                     # as flanges do trecho deitado
        p.torus('Metal#flange', (xd, y, DUCT_Z), 'y', DUCT_R + 0.05, 0.06, seg=20, rseg=6)
    # os cavaletes do duto (os pés de aço até o telhado ou o chão, a sela)
    for (y, zb) in ((-3.1, Z0), (yB + 1.4, EAVE + RISE * 0.3), (2.6, Z0)):
        for s in (-1, 1):
            tube(p, 'Paint#trestle', [(xd + s * 0.7, y, zb), (xd + s * 0.35, y, DUCT_Z - DUCT_R - 0.05)], 0.07, seg=8)
        p.box('Paint#trestle', xd - 0.6, xd + 0.6, y - 0.15, y + 0.15, DUCT_Z - DUCT_R - 0.18, DUCT_Z - DUCT_R + 0.08, bevel=0.03)
        if zb < 1.0:
            for s in (-1, 1):
                p.box('Concrete#foot', xd + s * 0.7 - 0.25, xd + s * 0.7 + 0.25, y - 0.25, y + 0.25, Z0, Z0 + 0.3, bevel=0.04)
    # a faixa de perigo do duto quente e a passarela de inspeção ao lado
    for y in (yA + 0.6, cy - 2.0):
        p.cyl('Hazard', (xd, y, DUCT_Z), 'y', DUCT_R + 0.012, 0.4, seg=20, bevel=0.0)


def gas_line(p):
    """O cano de gás amarelo: entra pelo canto do lote em pés baixos, faz a lira de dilatação (o П alto, a passagem por baixo), passa
    pelas válvulas e pelo armário do regulador e entra no pavilhão 2."""
    zl = 0.75
    y = 2.2
    pts = [(HX - 0.3, y, zl), (4.2, y, zl), (4.2, y, 3.6), (2.6, y, 3.6), (2.6, y, zl), (1.6, y, zl), (1.6, 0.9, zl), (PX1 + 0.05, 0.9, zl)]
    gas_pipe(p, pts, r=0.11, feet=Z0, step=1.4)
    valve(p, 5.6, y, zl, axis='x')
    valve(p, 1.6, 1.6, zl, axis='y')
    # o armário do regulador (a caixa amarela com o telhadinho e as grelhas)
    p.box('Concrete#foot', 4.7, 6.5, 2.7, 3.9, Z0, Z0 + 0.25, bevel=0.04)
    p.box('Yellow#cab', 4.85, 6.35, 2.85, 3.75, Z0 + 0.25, Z0 + 1.85, bevel=0.04)
    p.box('Yellow#cab', 4.7, 6.5, 2.7, 3.9, Z0 + 1.85, Z0 + 1.95, bevel=0.02)
    F = WallFace('-y', 2.85, 0.0)
    for x in (5.2, 6.0):
        for z in splits(Z0 + 0.6, Z0 + 1.5, 0.18):
            wbox(p, 'Dark', F, x - 0.25, x + 0.25, z - 0.03, z + 0.03, 0.0, 0.02, bevel=0.0)
    tube(p, 'Yellow#gas', [(5.6, 2.85, zl + 0.3), (5.6, y + 0.1, zl + 0.3), (5.6, y + 0.1, zl + 0.1)], 0.05, seg=8)


def yard(p):
    """O pátio dos transformadores (2) com o pórtico de saída treliçado, os isoladores e os cabos; a cerca de tela baixa."""
    for y in (-5.6, -1.6):
        transformer(p, 3.9, y, Z0, sx=2.2, sy=1.5, h=1.9)
        # o barramento até o pavilhão (os 3 cabos grossos em catenária)
        for k in range(3):
            yy = y - 0.45 + k * 0.45
            cable(p, (3.9 + 0.62, yy, Z0 + 0.3 + 1.9 + 0.85), (PX1 + 0.1, yy, EAVE - 0.6), sag=0.35, r=0.03, m='Dark#cable')
    # o pórtico de saída (as 2 torres treliçadas e a travessa com os isoladores pendurados)
    px = 6.2
    for y in (-7.2, 0.0):
        lattice_mast(p, (px, y), Z0, 7.0, 0.32, 0.22, 6, m='Paint#mast', leg=0.05, brace=0.03, tri=False)
        p.box('Concrete#foot', px - 0.55, px + 0.55, y - 0.55, y + 0.55, Z0, Z0 + 0.3, bevel=0.04)
    p.box('Paint#mast', px - 0.18, px + 0.18, -7.45, 0.25, 6.6, 6.95, bevel=0.03)
    for y in (-5.6, -3.6, -1.6):
        for j in range(4):
            p.cyl('White#ins', (px, y, 6.5 - j * 0.14), 'z', 0.13 if j % 2 == 0 else 0.09, 0.08, seg=12, bevel=0.0)
        cable(p, (px, y, 5.9), (3.9 + 0.62, y + (0.45 if y < -4 else -0.45 if y > -2 else 0.0), Z0 + 0.3 + 1.9 + 0.85), sag=0.6, r=0.025,
              m='Dark#cable')
    # a cerca de tela baixa em volta dos transformadores (os postes, as travessas, a placa de perigo)
    fx0, fx1, fy0, fy1 = 2.0, 5.6, -7.6, 0.3
    for (a, b) in (((fx0, fy0), (fx1, fy0)), ((fx1, fy0), (fx1, fy1)), ((fx0, fy1), (fx1, fy1))):
        L = math.dist(a, b)
        for k in range(int(L / 1.5) + 1):
            t = k / int(L / 1.5)
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            p.cyl('Metal#fence', (x, y, Z0 + 0.8), 'z', 0.04, 1.6, seg=8, bevel=0.0)
        for z in (Z0 + 0.25, Z0 + 1.55):
            tube(p, 'Metal#fence', [(a[0], a[1], z), (b[0], b[1], z)], 0.025, seg=6)
        n = int(L / 0.25)
        for k in range(n + 1):
            t = k / n
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            p.cyl('Trim#mesh', (x, y, Z0 + 0.9), 'z', 0.012, 1.3, seg=4, bevel=0.0)
    Fs = WallFace('+x', fx1 + 0.04, 0.0)
    wbox(p, 'White#sign', Fs, -4.1, -3.3, Z0 + 1.0, Z0 + 1.5, 0.0, 0.02, bevel=0.0)
    wbox(p, 'Red', Fs, -3.95, -3.45, Z0 + 1.08, Z0 + 1.42, 0.02, 0.03, bevel=0.0)


def control_room(p):
    """A casa de controle: 1 andar de painel com as janelas, a porta com a marquise, o condensador e a antena no teto."""
    x0, x1, y0, y1 = 1.4, 6.6, 4.0, 7.6
    door = (5.3, 6.3, FL + 0.3, FL + 2.5)
    ru_block(p, x0, x1, y0, y1, Z0, 3.5, t=T, pw=2.6, zs=[3.5], parapet=0.5, holes={'-x': [door]},
             win={'+x': dict(floors=[Z0], w=1.4, lit=[(0, 0)]), '-y': dict(floors=[Z0], w=1.4), '-x': dict(floors=[Z0], w=1.4, skip=[(0, 0)])},
             interior=True)
    entry_door(p, WallFace('-x', x0, 0.0), door[0], door[1], door[2], door[3], t=T, canopy=1.0)
    ac_box(p, 2.6, 6.6, 3.55)
    flag_pole(p, 2.2, 4.8, 3.55, 4.2, cloth=(1.4, 0.9), phase=0.8)          # a bandeira da cor do jogador no teto
    lattice_mast(p, (5.6, 6.8), 3.55, 8.2, 0.22, 0.12, 5, m='Paint#mast', leg=0.035, brace=0.02)
    p.cyl('Beacon', (5.6, 6.8, 8.3), 'z', 0.07, 0.14, seg=10, bevel=0.0, smooth=True)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    pavilions(p)
    chimney(p)
    gas_line(p)
    yard(p)
    control_room(p)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, EAVE + RISE + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 14 x 16 m; chamine em', CHIM, 'ate', round(Z0 + CHIM_H, 2), 'm; pavilhoes ate', EAVE + RISE, 'm')
    print('MODEL fire_points_m', [(-4.0, -5.2, EAVE + RISE + 0.1), (-0.6, -0.2, EAVE + RISE + 0.1), (3.6, 5.8, 3.6)])


if __name__ == '__main__':
    main()
