"""O caminhão de suprimento da USA provisório, modelado no Blender por script (sessão Modelos 3D, 13/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_supply_truck.py -- <pasta de saída>

Não há arte aprovada do caminhão (docs/ECONOMIA.md: "6 modelos novos sem conceito ainda"); o desenho segue a raiz
da facção, a ficha do tanque USA (docs/concept/units/usa_mbt_player_color.webp) e o Dozer aprovado: carroceria
facetada em cinza-areia claro, cabine blindada avançada com chanfro, equipamento de trabalho amarelo, e a cor do
jogador como no tanque (faixa azul segmentada nas saias, painéis das portas e a lona enrolada na cabeceira).
A base real é o caminhão tático médio do exército americano (FMTV/LMTV 6×6: cabine avançada sobre o motor,
grade de proteção de canos na frente, caçamba curta) com o guindaste de carga dobrado ao lado da caçamba,
a assinatura do coletor.

Tamanho (14/10, depois da bronca do Felipe: "um tanque é pra ser grande"): na vida real o caminhão tático é mais
ALTO que o tanque (LMTV M1078: 6,43 × 2,44 × 2,85 m, man.fas.org/dod-101/sys/land/m1078.htm; M1 Abrams: casco
7,93 × 3,66 m, teto da torre 2,44 m, en.wikipedia.org/wiki/M1_Abrams), e no Zero Hour o Supply Truck tem o
comprimento do tanque. Mas no nosso RTS o tanque domina a cena: o caminhão sai em ESCALA 0,72 do desenho
(5,1 m de comprimento, 2,1 m nas rodas, cabine a 1,75 m, contra o tanque USA de 8,1 × 3,8 m e torre a 2,5 m),
~60% do tanque, como o C&C clássico lê. A carga são 4 caixas PEQUENAS (SM_USA_SupplyCrate_Small, a caixa do
depósito em 0,72: 1,08 × 0,94 × 0,72 m), em 2 × 2 na caçamba, nas vagas cargo.slots_m do Units.json impressas no
fim (já na escala): x −1,76 / −0,61, y ±0,49, z 0,94.

Peças (origem no chão, no centro do chassi, frente em +X):
  SM_USA_SupplyTruck_Hull (tudo que não gira), SM_USA_SupplyTruck_Wheels (6 malhas wheel_001..006, uma por roda,
  no lugar dela: o jogo gira cada uma pelo centro da caixa), SM_USA_SupplyCrate_Small (a caixa da carga do caminhão,
  cargo.mesh no Units.json; o depósito continua com a caixa grande).
Materiais: Paint, Team, Dark, Metal, Rubber, Lens, Light, Yellow (todos na tabela do import_provisional.py).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group, mat, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_supply_truck')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_SupplyTruck'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas
FRONT, REAR = 3.2, -3.4          # 6,6 m (+ a grade de canos e o engate: 6,8)
WHEEL_R, WHEEL_W, WHEEL_Y = 0.5, 0.38, 1.25
AXLES_X = [2.25, -1.3, -2.6]     # 6×6: um eixo na frente, dois atrás
FRAME_Z = (0.72, 1.0)            # o chassi (vigas)
BED_Z = 1.3                      # o assoalho da caçamba (= z das vagas da carga)
BED_X = (-3.3, 0.4)              # a caçamba, da traseira à cabeceira
BED_HW = 1.42                    # meia-largura de fora da caçamba (2 caixas de 1,3 lado a lado por dentro)
CAB_X = (0.8, 3.05)              # a cabine avançada
CAB_Y = 1.22
CAB_Z = 2.4
SLOTS = [(-2.45, -0.68, BED_Z), (-2.45, 0.68, BED_Z), (-0.85, -0.68, BED_Z), (-0.85, 0.68, BED_Z)]
SCALE = 0.72                     # a escala final de todas as peças (o tanque domina a cena)
CRATE_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'captures', 'game', 'prov', 'supply_crate', 'supply_crate.glb')


def hull():
    p = Part(PREFIX + '_Hull')
    # ---------------------------------------------------------------- o chassi: duas vigas, travessas, o tanque, os para-choques
    for y in (-0.55, 0.55):
        p.box('Dark', REAR + 0.1, FRONT - 0.25, y - 0.09, y + 0.09, FRAME_Z[0], FRAME_Z[1], bevel=0.02)
    for x in (-2.9, -1.95, -0.6, 0.6, 1.7, 2.7):
        p.box('Dark', x - 0.05, x + 0.05, -0.62, 0.62, FRAME_Z[0] + 0.04, FRAME_Z[1] - 0.02, bevel=0.0)
    p.cyl('Paint', (0.4, -0.95, 0.88), 'x', 0.26, 1.4, seg=16, bevel=0.015, smooth=True)       # o tanque de combustível (esquerda)
    for dx in (-0.45, 0.45):
        p.cyl('Dark', (0.4 + dx, -0.95, 0.88), 'x', 0.28, 0.05, seg=16, bevel=0.0)
    p.box('Paint', -0.2, 1.0, 0.75, 1.15, 0.65, 1.1, bevel=0.02)                                    # a caixa de ferramentas (direita)
    p.box('Metal', FRONT - 0.12, FRONT, -1.35, 1.35, 0.6, 0.88, bevel=0.03)                       # o para-choque da frente
    p.box('Dark', FRONT - 0.08, FRONT + 0.1, -0.28, 0.28, 0.63, 0.85, bevel=0.02)                  # o guincho
    p.cyl('Metal', (FRONT + 0.02, 0.0, 0.74), 'x', 0.1, 0.12, seg=12, bevel=0.0)
    p.box('Metal', REAR - 0.08, REAR + 0.05, -1.3, 1.3, 0.62, 0.86, bevel=0.03)                   # o para-choque de trás e o engate
    p.box('Metal', REAR - 0.25, REAR - 0.08, -0.1, 0.1, 0.68, 0.82, bevel=0.01)
    for s in (-1, 1):                                                                              # as lanternas de trás
        p.box('Dark', REAR - 0.12, REAR - 0.08, 1.1 * s - 0.11, 1.1 * s + 0.11, 0.66, 0.82, bevel=0.0)
    # a grade de proteção de canos na frente (a cara dos caminhões do exército), presa no para-choque
    for y in (-1.0, -0.5, 0.0, 0.5, 1.0):
        p.cyl('Metal', (FRONT + 0.18, y, 1.38), 'z', 0.032, 1.3, seg=8, bevel=0.0)
    for z in (0.9, 1.3, 1.7, 2.0):
        p.cyl('Metal', (FRONT + 0.18, 0.0, z), 'y', 0.032, 2.1, seg=8, bevel=0.0)
    for s in (-1, 1):
        p.cyl('Metal', (FRONT - 0.0, 1.05 * s, 2.0), 'x', 0.032, 0.4, seg=8, bevel=0.0)
    # as saias entre os eixos de trás (chapa com a faixa azul segmentada: cor do jogador) e os para-lamas
    for s in (-1, 1):
        yo = (1.38 + 0.006) * s
        p.box('Paint', -0.5, 1.5, min(1.28 * s, 1.38 * s), max(1.28 * s, 1.38 * s), 0.9, BED_Z - 0.02, bevel=0.02)
        for (x0, x1) in ((-0.4, 0.45), (0.6, 1.4)):
            p.box('Team', x0, x1, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.02, 1.2, bevel=0.004)
        p.box('Dark', 0.5, 0.55, min(yo, yo + 0.006 * s), max(yo, yo + 0.006 * s), 0.95, BED_Z - 0.03, bevel=0.0)
        for x in AXLES_X:
            p.obox('Paint', (x, 1.35 * s, WHEEL_R + 0.62), (1.2, 0.46, 0.06), bevel=0.01)
            p.obox('Paint', (x + 0.56, 1.35 * s, WHEEL_R + 0.44), (0.1, 0.46, 0.4), pitch=18, bevel=0.01)
            p.obox('Paint', (x - 0.56, 1.35 * s, WHEEL_R + 0.44), (0.1, 0.46, 0.4), pitch=-18, bevel=0.01)
        p.box('Metal', 1.1, 1.6, 1.15 * s - 0.2 if s > 0 else -1.45, 1.45 if s > 0 else -1.15 + 0.2, 0.42, 0.48, bevel=0.0)   # o degrau
    # ---------------------------------------------------------------- a cabine blindada avançada (a linguagem do Dozer), com o para-brisa
    cx0, cx1 = CAB_X
    p.prism_xz('Paint#cab', [(cx0, 1.05), (cx1, 1.05), (cx1 + 0.12, 1.6), (cx1 - 0.02, 1.95), (cx1 - 0.3, CAB_Z), (cx0, CAB_Z)], -CAB_Y, CAB_Y, bevel=0.09)
    p.box('Paint', cx0 - 0.06, cx1 - 0.32, -CAB_Y - 0.06, CAB_Y + 0.06, CAB_Z, CAB_Z + 0.08, bevel=0.03)   # o teto
    p.box('Paint', cx0 + 0.25, cx0 + 0.85, -0.32, 0.32, CAB_Z + 0.08, CAB_Z + 0.22, bevel=0.03)          # a escotilha
    # o para-brisa inteiro em duas folhas (na face inclinada), as janelas das portas e a traseira
    ang = math.degrees(math.atan2(0.45, 0.28))                                                         # a inclinação da face (1,95→2,4)
    for y in (-0.6, 0.6):
        p.obox('Lens', (cx1 - 0.16, y, 2.17), (0.02, 1.08, 0.4), pitch=-ang, bevel=0.0)
    p.obox('Dark', (cx1 - 0.165, 0.0, 2.17), (0.025, 0.1, 0.44), pitch=-ang, bevel=0.0)
    p.obox('Dark', (cx1 - 0.165, 0.0, 2.38), (0.025, 2.3, 0.04), pitch=-ang, bevel=0.0)                 # a moldura de cima
    for s in (-1, 1):
        yo = (CAB_Y + 0.006) * s
        p.box('Lens', cx0 + 0.3, cx0 + 1.1, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.85, 2.22, bevel=0.0)   # a janela da porta
        p.box('Dark', cx0 + 1.1, cx0 + 1.14, min(yo, yo + 0.006 * s), max(yo, yo + 0.006 * s), 1.2, 2.3, bevel=0.0)    # a linha da porta
        p.box('Team', cx0 + 0.3, cx0 + 1.1, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.3, 1.75, bevel=0.004)  # o painel azul
        p.box('Metal', cx0 + 0.95, cx0 + 1.07, min(yo, yo + 0.03 * s), max(yo, yo + 0.03 * s), 1.62, 1.66, bevel=0.0)  # a maçaneta
        p.box('Metal', cx1 - 0.3, cx1 - 0.26, (CAB_Y + 0.28) * s - 0.02, (CAB_Y + 0.28) * s + 0.02, 1.8, 2.3, bevel=0.0)   # o retrovisor
        p.box('Dark', cx1 - 0.36, cx1 - 0.2, (CAB_Y + 0.28) * s - 0.11, (CAB_Y + 0.28) * s + 0.11, 1.95, 2.25, bevel=0.01)
        p.box('Dark', cx1 + 0.04, cx1 + 0.18, 0.8 * s - 0.16, 0.8 * s + 0.16, 1.62, 1.85, bevel=0.01)      # faróis em caixa, com a guarda
        p.obox('Lens', (cx1 + 0.19, 0.8 * s, 1.735), (0.02, 0.26, 0.17), bevel=0.0)
        p.obox('Light', (cx1 + 0.195, 0.8 * s, 1.735), (0.01, 0.18, 0.11), bevel=0.0)
    p.box('Paint', cx0 + 0.5, cx0 + 1.0, -0.9, -0.45, CAB_Z + 0.08, CAB_Z + 0.36, bevel=0.02)             # a caixa do sensor
    p.box('Lens', cx0 + 1.0, cx0 + 1.02, -0.84, -0.51, CAB_Z + 0.14, CAB_Z + 0.3, bevel=0.0)
    p.cyl('Dark', (cx0 + 0.2, 0.85, CAB_Z + 0.12), 'z', 0.045, 0.08, seg=10, bevel=0.0)
    p.cyl('Metal', (cx0 + 0.2, 0.85, CAB_Z + 0.6), 'z', 0.011, 0.9, seg=6, bevel=0.0)
    # o motor atrás da cabine: a caixa com as venezianas, a chaminé com cotovelo (como no Dozer) e o filtro de ar
    p.box('Paint', BED_X[1] + 0.05, cx0, -1.05, 1.05, 1.05, 2.0, bevel=0.05)
    for k in range(6):
        p.obox('Dark', (BED_X[1] + 0.02, 0.0, 1.2 + k * 0.13), (0.03, 1.7, 0.05), pitch=-30, bevel=0.0)
    p.cyl('Dark', (BED_X[1] + 0.22, -0.88, 2.4), 'z', 0.1, 0.85, seg=14, bevel=0.01, smooth=True)
    p.cyl('Metal', (BED_X[1] + 0.22, -0.88, 2.72), 'z', 0.12, 0.08, seg=14, bevel=0.0)
    p.cyl('Dark', (BED_X[1] + 0.22, -0.88, 2.9), (-1.0, 0.0, 0.6), 0.1, 0.26, seg=12, bevel=0.0)
    p.cyl('Paint', (BED_X[1] + 0.55, -0.55, 2.28), 'z', 0.19, 0.6, seg=14, bevel=0.015, smooth=True)
    p.cyl('Dark', (BED_X[1] + 0.55, -0.55, 2.6), 'z', 0.21, 0.05, seg=14, bevel=0.0)
    # ---------------------------------------------------------------- a caçamba curta: assoalho, laterais baixas, cabeceira com a lona azul
    bx0, bx1 = BED_X
    p.box('Paint', bx0, bx1, -BED_HW + 0.08, BED_HW - 0.08, BED_Z - 0.1, BED_Z, bevel=0.02)
    for k in range(6):                                                                             # as ripas do assoalho
        p.box('Dark', bx0 + 0.2 + k * 0.6, bx0 + 0.22 + k * 0.6, -BED_HW + 0.15, BED_HW - 0.15, BED_Z, BED_Z + 0.004, bevel=0.0)
    for s in (-1, 1):                                                                              # as laterais (chapas com nervuras)
        y0, y1 = (BED_HW - 0.08, BED_HW) if s > 0 else (-BED_HW, -BED_HW + 0.08)
        p.box('Paint', bx0, bx1, y0, y1, BED_Z, BED_Z + 0.55, bevel=0.015)
        for x in (bx0 + 0.25, bx0 + 1.45, bx0 + 2.65, bx1 - 0.25):
            p.box('Paint', x - 0.04, x + 0.04, y0 - 0.03 if s < 0 else y1, y0 if s < 0 else y1 + 0.03, BED_Z, BED_Z + 0.6, bevel=0.0)
        p.box('Dark', bx0 + 0.15, bx1 - 0.15, y0 - 0.012 if s < 0 else y1, y0 if s < 0 else y1 + 0.012, BED_Z + 0.22, BED_Z + 0.32, bevel=0.0)
    p.box('Paint', bx1 - 0.08, bx1, -BED_HW, BED_HW, BED_Z, BED_Z + 0.95, bevel=0.015)              # a cabeceira
    p.box('Paint', bx0, bx0 + 0.08, -BED_HW, BED_HW, BED_Z, BED_Z + 0.55, bevel=0.015)              # a tampa de trás
    p.cyl('Team', (bx1 - 0.1, -0.72, BED_Z + 1.12), 'y', 0.2, 1.3, seg=16, bevel=0.02, smooth=True)   # a lona enrolada (à esquerda)
    for y in (-1.2, -0.72, -0.24):
        p.cyl('Dark', (bx1 - 0.1, y, BED_Z + 1.12), 'y', 0.21, 0.04, seg=16, bevel=0.0)
    # ---------------------------------------------------------------- o guindaste de carga (MHC), amarelo: a coluna atrás da cabine, no
    # deck do motor à direita, e a lança dobrada pra trás por cima da carga (a assinatura do coletor, lida de cima)
    gx, gy = BED_X[1] + 0.3, 0.72
    p.cyl('Yellow', (gx, gy, 2.14), 'z', 0.24, 0.28, seg=16, bevel=0.02)                          # a base giratória
    p.box('Yellow', gx - 0.17, gx + 0.17, gy - 0.17, gy + 0.17, 2.28, 2.95, bevel=0.03)             # a coluna
    p.cyl('Dark', (gx, gy, 2.85), 'y', 0.1, 0.4, seg=12, bevel=0.0)
    p.box('Yellow', bx0 + 0.5, gx + 0.1, gy - 0.12, gy + 0.12, 2.6, 2.86, bevel=0.03)                # a lança deitada sobre a carga
    p.box('Yellow', bx0 + 0.2, bx0 + 1.9, gy - 0.09, gy + 0.09, 2.66, 2.82, bevel=0.02)             # a extensão
    d = Vector((bx0 + 1.4, gy, 2.6)) - Vector((gx - 0.1, gy + 0.03, 2.3))
    p.cyl('Metal', (gx - 0.1 + d.x * 0.5, gy + 0.03, 2.3 + d.z * 0.5), (d.x, d.y, d.z), 0.045, d.length, seg=10, bevel=0.0)
    p.cyl('Yellow', (gx - 0.1 + d.x * 0.25, gy + 0.03, 2.3 + d.z * 0.25), (d.x, d.y, d.z), 0.07, d.length * 0.45, seg=10, bevel=0.0)
    p.obox('Dark', (bx0 + 0.3, gy, 2.52), (0.1, 0.07, 0.24), bevel=0.0)                           # o gancho na ponta
    p.obox('Metal', (bx0 + 0.3, gy, 2.36), (0.14, 0.05, 0.16), pitch=90, bevel=0.0)
    return p.finish()


def wheels():
    objs = []
    n = 0
    for x in AXLES_X:
        for s in (-1, 1):
            n += 1
            w = Part(f'wheel_{n:03d}')
            y = WHEEL_Y * s
            w.cyl('Rubber', (x, y, WHEEL_R), 'y', WHEEL_R, WHEEL_W, seg=28, bevel=0.03, smooth=True)
            w.cyl('Paint', (x, y + 0.045 * s, WHEEL_R), 'y', 0.31, WHEEL_W - 0.04, seg=24, bevel=0.01)
            w.cyl('Metal', (x, y + 0.09 * s, WHEEL_R), 'y', 0.11, WHEEL_W, seg=14, bevel=0.008)
            for k in range(6):                                                                     # os furos do aro
                a = math.radians(k * 60)
                w.cyl('Dark', (x + 0.21 * math.cos(a), y + 0.18 * s, WHEEL_R + 0.21 * math.sin(a)), 'y', 0.045, 0.1, seg=8, bevel=0.0)
            objs.append(w.finish())
    return objs


def small_crate():
    """A caixa da carga do caminhão: a caixa do depósito (blender_provisional.py) na escala do caminhão."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(CRATE_SRC))
    objs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    select_only(objs[0])
    for o in objs:
        o.select_set(True)
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    o.name = 'SM_USA_SupplyCrate_Small'
    o.data.transform(Matrix.Scale(SCALE, 4))
    return o


def main():
    clear_scene()
    h = hull()
    h.data.transform(Matrix.Scale(SCALE, 4))
    total = export(h, OUT)
    ws = wheels()
    for w in ws:
        w.data.transform(Matrix.Scale(SCALE, 4))
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    clear_scene()
    total += export(small_crate(), OUT)
    print('MODEL total', total, 'tris; escala', SCALE)
    print('MODEL cargo.slots_m', [tuple(round(v * SCALE, 3) for v in sl) for sl in SLOTS])


main()
