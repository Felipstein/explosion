"""A defesa de base da USA (a bateria Patriot, usa_patriot), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_patriot.py, aprovada em 14/10: "top, manda pros cara botar no jogo") fica como está; esta é a que ele compara.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_patriot_v2.py C:/Users/luisf/explosion/captures/game/models/usa_patriot_v2

O mesmo desenho do A (a fórmula do ZH: a CAIXA LANÇADORA de 2 × 2 células inclinada 45° girando no bloco octogonal blindado, o
radar no mastro, a faixa de perigo no chão; lote 8 × 8 m) e as mesmas medidas, refeito peça por peça na linguagem dos prédios da USA
(a ficha da War Factory: placas de metal claro com o chanfro largo e a junta funda; docs/ESTUDO_DETALHE_AAA.md):
  - o bloco octogonal de PLACAS (2 por face, com os parafusos nos cantos), o rodapé, a rampa blindada de cima, o deck com o poço do
    rolamento; o painel azul do jogador nas 2 faces da frente; as luminárias de serviço com o corpo;
  - o pedestal torneado com as nervuras, a escotilha e o anel de rolamento com os parafusos;
  - a CAIXA LANÇADORA de quinas redondas com os 3 aros de reforço, as 4 células com a moldura e a tampa abaulada de nervuras, a
    faixa azul dos lados, o painel azul e a escotilha em cima, as olhais de içar, as venezianas do escape atrás; o berço com os munhões
    torneados, o cilindro de elevação (o corpo amarelo e a haste), o sensor de tiro com a lente;
  - o mastro cônico do radar com a escada, a plataforma, a caixa de serviço com a veneziana, o conduíte até o bloco e a sirene âmbar
    no braço; o radar com o prato parabólico de verdade (a casca com a borda), as costelas atrás, o tripé e a corneta;
  - o gerador de placas com a veneziana, o escapamento com o chapéu, a faixa azul, a porta; a escada de acesso ao deck com o
    corrimão de tubo; a faixa de perigo do chão como um meio-fio baixo (o Hazard do jogo desenha as listras).
A USA limpa (13/10): sem desgaste pintado.

Pivôs MANTIDOS (Units.json usa_patriot): turret_pivot_m [0, 0] (a caixa gira em Z no centro do lote), muzzle_height_m 3,75 (o centro
da caixa), spin do radar at_m (−2,35; −2,35; 4,4) = DISH_AT (a origem da malha do radar), turret_yaw_offset 0 (a boca em +X).
Peças (os NOMES do A): SM_USA_Patriot_Base, SM_USA_Patriot_Turret, SM_USA_Patriot_Dish (origem no eixo), SM_USA_Patriot_Scaffold.
Slots (os do A): Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, scaffold  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_patriot_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Patriot'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Crate': (112, 100, 62)})

# ---------------------------------------------------------------------------------- as medidas do A (mantidas)
HX = HY = 4.0                    # o lote 8 × 8
BASE_R, BASE_H = 2.3, 1.1        # o bloco octogonal blindado (raio até o canto, altura)
PED_Z = BASE_H + 0.75            # o topo do pedestal (o prato giratório senta aqui)
MAST = (-2.35, -2.35)            # o mastro do radar (canto de trás, esquerda)
MAST_H = 4.4
DISH_AT = (MAST[0], MAST[1], MAST_H)
BOX = (3.8, 2.3, 1.7)            # a caixa lançadora: comprimento, largura, altura
BOX_PITCH = 45.0
BOX_C = (0.6, 0.0, 3.75)         # o centro da caixa (sobre o berço)
G = 0.13                         # o topo da laje (o pé do bloco)
APO = BASE_R * math.cos(math.radians(22.5))     # o apótema do bloco (2,125)
DECK = G + BASE_H                # o deck do bloco (1,23)

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def octa(rc, z, rot=0.0):
    """O anel octogonal (cantos a k·45°, como o cilindro de 8 lados do A: as faces olham pra 22,5° + k·45°)."""
    return [(rc * math.cos(math.radians(k * 45 + rot)), rc * math.sin(math.radians(k * 45 + rot)), z) for k in range(8)]


def face(k, a):
    """O referencial da face k do octógono de apótema a: (centro no chão, eu ao longo, en pra fora)."""
    th = math.radians(k * 45 + 22.5)
    en = Vector((math.cos(th), math.sin(th), 0.0))
    eu = Vector((-math.sin(th), math.cos(th), 0.0))
    return en * a, eu, en


# ---------------------------------------------------------------------------------- a base (casco)
def base():
    p = Part(PREFIX + '_Base')
    ub.keys(p)
    # ---------------------------------------------------------------- a borda de fundação do lote, a laje octogonal chanfrada
    for (x0, x1, y0, y1) in ((-HX, HX, -HY, -HY + 0.35), (-HX, HX, HY - 0.35, HY), (-HX, -HX + 0.35, -HY + 0.35, HY - 0.35),
                             (HX - 0.35, HX, -HY + 0.35, HY - 0.35)):
        ub.obox(p, 'Concrete#pnl', ((x0 + x1) / 2, (y0 + y1) / 2, -0.09), (x1 - x0, y1 - y0, 0.42), X, Y, Z)
    p.loft('Concrete#pnl', [octa(3.25, -0.13), octa(3.25, 0.06), octa(3.17, G)])
    # a faixa de perigo: o meio-fio baixo (no A era a tinta a 13 cm, boiando na areia nos cantos)
    for (x0, x1, y0, y1) in ((-2.95, 2.95, -2.95, -2.55), (-2.95, 2.95, 2.55, 2.95), (-2.95, -2.55, -2.55, 2.55), (2.55, 2.95, -2.55, 2.55)):
        ub.obox(p, 'Hazard#sm', ((x0 + x1) / 2, (y0 + y1) / 2, 0.05), (x1 - x0, y1 - y0, 0.19), X, Y, Z)
    # ---------------------------------------------------------------- o bloco octogonal blindado: o miolo, o rodapé, a rampa de cima
    p.loft('Body#pnl', [octa(BASE_R, G - 0.02), octa(BASE_R, 0.95), octa(BASE_R - 0.2, DECK - 0.02), octa(BASE_R - 0.24, DECK)])
    p.loft('Concrete#pnl', [octa(BASE_R + 0.14, G - 0.02), octa(BASE_R + 0.14, G + 0.12), octa(BASE_R + 0.08, G + 0.17)])
    for k in range(8):
        c0, eu, en = face(k, APO)
        w = 2 * BASE_R * math.sin(math.radians(22.5))
        org = c0 - eu * (w / 2 - 0.05) + Vector((0, 0, G + 0.12))
        ub.panel_wall(p, org, eu, Z, en, w - 0.1, 0.68, pw=(w - 0.1) / 2, m='Body#pnl', gap=0.06, proud=0.07, embed=0.04)
        # os parafusos nos cantos das placas (o ritmo que lê de longe)
        for uu in (0.12, w / 2 - 0.17, w / 2 + 0.07, w - 0.22):
            for zz in (G + 0.22, G + 0.7):
                vp.nut(p, 'Dark', c0 - eu * (w / 2 - 0.05) + eu * uu + en * 0.075 + Vector((0, 0, zz)), en, r=0.024, h=0.016)
        if k in (0, 7):
            # o painel azul (cor do jogador) nas 2 faces da frente, por cima das placas
            ub.obox(p, 'Player#sm', c0 + en * 0.1 + Vector((0, 0, G + 0.55)), (1.2, 0.4, 0.05), eu, Z, en)
    # as luminárias de serviço (2 faces do lado), na junta do meio
    for k in (1, 6):
        c0, eu, en = face(k, APO)
        q = c0 + Vector((0, 0, 0.6))
        ub.obox(p, 'Dark#sm', q + en * 0.12, (0.34, 0.14, 0.16), eu, en, Z)
        ub.obox(p, 'Light#vp', q + en * 0.19 - Vector((0, 0, 0.03)), (0.26, 0.012, 0.07), eu, en, Z)
        ub.obox(p, 'Dark#sm', q + en * 0.2 + Vector((0, 0, 0.085)), (0.38, 0.18, 0.02), eu, en, Z)
    # o deck: a chapa de cima e o poço do rolamento em volta do pedestal
    p.loft('Body#pnl', [octa(BASE_R - 0.22, DECK - 0.04), octa(BASE_R - 0.22, DECK + 0.06), octa(BASE_R - 0.3, DECK + 0.1)])
    vp.lathe(p, 'Dark#vp', (0, 0, DECK + 0.1), 'z', [(1.15, 0.0), (1.42, 0.0), (1.42, 0.012), (1.15, 0.012)], seg=48, closed=True)
    vp.bolt_ring(p, 'Trim', (0, 0, DECK + 0.1), 'z', 1.55, 24, r=0.022, h=0.016)
    # ---------------------------------------------------------------- o pedestal torneado, as nervuras, a escotilha, o anel de rolamento
    z0 = DECK + 0.1
    vp.lathe(p, 'Body#vp', (0, 0, z0), 'z', [(0.0, 0.0), (1.22, 0.0), (1.22, 0.05), (1.12, 0.1), (1.06, 0.16), (1.06, PED_Z - z0 - 0.06),
                                              (1.16, PED_Z - z0 - 0.04), (1.16, PED_Z - z0 + 0.04), (0.0, PED_Z - z0 + 0.04)], seg=40)
    for k in range(8):
        a = math.radians(k * 45 + 22.5)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        if k == 4:
            continue
        ub.obox(p, 'Body#sm', d * 1.1 + Vector((0, 0, z0 + 0.38)), (0.12, 0.08, 0.52), d, d.cross(Vector(Z)) * -1, Z)
    d = Vector((-1.0, 0.0, 0.0))                          # a escotilha do pedestal (de trás, onde a escada chega)
    vp.prism(p, 'Body#vp', vp.rounded_rect(0.6, 0.42, 0.08, 3), Vector((-1.0, 0, z0 + 0.42)), Vector((0, -1, 0)), Vector(Z), d, 0.1)
    vp.hinge(p, 'Trim', (-1.11, 0.0, z0 + 0.66), 'y', 0.5, r=0.022, knuckles=3)
    vp.grab_handle(p, 'Trim', (-1.11, -0.15, z0 + 0.32), (-1.11, 0.15, z0 + 0.32), (-1, 0, 0), height=0.05, r=0.014)
    vp.lathe(p, 'Trim#vp', (0, 0, PED_Z + 0.04), 'z', [(0.0, 0.0), (1.2, 0.0), (1.2, 0.08), (1.14, 0.11), (0.0, 0.11)], seg=48)
    vp.bolt_ring(p, 'Dark', (0, 0, PED_Z + 0.15), 'z', 1.08, 24, r=0.022, h=0.016)
    # ---------------------------------------------------------------- o mastro cônico do radar, a plataforma, a escada, a caixa de serviço
    mx, my = MAST
    ub.obox(p, 'Concrete#pnl', (mx, my, 0.02), (0.8, 0.8, 0.36), X, Y, Z)
    vp.lathe(p, 'Body#vp', (mx, my, 0.2), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.05), (0.2, 0.09), (0.19, 0.2), (0.135, MAST_H - 0.4),
                                                (0.0, MAST_H - 0.4)], seg=20)
    for zz in (1.6, 3.0):                                    # as flanges das emendas
        vp.lathe(p, 'Trim#vp', (mx, my, zz), 'z', [(0.0, -0.03), (0.22, -0.03), (0.22, 0.03), (0.0, 0.03)], seg=20)
    vp.lathe(p, 'Trim#vp', (mx, my, MAST_H - 0.2), 'z', [(0.0, 0.0), (0.14, 0.0), (0.36, 0.08), (0.38, 0.14), (0.38, 0.2), (0.0, 0.2)],
             seg=24)
    ub.ladder(p, 'Trim', mx, my, 0.2, MAST_H - 0.3, (-0.707, -0.707, 0.0), w=0.42, cage=False, wall=0.24, top_ext=0.0)
    # a caixa de serviço no pé do mastro (de frente pro bloco) com a veneziana e a porta
    sb = Vector((mx, my + 0.42, 0.0))
    ub.obox(p, 'Dark#pnl', (sb.x, sb.y, 0.72), (0.72, 0.36, 1.2), X, Y, Z)
    ub.louvers(p, (sb.x - 0.15, sb.y + 0.19, 0.95), (0, 1, 0), (1, 0, 0), 0.26, 0.42, 5, frame='Trim', slat='Trim', back='Dark')
    vp.panel_seam(p, 'Dark', (sb.x + 0.08, sb.y + 0.181, 0.3), (sb.x + 0.08, sb.y + 0.181, 1.2), (0, 1, 0), w=0.02, d=0.01)
    vp.grab_handle(p, 'Trim', (sb.x + 0.2, sb.y + 0.18, 0.62), (sb.x + 0.2, sb.y + 0.18, 0.86), (0, 1, 0), height=0.04, r=0.012)
    # o conduíte da caixa até o bloco (a calha no chão)
    ub.pipe(p, 'Trim', [(sb.x + 0.36, sb.y, 0.32), (-1.75, -1.2, 0.32), (-1.62, -1.08, 0.32)], 0.06)
    # a sirene âmbar no braço do mastro
    arm = vp.bent_bar([Vector((mx + 0.16, my - 0.16, 1.48)), Vector((mx + 0.32, my - 0.32, 1.48)), Vector((mx + 0.32, my - 0.32, 1.58))],
                      R=0.06, n=3)
    vp.tube(p, 'Trim', arm, 0.03, seg=8)
    ub.beacon(p, (mx + 0.32, my - 0.32, 1.58), r=0.09, h=0.24)
    # ---------------------------------------------------------------- o gerador (frente-direita): placas, o radiador com o ventilador
    # em cima (lê de cima), a veneziana, o escapamento, a porta
    gx, gy = 2.4, 2.2
    ub.obox(p, 'Trim#sm', (gx, gy, 0.16), (1.6, 1.08, 0.1), X, Y, Z)                     # o skid
    ub.obox(p, 'Body#pnl', (gx, gy, 0.42), (1.42, 0.92, 0.44), X, Y, Z)
    ub.square_annulus(p, 'Body#sm', (gx + 0.2, gy), 0.55, 0.47, 0.34, 1.12, 0.48)
    ub.obox(p, 'Body#sm', (gx - 0.55, gy, 0.88), (0.4, 0.94, 0.48), X, Y, Z)
    ub.fan(p, (gx + 0.2, gy), 0.34, 1.12, depth=0.42)
    ub.louvers(p, (gx + 0.76, gy, 0.42), (1, 0, 0), (0, 1, 0), 0.62, 0.3, 4)
    ub.obox(p, 'Player#sm', (gx, gy - 0.485, 0.86), (1.3, 0.03, 0.26), X, Y, Z)        # a faixa azul do gerador
    vp.panel_seam(p, 'Dark', (gx - 0.2, gy + 0.461, 0.25), (gx - 0.2, gy + 0.461, 0.6), (0, 1, 0), w=0.022, d=0.01)
    vp.grab_handle(p, 'Trim', (gx - 0.08, gy + 0.46, 0.3), (gx - 0.08, gy + 0.46, 0.55), (0, 1, 0), height=0.045, r=0.014)
    ex_ = Vector((gx - 0.5, gy - 0.3, 1.12))
    vp.lathe(p, 'Dark#vp', ex_, 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.04), (0.065, 0.06), (0.065, 0.55), (0.0, 0.55)], seg=14)
    vp.tube(p, 'Dark', vp.bent_bar([ex_ + Vector((0, 0, 0.5)), ex_ + Vector((0, 0, 0.62)), ex_ + Vector((-0.12, 0, 0.66))], R=0.06, n=4),
            0.065, seg=12)
    vp.lathe(p, 'Trim#vp', (gx - 0.62, gy + 0.3, 1.12), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.05), (0.05, 0.07), (0.0, 0.07)], seg=12)
    # ---------------------------------------------------------------- a escada do deck com o corrimão (na face de trás-esquerda, k=3: no
    # A ela ficava em −X, num canto do octógono)
    c0, eu, en = face(3, APO)
    run, n_st = 1.3, 5
    for k in range(n_st):
        t = (k + 1) / (n_st + 1)
        q = c0 + en * (run * (1 - t) + 0.02) + Vector((0, 0, G + (DECK - G) * t))
        ub.obox(p, 'Trim#sm', q, (0.26, 0.86, 0.045), en, eu, Z)
        ub.obox(p, 'Dark#vp', q + Vector((0, 0, 0.024)), (0.2, 0.76, 0.006), en, eu, Z)
    st = (-en * run + Vector((0, 0, DECK - G))).normalized()
    for s_ in (-1, 1):
        a = c0 + en * (run + 0.02) + eu * (s_ * 0.45) + Vector((0, 0, G))
        b = c0 + en * 0.02 + eu * (s_ * 0.45) + Vector((0, 0, DECK))
        ub.obox(p, 'Trim#sm', (a + b) / 2, ((b - a).length, 0.04, 0.16), st, eu, st.cross(eu))
        vp.tube(p, 'Trim', vp.bent_bar([a + Vector((0, 0, 0.0)), a + Vector((0, 0, 1.0)), b + Vector((0, 0, 1.0)), b - en * 0.25 + Vector((0, 0, 1.0))],
                                        R=0.12, n=4), 0.024, seg=8)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- a caixa lançadora (torre)
def turret():
    p = Part(PREFIX + '_Turret')
    ub.keys(p)
    # o prato giratório com a borda e os parafusos
    z0 = PED_Z + 0.2
    vp.lathe(p, 'Body#vp', (0, 0, z0), 'z', [(0.0, 0.0), (1.02, 0.0), (1.08, 0.04), (1.08, 0.3), (1.02, 0.35), (0.0, 0.35)], seg=40)
    vp.lathe(p, 'Dark#vp', (0, 0, z0 + 0.35), 'z', [(0.0, 0.0), (0.92, 0.0), (0.92, 0.02), (0.0, 0.02)], seg=40)
    vp.bolt_ring(p, 'Trim', (0, 0, z0 + 0.35), 'z', 0.98, 20, r=0.02, h=0.016)
    # o berço: os 2 braços (trapézio de placa grossa com o furo de alívio escuro e a nervura), a mesa, os munhões torneados
    for s in (-1, 1):
        prof = [(-0.9, PED_Z + 0.55), (0.8, PED_Z + 0.55), (0.6, 3.0), (-0.3, 3.0)]
        vp.prism(p, 'Body#pnl', prof, Vector((0, 0.95 * s if s > 0 else -1.2, 0)), Vector(X), Vector(Z), Vector(Y), 0.25)
        vp.lathe(p, 'Dark#vp', (-0.05, (1.2 + 0.002) * s, 2.68), (0, s, 0), [(0.0, -0.01), (0.2, -0.01), (0.2, 0.0), (0.0, 0.0)], seg=20)
        vp.lathe(p, 'Trim#vp', (0.15, 1.2 * s, 2.95), (0, s, 0), [(0.0, 0.0), (0.32, 0.0), (0.32, 0.06), (0.28, 0.1), (0.14, 0.1),
                                                                  (0.12, 0.16), (0.0, 0.16)], seg=24)
        vp.bolt_ring(p, 'Dark', (0.15, (1.2 + 0.06) * s, 2.95), (0, s, 0), 0.24, 8, r=0.02, h=0.014)
    ub.obox(p, 'Body#pnl', (-0.05, 0.0, PED_Z + 0.7), (1.7, 1.9, 0.3), X, Y, Z)
    # o cilindro de elevação: o corpo amarelo, a haste, os olhais
    a, b = Vector((-1.0, 0.0, PED_Z + 0.62)), Vector((-0.1, 0.0, 3.1))
    u = (b - a).normalized()
    L = (b - a).length
    vp.lathe(p, 'Yellow#vp', a, u, [(0.0, 0.0), (0.12, 0.0), (0.15, 0.04), (0.15, L * 0.5), (0.17, L * 0.52), (0.17, L * 0.56), (0.07, L * 0.58),
                                    (0.0, L * 0.58)], seg=16)
    vp.lathe(p, 'Trim#vp', a, u, [(0.0, L * 0.5), (0.075, L * 0.5), (0.075, L - 0.05), (0.0, L - 0.05)], seg=12)
    for q in (a, b):
        vp.lathe(p, 'Trim#vp', q, (0, 1, 0), [(0.0, -0.09), (0.1, -0.09), (0.1, 0.09), (0.0, 0.09)], seg=14)
    # ---------------------------------------------------------------- a caixa lançadora
    Lb, W, H = BOX
    C = Vector(BOX_C)
    ca, sa = math.cos(math.radians(BOX_PITCH)), math.sin(math.radians(BOX_PITCH))
    fwd = Vector((ca, 0.0, sa))
    up = Vector((-sa, 0.0, ca))
    ey = Vector(Y)
    tail = C - fwd * (Lb / 2)
    nose = C + fwd * (Lb / 2)
    # o corpo: o retângulo de cantos redondos (14 cm) varrido ao longo do eixo, as pontas com o chanfro de 2,4 cm
    vp.prism(p, 'Body#sm', vp.rounded_rect(W, H, 0.14, 4), tail, ey, up, fwd, Lb)
    # os 3 aros de reforço (a cinta em volta, 12 cm, 4 cm pra fora)
    for t in (0.22, 1.9, Lb - 0.3):
        vp.prism(p, 'Body#sm', vp.rounded_rect(W + 0.08, H + 0.08, 0.18, 4), tail + fwd * (t - 0.06), ey, up, fwd, 0.12)
    # a faixa azul dos 2 lados (entre os aros) e a junta escura das células
    for s in (-1, 1):
        q = C + ey * (s * (W / 2 + 0.006)) - up * (H * 0.27) + fwd * (-0.85)
        ub.obox(p, 'Player#sm', q, (1.35, 0.03, 0.3), fwd, ey, up)
        q = C + ey * (s * (W / 2 + 0.006)) - up * (H * 0.27) + fwd * 0.8
        ub.obox(p, 'Player#sm', q, (1.15, 0.03, 0.3), fwd, ey, up)
        vp.panel_seam(p, 'Dark', C + ey * (s * W / 2) - fwd * (Lb / 2 - 0.4), C + ey * (s * W / 2) + fwd * (Lb / 2 - 0.4), ey * s, w=0.03,
                      d=0.012)
    vp.panel_seam(p, 'Dark', C + up * (H / 2) - fwd * (Lb / 2 - 0.4), C + up * (H / 2) + fwd * (Lb / 2 - 0.4), up, w=0.03, d=0.012)
    # o painel azul em cima (lê de cima), a escotilha de manutenção e as olhais de içar
    ub.obox(p, 'Player#sm', C + up * (H / 2 + 0.012) - fwd * 0.85 + ey * 0.42, (1.2, 0.62, 0.025), fwd, ey, up)
    hb = C + up * (H / 2) - fwd * 0.85 - ey * 0.5
    vp.prism(p, 'Body#vp', vp.rounded_rect(0.6, 0.5, 0.08, 3), hb, fwd, ey, up, 0.035)
    vp.hinge(p, 'Trim', hb - fwd * 0.33 + up * 0.03, ey, 0.42, r=0.02, knuckles=3)
    for t in (0.5, Lb - 0.5):
        for s in (-1, 1):
            q = tail + fwd * t + up * (H / 2 + 0.04) + ey * (s * (W / 2 - 0.18))
            vp.lathe(p, 'Trim#vp', q - up * 0.04, up, [(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.0, 0.03)], seg=10)
            p.torus('Trim#vp', tuple(q + up * 0.05), tuple(fwd) if False else 'y', 0.05, 0.016, seg=12, rseg=6)
    # a frente: a moldura de cada célula, a tampa abaulada (a cúpula frangível) com as nervuras e o cubo
    for (dy, dz) in ((-0.56, -0.41), (0.56, -0.41), (-0.56, 0.41), (0.56, 0.41)):
        c = nose + ey * dy + up * dz
        for sg in (-1, 1):
            ub.obox(p, 'Body#sm', c + ey * (sg * 0.5) + fwd * 0.03, (0.06, 0.08, 0.86), fwd, ey, up)
            ub.obox(p, 'Body#sm', c + up * (sg * 0.39) + fwd * 0.03, (0.06, 1.06, 0.08), fwd, ey, up)
        # a tampa frangível: o aro de borracha, a cúpula rasa com o degrau do anel de ruptura e o centro (não é ventilador: sem pás)
        vp.lathe(p, 'Trim#vp', c, fwd, [(0.0, 0.004), (0.37, 0.004), (0.39, 0.03), (0.36, 0.05), (0.3, 0.06), (0.29, 0.075),
                                         (0.2, 0.09), (0.0, 0.1)], seg=32)
        vp.lathe(p, 'Body#vp', c + fwd * 0.07, fwd, [(0.0, 0.0), (0.21, 0.0), (0.2, 0.03), (0.12, 0.045), (0.0, 0.05)], seg=24)
    # atrás: as venezianas do escape de cada célula
    for (dy, dz) in ((-0.56, -0.41), (0.56, -0.41), (-0.56, 0.41), (0.56, 0.41)):
        c = tail + ey * dy + up * dz - fwd * 0.004
        ub.louvers(p, c, -fwd, ey, 0.8, 0.6, 5)
    # o cabo do berço até a caixa
    vp.tube(p, 'Dark', vp.bent_bar([Vector((-0.4, 1.0, 3.0)), Vector((-0.55, 1.0, 3.25)), tail + fwd * 1.2 + ey * 1.16 - up * 0.3],
                                   R=0.15, n=4), 0.035, seg=8)
    # o sensor de tiro no munhão direito: a caixa, a capa, a lente
    sc = Vector((0.5, 1.42, 3.12))
    ub.obox(p, 'Dark#sm', sc, (0.4, 0.3, 0.34), X, Y, Z)
    ub.obox(p, 'Dark#sm', sc + Vector((0.08, 0, 0.19)), (0.5, 0.34, 0.03), X, Y, Z)
    vp.lathe(p, 'Dark#vp', sc + Vector((0.2, 0, 0)), 'x', [(0.0, 0.0), (0.11, 0.0), (0.11, 0.06), (0.0, 0.06)], seg=16)
    vp.lathe(p, 'Glass#vp', sc + Vector((0.26, 0, 0)), 'x', [(0.0, 0.0), (0.085, 0.0), (0.075, 0.015), (0.0, 0.02)], seg=16)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o radar (origem no eixo)
def dish():
    """O radar no topo do mastro: origem no eixo (DISH_AT); o jogo gira em Z (spin, ~12 rpm, para sem energia). O mesmo envelope do
    A (o prato de 1,6 m inclinado 35°)."""
    p = Part(PREFIX + '_Dish')
    ub.keys(p)
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.04), (0.13, 0.07), (0.12, 0.16), (0.0, 0.16)], seg=16)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.04), 'z', 0.165, 8, r=0.014, h=0.01)
    ub.obox(p, 'Body#sm', (0, 0, 0.34), (0.26, 0.26, 0.38), X, Y, Z)
    for s in (-1, 1):                                       # o garfo e o eixo de elevação
        ub.obox(p, 'Body#sm', (0.0, s * 0.2, 0.52), (0.16, 0.05, 0.22), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (0, 0, 0.55), 'y', [(0.0, -0.26), (0.08, -0.26), (0.08, 0.26), (0.0, 0.26)], seg=12)
    a = math.radians(35)
    n = Vector((math.cos(a), 0.0, math.sin(a)))
    c = Vector((0.0, 0.0, 0.55)) + n * 0.02
    # o prato parabólico (a casca com 3 cm, a borda virada) e o aro
    Rr, F = 0.78, 0.55
    prof = [(r, r * r / (4 * F)) for r in [0.0, 0.12, 0.25, 0.38, 0.5, 0.62, 0.72, Rr]]
    shell = prof + [(Rr + 0.03, Rr * Rr / (4 * F) + 0.02), (Rr + 0.03, Rr * Rr / (4 * F) - 0.02)] + \
        [(r, r * r / (4 * F) - 0.035) for r, _ in reversed(prof)]
    # (o prato cinza-escuro de metal: na câmera do jogo o claro sumia contra a areia; o A lia como o disco escuro)
    vp.lathe(p, 'Trim#vp', c, n, shell, seg=40, smooth=True, hard=60.0)
    vp.lathe(p, 'Dark#vp', c, n, [(Rr - 0.01, Rr * Rr / (4 * F) - 0.03), (Rr + 0.045, Rr * Rr / (4 * F) - 0.03),
                                  (Rr + 0.045, Rr * Rr / (4 * F) + 0.035), (Rr - 0.01, Rr * Rr / (4 * F) + 0.035)], seg=40, closed=True)
    # as costelas atrás e o cubo
    _, e1, e2 = vp.axis_frame(n)
    for k in range(6):
        t = 2 * math.pi * (k + 0.5) / 6
        d = e1 * math.cos(t) + e2 * math.sin(t)
        pts = [c + d * r + n * (r * r / (4 * F) - 0.05) for r in (0.18, 0.4, 0.6, 0.74)]
        vp.tube(p, 'Dark', pts, 0.022, seg=6)
    vp.lathe(p, 'Dark#vp', c - n * 0.2, n, [(0.0, 0.0), (0.16, 0.0), (0.2, 0.12), (0.2, 0.17), (0.0, 0.17)], seg=16)
    # o tripé do alimentador e a corneta no foco
    f = c + n * F
    for k in range(3):
        t = 2 * math.pi * k / 3 + math.pi / 2
        d = e1 * math.cos(t) + e2 * math.sin(t)
        q0 = c + d * 0.7 + n * (0.49 / (4 * F) + 0.01)
        vp.tube(p, 'Trim', [q0, f - n * 0.04], 0.016, seg=6)
    vp.lathe(p, 'Body#vp', f - n * 0.06, n, [(0.0, 0.0), (0.05, 0.0), (0.09, 0.1), (0.1, 0.14), (0.0, 0.14)], seg=14)
    return ub.finish(p)


def write_spec():
    import json
    spec = {
        'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Turret', [0, 0, 0]], [PREFIX + '_Dish', [DISH_AT[0], -DISH_AT[1], DISH_AT[2]]]],
        'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1200, 1200]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1200, 900]],
                  ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1200, 900]],
                  ['three_quarter', 'persp', 32.0, 215.0, [1600, 1000], 30.0, 0.95],
                  ['three_quarter_right', 'persp', 32.0, 145.0, [1600, 1000], 30.0, 0.95],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.95]],
        'usa_at': [0, 7.0, 0],
        'game': [['game_cam', 37.5, 200.0, 50.0, 0.8], ['game_cam_close', 37.5, 200.0, 28.0, 0.75]],
        'scaffold': PREFIX + '_Scaffold',
    }
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    t = turret()
    d = bake_in_place(dish(), DISH_AT)
    # a folga do giro: o raio da caixa na altura do radar contra o prato (o A tinha o mesmo envelope)
    vs = [t.matrix_world @ v.co for v in t.data.vertices if v.co.z > MAST_H - 0.1]
    r_box = max((v.x ** 2 + v.y ** 2) ** 0.5 for v in vs) if vs else 0.0
    r_dish = math.hypot(*MAST) - 0.83
    print('MODEL giro: a caixa chega a %.2f m do eixo acima de %.1f m; o prato começa a %.2f m (%s)'
          % (r_box, MAST_H - 0.1, r_dish, 'folga' if r_box < r_dish else 'TOCA'))
    total = 0
    for o in (b, t, d):
        total += export(o, OUT)
    total += export(scaffold(PREFIX + '_Scaffold', -3.0, 3.0, -3.0, 3.0, 5.3), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, t, d)})
    print('MODEL turret_pivot_m [0, 0]; muzzle_height_m', BOX_C[2], '; spin dish at_m', DISH_AT, 'rpm 12')
    write_spec()


main()
