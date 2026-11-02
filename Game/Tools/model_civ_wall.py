"""O muro de quintal e o muro com o portão (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_wall.py C:/Users/luisf/explosion/captures/game/models/civ_v3/wall

O que é (docs/concept/buildings/civil/civ_wall_gate_gpt.webp, a ficha aprovada: pilarete + trecho de 4 m + portão; os muros de
quintal de todas as vilas dos game shots, 16_beauty_pass/b_gpt embaixo à direita: o portão de chapa cinza entre os pilares): o
trecho de 4 m de bloco rebocado (26 cm, 2,2 m), a capa que sai dos 2 lados, o rodapé, o PILARETE numa ponta (mais alto, com a capa
própria: em fila a cada 4 m), a quina gasta, as lascas na capa e o furo de dreno no pé; o portão: os 2 pilares, as 2 folhas de ferro
(o quadro de tubo, a chapa ondulada de pé, as lanças em cima, a tranca e as dobradiças) com a fresta entre elas, os tocos de muro.

Saída: SM_civ_wall.glb e SM_civ_wall_gate.glb (SUBSTITUEM os do kit: a pegada 4 × 0,5 m do WorldVillage.cpp, ao longo de X, a origem
no centro e no chão, a saia até −0,4 m).

v3 (01/11 noite, "quase, muitas quinas, parece lego, sem detalhes, sem texturas"): o muro velho: o topo comido (as mordidas e os
blocos faltando, a capa em pedaços tortos: a mureta da casa, hp.mureta), o rodapé ondulando, o reboco caído com o tijolo nas 2
faces, o pilarete de quina redonda, o muro tombando de leve (mais numa ponta que na outra); o escorrido da capa e o pé úmido na cor
de vértice.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
import model_civ_parts as cp  # noqa: E402
from model_building_parts import WallFace, bake_in_place  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'wall')
os.makedirs(OUT, exist_ok=True)

H, T = 2.2, 0.26


def wall_run(p, x0, x1, seed, chips=True):
    """Um trecho de muro de x0 a x1: o corpo, a capa (em pedaços: uma lasca faltando aqui e ali), o rodapé, a saia.
    (v3) O corpo e a capa são a mureta comida (hp.mureta: o topo com as mordidas e os blocos faltando, a capa em pedaços tortos que
    some onde a mordida é funda); o rodapé ondulando nas 2 faces; o reboco caído com o tijolo."""
    F = WallFace('+y', T / 2, 0.0)
    hp.mureta(p, F, x0, x1, -0.02, H + 0.02, T, seed=seed, wear=1.0 if chips else 0.3, bites=None if chips else 0)
    p.box('Plaster#skirt', x0, x1, -T / 2 - 0.04, T / 2 + 0.04, -0.4, 0.02, bevel=0.0)
    for k, Fs in enumerate((F, WallFace('-y', -T / 2, 0.0))):
        hp.plinth_run(p, Fs, x0, x1, 0.3, seed=seed * 3 + k, out=0.025)
        hp.dirt_wall(Fs, H + 0.075)
        if chips and x1 - x0 > 1.5:
            hp.wall_spalls(p, Fs, x0, x1, 0.25, H, 1 + int(cp.h3(k, seed, 9) * 1.6), seed=seed * 7 + k)
    # o furo de dreno no pé (o escuro pequeno)
    if chips:
        x = x0 + (x1 - x0) * (0.3 + 0.4 * cp.h3(1, seed, 7))
        p.box('Dark', x - 0.07, x + 0.07, -T / 2 - 0.006, T / 2 + 0.006, 0.08, 0.16, bevel=0.0)


def pier(p, x, w=0.42, h=H + 0.3):
    # (v3) o pilarete de quina redonda e gasta (o raio no perfil), a capa um pouco torta
    vp.rbox(p, 'Plaster#pier', (x, 0.0, (h - 0.02) / 2), (w, w, h + 0.02), r=0.1, k=4)
    vp.rbox(p, 'Plaster#cap', (x, 0.0, h + 0.045), (w + 0.07, w + 0.07, 0.09), roll=1.5, r=0.12, k=4)
    vp.rbox(p, 'Plaster#cap', (x, 0.0, h + 0.125), (w - 0.12, w - 0.12, 0.07), roll=-2.0, r=0.08, k=4)
    p.box('Plaster#band', x - w / 2 - 0.025, x + w / 2 + 0.025, -w / 2 - 0.025, w / 2 + 0.025, 0.0, 0.32)
    p.box('Plaster#skirt', x - w / 2 - 0.04, x + w / 2 + 0.04, -w / 2 - 0.04, w / 2 + 0.04, -0.4, 0.02, bevel=0.0)


def wall():
    p = hp.new_part('SM_civ_wall', seed=8, dirt=0.95)
    pier(p, -1.79)
    wall_run(p, -1.58, 2.0, seed=3)
    o = hp.finish(p)
    hp.out_of_plumb(o, -2.0, 2.0, -T / 2, T / 2, lean=(0.0, 0.012), twist=0.0, bow=(0.0, 0.0))
    return o


def gate_leaf(p, x0, x1, z0=0.06, z1=2.12, hinge_x=None, seed=0):
    """A folha do portão de ferro: o quadro de tubo quadrado, a chapa ondulada de pé no meio, a travessa, as lanças em cima, as
    dobradiças no lado do pilar e a tranca no meio."""
    m = 'Metal#gate'
    for (a, b) in (((x0, z0), (x0, z1)), ((x1, z0), (x1, z1)), ((x0, z1), (x1, z1)), ((x0, z0), (x1, z0)), ((x0, 1.05), (x1, 1.05))):
        vp.rbox(p, m, ((a[0] + b[0]) / 2, 0.0, (a[1] + b[1]) / 2), (abs(b[0] - a[0]) + 0.05, 0.05, abs(b[1] - a[1]) + 0.05), r=0.01, k=1)
    hp.corr_panel(p, (x0 + 0.03, -0.008), (x1 - 0.03, -0.008), z0 + 0.03, 1.03, m='Metal#sheet', pitch=0.07, amp=0.016)
    hp.corr_panel(p, (x0 + 0.03, -0.008), (x1 - 0.03, -0.008), 1.07, z1 - 0.03, m='Metal#sheet', pitch=0.07, amp=0.016)
    n = max(3, int(abs(x1 - x0) / 0.14))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        vp.tube(p, m, [(x, 0.0, z1), (x, 0.0, z1 + 0.22)], 0.011, seg=5)
        vp.lathe(p, m, (x, 0.0, z1 + 0.22), 'z', [(0.0, 0.0), (0.03, 0.0), (0.0, 0.08)], seg=6)
    if hinge_x is not None:
        for z in (0.35, 1.8):
            vp.hinge(p, 'Dark#hinge', (hinge_x, 0.0, z), 'z', 0.16, r=0.024, knuckles=2)


def gate():
    p = hp.new_part('SM_civ_wall_gate', seed=9, dirt=0.9)
    for s in (-1, 1):
        pier(p, s * 1.45, w=0.46, h=2.55)
        if s < 0:
            wall_run(p, -2.0, -1.68, seed=5, chips=False)
        else:
            wall_run(p, 1.68, 2.0, seed=6, chips=False)
    gate_leaf(p, -1.20, -0.02, hinge_x=-1.21, seed=1)
    gate_leaf(p, 0.02, 1.20, hinge_x=1.21, seed=2)
    vp.rbox(p, 'Dark#latch', (0.0, 0.035, 1.12), (0.22, 0.03, 0.05), r=0.01, k=1)
    vp.rbox(p, 'Dark#latch', (0.06, 0.05, 1.12), (0.04, 0.05, 0.12), r=0.01, k=1)
    # a soleira de cimento no vão
    p.box('Plaster#step', -1.22, 1.22, -0.3, 0.3, -0.3, 0.05)
    return hp.finish(p)


clear_scene()
w = wall()
g = gate()
bake_in_place(g, (0.0, 8.0, 0.0))        # o portão longe do muro na hora de cozer as máscaras (a sombra de um não cai no outro)
print('CIV', w.name, cp.tris(w), g.name, cp.tris(g), 'tris (antes da grade)')
export(w, OUT)
export(g, OUT)
