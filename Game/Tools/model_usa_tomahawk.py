"""O lançador de mísseis de cruzeiro da USA (usa_tomahawk), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_tomahawk.py, aprovada em 23/10) fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_tomahawk.py C:/Users/luisf/explosion/captures/game/models/usa_tomahawk_v2
e depois a pose de tiro da prévia (o lançador a 45°, montado dos GLB já cozidos; pose/ não vai pro jogo):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_usa_tomahawk.py -- \
      C:/Users/luisf/explosion/captures/game/models/usa_tomahawk_v2 pose

O pedido (01/11): "dá essa melhorada brutal [...] inclusive o de atirar longe [...] nada de quinas, nada de quadrado, nada de lego".
Não existe ficha de 5 vistas do Tomahawk (o botão do HUD, docs/concept/hud/kit/photos/buttons/usa_rocket_launcher.png, é um
caminhão genérico): o alvo é o desenho aprovado (o A: os DOIS TUBOS GROSSOS deitados nas costas, o berço blindado com a faixa azul
segmentada, os anéis azuis, as pontas dos mísseis na boca, os macacos amarelos atrás), a raiz da USA (a ficha do tanque: o cinza
claro, a cor do jogador em detalhes pequenos) e o caminhão de verdade da medida: o HEMTT M985 (8×8, os eixos em 2 pares, a cabine
de frente chata com o para-brisa de 2 vidros e a viseira, a grade grande embaixo do para-brisa, o para-choque grosso com o guincho, a
chaminé e o filtro de ar atrás da cabine, a escada da porta, os retrovisores grandes nos braços de tubo). Escala de jogo (o tanque
domina): o mesmo tamanho do A (≈ 5,2 × 2,1 m; o HEMTT de verdade × 0,72 no comprimento seria 7,3 m), os pneus do HEMTT
(16.00R20, 1,25 m) × 0,64 = 0,80 m.

Pivôs MANTIDOS (Units.json usa_tomahawk; os números são os do A, já na escala final): turret_pivot_m (−1,296; 0),
launcher.hinge_m (−2,304; 0; 1,404), launcher.tubes_m (0,072; ±0,302; 1,699) = a boca de cada tubo deitado, muzzle_height_m 1,7.
Muda (dito no relatório): 8 rodas no lugar de 6 (wheel_001..008) e o pneu de 0,30 m (wheels.tyre_w_m era 0,274).

Peças (origem no chão, no centro do chassi, frente em +X; todas nas coordenadas do veículo; os NOMES do A):
  SM_USA_Tomahawk_Hull    o chassi, a cabine, o motor, o convés, o anel da mesa, os macacos;
  SM_USA_Tomahawk_Wheels  8 malhas wheel_001..008 (o jogo gira cada uma pelo centro da caixa dela);
  SM_USA_Tomahawk_Turret  a mesa giratória, o pedestal, as vigas até a dobradiça e os motores de elevação (gira em Z no PIVOT);
  SM_USA_Tomahawk_Pod     o berço blindado com os 2 tubos, DEITADOS (levanta em torno de HINGE, eixo Y);
  SM_USA_Tomahawk_Noses   nose_001 (y −) e nose_002 (y +): as pontas dos mísseis na boca (levantam com o Pod; o jogo esconde a
                          do tubo que atirou).
Slots (os do A): Paint, Team, Dark, Metal, Rubber, Lens, Light, Yellow.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group, select_only  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, solid, tris  # noqa: E402

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.getcwd(), 'usa_tomahawk_v2')
MODE = ARGS[1] if len(ARGS) > 1 else 'model'
os.makedirs(OUT, exist_ok=True)
POSE = os.path.join(OUT, 'pose')
os.makedirs(POSE, exist_ok=True)
PREFIX = 'SM_USA_Tomahawk'

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Yellow': (214, 164, 32), 'Dark': (40, 40, 38),
               'Metal': (95, 95, 92), 'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170)})

# ---------------------------------------------------------------------------------- pivôs mantidos (escala final, m)
PIVOT = (-1.296, 0.0)            # turret_pivot_m
HINGE = (-2.304, 1.404)          # launcher.hinge_m (x, z), eixo Y
TUBE_Z, TUBE_Y, TUBE_R = 1.6992, 0.3024, 0.2592
POD_X = (-2.664, 0.072)          # os tubos, da traseira à boca (a boca = launcher.tubes_m)
NOSE_TIP = 0.432                 # a ponta do míssil deitado (a do A)
NOSE_R = 0.18
FIRE_PITCH = 45.0
# ---------------------------------------------------------------------------------- o caminhão (HEMTT na escala do jogo)
FRONT, REAR = 2.40, -2.62        # o para-choque da frente e o de trás
DECK_Z = 0.936                   # o convés do lançador (o do A)
DECK_X = (-2.60, 0.30)
DECK_HW = 1.0
WHEEL_R, WHEEL_W, WHEEL_Y = 0.40, 0.30, 0.86
AXLES_X = [1.80, 0.70, -1.20, -2.10]     # 8×8: 2 pares (o HEMTT), a escada da porta entre os da frente
FRAME_Z = (0.50, 0.70)
CAB_X = (0.62, 2.32)             # a cabine por cima dos 2 eixos da frente (a de trás fica fora do giro do lançador com as pontas)
CAB_HW = 1.0
CAB_Z = (0.88, 1.93)
RING_R = 0.7056                  # o anel da mesa no convés


def tube(p, m, a, b, r, seg=10):
    vp.tube(p, m, [a, b], r, seg=seg)


# ---------------------------------------------------------------------------------- casco: chassi, cabine, convés
def cab_shell(p):
    """A cabine do HEMTT: a frente chata com a grade, o para-brisa inclinado, a viseira, o teto; os cantos da frente quebrados em
    planta (a cabine blindada). O sólido convexo pelos cantos: placas planas, o chanfro redondo do harden nas quinas."""
    x0, x1 = CAB_X
    z0, z1 = CAB_Z
    pts = []
    for s in (-1, 1):
        pts += [(x0, s * CAB_HW, z0), (x0, s * CAB_HW, z1), (x1 - 0.06, s * (CAB_HW - 0.12), z0), (x1 - 0.12, s * CAB_HW, z0),
                (x1, s * (CAB_HW - 0.14), 1.30), (x1 - 0.12, s * CAB_HW, 1.30), (x1 - 0.17, s * (CAB_HW - 0.08), 1.78),
                (x1 - 0.22, s * CAB_HW, 1.80), (x1 - 0.26, s * (CAB_HW - 0.06), z1), (x0, s * CAB_HW, z1)]
    return solid(p, 'Paint#shell', pts)


def windshield_quad(s):
    """Os 4 cantos de um dos vidros do para-brisa (s = o lado), na face inclinada da frente (de 1,32 a 1,76)."""
    x1 = CAB_X[1]

    def P(y, z):
        # a face inclinada: de (x1, 1.30) a (x1 − 0.17, 1.78); 1,5 cm pra fora
        t = (z - 1.30) / 0.48
        return Vector((x1 - 0.17 * t + 0.012, y, z))
    ya, yb = (0.05, 0.78) if s > 0 else (-0.78, -0.05)
    return P(ya, 1.36), P(yb, 1.36), P(ya, 1.74), P(yb, 1.74)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.06)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    p.bm('Light', 0.0)
    p.bm('Dark#plate', 0.02)
    p.bm('Paint#fender', 0.015)
    # ---------------------------------------------------------------- o chassi: as longarinas, as travessas, os eixos
    for y in (-0.42, 0.42):
        vp.rbox(p, 'Dark#plate', (-0.12, y, (FRAME_Z[0] + FRAME_Z[1]) / 2), (4.9, 0.12, FRAME_Z[1] - FRAME_Z[0]), r=0.03, k=2)
    for x in (-2.4, -1.65, -0.75, 0.2, 1.25, 2.1):
        vp.rbox(p, 'Dark', (x, 0.0, 0.62), (0.08, 0.84, 0.12), r=0.02, k=2)
    for x in AXLES_X:                                   # os eixos e os diferenciais (a barriga de baixo)
        tube(p, 'Dark', (x, -0.72, WHEEL_R), (x, 0.72, WHEEL_R), 0.06, seg=12)
        vp.lathe(p, 'Dark', (x, 0.0, WHEEL_R), 'y', [(0.0, -0.17), (0.12, -0.17), (0.17, -0.1), (0.17, 0.1), (0.12, 0.17), (0.0, 0.17)], seg=14)
        for s in (-1, 1):                               # as molas (o feixe)
            vp.rbox(p, 'Dark', (x, 0.42 * s, WHEEL_R + 0.1), (0.7, 0.09, 0.07), r=0.02, k=2)
    # ---------------------------------------------------------------- a cabine
    cab_shell(p)
    x0, x1 = CAB_X
    # o para-brisa de 2 vidros com a moldura e o montante do meio, a viseira por cima
    for s in (-1, 1):
        A1, B1, A2, B2 = windshield_quad(s)
        b = p.bm('Lens#vp', 0.0)
        q = [A1, B1, B2, A2]
        n = (B1 - A1).cross(A2 - A1).normalized()
        if n.x < 0:
            n = -n
        lo = [b.verts.new(V(*(v))) for v in q]
        hi = [b.verts.new(V(*(v + n * 0.01))) for v in q]
        b.faces.new(lo)
        b.faces.new(hi[::-1])
        for k in range(4):
            b.faces.new([lo[k], lo[(k + 1) % 4], hi[(k + 1) % 4], hi[k]])
        # a moldura de borracha em volta do vidro
        vp.tube(p, 'Dark', [A1 + n * 0.012, B1 + n * 0.012, B2 + n * 0.012, A2 + n * 0.012, A1 + n * 0.012], 0.014, seg=6)
    vp.rbox(p, 'Paint#plate', (x1 - 0.2, 0.0, 1.83), (0.22, 2.02, 0.05), pitch=-10, r=0.04, k=4)          # a viseira
    # a grade grande embaixo do para-brisa (as aletas na moldura), os faróis de cima e as setas
    vp.rbox(p, 'Dark', (x1 + 0.005, 0.0, 1.12), (0.02, 1.3, 0.3), r=0.04, k=2)
    for k in range(6):
        vp.rbox(p, 'Paint#thin', (x1 + 0.02, 0.0, 1.0 + k * 0.048), (0.025, 1.24, 0.022), pitch=-20, r=0.01, k=1)
    for s in (-1, 1):
        vp.tube(p, 'Paint', [(x1 + 0.02, 0.66 * s, 0.97), (x1 + 0.02, 0.66 * s, 1.27)], 0.025, seg=8)
        vp.lathe(p, 'Metal', (x1 - 0.01, 0.8 * s, 1.18), 'x', [(0.0, -0.02), (0.06, -0.02), (0.075, 0.0), (0.075, 0.035), (0.065, 0.045),
                                                               (0.06, 0.04), (0.0, 0.04)], seg=14)
        vp.lathe(p, 'Light', (x1 - 0.01, 0.8 * s, 1.18), 'x', [(0.0, 0.048), (0.05, 0.046), (0.06, 0.04), (0.0, 0.04)], seg=12)
        vp.rbox(p, 'Yellow', (x1 - 0.01, 0.93 * s, 1.18), (0.03, 0.06, 0.08), r=0.015, k=1)                    # a seta
    # as portas: o recorte (a fresta), a janela, o painel azul (cor do jogador), a maçaneta, a dobradiça
    for s in (-1, 1):
        yo = CAB_HW * s
        n = Vector((0, s, 0))
        for (a, b) in (((1.02, yo, 0.96), (1.02, yo, 1.84)), ((1.86, yo, 0.96), (1.86, yo, 1.84)), ((1.02, yo, 0.96), (1.86, yo, 0.96))):
            vp.panel_seam(p, 'Dark', a, b, n, w=0.02, d=0.01)
        vp.rbox(p, 'Lens', (1.44, yo + 0.006 * s, 1.58), (0.66, 0.3, 0.012), roll=90, r=0.05, k=3)
        vp.rbox(p, 'Dark', (1.44, yo + 0.003 * s, 1.58), (0.72, 0.36, 0.008), roll=90, r=0.06, k=3)
        vp.rbox(p, 'Team', (1.44, yo + 0.005 * s, 1.18), (0.66, 0.26, 0.008), roll=90, r=0.04, k=3)
        vp.rbox(p, 'Metal', (1.74, yo + 0.02 * s, 1.36), (0.12, 0.03, 0.035), r=0.012, k=1)
        for z in (1.05, 1.72):
            vp.hinge(p, 'Metal', (1.03, yo + 0.02 * s, z), 'z', 0.12, r=0.018, knuckles=2)
        # a escada da porta (2 degraus de chapa furada entre os eixos da frente) e a alça de subir
        for zz in (0.42, 0.68):
            vp.rbox(p, 'Paint#plate', (1.25, (CAB_HW - 0.05) * s, zz), (0.24, 0.2, 0.03), r=0.03, k=2)
            vp.rbox(p, 'Dark', (1.25, (CAB_HW - 0.05) * s, zz + 0.017), (0.18, 0.14, 0.006), r=0.02, k=2)
        for xx in (1.13, 1.37):
            tube(p, 'Metal', (xx, (CAB_HW - 0.05) * s + 0.09 * s, 0.4), (xx, (CAB_HW - 0.05) * s + 0.09 * s, 0.9), 0.016, seg=6)
        vp.grab_handle(p, 'Metal', (1.92, yo, 1.0), (1.92, yo, 1.5), n, height=0.07, r=0.015)
        # os para-lamas da frente (o arco sobre as 2 rodas da frente, presos embaixo da cabine) e o para-barro
        for xw in AXLES_X[:2]:
            # a chapa curva do para-lama (o arco com 3 cm de espessura, 40 cm de largura)
            outer = vp.arc_pts((xw, 0.0, WHEEL_R), (1, 0, 0), (0, 0, 1), WHEEL_R + 0.11, 20, 160, 10)
            inner = vp.arc_pts((xw, 0.0, WHEEL_R), (1, 0, 0), (0, 0, 1), WHEEL_R + 0.08, 20, 160, 10)
            rings = []
            for qo, qi in zip(outer, inner):
                rings.append([(qo.x, (WHEEL_Y - 0.2) * s, qo.z), (qo.x, (WHEEL_Y + 0.2) * s, qo.z), (qi.x, (WHEEL_Y + 0.2) * s, qi.z),
                              (qi.x, (WHEEL_Y - 0.2) * s, qi.z)])
            p.loft('Paint#fender', rings)
        # os retrovisores grandes nos braços de tubo dobrado (o HEMTT)
        arm = vp.bent_bar([(x1 - 0.1, CAB_HW * s, 1.66), (x1 - 0.1, (CAB_HW + 0.14) * s, 1.66), (x1 - 0.1, (CAB_HW + 0.16) * s, 1.84)],
                          R=0.06, n=3)
        vp.tube(p, 'Dark', arm, 0.018, seg=6)
        tube(p, 'Dark', (x1 - 0.1, CAB_HW * s, 1.36), (x1 - 0.1, (CAB_HW + 0.16) * s, 1.6), 0.016, seg=6)
        vp.rbox(p, 'Dark', (x1 - 0.12, (CAB_HW + 0.17) * s, 1.72), (0.05, 0.18, 0.3), r=0.04, k=3)
        vp.rbox(p, 'Lens', (x1 - 0.095, (CAB_HW + 0.17) * s, 1.72), (0.006, 0.14, 0.25), r=0.03, k=2)
    # o teto: a escotilha redonda, a caixa do sensor, as antenas, as alças, a luz de teto
    vp.hatch_round(p, (1.28, -0.35, CAB_Z[1]), 0.26, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.rbox(p, 'Paint#plate', (1.62, 0.5, CAB_Z[1] + 0.12), (0.46, 0.4, 0.24), r=0.06, k=4)
    vp.rbox(p, 'Lens', (1.855, 0.5, CAB_Z[1] + 0.13), (0.012, 0.28, 0.12), r=0.02, k=2)
    for (ax, ay, h) in ((0.82, 0.78, 1.0), (0.82, 0.5, 1.35)):
        vp.lathe(p, 'Dark', (ax, ay, CAB_Z[1]), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.05), (0.035, 0.06), (0.03, 0.14), (0.0, 0.15)], seg=10)
        p.cyl('Dark#vp', (ax, ay, CAB_Z[1] + 0.15 + h / 2), 'z', 0.008, h, seg=6, bevel=0.0)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (0.9, 0.85 * s, CAB_Z[1]), (1.3, 0.85 * s, CAB_Z[1]), (0, 0, 1), height=0.06, r=0.014)
    # ---------------------------------------------------------------- o motor atrás da cabine (baixo: o lançador gira por cima),
    # a chaminé (esquerda) e o filtro de ar (direita) nos cantos, fora do giro com as pontas dos mísseis
    vp.rbox(p, 'Paint#plate', (0.46, 0.0, 1.12), (0.32, 1.3, 0.5), r=0.06, k=4)
    for k in range(5):
        vp.rbox(p, 'Dark', (0.295, 0.0, 0.94 + k * 0.08), (0.02, 1.1, 0.04), pitch=-30, r=0.01, k=1)
    ex, ey = 0.45, -0.86
    vp.lathe(p, 'Dark', (ex, ey, 0.92), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.95), (0.075, 1.0), (0.0, 1.0)], seg=12)
    vp.lathe(p, 'Metal', (ex, ey, 1.25), 'z', [(0.09, 0.0), (0.105, 0.02), (0.105, 0.48), (0.09, 0.5), (0.085, 0.5), (0.085, 0.0)],
             seg=14, closed=True)                                                               # a capa de calor
    for k in range(6):                                                                          # os furos da capa (de fora)
        vp.rbox(p, 'Dark', (ex, ey - 0.104, 1.3 + k * 0.075), (0.07, 0.012, 0.03), r=0.005, k=1)
    elbow = vp.bent_bar([(ex, ey, 1.9), (ex, ey, 2.0), (ex - 0.14, ey, 2.06)], R=0.07, n=4)
    vp.tube(p, 'Dark', elbow, 0.07, seg=12)
    for z in (1.1, 1.62):
        tube(p, 'Metal', (ex + 0.06, ey, z), (CAB_X[0] + 0.01, ey + 0.06, z), 0.018, seg=6)
    fx, fy = 0.45, 0.82
    vp.lathe(p, 'Paint', (fx, fy, 0.95), 'z', [(0.0, 0.0), (0.14, 0.0), (0.15, 0.02), (0.15, 0.62), (0.13, 0.66), (0.08, 0.68),
                                               (0.08, 0.74), (0.0, 0.74)], seg=18)
    vp.lathe(p, 'Dark', (fx, fy, 1.69), 'z', [(0.0, 0.0), (0.13, 0.0), (0.16, 0.02), (0.15, 0.05), (0.06, 0.07), (0.0, 0.07)], seg=18)
    for z in (1.1, 1.45):
        vp.lathe(p, 'Metal', (fx, fy, z), 'z', [(0.145, -0.02), (0.162, -0.02), (0.162, 0.02), (0.145, 0.02)], seg=18, closed=True)
    # ---------------------------------------------------------------- o para-choque da frente, o guincho, a grade de canos
    vp.rbox(p, 'Paint#plate', (FRONT - 0.07, 0.0, 0.7), (0.16, 2.06, 0.28), r=0.06, k=4)
    vp.lathe(p, 'Dark', (FRONT + 0.03, 0.0, 0.68), 'y', [(0.0, -0.3), (0.09, -0.3), (0.12, -0.27), (0.12, 0.27), (0.09, 0.3), (0.0, 0.3)], seg=16)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (FRONT + 0.03, 0.36 * s, 0.68), (0.12, 0.06, 0.26), r=0.02, k=2)
        # as manilhas de reboque, os faróis baixos com a guarda e o pisca
        for d in (-1, 1):
            vp.rbox(p, 'Metal', (FRONT + 0.03, 0.62 * s + d * 0.05, 0.62), (0.1, 0.03, 0.12), r=0.02, k=2)
        p.torus('Dark#vp', (FRONT + 0.08, 0.62 * s, 0.56), 'y', 0.05, 0.016, seg=12, rseg=6)
        vp.lathe(p, 'Metal', (FRONT + 0.01, 0.88 * s, 0.76), 'x', [(0.0, -0.03), (0.07, -0.03), (0.085, 0.0), (0.085, 0.04), (0.07, 0.05),
                                                                  (0.0, 0.05)], seg=14)
        vp.lathe(p, 'Light', (FRONT + 0.01, 0.88 * s, 0.76), 'x', [(0.0, 0.056), (0.06, 0.054), (0.07, 0.05), (0.0, 0.05)], seg=12)
    # a grade de canos (a cara dos caminhões do exército): o tubo dobrado em volta da grade e dos faróis, presa no para-choque
    gx = FRONT + 0.1
    for s in (-1, 1):
        vp.tube(p, 'Metal', vp.bent_bar([(FRONT - 0.02, 0.98 * s, 0.84), (gx, 0.98 * s, 0.84), (gx, 0.98 * s, 1.36),
                                         (gx, 0.5 * s, 1.36), (gx, 0.5 * s, 0.84)], R=0.08, n=4), 0.024, seg=8)
    for z in (1.0, 1.18):
        tube(p, 'Metal', (gx, -0.98, z), (gx, 0.98, z), 0.02, seg=8)
    tube(p, 'Metal', (gx, -0.5, 1.36), (gx, 0.5, 1.36), 0.024, seg=8)
    # ---------------------------------------------------------------- o convés do lançador
    dx0, dx1 = DECK_X
    vp.rbox(p, 'Paint#plate', ((dx0 + dx1) / 2, 0.0, DECK_Z - 0.04), (dx1 - dx0, 2 * DECK_HW, 0.08), r=0.06, k=4)
    for s in (-1, 1):
        # a viga de lado (a borda do convés, por cima das rodas de trás) com a faixa azul segmentada e os parafusos
        vp.rbox(p, 'Paint#plate', ((dx0 + dx1) / 2, (DECK_HW - 0.04) * s, DECK_Z - 0.12), (dx1 - dx0, 0.1, 0.18), r=0.05, k=4)
        yo = (DECK_HW + 0.012) * s
        for (a, b) in ((-0.72, -0.08), (-0.02, 0.24)):
            vp.rbox(p, 'Team', ((a + b) / 2, yo, DECK_Z - 0.12), (b - a, 0.008, 0.1), r=0.02, k=2)
        vp.bolt_row(p, 'Metal', (dx0 + 0.1, (DECK_HW + 0.01) * s, DECK_Z - 0.05), (dx1 - 0.1, (DECK_HW + 0.01) * s, DECK_Z - 0.05), 16,
                    (0, s, 0), r=0.016, h=0.012)
        # as caixas de lado entre os pares de eixos (o tanque de combustível à esquerda, a de ferramentas à direita)
        if s < 0:
            vp.lathe(p, 'Paint', (-0.25, -0.8, 0.66), 'x', [(0.0, -0.45), (0.17, -0.45), (0.2, -0.42), (0.2, 0.42), (0.17, 0.45), (0.0, 0.45)], seg=20)
            for xx in (-0.5, 0.0):
                vp.lathe(p, 'Dark', (xx, -0.8, 0.66), 'x', [(0.195, -0.025), (0.215, -0.025), (0.215, 0.025), (0.195, 0.025)], seg=20, closed=True)
            vp.lathe(p, 'Metal', (-0.25, -0.8, 0.875), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.03), (0.0, 0.03)], seg=10)
        else:
            vp.rbox(p, 'Paint#plate', (-0.25, 0.82, 0.66), (0.86, 0.32, 0.38), r=0.05, k=4)
            vp.rbox(p, 'Paint#thin', (-0.25, 0.985, 0.66), (0.8, 0.012, 0.32), roll=0, r=0.03, k=2)
            for xx in (-0.5, 0.0):
                vp.rbox(p, 'Metal', (xx, 0.99, 0.78), (0.06, 0.02, 0.05), r=0.01, k=1)
        # os para-barros atrás de cada par
        for xw in (AXLES_X[1], AXLES_X[3]):
            vp.mudflap(p, (xw - WHEEL_R - 0.06, WHEEL_Y * s, DECK_Z - 0.1), 0.32, 0.36, yaw=0, m='Rubber')
        # a passarela dos 2 lados do anel: a chapa antiderrapante escura na moldura de quina redonda (de cima lê como a faixa
        # escura do A; as ripas claras liam como uma esteira de tanque)
        vp.rbox(p, 'Paint#thin', (-1.3, 0.86 * s, DECK_Z + 0.006), (2.24, 0.26, 0.012), r=0.04, k=3)
        vp.rbox(p, 'Dark', (-1.3, 0.86 * s, DECK_Z + 0.013), (2.16, 0.18, 0.006), r=0.03, k=3)
        for k in range(3):
            vp.rbox(p, 'Paint#thin', (-2.0 + k * 0.7, 0.86 * s, DECK_Z + 0.016), (0.03, 0.2, 0.008), r=0.006, k=1)
    # o anel da mesa giratória
    vp.lathe(p, 'Dark', (PIVOT[0], PIVOT[1], DECK_Z), 'z', [(RING_R - 0.08, 0.0), (RING_R + 0.03, 0.0), (RING_R + 0.03, 0.025),
                                                           (RING_R + 0.01, 0.034), (RING_R - 0.08, 0.034)], seg=48, closed=True)
    # o painel de tiro (esquerda, na frente do convés): baixo, por baixo do giro do lançador
    vp.rbox(p, 'Paint#plate', (-0.02, -0.8, DECK_Z + 0.09), (0.46, 0.28, 0.18), r=0.04, k=4)
    for k in range(4):
        vp.rbox(p, 'Dark', (-0.2 + k * 0.11, -0.945, DECK_Z + 0.09), (0.06, 0.008, 0.09), r=0.01, k=1)
    vp.rbox(p, 'Lens', (0.215, -0.8, DECK_Z + 0.1), (0.006, 0.16, 0.06), r=0.01, k=1)
    # a lona azul enrolada (o painel de identificação aérea) na frente-direita do convés, baixa
    vp.tarp_roll(p, (-0.1, 0.62, DECK_Z + 0.1), 'x', 0.66, 0.1, m='Team', straps=2, strap_m='Dark', buckle_m='Metal')
    # ---------------------------------------------------------------- os macacos estabilizadores amarelos nos cantos de trás
    for s in (-1, 1):
        jy = 1.12 * s
        vp.rbox(p, 'Paint#plate', (REAR + 0.1, 1.02 * s, DECK_Z - 0.16), (0.26, 0.3, 0.14), r=0.04, k=3)   # o suporte
        vp.lathe(p, 'Yellow', (REAR + 0.1, jy, 0.36), 'z', [(0.0, 0.0), (0.075, 0.0), (0.085, 0.02), (0.085, 0.5), (0.095, 0.52),
                                                            (0.095, 0.56), (0.0, 0.56)], seg=16)
        vp.lathe(p, 'Metal', (REAR + 0.1, jy, 0.2), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.17), (0.0, 0.17)], seg=12)
        vp.lathe(p, 'Dark', (REAR + 0.1, jy, 0.16), 'z', [(0.0, 0.0), (0.14, 0.0), (0.14, 0.025), (0.1, 0.045), (0.0, 0.05)], seg=16)
        for k in range(4):
            vp.lathe(p, 'Dark', (REAR + 0.1, jy, 0.48 + k * 0.1), 'z', [(0.085, -0.008), (0.09, -0.008), (0.09, 0.008), (0.085, 0.008)],
                     seg=16, closed=True)
    # ---------------------------------------------------------------- o para-choque de trás, a faixa de aviso, as lanternas, o engate
    vp.rbox(p, 'Paint#plate', (REAR + 0.06, 0.0, 0.66), (0.12, 1.86, 0.2), r=0.05, k=4)
    for k in range(8):
        y0 = -0.8 + k * 0.2
        vp.rbox(p, 'Yellow' if k % 2 == 0 else 'Dark', (REAR - 0.002, y0 + 0.1, 0.66), (0.01, 0.2, 0.14), r=0.0005, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (REAR + 0.0, 0.84 * s, 0.82), (0.05, 0.22, 0.12), r=0.02, k=2)
        vp.rbox(p, 'Light', (REAR - 0.03, 0.8 * s, 0.82), (0.012, 0.08, 0.07), r=0.015, k=1)
        vp.rbox(p, 'Lens', (REAR - 0.03, 0.9 * s, 0.82), (0.012, 0.08, 0.07), r=0.015, k=1)
    vp.rbox(p, 'Metal', (REAR - 0.08, 0.0, 0.62), (0.16, 0.16, 0.12), r=0.03, k=2)
    p.torus('Dark#vp', (REAR - 0.17, 0.0, 0.62), 'z', 0.06, 0.02, seg=14, rseg=6)
    return p.finish()


def wheels():
    """8 rodas (o HEMTT): o pneu de verdade (model_vehicle_parts.tyre: o flanco abaulado, os blocos em V subindo no ombro), a roda de
    aço rebaixada com os furos, o cubo e o anel de porcas. Uma malha por roda (o jogo gira pelo centro da caixa)."""
    objs = []
    n = 0
    for x in AXLES_X:
        for s in (-1, 1):
            n += 1
            w = Part(f'wheel_{n:03d}')
            vp.tyre(w, (x, WHEEL_Y * s, WHEEL_R), WHEEL_R, WHEEL_W, s, rim_r=0.235, rubber='Rubber', rim='Paint', hub='Metal', seg=32,
                    holes_m='Dark', style='mud', n_lugs=24)
            objs.append(w.finish())
    return objs


# ---------------------------------------------------------------------------------- a mesa e o berço (torre)
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('Paint#plate', 0.03)
    px, py = PIVOT
    hx, hz = HINGE
    # a mesa giratória (o rolamento com o anel de parafusos) e o motor do giro
    vp.lathe(p, 'Paint', (px, py, DECK_Z + 0.036), 'z', [(0.0, 0.0), (0.66, 0.0), (0.68, 0.02), (0.68, 0.09), (0.66, 0.11), (0.6, 0.12),
                                                       (0.0, 0.12)], seg=48)
    vp.bolt_ring(p, 'Metal', (px, py, DECK_Z + 0.156), 'z', 0.6, 24, r=0.016, h=0.012)
    vp.rbox(p, 'Dark', (px + 0.2, 0.55, DECK_Z + 0.22), (0.2, 0.14, 0.14), r=0.03, k=2)
    vp.lathe(p, 'Metal', (px + 0.2, 0.55, DECK_Z + 0.15), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.02), (0.0, 0.02)], seg=10)
    z0 = DECK_Z + 0.156
    zt = HINGE[1] - 0.014                      # o topo do pedestal (o fundo do berço deitado fica 1,4 cm acima)
    # o pedestal (a caixa de quina redonda) e as 2 vigas até a dobradiça
    vp.rbox(p, 'Paint#plate', (px - 0.02, 0.0, (z0 + zt) / 2), (1.1, 1.0, zt - z0), r=0.1, k=4)
    for s in (-1, 1):
        vp.rbox(p, 'Paint#plate', ((hx + px - 0.43) / 2, 0.36 * s, zt - 0.1), (px - 0.43 - hx, 0.17, 0.2), r=0.05, k=4)
        # a orelha da dobradiça (a chapa em perfil, quinas redondas) e o motor de elevação no eixo
        prof = [(hx - 0.144, zt - 0.23), (hx + 0.396, zt - 0.23), (hx + 0.396, zt - 0.13), (hx + 0.13, zt + 0.1), (hx - 0.144, zt + 0.1)]
        solid(p, 'Paint#plate', [(x, yy, z) for x, z in prof for yy in (0.677 * s, 0.72 * s)])
        vp.lathe(p, 'Dark', (hx, 0.72 * s, hz), (0, s, 0), [(0.0, 0.0), (0.14, 0.0), (0.145, 0.02), (0.145, 0.11), (0.12, 0.13), (0.0, 0.13)], seg=20)
        vp.lathe(p, 'Metal', (hx, 0.85 * s, hz), (0, s, 0), [(0.0, 0.0), (0.08, 0.0), (0.08, 0.035), (0.05, 0.045), (0.0, 0.045)], seg=14)
        vp.bolt_ring(p, 'Metal', (hx, 0.85 * s, hz), (0, s, 0), 0.115, 8, r=0.012, h=0.01)
        vp.rbox(p, 'Paint#plate', (hx + 0.13, 0.52 * s, zt - 0.2), (0.42, 0.34, 0.06), r=0.03, k=3)    # a ponte viga-orelha
        # a mangueira hidráulica descendo da orelha pro pedestal
        vp.tube(p, 'Dark', vp.bent_bar([(hx + 0.05, 0.64 * s, hz - 0.12), (hx + 0.3, 0.6 * s, zt - 0.25), (px - 0.6, 0.48 * s, z0 + 0.1)],
                                       R=0.1, n=3), 0.016, seg=6)
    vp.rbox(p, 'Dark', (hx + 0.08, 0.0, zt - 0.12), (0.17, 0.9, 0.13), r=0.03, k=2)                 # a travessa da dobradiça
    # o painel da frente do pedestal (as tampas com os parafusos)
    for s in (-1, 1):
        vp.rbox(p, 'Paint#thin', (px + 0.535, 0.24 * s, (z0 + zt) / 2), (0.02, 0.36, 0.16), r=0.03, k=2)
    vp.bolt_row(p, 'Metal', (px + 0.545, -0.4, zt - 0.03), (px + 0.545, 0.4, zt - 0.03), 6, (1, 0, 0), r=0.014, h=0.01)
    return p.finish()


def pod():
    """Os 2 tubos no berço blindado, deitados, a boca pra +X; levanta em torno de HINGE."""
    p = Part(PREFIX + '_Pod')
    p.bm('Paint#shell', 0.05)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    x0, x1 = POD_X
    bx0, bx1 = x0 + 0.072, x1 - 0.072
    z0 = HINGE[1]
    hw = 0.634
    # o berço blindado (a metade de baixo dos tubos), em placas: o sólido convexo pelos cantos
    prof = [(-hw + 0.06, z0), (hw - 0.06, z0), (hw, z0 + 0.06), (hw, TUBE_Z - 0.045), (hw - 0.05, TUBE_Z + 0.015),
            (-hw + 0.05, TUBE_Z + 0.015), (-hw, TUBE_Z - 0.045), (-hw, z0 + 0.06)]
    solid(p, 'Paint#shell', [(x, y, z) for (y, z) in prof for x in (bx0, bx1)])
    # os tubos: o perfil girado (o anel de trás, o corpo, o reforço da boca) com o FURO da boca (o fundo escuro: o tubo vazio
    # quando a ponta some) e as cintas
    for s in (-1, 1):
        ty = TUBE_Y * s
        st = [(x0, TUBE_R - 0.02), (x0 + 0.02, TUBE_R + 0.012), (x0 + 0.1, TUBE_R + 0.012), (x0 + 0.12, TUBE_R), (x1 - 0.16, TUBE_R),
              (x1 - 0.14, TUBE_R + 0.016), (x1 - 0.01, TUBE_R + 0.016), (x1, TUBE_R + 0.004)]
        vp.gun_barrel(p, st, TUBE_Z, y=ty, m='Paint', seg=36, bore=TUBE_R - 0.035, bore_depth=0.42, bore_m='Dark')
        # a tampa de trás (o disco do escape com as nervuras)
        vp.lathe(p, 'Dark', (x0 + 0.004, ty, TUBE_Z), (-1, 0, 0), [(0.0, 0.0), (TUBE_R - 0.03, 0.0), (TUBE_R - 0.03, 0.012), (0.0, 0.02)], seg=28)
        for k in range(2):                       # a cruz de travamento da tampa (as 6 nervuras de antes liam como turbina)
            a = math.pi * k / 2 + math.pi / 4
            vp.rbox(p, 'Metal', (x0 - 0.014, ty, TUBE_Z), (0.016, 2 * (TUBE_R - 0.05), 0.03), roll=math.degrees(a), r=0.008, k=1)
        vp.lathe(p, 'Metal', (x0 - 0.01, ty, TUBE_Z), (-1, 0, 0), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.04, 0.03), (0.0, 0.03)], seg=16)
        # as cintas de aço (o reforço) com os parafusos de cima, e os 2 anéis azuis perto da boca (como os do cano do tanque)
        for xx in (x0 + 0.38, (x0 + x1) / 2 - 0.1, x1 - 0.36):
            vp.ring_band(p, 'Dark', xx - 0.035, xx + 0.035, TUBE_R, TUBE_Z, y=ty, lip=0.014, seg=36)
            vp.nut(p, 'Metal', (xx, ty, TUBE_Z + TUBE_R + 0.014), (0, 0, 1), r=0.016, h=0.012)
        for xx in (x1 - 0.612, x1 - 0.778):
            vp.ring_band(p, 'Team', xx - 0.045, xx + 0.045, TUBE_R, TUBE_Z, y=ty, lip=0.008, seg=36)
        # o olhal de içar em cima de cada tubo e o conector do cabo umbilical atrás
        p.torus('Metal#vp', ((x0 + x1) / 2 + 0.5, ty + 0.1 * s, TUBE_Z + TUBE_R + 0.025), 'x', 0.04, 0.012, seg=12, rseg=6)
        vp.lathe(p, 'Metal', (x0 + 0.3, ty + 0.18 * s, TUBE_Z + 0.16), (0, s * 0.5, 1), [(0.0, 0.0), (0.04, 0.0), (0.04, 0.06), (0.0, 0.06)], seg=10)
    # a calha de cabos entre os tubos (a linha escura de cima)
    vp.rbox(p, 'Dark', ((x0 + x1) / 2, 0.0, TUBE_Z + 0.2), (x1 - x0 - 0.7, 0.07, 0.07), r=0.025, k=2)
    for k in range(6):
        vp.rbox(p, 'Metal', (x0 + 0.5 + k * 0.33, 0.0, TUBE_Z + 0.24), (0.03, 0.1, 0.02), r=0.008, k=1)
    # os quadros por cima dos tubos (frente, meio, trás): a ponte de cima e as pernas descendo até o berço, quinas redondas
    top = TUBE_Z + TUBE_R
    for fx in (x1 - 0.16, (x0 + x1) / 2 - 0.05, x0 + 0.1):
        vp.rbox(p, 'Paint#plate', (fx, 0.0, top + 0.04), (0.12, 1.28, 0.08), r=0.04, k=3)
        for s in (-1, 1):
            vp.rbox(p, 'Paint#plate', (fx, 0.6 * s, (TUBE_Z + top + 0.08) / 2 - 0.02), (0.12, 0.08, top + 0.08 - TUBE_Z + 0.04), r=0.03, k=3)
            vp.nut(p, 'Metal', (fx, 0.6 * s, top + 0.08), (0, 0, 1), r=0.018, h=0.012)
    # a faixa azul segmentada nos lados do berço, as juntas das placas e a fileira de parafusos
    for s in (-1, 1):
        yo = (hw + 0.004) * s
        for (a, b) in ((x0 + 0.25, x0 + 0.83), (x0 + 0.94, x0 + 1.52), (x0 + 1.63, x0 + 2.21)):
            vp.rbox(p, 'Team', ((a + b) / 2, yo, z0 + 0.12), (b - a, 0.008, 0.08), r=0.02, k=2)
        for x in (x0 + 0.885, x0 + 1.575, x0 + 2.265):
            vp.panel_seam(p, 'Dark', (x, hw * s, z0 + 0.06), (x, hw * s, TUBE_Z - 0.06), (0, s, 0), w=0.018, d=0.01)
        vp.bolt_row(p, 'Metal', (bx0 + 0.08, (hw + 0.004) * s, TUBE_Z - 0.09), (bx1 - 0.08, (hw + 0.004) * s, TUBE_Z - 0.09), 14,
                    (0, s, 0), r=0.015, h=0.01)
        # o pino da dobradiça no berço
        vp.lathe(p, 'Dark', (HINGE[0], hw * s, HINGE[1]), (0, s, 0), [(0.0, 0.0), (0.09, 0.0), (0.09, 0.035), (0.07, 0.045), (0.0, 0.045)], seg=16)
    return p.finish()


def nose(p, y):
    """A ponta do míssil saindo da boca: o cilindro que começa dentro do furo (10 cm), a ogiva (perfil de potência), o anel da emenda e
    o radome escuro na ponta. A ponta fica onde a do A ficava (NOSE_TIP)."""
    x_in = POD_X[1] - 0.10
    L = NOSE_TIP - x_in
    a0 = 0.10

    def r_at(t):
        return max(NOSE_R * (1.0 - t ** 1.7) ** 0.62, 0.006)
    og = [(r_at(k / 12), a0 + (L - a0) * k / 12) for k in range(13)]
    split = 7
    body = [(0.0, 0.0), (NOSE_R, 0.0)] + og[:split + 1] + [(0.0, og[split][1])]
    vp.lathe(p, 'Paint', (x_in, y, TUBE_Z), 'x', body, seg=24)
    tip = [(0.0, og[split][1] - 0.002), (og[split][0] + 0.001, og[split][1] - 0.002)] + og[split + 1:] + [(0.0, L)]
    vp.lathe(p, 'Dark', (x_in, y, TUBE_Z), 'x', tip, seg=24)
    t = 0.3
    vp.ring_band(p, 'Metal', x_in + a0 + (L - a0) * t - 0.008, x_in + a0 + (L - a0) * t + 0.008, r_at(t), TUBE_Z, y=y, lip=0.004, seg=24)


def noses():
    objs = []
    for n, s in enumerate((-1, 1), start=1):
        p = Part(f'nose_{n:03d}')
        nose(p, TUBE_Y * s)
        objs.append(p.finish())
    return objs


# ---------------------------------------------------------------------------------- conferência
def bvh(o, M):
    vs = [M @ v.co for v in o.data.vertices]
    return BVHTree.FromPolygons(vs, [tuple(q.vertices) for q in o.data.polygons])


def lift_matrix(pitch):
    return (Matrix.Translation(Vector((HINGE[0], 0.0, HINGE[1]))) @ Matrix.Rotation(math.radians(-pitch), 4, 'Y')
            @ Matrix.Translation(Vector((-HINGE[0], 0.0, -HINGE[1]))))


def sweep_check(hull_objs, t, pd, ns):
    """O lançador girando (deitado e na pose de tiro) não pode atravessar o casco: BVH contra BVH a cada 5°."""
    trees = [bvh(h, Matrix.Identity(4)) for h in hull_objs]
    piv = V(PIVOT[0], PIVOT[1], 0.0)
    for label, pm in (('deitado', Matrix.Identity(4)), ('pose de tiro', lift_matrix(FIRE_PITCH))):
        hits = []
        for k in range(72):
            Y = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(5 * k), 4, 'Z') @ Matrix.Translation(-piv)
            n = 0
            where = []
            for o, M in [(t, Y)] + [(q, Y @ pm) for q in [pd] + list(ns)]:
                tt = bvh(o, M)
                for hi, ht in enumerate(trees):
                    pairs = ht.overlap(tt)
                    n += len(pairs)
                    for (i, j) in pairs[:2]:
                        c = hull_objs[hi].data.polygons[i].center
                        where.append((o.name[-6:], round(c.x, 2), round(-c.y, 2), round(c.z, 2)))
            if n:
                hits.append((5 * k, n, where[:3]))
        print('MODEL giro (%s): %s' % (label, 'sem colisao em 360 graus' if not hits else 'COLISAO em %d posicoes' % len(hits)))
        for h in hits[:10]:
            print('MODEL   ', h)


def write_spec():
    import json
    parts = [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Wheels', '_Turret', '_Pod', '_Noses')]
    views = [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
             ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 820]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [900, 900]],
             ['back', 'ortho', [1, 0, 0], [0, 0, 1], [900, 900]],
             ['three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8],
             ['three_quarter_right', 'persp', 30.0, 40.0, [1600, 1000], 30.0, 0.8]]
    game = [['game_cam', 37.5, 35.0, 50.0, 0.8], ['game_cam_close', 37.5, 35.0, 28.0, 0.75]]
    spec = {'parts': parts, 'views': views, 'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': game}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)
    fire = {'parts': [[PREFIX + '_Hull', [0, 0, 0]], [PREFIX + '_Wheels', [0, 0, 0]],
                      ['captures/game/models/usa_tomahawk_v2/pose/' + PREFIX + '_Launcher_Fire', [0, 0, 0]]],
            'views': [['fire_left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 1000]],
                      ['fire_three_quarter', 'persp', 27.0, 232.0, [1600, 1000], 30.0, 0.8]],
            'usa_at': [0, 6.0, 0], 'tank_views': False, 'game': [['fire_game_cam_close', 37.5, 35.0, 28.0, 0.75]]}
    with open(os.path.join(OUT, 'preview_fire.json'), 'w', encoding='utf-8') as f:
        json.dump(fire, f, ensure_ascii=False, indent=1)


def make_pose():
    """A pose de tiro da prévia: o Turret, o Pod e as pontas JÁ COZIDOS (os GLB da pasta), o Pod e as pontas levantados FIRE_PITCH na
    dobradiça, numa malha só em pose/ (fora do glob da importação)."""
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    objs = []
    for n in ('_Turret', '_Pod', '_Noses'):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=os.path.join(OUT, PREFIX + n + '.glb'))
        new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
        bpy.context.view_layer.update()
        for o in new:
            o.data.transform(o.matrix_world)
            o.matrix_world = Matrix.Identity(4)
            if n != '_Turret':
                # (Blender: o eixo Y da dobradiça é o mesmo; o giro de −pitch em Y levanta a boca em +X)
                o.data.transform(lift_matrix(FIRE_PITCH))
        objs += new
    select_only(objs[0])
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = PREFIX + '_Launcher_Fire'
    path = os.path.join(POSE, o.name + '.glb')
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                              export_materials='EXPORT', export_normals=True, export_texcoords=True, export_vertex_color='ACTIVE',
                              export_all_vertex_colors=False)
    print('MODEL pose ->', path)


def main():
    clear_scene()
    h = hull()
    ws = wheels()
    t = turret()
    pd = pod()
    ns = noses()
    sweep_check([h] + ws, t, pd, ns)
    total = export(h, OUT)
    total += export_group(ws, OUT, PREFIX + '_Wheels')
    total += export(t, OUT)
    total += export(pd, OUT)
    total += export_group(ns, OUT, PREFIX + '_Noses')
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {h.name: tris(h), 'wheels': sum(tris(w) for w in ws), t.name: tris(t), pd.name: tris(pd),
                                  'noses': sum(tris(q) for q in ns)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, pd] + ws + ns)
    print('MODEL medidas: comprimento %.2f m (x %.2f..%.2f), largura %.2f m (com os retrovisores), altura %.2f m'
          % (ax1 - ax0, ax0, ax1, ay1 - ay0, az1))
    (bx0, bx1), (by0, by1), _ = bounds([h] + ws)
    print('MODEL casco + rodas: x %.2f..%.2f, y %.2f..%.2f' % (bx0, bx1, by0, by1))
    print('MODEL turret_pivot_m', list(PIVOT), 'hinge_m', [HINGE[0], 0.0, HINGE[1]], 'tubes_m', [[POD_X[1], -TUBE_Y, TUBE_Z], [POD_X[1], TUBE_Y, TUBE_Z]],
          'muzzle_height_m', round(TUBE_Z, 3))
    print('MODEL pneu: raio %.2f, largura %.2f (tyre_w_m), eixos %s, centro da roda y +-%.2f' % (WHEEL_R, WHEEL_W, AXLES_X, WHEEL_Y))
    a = math.radians(FIRE_PITCH)
    dx, dz = NOSE_TIP - HINGE[0], TUBE_Z - HINGE[1]
    print('MODEL pose de tiro: ponta do missil a x %.3f, z %.3f' % (HINGE[0] + dx * math.cos(a) - dz * math.sin(a),
                                                                  HINGE[1] + dx * math.sin(a) + dz * math.cos(a)))
    write_spec()


if MODE == 'pose':
    make_pose()
else:
    main()
