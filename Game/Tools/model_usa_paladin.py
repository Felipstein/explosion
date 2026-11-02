"""O Paladin da USA (usa_paladin), o tanque pesado, provisório, modelado no Blender por script (o elenco, 31/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_paladin.py -- captures/game/models/usa_paladin

O que é (docs/ELENCO.md §3, a unidade reservada em CONCEITO_UNIDADES.md §3): o tanque básico da USA "maior e um pouco
diferente", pro fim de jogo. A palavra, de cima: "a cunha com o laser". A planta é o design 3 de
docs/concept/units/usa_mbt_exploration.webp: a torre em CUNHA (as duas bochechas em bico apontando pra frente, o mantelete
recuado no vão entre elas: de cima, uma ponta de flecha com o entalhe do canhão), o nariz do casco com as saias da frente em
cunha e as fendas dos faróis, as saias grossas em placas, as caixas da defesa ativa nos lados da torre, o mastro do sensor
atrás. A pintura é a da ficha docs/concept/units/usa_mbt_player_color.webp: cinza-claro quase branco (Paint) e a cor do
jogador em detalhes azuis pequenos seguindo as placas (Team): a faixa segmentada nas saias, os retalhos nas placas dos lados
e do teto da torre, os 2 anéis no cano e a lona amarrada no motor.
A assinatura: o LASER DE DEFESA DE PONTO (o que derruba míssil), a torrinha redonda no teto de trás da torre, num pedestal
acima de tudo, com a lente grande azul (Lens) e o sensor: lê de cima como o círculo no meio das facetas. Inspiração real: o
M1A2 SEPv3 (o casco, as saias, o cesto, a mira independente do comandante), o Leopard 2A6 (a blindagem em cunha da torre) e
os protótipos de defesa a laser de veículo (o diretor de feixe numa torrinha própria, com a janela grande e o rastreador).
Família do tanque USA do Look (Game/Tools/look_tank.py): as mesmas medidas de casco escaladas, o mesmo jeito de saia, deck,
cesto, lona e cano; o Paladin é ~6% mais comprido, mais largo nas saias e mais alto na torre.

Medidas (frente +X, origem no chão no centro): casco 8,6 m (o nariz em +4,30, a traseira em −4,30), 4,0 m nas saias (0,20 m
de espessura), deck a 1,62 m, o teto da torre a 2,62 m, o laser a ~3,4 m; o canhão a 2,15 m com a boca em +6,45 m. 7 rodas
duplas por lado, a lagarta de 0,68 m. O tanque USA: 8,1 × 3,8 m, a torre a 2,48 m, a boca em +5,64 m.

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar delas, menos o laser):
  SM_USA_Paladin_Hull     o casco, as saias, o deck, a lona, o anel da torre;
  SM_USA_Paladin_Turret   a torre em cunha (gira em Z em volta de (TX, 0)), com o mantelete, as miras, as escotilhas, as caixas
                          da defesa ativa, o lança-fumaça, o pedestal do laser, o mastro, as antenas, o cesto com a carga;
  SM_USA_Paladin_Barrel   o cano (recua em −X a partir da posição de repouso; a boca é o X máximo);
  SM_USA_Paladin_Laser    a torrinha do laser com a ORIGEM NO EIXO DELA (gira em Z; fica em LASER_AT no teto da torre);
  SM_USA_Paladin_Wheels   as rodas (uma malha, como o tanque do Look);
  SM_USA_Paladin_Track_l / _Track_r  as lagartas.
Slots: Paint, Team, Cloth, Metal, Dark, Lens, Track.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, select_only, sheet, track_path  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_paladin')
os.makedirs(OUT, exist_ok=True)
POSE = os.path.join(OUT, 'pose')
os.makedirs(POSE, exist_ok=True)
PREFIX = 'SM_USA_Paladin'

# as cores de conferência do tanque do Look (o jogo usa as instâncias dele)
COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Cloth': (64, 86, 132), 'Lens': (40, 90, 160), 'Track': (55, 52, 48), 'Rubber': (28, 28, 28)})

# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 4.30, -4.30
DECK = 1.62
HALF_UP, HALF_LOW = 1.80, 1.08
SKIRT = (1.80, 2.00)             # as saias grossas (de dentro pra fora)
SKIRT_Z = (0.62, 1.56)
WHEELS_X = [2.85, 1.92, 0.99, 0.06, -0.87, -1.80, -2.73]
WHEEL_R, WHEEL_Z = 0.31, 0.38
IDLER = (3.82, 0.74, 0.30)
SPROCKET = (-3.78, 0.64, 0.27)
ROLLERS_X = [1.45, -0.40, -2.25]
ROLLER_Z, ROLLER_R = 1.04, 0.1
TRACK_Y, TRACK_W = 1.48, 0.68
TX = 0.0                         # o eixo da torre
RING_R = 1.28
T_Z0, T_Z1, T_Z2 = 1.67, 1.99, 2.62     # a torre: o pé, a cintura (as pontas das bochechas), o teto
GUN_Z = 2.15
GUN_X0 = 1.98                    # a saída do mantelete (o cano começa aqui)
MUZZLE_X = 6.45
LASER_AT = (TX - 1.02, 0.0, T_Z2 + 0.24)   # o eixo do laser (o pé da torrinha, em cima do pedestal)


def tube(p, m, a, b, r=0.03, seg=10):
    a, b = Vector(a), Vector(b)
    d = b - a
    p.cyl(m, tuple((a + b) / 2), tuple(d), r, d.length, seg=seg, bevel=0.0)


# ---------------------------------------------------------------------------------- casco
def glacis_z(x):
    """O glacis de cima: do deck (3,40; 1,62) ao nariz (4,30; 1,18)."""
    return DECK - (x - 3.40) * (0.44 / 0.90)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint', 0.035)
    p.bm('Paint#thin', 0.008)
    p.bm('Team', 0.004)
    p.bm('Dark', 0.0)
    p.bm('Metal', 0.006)
    p.bm('Lens', 0.0)
    # o casco de baixo (entre as lagartas) e os sobre-lagartas, em perfil de lado
    # (o de baixo fica 2-3 cm pra dentro do de cima: as faces que coincidiam davam o deck preto, normais trocadas no chanfro)
    p.prism_xz('Paint', [(3.95, 0.52), (NOSE - 0.03, 1.17), (3.42, DECK - 0.03), (-4.13, DECK - 0.03), (REAR + 0.02, 1.48), (REAR + 0.02, 0.58), (-4.12, 0.48)], -HALF_LOW, HALF_LOW)
    p.prism_xz('Paint', [(4.18, 0.98), (NOSE, 1.18), (3.40, DECK), (-4.15, DECK), (REAR, 1.50), (REAR, 0.98)], -HALF_UP, HALF_UP)
    # o vinco do glacis (a chapa de cima, a junta) e a escotilha do motorista com os periscópios
    gl = math.degrees(math.atan2(0.44, 0.90))
    p.obox('Paint#thin', (3.86, 0.0, glacis_z(3.86) + 0.03), (0.86, 1.9, 0.05), pitch=-gl)
    for y in (-0.98, 0.98):
        p.obox('Dark', (3.86, y, glacis_z(3.86) + 0.035), (0.9, 0.02, 0.05), pitch=-gl, bevel=0.0)
    vp.hatch_round(p, (3.05, 0.0, DECK), 0.32, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    for k in (-0.22, 0.0, 0.22):
        p.obox('Lens', (3.36, k, DECK + 0.05), (0.08, 0.15, 0.06), pitch=20, bevel=0.0)
    # o anel da torre
    p.cyl('Dark', (TX, 0.0, DECK + 0.01), 'z', RING_R + 0.05, 0.02, seg=48)
    # o deck do motor: as grades (aletas), as tampas, a lona azul amarrada (cor do jogador) caindo na traseira
    for k in range(10):
        p.obox('Dark', (-2.95 - k * 0.12, 0.0, DECK + 0.012), (0.05, 2.0, 0.025), pitch=35, bevel=0.0)
    for s in (-1, 1):
        p.box('Paint', -4.15, -2.85, 1.12 * s - 0.5 if s > 0 else -1.62, 1.62 if s > 0 else -1.12 + 0.5, DECK, DECK + 0.07)
    # (31/10, o detalhe AAA) a lona azul DOBRADA em camadas no deck do motor (as 3 cintas apertam e ela estufa entre elas) e a
    # aba caída pela traseira com as dobras e a bainha (antes: um bloco liso e uma placa)
    vp.tarp_bundle(p, (-3.68, 0.0, DECK + 0.02), (1.18, 2.06, 0.25), m='Team', strap_m='Dark', buckle_m='Metal', straps=3, layers=3,
                   seed=3.0, sag_ends=0.22, swap=True)
    vp.tarp_drape(p, (-4.33, 0.92, DECK + 0.05), (-4.33, -0.92, DECK + 0.05), DECK + 0.05 - 0.95, (-1.0, 0.0, 0.0), m='Team', folds=6,
                  amp=0.016, seed=2.0, tie_m='Dark', ties=(0.08, 0.5, 0.92))
    # as saias grossas: 6 placas por lado, a da frente em cunha (as fendas dos faróis), a faixa azul segmentada
    xs = [3.50, 2.55, 1.60, 0.65, -0.30, -1.25, -2.20]
    for s in (-1, 1):
        y0, y1 = (SKIRT[0], SKIRT[1]) if s > 0 else (-SKIRT[1], -SKIRT[0])
        for i in range(len(xs) - 1):
            p.box('Paint', xs[i + 1] + 0.02, xs[i] - 0.02, y0, y1, SKIRT_Z[0], SKIRT_Z[1])
            yo = (SKIRT[1] + 0.006) if s > 0 else -(SKIRT[1] + 0.006)
            # (31/10) a fileira de parafusos de cima (o ritmo) e os 2 de baixo das pontas
            vp.bolt_row(p, 'Metal', (xs[i + 1] + 0.12, yo - 0.006 * s, SKIRT_Z[1] - 0.09), (xs[i] - 0.12, yo - 0.006 * s, SKIRT_Z[1] - 0.09), 5,
                        (0, s, 0), r=0.022, h=0.016)
            for xb in (xs[i + 1] + 0.12, xs[i] - 0.12):
                vp.nut(p, 'Metal', (xb, yo - 0.006 * s, SKIRT_Z[0] + 0.1), (0, s, 0), r=0.022, h=0.016)
        for i in (1, 2, 3, 4):
            yo = (SKIRT[1] + 0.006) if s > 0 else -(SKIRT[1] + 0.006)
            p.box('Team', xs[i + 1] + 0.1, xs[i] - 0.1, min(yo, yo - 0.014 * s), max(yo, yo - 0.014 * s), 1.06, 1.25)
        # a placa da frente em cunha (o nariz das saias) com as fendas dos faróis e o retalho azul em diagonal
        p.prism_xz('Paint', [(xs[0] + 0.02, SKIRT_Z[1]), (4.22, 1.28), (4.05, 0.80), (xs[0] + 0.02, SKIRT_Z[0])], y0, y1)
        yo = (SKIRT[1] + 0.006) if s > 0 else -(SKIRT[1] + 0.006)
        p.obox('Team', (3.86, yo, 1.05), (0.34, 0.014, 0.44), pitch=-35)
        for (zz, ln) in ((1.36, 0.3), (1.28, 0.22)):
            p.obox('Lens', (4.10 - (1.36 - zz) * 0.6, 1.9 * s, zz), (0.025, 0.16, 0.035), yaw=0, pitch=-25, bevel=0.0)
        p.box('Dark', 3.90, 4.20, 1.90 * s - 0.1, 1.90 * s + 0.1, 0.70, 0.74, bevel=0.0)        # a aba de borracha
        # a caixa da grade de exaustão (atrás, no lado das saias) com as aletas
        gy0, gy1 = (1.82, 1.98) if s > 0 else (-1.98, -1.82)
        p.box('Paint', -3.30, -2.24, gy0, gy1, 0.70, 1.56)
        for k in range(11):
            z = 0.78 + k * 0.066
            yo2 = gy1 + 0.01 if s > 0 else gy0 - 0.01
            p.obox('Dark', (-2.77, yo2, z), (0.96, 0.03, 0.035), roll=40 * s, bevel=0.0)
        p.box('Paint', -4.25, -3.34, y0, y1, 0.82, SKIRT_Z[1])                                  # a traseira das saias
        p.box('Team', -3.60, -3.40, min(yo, yo - 0.014 * s), max(yo, yo - 0.014 * s), 1.06, 1.25)
        # o friso de cima das saias (a junta escura que segue as placas)
        p.box('Dark', -4.2, 4.0, min(yo, yo - 0.008 * s), max(yo, yo - 0.008 * s), 1.43, 1.45, bevel=0.0)
        # os faróis de cima do nariz, as lanternas e os ganchos
        p.box('Dark', 4.02, 4.24, 1.35 * s - 0.13, 1.35 * s + 0.13, 1.20, 1.34, bevel=0.01)
        p.obox('Lens', (4.245, 1.35 * s, 1.27), (0.02, 0.2, 0.09), bevel=0.0)
        p.box('Metal', 4.18, 4.36, 0.72 * s - 0.08, 0.72 * s + 0.08, 0.70, 0.86, bevel=0.01)
        p.box('Metal', -4.44, -4.28, 0.82 * s - 0.08, 0.82 * s + 0.08, 0.70, 0.84, bevel=0.01)
        p.box('Dark', -4.32, -4.30, 1.38 * s - 0.1, 1.38 * s + 0.1, 1.24, 1.36, bevel=0.0)
        p.box('Lens', -4.325, -4.32, 1.38 * s - 0.07, 1.38 * s + 0.07, 1.27, 1.33, bevel=0.0)
    # a placa de trás com as aletas do escape
    for k in range(7):
        p.obox('Dark', (-4.32, 0.0, 0.86 + k * 0.075), (0.03, 1.5, 0.035), pitch=-30, bevel=0.0)
    # (31/10) o cabo de reboque em U na chapa de trás, entre os ganchos
    vp.tow_cable(p, 'Metal', [(-4.34, -0.82, 0.77), (-4.35, -0.6, 0.68), (-4.35, 0.6, 0.68), (-4.34, 0.82, 0.77)], r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def plan(front_c, rec, tip_in, tip_out, cheek, side_rear, rear_c):
    """O contorno da torre em cunha (meia planta, da frente do meio até a traseira do meio, +y), espelhado."""
    half = [front_c, rec, tip_in, tip_out, cheek, side_rear, rear_c]
    pts = [(front_c[0], 0.0)] + half[1:-1] + [(rear_c[0], 0.0)]
    mirrored = [(x, -y) for (x, y) in reversed(half[1:-1])]
    return pts + mirrored


PLANS = {
    T_Z0: plan((1.70, 0), (1.70, 0.46), (2.42, 0.62), (2.30, 0.92), (1.25, 1.46), (-1.55, 1.46), (-2.05, 1.28)),
    T_Z1: plan((1.76, 0), (1.76, 0.46), (2.76, 0.62), (2.62, 0.96), (1.35, 1.63), (-1.70, 1.63), (-2.25, 1.40)),
    T_Z2: plan((1.56, 0), (1.56, 0.42), (1.86, 0.55), (1.76, 0.84), (1.05, 1.40), (-1.62, 1.45), (-2.15, 1.28)),
}


def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('Paint', 0.035)
    p.bm('Paint#thin', 0.012)
    p.bm('Team', 0.004)
    p.bm('Dark', 0.0)
    p.bm('Metal', 0.006)
    p.bm('Lens', 0.0)
    p.bm('Cloth', 0.04)
    rings = []
    for z in (T_Z0, T_Z1, T_Z2):
        rings.append([(TX + x, y, z) for (x, y) in PLANS[z]])
    p.loft('Paint', rings)
    p.cyl('Paint', (TX, 0.0, DECK + 0.055), 'z', RING_R - 0.03, 0.05, seg=48)               # o pé no anel (folga de 5 mm)
    # o vão do mantelete entre as bochechas (escuro no fundo) e o mantelete
    p.box('Dark', TX + 1.64, TX + 1.72, -0.44, 0.44, T_Z0 + 0.04, T_Z2 - 0.06, bevel=0.0)
    p.box('Paint', TX + 1.66, TX + 1.98, -0.40, 0.40, 1.86, 2.44)
    p.cyl('Dark', (TX + 2.0, 0.0, GUN_Z), 'x', 0.22, 0.06, seg=28)
    p.box('Paint#thin', TX + 1.74, TX + 2.0, -0.36, -0.12, 2.36, 2.54)                      # a caixa da mira do atirador (em cima do mantelete)
    p.box('Lens', TX + 2.0, TX + 2.01, -0.32, -0.16, 2.40, 2.50, bevel=0.0)
    # as juntas das placas das bochechas (seguem a cunha) e os retalhos azuis nos lados e no teto
    for s in (-1, 1):
        a = Vector((TX + 2.76, 0.62 * s, T_Z1))
        b = Vector((TX + 1.86, 0.55 * s, T_Z2))
        tube(p, 'Dark', a + Vector((0, 0, 0.01)), b + Vector((0, 0, 0.01)), 0.012, seg=6)
        yo = 1.63 * s
        # os retalhos azuis seguindo as placas do lado (2 placas, como a ficha)
        for (x0, x1) in ((0.55, 0.92), (-1.20, -0.86)):
            p.box('Team', TX + x0, TX + x1, yo - 0.006 if s > 0 else yo - 0.008, yo + 0.008 if s > 0 else yo + 0.006, T_Z1 - 0.26, T_Z1 + 0.32)
        p.box('Team', TX - 0.05, TX + 0.55, 1.12 * s - 0.17, 1.12 * s + 0.17, T_Z2, T_Z2 + 0.014)   # o retalho do teto (lê de cima)
        # as caixas da defesa ativa (2 × 2) nos lados de trás, com as bocas, e as de frente nas bochechas
        for (x0, yb) in ((-1.55, 1.60), (0.95, 1.55)):
            for i in range(2):
                for j in range(2):
                    xa = TX + x0 + i * 0.34
                    y0, y1 = ((yb + 0.02), (yb + 0.30)) if s > 0 else (-(yb + 0.30), -(yb + 0.02))
                    p.box('Paint', xa, xa + 0.3, y0, y1, 2.0 + j * 0.27, 2.24 + j * 0.27, bevel=0.015)
                    yf = (yb + 0.305) if s > 0 else -(yb + 0.305)
                    p.box('Dark', xa + 0.06, xa + 0.24, yf - 0.004, yf + 0.004, 2.05 + j * 0.27, 2.19 + j * 0.27, bevel=0.0)
        # o lança-fumaça: 4 tubos em leque em cima de cada bochecha
        for k in range(4):
            p.cyl('Dark', (TX + 1.40 - k * 0.12, 1.30 * s, T_Z2 + 0.05), (0.6, 0.55 * s, 0.6), 0.045, 0.22, seg=10)
        # as antenas nos cantos de trás
        ax, ay = TX - 2.0, 1.05 * s
        p.cyl('Dark', (ax, ay, T_Z2 + 0.03), 'z', 0.045, 0.06, seg=10)
        p.cyl('Metal', (ax, ay, T_Z2 + 0.06 + 0.7), 'z', 0.012, 1.4, seg=6)
    # o teto: a mira independente do comandante (frente-esquerda, a caixa com a lente azul), a cúpula, a escotilha do municiador
    p.cyl('Paint', (TX + 0.70, -0.72, T_Z2 + 0.06), 'z', 0.26, 0.12, seg=24)
    p.box('Paint', TX + 0.48, TX + 0.92, -0.95, -0.49, T_Z2 + 0.12, T_Z2 + 0.52, bevel=0.025)
    p.box('Paint', TX + 0.52, TX + 0.88, -0.92, -0.52, T_Z2 + 0.52, T_Z2 + 0.6, bevel=0.015)
    p.box('Lens', TX + 0.92, TX + 0.935, -0.88, -0.66, T_Z2 + 0.22, T_Z2 + 0.42, bevel=0.0)
    p.box('Dark', TX + 0.92, TX + 0.935, -0.62, -0.53, T_Z2 + 0.24, T_Z2 + 0.38, bevel=0.0)
    p.cyl('Paint', (TX - 0.20, -0.72, T_Z2 + 0.08), 'z', 0.46, 0.16, seg=8, bevel=0.015)     # a cúpula octogonal
    p.cyl('Paint', (TX - 0.20, -0.72, T_Z2 + 0.18), 'z', 0.34, 0.06, seg=24)
    for k in range(6):
        a = math.radians(k * 60 + 30)
        p.obox('Lens', (TX - 0.20 + 0.45 * math.cos(a), -0.72 + 0.45 * math.sin(a), T_Z2 + 0.1), (0.03, 0.14, 0.05), yaw=k * 60 + 30, bevel=0.0)
    vp.hatch_round(p, (TX - 0.15, 0.70, T_Z2), 0.31, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')    # o municiador
    for yy in (-0.15, 0.35):                                                                    # (31/10) as alças do teto
        vp.grab_handle(p, 'Metal', (TX + 1.3, yy - 0.12, T_Z2), (TX + 1.3, yy + 0.12, T_Z2), (0, 0, 1), height=0.07, r=0.016)
    p.box('Paint', TX + 0.95, TX + 1.30, 0.40, 0.80, T_Z2, T_Z2 + 0.06, bevel=0.01)         # a tampa de ventilação
    # o pedestal do laser (fixo na torre: o laser em cima gira sozinho) com as aletas de refrigeração
    lx, ly, lz = LASER_AT
    p.cyl('Paint', (lx, ly, T_Z2 + 0.11), 'z', 0.52, 0.22, seg=36, r2=0.45)
    p.cyl('Dark', (lx, ly, lz - 0.025), 'z', 0.42, 0.02, seg=36)
    for k in range(12):
        a = math.radians(k * 30 + 15)
        p.obox('Dark', (lx + 0.5 * math.cos(a), ly + 0.5 * math.sin(a), T_Z2 + 0.11), (0.07, 0.012, 0.16), yaw=k * 30 + 15, bevel=0.0)
    p.box('Team', lx - 0.56, lx - 0.50, -0.2, 0.2, T_Z2 + 0.02, T_Z2 + 0.16, bevel=0.0)       # o retalho azul no pedestal
    # o mastro do sensor (atrás, à direita: o do design 3)
    mx, my = TX - 1.80, 0.98
    p.cyl('Paint', (mx, my, T_Z2 + 0.35), 'z', 0.07, 0.7, seg=12)
    p.cyl('Paint', (mx, my, T_Z2 + 0.95), 'z', 0.05, 0.5, seg=12)
    p.cyl('Paint', (mx, my, T_Z2 + 1.28), 'z', 0.13, 0.22, seg=20)
    p.cyl('Lens', (mx + 0.12, my, T_Z2 + 1.28), 'x', 0.05, 0.02, seg=14)
    # o cesto atrás (tubos) com a carga: as caixas e o fardo de lona
    bx0, bx1 = TX - 2.18, TX - 2.72
    # (31/10, o detalhe AAA) o tubo redondo dobrado nas quinas (antes barras quadradas coladas), a carga de verdade: a caixa com a
    # tampa e os trincos, o fardo de lona dobrado com as cintas, o galão
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(bx1, 1.07 * s, 1.84), (bx1, 1.07 * s, 2.38)], 0.022, seg=8)
    for z in (1.88, 2.12, 2.36):
        vp.tube(p, 'Metal', vp.bent_bar([(bx0, -1.07, z), (bx1, -1.07, z), (bx1, 1.07, z), (bx0, 1.07, z)], R=0.08, n=3), 0.02, seg=8)
    vp.ammo_box(p, ((bx1 + bx0) / 2 + 0.02, -0.585, 2.05), (0.66, 0.46, 0.38), yaw=90, m='Paint', latch_m='Metal')
    vp.tarp_bundle(p, ((bx1 + bx0) / 2, 0.41, 1.86), (0.46, 0.78, 0.27), m='Cloth', strap_m='Dark', buckle_m='Metal', straps=2,
                   layers=3, seed=5.0, swap=True)
    vp.jerrycan(p, ((bx1 + bx0) / 2, -0.09, 1.86 + 0.235), yaw=90, m='Paint', cap_m='Metal')
    return p.finish()


# ---------------------------------------------------------------------------------- o laser (a assinatura)
def laser():
    """A torrinha do laser de defesa de ponto com a ORIGEM NO EIXO (o pé, em cima do pedestal da torre): o anel giratório, o
    garfo, a cabeça octogonal com a janela grande do feixe (a lente azul), o rastreador em cima e as aletas atrás."""
    p = Part(PREFIX + '_Laser')
    p.bm('Paint', 0.02)
    p.bm('Paint#thin', 0.006)
    p.bm('Team', 0.003)
    p.bm('Dark', 0.0)
    p.bm('Metal', 0.004)
    p.bm('Lens', 0.0)
    p.cyl('Paint', (0.0, 0.0, 0.05), 'z', 0.40, 0.10, seg=36)                               # o anel giratório
    p.cyl('Dark', (0.0, 0.0, 0.105), 'z', 0.33, 0.012, seg=36)
    for s in (-1, 1):                                                                        # o garfo
        y0, y1 = sorted((0.26 * s, 0.34 * s))
        p.prism_xz('Paint', [(-0.2, 0.10), (0.2, 0.10), (0.12, 0.50), (-0.12, 0.50)], y0, y1)
        p.cyl('Dark', (0.0, 0.30 * s, 0.42), 'y', 0.085, 0.09, seg=16)
    # a cabeça: o prisma octogonal deitado (o eixo em X), a janela do feixe na frente, o rastreador
    hz = 0.42
    oct_ = [(0.0 + 0.245 * math.cos(math.radians(22.5 + 45 * k)), 0.245 * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    rings = []
    for x, sc in ((-0.38, 0.92), (-0.29, 1.0), (0.30, 1.0), (0.37, 0.9)):
        rings.append([(x, u * sc * 1.0, hz + w * sc * 0.95) for (u, w) in oct_])
    p.loft('Paint', rings)
    p.cyl('Dark', (0.375, 0.0, hz), 'x', 0.19, 0.03, seg=32)                                  # o aro da janela
    p.cyl('Lens', (0.386, 0.0, hz), 'x', 0.16, 0.012, seg=32)                                # a lente grande do feixe
    p.cyl('Metal', (0.392, 0.0, hz), 'x', 0.05, 0.008, seg=16)
    p.box('Paint', -0.16, 0.22, -0.1, 0.1, hz + 0.22, hz + 0.34, bevel=0.012)               # o rastreador em cima
    p.box('Lens', 0.22, 0.23, -0.065, 0.065, hz + 0.245, hz + 0.315, bevel=0.0)
    for k in range(6):                                                                       # as aletas de refrigeração atrás
        p.box('Dark', -0.45, -0.37, -0.18 + k * 0.072, -0.155 + k * 0.072, hz - 0.16, hz + 0.16, bevel=0.0)
    p.box('Team', -0.03, 0.13, -0.245, -0.23, hz - 0.1, hz + 0.1, bevel=0.0)                # os 2 retalhos azuis do lado
    p.box('Team', -0.03, 0.13, 0.23, 0.245, hz - 0.1, hz + 0.1, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    p = Part(PREFIX + '_Barrel')
    x0 = TX + GUN_X0
    # (31/10, o detalhe AAA) o tubo num perfil girado (as transições cônicas, a luva térmica de trás, o evacuador abaulado com as
    # pontas inclinadas, a luva da frente, a boca mais grossa com o FURO de 155 mm), as braçadeiras de aço de borda redonda, os 2
    # anéis azuis em relevo e a referência da boca
    st = [(x0 - 0.02, 0.152), (x0 + 1.42, 0.145), (x0 + 1.47, 0.152), (x0 + 1.56, 0.176), (x0 + 1.88, 0.176), (x0 + 1.98, 0.13),
          (x0 + 2.02, 0.12), (x0 + 3.18, 0.118), (x0 + 3.24, 0.108), (MUZZLE_X - 0.1, 0.106), (MUZZLE_X - 0.08, 0.122),
          (MUZZLE_X - 0.01, 0.122), (MUZZLE_X, 0.114)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.078, bore_depth=0.4, bore_m='Dark')
    for xx, r in ((x0 + 0.05, 0.152), (x0 + 0.78, 0.149), (x0 + 1.45, 0.148), (x0 + 2.0, 0.124), (x0 + 2.7, 0.119), (x0 + 3.25, 0.108)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.012, seg=32)
    for xx, r in ((x0 + 2.45, 0.119), (x0 + 3.6, 0.107)):                                     # os 2 anéis azuis (cor do jogador)
        vp.ring_band(p, 'Team', xx - 0.05, xx + 0.05, r, GUN_Z, lip=0.008, seg=32)
    vp.rbox(p, 'Metal', (MUZZLE_X - 0.25, 0.0, GUN_Z + 0.16), (0.12, 0.07, 0.06), r=0.015, k=2)   # a referência da boca
    p.box('Metal', MUZZLE_X - 0.27, MUZZLE_X - 0.23, -0.02, 0.02, GUN_Z + 0.1, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    """(31/10, o detalhe AAA: model_vehicle_parts) as rodas de apoio duplas (a banda de borracha com o ombro redondo, o disco
    REBAIXADO, o cubo com o anel de porcas, o vão do dente-guia), a roda-guia de aço, a motriz com as coroas dentadas e os roletes."""
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            vp.road_wheel(p, (x, y, WHEEL_Z), WHEEL_R, 0.54, s, 'Paint', 'Metal', 'Rubber', gap=0.1, seg=24)
        ix, iz, ir = IDLER
        vp.idler(p, (ix, y, iz), ir, 0.54, s, 'Paint', 'Metal', gap=0.1)
        sx, sz, sr = SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.54, s, 'Paint', 'Metal', teeth=12, holes_m='Dark')
        for x in ROLLERS_X:
            vp.return_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.32, 'Metal', 'Rubber')
    return p.finish()


def track(side):
    """A esteira de elos com forma (model_vehicle_parts.track_links): as 2 almofadas com a garra, o conector do pino nas bordas
    (o ritmo de lado) e o dente-guia no vão das rodas duplas; a barriga entre os roletes de cima."""
    s = 1 if side == 'r' else -1
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.08), (IDLER[0], IDLER[1], IDLER[2] + 0.06)] + \
              [(x, WHEEL_Z, WHEEL_R + 0.06) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + 0.06) for x in ROLLERS_X]
    zt = ROLLER_Z + ROLLER_R + 0.06
    sags = [(ROLLERS_X[1] + 0.12, ROLLERS_X[0] - 0.12, zt, 0.03), (ROLLERS_X[2] + 0.12, ROLLERS_X[1] - 0.12, zt, 0.03)]
    return vp.track_links(f'{PREFIX}_Track_{side}', circles, TRACK_Y * s, TRACK_W, pitch=0.17, t_in=0.055, t_out=0.012, grouser=0.022,
                          guide='single', m='Track', guide_m='Metal', sags=sags, center_gap=0.13, horn_w=0.08)


# ---------------------------------------------------------------------------------- conferência e exportação
def tree(o, M):
    vs = [M @ v.co for v in o.data.vertices]
    return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])


def sweep_check(h, t, b, l):
    """A torre girando (com o cano e o laser) não pode atravessar o casco; o laser girando não pode bater na torre nem no cano."""
    ht = tree(h, Matrix.Identity(4))
    piv = V(TX, 0.0, 0.0)
    L = Matrix.Translation(V(*LASER_AT))
    hits = []
    for k in range(72):
        Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
        pairs = ht.overlap(tree(t, Y)) + ht.overlap(tree(b, Y)) + ht.overlap(tree(l, Y @ L))
        if pairs:
            cs = [h.data.polygons[i].center for i, j in pairs]
            box = tuple(round(f(c[ax] * (-1 if ax == 1 else 1) for c in cs), 2) for ax in range(3) for f in (min, max))
            hits.append((5 * k, len(pairs), box))
    print('MODEL giro da torre: %s' % ('sem colisao em 360 graus' if not hits else 'COLISAO em %s' % hits[:20]))
    tt, bt = tree(t, Matrix.Identity(4)), tree(b, Matrix.Identity(4))
    hits = []
    for k in range(36):
        M = L @ Matrix.Rotation(math.radians(10 * k), 4, 'Z')
        lt = tree(l, M)
        n = len(tt.overlap(lt)) + len(bt.overlap(lt))
        if n:
            hits.append((10 * k, n))
    print('MODEL giro do laser no teto: %s' % ('livre' if not hits else 'COLISAO em %s' % hits[:20]))


def bounds(objs):
    xs, ys, zs = [], [], []
    for o in objs:
        for v in o.data.vertices:
            xs.append(v.co.x)
            ys.append(-v.co.y)
            zs.append(v.co.z)
    return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def main():
    clear_scene()
    h = hull()
    t = turret()
    b = barrel()
    l = laser()
    w = wheels()
    tl, tr = track('l'), track('r')
    sweep_check(h, t, b, l)
    # (31/10) com o executor do detalhe (model_run_detail.py) as máscaras são cozidas depois do script, com as peças onde estão: o
    # laser fica NO LUGAR pro cozimento (antes era cozido na origem, dentro do casco) e só vai pra origem do eixo na hora de
    # exportar; a cópia da pose (só da prévia) não sai, senão entraria no cozimento em cima do laser
    import model_lib
    detail = model_lib.DETAIL.get('defer') is not None
    if detail:
        l.data.transform(Matrix.Translation(V(*LASER_AT)))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o is l and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-V(*LASER_AT)))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = 0
    for o in (h, t, b, l, w, tl, tr):
        total += export(o, OUT)
    # a pose da prévia: o laser no lugar dele (pose/ não vai pro jogo)
    c = l.copy()
    c.data = l.data.copy()
    bpy.context.scene.collection.objects.link(c)
    if not detail:
        c.data.transform(Matrix.Translation(V(*LASER_AT)))
    c.name = PREFIX + '_Laser_Placed'
    if not detail:
        export(c, POSE)
    print('MODEL total', total, 'tris (sem a pose)')
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr, c])
    (hx0, hx1), (hy0, hy1), _ = bounds([h])
    _, _, (_, tz1) = bounds([t])
    _, _, (_, lz1) = bounds([c])
    print('MODEL medidas: comprimento %.3f m com o cano (x %.3f..%.3f), casco %.3f m (x %.3f..%.3f), largura %.3f m, deck %.2f, teto da torre %.2f, o laser no alto %.3f, o mastro %.3f'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hx0, hx1, hy1 - hy0, DECK, T_Z2, lz1, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, '(%.2f a frente do pivo)' % (MUZZLE_X - TX))
    print('MODEL laser at_m (o eixo, no teto da torre, com a torre em repouso):', [round(v, 3) for v in LASER_AT],
          '(%.2f atras do pivo da torre); a lente do feixe a z %.3f' % (TX - LASER_AT[0], LASER_AT[2] + 0.42))
    print('MODEL tracks_m [bitola, largura]', [round(2 * TRACK_Y, 3), TRACK_W], 'contato x', [round(WHEELS_X[-1] - WHEEL_R, 2), round(WHEELS_X[0] + WHEEL_R, 2)])
    _, _, (_, turret_top) = bounds([t])
    print('MODEL turret_box_m (o corpo da torre, sem o mastro e as antenas): [[%.2f, %.2f, %.2f], [%.2f, %.2f, %.2f]]'
          % (TX - 2.74, -1.91, T_Z0 - 0.05, TX + 2.76, 1.91, T_Z2 + 0.6))


main()
