"""O depósito de suprimento (Supply Dock) provisório, modelado no Blender por script (sessão Modelos 3D, 14/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_supply_dock.py -- <pasta de saída>

As decisões do Felipe (14/10): o depósito é estoque finito, já no mapa, só diminui (nada repõe); "caixas jogadas no
chão" dos game shots são simples demais; contêineres não leem como suprimento; "tem que ser entendível e icônico,
que todo mundo entenda que é suprimento, olha o C&C ZH". No ZH o Supply Dock é a pilha de caixas de suprimento
(a caixa = dinheiro, o símbolo que todo jogador de C&C conhece) que encolhe conforme o estoque. Daí a forma:

A PIRÂMIDE DE SUPRIMENTO: as caixas empilhadas num bloco em degraus (6×5, 5×4, 4×3, 3×2, 2×1: 5 m), olive com a faixa amarela
na tampa (de cima a pirâmide lê como um monte dourado listrado: dinheiro), a rampa de madeira de carga encostada no
lado da doca (+X, onde o caminhão para), paletes vazios no pé e a placa com a flâmula amarela. A altura É o estoque:
o jogo tira as caixas de cima primeiro e a pirâmide baixa degrau a degrau, dá pra ler de qualquer lugar do mapa.
Encaixa em todo cenário sem mudar a forma (areia, neve, clareira, praça); a arte de conceito (folha de 4 ideias,
captures/concept/_attach/supply_dock/PROMPT.md) vem por cima disto.

O que sai daqui (coordenadas da Unreal, origem no centro do depósito, a doca em +X):
- SM_SupplyCrate.glb: a caixa de suprimento (1,5 × 1,25 × 1,0 m): corpo olive, tampa com aro, a faixa amarela da
  tampa (lê de cima), cantos reforçados, duas alças de corda, o remendo do estêncil; é a instância que o jogo mostra
  e esconde pelo estoque (ARTSDockActor);
- SM_SupplyDock_Yard.glb: o que fica quando as caixas acabam: a rampa, os paletes vazios, a placa; sem laje;
- dock_layout.json: 'heap_m' = a pirâmide, uma caixa por item [x, y, z, giro], as primeiras somem primeiro (o topo,
  depois cada degrau do lado da doca pro fundo) — pede uma linha nova no RTSGameData.cpp (se houver 'heap_m',
  CrateSlots vem dela, com o giro, como o pile_m); e 'pads_m' + 'stack_m' de reserva (o bloco 7×6 em 2 níveis),
  que funciona com o código de hoje.
Materiais (slots das construções): Crate (olive), Yellow, Dark, Trim, Paint, Rubber.
"""
import json
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, mat, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'supply_dock')
os.makedirs(OUT, exist_ok=True)

COLORS.update({'Crate': (96, 98, 62), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38), 'Trim': (70, 70, 68),
               'Paint': (196, 198, 192), 'Rubber': (32, 32, 31), 'Rope': (176, 160, 120)})

CRATE = (1.5, 1.25, 1.0)          # a caixa: comprimento (x), largura (y), altura
GAP = 0.1                         # a folga entre caixas no bloco
LEVELS = [(6, 5), (5, 4), (4, 3), (3, 2), (2, 1)]   # caixas por degrau (x, y), do chão ao topo: meia caixa de recuo por lado
rng = random.Random(1410)


def crate_mesh():
    """A caixa de suprimento: corpo olive, a tampa com aro e a FAIXA AMARELA (a cor do suprimento no C&C, lê de cima),
    cantos reforçados, duas alças de corda nas pontas, o remendo do estêncil."""
    L, W, H = CRATE
    p = Part('SM_SupplyCrate')
    hx, hy = L / 2, W / 2
    p.box('Crate', -hx, hx, -hy, hy, 0.0, H - 0.1, bevel=0.025)
    p.box('Crate', -hx - 0.02, hx + 0.02, -hy - 0.02, hy + 0.02, H - 0.1, H, bevel=0.02)          # a tampa com o aro
    p.box('Yellow', -hx - 0.021, hx + 0.021, -0.2, 0.2, H - 0.1, H + 0.006, bevel=0.0)              # a faixa amarela da tampa, de ponta a ponta
    p.box('Dark', -hx - 0.021, hx + 0.021, -0.21, -0.2, H - 0.1, H + 0.007, bevel=0.0)                # o filete escuro da faixa
    p.box('Dark', -hx - 0.021, hx + 0.021, 0.2, 0.21, H - 0.1, H + 0.007, bevel=0.0)
    for sx in (-1, 1):                                                                             # os cantos reforçados
        for sy in (-1, 1):
            p.box('Dark', sx * hx - 0.07 - 0.012, sx * hx + 0.07 + 0.012, sy * hy - 0.07 - 0.012, sy * hy + 0.07 + 0.012, 0.0, H - 0.1, bevel=0.0)
    for sx in (-1, 1):                                                                             # as alças de corda nas pontas
        x = sx * (hx + 0.012)
        p.cyl('Rope', (x, 0.0, 0.62), 'y', 0.03, 0.44, seg=8, bevel=0.0)
        for y in (-0.22, 0.22):
            p.cyl('Rope', (x, y, 0.56), 'z', 0.03, 0.12, seg=8, bevel=0.0)
    for s in (-1, 1):                                                                              # a ripa e o estêncil dos lados
        yy = s * (hy + 0.006)
        p.box('Dark', -hx + 0.1, hx - 0.1, min(yy, yy + 0.004 * s), max(yy, yy + 0.004 * s), 0.3, 0.34, bevel=0.0)
        p.box('Dark', -hx + 0.1, hx - 0.1, min(yy, yy + 0.004 * s), max(yy, yy + 0.004 * s), 0.62, 0.66, bevel=0.0)
        p.box('Paint', -0.28, 0.28, min(yy, yy + 0.006 * s), max(yy, yy + 0.006 * s), 0.38, 0.58, bevel=0.0)
        p.box('Dark', -0.2, 0.2, min(yy + 0.006 * s, yy + 0.009 * s), max(yy + 0.006 * s, yy + 0.009 * s), 0.45, 0.51, bevel=0.0)
    return p.finish()


def heap_layout():
    """A pirâmide: cada degrau centrado, inset de meia caixa; as de cima somem primeiro, e dentro do degrau as do
    lado da doca (+X) primeiro. Devolve [(x, y, z, giro)...] já na ordem de sumir."""
    L, W, H = CRATE
    sx, sy = L + GAP, W + GAP
    order = []
    for lvl, (nx, ny) in reversed(list(enumerate(LEVELS))):
        cells = []
        for i in range(nx):
            for j in range(ny):
                x = (i - (nx - 1) / 2) * sx + rng.uniform(-0.04, 0.04)
                y = (j - (ny - 1) / 2) * sy + rng.uniform(-0.04, 0.04)
                yaw = rng.uniform(-4, 4)
                cells.append((round(x, 3), round(y, 3), round(lvl * H, 3), round(yaw, 1)))
        cells.sort(key=lambda c: -c[0])                                                            # +X (a doca) some antes
        order += cells
    return order


def pads_fallback():
    """O bloco 7×6 em 2 níveis como pads_m + stack_m, pro código de hoje (o nível de cima some primeiro)."""
    L, W, H = CRATE
    nx, ny = LEVELS[0]
    pads = [[round((i - (nx - 1) / 2) * (L + GAP), 2), round((j - (ny - 1) / 2) * (W + GAP), 2)] for i in range(nx) for j in range(ny)]
    stack = [[0.0, 0.0, 0.0], [0.0, 0.0, H]]
    return pads, stack


def yard():
    """O que fica no chão quando o estoque acaba: a rampa de carga de madeira do lado da doca, paletes vazios, a placa."""
    p = Part('SM_SupplyDock_Yard')
    L, W, H = CRATE
    nx, ny = LEVELS[0]
    edge_x = (nx / 2) * (L + GAP)                                                                   # a borda do bloco em +X
    # a rampa: tábuas inclinadas do chão até o 1º degrau, com as vigas e a guarda
    for k in range(5):
        y = -1.3 + k * 0.65
        p.obox('Crate', (edge_x + 1.3, y, 0.5), (2.8, 0.6, 0.08), pitch=-20, bevel=0.0)
    for y in (-1.7, 1.7):
        p.obox('Crate', (edge_x + 1.3, y, 0.46), (2.9, 0.12, 0.2), pitch=-20, bevel=0.01)
        p.cyl('Trim', (edge_x + 0.2, y, 0.75), 'z', 0.03, 1.1, seg=6, bevel=0.0)
        p.obox('Trim', (edge_x + 1.3, y, 1.3), (2.9, 0.03, 0.03), pitch=-20, bevel=0.0)
    for x in (edge_x + 0.3, edge_x + 1.3, edge_x + 2.3):
        z = 0.95 - (x - edge_x) * math.tan(math.radians(20))
        p.box('Trim', x - 0.08, x + 0.08, -1.75, 1.75, 0.0, max(0.1, z - 0.05), bevel=0.0)
    # paletes vazios empilhados nos cantos e um solto
    for (x, y, n, yaw) in ((-edge_x - 1.6, -3.6, 4, 12), (-edge_x - 1.9, 3.1, 2, -20), (edge_x + 3.6, 3.4, 3, 35)):
        for k in range(n):
            z = k * 0.15
            p.obox('Crate', (x, y, z + 0.02), (L, W, 0.04), yaw=yaw, bevel=0.0)
            p.obox('Crate', (x, y, z + 0.09), (L - 0.1, W - 0.1, 0.1), yaw=yaw, bevel=0.0)
            p.obox('Crate', (x, y, z + 0.14), (L, W, 0.02), yaw=yaw, bevel=0.0)
    p.obox('Crate', (edge_x + 4.4, -3.2, 0.02), (L, W, 0.04), yaw=70, bevel=0.0)
    # a placa do depósito com a flâmula amarela (lê de longe)
    p.cyl('Trim', (edge_x + 3.4, -2.8, 1.75), 'z', 0.05, 3.5, seg=8, bevel=0.0)
    p.box('Paint', edge_x + 3.38, edge_x + 3.42, -3.4, -2.2, 1.3, 2.0, bevel=0.0)
    p.box('Dark', edge_x + 3.42, edge_x + 3.43, -3.25, -2.35, 1.45, 1.85, bevel=0.0)
    p.obox('Yellow', (edge_x + 3.4, -2.3, 3.25), (0.012, 1.1, 0.4), yaw=0, bevel=0.0)                  # a flâmula
    return p.finish()


def main():
    clear_scene()
    total = export(yard(), OUT)
    clear_scene()
    total += export(crate_mesh(), OUT)
    heap = heap_layout()
    pads, stack = pads_fallback()
    layout = {
        '_about': 'A pirâmide de suprimento (model_supply_dock.py, 14/10): heap_m = uma caixa por item [x, y, z, giro] em metros e '
                  'graus, Unreal (Y pra direita), as primeiras somem primeiro (o topo, depois cada degrau do lado da doca pro fundo); '
                  'pads_m + stack_m = o bloco 7×6 em 2 níveis, pro código de hoje. crate = a caixa; yard = o que fica.',
        'crate': 'SM_SupplyCrate', 'yard': 'SM_SupplyDock_Yard',
        'heap_m': [list(c) for c in heap], 'pads_m': pads, 'stack_m': stack,
    }
    with open(os.path.join(OUT, 'dock_layout.json'), 'w', encoding='utf-8') as f:
        json.dump(layout, f, indent=1, ensure_ascii=False)
    L, W, H = CRATE
    print('MODEL total', total, 'tris; piramide', len(heap), 'caixas, topo a', round(len(LEVELS) * H, 2), 'm; reserva', len(pads), 'vagas x', len(stack))


main()
