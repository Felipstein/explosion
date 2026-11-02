"""O caça da USA (usa_raptor), VERSÃO B no detalhe AAA (01/11/2026): a versão A (Game/Tools/model_usa_raptor.py, aprovada no sistema
aéreo de 24/10: "meus parabéns, muita qualidade!") fica como está; esta é a que ele compara com ela.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_usa_raptor_v2.py C:/Users/luisf/explosion/captures/game/models/usa_raptor_v2

O pedido (31/10 e 01/11): "TEXTURIZADO, DETALHADO [...] CURVAS, TEXTURAS, NADA DE QUINAS QUADRADAS", "deixe eles muito mais parecidos
[com os] detalhes da ficha". Não há ficha de 5 vistas do Raptor (docs/elenco/usa/usa_raptor.md §6: o diamante cinza-claro de 11 × 7,9 m,
as derivas como dois traços inclinados, os 4 mísseis brancos debaixo da asa; a cor do jogador nas pontas da asa, na faixa das derivas
e no painel da lombada). A base é o F-22A de verdade; o layout é o do A (as mesmas estações, a mesma planta), refeito peça por peça
no que o estudo pede pra aeronave (docs/ESTUDO_DETALHE_AAA.md §3):
  - a fuselagem pela seção furtiva interpolada (lisa no comprimento) com a QUINA ARREDONDADA (a linha que pega o sol, não a aresta
    viva), os sulcos de painel em anel (o radome, as juntas das baias) e as juntas corridas nos ombros, as tampas de acesso, a porta
    do canhão, as antenas de lâmina;
  - as entradas de ar em losango (o caret do F-22) com o lábio enflechado e o TÚNEL ESCURO que entra no corpo;
  - a cabine em bolha com o friso escuro da base;
  - a asa, os estabilizadores e as derivas de perfil, com as frestas das superfícies de comando (flaperon, aileron, o flap do bordo
    de ataque, o leme), a ponta e a faixa na cor do jogador, as luzes de navegação torneadas;
  - os bocais 2D de empuxo vetorado como copo de parede grossa (a boca funda, as pás de cima e de baixo afinando pra trás);
  - os pilones de quina redonda com o trilho, e os AIM-120 de verdade (a ogiva tangente, a faixa, as aletas do meio e as de trás);
  - o trem de pouso com o amortecedor e a haste, o compasso, o pneu de avião com os frisos e a roda de cubo, as portas.
A USA não tem desgaste pintado (13/10): o modelo é limpo; o pó vem do material do jogo.

Escala (como o A): desenhado no tamanho real e exportado × 0,58 (a escala do ar). Os detalhes que têm de ler na câmera são medidos
no tamanho FINAL e divididos pela escala (F).

Peças (os NOMES do A; origem no chão embaixo do centro do avião, com o trem baixado; frente em +X):
  SM_USA_Raptor_Body, SM_USA_Raptor_Gear (o jogo esconde no voo), SM_USA_Raptor_Missiles (missile_001..004: o jogo esconde o que
  saiu). Slots (os do A): Paint, Team, Dark, Metal, Rubber, Lens, Light.
Pivôs MANTIDOS (Units.json usa_raptor): afterburner.nozzles_m (−5,394; ±0,371; 1,16) = a boca dos bocais (NOZZLE × 0,58), os 4
mísseis nos mesmos pilones (a ponta de cada um impressa no fim, igual ao A).
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, export_group  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_air_parts as ap  # noqa: E402
from model_china_vehicle_kit import bounds, tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_raptor_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Raptor'
SCALE = 0.58                     # a escala do ar (a do A)
F = 1.0 / SCALE                  # o detalhe medido no tamanho final → no desenho

COLORS.update({'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
               'Rubber': (32, 32, 31), 'Lens': (92, 128, 168), 'Light': (255, 230, 170), 'Yellow': (214, 164, 32)})

# ---------------------------------------------------------------------------------- medidas do A (tamanho real, m)
ZC = 2.0
FUS = [(9.45, 0.06, 0.03, 0.03, 2.05), (9.0, 0.42, 0.2, 0.18, 2.03), (8.2, 0.9, 0.4, 0.34, 2.01), (7.2, 1.22, 0.56, 0.46, 2.0),
       (6.2, 1.4, 0.68, 0.54, 2.0), (5.0, 1.5, 0.74, 0.6, 2.0), (3.6, 1.62, 0.76, 0.62, 2.0), (2.2, 1.95, 0.74, 0.66, 2.0),
       (0.5, 2.35, 0.7, 0.7, 2.0), (-2.0, 2.55, 0.66, 0.7, 2.0), (-4.5, 2.6, 0.6, 0.62, 2.02), (-6.5, 2.45, 0.52, 0.52, 2.04),
       (-8.0, 2.3, 0.42, 0.42, 2.05)]
WING = [(2.6, 0.9, 2.0, 8.4, 0.46), (1.5, 2.2, 1.98, 7.0, 0.36), (-2.62, 6.78, 1.72, 1.55, 0.09)]
STAB = [(-5.3, 1.0, 2.0, 3.4, 0.22), (-8.2, 4.4, 1.98, 0.9, 0.06)]
CANT = 28.0
FIN_ROOT = (-3.9, 1.45, 2.55)
FIN = (3.7, 0.22, 2.9, -5.93, 1.6, 0.06)
NOZZLE = (-8.0, -9.3, 0.64, 1.05, 0.62)     # x da frente, x da boca, y do centro, largura, altura (a boca = afterburner.nozzles_m)
PYLONS = [(3.3, -0.4), (4.7, -0.9)]
MISSILE_L, MISSILE_R = 3.65, 0.09

# a seção furtiva com a QUINA ARREDONDADA (o A tinha a aresta viva): (y, z) normalizados, do lado +Y subindo por cima
CHINE_R = [(1.0, 0.0), (0.975, 0.11), (0.84, 0.42), (0.58, 0.8), (0.28, 0.97), (0.0, 1.0), (-0.28, 0.97), (-0.58, 0.8),
           (-0.84, 0.42), (-0.975, 0.11), (-1.0, 0.0), (-0.975, -0.11), (-0.82, -0.5), (-0.48, -0.86), (0.0, -1.0), (0.48, -0.86),
           (0.82, -0.5), (0.975, -0.11)]


def _dense_chine(k=3):
    """A seção fechada, interpolada (Catmull-Rom periódico): os pontos e o ângulo polar de cada um (0 → 2π)."""
    n = len(CHINE_R)
    out = []
    for i in range(n):
        a0, a1, a2, a3 = CHINE_R[(i - 1) % n], CHINE_R[i], CHINE_R[(i + 1) % n], CHINE_R[(i + 2) % n]
        for s in range(k):
            t = s / k
            out.append(tuple(0.5 * ((2 * a1[j]) + (-a0[j] + a2[j]) * t + (2 * a0[j] - 5 * a1[j] + 4 * a2[j] - a3[j]) * t * t
                                    + (-a0[j] + 3 * a1[j] - 3 * a2[j] + a3[j]) * t ** 3) for j in (0, 1)))
    ang = [math.atan2(z, y) % (2 * math.pi) for y, z in out]
    return out, ang


class ChineSection(ap.Section):
    """A fuselagem furtiva: estações (x, largura de quina a quina, altura acima da quina, abaixo, z da quina), a seção CHINE_R; θ é o
    ângulo polar na seção normalizada (0 = a quina da direita, 90° = o topo)."""

    def __init__(self, stations, smooth=5):
        self.rows = ap.catmull(stations, smooth)
        self.x0, self.x1 = self.rows[0][0], self.rows[-1][0]
        self.pts, self.ang = _dense_chine()

    def norm(self, th):
        th = th % (2 * math.pi)
        a, pts = self.ang, self.pts
        n = len(a)
        for i in range(n):
            a0, a1 = a[i], a[(i + 1) % n] if i + 1 < n else a[0] + 2 * math.pi
            if a0 <= th <= a1 or (i == n - 1):
                t = (th - a0) / max(a1 - a0, 1e-9)
                p0, p1 = pts[i], pts[(i + 1) % n]
                return p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t
        return pts[0]

    def P(self, x, th):
        _, w, ht, hb, zc = self.at(x)
        y, z = self.norm(th)
        return Vector((x, y * w / 2, zc + z * (ht if z > 0 else hb)))

    def N(self, x, th):
        e, ex = 0.004, 0.01
        n = (self.P(x, th + e) - self.P(x, th - e)).cross(self.P(x + ex, th) - self.P(x - ex, th))
        if n.length < 1e-9:
            n = Vector((0.0, math.cos(th), math.sin(th)))
        n.normalize()
        zc = self.at(x)[4]
        if n.dot(self.P(x, th) - Vector((x, 0.0, zc))) < 0:
            n = -n
        return n


SEC = ChineSection(FUS)


def wing_at(y):
    for a, b in zip(WING[:-1], WING[1:]):
        if a[1] <= y <= b[1]:
            t = (y - a[1]) / (b[1] - a[1])
            return tuple(a[i] + (b[i] - a[i]) * t for i in (0, 2, 3, 4))
    return WING[-1][0], WING[-1][2], WING[-1][3], WING[-1][4]


def foil_half_A(u):
    """A meia espessura do perfil do A (o model_air_lib.FOIL) em u: os pilones e os mísseis ficam onde ficavam."""
    from model_air_lib import FOIL
    pts = [(uu, f) for uu, f in FOIL if f >= 0]
    for (u0, f0), (u1, f1) in zip(pts[:-1], pts[1:]):
        if u0 <= u <= u1:
            return f0 + (f1 - f0) * (u - u0) / max(u1 - u0, 1e-6)
    return 0.0


def round_poly(pts, r, k=3):
    """O polígono (y, z) com os cantos arredondados (raio r, k pontos por canto)."""
    out = []
    n = len(pts)
    for i in range(n):
        a, b, c = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        d0, d1 = (b - a), (c - b)
        rr = min(r, d0.length * 0.45, d1.length * 0.45)
        p0, p1 = b - d0.normalized() * rr, b + d1.normalized() * rr
        for s in range(k + 1):
            t = s / k
            out.append(tuple((p0 * (1 - t) + b * t) * (1 - t) + (b * (1 - t) + p1 * t) * t))
    return out


def shrink(poly, d):
    """O laço (y, z) encolhido d pra dentro (pelo centro: a borda de espessura d)."""
    c = sum((Vector(q) for q in poly), Vector((0.0, 0.0))) / len(poly)
    out = []
    for q in poly:
        v = Vector(q) - c
        L = v.length
        out.append(tuple(c + v * max(0.0, (L - d) / max(L, 1e-9))))
    return out


# ---------------------------------------------------------------------------------- o corpo
def fuselage(p):
    ths = SEC.ang
    xs = [r[0] for r in SEC.rows]
    gw, gd = 0.025 * F, 0.01 * F        # o sulco: 2,5 cm de largura e 1 cm de fundo no final
    grooves = [(x, gw, gd) for x in (7.55, 1.85, -1.35, -4.4, -6.7)]
    ap.skin(p, 'Paint#fus', SEC, xs, ths, grooves=grooves, caps=(True, True))
    # as juntas corridas nos ombros (as linhas que de cima desenham a lombada) e a da quina de baixo
    sw = 0.015 * F                      # a linha de painel: 1,5 cm no final (meio pixel: lê como ritmo, não como desenho)
    for th in (math.radians(57), math.radians(123)):
        ap.sec_ribbon(p, 'Dark', SEC, 7.4, -7.7, th, sw)
    # a lombada: o painel na cor do jogador (lê de cima) e o contorno das tampas de acesso do F-22
    for x0, x1 in ((1.2, -3.2),):
        w = SEC.at((x0 + x1) / 2)[1]
        d = math.atan(0.31 / (w / 2))
        ap.sec_patch(p, 'Team', SEC, x0, x1, math.pi / 2 - d, math.pi / 2 + d, nx=10, nt=6)
    for s in (1, -1):
        def T(a):
            return math.radians(a) if s > 0 else math.pi - math.radians(a)
        ap.sec_rect(p, 'Dark', SEC, 0.7, -0.5, T(26), T(44), sw)
        ap.sec_rect(p, 'Dark', SEC, 6.4, 5.6, T(20), T(32), sw)
    # a porta do canhão M61 no ombro da direita (a fenda escura) e a porta do reabastecimento na lombada
    ap.sec_patch(p, 'Dark', SEC, 2.45, 1.55, math.radians(30), math.radians(37), nx=4, nt=2)
    ap.sec_rect(p, 'Dark', SEC, -0.05, -0.65, math.radians(84), math.radians(96), sw)
    # as antenas de lâmina (em cima atrás da cabine, em cima na cauda, embaixo)
    for x, th, h in ((0.9, 90, 0.26), (-5.6, 90, 0.2), (1.5, 270, 0.22)):
        q = SEC.P(x, math.radians(th))
        n = SEC.N(x, math.radians(th))
        vp.prism(p, 'Paint', [(0.0, 0.0), (-0.34, 0.0), (-0.3, h), (-0.12, h)], q - n * 0.02 - Vector((0, 0.02, 0)),
                 Vector((1, 0, 0)), n, Vector((0, 1, 0)), 0.04)
    # a luz anticolisão na lombada e a de cauda (torneadas)
    q = SEC.P(-7.2, math.pi / 2)
    vp.lathe(p, 'Light', q - Vector((0, 0, 0.01)), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.03), (0.045, 0.06), (0.0, 0.065)], seg=12)


def canopy(p):
    """A bolha da cabine (o vidro) assentada na lombada do nariz, e o friso escuro em volta da base."""
    rings = []
    base_l, base_r = [], []
    for x, w, h in ((7.05, 0.06, 0.03), (6.75, 0.46, 0.25), (6.4, 0.68, 0.44), (5.6, 0.86, 0.58), (4.6, 0.88, 0.62), (3.8, 0.76, 0.52),
                    (3.2, 0.52, 0.32), (2.85, 0.2, 0.1)):
        zc = SEC.P(x, math.pi / 2).z - 0.07
        seg = 18
        ring = []
        for k in range(seg + 1):
            t = math.pi * k / seg
            c, s = math.cos(t), math.sin(t)
            ring.append((x, (w / 2) * math.copysign(abs(c) ** (2 / 2.3), c), zc + h * math.copysign(abs(s) ** (2 / 2.3), s)))
        rings.append(ring)
        base_r.append(Vector(ring[0]))
        base_l.append(Vector(ring[-1]))
    ap.sheet(p, 'Lens#glass', rings, closed=True, smooth=True, caps=(True, True))
    # o friso da base (o trilho do vidro) dos dois lados, e o arco de trás (a junta do vidro com a lombada)
    vp.tube(p, 'Dark', [q + Vector((0, 0.012, 0.012)) for q in base_r[1:-1]], 0.022 * F, seg=6)
    vp.tube(p, 'Dark', [q + Vector((0, -0.012, 0.012)) for q in base_l[1:-1]], 0.022 * F, seg=6)


def intakes(p):
    """As entradas de ar em losango dos lados da cabine: a carenagem lisa de cantos redondos (o lábio enflechado: o canto de cima-fora
    mais à frente) e o túnel escuro que entra no corpo."""
    for s in (-1, 1):
        stations = [(3.5, 0.62, 1.42, 1.38, 2.32), (2.4, 0.66, 1.5, 1.36, 2.3), (0.6, 0.72, 1.45, 1.36, 2.24), (-1.4, 0.7, 1.32, 1.4, 2.16),
                    (-2.6, 0.66, 1.18, 1.5, 2.06)]
        rings = []
        for i, (x, yi, yo, z0, z1) in enumerate(stations):
            poly = round_poly([(yi, z0), (yo, z0 + 0.25), (yo - 0.12, z1), (yi, z1)], 0.09, k=3)
            ring = []
            for (y, z) in poly:
                xx = x
                if i == 0:      # o lábio enflechado: o canto de cima mais à frente, o de baixo-fora recuado
                    xx = x + 0.22 * (z - 1.85) / 0.47 - 0.14 * (y - 1.0) / 0.4
                ring.append((xx, s * y, z))
            rings.append(ring)
        if s < 0:
            rings = [r[::-1] for r in rings]
        ap.sheet(p, 'Paint#duct', rings, closed=True, smooth=True, caps=(False, True))
        # o túnel: o copo escuro com a borda de 7 cm (o lábio) e 1,6 m de fundo
        mouth = rings[0]
        yz = [(q[1], q[2]) for q in mouth]
        inner_yz = shrink(yz, 0.075)
        inner = [(q[0] - 0.03, y, z) for q, (y, z) in zip(mouth, inner_yz)]
        outer = [(q[0] - 0.005, q[1], q[2]) for q in mouth]
        outer = [(x, y * 0.995 + (1 - 0.995) * sum(a[1] for a in mouth) / len(mouth), z) for x, y, z in outer]
        ap.duct(p, 'Dark#duct', outer, inner, (-1, 0, 0), 1.6)
        # a junta do lábio (a borda do bico, mais clara: o material do jogo pega a quina) e a fileira de juntas da carenagem
        for x in (1.4, -0.4):
            yo = [st for st in stations if st[0] <= x + 1.2][0]
            vp.tube(p, 'Dark', [(x, s * (yo[1] + 0.25), 2.27), (x, s * (yo[2] - 0.08), 2.24)], 0.012 * F, seg=5)


def wings(p):
    W = ap.Wing(WING)
    W.build(p, 'Paint#wing', sides=(1, -1))
    tip = ap.Wing([(wing_at(y)[0] + 0.012, y, wing_at(y)[1], wing_at(y)[2] + 0.024, wing_at(y)[3] * 1.16) for y in (5.72, 6.8)])
    tip.build(p, 'Team#tip', sides=(1, -1))
    sw = 0.015 * F
    sides = (1, -1)
    # as frestas das superfícies de comando: o flaperon e o aileron (o bordo de fuga), o flap do bordo de ataque, e as pontas
    for top in (True, False):
        W.chord_line(p, 'Dark', 0.8, 2.3, 4.6, sw, sides=sides, top=top)
        W.chord_line(p, 'Dark', 0.8, 4.75, 6.2, sw, sides=sides, top=top)
        W.chord_line(p, 'Dark', 0.1, 1.6, 6.4, sw, sides=sides, top=top, n=8)
        for y in (2.3, 4.6, 4.75, 6.2):
            W.span_line(p, 'Dark', y, 0.8, 1.0, sw, sides=sides, top=top, n=3)
        W.span_line(p, 'Dark', 6.4, 0.0, 0.1, sw, sides=sides, top=top, n=3)
    # a junta da raiz da asa com o corpo (a carenagem) e as tampas de cima da asa
    W.span_line(p, 'Dark', 2.6, 0.15, 0.75, sw, sides=sides, n=6)
    for (y0, y1, u0, u1) in ((3.0, 3.8, 0.34, 0.56),):
        for y in (y0, y1):
            W.span_line(p, 'Dark', y, u0, u1, sw, sides=sides, n=4)
        for u in (u0, u1):
            W.chord_line(p, 'Dark', u, y0, y1, sw, sides=sides, n=4)
    # as luzes de navegação na ponta (a gota torneada)
    for s in (-1, 1):
        x, z, c, t = wing_at(6.78)
        vp.lathe(p, 'Light', (x - 0.42, s * 6.83, z), (1, 0, 0), [(0.0, -0.12), (0.035, -0.1), (0.045, 0.0), (0.03, 0.08), (0.0, 0.12)], seg=10)
    return W


def tail(p):
    S = ap.Wing(STAB)
    S.build(p, 'Paint#stab', sides=(1, -1))
    sw = 0.015 * F
    for top in (True, False):
        S.span_line(p, 'Dark', 1.4, 0.05, 0.9, sw, sides=(1, -1), top=top, n=6)       # a junta do pivô (o estabilizador gira inteiro)
    # as derivas inclinadas pra fora, com o leme e a faixa da cor do jogador
    c = math.radians(CANT)
    span_dir = Vector((0.0, math.sin(c), math.cos(c)))
    n = (0.0, math.cos(c), -math.sin(c))
    rc, rt, L, tip_le, tc, tt = FIN
    xr, yr, zr = FIN_ROOT

    def st(u, grow=1.0, dx=0.0):
        q = Vector((0.0, yr, zr)) + span_dir * (L * u)
        return (xr + (tip_le - xr) * u + dx, q.y, q.z, (rc + (tc - rc) * u) * grow, (rt + (tt - rt) * u) * (1.16 if grow > 1 else 1.0))
    Fn = ap.Wing([st(-0.08), st(0.35), st(1.0)], n=n)
    Fn.build(p, 'Paint#fin', sides=(1, -1))
    band = ap.Wing([st(0.62, 1.012, 0.012), st(0.8, 1.012, 0.012)], n=n)
    band.build(p, 'Team#band', sides=(1, -1))
    for top in (True, False):
        y0, y1 = st(0.04)[1], st(0.6)[1]
        Fn.chord_line(p, 'Dark', 0.72, y0, y1, sw, sides=(1, -1), top=top, n=6)    # o leme (a dobradiça)
    # a luz no topo da deriva
    for s in (-1, 1):
        q = st(1.0)
        vp.lathe(p, 'Light', (q[0] - q[3] * 0.85, s * q[1], q[2] + 0.02), (1, 0, 0), [(0.0, -0.08), (0.03, -0.06), (0.035, 0.0),
                                                                                    (0.025, 0.06), (0.0, 0.08)], seg=10)


def nozzles(p):
    """Os bocais 2D: o copo de parede grossa de quina redonda (a boca funda, Metal por fora e por dentro), o fundo escuro com as pás da
    turbina, e as pás de cima e de baixo do empuxo vetorado, afinando pra trás."""
    x0, x1, yc, w, h = NOZZLE
    for s in (-1, 1):
        y = s * yc

        def rr(xx, ww, hh, rad, k=3):
            return [(xx, y + u, ZC + v) for u, v in vp.rounded_rect(ww, hh, rad, k)]
        outer = [rr(x0 + 0.4, w * 1.02, h * 1.12, 0.16), rr(x0, w, h * 1.04, 0.14), rr(x1 + 0.35, w * 0.97, h * 0.96, 0.12),
                 rr(x1, w * 0.96, h * 0.92, 0.1)]
        ap.sheet(p, 'Metal#noz', outer, closed=True, smooth=True, caps=(True, False))
        # a boca: a parede de 6 cm e o túnel até o fundo
        o = [Vector(q) for q in outer[-1]]
        i_ = [(q[0], y + (q[1] - y) * 0.86, ZC + (q[2] - ZC) * 0.8) for q in outer[-1]]
        ap.duct(p, 'Metal#nozin', [tuple(q + Vector((0.004, 0, 0))) for q in o], i_, (1, 0, 0), 0.9)
        # o fundo (o anel da turbina com as pás em leque, escuro)
        vp.rbox(p, 'Dark', (x1 + 0.86, y, ZC), (0.04, w * 0.8, h * 0.7), r=0.08, k=3)
        for k in range(7):
            vp.rbox(p, 'Dark', (x1 + 0.82, y + (k - 3) * w * 0.11, ZC), (0.04, 0.03, h * 0.66), r=0.012, k=1)
        # as pás de cima e de baixo (o empuxo vetorado): chapas de quina redonda que passam a boca e afinam
        for sz in (1, -1):
            z = ZC + sz * (h * 0.46 + 0.035)
            vp.prism(p, 'Metal#plate', [(0.0, -w * 0.5), (0.0, w * 0.5), (-0.26, w * 0.47), (-0.34, w * 0.2), (-0.3, 0.0),
                                        (-0.34, -w * 0.2), (-0.26, -w * 0.47)],
                     Vector((x1 + 0.3, y, z)) - Vector((0, 0, 0.035)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.07)
        # o "rabo de castor" (a cauda chata entre os dois bocais, o F-22) e a junta do bocal com o corpo
        if s > 0:
            vp.prism(p, 'Paint#plate', [(0.0, -0.2), (0.0, 0.2), (-0.95, 0.16), (-1.1, 0.0), (-0.95, -0.16)],
                     Vector((x0 + 0.1, 0.0, ZC - 0.07)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), 0.14)
        ap.sheet(p, 'Dark#nozj', [rr(x0 + 0.02, w * 1.005, h * 1.045, 0.14), rr(x0 - 0.02, w * 1.005, h * 1.045, 0.14)],
                 closed=True, smooth=True, caps=(True, True))


def pylons(p, W):
    """Os pilones de quina redonda embaixo da asa, com o trilho de lançamento escuro e as escoras."""
    for s in (-1, 1):
        for y, xc in PYLONS:
            x, z, c, t = wing_at(y)
            zb = z - foil_half_A(0.5) * t / 2
            vp.rbox(p, 'Paint#plate', (xc - 0.05, s * y, zb - 0.1), (1.9, 0.12, 0.26), pitch=0, r=0.05, k=3)
            vp.rbox(p, 'Dark', (xc + 0.1, s * y, zb - 0.235), (1.5, 0.08, 0.04), r=0.025, k=2)
            for dx in (-0.6, 0.6):
                vp.tube(p, 'Metal', [(xc + dx, s * y, zb - 0.22), (xc + dx, s * (y - 0.07), zb - 0.27)], 0.012 * F, seg=5)


def body():
    p = Part(PREFIX + '_Body')
    p.bm('Paint#plate', 0.03)            # as peças de chapa (pilones): o chanfro redondo do harden nas quinas de cima e de baixo
    p.bm('Metal#plate', 0.02)
    fuselage(p)
    canopy(p)
    intakes(p)
    W = wings(p)
    tail(p)
    nozzles(p)
    pylons(p, W)
    return p.finish()


# ---------------------------------------------------------------------------------- o trem
def gear():
    p = Part(PREFIX + '_Gear')
    p.bm('Paint#plate', 0.02)
    # o do nariz: o amortecedor, o compasso, o pneu de avião e as portas
    zb = SEC.P(5.6, 3 * math.pi / 2).z
    ap.oleo(p, (5.62, 0.0, zb + 0.05), (5.55, 0.0, 0.33), 0.065, m='Metal', F=1.0)
    vp.lathe(p, 'Metal', (5.55, -0.13, 0.33), 'y', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.26), (0.0, 0.26)], seg=10)   # o eixo
    for sy in (-1, 1):
        ap.air_wheel(p, (5.55, sy * 0.06, 0.33), 0.33, 0.11, side=sy, rubber='Rubber', rim='Metal', hub='Metal', seg=24)
    for sy in (-1, 1):
        vp.rbox(p, 'Paint#plate', (5.45, sy * 0.2, zb - 0.17), (0.9, 0.34, 0.025), roll=90, r=0.06, k=3)
    # os principais (embaixo das entradas de ar): a perna inclinada, a escora, o pneu e a porta
    for s in (-1, 1):
        y = s * 1.55
        ap.oleo(p, (-0.9, y * 0.85, 1.38), (-1.0, y, 0.42), 0.085, m='Metal', F=1.0)
        vp.tube(p, 'Metal', [(-0.2, y * 0.8, 1.38), (-0.6, y * 0.92, 0.95), (-0.95, y, 0.62)], 0.045, seg=8)
        ap.air_wheel(p, (-1.0, y + s * 0.02, 0.42), 0.42, 0.26, side=s, rubber='Rubber', rim='Metal', hub='Metal', seg=28)
        vp.rbox(p, 'Paint#plate', (-1.1, y * 0.62, 1.12), (1.8, 0.56, 0.035), roll=90 + s * 6, r=0.07, k=3)
    return p.finish()


# ---------------------------------------------------------------------------------- os mísseis
HARDPOINTS = []


def missiles():
    """Os 4 AIM-120 (malhas próprias, missile_001..004), cada um embaixo do pilone dele (as posições do A)."""
    out = []
    k = 0
    for s in (1, -1):
        for y, xc in PYLONS:
            k += 1
            p = Part('missile_%03d' % k)
            x, z, c, t = wing_at(y)
            zm = z - foil_half_A(0.5) * t / 2 - 0.22 - MISSILE_R - 0.02
            L = MISSILE_L
            tip = (xc + L / 2, s * y, zm)
            ap.missile(p, tip, L, MISSILE_R, body='Paint', nose='Dark', fin='Paint', band='Team', nose_len=0.5, band_at=0.62,
                       fins_mid=(1.25, 0.42, 0.12, 0.2, 0.014 * F), fins_tail=(L - 0.42, 0.4, 0.2, 0.2, 0.014 * F), roll=45.0,
                       seg=16, F=F)
            out.append(p.finish())
            HARDPOINTS.append((round((xc + L / 2) * SCALE, 3), round(s * y * SCALE, 3), round(zm * SCALE, 3)))
    return out


# ---------------------------------------------------------------------------------- exportação
def write_spec():
    import json
    parts = [[PREFIX + n, [0, 0, 0]] for n in ('_Body', '_Gear', '_Missiles')]
    views = [['top', 'ortho', [0, 0, -1], [1, 0, 0], [1200, 1400]],
             ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1400, 600]],
             ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 500]],
             ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 35.0, 0.6],
             ['three_quarter_right', 'persp', 26.0, 140.0, [1600, 1000], 35.0, 0.6],
             ['back_three_quarter', 'persp', 25.0, 35.0, [1600, 1000], 35.0, 0.6]]
    spec = {'parts': parts, 'views': views, 'usa_at': [11.0, -6.0, 0], 'tank_views': False,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 26.0, 0.75]]}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = body()
    g = gear()
    ms = missiles()
    for o in [b, g] + ms:
        o.data.transform(Matrix.Scale(SCALE, 4))
    total = export(b, OUT) + export(g, OUT) + export_group(ms, OUT, PREFIX + '_Missiles')
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {b.name: tris(b), g.name: tris(g), 'missiles': sum(tris(m) for m in ms)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([b])
    print('MODEL tamanho: comprimento %.2f m, envergadura %.2f m, altura %.2f m' % (ax1 - ax0, ay1 - ay0, az1))
    x1, yc = NOZZLE[1], NOZZLE[2]
    print('MODEL afterburner nozzles_m', [[round(x1 * SCALE, 3), round(s * yc * SCALE, 3), round(ZC * SCALE, 3)] for s in (-1, 1)])
    print('MODEL hardpoints_m (a ponta de cada missil, missile_001..004):', HARDPOINTS)
    write_spec()


main()
