"""A Fábrica de tanques da Rússia (War Factory) provisória, modelada no Blender por script (a facção nova, 01/11/2026:
docs/FACCAO_RUSSIA.md §5.1).

Rodar (Blender sem janela, só CPU), com o detalhe AAA (o chanfro redondo e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_war_factory.py C:/Users/luisf/explosion/captures/game/models/russia_war_factory

A planta: o pavilhão de montagem soviético (a referência é a Uralvagonzavod, a fábrica do T-90 em Nizhny Tagil, e os pavilhões
industriais de painel dos anos 70–80): o salão comprido de painéis pré-moldados (3,2 × 1,6 m, as juntas fundas escuras), o
TELHADO-SERRA de chapa trapezoidal verde com as vidraças do shed, o portão alto de correr com as 2 folhas verdes abertas, os trilhos que
saem do salão pelo pátio de lajes e o PÓRTICO de treliça por cima deles, com os holofotes e a placa.

A palavra (o que lê de cima): O PAVILHÃO DE TELHADO-SERRA VERDE com os trilhos saindo pela frente. De cima, as 6 faixas verdes da serra
(o que a China e a USA não têm); de lado, o perfil dos dentes; de frente, o frontão com a estrela contornada e o letreiro.

Peça por peça:
- o salão (17 × 13 m, beiral a 7,2 m, a serra até 8,55 m): oco, paredes de painel de 0,35 m com o soco, o portão (6,4 × 5,7 m) com as
  ombreiras de aço e o perigo, o frontão de painel com a estrela vermelha contornada e o anel branco, o letreiro estêncil na verga, as
  folhas de correr de chapa verde no trilho de cima, a fita de janelas no alto da lateral (−Y), a porta e a escada de marinheiro nos fundos;
  por dentro, os pilares, as vigas, as tesouras de cada dente, as luminárias penduradas, a ponte rolante e o casco em montagem nos
  trilhos (o que a câmera vê pelo portão);
- o pátio de saída (as lajes de concreto chanfradas com os trilhos embutidos) e o pórtico de treliça (as colunas de aço, a viga treliçada,
  os holofotes, a placa vermelha com o letreiro);
- o prédio da administração (o АБК, +Y): 2 andares de painel com as janelas de caixilho branco, a entrada com a marquise, o teto plano
  com as saídas de ventilação e, na borda da frente, o LETREIRO DE TELHADO em letras vermelhas grandes;
- o telheiro (−Y): as colunas, o telhado de uma água de chapa verde, embaixo as pilhas cobertas com a lona da cor do jogador, as rodas de
  apoio empilhadas, as caixas de munição verdes; o contêiner e o mastro de holofotes no canto; a chaminé de aço estaiada nos fundos;
- as 3 bandeiras da cor do jogador na frente da administração.

Lote 26 × 28 m (X × Y), frente em +X (o portão de onde o tanque sai pelos trilhos). Alturas: o salão 7,2 m no beiral, a serra 8,55 m, o
frontão 9,8 m, o pórtico 8,3 m, a administração 6,6 m (o letreiro até 7,6 m), o mastro de holofotes 9,7 m, a chaminé de aço 11 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_WarFactory_Base: tudo (nada se mexe; as bandeiras saem no flags.json, o jogo desenha o pano no vento);
  SM_Russia_WarFactory_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia, model_russia_parts.py): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard,
Wood, Cloth, Rubber; e Yellow (a ponte rolante de dentro).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp, barrel  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, splits, hexa  # noqa: E402
from model_russia_parts import (RuPart, ru_block, ru_panel_wall, ru_star, stencil, sign_board, roof_slogan, sawtooth_roof,  # noqa: E402
                                sliding_leaf, ribbon_glazing, entry_door, prof_sheet, floodlight_mast, flag_pole, container,
                                tarp_stack, ammo_crate, ladder_on, vent_shaft, ac_box, stroke, ru_foundation)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_war_factory')
PREFIX = 'SM_Russia_WarFactory'

HX, HY = 13.0, 14.0                 # o lote 26 × 28
Z0 = 0.14                           # o topo da fundação
FL = Z0 + 0.12                      # o piso do salão e do pátio de lajes
T = 0.35                            # a espessura das paredes de painel
# o salão
X0, X1, Y0, Y1 = -11.5, 5.5, -6.5, 6.5
EAVE = 7.2                          # o beiral (o pé da serra)
TEETH, RISE = 6, 1.35               # os dentes da serra (2,83 m cada, a água de 3,1 m: a grade corta em 8) e a subida
DOOR, DOOR_Z = 3.2, 6.0             # o portão: meia-largura e o topo (6,4 × 5,74 m de vão livre acima do piso)
PAR = 0.7                           # a platibanda da frente
RAIL_Y = (-1.5, 1.5)                # os trilhos (a bitola larga do carro de transferência)
PORTAL_X = 10.2                     # o pórtico
# a administração (+Y) e o telheiro (−Y)
AX0, AX1, AY0, AY1, AZ = -11.5, 1.5, 7.0, 13.2, 6.0
SX0, SX1, SY = -11.2, 3.2, -12.45


# ===================================================================================================== o salão

def hall(p):
    """O salão de painéis (oco) e a frente: o portão, as folhas de correr, o frontão, a estrela, o letreiro."""
    F = WallFace('+x', X1, 0.0)
    ru_block(p, X0, X1, Y0, Y1, Z0, EAVE, t=T, pw=3.2, parapet=PAR, par_faces=('+x',), roof=None,
             holes={'+x': [(-DOOR, DOOR, Z0, DOOR_Z)], '-y': [(-10.3, 4.3, 5.56, 6.76)], '-x': [(-1.0, 1.0, FL + 0.02, FL + 2.4)]},
             zs={'+x': [2.36, 3.96, DOOR_Z], '-x': [2.36, 3.96, 5.56], '+y': [2.36, 3.96, 5.56], '-y': [2.36, 3.96, 5.56, 6.76]},
             cols=[(x, y) for x in (-8.1, -4.7, -1.3, 2.1) for y in (-4.7, 4.7)], step=8.5, along='x')
    ribbon_glazing(p, WallFace('-y', Y0, 0.0), -10.3, 4.3, 5.56, 6.76, t=T, step=1.6, rows=1)
    # a platibanda da frente vai até as quinas (as faces ±y param no beiral)
    for (a, b) in ((Y0 - 0.08, Y0 + T), (Y1 - T, Y1 + 0.08)):
        wbox(p, 'Concrete#pnlc', F, a, b, EAVE + 0.003, EAVE + PAR, -T, 0.08, bevel=0.1)
        wbox(p, 'Paint#flash', F, a - 0.04, b + 0.04, EAVE + PAR - 0.02, EAVE + PAR + 0.05, -T - 0.05, 0.15, bevel=0.02)
        wbox(p, 'Paint#flash', F, a - 0.04, b + 0.04, EAVE + PAR - 0.14, EAVE + PAR, 0.11, 0.15, bevel=0.0)
    # o frontão (o painel do meio sobe 1,8 m acima da platibanda) com o rufo, a estrela contornada e o anel branco
    zf0, zf1 = EAVE + PAR + 0.05, EAVE + PAR + 1.85
    ru_panel_wall(p, F, [-4.4, -1.45, 1.45, 4.4], [zf0, zf1], t=T)
    wbox(p, 'Paint#flash', F, -4.46, 4.46, zf1 - 0.02, zf1 + 0.05, -T - 0.05, 0.15, bevel=0.02)
    wbox(p, 'Paint#flash', F, -4.46, 4.46, zf1 - 0.14, zf1, 0.11, 0.15, bevel=0.0)
    for s in (-1, 1):                                                            # os ombros do frontão (o degrau)
        a, b = (4.44, 5.4) if s > 0 else (-5.4, -4.44)
        wbox(p, 'Concrete#pnl', F, a, b, zf0, zf0 + 0.6, -T, 0.08, bevel=0.1)
        wbox(p, 'Paint#flash', F, a - 0.04, b + 0.04, zf0 + 0.58, zf0 + 0.65, -T - 0.05, 0.15, bevel=0.02)
    zc = (zf0 + zf1) / 2
    ru_star(p, F, 0.0, zc - 0.04, 0.52, n=0.08, t=0.03, w=0.11)
    p.torus('White#ring', F.pt(0.0, zc, 0.12), 'x', 0.8, 0.05, seg=40, rseg=6)
    # o letreiro estêncil na platibanda, em cima do portão (a "oficina" com o número; letras que não formam palavra)
    stencil(p, F, 0.0, EAVE + 0.1, 'ЦЖШ-7', h=0.5, m='White', n=0.08)
    # as ombreiras de aço do vão (com o perigo no pé) e a soleira
    for s in (-1, 1):
        a, b = (DOOR, DOOR + 0.28) if s > 0 else (-DOOR - 0.28, -DOOR)
        wbox(p, 'Paint#jamb', F, a, b, Z0, DOOR_Z + 0.1, -T, 0.18, bevel=0.03)
        wbox(p, 'Hazard', F, a - 0.01, b + 0.01, FL, FL + 1.5, 0.15, 0.21, bevel=0.0)
    wbox(p, 'Paint#jamb', F, -DOOR - 0.28, DOOR + 0.28, DOOR_Z - 0.02, DOOR_Z + 0.3, -T, 0.18, bevel=0.03)
    # o trilho de cima das folhas (a viga de aço nas mãos-francesas), a guia de baixo e as 2 folhas abertas pros lados
    wbox(p, 'Paint#track', F, -6.85, 6.85, DOOR_Z + 0.32, DOOR_Z + 0.6, 0.08, 0.5, bevel=0.03)
    for u in splits(-6.6, 6.6, 1.65):
        wbox(p, 'Metal#brk', F, u - 0.06, u + 0.06, DOOR_Z + 0.25, DOOR_Z + 0.75, 0.0, 0.42, bevel=0.0)
    wbox(p, 'Metal#guide', F, -6.85, 6.85, Z0, FL + 0.06, 0.1, 0.34, bevel=0.0)
    for (a, b) in ((DOOR + 0.12, 6.6), (-6.6, -DOOR - 0.12)):
        sliding_leaf(p, F, a, b, FL + 0.08, DOOR_Z + 0.2, 0.3, star=False)
    # as luminárias da frente
    for u in (-5.2, 5.2):
        lamp(p, X1 + 0.08, u, 5.3, face='+x')
    lamp(p, X1 + 0.08, 0.0, DOOR_Z + 1.0, face='+x')


def roof(p):
    """O telhado-serra (6 dentes) com os exaustores redondos e a escada pro telhado nos fundos."""
    sawtooth_roof(p, X0, X1, Y0, Y1, EAVE, TEETH, RISE, t=T, vents=[(0, -3.0), (1, 2.5), (2, -3.5), (3, 3.0)], front=T)
    ladder_on(p, WallFace('-x', X0, 0.0), -4.2, Z0, EAVE)
    ladder_on(p, WallFace('-x', X0, 0.0), 3.6, Z0 + 2.6, EAVE)


def interior(p):
    """O que se vê pelo portão: os trilhos que entram, o casco em montagem no carro de transferência, a ponte rolante amarela com a
    torre pendurada no gancho, as luminárias penduradas."""
    # os trilhos dentro (até o fundo) e o casco do tanque em montagem sobre eles
    for y in RAIL_Y:
        p.box('Metal#rail', X0 + 0.6, X1, y - 0.04, y + 0.04, FL - 0.01, FL + 0.05, bevel=0.0)
    hx, hz = -0.2, FL + 0.55
    p.box('Metal#cart', hx - 2.8, hx + 2.8, -1.85, 1.85, FL + 0.05, FL + 0.32, bevel=0.03)                       # o carro
    for xx in (hx - 2.2, hx + 2.2):
        for y in RAIL_Y:
            p.cyl('Metal#cart', (xx, y, FL + 0.17), 'y', 0.17, 0.12, seg=12, bevel=0.0)
    p.box('Paint#hull', hx - 3.0, hx + 2.6, -1.55, 1.55, hz, hz + 0.95, bevel=0.06)                            # o casco
    p.prism_xz('Paint#hullg', [(hx + 2.59, hz), (hx + 3.5, hz + 0.45), (hx + 3.4, hz + 0.95), (hx + 2.59, hz + 0.95)], -1.54, 1.54, bevel=0.06)
    p.box('Paint#hull', hx - 3.0, hx + 2.9, -1.8, 1.8, hz + 0.95, hz + 1.12, bevel=0.03)                        # os para-lamas
    for s in (-1, 1):
        for k in range(6):
            xx = hx - 2.4 + k * 0.95
            p.cyl('Rubber#wheel', (xx, s * 1.42, hz + 0.05), 'y', 0.36, 0.22, seg=18, bevel=0.0)
            p.cyl('Metal#hub', (xx, s * 1.54, hz + 0.05), 'y', 0.14, 0.06, seg=10, bevel=0.0)
    p.box('Dark', hx - 0.2, hx + 1.4, -0.9, 0.9, hz + 1.1, hz + 1.14, bevel=0.0)                                # o anel da torre
    # a ponte rolante (as vigas de caminho nas paredes, a ponte, o carro, o gancho e a torre pendurada)
    zc = 6.2
    for y in (Y0 + T + 0.25, Y1 - T - 0.25):
        p.box('Metal#runway', X0 + T, X1 - T, y - 0.15, y + 0.15, zc - 0.5, zc - 0.2, bevel=0.02)
    bx = -0.2
    p.box('Yellow#crane', bx - 0.35, bx + 0.35, Y0 + T, Y1 - T, zc - 0.2, zc + 0.45, bevel=0.04)
    p.box('Yellow#crane', bx - 0.6, bx + 0.6, Y0 + T + 0.05, Y0 + T + 0.6, zc - 0.25, zc + 0.5, bevel=0.03)
    p.box('Yellow#crane', bx - 0.6, bx + 0.6, Y1 - T - 0.6, Y1 - T - 0.05, zc - 0.25, zc + 0.5, bevel=0.03)
    p.box('Metal#hoist', bx - 0.55, bx + 0.55, -0.6, 0.6, zc + 0.45, zc + 0.9, bevel=0.03)
    for dy in (-0.15, 0.15):
        p.cyl('Dark#cable', (bx, dy, zc - 1.0), 'z', 0.02, 2.4, seg=4, bevel=0.0)
    p.box('Hazard', bx - 0.2, bx + 0.2, -0.3, 0.3, zc - 2.5, zc - 2.15, bevel=0.0)
    tz = zc - 3.3                                                                 # a torre (a cunha baixa) pendurada
    p.loft('Paint#hull', [[(bx - 1.3, -1.25, tz), (bx + 1.0, -1.3, tz), (bx + 1.6, 0.0, tz), (bx + 1.0, 1.3, tz), (bx - 1.3, 1.25, tz)],
                          [(bx - 1.1, -1.0, tz + 0.7), (bx + 0.8, -1.05, tz + 0.7), (bx + 1.25, 0.0, tz + 0.7), (bx + 0.8, 1.05, tz + 0.7),
                           (bx - 1.1, 1.0, tz + 0.7)]], bevel=0.05)
    p.cyl('Paint#hull', (bx + 2.6, 0.0, tz + 0.3), 'x', 0.1, 2.2, seg=12, bevel=0.0)
    # as luminárias penduradas em 2 fileiras
    for x in (-9.8, -6.4, -3.0, 0.4, 3.6):
        for y in (-3.2, 3.2):
            p.cyl('Dark#lamp', (x, y, 6.55), 'z', 0.012, 0.8, seg=4, bevel=0.0)
            p.cyl('Dark#lamp', (x, y, 6.05), 'z', 0.32, 0.22, seg=14, r2=0.1, bevel=0.0)
            p.cyl('Light', (x, y, 5.93), 'z', 0.26, 0.02, seg=14, bevel=0.0)
    # o fundo: a porta de dentro acesa e as bancadas
    Fb = WallFace('+x', X0 + T, 0.0)
    wbox(p, 'Dark', Fb, -1.0, 1.0, FL, FL + 2.4, 0.0, 0.02, bevel=0.0)
    for (y0, y1) in ((-5.6, -3.2), (3.2, 5.6)):
        p.box('Metal#bench', -9.5, -6.5, y0, y1, FL + 0.85, FL + 0.95, bevel=0.02)
        for xx in (-9.4, -6.6):
            for yy in (y0 + 0.1, y1 - 0.1):
                p.box('Metal#bench', xx - 0.04, xx + 0.04, yy - 0.04, yy + 0.04, FL, FL + 0.85, bevel=0.0)


def back(p):
    """Os fundos: a porta de aço na parede de trás e a chaminé de aço estaiada no canto (−X, −Y)."""
    Fb = WallFace('-x', X0, 0.0)
    entry_door(p, Fb, -1.0, 1.0, FL + 0.02, FL + 2.4, t=T, canopy=1.0)
    cx, cy = -12.15, -11.9
    p.cyl('Concrete#stackb', (cx, cy, Z0 + 0.3), 'z', 0.75, 0.6, seg=20, bevel=0.05)
    p.cyl('Metal#stack', (cx, cy, Z0 + 0.6 + 5.2), 'z', 0.36, 10.4, seg=20, r2=0.32, bevel=0.0, smooth=True)
    for zz in (3.5, 7.0, 10.2):
        p.torus('Metal#stack', (cx, cy, zz), 'z', 0.38, 0.05, seg=20, rseg=6)
    p.torus('Dark#soot', (cx, cy, 11.05), 'z', 0.32, 0.07, seg=20, rseg=6)
    p.cyl('Dark', (cx, cy, 10.95), 'z', 0.28, 0.3, seg=20, bevel=0.0)
    for k in range(3):                                                            # os estais
        a = math.radians(30 + 120 * k)
        ax, ay = cx + 3.0 * math.cos(a), cy + 3.0 * math.sin(a)
        ax, ay = max(-HX + 0.4, min(HX - 0.4, ax)), max(-HY + 0.4, min(HY - 0.4, ay))
        cable(p, (cx + 0.35 * math.cos(a), cy + 0.35 * math.sin(a), 8.8), (ax, ay, Z0 + 0.2), sag=0.15, r=0.018, m='Metal#guy')
        p.box('Concrete#anchor', ax - 0.25, ax + 0.25, ay - 0.25, ay + 0.25, Z0, Z0 + 0.25, bevel=0.04)
    # o duto que sai do telheiro e entra na chaminé
    tube(p, 'Metal#duct', [(-10.4, -12.0, 2.6), (-11.6, -12.0, 2.6), (-11.9, -11.8, 2.6)], 0.22, seg=12)


# ===================================================================================================== o pátio e o pórtico

def yard(p):
    """O pátio de saída em lajes de concreto (chanfradas, com a junta) e os trilhos embutidos até a borda do lote; o pórtico de treliça
    com os holofotes e a placa vermelha."""
    xs = [X1 + 0.12, 7.4, 9.4, 11.4, HX - 0.1]
    ys = [(-4.6, RAIL_Y[0] - 0.12), (RAIL_Y[0] + 0.12, RAIL_Y[1] - 0.12), (RAIL_Y[1] + 0.12, 4.6)]
    g = 0.025
    for (xa, xb) in zip(xs[:-1], xs[1:]):
        for (ya, yb) in ys:
            p.box('Concrete#slab', xa + g, xb - g, ya + g, yb - g, Z0, FL, bevel=0.04)
    for y in RAIL_Y:                                                               # o trilho embutido (o boleto e o sulco)
        p.box('Dark', X1, HX - 0.1, y - 0.12, y + 0.12, Z0, FL - 0.03, bevel=0.0)
        p.box('Metal#rail', X1, HX - 0.1, y - 0.045, y + 0.045, Z0, FL + 0.035, bevel=0.0)
        p.box('Metal#rail', X1, HX - 0.1, y + 0.06, y + 0.1, Z0, FL - 0.005, bevel=0.0)
    # o pórtico: as 2 colunas-caixão nos pés de concreto, a viga treliçada, os holofotes, a placa
    zt0, zt1 = 7.2, 8.1
    for s in (-1, 1):
        y = s * 4.85
        p.box('Concrete#pier', PORTAL_X - 0.6, PORTAL_X + 0.6, y - 0.55, y + 0.55, Z0, FL + 0.55, bevel=0.06)
        p.box('Paint#portal', PORTAL_X - 0.25, PORTAL_X + 0.25, y - 0.3, y + 0.3, FL + 0.55, zt1, bevel=0.04)
        p.box('Hazard', PORTAL_X - 0.27, PORTAL_X + 0.27, y - 0.32, y + 0.32, FL + 0.6, FL + 2.0, bevel=0.0)
        for zz in (zt0 - 1.1, zt0 - 0.55):                                          # as mãos-francesas
            tube(p, 'Paint#portal', [(PORTAL_X, y - s * 0.3, zz), (PORTAL_X, y - s * (0.3 + zt0 - zz), zt0)], 0.07, seg=8)
    for zz in (zt0, zt1):
        p.box('Paint#portal', PORTAL_X - 0.22, PORTAL_X + 0.22, -5.15, 5.15, zz - 0.14, zz + 0.14, bevel=0.03)
    for k, y in enumerate(splits(-4.55, 4.55, 0.9)):
        tube(p, 'Paint#portal', [(PORTAL_X, y, zt0), (PORTAL_X, y, zt1)], 0.05, seg=6)
        if k < len(splits(-4.55, 4.55, 0.9)) - 1:
            y2 = splits(-4.55, 4.55, 0.9)[k + 1]
            a, b = ((y, zt0), (y2, zt1)) if k % 2 == 0 else ((y, zt1), (y2, zt0))
            tube(p, 'Paint#portal', [(PORTAL_X, a[0], a[1]), (PORTAL_X, b[0], b[1])], 0.04, seg=6)
    Fp = WallFace('+x', PORTAL_X + 0.22, 0.0)
    sign_board(p, Fp, 0.0, zt0 + 0.08, 'ГЭЦ-17', h=0.5, board='Red', ink='White', n=0.0, pad=0.12)
    for y in (-3.6, 3.6):                                                         # os holofotes em cima da viga, virados pro pátio
        for dy in (-0.35, 0.35):
            q = (PORTAL_X - 0.15, y + dy, zt1 + 0.32)
            p.obox('Dark#lamp', q, (0.3, 0.32, 0.32), yaw=180, pitch=-35, bevel=0.012)
            p.obox('Light', (q[0] - 0.16, q[1], q[2] - 0.08), (0.03, 0.26, 0.26), yaw=180, pitch=-35, bevel=0.0)
            p.box('Metal#brk', q[0] - 0.05, q[0] + 0.05, q[1] - 0.05, q[1] + 0.05, zt1 + 0.14, q[2] - 0.12, bevel=0.0)
    for y in RAIL_Y:                                                              # os batentes no fim dos trilhos
        p.box('Hazard', HX - 0.6, HX - 0.15, y - 0.3, y + 0.3, FL, FL + 0.55, bevel=0.03)
        p.box('Dark', HX - 0.61, HX - 0.14, y - 0.31, y + 0.31, FL + 0.2, FL + 0.32, bevel=0.0)


# ===================================================================================================== a administração e o telheiro

def office(p):
    """O prédio da administração (2 andares de painel, as janelas, a entrada com a marquise) com o letreiro de telhado."""
    door = (7.95, 9.55, FL + 0.3, FL + 2.5)
    ru_block(p, AX0, AX1, AY0, AY1, Z0, AZ, t=T, skin=('+x', '+y', '-x'), pw=3.0, zs=[3.34, AZ], parapet=0.6,
             holes={'+x': [door]}, floors=[3.34],
             win={'+x': dict(floors=[Z0, 3.34], skip=[(0, 0)], lit=[(1, 1)]), '+y': dict(floors=[Z0, 3.34], lit=[(1, 0), (3, 1)])})
    entry_door(p, WallFace('+x', AX1, 0.0), door[0], door[1], door[2], door[3], t=T, canopy=1.3)
    # o teto: as saídas de ventilação, o condensador, o alçapão, a antena
    for (x, y) in ((-9.0, 9.0), (-4.5, 11.8), (-1.5, 8.6)):
        vent_shaft(p, x, y, AZ + 0.05)
    ac_box(p, -7.0, 11.6, AZ + 0.05)
    p.box('Paint#cap', -10.6, -9.8, 11.6, 12.4, AZ + 0.05, AZ + 0.32, bevel=0.02)
    roof_slogan(p, AX1 - 0.65, (AY0 + AY1) / 2, AZ + 0.05, 'ГЖЦБЭ', h=1.0)


def shed(p):
    """O telheiro de uma água no lado −Y: as colunas, as vigas, o telhado de chapa verde descendo pra fora, a calha; embaixo as pilhas
    com a lona da cor do jogador, as rodas de apoio, as caixas de munição."""
    zh, zl = 5.15, 4.0
    xs = [SX0 + 0.2, -7.6, -4.0, -0.4, SX1 - 0.2]
    for x in xs:
        p.box('Concrete#pier', x - 0.3, x + 0.3, SY + 0.1 - 0.3, SY + 0.1 + 0.3, Z0, Z0 + 0.35, bevel=0.04)
        p.box('Paint#col', x - 0.13, x + 0.13, SY + 0.1 - 0.13, SY + 0.1 + 0.13, Z0 + 0.35, zl - 0.38, bevel=0.03)
        p.obox('Paint#col', (x, (Y0 - 0.1 + SY) / 2, (zh + zl) / 2 - 0.12), (0.14, Y0 - 0.1 - SY + 0.3, 0.24),
               roll=-math.degrees(math.atan2(zh - zl, Y0 - 0.1 - SY)), bevel=0.02)
        tube(p, 'Paint#col', [(x, SY + 0.1, zl - 1.0), (x, SY + 1.1, zl - 0.05 + 0.15)], 0.05, seg=6)
        p.box('Metal#brk', x - 0.2, x + 0.2, Y0 - 0.12, Y0 - 0.08, zh - 0.5, zh - 0.05, bevel=0.0)
    p.box('Paint#col', SX0, SX1, SY + 0.1 - 0.14, SY + 0.1 + 0.14, zl - 0.4, zl - 0.1, bevel=0.03)
    prof_sheet(p, 'Roof', Vector((SX0 - 0.1, Y0 - 0.06, zh + 0.04)), Vector((SX0 - 0.1, SY - 0.25, zl - 0.12)),
               Vector((SX1 + 0.1, Y0 - 0.06, zh + 0.04)), drip=0.06)
    wbox(p, 'Paint#flash', WallFace('-y', Y0, 0.0), SX0 - 0.1, SX1 + 0.1, zh, zh + 0.35, 0.0, 0.18, bevel=0.0)
    p.cyl('Paint#gut', ((SX0 + SX1) / 2, SY - 0.33, zl - 0.25), 'x', 0.09, SX1 - SX0 + 0.2, seg=10, bevel=0.0)
    tube(p, 'Paint#gut', [(SX1 - 0.1, SY - 0.33, zl - 0.25), (SX1 - 0.1, SY - 0.33, Z0 + 0.3)], 0.055, seg=8)
    # embaixo: as pilhas com a lona (a cor do jogador), as rodas de apoio empilhadas, as caixas de munição, os tambores
    tarp_stack(p, -8.6, -9.6, Z0, 2.6, 1.8, 1.25, yaw=3, seed=2)
    tarp_stack(p, -2.2, -9.3, Z0, 2.2, 1.6, 1.0, yaw=-4, seed=5)
    for (cx, cy, n) in ((-5.3, -10.9, 5), (-4.4, -11.6, 3)):
        for k in range(n):
            p.cyl('Rubber#wheel', (cx, cy, Z0 + 0.12 + k * 0.23), 'z', 0.38, 0.21, seg=18, bevel=0.0)
            p.cyl('Metal#hub', (cx, cy, Z0 + 0.12 + k * 0.23), 'z', 0.16, 0.23, seg=10, bevel=0.0)
    for (x, y, yaw, z) in ((1.0, -10.6, 8, Z0), (1.03, -10.58, 12, Z0 + 0.425), (2.2, -11.4, -5, Z0), (0.6, -11.8, 80, Z0)):
        ammo_crate(p, x, y, z, yaw=yaw)
    for (x, y) in ((-10.4, -12.1), (-9.75, -12.25), (-10.1, -11.5)):
        barrel(p, x, y, Z0, r=0.29, h=0.88, m='Paint#drum', ring='Dark')


def front_props(p):
    """As 3 bandeiras na frente da administração, o contêiner, a pilha coberta, o mastro de holofotes no canto."""
    for k, y in enumerate((6.4, 7.7, 9.0)):
        flag_pole(p, 11.3, y, Z0, 7.2, cloth=(1.7, 1.1), phase=0.7 * k)
    container(p, 8.4, -10.6, Z0, yaw=0.0, m='Paint#cont', doors='+x')
    tarp_stack(p, 8.0, -6.6, Z0, 2.4, 1.6, 1.1, yaw=90, seed=9)
    floodlight_mast(p, 12.0, -12.9, Z0, 9.3, face=165)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    hall(p)
    roof(p)
    interior(p)
    back(p)
    yard(p)
    office(p)
    shed(p)
    front_props(p)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, EAVE + 1.0, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 26 x 28 m; portao', 2 * DOOR, 'x', round(DOOR_Z - FL, 2), 'm em X', X1, '; trilhos Y', RAIL_Y, '; serra ate',
          EAVE + RISE, 'm')
    print('MODEL fire_points_m', [(-8.0, -3.5, 8.3), (-1.2, 3.8, 7.75), (-5.0, 10.2, AZ + 0.1)])


if __name__ == '__main__':
    main()
