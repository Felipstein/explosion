"""O quartel (Barracks) da China provisório, modelado no Blender por script (o elenco das 3 facções, 31/10/2026: docs/ELENCO.md §4.1).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_barracks.py -- captures/game/models/china_barracks

Onde nascem o Fuzileiro do ELP (em par) e o Caçador de tanque. Na bíblia da China (docs/CONCEITO_CONSTRUCOES.md §2): concreto
escuro puxado pro oliva, o telhado de pagode verde com a borda vermelha, a estrela vermelha, o alto-falante, os estandartes (2
aqui, mais a bandeira do mastro), nunca trem ou trilho, nunca cruz vermelha.

A palavra (o que lê de cima): O PÁTIO DE TREINO MURADO COM O PORTÃO DE PAGODE VERMELHO E O MASTRO DA BANDEIRA. Inspiração real:
o paifang (o portão cerimonial chinês de 3 vãos e 4 colunas, com um telhadinho em cima de cada vão, as colunas vermelhas, as
vigas pintadas e a placa no meio) e o pátio de parada dos quartéis do ELP (o mastro no centro, o dormitório comprido no fundo,
o muro com o capeamento de telha). Peça por peça:
- o portão (paifang) na frente (+X): 4 colunas vermelhas redondas nas pedras-base com as escoras, o vão do meio alto com a placa
  da estrela vermelha, os 2 vãos do lado mais baixos, 3 telhadinhos de pagode (o do meio maior), as mísulas douradas, as 2
  lanternas vermelhas penduradas; os soldados saem por ele;
- o muro baixo (1,5 m) com o capeamento de telha verde (o mini-telhado em cima do muro) e os pilaretes;
- o mastro da bandeira no centro do pátio, na base de 3 degraus, com a bandeira vermelha grande e a estrela dourada (a bandeira
  voa pro lado, de frente pra câmera do jogo), o quadro branco da praça de parada e o caminho do portão à porta;
- o dormitório comprido no fundo, 1 andar, com o telhado de pagode de quadril, a varanda com as colunas vermelhas, a fila de
  janelas, a porta com a placa da estrela, os 2 estandartes pendurados na varanda, as chaminés de fogão;
- o lado +Y do pátio: a pista de obstáculos (as 3 muretas, a escada horizontal, o muro de escalar);
- o lado −Y: os 4 bonecos de palha de treino (com o alvo no peito) e o cavalete de fuzis; o poste do alto-falante no canto.

Lote 16 × 20 m (X × Y), frente em +X (o portão). Alturas: o muro 1,8 m com o capeamento, o dormitório 3,6 m de parede e a
cumeeira a ~5,9 m, o portão 7,1 m com os ornamentos, o alto-falante 6,8 m, o mastro 13 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_Barracks_Base: tudo (o portão, o muro, o pátio, o mastro com a bandeira, o dormitório, a pista, os bonecos);
  SM_China_Barracks_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
O telhado de pagode, o estandarte e as cornetas vêm do model_china_war_factory.py (a mesma família).
Materiais (slots do model_faction_kit.py): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (o vermelho: colunas do portão,
borda dos telhados, bandeira, estandartes, lanternas, estrelas), Dark, Trim, Glass, Light, Beacon, Paint (o branco dos alvos);
Wood (os postes, a pista, o cavalete) e Cloth (a palha dos bonecos).
"""
import math
import os
import sys

import bpy  # noqa: F401
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, DETAIL, Part, clear_scene, export, scaffold  # noqa: E402
from model_faction_kit import foundation, lamp, ladder, railing, polygon_prism, star_points, star_wall, revolve  # noqa: E402
from model_china_war_factory import cn_roof, cn_frieze, wall_banner, horn, battered, louver  # noqa: E402
from model_building_parts import BPart, WallFace, cn_block, cn_window  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_barracks')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_Barracks'
COLORS.setdefault('Wood', (120, 88, 52))
COLORS.setdefault('Cloth', (150, 128, 92))

HX, HY = 8.0, 10.0               # o lote 16 × 20
G = 0.14                         # o topo da laje (a fundação do kit)
WALL_H = 1.5                     # o muro baixo (sem o capeamento)
FRONT_X = 6.6                    # a linha do muro da frente e do portão
SIDE_Y = 9.35                    # a linha dos muros dos lados
GATE_MAIN, GATE_SIDE = 1.95, 3.65  # as colunas do portão (Y)
# o dormitório
DX0, DX1, DY, DZ = -6.6, -2.8, 8.3, 3.6
VER_X = -1.45                    # a borda da varanda
FLAG = (2.4, 0.0)                # o mastro


# ===================================================================================================== peças

def tiled_wall(p, x0, x1, y0, y1, h=WALL_H, z0=G):
    """O trecho de muro com o rodapé claro e o capeamento de telha verde (o mini-telhado de 2 águas em cima do muro, com a
    cumeeira e as pontas de telha nas 2 beiradas): o muro dos pátios chineses."""
    along_x = (x1 - x0) >= (y1 - y0)
    p.box('CnConcrete', x0, x1, y0, y1, z0, z0 + h, bevel=0.04)
    p.box('CnPlinth', x0 - 0.05, x1 + 0.05, y0 - 0.05, y1 + 0.05, z0, z0 + 0.3, bevel=0.02)
    o, zr, rise = 0.18, z0 + h, 0.28
    if along_x:
        yc = (y0 + y1) / 2
        hw = (y1 - y0) / 2 + o
        p.loft('CnRoof', [[(x0 - 0.05, yc - hw, zr), (x0 - 0.05, yc + hw, zr), (x0 - 0.05, yc + hw, zr + 0.06), (x0 - 0.05, yc, zr + rise), (x0 - 0.05, yc - hw, zr + 0.06)],
                          [(x1 + 0.05, yc - hw, zr), (x1 + 0.05, yc + hw, zr), (x1 + 0.05, yc + hw, zr + 0.06), (x1 + 0.05, yc, zr + rise), (x1 + 0.05, yc - hw, zr + 0.06)]], bevel=0.0)
        p.box('CnRoof', x0 - 0.08, x1 + 0.08, yc - 0.09, yc + 0.09, zr + rise - 0.04, zr + rise + 0.1, bevel=0.0)
        n = int((x1 - x0) / 0.32)
        for k in range(n):
            xx = x0 + (k + 0.5) * (x1 - x0) / n
            for s in (-1, 1):
                p.box('CnRoof', xx - 0.06, xx + 0.06, yc + s * hw - 0.04, yc + s * hw + 0.04, zr - 0.02, zr + 0.1, bevel=0.0)
    else:
        xc = (x0 + x1) / 2
        hw = (x1 - x0) / 2 + o
        p.loft('CnRoof', [[(xc - hw, y0 - 0.05, zr), (xc + hw, y0 - 0.05, zr), (xc + hw, y0 - 0.05, zr + 0.06), (xc, y0 - 0.05, zr + rise), (xc - hw, y0 - 0.05, zr + 0.06)],
                          [(xc - hw, y1 + 0.05, zr), (xc + hw, y1 + 0.05, zr), (xc + hw, y1 + 0.05, zr + 0.06), (xc, y1 + 0.05, zr + rise), (xc - hw, y1 + 0.05, zr + 0.06)]], bevel=0.0)
        p.box('CnRoof', xc - 0.09, xc + 0.09, y0 - 0.08, y1 + 0.08, zr + rise - 0.04, zr + rise + 0.1, bevel=0.0)
        n = int((y1 - y0) / 0.32)
        for k in range(n):
            yy = y0 + (k + 0.5) * (y1 - y0) / n
            for s in (-1, 1):
                p.box('CnRoof', xc + s * hw - 0.04, xc + s * hw + 0.04, yy - 0.06, yy + 0.06, zr - 0.02, zr + 0.1, bevel=0.0)


def wall_post(p, x, y, h=WALL_H + 0.45, s=0.32):
    """O pilarete do muro (mais alto, com o chapéu de concreto)."""
    p.box('CnConcrete', x - s, x + s, y - s, y + s, G, G + h, bevel=0.03)
    p.box('CnPlinth', x - s - 0.07, x + s + 0.07, y - s - 0.07, y + s + 0.07, G + h, G + h + 0.12, bevel=0.02)
    p.box('CnPlinth', x - s - 0.05, x + s + 0.05, y - s - 0.05, y + s + 0.05, G, G + 0.3, bevel=0.02)


def lantern(p, x, y, z_top, drop=0.3):
    """A lanterna vermelha chinesa pendurada: o cordão, a tampa e o fundo dourados, o corpo de barril vermelho, a borla."""
    p.cyl('Dark', (x, y, z_top - drop / 2), 'z', 0.012, drop, seg=6, bevel=0.0)
    zt = z_top - drop
    p.cyl('CnGold', (x, y, zt - 0.04), 'z', 0.12, 0.08, seg=16, bevel=0.0)
    prof = [(0.12, zt - 0.08), (0.24, zt - 0.14), (0.3, zt - 0.28), (0.3, zt - 0.5), (0.24, zt - 0.64), (0.12, zt - 0.7)]
    revolve(p, 'Player', (x, y), prof, seg=20, smooth=True)
    p.cyl('CnGold', (x, y, zt - 0.74), 'z', 0.12, 0.08, seg=16, bevel=0.0)
    p.cyl('Player', (x, y, zt - 0.92), 'z', 0.035, 0.28, seg=8, r2=0.08, bevel=0.0)


def paifang(p):
    """O portão cerimonial (paifang) de 3 vãos e 4 colunas na frente do pátio (a palavra): as colunas vermelhas redondas nas
    pedras-base com as escoras de pedra, as vigas pintadas de verde com as pontas douradas, a placa da estrela vermelha no
    vão do meio (dos 2 lados), as plaquinhas vermelhas dos vãos do lado, as mísulas douradas, os 3 telhadinhos de pagode e
    as 2 lanternas."""
    gx = FRONT_X
    z_main, z_side = G + 5.4, G + 4.15
    for y in (-GATE_SIDE, -GATE_MAIN, GATE_MAIN, GATE_SIDE):
        top = z_main if abs(y) < 3 else z_side
        p.box('CnPlinth', gx - 0.45, gx + 0.45, y - 0.45, y + 0.45, G, G + 0.55, bevel=0.04)
        p.cyl('Player', (gx, y, (G + 0.55 + top) / 2), 'z', 0.26, top - G - 0.55, seg=20, bevel=0.0, smooth=True)
        p.cyl('CnGold', (gx, y, G + 0.62), 'z', 0.3, 0.14, seg=20, bevel=0.0)
        for s in (-1, 1):                                                            # as escoras de pedra (frente e trás)
            p.prism_xz('CnPlinth', [(gx + s * 0.4, G), (gx + s * 1.15, G), (gx + s * 0.4, G + 1.35)] if s > 0 else
                       [(gx - 1.15, G), (gx - 0.4, G), (gx - 0.4, G + 1.35)], y - 0.16, y + 0.16, bevel=0.02)
            p.cyl('CnPlinth', (gx + s * 0.7, y, G + 0.62), 'y', 0.3, 0.34, seg=16, bevel=0.0)
    # o vão do meio: a viga de baixo, a placa com a estrela, a viga de cima, as mísulas e o telhado grande
    def beam(y0, y1, z0, z1):
        p.box('CnRoof#beam', gx - 0.2, gx + 0.2, y0, y1, z0, z1, bevel=0.02)
        for yy in (y0 + 0.12, y1 - 0.12):
            p.box('CnGold', gx - 0.21, gx + 0.21, yy - 0.12, yy + 0.12, z0 + 0.04, z1 - 0.04, bevel=0.0)
        p.box('CnGold', gx - 0.21, gx + 0.21, (y0 + y1) / 2 - 0.6, (y0 + y1) / 2 + 0.6, (z0 + z1) / 2 - 0.03, (z0 + z1) / 2 + 0.03, bevel=0.0)
    beam(-GATE_MAIN - 0.3, GATE_MAIN + 0.3, G + 3.55, G + 3.95)
    beam(-GATE_MAIN - 0.3, GATE_MAIN + 0.3, G + 4.85, G + 5.15)
    p.box('CnGold', gx - 0.16, gx + 0.16, -0.95, 0.95, G + 3.95, G + 4.85, bevel=0.02)                      # a placa
    for s in (-1, 1):
        p.box('CnRoof', gx + s * 0.16 - (0.0 if s > 0 else 0.02), gx + s * 0.16 + (0.02 if s > 0 else 0.0), -0.82, 0.82, G + 4.03, G + 4.77, bevel=0.0)
        star_wall(p, (gx + s * 0.18, 0.0, G + 4.4), '+x' if s > 0 else '-x', 0.36, t=0.025)
    for s in (-1, 1):                                                                # o entalhe entre as vigas, dos lados da placa
        p.box('Player', gx - 0.12, gx + 0.12, s * 0.95, s * (GATE_MAIN - 0.26), G + 4.05, G + 4.75, bevel=0.0)
    cn_frieze(p, gx, 0.0, 0.3, GATE_MAIN + 0.25, z_main + 0.3, h=0.3, step=0.55)
    p.box('CnRoof', gx - 0.3, gx + 0.3, -GATE_MAIN - 0.25, GATE_MAIN + 0.25, z_main, z_main + 0.02, bevel=0.0)
    cn_roof(p, gx, 0.0, 0.3, GATE_MAIN + 0.25, z_main + 0.3, 0.85, over=0.5, curl=0.42, sweep=0.18, n=8, rib_step=0.32, tip=0.6)
    # os vãos do lado: a viga, a plaquinha vermelha com o friso dourado, as mísulas e o telhadinho
    for s in (-1, 1):
        yc = s * (GATE_MAIN + GATE_SIDE) / 2
        half = (GATE_SIDE - GATE_MAIN) / 2
        beam(yc - half - 0.25, yc + half + 0.25, G + 2.75, G + 3.05)
        p.box('CnGold', gx - 0.13, gx + 0.13, yc - half + 0.25, yc + half - 0.25, G + 3.05, G + 3.75, bevel=0.0)
        p.box('Player', gx - 0.15, gx + 0.15, yc - half + 0.33, yc + half - 0.33, G + 3.12, G + 3.68, bevel=0.0)
        cn_frieze(p, gx, yc, 0.26, half + 0.22, z_side + 0.28, h=0.28, step=0.5)
        p.box('CnRoof', gx - 0.26, gx + 0.26, yc - half - 0.22, yc + half + 0.22, z_side, z_side + 0.02, bevel=0.0)
        cn_roof(p, gx, yc, 0.26, half + 0.22, z_side + 0.28, 0.6, over=0.42, curl=0.32, sweep=0.15, n=6, rib_step=0.32, tip=0.6)
    # as 2 lanternas vermelhas do vão do meio
    for y in (-1.15, 1.15):
        lantern(p, gx, y, G + 3.55, drop=0.25)


def walls(p):
    """O muro baixo com o capeamento de telha (a frente com a abertura do portão e os 2 lados até o dormitório) e os
    pilaretes; as luzes nos pilaretes."""
    t = 0.25
    for s in (-1, 1):
        y0, y1 = (GATE_SIDE + 0.2, SIDE_Y) if s > 0 else (-SIDE_Y, -GATE_SIDE - 0.2)
        tiled_wall(p, FRONT_X - t, FRONT_X + t, y0, y1)
        tiled_wall(p, DX1 + 0.2, FRONT_X - t, s * SIDE_Y - t, s * SIDE_Y + t)
        for yy in (s * 6.4, s * SIDE_Y):
            wall_post(p, FRONT_X, yy)
        for xx in (-0.6, 3.0):
            wall_post(p, xx, s * SIDE_Y)
        lamp(p, FRONT_X + 0.32, s * 6.4, G + 1.45, face='+x')
    # as faixas de concreto do caminho: do portão ao mastro e do mastro à porta do dormitório
    p.box('CnConcrete', FLAG[0] + 1.7, FRONT_X + 1.2, -1.3, 1.3, G, G + 0.025, bevel=0.0)
    p.box('CnConcrete', VER_X, FLAG[0] - 1.7, -1.3, 1.3, G, G + 0.025, bevel=0.0)
    for xx in (FLAG[0] + 3.0, FLAG[0] + 4.4, -0.2):
        p.box('Dark', xx - 0.02, xx + 0.02, -1.3, 1.3, G + 0.025, G + 0.03, bevel=0.0)
    # o quadro branco da praça de parada em volta do mastro (onde a tropa forma)
    px0, px1, py0, py1 = -0.1, 4.9, -2.9, 2.9
    for (a0, a1, b0, b1) in ((px0, px1, py0, py0 + 0.1), (px0, px1, py1 - 0.1, py1), (px0, px0 + 0.1, py0, py1), (px1 - 0.1, px1, py0, py1)):
        p.box('Paint', a0, a1, b0, b1, G, G + 0.012, bevel=0.0)


def flag_y(p, pole, top, length, height, m='Player', phase=0.4):
    """A bandeira voando pro lado +Y (a câmera do jogo vê de frente): o pano com onda, preso no mastro, e a estrela dourada
    nos 2 lados, perto do mastro."""
    rings = []
    n = 12
    for k in range(n + 1):
        u = k / n
        y = pole[1] + length * u
        wave = 0.16 * math.sin(2 * math.pi * u * 1.2 + phase) * u
        x = pole[0] + wave
        h = height * (1 - 0.08 * u)
        z1 = top - 0.08 * u * u
        z0 = z1 - h
        t = 0.008
        rings.append([(x - t, y, z0), (x - t, y, z1), (x + t, y, z1), (x + t, y, z0)])
    # (01/11, "tudo que tem bandeira tem que ter física") no executor do detalhe o pano sai num material à parte (o jogo desenha o pano
    # no vento, com a estrela no material: o emblema 2) e a estrela de peça não entra
    if DETAIL['on']:
        COLORS.setdefault(m + 'Flag', COLORS.get(m, (200, 200, 200)))
        p.loft(m + 'Flag', rings, bevel=0.0, smooth=True)
        return
    p.loft(m, rings, bevel=0.0, smooth=True)
    u = 0.2
    wave = 0.16 * math.sin(2 * math.pi * u * 1.2 + phase) * u
    for s in (-1, 1):
        star_wall(p, (pole[0] + wave + s * 0.012, pole[1] + length * u, top - height * 0.3), '+x' if s > 0 else '-x',
                  height * 0.2, m='CnGold', t=0.01)


def flagpole(p):
    """O mastro no centro do pátio: a base de 3 degraus com as luzes nos cantos, o mastro afinando, o pomo dourado, a
    bandeira grande e a corda."""
    x, y = FLAG
    for k, s in enumerate((1.5, 1.15, 0.8)):
        p.box('CnPlinth', x - s, x + s, y - s, y + s, G + k * 0.22, G + (k + 1) * 0.22, bevel=0.03)
    p.box('CnConcrete', x - 0.35, x + 0.35, y - 0.35, y + 0.35, G + 0.66, G + 1.0, bevel=0.03)
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p.box('Light', x + sx * 1.35 - 0.08, x + sx * 1.35 + 0.08, y + sy * 1.35 - 0.08, y + sy * 1.35 + 0.08, G + 0.22, G + 0.3, bevel=0.0)
    top = 13.0
    p.cyl('CnMetal', (x, y, (G + 1.0 + top) / 2), 'z', 0.11, top - G - 1.0, seg=12, r2=0.06, bevel=0.0, smooth=True)
    p.cyl('CnGold', (x, y, top + 0.1), 'z', 0.12, 0.2, seg=12, bevel=0.0)
    p.cyl('CnGold', (x, y, top + 0.35), 'z', 0.07, 0.3, seg=8, r2=0.0, bevel=0.0)
    flag_y(p, (x, y + 0.07), top - 0.15, 3.6, 2.3)
    p.cyl('Trim', (x + 0.13, y, (G + 1.2 + top) / 2), 'z', 0.012, top - G - 1.2, seg=4, bevel=0.0)              # a corda
    p.box('CnMetal', x + 0.1, x + 0.2, y - 0.04, y + 0.04, G + 1.2, G + 1.35, bevel=0.0)


def dormitory(p):
    """O dormitório comprido no fundo: o rodapé, as paredes, a varanda com as colunas vermelhas e o piso elevado com os
    degraus, a fila de janelas, a porta dupla com a placa da estrela, os 2 estandartes pendurados, o telhado de pagode com a
    viga pintada, as chaminés de fogão; os fundos com as janelas e a porta."""
    # (31/10, o detalhe AAA) o dormitório OCO de placas (as paredes de 0,35 m, o talude no pé, o interior), as janelas e as portas
    # nos vãos de verdade (o caixilho recuado na espessura, o peitoril)
    fwin = [(y - 0.5, y + 0.5, G + 1.2, G + 2.45) for y in (-6.9, -4.65, -2.35, 2.35, 4.65, 6.9)]
    bwin = [(y - 0.45, y + 0.45, G + 1.6, G + 2.5) for y in (-6.2, -3.1, 3.1, 6.2)]
    swin = [(DX0 + 1.6, DX0 + 2.6, G + 1.4, G + 2.5)]
    cn_block(p, DX0, DX1, -DY, DY, G, DZ, t=0.35, skin=('+x', '-x', '+y', '-y'),
             holes={'+x': fwin + [(-0.75, 0.75, G + 0.35, G + 2.6)], '-x': bwin + [(-0.55, 0.55, G, G + 2.2)], '+y': swin, '-y': swin},
             roof_t=0.25, parapet=0.0, cornice=False, corners=True, pier=0.55, interior=True, plinth=0.55, pw=2.3, zs=[G, G + 1.1, DZ - 0.25])
    for f, at, ws in (('+x', DX1, fwin), ('-x', DX0, bwin), ('+y', DY, swin), ('-y', -DY, swin)):
        F = WallFace(f, at, 0.0)
        for (a, b, c, d) in ws:
            cn_window(p, F, a, b, c, d, t=0.35, mull=1)
    # a varanda: o piso elevado, os degraus no meio, as colunas vermelhas nas bases
    p.box('CnPlinth', DX1 - 0.1, VER_X, -DY, DY, G, G + 0.35, bevel=0.03)
    for k in range(2):
        p.box('CnPlinth', VER_X, VER_X + 0.35 * (2 - k), -1.4, 1.4, G, G + 0.12 * (k + 1), bevel=0.02)
    cols = (-8.0, -5.8, -3.5, -1.2, 1.2, 3.5, 5.8, 8.0)
    for y in cols:
        p.box('CnPlinth', VER_X - 0.62, VER_X - 0.12, y - 0.25, y + 0.25, G + 0.35, G + 0.55, bevel=0.02)
        p.cyl('Player', (VER_X - 0.37, y, (G + 0.55 + DZ) / 2), 'z', 0.18, DZ - G - 0.55, seg=16, bevel=0.0, smooth=True)
    # a viga pintada (frente da varanda e em volta das paredes) e o telhado
    rcx, rax = (DX0 + VER_X) / 2, (VER_X - DX0) / 2
    cn_frieze(p, rcx, 0.0, rax, DY, DZ + 0.32, h=0.34, step=0.8)
    p.box('CnConcrete', DX0, VER_X, -DY, DY, DZ, DZ + 0.32, bevel=0.0)
    top, _, _ = cn_roof(p, rcx, 0.0, rax, DY, DZ + 0.32, 1.85, over=0.62, curl=0.5, sweep=0.25, rib_step=0.42, tip=0.85)
    # a fila de janelas da frente (entre as colunas) e a porta dupla com a placa
    p.box('Dark', DX1 - 0.22, DX1 - 0.18, -0.75, 0.75, G + 0.35, G + 2.6, bevel=0.0)                        # a porta dupla, no vão
    p.box('CnMetal', DX1 - 0.18, DX1 - 0.12, -0.03, 0.03, G + 0.35, G + 2.6, bevel=0.0)
    for (a, b) in ((-0.95, -0.75), (0.75, 0.95)):
        p.box('Player#door', DX1 - 0.05, DX1 + 0.2, a, b, G + 0.35, G + 2.8, bevel=0.03)
    p.box('Player#door', DX1 - 0.05, DX1 + 0.2, -0.95, 0.95, G + 2.6, G + 2.8, bevel=0.03)
    p.box('CnGold', DX1 + 0.12, DX1 + 0.18, -0.75, 0.75, G + 2.85, G + 3.45, bevel=0.0)                      # a placa da porta
    p.box('CnRoof', DX1 + 0.18, DX1 + 0.2, -0.68, 0.68, G + 2.9, G + 3.4, bevel=0.0)
    star_wall(p, (DX1 + 0.2, 0.0, G + 3.15), '+x', 0.2, t=0.015)
    lamp(p, DX1, -1.2, G + 2.5, face='+x')
    lamp(p, DX1, 1.2, G + 2.5, face='+x')
    # os 2 estandartes pendurados na viga da varanda (entre as colunas do meio e as do lado)
    for y in (-2.35, 2.35):
        wall_banner(p, VER_X - 0.47, y, DZ + 0.05, '+x', w=0.95, h=2.3, notch=0.3)
    # os fundos (−X): as janelas altas e a porta; os lados: 1 janela e a luz
    p.box('Dark', DX0 + 0.18, DX0 + 0.22, -0.55, 0.55, G, G + 2.2, bevel=0.0)
    for s in (-1, 1):
        lamp(p, DX0 + 3.3, s * (DY + 0.12), G + 2.6, face='+y' if s > 0 else '-y')
    # as chaminés de fogão (atravessam a água de trás do telhado)
    for y in (-5.0, 5.0):
        p.box('CnConcrete', -5.95, -5.45, y - 0.25, y + 0.25, DZ, top + 0.25, bevel=0.03)
        p.box('CnConcrete', -6.05, -5.35, y - 0.35, y + 0.35, top + 0.25, top + 0.4, bevel=0.02)
        p.box('Dark', -5.85, -5.55, y - 0.15, y + 0.15, top + 0.39, top + 0.42, bevel=0.0)
    return top


def obstacle_course(p):
    """A pista de obstáculos no lado +Y do pátio: as 3 muretas de concreto, a escada horizontal (as 4 colunas, as 2 longarinas
    e os degraus) e o muro de escalar de madeira com as escoras."""
    for xx in (-0.2, 1.0, 2.2):
        p.box('CnConcrete', xx - 0.15, xx + 0.15, 5.6, 7.9, G, G + 0.9, bevel=0.03)
        p.box('CnPlinth', xx - 0.18, xx + 0.18, 5.57, 7.93, G + 0.9, G + 0.98, bevel=0.0)
    lx0, lx1, ly0, ly1, lz = 3.4, 5.9, 3.4, 4.3, G + 2.3
    for (xx, yy) in ((lx0, ly0), (lx0, ly1), (lx1, ly0), (lx1, ly1)):
        p.cyl('CnMetal', (xx, yy, (G + lz) / 2), 'z', 0.07, lz - G, seg=10, bevel=0.0)
        p.box('CnConcrete', xx - 0.2, xx + 0.2, yy - 0.2, yy + 0.2, G, G + 0.12, bevel=0.0)
    for yy in (ly0, ly1):
        p.cyl('CnMetal', ((lx0 + lx1) / 2, yy, lz), 'x', 0.05, lx1 - lx0 + 0.1, seg=8, bevel=0.0)
    n = int((lx1 - lx0) / 0.38)
    for k in range(n + 1):
        xx = lx0 + k * (lx1 - lx0) / n
        p.cyl('Trim', (xx, (ly0 + ly1) / 2, lz), 'y', 0.025, ly1 - ly0, seg=6, bevel=0.0)
    for xx in (lx0, lx1):
        p.cyl('CnMetal', (xx, (ly0 + ly1) / 2, lz - 0.1), 'y', 0.04, ly1 - ly0, seg=6, bevel=0.0)
    # o muro de escalar (madeira com as tábuas e a moldura), com as 2 escoras atrás
    wx, wy0, wy1, wh = 4.6, 5.8, 8.2, 2.2
    p.box('Wood', wx - 0.1, wx + 0.1, wy0, wy1, G, G + wh, bevel=0.02)
    for k in range(1, 6):
        zz = G + k * wh / 6
        p.box('Dark', wx + 0.1, wx + 0.12, wy0, wy1, zz - 0.015, zz + 0.015, bevel=0.0)
    for yy in (wy0, wy1):
        p.box('CnMetal', wx - 0.12, wx + 0.12, yy - 0.08, yy + 0.08, G, G + wh + 0.1, bevel=0.0)
        a = Vector((wx - 0.1, yy, G + wh - 0.2))
        b = Vector((wx - 1.2, yy, G))
        d = b - a
        p.cyl('Wood', tuple((a + b) / 2), tuple(d), 0.06, d.length, seg=8, bevel=0.0)
    p.box('CnPlinth', 0.2, 2.8, 3.3, 4.4, G, G + 0.02, bevel=0.0)                    # a caixa de areia do salto
    p.box('Dark', 0.18, 0.22, 3.3, 4.4, G, G + 0.03, bevel=0.0)


def dummy(p, x, y):
    """O boneco de palha de treino: o poste e a travessa (os braços) de madeira, o corpo e a cabeça de palha amarrados, o alvo
    pintado no peito (virado pra +X)."""
    p.box('CnConcrete', x - 0.18, x + 0.18, y - 0.18, y + 0.18, G, G + 0.1, bevel=0.0)
    p.cyl('Wood', (x, y, G + 0.95), 'z', 0.055, 1.7, seg=8, bevel=0.0)
    p.cyl('Wood', (x, y, G + 1.48), 'y', 0.045, 0.95, seg=8, bevel=0.0)
    z = G
    revolve(p, 'Cloth', (x, y), [(0.05, z + 0.95), (0.2, z + 1.0), (0.24, z + 1.2), (0.23, z + 1.42), (0.16, z + 1.58), (0.05, z + 1.62)], seg=14, smooth=True)
    revolve(p, 'Cloth', (x, y), [(0.03, z + 1.62), (0.12, z + 1.67), (0.15, z + 1.77), (0.12, z + 1.87), (0.03, z + 1.92)], seg=12, smooth=True)
    for zz in (z + 1.06, z + 1.5):
        p.cyl('Dark', (x, y, zz), 'z', 0.235, 0.04, seg=14, bevel=0.0)
    for s in (-1, 1):                                                                 # as pontas de palha dos braços
        p.cyl('Cloth', (x, y + s * 0.5, z + 1.48), 'y', 0.07, 0.22, seg=8, r2=0.1, bevel=0.0)
    p.cyl('Paint', (x + 0.225, y, z + 1.27), 'x', 0.13, 0.02, seg=16, bevel=0.0)
    p.cyl('Player', (x + 0.235, y, z + 1.27), 'x', 0.08, 0.02, seg=16, bevel=0.0)
    p.cyl('Dark', (x + 0.245, y, z + 1.27), 'x', 0.03, 0.02, seg=10, bevel=0.0)


def training(p):
    """O lado −Y do pátio: os 4 bonecos de palha, o cavalete de fuzis e o poste do alto-falante no canto da frente."""
    for xx in (0.4, 1.8, 3.2, 4.6):
        dummy(p, xx, -6.6)
    p.box('CnPlinth', -0.2, 5.2, -7.3, -5.9, G, G + 0.02, bevel=0.0)
    # o cavalete de fuzis (encostado no muro, perto do dormitório)
    rx, ry = -0.6, -8.6
    for yy in (ry - 0.6, ry + 0.6):
        p.box('Wood', rx - 0.05, rx + 0.05, yy - 0.05, yy + 0.05, G, G + 1.1, bevel=0.0)
    p.box('Wood', rx - 0.08, rx + 0.08, ry - 0.65, ry + 0.65, G + 1.0, G + 1.1, bevel=0.0)
    p.box('Wood', rx - 0.15, rx + 0.15, ry - 0.65, ry + 0.65, G + 0.15, G + 0.22, bevel=0.0)
    for k in range(5):
        yy = ry - 0.48 + k * 0.24
        p.obox('CnMetal', (rx + 0.05, yy, G + 0.65), (0.06, 0.05, 1.1), pitch=8, bevel=0.0)
        p.obox('Wood', (rx + 0.1, yy, G + 0.3), (0.07, 0.05, 0.3), pitch=8, bevel=0.0)
    # o poste do alto-falante (canto da frente, −Y), 4 cornetas e a caixa
    px, py = 5.7, -8.5
    p.box('CnConcrete', px - 0.35, px + 0.35, py - 0.35, py + 0.35, G, G + 0.35, bevel=0.03)
    p.cyl('CnMetal', (px, py, (G + 0.35 + 6.5) / 2), 'z', 0.1, 6.5 - G - 0.35, seg=12, bevel=0.0)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        horn(p, (px + math.cos(a) * 0.14, py + math.sin(a) * 0.14, 6.15), (math.cos(a), math.sin(a), -0.3), r1=0.32, L=0.72)
    p.cyl('CnMetal', (px, py, 6.55), 'z', 0.18, 0.1, seg=12, bevel=0.0)
    p.box('CnMetal', px - 0.2, px + 0.2, py + 0.1, py + 0.3, 2.0, 2.5, bevel=0.0)
    p.box('Player', px - 0.24, px + 0.24, py - 0.13, py - 0.1, 4.6, 5.2, bevel=0.0)
    star_wall(p, (px, py - 0.13, 4.9), '-y', 0.2, m='CnGold', t=0.008)


def base():
    p = BPart(PREFIX + '_Base')
    p.bm('CnRoof', 0.0)                         # a telha sem chanfro (as costelas e a casca: o chanfro só multiplicaria triângulo)
    foundation(p, HX, HY, m='CnPlinth', z=G)
    paifang(p)
    walls(p)
    flagpole(p)
    top = dormitory(p)
    obstacle_course(p)
    training(p)
    print('MODEL dormitorio: cumeeira a', round(top, 2), 'm')
    return p.finish()


def main():
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, DZ + 0.6, post='CnMetal'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 20 m; portao: vao do meio Y', -GATE_MAIN + 0.26, '..', GATE_MAIN - 0.26, 'em X', FRONT_X,
          '; os soldados saem em (', FRONT_X + 1.5, ', 0 )')
    print('MODEL fire_points_m', [(-4.4, -5.0, 5.0), (-4.4, 4.5, 5.0), (FRONT_X, 0.0, 6.3)])


if __name__ == '__main__':
    main()
