"""O sobrado (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md): a casa de 2 andares das vilas dos game shots.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_house_2story.py C:/Users/luisf/explosion/captures/game/models/civ_v3/house_2story

O que é (16_beauty_pass/b_gpt: o sobrado da esquerda com o tapete na sacada e a escada; 15_three_factions/b_gpt: o de baixo com as
duas caixas d'água e a parabólica; docs/concept/towns/_variacoes_novas.jpg, a fileira 6): o bloco de 2 andares (6,35 m até a laje), a
cinta de reboco no piso de cima, a SACADA de laje na frente com o guarda-corpo de ferro e o tapete pendurado, as janelas recuadas
dos 2 andares (as de baixo com grade), a porta de ferro com a marquise; o teto mais carregado: a casinha da escada, as 2 caixas
d'água (uma no cavalete alto), a parabólica, a antena de mastro, o varal, a condensadora, o ferro de espera nos 2 pilares da frente
(o 3º andar que nunca veio).

Saída: SM_civ_house_2story.glb (SUBSTITUI o civ_house_2story: a mesma pegada 8 × 8 m, a frente em +Y, origem no centro e no chão,
a saia até −0,4 m).

v3 (01/11 noite, "quase, muitas quinas, parece lego, sem detalhes, sem texturas"): o sobrado é a casa MAIS NOVA da vila (o reboco de
cimento, menos gasto: wear 0,65), pra não ser clone da térrea: a mureta lisa sem dentes (comida de leve), os pilares altos da frente
com o ferro de espera, mais remendos de reboco novo que de barro, poucas placas caídas; as mesmas janelas de casa velha (menos
quebradas), a cinta, a sacada; a parede fora de prumo pro outro lado; o escorrido e o pé úmido na cor de vértice.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
from model_building_parts import WallFace, ac_unit, cable  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'house_2story')
os.makedirs(OUT, exist_ok=True)

X0, X1, Y0, Y1 = -3.7, 3.7, -3.7, 3.7
ZF = 3.2                      # o piso do 2º andar
ZR = 6.35                     # o topo da laje do teto
T = 0.25


def railing(p, x0, x1, y_out, y_wall, z, h=1.0, m='Metal#rail'):
    """O guarda-corpo de ferro da sacada: os montantes, o corrimão, a travessa de baixo e as barras de pé (o ritmo que lê de longe)."""
    pts = [(x0, y_wall), (x0, y_out), (x1, y_out), (x1, y_wall)]
    vp.tube(p, m, vp.bent_bar([(x, y, z + h) for (x, y) in pts], R=0.06, n=2), 0.024, seg=8)
    vp.tube(p, m, [(x, y, z + 0.12) for (x, y) in pts], 0.016, seg=6)
    for (a, b) in zip(pts[:-1], pts[1:]):
        L = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
        n = max(2, int(L / 0.13))
        for k in range(n + 1):
            u = k / n
            x, y = a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u
            r = 0.024 if k in (0, n) else 0.011
            vp.tube(p, m, [(x, y, z + 0.02), (x, y, z + h)], r, seg=6)


def build():
    hp.FINE['u'] = 0.2                    # (v3, o orçamento: a casa grande) a estria do escorrido a cada 20 cm
    p = hp.new_part('SM_civ_house_2story', seed=2, dirt=0.8)
    holes = {
        '+y': [(-1.3, -0.3, 0.0, 2.15), (0.9, 2.1, 1.0, 2.2), (-1.45, -0.45, ZF + 0.05, ZF + 2.25), (0.9, 2.1, ZF + 1.0, ZF + 2.2),
               (-3.05, -2.35, ZF + 1.1, ZF + 2.0)],
        '+x': [(-2.0, -0.9, 1.1, 2.1), (0.9, 2.0, 1.1, 2.1), (-2.0, -0.9, ZF + 1.05, ZF + 2.15), (0.9, 2.0, ZF + 1.05, ZF + 2.15)],
        '-x': [(-0.5, 0.5, 1.2, 2.05), (-1.8, -0.8, ZF + 1.05, ZF + 2.1), (0.8, 1.8, ZF + 1.05, ZF + 2.1)],
        '-y': [(1.4, 2.1, 1.5, 2.1), (-2.0, -1.0, ZF + 1.1, ZF + 2.1), (1.0, 2.0, ZF + 1.1, ZF + 2.1)],
    }
    hp.house_block(p, X0, X1, Y0, Y1, ZR, t=T, holes=holes, floors=(ZF,), seed=2, patches=4, spalls=1.5,
                   cracks=1, wear=0.65, mud=0.15, pocks=6)
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        for k, (u0, u1, z0, z1) in enumerate(hs):
            if z0 < 0.1 or (f == '+y' and k == 2):
                continue
            kind = 'grille' if z0 < ZF else ('glass' if (k + len(f)) % 3 else 'dark')
            hp.window(p, F, u0, u1, z0, z1, t=T, kind=kind, seed=k + 11 * len(f) + ord(f[1]), shutters=(f == '+x' and z0 > ZF and k == 3),
                      wear=0.65)
    Ff = WallFace('+y', Y1, 0.0)
    hp.steel_door(p, Ff, -1.3, -0.3, 0.0, 2.15, t=T)
    hp.steel_door(p, Ff, -1.45, -0.45, ZF + 0.05, ZF + 2.25, t=T, canopy=False, step=False, m='Wood#door', double=True)
    # a sacada: a laje saindo da cinta (com a pingadeira embaixo), o guarda-corpo de ferro, o tapete pendurado nele
    p.box('Plaster#cap', -2.35, 0.55, Y1 - 0.05, Y1 + 1.05, ZF - 0.14, ZF + 0.04)
    p.box('Plaster#band', -2.3, 0.5, Y1, Y1 + 1.0, ZF - 0.2, ZF - 0.13)
    for x in (-2.15, 0.35):                                          # as mãos-francesas de concreto
        hp.hexa(p, 'Plaster#trim', [(x - 0.08, Y1, ZF - 0.75), (x + 0.08, Y1, ZF - 0.75), (x - 0.08, Y1 + 0.05, ZF - 0.75),
                                    (x + 0.08, Y1 + 0.05, ZF - 0.75), (x - 0.08, Y1, ZF - 0.14), (x + 0.08, Y1, ZF - 0.14),
                                    (x - 0.08, Y1 + 0.75, ZF - 0.14), (x + 0.08, Y1 + 0.75, ZF - 0.14)])
    railing(p, -2.3, 0.5, Y1 + 0.98, Y1 + 0.02, ZF + 0.04)
    hp.rug_over(p, (-2.05, Y1 + 1.0, 0.0), (-0.75, Y1 + 1.0, 0.0), ZF + 1.05, (0, 1, 0), drop=1.35, depth=0.2, m='Rust#rug', seed=5)
    # a vida na parede: o relógio de luz, o fio, os ar-condicionados de janela
    vp.rbox(p, 'Metal#meter', (-2.65, Y1 + 0.08, 2.45), (0.34, 0.14, 0.42), r=0.03, k=2)
    vp.tube(p, 'Dark#cable', vp.bent_bar([(-2.65, Y1 + 0.06, 2.66), (-2.65, Y1 + 0.06, ZF - 0.25), (-3.3, Y1 + 0.06, ZF - 0.25),
                                          (-3.3, Y1 + 0.06, ZR + 0.3)], R=0.1, n=3), 0.014, seg=6)
    ac_unit(p, WallFace('+x', X1, 0.0), -1.45, ZF + 0.55, m='Metal#ac', bracket='Metal#acb')
    ac_unit(p, WallFace('-y', Y0, 0.0), -1.5, ZF + 0.6, m='Metal#ac', bracket='Metal#acb')
    # o parapeito; o ferro de espera nos 2 pilares da frente
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.62, t=0.22, seed=7, rebar_at=[(X1 - 0.21, Y1 - 0.21), (X0 + 0.21, Y1 - 0.21)], teeth=False,
               wear=0.55, pier_h=0.45)
    for (f, u) in (('+x', 2.6), ('-y', 2.9), ('-x', -2.4)):
        hp.spout(p, WallFace(f, at[f], 0.0), u, ZR + 0.08)
    # o teto
    hp.stair_box(p, -3.48, -1.38, -3.48, -1.28, ZR, h=2.2, door='+x')
    hp.water_tank(p, (1.85, -2.35, ZR), r=0.55, h=1.15, stand=1.35, seed=2)
    hp.water_tank(p, (0.35, -2.55, ZR), r=0.48, h=1.0, stand=0.0, seed=3)
    hp.sat_dish(p, (2.75, 2.5, ZR), r=0.45, yaw=200.0, tilt=34.0)
    hp.antenna(p, -2.8, 2.6, ZR, h=2.4)
    hp.laundry(p, (-0.9, 0.9), (2.2, 0.9), ZR, seed=6)
    hp.roof_ac(p, (-0.6, -0.6, ZR), yaw=90.0)
    hp.roof_junk(p, 0.4, 2.4, -0.4, 0.4, ZR, n=3, seed=21)
    # a escada de mão do quintal até a sacada (encostada na lateral −X)
    hp.ladder(p, (X0 - 0.75, 1.6, 0.0), (X0 - 0.06, 1.6, ZF + 0.9))
    o = hp.finish(p)
    hp.out_of_plumb(o, X0, X1, Y0, Y1, lean=(-0.006, 0.008), twist=-0.003, bow=(-0.012, 0.016))
    return o


clear_scene()
o = build()
export(o, OUT)
