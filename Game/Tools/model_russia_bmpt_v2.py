"""O BMPT "Terminator" da Rússia, v2 (01/11/2026): refeito pelo veredito da galeria ("muito parecido com o outro tanque", o T-90M).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_russia_bmpt_v2.py C:/Users/luisf/explosion/captures/game/models/russia_bmpt_v2

A regra (docs/elenco/SILHUETAS_USA.md, a mesma pra Rússia): cada blindado diferente DE CIMA em cinco coisas, não só na torre.
Contra o T-90M (model_russia_t90m.py, o de referência):
| | T-90M | BMPT v2 |
| tamanho | casco 6,9 x 3,8 m, 9,5 m com o cano | casco 6,1 x 3,4 m (o mais curto dos 3 de lagarta), ~7,2 m com os canos, mas 4,3 m de LARGURA nos ombros |
| contorno do casco | retângulo, frente reta | retângulo curto com os 2 CASULOS dos lança-granadas AG-17 nas quinas da frente (o Object 199 original) |
| a torre | redonda, o Relikt em V, a gaiola por cima | baixa e larga, SEM canhão principal: o módulo blindado dos 2 canhões 2A42 no meio e as 2 CAIXAS grandes do Ataka nos ombros, pra fora das esteiras (a silhueta em cruz) |
| a assinatura | a gaiola com a lona e a rede | OS OMBROS VERMELHOS: as capas de lona da cor do jogador amarradas por cima das 2 caixas do Ataka (de cima, as 2 barras vermelhas dos lados) e os canos gêmeos curtos |
| laterais | o Relikt na frente, a grade atrás | a grade de barras na lateral INTEIRA (as rodas aparecem entre as barras) |
| rodas | 6 | 5 (o BMPT de verdade tem 6: aqui 5 pra ler menor e diferente; a escapada da regra do elenco) |
A gaiola da facção não cobre a torre (o módulo é remoto: ninguém embaixo pra proteger, e a torre tem que ler): vira a grade baixa
anti-drone por cima do motor (o "mangal" dos tanques de 2023), com a lona enrolada da cor do jogador. O emblema
(docs/elenco/EMBLEMAS.md); na lateral das caixas, a estrela aprovada (a vermelha com as bordas branca, azul e vermelha) e o número
tático branco.

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar):
  SM_Russia_BMPT_Hull     casco, saias de grade, os casulos AG-17, o deck com a grade anti-drone e a lona enrolada, o anel;
  SM_Russia_BMPT_Turret   a torre (gira em Z em volta de (TX, 0)), o módulo dos canhões, as caixas do Ataka com as capas, as miras;
  SM_Russia_BMPT_Barrel   os 2 canos de 30 mm (recuam em −X; as bocas são o X máximo);
  SM_Russia_BMPT_Wheels   a rodagem (o executor separa em wheel_*);
  SM_Russia_BMPT_Track_l / _Track_r  as esteiras.
Slots: Paint, Metal, Team, Track, Lens, Cloth, Rubber, Red, White, Navy (a borda azul da estrela), Dark.
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import clear_scene, export, sheet  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_russia_vehicle_kit as rk  # noqa: E402
from model_russia_vehicle_kit import bounds, frame_box, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_bmpt_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_BMPT'


class B:
    """O chassi curto do BMPT v2 (5 rodas de 750 mm, a mesma roda do T-72; esteira de 560 mm)."""
    NOSE, REAR = 3.05, -3.05
    BELLY, DECK = 0.47, 1.40
    LOW_HW = 1.0
    UP_HW = 1.16
    FENDER_Z = 1.14
    FENDER_HW = 1.66
    TRACK_Y, TRACK_W = 1.32, 0.56
    WHEEL_R = 0.375
    WHEEL_Z = 0.465
    WHEELS_X = [2.0, 1.0, 0.0, -1.0, -2.0]
    IDLER = (2.70, 0.60, 0.27)
    SPROCKET = (-2.72, 0.64, 0.29)
    ROLLERS_X, ROLLER_Z, ROLLER_R = [1.5, 0.0, -1.5], 0.90, 0.075
    GLACIS = ((3.05, 1.0), (2.05, 1.40))
    RING_X, RING_R = 0.15, 1.04


C = B
TX = C.RING_X
T_Z0, T_Z1, T_Z2 = 1.46, 1.80, 1.93          # a torre baixa
MOD_X = (TX - 0.55, TX + 1.15)               # o módulo dos canhões (a caixa blindada de cima)
MOD_Z = 2.46
GUN_Z = 2.20                                 # o eixo dos 2 canhões
GUN_Y = 0.17
MUZZLE_X = MOD_X[1] + 0.3 + 2.35             # ~0,9 m além do nariz
ATK = dict(x0=TX - 1.0, x1=TX + 1.4, yc=1.79, hw=0.31, z0=1.78, z1=2.36)   # a caixa do Ataka (cada lado)
DECK_CAGE = dict(x0=C.REAR + 0.06, x1=-1.95, hw=1.1, z=1.68)               # a grade anti-drone do motor (fora do giro da torre)

# o número tático pintado (estêncil de 7 traços; o "2" do batalhão, nunca letra)
SEG = {'a': ((0.0, 1.0), (1.0, 1.0)), 'b': ((1.0, 0.5), (1.0, 1.0)), 'c': ((1.0, 0.0), (1.0, 0.5)), 'd': ((0.0, 0.0), (1.0, 0.0)),
       'e': ((0.0, 0.0), (0.0, 0.5)), 'f': ((0.0, 0.5), (0.0, 1.0)), 'g': ((0.0, 0.5), (1.0, 0.5))}
DIG = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc', '8': 'abcdefg',
       '9': 'abcdfg'}


def tac_number(p, c, s, text, h=0.22, m='White'):
    """O número tático branco em estêncil (os traços com a ponte do molde nas pontas) pintado na face lateral de normal (0, s, 0), o
    centro da linha em c; lê da esquerda pra direita pra quem olha de fora."""
    c = Vector(c)
    rd = Vector((s, 0.0, 0.0))                  # o sentido de leitura (quem olha o lado +y vê a frente à direita)
    n = Vector((0.0, s, 0.0))
    up = Vector((0.0, 0.0, 1.0))
    w, gap, sw = h * 0.56, h * 0.3, h * 0.16
    total = len(text) * w + (len(text) - 1) * gap
    for i, ch in enumerate(text):
        ox = -total / 2 + i * (w + gap)
        for sg in DIG.get(ch, ''):
            (u0, v0), (u1, v1) = SEG[sg]
            a = c + rd * (ox + u0 * w) + up * ((v0 - 0.5) * h)
            b = c + rd * (ox + u1 * w) + up * ((v1 - 0.5) * h)
            d = (b - a)
            L = d.length
            ex = d.normalized()
            ey = up if abs(ex.z) < 0.5 else rd
            ey = ey - n * ey.dot(n)
            ey.normalize()
            frame_box(p, m + '#vp', (a + b) / 2 + n * 0.004, ex, ey, n, max(0.02, L - sw * 0.9), sw, 0.006)


def glacis_z(x):
    (x0, z0), (x1, z1) = C.GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


# ---------------------------------------------------------------------------------- casco
def grenade_pod(p, s):
    """O casulo blindado do lança-granadas AG-17D na quina da frente (o Object 199): a caixa de chapa com a frente em cunha, a
    rótula com o cano curto, o periscópio do operador e a escotilha, o farol embaixo da rótula."""
    y_in, y_out = (C.UP_HW - 0.03), C.FENDER_HW - 0.04
    x0, x1 = 2.12, 2.98
    zb, zt = C.FENDER_Z - 0.01, 1.56
    pts = []
    for y in (y_in, y_out):
        yy = y * s
        pts += [(x0, yy, zb), (x0, yy, zt), (x1 - 0.32, yy, zt), (x1, yy, zt - 0.2), (x1, yy, zb)]
    solid(p, 'Paint#shell', pts, bevel=0.04)
    yc = (y_in + y_out) / 2 * s
    # a rótula (a bola blindada) e o cano do AG-17 saindo pela frente inclinada
    vp.lathe(p, 'Paint', (x1 - 0.06, yc, 1.33), 'x', [(0.0, -0.02), (0.1, -0.02), (0.13, 0.03), (0.13, 0.08), (0.1, 0.11), (0.0, 0.11)],
             seg=20)
    vp.gun_barrel(p, [(x1 + 0.05, 0.05), (x1 + 0.12, 0.05), (x1 + 0.14, 0.036), (x1 + 0.48, 0.034), (x1 + 0.5, 0.042), (x1 + 0.58, 0.042)],
                  1.33, y=yc, m='Metal', seg=14, bore=0.02, bore_depth=0.1, bore_m='Dark')
    # o periscópio e a escotilha pequena no teto do casulo, as alças
    vp.rbox(p, 'Paint#plate', (x1 - 0.42, yc, zt + 0.06), (0.16, 0.24, 0.12), r=0.03, k=2)
    p.obox('Lens', (x1 - 0.335, yc, zt + 0.07), (0.012, 0.18, 0.05), bevel=0.0)
    vp.hatch_round(p, (x0 + 0.3, yc, zt), 0.17, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.grab_handle(p, 'Metal', (x0 + 0.08, y_out * s, zt - 0.1), (x0 + 0.45, y_out * s, zt - 0.1), (0, s, 0), height=0.05, r=0.013)
    # o farol com a grade embaixo da rótula
    rk.headlight(p, (x1 + 0.0, (y_in + 0.08) * s, 1.2), r=0.045, guard=False)
    vp.bolt_row(p, 'Metal', (x0 + 0.1, y_out * s, zb + 0.08), (x1 - 0.1, y_out * s, zb + 0.08), 5, (0, s, 0), r=0.016, h=0.012)


def hull():
    p = rk.new_part(PREFIX + '_Hull')
    # ---------------------------------------------------------------- o casco de baixo e o de cima (o T-72 encurtado)
    low = [(2.68, C.BELLY), (C.NOSE - 0.04, 0.92), (C.NOSE - 0.04, C.FENDER_Z), (C.REAR + 0.02, C.FENDER_Z), (C.REAR + 0.02, 0.66),
           (-2.85, C.BELLY)]
    solid(p, 'Paint#shell', [(x, s * C.LOW_HW, z) for x, z in low for s in (-1, 1)])
    (gx0, gz0), (gx1, gz1) = C.GLACIS
    up = [(C.NOSE, C.FENDER_Z - 0.04), (C.NOSE, gz0), (gx1, gz1), (C.REAR + 0.09, C.DECK), (C.REAR, 1.32), (C.REAR, C.FENDER_Z - 0.04)]
    solid(p, 'Paint#shell', [(x, s * C.UP_HW, z) for x, z in up for s in (-1, 1)])
    vp.weld_bead(p, 'Paint#vp', [(C.NOSE - 0.005, -C.UP_HW + 0.05, gz0 - 0.01), (C.NOSE - 0.005, C.UP_HW - 0.05, gz0 - 0.01)], r=0.018)
    # a lâmina de autoentrincheiramento dobrada na chapa de baixo do nariz
    a = math.degrees(math.atan2(0.92 - C.BELLY, C.NOSE - 0.04 - 2.68))
    p.obox('Paint#plate', (2.885, 0.0, 0.69), (0.07, 1.86, 0.42), pitch=-(90.0 - a), bevel=0.03)
    for y in (-0.58, 0.0, 0.58):
        p.obox('Paint#plate', (2.92, y, 0.675), (0.06, 0.08, 0.36), pitch=-(90.0 - a), bevel=0.02)
    # ---------------------------------------------------------------- o glacis: o Relikt em 2 fileiras, o vão do motorista
    eu = Vector((gx1 - gx0, 0.0, gz1 - gz0)).normalized()
    en = Vector((-eu.z, 0.0, eu.x)) * -1.0
    if en.z < 0:
        en = -en
    L = (Vector((gx1, 0, gz1)) - Vector((gx0, 0, gz0))).length
    nv = 6
    cv = (2 * C.UP_HW - 0.08) / nv
    rk.era_grid(p, Vector((gx0, -C.UP_HW + 0.04, gz0)) + eu * 0.06, eu, Vector((0, 1, 0)), en, 2, nv, (L - 0.28) / 2, cv, 0.1,
                skip=((1, 2), (1, 3)))
    hx = gx1 - 0.2
    vp.prism(p, 'Paint#plate', vp.rounded_rect(0.5, 0.56, 0.2, 3), Vector((hx, 0.0, C.DECK - 0.004)), Vector((1, 0, 0)),
             Vector((0, 1, 0)), Vector((0, 0, 1)), 0.045)
    vp.hinge(p, 'Metal', (hx - 0.27, 0.0, C.DECK + 0.012), 'y', 0.4, r=0.02, knuckles=3)
    sl = math.degrees(math.atan2(gz1 - gz0, gx0 - gx1))
    px = gx1 + 0.17
    vp.rbox(p, 'Paint#plate', (px, 0.0, glacis_z(px) + 0.06), (0.2, 0.32, 0.12), pitch=-sl, r=0.04, k=2)
    p.obox('Lens', (px + 0.1, 0.0, glacis_z(px) + 0.1), (0.012, 0.24, 0.05), pitch=-sl, bevel=0.0)
    # os ganchos de reboque na frente e atrás
    for s in (-1, 1):
        for xx, sg in ((C.NOSE - 0.04, 1), (C.REAR + 0.02, -1)):
            for d in (-1, 1):
                vp.rbox(p, 'Metal', (xx + 0.05 * sg, 0.66 * s + d * 0.055, 0.86), (0.12, 0.035, 0.15), r=0.03, k=2)
            p.cyl('Metal#vp', (xx + 0.07 * sg, 0.66 * s, 0.88), 'y', 0.022, 0.16, seg=10, bevel=0.0)
            p.torus('Dark#vp', (xx + 0.1 * sg, 0.66 * s, 0.81), 'y', 0.055, 0.017, seg=14, rseg=6)
    # ---------------------------------------------------------------- as prateleiras dos para-lamas e os casulos AG-17 nas quinas
    for s in (-1, 1):
        y0, y1 = sorted((C.UP_HW * s, C.FENDER_HW * s))
        vp.rbox(p, 'Paint#thin', ((C.NOSE - 0.1 + C.REAR + 0.02) / 2, (y0 + y1) / 2, C.FENDER_Z - 0.015),
                (C.NOSE - 0.1 - C.REAR - 0.02, y1 - y0, 0.03), r=0.03, k=2)
        vp.tube(p, 'Paint#vp', [(C.NOSE - 0.12, C.FENDER_HW * s, C.FENDER_Z), (C.REAR + 0.05, C.FENDER_HW * s, C.FENDER_Z)], 0.018, seg=6)
        vp.mudflap(p, (C.REAR + 0.05, 1.42 * s, C.FENDER_Z - 0.03), 0.46, 0.36, yaw=180, m='Rubber')
        grenade_pod(p, s)
        # as caixas da prateleira entre o casulo e a traseira (baixas: os canos e as caixas do Ataka passam por cima girando)
        yc = (C.UP_HW + C.FENDER_HW) / 2 * s
        for (xa, xb) in ((1.95, 1.2), (1.1, 0.35), (-1.25, -2.0)):
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, yc, C.FENDER_Z + 0.1), (xa - xb - 0.04, 0.44, 0.19), r=0.04, k=2)
            vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, yc, C.FENDER_Z + 0.205), (xa - xb, 0.47, 0.03), r=0.045, k=2)
            for d in (-0.18, 0.18):
                vp.rbox(p, 'Metal', ((xa + xb) / 2 + d, yc + 0.24 * s, C.FENDER_Z + 0.15), (0.05, 0.02, 0.06), r=0.008, k=1)
    # o tanque de combustível externo (direita) e a ferramenta (esquerda), entre as caixas
    vp.lathe(p, 'Paint', (-0.5, 1.42, C.FENDER_Z + 0.13), 'x', [(0.0, -0.32), (0.1, -0.32), (0.13, -0.29), (0.13, 0.29), (0.1, 0.32),
                                                             (0.0, 0.32)], seg=18)
    vp.crowbar(p, (-0.2, -1.5, C.FENDER_Z + 0.04), yaw=0, m='Metal', length=1.0)
    vp.shovel(p, (-0.75, -1.3, C.FENDER_Z + 0.03), yaw=180, handle_m='Metal', blade_m='Dark', length=0.9)
    # ---------------------------------------------------------------- a GRADE DE BARRAS na lateral inteira (a saia do BMPT v2)
    for s in (-1, 1):
        rk.slat_screen(p, 2.05, C.REAR + 0.12, 0.66, C.FENDER_Z + 0.04, (C.FENDER_HW - 0.12) * s, s, step=0.16)
        # a aba de borracha na frente da grade (o pedaço que protege o casulo da lama)
        vp.mudflap(p, (2.55, (C.FENDER_HW - 0.02) * s, C.FENDER_Z - 0.02), 0.85, 0.36, yaw=90 * s, m='Rubber', t=0.022, folds=1)
    # o forro escuro do casco de baixo atrás das rodas e os braços da suspensão
    for s in (-1, 1):
        yy0, yy1 = sorted((C.LOW_HW * s, (C.LOW_HW + 0.015) * s))
        p.box('Dark', -2.8, 2.7, yy0, yy1, 0.52, C.FENDER_Z - 0.05, bevel=0.0)
        for x in C.WHEELS_X:
            yy0, yy1 = sorted((C.LOW_HW * s, (C.TRACK_Y - C.TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.07, x + 0.3, yy0, yy1, C.WHEEL_Z - 0.05, C.WHEEL_Z + 0.05, bevel=0.0)
    # ---------------------------------------------------------------- o deck: o anel, as venezianas, a grade anti-drone, a lona
    vp.lathe(p, 'Dark', (C.RING_X, 0.0, C.DECK), 'z', [(C.RING_R - 0.06, 0.0), (C.RING_R + 0.06, 0.0), (C.RING_R + 0.06, 0.012),
                                                      (C.RING_R + 0.045, 0.02), (C.RING_R - 0.06, 0.02)], seg=64, closed=True)
    rk.grille_deck(p, -1.9, -1.1, -1.0, 1.0, C.DECK, 6, along='y')
    rk.grille_deck(p, -2.95, -2.05, -1.05, -0.08, C.DECK, 6, along='y')
    rk.grille_deck(p, -2.95, -2.05, 0.08, 1.05, C.DECK, 6, along='y')
    vp.bolt_row(p, 'Metal', (-0.98, -0.95, C.DECK), (-0.98, 0.95, C.DECK), 8, (0, 0, 1), r=0.016, h=0.012)
    d = DECK_CAGE
    posts = [(d['x1'] - 0.05, 0.95, C.DECK), (d['x1'] - 0.05, -0.95, C.DECK), (d['x0'] + 0.16, 0.95, C.DECK), (d['x0'] + 0.16, -0.95, C.DECK)]
    roof = rk.slat_cage(p, d['x0'], d['x1'], d['hw'], d['z'], d['z'], posts, visor=0.0, step=0.2, r=0.026)
    # a lona ENROLADA da cor do jogador amarrada em cima da grade, na quina de trás
    vp.tarp_roll(p, (d['x0'] + 0.22, 0.0, roof(d['x0'] + 0.22) + 0.1), 'y', 2.0, 0.095, m='Team', straps=3, strap_m='Cloth',
                 buckle_m='Metal')
    # a traseira: a fresta da tampa, as lanternas, a grade de barras, o cabo de reboque, o escape (esquerda)
    vp.panel_seam(p, 'Dark', (C.REAR, -0.95, 1.02), (C.REAR, 0.95, 1.02), (-1, 0, 0), w=0.024, d=0.01)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (C.REAR - 0.03, 1.05 * s, 1.22), (0.06, 0.16, 0.1), r=0.03, k=2)
        p.box('Lens', C.REAR - 0.07, C.REAR - 0.05, 1.05 * s - 0.05, 1.05 * s + 0.05, 1.19, 1.25, bevel=0.0)
    rk.slat_screen_x(p, -1.2, 1.2, 0.72, 1.3, C.REAR, -1)
    vp.tow_cable(p, 'Metal', [(C.REAR - 0.2, -0.8, 0.82), (C.REAR - 0.22, -0.55, 0.72), (C.REAR - 0.22, 0.55, 0.72),
                              (C.REAR - 0.2, 0.8, 0.82)], r=0.022)
    p.box('Dark', -2.4, -1.85, -C.UP_HW - 0.012, -C.UP_HW, C.FENDER_Z + 0.04, C.DECK - 0.04, bevel=0.0)
    for k in range(5):
        p.obox('Metal#vp', (-2.35 + k * 0.1, -C.UP_HW - 0.015, (C.FENDER_Z + C.DECK) / 2), (0.025, 0.025, 0.2), bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def ataka_box(p, s):
    """A caixa blindada do Ataka num ombro (2 tubos um em cima do outro, as tampas redondas na frente), o braço que a liga à torre, a
    CAPA DE LONA da cor do jogador amarrada por cima (as 3 cintas, a aba caída pra fora e atrás), o número tático embaixo da aba."""
    a = ATK
    y = a['yc'] * s
    yi, yo = (a['yc'] - a['hw']) * s, (a['yc'] + a['hw']) * s
    pts = []
    for xx in (a['x0'], a['x1'] - 0.14, a['x1']):
        dz = 0.05 if xx == a['x1'] else 0.0
        for yy in (yi, yo):
            pts += [(xx, yy, a['z0'] + dz), (xx, yy, a['z1'] - dz)]
    solid(p, 'Paint#shell', pts, bevel=0.04)
    # as tampas dos 2 tubos (a de cima e a de baixo) na frente
    for zz in (a['z0'] + 0.155, a['z1'] - 0.155):
        vp.lathe(p, 'Paint', (a['x1'], y, zz), 'x', [(0.0, 0.035), (0.11, 0.035), (0.13, 0.012), (0.13, -0.01), (0.0, -0.01)], seg=18)
        vp.lathe(p, 'Metal', (a['x1'] + 0.035, y, zz), 'x', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.016), (0.0, 0.016)], seg=12)
        vp.hinge(p, 'Metal', (a['x1'] + 0.02, y + 0.13 * s, zz), 'z', 0.18, r=0.016, knuckles=2)
    # o braço: o suporte da lateral da torre até a caixa (o pino de elevação) e o tirante de baixo
    vp.rbox(p, 'Paint#plate', (TX + 0.15, (1.17 + yi * s) / 2 * s + 0.0, 2.02), (0.62, abs(yi) - 1.15, 0.26), r=0.04, k=2)
    p.cyl('Metal#vp', (TX + 0.15, (abs(yi) - 0.04) * s, 2.02), 'y', 0.11, 0.1, seg=16, bevel=0.0)
    vp.tube(p, 'Metal', [(TX - 0.45, 1.16 * s, 1.68), (TX - 0.45, (abs(yi) + 0.02) * s, 1.86)], 0.035, seg=8)
    # (revisão) o BERÇO do ombro: a chapa da lateral da torre até embaixo da caixa e as 2 mãos-francesas (na 1ª volta a caixa parecia
    # solta no ar, presa só pelo braço do meio); o fundo a 1,74 m, 3 cm acima da grade do motor no giro
    sx0, sx1 = a['x0'] + 0.08, a['x1'] - 0.22
    p.box('Paint#shelf', sx0, sx1, *sorted((1.12 * s, (abs(yo) - 0.06) * s)), a['z0'] - 0.04, a['z0'] + 0.02, bevel=0.015)
    vp.bolt_row(p, 'Metal', (sx0 + 0.1, (abs(yo) - 0.06) * s, a['z0'] - 0.01), (sx1 - 0.1, (abs(yo) - 0.06) * s, a['z0'] - 0.01), 7,
                (0, s, 0), r=0.014, h=0.01)
    for xx in (sx0 + 0.25, TX + 0.5):
        vp.tube(p, 'Metal', [(xx, 1.14 * s, 1.56), (xx, (abs(yo) - 0.14) * s, a['z0'] - 0.05)], 0.035, seg=8)
    # a capa: o pano por cima da caixa (até 0,16 m antes da frente: as tampas ficam de fora), cedendo nas bordas, com as rugas
    x0, x1 = a['x0'] - 0.04, a['x1'] - 0.18
    ya, yb = sorted((yi - 0.035 * s, yo + 0.035 * s))
    zt = a['z1'] + 0.012

    def zf(u, v, z):
        edge = min(u, 1 - u, v, 1 - v)
        return zt + 0.012 * math.sin(u * 17.0 + s) * math.sin(v * 5.0) - 0.03 * max(0.0, 0.12 - edge) / 0.12 + 0.01
    sheet(p, 'Team#cloth', [(x0, ya, zt), (x1, ya, zt), (x1, yb, zt), (x0, yb, zt)], 34, 8, z_fn=zf, thick=0.016)
    # as abas caídas: pra fora (cobre metade da lateral), atrás, e a de dentro curta
    vp.tarp_drape(p, (x0, yo + 0.035 * s, zt), (x1, yo + 0.035 * s, zt), 0.3, (0, s, 0), m='Team', folds=7, amp=0.018, seed=2.0 + s,
                  tie_m='Cloth', nu=34, nv=6)
    vp.tarp_drape(p, (x0 - 0.035, yb if s > 0 else ya, zt), (x0 - 0.035, ya if s > 0 else yb, zt), 0.26, (-1, 0, 0), m='Team', folds=3,
                  amp=0.015, seed=4.0 + s, nu=14, nv=5)
    vp.tarp_drape(p, (x0, yi - 0.035 * s, zt), (x1, yi - 0.035 * s, zt), 0.1, (0, -s, 0), m='Team', folds=6, amp=0.012, seed=6.0 + s,
                  hem=False, nu=30, nv=3)
    # as 3 cintas (por cima e descendo pelos 2 lados) com a fivela
    for xx in (a['x0'] + 0.35, (a['x0'] + a['x1']) / 2 - 0.08, a['x1'] - 0.42):
        p.box('Cloth', xx - 0.03, xx + 0.03, ya - 0.012, yb + 0.012, zt + 0.012, zt + 0.032, bevel=0.0)
        for yy in (ya - 0.012, yb + 0.012):
            p.box('Cloth', xx - 0.03, xx + 0.03, yy - 0.006, yy + 0.006, a['z0'] - 0.02, zt + 0.032, bevel=0.0)
        vp.rbox(p, 'Metal', (xx, yo + 0.05 * s, a['z1'] - 0.36), (0.07, 0.016, 0.06), r=0.01, k=1)
    # o número tático (branco, embaixo da aba) e a fileira de parafusos da caixa
    tac_number(p, (TX + 0.62, (abs(yo) + 0.002) * s, a['z0'] + 0.13), s, '247', h=0.17)
    rk.star_outlined(p, (TX - 0.27, abs(yo) * s, a['z0'] + 0.15), (0, s, 0), r=0.085)
    vp.bolt_row(p, 'Metal', (a['x0'] + 0.1, (abs(yo) + 0.0) * s, a['z0'] + 0.04), (a['x1'] - 0.2, (abs(yo) + 0.0) * s, a['z0'] + 0.04),
                9, (0, s, 0), r=0.015, h=0.011)


def turret():
    p = rk.new_part(PREFIX + '_Turret')
    # o corpo baixo e largo, a frente em cunha rasa
    pts = []
    for s in (-1, 1):
        pts += [(TX + 1.25, 0.6 * s, T_Z0), (TX + 0.8, 1.14 * s, T_Z0), (TX - 1.2, 1.16 * s, T_Z0), (TX - 1.45, 0.92 * s, T_Z0)]
        pts += [(TX + 1.27, 0.62 * s, T_Z1), (TX + 0.82, 1.16 * s, T_Z1), (TX - 1.2, 1.18 * s, T_Z1), (TX - 1.45, 0.94 * s, T_Z1)]
        pts += [(TX + 1.0, 0.56 * s, T_Z2), (TX + 0.7, 1.06 * s, T_Z2), (TX - 1.15, 1.08 * s, T_Z2), (TX - 1.4, 0.86 * s, T_Z2)]
    solid(p, 'Paint#shell', pts)
    vp.lathe(p, 'Paint', (TX, 0.0, C.DECK), 'z', [(0.0, 0.03), (0.98, 0.03), (0.98, 0.06), (0.94, 0.07), (0.0, 0.07)], seg=56)
    # os tijolos do Relikt nas 2 bochechas da frente
    for s in (-1, 1):
        a, b = Vector((TX + 1.25, 0.62 * s, 0)), Vector((TX + 0.82, 1.16 * s, 0))
        d = (b - a).normalized()
        n = Vector((d.y, -d.x, 0.0))
        if n.x < 0:
            n = -n
        rk.era_grid(p, a + Vector((0, 0, T_Z0 + 0.03)), d, Vector((0, 0, 1)), n, 3, 2, (b - a).length / 3, (T_Z1 - T_Z0 - 0.06) / 2, 0.06)
    # ---------------------------------------------------------------- o módulo dos 2 canhões (no meio, por cima)
    mx0, mx1 = MOD_X
    pts = []
    for s in (-1, 1):
        pts += [(mx0, 0.46 * s, T_Z2 - 0.02), (mx0, 0.44 * s, MOD_Z - 0.02), (mx1 - 0.28, 0.42 * s, MOD_Z), (mx1, 0.4 * s, GUN_Z + 0.16),
                (mx1, 0.42 * s, GUN_Z - 0.16), (mx1 - 0.3, 0.46 * s, T_Z2 - 0.02)]
    solid(p, 'Paint#shell', pts, bevel=0.05)
    for x in (mx0 + 0.5, mx0 + 1.05):
        vp.panel_seam(p, 'Dark', (x, -0.42, MOD_Z + 0.001), (x, 0.42, MOD_Z + 0.001), (0, 0, 1), w=0.024, d=0.01)
    for s in (-1, 1):
        vp.grab_handle(p, 'Metal', (mx0 + 0.15, 0.455 * s, 2.3), (mx0 + 0.55, 0.455 * s, 2.3), (0, s, 0), height=0.06, r=0.014)
        vp.bolt_row(p, 'Metal', (mx0 + 0.1, 0.45 * s, T_Z2 + 0.06), (mx1 - 0.4, 0.45 * s, T_Z2 + 0.06), 6, (0, s, 0), r=0.015, h=0.011)
    # o mantelete dos 2 canos com as luvas e a metralhadora coaxial (embaixo, no meio)
    vp.rbox(p, 'Paint#plate', (mx1 + 0.08, 0.0, GUN_Z), (0.2, 0.66, 0.36), r=0.07, k=3)
    for s in (-1, 1):
        vp.lathe(p, 'Paint', (mx1 + 0.16, GUN_Y * s, GUN_Z), 'x', [(0.0, 0.0), (0.085, 0.0), (0.085, 0.18), (0.07, 0.2), (0.0, 0.2)], seg=20)
    vp.lathe(p, 'Dark', (mx1 + 0.18, 0.0, GUN_Z - 0.12), 'x', [(0.0, 0.0), (0.03, 0.0), (0.03, 0.06), (0.0, 0.06)], seg=10)
    # a mira do atirador (a caixa blindada à esquerda do módulo, com as portinholas) e a do comandante no mastro (atrás, direita)
    gx, gy = TX + 0.6, -0.8
    vp.rbox(p, 'Paint#plate', (gx, gy, T_Z2 + 0.2), (0.48, 0.42, 0.4), r=0.06, k=3)
    vp.rbox(p, 'Paint#thin', (gx - 0.02, gy, T_Z2 + 0.41), (0.52, 0.46, 0.03), r=0.06, k=2)
    vp.rbox(p, 'Dark', (gx + 0.24, gy, T_Z2 + 0.24), (0.02, 0.34, 0.22), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.25, gy - 0.06, T_Z2 + 0.24), (0.012, 0.15, 0.13), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.25, gy + 0.1, T_Z2 + 0.24), (0.012, 0.09, 0.09), r=0.03, k=2)
    for sg in (-1, 1):
        p.obox('Paint#thin', (gx + 0.27, gy + sg * 0.22, T_Z2 + 0.24), (0.14, 0.025, 0.24), yaw=sg * 35, bevel=0.0)
    cx, cy = TX - 0.85, 0.66
    vp.lathe(p, 'Paint', (cx, cy, T_Z2), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.12, 0.08), (0.1, 0.52), (0.13, 0.55),
                                              (0.0, 0.55)], seg=16)
    vp.rbox(p, 'Paint#plate', (cx, cy, T_Z2 + 0.68), (0.42, 0.36, 0.27), r=0.06, k=3)
    vp.rbox(p, 'Lens', (cx + 0.21, cy, T_Z2 + 0.68), (0.012, 0.26, 0.13), r=0.03, k=2)
    vp.rbox(p, 'Lens', (cx - 0.21, cy, T_Z2 + 0.68), (0.012, 0.18, 0.1), r=0.03, k=2)
    # as escotilhas atrás do módulo e os periscópios
    vp.hatch_round(p, (TX - 0.35, 0.74, T_Z2), 0.26, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    vp.hatch_round(p, (TX - 0.75, -0.62, T_Z2), 0.26, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    for (x, y) in ((TX + 0.02, 0.62), (TX - 0.4, -0.3)):
        vp.rbox(p, 'Paint#plate', (x, y, T_Z2 + 0.05), (0.15, 0.22, 0.1), r=0.03, k=2)
        p.obox('Lens', (x + 0.075, y, T_Z2 + 0.06), (0.012, 0.16, 0.04), bevel=0.0)
    # ---------------------------------------------------------------- as 2 caixas do Ataka nos ombros, com as capas da cor do jogador
    for s in (-1, 1):
        ataka_box(p, s)
        rk.smoke_bank(p, (TX - 1.25, 1.12 * s, 1.86), yaw=95 * s, n=4, spread=0.12, pitch=22.0)
    # o mastro do vento e as antenas atrás, o cesto de tubo atrás da torre com a caixa de munição
    vp.lathe(p, 'Paint', (TX - 1.2, -0.55, T_Z2), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.04), (0.03, 0.06), (0.025, 0.5),
                                                      (0.05, 0.52), (0.05, 0.62), (0.0, 0.63)], seg=12)
    for (ax, ay, hh) in ((TX - 1.25, 0.25, 1.6), (TX - 1.3, -0.15, 1.3)):
        vp.lathe(p, 'Dark', (ax, ay, T_Z2), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Dark#vp', (ax, ay, T_Z2 + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    xb = TX - 1.47
    for y in (-0.7, 0.7):
        vp.tube(p, 'Metal', vp.bent_bar([(xb + 0.02, y, 1.86), (xb - 0.3, y, 1.86), (xb - 0.3, y, 2.12)], R=0.06, n=3), 0.022, seg=8)
    vp.tube(p, 'Metal', [(xb - 0.3, -0.72, 2.12), (xb - 0.3, 0.72, 2.12)], 0.022, seg=8)
    p.box('Metal#vp', xb - 0.3, xb + 0.01, -0.72, 0.72, 1.84, 1.87, bevel=0.0)
    vp.ammo_box(p, (xb - 0.15, -0.3, 1.99), size=(0.42, 0.22, 0.24), yaw=90, m='Paint#plate', latch_m='Dark')
    vp.tarp_bundle(p, (xb - 0.15, 0.28, 1.95), (0.5, 0.26, 0.16), yaw=90, m='Cloth', strap_m='Cloth', buckle_m='Metal', straps=2,
                   layers=2, seed=5.0)
    return p.finish()


# ---------------------------------------------------------------------------------- canos
def barrel():
    """Os 2 canos de 30 mm (2A42): a luva do recuo, o cano fino com o anel do meio e o freio de boca de janelas; recuam juntos."""
    p = rk.new_part(PREFIX + '_Barrel')
    x0 = MOD_X[1] + 0.3
    for s in (-1, 1):
        y = GUN_Y * s
        st = [(x0 - 0.04, 0.062), (x0 + 0.32, 0.062), (x0 + 0.36, 0.048), (x0 + 1.25, 0.045), (x0 + 1.29, 0.052), (x0 + 1.37, 0.052),
              (x0 + 1.41, 0.043), (MUZZLE_X - 0.27, 0.04), (MUZZLE_X - 0.25, 0.058), (MUZZLE_X, 0.058)]
        vp.gun_barrel(p, st, GUN_Z, y=y, m='Paint', seg=16, bore=0.02, bore_depth=0.2, bore_m='Dark')
        for k in range(3):
            xx = MUZZLE_X - 0.2 + k * 0.065
            for d in (-1, 1):
                p.box('Dark#vp', xx - 0.016, xx + 0.016, y + d * 0.052, y + d * 0.06, GUN_Z - 0.026, GUN_Z + 0.026, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = C.TRACK_Y * s
        for x in C.WHEELS_X:
            rk.ribbed_wheel(p, (x, y, C.WHEEL_Z), C.WHEEL_R, 0.54, s, 'Paint')
        ix, iz, ir = C.IDLER
        vp.idler(p, (ix, y, iz), ir, 0.48, s, 'Paint', 'Metal', gap=0.09)
        sx, sz, sr = C.SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.5, s, 'Paint', 'Metal', teeth=12, holes_m='Dark')
        for x in C.ROLLERS_X:
            vp.return_roller(p, (x, y, C.ROLLER_Z), C.ROLLER_R, 0.27, 'Metal', 'Rubber')
    return p.finish()


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l', C=C, pitch=0.15), rk.t72_track(PREFIX + '_Track_r', 'r', C=C, pitch=0.15)
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (tx0, tx1), (ty0, ty1), (tz0, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.2f m com os canos (x %.2f..%.2f), largura %.2f m nos ombros, casco %.2f x %.2f m (x %.2f..%.2f), '
          'altura %.2f m' % (ax1 - ax0, ax0, ax1, ay1 - ay0, hx1 - hx0, hy1 - hy0, hx0, hx1, az1))
    print('MODEL torre: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f' % (tx0, tx1, ty0, ty1, tz0, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'bocas [x, y, z]', [[round(MUZZLE_X, 3), -GUN_Y, GUN_Z],
                                                                                           [round(MUZZLE_X, 3), GUN_Y, GUN_Z]],
          'tracks_m', [round(2 * C.TRACK_Y, 3), C.TRACK_W])
    print('MODEL Ataka: as 4 bocas', [[round(ATK['x1'] + 0.05, 3), round(ATK['yc'] * s, 3), round(z, 3)] for s in (-1, 1)
                                      for z in (ATK['z0'] + 0.155, ATK['z1'] - 0.155)])
    print('MODEL AG-17 (casulos): as 2 bocas', [[2.98 + 0.58, round((C.UP_HW - 0.03 + C.FENDER_HW - 0.04) / 2 * s, 3), 1.33] for s in (-1, 1)])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')])


main()
