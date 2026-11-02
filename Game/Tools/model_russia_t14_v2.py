"""O pesado da Rússia (T-14 Armata), v2 (01/11/2026): refeito pelo veredito da galeria ("muito parecido com o outro tanque", o T-90M).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      vehicle Game/Tools/model_russia_t14_v2.py C:/Users/luisf/explosion/captures/game/models/russia_t14_v2

A regra (docs/elenco/SILHUETAS_USA.md, a mesma pra Rússia): cada blindado diferente DE CIMA em cinco coisas. Contra o T-90M:
| | T-90M | T-14 v2 |
| tamanho | casco 6,9 x 3,8 m, 9,5 m com o cano | casco 8,7 x 3,5 m (+26% de comprimento, mais estreito: o mais comprido e o mais chato), ~11 m com o cano |
| contorno do casco | retângulo, frente reta | o NARIZ CHANFRADO (as quinas da frente cortadas 0,6 m: de cima a ponta de flecha rasa), a cápsula da tripulação com as 3 ESCOTILHAS EM FILA na frente |
| a torre | redonda, baixa, o Relikt em V, no meio | a torre SEM TRIPULAÇÃO, ALTA E FACETADA (as faces de cima caindo ~30° pra dentro: os ângulos duros que pegam a luz em planos), a ponta da frente em flecha, recuada; sem escotilha |
| a assinatura | a gaiola com a lona e a rede | a torre facetada com os sensores: os 4 painéis do radar do Afganit nas quinas, a mira do comandante no mastro alto, a CAIXA DA DEFESA ATIVA atrás no teto com os 2 lançadores; o cano comprido e liso (sem o evacuador) |
| laterais | o Relikt na frente, a grade atrás | os 4 painéis grandes e lisos do Malakhit na frente e a GRELHA GRANDE de quadros atrás |
| rodas | 6 | 7 |
| a cor do jogador | a lona e a rede na gaiola da torre | a LONA esticada e amarrada no deck do motor, com a rede embolada e a lona enrolada atrás (de cima: o vermelho na traseira, longe da torre) |
A gaiola da facção não vai na torre: a torre é sem tripulação (ninguém embaixo pra proteger) e a forma facetada é a assinatura; a grade
da família fica nas saias (a grelha grande). Na lateral da torre, a estrela aprovada (docs/elenco/EMBLEMAS.md: a vermelha com as bordas
branca, azul e vermelha) e o número tático branco.

Peças (origem no chão, no centro do casco, frente em +X; todas no lugar):
  SM_Russia_T14_Hull     casco, saias, a cápsula com as 3 escotilhas, o deck com a lona e a rede, o anel;
  SM_Russia_T14_Turret   a torre (gira em Z em volta de (TX, 0)), os sensores, o Afganit, a caixa da defesa ativa;
  SM_Russia_T14_Barrel   o canhão com a capa da raiz (recua em −X; a boca é o X máximo);
  SM_Russia_T14_Wheels   a rodagem (wheel_*);
  SM_Russia_T14_Track_l / _Track_r  as esteiras.
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

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'russia_t14_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_Russia_T14'


class T:
    """O chassi do T-14 (a plataforma Armata): 8,7 m, o casco de cima cobrindo as esteiras, 7 rodas de 700 mm, esteira de 620 mm."""
    NOSE, REAR = 4.35, -4.35
    BELLY, DECK = 0.48, 1.55
    LOW_HW = 1.02
    UP_HW = 1.68
    SPON_Z = 1.10                 # o fundo do casco de cima (por cima da esteira)
    SKIRT = (1.69, 1.73)
    CHAMF = 0.6                   # as quinas da frente cortadas
    NOSE_HW = 0.95                # a chapa reta da ponta
    TRACK_Y, TRACK_W = 1.36, 0.62
    WHEEL_R = 0.35
    WHEEL_Z = 0.44
    WHEELS_X = [3.0, 2.0, 1.0, 0.0, -1.0, -2.0, -3.0]
    IDLER = (3.82, 0.62, 0.27)
    SPROCKET = (-3.9, 0.68, 0.29)
    ROLLERS_X, ROLLER_Z, ROLLER_R = [2.5, 0.5, -1.5], 0.91, 0.07
    GLACIS = ((4.35, 1.2), (3.1, 1.55))
    RING_X, RING_R = -0.45, 1.15


C = T
TX = C.RING_X
T_Z0, T_Z1, T_Z2 = 1.60, 2.05, 2.72          # o pé, a cintura e o teto da torre (alta)
GUN_Z = 2.12
MANTLET_X = TX + 1.86
MUZZLE_X = 6.55                              # 2,2 m além do nariz: ~11 m com o cano
DECK_REAR = (-2.88, C.REAR + 0.12)           # o deck de trás fora do giro da torre (a lona e a rede)

SEG = {'a': ((0.0, 1.0), (1.0, 1.0)), 'b': ((1.0, 0.5), (1.0, 1.0)), 'c': ((1.0, 0.0), (1.0, 0.5)), 'd': ((0.0, 0.0), (1.0, 0.0)),
       'e': ((0.0, 0.0), (0.0, 0.5)), 'f': ((0.0, 0.5), (0.0, 1.0)), 'g': ((0.0, 0.5), (1.0, 0.5))}
DIG = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc', '8': 'abcdefg',
       '9': 'abcdfg'}


def tac_number(p, c, n, text, h=0.22, m='White'):
    """O número tático branco em estêncil de 7 traços na face de normal n (pode inclinar: a face da torre que abre pra fora), centro da
    linha em c; o "pra cima" do número segue a face."""
    c = Vector(c)
    n = Vector(n).normalized()
    rd = Vector((n.y, -n.x, 0.0)).normalized()      # quem olha a face de fora lê pra direita (o lado +y lê pra +x)
    up = n.cross(rd).normalized()
    if up.z < 0:
        up = -up
    w, gap, sw = h * 0.56, h * 0.3, h * 0.16
    total = len(text) * w + (len(text) - 1) * gap
    for i, ch in enumerate(text):
        ox = -total / 2 + i * (w + gap)
        for sg in DIG.get(ch, ''):
            (u0, v0), (u1, v1) = SEG[sg]
            a = c + rd * (ox + u0 * w) + up * ((v0 - 0.5) * h)
            b = c + rd * (ox + u1 * w) + up * ((v1 - 0.5) * h)
            ex = (b - a).normalized()
            ey = up if abs(ex.z) < 0.5 else rd
            frame_box(p, m + '#vp', (a + b) / 2 + n * 0.004, ex, ey, n, max(0.02, (b - a).length - sw * 0.9), sw, 0.006)


def glacis_z(x):
    (x0, z0), (x1, z1) = C.GLACIS
    return z0 + (z1 - z0) * (x - x0) / (x1 - x0)


def big_grille(p, x0, x1, z0, z1, y, s, cell=0.36, rows=3, w=0.075, t=0.03, out=0.11):
    """A GRELHA GRANDE da saia de trás do T-14: o quadro de chapa chata e a grade de quadros (barras em pé a cada 'cell' e 'rows'
    fileiras), afastada 'out' da saia (o vão mostra a roda e a esteira atrás: lê como grade, não como chapa)."""
    yo = y + s * out
    up = (0, s, 0)
    for z in (z0, z1):
        rk.flat_bar(p, 'Metal', (x0, yo, z), (x1, yo, z), 0.09, t, up=up)
    for k in range(1, rows):
        z = z0 + (z1 - z0) * k / rows
        rk.flat_bar(p, 'Metal', (x0, yo + s * 0.01, z), (x1, yo + s * 0.01, z), w * 0.8, t, up=up)
    n = max(2, int(round(abs(x1 - x0) / cell)))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        rk.flat_bar(p, 'Metal', (x, yo + s * 0.018, z0 - 0.03), (x, yo + s * 0.018, z1 + 0.03), w, t, up=up)
    for k in range(0, n + 1, 2):                       # os suportes até o casco
        x = x0 + (x1 - x0) * k / n
        for z in (z0 + 0.04, z1 - 0.04):
            p.box('Metal#vp', x - 0.03, x + 0.03, min(y, yo), max(y, yo), z - 0.025, z + 0.025, bevel=0.0)


# ---------------------------------------------------------------------------------- casco
def hull():
    p = rk.new_part(PREFIX + '_Hull')
    low = [(3.92, C.BELLY), (C.NOSE - 0.05, 0.98), (C.NOSE - 0.05, C.SPON_Z), (C.REAR + 0.03, C.SPON_Z), (C.REAR + 0.03, 0.72),
           (-4.1, C.BELLY)]
    solid(p, 'Paint#shell', [(x, s * C.LOW_HW, z) for x, z in low for s in (-1, 1)])
    (gx0, gz0), (gx1, gz1) = C.GLACIS
    xc = C.NOSE - C.CHAMF
    pts = []
    for s in (-1, 1):
        pts += [(C.NOSE, C.NOSE_HW * s, C.SPON_Z - 0.04), (C.NOSE, C.NOSE_HW * s, gz0),
                (xc, C.UP_HW * s, C.SPON_Z - 0.04), (xc, C.UP_HW * s, glacis_z(xc)), (gx1, C.UP_HW * s, gz1),
                (C.REAR + 0.1, C.UP_HW * s, C.DECK), (C.REAR, C.UP_HW * s, C.DECK - 0.1), (C.REAR, C.UP_HW * s, C.SPON_Z - 0.04)]
    solid(p, 'Paint#shell', pts)
    for s in (-1, 1):                                  # as soldas das quinas chanfradas
        vp.weld_bead(p, 'Paint#vp', [(C.NOSE - 0.01, C.NOSE_HW * s, gz0 - 0.01), (xc + 0.01, (C.UP_HW - 0.01) * s, glacis_z(xc) - 0.01)],
                     r=0.018)
    vp.weld_bead(p, 'Paint#vp', [(C.NOSE - 0.005, -C.NOSE_HW + 0.04, gz0 - 0.01), (C.NOSE - 0.005, C.NOSE_HW - 0.04, gz0 - 0.01)], r=0.018)
    # a lâmina de autoentrincheiramento dobrada na chapa de baixo do nariz
    a = math.degrees(math.atan2(0.98 - C.BELLY, C.NOSE - 0.05 - 3.92))
    p.obox('Paint#plate', (4.15, 0.0, 0.73), (0.07, 1.8, 0.44), pitch=-(90.0 - a), bevel=0.03)
    for y in (-0.55, 0.0, 0.55):
        p.obox('Paint#plate', (4.185, y, 0.715), (0.06, 0.08, 0.38), pitch=-(90.0 - a), bevel=0.02)
    # o glacis: os 3 painéis LISOS e grandes do Malakhit (as frestas largas, os parafusos), sem tijolo (o T-90M é que tem os tijolos)
    eu = Vector((gx1 - gx0, 0.0, gz1 - gz0)).normalized()
    en = Vector((-eu.z, 0.0, eu.x)) * -1.0
    if en.z < 0:
        en = -en
    L = (Vector((gx1, 0, gz1)) - Vector((gx0, 0, gz0))).length
    rk.era_grid(p, Vector((gx0, -C.NOSE_HW + 0.04, gz0)) + eu * 0.1, eu, Vector((0, 1, 0)), en, 1, 3, L - 0.2, (2 * C.NOSE_HW - 0.08) / 3,
                0.07, gap=0.05)
    # as chapas dos chanfros (os 2 painéis triangulares viram retângulos menores nas quinas)
    for s in (-1, 1):
        xq = 3.6
        frame_box(p, 'Paint#plate', (xq, 1.25 * s, glacis_z(xq) + 0.03), eu, Vector((0, 1, 0)), en, 0.6, 0.4, 0.06)
    # os faróis nas faces chanfradas (o T-14 de verdade: as quinas da frente) e os ganchos
    for s in (-1, 1):
        nrm = Vector((C.UP_HW - C.NOSE_HW, C.CHAMF * s, 0.0)).normalized()          # a normal pra fora da face do chanfro
        q = Vector((C.NOSE, C.NOSE_HW * s, 0)) + (Vector((xc, C.UP_HW * s, 0)) - Vector((C.NOSE, C.NOSE_HW * s, 0))) * 0.55
        yaw = math.degrees(math.atan2(nrm.y, nrm.x))
        vp.rbox(p, 'Paint#plate', (q.x, q.y, C.SPON_Z + 0.08), (0.12, 0.3, 0.16), yaw=yaw, r=0.03, k=2)
        vp.rbox(p, 'Lens', (q.x + nrm.x * 0.065, q.y + nrm.y * 0.065, C.SPON_Z + 0.08), (0.012, 0.24, 0.1), yaw=yaw, r=0.03, k=2)
        p.box('Light', q.x + nrm.x * 0.07 - 0.01, q.x + nrm.x * 0.07 + 0.01, q.y - 0.05, q.y + 0.05, C.SPON_Z + 0.06, C.SPON_Z + 0.1,
              bevel=0.0)
        for xx, sg in ((C.NOSE, 1), (C.REAR - 0.02, -1)):
            for dd in (-1, 1):
                vp.rbox(p, 'Metal', (xx + 0.0 * sg, 0.7 * s + dd * 0.055, 0.92), (0.12, 0.035, 0.15), r=0.03, k=2)
            p.torus('Dark#vp', (xx + 0.05 * sg, 0.7 * s, 0.86), 'y', 0.055, 0.017, seg=14, rseg=6)
    # ---------------------------------------------------------------- a cápsula da tripulação: a chapa elevada com as 3 ESCOTILHAS
    vp.rbox(p, 'Paint#plate', (2.56, 0.0, C.DECK + 0.03), (1.08, 2.5, 0.06), r=0.12, k=3)
    vp.bolt_row(p, 'Metal', (2.08, -1.15, C.DECK + 0.06), (2.08, 1.15, C.DECK + 0.06), 10, (0, 0, 1), r=0.016, h=0.012)
    for y in (-0.78, 0.0, 0.78):
        vp.hatch_round(p, (2.45, y, C.DECK + 0.06), 0.26, 'Paint', 'Metal', hinge_dir=(-1, 0, 0), handle_m='Metal')
        for dd in (-0.13, 0.13):
            vp.rbox(p, 'Paint#plate', (2.93, y + dd, C.DECK + 0.11), (0.15, 0.2, 0.1), r=0.03, k=2)
            p.obox('Lens', (3.008, y + dd, C.DECK + 0.12), (0.012, 0.15, 0.04), bevel=0.0)
    # ---------------------------------------------------------------- as saias: 4 painéis lisos do Malakhit na frente, a GRELHA atrás
    yi, yo = C.SKIRT
    t = yo - yi
    front = [xc - 0.05, 2.35, 1.2, 0.05, -1.1]
    for s in (-1, 1):
        for k in range(4):
            xa, xb = front[k + 1] + 0.03, front[k] - 0.03
            frame_box(p, 'Paint#plate', ((xa + xb) / 2, (yi + t / 2) * s, 1.1), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, s, 0)),
                      xb - xa, 0.8, t)
            # o módulo grosso do Malakhit por fora (a borda de baixo chanfrada: a cunha que lê de lado) e os parafusos
            pts = []
            for xx in (xa + 0.04, xb - 0.04):
                pts += [(xx, (yo - 0.005) * s, 0.78), (xx, (yo - 0.005) * s, 1.46), (xx, (yo + 0.08) * s, 1.42), (xx, (yo + 0.08) * s, 0.9)]
            solid(p, 'Paint#plate', pts, bevel=0.03)
            vp.bolt_row(p, 'Metal', (xa + 0.12, (yo + 0.08) * s, 1.33), (xb - 0.12, (yo + 0.08) * s, 1.33), 4, (0, s, 0), r=0.018, h=0.012)
        vp.mudflap(p, ((xc - 0.1 + -1.1) / 2, (yi + t / 2) * s, 0.74), xc + 1.0, 0.12, yaw=90 * s, m='Rubber', t=0.022, folds=3)
        # a grelha grande atrás (do fim dos painéis até a traseira) na chapa da saia
        frame_box(p, 'Paint#plate', ((-1.13 + C.REAR + 0.1) / 2, (yi + t / 2) * s, 1.3), Vector((1, 0, 0)), Vector((0, 0, 1)),
                  Vector((0, s, 0)), -1.13 - C.REAR - 0.1, 0.4, t)
        big_grille(p, -1.15, C.REAR + 0.12, 0.74, 1.5, yo * s, s, out=0.05)
    # o forro escuro do casco de baixo atrás das rodas e os braços
    for s in (-1, 1):
        yy0, yy1 = sorted((C.LOW_HW * s, (C.LOW_HW + 0.015) * s))
        p.box('Dark', -4.0, 3.85, yy0, yy1, 0.55, C.SPON_Z - 0.05, bevel=0.0)
        for x in C.WHEELS_X:
            yy0, yy1 = sorted((C.LOW_HW * s, (C.TRACK_Y - C.TRACK_W / 2 - 0.02) * s))
            p.box('Dark', x - 0.07, x + 0.28, yy0, yy1, C.WHEEL_Z - 0.05, C.WHEEL_Z + 0.05, bevel=0.0)
    # ---------------------------------------------------------------- o deck: o anel, as juntas, as venezianas dos lados
    vp.lathe(p, 'Dark', (TX, 0.0, C.DECK), 'z', [(C.RING_R - 0.06, 0.0), (C.RING_R + 0.06, 0.0), (C.RING_R + 0.06, 0.012),
                                                (C.RING_R + 0.045, 0.02), (C.RING_R - 0.06, 0.02)], seg=64, closed=True)
    for x in (1.75, -2.8):
        vp.panel_seam(p, 'Dark', (x, -1.5, C.DECK + 0.003), (x, 1.5, C.DECK + 0.003), (0, 0, 1), w=0.024, d=0.01)
    for s in (-1, 1):
        y0, y1 = sorted((1.36 * s, 1.64 * s))
        rk.grille_deck(p, -4.15, -2.95, y0, y1, C.DECK, 8, along='y')
        # o escape do lado esquerdo (a grelha na lateral de trás do casco de cima)
    p.box('Dark', -3.9, -3.1, -C.UP_HW - 0.012, -C.UP_HW, C.SPON_Z + 0.08, C.DECK - 0.06, bevel=0.0)
    for k in range(8):
        p.obox('Metal#vp', (-3.85 + k * 0.1, -C.UP_HW - 0.015, (C.SPON_Z + C.DECK) / 2), (0.025, 0.025, 0.28), bevel=0.0)
    # ---------------------------------------------------------------- a COR DO JOGADOR no deck do motor (fora do giro da torre)
    xa, xb = DECK_REAR
    # a carga coberta: as caixas e os galões por baixo da lona (as bossas que a lona marca)
    lumps = [(-3.2, -0.6, 0.2, 0.32), (-3.25, 0.55, 0.16, 0.3), (-3.75, 0.0, 0.22, 0.38), (-3.7, -0.95, 0.12, 0.25), (-3.6, 0.95, 0.14, 0.26)]

    def tz(u, v, z):
        x = xa + (xb - xa) * u
        y = -1.28 + 2.56 * v
        h = 0.06
        for (lx, ly, hh, rr) in lumps:
            h += hh * math.exp(-((x - lx) ** 2 + (y - ly) ** 2) / rr ** 2)
        edge = min(u, 1 - u, v, 1 - v)
        h *= min(1.0, 0.25 + edge / 0.1)
        return C.DECK + 0.02 + h + 0.01 * math.sin(x * 13.0) * math.sin(y * 7.0)
    sheet(p, 'Team#cloth', [(xa, -1.28, C.DECK), (xb, -1.28, C.DECK), (xb, 1.28, C.DECK), (xa, 1.28, C.DECK)], 30, 40, z_fn=tz, thick=0.016)
    # as 3 cintas atravessadas por cima da lona, presas nos olhais das bordas
    for x in (-3.05, -3.55, -4.0):
        pts = [(x, -1.33, C.DECK + 0.02)] + [(x, -1.25 + 2.5 * k / 16, tz((x - xa) / (xb - xa), k / 16 * 0.977 + 0.012, 0) + 0.018)
                                             for k in range(17)] + [(x, 1.33, C.DECK + 0.02)]
        vp.tube(p, 'Cloth', pts, 0.016, seg=6)
        for s in (-1, 1):
            p.torus('Metal#vp', (x, 1.36 * s, C.DECK + 0.03), 'x', 0.04, 0.012, seg=10, rseg=5)
    # a rede jogada por cima da metade da frente da lona, embolada, e a lona ENROLADA presa na quina de trás

    def nz(x, y):
        u = (x - xa) / (xb - xa)
        v = (y + 1.28) / 2.56
        return tz(min(1, max(0, u)), min(1, max(0, v)), 0) + 0.03 + 0.08 * math.exp(-((x + 3.1) ** 2 + (y - 0.2) ** 2) / 0.12)
    rk.net_drape(p, -3.55, xa - 0.02, -1.22, 1.22, nz, step=0.22, r=0.017, jitter=0.06, seed=31.0)
    vp.tarp_roll(p, (C.REAR + 0.16, 0.0, C.DECK + 0.1), 'y', 2.5, 0.11, m='Team', straps=3, strap_m='Cloth', buckle_m='Metal')
    for y in (-0.8, 0.0, 0.8):
        p.box('Metal#vp', C.REAR + 0.04, C.REAR + 0.3, y - 0.03, y + 0.03, C.DECK - 0.02, C.DECK + 0.01, bevel=0.0)
    # a traseira: a grelha grande do escape (as aletas), as lanternas, o cabo de reboque (sem a grade de barras do T-90M)
    p.box('Dark', C.REAR - 0.012, C.REAR, -1.1, 1.1, 1.12, 1.4, bevel=0.0)
    for k in range(6):
        vp.rbox(p, 'Metal', (C.REAR - 0.02, 0.0, 1.15 + k * 0.045), (0.03, 2.15, 0.022), pitch=-30, r=0.008, k=1)
    for s in (-1, 1):
        vp.rbox(p, 'Dark', (C.REAR - 0.03, 1.4 * s, 1.36), (0.06, 0.16, 0.1), r=0.03, k=2)
        p.box('Lens', C.REAR - 0.07, C.REAR - 0.05, 1.4 * s - 0.05, 1.4 * s + 0.05, 1.33, 1.39, bevel=0.0)
    vp.tow_cable(p, 'Metal', [(C.REAR - 0.18, -0.95, 0.86), (C.REAR - 0.2, -0.7, 0.78), (C.REAR - 0.2, 0.7, 0.78),
                              (C.REAR - 0.18, 0.95, 0.86)], r=0.022)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def outline(z):
    """O contorno da torre (por lado) na altura z: o pé, a cintura (a mais larga) e o teto (as faces de cima caindo pra dentro)."""
    lv = [(T_Z0, [(1.62, 0.5), (0.95, 1.36), (-1.5, 1.36), (-2.08, 1.0)]),
          (T_Z1, [(1.72, 0.52), (1.0, 1.42), (-1.55, 1.42), (-2.15, 1.02)]),
          (T_Z2, [(1.12, 0.42), (0.55, 1.0), (-1.42, 1.05), (-1.92, 0.8)])]
    for (za, A), (zb, Bv) in zip(lv[:-1], lv[1:]):
        if za <= z <= zb:
            t_ = (z - za) / (zb - za)
            return [(a[0] + (b[0] - a[0]) * t_, a[1] + (b[1] - a[1]) * t_) for a, b in zip(A, Bv)]
    return lv[0][1] if z < T_Z0 else lv[-1][1]


def turret():
    p = rk.new_part(PREFIX + '_Turret')
    pts = []
    for z in (T_Z0, T_Z1, T_Z2):
        for (dx, hw) in outline(z):
            for s in (-1, 1):
                pts.append((TX + dx, hw * s, z))
    solid(p, 'Paint#shell', pts, bevel=0.05)
    vp.lathe(p, 'Paint', (TX, 0.0, C.DECK), 'z', [(0.0, 0.03), (1.08, 0.03), (1.08, 0.055), (1.03, 0.065), (0.0, 0.065)], seg=56)
    # as soldas nas arestas da cintura (a linha dura que corre a torre) e as juntas do teto
    ring = [(TX + dx, hw, T_Z1 + 0.004) for (dx, hw) in outline(T_Z1)]
    for s in (-1, 1):
        vp.weld_bead(p, 'Paint#vp', [(x, y * s, z) for (x, y, z) in ring], r=0.02)
    vp.weld_bead(p, 'Paint#vp', [(TX + 1.72, -0.52, T_Z1 + 0.004), (TX + 1.72, 0.52, T_Z1 + 0.004)], r=0.02)
    for x in (TX + 0.15, TX - 0.85):
        hw = 0.99 if x > TX - 0.5 else 1.03
        vp.panel_seam(p, 'Dark', (x, -hw, T_Z2 + 0.002), (x, hw, T_Z2 + 0.002), (0, 0, 1), w=0.026, d=0.012)
    vp.panel_seam(p, 'Dark', (TX + 0.15, 0.0, T_Z2 + 0.002), (TX - 0.85, 0.0, T_Z2 + 0.002), (0, 0, 1), w=0.026, d=0.012)
    # o mantelete estreito e recuado na ponta (a fenda do canhão) e a coaxial à direita
    vp.rbox(p, 'Paint#plate', (TX + 1.62, 0.0, GUN_Z), (0.5, 0.56, 0.44), r=0.07, k=3)
    vp.lathe(p, 'Dark', (TX + 1.87, 0.22, GUN_Z + 0.1), 'x', [(0.0, 0.0), (0.035, 0.0), (0.035, 0.04), (0.0, 0.04)], seg=10)
    # ---------------------------------------------------------------- o Afganit: os 4 painéis do radar nas quinas, os tubos do pé
    for s in (-1, 1):
        for (u, which) in ((0.5, 'front'), (0.55, 'rear')):
            o = outline(T_Z1 - 0.18)
            if which == 'front':
                A, Bp = Vector((TX + o[0][0], o[0][1] * s, 0)), Vector((TX + o[1][0], o[1][1] * s, 0))
            else:
                A, Bp = Vector((TX + o[2][0], o[2][1] * s, 0)), Vector((TX + o[3][0], o[3][1] * s, 0))
            q = A + (Bp - A) * u
            d = (Bp - A).normalized()
            nrm = Vector((d.y, -d.x, 0.0))
            if nrm.dot(Vector((q.x - TX, q.y, 0))) < 0:
                nrm = -nrm
            zc = T_Z1 - 0.2
            frame_box(p, 'Paint#plate', (q.x + nrm.x * 0.03, q.y + nrm.y * 0.03, zc), d, Vector((0, 0, 1)), nrm, 0.56, 0.36, 0.06)
            frame_box(p, 'Dark', (q.x + nrm.x * 0.065, q.y + nrm.y * 0.065, zc), d, Vector((0, 0, 1)), nrm, 0.46, 0.27, 0.012)
            for k in range(3):                                         # a grade fina das antenas na face (lê como sensor, não como chapa)
                frame_box(p, 'Metal#vp', (q.x + nrm.x * 0.072, q.y + nrm.y * 0.072, zc - 0.09 + k * 0.09), d, Vector((0, 0, 1)), nrm,
                          0.44, 0.02, 0.008)
        # os tubos de lançar do Afganit (o pé da torre, de lado, inclinados pra fora e pra cima)
        for k in range(4):
            x = TX + 0.85 - k * 0.38
            vp.lathe(p, 'Metal', (x, 1.37 * s, T_Z0 + 0.1), (0, s * 0.9, 0.45), [(0.0, 0.0), (0.07, 0.0), (0.07, 0.24), (0.06, 0.26),
                                                                                 (0.0, 0.26)], seg=12)
            p.cyl('Dark#vp', (x, 1.37 * s + s * 0.9 * 0.24 / 1.006, T_Z0 + 0.1 + 0.45 * 0.24 / 1.006), (0, s * 0.9, 0.45), 0.05, 0.01,
                  seg=12, bevel=0.0)
        # o número tático na lateral (a face de baixo, quase em pé)
        # (revisão) a face de baixo da torre abre pra fora 7,6° (1,36 m no pé, 1,42 na cintura): o número e a estrela seguem a face
        lean = (outline(T_Z1)[2][1] - outline(T_Z0)[2][1]) / (T_Z1 - T_Z0)
        nf = (0.0, s, -lean)
        tac_number(p, (TX - 0.95, (outline(1.84)[2][1] + 0.002) * s, 1.84), nf, '301', h=0.2)
        rk.star_outlined(p, (TX - 1.36, outline(1.84)[2][1] * s, 1.84), nf, r=0.1)
        # as alças de mão nas faces de cima
        vp.grab_handle(p, 'Metal', (TX - 1.2, 1.2 * s, T_Z1 + 0.3), (TX - 0.7, 1.2 * s, T_Z1 + 0.3), (0, s, 0.55), height=0.05, r=0.014)
    # ---------------------------------------------------------------- o teto: as miras, a estação remota, a caixa da defesa ativa
    top = T_Z2
    gx, gy = TX + 0.72, -0.55
    vp.rbox(p, 'Paint#plate', (gx, gy, top + 0.19), (0.5, 0.44, 0.38), r=0.06, k=3)
    vp.rbox(p, 'Paint#thin', (gx - 0.02, gy, top + 0.39), (0.54, 0.48, 0.03), r=0.06, k=2)
    vp.rbox(p, 'Dark', (gx + 0.25, gy, top + 0.22), (0.02, 0.36, 0.2), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.26, gy - 0.07, top + 0.22), (0.012, 0.16, 0.13), r=0.03, k=2)
    vp.rbox(p, 'Lens', (gx + 0.26, gy + 0.1, top + 0.22), (0.012, 0.1, 0.1), r=0.03, k=2)
    for sg in (-1, 1):
        p.obox('Paint#thin', (gx + 0.28, gy + sg * 0.23, top + 0.22), (0.15, 0.025, 0.24), yaw=sg * 35, bevel=0.0)
    # a panorâmica do comandante no MASTRO ALTO (direita)
    cx, cy = TX + 0.25, 0.62
    vp.lathe(p, 'Paint', (cx, cy, top), 'z', [(0.0, 0.0), (0.22, 0.0), (0.22, 0.05), (0.13, 0.09), (0.1, 0.56), (0.13, 0.6), (0.0, 0.6)],
             seg=18)
    vp.rbox(p, 'Paint#plate', (cx, cy, top + 0.74), (0.44, 0.38, 0.28), r=0.07, k=3)
    vp.rbox(p, 'Paint#thin', (cx, cy, top + 0.895), (0.48, 0.42, 0.03), r=0.07, k=2)
    vp.rbox(p, 'Lens', (cx + 0.22, cy, top + 0.75), (0.012, 0.26, 0.14), r=0.03, k=2)
    vp.rbox(p, 'Lens', (cx - 0.22, cy, top + 0.75), (0.012, 0.2, 0.12), r=0.03, k=2)
    # a estação remota pequena (a PKTM) no meio
    rk.kord_rws(p, (TX - 0.35, -0.35, top), yaw=0.0, k=0.8)
    # a CAIXA DA DEFESA ATIVA atrás no teto: o caixote blindado chanfrado com os 2 lançadores giratórios (6 tubos cada) por cima
    bx = TX - 1.4
    pts = []
    for s in (-1, 1):
        pts += [(bx + 0.42, 0.72 * s, top - 0.02), (bx - 0.4, 0.66 * s, top - 0.02), (bx + 0.36, 0.66 * s, top + 0.32),
                (bx - 0.34, 0.6 * s, top + 0.32), (bx + 0.42, 0.62 * s, top + 0.24), (bx - 0.4, 0.56 * s, top + 0.24)]
    solid(p, 'Paint#shell', pts, bevel=0.04)
    vp.panel_seam(p, 'Dark', (bx, -0.58, top + 0.322), (bx, 0.58, top + 0.322), (0, 0, 1), w=0.022, d=0.01)
    for s in (-1, 1):
        lx, ly, lz = bx, 0.33 * s, top + 0.32
        vp.lathe(p, 'Metal', (lx, ly, lz), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.17, 0.07), (0.0, 0.07)], seg=20)
        ex, ey, ez = vp.ypr_frame(90 * s, 35.0, 0.0)
        base = Vector((lx, ly, lz + 0.16))
        frame_box(p, 'Paint#plate', base - ex * 0.02, ex, ey, ez, 0.34, 0.42, 0.24)
        for i in range(3):
            for j in range(2):
                q = base + ex * 0.17 + ey * ((i - 1) * 0.13) + ez * ((j - 0.5) * 0.11)
                p.cyl('Dark#vp', tuple(q), tuple(ex), 0.045, 0.012, seg=12, bevel=0.0)
    for (ax, ay, hh) in ((bx - 0.25, 0.5, 1.7), (bx - 0.25, -0.5, 1.4)):
        vp.lathe(p, 'Dark', (ax, ay, top + 0.32), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.05), (0.04, 0.07), (0.035, 0.16), (0.0, 0.17)],
                 seg=12)
        p.cyl('Dark#vp', (ax, ay, top + 0.52 + hh / 2), 'z', 0.009, hh, seg=6, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- canhão
def barrel():
    """O 2A82-1M de 125 mm: a capa quadrada da raiz (a do T-14), o cano LISO e comprido sem o evacuador (a torre é sem tripulação), a
    capa térmica com as braçadeiras, a referência da boca."""
    p = rk.new_part(PREFIX + '_Barrel')
    x0 = MANTLET_X
    vp.rbox(p, 'Paint#plate', (x0 + 0.32, 0.0, GUN_Z), (0.7, 0.38, 0.38), r=0.08, k=3)
    vp.rbox(p, 'Paint#plate', (x0 + 0.68, 0.0, GUN_Z), (0.06, 0.34, 0.34), r=0.08, k=3)
    st = [(x0 + 0.6, 0.16), (x0 + 0.72, 0.16), (x0 + 0.76, 0.135), (x0 + 1.5, 0.13), (x0 + 1.54, 0.125), (4.6, 0.12), (4.64, 0.116),
          (6.25, 0.11), (6.29, 0.115), (MUZZLE_X - 0.04, 0.115), (MUZZLE_X, 0.107)]
    vp.gun_barrel(p, st, GUN_Z, m='Paint', seg=32, bore=0.0625, bore_depth=0.4, bore_m='Dark')
    for xx, r in ((x0 + 1.15, 0.131), (3.6, 0.122), (4.6, 0.12), (5.5, 0.113)):
        vp.ring_band(p, 'Metal', xx - 0.03, xx + 0.03, r, GUN_Z, lip=0.009, seg=32)
        vp.rbox(p, 'Metal', (xx, 0.0, GUN_Z - r - 0.013), (0.05, 0.05, 0.03), r=0.012, k=1)
    vp.rbox(p, 'Metal', (MUZZLE_X - 0.24, 0.0, GUN_Z + 0.155), (0.13, 0.08, 0.06), r=0.015, k=2)
    p.box('Metal', MUZZLE_X - 0.27, MUZZLE_X - 0.21, -0.02, 0.02, GUN_Z + 0.1, GUN_Z + 0.13, bevel=0.0)
    return p.finish()


def wheels():
    p = rk.new_part(PREFIX + '_Wheels')
    for s in (-1, 1):
        y = C.TRACK_Y * s
        for x in C.WHEELS_X:
            rk.ribbed_wheel(p, (x, y, C.WHEEL_Z), C.WHEEL_R, 0.6, s, 'Paint', ribs=6)
        ix, iz, ir = C.IDLER
        vp.idler(p, (ix, y, iz), ir, 0.54, s, 'Paint', 'Metal', gap=0.1)
        sx, sz, sr = C.SPROCKET
        vp.sprocket(p, (sx, y, sz), sr, 0.56, s, 'Paint', 'Metal', teeth=12, holes_m='Dark')
        for x in C.ROLLERS_X:
            vp.return_roller(p, (x, y, C.ROLLER_Z), C.ROLLER_R, 0.3, 'Metal', 'Rubber')
    return p.finish()


def main():
    clear_scene()
    h, t, b, w = hull(), turret(), barrel(), wheels()
    tl, tr = rk.t72_track(PREFIX + '_Track_l', 'l', C=C, pitch=0.16), rk.t72_track(PREFIX + '_Track_r', 'r', C=C, pitch=0.16)
    sweep_check([h], [t, b], (TX, 0.0))
    total = 0
    for o in (h, t, b, w, tl, tr):
        total += export(o, OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (h, t, b, w, tl, tr)})
    (ax0, ax1), (ay0, ay1), (az0, az1) = bounds([h, t, b, w, tl, tr])
    (hx0, hx1), (hy0, hy1), _ = bounds([h, w, tl, tr])
    (tx0, tx1), (ty0, ty1), (tz0, tz1) = bounds([t])
    print('MODEL medidas: comprimento %.2f m com o cano (x %.2f..%.2f), casco %.2f x %.2f m (x %.2f..%.2f), altura %.2f m, deck %.2f, '
          'teto da torre %.2f' % (ax1 - ax0, ax0, ax1, hx1 - hx0, hy1 - hy0, hx0, hx1, az1, C.DECK, T_Z2))
    print('MODEL torre: x %.2f..%.2f, y %.2f..%.2f, z %.2f..%.2f' % (tx0, tx1, ty0, ty1, tz0, tz1))
    print('MODEL turret_pivot_m', [TX, 0.0], 'muzzle_height_m', GUN_Z, 'a boca em x', MUZZLE_X, 'tracks_m',
          [round(2 * C.TRACK_Y, 3), C.TRACK_W])
    rk.write_spec(OUT, [(PREFIX + n, (0, 0, 0)) for n in ('_Hull', '_Turret', '_Barrel', '_Wheels', '_Track_l', '_Track_r')])


main()
