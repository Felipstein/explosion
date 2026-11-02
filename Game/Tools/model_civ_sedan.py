"""O sedã civil (kit civil v2, 01/11/2026; a lista e as referências em docs/KIT_CIVIL_V2.md), e a carcaça queimada dele.

Rodar (Blender sem janela, só CPU; o executor do detalhe passa a grade, o UV e as máscaras):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_civ_sedan.py C:/Users/luisf/explosion/captures/game/models/civ_v3/sedan [burnt]

O que é: o sedã de 3 volumes dos anos 90 que roda no Oriente Médio (o Corolla E100, o Peugeot 405, o Mercedes W124 de táxi):
capô comprido e reto, para-brisa a ~33°, a coluna C grossa, a tampa do porta-malas alta e curta, os faróis retangulares com a
grade cromada no meio, o friso de borracha na porta e o para-choque de plástico preto que dobra nos cantos. Nos game shots: o sedã
prata da rua (captures/concept/16_beauty_pass/b_gpt.webp, a esquerda embaixo do recorte da direita), as carcaças queimadas na rua
(o mesmo b_gpt no meio, e 15_three_factions/b_gpt.webp).
Medidas reais: 4,45 × 1,70 × 1,39 m, entre-eixos 2,55 m, pneu 185/65 R14 (raio 0,30 m), bitola 1,43 m.

Saída: SM_civ_sedan.glb (a frente em +Y, origem no centro, no chão) e, com 'burnt', SM_civ_car_burnt.glb (a carcaça: o mesmo
nome e a mesma pegada 2,0 × 4,6 m do kit de hoje; sentada nos aros, 0,11 m mais baixa).

v3 (01/11 noite, a galeria: "parece jogo de lego de novo, muitas quinas, sem detalhes"): o carro que rodou anos no deserto. A lataria
mais redonda (ombro 10 cm, o vinco da lateral, o para-lama que salta em cima das rodas e a borda do arco), 4 amassados (a quina da
frente esquerda batida, a porta direita, o capô, o teto), a porta de trás direita bege (a de ferro-velho), o para-choque de trás
caído de um lado, o retrovisor direito arrancado, a ferrugem no pé e nos arcos, as lascas de tinta no capô e no teto; as janelas da
frente abertas com o banco, o painel e o volante aparecendo (a manta vermelha no banco do motorista); a frente do W124 (os faróis
largos e a grade de lâminas em pé no quadro cromado), diferente do hatch. O pó e o vidro sujo com o arco dos limpadores na cor de
vértice (model_civ_parts.apply_dirt).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export  # noqa: E402
import model_civ_parts as cp  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'civ_v3', 'sedan')
BURNT = 'burnt' in ARGS[1:]
os.makedirs(OUT, exist_ok=True)

SPEC = {
    'L': 4.45, 'FA': 1.345, 'RA': -1.205,
    'R': 0.30, 'tw': 0.185, 'rim_r': 0.178, 'track': 0.715, 'arch_r': 0.365, 'arch_dz': 0.03,
    'zb': 0.30, 'rs': 0.10, 'rb': 0.08, 'tumble': 0.035, 'bulge': 0.016,
    # (v3) o vinco da lateral (a aresta que pega o sol) e o para-lama que salta 2,2 cm em cima das rodas
    'crease': 0.010, 'tc': 0.62, 'flare': 0.022,
    # (x, pé, topo, meia-largura, coroa) da frente pra trás: o nariz redondo, o capô que sobe até o pé do para-brisa, a cintura reta,
    # a tampa do porta-malas um pouco mais alta e a traseira que dobra pra baixo
    'body': [(2.225, 0.40, 0.63, 0.76, 0.010), (2.212, 0.335, 0.685, 0.80, 0.014), (2.18, 0.29, 0.725, 0.832, 0.02),
             (2.09, 0.27, 0.752, 0.845, 0.026), (1.70, 0.28, 0.79, 0.85, 0.03), (0.82, 0.30, 0.875, 0.85, 0.03),
             (0.70, 0.30, 0.895, 0.85, 0.02), (-1.30, 0.30, 0.905, 0.85, 0.02), (-1.52, 0.30, 0.935, 0.85, 0.026),
             (-2.02, 0.31, 0.952, 0.845, 0.026), (-2.15, 0.34, 0.93, 0.832, 0.02), (-2.205, 0.39, 0.885, 0.80, 0.015),
             (-2.225, 0.45, 0.81, 0.77, 0.010)],
    # a estufa (s, z, frente, trás, meia-largura, raio do canto em planta)
    'gh': [(0.0, 0.845, 0.80, -1.45, 0.80, 0.13), (0.12, 0.905, 0.75, -1.41, 0.795, 0.135), (0.40, 1.06, 0.50, -1.27, 0.775, 0.15),
           (0.70, 1.21, 0.25, -1.10, 0.745, 0.17), (0.90, 1.31, 0.06, -0.97, 0.715, 0.185), (1.0, 1.352, -0.03, -0.90, 0.70, 0.19)],
    'crown': 0.035, 'roof_round': 0.075,
    # os vidros: (segmento da estufa, t0, t1, s0, s1[, aberta]); o lado esquerdo (6) corre de trás pra frente. As da frente abertas
    'windows': [(0, 0.03, 0.97, 0.15, 0.95), (4, 0.05, 0.95, 0.17, 0.93),
                (2, 0.05, 0.465, 0.16, 0.94, 0.85), (2, 0.535, 0.85, 0.16, 0.94),
                (6, 0.15, 0.465, 0.16, 0.94), (6, 0.535, 0.95, 0.16, 0.94, 0.9)],
    'pillars': ((2, 0.465, 0.535), (6, 0.465, 0.535)),
    'hubcap_missing': [(1, -1)], 'roof_sag': 0.085, 'tyre_style': None, 'hubcap': True,
    # (v3) os amassados (assimétricos): a quina da frente esquerda batida, a porta da frente direita, o capô, o paralama de trás
    # esquerdo e o teto
    'dents': [('corner', 2.22, -1, 0.52, 0.42, 0.07), ('side', 0.25, 1, 0.58, 0.34, 0.035), ('top', 1.55, 0.30, 0.32, 0.028),
              ('side', -1.75, -1, 0.66, 0.30, 0.03), ('roof', -0.50, -0.25, 0.34, 0.03)],
    'rust': 0.55, 'dust': 0.9,
    'details': {
        'bump_f': (0.30, 0.45, 0.075, 0.30), 'bump_r': (0.33, 0.48, 0.075, 0.28), 'plate_r_z': 0.62,
        'head': (0.565, 0.635, 0.36, 0.13), 'sig': (0.80, 0.60), 'grille': (0.62, 0.15, 0.645, 10), 'grille_v': True,
        'tail': (0.56, 0.735, 0.40, 0.15),
        'tail_strip': True, 'hood': (0.86, 2.06, 0.755), 'trunk': (-1.50, -2.10, 0.75),
        'door_v': [(0.765, 0.34, 0.89), (-0.22, 0.32, 0.90)],
        # a porta de trás contorna o arco; a soleira
        'door_lines': [[(-1.06, 0.90), (-1.06, 0.78), (-0.99, 0.70), (-0.88, 0.60), (-0.84, 0.45), (-0.86, 0.32)],
                       [(0.765, 0.335), (0.3, 0.335), (-0.22, 0.335), (-0.6, 0.335), (-0.86, 0.335)]],
        'trim': [(0.95, -0.80, 0.56)], 'handles': [(0.30, 0.80), (-0.70, 0.80)], 'mirror': (0.66, 0.93),
        'fuel': (-1.75, 0.80), 'antenna': (-1.95, -0.70, 0.94), 'interior': (0.70, -1.40, 0.36, 2),
        # (v3) o ferro-velho: a porta de trás direita bege, o para-choque de trás caído do lado esquerdo, o retrovisor direito arrancado,
        # a manta no banco do motorista
        'door_swap': [(1, -0.86, -0.22, 'GlaPaint')], 'bump_hang': ('r', 1, 6.0), 'mirror_missing': 1, 'seat_cover': 'Rust#cover',
    },
}



clear_scene()
obj, _ = cp.passenger_car(SPEC, BURNT, 'SM_civ_car_burnt' if BURNT else 'SM_civ_sedan', seed=3)
print('CIV', obj.name, cp.tris(obj), 'tris (antes da grade)')
export(obj, OUT)
