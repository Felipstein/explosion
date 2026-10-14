"""A Oficina do GLA (War Factory) provisória, modelada no Blender por script (sessão Modelos 3D, 13/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_gla_war_factory.py -- <pasta de saída>

A planta é a ficha aprovada docs/concept/buildings/gla_war_factory_sheet.webp ("por enquanto": armazém fechado
de um lado, casa e gaiola de armas do outro, rampa do túnel sob a rede de camuflagem) e a bíblia do GLA em
docs/CONCEITO_CONSTRUCOES.md: adobe cor de areia, pátio murado, telhados planos, escadas externas, chaminés
com fumaça preta, bandeiras verdes, rabiscos verdes à mão e a borda verde no alto dos muros (todo o verde é
a cor do jogador). Padrão dos provisórios: quina dura com chanfro, uma cor por material, sem desgaste.

Planta 24 × 32 m (6 × 8 quadrados da grade de 4 m; a USA tem 28 × 32), a frente em +X (a rampa sai pra
+X, como a porta da War Factory USA), a direita do prédio em +Y. Muros 3,2 m, armazém 8 m, casa 7 m, chaminé
11,8 m: nada tapa unidade atrás (docs/PRODUCAO_3D.md §3). O tanque nasce na boca do túnel (X −8, no chão)
e sai pela rampa; a entrada subterrânea é só desenho (a boca escura no fundo).

Peças: gla_war_factory (o prédio) e gla_war_factory_scaffold (o andaime da obra). Materiais (slots):
  Plaster (adobe), Earth (o chão batido do lote), Player (cor do jogador: bandeiras, rabiscos, borda dos
  muros), Dark, Trim (metal escuro: postes, portão de grade), Metal, Wood (portões, toldos, caixotes),
  Cloth (lonas), Net (rede de camuflagem), Crate (caixas de munição), Hazard (faixa amarela e preta),
  Rust (chaminés, tambores, caixa d'água), Light (lâmpadas).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag, mat, select_only, sheet  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_war_factory')
os.makedirs(OUT, exist_ok=True)

COLORS.update({'Plaster': (205, 180, 135), 'Earth': (168, 142, 104), 'Player': (86, 150, 62), 'Dark': (40, 40, 38),
               'Trim': (70, 68, 62), 'Metal': (92, 90, 82), 'Wood': (120, 88, 52), 'Cloth': (150, 128, 92),
               'Net': (96, 104, 62), 'Crate': (84, 86, 58), 'Hazard': (210, 170, 30), 'Rust': (118, 70, 40),
               'Light': (255, 220, 160)})

HX, HY = 12.0, 16.0          # o lote: 24 × 32 m
G = 0.2                      # o chão batido do lote
WALL, PIL = 3.2, 3.6         # muros e pilaretes
BAY = 5.5                    # meia-largura da rampa (11 m)


def band(p, x0, x1, y0, y1, z0, z1, faces='+x-x+y-y', m='Player', out=0.02):
    """A faixa da cor do jogador colada nas faces de fora de um bloco."""
    if '+x' in faces:
        p.box(m, x1, x1 + out, y0, y1, z0, z1, bevel=0.0)
    if '-x' in faces:
        p.box(m, x0 - out, x0, y0, y1, z0, z1, bevel=0.0)
    if '+y' in faces:
        p.box(m, x0, x1, y1, y1 + out, z0, z1, bevel=0.0)
    if '-y' in faces:
        p.box(m, x0, x1, y0 - out, y0, z0, z1, bevel=0.0)


def wall(p, x0, x1, y0, y1, h=WALL, top_band=True):
    """Um trecho de muro de adobe com a borda verde no alto."""
    p.box('Plaster', x0, x1, y0, y1, G, G + h, bevel=0.06)
    if top_band:
        along_x = (x1 - x0) > (y1 - y0)
        if along_x:
            band(p, x0, x1, y0, y1, G + h - 0.5, G + h - 0.05, faces='+y-y')
        else:
            band(p, x0, x1, y0, y1, G + h - 0.5, G + h - 0.05, faces='+x-x')


def pilaster(p, x, y, h=PIL, s=0.45):
    p.box('Plaster', x - s, x + s, y - s, y + s, G, G + h, bevel=0.05)
    p.box('Plaster', x - s - 0.08, x + s + 0.08, y - s - 0.08, y + s + 0.08, G + h - 0.25, G + h, bevel=0.03)


def scribble(p, c, face, size=1.9):
    """O rabisco verde à mão na parede: duas pinceladas cruzadas (a estrela do GLA), coladas na face."""
    x, y, z = c
    for yaw_or_roll in (28, -22, 75):
        if face in ('+x', '-x'):
            p.obox('Player', (x + (0.015 if face == '+x' else -0.015), y, z), (0.01, size, 0.32), roll=yaw_or_roll, bevel=0.0)
        else:
            p.obox('Player', (x, y + (0.015 if face == '+y' else -0.015), z), (size, 0.01, 0.32), pitch=yaw_or_roll, bevel=0.0)


def crate_stack(p, x, y, z, n=3, yaw=0.0, size=(1.6, 0.8, 0.5)):
    for k in range(n):
        p.obox('Crate', (x + (k % 2) * 0.05, y, z + size[2] / 2 + k * size[2]), size, yaw=yaw + (k % 2) * 4, bevel=0.03)
        p.obox('Dark', (x + (k % 2) * 0.05, y, z + size[2] * 0.5 + k * size[2]), (size[0] + 0.02, size[1] * 0.3, 0.05), yaw=yaw + (k % 2) * 4, bevel=0.0)


def barrel(p, x, y, z, r=0.3, h=0.9, m='Rust'):
    p.cyl(m, (x, y, z + h / 2), 'z', r, h, seg=16, bevel=0.02, smooth=True)
    for dz in (0.25, 0.65):
        p.cyl('Dark', (x, y, z + dz), 'z', r + 0.015, 0.05, seg=16, bevel=0.0)


def lamp(p, x, y, z, face='+x'):
    dx = 0.12 if face == '+x' else -0.12
    p.box('Dark', x, x + dx, y - 0.12, y + 0.12, z - 0.1, z + 0.1, bevel=0.0)
    p.box('Light', x + dx * 0.5, x + dx * 1.6, y - 0.1, y + 0.1, z - 0.08, z + 0.08, bevel=0.0)


def ladder(p, x, y0, y1, z0, z1):
    for y in (y0, y1):
        p.box('Trim', x - 0.03, x + 0.03, y - 0.03, y + 0.03, z0, z1, bevel=0.0)
    n = int((z1 - z0) / 0.3)
    for k in range(n):
        z = z0 + 0.15 + k * 0.3
        p.box('Trim', x - 0.025, x + 0.025, y0, y1, z - 0.025, z + 0.025, bevel=0.0)


def building():
    p = Part('gla_war_factory')
    # ---------------------------------------------------------------- o lote: o chão batido com a saia (desnível)
    p.box('Earth', -HX, HX, -HY, HY, -3.0, G, bevel=0.08)
    p.box('Dark', -HX - 0.04, HX + 0.04, -HY - 0.04, HY + 0.04, G - 0.12, G - 0.1, bevel=0.0)
    # ---------------------------------------------------------------- o armazém (direita, +Y): bloco de 8 m
    ax0, ax1, ay0, ay1, ah = -11.6, -1.5, 5.5, 15.5, 8.0
    p.box('Plaster', ax0, ax1, ay0, ay1, G, G + ah, bevel=0.08)
    for (x0, x1, y0, y1) in ((ax0, ax1, ay0 - 0.05, ay0 + 0.4), (ax0, ax1, ay1 - 0.4, ay1 + 0.05),
                             (ax0 - 0.05, ax0 + 0.4, ay0, ay1), (ax1 - 0.4, ax1 + 0.05, ay0, ay1)):
        p.box('Plaster', x0, x1, y0, y1, G + ah, G + ah + 0.5, bevel=0.04)                 # o parapeito
    band(p, ax0, ax1, ay0, ay1, G + 7.0, G + 7.6)
    for (x, y) in ((ax0, ay0), (ax0, ay1), (ax1, ay0), (ax1, ay1)):
        p.box('Plaster', x - 0.5, x + 0.5, y - 0.5, y + 0.5, G, G + ah + 0.75, bevel=0.05)   # pilaretes dos cantos
    # a frente do armazém (+X): o portão duplo de madeira com o toldo, a janela e a lâmpada
    p.box('Plaster', ax1 - 0.1, ax1 + 0.25, 8.6, 13.4, G, G + 4.2, bevel=0.04)             # a moldura
    p.box('Wood', ax1 + 0.1, ax1 + 0.2, 8.9, 13.1, G, G + 3.8, bevel=0.01)
    p.box('Dark', ax1 + 0.2, ax1 + 0.23, 10.97, 11.03, G + 0.2, G + 3.6, bevel=0.0)
    for y in (9.2, 10.6, 11.4, 12.8):
        p.box('Dark', ax1 + 0.2, ax1 + 0.24, y - 0.04, y + 0.04, G + 0.3, G + 3.5, bevel=0.0)
    p.obox('Wood', (ax1 + 0.8, 11.0, G + 4.55), (1.6, 5.2, 0.08), pitch=-14, bevel=0.0)      # o toldo
    for y in (8.7, 13.3):
        p.cyl('Wood', (ax1 + 1.45, y, G + 2.3), 'z', 0.06, 4.1, seg=8, bevel=0.0)
    p.box('Dark', ax1 - 0.02, ax1 + 0.06, 6.4, 7.6, G + 4.6, G + 5.6, bevel=0.0)           # a janela
    p.box('Plaster', ax1, ax1 + 0.12, 6.25, 7.75, G + 5.6, G + 5.75, bevel=0.0)
    lamp(p, ax1, 14.2, G + 5.2)
    # o lado de fora do armazém (+Y): porta em arco, lona de abrigo, caixas e tambores
    p.box('Plaster', -9.2, -7.4, ay1 - 0.1, ay1 + 0.25, G, G + 2.6, bevel=0.03)
    p.cyl('Plaster', (-8.3, ay1 + 0.08, G + 2.6), 'y', 0.9, 0.35, seg=20)
    p.box('Wood', -9.0, -7.6, ay1 + 0.1, ay1 + 0.18, G, G + 2.6, bevel=0.0)
    p.cyl('Wood', (-8.3, ay1 + 0.14, G + 2.6), 'y', 0.7, 0.08, seg=20, bevel=0.0)
    p.obox('Cloth', (-5.0, ay1 + 1.3, G + 2.6), (3.2, 2.8, 0.05), roll=-22, bevel=0.0)      # a lona-abrigo
    for x in (-6.4, -3.6):
        p.cyl('Wood', (x, ay1 + 2.5, G + 1.05), 'z', 0.05, 2.1, seg=8, bevel=0.0)
    crate_stack(p, -10.8, ay1 + 1.0, G, 3)
    barrel(p, -2.2, ay1 + 0.6, G)
    barrel(p, -1.4, ay1 + 1.2, G)
    # o teto do armazém: o quartinho com a chaminé e a caixa d'água, caixas, a bandeira
    sx0, sx1, sy0, sy1 = -11.4, -7.6, 11.2, 15.3
    p.box('Plaster', sx0, sx1, sy0, sy1, G + ah, G + ah + 2.0, bevel=0.06)
    band(p, sx0, sx1, sy0, sy1, G + ah + 1.3, G + ah + 1.8)
    p.cyl('Rust', (-9.9, 9.3, G + ah + 1.9), 'z', 0.72, 3.8, seg=20, bevel=0.03, smooth=True)   # a chaminé
    p.cyl('Dark', (-9.9, 9.3, G + ah + 3.8), 'z', 0.8, 0.12, seg=20, bevel=0.0)
    p.cyl('Dark', (-9.9, 9.3, G + ah + 3.75), 'z', 0.55, 0.06, seg=16, bevel=0.0)
    p.cyl('Rust', (-9.5, 13.3, G + ah + 2.9), 'z', 0.95, 1.6, seg=20, bevel=0.03, smooth=True)  # a caixa d'água
    p.cyl('Dark', (-9.5, 13.3, G + ah + 3.72), 'z', 1.0, 0.08, seg=20, bevel=0.0)
    for (dx, dy) in ((-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)):
        p.box('Trim', -9.5 + dx - 0.05, -9.5 + dx + 0.05, 13.3 + dy - 0.05, 13.3 + dy + 0.05, G + ah + 2.0, G + ah + 2.2, bevel=0.0)
    crate_stack(p, -4.0, 7.2, G + ah, 2, yaw=10)
    crate_stack(p, -2.8, 13.8, G + ah, 1, yaw=-30)
    p.cyl('Trim', (ax0 + 0.5, ay1 - 0.5, G + ah + 2.4), 'z', 0.04, 3.4, seg=8, bevel=0.0)
    flag(p, (ax0 + 0.5, ay1 - 0.5), G + ah + 4.1, 1.5, 0.95, m='Player')
    # ---------------------------------------------------------------- a rampa: a boca do túnel no fundo, as faixas, as marcas
    p.box('Plaster', -HX, -8.0, -BAY - 0.5, BAY + 0.5, G + 5.6, G + 8.0, bevel=0.08)           # o lintel sobre a boca
    band(p, -HX, -8.0, -BAY - 0.5, BAY + 0.5, G + 7.0, G + 7.6, faces='+x')
    p.box('Plaster', -HX, -8.0, BAY, BAY + 0.5, G, G + 5.6, bevel=0.05)                       # os ombros da boca
    p.box('Plaster', -HX, -8.0, -BAY - 0.5, -BAY, G, G + 5.6, bevel=0.05)
    p.box('Dark', -HX + 0.2, -8.2, -BAY + 0.03, BAY - 0.03, G - 0.01, G + 5.55, bevel=0.0)    # o escuro do túnel
    p.obox('Dark', (-10.2, 0.0, G - 0.9), (4.0, 2 * BAY - 0.1, 0.1), pitch=-28, bevel=0.0)     # o piso descendo
    for y in (-3.6, 0.0, 3.6):
        p.box('Light', -8.6, -8.3, y - 0.35, y + 0.35, G + 5.2, G + 5.35, bevel=0.0)
    for s in (-1, 1):
        for k in range(14):                                                                # as faixas de perigo
            x = -7.5 + k * 1.4
            p.box('Hazard' if k % 2 == 0 else 'Dark', x, x + 1.4, BAY * s - 0.3 if s > 0 else -BAY + 0.0, BAY * s if s > 0 else -BAY + 0.3, G, G + 0.02, bevel=0.0)
        p.box('Dark', -7.9, HX, 2.6 * s - 0.35, 2.6 * s + 0.35, G, G + 0.012, bevel=0.0)      # as marcas da esteira
    # os muros laterais da rampa (da boca até a frente), com a borda verde
    wall(p, -8.0, 3.0, BAY, BAY + 0.5, h=1.2, top_band=False)
    wall(p, -8.0, HX - 0.5, -BAY - 0.5, -BAY, h=1.2, top_band=False)
    # a rede de camuflagem: alta no armazém (+Y), baixa nos postes (−Y), barriga no meio; a lona do canto
    sheet(p, 'Net', [(-7.8, BAY + 0.3, G + 7.7), (3.6, BAY + 0.3, G + 7.4), (3.2, -BAY - 1.0, G + 6.3), (-7.4, -BAY - 1.0, G + 6.6)],
          14, 12, sag=0.9, thick=0.03)
    for (x, y, h) in ((3.2, -BAY - 1.0, 6.4), (-7.4, -BAY - 1.0, 6.7), (3.6, BAY + 0.3, 7.5)):
        p.cyl('Trim', (x, y, G + h / 2), 'z', 0.07, h, seg=8, bevel=0.0)
    p.cyl('Dark', (-2.1, -BAY - 1.0, G + 6.35), 'x', 0.02, 10.6, seg=6, bevel=0.0)             # o cabo entre os postes
    p.cyl('Dark', (-2.1, BAY + 0.3, G + 7.5), 'x', 0.02, 11.4, seg=6, bevel=0.0)
    sheet(p, 'Cloth', [(-11.5, -BAY - 2.2, G + 5.4), (-8.2, -BAY - 2.2, G + 5.4), (-8.2, -BAY - 0.3, G + 4.6), (-11.5, -BAY - 0.3, G + 4.6)],
          6, 4, sag=0.25, thick=0.03)
    for x in (-11.3, -8.4):
        p.cyl('Wood', (x, -BAY - 0.4, G + 2.3), 'z', 0.05, 4.6, seg=8, bevel=0.0)
    # ---------------------------------------------------------------- a casa (esquerda, −Y, no fundo): 2 andares
    hx0, hx1, hy0, hy1, hh = -11.6, -4.0, -15.5, -7.5, 7.0
    p.box('Plaster', hx0, hx1, hy0, hy1, G, G + hh, bevel=0.08)
    for (x0, x1, y0, y1) in ((hx0, hx1, hy0 - 0.05, hy0 + 0.4), (hx0, hx1, hy1 - 0.4, hy1 + 0.05),
                             (hx0 - 0.05, hx0 + 0.4, hy0, hy1), (hx1 - 0.4, hx1 + 0.05, hy0, hy1)):
        p.box('Plaster', x0, x1, y0, y1, G + hh, G + hh + 0.5, bevel=0.04)
    band(p, hx0, hx1, hy0, hy1, G + 6.0, G + 6.6)
    for (x, y) in ((hx0, hy0), (hx1, hy0), (hx1, hy1)):
        p.box('Plaster', x - 0.5, x + 0.5, y - 0.5, y + 0.5, G, G + hh + 0.75, bevel=0.05)
    # a frente da casa (+X): porta com toldo, janelas; a lateral (−Y): janelas; a escada de mão
    p.box('Plaster', hx1 - 0.1, hx1 + 0.2, -12.3, -10.7, G, G + 2.9, bevel=0.03)
    p.box('Wood', hx1 + 0.1, hx1 + 0.18, -12.1, -10.9, G, G + 2.6, bevel=0.0)
    p.obox('Wood', (hx1 + 0.7, -11.5, G + 3.2), (1.4, 2.6, 0.07), pitch=-14, bevel=0.0)
    for y in (-12.6, -10.4):
        p.cyl('Wood', (hx1 + 1.3, y, G + 1.55), 'z', 0.05, 2.9, seg=8, bevel=0.0)
    for (y, z) in ((-9.0, 4.6), (-13.8, 4.6), (-9.0, 1.6)):
        p.box('Dark', hx1 - 0.02, hx1 + 0.06, y - 0.55, y + 0.55, G + z, G + z + 1.0, bevel=0.0)
        p.box('Plaster', hx1, hx1 + 0.12, y - 0.7, y + 0.7, G + z + 1.0, G + z + 1.15, bevel=0.0)
    for (x, z) in ((-9.5, 4.6), (-6.5, 4.6), (-6.5, 1.6)):
        p.box('Dark', x - 0.55, x + 0.55, hy0 - 0.06, hy0 + 0.02, G + z, G + z + 1.0, bevel=0.0)
        p.box('Plaster', x - 0.7, x + 0.7, hy0 - 0.12, hy0, G + z + 1.0, G + z + 1.15, bevel=0.0)
    lamp(p, hx1, -13.2, G + 3.3)
    ladder(p, hx0 - 0.25, -12.4, -11.8, G + 0.3, G + hh + 0.6)
    # o teto da casa: caixa d'água, chaminé, bandeira, caixas
    p.cyl('Rust', (-9.8, -9.6, G + hh + 0.95), 'z', 0.85, 1.5, seg=20, bevel=0.03, smooth=True)
    p.cyl('Dark', (-9.8, -9.6, G + hh + 1.72), 'z', 0.9, 0.08, seg=20, bevel=0.0)
    for (dx, dy) in ((-0.55, -0.55), (0.55, -0.55), (0.55, 0.55), (-0.55, 0.55)):
        p.box('Trim', -9.8 + dx - 0.05, -9.8 + dx + 0.05, -9.6 + dy - 0.05, -9.6 + dy + 0.05, G + hh, G + hh + 0.2, bevel=0.0)
    p.cyl('Rust', (-6.0, -13.4, G + hh + 1.75), 'z', 0.55, 3.5, seg=16, bevel=0.03, smooth=True)
    p.cyl('Dark', (-6.0, -13.4, G + hh + 3.5), 'z', 0.62, 0.1, seg=16, bevel=0.0)
    p.cyl('Trim', (hx0 + 0.5, hy0 + 0.5, G + hh + 2.2), 'z', 0.04, 3.0, seg=8, bevel=0.0)
    flag(p, (hx0 + 0.5, hy0 + 0.5), G + hh + 3.7, 1.4, 0.9, m='Player', phase=0.8)
    crate_stack(p, -8.0, -14.2, G + hh, 1, yaw=20, size=(1.2, 0.7, 0.45))
    # a escada externa: do pátio ao teto da casa, encostada no muro da rampa (−Y)
    steps = 20
    for k in range(steps):
        x0 = 2.0 - k * 0.3
        z1 = G + (k + 1) * (hh / steps)
        p.box('Plaster', x0 - 0.3, x0, -7.4, -BAY - 0.55, G, z1, bevel=0.0)
    for k in range(0, steps, 2):                                                           # o corrimão
        x = 2.0 - k * 0.3 - 0.15
        z = G + (k + 1) * (hh / steps)
        p.cyl('Trim', (x, -7.45, z + 0.5), 'z', 0.03, 1.0, seg=6, bevel=0.0)
    # ---------------------------------------------------------------- o pátio da esquerda (−Y, na frente): a gaiola de armas
    wall(p, -4.0, HX, -HY + 0.2, -HY + 0.7)                                                  # o muro de fora (−Y)
    wall(p, HX - 0.5, HX, -HY + 0.2, -BAY - 0.5)                                            # o muro da frente (+X)
    for (x, y) in ((HX - 0.25, -HY + 0.45), (HX - 0.25, -BAY - 0.75), (-4.0, -HY + 0.45), (4.0, -HY + 0.45), (HX - 0.25, -11.0)):
        pilaster(p, x, y)
    scribble(p, (HX, -11.0, G + 1.7), '+x')
    scribble(p, (6.0, -HY + 0.2, G + 1.6), '-y', size=1.6)
    # a gaiola: postes, travessas e os arames; dentro, a prateleira de foguetes e as caixas
    cx0, cx1, cy0, cy1, ch = 2.5, 9.5, -14.4, -8.2, 3.0
    posts = [(cx0, cy0), (cx0, cy1), (cx1, cy0), (cx1, cy1), ((cx0 + cx1) / 2, cy0), ((cx0 + cx1) / 2, cy1), (cx0, (cy0 + cy1) / 2), (cx1, (cy0 + cy1) / 2)]
    for (x, y) in posts:
        p.cyl('Trim', (x, y, G + ch / 2), 'z', 0.06, ch, seg=8, bevel=0.0)
    for z in (G + ch - 0.04, G + 1.0, G + 1.9):
        for y in (cy0, cy1):
            p.box('Trim' if z > G + 2.5 else 'Dark', cx0, cx1, y - 0.02, y + 0.02, z - 0.02, z + 0.02, bevel=0.0)
        for x in (cx0, cx1):
            p.box('Trim' if z > G + 2.5 else 'Dark', x - 0.02, x + 0.02, cy0, cy1, z - 0.02, z + 0.02, bevel=0.0)
    for k in range(1, 6):                                                                   # os arames verticais
        for y in (cy0, cy1):
            x = cx0 + (cx1 - cx0) * k / 6
            p.box('Dark', x - 0.012, x + 0.012, y - 0.012, y + 0.012, G, G + ch - 0.05, bevel=0.0)
        for x in (cx0, cx1):
            y = cy0 + (cy1 - cy0) * k / 6
            p.box('Dark', x - 0.012, x + 0.012, y - 0.012, y + 0.012, G, G + ch - 0.05, bevel=0.0)
    p.box('Dark', cx0 + 0.6, cx0 + 1.1, cy0 + 0.8, cy1 - 0.8, G, G + 1.6, bevel=0.02)         # a prateleira de foguetes
    for k in range(8):
        y = cy0 + 1.1 + k * 0.6
        p.cyl('Metal', (cx0 + 0.85, y, G + 0.9), 'z', 0.06, 1.7, seg=8, bevel=0.0)
        p.cyl('Dark', (cx0 + 0.85, y, G + 1.78), 'z', 0.1, 0.12, seg=8, bevel=0.0)
    crate_stack(p, cx1 - 1.2, cy0 + 1.4, G, 2, yaw=0, size=(1.2, 0.7, 0.45))
    crate_stack(p, cx1 - 1.2, cy1 - 1.4, G, 1, yaw=35, size=(1.2, 0.7, 0.45))
    p.box('Wood', 5.5, 8.5, -HY + 1.0, -HY + 2.0, G + 0.8, G + 0.88, bevel=0.01)             # a bancada
    for (x, y) in ((5.7, -HY + 1.1), (8.3, -HY + 1.1), (5.7, -HY + 1.9), (8.3, -HY + 1.9)):
        p.box('Wood', x - 0.04, x + 0.04, y - 0.04, y + 0.04, G, G + 0.8, bevel=0.0)
    barrel(p, 10.8, -14.6, G)
    barrel(p, 10.0, -15.0, G)
    crate_stack(p, -2.0, -14.0, G, 2, yaw=-15)
    p.cyl('Trim', (HX - 0.5, -HY + 0.5, G + PIL + 1.5), 'z', 0.04, 3.0, seg=8, bevel=0.0)
    flag(p, (HX - 0.5, -HY + 0.5), G + PIL + 3.0, 1.3, 0.85, m='Player', phase=2.0)
    # ---------------------------------------------------------------- o pátio da direita (+Y, na frente): muros com o canto chanfrado
    wall(p, ax1, 7.6, HY - 0.7, HY - 0.2)                                                   # o muro de fora (+Y), até o chanfro
    wall(p, HX - 0.5, HX, BAY + 0.5, 11.4)                                                  # o muro da frente (+X), até o chanfro
    p.obox('Plaster', (9.85, 13.55, G + WALL / 2), (6.2, 0.5, WALL), yaw=-45, bevel=0.06)      # o chanfro
    p.obox('Player', (9.85 + 0.18, 13.55 + 0.18, G + WALL - 0.28), (6.2, 0.02, 0.45), yaw=-45, bevel=0.0)
    for (x, y) in ((HX - 0.25, BAY + 0.75), (HX - 0.25, 11.2), (7.8, HY - 0.45), (ax1, HY - 0.45)):
        pilaster(p, x, y)
    scribble(p, (HX, 8.5, G + 1.7), '+x')
    crate_stack(p, 4.0, 13.5, G, 3, yaw=5)
    crate_stack(p, 6.2, 13.3, G, 2, yaw=-20)
    barrel(p, 1.0, 14.6, G)
    barrel(p, 1.9, 14.2, G)
    p.cyl('Trim', (HX - 0.5, 11.2, G + PIL + 1.5), 'z', 0.04, 3.0, seg=8, bevel=0.0)
    flag(p, (HX - 0.5, 11.2), G + PIL + 3.0, 1.3, 0.85, m='Player', phase=1.3)
    # ---------------------------------------------------------------- o muro de trás (−X) fechando o pátio da casa e o do armazém
    wall(p, -HX, -HX + 0.5, -HY + 0.2, hy0)
    wall(p, -HX, -HX + 0.5, ay1, HY - 0.2)
    return p.finish()


def scaffold():
    """O andaime da obra em volta do lote (some quando o prédio fica pronto): postes de madeira, tábuas
    por nível e o rodapé de perigo, como o dos provisórios da USA (blender_provisional.py)."""
    p = Part('gla_war_factory_scaffold')
    gap, bay, lift = 0.9, 3.0, 2.0
    X0, Y0, X1, Y1 = -HX - gap, -HY - gap, HX + gap, HY + gap
    z0, top = G, 8.0 + 1.0
    levels = max(2, int((top - z0) / lift))
    perim = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
    outward = [(0.0, -1.0), (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)]
    for i in range(4):
        (ax, ay), (bx, by) = perim[i], perim[(i + 1) % 4]
        ox, oy = outward[i]
        L = math.hypot(bx - ax, by - ay)
        n = max(1, int(L / bay))
        for k in range(n + 1):
            t = k / n
            px, py = ax + (bx - ax) * t, ay + (by - ay) * t
            for d in (0.0, 1.0):
                p.cyl('Wood', (px + ox * d, py + oy * d, (z0 + top) / 2), 'z', 0.06, top - z0, seg=8, bevel=0.0)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            for d in (0.0, 1.0):
                mx, my = (ax + bx) / 2 + ox * d, (ay + by) / 2 + oy * d
                if i in (0, 2):
                    p.box('Wood', min(ax, bx), max(ax, bx), my - 0.04, my + 0.04, z - 0.04, z + 0.04, bevel=0.0)
                else:
                    p.box('Wood', mx - 0.04, mx + 0.04, min(ay, by), max(ay, by), z - 0.04, z + 0.04, bevel=0.0)
            if i in (0, 2):
                yy = ay + oy * 0.5
                p.box('Crate', min(ax, bx), max(ax, bx), yy - 0.45, yy + 0.45, z - 0.06, z, bevel=0.01)
                p.box('Hazard', min(ax, bx), max(ax, bx), ay + oy - 0.02, ay + oy + 0.02, z, z + 0.18, bevel=0.0)
            else:
                xx = ax + ox * 0.5
                p.box('Crate', xx - 0.45, xx + 0.45, min(ay, by), max(ay, by), z - 0.06, z, bevel=0.01)
                p.box('Hazard', ax + ox - 0.02, ax + ox + 0.02, min(ay, by), max(ay, by), z, z + 0.18, bevel=0.0)
        for k in range(n):
            t0, t1 = k / n, (k + 1) / n
            pa = (ax + (bx - ax) * t0 + ox, ay + (by - ay) * t0 + oy)
            pb = (ax + (bx - ax) * t1 + ox, ay + (by - ay) * t1 + oy)
            zl = z0 + (top - z0) * ((k % levels) / levels)
            zh = z0 + (top - z0) * ((k % levels + 1) / levels)
            d = Vector((pb[0] - pa[0], pb[1] - pa[1], zh - zl))
            p.cyl('Wood', ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2, (zl + zh) / 2), (d.x, d.y, d.z), 0.04, d.length, seg=6, bevel=0.0)
    return p.finish()


def main():
    clear_scene()
    only = os.environ.get('MODEL_ONLY')
    for key, build in (('building', building), ('scaffold', scaffold)):
        if only and key not in only.split(','):
            continue
        o = build()
        export(o, OUT)
        bpy.data.objects.remove(o, do_unlink=True)


main()
