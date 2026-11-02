"""O BMPT "Terminator" da Rússia (o blindado contra infantaria) provisório, modelado no Blender por script (01/11/2026;
docs/FACCAO_RUSSIA.md §5.2: "o chassi de tanque com os 2 canhões de 30 mm e os mísseis").

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_russia_bmpt.py -- ...
  (pelo model_run_detail.py: vehicle Game/Tools/model_russia_bmpt.py C:/Users/luisf/explosion/captures/game/models/russia_bmpt)

A base é o BMPT-72 de verdade, na escala de verdade: o casco do T-72/T-90 do kit (o glacis e a saia com o Relikt, a grade de barras
atrás), a torre baixa e larga com o MÓDULO DOS 2 CANHÕES 2A42 de 30 mm por cima (os canos gêmeos juntos, com as luvas e o freio de boca
furado), as 2 CAIXAS BLINDADAS do Ataka nos ombros (2 tubos de míssil em cada, as tampas redondas na frente), a mira do atirador na
frente e a panorâmica do comandante no pedestal alto atrás; a gaiola de ripas por cima com a lona e a rede da cor do jogador.
A palavra: os CANOS GÊMEOS E OS OMBROS. De cima: os 2 canos finos juntos saindo do módulo do meio, as caixas compridas dos mísseis dos
2 lados da torre (mais larga que a do T-90M), a gaiola por cima da parte de trás.

Peças (origem no chão, no centro do casco, frente em +X):
  SM_Russia_BMPT_Hull     casco, saias, deck, lona enrolada, o anel da torre;
  SM_Russia_BMPT_Turret   a torre (gira em Z em volta de (TX, 0)), o módulo dos canhões, as caixas do Ataka, as miras, a gaiola;
  SM_Russia_BMPT_Barrel   os 2 canos de 30 mm (recuam em −X; as bocas são o X máximo);
  SM_Russia_BMPT_Wheels   a rodagem (o executor separa em wheel_*);
  SM_Russia_BMPT_Track_l / _Track_r  as esteiras.
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

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_bmpt')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_BMPT'

TX = T72.RING_X
T_Z0, T_Z1, T_Z2 = 1.47, 1.82, 1.98       # a torre baixa
GUN_Z = 2.3                                # o eixo dos 2 canhões (no módulo de cima)
GUN_Y = 0.16                               # cada cano a ±0,16 do meio
MOD_X = (TX - 0.55, TX + 1.2)              # o módulo dos canhões (a caixa blindada de cima)
MUZZLE_X = TX + 3.75                       # 2,4 m de cano pra fora do módulo + o freio
ATAKA_Y = 1.42                             # o centro das caixas do Ataka
CAGE = dict(x0=TX - 1.9, x1=TX + 0.7, hw=1.25, z_rear=2.86, z_front=2.84, visor=0.35)


def hull():
    p = rk.new_part(PREFIX + '_Hull')
    rk.t72_hull(p, era_glacis=True, era_skirt=True, rear_slats=True, tarp_roll=True)
    for s in (-1, 1):
        for x in (2.15, -3.25):
            p.torus('Metal#vp', (x, 1.1 * s, T72.DECK + 0.03), 'x', 0.05, 0.014, seg=12, rseg=6)
    vp.tow_cable(p, 'Metal', [(T72.REAR - 0.05, -0.85, 0.82), (T72.REAR - 0.07, -0.6, 0.72), (T72.REAR - 0.07, 0.6, 0.72),
                              (T72.REAR - 0.05, 0.85, 0.82)], r=0.022)
    return p.finish()


def turret():
    p = rk.new_part(PREFIX + '_Turret')
    # a torre: o casco baixo e largo, a frente em cunha rasa
    pts = []
    for s in (-1, 1):
        pts += [(TX + 1.35, 0.6 * s, T_Z0), (TX + 0.9, 1.18 * s, T_Z0), (TX - 1.3, 1.18 * s, T_Z0), (TX - 1.55, 0.95 * s, T_Z0)]
        pts += [(TX + 1.3, 0.62 * s, T_Z1), (TX + 0.86, 1.2 * s, T_Z1), (TX - 1.3, 1.2 * s, T_Z1), (TX - 1.55, 0.97 * s, T_Z1)]
        pts += [(TX + 1.0, 0.55 * s, T_Z2), (TX + 0.7, 1.08 * s, T_Z2), (TX - 1.25, 1.1 * s, T_Z2), (TX - 1.48, 0.9 * s, T_Z2)]
    solid(p, 'Paint#shell', pts)
    vp.lathe(p, 'Paint', (TX, 0.0, T72.DECK), 'z', [(0.0, 0.03), (1.0, 0.03), (1.0, 0.06), (0.96, 0.07), (0.0, 0.07)], seg=56)
    # os tijolos do Relikt na frente da torre (2 fileiras na cunha)
    for s in (-1, 1):
        a, b = Vector((TX + 1.33, 0.62 * s, 0)), Vector((TX + 0.88, 1.19 * s, 0))
        d = (b - a).normalized()
        n = Vector((d.y, -d.x, 0.0))
        if n.x < 0:
            n = -n
        rk.era_grid(p, a + Vector((0, 0, T_Z0 + 0.03)), d, Vector((0, 0, 1)), n, 3, 2, (b - a).length / 3, (T_Z1 - T_Z0 - 0.06) / 2, 0.06)
    # ---------------------------------------------------------------- o módulo dos 2 canhões (a caixa blindada em cima, no meio)
    mx0, mx1 = MOD_X
    pts = []
    for s in (-1, 1):
        pts += [(mx0, 0.46 * s, T_Z2), (mx0, 0.44 * s, 2.5), (mx1 - 0.25, 0.42 * s, 2.52), (mx1, 0.4 * s, 2.4), (mx1, 0.42 * s, 2.1),
                (mx1 - 0.3, 0.46 * s, T_Z2)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    # as frestas das tampas do módulo e as alças
    for x in (mx0 + 0.55, mx0 + 1.1):
        vp.panel_seam(p, 'Dark', (x, -0.42, 2.51), (x, 0.42, 2.51), (0, 0, 1), w=0.024, d=0.01)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (mx0 + 0.2, 0.46 * s, 2.3), (mx0 + 0.6, 0.46 * s, 2.3), (0, s, 0), height=0.06, r=0.014)
    # o mantelete dos 2 canos (o bloco da frente com as 2 luvas) e a metralhadora coaxial (entre e embaixo)
    vp.rbox(p, 'Paint#plate', (mx1 + 0.08, 0.0, GUN_Z), (0.2, 0.62, 0.34), r=0.07, k=3)
    for s in (-1, 1):
        vp.lathe(p, 'Paint', (mx1 + 0.16, GUN_Y * s, GUN_Z), 'x', [(0.0, 0.0), (0.085, 0.0), (0.085, 0.18), (0.07, 0.2), (0.0, 0.2)], seg=20)
    vp.lathe(p, 'Dark', (mx1 + 0.18, 0.0, GUN_Z - 0.12), 'x', [(0.0, 0.0), (0.03, 0.0), (0.03, 0.06), (0.0, 0.06)], seg=10)
    # a mira do atirador (a caixa blindada na frente-esquerda, com as portinholas) e a do comandante no pedestal alto (atrás, direita)
    gx, gy = TX + 0.62, -0.78
    vp.rbox(p, 'Paint#plate', (gx, gy, T_Z2 + 0.2), (0.48, 0.42, 0.4), r=0.06, k=3)
    vp.rbox(p, 'Dark', (gx + 0.24, gy, T_Z2 + 0.24), (0.02, 0.34, 0.22), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.25, gy - 0.06, T_Z2 + 0.24), (0.012, 0.15, 0.13), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.25, gy + 0.1, T_Z2 + 0.24), (0.012, 0.09, 0.09), r=0.03, k=2)
    cx, cy = TX - 0.95, 0.68
    vp.lathe(p, 'Paint', (cx, cy, T_Z2), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.12, 0.08), (0.1, 0.45), (0.13, 0.48),
                                              (0.0, 0.48)], seg=16)
    vp.rbox(p, 'Paint#plate', (cx, cy, T_Z2 + 0.6), (0.4, 0.34, 0.26), r=0.06, k=3)
    vp.rbox(p, 'Lens', (cx + 0.2, cy, T_Z2 + 0.6), (0.012, 0.24, 0.13), r=0.03, k=2)
    # as escotilhas (comandante à direita, atirador à esquerda, atrás do módulo) e os periscópios
    vp.hatch_round(p, (TX - 0.45, 0.72, T_Z2), 0.27, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.hatch_round(p, (TX - 0.5, -0.72, T_Z2), 0.27, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # ---------------------------------------------------------------- as caixas blindadas do Ataka nos ombros (2 tubos em cada)
    for s in (-1, 1):
        y = ATAKA_Y * s
        x0, x1 = TX - 0.95, TX + 1.05
        z0, z1 = 1.64, 2.2
        pts = []
        for xx, zz0, zz1 in ((x0, z0, z1), (x1 - 0.12, z0, z1), (x1, z0 + 0.04, z1 - 0.04)):
            for yy in (y - 0.24, y + 0.24):
                pts += [(xx, yy, zz0), (xx, yy, zz1)]
        solid(p, 'Paint#shell', pts, bevel=0.04)
        # as tampas redondas dos 2 tubos na frente (a da boca, com o aro) e a chapa de trás
        for zz in (z0 + 0.15, z1 - 0.15):
            vp.lathe(p, 'Paint', (x1, y, zz), 'x', [(0.0, 0.03), (0.11, 0.03), (0.13, 0.01), (0.13, -0.01), (0.0, -0.01)], seg=18)
            vp.lathe(p, 'Metal', (x1 + 0.03, y, zz), 'x', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.015), (0.0, 0.015)], seg=12)
        # o braço que levanta a caixa (o suporte na lateral da torre) e as frestas
        vp.rbox(p, 'Paint#plate', (TX + 0.05, (1.17 + 0.03) * s, 1.86), (0.5, 0.12, 0.3), r=0.04, k=2)
        p.cyl('Metal#vp', (TX + 0.05, (1.17 + 0.08) * s, 1.9), 'y', 0.09, 0.14, seg=14, bevel=0.0)
        for x in (x0 + 0.65, x0 + 1.3):
            vp.panel_seam(p, 'Dark', (x, y - 0.24, z1 + 0.002), (x, y + 0.24, z1 + 0.002), (0, 0, 1), w=0.022, d=0.01)
        vp.bolt_row(p, 'Metal', (x0 + 0.1, y + 0.245 * s, z1 - 0.06), (x1 - 0.2, y + 0.245 * s, z1 - 0.06), 8, (0, s, 0), r=0.016, h=0.012)
        # a estrela contornada na lateral da caixa
        rk.star_outlined(p, (TX - 0.35, (ATAKA_Y + 0.244) * s, 1.92), (0, s, 0), r=0.14)
        # o banco de lança-fumaça atrás da caixa
        rk.smoke_bank(p, (TX - 1.15, 1.25 * s, 1.92), yaw=80 * s, n=5, spread=0.12, pitch=22.0)
    # o mastro do vento e as antenas atrás
    vp.lathe(p, 'Paint', (TX - 1.25, -0.6, T_Z2), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.04), (0.03, 0.06), (0.025, 0.5),
                                                       (0.05, 0.52), (0.05, 0.62), (0.0, 0.63)], seg=12)
    for (ax, ay, hh) in ((TX - 1.3, 0.25, 1.6), (TX - 1.35, -0.2, 1.3)):
        vp.lathe(p, 'Dark', (ax, ay, T_Z2), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Dark#vp', (ax, ay, T_Z2 + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    # a caixa de carga atrás (o cesto de tubo com a lona dobrada)
    vp.rbox(p, 'Paint#plate', (TX - 1.75, 0.0, 1.78), (0.42, 1.7, 0.4), r=0.05, k=3)
    # ---------------------------------------------------------------- a GAIOLA por cima da parte de trás da torre (deixa o módulo dos
    # canhões livre na frente), a lona dobrada na esquerda e a rede na direita
    c = CAGE
    posts = [(TX + 0.55, 1.0, T_Z2), (TX + 0.55, -1.0, T_Z2), (TX - 1.2, 1.05, T_Z2), (TX - 1.2, -1.05, T_Z2)]
    roof = rk.slat_cage(p, c['x0'], c['x1'], c['hw'], c['z_rear'], c['z_front'], posts, visor=c['visor'], step=0.24)
    step = (c['x1'] - c['x0']) / max(2, int(round((c['x1'] - c['x0']) / 0.24)))
    rk.tarp_on_slats(p, c['x0'] - 0.05, TX - 0.8, -c['hw'] - 0.06, -0.15, roof, step, c['x0'], m='Team', seed=4.0,
                     drape=[('rear', 0.35), ('left', 0.26)])
    vp.tarp_roll(p, (TX - 0.8, (-c['hw'] - 0.15) / 2, roof(TX - 0.8) + 0.1), 'y', c['hw'] - 0.05, 0.085, m='Team', straps=2,
                 strap_m='Cloth', buckle_m='Metal')

    def zf(x, y):
        z = roof(min(x, c['x1'])) + 0.035 + 0.07 * math.exp(-((x - TX + 0.6) ** 2 + (y - 0.6) ** 2) / 0.1)
        z -= 0.02 * math.sin(math.pi * (((x - c['x0']) / step) % 1.0))
        if y > c['hw']:
            z -= min(0.3, (y - c['hw']) * 1.4)
        return z
    rk.net_drape(p, TX - 1.15, TX + 0.45, 0.0, c['hw'] + 0.22, zf, step=0.22, r=0.017, jitter=0.06, seed=6.0)
    return p.finish()


def barrel():
    """Os 2 canos de 30 mm (2A42): a luva, o cano fino com o anel do meio e o freio de boca furado; recuam juntos."""
    p = rk.new_part(PREFIX + '_Barrel')
    x0 = MOD_X[1] + 0.34
    for s in (-1, 1):
        y = GUN_Y * s
        st = [(x0 - 0.04, 0.06), (x0 + 0.3, 0.06), (x0 + 0.34, 0.048), (x0 + 1.3, 0.045), (x0 + 1.34, 0.05), (x0 + 1.4, 0.05),
              (x0 + 1.44, 0.043), (MUZZLE_X - 0.24, 0.04), (MUZZLE_X - 0.22, 0.055), (MUZZLE_X, 0.055)]
        vp.gun_barrel(p, st, GUN_Z, y=y, m='Paint', seg=16, bore=0.02, bore_depth=0.2, bore_m='Dark')
        for k in range(3):                                                    # as janelas do freio de boca
            xx = MUZZLE_X - 0.19 + k * 0.06
            for d in (-1, 1):
                p.box('Dark#vp', xx - 0.015, xx + 0.015, y + d * 0.05, y + d * 0.058, GUN_Z - 0.025, GUN_Z + 0.025, bevel=0.0)
    return p.finish()


def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    rk.t72_wheels(p)
    return p.finish()


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
    print('MODEL medidas: comprimento %.2f m com os canos (x %.2f..%.2f), casco %.2f x %.2f m, altura %.2f m (a gaiola %.2f)'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, az1, CAGE['z_rear']))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'bocas [x, y, z]', [[MUZZLE_X, -GUN_Y, GUN_Z], [MUZZLE_X, GUN_Y, GUN_Z]],
          'tracks_m', [round(2 * T72.TRACK_Y, 3), T72.TRACK_W])
    print('MODEL Ataka: as 4 bocas', [[TX + 1.08, ATAKA_Y * s, z] for s in (-1, 1) for z in (1.79, 2.05)])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')])


main()
