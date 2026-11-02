"""A casa térrea (kit civil v2, 01/11/2026; docs/KIT_CIVIL_V2.md): a peça que mais aparece nas vilas dos game shots.

Rodar (Blender sem janela, só CPU; o executor do detalhe passa a grade, o UV e as máscaras):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_civ_house_1story.py C:/Users/luisf/explosion/captures/game/models/civ_v3/house_1story

O que é (docs/concept/towns/casas, as casas térreas de teto plano; 16_beauty_pass/b_gpt embaixo à esquerda; 15_three_factions/b_gpt):
o bloco rebocado de 7,4 × 7,4 m e 3,35 m até a laje, a parede de 25 cm com a quina gasta, o rodapé grosso, a porta de ferro com a
marquise e o degrau, as janelas recuadas com a moldura, o peitoril e a grade (as de baixo), o relógio de luz com o fio que desce;
o parapeito com a capa e os dentes, os pilares de quina mais altos com o ferro de espera num deles; no teto a casinha da escada, a
caixa d'água no cavalete, a parabólica, o varal e o tapete jogado no parapeito da frente, as bicas.

Saída: SM_civ_house_1story.glb (SUBSTITUI o civ_house_1story do kit: a mesma pegada 8 × 8 m do WorldVillage.cpp, a frente em +Y,
a origem no centro da planta e no chão, a saia até −0,4 m como a de hoje).

v3 (01/11 noite, a galeria: "quase, muitas quinas, parece lego, sem detalhes, sem texturas"; o alvo: as casas do 16_beauty_pass/b_gpt):
a casa velha de adobe rebocado. A mureta COMIDA (as mordidas e os blocos faltando no topo, a capa em pedaços tortos, os dentes uns
faltando e uns quebrados), os pilares de canto com a quina redonda e GASTA (comida no pé), o rodapé ondulando, o REBOCO CAÍDO em placas
com o tijolo de adobe à mostra, os remendos de reboco e de barro (a outra cor), as FRESTAS saindo das janelas, as janelas de casa
velha (a moldura lascada, a verga de madeira torta nas da lateral, o peitoril quebrado, o caixilho fora de esquadro, um vidro
quebrado, a grade com a barra entortada, a veneziana caída), a parede FORA DE PRUMO (a casa tomba 1–2 cm por metro e embarriga); o
escorrido debaixo das janelas, das bicas e da capa e o pé úmido na cor de vértice (model_civ_parts.house_dirt).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_house_parts as hp  # noqa: E402
from model_building_parts import WallFace, ac_unit, cable  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'civ_v3', 'house_1story')
os.makedirs(OUT, exist_ok=True)

X0, X1, Y0, Y1 = -3.7, 3.7, -3.7, 3.7
ZR = 3.35                     # o topo da laje
T = 0.25


def build():
    p = hp.new_part('SM_civ_house_1story', seed=1, dirt=0.95)
    # os vãos: a porta e as janelas (u = X nas faces ±y, Y nas faces ±x)
    holes = {
        '+y': [(-1.25, -0.25, 0.0, 2.15), (0.85, 2.15, 1.0, 2.2), (-3.05, -2.25, 1.15, 2.05)],
        '+x': [(-2.0, -0.9, 1.1, 2.1), (0.9, 2.0, 1.1, 2.1)],
        '-x': [(-0.5, 0.5, 1.2, 2.05)],
        '-y': [(1.4, 2.1, 1.5, 2.1), (-2.2, -1.2, 1.15, 2.05)],
    }
    hp.house_block(p, X0, X1, Y0, Y1, ZR, t=T, holes=holes, seed=1, spalls=4, cracks=2, wear=1.0, mud=0.4, pocks=14)
    at = {'+x': X1, '-x': X0, '+y': Y1, '-y': Y0}
    kinds = {'+y': ['grille', 'grille'], '+x': ['grille', 'glass'], '-x': ['glass'], '-y': ['dark', 'grille']}
    for f, hs in holes.items():
        F = WallFace(f, at[f], 0.0)
        wins = [h for h in hs if h[2] > 0.1]
        for k, (u0, u1, z0, z1) in enumerate(wins):
            hp.window(p, F, u0, u1, z0, z1, t=T, kind=kinds[f][k], seed=k + 7 * len(f) + ord(f[1]), shutters=(f == '+x' and k == 1),
                      wear=1.0, lintel='wood' if f in ('+x', '-x') else None)
    Ff = WallFace('+y', Y1, 0.0)
    hp.steel_door(p, Ff, -1.25, -0.25, 0.0, 2.15, t=T)
    # o relógio de luz na frente e o fio que sobe pro poste (a vida na parede)
    vp.rbox(p, 'Metal#meter', (-2.65, Y1 + 0.08, 2.45), (0.34, 0.14, 0.42), r=0.03, k=2)
    vp.rbox(p, 'Dark#meterwin', (-2.65, Y1 + 0.152, 2.52), (0.18, 0.01, 0.12), r=0.02, k=1)
    vp.tube(p, 'Dark#cable', vp.bent_bar([(-2.65, Y1 + 0.06, 2.66), (-2.65, Y1 + 0.06, ZR + 0.3), (-2.65, Y1 - 0.05, ZR + 0.66)], R=0.1, n=3),
            0.014, seg=6)
    cable(p, (-2.55, Y1 + 0.06, 2.24), (-1.45, Y1 + 0.06, 2.24), sag=0.06, r=0.012)
    ac_unit(p, WallFace('-x', X0, 0.0), 1.9, 2.25, m='Metal#ac', bracket='Metal#acb')
    # o parapeito com os dentes e os pilares; o ferro de espera no pilar da frente-direita
    hp.parapet(p, X0, X1, Y0, Y1, ZR, h=0.62, t=0.22, seed=3, rebar_at=[(X1 - 0.21, Y1 - 0.21)], teeth_skip={'+y': [(0.2, 1.55)]},
               wear=1.0)
    for (f, u) in (('+x', 2.6), ('-y', -2.9), ('-x', -2.4)):
        hp.spout(p, WallFace(f, at[f], 0.0), u, ZR + 0.08)
    # o teto: a casinha da escada (fundo-esquerda, a porta pro teto), a caixa d'água no cavalete, a parabólica, o varal, o tapete
    hp.stair_box(p, -3.48, -1.38, -3.48, -1.28, ZR, h=2.2, door='+x')
    hp.water_tank(p, (1.9, -2.3, ZR), r=0.55, h=1.15, stand=0.85, seed=1)
    hp.sat_dish(p, (2.75, 2.45, ZR), r=0.42, yaw=200.0, tilt=34.0)
    hp.laundry(p, (-0.6, 0.6), (2.4, 0.2), ZR, seed=4)
    hp.rug_over(p, (0.2, Y1 + 0.03, ZR), (1.55, Y1 + 0.03, ZR), ZR + 0.62 + 0.08, (0, 1, 0), drop=1.25, depth=0.3, m='Rust#rug', seed=2)
    hp.roof_ac(p, (-0.2, -2.6, ZR), yaw=0.0)
    hp.roof_junk(p, -0.8, 1.2, -1.6, -0.4, ZR, n=4, seed=3)
    hp.roof_junk(p, -2.6, -1.6, 1.6, 2.6, ZR, n=1, seed=9)
    # a escada de mão encostada no lado +X, passando do parapeito (o jeito de subir no teto: as 3 do 16_beauty_pass/b_gpt)
    hp.ladder(p, (X1 + 0.85, 0.0, 0.0), (X1 + 0.06, 0.0, ZR + 0.95))
    o = hp.finish(p)
    hp.out_of_plumb(o, X0, X1, Y0, Y1, lean=(0.012, -0.008), twist=0.005, bow=(0.02, -0.015))
    return o


clear_scene()
o = build()
export(o, OUT)
