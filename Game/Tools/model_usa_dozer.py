"""O Dozer da USA provisório, modelado no Blender por script (sessão Modelos 3D, 13/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_dozer.py -- <pasta de saída>

A referência é a arte aprovada docs/concept/units/usa_dozer.webp (Dozer de mineração v2, 11/10; a ficha de
5 vistas ainda não existe): trator de mineração blindado na linguagem facetada do tanque USA, esteira em
triângulo com a roda motriz alta atrás (Cat D9/D11), lâmina maciça amarela de uma peça, escarificador grande
amarelo atrás, braço robótico amarelo dobrado sobre a traseira, duas chaminés de escape, corrimão amarelo
em volta do deck, e a cor do time como no tanque: a lona azul amarrada no deck de trás e a faixa azul
segmentada nas saias. Proporção da arte (e do C&C clássico): tão comprido quanto o tanque (8 m da lâmina
ao escarificador, o tanque USA tem 7,9 m de casco) e mais baixo que o mastro dele.

Peças (GLB separados, origem no chão, no centro do casco, frente em +X):
  SM_USA_Dozer_Hull (casco, cabine, chaminés, braço, lona, corrimão, escarificador, quadros das esteiras),
  SM_USA_Dozer_Blade (a lâmina com os braços de empurrar: a ORIGEM é o pivô dos braços, BLADE_PIVOT, que vai
  no Units.json como blade.at_m; o jogo gira a peça em volta dele quando o Dozer trabalha),
  SM_USA_Dozer_Beacon (a luz que gira dentro da sirene amarela do teto da cabine: origem no eixo, BEACON, que vai no
  Units.json como beacon.at_m; gira em Z a ~90 rpm e acende só enquanto o Dozer constrói; a cúpula âmbar fica no casco,
  slot Beacon, que pisca),
  SM_USA_Dozer_Wheels, SM_USA_Dozer_Track_l, SM_USA_Dozer_Track_r.
Materiais: Paint, Team, Yellow, Dark, Metal, Track, Lens, Beacon, Light (todos já na tabela do import_provisional.py).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, mat, select_only, track_path  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_dozer')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Dozer'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Track': (55, 52, 48), 'Lens': (30, 60, 110), 'Beacon': (255, 170, 40), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas
TRACK_Y, TRACK_W = 1.6, 0.7                 # centro e largura da esteira
SPROCKET = (-1.15, 1.28, 0.6)               # a roda motriz alta (x, z, raio): o topo da esteira a 1,96 m, sob o entalhe do para-lama
IDLER_F = (1.75, 0.5, 0.42)                 # roda-guia da frente
IDLER_R = (-1.95, 0.45, 0.38)               # roda-guia de trás
ROAD_X = [1.15, 0.5, -0.15, -0.8, -1.45]    # os roletes de baixo
BEACON = (0.45, 0.3, 3.25)                   # a sirene amarela: o eixo da luz que gira (beacon.at_m no Units.json, ANTES da escala)
ROAD_Z, ROAD_R = 0.27, 0.24
DECK = 2.05                                 # o deck do casco
SCALE = 0.88                                # a escala final de TODAS as peças (medida contra o tanque: tão comprido, mais baixo)
BLADE_PIVOT = (-0.4, 0.0, 0.75)             # o pivô dos braços da lâmina (blade.at_m no Units.json)


def hull():
    p = Part(PREFIX + '_Hull')
    # o casco de baixo (entre as esteiras) com o nariz em cunha avançando sobre a roda-guia, e os sobre-esteiras
    p.prism_xz('Paint', [(2.45, 1.05), (2.85, 1.3), (2.85, 1.55), (2.35, DECK), (-0.3, DECK), (-0.3, 1.05)], -1.15, 1.15)
    p.prism_xz('Dark', [(-0.3, 1.05), (-0.3, DECK), (-2.6, DECK), (-2.85, 1.9), (-2.85, 1.2), (-2.6, 1.05)], -1.16, 1.16, bevel=0.02)
    p.box('Dark', -2.3, 2.1, -1.22, 1.22, 0.35, 1.06, bevel=0.02)                                   # o trem de rodagem, escuro
    spon = [(2.85, 1.55), (2.35, DECK), (-2.6, DECK), (-2.85, 1.55), (-2.15, 1.55), (-1.95, 1.98), (-0.4, 1.98), (-0.3, 1.55)]
    p.prism_xz('Paint', spon, 1.15, 1.95)
    p.prism_xz('Paint', spon, -1.95, -1.15)
    # a placa rebitada do nariz e o para-choque
    p.obox('Dark', (2.62, 0.0, 1.78), (0.02, 2.3, 0.08), pitch=-48, bevel=0.0)
    p.box('Metal', 2.8, 2.95, -1.3, 1.3, 1.1, 1.3, bevel=0.02)
    # os quadros das esteiras (a viga que segura os roletes) e os suportes da motriz
    for s in (-1, 1):
        y = TRACK_Y * s
        p.box('Dark', -2.15, 1.95, y - 0.3, y + 0.3, 0.42, 0.98, bevel=0.02)
        p.box('Paint', -2.15, 1.95, y - 0.2, y + 0.2, 0.98, 1.1, bevel=0.02)
        p.box('Paint', -1.45, -0.85, y - 0.2, y + 0.2, 0.9, 1.55, bevel=0.03)
        p.box('Paint', 1.35, 1.8, y - 0.18, y + 0.18, 0.75, 1.05, bevel=0.02)
        # a saia sobre a esteira com a faixa azul segmentada (cor do jogador) e as placas rebitadas
        yo = (1.95 + 0.006) * s
        for i, (x0, x1) in enumerate(((-0.25, 0.65), (0.75, 1.65), (1.75, 2.55))):
            if i in (0, 1):
                p.box('Team', x0 + 0.08, x1 - 0.08, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.68, 1.88, bevel=0.004)
            p.box('Dark', x1 - 0.03, x1 + 0.03, min(yo, yo + 0.006 * s), max(yo, yo + 0.006 * s), 1.6, DECK - 0.03, bevel=0.0)
        p.box('Dark', -0.28, -0.22, min(yo, yo + 0.006 * s), max(yo, yo + 0.006 * s), 1.6, DECK - 0.03, bevel=0.0)
        # faróis no nariz e os ganchos
        p.box('Dark', 2.78, 2.9, 1.3 * s - 0.14, 1.3 * s + 0.14, 1.7, 1.9, bevel=0.01)
        p.obox('Lens', (2.91, 1.3 * s, 1.8), (0.02, 0.22, 0.14), bevel=0.0)
    # a cabine blindada à esquerda da frente: bloco com a frente em cunha, as janelas estreitas, a caixa do sensor e a antena
    cx0, cx1, cy0, cy1 = 0.2, 2.15, -1.15, 0.55
    p.prism_xz('Paint#hull', [(cx0, DECK), (cx1, DECK), (cx1, 2.45), (cx1 - 0.35, 2.9), (cx0, 2.9)], cy0, cy1, bevel=0.1)
    p.box('Paint', cx0 - 0.08, cx1 - 0.3, cy0 - 0.08, cy1 + 0.08, 2.9, 3.0, bevel=0.03)         # o teto
    p.box('Paint', cx0 + 0.15, cx0 + 0.85, -0.35, 0.35, 3.0, 3.16, bevel=0.03)                    # a escotilha saliente
    p.obox('Team', (cx1 - 0.08, cy0 + 0.14, 2.62), (0.02, 0.2, 0.5), pitch=-55, bevel=0.0)         # a faixa azul do canto
    p.box('Team', cx0 + 0.3, cx0 + 0.9, cy0 - 0.012, cy0, 2.12, 2.3, bevel=0.0)
    p.obox('Lens', (cx1 - 0.17, -0.65, 2.68), (0.02, 0.55, 0.34), pitch=-55, bevel=0.0)            # as janelas da frente
    p.obox('Lens', (cx1 - 0.17, 0.05, 2.68), (0.02, 0.55, 0.34), pitch=-55, bevel=0.0)
    p.obox('Dark', (cx1 - 0.175, -0.3, 2.68), (0.025, 0.08, 0.38), pitch=-55, bevel=0.0)
    p.box('Lens', 0.5, 1.3, cy0 - 0.02, cy0, 2.4, 2.78, bevel=0.0)                                 # a janela de lado
    p.box('Dark', 0.86, 0.94, cy0 - 0.025, cy0, 2.4, 2.78, bevel=0.0)
    p.box('Paint', 0.4, 0.95, -0.95, -0.45, 3.0, 3.3, bevel=0.02)                                   # a caixa do sensor
    p.box('Lens', 0.95, 0.97, -0.88, -0.52, 3.07, 3.23, bevel=0.0)
    p.cyl('Dark', (1.15, 0.2, 3.05), 'z', 0.05, 0.1, seg=10, bevel=0.0)
    p.cyl('Metal', (1.15, 0.2, 3.65), 'z', 0.012, 1.2, seg=6, bevel=0.0)
    p.box('Paint', cx1 - 0.3, cx1 + 0.3, 0.7, 1.9, DECK, DECK + 0.3, bevel=0.03)                   # o degrau à direita da cabine
    # o bloco do motor atrás da cabine, com a grade do radiador à direita e as venezianas atrás
    ex0, ex1, ey0, ey1, ez = -1.7, cx0, -1.4, 1.4, 2.75
    p.loft('Paint', [[(ex0, ey0, DECK), (ex0, ey1, DECK), (ex1, ey1, DECK), (ex1, ey0, DECK)],
                     [(ex0 + 0.05, ey0 + 0.05, DECK + 0.5), (ex0 + 0.05, ey1 - 0.05, DECK + 0.5), (ex1 - 0.05, ey1 - 0.05, DECK + 0.5), (ex1 - 0.05, ey0 + 0.05, DECK + 0.5)],
                     [(ex0 + 0.2, ey0 + 0.25, ez), (ex0 + 0.2, ey1 - 0.25, ez), (ex1 - 0.1, ey1 - 0.25, ez), (ex1 - 0.1, ey0 + 0.25, ez)]], bevel=0.05)
    p.box('Paint', ex0 + 0.3, ex1 - 0.2, ey0 + 0.35, ey1 - 0.35, ez, ez + 0.12, bevel=0.03)
    p.obox('Dark', (-0.75, ey1 - 0.12, DECK + 0.38), (1.1, 0.02, 0.5), roll=-16, bevel=0.0)             # a grade do radiador (na face inclinada)
    for k in range(9):
        p.obox('Dark', (-1.25 + k * 0.125, ey1 - 0.09, DECK + 0.4), (0.03, 0.04, 0.46), roll=-16, yaw=30, bevel=0.0)
    for k in range(6):
        p.obox('Dark', (ex0 + 0.03 + k * 0.02, 0.0, DECK + 0.2 + k * 0.1), (0.03, 2.0, 0.04), pitch=-30, bevel=0.0)
    p.box('Paint', -0.9, -0.2, -0.5, 0.5, ez + 0.12, ez + 0.32, bevel=0.02)                          # a tampa do motor
    # as duas chaminés de escape à esquerda, com o cotovelo e a tela
    for y in (-0.95, -0.4):
        p.cyl('Dark', (-1.2, y, ez + 0.5), 'z', 0.14, 1.1, seg=14, bevel=0.01, smooth=True)
        p.cyl('Metal', (-1.2, y, ez + 0.95), 'z', 0.16, 0.1, seg=14, bevel=0.0)
        p.cyl('Metal', (-1.2, y, ez + 0.2), 'z', 0.17, 0.12, seg=14, bevel=0.0)
        p.cyl('Dark', (-1.2, y, ez + 1.12), (-1.0, 0.0, 0.6), 0.14, 0.3, seg=12, bevel=0.0)       # o cotovelo
        p.cyl('Dark', (-1.42, y, ez + 1.22), 'x', 0.15, 0.1, seg=12, bevel=0.0)
    # o deck de trás com a lona azul amarrada (cor do jogador) e o corrimão amarelo
    p.box('Paint', -2.75, ex0, -1.2, 1.2, DECK, DECK + 0.08, bevel=0.02)
    p.loft('Team', [[(-2.65, -0.95, DECK + 0.08), (-2.65, 0.95, DECK + 0.08), (-1.75, 0.95, DECK + 0.08), (-1.75, -0.95, DECK + 0.08)],
                    [(-2.67, -0.98, DECK + 0.3), (-2.67, 0.98, DECK + 0.33), (-1.73, 0.98, DECK + 0.31), (-1.73, -0.98, DECK + 0.3)],
                    [(-2.55, -0.85, DECK + 0.55), (-2.55, 0.85, DECK + 0.58), (-1.85, 0.85, DECK + 0.53), (-1.85, -0.85, DECK + 0.56)]],
           bevel=0.04)
    for y in (-0.5, 0.5):
        p.box('Dark', -2.7, -1.7, y - 0.03, y + 0.03, DECK + 0.1, DECK + 0.6, bevel=0.004)
    rail = [(-0.2, -1.3), (-2.8, -1.3), (-2.8, 1.3), (-0.2, 1.3)]
    for (x0, y0), (x1, y1) in zip(rail[:-1], rail[1:]):
        for z in (DECK + 0.55, DECK + 1.0):
            p.obox('Yellow', ((x0 + x1) / 2, (y0 + y1) / 2, z), (abs(x1 - x0) + 0.04 if x0 != x1 else 0.04, abs(y1 - y0) + 0.04 if y0 != y1 else 0.04, 0.04), bevel=0.0)
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.9))
        for k in range(n + 1):
            u = k / n
            p.box('Yellow', x0 + (x1 - x0) * u - 0.02, x0 + (x1 - x0) * u + 0.02, y0 + (y1 - y0) * u - 0.02, y0 + (y1 - y0) * u + 0.02, DECK + 0.08, DECK + 1.0, bevel=0.0)
    # o braço robótico dobrado à direita: base, 2 segmentos amarelos, cilindros, a garra na ponta
    bx, by, bz = -0.5, 1.0, DECK + 0.3
    p.cyl('Yellow', (bx, by, DECK + 0.15), 'z', 0.42, 0.3, seg=20, bevel=0.02)
    p.box('Yellow', bx - 0.3, bx + 0.3, by - 0.25, by + 0.25, DECK + 0.3, DECK + 0.75, bevel=0.03)
    elbow = (-2.4, by, 3.5)
    tip = (-3.4, by - 0.25, 2.3)
    segA = Vector(elbow) - Vector((bx, by, bz + 0.45))
    segB = Vector(tip) - Vector(elbow)
    for (a, d) in (((bx, by, bz + 0.45), segA), (elbow, segB)):
        c = Vector(a) + d / 2
        L = d.length
        yaw = math.degrees(math.atan2(d.y, d.x))
        pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
        p.obox('Yellow', (c.x, c.y, c.z), (L, 0.4, 0.46), yaw=yaw, pitch=pitch, bevel=0.04)
        p.obox('Dark', (c.x, c.y + 0.0, c.z), (L * 0.8, 0.42, 0.06), yaw=yaw, pitch=pitch, bevel=0.0)
    p.cyl('Yellow', (elbow[0], elbow[1], elbow[2]), 'y', 0.34, 0.5, seg=16, bevel=0.02)
    p.cyl('Dark', (elbow[0], elbow[1], elbow[2]), 'y', 0.1, 0.46, seg=12, bevel=0.0)
    for (a, b, r) in (((bx + 0.4, by + 0.25, bz + 0.2), (elbow[0] + 0.9, by + 0.25, elbow[2] - 0.25), 0.07),
                      ((elbow[0] - 0.3, by - 0.28, elbow[2] - 0.2), (elbow[0] + (tip[0] - elbow[0]) * 0.6, tip[1] - 0.03, elbow[2] + (tip[2] - elbow[2]) * 0.6 + 0.15), 0.06)):
        d = Vector(b) - Vector(a)
        c = Vector(a) + d / 2
        p.cyl('Metal', (c.x, c.y, c.z), (d.x, d.y, d.z), r, d.length, seg=10, bevel=0.0)
        p.cyl('Yellow', (a[0] + d.x * 0.3, a[1] + d.y * 0.3, a[2] + d.z * 0.3), (d.x, d.y, d.z), r + 0.03, d.length * 0.4, seg=10, bevel=0.0)
    p.obox('Yellow', (tip[0] - 0.1, tip[1], tip[2] - 0.25), (0.5, 0.3, 0.34), pitch=60, bevel=0.03)    # a garra, de ponta pra baixo
    p.obox('Dark', (tip[0] - 0.05, tip[1], tip[2] - 0.55), (0.3, 0.24, 0.14), pitch=80, bevel=0.0)
    # o escarificador atrás: a mesa amarela, os dois braços grossos (paralelogramo), o dente curvo maciço e o cilindro
    p.box('Yellow', -3.05, -2.75, -0.7, 0.7, 1.0, 1.7, bevel=0.04)
    for s in (-1, 1):
        for z in (1.15, 1.6):
            p.obox('Yellow', (-3.3, 0.5 * s, z + 0.15), (1.1, 0.22, 0.28), pitch=22, bevel=0.03)
        p.cyl('Metal', (-2.95, 0.5 * s, 1.15), 'y', 0.12, 0.3, seg=12, bevel=0.0)
    p.box('Yellow', -4.0, -3.5, -0.55, 0.55, 1.45, 2.25, bevel=0.05)                                   # o bloco do dente
    for k, (x, z, pitch) in enumerate(((-3.8, 1.3, 85), (-3.95, 0.95, 65), (-4.15, 0.62, 40), (-4.35, 0.42, 15))):
        p.obox('Yellow' if k < 3 else 'Dark', (x, 0.0, z), (0.5, 0.42 - k * 0.06, 0.4), pitch=pitch, bevel=0.03)
    d = Vector((-3.6, 0.0, 2.2)) - Vector((-2.3, 0.0, 2.7))
    p.cyl('Metal', (-2.95, 0.0, 2.45), (d.x, d.y, d.z), 0.08, d.length, seg=10, bevel=0.0)
    p.cyl('Yellow', (-2.65, 0.0, 2.58), (d.x, d.y, d.z), 0.12, 0.6, seg=10, bevel=0.0)
    # a sirene amarela (pedido do Felipe, 13/10): no canto de trás do teto da cabine, num mastro curto com a gaiola;
    # a cúpula âmbar fica aqui (slot Beacon: emissiva, pisca); a luz que gira é a peça SM_USA_Dozer_Beacon
    bxp, byp, bzp = BEACON
    p.cyl('Dark', (bxp, byp, bzp - 0.2), 'z', 0.05, 0.12, seg=10, bevel=0.0)                        # o mastro
    p.cyl('Dark', (bxp, byp, bzp - 0.12), 'z', 0.13, 0.05, seg=16, bevel=0.005)                     # a base
    p.cyl('Beacon', (bxp, byp, bzp + 0.03), 'z', 0.1, 0.24, seg=16, bevel=0.0, smooth=True)         # a cúpula âmbar
    p.cyl('Dark', (bxp, byp, bzp + 0.165), 'z', 0.105, 0.03, seg=16, bevel=0.0)                     # a tampa
    for k in range(3):                                                                              # a gaiola
        ang = math.radians(k * 120 + 30)
        p.cyl('Metal', (bxp + 0.135 * math.cos(ang), byp + 0.135 * math.sin(ang), bzp + 0.02), 'z', 0.012, 0.3, seg=6, bevel=0.0)
    p.torus('Metal', (bxp, byp, bzp + 0.1), 'z', 0.135, 0.012, seg=16, rseg=6)
    # os cilindros de levantar a lâmina (presos no casco; a lâmina leva a outra ponta)
    for s in (-1, 1):
        a = Vector((1.9, 0.95 * s, DECK + 0.05))
        d = Vector((3.1, 1.4 * s, 1.75)) - a
        p.cyl('Metal', (a.x + d.x * 0.5, a.y + d.y * 0.5, a.z + d.z * 0.5), (d.x, d.y, d.z), 0.09, d.length, seg=12, bevel=0.0)
        p.cyl('Yellow', (a.x + d.x * 0.28, a.y + d.y * 0.28, a.z + d.z * 0.28), (d.x, d.y, d.z), 0.15, d.length * 0.5, seg=12, bevel=0.01)
        p.box('Yellow', 1.6, 2.15, 0.95 * s - 0.22, 0.95 * s + 0.22, DECK, DECK + 0.35, bevel=0.03)
        p.cyl('Metal', (1.9, 0.95 * s, DECK + 0.2), 'y', 0.16, 0.5, seg=12, bevel=0.0)
    return p.finish()


def blade():
    p = Part(PREFIX + '_Blade')
    # a lâmina maciça: a face côncava virada pra FRENTE (+X), o fio de corte embaixo e o lábio em cima avançados,
    # o meio recuado; as costas (lado do casco) retas com as nervuras; 1,9 m de altura, 0,7 m de fundo na base
    face = [(3.95, 0.05), (3.7, 0.45), (3.55, 0.95), (3.6, 1.45), (3.85, 1.95)]
    back = [(3.2, 1.95), (3.0, 1.5), (3.0, 0.9), (3.15, 0.4), (3.3, 0.05)]
    p.prism_xz('Yellow', face + list(reversed(back)), -2.1, 2.1, bevel=0.035)
    p.obox('Dark', (3.96, 0.0, 0.09), (0.1, 4.2, 0.16), pitch=-35, bevel=0.0)                        # o fio de corte
    p.box('Dark', 3.15, 3.9, -2.1, 2.1, 1.95, 2.03, bevel=0.0)                                        # o lábio de cima
    for y in (-1.5, -0.5, 0.5, 1.5):                                                                  # as nervuras atrás
        p.box('Yellow', 2.88, 3.12, y - 0.08, y + 0.08, 0.3, 1.8, bevel=0.015)
    p.box('Yellow', 2.88, 3.08, -1.85, 1.85, 1.0, 1.3, bevel=0.015)
    for s in (-1, 1):                                                                                 # as abas laterais
        p.prism_xz('Yellow', [(4.05, 0.05), (3.8, 0.45), (3.65, 0.95), (3.7, 1.45), (3.95, 1.95), (3.05, 1.95), (3.05, 0.05)],
                   2.1 * s, (2.1 + 0.1) * s, bevel=0.012)
        # os braços de empurrar (vigas grossas), das costas da lâmina ao pivô no quadro da esteira, com os mancais
        a = Vector((3.1, 1.55 * s, 0.7))
        b = Vector((BLADE_PIVOT[0], 1.6 * s, BLADE_PIVOT[2]))
        d = a - b
        c = b + d / 2
        p.obox('Yellow', (c.x, c.y, c.z), (d.length, 0.4, 0.5), yaw=math.degrees(math.atan2(d.y, d.x)),
               pitch=math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), bevel=0.04)
        p.cyl('Metal', (b.x, b.y, b.z), 'y', 0.26, 0.5, seg=16, bevel=0.015)
        p.cyl('Dark', (b.x, b.y, b.z), 'y', 0.1, 0.56, seg=12, bevel=0.0)
        p.box('Yellow', 2.85, 3.3, 1.55 * s - 0.25, 1.55 * s + 0.25, 0.4, 1.0, bevel=0.03)
        # a ponta de cima do cilindro de levantar
        p.box('Yellow', 2.95, 3.3, 1.4 * s - 0.14, 1.4 * s + 0.14, 1.55, 1.9, bevel=0.02)
    o = p.finish()
    o.data.transform(Matrix.Translation(-V(*BLADE_PIVOT)))           # a origem no pivô (o jogo gira a lâmina em volta dele)
    return o


def beacon():
    """A luz que gira dentro da cúpula da sirene: o refletor (meia-concha) e a lâmpada, com a ORIGEM no eixo
    (BEACON no Units.json como beacon.at_m); o jogo gira em Z (~90 rpm) e acende só enquanto o Dozer constrói."""
    p = Part(PREFIX + '_Beacon')
    p.cyl('Metal', (0.0, 0.0, 0.0), 'z', 0.03, 0.04, seg=10, bevel=0.0)
    p.obox('Metal', (-0.04, 0.0, 0.07), (0.02, 0.1, 0.12), bevel=0.0)                                # o refletor
    p.obox('Light', (-0.015, 0.0, 0.07), (0.03, 0.06, 0.08), bevel=0.0)                              # a lâmpada (emissiva)
    return p.finish()


def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        # a roda motriz alta (dentes), as duas rodas-guia e os roletes
        p.cyl('Metal', (SPROCKET[0], y, SPROCKET[1]), 'y', SPROCKET[2] - 0.04, 0.5, seg=28, bevel=0.015)
        p.cyl('Paint', (SPROCKET[0], y, SPROCKET[1]), 'y', 0.3, 0.56, seg=20, bevel=0.01)
        p.cyl('Dark', (SPROCKET[0], y, SPROCKET[1]), 'y', 0.12, 0.6, seg=12, bevel=0.0)
        for k in range(18):
            a = math.radians(k * 20)
            rr = SPROCKET[2] + 0.02
            p.obox('Metal', (SPROCKET[0] + rr * math.cos(a), y, SPROCKET[1] + rr * math.sin(a)), (0.09, 0.46, 0.08),
                   pitch=-math.degrees(a), bevel=0.0)
        for (x, z, r) in (IDLER_F, IDLER_R):
            p.cyl('Paint', (x, y, z), 'y', r, 0.5, seg=24, bevel=0.015)
            p.cyl('Dark', (x, y, z), 'y', r - 0.1, 0.54, seg=20, bevel=0.0)
            p.cyl('Metal', (x, y, z), 'y', 0.09, 0.6, seg=12, bevel=0.0)
        for x in ROAD_X:
            p.cyl('Dark', (x, y, ROAD_Z), 'y', ROAD_R, 0.46, seg=18, bevel=0.01)
            p.cyl('Metal', (x, y, ROAD_Z), 'y', 0.07, 0.5, seg=10, bevel=0.0)
    return p.finish()


def track(side):
    s = 1 if side == 'r' else -1
    p = Part(f'{PREFIX}_Track_{side}')
    y = TRACK_Y * s
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.08), (IDLER_F[0], IDLER_F[1], IDLER_F[2] + 0.08),
               (IDLER_R[0], IDLER_R[1], IDLER_R[2] + 0.08)] + [(x, ROAD_Z, ROAD_R + 0.08) for x in ROAD_X]
    for (x, z), ang in track_path(circles, 0.2):
        deg = math.degrees(ang)
        nx, nz = math.sin(ang), -math.cos(ang)
        p.obox('Track', (x, y, z), (0.205, TRACK_W, 0.09), pitch=deg, bevel=0.006)                # a sapata (sem vão)
        p.obox('Track', (x + nx * 0.06, y, z + nz * 0.06), (0.06, TRACK_W * 0.96, 0.035), pitch=deg, bevel=0.0)   # a garra
        p.obox('Metal', (x - nx * 0.06, y, z - nz * 0.06), (0.1, 0.12, 0.05), pitch=deg, bevel=0.0)              # a guia
    return p.finish()


def main():
    clear_scene()
    only = os.environ.get('MODEL_ONLY')
    builds = {'hull': hull, 'blade': blade, 'beacon': beacon, 'wheels': wheels, 'track_l': lambda: track('l'), 'track_r': lambda: track('r')}
    total = 0
    for key, build in builds.items():
        if only and key not in only.split(','):
            continue
        o = build()
        o.data.transform(Matrix.Scale(SCALE, 4))
        total += export(o, OUT)
        bpy.data.objects.remove(o, do_unlink=True)
    print('MODEL total', total, 'tris; escala', SCALE)
    print('MODEL blade.at_m', tuple(round(v * SCALE, 3) for v in BLADE_PIVOT), 'beacon.at_m', tuple(round(v * SCALE, 3) for v in BEACON))


main()
