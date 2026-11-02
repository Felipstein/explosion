"""O tanque básico da China (Type 99, o "Battlemaster") provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_type99.py -- captures/game/models/china_type99

A planta é a ficha aprovada docs/concept/units/china_mbt_sheet.webp com a cor do jogador de china_mbt_player_color.webp (5 vistas;
fictício inspirado no ZTZ-99, não cópia). A palavra: a TORRE-ESCUDO. De cima: a torre comprida e pontuda na frente (o hexágono
alongado: o nariz estreito do mantelete, as bochechas abrindo em diagonal, os lados retos até a traseira chanfrada), a gaiola de
carga em U abraçando a traseira da torre, um canhão só.
Medidas (a ficha medida em pixels; a régua é o tanque USA, casco 8,1 × 3,8 m, torre a 2,48 m):
  - casco 7,8 × 3,74 m (com as saias), barriga a 0,46 m, deck a 1,60 m, glacis raso de 28°, teto da torre a 2,48 m;
  - 6 rodas duplas de 0,74 m por lado (sem rolete aparente: as saias cobrem a metade de cima), roda-guia na frente, motriz atrás,
    3 roletes escondidos, esteira de 0,62 m; as saias de chapas com os recortes em cima de cada roda (a ficha);
  - a torre em (-0,15; 0): 4,1 m de comprimento × 3,04 m de largura, o mantelete na frente, o canhão a 1,96 m com a boca em
    x 6,05 (2,15 m além do nariz do casco, como a ficha);
  - na torre: a cúpula do comandante com a metralhadora de 12,7 mm (esquerda), a mira do atirador em caixa (direita, na frente),
    a escotilha do municiador, a mira panorâmica, os lançadores de fumaça nos cantos da frente, as caixas dos lados, o mastro do
    sensor de vento e a antena atrás; a gaiola de barras em U com o tambor e a caixa dentro.
Cor do jogador (slot Team; a bíblia: lonas pequenas dobradas e presas rente + a faixa no cano, ~4% de cima): a lona DOBRADA em cima
da gaiola, atrás da torre, caindo rente pela grade de trás (a ficha: centro-direita), o ROLO amarrado atrás do motor (a ficha), a
faixa no cano e as estrelas nas bochechas da torre (a estrela troca com a cor do jogador). Nada de faixa pintada.
Camuflagem digital: os blocos CnTankCamo em todas as faces de fora do casco, das saias e da torre (model_china_vehicle_kit.py).

Peças (origem no chão, no centro do casco, frente em +X; todas nas coordenadas do veículo, no lugar delas):
  SM_China_Type99_Hull      casco, saias, deck, grelhas, faróis, o anel da torre, o rolo de lona atrás;
  SM_China_Type99_Turret    a torre (gira em Z em volta de TURRET_PIVOT), a gaiola, a lona dobrada, as miras, a metralhadora;
  SM_China_Type99_Barrel    o canhão (recua em −X a partir daqui); a boca é o X máximo;
  SM_China_Type99_Wheels    rodas, roda-guia, motriz e roletes (uma malha: como o tanque do GLA e o Linebacker, a esteira não
                            usa as malhas wheel_*; o jogo lê a bitola de tracks_m);
  SM_China_Type99_Track_l / _Track_r  as esteiras.
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Track, Rubber, Team, Lens, Dark, Light.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, bounds, camo_faces, camo_poly, find_face, frame_box, grille, hatch, headlight, hmg,  # noqa: E402
                                     idler, on_face, periscope, plane_point, road_wheel, roller, smoke_launchers, solid,
                                     sprocket, star, sweep_check, tarp_fold, tarp_roll, tow_hook, track, tris, write_preview,
                                     basis)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_type99')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_Type99'

# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 3.9, -3.9           # o casco: 7,8 m
BELLY, DECK = 0.46, 1.60
LOW_HW = 1.06                    # o casco de baixo, entre as esteiras
UP_HW = 1.80                     # o casco de cima (sobre as esteiras)
SKIRT = (1.80, 1.87)             # a saia (face de dentro e de fora)
SKIRT_Z = (0.64, 1.52)
SPON_Z = 1.10                    # a barriga do casco de cima (por cima da esteira)
TRACK_Y, TRACK_W = 1.44, 0.62
WHEEL_R = 0.37
WHEEL_Z = WHEEL_R + 0.07
WHEELS_X = [2.53, 1.51, 0.49, -0.53, -1.55, -2.57]
IDLER = (3.42, 0.62, 0.30)
SPROCKET = (-3.44, 0.66, 0.33)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.95, 0.0, -1.95], 0.92, 0.09
GLACIS = ((NOSE, 1.16), (3.08, DECK))   # o glacis: do nariz ao deck
TX = -0.15                       # o eixo da torre
T_Z0, T_Z1, T_Z2 = 1.64, 2.20, 2.48   # a base da torre, a quina das bochechas, o teto
GUN_Z = 1.96
MANTLET_X = 2.22                 # a frente do mantelete (o cano sai daqui)
MUZZLE_X = 6.05


def glacis_z(x):
    (x0, z0), (x1, z1) = GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('CnTankPaint#shell', 0.05)
    p.bm('CnTankPaint', 0.025)
    p.bm('CnTankPaint#thin', 0.008)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.006)
    # o casco de baixo (entre as esteiras): a chapa de baixo da frente, a barriga, a traseira
    low = [(3.45, BELLY), (NOSE, 1.0), (NOSE, SPON_Z + 0.02), (REAR, SPON_Z + 0.02), (REAR, 0.76), (-3.62, BELLY)]
    f_low = solid(p, 'CnTankPaint#shell', [(x, s * LOW_HW, z) for x, z in low for s in (-1, 1)])
    # o casco de cima, largura toda: o glacis raso, o deck, a quina de trás
    up = [(NOSE, SPON_Z), (NOSE, GLACIS[0][1]), (GLACIS[1][0], DECK), (-3.84, DECK), (REAR, 1.54), (REAR, SPON_Z)]
    f_up = solid(p, 'CnTankPaint#shell', [(x, s * UP_HW, z) for x, z in up for s in (-1, 1)])
    camo_faces(p, f_low, seed=1, skip=lambda c, n: abs(n.y) > 0.9 or c.z > SPON_Z)
    camo_faces(p, f_up, seed=1, skip=lambda c, n: abs(n.y) > 0.9 and c.z < SKIRT_Z[1])
    # ---------------------------------------------------------------- as saias: 6 chapas grossas com a junta escura, os recortes
    # em cima de cada roda (a ficha) e os parafusos; a da frente com o canto chanfrado
    cuts = [3.88, 2.98, 1.97, 0.97, -0.03, -1.03, -2.06, -3.86]
    for s in (-1, 1):
        y0, y1 = (SKIRT[0], SKIRT[1]) if s > 0 else (-SKIRT[1], -SKIRT[0])
        for i, (xa, xb) in enumerate(zip(cuts[:-1], cuts[1:])):
            xa, xb = xa - 0.02, xb + 0.02
            if i == 0:
                prof = [(xa, SKIRT_Z[1]), (xa, 1.16), (xa - 0.38, SKIRT_Z[0]), (xb, SKIRT_Z[0]), (xb, SKIRT_Z[1])]
            elif i == len(cuts) - 2:
                prof = [(xa, SKIRT_Z[1]), (xa, SKIRT_Z[0]), (xb + 0.22, SKIRT_Z[0]), (xb, 0.98), (xb, SKIRT_Z[1])]
            else:
                prof = [(xa, SKIRT_Z[1]), (xa, SKIRT_Z[0]), (xb, SKIRT_Z[0]), (xb, SKIRT_Z[1])]
            fs = solid(p, 'CnTankPaint', [(x, y, z) for x, z in prof for y in (y0, y1)], bevel=0.02)
            camo_faces(p, fs, seed=1, skip=lambda c, n: abs(n.y) < 0.9 or n.y * s < 0, inset=0.04)
            # (31/10) a fileira de parafusos de cima (o ritmo que lê de longe; o parafuso solto não lê)
            nb = max(3, int(round((xa - xb) / 0.19)))
            vp.bolt_row(p, 'CnTankMetal', (xa - 0.1, SKIRT[1] * s, SKIRT_Z[1] - 0.08), (xb + 0.1, SKIRT[1] * s, SKIRT_Z[1] - 0.08), nb,
                        (0, s, 0), r=0.022, h=0.016)
        # a junta escura entre as chapas (o vão mostra o fundo)
        for xc in cuts[1:-1]:
            yy0, yy1 = sorted((UP_HW * s, (SKIRT[0] + 0.01) * s))
            p.box('Dark', xc - 0.03, xc + 0.03, yy0, yy1, SKIRT_Z[0] + 0.02, SKIRT_Z[1] - 0.02, bevel=0.0)
        # os recortes em cima de cada roda (a ficha: o retângulo escuro na borda de baixo da saia)
        for x in WHEELS_X:
            yy0, yy1 = sorted((SKIRT[1] * s, (SKIRT[1] + 0.006) * s))
            p.box('Dark', x - 0.09, x + 0.09, yy0, yy1, SKIRT_Z[0] + 0.03, SKIRT_Z[0] + 0.16, bevel=0.0)
        # a beirada do para-lama (a aba em cima da saia, a linha que corre o carro)
        yy0, yy1 = sorted((UP_HW * s, (SKIRT[1] + 0.015) * s))
        p.box('CnTankPaint#thin', -3.86, 3.0, yy0, yy1, SKIRT_Z[1], SKIRT_Z[1] + 0.035)
        # o casco de baixo atrás das rodas (os braços da suspensão e o forro escuro por dentro das esteiras)
        yy0, yy1 = sorted((LOW_HW * s, (LOW_HW + 0.02) * s))
        p.box('Dark', -3.3, 3.2, yy0, yy1, 0.5, SPON_Z - 0.02, bevel=0.0)
        for x in WHEELS_X:
            yy0, yy1 = sorted((LOW_HW * s, (TRACK_Y - TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.09, x + 0.3, yy0, yy1, WHEEL_Z - 0.06, WHEEL_Z + 0.06, bevel=0.0)
    # ---------------------------------------------------------------- o deck: o anel da torre, o motorista, as tampas
    p.cyl('Dark', (TX, 0.0, DECK + 0.015), 'z', 1.08, 0.03, seg=48, bevel=0.0)
    p.cyl('CnTankMetal', (TX, 0.0, DECK + 0.005), 'z', 1.14, 0.012, seg=48, bevel=0.0)
    hatch(p, 2.72, 0.0, DECK, r=0.3, hinge_side=1)                              # o motorista, no meio (ZTZ-99)
    for k, yaw in enumerate((-30, 0, 30)):                                      # os 3 periscópios na frente da escotilha
        a = math.radians(yaw)
        x = 3.06 + 0.02 * abs(k - 1)
        periscope(p, x - 0.06, (k - 1) * 0.26, glacis_z(x) - 0.04, yaw=yaw)
    # as linhas das tampas do deck (juntas finas) e as argolas de amarração
    for (x0, x1, y0, y1) in ((1.25, 2.55, -1.72, -1.14), (1.25, 2.55, 1.14, 1.72), (-2.4, -1.25, -1.72, -1.2), (-2.4, -1.25, 1.2, 1.72)):
        p.box('CnTankPaint#thin', x0, x1, y0, y1, DECK - 0.01, DECK + 0.03)                  # (31/10) a chapa com espessura
        p.box('Dark', x0 - 0.025, x1 + 0.025, y0 - 0.025, y1 + 0.025, DECK, DECK + 0.004, bevel=0.0)  # a fresta em volta
    for (x, y) in ((2.95, 1.62), (2.95, -1.62), (-3.6, 1.62), (-3.6, -1.62)):            # (fora do giro da torre)
        p.torus('CnTankMetal', (x, y, DECK + 0.03), 'y', 0.05, 0.014, seg=12, rseg=6)
    # ---------------------------------------------------------------- o motor atrás: 2 grelhas grandes e a tampa do meio, o escape
    grille(p, -3.72, -2.62, -1.5, -0.25, DECK, n=9)
    grille(p, -3.72, -2.62, 0.25, 1.5, DECK, n=9)
    p.box('CnTankPaint#thin', -3.72, -2.62, -0.2, 0.2, DECK, DECK + 0.05)
    p.box('CnTankMetal', -3.3, -3.0, -0.08, 0.08, DECK + 0.05, DECK + 0.08, bevel=0.0)
    # o escape do lado esquerdo, atrás (a grelha na lateral do casco de cima, por cima da saia)
    p.box('Dark', -3.55, -2.65, -UP_HW - 0.012, -UP_HW, SKIRT_Z[1] + 0.045, DECK - 0.03, bevel=0.0)
    for k in range(7):
        p.obox('CnTankMetal', (-3.48 + k * 0.13, -UP_HW - 0.02, (SKIRT_Z[1] + DECK) / 2 + 0.01), (0.03, 0.03, 0.05), bevel=0.0)
    # ---------------------------------------------------------------- a frente: faróis com guarda, ganchos, a chapa do glacis
    for s in (-1, 1):
        x = 3.62
        z = glacis_z(x)
        p.obox('CnTankPaint', (x, 1.38 * s, z + 0.1), (0.42, 0.44, 0.18), pitch=-28, bevel=0.015)    # o bloco do farol
        headlight(p, (x + 0.15, 1.48 * s, z + 0.12), r=0.075)
        headlight(p, (x + 0.15, 1.28 * s, z + 0.12), r=0.06, guard=False)
        tow_hook(p, NOSE - 0.02, 0.62 * s, 0.95, s=1)
        tow_hook(p, REAR + 0.02, 0.7 * s, 0.98, s=-1)
    p.obox('CnTankPaint#thin', (3.42, 0.0, glacis_z(3.42) + 0.03), (0.08, 2.2, 0.06), pitch=-28, bevel=0.0)  # a régua de respingo
    # ---------------------------------------------------------------- a traseira: a grelha grande, as lanternas, os para-lamas
    p.box('Dark', REAR - 0.02, REAR, -0.8, 0.8, 0.9, 1.46, bevel=0.0)
    for k in range(9):
        p.obox('CnTankMetal', (REAR - 0.03, 0.0, 0.94 + k * 0.058), (0.03, 1.55, 0.025), pitch=-30, bevel=0.0)
    p.box('CnTankPaint#thin', REAR - 0.04, REAR, -0.04, 0.04, 0.9, 1.46)
    for s in (-1, 1):
        p.box('Dark', REAR - 0.06, REAR, 1.42 * s - 0.1, 1.42 * s + 0.1, 1.3, 1.44, bevel=0.0)
        p.box('Lens', REAR - 0.07, REAR - 0.05, 1.42 * s - 0.07, 1.42 * s + 0.07, 1.33, 1.41, bevel=0.0)
        yy0, yy1 = sorted((1.1 * s, 1.86 * s))
        p.obox('CnTankPaint', (REAR - 0.02, (1.1 + 1.86) / 2 * s, 0.9), (0.04, 0.74, 0.4), pitch=-20, bevel=0.01)   # o para-lama de trás
    # o rolo de lona atrás do motor (a ficha: o rolo vermelho amarrado na traseira), nos 2 suportes; baixo o bastante pro cano
    # passar por cima com a torre virada pra trás
    for s in (-1, 1):
        p.obox('CnTankMetal', (REAR - 0.12, 0.42 * s, 1.43), (0.3, 0.06, 0.06), bevel=0.0)
        p.obox('CnTankMetal', (REAR - 0.24, 0.42 * s, 1.36), (0.06, 0.06, 0.2), bevel=0.0)
    tarp_roll(p, (REAR - 0.27, 0.0, 1.5), 'y', 1.2, 0.24, m='Team', straps=2)
    # (31/10, o detalhe AAA) os 2 galões em pé na chapa de trás, presos pela cinta, e o cabo de reboque em U por baixo da grelha
    for s in (-1, 1):
        vp.jerrycan(p, (REAR - 0.095, 1.1 * s, 1.165), yaw=90, m='CnTankCamo', cap_m='CnTankMetal')
        p.box('CnTankMetal', REAR - 0.2, REAR - 0.17, 1.1 * s - 0.2, 1.1 * s + 0.2, 1.05, 1.09, bevel=0.0)
        p.box('CnTankMetal', REAR - 0.04, REAR, 1.1 * s - 0.2, 1.1 * s + 0.2, 0.9, 0.95, bevel=0.0)       # a prateleira
    vp.tow_cable(p, 'CnTankMetal', [(REAR - 0.05, -0.7, 0.93), (REAR - 0.05, -0.56, 0.83), (REAR - 0.05, 0.56, 0.83),
                                     (REAR - 0.05, 0.7, 0.93)], r=0.02, eye_m='CnTankMetal')
    # as caixas de ferramenta nos para-lamas da frente (baixas: o cano passa por cima com a torre girando)
    for s in (-1, 1):
        vp.rbox(p, 'CnTankPaint', (2.82, 1.6 * s, DECK + 0.07), (0.5, 0.3, 0.14), r=0.03, k=2)
        vp.rbox(p, 'CnTankPaint', (2.82, 1.6 * s, DECK + 0.147), (0.53, 0.33, 0.025), r=0.035, k=2)          # a tampa
        for d in (-0.14, 0.14):
            vp.rbox(p, 'CnTankMetal', (2.82 + d, 1.6 * s + 0.16 * s, DECK + 0.1), (0.05, 0.02, 0.06), r=0.008, k=1)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_points():
    """Os cantos da torre-escudo (o hexágono alongado de cima): o nariz, a bochecha, o lado, a quina de trás; 3 alturas."""
    pts = []
    for s in (-1, 1):
        pts += [(1.92, 0.44 * s, T_Z0), (0.76, 1.52 * s, T_Z0), (-1.98, 1.48 * s, T_Z0), (-2.18, 1.2 * s, T_Z0)]
        pts += [(1.81, 0.41 * s, T_Z1), (0.69, 1.46 * s, T_Z1), (-1.98, 1.42 * s, T_Z1), (-2.18, 1.14 * s, T_Z1)]
        pts += [(1.20, 0.36 * s, T_Z2), (0.40, 1.30 * s, T_Z2), (-1.95, 1.32 * s, T_Z2), (-2.12, 1.06 * s, T_Z2)]
    return pts


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('CnTankPaint#shell', 0.075)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.0)
    faces = solid(p, 'CnTankPaint#shell', turret_points())
    p.cyl('Dark', (TX, 0.0, 1.648), 'z', 1.04, 0.026, seg=48, bevel=0.0)                            # a saia do anel
    # as bochechas: a estrela no meio de cada uma, com a camuflagem contornando (o furo debaixo da estrela)
    stars = []
    for s in (-1, 1):
        q, n = find_face(faces, (0.72, 0.68 * s, 0.1), near=(1.4, 1.0 * s, 1.9))
        stars.append((q, n, sum(q, Vector()) / len(q)))
    star_faces = [sc for _, _, sc in stars]
    camo_faces(p, faces, seed=2, skip=lambda c, n: any((c - sc).length < 0.02 for sc in star_faces))
    for q, n, c in stars:
        ex, ey, _ = basis(n)
        r = 0.2
        camo_poly(p, q, n, seed=2, holes=[(c.dot(ex) - r, c.dot(ex) + r, c.dot(ey) - r, c.dot(ey) + r)])
        star(p, c, n, r)
    # o mantelete: o bloco do canhão saindo do nariz, com o colar
    mf = solid(p, 'CnTankPaint', [(1.70, s * 0.44, z) for s in (-1, 1) for z in (1.70, 2.20)] +
               [(MANTLET_X, s * 0.34, z) for s in (-1, 1) for z in (1.77, 2.13)], bevel=0.02)
    camo_faces(p, mf, seed=2, skip=lambda c, n: n.x > 0.9)
    p.cyl('Dark', (MANTLET_X + 0.01, 0.0, GUN_Z), 'x', 0.2, 0.02, seg=24, bevel=0.0)
    # (31/10) o fole de lona do mantelete (a capa entre o cano e a torre: as dobras em anel) e as fileiras de parafusos da chapa
    vp.bellows(p, 'CnTankCamo', MANTLET_X - 0.01, MANTLET_X + 0.25, 0.205, 0.138, GUN_Z, folds=4, depth=0.02, seg=28)
    vp.bolt_row(p, 'CnTankMetal', (MANTLET_X, -0.27, 1.815), (MANTLET_X, 0.27, 1.815), 5, (1, 0, 0), r=0.02, h=0.016)
    vp.bolt_row(p, 'CnTankMetal', (MANTLET_X, -0.27, 2.095), (MANTLET_X, 0.05, 2.095), 3, (1, 0, 0), r=0.02, h=0.016)
    vp.rbox(p, 'CnTankMetal', (MANTLET_X - 0.01, 0.24, 2.08), (0.1, 0.14, 0.1), r=0.02, k=2)                   # a mira coaxial
    vp.lathe(p, 'Lens', (MANTLET_X + 0.04, 0.24, 2.08), 'x', [(0.0, 0.006), (0.032, 0.004), (0.036, -0.01), (0.0, -0.01)], seg=12)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    # a cúpula do comandante (esquerda) com a metralhadora de 12,7 mm (a ficha: o pino alto no meio-esquerda)
    cx, cy = -0.62, -0.78
    vp.lathe(p, 'CnTankPaint', (cx, cy, top), 'z', [(0.0, 0.0), (0.4, 0.0), (0.4, 0.03), (0.385, 0.045), (0.37, 0.16), (0.355, 0.18),
                                                   (0.3, 0.19), (0.29, 0.215), (0.24, 0.245), (0.12, 0.258), (0.0, 0.262)], seg=32)
    for k in range(6):
        a = math.radians(k * 50 - 125)
        ca, sa = math.cos(a), math.sin(a)
        p.obox('Dark', (cx + 0.365 * ca, cy + 0.365 * sa, top + 0.105), (0.04, 0.15, 0.065), yaw=k * 50 - 125, bevel=0.0)
        vp.rbox(p, 'CnTankPaint', (cx + 0.375 * ca, cy + 0.375 * sa, top + 0.155), (0.08, 0.2, 0.025), yaw=k * 50 - 125, r=0.02, k=2)
    p.box('CnTankMetal', cx + 0.2, cx + 0.32, cy - 0.06, cy + 0.06, top + 0.18, top + 0.26, bevel=0.0)
    hmg(p, (cx + 0.18, cy + 0.0, top + 0.24), yaw=0.0, scale=1.0)
    # a mira do atirador em caixa (direita, na frente), com a janela e a tampa
    gx, gy = 0.32, 0.72
    p.box('CnTankPaint', gx - 0.3, gx + 0.26, gy - 0.25, gy + 0.25, top, top + 0.36, bevel=0.02)
    p.box('Dark', gx + 0.26, gx + 0.28, gy - 0.19, gy + 0.19, top + 0.1, top + 0.3, bevel=0.0)
    p.box('Lens', gx + 0.275, gx + 0.29, gy - 0.16, gy + 0.16, top + 0.13, top + 0.27, bevel=0.0)
    p.box('CnTankPaint#thin', gx - 0.05, gx + 0.34, gy - 0.27, gy + 0.27, top + 0.36, top + 0.39)
    for sgn in (-1, 1):                                                          # as portinholas blindadas abertas de lado
        p.obox('CnTankPaint#thin', (gx + 0.3, gy + sgn * 0.27, top + 0.2), (0.16, 0.025, 0.2), yaw=sgn * 30, bevel=0.0)
    # a mira panorâmica do comandante (o tambor no pé)
    p.cyl('CnTankMetal', (-0.05, -0.32, top + 0.12), 'z', 0.06, 0.24, seg=12, bevel=0.0)
    p.cyl('CnTankPaint', (-0.05, -0.32, top + 0.32), 'z', 0.15, 0.2, seg=20, bevel=0.01)
    p.box('Lens', 0.08, 0.1, -0.4, -0.24, top + 0.27, top + 0.37, bevel=0.0)
    # (31/10) as alças de mão do teto (a fileira de U na frente do teto)
    for yy in (-0.32, 0.22):
        vp.grab_handle(p, 'CnTankMetal', (1.06, yy - 0.12, top), (1.06, yy + 0.12, top), (0, 0, 1), height=0.07, r=0.016)
    # a escotilha do municiador e os periscópios, o respiro
    hatch(p, -0.5, 0.74, top, r=0.3, hinge_side=1)
    periscope(p, 0.0, 0.62, top, yaw=20)
    periscope(p, 0.62, -0.2, top, yaw=0)
    p.cyl('CnTankPaint', (-1.35, 0.1, top + 0.04), 'z', 0.18, 0.08, seg=20, bevel=0.01)
    p.cyl('Dark', (-1.35, 0.1, top + 0.09), 'z', 0.12, 0.02, seg=16, bevel=0.0)
    # o mastro do sensor de vento (o tubo alto atrás, à direita) e a antena (atrás, à esquerda)
    p.cyl('CnTankMetal', (-1.86, 0.92, top + 0.3), 'z', 0.045, 0.6, seg=12, bevel=0.0)
    p.cyl('CnTankPaint', (-1.86, 0.92, top + 0.62), 'z', 0.07, 0.08, seg=12, bevel=0.005)
    p.cyl('Dark', (-1.86, 0.92, top + 0.7), 'z', 0.03, 0.1, seg=8, bevel=0.0)
    antenna(p, -1.86, -0.96, top, 1.7)
    # os lançadores de fumaça nos cantos da frente (a ficha: os blocos nas quinas da frente da torre)
    for s in (-1, 1):
        smoke_launchers(p, (0.5, 1.12 * s, top + 0.13), yaw=28 * s, rows=2, cols=3)
        p.box('CnTankMetal', 0.24, 0.5, 1.12 * s - 0.12, 1.12 * s + 0.12, top, top + 0.06, bevel=0.0)
    # as caixas de carga presas nos lados, atrás das bochechas (a ficha: o painel em relevo)
    for s in (-1, 1):
        fs = find_face(faces, (0, s, -0.1), near=(-0.6, 1.47 * s, 1.9))
        c = plane_point(fs, x=-0.15, z=1.9)
        on_face(p, 'CnTankPaint', fs, c, (0.9, 0.36, 0.12), bevel=0.015)
        # (31/10) a tampa (a aba de cima um pouco maior: a fresta da tampa), os 2 trincos e a alça na frente da caixa
        on_face(p, 'CnTankPaint', fs, c + Vector((0, 0, 0.13)), (0.94, 0.12, 0.14), bevel=0.015)
        ex_, ey_, n_ = basis(fs[1])
        front = c + n_ * 0.12
        for d in (-0.3, 0.3):
            vp.rbox(p, 'CnTankMetal', front + ex_ * d + ey_ * 0.06 + n_ * 0.012, (0.05, 0.022, 0.07),
                    yaw=math.degrees(math.atan2(ex_.y, ex_.x)), r=0.008, k=1)
        vp.grab_handle(p, 'CnTankMetal', front + ex_ * -0.1 - ey_ * 0.06, front + ex_ * 0.1 - ey_ * 0.06, n_, height=0.05, r=0.014)
        # as alças de mão na parede, por cima da caixa
        a_ = plane_point(fs, x=-0.95, z=2.18)
        b_ = plane_point(fs, x=-0.6, z=2.18)
        vp.grab_handle(p, 'CnTankMetal', a_, b_, fs[1], height=0.08, r=0.016)
    # ---------------------------------------------------------------- a gaiola de carga em U atrás (barras e grade)
    gy = 1.64                       # o lado da gaiola (fora da parede da torre)
    gx0, gx1 = -0.72, -2.96         # do meio da torre até atrás
    z0, z1 = 1.86, 2.44             # o fundo e o corrimão
    def bar(a, b, r=0.022, m='CnTankMetal'):
        a, b = Vector(a), Vector(b)
        d = b - a
        p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=6, bevel=0.0)
    # (31/10) os 3 corrimãos em U contínuos, com as quinas de trás dobradas (o tubo dobrado, não barras coladas)
    for z in (z0, (z0 + z1) / 2, z1):
        vp.tube(p, 'CnTankMetal', vp.bent_bar([(gx0, -gy, z), (gx1, -gy, z), (gx1, gy, z), (gx0, gy, z)], R=0.12, n=4), 0.026, seg=8)
    for s in (-1, 1):
        n = 18
        for k in range(n + 1):
            x = gx0 + (gx1 - gx0) * k / n
            bar((x, gy * s, z0), (x, gy * s, z1), r=0.014 if 0 < k < n else 0.026)
        # os suportes até a parede da torre
        for x in (gx0 + 0.1, -1.6):
            bar((x, gy * s, z0), (x, 1.47 * s, z0 + 0.04), r=0.03)
            bar((x, gy * s, z1), (x, 1.36 * s, z1 - 0.02), r=0.03)
    # a parte de trás: a grade e o fundo de barras
    for k in range(25):
        y = -gy + 2 * gy * k / 24
        bar((gx1, y, z0), (gx1, y, z1), r=0.014)
    for k in range(9):
        y = -gy + 2 * gy * k / 8
        bar((-2.15, y, z0), (gx1, y, z0), r=0.02)
    for x in (-2.18, -2.5):
        bar((x, -gy, z0), (x, gy, z0), r=0.022)
    # a carga: o tambor deitado (atrás, no meio) e a caixa (à esquerda)
    vp.drum(p, (-2.62, -0.12, z0 + 0.22), 'y', 0.2, 0.96, m='CnTankMetal', cap_m='Dark', seg=24)
    vp.ammo_box(p, (-2.575, -1.19, z0 + 0.2), (0.6, 0.52, 0.36), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    for yy in (0.66, 1.02):
        vp.jerrycan(p, (-2.62, yy, z0 + 0.255), yaw=90, m='CnTankCamo', cap_m='CnTankMetal')
    # as ferramentas presas por fora da gaiola (a picareta à esquerda, a pá à direita, nas presilhas)
    vp.pickaxe(p, (-1.45, -gy - 0.05, (z0 + z1) / 2), yaw=180, roll=90, handle_m='CnTankMetal', head_m='Dark', length=0.95, head=0.21)
    vp.shovel(p, (-1.4, gy + 0.05, (z0 + z1) / 2), yaw=180, roll=90, handle_m='CnTankMetal', blade_m='Dark', length=1.05)
    # as travessas de cima da parte de trás (a lona apoia nelas)
    for x in (-2.45, -2.75):
        bar((x, -gy, z1), (x, gy, z1), r=0.022)
    # a lona DOBRADA (cor do jogador) atrás da torre, metade no teto e metade nas travessas, à direita, e a aba caindo rente
    # pela grade de trás (a ficha: centro-direita, de cima, de lado e de trás)
    tarp_fold(p, (-2.2, 0.42, z1 + 0.04), (0.64, 1.12, 0.2), m='Team', straps=2, seed=1.0)
    for yy in (0.14, 0.7):                                                       # as cordas amarrando o fardo no corrimão
        vp.rope(p, 'Cloth', [(-2.5, yy, z1 + 0.09), (-2.6, yy + 0.04, z1 + 0.05), (-2.75, yy + 0.06, z1 + 0.01)], r=0.011, sag=0.02)
    tarp_fold(p, (gx1 - 0.015, 0.42, 2.14), (0.4, 1.04, 0.07), pitch=90, m='Team', straps=2, seed=3.0, layers=1)
    return p.finish()


# ---------------------------------------------------------------------------------- canhão
def barrel():
    p = Part(PREFIX + '_Barrel')
    # (31/10, o detalhe AAA) o tubo por estações (x, raio) num perfil girado só: as transições cônicas (nada de degrau de
    # cilindro), a capa térmica em 3 trechos, o evacuador de fumaça abaulado com as pontas inclinadas, a boca mais grossa com o
    # FURO de 125 mm fundo (o fundo escuro), e as braçadeiras de aço com a borda redonda; a faixa da cor do jogador em relevo
    st = [(2.12, 0.13), (2.44, 0.13), (2.48, 0.121), (3.15, 0.118),
          (3.19, 0.112), (3.84, 0.110),
          (3.88, 0.114), (3.98, 0.147), (4.40, 0.15), (4.52, 0.113),
          (4.58, 0.106), (5.78, 0.097),
          (5.82, 0.104), (5.85, 0.113), (6.03, 0.113), (MUZZLE_X, 0.104)]
    vp.gun_barrel(p, st, GUN_Z, m='CnTankPaint', seg=32, bore=0.0625, bore_depth=0.4, bore_m='Dark')
    for x0, x1, r in ((3.13, 3.21, 0.118), (3.81, 3.89, 0.112), (4.52, 4.6, 0.11), (5.19, 5.26, 0.102)):
        vp.ring_band(p, 'CnTankMetal', x0, x1, r, GUN_Z, lip=0.008, seg=32)
        vp.rbox(p, 'CnTankMetal', ((x0 + x1) / 2, 0.0, GUN_Z - r - 0.012), (0.05, 0.05, 0.03), r=0.012, k=1)   # o fecho da braçadeira
    vp.ring_band(p, 'Team', 4.66, 5.08, 0.104, GUN_Z, lip=0.006, seg=32)        # a faixa no cano (cor do jogador)
    # o sensor de referência da boca (a caixinha no suporte, em cima)
    vp.rbox(p, 'CnTankMetal', (5.9, 0.0, GUN_Z + 0.15), (0.13, 0.07, 0.05), r=0.015, k=2)
    p.box('CnTankMetal', 5.87, 5.93, -0.02, 0.02, GUN_Z + 0.1, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            road_wheel(p, x, y, WHEEL_Z, WHEEL_R, 0.56, s)
        idler(p, IDLER[0], y, IDLER[1], IDLER[2], 0.5, s)
        sprocket(p, SPROCKET[0], y, SPROCKET[1], SPROCKET[2], 0.52, s)
        for x in ROLLERS_X:
            roller(p, x, y, ROLLER_Z, ROLLER_R, 0.36)
    return p.finish()


def circles():
    return [(IDLER[0], IDLER[1], IDLER[2] + 0.045), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.045)] + \
           [(x, WHEEL_Z, WHEEL_R + 0.045) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + 0.045) for x in ROLLERS_X]


def tracks(side):
    s = 1 if side == 'r' else -1
    return track(f'{PREFIX}_Track_{side}', circles(), TRACK_Y * s, TRACK_W, pitch=0.16, thick=0.05)


# ---------------------------------------------------------------------------------- exportação
def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = tracks('l'), tracks('r')
    sweep_check([h, w], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), (hz0, hz1) = bounds([h, w, tl, tr])
    (_, _), (_, _), (_, tz1) = bounds([t])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f), largura %.2f m, altura %.2f m' % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL hull_m (casco com as saias, sem o cano): %.2f x %.2f m; x %.2f..%.2f; o deck a %.2f m' % (hx1 - hx0, hy1 - hy0, hx0, hx1, DECK))
    print('MODEL teto da torre %.2f m (com a cupula/metralhadora %.2f m)' % (T_Z2, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'boca [x, y, z]', [MUZZLE_X, 0.0, GUN_Z])
    print('MODEL tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    write_preview(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')],
                  extra={'tank_views': False, 'usa_at': [0, 6.5, 0]})


main()
