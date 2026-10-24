"""O lançador de mísseis de cruzeiro da USA (Tomahawk Launcher) provisório, modelado no Blender por script
(sessão Modelos 3D, 22/10/2026).

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_tomahawk.py -- <pasta de saída>

Sem arte aprovada (o botão do HUD, docs/concept/hud/kit/photos/buttons/usa_rocket_launcher.png, é só um caminhão com
uma caixa). O desenho segue a raiz da facção (a ficha do tanque USA, docs/concept/units/usa_mbt_player_color.webp:
cinza-areia claro, placas facetadas, a cor do jogador em detalhes pequenos espalhados) e é da MESMA FAMÍLIA do
caminhão de suprimento aprovado (Game/Tools/model_usa_supply_truck.py): o chassi 6×6, a cabine avançada blindada, a
grade de canos, as saias com a faixa azul, tudo igual; a caçamba e o guindaste dão lugar ao lançador.
Base real (inspiração, não cópia): o lançador terrestre do míssil de cruzeiro (o TEL do BGM-109G, tubos que se
levantam pela traseira), o HIMARS no chassi FMTV (o mesmo caminhão do supply truck) e o NMESIS de 2 células.
A assinatura, lida de cima: os DOIS TUBOS GROSSOS deitados nas costas do caminhão, com as pontas dos mísseis
aparecendo na boca, o berço blindado com a faixa azul segmentada e os dois anéis azuis em cada tubo (como os anéis
do cano do tanque); de lado, o lançador que se levanta pela dobradiça de trás e os macacos amarelos na traseira.

Tamanho: o mesmo do caminhão (escala 0,72 do desenho, aplicada na exportação; "um tanque é pra ser grande"): 5,1 m
de comprimento com a sobra dos tubos atrás, 2,3 m de largura, cabine a 1,75 m, lançador deitado a ~2,0 m; contra o
tanque USA de 8,1 × 3,8 m e torre a 2,5 m.

Peças (origem no chão, no centro do chassi, frente em +X; TODAS nas coordenadas do veículo, como o tanque do Look):
  SM_USA_Tomahawk_Hull   (hull) o chassi, a cabine, o convés do lançador, o anel da mesa, os macacos de trás;
  SM_USA_Tomahawk_Wheels (hull) 6 malhas wheel_001..006 (o jogo gira cada uma pelo centro da caixa dela);
  SM_USA_Tomahawk_Turret (turret, gira em Z no PIVOT) a mesa giratória, o berço com as vigas até a dobradiça e os
                         motores de elevação;
  SM_USA_Tomahawk_Pod    (turret) os 2 tubos no berço blindado, DEITADOS (posição de marcha); a elevação gira em
                         torno de HINGE (eixo Y, positivo levanta a boca); (23/10, v4) o jogo levanta o Pod na dobradiça
                         (o bloco "launcher" do Units.json);
  SM_USA_Tomahawk_Noses  (turret, levanta com o Pod) 2 malhas nose_001 e nose_002, as pontas dos mísseis na boca de cada
                         tubo: o jogo esconde a do tubo que atirou até a recarga (o ProjectileBoneFeedback do ZH).
  pose/SM_USA_Tomahawk_Launcher_Fire: Turret + Pod levantado FIRE_PITCH, só pra prévia (fora do glob da importação).
Materiais: Paint, Team, Dark, Metal, Rubber, Lens, Light, Yellow (os do caminhão: DOZER_SLOTS no import_provisional.py).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group, select_only  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_tomahawk')
os.makedirs(OUT, exist_ok=True)
POSE = os.path.join(OUT, 'pose')
os.makedirs(POSE, exist_ok=True)
PREFIX = 'SM_USA_Tomahawk'

# as cores de conferência do caminhão (o jogo usa as instâncias dele)
COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- medidas (desenho, antes da escala)
FRONT, REAR = 3.2, -3.4          # o chassi do caminhão (6,6 m + a grade de canos e o engate)
WHEEL_R, WHEEL_W, WHEEL_Y = 0.5, 0.38, 1.25
AXLES_X = [2.25, -1.3, -2.6]     # 6×6: um eixo na frente, dois atrás
FRAME_Z = (0.72, 1.0)
DECK_Z = 1.3                     # o convés do lançador (= o assoalho da caçamba do caminhão)
DECK_X = (-3.35, 0.42)
DECK_HW = 1.4
ENGINE_X0 = 0.45                 # a caixa do motor atrás da cabine (do caminhão)
CAB_X = (0.8, 3.05)
CAB_Y = 1.22
CAB_Z = 2.4
PIVOT = (-1.8, 0.0)              # o eixo da mesa giratória (turret_pivot_m antes da escala)
RING_R = 0.98                    # o anel da mesa no convés
POD_X = (-3.7, 0.1)              # os tubos, da traseira à boca (a boca olha pra +X: atira por cima da cabine)
POD_Z0 = 1.95                    # o fundo do berço
TUBE_R, TUBE_Y, TUBE_Z = 0.36, 0.42, 2.36
NOSE_L, NOSE_R = 0.5, 0.25       # a ponta do míssil pra fora da boca: mais fina que o tubo (o anel escuro da boca
                                 # aparece em volta) e afilada com a ponta escura; redonda e grossa, lia como botijão
HINGE = (-3.2, POD_Z0)           # a dobradiça da elevação (x, z), eixo em Y: 0,5 m antes do fim dos tubos (na ponta,
                                 # os motores de elevação passavam na veneziana do motor com o lançador virado pra trás)
FIRE_PITCH = 45.0                # a pose de tiro da prévia
SCALE = 0.72                     # a escala final (a mesma do caminhão)


def hull():
    p = Part(PREFIX + '_Hull')
    # ---------------------------------------------------------------- o chassi do caminhão: vigas, travessas, tanque, para-choques
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
    # a faixa de aviso no para-choque de trás (amarelo e preto pintados na peça: a faixa procedural do mundo escorre
    # num veículo que anda)
    for k in range(8):
        y0 = -0.8 + k * 0.2
        p.box('Yellow' if k % 2 == 0 else 'Dark', REAR - 0.09, REAR - 0.08, y0, y0 + 0.2, 0.66, 0.82, bevel=0.0)
    # a grade de proteção de canos na frente (a cara dos caminhões do exército), presa no para-choque
    for y in (-1.0, -0.5, 0.0, 0.5, 1.0):
        p.cyl('Metal', (FRONT + 0.18, y, 1.38), 'z', 0.032, 1.3, seg=8, bevel=0.0)
    for z in (0.9, 1.3, 1.7, 2.0):
        p.cyl('Metal', (FRONT + 0.18, 0.0, z), 'y', 0.032, 2.1, seg=8, bevel=0.0)
    for s in (-1, 1):
        p.cyl('Metal', (FRONT - 0.0, 1.05 * s, 2.0), 'x', 0.032, 0.4, seg=8, bevel=0.0)
    # as saias entre os eixos (chapa com a faixa azul segmentada: cor do jogador) e os para-lamas
    for s in (-1, 1):
        yo = (1.38 + 0.006) * s
        p.box('Paint', -0.5, 1.5, min(1.28 * s, 1.38 * s), max(1.28 * s, 1.38 * s), 0.9, DECK_Z - 0.02, bevel=0.02)
        for (x0, x1) in ((-0.4, 0.45), (0.6, 1.4)):
            p.box('Team', x0, x1, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), 1.02, 1.2, bevel=0.004)
        p.box('Dark', 0.5, 0.55, min(yo, yo + 0.006 * s), max(yo, yo + 0.006 * s), 0.95, DECK_Z - 0.03, bevel=0.0)
        for x in AXLES_X:
            p.obox('Paint', (x, 1.35 * s, WHEEL_R + 0.62), (1.2, 0.46, 0.06), bevel=0.01)
            p.obox('Paint', (x + 0.56, 1.35 * s, WHEEL_R + 0.44), (0.1, 0.46, 0.4), pitch=18, bevel=0.01)
            p.obox('Paint', (x - 0.56, 1.35 * s, WHEEL_R + 0.44), (0.1, 0.46, 0.4), pitch=-18, bevel=0.01)
        p.box('Metal', 1.1, 1.6, 1.15 * s - 0.2 if s > 0 else -1.45, 1.45 if s > 0 else -1.15 + 0.2, 0.42, 0.48, bevel=0.0)   # o degrau
    # ---------------------------------------------------------------- a cabine blindada avançada (igual à do caminhão)
    cx0, cx1 = CAB_X
    p.prism_xz('Paint#cab', [(cx0, 1.05), (cx1, 1.05), (cx1 + 0.12, 1.6), (cx1 - 0.02, 1.95), (cx1 - 0.3, CAB_Z), (cx0, CAB_Z)], -CAB_Y, CAB_Y, bevel=0.09)
    p.box('Paint', cx0 - 0.06, cx1 - 0.32, -CAB_Y - 0.06, CAB_Y + 0.06, CAB_Z, CAB_Z + 0.08, bevel=0.03)   # o teto
    p.box('Paint', cx0 + 0.25, cx0 + 0.85, -0.32, 0.32, CAB_Z + 0.08, CAB_Z + 0.22, bevel=0.03)          # a escotilha
    ang = math.degrees(math.atan2(0.45, 0.28))                                                         # a inclinação do para-brisa
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
        p.box('Dark', cx1 + 0.04, cx1 + 0.18, 0.8 * s - 0.16, 0.8 * s + 0.16, 1.62, 1.85, bevel=0.01)      # faróis em caixa
        p.obox('Lens', (cx1 + 0.19, 0.8 * s, 1.735), (0.02, 0.26, 0.17), bevel=0.0)
        p.obox('Light', (cx1 + 0.195, 0.8 * s, 1.735), (0.01, 0.18, 0.11), bevel=0.0)
    p.box('Paint', cx0 + 0.5, cx0 + 1.0, -0.9, -0.45, CAB_Z + 0.08, CAB_Z + 0.36, bevel=0.02)             # a caixa do sensor
    p.box('Lens', cx0 + 1.0, cx0 + 1.02, -0.84, -0.51, CAB_Z + 0.14, CAB_Z + 0.3, bevel=0.0)
    for (ay, h) in ((0.85, 0.9), (0.55, 1.3)):                                                         # duas antenas (o rádio do tiro)
        p.cyl('Dark', (cx0 + 0.2, ay, CAB_Z + 0.12), 'z', 0.045, 0.08, seg=10, bevel=0.0)
        p.cyl('Metal', (cx0 + 0.2, ay, CAB_Z + 0.15 + h / 2), 'z', 0.011, h, seg=6, bevel=0.0)
    # o motor atrás da cabine: a caixa com as venezianas e o filtro de ar; a chaminé com cotovelo sobe no canto de trás
    # da cabine, por fora (atrás da cabine, como no caminhão, o lançador levantado e virado pra trás passava nela)
    p.box('Paint', ENGINE_X0, cx0, -1.05, 1.05, 1.05, 1.82, bevel=0.05)                              # (mais baixa que a do caminhão: o
    for k in range(5):                                                                                 # fim dos tubos levantados passa por cima)
        p.obox('Dark', (ENGINE_X0 - 0.03, 0.0, 1.2 + k * 0.13), (0.03, 1.7, 0.05), pitch=-30, bevel=0.0)
    ex, ey = cx0 + 0.12, -CAB_Y - 0.16
    p.cyl('Dark', (ex, ey, 2.0), 'z', 0.09, 1.3, seg=14, bevel=0.01, smooth=True)
    p.cyl('Metal', (ex, ey, 2.45), 'z', 0.11, 0.08, seg=14, bevel=0.0)
    p.cyl('Dark', (ex - 0.05, ey, 2.72), (-1.0, 0.0, 0.6), 0.09, 0.24, seg=12, bevel=0.0)
    for z in (1.55, 2.2):                                                                              # as presilhas na cabine
        p.box('Metal', ex - 0.03, ex + 0.03, ey + 0.05, -CAB_Y, z - 0.03, z + 0.03, bevel=0.0)
    p.cyl('Paint', (1.0, -0.72, 2.28), 'z', 0.19, 0.6, seg=14, bevel=0.015, smooth=True)              # (mais pra fora que no
    p.cyl('Dark', (1.0, -0.72, 2.6), 'z', 0.21, 0.05, seg=14, bevel=0.0)                              # caminhão: a ponta do míssil gira perto)
    # ---------------------------------------------------------------- o convés do lançador (no lugar da caçamba)
    dx0, dx1 = DECK_X
    p.box('Paint', dx0, dx1, -DECK_HW, DECK_HW, DECK_Z - 0.1, DECK_Z, bevel=0.02)
    for s in (-1, 1):
        y0, y1 = (DECK_HW - 0.06, DECK_HW) if s > 0 else (-DECK_HW, -DECK_HW + 0.06)
        p.box('Paint', dx0, dx1, y0, y1, DECK_Z - 0.12, DECK_Z + 0.06, bevel=0.015)                # a borda do convés (acima dos para-lamas)
        yo = (DECK_HW + 0.004) * s
        p.box('Dark', dx0 + 0.1, dx1 - 0.1, min(yo, yo + 0.01 * s), max(yo, yo + 0.01 * s), DECK_Z - 0.08, DECK_Z - 0.03, bevel=0.0)
        p.box('Dark', dx0 + 0.15, -0.35, 1.0 * s - 0.15, 1.0 * s + 0.15, DECK_Z, DECK_Z + 0.006, bevel=0.0)   # a passarela (grade escura)
    p.cyl('Dark', (PIVOT[0], PIVOT[1], DECK_Z + 0.017), 'z', RING_R, 0.034, seg=40, bevel=0.0)       # o anel da mesa giratória
    p.cyl('Metal', (PIVOT[0], PIVOT[1], DECK_Z + 0.02), 'z', RING_R - 0.08, 0.04, seg=40, bevel=0.0)
    # o painel de tiro (esquerda, na frente do convés) e a caixa de cabos: baixos, por baixo do giro do lançador
    p.box('Paint', -0.35, 0.3, -1.32, -0.95, DECK_Z, DECK_Z + 0.24, bevel=0.03)
    for k in range(4):
        p.box('Dark', -0.3 + k * 0.15, -0.22 + k * 0.15, -1.326, -1.32, DECK_Z + 0.06, DECK_Z + 0.19, bevel=0.0)
    p.box('Lens', 0.3, 0.304, -1.25, -1.05, DECK_Z + 0.09, DECK_Z + 0.17, bevel=0.0)
    # a lona azul enrolada (o painel de identificação aérea, como a do tanque) na frente-direita do convés, baixa:
    # o lançador gira por cima dela
    p.cyl('Team', (-0.15, 1.1, DECK_Z + 0.14), 'x', 0.14, 0.95, seg=16, bevel=0.02, smooth=True)
    for x in (-0.5, -0.15, 0.2):
        p.cyl('Dark', (x, 1.1, DECK_Z + 0.14), 'x', 0.15, 0.04, seg=16, bevel=0.0)
    # os macacos estabilizadores nos cantos de trás, por fora (amarelos, recolhidos: a artilharia firma o caminhão)
    for s in (-1, 1):
        jy = 1.42 * s
        p.box('Paint', REAR - 0.02, dx0 + 0.03, jy - 0.15, jy + 0.15, DECK_Z - 0.3, DECK_Z - 0.1, bevel=0.01)   # o suporte
        p.box('Yellow', REAR - 0.22, REAR, jy - 0.11, jy + 0.11, 0.55, DECK_Z - 0.12, bevel=0.02)
        p.box('Dark', REAR - 0.23, REAR + 0.01, jy - 0.12, jy + 0.12, DECK_Z - 0.16, DECK_Z - 0.11, bevel=0.0)
        p.cyl('Metal', (REAR - 0.11, jy, 0.44), 'z', 0.05, 0.24, seg=10, bevel=0.0)
        p.box('Dark', REAR - 0.28, REAR + 0.06, jy - 0.16, jy + 0.16, 0.28, 0.33, bevel=0.01)          # a sapata
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


def turret():
    """A mesa giratória e o berço: gira em Z no PIVOT. As vigas levam o berço até a dobradiça de trás (HINGE),
    onde ficam os motores de elevação (sem cilindro hidráulico: o tubo levantado não quebra a peça)."""
    p = Part(PREFIX + '_Turret')
    px, py = PIVOT
    hx, hz = HINGE
    p.cyl('Paint', (px, py, DECK_Z + 0.045 + 0.09), 'z', 0.92, 0.18, seg=32, bevel=0.03)             # a mesa (acima do anel)
    p.cyl('Dark', (px, py, DECK_Z + 0.23), 'z', 0.8, 0.01, seg=32, bevel=0.0)
    z0 = DECK_Z + 0.225
    p.box('Dark', px - 0.15, px + 0.15, 0.72, 0.86, z0, z0 + 0.22, bevel=0.01)                        # o motor do giro
    # o pedestal sobre a mesa e as duas vigas até a dobradiça
    p.box('Paint', px - 0.8, px + 0.75, -0.72, 0.72, z0, POD_Z0 - 0.02, bevel=0.04)
    for s in (-1, 1):
        p.box('Paint', hx, px - 0.6, 0.5 * s - 0.12, 0.5 * s + 0.12, POD_Z0 - 0.3, POD_Z0 - 0.02, bevel=0.03)   # (nada atrás da dobradiça por baixo dos tubos: o fim deles desce quando levanta)
        # a orelha da dobradiça (chapa) e o motor de elevação no eixo
        p.prism_xz('Paint', [(hx - 0.2, POD_Z0 - 0.32), (hx + 0.55, POD_Z0 - 0.32), (hx + 0.55, POD_Z0 - 0.18),
                             (hx + 0.18, POD_Z0 + 0.14), (hx - 0.2, POD_Z0 + 0.14)], 0.94 * s, 1.0 * s, bevel=0.015)
        p.cyl('Dark', (hx, 1.09 * s, hz), 'y', 0.2, 0.18, seg=20, bevel=0.0)
        p.cyl('Metal', (hx, 1.2 * s, hz), 'y', 0.11, 0.05, seg=14, bevel=0.0)
        p.box('Paint', hx - 0.2, hx + 0.55, min(0.62 * s, 0.94 * s), max(0.62 * s, 0.94 * s), POD_Z0 - 0.32, POD_Z0 - 0.2, bevel=0.0)   # a ponte viga-orelha
    p.box('Dark', hx - 0.04, hx + 0.2, -0.62, 0.62, POD_Z0 - 0.28, POD_Z0 - 0.1, bevel=0.0)         # a travessa da dobradiça
    p.box('Dark', px + 0.75, px + 0.78, -0.5, 0.5, z0 + 0.08, POD_Z0 - 0.12, bevel=0.0)              # o painel da frente do pedestal
    return p.finish()


def nose(p, x0, y, z):
    """A ponta do míssil saindo da boca do tubo: ogiva afilada (perfil de potência), o último quarto escuro (o radome)."""
    rings = []
    n = 10
    for k in range(n + 1):
        t = k / n
        r = max(NOSE_R * (1.0 - t ** 1.6) ** 0.6, 0.012)
        x = x0 + NOSE_L * t
        rings.append([(x, y + r * math.cos(2 * math.pi * j / 20), z + r * math.sin(2 * math.pi * j / 20)) for j in range(20)])
    p.loft('Paint#nose', rings[:8], bevel=0.0, smooth=True)
    p.loft('Dark#nose', rings[7:], bevel=0.0, smooth=True)


def pod():
    """Os 2 tubos no berço blindado, deitados, a boca pra +X; a elevação gira em torno de HINGE."""
    p = Part(PREFIX + '_Pod')
    x0, x1 = POD_X
    bx0, bx1 = x0 + 0.1, x1 - 0.1
    # o berço blindado (a metade de baixo dos tubos), facetado como as placas do tanque
    prof = [(-0.8, POD_Z0), (0.8, POD_Z0), (0.88, POD_Z0 + 0.08), (0.88, TUBE_Z - 0.06), (0.8, TUBE_Z + 0.02),
            (-0.8, TUBE_Z + 0.02), (-0.88, TUBE_Z - 0.06), (-0.88, POD_Z0 + 0.08)]
    p.loft('Paint', [[(bx0, y, z) for (y, z) in prof], [(bx1, y, z) for (y, z) in prof]], bevel=0.02)
    # os tubos (lisos), as cintas, os dois anéis azuis em cada um (como os do cano do tanque)
    xc, L = (x0 + x1) / 2, x1 - x0
    for s in (-1, 1):
        ty = TUBE_Y * s
        p.cyl('Paint#tube', (xc, ty, TUBE_Z), 'x', TUBE_R, L, seg=32, bevel=0.0, smooth=True)
        for x in (x0 + 0.45, xc, x1 - 0.45):
            p.cyl('Dark', (x, ty, TUBE_Z), 'x', TUBE_R + 0.014, 0.08, seg=32, bevel=0.0)
        for x in (x1 - 0.85, x1 - 1.08):
            p.cyl('Team', (x, ty, TUBE_Z), 'x', TUBE_R + 0.01, 0.12, seg=32, bevel=0.0)
        # a boca: o aro escuro e o fundo escuro (a ponta do míssil saindo é outra malha, noses(): o jogo esconde a do tubo que
        # atirou até a recarga, como o ProjectileBoneFeedbackEnabledSlots do Tomahawk do ZH; o fundo escuro é o tubo vazio)
        p.cyl('Dark', (x1 - 0.02, ty, TUBE_Z), 'x', TUBE_R + 0.025, 0.06, seg=32, bevel=0.0)
        p.cyl('Dark', (x1 + 0.012, ty, TUBE_Z), 'x', TUBE_R - 0.03, 0.01, seg=32, bevel=0.0)
        # a traseira: a tampa do escape e o aro
        p.cyl('Metal', (x0 + 0.02, ty, TUBE_Z), 'x', TUBE_R + 0.025, 0.06, seg=32, bevel=0.0)
        p.cyl('Dark', (x0 - 0.012, ty, TUBE_Z), 'x', TUBE_R - 0.04, 0.01, seg=32, bevel=0.0)
    # a calha de cabos entre os tubos (a linha escura de cima)
    p.box('Dark', x0 + 0.3, x1 - 0.3, -0.05, 0.05, TUBE_Z, TUBE_Z + 0.24, bevel=0.0)
    # os quadros (frente, meio, trás) por cima dos tubos: a estrutura do lançador lida de cima
    top = TUBE_Z + TUBE_R
    for fx in (x1 - 0.18, xc - 0.07, x0 + 0.04):
        p.box('Paint', fx, fx + 0.14, -0.88, 0.88, top, top + 0.08, bevel=0.015)
        for s in (-1, 1):
            p.box('Paint', fx, fx + 0.14, min(0.8 * s, 0.88 * s), max(0.8 * s, 0.88 * s), TUBE_Z - 0.06, top + 0.08, bevel=0.015)
    # a faixa azul segmentada nos dois lados do berço, com as juntas das placas
    for s in (-1, 1):
        yo = (0.88 + 0.004) * s
        for (a, b) in ((x0 + 0.35, x0 + 1.15), (x0 + 1.3, x0 + 2.1), (x0 + 2.25, x0 + 3.05)):
            p.box('Team', a, b, min(yo, yo + 0.012 * s), max(yo, yo + 0.012 * s), POD_Z0 + 0.14, POD_Z0 + 0.25, bevel=0.0)
        for x in (x0 + 1.22, x0 + 2.17, x0 + 3.12):
            p.box('Dark', x - 0.01, x + 0.01, min(yo, yo + 0.008 * s), max(yo, yo + 0.008 * s), POD_Z0 + 0.1, TUBE_Z - 0.08, bevel=0.0)
        p.box('Dark', bx0 + 0.05, bx1 - 0.05, min(yo, yo + 0.008 * s), max(yo, yo + 0.008 * s), TUBE_Z - 0.13, TUBE_Z - 0.11, bevel=0.0)
        # o pino da dobradiça no berço
        p.cyl('Dark', (HINGE[0], 0.91 * s, HINGE[1]), 'y', 0.13, 0.06, seg=16, bevel=0.0)
    return p.finish()


def noses():
    """(23/10, v4) As pontas dos 2 mísseis saindo da boca, uma malha por tubo (nose_001 no y −, nose_002 no y + do desenho; no
    jogo o Y espelha, e ele casa cada uma com o tubo mais perto): o jogo esconde a do tubo que atirou e mostra de novo na recarga
    da rajada. Nas coordenadas do veículo como o Pod (deitadas): levantam juntas na dobradiça."""
    objs = []
    for n, s in enumerate((-1, 1), start=1):
        p = Part(f'nose_{n:03d}')
        nose(p, POD_X[1] + 0.005, TUBE_Y * s, TUBE_Z)
        objs.append(p.finish())
    return objs


def sweep_check(h, t, pd, ns=()):
    """O lançador girando não pode atravessar o casco (cabine, chaminé, filtro, lona, painel): testa a colisão de
    verdade (BVH, triângulo contra triângulo) do Turret + Pod (+ as pontas dos mísseis, ns) contra o Hull a cada 5° de giro,
    deitado e na pose de tiro."""
    def tree(o, M):
        vs = [M @ v.co for v in o.data.vertices]
        return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])
    hull_t = tree(h, Matrix.Identity(4))
    piv = V(PIVOT[0], PIVOT[1], 0.0)
    lift = (Matrix.Translation(Vector((HINGE[0], 0.0, HINGE[1]))) @ Matrix.Rotation(math.radians(-FIRE_PITCH), 4, 'Y')
            @ Matrix.Translation(Vector((-HINGE[0], 0.0, -HINGE[1]))))
    for label, pod_m in (('deitado', Matrix.Identity(4)), ('pose de tiro', lift)):
        hits = []
        for k in range(72):
            Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
            pairs = [('T', q) for q in hull_t.overlap(tree(t, Y))] + [('P', q) for o in (pd,) + tuple(ns) for q in hull_t.overlap(tree(o, Y @ pod_m))]
            if pairs:
                # onde bate: a caixa das faces do casco atingidas (coordenadas do desenho) e qual peça
                cs = [h.data.polygons[i].center for _, (i, j) in pairs]
                box = tuple(round(f(c[ax] * (-1 if ax == 1 else 1) for c in cs), 2) for ax in range(3) for f in (min, max))
                hits.append((5 * k, len(pairs), ''.join(sorted(set(w for w, _ in pairs))), box))
        print('MODEL giro (%s): %s' % (label, 'sem colisao em 360 graus' if not hits else 'COLISAO em'))
        for hit in hits:
            print('MODEL   ', hit)


def fire_pose(t, pd, ns=()):
    """Turret + Pod (+ as pontas dos mísseis, ns) com o tubo levantado FIRE_PITCH na dobradiça: só pra prévia (fica em pose/,
    fora da importação)."""
    objs = []
    for o in (t, pd) + tuple(ns):
        c = o.copy()
        c.data = o.data.copy()
        bpy.context.scene.collection.objects.link(c)
        objs.append(c)
    hx, hz = HINGE[0] * SCALE, HINGE[1] * SCALE
    M = Matrix.Translation(Vector((hx, 0.0, hz))) @ Matrix.Rotation(math.radians(-FIRE_PITCH), 4, 'Y') @ Matrix.Translation(Vector((-hx, 0.0, -hz)))
    for o in objs[1:]:
        o.data.transform(M)
    select_only(objs[0])
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = PREFIX + '_Launcher_Fire'
    return o


def main():
    clear_scene()
    h = hull()
    ws = wheels()
    t = turret()
    pd = pod()
    ns = noses()
    sweep_check(h, t, pd, ns)
    for o in [h, t, pd] + ws + ns:
        o.data.transform(Matrix.Scale(SCALE, 4))
    total = export(h, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    total += export(t, OUT)
    total += export(pd, OUT)
    total += export_group(ns, OUT, PREFIX + '_Noses')
    export(fire_pose(t, pd, ns), POSE)
    print('MODEL total', total, 'tris; escala', SCALE)
    k = SCALE
    nose_x = (POD_X[1] + NOSE_L) * k
    print('MODEL turret_pivot_m', [round(PIVOT[0] * k, 3), round(PIVOT[1] * k, 3)])
    print('MODEL pod_hinge_m (x, z; eixo Y)', [round(HINGE[0] * k, 3), round(HINGE[1] * k, 3)], 'fire_pitch_deg', FIRE_PITCH)
    print('MODEL tubos: centro a z %.3f m, y +-%.3f m; boca a x %.3f m, ponta do missil a x %.3f m (%.3f m a frente do pivo)'
          % (TUBE_Z * k, TUBE_Y * k, POD_X[1] * k, nose_x, nose_x - PIVOT[0] * k))
    a = math.radians(FIRE_PITCH)
    dx, dz = POD_X[1] + NOSE_L - HINGE[0], TUBE_Z - HINGE[1]
    fx = HINGE[0] + dx * math.cos(a) - dz * math.sin(a)
    fz = HINGE[1] + dx * math.sin(a) + dz * math.cos(a)
    print('MODEL pose de tiro: ponta do missil a x %.3f m, z %.3f m (%.3f m a frente do pivo)' % (fx * k, fz * k, (fx - PIVOT[0]) * k))


main()
