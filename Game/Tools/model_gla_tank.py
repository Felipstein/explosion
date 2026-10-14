"""O tanque do GLA provisório, modelado no Blender por script (sessão Modelos 3D, 13/10/2026).

Rodar (Blender sem janela, só CPU; não usa a placa):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_tank.py -- <pasta de saída>

Padrão dos provisórios (o do tanque USA do Look, Game/Tools/look_tank.py): quina dura com chanfro, normais
duras, nunca remesh nem ruído; uma cor por material (o pó do mundo entra por cima no jogo); sem desgaste
(decisão do Felipe, 13/10: provisório bonito, não final).

A planta é a ficha aprovada docs/concept/units/gla_mbt_sheet.webp (5 vistas: frente, cima, lado, trás, 3/4).
O tanque da ficha é um T-55/Tipo 59 com o kit do GLA, então as medidas são as do T-55 de verdade (a régua
do jogo é o tanque USA, 7,9 m de casco; o do GLA é menor, como o Scorpion do Zero Hour é menor que o Crusader):
- casco 6,45 m × 3,27 m, barriga a 0,43 m do chão, para-lamas a 1,10 m, deck a 1,55 m, teto da torre a 2,40 m;
- 5 rodas grandes por lado (81 cm), sem roletes de retorno, roda-guia na frente, roda motriz atrás, esteira de 58 cm;
- torre em cúpula (anel de 1,9 m, bojo de 2,25 m), mantelete e o canhão de 100 mm: ponta a 5,8 m na frente
  (9,0 m de comprimento total com o cano, como o T-55); holofote grande à direita do mantelete.
- o kit do GLA da ficha: sacos de areia (glacis, bordas do casco, meia-lua na torre), gaiola de barras nas
  laterais (sobre as rodas), chapas remendadas (teal e ferrugem) na torre e na lateral da frente, caixas e
  galões nos para-lamas, tanques de combustível externos à direita, lona enrolada, cesto atrás da torre com
  carga, pneu sobressalente amarrado na traseira, metralhadora pesada (DShK) na torre, antena, e a cor do
  jogador: as faixas verdes (uma no glacis, uma ao longo da torre e uma em diagonal), o anel verde no cano e
  as duas bandeiras (a alta na torre, a baixa no para-lama de trás).

Materiais (cada nome vira um slot na Unreal; a importação escolhe a instância pelo nome):
  Paint (areia-cáqui do GLA), Team (a cor do jogador), Bag (sacos de areia, juta), Patch (chapa teal),
  Rust (chapa enferrujada), Metal, Dark, Rubber (pneus), Track (esteira), Wood (caixotes), Lens (vidro).
Peças em GLBs separados, todas com a origem do tanque (no chão, no centro do casco): a torre gira em
volta de (TX, 0), o cano recua ao longo de +X. Eixos: Unreal X pra frente, Y pra direita, Z pra cima;
o Blender recebe (x, −y, z), como no look_tank.py.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, mat, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_tank')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_GLA_MBT'


COLORS.update({'Paint': (196, 178, 128), 'Team': (78, 152, 58), 'Bag': (212, 198, 160), 'Patch': (126, 158, 148),
               'Rust': (122, 78, 48), 'Metal': (92, 90, 82), 'Dark': (40, 40, 38), 'Rubber': (32, 32, 31),
               'Track': (58, 55, 50), 'Wood': (118, 104, 72), 'Lens': (18, 26, 40)})


# ---------------------------------------------------------------------------------- medidas
NOSE, REAR = 3.25, -3.2          # o casco: 6,45 m
BELLY, FENDER, DECK = 0.43, 1.10, 1.55
LOW, UP = 0.98, 1.635            # meia-largura do casco de baixo (entre as esteiras) e de cima (com os para-lamas)
TRACK_Y, TRACK_W = 1.30, 0.58
WHEELS_X = [2.10, 1.05, 0.12, -0.80, -1.72]
WHEEL_R, WHEEL_Z = 0.405, 0.47
IDLER = (2.98, 0.68, 0.30)       # (x, z, raio) roda-guia
SPROCKET = (-2.72, 0.62, 0.32)   # roda motriz
TX = 0.35                        # o eixo da torre
GUN_Z = 2.05
# a cúpula da torre: (distância ao eixo, altura acima do deck), do anel até o teto
DOME = [(0.97, 0.0), (1.07, 0.10), (1.12, 0.25), (1.09, 0.42), (0.98, 0.57), (0.80, 0.70), (0.58, 0.79), (0.34, 0.845), (0.12, 0.862)]


def glacis_x(z):
    """O glacis: do nariz (3,25; 1,0) até o deck (2,3; 1,55), 30° de inclinação."""
    return NOSE - (z - 1.0) * (0.95 / 0.55)


DOME_TOP = [(0.0, 0.875)] + [(d, z) for d, z in reversed(DOME) if z >= 0.25]   # só a parte de cima, do teto ao bojo


def dome_z(d):
    """A altura do teto da cúpula (acima do deck) a uma distância d do eixo (a parte de cima, d ≤ 1,12)."""
    top = DOME_TOP
    for (d0, z0), (d1, z1) in zip(top[:-1], top[1:]):
        if d0 <= d <= d1:
            u = (d - d0) / (d1 - d0)
            return z0 + (z1 - z0) * u
    return top[-1][1]


def dome_point(az, d):
    """Ponto na cúpula (Unreal) no azimute az (graus, +X→+Y) e distância d, mais a normal e a inclinação."""
    a = math.radians(az)
    z = DECK + dome_z(d)
    dz = (dome_z(min(d + 0.01, 1.12)) - dome_z(max(d - 0.01, 0.0))) / 0.02      # dz/dd (negativo: desce pra fora)
    n = Vector((-dz * math.cos(a), -dz * math.sin(a), 1.0)).normalized()
    phi = math.degrees(math.acos(n.z))                                          # ângulo da normal com a vertical
    return Vector((TX + d * math.cos(a), d * math.sin(a), z)), n, phi


def on_dome(p, m, az, d, size, lift=0.0, bevel=None, yaw_extra=0.0):
    """Uma caixa deitada na cúpula: o x local pra fora (descendo a encosta), o z local na normal."""
    pos, n, phi = dome_point(az, d)
    c = pos + n * (size[2] / 2 + lift)
    p.obox(m, (c.x, c.y, c.z), size, yaw=az + yaw_extra, pitch=-phi, bevel=bevel)


def strip_on_dome(p, m, path, width, thick=0.012, step=0.05, lift=0.004):
    """Uma faixa (ou chapa) conformada ao teto da cúpula: path = [(x, y), ...] no plano, relativo ao tanque."""
    pts = []
    for (x0, y0), (x1, y1) in zip(path[:-1], path[1:]):
        L = math.hypot(x1 - x0, y1 - y0)
        n = max(2, int(L / step) + 1)
        for k in range(n + 1):
            if pts and k == 0:
                continue
            u = k / n
            pts.append((x0 + (x1 - x0) * u, y0 + (y1 - y0) * u))
    rings = []
    for i, (x, y) in enumerate(pts):
        if i < len(pts) - 1:
            dx, dy = pts[i + 1][0] - x, pts[i + 1][1] - y
        else:
            dx, dy = x - pts[i - 1][0], y - pts[i - 1][1]
        L = math.hypot(dx, dy) or 1.0
        tx, ty = -dy / L, dx / L
        ring = []
        for sgn, h in ((-1, 0), (1, 0), (1, 1), (-1, 1)):
            px, py = x + tx * width / 2 * sgn, y + ty * width / 2 * sgn
            d = min(math.hypot(px - TX, py), 1.12)
            az = math.degrees(math.atan2(py, px - TX))
            pos, nrm, _ = dome_point(az, d)
            q = pos + nrm * (lift + h * thick)
            ring.append((q.x, q.y, q.z))
        rings.append(ring)
    p.loft(m, rings, bevel=0.0)


def flag(p, pole, top, length, height, trail=-1.0, phase=0.0):
    """Bandeira na cor do jogador: pano com onda (loft de seções finas), preso no mastro em (x, y), do topo pra baixo.
    trail −1: o pano vai pra trás (−X), como quando o tanque anda."""
    rings = []
    n = 9
    for k in range(n + 1):
        u = k / n
        x = pole[0] + trail * length * u
        wave = 0.11 * math.sin(2 * math.pi * u * 1.1 + phase) * u
        y = pole[1] + wave
        h = height * (1 - 0.1 * u)
        z1 = top - 0.06 * u * u
        z0 = z1 - h
        t = 0.006
        rings.append([(x, y - t, z0), (x, y + t, z0), (x, y + t, z1), (x, y - t, z1)])
    p.loft('Team', rings, bevel=0.0, smooth=True)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = Part(PREFIX + '_Hull')
    # o casco de baixo (entre as esteiras) e os sobre-esteiras (full width, do para-lama ao deck), com o glacis
    low = [(2.95, BELLY), (NOSE, 1.0), (2.3, DECK), (-3.0, DECK), (REAR, 1.45), (REAR, 0.6), (-3.0, BELLY)]
    p.prism_xz('Paint', low, -LOW, LOW)
    spon = [(glacis_x(FENDER), FENDER), (2.3, DECK), (-3.0, DECK), (REAR, 1.45), (REAR, FENDER)]
    p.prism_xz('Paint', spon, LOW, UP)
    p.prism_xz('Paint', spon, -UP, -LOW)
    # os para-lamas da frente (sobre a roda-guia, com a aba dobrada) e de trás
    for s in (-1, 1):
        y0, y1 = (LOW + 0.02, UP) if s > 0 else (-UP, -LOW - 0.02)
        p.box('Paint', glacis_x(FENDER) - 0.05, 3.55, y0, y1, FENDER - 0.04, FENDER + 0.01, bevel=0.01)
        p.obox('Paint', (3.68, 1.33 * s, FENDER - 0.1), (0.3, UP - LOW - 0.04, 0.03), pitch=-40, bevel=0.01)
        p.box('Paint', -3.45, REAR + 0.05, y0, y1, FENDER - 0.04, FENDER + 0.01, bevel=0.01)
        p.obox('Paint', (-3.55, 1.33 * s, FENDER - 0.1), (0.25, UP - LOW - 0.04, 0.03), pitch=40, bevel=0.01)
    # o deck do motor: painel elevado com as duas grades (aletas) e a grade transversal de trás
    p.box('Paint', -3.0, -1.35, -1.5, 1.5, DECK, DECK + 0.06)
    for s in (-1, 1):
        y0, y1 = (0.28, 1.35) if s > 0 else (-1.35, -0.28)
        p.box('Dark', -2.75, -1.55, y0, y1, DECK + 0.06, DECK + 0.075, bevel=0.0)
        for k in range(8):                                                           # as aletas da grade
            p.obox('Dark', (-2.68 + k * 0.155, 0.815 * s, DECK + 0.095), (0.035, y1 - y0 - 0.06, 0.04), pitch=35, bevel=0.0)
    p.box('Dark', -3.0, -2.82, -1.25, 1.25, DECK + 0.06, DECK + 0.075, bevel=0.0)      # a grade transversal de trás
    for k in range(7):
        p.obox('Dark', (-2.98 + k * 0.025, 0.0, DECK + 0.09), (0.02, 2.4, 0.045), pitch=-35, bevel=0.0)
    # o anel da torre (a base escura) e a escotilha do motorista (à esquerda, no glacis alto)
    p.cyl('Dark', (TX, 0.0, DECK + 0.015), 'z', 1.0, 0.03, seg=40, bevel=0.0)
    p.cyl('Paint', (2.05, -0.62, DECK + 0.03), 'z', 0.31, 0.06, seg=24)
    p.box('Dark', 2.22, 2.34, -0.78, -0.48, DECK + 0.03, DECK + 0.1, bevel=0.005)      # os periscópios
    # faróis (à direita, no glacis, como na vista de frente da ficha) com a guarda de barras
    for (y, r) in ((1.18, 0.1), (0.92, 0.075)):
        z = 1.28
        p.cyl('Metal', (glacis_x(z) + 0.06, y, z), 'x', r, 0.2, seg=16)
        p.cyl('Lens', (glacis_x(z) + 0.165, y, z), 'x', r - 0.02, 0.02, seg=16, bevel=0.0)
    p.obox('Metal', (glacis_x(1.28) + 0.2, 1.05, 1.4), (0.025, 0.5, 0.025), bevel=0.0)
    p.obox('Metal', (glacis_x(1.28) + 0.2, 1.05, 1.16), (0.025, 0.5, 0.025), bevel=0.0)
    for y in (1.3, 0.8):
        p.obox('Metal', (glacis_x(1.28) + 0.14, y, 1.28), (0.14, 0.025, 0.3), pitch=-30, bevel=0.0)
    # ganchos de reboque na frente e atrás
    for s in (-1, 1):
        p.box('Metal', NOSE - 0.02, NOSE + 0.12, 0.75 * s - 0.07, 0.75 * s + 0.07, 0.62, 0.78, bevel=0.01)
        p.box('Metal', REAR - 0.12, REAR + 0.02, 0.85 * s - 0.07, 0.85 * s + 0.07, 0.75, 0.9, bevel=0.01)
    # a faixa verde (cor do jogador): placa de baixo, glacis e o deck até a torre
    p.obox('Team', (3.11, 0.0, 0.72), (0.6, 0.2, 0.012), pitch=62, bevel=0.003)
    p.obox('Team', (glacis_x(1.275) + 0.005, 0.0, 1.28), (1.06, 0.2, 0.012), pitch=-30, bevel=0.003)
    p.box('Team', 1.33, 2.3, -0.1, 0.1, DECK, DECK + 0.012, bevel=0.003)
    # sacos de areia no pé do glacis (dos dois lados da faixa), o monte no canto da frente-direita (a ficha), os da
    # frente-esquerda e os dos cantos de trás
    for s in (-1, 1):
        for (y, z, L) in ((0.45, 1.1, 0.46), (0.92, 1.1, 0.5), (1.38, 1.12, 0.42), (0.68, 1.3, 0.48), (1.16, 1.3, 0.46)):
            p.obox('Bag', (glacis_x(z) + 0.1, y * s, z + 0.02), (0.3, L, 0.2), pitch=-30, bevel=0.05)
    for (x, y, yaw) in ((2.15, 1.15, 0), (2.15, 1.5, 0), (1.7, 1.15, 0), (1.7, 1.5, 0), (1.25, 1.33, 90)):
        p.obox('Bag', (x, y, DECK + 0.1), (0.42, 0.3, 0.2), yaw=yaw, bevel=0.05)
    for (x, y, yaw) in ((1.95, 1.33, 90), (1.5, 1.33, 90)):
        p.obox('Bag', (x, y, DECK + 0.29), (0.42, 0.3, 0.19), yaw=yaw, bevel=0.05)
    for (x, y, yaw) in ((2.5, -1.38, 0), (2.05, -1.38, 0)):
        p.obox('Bag', (x, y, DECK + 0.1), (0.42, 0.3, 0.2), yaw=yaw, bevel=0.05)
    for (x, y, yaw) in ((-2.55, -1.38, 0), (-3.0, -1.3, 0), (-2.8, -1.35, 90), (-2.55, 1.36, 0)):
        z = DECK + 0.1 if yaw == 0 else DECK + 0.29
        p.obox('Bag', (x, y, z), (0.44, 0.3, 0.2), yaw=yaw, bevel=0.05)
    # as chapas remendadas da lateral da frente (teal e ferrugem, sobre as rodas da frente)
    for s in (-1, 1):
        yy = 1.73 * s
        p.obox('Patch', (2.05, yy, 0.84), (1.55, 0.035, 0.5), bevel=0.01)
        p.obox('Rust', (1.0, yy + 0.005 * s, 0.82), (0.5, 0.04, 0.46), bevel=0.01)
        p.obox('Patch', (2.75, yy + 0.004 * s, 1.0), (0.3, 0.045, 0.3), bevel=0.01)
        for x in (2.6, 1.5, 0.8):                                                   # os suportes
            p.obox('Metal', (x, (UP + 0.05) * s, FENDER - 0.03), (0.04, 0.2, 0.04), bevel=0.0)
    # a gaiola de barras (blindagem de grade) nas laterais, sobre as rodas de trás
    for s in (-1, 1):
        yy = 1.78 * s
        for k in range(6):
            p.obox('Metal', (-1.3, yy, 0.52 + k * 0.11), (3.3, 0.03, 0.03), bevel=0.0)
        for x in (-2.93, -2.1, -1.27, -0.44, 0.33):
            p.obox('Metal', (x, yy, 0.79), (0.04, 0.04, 0.64), bevel=0.0)
        for x in (-2.7, -1.3, 0.1):
            p.obox('Metal', (x, (UP + 0.07) * s, FENDER - 0.04), (0.04, 0.22, 0.04), bevel=0.0)
            p.obox('Metal', (x, (UP + 0.07) * s, 0.5), (0.04, 0.22, 0.04), bevel=0.0)
    # carga nos para-lamas: caixa de ferramentas e caixote à esquerda, tanques de combustível à direita,
    # galões e lona enrolada atrás, e o caixote de munição
    p.box('Paint', 1.25, 2.25, -1.6, -1.18, DECK, DECK + 0.24)
    p.box('Dark', 1.3, 2.2, -1.605, -1.59, DECK + 0.06, DECK + 0.08, bevel=0.0)
    p.box('Wood', -0.25, 0.55, -1.58, -1.2, DECK, DECK + 0.3, bevel=0.015)
    p.box('Dark', -0.2, 0.5, -1.585, -1.575, DECK + 0.15, DECK + 0.165, bevel=0.0)
    for x in (0.3, -1.0):
        p.cyl('Paint', (x, 1.4, DECK + 0.2), 'x', 0.19, 1.0, seg=20, bevel=0.01, smooth=True)
        for dx in (-0.3, 0.3):
            p.cyl('Dark', (x + dx, 1.4, DECK + 0.2), 'x', 0.2, 0.04, seg=20, bevel=0.0)
        p.box('Metal', x - 0.42, x + 0.42, 1.2, 1.6, DECK, DECK + 0.06, bevel=0.005)
    for x in (-2.0, -2.4):
        p.box('Metal', x - 0.17, x + 0.17, -1.58, -1.42, DECK, DECK + 0.46, bevel=0.012)
        p.box('Dark', x - 0.05, x + 0.05, -1.56, -1.44, DECK + 0.46, DECK + 0.5, bevel=0.0)
    p.cyl('Bag', (-1.55, 0.0, DECK + 0.19), 'y', 0.13, 1.1, seg=16, bevel=0.0, smooth=True)
    for y in (-0.35, 0.35):
        p.cyl('Dark', (-1.55, y, DECK + 0.19), 'y', 0.14, 0.03, seg=16, bevel=0.0)
    p.box('Wood', -3.0, -2.5, 0.35, 1.05, DECK + 0.06, DECK + 0.34, bevel=0.015)
    # a traseira: as aletas do escape (à direita) e o pneu sobressalente amarrado no meio, com o aro
    for k in range(5):
        p.obox('Dark', (REAR - 0.015, 0.95, 0.88 + k * 0.1), (0.03, 0.7, 0.04), pitch=-30, bevel=0.0)
    p.torus('Rubber', (-3.42, 0.0, 1.0), 'x', 0.46, 0.145, seg=28, rseg=12)
    p.cyl('Metal', (-3.42, 0.0, 1.0), 'x', 0.31, 0.14, seg=20)
    p.cyl('Dark', (-3.5, 0.0, 1.0), 'x', 0.12, 0.06, seg=12, bevel=0.0)
    for roll in (45, -45):
        p.obox('Bag', (-3.6, 0.0, 1.0), (0.025, 1.26, 0.035), roll=roll, bevel=0.0)
    # a bandeira baixa no para-lama de trás, à direita
    p.cyl('Metal', (-2.85, 1.45, DECK + 0.6), 'z', 0.013, 1.2, seg=6, bevel=0.0)
    flag(p, (-2.85, 1.45), DECK + 1.2, 0.34, 0.22, phase=1.0)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret():
    p = Part(PREFIX + '_Turret')
    rings = []
    N = 40
    for d, z in DOME:
        fwd = 0.05 * min(z / 0.42, 1.0)                                    # a cúpula bojuda pra frente
        rings.append([(TX + fwd + d * math.cos(2 * math.pi * k / N), d * math.sin(2 * math.pi * k / N), DECK + z)
                      for k in range(N)])
    p.loft('Paint#dome', rings, bevel=0.0, smooth=True)
    p.cyl('Dark', (TX, 0.0, DECK + 0.03), 'z', 0.98, 0.06, seg=40, bevel=0.0)   # a saia do anel
    # o mantelete (o colar do canhão) e a máscara
    p.cyl('Paint', (TX + 1.12, 0.0, GUN_Z), 'x', 0.3, 0.42, r2=0.24, seg=24, smooth=True)
    p.cyl('Dark', (TX + 1.34, 0.0, GUN_Z), 'x', 0.2, 0.04, seg=24, bevel=0.0)
    # o holofote grande à direita do mantelete, com o vidro, e o pequeno em cima dele
    p.cyl('Paint', (TX + 0.98, 0.78, 2.32), 'x', 0.27, 0.3, seg=24, bevel=0.01, smooth=True)
    p.cyl('Dark', (TX + 1.135, 0.78, 2.32), 'x', 0.25, 0.03, seg=24, bevel=0.0)
    p.cyl('Lens', (TX + 1.155, 0.78, 2.32), 'x', 0.22, 0.01, seg=24, bevel=0.0)
    p.obox('Metal', (TX + 0.85, 0.78, 2.05), (0.2, 0.08, 0.3), bevel=0.0)
    # as faixas verdes no teto: ao longo e em diagonal (cor do jogador)
    strip_on_dome(p, 'Team', [(TX + 0.98, 0.0), (TX - 1.0, 0.0)], 0.17)
    strip_on_dome(p, 'Team', [(TX + 0.72, -0.62), (TX - 0.72, 0.6)], 0.15, lift=0.009)
    # as chapas remendadas na cúpula (teal e ferrugem): placas rígidas parafusadas por cima da curva, como na ficha
    # (na lateral da cúpula, quase em pé, como na ficha: d = 1,09 é a encosta de 80°)
    on_dome(p, 'Patch', 212, 1.09, (0.5, 0.64, 0.03), lift=-0.012, bevel=0.006)
    on_dome(p, 'Rust', 250, 1.08, (0.42, 0.48, 0.03), lift=-0.012, bevel=0.006)
    on_dome(p, 'Patch', 318, 1.07, (0.32, 0.38, 0.03), lift=-0.012, bevel=0.006)
    on_dome(p, 'Rust', 285, 1.08, (0.3, 0.32, 0.03), lift=-0.012, bevel=0.006)
    on_dome(p, 'Patch', 60, 0.6, (0.34, 0.3, 0.03), lift=-0.012, bevel=0.006)
    for az, d, size in ((212, 1.09, (0.5, 0.64)), (250, 1.08, (0.42, 0.48))):            # os parafusos das chapas
        for sx in (-1, 1):
            for sy in (-1, 1):
                pos, n, phi = dome_point(az, d)
                a = math.radians(az)
                tang = Vector((-math.sin(a), math.cos(a), 0.0))
                radial = Vector((math.cos(a), math.sin(a), 0.0)) * math.cos(math.radians(phi)) - Vector((0, 0, math.sin(math.radians(phi))))
                c = pos + n * 0.022 + radial * (size[0] / 2 - 0.05) * sx + tang * (size[1] / 2 - 0.05) * sy
                p.obox('Dark', (c.x, c.y, c.z), (0.035, 0.035, 0.02), yaw=az, pitch=-phi, bevel=0.0)
    # a cúpula do comandante (à esquerda, atrás) com a tampa e os visores; a escotilha do municiador (à direita)
    cx, cy = TX - 0.18, -0.46
    zc = DECK + dome_z(math.hypot(cx - TX, cy))
    p.cyl('Paint', (cx, cy, zc + 0.04), 'z', 0.34, 0.26, seg=24, bevel=0.012)
    p.cyl('Paint', (cx, cy, zc + 0.2), 'z', 0.3, 0.06, seg=24, bevel=0.01)
    p.cyl('Paint', (cx + 0.05, cy - 0.05, zc + 0.26), 'z', 0.26, 0.04, seg=20, bevel=0.008)
    for k in range(5):
        a = math.radians(k * 50 - 100)
        p.obox('Dark', (cx + 0.33 * math.cos(a), cy + 0.33 * math.sin(a), zc + 0.1), (0.03, 0.12, 0.05), yaw=k * 50 - 100, bevel=0.0)
    lx, ly = TX - 0.2, 0.5
    zl = DECK + dome_z(math.hypot(lx - TX, ly))
    p.cyl('Paint', (lx, ly, zl + 0.03), 'z', 0.3, 0.1, seg=24, bevel=0.012)
    p.box('Metal', lx - 0.06, lx + 0.06, ly + 0.28, ly + 0.38, zl + 0.02, zl + 0.1, bevel=0.0)
    # a metralhadora pesada (DShK) no pedestal, na frente da escotilha, apontando pra frente
    gx, gy = TX + 0.2, 0.55
    zg = DECK + dome_z(math.hypot(gx - TX, gy))
    p.cyl('Metal', (gx, gy, zg + 0.2), 'z', 0.05, 0.44, seg=10, bevel=0.0)
    p.box('Metal', gx - 0.1, gx + 0.1, gy - 0.07, gy + 0.07, zg + 0.4, zg + 0.46, bevel=0.005)
    p.box('Dark', gx - 0.3, gx + 0.3, gy - 0.065, gy + 0.065, zg + 0.44, zg + 0.58, bevel=0.01)
    p.cyl('Dark', (gx + 0.75, gy, zg + 0.52), 'x', 0.028, 0.95, seg=10, bevel=0.0)
    p.cyl('Dark', (gx + 0.55, gy, zg + 0.52), 'x', 0.042, 0.45, seg=10, bevel=0.0)
    p.cyl('Dark', (gx + 1.25, gy, zg + 0.52), 'x', 0.05, 0.14, seg=10, bevel=0.0)
    p.box('Wood', gx - 0.2, gx + 0.05, gy - 0.3, gy - 0.1, zg + 0.42, zg + 0.6, bevel=0.01)
    for dy in (-0.05, 0.05):
        p.box('Dark', gx - 0.48, gx - 0.3, gy + dy - 0.012, gy + dy + 0.012, zg + 0.44, zg + 0.56, bevel=0.0)
    # a antena (à direita, atrás) e o mastro com a bandeira alta (à esquerda, atrás)
    ax, ay = TX - 0.85, 0.62
    za = DECK + dome_z(math.hypot(ax - TX, ay))
    p.cyl('Dark', (ax, ay, za + 0.03), 'z', 0.045, 0.08, seg=10, bevel=0.0)
    p.cyl('Metal', (ax, ay, za + 0.75), 'z', 0.011, 1.5, seg=6, bevel=0.0)
    fx, fy = TX - 0.72, -0.36
    zf = DECK + dome_z(math.hypot(fx - TX, fy))
    p.cyl('Dark', (fx, fy, zf + 0.04), 'z', 0.05, 0.1, seg=10, bevel=0.0)
    p.cyl('Metal', (fx, fy, (zf + 3.95) / 2), 'z', 0.016, 3.95 - zf, seg=8, bevel=0.0)
    flag(p, (fx, fy), 3.95, 0.64, 0.42)
    # a meia-lua de sacos de areia na direita da torre (da frente-direita até atrás)
    for k, az in enumerate((28, 52, 76, 100, 124, 148, 170)):
        on_dome(p, 'Bag', az, 1.07, (0.3, 0.48, 0.2), lift=-0.04, bevel=0.05)
    on_dome(p, 'Bag', 88, 0.86, (0.28, 0.44, 0.19), lift=-0.02, bevel=0.05)
    on_dome(p, 'Bag', 136, 0.86, (0.28, 0.44, 0.19), lift=-0.02, bevel=0.05)
    # o cesto de barras atrás da torre, com a carga (saco e caixote)
    bx0, bx1 = TX - 1.12, TX - 1.52
    for z in (1.98, 2.3):
        p.obox('Metal', (bx1, 0.0, z), (0.03, 1.36, 0.03), bevel=0.0)
        for s in (-1, 1):
            p.obox('Metal', ((bx0 + bx1) / 2, 0.68 * s, z), (0.44, 0.03, 0.03), bevel=0.0)
    for y in (-0.68, -0.23, 0.23, 0.68):
        p.obox('Metal', (bx1, y, 2.14), (0.03, 0.03, 0.35), bevel=0.0)
    for y in (-0.45, 0.0, 0.45):
        p.obox('Metal', ((bx0 + bx1) / 2, y, 1.98), (0.44, 0.03, 0.03), bevel=0.0)
    p.obox('Bag', (bx0 - 0.2, -0.32, 2.13), (0.34, 0.56, 0.26), yaw=8, bevel=0.05)
    p.box('Wood', bx1 + 0.05, bx0 - 0.05, 0.1, 0.6, 2.0, 2.3, bevel=0.015)
    p.obox('Bag', (bx0 - 0.2, 0.35, 2.38), (0.3, 0.4, 0.18), yaw=-10, bevel=0.05)
    # alças de mão e a tampa de ventilação
    for az, d in ((215, 0.9), (245, 0.9), (330, 0.98)):
        on_dome(p, 'Metal', az, d, (0.04, 0.26, 0.04), lift=0.04, bevel=0.0)
    p.cyl('Paint', (TX - 0.62, 0.05, DECK + dome_z(0.62) + 0.02), 'z', 0.13, 0.05, seg=16, bevel=0.008)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    p = Part(PREFIX + '_Barrel')
    x0 = TX + 1.3
    p.cyl('Paint', (x0 + 0.5, 0.0, GUN_Z), 'x', 0.105, 1.0, seg=24, smooth=True)            # a base grossa
    p.cyl('Metal', (x0 + 1.02, 0.0, GUN_Z), 'x', 0.115, 0.06, seg=24, bevel=0.004)           # o colar
    p.cyl('Paint', (x0 + 2.6, 0.0, GUN_Z), 'x', 0.088, 3.1, seg=24, r2=0.078, smooth=True)   # o tubo até a boca
    p.cyl('Team', (x0 + 3.0, 0.0, GUN_Z), 'x', 0.092, 0.2, seg=24, bevel=0.003)             # o anel verde
    p.cyl('Team', (x0 + 3.24, 0.0, GUN_Z), 'x', 0.09, 0.05, seg=24, bevel=0.003)
    p.cyl('Dark', (5.72, 0.0, GUN_Z), 'x', 0.09, 0.16, seg=24, bevel=0.005)                  # a boca
    p.cyl('Dark', (5.805, 0.0, GUN_Z), 'x', 0.05, 0.01, seg=16, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            p.cyl('Rubber', (x, y, WHEEL_Z), 'y', WHEEL_R, 0.26, seg=28, bevel=0.015, smooth=True)
            p.cyl('Paint', (x, y, WHEEL_Z), 'y', 0.335, 0.3, seg=28, bevel=0.01)
            p.cyl('Paint', (x, y, WHEEL_Z), 'y', 0.2, 0.34, seg=20, bevel=0.01)
            p.cyl('Metal', (x, y, WHEEL_Z), 'y', 0.095, 0.38, seg=14, bevel=0.008)
            for k in range(5):
                a = k * 72 + 18
                p.obox('Paint', (x + 0.23 * math.cos(math.radians(a)), y, WHEEL_Z + 0.23 * math.sin(math.radians(a))),
                       (0.2, 0.33, 0.06), pitch=a, bevel=0.0)
        # a roda-guia na frente e a roda motriz atrás, com os dentes
        p.cyl('Paint', (IDLER[0], y, IDLER[1]), 'y', IDLER[2], 0.3, seg=24, bevel=0.015)
        p.cyl('Metal', (IDLER[0], y, IDLER[1]), 'y', 0.09, 0.36, seg=12, bevel=0.005)
        p.cyl('Metal', (SPROCKET[0], y, SPROCKET[1]), 'y', SPROCKET[2] - 0.03, 0.32, seg=24, bevel=0.01)
        p.cyl('Dark', (SPROCKET[0], y, SPROCKET[1]), 'y', 0.12, 0.36, seg=12, bevel=0.005)
        for k in range(14):
            a = math.radians(k * 360 / 14)
            rr = SPROCKET[2] + 0.02
            p.obox('Metal', (SPROCKET[0] + rr * math.cos(a), y, SPROCKET[1] + rr * math.sin(a)), (0.07, 0.3, 0.06),
                   pitch=-math.degrees(a), bevel=0.0)
    return p.finish()


def track_path():
    """O contorno da esteira: o envoltório convexo das rodas (sem roletes: a parte de cima vai reta da motriz à guia)."""
    pts = []
    circles = [(IDLER[0], IDLER[1], IDLER[2] + 0.06), (SPROCKET[0], SPROCKET[1], SPROCKET[2] + 0.06)] + \
              [(x, WHEEL_Z, WHEEL_R + 0.06) for x in WHEELS_X]
    for cx, cz, r in circles:
        for k in range(64):
            a = 2 * math.pi * k / 64
            pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    pts = sorted(set(pts))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for q in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    hull_ = lower[:-1] + upper[:-1]
    seg = list(zip(hull_, hull_[1:] + hull_[:1]))
    total = sum(math.dist(a, b) for a, b in seg)
    n = int(total / 0.14)                                      # o passo do elo do T-55 (13,7 cm)
    step = total / n
    out, t = [], 0.0
    for a, b in seg:
        L = math.dist(a, b)
        while t <= L:
            u = t / L
            out.append(((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u), math.atan2(b[1] - a[1], b[0] - a[0])))
            t += step
        t -= L
    return out


def track(side):
    s = 1 if side == 'r' else -1
    p = Part(f'{PREFIX}_Track_{side}')
    y = TRACK_Y * s
    for (x, z), ang in track_path():
        deg = math.degrees(ang)
        nx, nz = math.sin(ang), -math.cos(ang)
        p.obox('Track', (x, y, z), (0.125, TRACK_W, 0.045), pitch=deg, bevel=0.006)
        p.obox('Track', (x + nx * 0.03, y, z + nz * 0.03), (0.035, TRACK_W * 0.9, 0.025), pitch=deg, bevel=0.0)
        p.obox('Metal', (x - nx * 0.035, y, z - nz * 0.035), (0.06, 0.07, 0.05), pitch=deg, bevel=0.0)   # a guia central
    return p.finish()


# ---------------------------------------------------------------------------------- exportação
def main():
    clear_scene()
    only = os.environ.get('MODEL_ONLY')
    builds = {'hull': hull, 'turret': turret, 'barrel': barrel, 'wheels': wheels,
              'track_l': lambda: track('l'), 'track_r': lambda: track('r')}
    for key, build in builds.items():
        if only and key not in only.split(','):
            continue
        o = build()
        export(o, OUT)
        bpy.data.objects.remove(o, do_unlink=True)


main()
