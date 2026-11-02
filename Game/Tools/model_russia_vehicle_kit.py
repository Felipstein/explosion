"""O kit dos veículos da Rússia provisórios por script (01/11/2026; a facção: docs/FACCAO_RUSSIA.md).

Os 7 veículos (model_russia_t90m.py, _t14, _bmpt, _tos1a, _pantsir, _dozer, _supply_truck) nascem daqui pra serem da mesma família:
  - a ASSINATURA de todo blindado: a gaiola de ripas de aço por cima da torre (o "telhado" anti-drone), com a lona e a rede da cor
    do jogador (slot Team) por cima (slat_cage, tarp_on_slats, net_drape);
  - o Relikt: a blindagem reativa em tijolos com relevo (era_grid), a cunha na frente da torre e o glacis;
  - (01/11, aprovada) a estrela vermelha com as bordas branca, azul e vermelha na lateral da torre (star_outlined: Red, Navy, White);
  - o chassi do T-72/T-90 (t72_hull, t72_wheels, t72_track): o T-90M, o BMPT, o TOS-1A e o IMR-3M são todos sobre ele, como os
    de verdade: o casco baixo, o glacis raso com os tijolos, as prateleiras dos para-lamas com as caixas, a saia com o Relikt e a aba
    de borracha na frente e a grade de barras atrás, o deck do motor com as venezianas, a lâmina de autoentrincheiramento dobrada
    embaixo do nariz;
  - a cabine do KamAZ (kamaz_cab) e o pneu dos caminhões (o Pantsir 8×8 e o caminhão 6×6).
A camuflagem (o aço frio azulado com as manchas grandes pretas e cinza-claras) é do MATERIAL (o RuPaint do import_provisional.py):
a geometria não pinta nada. Nunca Z, V nem a faixa branca.

Slots: Paint (a lataria), Metal (o aço escuro), Team (a lona e a rede da cor do jogador), Track, Lens, Cloth (a lona neutra, as
cintas), Rubber (os pneus, a aba da saia, a borracha das rodas), Red e White (a estrela); e os de sempre que o importador já
conhece com o próprio nome: Dark (frestas, furos, o fundo das grelhas), Light (o farol aceso) e Beacon (a sirene do trator).
Em cima do model_lib.py e do model_vehicle_parts.py; usa as utilidades do kit da China (solid, bounds, sweep_check) sem mexer nele.
"""
import json
import math
import os

from mathutils import Matrix, Vector

from model_lib import COLORS, Part, V  # noqa: F401
import model_vehicle_parts as vp
from model_china_vehicle_kit import bounds, frame_box, pillow, solid, sweep_check, tris  # noqa: F401
from model_faction_kit import star_points

# as cores de conferência (o jogo troca pelas instâncias: RuPaint, RuMetal, RuRed, RuWhite...; as do import_provisional.py em sRGB)
COLORS.update({'Navy': (24, 52, 128), 'Paint': (97, 105, 111), 'Metal': (63, 64, 66), 'Team': (176, 38, 32), 'Track': (44, 42, 38),
               'Lens': (40, 60, 80), 'Cloth': (120, 112, 92), 'Rubber': (28, 28, 28), 'Red': (149, 49, 44),
               'White': (206, 206, 203), 'Dark': (36, 37, 34), 'Light': (255, 230, 170), 'Beacon': (255, 170, 40)})

PAINT_BEVELS = (('Paint#shell', 0.07), ('Paint#plate', 0.03), ('Paint#thin', 0.008), ('Paint', 0.02), ('Metal', 0.012),
                ('Dark', 0.0), ('Lens', 0.0), ('Team', 0.0), ('Red', 0.0), ('White', 0.0), ('Rubber', 0.0), ('Cloth', 0.0),
                # as malhas '#vp' (as peças de detalhe) nunca levam o chanfro do harden, venha quem vier primeiro
                ('Paint#vp', 0.0), ('Metal#vp', 0.0), ('Dark#vp', 0.0), ('Rubber#vp', 0.0))


def new_part(name):
    """A peça com as malhas e os chanfros de sempre já criados (a 1ª criação de uma chave decide o chanfro dela)."""
    p = Part(name)
    for k, b in PAINT_BEVELS:
        p.bm(k, b)
    return p


def bar(p, a, b, r=0.025, m='Metal', seg=8):
    """O tubo reto de a até b (a barra da gaiola, o corrimão)."""
    vp.tube(p, m, [a, b], r, seg=seg)


def flat_bar(p, m, a, b, w, t, up=(0, 0, 1)):
    """A barra chata (a ripa da gaiola): de a até b, largura w no plano perpendicular a 'up', espessura t ao longo de 'up'."""
    a, b, up = Vector(a), Vector(b), Vector(up).normalized()
    m = m if '#' in m else m + '#vp'          # sem o chanfro do harden: a barra fina de 2 cm não ganha nada com ele (e custava 300 triângulos)
    ex = (b - a).normalized()
    ez = (up - ex * up.dot(ex)).normalized()
    ey = ez.cross(ex)
    frame_box(p, m, (a + b) / 2, ex, ey, ez, (b - a).length, w, t)


# ===================================================================================================== estrela, tijolos
def star_outlined(p, c, n, r=0.16, up=(0, 0, 1)):
    """(01/11, APROVADA: `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`) a estrela vermelha com as bordas
    BRANCA, AZUL e VERMELHA (a insígnia das Forças Armadas russas de hoje), colada na face de normal n, a ponta pra 'up'; r = o raio da
    estrela vermelha do meio."""
    n = Vector(n).normalized()
    upv = Vector(up) - n * Vector(up).dot(n)
    if upv.length < 1e-4:
        upv = Vector((1, 0, 0)) - n * n.x
    upv.normalize()
    rt = upv.cross(n)
    c = Vector(c)
    for k, (m, kr) in enumerate((('Red', 1.53), ('Navy', 1.37), ('White', 1.2), ('Red', 1.0))):
        rr = r * kr
        pts = star_points(0.0, 0.0, rr, r_in=rr * (0.42 if k < 3 else 0.382), rot_deg=90.0)
        lift = 0.004 + 0.004 * k
        r0 = [tuple(c + rt * u + upv * v + n * lift) for (u, v) in pts]
        r1 = [tuple(c + rt * u + upv * v + n * (lift + 0.005)) for (u, v) in pts]
        p.loft(m, [r0, r1], bevel=0.0)


def era_grid(p, O, eu, ev, en, nu, nv, cu, cv, h, gap=0.035, m='Paint#plate', bolts=True, skip=()):
    """Os tijolos do Relikt (a blindagem reativa): nu × nv caixas de cu × cv (a fresta 'gap' entre elas: a sombra que lê o "mosaico"
    de cima) e h de altura, assentadas no plano (eu, ev) em O (o canto), saindo pra en. Cada tijolo com a tampa e os 2 parafusos
    (a fileira que vira ritmo). skip = [(i, j)...] tijolos que não vão (o vão do periscópio)."""
    O, eu, ev, en = Vector(O), Vector(eu).normalized(), Vector(ev).normalized(), Vector(en).normalized()
    for i in range(nu):
        for j in range(nv):
            if (i, j) in skip:
                continue
            c = O + eu * (cu * (i + 0.5)) + ev * (cv * (j + 0.5))
            frame_box(p, m, c + en * (h / 2), eu, ev, en, cu - gap, cv - gap, h)
            if bolts:
                for d in (-1, 1):
                    vp.nut(p, 'Metal', c + en * h + eu * (d * (cu - gap) * 0.3), en, r=0.016, h=0.012)


# ===================================================================================================== a gaiola (a assinatura)
def slat_cage(p, x0, x1, hw, z_rear, z_front, posts, visor=0.0, step=0.24, slat_w=0.07, m='Metal', frame_m='Metal', r=0.03, yc=0.0):
    """O telhado de ripas anti-drone por cima da torre (a "cope cage"; FACCAO_RUSSIA.md §3, a assinatura vista de cima): o quadro de
    tubo dobrado (de x0 atrás até x1 na frente, yc ± hw), o telhado caindo de z_rear pra z_front, a VISEIRA da frente ('visor' m, dobrada
    pra baixo mais 18°), as ripas chatas atravessadas a cada 'step' (barra ≥ 6 cm com vão ≥ 15 cm: a regra da TSR, sem moiré), as 2
    longarinas de baixo e os postes com as mãos-francesas até a torre (posts = [(x, y, z_pé)...]). Devolve a altura do telhado
    roof(x) (o eixo das ripas)."""
    xv = x1 - visor

    def roof(x):
        if x <= xv:
            return z_rear + (z_front - z_rear) * (x - x0) / max(1e-6, xv - x0)
        return z_front - (x - xv) * math.tan(math.radians(18.0))
    # o quadro: 2 trilhos laterais dobrados na viseira, o tubo de trás e o da frente (o U contínuo com as quinas redondas)
    for s in (-1, 1):
        y = yc + s * hw
        pts = [(x0, y, roof(x0)), (xv, y, roof(xv)), (x1, y, roof(x1))]
        vp.tube(p, frame_m, vp.bent_bar(pts, R=0.25, n=4), r, seg=8)
    vp.tube(p, frame_m, vp.bent_bar([(x0 + 0.12, yc - hw, roof(x0 + 0.12)), (x0, yc - hw + 0.12, roof(x0)), (x0, yc + hw - 0.12, roof(x0)),
                                     (x0 + 0.12, yc + hw, roof(x0 + 0.12))], R=0.1, n=3), r, seg=8)
    bar(p, (x1, yc - hw, roof(x1)), (x1, yc + hw, roof(x1)), r=r, m=frame_m)
    # as longarinas por baixo das ripas
    for y in (yc - hw * 0.42, yc + hw * 0.42):
        pts = [(x0, y, roof(x0) - 0.05), (xv, y, roof(xv) - 0.05), (x1, y, roof(x1) - 0.05)]
        vp.tube(p, frame_m, vp.bent_bar(pts, R=0.2, n=3), r * 0.8, seg=6)
    # as ripas atravessadas (a barra chata: 7 × 2,5 cm)
    n = max(2, int(round((x1 - x0) / step)))
    for k in range(1, n):
        x = x0 + (x1 - x0) * k / n
        z = roof(x)
        flat_bar(p, m, (x, yc - hw + 0.02, z + 0.005), (x, yc + hw - 0.02, z + 0.005), slat_w, 0.025)
    # os postes e as mãos-francesas (o tubo do pé até o trilho, e a diagonal pro meio)
    for (x, y, zb) in posts:
        s = 1 if y >= yc else -1
        top = Vector((x, yc + s * hw, roof(x) - 0.02))
        base = Vector((x, y, zb))
        vp.tube(p, frame_m, [base, base + Vector((0, 0, 0.05)), top], r * 0.9, seg=8)
        p.cyl(frame_m + '#vp', (x, y, zb + 0.012), 'z', r * 1.9, 0.024, seg=10, bevel=0.0)     # a sapata soldada
        mid = Vector((x, yc + s * hw * 0.42, roof(x) - 0.06))
        vp.tube(p, frame_m, [base + (top - base) * 0.45, mid], r * 0.7, seg=6)
    return roof


def tarp_on_slats(p, x0, x1, y0, y1, roof, step, x_slat0, m='Team', seed=0.0, sag=0.035, nx=None, ny=12, drape=None):
    """A lona esticada nas ripas (a cor do jogador): apoia na crista de cada ripa e CEDE entre elas (a barriga da lona), com rugas leves;
    drape = [(lado, queda)...] as abas caídas ('rear', 'left', 'right') com as dobras (vp.tarp_drape)."""
    nx = nx or max(8, int((x1 - x0) / 0.06))

    def zf(u, v, z):
        x = x0 + (x1 - x0) * u
        y = y0 + (y1 - y0) * v
        ph = ((x - x_slat0) / step) % 1.0
        dip = sag * math.sin(math.pi * ph)
        wr = 0.012 * math.sin(y * 7.3 + seed) * math.sin(x * 5.1 + seed * 2.0)
        return roof(x) + 0.03 - dip + wr
    from model_lib import sheet
    sheet(p, m + '#cloth', [(x0, y0, 0.0), (x1, y0, 0.0), (x1, y1, 0.0), (x0, y1, 0.0)], nx, ny, sag=0.0, z_fn=zf, thick=0.016)
    for side, drop in (drape or ()):
        if side == 'rear':
            a, b, n = (x0 - 0.01, y1, roof(x0) + 0.02), (x0 - 0.01, y0, roof(x0) + 0.02), (-1, 0, 0)
        elif side == 'left':
            a, b, n = (x0, y0 - 0.01, roof(x0) + 0.02), (x1, y0 - 0.01, roof(x1) + 0.02), (0, -1, 0)
        else:
            a, b, n = (x1, y1 + 0.01, roof(x1) + 0.02), (x0, y1 + 0.01, roof(x0) + 0.02), (0, 1, 0)
        vp.tarp_drape(p, a, b, drop, n, m=m, folds=max(3, int((Vector(a) - Vector(b)).length / 0.28)), amp=0.018, seed=seed + 1.7,
                      tie_m='Cloth', nu=28, nv=6)


def net_drape(p, x0, x1, y0, y1, zf, step=0.22, r=0.017, m='Team', jitter=0.06, seed=0.0):
    """A rede de camuflagem jogada por cima (a cor do jogador): os nós numa malha IRREGULAR (cada nó deslocado até 'jitter' por um
    sorteio fixo: a rede de verdade não é grade de cerca), os cordões ligando os nós nos 2 sentidos e cedendo no meio de cada vão
    (≥ 3,4 cm de cordão e ~22 cm de malha: lê como trama, sem moiré), seguindo a superfície zf(x, y) (as bolas e a queda nas bordas)."""
    nxs = max(2, int(round((x1 - x0) / step)))
    nys = max(2, int(round((y1 - y0) / step)))
    h = vp._hash1

    def node(i, j):
        x = x0 + (x1 - x0) * i / nxs
        y = y0 + (y1 - y0) * j / nys
        if 0 < i < nxs:
            x += jitter * (2 * h(seed + i * 7.1 + j * 13.3) - 1)
        if 0 < j < nys:
            y += jitter * (2 * h(seed + i * 3.7 + j * 5.9 + 0.5) - 1)
        return x, y

    def cord(nodes):
        pts = []
        for k in range(len(nodes) - 1):
            (xa, ya), (xb, yb) = nodes[k], nodes[k + 1]
            for t in ((0.0, 0.5) if k < len(nodes) - 2 else (0.0, 0.5, 1.0)):
                x, y = xa + (xb - xa) * t, ya + (yb - ya) * t
                pts.append((x, y, zf(x, y) - (0.018 if t == 0.5 else 0.0)))
        vp.tube(p, m + '#cloth', pts, r, seg=5)
    for i in range(nxs + 1):
        cord([node(i, j) for j in range(nys + 1)])
    for j in range(nys + 1):
        cord([node(i, j) for i in range(nxs + 1)])


# ===================================================================================================== armas e equipamento
def smoke_bank(p, c, yaw, n=6, spread=0.13, pitch=20.0, r=0.042, L=0.3, m='Metal', cap='Dark'):
    """O lançador de fumaça 902 (Tucha): a fileira de tubos num arco (o ritmo de bocas escuras), na base de chapa."""
    ex, ey, ez = vp.ypr_frame(yaw, pitch, 0.0)
    C = Vector(c)
    frame_box(p, 'Paint#plate', C - ex * 0.06 - ez * (r + 0.03), ex, ey, ez, L * 0.8, n * spread + 0.08, 0.05)
    for k in range(n):
        q = C + ey * ((k - (n - 1) / 2) * spread) + ez * (0.025 * math.cos((k - (n - 1) / 2) * 0.45))
        vp.lathe(p, m, q, ex, [(0.0, -L * 0.5), (r * 0.8, -L * 0.5), (r, -L * 0.42), (r, L * 0.5), (r * 0.72, L * 0.5),
                                (r * 0.72, L * 0.38), (0.0, L * 0.38)], seg=12)
        p.cyl(cap + '#vp', tuple(q + ex * (L * 0.39)), tuple(ex), r * 0.7, 0.01, seg=12, bevel=0.0)


def kord_rws(p, c, yaw=0.0, k=1.0):
    """A estação remota com a metralhadora Kord de 12,7 mm (a do T-90M): o pedestal giratório, a caixa blindada da arma com a mira, o
    cano com o quebra-chamas e a caixa de munição de lado."""
    ex, ey, ez = vp.ypr_frame(yaw, 0.0, 0.0)
    C = Vector(c)
    vp.lathe(p, 'Paint', C, ez, [(0.0, 0.0), (0.2 * k, 0.0), (0.2 * k, 0.05 * k), (0.15 * k, 0.07 * k), (0.12 * k, 0.16 * k),
                                 (0.0, 0.16 * k)], seg=18)
    vp.rbox(p, 'Paint#plate', C + ez * (0.27 * k) - ex * (0.05 * k), (0.62 * k, 0.26 * k, 0.2 * k), yaw=yaw, r=0.04 * k, k=2)
    vp.rbox(p, 'Paint#plate', C + ez * (0.3 * k) + ey * (0.21 * k), (0.34 * k, 0.16 * k, 0.24 * k), yaw=yaw, r=0.03 * k, k=2)   # a munição
    vp.rbox(p, 'Paint#plate', C + ez * (0.32 * k) - ey * (0.2 * k) + ex * (0.05 * k), (0.22 * k, 0.12 * k, 0.16 * k), yaw=yaw,
            r=0.03 * k, k=2)                                                                                     # a mira
    vp.rbox(p, 'Lens', C + ez * (0.33 * k) - ey * (0.2 * k) + ex * (0.162 * k), (0.012, 0.08 * k, 0.09 * k), yaw=yaw, r=0.02, k=2)
    tip = C + ez * (0.28 * k) + ex * (0.25 * k)
    vp.lathe(p, 'Dark', tip, ex, [(0.0, 0.0), (0.032 * k, 0.0), (0.026 * k, 0.06 * k), (0.022 * k, 0.9 * k), (0.034 * k, 0.92 * k),
                                  (0.034 * k, 1.04 * k), (0.012 * k, 1.04 * k), (0.012 * k, 1.0 * k), (0.0, 1.0 * k)], seg=10)


def headlight(p, c, r=0.07, guard=True, lit=False):
    """O farol redondo virado pra +X: o copo de aço, o vidro abaulado e a grade de proteção (2 barras dobradas)."""
    x, y, z = c
    vp.lathe(p, 'Metal', c, 'x', [(0.0, -0.06), (r * 0.7, -0.06), (r + 0.018, -0.04), (r + 0.022, 0.02), (r + 0.022, 0.045),
                                  (r + 0.012, 0.055), (r, 0.05), (r, 0.035), (0.0, 0.035)], seg=16)
    vp.lathe(p, 'Lens', c, 'x', [(0.0, 0.056), (r * 0.6, 0.054), (r * 0.95, 0.046), (r, 0.04), (0.0, 0.04)], seg=16)
    if lit:
        p.cyl('Light', (x + 0.058, y, z), 'x', r * 0.5, 0.006, seg=12, bevel=0.0)
    if guard:
        g = r + 0.045
        vp.tube(p, 'Metal', vp.bent_bar([(x - 0.03, y - g, z - g * 0.6), (x + 0.12, y - g, z - g * 0.6), (x + 0.12, y - g, z + g),
                                         (x + 0.12, y + g, z + g), (x + 0.12, y + g, z - g * 0.6), (x - 0.03, y + g, z - g * 0.6)],
                                        R=0.04, n=3), 0.012, seg=6)


def grille_deck(p, x0, x1, y0, y1, z, n, along='y', tilt=30.0):
    """A veneziana do deck do motor: o fundo escuro, a moldura de chapa e as aletas inclinadas (o ritmo de linhas de cima)."""
    p.box('Dark', x0 + 0.03, x1 - 0.03, y0 + 0.03, y1 - 0.03, z - 0.01, z + 0.006, bevel=0.0)
    for (a, b, c, d) in ((x0, x1, y0, y0 + 0.05), (x0, x1, y1 - 0.05, y1), (x0, x0 + 0.05, y0, y1), (x1 - 0.05, x1, y0, y1)):
        p.box('Paint#thin', a, b, c, d, z, z + 0.045)
    if along == 'y':
        st = (x1 - x0 - 0.1) / n
        for k in range(n):
            xx = x0 + 0.05 + st * (k + 0.5)
            p.obox('Metal#vp', (xx, (y0 + y1) / 2, z + 0.025), (st * 0.62, y1 - y0 - 0.1, 0.012), pitch=tilt, bevel=0.0)
    else:
        st = (y1 - y0 - 0.1) / n
        for k in range(n):
            yy = y0 + 0.05 + st * (k + 0.5)
            p.obox('Metal#vp', ((x0 + x1) / 2, yy, z + 0.025), (x1 - x0 - 0.1, st * 0.62, 0.012), roll=tilt, bevel=0.0)


def slat_screen(p, x0, x1, z0, z1, y, s, step=0.17, w=0.06, t=0.02):
    """A grade de barras em pé (a blindagem de ripas das laterais e da traseira do motor do T-90M): o quadro de cantoneira e as barras
    verticais a cada 'step' (o vão mostra a roda atrás: lê como grade, não como chapa), a 'y' do lado s, as barras a 0,12 m pra fora."""
    yo = y + s * 0.12
    for (a, b) in (((x0, z0), (x1, z0)), ((x0, z1), (x1, z1))):
        flat_bar(p, 'Metal', (a[0], yo, a[1]), (b[0], yo, b[1]), 0.06, 0.03, up=(0, s, 0))
    n = max(2, int(round(abs(x1 - x0) / step)))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        flat_bar(p, 'Metal', (x, yo + s * 0.015, z0 - 0.02), (x, yo + s * 0.015, z1 + 0.02), w, t, up=(0, s, 0))
    for x in (x0 + 0.05, (x0 + x1) / 2, x1 - 0.05):        # os suportes até o casco
        p.box('Metal#vp', x - 0.025, x + 0.025, min(y, yo), max(y, yo), z1 - 0.06, z1 - 0.01, bevel=0.0)
        p.box('Metal#vp', x - 0.025, x + 0.025, min(y, yo), max(y, yo), z0 + 0.01, z0 + 0.06, bevel=0.0)


def slat_screen_x(p, y0, y1, z0, z1, x, s, step=0.17, w=0.06, t=0.02):
    """A mesma grade de barras na traseira (o plano YZ em x, pra fora em s)."""
    xo = x + s * 0.14
    for z in (z0, z1):
        flat_bar(p, 'Metal', (xo, y0, z), (xo, y1, z), 0.06, 0.03, up=(s, 0, 0))
    n = max(2, int(round(abs(y1 - y0) / step)))
    for k in range(n + 1):
        y = y0 + (y1 - y0) * k / n
        flat_bar(p, 'Metal', (xo + s * 0.015, y, z0 - 0.02), (xo + s * 0.015, y, z1 + 0.02), w, t, up=(s, 0, 0))
    for y in (y0 + 0.05, (y0 + y1) / 2, y1 - 0.05):
        p.box('Metal#vp', min(x, xo), max(x, xo), y - 0.025, y + 0.025, z1 - 0.06, z1 - 0.01, bevel=0.0)
        p.box('Metal#vp', min(x, xo), max(x, xo), y - 0.025, y + 0.025, z0 + 0.01, z0 + 0.06, bevel=0.0)


# ===================================================================================================== o chassi do T-72/T-90
class T72:
    """As medidas do chassi do T-72/T-90 (o T-72B: casco de 6,86 m, 3,46 m nas esteiras, 3,78 com a saia; a barriga a 0,47; 6 rodas de
    750 mm por lado; esteira de 580 mm; o anel da torre de 2,16 m), na escala de verdade (o tanque USA é o M1A2 de verdade)."""
    NOSE, REAR = 3.45, -3.45
    BELLY, DECK = 0.47, 1.42
    LOW_HW = 1.06                  # o casco de baixo, entre as esteiras
    UP_HW = 1.24                   # o casco de cima (o deck) — as prateleiras dos para-lamas ficam por fora, mais baixas
    FENDER_Z = 1.16                # a prateleira do para-lama, por cima da esteira
    FENDER_HW = 1.80
    SKIRT = (1.76, 1.82)           # a saia (dentro, fora)
    TRACK_Y, TRACK_W = 1.42, 0.58
    WHEEL_R = 0.375
    WHEEL_Z = 0.465
    WHEELS_X = [2.42, 1.46, 0.50, -0.46, -1.42, -2.38]
    IDLER = (3.08, 0.60, 0.27)
    SPROCKET = (-3.13, 0.64, 0.29)
    ROLLERS_X, ROLLER_Z, ROLLER_R = [1.95, 0.0, -1.9], 0.90, 0.075
    GLACIS = ((3.45, 1.0), (2.32, 1.42))          # o glacis de cima: da quina do nariz ao deck
    RING_X, RING_R = 0.35, 1.08                   # o anel da torre
    ERA_SKIRT_X = (3.25, -0.95)                   # a saia com o Relikt (a frente); atrás dela, a grade de barras


def glacis_z(x, G=T72.GLACIS):
    (x0, z0), (x1, z1) = G
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


def t72_hull(p, era_glacis=True, era_skirt=True, rear_slats=True, deck_rear=True, tarp_roll=True, fuel_drums=False,
             front_blade=True, driver=True, fender_boxes=True, exclude_deck=()):
    """O casco do T-72/T-90 (na Part p): o casco de baixo e o de cima, o glacis raso (com os tijolos do Relikt se era_glacis), as
    prateleiras dos para-lamas com as caixas, a saia (o Relikt com a aba de borracha + a grade de barras atrás, ou a de borracha
    lisa), o deck do motor com as venezianas, a traseira com a grade de barras, o escape do lado esquerdo, os faróis com a grade, os
    ganchos e a lâmina de autoentrincheiramento dobrada embaixo do nariz. exclude_deck = [(x0, x1)] faixas do deck sem peças (o
    lançador do TOS deitado, o braço do IMR)."""
    C = T72
    # ---------------------------------------------------------------- o casco
    low = [(3.05, C.BELLY), (C.NOSE - 0.04, 0.92), (C.NOSE - 0.04, C.FENDER_Z), (C.REAR + 0.02, C.FENDER_Z), (C.REAR + 0.02, 0.66),
           (-3.25, C.BELLY)]
    solid(p, 'Paint#shell', [(x, s * C.LOW_HW, z) for x, z in low for s in (-1, 1)])
    (gx0, gz0), (gx1, gz1) = C.GLACIS
    up = [(C.NOSE, C.FENDER_Z - 0.04), (C.NOSE, gz0), (gx1, gz1), (-3.36, C.DECK), (C.REAR, 1.34), (C.REAR, C.FENDER_Z - 0.04)]
    solid(p, 'Paint#shell', [(x, s * C.UP_HW, z) for x, z in up for s in (-1, 1)])
    # o nariz: a quina soldada e a chapa de baixo (a lâmina de autoentrincheiramento dobrada nela)
    vp.weld_bead(p, 'Paint#vp', [(C.NOSE - 0.005, -C.UP_HW + 0.05, gz0 - 0.01), (C.NOSE - 0.005, C.UP_HW - 0.05, gz0 - 0.01)], r=0.018)
    if front_blade:
        # a chapa de baixo do nariz sobe a ~51°: a lâmina deitada nela (a normal pra frente e pra baixo), as 3 nervuras e o fio
        a = math.degrees(math.atan2(0.92 - C.BELLY, C.NOSE - 0.04 - 3.05))
        p.obox('Paint#plate', (3.265, 0.0, 0.675), (0.07, 2.0, 0.44), pitch=-(90.0 - a), bevel=0.03)
        for y in (-0.62, 0.0, 0.62):
            p.obox('Paint#plate', (3.3, y, 0.66), (0.06, 0.08, 0.38), pitch=-(90.0 - a), bevel=0.02)
        p.obox('Metal', (3.13, 0.0, 0.49), (0.06, 2.02, 0.05), pitch=-(90.0 - a), bevel=0.012)          # o fio da lâmina
    # ---------------------------------------------------------------- o glacis: o Relikt em 2 fileiras com o vão do motorista
    eu = Vector((gx1 - gx0, 0.0, gz1 - gz0)).normalized()          # subindo o glacis (pra trás)
    en = Vector((-eu.z, 0.0, eu.x)) * -1.0
    if en.z < 0:
        en = -en
    if era_glacis:
        L = (Vector((gx1, 0, gz1)) - Vector((gx0, 0, gz0))).length
        nv = 7
        cv = (2 * C.UP_HW - 0.08) / nv
        O = Vector((gx0, -C.UP_HW + 0.04, gz0)) + eu * 0.06
        era_grid(p, O, eu, Vector((0, 1, 0)), en, 2, nv, (L - 0.3) / 2, cv, 0.1, skip=((1, 3),))
    # a régua de respingo em V do glacis (a do T-72) por cima dos tijolos
    for s in (-1, 1):
        a, b = Vector((gx0 - 0.03, 0.0, gz0 + 0.12)) + eu * 0.25, Vector((gx0, 0.95 * s, gz0 + 0.1)) + eu * 0.02
        vp.tube(p, 'Paint#vp', [a, b], 0.022, seg=6)
    if driver:
        # o motorista: a escotilha no meio, logo atrás do glacis, e o bloco do periscópio no vão dos tijolos (o glacis desce 20°)
        hx = gx1 - 0.18
        vp.prism(p, 'Paint#plate', vp.rounded_rect(0.5, 0.56, 0.2, 3), Vector((hx, 0.0, C.DECK - 0.004)), Vector((1, 0, 0)),
                 Vector((0, 1, 0)), Vector((0, 0, 1)), 0.045)
        vp.hinge(p, 'Metal', (hx - 0.27, 0.0, C.DECK + 0.012), 'y', 0.4, r=0.02, knuckles=3)
        sl = math.degrees(math.atan2(gz1 - gz0, gx0 - gx1))
        px = gx1 + 0.16
        vp.rbox(p, 'Paint#plate', (px, 0.0, glacis_z(px) + 0.06), (0.2, 0.3, 0.12), pitch=-sl, r=0.04, k=2)
        p.obox('Lens', (px + 0.1, 0.0, glacis_z(px) + 0.1), (0.012, 0.22, 0.05), pitch=-sl, bevel=0.0)
    # os faróis com a grade na frente da prateleira da esquerda (o T-72: o farol e o infravermelho grande) e os ganchos de reboque
    for (y, r) in ((-1.62, 0.075), (-1.4, 0.06)):
        headlight(p, (C.NOSE - 0.16, y, C.FENDER_Z + 0.13), r=r)
        vp.rbox(p, 'Paint#plate', (C.NOSE - 0.22, y, C.FENDER_Z + 0.05), (0.14, 0.18, 0.1), r=0.03, k=2)
    for s in (-1, 1):
        for xx, sg in ((C.NOSE - 0.04, 1), (C.REAR + 0.02, -1)):
            for d in (-1, 1):
                vp.rbox(p, 'Metal', (xx + 0.05 * sg, 0.7 * s + d * 0.055, 0.86), (0.12, 0.035, 0.15), r=0.03, k=2)
            p.cyl('Metal#vp', (xx + 0.07 * sg, 0.7 * s, 0.88), 'y', 0.022, 0.16, seg=10, bevel=0.0)
            p.torus('Dark#vp', (xx + 0.1 * sg, 0.7 * s, 0.81), 'y', 0.055, 0.017, seg=14, rseg=6)
    # ---------------------------------------------------------------- as prateleiras dos para-lamas, os para-barros da frente
    for s in (-1, 1):
        y0, y1 = sorted((C.UP_HW * s, C.FENDER_HW * s))
        vp.rbox(p, 'Paint#thin', ((C.NOSE - 0.12 + C.REAR + 0.02) / 2, (y0 + y1) / 2, C.FENDER_Z - 0.015),
                (C.NOSE - 0.12 - C.REAR - 0.02, y1 - y0, 0.03), r=0.03, k=2)
        # o para-barro da frente (a chapa que levanta, articulada) e o de trás
        vp.rbox(p, 'Paint#thin', (C.NOSE - 0.02, 1.5 * s, C.FENDER_Z - 0.12), (0.03, 0.56, 0.26), pitch=-10, r=0.04, k=2)
        vp.hinge(p, 'Metal', (C.NOSE - 0.1, 1.5 * s, C.FENDER_Z + 0.01), 'y', 0.46, r=0.02, knuckles=3)
        vp.mudflap(p, (C.REAR + 0.05, 1.5 * s, C.FENDER_Z - 0.03), 0.5, 0.36, yaw=180, m='Rubber')
        # a beirada dobrada do para-lama (a linha que corre o carro)
        yy = C.FENDER_HW * s
        vp.tube(p, 'Paint#vp', [(C.NOSE - 0.15, yy, C.FENDER_Z), (C.REAR + 0.05, yy, C.FENDER_Z)], 0.018, seg=6)
    # ---------------------------------------------------------------- a saia: o Relikt com a aba de borracha + a grade de barras atrás
    ex0, ex1 = C.ERA_SKIRT_X
    for s in (-1, 1):
        yi, yo = C.SKIRT[0] * s, C.SKIRT[1] * s
        t = C.SKIRT[1] - C.SKIRT[0]
        if era_skirt:
            seams = [ex0 - (ex0 - ex1) * k / 6 for k in range(7)]
            for k in range(6):
                xa, xb = seams[k + 1] + 0.015, seams[k] - 0.015
                # a chapa de cima (o suporte) e a caixa do Relikt nela (a tampa com os parafusos)
                vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 1.0), (xb - xa, 0.32, t), roll=90, r=0.04, k=2)
                frame_box(p, 'Paint#plate', ((xa + xb) / 2, (C.SKIRT[1] + 0.05) * s, 0.98), Vector((1, 0, 0)), Vector((0, 0, 1)),
                          Vector((0, s, 0)), xb - xa - 0.05, 0.4, 0.1)
                vp.bolt_row(p, 'Metal', (xa + 0.09, (C.SKIRT[1] + 0.1) * s, 1.13), (xb - 0.09, (C.SKIRT[1] + 0.1) * s, 1.13), 3,
                            (0, s, 0), r=0.018, h=0.012)
                vp.bolt_row(p, 'Metal', (xa + 0.09, (C.SKIRT[1] + 0.1) * s, 0.83), (xb - 0.09, (C.SKIRT[1] + 0.1) * s, 0.83), 3,
                            (0, s, 0), r=0.018, h=0.012)
                # a aba de borracha embaixo (pendurada, com onda)
                vp.mudflap(p, ((xa + xb) / 2, (C.SKIRT[0] + t / 2) * s, 0.79), xb - xa + 0.005, 0.26, yaw=90 * s, m='Rubber',
                           t=0.022, folds=1)
            # a da frente em cunha (o canto chanfrado da frente)
            vp.rbox(p, 'Paint#plate', (ex0 + 0.12, (C.SKIRT[0] + t / 2) * s, 1.04), (0.24, 0.5, t), roll=90, pitch=0, r=0.08, k=3)
        else:
            vp.mudflap(p, ((ex0 + ex1) / 2, (C.SKIRT[0] + t / 2) * s, C.FENDER_Z - 0.02), ex0 - ex1, 0.48, yaw=90 * s, m='Rubber',
                       t=0.025, folds=6)
        if rear_slats:
            slat_screen(p, ex1 - 0.05, C.REAR + 0.15, 0.7, C.FENDER_Z + 0.05, C.SKIRT[0] * s, s)
        else:
            vp.mudflap(p, ((ex1 + C.REAR) / 2, (C.SKIRT[0] + t / 2) * s, C.FENDER_Z - 0.02), ex1 - C.REAR - 0.1, 0.45, yaw=90 * s,
                       m='Rubber', t=0.025, folds=4)
    # o casco de baixo atrás das rodas (o forro escuro e os braços da suspensão)
    for s in (-1, 1):
        yy0, yy1 = sorted((C.LOW_HW * s, (C.LOW_HW + 0.015) * s))
        p.box('Dark', -3.2, 3.1, yy0, yy1, 0.52, C.FENDER_Z - 0.05, bevel=0.0)
        for x in C.WHEELS_X:
            yy0, yy1 = sorted((C.LOW_HW * s, (C.TRACK_Y - C.TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.07, x + 0.3, yy0, yy1, C.WHEEL_Z - 0.05, C.WHEEL_Z + 0.05, bevel=0.0)
    # ---------------------------------------------------------------- o deck: o anel da torre
    vp.lathe(p, 'Dark', (C.RING_X, 0.0, C.DECK), 'z', [(C.RING_R - 0.06, 0.0), (C.RING_R + 0.06, 0.0), (C.RING_R + 0.06, 0.012),
                                                      (C.RING_R + 0.045, 0.02), (C.RING_R - 0.06, 0.02)], seg=64, closed=True)

    def free(x0, x1):
        return not any(a < x1 and x0 < b for a, b in exclude_deck)
    # ---------------------------------------------------------------- o deck do motor: as venezianas, as tampas, a lona enrolada
    if deck_rear:
        if free(-3.35, -1.25):
            grille_deck(p, -2.25, -1.3, -1.05, 1.05, C.DECK, 7, along='y')
            grille_deck(p, -3.3, -2.35, -1.1, -0.1, C.DECK, 7, along='y')
            grille_deck(p, -3.3, -2.35, 0.1, 1.1, C.DECK, 7, along='y')
            vp.panel_seam(p, 'Dark', (-1.18, -1.15, C.DECK + 0.003), (-1.18, 1.15, C.DECK + 0.003), (0, 0, 1), w=0.024, d=0.01)
        for (x0, x1) in ((-1.15, -0.8),):
            if free(x0, x1):
                vp.bolt_row(p, 'Metal', (x0, -1.0, C.DECK), (x0, 1.0, C.DECK), 9, (0, 0, 1), r=0.016, h=0.012)
    # a traseira: a placa com as dobradiças, a grade de barras por cima, o escape (lado esquerdo), as lanternas
    vp.panel_seam(p, 'Dark', (C.REAR, -1.0, 1.02), (C.REAR, 1.0, 1.02), (-1, 0, 0), w=0.024, d=0.01)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (C.REAR - 0.03, 1.12 * s, 1.24), (0.06, 0.16, 0.1), roll=0, r=0.03, k=2)
        p.box('Lens', C.REAR - 0.07, C.REAR - 0.05, 1.12 * s - 0.05, 1.12 * s + 0.05, 1.21, 1.27, bevel=0.0)
    if rear_slats:
        slat_screen_x(p, -1.3, 1.3, 0.72, 1.36, C.REAR, -1)
    # o escape do lado esquerdo (a grelha na lateral do casco de cima, logo acima da prateleira, atrás)
    p.box('Dark', -2.65, -2.05, -C.UP_HW - 0.012, -C.UP_HW, C.FENDER_Z + 0.04, C.DECK - 0.04, bevel=0.0)
    for k in range(6):
        p.obox('Metal#vp', (-2.6 + k * 0.1, -C.UP_HW - 0.015, (C.FENDER_Z + C.DECK) / 2), (0.025, 0.025, 0.2), bevel=0.0)
    # ---------------------------------------------------------------- as caixas das prateleiras (baixas: o cano passa por cima girando)
    if fender_boxes:
        for s in (-1, 1):
            yc = (C.UP_HW + C.FENDER_HW) / 2 * s
            xs = ((2.55, 1.85), (1.75, 1.05), (-0.95, -1.75), (-1.85, -2.55)) if s > 0 else ((2.55, 1.85), (1.75, 1.05), (-0.95, -1.75))
            for (xa, xb) in xs:
                vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, yc, C.FENDER_Z + 0.11), (xa - xb - 0.04, 0.5, 0.21), r=0.04, k=2)
                vp.rbox(p, 'Paint#plate', ((xa + xb) / 2, yc, C.FENDER_Z + 0.225), (xa - xb, 0.53, 0.03), r=0.045, k=2)
                for d in (-0.2, 0.2):
                    vp.rbox(p, 'Metal', ((xa + xb) / 2 + d, yc + 0.27 * s, C.FENDER_Z + 0.17), (0.05, 0.02, 0.07), r=0.008, k=1)
        # os tanques de combustível externos (o T-72: os cilindros na prateleira da direita, presos pela cinta)
        for x in (0.62, -0.18):
            vp.lathe(p, 'Paint', (x, 1.52, C.FENDER_Z + 0.13), 'x', [(0.0, -0.36), (0.1, -0.36), (0.13, -0.33), (0.13, 0.33),
                                                                     (0.1, 0.36), (0.0, 0.36)], seg=18)
            for d in (-0.2, 0.2):
                vp.lathe(p, 'Metal', (x + d, 1.52, C.FENDER_Z + 0.13), 'x', [(0.128, -0.02), (0.14, -0.02), (0.14, 0.02),
                                                                              (0.128, 0.02)], seg=18, closed=True)
        # a ferramenta na prateleira da esquerda (o pé de cabra e a pá)
        vp.crowbar(p, (0.35, -1.62, C.FENDER_Z + 0.04), yaw=0, m='Metal', length=1.2)
        vp.shovel(p, (-0.3, -1.4, C.FENDER_Z + 0.03), yaw=180, handle_m='Metal', blade_m='Dark', length=1.0)
    # os tambores de combustível atrás (o T-72 de sempre; o T-90M não leva)
    if fuel_drums:
        for s in (-1, 1):
            vp.drum(p, (C.REAR - 0.3, 0.62 * s, 1.55), 'y', 0.29, 0.86, m='Paint', cap_m='Metal', seg=24)
    if tarp_roll:
        # a lona ENROLADA da cor do jogador presa na quina de trás do deck (FACCAO_RUSSIA.md §2: "a lona enrolada no motor")
        vp.tarp_roll(p, (C.REAR + 0.14, 0.0, C.DECK + 0.09), 'y', 2.1, 0.11, m='Team', straps=3, strap_m='Cloth',
                     buckle_m='Metal')
        for y in (-0.7, 0.0, 0.7):
            p.box('Metal#vp', C.REAR + 0.05, C.REAR + 0.28, y - 0.03, y + 0.03, C.DECK - 0.02, C.DECK + 0.01, bevel=0.0)


def t72_wheels(p, single_rib=True, paint='Paint'):
    """A rodagem do T-72/T-90: 6 rodas de apoio DUPLAS de 750 mm (a banda de borracha, o disco rebaixado com as 8 nervuras radiais do
    fundido do T-72, o cubo com as porcas), a roda-guia de aço na frente, a motriz dentada atrás e os 3 roletes de cima."""
    C = T72
    for s in (-1, 1):
        y = C.TRACK_Y * s
        for x in C.WHEELS_X:
            ribbed_wheel(p, (x, y, C.WHEEL_Z), C.WHEEL_R, 0.56, s, paint)
        ix, iz, ir = C.IDLER
        vp.idler(p, (ix, y, iz), ir, 0.5, s, paint, 'Metal', gap=0.09)
        sx, sz, sr = C.SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.52, s, paint, 'Metal', teeth=12, holes_m='Dark')
        for x in C.ROLLERS_X:
            vp.return_roller(p, (x, y, C.ROLLER_Z), C.ROLLER_R, 0.28, 'Metal', 'Rubber')


def ribbed_wheel(p, c, r, w, side, paint, gap=0.09, ribs=8):
    """A roda de apoio dupla com as nervuras radiais no prato de fora (o fundido do T-72: o ritmo de raios que lê de lado)."""
    vp.road_wheel(p, c, r, w, side, paint, 'Metal', 'Rubber', gap=gap, seg=24, dish=0.05)
    A = Vector((0, side, 0))
    _, E1, E2 = vp.axis_frame(A)
    ww = (w - gap) / 2
    hw = ww / 2
    Cw = Vector(c) + A * (gap / 2 + hw)
    face = Cw + A * (hw - 0.05)
    for k in range(ribs):
        a = 2 * math.pi * (k + 0.5) / ribs
        rad = E1 * math.cos(a) + E2 * math.sin(a)
        tan = A.cross(rad).normalized()
        vp._obox_frame(p, paint, face + rad * (r * 0.55) + A * 0.022, rad, tan, A, r * 0.3, 0.04, 0.044)


def t72_track(name, side, C=T72, pitch=0.15):
    """A esteira de elos (model_vehicle_parts.track_links: anda no jogo) do T-72: o elo de 580 mm com a garra, as 2 almofadas, o
    conector do pino nas bordas e o dente-guia no vão das rodas duplas; a barriga entre os roletes de cima."""
    s = 1 if side == 'r' else -1
    m = 0.06
    circles = [(C.SPROCKET[0], C.SPROCKET[1], C.SPROCKET[2] + m + 0.01), (C.IDLER[0], C.IDLER[1], C.IDLER[2] + m)] + \
              [(x, C.WHEEL_Z, C.WHEEL_R + m) for x in C.WHEELS_X] + [(x, C.ROLLER_Z, C.ROLLER_R + m) for x in C.ROLLERS_X]
    zt = C.ROLLER_Z + C.ROLLER_R + m
    rx = sorted(C.ROLLERS_X)
    sags = [(a + 0.12, b - 0.12, zt, 0.03) for a, b in zip(rx[:-1], rx[1:])]
    return vp.track_links(name, circles, C.TRACK_Y * s, C.TRACK_W, pitch=pitch, t_in=0.05, t_out=0.014, grouser=0.022,
                          guide='single', m='Track', guide_m='Metal', sags=sags, center_gap=0.09, horn_w=0.065)


# ===================================================================================================== caminhão (KamAZ)
def kamaz_cab(p, x1, hw, zb, zr, depth=1.72, armored=False):
    """A cabine avançada do KamAZ (por cima da roda da frente): a frente chata num plano só, deitada ~4° (o para-brisa em 2 folhas com a
    coluna do meio e os limpadores, a grade larga de aletas embaixo dele), a quina de cima redonda, a pala de sol, o para-choque grosso
    com os faróis retangulares e o guincho, as portas com a janela, a maçaneta, os 2 degraus na frente da roda, o retrovisor grande de
    braço, a escotilha e as luzes de teto. armored: o 6560 do Pantsir (as janelas menores com a moldura blindada). x1 = a frente (no
    pé do para-brisa); a cabine vai até x1 − depth, do piso zb ao teto zr. Devolve fx(z), o x da frente na altura z."""
    x0 = x1 - depth
    F0x, F0z = x1 + 0.02, zb + 0.15
    F1x, F1z = x1 - 0.06, zr - 0.16
    lean = (F1x - F0x) / (F1z - F0z)

    def fx(z):
        return F0x + (z - F0z) * lean
    pa = math.degrees(math.atan2(-(F1x - F0x), F1z - F0z))       # a inclinação da frente (a normal pra frente e pra cima)
    pts = []
    for s in (-1, 1):
        pts += [(x0, s * hw, zb), (x0, s * hw, zr), (x1 - 0.26, s * (hw - 0.02), zr), (x1 - 0.1, s * (hw - 0.02), zr - 0.05),
                (F1x, s * hw, F1z), (F0x, s * hw, F0z), (x1 - 0.02, s * hw, zb)]
    solid(p, 'Paint#shell', pts, bevel=0.06)
    # o para-brisa em 2 folhas, no plano da frente
    zw1 = zr - 0.24
    zw0 = zw1 - (0.48 if not armored else 0.38)
    zc = (zw0 + zw1) / 2
    for y in (-hw * 0.49, hw * 0.49):
        w = hw * (0.86 if not armored else 0.62)
        p.obox('Lens', (fx(zc) + 0.008, y, zc), (0.02, w, zw1 - zw0), pitch=pa, bevel=0.0)
        if armored:
            # a moldura blindada grossa em volta (a chapa com a janela recortada: 4 barras)
            for (dy, dz, sy, sz) in ((0, (zw1 - zw0) / 2 + 0.05, w + 0.2, 0.1), (0, -(zw1 - zw0) / 2 - 0.05, w + 0.2, 0.1),
                                     (w / 2 + 0.05, 0, 0.1, zw1 - zw0), (-w / 2 - 0.05, 0, 0.1, zw1 - zw0)):
                z = zc + dz
                p.obox('Paint#plate', (fx(z) + 0.03, y + dy, z), (0.05, sy, sz), pitch=pa, bevel=0.015)
    p.obox('Dark', (fx(zc) + 0.012, 0.0, zc), (0.025, 0.08, zw1 - zw0 + 0.04), pitch=pa, bevel=0.0)
    for y in (-hw * 0.49, hw * 0.49):                                              # os limpadores (deitados no pé do vidro)
        p.obox('Dark#vp', (fx(zw0 + 0.06) + 0.03, y - 0.1, zw0 + 0.06), (0.015, 0.5, 0.02), roll=8, bevel=0.0)
    # a pala de sol por cima do para-brisa
    vp.rbox(p, 'Paint#plate', (fx(zr - 0.12) + 0.1, 0.0, zr - 0.1), (0.26, 2 * hw - 0.12, 0.035), pitch=-8, r=0.04, k=2)
    # a grade larga embaixo do para-brisa (as aletas horizontais) e a chapa redonda no meio (sem letra nem emblema)
    gz0, gz1 = zb + 0.12, zw0 - 0.06
    p.obox('Dark', (fx((gz0 + gz1) / 2) + 0.004, 0.0, (gz0 + gz1) / 2), (0.02, hw * 1.64, gz1 - gz0), pitch=pa, bevel=0.0)
    n = max(3, int((gz1 - gz0) / 0.065))
    for k in range(n):
        z = gz0 + (gz1 - gz0) * (k + 0.5) / n
        vp.rbox(p, 'Paint#thin', (fx(z) + 0.025, 0.0, z), (0.03, hw * 1.6, 0.024), pitch=pa, r=0.01, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Paint#plate', (fx((gz0 + gz1) / 2) + 0.02, hw * 0.86 * s, (gz0 + gz1) / 2), (0.05, 0.08, gz1 - gz0 + 0.08),
                pitch=pa, r=0.02, k=2)
    vp.lathe(p, 'Metal', (fx((gz0 + gz1) / 2) + 0.05, 0.0, (gz0 + gz1) / 2), 'x', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.02), (0.05, 0.03),
                                                                                (0.0, 0.03)], seg=16)
    # o para-choque grosso (embaixo do piso, na frente do chassi) com os faróis retangulares, as setas, o guincho e as argolas
    bz = zb - 0.1
    vp.rbox(p, 'Paint#plate', (x1 + 0.1, 0.0, bz), (0.2, 2 * hw + 0.04, 0.32), r=0.05, k=3)
    for s in (-1, 1):
        for yy in (hw - 0.3, hw - 0.6):
            vp.rbox(p, 'Metal', (x1 + 0.2, yy * s, bz + 0.04), (0.03, 0.24, 0.13), r=0.02, k=2)
            vp.rbox(p, 'Lens', (x1 + 0.21, yy * s, bz + 0.04), (0.02, 0.2, 0.1), r=0.02, k=2)
            p.box('Light', x1 + 0.214, x1 + 0.218, yy * s - 0.06, yy * s + 0.06, bz + 0.02, bz + 0.06, bevel=0.0)
        p.box('Lens', x1 + 0.2, x1 + 0.215, (hw - 0.1) * s - 0.04, (hw - 0.1) * s + 0.04, bz + 0.0, bz + 0.08, bevel=0.0)
        p.torus('Metal#vp', (x1 + 0.22, 0.42 * s, bz - 0.13), 'y', 0.06, 0.018, seg=14, rseg=6)
    vp.rbox(p, 'Metal', (x1 + 0.24, 0.0, bz - 0.04), (0.12, 0.5, 0.16), r=0.03, k=2)
    vp.lathe(p, 'Metal', (x1 + 0.3, 0.0, bz - 0.04), 'y', [(0.0, -0.2), (0.08, -0.2), (0.08, -0.18), (0.06, -0.17), (0.06, 0.17),
                                                         (0.08, 0.18), (0.08, 0.2), (0.0, 0.2)], seg=14)
    # as portas, as janelas, os degraus (na frente da roda), o corrimão, o retrovisor grande
    for s in (-1, 1):
        y = hw * s
        yo = (hw + 0.006) * s
        dx0, dx1 = x0 + 0.22, x1 - 0.2
        for xx in (dx0, dx1):
            p.box('Dark', xx - 0.012, xx + 0.012, min(y, yo), max(y, yo), zb + 0.08, zr - 0.08, bevel=0.0)
        wz0 = zr - (0.62 if not armored else 0.46)
        p.box('Lens', dx0 + 0.08, dx1 - 0.1, min(y, yo + 0.01 * s), max(y, yo + 0.01 * s), wz0, zr - 0.15, bevel=0.0)
        if armored:
            p.box('Paint#thin', dx0 + 0.03, dx1 - 0.05, min(y, yo + 0.035 * s), max(y, yo + 0.035 * s), wz0 - 0.07, wz0 - 0.01)
        p.box('Metal', dx0 + 0.1, dx0 + 0.24, min(y, yo + 0.03 * s), max(y, yo + 0.03 * s), wz0 - 0.13, wz0 - 0.09, bevel=0.0)
        for k, zz in enumerate((zb - 0.42, zb - 0.16)):
            vp.rbox(p, 'Metal', (x1 - 0.16, (hw - 0.14) * s, zz), (0.3, 0.26, 0.035), r=0.03, k=1)
            p.box('Metal#vp', x1 - 0.31, x1 - 0.29, min((hw - 0.27) * s, (hw - 0.01) * s), max((hw - 0.27) * s, (hw - 0.01) * s),
                  zz - 0.02, zb, bevel=0.0)
        vp.grab_handle(p, 'Metal', (x1 - 0.12, y, zb + 0.25), (x1 - 0.12, y, zr - 0.4), (0, s, 0), height=0.05, r=0.014)
        root = Vector((x1 - 0.1, y, zr - 0.35))
        vp.tube(p, 'Metal', vp.bent_bar([root, root + Vector((0.05, 0.26 * s, 0.05)), root + Vector((0.08, 0.3 * s, -0.25))], R=0.05,
                                        n=2), 0.016, seg=6)
        vp.rbox(p, 'Dark', (x1 - 0.02, (hw + 0.32) * s, zr - 0.52), (0.05, 0.17, 0.36), yaw=-8 * s, r=0.03, k=2)
        vp.rbox(p, 'Lens', (x1 + 0.005, (hw + 0.32) * s, zr - 0.52), (0.012, 0.13, 0.3), yaw=-8 * s, r=0.03, k=2)
    # o teto: a escotilha, as 3 luzes de gabarito e a antena
    vp.hatch_round(p, (x0 + 0.62, -hw * 0.38, zr), 0.26, 'Paint', 'Metal', handle_m='Metal')
    for y in (-0.25, 0.0, 0.25):
        vp.rbox(p, 'Lens', (x1 - 0.3, y, zr + 0.03), (0.08, 0.12, 0.05), r=0.02, k=2)
    vp.lathe(p, 'Dark', (x0 + 0.2, hw - 0.2, zr), 'z', [(0.0, 0.0), (0.05, 0.0), (0.05, 0.05), (0.03, 0.07), (0.025, 0.14), (0.0, 0.15)],
             seg=10)
    p.cyl('Dark#vp', (x0 + 0.2, hw - 0.2, zr + 0.15 + 0.6), 'z', 0.008, 1.2, seg=6, bevel=0.0)
    return fx


def arc_fender(p, m, x, z, y0, y1, R, a0=15.0, a1=165.0, t=0.03, n=14):
    """O para-lama curvo por cima de uma roda (centro x, z; de y0 a y1): a chapa no arco de a0 a a1 graus (0 = pra frente; sobe por
    cima), com a espessura t."""
    rings = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        c, s_ = math.cos(a), math.sin(a)
        rings.append([(x + c * R, y0, z + s_ * R), (x + c * R, y1, z + s_ * R), (x + c * (R + t), y1, z + s_ * (R + t)),
                      (x + c * (R + t), y0, z + s_ * (R + t))])
    p.loft(m, rings, bevel=0.0, smooth=True)


def tyre_wheels(prefix_fmt, axles, wheel_y, r, w, rim_r, start=1, style='mud', nuts=8):
    """As rodas de caminhão (uma Part por roda, wheel_NNN: o jogo gira cada uma pelo centro da caixa): o pneu de verdade (o flanco
    abaulado, os blocos em V subindo no ombro), o aro de aço rebaixado com os furos, o cubo e o anel de porcas; o lado de fora mostra o
    aro (a roda da esquerda e a da direita espelhadas)."""
    objs = []
    n = start - 1
    for x in axles:
        for s in (-1, 1):
            n += 1
            wp = Part(prefix_fmt % n)
            vp.tyre(wp, (x, wheel_y * s, r), r, w, s, rim_r=rim_r, rubber='Rubber', rim='Paint', hub='Metal', seg=32, holes_m='Dark',
                    style=style, nuts=nuts)
            objs.append(wp.finish())
    return objs


# ===================================================================================================== saída
def write_spec(out, parts, usa_at=(0, 6.5, 0), game_yaw=200.0, extra=None):
    """O preview.json do model_preview.py (as vistas da ficha e a câmera do jogo; o tanque da USA de régua do lado)."""
    spec = {
        'parts': [[n, [round(a, 4) for a in at]] for n, at in parts],
        'views': [['top', 'ortho', [0, 0, -1], [0, -1, 0], [1800, 1000]],
                  ['left', 'ortho', [0, -1, 0], [0, 0, 1], [1800, 760]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['back', 'ortho', [1, 0, 0], [0, 0, 1], [1000, 760]],
                  ['three_quarter', 'persp', 30.0, 215.0, [1600, 1000], 30.0, 0.78],
                  ['back_three_quarter', 'persp', 30.0, 35.0, [1600, 1000], 30.0, 0.78]],
        'usa_at': list(usa_at), 'tank_views': False,
        'game': [['game_cam', 37.5, game_yaw, 50.0, 0.85], ['game_cam_close', 37.5, game_yaw, 30.0, 0.8]],
    }
    if extra:
        spec.update(extra)
    with open(os.path.join(out, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def keep_in_place_until_export(objs_origins):
    """(o executor do detalhe: model_run_detail.py) as peças com origem própria (a lâmina, o radar, a sirene, o lançador) ficam NO
    LUGAR pro cozimento das máscaras (a sombra de contato certa) e só vão pra origem delas na hora de exportar. objs_origins =
    [(objeto, (x, y, z) Unreal)]. Sem o executor, leva pra origem já."""
    import model_lib
    if model_lib.DETAIL.get('defer') is None:
        for o, at in objs_origins:
            o.data.transform(Matrix.Translation(-V(*at)))
        return
    _orig = model_lib.export
    table = {o.as_pointer(): at for o, at in objs_origins}

    def _export(o, out_dir):
        if model_lib.DETAIL.get('defer') is None and o.as_pointer() in table:
            o.data.transform(Matrix.Translation(-V(*table.pop(o.as_pointer()))))
        return _orig(o, out_dir)
    model_lib.export = _export
