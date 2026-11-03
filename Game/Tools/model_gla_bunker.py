"""O Bunker da GLA (prédio novo: a casamata de 3 vagas, onde a infantaria entra pra atirar de dentro), 03/11/2026: O NINHO NO TELHADO.
Fiel ao conceito aprovado docs/concept/buildings/gla_bunker_ninho_APROVADO.jpg.

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_gla_bunker.py captures/game/models/gla_bunker

A palavra (o que lê em 1 segundo): A CASINHA CÚBICA DE BARRO COM O NINHO DE SACOS NO TETO. A casinha de adobe (a quina gasta, as vigas
saindo da parede, as janelinhas de grade, a faixa verde a pincel em volta do alto), a escada maciça de barro subindo por fora na face da
frente até o teto, a porta em arco com a moldura verde na frente (ao lado do topo da escada) e outra no lado −Y; no teto, o parapeito com o
anel de sacos de areia em cima e as 3 VAGAS de tiro (o estrado de tábua e o vão mais baixo nos sacos virado pra fora), a rede de
camuflagem só no canto da frente (+X, +Y), o pilar do canto de trás com a bandeira; no pé do lado −Y, o toldo de chapa enferrujada sobre
os caixotes nos estrados. Sem soldados na malha: o jogo põe os que entrarem nas vagas (VAGAS, no relatório).

Lote 8 × 8 m, frente em +X. Alturas: o teto 3,8 m (o piso das vagas), o parapeito com os sacos até ~4,8 m, o pilar até 4,9 m (a bandeira
~6,5 m), a rede ~5,1–6,1 m nas 4 varas.

Peças (origem no chão, no centro do lote; Unreal X frente, Y direita, Z cima, metros):
  SM_GLA_Bunker_Base: tudo (a bandeira sai no flags.json);
  SM_GLA_Bunker_Scaffold: o andaime da obra em madeira.
Materiais (slots da GLA e genéricos): Plaster (o barro), Player (o verde), Wood, Cloth (os sacos, a lona), Net (a rede), Rust (o toldo),
  Crate (os caixotes), Earth, Dark, Trim, Metal, Hazard (só no andaime).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import corrugated, barrel  # noqa: E402
from model_building_parts import (WallFace, wbox, adobe_window, paint_band_block, tube, rect_minus, splits,  # noqa: E402,F401
                                  gla_wall_seg as wall_seg, gla_net_surface as net_surface)
from model_gla_palace_v2 import (new_part, finish, _hash, fbm, rod, bag, earth_pad, blob, crate, flagpole, rope,  # noqa: E402,F401
                                 arch_fill, arch_band)

PREFIX = 'SM_GLA_Bunker'
X0, X1, Y0, Y1 = -3.0, 2.0, -2.4, 2.6     # a casinha
ZT = 3.8                                  # o teto (o piso do ninho)
WT = 0.5                                  # a espessura das paredes
PAR_H, PAR_T = 0.36, 0.4                  # o parapeito
STAIR_Y0, STAIR_N, STAIR_RUN = -2.5, 11, 0.28
STAIR_W = 0.92
LAND = (STAIR_Y0 + STAIR_N * STAIR_RUN, STAIR_Y0 + STAIR_N * STAIR_RUN + 0.45)   # o patamar do topo (y)
GAP = (LAND[0] - 0.12, LAND[1] + 0.08)    # o vão do parapeito onde a escada chega
DOOR_A = ('+x', X1, 1.66, 0.85, 1.72)     # a porta da frente: a face, o plano, o centro (u), a largura, a imposta do arco
DOOR_B = ('-y', Y0, 1.0, 0.85, 1.72)      # a porta do lado −Y
WINDOWS = [('+x', -1.6, 1.75, 0.5, 0.55), ('-y', -0.3, 2.45, 0.5, 0.5), ('-y', -2.0, 2.45, 0.5, 0.5), ('-x', 0.25, 2.35, 0.5, 0.5),
           ('+y', -0.95, 2.35, 0.5, 0.5), ('+y', 0.9, 2.35, 0.5, 0.5)]
# as 3 vagas de tiro no teto (x, y): o estrado e o vão nos sacos virado pra fora (a face)
VAGAS = [((0.72, 1.78), '+y'), ((-2.05, -0.15), '-x'), ((-0.95, -1.58), '-y')]
FLAG_PILLAR = (X0 + 0.45, Y0 + 0.45)


def at_of(face):
    return {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}[face]


def wall_center(face, u):
    """O ponto (x, y) no meio da espessura da parede da face, na posição u ao longo dela."""
    s = 1 if face[0] == '+' else -1
    a = at_of(face) - s * WT / 2
    return (a, u) if face[1] == 'x' else (u, a)


# ===================================================================================== a casinha
def house(p):
    """O bloco de adobe oco (as paredes de 0,5 m em volta dos vãos de verdade, a laje do teto, as vigas que atravessam e saem pelos lados
    ±Y), as portas em arco com a moldura verde, as janelinhas de grade, a faixa verde a pincel no alto, o parapeito redondo."""
    holes = {}
    for (f, at, u, w, sp) in (DOOR_A, DOOR_B):
        holes.setdefault(f, []).append((u - w / 2, u + w / 2, 0.0, sp + w / 2))
    for (f, u, z, w, h) in WINDOWS:
        holes.setdefault(f, []).append((u - w / 2, u + w / 2, z, z + h))
    # (as quinas numa peça só: no adobe_block do kit as 2 paredes se sobrepõem na quina, as superfícies coincidem e a quina sai preta no
    # Cycles e pisca na Unreal) as 4 quinas são blocos com a quina gasta de 13 cm; as paredes entre elas, com o chanfro miúdo (sem sulco
    # nas emendas das tiras em volta dos vãos), montadas em volta dos vãos de verdade
    for (cx0, cx1) in ((X0, X0 + WT), (X1 - WT, X1)):
        for (cy0, cy1) in ((Y0, Y0 + WT), (Y1 - WT, Y1)):
            p.box('Plaster', cx0, cx1, cy0, cy1, -0.02, ZT)
    for f in ('+x', '-x', '+y', '-y'):
        F = WallFace(f, at_of(f), 0.0)
        u0, u1 = ((Y0 + WT - 0.001, Y1 - WT + 0.001) if f[1] == 'x' else (X0 + WT - 0.001, X1 - WT + 0.001))
        for (a, b, c, d, lo, hi) in rect_minus(u0, u1, -0.02, ZT, holes.get(f, ())):
            wbox(p, 'Plaster#lo', F, a, b, c, d, -WT, 0.0)
    p.box('Plaster#slab', X0 + WT - 0.05, X1 - WT + 0.05, Y0 + WT - 0.05, Y1 - WT + 0.05, ZT - 0.32, ZT - 0.02, bevel=0.03)
    p.box('Plaster#slab', X0 + WT - 0.05, X1 - WT + 0.05, Y0 + WT - 0.05, Y1 - WT + 0.05, -0.05, 0.06, bevel=0.0)
    for k, x in enumerate(splits(X0 + 0.6, X1 - 0.6, 1.2)):
        rr = 0.08 * (0.88 + 0.24 * _hash(k, 3, 5))
        p.cyl('Wood#beam', (x, (Y0 + Y1) / 2, ZT - 0.42), 'y', rr, Y1 - Y0 + 1.1, seg=8, bevel=0.0)
    p.cyl('Wood#beam', (-0.5, 0.1, (ZT - 0.42) / 2), 'z', 0.11, ZT - 0.42, seg=8, bevel=0.0)
    for (f, at, u, w, sp) in (DOOR_A, DOOR_B):
        arch_door(p, f, u, w, sp)
    for (f, u, z, w, h) in WINDOWS:
        adobe_window(p, WallFace(f, at_of(f), 0.0), u - w / 2, u + w / 2, z, z + h, t=WT, grille=True)
    # a faixa verde a pincel em volta do alto das paredes (a do conceito, logo abaixo das vigas)
    paint_band_block(p, X0, X1, Y0, Y1, ZT - 0.82, ZT - 0.5, faces='+x-x+y-y', seed=21)
    # o parapeito de topo redondo (abre onde a escada chega, na face +X)
    zp = ZT - 0.03
    h2 = PAR_T / 2
    wall_seg(p, (X0, Y0 + h2), (X1, Y0 + h2), PAR_H, t=PAR_T, z0=zp, band=False)
    wall_seg(p, (X0, Y1 - h2), (X1, Y1 - h2), PAR_H, t=PAR_T, z0=zp, band=False)
    wall_seg(p, (X0 + h2, Y0 + PAR_T), (X0 + h2, Y1 - PAR_T), PAR_H, t=PAR_T, z0=zp, band=False)
    wall_seg(p, (X1 - h2, Y0 + PAR_T), (X1 - h2, GAP[0]), PAR_H, t=PAR_T, z0=zp, band=False)
    wall_seg(p, (X1 - h2, GAP[1]), (X1 - h2, Y1 - PAR_T), PAR_H, t=PAR_T, z0=zp, band=False)
    # as canaletas de madeira do teto (as bicas), saindo no alto
    for (x, y, f) in ((-1.6, Y1, '+y'), (X0, 1.3, '-x'), (-1.9, Y0, '-y')):
        sgn = 1 if f[0] == '+' else -1
        if f[1] == 'y':
            p.box('Wood#spout', x - 0.1, x + 0.1, min(y, y + sgn * 0.6), max(y, y + sgn * 0.6), ZT - 0.08, ZT + 0.06, bevel=0.0)
            p.box('Dark', x - 0.05, x + 0.05, min(y, y + sgn * 0.6), max(y, y + sgn * 0.6), ZT + 0.06, ZT + 0.065, bevel=0.0)
        else:
            p.box('Wood#spout', min(x, x + sgn * 0.6), max(x, x + sgn * 0.6), y - 0.1, y + 0.1, ZT - 0.08, ZT + 0.06, bevel=0.0)
            p.box('Dark', min(x, x + sgn * 0.6), max(x, x + sgn * 0.6), y - 0.05, y + 0.05, ZT + 0.06, ZT + 0.065, bevel=0.0)
    # o reboco remendado (as manchas de barro mais novo, em relevo baixo) na face −Y e na +X
    for (f, u, z, w, h) in (('-y', -1.2, 1.2, 0.9, 0.6), ('+x', 0.3, 0.9, 0.7, 0.5), ('-y', 0.0, 0.5, 0.6, 0.4)):
        wbox(p, 'Plaster#patch', WallFace(f, at_of(f), 0.0), u - w / 2, u + w / 2, z, z + h, -0.02, 0.025, bevel=0.02)


def arch_door(p, f, u, w, sp):
    """A porta em arco no vão fundo: o tímpano de barro em volta do arco, a moldura verde a pincel (o arco e os 2 lados), a porta de
    tábuas recortada no arco (recuada no meio da parede), as ferragens, a soleira de pedra."""
    F = WallFace(f, at_of(f), 0.0)
    r = w / 2
    s = 1 if f[0] == '+' else -1
    c = wall_center(f, u)
    arch_fill(p, 'Plaster#arch', c, f[1], w, sp, sp + r + 0.001, WT)
    arch_band(p, 'Player', c, f[1], w, sp, s * (WT / 2 + 0.006), width=0.17, t=0.018)
    for (a, b) in ((u - r - 0.21, u - r - 0.04), (u + r + 0.04, u + r + 0.21)):
        wbox(p, 'Player', F, a, b, 0.05, sp, 0.006, 0.024, bevel=0.0)
    n = 4
    for k in range(n):
        ua = u - r + 0.03 + (w - 0.06) * k / n
        ub = u - r + 0.03 + (w - 0.06) * (k + 1) / n - 0.015
        um = (ua + ub) / 2 - u
        top = sp + math.sqrt(max(0.0, (r - 0.03) ** 2 - um * um)) - 0.02
        wbox(p, 'Wood#door', F, ua, ub, 0.02, top, -0.3, -0.25, bevel=0.012)
    for zz in (0.4, sp - 0.25):
        wbox(p, 'Wood#door', F, u - r + 0.06, u + r - 0.06, zz - 0.07, zz + 0.07, -0.25, -0.21, bevel=0.0)
        wbox(p, 'Dark#iron', F, u - r + 0.08, u - r + 0.3, zz - 0.04, zz + 0.04, -0.21, -0.19, bevel=0.0)
    wbox(p, 'Dark#iron', F, u + r - 0.18, u + r - 0.12, sp * 0.55 - 0.05, sp * 0.55 + 0.05, -0.21, -0.17, bevel=0.0)
    wbox(p, 'Plaster#sill', F, u - r - 0.12, u + r + 0.12, -0.03, 0.09, -WT + 0.05, 0.28, bevel=0.03)


def stair(p):
    """A escada maciça de barro por fora, na face da frente: sobe de −Y pra +Y até o patamar no teto (cada degrau um bloco do chão até
    o piso dele: a cunha maciça), com a quina dos degraus gasta e a faixa verde correndo pela beira, como no conceito."""
    x0, x1 = X1 - 0.05, X1 + STAIR_W
    rise = ZT / STAIR_N
    # (um sólido só: o perfil em dente de serra extrudado; os degraus em blocos riscavam a lateral de sulcos)
    prof = [(STAIR_Y0, -0.02)]
    for k in range(STAIR_N):
        ya = STAIR_Y0 + k * STAIR_RUN
        last = k == STAIR_N - 1
        zt = ZT if last else (k + 1) * rise + (_hash(k, 1, 13) - 0.5) * 0.03
        prof += [(ya, zt), (LAND[1] if last else ya + STAIR_RUN, zt)]
    prof += [(LAND[1], -0.02)]
    from model_gla_palace_v2 import prism
    prism(p, 'Plaster#step', [(x0, y, z) for (y, z) in prof], (x1 - x0, 0.0, 0.0))
    # a mureta baixa do patamar (o parapeito continua em volta da chegada)
    wall_seg(p, (x1 - 0.14, LAND[0] - 0.4), (x1 - 0.14, LAND[1]), 0.3, t=0.28, z0=ZT - 0.03, band=False)
    wall_seg(p, (X1 - 0.02, LAND[1] - 0.1), (x1 - 0.02, LAND[1] - 0.1), 0.3, t=0.22, z0=ZT - 0.03, band=False)
    # a faixa verde a pincel descendo pela face de fora da escada (a diagonal do conceito)
    b = p.bm('Player#stair', 0.0)
    n = STAIR_N
    pts = []
    for k in range(n + 1):
        y = STAIR_Y0 + k * STAIR_RUN
        z = k * rise - 0.32 + 0.03 * math.sin(k * 1.7)
        pts.append((y, z))
    xo = x1 + 0.012
    rows = []
    for (y, z) in pts:
        rows.append([b.verts.new(V(xo, y, max(0.05, z - 0.14))), b.verts.new(V(xo, y, max(0.1, z + 0.02))),
                     b.verts.new(V(xo + 0.018, y, max(0.1, z + 0.02))), b.verts.new(V(xo + 0.018, y, max(0.05, z - 0.14)))])
    for r0, r1 in zip(rows[:-1], rows[1:]):
        for i in range(4):
            j = (i + 1) % 4
            b.faces.new([r0[i], r0[j], r1[j], r1[i]])
    b.faces.new(rows[0][::-1])
    b.faces.new(rows[-1])


def roof(p):
    """O ninho no teto: os sacos de areia em cima do parapeito (2 fileiras; na frente de cada vaga a fileira de cima falta: o vão de tiro),
    os 3 estrados das vagas, o pilar do canto de trás com a bandeira e a faixa verde, a rede de camuflagem no canto da frente, o
    caixote de munição e o alçapão de madeira do teto."""
    L_, W_, H_ = 0.62, 0.36, 0.24
    zb = ZT - 0.03 + PAR_H + PAR_T / 2 * 0.97 - 0.07
    h2 = PAR_T / 2
    runs = [((X0 + PAR_T + 0.9, Y0 + h2), (X1 - 0.1, Y0 + h2), '-y'), ((X0 + 0.1, Y1 - h2), (X1 - 0.1, Y1 - h2), '+y'),
            ((X0 + h2, Y0 + 1.1), (X0 + h2, Y1 - 0.1), '-x'), ((X1 - h2, Y0 + 0.1), (X1 - h2, GAP[0] - 0.1), '+x'),
            ((X1 - h2, GAP[1] + 0.1), (X1 - h2, Y1 - 0.1), '+x')]
    for ri, ((ax, ay), (bx, by), f) in enumerate(runs):
        L = math.hypot(bx - ax, by - ay)
        yaw = math.degrees(math.atan2(by - ay, bx - ax))
        n = max(1, int(round(L / (L_ * 0.92))))
        notch = [v for (v, vf) in VAGAS if vf == f]
        for lv in range(2):
            for k in range(n + (1 if lv % 2 else 0)):
                t = (k + (0.5 if lv % 2 == 0 else 0.0)) / n
                if t < 0.0 or t > 1.0:
                    continue
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                if lv == 1 and any((abs(x - vx) if f[1] == 'y' else abs(y - vy)) < 0.5 for (vx, vy) in notch):
                    continue
                bag(p, (x, y, zb + H_ * 0.42 + lv * H_ * 0.82), (L_, W_, H_), yaw=yaw + (_hash(k, lv, ri + 70) - 0.5) * 12)
    # os estrados das vagas (as tábuas no piso do teto, encostadas no parapeito)
    for (vx, vy), f in VAGAS:
        yaw = 0.0 if f[1] == 'x' else 90.0
        for j in range(4):
            o = (j - 1.5) * 0.17
            cx, cy = (vx, vy + o) if f[1] == 'x' else (vx + o, vy)
            p.obox('Wood#plank', (cx, cy, ZT + 0.03), (0.62 if f[1] == 'x' else 0.15, 0.15 if f[1] == 'x' else 0.62, 0.05),
                   yaw=(_hash(j, int(vx * 10), 3) - 0.5) * 4, bevel=0.012)
        p.obox('Dark#plank', (vx, vy, ZT - 0.0), (0.7 if f[1] == 'x' else 0.68, 0.68 if f[1] == 'x' else 0.7, 0.02), bevel=0.0)
    # o pilar do canto de trás (−X, −Y) com a capa, a faixa verde e a bandeira
    fx, fy = FLAG_PILLAR
    p.box('Plaster', fx - 0.5, fx + 0.48, fy - 0.5, fy + 0.48, ZT - 0.1, ZT + 0.95, bevel=0.13)
    p.box('Plaster#cap', fx - 0.55, fx + 0.53, fy - 0.55, fy + 0.53, ZT + 0.95, ZT + 1.12, bevel=0.05)
    from model_building_parts import paint_band
    paint_band(p, (fx + 0.48, fy - 0.5), (fx + 0.48, fy + 0.48), ZT + 0.45, ZT + 0.68, side=-1, off=0.0, seed=31)
    paint_band(p, (fx - 0.5, fy - 0.5), (fx + 0.48, fy - 0.5), ZT + 0.45, ZT + 0.68, side=-1, off=0.0, seed=32)
    flagpole(p, fx, fy, ZT + 1.12, 1.55, length=1.0, height=0.62, phase=0.7)
    # a rede de camuflagem no canto da frente (+X, +Y): 2 varas na quina, amarrada baixo no parapeito do outro lado
    # (a rede quase plana nas 4 varas, com a barriga: caindo pra dentro a câmera do jogo via o avesso de raspão, uma tira)
    c0, c1, c2, c3 = (-0.6, -0.1, ZT + 1.7), (X1 - 0.2, -0.15, ZT + 2.1), (X1 - 0.2, Y1 - 0.2, ZT + 2.3), (-0.65, Y1 - 0.2, ZT + 1.8)
    for (x, y, z) in (c1, c2):
        rod(p, 'Wood#pole', (x, y, ZT - 0.3), (x + 0.12, y + (0.1 if y > 1 else -0.06), z + 0.05), 0.055, seg=6)
    for (x, y, z) in (c0, c3):
        rod(p, 'Wood#pole', (x, y, ZT), (x - 0.05, y, z + 0.03), 0.05, seg=6)
        p.box('Plaster#stone', x - 0.14, x + 0.14, y - 0.14, y + 0.14, ZT - 0.02, ZT + 0.12, bevel=0.03)
    rope(p, (c2[0], c2[1], c2[2]), (X1 + 0.9, Y1 + 0.6, 0.05), sag=0.1, r=0.016, m='Dark#rope')

    def fn(u, v):
        x = (1 - u) * (1 - v) * c0[0] + u * (1 - v) * c1[0] + u * v * c2[0] + (1 - u) * v * c3[0]
        y = (1 - u) * (1 - v) * c0[1] + u * (1 - v) * c1[1] + u * v * c2[1] + (1 - u) * v * c3[1]
        z = (1 - u) * (1 - v) * c0[2] + u * (1 - v) * c1[2] + u * v * c2[2] + (1 - u) * v * c3[2]
        k = 1.1                                        # a rede passa um pouco das varas (a borda solta cai)
        x, y = 0.75 + (x - 0.75) * k, 1.25 + (y - 1.25) * k
        edge = min(u, 1 - u, v, 1 - v)
        return (x, y, z - 0.6 * math.sin(math.pi * u) * math.sin(math.pi * v) - 0.35 * max(0.0, 0.1 - edge) / 0.1)
    net_surface(p, fn, 15, 17, seed=9, blotch=1.1, holes=0.3, light=0.63, ragged=0.32, thick=0.04)
    # o caixote de munição debaixo da rede e o alçapão de madeira do teto
    crate(p, 1.0, 1.25, ZT, size=(0.75, 0.45, 0.36), yaw=12)
    for j in range(5):
        p.box('Wood#hatch', -0.95 + j * 0.17, -0.95 + j * 0.17 + 0.155, -0.05, 0.75, ZT - 0.01, ZT + 0.04, bevel=0.01)
    p.box('Dark#iron', -0.9, -0.2, 0.05, 0.11, ZT + 0.04, ZT + 0.06, bevel=0.0)


def yard(p):
    """O pé: o chão batido acompanhando a casa, o toldo de chapa enferrujada no lado −Y sobre os caixotes nos estrados (um coberto de
    lona), o galão no pé da escada, a pedra grande, o entulho miúdo no pé das paredes."""
    earth_pad(p, (-0.3, 0.1), blob(-0.3, 0.1, 3.5, 3.7, seed=12, amp=0.04), h=0.05)
    # o toldo: a chapa ondulada caindo da parede (z 2,3) pra fora (z 1,95), a viga, os 2 paus e as 2 mãos-francesas
    xa, xb, yo = -2.85, -0.6, -3.55
    corrugated(p, xa, xb, yo, Y0 + 0.02, 1.95, 2.3, axis='y', m='Rust#sheet', patch=True, seed=3)
    rod(p, 'Wood#aw', (xa - 0.1, yo + 0.12, 1.9), (xb + 0.1, yo + 0.12, 1.9), 0.055, seg=6)
    for x in (xa + 0.1, xb - 0.1):
        rod(p, 'Wood#aw', (x, yo + 0.12, -0.05), (x, yo + 0.12, 1.95), 0.06, seg=6)
        rod(p, 'Wood#aw', (x, Y0 - 0.02, 2.25), (x, yo + 0.12, 1.92), 0.045, seg=6)
    # os estrados e os caixotes
    for (x, y) in ((-2.25, -3.0), (-1.15, -3.0)):
        for j in range(5):
            p.box('Wood#pallet', x - 0.55, x + 0.55, y - 0.42 + j * 0.2, y - 0.42 + j * 0.2 + 0.13, 0.1, 0.14, bevel=0.0)
        for j in range(3):
            p.box('Wood#pallet', x - 0.55 + j * 0.5, x - 0.45 + j * 0.5, y - 0.45, y + 0.45, 0.0, 0.1, bevel=0.0)
    crate(p, -2.25, -2.78, 0.14, size=(1.05, 0.42, 0.38), yaw=0)
    crate(p, -2.25, -3.22, 0.14, size=(1.05, 0.42, 0.38), yaw=0)
    crate(p, -2.2, -3.0, 0.52, size=(1.0, 0.42, 0.36), yaw=4)
    crate(p, -1.15, -2.95, 0.14, size=(0.95, 0.8, 0.5), yaw=-3)
    # a lona amarrada por cima do 2º (o pano caído pelos lados: a caixa maior de lona com as cordas)
    p.obox('Cloth#cover', (-1.15, -2.95, 0.42), (1.02, 0.88, 0.58), yaw=-3, bevel=0.06)
    for o in (-0.25, 0.25):
        p.obox('Dark#rope', (-1.15 + o, -2.95, 0.42), (0.03, 0.9, 0.6), yaw=-3, bevel=0.0)
    # os caixotes compridos de míssil no chão, virados pra fora (entre o toldo e a porta B)
    crate(p, 0.12, -3.2, 0.0, size=(1.3, 0.42, 0.36), yaw=88)
    crate(p, 0.14, -3.15, 0.36, size=(1.25, 0.4, 0.34), yaw=93)
    p.obox('Player#stencil', (0.14, -3.15, 0.705), (0.45, 0.2, 0.01), yaw=93, bevel=0.0)
    # o galão no pé da escada e o tambor
    p.obox('Crate#can', (2.85, -2.75, 0.25), (0.34, 0.17, 0.48), yaw=20, bevel=0.03)
    p.obox('Dark#can', (2.85, -2.75, 0.52), (0.12, 0.05, 0.06), yaw=20, bevel=0.0)
    barrel(p, -3.45, 1.7, 0.0, r=0.28, h=0.86, m='Rust')
    # a pedra grande (a do conceito, na quina) e o entulho miúdo no pé das paredes
    p.obox('Plaster#rock', (2.55, 3.15, 0.22), (0.75, 0.6, 0.5), yaw=25, pitch=8, bevel=0.12)
    p.obox('Plaster#rock', (2.95, 2.75, 0.1), (0.35, 0.3, 0.22), yaw=-10, roll=12, bevel=0.06)
    for k in range(34):
        side = k % 4
        t = _hash(k, 2, 9)
        if side == 0:
            x, y = X1 + 0.15 + 0.4 * _hash(k, 3, 9), Y0 + 0.2 + (Y1 - Y0 - 0.4) * t
            if y < LAND[1] + 0.1:
                x += STAIR_W
        elif side == 1:
            x, y = X0 + (X1 - X0) * t, Y0 - 0.15 - 0.4 * _hash(k, 3, 9)
            if -2.9 < x < 1.6:
                continue
        elif side == 2:
            x, y = X0 - 0.15 - 0.4 * _hash(k, 3, 9), Y0 + (Y1 - Y0) * t
        else:
            x, y = X0 + (X1 - X0) * t, Y1 + 0.15 + 0.4 * _hash(k, 3, 9)
        if max(abs(x), abs(y)) > 3.9:
            continue
        s = 0.07 + 0.12 * _hash(k, 4, 9) ** 2
        p.obox('Plaster#stone', (x, y, s * 0.3), (s * 1.3, s, s * 0.8), yaw=_hash(k, 5, 9) * 180, pitch=(_hash(k, 6, 9) - 0.5) * 30, bevel=0.03)


def base():
    p = new_part(PREFIX + '_Base')
    for m, b in (('Player#stair', 0.0), ('Plaster#step', 0.05), ('Plaster#patch', 0.02), ('Plaster#cap', 0.05), ('Plaster#rock', 0.12),
                 ('Plaster#lo', 0.012)):
        p.bm(m, b)
    yard(p)
    house(p)
    stair(p)
    roof(p)
    return finish(p)


def write_preview(out):
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]], ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1200, 1000]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1200, 1000]],
                      ['three_quarter', 'persp', 32.0, 215.0, [1400, 1100], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1400, 1100], 30.0, 0.95]],
            'usa_at': [5.2, -6.0, 0.0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'gla_bunker')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    total = export(base(), out)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', X0, X1 + STAIR_W, Y0, Y1, ZT + 0.6, post='Wood', plank='Crate', stripe='Hazard'), out)
    write_preview(out)
    print('MODEL total', total, 'tris')
    print('MODEL lote 8 x 8 m; o teto (piso das vagas) z', ZT, '; o parapeito com os sacos ate ~', round(ZT + PAR_H + 0.2 + 0.45, 2))
    print('MODEL vagas_m', [(x, y, round(ZT + 0.06, 2), f) for (x, y), f in VAGAS])
    print('MODEL portas_m', [('A +x', (X1, DOOR_A[2], 0.0)), ('B -y', (DOOR_B[2], Y0, 0.0))])
    print('MODEL fire_points_m', [(-0.5, 0.1, ZT + 0.3), (X1 + 0.1, DOOR_A[2], 1.2), (-1.5, Y0 - 0.2, 1.5)])


if __name__ == '__main__':
    main()
