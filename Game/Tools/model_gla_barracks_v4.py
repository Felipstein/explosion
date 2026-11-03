"""O Quartel da GLA (Barracks), v4 (03/11/2026): A FILEIRA DE CÚPULAS DOURADAS. A direção fechada em docs/elenco/ICONES_03_11.md (5ª
rodada: o ônibus do v3 caiu por ser objeto de fora do jogo; o pedido é o ícone da família, autêntico, que todo mundo lembra, sem copiar
o ZH). O v1, o v2 e o v3 ficam como estão.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_barracks_v4.py captures/game/models/gla_barracks_v4

A palavra (o que lê em 1 segundo): 5 BOLHAS DOURADAS EM FILA. O salão comprido de adobe dos bazares cobertos persas (Isfahan, Tabriz,
Alepo: a rua coberta de cúpulas em fila, cada uma com o lanternim que deixa entrar a luz), aqui com as cúpulas DOURADAS (o dourado das
cúpulas do quartel do ZH, que não se confunde com o turquesa do Palace): cada cúpula no tambor de reboco com as janelinhas, a faixa verde
(a cor do jogador) no pé, os gomos e o lanternim no topo. Na frente (+X), o portal alto (o pishtaq) com o arco fundo e a porta de onde
a infantaria sai, as 2 entradas em arco dos lados com os toldos listrados, a faixa verde pintada à mão em volta do salão, as vigas de
madeira saindo das paredes, as bandeiras no portal e nos cantos. Na frente, o pátio de treino na terra batida (sem muro): a pista de
pneus, a barra de escalar, a linha de tiro de sacos e os alvos de saco (o pórtico com os sacos pendurados e os bonecos de saco nas
estacas). De cima: a fila de 5 círculos dourados no teto claro, o pátio de terra com os pneus.

Lote 18 × 22 m (X × Y), frente em +X. A saída da infantaria: a porta do portal em (−0,6; 0), pela faixa livre de Y −3,3..3,3 até a
borda +X (exit_lane_m 6,6). Alturas: o salão 4,6 m (o parapeito até ~5,2), o portal 6,4 m (as ameias até ~7,1), as cúpulas até ~7,0 m
e os lanternins até ~8,1 m.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Barracks_Base: tudo (as bandeiras saem no flags.json; o jogo desenha o pano no vento);
  SM_GLA_Barracks_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA): Plaster (o adobe), CnGold (o dourado das cúpulas e dos remates), Player (o verde: as faixas, as bandeiras, os
anéis dos alvos), Wood, Cloth (os sacos, as listras claras dos toldos), Rust (as listras escuras dos toldos), Rubber (os pneus), Dark,
Earth (o pátio de terra batida), Trim, Crate e Hazard (só no andaime).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import barrel, disc_ring, revolve  # noqa: E402,F401
from model_building_parts import (WallFace, wbox, splits, tube, catenary, adobe_block, adobe_door, paint_band,  # noqa: E402,F401
                                  rect_minus)
from model_building_parts import gla_round_merlons as round_merlons, gla_wall_seg as wall_seg  # noqa: E402,F401
# o kit GLA dos prédios aprovados (o Palace v2: a peça, o arco, a cúpula persa, os sacos, o chão batido, o mastro)
from model_gla_palace_v2 import (new_part, finish, _hash, rod, prism, bag, bag_row, tire, flagpole, earth_pad, blob,  # noqa: E402,F401
                                 arch_fill, arch_band, pdome, arch_window, scrawl, crate)

PREFIX = 'SM_GLA_Barracks'
COLORS.setdefault('Yellow', (214, 164, 32))
HX0, HX1, HY = -7.6, -1.2, 9.6       # o salão
HZ0, HZ1 = 0.2, 4.6                  # o pé (no rodapé) e o topo da laje do teto
WT = 0.55                            # a espessura das paredes
BAY = 3.84                           # o vão de cada cúpula
DOMES = [-2 * BAY, -BAY, 0.0, BAY, 2 * BAY]
DCX = (HX0 + HX1) / 2                # o eixo das cúpulas
DRUM_R, DRUM_H = 1.72, 0.5
DOME_R, DOME_H = 1.62, 1.72
PORTAL = (-1.3, -0.15, 2.3, 6.4)     # o portal: x0, x1, meia-largura, topo
IWAN_W, IWAN_SPRING = 2.3, 3.55      # o arco fundo do portal
SIDE_Y = 2 * BAY                     # as entradas dos lados (as das pontas)


# ===================================================================================== as peças
def awning(p, x0, x1, y0, y1, z_hi, z_lo, n=7, sag=0.1, poles=True):
    """O toldo listrado (as listras correm do muro pra beira, alternando o cru e o escuro): as tiras com a barriga, a sanefa recortada
    na beira (as abas em arco), a vara da frente e os 2 paus finos até o chão."""
    for k in range(n):
        ya = y0 + (y1 - y0) * k / n
        yb = y0 + (y1 - y0) * (k + 1) / n
        m = 'Cloth#aw' if k % 2 == 0 else 'Rust#aw'
        rings = []
        N = 8
        for i in range(N + 1):
            t = i / N
            x = x0 + (x1 - x0) * t
            z = z_hi + (z_lo - z_hi) * t - sag * math.sin(math.pi * t)
            rings.append([(x, ya, z - 0.012), (x, yb, z - 0.012), (x, yb, z + 0.012), (x, ya, z + 0.012)])
        p.loft(m, rings, bevel=0.0)
        # a aba da sanefa (o arco embaixo da beira)
        ym = (ya + yb) / 2
        w = (yb - ya)
        pts = [(x1 + 0.005, ya, z_lo), (x1 + 0.005, yb, z_lo), (x1 + 0.005, yb - w * 0.15, z_lo - 0.22), (x1 + 0.005, ym, z_lo - 0.3),
               (x1 + 0.005, ya + w * 0.15, z_lo - 0.22)]
        prism(p, m, pts, (0.02, 0.0, 0.0))
    rod(p, 'Wood#aw', (x1, y0 - 0.1, z_lo + 0.02), (x1, y1 + 0.1, z_lo + 0.02), 0.04)
    rod(p, 'Wood#aw', (x0 + 0.02, y0 - 0.05, z_hi + 0.03), (x0 + 0.02, y1 + 0.05, z_hi + 0.03), 0.035)
    if poles:
        for y in (y0 + 0.08, y1 - 0.08):
            rod(p, 'Wood#aw', (x1 - 0.02, y, 0.0), (x1 - 0.02, y, z_lo + 0.05), 0.045)


def golden_dome(p, cx, cy, z0, seed=0):
    """A cúpula do bazar: o tambor de reboco com as 8 janelinhas e a cornija, a faixa verde no pé, a casca dourada (o perfil persa
    com a leve ponta), os 12 gomos em relevo, o anel do óculo e o lanternim (o tamborzinho com as frestas e o chapéu dourado com o
    remate)."""
    p.cyl('Plaster#s', (cx, cy, z0 + DRUM_H / 2), 'z', DRUM_R, DRUM_H, seg=48, bevel=0.0, smooth=True)
    disc_ring(p, 'Plaster#lo', (cx, cy), DRUM_R + 0.1, DRUM_R - 0.25, z0 + DRUM_H - 0.04, z0 + DRUM_H + 0.1, seg=48)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        q = (cx + (DRUM_R - 0.02) * math.cos(a), cy + (DRUM_R - 0.02) * math.sin(a))
        p.obox('Dark', (q[0], q[1], z0 + 0.26), (0.06, 0.24, 0.3), yaw=math.degrees(a), bevel=0.0)
    zb = z0 + DRUM_H + 0.1
    disc_ring(p, 'Player#dome', (cx, cy), DOME_R + 0.05, DOME_R - 0.2, zb - 0.02, zb + 0.24, seg=48)
    rings = 14
    prof = [pdome(DOME_R, DOME_H, j / rings) for j in range(rings + 1)]
    revolve(p, 'CnGold#dome', (cx, cy), [(max(r, 0.02), zb + z) for (r, z) in prof], seg=48)
    for k in range(12):
        a = 2 * math.pi * k / 12
        rr_ = []
        for j in range(8):
            t = 0.1 + 0.62 * j / 7
            r, z = pdome(DOME_R + 0.03, DOME_H + 0.03, t)
            w = 0.07 * (1.0 - 0.5 * j / 7)
            tx, ty = -math.sin(a), math.cos(a)
            px, py = cx + r * math.cos(a), cy + r * math.sin(a)
            o = 0.04
            rr_.append([(px - tx * w, py - ty * w, zb + z), (px + tx * w, py + ty * w, zb + z),
                        (px + tx * w + math.cos(a) * o, py + ty * w + math.sin(a) * o, zb + z + o * 0.5),
                        (px - tx * w + math.cos(a) * o, py - ty * w + math.sin(a) * o, zb + z + o * 0.5)])
        p.loft('CnGold#rib', rr_, bevel=0.0)
    # o lanternim
    zt = zb + DOME_H
    r_l = 0.4
    zl = zt - 0.12
    disc_ring(p, 'CnGold#rib', (cx, cy), r_l + 0.12, r_l - 0.05, zl - 0.04, zl + 0.08, seg=24)
    p.cyl('Plaster#s', (cx, cy, zl + 0.25), 'z', r_l, 0.42, seg=24, bevel=0.0, smooth=True)
    for k in range(6):
        a = 2 * math.pi * k / 6 + seed * 0.3
        p.obox('Dark', (cx + (r_l - 0.01) * math.cos(a), cy + (r_l - 0.01) * math.sin(a), zl + 0.26), (0.05, 0.12, 0.24),
               yaw=math.degrees(a), bevel=0.0)
    zc = zl + 0.46
    disc_ring(p, 'Plaster#lo', (cx, cy), r_l + 0.08, r_l - 0.1, zc - 0.04, zc + 0.04, seg=24)
    cap = [pdome(0.46, 0.44, j / 6) for j in range(7)]
    revolve(p, 'CnGold#dome', (cx, cy), [(max(r, 0.02), zc + z) for (r, z) in cap], seg=24)
    revolve(p, 'CnGold', (cx, cy), [(0.02, zc + 0.4), (0.09, zc + 0.46), (0.11, zc + 0.55), (0.07, zc + 0.63), (0.03, zc + 0.68),
                                    (0.02, zc + 0.86), (0.005, zc + 0.9)], seg=12)
    return zc + 0.9


def hall(p):
    """O salão comprido: as paredes de adobe (o bloco oco com a laje, o parapeito de topo redondo e as vigas saindo pelas faces ±X), os
    vãos (a porta do portal, as 2 entradas dos lados, as janelas em arco), o rodapé, a faixa verde e as 5 cúpulas."""
    door = (-0.85, 0.85, HZ0, HZ0 + 2.55)
    side = [(s * SIDE_Y - 0.95, s * SIDE_Y + 0.95, HZ0, HZ0 + 2.95) for s in (-1, 1)]
    winf = [(s * BAY - 0.42, s * BAY + 0.42, HZ0 + 1.4, HZ0 + 2.9) for s in (-1, 1)]
    winb = [(y - 0.42, y + 0.42, HZ0 + 1.4, HZ0 + 2.9) for y in (-2 * BAY, -BAY, BAY, 2 * BAY)]
    backdoor = [(-0.7, 0.7, HZ0, HZ0 + 2.3)]
    wins = [(DCX - 0.42, DCX + 0.42, HZ0 + 1.4, HZ0 + 2.9)]
    holes = {'+x': [door] + side + winf, '-x': winb + backdoor, '+y': wins, '-y': wins}
    adobe_block(p, HX0, HX1, -HY, HY, HZ0, HZ1, t=WT, holes=holes, parapet=0.55, par_t=0.42, beams=('+x', '-x'), beam_step=0.96,
                beam_out=0.42, beam_r=0.09, interior=True, cols=[(DCX, y) for y in (-1.92, 1.92)], roof_t=0.34)
    # o rodapé (a base mais larga que acompanha o salão, a quina gasta)
    p.box('Plaster#plinth', HX0 - 0.22, HX1 + 0.22, -HY - 0.22, HY + 0.22, -0.1, HZ0 + 0.32, bevel=0.1)
    # os arcos: os tímpanos na espessura das paredes, o escuro de dentro, as molduras
    for s in (-1, 1):
        y = s * SIDE_Y
        arch_fill(p, 'Plaster#arch', (HX1 - WT / 2, y), 'x', 1.9, HZ0 + 2.0, HZ0 + 2.95, WT)
        arch_band(p, 'Plaster#s', (HX1, y), 'x', 1.9, HZ0 + 2.0, 0.005, width=0.16, t=0.06, seg=16)
        wbox(p, 'Dark', WallFace('+x', HX1, 0.0), y - 0.95, y + 0.95, HZ0, HZ0 + 2.95, -WT - 0.9, -WT - 0.86, bevel=0.0)
        p.box('Dark#floor', HX1 - WT - 0.9, HX1 - WT + 0.02, y - 0.95, y + 0.95, HZ0, HZ0 + 0.02, bevel=0.0)
        arch_window(p, HX1, s * BAY, HZ0 + 1.4, '+x', w=0.84, h=1.5, t=WT)
    for y in (-2 * BAY, -BAY, BAY, 2 * BAY):
        arch_window(p, HX0, y, HZ0 + 1.4, '-x', w=0.84, h=1.5, t=WT)
    for f, at in (('+y', HY), ('-y', -HY)):
        arch_window(p, DCX, at, HZ0 + 1.4, f, w=0.84, h=1.5, t=WT)
    Fb = WallFace('-x', HX0, 0.0)
    adobe_door(p, Fb, -0.7, 0.7, HZ0, HZ0 + 2.3, t=WT)
    # a faixa verde pintada à mão em volta do salão, logo abaixo do parapeito
    zl, zh = HZ1 - 0.75, HZ1 - 0.35
    for (a, b, side_) in (((HX1, -HY), (HX1, -PORTAL[2] - 0.05), -1), ((HX1, PORTAL[2] + 0.05), (HX1, HY), -1),
                          ((HX0, -HY), (HX0, HY), 1), ((HX0, HY), (HX1, HY), -1), ((HX0, -HY), (HX1, -HY), 1)):
        paint_band(p, a, b, zl, zh, side=side_, off=0.0, m='Player', seed=int(abs(a[1] * 7 + b[0] * 3)))
    # os rabiscos verdes nos lados que a câmera vê
    scrawl(p, (HX1 + 0.01, -5.6, 1.5), '+x', size=1.0, seed=11)
    scrawl(p, (-5.3, -HY - 0.01, 1.4), '-y', size=1.1, seed=12)
    # as 5 cúpulas douradas em fila
    tops = [golden_dome(p, DCX, y, HZ1, seed=k) for k, y in enumerate(DOMES)]
    # entre as cúpulas: o lombo de reboco por cima do arco que separa os vãos (a costura do teto do bazar)
    for k in range(4):
        y = (DOMES[k] + DOMES[k + 1]) / 2
        p.cyl('Plaster#s', (DCX, y, HZ1), 'x', 0.32, HX1 - HX0 - 2 * WT, seg=12, bevel=0.0, smooth=True)
    return max(tops)


def turrets(p):
    """As 4 torrinhas redondas dos cantos do salão (as do caravançarai: o tambor de barro afunilado, a cornija, a faixa verde e a coroa de
    ameias redondas) e os pilaretes entre os vãos das cúpulas (sobem acima do parapeito, com o chapéu redondo): o ritmo da fachada
    comprida acompanha as cúpulas."""
    from model_building_parts import merlons
    for (x, y) in ((HX0, -HY), (HX0, HY), (HX1, -HY), (HX1, HY)):
        prof = [(0.82, -0.05), (0.8, 0.3), (0.68, HZ1 + 0.25), (0.78, HZ1 + 0.3), (0.78, HZ1 + 0.46), (0.7, HZ1 + 0.5), (0.7, HZ1 + 0.95),
                (0.6, HZ1 + 1.0), (0.45, HZ1 + 0.98), (0.45, HZ1 + 0.62), (0.02, HZ1 + 0.62)]
        revolve(p, 'Plaster#turret', (x, y), prof, seg=28, smooth=True)
        disc_ring(p, 'Player#band', (x, y), 0.715, 0.6, HZ1 - 0.45, HZ1 - 0.12, seg=28)
        for k in range(5):
            a = 2 * math.pi * (k + 0.25) / 5
            d = 0.2 / 0.58
            A = (x + 0.58 * math.cos(a - d), y + 0.58 * math.sin(a - d))
            B = (x + 0.58 * math.cos(a + d), y + 0.58 * math.sin(a + d))
            merlons(p, A, B, HZ1 + 0.95, n=1, w=0.36, t=0.24, h=0.42, m='Plaster#merlon', bevel=0.08)
    for (xf, s_) in ((HX1, 1), (HX0, -1)):
        ys = (-5.76, 5.76) if s_ > 0 else (-5.76, -1.92, 1.92, 5.76)
        for y in ys:
            x0, x1 = sorted((xf - s_ * 0.05, xf + s_ * 0.32))
            p.box('Plaster#pil', x0, x1, y - 0.36, y + 0.36, -0.05, HZ1 + 0.72, bevel=0.1)
            revolve(p, 'Plaster#s', ((x0 + x1) / 2, y), [(0.3, HZ1 + 0.7), (0.3, HZ1 + 0.8), (0.2, HZ1 + 0.98), (0.02, HZ1 + 1.06)], seg=16)


def portal(p):
    """O portal alto (o pishtaq) no meio da frente: o bloco com o arco fundo (o iwan), a porta de tábuas cravejada no fundo do arco, a
    moldura em degrau, a faixa verde, as ameias redondas e as 2 bandeiras."""
    x0, x1, hw, top = PORTAL
    F = WallFace('+x', x1, 0.0)
    depth = x1 - x0
    for (a, b, c, d, lo, hi) in rect_minus(-hw, hw, HZ0, top, [(-IWAN_W / 2, IWAN_W / 2, HZ0, IWAN_SPRING + IWAN_W / 2)]):
        wbox(p, 'Plaster', F, a, b, c, d, -depth, 0.0)
    arch_fill(p, 'Plaster#arch', (x1 - depth / 2, 0.0), 'x', IWAN_W, IWAN_SPRING, IWAN_SPRING + IWAN_W / 2 + 0.01, depth)
    arch_band(p, 'Plaster#s', (x1, 0.0), 'x', IWAN_W, IWAN_SPRING, 0.005, width=0.22, t=0.08, seg=18)
    arch_band(p, 'Player#arch', (x1, 0.0), 'x', IWAN_W + 0.5, IWAN_SPRING, 0.005, width=0.12, t=0.03, seg=18)
    # a moldura retangular em volta do arco (o alfiz) e o friso verde
    for (a, b, c, d) in ((-hw + 0.25, -hw + 0.45, HZ0 + 0.3, top - 0.7), (hw - 0.45, hw - 0.25, HZ0 + 0.3, top - 0.7),
                         (-hw + 0.25, hw - 0.25, top - 0.9, top - 0.7)):
        wbox(p, 'Plaster#s', F, a, b, c, d, -0.02, 0.07, bevel=0.0)
    paint_band(p, (x1, -hw), (x1, hw), top - 0.62, top - 0.3, side=-1, off=0.0, m='Player', seed=5)
    for (y0, y1) in ((-hw, -hw + 0.02), (hw - 0.02, hw)):
        pass
    p.box('Plaster#lo', x0 - 0.05, x1 + 0.12, -hw - 0.12, hw + 0.12, top - 0.05, top + 0.12, bevel=0.0)
    round_merlons(p, (x1 - 0.2, -hw + 0.25), (x1 - 0.2, hw - 0.25), top + 0.12, n=5, w=0.48, t=0.36, h=0.42)
    for s in (-1, 1):
        round_merlons(p, (x0 + 0.25, s * (hw - 0.2)), (x1 - 0.45, s * (hw - 0.2)), top + 0.12, n=1, w=0.48, t=0.36, h=0.42)
    # o fundo do arco: a parede do salão com a porta de tábuas cravejada (de onde a infantaria sai) e a lanterna
    Fd = WallFace('+x', HX1, 0.0)
    adobe_door(p, Fd, -0.85, 0.85, HZ0, HZ0 + 2.55, t=WT)
    for z in splits(HZ0 + 0.5, HZ0 + 2.1, 0.4):
        for u in (-0.6, -0.25, 0.25, 0.6):
            wbox(p, 'CnGold#stud', Fd, u - 0.03, u + 0.03, z - 0.03, z + 0.03, -WT * 0.55 + 0.04, -WT * 0.55 + 0.08, bevel=0.0)
    p.box('Plaster#floor', HX1 - 0.05, x1, -IWAN_W / 2, IWAN_W / 2, HZ0, HZ0 + 0.06, bevel=0.0)
    q = (x1 - depth * 0.45, 0.0, IWAN_SPRING + IWAN_W / 2 - 0.05)
    rod(p, 'Dark', q, (q[0], q[1], q[2] - 0.55), 0.008, seg=4)
    p.cyl('CnGold#lamp', (q[0], q[1], q[2] - 0.7), 'z', 0.13, 0.28, seg=10, r2=0.08, bevel=0.0)
    p.cyl('Light', (q[0], q[1], q[2] - 0.9), 'z', 0.09, 0.14, seg=10, bevel=0.0, smooth=True)
    for s in (-1, 1):
        flagpole(p, x1 - 0.35, s * (hw - 0.3), top + 0.12, 2.4, length=1.3, height=0.85, phase=0.5 + s)


def ends(p):
    """Os 2 toldos listrados nas entradas dos lados, os barris e o pote d'água, as bandeiras dos cantos de trás."""
    for s in (-1, 1):
        y = s * SIDE_Y
        awning(p, HX1 + 0.05, HX1 + 2.2, y - 1.4, y + 1.4, HZ0 + 3.45, HZ0 + 2.55, n=7, sag=0.12)
    for (x, y) in ((HX1 + 0.55, -5.5), (HX1 + 0.5, 5.6)):
        barrel(p, x, y, 0.0, r=0.29, h=0.86, m='Rust')
    barrel(p, HX1 + 0.6, 5.0, 0.0, r=0.29, h=0.86, m='Metal')
    # o pote d'água de barro com a tampa de pano (junto da entrada da direita)
    revolve(p, 'Plaster#jar', (HX1 + 0.6, -9.0), [(0.12, 0.0), (0.3, 0.12), (0.42, 0.45), (0.38, 0.8), (0.22, 0.98), (0.2, 1.08),
                                                  (0.24, 1.12)], seg=18)
    p.cyl('Cloth', (HX1 + 0.6, -9.0, 1.13), 'z', 0.25, 0.04, seg=16, bevel=0.0)
    for (x, y, ph) in ((HX0 + 0.35, -HY + 0.35, 0.3), (HX0 + 0.35, HY - 0.35, 1.9)):
        flagpole(p, x, y, HZ1 + 0.55, 2.8, length=1.4, height=0.9, phase=ph)


# ===================================================================================== o pátio de treino
def yard(p):
    """O pátio de treino na terra batida, sem muro: a pista de pneus e a barra de escalar do lado −Y; a linha de tiro de sacos, o
    pórtico dos sacos pendurados e os bonecos de saco do lado +Y. A faixa Y −3,3..3,3 fica livre (a saída)."""
    earth_pad(p, (4.2, 0.0), blob(4.2, 0.0, 4.75, 10.2, seed=4, amp=0.07), h=0.05)
    # a pista de pneus: 2 fileiras desencontradas de 5
    for k in range(5):
        x = 1.9 + k * 1.1
        for (row, off) in ((-4.6, 0.12), (-5.6, -0.12)):
            tire(p, (x, row + (off if k % 2 else -off), 0.13), R=0.36, r=0.13)
    for (x, y, n) in ((7.8, -4.9, 3), (7.75, -5.8, 2)):
        for k in range(n):
            tire(p, (x + 0.03 * (k % 2), y, 0.13 + k * 0.25), R=0.36, r=0.13)
    # a barra de escalar (a escada horizontal nos 4 postes, com as escadinhas nas pontas)
    bx0, bx1, by0, by1, bz = 2.0, 6.4, -8.75, -7.85, 2.3
    for (x, y) in ((bx0, by0), (bx0, by1), (bx1, by0), (bx1, by1)):
        rod(p, 'Wood#post', (x, y, -0.1), (x, y, bz + 0.1), 0.08, seg=8)
    for y in (by0, by1):
        rod(p, 'Wood#rail', (bx0 - 0.1, y, bz), (bx1 + 0.1, y, bz), 0.055, seg=8)
    for x in splits(bx0 + 0.25, bx1 - 0.25, 0.42):
        rod(p, 'Metal#rung', (x, by0, bz + 0.02), (x, by1, bz + 0.02), 0.025, seg=6)
    for x in (bx0, bx1):
        for z in splits(0.4, bz - 0.3, 0.42):
            rod(p, 'Wood#rung', (x, by0, z), (x, by1, z), 0.025, seg=6)
    # a linha de tiro: a mureta baixa de sacos (2 camadas) e os caixotes de munição
    bag_row(p, [(1.9, 4.1), (7.4, 4.1)], 0.0, layers=2, bag_size=(0.62, 0.34, 0.22), seed=21)
    crate(p, 1.4, 3.9, 0.0, size=(0.8, 0.5, 0.4), yaw=8)
    crate(p, 1.45, 3.95, 0.4, size=(0.7, 0.45, 0.35), yaw=-4)
    # o pórtico dos sacos pendurados (os sacos de pancada nas cordas)
    gx0, gx1, gy, gz = 2.6, 6.8, 6.3, 2.7
    for x in (gx0, gx1):
        rod(p, 'Wood#post', (x, gy, -0.1), (x, gy, gz + 0.1), 0.085, seg=8)
        for s in (-1, 1):
            rod(p, 'Wood#post', (x, gy + s * 0.75, 0.0), (x, gy, 1.4), 0.05, seg=6)
    rod(p, 'Wood#rail', (gx0 - 0.15, gy, gz), (gx1 + 0.15, gy, gz), 0.075, seg=8)
    for k, x in enumerate((3.4, 4.7, 6.0)):
        ztop = gz - 0.4 - 0.08 * k
        rod(p, 'Dark#rope', (x, gy, gz), (x, gy, ztop), 0.018, seg=4)
        revolve(p, 'Cloth#sack', (x, gy), [(0.06, ztop), (0.22, ztop - 0.08), (0.27, ztop - 0.3), (0.28, ztop - 0.75), (0.24, ztop - 1.0),
                                          (0.12, ztop - 1.08), (0.03, ztop - 1.1)], seg=14)
        p.torus('Dark#rope', (x, gy, ztop - 0.12), 'z', 0.2, 0.02, seg=12, rseg=4)
    # os bonecos de saco nas estacas (o saco cheio amarrado na cruz, o alvo verde pintado no peito, virados pra −X: a linha de tiro)
    for k, x in enumerate((2.5, 3.9, 5.3, 6.7)):
        y = 8.6
        rod(p, 'Wood#post', (x, y, -0.1), (x, y, 1.9), 0.06, seg=6)
        rod(p, 'Wood#post', (x, y - 0.45, 1.45), (x, y + 0.45, 1.45), 0.045, seg=6)
        revolve(p, 'Cloth#sack', (x, y), [(0.05, 0.75), (0.2, 0.8), (0.26, 1.0), (0.27, 1.35), (0.22, 1.55), (0.1, 1.6), (0.04, 1.62)], seg=14)
        revolve(p, 'Cloth#sack', (x, y), [(0.03, 1.62), (0.13, 1.66), (0.15, 1.76), (0.12, 1.86), (0.03, 1.9)], seg=10)
        for zz in (0.95, 1.42):
            p.torus('Dark#rope', (x, y, zz), 'z', 0.265, 0.018, seg=12, rseg=4)
        p.cyl('Cloth#target', (x, y - 0.25, 1.2), 'y', 0.17, 0.02, seg=16, bevel=0.0)
        p.cyl('Player#target', (x, y - 0.262, 1.2), 'y', 0.11, 0.02, seg=16, bevel=0.0)
        p.cyl('Dark#target', (x, y - 0.274, 1.2), 'y', 0.045, 0.02, seg=10, bevel=0.0)


def base():
    p = new_part(PREFIX + '_Base')
    for m in ('CnGold#dome', 'CnGold#rib', 'Player#dome', 'Cloth#aw', 'Rust#aw', 'Cloth#sack', 'Player#arch', 'Player#band'):
        p.bm(m, 0.0)
    p.bm('Plaster#plinth', 0.13)
    p.bm('Plaster#turret', 0.1)
    top = hall(p)
    turrets(p)
    portal(p)
    ends(p)
    yard(p)
    return finish(p), top


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1500]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 900]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1300, 900]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1600, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1100], 30.0, 0.95]],
            'usa_at': [7.0, -14.5, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_barracks_v4')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b, top = base()
    total = export(b, out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', HX0 - 0.2, PORTAL[1] + 0.2, -HY - 0.2, HY + 0.2, HZ1 + 1.2, post='Wood', plank='Crate',
                             stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 18 x 22 m; o salao', HZ1, 'm; o portal', PORTAL[3], 'm; os lanternins ate', round(top, 2), 'm')
    print('MODEL saida: a porta do portal em (', HX1 + 0.6, ', 0 ); a faixa livre Y -3,3..3,3 ate a borda +X')
    print('MODEL fire_points_m', [(DCX, -1.92, HZ1 + 0.6), (DCX, 1.92 + BAY, HZ1 + 0.6), (DCX, -1.92 - BAY, HZ1 + 0.6)])


if __name__ == '__main__':
    main()
