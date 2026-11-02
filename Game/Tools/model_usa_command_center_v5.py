"""O Command Center da USA (usa_command_center), VERSÃO 5 (01/11/2026): o desenho ORIGINAL, a gramática do Zero Hour com as formas nossas.

Por quê: o Felipe aprovou a FORMA icônica da v4 (o ZH quase copiado) e pediu o nosso: "agora obviamente você vai fazer um modelo novo
pra gente do zero, porque não se pode copiar de outro jogo". O desenho: docs/elenco/USA_CC_E_USINA.md §6. A régua do acabamento: as
aprovadas da USA (model_usa_war_factory_v2.py e a ficha docs/concept/buildings/usa_war_factory_sheet.webp, model_usa_supply_center_v2.py,
model_usa_barracks_v2.py) e docs/ESTUDO_DETALHE_AAA.md (chapas com fresta, quinas chanfradas, parafusos em ritmo, sombra de contato).
Da v4 vêm só a interface (o lote, as 3 peças, os slots, o radar que gira, a bandeira pelo flags.json) e o ferramental genérico de
parede/porta/janela/cerca (importado de model_usa_command_center_v4.py); nenhuma forma.

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_command_center_v5.py C:/Users/luisf/explosion/captures/game/models/usa_command_center_v5

A GRAMÁTICA DO ZH (o que ele ensina) e O NOSSO (o que muda), peça por peça:
  - o elemento ALTO que GIRA: no ZH a torre de treliça quadrada com o prato redondo; aqui o MASTRO TUBULAR de comunicação militar
    (o tubo cinza em 3 lances com as flanges, os anéis de estai, 2 plataformas de serviço em C, a escada com gaiola, a bandeja de
    cabo), ESTAIADO em 2 níveis até 3 blocos de ancoragem (os cabos com a capa amarela de proteção no pé), e no topo a ANTENA DE
    RADAR RETANGULAR de painel (o arranjo plano inclinado, as faixas dos guias de onda, a barra do IFF em cima: a família do
    AN/TPS-75), a peça _Radar: de cima, o retângulo que varre, não o disco. Na quina da frente do lado −Y (no ZH a torre fica na quina
    da frente do outro lado);
  - o salão com o teto que NÃO é caixa: no ZH o galpão comprido com a abóbada em gomos; aqui o SALÃO DE COMANDO baixo e largo com o
    TETO EM SERRA (5 dentes: as águas de chapa e as claraboias escuras inclinadas viradas pra frente, a calha em cada vale), a faixa
    azul no beiral e, na quina da frente (+X, +Y), a SALA DE OPERAÇÕES envidraçada em balanço (o vidro inclinado pra fora em 3 lados,
    o de torre de controle, a faixa acesa do forro por dentro, as mãos-francesas de aço e as luzes embaixo do balanço sobre a porta);
  - o volume da SAÍDA com o emblema grande: no ZH o hangar meia-lua com a estrela no flanco; aqui o PAVILHÃO de chapas de duas águas
    (a cumeeira ao longo de X, o lanternim de ventilação na cumeeira, as chapas com a junta descendo as águas) com o PORTÃO SECCIONAL
    alto em +X (aberto: a última folha recolhida no alto do vão, a soleira e os batentes de perigo) e, na EMPENA acima do portão (o
    oitão que sobe acima do telhado com a capa escura), o grande DISCO AZUL (a cor do jogador) com o aro e a ESTRELA BRANCA do
    Exército; o semáforo da saída ao lado;
  - o BLOCO DE RÁDIO: no ZH o bloco elevado em cima do galpão; aqui a CASA DE EQUIPAMENTOS separada (o abrigo de comunicação) com o
    FEIXE de antenas de chicote no tripé do teto e a antena log-periódica (a espinha de peixe), os 2 ar-condicionados de parede
    virados pra frente, o GERADOR encostado com o tanque de diesel, e a calha de cabos aérea (a ponte) até o pé do mastro;
  - a BANDEIRA no mastro na frente do salão, com o anel de sacos de areia; a CERCA de alambrado de um lado só (−Y); os HESCO no canto
    do gerador e os sacos no pé; o chão SÓ no contorno (nunca a laje do lote inteiro: regra do Felipe);
  - assimétrico, com o vão entre as peças: em planta, o salão (+Y) | o pavilhão (centro) | o mastro e o rádio (−Y), em 3 alturas
    (5,9 / 9,3 / 16 m).
A USA limpa (13/10): sem desgaste pintado no modelo (vem do material do jogo, pelas juntas, chapas e máscaras daqui).

O lote 25,28 × 29,52 m (x −12,64..12,64, y −14,76..14,76; Units.json usa_command_center), a frente em +X. A SAÍDA: o portão do pavilhão na
face +X (x = 4,4), centrado em y = 0, o vão de 6,8 m (6,6 m livres entre as guias), 4,45 m livres até a folha recolhida; a faixa de saída
sem nada até a borda do lote (o pátio até x = 9,6, depois o chão). Alturas: o beiral do salão 4,5 (os dentes 5,8), o beiral do pavilhão
5,4, a cumeeira 8,7, o oitão 9,3, a sala de operações 7,85, a plataforma do mastro 11,8, o eixo do radar 12,6 (spin at_m), o topo do
radar ~16,0 (a regra: o mastro até ~16 m).
Peças: SM_USA_CommandCenter_Base, SM_USA_CommandCenter_Radar (origem no eixo de giro: spin at_m = RADAR_AT), SM_USA_CommandCenter_Scaffold.
Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime; aqui também o saco de areia e
a terra do HESCO).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import CPart, check, preview_spec, slab_tiles  # noqa: E402
# o ferramental genérico da v4 (as placas com a junta funda, o rodapé, a porta, a janela, o guarda-corpo, a cerca, o andaime): só a
# ferramenta; as formas daqui são todas novas
from model_usa_command_center_v4 import (axes, bar, base_run, bx, clad, door, fan_box, fence, lamp_head, louvers,  # noqa: E402
                                         panel_wall, paint, parapet_run, pipe, rail, roof_lines, scaffold_runs, solid, star_pts,
                                         wall_lamp, window, wpt)

PREFIX = 'SM_USA_CommandCenter'
COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62),
               'Rubber': (32, 32, 31)})

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)

# ---------------------------------------------------------------------------------- as medidas
HX, HY = 12.64, 14.76            # o lote 25,28 × 29,52
Z0 = 0.14                        # o topo das lajes do contorno (o rodapé da v4 usa o mesmo Z0 e BASE_T = 0,5)
BASE_T = 0.5
ZW = Z0 + BASE_T + 0.01          # onde as placas das paredes começam (em cima do rodapé)
# o pavilhão da saída (duas águas, a cumeeira ao longo de X)
PX0, PX1 = -8.2, 4.4             # os fundos e a face do portão
PW = 4.9                         # a meia-largura (as paredes em y = ±4,9)
PE = 5.4                         # o beiral
KR = math.tan(math.radians(34.0))  # a inclinação das águas
PR = PE + PW * KR                # a cumeeira (na linha das paredes) = 8,70
PAR = 0.6                        # o oitão da frente sobe 0,6 m acima do telhado (o topo 9,3)
OV = 0.35                        # o beiral em balanço
DOOR_HW, JAMB = 3.4, 0.38        # o vão do portão (±3,4 m) e a largura dos batentes
DOOR_TOP = Z0 + 5.0              # o topo do vão (5,14)
DOOR_LO = Z0 + 4.45              # a borda de baixo da folha recolhida (4,45 m livres)
CAV_X0, CAV_HW, CAV_TOP = PX1 - 6.6, 3.55, Z0 + 5.16   # o vão escuro por dentro (6,6 m de fundo)
PB = (PE - 0.6, PE)              # a faixa azul do beiral (a fileira de cima das placas)
PZR = [Z0 + 1.9, Z0 + 3.5, PB[0]]
DISC = (PX1 + 0.08, 0.0, 7.1)    # o disco do emblema na empena (o centro) e o raio
DISC_R = 1.5
PIL_P = [PX0 + 3.15 * k for k in (1, 2, 3)]       # os pilares do pórtico nas laterais
# o salão de comando (o teto em serra)
HX0, HX1, HY0, HY1 = -11.6, 3.6, 6.3, 13.8
HE = 4.5                         # o beiral (o teto plano)
HB = (HE - 0.55, HE)
HZR = [Z0 + 1.9, Z0 + 3.2, HB[0]]
SX0, SX1, NT = HX0 + 0.35, -0.45, 5                  # os dentes da serra: de x SX0 a SX1, 5 dentes
SP = (SX1 - SX0) / NT            # o passo (2,16 m)
ST, SG = 1.3, 0.58               # a altura do dente e o avanço da claraboia (inclinada 66°)
TY0, TY1 = HY0 + 0.56, HY1 - 0.56  # os dentes dentro do parapeito (6,38 m: a grade do detalhe corta ao meio acima de 0,4·2^k)
HDOOR_Y = 11.7                   # a porta do salão (embaixo do balanço)
# a sala de operações em balanço na quina (+X, +Y)
OX0, OX1, OY0, OY1 = 0.95, 6.3, 9.5, 13.9
OZ0, OZS, OZG, OZT = 4.42, 4.95, 5.35, 7.55          # o fundo da laje, o piso, o pé do vidro, o topo do vidro
OTL = 0.3                        # o vidro inclinado pra fora (o topo 0,3 m pra fora em +X, −Y e +Y)
# o mastro do radar (na quina da frente do lado −Y)
MC = (8.4, -10.2)
MR = (0.34, 0.3, 0.26)           # os raios dos 3 lances do tubo (as flanges em 4,6 e 8,6 m)
MZ_DECK = 11.8                   # a plataforma de cima
RADAR_AT = (MC[0], MC[1], 12.6)  # o eixo de giro do radar (spin at_m)
GUY_R, GUY_A, GUY_Z = 3.9, (25.0, 140.0, 265.0), (5.6, 10.8)   # os estais: o raio das âncoras, os ângulos, as 2 alturas
# a casa de rádio, o gerador
RB = (-3.0, 2.2, -11.8, -8.2)    # x0, x1, y0, y1
RBH = 3.3                        # o teto
GEN = (-7.4, -4.2, -10.9, -9.3)
FLAG_AT = (7.6, 6.6)

FIRE_POINTS = [(-5.4, 10.0, HE + 0.75), (-2.0, 2.0, round(PR - 2.0 * KR + 0.15, 2)), (-0.4, -10.6, RBH + 0.1)]


# ---------------------------------------------------------------------------------- ajudantes novos
def poly_plate(p, m, pts, O, eu, ev, en, t0, t1, open_sides=()):
    """O polígono plano pts [(u, v)] no plano (O, eu, ev), engrossado de t0 a t1 ao longo de en (a placa recortada, o prisma do miolo);
    open_sides = os lados (o lado i liga pts[i] a pts[i+1]) que não levam face (o fundo escondido)."""
    b = p.bm(m, ub._bev(m))
    O, eu, ev, en = Vector(O), Vector(eu), Vector(ev), Vector(en)
    lo = [b.verts.new(V(*(O + eu * u + ev * v + en * t0))) for u, v in pts]
    hi = [b.verts.new(V(*(O + eu * u + ev * v + en * t1))) for u, v in pts]
    b.faces.new(lo)
    b.faces.new(hi[::-1])
    n = len(pts)
    for i in range(n):
        if i in open_sides:
            continue
        k = (i + 1) % n
        b.faces.new([lo[i], lo[k], hi[k], hi[i]])


def core(p, x0, x1, y0, y1, z0, z1, skip=(), top=True, m='Body#vp'):
    """O miolo escondido atrás das placas: a caixa sem o fundo, sem os lados de 'skip' e (top=False) sem a tampa que fica debaixo de
    outro miolo. (A grade do executor corta toda aresta > 0,4 m ao meio: face escondida também custa triângulo.)"""
    x0, x1, y0, y1 = min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1)
    b = p.bm(m, 0.0)
    vs = {(i, j, k): b.verts.new(V((x0, x1)[i], (y0, y1)[j], (z0, z1)[k])) for i in (0, 1) for j in (0, 1) for k in (0, 1)}
    q = lambda *ks: [vs[t] for t in ks]
    if top:
        b.faces.new(q((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)))
    sides = {'-y': q((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)), '+y': q((1, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 1)),
             '-x': q((0, 1, 0), (0, 0, 0), (0, 0, 1), (0, 1, 1)), '+x': q((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1))}
    for k, f_ in sides.items():
        if k not in skip:
            b.faces.new(f_)


def quad_slab(p, m, q, nrm, t0, t1):
    """O quadrilátero q (4 pontos no plano dele) engrossado de t0 a t1 ao longo de nrm (o vidro, a faixa acesa)."""
    b = p.bm(m, ub._bev(m))
    N = Vector(nrm).normalized()
    lo = [b.verts.new(V(*(Vector(v) + N * t0))) for v in q]
    hi = [b.verts.new(V(*(Vector(v) + N * t1))) for v in q]
    b.faces.new(lo)
    b.faces.new(hi[::-1])
    for i in range(4):
        k = (i + 1) % 4
        b.faces.new([lo[i], lo[k], hi[k], hi[i]])


def tent_cell(a, b, c, d, tent):
    """O retângulo (a..b × c..d) cortado por cima pela tenda v = A − k·|u − uc| (o caimento da empena): o polígono [(u, v)] ou None."""
    if tent is None:
        return [(a, c), (b, c), (b, d), (a, d)]
    uc, A, k = tent
    f = lambda u: A - k * abs(u - uc)
    cand = {a, b}
    if a < uc < b:
        cand.add(uc)
    for L in (c, d):
        if A > L:
            for u in (uc - (A - L) / k, uc + (A - L) / k):
                if a < u < b:
                    cand.add(u)
    us = sorted(u for u in cand if f(u) >= c - 1e-6)
    if len(us) < 2 or us[-1] - us[0] < 0.25:
        return None
    pts = [(us[0], c), (us[-1], c)] + [(u, min(d, f(u))) for u in reversed(us)]
    out = []
    for q in pts:
        if not out or abs(q[0] - out[-1][0]) > 1e-4 or abs(q[1] - out[-1][1]) > 1e-4:
            out.append(q)
    if abs(out[0][0] - out[-1][0]) < 1e-4 and abs(out[0][1] - out[-1][1]) < 1e-4:
        out.pop()
    area = 0.5 * sum(out[i][0] * out[(i + 1) % len(out)][1] - out[(i + 1) % len(out)][0] * out[i][1] for i in range(len(out)))
    return out if len(out) >= 3 and area > 0.08 else None


def face_clad(p, O, eu, ev, en, us, vs, tent=None, holes=(), band=None, m='Body#pnl', gap=0.07, proud=0.08, embed=0.04, min_piece=0.28):
    """As placas de uma face plana com a junta funda (a da War Factory B), em colunas us e fileiras vs (no plano: u ao longo de eu, v ao
    longo de ev, a partir de O), o topo cortado pela tenda (a empena) e desviando dos vãos holes [(u0, u1, v0, v1)]; band = (v0, v1) a
    fileira na cor do jogador."""
    g = gap / 2
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            a = us[i] + (g if i > 0 else 0.0)
            b = us[i + 1] - (g if i < len(us) - 2 else 0.0)
            c = vs[j] + (g if j > 0 else 0.0)
            d = vs[j + 1] - (g if j < len(vs) - 2 else 0.0)
            mm = 'Player#pnl' if band and abs(vs[j] - band[0]) < 1e-6 else m
            pieces = ub._rect_minus(a, b, c, d, holes, g)
            for (a2, b2, c2, d2) in pieces:
                if len(pieces) > 1:
                    a2, b2 = a2 + (g if a2 > a + 1e-6 else 0.0), b2 - (g if b2 < b - 1e-6 else 0.0)
                    c2, d2 = c2 + (g if c2 > c + 1e-6 else 0.0), d2 - (g if d2 < d - 1e-6 else 0.0)
                if b2 - a2 < min_piece or d2 - c2 < min_piece:
                    continue
                pts = tent_cell(a2, b2, c2, d2, tent)
                if pts:
                    poly_plate(p, mm, pts, O, eu, ev, en, -embed, proud)


def arc_slab(p, m, c, r0, r1, a0, a1, z0, z1, n=None):
    """A coroa (r0..r1) de a0 a a1 graus, de z0 a z1: o piso da plataforma em C (o vão é a passagem da escada)."""
    n = n or max(4, int(abs(a1 - a0) / 10))
    b = p.bm(m, ub._bev(m))
    rings = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        ca, sa = math.cos(a), math.sin(a)
        rings.append([b.verts.new(V(c[0] + r * ca, c[1] + r * sa, z)) for (r, z) in ((r0, z0), (r1, z0), (r1, z1), (r0, z1))])
    for i in range(n):
        for e in range(4):
            b.faces.new([rings[i][e], rings[i][(e + 1) % 4], rings[i + 1][(e + 1) % 4], rings[i + 1][e]])
    b.faces.new(rings[0][::-1])
    b.faces.new(rings[-1])


def circle(R, n=48):
    return [(R * math.cos(2 * math.pi * k / n), R * math.sin(2 * math.pi * k / n)) for k in range(n)]


def pilasters(p, axis, at, s, us, top):
    """Os pilares de pé na face (o ritmo do pórtico): o fuste de placa e o pé escuro (os da War Factory B)."""
    eu, en = axes(axis, s)
    for u in us:
        ub.obox(p, 'Body#pil', wpt(axis, at, s, u, (Z0 + top) / 2, 0.11), (0.5, top - Z0, 0.26), eu, Z, en)
        ub.obox(p, 'Trim#pil', wpt(axis, at, s, u, Z0 + 0.42, 0.17), (0.66, 0.9, 0.38), eu, Z, en)


def corner_pils(p, items):
    """Os pilares das quinas [(x, y, sx, sy, topo)] (cobrem as pontas das placas)."""
    for (x, y, sx, sy, z1) in items:
        bx(p, 'Body#pil', x - 0.32 + sx * 0.06, x + 0.32 + sx * 0.06, y - 0.32 + sy * 0.06, y + 0.32 + sy * 0.06, Z0 - 0.02, z1)
        bx(p, 'Trim#pil', x - 0.4 + sx * 0.06, x + 0.4 + sx * 0.06, y - 0.4 + sy * 0.06, y + 0.4 + sy * 0.06, Z0 - 0.02, Z0 + 0.9)


def sandbag(p, c, yaw, L=0.62, W=0.34, H=0.17):
    """O saco de areia: o travesseiro de quinas redondas (o corpo e o lombo em 2 degraus; sem chanfro: o chanfro custava ~380
    triângulos por saco na grade do detalhe)."""
    vp.rbox(p, 'Crate#vp', (c[0], c[1], c[2] + H * 0.3), (L, W, H * 0.6), yaw=yaw, r=0.1, k=2)
    vp.rbox(p, 'Crate#vp', (c[0], c[1], c[2] + H * 0.8), (L - 0.07, W - 0.07, H * 0.4), yaw=yaw, r=0.09, k=2)


def hesco(p, c, yaw=0.0, s=1.06, h=1.1):
    """O HESCO (o gabião de tela com a manta, cheio de terra): o corpo de manta, a tela de aço (as barras de 4,5 cm, a regra do estudo:
    menos que 4 cm vira moiré), o aro de cima e a terra."""
    a = math.radians(yaw)
    ex, ey = Vector((math.cos(a), math.sin(a), 0.0)), Vector((-math.sin(a), math.cos(a), 0.0))
    C = Vector((c[0], c[1], -0.05 + h / 2))
    ub.obox(p, 'Concrete#sm', C, (s - 0.03, s - 0.03, h), ex, ey, Vector(Z))
    for (e, f) in ((ex, ey), (-ex, ey), (ey, ex), (-ey, ex)):
        F = C + e * (s / 2 - 0.005)
        for t in (-1 / 6, 1 / 6):
            ub.obox(p, 'Trim#vp', F + f * (s * t), (0.02, 0.045, h - 0.04), e, f, Vector(Z))
        ub.obox(p, 'Trim#vp', F + Vector((0, 0, h / 2 - 0.03)), (0.03, s, 0.06), e, f, Vector(Z))
    ub.obox(p, 'Crate#sm', C + Vector((0, 0, h / 2 - 0.02)), (s - 0.14, s - 0.14, 0.08), ex, ey, Vector(Z))


# ---------------------------------------------------------------------------------- o chão: o contorno, as marcas
def ground(p):
    """SÓ o contorno de concreto em volta das peças (nunca a laje do lote): o pavilhão e o pátio de saída, o salão e a frente da sala de
    operações (a bandeira), o pátio do rádio e do gerador, os fundos. O mastro, as âncoras, a carga e os HESCO ficam no chão."""
    # (as lajes entram 15 cm debaixo das paredes e param ali: o miolo debaixo dos prédios não leva laje, que a grade do detalhe
    # cobrava sem ninguém ver; as lajes de até 3,2 m pela mesma razão)
    pads = [
        (-9.1, 4.22, -5.8, -4.75, 5, 1),         # o lado −Y do pavilhão
        (-9.1, 3.39, 4.75, 6.45, 5, 1),          # a passagem entre o pavilhão e o salão
        (-9.1, -8.05, -4.69, 4.69, 1, 4),        # os fundos do pavilhão
        (4.28, 9.6, -5.8, 4.7, 2, 4),            # o pátio de saída
        (3.45, 9.0, 4.76, 14.45, 2, 4),          # a frente do salão, a sala de operações e a bandeira
        (-12.3, -11.45, 5.41, 14.45, 1, 3),      # os fundos do salão
        (-11.39, -9.16, 5.41, 6.45, 1, 1),       # o lado −Y do salão (o pedaço dos fundos)
        (-11.39, 3.39, 13.65, 14.45, 5, 1),      # o lado +Y do salão
        (-9.8, 3.0, -12.6, -7.0, 4, 2),          # o pátio do rádio e do gerador
    ]
    for (x0, x1, y0, y1, nx, ny) in pads:
        slab_tiles(p, x0, x1, y0, y1, nx, ny, z0=-0.12, z1=Z0, joint=0.06)
    # a faixa de saída: as bordas amarelas da faixa de 6,6 m, o eixo tracejado, a soleira de perigo no portão
    for y in (-3.3, 3.3):
        paint(p, 'Yellow', PX1 + 0.45, 9.4, y - 0.08, y + 0.08)
    x = PX1 + 0.9
    while x + 0.9 < 9.4:
        paint(p, 'Yellow', x, x + 0.9, -0.07, 0.07)
        x += 1.6
    paint(p, 'Hazard', PX1 + 0.32, PX1 + 0.62, -DOOR_HW + 0.05, DOOR_HW - 0.05)
    # dentro do pavilhão: o piso escuro (o vão lê escuro) e o eixo
    for (a, b) in ((-CAV_HW - 0.05, 0.0), (0.0, CAV_HW + 0.05)):
        bx(p, 'Trim#vp', CAV_X0 - 0.02, PX1 - 0.04, a, b, -0.12, Z0 + 0.004)
    x = CAV_X0 + 0.6
    while x + 0.9 < PX1 - 0.5:
        paint(p, 'Yellow', x, x + 0.9, -0.07, 0.07)
        x += 1.6
    # a calçada da sala de operações: a faixa de pedestre até a porta
    for k in range(5):
        yy = 10.95 + k * 0.38
        paint(p, 'Paint', HX1 + 0.6, HX1 + 1.9, yy, yy + 0.2)


# ---------------------------------------------------------------------------------- o pavilhão da saída
def pavilion(p):
    """O pavilhão de duas águas: o miolo com o vão do portão, o prisma do telhado e o oitão da frente; as placas das 4 faces (a empena
    recortada pelo caimento), a faixa azul do beiral, os pilares do pórtico, o rodapé, as janelas altas, as portas."""
    # o miolo: o fundo, os 2 lados do vão, a tampa do vão, o prisma do telhado, o oitão da frente (sobe PAR acima do telhado)
    core(p, PX0, CAV_X0, -PW, PW, -0.1, PE, top=False)
    core(p, CAV_X0, PX1, CAV_HW, PW, -0.1, PE, skip=('-x',), top=False)
    core(p, CAV_X0, PX1, -PW, -CAV_HW, -0.1, PE, skip=('-x',), top=False)
    core(p, CAV_X0, PX1, -CAV_HW, CAV_HW, CAV_TOP, PE, skip=('-x', '+y', '-y'), top=False)
    poly_plate(p, 'Body#vp', [(-PW, PE - 0.05), (PW, PE - 0.05), (0.0, PR)], (PX0, 0, 0), Y, Z, X, 0.0, PX1 - PX0, open_sides=(0,))
    pw_ = PW + 0.06
    poly_plate(p, 'Body#vp', [(-pw_, PE - 0.1), (pw_, PE - 0.1), (pw_, PR + PAR - KR * pw_), (0.0, PR + PAR), (-pw_, PR + PAR - KR * pw_)],
               (PX1 - 0.32, 0, 0), Y, Z, X, 0.0, 0.32)
    # o rodapé de concreto (as faces de y de quina a quina, as de x param 12 cm antes), cortado no portão e nas portas
    base_run(p, 'y', PW, 1, PX0 - 0.14, PX1 + 0.14)
    base_run(p, 'y', -PW, -1, PX0 - 0.14, PX1 + 0.14, gaps=[(-0.9, 0.3)])
    base_run(p, 'x', PX1, 1, -PW + 0.12, PW - 0.12, gaps=[(-3.82, 3.82)])
    base_run(p, 'x', PX0, -1, -PW + 0.12, PW - 0.12, gaps=[(-2.65, -1.45), (0.35, 3.25)])
    # as laterais: as placas, a faixa azul no beiral, as janelas altas entre os pilares (e a porta no lado −Y)
    wins = [(u - 0.75, u + 0.75) for u in (-6.625, -3.475, -0.325, 2.825)]
    clad(p, 'y', PW, 1, PX0 + 0.32, PX1 - 0.32, ZW, PE, pw=2.99, zrows=PZR, band=PB, holes=[(a, b, 3.85, 4.5) for a, b in wins])
    clad(p, 'y', -PW, -1, PX0 + 0.32, PX1 - 0.32, ZW, PE, pw=1.58, zrows=PZR, band=PB,
         holes=[(a, b, 3.85, 4.5) for a, b in wins if a > 0 or a < -1.5] + [(-0.88, 0.28, ZW, Z0 + 2.38)])
    for s in (-1, 1):
        for (a, b) in wins:
            if s < 0 and -1.5 < a < 0:
                continue
            window(p, 'y', s * PW, s, a, b, 3.85, 4.5, mull=1)
        pilasters(p, 'y', s * PW, s, PIL_P, PE - 0.25)
    door(p, 'y', -PW, -1, -0.3)
    for u in (-3.475, 2.825):
        wall_lamp(p, 'y', -PW, -1, u, 3.35)
    # as quinas (as da frente sobem até a ponta do oitão)
    corner_pils(p, [(PX0, -PW, -1, -1, PE - 0.18), (PX0, PW, -1, 1, PE - 0.18),
                    (PX1, -PW, 1, -1, PE + PAR - KR * 0.0 + 0.2), (PX1, PW, 1, 1, PE + PAR + 0.2)])
    # a empena da frente: as placas em volta do portão, recortadas pelo caimento do oitão (a capa escura por cima)
    face_clad(p, (PX1, 0, 0), Y, Z, X, [-PW + 0.32, -3.82, -2.3, -0.75, 0.75, 2.3, 3.82, PW - 0.32],
              [ZW, Z0 + 1.9, Z0 + 3.5, PB[0], PB[1], PE + 1.6, PR + PAR], tent=(0.0, PR + PAR - 0.14, KR),
              holes=[(-3.82, 3.82, 0.0, PE + 0.02)], band=PB)
    # a empena de trás: as placas debaixo do beiral do telhado, a porta e a porta de enrolar
    face_clad(p, (PX0, 0, 0), Y, Z, (-1, 0, 0), [-PW + 0.32, -2.9, -0.9, 0.9, 2.9, PW - 0.32],
              [ZW, Z0 + 1.9, Z0 + 3.5, PB[0], PB[1], PE + 1.6, PR], tent=(0.0, PR - 0.08, KR),
              holes=[(-2.6, -1.5, 0.0, Z0 + 2.38), (0.4, 3.2, 0.0, Z0 + 3.35)], band=PB)
    door(p, 'x', PX0, -1, -2.05)
    eu, en = axes('x', -1)
    yc, w, h = 1.8, 2.4, 3.0
    ub.obox(p, 'Dark#vp', wpt('x', PX0, -1, yc, Z0 + h / 2), (w, h, 0.06), eu, Z, en)
    zz = Z0 + 0.3
    while zz < Z0 + h - 0.15:
        ub.obox(p, 'Trim#vp', wpt('x', PX0, -1, yc, zz, 0.05), (w - 0.1, 0.24, 0.04), eu, Z, en)
        zz += 0.3
    ub.obox(p, 'Body#sm', wpt('x', PX0, -1, yc, Z0 + h + 0.24, 0.2), (w + 0.4, 0.46, 0.4), eu, Z, en)
    ub.obox(p, 'Hazard#vp', wpt('x', PX0, -1, yc, Z0 + 0.09, 0.07), (w - 0.1, 0.14, 0.04), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt('x', PX0, -1, yc + su * (w / 2 + 0.07), Z0 + h / 2, 0.08), (0.14, h + 0.05, 0.16), eu, Z, en)
    wall_lamp(p, 'x', PX0, -1, -0.15, 4.0)
    # os canos entre o pavilhão e o salão (a ligação das 2 peças na passagem)
    for (z, r) in ((2.9, 0.09), (3.3, 0.07)):
        pipe(p, 'Dark', [(-6.2, PW + 0.09, z), (-6.2, HY0 - 0.09, z)], r, step=0.6)


def pavilion_roof(p):
    """As 2 águas de chapas com a junta descendo (o telhado de pavilhão de aço), o lanternim de ventilação na cumeeira, a capa do oitão,
    o rufo dos fundos, as calhas e as descidas, os 2 exaustores."""
    a = math.atan(KR)
    ca, sa = math.cos(a), math.sin(a)
    x0, x1 = PX0 - 0.3, PX1 - 0.34
    n = 9
    y_lo = 0.06
    Ls = (PW + OV - y_lo) / ca
    ymid = y_lo + (PW + OV - y_lo) / 2
    for s in (-1, 1):
        ev = Vector((0.0, s * ca, -sa))
        en = Vector((0.0, s * sa, ca))
        c0 = Vector((0.0, s * ymid, PR - KR * ymid)) + en * 0.04
        for k in range(n):
            xa = x0 + (x1 - x0) * k / n + (0.03 if k else 0.0)
            xb = x0 + (x1 - x0) * (k + 1) / n - (0.03 if k < n - 1 else 0.0)
            ub.obox(p, 'Paint#pnl', c0 + Vector(((xa + xb) / 2, 0.0, 0.0)), (xb - xa, Ls, 0.12), X, ev, en)
        # o rufo de trás (a testeira do beiral da empena)
        ub.obox(p, 'Trim#sm', c0 + Vector((PX0 - 0.38, 0.0, 0.0)) + en * 0.04, (0.12, Ls + 0.06, 0.26), X, ev, en)
        # a calha do beiral e as descidas
        yg = s * (PW + OV + 0.08)
        zb = PE - KR * OV - 0.24
        bx(p, 'Trim#sm', PX0 - 0.25, PX1 - 0.06, yg - 0.11, yg + 0.11, zb, zb + 0.22)
        for xd in (PX0 + 0.45,):
            pipe(p, 'Trim', [(xd, yg, zb + 0.02), (xd, s * (PW + 0.2), zb - 0.35), (xd, s * (PW + 0.2), Z0 + 0.4),
                             (xd, s * (PW + 0.45), Z0 + 0.14)], 0.07, step=2.6)
    # a capa escura do oitão (as 2 réguas no caimento e o remate da ponta com a baliza)
    for s in (-1, 1):
        L = (PW + 0.12) / ca
        d = Vector((0.0, s * ca, -sa))
        nn = Vector((0.0, s * sa, ca))
        c = Vector((PX1 - 0.16, s * (PW + 0.12) / 2, PR + PAR - KR * (PW + 0.12) / 2)) + nn * 0.05
        ub.obox(p, 'Trim#sm', c, (0.44, L, 0.12), X, d, nn)
    bx(p, 'Trim#sm', PX1 - 0.4, PX1 + 0.08, -0.22, 0.22, PR + PAR - 0.02, PR + PAR + 0.16)
    ub.beacon(p, (PX1 - 0.16, 0.0, PR + PAR + 0.16), r=0.09, h=0.2)
    # o lanternim da cumeeira: a base de placa, a faixa de veneziana escura com as aletas, o chapéu de duas águas
    xv0, xv1 = PX0 + 0.5, PX1 - 1.0
    bx(p, 'Body#pnl', xv0, xv1, -0.42, 0.42, PR - 0.36, PR + 0.3)
    for s in (-1, 1):
        bx(p, 'Dark#vp', xv0 + 0.18, xv1 - 0.18, s * 0.41, s * 0.435, PR - 0.06, PR + 0.2)
        x = xv0 + 0.4
        while x < xv1 - 0.3:
            bx(p, 'Trim#vp', x - 0.03, x + 0.03, s * 0.42, s * 0.46, PR - 0.08, PR + 0.22)
            x += 1.2
    poly_plate(p, 'Paint#sm', [(-0.62, PR + 0.29), (0.62, PR + 0.29), (0.62, PR + 0.35), (0.0, PR + 0.5), (-0.62, PR + 0.35)],
               (xv0 - 0.12, 0, 0), Y, Z, X, 0.0, xv1 - xv0 + 0.24)
    # os 2 exaustores giratórios na água −Y (o colar de chapa no caimento)
    for xe in (-5.6, -1.6):
        ye = -2.7
        zt = PR - KR * (abs(ye) - 0.4) + 0.25          # (o topo do colar acima do telhado no lado de cima da água)
        bx(p, 'Body#sm', xe - 0.4, xe + 0.4, ye - 0.4, ye + 0.4, PR - KR * (abs(ye) + 0.4) + 0.04, zt)
        ub.turbine_vent(p, xe, ye, zt, r=0.3)


def pavilion_front(p):
    """O portão seccional: os batentes e a verga, as guias, a última folha recolhida no alto do vão (as janelinhas, a borracha, a régua
    de perigo), os frades; o vão escuro por dentro (as placas escuras, o forro com as calhas de luz, o portão do fundo); o semáforo
    da saída, os holofotes; o DISCO com a estrela branca do Exército na empena."""
    for s in (-1, 1):
        bx(p, 'Trim#pnl', PX1 - 0.1, PX1 + 0.3, s * DOOR_HW, s * (DOOR_HW + JAMB), Z0 - 0.02, DOOR_TOP + 0.02)
        bx(p, 'Hazard#pnl', PX1 - 0.13, PX1 + 0.34, s * (DOOR_HW - 0.04), s * (DOOR_HW + JAMB + 0.04), Z0 - 0.02, Z0 + 1.15)
        bx(p, 'Dark#vp', PX1 - 0.42, PX1 - 0.26, s * (DOOR_HW - 0.12), s * (DOOR_HW + 0.02), Z0, CAV_TOP)
        vp.lathe(p, 'Hazard#vp', (PX1 + 0.75, s * (DOOR_HW + 0.68), Z0), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.95), (0.13, 1.02), (0.0, 1.04)],
                 seg=14)
        # os holofotes da saída, de cada lado do disco
        lamp_head(p, (PX1 + 0.36, s * 2.65, 6.05), (1.0, s * 0.25, -1.2), w=0.5, h=0.26)
        ub.obox(p, 'Trim#sm', (PX1 + 0.14, s * 2.65, 6.05), (0.18, 0.22, 0.18), X, Y, Z)
    bx(p, 'Trim#pnl', PX1 - 0.12, PX1 + 0.32, -(DOOR_HW + JAMB + 0.03), DOOR_HW + JAMB + 0.03, DOOR_TOP, PE + 0.02)
    ub.obox(p, 'Hazard#vp', (PX1 + 0.33, 0.0, DOOR_TOP + 0.07), (0.02, 2 * DOOR_HW, 0.1), X, Y, Z)
    # a última folha do portão recolhido (aparece no alto do vão)
    bx(p, 'Body#pnl', PX1 - 0.38, PX1 - 0.28, -3.32, 3.32, DOOR_LO, CAV_TOP - 0.03)
    bx(p, 'Hazard#vp', PX1 - 0.39, PX1 - 0.27, -3.3, 3.3, DOOR_LO + 0.04, DOOR_LO + 0.14)
    bx(p, 'Dark#vp', PX1 - 0.37, PX1 - 0.29, -3.3, 3.3, DOOR_LO - 0.05, DOOR_LO + 0.02)
    for yc in (-2.6, -1.3, 0.0, 1.3, 2.6):
        bx(p, 'Glass#vp', PX1 - 0.285, PX1 - 0.27, yc - 0.4, yc + 0.4, DOOR_LO + 0.22, DOOR_LO + 0.46)
    # por dentro: as paredes de placas escuras, o forro com as calhas de luz, o fundo com o portão de enrolar fechado
    for s in (-1, 1):
        bx(p, 'Trim#vp', CAV_X0 + 0.05, PX1 - 0.48, s * (CAV_HW - 0.03), s * CAV_HW, Z0, CAV_TOP - 0.06)
        for xr in (-0.6, 1.0, 2.6):
            bx(p, 'Dark#vp', xr - 0.04, xr + 0.04, s * (CAV_HW - 0.05), s * (CAV_HW - 0.02), Z0, CAV_TOP - 0.06)
    bx(p, 'Dark#vp', CAV_X0, PX1 - 0.3, -CAV_HW, CAV_HW, CAV_TOP - 0.05, CAV_TOP)
    for y in (-1.7, 0.0, 1.7):
        bx(p, 'Light#vp', CAV_X0 + 0.6, PX1 - 1.0, y - 0.09, y + 0.09, CAV_TOP - 0.08, CAV_TOP - 0.045)
    for (a, b) in ((-CAV_HW + 0.05, -1.7), (1.7, CAV_HW - 0.05)):
        bx(p, 'Trim#vp', CAV_X0, CAV_X0 + 0.03, a, b, Z0, CAV_TOP - 0.06)
    bx(p, 'Trim#vp', CAV_X0, CAV_X0 + 0.03, -1.7, 1.7, Z0 + 3.5, CAV_TOP - 0.06)
    bx(p, 'Dark#vp', CAV_X0, CAV_X0 + 0.03, -1.6, 1.6, Z0, Z0 + 3.4)
    zz = Z0 + 0.3
    while zz < Z0 + 3.3:
        bx(p, 'Trim#vp', CAV_X0 + 0.02, CAV_X0 + 0.06, -1.55, 1.55, zz - 0.11, zz + 0.11)
        zz += 0.3
    lamp_head(p, (CAV_X0 + 0.25, 0.0, Z0 + 4.2), (1.0, 0.0, -1.2), w=0.6, h=0.26)
    # o semáforo da saída (na face, entre o batente e a quina −Y)
    bx(p, 'Dark#sm', PX1 + 0.08, PX1 + 0.3, -4.38, -4.08, 2.7, 3.45)
    for (z, m) in ((3.24, 'Beacon'), (2.9, 'Light')):
        vp.lathe(p, m + '#vp', (PX1 + 0.3, -4.23, z), 'x', [(0.0, 0.0), (0.1, 0.0), (0.1, 0.02), (0.0, 0.05)], seg=14)
        ub.obox(p, 'Dark#vp', (PX1 + 0.36, -4.23, z + 0.12), (0.12, 0.24, 0.02), X, Y, Z)
    # o DISCO: a placa de fundo, o aro escuro em relevo com os parafusos, o campo azul (a cor do jogador), o anel e a estrela brancos
    cx, cy, cz = DISC
    vp.prism(p, 'Trim#vp', circle(DISC_R + 0.1, 64), (cx, cy, cz), Vector(Y), Vector(Z), Vector(X), 0.06)
    vp.prism(p, 'Player#vp', circle(DISC_R, 64), (cx + 0.06, cy, cz), Vector(Y), Vector(Z), Vector(X), 0.03)
    vp.lathe(p, 'Trim#vp', (cx + 0.06, cy, cz), 'x', [(DISC_R, 0.0), (DISC_R + 0.16, 0.0), (DISC_R + 0.16, 0.07), (DISC_R + 0.06, 0.11),
                                                     (DISC_R, 0.11)], seg=64, closed=True)
    vp.lathe(p, 'Paint#vp', (cx + 0.09, cy, cz), 'x', [(1.14, 0.0), (1.28, 0.0), (1.28, 0.018), (1.14, 0.018)], seg=64, closed=True)
    vp.prism(p, 'Paint#vp', star_pts(1.04, 0.4), (cx + 0.09, cy, cz), Vector(Y), Vector(Z), Vector(X), 0.02)
    vp.bolt_ring(p, 'Dark', (cx + 0.15, cy, cz), 'x', DISC_R + 0.1, 16, r=0.035, h=0.02, phase=math.pi / 16)


# ---------------------------------------------------------------------------------- o salão de comando
def hall(p):
    """O salão baixo e largo: o miolo, o rodapé, as placas das 4 faces com a faixa azul no beiral, os pilares, as portas e janelas, os
    parapeitos (abertos na quina da sala de operações)."""
    xc = [HX0, HX0 + 6.0, HX0 + 12.0, HX1]
    for i in range(3):
        core(p, xc[i], xc[i + 1], HY0, HY1, -0.1, HE, skip=(('-x',) if i else ()) + (('+x',) if i < 2 else ()))
    base_run(p, 'y', HY0, -1, HX0 - 0.14, HX1 + 0.14, gaps=[(-6.4, -6.38), (1.05, 2.15)])
    base_run(p, 'y', HY1, 1, HX0 - 0.14, HX1 + 0.14, gaps=[(-6.45, -5.35), (-0.8, -0.78)])
    base_run(p, 'x', HX1, 1, HY0 + 0.12, HY1 - 0.12, gaps=[(HDOOR_Y - 1.1, HDOOR_Y + 1.1)])
    base_run(p, 'x', HX0, -1, HY0 + 0.12, HY1 - 0.12, gaps=[(8.15, 10.65), (11.85, 12.95)])
    # a frente (+X): a janela larga, a porta dupla embaixo do balanço
    clad(p, 'x', HX1, 1, HY0 + 0.32, HY1 - 0.32, ZW, HE, pw=1.5, zrows=HZR, band=HB,
         holes=[(6.95, 9.05, Z0 + 1.45, Z0 + 2.55), (HDOOR_Y - 1.08, HDOOR_Y + 1.08, ZW, Z0 + 2.62)])
    window(p, 'x', HX1, 1, 6.95, 9.05, Z0 + 1.45, Z0 + 2.55, mull=2)
    for k, yc in enumerate((HDOOR_Y - 0.48, HDOOR_Y + 0.48)):
        door(p, 'x', HX1, 1, yc, w=0.96, h=2.45, canopy=False, lamp=False)
    bx(p, 'Player#sm', HX1 + 0.08, HX1 + 0.15, HDOOR_Y - 0.9, HDOOR_Y + 0.9, Z0 + 2.85, Z0 + 3.2)
    wall_lamp(p, 'x', HX1, 1, 8.0, 3.4)
    # o lado −Y (a passagem pro pavilhão): as janelas, a porta, as descidas dos vales
    clad(p, 'y', HY0, -1, HX0 + 0.32, HX1 - 0.32, ZW, HE, pw=1.56, zrows=HZR, band=HB,
         holes=[(-10.4, -9.0, Z0 + 1.45, Z0 + 2.35), (-6.6, -5.2, Z0 + 1.45, Z0 + 2.35), (1.05, 2.15, ZW, Z0 + 2.38)])
    window(p, 'y', HY0, -1, -10.4, -9.0, Z0 + 1.45, Z0 + 2.35)
    window(p, 'y', HY0, -1, -6.6, -5.2, Z0 + 1.45, Z0 + 2.35)
    door(p, 'y', HY0, -1, 1.6)
    for xd in (SX0 + 2 * SP, SX0 + 4 * SP):
        pipe(p, 'Trim', [(xd, HY0 - 0.05, HE + 0.12), (xd, HY0 - 0.22, HE - 0.2), (xd, HY0 - 0.22, Z0 + 0.4), (xd, HY0 - 0.45, Z0 + 0.14)],
             0.07, step=1.4)
    # o lado +Y: as janelas, a porta
    wy = [(-10.4, -9.0), (-3.0, -1.6), (1.5, 2.9)]
    clad(p, 'y', HY1, 1, HX0 + 0.32, HX1 - 0.32, ZW, HE, pw=2.91, zrows=HZR, band=HB,
         holes=[(a, b, Z0 + 1.45, Z0 + 2.35) for a, b in wy] + [(-6.45, -5.35, ZW, Z0 + 2.38)])
    for (a, b) in wy:
        window(p, 'y', HY1, 1, a, b, Z0 + 1.45, Z0 + 2.35)
    door(p, 'y', HY1, 1, -5.9, canopy=False)
    wall_lamp(p, 'y', HY1, 1, -8.5, 3.5)
    # os fundos (−X): a porta de enrolar e a de gente
    clad(p, 'x', HX0, -1, HY0 + 0.32, HY1 - 0.32, ZW, HE, pw=1.5, zrows=HZR, band=HB,
         holes=[(8.2, 10.6, ZW, Z0 + 3.3), (11.9, 12.9, ZW, Z0 + 2.38)])
    eu, en = axes('x', -1)
    yc, w, h = 9.4, 2.4, 3.0
    ub.obox(p, 'Dark#vp', wpt('x', HX0, -1, yc, Z0 + h / 2), (w, h, 0.06), eu, Z, en)
    zz = Z0 + 0.3
    while zz < Z0 + h - 0.15:
        ub.obox(p, 'Trim#vp', wpt('x', HX0, -1, yc, zz, 0.05), (w - 0.1, 0.24, 0.04), eu, Z, en)
        zz += 0.3
    ub.obox(p, 'Body#sm', wpt('x', HX0, -1, yc, Z0 + h + 0.24, 0.2), (w + 0.4, 0.46, 0.4), eu, Z, en)
    ub.obox(p, 'Hazard#vp', wpt('x', HX0, -1, yc, Z0 + 0.09, 0.07), (w - 0.1, 0.14, 0.04), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt('x', HX0, -1, yc + su * (w / 2 + 0.07), Z0 + h / 2, 0.08), (0.14, h + 0.05, 0.16), eu, Z, en)
    door(p, 'x', HX0, -1, 12.4)
    # os pilares: o ritmo das faces compridas e o da frente (onde a mão-francesa do balanço pousa)
    for s, at in ((-1, HY0), (1, HY1)):
        pilasters(p, 'y', at, s, [-7.8, -4.0, -0.2], HE + 0.12)
    pilasters(p, 'x', HX1, 1, [9.9], OZ0 - 0.02)
    corner_pils(p, [(HX0, HY0, -1, -1, HE + 0.42), (HX0, HY1, -1, 1, HE + 0.42), (HX1, HY0, 1, -1, HE + 0.42),
                    (HX1, HY1, 1, 1, OZ0 - 0.02)])
    # os parapeitos com a capa (abertos onde a sala de operações senta)
    zp0, zp1 = HE - 0.1, HE + 0.35
    parapet_run(p, (HX0, HY0), (HX1, HY0), zp0, zp1)
    parapet_run(p, (HX1, HY0), (HX1, OY0 - 0.08), zp0, zp1)
    parapet_run(p, (OX0 - 0.08, HY1), (HX0, HY1), zp0, zp1)
    parapet_run(p, (HX0, HY1), (HX0, HY0), zp0, zp1)


def sawtooth(p):
    """O TETO EM SERRA: 5 dentes; cada um com o miolo em prisma, a água de chapas com a junta, a claraboia escura inclinada virada pra
    frente com os montantes, o rufo da crista, a calha no vale e as empenas triangulares de placa nas pontas; a faixa de teto plano da
    frente com as juntas, o condensador, o alçapão e o exaustor."""
    for k in range(NT):
        xa = SX0 + k * SP
        xb = xa + SP
        xs = xb - SG
        zt = HE + ST
        poly_plate(p, 'Body#vp', [(xa, HE - 0.05), (xb, HE - 0.05), (xs, zt)], (0, TY0, 0), X, Z, Y, 0.0, TY1 - TY0)
        Ls = math.hypot(xs - xa, ST)
        Sv = Vector(((xs - xa) / Ls, 0.0, ST / Ls))
        Sn = Vector((-ST / Ls, 0.0, (xs - xa) / Ls))
        panel_wall(p, (xa, TY0, HE), Y, Sv, Sn, TY1 - TY0, Ls, pw=1.4, m='Body#pnl')
        Lg = math.hypot(SG, ST)
        Gv = Vector((-SG / Lg, 0.0, ST / Lg))
        Gn = Vector((ST / Lg, 0.0, SG / Lg))
        gc = Vector(((xb + xs) / 2, (TY0 + TY1) / 2, HE + ST / 2))
        ub.obox(p, 'Glass#vp', gc + Gn * 0.012, (TY1 - TY0 - 0.12, Lg - 0.14, 0.03), Y, Gv, Gn)
        nm = 4
        for i in range(1, nm):
            yy = TY0 + (TY1 - TY0) * i / nm
            ub.obox(p, 'Trim#vp', Vector(((xb + xs) / 2, yy, HE + ST / 2)) + Gn * 0.04, (0.07, Lg - 0.08, 0.07), Y, Gv, Gn)
        for s in (-1, 1):
            yy = (TY0 + 0.05) if s < 0 else (TY1 - 0.05)
            ub.obox(p, 'Trim#vp', Vector(((xb + xs) / 2, yy, HE + ST / 2)) + Gn * 0.04, (0.1, Lg - 0.04, 0.08), Y, Gv, Gn)
        ub.obox(p, 'Trim#sm', (xs + 0.02, (TY0 + TY1) / 2, zt + 0.01), (0.3, TY1 - TY0 - 0.02, 0.1), X, Y, Z)
        # as empenas triangulares das pontas
        for (ye, t0, t1) in ((TY0, -0.06, 0.03), (TY1, -0.03, 0.06)):
            poly_plate(p, 'Body#sm', [(xa + 0.06, HE + 0.03), (xb - 0.05, HE + 0.03), (xs, zt - 0.05)], (0, ye, 0), X, Z, Y, t0, t1)
    # as calhas dos vales (e a de trás e a da frente)
    for k in range(NT + 1):
        xv = SX0 + k * SP
        bx(p, 'Trim#vp', xv - 0.17, xv + 0.17, TY0 + 0.01, TY1 - 0.01, HE - 0.04, HE + 0.11)
    # a faixa de teto plano da frente (fora da sala de operações): as juntas, o condensador, o alçapão, o exaustor
    roof_lines(p, SX1 + 0.25, HX1 - 0.35, HY0 + 0.35, OY0 - 0.2, HE, 2, 2)
    fan_box(p, (2.1, 7.45), (2.2, 1.2, 0.85), fans=2, z0=HE)
    bx(p, 'Body#pnl', 0.25, 1.05, 8.45, 9.25, HE - 0.05, HE + 0.32)
    ub.obox(p, 'Body#sm', (0.65, 8.85, HE + 0.35), (0.86, 0.86, 0.06), X, Y, Z)
    ub.turbine_vent(p, 0.2, 7.2, HE, r=0.3)


def ops_room(p):
    """A SALA DE OPERAÇÕES em balanço na quina (+X, +Y): a laje com o peitoril, o vidro inclinado pra fora em 3 lados (os montantes, o
    caixilho, a faixa acesa do forro por dentro), a parede de trás com a porta e os degraus pro teto do salão, a laje do teto com o
    beiral, o condensador, as antenas, o anemômetro; embaixo do balanço as mãos-francesas, a viga de borda e as luzes sobre a porta."""
    # a laje e o peitoril (o friso escuro na altura do piso, a pingadeira embaixo)
    bx(p, 'Body#pnl', OX0, OX1, OY0, OY1, OZ0, OZG)
    bx(p, 'Dark#vp', OX0 + 0.02, OX1 + 0.012, OY0 - 0.012, OY1 + 0.012, OZS - 0.025, OZS + 0.025)
    bx(p, 'Trim#sm', OX0 + 0.02, OX1 + 0.03, OY0 - 0.03, OY1 + 0.03, OZ0 - 0.04, OZ0 + 0.1)
    # o vidro inclinado: os 3 lados (−Y, +X, +Y) do pé (z OZG, a borda da laje) ao topo (z OZT, 0,3 m pra fora)
    A0, B0, C0, D0 = (OX0, OY0), (OX1, OY0), (OX1, OY1), (OX0, OY1)
    A1, B1, C1, D1 = (OX0, OY0 - OTL), (OX1 + OTL, OY0 - OTL), (OX1 + OTL, OY1 + OTL), (OX0, OY1 + OTL)
    sides = [((A0, B0), (A1, B1), 4, 0), ((B0, C0), (B1, C1), 3, 1), ((C0, D0), (C1, D1), 4, 1)]
    for (P0, Q0), (P1, Q1), n, skip in sides:
        P0v, Q0v = Vector((P0[0], P0[1], OZG)), Vector((Q0[0], Q0[1], OZG))
        P1v, Q1v = Vector((P1[0], P1[1], OZT)), Vector((Q1[0], Q1[1], OZT))
        along = (Q0v - P0v).normalized()
        up = ((P1v + Q1v) / 2 - (P0v + Q0v) / 2).normalized()
        N = along.cross(up).normalized()
        for i in range(n):
            ba, bb = P0v.lerp(Q0v, i / n), P0v.lerp(Q0v, (i + 1) / n)
            ta, tb = P1v.lerp(Q1v, i / n), P1v.lerp(Q1v, (i + 1) / n)
            quad_slab(p, 'Glass#vp', [ba, bb, tb, ta], N, -0.03, 0.0)
            quad_slab(p, 'Light#vp', [ba.lerp(ta, 0.8), bb.lerp(tb, 0.8), bb.lerp(tb, 0.88), ba.lerp(ta, 0.88)], N, 0.0, 0.006)
        for i in range(skip, n + 1):
            b_, t_ = P0v.lerp(Q0v, i / n), P1v.lerp(Q1v, i / n)
            corner = i in (0, n)
            bar(p, 'Trim#vp', b_ + N * 0.04, t_ + N * 0.04, 0.12 if corner else 0.08, 0.1 if corner else 0.08)
        bar(p, 'Trim#vp', P0v + N * 0.05, Q0v + N * 0.05, 0.12, 0.12)
        bar(p, 'Trim#vp', P1v + N * 0.05, Q1v + N * 0.05, 0.14, 0.12)
    # a parede de trás (o trapézio: o topo acompanha o vidro), as placas, a porta e os degraus pro teto do salão
    poly_plate(p, 'Body#vp', [(OY0, OZ0), (OY1, OZ0), (OY1, OZG), (OY1 + OTL, OZT), (OY0 - OTL, OZT), (OY0, OZG)], (OX0, 0, 0), Y, Z, X,
               0.0, 0.25)
    face_clad(p, (OX0, 0, 0), Y, Z, (-1, 0, 0), [OY0 + 0.05, 10.75, 12.65, OY1 - 0.05], [OZG, 6.45, OZT - 0.02],
              holes=[(11.15, 12.25, OZS, OZS + 2.3)])
    door(p, 'x', OX0, -1, 11.7, w=1.0, h=2.25, canopy=False, lamp=False, zb=OZS)
    bx(p, 'Trim#sm', OX0 - 0.62, OX0 - 0.02, 11.1, 12.3, HE - 0.02, HE + 0.22)
    bx(p, 'Trim#sm', OX0 - 0.33, OX0 - 0.03, 11.1, 12.3, HE + 0.2, OZS - 0.01)
    # a laje do teto com o beiral, o friso escuro, o condensador, as antenas, o anemômetro, as balizas
    bx(p, 'Body#pnl', OX0 - 0.15, OX1 + OTL + 0.3, OY0 - OTL - 0.3, OY1 + OTL + 0.3, OZT, OZT + 0.3)
    bx(p, 'Dark#vp', OX0 - 0.1, OX1 + OTL + 0.312, OY0 - OTL - 0.312, OY1 + OTL + 0.312, OZT + 0.12, OZT + 0.16)
    zr = OZT + 0.3
    rx0, rx1, ry0, ry1 = OX0 - 0.15, OX1 + OTL + 0.3, OY0 - OTL - 0.3, OY1 + OTL + 0.3
    roof_lines(p, rx0 + 0.25, rx1 - 0.25, ry0 + 0.25, ry1 - 0.25, zr, 3, 3)
    for (a, b) in (((rx0, ry0), (rx1, ry0)), ((rx1, ry0), (rx1, ry1)), ((rx1, ry1), (rx0, ry1)), ((rx0, ry1), (rx0, ry0))):
        parapet_run(p, a, b, zr - 0.05, zr + 0.12, t=0.16, out=0.0)
    rail(p, 'Trim', [(rx0 + 0.3, ry0 + 0.25), (rx1 - 0.25, ry0 + 0.25), (rx1 - 0.25, ry1 - 0.25), (rx0 + 0.3, ry1 - 0.25)], zr,
         h=1.0, step=1.6, mid=False)
    fan_box(p, (1.9, 11.0), (1.3, 1.0, 0.7), fans=1, z0=zr)
    bx(p, 'Body#pnl', 1.45, 2.35, 12.3, 13.2, zr - 0.05, zr + 0.55)
    louvers(p, (2.355, 12.75, zr + 0.25), (1, 0, 0), (0, 1, 0), 0.6, 0.35, 4)
    for (x, y, hh) in ((1.6, 13.6, 3.2), (5.9, 9.8, 2.4)):
        ub.antenna(p, x, y, zr, hh, r=0.07)
        ub.beacon(p, (x, y, zr + hh + 0.04), r=0.06, h=0.12, cage=False)
    ax, ay = 1.75, 9.75
    vp.tube(p, 'Trim', [(ax, ay, zr - 0.05), (ax, ay, zr + 1.7)], 0.04, seg=8)
    vp.tube(p, 'Trim', [(ax - 0.45, ay, zr + 1.55), (ax + 0.45, ay, zr + 1.55)], 0.025, seg=6)
    for k in range(3):
        a = 2 * math.pi * k / 3
        q = Vector((ax + 0.45 + 0.2 * math.cos(a), ay + 0.2 * math.sin(a), zr + 1.62))
        vp.tube(p, 'Trim', [(ax + 0.45, ay, zr + 1.62), q], 0.012, seg=5)
        vp.lathe(p, 'Paint#vp', q, (math.cos(a + math.pi / 2), math.sin(a + math.pi / 2), 0.0),
                 [(0.0, -0.05), (0.06, -0.03), (0.07, 0.0), (0.0, 0.0)], seg=10)
    vp.tube(p, 'Trim', [(ax + 0.45, ay, zr + 1.55), (ax + 0.45, ay, zr + 1.66)], 0.02, seg=6)
    vp.tube(p, 'Trim', [(ax - 0.45, ay, zr + 1.55), (ax - 0.45, ay, zr + 1.7)], 0.02, seg=6)
    ub.obox(p, 'Trim#vp', (ax - 0.62, ay, zr + 1.7), (0.4, 0.02, 0.18), X, Y, Z)
    for (x, y) in ((rx1 - 0.08, ry0 + 0.08), (rx1 - 0.08, ry1 - 0.08)):
        ub.beacon(p, (x, y, zr + 0.2), r=0.08, h=0.17)
    # embaixo do balanço: as mãos-francesas (pousam no pilar do meio e no da quina), a viga de borda, as luzes
    for (y, x0) in ((9.9, HX1 + 0.24), (13.5, HX1 + 0.38)):
        bx(p, 'Trim#sm', x0, x0 + 0.08, y - 0.2, y + 0.2, 1.95, 2.65)
        bar(p, 'Trim#vp', (x0 + 0.06, y, 2.3), (OX1 - 0.32, y, OZ0 - 0.06), 0.22, 0.26)
    bx(p, 'Trim#pnl', OX1 - 0.48, OX1 - 0.16, OY0 + 0.2, OY1 - 0.2, OZ0 - 0.3, OZ0 + 0.02)
    for y in (10.6, 11.7, 12.8):
        bx(p, 'Light#vp', 4.5, 5.1, y - 0.25, y + 0.25, OZ0 - 0.02, OZ0 + 0.005)


# ---------------------------------------------------------------------------------- o mastro do radar
def mast(p):
    """O MASTRO TUBULAR estaiado: a fundação octogonal, a placa de base com os chumbadores e as mãos-francesas, o tubo em 3 lances com as
    flanges aparafusadas, os 2 anéis de estai com as orelhas, os 6 estais até os 3 blocos de ancoragem (os esticadores, a capa amarela
    de proteção no pé), as 2 plataformas em C (o piso, a viga de borda, as escoras, o guarda-corpo), a escada com gaiola, a bandeja de
    cabo, as caixas de rádio, as 2 antenas de setor, as balizas; no topo o mancal fixo do radar."""
    cx, cy = MC
    ring8 = lambda r, z: [(cx + r * math.cos(math.radians(22.5 + 45 * k)), cy + r * math.sin(math.radians(22.5 + 45 * k)), z) for k in range(8)]
    p.loft('Concrete#pnl', [ring8(1.25, -0.1), ring8(1.25, 0.4), ring8(0.95, 0.75)], bevel=0.04)
    vp.lathe(p, 'Trim#vp', (cx, cy, 0.75), 'z', [(0.0, 0.0), (0.74, 0.0), (0.74, 0.08), (0.0, 0.08)], seg=24)
    vp.bolt_ring(p, 'Dark', (cx, cy, 0.83), 'z', 0.62, 8, r=0.04, h=0.05, phase=math.pi / 8)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        e = Vector((math.cos(a), math.sin(a), 0.0))
        t = Vector((-math.sin(a), math.cos(a), 0.0))
        poly_plate(p, 'Trim#vp', [(MR[0] - 0.02, 0.83), (MR[0] + 0.34, 0.83), (MR[0] - 0.02, 1.45)], (cx, cy, 0), e, Z, t, -0.02, 0.02)
    # o tubo em 3 lances (o raio diminui) e as flanges aparafusadas
    vp.lathe(p, 'Body#vp', (cx, cy, 0.0), 'z', [(0.0, 0.83), (MR[0], 0.83), (MR[0], 4.62), (MR[1], 4.66), (MR[1], 8.62), (MR[2], 8.66),
                                                (MR[2], MZ_DECK - 0.1), (0.0, MZ_DECK - 0.1)], seg=28)
    for zf, ri, ro in ((4.6, MR[1], MR[0]), (8.6, MR[2], MR[1])):
        vp.lathe(p, 'Trim#vp', (cx, cy, zf), 'z', [(ri - 0.01, -0.06), (ro + 0.1, -0.06), (ro + 0.1, 0.06), (ri - 0.01, 0.06)], seg=28,
                 closed=True)
        vp.bolt_ring(p, 'Dark', (cx, cy, zf + 0.06), 'z', ro + 0.05, 10, r=0.03, h=0.03)
    # a faixa da cor do jogador no lance de cima
    vp.lathe(p, 'Player#vp', (cx, cy, 9.6), 'z', [(MR[2] - 0.005, 0.0), (MR[2] + 0.012, 0.0), (MR[2] + 0.012, 0.5), (MR[2] - 0.005, 0.5)],
             seg=28, closed=True)
    # os anéis de estai, as orelhas e os estais até as âncoras
    anchors = []
    for ang in GUY_A:
        a = math.radians(ang)
        anchors.append((Vector((cx + GUY_R * math.cos(a), cy + GUY_R * math.sin(a), 0.0)), Vector((math.cos(a), math.sin(a), 0.0))))
    for zg, rt in zip(GUY_Z, (MR[1], MR[2])):
        vp.lathe(p, 'Trim#vp', (cx, cy, zg), 'z', [(rt - 0.005, -0.12), (rt + 0.09, -0.12), (rt + 0.09, 0.12), (rt - 0.005, 0.12)], seg=24,
                 closed=True)
        for (A, e) in anchors:
            ub.obox(p, 'Trim#sm', (cx + e.x * (rt + 0.2), cy + e.y * (rt + 0.2), zg), (0.26, 0.06, 0.2), e, Vector((-e.y, e.x, 0.0)), Vector(Z))
    for (A, e) in anchors:
        side = Vector((-e.y, e.x, 0.0))
        d_in = -e                                            # da âncora pro mastro
        ub.obox(p, 'Concrete#pnl', (A.x, A.y, 0.12), (1.0, 1.0, 0.46), d_in, side, Vector(Z))
        bx(p, 'Trim#vp', A.x - 0.2, A.x + 0.2, A.y - 0.2, A.y + 0.2, 0.34, 0.38)
        for j, (zg, rt) in enumerate(zip(GUY_Z, (MR[1], MR[2]))):
            top = Vector((cx + e.x * (rt + 0.3), cy + e.y * (rt + 0.3), zg))
            foot = Vector((A.x, A.y, 0.38)) + side * (0.1 if j else -0.1)
            w = (top - foot).normalized()
            vp.tube(p, 'Trim', [foot, foot + w * 0.25], 0.035, seg=6)
            vp.lathe(p, 'Trim#vp', foot + w * 0.25, w, [(0.0, 0.0), (0.055, 0.0), (0.055, 0.34), (0.0, 0.34)], seg=10)
            vp.tube(p, 'Trim', [foot + w * 0.59, top], 0.022, seg=6)
            vp.tube(p, 'Yellow', [foot + w * 0.75, foot + w * 2.6], 0.05, seg=8)
    # as 2 plataformas em C (o vão é a passagem da escada, em +X)
    for (zd, r1) in ((7.2, 0.95), (MZ_DECK, 1.2)):
        rt = MR[1] if zd < 8.6 else MR[2]
        arc_slab(p, 'Trim#vp', (cx, cy), rt - 0.01, r1, 28.0, 332.0, zd - 0.08, zd)
        arc_slab(p, 'Trim#vp', (cx, cy), r1 - 0.08, r1, 28.0, 332.0, zd - 0.26, zd - 0.08)
        for k in range(1, 9):
            a = math.radians(28 + (332 - 28) * k / 9)
            q = Vector((cx + (rt + 0.1) * math.cos(a), cy + (rt + 0.1) * math.sin(a), zd + 0.003))
            q2 = Vector((cx + (r1 - 0.12) * math.cos(a), cy + (r1 - 0.12) * math.sin(a), zd + 0.003))
            dd = (q2 - q).normalized()
            ub.obox(p, 'Dark#vp', (q + q2) / 2, ((q2 - q).length, 0.05, 0.01), dd, Vector((-dd.y, dd.x, 0.0)), Vector(Z))
        for a_ in (75.0, 190.0, 230.0, 315.0):
            a = math.radians(a_)
            bar(p, 'Trim#vp', (cx + rt * math.cos(a), cy + rt * math.sin(a), zd - 1.0),
                (cx + (r1 - 0.12) * math.cos(a), cy + (r1 - 0.12) * math.sin(a), zd - 0.26), 0.07)
        rp = [(cx + (r1 - 0.05) * math.cos(math.radians(a)), cy + (r1 - 0.05) * math.sin(math.radians(a))) for a in range(30, 331, 15)]
        rail(p, 'Trim', rp, zd, h=1.0, step=1.0)
    ub.beacon(p, (cx - 1.15, cy, MZ_DECK + 1.01), r=0.08, h=0.17)
    ub.beacon(p, (cx - 0.9, cy, 7.2 + 1.01), r=0.07, h=0.15, cage=False)
    # a escada com gaiola (em +X), a bandeja de cabo (em −X), as caixas de rádio e as 2 antenas de setor
    ub.ladder(p, 'Trim', cx + MR[2], cy, 0.83, MZ_DECK, (1, 0, 0), w=0.45, cage=True, wall=0.42, top_ext=1.0)
    c0, c1, c2 = MR[0] + 0.05, MR[1] + 0.05, MR[2] + 0.05
    vp.tube(p, 'Dark', [(cx - c0, cy, 0.9), (cx - c0, cy, 4.45), (cx - c1, cy, 4.8), (cx - c1, cy, 8.45), (cx - c2, cy, 8.8),
                        (cx - c2, cy, MZ_DECK - 0.1)], 0.045, seg=8)
    for zc in (9.2, 10.2):
        ub.obox(p, 'Body#sm', (cx, cy - MR[2] - 0.18, zc), (0.42, 0.3, 0.62), X, Y, Z)
        ub.obox(p, 'Trim#vp', (cx, cy - MR[2] - 0.02, zc), (0.2, 0.06, 0.3), X, Y, Z)
    for a_ in (110.0, 250.0):
        a = math.radians(a_)
        e = Vector((math.cos(a), math.sin(a), 0.0))
        q = Vector((cx, cy, MZ_DECK)) + e * 1.3
        vp.tube(p, 'Trim', [q - e * 0.16 + Vector((0, 0, 0.1)), q + Vector((0, 0, 0.1))], 0.03, seg=6)
        vp.tube(p, 'Trim', [q - e * 0.16 + Vector((0, 0, 0.8)), q + Vector((0, 0, 0.8))], 0.03, seg=6)
        ub.obox(p, 'Body#sm', q + e * 0.08 + Vector((0, 0, 0.45)), (0.12, 0.32, 1.2), e, Vector((-e.y, e.x, 0.0)), Vector(Z))
    # o mancal fixo do radar em cima da plataforma
    hz = RADAR_AT[2] - MZ_DECK
    vp.lathe(p, 'Trim#vp', (cx, cy, MZ_DECK), 'z', [(0.0, 0.0), (0.5, 0.0), (0.5, 0.06), (0.42, 0.12), (0.42, hz), (0.0, hz)], seg=28)
    vp.bolt_ring(p, 'Dark', (cx, cy, MZ_DECK + 0.06), 'z', 0.46, 12, r=0.025, h=0.02)


# ---------------------------------------------------------------------------------- a casa de rádio, o gerador, a ponte de cabos
def radio_block(p):
    """A CASA DE EQUIPAMENTOS (o abrigo de comunicação): o miolo, o rodapé, as placas com a faixa azul, a porta, a veneziana, os 2
    ar-condicionados de parede virados pra frente, o parapeito; no teto o FEIXE de antenas de chicote no tripé e a antena log-periódica;
    o GERADOR encostado (o skid, as venezianas, o escape) com o tanque de diesel; a ponte de cabos até o mastro."""
    x0, x1, y0, y1 = RB
    solid(p, x0, x1, y0, y1, -0.1, RBH)
    base_run(p, 'y', y0, -1, x0 - 0.14, x1 + 0.14, gaps=[(-1.55, -0.45)])
    base_run(p, 'y', y1, 1, x0 - 0.14, x1 + 0.14)
    base_run(p, 'x', x1, 1, y0 + 0.12, y1 - 0.12)
    base_run(p, 'x', x0, -1, y0 + 0.12, y1 - 0.12)
    zr = [Z0 + 1.6, RBH - 0.5]
    band = (RBH - 0.5, RBH)
    clad(p, 'x', x1, 1, y0 + 0.3, y1 - 0.3, ZW, RBH, pw=1.2, zrows=zr, band=band)
    clad(p, 'x', x0, -1, y0 + 0.3, y1 - 0.3, ZW, RBH, pw=3.0, zrows=zr, band=band)
    clad(p, 'y', y0, -1, x0 + 0.3, x1 - 0.3, ZW, RBH, pw=1.25, zrows=zr, band=band, holes=[(-1.55, -0.45, ZW, Z0 + 2.38)])
    clad(p, 'y', y1, 1, x0 + 0.3, x1 - 0.3, ZW, RBH, pw=2.3, zrows=zr, band=band)
    door(p, 'y', y0, -1, -1.0)
    louvers(p, (1.0, y0 - 0.085, 1.5), (0, -1, 0), (1, 0, 0), 0.8, 0.6, 6)
    louvers(p, (-0.4, y1 + 0.085, 1.6), (0, 1, 0), (1, 0, 0), 1.0, 0.7, 7)
    corner_pils(p, [(x0, y0, -1, -1, RBH + 0.38), (x0, y1, -1, 1, RBH + 0.38), (x1, y0, 1, -1, RBH + 0.38), (x1, y1, 1, 1, RBH + 0.38)])
    parapet_run(p, (x0, y0), (x1, y0), RBH - 0.1, RBH + 0.3)
    parapet_run(p, (x1, y0), (x1, y1), RBH - 0.1, RBH + 0.3)
    parapet_run(p, (x1, y1), (x0, y1), RBH - 0.1, RBH + 0.3)
    parapet_run(p, (x0, y1), (x0, y0), RBH - 0.1, RBH + 0.3)
    roof_lines(p, x0 + 0.3, x1 - 0.3, y0 + 0.3, y1 - 0.3, RBH, 2, 2)
    # os 2 ar-condicionados de parede (o de abrigo de rádio): a caixa, a grelha de baixo, a de cima, a capa, a mão-francesa
    for yc in (-10.95, -9.15):
        bx(p, 'Body#pnl', x1 + 0.08, x1 + 0.62, yc - 0.48, yc + 0.48, 0.62, 2.42)
        louvers(p, (x1 + 0.625, yc, 1.15), (1, 0, 0), (0, 1, 0), 0.72, 0.8, 8)
        louvers(p, (x1 + 0.625, yc, 2.02), (1, 0, 0), (0, 1, 0), 0.72, 0.36, 4)
        bx(p, 'Trim#sm', x1 + 0.05, x1 + 0.68, yc - 0.52, yc + 0.52, 2.42, 2.5)
        bx(p, 'Trim#vp', x1 + 0.08, x1 + 0.6, yc - 0.4, yc + 0.4, 0.54, 0.62)
        for s in (-1, 1):
            bar(p, 'Trim#vp', (x1 + 0.09, yc + s * 0.36, 0.3), (x1 + 0.56, yc + s * 0.36, 0.58), 0.05)
        ub.obox(p, 'Player#sm', (x1 + 0.63, yc, 2.3), (0.02, 0.6, 0.1), X, Y, Z)
    # o FEIXE de antenas: o tripé com a chapa no topo e os 6 chicotes; a antena log-periódica no poste da quina
    tx, ty = -0.6, -10.0
    apex = Vector((tx, ty, RBH + 2.2))
    for k in range(3):
        a = math.radians(90 + 120 * k)
        foot = Vector((tx + 0.75 * math.cos(a), ty + 0.75 * math.sin(a), RBH))
        vp.tube(p, 'Trim', [foot, apex - Vector((0, 0, 0.05))], 0.045, seg=8)
        bx(p, 'Trim#vp', foot.x - 0.12, foot.x + 0.12, foot.y - 0.12, foot.y + 0.12, RBH - 0.02, RBH + 0.03)
    vp.lathe(p, 'Trim#vp', apex - Vector((0, 0, 0.06)), 'z', [(0.0, 0.0), (0.4, 0.0), (0.4, 0.07), (0.0, 0.07)], seg=6)
    for k, hh in enumerate((4.6, 3.2, 3.9, 2.6, 4.2, 3.0)):
        a = math.radians(30 + 60 * k)
        ub.antenna(p, tx + 0.3 * math.cos(a), ty + 0.3 * math.sin(a), apex.z + 0.01, hh, r=0.07)
    ub.beacon(p, (tx, ty, apex.z + 0.01), r=0.07, h=0.15, cage=False)
    lx, ly = 1.45, -11.25
    vp.tube(p, 'Trim', [(lx, ly, RBH - 0.05), (lx, ly, RBH + 3.1)], 0.06, seg=8)
    bx(p, 'Trim#vp', lx - 0.18, lx + 0.18, ly - 0.18, ly + 0.18, RBH - 0.02, RBH + 0.04)
    da = math.radians(215.0)
    d = Vector((math.cos(da), math.sin(da), 0.0))
    sd = Vector((-d.y, d.x, 0.0))
    P = Vector((lx, ly, RBH + 3.05))
    vp.tube(p, 'Trim', [P - d * 1.15, P + d * 1.15], 0.045, seg=8)
    for i in range(10):
        q = P - d * 1.05 + d * (2.1 * i / 9)
        half = 1.15 - 0.085 * i
        vp.tube(p, 'Trim', [q - sd * half, q + sd * half], 0.022, seg=5)
    # o gerador: o skid, o corpo, as venezianas, o radiador, o painel, o escape com o chapéu, as orelhas, a faixa azul
    gx0, gx1, gy0, gy1 = GEN
    gh = 1.95
    bx(p, 'Trim#pnl', gx0 - 0.1, gx1 + 0.1, gy0 - 0.1, gy1 + 0.1, Z0 - 0.02, Z0 + 0.2)
    bx(p, 'Body#pnl', gx0, gx1, gy0, gy1, Z0 + 0.2, gh)
    for s, yy in ((-1, gy0), (1, gy1)):
        for xc in (gx0 + 0.85, gx1 - 0.85):
            louvers(p, (xc, yy + s * 0.085, 1.05), (0, s, 0), (1, 0, 0), 1.1, 0.8, 7)
        bx(p, 'Player#sm', gx0 + 0.1, gx1 - 0.1, yy + s * 0.07 - 0.015, yy + s * 0.07 + 0.015, gh - 0.3, gh - 0.18)
    louvers(p, (gx0 - 0.085, (gy0 + gy1) / 2, 1.05), (-1, 0, 0), (0, 1, 0), 1.1, 1.2, 10)
    ub.obox(p, 'Trim#sm', (gx1 + 0.06, (gy0 + gy1) / 2, 1.15), (0.04, 0.9, 1.1), X, Y, Z)
    ub.obox(p, 'Dark#vp', (gx1 + 0.085, (gy0 + gy1) / 2, 1.35), (0.02, 0.5, 0.3), X, Y, Z)
    ex = gx1 - 0.55
    pipe(p, 'Dark', [(ex, (gy0 + gy1) / 2, gh - 0.05), (ex, (gy0 + gy1) / 2, gh + 0.95)], 0.09, step=0.6)
    vp.lathe(p, 'Trim#vp', (ex, (gy0 + gy1) / 2, gh + 0.97), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.04), (0.0, 0.1)], seg=12)
    for xx in (gx0 + 0.3, gx1 - 0.3):
        vp.lathe(p, 'Trim#vp', (xx, (gy0 + gy1) / 2, gh), 'z', [(0.0, 0.0), (0.07, 0.0), (0.07, 0.14), (0.0, 0.18)], seg=8)
    pipe(p, 'Dark', [(gx1 + 0.05, gy1 - 0.25, 0.42), (x0 - 0.05, gy1 - 0.25, 0.42)], 0.07, step=0.8)
    ub.h_tank(p, (-8.9, -10.1, Z0 + 0.9), 'x', 0.55, 2.0, foot_z=Z0)
    pipe(p, 'Trim', [(-8.4, -10.1, Z0 + 1.42), (-8.4, -10.1, 2.15), (-6.6, -10.1, 2.15), (-6.6, -10.1, gh - 0.04)], 0.04, step=1.0)
    # a ponte de cabos até o mastro (as 2 longarinas, os degraus, os cabos deitados, os 2 postes em T)
    yb = -10.08
    xs, xe = x1 + 0.05, MC[0] - MR[0] - 0.03
    zb = 2.72
    for s in (-1, 1):
        bx(p, 'Trim#vp', xs, xe, yb + s * 0.22 - 0.02, yb + s * 0.22 + 0.02, zb, zb + 0.1)
    x = xs + 0.2
    while x < xe - 0.1:
        bx(p, 'Trim#vp', x - 0.02, x + 0.02, yb - 0.2, yb + 0.2, zb, zb + 0.03)
        x += 0.6
    for k in (-1, 0, 1):
        vp.tube(p, 'Dark', [(xs, yb + k * 0.1, zb + 0.07), (xe, yb + k * 0.1, zb + 0.07)], 0.03, seg=6)
    for xp in (4.3, 6.6):
        bx(p, 'Concrete#sm', xp - 0.2, xp + 0.2, yb - 0.2, yb + 0.2, -0.05, 0.25)
        vp.tube(p, 'Trim', [(xp, yb, 0.25), (xp, yb, zb - 0.02)], 0.06, seg=8)
        bx(p, 'Trim#vp', xp - 0.05, xp + 0.05, yb - 0.3, yb + 0.3, zb - 0.08, zb)
    bx(p, 'Trim#sm', x1 + 0.02, x1 + 0.2, yb - 0.3, yb + 0.3, zb - 0.15, zb + 0.25)
    vp.lathe(p, 'Trim#vp', (MC[0], MC[1], zb + 0.05), 'z', [(MR[0] - 0.01, -0.1), (MR[0] + 0.07, -0.1), (MR[0] + 0.07, 0.1),
                                                         (MR[0] - 0.01, 0.1)], seg=20, closed=True)


# ---------------------------------------------------------------------------------- a bandeira, os HESCO, os sacos, a carga, a cerca
def flag_mast(p):
    """O mastro da bandeira no pé de concreto em tronco de pirâmide, com o anel de sacos de areia (2 fiadas); o pano no vento é do
    jogo (flags.json)."""
    fx, fy = FLAG_AT
    sq = ((1, 1), (-1, 1), (-1, -1), (1, -1))
    p.loft('Concrete#pnl', [[(fx + a * 0.62, fy + b * 0.62, -0.1) for a, b in sq], [(fx + a * 0.36, fy + b * 0.36, 0.95) for a, b in sq]],
           bevel=0.04)
    ub.obox(p, 'Trim#sm', (fx, fy, 0.98), (0.5, 0.5, 0.06), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 1.0), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.05), (0.09, 0.1), (0.075, 0.4), (0.05, 8.4), (0.0, 8.4)], seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 9.42), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), 9.35, 2.4, 1.45, m='Player')
    for course in range(2):
        n = 10
        for k in range(n):
            a = 2 * math.pi * (k + 0.5 * course) / n
            sandbag(p, (fx + 1.18 * math.cos(a), fy + 1.18 * math.sin(a), Z0 + 0.155 * course), math.degrees(a) + 90.0)


def defenses(p):
    """Os HESCO no canto do gerador (o L de fora), os sacos de areia nos 2 lados da porta do salão, a carga com a lona azul e os
    tambores nos fundos do pavilhão, a cerca de alambrado de um lado só (−Y) com o portão, os postes de luz."""
    for k in range(4):
        hesco(p, (-11.7 + 1.1 * k + 1.1, -12.95))
    for k in range(4):
        hesco(p, (-11.7, -12.95 + 1.1 * k))
    for yw in (HDOOR_Y - 1.25, HDOOR_Y + 1.25):
        for course in range(2):
            for k in range(2):
                sandbag(p, (HX1 + 0.65 + 0.64 * k + (0.3 if course % 2 else 0.0), yw, Z0 + 0.155 * course), 0.0)
    ub.obox(p, 'Crate#sm', (-10.7, -1.6, 0.08), (1.6, 2.4, 0.16), X, Y, Z)
    ub.obox(p, 'Body#sm', (-10.7, -1.6, 0.6), (1.3, 2.1, 0.9), X, Y, Z)
    ub.tarp_cover_at(p, (-10.7, -1.6), 2.4, 1.6, 90.0, 0.2, 0.95, m='Player', seed=2.1, ties=3, box=14)
    for (x, y) in ((-10.5, 2.0), (-11.1, 2.35), (-10.55, 2.7)):
        vp.lathe(p, 'Trim#vp', (x, y, -0.02), 'z', [(0.0, 0.0), (0.29, 0.0), (0.29, 0.88), (0.27, 0.9), (0.0, 0.9)], seg=16)
        for z in (0.3, 0.58):
            p.torus('Dark#vp', (x, y, z), 'z', 0.295, 0.015, seg=16, rseg=4)
    fence(p, (-7.2, -14.25), (3.0, -14.25), gate=(5.2, 8.2))
    ub.lamp_post(p, 10.0, 4.9, h=6.2, aim=(7.0, 0.0))
    ub.lamp_post(p, 7.7, 13.6, h=6.2, aim=(5.0, 11.7))


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    for k, bev in (('Body#pil', 0.04), ('Trim#pil', 0.04)):
        p.bm(k, bev)
    ground(p)
    pavilion(p)
    pavilion_roof(p)
    pavilion_front(p)
    hall(p)
    sawtooth(p)
    ops_room(p)
    mast(p)
    radio_block(p)
    flag_mast(p)
    defenses(p)
    vp.poke_ngons(p, 0.05)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o radar (origem no eixo de giro)
RAD_TH = math.radians(18.0)      # a face olha pra +X e 18° pra cima
RAD_W, RAD_H, RAD_D = 4.5, 2.05, 0.3
RAD_PIV = Vector((0.35, 0.0, 0.95))   # a quina de baixo da face (a articulação da elevação)


def radar():
    """A ANTENA DE RADAR RETANGULAR que gira (x local = pra onde ela olha): a mesa giratória, a caixa do acionamento com as venezianas,
    a articulação, o arranjo plano inclinado (o corpo de chapa, as 8 faixas claras dos guias de onda no fundo escuro), as nervuras e as
    escoras das costas, a caixa do receptor atrás no braço, a barra do IFF em cima nos 3 pés, a luz de aviso."""
    p = CPart(PREFIX + '_Radar', cull=False)
    ub.keys(p)
    n = Vector((math.cos(RAD_TH), 0.0, math.sin(RAD_TH)))
    ev = Vector((-math.sin(RAD_TH), 0.0, math.cos(RAD_TH)))
    ey = Vector(Y)
    W, H, D = RAD_W, RAD_H, RAD_D
    C = RAD_PIV + ev * (H / 2) - n * (D / 2)
    # a mesa e o acionamento
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (0.62, 0.0), (0.62, 0.12), (0.54, 0.18), (0.0, 0.18)], seg=32)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.18), 'z', 0.5, 16, r=0.025, h=0.018)
    bx(p, 'Body#pnl', -0.6, 0.45, -0.46, 0.46, 0.17, 0.78)
    for s in (-1, 1):
        louvers(p, (-0.1, s * 0.465, 0.47), (0, s, 0), (1, 0, 0), 0.7, 0.34, 4)
    # a articulação: as 2 orelhas e o eixo
    hinge = RAD_PIV - n * 0.2
    for s in (-1, 1):
        bx(p, 'Trim#sm', hinge.x - 0.16, hinge.x + 0.16, s * 0.3, s * 0.44, 0.76, hinge.z + 0.12)
    vp.lathe(p, 'Trim#vp', (hinge.x, -0.8, hinge.z), 'y', [(0.0, 0.0), (0.09, 0.0), (0.09, 1.6), (0.0, 1.6)], seg=14)
    # o arranjo: o corpo, o fundo escuro, as 8 faixas dos guias de onda
    ub.obox(p, 'Body#pnl', C, (W, H, D), ey, ev, n)
    F = C + n * (D / 2)
    ub.obox(p, 'Dark#vp', F + n * 0.005, (W - 0.22, H - 0.22, 0.02), ey, ev, n)
    nr = 8
    hu = H - 0.3
    for i in range(nr):
        vc = -hu / 2 + (i + 0.5) * hu / nr
        ub.obox(p, 'Paint#sm', F + ev * vc + n * 0.025, (W - 0.34, hu / nr - 0.07, 0.04), ey, ev, n)
    # as costas: as nervuras, as escoras até a caixa do acionamento
    B = C - n * (D / 2)
    for v in (-0.6, 0.0, 0.6):
        ub.obox(p, 'Trim#vp', B - n * 0.06 + ev * v, (W - 0.4, 0.1, 0.12), ey, ev, n)
    for s in (-1, 1):
        ub.obox(p, 'Trim#vp', B - n * 0.06 + ey * (s * 1.4), (0.1, H - 0.3, 0.12), ey, ev, n)
        bar(p, 'Trim#vp', (-0.35, s * 0.38, 0.76), B - n * 0.1 + ev * 0.45 + ey * (s * 1.4), 0.09)
        bar(p, 'Trim#vp', (-0.5, s * 0.25, 0.76), B - n * 0.08 + ey * (s * 0.7), 0.08)
    # a caixa do receptor no braço atrás
    bx(p, 'Trim#vp', -0.8, -0.58, -0.25, 0.25, 0.4, 0.62)
    bx(p, 'Body#pnl', -1.45, -0.78, -0.6, 0.6, 0.4, 1.25)
    for s in (-1, 1):
        louvers(p, (-1.1, s * 0.605, 0.82), (0, s, 0), (1, 0, 0), 0.45, 0.5, 5)
    ub.obox(p, 'Player#sm', (-1.455, 0.0, 1.1), (0.02, 0.9, 0.12), X, Y, Z)
    # a barra do IFF em cima, nos 3 pés, e a luz de aviso
    T = RAD_PIV + ev * H - n * (D / 2)
    for u in (-1.6, 0.0, 1.6):
        ub.obox(p, 'Trim#vp', T + ey * u + ev * 0.09, (0.1, 0.2, 0.1), ey, ev, n)
    ub.obox(p, 'Dark#sm', T + ev * 0.27, (W + 0.3, 0.2, 0.16), ey, ev, n)
    ub.beacon(p, tuple(T + ev * 0.34 + Vector((0, 0, 0.0))), r=0.07, h=0.14, cage=False)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o andaime
def scaffold():
    """O andaime da obra (0,6 m de largura: o salão fica a 1 m da borda do lote): o lado −Y e os fundos do pavilhão, o lado +Y e a
    frente do salão (fora do balanço), o lado −Y da casa de rádio; o portão e a faixa de saída ficam livres."""
    runs = [((PX0 + 0.2, -PW - 0.9), (PX1 - 0.2, -PW - 0.9), PE + 0.3, (0, -1)),
            ((PX0 - 0.95, -PW + 0.2), (PX0 - 0.95, PW - 0.2), PE + 0.8, (-1, 0)),
            ((-11.0, HY1 + 0.3), (OX0 - 0.5, HY1 + 0.3), HE + 0.4, (0, 1)),
            ((HX1 + 0.45, HY0 + 0.2), (HX1 + 0.45, OY0 - 0.4), HE + 0.4, (1, 0)),
            ((RB[0] + 0.2, RB[2] - 0.9), (RB[1] - 0.2, RB[2] - 0.9), RBH + 0.4, (0, -1))]
    return scaffold_runs(PREFIX + '_Scaffold', runs, out=0.6)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_command_center_v5')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    r = radar()
    sc = scaffold()
    check([b, r, sc], HX, HY, at={r.name: RADAR_AT})
    sweep = max(math.hypot(v.co.x, v.co.y) for v in r.data.vertices)
    top = max(v.co.z for v in r.data.vertices) + RADAR_AT[2]
    low = min(v.co.z for v in r.data.vertices if math.hypot(v.co.x, v.co.y) > 1.15) + RADAR_AT[2]
    print('MODEL radar: varre r = %.2f m (o círculo: x %.2f..%.2f, y %.2f..%.2f; o lote ±%.2f × ±%.2f); o topo a z = %.2f; fora do '
          'guarda-corpo (r > 1,15) o mais baixo a z = %.2f (o guarda-corpo vai até %.2f)' %
          (sweep, MC[0] - sweep, MC[0] + sweep, MC[1] - sweep, MC[1] + sweep, HX, HY, top, low, MZ_DECK + 1.0))
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, r, sc)})
    r = bake_in_place(r, RADAR_AT)
    total = export(b, out)
    total += export(r, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL spin at_m', [round(v, 3) for v in RADAR_AT], 'rpm 6')
    print('MODEL fire_points_m', [list(q) for q in FIRE_POINTS])
    print('MODEL saida: o portão em x = %.2f (a face da empena), y = 0; o vão %.2f m (%.2f m entre as guias), %.2f m livres até a folha '
          'recolhida; o pátio de saída até x = 9,6 (depois o chão, livre até a borda do lote)' %
          (PX1, 2 * DOOR_HW, 2 * (DOOR_HW - 0.1), DOOR_LO - Z0))
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]]],   # (posição do Blender)
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1500]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1700, 950]],
                  ['side', 'ortho', [0, -1, 0], [0, 0, 1], [1700, 900]],
                  ['three_quarter', 'persp', 40.0, 221.0, [1600, 1200], 38.0, 0.9],
                  ['back_three_quarter', 'persp', 32.0, 40.0, [1600, 1000], 30.0, 0.95]],
                 (8.6, 0.0, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
