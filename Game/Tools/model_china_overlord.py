"""O Overlord da China (o tanque gigante de dois canos) provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_overlord.py -- captures/game/models/china_overlord

A escapada de ficção (docs/ELENCO.md §4.2: "o tanque chinês gigante", o pesado do mod Contra), com a ideia guardada pra ele em
docs/concept/units/china_mbt_exploration.webp, design 2 (a unidade reservada da bíblia: o canhão duplo é só dele). A palavra: os
DOIS CANOS. De cima: o retângulo enorme (1,3× o tanque USA), a torre-bloco com os dois canos grossos lado a lado saindo de duas
caixas blindadas, as esteiras largas e as saias de módulos. Da família do Type 99 (model_china_type99.py): a torre facetada com
as bochechas e a estrela, a gaiola em U atrás com a lona dobrada, a camuflagem digital, o rolo e as cintas.
Medidas (a régua: o tanque USA, casco 8,1 × 3,8 m, torre a 2,48 m):
  - casco 10,5 × 5,04 m (com os módulos das saias), barriga a 0,55 m, deck a 2,05 m, teto da torre a 3,25 m (1,3× o USA);
  - 8 rodas de 0,9 m por lado, 4 roletes, a roda-guia na frente e a motriz atrás, esteira de 0,92 m com a guia dupla;
  - as saias de 7 MÓDULOS grossos (18 cm) por lado, cada um com a chapa por cima, os parafusos e a junta escura;
  - o glacis com 2 fileiras de tijolos de blindagem reativa (12), os faróis em caixa blindada nos cantos;
  - a torre em (-0,45; 0): 6,9 m da caixa dos canos à traseira, 4,3 m de largura; as duas caixas blindadas dos canos saindo
    da frente; os canos em y ±0,64 a 2,63 m, com o evacuador, a luva térmica e o freio de boca quadrado; a boca em x 8,15;
  - na torre: a cúpula do comandante com a metralhadora de 12,7 mm, a mira do atirador, a mira panorâmica, os lançadores de
    fumaça de 6 tubos nos lados, as caixas laterais, a gaiola em U, o estandarte pequeno e a antena;
  - atrás: os 2 tambores de combustível (como os tanques chineses levam), os escapes, a lona dobrada no deck do motor.
Cor do jogador (slot Team; lonas pequenas dobradas e presas rente + a faixa no cano, a bíblia): a lona dobrada na gaiola atrás
da torre (e a aba caindo pela grade), a lona dobrada no deck atrás do motor, a faixa nos DOIS canos, o estandarte pequeno e as
estrelas nas bochechas. Nunca faixa pintada.

Peças (origem no chão, no centro do casco, frente em +X; todas nas coordenadas do veículo, no lugar delas):
  SM_China_Overlord_Hull     casco, módulos das saias, glacis com a blindagem reativa, deck, tambores, o anel da torre;
  SM_China_Overlord_Turret   a torre (gira em Z em volta de TURRET_PIVOT) com as caixas dos canos, a gaiola, o estandarte;
  SM_China_Overlord_Barrel   os DOIS canos numa peça só (recuam juntos em −X); as bocas são os X máximos (y ±0,64);
  SM_China_Overlord_Wheels   rodas, roletes, roda-guia, motriz (uma malha: a esteira não usa as malhas wheel_*);
  SM_China_Overlord_Track_l / _Track_r  as esteiras largas.
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Track, Rubber, Team, Lens, Dark, Cloth.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, basis, bounds, camo_faces, camo_poly, find_face, frame_box, grille, hatch,  # noqa: E402
                                     headlight, hmg, idler, on_face, periscope, plane_point, road_wheel, roller,
                                     smoke_launchers, solid, sprocket, star, sweep_check, tarp_fold, tow_hook, track, tris,
                                     write_preview, _frame)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_overlord')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_Overlord'

# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 5.25, -5.25         # o casco: 10,5 m
BELLY, DECK = 0.55, 2.05
LOW_HW = 1.34
UP_HW = 2.34
SKIRT = (2.34, 2.52)             # os módulos das saias (de dentro, de fora)
SKIRT_Z = (0.80, 1.96)
SPON_Z = 1.40
TRACK_Y, TRACK_W = 1.86, 0.92
WHEEL_R = 0.45
WHEEL_Z = WHEEL_R + 0.08
WHEELS_X = [3.85, 2.75, 1.65, 0.55, -0.55, -1.65, -2.75, -3.85]
IDLER = (4.72, 0.80, 0.38)
SPROCKET = (-4.78, 0.84, 0.42)
ROLLERS_X, ROLLER_Z, ROLLER_R = [3.3, 1.1, -1.1, -3.3], 1.18, 0.11
GLACIS = ((NOSE, 1.52), (3.95, DECK))
TX = -0.45                       # o eixo da torre
T_Z0, T_Z1, T_Z2 = 2.14, 2.88, 3.42
GUN_Z, GUN_Y = 2.72, 0.64
HOUSING_X = 3.35                 # a frente das caixas dos canos
MUZZLE_X = 8.15


def glacis_z(x):
    (x0, z0), (x1, z1) = GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


def bar(p, a, b, r=0.025, m='CnTankMetal', seg=6):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=seg, bevel=0.0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('CnTankPaint#shell', 0.06)
    p.bm('CnTankPaint', 0.03)
    p.bm('CnTankPaint#thin', 0.008)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.006)
    low = [(4.85, BELLY), (NOSE, 1.32), (NOSE, SPON_Z + 0.02), (REAR, SPON_Z + 0.02), (REAR, 0.92), (-4.95, BELLY)]
    f_low = solid(p, 'CnTankPaint#shell', [(x, s * LOW_HW, z) for x, z in low for s in (-1, 1)])
    up = [(NOSE, SPON_Z), (NOSE, GLACIS[0][1]), (GLACIS[1][0], DECK), (-5.15, DECK), (REAR, 1.95), (REAR, SPON_Z)]
    f_up = solid(p, 'CnTankPaint#shell', [(x, s * UP_HW, z) for x, z in up for s in (-1, 1)])
    camo_faces(p, f_low, seed=11, skip=lambda c, n: abs(n.y) > 0.9 or c.z > SPON_Z)
    glacis = find_face(f_up, (0.42, 0.0, 0.91))
    camo_faces(p, f_up, seed=11, skip=lambda c, n: (abs(n.y) > 0.9 and c.z < SKIRT_Z[1]) or n.x > 0.3 and n.z > 0.5)
    # ---------------------------------------------------------------- as saias: 7 módulos grossos por lado (a chapa por cima, os
    # parafusos, a quina de baixo chanfrada pra dentro, a junta escura entre eles)
    cuts = [5.08, 3.62, 2.2, 0.8, -0.6, -2.0, -3.42, -5.18]
    for s in (-1, 1):
        yi, yo = SKIRT[0] * s, SKIRT[1] * s
        for i, (xa, xb) in enumerate(zip(cuts[:-1], cuts[1:])):
            xa, xb = xa - 0.03, xb + 0.03
            z0, z1 = SKIRT_Z
            fr = 0.55 if i == 0 else 0.0           # o chanfro da frente do 1º módulo
            bk = 0.35 if i == len(cuts) - 2 else 0.0
            pts = [(xa, yi, z1), (xb, yi, z1), (xa - fr, yi, z0), (xb + bk, yi, z0),
                   (xa, yo, z1 - 0.05), (xb, yo, z1 - 0.05), (xa - fr, yo, z0 + 0.14), (xb + bk, yo, z0 + 0.14)]
            if fr:
                pts += [(xa, yi, 1.45), (xa, yo, 1.45)]
            if bk:
                pts += [(xb, yi, 1.2), (xb, yo, 1.2)]
            fs = solid(p, 'CnTankPaint', pts, bevel=0.03)
            # a chapa por cima do módulo (a placa aparafusada) e os 4 parafusos
            fo = find_face(fs, (0, s, 0))
            q, n = fo
            ex, ey, _ = basis(n)
            px0, px1 = xb + bk + 0.14, xa - fr - 0.14 if fr else xa - 0.14
            pc = plane_point(fo, x=(px0 + px1) / 2, z=(z0 + 0.14 + z1 - 0.05) / 2)
            pw, ph = px1 - px0, (z1 - z0) - 0.42
            on_face(p, 'CnTankPaint', fo, pc, (pw, ph, 0.05), bevel=0.02)
            camo_poly(p, [pc + n * 0.05 + ex * (a * pw / 2) + ey * (b * ph / 2) for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))], n,
                      seed=11, inset=0.03)
            # (31/10) as fileiras de parafusos da chapa (em cima e embaixo: o ritmo), assentadas na chapa
            nb = max(3, int(round(pw / 0.2)))
            for bz in (ph / 2 - 0.06, -ph / 2 + 0.06):
                a_ = pc + n * 0.05 + ex * (-pw / 2 + 0.08) + ey * bz
                b_ = pc + n * 0.05 + ex * (pw / 2 - 0.08) + ey * bz
                vp.bolt_row(p, 'CnTankMetal', a_, b_, nb, n, r=0.026, h=0.02)
        for xc in cuts[1:-1]:
            yy0, yy1 = sorted((UP_HW * s, (SKIRT[0] + 0.02) * s))
            p.box('Dark', xc - 0.035, xc + 0.035, yy0, yy1, SKIRT_Z[0] + 0.05, SKIRT_Z[1], bevel=0.0)
        # a beirada do para-lama por cima dos módulos
        yy0, yy1 = sorted((UP_HW * s, (SKIRT[1] + 0.02) * s))
        p.box('CnTankPaint#thin', -5.18, 4.6, yy0, yy1, SKIRT_Z[1], SKIRT_Z[1] + 0.05)
        # o forro escuro por dentro das esteiras e os braços da suspensão
        yy0, yy1 = sorted((LOW_HW * s, (LOW_HW + 0.02) * s))
        p.box('Dark', -4.6, 4.5, yy0, yy1, 0.6, SPON_Z - 0.02, bevel=0.0)
        for x in WHEELS_X:
            yy0, yy1 = sorted((LOW_HW * s, (TRACK_Y - TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.1, x + 0.36, yy0, yy1, WHEEL_Z - 0.08, WHEEL_Z + 0.08, bevel=0.0)
    # ---------------------------------------------------------------- o glacis: a blindagem reativa em 2 fileiras (o vão do meio é
    # o do motorista) e a régua de respingo
    for row, u in enumerate((0.3, 0.72)):
        x = NOSE + (GLACIS[1][0] - NOSE) * u
        for y in (-1.95, -1.38, -0.81, 0.81, 1.38, 1.95):
            c = plane_point(glacis, x=x, y=y)
            on_face(p, 'CnTankPaint', glacis, c, (0.5, 0.5, 0.12), bevel=0.015)
            on_face(p, 'Dark', glacis, c, (0.52, 0.04, 0.125), lift=0.0)
    p.obox('CnTankPaint#thin', (NOSE - 0.2, 0.0, glacis_z(NOSE - 0.2) + 0.04), (0.08, 1.2, 0.08), pitch=-22, bevel=0.0)
    # o motorista no meio, atrás do glacis, com os periscópios
    hatch(p, 3.62, 0.0, DECK, r=0.34, hinge_side=1)
    for k, yaw in enumerate((-30, 0, 30)):
        periscope(p, 4.0, (k - 1) * 0.3, DECK - 0.02, yaw=yaw)
    # o anel da torre
    p.cyl('Dark', (TX, 0.0, DECK + 0.015), 'z', 1.75, 0.03, seg=56, bevel=0.0)
    p.cyl('CnTankMetal', (TX, 0.0, DECK + 0.006), 'z', 1.82, 0.012, seg=56, bevel=0.0)
    # as tampas do deck e o cabo de reboque ao longo do para-lama (o laço escuro)
    for (x0, x1, y0, y1) in ((1.8, 3.4, -2.2, -1.5), (1.8, 3.4, 1.5, 2.2), (-3.5, -2.3, -2.2, -1.55), (-3.5, -2.3, 1.55, 2.2)):
        p.box('CnTankPaint#thin', x0, x1, y0, y1, DECK - 0.01, DECK + 0.035)                 # (31/10) a chapa com espessura
        p.box('Dark', x0 - 0.025, x1 + 0.025, y0 - 0.025, y1 + 0.025, DECK, DECK + 0.004, bevel=0.0)
    for s in (-1, 1):
        y = 2.22 * s
        vp.tow_cable(p, 'CnTankMetal', [(3.4, y, DECK + 0.032), (-3.4, y, DECK + 0.032)], r=0.028, eye_m='CnTankMetal')
        for x in (2.4, 0.0, -2.4):                                            # as presilhas do cabo
            p.box('CnTankMetal', x - 0.04, x + 0.04, y - 0.05, y + 0.05, DECK, DECK + 0.07, bevel=0.0)
    # ---------------------------------------------------------------- o motor: 2 grelhas grandes e a tampa do meio com a lona
    grille(p, -5.08, -3.9, -2.1, -0.45, DECK, n=10)
    grille(p, -5.08, -3.9, 0.45, 2.1, DECK, n=10)
    p.box('CnTankPaint#thin', -5.08, -3.9, -0.4, 0.4, DECK, DECK + 0.04)
    # a lona DOBRADA atrás do motor (cor do jogador), presa rente na tampa do meio
    tarp_fold(p, (-4.72, 0.0, DECK + 0.04), (1.0, 0.66, 0.15), yaw=90, m='Team', straps=2, seed=4.0)
    # ---------------------------------------------------------------- a frente: os faróis em caixa blindada, os ganchos
    for s in (-1, 1):
        x = 4.85
        z = glacis_z(x)
        p.obox('CnTankPaint', (x, 2.0 * s, z + 0.14), (0.55, 0.56, 0.26), pitch=-22, bevel=0.02)
        headlight(p, (x + 0.22, 2.12 * s, z + 0.17), r=0.085)
        headlight(p, (x + 0.22, 1.88 * s, z + 0.17), r=0.065, guard=False)
        tow_hook(p, NOSE + 0.12, 0.95 * s, 1.0, s=1)
        tow_hook(p, REAR + 0.02, 1.4 * s, 1.12, s=-1)
    # o para-choque baixo da frente (a chapa grossa espaçada na frente da chapa de baixo, com as manilhas): o "focinho" do pesado
    p.obox('CnTankPaint', (NOSE + 0.02, 0.0, 1.02), (0.22, 2.6, 0.42), pitch=-25, bevel=0.03)
    for s in (-1, 1):
        p.box('CnTankMetal', NOSE - 0.12, NOSE + 0.05, 0.75 * s - 0.08, 0.75 * s + 0.08, 0.85, 1.0, bevel=0.0)
    # ---------------------------------------------------------------- a traseira: os escapes nos cantos, as lanternas, os tambores
    for s in (-1, 1):
        y0, y1 = sorted((1.15 * s, 2.15 * s))
        p.box('Dark', REAR - 0.02, REAR, y0, y1, 1.42, 1.86, bevel=0.0)
        for k in range(6):
            p.obox('CnTankMetal', (REAR - 0.03, 1.65 * s, 1.46 + k * 0.07), (0.03, 0.95, 0.03), pitch=-30, bevel=0.0)
        p.box('Dark', REAR - 0.06, REAR, 2.0 * s - 0.1, 2.0 * s + 0.1, 1.2, 1.34, bevel=0.0)
        p.box('Lens', REAR - 0.07, REAR - 0.05, 2.0 * s - 0.07, 2.0 * s + 0.07, 1.23, 1.31, bevel=0.0)
        p.obox('CnTankPaint', (REAR - 0.03, 2.0 * s, 1.1), (0.05, 0.9, 0.5), pitch=-20, bevel=0.01)   # o para-lama de trás
        # o tambor de combustível deitado no suporte (as faixas escuras, a tampa)
        cy = 0.75 * s
        vp.drum(p, (REAR - 0.38, cy, 1.62), 'y', 0.32, 1.0, m='CnTankPaint', cap_m='CnTankMetal', seg=28)
        for dy in (-0.26, 0.26):                                              # as cintas de aço com o esticador
            vp.lathe(p, 'CnTankMetal', (REAR - 0.38, cy + dy, 1.62), 'y', [(0.33, -0.025), (0.345, -0.025), (0.345, 0.025),
                                                                            (0.33, 0.025)], seg=28, closed=True)
            vp.rbox(p, 'CnTankMetal', (REAR - 0.38, cy + dy, 1.62 + 0.36), (0.1, 0.06, 0.05), r=0.015, k=1)
        for dy in (-0.38, 0.38):
            p.box('CnTankMetal', REAR - 0.4, REAR, cy + dy - 0.04, cy + dy + 0.04, 1.26, 1.32, bevel=0.0)
            p.box('CnTankMetal', REAR - 0.4, REAR - 0.34, cy + dy - 0.04, cy + dy + 0.04, 1.26, 1.5, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_points():
    pts = []
    for s in (-1, 1):
        pts += [(2.25, 1.55 * s, T_Z0), (1.55, 2.15 * s, T_Z0), (-3.15, 2.12 * s, T_Z0), (-3.5, 1.72 * s, T_Z0)]
        pts += [(2.15, 1.50 * s, T_Z1), (1.45, 2.10 * s, T_Z1), (-3.15, 2.06 * s, T_Z1), (-3.5, 1.66 * s, T_Z1)]
        pts += [(1.55, 1.38 * s, T_Z2), (1.0, 1.90 * s, T_Z2), (-3.08, 1.90 * s, T_Z2), (-3.36, 1.52 * s, T_Z2)]
    return pts


def housing_points(s):
    """A caixa blindada de um canho: sai da frente da torre, afinando, com o teto inclinado pra frente."""
    a, b = 0.14, 1.14
    zb, zt = 2.22, GUN_Z + 0.58
    return [(1.6, s * a, zb), (1.6, s * b, zb), (1.6, s * a, zt), (1.6, s * b, zt),
            (2.9, s * (a + 0.04), zt - 0.06), (2.9, s * (b - 0.04), zt - 0.06),
            (HOUSING_X, s * (a + 0.1), GUN_Z + 0.34), (HOUSING_X, s * (b - 0.1), GUN_Z + 0.34),
            (HOUSING_X, s * (a + 0.1), GUN_Z - 0.34), (HOUSING_X, s * (b - 0.1), GUN_Z - 0.34),
            (3.05, s * (a + 0.04), zb), (3.05, s * (b - 0.04), zb)]


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('CnTankPaint#shell', 0.085)
    p.bm('CnTankPaint', 0.025)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.0)
    faces = solid(p, 'CnTankPaint#shell', turret_points())
    p.cyl('Dark', (TX, 0.0, (DECK + 0.03 + T_Z0) / 2), 'z', 1.7, T_Z0 - DECK - 0.035, seg=56, bevel=0.0)   # a saia do anel
    # a estrela grande nas bochechas (a diagonal da frente), com a camuflagem contornando
    stars = []
    for s in (-1, 1):
        q, n = find_face(faces, (0.65, 0.76 * s, 0.05), near=(1.9, 1.85 * s, 2.4))
        stars.append((q, n, sum(q, Vector()) / len(q)))
    centers = [c for _, _, c in stars]
    camo_faces(p, faces, seed=12, skip=lambda c, n: any((c - sc).length < 0.02 for sc in centers))
    for q, n, c in stars:
        ex, ey, _ = basis(n)
        r = 0.27
        camo_poly(p, q, n, seed=12, holes=[(c.dot(ex) - r, c.dot(ex) + r, c.dot(ey) - r, c.dot(ey) + r)])
        star(p, c, n, r)
    # as duas caixas blindadas dos canos, o colar escuro na boca de cada uma e o vão do meio (escuro)
    for s in (-1, 1):
        hf = solid(p, 'CnTankPaint', housing_points(s), bevel=0.03)
        camo_faces(p, hf, seed=12, skip=lambda c, n: n.x > 0.9)
        p.cyl('Dark', (HOUSING_X + 0.01, s * GUN_Y, GUN_Z), 'x', 0.31, 0.03, seg=28, bevel=0.0)
        # (31/10) o fole de lona de cada cano e o anel de parafusos da boca da caixa
        vp.bellows(p, 'CnTankCamo', HOUSING_X, HOUSING_X + 0.3, 0.29, 0.225, GUN_Z, y=s * GUN_Y, folds=4, depth=0.024, seg=28)
        vp.bolt_ring(p, 'CnTankMetal', (HOUSING_X + 0.005, s * GUN_Y, GUN_Z), 'x', 0.33, 10, r=0.022, h=0.016)
        # a mira coaxial em cima de cada caixa
        p.box('CnTankMetal', 2.62, 2.9, s * GUN_Y - 0.1, s * GUN_Y + 0.1, GUN_Z + 0.5, GUN_Z + 0.6, bevel=0.0)
    p.box('Dark', 2.2, 2.85, -0.19, 0.19, 2.3, GUN_Z + 0.4, bevel=0.0)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    # a cúpula do comandante (atrás, à esquerda) com a metralhadora de 12,7 mm
    cx, cy = -1.75, -1.05
    vp.lathe(p, 'CnTankPaint', (cx, cy, top), 'z', [(0.0, 0.0), (0.48, 0.0), (0.48, 0.035), (0.462, 0.05), (0.445, 0.18), (0.43, 0.2),
                                                   (0.36, 0.21), (0.345, 0.235), (0.29, 0.27), (0.14, 0.285), (0.0, 0.29)], seg=32)
    for k in range(7):
        a = math.radians(k * 45 - 135)
        ca, sa = math.cos(a), math.sin(a)
        p.obox('Dark', (cx + 0.44 * ca, cy + 0.44 * sa, top + 0.115), (0.04, 0.17, 0.075), yaw=k * 45 - 135, bevel=0.0)
        vp.rbox(p, 'CnTankPaint', (cx + 0.45 * ca, cy + 0.45 * sa, top + 0.17), (0.09, 0.22, 0.028), yaw=k * 45 - 135, r=0.02, k=2)
    hmg(p, (cx + 0.22, cy, top + 0.26), yaw=0.0, scale=1.15)
    # a mira do atirador em caixa (à direita, na frente) e a panorâmica do comandante
    gx, gy = 0.55, 1.05
    p.box('CnTankPaint', gx - 0.4, gx + 0.32, gy - 0.32, gy + 0.32, top, top + 0.46, bevel=0.025)
    p.box('Dark', gx + 0.32, gx + 0.34, gy - 0.25, gy + 0.25, top + 0.12, top + 0.38, bevel=0.0)
    p.box('Lens', gx + 0.335, gx + 0.35, gy - 0.21, gy + 0.21, top + 0.15, top + 0.35, bevel=0.0)
    p.box('CnTankPaint#thin', gx - 0.05, gx + 0.42, gy - 0.34, gy + 0.34, top + 0.46, top + 0.5)
    p.cyl('CnTankMetal', (0.2, -0.75, top + 0.14), 'z', 0.07, 0.28, seg=12, bevel=0.0)
    p.cyl('CnTankPaint', (0.2, -0.75, top + 0.38), 'z', 0.18, 0.24, seg=20, bevel=0.01)
    p.box('Lens', 0.36, 0.38, -0.85, -0.65, top + 0.32, top + 0.44, bevel=0.0)
    # (31/10) as alças de mão do teto
    for yy in (-0.5, 0.0):
        vp.grab_handle(p, 'CnTankMetal', (1.3, yy - 0.14, top), (1.3, yy + 0.14, top), (0, 0, 1), height=0.08, r=0.018)
    # as escotilhas, os periscópios, os respiros
    hatch(p, -1.2, 1.05, top, r=0.34, hinge_side=1)
    periscope(p, -0.6, 1.25, top, yaw=15)
    periscope(p, 0.95, -0.35, top, yaw=0)
    for (x, y) in ((-2.55, 0.15), (-0.4, 0.1)):
        p.cyl('CnTankPaint', (x, y, top + 0.05), 'z', 0.22, 0.1, seg=20, bevel=0.01)
        p.cyl('Dark', (x, y, top + 0.11), 'z', 0.15, 0.02, seg=16, bevel=0.0)
    # o sensor de vento e a antena (atrás à direita), o ESTANDARTE pequeno (atrás à esquerda)
    p.cyl('CnTankMetal', (-2.85, 1.4, top + 0.32), 'z', 0.05, 0.64, seg=12, bevel=0.0)
    p.cyl('CnTankPaint', (-2.85, 1.4, top + 0.68), 'z', 0.08, 0.09, seg=12, bevel=0.005)
    antenna(p, -3.05, 1.2, top, 2.0)
    banner_small(p, (-2.95, -1.55), top)
    # os lançadores de fumaça de 6 tubos nos lados (a ficha: os blocos de tubos nos lados da frente da torre)
    for s in (-1, 1):
        fs = find_face(faces, (0.0, s, 0.05), near=(0.0, 2.1 * s, 2.45))
        c = plane_point(fs, x=0.55, z=2.62)
        on_face(p, 'CnTankMetal', fs, c, (0.5, 0.3, 0.14), bevel=0.0)
        smoke_launchers(p, (0.62, 2.36 * s, 2.66), yaw=22 * s, rows=2, cols=3, r=0.055, L=0.42)
        # as caixas de carga nos lados, atrás dos lançadores
        c2 = plane_point(fs, x=-0.9, z=2.45)
        on_face(p, 'CnTankPaint', fs, c2, (1.5, 0.48, 0.2), bevel=0.02)
        # (31/10) a tampa com a aba (a fresta), os trincos e a alça na frente da caixa, as alças de mão por cima
        on_face(p, 'CnTankPaint', fs, c2 + Vector((0, 0, 0.18)), (1.54, 0.14, 0.23), bevel=0.02)
        ex_, ey_, n_ = basis(fs[1])
        front = c2 + n_ * 0.2
        for d in (-0.5, 0.0, 0.5):
            vp.rbox(p, 'CnTankMetal', front + ex_ * d + ey_ * 0.1 + n_ * 0.014, (0.06, 0.026, 0.09),
                    yaw=math.degrees(math.atan2(ex_.y, ex_.x)), r=0.01, k=1)
        vp.grab_handle(p, 'CnTankMetal', front + ex_ * -0.14 - ey_ * 0.08, front + ex_ * 0.14 - ey_ * 0.08, n_, height=0.06, r=0.016)
        vp.grab_handle(p, 'CnTankMetal', plane_point(fs, x=-2.3, z=2.95), plane_point(fs, x=-1.9, z=2.95), fs[1], height=0.09, r=0.018)
    # ---------------------------------------------------------------- a gaiola em U atrás
    gy = 2.24
    gx0, gx1 = -1.95, -4.15
    z0, z1 = 2.5, 3.38
    # (31/10) os corrimãos em U contínuos (o tubo dobrado nas quinas de trás)
    for z in (z0, (z0 + z1) / 2, z1):
        vp.tube(p, 'CnTankMetal', vp.bent_bar([(gx0, -gy, z), (gx1, -gy, z), (gx1, gy, z), (gx0, gy, z)], R=0.14, n=4), 0.03, seg=8)
    for s in (-1, 1):
        n = 16
        for k in range(n + 1):
            x = gx0 + (gx1 - gx0) * k / n
            bar(p, (x, gy * s, z0), (x, gy * s, z1), r=0.017 if 0 < k < n else 0.03)
        for x in (gx0 + 0.1, -3.0):
            bar(p, (x, gy * s, z0), (x, 2.1 * s, z0 + 0.04), r=0.035)
            bar(p, (x, gy * s, z1), (x, 1.9 * s, z1 - 0.02), r=0.035)
    for k in range(29):
        y = -gy + 2 * gy * k / 28
        bar(p, (gx1, y, z0), (gx1, y, z1), r=0.017)
    for k in range(9):
        y = -gy + 2 * gy * k / 8
        bar(p, (-3.48, y, z0), (gx1, y, z0), r=0.024)
    for x in (-3.5, -3.82):
        bar(p, (x, -gy, z0), (x, gy, z0), r=0.026)
    for x in (-3.72, -4.0):
        bar(p, (x, -gy, z1), (x, gy, z1), r=0.026)
    # a carga: as caixas de munição e o tambor
    vp.ammo_box(p, (-3.8, -1.65, z0 + 0.235), (0.86, 0.5, 0.43), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    vp.ammo_box(p, (-3.8, -0.78, z0 + 0.2), (0.62, 0.5, 0.36), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    vp.drum(p, (-3.82, 1.1, z0 + 0.25), 'y', 0.22, 1.02, m='CnTankMetal', cap_m='Dark', seg=24)
    for yy in (1.85, ):
        vp.jerrycan(p, (-3.85, yy, z0 + 0.255), yaw=90, m='CnTankCamo', cap_m='CnTankMetal')
    # as ferramentas presas por fora da gaiola
    vp.pickaxe(p, (-2.9, -gy - 0.06, (z0 + z1) / 2), yaw=180, roll=90, handle_m='CnTankMetal', head_m='Dark', length=1.0, head=0.24)
    vp.shovel(p, (-2.85, gy + 0.06, (z0 + z1) / 2), yaw=180, roll=90, handle_m='CnTankMetal', blade_m='Dark', length=1.1)
    # a lona DOBRADA (cor do jogador) atrás da torre: metade no teto, metade nas travessas, e a aba caindo pela grade
    tarp_fold(p, (-3.42, 0.3, T_Z2), (0.8, 1.45, 0.24), m='Team', straps=2, seed=5.0)
    for yy in (-0.06, 0.66):                                                     # as cordas do fardo até o corrimão
        vp.rope(p, 'Cloth', [(-3.83, yy, T_Z2 + 0.06), (-3.95, yy + 0.04, T_Z2 + 0.0), (-4.12, yy + 0.06, z1 + 0.01)], r=0.012, sag=0.02)
    tarp_fold(p, (gx1 - 0.017, 0.3, 2.86), (0.48, 1.36, 0.08), pitch=90, m='Team', straps=2, seed=6.0, layers=1)
    return p.finish()


def banner_small(p, pole, top):
    """O estandarte pequeno da China (o pano vermelho em pé na travessa, a cor do jogador) no mastro curto com a ponta de lança;
    o pano com uma ondinha (é tecido, não placa)."""
    x, y = pole
    h = 1.75
    p.cyl('CnTankMetal', (x, y, top + 0.05), 'z', 0.07, 0.1, seg=12, bevel=0.0)
    p.cyl('Dark', (x, y, top + h / 2), 'z', 0.025, h, seg=8, bevel=0.0)
    p.cyl('CnTankMetal', (x, y, top + h + 0.1), 'z', 0.045, 0.2, seg=8, r2=0.004, bevel=0.0)       # a ponta de lança
    zt = top + h - 0.08
    p.cyl('Dark', (x - 0.28, y, zt), 'x', 0.015, 0.58, seg=6, bevel=0.0)                            # a travessa (pra trás)
    rings = []
    n = 8
    W, H = 0.52, 0.78
    for k in range(n + 1):
        u = k / n
        xx = x - 0.03 - W * u
        wave = 0.05 * math.sin(math.pi * u * 1.6) * u
        z1 = zt - 0.02
        z0 = z1 - H * (1 - 0.18 * u)                     # a ponta de baixo em bico (o corte do estandarte)
        rings.append([(xx, y + wave - 0.006, z0), (xx, y + wave + 0.006, z0), (xx, y + wave + 0.006, z1), (xx, y + wave - 0.006, z1)])
    p.loft('Team#banner', rings, bevel=0.0, smooth=True)
    p.box('Dark', x - 0.58, x - 0.02, y - 0.012, y + 0.012, zt - 0.06, zt - 0.02, bevel=0.0)          # a bainha de cima


# ---------------------------------------------------------------------------------- canos
def barrel():
    p = Part(PREFIX + '_Barrel')
    # (31/10, o detalhe AAA) cada cano num perfil girado (as transições cônicas, o evacuador abaulado, a luva térmica com as
    # braçadeiras), a faixa da cor do jogador em relevo, e o freio de boca de dois defletores com as quinas redondas, as janelas
    # escuras dos lados e o furo
    for s in (-1, 1):
        y = s * GUN_Y
        st = [(HOUSING_X - 0.05, 0.215), (HOUSING_X + 0.3, 0.212), (HOUSING_X + 0.34, 0.198), (4.66, 0.192),
              (4.74, 0.2), (4.86, 0.258), (5.48, 0.256), (5.6, 0.198),
              (5.66, 0.186), (7.46, 0.176), (7.56, 0.176)]
        vp.gun_barrel(p, st, GUN_Z, y=y, m='CnTankPaint', seg=32, bore=0.0, bore_depth=0.0, bore_m=None)
        for x0, x1, r in ((4.66, 4.76, 0.196), (5.56, 5.66, 0.196), (6.98, 7.04, 0.18), (7.42, 7.48, 0.177)):
            vp.ring_band(p, 'CnTankMetal', x0, x1, r, GUN_Z, y=y, lip=0.01, seg=32)
            vp.rbox(p, 'CnTankMetal', ((x0 + x1) / 2, y, GUN_Z - r - 0.016), (0.07, 0.07, 0.035), r=0.015, k=1)
        vp.ring_band(p, 'Team', 5.98, 6.52, 0.183, GUN_Z, y=y, lip=0.008, seg=32)        # a faixa (cor do jogador)
        # o freio de boca quadrado: o bloco de quinas redondas, as 2 janelas de cada lado, o furo
        bx0, bx1 = 7.5, MUZZLE_X
        vp.prism(p, 'CnTankMetal#brake', vp.rounded_rect(0.5, 0.44, 0.08, 3), Vector((bx0, y, GUN_Z)), Vector((0, 1, 0)), Vector((0, 0, 1)),
                 Vector((1, 0, 0)), bx1 - bx0, bevel=0.012)
        for (a_, b_) in ((bx0 + 0.09, bx0 + 0.25), (bx0 + 0.36, bx0 + 0.52)):
            p.box('Dark', a_, b_, y - 0.255, y + 0.255, GUN_Z - 0.14, GUN_Z + 0.14, bevel=0.0)
        vp.lathe(p, 'Dark', (bx1, y, GUN_Z), 'x', [(0.0, 0.004), (0.11, 0.004), (0.11, -0.2), (0.0, -0.2)], seg=20)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            road_wheel(p, x, y, WHEEL_Z, WHEEL_R, 0.82, s, seg=22)
        idler(p, IDLER[0], y, IDLER[1], IDLER[2], 0.76, s)
        sprocket(p, SPROCKET[0], y, SPROCKET[1], SPROCKET[2], 0.78, s, teeth=14)
        for x in ROLLERS_X:
            roller(p, x, y, ROLLER_Z, ROLLER_R, 0.5)
    return p.finish()


def circles():
    return [(IDLER[0], IDLER[1], IDLER[2] + 0.055), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.055)] + \
           [(x, WHEEL_Z, WHEEL_R + 0.055) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + 0.055) for x in ROLLERS_X]


def tracks(side):
    s = 1 if side == 'r' else -1
    return track(f'{PREFIX}_Track_{side}', circles(), TRACK_Y * s, TRACK_W, pitch=0.21, thick=0.06, bevel=0.0, corner_k=1)


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
    (tx0, tx1), (ty0, ty1), (_, tz1) = bounds([t])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f), largura %.2f m, altura %.2f m' % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL hull_m (casco com os modulos, sem os canos): %.2f x %.2f m; x %.2f..%.2f; o deck a %.2f m' % (hx1 - hx0, hy1 - hy0, hx0, hx1, DECK))
    print('MODEL torre: x %.2f..%.2f, largura %.2f m, teto %.2f m (com a cupula/estandarte %.2f m)' % (tx0, tx1, ty1 - ty0, T_Z2, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'bocas [x, y, z]', [[MUZZLE_X, -GUN_Y, GUN_Z], [MUZZLE_X, GUN_Y, GUN_Z]])
    print('MODEL tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    write_preview(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')],
                  extra={'tank_views': False, 'usa_at': [0, 8.0, 0]})


main()
