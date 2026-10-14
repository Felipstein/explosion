"""O depósito de suprimento como PÁTIO DE CONTÊINERES com o guindaste-pórtico, provisório por script (sessão Modelos 3D, 14/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_supply_yard.py -- <pasta de saída>

Decisão (14/10, depois de o Felipe reprovar "caixas jogadas no chão" e qualquer ideia que reponha o estoque): o depósito
neutro é um pátio de contêineres: pilhas de 2 e 3 andares em duas fileiras, o pórtico amarelo sobre pneus (RTG) em H
por cima das fileiras e da pista do caminhão no meio, mastros de luz nos cantos. O estoque é finito e visível: o jogo
mostra uma instância por contêiner e tira as de cima primeiro (pads_m × stack_m do Units.json: o nível de cima some
em todas as vagas, depois o do meio, depois o chão), até sobrar o pórtico parado sobre o chão vazio.
A arte de conceito (folha de 4 ideias) está sendo gerada; este provisório segue o design 1 da folha
(captures/concept/_attach/supply_dock/PROMPT.md) e entra no jogo enquanto a arte não chega.

O que sai daqui (coordenadas da Unreal, origem no centro do lote, a pista do caminhão ao longo de Y, lote 26 × 32 m):
- SM_SupplyYard.glb: o pórtico, os mastros de luz, os blocos de amarração das vagas, a cabine do pátio; sem laje
  (o chão é o do mapa, só as marcas das vagas);
- SM_SupplyContainer.glb: o contêiner de 20 pés (2,44 × 6,06 × 2,59 m, o eixo comprido ao longo de Y), corrugado,
  com as portas, os cantos de fundição e as travas; é a "caixa" do depósito (crate no Units.json);
- yard_layout.json: pads_m (as vagas: 2 fileiras × 5) e stack_m (3 níveis), mais 'heap_m' por contêiner pra conferência.
Materiais (slots das construções): Yellow, Dark, Trim, Rubber, Glass, Light, Hazard, Crate (o contêiner; sugestão pro
jogo: uma instância 'Container' com ToneVariation=1, pra cada contêiner sair num tom), Paint, Body.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, mat, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_yard')
os.makedirs(OUT, exist_ok=True)

COLORS.update({'Yellow': (214, 164, 32), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68), 'Rubber': (32, 32, 31),
               'Glass': (30, 50, 80), 'Light': (255, 230, 170), 'Hazard': (214, 170, 30), 'Crate': (140, 72, 48),
               'Paint': (196, 198, 192), 'Body': (190, 190, 184), 'Metal': (95, 95, 92)})

CONT = (2.44, 6.06, 2.59)        # o contêiner de 20 pés: largura (x), comprimento (y), altura
ROW_X = (-7.6, 7.6)              # as duas fileiras (centro em x); a pista do caminhão fica no meio
SLOTS_Y = [-12.6, -6.3, 0.0, 6.3, 12.6]
LEVELS = 3
GANTRY_SPAN = 23.0               # entre as pernas do pórtico (x)
GANTRY_H = 10.4                  # a viga de cima
GANTRY_Y = 3.5                   # onde o pórtico está parado


def container():
    """O contêiner de 20 pés, o eixo comprido ao longo de Y: corpo corrugado, portas com as barras, cantos de fundição."""
    W, L, H = CONT
    p = Part('SM_SupplyContainer')
    hx, hy = W / 2, L / 2
    p.box('Crate', -hx, hx, -hy, hy, 0.0, H, bevel=0.03)
    for s in (-1, 1):                                                                   # a corrugação dos lados (nervuras)
        for k in range(14):
            y = -hy + 0.3 + k * 0.4
            p.box('Crate', s * hx - 0.02 if s < 0 else hx, s * hx if s < 0 else hx + 0.02, y - 0.08, y + 0.08, 0.15, H - 0.15, bevel=0.0)
    for s in (-1, 1):                                                                   # os cantos de fundição
        for t in (-1, 1):
            for z0 in (0.0, H - 0.18):
                p.box('Dark', s * hx - 0.1 - 0.012, s * hx + 0.1 + 0.012, t * hy - 0.1 - 0.012, t * hy + 0.1 + 0.012, z0, z0 + 0.18, bevel=0.0)
    # as portas (atrás, −Y): o quadro, as 4 barras de trava com as alavancas
    p.box('Dark', -hx + 0.05, hx - 0.05, -hy - 0.012, -hy, 0.12, H - 0.12, bevel=0.0)
    p.box('Crate', -hx + 0.1, hx - 0.1, -hy - 0.02, -hy - 0.012, 0.15, H - 0.15, bevel=0.0)
    p.box('Dark', -0.02, 0.02, -hy - 0.03, -hy - 0.012, 0.15, H - 0.15, bevel=0.0)
    for x in (-0.95, -0.35, 0.35, 0.95):
        p.cyl('Trim', (x, -hy - 0.05, H / 2), 'z', 0.03, H - 0.4, seg=8, bevel=0.0)
        p.box('Trim', x - 0.015, x + 0.015, -hy - 0.14, -hy - 0.05, 1.05, 1.08, bevel=0.0)
    for s in (-1, 1):                                                                   # o painel do número nos lados
        p.box('Paint', s * hx + (0.021 if s > 0 else -0.021), s * hx + (0.03 if s > 0 else -0.03), 0.6, 1.9, H - 0.75, H - 0.35, bevel=0.0)
        p.box('Dark', s * hx + (0.03 if s > 0 else -0.03), s * hx + (0.034 if s > 0 else -0.034), 0.75, 1.75, H - 0.62, H - 0.48, bevel=0.0)
    p.box('Dark', -hx + 0.2, hx - 0.2, -hy + 0.2, hy - 0.2, H, H + 0.012, bevel=0.0)      # o teto com a calha
    return p.finish()


def yard():
    p = Part('SM_SupplyYard')
    # as marcas das vagas no chão (quadros pintados) e os blocos de amarração
    W, L, H = CONT
    for x in ROW_X:
        for y in SLOTS_Y:
            for (x0, x1, y0, y1) in ((x - W / 2 - 0.15, x + W / 2 + 0.15, y - L / 2 - 0.15, y - L / 2 - 0.05),
                                     (x - W / 2 - 0.15, x + W / 2 + 0.15, y + L / 2 + 0.05, y + L / 2 + 0.15),
                                     (x - W / 2 - 0.15, x - W / 2 - 0.05, y - L / 2 - 0.15, y + L / 2 + 0.15),
                                     (x + W / 2 + 0.05, x + W / 2 + 0.15, y - L / 2 - 0.15, y + L / 2 + 0.15)):
                p.box('Paint', x0, x1, y0, y1, 0.0, 0.012, bevel=0.0)
            for sx in (-1, 1):
                for sy in (-1, 1):
                    p.box('Dark', x + sx * (W / 2 + 0.3) - 0.12, x + sx * (W / 2 + 0.3) + 0.12, y + sy * (L / 2 + 0.3) - 0.12, y + sy * (L / 2 + 0.3) + 0.12, 0.0, 0.14, bevel=0.01)
    # a pista do caminhão: as faixas amarelas pontilhadas no meio
    for k in range(12):
        y = -14.5 + k * 2.6
        p.box('Hazard', -0.08, 0.08, y, y + 1.4, 0.0, 0.012, bevel=0.0)
    # o pórtico sobre pneus (RTG): 2 pernas em A de cada lado, a viga dupla em cima, o carrinho com o spreader, as rodas
    hx = GANTRY_SPAN / 2
    for s in (-1, 1):
        for y in (GANTRY_Y - 4.0, GANTRY_Y + 4.0):
            p.box('Yellow', s * hx - 0.45, s * hx + 0.45, y - 0.45, y + 0.45, 0.9, GANTRY_H, bevel=0.05)          # as pernas
            p.box('Hazard', s * hx - 0.46, s * hx + 0.46, y - 0.46, y + 0.46, 0.9, 2.2, bevel=0.0)                 # a faixa de perigo no pé
            p.obox('Yellow', (s * hx, y, 2.4), (1.2, 2.2, 0.4), bevel=0.03)                                           # o bogie
            for yy in (-0.75, 0.75):                                                                                # os pneus duplos
                p.cyl('Rubber', (s * hx, y + yy, 0.75), 'x', 0.75, 1.1, seg=20, bevel=0.03, smooth=True)
                p.cyl('Trim', (s * hx, y + yy, 0.75), 'x', 0.3, 1.14, seg=14, bevel=0.0)
        p.box('Yellow', s * hx - 0.45, s * hx + 0.45, GANTRY_Y - 4.45, GANTRY_Y + 4.45, GANTRY_H - 1.0, GANTRY_H, bevel=0.05)   # a travessa das pernas
        p.obox('Yellow', (s * hx, GANTRY_Y, 6.0), (0.25, 7.0, 0.25), roll=0, bevel=0.0)                                # o travamento em X
        p.obox('Yellow', (s * hx, GANTRY_Y - 2.0, 5.5), (0.22, 0.22, 7.0), pitch=0, roll=42, bevel=0.0)
        p.obox('Yellow', (s * hx, GANTRY_Y + 2.0, 5.5), (0.22, 0.22, 7.0), pitch=0, roll=-42, bevel=0.0)
    for y in (GANTRY_Y - 1.2, GANTRY_Y + 1.2):                                                                      # as vigas de cima
        p.box('Yellow', -hx - 0.6, hx + 0.6, y - 0.4, y + 0.4, GANTRY_H, GANTRY_H + 1.2, bevel=0.05)
    for x in range(-10, 11, 4):                                                                                      # as travessas entre as vigas
        p.box('Yellow', x - 0.15, x + 0.15, GANTRY_Y - 0.8, GANTRY_Y + 0.8, GANTRY_H + 0.9, GANTRY_H + 1.2, bevel=0.0)
    p.box('Trim', -hx - 0.6, hx + 0.6, GANTRY_Y - 1.5, GANTRY_Y + 1.5, GANTRY_H + 1.2, GANTRY_H + 1.25, bevel=0.0)    # o passadiço
    for x in range(-11, 12, 2):
        p.box('Trim', x - 0.03, x + 0.03, GANTRY_Y - 1.5, GANTRY_Y - 1.44, GANTRY_H + 1.25, GANTRY_H + 2.2, bevel=0.0)
    p.box('Trim', -hx - 0.6, hx + 0.6, GANTRY_Y - 1.5, GANTRY_Y - 1.44, GANTRY_H + 2.15, GANTRY_H + 2.2, bevel=0.0)
    # o carrinho sobre a fileira da direita (carregando o caminhão): a cabine do operador, o spreader descido pelos cabos
    tx = ROW_X[1] * 0.0 + 3.5                                                                                       # sobre a pista, puxando pra fileira
    p.box('Yellow', tx - 1.6, tx + 1.6, GANTRY_Y - 1.7, GANTRY_Y + 1.7, GANTRY_H - 0.3, GANTRY_H + 0.0, bevel=0.04)
    p.box('Body', tx - 1.0, tx + 0.2, GANTRY_Y - 2.6, GANTRY_Y - 1.7, GANTRY_H - 2.2, GANTRY_H - 0.3, bevel=0.04)    # a cabine
    p.box('Glass', tx - 0.9, tx + 0.1, GANTRY_Y - 2.62, GANTRY_Y - 2.6, GANTRY_H - 1.9, GANTRY_H - 0.9, bevel=0.0)
    p.box('Glass', tx - 0.9, tx + 0.1, GANTRY_Y - 2.6, GANTRY_Y - 1.7, GANTRY_H - 2.21, GANTRY_H - 2.2, bevel=0.0)
    for (dx, dy) in ((-1.0, -2.6), (1.0, -2.6), (-1.0, 2.6), (1.0, 2.6)):                                           # os cabos
        p.cyl('Dark', (tx + dx, GANTRY_Y + dy, GANTRY_H - 2.6), 'z', 0.03, 4.6, seg=6, bevel=0.0)
    p.box('Yellow', tx - 1.4, tx + 1.4, GANTRY_Y - 3.2, GANTRY_Y + 3.2, GANTRY_H - 5.2, GANTRY_H - 4.8, bevel=0.03)   # o spreader
    for (dx, dy) in ((-1.22, -3.03), (1.22, -3.03), (-1.22, 3.03), (1.22, 3.03)):
        p.box('Dark', tx + dx - 0.12, tx + dx + 0.12, GANTRY_Y + dy - 0.12, GANTRY_Y + dy + 0.12, GANTRY_H - 5.5, GANTRY_H - 5.2, bevel=0.0)
    p.box('Light', tx - 0.4, tx + 0.4, GANTRY_Y - 0.3, GANTRY_Y + 0.3, GANTRY_H - 0.45, GANTRY_H - 0.3, bevel=0.0)  # o farol de trabalho
    # os mastros de luz nos 4 cantos do lote e a cabine do pátio num canto
    for (x, y) in ((-12.0, -15.0), (12.0, -15.0), (-12.0, 15.0), (12.0, 15.0)):
        p.cyl('Trim', (x, y, 5.0), 'z', 0.12, 10.0, seg=10, bevel=0.0)
        p.cyl('Trim', (x, y, 0.15), 'z', 0.4, 0.3, seg=12, bevel=0.02)
        p.box('Dark', x - 0.6, x + 0.6, y - 0.25, y + 0.25, 9.6, 10.1, bevel=0.02)
        p.box('Light', x - 0.5, x + 0.5, y - 0.26 if y < 0 else y + 0.25, y - 0.25 if y < 0 else y + 0.26, 9.65, 10.05, bevel=0.0)
    p.box('Body', 9.0, 12.4, -15.4, -11.6, 0.0, 2.9, bevel=0.06)                                                     # a cabine do pátio
    p.box('Trim', 8.8, 12.6, -15.6, -11.4, 2.9, 3.1, bevel=0.03)
    p.box('Glass', 8.98, 9.0, -14.6, -12.4, 1.3, 2.3, bevel=0.0)
    p.box('Dark', 9.4, 10.4, -15.41, -15.4, 0.0, 2.2, bevel=0.0)
    return p.finish()


def main():
    clear_scene()
    total = export(yard(), OUT)
    clear_scene()
    total += export(container(), OUT)
    W, L, H = CONT
    pads = [[x, y] for x in ROW_X for y in SLOTS_Y]
    stack = [[0.0, 0.0, k * H] for k in range(LEVELS)]
    heap = []
    for k in reversed(range(LEVELS)):
        for (x, y) in pads:
            heap.append([x, y, k * H, 0.0])
    layout = {
        '_about': 'O pátio de contêineres (model_supply_yard.py, 14/10): pads_m = as vagas (2 fileiras × 5), stack_m = os 3 níveis '
                  '(o jogo tira o de cima primeiro em todas as vagas, depois o do meio); crate = o contêiner. heap_m só pra conferência.',
        'crate': 'SM_SupplyContainer', 'yard': 'SM_SupplyYard', 'pads_m': pads, 'stack_m': stack, 'heap_m': heap,
    }
    with open(os.path.join(OUT, 'yard_layout.json'), 'w', encoding='utf-8') as f:
        json.dump(layout, f, indent=1, ensure_ascii=False)
    print('MODEL total', total, 'tris;', len(pads), 'vagas x', LEVELS, 'niveis =', len(heap), 'conteineres')


main()
