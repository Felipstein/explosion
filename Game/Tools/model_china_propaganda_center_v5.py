"""O Propaganda Center da China, v5 (03/11/2026): A TORRE VERMELHA. A direção fechada em docs/elenco/ICONES_03_11.md (5ª rodada: as 4
anteriores inventavam objeto de fora do jogo; o pedido é um ícone da família, autêntico, que todo mundo lembra, sem copiar o ZH).

Rodar (Blender sem janela, só CPU), com o detalhe AAA:
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_run_detail.py -- \
      building Game/Tools/model_china_propaganda_center_v5.py captures/game/models/china_propaganda_center_v5

A palavra (o que lê em 1 segundo): A TORRE VESTIDA DE FAIXAS VERMELHAS COM A ESTRELA NO TOPO.
- A torre de 3 andares da família da China (o quartel, o QG e a fábrica aprovados): a base de concreto oliva de bunker (as paredes
  inclinadas de placas, os pilares de quina, a cornija, as ameias), o salão das colunas vermelhas (o peristilo de 12 colunas em volta
  da cela de treliça) com o telhado verde de pagode em saia, o andar de cima com as colunas de canto e o telhado de pavilhão (as 4
  águas que se juntam na ponta, a beira vermelha, as pontas levantadas).
- VESTIDA por 4 faixas verticais (横幅, as faixas de slogan dos prédios chineses), uma em cada face, na cor forte do prédio (a cor do
  jogador: o vermelho da China por padrão, docs/CONCEITO_CONSTRUCOES.md §8), de ~7 m: penduradas na vara sob o beiral do 1º telhado,
  caem até a base com o caimento leve de pano (a barriga pra fora, as dobras rasas que saem da vara) e o peso embaixo amarrado nas
  pedras do pódio. Cada uma com 4 caracteres dourados em relevo (a fonte Microsoft YaHei Bold do Windows, em malha), lidos de cima pra
  baixo e no sentido certo em cada face (a Unreal é de mão esquerda: nas faces +X e −Y o texto espelha, como no star_wall):
  +X 人民万岁, −Y 团结奋斗, −X 中国万岁, +Y 保卫祖国.
- No topo, a ESTRELA VERMELHA grande (o slot Red fixo, como a estrela aprovada do ELP) facetada como as estrelas das torres (as
  arestas que pegam o sol), com a borda dourada (CnGold), na peça que gira (Speakers) em volta do mastro; o pomo dourado embaixo.
- Os alto-falantes (as cornetas de 2 em 2) pendurados nos 4 cantos do beiral de cima.
- O lote 14 × 24 m: a torre fica no meio-fundo, no pódio de pedra de 2 degraus que acompanha a planta dela; na frente, a praça de
  lajes de pedra com o portão de colunas vermelhas (o paifang de 3 vãos da família, baixo: na câmera do jogo ele não tapa a faixa da
  frente), as 4 bandeiras (o pano no vento: flags.json) e as 2 lanternas de pedra. O resto é o chão do mapa (nada de laje embaixo de tudo, nada de muro).
- De cima: o telhado verde em 2 anéis com a cruz vermelha das 4 faixas saindo do beiral, a estrela no meio; na frente, a praça clara.

Peças (origem no chão, no centro do lote; coordenadas da Unreal, X frente, Y direita, Z cima, metros):
  SM_China_PropagandaCenter_Base: tudo o que não se mexe (as bandeiras saem no flags.json);
  SM_China_PropagandaCenter_Speakers: a estrela (facetada, a borda dourada, o colar e a haste), feita com a origem no eixo, no topo
    do mastro: spin em Z, at_m = SPK_AT;
  SM_China_PropagandaCenter_Scaffold: o andaime da obra (tubos de aço, scaffold() do model_lib).
Materiais (os slots da China): CnConcrete, CnPlinth, CnRoof, CnMetal, CnGold, Player (as faixas, as colunas, a beira dos telhados, as
portas, as bandeiras), Red (a estrela), Dark, Glass, Light, Metal (as cornetas), Trim, Crate e Hazard (só no andaime).
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model_lib import COLORS, V, clear_scene, export, flag, scaffold  # noqa: E402,F401
from model_faction_kit import lamp, revolve, star_points, star_wall, polygon_prism  # noqa: E402,F401
from model_building_parts import (BPart, WallFace, wbox, cn_block, cn_window, splits, tube, bake_in_place,  # noqa: E402,F401
                                  ring_boxes, catenary)
from model_china_war_factory import cn_roof, cn_frieze, pagoda_story, horn  # noqa: E402

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'china_propaganda_center_v5')
os.makedirs(OUT, exist_ok=True)
PREFIX = 'SM_China_PropagandaCenter'
COLORS.setdefault('Metal', (92, 90, 82))
COLORS.setdefault('Crate', (84, 86, 58))
FONT_PATH = r'C:\Windows\Fonts\msyhbd.ttc'

# ===================================================================================================== as medidas
TX, TY = -2.0, 0.0                 # o eixo da torre (o mais pro fundo que o pódio deixa: a praça e o portão longe da faixa da frente)
POD = (5.0, 4.45)                  # o pódio de pedra: as meias-medidas dos 2 degraus (0,22 m cada)
PZ = 0.45                          # o topo do pódio
BH = 3.6                           # a meia-medida do bunker no pé
BZ = 4.05                          # o topo do bunker
LEAN = 0.06                        # o recuo das paredes do bunker por metro
COL = 2.75                         # o peristilo (as 12 colunas vermelhas)
CELLA = 2.15                       # a cela de treliça dentro do peristilo
ZS = 4.3                           # o piso do salão (o topo do estilóbata)
Z1 = 7.9                           # o beiral do 1º telhado
R1 = dict(rise=1.15, over=1.2)
UP = 1.75                          # o andar de cima
UP_H = 2.0
R2 = dict(rise=1.8, over=1.0)
STAR_R = 1.3                       # o raio da estrela (ponta)
STAR_ZC = 1.25                     # o centro da estrela acima do eixo
BAN_W, BAN_T = 2.0, 0.04           # a faixa: largura e espessura
CELL = 1.4                         # o tamanho de cada caractere
SLOGANS = {'+x': '人民万岁', '-y': '团结奋斗', '-x': '中国万岁', '+y': '保卫祖国'}
GX = 6.35                          # o portão: as colunas do vão do meio e as dos lados
GATE_Y, GATE_Y2 = 1.3, 2.75
FLAGS = [(6.45, -4.3), (6.45, -6.5), (6.45, 4.3), (6.45, 6.5)]


# ===================================================================================================== utilidades
def finish(p):
    o = p.finish()
    cnt = {}
    for poly in o.data.polygons:
        k = o.data.materials[poly.material_index].name if o.data.materials else '?'
        cnt[k] = cnt.get(k, 0) + len(poly.vertices) - 2
    print('TRIS', o.name, sum(cnt.values()), sorted(cnt.items(), key=lambda kv: -kv[1])[:14])
    return o


def fmap(face, u, n, z):
    """O ponto no quadro da face da torre: u ao longo (o Y do mundo nas faces ±x, o X nas ±y), n pra fora a partir do eixo."""
    if face == '+x':
        return (TX + n, TY + u, z)
    if face == '-x':
        return (TX - n, TY + u, z)
    if face == '+y':
        return (TX + u, TY + n, z)
    return (TX + u, TY - n, z)


def rod(p, m, a, b, r, seg=8):
    d = Vector(b) - Vector(a)
    if d.length > 1e-4:
        c = (Vector(a) + Vector(b)) / 2
        p.cyl(m, tuple(c), tuple(d), r, d.length, seg=seg, bevel=0.0)


# ===================================================================================================== os caracteres (a fonte em malha)
_GLYPHS = {}


def glyph(ch):
    """O caractere da fonte em malha (o contorno preenchido e extrudado), centrado na caixa dele, em unidades da fonte (tamanho 1):
    devolve (vértices, faces, meia-espessura)."""
    if ch in _GLYPHS:
        return _GLYPHS[ch]
    font = bpy.data.fonts.load(FONT_PATH, check_existing=True)
    cu = bpy.data.curves.new('glyph', 'FONT')
    cu.body = ch
    cu.font = font
    cu.size = 1.0
    e = 0.024
    cu.extrude = e
    cu.fill_mode = 'BOTH'
    cu.resolution_u = 3
    o = bpy.data.objects.new('glyph', cu)
    bpy.context.scene.collection.objects.link(o)
    dg = bpy.context.evaluated_depsgraph_get()
    oe = o.evaluated_get(dg)
    me = oe.to_mesh()
    vs = [tuple(v.co) for v in me.vertices]
    fs = [tuple(f.vertices) for f in me.polygons]
    oe.to_mesh_clear()
    bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.curves.remove(cu)
    cx = (min(v[0] for v in vs) + max(v[0] for v in vs)) / 2
    cy = (min(v[1] for v in vs) + max(v[1] for v in vs)) / 2
    _GLYPHS[ch] = ([(x - cx, y - cy, z) for (x, y, z) in vs], fs, e)
    return _GLYPHS[ch]


# ===================================================================================================== a faixa vertical
class Banner:
    """A faixa de slogan pendurada numa face da torre: o pano de largura BAN_W que cai de ZT até ZT − L, de n0 (embaixo da vara) até n1
    (o peso, amarrado no pódio), com a barriga pra fora e as dobras rasas que saem da vara. S(a, t): a ∈ [−½, ½] na largura, t ∈ [0, 1]
    de cima pra baixo."""

    def __init__(self, face, zt, L, n0, n1):
        self.face, self.zt, self.L, self.n0, self.n1 = face, zt, L, n0, n1
        self.out = Vector({'+x': (1, 0, 0), '-x': (-1, 0, 0), '+y': (0, 1, 0), '-y': (0, -1, 0)}[face])

    def S(self, a, t):
        u = a * BAN_W * (1.0 - 0.03 * math.sin(math.pi * t))
        n = self.n0 + (self.n1 - self.n0) * t + 0.16 * math.sin(math.pi * t) + 0.028 * math.cos(4 * math.pi * a) * (0.35 + 0.65 * t)
        z = self.zt - self.L * t
        return Vector(fmap(self.face, u, n, z))

    def N(self, a, t):
        e = 1e-3
        da = self.S(a + e, t) - self.S(a - e, t)
        dt = self.S(a, t + e) - self.S(a, t - e)
        nn = da.cross(dt).normalized()
        return nn if nn.dot(self.out) > 0 else -nn

    def cloth(self, p, m='Player#ban', nu=10, nv=30):
        """O pano com espessura (as 2 camadas pela normal, as bordas fechadas)."""
        b = p.bm(m, 0.0)
        top, bot = [], []
        for j in range(nv + 1):
            t = j / nv
            rt, rb = [], []
            for i in range(nu + 1):
                a = -0.5 + i / nu
                q, nn = self.S(a, t), self.N(a, t)
                rt.append(b.verts.new(V(*(q + nn * BAN_T / 2))))
                rb.append(b.verts.new(V(*(q - nn * BAN_T / 2))))
            top.append(rt)
            bot.append(rb)
        for j in range(nv):
            for i in range(nu):
                b.faces.new([top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]]).smooth = True
                b.faces.new([bot[j + 1][i], bot[j + 1][i + 1], bot[j][i + 1], bot[j][i]]).smooth = True
        edge = [(0, i) for i in range(nu)] + [(j, nu) for j in range(nv)] + [(nv, i) for i in range(nu, 0, -1)] + [(j, 0) for j in range(nv, 0, -1)]
        for (j0, i0), (j1, i1) in zip(edge, edge[1:] + edge[:1]):
            b.faces.new([top[j0][i0], bot[j0][i0], bot[j1][i1], top[j1][i1]])

    def strip(self, p, m, a0, a1, t0, t1, lift=0.004, h=0.014, nv=24):
        """A tira em relevo colada no pano (o friso dourado das bordas)."""
        b = p.bm(m, 0.0)
        rings = []
        for j in range(nv + 1):
            t = t0 + (t1 - t0) * j / nv
            ring = []
            for (a, d) in ((a0, lift), (a1, lift), (a1, lift + h), (a0, lift + h)):
                q, nn = self.S(a, t), self.N(a, t)
                ring.append(tuple(q + nn * (BAN_T / 2 + d)))
            rings.append(ring)
        p.loft(m, rings, bevel=0.0)

    def text(self, p, chars, m='CnGold#glyph'):
        """Os caracteres dourados em relevo, de cima pra baixo; nas faces +X e −Y o u espelha (a leitura certa de quem olha)."""
        mir = -1.0 if self.face in ('+x', '-y') else 1.0
        pitch = CELL + 0.2
        t_first = 0.5 - (len(chars) - 1) * pitch / 2 / self.L
        b = p.bm(m, 0.0)
        for k, ch in enumerate(chars):
            vs, fs, e = glyph(ch)
            s = CELL / 0.82
            tc = t_first + k * pitch / self.L
            new = []
            for (x, y, z) in vs:
                a = mir * x * s / BAN_W
                t = tc - y * s / self.L
                q, nn = self.S(a, t), self.N(a, t)
                new.append(b.verts.new(V(*(q + nn * (BAN_T / 2 + 0.003 + (z + e) * s)))))
            for f in fs:
                try:
                    b.faces.new([new[i] for i in f])
                except ValueError:
                    pass

    def hardware(self, p):
        """A vara de cima (presa no beiral) com as pontas douradas, o peso de baixo com as pontas, as cordas até as pedras do pódio."""
        for (t, r) in ((0.0, 0.055), (1.0, 0.065)):
            a = self.S(-0.5 - 0.09, t)
            b_ = self.S(0.5 + 0.09, t)
            nn = self.N(0.0, t)
            off = nn * (-0.01 if t == 0 else 0.0) + Vector((0, 0, 0.04 if t == 0 else -0.03))
            rod(p, 'CnMetal#ban', a + off, b_ + off, r, seg=10)
            for q in (a + off, b_ + off):
                d = (b_ - a).normalized()
                sgn = 1 if (q - a).length > 0.1 else -1
                p.cyl('CnGold#ban', tuple(q + d * sgn * 0.04), tuple(d), r * 1.6, 0.1, seg=10, bevel=0.0)
        # as 2 alças de cima (sobem até a fáscia do beiral)
        for a in (-0.38, 0.38):
            q = self.S(a, 0.0) + Vector((0, 0, 0.04))
            rod(p, 'Dark#ban', q, q + Vector((0, 0, 0.32)), 0.02, seg=6)
        # o peso de baixo amarrado nas pedras do pódio
        for a in (-0.42, 0.42):
            q = self.S(a, 1.0) + Vector((0, 0, -0.05))
            g = Vector(fmap(self.face, a * BAN_W * 1.08, POD[0] + 0.45, 0.0))
            tube(p, 'Dark#rope', catenary(tuple(q), tuple(g + Vector((0, 0, 0.3))), 0.04, 6), 0.02, seg=6)
            c = g + Vector((0, 0, 0.15))
            p.obox('CnPlinth#anchor', tuple(c), (0.42, 0.42, 0.3), bevel=0.06)
            p.torus('CnMetal#ring', tuple(g + Vector((0, 0, 0.34))), 'z', 0.06, 0.015, seg=10, rseg=4)

    def build(self, p, chars):
        self.cloth(p)
        for (a0, a1) in ((-0.475, -0.43), (0.43, 0.475)):
            self.strip(p, 'CnGold#trim', a0, a1, 0.012, 0.988)
        self.strip(p, 'CnGold#trim', -0.475, 0.475, 0.022, 0.03, nv=2)
        self.strip(p, 'CnGold#trim', -0.475, 0.475, 0.97, 0.978, nv=2)
        self.text(p, chars)
        self.hardware(p)


# ===================================================================================================== o pódio e a praça
def podium(p):
    """O pódio de pedra de 2 degraus que acompanha a planta da torre (a quina chanfrada) e a praça de lajes na frente."""
    for k, (h, z0, z1) in enumerate(((POD[0], -0.08, 0.22), (POD[1], 0.2, PZ))):
        c = 0.9 - 0.25 * k
        pts = [(TX + h, TY - h + c), (TX + h, TY + h - c), (TX + h - c, TY + h), (TX - h + c, TY + h), (TX - h, TY + h - c),
               (TX - h, TY - h + c), (TX - h + c, TY - h), (TX + h - c, TY - h)]
        p.loft('CnPlinth#pod', [[(x, y, z0) for (x, y) in pts], [(x, y, z1) for (x, y) in pts]], bevel=0.05)
    # a praça de lajes (as placas de pedra com a junta), da frente do pódio até o portão
    x0, x1, y0, y1 = TX + POD[0] - 0.4, 6.95, -7.4, 7.4
    p.box('CnConcrete#plaza', x0, x1, y0, y1, -0.08, 0.04, bevel=0.03)
    xs = splits(x0 + 0.1, x1 - 0.1, 1.15)
    ys = splits(y0 + 0.1, y1 - 0.1, 1.2)
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            mid = abs((ys[j] + ys[j + 1]) / 2) < 1.6
            p.box('CnPlinth#slab' if not mid else 'CnPlinth#path', xs[i] + 0.03, xs[i + 1] - 0.03, ys[j] + 0.03, ys[j + 1] - 0.03,
                  0.02, 0.1 if not mid else 0.12, bevel=0.03)
    # as 2 lanternas de pedra junto do pódio
    for s in (-1, 1):
        x, y = TX + POD[0] + 0.7, s * 2.9
        p.box('CnPlinth', x - 0.36, x + 0.36, y - 0.36, y + 0.36, 0.1, 0.4, bevel=0.04)
        p.box('CnPlinth', x - 0.24, x + 0.24, y - 0.24, y + 0.24, 0.4, 1.35, bevel=0.04)
        p.box('Dark', x - 0.26, x + 0.26, y - 0.26, y + 0.26, 1.35, 1.42, bevel=0.0)
        p.box('Light', x - 0.2, x + 0.2, y - 0.2, y + 0.2, 1.42, 1.82, bevel=0.0)
        for (dx, dy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p.box('CnMetal', x + dx * 0.22 - 0.035, x + dx * 0.22 + 0.035, y + dy * 0.22 - 0.035, y + dy * 0.22 + 0.035, 1.42, 1.82, bevel=0.0)
        p.loft('CnRoof', [[(x - 0.42, y - 0.42, 1.82), (x + 0.42, y - 0.42, 1.82), (x + 0.42, y + 0.42, 1.82), (x - 0.42, y + 0.42, 1.82)],
                          [(x - 0.05, y - 0.05, 2.12), (x + 0.05, y - 0.05, 2.12), (x + 0.05, y + 0.05, 2.12), (x - 0.05, y + 0.05, 2.12)]], bevel=0.02)
        p.cyl('CnGold', (x, y, 2.2), 'z', 0.07, 0.16, seg=10, r2=0.02, bevel=0.0)


def gate(p):
    """O portão da praça (o paifang da família, o do quartel aprovado em escala de praça: 3 vãos, 4 colunas): as colunas vermelhas nos
    tambores de pedra, as vigas vermelhas com o friso dourado, a placa da estrela do ELP no vão do meio, as plaquinhas dos lados e os 3
    telhadinhos. BAIXO de propósito (o topo da telha a ~3,9 m): na câmera do jogo (37,5°) ele não tapa a faixa da frente da torre."""
    z_main, z_side = 2.85, 2.25
    for y in (-GATE_Y2, -GATE_Y, GATE_Y, GATE_Y2):
        top = z_main if abs(y) < GATE_Y + 0.1 else z_side
        p.box('CnPlinth', GX - 0.36, GX + 0.36, y - 0.36, y + 0.36, 0.05, 0.42, bevel=0.04)
        for d in (-1, 1):
            p.cyl('CnPlinth', (GX + d * 0.34, y, 0.78), 'y', 0.32, 0.22, seg=18, bevel=0.0)
            p.box('CnPlinth', min(GX + d * 0.16, GX + d * 0.58), max(GX + d * 0.16, GX + d * 0.58), y - 0.11, y + 0.11, 0.42, 0.55, bevel=0.02)
        p.cyl('Player', (GX, y, (0.42 + top) / 2), 'z', 0.19, top - 0.42, seg=18, bevel=0.0, smooth=True)
        p.cyl('CnGold', (GX, y, 0.5), 'z', 0.23, 0.1, seg=18, bevel=0.0)

    def beam(y0, y1, z0, z1):
        p.box('Player#beam', GX - 0.17, GX + 0.17, y0, y1, z0, z1, bevel=0.02)
        for s in (-1, 1):
            p.box('CnGold', GX + s * 0.17 - 0.012, GX + s * 0.17 + 0.012, y0 + 0.08, y1 - 0.08, (z0 + z1) / 2 - 0.025, (z0 + z1) / 2 + 0.025, bevel=0.0)
    beam(-GATE_Y - 0.25, GATE_Y + 0.25, z_main - 0.75, z_main - 0.5)
    beam(-GATE_Y - 0.25, GATE_Y + 0.25, z_main - 0.02, z_main + 0.22)
    p.box('CnGold', GX - 0.1, GX + 0.1, -0.72, 0.72, z_main - 0.5, z_main - 0.02, bevel=0.02)           # a placa
    for s in (-1, 1):
        p.box('Player', GX + s * 0.1 - 0.012, GX + s * 0.1 + 0.012, -0.62, 0.62, z_main - 0.45, z_main - 0.07, bevel=0.0)
        star_wall(p, (GX + s * 0.115, 0.0, z_main - 0.26), '+x' if s > 0 else '-x', 0.16, t=0.012)
    cn_frieze(p, GX, 0.0, 0.24, GATE_Y + 0.2, z_main + 0.42, h=0.2, step=0.45)
    gate_roof(p, GX, 0.0, 0.24, GATE_Y + 0.2, z_main + 0.42, 0.55, over=0.42, curl=0.3, sweep=0.14, tip=0.45)
    for s in (-1, 1):
        yc = s * (GATE_Y + GATE_Y2) / 2
        half = (GATE_Y2 - GATE_Y) / 2
        beam(yc - half - 0.2, yc + half + 0.2, z_side - 0.3, z_side - 0.05)
        p.box('CnGold', GX - 0.09, GX + 0.09, yc - half + 0.22, yc + half - 0.22, z_side - 0.62, z_side - 0.3, bevel=0.0)
        p.box('Player', GX - 0.1, GX + 0.1, yc - half + 0.28, yc + half - 0.28, z_side - 0.57, z_side - 0.35, bevel=0.0)
        cn_frieze(p, GX, yc, 0.2, half + 0.18, z_side + 0.17, h=0.18, step=0.36)
        gate_roof(p, GX, yc, 0.2, half + 0.18, z_side + 0.17, 0.42, over=0.34, curl=0.24, sweep=0.12, tip=0.38)
    for y in (-0.9, 0.9):
        lantern(p, GX, y, z_main - 0.75, drop=0.14)


def gate_roof(p, cx, cy, ax, ay, z0, rise, over, curl, sweep, tip):
    """O telhadinho do portão: o cn_roof da família sem a cumeeira grande (o chiwen dele tem ~1 m, do tamanho do telhado do salão: no
    portão de praça virava um gancho dourado enorme) e a cumeeira na escala dele: o rolo de telha, a linha vermelha e as pontas
    douradas pequenas viradas pra cima."""
    top, tx, ty = cn_roof(p, cx, cy, ax, ay, z0, rise, over=over, curl=curl, sweep=sweep, n=6, rib_step=0.3, tip=tip, ridge=False)
    L = max(tx, ty) + 0.08
    p.box('CnRoof#ridge', cx - 0.11, cx + 0.11, cy - L, cy + L, top - 0.06, top + 0.13, bevel=0.03)
    p.box('Player#ridge', cx - 0.115, cx + 0.115, cy - L, cy + L, top + 0.06, top + 0.1, bevel=0.0)
    for s in (-1, 1):
        p.obox('CnGold#orn', (cx, cy + s * (L + 0.02), top + 0.2), (0.16, 0.14, 0.3), roll=-s * 22, bevel=0.02)
        p.obox('CnGold#orn', (cx, cy + s * (L - 0.06), top + 0.37), (0.14, 0.2, 0.08), roll=s * 28, bevel=0.0)
    p.cyl('CnGold#orn', (cx, cy, top + 0.2), 'z', 0.08, 0.16, seg=10, r2=0.03, bevel=0.0)


def flags(p):
    """As 4 bandeiras da praça (2 de cada lado do portão): o mastro de aço com a ponta dourada na base de pedra; o pano sai no flags.json."""
    for k, (x, y) in enumerate(FLAGS):
        p.box('CnPlinth', x - 0.32, x + 0.32, y - 0.32, y + 0.32, 0.05, 0.42, bevel=0.04)
        p.cyl('CnMetal', (x, y, (0.42 + 7.2) / 2), 'z', 0.075, 7.2 - 0.42, seg=10, r2=0.05, bevel=0.0, smooth=True)
        p.cyl('CnGold', (x, y, 7.28), 'z', 0.09, 0.16, seg=10, bevel=0.0)
        p.cyl('CnGold', (x, y, 7.52), 'z', 0.07, 0.32, seg=8, r2=0.0, bevel=0.0)
        flag(p, (x, y), 7.05, 2.1, 1.35, m='Player', phase=0.7 * k)


# ===================================================================================================== a torre
def bunker(p):
    """O 1º andar: o bunker oco de placas com as paredes inclinadas (cn_block), as 2 portas vermelhas na frente (o meio é da faixa), as
    janelinhas, as luzes, o parapeito de ameias em volta do terraço."""
    x0, x1, y0, y1 = TX - BH, TX + BH, TY - BH, TY + BH
    door = [(TY + s * 2.05 - 0.55, TY + s * 2.05 + 0.55, PZ, PZ + 2.3) for s in (-1, 1)]
    slit = lambda c: [(c + s * 2.05 - 0.28, c + s * 2.05 + 0.28, PZ + 2.15, PZ + 2.95) for s in (-1, 1)]
    holes = {'+x': door, '-x': slit(TY), '+y': slit(TX), '-y': slit(TX)}
    lean = {f: LEAN for f in ('+x', '-x', '+y', '-y')}
    top = cn_block(p, x0, x1, y0, y1, PZ - 0.05, BZ, t=0.4, lean=lean, holes=holes, skin=('+x', '-x', '+y', '-y'), roof_t=0.35,
                   parapet=0.0, corners=True, pier=0.75, cornice=True, interior=True, plinth=0.75, pw=2.35, zs=[PZ - 0.05, 1.7, 3.0])
    at = {'+x': x1, '-x': x0, '+y': y1, '-y': y0}
    for f, hs in holes.items():
        F = WallFace(f, at[f], PZ - 0.05, LEAN)
        for (a, b, c, d) in hs:
            if f == '+x':
                # a porta: a moldura clara, as 2 folhas vermelhas recuadas com os cravos dourados, a verga, a luz por cima
                wbox(p, 'Dark', F, a, b, c, d, -0.4, -0.37, bevel=0.0)
                for (ua, ub) in ((a + 0.04, (a + b) / 2 - 0.01), ((a + b) / 2 + 0.01, b - 0.04)):
                    wbox(p, 'Player#door', F, ua, ub, c, d - 0.04, -0.3, -0.24, bevel=0.0)
                    for zz in splits(c + 0.35, d - 0.35, 0.42):
                        for uu in (ua + 0.12, ub - 0.12):
                            wbox(p, 'CnGold#stud', F, uu - 0.03, uu + 0.03, zz - 0.03, zz + 0.03, -0.24, -0.2, bevel=0.0)
                wbox(p, 'CnPlinth#jamb', F, a - 0.16, a, c, d + 0.16, -0.25, 0.16, bevel=0.03)
                wbox(p, 'CnPlinth#jamb', F, b, b + 0.16, c, d + 0.16, -0.25, 0.16, bevel=0.03)
                wbox(p, 'CnPlinth#jamb', F, a - 0.24, b + 0.24, d, d + 0.2, -0.25, 0.22, bevel=0.03)
                u = (a + b) / 2
                xx = at[f] - LEAN * (d + 0.55 - PZ)
                lamp(p, xx, u, d + 0.55, face='+x')
            else:
                cn_window(p, F, a, b, c, d, t=0.4, mull=0)
    # o terraço em cima: o piso, o parapeito com as ameias e a capa (a "fortaleza" do QG)
    tx0, tx1, ty0, ty1 = top
    p.box('CnPlinth#deck', tx0 + 0.2, tx1 - 0.2, ty0 + 0.2, ty1 - 0.2, BZ, BZ + 0.04, bevel=0.0)
    pw, ph, mh = 0.36, 0.42, 0.55
    segs = [((tx0, ty0), (tx1, ty0)), ((tx0, ty1), (tx1, ty1)), ((tx0, ty0 + pw), (tx0, ty1 - pw)), ((tx1, ty0 + pw), (tx1, ty1 - pw))]
    for (ax, ay), (bx, by) in segs:
        if ay == by:
            yy = ay + (pw / 2 if ay < TY else -pw / 2)
            p.box('CnConcrete', ax, bx, yy - pw / 2, yy + pw / 2, BZ, BZ + ph, bevel=0.04)
            n = max(1, round((bx - ax) / 1.25))
            for k in range(n):
                xm = ax + (k + 0.5) * (bx - ax) / n
                p.box('CnConcrete#merlon', xm - 0.32, xm + 0.32, yy - pw / 2 - 0.02, yy + pw / 2 + 0.02, BZ + ph, BZ + ph + mh, bevel=0.015)
                p.box('CnPlinth#cap', xm - 0.36, xm + 0.36, yy - pw / 2 - 0.05, yy + pw / 2 + 0.05, BZ + ph + mh, BZ + ph + mh + 0.08, bevel=0.015)
        else:
            xx = ax + (pw / 2 if ax < TX else -pw / 2)
            p.box('CnConcrete', xx - pw / 2, xx + pw / 2, ay, by, BZ, BZ + ph, bevel=0.04)
            n = max(1, round((by - ay) / 1.25))
            for k in range(n):
                ym = ay + (k + 0.5) * (by - ay) / n
                p.box('CnConcrete#merlon', xx - pw / 2 - 0.02, xx + pw / 2 + 0.02, ym - 0.32, ym + 0.32, BZ + ph, BZ + ph + mh, bevel=0.015)
                p.box('CnPlinth#cap', xx - pw / 2 - 0.05, xx + pw / 2 + 0.05, ym - 0.36, ym + 0.36, BZ + ph + mh, BZ + ph + mh + 0.08, bevel=0.015)


def hall(p):
    """O 2º andar: o estilóbata, a cela de treliça, o peristilo das 12 colunas vermelhas com a viga, o forro e o 1º telhado (em saia)."""
    p.box('CnPlinth', TX - COL - 0.4, TX + COL + 0.4, TY - COL - 0.4, TY + COL + 0.4, BZ, ZS, bevel=0.05)
    p.box('Dark', TX - COL - 0.42, TX + COL + 0.42, TY - COL - 0.42, TY + COL + 0.42, ZS - 0.1, ZS - 0.07, bevel=0.0)
    pagoda_story(p, TX, TY, CELLA, CELLA, ZS, Z1 - 0.35 - ZS, win_step=1.25)
    cols = splits(-COL, COL, 1.83)
    for (sx, sy) in [(x, y) for x in cols for y in cols if abs(x) > COL - 0.01 or abs(y) > COL - 0.01]:
        x, y = TX + sx, TY + sy
        p.cyl('CnPlinth', (x, y, ZS + 0.1), 'z', 0.32, 0.2, seg=16, bevel=0.0)
        p.cyl('Player#col', (x, y, (ZS + Z1 - 0.35) / 2), 'z', 0.19, Z1 - 0.35 - ZS, seg=16, bevel=0.0, smooth=True)
        p.cyl('CnGold#col', (x, y, Z1 - 0.4), 'z', 0.23, 0.08, seg=16, bevel=0.0)
    # a viga vermelha sobre as colunas (com o friso dourado), o forro escuro do peristilo e a viga pintada com as mísulas
    ring_boxes(p, 'Player#beam', TX - COL - 0.22, TX + COL + 0.22, TY - COL - 0.22, TY + COL + 0.22, Z1 - 0.42, Z1 - 0.3, 0.44, bevel=0.02)
    for s in (-1, 1):
        p.box('CnGold', TX - COL - 0.24, TX + COL + 0.24, TY + s * (COL + 0.22) - 0.02, TY + s * (COL + 0.22) + 0.02, Z1 - 0.39, Z1 - 0.33, bevel=0.0)
        p.box('CnGold', TX + s * (COL + 0.22) - 0.02, TX + s * (COL + 0.22) + 0.02, TY - COL - 0.24, TY + COL + 0.24, Z1 - 0.39, Z1 - 0.33, bevel=0.0)
    p.box('CnConcrete#ceil', TX - COL - 0.2, TX + COL + 0.2, TY - COL - 0.2, TY + COL + 0.2, Z1 - 0.32, Z1 - 0.18, bevel=0.0)
    cn_frieze(p, TX, TY, COL + 0.1, COL + 0.1, Z1, h=0.2, step=0.92)
    # as lanternas vermelhas penduradas nos vãos da frente (dos 2 lados da faixa)
    for y in (-1.83, 1.83):
        lantern(p, TX + COL + 0.05, TY + y, Z1 - 0.42)
    top1, tx1, ty1 = cn_roof(p, TX, TY, COL + 0.1, COL + 0.1, Z1, R1['rise'], over=R1['over'], inset=COL + 0.1 + R1['over'] - UP - 0.1,
                             curl=0.55, sweep=0.3)
    p.box('CnRoof', TX - tx1, TX + tx1, TY - ty1, TY + ty1, top1 - 0.05, top1 + 0.05, bevel=0.0)
    return top1


def lantern(p, x, y, z_top, drop=0.32):
    p.cyl('Dark', (x, y, z_top - drop / 2), 'z', 0.012, drop, seg=6, bevel=0.0)
    zt = z_top - drop
    p.cyl('CnGold', (x, y, zt - 0.04), 'z', 0.11, 0.08, seg=14, bevel=0.0)
    revolve(p, 'Player#lan', (x, y), [(0.11, zt - 0.08), (0.22, zt - 0.14), (0.27, zt - 0.27), (0.27, zt - 0.47), (0.22, zt - 0.6), (0.11, zt - 0.66)],
            seg=16, smooth=True)
    p.cyl('CnGold', (x, y, zt - 0.7), 'z', 0.11, 0.08, seg=14, bevel=0.0)
    p.cyl('Player#lan', (x, y, zt - 0.86), 'z', 0.03, 0.24, seg=8, r2=0.07, bevel=0.0)


def upper(p, top1):
    """O 3º andar: a parede curta com as colunas de canto e a treliça, o telhado de pavilhão (as 4 águas até a ponta), o pomo dourado
    e o mastro; as cornetas penduradas nos 4 cantos do beiral."""
    pagoda_story(p, TX, TY, UP, UP, top1 - 0.05, UP_H, win_step=1.1)
    z2 = top1 - 0.05 + UP_H
    Ex = UP + R2['over']
    top2, _, _ = cn_roof(p, TX, TY, UP, UP, z2, R2['rise'], over=R2['over'], inset=Ex - 0.3, curl=0.6, sweep=0.32, ridge=False, tip=0.85)
    # o pomo (宝顶): o pescoço de telha, o prato e a cabaça dourada, e o mastro de aço até o eixo da estrela
    p.box('CnRoof#cap', TX - 0.36, TX + 0.36, TY - 0.36, TY + 0.36, top2 - 0.05, top2 + 0.12, bevel=0.03)
    revolve(p, 'CnGold#fin', (TX, TY), [(0.02, top2 + 0.1), (0.3, top2 + 0.12), (0.34, top2 + 0.2), (0.2, top2 + 0.28), (0.16, top2 + 0.36),
                                         (0.28, top2 + 0.52), (0.3, top2 + 0.66), (0.2, top2 + 0.82), (0.1, top2 + 0.9), (0.08, top2 + 0.96)],
            seg=24, smooth=True)
    z_axle = top2 + 1.35
    p.cyl('CnMetal#mast', (TX, TY, (top2 + 0.9 + z_axle) / 2), 'z', 0.075, z_axle - top2 - 0.9, seg=12, bevel=0.0)
    # as cornetas: o braço de aço saindo do canto (embaixo da ponta levantada do beiral) e 2 cornetas abertas em leque, pra baixo
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        d = Vector((sx, sy, 0.0)).normalized()
        c0 = Vector((TX + sx * UP, TY + sy * UP, z2 - 0.5))
        c1 = c0 + d * 1.05
        rod(p, 'CnMetal#arm', c0, c1, 0.06, seg=8)
        rod(p, 'CnMetal#arm', c0 - Vector((0, 0, 0.55)), c1 - Vector((0, 0, 0.02)), 0.04, seg=6)
        p.box('CnMetal#arm', c1.x - 0.16, c1.x + 0.16, c1.y - 0.16, c1.y + 0.16, c1.z - 0.22, c1.z + 0.06, bevel=0.0)
        for sgn in (-1, 1):
            a = math.atan2(sy, sx) + sgn * math.radians(26)
            dd = (math.cos(a) * 0.9, math.sin(a) * 0.9, -0.42)
            horn(p, (c1.x + math.cos(a) * 0.12, c1.y + math.sin(a) * 0.12, c1.z - 0.14), dd, r0=0.09, r1=0.36, L=0.95)
        tube(p, 'Dark#cable', catenary(tuple(c0 + Vector((0, 0, -0.1))), tuple(c1 + Vector((0, 0, -0.2))), 0.12, 6), 0.018, seg=5)
    return z_axle


def banners(p):
    """As 4 faixas (uma por face), penduradas sob a fáscia do 1º telhado, caindo até o pódio."""
    Ex = COL + 0.1 + R1['over']
    zt = Z1 - 0.38
    L = zt - (PZ + 0.18)
    for f, txt in SLOGANS.items():
        Banner(f, zt, L, Ex + 0.03, Ex + 0.95).build(p, txt)


def base():
    p = BPart(PREFIX + '_Base')
    for m in ('Player#ban', 'CnGold#glyph', 'CnGold#trim', 'Dark#rope', 'CnRoof'):
        p.bm(m, 0.0)
    podium(p)
    gate(p)
    flags(p)
    bunker(p)
    top1 = hall(p)
    z_axle = upper(p, top1)
    banners(p)
    return finish(p), z_axle


# ===================================================================================================== a estrela que gira
def offset_poly(pts, w):
    """O polígono deslocado w pra fora (cada vértice na interseção das 2 arestas deslocadas)."""
    n = len(pts)
    out = []
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    sgn = 1.0 if area > 0 else -1.0
    for i in range(n):
        a, b, c = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        d0, d1 = (b - a).normalized(), (c - b).normalized()
        n0 = Vector((d0.y, -d0.x)) * sgn
        n1 = Vector((d1.y, -d1.x)) * sgn
        m = (n0 + n1)
        cosh = max(0.25, m.length / 2)
        out.append(tuple(b + m.normalized() * (w / cosh)))
    return out


def speakers():
    """A estrela facetada (as 10 facetas de cada lado se encontram no centro levantado: as arestas pegam o sol), vermelha (Red, fixo), na
    borda dourada com espessura; a haste e o colar dourado no eixo. Origem no eixo (o topo do mastro); a estrela no plano YZ (de frente
    pra +X em repouso)."""
    p = BPart(PREFIX + '_Speakers', cull=False)
    p.bm('Red', 0.0)
    p.bm('CnGold#rim', 0.0)
    zc = STAR_ZC
    pts = star_points(0.0, zc, STAR_R, r_in=STAR_R * 0.40, rot_deg=90.0)      # (y, z)
    rim = offset_poly(pts, 0.13)
    e, D = 0.06, 0.24
    b = p.bm('Red', 0.0)
    cf = b.verts.new(V(e + D, 0.0, zc))
    cb = b.verts.new(V(-e - D, 0.0, zc))
    of = [b.verts.new(V(e, y, z)) for (y, z) in pts]
    ob = [b.verts.new(V(-e, y, z)) for (y, z) in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        b.faces.new([cf, of[i], of[j]])
        b.faces.new([cb, ob[j], ob[i]])
        b.faces.new([of[i], ob[i], ob[j], of[j]])
    # a borda dourada (o anel em forma de estrela, mais grosso que a beira do vermelho)
    g = p.bm('CnGold#rim', 0.0)
    T = 0.1
    fi = [g.verts.new(V(T, y, z)) for (y, z) in pts]
    fo = [g.verts.new(V(T, y, z)) for (y, z) in rim]
    bi = [g.verts.new(V(-T, y, z)) for (y, z) in pts]
    bo = [g.verts.new(V(-T, y, z)) for (y, z) in rim]
    for i in range(n):
        j = (i + 1) % n
        g.faces.new([fi[i], fo[i], fo[j], fi[j]])
        g.faces.new([bi[j], bo[j], bo[i], bi[i]])
        g.faces.new([fo[i], bo[i], bo[j], fo[j]])
        g.faces.new([fi[j], bi[j], bi[i], fi[i]])
    # a haste até o entalhe de baixo da estrela e o colar dourado no eixo
    z_notch = zc - STAR_R * 0.40
    p.cyl('CnMetal', (0.0, 0.0, (0.1 + z_notch + 0.1) / 2), 'z', 0.07, z_notch, seg=12, bevel=0.0)
    p.cyl('CnGold', (0.0, 0.0, 0.1), 'z', 0.17, 0.2, seg=16, bevel=0.0)
    p.cyl('CnGold', (0.0, 0.0, 0.24), 'z', 0.12, 0.08, seg=16, bevel=0.0)
    return finish(p)


# ===================================================================================================== saída
def main():
    clear_scene()
    b, z_axle = base()
    total = export(b, OUT)
    spk_at = (TX, TY, round(z_axle, 3))
    clear_scene()
    s = speakers()
    bake_in_place(s, spk_at)
    total += export(s, OUT)
    clear_scene()
    sc = scaffold(PREFIX + '_Scaffold', TX - BH - 0.6, TX + BH + 0.6, TY - BH - 0.6, TY + BH + 0.6, 12.0, post='CnMetal', plank='Crate',
                  stripe='Hazard')
    total += export(sc, OUT)
    print('MODEL total', total, 'tris (antes do detalhe)')
    print('MODEL lote 14 x 24 m; speakers spin at_m', list(spk_at), 'rpm 3; topo da estrela', round(z_axle + STAR_ZC + STAR_R, 2), 'm')
    print('MODEL fire_points_m', [[TX + 2.9, TY - 2.9, BZ + 0.6], [TX - 2.9, TY + 2.9, BZ + 0.6], [TX, TY, round(Z1 + 1.6, 2)]])
    spec = {'parts': [[PREFIX + '_Base', [0, 0, 0]], [PREFIX + '_Speakers', [spk_at[0], -spk_at[1], spk_at[2]]]],
            'views': [['top', 'ortho', [0, 0, -1], [-1, 0, 0], [1100, 1500]],
                      ['front', 'ortho', [-1, 0, 0], [0, 0, 1], [1600, 1100]],
                      ['side_left', 'ortho', [0, -1, 0], [0, 0, 1], [1100, 1300]],
                      ['three_quarter', 'persp', 30.0, 215.0, [1400, 1400], 30.0, 0.95],
                      ['back_three_quarter', 'persp', 30.0, 35.0, [1400, 1400], 30.0, 0.95],
                      ['banner_close', 'persp', 12.0, 200.0, [1000, 1400], 22.0, 0.7]],
            'usa_at': [9.0, -14.5, 0], 'tank_views': True,
            'game': [['game_cam', 37.5, 200.0, 50.0, 0.85], ['game_cam_close', 37.5, 200.0, 30.0, 0.8]],
            'scaffold': PREFIX + '_Scaffold'}
    with open(os.path.join(OUT, 'preview.json'), 'w', encoding='utf-8') as f:
        json.dump(spec, f, indent=1)


if __name__ == '__main__':
    main()
