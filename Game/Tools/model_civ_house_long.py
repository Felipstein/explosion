"""A casa comprida com a loja (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md): a fileira de 2 unidades da rua da vila.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_house_long.py C:/Users/luisf/explosion/captures/game/models/civ_v3/house_long

O que é (docs/concept/towns/_variacoes_novas.jpg, a fileira 6 "loja com toldo de pano"; 16_beauty_pass/b_gpt embaixo à esquerda: o
toldo cor de terra sobre a porta com a escada de mão; town_battle_top_gemini: a casa da direita com as portas de enrolar): o bloco
térreo de 15,4 × 7,4 m, metade casa (a porta de ferro, as janelas com grade) e metade LOJA (as 2 portas de enrolar, uma meio aberta,
a placa em cima, o TOLDO de pano esticado nos paus até a calçada); no teto a mureta que divide as 2 casas, as caixas d'água, as
condensadoras da loja, a parabólica, o varal e a casinha da escada.

Saída: SM_civ_house_long.glb (SUBSTITUI o civ_house_long: a mesma pegada 16 × 8 m, a frente em +Y, origem no centro e no chão).

v3 (01/11 noite, "quase, muitas quinas, parece lego, sem detalhes, sem texturas"): a fileira velha da rua da feira: a mureta comida
com os dentes, a mureta do meio também comida, as vergas de madeira na fachada da casa, muito remendo de barro (as 2 casas de donos
diferentes), as placas de reboco caídas, as frestas; o escorrido debaixo das janelas e das bicas e o pé úmido na cor de vértice; a
parede fora de prumo.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
from model_building_parts import WallFace, cable  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'house_long')
os.makedirs(OUT, exist_ok=True)

X0, X1, Y0, Y1 = -7.7, 7.7, -3.7, 3.7
ZR = 3.55
T = 0.25


def build():
    hp.FINE['u'] = 0.2                    # (v3, o orçamento: a casa grande) a estria do escorrido a cada 20 cm
    p = hp.new_part('SM_civ_house_long', seed=3, dirt=0.95)
    holes = {
        '+y': [(-5.1, -4.1, 0.0, 2.15), (-7.0, -5.9, 1.05, 2.2), (-2.9, -1.7, 1.05, 2.2), (0.6, 3.3, 0.0, 2.6), (4.1, 6.8, 0.0, 2.6)],
        '+x': [(-1.6, -0.6, 1.2, 2.2), (0.8, 1.8, 1.2, 2.2)],
        '-x': [(-0.5, 0.5, 1.2, 2.1)],
        '-y': [(-6.0, -5.0, 1.3, 2.2), (-2.5, -1.8, 1.5, 2.1), (2.0, 3.0, 1.3, 2.2), (5.0, 6.0, 1.3, 2.2)],
    }
    hp.house_block(p, X0, X1, Y0, Y1, ZR, t=T, holes=holes, seed=3, spalls=3, cracks=1, wear=0.9, mud=0.55, pocks=10)
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for k, (u0, u1, z0, z1) in enumerate(hs):
            if z0 < 0.1:
                continue
            hp.window(p, F, u0, u1, z0, z1, t=T, kind='grille' if f == '+y' else ('glass' if k % 2 else 'dark'),
                      seed=k + 5 * len(f) + ord(f[1]), wear=0.9, lintel='wood' if f == '+y' else None)
    Ff = WallFace('+y', Y1, 0.0)
    hp.steel_door(p, Ff, -5.1, -4.1, 0.0, 2.15, t=T)
    # a loja: as 2 portas de enrolar (a da direita meio aberta: o escuro de dentro), a placa, o toldo de pano
    hp.rollup(p, Ff, 0.6, 3.3, 0.0, 2.6, t=T, open_frac=0.0)
    hp.rollup(p, Ff, 4.1, 6.8, 0.0, 2.6, t=T, open_frac=0.62)
    vp.rbox(p, 'Metal#sign', (3.7, Y1 + 0.06, 3.0), (6.6, 0.06, 0.5), r=0.03, k=2)
    for k in range(7):                                                   # as letras (o ritmo escuro da placa)
        vp.rbox(p, 'Dark#letter', (1.0 + k * 0.85, Y1 + 0.095, 3.0), (0.5, 0.012, 0.22), r=0.02, k=1)
    hp.awning(p, 0.4, 7.1, Y1 + 0.02, 2.3, 2.85, 2.45, m='Cloth#awn', seed=3, stripes='Rust#awn')
    hp.awning(p, -3.2, -1.4, Y1 + 0.02, 0.9, 2.55, 2.3, m='Rust#awn2', posts=False, seed=5, valance=False)
    # a vida: o relógio de luz, os fios, o ar-condicionado da loja na parede de trás
    vp.rbox(p, 'Metal#meter', (-0.4, Y1 + 0.08, 2.5), (0.34, 0.14, 0.42), r=0.03, k=2)
    vp.tube(p, 'Dark#cable', vp.bent_bar([(-0.4, Y1 + 0.06, 2.72), (-0.4, Y1 + 0.06, ZR + 0.3), (-0.4, Y1 - 0.05, ZR + 0.68)], R=0.1, n=3),
            0.014, seg=6)
    cable(p, (-0.3, Y1 + 0.06, 2.3), (0.55, Y1 + 0.06, 2.75), sag=0.05, r=0.012)
    # o parapeito, a mureta que divide as 2 casas no teto
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.62, t=0.22, seed=11, rebar_at=[(X0 + 0.21, Y1 - 0.21)], wear=0.9)
    # (v3) a mureta do meio, comida como a de fora (a face +x dela é o plano em x = 0,11)
    hp.mureta(p, WallFace('+x', 0.11, 0.0), Y0 + 0.2, Y1 - 0.2, ZR - 0.02, 0.52, 0.22, seed=37, wear=0.9)
    for (f, u) in (('+x', 2.6), ('-y', -5.5), ('-y', 4.5), ('-x', -2.4)):
        hp.spout(p, WallFace(f, at[f], 0.0), u, ZR + 0.08)
    # o teto da casa (esquerda). (01/11, KIT_CIVIL_V2 §6: a casinha da escada era a mesma em todas) aqui a escada de fora, de alvenaria,
    # encostada na parede de trás entre as janelas, subindo da direita pro teto da casa (como no 16_beauty_pass/b_gpt)
    hp.ext_stair(p, 1.6, -1.4, Y0, -1, ZR)
    hp.roof_junk(p, -7.3, -5.6, -3.3, -1.5, ZR, n=3, seed=6)
    hp.water_tank(p, (-2.2, -2.4, ZR), r=0.55, h=1.15, stand=0.9, seed=4)
    hp.sat_dish(p, (-1.0, 2.6, ZR), r=0.42, yaw=200.0, tilt=34.0)
    hp.laundry(p, (-5.0, 1.2), (-2.0, 1.4), ZR, seed=8)
    # o teto da loja (direita)
    hp.water_tank(p, (6.4, -2.4, ZR), r=0.5, h=1.05, stand=0.0, seed=5)
    hp.water_tank(p, (5.1, -2.5, ZR), r=0.5, h=1.05, stand=0.0, seed=6)
    hp.roof_ac(p, (2.0, -2.2, ZR), yaw=0.0)
    hp.roof_ac(p, (3.3, -2.2, ZR), yaw=0.0)
    hp.tank_horizontal(p, (2.8, 1.3, ZR), r=0.4, L=1.5, yaw=0.0, stand=0.5)
    hp.antenna(p, 6.9, 2.8, ZR, h=2.0)
    hp.roof_junk(p, 3.6, 5.6, 0.2, 2.4, ZR, n=3, seed=31)
    hp.roof_junk(p, -6.0, -3.0, -1.0, 0.4, ZR, n=3, seed=33)
    # a escada de mão encostada na fachada, do lado do toldo (como no 16_beauty_pass/b_gpt)
    hp.ladder(p, (0.05, Y1 + 0.85, 0.0), (0.05, Y1 + 0.06, ZR + 0.95))
    o = hp.finish(p)
    hp.out_of_plumb(o, X0, X1, Y0, Y1, lean=(0.007, 0.010), twist=0.004, bow=(0.015, 0.022))
    return o


clear_scene()
o = build()
export(o, OUT)
