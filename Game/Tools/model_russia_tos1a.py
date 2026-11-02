"""O TOS-1A "Solntsepyok" da Rússia (a artilharia termobárica) provisório, modelado no Blender por script (01/11/2026;
docs/FACCAO_RUSSIA.md §3: "o lançador de 24 tubos grossos num chassi de tanque, inclinado: a silhueta que ninguém tem").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  (model_run_detail.py) vehicle Game/Tools/model_russia_tos1a.py C:/Users/luisf/explosion/captures/game/models/russia_tos1a

A base é o TOS-1A de verdade (o BM-1 sobre o chassi do T-72, 24 tubos de 220 mm): o casco do T-72/T-90 do kit (o glacis e a saia com
o Relikt, a grade de barras atrás), a MESA GIRATÓRIA baixa no anel da torre com as 2 orelhas da dobradiça atrás e os 2 cilindros de
elevação na frente, e o BLOCO DE 24 TUBOS (6 × 4) na caixa blindada (as chapas de lado e de cima com as nervuras, a frente aberta mostrando
as bocas, a traseira com as tampas), deitado pra frente em marcha; por cima do bloco, a gaiola de ripas com a rede da cor do jogador (a
assinatura). O jogo gira a mesa (turret) e levanta o bloco pela dobradiça de trás (o bloco "launcher", como o do Tomahawk).
A palavra: o BLOCO DE CANOS. De cima: a caixa comprida e larga cobrindo o meio do tanque, a gaiola com a rede por cima; de frente, a
grade de 24 bocas escuras.

Peças (origem no chão, no centro do casco, frente em +X):
  SM_Russia_TOS1A_Hull      casco, saias, deck, lona enrolada, o anel;
  SM_Russia_TOS1A_Turret    a mesa giratória (gira em Z em volta de (TX, 0)) com as orelhas da dobradiça e os corpos dos cilindros;
  SM_Russia_TOS1A_Launcher  o bloco de 24 tubos com a gaiola e a rede, DEITADO (marcha); levanta em volta de HINGE (eixo Y, positivo
                            levanta as bocas): o bloco "launcher" do Units.json (hinge_m, fire_pitch_deg, tubes_m);
  SM_Russia_TOS1A_Wheels    a rodagem (wheel_*);
  SM_Russia_TOS1A_Track_l / _Track_r  as esteiras.
  pose/SM_Russia_TOS1A_Launcher_Fire: a mesa + o bloco levantado FIRE_PITCH, só pra prévia (fora do glob da importação).
Slots: Paint, Metal, Team, Track, Lens, Cloth, Rubber, Red, White, Dark.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import T72, bounds, frame_box, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_tos1a')
os.makedirs(OUT, exist_ok=True)
POSE = os.path.join(OUT, 'pose')
os.makedirs(POSE, exist_ok=True)
PREFIX = 'SM_Russia_TOS1A'

TX = T72.RING_X
DECK_T = 1.62                       # o tampo da mesa giratória
NC, NR = 6, 4                       # os tubos: 6 de largura, 4 de altura (24)
PITCH = 0.29                        # o passo dos tubos (220 mm + a parede e o vão)
TUBE_R = 0.13
POD_X = (-1.25, 3.65)               # o bloco: de trás (a dobradiça) até a frente (passa 0,2 m do nariz)
POD_Z0 = 1.7                        # o fundo do bloco deitado
WALL = 0.06                         # a chapa da caixa
POD_HW = NC * PITCH / 2 + WALL
POD_Z1 = POD_Z0 + NR * PITCH + 2 * WALL
HINGE = (POD_X[0] + 0.18, POD_Z0 - 0.02)     # a dobradiça (x, z), eixo em Y
FIRE_PITCH = 40.0
CAGE_DZ = 0.26                      # a gaiola por cima do bloco
# (01/11) os 2 cilindros de elevação (y ±0,62): do pino na frente da mesa (RAM_BASE, x z) ao olhal embaixo do bloco (RAM_TIP, deitado).
# Deitado o pistão tem 0,90 m e na pose de tiro (40°) 2,01 m: 3 estágios telescópicos (o cilindro, o do meio e a haste), cada um uma
# peça; o jogo aponta as 3 da base pro olhal a cada quadro e desliza o do meio metade da extensão (o pistão de jogo grande: as peças
# rígidas que se miram, sem esticar malha). Antes o cilindro ficava deitado na mesa e a haste ia com o bloco: soltavam ao levantar
RAM_BASE = (TX + 0.85, DECK_T + 0.08)
RAM_TIP = (TX + 1.75, POD_Z0 - 0.04)
RAM_CYL = (0.0, 0.70, 0.08)         # (de, até, raio) em metros pelo eixo, a partir da base
RAM_MID = (0.05, 0.85, 0.06)
RAM_ROD = (0.15, None, 0.045)       # a haste vai até o olhal


def tube_centers():
    out = []
    for i in range(NR):
        for j in range(NC):
            out.append(((j - (NC - 1) / 2) * PITCH, POD_Z0 + WALL + (i + 0.5) * PITCH))
    return out


def hull():
    p = rk.new_part(PREFIX + '_Hull')
    rk.t72_hull(p, era_glacis=True, era_skirt=True, rear_slats=True, tarp_roll=True)
    # o descanso do bloco em marcha (o cavalete em A no glacis, com o calço de borracha)
    for s in (-1, 1):
        vp.tube(p, 'Metal', [(2.55, 0.7 * s, T72.DECK), (2.6, 0.55 * s, POD_Z0 - 0.06)], 0.04, seg=8)
    vp.rbox(p, 'Rubber', (2.6, 0.0, POD_Z0 - 0.04), (0.18, 1.2, 0.05), r=0.02, k=1)
    vp.tube(p, 'Metal', [(2.6, -0.55, POD_Z0 - 0.07), (2.6, 0.55, POD_Z0 - 0.07)], 0.035, seg=8)
    return p.finish()


def turret():
    """A mesa giratória baixa: o disco no anel, a plataforma, as 2 orelhas da dobradiça atrás e os corpos dos cilindros de elevação."""
    p = rk.new_part(PREFIX + '_Turret')
    vp.lathe(p, 'Paint', (TX, 0.0, T72.DECK), 'z', [(0.0, 0.03), (1.0, 0.03), (1.0, 0.06), (0.96, 0.07), (0.0, 0.07)], seg=56)
    pts = []
    for s in (-1, 1):
        pts += [(TX + 1.15, 0.7 * s, 1.48), (TX + 0.7, 1.15 * s, 1.48), (TX - 1.55, 1.12 * s, 1.48), (TX - 1.75, 0.8 * s, 1.48)]
        pts += [(TX + 1.05, 0.66 * s, DECK_T), (TX + 0.65, 1.1 * s, DECK_T), (TX - 1.5, 1.07 * s, DECK_T), (TX - 1.7, 0.76 * s, DECK_T)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    hx, hz = HINGE
    for s in (-1, 1):
        # as orelhas da dobradiça (as chapas em pé atrás) e o eixo
        y = (POD_HW + 0.06) * s
        vp.prism(p, 'Paint#plate', [(-0.3, 0.0), (0.3, 0.0), (0.12, hz - DECK_T + 0.12), (-0.12, hz - DECK_T + 0.12)],
                 Vector((hx, y - 0.04, DECK_T)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0)), 0.08)
        p.cyl('Metal#vp', (hx, (POD_HW + 0.14) * s, hz), 'y', 0.07, 0.08, seg=14, bevel=0.0)
        # a base do cilindro de elevação (o garfo e o pino na frente da mesa); o cilindro, o estágio do meio e a haste são peças
        # próprias (rams(): o jogo aponta as 3 do pino da mesa pro olhal do bloco a cada quadro)
        a = Vector((RAM_BASE[0], 0.62 * s, RAM_BASE[1]))
        vp.rbox(p, 'Paint#plate', (a.x, a.y, DECK_T + 0.05), (0.22, 0.2, 0.1), r=0.03, k=2)
        p.cyl('Metal#vp', tuple(a), 'y', 0.09, 0.14, seg=10, bevel=0.0)
    # a caixa do operador do lançador (o sensor e o computador de tiro, à esquerda na frente da mesa)
    vp.rbox(p, 'Paint#plate', (TX + 0.4, -0.95, DECK_T + 0.12), (0.5, 0.3, 0.24), r=0.04, k=2)
    vp.rbox(p, 'Lens', (TX + 0.655, -0.95, DECK_T + 0.15), (0.012, 0.18, 0.1), r=0.02, k=2)
    # as estrelas contornadas nas laterais da mesa
    for s in (-1, 1):
        rk.star_outlined(p, (TX - 0.6, 1.095 * s, 1.55), (0, s, 0), r=0.1)
    return p.finish()


def launcher():
    """O bloco de 24 tubos na caixa blindada (deitado), com a gaiola e a rede por cima."""
    p = rk.new_part(PREFIX + '_Launcher')
    x0, x1 = POD_X
    hw = POD_HW
    z0, z1 = POD_Z0, POD_Z1
    # as chapas da caixa: o fundo, as 2 laterais (com o chanfro de cima pra dentro), o teto; a frente e a traseira abertas
    for s in (-1, 1):
        pts = []
        for x in (x0, x1):
            pts += [(x, (hw - WALL) * s, z0), (x, hw * s, z0), (x, hw * s, z1 - 0.12), (x, (hw - 0.1) * s, z1),
                    (x, (hw - WALL - 0.04) * s, z1 - WALL), (x, (hw - WALL) * s, z0 + WALL)]
        solid(p, 'Paint#shell', pts, bevel=0.04)
        # as nervuras verticais de reforço nas laterais e as fileiras de parafusos
        for k in range(7):
            x = x0 + 0.3 + k * (x1 - x0 - 0.6) / 6
            vp.rbox(p, 'Paint#plate', (x, (hw + 0.025) * s, (z0 + z1) / 2 - 0.05), (0.08, 0.05, z1 - z0 - 0.25), r=0.02, k=1)
        vp.bolt_row(p, 'Metal', (x0 + 0.15, (hw + 0.004) * s, z0 + 0.06), (x1 - 0.15, (hw + 0.004) * s, z0 + 0.06), 16, (0, s, 0),
                    r=0.016, h=0.012)
        # o olhal embaixo do bloco (onde a haste do cilindro de elevação prende: rams())
        b = Vector((RAM_TIP[0], 0.62 * s, RAM_TIP[1]))
        vp.rbox(p, 'Paint#plate', (b.x, b.y, b.z), (0.2, 0.16, 0.1), r=0.03, k=2)
        # a estrela contornada na lateral do bloco (lê de lado e de 3/4)
        rk.star_outlined(p, (x1 - 1.0, (hw + 0.004) * s, (z0 + z1) / 2 + 0.05), (0, s, 0), r=0.17)
    vp.rbox(p, 'Paint#plate', ((x0 + x1) / 2, 0.0, z0 + WALL / 2), (x1 - x0, 2 * hw, WALL), r=0.04, k=2)
    vp.rbox(p, 'Paint#plate', ((x0 + x1) / 2, 0.0, z1 - WALL / 2), (x1 - x0 - 0.02, 2 * hw - 0.2, WALL), r=0.04, k=2)
    for k in range(5):                                                   # as costelas do teto
        x = x0 + 0.4 + k * (x1 - x0 - 0.8) / 4
        vp.rbox(p, 'Paint#plate', (x, 0.0, z1 + 0.02), (0.1, 2 * hw - 0.25, 0.05), r=0.02, k=1)
    # os 24 tubos: o cano grosso (aparece na frente e atrás), a boca com o aro e o fundo escuro, as divisórias da grade
    for (y, z) in tube_centers():
        # (só as pontas: o meio do tubo fica escondido na caixa)
        vp.lathe(p, 'Metal', (0.0, y, z), 'x', [(0.0, x1 - 0.45), (TUBE_R, x1 - 0.45), (TUBE_R, x1 + 0.03), (TUBE_R - 0.025, x1 + 0.03),
                                                (TUBE_R - 0.025, x1 - 0.3), (0.0, x1 - 0.3)], seg=12)
        vp.lathe(p, 'Metal', (0.0, y, z), 'x', [(0.0, x0 + 0.3), (TUBE_R, x0 + 0.3), (TUBE_R, x0 + 0.08), (TUBE_R - 0.01, x0 + 0.05),
                                                (0.0, x0 + 0.05)], seg=10)
        p.cyl('Dark#vp', (x1 - 0.28, y, z), 'x', TUBE_R - 0.03, 0.01, seg=10, bevel=0.0)
        vp.lathe(p, 'Paint', (x0 + 0.04, y, z), (-1, 0, 0), [(0.0, 0.0), (TUBE_R - 0.02, 0.0), (TUBE_R - 0.01, 0.02), (0.0, 0.03)], seg=10)
    # a moldura da frente (a grade de chapa entre as bocas: os quadros que dão o "favo" de frente)
    for i in range(NR + 1):
        z = z0 + WALL + i * PITCH
        frame_box(p, 'Paint#vp', (x1 - 0.06, 0.0, z), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.12, 2 * hw, 0.03)
    for j in range(NC + 1):
        y = (j - NC / 2) * PITCH
        frame_box(p, 'Paint#vp', (x1 - 0.06, y, (z0 + z1) / 2), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.12, 0.03,
                  z1 - z0 - 2 * WALL)
    # a dobradiça embaixo de trás (o eixo e as orelhas do bloco)
    hx, hz = HINGE
    for s in (-1, 1):
        vp.rbox(p, 'Paint#plate', (hx, (hw - 0.05) * s, hz + 0.08), (0.36, 0.08, 0.2), r=0.04, k=2)
    p.cyl('Metal#vp', (hx, 0.0, hz), 'y', 0.06, 2 * hw + 0.16, seg=14, bevel=0.0)
    # ---------------------------------------------------------------- a gaiola por cima do bloco (a assinatura) com a rede e a lona
    gz = z1 + CAGE_DZ
    posts = [(x0 + 0.4, 0.85, z1), (x0 + 0.4, -0.85, z1), (x0 + 2.3, 0.85, z1), (x0 + 2.3, -0.85, z1), (x1 - 0.5, 0.85, z1),
             (x1 - 0.5, -0.85, z1)]
    roof = rk.slat_cage(p, x0 + 0.1, x1 - 0.15, hw + 0.12, gz, gz, posts, visor=0.0, step=0.26)
    step = (x1 - 0.15 - x0 - 0.1) / max(2, int(round((x1 - 0.15 - x0 - 0.1) / 0.26)))

    def zf(x, y):
        z = roof(x) + 0.035 + 0.08 * math.exp(-((x - x0 - 1.6) ** 2 + (y + 0.3) ** 2) / 0.18)
        z -= 0.02 * math.sin(math.pi * (((x - x0 - 0.1) / step) % 1.0))
        if y < -(hw + 0.12):
            z -= min(0.3, (-(hw + 0.12) - y) * 1.4)
        return z
    rk.net_drape(p, x0 + 0.5, x0 + 2.75, -hw - 0.32, 0.45, zf, step=0.22, r=0.017, jitter=0.06, seed=12.0)
    rk.tarp_on_slats(p, x1 - 1.45, x1 - 0.2, -0.1, hw + 0.18, roof, step, x0 + 0.1, m='Team', seed=8.0, drape=[('right', 0.26)])
    vp.tarp_roll(p, (x1 - 1.45, (hw + 0.08) / 2, roof(x1 - 1.45) + 0.1), 'y', hw + 0.1, 0.08, m='Team', straps=2, strap_m='Cloth',
                 buckle_m='Metal')
    return p.finish()


def rams():
    """Os 3 estágios dos 2 cilindros de elevação, deitados (a pose de marcha), cada estágio uma peça (o jogo mira e desliza)."""
    base = Vector((RAM_BASE[0], 0.0, RAM_BASE[1]))
    tip = Vector((RAM_TIP[0], 0.0, RAM_TIP[1]))
    u = (tip - base).normalized()
    L = (tip - base).length
    out = []
    for name, (u0, u1, r), m in (('_RamCyl', RAM_CYL, 'Paint'), ('_RamMid', RAM_MID, 'Metal#ram'), ('_RamRod', RAM_ROD, 'Metal#rod')):
        p = rk.new_part(PREFIX + name)
        u1 = L if u1 is None else u1
        for s in (-1, 1):
            o = base + Vector((0.0, 0.62 * s, 0.0))
            vp.tube(p, m, [tuple(o + u * u0), tuple(o + u * u1)], r, seg=12)
            # o colar na ponta de cada estágio (a gaxeta que lê o telescópio) e, na haste, o olho do pino
            vp.tube(p, 'Paint#plate' if name == '_RamCyl' else 'Metal', [tuple(o + u * (u1 - 0.05)), tuple(o + u * u1)], r + 0.012, seg=12)
            if name == '_RamRod':
                p.cyl('Metal#vp', tuple(o + u * L), 'y', 0.05, 0.12, seg=10, bevel=0.0)
        out.append(p.finish())
    return out


def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    rk.t72_wheels(p)
    return p.finish()


def lift_matrix(deg):
    hx, hz = HINGE
    return (Matrix.Translation(Vector((hx, 0.0, hz))) @ Matrix.Rotation(math.radians(-deg), 4, 'Y')
            @ Matrix.Translation(Vector((-hx, 0.0, -hz))))


def launcher_check(h, t, l):
    """O bloco não pode bater no casco: deitado e na pose de tiro, a mesa girando 360° (BVH, triângulo contra triângulo)."""
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    th = tree(h, Matrix.Identity(4))
    piv = V(TX, 0.0, 0.0)
    for label, deg in (('deitado', 0.0), ('pose de tiro %d graus' % FIRE_PITCH, FIRE_PITCH)):
        hits = []
        for k in range(0, 360, 10):
            Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(k), 4, 'Z') @ Matrix.Translation(-piv)
            n = len(th.overlap(tree(l, Y @ lift_matrix(deg)))) + len(th.overlap(tree(t, Y)))
            if n:
                hits.append((k, n))
        print('MODEL lancador %s: %s' % (label, 'sem colisao em 360 graus' if not hits else 'COLISAO %s' % hits[:8]))


def fire_pose(t, l):
    """A mesa + o bloco levantado FIRE_PITCH (só pra prévia: pose/)."""
    o = l.copy()
    o.data = l.data.copy()
    o.data.transform(lift_matrix(FIRE_PITCH))
    t2 = t.copy()
    t2.data = t.data.copy()
    for x in (o, t2):
        bpy.context.scene.collection.objects.link(x)
    from model_lib import select_only
    select_only(o)
    t2.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = PREFIX + '_Launcher_Fire'
    return o


def main():
    clear_scene()
    h, t, l, w = hull(), turret(), launcher(), wheels()
    rc, rm, rr = rams()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l'), rk.t72_track(PREFIX + '_Track_r', 'r')
    sweep_check([h], [t], (TX, 0.0), label='mesa')
    launcher_check(h, t, l)
    import model_lib
    fire = None
    if model_lib.DETAIL.get('defer') is None:
        fire = fire_pose(t, l)
    total = 0
    for o in (h, t, l, w, tl, tr, rc, rm, rr):
        total += export(o, OUT)
    if fire is not None:
        export(fire, POSE)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, l, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, l, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), casco %.2f x %.2f m, altura %.2f m (o bloco %.2f, a gaiola %.2f)'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, az1, POD_Z1, POD_Z1 + CAGE_DZ))
    print('MODEL turret_pivot_m', [TX, 0.0], 'tracks_m', [round(2 * T72.TRACK_Y, 3), T72.TRACK_W])
    print('MODEL launcher: mesh SM_Russia_TOS1A_Launcher, hinge_m', [round(HINGE[0], 3), 0.0, round(HINGE[1], 3)], '(eixo Y)',
          'fire_pitch_deg', FIRE_PITCH)
    tubes = [[round(POD_X[1] + 0.03, 3), round(y, 3), round(z, 3)] for (y, z) in tube_centers()]
    print('MODEL tubes_m (as 24 bocas, deitado; 4 fileiras de 6, de baixo pra cima)', tubes)
    print('MODEL ram: base_m', [round(RAM_BASE[0], 3), 0.0, round(RAM_BASE[1], 3)], 'tip_m', [round(RAM_TIP[0], 3), 0.0, round(RAM_TIP[1], 3)])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Launcher', '_Wheels', '_Track_l', '_Track_r', '_RamCyl',
                                                          '_RamMid', '_RamRod')])


main()
