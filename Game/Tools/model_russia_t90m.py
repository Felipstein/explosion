"""O tanque básico da Rússia (T-90M "Proryv") provisório, modelado no Blender por script (01/11/2026; a facção: docs/FACCAO_RUSSIA.md).

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_russia_t90m.py C:/Users/luisf/explosion/captures/game/models/russia_t90m

Sem ficha aprovada ainda (o caminho da facção: a paleta, a folha de ideias, a ficha). A base é o T-90M de verdade, na escala de
verdade (o tanque da USA é o M1A2 de verdade, 9,75 m com o cano): casco de 6,9 m, 9,5 m com o cano, 3,8 m de largura com a saia, o
teto da torre a 2,17 m (o soviético baixo); a gaiola leva o alto a ~2,95 m.
A palavra: a GAIOLA. De cima: o telhado de ripas por cima da torre inteira (a viseira dobrada na frente), com a lona da cor do
jogador esticada na parte de trás e a rede jogada por cima da frente e caindo pela borda da esquerda; por baixo, a cunha de tijolos do
Relikt abrindo em V na frente da torre (as "escamas"), o cano comprido com o evacuador.
  - o casco do T-72/T-90 (model_russia_vehicle_kit.t72_hull): o glacis raso com os tijolos, a saia com o Relikt e a aba de borracha,
    a grade de barras atrás e na traseira, as prateleiras com as caixas e os tanques, as venezianas, a lona enrolada no motor;
  - a torre soldada de chapas, o bustle comprido atrás, a cunha do Relikt nas bochechas, o mantelete com o fole, a mira do atirador
    (a caixa da Sosna-U, esquerda), a panorâmica do comandante e a estação remota com a Kord (direita), as escotilhas, os 2 bancos
    de lança-fumaça, o mastro do sensor de vento e as antenas; a estrela vermelha contornada de branco na lateral;
  - o canhão 2A46M-5 de 125 mm: a luva, a capa térmica com as braçadeiras, o evacuador abaulado, a boca com o furo e a referência.

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar delas):
  SM_Russia_T90M_Hull     casco, saias, deck, lona enrolada, o anel da torre;
  SM_Russia_T90M_Turret   a torre (gira em Z em volta de (TX, 0)), a gaiola com a lona e a rede, as miras, a Kord;
  SM_Russia_T90M_Barrel   o canhão (recua em −X; a boca é o X máximo);
  SM_Russia_T90M_Wheels   a rodagem (o executor separa em wheel_*);
  SM_Russia_T90M_Track_l / _Track_r  as esteiras de elos (andam no jogo).
Slots: Paint, Metal, Team, Track, Lens, Cloth, Rubber, Red, White, Dark.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import T72, bounds, frame_box, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_t90m')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_T90M'

TX = T72.RING_X                  # o eixo da torre (0,35 à frente do meio do casco)
T_Z0, T_Z1, T_Z2 = 1.47, 1.92, 2.17
GUN_Z = 1.80
MANTLET_X = TX + 1.62
MUZZLE_X = 6.10                  # 2,65 m além do nariz (o T-90M: 9,53 m com o cano)
CAGE = dict(x0=TX - 2.45, x1=TX + 1.45, hw=1.4, z_rear=2.8, z_front=2.76, visor=0.7)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = rk.new_part(PREFIX + '_Hull')
    rk.t72_hull(p, era_glacis=True, era_skirt=True, rear_slats=True, tarp_roll=True)
    # o T-90M: as argolas de amarração nos cantos do deck e o cabo de reboque preso na traseira
    for s in (-1, 1):
        for x in (2.15, -3.25):
            p.torus('Metal#vp', (x, 1.1 * s, T72.DECK + 0.03), 'x', 0.05, 0.014, seg=12, rseg=6)
    vp.tow_cable(p, 'Metal', [(T72.REAR - 0.05, -0.85, 0.82), (T72.REAR - 0.07, -0.6, 0.72), (T72.REAR - 0.07, 0.6, 0.72),
                              (T72.REAR - 0.05, 0.85, 0.82)], r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_points():
    pts = []
    for s in (-1, 1):
        pts += [(TX + 1.30, 0.50 * s, T_Z0), (TX + 0.85, 1.22 * s, T_Z0), (TX - 0.95, 1.25 * s, T_Z0), (TX - 1.2, 1.05 * s, T_Z0)]
        pts += [(TX + 1.32, 0.52 * s, T_Z1), (TX + 0.86, 1.26 * s, T_Z1), (TX - 0.95, 1.27 * s, T_Z1), (TX - 1.2, 1.07 * s, T_Z1)]
        pts += [(TX + 1.05, 0.45 * s, T_Z2), (TX + 0.72, 1.1 * s, T_Z2), (TX - 0.92, 1.13 * s, T_Z2), (TX - 1.12, 0.96 * s, T_Z2)]
    return pts


def wedge(p, s):
    """A cunha do Relikt na bochecha (lado s): o bloco em diagonal (o V de cima), com os tijolos em relevo na face da frente (3 × 4)
    e no topo (2 × 4)."""
    FI, FO = Vector((TX + 1.80, 0.55 * s, 0)), Vector((TX + 0.92, 1.45 * s, 0))
    BO = Vector((TX + 0.60, 1.14 * s, 0))
    BI = FI + (BO - FO)                       # o paralelogramo (o topo plano)
    zb, zf, zt = 1.52, 1.98, 2.1
    pts = []
    for q, ztop in ((FI, zf), (FO, zf), (BO, zt), (BI, zt)):
        pts += [(q.x, q.y, zb), (q.x, q.y, ztop)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    d = (FO - FI).normalized()
    n = Vector((d.y, -d.x, 0.0)) * s * -1.0
    if n.x < 0:
        n = -n
    L = (FO - FI).length
    # a face da frente (em pé): 3 fileiras × 4 tijolos
    rk.era_grid(p, FI + Vector((0, 0, zb + 0.03)) + n * 0.0, d, Vector((0, 0, 1)), n, 4, 3, (L - 0.04) / 4, (zf - zb - 0.06) / 3,
                0.07)
    # o topo (inclinado de zf na frente a zt atrás): 2 fileiras × 4
    back = (BO - FO)
    depth = back.length
    up = (Vector((back.x, back.y, zt - zf))).normalized()
    nt = d.cross(up)
    if nt.z < 0:
        nt = -nt
    rk.era_grid(p, FI + Vector((0, 0, zf)) + up * 0.04, d, up, nt, 4, 2, (L - 0.04) / 4, (depth - 0.08) / 2, 0.06)


def turret():
    p = rk.new_part(PREFIX + '_Turret')
    solid(p, 'Paint#shell', turret_points())
    vp.lathe(p, 'Paint', (TX, 0.0, T72.DECK), 'z', [(0.0, 0.03), (1.0, 0.03), (1.0, 0.06), (0.96, 0.07), (0.0, 0.07)], seg=56)
    # o bustle (a caixa de munição atrás, um pouco mais baixa que o teto, a traseira inclinada)
    pts = []
    for s in (-1, 1):
        pts += [(TX - 1.0, 1.0 * s, 1.62), (TX - 1.0, 1.02 * s, 2.12), (TX - 2.18, 0.98 * s, 2.12), (TX - 2.3, 0.95 * s, 1.95),
                (TX - 2.2, 0.92 * s, 1.66)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    for s in (-1, 1):
        # as tampas do bustle (a fresta e os trincos)
        vp.panel_seam(p, 'Dark', (TX - 1.55, 1.005 * s, 1.68), (TX - 1.55, 1.005 * s, 2.08), (0, s, 0), w=0.024, d=0.01)
        for x in (TX - 1.3, TX - 1.85):
            vp.rbox(p, 'Metal', (x, 1.02 * s, 1.98), (0.06, 0.03, 0.08), r=0.01, k=1)
        # as caixas de ferramenta penduradas na lateral do bustle
        vp.rbox(p, 'Paint#plate', (TX - 1.6, 1.16 * s, 1.78), (0.9, 0.26, 0.3), r=0.04, k=3)
        vp.rbox(p, 'Paint#plate', (TX - 1.6, 1.16 * s, 1.945), (0.94, 0.29, 0.03), r=0.045, k=2)
    # a cunha do Relikt nas 2 bochechas
    for s in (-1, 1):
        wedge(p, s)
    # o mantelete com o fole de lona e a metralhadora coaxial (direita do cano)
    vp.rbox(p, 'Paint#plate', (TX + 1.44, 0.0, GUN_Z + 0.02), (0.36, 0.62, 0.46), r=0.08, k=3)
    vp.bellows(p, 'Cloth', MANTLET_X - 0.04, MANTLET_X + 0.22, 0.2, 0.15, GUN_Z, folds=4, depth=0.02, seg=28)
    vp.lathe(p, 'Dark', (MANTLET_X, 0.24, GUN_Z + 0.06), 'x', [(0.0, 0.0), (0.04, 0.0), (0.04, 0.03), (0.0, 0.03)], seg=12)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    # a mira do atirador (a Sosna-U: a caixa blindada com as portinholas, esquerda na frente)
    gx, gy = TX + 0.62, -0.62
    vp.rbox(p, 'Paint#plate', (gx, gy, top + 0.17), (0.46, 0.42, 0.34), r=0.06, k=3)
    vp.rbox(p, 'Paint#thin', (gx - 0.02, gy, top + 0.35), (0.5, 0.46, 0.03), r=0.06, k=2)
    vp.rbox(p, 'Dark', (gx + 0.23, gy, top + 0.2), (0.02, 0.32, 0.18), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.24, gy - 0.06, top + 0.2), (0.012, 0.14, 0.12), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.24, gy + 0.1, top + 0.2), (0.012, 0.09, 0.09), r=0.03, k=2)
    for sg in (-1, 1):
        p.obox('Paint#thin', (gx + 0.26, gy + sg * 0.22, top + 0.2), (0.14, 0.025, 0.2), yaw=sg * 35, bevel=0.0)
    # a panorâmica do comandante (o pedestal e a cabeça giratória, direita na frente)
    cx, cy = TX + 0.5, 0.66
    vp.lathe(p, 'Paint', (cx, cy, top), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.13, 0.08), (0.11, 0.24), (0.0, 0.24)], seg=18)
    vp.rbox(p, 'Paint#plate', (cx, cy, top + 0.35), (0.34, 0.3, 0.22), r=0.06, k=3)
    vp.rbox(p, 'Lens', (cx + 0.17, cy, top + 0.36), (0.012, 0.2, 0.11), r=0.03, k=2)
    # a cúpula do comandante (direita) com a escotilha e a Kord por cima (a estação remota do T-90M)
    kx, ky = TX - 0.18, 0.56
    vp.lathe(p, 'Paint', (kx, ky, top), 'z', [(0.0, 0.0), (0.42, 0.0), (0.42, 0.06), (0.39, 0.1), (0.0, 0.1)], seg=28)
    vp.hatch_round(p, (kx - 0.08, ky - 0.08, top + 0.06), 0.24, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    rk.kord_rws(p, (kx + 0.05, ky + 0.36, top + 0.1), yaw=0.0, k=1.0)
    # a escotilha do atirador (esquerda) e os periscópios
    vp.hatch_round(p, (TX - 0.28, -0.56, top), 0.28, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    for (x, y, yaw) in ((TX + 0.12, -0.32, 0), (TX + 0.08, 0.1, 15)):
        vp.rbox(p, 'Paint#plate', (x, y, top + 0.05), (0.16, 0.22, 0.1), yaw=yaw, r=0.03, k=2)
        p.obox('Lens', (x + 0.08, y, top + 0.06), (0.012, 0.16, 0.04), yaw=yaw, bevel=0.0)
    # os 2 bancos de lança-fumaça nas bochechas, de lado (o 902: 6 tubos num arco, apontando pra frente e pra fora)
    for s in (-1, 1):
        rk.smoke_bank(p, (TX + 0.22, 1.38 * s, 2.0), yaw=60 * s, n=6, spread=0.12, pitch=22.0)
    # o mastro do sensor de vento (atrás, esquerda) e as 2 antenas
    vp.lathe(p, 'Paint', (TX - 0.95, -0.82, top), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.04), (0.03, 0.06), (0.025, 0.5),
                                                       (0.05, 0.52), (0.05, 0.62), (0.0, 0.63)], seg=12)
    for (ax, ay, hh) in ((TX - 1.0, 0.92, 1.7), (TX - 1.05, -0.4, 1.4)):
        vp.lathe(p, 'Dark', (ax, ay, top), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Dark#vp', (ax, ay, top + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    # as alças de mão e os olhais de içar, as juntas das chapas do teto
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (TX - 0.75, 1.27 * s, 2.05), (TX - 0.35, 1.27 * s, 2.05), (0, s, 0), height=0.07, r=0.015)
        p.torus('Metal#vp', (TX + 0.6, 1.0 * s, top + 0.035), 'x', 0.05, 0.016, seg=12, rseg=6)
    vp.panel_seam(p, 'Dark', (TX + 0.35, -1.08, top + 0.003), (TX + 0.35, 1.08, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    vp.panel_seam(p, 'Dark', (TX - 0.75, -1.1, top + 0.003), (TX - 0.75, 1.1, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    # a estrela contornada na lateral da torre (as 2), entre a bochecha e o bustle
    for s in (-1, 1):
        rk.star_outlined(p, (TX - 0.55, 1.27 * s + 0.004 * s, 1.72), (0, s, 0), r=0.15)
    # ---------------------------------------------------------------- a GAIOLA (a assinatura)
    c = CAGE
    posts = [(TX + 0.66, 1.0, top), (TX + 0.66, -1.0, top), (TX - 0.85, 1.1, top), (TX - 0.85, -1.1, top),
             (TX - 2.12, 0.92, 2.12), (TX - 2.12, -0.92, 2.12)]
    roof = rk.slat_cage(p, c['x0'], c['x1'], c['hw'], c['z_rear'], c['z_front'], posts, visor=c['visor'], step=0.24)
    step = (c['x1'] - c['x0']) / max(2, int(round((c['x1'] - c['x0']) / 0.24)))
    # a lona da cor do jogador na parte de trás (esticada nas ripas, cedendo entre elas) caindo pela traseira e pela direita
    rk.tarp_on_slats(p, c['x0'] - 0.05, TX - 1.55, 0.18, c['hw'] + 0.06, roof, step, c['x0'], m='Team', seed=2.0,
                     drape=[('rear', 0.4), ('right', 0.28)])
    # a frente da lona ENROLADA (dobrada pra trás: o rolo atravessado amarrado nas ripas)
    vp.tarp_roll(p, (TX - 1.55, (c['hw'] + 0.18) / 2 + 0.03, roof(TX - 1.55) + 0.1), 'y', c['hw'] - 0.12, 0.085, m='Team', straps=2,
                 strap_m='Cloth', buckle_m='Metal')
    vp.rope(p, 'Cloth', [(TX - 1.7, 0.18, roof(TX - 1.7) + 0.05), (TX - 1.8, -0.62, roof(TX - 1.8) + 0.01),
                         (TX - 1.9, -c['hw'], roof(TX - 1.9) - 0.01)], r=0.012, sag=0.02)

    # a rede jogada por cima da frente, embolada (as bolas) e caindo pela borda da esquerda
    lumps = [(TX - 0.6, -0.55, 0.09, 0.32), (TX + 0.1, -0.95, 0.07, 0.28), (TX - 1.1, 0.05, 0.06, 0.3), (TX + 0.35, -0.2, 0.05, 0.25)]

    def zf(x, y):
        z = roof(min(x, c['x1'])) + 0.035
        ph = ((x - c['x0']) / step) % 1.0
        z -= 0.02 * math.sin(math.pi * ph)
        for (lx, ly, h, rr) in lumps:
            z += h * math.exp(-((x - lx) ** 2 + (y - ly) ** 2) / rr ** 2)
        if y < -c['hw']:
            z -= min(0.3, (-c['hw'] - y) * 1.4)
        return z
    rk.net_drape(p, TX - 1.42, TX + 0.4, -1.62, 0.3, zf, step=0.22, r=0.017, jitter=0.06, seed=3.0)
    return p.finish()


# ---------------------------------------------------------------------------------- canhão
def barrel():
    p = rk.new_part(PREFIX + '_Barrel')
    x0 = MANTLET_X
    # o 2A46M-5 por estações: a luva junto do mantelete, a capa térmica, o EVACUADOR abaulado no meio, a capa até a boca reforçada
    # com o furo de 125 mm
    st = [(x0 - 0.06, 0.16), (x0 + 0.08, 0.16), (x0 + 0.12, 0.13), (x0 + 1.1, 0.125), (x0 + 1.14, 0.12),
          (3.30, 0.12), (3.36, 0.15), (3.44, 0.165), (3.86, 0.165), (3.94, 0.15), (4.0, 0.118),
          (5.82, 0.11), (5.86, 0.112), (5.9, 0.112), (MUZZLE_X - 0.04, 0.112), (MUZZLE_X, 0.104)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.0625, bore_depth=0.4, bore_m='Dark')
    for xx, r in ((x0 + 0.62, 0.126), (3.0, 0.121), (4.5, 0.116), (5.2, 0.113)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.009, seg=32)
        vp.rbox(p, 'Metal', (xx, 0.0, GUN_Z - r - 0.013), (0.05, 0.05, 0.03), r=0.012, k=1)
    # as 2 juntas do evacuador (o anel de parafusos)
    for xx in (3.4, 3.9):
        vp.bolt_ring(p, 'Metal', (xx, 0.0, GUN_Z), 'x', 0.15, 10, r=0.014, h=0.012)
    # a referência da boca (o espelho na caixinha, em cima)
    vp.rbox(p, 'Metal', (MUZZLE_X - 0.24, 0.0, GUN_Z + 0.15), (0.13, 0.08, 0.06), r=0.015, k=2)
    p.box('Metal', MUZZLE_X - 0.27, MUZZLE_X - 0.21, -0.02, 0.02, GUN_Z + 0.1, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    rk.t72_wheels(p)
    return p.finish()


# ---------------------------------------------------------------------------------- exportação
def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l'), rk.t72_track(PREFIX + '_Track_r', 'r')
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (tx0, tx1), (ty0, ty1), (tz0, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), casco %.2f x %.2f m (x %.2f..%.2f), altura %.2f m (a gaiola), '
          'deck %.2f, teto da torre %.2f' % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, hx0, hx1, az1, T72.DECK, T_Z2))
    print('MODEL torre com a gaiola: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f' % (tx0, tx1, ty0, ty1, tz0, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, 'tracks_m',
          [round(2 * T72.TRACK_Y, 3), T72.TRACK_W])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')])


main()
