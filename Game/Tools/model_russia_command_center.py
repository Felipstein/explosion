"""O Quartel-General da Rússia (Command Center, o Штаб) provisório, modelado no Blender por script (a facção nova, 01/11/2026:
docs/FACCAO_RUSSIA.md §5.1).

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_command_center.py C:/Users/luisf/explosion/captures/game/models/russia_command_center

A planta: o estado-maior soviético dos anos 70–80 (os prédios de painel em degraus dos ministérios e dos estados-maiores de distrito)
com a torre de rádio de Shukhov (a treliça hiperboloide da torre da rua Shabolovka, Moscou, 1922: a engenharia russa que todo mundo
reconhece), o portão verde de quartel com as estrelas e o posto de controle com a cancela listrada.

A palavra (o que lê de cima): O BLOCO DE PAINEL EM 3 DEGRAUS COM A TORRE DE SHUKHOV. De cima, os 3 retângulos de teto azul-acinzentado
um dentro do outro (os terraços), o círculo listrado da torre no canto com a antena girando, a estrela no frontão; de frente, o frontão
com a estrela contornada, a marquise da entrada, o portão verde com as estrelas e o letreiro vermelho no telhado.

Peça por peça:
- o bloco de baixo (2 andares, 13,8 × 24 m): painéis de 3 m com uma janela de caixilho branco em cada (o ritmo do prédio de painel),
  as juntas fundas escuras, o soco; na frente o corpo da entrada saliente (a porta de vidro, a fita vertical da escada, a marquise fina
  em balanço nas 2 colunas, os degraus) e a garagem do trator com o PORTÃO VERDE DE 2 FOLHAS COM AS ESTRELAS (aberto);
- o 2º degrau (1 andar, 9 × 15 m) e o 3º (a torre do estado-maior, 5 × 6 m) com o FRONTÃO: o painel do meio sobe 2 m com a estrela
  vermelha contornada de branco no anel branco; no teto do 3º, o mastro com a bandeira da cor do jogador;
- a TORRE DE SHUKHOV no teto do bloco de baixo (3 seções de hiperboloide em vermelho e branco, 9 m) com a plataforma e a antena
  (_Radar) de 6 Yagis que gira;
- no teto: a manta azul-acinzentada em faixas, as saídas de ventilação, os condensadores, as antenas, o LETREIRO de telhado vermelho;
- o pátio: o posto de controle (a guarita de painel com as janelas largas e o telhado de chapa verde), a CANCELA listrada levantada, o
  cogumelo da sentinela, a CERCA DE CONCRETO PO-2 com os losangos na frente e no lado −Y, as 3 bandeiras;
- por dentro: as lajes dos andares, os pilares, as vigas (o furo do dano mostra um prédio).

Lote 26 × 28 m (X × Y), frente em +X. Alturas: o bloco de baixo 6,6 m, o 2º degrau 9,8 m, o 3º 13,0 m, o frontão 14,9 m, a torre de
Shukhov 15,5 m e a antena até ~17,3 m; a bandeira do topo até ~17 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_CommandCenter_Base: tudo o que não se mexe (as bandeiras saem no flags.json, o jogo desenha o pano no vento);
  SM_Russia_CommandCenter_Radar: a antena de Yagis (gira em Z; origem no eixo, no topo da plataforma da torre: at_m = RADAR_AT, 6 rpm);
  SM_Russia_CommandCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Wood, Rubber; Beacon (a
luz de obstáculo no topo da antena).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp, disc_ring  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, splits, bake_in_place  # noqa: E402
from model_russia_parts import (RuPart, ru_block, ru_panel_wall, ru_star, stencil, roof_slogan, entry_door, prof_sheet, face_prism,  # noqa: E402
                                flag_pole, star_gate, po2_fence, boom_barrier, sentry_mushroom, shukhov_tower, vent_shaft, ac_box,
                                ladder_on, lattice_mast, ru_foundation)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_command_center')
PREFIX = 'SM_Russia_CommandCenter'

HX, HY = 13.0, 14.0                 # o lote 26 × 28
Z0 = 0.14                           # o topo da fundação
FL = Z0 + 0.12
T = 0.35
# os 3 degraus (x0, x1, y0, y1, z do piso, z do topo da laje)
B1 = (-11.8, 2.0, -12.0, 12.0, Z0, 6.0)
B2 = (-10.0, -1.0, -7.5, 7.5, 6.0, 9.2)
B3 = (-8.5, -3.5, -3.0, 3.0, 9.2, 12.4)
PAR = 0.6
F2 = 3.34                           # o piso do 2º andar do bloco de baixo
GARAGE = (-10.5, -5.9, FL, 4.0)     # o vão da garagem do trator (Y, Z)
SHU = (-9.2, 10.0)                  # o eixo da torre de Shukhov
SHU_SECTIONS = [(1.75, 1.15, 3.6, 62.0), (1.15, 0.78, 3.0, 62.0), (0.78, 0.55, 2.4, 62.0)]
SHU_Z0 = B1[5] + 0.32
RADAR_AT = (SHU[0], SHU[1], SHU_Z0 + sum(s[2] for s in SHU_SECTIONS) + 0.2)


# ===================================================================================================== os degraus

def lower(p):
    """O bloco de baixo: 2 andares de painel com as janelas, a garagem com o portão verde, o corpo da entrada com a marquise."""
    x0, x1, y0, y1, z0, z1 = B1
    gate_h = (GARAGE[0], GARAGE[1], Z0, GARAGE[3])
    ent_h = (-1.6, 1.6, Z0, F2 - 0.3)                    # o vão atrás do corpo da entrada
    # as colunas das janelas na frente: as juntas obrigatórias nas bordas da garagem e do corpo da entrada
    # a frente: o painel estreito da quina, as 2 colunas da garagem, 2 colunas, o corpo da entrada, 4 colunas
    ru_block(p, x0, x1, y0, y1, z0, z1, t=T, pw=3.0, pwf={'+x': 2.6}, zs=[F2, z1], parapet=PAR, floors=[F2], roof_skip=B2[:4],
             holes={'+x': [gate_h, ent_h]}, ukeep={'+x': [-10.5, -8.2, -5.9, -3.75, -1.6, 1.6]},
             win={'+x': dict(floors=[Z0, F2], skip=[(0, None), (1, None), (2, None), (5, None)], lit=[(7, 1), (3, 1)]),
                  '-y': dict(floors=[Z0, F2], lit=[(2, 1)]), '+y': dict(floors=[Z0, F2], lit=[(3, 0)])},
             cols=[(x, y) for x in (-7.5, -3.0) for y in (-6.0, 0.0, 6.0)], step=6.0, along='y')
    # a garagem: o fundo escuro, as luzes de dentro, a soleira e o portão verde de quartel com as estrelas (aberto pra fora)
    Fg = WallFace('+x', x1, 0.0)
    wbox(p, 'Dark', WallFace('+x', x1 - 5.5, 0.0), GARAGE[0], GARAGE[1], Z0, GARAGE[3], 0.0, 0.03, bevel=0.0)
    for y in (-9.2, -7.2):
        p.box('Dark#lamp', x1 - 3.2, x1 - 2.2, y - 0.12, y + 0.12, GARAGE[3] - 0.25, GARAGE[3] - 0.15, bevel=0.0)
        p.box('Light', x1 - 3.15, x1 - 2.25, y - 0.08, y + 0.08, GARAGE[3] - 0.27, GARAGE[3] - 0.25, bevel=0.0)
    wbox(p, 'Paint#jamb', Fg, GARAGE[0] - 0.1, GARAGE[1] + 0.1, GARAGE[3] - 0.02, GARAGE[3] + 0.3, -T, 0.16, bevel=0.03)
    star_gate(p, x1 + 0.12, GARAGE[0] + 0.05, GARAGE[1] - 0.05, h=GARAGE[3] - 0.5, open_deg=100.0)
    for y in (GARAGE[0] - 0.28, GARAGE[1] + 0.28):                               # o perigo nos pilares do portão
        p.box('Hazard', x1 + 0.4, x1 + 0.44, y - 0.24, y + 0.24, FL, FL + 1.0, bevel=0.0)
    lamp(p, x1 + 0.08, (GARAGE[0] + GARAGE[1]) / 2, GARAGE[3] + 0.75, face='+x')
    # o letreiro em cima da garagem (o painel cego entre o portão e a platibanda)
    stencil(p, Fg, (GARAGE[0] + GARAGE[1]) / 2, F2 + 1.55, 'ГЖ-04', h=0.6, m='White', n=0.08)
    # o corpo da entrada (saliente, sobe acima da platibanda): a porta de vidro, a fita vertical da escada, a marquise
    ru_block(p, x1, x1 + 1.8, -3.0, 3.0, Z0, z1 + 0.7, t=T, walls=('+x', '+y', '-y'), skin=('+x', '+y', '-y'), pw=2.0,
             zs=[F2, z1 + 0.7], parapet=0.5, roof='flat', interior=False,
             holes={'+x': [(-1.25, 1.25, Z0, 3.0), (-1.25, 1.25, 3.7, z1 + 0.2)]}, ukeep={'+x': [-1.25, 1.25]})
    Fe = WallFace('+x', x1 + 1.8, 0.0)
    # a porta de vidro (o caixilho de alumínio pintado, as folhas de vidro) e a fita da escada (o vidro alto com as travessas)
    wbox(p, 'Glass', Fe, -1.2, 1.2, FL + 0.32, 2.95, -0.2, -0.18, bevel=0.0)
    wbox(p, 'Concrete#step', Fe, -1.25, 1.25, Z0, FL + 0.32, -T, 0.0, bevel=0.0)
    for u in (-1.2, -0.4, 0.4, 1.2):
        wbox(p, 'White#win', Fe, u - 0.05, u + 0.05, FL + 0.32, 2.95, -0.24, -0.14, bevel=0.012)
    for z in (FL + 0.37, 2.2, 2.92):
        wbox(p, 'White#win', Fe, -1.2, 1.2, z - 0.05, z + 0.05, -0.24, -0.14, bevel=0.012)
    wbox(p, 'Glass', Fe, -1.2, 1.2, 3.75, z1 + 0.15, -0.2, -0.18, bevel=0.0)
    for u in (-1.2, 0.0, 1.2):
        wbox(p, 'White#win', Fe, u - 0.05, u + 0.05, 3.7, z1 + 0.2, -0.24, -0.14, bevel=0.012)
    for z in splits(3.75, z1 + 0.15, 0.62):
        wbox(p, 'White#win', Fe, -1.2, 1.2, z - 0.04, z + 0.04, -0.24, -0.14, bevel=0.012)
    for z in (F2 + 0.9, F2 + 2.4):                                               # os patamares que aparecem pelo vidro
        p.box('Concrete#int', x1 + 0.4, x1 + 1.4, -1.2, 1.2, z - 0.08, z + 0.08, bevel=0.0)
    p.box('Light', x1 + 0.6, x1 + 1.0, -0.3, 0.3, z1 - 0.15, z1 - 0.1, bevel=0.0)
    # a marquise fina em balanço (o козырёк) nas 2 colunas redondas, os degraus
    xm = x1 + 1.8
    p.box('Concrete#canopy', xm - 0.1, xm + 2.8, -3.4, 3.4, 3.15, 3.38, bevel=0.06)
    p.box('Paint#flash', xm - 0.1, xm + 2.84, -3.44, 3.44, 3.36, 3.42, bevel=0.0)
    for y in (-2.9, 2.9):
        p.cyl('White#col', (xm + 2.4, y, (FL + 3.15) / 2), 'z', 0.16, 3.15 - FL, seg=16, bevel=0.0, smooth=True)
        p.cyl('Concrete#colb', (xm + 2.4, y, FL + 0.08), 'z', 0.24, 0.16, seg=16, bevel=0.0)
    for k in range(2):
        p.box('Concrete#step', xm - 0.05, xm + 1.4 - k * 0.5, -2.4 + k * 0.15, 2.4 - k * 0.15, Z0, FL + 0.16 * (k + 1), bevel=0.04)
    for y in (-2.0, 2.0):
        p.cyl('Dark#lamp', (xm + 1.3, y, 3.1), 'z', 0.16, 0.06, seg=12, bevel=0.0)
        p.cyl('Light', (xm + 1.3, y, 3.065), 'z', 0.12, 0.02, seg=12, bevel=0.0)
    # o teto do bloco de baixo: as saídas de ventilação, os condensadores, o alçapão, o letreiro vermelho
    for (x, y) in ((-10.6, -10.6), (-6.0, -10.8), (-1.6, -10.4), (-10.6, 6.0), (-1.8, 10.5)):
        vent_shaft(p, x, y, z1 + 0.05)
    ac_box(p, -4.0, 10.0, z1 + 0.05)
    ac_box(p, -5.6, -10.4, z1 + 0.05, yaw=90)
    p.box('Paint#cap', -1.0, -0.2, 10.4, 11.2, z1 + 0.05, z1 + 0.33, bevel=0.02)
    roof_slogan(p, x1 - 0.7, 7.6, z1 + 0.05, 'ДШЦЛЯ', h=1.0)


def upper(p):
    """O 2º e o 3º degraus (os terraços), o frontão do 3º com a estrela contornada, o mastro com a bandeira."""
    x0, x1, y0, y1, z0, z1 = B2
    ru_block(p, x0, x1, y0, y1, z0, z1, t=T, pw=3.0, zs=[z1], parapet=PAR, socle=0.0, roof_skip=B3[:4],
             win={'+x': dict(floors=[z0], lit=[(1, 0)]), '-y': dict(floors=[z0]), '+y': dict(floors=[z0])},
             cols=[((x0 + x1) / 2, 0.0)], step=4.5)
    for (x, y) in ((-8.6, -6.4), (-2.2, 6.3)):
        vent_shaft(p, x, y, z1 + 0.05, sx=0.8, sy=0.6, h=0.7)
    x0, x1, y0, y1, z0, z1 = B3
    ru_block(p, x0, x1, y0, y1, z0, z1, t=T, pw=2.0, zs=[z1], parapet=PAR, socle=0.0, interior=True,
             win={'+x': dict(floors=[z0], w=1.0, skip=[(1, 0)]), '-y': dict(floors=[z0], w=1.2), '+y': dict(floors=[z0], w=1.2, lit=[(0, 0)])})
    # o frontão: o painel do meio sobe 2 m acima da platibanda, os ombros em degrau, a estrela contornada no anel branco
    F = WallFace('+x', x1, 0.0)
    zf0, zf1 = z1 + PAR + 0.05, z1 + PAR + 1.95
    ru_panel_wall(p, F, [-1.7, 1.7], [zf0, zf1], t=T)
    wbox(p, 'Paint#flash', F, -1.76, 1.76, zf1 - 0.02, zf1 + 0.05, -T - 0.05, 0.15, bevel=0.02)
    wbox(p, 'Paint#flash', F, -1.76, 1.76, zf1 - 0.14, zf1, 0.11, 0.15, bevel=0.0)
    for (a, b) in ((-2.7, -1.74), (1.74, 2.7)):
        wbox(p, 'Concrete#pnl', F, a, b, zf0, zf0 + 0.7, -T, 0.08, bevel=0.1)
        wbox(p, 'Paint#flash', F, a - 0.04, b + 0.04, zf0 + 0.68, zf0 + 0.75, -T - 0.05, 0.15, bevel=0.02)
    zc = (zf0 + zf1) / 2 - 0.05
    ru_star(p, F, 0.0, zc, 0.56, n=0.08, t=0.03, w=0.11)
    p.torus('White#ring', F.pt(0.0, zc + 0.04, 0.12), 'x', 0.85, 0.055, seg=40, rseg=6)
    lamp(p, x1 + 0.08, 0.0, z1 - 0.45, face='+x')
    # o mastro da bandeira no teto do 3º degrau (a bandeira da cor do jogador, a maior do prédio)
    flag_pole(p, -7.4, 0.0, z1 + 0.05, 4.4, cloth=(2.0, 1.3), phase=0.3)
    # a escada de marinheiro do 2º pro 3º e do 1º pro 2º (nos fundos)
    ladder_on(p, WallFace('-x', B3[0], 0.0), -2.0, B2[5] + 0.05, z1 + PAR - 0.2, cage=False)
    ladder_on(p, WallFace('-x', B2[0], 0.0), 5.6, B1[5] + 0.05, B2[5] + PAR - 0.2, cage=False)


def radio_tower(p):
    """A torre de Shukhov no teto do bloco de baixo: a sapata redonda, as 3 seções listradas, a plataforma de cima com o guarda-corpo e
    as luzes, a escada pelo meio, os cabos que descem pro teto."""
    cx, cy = SHU
    z0 = B1[5] + 0.05
    disc_ring(p, 'Concrete#shub', SHU, 2.05, 1.45, z0, SHU_Z0, seg=40)
    for k in range(6):
        a = 2 * math.pi * k / 6
        p.box('Metal#shub', cx + 1.75 * math.cos(a) - 0.18, cx + 1.75 * math.cos(a) + 0.18, cy + 1.75 * math.sin(a) - 0.18,
              cy + 1.75 * math.sin(a) + 0.18, SHU_Z0 - 0.02, SHU_Z0 + 0.12, bevel=0.0)
    zt = shukhov_tower(p, SHU, SHU_Z0, SHU_SECTIONS, n=14, r_tube=0.06, ring_r=0.075, rings_per=3)
    # a plataforma do topo
    disc_ring(p, 'Metal#plat', SHU, 1.0, 0.25, zt - 0.02, zt + 0.08, seg=32)
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        p.cyl('Metal#rail', (cx + 0.95 * math.cos(a), cy + 0.95 * math.sin(a), zt + 0.55), 'z', 0.025, 0.95, seg=6, bevel=0.0)
    p.torus('Metal#rail', (cx, cy, zt + 1.0), 'z', 0.95, 0.03, seg=32, rseg=6)
    p.cyl('Metal#plat', (cx, cy, zt + 0.14), 'z', 0.32, 0.12, seg=20, bevel=0.0)
    # a escada pelo meio da torre (a coluna central com os degraus) e o cabo de antena que desce
    p.cyl('Metal#core', (cx, cy, (SHU_Z0 + zt) / 2), 'z', 0.12, zt - SHU_Z0, seg=10, bevel=0.0)
    cable(p, (cx + 0.2, cy - 0.1, zt), (-5.5, 11.2, B1[5] + 0.08), sag=1.2, r=0.025, m='Dark#cable')
    p.box('Paint#box', -5.9, -5.1, 10.8, 11.6, B1[5] + 0.05, B1[5] + 0.9, bevel=0.04)


def yard(p):
    """O pátio: o posto de controle com a cancela, o cogumelo, a cerca PO-2, as bandeiras, os holofotes de rodapé."""
    # o posto de controle: a guarita de painel com as janelas largas e o telhado de uma água de chapa verde
    gx0, gx1, gy0, gy1 = 9.6, 12.5, -13.5, -10.7
    ru_block(p, gx0, gx1, gy0, gy1, Z0, 2.9, t=0.25, pw=2.3, zs=[2.9], parapet=0.0, socle=0.45, roof='slab', interior=False,
             holes={'-x': [(-12.5, -11.6, FL + 0.2, 2.3)]},
             win={'+x': dict(floors=[Z0], w=1.5, h=1.15, sill=0.85), '+y': dict(floors=[Z0], w=1.6, h=1.15, sill=0.85, lit=[(0, 0)]),
                  '-y': dict(floors=[Z0], w=1.6, h=1.15, sill=0.85)})
    entry_door(p, WallFace('-x', gx0, 0.0), -12.5, -11.6, FL + 0.2, 2.3, t=0.25, canopy=0.0)
    prof_sheet(p, 'Roof', Vector((gx0 - 0.4, gy1 + 0.45, 3.42)), Vector((gx0 - 0.4, gy0 - 0.4, 3.0)),
               Vector((gx1 + 0.38, gy1 + 0.45, 3.42)), drip=0.05)
    p.box('Paint#flash', gx0 - 0.1, gx1 + 0.1, gy0 - 0.1, gy1 + 0.1, 2.9, 3.0 + 0.0, bevel=0.0)
    p.box('Paint#flash', gx0 - 0.05, gx1 + 0.05, gy1 - 0.05, gy1 + 0.05, 2.95, 3.35, bevel=0.0)
    for (f, at) in (('+x', gx1), ('-x', gx0)):                                    # os oitões de chapa (fecham o vão debaixo do telhado)
        face_prism(p, 'Paint#flash', WallFace(f, at, 0.0), [(gy0, 2.93), (gy1, 2.93), (gy1, 3.36)], -0.2, 0.0, bevel=0.0)
    stencil(p, WallFace('+x', gx1 + 0.08, 0.0), (gy0 + gy1) / 2, 2.35, 'ЛЖ-1', h=0.32, m='White', n=0.0)
    lamp(p, gx1 + 0.08, -11.3, 2.55, face='+x')
    # o holofote de busca no teto da guarita
    p.cyl('Metal#sl', (gx1 - 0.8, gy0 + 0.8, 3.5), 'z', 0.05, 0.8, seg=8, bevel=0.0)
    p.obox('Paint#sl', (gx1 - 0.7, gy0 + 0.8, 3.95), (0.45, 0.42, 0.42), yaw=30, pitch=-15, bevel=0.03)
    p.obox('Light', (gx1 - 0.47, gy0 + 0.93, 3.89), (0.03, 0.32, 0.32), yaw=30, pitch=-15, bevel=0.0)
    # a cancela listrada (levantada: o trator passa) e o cogumelo da sentinela
    boom_barrier(p, 11.2, -10.2, length=4.6, angle=76.0, yaw=90.0)
    sentry_mushroom(p, 11.3, -2.2, Z0)
    # a cerca PO-2: na frente (da cancela até o canto +Y) e no lado −Y (do prédio até a guarita)
    po2_fence(p, (12.75, -3.4), (12.75, 13.6), h=2.0, z0=Z0, side=-1)
    po2_fence(p, (2.9, -13.7), (9.5, -13.7), h=2.0, z0=Z0, side=-1)
    # as 3 bandeiras na frente (à direita da entrada)
    for k, y in enumerate((4.6, 5.9, 7.2)):
        flag_pole(p, 9.6, y, Z0, 7.0, cloth=(1.7, 1.1), phase=0.6 * k)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    lower(p)
    upper(p)
    radio_tower(p)
    yard(p)
    return p.finish()


def radar():
    """A antena de busca (a de Yagis do P-18 soviético): a mesa giratória, o mastro, o quadro com 2 fileiras de 3 Yagis apontando pra
    +X em repouso (o refletor atrás, os diretores na frente), a caixa do receptor atrás e a luz de obstáculo. Origem no eixo, no pé."""
    p = RuPart(PREFIX + '_Radar')
    p.cyl('Metal#turn', (0.0, 0.0, 0.12), 'z', 0.42, 0.24, seg=24, bevel=0.02)
    p.cyl('Dark', (0.0, 0.0, 0.25), 'z', 0.34, 0.03, seg=24, bevel=0.0)
    p.cyl('Paint#mast', (0.0, 0.0, 0.7), 'z', 0.11, 0.9, seg=12, bevel=0.0)
    p.box('Paint#box', -0.75, -0.2, -0.35, 0.35, 0.3, 0.85, bevel=0.04)                      # o receptor (contrapeso)
    p.box('Red#box', -0.77, -0.75, -0.25, 0.25, 0.55, 0.7, bevel=0.0)
    zf0, zf1, W = 0.95, 2.05, 1.75
    for z in (zf0, zf1):
        tube(p, 'Paint#frame', [(0.0, -W, z), (0.0, W, z)], 0.045, seg=8)
    for y in (-W, -W / 3, W / 3, W):
        tube(p, 'Paint#frame', [(0.0, y, zf0), (0.0, y, zf1)], 0.04, seg=8)
    tube(p, 'Paint#frame', [(0.0, 0.0, 0.85), (0.0, -W * 0.6, zf0)], 0.04, seg=6)
    tube(p, 'Paint#frame', [(0.0, 0.0, 0.85), (0.0, W * 0.6, zf0)], 0.04, seg=6)
    for z in (zf0 + 0.28, zf1 - 0.28):
        for y in (-W * 2 / 3, 0.0, W * 2 / 3):
            tube(p, 'Metal#yagi', [(-0.35, y, z), (1.85, y, z)], 0.035, seg=6)
            for k in range(7):
                xx = -0.25 + k * 0.33
                L = 0.82 if k == 0 else 0.66 - k * 0.02
                tube(p, 'White#yagi', [(xx, y - L / 2, z), (xx, y + L / 2, z)], 0.024, seg=4)
    p.cyl('Beacon', (0.0, 0.0, zf1 + 0.16), 'z', 0.08, 0.18, seg=12, bevel=0.0, smooth=True)
    p.cyl('Dark', (0.0, 0.0, zf1 + 0.05), 'z', 0.09, 0.08, seg=12, bevel=0.0)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(bake_in_place(radar(), RADAR_AT), OUT)      # o cozimento com a antena no lugar dela
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, B2[5] + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 26 x 28 m; radar at_m', tuple(round(v, 2) for v in RADAR_AT), 'rpm 6; garagem Y', GARAGE[0], '..', GARAGE[1],
          'em X', B1[1])
    print('MODEL fire_points_m', [(-6.0, -10.0, B1[5] + 0.1), (-5.5, 5.0, B2[5] + 0.1), (-6.0, -1.5, B3[5] + 0.1)])


if __name__ == '__main__':
    main()
