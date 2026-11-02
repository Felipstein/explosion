"""O tanque principal da USA (usa_mbt), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/look_tank.py, aprovada)
fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU; não usa a placa), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_mbt.py C:/Users/luisf/explosion/captures/game/models/usa_mbt

O pedido (01/11): "dá essa melhorada brutal no nosso tanque atual da USA [...] deixe eles muito mais parecidos e [com os] detalhes
da ficha [...] nada de quinas, nada de quadrado, nada de lego". A ÚNICA referência é a ficha docs/concept/units/usa_mbt_player_color.webp
(5 vistas), medida em pixels com o casco de 7,9 m como régua (o M1A2: casco 7,93 × 3,66 m, teto da torre 2,44 m, 7 rodas de 0,635 m,
lagarta de 0,635 m; a ficha dá 3,8 m com as saias). As vistas da ficha não batem entre si (a de cima põe a torre 0,3 m mais à frente
e o cano 0,4 m mais comprido que a de lado): manda a de cima (a câmera do jogo é de cima) e a de lado dá as alturas.

O que a ficha mostra e entra aqui:
  - o casco: o nariz com a face quase em pé e o glacis raso, os cantos da frente chanfrados em planta; as saias em 7 chapas grossas
    com a fresta e os parafusos, a da frente em cunha com o triângulo azul; a caixa de venezianas do lado de trás; o deck do motor
    com as grelhas e a LONA AZUL dobrada com as 2 cintas, caindo pela traseira; os faróis com a guarda em U; as lanternas redondas;
    as escotilhas e os periscópios do motorista;
  - a torre em ponta de flecha (as bochechas em cunha com o mantelete no entalhe, a face de cima inclinada e o queixo recuado), as
    ASAS de trás mais largas (as caixas laterais), as caixas da defesa ativa 2×2 na frente das asas e nos cantos de trás, as faixas
    azuis em pé nas bochechas e nas asas, a mira do atirador com o tampo azul, a mira independente do comandante (o CITV, a caixa com
    as janelas no pedestal), a cúpula octogonal com os periscópios, o mastro do sensor, as 2 antenas, o cesto de tubo com a carga;
  - o cano: a luva grossa perto do mantelete, o degrau, os anéis azuis (a faixa larga, o par do meio, o par da boca), a boca reforçada.
A cor do jogador: Team (as faixas, os anéis, os retalhos, o tampo da mira) e Cloth (a lona: o jogo pinta as duas).

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar delas; os NOMES da versão A, pro jogo trocar sem mexer):
  SM_LL_USATank_Hull     casco, saias, deck, lona, faróis, o anel da torre;
  SM_LL_USATank_Turret   a torre (gira em Z em volta de (TX, 0)), o mantelete, as miras, a cúpula, o mastro, as antenas, o cesto;
  SM_LL_USATank_Barrel   o cano (recua em −X a partir daqui; a boca é o X máximo);
  SM_LL_USATank_Wheels   a rodagem (o executor separa em wheel_001..: o jogo gira cada uma pelo centro da caixa dela);
  SM_LL_USATank_Track_l / _Track_r  as esteiras de elos (a tangente de cada elo no UV2/UV3 pro jogo correr a esteira).
Slots (os da versão A): Paint, Team, Cloth, Dark, Metal, Lens, Track. A borracha das rodas vai no Dark (a A não tem o slot Rubber).
Pivôs mantidos (Units.json usa_mbt): turret_pivot_m (0,1; 0), muzzle_height_m 2,08, tracks_m (2,85; 0,64).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
from model_china_vehicle_kit import bounds, frame_box, pillow, solid, sweep_check, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_mbt')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_LL_USATank'

# as cores de conferência da versão A (o jogo usa as instâncias dele: TankPaint, TankTeam, TankCloth...)
COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Cloth': (64, 86, 132), 'Lens': (40, 90, 160), 'Track': (55, 52, 48), 'Rubber': (28, 28, 28)})

# ---------------------------------------------------------------------------------- medidas (m; a ficha de cima × 0,0207 m/px)
NOSE, REAR = 3.95, -3.95         # o casco: 7,9 m (M1A2: 7,93)
BELLY, DECK = 0.46, 1.60         # a barriga (M1: 0,48 do chão) e o deck (a ficha de lado: 1,60–1,65)
LOW_HW = 1.06                    # o casco de baixo, entre as lagartas
UP_HW = 1.76                     # o casco de cima (por cima das lagartas)
SPON_Z = 1.10                    # a barriga do casco de cima (por cima da esteira)
SKIRT = (1.77, 1.90)             # a saia (dentro, fora): 3,8 m de largura
SKIRT_Z = (0.60, 1.45)
SEAMS = [2.99, 2.11, 1.27, 0.36, -0.48, -1.28, -2.02]   # as frestas das chapas da saia (a ficha de lado)
GRILLE_X = (-2.02, -2.97)        # a caixa de venezianas do lado de trás
TRACK_Y, TRACK_W = 1.425, 0.64   # tracks_m (2,85; 0,64) mantido
WHEEL_R = 0.315                  # a roda de apoio do M1 (0,635 m)
WHEEL_Z = 0.40
WHEELS_X = [2.50, 1.66, 0.82, -0.02, -0.86, -1.70, -2.54]
IDLER = (3.42, 0.63, 0.30)       # a roda-guia na frente, alta (a ficha: o pé da lagarta sobe na frente)
SPROCKET = (-3.52, 0.66, 0.29)   # a motriz atrás (a ficha de lado: a lagarta acaba no fim do casco)
ROLLERS_X, ROLLER_Z, ROLLER_R = [1.66, -0.02, -1.70], 0.89, 0.085
TX = 0.1                         # o eixo da torre (turret_pivot_m, mantido)
T_Z0, T_Z1, T_Z2 = 1.66, 1.98, 2.48   # a torre: o queixo, a ponta das bochechas (a parte mais larga), o teto
GUN_Z = 2.08                     # muzzle_height_m, mantido
MANTLET_X = 2.24                 # a face do mantelete (o cano sai daqui)
MUZZLE_X = 5.80                  # a boca: 1,85 m à frente do nariz (M1A2: 1,84; a ficha: 1,6 de lado, 2,1 de cima)
RING_R = 1.12

# a planta da torre em 3 alturas (meia planta, do meio da frente ao meio de trás, y ≥ 0, relativa ao casco; x, y, z): o entalhe do
# mantelete, a ponta da bochecha, a quina, o lado, o degrau da asa, a asa, a quina de trás. A volta do meio desce nos lados (a
# ficha de lado: a parede em pé de ~1,8 ao teto e o queixo escuro só embaixo; a ponta da bochecha fica na altura do cano)
PLAN = {
    T_Z0: [(1.80, 0.0, T_Z0), (1.80, 0.36, T_Z0), (1.88, 0.42, T_Z0), (1.46, 1.28, T_Z0), (0.10, 1.38, T_Z0), (-0.08, 1.56, T_Z0),
           (-1.36, 1.42, T_Z0), (-1.53, 1.20, T_Z0), (-1.56, 0.0, T_Z0)],
    T_Z1: [(2.10, 0.0, 1.98), (2.10, 0.38, 1.98), (2.32, 0.44, 1.98), (1.64, 1.42, 1.9), (0.10, 1.48, 1.8), (-0.08, 1.68, 1.8),
           (-1.42, 1.50, 1.8), (-1.62, 1.27, 1.8), (-1.65, 0.0, 1.8)],
    T_Z2: [(1.95, 0.0, T_Z2), (1.95, 0.34, T_Z2), (2.08, 0.38, T_Z2), (1.56, 1.18, T_Z2), (0.10, 1.28, T_Z2), (-0.08, 1.52, T_Z2),
           (-1.36, 1.38, T_Z2), (-1.53, 1.17, T_Z2), (-1.56, 0.0, T_Z2)],
}


# ---------------------------------------------------------------------------------- utilidades
def ring(z):
    """O anel completo da planta na altura z (frente → lado direito → trás → lado esquerdo)."""
    half = PLAN[z]
    return half + [(x, -y, zz) for (x, y, zz) in reversed(half[1:-1])]


def inset(rg, k, c=(0.25, 0.0)):
    """O anel encolhido pro centro (as voltas de dentro do teto: o teto ganha quadriláteros, a grade e a máscara têm resolução)."""
    return [(c[0] + (x - c[0]) * k, c[1] + (y - c[1]) * k, z) for (x, y, z) in rg]


def lerp(a, b, t):
    return Vector(a) + (Vector(b) - Vector(a)) * t


def patch(p, m, A1, B1, A2, B2, u0, u1, v0, v1, lift=0.004, t=0.008, nu=4, nv=4, out=None):
    """A chapinha fina colada no quadrilátero (A1 B1 em baixo, A2 B2 em cima), do u0 ao u1 e do v0 ao v1: a faixa azul em pé, a fenda
    escura da bochecha. 'out' = um ponto de dentro (a normal sai pro outro lado)."""
    A1, B1, A2, B2 = (Vector(q) for q in (A1, B1, A2, B2))
    n = (B1 - A1).cross(A2 - A1).normalized()
    if out is not None and n.dot((A1 + B2) / 2 - Vector(out)) < 0:
        n = -n
    b = p.bm(m if '#' in m else m + '#vp', 0.0)

    def P(u, v):
        return lerp(lerp(A1, B1, u), lerp(A2, B2, u), v)
    lo, hi = [], []
    for j in range(nv + 1):
        v = v0 + (v1 - v0) * j / nv
        lo.append([b.verts.new(V(*(P(u0 + (u1 - u0) * i / nu, v) + n * lift))) for i in range(nu + 1)])
        hi.append([b.verts.new(V(*(P(u0 + (u1 - u0) * i / nu, v) + n * (lift + t)))) for i in range(nu + 1)])
    for j in range(nv):
        for i in range(nu):
            b.faces.new([hi[j][i], hi[j][i + 1], hi[j + 1][i + 1], hi[j + 1][i]])
            b.faces.new([lo[j + 1][i], lo[j + 1][i + 1], lo[j][i + 1], lo[j][i]])
    edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
    for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
        b.faces.new([lo[j0][i0], lo[j1][i1], hi[j1][i1], hi[j0][i0]])


def plate_xz(p, m, x0, x1, z0, z1, y, t, r=0.05, side=1):
    """A chapa em pé no plano XZ (a saia, a placa da traseira) com os cantos redondos, de y pra fora (side) com a espessura t."""
    vp.rbox(p, m, ((x0 + x1) / 2, y + side * t / 2, (z0 + z1) / 2), (x1 - x0, z1 - z0, t), roll=90, r=r, k=3)


def grille_slats(p, c, size, n, frame='Paint#plate', slat='Paint#thin', axis_n=(0, 0, 1), along=(1, 0, 0), tilt=35.0):
    """A grelha numa face de normal axis_n (c = o centro na face): o fundo escuro, a moldura de 4 barras de quina redonda e as
    aletas inclinadas (o vão escuro entre elas lê de cima como o ritmo de linhas); as aletas correm ao longo de 'along'."""
    eu, ev, en = vp.face_frame(axis_n, along)
    C = Vector(c)
    L, W = size
    vp.prism(p, 'Dark#vp', vp.rounded_rect(L + 0.02, W + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.01)
    fw = 0.05
    for sgn in (-1, 1):
        frame_box(p, frame, C + ev * (sgn * (W / 2 + fw / 2)) + en * 0.022, eu, ev, en, L + 2 * fw, fw, 0.045)
        frame_box(p, frame, C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.022, eu, ev, en, fw, W, 0.045)
    step = W / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        q = C + ev * (-W / 2 + step * (k + 0.5)) + en * 0.022
        frame_box(p, slat, q, eu, e2, e3, L, step * 0.8, 0.012)


def headlight(p, c, r=0.075):
    """O farol redondo virado pra frente: o copo de aço com o aro e o vidro abaulado."""
    vp.lathe(p, 'Metal', c, 'x', [(0.0, -0.06), (r * 0.7, -0.06), (r + 0.018, -0.04), (r + 0.022, 0.02), (r + 0.022, 0.045),
                                  (r + 0.012, 0.055), (r, 0.05), (r, 0.035), (0.0, 0.035)], seg=16)
    vp.lathe(p, 'Lens', c, 'x', [(0.0, 0.056), (r * 0.6, 0.054), (r * 0.95, 0.046), (r, 0.04), (0.0, 0.04)], seg=16)


def light_guard(p, c, w=0.34, h=0.32, depth=0.16, s=1):
    """A guarda em U dos faróis da frente (a ficha: o U escuro em pé no nariz, aberto pra cima): 2 pernas e o fundo, tubo dobrado."""
    x, y, z = c
    pts = vp.bent_bar([(x - 0.02, y - w / 2, z), (x + depth, y - w / 2, z), (x + depth, y - w / 2, z + h),
                       (x + depth, y + w / 2, z + h), (x + depth, y + w / 2, z), (x - 0.02, y + w / 2, z)], R=0.09, n=4)
    vp.tube(p, 'Dark', pts, 0.022, seg=8)


# ---------------------------------------------------------------------------------- casco
# (01/11, o Felipe: "a frente dele não tem uma descida ali, olha o game shot") o glacis: a chapa que DESCE do deck, logo à frente da
# torre, até a quina do nariz, ~16° (a ficha de lado e o r1_raid_slabs: ~0,5 m em ~2 m). Antes era raso (0,2 m em 1,08 m) e a frente
# lia reta, com a face do nariz quase em pé
GLACIS_X0, NOSE_TOP = 2.40, 1.16


def glacis_z(x):
    """O glacis de cima: da quina do nariz (3,93; NOSE_TOP) ao deck (GLACIS_X0; DECK)."""
    return NOSE_TOP + (DECK - NOSE_TOP) * (3.93 - x) / (3.93 - GLACIS_X0)


def deck_z(x):
    return DECK if x <= GLACIS_X0 else glacis_z(x)


def hull():
    p = Part(PREFIX + '_Hull')
    p.bm('Paint#shell', 0.07)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    # ---------------------------------------------------------------- o casco: o de baixo (entre as lagartas) e o de cima
    low = [(3.55, BELLY), (NOSE - 0.02, 0.96), (NOSE - 0.02, SPON_Z + 0.02), (REAR + 0.02, SPON_Z + 0.02), (REAR + 0.02, 0.62),
           (-3.80, BELLY)]
    solid(p, 'Paint#shell', [(x, s * LOW_HW, z) for x, z in low for s in (-1, 1)])
    up = []
    for s in (-1, 1):
        # o nariz com os cantos chanfrados em planta (a ficha de cima), a face quase em pé, o glacis raso, o deck, a quina de trás
        up += [(NOSE, s * (UP_HW - 0.2), SPON_Z), (NOSE, s * (UP_HW - 0.2), 0.98), (NOSE - 0.03, s * (UP_HW - 0.22), NOSE_TOP),
               (NOSE - 0.2, s * UP_HW, SPON_Z), (NOSE - 0.2, s * UP_HW, glacis_z(NOSE - 0.2)),
               (GLACIS_X0, s * UP_HW, DECK), (-3.86, s * UP_HW, DECK), (REAR, s * UP_HW, 1.52), (REAR, s * UP_HW, SPON_Z)]
    solid(p, 'Paint#shell', up)
    # a chapa de baixo da frente (a ficha de frente: a placa clara entre as lagartas, embaixo)
    vp.rbox(p, 'Paint#plate', (3.77, 0.0, 0.69), (0.5, 1.9, 0.06), pitch=52.8, r=0.05, k=4)
    # ---------------------------------------------------------------- as saias: 7 chapas grossas, a da frente em cunha
    xs = [NOSE - 0.09] + SEAMS
    for s in (-1, 1):
        y_in = SKIRT[0] * s
        t = SKIRT[1] - SKIRT[0]
        for i in range(len(xs) - 1):
            x0, x1 = xs[i + 1] + 0.016, xs[i] - 0.016
            if i == 0:
                # a da frente: o canto de baixo cortado em diagonal (a ficha de lado) e o de cima redondo
                # (01/11) o topo desce em cunha com o glacis (a ficha de lado: a saia da frente acompanha a descida)
                zt = min(SKIRT_Z[1], glacis_z(x1 - 0.12) - 0.04)
                prof = [(x0, SKIRT_Z[1]), (x1 - 0.12, zt), (x1, zt - 0.08), (x1, min(1.12, zt - 0.12)), (x1 - 0.36, SKIRT_Z[0]),
                        (x0, SKIRT_Z[0])]
                solid(p, 'Paint#plate', [(x, y_in + s * yy, z) for x, z in prof for yy in (0.0, t)])
                # o triângulo azul no canto de baixo de trás (a ficha) e a aba de borracha
                yo = (SKIRT[1] + 0.004) * s
                tri = [(x0 + 0.05, SKIRT_Z[0] + 0.05), (x0 + 0.05, 1.20), (x0 + 0.52, SKIRT_Z[0] + 0.05)]
                b = p.bm('Team#vp', 0.0)
                lo = [b.verts.new(V(x, yo, z)) for x, z in tri]
                hi = [b.verts.new(V(x, yo + 0.008 * s, z)) for x, z in tri]
                b.faces.new(lo)
                b.faces.new(hi[::-1])
                for k in range(3):
                    b.faces.new([lo[k], lo[(k + 1) % 3], hi[(k + 1) % 3], hi[k]])
                vp.mudflap(p, (x1 - 0.5, (SKIRT[0] + t / 2) * s, SKIRT_Z[0] + 0.03), 0.34, 0.16, yaw=90, m='Dark')
            else:
                plate_xz(p, 'Paint#plate', x0, x1, SKIRT_Z[0], SKIRT_Z[1], y_in if s > 0 else -SKIRT[1], t, r=0.06)
            # a fileira de parafusos de cima (o ritmo que lê de longe) e os 2 de baixo das pontas
            yb = SKIRT[1] * s
            nb = max(3, int(round((x1 - x0) / 0.2)))
            if i == 0:
                # (01/11) na chapa da frente a fileira segue o topo que desce (reta, os parafusos ficavam no ar acima da chapa)
                top = lambda x: SKIRT_Z[1] + (zt - SKIRT_Z[1]) * (x - x0) / (x1 - 0.12 - x0)
                xa, xb = x0 + 0.1, x1 - 0.22
                vp.bolt_row(p, 'Metal', (xa, yb, top(xa) - 0.075), (xb, yb, top(xb) - 0.075), max(3, int(round((xb - xa) / 0.2))),
                            (0, s, 0), r=0.022, h=0.016)
            else:
                vp.bolt_row(p, 'Metal', (x0 + 0.1, yb, SKIRT_Z[1] - 0.075), (x1 - 0.1, yb, SKIRT_Z[1] - 0.075), nb, (0, s, 0), r=0.022,
                            h=0.016)
            if i > 0:
                for xb in (x0 + 0.1, x1 - 0.1):
                    vp.nut(p, 'Metal', (xb, yb, SKIRT_Z[0] + 0.08), (0, s, 0), r=0.022, h=0.016)
        # a faixa azul segmentada (3 chapas do meio: a ficha de lado, z 1,07–1,21)
        for i in (2, 3, 4):
            x0, x1 = xs[i + 1] + 0.07, xs[i] - 0.07
            plate_xz(p, 'Team', x0, x1, 1.06, 1.22, SKIRT[1] * s if s > 0 else -SKIRT[1] - 0.006, 0.006, r=0.02)
        # a caixa de venezianas do lado de trás (no lugar de uma chapa) e a chapa de trás
        gx0, gx1 = GRILLE_X[1] + 0.016, GRILLE_X[0] - 0.016
        plate_xz(p, 'Paint#plate', gx0, gx1, SKIRT_Z[0], DECK - 0.02, y_in if s > 0 else -SKIRT[1], t, r=0.06)
        grille_slats(p, ((gx0 + gx1) / 2, (SKIRT[1] + 0.005) * s, 0.97), (gx1 - gx0 - 0.18, 0.62), 9, axis_n=(0, s, 0),
                     along=(1, 0, 0), tilt=-38 * s)
        grille_slats(p, ((gx0 + gx1) / 2, (SKIRT[1] + 0.005) * s, 1.42), (gx1 - gx0 - 0.18, 0.2), 3, axis_n=(0, s, 0),
                     along=(1, 0, 0), tilt=-38 * s)
        plate_xz(p, 'Paint#plate', REAR + 0.02, GRILLE_X[1] - 0.016, 0.78, SKIRT_Z[1], y_in if s > 0 else -SKIRT[1], t, r=0.06)
        # o para-lama: a aba que corre em cima das saias (a linha clara entre a saia e o casco de cima)
        yy0, yy1 = sorted((UP_HW * s, (SKIRT[1] + 0.012) * s))
        # (01/11, o Felipe: "olha ali no canto, tem coisa mal encaixada") na frente a aba desce junto com o topo da saia da frente: reta
        # ela passava 28 cm acima do nariz, uma vara solta no ar depois do glacis
        fx0, fx1 = xs[1] + 0.016, xs[0] - 0.016 - 0.12
        fz1 = min(SKIRT_Z[1], glacis_z(fx1) - 0.04) + 0.015
        for (xa, xb) in ((GRILLE_X[0] + 0.01, fx0 + 0.01), (REAR + 0.05, GRILLE_X[1] - 0.01)):
            vp.rbox(p, 'Paint#thin', ((xa + xb) / 2, (yy0 + yy1) / 2, SKIRT_Z[1] + 0.015), (xb - xa, yy1 - yy0, 0.03), r=0.02, k=2)
        dz = SKIRT_Z[1] + 0.015 - fz1
        vp.rbox(p, 'Paint#thin', ((fx0 + fx1) / 2, (yy0 + yy1) / 2, (SKIRT_Z[1] + 0.015 + fz1) / 2),
                (math.hypot(fx1 - fx0, dz) + 0.01, yy1 - yy0, 0.03), pitch=-math.degrees(math.atan2(dz, fx1 - fx0)), r=0.02, k=2)
    # ---------------------------------------------------------------- o nariz: faróis com a guarda em U, ganchos, as juntas
    for s in (-1, 1):
        light_guard(p, (NOSE - 0.04, 0.9 * s, 1.08), w=0.32, h=0.3, depth=0.14)
        headlight(p, (NOSE + 0.01, 0.9 * s, 1.2), r=0.07)
        vp.rbox(p, 'Paint#plate', (NOSE - 0.02, 0.9 * s, 1.2), (0.08, 0.26, 0.22), r=0.05, k=4)
        # os ganchos de reboque (as orelhas soldadas com o pino e a manilha)
        for xx, sg in ((NOSE - 0.06, 1), (REAR + 0.06, -1)):
            for d in (-1, 1):
                vp.rbox(p, 'Metal', (xx + 0.05 * sg, 0.62 * s + d * 0.055, 0.84), (0.12, 0.035, 0.15), r=0.03, k=2)
            p.cyl('Metal#vp', (xx + 0.07 * sg, 0.62 * s, 0.86), 'y', 0.022, 0.16, seg=10, bevel=0.0)
            p.torus('Dark#vp', (xx + 0.1 * sg, 0.62 * s, 0.8), 'y', 0.055, 0.017, seg=14, rseg=6)
    # as juntas do nariz (a chapa do glacis, a solda da quina) e o respingo
    vp.weld_bead(p, 'Paint#thin', [(NOSE - 0.025, -1.5, NOSE_TOP + 0.005), (NOSE - 0.025, 1.5, NOSE_TOP + 0.005)], r=0.016)
    # (01/11, o Felipe: "o que é isso na frente do tanque?") as 2 juntas em V que cruzavam o glacis liam como riscos tortos (com a
    # sombra em cunha da torre, um triângulo); o glacis do M1 é uma chapa lisa: só a junta reta onde ela encontra o deck
    gx = GLACIS_X0 + 0.07
    vp.panel_seam(p, 'Dark', (gx, -1.52, glacis_z(gx) + 0.004), (gx, 1.52, glacis_z(gx) + 0.004), (0.29, 0, 1), w=0.025, d=0.012)
    # ---------------------------------------------------------------- o deck da frente: o motorista e as tampas dos para-lamas
    for k, yaw in enumerate((-25, 0, 25)):                   # os 3 periscópios do motorista (a ficha: o vão escuro na frente da torre)
        y = (k - 1) * 0.24
        x = 2.62 - 0.03 * abs(k - 1)
        vp.rbox(p, 'Paint#plate', (x, y, deck_z(x) + 0.05), (0.2, 0.22, 0.1), yaw=yaw, r=0.04, k=4)
        a = math.radians(yaw)
        p.obox('Lens', (x + 0.1 * math.cos(a), y + 0.1 * math.sin(a), deck_z(x) + 0.06), (0.02, 0.15, 0.05), yaw=yaw, bevel=0.0)
    hz = glacis_z(3.1)
    ga = math.degrees(math.atan2(DECK - NOSE_TOP, 3.93 - GLACIS_X0))                         # a inclinação do glacis
    vp.prism(p, 'Paint#plate', vp.rounded_rect(0.62, 0.78, 0.12, 3), Vector((3.1, 0.0, hz - 0.005)),
             Vector((math.cos(math.radians(ga)), 0, -math.sin(math.radians(ga)))), Vector((0, 1, 0)),
             Vector((math.sin(math.radians(ga)), 0, math.cos(math.radians(ga)))), 0.045)            # a escotilha do motorista
    vp.hinge(p, 'Metal', (2.82, 0.0, glacis_z(2.82) + 0.035), 'y', 0.5, r=0.026, knuckles=3)
    for s in (-1, 1):
        # as grelhas pequenas dos para-lamas da frente e as tampas de enchimento
        # (baixas: a torre passa por cima girando, o fundo dela fica a 1,66)
        grille_slats(p, (2.38, 1.52 * s, DECK), (0.26, 0.3), 4, along=(0, 1, 0), tilt=30)
        grille_slats(p, (1.2, 1.54 * s, DECK), (0.24, 0.24), 3, along=(0, 1, 0), tilt=30)
        # as argolas de amarração nos cantos
        for x in (3.3, -3.7):
            p.torus('Metal#vp', (x, 1.62 * s, deck_z(x) + 0.035), 'x', 0.05, 0.014, seg=12, rseg=6)
    # as juntas das chapas do deck (a ficha de cima: o para-lama em chapas, a emenda com o casco) e os parafusos da beirada
    for s in (-1, 1):
        vp.panel_seam(p, 'Dark', (GLACIS_X0 - 0.05, 1.3 * s, DECK + 0.003), (-2.35, 1.3 * s, DECK + 0.003), (0, 0, 1), w=0.024, d=0.012)
        for x in (2.0, 0.55, -0.85):
            vp.panel_seam(p, 'Dark', (x, 1.32 * s, DECK + 0.003), (x, 1.74 * s, DECK + 0.003), (0, 0, 1), w=0.024, d=0.012)
        vp.bolt_row(p, 'Metal', (GLACIS_X0 - 0.05, 1.68 * s, DECK), (-2.3, 1.68 * s, DECK), 22, (0, 0, 1), r=0.018, h=0.012)
    # o anel da torre (a pista escura no deck)
    vp.lathe(p, 'Dark', (TX, 0.0, DECK), 'z', [(RING_R - 0.06, 0.0), (RING_R + 0.07, 0.0), (RING_R + 0.07, 0.012),
                                             (RING_R + 0.05, 0.022), (RING_R - 0.06, 0.022)], seg=64, closed=True)
    # ---------------------------------------------------------------- o deck do motor: as tampas, as grelhas, a lona azul
    for s in (-1, 1):
        # as grelhas grandes dos cantos de trás (a ficha de cima: as venezianas escuras nos para-lamas de trás)
        grille_slats(p, (-2.78, 1.42 * s, DECK), (0.6, 0.5), 6, along=(0, 1, 0), tilt=30)
        grille_slats(p, (-3.55, 1.42 * s, DECK), (0.5, 0.5), 5, along=(0, 1, 0), tilt=30)
        # as tampas de enchimento redondas na frente do motor (a ficha de cima, atrás do cesto)
        vp.lathe(p, 'Paint', (-2.2, 0.98 * s, DECK), 'z', [(0.0, 0.0), (0.13, 0.0), (0.13, 0.04), (0.11, 0.06), (0.0, 0.065)], seg=18)
        p.cyl('Metal#vp', (-2.2, 0.98 * s, DECK + 0.07), 'z', 0.04, 0.02, seg=10, bevel=0.0)
    # as chapas de acesso do motor (a fresta em volta) sob a lona
    for (x0, x1) in ((-2.45, -3.2), (-3.25, -3.9)):
        vp.panel_seam(p, 'Dark', (x0, -1.15, DECK + 0.003), (x0, 1.15, DECK + 0.003), (0, 0, 1), w=0.025, d=0.01)
    # a lona azul DOBRADA no deck do motor (as 2 cintas atravessam no sentido do comprimento do carro: a ficha de cima) e a aba
    # que cai pela traseira com as dobras (a ficha de trás); baixa na frente: o cesto da torre passa por cima girando
    vp.tarp_bundle(p, (-3.2, 0.0, DECK + 0.005), (1.36, 2.26, 0.13), m='Cloth', strap_m='Dark', buckle_m='Metal', straps=2, layers=2,
                   seed=4.0, sag_ends=0.2, swap=True)
    vp.tarp_drape(p, (REAR - 0.025, 1.06, DECK + 0.02), (REAR - 0.025, -1.06, DECK + 0.02), DECK + 0.02 - 0.92, (-1.0, 0.0, 0.0),
                  m='Cloth', folds=7, amp=0.02, seed=2.5, tie_m='Dark', ties=(0.06, 0.5, 0.94))
    for yy in (-0.58, 0.58):                                   # as cintas descendo pela traseira até as argolas
        vp.tube(p, 'Dark', [(REAR + 0.02, yy, DECK + 0.05), (REAR - 0.05, yy, DECK - 0.1), (REAR - 0.06, yy, 0.86)], 0.014, seg=6)
    # ---------------------------------------------------------------- a traseira: as lanternas, as aletas do escape, o cabo
    for s in (-1, 1):
        # as lanternas redondas nos cantos de cima (a ficha de trás: o copo com o vidro vermelho escuro)
        vp.lathe(p, 'Paint', (REAR - 0.005, 1.5 * s, 1.36), (-1, 0, 0), [(0.0, 0.0), (0.13, 0.0), (0.13, 0.05), (0.115, 0.065),
                                                                       (0.1, 0.065), (0.1, 0.055), (0.0, 0.055)], seg=20)
        vp.lathe(p, 'Lens', (REAR - 0.005, 1.5 * s, 1.36), (-1, 0, 0), [(0.0, 0.07), (0.07, 0.066), (0.095, 0.058), (0.0, 0.058)], seg=16)
        vp.rbox(p, 'Paint#plate', (REAR - 0.05, 1.5 * s, 1.1), (0.1, 0.24, 0.2), r=0.05, k=4)    # a caixa de tomada
    for k in range(6):                                          # as aletas do escape (embaixo da lona)
        vp.rbox(p, 'Dark', (REAR - 0.02, 0.0, 0.7 + k * 0.045), (0.03, 1.5, 0.025), pitch=-25, r=0.01, k=1)
    vp.tow_cable(p, 'Metal', [(REAR - 0.04, -0.8, 0.72), (REAR - 0.06, -0.62, 0.64), (REAR - 0.06, 0.62, 0.64), (REAR - 0.04, 0.8, 0.72)], r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret():
    p = Part(PREFIX + '_Turret')
    p.bm('Paint#shell', 0.07)
    p.bm('Paint#plate', 0.03)
    p.bm('Paint#thin', 0.008)
    p.bm('Lens', 0.0)
    r0, r1, r2 = ring(T_Z0), ring(T_Z1), ring(T_Z2)
    # o casco da torre: as 3 alturas e as voltas de dentro do teto e do fundo (quadriláteros: a máscara tem onde pintar)
    rings = [inset(r0, 0.12), inset(r0, 0.45), inset(r0, 0.78), r0, r1, r2, inset(r2, 0.8), inset(r2, 0.55), inset(r2, 0.3), inset(r2, 0.08)]
    p.loft('Paint#shell', rings)
    vp.lathe(p, 'Paint', (TX, 0.0, DECK), 'z', [(0.0, 0.025), (RING_R - 0.04, 0.025), (RING_R - 0.04, 0.07), (RING_R - 0.08, 0.075),
                                              (0.0, 0.075)], seg=64)                         # o pé no anel (5 mm de folga)
    n = len(r1)
    cin = (0.3, 0.0, 2.1)

    def seg_(k, ra, rb):
        return ra[k], ra[(k + 1) % n], rb[k], rb[(k + 1) % n]
    for s in (-1, 1):
        # os índices da meia planta: 2 = a ponta da bochecha, 3 = a quina, 4 = o fim do lado, 5 = a asa, 6 = a quina de trás; no lado
        # esquerdo o anel espelhado guarda o ponto i da meia planta no índice n − i
        idx = (lambda i: i) if s > 0 else (lambda i: (n - i) % n)

        def side_quad(i, ra, rb):
            return ra[idx(i)], ra[idx(i + 1)], rb[idx(i)], rb[idx(i + 1)]
        # a fenda escura em cada bochecha (a face de cima, inclinada: a ficha de frente)
        A1, B1, A2, B2 = side_quad(2, r1, r2)
        patch(p, 'Dark', A1, B1, A2, B2, 0.3, 0.72, 0.42, 0.5, lift=0.003, t=0.012, out=cin)
        # a faixa azul em pé no lado da bochecha, logo atrás da quina (a ficha: de lado, de cima e de 3/4)
        A1, B1, A2, B2 = side_quad(3, r1, r2)
        patch(p, 'Team', A1, B1, A2, B2, 0.12, 0.26, 0.08, 0.95, lift=0.004, t=0.008, out=cin)
        A1, B1, A2, B2 = side_quad(3, r0, r1)
        patch(p, 'Team', A1, B1, A2, B2, 0.12, 0.26, 0.25, 0.97, lift=0.004, t=0.008, out=cin)
        # a faixa azul da asa (em pé, no meio da asa) que dobra pro teto (a ficha de cima: o retalho azul na borda da asa)
        A1, B1, A2, B2 = side_quad(5, r1, r2)
        patch(p, 'Team', A1, B1, A2, B2, 0.24, 0.42, 0.05, 0.96, lift=0.004, t=0.008, out=cin)
        A1, B1, A2, B2 = side_quad(5, r0, r1)
        patch(p, 'Team', A1, B1, A2, B2, 0.24, 0.42, 0.3, 0.97, lift=0.004, t=0.008, out=cin)
        a, b = r2[idx(5)], r2[idx(6)]
        ia, ib = inset(r2, 0.8)[idx(5)], inset(r2, 0.8)[idx(6)]
        patch(p, 'Team', a, b, ia, ib, 0.24, 0.42, 0.04, 0.9, lift=0.004, t=0.008, out=(TX, 0.0, 0.0))
        # as caixas da defesa ativa 2×2 na frente da asa (no degrau, contra o lado da bochecha) e nos cantos de trás
        # (a ficha de lado e de frente: as caixas no alto do lado, o topo da fileira de cima no nível do teto)
        for (xa, ya, yaw) in ((0.40, 1.6, 12), (-1.86, 1.36, -30)):
            ex = Vector((math.cos(math.radians(yaw * s)), math.sin(math.radians(yaw * s)), 0))
            ey = Vector((-math.sin(math.radians(yaw * s)), math.cos(math.radians(yaw * s)), 0)) * s
            for i in range(2):
                for j in range(2):
                    c = Vector((xa, ya * s, 2.09 + j * 0.27)) + ex * ((i - 0.5) * 0.31)
                    vp.rbox(p, 'Paint#plate', c, (0.28, 0.26, 0.24), yaw=yaw * s, r=0.05, k=4)
                    vp.rbox(p, 'Dark', c + ey * 0.131, (0.17, 0.012, 0.13), yaw=yaw * s, r=0.02, k=2)   # a boca
            # a base das caixas (o suporte) e a chapa que as junta
            vp.rbox(p, 'Paint#plate', (xa, ya * s, 1.94), (0.68, 0.32, 0.06), yaw=yaw * s, r=0.04, k=4)
        # os olhais de içar nos cantos do teto
        for (xx, yy) in ((1.45, 1.0), (-1.35, 1.2)):
            p.torus('Metal#vp', (xx, yy * s, T_Z2 + 0.035), 'x', 0.05, 0.016, seg=12, rseg=6)
    # ---------------------------------------------------------------- o mantelete no entalhe
    vp.rbox(p, 'Paint#plate', (2.13, 0.0, 2.09), (0.22, 0.78, 0.5), r=0.08, k=4)
    vp.lathe(p, 'Paint', (MANTLET_X - 0.01, 0.0, GUN_Z), 'x', [(0.0, 0.0), (0.21, 0.0), (0.21, 0.03), (0.19, 0.05), (0.17, 0.05),
                                                            (0.17, 0.02), (0.0, 0.02)], seg=32)     # o colar do cano
    vp.lathe(p, 'Dark', (2.24, 0.27, 2.2), 'x', [(0.0, 0.0), (0.045, 0.0), (0.045, 0.02), (0.0, 0.02)], seg=12)   # a coaxial
    vp.rbox(p, 'Metal', (2.23, -0.25, 2.22), (0.06, 0.14, 0.1), r=0.02, k=2)                 # a mira auxiliar
    p.box('Lens', 2.26, 2.27, -0.3, -0.2, 2.2, 2.245, bevel=0.0)
    vp.bolt_row(p, 'Metal', (2.245, -0.3, 1.9), (2.245, 0.3, 1.9), 6, (1, 0, 0), r=0.018, h=0.012)
    # ---------------------------------------------------------------- o teto
    top = T_Z2
    # a mira do atirador (a casinha na frente-direita) com o TAMPO AZUL (a ficha de cima) e a janela na frente
    gx, gy = 0.86, 0.78
    vp.rbox(p, 'Paint#plate', (gx, gy, top + 0.15), (0.38, 0.4, 0.3), r=0.06, k=4)
    vp.rbox(p, 'Team', (gx - 0.01, gy, top + 0.305), (0.3, 0.32, 0.016), r=0.04, k=2)
    vp.rbox(p, 'Dark', (gx + 0.19, gy, top + 0.17), (0.02, 0.3, 0.17), r=0.02, k=2)
    vp.rbox(p, 'Lens', (gx + 0.2, gy, top + 0.17), (0.012, 0.25, 0.12), r=0.015, k=2)
    vp.rbox(p, 'Paint#thin', (gx + 0.16, gy, top + 0.27), (0.12, 0.4, 0.02), pitch=-12, r=0.02, k=2)   # a viseira
    # a mira independente do comandante (o CITV: o pedestal redondo e a caixa com a janela grande e a redonda; a ficha de frente)
    cx, cy = -0.12, 0.54
    vp.lathe(p, 'Paint', (cx, cy, top), 'z', [(0.0, 0.0), (0.32, 0.0), (0.32, 0.05), (0.28, 0.08), (0.18, 0.09), (0.17, 0.2),
                                             (0.22, 0.22), (0.0, 0.225)], seg=28)
    vp.rbox(p, 'Paint#plate', (cx - 0.02, cy, top + 0.32), (0.46, 0.44, 0.2), r=0.07, k=4)          # o berço
    vp.rbox(p, 'Paint#plate', (cx, cy, top + 0.66), (0.6, 0.62, 0.48), r=0.08, k=4)                 # a caixa do sensor
    vp.rbox(p, 'Paint#thin', (cx - 0.04, cy, top + 0.915), (0.5, 0.54, 0.03), r=0.06, k=2)
    vp.rbox(p, 'Dark', (cx + 0.3, cy + 0.1, top + 0.68), (0.02, 0.28, 0.27), r=0.03, k=2)
    vp.rbox(p, 'Lens', (cx + 0.31, cy + 0.1, top + 0.68), (0.012, 0.23, 0.22), r=0.03, k=2)       # a janela grande (azul)
    vp.lathe(p, 'Dark', (cx + 0.3, cy - 0.17, top + 0.73), 'x', [(0.0, 0.0), (0.075, 0.0), (0.075, 0.035), (0.0, 0.035)], seg=16)
    vp.lathe(p, 'Lens', (cx + 0.33, cy - 0.17, top + 0.73), 'x', [(0.0, 0.0), (0.05, 0.0), (0.045, 0.01), (0.0, 0.012)], seg=16)
    vp.lathe(p, 'Dark', (cx + 0.3, cy - 0.17, top + 0.57), 'x', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.03), (0.0, 0.03)], seg=12)
    vp.rbox(p, 'Paint#plate', (cx + 0.02, cy + 0.36, top + 0.6), (0.3, 0.12, 0.3), r=0.05, k=4)        # a orelha de lado
    vp.grab_handle(p, 'Metal', (cx - 0.15, cy - 0.31, top + 0.72), (cx + 0.12, cy - 0.31, top + 0.72), (0, -1, 0), height=0.06, r=0.013)
    # a cúpula octogonal do comandante (esquerda) com os periscópios em volta e a escotilha
    kx, ky = -0.12, -0.62
    vp.lathe(p, 'Paint', (kx, ky, top), 'z', [(0.0, 0.0), (0.48, 0.0), (0.48, 0.16), (0.44, 0.19), (0.0, 0.19)], seg=8,
             phase=math.pi / 8, smooth=False)
    for k in range(8):
        a = math.radians(k * 45)
        if k == 4:
            continue
        p.obox('Lens', (kx + 0.455 * math.cos(a), ky + 0.455 * math.sin(a), top + 0.1), (0.03, 0.2, 0.06), yaw=k * 45, bevel=0.0)
    vp.hatch_round(p, (kx, ky, top + 0.16), 0.3, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
    # a escotilha retangular do municiador (na frente do meio do teto) com a dobradiça
    vp.prism(p, 'Paint#plate', vp.rounded_rect(0.42, 0.72, 0.08, 3), Vector((0.66, 0.06, top - 0.005)), Vector((1, 0, 0)),
             Vector((0, 1, 0)), Vector((0, 0, 1)), 0.05)
    vp.hinge(p, 'Metal', (0.42, 0.06, top + 0.045), 'y', 0.56, r=0.024, knuckles=3)
    vp.grab_handle(p, 'Metal', (0.74, -0.08, top + 0.045), (0.74, 0.2, top + 0.045), (0, 0, 1), height=0.05, r=0.012)
    # as chapas de trás do teto (as tampas de ventilação) com os parafusos
    for (y0, y1) in ((0.14, 0.86), (-0.56, 0.08)):
        vp.rbox(p, 'Paint#plate', (-1.0, (y0 + y1) / 2, top + 0.02), (0.6, y1 - y0, 0.05), r=0.05, k=2)
        for xx in (-0.76, -1.24):
            vp.bolt_row(p, 'Metal', (xx, y0 + 0.08, top + 0.045), (xx, y1 - 0.08, top + 0.045), 4, (0, 0, 1), r=0.016, h=0.012)
    vp.rbox(p, 'Paint#plate', (-1.42, 0.18, top + 0.1), (0.2, 0.68, 0.2), r=0.06, k=4)            # a caixa comprida de trás
    # o sensor de vento (o poste curto com o cilindro)
    p.cyl('Metal#vp', (-0.62, 0.12, top + 0.15), 'z', 0.025, 0.3, seg=10, bevel=0.0)
    vp.lathe(p, 'Paint', (-0.62, 0.12, top + 0.3), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.12), (0.04, 0.14), (0.0, 0.14)], seg=14)
    # o mastro do sensor (atrás, à esquerda: a ficha de frente, de lado e de trás) com os anéis e a cabeça
    mx, my = -1.25, -0.7
    vp.lathe(p, 'Paint', (mx, my, top), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.06), (0.11, 0.12), (0.095, 0.24), (0.08, 0.26),
                                             (0.075, 0.82), (0.085, 0.84), (0.085, 0.9), (0.06, 0.92), (0.055, 1.18), (0.07, 1.2),
                                             (0.07, 1.26), (0.0, 1.27)], seg=16)
    vp.rbox(p, 'Paint', (mx, my, top + 1.33), (0.16, 0.14, 0.14), r=0.03, k=2)
    vp.lathe(p, 'Lens', (mx + 0.08, my, top + 1.33), 'x', [(0.0, 0.0), (0.04, 0.0), (0.035, 0.01), (0.0, 0.012)], seg=12)
    for (zz, rr) in ((0.42, 0.077), (0.62, 0.077), (1.02, 0.056)):           # os anéis do mastro
        vp.lathe(p, 'Metal', (mx, my, top + zz), 'z', [(rr - 0.01, -0.02), (rr + 0.012, -0.02), (rr + 0.012, 0.02), (rr - 0.01, 0.02)],
                 seg=16, closed=True)
    p.cyl('Dark#vp', (mx, my, top + 1.4 + 0.45), 'z', 0.01, 0.9, seg=6, bevel=0.0)
    # as 2 antenas nos cantos de trás (a base com a mola e o chicote)
    for (ax, ay, hh) in ((-1.3, 0.92, 1.6), (-1.36, -0.3, 1.35)):
        vp.lathe(p, 'Dark', (ax, ay, top), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)], seg=12)
        p.cyl('Metal#vp', (ax, ay, top + 0.19), 'z', 0.02, 0.05, seg=8, bevel=0.0)
        p.cyl('Dark#vp', (ax, ay, top + 0.2 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    # ---------------------------------------------------------------- o cesto de tubo atrás, com a carga
    bx0, bx1 = -1.5, -2.46
    by = 1.02
    z0, z1 = 1.98, 2.44     # (a ficha de lado e de trás: o cesto alto, do meio da torre ao teto)
    for z in (z0, (z0 + z1) / 2, z1):
        vp.tube(p, 'Metal', vp.bent_bar([(bx0, -by, z), (bx1, -by, z), (bx1, by, z), (bx0, by, z)], R=0.12, n=4), 0.022, seg=8)
    for s in (-1, 1):
        for x in (bx0 + 0.02, (bx0 + bx1) / 2, bx1 + 0.06):
            vp.tube(p, 'Metal', [(x, by * s, z0 - 0.03), (x, by * s, z1 + 0.01)], 0.018, seg=8)
    for k in range(7):
        y = -by + 2 * by * (k + 0.5) / 7
        vp.tube(p, 'Metal', [(bx1, y, z0), (bx1, y, z1)], 0.014, seg=6)
    for y in (-0.8, -0.27, 0.27, 0.8):                        # o fundo de barras
        vp.tube(p, 'Metal', [(bx0, y, z0 - 0.01), (bx1, y, z0 - 0.01)], 0.018, seg=6)
    # a carga: a caixa de munição, a caixa com as nervuras do meio (a ficha de trás), os galões e o rolo de lona
    vp.ammo_box(p, ((bx0 + bx1) / 2, -0.72, z0 + 0.17), (0.6, 0.44, 0.32), yaw=90, m='Paint', latch_m='Metal')
    vp.rbox(p, 'Paint#plate', ((bx0 + bx1) / 2 - 0.05, -0.08, z0 + 0.19), (0.56, 0.58, 0.36), r=0.06, k=4)
    for k in range(5):
        vp.rbox(p, 'Paint#thin', (bx1 + 0.24, -0.08, z0 + 0.06 + k * 0.065), (0.03, 0.5, 0.025), r=0.01, k=1)
    for yy in (0.42, 0.66):
        vp.jerrycan(p, ((bx0 + bx1) / 2 + 0.05, yy, z0 + 0.235 - 0.02), yaw=90, m='Paint', cap_m='Metal')
    # os sacos de lona por cima (a ficha de trás e de cima: os volumes claros saindo do cesto) presos pela corda
    pillow(p, 'Paint', ((bx0 + bx1) / 2, -0.62, z0 + 0.42), (0.5, 0.38, 0.2), yaw=90, seed=1.0)
    pillow(p, 'Paint', ((bx0 + bx1) / 2 + 0.02, 0.56, z0 + 0.5), (0.62, 0.36, 0.22), yaw=84, seed=2.0)
    vp.rope(p, 'Dark', [((bx0 + bx1) / 2 - 0.1, -by, z1), ((bx0 + bx1) / 2 - 0.1, -0.6, z0 + 0.53), ((bx0 + bx1) / 2 - 0.1, 0.5, z0 + 0.62),
                        ((bx0 + bx1) / 2 - 0.1, by, z1)], r=0.012, sag=0.0)
    # ---------------------------------------------------------------- as juntas das chapas do teto (o sulco escuro que a ficha tem
    # em todo lado) e os parafusos da emenda dos módulos das bochechas
    for s in (-1, 1):
        a, b = Vector((1.84, 0.36 * s, top + 0.003)), Vector((1.3, 1.04 * s, top + 0.003))
        vp.panel_seam(p, 'Dark', a, b, (0, 0, 1), w=0.024, d=0.012)
        nb = 6
        for k in range(nb):
            q = a + (b - a) * ((k + 0.5) / nb) + Vector((-0.06, -0.045 * s, 0.0))
            vp.nut(p, 'Metal', q, (0, 0, 1), r=0.02, h=0.014)
        vp.panel_seam(p, 'Dark', (0.12, 1.14 * s, top + 0.003), (-1.3, 1.14 * s, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    vp.panel_seam(p, 'Dark', (0.4, -1.2, top + 0.003), (0.4, 1.2, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    vp.panel_seam(p, 'Dark', (1.3, -1.04, top + 0.003), (1.3, 1.04, top + 0.003), (0, 0, 1), w=0.024, d=0.012)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    p = Part(PREFIX + '_Barrel')
    x0 = MANTLET_X
    L = MUZZLE_X - x0
    # o tubo por estações (as transições cônicas): a luva grossa perto do mantelete (a ficha: o primeiro quarto), o degrau, o tubo
    # fino até a boca reforçada com o furo de 120 mm
    st = [(x0 - 0.06, 0.15), (x0 + 0.04, 0.15), (x0 + 0.07, 0.135), (x0 + 0.86, 0.13), (x0 + 0.9, 0.122), (x0 + 0.96, 0.108),
          (x0 + 1.0, 0.102), (MUZZLE_X - 0.2, 0.094), (MUZZLE_X - 0.17, 0.106), (MUZZLE_X - 0.02, 0.106), (MUZZLE_X, 0.1)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.06, bore_depth=0.4, bore_m='Dark')
    # as braçadeiras da luva (aço, borda redonda) com o fecho embaixo
    for xx, r in ((x0 + 0.3, 0.134), (x0 + 0.6, 0.132), (x0 + 1.3, 0.101), (x0 + 2.75, 0.096)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.01, seg=32)
        vp.rbox(p, 'Metal', (xx, 0.0, GUN_Z - r - 0.014), (0.05, 0.05, 0.03), r=0.012, k=1)
    # os anéis azuis (a ficha: a faixa larga, o par do meio, o par da boca)
    for (a, b) in ((0.43, 0.47), (0.625, 0.638), (0.65, 0.663), (0.875, 0.888), (0.9, 0.913)):
        xa, xb = x0 + L * a, x0 + L * b
        r = 0.102 - (0.008 * (a - 0.3) / 0.6)
        vp.ring_band(p, 'Team', xa, xb, r, GUN_Z, lip=0.007, seg=32)
    # a referência da boca (a caixinha no suporte, em cima)
    vp.rbox(p, 'Metal', (MUZZLE_X - 0.3, 0.0, GUN_Z + 0.15), (0.12, 0.07, 0.05), r=0.015, k=2)
    p.box('Metal', MUZZLE_X - 0.32, MUZZLE_X - 0.28, -0.02, 0.02, GUN_Z + 0.09, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    """As rodas de apoio duplas (a banda de borracha com o ombro redondo, o disco REBAIXADO, o cubo com o anel de porcas, o vão do
    dente-guia), a roda-guia de aço, a motriz com as coroas dentadas e os roletes de cima. A borracha vai no Dark (os slots da A)."""
    p = Part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            vp.road_wheel(p, (x, y, WHEEL_Z), WHEEL_R, 0.56, s, 'Paint', 'Metal', 'Dark', gap=0.1, seg=24)
        ix, iz, ir = IDLER
        vp.idler(p, (ix, y, iz), ir, 0.54, s, 'Paint', 'Metal', gap=0.1)
        sx, sz, sr = SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.54, s, 'Paint', 'Metal', teeth=11, holes_m='Dark')
        for x in ROLLERS_X:
            vp.return_roller(p, (x, y, ROLLER_Z), ROLLER_R, 0.3, 'Metal', 'Dark')
    return p.finish()


def track(side):
    """A esteira de elos com forma (model_vehicle_parts.track_links): as 2 almofadas com a garra, o conector do pino nas bordas e o
    dente-guia no vão das rodas duplas; a barriga entre os roletes de cima."""
    s = 1 if side == 'r' else -1
    m = 0.06
    circles = [(SPROCKET[0], SPROCKET[1], SPROCKET[2] + m + 0.01), (IDLER[0], IDLER[1], IDLER[2] + m)] + \
              [(x, WHEEL_Z, WHEEL_R + m) for x in WHEELS_X] + [(x, ROLLER_Z, ROLLER_R + m) for x in ROLLERS_X]
    zt = ROLLER_Z + ROLLER_R + m
    sags = [(ROLLERS_X[1] + 0.12, ROLLERS_X[0] - 0.12, zt, 0.025), (ROLLERS_X[2] + 0.12, ROLLERS_X[1] - 0.12, zt, 0.025)]
    return vp.track_links(f'{PREFIX}_Track_{side}', circles, TRACK_Y * s, TRACK_W, pitch=0.18, t_in=0.055, t_out=0.014,
                          grouser=0.02, guide='single', m='Track', guide_m='Metal', sags=sags, center_gap=0.11, horn_w=0.075)


# ---------------------------------------------------------------------------------- conferência e exportação
def write_spec():
    import json
    spec = {
        'parts': [[PREFIX + n, [0, 0, 0]] for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')],
        'views': [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 760]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 30.0, 0.78],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.78]],
        'usa_at': [0, 6.5, 0], 'tank_views': False,
        'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
    }
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = track('l'), track('r')
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (tx0, tx1), (ty0, ty1), (tz0, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), casco %.2f x %.2f m (x %.2f..%.2f), altura %.2f m, deck %.2f, teto %.2f'
          % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, hx0, hx1, az1, DECK, T_Z2))
    print('MODEL torre (com o cesto e o mastro): x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f' % (tx0, tx1, ty0, ty1, tz0, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, 'tracks_m', [round(2 * TRACK_Y, 3), TRACK_W])
    write_spec()


main()
