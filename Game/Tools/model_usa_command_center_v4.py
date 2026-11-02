"""O Command Center da USA (usa_command_center), VERSÃO 4 (01/11/2026): o do ZERO HOUR, quase copiado, no detalhe AAA das peças aprovadas.

Por quê: 4 tentativas reprovadas seguidas (o A, o B, o Pentágono da v3 e as duas folhas de conceito por IA). O Felipe: "um lixo, parece
jogo de criança, não encaixa com o jogo; desiste dessa IA, pensa você, faz você, olha o C&C ZH, nem que copie quase tudo". A referência:
docs/concept/buildings/ref_zh/zh_usa_command_center.jpg; o contexto: docs/elenco/USA_CC_E_USINA.md §5. A linguagem (as chapas com
fresta, o chanfro largo, a faixa azul, o detalhe que lê na câmera) é a das aprovadas: model_usa_war_factory_v2.py (a ficha aprovada),
model_usa_barracks_v2.py (o hangar arredondado da mesma família), model_usa_supply_center_v2.py; docs/ESTUDO_DETALHE_AAA.md. Da v3 só a
interface (o lote, as 3 peças, o radar que gira, a bandeira pelo flags.json).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_command_center_v4.py C:/Users/luisf/explosion/captures/game/models/usa_command_center_v4

A COMPOSIÇÃO DO ORIGINAL, medida na foto do ZH (os eixos da planta pelas bordas do pátio e das paredes: a câmera dele ~40° de
inclinação, ~41° de giro; o galpão 2,05 × mais comprido que largo; o hangar tão largo quanto o galpão, 0,56 do comprimento dele, ao lado
do galpão e recuado; a torre na quina da frente do galpão, do lado de fora; a placa na frente da torre; a bandeira na frente do
hangar, do lado de fora; a cerca fechando o pátio do lado do hangar). Peça por peça:
  - o GALPÃO comprido: as paredes de PLACAS com a junta funda e os pilares de pé (as faixas verticais do original) com o pé escuro, a
    FAIXA AZUL de placas no beiral, o rodapé de concreto; o TETO CURVO: a abóbada rasa (meia-elipse 2,9 × 1,1 m) de chapa clara em
    6 gomos com a quina redonda e a BORDA EM RELEVO na costura (as listras atravessadas do original), as pontas em "nariz", a calha do
    beiral; o resto do teto plano com os 2 ventiladores redondos, o duto comprido com o guarda-corpo, a veneziana, as juntas das lajes;
  - a ENTRADA na frente (+X): o portão grande escuro de 2 folhas com as almofadas, os pilares escuros dos lados, a placa azul, a
    luminária, as claraboias; as 3 estrelas pintadas no chão (as do original, na frente da porta);
  - o BLOCO DE RÁDIO elevado atrás (+Y, nos fundos): as placas, as janelas, o parapeito com a capa, as 2 caixas de ar-condicionado com
    os ventiladores, o armário de rádio, as 5 ANTENAS DE CHICOTE, as balizas, o guarda-corpo, a escada do teto do galpão até ele;
  - a ESCADA DE MARINHEIRO subindo a parede do lado do hangar e passando por cima do teto curvo (a do original);
  - entre o galpão e o hangar: a caixa escura de equipamento (o gerador) com as venezianas e o ventilador, os canos;
  - o HANGAR MEIA-LUA (o arco alteado deitado): os 2 PÓRTICOS grossos de chapa clara (o de trás fechado pela parede de placas com a
    porta; o da frente aberto: o PORTÃO, a saída do Dozer), a casca de CHAPAS CURVAS com a fresta, as nervuras, o meio-fio de concreto,
    o pé escuro dos pórticos, a faixa de perigo nas pernas do portão, o trilho da porta de enrolar no arco, a faixa azul no alto do
    pórtico, os holofotes, a passarela com o guarda-corpo na cumeeira, a escada; A ESTRELA BRANCA DO EXÉRCITO num disco escuro com o
    aro branco no flanco virado pro lado da cerca (onde o original põe: a empena da frente é toda portão); por dentro, o escuro: o
    piso escuro, o forro, as luzes, o portão de enrolar do fundo, a linha amarela;
  - a TORRE DE TRELIÇA (a assinatura): o pedestal de concreto em tronco de pirâmide, as 4 pernas em tronco de pirâmide com 5 lances de
    X, a plataforma com o guarda-corpo, a casinha de equipamento, a escada com gaiola; em cima o PRATO GRANDE que gira (a peça à parte);
  - o chão: SÓ o contorno de concreto em volta do prédio (a faixa de saída na frente do portão com as faixas amarelas, o pátio cercado
    do lado do hangar, as calçadas, o pé da torre), nunca a laje do lote inteiro (a regra do Felipe); a CERCA de alambrado com o
    portão, o MASTRO da bandeira no pé de concreto (o pano no vento é do jogo: flags.json), a PLACA inclinada com o emblema no pé de
    concreto dela, os postes de luz.
A USA limpa (13/10): sem desgaste pintado no modelo (vem do material do jogo, pelas juntas, chapas e máscaras daqui).

O lote 25,28 × 29,52 m (x −12,64..12,64, y −14,76..14,76; Units.json usa_command_center), a frente em +X. A SAÍDA: o portão do hangar na
face +X (x = 1,2), centrado em y = 0, o vão de 6,8 m (6,6 m livres entre os trilhos da porta), 4,3 m livres a ±1,75 m do eixo e 4,8 m no
meio; a faixa de saída sem nada até a borda do lote (o concreto até x = 6,4, depois o chão). O que mudou da composição do ZH pra caber
no lote com o portão no meio: o hangar vira o centro (y = 0) e o galpão fica à esquerda dele (+Y) com uma passagem de 1,1 m (no ZH
~3 m); a torre, que no ZH fica ao lado do galpão, vai pra frente do canto dele (+X, +Y), na frente de um recorte da quina; o pátio
cercado à direita (−Y) fica mais largo; o prato um pouco menor que o do ZH (4 m) pra varrer dentro do lote e ficar nos ~16 m.
Alturas: o beiral do galpão 4,6 m, a cumeeira do teto curvo 5,7, o bloco de rádio 7,2 (as antenas até 13), o pórtico do hangar 5,85,
a plataforma da torre 11,4, o eixo do prato 12,95 (spin at_m), o topo do prato 16,25 (a regra: a torre até ~16 m).
Peças: SM_USA_CommandCenter_Base, SM_USA_CommandCenter_Radar (origem no eixo de giro: spin at_m = RADAR_AT), SM_USA_CommandCenter_Scaffold.
Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light, Beacon, Yellow, Paint, Crate (o andaime).
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402
from model_usa_power_plant_v2 import CPart, check, preview_spec, slab_tiles  # noqa: E402

PREFIX = 'SM_USA_CommandCenter'
COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62),
               'Rubber': (32, 32, 31)})

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)

# ---------------------------------------------------------------------------------- as medidas
HX, HY = 12.64, 14.76            # o lote 25,28 × 29,52
Z0 = 0.14                        # o topo das lajes do contorno
# o galpão
XH0, XH1 = -9.0, 7.0             # os fundos e a frente (a face da entrada)
XN = 5.6                         # o recorte da quina da frente (+Y): a torre fica na frente dele
YH0, YH1 = 5.4, 13.6             # a face do lado do hangar (−Y) e a de fora (+Y)
YE1 = 10.6                       # a face +Y da entrada = a face −Y do bloco de rádio
EAVE = 4.6                       # o beiral
XR1, ZR = -0.4, 7.2              # o bloco de rádio: a frente e o teto
BASE_T = 0.5                     # o rodapé de concreto (até Z0 + BASE_T)
BAND = (EAVE - 0.55, EAVE)       # a faixa azul do beiral (a fileira de cima das placas)
RV = 1.45                        # o teto curvo: a meia-largura da abóbada
RVZ = 1.1                        # ... e a altura (rasa: a meia-elipse do original, não o tubo)
YV = YH0 + 0.2 + RV              # o eixo dela (y)
XV0, XV1 = XH0 + 0.05 + RV, XH1 - 0.05 - RV      # o trecho reto (as pontas em nariz até XH0+0,05 e XH1−0,05)
DOOR_Y, DOOR_W, DOOR_H = YV, 3.0, 3.2            # o portão da entrada (embaixo do nariz do teto curvo)
# o hangar (o arco alteado deitado ao longo de X)
HX0, HX1 = -8.8, 1.2             # o pórtico de trás (a face de trás) e o da frente (a face do portão)
HF_D = 0.7                       # a profundidade dos pórticos
HS = 1.5                         # a altura onde o arco nasce (as pernas a prumo embaixo)
HB_R = 4.0                       # a casca: o raio (a face de dentro das chapas) e a meia-largura das pernas
HO_R, HF_R = 3.4, 4.35           # o pórtico: o raio do vão (o portão) e o de fora
ZC = Z0 + 0.45                   # o topo do meio-fio (onde as chapas começam)
SH_T = 0.09                      # a espessura das chapas da casca
LLEG = HS - ZC
SARC = math.pi * HB_R
STOT = 2 * LLEG + SARC           # o comprimento do contorno da casca (pelo raio HB_R)
XB0, XB1 = HX0 + HF_D - 0.03, HX1 - HF_D + 0.03  # a casca entre os pórticos (as pontas entram 3 cm neles)
RIBS = (-5.97, -3.83)            # as nervuras (o vão da frente fica liso: a estrela)
STAR_X, STAR_A = -1.66, math.radians(142.0)      # a estrela: o centro em x e o ângulo no arco (90° = a cumeeira, 180° = a perna −Y)
WIN_E = (9.65, 10.2)             # a janelinha da frente da entrada (y)
# a torre
TC = (7.9, 12.35)                # o eixo da torre (o do prato): o prato girando (r 2,4 m) não sai do lote
TZ0, TZ1 = 1.3, 11.4             # o topo do pedestal e a plataforma
TW0, TW1 = 0.95, 0.52            # a meia-largura das pernas embaixo e em cima
PL0, PL1 = 1.35, 1.12            # o pedestal: a meia-largura embaixo e em cima
RADAR_AT = (TC[0], TC[1], 12.95) # o eixo de giro do prato (spin at_m)
# o pátio cercado (−Y) e a bandeira
FY = -9.5                        # a cerca do lado de fora
FX0, FX1 = -8.6, 1.0             # a cerca de trás e a da frente
FLAG_AT = (4.6, -6.6)
SIGN_AT = (10.85, 11.7)

FIRE_POINTS = [(-5.0, 12.0, ZR + 0.1), (2.0, 9.6, EAVE + 0.1), (-4.5, 0.0, HS + HB_R + SH_T + 0.05)]


# ---------------------------------------------------------------------------------- ajudantes (os da War Factory B, a aprovada)
def bx(p, m, x0, x1, y0, y1, z0, z1):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    ub.obox(p, m, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), X, Y, Z)


def axes(axis, s):
    """(eu, en) da face: ao longo (+Y nas faces de x, +X nas de y) e a normal pra fora."""
    return (Vector(Y), Vector((s, 0, 0))) if axis == 'x' else (Vector(X), Vector((0, s, 0)))


def wpt(axis, at, s, u, z, n=0.0):
    """O ponto na face (u = a coordenada absoluta ao longo dela), n metros pra fora."""
    return Vector((at + s * n, u, z)) if axis == 'x' else Vector((u, at + s * n, z))


def plate(p, m, c, size, eu, ev, en):
    """A placa da parede: a caixa SEM a face de trás (a que fica enterrada no miolo)."""
    b = p.bm(m, ub._bev(m))
    C, eu, ev, en = Vector(c), Vector(eu), Vector(ev), Vector(en)
    vs = {}
    for i in (-1, 1):
        for j in (-1, 1):
            for k in (-1, 1):
                vs[(i, j, k)] = b.verts.new(V(*(C + eu * (size[0] / 2 * i) + ev * (size[1] / 2 * j) + en * (size[2] / 2 * k))))
    q = lambda *ks: [vs[t] for t in ks]
    b.faces.new(q((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)))
    b.faces.new(q((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)))
    b.faces.new(q((1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)))
    b.faces.new(q((-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)))
    b.faces.new(q((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)))


def panel_wall(p, origin, eu, ev, en, w, h, pw=2.0, m='Body#pnl', gap=0.07, proud=0.08, embed=0.04, holes=(), min_piece=0.28,
               rows=None, row_mats=None):
    """As placas de uma parede com a junta funda (a da War Factory B: os pedaços cortados por um vão ganham a junta entre eles)."""
    O, eu, ev, en = Vector(origin), Vector(eu), Vector(ev), Vector(en)
    nu = max(1, int(math.ceil(w / pw - 1e-6)))
    us = [w * i / nu for i in range(nu + 1)]
    vs = [0.0] + [r for r in (rows or []) if 0 < r < h] + [h]
    g = gap / 2
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            a = us[i] + (g if i > 0 else 0.0)
            b = us[i + 1] - (g if i < len(us) - 2 else 0.0)
            c = vs[j] + (g if j > 0 else 0.0)
            d = vs[j + 1] - (g if j < len(vs) - 2 else 0.0)
            pieces = ub._rect_minus(a, b, c, d, holes, g)
            for (a2, b2, c2, d2) in pieces:
                if len(pieces) > 1:
                    a2, b2 = a2 + (g if a2 > a + 1e-6 else 0.0), b2 - (g if b2 < b - 1e-6 else 0.0)
                    c2, d2 = c2 + (g if c2 > c + 1e-6 else 0.0), d2 - (g if d2 < d - 1e-6 else 0.0)
                if b2 - a2 < min_piece or d2 - c2 < min_piece:
                    continue
                cc = O + eu * ((a2 + b2) / 2) + ev * ((c2 + d2) / 2) + en * ((proud - embed) / 2)
                plate(p, (row_mats or {}).get(j, m), cc, (b2 - a2, d2 - c2, proud + embed), eu, ev, en)


def clad(p, axis, at, s, a0, a1, z0, z1, holes=(), pw=2.0, zrows=(), band=None, m='Body#pnl'):
    """As placas de uma parede: as juntas nas alturas absolutas zrows, a fileira 'band' (z de baixo, z de cima) na cor do jogador;
    holes em absoluto [(u0, u1, z0, z1)]."""
    eu, en = axes(axis, s)
    rows = [zr - z0 for zr in zrows if z0 + 0.05 < zr < z1 - 0.05]
    vs = [0.0] + rows + [z1 - z0]
    rm = {}
    for j in range(len(vs) - 1):
        if band and abs(vs[j] - (band[0] - z0)) < 1e-6:
            rm[j] = 'Player#pnl'
    hs = [(u0 - a0, u1 - a0, zb - z0, zt - z0) for (u0, u1, zb, zt) in holes]
    panel_wall(p, wpt(axis, at, s, a0, z0), eu, Z, en, a1 - a0, z1 - z0, pw=pw, m=m, holes=hs, rows=rows, row_mats=rm)


def solid(p, x0, x1, y0, y1, z0, z1, m='Body#vp', skip=()):
    """O miolo (escondido atrás das placas): a caixa sem o fundo e sem os lados de 'skip'."""
    x0, x1, y0, y1 = min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1)
    b = p.bm(m, 0.0)
    vs = {(i, j, k): b.verts.new(V((x0, x1)[i], (y0, y1)[j], (z0, z1)[k])) for i in (0, 1) for j in (0, 1) for k in (0, 1)}
    q = lambda *ks: [vs[t] for t in ks]
    b.faces.new(q((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)))
    sides = {'-y': q((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)), '+y': q((1, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 1)),
             '-x': q((0, 1, 0), (0, 0, 0), (0, 0, 1), (0, 1, 1)), '+x': q((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1))}
    for k, f_ in sides.items():
        if k not in skip:
            b.faces.new(f_)


def base_run(p, axis, at, s, a0, a1, gaps=(), t_out=0.14, t_in=0.1, z1=None):
    """O rodapé de concreto ao longo da face (de a0 a a1), cortado nos vãos das portas (gaps = [(u0, u1)])."""
    z1 = Z0 + BASE_T if z1 is None else z1
    cur, segs = a0, []
    for (g0, g1) in sorted(gaps):
        segs.append((cur, g0))
        cur = g1
    segs.append((cur, a1))
    for (u0, u1) in segs:
        if u1 - u0 < 0.1:
            continue
        if axis == 'x':
            bx(p, 'Concrete#pnl', at - s * t_in, at + s * t_out, u0, u1, -0.1, z1)
        else:
            bx(p, 'Concrete#pnl', u0, u1, at - s * t_in, at + s * t_out, -0.1, z1)


def louvers(p, c, n_face, along, L, H, n, frame='Trim', slat='Trim', back='Dark', tilt=35.0):
    """A veneziana (a da War Factory B: os montantes entrando na travessa)."""
    en = Vector(n_face).normalized()
    eu = Vector(along).normalized()
    ev = en.cross(eu).normalized()
    if ev.z < 0:
        ev = -ev
    C = Vector(c)
    vp.prism(p, back + '#vp', vp.rounded_rect(L + 0.02, H + 0.02, 0.03, 2), C - en * 0.004, eu, ev, en, 0.012)
    fw = 0.05
    for sgn in (-1, 1):
        ub.obox(p, frame + '#vp', C + ev * (sgn * (H / 2 + fw / 2)) + en * 0.025, (L + 2 * fw, fw, 0.05), eu, ev, en)
        ub.obox(p, frame + '#vp', C + eu * (sgn * (L / 2 + fw / 2)) + en * 0.026, (fw - 0.006, H + fw, 0.048), eu, ev, en)
    step = H / n
    a = math.radians(tilt)
    e2 = ev * math.cos(a) + en * math.sin(a)
    e3 = en * math.cos(a) - ev * math.sin(a)
    for k in range(n):
        ub.obox(p, slat + '#vp', C + ev * (-H / 2 + step * (k + 0.5)) + en * 0.02, (L - 0.004, step * 0.85, 0.014), eu, e2, e3)


def small_fan(p, c, R, z_top, depth=0.42, blades=7):
    """O ventilador (o poço escuro, a moldura, o cubo, as pás, a grade)."""
    x, y = c
    vp.lathe(p, 'Dark#vp', (x, y, z_top), 'z', [(R - 0.002, 0.0), (R - 0.002, -depth), (0.0, -depth)], seg=24, smooth=True)
    vp.lathe(p, 'Trim#vp', (x, y, z_top), 'z', [(R - 0.03, 0.0), (R - 0.03, 0.06), (R + 0.06, 0.09), (R + 0.12, 0.0)], seg=28, smooth=True,
             closed=True)
    zf = z_top - depth * 0.45
    vp.lathe(p, 'Trim#vp', (x, y, zf - 0.12), 'z', [(0.0, 0.0), (R * 0.22, 0.0), (R * 0.2, 0.16), (0.0, 0.21)], seg=14)
    for k in range(blades):
        a = 2 * math.pi * k / blades
        d = Vector((math.cos(a), math.sin(a), 0.0))
        sv = Vector((-math.sin(a), math.cos(a), 0.0))
        tw = math.radians(28)
        e2 = sv * math.cos(tw) + Vector((0, 0, 1)) * math.sin(tw)
        e3 = d.cross(e2).normalized()
        L = R * 0.74
        ub.obox(p, 'Trim#vp', Vector((x, y, zf)) + d * (R * 0.2 + L / 2), (L, R * 0.36, 0.03), d, e2, e3)
    p.torus('Trim#vp', (x, y, z_top + 0.11), 'z', R * 0.98, 0.022, seg=28, rseg=5)
    for a in (0.0, math.pi / 2):
        d = Vector((math.cos(a), math.sin(a), 0.0))
        vp.tube(p, 'Trim', [Vector((x, y, z_top + 0.11)) - d * R, Vector((x, y, z_top + 0.11)) + d * R], 0.02, seg=6)


def fan_box(p, c, size, fans=1, m='Body', z0=0.0, R=None):
    """O condensador do teto (o da War Factory B): a caixa, os pés, as tampas com os ventiladores, as venezianas dos lados."""
    x, y = c
    sx, sy, sz = size
    t = 0.46
    ub.obox(p, m + '#pnl', (x, y, z0 + 0.08 + (sz - t + 0.02 - 0.08) / 2), (sx - 0.08, sy - 0.08, sz - t + 0.02 - 0.08), X, Y, Z)
    for sxx in (-1, 1):
        for syy in (-1, 1):
            ub.obox(p, 'Trim#vp', (x + sxx * (sx / 2 - 0.2), y + syy * (sy / 2 - 0.2), z0 + 0.05), (0.3, 0.3, 0.1), X, Y, Z)
    R = R or min(sy * 0.38, sx / (2 * fans) * 0.78)
    for k in range(fans):
        cx = x + (k - (fans - 1) / 2) * sx / fans
        ub.square_annulus(p, m + '#vp', (cx, y), sx / fans / 2 - (0.02 if fans > 1 else 0.0), sy / 2, R, z0 + sz, t)
        small_fan(p, (cx, y), R, z0 + sz, depth=t - 0.04)
    for k in range(1, fans):
        xx = x - sx / 2 + k * sx / fans
        ub.obox(p, 'Trim#vp', (xx, y, z0 + sz + 0.015), (0.1, sy - 0.02, 0.05), X, Y, Z)
    for sg in (-1, 1):
        hb = sz - t - 0.08
        louvers(p, (x, y + sg * (sy / 2 - 0.04), z0 + 0.08 + hb / 2), (0, sg, 0), (1, 0, 0), sx * 0.7, hb * 0.6, max(3, int(hb * 0.6 / 0.09)))


def rail(p, m, pts, z, h=1.05, r=0.025, step=2.0, mid=True):
    """O guarda-corpo de tubo (o da War Factory B): o corrimão, o do meio, os montantes com o pé torneado."""
    top = vp.bent_bar([Vector((x, y, z + h)) for x, y in pts], R=0.12, n=3)
    vp.tube(p, m, top, r, seg=6)
    if mid:
        vp.tube(p, m, vp.bent_bar([Vector((x, y, z + h * 0.5)) for x, y in pts], R=0.12, n=3), r * 0.8, seg=6)
    posts = []
    for (ax, ay), (bx_, by) in zip(pts[:-1], pts[1:]):
        L = math.hypot(bx_ - ax, by - ay)
        n = max(1, int(round(L / step)))
        for k in range(n + 1):
            q = (ax + (bx_ - ax) * k / n, ay + (by - ay) * k / n)
            if not any(abs(q[0] - o[0]) < 0.05 and abs(q[1] - o[1]) < 0.05 for o in posts):
                posts.append(q)
    for (x, y) in posts:
        vp.tube(p, m, [(x, y, z), (x, y, z + h)], r, seg=6)
        vp.lathe(p, m + '#vp', (x, y, z), 'z', [(0.0, 0.0), (0.06, 0.0), (0.06, 0.015), (0.035, 0.03), (0.0, 0.03)], seg=8)


def lamp_head(p, c, aim, w=0.5, h=0.32, body='Dark'):
    """O holofote: o corpo de quinas redondas virado pra 'aim', a lente acesa e a aba."""
    d = Vector(aim).normalized()
    ey = Vector((0, 0, 1)).cross(d).normalized()
    ez = d.cross(ey).normalized()
    C = Vector(c)
    vp.prism(p, body + '#vp', vp.rounded_rect(w, h, 0.04, 2), C - d * 0.11, ey, ez, d, 0.22)
    ub.obox(p, 'Light#vp', C + d * 0.115, (0.012, w - 0.08, h - 0.08), d, ey, ez)
    ub.obox(p, body + '#vp', C + d * 0.12 + ez * (h / 2 + 0.01), (0.16, w + 0.02, 0.02), d, ey, ez)


def wall_lamp(p, axis, at, s, u, z):
    """A luminária de parede virada pra baixo, com o suporte."""
    eu, en = axes(axis, s)
    lamp_head(p, wpt(axis, at, s, u, z, 0.24), en - Vector((0, 0, 1.0)), w=0.5, h=0.24)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, u, z, 0.09), (0.14, 0.24, 0.14), eu, Z, en)


def pipe(p, m, pts, r, step=2.4):
    return ub.pipe(p, m, pts, r, step=step, seg=10)


def parapet_run(p, a, b, z0, z1, t=0.3, out=0.1):
    """Um trecho de parapeito (de 'a' a 'b' na planta, a face de fora do lado esquerdo do sentido) com a capa escura."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    d = (B - A).normalized()
    n = Vector((d.y, -d.x, 0.0))          # a direita do sentido = pra fora
    L = (B - A).length
    c = (A + B) / 2 + n * (out - t / 2)
    ub.obox(p, 'Body#vp', c + Vector((0, 0, (z0 + z1) / 2)), (L, t + 0.0, z1 - z0), d, n, Z)
    ub.obox(p, 'Trim#vp', c + Vector((0, 0, z1 + 0.04)), (L + 0.08, t + 0.08, 0.08), d, n, Z)


def roof_lines(p, x0, x1, y0, y1, z, nx, ny):
    """As juntas das lajes do teto: os frisos escuros por cima."""
    dx, dy = (x1 - x0) / nx, (y1 - y0) / ny
    for i in range(1, nx):
        bx(p, 'Dark#vp', x0 + i * dx - 0.035, x0 + i * dx + 0.035, y0, y1, z - 0.01, z + 0.006)
    for j in range(1, ny):
        bx(p, 'Dark#vp', x0, x1, y0 + j * dy - 0.035, y0 + j * dy + 0.035, z - 0.01, z + 0.006)


def door(p, axis, at, s, uc, w=1.1, h=2.25, canopy=True, lamp=True, zb=Z0):
    """A porta de gente: o vão escuro, o batente, a folha com o visor e a alça, a marquise e o holofote (zb = o piso)."""
    eu, en = axes(axis, s)
    ub.obox(p, 'Dark#vp', wpt(axis, at, s, uc, zb + h / 2, 0.0), (w, h, 0.06), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.05), zb + h / 2, 0.08), (0.1, h + 0.1, 0.1), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, zb + h + 0.05, 0.08), (w + 0.2 - 0.004, 0.1, 0.1), eu, Z, en)
    ub.obox(p, 'Body#sm', wpt(axis, at, s, uc, zb + h / 2 - 0.02, 0.05), (w - 0.1, h - 0.08, 0.05), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, zb + h * 0.72, 0.08), (w - 0.3, 0.3, 0.02), eu, Z, en)
    a = wpt(axis, at, s, uc + (w / 2 - 0.18), zb + 0.95, 0.08)
    vp.grab_handle(p, 'Trim', a, a + Vector((0, 0, 0.28)), en, height=0.05, r=0.014)
    if canopy:
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, zb + h + 0.32, 0.4), (w + 0.7, 0.8, 0.08), eu, en, Z)
        for su in (-1, 1):
            q0 = wpt(axis, at, s, uc + su * (w / 2 + 0.25), zb + h + 0.0, 0.05)
            vp.tube(p, 'Trim', [q0, wpt(axis, at, s, uc + su * (w / 2 + 0.25), zb + h + 0.3, 0.72)], 0.02, seg=6)
    if lamp:
        lamp_head(p, wpt(axis, at, s, uc, zb + h + (0.16 if canopy else 0.3), 0.2 if canopy else 0.14), en - Vector((0, 0, 1.2)),
                  w=0.36, h=0.18)


def window(p, axis, at, s, u0, u1, z0, z1, mull=1):
    """A janela: o vidro recuado, o batente, os montantes, o peitoril."""
    eu, en = axes(axis, s)
    uc, zc, w, h = (u0 + u1) / 2, (z0 + z1) / 2, u1 - u0, z1 - z0
    ub.obox(p, 'Glass#vp', wpt(axis, at, s, uc, zc, 0.0), (w, h, 0.05), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.04), zc, 0.07), (0.08, h + 0.16, 0.1), eu, Z, en)
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, zc + su * (h / 2 + 0.04), 0.07), (w - 0.004, 0.08, 0.1), eu, Z, en)
    for k in range(mull):
        uu = u0 + w * (k + 1) / (mull + 1)
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uu, zc, 0.04), (0.06, h, 0.06), eu, Z, en)
    ub.obox(p, 'Body#sm', wpt(axis, at, s, uc, z0 - 0.12, 0.13), (w + 0.3, 0.1, 0.2), eu, Z, en)


def paint(p, m, x0, x1, y0, y1, lift=0.0):
    ub.flat_paint(p, m, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], Z0 + 0.002 + lift, Z0 + 0.014 + lift)


def star_pts(R, r, rot=90.0):
    """A estrela de 5 pontas (10 vértices, a ponta em 'rot' graus)."""
    return [((R if k % 2 == 0 else r) * math.cos(math.radians(rot + 36 * k)), (R if k % 2 == 0 else r) * math.sin(math.radians(rot + 36 * k)))
            for k in range(10)]


def sheet(p, m, P, ns, nt, h0, h1, smooth=True):
    """O sólido fino entre as superfícies P(s, t, h0) e P(s, t, h1) (s, t em 0..1, ns × nt células): a chapa curva, a nervura, o
    gomo. As faces de cima e de baixo lisas; os 4 lados planos (a quina que o chanfro da chave arredonda)."""
    b = p.bm(m, ub._bev(m))
    top = [[b.verts.new(V(*P(i / ns, j / nt, h1))) for j in range(nt + 1)] for i in range(ns + 1)]
    bot = [[b.verts.new(V(*P(i / ns, j / nt, h0))) for j in range(nt + 1)] for i in range(ns + 1)]
    for i in range(ns):
        for j in range(nt):
            b.faces.new([top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]]).smooth = smooth
            b.faces.new([bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j], bot[i][j]]).smooth = smooth
    for i in range(ns):
        b.faces.new([bot[i][0], bot[i + 1][0], top[i + 1][0], top[i][0]])
        b.faces.new([top[i][nt], top[i + 1][nt], bot[i + 1][nt], bot[i][nt]])
    for j in range(nt):
        b.faces.new([top[0][j], top[0][j + 1], bot[0][j + 1], bot[0][j]])
        b.faces.new([bot[ns][j], bot[ns][j + 1], top[ns][j + 1], top[ns][j]])


def nseg(L, cell=0.36):
    """Quantas células pra o lado L ficar ≤ 0,36 m (a grade do executor corta acima de 0,4 m: aqui já sai cortada)."""
    return max(1, int(math.ceil(abs(L) / cell - 1e-6)))


# ---------------------------------------------------------------------------------- o contorno do hangar (o arco alteado)
def stilt(r, z_foot, n_leg=4, n_arc=40):
    """O arco alteado de raio r (as pernas a prumo de z_foot até HS, o semicírculo em cima, centro (0, HS)), do pé −Y ao pé +Y."""
    out = []
    for i in range(n_leg):
        out.append((-r, z_foot + (HS - z_foot) * i / n_leg))
    for j in range(n_arc + 1):
        a = math.pi * (1 - j / n_arc)
        out.append((r * math.cos(a), HS + r * math.sin(a)))
    for i in range(1, n_leg + 1):
        out.append((r, HS - (HS - z_foot) * i / n_leg))
    return out


def bar_pt(u, h):
    """O ponto da casca no parâmetro u (o comprimento pelo raio HB_R, do pé −Y no topo do meio-fio ao pé +Y), h pra fora: (y, z)."""
    r = HB_R + h
    if u <= LLEG:
        return -r, ZC + u
    if u <= LLEG + SARC:
        a = math.pi - (u - LLEG) / HB_R
        return r * math.cos(a), HS + r * math.sin(a)
    return r, HS - (u - LLEG - SARC)


def barrel_band(p, m, xa, xb, ua, ub_, h0, h1, ns=None):
    """A faixa da casca de xa a xb, de ua a ub_ no contorno, de h0 a h1 pra fora (a chapa, a nervura, o forro)."""
    def P(s, t, h):
        y, z = bar_pt(ua + (ub_ - ua) * t, h)
        return (xa + (xb - xa) * s, y, z)
    sheet(p, m, P, ns or nseg(xb - xa), nseg((ub_ - ua) * (HB_R + h1) / HB_R), h0, h1)


def barrel_poly(p, m, poly, xc, ac, h0, h1, nr=3):
    """O polígono (u ao longo de X, w pra cumeeira; estrelado em volta da origem) deitado na casca no ângulo ac, de h0 a h1 pra fora
    (o disco, a estrela, o aro: a chapa que acompanha a curva, sem flutuar)."""
    b = p.bm(m, 0.0)

    def P(u, w, h):
        a = ac - w / HB_R
        return V(xc + u, (HB_R + h) * math.cos(a), HS + (HB_R + h) * math.sin(a))
    n = len(poly)
    layers = []
    for h in (h0, h1):
        c = b.verts.new(P(0.0, 0.0, h))
        rings = [[b.verts.new(P(u * (j / nr), w * (j / nr), h)) for (u, w) in poly] for j in range(1, nr + 1)]
        layers.append((c, rings))
    for li, (c, rings) in enumerate(layers):
        flip = li == 0
        for k in range(n):
            kk = (k + 1) % n
            f = [c, rings[0][k], rings[0][kk]]
            b.faces.new(f[::-1] if flip else f).smooth = True
            for j in range(nr - 1):
                f = [rings[j][k], rings[j + 1][k], rings[j + 1][kk], rings[j][kk]]
                b.faces.new(f[::-1] if flip else f).smooth = True
    ob, ot = layers[0][1][-1], layers[1][1][-1]
    for k in range(n):
        kk = (k + 1) % n
        b.faces.new([ob[k], ob[kk], ot[kk], ot[k]])


def barrel_ring(p, m, xc, ac, r0, r1, h0, h1, seg=48):
    """O aro deitado na casca (a coroa de r0 a r1 em volta do centro)."""
    b = p.bm(m, 0.0)

    def P(u, w, h):
        a = ac - w / HB_R
        return V(xc + u, (HB_R + h) * math.cos(a), HS + (HB_R + h) * math.sin(a))
    rings = []
    for (r, h) in ((r1, h0), (r1, h1), (r0, h1), (r0, h0)):
        rings.append([b.verts.new(P(r * math.cos(2 * math.pi * k / seg), r * math.sin(2 * math.pi * k / seg), h)) for k in range(seg)])
    for i in range(4):
        ra, rb = rings[i], rings[(i + 1) % 4]
        for k in range(seg):
            b.faces.new([ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k]]).smooth = i in (0, 2)


def portal_frame(p, x0, x1, m='Body#frame'):
    """O pórtico do hangar: a faixa entre o arco do vão (HO_R) e o de fora (HF_R), de x0 a x1, as pernas até o chão (o chanfro largo
    da chave pega as 4 quinas compridas)."""
    b = p.bm(m, 0.04)
    pin, pout = stilt(HO_R, Z0 - 0.02), stilt(HF_R, Z0 - 0.02)
    rings = []
    for (yi, zi), (yo, zo) in zip(pin, pout):
        rings.append([b.verts.new(V(x0, yi, zi)), b.verts.new(V(x0, yo, zo)), b.verts.new(V(x1, yo, zo)), b.verts.new(V(x1, yi, zi))])
    for i in range(len(rings) - 1):
        for e in range(4):
            b.faces.new([rings[i][e], rings[i][(e + 1) % 4], rings[i + 1][(e + 1) % 4], rings[i + 1][e]]).smooth = e in (1, 3)
    b.faces.new(rings[0][::-1])
    b.faces.new(rings[-1])


def arch_strip(p, m, x0, x1, r0, r1, a0, a1, n=24, smooth=True):
    """A faixa no plano do pórtico entre os raios r0 e r1 e os ângulos a0..a1 (graus; 90 = o alto), de x0 a x1."""
    def P(s, t, h):
        a = math.radians(a0 + (a1 - a0) * t)
        r = r0 + (r1 - r0) * s
        return (h, r * math.cos(a), HS + r * math.sin(a))
    sheet(p, m, P, 1, n, x0, x1, smooth=smooth)


# ---------------------------------------------------------------------------------- o chão: o contorno, as marcas
def ground(p):
    """SÓ o contorno de concreto em volta do prédio (nunca a laje do lote): as lajes com a junta funda; o pátio de saída na frente do
    hangar com as faixas amarelas; o pátio cercado; as calçadas; as 3 estrelas pintadas na frente da entrada."""
    pads = [
        (-10.0, XH1, YH0 - 1.0, YH1 + 0.9, 6, 4),        # o galpão e as calçadas dos fundos e de fora (o miolo fica embaixo)
        (-10.0, HX1, -4.4, YH0 - 1.0, 4, 3),             # o hangar e a calçada de trás
        (-10.0, HX1, FY - 0.3, -4.4, 4, 2),              # o pátio cercado
        (HX1, 6.4, -HF_R - 0.6, YH0 - 1.0, 2, 3),        # o pátio de saída: só a faixa na frente do portão (sem laje na frente da cerca)
        (XH1, 8.6, YH0 - 1.0, YE1, 1, 2),                # a calçada da entrada
        (XH1, 9.6, YE1, YH1 + 0.9, 1, 2),                # o pé da torre (a placa fica no chão, no pé de concreto dela)
    ]
    for (x0, x1, y0, y1, nx, ny) in pads:
        # (3 cm de folga entre os pátios: com o vértice no mesmo lugar o remove_doubles soldava as lajes e o chanfro estourava)
        slab_tiles(p, x0 + 0.03, x1 - 0.03, y0 + 0.03, y1 - 0.03, nx, ny, z0=-0.12, z1=Z0, joint=0.06)
    # as faixas amarelas do pátio de saída (as bordas da faixa de 6,6 m e o eixo tracejado) e a soleira de perigo no portão
    for y in (-3.3, 3.3):
        paint(p, 'Yellow', HX1 + 0.15, 6.25, y - 0.08, y + 0.08)
    x = HX1 + 0.6
    while x + 0.9 < 6.3:
        paint(p, 'Yellow', x, x + 0.9, -0.07, 0.07)
        x += 1.6
    # dentro do hangar: o piso escuro (o vão lê escuro como o portão do original) e a linha do eixo
    ub.flat_paint(p, 'Trim', [(XB0 + 0.05, -HB_R + 0.12), (HX1 - 0.04, -HB_R + 0.12), (HX1 - 0.04, HB_R - 0.12), (XB0 + 0.05, HB_R - 0.12)],
                  Z0 + 0.001, Z0 + 0.006)
    x = XB0 + 0.8
    while x + 0.9 < HX1 - HF_D - 0.2:
        paint(p, 'Yellow', x, x + 0.9, -0.07, 0.07)
        x += 1.6
    # a faixa tracejada do original na frente do galpão (do canto da entrada ao portão)
    x = HX1 + 0.4
    while x + 0.8 < XH1 - 0.2:
        paint(p, 'Yellow', x, x + 0.8, YH0 - 0.62, YH0 - 0.48)
        x += 1.5
    # as 3 estrelas na frente da entrada (as do original, escuras)
    for y in (DOOR_Y + 2.1, DOOR_Y + 2.75, DOOR_Y + 3.4):
        ub.flat_paint(p, 'Dark', [(XH1 + 0.85 + v, y + u) for (u, v) in star_pts(0.27, 0.11)], Z0 + 0.002, Z0 + 0.014)


# ---------------------------------------------------------------------------------- o galpão
def hall(p):
    # o miolo: o galpão com a entrada, o canto baixo da frente (+Y), o bloco de rádio
    solid(p, XH0, XH1, YH0, YE1, -0.1, EAVE)
    solid(p, XR1, XN, YE1, YH1, -0.1, EAVE, skip=('-y',))
    solid(p, XH0, XR1, YE1, YH1, -0.1, ZR, skip=('-y',))
    # o rodapé de concreto (as faces de y de quina a quina, as de x param 12 cm antes; os pés dos pilares cobrem as quinas), cortado
    # nas portas
    base_run(p, 'y', YH0, -1, XH0 - 0.14, XH1 - 0.3)
    base_run(p, 'x', XH0, -1, YH0 + 0.12, YH1 - 0.12, gaps=[(6.55, 9.25), (11.2, 12.5)])
    base_run(p, 'y', YH1, 1, XH0 - 0.14, XN + 0.14, gaps=[(1.5, 2.8)])
    base_run(p, 'x', XN, 1, YE1 + 0.12, YH1 - 0.12)
    base_run(p, 'y', YE1, 1, XN + 0.12, XH1 + 0.14)
    base_run(p, 'x', XH1, 1, WIN_E[0] - 0.15, YE1 - 0.12)
    zr = [Z0 + 1.9, Z0 + 3.2, BAND[0]]
    z0 = Z0 + BASE_T + 0.01
    # a face do lado do hangar (−Y): as placas, a faixa azul no beiral (os pilares de pé por cima)
    clad(p, 'y', YH0, -1, XH0 + 0.3, XH1 - 0.3, z0, EAVE, pw=2.0, zrows=zr, band=BAND)
    # os fundos (−X): o galpão e o bloco de rádio (a porta de enrolar, a de gente)
    clad(p, 'x', XH0, -1, YH0 + 0.3, YE1 - 0.05, z0, EAVE, pw=1.8, zrows=zr, band=BAND, holes=[(6.6, 9.2, z0, Z0 + 3.3)])
    zr_r = [Z0 + 1.9, Z0 + 3.2, BAND[0], EAVE + 1.25]
    clad(p, 'x', XH0, -1, YE1 + 0.05, YH1 - 0.3, z0, ZR, pw=1.7, zrows=zr_r, band=None, holes=[(11.3, 12.4, z0, Z0 + 2.3)])
    # a face de fora (+Y): o bloco de rádio (até o teto dele) e o canto baixo da frente
    clad(p, 'y', YH1, 1, XH0 + 0.3, XR1 - 0.05, z0, ZR, pw=2.1, zrows=zr_r, band=None,
         holes=[(-7.6, -6.0, Z0 + 1.4, Z0 + 2.3), (-4.2, -2.6, Z0 + 1.4, Z0 + 2.3), (-7.6, -6.0, 5.55, 6.45), (-4.2, -2.6, 5.55, 6.45)])
    clad(p, 'y', YH1, 1, XR1 + 0.05, XN - 0.3, z0, EAVE, pw=1.6, zrows=zr, band=BAND, holes=[(1.6, 2.7, z0, Z0 + 2.3)])
    # o recorte (+X, atrás da torre) e o lado +Y da entrada
    clad(p, 'x', XN, 1, YE1 + 0.3, YH1 - 0.3, z0, EAVE, pw=1.4, zrows=zr, band=BAND, holes=[(11.6, 12.8, Z0 + 1.4, Z0 + 2.3)])
    clad(p, 'y', YE1, 1, XN + 0.3, XH1 - 0.3, z0, EAVE, pw=1.1, zrows=zr, band=BAND)
    # a frente da entrada (+X): o portão grande entre os pilares escuros
    y0d, y1d = DOOR_Y - DOOR_W / 2, DOOR_Y + DOOR_W / 2
    clad(p, 'x', XH1, 1, YH0 + 0.3, YE1 - 0.3, z0, EAVE, pw=1.3, zrows=zr, band=BAND,
         holes=[(y0d - 0.35, y1d + 0.35, z0, Z0 + DOOR_H + 0.35), (WIN_E[0], WIN_E[1], Z0 + 1.4, Z0 + 2.3)])
    # o bloco de rádio acima do teto: a frente (+X) e o lado do teto plano (−Y)
    clad(p, 'x', XR1, 1, YE1 + 0.05, YH1 - 0.3, EAVE + 0.02, ZR, pw=1.5, zrows=[EAVE + 1.25], band=None,
         holes=[(11.2, 12.9, 5.55, 6.45)])
    clad(p, 'y', YE1, -1, XH0 + 0.3, XR1 - 0.3, EAVE + 0.02, ZR, pw=2.1, zrows=[EAVE + 1.25], band=None,
         holes=[(-7.4, -5.8, 5.55, 6.45), (-3.6, -2.0, 5.55, 6.45)])
    # os pilares de pé (as faixas verticais do original): o fuste de placa, a capa, o pé escuro
    pil = [(x, YH0, -1, 'y') for x in (XH0 + 3.2, XH0 + 6.4, XH0 + 9.6, XH0 + 12.8)]
    pil += [(x, YH1, 1, 'y') for x in (XH0 + 4.3,)]
    for (u, at, s, axis) in pil:
        eu, en = axes(axis, s)
        top = EAVE + 0.12 if at == YH0 else ZR + 0.12
        ub.obox(p, 'Body#pil', wpt(axis, at, s, u, (Z0 + top) / 2, 0.11), (0.5, top - Z0, 0.26), eu, Z, en)
        ub.obox(p, 'Trim#pil', wpt(axis, at, s, u, Z0 + 0.42, 0.17), (0.66, 0.9, 0.38), eu, Z, en)
    # as quinas: os pilares de quina (o ritmo de pé; cobrem as pontas das placas)
    for (x, y, sx, sy, z1, m) in ((XH0, YH0, -1, -1, EAVE + 0.4, 'Body#pil'), (XH0, YH1, -1, 1, ZR + 0.4, 'Body#pil'),
                                  (XN, YH1, 1, 1, EAVE + 0.4, 'Body#pil'), (XH1, YE1, 1, 1, EAVE + 0.4, 'Body#pil'),
                                  (XR1, YH1, 1, 1, ZR + 0.4, 'Body#pil')):
        bx(p, m, x - 0.32 + sx * 0.06, x + 0.32 + sx * 0.06, y - 0.32 + sy * 0.06, y + 0.32 + sy * 0.06, Z0 - 0.02, z1)
        bx(p, 'Trim#pil', x - 0.4 + sx * 0.06, x + 0.4 + sx * 0.06, y - 0.4 + sy * 0.06, y + 0.4 + sy * 0.06, Z0 - 0.02, Z0 + 0.9)
    # as quinas do bloco de rádio acima do teto do galpão; a quina de dentro do recorte
    for (x, y) in ((XR1, YE1), (XH0, YE1)):
        bx(p, 'Body#pil', x - 0.28, x + 0.28, y - 0.28, y + 0.28, EAVE - 0.05, ZR + 0.4)
    bx(p, 'Body#pil', XN - 0.02, XN + 0.36, YE1 - 0.02, YE1 + 0.36, Z0 - 0.02, EAVE + 0.4)


def entrance(p):
    """A frente da entrada (+X): o portão grande escuro de 2 folhas (as almofadas, o batente pesado, a verga), os pilares escuros dos
    lados (o da quina e o do meio), a placa azul, o holofote, a janelinha; as 2 claraboias do teto."""
    y0d, y1d = DOOR_Y - DOOR_W / 2, DOOR_Y + DOOR_W / 2
    xf = XH1
    # as 2 folhas na face do miolo (o batente e as placas em volta ficam na frente: a porta lê recuada), as almofadas (2 × 3 por
    # folha), a fresta escura do meio, as alças, o chute de perigo
    lw = DOOR_W / 2 - 0.03
    for k in (0, 1):
        yc = y0d + 0.015 + lw / 2 + k * (lw + 0.03)
        ub.obox(p, 'Trim#sm', (xf + 0.03, yc, Z0 + DOOR_H / 2), (0.06, lw, DOOR_H - 0.02), X, Y, Z)
        for i in range(2):
            for j in range(3):
                yy = yc - lw / 2 + 0.12 + (lw - 0.24) * (i + 0.5) / 2
                zz = Z0 + 0.18 + (DOOR_H - 0.36) * (j + 0.5) / 3
                ub.obox(p, 'Dark#vp', (xf + 0.066, yy, zz), (0.012, (lw - 0.24) / 2 - 0.1, (DOOR_H - 0.36) / 3 - 0.12), X, Y, Z)
        yh = DOOR_Y + (0.22 if k else -0.22)
        vp.grab_handle(p, 'Trim', (xf + 0.075, yh, Z0 + 1.0), (xf + 0.075, yh, Z0 + 1.6), (1, 0, 0), height=0.07, r=0.02)
    ub.obox(p, 'Dark#vp', (xf + 0.03, DOOR_Y, Z0 + DOOR_H / 2), (0.05, 0.03, DOOR_H), X, Y, Z)
    ub.obox(p, 'Hazard#vp', (xf + 0.07, DOOR_Y, Z0 + 0.09), (0.02, DOOR_W - 0.06, 0.12), X, Y, Z)
    # o batente e a verga
    for s in (-1, 1):
        ub.obox(p, 'Trim#sm', (xf + 0.04, DOOR_Y + s * (DOOR_W / 2 + 0.16), Z0 + (DOOR_H + 0.25) / 2), (0.24, 0.32, DOOR_H + 0.25), X, Y, Z)
    ub.obox(p, 'Trim#sm', (xf + 0.05, DOOR_Y, Z0 + DOOR_H + 0.18), (0.26, DOOR_W + 0.64, 0.36), X, Y, Z)
    # os pilares escuros dos lados (o da quina −Y e o do meio), com o pé
    for yc, xa in ((YH0 + 0.02, xf - 0.4), (y1d + 0.62, xf - 0.05)):
        bx(p, 'Trim#pil', xa, xf + 0.32, yc - 0.32, yc + 0.32, Z0 - 0.02, EAVE + 0.3)
        bx(p, 'Concrete#pnl', xa - 0.05, xf + 0.42, yc - 0.42, yc + 0.42, Z0 - 0.02, Z0 + 0.75)
    # a placa azul em cima da verga e o holofote
    ub.obox(p, 'Player#sm', (xf + 0.1, DOOR_Y, Z0 + DOOR_H + 0.62), (0.06, 2.2, 0.36), X, Y, Z)
    lamp_head(p, (xf + 0.32, DOOR_Y, EAVE - 0.35), (1.0, 0.0, -1.3), w=0.5, h=0.24)
    ub.obox(p, 'Trim#vp', (xf + 0.12, DOOR_Y, EAVE - 0.35), (0.2, 0.2, 0.14), X, Y, Z)
    window(p, 'x', xf, 1, WIN_E[0], WIN_E[1], Z0 + 1.4, Z0 + 2.3, mull=0)
    # as 2 claraboias escuras do teto da entrada
    for y in (9.2, 10.0):
        bx(p, 'Body#sm', XH1 - 1.15, XH1 - 0.45, y - 0.32, y + 0.32, EAVE - 0.05, EAVE + 0.22)
        ub.obox(p, 'Glass#vp', (XH1 - 0.8, y, EAVE + 0.235), (0.6, 0.54, 0.03), X, Y, Z)


# ---------------------------------------------------------------------------------- o teto curvo e o teto plano
def vault_segment(p, xa, xb, m='Paint#vault', nphi=20, yc=None, zc=None, R=None, Rz=None):
    """Um gomo da abóbada: a meia-elipse (meia-largura R, altura Rz) de xa a xb sobre o teto (o fundo no teto, as pontas planas), a
    quina redonda nas pontas (o padrão é o teto curvo do galpão)."""
    yc = YV if yc is None else yc
    zc = EAVE if zc is None else zc
    R = RV if R is None else R
    Rz = (RVZ if R == RV else R) if Rz is None else Rz
    b = p.bm(m, 0.04)
    nx = nseg(xb - xa)
    g = [[b.verts.new(V(xa + (xb - xa) * i / nx, yc + R * math.cos(math.pi * j / nphi), zc + Rz * math.sin(math.pi * j / nphi)))
          for j in range(nphi + 1)] for i in range(nx + 1)]
    for i in range(nx):
        for j in range(nphi):
            b.faces.new([g[i][j], g[i + 1][j], g[i + 1][j + 1], g[i][j + 1]]).smooth = True
    b.faces.new([g[0][j] for j in range(nphi + 1)])
    b.faces.new([g[nx][j] for j in range(nphi + 1)][::-1])
    b.faces.new([g[i][0] for i in range(nx + 1)] + [g[i][nphi] for i in range(nx, -1, -1)])


def vault_nose(p, xe, d, m='Paint#vault', nth=8, nphi=20):
    """A ponta em nariz (o quarto de esfera) da meia-cana em xe, pra fora no sentido d (±1)."""
    b = p.bm(m, 0.04)
    pole = b.verts.new(V(xe + d * RV, YV, EAVE))
    g = [[pole] * (nphi + 1)]
    for i in range(1, nth + 1):
        th = (math.pi / 2) * i / nth
        g.append([b.verts.new(V(xe + d * RV * math.cos(th), YV + RV * math.sin(th) * math.cos(math.pi * j / nphi),
                                EAVE + RVZ * math.sin(th) * math.sin(math.pi * j / nphi))) for j in range(nphi + 1)])
    for i in range(nth):
        for j in range(nphi):
            if i == 0:
                b.faces.new([pole, g[1][j], g[1][j + 1]]).smooth = True
            else:
                b.faces.new([g[i][j], g[i + 1][j], g[i + 1][j + 1], g[i][j + 1]]).smooth = True
    b.faces.new([g[nth][j] for j in range(nphi + 1)])
    b.faces.new([g[i][0] for i in range(nth, 0, -1)] + [pole] + [g[i][nphi] for i in range(1, nth + 1)])


def roofs(p):
    """O teto curvo em 6 gomos com as pontas em nariz, a calha do beiral com as descidas; o teto plano: as juntas, os parapeitos, os 2
    ventiladores redondos, o duto comprido com o guarda-corpo, a veneziana; o teto do bloco de rádio: o parapeito, as caixas de
    ar-condicionado, o armário de rádio, as 5 antenas de chicote, as balizas, o guarda-corpo, a escada."""
    n = 6
    g = 0.045
    for k in range(n):
        xa = XV0 + (XV1 - XV0) * k / n + g / 2
        xb = XV0 + (XV1 - XV0) * (k + 1) / n - g / 2
        vault_segment(p, xa, xb)
    vault_nose(p, XV0 - g / 2, -1)
    vault_nose(p, XV1 + g / 2, 1)
    # as bordas dos gomos em relevo (a costura das chapas do original: as listras atravessadas que leem de cima)
    for k in range(n + 1):
        xj = XV0 + (XV1 - XV0) * k / n

        def P(s, t, h, xj=xj):
            a = math.pi * t
            return (xj - 0.09 + 0.18 * s, YV + (RV + h) * math.cos(a), EAVE + (RVZ + h) * math.sin(a))
        sheet(p, 'Body#rib', P, 1, 24, -0.01, 0.055)
    # o pé da meia-cana: a chapa de vedação (o rufo) dos 2 lados
    for y in (YV - RV - 0.06, YV + RV + 0.06):
        bx(p, 'Trim#vp', XH0 + 0.1, XH1 - 0.1, y - 0.06, y + 0.06, EAVE - 0.02, EAVE + 0.1)
    # a calha do beiral (−Y) e as 2 descidas
    bx(p, 'Trim#sm', XH0 + 0.3, XH1 - 0.45, YH0 - 0.22, YH0 + 0.12, EAVE - 0.06, EAVE + 0.16)
    for x in (XH0 + 1.6, XH1 - 1.0):
        q = Vector((x, YH0 - 0.2, 0.0))
        pipe(p, 'Trim', [(q.x, q.y + 0.1, EAVE), (q.x, q.y, EAVE - 0.3), (q.x, q.y, Z0 + 0.35), (q.x, q.y - 0.3, Z0 + 0.12)], 0.07, step=1.4)
    # o teto plano: as juntas (entre a meia-cana e o bloco de rádio; o canto da frente)
    roof_lines(p, XH0 + 0.3, XH1 - 0.35, YV + RV + 0.15, YE1 - 0.1, EAVE, 6, 1)
    roof_lines(p, XR1 + 0.25, XN - 0.35, YE1 + 0.1, YH1 - 0.35, EAVE, 2, 2)
    # os parapeitos do teto plano (a frente, os fundos, o recorte, o lado de fora do canto baixo)
    zp0, zp1 = EAVE - 0.1, EAVE + 0.36
    parapet_run(p, (XH1, YV + RV + 0.14), (XH1, YE1), zp0, zp1)
    parapet_run(p, (XH1, YE1), (XN + 0.0, YE1), zp0, zp1)
    parapet_run(p, (XN, YE1 + 0.05), (XN, YH1), zp0, zp1)
    parapet_run(p, (XN - 0.05, YH1), (XR1 + 0.3, YH1), zp0, zp1)
    parapet_run(p, (XH0, YE1 - 0.02), (XH0, YV + RV + 0.14), zp0, zp1)
    # os 2 ventiladores redondos (a carcaça torneada clara com o ventilador em cima: os do original)
    for x in (-3.0, 0.4):
        c = (x, 9.15)
        vp.lathe(p, 'Body#vp', (c[0], c[1], EAVE), 'z', [(0.0, 0.0), (0.55, 0.0), (0.55, 0.06), (0.5, 0.1), (0.5, 0.62), (0.53, 0.66),
                                                         (0.53, 0.72), (0.0, 0.72)], seg=28)
        for z in (0.24, 0.44):
            p.torus('Trim#vp', (c[0], c[1], EAVE + z), 'z', 0.505, 0.018, seg=28, rseg=5)
        small_fan(p, c, 0.42, EAVE + 0.72, depth=0.3)
    # o duto comprido do original (a meia-cana pequena de gomos claros no embasamento de chapa) e o guarda-corpo do lado do teto
    yd0, yd1 = 9.95, 10.5
    xd0, xd1 = XH0 + 0.5, XN - 0.2
    for x in (xd0 + 0.4, -5.0, -1.6, 1.8, xd1 - 0.4):
        bx(p, 'Trim#vp', x - 0.08, x + 0.08, yd0 + 0.04, yd1 - 0.04, EAVE - 0.02, EAVE + 0.08)
    bx(p, 'Body#pnl', xd0, xd1, yd0, yd1, EAVE + 0.08, EAVE + 0.26)
    nd = 6
    for k in range(nd):
        vault_segment(p, xd0 + 0.05 + (xd1 - xd0 - 0.1) * k / nd + 0.02, xd0 + 0.05 + (xd1 - xd0 - 0.1) * (k + 1) / nd - 0.02, nphi=12,
                      yc=(yd0 + yd1) / 2, zc=EAVE + 0.26, R=0.24)
    rail(p, 'Trim', [(XH0 + 0.6, 9.82), (XN - 0.4, 9.82)], EAVE, h=1.0, step=2.2, mid=True)
    # a caixa da veneziana do teto plano (a grelha do original)
    bx(p, 'Body#pnl', 2.6, 3.8, 8.75, 9.45, EAVE - 0.05, EAVE + 0.45)
    louvers(p, (3.2, 8.75 - 0.005, EAVE + 0.2), (0, -1, 0), (1, 0, 0), 0.9, 0.3, 4)
    ub.obox(p, 'Trim#vp', (3.2, 9.1, EAVE + 0.47), (1.1, 0.6, 0.04), X, Y, Z)
    # o bloco de rádio: o parapeito, as juntas, as caixas de ar-condicionado, o armário, as antenas, as balizas, o guarda-corpo
    zq0, zq1 = ZR - 0.1, ZR + 0.38
    parapet_run(p, (XH0, YH1), (XH0, YE1), zq0, zq1)
    parapet_run(p, (XH0, YE1), (XR1, YE1), zq0, zq1)
    parapet_run(p, (XR1, YE1), (XR1, YH1), zq0, zq1)
    parapet_run(p, (XR1, YH1), (XH0, YH1), zq0, zq1)
    roof_lines(p, XH0 + 0.3, XR1 - 0.3, YE1 + 0.3, YH1 - 0.3, ZR, 3, 2)
    fan_box(p, (-6.4, 12.15), (2.2, 1.3, 0.95), fans=2, z0=ZR)
    fan_box(p, (-2.4, 12.3), (1.3, 1.0, 0.8), fans=1, z0=ZR)
    ub.obox(p, 'Body#pnl', (-3.95, 12.9, ZR + 0.62), (1.0, 0.8, 1.2), X, Y, Z)
    louvers(p, (-3.95, 12.9 - 0.405, ZR + 0.66), (0, -1, 0), (1, 0, 0), 0.6, 0.6, 6)
    ub.obox(p, 'Player#sm', (-3.95 + 0.51, 12.9, ZR + 0.95), (0.03, 0.6, 0.2), X, Y, Z)
    ub.antenna(p, -3.75, 13.1, ZR + 1.22, 4.2, r=0.07)
    ub.beacon(p, (-3.75, 13.1, ZR + 1.22 + 4.24), r=0.06, h=0.12, cage=False)
    for (x, y, h) in ((-8.5, 11.0, 5.6), (-8.4, 13.15, 4.2), (-0.95, 11.6, 3.4), (-1.0, 13.15, 4.8)):
        ub.antenna(p, x, y, ZR, h, r=0.07)
        ub.beacon(p, (x, y, ZR + h + 0.04), r=0.06, h=0.12, cage=False)
    for (x, y) in ((XH0 + 0.25, YE1 + 0.25), (XR1 - 0.25, YH1 - 0.25)):
        ub.beacon(p, (x, y, zq1 + 0.08), r=0.08, h=0.17)
    rail(p, 'Trim', [(-7.9, YE1 + 0.35), (-0.9, YE1 + 0.35)], ZR, h=1.0, step=2.0, mid=False)
    # a escada do teto do galpão ao do bloco de rádio (na frente dele)
    ub.ladder(p, 'Trim', XR1, 11.0, EAVE, ZR + 0.38, (1, 0, 0), w=0.5, cage=False, wall=0.18, top_ext=0.8)
    # o canto baixo da frente: o exaustor e a escotilha
    ub.turbine_vent(p, 1.0, 12.4, EAVE, r=0.32)
    bx(p, 'Body#pnl', 2.6, 3.5, 11.7, 12.6, EAVE - 0.05, EAVE + 0.35)
    ub.obox(p, 'Body#sm', (3.08, 12.15, EAVE + 0.38), (0.8, 0.82, 0.06), X, Y, Z)


def hall_details(p):
    """O lado do hangar (−Y): a escada de marinheiro que sobe a parede e passa por cima do teto curvo, as luminárias, a caixa escura de
    equipamento (o gerador) entre o galpão e o hangar com os canos; os fundos e o lado de fora: as portas, as janelas, as luminárias."""
    # a escada: a reta na parede até o beiral e a curva por cima da meia-cana até a cumeeira
    xl = 2.2
    ub.ladder(p, 'Trim', xl, YH0 - 0.08, Z0, EAVE + 0.05, (0, -1, 0), w=0.5, cage=True, wall=0.25, top_ext=0.0)
    arc = [Vector((0, YH0 - 0.33, EAVE + 0.05)), Vector((0, YH0 - 0.3, EAVE + 0.3))]
    rr, rz = RV + 0.14, RVZ + 0.14             # (a abóbada é meia-elipse: o trilho 14 cm acima dela)
    arc += [Vector((0, YV + rr * math.cos(math.radians(a)), EAVE + rz * math.sin(math.radians(a)))) for a in range(160, 84, -8)]
    for s in (-1, 1):
        vp.tube(p, 'Trim', [q + Vector((xl + s * 0.25, 0, 0)) for q in arc], 0.025, seg=8)
    for a in range(158, 92, -10):
        q = Vector((0, YV + rr * math.cos(math.radians(a)), EAVE + rz * math.sin(math.radians(a))))
        vp.tube(p, 'Trim', [q + Vector((xl - 0.25, 0, 0)), q + Vector((xl + 0.25, 0, 0))], 0.018, seg=6)
    for a in (150, 115):
        q = Vector((0, YV + RV * math.cos(math.radians(a)), EAVE + RVZ * math.sin(math.radians(a))))
        q2 = Vector((0, YV + rr * math.cos(math.radians(a)), EAVE + rz * math.sin(math.radians(a))))
        for s in (-1, 1):
            vp.tube(p, 'Trim', [q + Vector((xl + s * 0.25, 0, 0)), q2 + Vector((xl + s * 0.25, 0, 0))], 0.02, seg=6)
    # as luminárias da face do hangar
    for x in (XH0 + 4.8, XH0 + 14.4):
        wall_lamp(p, 'y', YH0, -1, x, EAVE - 0.9)
    # a caixa de equipamento (o gerador) na passagem, entre 2 pilares: o corpo escuro, as venezianas, a tampa com o ventilador, os
    # canos pra parede
    gx0, gx1, gy0, gy1, gz = -5.25, -3.1, 4.5, 5.2, 2.1
    bx(p, 'Trim#pnl', gx0, gx1, gy0, gy1, Z0 - 0.02, gz)
    bx(p, 'Trim#vp', gx0 - 0.06, gx1 + 0.06, gy0 - 0.06, gy1 + 0.06, Z0 - 0.02, Z0 + 0.16)
    for xx in (gx0 + 0.65, gx1 - 0.65):
        louvers(p, (xx, gy0 - 0.005, Z0 + 1.15), (0, -1, 0), (1, 0, 0), 0.9, 1.1, 9)
    louvers(p, (gx0 - 0.005, (gy0 + gy1) / 2, Z0 + 1.2), (-1, 0, 0), (0, 1, 0), 0.45, 0.9, 7)
    ub.obox(p, 'Paint#vp', (gx1 + 0.005, (gy0 + gy1) / 2, Z0 + 1.7), (0.02, 0.4, 0.12), X, Y, Z)
    small_fan(p, ((gx0 + gx1) / 2, (gy0 + gy1) / 2), 0.28, gz + 0.06, depth=0.25)
    ub.square_annulus(p, 'Trim#vp', ((gx0 + gx1) / 2, (gy0 + gy1) / 2), 0.36, 0.33, 0.28, gz + 0.06, 0.08)
    for (z, r) in ((1.0, 0.09), (1.5, 0.06)):
        pipe(p, 'Dark', [(gx1 - 0.3, gy1 - 0.05, z), (gx1 - 0.3, YH0 - 0.04, z)], r, step=1.0)
    pipe(p, 'Trim', [(gx0 + 0.3, gy0 + 0.35, gz), (gx0 + 0.3, gy0 + 0.35, gz + 0.5), (gx0 + 0.3, YH0 - 0.04, gz + 0.5)], 0.08, step=1.0)
    # os fundos: a porta de enrolar e a de gente; as luminárias
    eu, en = axes('x', -1)
    yc, w, h = 7.9, 2.4, 3.0
    ub.obox(p, 'Dark#vp', wpt('x', XH0, -1, yc, Z0 + h / 2), (w, h, 0.06), eu, Z, en)
    zz = Z0 + 0.3
    while zz < Z0 + h - 0.15:
        ub.obox(p, 'Trim#vp', wpt('x', XH0, -1, yc, zz, 0.05), (w - 0.1, 0.24, 0.04), eu, Z, en)
        zz += 0.3
    ub.obox(p, 'Body#pnl', wpt('x', XH0, -1, yc, Z0 + h + 0.26, 0.2), (w + 0.4, 0.5, 0.4), eu, Z, en)
    ub.obox(p, 'Hazard#vp', wpt('x', XH0, -1, yc, Z0 + 0.09, 0.07), (w - 0.1, 0.14, 0.04), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt('x', XH0, -1, yc + su * (w / 2 + 0.07), Z0 + h / 2, 0.08), (0.14, h + 0.05, 0.16), eu, Z, en)
    door(p, 'x', XH0, -1, 11.85)
    wall_lamp(p, 'x', XH0, -1, 6.0, EAVE - 0.9)
    # o lado de fora (+Y): as janelas, a porta, as luminárias, o conduíte
    for (u0, u1, z0, z1) in ((-7.6, -6.0, Z0 + 1.4, Z0 + 2.3), (-4.2, -2.6, Z0 + 1.4, Z0 + 2.3), (-7.6, -6.0, 5.55, 6.45), (-4.2, -2.6, 5.55, 6.45)):
        window(p, 'y', YH1, 1, u0, u1, z0, z1)
    door(p, 'y', YH1, 1, 2.15)
    wall_lamp(p, 'y', YH1, 1, -1.5, EAVE - 0.9)
    pipe(p, 'Dark', [(0.4, YH1 + 0.18, Z0 + 0.6), (0.4, YH1 + 0.18, EAVE - 0.4), (0.4, YH1 + 0.18, EAVE + 0.3)], 0.05)
    # o recorte (atrás da torre) e o lado +Y da entrada: as janelas; o bloco de rádio: as janelas de cima
    window(p, 'x', XN, 1, 11.6, 12.8, Z0 + 1.4, Z0 + 2.3)
    window(p, 'x', XR1, 1, 11.2, 12.9, 5.55, 6.45)
    window(p, 'y', YE1, -1, -7.4, -5.8, 5.55, 6.45)
    window(p, 'y', YE1, -1, -3.6, -2.0, 5.55, 6.45)


# ---------------------------------------------------------------------------------- o hangar
def hangar(p):
    """O hangar meia-lua: os 2 pórticos, a casca de chapas, as nervuras, o meio-fio, o forro, a parede de trás, a estrela."""
    # os pórticos (o da frente aberto: o portão) e o pé deles: escuro atrás, listrado de perigo no portão (o pé dos pilares da War
    # Factory aprovada)
    for (x0, x1, m) in ((HX0, HX0 + HF_D, 'Trim#pnl'), (HX1 - HF_D, HX1, 'Hazard#pnl')):
        portal_frame(p, x0, x1)
        for s in (-1, 1):
            bx(p, m, x0 - 0.08, x1 + 0.08, s * (HO_R - 0.06), s * (HF_R + 0.14), Z0 - 0.02, Z0 + 0.95)
    # o meio-fio de concreto em que a casca senta (entre os pórticos)
    for s in (-1, 1):
        bx(p, 'Concrete#pnl', XB0 + 0.03, XB1 - 0.03, s * (HB_R - 0.005), s * (HB_R + 0.3), Z0 - 0.02, ZC)
    # a casca: as chapas curvas (4 colunas × 8) com a fresta; o forro escuro por dentro (aparece nas frestas e pela boca)
    cols = [(XB0, RIBS[0]), (RIBS[0], RIBS[1]), (RIBS[1], STAR_X), (STAR_X, XB1)]
    g = 0.05
    nt = 8
    for ci, (xa, xb) in enumerate(cols):
        xa2 = xa + (g / 2 if ci else 0.0)
        xb2 = xb - (g / 2 if ci < len(cols) - 1 else 0.0)
        for k in range(nt):
            ua = STOT * k / nt + (g / 2 if k else 0.0)
            ub_ = STOT * (k + 1) / nt - (g / 2 if k < nt - 1 else 0.0)
            barrel_band(p, 'Body#shell', xa2, xb2, ua, ub_, 0.0, SH_T)
    barrel_band(p, 'Trim#vp', XB0, XB1, -(ZC - Z0) + 0.02, STOT + (ZC - Z0) - 0.02, -0.07, -0.012, ns=nseg(XB1 - XB0))
    # as nervuras (o arco de perfil por fora) e o pé delas no meio-fio
    for x in RIBS:
        barrel_band(p, 'Body#rib', x - 0.11, x + 0.11, 0.0, STOT, SH_T - 0.01, SH_T + 0.11, ns=1)
        for s in (-1, 1):
            bx(p, 'Trim#vp', x - 0.2, x + 0.2, s * (HB_R - 0.02), s * (HB_R + 0.38), ZC - 0.04, ZC + 0.08)
    # a parede de trás (dentro do pórtico de trás): o miolo escuro e as placas por fora, a porta
    xw = HX0 + 0.35
    pts = [(y, z) for (y, z) in stilt(HO_R + 0.02, Z0 - 0.02)]
    vp.prism(p, 'Trim#vp', pts, Vector((xw - 0.1, 0.0, 0.0)), Vector(Y), Vector(Z), Vector(X), 0.2)
    ncol = 8
    cw = 2 * HO_R / ncol
    for i in range(ncol):
        u0, u1 = -HO_R + i * cw + 0.035, -HO_R + (i + 1) * cw - 0.035
        um = max(abs(u0), abs(u1))
        top = HS + math.sqrt(max(0.0, HO_R ** 2 - um ** 2)) - 0.08
        holes = [(2.0 - u0, 3.1 - u0, 0.0, 2.25)]
        rows = [r for r in (1.3, 2.6, 3.9) if r < top - Z0 - 0.3]
        panel_wall(p, (xw - 0.1, u0, Z0), Y, Z, (-1, 0, 0), u1 - u0, top - Z0, pw=10.0, holes=holes, rows=rows)
    door(p, 'x', xw - 0.1, -1, 2.55, canopy=False)
    # por dentro: o portão de enrolar fechado do fundo, a porta, as luzes do teto, a luminária do fundo
    eu, en = Vector(Y), Vector(X)
    ub.obox(p, 'Dark#vp', (xw + 0.11, -0.2, Z0 + 1.6), (0.02, 3.0, 3.2), X, Y, Z)
    zz = Z0 + 0.3
    while zz < Z0 + 3.0:
        ub.obox(p, 'Trim#vp', (xw + 0.13, -0.2, zz), (0.03, 2.9, 0.22), X, Y, Z)
        zz += 0.3
    ub.obox(p, 'Dark#vp', (xw + 0.11, 2.55, Z0 + 1.12), (0.02, 1.0, 2.2), X, Y, Z)
    lamp_head(p, (xw + 0.3, -0.2, Z0 + 3.7), (1.0, 0.0, -1.2), w=0.6, h=0.26)
    for a in (72.0, 90.0, 108.0):
        r = HB_R - 0.09
        q = (HS + r * math.sin(math.radians(a)))
        ub.obox(p, 'Light#vp', ((XB0 + XB1) / 2 - 0.4, r * math.cos(math.radians(a)), q), (XB1 - XB0 - 2.4, 0.16, 0.03),
                (1, 0, 0), (0, math.sin(math.radians(a)), -math.cos(math.radians(a))), (0, math.cos(math.radians(a)), math.sin(math.radians(a))))
    # a estrela branca do Exército no disco escuro com o aro branco, deitada na casca (o flanco virado pro pátio cercado)
    disc = [(1.32 * math.cos(2 * math.pi * k / 48), 1.32 * math.sin(2 * math.pi * k / 48)) for k in range(48)]
    barrel_poly(p, 'Dark#vp', disc, STAR_X, STAR_A, SH_T + 0.004, SH_T + 0.05, nr=3)
    barrel_ring(p, 'Paint#vp', STAR_X, STAR_A, 1.12, 1.24, SH_T + 0.05, SH_T + 0.065)
    barrel_poly(p, 'Paint#vp', star_pts(0.98, 0.39), STAR_X, STAR_A, SH_T + 0.05, SH_T + 0.065, nr=2)
    for k in range(12):
        a = 2 * math.pi * (k + 0.5) / 12
        u, w = 1.28 * math.cos(a), 1.28 * math.sin(a)
        aa = STAR_A - w / HB_R
        n = Vector((0.0, math.cos(aa), math.sin(aa)))
        c = Vector((STAR_X + u, (HB_R + SH_T + 0.05) * math.cos(aa), HS + (HB_R + SH_T + 0.05) * math.sin(aa)))
        vp.nut(p, 'Trim', c, n, r=0.035, h=0.02)


def hangar_details(p):
    """O portão: o trilho da porta de enrolar no arco do vão, a faixa de perigo nas pernas, a faixa azul no alto do pórtico, os
    holofotes, a baliza; os frades; a passarela da cumeeira com o guarda-corpo e a escada de trás."""
    x1 = HX1
    # o trilho da porta (o U escuro no arco do vão, recuado no pórtico)
    arch_strip(p, 'Trim#vp', x1 - 0.45, x1 - 0.3, HO_R - 0.1, HO_R + 0.01, 0.0, 180.0, n=40)
    for s in (-1, 1):
        bx(p, 'Trim#vp', x1 - 0.45, x1 - 0.3, s * (HO_R - 0.1), s * (HO_R + 0.01), Z0 + 0.95, HS)
        # os holofotes do pórtico virados pro pátio de saída
        lamp_head(p, (x1 + 0.22, s * 3.9, 4.2), (1.0, s * 0.35, -0.9), w=0.5, h=0.28)
        ub.obox(p, 'Trim#vp', (x1 + 0.07, s * 3.9, 4.2), (0.14, 0.22, 0.2), X, Y, Z)
        # os frades listrados nos 2 lados do portão
        vp.lathe(p, 'Hazard#vp', (x1 + 0.55, s * (HO_R + 0.35), Z0), 'z', [(0.0, 0.0), (0.17, 0.0), (0.17, 0.95), (0.13, 1.02), (0.0, 1.04)],
                 seg=14)
    # a faixa azul no alto do pórtico (a cor do jogador na saída) e a baliza da cumeeira
    arch_strip(p, 'Player#vp', x1 - 0.002, x1 + 0.03, 3.72, 4.08, 58.0, 122.0, n=16)
    ub.beacon(p, (x1 - HF_D / 2, 0.0, HS + HF_R + 0.0), r=0.09, h=0.2)
    ub.beacon(p, (HX0 + HF_D / 2, 0.0, HS + HF_R + 0.0), r=0.09, h=0.2, cage=False)
    # a passarela da cumeeira: os pés na casca e nos pórticos, o piso de grade, o guarda-corpo dos 2 lados
    zd = HS + HF_R + 0.1
    for x in (HX0 + 0.35, RIBS[0], RIBS[1], -0.9, HX1 - 0.35):
        if x < XB0 or x > XB1:
            rt = HF_R                                   # no pórtico
        elif x in RIBS:
            rt = HB_R + SH_T + 0.11                     # na nervura
        else:
            rt = HB_R + SH_T                            # na chapa
        zb = HS + math.sqrt(rt * rt - 0.32 * 0.32) - 0.03
        for s in (-1, 1):
            vp.tube(p, 'Trim', [(x, s * 0.32, zb), (x, s * 0.32, zd)], 0.035, seg=6)
            vp.lathe(p, 'Trim#vp', (x, s * 0.32, zb), 'z', [(0.0, 0.0), (0.08, 0.0), (0.08, 0.04), (0.0, 0.05)], seg=8)
        bx(p, 'Trim#vp', x - 0.05, x + 0.05, -0.45, 0.45, zd - 0.1, zd)
    bx(p, 'Trim#vp', HX0 + 0.15, HX1 - 0.15, -0.45, 0.45, zd, zd + 0.06)
    x = HX0 + 0.35
    while x < HX1 - 0.3:
        bx(p, 'Dark#vp', x, x + 0.06, -0.38, 0.38, zd + 0.055, zd + 0.066)
        x += 0.3
    for s in (-1, 1):
        rail(p, 'Trim', [(HX0 + 0.2, s * 0.42), (HX1 - 0.2, s * 0.42)], zd + 0.06, h=1.0, step=2.0)
    # a escada de trás (no pórtico de trás, do chão à passarela)
    ub.ladder(p, 'Trim', HX0, -0.9, Z0, zd + 0.06, (-1, 0, 0), w=0.5, cage=True, wall=0.2, top_ext=1.0)
    for z in (Z0 + 0.4, 2.6):                    # embaixo a parede de trás fica recuada no pórtico: as mãos-francesas até ela
        for s in (-1, 1):
            vp.tube(p, 'Trim', [(HX0 - 0.2, -0.9 + s * 0.25, z), (HX0 + 0.25, -0.9 + s * 0.25, z)], 0.02, seg=6)
    # as luminárias do flanco (no suporte)
    for x in (HX0 + 2.0, -4.9):
        a = math.radians(160.0)
        n = Vector((0.0, math.cos(a), math.sin(a)))
        q = Vector((x, (HB_R + SH_T) * math.cos(a), HS + (HB_R + SH_T) * math.sin(a)))
        ub.obox(p, 'Trim#vp', q + n * 0.08, (0.16, 0.2, 0.18), (1, 0, 0), (0, -n.z, n.y), n)
        lamp_head(p, q + n * 0.28, (0.0, -1.0, -1.0), w=0.5, h=0.24)


# ---------------------------------------------------------------------------------- a torre de treliça
def bar(p, m, a, b, w, t=None):
    """A barra de seção retangular de a até b (w de largura, t de espessura; a face larga de pé quando dá)."""
    A, B = Vector(a), Vector(b)
    d = B - A
    L = d.length
    d.normalize()
    ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    e1 = ref.cross(d).normalized()
    e2 = d.cross(e1).normalized()
    ub.obox(p, m, (A + B) / 2, (L, w, t or w), d, e1, e2)


def tower(p):
    """A torre de treliça: o pedestal de concreto em tronco de pirâmide com a faixa escura e as placas de base, as 4 pernas (o perfil
    quadrado) em tronco de pirâmide, os 5 lances com as travessas e o X em cada face, a plataforma de grade com o rodapé e o
    guarda-corpo, a casinha de equipamento (a veneziana, a porta, a baliza), o mancal do prato, a escada com gaiola por dentro."""
    cx, cy = TC
    # o pedestal
    sq = ((1, 1), (-1, 1), (-1, -1), (1, -1))
    p.loft('Concrete#pnl', [[(cx + a * PL0, cy + b * PL0, -0.1) for a, b in sq], [(cx + a * PL0, cy + b * PL0, 0.35) for a, b in sq],
                            [(cx + a * PL1, cy + b * PL1, TZ0) for a, b in sq]], bevel=0.04)
    hw_at = lambda z: PL0 - (PL0 - PL1) * (z - 0.35) / (TZ0 - 0.35)
    p.loft('Trim#sm', [[(cx + a * (hw_at(0.78) + 0.035), cy + b * (hw_at(0.78) + 0.035), 0.78) for a, b in sq],
                       [(cx + a * (hw_at(0.95) + 0.035), cy + b * (hw_at(0.95) + 0.035), 0.95) for a, b in sq]], bevel=0.012)

    def leg(z, sx, sy):
        t = (z - TZ0) / (TZ1 - TZ0)
        w = TW0 + (TW1 - TW0) * t
        return Vector((cx + sx * w, cy + sy * w, z))
    corners = ((1, 1), (-1, 1), (-1, -1), (1, -1))
    for (sx, sy) in corners:
        bar(p, 'Body#vp', leg(TZ0, sx, sy), leg(TZ1 + 0.02, sx, sy), 0.2)
        bx(p, 'Trim#vp', cx + sx * TW0 - 0.16, cx + sx * TW0 + 0.16, cy + sy * TW0 - 0.16, cy + sy * TW0 + 0.16, TZ0 - 0.01, TZ0 + 0.06)
        for k in range(4):
            a = math.pi / 2 * k + math.pi / 4
            vp.nut(p, 'Dark', (cx + sx * TW0 + 0.15 * math.cos(a), cy + sy * TW0 + 0.15 * math.sin(a), TZ0 + 0.06), (0, 0, 1), r=0.03, h=0.03)
    nb = 5
    zs = [TZ0 + 0.35 + (TZ1 - TZ0 - 0.35) * k / nb for k in range(nb + 1)]
    for k in range(nb + 1):
        for i in range(4):
            a, b = corners[i], corners[(i + 1) % 4]
            bar(p, 'Body#vp', leg(zs[k], *a), leg(zs[k], *b), 0.13)
    for k in range(nb):
        for i in range(4):
            a, b = corners[i], corners[(i + 1) % 4]
            pa0, pb0, pa1, pb1 = leg(zs[k] + 0.06, *a), leg(zs[k] + 0.06, *b), leg(zs[k + 1] - 0.06, *a), leg(zs[k + 1] - 0.06, *b)
            n = (pa0 + pb0) / 2 - Vector((cx, cy, (pa0.z)))
            n.z = 0
            n.normalize()
            off = n * 0.02
            bar(p, 'Body#vp', pa0 + off, pb1 + off, 0.06, 0.11)        # (a face larga no plano da face da torre)
            bar(p, 'Body#vp', pb0 - off, pa1 - off, 0.06, 0.11)
            # a chapa de ligação no cruzamento
            c = (pa0 + pb1) / 2
            ub.obox(p, 'Trim#vp', c + n * 0.06, (0.22, 0.22, 0.02), Vector((-n.y, n.x, 0.0)), Vector((0, 0, 1)), n)
    # a plataforma: o piso de grade, as vigas, o rodapé, o guarda-corpo
    hw = 1.1
    bx(p, 'Trim#vp', cx - hw, cx + hw, cy - hw, cy + hw, TZ1 - 0.08, TZ1)
    bx(p, 'Body#vp', cx - hw - 0.04, cx + hw + 0.04, cy - hw - 0.04, cy + hw + 0.04, TZ1 - 0.28, TZ1 - 0.08)
    x = cx - hw + 0.12
    while x < cx + hw - 0.1:
        bx(p, 'Dark#vp', x, x + 0.05, cy - hw + 0.08, cy + hw - 0.08, TZ1 - 0.002, TZ1 + 0.008)
        x += 0.24
    for (sx, sy) in corners:
        bar(p, 'Trim#vp', leg(TZ1 - 1.0, sx, sy), Vector((cx + sx * (hw - 0.05), cy + sy * (hw - 0.05), TZ1 - 0.28)), 0.08)
    rp = [(cx + hw - 0.06, cy + hw - 0.06), (cx - hw + 0.06, cy + hw - 0.06), (cx - hw + 0.06, cy - hw + 0.06), (cx + hw - 0.06, cy - hw + 0.06)]
    rail(p, 'Trim', rp + [rp[0]], TZ1, h=1.0, step=1.2)
    # a casinha de equipamento, a veneziana, a porta, a baliza; o mancal fixo do prato
    hb = 0.55
    bx(p, 'Body#pnl', cx - hb, cx + hb, cy - hb, cy + hb, TZ1, TZ1 + 1.3)
    bx(p, 'Trim#sm', cx - hb - 0.06, cx + hb + 0.06, cy - hb - 0.06, cy + hb + 0.06, TZ1 + 1.3, TZ1 + 1.36)
    louvers(p, (cx + hb + 0.005, cy, TZ1 + 0.75), (1, 0, 0), (0, 1, 0), 0.7, 0.6, 6)
    ub.obox(p, 'Dark#vp', (cx, cy - hb - 0.01, TZ1 + 0.6), (0.6, 0.03, 1.05), X, Y, Z)
    ub.obox(p, 'Body#sm', (cx, cy - hb - 0.03, TZ1 + 0.59), (0.52, 0.03, 0.97), X, Y, Z)
    ub.obox(p, 'Player#sm', (cx - hb - 0.01, cy, TZ1 + 1.1), (0.03, 0.8, 0.18), X, Y, Z)
    ub.beacon(p, (cx + hb - 0.12, cy + hb - 0.12, TZ1 + 1.36), r=0.08, h=0.17)
    vp.lathe(p, 'Trim#vp', (cx, cy, TZ1 + 1.36), 'z', [(0.0, 0.0), (0.5, 0.0), (0.5, 0.06), (0.42, 0.12), (0.42, RADAR_AT[2] - TZ1 - 1.36),
                                                      (0.0, RADAR_AT[2] - TZ1 - 1.36)], seg=28)
    vp.bolt_ring(p, 'Dark', (cx, cy, TZ1 + 1.42), 'z', 0.46, 12, r=0.025, h=0.02)
    # a escada na face de trás (−X), inclinada com a face, presa nas travessas; o alçapão na plataforma; o cabo por dentro
    wz = lambda z: TW0 + (TW1 - TW0) * (z - TZ0) / (TZ1 - TZ0)
    za, zb = 0.8, TZ1                          # (o pé assenta na rampa do pedestal)
    lx = lambda z: cx - wz(z) - 0.28
    for s in (-1, 1):
        vp.tube(p, 'Trim', [(lx(za), cy + s * 0.23, za), (lx(zb), cy + s * 0.23, zb), (lx(zb), cy + s * 0.23, zb + 1.0)], 0.025, seg=8)
        for z in zs:
            vp.tube(p, 'Trim', [(lx(z), cy + s * 0.23, z), (cx - wz(z) + 0.02, cy + s * 0.23, z)], 0.02, seg=6)
    z = za + 0.3
    while z < zb:
        vp.tube(p, 'Trim', [(lx(z), cy - 0.23, z), (lx(z), cy + 0.23, z)], 0.017, seg=6)
        z += 0.3
    bx(p, 'Dark#vp', lx(zb) - 0.18, lx(zb) + 0.18, cy - 0.33, cy + 0.33, TZ1 - 0.002, TZ1 + 0.012)
    for s in (-1, 1):
        bx(p, 'Trim#vp', lx(zb) - 0.2, lx(zb) + 0.2, cy + s * 0.33 - 0.04, cy + s * 0.33 + 0.04, TZ1, TZ1 + 0.04)
    pipe(p, 'Dark', [(cx + 0.5, cy - 0.45, TZ0 + 0.05), (cx + 0.5, cy - 0.45, TZ1 - 0.3)], 0.05, step=1.5)


# ---------------------------------------------------------------------------------- a cerca, a bandeira, a placa, os postes
def fence(p, a, b, h=2.2, gate=None):
    """O alambrado de 'a' a 'b': os postes torneados com a tampa, os tubos de cima e de baixo, a trama em losango (as barras de 4,5 cm
    num vão de 36 cm: a regra do estudo, barra < 4 cm vira moiré) em 2 planos desencontrados; gate = (u0, u1) o portão (a moldura)."""
    A, B = Vector((a[0], a[1], 0.0)), Vector((b[0], b[1], 0.0))
    L = (B - A).length
    d = (B - A).normalized()
    n = Vector((-d.y, d.x, 0.0))
    zb, zt = Z0 + 0.12, Z0 + h
    m_ = max(1, int(round(L / 2.4)))
    posts = [L * k / m_ for k in range(m_ + 1)]
    if gate:
        posts = sorted(set([round(u, 3) for u in posts if not (gate[0] - 0.4 < u < gate[1] + 0.4)] + [gate[0], gate[1]]))
    for u in posts:
        q = A + d * u
        vp.lathe(p, 'Trim#vp', (q.x, q.y, Z0 - 0.02), 'z', [(0.0, 0.0), (0.09, 0.0), (0.09, 0.12), (0.05, 0.16), (0.05, h + 0.05), (0.065, h + 0.07),
                                                           (0.065, h + 0.12), (0.0, h + 0.16)], seg=10)
    # os painéis entre os postes (menos o do portão): os tubos de cima, do meio e de baixo e a trama em losango, cortada no painel
    pitch = 0.36
    H = zt - zb
    for (ua, ub2) in zip(posts[:-1], posts[1:]):
        if gate and abs(ua - gate[0]) < 1e-6 and abs(ub2 - gate[1]) < 1e-6:
            continue
        for z, r in ((zt, 0.035), (zb, 0.028), ((zb + zt) / 2, 0.022)):
            vp.tube(p, 'Trim', [A + d * (ua + 0.04) + Vector((0, 0, z)), A + d * (ub2 - 0.04) + Vector((0, 0, z))], r, seg=6)
        a0, a1 = ua + 0.05, ub2 - 0.05
        for sgn, off in ((1, 0.012), (-1, -0.012)):
            k0, k1 = int(math.floor((a0 - H) / pitch)) - 1, int(math.ceil((a1 + H) / pitch)) + 1
            for k in range(k0, k1 + 1):
                c = k * pitch                     # a reta u = c + sgn·v, v de 0 a H (a grade é a mesma em todos os painéis)
                if sgn > 0:
                    v0, v1 = max(0.0, a0 - c), min(H, a1 - c)
                else:
                    v0, v1 = max(0.0, c - a1), min(H, c - a0)
                if v1 - v0 < 0.06:
                    continue
                pa = A + d * (c + sgn * v0) + n * off + Vector((0, 0, zb + v0))
                pb = A + d * (c + sgn * v1) + n * off + Vector((0, 0, zb + v1))
                dd = (pb - pa).normalized()
                ub.obox(p, 'Trim#vp', (pa + pb) / 2, ((pb - pa).length, 0.045, 0.015), dd, n.cross(dd).normalized(), n)
    if gate:
        # o portão: a moldura de tubo das 2 folhas e a trava
        for (g0, g1) in ((gate[0] + 0.08, (gate[0] + gate[1]) / 2 - 0.04), ((gate[0] + gate[1]) / 2 + 0.04, gate[1] - 0.08)):
            qa, qb = A + d * g0, A + d * g1
            frame_pts = [qa + Vector((0, 0, zb)), qb + Vector((0, 0, zb)), qb + Vector((0, 0, zt - 0.06)), qa + Vector((0, 0, zt - 0.06)),
                         qa + Vector((0, 0, zb))]
            vp.tube(p, 'Trim', frame_pts, 0.03, seg=6)
            vp.tube(p, 'Trim', [qa + Vector((0, 0, zb)), qb + Vector((0, 0, zt - 0.06))], 0.022, seg=6)
        qm = A + d * ((gate[0] + gate[1]) / 2)
        ub.obox(p, 'Hazard#vp', qm + Vector((0, 0, Z0 + 1.1)) + n * 0.03, (0.12, 0.04, 0.2), d, n, Vector(Z))


def sign(p, c, yaw):
    """A placa inclinada com o emblema (a do original): o pé de concreto em cunha, a moldura escura, o campo azul (a cor do jogador),
    a estrela branca no aro, as 2 faixas amarelas embaixo."""
    d = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))       # pra onde a placa olha
    e = Vector((-d.y, d.x, 0.0))
    C = Vector((c[0], c[1], 0.0))
    W = 2.0
    prof = [(-0.65, -0.1), (0.55, -0.1), (0.55, 0.32), (-0.65, 1.55)]
    vp.prism(p, 'Concrete#vp', prof, C - e * (W / 2), d, Vector(Z), e, W)
    # o plano inclinado: de (0,55; 0,32) a (−0,65; 1,55)
    a0, a1 = Vector((0.55, 0.32)), Vector((-0.65, 1.55))
    t = (a1 - a0).normalized()
    ev = d * t.x + Vector(Z) * t.y             # sobe pela face (pra trás e pra cima)
    en = (d * t.y - Vector(Z) * t.x).normalized()   # a normal da face (pra frente e pra cima)
    mid = C + d * ((a0.x + a1.x) / 2) + Vector((0, 0, (a0.y + a1.y) / 2))
    Lp = (a1 - a0).length
    ub.obox(p, 'Trim#sm', mid + en * 0.03, (W - 0.12, Lp - 0.12, 0.06), e, ev, en)
    ub.obox(p, 'Player#vp', mid + en * 0.065 + ev * 0.12, (W - 0.36, Lp - 0.6, 0.02), e, ev, en)
    q = mid + en * 0.074 + ev * 0.2
    vp.prism(p, 'Paint#vp', [(0.45 * math.cos(2 * math.pi * k / 32), 0.45 * math.sin(2 * math.pi * k / 32)) for k in range(32)], q, e, ev, en, 0.016)
    vp.prism(p, 'Player#vp', [(0.39 * math.cos(2 * math.pi * k / 32), 0.39 * math.sin(2 * math.pi * k / 32)) for k in range(32)], q + en * 0.016, e, ev,
             en, 0.006)
    vp.prism(p, 'Paint#vp', star_pts(0.34, 0.135), q + en * 0.022, e, ev, en, 0.008)
    for k, off in enumerate((-0.31, -0.4)):
        ub.obox(p, 'Yellow#vp', mid + en * 0.08 + ev * off, (W - 0.7 - 0.25 * k, 0.045, 0.012), e, ev, en)


def flag_mast(p):
    """O mastro da bandeira no pé de concreto em tronco de pirâmide (o do original): o poste torneado, a bola, o pano (o jogo
    desenha no vento: flags.json)."""
    fx, fy = FLAG_AT
    p.loft('Concrete#pnl', [[(fx + a * 0.62, fy + b * 0.62, -0.1) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))],
                            [(fx + a * 0.36, fy + b * 0.36, 0.95) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))]], bevel=0.04)
    ub.obox(p, 'Trim#sm', (fx, fy, 0.98), (0.5, 0.5, 0.06), X, Y, Z)
    vp.lathe(p, 'Trim#vp', (fx, fy, 1.0), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.05), (0.09, 0.1), (0.075, 0.4), (0.05, 8.4), (0.0, 8.4)], seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, 9.42), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), 9.35, 2.4, 1.45, m='Player')


def yard(p):
    """O pátio cercado do lado do hangar: a cerca (a frente com o portão, o lado de fora, os fundos), a bandeira na frente, os postes
    de luz, os caixotes com a lona azul encostados."""
    fence(p, (FX1, -HF_R - 0.2), (FX1, FY), gate=(1.2, 4.0))
    fence(p, (FX1, FY), (FX0, FY))
    fence(p, (FX0, FY), (FX0, -HF_R - 0.2))
    flag_mast(p)
    # os postes de luz
    ub.lamp_post(p, 7.0, -7.4, h=6.2, aim=(3.0, -2.0))
    ub.lamp_post(p, 7.6, 4.7, h=6.2, aim=(3.0, 1.0))
    ub.lamp_post(p, -9.5, -9.1, h=6.2, aim=(-4.0, -6.5))
    ub.lamp_post(p, -9.6, 14.1, h=6.2, aim=(-6.0, 12.0))
    # a carga com a lona azul no canto do pátio cercado
    ub.obox(p, 'Crate#sm', (-6.6, -8.3, Z0 + 0.08), (2.4, 1.6, 0.16), X, Y, Z)
    ub.obox(p, 'Body#sm', (-6.6, -8.3, Z0 + 0.6), (2.1, 1.3, 0.9), X, Y, Z)
    ub.tarp_cover_at(p, (-6.6, -8.3), 2.4, 1.6, 0.0, Z0 + 0.2, 0.95, m='Player', seed=1.3, ties=3, box=14)
    for (x, y) in ((-3.4, -8.7), (-2.8, -8.9), (-3.1, -8.2)):
        vp.lathe(p, 'Trim#vp', (x, y, Z0), 'z', [(0.0, 0.0), (0.29, 0.0), (0.29, 0.86), (0.27, 0.88), (0.0, 0.88)], seg=16)
        for z in (0.3, 0.58):
            p.torus('Dark#vp', (x, y, Z0 + z), 'z', 0.295, 0.015, seg=16, rseg=4)


def base():
    p = CPart(PREFIX + '_Base')
    ub.keys(p)
    for k, bev in (('Body#frame', 0.04), ('Body#shell', 0.04), ('Body#rib', 0.012), ('Body#pil', 0.04), ('Trim#pil', 0.04),
                   ('Paint#vault', 0.04)):
        p.bm(k, bev)
    ground(p)
    hall(p)
    entrance(p)
    roofs(p)
    hall_details(p)
    hangar(p)
    hangar_details(p)
    tower(p)
    sign(p, SIGN_AT, -18.0)
    yard(p)
    vp.poke_ngons(p, 0.05)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o prato do radar (origem no eixo de giro)
DISH_EL = math.radians(30.0)     # a elevação do prato
DISH_R, DISH_DEPTH = 2.0, 0.625  # o raio da borda e a profundidade (f = R²/4p = 1,6 m)
AXLE = Vector((0.0, 0.0, 0.8))   # o eixo de elevação (local)
VERTEX = AXLE + Vector((math.cos(DISH_EL), 0.0, math.sin(DISH_EL))) * 0.4


def radar():
    """O prato grande que gira (x local = pra onde ele olha): a mesa giratória, o garfo com o eixo de elevação, o pescoço, o prato
    parabólico (a face clara, as costas com as 8 nervuras e o anel), a borda, o alimentador nas 4 hastes e a vareta comprida da
    ponta (a do original), o contrapeso atrás, a luz de aviso no alto da borda."""
    p = CPart(PREFIX + '_Radar', cull=False)
    ub.keys(p)
    D = Vector((math.cos(DISH_EL), 0.0, math.sin(DISH_EL)))
    E1 = Vector((-math.sin(DISH_EL), 0.0, math.cos(DISH_EL)))
    E2 = Vector((0.0, 1.0, 0.0))
    # a mesa e o garfo
    vp.lathe(p, 'Trim#vp', (0, 0, 0), 'z', [(0.0, 0.0), (0.62, 0.0), (0.62, 0.12), (0.55, 0.18), (0.0, 0.18)], seg=32)
    vp.bolt_ring(p, 'Dark', (0, 0, 0.18), 'z', 0.5, 16, r=0.025, h=0.018)
    vp.lathe(p, 'Body#vp', (0, 0, 0.18), 'z', [(0.0, 0.0), (0.38, 0.0), (0.38, 0.3), (0.3, 0.38), (0.0, 0.38)], seg=24)
    for s in (-1, 1):
        prof = [(-0.32, 0.0), (0.32, 0.0), (0.22, 0.72), (0.0, 0.84), (-0.22, 0.72)]
        vp.prism(p, 'Body#vp', prof, Vector((0.0, s * 0.42, 0.3)), Vector(X), Vector(Z), Vector((0, s, 0)), 0.1)
    vp.lathe(p, 'Trim#vp', AXLE - Vector((0, 0.58, 0)), 'y', [(0.0, 0.0), (0.13, 0.0), (0.13, 1.16), (0.0, 1.16)], seg=16)
    for s in (-1, 1):
        vp.lathe(p, 'Dark#vp', AXLE + Vector((0, s * 0.56, 0)), (0, s, 0), [(0.0, 0.0), (0.17, 0.0), (0.17, 0.06), (0.0, 0.08)], seg=16)
    # o pescoço até o cubo do prato
    hub = VERTEX - D * 0.16
    vp.tube(p, 'Body', [AXLE, hub], 0.2, seg=12)
    vp.lathe(p, 'Trim#vp', hub - D * 0.12, D, [(0.0, 0.0), (0.36, 0.0), (0.36, 0.16), (0.3, 0.2), (0.0, 0.2)], seg=20)
    # o prato: a face (Paint) e as costas (Body), a borda
    k = DISH_DEPTH / DISH_R ** 2
    rs = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.7, 1.87, DISH_R]
    front = [(r, k * r * r) for r in rs] + [(DISH_R + 0.06, DISH_DEPTH + 0.02), (DISH_R + 0.06, DISH_DEPTH - 0.05)]
    vp.lathe(p, 'Paint#vp', VERTEX, D, front, seg=48, hard=50.0)
    back = [(DISH_R + 0.06, DISH_DEPTH - 0.05)] + [(r, k * r * r - 0.08) for r in rs[::-1]]
    back[-1] = (0.0, -0.08)
    vp.lathe(p, 'Body#vp', VERTEX, D, back, seg=48, hard=50.0)
    # as 8 nervuras das costas e o anel
    for i in range(8):
        a = 2 * math.pi * (i + 0.5) / 8
        e = E1 * math.cos(a) + E2 * math.sin(a)
        pts = [VERTEX + e * r + D * (k * r * r - 0.12 - 0.22 * (1 - r / DISH_R)) for r in (0.32, 0.8, 1.3, 1.82)]
        vp.tube(p, 'Trim', [hub - D * 0.05 + e * 0.3] + pts, 0.045, seg=6)
    r = 1.25
    ring = [VERTEX + (E1 * math.cos(2 * math.pi * i / 32) + E2 * math.sin(2 * math.pi * i / 32)) * r + D * (k * r * r - 0.19) for i in range(33)]
    vp.tube(p, 'Trim', ring, 0.04, seg=6)
    # o alimentador nas 4 hastes e a vareta comprida
    F = VERTEX + D * (DISH_R ** 2 / (4 * DISH_DEPTH))
    for i in range(4):
        a = 2 * math.pi * i / 4 + math.pi / 4
        e = E1 * math.cos(a) + E2 * math.sin(a)
        rr = DISH_R - 0.12
        vp.tube(p, 'Trim', [VERTEX + e * rr + D * (k * rr * rr + 0.01), F - D * 0.12 + e * 0.07], 0.03, seg=6)
    vp.lathe(p, 'Trim#vp', F - D * 0.2, D, [(0.0, 0.0), (0.1, 0.0), (0.1, 0.18), (0.16, 0.3), (0.16, 0.36), (0.0, 0.36)], seg=14)
    vp.lathe(p, 'Dark#vp', F + D * 0.16, D, [(0.0, 0.0), (0.13, 0.0), (0.0, 0.02)], seg=14)
    vp.tube(p, 'Trim', [F + D * 0.16, F + D * 0.62], 0.03, seg=6)
    vp.lathe(p, 'Dark#vp', F + D * 0.62, D, [(0.0, 0.0), (0.05, 0.0), (0.05, 0.08), (0.0, 0.12)], seg=8)
    # o contrapeso atrás do eixo
    cw = AXLE - D * 0.95 + E1 * 0.25
    vp.tube(p, 'Trim', [AXLE, cw], 0.09, seg=8)
    vp.rbox(p, 'Dark', tuple(cw - D * 0.15), (0.5, 0.7, 0.42), yaw=0.0, pitch=math.degrees(DISH_EL), r=0.06, k=2)
    # a luz de aviso no alto da borda
    top = VERTEX + E1 * (DISH_R + 0.06) + D * (DISH_DEPTH + 0.02)
    ub.beacon(p, tuple(top), r=0.07, h=0.14, cage=False)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o andaime
def scaffold_runs(name, runs, post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14, out=0.85):
    """O andaime da obra em trechos [(a, b, h, (ox, oy))]: cada um com a altura e o lado de fora explícitos (o contorno do CC tem
    reentrâncias: o lado de fora pelo centro dava errado)."""
    p = Part(name)
    for (a, b, h, (ox, oy)) in runs:
        top = h + 1.0
        levels = max(2, int((top - z0) / lift))
        (ax, ay), (bx_, by) = a, b
        L = math.hypot(bx_ - ax, by - ay)
        dx, dy = (bx_ - ax) / L, (by - ay) / L
        yaw = math.degrees(math.atan2(dy, dx))
        m = max(1, int(L / bay))
        for k in range(m + 1):
            t = k / m
            px, py = ax + (bx_ - ax) * t, ay + (by - ay) * t
            for d in (0.0, out):
                p.cyl(post, (px + ox * d, py + oy * d, (z0 + top) / 2), 'z', 0.05, top - z0, seg=8, bevel=0.0)
        for lv in range(1, levels + 1):
            z = z0 + (top - z0) * lv / levels
            mx, my = (ax + bx_) / 2, (ay + by) / 2
            for d in (0.0, out):
                p.cyl(post, (mx + ox * d, my + oy * d, z), (dx, dy, 0.0), 0.045, L, seg=8, bevel=0.0)
            p.obox(plank, (mx + ox * out / 2, my + oy * out / 2, z - 0.03), (L, out - 0.1, 0.06), yaw=yaw, bevel=0.01)
            p.obox(stripe, (mx + ox * out, my + oy * out, z + 0.09), (L, 0.04, 0.18), yaw=yaw, bevel=0.0)
        for k in range(m):
            t0, t1 = k / m, (k + 1) / m
            pa = Vector((ax + (bx_ - ax) * t0 + ox * out, ay + (by - ay) * t0 + oy * out, z0 + (top - z0) * ((k % levels) / levels)))
            pb = Vector((ax + (bx_ - ax) * t1 + ox * out, ay + (by - ay) * t1 + oy * out, z0 + (top - z0) * ((k % levels + 1) / levels)))
            d = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (d.x, d.y, d.z), 0.035, d.length, seg=6, bevel=0.0)
    return p.finish()


def scaffold():
    """O andaime: o lado de fora do hangar (no pátio cercado), os fundos do hangar e do galpão, o lado de fora do galpão, a frente da
    entrada; o portão do hangar fica livre (a saída) e o recorte da torre também."""
    hh = HS + HF_R
    runs = [((HX0 + 0.2, -HF_R - 0.35), (HX1 - 0.1, -HF_R - 0.35), hh, (0, -1)),
            ((HX0 - 0.3, -HF_R + 0.1), (HX0 - 0.3, YH0 - 0.2), hh, (-1, 0)),
            ((XH0 - 0.4, YH0 - 0.1), (XH0 - 0.4, YH1 + 0.2), ZR, (-1, 0)),
            ((XH0 + 0.5, YH1 + 0.24), (XN - 0.5, YH1 + 0.24), ZR, (0, 1)),
            ((XH1 + 0.45, YH0 + 0.1), (XH1 + 0.45, YE1 - 0.1), EAVE + 0.4, (1, 0))]
    return scaffold_runs(PREFIX + '_Scaffold', runs)


def main():
    out = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_command_center_v4')
    os.makedirs(out, exist_ok=True)
    clear_scene()
    b = base()
    r = radar()
    sc = scaffold()
    check([b, r, sc], HX, HY, at={r.name: RADAR_AT})
    sweep = max(math.hypot(v.co.x, v.co.y) for v in r.data.vertices)
    low = [(math.hypot(v.co.x, v.co.y), v.co.z + RADAR_AT[2]) for v in r.data.vertices if v.co.z + RADAR_AT[2] < TZ1 + 1.1]
    near = min((q[0] for q in low), default=99.0)
    top = max(v.co.z for v in r.data.vertices) + RADAR_AT[2]
    print('MODEL prato: varre r = %.2f m; abaixo do guarda-corpo (z < %.2f) o mais perto do eixo a r = %.2f m (o guarda-corpo vai até r '
          '%.2f); o topo a z = %.2f; o círculo varrido vai de y %.2f a %.2f e de x %.2f a %.2f (o lote ±%.2f × ±%.2f)' %
          (sweep, TZ1 + 1.1, near, 1.1 * math.sqrt(2), top, TC[1] - sweep, TC[1] + sweep, TC[0] - sweep, TC[0] + sweep, HX, HY))
    print('MODEL tris por peca (antes da grade)', {o.name: tris(o) for o in (b, r, sc)})
    r = bake_in_place(r, RADAR_AT)
    total = export(b, out)
    total += export(r, out)
    total += export(sc, out)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL spin at_m', [round(v, 3) for v in RADAR_AT], 'rpm 4')
    print('MODEL fire_points_m', [list(q) for q in FIRE_POINTS])
    print('MODEL saida: o portão em x = %.2f (face do pórtico), y = 0; o vão %.2f m no chão (%.2f m entre os trilhos), livre %.2f m a '
          'y ±1,75 e %.2f m no meio; o pátio de saída até x = 6,4 (depois o chão, livre até a borda do lote)' %
          (HX1, 2 * HO_R, 2 * (HO_R - 0.1), HS + math.sqrt((HO_R - 0.1) ** 2 - 1.75 ** 2), HS + HO_R - 0.1))
    preview_spec(out, [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Radar', [RADAR_AT[0], -RADAR_AT[1], RADAR_AT[2]]]],   # (posição do Blender)
                 [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1500]],
                  ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1700, 950]],
                  ['side', 'ortho', [0, -1, 0], [0, 0, 1], [1700, 900]],
                  ['three_quarter', 'persp', 40.0, 221.0, [1600, 1200], 38.0, 0.9],
                  ['back_three_quarter', 'persp', 32.0, 40.0, [1600, 1000], 30.0, 0.95]],
                 (7.0, -0.2, 0), [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
                 scaffold=PREFIX + '_Scaffold')


if __name__ == '__main__':
    main()
