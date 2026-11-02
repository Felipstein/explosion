"""O Instituto de pesquisa da Rússia (o НИИ, a Tecnologia) provisório, modelado no Blender por script (a facção nova, 01/11/2026:
docs/FACCAO_RUSSIA.md §1 e §5.1: "o prédio de pesquisa com o domo do radar e o mastro de antenas de guerra eletrônica").

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- building C:/Users/luisf/explosion/Game/Tools/model_russia_institute.py C:/Users/luisf/explosion/captures/game/models/russia_institute

A planta: o instituto de pesquisa soviético dos anos 70 (o bloco de painel de 3 andares com as janelas largas, o relevo do átomo na
empena cega, a sputnik no pedestal na frente) com a torre do radar (o tambor de painéis curvos com o DOMO BRANCO GEODÉSICO por cima, o
radome) e o MASTRO DE GUERRA ELETRÔNICA (a treliça triangular cheia de antenas: as log-periódicas, os painéis, a parabólica, as Yagis, as
varetas; a referência é o Krasukha e o Leer-3 da doutrina russa).

A palavra (o que lê de cima): O DOMO BRANCO. De cima, a maior bola branca da base (a assinatura redonda num mundo de retângulos) ao lado do
bloco de teto azul-acinzentado, e o mastro espetado de antenas no canto.

Peça por peça:
- o bloco (9,2 × 11 m, 3 andares, 10,6 m na platibanda): oco, painéis de 3 m com as janelas largas de caixilho branco, as lajes dos
  andares e os pilares por dentro; a entrada com a marquise, as 2 faixas verticais de pano da cor do jogador, o letreiro estêncil; a
  empena −Y cega com o RELEVO DO ÁTOMO (as 3 órbitas brancas e o núcleo vermelho, o painel decorativo soviético); no teto, a casa de
  máquinas, os condensadores, as saídas de ventilação;
- a torre do radar: o tambor oco de 18 painéis curvos com as juntas fundas (2 fiadas), a porta, as frestas de janela, o anel do topo e o
  domo geodésico branco (as costuras em relevo seguem a esfera), o para-raios e a luz de obstáculo; a passarela de ligação com o bloco;
- o mastro de guerra eletrônica (15,8 m): a treliça triangular, as plataformas, as antenas, a luz do topo, os cabos que descem;
- a sputnik no pedestal (a esfera polida e as 4 antenas) na praça da frente, o mastro com a bandeira.

Lote 16 × 22 m (X × Y), frente em +X. Alturas: o bloco 10,6 m, o tambor 6,9 m e o domo até 12,4 m, o mastro de antenas 15,8 m (a luz
16,1 m), a sputnik 3,4 m, a bandeira 8 m.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_Russia_Institute_Base: tudo (nada gira; a bandeira sai no flags.json);
  SM_Russia_Institute_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da Rússia): Concrete, Roof, Metal, Red, White, Paint, Team, Glass, Light, Dark, Trim, Hazard, Rubber; Beacon (a luz de
obstáculo).
"""
import math
import os
import sys

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, scaffold  # noqa: E402,F401
from model_faction_kit import lamp, disc_ring, revolve  # noqa: E402
from model_building_parts import WallFace, wbox, tube, cable, splits  # noqa: E402
from model_russia_parts import (ru_foundation, RuPart, ru_block, stencil, entry_door, flag_pole, lattice_mast, vent_shaft, ac_box, ladder_on,  # noqa: E402
                                JOINT, ru_star)

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_institute')
PREFIX = 'SM_Russia_Institute'

HX, HY = 8.0, 11.0                  # o lote 16 × 22
Z0 = 0.14
FL = Z0 + 0.12
T = 0.35
BX0, BX1, BY0, BY1 = -7.4, 1.8, -8.4, 2.6
F2, F3, ZT = 3.44, 6.74, 10.0
DOME_C = (-2.2, 6.9)                # o eixo do tambor e do domo
DR, DZ = 3.0, 6.4                   # o raio do tambor e o topo dele
MAST = (-6.2, 9.7)


def block(p):
    """O bloco de 3 andares (oco) com as janelas largas, a entrada, as faixas de pano, o relevo do átomo na empena −Y, o teto."""
    door = (-3.4, -1.6, FL + 0.3, FL + 2.6)
    ru_block(p, BX0, BX1, BY0, BY1, Z0, ZT, t=T, pw=3.0, zs=[F2, F3, ZT], parapet=0.6, floors=[F2, F3],
             holes={'+x': [door]},
             win={'+x': dict(floors=[Z0, F2, F3], w=2.1, h=1.6, sill=0.85, skip=[(1, 0)], lit=[(0, 2), (2, 1)]),
                  '-x': dict(floors=[Z0, F2, F3], w=2.1, h=1.6, sill=0.85, lit=[(1, 1)]),
                  '+y': dict(floors=[Z0, F2, F3], w=2.1, h=1.6, sill=0.85, skip=[(1, 0)])},
             cols=[(-2.8, -3.0)], step=3.0)
    F = WallFace('+x', BX1, 0.0)
    entry_door(p, F, door[0], door[1], door[2], door[3], t=T, canopy=1.6)
    stencil(p, F, -2.5, door[3] + 0.75, 'ЭЛГ-4', h=0.34, m='White', n=0.08)
    # as 2 faixas verticais de pano (a cor do jogador) penduradas da platibanda, com a barra de baixo
    for u in (-5.6, 0.6):
        for (za, zb) in ((ZT + 0.45, ZT - 4.2),):
            pts_top = [F.pt(u - 0.55 + 1.1 * k / 8, za, 0.12 + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
            pts_bot = [F.pt(u - 0.55 + 1.1 * k / 8, zb, 0.12 + 0.03 * math.sin(k * math.pi / 2.0)) for k in range(9)]
            pts_top2 = [(q[0] + 0.02, q[1], q[2]) for q in pts_top]
            pts_bot2 = [(q[0] + 0.02, q[1], q[2]) for q in pts_bot]
            p.loft('Team#banner', [pts_bot + list(reversed(pts_bot2)), pts_top + list(reversed(pts_top2))], bevel=0.0)
        p.cyl('Paint#bar', (BX1 + 0.17, u, ZT - 4.25), 'y', 0.035, 1.3, seg=8, bevel=0.0)
        ru_star(p, WallFace('+x', BX1 + 0.17, 0.0), u, ZT - 1.4, 0.3, n=0.0, t=0.012, w=0.12)
    # o relevo do átomo na empena cega −Y (o painel decorativo soviético): 3 órbitas brancas e o núcleo vermelho
    Fs = WallFace('-y', BY0, 0.0)
    cu, cz = (BX0 + BX1) / 2, 6.2
    for ang in (0.0, 60.0, 120.0):
        a = math.radians(ang)
        pts = []
        for k in range(49):
            t = 2 * math.pi * k / 48
            eu, ez = 3.0 * math.cos(t), 1.05 * math.sin(t)
            pts.append(Fs.pt(cu + eu * math.cos(a) - ez * math.sin(a), cz + eu * math.sin(a) + ez * math.cos(a), 0.16))
        tube(p, 'White#atom', pts, 0.075, seg=8)
    c = Vector(Fs.pt(cu, cz, 0.12))
    p.cyl('Red#atom', tuple(c), 'y', 0.5, 0.14, seg=28, bevel=0.0)
    p.torus('White#atom', Fs.pt(cu, cz, 0.2), 'y', 0.5, 0.05, seg=28, rseg=6)
    for k in range(3):                                                           # os elétrons
        a = math.radians(60.0 * k + 25.0)
        q = Fs.pt(cu + 3.0 * math.cos(a) * math.cos(math.radians(60 * k)) - 1.05 * math.sin(a) * math.sin(math.radians(60 * k)),
                  cz + 3.0 * math.cos(a) * math.sin(math.radians(60 * k)) + 1.05 * math.sin(a) * math.cos(math.radians(60 * k)), 0.2)
        p.cyl('Red#atom', q, 'y', 0.16, 0.12, seg=16, bevel=0.0)
    # o teto: a casa de máquinas, os condensadores, as saídas de ventilação, a escada
    ru_block(p, -6.6, -4.0, -7.6, -5.0, ZT + 0.05, ZT + 2.45, t=0.25, pw=2.6, zs=[ZT + 2.45], parapet=0.0, socle=0.0, roof='flat',
             interior=False, holes={'+x': [(-6.7, -5.9, ZT + 0.1, ZT + 2.1)]})
    entry_door(p, WallFace('+x', -4.0, 0.0), -6.7, -5.9, ZT + 0.12, ZT + 2.1, t=0.25, canopy=0.0)
    for (x, y) in ((-1.6, -6.6), (-1.6, -4.4)):
        ac_box(p, x, y, ZT + 0.05)
    for (x, y) in ((-6.4, 1.2), (-0.6, 1.4), (-4.0, -1.8)):
        vent_shaft(p, x, y, ZT + 0.05, sx=0.8, sy=0.6, h=0.7)
    ladder_on(p, WallFace('-x', BX0, 0.0), -6.4, Z0, ZT + 0.6, cage=False)
    lamp(p, BX1 + 0.08, -6.6, 3.0, face='+x')


def arc_panel(p, m, c, r0, r1, a0, a1, z0, z1, seg=3, bevel=0.08):
    """Um painel curvo do tambor (o setor de anel entre os ângulos a0..a1, raios r0..r1, de z0 a z1): só quads."""
    bb = p.bm(m, bevel)
    ang = [a0 + (a1 - a0) * k / seg for k in range(seg + 1)]

    def P(r, a, z):
        return bb.verts.new(V(c[0] + r * math.cos(a), c[1] + r * math.sin(a), z))
    o0 = [P(r1, a, z0) for a in ang]
    o1 = [P(r1, a, z1) for a in ang]
    i0 = [P(r0, a, z0) for a in ang]
    i1 = [P(r0, a, z1) for a in ang]
    for k in range(seg):
        bb.faces.new([o0[k], o0[k + 1], o1[k + 1], o1[k]])
        bb.faces.new([i0[k + 1], i0[k], i1[k], i1[k + 1]])
        bb.faces.new([o1[k], o1[k + 1], i1[k + 1], i1[k]])
        bb.faces.new([o0[k + 1], o0[k], i0[k], i0[k + 1]])
    bb.faces.new([o0[0], o1[0], i1[0], i0[0]])
    bb.faces.new([o0[-1], i0[-1], i1[-1], o1[-1]])


def radar_tower(p):
    """O tambor de painéis curvos (18 por fiada, 2 fiadas, as juntas fundas com a massa) com a porta e as frestas, o anel do topo, o domo
    geodésico branco com as costuras em relevo, o para-raios e a luz; a passarela de ligação com o bloco."""
    cx, cy = DOME_C
    n = 18
    gap = 0.045 / DR
    rows = [(Z0, 3.4), (3.4, DZ)]
    door_a = 0.0                                                                  # a porta virada pra +X
    for (za, zb) in rows:
        for k in range(n):
            a0 = 2 * math.pi * k / n + gap
            a1 = 2 * math.pi * (k + 1) / n - gap
            mid = (a0 + a1) / 2
            if za < 1.0 and abs(math.atan2(math.sin(mid - door_a), math.cos(mid - door_a))) < math.pi / n:
                arc_panel(p, 'Concrete#drum', DOME_C, DR - 0.32, DR, a0, a1, 2.7, zb - 0.02)    # o painel em cima da porta
                continue
            arc_panel(p, 'Concrete#drum', DOME_C, DR - 0.32, DR, a0, a1, za + 0.02, zb - 0.02)
        if za > 1.0:                                                              # a massa da junta horizontal entre as fiadas
            disc_ring(p, JOINT, DOME_C, DR - 0.06, DR - 0.31, za - 0.015, za + 0.015, seg=36)
    for k in range(n):                                                            # a massa no fundo das juntas verticais
        a = 2 * math.pi * k / n
        p.obox(JOINT, (cx + (DR - 0.15) * math.cos(a), cy + (DR - 0.15) * math.sin(a), (Z0 + DZ) / 2),
               (0.24, 0.1, DZ - Z0 - 0.04), yaw=math.degrees(a), bevel=0.0)
    # a porta (o vão do painel de baixo virado pra +X) e as frestas de janela (2 por painel, recuadas)
    p.obox('Dark', (cx + DR - 0.2, cy, Z0 + 1.3), (0.06, 0.9, 2.5), bevel=0.0)
    p.obox('Paint#door', (cx + DR - 0.1, cy, Z0 + 1.25), (0.06, 0.85, 2.3), bevel=0.0)
    p.obox('Concrete#canopy', (cx + DR + 0.35, cy, 2.85), (0.9, 1.6, 0.16), bevel=0.04)
    for k in range(n):
        a = 2 * math.pi * (k + 0.5) / n
        if abs(math.atan2(math.sin(a - math.pi), math.cos(a - math.pi))) < 0.9 or k % 2:
            continue
        for zz in (4.3, 5.4):
            p.obox('Glass', (cx + (DR + 0.005) * math.cos(a), cy + (DR + 0.005) * math.sin(a), zz), (0.04, 0.22, 0.8),
                   yaw=math.degrees(a), bevel=0.0)
            p.obox('Metal#slit', (cx + (DR + 0.03) * math.cos(a), cy + (DR + 0.03) * math.sin(a), zz - 0.43), (0.12, 0.34, 0.06),
                   yaw=math.degrees(a), bevel=0.0)
    # o anel do topo (a cinta de concreto e o rufo) e o domo geodésico
    revolve(p, 'Concrete#ring', DOME_C, [(DR - 0.4, DZ - 0.02), (DR + 0.12, DZ - 0.02), (DR + 0.12, DZ + 0.42), (DR - 0.4, DZ + 0.42),
                                         (DR - 0.4, DZ - 0.02)], seg=48, smooth=False, caps=(False, False))
    disc_ring(p, 'Paint#flash', DOME_C, DR + 0.16, DR - 0.44, DZ + 0.42, DZ + 0.47, seg=48)
    R = 3.25
    zc = DZ + 0.47 - R * math.sin(math.radians(-22.0))
    prof = [(R * math.cos(math.radians(a)), zc + R * math.sin(math.radians(a))) for a in range(-22, 91, 7)]
    prof[-1] = (0.02, zc + R)
    revolve(p, 'White#dome', DOME_C, prof, seg=56, smooth=True, caps=(True, False))
    geodesic_seams(p, (cx, cy, zc), R, zcut=DZ + 0.5)
    p.cyl('Metal#rod', (cx, cy, zc + R + 0.5), 'z', 0.03, 1.0, seg=6, bevel=0.0)
    p.cyl('Beacon', (cx, cy, zc + R + 1.05), 'z', 0.07, 0.14, seg=10, bevel=0.0, smooth=True)
    # a passarela de ligação com o bloco (1 andar, envidraçada) e a escada externa do anel
    lx0, lx1 = -3.4, -1.0
    p.box('Concrete#link', lx0, lx1, BY1 + 0.05, cy - DR + 0.35, F2 - 0.25, F2 + 0.05, bevel=0.04)
    p.box('Concrete#link', lx0, lx1, BY1 + 0.05, cy - DR + 0.35, F2 + 2.9, F2 + 3.2, bevel=0.04)
    for x in (lx0 + 0.08, lx1 - 0.08):
        wbox(p, 'Glass', WallFace('+x' if x > -2 else '-x', x, 0.0), BY1 + 0.1, cy - DR + 0.3, F2 + 0.1, F2 + 2.85, -0.02, 0.0, bevel=0.0)
        for y in splits(BY1 + 0.1, cy - DR + 0.3, 0.6):
            p.box('White#win', x - 0.05, x + 0.05, y - 0.04, y + 0.04, F2 + 0.05, F2 + 2.9, bevel=0.012)
    for x in (lx0 + 0.2, lx1 - 0.2):
        p.box('Concrete#link', x - 0.2, x + 0.2, BY1 + 0.6, BY1 + 1.0, Z0, F2 - 0.25, bevel=0.04)


def geodesic_seams(p, c, R, zcut, sub=2, r=0.035):
    """As costuras do radome geodésico: as arestas do icosaedro subdividido, seguindo a esfera (cada aresta em 4 trechos no arco), só
    acima do anel."""
    tmp = bmesh.new()
    bmesh.ops.create_icosphere(tmp, subdivisions=sub, radius=1.0)
    cx, cy, cz = c
    for e in tmp.edges:
        a, b = e.verts[0].co.copy(), e.verts[1].co.copy()
        za, zb = cz + R * a.z, cz + R * b.z
        if min(za, zb) < zcut + 0.15:
            continue
        pts = []
        for k in range(5):
            q = (a * (1 - k / 4) + b * (k / 4)).normalized() * (R + 0.012)
            pts.append((cx + q.x, cy - q.y, cz + q.z))
        tube(p, 'White#seam', pts, r, seg=5, smooth=True)
    tmp.free()


def ew_mast(p):
    """O mastro de guerra eletrônica: a treliça triangular de 15,8 m nas sapatas, as faixas de aviação no topo, as 2 plataformas com o
    guarda-corpo, as antenas (as log-periódicas, os painéis, a parabólica, as Yagis, as varetas), a luz do topo, o cabo que desce."""
    mx, my = MAST
    z1 = 15.8
    p.box('Concrete#foot', mx - 1.0, mx + 1.0, my - 1.0, my + 1.0, Z0 - 0.02, Z0 + 0.35, bevel=0.05)
    lattice_mast(p, MAST, Z0 + 0.35, 11.5, 0.72, 0.42, 8, m='Paint#mast', leg=0.075, brace=0.04)
    lattice_mast(p, MAST, 11.5, z1, 0.42, 0.24, 3, m='Red#mast', leg=0.06, brace=0.035)
    for zp, rr in ((7.0, 1.15), (12.6, 0.95)):
        disc_ring(p, 'Metal#plat', MAST, rr, 0.2, zp - 0.06, zp, seg=24)
        for k in range(10):
            a = 2 * math.pi * (k + 0.5) / 10
            p.cyl('Metal#rail', (mx + rr * math.cos(a), my + rr * math.sin(a), zp + 0.5), 'z', 0.022, 1.0, seg=6, bevel=0.0)
        p.torus('Metal#rail', (mx, my, zp + 1.0), 'z', rr, 0.03, seg=24, rseg=6)
    # as log-periódicas (o garfo de dipolos que encolhe ao longo da lança) apontando pra +X e pra −Y
    for (yaw, z) in ((0.0, 5.6), (-90.0, 5.0), (-45.0, 9.6)):
        a = math.radians(yaw)
        d, s = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
        base = Vector((mx, my, z)) + d * 0.5
        tube(p, 'Metal#lpda', [tuple(base), tuple(base + d * 2.2)], 0.04, seg=6)
        tube(p, 'Metal#lpda', [tuple(base + Vector((0, 0, 0.12))), tuple(base + d * 2.2 + Vector((0, 0, 0.12)))], 0.03, seg=6)
        for k in range(8):
            q = base + d * (0.15 + k * 0.28)
            L = 1.9 - k * 0.19
            tube(p, 'White#lpda', [tuple(q + s * L / 2), tuple(q - s * L / 2)], 0.022, seg=4)
        tube(p, 'Metal#lpda', [(mx, my, z - 0.8), tuple(base)], 0.035, seg=6)
    # os painéis (os setoriais brancos) em volta do topo da parte cinza
    for k in range(3):
        a = math.radians(30 + 120 * k)
        q = (mx + 0.75 * math.cos(a), my + 0.75 * math.sin(a), 10.3)
        p.obox('White#panel', q, (0.16, 0.42, 1.5), yaw=math.degrees(a), bevel=0.03)
        p.obox('Metal#brk', (mx + 0.55 * math.cos(a), my + 0.55 * math.sin(a), 10.3), (0.3, 0.06, 0.06), yaw=math.degrees(a), bevel=0.0)
    # a parabólica na plataforma de cima, virada pra +X
    dc = Vector((mx + 0.9, my - 0.2, 13.5))
    p.cyl('White#dish', tuple(dc), 'x', 0.7, 0.12, seg=24, r2=0.55, bevel=0.0, smooth=True)
    p.cyl('Dark', tuple(dc + Vector((0.065, 0, 0))), 'x', 0.52, 0.01, seg=24, bevel=0.0)
    tube(p, 'Metal#feed', [tuple(dc), tuple(dc + Vector((0.7, 0, 0)))], 0.025, seg=6)
    tube(p, 'Metal#brk', [tuple(dc - Vector((0.1, 0, 0))), (mx, my, 13.5)], 0.045, seg=6)
    # as Yagis no topo vermelho e as varetas
    for (yaw, z) in ((-45.0, 14.4), (-135.0, 15.0)):
        a = math.radians(yaw)
        d, s = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
        base = Vector((mx, my, z)) + d * 0.3
        tube(p, 'Metal#yagi', [tuple(base), tuple(base + d * 1.6)], 0.03, seg=6)
        for k in range(6):
            q = base + d * (0.1 + k * 0.28)
            L = 0.9 if k == 0 else 0.72
            tube(p, 'White#yagi', [tuple(q + s * L / 2), tuple(q - s * L / 2)], 0.02, seg=4)
    for (dx, dy, h) in ((0.12, 0.1, 1.6), (-0.15, 0.05, 1.1), (0.0, -0.16, 0.8)):
        p.cyl('Metal#whip', (mx + dx, my + dy, z1 + h / 2), 'z', 0.025, h, seg=6, bevel=0.0)
    p.cyl('Beacon', (mx, my, z1 + 0.08), 'z', 0.08, 0.16, seg=10, bevel=0.0, smooth=True)
    cable(p, (mx + 0.4, my - 0.4, 6.8), (BX0 + 2.0, BY1 - 0.4, ZT + 0.65), sag=0.8, r=0.03, m='Dark#cable')


def ground_dish(p, x, y, yaw=205.0, elev=32.0, R=1.55):
    """A antena parabólica de chão (a estação de rastreio do instituto): a sapata, o pedestal, a cabeça de azimute, o prato branco
    (o perfil parabólico de revolução), as 4 hastes do alimentador e o contrapeso."""
    p.box('Concrete#foot', x - 0.8, x + 0.8, y - 0.8, y + 0.8, Z0, Z0 + 0.3, bevel=0.05)
    p.cyl('Paint#ped', (x, y, Z0 + 1.2), 'z', 0.32, 1.8, seg=16, r2=0.24, bevel=0.0)
    p.box('Paint#head', x - 0.35, x + 0.35, y - 0.35, y + 0.35, Z0 + 2.1, Z0 + 2.5, bevel=0.04)
    a, e = math.radians(yaw), math.radians(elev)
    d = Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)))
    c = Vector((x, y, Z0 + 2.75)) + d * 0.35
    ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    u = d.cross(ref).normalized()
    v = d.cross(u).normalized()
    rings = []
    for (r, h) in ((0.12, 0.0), (0.5, 0.06), (0.9, 0.2), (1.25, 0.39), (R, 0.6), (R + 0.05, 0.62), (R, 0.66), (1.2, 0.47), (0.12, 0.1)):
        q = c + d * h
        rings.append([tuple(q + (u * math.cos(2 * math.pi * k / 32) + v * math.sin(2 * math.pi * k / 32)) * r) for k in range(32)])
    p.loft('White#gdish', rings, bevel=0.0, smooth=True)
    f = c + d * 1.55
    for k in range(4):
        w = u * math.cos(math.pi * k / 2 + 0.785) + v * math.sin(math.pi * k / 2 + 0.785)
        tube(p, 'Metal#feed', [tuple(c + d * 0.6 + w * (R - 0.1)), tuple(f)], 0.03, seg=6)
    p.cyl('Paint#feed', tuple(f), tuple(d), 0.12, 0.3, seg=12, bevel=0.0)
    p.cyl('Metal#cw', tuple(c - d * 0.5), tuple(d), 0.22, 0.4, seg=12, bevel=0.0)
    tube(p, 'Metal#yoke', [(x, y, Z0 + 2.45), tuple(c)], 0.08, seg=8)


def plaza(p):
    """A praça da frente: a sputnik no pedestal (a esfera polida, as 4 antenas varridas pra trás) e o mastro com a bandeira."""
    sx, sy = 5.4, -4.6
    p.box('Concrete#ped', sx - 0.9, sx + 0.9, sy - 0.9, sy + 0.9, Z0, Z0 + 0.3, bevel=0.05)
    p.loft('Concrete#ped', [[(sx - 0.45, sy - 0.45, Z0 + 0.3), (sx + 0.45, sy - 0.45, Z0 + 0.3), (sx + 0.45, sy + 0.45, Z0 + 0.3),
                             (sx - 0.45, sy + 0.45, Z0 + 0.3)],
                            [(sx - 0.28, sy - 0.28, 2.4), (sx + 0.28, sy - 0.28, 2.4), (sx + 0.28, sy + 0.28, 2.4), (sx - 0.28, sy + 0.28, 2.4)]],
           bevel=0.06)
    cz = 2.95
    revolve(p, 'Metal#sput', (sx, sy), [(0.02, cz - 0.55)] + [(0.55 * math.cos(math.radians(a)), cz + 0.55 * math.sin(math.radians(a)))
                                                            for a in range(-80, 81, 16)] + [(0.02, cz + 0.55)], seg=24, smooth=True)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        q0 = Vector((sx + 0.4 * math.cos(a), sy + 0.4 * math.sin(a), cz + 0.15))
        q1 = q0 + Vector((-1.9, 0.5 * math.cos(a), -0.35 + 0.25 * math.sin(a)))
        tube(p, 'Metal#sput', [tuple(q0), tuple(q1)], 0.02, seg=5)
    flag_pole(p, 6.0, 0.4, Z0, 7.6, cloth=(1.8, 1.15), phase=0.5)
    ground_dish(p, 4.9, 6.9)


def base():
    p = RuPart(PREFIX + '_Base')
    ru_foundation(p, HX - 0.1, HY - 0.1, z=Z0)
    block(p)
    radar_tower(p)
    ew_mast(p)
    plaza(p)
    return p.finish()


def main():
    os.makedirs(OUT, exist_ok=True)
    clear_scene()
    total = export(base(), OUT)
    clear_scene()
    total += export(scaffold(PREFIX + '_Scaffold', -HX + 1.9, HX - 1.9, -HY + 1.9, HY - 1.9, ZT + 0.6, post='Metal', plank='Wood',
                             stripe='Hazard'), OUT)
    print('MODEL total', total, 'tris')
    print('MODEL lote 16 x 22 m; domo em', DOME_C, 'ate ~', round(DZ + 0.47 + 3.25 * (1 + math.sin(math.radians(22))), 2), 'm; mastro em',
          MAST, 'ate 15.8 m')
    print('MODEL fire_points_m', [(-4.6, -3.0, ZT + 0.1), (-1.2, 0.6, ZT + 0.1), (DOME_C[0] + 2.6, DOME_C[1], DZ + 0.5)])


if __name__ == '__main__':
    main()
