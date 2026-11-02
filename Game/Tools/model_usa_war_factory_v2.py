"""A War Factory da USA (usa_war_factory), VERSÃO B no detalhe AAA (01/11/2026): o pedido do Felipe ("refazer as construções iniciais
da USA, ficaram todas quadradas, sem vida, sem qualidade [...] as novas que você fez todas ficaram muito boas"). A versão A é a caixa
do script antigo (Game/Tools/blender_provisional.py, war_factory()); esta é a ficha aprovada refeita peça por peça na linguagem dos
prédios da USA (a do Strategy Center B: Game/Tools/model_strategy_center_v2.py; docs/ESTUDO_DETALHE_AAA.md).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_war_factory_v2.py C:/Users/luisf/explosion/captures/game/models/usa_war_factory_v2

A PALAVRA (docs/elenco/usa/usa_war_factory.md §2): O PÓRTICO AMARELO sobre o portão, com os DOIS VENTILADORES girando no teto. A ficha
aprovada (docs/concept/buildings/usa_war_factory.webp e usa_war_factory_sheet.webp), fiel nas vistas:
  - o galpão: o bloco alto da frente (o dos ventiladores) e o bloco baixo de trás com as claraboias; as paredes de PLACAS com a junta
    funda e a FAIXA AZUL feita de placas logo abaixo da linha do teto baixo (a da ficha), os pilares nos cantos, o parapeito com a
    capa, as portas de enrolar e de gente com a marquise e a luminária;
  - o PORTÃO: o vão fundo de 10 m (o tanque de 3,7 m passa com folga) com o interior escuro e aceso (as paredes de placas escuras, as
    colunas, as treliças e as calhas de luz do teto, o portão de enrolar do fundo), a caixa da porta de enrolar recolhida em cima, a
    soleira de perigo, as faixas amarelas e as grelhas da pista até a borda do lote;
  - o PÓRTICO: os dois pilares de placas na frente com o pé de perigo e as luzes viradas pra pista, as vigas de rolamento do galpão
    até eles, a PONTE AMARELA atravessada (a viga-caixão com as nervuras, os carros de ponta com as rodas no trilho, o trole com o
    tambor e o motor, os cabos e o moitão com o gancho, a passarela com o corrimão), os frades de perigo;
  - o teto: a CAIXA DOS 2 VENTILADORES (os poços com o fundo escuro, o motor e as travessas, a grade de proteção; o rotor é a malha
    à parte que GIRA), as venezianas, os condensadores, os corrimãos, a escada de marinheiro, as balizas;
  - o canto de trás (+Y): o DOMO DE RADAR na torre treliçada (as 4 pernas com o travamento em X, a plataforma com o guarda-corpo, a
    esfera com as costuras), as DUAS CHAMINÉS (o vapor é do jogo) com os anéis, a escada com gaiola, o quadro de amarração e a casa
    de máquinas, a linha de canos até o galpão;
  - o PÁTIO DO BRAÇO ROBÓTICO (+Y, frente): a moldura de perigo pintada, o braço de 6 eixos com as tampas azuis das juntas e as
    mangueiras, a bancada com a peça, as caixas;
  - o ANEXO (−Y, frente): as placas com a faixa azul no meio da parede (a da ficha), as janelas, a porta, a carga com a LONA AZUL
    amarrada no teto e o MASTRO DA BANDEIRA (o pano no vento é do jogo: flags.json);
  - o canto de trás (−Y): o tanque de combustível deitado na bacia e a carga com a lona; os postes de luz com o pé de perigo.
A USA limpa (13/10): sem desgaste pintado.

As medidas do jogo (Units.json usa_war_factory): o lote 22,32 × 25,28 m (X × Y); o portão em +X, centrado em Y = 0 (a faixa de saída
de 6,6 m do SimCore sai do centro do lote). Peças: SM_USA_WarFactory_Base, SM_USA_WarFactory_Fan (UM rotor, origem no eixo: o jogo
põe uma cópia em cada FAN_AT e gira em Z), SM_USA_WarFactory_Scaffold. Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass, Light,
Beacon, Yellow, Paint, Crate (o andaime).
"""
import math
import os
import sys

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, Part, V, clear_scene, export, flag  # noqa: E402
import model_vehicle_parts as vp  # noqa: E402
import model_usa_building_parts as ub  # noqa: E402
from model_building_parts import bake_in_place  # noqa: E402
from model_china_vehicle_kit import tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_war_factory_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_WarFactory'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62)})

# ---------------------------------------------------------------------------------- as medidas
HX, HY = 11.16, 12.64            # o lote 22,32 × 25,28 (Units.json size_m)
Z0 = 0.14                        # o topo da laje
TX0, TX1, TY, TZ = -1.4, 5.0, 7.0, 7.0      # o bloco alto (o dos ventiladores): x, meia-largura, o piso do teto
RX0, RY0, RY1, RZ = -8.4, -7.0, 3.0, 4.6    # o bloco baixo de trás: x RX0..TX0, y RY0..RY1, o piso do teto
BY, BZ, BX0 = 5.0, 5.4, -1.0                # o vão do portão: ±BY (10 m), o teto (5,26 m livres), o fundo
PX0, PX1, PY0, PY1 = 7.2, 9.2, 5.2, 7.2     # os pilares do pórtico (x, |y|): 2 × 2 m (os pilares maciços do alvo r1)
BEAM = (5.6, 6.9)                           # a viga de rolamento (z)
GX0, GX1, GZ0, GZ1 = 7.8, 8.6, 5.45, 6.35   # a ponte amarela (x, z) na linha dos pilares: 2 vigas-caixão, a fresta dos cabos
FAN_X, FAN_Y, FAN_R = 1.5, 1.8, 1.5         # os ventiladores: o centro (x, ±y) e o raio do poço
FH_X0, FH_X1, FH_Y, FH_TOP = -0.2, 3.2, 3.6, 7.95   # a caixa dos ventiladores
FAN_Z = FH_TOP - 0.34                       # o plano do rotor (o at_m)
FAN_AT = [(FAN_X, FAN_Y, FAN_Z), (FAN_X, -FAN_Y, FAN_Z)]
AX0, AX1, AY0, AY1, AZ = 2.2, 8.6, -11.8, -7.6, 3.6  # o anexo (−Y, frente): 6,4 m
PAD = (1.4, 8.4, 7.6, 12.2)                 # o pátio do braço robótico (x0, x1, y0, y1)
RC, R_SPH, RZC = (-6.4, 5.2), 1.75, 7.2      # o domo: o centro (x, y), o raio, a altura do centro (topo 8,95 m, a ficha ~9)
UBK = (-8.9, -6.5, 7.5, 12.1, 3.3)          # a casa de máquinas das chaminés (x0, x1, y0, y1, altura)
CHIM = [(-10.05, 8.6), (-10.05, 10.9)]       # as chaminés
CH_R, CH_TOP = 0.75, 8.0
ZROWS = [Z0 + r for r in (1.6, 3.2, 3.75, 5.3)]
J = 0.035                                               # a meia junta entre 2 paredes no mesmo plano (não encostam: soldavam)   # as juntas das placas (absolutas)
BAND = (Z0 + 3.2, Z0 + 3.75)                            # a fileira azul (abaixo da linha do teto baixo, como a ficha)

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
NONMANIFOLD = []


class UPart(Part):
    """O Part com o aviso do model_russia_parts.RuPart: antes do chanfro, a malha chanfrada com aresta de 3+ faces depois da solda
    (peça encostando em peça do mesmo balde) avisa NONMANIFOLD; o chanfro do Blender joga vértices pro infinito nela."""

    def finish(self):
        for m, b in self.bms.items():
            if self.bevel[m] <= 0:
                continue
            c = b.copy()
            bmesh.ops.remove_doubles(c, verts=c.verts, dist=1e-5)
            bad = [e for e in c.edges if len(e.link_faces) > 2]
            if bad:
                q = bad[0].verts[0].co
                print('NONMANIFOLD', self.name, m, len(bad), 'arestas; a 1a em (%.2f, %.2f, %.2f)' % (q.x, -q.y, q.z))
                NONMANIFOLD.append((self.name, m, len(bad)))
                spots = sorted({(round(e.verts[0].co.x, 1), round(-e.verts[0].co.y, 1), round(e.verts[0].co.z, 1)) for e in bad})
                print('NONMANIFOLD onde', spots[:40])
            c.free()
        return super().finish()


# ---------------------------------------------------------------------------------- ajudantes
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


def clad(p, axis, at, s, a0, a1, z0, z1, holes=(), pw=3.2, zrows=ZROWS, band=BAND, m='Body#pnl', mats=None, gap=0.07):
    """As placas de uma parede (ub.panel_wall) com as juntas nas alturas absolutas zrows e a fileira 'band' na cor do jogador;
    holes em absoluto [(u0, u1, z0, z1)]; mats = {z de baixo da fileira: material} (o pé de perigo dos pilares)."""
    eu, en = axes(axis, s)
    rows = [zr - z0 for zr in zrows if z0 + 0.05 < zr < z1 - 0.05]
    vs = [0.0] + rows + [z1 - z0]
    rm = {}
    for j in range(len(vs) - 1):
        if band and abs(vs[j] - (band[0] - z0)) < 1e-6:
            rm[j] = 'Player#pnl'
        for zb, mm in (mats or {}).items():
            if abs(vs[j] - (zb - z0)) < 1e-6:
                rm[j] = mm
    hs = [(u0 - a0, u1 - a0, zb - z0, zt - z0) for (u0, u1, zb, zt) in holes]
    panel_wall(p, wpt(axis, at, s, a0, z0), eu, Z, en, a1 - a0, z1 - z0, pw=pw, m=m, gap=gap, proud=0.08, embed=0.04, holes=hs,
               rows=rows, row_mats=rm, min_piece=0.28)


def panel_wall(p, origin, eu, ev, en, w, h, pw=2.0, ph=None, m='Body#pnl', gap=0.06, proud=0.07, embed=0.05, holes=(),
               min_piece=0.3, rows=None, row_mats=None):
    """O ub.panel_wall com uma diferença: as placas cortadas por um vão (a porta, a janela) ganham a junta também entre os pedaços
    (no kit os pedaços encostam com vértice em comum e o remove_doubles do Part solda: a aresta de 3 faces que estraga o chanfro)."""
    O, eu, ev, en = Vector(origin), Vector(eu), Vector(ev), Vector(en)
    nu = max(1, int(math.ceil(w / pw - 1e-6)))
    us = [w * i / nu for i in range(nu + 1)]
    if rows is None:
        ph = ph or h
        nv = max(1, int(round(h / ph)))
        vs = [h * j / nv for j in range(nv + 1)]
    else:
        vs = [0.0] + [r for r in rows if 0 < r < h] + [h]
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


def plate(p, m, c, size, eu, ev, en):
    """A placa da parede: a caixa SEM a face de trás (a que fica enterrada no miolo). O chanfro pega a borda da frente e as quinas de
    pé; a borda aberta de trás fica sem chanfro (ninguém vê)."""
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


def louvers(p, c, n_face, along, L, H, n, frame='Trim', slat='Trim', back='Dark', tilt=35.0):
    """O ub.louvers com os montantes da moldura entrando na travessa (no kit as 4 peças tinham quina em comum: soldavam)."""
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


def fan_box(p, c, size, fans=1, m='Body', z0=0.0, R=None):
    """O ub.fan_box (o condensador do teto) com as tampas de 2 ventiladores separadas por uma fresta coberta (no kit elas
    encostavam) e as venezianas daqui."""
    x, y = c
    sx, sy, sz = size
    t = 0.46
    ub.obox(p, m + '#pnl', (x, y, z0 + 0.08 + (sz - t + 0.02 - 0.08) / 2), (sx - 0.08, sy - 0.08, sz - t + 0.02 - 0.08), X, Y, Z)
    for sxx in (-1, 1):
        for syy in (-1, 1):
            ub.obox(p, 'Trim#vp', (x + sxx * (sx / 2 - 0.2), y + syy * (sy / 2 - 0.2), z0 + 0.05), (0.3, 0.3, 0.1), X, Y, Z)
    R = R or min(sy * 0.38, sx / (2 * fans) * 0.78)
    cs = [(x + (k - (fans - 1) / 2) * sx / fans, y) for k in range(fans)]
    for (cx, cy) in cs:
        ub.square_annulus(p, m + '#vp', (cx, cy), sx / fans / 2 - (0.02 if fans > 1 else 0.0), sy / 2, R, z0 + sz, t)
        small_fan(p, (cx, cy), R, z0 + sz, depth=t - 0.04)
    if fans > 1:
        for k in range(1, fans):
            xx = x - sx / 2 + k * sx / fans
            ub.obox(p, 'Trim#vp', (xx, y, z0 + sz + 0.015), (0.1, sy - 0.02, 0.05), X, Y, Z)
    for sg in (-1, 1):
        hb = sz - t - 0.08
        louvers(p, (x, y + sg * (sy / 2 - 0.04), z0 + 0.08 + hb / 2), (0, sg, 0), (1, 0, 0), sx * 0.7, hb * 0.6, max(3, int(hb * 0.6 / 0.09)))


def small_fan(p, c, R, z_top, depth=0.42, blades=7):
    """O ub.fan dos condensadores mais leve: o poço escuro, a moldura, o cubo, as pás (sem o chanfro: 2,5 cm não leva 3 segmentos) e
    a grade (o anel e 2 barras)."""
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


def rail(p, m, pts, z, h=1.05, r=0.025, step=2.0, mid=True):
    """O ub.rail mais leve: os tubos de 6 lados, o pé torneado de 8, os montantes a cada 2 m."""
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
    """O holofote (o ub.lamp_head sem o chanfro das caixas): o corpo de quinas redondas virado pra 'aim', a lente acesa e a aba."""
    d = Vector(aim).normalized()
    ey = Vector((0, 0, 1)).cross(d).normalized()
    ez = d.cross(ey).normalized()
    C = Vector(c)
    vp.prism(p, body + '#vp', vp.rounded_rect(w, h, 0.04, 2), C - d * 0.11, ey, ez, d, 0.22)
    ub.obox(p, 'Light#vp', C + d * 0.115, (0.012, w - 0.08, h - 0.08), d, ey, ez)
    ub.obox(p, body + '#vp', C + d * 0.12 + ez * (h / 2 + 0.01), (0.16, w + 0.02, 0.02), d, ey, ez)


def pipe(p, m, pts, r, step=2.4):
    """O ub.pipe com 10 lados e as braçadeiras a cada 2,4 m (o de 14 lados a cada 1,6 m custava caro na grade)."""
    return ub.pipe(p, m, pts, r, step=step, seg=10)


def solid(p, x0, x1, y0, y1, z0, z1, m='Body#vp', skip=()):
    """O miolo (escondido atrás das placas): a caixa sem o fundo e sem os lados de 'skip' ('+x', '-x', '+y', '-y': os que ficam
    dentro de outro bloco)."""
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


def pilaster(p, x, y, z1, w=0.5, m='Body#pnl', z0=Z0 - 0.02):
    bx(p, m, x - w / 2, x + w / 2, y - w / 2, y + w / 2, z0, z1)


def parapet(p, x0, x1, y0, y1, z0, z1, t=0.32, skip=()):
    """O parapeito retangular (os 4 lados sem vértice em comum: os de y encaixam 2 cm nos de x) com a capa escura em cima."""
    o = 0.1
    xa = x0 if '-x' in skip else x0 + t - 0.02
    xb = x1 if '+x' in skip else x1 - t + 0.02
    sides = {'+x': (x1 - t, x1 + o, y0 - o, y1 + o), '-x': (x0 - o, x0 + t, y0 - o, y1 + o),
             '+y': (xa, xb, y1 - t, y1 + o), '-y': (xa, xb, y0 - o, y0 + t)}
    for k, (a, b, c, d) in sides.items():
        if k in skip:
            continue
        bx(p, 'Body#vp', a, b, c, d, z0, z1)
        if k[1] == 'x':
            bx(p, 'Trim#vp', a - 0.04, b + 0.04, c - 0.04, d + 0.04, z1, z1 + 0.08)
        else:
            bx(p, 'Trim#vp', a, b, c - 0.04, d + 0.04, z1, z1 + 0.08)


def roof_deck(p, x0, x1, y0, y1, z, nx, ny, m='Dark#vp'):
    """O piso do teto em lajes: o topo do miolo é o piso; as juntas são os frisos escuros por cima (o piso liso lia como uma placa
    só, clara; as lajes soltas dobravam os triângulos depois da grade)."""
    dx, dy = (x1 - x0) / nx, (y1 - y0) / ny
    for i in range(1, nx):
        bx(p, m, x0 + i * dx - 0.035, x0 + i * dx + 0.035, y0, y1, z - 0.01, z + 0.006)
    for j in range(1, ny):
        bx(p, m, x0, x1, y0 + j * dy - 0.035, y0 + j * dy + 0.035, z - 0.01, z + 0.006)


def door(p, axis, at, s, uc, w=1.1, h=2.25, canopy=True, lamp=True, zb=Z0):
    """A porta de gente: o vão escuro, o batente, a folha com a alça, a marquise e a luminária (zb = o piso)."""
    Z0 = zb
    eu, en = axes(axis, s)
    ub.obox(p, 'Dark#vp', wpt(axis, at, s, uc, Z0 + h / 2, 0.0), (w, h, 0.06), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.05), Z0 + h / 2, 0.08), (0.1, h + 0.1, 0.1), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0 + h + 0.05, 0.08), (w + 0.2, 0.1, 0.1), eu, Z, en)
    ub.obox(p, 'Body#sm', wpt(axis, at, s, uc, Z0 + h / 2 - 0.02, 0.05), (w - 0.1, h - 0.08, 0.05), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0 + h * 0.72, 0.08), (w - 0.3, 0.3, 0.02), eu, Z, en)       # o visor
    a = wpt(axis, at, s, uc + (w / 2 - 0.18), Z0 + 0.95, 0.08)
    vp.grab_handle(p, 'Trim', a, a + Vector((0, 0, 0.28)), en, height=0.05, r=0.014)
    if canopy:
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0 + h + 0.32, 0.4), (w + 0.7, 0.8, 0.08), eu, en, Z)
        for su in (-1, 1):
            q0 = wpt(axis, at, s, uc + su * (w / 2 + 0.25), Z0 + h + 0.0, 0.05)
            vp.tube(p, 'Trim', [q0, wpt(axis, at, s, uc + su * (w / 2 + 0.25), Z0 + h + 0.3, 0.72)], 0.02, seg=6)
    if lamp:
        lamp_head(p, wpt(axis, at, s, uc, Z0 + h + (0.16 if canopy else 0.3), 0.2 if canopy else 0.14), en - Vector((0, 0, 1.2)),
                     w=0.36, h=0.18)


def rollup(p, axis, at, s, uc, w, h, box=True):
    """A porta de enrolar: o vão escuro, a cortina de lâminas (escura: a porta clara não lia como porta), os trilhos, a caixa do rolo,
    os frades de perigo."""
    eu, en = axes(axis, s)
    ub.obox(p, 'Dark#vp', wpt(axis, at, s, uc, Z0 + h / 2, 0.0), (w, h, 0.06), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0 + h / 2 + 0.02, 0.05), (w - 0.06, h - 0.04, 0.05), eu, Z, en)
    k = 0
    zz = Z0 + 0.3
    while zz < Z0 + h - 0.15:
        ub.obox(p, 'Dark#vp', wpt(axis, at, s, uc, zz, 0.085), (w - 0.16, 0.045, 0.025), eu, Z, en)
        zz += 0.32
        k += 1
    ub.obox(p, 'Hazard#vp', wpt(axis, at, s, uc, Z0 + 0.09, 0.09), (w - 0.1, 0.14, 0.04), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.07), Z0 + h / 2, 0.08), (0.14, h + 0.05, 0.16), eu, Z, en)
        vp.lathe(p, 'Hazard#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.45), Z0, 0.35), 'z',
                 [(0.0, 0.0), (0.13, 0.0), (0.13, 0.8), (0.1, 0.86), (0.0, 0.88)], seg=12)
    if box:
        ub.obox(p, 'Body#pnl', wpt(axis, at, s, uc, Z0 + h + 0.26, 0.2), (w + 0.4, 0.5, 0.4), eu, Z, en)


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


def bollard_box(p, x, y, s=0.8, h=1.0):
    """O caixote de perigo (a ficha: os blocos listrados no pé dos pilares e nos cantos)."""
    bx(p, 'Hazard#pnl', x - s / 2, x + s / 2, y - s / 2, y + s / 2, Z0 - 0.02, Z0 + h)
    bx(p, 'Trim#vp', x - s / 2 - 0.03, x + s / 2 + 0.03, y - s / 2 - 0.03, y + s / 2 + 0.03, Z0 + h - 0.04, Z0 + h + 0.04)


def paint(p, m, x0, x1, y0, y1, lift=0.0):
    ub.flat_paint(p, m, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], Z0 + 0.002 + lift, Z0 + 0.014 + lift)


# ---------------------------------------------------------------------------------- o chão
def ground(p):
    """A laje do lote em placas de 3,2 m com a junta funda (a ficha: o pátio de concreto com as juntas), a pista do portão com as
    faixas amarelas e as grelhas, a soleira de perigo."""
    nx, ny = 4, 4
    xs = [-HX + 2 * HX * i / nx for i in range(nx + 1)]
    ys = [-HY + 2 * HY * j / ny for j in range(ny + 1)]
    for i in range(nx):
        for j in range(ny):
            x0, x1, y0, y1 = xs[i] + 0.03, xs[i + 1] - 0.03, ys[j] + 0.03, ys[j + 1] - 0.03
            ub.slab(p, 'Concrete#vp', x0, x1, y0, y1, -0.12, Z0)
            xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
            bx(p, 'Dark#vp', xm - 0.03, xm + 0.03, y0 + 0.1, y1 - 0.1, Z0 - 0.004, Z0 + 0.004)
            bx(p, 'Dark#vp', x0 + 0.1, xm - 0.05, ym - 0.03, ym + 0.03, Z0 - 0.004, Z0 + 0.004)
            bx(p, 'Dark#vp', xm + 0.05, x1 - 0.1, ym - 0.03, ym + 0.03, Z0 - 0.004, Z0 + 0.004)
    # a pista do portão: as faixas amarelas das bordas (a faixa de saída de 6,6 m), a linha do eixo tracejada, as grelhas
    for y in (-3.3, 3.3):
        paint(p, 'Yellow', BX0 + 0.4, HX - 0.15, y - 0.08, y + 0.08)
    x = TX1 + 0.8
    while x + 1.2 < HX:
        paint(p, 'Yellow', x, x + 1.2, -0.06, 0.06)
        x += 2.2
    for y in (-1.45, 1.45):
        bx(p, 'Dark#vp', TX1 + 0.6, HX - 0.25, y - 0.32, y + 0.32, Z0 - 0.06, Z0 - 0.02)
        bx(p, 'Trim#vp', TX1 + 0.55, HX - 0.2, y - 0.38, y - 0.32, Z0 - 0.05, Z0 + 0.005)
        bx(p, 'Trim#vp', TX1 + 0.55, HX - 0.2, y + 0.32, y + 0.38, Z0 - 0.05, Z0 + 0.005)
        k = 0
        x = TX1 + 0.75
        while x < HX - 0.35:
            bx(p, 'Trim#vp', x - 0.03, x + 0.03, y - 0.31, y + 0.31, Z0 - 0.05, Z0 - 0.005)
            x += 0.24
            k += 1
    # a soleira de perigo na boca do vão
    paint(p, 'Hazard', TX1 - 0.2, TX1 + 0.6, -BY + 0.1, BY - 0.1, 0.002)


# ---------------------------------------------------------------------------------- o galpão
def hall(p):
    # o miolo (escondido atrás das placas): o bloco alto em 4 caixas em volta do vão, o bloco baixo de trás
    solid(p, TX0, TX1, BY, TY, Z0 - 0.05, TZ)
    solid(p, TX0, TX1, -TY, -BY, Z0 - 0.05, TZ)
    bx(p, 'Body#vp', TX0, TX1, -BY - 0.05, BY + 0.05, BZ, TZ - 0.002)
    solid(p, TX0, BX0, -BY - 0.05, BY + 0.05, Z0 - 0.05, BZ + 0.05)
    solid(p, RX0, TX0 + 0.05, RY0, RY1, Z0 - 0.05, RZ, skip=('+x',))
    # a frente (+X) com o vão; os lados; os fundos
    clad(p, 'x', TX1, 1, -TY, TY, Z0, TZ, holes=[(-BY, BY, Z0, BZ)])
    rollup_y = (0.3, 3.3)
    clad(p, 'y', TY, 1, TX0, TX1, Z0, TZ, holes=[(rollup_y[0] - 0.1, rollup_y[1] + 0.1, Z0, Z0 + 3.9)])
    clad(p, 'y', -TY, -1, TX0 + J, AX0 - 0.2, Z0, TZ)
    clad(p, 'y', -TY, -1, AX0 - 0.2 + 2 * J, TX1, AZ + 0.45, TZ, band=None)
    clad(p, 'x', TX0, -1, RY1 + J, TY, Z0, TZ, holes=[(4.6, 5.8, Z0, Z0 + 2.35)])
    clad(p, 'x', TX0, -1, -TY, RY1 - J, RZ + 0.45, TZ, band=None)
    clad(p, 'x', RX0, -1, RY0, RY1, Z0, RZ, holes=[(-1.4, -0.2, Z0, Z0 + 2.35)])
    clad(p, 'y', RY0, -1, RX0, TX0 - J, Z0, RZ, holes=[(-6.7, -3.7, Z0, Z0 + 3.5), (-2.9, -1.8, Z0, Z0 + 2.35)])
    clad(p, 'y', RY1, 1, RX0, TX0, Z0, RZ, holes=[(-3.6, -2.5, Z0, Z0 + 2.35)])
    # os pilares dos cantos (o ritmo de pé; cobrem a quina das placas)
    for (x, y, z1) in ((TX1, TY, TZ + 0.42), (TX1, -TY, TZ + 0.42), (RX0, RY0, RZ + 0.42)):
        pilaster(p, x + 0.06 * (1 if x > 0 else -1), y + 0.06 * (1 if y > 0 else -1), z1, w=0.56)
    # os parapeitos e os pisos dos tetos
    parapet(p, TX0, TX1, -TY, TY, TZ - 0.12, TZ + 0.42)
    parapet(p, RX0, TX0 - 0.3, RY0, RY1, RZ - 0.12, RZ + 0.42, skip=('+x',))
    roof_deck(p, TX0 + 0.3, TX1 - 0.3, -TY + 0.3, TY - 0.3, TZ, 2, 5)
    roof_deck(p, RX0 + 0.3, TX0 - 0.1, RY0 + 0.3, RY1 - 0.3, RZ, 3, 4)
    # as portas
    rollup(p, 'y', TY, 1, sum(rollup_y) / 2, rollup_y[1] - rollup_y[0], 3.7)
    rollup(p, 'y', RY0, -1, -5.2, 2.8, 3.3)
    door(p, 'x', TX0, -1, 5.2)
    door(p, 'x', RX0, -1, -0.8)
    door(p, 'y', RY0, -1, -2.35)
    door(p, 'y', RY1, 1, -3.05)
    # as calhas de descida (o cano com as braçadeiras do teto até o chão)
    for (x, y, nn, z1) in ((TX1 - 0.5, TY, (0, 1, 0), TZ), (RX0, RY0 + 0.6, (-1, 0, 0), RZ), (RX0, RY1 - 0.6, (-1, 0, 0), RZ)):
        n = Vector(nn)
        q = Vector((x, y, 0.0)) + n * 0.22
        pipe(p, 'Trim', [(q.x, q.y, z1 + 0.25), (q.x, q.y, Z0 + 0.35), (q.x + n.x * 0.3, q.y + n.y * 0.3, Z0 + 0.12)], 0.07, step=1.4)
    # as luminárias de parede (o corpo, a lente, a aba) nos lados
    for (axis, at, s, u, z) in (('y', TY, 1, -0.6, Z0 + 4.9), ('y', -TY, -1, 2.0, Z0 + 4.9), ('y', RY0, -1, -7.2, Z0 + 3.3),
                                ('x', RX0, -1, 1.6, Z0 + 3.3), ('y', RY1, 1, -6.0, Z0 + 3.3)):
        eu, en = axes(axis, s)
        lamp_head(p, wpt(axis, at, s, u, z, 0.22), en - Vector((0, 0, 1.0)), w=0.5, h=0.24)
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, u, z, 0.07), (0.14, 0.24, 0.14), eu, Z, en)
    # a caixa de força e o conduíte (+Y, ao lado da porta de enrolar)
    ub.obox(p, 'Body#pnl', (4.35, TY + 0.25, Z0 + 1.6), (0.7, 0.36, 1.2), X, Y, Z)
    ub.obox(p, 'Paint#vp', (4.35, TY + 0.44, Z0 + 1.95), (0.5, 0.03, 0.16), X, Y, Z)
    pipe(p, 'Dark', [(4.35, TY + 0.2, Z0 + 2.2), (4.35, TY + 0.2, Z0 + 4.6)], 0.05)
    # as venezianas do bloco baixo (os fundos e o lado +Y)
    louvers(p, (RX0 - 0.09, -4.2, Z0 + 2.0), (-1, 0, 0), (0, 1, 0), 1.8, 1.0, 9)
    louvers(p, (RX0 - 0.09, 1.6, Z0 + 1.2), (-1, 0, 0), (0, 1, 0), 1.2, 0.8, 7)
    louvers(p, (-6.4, RY1 + 0.09, Z0 + 2.4), (0, 1, 0), (1, 0, 0), 1.6, 0.9, 8)


def bay(p):
    """O interior do vão (o que a câmera vê pela boca): as paredes de placas escuras, as colunas, o teto escuro com as treliças e as
    calhas de luz, o portão de enrolar do fundo, a caixa da porta recolhida em cima, os batentes de perigo."""
    for s in (-1, 1):
        clad(p, 'y', s * BY, -s, BX0, TX1 - 0.25, Z0, BZ, zrows=[Z0 + 1.8, Z0 + 3.6], band=None, m='Trim#vp', pw=2.0)
        for x in (0.6, 2.6):
            bx(p, 'Body#vp', x - 0.18, x + 0.18, s * (BY - 0.3), s * (BY + 0.05), Z0 - 0.02, BZ)
            ub.obox(p, 'Light#vp', (x, s * (BY - 0.31), Z0 + 3.3), (0.2, 0.04, 0.5), X, Y, Z)
            ub.obox(p, 'Dark#vp', (x, s * (BY - 0.33), Z0 + 3.6), (0.3, 0.1, 0.06), X, Y, Z)
        # as caixas de ferramenta encostadas
        ub.obox(p, 'Trim#vp', (1.6, s * (BY - 0.4), Z0 + 0.6), (1.0, 0.5, 1.2), X, Y, Z)
        for zz in (0.35, 0.65, 0.95):
            ub.obox(p, 'Dark#vp', (1.6, s * (BY - 0.66), Z0 + zz), (0.9, 0.03, 0.05), X, Y, Z)
    # o teto: o forro escuro, as 3 treliças, as 4 calhas de luz
    for x in (0.0, 1.8, 3.6):
        bx(p, 'Trim#vp', x - 0.12, x + 0.12, -BY + 0.05, BY - 0.05, BZ - 0.5, BZ - 0.06)
        bx(p, 'Trim#vp', x - 0.18, x + 0.18, -BY + 0.05, BY - 0.05, BZ - 0.56, BZ - 0.5)
    for y in (-3.6, -1.2, 1.2, 3.6):
        bx(p, 'Dark#vp', BX0 + 0.4, TX1 - 0.9, y - 0.18, y + 0.18, BZ - 0.66, BZ - 0.56)
        bx(p, 'Light#vp', BX0 + 0.45, TX1 - 0.95, y - 0.12, y + 0.12, BZ - 0.69, BZ - 0.66)
    # o fundo: as placas escuras e o portão de enrolar fechado
    clad(p, 'x', BX0, 1, -BY, BY, Z0, BZ, zrows=[Z0 + 2.1, Z0 + 4.2], band=None, m='Trim#vp', pw=2.5,
         holes=[(-2.4, 2.4, Z0, Z0 + 4.1)])
    rollup(p, 'x', BX0, 1, 0.0, 4.6, 4.0, box=False)
    ub.obox(p, 'Light#vp', (BX0 + 0.12, 0.0, Z0 + 4.4), (0.04, 2.6, 0.14), X, Y, Z)
    ub.obox(p, 'Paint#vp', (BX0 + 0.11, -3.6, Z0 + 2.6), (0.04, 1.0, 0.5), X, Y, Z)
    # a boca: o batente de placas grossas, a caixa da porta de enrolar recolhida, a barra de perigo da porta
    for s in (-1, 1):
        bx(p, 'Body#pnl', TX1 - 0.3, TX1 + 0.14, s * BY - 0.25 * (1 + s) / 2 - 0.0, s * BY + 0.25 * (1 - s) / 2, Z0 - 0.02, BZ)
        bx(p, 'Hazard#vp', TX1 - 0.32, TX1 + 0.16, s * BY - 0.27 * (1 + s) / 2, s * BY + 0.27 * (1 - s) / 2, Z0 - 0.02, Z0 + 1.05)
    bx(p, 'Body#pnl', TX1 - 0.65, TX1 - 0.1, -BY + 0.05, BY - 0.05, BZ - 0.42, BZ + 0.02)
    bx(p, 'Hazard#vp', TX1 - 0.45, TX1 - 0.32, -BY + 0.1, BY - 0.1, BZ - 0.54, BZ - 0.42)
    bx(p, 'Trim#pnl', TX1 + 0.02, TX1 + 0.3, -BY - 0.25, BY + 0.25, BZ - 0.02, BZ + 0.42)
    ub.obox(p, 'Player#vp', (TX1 + 0.32, 0.0, BZ + 0.2), (0.04, 3.2, 0.3), X, Y, Z)
    # a linha amarela da pista continua dentro; a grelha do meio
    bx(p, 'Dark#vp', BX0 + 0.5, TX1 - 0.5, -0.25, 0.25, Z0 - 0.04, Z0 + 0.004)


# ---------------------------------------------------------------------------------- o pórtico
def portal(p):
    for s in (-1, 1):
        yc = s * (PY0 + PY1) / 2
        hw = (PY1 - PY0) / 2
        # a sapata, o miolo, as placas (o pé de perigo na 1ª fileira), a capa
        bx(p, 'Concrete#pnl', PX0 - 0.18, PX1 + 0.18, yc - hw - 0.18, yc + hw + 0.18, -0.1, Z0 + 0.3)
        solid(p, PX0, PX1, yc - hw, yc + hw, Z0, BEAM[0] + 0.05)
        zr = [Z0 + 1.1, Z0 + 2.9, Z0 + 4.5]
        mats = {Z0 + 0.3: 'Hazard#pnl'}
        zt = BEAM[0] - J
        clad(p, 'x', PX1, 1, yc - hw, yc + hw, Z0 + 0.3, zt, zrows=[Z0 + 1.3] + zr[1:], band=None, pw=2.0, mats=mats)
        clad(p, 'x', PX0, -1, yc - hw, yc + hw, Z0 + 0.3, zt, zrows=[Z0 + 1.3] + zr[1:], band=None, pw=2.0, mats=mats)
        clad(p, 'y', yc + s * hw, s, PX0, PX1, Z0 + 0.3, zt, zrows=[Z0 + 1.3] + zr[1:], band=None, pw=2.0, mats=mats)
        clad(p, 'y', yc - s * hw, -s, PX0, PX1, Z0 + 0.3, zt, zrows=[Z0 + 1.3] + zr[1:], band=None, pw=2.0, mats=mats)
        for (xx, yy) in ((PX1, yc + s * hw), (PX1, yc - s * hw)):           # as quinas da frente: a cantoneira escura de pé
            bx(p, 'Trim#vp', xx - 0.06, xx + 0.1, yy - 0.08, yy + 0.08, Z0 + 0.3, zt)
        # a viga de rolamento: do galpão até a frente do pilar, as placas, o trilho em cima, o guarda-corpo de fora
        bx(p, 'Body#vp', TX1 - 0.2, PX1, yc - hw, yc + hw, BEAM[0], BEAM[1])
        clad(p, 'y', yc + s * hw, s, TX1 + 0.1, PX1, BEAM[0] + J, BEAM[1], zrows=[], band=None, pw=2.05)
        clad(p, 'y', yc - s * hw, -s, TX1 + 0.1, PX1, BEAM[0] + J, BEAM[1], zrows=[], band=None, pw=2.05)
        clad(p, 'x', PX1, 1, yc - hw, yc + hw, BEAM[0] + J, BEAM[1], zrows=[], band=None, pw=2.0)
        bx(p, 'Body#pnl', TX1 + 0.05, PX1 + 0.1, yc - hw - 0.1, yc + hw + 0.1, BEAM[1] - 0.02, BEAM[1] + 0.16)
        bx(p, 'Trim#vp', TX1 + 0.1, PX1 + 0.05, yc - 0.12, yc + 0.12, BEAM[1] + 0.16, BEAM[1] + 0.26)
        bx(p, 'Dark#vp', TX1 + 0.1, PX1 + 0.05, yc - 0.05, yc + 0.05, BEAM[1] + 0.26, BEAM[1] + 0.32)
        rail(p, 'Trim', [(TX1 + 0.2, yc + s * (hw - 0.05)), (PX1 - 0.05, yc + s * (hw - 0.05))], BEAM[1] + 0.16, h=0.95, step=1.3)
        # a mão-francesa do galpão até a viga (o tubo grosso) e a caixa de cima com o holofote e a baliza
        vp.tube(p, 'Trim', [(TX1 + 0.12, yc, BEAM[0] - 1.7), (TX1 + 1.4, yc, BEAM[0] + 0.02)], 0.1, seg=10)
        lamp_head(p, (PX1 + 0.2, yc - s * 0.45, BEAM[1] - 0.35), (1.0, -s * 0.5, -0.9), w=0.5, h=0.28)
        ub.obox(p, 'Trim#vp', (PX1 + 0.07, yc - s * 0.45, BEAM[1] - 0.35), (0.14, 0.2, 0.2), X, Y, Z)
        # as luzes da face de dentro (viradas pra pista) e o conduíte
        for z in (Z0 + 2.2, Z0 + 4.4):
            lamp_head(p, ((PX0 + PX1) / 2, s * (PY0 - 0.2), z), (0.4, -s * 1.0, -0.5), w=0.42, h=0.22)
        pipe(p, 'Dark', [(PX0 + 0.25, s * (PY0 - 0.12), Z0 + 0.4), (PX0 + 0.25, s * (PY0 - 0.12), BEAM[0] - 0.1)], 0.04, step=1.2)
        # os frades de perigo na frente
        bollard_box(p, PX1 + 0.7, yc - s * 0.3)
    ponte(p)


def ponte(p):
    """A ponte rolante amarela: a viga-caixão com as nervuras, os carros de ponta em cima das vigas com as rodas no trilho, o trole
    com o tambor e o motor, os 2 cabos e o moitão com o gancho, a passarela com o corrimão."""
    for (a, b) in ((GX0, GX0 + 0.3), (GX1 - 0.3, GX1)):
        bx(p, 'Yellow#pnl', a, b, -PY0 - 0.12, PY0 + 0.12, GZ0, GZ1)
    for y in (-PY0 + 0.3, -1.0, 1.0, PY0 - 0.3):
        bx(p, 'Yellow#vp', GX0 + 0.28, GX1 - 0.28, y - 0.1, y + 0.1, GZ0 + 0.05, GZ0 + 0.25)
    for k in range(7):
        y = -PY0 + 0.6 + k * (2 * PY0 - 1.2) / 6
        for x in (GX0 - 0.04, GX1 + 0.04):
            bx(p, 'Yellow#vp', x - 0.04, x + 0.04, y - 0.06, y + 0.06, GZ0 + 0.06, GZ1 - 0.06)
    ub.obox(p, 'Dark#vp', (GX1 + 0.07, 0.0, (GZ0 + GZ1) / 2), (0.02, 2.4, 0.3), X, Y, Z)
    ub.obox(p, 'Paint#vp', (GX1 + 0.085, 0.0, (GZ0 + GZ1) / 2), (0.02, 2.2, 0.2), X, Y, Z)
    for s in (-1, 1):
        yc = s * (PY0 + PY1) / 2
        bx(p, 'Yellow#pnl', GX0 - 0.35, GX1 + 0.35, yc - 0.62, yc + 0.62, BEAM[1] + 0.3, BEAM[1] + 0.78)
        bx(p, 'Yellow#vp', GX0 - 0.02, GX1 + 0.02, s * (PY0 - 0.02), s * (PY0 + 0.2), GZ0 + 0.2, BEAM[1] + 0.55)
        for x in (GX0 - 0.12, GX1 + 0.12):
            vp.lathe(p, 'Dark#vp', (x, yc, BEAM[1] + 0.42), 'y', [(0.0, -0.08), (0.17, -0.08), (0.17, 0.08), (0.0, 0.08)], seg=14)
        ub.beacon(p, ((GX0 + GX1) / 2, yc, BEAM[1] + 0.78), r=0.07, h=0.14, cage=False)
    # o trole
    tz = GZ1
    bx(p, 'Yellow#pnl', GX0 - 0.12, GX1 + 0.12, -0.75, 0.75, tz, tz + 0.5)
    vp.lathe(p, 'Trim#vp', ((GX0 + GX1) / 2, 0.0, tz + 0.72), 'y', [(0.0, -0.55), (0.24, -0.55), (0.24, -0.48), (0.2, -0.45), (0.2, 0.45),
                                                                    (0.24, 0.48), (0.24, 0.55), (0.0, 0.55)], seg=20)
    vp.rbox(p, 'Dark', (GX0 + 0.1, 0.0, tz + 0.75), (0.36, 0.5, 0.42), r=0.06, k=2)
    for y in (-0.25, 0.25):
        vp.tube(p, 'Dark', [((GX0 + GX1) / 2, y, tz + 0.05), ((GX0 + GX1) / 2, y * 0.6, 4.95)], 0.025, seg=6)
    # o moitão e o gancho (o tanque tem 3 m: folga de 1 m)
    vp.rbox(p, 'Yellow', ((GX0 + GX1) / 2, 0.0, 4.7), (0.42, 0.5, 0.5), r=0.08, k=2)
    vp.lathe(p, 'Trim#vp', ((GX0 + GX1) / 2, 0.0, 4.82), 'x', [(0.0, -0.23), (0.17, -0.23), (0.17, 0.23), (0.0, 0.23)], seg=14)
    hook = [Vector(((GX0 + GX1) / 2, 0.0, 4.45))] + [Vector(((GX0 + GX1) / 2, 0.16 * math.sin(math.radians(a)), 4.22 - 0.16 * math.cos(math.radians(a))))
                                                     for a in range(0, 271, 30)]
    vp.tube(p, 'Trim', hook, 0.045, seg=8)
    # a passarela de trás com o corrimão
    bx(p, 'Trim#vp', GX0 - 0.6, GX0 - 0.02, -PY0, PY0, GZ1 - 0.08, GZ1)
    rail(p, 'Yellow', [(GX0 - 0.55, -PY0 + 0.1), (GX0 - 0.55, PY0 - 0.1)], GZ1, h=0.9, r=0.022, step=1.4)


# ---------------------------------------------------------------------------------- o teto
def roof(p):
    # a caixa dos ventiladores: o corpo de placas, a tampa grossa com os 2 poços, as venezianas, o pé
    hx, t = (FH_X1 - FH_X0) / 2, 0.24
    xc = (FH_X0 + FH_X1) / 2
    bx(p, 'Trim#vp', FH_X0 - 0.1, FH_X1 + 0.1, -FH_Y - 0.1, FH_Y + 0.1, TZ - 0.02, TZ + 0.14)
    zs = FH_TOP - 0.66                                      # o corpo baixo (embaixo do fundo dos poços) e a saia oca em volta
    solid(p, FH_X0 + 0.04, FH_X1 - 0.04, -FH_Y + 0.04, FH_Y - 0.04, TZ + 0.1, zs)
    for (a, b, c, d) in ((FH_X0 + 0.04, FH_X1 - 0.04, FH_Y - 0.16, FH_Y - 0.04), (FH_X0 + 0.04, FH_X1 - 0.04, -FH_Y + 0.04, -FH_Y + 0.16),
                         (FH_X0 + 0.04, FH_X0 + 0.16, -FH_Y + 0.18, FH_Y - 0.18), (FH_X1 - 0.16, FH_X1 - 0.04, -FH_Y + 0.18, FH_Y - 0.18)):
        bx(p, 'Body#pnl', a, b, c, d, zs - 0.05, FH_TOP - t + 0.02)
    for s in (-1, 1):
        ub.square_annulus(p, 'Body#sm', (xc, s * FAN_Y), hx, FH_Y / 2 - 0.03, FAN_R, FH_TOP, t)
        fan_well(p, (FAN_X, s * FAN_Y))
        louvers(p, (FH_X1 + 0.005, s * FAN_Y, (TZ + 0.1 + FH_TOP - t) / 2 + 0.02), (1, 0, 0), (0, 1, 0), 2.6, 0.4, 5)
    bx(p, 'Trim#vp', FH_X0 + 0.05, FH_X1 - 0.05, -0.05, 0.05, FH_TOP - 0.02, FH_TOP + 0.04)
    ub.obox(p, 'Player#vp', (FH_X1 + 0.03, 0.0, FH_TOP - 0.12), (0.04, 2.4, 0.16), X, Y, Z)
    # os condensadores e a casa da escada nos cantos de trás do teto alto
    fan_box(p, (TX0 + 1.0, 5.2), (1.4, 1.0, 0.85), fans=1, z0=TZ)
    fan_box(p, (TX0 + 1.0, -5.2), (1.4, 1.0, 0.85), fans=1, z0=TZ)
    ub.obox(p, 'Body#pnl', (3.6, -5.5, TZ + 0.5), (1.2, 1.4, 1.0), X, Y, Z)
    louvers(p, (4.21, -5.5, TZ + 0.55), (1, 0, 0), (0, 1, 0), 0.9, 0.6, 6)
    # o corrimão da frente e dos lados do teto alto (atrás do parapeito), as balizas, a antena
    rail(p, 'Trim', [(TX0 + 0.5, TY - 0.45), (TX1 - 0.45, TY - 0.45), (TX1 - 0.45, -TY + 0.45), (TX0 + 0.5, -TY + 0.45)], TZ, h=1.0,
         step=2.0, mid=False)
    for (x, y) in ((TX1 - 0.2, TY - 0.2), (TX1 - 0.2, -TY + 0.2), (TX0 + 0.2, TY - 0.2)):
        ub.beacon(p, (x, y, TZ + 0.5), r=0.08, h=0.17)
    ub.antenna(p, TX0 + 0.55, -2.4, TZ, 3.2, r=0.05)
    ub.antenna(p, TX0 + 0.55, -3.0, TZ, 2.2, r=0.05)
    # a escada de marinheiro do teto baixo até o alto
    ub.ladder(p, 'Trim', TX0, -1.3, RZ, TZ + 0.42, (-1, 0, 0), w=0.5, cage=False, wall=0.16, top_ext=0.8)
    # o teto baixo: as 5 claraboias (a fileira azul da ficha), o condensador grande, a casa da escada, os exaustores, o corrimão
    for k in range(5):
        y = RY0 + 1.0 + k * 1.75
        if y + 1.4 > RY1 - 0.3:
            break
        bx(p, 'Body#sm', -7.3, -4.7, y, y + 1.4, RZ - 0.05, RZ + 0.32)
        ub.obox(p, 'Glass#vp', (-6.0, y + 0.7, RZ + 0.34), (2.4, 1.2, 0.04), X, Y, Z)
        bx(p, 'Trim#vp', -6.04, -5.96, y + 0.08, y + 1.32, RZ + 0.33, RZ + 0.38)
    fan_box(p, (-3.0, -4.6), (2.6, 1.4, 1.0), fans=2, z0=RZ)
    ub.obox(p, 'Body#pnl', (-3.0, 1.4, RZ + 0.95), (1.8, 2.0, 1.9), X, Y, Z)
    door(p, 'x', -2.1, 1, 1.4, w=0.9, h=1.75, canopy=False, lamp=True, zb=RZ)
    for (x, y) in ((-3.0, -1.4), (-2.4, -6.1)):
        ub.turbine_vent(p, x, y, RZ, r=0.3)
    rail(p, 'Trim', [(TX0 - 0.45, RY0 + 0.45), (RX0 + 0.45, RY0 + 0.45), (RX0 + 0.45, RY1 - 0.45), (TX0 - 0.45, RY1 - 0.45)], RZ, h=1.0,
         step=2.0, mid=False)
    ub.ladder(p, 'Trim', RX0, -2.6, Z0, RZ + 0.42, (-1, 0, 0), w=0.5, cage=False, wall=0.2, top_ext=0.9)
    for (x, y) in ((RX0 + 0.2, RY0 + 0.2), (RX0 + 0.2, RY1 - 0.2)):
        ub.beacon(p, (x, y, RZ + 0.5), r=0.08, h=0.17)


def fan_well(p, c):
    """O poço do ventilador (fica no prédio; o rotor gira à parte): a parede e o fundo escuro, a moldura saliente, o motor e as 2
    travessas embaixo do rotor, a grade de proteção (o anel e as barras ≥ 4 cm)."""
    x, y = c
    depth = 0.62
    vp.lathe(p, 'Dark#vp', (x, y, FH_TOP), 'z', [(FAN_R - 0.002, 0.0), (FAN_R - 0.002, -depth), (0.0, -depth)], seg=40, smooth=True)
    vp.lathe(p, 'Trim#vp', (x, y, FH_TOP), 'z', [(FAN_R - 0.03, 0.0), (FAN_R - 0.03, 0.06), (FAN_R + 0.02, 0.1), (FAN_R + 0.12, 0.1),
                                                 (FAN_R + 0.15, 0.05), (FAN_R + 0.15, 0.0)], seg=48, smooth=True, closed=True)
    zb = FH_TOP - depth                                     # o fundo do poço; o motor e as travessas logo acima, embaixo do rotor
    vp.lathe(p, 'Dark#vp', (x, y, zb + 0.005), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.1), (0.26, 0.135), (0.0, 0.135)], seg=20)
    for a in (45.0, 135.0):
        d = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0.0))
        vp.tube(p, 'Trim', [Vector((x, y, zb + 0.07)) - d * (FAN_R - 0.02), Vector((x, y, zb + 0.07)) + d * (FAN_R - 0.02)], 0.05, seg=8)
    zg = FH_TOP + 0.13
    p.torus('Trim#vp', (x, y, zg), 'z', FAN_R - 0.02, 0.026, seg=48, rseg=6)
    p.torus('Trim#vp', (x, y, zg), 'z', FAN_R * 0.5, 0.022, seg=32, rseg=6)
    for k in range(4):
        a = math.pi * k / 4 + math.pi / 8
        d = Vector((math.cos(a), math.sin(a), 0.0))
        vp.tube(p, 'Trim', [Vector((x, y, zg)) - d * (FAN_R - 0.02), Vector((x, y, zg)) + d * (FAN_R - 0.02)], 0.022, seg=6)


# ---------------------------------------------------------------------------------- o canto de trás (+Y): o domo, as chaminés
def radome(p):
    cx, cy = RC
    zp = 5.1                                                # a plataforma
    for sx in (-1, 1):
        for sy in (-1, 1):
            bx(p, 'Concrete#pnl', cx + sx * 1.2 - 0.32, cx + sx * 1.2 + 0.32, cy + sy * 1.2 - 0.32, cy + sy * 1.2 + 0.32, -0.1, Z0 + 0.35)
            vp.tube(p, 'Trim', [(cx + sx * 1.2, cy + sy * 1.2, Z0 + 0.3), (cx + sx * 0.98, cy + sy * 0.98, zp)], 0.11, seg=10)
            vp.bolt_ring(p, 'Dark', (cx + sx * 1.2, cy + sy * 1.2, Z0 + 0.35), 'z', 0.2, 6, r=0.03, h=0.02)
    # o travamento: os anéis em 2 alturas e os X de cada face
    for z0_, z1_ in ((Z0 + 0.5, 2.95), (2.95, zp - 0.05)):
        rr0 = 1.2 - (1.2 - 0.98) * (z0_ - Z0) / (zp - Z0)
        rr1 = 1.2 - (1.2 - 0.98) * (z1_ - Z0) / (zp - Z0)
        cs0 = [(cx + a * rr0, cy + b * rr0) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        cs1 = [(cx + a * rr1, cy + b * rr1) for a, b in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        for k in range(4):
            a0, b0 = cs0[k], cs0[(k + 1) % 4]
            a1, b1 = cs1[k], cs1[(k + 1) % 4]
            vp.tube(p, 'Trim', [(a0[0], a0[1], z0_), (b1[0], b1[1], z1_)], 0.045, seg=6)
            vp.tube(p, 'Trim', [(b0[0], b0[1], z0_), (a1[0], a1[1], z1_)], 0.045, seg=6)
            vp.tube(p, 'Trim', [(a1[0], a1[1], z1_), (b1[0], b1[1], z1_)], 0.055, seg=6)
    # a plataforma octogonal com a borda, o guarda-corpo, a escada
    oct8 = lambda r, z: [(cx + r * math.cos(math.radians(45 * k + 22.5)), cy + r * math.sin(math.radians(45 * k + 22.5)), z) for k in range(8)]
    p.loft('Body#pnl', [oct8(1.95, zp - 0.05), oct8(1.95, zp + 0.14), oct8(1.88, zp + 0.2)])
    rp = [(q[0], q[1]) for q in oct8(1.86, 0)]
    rail(p, 'Trim', rp + [rp[0]], zp + 0.2, h=0.9, r=0.022, step=1.2)
    ub.ladder(p, 'Trim', cx + 1.25, cy - 0.2, Z0, zp - 0.1, (1, 0, 0), w=0.45, cage=False, wall=0.12, top_ext=0.0)
    # o pedestal e a esfera (as costuras: 2 paralelos e 6 meridianos)
    vp.lathe(p, 'Trim#vp', (cx, cy, zp + 0.2), 'z', [(0.0, 0.0), (1.2, 0.0), (1.2, 0.12), (0.9, 0.22), (0.9, 0.45), (0.0, 0.45)], seg=32)
    zc = RZC
    prof = [(R_SPH * math.sin(math.radians(a)), -R_SPH * math.cos(math.radians(a))) for a in range(0, 181, 10)]
    prof[0] = (0.0, prof[0][1])
    prof[-1] = (0.0, prof[-1][1])
    vp.lathe(p, 'Paint#vp', (cx, cy, zc), 'z', prof, seg=40, smooth=True, hard=60.0)
    for a in (-20.0, 22.0, 55.0):
        rr = R_SPH * math.cos(math.radians(a))
        p.torus('Body#vp', (cx, cy, zc + R_SPH * math.sin(math.radians(a))), 'z', rr + 0.008, 0.028, seg=40, rseg=5)
    for k in range(6):
        t = math.radians(30 + 60 * k)
        pts = [Vector((cx + (R_SPH + 0.012) * math.cos(math.radians(a)) * math.cos(t), cy + (R_SPH + 0.012) * math.cos(math.radians(a)) * math.sin(t),
                       zc + (R_SPH + 0.012) * math.sin(math.radians(a)))) for a in range(-40, 81, 10)]
        vp.tube(p, 'Body', pts, 0.026, seg=5)
    ub.beacon(p, (cx, cy, zc + R_SPH - 0.02), r=0.08, h=0.16, cage=False)
    # os canos do pé da torre pro bloco baixo
    for (z, r) in ((Z0 + 0.6, 0.15), (Z0 + 1.15, 0.11)):
        pipe(p, 'Trim', [(cx - 0.5, cy - 0.3, z), (cx - 0.5, RY1 + 0.45, z), (cx + 0.6, RY1 + 0.45, z), (cx + 0.6, RY1 + 0.1, z)], r,
                step=1.2)


def chimneys(p):
    x0, x1, y0, y1, h = UBK
    # a casa de máquinas: o miolo, as placas com a faixa azul, o parapeito, a porta, as venezianas, o piso do teto
    solid(p, x0, x1, y0, y1, Z0 - 0.05, h)
    zr = [Z0 + 1.5, Z0 + 1.95]
    band = (Z0 + 1.5, Z0 + 1.95)
    clad(p, 'x', x1, 1, y0, y1, Z0, h, zrows=zr, band=band, holes=[(9.0, 10.1, Z0, Z0 + 2.25)])
    clad(p, 'y', y0, -1, x0, x1, Z0, h, zrows=zr, band=band)
    clad(p, 'y', y1, 1, x0, x1, Z0, h, zrows=zr, band=band)
    parapet(p, x0, x1, y0, y1, h - 0.1, h + 0.36, t=0.28)
    roof_deck(p, x0 + 0.25, x1 - 0.25, y0 + 0.25, y1 - 0.25, h, 1, 2)
    door(p, 'x', x1, 1, 9.55, w=1.0)
    louvers(p, (x1 + 0.09, 11.3, Z0 + 1.3), (1, 0, 0), (0, 1, 0), 1.1, 0.9, 8)
    fan_box(p, ((x0 + x1) / 2, (y0 + y1) / 2 + 0.6), (1.4, 1.0, 0.8), fans=1, z0=h)
    # as chaminés: a sapata, o fuste torneado com os anéis, a boca com a borda e o fundo de fuligem, a escada com gaiola, a baliza
    for (cx, cy) in CHIM:
        bx(p, 'Concrete#pnl', cx - 0.95, cx + 0.95, cy - 0.95, cy + 0.95, -0.1, Z0 + 0.4)
        prof = [(0.0, 0.0), (CH_R + 0.12, 0.0), (CH_R + 0.12, 0.25), (CH_R + 0.03, 0.32), (CH_R, 0.4), (CH_R, CH_TOP - Z0 - 0.75),
                (CH_R + 0.07, CH_TOP - Z0 - 0.7), (CH_R + 0.07, CH_TOP - Z0 - 0.05), (CH_R + 0.02, CH_TOP - Z0), (CH_R - 0.1, CH_TOP - Z0),
                (CH_R - 0.1, CH_TOP - Z0 - 0.3), (0.0, CH_TOP - Z0 - 0.3)]
        vp.lathe(p, 'Body#vp', (cx, cy, Z0 + 0.4), 'z', prof, seg=28, smooth=True)
        vp.lathe(p, 'Dark#vp', (cx, cy, CH_TOP - 0.28), 'z', [(0.0, 0.0), (CH_R - 0.11, 0.0), (CH_R - 0.11, 0.04), (0.0, 0.04)], seg=28)
        for z in (2.4, 4.4, 6.2):
            vp.lathe(p, 'Trim#vp', (cx, cy, z), 'z', [(CH_R - 0.01, -0.07), (CH_R + 0.06, -0.07), (CH_R + 0.06, 0.07), (CH_R - 0.01, 0.07)],
                     seg=36, closed=True)
            vp.bolt_ring(p, 'Dark', (cx, cy, z + 0.07), 'z', CH_R + 0.03, 12, r=0.025, h=0.016)
        vp.lathe(p, 'Player#vp', (cx, cy, 5.3), 'z', [(CH_R + 0.005, -0.22), (CH_R + 0.02, -0.22), (CH_R + 0.02, 0.22), (CH_R + 0.005, 0.22)],
                 seg=36, closed=True)
        # o cano da casa de máquinas até a chaminé
        pipe(p, 'Trim', [(x0 - 0.05, cy, 1.7), (cx + CH_R - 0.05, cy, 1.7)], 0.16, step=0.8)
        ub.beacon(p, (cx - CH_R - 0.02, cy, CH_TOP - 0.05), r=0.06, h=0.12, cage=False)
    ub.ladder(p, 'Trim', CHIM[1][0], CHIM[1][1] + CH_R, Z0 + 0.4, CH_TOP - 0.3, (0, 1, 0), w=0.45, cage=False, wall=0.1, top_ext=0.6)
    # o quadro de amarração das 2 chaminés até o teto da casa (os tubos nos anéis de 4,4 e 6,2)
    for z in (4.4, 6.2):
        ya, yb = CHIM[0][1], CHIM[1][1]
        vp.tube(p, 'Trim', [(CHIM[0][0] + CH_R * 0.7, ya + CH_R * 0.7, z), (CHIM[1][0] + CH_R * 0.7, yb - CH_R * 0.7, z)], 0.06, seg=8)
        vp.tube(p, 'Trim', [(CHIM[0][0] - CH_R * 0.7, ya + CH_R * 0.7, z), (CHIM[1][0] - CH_R * 0.7, yb - CH_R * 0.7, z)], 0.06, seg=8)
    for (cx, cy) in CHIM:
        vp.tube(p, 'Trim', [(cx + CH_R + 0.04, cy, 4.4), (x0 + 0.3, cy, h + 0.36)], 0.07, seg=8)
    # a linha de canos da casa de máquinas até o galpão (no cavalete), com a válvula de volante
    for (z, r, yy) in ((2.55, 0.17, 7.05), (2.95, 0.11, 7.3)):
        pipe(p, 'Trim', [(x1 + 0.05, yy + 0.6, z), (x1 + 0.6, yy + 0.6, z), (x1 + 1.1, yy, z), (TX0 - 0.6, yy, z), (TX0 - 0.6, 6.0, z),
                            (TX0 - 0.1, 6.0, z)], r, step=1.6)
    for x in (-5.2, -3.6, -2.2):
        vp.tube(p, 'Trim', [(x, 7.15, Z0 + 0.3), (x, 7.15, 2.33)], 0.06, seg=8)
        bx(p, 'Trim#vp', x - 0.06, x + 0.06, 6.85, 7.5, 2.33, 2.41)
        bx(p, 'Concrete#pnl', x - 0.22, x + 0.22, 6.93, 7.37, -0.05, Z0 + 0.3)
    vp.lathe(p, 'Yellow#vp', (-4.4, 7.05, 2.55), 'y', [(0.0, -0.28), (0.24, -0.28), (0.24, 0.28), (0.0, 0.28)], seg=16)
    vp.tube(p, 'Yellow', [(-4.4, 7.05, 2.79), (-4.4, 7.05, 3.05)], 0.03, seg=6)
    p.torus('Yellow#vp', (-4.4, 7.05, 3.08), 'z', 0.2, 0.025, seg=16, rseg=6)


# ---------------------------------------------------------------------------------- o pátio do braço robótico (+Y, frente)
def robot_yard(p):
    x0, x1, y0, y1 = PAD
    w = 0.45
    for (a, b, c, d) in ((x0, x1, y0, y0 + w), (x0, x1, y1 - w, y1), (x0, x0 + w, y0 + w, y1 - w), (x1 - w, x1, y0 + w, y1 - w)):
        paint(p, 'Hazard', a, b, c, d)
    kk = 1.3
    q = robot_arm(p, (3.4, 10.6), -32.0, k=kk)
    # a bancada com a peça (o pacote do motor) que o braço aperta, embaixo da ferramenta
    ub.obox(p, 'Trim#vp', (q.x, q.y, Z0 + 0.84 * kk), (1.5 * kk, 1.1 * kk, 0.08), X, Y, Z)
    for sx in (-1, 1):
        for sy in (-1, 1):
            vp.tube(p, 'Trim', [(q.x + sx * 0.66 * kk, q.y + sy * 0.46 * kk, Z0), (q.x + sx * 0.66 * kk, q.y + sy * 0.46 * kk, Z0 + 0.8 * kk)],
                    0.045, seg=8)
    vp.rbox(p, 'Dark', (q.x, q.y, Z0 + 1.13 * kk), (1.0 * kk, 0.72 * kk, 0.5 * kk), yaw=-32, r=0.08, k=2)
    vp.rbox(p, 'Body', (q.x - 0.1, q.y + 0.05, Z0 + 1.46 * kk), (0.6 * kk, 0.5 * kk, 0.18 * kk), yaw=-32, r=0.06, k=2)
    for k in (-1, 1):
        vp.lathe(p, 'Trim#vp', (q.x + 0.32 * k * kk, q.y + 0.2 * k * kk, Z0 + 1.38 * kk), 'z',
                 [(0.0, 0.0), (0.15, 0.0), (0.15, 0.24), (0.0, 0.24)], seg=12)
    # as caixas de munição e o armário de ferramentas
    for (x, y, yaw) in ((7.4, 11.5, 4.0), (7.4, 10.7, -3.0), (7.45, 11.1, 8.0)):
        zz = Z0 + (0.0 if y != 11.1 else 0.5)
        vp.rbox(p, 'Trim', (x, y, zz + 0.25), (1.0, 0.6, 0.5), yaw=yaw, r=0.05, k=2)
        ub.obox(p, 'Paint#vp', (x - 0.51, y, zz + 0.3), (0.02, 0.4, 0.12), X, Y, Z)
        for d in (-0.3, 0.3):
            ub.obox(p, 'Dark#vp', (x + d, y - 0.31, zz + 0.4), (0.1, 0.03, 0.08), X, Y, Z)
    ub.obox(p, 'Body#pnl', (2.15, 8.75, Z0 + 0.95), (0.5, 1.2, 1.9), X, Y, Z)
    ub.obox(p, 'Player#vp', (2.415, 8.75, Z0 + 1.7), (0.03, 1.0, 0.14), X, Y, Z)
    vp.grab_handle(p, 'Trim', (2.42, 9.05, Z0 + 0.8), (2.42, 9.05, Z0 + 1.2), (1, 0, 0), height=0.05, r=0.014)
    bollard_box(p, x1 - 0.1, y0 + 0.1, s=0.7, h=0.9)


def robot_arm(p, c, yaw, k=1.3):
    """O braço de 6 eixos parado no gesto de apertar a peça (k = a escala: na ficha ele tem ~60% da altura do galpão): a base
    torneada, a coluna, o ombro de garfo, o braço e o antebraço de quinas redondas, as tampas azuis das juntas, as mangueiras, o punho
    e a ferramenta."""
    ex = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))
    ew = Vector((-ex.y, ex.x, 0.0))
    zz = Vector((0, 0, 1))
    B = Vector((c[0], c[1], Z0))
    bx(p, 'Concrete#pnl', B.x - 0.8 * k, B.x + 0.8 * k, B.y - 0.8 * k, B.y + 0.8 * k, Z0 - 0.04, Z0 + 0.22)
    vp.bolt_ring(p, 'Dark', (B.x, B.y, Z0 + 0.22), 'z', 0.68 * k, 8, r=0.035, h=0.03)
    q = lambda prof: [(r_ * k, a_ * k) for r_, a_ in prof]
    vp.lathe(p, 'Body#vp', B + zz * 0.22, 'z', q([(0.0, 0.0), (0.62, 0.0), (0.62, 0.12), (0.55, 0.18), (0.5, 0.55), (0.56, 0.6),
                                                 (0.56, 0.7), (0.0, 0.7)]), seg=28)
    vp.lathe(p, 'Player#vp', B + zz * (0.22 + 0.7 * k), 'z', q([(0.0, 0.0), (0.5, 0.0), (0.5, 0.12), (0.0, 0.12)]), seg=28)
    vp.lathe(p, 'Body#vp', B + zz * (0.22 + 0.82 * k), 'z', q([(0.0, 0.0), (0.46, 0.0), (0.46, 0.45), (0.36, 0.55), (0.0, 0.55)]), seg=24)
    S = B + zz * (0.22 + 1.73 * k) + ex * (0.1 * k)
    # o ombro: o garfo (2 chapas) e o eixo com as tampas azuis
    for sw in (-1, 1):
        prof = [(u * k, v * k) for u, v in ((-0.5, -0.36), (0.5, -0.36), (0.42, 0.1), (0.25, 0.36), (-0.25, 0.36), (-0.42, 0.1))]
        vp.prism(p, 'Body', prof, S + ew * (sw * 0.3 * k), ex, zz, ew * sw, 0.14 * k)
        vp.lathe(p, 'Player#vp', S + ew * (sw * 0.44 * k), ew * sw, q([(0.0, 0.0), (0.3, 0.0), (0.3, 0.06), (0.24, 0.12), (0.0, 0.13)]),
                 seg=24)
    vp.lathe(p, 'Trim#vp', S - ew * (0.45 * k), ew, q([(0.0, 0.0), (0.2, 0.0), (0.2, 0.9), (0.0, 0.9)]), seg=18)
    # o braço (62° pra cima), o cotovelo, o antebraço (55° pra baixo), o punho, a ferramenta
    a1, L1 = 62.0, 2.0 * k
    E = S + (ex * math.cos(math.radians(a1)) + zz * math.sin(math.radians(a1))) * L1
    vp.rbox(p, 'Body', (S + E) / 2, (L1 + 0.3 * k, 0.42 * k, 0.36 * k), yaw=yaw, pitch=a1, r=0.12 * k, k=3)
    vp.rbox(p, 'Trim', S - (E - S).normalized() * (0.45 * k), (0.5 * k, 0.5 * k, 0.42 * k), yaw=yaw, pitch=a1, r=0.08 * k, k=2)
    vp.lathe(p, 'Trim#vp', E - ew * (0.33 * k), ew, q([(0.0, 0.0), (0.2, 0.0), (0.2, 0.66), (0.0, 0.66)]), seg=18)
    for sw in (-1, 1):
        vp.lathe(p, 'Player#vp', E + ew * (sw * 0.3 * k), ew * sw, q([(0.0, 0.0), (0.24, 0.0), (0.24, 0.05), (0.19, 0.1), (0.0, 0.11)]),
                 seg=20)
    a2, L2 = -55.0, 1.8 * k
    W = E + (ex * math.cos(math.radians(a2)) + zz * math.sin(math.radians(a2))) * L2
    vp.rbox(p, 'Body', (E + W) / 2, (L2 + 0.2 * k, 0.32 * k, 0.3 * k), yaw=yaw, pitch=a2, r=0.1 * k, k=3)
    d2 = (W - E).normalized()
    vp.lathe(p, 'Player#vp', W - d2 * (0.12 * k), d2, q([(0.0, 0.0), (0.19, 0.0), (0.19, 0.16), (0.0, 0.16)]), seg=18)
    vp.lathe(p, 'Trim#vp', W + d2 * (0.04 * k), d2, q([(0.0, 0.0), (0.15, 0.0), (0.15, 0.12), (0.1, 0.18), (0.0, 0.18)]), seg=16)
    T = W - zz * (0.1 * k) + ex * (0.12 * k)
    vp.lathe(p, 'Dark#vp', T, (0, 0, -1), q([(0.0, 0.0), (0.13, 0.0), (0.13, 0.22), (0.06, 0.3), (0.05, 0.42), (0.0, 0.42)]), seg=14)
    for sw in (-1, 1):
        vp.rbox(p, 'Trim', T + ew * (sw * 0.09 * k) - zz * (0.38 * k), (0.08 * k, 0.05 * k, 0.22 * k), yaw=yaw, r=0.015 * k, k=1)
    # as mangueiras (uma azul, uma escura) da base pelo braço até o cotovelo
    for (off, m, r) in ((0.26, 'Player', 0.045), (-0.24, 'Dark', 0.04)):
        q0 = B + zz * (0.22 + 0.68 * k) - ex * (0.45 * k) + ew * (off * k)
        q1 = S + ew * (off * 1.25 * k) - ex * (0.15 * k) + zz * (0.2 * k)
        q2 = (S + E) / 2 + ew * (off * 0.9 * k) + zz * (0.05 * k) - ex * (0.15 * k)
        q3 = E + ew * (off * 0.9 * k) - ex * (0.1 * k) + zz * (0.18 * k)
        vp.tube(p, m, vp.bent_bar([q0, q1, q2, q3], R=0.3 * k, n=5), r, seg=8)
    return B + ex * ((0.1 + math.cos(math.radians(a1)) * 2.0 + math.cos(math.radians(a2)) * 1.8 + 0.12) * k)


# ---------------------------------------------------------------------------------- o anexo (−Y, frente) e a bandeira
def annex(p):
    zr = [Z0 + 1.6, Z0 + 2.0]
    band = (Z0 + 1.6, Z0 + 2.0)
    solid(p, AX0, AX1, AY0, AY1, AZ - 3.2, AZ)
    clad(p, 'x', AX1, 1, AY0, AY1, Z0, AZ, zrows=zr, band=band, holes=[(-9.6, -8.5, Z0, Z0 + 2.25), (-11.4, -10.2, Z0 + 0.7, Z0 + 1.36)])
    clad(p, 'y', AY0, -1, AX0, AX1, Z0, AZ, zrows=zr, band=band, holes=[(3.0, 4.6, Z0 + 0.7, Z0 + 1.36), (5.6, 7.2, Z0 + 0.7, Z0 + 1.36)])
    clad(p, 'x', AX0, -1, AY0, AY1, Z0, AZ, zrows=zr, band=band)
    for (x, y) in ((AX1, AY0), (AX0, AY0)):
        pilaster(p, x + 0.06 * (1 if x > AX0 else -1), y + 0.06 * (1 if y > AY0 else -1), AZ + 0.38, w=0.46)
    parapet(p, AX0, AX1, AY0, AY1, AZ - 0.1, AZ + 0.36, t=0.28)
    roof_deck(p, AX0 + 0.25, AX1 - 0.25, AY0 + 0.25, AY1 - 0.25, AZ, 3, 2)
    door(p, 'x', AX1, 1, -9.05)
    window(p, 'x', AX1, 1, -11.4, -10.2, Z0 + 0.7, Z0 + 1.36)
    window(p, 'y', AY0, -1, 3.0, 4.6, Z0 + 0.7, Z0 + 1.36)
    window(p, 'y', AY0, -1, 5.6, 7.2, Z0 + 0.7, Z0 + 1.36)
    ub.obox(p, 'Player#vp', (AX1 + 0.12, -9.05, Z0 + 2.95), (0.04, 1.4, 0.32), X, Y, Z)
    # o teto: a carga com a lona azul amarrada, a claraboia, o condensador, a antena
    ub.obox(p, 'Body#sm', (3.9, -9.7, AZ + 0.3), (2.4, 1.7, 0.6), X, Y, Z)
    ub.tarp_cover_at(p, (3.9, -9.7), 2.7, 2.0, 0.0, AZ + 0.06, 0.62, m='Player', seed=1.7, ties=3, box=14)
    bx(p, 'Body#sm', 5.0, 6.6, -10.4, -8.9, AZ - 0.05, AZ + 0.3)
    ub.obox(p, 'Glass#vp', (5.8, -9.65, AZ + 0.32), (1.4, 1.3, 0.04), X, Y, Z)
    fan_box(p, (7.4, -8.4), (1.2, 0.9, 0.75), fans=1, z0=AZ)
    ub.antenna(p, 7.6, -10.2, AZ, 2.4, r=0.05)
    # o mastro da bandeira no canto da frente: o pedestal, o poste torneado, a bola, o pano (o jogo desenha no vento)
    fx, fy = AX1 - 0.55, AY0 + 0.55
    bx(p, 'Body#pnl', fx - 0.4, fx + 0.4, fy - 0.4, fy + 0.4, AZ - 0.1, AZ + 0.55)
    vp.lathe(p, 'Trim#vp', (fx, fy, AZ + 0.55), 'z', [(0.0, 0.0), (0.2, 0.0), (0.2, 0.05), (0.11, 0.1), (0.09, 0.4), (0.055, 5.3), (0.0, 5.3)],
             seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, AZ + 5.85), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), AZ + 5.75, 2.4, 1.45, m='Player')


def rear_yard(p):
    """O canto de trás (−Y): o tanque de combustível deitado na bacia com a mureta e a linha até o galpão; a carga com a lona."""
    x0, x1, y0, y1 = -9.4, -3.4, -12.2, -8.6
    ub.slab(p, 'Concrete#vp', x0, x1, y0, y1, Z0 - 0.04, Z0 + 0.1)
    ub.h_tank(p, (-6.4, -10.4, 1.4), 'x', 0.9, 4.2, m='Body', band='Player', saddles=2, foot_z=Z0 + 0.1)
    ub.ladder(p, 'Trim', -6.4 + 0.6, -10.4 - 0.95, Z0 + 0.1, 2.3, (0, -1, 0), w=0.45, cage=False, wall=0.06, top_ext=0.5)
    pipe(p, 'Trim', [(-7.7, -9.55, 1.3), (-7.7, -7.75, 1.3), (-7.7, -7.25, 1.3)], 0.08, step=1.3)
    vp.lathe(p, 'Yellow#vp', (-7.7, -8.3, 1.3), 'y', [(0.0, -0.12), (0.14, -0.12), (0.14, 0.12), (0.0, 0.12)], seg=14)
    vp.tube(p, 'Yellow', [(-7.7, -8.3, 1.44), (-7.7, -8.3, 1.62)], 0.025, seg=6)
    p.torus('Yellow#vp', (-7.7, -8.3, 1.65), 'z', 0.16, 0.022, seg=16, rseg=6)
    paint(p, 'Hazard', x1 + 0.1, x1 + 0.5, y0 + 0.2, y1 - 0.2)
    # a carga paletizada com a lona azul (o objeto da cor do jogador)
    ub.obox(p, 'Crate#sm', (-1.6, -10.4, Z0 + 0.08), (2.6, 1.8, 0.16), X, Y, Z)
    ub.obox(p, 'Body#sm', (-1.6, -10.4, Z0 + 0.6), (2.3, 1.5, 0.9), X, Y, Z)
    ub.tarp_cover_at(p, (-1.6, -10.4), 2.6, 1.8, 0.0, Z0 + 0.2, 0.95, m='Player', seed=3.1, ties=3, box=14)


def lamps(p):
    ub.lamp_post(p, 10.3, 11.7, h=6.2, aim=(6.0, 9.0))
    ub.lamp_post(p, 10.3, -11.7, h=6.2, aim=(8.0, -2.0))
    ub.lamp_post(p, -10.5, -12.0, h=6.2, aim=(-6.0, -8.0))


def base():
    p = UPart(PREFIX + '_Base')
    ub.keys(p)
    ground(p)
    hall(p)
    bay(p)
    portal(p)
    roof(p)
    radome(p)
    chimneys(p)
    robot_yard(p)
    annex(p)
    rear_yard(p)
    lamps(p)
    vp.poke_ngons(p, 0.05)
    return ub.finish(p)


# ---------------------------------------------------------------------------------- o rotor (origem no eixo)
def fan_rotor():
    """O rotor do ventilador: o cubo torneado com o bico, as 10 pás largas torcidas (a raiz a 34°, a ponta a 14°) e enviesadas pra
    trás; clara sobre o poço escuro (de cima o disco de pás lê girando)."""
    p = UPart(PREFIX + '_Fan')
    ub.keys(p)
    vp.lathe(p, 'Trim#vp', (0.0, 0.0, -0.16), 'z', [(0.0, 0.0), (0.36, 0.0), (0.36, 0.2), (0.32, 0.26), (0.2, 0.31), (0.0, 0.33)], seg=28)
    vp.lathe(p, 'Dark#vp', (0.0, 0.0, 0.16), 'z', [(0.0, 0.0), (0.12, 0.0), (0.08, 0.06), (0.0, 0.08)], seg=14)
    vp.bolt_ring(p, 'Dark', (0.0, 0.0, 0.1), 'z', 0.24, 8, r=0.025, h=0.016)
    n = 10
    for k in range(n):
        a = 2 * math.pi * k / n
        rings = []
        for i, r in enumerate((0.3, 0.6, 0.95, 1.25, FAN_R - 0.08)):
            t = i / 4
            w = 0.24 + (0.42 - 0.24) * t
            tw = math.radians(34 - 20 * t)
            sweep = -0.18 * t * t
            aa = a + sweep / max(r, 0.3)
            d = Vector((math.cos(aa), math.sin(aa), 0.0))
            s = Vector((-math.sin(aa), math.cos(aa), 0.0))
            ch = s * math.cos(tw) + Vector((0, 0, 1)) * math.sin(tw)
            nn = d.cross(ch).normalized()
            c = d * r
            th = 0.028 * (1.0 - 0.4 * t)
            rings.append([tuple(c - ch * (w / 2) - nn * (th / 2)), tuple(c + ch * (w / 2) - nn * (th / 2)),
                          tuple(c + ch * (w / 2) + nn * (th / 2)), tuple(c - ch * (w / 2) + nn * (th / 2))])
        p.loft('Body#vp', rings, bevel=0.0)
    return ub.finish(p)


def scaffold_poly(name, pts, h, out=1.0, skip=(), post='Trim', plank='Crate', stripe='Hazard', bay=3.0, lift=2.0, z0=0.14):
    """O andaime da obra ao longo do contorno (o do Strategy Center B)."""
    p = Part(name)
    top = h + 1.0
    levels = max(2, int((top - z0) / lift))
    n = len(pts)
    cx, cy = sum(q[0] for q in pts) / n, sum(q[1] for q in pts) / n
    for i in range(n):
        if i in skip:
            continue
        (ax, ay), (bx_, by) = pts[i], pts[(i + 1) % n]
        L = math.hypot(bx_ - ax, by - ay)
        dx, dy = (bx_ - ax) / L, (by - ay) / L
        ox, oy = dy, -dx
        if ox * ((ax + bx_) / 2 - cx) + oy * ((ay + by) / 2 - cy) < 0:
            ox, oy = -ox, -oy
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
            p.obox(plank, (mx + ox * out / 2, my + oy * out / 2, z - 0.03), (L, abs(out) - 0.1, 0.06), yaw=yaw, bevel=0.01)
            p.obox(stripe, (mx + ox * out, my + oy * out, z + 0.09), (L, 0.04, 0.18), yaw=yaw, bevel=0.0)
        for k in range(m):
            t0, t1 = k / m, (k + 1) / m
            pa = Vector((ax + (bx_ - ax) * t0 + ox * out, ay + (by - ay) * t0 + oy * out, z0 + (top - z0) * ((k % levels) / levels)))
            pb = Vector((ax + (bx_ - ax) * t1 + ox * out, ay + (by - ay) * t1 + oy * out, z0 + (top - z0) * ((k % levels + 1) / levels)))
            d = pb - pa
            c = (pa + pb) / 2
            p.cyl(post, (c.x, c.y, c.z), (d.x, d.y, d.z), 0.035, d.length, seg=6, bevel=0.0)
    return p.finish()


FIRE_POINTS = [(-3.4, -2.2, RZ + 0.1), (2.2, 5.3, TZ + 0.1), (2.0, -5.3, TZ + 0.1)]


def write_spec():
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]] + [[PREFIX + '_Fan', [x, -y, z]] for (x, y, z) in FAN_AT],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1300, 1450]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1700, 700]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1700, 700]],
                      ['three_quarter', 'persp', 36.0, 155.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 34.0, 325.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [8.8, 0.0, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    f = bake_in_place(fan_rotor(), FAN_AT[0])
    total = export(b, OUT)
    total += export(f, OUT)
    pts = [(RX0 - 0.4, -TY - 0.4), (TX1 + 0.4, -TY - 0.4), (TX1 + 0.4, TY + 0.4), (RX0 - 0.4, TY + 0.4)]
    total += export(scaffold_poly(PREFIX + '_Scaffold', pts, TZ + 0.42, out=1.0, skip=(1,)), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {o.name: tris(o) for o in (b, f)})
    import bpy
    bpy.context.view_layer.update()
    pts = [b.matrix_world @ Vector(c) for c in b.bound_box]
    lo = (min(q.x for q in pts), min(-q.y for q in pts), min(q.z for q in pts))
    hi = (max(q.x for q in pts), max(-q.y for q in pts), max(q.z for q in pts))
    print('MODEL caixa da Base (Unreal) x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f; lote x ±%.2f y ±%.2f' % (lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                                                                                                         HX, HY))
    print('MODEL portão: vão y ±%.2f (%.2f m), altura livre %.2f m (ponte %.2f, moitão %.2f); tanque USA 3,7 × 3,0 m' % (
        BY, 2 * BY, BZ - Z0, GZ0 - Z0, 4.45 - Z0))
    print('MODEL spin fan at_m', [tuple(round(v, 3) for v in a) for a in FAN_AT], 'rpm 38 e 44')
    print('MODEL fire_points_m', FIRE_POINTS)
    print('MODEL NONMANIFOLD', NONMANIFOLD if NONMANIFOLD else 'nenhum')
    write_spec()


if not os.environ.get('MODEL_NO_MAIN'):           # (as contas de triângulos por seção importam o modelo sem rodar)
    main()
