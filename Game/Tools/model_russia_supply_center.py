"""O Depósito de suprimentos da Rússia (Supply Center) provisório, modelado no Blender por script (a facção nova, 01/11/2026:
docs/FACCAO_RUSSIA.md §5.1: "o galpão de carga com a doca e a ponte rolante; o pórtico amarelo da ponte rolante").

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_supply_center.py C:/Users/luisf/explosion/captures/game/models/russia_supply_center

A planta: a base de abastecimento soviética (o склад com a rampa de carga alta de concreto e a marquise de chapa por cima, a ponte rolante
de pátio nas colunas de concreto com a cabine do operador pendurada, os contêineres de aço de 20 pés, as caixas de munição verdes, a
guarita do despachante em cima do escritório).

A palavra (o que lê de cima): A PONTE ROLANTE AMARELA. De cima, o retângulo amarelo das 2 vigas atravessando o pátio entre as 2 linhas de
colunas, ao lado do telhado verde comprido do armazém; a função aparece: o caminhão entra pela frente (+X) na faixa marcada, a ponte
corre nos trilhos até em cima dele, o guincho levanta a carga e leva pros contêineres e pras pilhas no fundo do pátio.

Peça por peça:
- o armazém (13,4 × 6,2 m, beiral a 5,0 m, cumeeira a 6,3 m): oco, painéis industriais, a fita de janelas alta nos fundos, as 2 portas
  de correr de chapa verde (meio abertas) sobre a rampa, o oitão da frente com a porta e o letreiro estêncil; o telhado de chapa verde;
- a rampa de carga (1,2 m de altura) com a cantoneira de aço na borda, os para-choques de borracha, a escada, a marquise de chapa verde
  em balanço nas mãos-francesas;
- as colunas da ponte rolante (concreto pré-moldado com o consolo), as vigas de caminho de aço com o trilho, os batentes listrados, o
  contraventamento;
- o escritório (1 andar de painel) com a GUARITA DO DESPACHANTE envidraçada no teto (olhando o pátio), o mastro com a bandeira da cor do
  jogador, a escada;
- o pátio: a faixa do caminhão marcada no chão (as linhas e os calços), os 2 contêineres empilhados, as pilhas cobertas com a lona da
  cor do jogador, as caixas de munição verdes nos paletes, o mastro de holofotes.

Lote 22 × 20 m (X × Y), frente em +X (a entrada do caminhão). Alturas: o armazém 6,3 m, as vigas de caminho 7,1 m, a ponte 8,2 m, o
escritório 3,9 m e a guarita 6,4 m, a bandeira até ~9 m, o mastro de holofotes 10 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_SupplyCenter_Base: tudo o que não se mexe;
  SM_Russia_SupplyCenter_Crane: a PONTE ROLANTE amarela inteira (as 2 vigas, as cabeceiras com as rodas no trilho, o carro do guincho, a
    cabine pendurada, os cabos, o moitão e a carga pendurada). Origem no meio da ponte, no topo do trilho: at_m = CRANE_AT (em repouso em
    cima do caminhão); sem giro. CORRE EM X pelos trilhos (CRANE_TRAVEL_X: de cima do caminhão até as pilhas do fundo);
  SM_Russia_SupplyCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Wood, Rubber; Yellow (a
ponte rolante).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp, barrel  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, splits, bake_in_place  # noqa: E402
from model_russia_parts import (ru_foundation, RuPart, ru_block, stencil, sign_board, gable_roof, ribbon_glazing, entry_door, prof_sheet,  # noqa: E402
                                sliding_leaf, container, tarp_stack, ammo_crate, floodlight_mast, flag_pole, ladder_on, ac_box,
                                face_prism, stairs_x)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_supply_center')
PREFIX = 'SM_Russia_SupplyCenter'

HX, HY = 11.0, 10.0                 # o lote 22 × 20
Z0 = 0.14
FL = Z0 + 0.12
T = 0.3
# o armazém e a rampa
WX0, WX1, WY0, WY1 = -10.4, 3.0, 3.2, 9.4
EAVE, RISE = 5.0, 1.3
RAMP_Z = Z0 + 1.2                   # o topo da rampa de carga
RAMP_Y0 = 1.4
# a ponte rolante: as linhas de colunas (Y), os eixos das colunas (X), o topo do trilho
RUN_Y = (-8.7, 0.7)
COL_X = (-9.4, -3.1, 3.2, 9.4)
RAIL_Z = 7.1
CRANE_AT = (4.6, (RUN_Y[0] + RUN_Y[1]) / 2, RAIL_Z)
CRANE_TRAVEL_X = (-6.2, 4.6)
TROLLEY_DY = -0.7                   # o carro do guincho em relação ao meio da ponte (em cima da faixa do caminhão)


# ===================================================================================================== o armazém e a rampa

def warehouse(p):
    """O armazém de painel (oco) com o telhado verde de duas águas, as 2 portas de correr sobre a rampa, a fita de janelas nos fundos,
    a porta e o letreiro no oitão da frente."""
    doors = [(-7.6, -4.6, RAMP_Z, 4.0), (-1.9, 1.1, RAMP_Z, 4.0)]
    ru_block(p, WX0, WX1, WY0, WY1, Z0, EAVE, t=T, pw=3.2, parapet=0.0, roof=None,
             zs={'-y': [RAMP_Z, 2.6, 4.0], '+y': [1.96, 3.16, 4.36], '+x': [1.96, 3.16, 4.36], '-x': [1.96, 3.16, 4.36]},
             holes={'-y': doors, '+y': [(-9.4, 2.0, 3.16, 4.36)], '+x': [(4.3, 5.5, FL + 0.3, FL + 2.5)]},
             ukeep={'-y': [-7.6, -4.6, -1.9, 1.1], '+y': [-9.4, 2.0]}, cols=[], step=4.4, along='x')
    ribbon_glazing(p, WallFace('+y', WY1, 0.0), -9.4, 2.0, 3.16, 4.36, t=T, step=1.4, rows=1)
    gable_roof(p, WX0, WX1, WY0, WY1, EAVE, RISE, along='x', over=0.4, eave=0.35, t=T, gutter=True)
    F = WallFace('-y', WY0, 0.0)
    for (u0, u1, z0, z1) in doors:
        wbox(p, 'Dark', WallFace('-y', WY0 + T, 0.0), u0, u1, z0, z1, 0.0, 0.02, bevel=0.0)
        # a folha de correr empurrada pro lado (meio aberta) e o trilho de cima
        w = u1 - u0
        sliding_leaf(p, F, u0 + w * 0.45, u1 + w * 0.45, z0 + 0.03, z1 + 0.1, 0.12)
        wbox(p, 'Paint#track', F, u0 - 0.2, u1 + w * 0.5 + 0.3, z1 + 0.22, z1 + 0.42, 0.08, 0.32, bevel=0.02)
        wbox(p, 'Hazard', F, u0 - 0.1, u0, z0, z0 + 1.2, 0.08, 0.12, bevel=0.0)
    # a luz de dentro que aparece pelas portas e as pilhas de caixas lá dentro
    for x in (-6.1, -0.4):
        p.box('Light', x - 0.6, x + 0.6, 5.6, 6.0, EAVE - 0.5, EAVE - 0.45, bevel=0.0)
    for (x, y) in ((-6.6, 6.4), (-5.2, 7.6), (-0.9, 6.6)):
        ammo_crate(p, x, y, RAMP_Z - 1.06, yaw=0)
    # o oitão da frente: a porta, o letreiro estêncil, a luminária
    Fx = WallFace('+x', WX1, 0.0)
    entry_door(p, Fx, 4.3, 5.5, FL + 0.3, FL + 2.5, t=T, canopy=0.9)
    stencil(p, Fx, 6.3, EAVE + 0.15, 'ШЛ-17', h=0.42, m='White', n=0.08)
    lamp(p, WX1 + 0.08, 4.9, 3.6, face='+x')
    lamp(p, WX1 + 0.08, 7.8, 3.6, face='+x')


def ramp(p):
    """A rampa de carga (1,2 m) ao longo da face −Y do armazém: o bloco de concreto, a cantoneira de aço na borda, os para-choques de
    borracha, a escada da ponta, a marquise de chapa verde em balanço nas mãos-francesas, a calha."""
    x0, x1 = WX0 - 0.2, WX1 + 0.2
    y0, y1 = RAMP_Y0, WY0 - 0.08
    p.box('Concrete#ramp', x0, x1, y0, y1, Z0 - 0.02, RAMP_Z, bevel=0.05)
    p.box('Paint#edge', x0, x1, y0 - 0.04, y0 + 0.12, RAMP_Z - 0.12, RAMP_Z + 0.02, bevel=0.0)
    for x in splits(x0 + 0.6, x1 - 0.6, 1.6):
        p.box('Rubber#bump', x - 0.16, x + 0.16, y0 - 0.16, y0, RAMP_Z - 0.55, RAMP_Z - 0.12, bevel=0.03)
    stairs_x(p, 'Concrete#step', x1 - 0.05, x1 + 1.3, y0 + 0.1, y0 + 1.3, Z0, RAMP_Z, 5)             # a escada na ponta da frente
    for (yy) in (y0 + 0.1, y0 + 1.3):
        tube(p, 'Metal#rail', [(x1 + 1.3, yy, Z0 + 1.0), (x1, yy, RAMP_Z + 1.0)], 0.03, seg=6)
    # a marquise: as mãos-francesas na parede, a chapa verde caindo pra fora, a calha
    zm, zl = 4.45, 4.15
    for x in splits(x0 + 0.4, x1 - 0.4, 2.2):
        tube(p, 'Paint#col', [(x, WY0 - 0.08, zm - 0.9), (x, y0 + 0.15, zl - 0.06)], 0.05, seg=8)
        p.box('Paint#col', x - 0.06, x + 0.06, y0 + 0.05, WY0 - 0.08, zl - 0.12, zm - 0.02, bevel=0.0)
    prof_sheet(p, 'Roof', Vector((x0 - 0.1, WY0 - 0.08, zm + 0.02)), Vector((x0 - 0.1, y0 - 0.3, zl - 0.04)),
               Vector((x1 + 0.1, WY0 - 0.08, zm + 0.02)), drip=0.05)
    p.cyl('Paint#gut', ((x0 + x1) / 2, y0 - 0.38, zl - 0.15), 'x', 0.08, x1 - x0 + 0.2, seg=10, bevel=0.0)
    for x in (x0 + 0.3, x1 - 0.3):
        tube(p, 'Paint#gut', [(x, y0 - 0.38, zl - 0.15), (x, y0 - 0.38, RAMP_Z + 0.1)], 0.05, seg=8)


# ===================================================================================================== a ponte rolante (o caminho)

def runway(p):
    """As 2 linhas de colunas de concreto pré-moldado (o fuste, o consolo, a sapata), as vigas de caminho de aço com o trilho em cima, os
    batentes listrados nas pontas e o contraventamento em X num vão de cada linha."""
    for y in RUN_Y:
        for x in COL_X:
            p.box('Concrete#foot', x - 0.6, x + 0.6, y - 0.6, y + 0.6, Z0 - 0.02, Z0 + 0.35, bevel=0.05)
            p.box('Concrete#col', x - 0.28, x + 0.28, y - 0.32, y + 0.32, Z0 + 0.35, RAIL_Z - 0.66, bevel=0.06)
            p.box('Metal#cap', x - 0.34, x + 0.34, y - 0.3, y + 0.3, RAIL_Z - 0.68, RAIL_Z - 0.63, bevel=0.0)
            p.box('Hazard', x - 0.29, x + 0.29, y - 0.33, y + 0.33, Z0 + 0.4, Z0 + 1.6, bevel=0.0)
        # a viga de caminho (o perfil I: as abas e a alma) e o trilho
        xa, xb = COL_X[0] - 0.5, COL_X[-1] + 0.5
        p.box('Paint#runway', xa, xb, y - 0.22, y + 0.22, RAIL_Z - 0.64, RAIL_Z - 0.56, bevel=0.0)
        p.box('Paint#runway', xa, xb, y - 0.05, y + 0.05, RAIL_Z - 0.58, RAIL_Z - 0.16, bevel=0.0)
        p.box('Paint#runway', xa, xb, y - 0.22, y + 0.22, RAIL_Z - 0.18, RAIL_Z - 0.1, bevel=0.0)
        p.box('Metal#rail', xa, xb, y - 0.04, y + 0.04, RAIL_Z - 0.1, RAIL_Z, bevel=0.0)
        for x in splits(xa + 0.3, xb - 0.3, 1.2):                                 # os enrijecedores da alma
            p.box('Paint#runway', x - 0.015, x + 0.015, y - 0.2, y + 0.2, RAIL_Z - 0.56, RAIL_Z - 0.18, bevel=0.0)
        for x in (xa + 0.15, xb - 0.15):
            p.box('Hazard', x - 0.15, x + 0.15, y - 0.2, y + 0.2, RAIL_Z, RAIL_Z + 0.35, bevel=0.02)
        # o contraventamento em X no vão de trás
        xa2, xb2 = COL_X[0] + 0.3, COL_X[1] - 0.3
        for (a, b) in (((xa2, Z0 + 0.5), (xb2, RAIL_Z - 1.5)), ((xa2, RAIL_Z - 1.5), (xb2, Z0 + 0.5))):
            tube(p, 'Paint#brace', [(a[0], y, a[1]), (b[0], y, b[1])], 0.05, seg=6)
    # o cabo de força da ponte (o festão no arame ao longo da linha de trás)
    tube(p, 'Metal#wire', [(COL_X[0], RUN_Y[1] + 0.3, RAIL_Z + 0.4), (COL_X[-1], RUN_Y[1] + 0.3, RAIL_Z + 0.4)], 0.012, seg=4)
    for x in splits(COL_X[0], COL_X[-1], 1.0):
        p.cyl('Dark#cable', (x, RUN_Y[1] + 0.3, RAIL_Z + 0.1), 'z', 0.03, 0.55, seg=6, bevel=0.0)


# ===================================================================================================== o escritório e o pátio

def office(p):
    """O escritório (1 andar de painel) com a guarita do despachante envidraçada no teto, a escada externa, a bandeira."""
    x0, x1, y0, y1, z1 = 4.8, 10.4, 3.4, 9.4, 3.4
    door = (7.0, 8.2, FL + 0.3, FL + 2.5)
    ru_block(p, x0, x1, y0, y1, Z0, z1, t=T, pw=2.8, zs=[z1], parapet=0.5, roof_skip=(6.6, 9.8, 3.8, 6.8),
             holes={'-y': [door]},
             win={'+x': dict(floors=[Z0], w=1.4, lit=[(1, 0)]), '-y': dict(floors=[Z0], w=1.4, skip=[(1, 0)]), '+y': dict(floors=[Z0], w=1.4)})
    entry_door(p, WallFace('-y', y0, 0.0), door[0], door[1], door[2], door[3], t=T, canopy=1.0)
    # a guarita do despachante: o cubo envidraçado no teto (olha o pátio), o telhadinho de chapa verde
    gx0, gx1, gy0, gy1, gz0, gz1 = 6.6, 9.8, 3.8, 6.8, z1 + 0.05, z1 + 2.7
    ru_block(p, gx0, gx1, gy0, gy1, gz0, gz1, t=0.22, pw=1.6, zs=[gz1], parapet=0.0, socle=0.0, roof='slab', interior=False,
             win={'-y': dict(floors=[gz0], w=1.25, h=1.3, sill=0.8, lit=[(0, 0)]), '-x': dict(floors=[gz0], w=1.25, h=1.3, sill=0.8),
                  '+x': dict(floors=[gz0], w=1.25, h=1.3, sill=0.8)})
    prof_sheet(p, 'Roof', Vector((gx0 - 0.4, gy1 + 0.4, gz1 + 0.42)), Vector((gx0 - 0.4, gy0 - 0.45, gz1 + 0.08)),
               Vector((gx1 + 0.4, gy1 + 0.4, gz1 + 0.42)), drip=0.05)
    for (f, at) in (('+x', gx1), ('-x', gx0)):
        face_prism(p, 'Paint#flash', WallFace(f, at, 0.0), [(gy0, gz1 - 0.02), (gy1, gz1 - 0.02), (gy1, gz1 + 0.4)], -0.2, 0.0, bevel=0.0)
    p.box('Paint#flash', gx0 - 0.05, gx1 + 0.05, gy1 - 0.05, gy1 + 0.05, gz1 - 0.02, gz1 + 0.4, bevel=0.0)
    # a escada de marinheiro até o teto, o condensador, a bandeira no canto
    ladder_on(p, WallFace('+y', y1, 0.0), 6.0, Z0, z1 + 0.4, cage=False)
    entry_door(p, WallFace('+y', gy1, 0.0), 7.2, 8.0, gz0 + 0.05, gz0 + 2.1, t=0.22, canopy=0.0)
    ac_box(p, 5.8, 8.4, z1 + 0.05)
    flag_pole(p, 10.0, 9.0, z1 + 0.05, 5.0, cloth=(1.6, 1.0), phase=0.4)


def yard(p):
    """O pátio: a faixa do caminhão no chão (as linhas, os calços), os contêineres empilhados, as pilhas cobertas, as caixas de munição
    nos paletes, os tambores, o mastro de holofotes."""
    # a faixa do caminhão (debaixo da ponte, entrando pela frente)
    ly0, ly1 = -7.0, -2.4
    for y in (ly0, ly1):
        p.box('White#line', 0.6, HX - 0.4, y - 0.06, y + 0.06, Z0, Z0 + 0.012, bevel=0.0)
    p.box('White#line', 0.6, 0.75, ly0, ly1, Z0, Z0 + 0.012, bevel=0.0)
    for k in range(6):                                                             # as divisas da entrada
        x = 8.0 + k * 0.5
        p.obox('Hazard', (x, (ly0 + ly1) / 2, Z0 + 0.008), (0.18, ly1 - ly0 - 0.4, 0.012), yaw=0, bevel=0.0)
    for y in (ly0 + 0.9, ly1 - 0.9):                                                # os calços de roda (o caminhão para aqui)
        p.box('Hazard', 1.2, 1.55, y - 0.4, y + 0.4, Z0, Z0 + 0.22, bevel=0.03)
    # os contêineres empilhados no fundo do pátio (debaixo do curso da ponte)
    container(p, -6.3, -6.9, Z0, yaw=0.0, m='Paint#cont', doors='+x')
    container(p, -6.0, -6.9, Z0 + 2.6, yaw=0.0, m='Roof#cont', doors='+x')
    container(p, -6.6, -3.9, Z0, yaw=0.0, m='Roof#cont', doors='+x')
    # as pilhas cobertas e as caixas de munição nos paletes
    tarp_stack(p, -1.4, -6.2, Z0, 2.4, 1.8, 1.3, yaw=4, seed=3)
    tarp_stack(p, -1.8, -1.6, Z0, 2.0, 1.6, 1.0, yaw=-6, seed=7)
    for (x, y, yaw) in ((-9.6, -1.4, 90), (-8.9, -1.4, 92)):
        p.box('Wood#pallet', x - 0.6, x + 0.6, y - 0.9, y + 0.9, Z0, Z0 + 0.14, bevel=0.02)
        for k in range(3):
            ammo_crate(p, x + (k // 2) * 0.03, y - 0.5 + (k % 2) * 0.62, Z0 + 0.14 + (k // 2) * 0.425, yaw=yaw + (k // 2) * 3)
    for (x, y) in ((1.6, 0.0), (2.2, 0.15), (1.9, -0.5)):
        barrel(p, x, y, Z0, r=0.29, h=0.88, m='Paint#drum', ring='Dark')
    floodlight_mast(p, 10.05, -9.3, Z0, 9.4, face=160)
    sign_board(p, WallFace('+x', 10.75, 0.0), -1.4, 1.6, 'ДЛ-4', h=0.42, board='Red', ink='White', pad=0.15)
    for y in (-2.1, -0.7):
        p.cyl('Paint#post', (10.65, y, Z0 + 1.0), 'z', 0.05, 2.0, seg=8, bevel=0.0)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    warehouse(p)
    ramp(p)
    runway(p)
    office(p)
    yard(p)
    return p.finish()


# ===================================================================================================== a ponte rolante (a peça que corre)

def crane():
    """A ponte rolante amarela inteira, com a origem no meio dela no topo do trilho: as 2 vigas-caixão, as cabeceiras com as rodas, as
    passarelas com o guarda-corpo, o carro do guincho com o motor e o tambor, a cabine do operador pendurada numa ponta, os cabos, o moitão,
    a carga pendurada (o palete de caixas com as cintas) e os batentes listrados."""
    p = RuPart(PREFIX + '_Crane')
    half = (RUN_Y[1] - RUN_Y[0]) / 2
    zg0, zg1 = 0.28, 1.12                       # as vigas-caixão (em cima das cabeceiras)
    for s in (-1, 1):
        x = s * 0.62
        p.box('Yellow#crane', x - 0.22, x + 0.22, -half - 0.25, half + 0.25, zg0, zg1, bevel=0.04)
        for y in splits(-half + 0.6, half - 0.6, 1.0):                          # os enrijecedores (o ritmo)
            p.box('Yellow#crane', x + s * 0.22 - 0.02, x + s * 0.22 + 0.02, y - 0.03, y + 0.03, zg0 + 0.04, zg1 - 0.04, bevel=0.0)
    for y in (-half, half):                                                         # as cabeceiras com as rodas
        p.box('Yellow#crane', -1.6, 1.6, y - 0.28, y + 0.28, 0.12, zg0 + 0.2, bevel=0.04)
        for x in (-1.15, 1.15):
            p.cyl('Metal#wheel', (x, y, 0.2), 'y', 0.2, 0.14, seg=16, bevel=0.0)
            p.cyl('Metal#wheel', (x, y, 0.2), 'y', 0.24, 0.03, seg=16, bevel=0.0)
        for s in (-1, 1):
            p.box('Hazard', s * 1.6 - (0.0 if s > 0 else 0.18), s * 1.6 + (0.18 if s > 0 else 0.0), y - 0.2, y + 0.2, 0.15, zg0 + 0.15, bevel=0.02)
        p.box('Metal#motor', -0.4, 0.4, y - (0.6 if y > 0 else -0.28), y - (0.28 if y > 0 else -0.6), zg0 + 0.1, zg0 + 0.55, bevel=0.03)
    # as passarelas dos 2 lados com o guarda-corpo
    for s in (-1, 1):
        x0, x1 = (0.84, 1.45) if s > 0 else (-1.45, -0.84)
        p.box('Metal#walk', x0, x1, -half + 0.3, half - 0.3, zg0 + 0.05, zg0 + 0.1, bevel=0.0)
        xr = x1 if s > 0 else x0
        for y in splits(-half + 0.3, half - 0.3, 1.5):
            p.cyl('Yellow#rail', (xr, y, zg0 + 0.6), 'z', 0.025, 1.0, seg=6, bevel=0.0)
        for zz in (zg0 + 0.6, zg0 + 1.1):
            tube(p, 'Yellow#rail', [(xr, -half + 0.3, zz), (xr, half - 0.3, zz)], 0.025, seg=6)
    # o carro do guincho em cima das vigas (sobre a faixa do caminhão)
    ty = TROLLEY_DY
    p.box('Yellow#trolley', -0.95, 0.95, ty - 0.75, ty + 0.75, zg1, zg1 + 0.25, bevel=0.03)
    p.cyl('Metal#drum', (0.0, ty, zg1 + 0.6), 'y', 0.32, 1.1, seg=18, bevel=0.0)
    p.box('Paint#motor', -0.85, -0.2, ty + 0.25, ty + 0.7, zg1 + 0.25, zg1 + 0.8, bevel=0.03)
    p.box('Paint#motor', 0.3, 0.85, ty - 0.7, ty - 0.2, zg1 + 0.25, zg1 + 0.65, bevel=0.03)
    for x in (-0.75, 0.75):
        for dy in (-0.55, 0.55):
            p.cyl('Metal#wheel', (x, ty + dy, zg1 + 0.05), 'x', 0.11, 0.1, seg=12, bevel=0.0)
    # os cabos (entre as 2 vigas), o moitão, o gancho e a carga pendurada
    zh = -3.6
    for dy in (-0.16, 0.16):
        tube(p, 'Dark#cable', [(0.0, ty + dy, zg1 + 0.3), (0.0, ty + dy, zh + 0.5)], 0.022, seg=6)
    p.box('Yellow#hook', -0.24, 0.24, ty - 0.32, ty + 0.32, zh + 0.1, zh + 0.5, bevel=0.03)
    for k in range(3):
        p.box('Dark', -0.25, 0.25, ty - 0.3 + k * 0.24, ty - 0.26 + k * 0.24, zh + 0.12, zh + 0.48, bevel=0.0)
    p.torus('Metal#hook', (0.0, ty, zh - 0.05), 'x', 0.14, 0.045, seg=14, rseg=6)
    for (a, b) in (((-0.75, ty - 0.55), (0.0, ty)), ((0.75, ty - 0.55), (0.0, ty)), ((-0.75, ty + 0.55), (0.0, ty)), ((0.75, ty + 0.55), (0.0, ty))):
        tube(p, 'Dark#cable', [(a[0], a[1], zh - 1.0), (b[0], b[1], zh - 0.12)], 0.015, seg=4)
    zl = zh - 1.0
    p.box('Wood#pallet', -0.85, 0.85, ty - 0.65, ty + 0.65, zl - 0.14, zl, bevel=0.02)
    p.box('Roof#crate', -0.8, 0.8, ty - 0.6, ty + 0.6, zl - 0.95, zl - 0.14, bevel=0.04)
    for x in (-0.45, 0.45):
        p.box('Dark#strap', x - 0.04, x + 0.04, ty - 0.62, ty + 0.62, zl - 0.97, zl - 0.12, bevel=0.0)
    p.box('White#sten', -0.5, 0.5, ty - 0.605, ty - 0.6, zl - 0.6, zl - 0.48, bevel=0.0)
    # a cabine do operador pendurada debaixo da ponta +Y (as janelas em volta, o teto, o degrau)
    cy0, cy1 = half - 1.6, half - 0.3
    p.box('Yellow#cab', 0.2, 1.5, cy0, cy1, -1.7, zg0 - 0.05, bevel=0.04)
    for (f, at, u0, u1) in (('+x', 1.5, cy0 + 0.15, cy1 - 0.15), ('-y', cy0, 0.35, 1.35)):
        Fc = WallFace(f, at, 0.0)
        wbox(p, 'Glass', Fc, u0, u1, -1.25, -0.25, -0.02, 0.01, bevel=0.0)
        wbox(p, 'Metal#frame', Fc, u0 - 0.04, u1 + 0.04, -1.29, -1.21, -0.02, 0.03, bevel=0.0)
        wbox(p, 'Metal#frame', Fc, u0 - 0.04, u1 + 0.04, -0.29, -0.21, -0.02, 0.03, bevel=0.0)
    p.box('Light', 0.6, 1.1, cy0 + 0.3, cy0 + 0.6, -1.72, -1.7, bevel=0.0)
    p.box('Metal#hanger', 0.4, 0.6, cy0 + 0.2, cy1 - 0.2, zg0 - 0.05, zg0 + 0.02, bevel=0.0)
    # o letreiro da capacidade na viga (estêncil preto no amarelo) e as luzes de baixo
    stencil(p, WallFace('+x', 0.84, 0.0), -2.4, zg0 + 0.2, 'Ц-10', h=0.5, m='Dark', n=0.0)
    for y in (-half + 1.4, half - 2.2):
        p.box('Dark#lamp', -0.3, 0.3, y - 0.2, y + 0.2, zg0 - 0.12, zg0 + 0.0, bevel=0.0)
        p.box('Light', -0.25, 0.25, y - 0.15, y + 0.15, zg0 - 0.13, zg0 - 0.11, bevel=0.0)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(bake_in_place(crane(), CRANE_AT), OUT)      # o cozimento com a ponte no lugar dela (em cima do caminhão)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, EAVE + RISE + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 22 x 20 m; crane (a ponte) at_m', tuple(round(v, 2) for v in CRANE_AT), 'corre em X', CRANE_TRAVEL_X,
          '; caminhao para na faixa X 1.2..10.6, Y -7.0..-2.4 (centro 5.9, -4.7); carro do guincho em Y', round(CRANE_AT[1] + TROLLEY_DY, 2))
    print('MODEL fire_points_m', [(-6.0, 6.3, EAVE + RISE + 0.1), (-1.0, 4.6, 5.65), (6.0, 8.4, 3.5)])


if __name__ == '__main__':
    main()
