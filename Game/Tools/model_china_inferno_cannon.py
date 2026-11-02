"""O Inferno Cannon da China (a artilharia: o obuseiro autopropulsado de cano longo) provisório, por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_inferno_cannon.py -- captures/game/models/china_inferno_cannon

Fictício inspirado no PLZ-05 (o obuseiro de 155 mm do ELP), não cópia; o pedido de 31/10 (o docs/ELENCO.md §4.2 ainda cita o PHL-03,
o lançador de foguetes: a forma pedida foi a do obuseiro). A palavra: o CANO LONGO. De cima: a torre-caixa grande e quadrada
atrás do meio, o cano comprido (7 m do munhão à boca) passando por cima do nariz, o freio de boca na ponta; atrás, a pá de apoio
levantada. Da família do Type 99: a torre facetada com as bochechas e a estrela, a camuflagem digital, as saias com os recortes,
a lona dobrada no cesto de trás.
Medidas (a régua: o tanque USA, casco 8,1 × 3,8 m, torre a 2,48 m; a artilharia tem a torre alta, como o PLZ-05):
  - chassi de 7,5 × 3,5 m (com as saias), deck a 1,55 m; 7 rodas de 0,66 m por lado, a motriz NA FRENTE (o motor fica na
    frente à direita, o motorista à esquerda), a roda-guia atrás, 3 roletes, esteira de 0,55 m;
  - a torre em (-1,15; 0): 4,25 × 3,1 m, alta no anel (a base a 1,78 m, como a do PLZ-05), teto a 2,88 m; o munhão (o pivô
    de elevação) em HINGE = (0,55; 0; 2,33);
  - o cano: do munhão à boca 7,05 m (a boca em x 7,6, 3,85 m além do nariz), com o berço, os cilindros de recuo, o evacuador
    de fumaça, a faixa e o freio de boca de duas janelas; em repouso horizontal (a pose de marcha); o jogo levanta em volta de
    HINGE (eixo Y) pra atirar em arco;
  - atrás: a pá de apoio (a lâmina de 2,3 m levantada contra a traseira, com os 2 cilindros); na frente, a trava de marcha do
    cano dobrada no glacis;
  - na torre: a cúpula com a metralhadora de 12,7 mm, a mira, as escotilhas, a porta de munição atrás, os cestos dos lados,
    os lançadores de fumaça, a antena; o cesto de trás com a lona dobrada.
Cor do jogador (slot Team): a lona dobrada no cesto atrás da torre, a lona dobrada no deck atrás do motor (na frente da torre,
o motor é na frente), a faixa no cano e as estrelas nos lados da torre. Nunca faixa pintada.

Peças (origem no chão, no centro do casco, frente em +X; todas nas coordenadas do veículo, no lugar delas):
  SM_China_InfernoCannon_Hull     casco, saias, deck, a pá de apoio, a trava de marcha, o anel da torre, a lona do deck;
  SM_China_InfernoCannon_Turret   a torre (gira em Z em volta de TURRET_PIVOT), os cestos, a lona do cesto, a cúpula;
  SM_China_InfernoCannon_Barrel   o cano com o berço (recua em −X; LEVANTA em volta de HINGE, eixo Y); a boca é o X máximo;
  SM_China_InfernoCannon_Wheels   rodas, roletes, motriz, roda-guia (uma malha);
  SM_China_InfernoCannon_Track_l / _Track_r  as esteiras.
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Track, Rubber, Team, Lens, Dark, Cloth.
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, basis, bounds, camo_faces, camo_poly, find_face, grille, hatch, headlight,  # noqa: E402
                                     hmg, idler, on_face, periscope, plane_point, road_wheel, roller, smoke_launchers, solid,
                                     sprocket, star, sweep_check, tarp_fold, tow_hook, track, tris, write_preview)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_inferno_cannon')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_InfernoCannon'

# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 3.75, -3.75         # o chassi: 7,5 m
BELLY, DECK = 0.44, 1.55
LOW_HW = 1.0
UP_HW = 1.70
SKIRT = (1.70, 1.75)
SKIRT_Z = (0.66, 1.46)
SPON_Z = 1.04
TRACK_Y, TRACK_W = 1.36, 0.55
WHEEL_R = 0.33
WHEEL_Z = WHEEL_R + 0.065
WHEELS_X = [2.7, 1.8, 0.9, 0.0, -0.9, -1.8, -2.7]
SPROCKET = (3.3, 0.66, 0.3)      # a motriz na frente (o motor é na frente)
IDLER = (-3.32, 0.6, 0.27)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.8, 0.0, -1.8], 0.88, 0.085
GLACIS = ((NOSE, 1.1), (2.75, DECK))
TX = -1.15                       # o eixo da torre
T_Z0, T_Z1, T_Z2 = 1.78, 2.38, 2.88
HINGE = (0.55, 0.0, 2.33)        # o munhão: o pivô de elevação do cano (eixo Y)
GUN_Z = HINGE[2]
MUZZLE_X = 7.6


def glacis_z(x):
    (x0, z0), (x1, z1) = GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


def bar(p, a, b, r=0.02, m='CnTankMetal', seg=6):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=seg, bevel=0.0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('CnTankPaint#shell', 0.045)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.005)
    low = [(3.4, BELLY), (NOSE, 0.98), (NOSE, SPON_Z + 0.02), (REAR, SPON_Z + 0.02), (REAR, 0.7), (-3.45, BELLY)]
    f_low = solid(p, 'CnTankPaint#shell', [(x, s * LOW_HW, z) for x, z in low for s in (-1, 1)])
    up = [(NOSE, SPON_Z), (NOSE, GLACIS[0][1]), (GLACIS[1][0], DECK), (-3.68, DECK), (REAR, 1.5), (REAR, SPON_Z)]
    f_up = solid(p, 'CnTankPaint#shell', [(x, s * UP_HW, z) for x, z in up for s in (-1, 1)])
    camo_faces(p, f_low, seed=31, skip=lambda c, n: abs(n.y) > 0.9 or c.z > SPON_Z)
    camo_faces(p, f_up, seed=31, skip=lambda c, n: abs(n.y) > 0.9 and c.z < SKIRT_Z[1])
    # ---------------------------------------------------------------- as saias (4 chapas por lado, os recortes sobre as rodas)
    cuts = [3.6, 1.85, 0.0, -1.85, -3.6]
    for s in (-1, 1):
        y0, y1 = (SKIRT[0], SKIRT[1]) if s > 0 else (-SKIRT[1], -SKIRT[0])
        for i, (xa, xb) in enumerate(zip(cuts[:-1], cuts[1:])):
            xa, xb = xa - 0.02, xb + 0.02
            if i == 0:
                prof = [(xa, SKIRT_Z[1]), (xa, 1.08), (xa - 0.32, SKIRT_Z[0]), (xb, SKIRT_Z[0]), (xb, SKIRT_Z[1])]
            elif i == len(cuts) - 2:
                prof = [(xa, SKIRT_Z[1]), (xa, SKIRT_Z[0]), (xb + 0.2, SKIRT_Z[0]), (xb, 0.92), (xb, SKIRT_Z[1])]
            else:
                prof = [(xa, SKIRT_Z[1]), (xa, SKIRT_Z[0]), (xb, SKIRT_Z[0]), (xb, SKIRT_Z[1])]
            fs = solid(p, 'CnTankPaint', [(x, y, z) for x, z in prof for y in (y0, y1)], bevel=0.015)
            camo_faces(p, fs, seed=31, skip=lambda c, n: abs(n.y) < 0.9 or n.y * s < 0, inset=0.04)
            # (31/10) a fileira de parafusos de cima da chapa
            nb = max(3, int(round((xa - xb) / 0.2)))
            vp.bolt_row(p, 'CnTankMetal', (xa - 0.1, SKIRT[1] * s, SKIRT_Z[1] - 0.07), (xb + 0.1, SKIRT[1] * s, SKIRT_Z[1] - 0.07), nb,
                        (0, s, 0), r=0.02, h=0.015)
        for xc in cuts[1:-1]:
            yy0, yy1 = sorted((UP_HW * s, (SKIRT[0] + 0.01) * s))
            p.box('Dark', xc - 0.025, xc + 0.025, yy0, yy1, SKIRT_Z[0] + 0.02, SKIRT_Z[1] - 0.02, bevel=0.0)
        for x in WHEELS_X:
            yy0, yy1 = sorted((SKIRT[1] * s, (SKIRT[1] + 0.006) * s))
            p.box('Dark', x - 0.08, x + 0.08, yy0, yy1, SKIRT_Z[0] + 0.03, SKIRT_Z[0] + 0.14, bevel=0.0)
        yy0, yy1 = sorted((UP_HW * s, (SKIRT[1] + 0.012) * s))
        p.box('CnTankPaint#thin', -3.6, 3.3, yy0, yy1, SKIRT_Z[1], SKIRT_Z[1] + 0.03)
        yy0, yy1 = sorted((LOW_HW * s, (LOW_HW + 0.02) * s))
        p.box('Dark', -3.2, 3.1, yy0, yy1, 0.48, SPON_Z - 0.02, bevel=0.0)
        for x in WHEELS_X:
            yy0, yy1 = sorted((LOW_HW * s, (TRACK_Y - TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.24, x + 0.08, yy0, yy1, WHEEL_Z - 0.05, WHEEL_Z + 0.05, bevel=0.0)
    # ---------------------------------------------------------------- a frente: o motorista (esquerda), o motor (direita), o escape
    hatch(p, 2.25, -0.85, DECK, r=0.28, hinge_side=-1)
    for k, yaw in enumerate((-25, 0, 25)):
        periscope(p, 2.62, -0.85 + (k - 1) * 0.24, DECK - 0.02, yaw=yaw)
    grille(p, 1.55, 2.6, 0.15, 1.5, DECK, n=9)
    p.box('Dark', 1.6, 2.5, UP_HW, UP_HW + 0.012, SKIRT_Z[1] + 0.035, DECK - 0.03, bevel=0.0)
    for k in range(6):
        p.obox('CnTankMetal', (1.67 + k * 0.155, UP_HW + 0.02, (SKIRT_Z[1] + DECK) / 2 + 0.01), (0.03, 0.03, 0.04), bevel=0.0)
    # a lona DOBRADA atrás do motor (cor do jogador): no deck, entre a grelha e a torre, à direita
    tarp_fold(p, (1.1, 0.85, DECK), (0.62, 1.05, 0.15), m='Team', straps=2, seed=9.0)
    # a trava de marcha do cano, dobrada no glacis (o "A" deitado com o berço em U)
    for s in (-1, 1):
        a = (3.5, 0.32 * s, glacis_z(3.5) + 0.04)
        b = (2.85, 0.12 * s, glacis_z(2.85) + 0.04)
        vp.tube(p, 'CnTankMetal', [a, b], 0.035, seg=10)
        vp.rbox(p, 'CnTankMetal', (3.49, 0.32 * s, glacis_z(3.5) + 0.03), (0.14, 0.12, 0.1), r=0.03, k=2)
        p.cyl('Dark#vp', (3.49, 0.32 * s, glacis_z(3.5) + 0.04), 'y', 0.03, 0.16, seg=10, bevel=0.0)
    p.obox('CnTankMetal', (2.8, 0.0, glacis_z(2.8) + 0.08), (0.12, 0.42, 0.1), pitch=-22, bevel=0.0)
    p.obox('Dark', (2.8, 0.0, glacis_z(2.8) + 0.14), (0.1, 0.24, 0.04), pitch=-22, bevel=0.0)
    # os faróis, os ganchos, a régua de respingo
    for s in (-1, 1):
        x = 3.45
        z = glacis_z(x)
        p.obox('CnTankPaint', (x, 1.38 * s, z + 0.08), (0.36, 0.38, 0.15), pitch=-24, bevel=0.012)
        headlight(p, (x + 0.12, 1.46 * s, z + 0.1), r=0.065)
        headlight(p, (x + 0.12, 1.28 * s, z + 0.1), r=0.05, guard=False)
        tow_hook(p, NOSE - 0.02, 0.6 * s, 0.9, s=1)
    # (31/10) as ferramentas presas no para-lama esquerdo, o cabo de reboque no direito (baixos: a torre passa a 23 cm do deck)
    vp.shovel(p, (1.0, -1.58, DECK + 0.025), yaw=180, handle_m='CnTankMetal', blade_m='Dark', length=1.05)
    vp.pickaxe(p, (-1.0, -1.58, DECK + 0.025), yaw=180, handle_m='CnTankMetal', head_m='Dark', length=0.95, head=0.2)
    vp.crowbar(p, (-2.4, -1.5, DECK + 0.018), yaw=0, m='CnTankMetal', length=1.0)
    for x in (1.3, 0.55, -0.7, -1.35):
        p.box('CnTankMetal', x - 0.03, x + 0.03, -1.66, -1.48, DECK, DECK + 0.06, bevel=0.0)              # as presilhas
    vp.tow_cable(p, 'CnTankMetal', [(1.6, 1.58, DECK + 0.026), (-2.7, 1.58, DECK + 0.026)], r=0.024)
    # o anel da torre e as argolas
    p.cyl('Dark', (TX, 0.0, DECK + 0.015), 'z', 1.3, 0.03, seg=48, bevel=0.0)
    p.cyl('CnTankMetal', (TX, 0.0, DECK + 0.005), 'z', 1.36, 0.012, seg=48, bevel=0.0)
    for (x, y) in ((3.2, 1.55), (3.2, -1.55)):
        p.torus('CnTankMetal', (x, y, DECK + 0.03), 'y', 0.045, 0.012, seg=12, rseg=6)
    # ---------------------------------------------------------------- a traseira: a PÁ DE APOIO levantada (a lâmina, os dentes, as
    # dobradiças embaixo e os 2 cilindros hidráulicos), as lanternas
    bx = REAR - 0.2
    p.prism_xz('CnTankPaint', [(bx - 0.1, 0.5), (bx + 0.04, 0.5), (bx + 0.04, 1.34), (bx - 0.02, 1.4), (bx - 0.14, 1.32), (bx - 0.14, 0.62)],
               -1.15, 1.15, bevel=0.02)
    for y in (-0.85, -0.3, 0.3, 0.85):
        p.box('CnTankPaint', bx - 0.2, bx - 0.13, y - 0.05, y + 0.05, 0.62, 1.3, bevel=0.01)          # as nervuras
    for y in (-0.8, 0.0, 0.8):
        p.prism_xz('Dark', [(bx - 0.12, 0.5), (bx + 0.02, 0.5), (bx - 0.05, 0.36)], y - 0.12, y + 0.12)  # os dentes
    for s in (-1, 1):
        p.box('CnTankMetal', REAR - 0.16, REAR, 0.95 * s - 0.08, 0.95 * s + 0.08, 0.5, 0.66, bevel=0.0)    # as dobradiças
        p.cyl('Dark', (REAR - 0.1, 0.95 * s, 0.58), 'y', 0.07, 0.22, seg=12, bevel=0.0)
        a = Vector((REAR - 0.02, 0.62 * s, 1.42))
        b = Vector((bx - 0.17, 0.62 * s, 1.0))
        d = b - a
        L = d.length
        vp.lathe(p, 'CnTankPaint', a, d, [(0.0, 0.02), (0.06, 0.02), (0.075, 0.05), (0.075, L * 0.55), (0.06, L * 0.58), (0.0, L * 0.58)], seg=14)
        vp.lathe(p, 'CnTankMetal', a, d, [(0.0, L * 0.5), (0.038, L * 0.5), (0.038, L - 0.04), (0.0, L - 0.04)], seg=12)
        for q in (a, b):
            p.cyl('CnTankMetal#vp', tuple(q), 'y', 0.05, 0.16, seg=12, bevel=0.0)
        p.box('Dark', REAR - 0.06, REAR, 1.45 * s - 0.09, 1.45 * s + 0.09, 1.28, 1.4, bevel=0.0)
        p.box('Lens', REAR - 0.07, REAR - 0.05, 1.45 * s - 0.06, 1.45 * s + 0.06, 1.31, 1.37, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_points():
    pts = []
    for s in (-1, 1):
        pts += [(0.95, 1.0 * s, T_Z0), (0.55, 1.56 * s, T_Z0), (-3.15, 1.56 * s, T_Z0), (-3.3, 1.3 * s, T_Z0)]
        pts += [(0.85, 0.95 * s, T_Z1), (0.45, 1.52 * s, T_Z1), (-3.15, 1.52 * s, T_Z1), (-3.3, 1.26 * s, T_Z1)]
        pts += [(0.25, 0.85 * s, T_Z2), (-0.1, 1.42 * s, T_Z2), (-3.08, 1.42 * s, T_Z2), (-3.22, 1.18 * s, T_Z2)]
    return pts


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('CnTankPaint#shell', 0.07)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.0)
    faces = solid(p, 'CnTankPaint#shell', turret_points())
    p.cyl('Dark', (TX, 0.0, (DECK + 0.03 + T_Z0) / 2), 'z', 1.26, T_Z0 - DECK - 0.035, seg=48, bevel=0.0)
    # a estrela nos lados (na frente da parede, antes do cesto), com a camuflagem contornando
    stars = []
    for s in (-1, 1):
        fs = find_face(faces, (0.0, s, 0.05), near=(-1.0, 1.55 * s, 1.95))
        stars.append((fs, plane_point(fs, x=0.05, z=2.0)))
    side_centers = [sum(f[0], Vector()) / len(f[0]) for f, _ in stars]
    camo_faces(p, faces, seed=32, skip=lambda c, n: any((c - sc).length < 0.02 for sc in side_centers))
    for (q, n), c in stars:
        ex, ey, _ = basis(n)
        r = 0.22
        camo_poly(p, q, n, seed=32, holes=[(c.dot(ex) - r, c.dot(ex) + r, c.dot(ey) - r, c.dot(ey) + r)])
        star(p, c, n, r)
    # a janela do cano na frente (a fenda escura por onde o cano levanta) e a moldura
    nf = find_face(faces, (1.0, 0.0, 0.1), near=(0.9, 0.0, 1.9))
    c = plane_point(nf, y=0.0, z=HINGE[2] - 0.03)
    on_face(p, 'CnTankPaint', nf, c, (0.95, 0.9, 0.08), bevel=0.02)
    on_face(p, 'Dark', nf, c, (0.52, 0.76, 0.09), bevel=0.0)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    cx, cy = -2.25, -0.72
    vp.lathe(p, 'CnTankPaint', (cx, cy, top), 'z', [(0.0, 0.0), (0.4, 0.0), (0.4, 0.03), (0.385, 0.045), (0.37, 0.16), (0.355, 0.18),
                                                   (0.3, 0.19), (0.29, 0.215), (0.24, 0.245), (0.12, 0.258), (0.0, 0.262)], seg=30)
    for k in range(6):
        a = math.radians(k * 50 - 125)
        ca, sa = math.cos(a), math.sin(a)
        p.obox('Dark', (cx + 0.365 * ca, cy + 0.365 * sa, top + 0.105), (0.04, 0.15, 0.065), yaw=k * 50 - 125, bevel=0.0)
        vp.rbox(p, 'CnTankPaint', (cx + 0.375 * ca, cy + 0.375 * sa, top + 0.155), (0.08, 0.2, 0.025), yaw=k * 50 - 125, r=0.02, k=2)
    for yy in (-0.15, 0.35):                                                      # (31/10) as alças do teto
        vp.grab_handle(p, 'CnTankMetal', (0.0, yy - 0.12, top), (0.0, yy + 0.12, top), (0, 0, 1), height=0.07, r=0.016)
    hmg(p, (cx + 0.18, cy, top + 0.24), yaw=0.0, scale=1.0)
    # a mira do atirador (a caixa na frente, à esquerda) e a escotilha à direita, os respiros
    gx, gy = -0.45, -0.8
    p.box('CnTankPaint', gx - 0.3, gx + 0.25, gy - 0.25, gy + 0.25, top, top + 0.36, bevel=0.02)
    p.box('Dark', gx + 0.25, gx + 0.27, gy - 0.18, gy + 0.18, top + 0.1, top + 0.3, bevel=0.0)
    p.box('Lens', gx + 0.265, gx + 0.28, gy - 0.15, gy + 0.15, top + 0.13, top + 0.27, bevel=0.0)
    p.box('CnTankPaint#thin', gx - 0.05, gx + 0.33, gy - 0.27, gy + 0.27, top + 0.36, top + 0.39)
    hatch(p, -1.75, 0.75, top, r=0.3, hinge_side=1)
    periscope(p, -1.1, 0.75, top, yaw=10)
    for (x, y) in ((-1.0, -0.1), (-2.75, 0.6)):
        p.cyl('CnTankPaint', (x, y, top + 0.04), 'z', 0.18, 0.08, seg=20, bevel=0.01)
        p.cyl('Dark', (x, y, top + 0.09), 'z', 0.12, 0.02, seg=16, bevel=0.0)
    antenna(p, -2.95, 1.05, top, 1.8)
    for s in (-1, 1):
        smoke_launchers(p, (0.0, 1.15 * s, top + 0.12), yaw=30 * s, rows=2, cols=3)
    # a porta de munição na traseira da torre
    rf = find_face(faces, (-1.0, 0.0, 0.0), near=(-3.3, 0.0, 2.0))
    rc = plane_point(rf, y=0.0, z=2.3)
    on_face(p, 'Dark', rf, rc, (1.16, 0.81, 0.012))
    on_face(p, 'CnTankPaint', rf, rc, (1.1, 0.75, 0.06), bevel=0.02)                      # (31/10) a porta com espessura
    for yy in (-0.32, 0.32):
        vp.hinge(p, 'CnTankMetal', rc + Vector((-0.07, yy, 0.38)), 'y', 0.2, r=0.028, knuckles=3)
    vp.grab_handle(p, 'CnTankMetal', rc + Vector((-0.06, -0.15, -0.15)), rc + Vector((-0.06, 0.15, -0.15)), (-1, 0, 0), height=0.07, r=0.016)
    # ---------------------------------------------------------------- os cestos dos lados (barras com as caixas dentro) e o de trás
    for s in (-1, 1):
        y = 1.72 * s
        x0, x1 = -0.55, -2.95
        z0, z1 = 1.95, 2.62
        for z in (z0, z1):
            vp.tube(p, 'CnTankMetal', vp.bent_bar([(x0, 1.55 * s, z), (x0, y, z), (x1, y, z), (x1, 1.4 * s, z)], R=0.08, n=3), 0.022, seg=8)
        for k in range(9):
            x = x0 + (x1 - x0) * k / 8
            bar(p, (x, y, z0), (x, y, z1), r=0.016 if 0 < k < 8 else 0.022)
            if k in (0, 4, 8):
                bar(p, (x, y, z0), (x, 1.53 * s, z0 + 0.03), r=0.02)
        bar(p, (x0, 1.62 * s, z0), (x1, 1.62 * s, z0), r=0.02)
        for (xa, xb) in ((-0.7, -1.4), (-1.5, -2.1)):
            vp.ammo_box(p, ((xa + xb) / 2, 1.63 * s, z0 + 0.21), (0.68, 0.14, 0.38), m='CnTankPaint', latch_m='CnTankMetal')
        vp.drum(p, (-2.55, 1.63 * s, z0 + 0.26), 'x', 0.11, 0.6, m='CnTankMetal', cap_m='Dark', seg=18)
    gx0, gx1, gy = -3.25, -3.85, 1.3
    z0, z1 = 1.92, 2.78
    for z in (z0, z1):
        vp.tube(p, 'CnTankMetal', vp.bent_bar([(gx0, -gy, z), (gx1, -gy, z), (gx1, gy, z), (gx0, gy, z)], R=0.1, n=4), 0.022, seg=8)
    for k in range(15):
        y = -gy + 2 * gy * k / 14
        bar(p, (gx1, y, z0), (gx1, y, z1), r=0.015)
    for k in range(7):
        y = -gy + 2 * gy * k / 6
        bar(p, (gx0, y, z0), (gx1, y, z0), r=0.018)
    for x in (-3.45, -3.68):
        bar(p, (x, -gy, z1), (x, gy, z1), r=0.02)
    vp.ammo_box(p, (-3.56, -0.875, z0 + 0.21), (0.65, 0.46, 0.38), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    vp.ammo_box(p, (-3.56, 0.825, z0 + 0.17), (0.75, 0.46, 0.3), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    # a lona DOBRADA (cor do jogador) em cima do cesto de trás, metade no teto, e a aba caindo rente pela grade
    tarp_fold(p, (-3.57, -0.05, z1 + 0.02), (0.54, 1.2, 0.2), m='Team', straps=2, seed=10.0)
    tarp_fold(p, (gx1 - 0.015, -0.05, 2.46), (0.42, 1.12, 0.07), pitch=90, m='Team', straps=2, seed=11.0, layers=1)
    return p.finish()


# ---------------------------------------------------------------------------------- cano (levanta em volta de HINGE)
def barrel():
    p = Part(PREFIX + '_Barrel')
    hx, _, hz = HINGE
    # (31/10, o detalhe AAA) o berço: o munhão, o colar torneado que sai da janela, os cilindros de recuo (camisa + haste) e a
    # braçadeira; o tubo num perfil girado (as transições cônicas, o evacuador abaulado), as braçadeiras de aço, a faixa em relevo e
    # o freio de boca de duas janelas com o furo
    p.cyl('CnTankMetal#vp', (hx, 0.0, hz), 'y', 0.16, 0.9, seg=20, bevel=0.0)
    vp.lathe(p, 'CnTankPaint', (hx, 0.0, hz), 'x', [(0.0, 0.0), (0.27, 0.0), (0.275, 0.04), (0.275, 0.84), (0.26, 0.9), (0.2, 0.93),
                                                  (0.0, 0.93)], seg=28)
    vp.ring_band(p, 'CnTankMetal', hx + 0.88, hx + 0.94, 0.265, hz, lip=0.012, seg=28)
    for dz in (-0.22, 0.22):
        vp.lathe(p, 'CnTankPaint', (hx + 0.7, 0.0, hz + dz), 'x', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.48), (0.075, 0.52), (0.0, 0.52)], seg=14)
        vp.lathe(p, 'CnTankMetal', (hx + 1.2, 0.0, hz + dz), 'x', [(0.0, 0.0), (0.055, 0.0), (0.055, 0.5), (0.065, 0.52), (0.065, 0.58),
                                                                  (0.0, 0.58)], seg=12)
    vp.rbox(p, 'CnTankPaint', (hx + 1.65, 0.0, hz), (0.2, 0.24, 0.62), r=0.05, k=2)              # a braçadeira dos cilindros
    st = [(hx + 0.9, 0.16), (3.15, 0.15), (3.22, 0.152), (3.32, 0.192), (3.82, 0.188), (3.92, 0.15),
          (4.0, 0.136), (6.95, 0.125), (7.0, 0.13)]
    vp.gun_barrel(p, st, hz, m='CnTankPaint', seg=30, bore=0.0, bore_depth=0.0, bore_m=None)
    for x0, x1, r in ((3.17, 3.24, 0.152), (3.88, 3.95, 0.15), (4.58, 4.64, 0.134), (6.38, 6.44, 0.127)):
        vp.ring_band(p, 'CnTankMetal', x0, x1, r, hz, lip=0.01, seg=30)
    vp.ring_band(p, 'Team', 5.05, 5.55, 0.131, hz, lip=0.007, seg=30)                        # a faixa no cano
    vp.muzzle_brake(p, 'CnTankMetal', 7.0, MUZZLE_X, 0.21, hz, ports=2, port_m='Dark', seg=26)
    p.cyl('Dark#vp', (MUZZLE_X - 0.02, 0.0, hz), 'x', 0.105, 0.03, seg=20, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            road_wheel(p, x, y, WHEEL_Z, WHEEL_R, 0.49, s)
        sprocket(p, SPROCKET[0], y, SPROCKET[1], SPROCKET[2], 0.46, s, teeth=12)
        idler(p, IDLER[0], y, IDLER[1], IDLER[2], 0.44, s)
        for x in ROLLERS_X:
            roller(p, x, y, ROLLER_Z, ROLLER_R, 0.32)
    return p.finish()


def circles():
    return [(IDLER[0], IDLER[1], IDLER[2] + 0.04), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.04)] + \
           [(x, WHEEL_Z, WHEEL_R + 0.04) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + 0.04) for x in ROLLERS_X]


def tracks(side):
    s = 1 if side == 'r' else -1
    return track(f'{PREFIX}_Track_{side}', circles(), TRACK_Y * s, TRACK_W, pitch=0.15, thick=0.045, bevel=0.005)


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
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (_, _), (_, _), (_, tz1) = bounds([t])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f), largura %.2f m, altura %.2f m' % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL hull_m (chassi com as saias e a pa): %.2f x %.2f m; x %.2f..%.2f; o deck a %.2f m' % (hx1 - hx0, hy1 - hy0, hx0, hx1, DECK))
    print('MODEL teto da torre %.2f m (com a cupula %.2f m)' % (T_Z2, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'boca [x, y, z]', [MUZZLE_X, 0.0, GUN_Z],
          'cano: do munhao a boca %.2f m' % (MUZZLE_X - HINGE[0]))
    print('MODEL barrel hinge_m (pivo de elevacao, eixo Y)', list(HINGE))
    print('MODEL tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    # a pose de tiro (só pra prévia, fora da importação): o cano levantado 45° em volta do munhão. (31/10) Com o executor do detalhe
    # (model_run_detail.py) a exportação fica adiada: girar e renomear o cano aqui estragava o GLB principal (saía levantado e com o
    # nome _Barrel_Fire na pasta da importação). Aí a pose não sai
    import model_lib
    if model_lib.DETAIL.get('defer') is None:
        pose = os.path.join(OUT, 'pose')
        os.makedirs(pose, exist_ok=True)
        piv = V(*HINGE)
        b.data.transform(Matrix.Translation(piv) @ Matrix.Rotation(math.radians(-45), 4, 'Y') @ Matrix.Translation(-piv))   # (no Blender, −45° em Y levanta o nariz)
        b.name = PREFIX + '_Barrel_Fire'
        export(b, pose)
    write_preview(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')],
                  extra={'tank_views': False, 'usa_at': [0, 6.5, 0]})


main()
