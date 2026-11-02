"""O Chinook da USA (usa_chinook, o helicóptero de carga, coletor de suprimento), VERSÃO B no detalhe AAA (01/11/2026): a versão A
(Game/Tools/model_usa_chinook.py, aprovada no sistema aéreo de 24/10: "meus parabéns, muita qualidade!") fica como está; esta é a que
ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_chinook_v2.py C:/Users/luisf/explosion/captures/game/models/usa_chinook_v2

A referência é a ficha aprovada docs/concept/units/usa_chinook_sheet.webp (o CH-47F cinza claro, os painéis azuis compridos nos
lados de cima, nas carenagens e no teto, as 3 pás escuras por rotor, a rampa de trás) e o Chinook do game shot
captures/concept/44_base_gameplay/r1_raid_slabs_gemini.webp. O pedido (01/11): "deixe eles muito mais parecidos [com os] detalhes
da ficha". O layout é o do A (as mesmas estações, os mesmos cubos, a mesma dobradiça), refeito no que a ficha mostra e o estudo pede
(docs/ESTUDO_DETALHE_AAA.md §3):
  - a fuselagem lisa de lados retos, com os SULCOS de painel das cavernas, as juntas corridas e a calha comprida dos lados, as
    janelas REDONDAS com o aro de borracha, a porta da tripulação (o contorno, a janela, a maçaneta, o degrau), os tubos de pitot;
  - o para-brisa de vários vidros com os montantes escuros, as janelas do queixo e as de lado da cabine;
  - as carenagens laterais (os tanques) lisas com a faixa azul, as tampas e o bocal de combustível;
  - a torre da frente com a grelha do radiador de óleo, o túnel do eixo com as tampas, a torre de trás alta com os painéis azuis;
  - os dois motores com a boca funda e o cone, o escape funil escuro, o suporte liso até a torre, a faixa azul;
  - os mastros com o prato oscilante; os rotores de 3 pás que AFINAM e TORCEM, com a ponta amarela e o cubo com os braços, os
    amortecedores e as bielas;
  - o trem (as pernas da frente com 2 rodas, as de trás com 1, o amortecedor, o pneu de avião), o gancho da carga, as luzes;
  - a rampa com o piso antiderrapante, a borda amarela, as abas, as dobradiças.
A USA não tem desgaste pintado (13/10). Escala (como o A): desenhado no tamanho real e exportado × 0,58; os detalhes que têm de ler
na câmera medidos no final e divididos pela escala (F).

Peças (os NOMES do A; origem no chão no meio da fuselagem, entre os rotores; frente em +X):
  SM_USA_Chinook_Body, SM_USA_Chinook_RotorFront (origem no cubo da frente), SM_USA_Chinook_RotorRear (origem no cubo de trás),
  SM_USA_Chinook_Ramp (origem na dobradiça, eixo Y; fechada).
  Slots (os do A): Paint, Team, Dark, Metal, Rubber, Lens, Light, Beacon, Yellow.
Pivôs MANTIDOS (Units.json usa_chinook): spin at_m (3,463; 0; 2,697) e (−3,463; 0; 3,277), raio 5,301, corda 0,47, ponta 0,29;
ramp.hinge_m (−2,668; 0; 0,435), open_deg 39,3.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_lib  # noqa: E402
from model_lib import COLORS, Part, V, clear_scene, export  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_air_parts as ap  # noqa: E402
from model_air_lib import round_ring  # noqa: E402
from model_china_vehicle_kit import bounds, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_chinook_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Chinook'
SCALE = 0.58
F = 1.0 / SCALE

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170), 'Beacon': (255, 170, 40),
               'Yellow': (214, 164, 32)})

# ---------------------------------------------------------------------------------- medidas do A (tamanho real, m)
Z0, H, W = 0.75, 2.65, 2.55
ZC = Z0 + H / 2
PEXP = 3.2
NOSE = [(8.05, 0.5, 0.8, 1.75, 2.2), (7.85, 1.35, 1.7, 1.86, 2.4), (7.45, 2.05, 2.3, 1.98, 2.7), (6.85, 2.45, 2.58, 2.06, 3.0),
        (6.1, W, H, ZC, PEXP)]
X_HINGE = -4.6
RAMP_HINGE = (X_HINGE, 0.0, Z0)
RAMP_L, RAMP_W, RAMP_T = 3.22, 2.3, 0.15
RAMP_PITCH = math.degrees(math.atan2(1.4, 2.9))
RAMP_OPEN_DEG = RAMP_PITCH + math.degrees(math.asin(Z0 / RAMP_L))
ROTOR_FRONT_AT = (5.97, 0.0, 4.65)
ROTOR_REAR_AT = (-5.97, 0.0, 5.65)
ROTOR_R, BLADES, CHORD, TIP_LEN = 9.14, 3, 0.81, 0.5
CARGO_HOOK = (0.3, 0.0, Z0 - 0.25)

SEC = ap.Section(NOSE + [(4.0, W, H, ZC, PEXP), (0.0, W, H, ZC, PEXP), (X_HINGE, W, H, ZC, PEXP)], smooth=5)
SW = 0.015 * F
GW, GD = 0.025 * F, 0.01 * F
PANELS = [(3.1, 1.45), (1.2, -0.6), (-0.85, -2.6)]   # os painéis azuis compridos do teto e dos lados de cima (a ficha), com a fresta


def deg(a):
    return math.radians(a)


def ramp_z(x):
    return Z0 + (X_HINGE - x) * math.tan(math.radians(RAMP_PITCH))


def side_th(x, z, s):
    """O ângulo da pele do lado s (±1) na altura z."""
    if s > 0:
        return ap.th_for(SEC, x, 2, z, -math.pi / 2 + 0.01, math.pi / 2 - 0.01)
    return ap.th_for(SEC, x, 2, z, 3 * math.pi / 2 - 0.01, math.pi / 2 + 0.01)


def top_th(x, y):
    """O ângulo da pele do teto em y."""
    return ap.th_for(SEC, x, 1, y, 0.01, math.pi - 0.01)


# ---------------------------------------------------------------------------------- a fuselagem
def fuselage(p):
    ths = ap.angles(40)
    xs = [r[0] for r in SEC.rows] + [5.0, 3.0, 2.0, 1.0, -1.0, -2.0, -3.0, -4.0]
    xs = sorted(set(round(x, 4) for x in xs), reverse=True)
    ap.skin(p, 'Paint#fus', SEC, xs, ths, grooves=[(x, GW, GD) for x in (6.0, 3.85, 0.75, -2.3)], caps=(True, True))
    # a parte de trás, em cima da rampa: o casco aberto embaixo (Paint) e o fundo escuro do porão (o A, liso e com mais estações)
    aft = []
    for x, w, top in ((X_HINGE, W, ZC + H / 2), (-5.1, W, 3.39), (-5.6, W, 3.38), (-6.1, 2.53, 3.37), (-6.6, 2.5, 3.36), (-7.1, 2.45, 3.33),
                      (-7.5, 2.4, 3.3), (-7.8, 2.2, 3.2)):
        bot = ramp_z(max(x, -7.5)) + RAMP_T + 0.02
        aft.append((x, w, top - bot, (top + bot) / 2))
    a0, a1 = math.radians(-55), math.radians(235)
    ap.sheet(p, 'Paint#aft', [round_ring(x, w, h, zc, pexp=PEXP, seg=32, a0=a0, a1=a1) for x, w, h, zc in aft], closed=True, smooth=True,
             caps=(False, True), hard_cols=(32,))
    ap.sheet(p, 'Dark#hold', [round_ring(x, w * 0.995, h * 0.995, zc, pexp=PEXP, seg=6, a0=a1, a1=a0 + 2 * math.pi) for x, w, h, zc in aft],
             closed=True, smooth=False, caps=(False, True))
    # as juntas corridas: a calha comprida dos lados (a "rain gutter"), a junta da barriga e a do teto
    for s in (1, -1):
        for z in (2.86, 1.05):
            x0, x1 = 6.4, X_HINGE + 0.1
            xs_ = [x0 + (x1 - x0) * k / 20 for k in range(21)]
            ap.ribbon(p, 'Dark', [SEC.P(x, side_th(x, z, s)) for x in xs_], [SEC.N(x, side_th(x, z, s)) for x in xs_], SW)
        # a calha em relevo (o friso que pega o sol) logo acima das janelas
        xs_ = [5.9 + (X_HINGE + 0.2 - 5.9) * k / 20 for k in range(21)]
        pts = [SEC.P(x, side_th(x, 2.62, s)) + SEC.N(x, side_th(x, 2.62, s)) * 0.01 for x in xs_]
        vp.tube(p, 'Paint', pts, 0.028, seg=6)
        # os painéis azuis compridos dos lados de cima (a ficha) e as tampas de acesso
        # (a ficha: em painéis retangulares com a fresta entre eles, não a fita corrida)
        ta, tb = sorted((side_th(0.0, 2.72, s), side_th(0.0, 2.98, s)))
        for xa, xb in PANELS:
            ap.sec_patch(p, 'Team', SEC, xa, xb, ta, tb, nx=5, nt=3)
        th_a, th_b = sorted((side_th(0.0, 1.35, s), side_th(0.0, 1.85, s)))
        ap.sec_rect(p, 'Dark', SEC, -3.4, -4.3, th_a, th_b, SW)
    # os painéis azuis em cima do teto (lê de cima)
    for s in (1, -1):
        ta, tb = sorted((top_th(0.0, s * 0.62), top_th(0.0, s * 1.0)))
        for xa, xb in PANELS:
            ap.sec_patch(p, 'Team', SEC, xa, xb, ta, tb, nx=5, nt=3)
    # as janelas redondas (a ficha: a fileira no meio da altura) e a porta da tripulação da frente-direita
    for s in (-1, 1):
        for x in (4.6, 3.1, 1.5, 0.0, -1.5, -3.0):
            if s > 0 and x == 4.6:
                continue
            th = side_th(x, 2.3, s)
            ap.round_window(p, 'Lens', 'Dark', SEC.P(x, th), SEC.N(x, th), 0.17, F=1.0)
    s = 1
    for (xa, xb, za, zb) in ((5.28, 5.28, 0.95, 2.52), (4.37, 4.37, 0.95, 2.52)):
        zs = [za + (zb - za) * k / 10 for k in range(11)]
        ap.ribbon(p, 'Dark', [SEC.P(xa, side_th(xa, z, s)) for z in zs], [SEC.N(xa, side_th(xa, z, s)) for z in zs], SW * 1.4)
    for z in (0.95, 2.52):
        xs_ = [5.28 + (4.37 - 5.28) * k / 6 for k in range(7)]
        ap.ribbon(p, 'Dark', [SEC.P(x, side_th(x, z, s)) for x in xs_], [SEC.N(x, side_th(x, z, s)) for x in xs_], SW * 1.4)
    th = side_th(4.82, 2.2, s)
    ap.round_window(p, 'Lens', 'Dark', SEC.P(4.82, th), SEC.N(4.82, th), 0.15, F=1.0)
    th = side_th(5.12, 1.65, s)
    q, n = SEC.P(5.12, th), SEC.N(5.12, th)
    vp.rbox(p, 'Metal', tuple(q + n * 0.02), (0.12, 0.04, 0.035), r=0.012, k=1)
    # o degrau da porta (preso no pé da parede pelas 2 escoras)
    vp.rbox(p, 'Paint#plate', (4.82, W / 2 + 0.06, 0.66), (0.5, 0.22, 0.04), r=0.04, k=3)
    for dx in (-0.2, 0.2):
        q = SEC.P(4.82 + dx, side_th(4.82 + dx, 0.95, 1))
        vp.tube(p, 'Metal', [q, Vector((4.82 + dx, W / 2 + 0.12, 0.66))], 0.02 * F, seg=6)
    # os tubos de pitot no nariz e as antenas de lâmina
    for sy in (-1, 1):
        q = SEC.P(7.55, deg(90 - sy * 35))
        vp.tube(p, 'Metal', [q, q + Vector((0.35, 0, 0.05))], 0.018 * F, seg=6)
    for x, th, h in ((2.0, 270, 0.26), (-0.8, 270, 0.22)):
        q, n = SEC.P(x, deg(th)), SEC.N(x, deg(th))
        vp.prism(p, 'Paint', [(0.0, 0.0), (-0.3, 0.0), (-0.26, h), (-0.1, h)], q - n * 0.02 - Vector((0, 0.02, 0)), Vector((1, 0, 0)), n,
                 Vector((0, 1, 0)), 0.04)
    # o gancho da carga (a ferragem torneada, o gancho curvo) e a luz da barriga
    hx, hy, hz = CARGO_HOOK
    vp.lathe(p, 'Metal', (hx, hy, Z0 + 0.02), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, -0.04), (0.08, -0.08), (0.06, -0.14), (0.0, -0.14)], seg=16)
    vp.tube(p, 'Metal', vp.bent_bar([(hx, 0.0, Z0 - 0.12), (hx, 0.0, hz + 0.02), (hx + 0.12, 0.0, hz + 0.02), (hx + 0.12, 0.0, hz + 0.1)],
                                    R=0.05, n=4), 0.025, seg=8)
    vp.lathe(p, 'Beacon', (-1.2, 0.0, Z0 + 0.01), (0, 0, -1), [(0.0, 0.0), (0.09, 0.0), (0.09, 0.05), (0.07, 0.09), (0.0, 0.1)], seg=14)


def cockpit_glass(p):
    """O para-brisa de vários vidros (a faixa de vidro em volta do nariz) com os montantes, as janelas do queixo e as de lado."""
    th0, th1 = deg(20), deg(160)
    ap.sec_patch(p, 'Lens', SEC, 7.86, 6.92, th0, th1, nx=6, nt=16, lift=0.008, t=0.01)
    for th in (deg(20), deg(55), deg(90), deg(125), deg(160)):
        ap.sec_ribbon(p, 'Dark', SEC, 7.9, 6.88, th, 0.025 * F, n=8, lift=0.012)
    for x in (7.86, 7.38, 6.92):
        ap.sec_arc(p, 'Dark', SEC, x, th0, th1, 0.025 * F, n=16, lift=0.012)
    # as janelas do queixo e as de lado da cabine (com o aro)
    for s in (1, -1):
        for (xa, xb, ta, tb) in ((7.8, 7.3, -38, -14), (6.75, 6.2, 12, 48)):
            a, b = (deg(ta), deg(tb)) if s > 0 else (math.pi - deg(tb), math.pi - deg(ta))
            ap.sec_patch(p, 'Lens', SEC, xa, xb, a, b, nx=4, nt=4, lift=0.008, t=0.01)
            ap.sec_rect(p, 'Dark', SEC, xa, xb, a, b, 0.022 * F, lift=0.011)


def sponsons(p):
    """As carenagens laterais (os tanques): lisas, com a faixa azul (a ficha), as juntas, as tampas e o bocal."""
    for s in (-1, 1):
        yc = s * (W / 2 + 0.475 - 0.45)
        S = ap.Section([(3.6, 0.2, 0.3, Z0 + 0.48, 2.6, s * (W / 2 + 0.1 - 0.45)), (3.3, 0.62, 0.72, Z0 + 0.48, 2.6, s * (W / 2 + 0.31 - 0.45)),
                        (2.7, 0.95, 0.95, Z0 + 0.48, 2.6, yc), (-3.6, 0.95, 0.95, Z0 + 0.48, 2.6, yc),
                        (-4.2, 0.75, 0.8, Z0 + 0.48, 2.6, s * (W / 2 + 0.375 - 0.45)), (-4.5, 0.2, 0.3, Z0 + 0.48, 2.6, s * (W / 2 + 0.1 - 0.45))],
                       smooth=4)
        ap.skin(p, 'Paint#spon', S, [r[0] for r in S.rows] + [1.5, 0.0, -1.5, -2.5], ap.angles(28),
                grooves=[(x, GW, GD) for x in (2.2, -0.4, -3.1)], caps=(True, True))
        ths = (deg(-6), deg(14)) if s > 0 else (math.pi - deg(14), math.pi + deg(6))
        ap.sec_patch(p, 'Team', S, 1.6, -2.6, ths[0], ths[1], nx=10, nt=3)
        tt = (deg(40), deg(70)) if s > 0 else (math.pi - deg(70), math.pi - deg(40))
        ap.sec_rect(p, 'Dark', S, 0.9, -0.3, tt[0], tt[1], SW)
        # o bocal de combustível (a tampa torneada) e a luz de posição na frente
        th = deg(55) if s > 0 else math.pi - deg(55)
        q, n = S.P(-1.9, th), S.N(-1.9, th)
        vp.lathe(p, 'Metal', q - n * 0.01, n, [(0.0, 0.0), (0.07, 0.0), (0.07, 0.025), (0.05, 0.035), (0.0, 0.035)], seg=12)
        vp.lathe(p, 'Light', (3.38, s * (W / 2 + 0.15), Z0 + 0.74), (1, 0, 0), [(0.0, -0.06), (0.04, -0.05), (0.045, 0.0), (0.03, 0.05),
                                                                             (0.0, 0.06)], seg=10)


def pylons_and_engines(p):
    """A torre da frente com a grelha, o túnel do eixo, a torre de trás alta com os painéis azuis, os dois motores, os mastros."""
    rows = []
    for x, w, top in ((7.3, 0.4, 3.45), (6.8, 1.2, 3.95), (6.0, 1.45, 4.15), (5.0, 1.4, 4.05), (4.0, 1.0, 3.65), (3.4, 0.4, 3.45)):
        rows.append((x, w, top - 3.2, (top + 3.2) / 2, 2.6))
    FP = ap.Section(rows, smooth=4)
    ap.skin(p, 'Paint#pyl', FP, [r[0] for r in FP.rows], ap.angles(28), grooves=[(5.4, GW, GD)], caps=(True, True))
    for s in (-1, 1):
        # a grelha do radiador de óleo dos lados da torre da frente (as aletas escuras na moldura)
        for k in range(6):
            x = 6.3 - k * 0.13
            th = deg(12) if s > 0 else math.pi - deg(12)
            q, n = FP.P(x, th), FP.N(x, th)
            vp.rbox(p, 'Dark', tuple(q + n * 0.004), (0.06, 0.025, 0.3), r=0.01, k=1)
        th0, th1 = (deg(-4), deg(28)) if s > 0 else (math.pi - deg(28), math.pi + deg(4))
        ap.sec_rect(p, 'Dark', FP, 6.42, 5.55, th0, th1, SW)
    # o túnel do eixo em cima do teto (as tampas articuladas e as juntas)
    T = ap.Section([(3.75, 0.2, 0.2, 3.3, 3.0), (3.55, 0.76, 0.66, 3.3, 3.0), (-3.0, 0.76, 0.66, 3.3, 3.0), (-3.25, 0.3, 0.3, 3.3, 3.0)],
                   smooth=3)
    ap.skin(p, 'Paint#spine', T, [r[0] for r in T.rows] + [2.0, 0.0, -2.0], ap.angles(20),
            grooves=[(x, GW, GD) for x in (2.6, 1.3, 0.0, -1.3, -2.4)], caps=(True, True))
    # a torre de trás
    rows = []
    for x, w, top in ((-2.9, 0.6, 3.55), (-3.6, 1.45, 4.45), (-4.6, 1.6, 5.0), (-5.97, 1.55, 5.2), (-7.0, 1.4, 5.1), (-7.6, 1.2, 4.85),
                      (-7.85, 0.7, 4.3)):
        rows.append((x, w, top - 3.0, (top + 3.0) / 2, 2.8))
    RP = ap.Section(rows, smooth=4)
    ap.skin(p, 'Paint#pyl', RP, [r[0] for r in RP.rows], ap.angles(28), grooves=[(-4.0, GW, GD), (-6.9, GW, GD)], caps=(True, True))
    for s in (-1, 1):
        a = ap.th_for(RP, -5.3, 2, 4.25, -math.pi / 2 + 0.05, math.pi / 2 - 0.05) if s > 0 else \
            ap.th_for(RP, -5.3, 2, 4.25, 3 * math.pi / 2 - 0.05, math.pi / 2 + 0.05)
        b = ap.th_for(RP, -5.3, 2, 4.62, -math.pi / 2 + 0.05, math.pi / 2 - 0.05) if s > 0 else \
            ap.th_for(RP, -5.3, 2, 4.62, 3 * math.pi / 2 - 0.05, math.pi / 2 + 0.05)
        ap.sec_patch(p, 'Team', RP, -4.0, -6.6, min(a, b), max(a, b), nx=8, nt=3)
    # o farol de cima (a sirene âmbar) na torre de trás
    vp.lathe(p, 'Dark', (-7.2, 0.0, 5.02), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 0.04), (0.0, 0.04)], seg=14)
    vp.lathe(p, 'Beacon', (-7.2, 0.0, 5.06), 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.08), (0.07, 0.13), (0.0, 0.15)], seg=14)
    # os mastros (o cone da carenagem, o eixo e o prato oscilante parado)
    for (mx, _, mz), base in ((ROTOR_FRONT_AT, 4.05), (ROTOR_REAR_AT, 5.1)):
        vp.lathe(p, 'Paint', (mx, 0.0, base), 'z', [(0.0, 0.0), (0.42, 0.0), (0.36, 0.08), (0.26, 0.14), (0.24, mz - base - 0.42),
                                                  (0.0, mz - base - 0.42)], seg=20)
        vp.lathe(p, 'Metal', (mx, 0.0, base), 'z', [(0.0, 0.0), (0.18, 0.0), (0.18, mz - base + 0.05), (0.0, mz - base + 0.05)], seg=14)
        vp.lathe(p, 'Metal', (mx, 0.0, mz - 0.4), 'z', [(0.2, 0.0), (0.5, 0.0), (0.52, 0.03), (0.5, 0.07), (0.2, 0.07)], seg=24, closed=True)
    # os dois motores nos lados da torre de trás
    for s in (-1, 1):
        yc, zc = s * 1.45, 3.75
        E = ap.Section([(-3.25, 0.88, 0.88, zc, 2.0, yc), (-3.6, 1.04, 1.04, zc, 2.0, yc), (-4.4, 1.04, 1.04, zc, 2.0, yc),
                        (-5.8, 1.0, 1.0, zc, 2.0, yc), (-6.55, 0.78, 0.78, zc, 2.0, yc), (-6.75, 0.66, 0.66, zc, 2.0, yc)], smooth=4)
        ths = ap.angles(24)
        ap.skin(p, 'Paint#eng', E, [r[0] for r in E.rows], ths, grooves=[(-4.7, GW, GD), (-5.9, GW, GD)], caps=(False, False))
        # a boca funda com o cone e o escape escuro de funil
        mouth = E.ring(-3.25, ths)
        inner = [(q[0] - 0.02, yc + (q[1] - yc) * 0.8, zc + (q[2] - zc) * 0.8) for q in mouth]
        ap.duct(p, 'Dark#duct', [(q[0] - 0.004, q[1], q[2]) for q in mouth], inner, (-1, 0, 0), 0.5)
        vp.lathe(p, 'Metal', (-3.2, yc, zc), (-1, 0, 0), [(0.0, -0.06), (0.06, -0.03), (0.14, 0.12), (0.16, 0.3), (0.0, 0.3)], seg=16)
        tail = E.ring(-6.75, ths)
        tin = [(q[0] + 0.02, yc + (q[1] - yc) * 0.82, zc + (q[2] - zc) * 0.82) for q in tail]
        ap.duct(p, 'Dark#exh', [(q[0] + 0.004, q[1], q[2]) for q in tail], tin, (1, 0, 0), 0.45)
        vp.lathe(p, 'Metal', (-6.75, yc, zc), (-1, 0, 0), [(0.27, 0.0), (0.33, 0.0), (0.31, 0.25), (0.25, 0.25)], seg=20, closed=True)
        # a faixa azul do lado de fora e o suporte liso até a torre
        th0, th1 = (deg(-10), deg(10)) if s > 0 else (math.pi - deg(10), math.pi + deg(10))
        ap.sec_patch(p, 'Team', E, -4.1, -5.7, th0, th1, nx=6, nt=3)
        rings = []
        for x, hh in ((-3.9, 0.12), (-4.3, 0.26), (-5.3, 0.26), (-5.6, 0.12)):
            rings.append([(x, s * 0.95 + u * s, zc + v) for u, v in vp.rounded_rect(0.55, hh * 2, 0.08, 3)])
        if s < 0:
            rings = [r[::-1] for r in rings]
        ap.sheet(p, 'Paint#strut', rings, closed=True, smooth=True, caps=(True, True))


def gear(p):
    """O trem: as pernas da frente com 2 rodas e as de trás com 1 (a de trás gira), o amortecedor e o pneu de avião."""
    for s in (-1, 1):
        ap.oleo(p, (3.3, s * 1.32, Z0 + 0.12), (3.3, s * 1.62, 0.42), 0.075, m='Metal', F=1.0)
        vp.lathe(p, 'Metal', (3.3, s * 1.4, 0.42), (0, s, 0), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.48), (0.0, 0.48)], seg=10)
        for dy in (1.48, 1.78):
            ap.air_wheel(p, (3.3, s * dy, 0.42), 0.42, 0.22, side=s if dy > 1.6 else -s, rubber='Rubber', rim='Metal', hub='Metal', seg=24)
        ap.oleo(p, (-3.9, s * 1.42, Z0 + 0.12), (-3.95, s * 1.68, 0.42), 0.085, m='Metal', F=1.0)
        vp.tube(p, 'Metal', [(-3.55, s * 1.3, Z0 + 0.1), (-3.8, s * 1.55, 0.55)], 0.04, seg=8)
        vp.lathe(p, 'Metal', (-3.95, s * 1.55, 0.42), (0, s, 0), [(0.0, 0.0), (0.06, 0.0), (0.06, 0.2), (0.0, 0.2)], seg=10)
        ap.air_wheel(p, (-3.95, s * 1.7, 0.42), 0.42, 0.24, side=s, rubber='Rubber', rim='Metal', hub='Metal', seg=24)


def body():
    p = Part(PREFIX + '_Body')
    p.bm('Paint#plate', 0.03)
    fuselage(p)
    cockpit_glass(p)
    sponsons(p)
    pylons_and_engines(p)
    gear(p)
    return p.finish()


# ---------------------------------------------------------------------------------- rotores (origem no cubo)
def rotor(name, phase, lead):
    """O rotor de 3 pás: o cubo torneado, os 3 braços com o pino de batimento, os amortecedores, as bielas, e as pás que afinam e
    torcem, com a ponta amarela (a corda, o raio e a ponta do A: o disco borrado do jogo)."""
    p = Part(name)
    p.bm('Metal#plate', 0.02)
    vp.lathe(p, 'Metal', (0.0, 0.0, -0.22), 'z', [(0.0, 0.0), (0.34, 0.0), (0.5, 0.06), (0.55, 0.18), (0.5, 0.3), (0.36, 0.36),
                                                (0.24, 0.4), (0.16, 0.56), (0.0, 0.6)], seg=24)
    r0 = 0.95
    for k in range(BLADES):
        a = math.radians(phase + 360.0 * k / BLADES)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        side = Vector((-math.sin(a), math.cos(a), 0.0))
        vp.rbox(p, 'Metal#plate', tuple(d * 0.72), (0.5, 0.26, 0.2), yaw=math.degrees(a), r=0.07, k=3)
        vp.lathe(p, 'Metal', tuple(d * 0.5 - Vector((0, 0, 0.14))), (0, 0, 1), [(0.0, 0.0), (0.07, 0.0), (0.07, 0.28), (0.0, 0.28)], seg=10)
        vp.tube(p, 'Dark', [d * 0.4 + side * 0.24 + Vector((0, 0, 0.06)), d * 0.86 + side * 0.18 + Vector((0, 0, 0.06))], 0.045, seg=8)
        horn = d * 0.62 - side * 0.2
        vp.tube(p, 'Metal', [horn, horn * 0.85 + Vector((0, 0, -0.36))], 0.028, seg=6)
        ap.blade(p, 'Dark', (0.0, 0.0, 0.0), d, r0, ROTOR_R, CHORD, 0.11, twist=(10.0, -2.0), taper=0.85, sweep=0.0, tip_m='Yellow',
                 tip_from=1.0 - TIP_LEN / (ROTOR_R - r0), n=12, droop=0.14, root_chord=0.42, lead=lead)
    return p.finish()


# ---------------------------------------------------------------------------------- a rampa (origem na dobradiça)
def ramp():
    """A rampa fechada: a chapa de quina redonda, o piso de dentro com as travessas antiderrapantes, a borda amarela, as abas dos
    lados, as dobradiças e as placas da ponta."""
    p = Part(PREFIX + '_Ramp')
    p.bm('Paint#plate', 0.03)
    p.bm('Metal#plate', 0.015)
    p.bm('Yellow#plate', 0.02)
    a = math.radians(RAMP_PITCH)
    xd = Vector((-math.cos(a), 0.0, math.sin(a)))
    up = Vector((math.sin(a), 0.0, math.cos(a)))

    def at(u, h):
        return xd * u + up * h

    def plate(m, u, h, size, r):
        q = at(u, h)
        # o rbox no referencial da rampa: o x local = xd (yaw 180 e pitch), y = Y, z = up
        vp.rbox(p, m, tuple(q), size, yaw=180.0, pitch=RAMP_PITCH, r=r, k=3)
    plate('Paint#plate', RAMP_L / 2, RAMP_T / 2, (RAMP_L, RAMP_W, RAMP_T), 0.08)
    plate('Metal#plate', RAMP_L / 2 + 0.03, RAMP_T + 0.006, (RAMP_L - 0.3, RAMP_W - 0.3, 0.014), 0.05)
    for k in range(9):
        plate('Dark', 0.3 + k * (RAMP_L - 0.6) / 8, RAMP_T + 0.016, (0.05, RAMP_W - 0.42, 0.014), 0.012)
    plate('Yellow#plate', RAMP_L - 0.07, RAMP_T / 2, (0.14, RAMP_W + 0.012, RAMP_T + 0.012), 0.03)
    for s in (-1, 1):
        q = at(RAMP_L / 2, RAMP_T + 0.12) + Vector((0.0, s * (RAMP_W / 2 - 0.03), 0.0))
        vp.rbox(p, 'Paint#plate', tuple(q), (RAMP_L - 0.12, 0.06, 0.24), yaw=180.0, pitch=RAMP_PITCH, r=0.03, k=2)
        # as dobradiças na raiz e as placas da ponta (as abas que encostam no chão)
        vp.lathe(p, 'Metal', (0.0, s * 0.75, 0.04), (0, s, 0), [(0.0, -0.2), (0.07, -0.2), (0.07, 0.2), (0.0, 0.2)], seg=12)
    q = at(RAMP_L + 0.12, 0.04) + Vector((0.0, -0.6, 0.0))
    vp.rbox(p, 'Metal#plate', tuple(q), (0.26, 0.7, 0.03), yaw=180.0, pitch=RAMP_PITCH, r=0.04, k=3)
    q = at(RAMP_L + 0.12, 0.04) + Vector((0.0, 0.6, 0.0))
    vp.rbox(p, 'Metal#plate', tuple(q), (0.26, 0.7, 0.03), yaw=180.0, pitch=RAMP_PITCH, r=0.04, k=3)
    return p.finish()


# ---------------------------------------------------------------------------------- exportação
def write_spec():
    import json
    k = SCALE
    parts = [[PREFIX + '_Body', [0, 0, 0]], [PREFIX + '_RotorFront', [round(ROTOR_FRONT_AT[0] * k, 4), 0, round(ROTOR_FRONT_AT[2] * k, 4)]],
             [PREFIX + '_RotorRear', [round(ROTOR_REAR_AT[0] * k, 4), 0, round(ROTOR_REAR_AT[2] * k, 4)]],
             [PREFIX + '_Ramp', [round(RAMP_HINGE[0] * k, 4), 0, round(RAMP_HINGE[2] * k, 4)]]]
    views = [['top', 'ortho', [0, 0, -1], [1, 0, 0], [1200, 1400]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 600]],
             ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 500]],
             ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1600, 500]],
             ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 35.0, 0.6],
             ['back_three_quarter', 'persp', 25.0, 35.0, [1600, 1000], 35.0, 0.6]]
    spec = {'parts': parts, 'views': views, 'usa_at': [13.5, -5.5, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 26.0, 0.75]]}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = body()
    rf = rotor(PREFIX + '_RotorFront', 0.0, 1)
    rr = rotor(PREFIX + '_RotorRear', 60.0, -1)
    rp = ramp()
    for o in (b, rf, rr, rp):
        o.data.transform(Matrix.Scale(SCALE, 4))
    # (como a lâmina do Dozer B) os rotores e a rampa ficam NO LUGAR pro cozimento das máscaras e voltam pra origem (o cubo, a
    # dobradiça) só na hora de exportar
    detail = model_lib.DETAIL.get('defer') is not None
    moved = {rf.name: V(*[c * SCALE for c in ROTOR_FRONT_AT]), rr.name: V(*[c * SCALE for c in ROTOR_REAR_AT]),
             rp.name: V(*[c * SCALE for c in RAMP_HINGE])}
    if detail:
        for o in (rf, rr, rp):
            o.data.transform(Matrix.Translation(moved[o.name]))
        _orig = model_lib.export

        def _export(o, out_dir):
            if o.name in moved and model_lib.DETAIL.get('defer') is None:
                o.data.transform(Matrix.Translation(-moved[o.name]))
            return _orig(o, out_dir)
        model_lib.export = _export
    total = export(b, OUT) + export(rf, OUT) + export(rr, OUT) + export(rp, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, rf, rr, rp)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([b])
    k = SCALE
    print('MODEL corpo: comprimento %.2f m, largura %.2f m, altura %.2f m' % (ax1 - ax0, ay1 - ay0, az1))
    print('MODEL rotores (malha)', [round(c * k, 3) for c in ROTOR_FRONT_AT], [round(c * k, 3) for c in ROTOR_REAR_AT],
          'raio %.3f corda %.3f ponta %.3f' % (ROTOR_R * k, CHORD * k, TIP_LEN * k))
    print('MODEL ramp hinge (malha)', [round(c * k, 3) for c in RAMP_HINGE], 'open_deg %.1f' % RAMP_OPEN_DEG)
    write_spec()


main()
