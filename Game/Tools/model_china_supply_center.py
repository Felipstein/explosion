"""O Supply Center da China provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4.1).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_supply_center.py -- captures/game/models/china_supply_center

O centro de suprimento (onde o caminhão descarrega as caixas e vira dinheiro), na bíblia da China (docs/CONCEITO_CONSTRUCOES.md
§2): indústria pesada, concreto escuro puxado pro oliva, o telhado de pagode verde com a borda vermelha, a estrela vermelha, a
chaminé listrada, os estandartes (2 aqui), nunca trem ou trilho (a ponte rolante corre em vigas no alto, não no chão).

A palavra (o que lê de cima): A PONTE ROLANTE VERMELHA SOBRE A DOCA DE DESCARGA. De cima, o quadro vermelho (as 2 vigas de
caminho nas colunas e a ponte atravessada) ao lado do telhado comprido verde do armazém. A função aparece: o caminhão entra pela
frente (+X) na faixa amarela, para debaixo da ponte, o carrinho levanta as caixas da caçamba e põe na doca (a plataforma alta
com as faixas de perigo e os para-choques), e as caixas entram no armazém pelas portas de enrolar. Peça por peça:
- o armazém comprido (+Y) com o beiral duplo de pagode (a saia de telha em volta e o telhado de quadril com a cumeeira, os
  ornamentos e as pontas levantadas), as janelinhas do andar de cima, os pilares, a estrela grande no oitão da frente;
- a doca (a plataforma de 1,15 m), as 2 portas de enrolar meio abertas com o interior aceso, o estandarte entre elas, as
  rampinhas niveladoras, a escada com o corrimão, as caixas;
- a ponte rolante: 6 colunas vermelhas com as mãos-francesas, as vigas de caminho com o trilho em cima, a ponte de 2 vigas com a
  estrela, os truques das pontas, a passarela, a cabine pendurada, as balizas; o carrinho (peça separada) com o guincho e a
  caixa pendurada no balancim;
- a faixa do caminhão (a caixa amarela, os batentes de roda, a linha de centro);
- o escritório pequeno na frente (−Y) com as janelas, o telhadinho de pagode, a porta com a marquise, o estandarte, o poste do
  alto-falante; as pilhas de caixas verde-oliva com a estrela; a casa da caldeira com a chaminé listrada fina e os estais;
  a caixa d'água redonda nas 4 pernas (o redondo do lote, regra 2 do estudo §3b).

Lote 22 × 20 m (X × Y), frente em +X (a entrada do caminhão). Alturas: o armazém 4,4 m de parede, cumeeira 7,6 m, ornamentos
~8,1 m; a ponte rolante 8,1 m (o carrinho 8,9 m); a chaminé 10,4 m; a caixa d'água 6,6 m; o escritório 4,7 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_SupplyCenter_Base: a fundação, o armazém, a doca, as colunas, as vigas de caminho e a ponte, o escritório, a
    caldeira, a chaminé, a caixa d'água, as caixas;
  SM_China_SupplyCenter_Crane: o CARRINHO da ponte (o quadro com as rodas, o tambor do guincho, o motor, os cabos, o balancim
    e a caixa pendurada). Origem no eixo do carrinho, no topo das vigas da ponte: at_m = CRANE_AT; sem giro. Corre em Y pela
    ponte (CRANE_TRAVEL_Y, de cima da caçamba até a doca), pro jogo animar depois;
  SM_China_SupplyCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
O telhado de pagode, a parede do andar, o estandarte, as cornetas e a chaminé vêm do model_china_war_factory.py (a mesma família).
Materiais (slots do model_faction_kit.py, China prédio): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho: a
ponte rolante, os estandartes, a borda do telhado, as estrelas, as faixas da chaminé), Dark, Trim, Glass, Light, Beacon, Hazard,
Paint; Crate (as caixas verde-oliva) e Wood (os paletes).
"""
import math
import os
import sys

import bpy  # noqa: F401
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, clear_scene, export, scaffold  # noqa: E402
from model_faction_kit import foundation, lamp, ladder, railing, polygon_prism, star_points, star_wall, disc_ring  # noqa: E402
from model_china_war_factory import (cn_roof, cn_frieze, pagoda_story, wall_banner, horn, chimney5, battered,  # noqa: E402
                                     louver, parapet_ring, tube, seams)
from model_building_parts import BPart, WallFace, cn_block, bake_in_place  # noqa: E402
from model_building_parts import cn_window as cn_window_v  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_supply_center')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_SupplyCenter'
COLORS.setdefault('Crate', (84, 86, 58))
COLORS.setdefault('Wood', (120, 88, 52))

HX, HY = 11.0, 10.0              # o lote 22 × 20
G = 0.14                         # o topo da laje (a fundação do kit)
# o armazém (centro, meias-medidas da parede) e a doca
WX0, WX1, WY0, WY1, WZ = -9.0, 5.0, 2.8, 8.4, 4.4
DOCK_Z = 1.3
# a ponte rolante: as linhas das colunas (Y), as colunas (X), a altura das vigas de caminho, a ponte e o carrinho
ROW_Y = (-5.4, 1.5)
COL_X = (-8.3, -3.0, 2.3)
RUN_Z0, RUN_Z1 = 6.3, 6.9
BRIDGE_X = -1.6
GIRDER_Z1 = 8.1
CRANE_AT = (BRIDGE_X, -2.3, GIRDER_Z1)
CRANE_TRAVEL_Y = (-4.6, 0.9)


# ===================================================================================================== peças

def crate(p, x, y, z, size=(1.2, 1.0, 0.9), yaw=0.0, star_faces=('top', '+x', '-y')):
    """A caixa de suprimento verde-oliva com as cintas escuras e a estrela vermelha pintada (em cima e nos lados que a câmera
    vê)."""
    sx, sy, sz = size
    p.obox('Crate', (x, y, z + sz / 2), size, yaw=yaw, bevel=0.04)
    for t in (-0.3, 0.3):
        ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        p.obox('Dark', (x + ca * sx * t, y + sa * sx * t, z + sz / 2), (0.08, sy + 0.02, sz + 0.02), yaw=yaw, bevel=0.0)
    if yaw == 0.0:
        r = min(sy, sz) * 0.3
        if 'top' in star_faces:
            polygon_prism(p, 'Player', star_points(x, y, min(sx, sy) * 0.3, rot_deg=0.0), z + sz, z + sz + 0.012)
        if '+x' in star_faces:
            star_wall(p, (x + sx / 2, y, z + sz / 2), '+x', r, t=0.012)
        if '-y' in star_faces:
            star_wall(p, (x, y - sy / 2, z + sz / 2), '-y', r, t=0.012)


def pallet(p, x, y, z, sx, sy):
    p.box('Wood', x - sx / 2, x + sx / 2, y - sy / 2, y + sy / 2, z + 0.1, z + 0.15, bevel=0.0)
    for t in (-0.4, 0.0, 0.4):
        p.box('Wood', x - sx / 2, x + sx / 2, y + sy * t - 0.06, y + sy * t + 0.06, z, z + 0.1, bevel=0.0)


def cn_window(p, x, y, z, face, w=1.0, h=1.1, frame='CnMetal'):
    """A janela de concreto da China: o vidro recuado, a moldura de aço com a travessa, o peitoril e a verga de concreto."""
    s = 1 if face[0] == '+' else -1
    if face in ('+x', '-x'):
        p.box('Glass', min(x - 0.04 * s, x + 0.01 * s), max(x - 0.04 * s, x + 0.01 * s), y - w / 2, y + w / 2, z, z + h, bevel=0.0)
        for (a, b, c, d) in ((y - w / 2 - 0.06, y - w / 2, z, z + h), (y + w / 2, y + w / 2 + 0.06, z, z + h),
                             (y - w / 2, y + w / 2, z + h * 0.55 - 0.03, z + h * 0.55 + 0.03), (y - 0.03, y + 0.03, z, z + h)):
            p.box(frame, min(x, x + 0.05 * s), max(x, x + 0.05 * s), a, b, c, d, bevel=0.0)
        p.box('CnPlinth', min(x, x + 0.14 * s), max(x, x + 0.14 * s), y - w / 2 - 0.12, y + w / 2 + 0.12, z - 0.1, z, bevel=0.0)
        p.box('CnConcrete', min(x, x + 0.1 * s), max(x, x + 0.1 * s), y - w / 2 - 0.12, y + w / 2 + 0.12, z + h, z + h + 0.14, bevel=0.0)
    else:
        p.box('Glass', x - w / 2, x + w / 2, min(y - 0.04 * s, y + 0.01 * s), max(y - 0.04 * s, y + 0.01 * s), z, z + h, bevel=0.0)
        for (a, b, c, d) in ((x - w / 2 - 0.06, x - w / 2, z, z + h), (x + w / 2, x + w / 2 + 0.06, z, z + h),
                             (x - w / 2, x + w / 2, z + h * 0.55 - 0.03, z + h * 0.55 + 0.03), (x - 0.03, x + 0.03, z, z + h)):
            p.box(frame, a, b, min(y, y + 0.05 * s), max(y, y + 0.05 * s), c, d, bevel=0.0)
        p.box('CnPlinth', x - w / 2 - 0.12, x + w / 2 + 0.12, min(y, y + 0.14 * s), max(y, y + 0.14 * s), z - 0.1, z, bevel=0.0)
        p.box('CnConcrete', x - w / 2 - 0.12, x + w / 2 + 0.12, min(y, y + 0.1 * s), max(y, y + 0.1 * s), z + h, z + h + 0.14, bevel=0.0)


def rollup_door(p, x0, x1, y, z0, z1, open_frac=0.55):
    """A porta de enrolar na face −Y (a doca): o vão escuro com o interior aceso, a cortina meio levantada, a caixa do rolo,
    as guias com a faixa de perigo."""
    zo = z0 + (z1 - z0) * open_frac
    p.box('Dark', x0, x1, y - 0.02, y + 0.3, z0, z1, bevel=0.0)                      # o fundo (o interior)
    p.box('Light', x0 + 0.3, x1 - 0.3, y + 0.25, y + 0.28, z1 - 0.35, z1 - 0.25, bevel=0.0)
    p.box('CnPlinth', x0 + 0.1, x1 - 0.1, y + 0.27, y + 0.31, z0, z1 - 0.4, bevel=0.0)
    p.box('CnMetal', x0, x1, y - 0.05, y - 0.01, zo, z1, bevel=0.0)                  # a cortina levantada até o meio
    for k in range(int((z1 - zo) / 0.22)):
        zz = zo + 0.11 + k * 0.22
        p.box('Dark', x0 + 0.02, x1 - 0.02, y - 0.07, y - 0.04, zz - 0.02, zz + 0.02, bevel=0.0)
    p.box('CnMetal', x0 - 0.15, x1 + 0.15, y - 0.4, y, z1, z1 + 0.45, bevel=0.02)   # a caixa do rolo
    for xx in (x0 - 0.1, x1 + 0.1):
        p.box('CnMetal', xx - 0.1, xx + 0.1, y - 0.12, y, z0, z1, bevel=0.0)
        for k in range(4):
            zz = z0 + k * 0.3
            p.box('Hazard' if k % 2 == 0 else 'Dark', xx - 0.11, xx + 0.11, y - 0.14, y - 0.12, zz, zz + 0.3, bevel=0.0)


def star_medallion(p, x, y, z, face, r=1.1):
    """O medalhão da estrela no oitão: o aro dourado, o fundo verde (como a viga pintada) e a estrela vermelha."""
    s = 1 if face[0] == '+' else -1
    axis = 'x' if face in ('+x', '-x') else 'y'
    if axis == 'x':
        p.cyl('CnGold', (x + s * 0.04, y, z), 'x', r, 0.08, seg=40, bevel=0.0)
        p.cyl('CnRoof', (x + s * 0.07, y, z), 'x', r * 0.88, 0.08, seg=40, bevel=0.0)
        star_wall(p, (x + s * 0.11, y, z), face, r * 0.74, t=0.03)
    else:
        p.cyl('CnGold', (x, y + s * 0.04, z), 'y', r, 0.08, seg=40, bevel=0.0)
        p.cyl('CnRoof', (x, y + s * 0.07, z), 'y', r * 0.88, 0.08, seg=40, bevel=0.0)
        star_wall(p, (x, y + s * 0.11, z), face, r * 0.74, t=0.03)


# ===================================================================================================== a base

def warehouse(p):
    """O armazém comprido: o rodapé inclinado, as paredes com os pilares, o beiral duplo de pagode, o oitão com a estrela,
    as portas da doca, o estandarte, as janelas e os tubos de descida."""
    cx, cy, ax, ay = (WX0 + WX1) / 2, (WY0 + WY1) / 2, (WX1 - WX0) / 2, (WY1 - WY0) / 2
    # (31/10, o detalhe AAA) o armazém OCO de placas (as paredes de 0,4 m, a pele com o chanfro largo e a junta funda, o talude no
    # pé, o interior com as vigas e os pilares), os vãos de verdade (as portas da doca, a porta do oitão, a dos fundos, as janelas
    # altas recuadas na espessura)
    bay = (WX1 - WX0 - 0.5) / 6
    wins_b = [(WX0 + 1.4 + k * 2.3 - 0.5, WX0 + 1.4 + k * 2.3 + 0.5, 2.6, 3.5) for k in range(6)]
    doors_d = [(dx - 1.7, dx + 1.7, DOCK_Z, DOCK_Z + 2.65) for dx in (-6.2, 0.8)]
    cn_block(p, WX0, WX1, WY0, WY1, G, WZ, t=0.4, skin=('+x', '-x', '+y', '-y'),
             holes={'+y': wins_b, '-y': doors_d, '+x': [(cy - 0.7, cy + 0.7, G, G + 2.2)], '-x': [(cy - 0.6, cy + 0.6, G, G + 2.2)]},
             roof_t=0.3, parapet=0.0, cornice=False, corners=True, pier=0.7, interior=True, plinth=0.6, pw=bay,
             zs=[G, G + 1.6, WZ - 0.3])
    # os pilares das faces compridas (o ritmo do salão chinês: cobrem as juntas das placas), chanfrados e mais fundos
    for k in range(7):
        xx = WX0 + 0.25 + k * bay
        p.box('CnConcrete#pier', xx - 0.28, xx + 0.28, WY1, WY1 + 0.36, G, WZ - 0.1, bevel=0.1)
    for xx in (WX0 + 0.25, -4.15, -1.25, 2.85, WX1 - 0.25):                     # na face da doca, entre as portas
        p.box('CnConcrete#pier', xx - 0.28, xx + 0.28, WY0 - 0.36, WY0, G, WZ - 0.1, bevel=0.1)
    # o beiral de baixo (a saia de telha em volta), o andar de cima com as janelinhas e o telhado de quadril
    top0, tx0, ty0 = cn_roof(p, cx, cy, ax, ay, WZ, 0.55, over=0.8, inset=1.3, curl=0.32, sweep=0.2, rib_step=0.42)
    pagoda_story(p, cx, cy, tx0, ty0, top0, 0.75, win_step=1.3)
    cn_roof(p, cx, cy, tx0, ty0, top0 + 0.75, 1.6, over=0.85, curl=0.5, rib_step=0.42)
    # o oitão da frente (+X): o medalhão da estrela, a porta com a marquise, as luzes
    star_medallion(p, WX1, cy, 2.75, '+x', r=1.05)
    p.box('Dark', WX1 - 0.28, WX1 - 0.24, cy - 0.7, cy + 0.7, G, G + 2.2, bevel=0.0)                   # a porta, recuada no vão
    p.box('CnMetal#frame', WX1 - 0.1, WX1 + 0.2, cy - 0.85, cy - 0.7, G, G + 2.3, bevel=0.02)
    p.box('CnMetal#frame', WX1 - 0.1, WX1 + 0.2, cy + 0.7, cy + 0.85, G, G + 2.3, bevel=0.02)
    p.obox('CnMetal', (WX1 + 0.55, cy, G + 2.45), (0.9, 2.0, 0.08), pitch=-10, bevel=0.0)
    lamp(p, WX1, cy - 1.6, G + 2.0, face='+x')
    lamp(p, WX1, cy + 1.6, G + 2.0, face='+x')
    # a face da doca (−Y): as 2 portas de enrolar, o estandarte entre elas, as luzes sobre as portas
    for dx in (-6.2, 0.8):
        rollup_door(p, dx - 1.7, dx + 1.7, WY0, DOCK_Z, DOCK_Z + 2.65)
        lamp(p, dx, WY0 - 0.4, DOCK_Z + 3.0, face='-y')
    wall_banner(p, -2.7, WY0, WZ - 0.15, '-y', w=1.1, h=2.6, notch=0.4)
    # os fundos (−X): a porta pequena e a grelha; a face de trás (+Y): as janelas altas e os tubos de descida
    p.box('Dark', WX0 + 0.24, WX0 + 0.28, cy - 0.6, cy + 0.6, G, G + 2.2, bevel=0.0)
    louver(p, WX0 - 0.13, WX0 - 0.12, cy + 1.2, cy + 2.4, 2.4, 3.4, '-x', n=4)
    lamp(p, WX0, cy - 1.2, 2.6, face='-x')
    Fb = WallFace('+y', WY1, 0.0)
    for (a, b, c, d) in wins_b:
        cn_window_v(p, Fb, a, b, c, d, t=0.4, mull=1)
    for xx in (WX0 + 0.6, WX1 - 0.6):
        p.cyl('Trim', (xx, WY1 + 0.3, (G + WZ) / 2), 'z', 0.08, WZ - G, seg=8, bevel=0.0)


def dock(p):
    """A doca: a plataforma alta com a borda de perigo e os para-choques, as rampinhas niveladoras, a escada com o corrimão,
    as caixas que já desceram."""
    x0, x1, y0, y1 = WX0 + 0.2, 4.6, 0.8, WY0
    p.box('CnPlinth', x0, x1, y0, y1, G, DOCK_Z, bevel=0.05)
    p.box('Dark', x0, x1, y0 - 0.02, y0 + 0.02, G + 0.05, G + 0.1, bevel=0.0)
    n = int((x1 - x0) / 0.5)
    for k in range(n):                                                               # a borda listrada
        xx = x0 + k * (x1 - x0) / n
        p.box('Hazard' if k % 2 == 0 else 'Dark', xx, xx + (x1 - x0) / n, y0 - 0.005, y0 + 0.28, DOCK_Z, DOCK_Z + 0.012, bevel=0.0)
    for k in range(6):                                                               # os para-choques de borracha
        xx = x0 + 1.0 + k * (x1 - x0 - 2.0) / 5
        p.box('Dark', xx - 0.22, xx + 0.22, y0 - 0.16, y0, G + 0.45, G + 1.0, bevel=0.03)
    for dx in (-6.2, 0.8):                                                           # as rampinhas niveladoras
        p.box('CnMetal', dx - 1.1, dx + 1.1, y0, y0 + 0.9, DOCK_Z, DOCK_Z + 0.03, bevel=0.0)
        p.obox('CnMetal', (dx, y0 - 0.22, DOCK_Z - 0.05), (2.2, 0.5, 0.04), roll=-14, bevel=0.0)
    # a escada da ponta da frente (descendo pra +X) e o corrimão
    steps = 5
    for k in range(steps):
        xa = x1 + k * 0.3
        p.box('CnPlinth', xa, xa + 0.3, y0 + 0.3, y1 - 0.2, G, DOCK_Z - k * (DOCK_Z - G) / steps, bevel=0.0)
    railing(p, [(x1, y0 + 0.3), (x1 + 1.5, y0 + 0.3)], DOCK_Z - 0.4, h=0.9, m='Trim', post_step=0.75)
    railing(p, [(x0 + 0.1, y0 + 0.1), (x0 + 0.1, y1 - 0.1)], DOCK_Z, h=1.0, m='Trim', post_step=1.0)
    # as caixas na doca (as que já desceram do caminhão)
    for (xx, yy, n) in ((-7.7, 1.85, 2), (2.9, 1.85, 2), (3.9, 1.85, 1)):
        pallet(p, xx, yy, DOCK_Z, 1.3, 1.1)
        for k in range(n):
            crate(p, xx, yy, DOCK_Z + 0.15 + k * 0.9)


def crane(p):
    """A ponte rolante (a palavra): as 6 colunas vermelhas nas sapatas, as faixas de perigo nas colunas da pista, as
    mãos-francesas, as vigas de caminho com o trilho e os batentes, a ponte de 2 vigas com a estrela, os truques com as rodas,
    a passarela com o corrimão, a cabine pendurada com a escada, as balizas e os holofotes."""
    run_x0, run_x1 = COL_X[0] - 0.6, COL_X[-1] + 0.7
    for (yi, y) in enumerate(ROW_Y):
        zb = DOCK_Z if y > 0 else G
        for x in COL_X:
            p.box('CnConcrete', x - 0.45, x + 0.45, y - 0.45, y + 0.45, zb, zb + 0.3, bevel=0.03)
            p.box('Player', x - 0.25, x + 0.25, y - 0.25, y + 0.25, zb + 0.3, RUN_Z0, bevel=0.03)
            p.box('CnMetal', x - 0.34, x + 0.34, y - 0.34, y + 0.34, zb + 0.3, zb + 0.38, bevel=0.0)
            for k in range(4 if y < 0 else 0):                                       # as faixas de perigo (o caminhão passa ao lado)
                zz = zb + 0.4 + k * 0.3
                p.box('Hazard' if k % 2 == 0 else 'Dark', x - 0.27, x + 0.27, y - 0.27, y + 0.27, zz, zz + 0.3, bevel=0.0)
            for s in (-1, 1):                                                        # as mãos-francesas
                if (x == COL_X[0] and s < 0) or (x == COL_X[-1] and s > 0):
                    continue
                a = Vector((x + s * 0.2, y, RUN_Z0 - 1.2))
                b = Vector((x + s * 1.2, y, RUN_Z0))
                d = b - a
                p.cyl('Player', tuple((a + b) / 2), tuple(d), 0.09, d.length, seg=8, bevel=0.0)
            # o holofote da coluna, virado pra pista
            fy = y + (0.3 if y < 0 else -0.3)
            p.box('Dark', x - 0.2, x + 0.2, min(fy, fy + (0.25 if y < 0 else -0.25)), max(fy, fy + (0.25 if y < 0 else -0.25)), 5.4, 5.7, bevel=0.0)
            p.obox('Light', (x, fy + (0.27 if y < 0 else -0.27), 5.5), (0.32, 0.03, 0.2), roll=(25 if y < 0 else -25), bevel=0.0)
        # a viga de caminho, a aba, o trilho e os batentes
        p.box('Player', run_x0, run_x1, y - 0.25, y + 0.25, RUN_Z0, RUN_Z1, bevel=0.03)
        p.box('Player', run_x0, run_x1, y - 0.34, y + 0.34, RUN_Z1 - 0.08, RUN_Z1 + 0.03, bevel=0.0)
        p.box('Player', run_x0, run_x1, y - 0.32, y + 0.32, RUN_Z0 - 0.03, RUN_Z0 + 0.07, bevel=0.0)
        p.box('CnMetal', run_x0, run_x1, y - 0.05, y + 0.05, RUN_Z1 + 0.03, RUN_Z1 + 0.1, bevel=0.0)
        for xx in (run_x0 + 0.15, run_x1 - 0.15):
            p.box('Hazard', xx - 0.15, xx + 0.15, y - 0.2, y + 0.2, RUN_Z1, RUN_Z1 + 0.35, bevel=0.02)
        for k in range(int((run_x1 - run_x0) / 1.2)):                                # as nervuras
            xx = run_x0 + 0.6 + k * 1.2
            for s in (-1, 1):
                p.box('Player', xx - 0.04, xx + 0.04, min(y + s * 0.25, y + s * 0.31), max(y + s * 0.25, y + s * 0.31), RUN_Z0 + 0.07, RUN_Z1 - 0.08, bevel=0.0)
    # a ponte: os truques nas pontas (com as rodas no trilho), as 2 vigas-caixão, a estrela, a passarela, a cabine
    gx = (BRIDGE_X - 0.6, BRIDGE_X + 0.6)
    y0, y1 = ROW_Y[0] - 0.55, ROW_Y[1] + 0.55
    for y in ROW_Y:
        p.box('CnMetal', BRIDGE_X - 1.3, BRIDGE_X + 1.3, y - 0.36, y + 0.36, RUN_Z1 + 0.18, RUN_Z1 + 0.7, bevel=0.03)
        for xx in (BRIDGE_X - 0.9, BRIDGE_X + 0.9):
            p.cyl('Dark', (xx, y, RUN_Z1 + 0.3), 'y', 0.22, 0.2, seg=16, bevel=0.0)
        for xx in (BRIDGE_X - 1.3, BRIDGE_X + 1.3):
            p.box('Hazard', xx - 0.08, xx + 0.08, y - 0.25, y + 0.25, RUN_Z1 + 0.25, RUN_Z1 + 0.6, bevel=0.0)
    for g in gx:
        p.box('Player', g - 0.22, g + 0.22, y0, y1, RUN_Z1 + 0.45, GIRDER_Z1 - 0.03, bevel=0.04)
        p.box('Player', g - 0.3, g + 0.3, y0 - 0.02, y1 + 0.02, GIRDER_Z1 - 0.08, GIRDER_Z1, bevel=0.0)
        p.box('CnMetal', g - 0.04, g + 0.04, y0 + 0.3, y1 - 0.3, GIRDER_Z1, GIRDER_Z1 + 0.06, bevel=0.0)   # o trilho do carrinho
        for k in range(8):
            yy = y0 + 0.5 + k * (y1 - y0 - 1.0) / 7
            p.box('Player', g + 0.22, g + 0.27, yy - 0.04, yy + 0.04, RUN_Z1 + 0.5, GIRDER_Z1 - 0.06, bevel=0.0)
    for yy in (y0 + 0.3, y1 - 0.3, (y0 + y1) / 2):                                  # as travessas entre as vigas
        p.box('Player', gx[0], gx[1], yy - 0.12, yy + 0.12, GIRDER_Z1 - 0.45, GIRDER_Z1 - 0.1, bevel=0.0)
    # a estrela dourada na placa vermelha, no meio da viga da frente (vista da câmera do jogo)
    sy = (y0 + y1) / 2
    gf = gx[1] + 0.27
    p.box('Player', gf - 0.02, gf + 0.08, sy - 0.62, sy + 0.62, RUN_Z1 + 0.42, GIRDER_Z1 + 0.12, bevel=0.02)
    p.cyl('CnGold', (gf + 0.09, sy, (RUN_Z1 + GIRDER_Z1) / 2 + 0.27), 'x', 0.5, 0.02, seg=32, bevel=0.0)
    p.cyl('Player', (gf + 0.1, sy, (RUN_Z1 + GIRDER_Z1) / 2 + 0.27), 'x', 0.44, 0.02, seg=32, bevel=0.0)
    star_wall(p, (gf + 0.11, sy, (RUN_Z1 + GIRDER_Z1) / 2 + 0.27), '+x', 0.38, m='CnGold', t=0.015)
    # a passarela do lado de fora (−X) e o corrimão
    p.box('CnMetal', gx[0] - 0.95, gx[0] - 0.22, y0 + 0.2, y1 - 0.2, RUN_Z1 + 0.85, RUN_Z1 + 0.9, bevel=0.0)
    for k in range(int((y1 - y0 - 0.4) / 0.25)):
        yy = y0 + 0.3 + k * 0.25
        p.box('Dark', gx[0] - 0.93, gx[0] - 0.24, yy - 0.02, yy + 0.02, RUN_Z1 + 0.84, RUN_Z1 + 0.86, bevel=0.0)
    railing(p, [(gx[0] - 0.92, y0 + 0.25), (gx[0] - 0.92, y1 - 0.25)], RUN_Z1 + 0.9, h=0.9, m='Trim', post_step=1.2)
    # a cabine pendurada na ponta −Y (o operador), com os vidros pra pista e a escadinha
    cy_ = ROW_Y[0] + 1.1
    p.box('CnMetal', BRIDGE_X - 0.75, BRIDGE_X + 0.75, cy_ - 0.65, cy_ + 0.65, 5.35, RUN_Z1 + 0.45, bevel=0.04)
    p.box('Glass', BRIDGE_X + 0.75, BRIDGE_X + 0.78, cy_ - 0.5, cy_ + 0.5, 5.9, 6.75, bevel=0.0)
    p.box('Glass', BRIDGE_X - 0.6, BRIDGE_X + 0.6, cy_ + 0.65, cy_ + 0.68, 5.9, 6.75, bevel=0.0)
    p.box('Glass', BRIDGE_X - 0.6, BRIDGE_X + 0.6, cy_ - 0.68, cy_ - 0.65, 5.9, 6.75, bevel=0.0)
    p.box('Player', BRIDGE_X - 0.78, BRIDGE_X + 0.78, cy_ - 0.68, cy_ + 0.68, 5.3, 5.45, bevel=0.0)
    ladder(p, BRIDGE_X - 0.95, cy_ - 0.2, cy_ + 0.2, 5.4, RUN_Z1 + 0.9)
    # as balizas nas pontas da ponte
    for yy in (y0 + 0.2, y1 - 0.2):
        p.cyl('Dark', (gx[1], yy, GIRDER_Z1 + 0.06), 'z', 0.1, 0.12, seg=12, bevel=0.0)
        p.cyl('Beacon', (gx[1], yy, GIRDER_Z1 + 0.2), 'z', 0.08, 0.16, seg=12, bevel=0.0, smooth=True)


def truck_bay(p):
    """A faixa do caminhão no piso: a caixa amarela onde ele para (a caçamba debaixo do carrinho), os batentes de roda, a
    linha tracejada da entrada."""
    x0, x1, y0, y1 = -5.4, 2.4, -4.4, -0.2
    for (a0, a1, b0, b1) in ((x0, x1, y0, y0 + 0.12), (x0, x1, y1 - 0.12, y1), (x0, x0 + 0.12, y0, y1), (x0 - 0.3, x0 - 0.18, y0, y1)):
        p.box('Hazard', a0, a1, b0, b1, G, G + 0.012, bevel=0.0)
    for yy in (-3.35, -1.25):
        p.box('Hazard', x0 + 0.2, x0 + 0.5, yy - 0.5, yy + 0.5, G, G + 0.18, bevel=0.03)
        for k in range(3):
            p.box('Dark', x0 + 0.19, x0 + 0.51, yy - 0.5 + k * 0.36, yy - 0.32 + k * 0.36, G + 0.0, G + 0.185, bevel=0.0)
    for k in range(4):
        xx = x1 + 0.8 + k * 2.0
        p.box('Hazard', xx, xx + 1.0, -2.36, -2.24, G, G + 0.012, bevel=0.0)


def office(p):
    """O escritório pequeno na frente (−Y): o rodapé, as janelas, a porta com a marquise e a luz, o telhadinho de pagode,
    o estandarte na frente, a antena; o poste do alto-falante ao lado."""
    x0, x1, y0, y1, h = 3.4, 9.2, -8.75, -5.55, 3.5
    cx, cy, ax, ay = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
    wins = {'+y': [(xx - 0.5, xx + 0.5, 1.3, 2.4) for xx in (4.4, 5.8)] + [(7.4, 8.4, G, G + 2.2)],
            '-y': [(xx - 0.5, xx + 0.5, 1.3, 2.4) for xx in (4.5, 6.3, 8.1)],
            '-x': [(cy - 0.5, cy + 0.5, 1.3, 2.4)], '+x': [(y1 - 1.35, y1 - 0.45, 1.3, 2.4)]}
    cn_block(p, x0, x1, y0, y1, G, h, t=0.32, skin=('+x', '-x', '+y', '-y'), holes=wins, roof_t=0.25, parapet=0.0, cornice=False,
             corners=True, pier=0.55, interior=True, plinth=0.5, pw=1.45, zs=[G, 1.2, h - 0.25])
    for f, at in (('+y', y1), ('-y', y0), ('-x', x0), ('+x', x1)):
        F = WallFace(f, at, 0.0)
        for (a, b, c, d) in wins[f]:
            if d - c < 2.0:
                cn_window_v(p, F, a, b, c, d, t=0.32, mull=0)
    cn_frieze(p, cx, cy, ax, ay, h + 0.02, h=0.3)
    cn_roof(p, cx, cy, ax, ay, h + 0.02, 1.15, over=0.6, curl=0.38, sweep=0.2, rib_step=0.4)
    # a porta (face +Y, pra pista) com a marquise e a luz
    p.box('Dark', 7.4, 8.4, y1 - 0.24, y1 - 0.2, G, G + 2.2, bevel=0.0)                                  # a porta, no vão
    p.box('CnMetal#frame', 7.25, 7.4, y1 - 0.1, y1 + 0.2, G, G + 2.3, bevel=0.02)
    p.box('CnMetal#frame', 8.4, 8.55, y1 - 0.1, y1 + 0.2, G, G + 2.3, bevel=0.02)
    p.obox('CnMetal', (7.9, y1 + 0.65, G + 2.55), (1.8, 1.1, 0.08), roll=10, bevel=0.0)
    lamp(p, 6.9, y1 + 0.12, G + 2.4, face='+y')
    wall_banner(p, x1 + 0.12, y0 + 1.05, h - 0.2, '+x', w=1.0, h=2.4, notch=0.35)
    # a antena de rádio no telhado
    p.cyl('Trim', (4.2, y0 + 0.5, h + 1.5), 'z', 0.03, 3.0, seg=6, bevel=0.0)
    p.cyl('Beacon', (4.2, y0 + 0.5, h + 3.05), 'z', 0.05, 0.1, seg=8, bevel=0.0)
    # o poste do alto-falante (na frente, entre o escritório e a pista): 3 cornetas e a placa da estrela
    px, py = 9.9, -4.95
    p.box('CnConcrete', px - 0.4, px + 0.4, py - 0.4, py + 0.4, G, G + 0.4, bevel=0.03)
    p.cyl('CnMetal', (px, py, (G + 0.4 + 6.2) / 2), 'z', 0.12, 6.2 - G - 0.4, seg=12, bevel=0.0)
    for k, yaw in enumerate((0.0, 70.0, -70.0)):
        a = math.radians(yaw)
        horn(p, (px + math.cos(a) * 0.15, py + math.sin(a) * 0.15, 5.7), (math.cos(a), math.sin(a), -0.25), r1=0.36, L=0.8)
    p.box('Player', px + 0.13, px + 0.18, py - 0.35, py + 0.35, 4.4, 5.1, bevel=0.0)
    star_wall(p, (px + 0.18, py, 4.75), '+x', 0.26, m='CnGold', t=0.01)
    p.cyl('CnMetal', (px, py, 6.25), 'z', 0.2, 0.12, seg=12, bevel=0.0)


def yard(p):
    """As pilhas de caixas verde-oliva com a estrela, nos paletes (poucas: a bíblia não quer montanha de caixa)."""
    for (xx, yy) in ((-3.0, -8.6), (-1.7, -8.6), (-3.0, -7.45)):
        pallet(p, xx, yy, G, 1.3, 1.1)
    for (xx, yy, n) in ((-3.0, -8.6, 3), (-1.7, -8.6, 2), (-3.0, -7.45, 2)):
        for k in range(n):
            crate(p, xx, yy, G + 0.15 + k * 0.9)
    pallet(p, 0.4, -8.4, G, 1.3, 1.1)
    crate(p, 0.4, -8.4, G + 0.15)
    crate(p, 0.0, -6.9, G, yaw=18.0)


def boiler(p):
    """A casa da caldeira (canto de trás, −Y) com a chaminé listrada fina, os 3 estais, a grelha e a porta."""
    x0, x1, y0, y1, h = -10.6, -7.2, -9.6, -6.6, 3.2
    cn_block(p, x0, x1, y0, y1, G, h, t=0.3, skin=('+x', '-x', '+y', '-y'),
             holes={'-x': [(-8.9, -7.3, 1.2, 2.4)], '-y': [(-9.8, -8.0, 1.4, 2.4)], '+x': [(-8.6, -7.6, G, G + 2.2)]},
             roof_t=0.25, parapet=0.28, corners=True, pier=0.55, interior=False, plinth=0.6, pw=1.6, zs=[G, 1.6, h - 0.25])
    louver(p, x0 - 0.02, x0, -8.9, -7.3, 1.2, 2.4, '-x', n=4)
    p.box('CnMetal#door', x1 - 0.2, x1 - 0.16, -8.6, -7.6, G, G + 2.2, bevel=0.012)
    lamp(p, x1 + 0.12, -7.2, G + 2.4, face='+x')
    louver(p, -9.8, -8.0, y0 - 0.02, y0, 1.4, 2.4, '-y', n=4)
    c = (-9.0, -8.1)
    p.cyl('CnConcrete', (c[0], c[1], h + 0.2), 'z', 0.75, 0.4, seg=24, bevel=0.02)
    chimney5(p, c, 0.5, 0.38, h + 0.4, 10.2 - h - 0.4)
    for (ax_, ay_, az) in ((-10.45, -9.45, h + 0.3), (-7.35, -9.45, h + 0.3), (-7.0, -5.2, G)):
        a = Vector((c[0], c[1], 8.3))
        b = Vector((ax_, ay_, az))
        d = b - a
        p.cyl('Dark', tuple((a + b) / 2), tuple(d), 0.018, d.length, seg=6, bevel=0.0)
        p.box('CnMetal', ax_ - 0.12, ax_ + 0.12, ay_ - 0.12, ay_ + 0.12, az, az + 0.15, bevel=0.0)
    disc_ring(p, 'CnMetal', c, 0.48, 0.4, 8.25, 8.4, seg=20)


def water_tower(p, c=(8.5, 8.2)):
    """A caixa d'água redonda nas 4 pernas (o redondo do lote), com o X das pernas, o deque com o corrimão, a faixa vermelha,
    o telhado cônico e a escada."""
    x, y = c
    leg, zt = 0.95, 3.7
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p.box('CnConcrete', x + sx * leg - 0.3, x + sx * leg + 0.3, y + sy * leg - 0.3, y + sy * leg + 0.3, G, G + 0.3, bevel=0.03)
        p.cyl('CnMetal', (x + sx * leg, y + sy * leg, (G + zt) / 2), 'z', 0.12, zt - G, seg=10, bevel=0.0)
    for (a, b) in (((-1, -1), (1, -1)), ((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1))):
        for (za, zb) in ((G + 0.4, zt - 0.2), (zt - 0.2, G + 0.4)):
            pa = Vector((x + a[0] * leg, y + a[1] * leg, za))
            pb = Vector((x + b[0] * leg, y + b[1] * leg, zb))
            d = pb - pa
            p.cyl('CnMetal', tuple((pa + pb) / 2), tuple(d), 0.04, d.length, seg=6, bevel=0.0)
    p.box('CnMetal', x - 1.5, x + 1.5, y - 1.5, y + 1.5, zt, zt + 0.15, bevel=0.02)
    railing(p, [(x - 1.45, y - 1.45), (x + 1.45, y - 1.45), (x + 1.45, y + 1.45), (x - 1.45, y + 1.45), (x - 1.45, y - 1.45)],
            zt + 0.15, h=0.9, m='Trim', post_step=1.0)
    p.cyl('CnPlinth', (x, y, zt + 0.15 + 1.0), 'z', 1.25, 2.0, seg=36, bevel=0.02, smooth=True)
    p.cyl('Player', (x, y, zt + 0.15 + 1.7), 'z', 1.265, 0.35, seg=36, bevel=0.0, smooth=True)
    for zz in (zt + 0.6, zt + 1.25):
        p.cyl('Trim', (x, y, zz), 'z', 1.27, 0.05, seg=36, bevel=0.0)
    p.cyl('CnRoof', (x, y, zt + 2.15 + 0.3), 'z', 1.38, 0.6, seg=36, r2=0.12, bevel=0.0)
    p.cyl('CnGold', (x, y, zt + 2.85), 'z', 0.1, 0.2, seg=8, r2=0.02, bevel=0.0)
    ladder(p, x + leg + 0.15, y - 0.25, y + 0.25, G, zt + 0.15)
    ladder(p, x + 1.3, y - 0.2, y + 0.2, zt + 0.15, zt + 2.2)


def base():
    p = BPart(PREFIX + '_Base')
    p.bm('CnRoof', 0.0)                         # a telha sem chanfro (só multiplicaria triângulo)
    foundation(p, HX, HY, m='CnPlinth', z=G)
    warehouse(p)
    dock(p)
    crane(p)
    truck_bay(p)
    office(p)
    yard(p)
    boiler(p)
    water_tower(p)
    return p.finish()


def trolley():
    """O carrinho da ponte rolante (peça separada, origem no eixo dele, no topo das vigas da ponte): o quadro com as 4 rodas
    nos trilhos das vigas, o tambor do guincho, o motor e a caixa de engrenagem, a tampa vermelha, os 2 cabos, o balancim
    amarelo com os 4 cabos curtos e a caixa de suprimento pendurada (descendo da caçamba pra doca)."""
    p = BPart(PREFIX + '_Crane')
    gx = 0.6                                                                         # as vigas da ponte em ±0,6 (X local)
    p.box('CnMetal', -0.85, 0.85, -0.75, 0.75, 0.12, 0.38, bevel=0.03)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p.cyl('Dark', (sx * gx, sy * 0.5, 0.16), 'y', 0.14, 0.12, seg=12, bevel=0.0)
    p.cyl('Trim', (0.0, 0.0, 0.72), 'y', 0.32, 1.1, seg=20, bevel=0.0)              # o tambor
    for s in (-1, 1):
        p.cyl('CnMetal', (0.0, s * 0.58, 0.72), 'y', 0.4, 0.08, seg=20, bevel=0.0)
    p.box('CnMetal', -0.85, -0.35, -0.7, -0.15, 0.38, 0.9, bevel=0.03)              # o motor e a caixa
    p.box('CnMetal', 0.35, 0.85, 0.15, 0.7, 0.38, 0.85, bevel=0.03)
    p.box('Player', -0.86, -0.34, -0.71, -0.14, 0.86, 0.92, bevel=0.0)
    p.box('Hazard', -0.86, 0.86, -0.76, 0.76, 0.3, 0.36, bevel=0.0)
    # os cabos (descem entre as 2 vigas), o balancim e a caixa
    zc = -(GIRDER_Z1 - 3.95)                                                           # o balancim a 3,95 m do chão
    for dy in (-0.2, 0.2):
        p.cyl('Dark', (0.0, dy, (0.6 + zc) / 2), 'z', 0.02, 0.6 - zc, seg=6, bevel=0.0)
    p.box('Hazard', -0.75, 0.75, -0.14, 0.14, zc - 0.2, zc, bevel=0.02)
    p.box('CnMetal', -0.2, 0.2, -0.25, 0.25, zc - 0.05, zc + 0.25, bevel=0.02)
    zb = zc - 0.95
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        a = Vector((sx * 0.68, sy * 0.1, zc - 0.2))
        b = Vector((sx * 0.55, sy * 0.45, zb + 0.9))
        d = b - a
        p.cyl('Dark', tuple((a + b) / 2), tuple(d), 0.015, d.length, seg=6, bevel=0.0)
    crate(p, 0.0, 0.0, zb, star_faces=('top', '+x', '-y'))
    return p.finish()


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(bake_in_place(trolley(), CRANE_AT), OUT)   # (31/10) o cozimento com o carrinho no lugar dele
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, WZ + 0.8, post='CnMetal'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 22 x 20 m; crane (carrinho) at_m', CRANE_AT, 'corre em Y', CRANE_TRAVEL_Y, '; ponte em X', BRIDGE_X,
          '; caminhao para na faixa amarela X -5.4..2.4, Y -4.4..-0.2 (centro -1.5, -2.3)')
    print('MODEL fire_points_m', [(-5.0, 5.6, 7.0), (1.6, 5.6, 7.0), (6.3, -7.15, 4.3)])


if __name__ == '__main__':
    main()
