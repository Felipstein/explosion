"""A usina da USA (usa_power_plant), VERSÃO 3 (01/11/2026): a SUBESTAÇÃO com a TORRE DE ALTA TENSÃO. O estudo: docs/elenco/USA_CC_E_USINA.md.

A v2 foi reprovada (o Felipe, 01/11: "não tem cara de power plant"): o reator cilíndrico com anéis lia de cima como círculos
concêntricos (caixa d'água, silo, radar) e nada nela dizia ELETRICIDADE; e a torre de resfriamento com vapor é da China. Esta usa o
símbolo da eletricidade que todo mundo conhece e não é de ninguém: a torre de treliça com os braços, as cadeias de isoladores e os
cabos pendurados, e o pátio dos transformadores.
(01/11, 17h35, o Felipe: "a subestação eu até gostei, mas faltou mais um aspecto que defina que é um power plant") O microrreator em
contêineres (de longe, "máquina com ventiladores") saiu; entrou o DOMO DE CONTENÇÃO, a cúpula baixa e larga de concreto da usina
nuclear: o segundo ícone universal de usina (a torre de resfriamento é da China, a chaminé listrada da Rússia). A torre, os cabos, os
transformadores e a cerca ficaram como ele gostou.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_power_plant_v3.py C:/Users/luisf/explosion/captures/game/models/usa_power_plant_v3

A PALAVRA (CONCEITO_CONSTRUCOES.md §3b): A TORRE DE ALTA TENSÃO. De cima e na câmera do jogo: a treliça vazada de 11,8 m (a peça mais
alta do lote: a sombra dela desenha a torre no chão) com o braço largo e os 3 cabos descendo em catenária sobre o pátio dos
transformadores até o pórtico da frente; do lado, a CÚPULA de concreto com o anel de janelas azuis no pé. O que se mexe: a luz de
aviso no topo pisca, o anel do núcleo pulsa, o jogo corre o pulso azul nos 3 cabos (os pontos: lines.json).
  - a TORRE (a de "taça", a do tipo americano de linha simples): os 4 pés de cantoneira com as sapatas, o tronco que afina até a
    cintura com o travamento em X e as travessas, o V das 2 pernas que abre a janela, o braço em treliça triangular de 6 m, os 2
    picos do cabo-guarda com a luz de aviso, as 3 cadeias de ancoragem (os pratos de porcelana em pilha) apontadas pro pátio, a
    placa de perigo e os degraus;
  - os 3 CABOS (cada um um tubo contínuo: a catenária da torre até as cadeias do PÓRTICO da frente e o jumper descendo; o pórtico
    com as 2 colunas e a viga em treliça, os holofotes) até os SECCIONADORES (o suporte de aço, os 2 isoladores de coluna e a lâmina de cada fase, a manivela), o
    BARRAMENTO de tubo sobre os isoladores de apoio e os jumpers até as buchas;
  - os 2 TRANSFORMADORES no pátio: o tanque com as nervuras, a tampa aparafusada, as 3 BUCHAS de alta (os pratos empilhados), o
    RADIADOR de aletas com os coletores, o conservador no suporte com o cano, a caixa de cabos, a base com a bacia de óleo;
  - o pátio de CASCALHO (as pedras soltas sobre o fundo escuro) com o meio-fio, a CERCA de grade com as placas de perigo (o raio);
  - o DOMO DE CONTENÇÃO (frente, −Y: o lado da câmera): a fundação redonda (a base acompanha a forma), o ANEL DE JANELAS DO
    NÚCLEO no pé (16 janelas acesas entre os pilares: o slot Core, o jogo põe a luz azul), o cilindro de concreto com os 3
    contrafortes e os anéis de protensão discretos, a faixa azul, a galeria com o guarda-corpo e a escada com gaiola, a cornija, a
    CÚPULA com os anéis, a coroa com a luz de aviso, a ESCOTILHA DE EQUIPAMENTO (virada pra câmera) com o anel de parafusos e o
    patamar, o trifólio;
  - o DUTO DA TURBINA até a casa da turbina baixa de trás (as placas, a faixa azul, os ventiladores), e a CANALETA dos cabos até as
    caixas de cabos dos transformadores (os cabos grossos sobem dela).
A USA limpa (13/10): sem desgaste pintado.

Lote 9,28 × 12,64 m (x −4,64..4,64, y −6,32..6,32), a frente em +X; nada sai do lote. A torre em (−2,7; 2,85), o braço ao longo de Y.
Peças: SM_USA_PowerPlant_Base (tudo: nada gira), SM_USA_PowerPlant_Scaffold (o andaime em volta do domo). lines.json: os pontos
dos 3 cabos (da ponta da cadeia da torre ao seccionador), no quadro da Unreal.
Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime), Rubber (os cabos), Core (a
anel de janelas do núcleo; o jogo põe a luz azul nele).
O kit (CPart, Plan, poly_slab, ...) vem do model_usa_power_plant_v2.py (só a interface, nada do desenho).
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import (CPart, chamfer_rect, poly_slab, poly_band, slab_tiles, louvers, scaffold_poly, plan_core,  # noqa: E402
                                      plan_walls, plan_pillars, check, preview_spec, X, Y, Z)

PREFIX = 'SM_USA_PowerPlant'
COLORS.update({'Rubber': (32, 32, 31), 'Core': (150, 200, 255), 'PowerLine': (22, 22, 24)})   # (03/11) o cabo do pulso precisa da cor de conferência

HX, HY = 4.64, 6.32               # o lote 9,28 × 12,64
FLOOR = 0.12                      # o topo da laje do módulo
# a torre
TC = (-2.7, 2.85)                 # o centro
T_FOOT, T_WAIST, Z_WAIST = 1.2, 0.45, 7.0          # a meia largura no pé e na cintura, a altura da cintura
Z_ARM0, Z_ARM1 = 9.9, 10.6        # o braço: os banzos de baixo e o de cima
ARM = 3.0                         # a meia envergadura do braço
V_TOP = (0.32, 1.9)               # o topo das pernas do V (meia largura em x, afastamento em y)
PH = [-2.7, 0.0, 2.7]             # as 3 fases no braço (y relativo)
Z_PEAK = 11.8
# o pátio
BUS_Y = [1.85, 2.85, 3.85]        # as fases no pórtico, nos seccionadores, no barramento e nas buchas
GANTRY_X, GANTRY_Y, GANTRY_H = 4.15, (0.65, 5.05), 5.25
SW = (2.95, 3.65)                 # os 2 isoladores de cada seccionador (x)
BUS_Z = 3.98                      # o barramento
TRAFO = [(-0.05, 2.85), (2.05, 2.85)]               # os centros dos 2 transformadores
TK = (1.1, 2.7, 1.9, 0.3)         # o tanque: x, y, altura, a base
STAND_X = 1.0                     # os isoladores de apoio do barramento entre os 2
# o domo de contenção, a casa da turbina, a canaleta
DOME_C = (1.65, -3.35)            # o centro do domo
R_WALL = 2.55                     # o raio do cilindro (5,1 m; com a galeria 5,8 m)
Z_WIN = (0.38, 0.92)              # o anel de janelas do núcleo (o pé)
Z_SPRING = 4.7                    # a cornija: onde a cúpula arranca
DOME_H = 2.45                     # a altura da cúpula (o topo 7,25 m, a coroa 7,4)
Z_GAL = 4.5                       # o piso da galeria
HALL = (-4.25, -1.8, -5.6, -1.2, 3.0)      # a casa da turbina (x0, x1, y0, y1, altura)
DUCT_Z = 2.5
TRENCH_X, TRENCH_Y = -2.45, 0.2   # a canaleta: o trecho que sai da casa (x) e o que corre no pátio (y)
YARD = (-4.45, 4.45, -0.35, 6.15)
BUSH_TOP = TK[3] + TK[2] + 0.09 + 0.22 + 0.06 + 8 * 0.12 + 0.08 + 0.1      # o topo do terminal das buchas de alta


LINES = []                        # os 3 cabos (os pontos de cada um, no quadro da Unreal), pro lines.json


def seg(p, m, a, b, w, d=None, hint=(0.0, 0.0, 1.0), ext=0.004):
    """A barra de seção w × d entre os pontos a e b (a cantoneira da treliça, o perfil); 'hint' vira a largura. Passa 'ext' das
    pontas: as barras em fila (os banzos) se cruzam em vez de ter a quina em comum (o remove_doubles soldava: 3+ faces na aresta)."""
    a, b = Vector(a), Vector(b)
    ex = (b - a).normalized()
    a, b = a - ex * ext, b + ex * ext
    h = Vector(hint)
    ey = h - ex * h.dot(ex)
    if ey.length < 1e-4:
        h = Vector((1.0, 0.0, 0.0)) if abs(ex.x) < 0.9 else Vector((0.0, 1.0, 0.0))
        ey = h - ex * h.dot(ex)
    ey.normalize()
    ez = ex.cross(ey)
    ub.obox(p, m, (a + b) / 2, ((b - a).length, w, d if d is not None else w), ex, ey, ez)


def sheds(n, r_core, r_shed, pitch, base=0.06, cap=0.06):
    """O perfil da porcelana com os pratos (a bucha, o isolador de coluna): o núcleo e n pratos inclinados."""
    prof = [(0.0, 0.0), (r_core + 0.03, 0.0), (r_core + 0.03, base)]
    z = base
    for _ in range(n):
        prof += [(r_core, z + 0.01), (r_shed, z + pitch * 0.32), (r_shed, z + pitch * 0.45), (r_core, z + pitch * 0.75)]
        z += pitch
    prof += [(r_core, z + 0.01), (r_core + 0.02, z + cap), (0.0, z + cap + 0.01)]
    return prof, z + cap + 0.01


def post_insulator(p, c, n=6, pitch=0.13, r_core=0.06, r_shed=0.13):
    """O isolador de coluna em pé: a base de aço, a porcelana com os pratos e o terminal; devolve o topo."""
    x, y, z = c
    vp.lathe(p, 'Trim#vp', (x, y, z), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.06), (0.08, 0.1), (0.0, 0.1)], seg=12)
    prof, h = sheds(n, r_core, r_shed, pitch)
    vp.lathe(p, 'Paint#vp', (x, y, z + 0.1), 'z', prof, seg=14)
    vp.lathe(p, 'Trim#vp', (x, y, z + 0.1 + h), 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.06), (0.05, 0.1), (0.0, 0.1)], seg=12)
    return z + 0.2 + h


def string(p, a, b, n=None):
    """A cadeia de isoladores de ancoragem de a até b: os pratos de porcelana em pilha (o eixo é a cadeia), a haste, as ferragens."""
    A, B = Vector(a), Vector(b)
    d = (B - A).normalized()
    L = (B - A).length
    n = n or max(4, int((L - 0.3) / 0.15))
    vp.tube(p, 'Trim', [A, B], 0.02, seg=6)
    ub.obox(p, 'Trim#sm', A + d * 0.06, (0.14, 0.08, 0.12), d, Vector((0, 0, 1)).cross(d).normalized(),
            d.cross(Vector((0, 0, 1)).cross(d).normalized()))
    for k in range(n):
        q = A + d * (0.18 + (L - 0.3) * (k + 0.5) / n)
        vp.lathe(p, 'Paint#vp', q, d, [(0.0, -0.05), (0.03, -0.05), (0.13, -0.015), (0.13, 0.0), (0.05, 0.035), (0.0, 0.045)], seg=12)


def catenary(a, b, sag, n=18):
    """Os pontos do cabo de a até b com a barriga 'sag' no meio (a parábola: a catenária curta)."""
    A, B = Vector(a), Vector(b)
    return [A + (B - A) * (i / n) - Vector((0, 0, sag * 4 * (i / n) * (1 - i / n))) for i in range(n + 1)]


def lattice_column(p, x, y, h, w=0.36, step=0.55, m='Trim#sm', brace='Trim#vp', z0=0.25):
    """A coluna de treliça quadrada do pórtico: as 4 cantoneiras, o zigue-zague nas 4 faces, a sapata."""
    ub.obox(p, 'Concrete#pnl', (x, y, 0.08), (w + 0.3, w + 0.3, 0.4), X, Y, Z)
    hw = w / 2
    cs = [(-hw, -hw), (hw, -hw), (hw, hw), (-hw, hw)]
    for cx, cy in cs:
        seg(p, m, (x + cx, y + cy, z0), (x + cx, y + cy, h), 0.07, hint=(cx, cy, 0))
    n = max(2, int((h - z0) / step))
    for i in range(n):
        za, zb = z0 + (h - z0) * i / n, z0 + (h - z0) * (i + 1) / n
        for k in range(4):
            (ax, ay), (bx, by) = cs[k], cs[(k + 1) % 4]
            if i % 2:
                (ax, ay), (bx, by) = (bx, by), (ax, ay)
            seg(p, brace, (x + ax * 0.93, y + ay * 0.93, za), (x + bx * 0.93, y + by * 0.93, zb), 0.045)


# ---------------------------------------------------------------------------------- a torre de alta tensão
def tower_node(sx, sy, z):
    """O ponto da perna (sx, sy = ±1) na altura z do tronco (a pirâmide do pé à cintura)."""
    t = (z - 0.35) / (Z_WAIST - 0.35)
    hw = T_FOOT + (T_WAIST - T_FOOT) * t
    return Vector((TC[0] + sx * hw, TC[1] + sy * hw, z))


def v_node(sx, sy, t):
    """O ponto da perna do V (sx = ±1 em x, sy = ±1 o lado) na fração t da cintura ao braço."""
    x = T_WAIST + (V_TOP[0] - T_WAIST) * t
    y = T_WAIST + (V_TOP[1] - T_WAIST) * t
    return Vector((TC[0] + sx * x, TC[1] + sy * y, Z_WAIST + (Z_ARM0 - Z_WAIST) * t))


def arm_f(y):
    """A largura relativa do braço na posição y (relativa ao centro): cheia sobre o V, afinando pra ponta."""
    a = abs(y)
    return 1.0 if a <= V_TOP[1] else 1.0 - 0.62 * (a - V_TOP[1]) / (ARM - V_TOP[1])


def arm_top(y):
    """O banzo de cima do braço na posição y (baixa pra ponta)."""
    return Vector((TC[0], TC[1] + y, Z_ARM0 + (Z_ARM1 - Z_ARM0) * arm_f(y)))


def pylon(p):
    """A torre de treliça: o tronco, o V, o braço, os picos, as cadeias, a placa, os degraus."""
    cx, cy = TC
    leg, brace = 'Trim#sm', 'Trim#vp'
    # as sapatas e os pés
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = tower_node(sx, sy, 0.35)
            ub.obox(p, 'Concrete#pnl', (q.x, q.y, 0.12), (0.62, 0.62, 0.48), X, Y, Z)
            ub.obox(p, 'Trim#sm', (q.x, q.y, 0.42), (0.3, 0.3, 0.12), X, Y, Z)
    # o tronco: as pernas e os painéis com o X e a travessa
    lv = [0.35, 1.85, 3.2, 4.4, 5.5, 6.35, Z_WAIST]
    for sx in (-1, 1):
        for sy in (-1, 1):
            seg(p, leg, tower_node(sx, sy, lv[0]), tower_node(sx, sy, Z_WAIST), 0.16, hint=(sx, sy, 0))
    faces = [((-1, -1), (1, -1)), ((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1))]
    for i in range(len(lv) - 1):
        za, zb = lv[i], lv[i + 1]
        for (ax, ay), (bx, by) in faces:
            n = Vector(((ax + bx) / 2, (ay + by) / 2, 0.0))
            a0, b0 = tower_node(ax, ay, za), tower_node(bx, by, za)
            a1, b1 = tower_node(ax, ay, zb), tower_node(bx, by, zb)
            seg(p, brace, a0 + (b0 - a0) * 0.04, b1 + (a1 - b1) * 0.04, 0.075, hint=n)
            seg(p, brace, b0 + (a0 - b0) * 0.04, a1 + (b1 - a1) * 0.04, 0.075, hint=n)
            if i > 0:
                seg(p, brace, a0, b0, 0.085, hint=n)
    # a travessa e o diafragma da cintura
    for (ax, ay), (bx, by) in faces:
        seg(p, leg, tower_node(ax, ay, Z_WAIST), tower_node(bx, by, Z_WAIST), 0.12, hint=(0, 0, 1))
    seg(p, brace, tower_node(-1, -1, Z_WAIST), tower_node(1, 1, Z_WAIST), 0.07)
    seg(p, brace, tower_node(1, -1, Z_WAIST), tower_node(-1, 1, Z_WAIST), 0.07)
    # o V: as 2 pernas (cada uma um par de cantoneiras com o zigue-zague) da cintura até o braço
    for sy in (-1, 1):
        for sx in (-1, 1):
            seg(p, leg, v_node(sx, sy, 0.0), v_node(sx, sy, 1.0), 0.14, hint=(sx, 0, 0))
        nv = 5
        for i in range(nv):
            t0, t1 = i / nv, (i + 1) / nv
            a, b = v_node(-1, sy, t0), v_node(1, sy, t1)
            if i % 2:
                a, b = v_node(1, sy, t0), v_node(-1, sy, t1)
            seg(p, brace, a, b, 0.07, hint=(0, sy, 0))
            seg(p, brace, v_node(-1, sy, t1), v_node(1, sy, t1), 0.07, hint=(0, sy, 0))
        # as diagonais do lado de fora e de dentro do V (o V é uma treliça plana: travar contra o braço)
        seg(p, brace, v_node(-1, sy, 0.0) + Vector((0, 0, 0.05)), v_node(-1, -sy, 0.45), 0.07, hint=(1, 0, 0))
        seg(p, brace, v_node(1, sy, 0.0) + Vector((0, 0, 0.05)), v_node(1, -sy, 0.45), 0.07, hint=(1, 0, 0))
    # o braço: os 2 banzos de baixo, o de cima, o zigue-zague das 2 faces inclinadas e a escada de baixo
    ys = [-ARM + 2 * ARM * i / 12 for i in range(13)]
    for sx in (-1, 1):
        pts = [Vector((cx + sx * V_TOP[0] * arm_f(y), cy + y, Z_ARM0)) for y in ys]
        for a, b in zip(pts[:-1], pts[1:]):
            seg(p, leg, a, b, 0.12, hint=(0, 0, 1))
        for i in range(12):
            top_a = arm_top(ys[i])
            top_b = arm_top(ys[i + 1])
            if i % 2 == 0:
                seg(p, brace, pts[i], top_b, 0.065, hint=(sx, 0, 0.4))
            else:
                seg(p, brace, top_a, pts[i + 1], 0.065, hint=(sx, 0, 0.4))
    tops = [arm_top(y) for y in ys]
    for a, b in zip(tops[:-1], tops[1:]):
        seg(p, leg, a, b, 0.12, hint=(1, 0, 0))
    for y in ys[1:-1]:
        f = arm_f(y)
        seg(p, brace, (cx - V_TOP[0] * f, cy + y, Z_ARM0), (cx + V_TOP[0] * f, cy + y, Z_ARM0), 0.06)
    for y in (ys[0], ys[-1]):
        seg(p, leg, (cx - V_TOP[0] * arm_f(y) - 0.02, cy + y, Z_ARM0), (cx, cy + y, arm_top(y).z + 0.02), 0.1)
        seg(p, leg, (cx + V_TOP[0] * arm_f(y) + 0.02, cy + y, Z_ARM0), (cx, cy + y, arm_top(y).z + 0.02), 0.1)
    # os 2 picos do cabo-guarda (sobre as pernas do V) com a presilha; a luz de aviso no do lado do pátio
    for sy in (-1, 1):
        yb = cy + sy * V_TOP[1]
        apex = Vector((cx, yb, Z_PEAK))
        f = arm_f(sy * V_TOP[1])
        for sx in (-1, 1):
            seg(p, leg, (cx + sx * V_TOP[0] * f, yb, Z_ARM0), apex, 0.1, hint=(0, 1, 0))
        seg(p, brace, arm_top(sy * (V_TOP[1] - 0.7)), apex - Vector((0, 0, 0.1)), 0.06)
        seg(p, brace, arm_top(sy * (V_TOP[1] + 0.7)), apex - Vector((0, 0, 0.1)), 0.06)
        vp.lathe(p, 'Trim#vp', apex, 'z', [(0.0, -0.06), (0.09, -0.06), (0.09, 0.06), (0.0, 0.08)], seg=10)
    ub.beacon(p, (cx, cy + V_TOP[1], Z_PEAK + 0.08), r=0.08, h=0.17, cage=False)
    ub.beacon(p, (cx, cy - V_TOP[1], Z_PEAK + 0.08), r=0.08, h=0.17, cage=False)
    # as 3 cadeias de ancoragem (do banzo de baixo da frente, apontadas pro pórtico) e os cabos
    ends = []
    for k, y in enumerate(PH):
        att = Vector((cx + V_TOP[0] * arm_f(y) + 0.06, cy + y, Z_ARM0 - 0.06))
        tgt = Vector((GANTRY_X - 0.95, BUS_Y[k], GANTRY_H + 0.05))
        d = (tgt - att).normalized()
        d = (d + Vector((0, 0, -0.25))).normalized()
        e = att + d * 1.55
        string(p, att, e)
        ends.append(e)
    # a placa de perigo e o anti-escalada (as farpas) no tronco, os degraus numa perna
    q0, q1 = tower_node(1, -1, 2.6), tower_node(1, 1, 2.6)
    mid = (q0 + q1) / 2 + Vector((0.06, 0, 0))
    ub.obox(p, 'Yellow#sm', mid, (0.03, 0.55, 0.42), X, Y, Z)
    bolt = [(0.03, 0.17), (-0.07, 0.0), (0.0, 0.0), (-0.05, -0.17), (0.08, 0.03), (0.01, 0.03), (0.06, 0.17)]
    vp.prism(p, 'Dark#vp', bolt, mid + Vector((0.016, 0, 0)), Vector(Y), Vector(Z), Vector(X), 0.01)
    for (ax, ay), (bx, by) in faces:
        a, b = tower_node(ax, ay, 3.0), tower_node(bx, by, 3.0)
        n = Vector(((ax + bx) / 2, (ay + by) / 2, 0.0)).normalized()
        seg(p, brace, a + n * 0.12, b + n * 0.12, 0.05, hint=n)
    for k in range(14):
        q = tower_node(1, -1, 0.9 + k * 0.4)
        vp.tube(p, 'Trim', [q + Vector((0.0, 0.0, 0.0)), q + Vector((0.16, -0.16, 0.0))], 0.016, seg=5)
    return ends


# ---------------------------------------------------------------------------------- o pórtico, os seccionadores, o barramento
def yard_steel(p, ends):
    """O pórtico da frente com as cadeias, os 3 cabos da torre até ele, os jumpers, os 3 seccionadores no suporte, o barramento nos
    isoladores de apoio e os jumpers até as buchas dos transformadores."""
    gx = GANTRY_X
    for y in GANTRY_Y:
        lattice_column(p, gx, y, GANTRY_H + 0.42)
    # a viga em treliça (4 banzos e o zigue-zague)
    y0, y1 = GANTRY_Y[0] - 0.22, GANTRY_Y[1] + 0.22
    zb, zt = GANTRY_H, GANTRY_H + 0.42
    for dx in (-0.18, 0.18):
        for z in (zb, zt):
            seg(p, 'Trim#sm', (gx + dx, y0, z), (gx + dx, y1, z), 0.07, hint=(0, 0, 1))
    n = 10
    for i in range(n):
        ya, yb = y0 + (y1 - y0) * i / n, y0 + (y1 - y0) * (i + 1) / n
        for dx in (-0.17, 0.17):
            a, b = (zb, zt) if i % 2 == 0 else (zt, zb)
            seg(p, 'Trim#vp', (gx + dx, ya, a), (gx + dx, yb, b), 0.045, hint=(1, 0, 0))
        for z in (zb, zt):
            seg(p, 'Trim#vp', (gx - 0.17, ya, z), (gx + 0.17, yb, z), 0.045, hint=(0, 0, 1))
    # os holofotes do pórtico virados pro pátio e a placa azul
    for y in (1.35, 4.35):
        ub.lamp_head(p, (gx - 0.32, y, zb - 0.12), (-1.0, 0.0, -1.0), w=0.45, h=0.26)
    ub.obox(p, 'Player#sm', (gx + 0.22, (y0 + y1) / 2, (zb + zt) / 2), (0.03, 1.6, 0.3), X, Y, Z)
    # as cadeias do pórtico, os cabos (a catenária) e os jumpers até os seccionadores
    for k, y in enumerate(BUS_Y):
        att = Vector((gx - 0.2, y, zb + 0.02))
        e = Vector((gx - 0.95, y, zb + 0.05))
        string(p, att, e, n=5)
        # (01/11) o cabo inteiro num tubo só (a catenária e o jumper até o seccionador): o jogo corre o pulso azul nele (lines.json)
        jump = vp.bent_bar([e, e + Vector((-0.05, 0.0, -0.45)), Vector((SW[1] + 0.02, y, BUS_Z + 0.45)), Vector((SW[1], y, BUS_Z + 0.05))],
                           R=0.25, n=4)
        line = catenary(ends[k], e, 0.55) + jump[1:]
        vp.tube(p, 'PowerLine', line, 0.04, seg=6)   # (01/11) o slot do pulso azul (import_provisional.py: PowerLine, Pulse)
        LINES.append([[round(q.x, 3), round(q.y, 3), round(q.z, 3)] for q in line])
    # o suporte dos seccionadores (2 colunas, a viga) e os 3 seccionadores (a base, 2 isoladores, a lâmina, a manivela)
    sx = (SW[0] + SW[1]) / 2
    zs = BUS_Z - 0.64 - sheds(6, 0.06, 0.13, 0.13)[1]          # o topo dos isoladores na altura do barramento
    for y in (1.15, 4.55):
        ub.obox(p, 'Concrete#pnl', (sx, y, 0.08), (0.6, 0.6, 0.4), X, Y, Z)
        ub.obox(p, 'Trim#sm', (sx, y, (0.28 + zs) / 2), (0.22, 0.22, zs - 0.28), X, Y, Z)
    ub.obox(p, 'Trim#sm', (sx, 2.85, zs + 0.14), (0.25, 3.8, 0.28), X, Y, Z)
    for y in BUS_Y:
        ub.obox(p, 'Trim#sm', (sx, y, zs + 0.36), (SW[1] - SW[0] + 0.3, 0.16, 0.16), X, Y, Z)
        tops = [post_insulator(p, (x, y, zs + 0.44), n=6) for x in SW]
        vp.tube(p, 'Trim', [Vector((SW[0], y, tops[0] + 0.02)), Vector((SW[1] + 0.05, y, tops[1] + 0.02))], 0.035, seg=8)
        ub.obox(p, 'Dark#sm', (SW[1] + 0.02, y, tops[1] + 0.05), (0.14, 0.12, 0.1), X, Y, Z)
        vp.tube(p, 'Trim', [Vector((SW[0] + 0.1, y, zs + 0.3)), Vector((SW[0] + 0.1, y, zs - 0.2))], 0.02, seg=5)
    vp.tube(p, 'Trim', [Vector((SW[0] + 0.1, BUS_Y[0], zs - 0.2)), Vector((SW[0] + 0.1, 1.15, 1.3))], 0.025, seg=6)
    ub.obox(p, 'Body#sm', (sx - 0.05, 1.15 - 0.2, 1.1), (0.3, 0.2, 0.42), X, Y, Z)
    ub.obox(p, 'Hazard#sm', (sx - 0.05, 1.15 - 0.31, 1.1), (0.2, 0.02, 0.3), X, Y, Z)
    # os isoladores de apoio do barramento entre os 2 transformadores (o suporte, a viga, 3 isoladores)
    zs = BUS_Z - 0.48 - sheds(7, 0.06, 0.13, 0.13)[1]
    for y in (1.2, 4.5):
        ub.obox(p, 'Concrete#pnl', (STAND_X, y, 0.08), (0.4, 0.4, 0.4), X, Y, Z)
        ub.obox(p, 'Trim#sm', (STAND_X, y, (0.28 + zs) / 2), (0.2, 0.2, zs - 0.28), X, Y, Z)
    ub.obox(p, 'Trim#sm', (STAND_X, 2.85, zs + 0.14), (0.24, 3.7, 0.28), X, Y, Z)
    for y in BUS_Y:
        post_insulator(p, (STAND_X, y, zs + 0.28), n=7)
    # o barramento (o tubo de alumínio) e os jumpers até as buchas
    for y in BUS_Y:
        vp.tube(p, 'Paint', [Vector((SW[0], y, BUS_Z + 0.05)), Vector((TRAFO[0][0] - 0.25, y, BUS_Z + 0.05))], 0.05, seg=10)
        for tx, _ in TRAFO:
            top = Vector((tx, y, BUSH_TOP))
            vp.tube(p, 'Dark', vp.bent_bar([Vector((tx + 0.25, y, BUS_Z)), Vector((tx + 0.12, y, BUS_Z - 0.12)), top + Vector((0.03, 0, 0.12)),
                                            top], R=0.12, n=3), 0.025, seg=6)


# ---------------------------------------------------------------------------------- os transformadores
def transformer(p, c):
    """O transformador de força: a base com a bacia de óleo, o tanque com as nervuras e a tampa aparafusada, as 3 buchas de alta, o
    radiador de aletas (+Y) com os coletores, o conservador no suporte com o cano, a caixa de cabos (−Y), a placa."""
    tx, ty = c
    sx, sy, h, zb = TK
    x0, x1, y0, y1 = tx - sx / 2, tx + sx / 2, ty - sy / 2, ty + sy / 2
    # a base com a bacia (o meio-fio de concreto em volta)
    ub.obox(p, 'Concrete#pnl', (tx, ty, 0.06), (sx + 0.5, sy + 1.6, 0.3), X, Y, Z)
    for s in (-1, 1):
        ub.obox(p, 'Concrete#sm', (tx + s * (sx / 2 + 0.2), ty, 0.24), (0.1, sy + 1.6, 0.12), X, Y, Z)
        ub.obox(p, 'Concrete#sm', (tx, ty + s * (sy / 2 + 0.75), 0.24), (sx + 0.36, 0.1, 0.12), X, Y, Z)
    ub.obox(p, 'Trim#sm', (tx, ty, zb - 0.04), (sx + 0.1, sy - 0.2, 0.2), X, Y, Z)
    # o tanque, as nervuras dos lados compridos, a tampa com o anel de parafusos
    ub.obox(p, 'Body#pnl', (tx, ty, zb + h / 2), (sx, sy, h), X, Y, Z)
    for s in (-1, 1):
        for k in range(5):
            y = y0 + 0.35 + k * (sy - 0.7) / 4
            ub.obox(p, 'Body#sm', (tx + s * (sx / 2 + 0.04), y, zb + h / 2 - 0.05), (0.08, 0.12, h - 0.3), X, Y, Z)
        ub.obox(p, 'Body#sm', (tx + s * (sx / 2 + 0.02), ty, zb + 0.18), (0.06, sy - 0.1, 0.12), X, Y, Z)
    ub.obox(p, 'Body#sm', (tx, ty, zb + h + 0.04), (sx + 0.12, sy + 0.12, 0.1), X, Y, Z)
    for s in (-1, 1):
        vp.bolt_row(p, 'Dark', (x0 + 0.1, ty + s * (sy / 2 + 0.06), zb + h + 0.06), (x1 - 0.1, ty + s * (sy / 2 + 0.06), zb + h + 0.06), 5,
                    (0, s, 0), r=0.03, h=0.02)
        vp.bolt_row(p, 'Dark', (tx + s * (sx / 2 + 0.06), y0 + 0.1, zb + h + 0.06), (tx + s * (sx / 2 + 0.06), y1 - 0.1, zb + h + 0.06), 11,
                    (s, 0, 0), r=0.03, h=0.02)
    ub.obox(p, 'Player#sm', (x1 + 0.09, ty - 0.55, zb + h - 0.45), (0.02, 0.5, 0.32), X, Y, Z)
    # as 3 buchas de alta (a torre de aço, a porcelana com os pratos, o terminal)
    zt = zb + h + 0.09
    for y in BUS_Y:
        vp.lathe(p, 'Trim#vp', (tx, y, zt), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.08), (0.14, 0.12), (0.14, 0.22), (0.0, 0.22)], seg=16)
        vp.bolt_ring(p, 'Dark', (tx, y, zt + 0.08), 'z', 0.17, 8, r=0.025, h=0.018)
        prof, hh = sheds(8, 0.075, 0.16, 0.12)
        vp.lathe(p, 'Paint#vp', (tx, y, zt + 0.22), 'z', prof, seg=16)
        vp.lathe(p, 'Trim#vp', (tx, y, zt + 0.22 + hh), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.08), (0.0, 0.1)], seg=10)
    # o radiador de aletas (+Y): os coletores e as aletas
    ry0, ry1 = y1 + 0.12, y1 + 0.72
    for z in (zb + 0.3, zb + h - 0.15):
        vp.tube(p, 'Trim', [Vector((x0 + 0.1, (ry0 + ry1) / 2, z)), Vector((x1 - 0.1, (ry0 + ry1) / 2, z))], 0.06, seg=8)
        vp.tube(p, 'Trim', [Vector((tx, y1 - 0.02, z)), Vector((tx, ry0 + 0.05, z))], 0.07, seg=8)
    nf = 9
    for k in range(nf):
        x = x0 + 0.08 + (sx - 0.16) * k / (nf - 1)
        ub.obox(p, 'Body#vp', (x, (ry0 + ry1) / 2, (zb + 0.25 + zb + h - 0.1) / 2), (0.04, ry1 - ry0, h - 0.35), X, Y, Z)
    ub.obox(p, 'Trim#sm', (tx, ry1 - 0.02, zb + 0.08), (sx - 0.1, 0.08, 0.12), X, Y, Z)
    for x in (x0 + 0.12, x1 - 0.12):
        ub.obox(p, 'Trim#sm', (x, ry1 - 0.05, (0.24 + zb + 0.1) / 2), (0.08, 0.08, zb + 0.1 - 0.24), X, Y, Z)
    # o conservador (o tanque deitado em cima do radiador) no suporte, o cano até a tampa
    cz = zb + h + 0.62
    vp.lathe(p, 'Body#vp', (tx, (ry0 + ry1) / 2, cz), 'x', [(0.0, -0.5), (0.18, -0.49), (0.24, -0.44), (0.24, 0.44), (0.18, 0.49), (0.0, 0.5)],
             seg=18)
    for x in (tx - 0.32, tx + 0.32):
        ub.obox(p, 'Trim#sm', (x, (ry0 + ry1) / 2, (zb + h + cz - 0.2) / 2), (0.07, 0.07, cz - 0.2 - zb - h + 0.1), X, Y, Z)
        ub.obox(p, 'Trim#sm', (x, (ry0 + ry1) / 2, cz - 0.2), (0.08, 0.3, 0.06), X, Y, Z)
    vp.tube(p, 'Trim', vp.bent_bar([Vector((tx + 0.3, (ry0 + ry1) / 2, cz - 0.15)), Vector((tx + 0.3, y1 - 0.25, cz - 0.15)),
                                    Vector((tx + 0.3, y1 - 0.25, zb + h + 0.08))], R=0.12, n=3), 0.035, seg=8)
    # a caixa de cabos (−Y) com os prensa-cabos
    ub.obox(p, 'Body#pnl', (tx, y0 - 0.2, zb + 0.75), (0.8, 0.42, 0.85), X, Y, Z)
    vp.bolt_row(p, 'Dark', (tx - 0.32, y0 - 0.42, zb + 1.12), (tx + 0.32, y0 - 0.42, zb + 1.12), 5, (0, -1, 0), r=0.025, h=0.018)
    for dx in (-0.22, 0.0, 0.22):
        vp.lathe(p, 'Dark#vp', (tx + dx, y0 - 0.2, zb + 0.32), 'z', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.06), (0.0, 0.06)], seg=10)


def cable_trench(p):
    """A canaleta de concreto dos cabos (da casa da turbina pro pátio e ao longo dele, com as tampas) e os cabos grossos que sobem dela
    pras caixas dos transformadores e descem da caixa de junção da casa."""
    x0h, x1h, y0h, y1h, hh = HALL
    xe = TRAFO[1][0] + 0.45
    runs = [((TRENCH_X, y1h - 0.05), (TRENCH_X, TRENCH_Y - 0.02)), ((TRENCH_X - 0.31, TRENCH_Y), (xe, TRENCH_Y))]
    for (ax, ay), (bx, by) in runs:
        x0, x1 = min(ax, bx) - (0.3 if ax == bx else 0.0), max(ax, bx) + (0.3 if ax == bx else 0.0)
        y0, y1 = min(ay, by) - (0.3 if ay == by else 0.0), max(ay, by) + (0.3 if ay == by else 0.0)
        ub.slab(p, 'Concrete#pnl', x0, x1, y0, y1, -0.08, 0.15)
        L = max(x1 - x0, y1 - y0)
        n = max(1, int(round(L / 0.9)))
        for k in range(n):
            t0, t1 = k / n, (k + 1) / n
            if ax == bx:
                ub.obox(p, 'Trim#sm', ((x0 + x1) / 2, y0 + (y1 - y0) * (t0 + t1) / 2, 0.165), (0.5, (y1 - y0) / n - 0.05, 0.03), X, Y, Z)
            else:
                ub.obox(p, 'Trim#sm', (x0 + (x1 - x0) * (t0 + t1) / 2, (y0 + y1) / 2, 0.165), ((x1 - x0) / n - 0.05, 0.5, 0.03), X, Y, Z)
    # os cabos que sobem da canaleta pras caixas de cabos dos transformadores
    tz = TK[3] + 0.32
    for tx, ty in TRAFO:
        yb = ty - TK[1] / 2 - 0.2
        for dx in (-0.22, 0.0, 0.22):
            x = tx + dx
            pts = [Vector((x, yb, tz)), Vector((x, yb, 0.22)), Vector((x, yb - 0.4, 0.12)), Vector((x, TRENCH_Y + 0.35, 0.14)),
                   Vector((x, TRENCH_Y + 0.12, 0.05))]
            vp.tube(p, 'Rubber', vp.bent_bar(pts, R=0.22, n=4), 0.065, seg=8)
    # a caixa de junção na face da casa (+Y) e os cabos descendo pra canaleta
    ub.obox(p, 'Body#pnl', (TRENCH_X, y1h + 0.14, 0.95), (0.9, 0.28, 0.9), X, Y, Z)
    ub.obox(p, 'Hazard#sm', (TRENCH_X, y1h + 0.29, 1.25), (0.6, 0.02, 0.12), X, Y, Z)
    for dx in (-0.22, 0.0, 0.22):
        x = TRENCH_X + dx
        vp.tube(p, 'Rubber', vp.bent_bar([Vector((x, y1h + 0.14, 0.55)), Vector((x, y1h + 0.2, 0.25)), Vector((x, y1h + 0.45, 0.1))], R=0.15, n=3),
                0.065, seg=8)


def turbine_hall(p):
    """A casa da turbina e do gerador (baixa, atrás do domo): o miolo, as placas com a faixa azul, os pilares, o parapeito, o teto com
    a caixa de ventiladores, as venezianas, a porta."""
    x0, x1, y0, y1, h = HALL
    pl = chamfer_rect(((x0 + x1) / 2, (y0 + y1) / 2), (x1 - x0) / 2, (x1 - x0) / 2, (y1 - y0) / 2, (y1 - y0) / 2, (0.3, 0.3, 0.3, 0.3))
    poly_band(p, 'Concrete#pnl', pl, 0.25, -0.1, -0.1, 0.12)
    plan_core(p, pl, 0.0, 0.1, h)
    plan_walls(p, pl, 0.0, 0.12, h - 0.05, rows=(0.9, 1.85, 2.3), blue=2, pw=2.2)
    plan_pillars(p, pl, 0.05, 0.12, h + 0.1, r=0.22)
    poly_band(p, 'Body#pnl', pl, 0.1, -0.22, h - 0.12, h + 0.3)
    poly_band(p, 'Trim#sm', pl, 0.15, -0.27, h + 0.3, h + 0.38)
    poly_slab(p, 'Concrete#pnl', pl, -0.2, h - 0.2, h, cell=1.4)
    ub.fan_box(p, ((x0 + x1) / 2, y0 + 1.3), (1.7, 1.5, 0.85), fans=1, z0=h)
    ub.fan_box(p, ((x0 + x1) / 2, y1 - 1.3), (1.7, 1.5, 0.85), fans=1, z0=h)
    louvers(p, (x0 - 0.09, (y0 + y1) / 2, 1.5), (-1, 0, 0), (0, 1, 0), 2.4, 0.8, 9)
    # a porta (−Y) com a marquise
    q = Vector(((x0 + x1) / 2, y0, 0.12))
    ub.obox(p, 'Dark#vp', q - Vector((0, 0.1, -1.05)), (1.1, 0.1, 2.1), X, Y, Z)
    ub.obox(p, 'Trim#sm', q - Vector((0, 0.14, -1.02)), (0.96, 0.08, 2.0), X, Y, Z)
    ub.obox(p, 'Trim#sm', q - Vector((0, 0.3, -2.35)), (1.6, 0.5, 0.08), X, Y, Z)
    ub.lamp_head(p, q - Vector((0, 0.3, -2.22)), (0.0, -1.0, -1.2), w=0.4, h=0.22)
    ub.beacon(p, (x0 + 0.3, y1 - 0.3, h + 0.38), r=0.08, h=0.17)


def containment(p):
    """O domo de contenção: a fundação redonda, o anel de janelas do núcleo no pé, o cilindro com os contrafortes, os anéis de
    protensão e a faixa azul, a galeria com o guarda-corpo e a escada, a cornija, a cúpula com os anéis, a coroa com a luz, a
    escotilha de equipamento com o patamar, o trifólio, a escada, o duto da turbina."""
    cx, cy = DOME_C
    C = Vector((cx, cy, 0.0))
    R = R_WALL

    def ring(m, r0, r1, z0, z1, seg=64):
        vp.lathe(p, m, C, 'z', [(r0, z0), (r1, z0), (r1, z1), (r0, z1)], seg=seg, closed=True)

    def at(ang, r, z):
        a = math.radians(ang)
        return Vector((cx + r * math.cos(a), cy + r * math.sin(a), z)), Vector((math.cos(a), math.sin(a), 0.0))
    # a fundação redonda com o degrau
    vp.lathe(p, 'Concrete#vp', C, 'z', [(0.0, -0.1), (R + 0.22, -0.1), (R + 0.22, 0.2), (R + 0.14, 0.3), (0.0, 0.3)], seg=64)
    # o anel de janelas do núcleo: o vidro aceso (Core) atrás, o peitoril e a verga, os 16 pilares
    ring('Core#vp', R - 0.14, R - 0.09, Z_WIN[0] - 0.04, Z_WIN[1] + 0.04)
    ring('Dark#vp', R - 0.3, R - 0.15, Z_WIN[0] - 0.04, Z_WIN[1] + 0.04)
    ring('Concrete#vp', R - 0.3, R + 0.07, 0.29, Z_WIN[0])
    ring('Concrete#vp', R - 0.3, R + 0.07, Z_WIN[1], Z_WIN[1] + 0.13)
    for k in range(16):
        q, d = at(11.25 + 22.5 * k, R - 0.04, (Z_WIN[0] + Z_WIN[1]) / 2)
        ub.obox(p, 'Concrete#sm', q, (0.3, 0.24, Z_WIN[1] - Z_WIN[0] + 0.02), d, Vector((-d.y, d.x, 0)), Z)
    # o cilindro, os anéis de protensão, a faixa azul, os 3 contrafortes
    z_w0 = Z_WIN[1] + 0.12
    ring('Concrete#vp', R - 0.3, R, z_w0, Z_SPRING)
    for z in (1.85, 2.85, 3.75):
        ring('Concrete#vp', R - 0.02, R + 0.035, z, z + 0.07)
    ring('Player#vp', R - 0.02, R + 0.025, 4.0, 4.38)
    for ang in (30, 150, 270):
        q, d = at(ang, R + 0.06, (z_w0 + Z_SPRING) / 2)
        ub.obox(p, 'Concrete#pnl', q, (0.36, 0.62, Z_SPRING - z_w0), d, Vector((-d.y, d.x, 0)), Z)
    # a galeria com as mãos-francesas e o guarda-corpo
    ring('Trim#vp', R - 0.05, R + 0.36, Z_GAL - 0.08, Z_GAL)
    for k in range(12):
        q, d = at(15 + 30 * k, R, Z_GAL - 0.08)
        vp.prism(p, 'Trim#vp', [(0.0, 0.0), (0.34, 0.0), (0.0, -0.42)], q - Vector((-d.y, d.x, 0)) * 0.03, d, Vector(Z), Vector((-d.y, d.x, 0)), 0.06)
    pts = [tuple(at(360.0 * k / 28, R + 0.31, 0.0)[0])[:2] for k in range(28)]
    ub.rail(p, 'Trim', pts + [pts[0]], Z_GAL, h=0.95, r=0.026, step=1.1, toe='Trim')
    # a cornija, a cúpula (elipse), os anéis da cúpula, a coroa com o respiro e a luz de aviso
    ring('Concrete#vp', R - 0.3, R + 0.16, Z_SPRING - 0.05, Z_SPRING + 0.12)
    zc = Z_SPRING + 0.1
    prof = [(0.0, zc)] + [(R * math.cos(math.radians(a)), zc + DOME_H * math.sin(math.radians(a))) for a in range(0, 91, 6)]
    prof[-1] = (0.0, prof[-1][1])
    vp.lathe(p, 'Concrete#vp', C, 'z', prof, seg=64)
    for a in (28.0, 55.0):
        r0, z0 = R * math.cos(math.radians(a)), zc + DOME_H * math.sin(math.radians(a))
        ring('Concrete#vp', r0 - 0.03, r0 + 0.03, z0 - 0.02, z0 + 0.05)
    top = zc + DOME_H
    vp.lathe(p, 'Trim#vp', C + Vector((0, 0, top - 0.06)), 'z', [(0.0, 0.0), (0.62, 0.0), (0.62, 0.1), (0.52, 0.16), (0.0, 0.16)], seg=32)
    vp.bolt_ring(p, 'Dark', C + Vector((0, 0, top + 0.1)), 'z', 0.55, 16, r=0.03, h=0.02)
    vp.lathe(p, 'Body#vp', C + Vector((0, 0, top + 0.1)), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.32), (0.36, 0.36), (0.36, 0.42), (0.0, 0.44)],
             seg=20)
    ub.beacon(p, (cx, cy, top + 0.54), r=0.09, h=0.18)
    # a escotilha de equipamento (315°: virada pra câmera): a moldura, a tampa abaulada, os parafusos, as dobradiças, o patamar
    hz = 2.55
    q, d = at(315, R, hz)
    t = Vector((-d.y, d.x, 0.0))
    vp.lathe(p, 'Trim#vp', q, d, [(0.66, -0.25), (0.92, -0.25), (0.92, 0.14), (0.84, 0.22), (0.66, 0.22)], seg=36, closed=True)
    vp.lathe(p, 'Body#vp', q, d, [(0.0, 0.0), (0.68, 0.0), (0.68, 0.1), (0.58, 0.18), (0.0, 0.2)], seg=36)
    vp.bolt_ring(p, 'Dark', q + d * 0.22, d, 0.78, 18, r=0.035, h=0.025)
    for s in (-1, 1):
        vp.hinge(p, 'Trim', q + d * 0.2 + t * 0.72 + Vector((0, 0, s * 0.35)), 'z', 0.3, r=0.05, knuckles=2)
    ub.obox(p, 'Hazard#sm', q + d * 0.24 - Vector((0, 0, 0.98)), (0.06, 1.5, 0.12), d, t, Z)
    ub.obox(p, 'Trim#sm', q + d * 0.48 - Vector((0, 0, 1.0)), (0.72, 1.3, 0.08), d, t, Z)
    for s in (-1, 1):
        vp.tube(p, 'Trim', [q + d * 0.12 + t * (s * 0.55) - Vector((0, 0, 1.55)), q + d * 0.8 + t * (s * 0.55) - Vector((0, 0, 1.04))], 0.035, seg=6)
    # o trifólio ao lado da escotilha
    qt, dt_ = at(292, R + 0.04, 3.2)
    trefoil(p, qt, Vector((-dt_.y, dt_.x, 0.0)), Vector(Z), dt_, s=0.55)
    # a escada de marinheiro com gaiola até a galeria (345°)
    q, d = at(345, R, 0.0)
    ub.ladder(p, 'Trim', q.x, q.y, 0.3, Z_GAL, tuple(d), w=0.5, cage=True, wall=0.12, top_ext=1.0)
    # o duto da turbina (180°) até a casa: o tubo grosso, os flanges, o fole, e o cano menor embaixo
    x0 = cx - R + 0.1
    x1 = HALL[1] - 0.05
    vp.lathe(p, 'Body#vp', (0, cy, DUCT_Z), 'x', [(0.0, x1), (0.46, x1), (0.46, x0), (0.0, x0)], seg=28)
    for x in (x1 + 0.12, x0 - 0.12):
        vp.lathe(p, 'Trim#vp', (0, cy, DUCT_Z), 'x', [(0.45, x - 0.06), (0.6, x - 0.06), (0.6, x + 0.06), (0.45, x + 0.06)], seg=28, closed=True)
        vp.bolt_ring(p, 'Dark', (x + 0.065, cy, DUCT_Z), 'x', 0.54, 12, r=0.025, h=0.018)
    vp.bellows(p, 'Rubber', (x0 + x1) / 2 - 0.15, (x0 + x1) / 2 + 0.15, 0.47, 0.47, DUCT_Z, y=cy, folds=3, depth=0.04)
    vp.lathe(p, 'Player#vp', (0, cy, DUCT_Z), 'x', [(0.455, x0 - 0.35), (0.48, x0 - 0.35), (0.48, x0 - 0.25), (0.455, x0 - 0.25)], seg=28, closed=True)
    ub.pipe(p, 'Trim', [(cx - 2.42, cy - 0.85, 1.55), (x1, cy - 0.85, 1.55)], 0.12)


def tri_sector(r0, r1, a0, a1, n=6):
    pts = [(r1 * math.cos(math.radians(a0 + (a1 - a0) * i / n)), r1 * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    pts += [(r0 * math.cos(math.radians(a1 - (a1 - a0) * i / n)), r0 * math.sin(math.radians(a1 - (a1 - a0) * i / n))) for i in range(n + 1)]
    return pts


def trefoil(p, c, eu, ev, en, s=1.0):
    """A placa do trifólio (o símbolo da radiação): o quadrado amarelo, o disco do meio e as 3 pás escuras."""
    C = Vector(c)
    ub.obox(p, 'Yellow#sm', C, (0.9 * s, 0.9 * s, 0.03), eu, ev, en)
    vp.lathe(p, 'Dark#vp', C + en * 0.015, en, [(0.0, 0.0), (0.075 * s, 0.0), (0.075 * s, 0.012), (0.0, 0.012)], seg=16)
    for a in (90, 210, 330):
        pts = [(u * s, v * s) for u, v in tri_sector(0.12, 0.36, a - 30, a + 30)]
        vp.prism(p, 'Dark#vp', pts, C + en * 0.015, eu, ev, en, 0.012)


# ---------------------------------------------------------------------------------- o chão do pátio e a cerca
def yard_ground(p):
    """O pátio de cascalho: o fundo escuro, o meio-fio de concreto e as pedras soltas (cada uma um tom no material) fora das bases."""
    x0, x1, y0, y1 = YARD
    ub.slab(p, 'Dark#vp', x0, x1, y0, y1, -0.1, 0.04)
    for (a, b, c, d) in ((x0 - 0.05, x1 + 0.05, y0 - 0.1, y0 + 0.1), (x0 - 0.05, x1 + 0.05, y1 - 0.1, y1 + 0.1),
                         (x0 - 0.1, x0 + 0.1, y0, y1), (x1 - 0.1, x1 + 0.1, y0, y1)):
        ub.slab(p, 'Concrete#pnl', a, b, c, d, -0.1, 0.16)
    keep_out = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = tower_node(sx, sy, 0.35)
            keep_out.append((q.x - 0.36, q.x + 0.36, q.y - 0.36, q.y + 0.36))
    for tx, ty in TRAFO:
        keep_out.append((tx - TK[0] / 2 - 0.28, tx + TK[0] / 2 + 0.28, ty - TK[1] / 2 - 0.83, ty + TK[1] / 2 + 0.83))
    for y in GANTRY_Y:
        keep_out.append((GANTRY_X - 0.42, GANTRY_X + 0.42, y - 0.42, y + 0.42))
    for x, y in (((SW[0] + SW[1]) / 2, 1.15), ((SW[0] + SW[1]) / 2, 4.55), (STAND_X, 1.2), (STAND_X, 4.5)):
        keep_out.append((x - 0.32, x + 0.32, y - 0.32, y + 0.32))
    keep_out.append((TRENCH_X - 0.36, TRENCH_X + 0.36, y0 - 0.2, TRENCH_Y + 0.36))
    keep_out.append((TRENCH_X - 0.36, TRAFO[1][0] + 0.8, TRENCH_Y - 0.36, TRENCH_Y + 0.36))
    rnd = random.Random(11)
    b = p.bm('Concrete#vp', 0.0)
    count = 0
    area = (x1 - x0 - 0.3) * (y1 - y0 - 0.3)
    for _ in range(int(area * 105)):
        x = rnd.uniform(x0 + 0.14, x1 - 0.14)
        y = rnd.uniform(y0 + 0.14, y1 - 0.14)
        if any(a <= x <= bb and c <= y <= d for a, bb, c, d in keep_out):
            continue
        r = rnd.uniform(0.05, 0.085)
        hgt = r * rnd.uniform(0.55, 0.85)
        rot = rnd.uniform(0, math.pi)
        cz = 0.04 + hgt * 0.35
        pts = []
        for k in range(4):
            a = rot + math.pi / 2 * k + rnd.uniform(-0.3, 0.3)
            rr = r * rnd.uniform(0.75, 1.15)
            pts.append(b.verts.new(V(x + rr * math.cos(a), y + rr * math.sin(a), cz + rnd.uniform(-0.01, 0.01))))
        top = b.verts.new(V(x + rnd.uniform(-0.2, 0.2) * r, y + rnd.uniform(-0.2, 0.2) * r, cz + hgt))
        bot = b.verts.new(V(x, y, cz - hgt * 0.6))
        for k in range(4):
            b.faces.new([pts[k], pts[(k + 1) % 4], top])
            b.faces.new([pts[(k + 1) % 4], pts[k], bot])
        count += 1
    print('MODEL pedras do cascalho:', count)


def fence(p):
    """A cerca de grade do pátio (frente, +Y e atrás): os mourões, as 2 travessas, as lanças de 5 cm a cada 20 cm com a ponta, as
    placas de perigo com o raio."""
    x0, x1, y0, y1 = YARD
    xf, yf, xb = x1 + 0.1, y1 + 0.06, x0 - 0.1
    runs = [((xf, y0 + 0.05), (xf, yf)), ((xf, yf), (xb, yf)), ((xb, yf), (xb, y0 + 0.05))]
    h = 2.1
    for (ax, ay), (bx, by) in runs:
        A, B = Vector((ax, ay, 0.0)), Vector((bx, by, 0.0))
        L = (B - A).length
        d = (B - A).normalized()
        e = Vector((-d.y, d.x, 0.0))
        nposts = max(1, int(round(L / 2.0)))
        for i in range(nposts + 1):
            q = A + d * (L * i / nposts)
            ub.obox(p, 'Trim#sm', (q.x, q.y, (0.1 + h + 0.15) / 2), (0.09, 0.09, h + 0.05), d, e, Z)
            vp.tube(p, 'Trim', [q + Vector((0, 0, h + 0.1)), q + Vector((0, 0, h + 0.5))], 0.02, seg=5)
        for z in (0.35, h - 0.25):
            ub.obox(p, 'Trim#sm', (A + B) / 2 + Vector((0, 0, z)), (L, 0.04, 0.07), d, e, Z)
        for zz in (h + 0.2, h + 0.33, h + 0.46):
            vp.tube(p, 'Dark', [A + Vector((0, 0, zz)), B + Vector((0, 0, zz))], 0.012, seg=4)
        n = int(L / 0.2)
        for i in range(n):
            q = A + d * (L * (i + 0.5) / n)
            ub.obox(p, 'Trim#vp', (q.x, q.y, (0.12 + h) / 2), (0.05, 0.02, h - 0.12), d, e, Z)
            vp.prism(p, 'Trim#vp', [(-0.025, 0.0), (0.025, 0.0), (0.0, 0.09)], q + Vector((0, 0, h)) - e * 0.01, d, Vector(Z), e, 0.02)
    # as placas de perigo (amarela, o raio escuro, a borda): na frente e no lado +Y
    for c, eu, en in (((xf + 0.04, 3.6, 1.45), Vector((0, 1, 0)), Vector((1, 0, 0))), ((-1.6, yf + 0.04, 1.45), Vector((-1, 0, 0)), Vector((0, 1, 0)))):
        C = Vector(c)
        ub.obox(p, 'Dark#sm', C, (0.66, 0.5, 0.02), eu, Vector(Z), en)
        ub.obox(p, 'Yellow#sm', C + en * 0.015, (0.6, 0.44, 0.02), eu, Vector(Z), en)
        bolt = [(0.03, 0.18), (-0.07, 0.0), (0.0, 0.0), (-0.05, -0.18), (0.08, 0.03), (0.01, 0.03), (0.06, 0.18)]
        vp.prism(p, 'Dark#vp', bolt, C + en * 0.026, eu, Vector(Z), en, 0.01)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    yard_ground(p)
    ends = pylon(p)
    yard_steel(p, ends)
    for c in TRAFO:
        transformer(p, c)
    containment(p)
    turbine_hall(p)
    cable_trench(p)
    fence(p)
    return ub.finish(p)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_power_plant_v3')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    ra = 3.15                                       # o andaime em volta do domo (o octógono, por fora da galeria)
    pts = [(DOME_C[0] + ra * math.cos(math.radians(22.5 + 45 * k)), DOME_C[1] + ra * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]
    sc = scaffold_poly(PREFIX + '_Scaffold', pts, Z_GAL, out=-0.22)
    check([b, sc], HX, HY)
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, sc)})
    total = export(b, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL fire_points_m', [[DOME_C[0], DOME_C[1], round(Z_SPRING + 0.1 + DOME_H, 2)], [round((HALL[0] + HALL[1]) / 2, 2), round((HALL[2] + HALL[3]) / 2, 2), HALL[4]],
                                  [TRAFO[1][0], TRAFO[1][1], round(TK[3] + TK[2], 2)]])
    import json
    with open(os.path.join(out, 'lines.json'), 'w', encoding='utf-8') as fh:
        json.dump({'lines_m': LINES, 'about': 'os 3 cabos da torre (fases a −Y, ao meio, a +Y), da ponta da cadeia da torre ao seccionador; '
                   'quadro da Unreal (x frente, y direita, z cima, m); cada um é um tubo contínuo (Dark, r 4 cm) na SM_USA_PowerPlant_Base'},
                  fh, indent=1)
    for k, ln in enumerate(LINES):
        print('MODEL cabo %d: %d pontos, de %s a %s' % (k, len(ln), ln[0], ln[-1]))
    print('MODEL torre: topo dos picos %.2f m, braço %.2f m' % (Z_PEAK, Z_ARM1))
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]]],
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1300]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1300, 1000]],
                  ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1300, 1100]],
                  ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
                 (1.5, 10.5, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
