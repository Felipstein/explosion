"""A casa com a varanda (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_house_porch.py C:/Users/luisf/explosion/captures/game/models/civ_v3/house_porch

O que é (docs/concept/buildings/civil/civ_house_porch_gemini.webp, a ficha aprovada em 08/10; docs/concept/towns/casas, as casas com
a cobertura de chapa na frente; 16_beauty_pass/b_gpt, o toldo cor de terra): a casa térrea de 7,4 × 7,4 m no fundo do lote e, na
frente, a VARANDA de 4,2 m: o piso de laje com o degrau, os 4 esteios de madeira com a viga e os caibros, a cobertura de chapa ondulada
enferrujada caindo pra frente (com um pedaço de lona remendando), o banco de tábua, os potes de barro, as caixas e o tapete no
guarda-corpo. No teto da casa: a casinha da escada, a caixa d'água, a parabólica e o varal.

Saída: SM_civ_house_porch.glb (SUBSTITUI o civ_house_porch: a mesma pegada 8 × 12 m, a frente em +Y, origem no centro e no chão).

v3 (01/11 noite, "quase, muitas quinas, parece lego, sem detalhes, sem texturas"): a casa de adobe mais rústica (a ficha aprovada):
a mureta baixa de crista redonda e comida (sem dentes), as vergas de madeira em todas as janelas, muito barro nos remendos, o reboco
caído com o tijolo; o escorrido e o pé úmido na cor de vértice; a parede fora de prumo.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
from model_building_parts import WallFace, tarp  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'house_porch')
os.makedirs(OUT, exist_ok=True)

X0, X1, Y0, Y1 = -3.7, 3.7, -5.9, 1.5      # a casa no fundo
PY = 5.62                                   # a frente da varanda
ZR = 3.35
T = 0.25


def jar(p, x, y, h=0.62, r=0.24, m='Rust#clay'):
    """O pote de barro (a moringa): o perfil girado com a barriga, o gargalo e a boca virada."""
    vp.lathe(p, m, (x, y, 0.18), 'z', [(0.0, 0.0), (r * 0.55, 0.0), (r * 0.9, h * 0.2), (r, h * 0.45), (r * 0.85, h * 0.72),
                                        (r * 0.42, h * 0.88), (r * 0.38, h * 0.95), (r * 0.5, h), (r * 0.42, h + 0.01), (0.0, h - 0.05)],
             seg=18, smooth=True)


def build():
    p = hp.new_part('SM_civ_house_porch', seed=4, dirt=0.95)
    holes = {
        '+y': [(-0.5, 0.5, 0.0, 2.15), (-2.9, -1.7, 1.0, 2.15), (1.7, 2.9, 1.0, 2.15)],
        '+x': [(-3.6, -2.5, 1.1, 2.1), (-0.9, 0.2, 1.1, 2.1)],
        '-x': [(-2.8, -1.8, 1.2, 2.05)],
        '-y': [(-2.2, -1.2, 1.2, 2.05), (1.4, 2.1, 1.5, 2.1)],
    }
    hp.house_block(p, X0, X1, Y0, Y1, ZR, t=T, holes=holes, seed=4, spalls=4, cracks=2, wear=1.0, mud=0.6, pocks=8)
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for k, (u0, u1, z0, z1) in enumerate(hs):
            if z0 < 0.1:
                continue
            hp.window(p, F, u0, u1, z0, z1, t=T, kind='glass' if f == '+y' else ('grille' if k % 2 == 0 else 'dark'),
                      seed=k + 13 * len(f) + ord(f[1]), shutters=(f == '+y'), wear=1.0, lintel='wood')
    hp.steel_door(p, WallFace('+y', Y1, 0.0), -0.5, 0.5, 0.0, 2.15, t=T, canopy=False, step=False, m='Wood#door')
    # ---- a varanda: o piso de laje com o degrau, os esteios, a viga, os caibros, a cobertura de chapa (com o remendo de lona)
    p.box('Plaster#step', X0, X1, Y1, PY, -0.3, 0.18)
    p.box('Plaster#step', -1.0, 1.0, PY, PY + 0.32, -0.3, 0.09)
    for x in (-3.5, -1.2, 1.2, 3.5):
        vp.rbox(p, 'Wood#post', (x, PY - 0.12, 1.42), (0.15, 0.15, 2.5), r=0.03, k=2)
        vp.rbox(p, 'Plaster#step', (x, PY - 0.12, 0.25), (0.26, 0.26, 0.14), r=0.03, k=1)
    vp.rbox(p, 'Wood#beam', (0.0, PY - 0.12, 2.72), (7.5, 0.16, 0.2), r=0.03, k=2)
    vp.rbox(p, 'Wood#beam', (0.0, Y1 + 0.08, 2.98), (7.3, 0.12, 0.14), r=0.025, k=2)      # o sarrafo na parede
    for k in range(9):
        x = -3.5 + 7.0 * k / 8
        vp.tube(p, 'Wood#rafter', [(x, Y1 + 0.05, 3.02), (x, PY + 0.3, 2.78)], 0.045, seg=6)
    # a chapa ondulada (as ondas descendo pra frente), em 3 folhas desencontradas, e o remendo de lona
    for (x0, x1, dz) in ((-3.8, -1.1, 0.0), (-1.2, 1.5, 0.012), (1.4, 3.85, 0.0)):
        hp.corrugated(p, x0, x1, Y1 + 0.05, PY + 0.4, 3.08 + dz, 2.84 + dz, axis='y', m='Rust#sheet', pitch=0.09, amp=0.022)
    tarp(p, [(0.2, 2.6, 3.12), (1.9, 2.4, 3.12), (2.0, 4.6, 3.0), (0.3, 4.8, 3.0)], m='Cloth#patch', sag=0.0, folds=2, amp=0.02, thick=0.015,
         seed=3)
    # os pneus velhos segurando a lona e a chapa contra o vento (o jeito da região: lê de cima como os 2 anéis escuros)
    hp.loose_tyre(p, (0.75, 3.1, 3.16), R=0.3, w=0.18, axis='z')
    hp.loose_tyre(p, (1.45, 4.2, 3.10), R=0.28, w=0.17, axis='z')
    hp.loose_tyre(p, (-2.6, 4.6, 3.04), R=0.3, w=0.18, axis='z')
    # o banco de tábua, os potes, as caixas, o tapete jogado no esteio
    vp.rbox(p, 'Wood#bench', (-2.3, Y1 + 0.45, 0.62), (1.8, 0.42, 0.06), r=0.02, k=1)
    for x in (-3.05, -1.55):
        vp.rbox(p, 'Wood#bench', (x, Y1 + 0.45, 0.39), (0.06, 0.38, 0.42), r=0.01, k=1)
    jar(p, 2.4, Y1 + 0.5)
    jar(p, 2.95, Y1 + 0.45, h=0.5, r=0.2)
    jar(p, 2.65, Y1 + 0.95, h=0.45, r=0.19)
    for (x, y, z, s) in ((3.1, 4.6, 0.18, 0.55), (3.05, 4.55, 0.73, 0.45), (2.45, 4.9, 0.18, 0.5)):
        vp.rbox(p, 'Wood#crate', (x, y, z + s / 2), (s, s * 0.8, s), yaw=8 * x, r=0.02, k=1)
    hp.rug_over(p, (-3.42, PY - 0.2, 0.0), (-3.42, PY - 1.4, 0.0), 1.6, (-1, 0, 0), drop=0.9, depth=0.15, m='Rust#rug', seed=7)
    vp.rbox(p, 'Wood#rail', (-3.5, PY - 1.0, 1.6), (0.07, 2.0, 0.07), r=0.015, k=1)
    # ---- o teto da casa
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.50, t=0.24, seed=13, rebar_at=[(X0 + 0.21, Y0 + 0.21)], teeth=False, wear=1.2,
               pier_h=0.2)
    for (f, u) in (('+x', -4.6), ('-y', 2.9), ('-x', -1.0)):
        hp.spout(p, WallFace(f, at[f], 0.0), u, ZR + 0.08)
    hp.stair_box(p, 1.38, 3.48, -5.68, -3.48, ZR, h=2.2, door='-x')
    hp.water_tank(p, (-2.2, -4.4, ZR), r=0.55, h=1.15, stand=0.9, seed=7)
    hp.sat_dish(p, (-2.8, 0.7, ZR), r=0.42, yaw=200.0, tilt=34.0)
    hp.laundry(p, (-1.0, -1.0), (2.5, -1.6), ZR, seed=9)
    o = hp.finish(p)
    hp.out_of_plumb(o, X0, X1, Y0, Y1, lean=(-0.010, -0.009), twist=0.006, bow=(0.02, 0.018))
    return o


clear_scene()
o = build()
export(o, OUT)
