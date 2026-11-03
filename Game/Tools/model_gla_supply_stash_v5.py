"""O Supply Stash da GLA (o depósito de dinheiro), v5 (03/11/2026): O POÇO DO CONTRABANDO. Fiel ao conceito aprovado
docs/concept/buildings/gla_supply_stash_poco_APROVADO.jpg. O v1 a v4 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_supply_stash_v5.py captures/game/models/gla_supply_stash_v5

A palavra (o que lê em 1 segundo): O CASTELETE DE TRILHO COM A RODA GRANDE EM CIMA DO POÇO. O poço quadrado com a gola de blocos de
adobe (a faixa verde a pincel por fora, a faixa de perigo na boca, o forro de tábuas descendo pro escuro); em cima da gola +Y, o castelete de
trilho soldado (as 4 pernas de perfil I, os X de chapa, o quadro do topo em balanço sobre a boca) com a RODA raiada grande no alto (o plano dela em Y:
de frente pra câmera do jogo) e a bandeira; o cabo desce da roda pra gaiola com as caixas, pendurada no gancho; atrás (+Y), o cabo vai
pro tambor do guincho, tocado pelo motor de caminhão velho (à frente do castelete, do lado +Y) em cima dos blocos de madeira (o escapamento soltando a fumaça). A lona listrada de verde cobre a metade −Y da boca e
desce até as pilhas de caixotes do lado de fora; na frente (+X), a doca de tábuas onde a picape encosta de ré (a faixa de Y ±3,3 livre).

Lote 16 × 16 m, frente em +X. Alturas: a gola 1,35 m, a lona 2,45–3,65 m, o topo do castelete 6,4 m, a roda até ~8,0 m, a bandeira
até ~9,6 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_SupplyStash_Base: tudo o que não se mexe (a bandeira sai no flags.json);
  SM_GLA_SupplyStash_Wheel (gira; a origem no cubo, WHEEL_AT; o eixo é o X): a roda raiada com o eixo;
  SM_GLA_SupplyStash_Cage (sobe e desce em Z; a origem no topo do gancho, HOOK_AT): a gaiola com as caixas, as correntes e o gancho;
  SM_GLA_SupplyStash_Cable (a origem em cima, CABLE_TOP, o cabo descendo 1 m por unidade de escala Z × CABLE_LEN): o cabo da roda até o
    gancho; o jogo estica em Z quando a gaiola desce;
  SM_GLA_SupplyStash_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA e genéricos): Plaster (a gola), Player (o verde), Hazard (a faixa de perigo; o andaime), Rust (o castelete, o
  tambor, a roda), Metal, Wood (o forro, a doca, os blocos, as caixas de madeira), Cloth (a lona, os sacos), Crate (o motor, as caixas
  oliva), Net (a lona das caixas), Dark, Trim, Earth.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel  # noqa: E402
from model_building_parts import sweep, tube, catenary, paint_band_block, bake_in_place  # noqa: E402
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, prism, bag, crate, earth_pad, blob, flagpole, rope, stake,  # noqa: E402,F401
                                 surface, tire)

PREFIX = 'SM_GLA_SupplyStash'
# o poço: a boca (por dentro) e a gola
MX0, MX1, MY0, MY1 = -3.6, 0.4, -2.0, 2.0
CT, ZC = 1.0, 1.35                 # a espessura e a altura da gola
OX0, OX1, OY0, OY1 = MX0 - CT, MX1 + CT, MY0 - CT, MY1 + CT
# o castelete (trilho soldado) em cima da gola +Y: a linha do guincho (a roda, o cabo, o tambor) corre em Y, então a roda fica de frente
# pra câmera do jogo (com a linha em X ela aparecia de lado, um risco); o tambor e o motor atrás à esquerda, como no conceito visto do jogo
XC = -2.3                          # o plano da roda, do cabo e do tambor (x)
LEG_B = [(-3.45, 4.95), (-3.3, 2.3), (-1.2, 2.3), (-1.15, 4.95)]   # o pé das 4 pernas: as de trás na sapata, as da frente na gola +Y
LEG_T = [(-3.15, 4.45), (-3.15, 2.75), (-1.45, 2.75), (-1.45, 4.45)]
ZTOP = 6.3                         # o eixo das vigas do topo
BEAM_X = (-3.15, -1.45)            # as 2 vigas compridas do topo (correm em Y, em balanço até a roda)
# a roda, o cabo, a gaiola, o tambor
WR, WG = 1.35, 1.29                # o raio da roda e o do fundo do canal
HOOK_Y = 1.0
WHEEL_AT = (XC, HOOK_Y + WG, 6.62)
CABLE_TOP = (XC, HOOK_Y, WHEEL_AT[2])
HOOK_AT = (XC, HOOK_Y, 3.8)
CABLE_LEN = CABLE_TOP[2] - HOOK_AT[2]
CAGE = 1.3                         # o lado da gaiola
CAGE_Z0 = 1.55                     # o fundo da gaiola em repouso
DRUM = (XC, 4.2, 0.9)              # o tambor do guincho (eixo X)
DRUM_R = 0.55
FLY_X = -0.55                      # o volante entre o castelete e o motor (o motor à frente, +X: a câmera do jogo vê)
# a lona (a metade −Y da boca até fora da gola)
TARP_X = (-4.6, 1.3)
TARP_Y = (-0.45, -6.35)
TARP_Z = (3.65, 2.45)
STRIPES = [(-3.25, -2.6), (-1.15, -0.5)]
# a doca
DOCK_X = (1.6, 7.45)
DOCK_HW = 2.3
EXHAUST = (0.55, 3.38, 3.05)


# ===================================================================================== perfis
def iprof(w, h, tf, tw):
    return [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, -h / 2 + tf), (tw / 2, -h / 2 + tf), (tw / 2, h / 2 - tf), (w / 2, h / 2 - tf),
            (w / 2, h / 2), (-w / 2, h / 2), (-w / 2, h / 2 - tf), (-tw / 2, h / 2 - tf), (-tw / 2, -h / 2 + tf), (-w / 2, -h / 2 + tf)]


def ibeam(p, a, b, w=0.2, h=0.22, up=(0.0, 0.0, 1.0), m='Rust#beam'):
    """O trilho / a viga de perfil I (as abas e a alma) de a até b."""
    sweep(p, m, [a, b], iprof(w, h, 0.035, 0.03), up=up)


def bar(p, a, b, w=0.12, t=0.025, up=(0.0, 0.0, 1.0), m='Rust#brace'):
    """A chapa chata (o X de travamento) de a até b."""
    sweep(p, m, [a, b], [(-t / 2, -w / 2), (t / 2, -w / 2), (t / 2, w / 2), (-t / 2, w / 2)], up=up)


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(len(a)))


# ===================================================================================== o poço
def collar(p):
    """A gola de adobe: as 4 paredes de blocos grandes (a quina gasta), os 4 blocos de quina maiores e mais altos, a faixa verde a pincel
    por fora, a faixa de perigo na boca, o forro de tábuas e os quadros de madeira descendo pro escuro, o fundo escuro."""
    walls = [(MX1, OX1, MY0, MY1), (OX0, MX0, MY0, MY1), (MX0, MX1, MY1, OY1), (MX0, MX1, OY0, MY0)]
    for (x0, x1, y0, y1) in walls:
        ex = 0.001 if (x1 - x0) > (y1 - y0) else 0.0
        ey = 0.001 if (y1 - y0) > (x1 - x0) else 0.0
        p.box('Plaster', x0 - ex, x1 + ex, y0 - ey, y1 + ey, -0.02, ZC)
    for (x0, x1) in ((OX0 - 0.06, MX0), (MX1, OX1 + 0.06)):
        for (y0, y1) in ((OY0 - 0.06, MY0), (MY1, OY1 + 0.06)):
            p.box('Plaster', x0, x1, y0, y1, -0.02, ZC + 0.12)
    # as juntas dos blocos (a fresta entre os blocos grandes de cada parede: o ritmo do conceito)
    for (x0, x1, y0, y1) in walls:
        along_x = (x1 - x0) > (y1 - y0)
        L = (x1 - x0) if along_x else (y1 - y0)
        n = 2 if L > 3 else 1
        for k in range(1, n + 1):
            t = k / (n + 1)
            if along_x:
                x = x0 + (x1 - x0) * t
                for y in (y0, y1):
                    if (y == OY0) or (y == OY1):
                        p.box('Dark#joint', x - 0.018, x + 0.018, y - 0.012, y + 0.012, 0.0, ZC - 0.1, bevel=0.0)
            else:
                y = y0 + (y1 - y0) * t
                for x in (x0, x1):
                    if (x == OX0) or (x == OX1):
                        p.box('Dark#joint', x - 0.012, x + 0.012, y - 0.018, y + 0.018, 0.0, ZC - 0.1, bevel=0.0)
    # a faixa verde a pincel por fora (nas paredes e nos blocos de quina)
    zl, zh = 0.5, 0.8
    paint_band_block(p, MX1, OX1, MY0, MY1, zl, zh, faces='+x', seed=1)
    paint_band_block(p, OX0, MX0, MY0, MY1, zl, zh, faces='-x', seed=2)
    paint_band_block(p, MX0, MX1, MY1, OY1, zl, zh, faces='+y', seed=3)
    paint_band_block(p, MX0, MX1, OY0, MY0, zl, zh, faces='-y', seed=4)
    for (x0, x1, fx) in ((OX0 - 0.06, MX0, '-x'), (MX1, OX1 + 0.06, '+x')):
        for (y0, y1, fy) in ((OY0 - 0.06, MY0, '-y'), (MY1, OY1 + 0.06, '+y')):
            paint_band_block(p, x0, x1, y0, y1, zl, zh, faces=fx + fy, seed=int(x0 * 3 + y0 * 7))
    # a faixa de perigo na boca (por dentro, logo abaixo da quina gasta) e na beira de cima da parede da frente
    zh0, zh1 = ZC - 0.44, ZC - 0.17
    p.box('Hazard#lip', MX1 - 0.016, MX1, MY0, MY1, zh0, zh1, bevel=0.0)
    p.box('Hazard#lip', MX0, MX0 + 0.016, MY0, MY1, zh0, zh1, bevel=0.0)
    p.box('Hazard#lip', MX0, MX1, MY1 - 0.016, MY1, zh0, zh1, bevel=0.0)
    p.box('Hazard#lip', MX0, MX1, MY0, MY0 + 0.016, zh0, zh1, bevel=0.0)
    # o forro de tábuas em pé nas 4 faces de dentro, os quadros de madeira nos cantos, o fundo escuro
    zf0, zf1 = 0.09, zh0 - 0.03
    for (fx, a0, a1, fixed, sgn) in (('y', MY0, MY1, MX1, -1), ('y', MY0, MY1, MX0, 1), ('x', MX0, MX1, MY1, -1), ('x', MX0, MX1, MY0, 1)):
        n = int((a1 - a0) / 0.25)
        for k in range(n):
            u0 = a0 + (a1 - a0) * k / n + 0.015
            u1 = a0 + (a1 - a0) * (k + 1) / n - 0.015
            top = zf1 - 0.05 * _hash(k, int(fixed * 10), 3)
            d0, d1 = (fixed, fixed + sgn * 0.045)
            if fx == 'y':
                p.box('Wood#lining', min(d0, d1), max(d0, d1), u0, u1, zf0, top, bevel=0.012)
            else:
                p.box('Wood#lining', u0, u1, min(d0, d1), max(d0, d1), zf0, top, bevel=0.012)
    for z in (0.35, zf1 - 0.12):
        for (x0, x1, y0, y1) in ((MX0 + 0.04, MX1 - 0.04, MY0 + 0.045, MY0 + 0.2), (MX0 + 0.04, MX1 - 0.04, MY1 - 0.2, MY1 - 0.045),
                                 (MX0 + 0.045, MX0 + 0.2, MY0 + 0.2, MY1 - 0.2), (MX1 - 0.2, MX1 - 0.045, MY0 + 0.2, MY1 - 0.2)):
            p.box('Wood#set', x0, x1, y0, y1, z - 0.08, z + 0.08, bevel=0.02)
    p.box('Dark#pit', MX0, MX1, MY0, MY1, 0.06, 0.1, bevel=0.0)


# ===================================================================================== o castelete
def headframe(p):
    """O castelete de trilho soldado em cima da gola +Y: as 4 pernas de perfil I (as da frente sentadas na gola, as de trás nas sapatas), as
    cintas em 2 níveis, os X de chapa (menos no lance de cima da frente, onde a roda passa), o quadro do topo com as 2 vigas em balanço
    até a roda (nenhuma travessa no disco), as mãos-francesas, os mancais, o estrado de trás, a bandeira, os 2 estais."""
    zb = [0.0, ZC, ZC, 0.0]
    for i in range(4):
        bx, by = LEG_B[i]
        if zb[i] == 0.0:
            p.box('Plaster#pad', bx - 0.38, bx + 0.38, by - 0.38, by + 0.38, -0.05, 0.25, bevel=0.06)
            zb[i] = 0.25
        p.box('Metal#plate', bx - 0.24, bx + 0.24, by - 0.24, by + 0.24, zb[i], zb[i] + 0.04, bevel=0.0)
    legs = []
    for i in range(4):
        a = (LEG_B[i][0], LEG_B[i][1], zb[i] + 0.04)
        b = (LEG_T[i][0], LEG_T[i][1], ZTOP)
        ibeam(p, a, b, w=0.22, h=0.22, up=(1.0, 0.0, 0.0))
        legs.append((a, b))

    def leg_at(i, z):
        a, b = legs[i]
        t = (z - a[2]) / (b[2] - a[2])
        return lerp(a, b, t)
    for z in (2.6, 4.5):
        for i in range(4):
            ibeam(p, leg_at(i, z), leg_at((i + 1) % 4, z), w=0.14, h=0.16)
    # os X de chapa em cada face e cada lance (a face da frente, 1→2, sem o X de cima: a roda desce ali)
    lv = [ZC + 0.5, 2.6, 4.5, ZTOP - 0.12]
    for k in range(3):
        for i in range(4):
            j = (i + 1) % 4
            if i == 1 and k == 2:
                continue
            za, zb_ = lv[k], lv[k + 1]
            bar(p, leg_at(i, max(za, zb[i] + 0.3)), leg_at(j, zb_), up=(0.0, 0.0, 1.0))
            bar(p, leg_at(j, max(za, zb[j] + 0.3)), leg_at(i, zb_), up=(0.0, 0.0, 1.0))
    # o quadro do topo: as 2 vigas compridas em balanço até passar da roda, as travessas só atrás dela, as mãos-francesas da frente
    wx, wy, wz = WHEEL_AT
    yf = wy - 0.75
    for x in BEAM_X:
        ibeam(p, (x, LEG_T[0][1] + 0.25, ZTOP), (x, yf, ZTOP), w=0.18, h=0.25)
    for y in (LEG_T[0][1], 4.0):
        ibeam(p, (BEAM_X[0] - 0.12, y, ZTOP - 0.03), (BEAM_X[1] + 0.12, y, ZTOP - 0.03), w=0.14, h=0.18)
    for (i, x) in ((1, BEAM_X[0]), (2, BEAM_X[1])):
        ibeam(p, leg_at(i, 4.6), (x, yf + 0.15, ZTOP - 0.13), w=0.14, h=0.14, up=(0.0, 0.0, 1.0))
    # as chapas de ligação (os gussets) nas juntas do topo
    for (x, y) in [(BEAM_X[0], LEG_T[0][1]), (BEAM_X[1], LEG_T[0][1]), (BEAM_X[0], LEG_T[1][1]), (BEAM_X[1], LEG_T[1][1])]:
        p.box('Metal#gusset', x - 0.2, x + 0.2, y - 0.2, y + 0.2, ZTOP + 0.125, ZTOP + 0.145, bevel=0.0)
        for (dx, dy) in ((-0.13, -0.13), (0.13, -0.13), (-0.13, 0.13), (0.13, 0.13)):
            p.cyl('Dark#bolt', (x + dx, y + dy, ZTOP + 0.16), 'z', 0.03, 0.04, seg=6, bevel=0.0)
    # os mancais da roda nas 2 vigas (o bloco, a caixa redonda, a base)
    for x in BEAM_X:
        p.box('Dark#bearing', x - 0.11, x + 0.11, wy - 0.2, wy + 0.2, ZTOP + 0.125, wz - 0.05, bevel=0.02)
        p.cyl('Rust#bearing', (x, wy, wz), 'x', 0.16, 0.26, seg=16, bevel=0.0)
        p.box('Dark#bearing', x - 0.13, x + 0.13, wy - 0.3, wy + 0.3, ZTOP + 0.125, ZTOP + 0.18, bevel=0.0)
    # o estrado de tábuas atrás da roda (fora do caminho do cabo de trás)
    for k in range(2):
        y = 4.15 + k * 0.24
        p.box('Wood#plank', BEAM_X[0] - 0.1, BEAM_X[1] + 0.1, y - 0.11, y + 0.11, ZTOP + 0.125, ZTOP + 0.17, bevel=0.01)
    # a bandeira no canto de trás à esquerda, alta
    flagpole(p, LEG_T[0][0] + 0.02, LEG_T[0][1] + 0.02, ZTOP + 0.17, 3.0, length=1.35, height=0.85, phase=0.6, m_pole='Trim')
    # os 2 estais: da quina da frente +X até a estaca de fora (fora da faixa da picape); da quina de trás −X até a estaca de trás
    for (top, gnd) in (((LEG_T[2][0], LEG_T[2][1], ZTOP), (1.7, 6.3, 0.0)), ((LEG_T[0][0], LEG_T[0][1], ZTOP), (-6.7, 7.1, 0.0))):
        st = stake(p, gnd[0], gnd[1], toward=(top[0], top[1]))
        rope(p, top, st, sag=0.15, r=0.016, m='Dark#guy')


# ===================================================================================== o guincho e o motor
def winch(p):
    """O tambor do guincho entre as pernas de trás (eixo X: os flanges, as voltas do cabo), o eixo até o volante, o volante grande, o
    motor de caminhão velho em cima dos blocos de madeira (o bloco, o cabeçote, o cárter, o radiador com a grade na ponta −X; do lado −Y,
    o que a câmera vê, o coletor e o cano de escape subindo), os tambores de combustível, e o cabo: do tambor sobe pra trás da roda, dá a
    volta por cima no canal (parado: a roda gira por baixo) e desce pra boca (a peça _Cable)."""
    dx, dy, dz = DRUM
    for x in (dx - 0.62, dx + 0.62):
        for j in range(3):
            if j % 2 == 0:
                p.box('Wood#crib', x - 0.11, x + 0.11, dy - 0.55, dy + 0.55, j * 0.15, j * 0.15 + 0.15, bevel=0.012)
            else:
                for yy in (dy - 0.4, dy + 0.4):
                    p.box('Wood#crib', x - 0.25, x + 0.25, yy - 0.1, yy + 0.1, j * 0.15, j * 0.15 + 0.15, bevel=0.012)
        p.box('Rust#frame', x - 0.06, x + 0.06, dy - 0.18, dy + 0.18, 0.45, dz + 0.05, bevel=0.0)
    p.cyl('Rust#drum', (dx, dy, dz), 'x', DRUM_R, 0.82, seg=28, bevel=0.0, smooth=True)
    for s_ in (-1, 1):
        p.cyl('Rust#flange', (dx + s_ * 0.43, dy, dz), 'x', 0.72, 0.05, seg=28, bevel=0.0)
    for k in range(6):
        p.torus('Dark#wrap', (dx - 0.33 + k * 0.13, dy, dz), 'x', DRUM_R + 0.025, 0.028, seg=28, rseg=5)
    p.cyl('Metal#shaft', ((dx + FLY_X) / 2, dy, dz), 'x', 0.07, FLY_X - dx + 0.3, seg=12, bevel=0.0)
    # o volante (a polia grande) entre o castelete e o motor, a face rebaixada com os furos virada pro castelete
    p.cyl('Rust#fly', (FLY_X, dy, dz), 'x', 0.66, 0.16, seg=32, bevel=0.0)
    p.cyl('Dark#fly', (FLY_X - 0.085, dy, dz), 'x', 0.5, 0.02, seg=28, bevel=0.0)
    for k in range(6):
        a = 2 * math.pi * k / 6
        p.cyl('Dark#flyhole', (FLY_X - 0.09, dy + 0.32 * math.cos(a), dz + 0.32 * math.sin(a)), 'x', 0.08, 0.02, seg=10, bevel=0.0)
    p.cyl('Metal#hub', (FLY_X - 0.12, dy, dz), 'x', 0.13, 0.12, seg=12, bevel=0.0)
    # o motor (o virabrequim em X) nos blocos de madeira, à frente do castelete: a capa do volante na ponta −X, o radiador na ponta +X
    ex0, ex1, ey0, ey1 = FLY_X + 0.2, FLY_X + 1.95, dy - 0.62, dy + 0.62
    for x in (ex0 + 0.3, ex1 - 0.35):
        for j in range(3):
            if j % 2 == 0:
                p.box('Wood#crib', x - 0.12, x + 0.12, ey0 - 0.25, ey1 + 0.25, j * 0.15, j * 0.15 + 0.15, bevel=0.012)
            else:
                for yy in (ey0 - 0.05, ey1 + 0.05):
                    p.box('Wood#crib', x - 0.3, x + 0.3, yy - 0.11, yy + 0.11, j * 0.15, j * 0.15 + 0.15, bevel=0.012)
    for y in (ey0 + 0.1, ey1 - 0.1):
        p.box('Rust#skid', ex0 - 0.1, ex1 + 0.15, y - 0.08, y + 0.08, 0.45, 0.6, bevel=0.0)
    zb = 0.6
    p.box('Crate#engine', ex0 + 0.25, ex1 - 0.35, ey0 + 0.12, ey1 - 0.12, zb + 0.15, zb + 0.95, bevel=0.04)            # o bloco
    p.box('Crate#engine', ex0 + 0.35, ex1 - 0.45, ey0 + 0.25, ey1 - 0.25, zb + 0.95, zb + 1.22, bevel=0.04)            # o cabeçote
    p.box('Dark#pan', ex0 + 0.35, ex1 - 0.45, ey0 + 0.22, ey1 - 0.22, zb, zb + 0.15, bevel=0.0)
    for k in range(6):
        x = ex0 + 0.45 + k * (ex1 - ex0 - 1.0) / 5
        p.cyl('Dark#plug', (x, dy, zb + 1.27), 'z', 0.035, 0.1, seg=6, bevel=0.0)
    p.box('Crate#bell', ex0 - 0.12, ex0 + 0.28, ey0 + 0.2, ey1 - 0.2, zb + 0.05, zb + 0.85, bevel=0.06)               # a capa do volante
    # o radiador na ponta +X (a colmeia escura, a moldura oliva, a grade virada pra câmera) e a tampa
    p.box('Crate#rad', ex1 - 0.3, ex1 - 0.08, ey0 + 0.02, ey1 - 0.02, zb, zb + 1.45, bevel=0.03)
    p.box('Dark#core', ex1 - 0.09, ex1 - 0.05, ey0 + 0.12, ey1 - 0.12, zb + 0.12, zb + 1.33, bevel=0.0)
    for k in range(9):
        z = zb + 0.18 + k * 0.13
        p.box('Crate#grille', ex1 - 0.07, ex1 - 0.03, ey0 + 0.1, ey1 - 0.1, z - 0.02, z + 0.02, bevel=0.0)
    p.cyl('Metal#cap', (ex1 - 0.2, dy, zb + 1.5), 'z', 0.06, 0.08, seg=10, bevel=0.0)
    # o lado −Y (o que a câmera vê): o coletor de escape, o cano subindo com o chapéu; o motor de arranque; do lado +Y o alternador
    for k in range(3):
        x = ex0 + 0.55 + k * 0.38
        p.box('Rust#manifold', x - 0.1, x + 0.1, ey0 + 0.02, ey0 + 0.14, zb + 0.75, zb + 0.95, bevel=0.0)
    p.cyl('Rust#manifold', (ex0 + 0.93, ey0 + 0.02, zb + 0.95), 'x', 0.09, 0.95, seg=10, bevel=0.0)
    tube(p, 'Rust#exhaust', [(ex0 + 1.4, ey0 + 0.02, zb + 0.95), (ex0 + 1.2, ey0 - 0.1, zb + 1.1), (EXHAUST[0], EXHAUST[1], zb + 1.3),
                             (EXHAUST[0], EXHAUST[1], EXHAUST[2] - 0.15)], 0.075, seg=10)
    p.cyl('Dark#cap', (EXHAUST[0], EXHAUST[1], EXHAUST[2] - 0.08), 'z', 0.1, 0.06, seg=10, bevel=0.0)
    p.cyl('Metal#starter', (ex0 + 0.45, ey0 + 0.05, zb + 0.35), 'x', 0.11, 0.35, seg=12, bevel=0.0)
    p.cyl('Metal#alt', (ex1 - 0.5, ey1 - 0.05, zb + 1.0), 'x', 0.13, 0.22, seg=12, bevel=0.0)
    p.cyl('Crate#filter', (ex0 + 0.45, dy - 0.15, zb + 1.38), 'z', 0.2, 0.18, seg=16, bevel=0.0)
    # os tambores de combustível, o galão e a mangueira (fora da faixa da picape: y > 3,3)
    barrel(p, ex1 + 0.5, dy - 0.32, 0.0, r=0.3, h=0.9, m='Rust')
    barrel(p, ex1 + 0.45, dy + 0.42, 0.0, r=0.3, h=0.9, m='Crate')
    tube(p, 'Dark#hose', [(ex1 + 0.45, dy + 0.42, 0.92), (ex1 + 0.2, dy + 0.3, 0.75), (ex1 - 0.1, dy, 0.85)], 0.02, seg=6)
    # o cabo (no plano x = XC): do tambor sobe até a tangente de trás da roda, dá a volta por cima no canal e chega na frente (y = HOOK_Y)
    wx, wy, wz = WHEEL_AT
    r = WG + 0.02
    vy, vz = dy - wy, dz + DRUM_R - wz
    dep = math.atan2(vz, vy) + math.acos(min(1.0, r / math.hypot(vy, vz)))
    tube(p, 'Dark#cable', [(dx, dy, dz + DRUM_R + 0.03), (dx, wy + r * math.cos(dep), wz + r * math.sin(dep))], 0.028, seg=8)
    n = 24
    arc = [(dx, wy + r * math.cos(math.pi + (dep - math.pi) * k / n), wz + r * math.sin(math.pi + (dep - math.pi) * k / n)) for k in range(n + 1)]
    tube(p, 'Dark#cable', arc, 0.028, seg=8)


# ===================================================================================== a lona e as caixas
def tarp_z(x, y):
    """A lona de meia-água: alta na cumeeira (em cima da boca, y = −0,45), baixa na beira de fora; a barriga entre as varas, as dobras de
    tensão que saem dos cantos amarrados."""
    u = (x - TARP_X[0]) / (TARP_X[1] - TARP_X[0])
    v = (y - TARP_Y[0]) / (TARP_Y[1] - TARP_Y[0])
    z = TARP_Z[0] + (TARP_Z[1] - TARP_Z[0]) * v
    z -= 0.28 * math.sin(math.pi * u) * math.sin(math.pi * v)
    z -= 0.06 * math.sin(math.pi * v) * (0.5 + 0.5 * math.sin(u * 13.0 + 0.7)) * math.sin(math.pi * u)
    for (cu, cv) in ((0.0, 1.0), (1.0, 1.0), (0.0, 0.0), (1.0, 0.0)):
        r = math.hypot((u - cu) * 5.9, (v - cv) * 5.9)
        if r > 1e-3:
            a = math.atan2(v - cv, u - cu)
            z += 0.045 * (r / 0.6) * math.exp(-r / 1.4) * math.sin(6 * a + cu * 2 + cv)
    return z


def tarp(p):
    """A lona listrada: o pano cru com as 2 listras verdes descendo a água, a aba solta pendurada na cumeeira sobre a boca, as 4 varas
    (2 em cima da gola, 2 no chão de fora), a corda da cumeeira e as cordas até as estacas."""
    x0, x1 = TARP_X
    y0, y1 = TARP_Y

    def fn(uu, vv):
        x = x0 - 0.15 + (x1 - x0 + 0.3) * uu
        y = y0 + (y1 - y0 - 0.1) * vv
        return (x, y, tarp_z(x, y))
    surface(p, 'Cloth#tarp', fn, 34, 32, thick=0.03)
    for (sa, sb) in STRIPES:
        def fs(uu, vv, sa=sa, sb=sb):
            x = sa + (sb - sa) * uu
            y = y0 + 0.02 + (y1 - y0 - 0.14) * vv
            wob = 0.03 * math.sin(vv * 11 + sa)
            return (x + wob, y, tarp_z(x + wob, y) + 0.022)
        surface(p, 'Player#stripe', fs, 4, 32, thick=0.012)
    # a aba solta na cumeeira, caindo sobre a boca (o pano dobrado com a onda)
    b = []
    for k in range(25):
        x = x0 - 0.1 + (x1 - x0 + 0.2) * k / 24
        b.append(x)

    def fa(uu, vv):
        x = x0 - 0.1 + (x1 - x0 + 0.2) * uu
        z0 = tarp_z(x, y0) - 0.02
        return (x, y0 + 0.03 + 0.07 * math.sin(uu * 19.0) * vv, z0 - 0.55 * vv - 0.12 * math.sin(uu * 7.0 + 1) * vv)
    surface(p, 'Cloth#flap', fa, 24, 3, thick=0.025)
    # as varas: as 2 de dentro em cima da gola (na cumeeira), as 2 de fora no chão; a corda da cumeeira
    poles = [(x0 + 0.15, y0, ZC, TARP_Z[0] + 0.12), (x1 - 0.4, y0, ZC, TARP_Z[0] + 0.1),
             (x0 + 0.3, y1 + 0.12, 0.0, TARP_Z[1] + 0.1), (x1 - 0.3, y1 + 0.12, 0.0, TARP_Z[1] + 0.08)]
    for (x, y, za, zt) in poles:
        rod(p, 'Wood#pole', (x, y, za), (x, y, zt), 0.065, seg=8)
        p.cyl('Dark#cap', (x, y, zt + 0.02), 'z', 0.08, 0.05, seg=8, bevel=0.0)
    rope(p, (x0 + 0.15, y0, TARP_Z[0] + 0.05), (x1 - 0.4, y0, TARP_Z[0] + 0.05), sag=0.12, r=0.018, m='Dark#rope')
    for (x, y, za, zt) in poles[2:]:
        st = stake(p, x + (0.6 if x > 0 else -0.6), y - 0.85, toward=(x, y))
        rope(p, (x, y, zt - 0.05), st, sag=0.05, r=0.016, m='Dark#rope')


def stores(p):
    """As pilhas de caixotes debaixo da lona (fora da gola, lado −Y): as caixas oliva compridas, as de madeira, a pilha coberta com a lona
    oliva amarrada; os sacos e o galão no pé da gola (+Y)."""
    stacks = [(-3.5, -4.1, 2, 'Crate', (1.3, 0.62, 0.48), 8), (-3.35, -5.2, 1, 'Wood#crate', (0.9, 0.8, 0.6), -6),
              (-2.0, -4.4, 3, 'Wood#crate', (0.85, 0.8, 0.58), 12), (-0.75, -4.0, 2, 'Crate', (1.4, 0.55, 0.44), -4),
              (-0.4, -5.3, 1, 'Crate', (1.5, 0.5, 0.42), 80), (0.6, -4.5, 2, 'Wood#crate', (0.9, 0.85, 0.55), 20)]
    for (x, y, n, m, size, yaw) in stacks:
        for k in range(n):
            crate(p, x + 0.04 * (k % 2), y - 0.03 * k, k * size[2], size=size, yaw=yaw + (k % 2) * 5, m=m)
    # a pilha coberta: as caixas no estrado com a lona oliva caída e as cordas
    cx, cy = -1.75, -5.6
    for j in range(5):
        p.box('Wood#pallet', cx - 0.6, cx + 0.6, cy - 0.5 + j * 0.24, cy - 0.5 + j * 0.24 + 0.16, 0.1, 0.14, bevel=0.0)
    for j in range(3):
        p.box('Wood#pallet', cx - 0.6 + j * 0.55, cx - 0.5 + j * 0.55, cy - 0.5, cy + 0.5, 0.0, 0.1, bevel=0.0)
    p.obox('Net#cover', (cx, cy, 0.62), (1.18, 1.0, 0.95), yaw=4, bevel=0.07)
    for o in (-0.3, 0.3):
        p.obox('Dark#rope', (cx + o, cy, 0.62), (0.03, 1.02, 0.97), yaw=4, bevel=0.0)
    # o pé da gola do lado +Y: os 2 sacos encostados, o galão, a caixa oliva, o tambor
    for (x, y, yaw) in ((-0.95, OY1 + 0.3, 4), (-0.25, OY1 + 0.32, -6)):
        bag(p, (x, y, 0.3), (0.72, 0.46, 0.48), yaw=yaw)
    p.obox('Rust#can', (0.4, OY1 + 0.22, 0.26), (0.36, 0.18, 0.5), yaw=15, bevel=0.03)
    crate(p, -4.4, OY1 + 0.75, 0.0, size=(0.8, 0.55, 0.5), yaw=-8)
    tire(p, (-4.1, OY1 + 1.7, 0.13), R=0.38, r=0.13)


def dock(p):
    """A doca de tábuas na frente (+X) onde a picape encosta de ré: as travessas no chão, as tábuas atravessadas (com a fresta), as
    vigas de beira (baixas: a faixa de Y ±3,3 fica livre), o calço de batente junto da gola, a rampa de tábuas na ponta."""
    x0, x1 = DOCK_X
    for k in range(int((x1 - x0) / 1.2) + 1):
        x = x0 + 0.15 + k * 1.2
        p.box('Wood#sleeper', x - 0.1, x + 0.1, -DOCK_HW, DOCK_HW, 0.0, 0.07, bevel=0.0)
    n = int((x1 - x0) / 0.25)
    for k in range(n):
        xa = x0 + k * (x1 - x0) / n + 0.01
        xb = x0 + (k + 1) * (x1 - x0) / n - 0.015
        dz = 0.01 * (_hash(k, 1, 5) - 0.5)
        p.box('Wood#deck', xa, xb, -DOCK_HW + 0.08 * _hash(k, 2, 5), DOCK_HW - 0.08 * _hash(k, 3, 5), 0.07, 0.115 + dz, bevel=0.012)
    for s in (-1, 1):
        p.box('Wood#curb', x0, x1 + 0.35, s * DOCK_HW - 0.11, s * DOCK_HW + 0.11, 0.0, 0.2, bevel=0.03)
    p.box('Wood#chock', x0 + 0.02, x0 + 0.24, -1.5, 1.5, 0.11, 0.3, bevel=0.03)
    for y in (-1.3, 1.3):
        p.box('Dark#iron', x0 + 0.01, x0 + 0.25, y - 0.05, y + 0.05, 0.3, 0.31, bevel=0.0)
    # a rampa de tábuas descendo na ponta (do convés pro chão)
    for k in range(8):
        y0 = -DOCK_HW + 0.06 + k * (2 * DOCK_HW - 0.12) / 8
        y1 = y0 + (2 * DOCK_HW - 0.12) / 8 - 0.025
        prism(p, 'Wood#ramp', [(x1, y0, 0.115), (x1 + 0.48, y0, 0.0), (x1 + 0.48, y0, 0.035), (x1, y0, 0.155)], (0.0, y1 - y0, 0.0))


def base():
    p = new_part(PREFIX + '_Base')
    for m, b in (('Rust#beam', 0.0), ('Rust#brace', 0.0), ('Cloth#tarp', 0.0), ('Player#stripe', 0.0), ('Cloth#flap', 0.0),
                 ('Wood#lining', 0.012), ('Wood#deck', 0.012), ('Plaster#pad', 0.06), ('Wood#ramp', 0.0)):
        p.bm(m, b)
    earth_pad(p, (-1.0, 0.0), blob(-1.0, 0.0, 6.6, 7.1, seed=3, amp=0.04), h=0.05)
    collar(p)
    headframe(p)
    winch(p)
    tarp(p)
    stores(p)
    dock(p)
    # as pedras e os torrões no pé da gola
    for k in range(30):
        side = k % 4
        t = _hash(k, 1, 31)
        if side == 0:
            x, y = OX0 - 0.2 - 0.4 * _hash(k, 2, 31), OY0 + (OY1 - OY0) * t
        elif side == 1:
            x, y = OX0 + (OX1 - OX0) * t, OY1 + 0.25 + 0.4 * _hash(k, 2, 31)
        elif side == 2:
            x, y = OX0 + (OX1 - OX0) * t, OY0 - 0.2 - 0.3 * _hash(k, 2, 31)
        else:
            x, y = OX1 + 0.15 + 0.15 * _hash(k, 2, 31), OY0 + (OY1 - OY0) * t
            if abs(y) < DOCK_HW + 0.3:
                continue
        s = 0.07 + 0.13 * _hash(k, 3, 31) ** 2
        p.obox('Plaster#stone', (x, y, s * 0.3), (s * 1.3, s, s * 0.8), yaw=_hash(k, 4, 31) * 180, pitch=(_hash(k, 5, 31) - 0.5) * 30, bevel=0.03)
    return finish(p)


# ===================================================================================== as peças que se mexem
def revolve_x(p, m, c, prof, seg=48, closed=True):
    """O sólido de revolução em volta do eixo X por c: prof = [(raio, x)...] (fechado: o perfil volta ao começo)."""
    rings = [[(c[0] + t, c[1] + r * math.cos(2 * math.pi * k / seg), c[2] + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
             for (r, t) in prof]
    if closed:
        rings.append(rings[0])
    p.loft(m, rings, bevel=0.0, smooth=True, caps=(False, False) if closed else (True, True))


def wheel():
    """A roda raiada do castelete (a origem no cubo, o eixo X): o aro de canal (as 2 abas e o fundo onde corre o cabo), os 10 raios de
    chapa, o cubo com a tampa e os parafusos, o eixo passando nos 2 mancais."""
    p = new_part(PREFIX + '_Wheel')
    for m in ('Rust#rim', 'Rust#spoke'):
        p.bm(m, 0.0)
    c = (0.0, 0.0, 0.0)
    revolve_x(p, 'Rust#rim', c, [(1.18, -0.13), (WR, -0.13), (WR, -0.085), (WG + 0.01, -0.03), (WG, 0.0), (WG + 0.01, 0.03), (WR, 0.085),
                                  (WR, 0.13), (1.18, 0.13), (1.16, 0.0)], seg=60)
    for k in range(10):
        a = 2 * math.pi * k / 10
        r0, r1 = 0.26, 1.2
        mid = (0.0, (r0 + r1) / 2 * math.cos(a), (r0 + r1) / 2 * math.sin(a))
        p.obox('Rust#spoke', mid, (r1 - r0, 0.05, 0.13), yaw=90.0, pitch=math.degrees(a), bevel=0.0)
        p.obox('Rust#spoke', (0.0, r1 * math.cos(a) * 0.97, r1 * math.sin(a) * 0.97), (0.06, 0.2, 0.17), yaw=90.0, pitch=math.degrees(a), bevel=0.0)
    p.cyl('Rust#hub', c, 'x', 0.3, 0.32, seg=24, bevel=0.0)
    for s in (-1, 1):
        p.cyl('Dark#hubcap', (s * 0.17, 0.0, 0.0), 'x', 0.2, 0.04, seg=20, bevel=0.0)
        for k in range(6):
            a = 2 * math.pi * k / 6
            p.cyl('Dark#bolt', (s * 0.17, 0.24 * math.cos(a), 0.24 * math.sin(a)), 'x', 0.03, 0.06, seg=6, bevel=0.0)
    span = BEAM_X[1] - BEAM_X[0] + 0.3
    p.cyl('Metal#axle', ((BEAM_X[0] + BEAM_X[1]) / 2 - WHEEL_AT[0], 0.0, 0.0), 'x', 0.09, span, seg=12, bevel=0.0)
    return finish(p)


def cage():
    """A gaiola (a origem no topo do gancho): a armação de cantoneira, as ripas de madeira dos lados (com a fresta), o assoalho, as caixas
    dentro, as 4 correntes até o moitão, o moitão com o gancho."""
    p = new_part(PREFIX + '_Cage')
    hz = HOOK_AT[2]
    z0, z1 = CAGE_Z0 - hz, CAGE_Z0 + 1.1 - hz
    h = CAGE / 2
    for (x, y) in ((-h, -h), (h, -h), (h, h), (-h, h)):
        p.box('Rust#angle', x - 0.035, x + 0.035, y - 0.035, y + 0.035, z0, z1, bevel=0.0)
    for z in (z0, z0 + 0.55, z1 - 0.035):
        for (a, b, c_, d) in ((-h, h, -h, -h), (-h, h, h, h), (-h, -h, -h, h), (h, h, -h, h)):
            p.box('Rust#angle', min(a, b) - 0.035, max(a, b) + 0.035, min(c_, d) - 0.035, max(c_, d) + 0.035, z, z + 0.05, bevel=0.0)
    for s in (-1, 1):
        for k in range(6):
            u = -h + 0.12 + k * (CAGE - 0.24) / 5
            p.box('Wood#slat', u - 0.045, u + 0.045, s * h - 0.015, s * h + 0.015, z0 + 0.05, z1 - 0.05, bevel=0.0)
            p.box('Wood#slat', s * h - 0.015, s * h + 0.015, u - 0.045, u + 0.045, z0 + 0.05, z1 - 0.05, bevel=0.0)
    for k in range(6):
        u = -h + 0.05 + k * (CAGE - 0.1) / 6
        p.box('Wood#floor', u, u + (CAGE - 0.1) / 6 - 0.015, -h + 0.03, h - 0.03, z0 - 0.04, z0 + 0.01, bevel=0.0)
    crate(p, -0.18, -0.2, z0 + 0.01, size=(0.75, 0.55, 0.45), yaw=4, m='Crate')
    crate(p, 0.25, 0.28, z0 + 0.01, size=(0.6, 0.55, 0.5), yaw=-6, m='Wood#crate')
    crate(p, -0.05, 0.05, z0 + 0.48, size=(0.7, 0.5, 0.36), yaw=10, m='Wood#crate')
    # as 4 correntes até o moitão (os elos como argolas alternadas)
    bz = -0.45
    for (x, y) in ((-h, -h), (h, -h), (h, h), (-h, h)):
        a = Vector((x, y, z1))
        b = Vector((0.0, 0.0, bz))
        L = (b - a).length
        n = int(L / 0.09)
        d = (b - a).normalized()
        for k in range(n):
            q = a + d * (L * (k + 0.5) / n)
            ax = 'x' if k % 2 else 'y'
            p.torus('Dark#chain', tuple(q), ax, 0.035, 0.011, seg=8, rseg=4)
    # o moitão (as bochechas, a polia, o gancho) e a argola do topo
    p.box('Rust#block', -0.13, 0.13, -0.08, 0.08, bz, -0.12, bevel=0.02)
    p.cyl('Dark#sheave', (0.0, 0.0, -0.28), 'y', 0.11, 0.17, seg=16, bevel=0.0)
    p.torus('Dark#hook', (0.0, 0.0, bz - 0.07), 'y', 0.09, 0.025, seg=12, rseg=5)
    p.torus('Dark#eye', (0.0, 0.0, -0.06), 'x', 0.06, 0.018, seg=10, rseg=5)
    return finish(p)


def cable():
    """O cabo de aço da roda até o gancho (a origem em cima, CABLE_TOP; 1 m de cabo por unidade de escala em Z vezes CABLE_LEN)."""
    p = new_part(PREFIX + '_Cable')
    p.cyl('Dark#cable', (0.0, 0.0, -CABLE_LEN / 2), 'z', 0.028, CABLE_LEN, seg=8, bevel=0.0)
    return finish(p)


def write_preview(out):
    import json
    bl = lambda q: [q[0], -q[1], q[2]]
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Wheel', bl(WHEEL_AT)], [PREFIX + '_Cage', bl(HOOK_AT)],
                      [PREFIX + '_Cable', bl(CABLE_TOP)],
                      ['captures/game/models/gla_supply_pickup/SM_GLA_SupplyPickup_Hull', [4.55, 0.0, 0.0]],
                      ['captures/game/models/gla_supply_pickup/SM_GLA_SupplyPickup_Wheels', [4.55, 0.0, 0.0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1400]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1500, 1000]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1500, 1000]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1500, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1500, 1100], 30.0, 0.95]],
            'usa_at': [6.0, -12.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_supply_stash_v5')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(bake_in_place(wheel(), WHEEL_AT), out)
    clear_scene()
    total += export(bake_in_place(cage(), HOOK_AT), out)
    clear_scene()
    total += export(bake_in_place(cable(), CABLE_TOP), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -6.9, OX1 + 0.1, -6.6, 5.6, ZTOP + 0.4, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 16 m; gola', ZC, 'm; topo do castelete', ZTOP, 'm; roda ate', round(WHEEL_AT[2] + WR, 2), 'm')
    print('MODEL wheel_at_m', tuple(round(v, 3) for v in WHEEL_AT), 'eixo X (1, 0, 0); raio', WR)
    print('MODEL hook_at_m', tuple(round(v, 3) for v in HOOK_AT), '; fundo da gaiola z', CAGE_Z0, '; cable_top_m', tuple(round(v, 3) for v in CABLE_TOP),
          '; cable_len_m', round(CABLE_LEN, 3))
    print('MODEL saida da picape: +X, doca x', DOCK_X, 'y +-', DOCK_HW, '(faixa livre y +-3.3); batente x', DOCK_X[0] + 0.24)
    print('MODEL exhaust_m', EXHAUST)
    print('MODEL fire_points_m', [(0.5, 4.2, 1.4), (-1.8, -4.6, 1.0), (XC, 3.6, 6.5), (-1.6, -0.6, 0.6)])


if __name__ == '__main__':
    main()
