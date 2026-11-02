"""O quartel da USA (usa_barracks), VERSÃO B no detalhe AAA (01/11/2026): o pedido do Felipe ("refazer as construções iniciais da USA,
ficaram todas quadradas, sem vida, sem qualidade [...] as novas que você fez todas ficaram muito boas"). A versão A é o salão de teto
reto do script antigo (Game/Tools/blender_provisional.py, barracks()); esta volta pro ARCO, na linguagem dos prédios da USA (a do
Strategy Center B: Game/Tools/model_strategy_center_v2.py; docs/ESTUDO_DETALHE_AAA.md).

Rodar (Blender sem janela, só CPU), pelo executor do detalhe (a grade, o UV e as máscaras na cor de vértice):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_usa_barracks_v2.py C:/Users/luisf/explosion/captures/game/models/usa_barracks_v2

A PALAVRA (docs/CONCEITO_CONSTRUCOES.md §3b; docs/elenco/usa/usa_barracks.md §2): O HANGAR ARREDONDADO. De cima, o meio-cilindro com as
NERVURAS em listras atravessadas (a chapa ondulada de verdade, 0,32 m de passo e 9 cm de fundo, e os arcos de perfil por fora a cada
1,6 m), o pátio de treino mais claro na frente cercado pela mureta irregular de SACOS DE AREIA, o ponto do MASTRO; a porta de onde a
infantaria sai, na frente (+X). Peça por peça:
  - o arco: a casca ondulada (as ondas seguem o arco, como no abrigo de campanha), os 9 arcos de perfil com a sapata, a faixa azul
    pintada na onda perto da frente (a cor do jogador no arco), a mureta de concreto do pé, o rufo escuro contornando as empenas;
  - as empenas de placas com a junta funda; na frente o vestíbulo de placas com a porta dupla, a marquise, os holofotes, a placa
    azul e a lona azul amarrada no teto dele, as janelas, a estrela branca no disco azul; atrás a porta, o mastro de rádio;
  - os lados: as águas-furtadas das janelas, a porta lateral com o alpendre, os condensadores com ventilador, a chaminé da estufa
    com o chapéu e os exaustores redondos na cumeeira;
  - o pátio: as lajes claras, a mureta de sacos em U (3 fiadas desencontradas, a saída de 7 m no meio, as pontas em curva), a mureta
    em L dos dois lados da porta, o mastro torneado no círculo de pedras pintadas (o pano no vento é do jogo: flags.json), a barra
    de exercício, a carga com a lona azul, os postes de luz com o pé de perigo.
A USA limpa (13/10): sem desgaste pintado (o desgaste vem do material do jogo); aqui só as juntas, as chapas e os detalhes.

As medidas do jogo (Units.json usa_barracks): o lote 23,2 × 18,88 m (X × Y); a porta e a saída em +X (a faixa de 6,6 m do SimCore sai
do centro do lote). Peças: SM_USA_Barracks_Base, SM_USA_Barracks_Scaffold. Slots: Body, Trim, Dark, Player, Concrete, Hazard, Glass,
Light, Beacon, Yellow, Paint, Crate (o andaime) e CivCloth (os sacos de areia: o slot cáqui que o quartel A já usava).
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
from model_china_vehicle_kit import tris  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'usa_barracks_v2')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_USA_Barracks'

COLORS.update({'Body': (190, 190, 184), 'Trim': (70, 70, 68), 'Dark': (40, 40, 38), 'Player': (86, 108, 150),
               'Concrete': (150, 146, 136), 'Hazard': (214, 170, 30), 'Glass': (30, 50, 80), 'Light': (255, 230, 170),
               'Beacon': (255, 170, 40), 'Yellow': (214, 164, 32), 'Paint': (222, 222, 216), 'Crate': (112, 100, 62),
               'CivCloth': (151, 128, 98)})

# ---------------------------------------------------------------------------------- as medidas
HX, HY = 11.6, 9.44              # o lote 23,2 × 18,88 (Units.json size_m)
Z0 = 0.14                        # o topo das lajes
QX0, QX1 = -10.6, 2.2            # o arco: de x QX0 (os fundos) a QX1 (a empena da frente), 12,8 m
QA, QB, KZ = 5.2, 4.35, 0.95     # a meia-largura (10,4 m de vão), a flecha e o topo da mureta: a cumeeira a 5,3 m
T0, CA, CP = 0.05, 0.045, 0.32   # a casca: a espessura média sobre o arco, a meia-altura da onda e o passo (a chapa ondulada)
EAVE = 0.15                      # a casca passa das empenas
NT = 44                          # os segmentos do arco (≤ 0,37 m: a grade do executor não corta)
RIBS = [QX0 + 0.4 + 1.6 * k for k in range(8)] + [QX1 - 0.12]   # os arcos de perfil por fora
BAND = (1.3, 1.9)                # a faixa azul pintada na onda (x)
VX1, VY, VZ = QX1 + 1.9, 2.0, 3.3   # o vestíbulo da porta: até x VX1, ±VY, o teto
YARD = (QX1, HX - 0.2, -HY + 0.25, HY - 0.25)        # o pátio de treino (as lajes)
BAGS_X, BAGS_Y, GAP_Y = HX - 0.7, HY - 0.6, 3.5        # a mureta de sacos: a frente em x, os lados em ±y, a saída ±GAP_Y
FLAG_AT = (7.6, -4.4)
SANDBAG = (0.68, 0.42, 0.24)     # o saco (exagerado ~1,1×: a 3–5 cm/px o de 0,6 m vira risco)
DORMERS = [-7.8, -4.6, -1.4]

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
J = 0.035
NONMANIFOLD = []


class UPart(Part):
    """O Part com o aviso do model_russia_parts.RuPart (NONMANIFOLD: peça encostando em peça do mesmo balde chanfrado)."""

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


# ---------------------------------------------------------------------------------- ajudantes (os mesmos da War Factory B)
def bx(p, m, x0, x1, y0, y1, z0, z1):
    x0, x1 = min(x0, x1), max(x0, x1)
    y0, y1 = min(y0, y1), max(y0, y1)
    ub.obox(p, m, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), X, Y, Z)


def axes(axis, s):
    return (Vector(Y), Vector((s, 0, 0))) if axis == 'x' else (Vector(X), Vector((0, s, 0)))


def wpt(axis, at, s, u, z, n=0.0):
    return Vector((at + s * n, u, z)) if axis == 'x' else Vector((u, at + s * n, z))


def panel_wall(p, origin, eu, ev, en, w, h, pw=2.0, ph=None, m='Body#pnl', gap=0.06, proud=0.07, embed=0.05, holes=(),
               min_piece=0.3, rows=None, row_mats=None):
    """O ub.panel_wall com a junta também entre os pedaços cortados por um vão (no kit eles encostavam e soldavam)."""
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
    """A placa: a caixa SEM a face de trás (enterrada no miolo; era ~1/3 dos triângulos da parede depois da grade)."""
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


def lamp_head(p, c, aim, w=0.5, h=0.32, body='Dark'):
    """O holofote sem o chanfro das caixas (o do kit custava ~500 triângulos por luminária depois da grade)."""
    d = Vector(aim).normalized()
    ey = Vector((0, 0, 1)).cross(d).normalized()
    ez = d.cross(ey).normalized()
    C = Vector(c)
    vp.prism(p, body + '#vp', vp.rounded_rect(w, h, 0.04, 2), C - d * 0.11, ey, ez, d, 0.22)
    ub.obox(p, 'Light#vp', C + d * 0.115, (0.012, w - 0.08, h - 0.08), d, ey, ez)
    ub.obox(p, body + '#vp', C + d * 0.12 + ez * (h / 2 + 0.01), (0.16, w + 0.02, 0.02), d, ey, ez)


def small_fan(p, c, R, z_top, depth=0.42, blades=7):
    """O ventilador dos condensadores (o ub.fan mais leve: as pás sem chanfro, a grade com o anel e 2 barras)."""
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


def pipe(p, m, pts, r, step=2.4):
    return ub.pipe(p, m, pts, r, step=step, seg=10)


def louvers(p, c, n_face, along, L, H, n, frame='Trim', slat='Trim', back='Dark', tilt=35.0):
    """O ub.louvers com os montantes entrando na travessa (no kit as 4 peças da moldura soldavam)."""
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


def fan_box(p, c, size, m='Body', z0=0.0, R=None):
    """O condensador com 1 ventilador (o ub.fan_box com as venezianas daqui)."""
    x, y = c
    sx, sy, sz = size
    t = 0.46
    ub.obox(p, m + '#pnl', (x, y, z0 + 0.08 + (sz - t + 0.02 - 0.08) / 2), (sx - 0.08, sy - 0.08, sz - t + 0.02 - 0.08), X, Y, Z)
    for sxx in (-1, 1):
        for syy in (-1, 1):
            ub.obox(p, 'Trim#vp', (x + sxx * (sx / 2 - 0.2), y + syy * (sy / 2 - 0.2), z0 + 0.05), (0.3, 0.3, 0.1), X, Y, Z)
    R = R or min(sy * 0.38, sx / 2 * 0.78)
    ub.square_annulus(p, m + '#vp', (x, y), sx / 2, sy / 2, R, z0 + sz, t)
    small_fan(p, (x, y), R, z0 + sz, depth=t - 0.04)
    for sg in (-1, 1):
        hb = sz - t - 0.08
        louvers(p, (x + sg * (sx / 2 - 0.04), y, z0 + 0.08 + hb / 2), (sg, 0, 0), (0, 1, 0), sy * 0.7, hb * 0.6, max(3, int(hb * 0.6 / 0.09)))


def clad(p, axis, at, s, a0, a1, z0, z1, holes=(), pw=1.6, zrows=(), m='Body#pnl', mats=None, gap=0.07):
    """As placas de uma parede (holes e zrows em absoluto; mats = {z de baixo da fileira: material})."""
    eu, en = axes(axis, s)
    rows = [zr - z0 for zr in zrows if z0 + 0.05 < zr < z1 - 0.05]
    vs = [0.0] + rows + [z1 - z0]
    rm = {}
    for j in range(len(vs) - 1):
        for zb, mm in (mats or {}).items():
            if abs(vs[j] - (zb - z0)) < 1e-6:
                rm[j] = mm
    hs = [(u0 - a0, u1 - a0, zb - z0, zt - z0) for (u0, u1, zb, zt) in holes]
    panel_wall(p, wpt(axis, at, s, a0, z0), eu, Z, en, a1 - a0, z1 - z0, pw=pw, m=m, gap=gap, proud=0.08, embed=0.04, holes=hs,
               rows=rows, row_mats=rm, min_piece=0.28)


def parapet(p, x0, x1, y0, y1, z0, z1, t=0.3, skip=()):
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


def door(p, axis, at, s, uc, w=1.1, h=2.25, canopy=True, lamp=True, zb=Z0, leaves=1):
    """A porta: o vão escuro, o batente, a(s) folha(s) com o visor e a alça, a marquise e o holofote."""
    Z0_ = zb
    eu, en = axes(axis, s)
    ub.obox(p, 'Dark#vp', wpt(axis, at, s, uc, Z0_ + h / 2, 0.0), (w, h, 0.06), eu, Z, en)
    for su in (-1, 1):
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc + su * (w / 2 + 0.05), Z0_ + h / 2, 0.08), (0.1, h + 0.1, 0.1), eu, Z, en)
    ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0_ + h + 0.05, 0.08), (w + 0.2 - 0.004, 0.1, 0.1), eu, Z, en)
    lw = (w - 0.1) / leaves
    for k in range(leaves):
        u = uc - (w - 0.1) / 2 + lw * (k + 0.5)
        ub.obox(p, 'Body#sm', wpt(axis, at, s, u, Z0_ + h / 2 - 0.02, 0.05), (lw - 0.04, h - 0.08, 0.05), eu, Z, en)
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, u, Z0_ + h * 0.72, 0.08), (lw - 0.3, 0.3, 0.02), eu, Z, en)
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, u, Z0_ + 0.3, 0.08), (lw - 0.16, 0.2, 0.02), eu, Z, en)
        hu = u + (lw / 2 - 0.16) * (1 if (leaves == 1 or k == 0) else -1)
        a = wpt(axis, at, s, hu, Z0_ + 0.95, 0.08)
        vp.grab_handle(p, 'Trim', a, a + Vector((0, 0, 0.28)), en, height=0.05, r=0.014)
    if canopy:
        ub.obox(p, 'Trim#vp', wpt(axis, at, s, uc, Z0_ + h + 0.32, 0.45), (w + 0.8, 0.9, 0.08), eu, en, Z)
        for su in (-1, 1):
            q0 = wpt(axis, at, s, uc + su * (w / 2 + 0.3), Z0_ + h + 0.0, 0.05)
            vp.tube(p, 'Trim', [q0, wpt(axis, at, s, uc + su * (w / 2 + 0.3), Z0_ + h + 0.3, 0.82)], 0.02, seg=6)
    if lamp:
        for su in ((-1, 1) if leaves > 1 else (0,)):
            lamp_head(p, wpt(axis, at, s, uc + su * (w / 2 - 0.2), Z0_ + h + (0.16 if canopy else 0.3), 0.2 if canopy else 0.14),
                         en - Vector((0, 0, 1.2)), w=0.36, h=0.18)


def window(p, axis, at, s, u0, u1, z0, z1, mull=1):
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


# ---------------------------------------------------------------------------------- o arco
def arch_pt(th, off):
    """O ponto do arco (a semi-elipse de meia-largura QA e flecha QB sobre a mureta) no ângulo th (0 = o pé do lado +Y, π = o do −Y),
    deslocado 'off' pela normal: (y, z)."""
    c, s = math.cos(th), math.sin(th)
    ny, nz = QB * c, QA * s
    L = math.hypot(ny, nz)
    return QA * c + ny / L * off, KZ + QB * s + nz / L * off


def arch_z(y, off=0.0):
    """A altura do arco no y (a casca de dentro + off, aproximado pela vertical)."""
    t = min(1.0, abs(y) / QA)
    return KZ + QB * math.sqrt(max(0.0, 1.0 - t * t)) + off


def wave(x):
    """A onda trapezoidal da chapa (o passo CP): a crista, o flanco, o vale, o flanco."""
    f = ((x - QX0) / CP) % 1.0
    if f < 0.3:
        return CA
    if f < 0.5:
        return CA - 2 * CA * (f - 0.3) / 0.2
    if f < 0.8:
        return -CA
    return -CA + 2 * CA * (f - 0.8) / 0.2


def skin(p):
    """A casca ondulada fechada (a de fora com a onda, a de dentro lisa, as bordas): as ondas seguem o arco (as listras atravessadas
    que leem de cima). Cada faixa de x vai pro material dela (a faixa azul pintada na onda); o vinco das ondas fica duro, o arco liso."""
    lo, hi = QX0 - EAVE, QX1 + EAVE
    xs = {lo, hi}
    for n in range(-2, int((hi - QX0) / CP) + 3):
        for f in (0.0, 0.3, 0.5, 0.8):
            xx = QX0 + (n + f) * CP
            if lo + 0.02 < xx < hi - 0.02:
                xs.add(round(xx, 5))
    xs = sorted(xs)
    ths = [math.pi * i / NT for i in range(NT + 1)]
    for i in range(len(xs) - 1):
        xa, xb = xs[i], xs[i + 1]
        m = 'Player#roof' if BAND[0] <= (xa + xb) / 2 <= BAND[1] else 'Body#roof'
        b = p.bm(m, 0.0)
        ring = {}
        for xx in (xa, xb):
            o = T0 + wave(xx)
            ring[xx] = ([b.verts.new(V(xx, *arch_pt(th, o))) for th in ths], [b.verts.new(V(xx, *arch_pt(th, -0.03))) for th in ths])
        (oa, ia), (ob, ib) = ring[xa], ring[xb]
        for j in range(NT):
            f = b.faces.new([oa[j], ob[j], ob[j + 1], oa[j + 1]])
            f.smooth = True
            f = b.faces.new([ia[j + 1], ib[j + 1], ib[j], ia[j]])
            f.smooth = True
        for (o_, i_) in ((0, 0), (NT, NT)):
            b.faces.new([oa[o_], ia[i_], ib[i_], ob[o_]])
        for (o_, i_, xx) in ((oa, ia, xa), (ob, ib, xb)):
            for e in [b.edges.get([o_[j], o_[j + 1]]) for j in range(NT)] + [b.edges.get([i_[j], i_[j + 1]]) for j in range(NT)]:
                if e is not None:
                    e.smooth = False
        if i == 0:
            for j in range(NT):
                b.faces.new([oa[j + 1], ia[j + 1], ia[j], oa[j]])
        if i == len(xs) - 2:
            for j in range(NT):
                b.faces.new([ob[j], ib[j], ib[j + 1], ob[j + 1]])


def arch_band(p, m, x0, x1, off0, off1, th0=0.0, th1=math.pi, n=NT):
    """A tira que segue o arco (o arco de perfil, o rufo da empena): a seção retangular x0..x1 × off0..off1, lisa ao longo do arco e
    com os 4 vincos duros."""
    b = p.bm(m, 0.0)
    rings = []
    for i in range(n + 1):
        th = th0 + (th1 - th0) * i / n
        ya, za = arch_pt(th, off0)
        yb, zb = arch_pt(th, off1)
        rings.append([b.verts.new(V(x0, ya, za)), b.verts.new(V(x1, ya, za)), b.verts.new(V(x1, yb, zb)), b.verts.new(V(x0, yb, zb))])
    for i in range(n):
        for k in range(4):
            f = b.faces.new([rings[i][k], rings[i][(k + 1) % 4], rings[i + 1][(k + 1) % 4], rings[i + 1][k]])
            f.smooth = True
    b.faces.new(rings[0][::-1])
    b.faces.new(rings[-1])
    for k in range(4):
        for i in range(n):
            e = b.edges.get([rings[i][k], rings[i + 1][k]])
            if e is not None:
                e.smooth = False
    for r in (rings[0], rings[-1]):
        for k in range(4):
            e = b.edges.get([r[k], r[(k + 1) % 4]])
            if e is not None:
                e.smooth = False


def hut(p):
    skin(p)
    # os arcos de perfil por fora (a nervura grossa), com a sapata na mureta e as placas de emenda no meio
    for k, x in enumerate(RIBS):
        w = 0.2 if 0 < k < len(RIBS) - 1 else 0.16
        arch_band(p, 'Trim#vp', x - w / 2, x + w / 2, T0 - 0.02, T0 + CA + 0.12)
        for s in (-1, 1):
            bx(p, 'Trim#vp', x - 0.2, x + 0.2, s * (QA - 0.05), s * (QA + 0.32), KZ - 0.02, KZ + 0.1)
    # a mureta de concreto do pé (4 trechos de 3,2 m com a junta)
    for s in (-1, 1):
        for i in range(4):
            xa = QX0 + 3.2 * i + (0.02 if i else 0.0)
            xb = QX0 + 3.2 * (i + 1) - (0.02 if i < 3 else 0.0)
            bx(p, 'Concrete#pnl', xa, xb, s * (QA - 0.15), s * (QA + 0.22), -0.1, KZ)
    # as empenas (o miolo e as placas com a junta funda, que sobem até o arco), o rufo escuro contornando o arco
    for (xg, s) in ((QX1, 1), (QX0, -1)):
        pts = [(QA, Z0 - 0.05)] + [arch_pt(math.pi * i / NT, -0.01) for i in range(NT + 1)] + [(-QA, Z0 - 0.05)]
        vp.prism(p, 'Body#vp', pts, Vector((xg - s * 0.14, 0.0, 0.0)), Vector(Y), Vector(Z), Vector((s, 0, 0)), 0.14)
        holes = []
        if s > 0:
            holes = [(-VY - 0.05, VY + 0.05, Z0, VZ + 0.1), (-4.25, -2.95, Z0 + 1.45, Z0 + 2.4), (2.95, 4.25, Z0 + 1.45, Z0 + 2.4)]
        else:
            holes = [(1.0, 2.2, Z0, Z0 + 2.35), (-3.6, -2.4, Z0 + 1.45, Z0 + 2.4)]
        gable_plates(p, xg, s, holes)
        arch_band(p, 'Trim#vp', xg - 0.03 if s > 0 else xg - 0.11, xg + 0.11 if s > 0 else xg + 0.03, -0.3, 0.02)
    # a faixa azul pintada nas sapatas (a faixa do arco termina na mureta)
    for s in (-1, 1):
        bx(p, 'Player#vp', BAND[0], BAND[1], s * (QA + 0.2), s * (QA + 0.235), KZ - 0.35, KZ - 0.05)


def gable_plates(p, xg, s, holes):
    """As placas da empena em colunas de ~0,87 m, cada uma subindo até o arco (o rufo cobre o degrau)."""
    n = 12
    cw = 2 * QA / n
    for i in range(n):
        u0, u1 = -QA + i * cw + J, -QA + (i + 1) * cw - J
        top = arch_z(max(abs(u0), abs(u1))) - 0.06
        if top - Z0 < 0.4:
            continue
        hs = [(a - u0, b - u0, c - Z0, d - Z0) for (a, b, c, d) in holes]
        rows = [r for r in (1.25, 2.5, 3.75) if r < top - Z0 - 0.3]
        panel_wall(p, (xg, u0, Z0), Y, Z, (s, 0, 0), u1 - u0, top - Z0, pw=10.0, m='Body#pnl', gap=0.07, proud=0.08, embed=0.04,
                   holes=hs, rows=rows, min_piece=0.28)


def front(p):
    """A frente: o vestíbulo de placas com a porta dupla, as janelas da empena, a estrela branca no disco azul."""
    x0, x1 = QX1 - 0.1, VX1
    bx(p, 'Body#vp', x0, x1, -VY, VY, Z0 - 0.05, VZ - 0.1)
    zr = [Z0 + 1.25, Z0 + 2.5]
    clad(p, 'x', x1, 1, -VY, VY, Z0, VZ, holes=[(-1.3, 1.3, Z0, Z0 + 2.6)], pw=1.0, zrows=zr)
    clad(p, 'y', VY, 1, QX1 + 0.12, x1, Z0, VZ, pw=0.9, zrows=zr)
    clad(p, 'y', -VY, -1, QX1 + 0.12, x1, Z0, VZ, pw=0.9, zrows=zr)
    for y in (-VY, VY):
        bx(p, 'Body#pnl', x1 - 0.24 + 0.06, x1 + 0.3, y - 0.24 + 0.06 * (1 if y > 0 else -1), y + 0.24 + 0.06 * (1 if y > 0 else -1),
           Z0 - 0.02, VZ + 0.36)
    parapet(p, x0 + 0.05, x1, -VY, VY, VZ - 0.1, VZ + 0.36, t=0.26, skip=('-x',))
    ub.slab(p, 'Concrete#pnl', x0 + 0.1, x1 - 0.22, -VY + 0.22, VY - 0.22, VZ - 0.14, VZ)
    door(p, 'x', x1, 1, 0.0, w=2.4, h=2.6, leaves=2)
    ub.obox(p, 'Player#vp', (x1 + 0.13, 0.0, VZ + 0.13), (0.04, 2.6, 0.3), X, Y, Z)
    # a lona azul amarrada no teto do vestíbulo (a carga) e o exaustor
    ub.obox(p, 'Body#sm', (QX1 + 0.95, -0.6, VZ + 0.28), (1.2, 2.0, 0.56), X, Y, Z)
    ub.tarp_cover_at(p, (QX1 + 0.95, -0.6), 2.3, 1.4, 90.0, VZ + 0.04, 0.6, m='Player', seed=0.7, ties=3, box=14)
    ub.turbine_vent(p, QX1 + 1.0, 1.3, VZ, r=0.26)
    # a soleira e os degraus
    ub.slab(p, 'Concrete#pnl', x1 + 0.05, x1 + 1.0, -1.8, 1.8, Z0 - 0.04, Z0 + 0.12)
    # as janelas da empena e os holofotes das quinas
    for s in (-1, 1):
        window(p, 'x', QX1, 1, s * 3.6 - 0.62, s * 3.6 + 0.62, Z0 + 1.48, Z0 + 2.38)
    # a estrela branca de 5 pontas no disco azul (a marca da USA), na empena acima do vestíbulo
    c = Vector((QX1 + 0.1, 0.0, 4.25))
    vp.lathe(p, 'Player#vp', c, 'x', [(0.0, 0.0), (0.58, 0.0), (0.62, 0.03), (0.62, 0.07), (0.0, 0.07)], seg=40)
    star = []
    for k in range(10):
        a = math.radians(90 + 36 * k)
        r = 0.5 if k % 2 == 0 else 0.2
        star.append((r * math.cos(a), r * math.sin(a)))
    vp.prism(p, 'Paint#vp', star, c + Vector((0.07, 0.0, 0.0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)), 0.02)
    p.torus('Paint#vp', (c.x + 0.075, 0.0, c.z), 'x', 0.545, 0.018, seg=40, rseg=5)


def sides(p):
    """Os lados do arco: as águas-furtadas das janelas, a porta lateral com o alpendre, os condensadores, a estufa, os exaustores."""
    for x in DORMERS:
        for s in (-1, 1):
            dormer(p, x, s)
    # a porta lateral (+Y, nos fundos) com o alpendre de placas
    xa, xb, y0, y1, h = -10.2, -8.5, QA - 0.7, QA + 1.05, 2.9
    bx(p, 'Body#vp', xa, xb, y0, y1, Z0 - 0.05, h - 0.08)
    clad(p, 'y', y1, 1, xa, xb, Z0, h, holes=[(-9.9, -8.8, Z0, Z0 + 2.25)], pw=1.0, zrows=[Z0 + 1.25])
    clad(p, 'x', xa, -1, QA + 0.28, y1, Z0, h, pw=0.8, zrows=[Z0 + 1.25])
    clad(p, 'x', xb, 1, QA + 0.28, y1, Z0, h, pw=0.8, zrows=[Z0 + 1.25])
    bx(p, 'Body#pnl', xa - 0.12, xb + 0.12, y0, y1 + 0.14, h - 0.12, h + 0.06)
    bx(p, 'Trim#vp', xa - 0.16, xb + 0.16, y1 + 0.04, y1 + 0.2, h - 0.02, h + 0.1)
    door(p, 'y', y1, 1, -9.35, w=1.1, canopy=False)
    ub.slab(p, 'Concrete#pnl', xa - 0.1, xb + 0.1, y1 + 0.05, y1 + 0.9, Z0 - 0.04, Z0 + 0.1)
    # os condensadores no pé do lado −Y, com o cano até a casca
    for x in (-7.8, -4.6):
        fan_box(p, (x, -QA - 0.9), (1.4, 0.95, 1.0), z0=Z0)
        pipe(p, 'Trim', [(x + 0.45, -QA - 0.45, Z0 + 0.6), (x + 0.45, -QA - 0.1, Z0 + 0.6), (x + 0.45, -QA - 0.1, 1.45),
                            (x + 0.45, -QA + 0.2, 1.45)], 0.05, step=1.2)
    # a chaminé da estufa (o cano do aquecedor saindo da casca), com a gola, as braçadeiras e o chapéu
    y = 2.9
    zt = arch_z(y)
    vp.lathe(p, 'Trim#vp', (-6.2, y, zt - 0.3), 'z', [(0.0, 0.0), (0.3, 0.0), (0.3, 0.12), (0.2, 0.3), (0.16, 0.42), (0.0, 0.42)], seg=20)
    vp.lathe(p, 'Dark#vp', (-6.2, y, zt), 'z', [(0.0, 0.0), (0.12, 0.0), (0.12, 1.7), (0.0, 1.7)], seg=16)
    for z in (0.6, 1.2):
        vp.lathe(p, 'Trim#vp', (-6.2, y, zt + z), 'z', [(0.115, -0.04), (0.15, -0.04), (0.15, 0.04), (0.115, 0.04)], seg=16, closed=True)
    vp.lathe(p, 'Dark#vp', (-6.2, y, zt + 1.75), 'z', [(0.0, 0.0), (0.06, 0.0), (0.34, 0.08), (0.34, 0.1), (0.0, 0.22)], seg=20)
    for k in range(3):
        a = math.radians(30 + 120 * k)
        vp.tube(p, 'Dark', [(-6.2 + 0.11 * math.cos(a), y + 0.11 * math.sin(a), zt + 1.68),
                            (-6.2 + 0.3 * math.cos(a), y + 0.3 * math.sin(a), zt + 1.82)], 0.015, seg=5)
    # os exaustores redondos na cumeeira (entre os arcos)
    for x in (-7.8, -4.6, -1.4):
        ub.turbine_vent(p, x, 0.0, arch_z(0.0) + T0 - 0.04, r=0.36)


def dormer(p, x, s):
    """A água-furtada da janela no flanco do arco: a caixa de placas que sai da casca, o vidro recuado com a moldura e o montante, a
    aba de cima."""
    za, zb = 1.4, 2.6
    yi, yo = QA - 0.6, QA + 0.28
    bx(p, 'Body#pnl', x - 0.62, x + 0.62, s * yi, s * yo, za - 0.1, zb + 0.05)
    window(p, 'y', s * yo, s, x - 0.42, x + 0.42, za + 0.15, zb - 0.2)
    bx(p, 'Body#sm', x - 0.7, x + 0.7, s * (yi + 0.1), s * (yo + 0.16), zb + 0.05, zb + 0.13)
    bx(p, 'Trim#vp', x - 0.7, x + 0.7, s * (yo + 0.1), s * (yo + 0.2), zb - 0.02, zb + 0.08)


def rear(p):
    """Os fundos: a porta com a marquise, a janela, o mastro de rádio estaiado na empena, o condensador."""
    door(p, 'x', QX0, -1, 1.6, w=1.1)
    window(p, 'x', QX0, -1, -3.6, -2.4, Z0 + 1.48, Z0 + 2.38)
    ub.slab(p, 'Concrete#pnl', QX0 - 0.9, QX0 - 0.12, 0.9, 2.3, Z0 - 0.04, Z0 + 0.1)
    xm, ym = QX0 - 0.35, -1.0
    vp.lathe(p, 'Trim#vp', (xm, ym, Z0), 'z', [(0.0, 0.0), (0.16, 0.0), (0.16, 0.05), (0.09, 0.1), (0.07, 0.3), (0.05, 7.4), (0.0, 7.4)], seg=12)
    for z in (2.5, 4.6):
        vp.tube(p, 'Trim', [(xm, ym, z), (QX0 - 0.03, ym, z)], 0.035, seg=6)
        bx(p, 'Trim#vp', QX0 - 0.1, QX0 - 0.02, ym - 0.12, ym + 0.12, z - 0.12, z + 0.12)
    ub.antenna(p, xm, ym, Z0 + 7.4, 1.6, r=0.05)
    for a in (-0.7, 0.7):
        vp.tube(p, 'Dark', [(xm - 0.18, ym + a * 0.3, Z0 + 5.2), (xm - 0.6, ym + a * 1.2, Z0 + 5.6)], 0.03, seg=6)
    ub.beacon(p, (xm, ym, Z0 + 7.42), r=0.07, h=0.14, cage=False)
    lamp_head(p, (QX0 - 0.2, -3.0, Z0 + 3.0), (-1.0, 0.0, -1.0), w=0.4, h=0.2)


# ---------------------------------------------------------------------------------- o chão, o pátio, os sacos
def ground(p):
    """As lajes: a calçada em volta do arco e o pátio de treino na frente (as placas de ~3 m com a junta funda)."""
    for s in (-1, 1):
        for i in range(4):
            xa = QX0 - 0.6 + 3.35 * i + (0.03 if i else 0.0)
            xb = QX0 - 0.6 + 3.35 * (i + 1) - (0.03 if i < 3 else 0.0)
            ub.slab(p, 'Concrete#pnl', xa, xb, s * (QA + 0.2), s * (QA + 1.1), -0.12, Z0)
    ub.slab(p, 'Concrete#pnl', QX0 - 0.95, QX0 - 0.1, -QA + 0.1, QA - 0.1, -0.12, Z0)
    x0, x1, y0, y1 = YARD
    nx, ny = 3, 6
    for i in range(nx):
        for j in range(ny):
            xa, xb = x0 + (x1 - x0) * i / nx + 0.03, x0 + (x1 - x0) * (i + 1) / nx - 0.03
            ya, yb = y0 + (y1 - y0) * j / ny + 0.03, y0 + (y1 - y0) * (j + 1) / ny - 0.03
            ub.slab(p, 'Concrete#pnl', xa, xb, ya, yb, -0.12, Z0)
    # a faixa amarela da saída (a linha de formatura) e as marcas de posição
    paint(p, 'Yellow', VX1 + 1.2, VX1 + 1.36, -2.6, 2.6)
    for k in range(5):
        y = -2.0 + k
        paint(p, 'Paint', VX1 + 2.4, VX1 + 2.64, y - 0.12, y + 0.12)


def bag(p, c, size, yaw=0.0, seed=0.0):
    """O saco de areia (o travesseiro cheio de pontas gordas, o fundo achatado), 7 seções × 8 lados."""
    L, W, H = size
    h = (math.sin(c[0] * 12.9898 + c[1] * 78.233 + c[2] * 37.719 + seed) * 43758.5453) % 1.0
    L *= 0.94 + 0.12 * h
    H *= 0.9 + 0.2 * ((h * 7.31) % 1.0)
    ex, ey, ez = vp.ypr_frame(yaw, (h - 0.5) * 6.0, ((h * 3.7) % 1.0 - 0.5) * 6.0)
    C = Vector(c)
    rings = []
    for i in range(7):
        u = i / 6
        bulge = math.sin(math.pi * u) ** 0.18
        w = W / 2 * (0.72 + 0.28 * bulge)
        t = H / 2 * (0.3 + 0.7 * math.sin(math.pi * u) ** 0.6)
        x = L * (u - 0.5)
        ring = []
        for k in range(8):
            th = 2 * math.pi * k / 8
            ct, st = math.cos(th), math.sin(th)
            yy = w * math.copysign(abs(ct) ** 0.7, ct)
            zz = t * math.copysign(abs(st) ** 0.7, st)
            if zz < 0:
                zz *= 0.55
            ring.append(tuple(C + ex * x + ey * yy + ez * (zz + H * 0.32)))
        rings.append(ring)
    p.loft('CivCloth#bag', rings, bevel=0.0, smooth=True)


def bag_wall(p, pts, layers=3, size=(0.62, 0.36, 0.22), seed=0, z0=Z0):
    """A mureta de sacos ao longo da polilinha: as fiadas desencontradas, cada fiada um pouco pra dentro."""
    for lay in range(layers):
        for i, (a, b_) in enumerate(zip(pts[:-1], pts[1:])):
            A, B = Vector((a[0], a[1], 0.0)), Vector((b_[0], b_[1], 0.0))
            L = (B - A).length
            n = max(1, int(math.ceil(L / (size[0] * 0.88))))
            yaw = math.degrees(math.atan2(B.y - A.y, B.x - A.x))
            off = 0.5 if lay % 2 else 0.0
            for k in range(n + (1 if lay % 2 else 0)):
                u = (k + 0.5 - off) / n
                if u < 0.02 or u > 0.98:
                    continue
                q = A + (B - A) * u
                bag(p, (q.x, q.y, z0 + lay * size[2] * 0.8), (size[0], size[1] - lay * 0.04, size[2]), yaw=yaw,
                    seed=seed * 100 + lay * 10 + k + i * 1000)


def offset_poly(pts, d, toward=(7.0, 0.0)):
    """A polilinha deslocada 'd' pro lado de 'toward' (a fileira de trás da mureta)."""
    out = []
    ns = []
    for (a, b_) in zip(pts[:-1], pts[1:]):
        t = Vector((b_[0] - a[0], b_[1] - a[1], 0.0)).normalized()
        n = Vector((-t.y, t.x, 0.0))
        mid = Vector(((a[0] + b_[0]) / 2, (a[1] + b_[1]) / 2, 0.0))
        if n.dot(Vector((toward[0], toward[1], 0.0)) - mid) < 0:
            n = -n
        ns.append(n)
    for i, q in enumerate(pts):
        n = (ns[max(0, i - 1)] + ns[min(len(ns) - 1, i)]).normalized()
        out.append((q[0] + n.x * d, q[1] + n.y * d))
    return out


def arc_pts(c, r, a0, a1, n):
    return [(c[0] + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), c[1] + r * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
            for k in range(n + 1)]


def yard(p):
    """O pátio: a mureta de sacos em U (as pontas da frente em curva, a saída no meio), a mureta em L dos 2 lados da porta, o mastro
    no círculo de pedras, a barra de exercício, a carga com a lona azul, os postes."""
    xb, yb, r = BAGS_X, BAGS_Y, 0.9
    for s in (-1, 1):
        side = [(QX1 + 0.6, s * (QA + 1.3)), (QX1 + 1.8, s * yb), (xb - r, s * yb)]
        side += [(q[0], q[1]) for q in arc_pts((xb - r, s * (yb - r)), r, 90 * s, 0, 3)][1:]
        side += [(xb, s * GAP_Y)]
        bag_wall(p, side, layers=4, size=SANDBAG, seed=1 + (s > 0))
        bag_wall(p, offset_poly(side, 0.4), layers=3, size=SANDBAG, seed=3 + (s > 0))
        # a mureta em L protegendo a porta
        bag_wall(p, [(VX1 + 0.35, s * 1.6), (VX1 + 1.35, s * 1.6), (VX1 + 1.35, s * 2.75)], layers=3, size=SANDBAG, seed=5 + (s > 0))
    # o mastro: o círculo de concreto com a borda de pedras pintadas, o poste torneado, a bola, o pano (o jogo desenha)
    fx, fy = FLAG_AT
    vp.lathe(p, 'Concrete#vp', (fx, fy, Z0 - 0.02), 'z', [(0.0, 0.0), (1.05, 0.0), (1.05, 0.1), (1.0, 0.14), (0.0, 0.14)], seg=36)
    for k in range(14):
        a = 2 * math.pi * k / 14
        vp.rbox(p, 'Paint', (fx + 1.15 * math.cos(a), fy + 1.15 * math.sin(a), Z0 + 0.07), (0.3, 0.2, 0.15), yaw=math.degrees(a) + 90,
                r=0.06, k=2)
    vp.lathe(p, 'Trim#vp', (fx, fy, Z0 + 0.12), 'z', [(0.0, 0.0), (0.22, 0.0), (0.22, 0.05), (0.12, 0.1), (0.1, 0.4), (0.06, 8.4), (0.0, 8.4)],
             seg=12)
    vp.lathe(p, 'Paint#vp', (fx, fy, Z0 + 8.5), 'z', [(0.0, 0.0), (0.06, 0.0), (0.12, 0.08), (0.12, 0.12), (0.06, 0.2), (0.0, 0.2)], seg=12)
    flag(p, (fx, fy), Z0 + 8.4, 2.4, 1.45, m='Player')
    # a barra de exercício (+Y): 3 postes, 2 barras em alturas diferentes
    bxp = 7.2
    for (y, h) in ((3.0, 2.5), (4.4, 2.5), (5.8, 2.1)):
        vp.lathe(p, 'Concrete#vp', (bxp, y, Z0), 'z', [(0.0, 0.0), (0.24, 0.0), (0.24, 0.14), (0.0, 0.14)], seg=14)
        vp.tube(p, 'Trim', [(bxp, y, Z0 + 0.1), (bxp, y, Z0 + h + 0.06)], 0.085, seg=12)
    vp.tube(p, 'Yellow', [(bxp, 3.0, Z0 + 2.4), (bxp, 4.4, Z0 + 2.4)], 0.05, seg=10)
    vp.tube(p, 'Yellow', [(bxp, 4.4, Z0 + 2.0), (bxp, 5.8, Z0 + 2.0)], 0.05, seg=10)
    # o muro de escalada (as tábuas de pé, as 2 escoras atrás, a plataforma de descida)
    wx, wy0, wy1, wh = 5.3, -6.3, -3.7, 2.1
    for k in range(9):
        y = wy0 + (wy1 - wy0) * (k + 0.5) / 9
        ub.obox(p, 'Crate#vp', (wx, y, Z0 + wh / 2), (0.12, (wy1 - wy0) / 9 - 0.03, wh), X, Y, Z)
    for y in (wy0 + 0.2, wy1 - 0.2):
        vp.tube(p, 'Trim', [(wx - 0.08, y, Z0 + wh - 0.1), (wx - 1.2, y, Z0 + 0.05)], 0.06, seg=8)
        vp.tube(p, 'Trim', [(wx + 0.08, y, Z0 + 0.1), (wx + 0.08, y, Z0 + wh + 0.05)], 0.06, seg=8)
    ub.obox(p, 'Trim#vp', (wx, (wy0 + wy1) / 2, Z0 + wh + 0.04), (0.2, wy1 - wy0 + 0.1, 0.08), X, Y, Z)
    # a carga com a lona azul amarrada (o canto de trás do pátio, +Y)
    ub.obox(p, 'Crate#vp', (5.7, 7.5, Z0 + 0.08), (2.2, 1.6, 0.16), X, Y, Z)
    ub.obox(p, 'Body#sm', (5.7, 7.5, Z0 + 0.62), (1.9, 1.3, 0.92), X, Y, Z)
    ub.tarp_cover_at(p, (5.7, 7.5), 2.2, 1.6, 0.0, Z0 + 0.2, 0.98, m='Player', seed=2.3, ties=3, box=14)
    # as caixas de munição empilhadas (−Y, perto da porta)
    for (x, y, z, yaw) in ((VX1 + 0.5, -6.8, 0.0, 3.0), (VX1 + 0.5, -7.65, 0.0, -2.0), (VX1 + 0.55, -7.2, 0.5, 6.0)):
        vp.rbox(p, 'Trim', (x, y, Z0 + z + 0.25), (1.0, 0.6, 0.5), yaw=yaw, r=0.05, k=2)
        ub.obox(p, 'Paint#vp', (x + 0.51, y, Z0 + z + 0.3), (0.02, 0.4, 0.12), X, Y, Z)
    # os postes de luz nas pontas da frente
    ub.lamp_post(p, 9.9, 6.9, h=5.6, aim=(6.0, 0.0))
    ub.lamp_post(p, 9.9, -6.9, h=5.6, aim=(6.0, 0.0))


def base():
    p = UPart(PREFIX + '_Base')
    ub.keys(p)
    ground(p)
    hut(p)
    front(p)
    sides(p)
    rear(p)
    yard(p)
    vp.poke_ngons(p, 0.05)
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


FIRE_POINTS = [(-9.0, 1.8, round(arch_z(1.8, T0 + 0.06), 2)), (-4.6, -2.6, round(arch_z(2.6, T0 + 0.06), 2)),
               (0.4, 2.0, round(arch_z(2.0, T0 + 0.06), 2))]


def write_spec():
    import json
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1400, 1200]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1500, 750]],
                      ['side', 'ortho', [0, 1, 0], [0, 0, 1], [1600, 650]],
                      ['three_quarter', 'persp', 34.0, 215.0, [1600, 1000], 30.0, 0.95],
                      ['three_quarter_right', 'persp', 34.0, 145.0, [1600, 1000], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 32.0, 35.0, [1600, 1000], 30.0, 0.95]],
            'usa_at': [6.0, 13.6, 0],
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, ensure_ascii=False, indent=1)


def main():
    clear_scene()
    b = base()
    total = export(b, OUT)
    pts = [(QX0 - 0.5, -QA - 0.7), (QX1 + 0.3, -QA - 0.7), (QX1 + 0.3, QA + 0.7), (QX0 - 0.5, QA + 0.7)]
    total += export(scaffold_poly(PREFIX + '_Scaffold', pts, arch_z(0.0) + 0.1, out=1.0, skip=(1,)), OUT)
    print('MODEL total', total, 'tris (antes da grade do executor)')
    print('MODEL tris por peca', {b.name: tris(b)})
    import bpy
    bpy.context.view_layer.update()
    pts = [b.matrix_world @ Vector(c) for c in b.bound_box]
    lo = (min(q.x for q in pts), min(-q.y for q in pts), min(q.z for q in pts))
    hi = (max(q.x for q in pts), max(-q.y for q in pts), max(q.z for q in pts))
    print('MODEL caixa da Base (Unreal) x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f; lote x ±%.2f y ±%.2f' % (lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                                                                                                         HX, HY))
    print('MODEL arco: cumeeira %.2f m, vão %.1f m, comprimento %.1f m; porta 2,4 × 2,6 m em x %.2f; saída dos sacos ±%.1f m' % (
        arch_z(0.0) + T0 + CA, 2 * QA, QX1 - QX0, VX1, GAP_Y))
    print('MODEL fire_points_m', FIRE_POINTS)
    print('MODEL NONMANIFOLD', NONMANIFOLD if NONMANIFOLD else 'nenhum')
    write_spec()


if not os.environ.get('MODEL_NO_MAIN'):           # (as contas de triângulos por seção importam o modelo sem rodar)
    main()
