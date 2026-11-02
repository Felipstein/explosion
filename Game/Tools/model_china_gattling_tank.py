"""O Gattling Tank da China (o antiaéreo e anti-infantaria de canos rotativos) provisório, por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_china_gattling_tank.py -- captures/game/models/china_gattling_tank

Fictício inspirado no PGZ-09 e no Type 95 (os antiaéreos de canos do ELP; docs/ELENCO.md §4.2), não cópia. A palavra: os DOIS
TAMBORES DE 6 CANOS. De cima: o chassi mais leve que o tanque, a torre-caixa larga com os dois tambores rotativos dos lados
(os feixes de canos saindo pra frente) e o prato do radar de busca girando no alto, atrás. Da família do Type 99: a torre
facetada com as bochechas e a estrela, a camuflagem digital, as lonas dobradas, a gaiola atrás da torre.
Medidas (a régua: o tanque USA, casco 8,1 × 3,8 m, torre a 2,48 m):
  - chassi de 7,0 × 3,3 m (com os para-lamas), deck a 1,48 m; esteira de 0,52 m com 6 rodas de 0,64 m e os 3 roletes à
    mostra (o chassi leve: sem a saia inteira do tanque, só a aba de borracha em cima);
  - o motor na frente à direita (a grelha), o motorista na frente à esquerda (o layout do PGZ-09), a torre atrás do meio;
  - a torre em (-0,45; 0): 3,4 × 2,5 m, teto a 2,52 m; os 2 canhões rotativos de 6 canos nos lados da torre (fora dela, no
    munhão), o eixo a 2,02 m, as bocas em x 2,98 (y ±1,52); o radar de rastreio (o prato redondo fixo) na frente da torre;
  - o radar de busca: o prato curvo de 1,5 m no mastro atrás, girando (peça _Radar);
  - a gaiola de carga atrás da torre, a lona dobrada nela, os lançadores de fumaça, a cúpula, as miras.
Cor do jogador (slot Team): a lona dobrada na gaiola atrás da torre, a lona dobrada no deck de trás, o anel vermelho na boca de
cada tambor (a "faixa no cano" do antiaéreo) e as estrelas nas bochechas. Nunca faixa pintada.

Peças (origem no chão, no centro do casco, frente em +X; todas nas coordenadas do veículo, no lugar delas):
  SM_China_GattlingTank_Hull      casco, para-lamas, deck, grelha do motor, o anel da torre, a lona de trás;
  SM_China_GattlingTank_Turret    a torre (gira em Z em volta de TURRET_PIVOT) COM os dois tambores de canos e o radar fixo;
  SM_China_GattlingTank_Radar     o prato do radar de busca com a cabeça giratória: a ORIGEM no eixo do mastro (RADAR_AT), gira
                                  em Z no próprio eixo; vai em cima da torre (spin "on_turret", como o prato do Avenger);
  SM_China_GattlingTank_Wheels    rodas, roletes, roda-guia, motriz (uma malha);
  SM_China_GattlingTank_Track_l / _Track_r  as esteiras.
Materiais: CnTankPaint, CnTankCamo, CnTankMetal, Track, Rubber, Team, Lens, Dark, Cloth.
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import (antenna, basis, bounds, camo_faces, camo_poly, find_face, frame_box, grille, hatch,  # noqa: E402
                                     headlight, idler, on_face, periscope, plane_point, road_wheel, roller, smoke_launchers,
                                     solid, sprocket, star, sweep_check, tarp_fold, tow_hook, track, tris, write_preview)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_gattling_tank')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_GattlingTank'

# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 3.5, -3.5           # o chassi: 7,0 m
BELLY, DECK = 0.42, 1.48
LOW_HW = 0.98
UP_HW = 1.62
SPON_Z = 1.0
TRACK_Y, TRACK_W = 1.3, 0.52
WHEEL_R = 0.32
WHEEL_Z = WHEEL_R + 0.065
WHEELS_X = [2.35, 1.42, 0.49, -0.44, -1.37, -2.3]
IDLER = (3.02, 0.6, 0.26)
SPROCKET = (-3.04, 0.64, 0.29)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.88, -0.0, -1.84], 0.83, 0.085
GLACIS = ((NOSE, 1.0), (2.45, DECK))
TX = -0.45                       # o eixo da torre
T_Z0, T_Z1, T_Z2 = 1.6, 2.14, 2.52
GUN_Z = 2.02                     # o eixo dos tambores
GUN_Y = 1.52                     # o lado dos tambores (fora da torre)
GUN_X0, GUN_X1 = 0.0, 0.95       # o tambor (a carcaça com o motor)
MUZZLE_X = 2.98
RADAR_AT = (TX - 1.1, 0.0, T_Z2 + 0.42)   # o eixo do radar de busca no alto do mastro (spin at_m)


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
    low = [(3.1, BELLY), (NOSE, 0.92), (NOSE, SPON_Z + 0.02), (REAR, SPON_Z + 0.02), (REAR, 0.7), (-3.22, BELLY)]
    f_low = solid(p, 'CnTankPaint#shell', [(x, s * LOW_HW, z) for x, z in low for s in (-1, 1)])
    up = [(NOSE, SPON_Z), (NOSE, GLACIS[0][1]), (GLACIS[1][0], DECK), (-3.42, DECK), (REAR, 1.42), (REAR, SPON_Z)]
    f_up = solid(p, 'CnTankPaint#shell', [(x, s * UP_HW, z) for x, z in up for s in (-1, 1)])
    camo_faces(p, f_low, seed=21, skip=lambda c, n: abs(n.y) > 0.9 or c.z > SPON_Z)
    camo_faces(p, f_up, seed=21)
    # ---------------------------------------------------------------- os lados: a aba de borracha (o chassi leve deixa a esteira à
    # mostra), as caixas no para-lama, os braços da suspensão
    for s in (-1, 1):
        yo = (UP_HW + 0.005) * s
        y0, y1 = sorted((yo, yo + 0.025 * s))
        p.box('Rubber', -3.3, 3.05, y0, y1, 0.78, SPON_Z + 0.02, bevel=0.0)                 # a aba de borracha
        # (31/10) a régua de aço que prende a aba, com a fileira de parafusos
        ya, yb = sorted(((UP_HW + 0.02) * s, (UP_HW + 0.045) * s))
        p.box('CnTankMetal', -3.3, 3.05, ya, yb, SPON_Z - 0.07, SPON_Z + 0.01, bevel=0.0)
        vp.bolt_row(p, 'CnTankMetal', (-3.2, (UP_HW + 0.045) * s, SPON_Z - 0.03), (2.95, (UP_HW + 0.045) * s, SPON_Z - 0.03), 26,
                    (0, s, 0), r=0.018, h=0.014)
        for x in (-2.2, -1.1, 0.0, 1.1, 2.2):
            p.box('Dark', x - 0.02, x + 0.02, y0 - 0.003 * s, y1 + 0.003 * s, 0.78, SPON_Z + 0.02, bevel=0.0)
        yy0, yy1 = sorted((LOW_HW * s, (LOW_HW + 0.02) * s))
        p.box('Dark', -2.9, 2.9, yy0, yy1, 0.45, SPON_Z - 0.02, bevel=0.0)
        for x in WHEELS_X:
            yy0, yy1 = sorted((LOW_HW * s, (TRACK_Y - TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.08, x + 0.26, yy0, yy1, WHEEL_Z - 0.05, WHEEL_Z + 0.05, bevel=0.0)
        # as caixas presas na parede do casco de cima (a lateral não fica lisa: o chassi leve leva a carga do lado de fora)
        for (x0, x1) in (((1.55, 2.45),) if s < 0 else ()) + ((-0.25, 0.95), (-2.5, -1.15)):   # (à direita na frente: o escape)
            yy0, yy1 = sorted((UP_HW * s, (UP_HW + 0.13) * s))
            p.box('CnTankPaint', x0, x1, yy0, yy1, SPON_Z + 0.1, DECK - 0.17, bevel=0.015)
            # (31/10) a tampa com a aba (a fresta), os trincos de alavanca e a alça
            yy0, yy1 = sorted((UP_HW * s, (UP_HW + 0.15) * s))
            p.box('CnTankPaint', x0 - 0.015, x1 + 0.015, yy0, yy1, DECK - 0.165, DECK - 0.09, bevel=0.015)
            yo = (UP_HW + 0.13) * s
            for xx in (x0 + 0.15, x1 - 0.15):
                vp.rbox(p, 'CnTankMetal', (xx, yo + 0.012 * s, DECK - 0.2), (0.06, 0.024, 0.09), r=0.01, k=1)
            vp.grab_handle(p, 'CnTankMetal', ((x0 + x1) / 2 - 0.12, yo, SPON_Z + 0.2), ((x0 + x1) / 2 + 0.12, yo, SPON_Z + 0.2), (0, s, 0),
                           height=0.05, r=0.014)
        # as caixas de ferramentas no para-lama, atrás (fora do giro dos canhões: baixas)
        vp.ammo_box(p, (-2.82, 1.2 * s, DECK + 0.11), (0.95, 0.4, 0.22), m='CnTankPaint', latch_m='CnTankMetal')
    # ---------------------------------------------------------------- a frente: o motorista à esquerda, a grelha do motor à direita
    hatch(p, 2.05, -0.72, DECK, r=0.28, hinge_side=-1)
    for k, yaw in enumerate((-25, 0, 25)):
        periscope(p, 2.4, -0.72 + (k - 1) * 0.24, DECK - 0.01, yaw=yaw)
    grille(p, 1.3, 2.35, 0.1, 1.42, DECK, n=8)
    p.box('CnTankPaint#thin', 0.95, 1.25, 0.1, 1.42, DECK, DECK + 0.03)
    # o escape do motor (a grelha na lateral direita, na frente)
    p.box('Dark', 1.2, 2.2, UP_HW, UP_HW + 0.012, SPON_Z + 0.12, DECK - 0.06, bevel=0.0)
    for k in range(7):
        p.obox('CnTankMetal', (1.26 + k * 0.15, UP_HW + 0.02, (SPON_Z + DECK) / 2 + 0.03), (0.03, 0.03, 0.04), bevel=0.0)
    # os faróis no glacis, os ganchos, a régua de respingo
    for s in (-1, 1):
        x = 3.2
        z = glacis_z(x)
        p.obox('CnTankPaint', (x, 1.28 * s, z + 0.08), (0.36, 0.38, 0.15), pitch=-24, bevel=0.012)
        headlight(p, (x + 0.12, 1.36 * s, z + 0.1), r=0.065)
        headlight(p, (x + 0.12, 1.18 * s, z + 0.1), r=0.05, guard=False)
        tow_hook(p, NOSE - 0.02, 0.55 * s, 0.86, s=1)
        tow_hook(p, REAR + 0.02, 0.55 * s, 0.9, s=-1)
    p.obox('CnTankPaint#thin', (3.0, 0.0, glacis_z(3.0) + 0.03), (0.07, 1.8, 0.05), pitch=-24, bevel=0.0)
    # (31/10) os elos de reserva presos no glacis (o costume dos blindados do ELP) e o cabo de reboque na traseira
    gn = Vector((0.48, 0.0, 1.05)).normalized()
    vp.spare_links(p, (3.27, 0.0, glacis_z(3.27)), gn, (-1.05, 0.0, 0.48), 3, pitch=0.15, width=0.46, m='Track', guide_m='CnTankMetal')
    vp.tow_cable(p, 'CnTankMetal', [(REAR - 0.05, -0.55, 0.86), (REAR - 0.06, -0.4, 0.74), (REAR - 0.06, 0.4, 0.74),
                                     (REAR - 0.05, 0.55, 0.86)], r=0.02)
    # o anel da torre e as argolas
    p.cyl('Dark', (TX, 0.0, DECK + 0.015), 'z', 1.2, 0.03, seg=48, bevel=0.0)
    p.cyl('CnTankMetal', (TX, 0.0, DECK + 0.005), 'z', 1.26, 0.012, seg=48, bevel=0.0)
    for (x, y) in ((2.85, 1.45), (2.85, -1.45), (-3.25, 1.45), (-3.25, -1.45)):
        p.torus('CnTankMetal', (x, y, DECK + 0.03), 'y', 0.045, 0.012, seg=12, rseg=6)
    # ---------------------------------------------------------------- o deck de trás: a tampa, a lona DOBRADA (cor do jogador)
    p.box('CnTankPaint#thin', -3.3, -2.45, -0.75, 0.75, DECK, DECK + 0.03)
    tarp_fold(p, (-2.85, 0.0, DECK + 0.03), (1.1, 0.62, 0.15), yaw=90, m='Team', straps=2, seed=7.0)
    # a traseira: a porta de acesso, as lanternas, os para-lamas
    p.box('Dark', REAR - 0.02, REAR, -0.55, 0.55, 0.8, 1.32, bevel=0.0)
    p.box('CnTankPaint#thin', REAR - 0.05, REAR, -0.5, 0.5, 0.84, 1.28)
    p.box('CnTankMetal', REAR - 0.08, REAR - 0.04, 0.32, 0.42, 1.0, 1.08, bevel=0.0)
    for s in (-1, 1):
        p.box('Dark', REAR - 0.06, REAR, 1.3 * s - 0.09, 1.3 * s + 0.09, 1.2, 1.32, bevel=0.0)
        p.box('Lens', REAR - 0.07, REAR - 0.05, 1.3 * s - 0.06, 1.3 * s + 0.06, 1.23, 1.29, bevel=0.0)
        p.obox('CnTankPaint', (REAR - 0.02, 1.3 * s, 0.8), (0.04, 0.6, 0.36), pitch=-20, bevel=0.01)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_points():
    pts = []
    for s in (-1, 1):
        pts += [(1.25, 0.62 * s, T_Z0), (0.55, 1.26 * s, T_Z0), (-2.05, 1.22 * s, T_Z0), (-2.2, 0.98 * s, T_Z0)]
        pts += [(1.15, 0.6 * s, T_Z1), (0.5, 1.22 * s, T_Z1), (-2.05, 1.18 * s, T_Z1), (-2.2, 0.94 * s, T_Z1)]
        pts += [(0.75, 0.52 * s, T_Z2), (0.3, 1.06 * s, T_Z2), (-1.98, 1.06 * s, T_Z2), (-2.12, 0.84 * s, T_Z2)]
    return pts


def gattling(p, s):
    """Um canhão rotativo de 6 canos no lado da torre (31/10, o detalhe AAA): o munhão, o tambor torneado com os aros e a tampa do
    motor atrás, a CALHA DE MUNIÇÃO articulada (os gomos) subindo da torre pro tambor, o feixe de 6 canos (cada um com a boca
    furada), a braçadeira do meio com os parafusos e o anel da boca (cor do jogador)."""
    y = GUN_Y * s
    yy0, yy1 = sorted((1.0 * s, (GUN_Y - 0.12) * s))
    p.box('CnTankPaint', 0.05, 0.75, yy0, yy1, GUN_Z - 0.2, GUN_Z + 0.2, bevel=0.02)
    vp.lathe(p, 'CnTankMetal', (0.4, y - 0.05 * s, GUN_Z), (0, s, 0), [(0.0, 0.2), (0.12, 0.2), (0.16, 0.17), (0.16, -0.17), (0.0, -0.17)],
             seg=20)
    # a calha de munição articulada (da parede da torre até a barriga do tambor)
    vp.chute(p, 'Dark', vp.bent_bar([(0.12, 1.0 * s, GUN_Z - 0.28), (0.18, 1.18 * s, GUN_Z - 0.46), (0.32, 1.42 * s, GUN_Z - 0.4),
                                     (0.36, (GUN_Y - 0.02) * s, GUN_Z - 0.24)], R=0.1, n=4), 0.15, 0.07, pitch=0.085)
    # o tambor: a tampa do motor atrás, a carcaça com os 2 aros, o colar da frente
    vp.lathe(p, 'CnTankMetal', (GUN_X0, y, GUN_Z), 'x', [(0.0, -0.26), (0.15, -0.26), (0.19, -0.23), (0.235, -0.1), (0.25, 0.0),
                                                       (0.0, 0.0)], seg=24)
    vp.lathe(p, 'CnTankPaint', (GUN_X0, y, GUN_Z), 'x', [(0.0, 0.0), (0.262, 0.0), (0.272, 0.02), (0.272, GUN_X1 - GUN_X0 - 0.02),
                                                       (0.262, GUN_X1 - GUN_X0), (0.0, GUN_X1 - GUN_X0)], seg=28)
    for xa in (GUN_X0 + 0.22, GUN_X0 + 0.68):
        vp.ring_band(p, 'CnTankMetal', xa, xa + 0.06, 0.272, GUN_Z, y=y, lip=0.014, seg=28)
    vp.lathe(p, 'CnTankMetal', (GUN_X1, y, GUN_Z), 'x', [(0.0, 0.0), (0.235, 0.0), (0.235, 0.05), (0.2, 0.12), (0.0, 0.12)], seg=28)
    # o feixe de 6 canos: cada um com a boca mais grossa e o furo
    L = MUZZLE_X - GUN_X1 - 0.1
    for k in range(6):
        a = math.radians(k * 60 + 30)
        cy, cz = y + 0.115 * math.cos(a), GUN_Z + 0.115 * math.sin(a)
        vp.lathe(p, 'Dark', (GUN_X1 + 0.1, cy, cz), 'x', [(0.0, 0.0), (0.036, 0.0), (0.033, 0.1), (0.032, L - 0.09), (0.04, L - 0.08),
                                                         (0.04, L), (0.019, L), (0.019, L - 0.06), (0.0, L - 0.06)], seg=10)
    p.cyl('CnTankMetal', ((GUN_X1 + MUZZLE_X) / 2, y, GUN_Z), 'x', 0.05, MUZZLE_X - GUN_X1 - 0.1, seg=12, bevel=0.0)
    vp.lathe(p, 'CnTankMetal', (1.9, y, GUN_Z), 'x', [(0.0, -0.035), (0.158, -0.035), (0.168, -0.025), (0.168, 0.025), (0.158, 0.035),
                                                    (0.0, 0.035)], seg=24)
    vp.bolt_ring(p, 'CnTankMetal', (1.935, y, GUN_Z), 'x', 0.14, 6, r=0.016, h=0.014, phase=0.0)
    vp.lathe(p, 'Team', (MUZZLE_X - 0.2, y, GUN_Z), 'x', [(0.0, -0.07), (0.165, -0.07), (0.175, -0.055), (0.175, 0.055), (0.165, 0.07),
                                                         (0.0, 0.07)], seg=24)     # o anel da boca (cor do jogador)


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('CnTankPaint#shell', 0.065)
    p.bm('CnTankPaint', 0.02)
    p.bm('CnTankPaint#thin', 0.006)
    p.bm('CnTankCamo', 0.0)
    p.bm('Team', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.0)
    faces = solid(p, 'CnTankPaint#shell', turret_points())
    p.cyl('Dark', (TX, 0.0, (DECK + 0.03 + T_Z0) / 2), 'z', 1.16, T_Z0 - DECK - 0.035, seg=48, bevel=0.0)
    # as bochechas com a estrela
    stars = []
    for s in (-1, 1):
        q, n = find_face(faces, (0.68, 0.73 * s, 0.05), near=(0.9, 0.95 * s, 1.8))
        stars.append((q, n, sum(q, Vector()) / len(q)))
    centers = [c for _, _, c in stars]
    camo_faces(p, faces, seed=22, skip=lambda c, n: any((c - sc).length < 0.02 for sc in centers))
    for q, n, c in stars:
        ex, ey, _ = basis(n)
        r = 0.18
        camo_poly(p, q, n, seed=22, holes=[(c.dot(ex) - r, c.dot(ex) + r, c.dot(ey) - r, c.dot(ey) + r)])
        star(p, c + Vector((0, 0, 0.04)), n, r)
    # os dois canhões rotativos
    for s in (-1, 1):
        gattling(p, s)
    # o radar de rastreio na frente (o prato redondo fixo no nariz da torre, com a moldura e o alimentador)
    nf = find_face(faces, (1, 0, 0.1), near=(1.2, 0, 1.8))
    c = plane_point(nf, y=0.0, z=1.86)
    on_face(p, 'CnTankPaint', nf, c, (0.86, 0.62, 0.1), bevel=0.02)
    p.cyl('Dark', (1.36, 0.0, 1.86), 'x', 0.27, 0.03, seg=28, bevel=0.0)
    p.cyl('CnTankMetal', (1.38, 0.0, 1.86), 'x', 0.25, 0.05, seg=28, r2=0.16, bevel=0.0)
    p.cyl('Dark', (1.5, 0.0, 1.86), 'x', 0.04, 0.2, seg=10, bevel=0.0)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    # a mira eletro-óptica (a caixa com as janelas, à frente, à direita) e a cúpula do comandante (à esquerda)
    p.box('CnTankPaint', 0.0, 0.5, 0.25, 0.75, top, top + 0.34, bevel=0.02)
    p.box('Dark', 0.5, 0.52, 0.3, 0.7, top + 0.08, top + 0.28, bevel=0.0)
    p.box('Lens', 0.515, 0.53, 0.34, 0.5, top + 0.11, top + 0.25, bevel=0.0)
    p.box('Lens', 0.515, 0.53, 0.55, 0.66, top + 0.15, top + 0.25, bevel=0.0)
    p.box('CnTankPaint#thin', 0.15, 0.58, 0.23, 0.77, top + 0.34, top + 0.37)
    cx, cy = -0.55, -0.6
    vp.lathe(p, 'CnTankPaint', (cx, cy, top), 'z', [(0.0, 0.0), (0.34, 0.0), (0.34, 0.03), (0.325, 0.042), (0.31, 0.15), (0.295, 0.165),
                                                   (0.25, 0.175), (0.24, 0.195), (0.2, 0.22), (0.1, 0.232), (0.0, 0.235)], seg=28)
    for k in range(5):
        a = math.radians(k * 50 - 100)
        ca, sa = math.cos(a), math.sin(a)
        p.obox('Dark', (cx + 0.31 * ca, cy + 0.31 * sa, top + 0.095), (0.04, 0.13, 0.055), yaw=k * 50 - 100, bevel=0.0)
        vp.rbox(p, 'CnTankPaint', (cx + 0.32 * ca, cy + 0.32 * sa, top + 0.14), (0.07, 0.17, 0.022), yaw=k * 50 - 100, r=0.018, k=2)
    for yy in (-0.2, 0.25):                                                       # (31/10) as alças do teto
        vp.grab_handle(p, 'CnTankMetal', (0.85 - 0.1, yy, top), (0.85 + 0.12, yy, top), (0, 0, 1), height=0.06, r=0.015)
    hatch(p, -0.6, 0.62, top, r=0.26, hinge_side=1)
    periscope(p, 0.1, -0.45, top, yaw=0)
    # o mastro do radar de busca (fixo; o prato e a cabeça giram na peça _Radar) e a base
    rx, ry, rz = RADAR_AT
    p.cyl('CnTankPaint', (rx, ry, top + 0.06), 'z', 0.3, 0.12, seg=24, bevel=0.01)
    p.cyl('CnTankMetal', (rx, ry, (top + 0.12 + rz - 0.005) / 2), 'z', 0.11, rz - 0.005 - top - 0.12, seg=16, bevel=0.0)
    p.cyl('Dark', (rx, ry, rz - 0.025), 'z', 0.15, 0.04, seg=20, bevel=0.0)
    # a antena e os lançadores de fumaça
    antenna(p, -1.85, 0.85, top, 1.5)
    for s in (-1, 1):
        smoke_launchers(p, (0.6, 0.86 * s, top + 0.1), yaw=30 * s, rows=2, cols=2, r=0.042, L=0.3)
    # ---------------------------------------------------------------- a gaiola de carga atrás (as barras) com a lona e a caixa
    gx0, gx1, gy = -2.02, -2.75, 1.08
    z0, z1 = 1.74, top
    for z in (z0, z1):
        vp.tube(p, 'CnTankMetal', vp.bent_bar([(gx0, -gy, z), (gx1, -gy, z), (gx1, gy, z), (gx0, gy, z)], R=0.1, n=4), 0.022, seg=8)
    for k in range(13):
        y = -gy + 2 * gy * k / 12
        bar(p, (gx1, y, z0), (gx1, y, z1), r=0.014)
    for s in (-1, 1):
        for k in range(1, 5):
            x = gx0 + (gx1 - gx0) * k / 5
            bar(p, (x, gy * s, z0), (x, gy * s, z1), r=0.014)
    for k in range(7):
        y = -gy + 2 * gy * k / 6
        bar(p, (gx0, y, z0), (gx1, y, z0), r=0.018)
    for x in (-2.35, -2.6):
        bar(p, (x, -gy, z1), (x, gy, z1), r=0.02)
    vp.ammo_box(p, (-2.45, -0.72, z0 + 0.19), (0.55, 0.5, 0.34), yaw=90, m='CnTankPaint', latch_m='CnTankMetal')
    vp.jerrycan(p, (-2.45, -0.15, z0 + 0.255), yaw=90, m='CnTankCamo', cap_m='CnTankMetal')
    tarp_fold(p, (-2.33, 0.25, top), (0.62, 1.0, 0.18), m='Team', straps=2, seed=8.0)
    for yy in (0.0, 0.5):                                                          # as cordas do fardo até o corrimão
        vp.rope(p, 'Cloth', [(-2.62, yy, top + 0.07), (-2.7, yy + 0.03, top + 0.03), (-2.75, yy + 0.05, z1 + 0.0)], r=0.011, sag=0.015)
    return p.finish()


# ---------------------------------------------------------------------------------- radar de busca (gira no próprio eixo)
def radar():
    """O prato do radar de busca: a cabeça giratória no topo do mastro, os 2 braços e o refletor curvo (um pedaço de
    paraboloide, mais largo que alto, 12° pra cima) com o alimentador no braço da frente. Feito no lugar e levado pra origem no
    eixo (RADAR_AT): o jogo gira em Z em volta dele."""
    p = Part(PREFIX + '_Radar')
    p.bm('CnTankPaint', 0.012)
    p.bm('CnTankPaint#dish', 0.0)
    p.bm('Dark', 0.0)
    p.bm('CnTankMetal', 0.0)
    rx, ry, rz = RADAR_AT
    p.cyl('CnTankMetal', (rx, ry, rz + 0.07), 'z', 0.16, 0.11, seg=20)
    p.box('CnTankPaint', rx - 0.18, rx + 0.12, ry - 0.2, ry + 0.2, rz + 0.12, rz + 0.36)          # a cabeça (o transmissor)
    # o refletor: a face côncava pra frente (+X), 1,5 × 0,55 m, a borda em x = rx + 0.04
    W, H, depth = 1.5, 0.55, 0.16
    tilt = math.radians(12)
    cz = rz + 0.42
    rings = []
    nu, nv = 14, 6
    for i in range(nu + 1):
        u = (i / nu - 0.5) * W
        ring = []
        for j in range(nv + 1):
            v = (j / nv - 0.5) * H
            x = rx - 0.02 - depth * ((2 * u / W) ** 2 * 0.75 + (2 * v / H) ** 2 * 0.25)
            z = cz + v
            # a inclinação pra cima (gira em volta do eixo y na altura cz)
            dx, dz = x - rx, z - cz
            x2 = rx + dx * math.cos(tilt) - dz * math.sin(tilt)
            z2 = cz + dx * math.sin(tilt) + dz * math.cos(tilt)
            ring.append((x2, ry + u, z2))
        rings.append(ring)
    b = p.bm('CnTankPaint#dish', 0.0)
    top = [[b.verts.new(V(*q)) for q in r] for r in rings]
    bot = [[b.verts.new(V(q[0] - 0.035 * math.cos(tilt), q[1], q[2] - 0.035 * math.sin(tilt))) for q in r] for r in rings]
    for i in range(nu):
        for j in range(nv):
            f = b.faces.new([top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]])
            f.smooth = True
            f = b.faces.new([bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j], bot[i][j]])
            f.smooth = True
    edge = [(i, 0) for i in range(nu)] + [(nu, j) for j in range(nv)] + [(i, nv) for i in range(nu, 0, -1)] + [(0, j) for j in range(nv, 0, -1)]
    for (i0, j0), (i1, j1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([top[i0][j0], bot[i0][j0], bot[i1][j1], top[i1][j1]])
    # as nervuras de trás do refletor e os braços até a cabeça
    for u in (-0.45, 0.0, 0.45):
        p.obox('CnTankMetal', (rx - 0.16, ry + u, cz - 0.02), (0.06, 0.05, H * 0.9), pitch=-12)
    p.box('CnTankMetal', rx - 0.2, rx - 0.12, ry - 0.62, ry + 0.62, cz - 0.04, cz + 0.02)
    p.box('CnTankMetal', rx - 0.16, rx - 0.08, ry - 0.05, ry + 0.05, rz + 0.34, cz)
    # o alimentador: o braço pra frente e a corneta no foco
    p.cyl('CnTankMetal', (rx + 0.2, ry, cz + 0.03), (1.0, 0.0, 0.2), 0.018, 0.45, seg=8)
    p.cyl('Dark', (rx + 0.42, ry, cz + 0.08), (1.0, 0.0, 0.2), 0.05, 0.1, seg=10, r2=0.07)
    o = p.finish()
    o.data.transform(Matrix.Translation(-V(*RADAR_AT)))                 # a origem no eixo
    return o


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            road_wheel(p, x, y, WHEEL_Z, WHEEL_R, 0.46, s)
        idler(p, IDLER[0], y, IDLER[1], IDLER[2], 0.42, s)
        sprocket(p, SPROCKET[0], y, SPROCKET[1], SPROCKET[2], 0.44, s, teeth=12)
        for x in ROLLERS_X:
            roller(p, x, y, ROLLER_Z, ROLLER_R, 0.3)
    return p.finish()


def circles():
    return [(IDLER[0], IDLER[1], IDLER[2] + 0.04), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.04)] + \
           [(x, WHEEL_Z, WHEEL_R + 0.04) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + 0.04) for x in ROLLERS_X]


def tracks(side):
    s = 1 if side == 'r' else -1
    zt = ROLLER_Z + ROLLER_R + 0.04
    sags = [(ROLLERS_X[1] + 0.12, ROLLERS_X[0] - 0.12, zt, 0.035), (ROLLERS_X[2] + 0.12, ROLLERS_X[1] - 0.12, zt, 0.035),
            (ROLLERS_X[0] + 0.12, IDLER[0] - 0.3, zt, 0.02), (SPROCKET[0] + 0.32, ROLLERS_X[2] - 0.12, zt, 0.02)]
    return track(f'{PREFIX}_Track_{side}', circles(), TRACK_Y * s, TRACK_W, pitch=0.15, thick=0.045, bevel=0.005, sags=sags)


# ---------------------------------------------------------------------------------- exportação
def main():
    clear_scene()
    h, t, r, w = hull(), turret(), radar(), wheels()
    tl, tr = tracks('l'), tracks('r')
    sweep_check([h, w], [t], (TX, 0.0))
    # o radar girando no próprio eixo não pode bater na torre (o mastro fica): a peça volta pro lugar pra conferir
    r.data.transform(Matrix.Translation(V(*RADAR_AT)))
    sweep_check([t], [r], (RADAR_AT[0], RADAR_AT[1]), label='radar')
    r.data.transform(Matrix.Translation(-V(*RADAR_AT)))
    # (31/10) com o executor do detalhe (model_run_detail.py) as máscaras são cozidas depois do script, com as peças onde estão: o radar
    # fica NO LUGAR (em cima do mastro) pro cozimento e só vai pra origem do eixo na hora de exportar (antes ele era cozido na origem,
    # dentro da barriga do casco, e saía com a oclusão errada)
    import model_lib
    if model_lib.DETAIL.get('defer') is not None:
        r.data.transform(Matrix.Translation(V(*RADAR_AT)))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o is r and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-V(*RADAR_AT)))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = 0
    for o in (h, t, r, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (_, _), (_, _), (_, rz1) = bounds([r])
    print('MODEL medidas: comprimento total %.2f m (x %.2f..%.2f), largura %.2f m (com os tambores), altura %.2f m' % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    print('MODEL hull_m (chassi): %.2f x %.2f m; x %.2f..%.2f; o deck a %.2f m' % (hx1 - hx0, hy1 - hy0, hx0, hx1, DECK))
    print('MODEL teto da torre %.2f m; o radar chega a %.2f m' % (T_Z2, rz1 if model_lib.DETAIL.get('defer') is not None else RADAR_AT[2] + rz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'bocas [x, y, z]', [[MUZZLE_X, -GUN_Y, GUN_Z], [MUZZLE_X, GUN_Y, GUN_Z]])
    print('MODEL radar spin at_m', [round(v, 3) for v in RADAR_AT], 'on_turret true')
    print('MODEL tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, r, w, tl, tr)})
    write_preview(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Wheels', '_Track_l', '_Track_r')] +
                  [(PREFIX + '_Radar', (RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]))],
                  extra={'tank_views': False, 'usa_at': [0, 6.5, 0]})


main()
