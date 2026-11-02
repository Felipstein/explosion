"""O Strategy Center da USA (usa_strategy_center), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_strategy_center.py, 22/10) fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_strategy_center_v2.py C:/Users/luisf/explosion/captures/game/models/usa_strategy_center_v2

O mesmo desenho do A (a palavra é A ANTENA DE SATÉLITE: o prato branco de 8,4 m girando devagar em cima do bunker octogonal blindado;
lote 20 × 20 m, o portão em +X) e as mesmas medidas, refeito peça por peça na linguagem dos prédios da USA (a ficha da War Factory e o
prédio com o prato no teto do game shot r1_calm_slabs; docs/ESTUDO_DETALHE_AAA.md):
  - o bunker: a saia inclinada coberta de CHAPAS de blindagem (3 por face), a parede de PLACAS com a junta funda e a FAIXA AZUL feita
    de placas (lê de cima e de lado), os pilares redondos nos 8 cantos, o parapeito com a capa, as luminárias com o corpo;
  - o portão blindado: os pilares de placas com a base de perigo, a verga com a placa azul e os holofotes, as 2 folhas da porta com as
    nervuras e o trilho, a soleira de perigo, os frades;
  - o teto: o tambor torneado do giro com as nervuras e a porta, a caixa de ventiladores (a assinatura do teto da USA), o armário de
    rádio com as antenas e a sirene, as 2 cargas com a LONA azul amarrada (o pano que cai), as balizas;
  - o gerador de placas (veneziana, escapamento com o chapéu, a faixa azul) e o conduíte até o bunker; o tanque de combustível
    deitado (model_usa_building_parts.h_tank); o mastro da bandeira torneado (o pano no vento é do jogo: flags.json); o poste de luz;
  - o prato: a casca parabólica (o A) com o aro, as 8 costelas, o anel de treliça atrás, o cubo, o subrefletor nos 4 braços e a
    corneta; a mesa giratória torneada com os parafusos, a casa do azimute de placas com a porta e a faixa azul, o garfo de placa
    grossa com os munhões, o eixo, o macaco de elevação (o corpo amarelo e a haste) e o contrapeso.
A USA limpa (13/10): sem desgaste pintado.

Pivôs MANTIDOS: o prato gira em Z na origem da malha dele, DISH_AT = (0; 0; 5,8) (o eixo em cima do tambor, como no A). ATENÇÃO: o
Units.json (usa_strategy_center spin at_m) diz (0; 0; 9,0), não 5,8: com a malha do A ou do B o prato fica 3,2 m acima do tambor no
jogo. O número certo pra esta malha é 5,8 (o mesmo DISH_AT do A); não mexi no Units.json.
Peças (os NOMES do A): SM_USA_StrategyCenter_Base, SM_USA_StrategyCenter_Dish (origem no eixo), SM_USA_StrategyCenter_Scaffold.
Slots (os do A): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_strategy_center_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_StrategyCenter'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62)})

# ---------------------------------------------------------------------------------- as medidas do A (mantidas)
HX = HY = 10.0                   # o lote 20 × 20
SLAB_A = 8.8                     # a laje octogonal (apótema)
GLACIS_A, WALL_A = 7.2, 6.5      # o bunker: a saia inclinada no chão e a parede reta
Z0 = 0.14                        # o topo da laje
GLACIS_Z, WALL_Z = 1.5, 4.35     # a saia vai até 1,5 m; a parede até 4,35 m
ROOF_Z = 4.6                     # o piso do teto (dentro do parapeito)
DISH_AT = (0.0, 0.0, 5.8)        # o eixo do giro, em cima da base redonda do teto
OCT = 22.5                       # os cantos a 22,5° + k·45°: a face 0 olha pra +X (o portão)
EL = 55.0
AX = Vector((0.3, 0.0, 2.1))
R_DISH, F_DISH, OFF, THICK = 4.2, 3.2, 0.7, 0.12
N_DISH = Vector((math.cos(math.radians(EL)), 0.0, math.sin(math.radians(EL))))
U_DISH = Vector((-math.sin(math.radians(EL)), 0.0, math.cos(math.radians(EL))))
Y_DISH = Vector((0.0, 1.0, 0.0))
PX0, PX1 = WALL_A - 0.3, 8.6     # o portão (x)

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def corner_r(a):
    return a / math.cos(math.radians(OCT))


def octa_ring(a, z):
    r = corner_r(a)
    return [(r * math.cos(math.radians(OCT + 45 * k)), r * math.sin(math.radians(OCT + 45 * k)), z) for k in range(8)]


def octa(p, m, sections, bottom=True):
    p.loft(m, [octa_ring(a, z) for a, z in sections], caps=(bottom, True))


def octa_band(p, m, a_out, a_in, z0, z1):
    """Um anel octogonal (o parapeito): as faces de fora, de cima, de dentro e de baixo."""
    b = p.bm(m, ub._bev(m))
    rings = [[b.verts.new(V(*q)) for q in octa_ring(a, z)] for a, z in ((a_out, z0), (a_out, z1), (a_in, z1), (a_in, z0))]
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(8):
            b.faces.new([ra[k], ra[(k + 1) % 8], rb[(k + 1) % 8], rb[k]])


def face_frame(k, a):
    """A face k (normal a 45°·k) do octógono de apótema a: (o centro no chão, eu ao longo, en pra fora, a largura)."""
    th = math.radians(45 * k)
    en = Vector((math.cos(th), math.sin(th), 0.0))
    eu = Vector((-math.sin(th), math.cos(th), 0.0))
    return en * a, eu, en, 2 * a * math.tan(math.radians(OCT))


# ---------------------------------------------------------------------------------- o bunker
def bunker(p):
    octa(p, 'Concrete#pnl', [(SLAB_A, -0.12), (SLAB_A, Z0 - 0.06), (SLAB_A - 0.08, Z0)], bottom=False)
    # o miolo: a saia inclinada e a parede reta (as chapas e as placas vão por cima)
    octa(p, 'Body#pnl', [(GLACIS_A, Z0 - 0.05), (WALL_A, GLACIS_Z), (WALL_A, WALL_Z)])
    # a saia: 3 chapas de blindagem por face, no plano inclinado, com os parafusos na borda de cima
    run = GLACIS_A - WALL_A
    rise = GLACIS_Z - Z0
    Ls = math.hypot(run, rise)
    for k in range(8):
        c0, eu, en, w = face_frame(k, (GLACIS_A + WALL_A) / 2)
        ev = (Vector((0, 0, rise)) - en * run).normalized()
        nn = eu.cross(ev).normalized()
        if nn.dot(en) < 0:
            nn = -nn
        mid = c0 + Vector((0, 0, (Z0 + GLACIS_Z) / 2))
        wt = 2 * WALL_A * math.tan(math.radians(OCT)) - 0.4
        pw = wt / 3
        if k == 0:
            continue                                        # a face do portão: os pilares cobrem a saia
        for i in range(3):
            q = mid + eu * (-wt / 2 + pw * (i + 0.5)) + nn * 0.02
            ub.obox(p, 'Body#pnl', q, (pw - 0.08, Ls - 0.16, 0.12), eu, ev, nn)
            for t in (-0.32, 0.0, 0.32):
                vp.nut(p, 'Dark', q + eu * (t * pw) + ev * (Ls / 2 - 0.2) + nn * 0.08, nn, r=0.03, h=0.02)
    # a parede: as placas em 4 fileiras, a do meio-alto em azul (a faixa do jogador feita de placas)
    rows = [0.55, 1.65, 2.3]
    for k in range(1, 8):                                   # (a face 0 fica atrás do portão)
        c0, eu, en, w = face_frame(k, WALL_A)
        org = c0 - eu * (w / 2 - 0.3) + Vector((0, 0, GLACIS_Z))
        ub.panel_wall(p, org, eu, Z, en, w - 0.6, WALL_Z - GLACIS_Z, pw=(w - 0.6) / 4, m='Body#pnl', gap=0.07, proud=0.08, embed=0.04,
                      rows=rows, row_mats={2: 'Player#pnl'})
    # os pilares redondos nos 8 cantos (o ritmo de pé da War Factory)
    for k in range(8):
        a = math.radians(OCT + 45 * k)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        q = d * (corner_r(WALL_A) - 0.08)
        vp.lathe(p, 'Body#vp', (q.x, q.y, GLACIS_Z - 0.3), 'z', [(0.0, 0.0), (0.36, 0.0), (0.36, 0.1), (0.3, 0.16), (0.3, WALL_Z - GLACIS_Z + 0.2),
                                                                (0.34, WALL_Z - GLACIS_Z + 0.26), (0.34, WALL_Z - GLACIS_Z + 0.4),
                                                                (0.0, WALL_Z - GLACIS_Z + 0.4)], seg=16)
    # o parapeito com a capa e o piso do teto
    octa_band(p, 'Body#pnl', WALL_A + 0.12, WALL_A - 0.24, WALL_Z - 0.1, ROOF_Z + 0.34)
    octa_band(p, 'Trim#sm', WALL_A + 0.18, WALL_A - 0.3, ROOF_Z + 0.34, ROOF_Z + 0.44)
    octa(p, 'Concrete#pnl', [(WALL_A - 0.25, WALL_Z - 0.1), (WALL_A - 0.25, ROOF_Z)])
    # as luminárias com o corpo (faces dos lados)
    for k in (2, 6):
        c0, eu, en, w = face_frame(k, WALL_A)
        q = c0 + Vector((0, 0, 2.75))
        ub.obox(p, 'Dark#sm', q + en * 0.15, (0.6, 0.18, 0.22), eu, en, Z)
        ub.obox(p, 'Light#vp', q + en * 0.25 - Vector((0, 0, 0.05)), (0.5, 0.012, 0.1), eu, en, Z)
        ub.obox(p, 'Dark#sm', q + en * 0.26 + Vector((0, 0, 0.13)), (0.64, 0.2, 0.025), eu, en, Z)


def gate(p):
    """O portão blindado (+X): os pilares de placas com a base de perigo, a verga, a porta de 2 folhas recuada com as nervuras e o
    trilho, a soleira, os holofotes, a placa azul, os frades."""
    for s in (-1, 1):
        yc = 2.4 * s
        ub.obox(p, 'Body#pnl', ((PX0 + PX1) / 2, yc, (Z0 + 4.0) / 2), (PX1 - PX0, 1.0, 4.0 - Z0), X, Y, Z)
        ub.panel_wall(p, (PX1, yc - 0.5 * s, Z0 + 1.15), (0, s, 0), Z, X, 1.0, 2.7, pw=1.0, ph=0.9, m='Body#pnl', gap=0.06, proud=0.06,
                      embed=0.03)
        ub.obox(p, 'Hazard#sm', (PX1 + 0.02, yc, Z0 + 0.55), (0.08, 1.04, 1.0), X, Y, Z)
        ub.obox(p, 'Hazard#sm', ((PX0 + PX1) / 2, yc - 0.52 * s, Z0 + 0.55), (PX1 - PX0 - 0.2, 0.06, 1.0), X, Y, Z)
    ub.obox(p, 'Body#pnl', ((PX0 + PX1) / 2, 0.0, 3.55), (PX1 - PX0, 3.8, 0.9), X, Y, Z)
    ub.obox(p, 'Trim#sm', ((PX0 + PX1) / 2 + 0.05, 0.0, 4.09), (PX1 - PX0 + 0.2, 6.0, 0.18), X, Y, Z)
    ub.obox(p, 'Player#sm', (PX1 + 0.035, 0.0, 3.55), (0.04, 1.8, 0.54), X, Y, Z)
    for y in (-1.45, 1.45):
        ub.lamp_head(p, (PX1 + 0.18, y, 3.3), (1.0, 0.0, -0.8), w=0.55, h=0.3)
        ub.obox(p, 'Dark#sm', (PX1 + 0.06, y, 3.3), (0.12, 0.1, 0.2), X, Y, Z)
    # a porta recuada (o vão escuro em volta) e as 2 folhas com as nervuras
    ub.obox(p, 'Dark#vp', (7.82, 0.0, (Z0 + 3.1) / 2), (0.1, 3.8, 3.1 - Z0), X, Y, Z)
    # (as folhas de metal escuro: na câmera do jogo a porta clara não lia como porta; o A era o vão preto)
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', (7.9, 0.94 * s, (Z0 + 3.02) / 2), (0.1, 1.84, 2.88), X, Y, Z)
        for k in range(5):
            ub.obox(p, 'Dark#sm', (7.97, 0.94 * s, Z0 + 0.45 + k * 0.52), (0.05, 1.7, 0.12), X, Y, Z)
        vp.grab_handle(p, 'Trim', (7.99, 0.12 * s, 1.2), (7.99, 0.12 * s, 1.75), (1, 0, 0), height=0.06, r=0.018)
    ub.pipe(p, 'Trim', [(7.98, -1.9, 3.06), (7.98, 1.9, 3.06)], 0.05)
    ub.obox(p, 'Hazard#sm', ((7.85 + PX1) / 2 + 0.05, 0.0, Z0 + 0.012), (PX1 - 7.85 + 0.2, 3.7, 0.03), X, Y, Z)
    for y in (-3.4, 3.4):                                  # os frades na frente dos pilares
        vp.lathe(p, 'Hazard#vp', (9.35, y, Z0), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.85), (0.12, 0.92), (0.0, 0.94)], seg=14)


def roof(p):
    # o piso do teto em lajes: as 8 juntas radiais e o anel em volta do trilho (o piso liso lia como uma placa só, clara)
    for k in range(8):
        a = math.radians(45 * k + OCT)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        r0, r1 = 3.3, corner_r(WALL_A - 0.25) - 0.1
        ub.obox(p, 'Dark#vp', d * ((r0 + r1) / 2) + Vector((0, 0, ROOF_Z + 0.004)), (r1 - r0, 0.07, 0.012), d, Vector((-d.y, d.x, 0)), Z)
    vp.lathe(p, 'Dark#vp', (0, 0, ROOF_Z), 'z', [(4.6, 0.0), (4.68, 0.0), (4.68, 0.012), (4.6, 0.012)], seg=48, closed=True)
    # a escotilha do teto (a subida pela escada de dentro) com a tampa, a dobradiça e o corrimão de saída
    hx_, hy_ = -5.3, 0.0
    ub.obox(p, 'Body#pnl', (hx_, hy_, ROOF_Z + 0.2), (1.0, 1.0, 0.4), X, Y, Z)
    ub.obox(p, 'Body#sm', (hx_ + 0.03, hy_, ROOF_Z + 0.43), (0.9, 0.92, 0.06), X, Y, Z)
    vp.hinge(p, 'Trim', (hx_ - 0.44, hy_, ROOF_Z + 0.44), 'y', 0.7, r=0.025, knuckles=3)
    vp.tube(p, 'Trim', vp.bent_bar([Vector((hx_ + 0.5, hy_ - 0.42, ROOF_Z + 0.4)), Vector((hx_ + 0.5, hy_ - 0.42, ROOF_Z + 1.4)),
                                    Vector((hx_ + 0.5, hy_ + 0.42, ROOF_Z + 1.4)), Vector((hx_ + 0.5, hy_ + 0.42, ROOF_Z + 0.4))], R=0.12, n=4),
            0.025, seg=8)
    # a base do giro: o trilho escuro, o anel, o tambor torneado com as nervuras e a porta
    vp.lathe(p, 'Dark#vp', (0, 0, ROOF_Z), 'z', [(2.6, 0.0), (3.2, 0.0), (3.2, 0.04), (2.6, 0.04)], seg=48, closed=True)
    vp.lathe(p, 'Trim#vp', (0, 0, ROOF_Z), 'z', [(0.0, 0.0), (2.85, 0.0), (2.85, 0.3), (2.75, 0.36), (0.0, 0.36)], seg=48)
    h = DISH_AT[2] - ROOF_Z - 0.36
    vp.lathe(p, 'Body#vp', (0, 0, ROOF_Z + 0.36), 'z', [(0.0, 0.0), (2.5, 0.0), (2.5, 0.06), (2.36, 0.14), (2.36, h - 0.14),
                                                         (2.42, h - 0.1), (2.42, h - 0.02), (0.0, h - 0.02)], seg=48)
    vp.bolt_ring(p, 'Dark', (0, 0, DISH_AT[2] - 0.02), 'z', 2.3, 28, r=0.025, h=0.016)
    for k in range(12):
        a = math.radians(k * 30 + 15)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        if k == 0:
            continue
        ub.obox(p, 'Body#sm', d * 2.4 + Vector((0, 0, ROOF_Z + 0.36 + h / 2)), (0.14, 0.1, h - 0.3), d, Vector((-d.y, d.x, 0)), Z)
    d = Vector((math.cos(math.radians(15)), math.sin(math.radians(15)), 0.0))       # a porta do tambor (de frente)
    ub.obox(p, 'Body#sm', d * 2.38 + Vector((0, 0, ROOF_Z + 0.36 + 0.42)), (0.08, 0.7, 0.66), d, Vector((-d.y, d.x, 0)), Z)
    vp.grab_handle(p, 'Trim', d * 2.43 + Vector((0, 0, ROOF_Z + 0.6)) + Vector((-d.y, d.x, 0)) * 0.2,
                   d * 2.43 + Vector((0, 0, ROOF_Z + 0.95)) + Vector((-d.y, d.x, 0)) * 0.2, d, height=0.05, r=0.014)
    # as 2 cargas com a lona azul amarrada (os 2 lados de trás)
    for s in (-1, 1):
        a = math.radians(25)
        c = (-4.35 * math.cos(a), s * 4.35 * math.sin(a))
        yaw = s * (90 - 25)
        ub.obox(p, 'Body#sm', (c[0], c[1], ROOF_Z + 0.25), (2.3, 1.4, 0.5), Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)),
                Vector((-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0)), Z)
        ub.tarp_cover_at(p, c, 2.6, 1.7, yaw, ROOF_Z + 0.08, 0.52, m='Player', seed=2.0 + s, ties=3, box=14)
    # a caixa de ventiladores (frente-direita): a assinatura do teto da USA
    ub.fan_box(p, (3.3, 3.3), (2.0, 1.3, 0.9), fans=2, z0=ROOF_Z)
    # o armário de rádio com as antenas e a sirene (frente-esquerda), fora do giro do prato
    ax_, ay_ = 3.75, -3.75
    ub.obox(p, 'Body#pnl', (ax_, ay_, ROOF_Z + 0.58), (1.1, 1.1, 1.1), X, Y, Z)
    ub.louvers(p, (ax_ + 0.55, ay_, ROOF_Z + 0.6), (1, 0, 0), (0, 1, 0), 0.7, 0.55, 6)
    ub.obox(p, 'Player#sm', (ax_, ay_ - 0.56, ROOF_Z + 0.85), (0.9, 0.03, 0.22), X, Y, Z)
    ub.antenna(p, ax_ + 0.3, ay_ + 0.3, ROOF_Z + 1.13, 3.6, r=0.05)
    ub.antenna(p, ax_ - 0.3, ay_ - 0.3, ROOF_Z + 1.13, 2.6, r=0.05)
    ub.beacon(p, (ax_ + 0.3, ay_ + 0.3, ROOF_Z + 1.13 + 3.64), r=0.08, h=0.16, cage=False)
    ub.pipe(p, 'Trim', [(ax_ - 0.55, ay_ + 0.2, ROOF_Z + 0.2), (1.9, -2.3, ROOF_Z + 0.2), (1.5, -1.95, ROOF_Z + 0.2)], 0.05)
    for k in (3, 4):                                        # as balizas dos cantos de trás
        r = corner_r(WALL_A - 0.15)
        a = math.radians(OCT + 45 * k)
        ub.beacon(p, (r * math.cos(a), r * math.sin(a), ROOF_Z + 0.44), r=0.08, h=0.17)


def generator(p):
    gx0, gx1, gy0, gy1 = -9.3, -6.7, -9.3, -7.3
    gx, gy = (gx0 + gx1) / 2, (gy0 + gy1) / 2
    ub.obox(p, 'Concrete#pnl', (gx, gy, 0.0), (gx1 - gx0 + 0.4, gy1 - gy0 + 0.4, 0.4), X, Y, Z)
    ub.obox(p, 'Trim#sm', (gx, gy, 0.26), (gx1 - gx0 + 0.1, gy1 - gy0 + 0.1, 0.12), X, Y, Z)
    ub.obox(p, 'Body#pnl', (gx, gy, 1.2), (gx1 - gx0, gy1 - gy0, 1.8), X, Y, Z)
    ub.obox(p, 'Body#pnl', (gx, gy, 2.14), (gx1 - gx0 + 0.12, gy1 - gy0 + 0.12, 0.12), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Player#sm', (gx, gy + s * ((gy1 - gy0) / 2 + 0.02), 1.4), (gx1 - gx0 - 0.3, 0.04, 0.4), X, Y, Z)
        vp.panel_seam(p, 'Dark', (gx - 0.4, gy + s * (gy1 - gy0) / 2, 0.4), (gx - 0.4, gy + s * (gy1 - gy0) / 2, 2.0), (0, s, 0), w=0.025,
                      d=0.012)
    ub.louvers(p, (gx1 + 0.005, gy, 1.15), (1, 0, 0), (0, 1, 0), 1.5, 1.2, 10)
    vp.grab_handle(p, 'Trim', (gx - 0.6, gy1 + 0.0, 0.95), (gx - 0.6, gy1 + 0.0, 1.35), (0, 1, 0), height=0.05, r=0.016)
    e = Vector((gx0 + 0.45, gy0 + 0.45, 2.2))
    vp.lathe(p, 'Dark#vp', e, 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.05), (0.11, 0.08), (0.11, 0.7), (0.0, 0.7)], seg=16)
    vp.tube(p, 'Dark', vp.bent_bar([e + Vector((0, 0, 0.66)), e + Vector((0, 0, 0.8)), e + Vector((0.2, 0.2, 0.86))], R=0.1, n=4), 0.11,
            seg=14)
    ub.pipe(p, 'Trim', [(gx1 + 0.1, gy1 - 0.2, 0.35), (-5.9, -6.4, 0.35), (-5.35, -5.95, 0.35)], 0.09)


def yard(p):
    # o tanque de combustível deitado (canto de trás, direita) no pátio de concreto com a mureta
    ub.obox(p, 'Concrete#pnl', (-8.2, 8.5, 0.0), (2.6, 1.7, 0.4), X, Y, Z)
    ub.h_tank(p, (-8.2, 8.5, 1.15), 'x', 0.62, 2.0, m='Body', band='Player', saddles=2, foot_z=0.2)
    # o mastro da bandeira (frente, esquerda): a sapata, o poste torneado, a bola, o pano (o jogo desenha no vento)
    fx, fy = 8.6, -8.6
    ub.obox(p, 'Concrete#pnl', (fx, fy, 0.05), (1.0, 1.0, 0.5), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 0.3), 'z', [(0.0, 0.0), (0.22, 0.0), (0.22, 0.05), (0.12, 0.1), (0.1, 0.4), (0.06, 8.7), (0.0, 8.7)], seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 9.0), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), 8.9, 2.4, 1.45, m='Player')
    # o poste de luz (frente, direita), virado pro portão
    ub.lamp_post(p, 8.6, 8.6, h=6.0, aim=(8.6, 0.0))


def base():
    p = Part(PREFIX + '_Base')
    ub.keys(p)
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.4), (-HX, HX, HY - 0.4, HY), (-HX, -HX + 0.4, -HY + 0.4, HY - 0.4),
                             (HX - 0.4, HX, -HY + 0.4, HY - 0.4)):
        ub.obox(p, 'Concrete#pnl', ((x0 + x1) / 2, (y0 + y1) / 2, -0.09), (x1 - x0, y1 - y0, 0.42), X, Y, Z)
    bunker(p)
    gate(p)
    roof(p)
    generator(p)
    yard(p)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o prato (origem no eixo)
def z_par(r):
    return r * r / (4 * F_DISH)


def dish_point(r, t, back=0.0):
    return AX + N_DISH * (OFF + z_par(r) - back) + (U_DISH * math.cos(t) + Y_DISH * math.sin(t)) * r


def dish_annulus(p, key, r0, r1, nr, seg, thick):
    """Uma casca parabólica fechada entre os raios r0 e r1 (a frente, as costas e as duas bordas), lisa (o A)."""
    b = p.bm(key, 0.0)
    radii = [r0 + (r1 - r0) * i / nr for i in range(nr + 1)]
    front = [[b.verts.new(V(*dish_point(r, 2 * math.pi * k / seg))) for k in range(seg)] for r in radii]
    backv = [[b.verts.new(V(*dish_point(r, 2 * math.pi * k / seg, thick))) for k in range(seg)] for r in radii]
    for i in range(nr):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([front[i][k], front[i][j], front[i + 1][j], front[i + 1][k]]).smooth = True
            b.faces.new([backv[i + 1][k], backv[i + 1][j], backv[i][j], backv[i][k]]).smooth = True
    for ring_f, ring_b in ((front[0], backv[0]), (front[-1], backv[-1])):
        for k in range(seg):
            j = (k + 1) % seg
            b.faces.new([ring_f[k], ring_b[k], ring_b[j], ring_f[j]])


def dish():
    p = Part(PREFIX + '_Dish')
    ub.keys(p)
    # a mesa giratória torneada com os parafusos
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (2.15, 0.0), (2.15, 0.22), (2.05, 0.3), (0.0, 0.3)], seg=48)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.3), 'z', 1.95, 24, r=0.025, h=0.016)
    # a casa do azimute de placas, a porta, a faixa azul
    ub.obox(p, 'Body#pnl', (-0.15, 0.0, 0.8), (2.3, 2.0, 1.0), X, Y, Z)
    ub.obox(p, 'Player#sm', (-1.32, 0.0, 0.8), (0.04, 1.6, 0.3), X, Y, Z)
    ub.obox(p, 'Body#sm', (1.02, 0.0, 0.78), (0.06, 0.9, 0.7), X, Y, Z)
    vp.grab_handle(p, 'Trim', (1.06, -0.3, 0.6), (1.06, -0.3, 0.95), (1, 0, 0), height=0.05, r=0.014)
    for s in (-1, 1):
        ub.louvers(p, (-0.4, s * 1.005, 0.8), (0, s, 0), (1, 0, 0), 0.9, 0.5, 5)
    # o garfo (placa grossa de quinas redondas), os munhões torneados, o eixo
    for s in (-1, 1):
        prof = [(-0.7, 1.3), (0.9, 1.3), (AX.x + 0.42, AX.z + 0.05), (AX.x + 0.25, AX.z + 0.38), (AX.x - 0.25, AX.z + 0.38),
                (AX.x - 0.42, AX.z + 0.05)]
        vp.prism(p, 'Body#pnl', prof, Vector((0, 1.05 if s > 0 else -1.4, 0)), Vector(X), Vector(Z), Vector(Y), 0.35)
        vp.lathe(p, 'Trim#vp', (AX.x, 1.4 * s, AX.z), (0, s, 0), [(0.0, 0.0), (0.4, 0.0), (0.4, 0.08), (0.34, 0.14), (0.18, 0.14),
                                                                   (0.16, 0.22), (0.0, 0.22)], seg=24)
        vp.bolt_ring(p, 'Dark', (AX.x, (1.4 + 0.08) * s, AX.z), (0, s, 0), 0.3, 10, r=0.022, h=0.014)
    vp.lathe(p, 'Trim#vp', (AX.x, 0, AX.z), 'y', [(0.0, -1.1), (0.3, -1.1), (0.3, 1.1), (0.0, 1.1)], seg=20)
    # o cubo atrás do prato, o contrapeso, o macaco de elevação
    vp.lathe(p, 'Dark#vp', AX + N_DISH * 0.1, N_DISH, [(0.0, 0.0), (0.75, 0.0), (0.8, 0.05), (0.8, 0.6), (0.7, 0.72), (0.0, 0.72)], seg=28)
    ub.obox(p, 'Dark#pnl', AX - N_DISH * 0.5 - U_DISH * 0.2, (0.7, 1.4, 0.6), N_DISH, Y_DISH, N_DISH.cross(Y_DISH))
    a = Vector((-0.9, 0.0, 1.3))
    b = dish_point(1.4, math.pi, THICK + 0.05)
    u = (b - a).normalized()
    L = (b - a).length
    vp.lathe(p, 'Yellow#vp', a, u, [(0.0, 0.0), (0.16, 0.0), (0.19, 0.05), (0.19, L * 0.55), (0.21, L * 0.57), (0.21, L * 0.6),
                                    (0.09, L * 0.62), (0.0, L * 0.62)], seg=16)
    vp.lathe(p, 'Trim#vp', a, u, [(0.0, L * 0.55), (0.09, L * 0.55), (0.09, L), (0.0, L)], seg=12)
    for q in (a, b):
        vp.lathe(p, 'Trim#vp', q, (0, 1, 0), [(0.0, -0.13), (0.12, -0.13), (0.12, 0.13), (0.0, 0.13)], seg=14)
    # a corneta do alimentador na frente do cubo
    vp.lathe(p, 'Trim#vp', AX + N_DISH * (OFF + 0.05), N_DISH, [(0.0, 0.0), (0.32, 0.0), (0.3, 0.3), (0.22, 0.9), (0.18, 1.15), (0.0, 1.15)],
             seg=20)
    # a casca do prato (branca, lisa), o aro e o lábio
    dish_annulus(p, 'Paint#dish', 0.6, R_DISH, 9, 64, THICK)
    dish_annulus(p, 'Trim#rim', R_DISH, R_DISH + 0.14, 1, 64, THICK + 0.06)
    # as 8 costelas atrás (seguindo a curva) e o anel de treliça
    for k in range(8):
        t = 2 * math.pi * (k + 0.5) / 8
        tv = -U_DISH * math.sin(t) + Y_DISH * math.cos(t)
        rings = []
        for i in range(7):
            r = 0.7 + (R_DISH * 0.94 - 0.7) * i / 6
            q = dish_point(r, t, THICK + 0.11)
            hh = 0.14 * (1.0 - 0.5 * i / 6)
            rings.append([tuple(q - tv * 0.06 - N_DISH * hh), tuple(q + tv * 0.06 - N_DISH * hh),
                          tuple(q + tv * 0.06 + N_DISH * 0.11), tuple(q - tv * 0.06 + N_DISH * 0.11)])
        p.loft('Dark#rib', rings, bevel=0.0)
    ring = [dish_point(2.6, 2 * math.pi * k / 32, THICK + 0.18) for k in range(33)]
    vp.tube(p, 'Dark', ring, 0.045, seg=8, caps=False)
    for k in range(8):
        t = 2 * math.pi * (k + 0.5) / 8
        vp.tube(p, 'Dark', [dish_point(1.0, t, THICK + 0.2), dish_point(2.6, t + 0.2, THICK + 0.18)], 0.03, seg=6)
    # o subrefletor no foco, os 4 braços do aro até ele, a sirene
    F = AX + N_DISH * (OFF + F_DISH)
    vp.lathe(p, 'Body#vp', F - N_DISH * 0.1, N_DISH, [(0.0, 0.0), (0.2, 0.0), (0.5, 0.1), (0.56, 0.16), (0.54, 0.2), (0.0, 0.22)], seg=28)
    vp.lathe(p, 'Dark#vp', F - N_DISH * 0.3, N_DISH, [(0.0, 0.0), (0.22, 0.0), (0.3, 0.2), (0.0, 0.2)], seg=16)
    for k in range(4):
        t = 2 * math.pi * (k + 0.5) / 4
        q0 = dish_point(R_DISH * 0.93, t, -0.02)
        q1 = F - N_DISH * 0.1 + (U_DISH * math.cos(t) + Y_DISH * math.sin(t)) * 0.35
        vp.tube(p, 'Trim', [q0, q1], 0.05, seg=8)
        vp.lathe(p, 'Trim#vp', q0, (q1 - q0).normalized(), [(0.0, 0.0), (0.09, 0.0), (0.09, 0.08), (0.0, 0.08)], seg=10)
    ub.beacon(p, tuple(F + N_DISH * 0.1), r=0.08, h=0.14, cage=False)
    return ub.finish(p)


def scaffold_poly(name, pts, h, out=1.0, skip=(), post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra ao longo do contorno (o do A, copiado: o model_strategy_center.py roda o main() ao ser importado)."""
    p = Part(name)
    top = h + 1.0
    levels = max(2, int((top - z0) / lift))
    n = len(pts)
    cx, cy = sum(q[0] for q in pts) / n, sum(q[1] for q in pts) / n
    for i in range(n):
        if i in skip:
            continue
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        ox, oy = dy, -dx
        if ox * ((ax + bx) / 2 - cx) + oy * ((ay + by) / 2 - cy) < 0:
            ox, oy = -ox, -oy
        yaw = math.degrees(math.atan2(dy, dx))
        m = max(1, int(L / bay))
        for k in range(m + 1):
            t = k / m
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            for d in (0.0, out):
                p.cyl(post, (px + ox * d, py + oy * d, (z0 + top) / 2), 'z', 0.05, top - z0, seg=8, bevel=0.0)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            mx, my = (ax + bx) / 2, (ay + by) / 2
            for d in (0.0, out):
                p.cyl(post, (mx + ox * d, my + oy * d, z), (dx, dy, 0.0), 0.045, L, seg=8, bevel=0.0)
            p.obox(plank, (mx + ox * out / 2, my + oy * out / 2, z - 0.03), (L, abs(out) - 0.1, 0.06), yaw=yaw, bevel=0.01)
            p.obox(stripe, (mx + ox * out, my + oy * out, z + 0.09), (L, 0.04, 0.18), yaw=yaw, bevel=0.0)
        for k in range(m):
            t0, t1 = k / m, (k + 1) / m
            pa = Vector((ax + (bx - ax) * t0 + ox * out, ay + (by - ay) * t0 + oy * out, z0 + (top - z0) * ((k % levels) / levels)))
            pb = Vector((ax + (bx - ax) * t1 + ox * out, ay + (by - ay) * t1 + oy * out, z0 + (top - z0) * ((k % levels + 1) / levels)))
            d = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (d.x, d.y, d.z), 0.035, d.length, seg=6, bevel=0.0)
    return p.finish()


def write_spec():
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Dish', [0.0, 0.0, DISH_AT[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1300]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 900]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1400, 900]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [2.0, 13.8, 0], 'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    d = bake_in_place(dish(), DISH_AT)
    total = export(b, OUT)
    total += export(d, OUT)
    pts = [(corner_r(GLACIS_A + 0.3) * math.cos(math.radians(OCT + 45 * k)), corner_r(GLACIS_A + 0.3) * math.sin(math.radians(OCT + 45 * k)))
           for k in range(8)]
    total += export(scaffold_poly(PREFIX + '_Scaffold', pts, WALL_Z + 0.4, out=1.0, skip=(7,)), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, d)})
    top = DISH_AT[2] + max((dish_point(R_DISH + 0.14, t)).z for t in [2 * math.pi * k / 64 for k in range(64)])
    print('MODEL lote 20 x 20 m; bunker', WALL_Z + 0.44, 'm; prato ate', round(top, 2), 'm; spin dish at_m', DISH_AT, 'rpm 3')
    write_spec()


main()
