"""O tanque USA provisório da maquete do look (LookLab), modelado no Blender por script.

Rodar (Blender sem janela, só CPU; o look.ps1 tank chama):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/look_tank.py -- <pasta de saída>

Pedido do Felipe (11/10): um tanque bem detalhado pra ver o look completo na maquete, até os
modelos finais dele chegarem (aí eu julgo se eles casam com o ambiente). Provisório, mas no
padrão dos provisórios: quina dura com chanfro, normais duras, nunca remesh nem ruído.

A planta é a ficha docs/concept/units/usa_mbt_player_color.webp (5 vistas, com a de cima),
medida com o casco de 7,9 m como régua (docs/PRODUCAO_3D.md):
- casco: deck a 1,55 m; nariz em cunha (glacis superior de 3,15 a 3,95 m, inferior até 0,55 m);
  3,8 m de largura com as saias; a traseira com a grade do motor e a lona azul (os últimos 1,45 m);
- torre facetada em cunha: do nariz em +2,27 m ao cesto em −2,55 m, 3,0 m de largura nas faces,
  topo a 2,48 m; caixa do sensor com lente azul, mira do atirador, cúpula octogonal, escotilha,
  lançadores de defesa ativa (2×2 nas faces e atrás), lança-fumaça, mastro do sensor (4,4 m),
  antenas e o cesto com carga;
- cano: 1,69 m além do nariz do casco (ponta em +5,64 m), luva térmica com braçadeiras,
  evacuador, boca; mantelete;
- rodagem: 7 rodas duplas por lado (a cada ~0,82 m), roda-guia na frente, roda motriz atrás;
  esteira com elos de 17 cm (sapata, garra e guia).
A cor do jogador (CONCEITO_UNIDADES.md: detalhes pequenos seguindo as placas, nunca uma placa
grande): a faixa segmentada nas saias, retalhos nas faces e bordas da torre, três anéis no cano
e a lona amarrada no motor.

Materiais (cada nome vira um slot na Unreal; o look_lab.py liga o pó do mundo por cima):
Paint, Team (a cor do jogador: o jogo pinta pelo parâmetro TeamColor), Dark, Metal, Cloth, Lens, Track. Peças em GLBs separados (casco, torre, cano,
rodas, esteiras), todas com a origem do tanque (no chão, no centro): a maquete põe juntas.
Eixos: Unreal X pra frente, Y pra direita, Z pra cima; o Blender recebe (x, −y, z).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'tank')
os.makedirs(OUT, exist_ok=True)


def srgb(r, g, b):
    f = lambda c: (c / 255) / 12.92 if c / 255 <= 0.04045 else ((c / 255 + 0.055) / 1.055) ** 2.4
    return (f(r), f(g), f(b), 1.0)


COLORS = {'Char': (26, 23, 20), 'Rust': (74, 46, 30), 'Paint': (196, 198, 192), 'Team': (86, 108, 150), 'Dark': (40, 40, 38), 'Metal': (95, 95, 92),
          'Cloth': (64, 86, 132), 'Lens': (30, 60, 110), 'Track': (55, 52, 48)}
MATS = {}


def mat(name):
    if name not in MATS:
        m = bpy.data.materials.new(name)
        m.diffuse_color = srgb(*COLORS[name])
        MATS[name] = m
    return MATS[name]


def V(x, y, z):
    return Vector((x, -y, z))


class Part:
    """Uma peça (casco, torre...): uma bmesh por material, chanfradas e juntadas no fim."""

    def __init__(self, name):
        self.name = name
        self.bms = {}
        self.bevel = {}

    def bm(self, m, bevel=0.03):
        if m not in self.bms:
            self.bms[m] = bmesh.new()
            self.bevel[m] = bevel
        return self.bms[m]

    # --- primitivas (coordenadas da Unreal, metros) ---
    def box(self, m, x0, x1, y0, y1, z0, z1, bevel=None):
        b = self.bm(m, bevel if bevel is not None else 0.03)
        vs = [b.verts.new(V(x, y, z)) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
            b.faces.new([vs[i] for i in f])

    def obox(self, m, c, size, yaw=0.0, pitch=0.0, roll=0.0, bevel=None):
        """Caixa girada: centro c, tamanho (sx, sy, sz), giro (graus, Unreal)."""
        b = self.bm(m, bevel if bevel is not None else 0.02)
        r = bmesh.ops.create_cube(b, size=1.0)
        M = (Matrix.Translation(V(*c)) @ Matrix.Rotation(math.radians(-yaw), 4, 'Z')
             @ Matrix.Rotation(math.radians(-pitch), 4, 'Y') @ Matrix.Rotation(math.radians(roll), 4, 'X')
             @ Matrix.Diagonal((size[0], size[1], size[2], 1.0)))
        bmesh.ops.transform(b, matrix=M, verts=r['verts'])

    def cyl(self, m, p, axis, r, length, seg=24, r2=None, bevel=None):
        """Cilindro centrado em p ao longo de 'x', 'y' ou 'z' (ou vetor (x,y,z) da Unreal)."""
        b = self.bm(m, bevel if bevel is not None else 0.01)
        res = bmesh.ops.create_cone(b, cap_ends=True, segments=seg, radius1=r, radius2=r2 if r2 is not None else r,
                                    depth=length)
        if axis == 'x':
            R = Matrix.Rotation(math.radians(90), 4, 'Y')
        elif axis == 'y':
            R = Matrix.Rotation(math.radians(90), 4, 'X')
        elif axis == 'z':
            R = Matrix.Identity(4)
        else:
            d = V(*axis).normalized()
            R = Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4()
        bmesh.ops.transform(b, matrix=Matrix.Translation(V(*p)) @ R, verts=res['verts'])

    def prism_xz(self, m, pts, y0, y1, bevel=None):
        """Perfil de lado (x, z) extrudado de y0 a y1."""
        b = self.bm(m, bevel if bevel is not None else 0.03)
        a = [b.verts.new(V(x, y0, z)) for x, z in pts]
        c = [b.verts.new(V(x, y1, z)) for x, z in pts]
        b.faces.new(a)
        b.faces.new(list(reversed(c)))
        n = len(pts)
        for i in range(n):
            j = (i + 1) % n
            b.faces.new([a[i], a[j], c[j], c[i]])

    def loft(self, m, rings, bevel=None):
        """Anéis (listas de (x, y, z) com o mesmo número de pontos) ligados, com tampa nas pontas."""
        b = self.bm(m, bevel if bevel is not None else 0.03)
        vr = [[b.verts.new(V(*p)) for p in ring] for ring in rings]
        b.faces.new(list(reversed(vr[0])))
        b.faces.new(vr[-1])
        n = len(rings[0])
        for r0, r1 in zip(vr[:-1], vr[1:]):
            for i in range(n):
                j = (i + 1) % n
                b.faces.new([r0[i], r0[j], r1[j], r1[i]])

    def finish(self):
        objs = []
        for m, b in self.bms.items():
            bmesh.ops.remove_doubles(b, verts=b.verts, dist=1e-5)
            bmesh.ops.recalc_face_normals(b, faces=b.faces)
            me = bpy.data.meshes.new(f'{self.name}_{m}')
            b.to_mesh(me)
            b.free()
            o = bpy.data.objects.new(f'{self.name}_{m}', me)
            bpy.context.scene.collection.objects.link(o)
            o.data.materials.append(mat(m))
            if self.bevel[m] > 0:
                harden(o, self.bevel[m])
            objs.append(o)
        select_only(objs[0])
        for o in objs:
            o.select_set(True)
        bpy.ops.object.join()
        o = bpy.context.view_layer.objects.active
        o.name = self.name
        return o


def select_only(o):
    bpy.context.view_layer.update()
    for x in bpy.context.view_layer.objects:
        if x:
            x.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o


def harden(o, width):
    select_only(o)
    b = o.modifiers.new('bevel', 'BEVEL')
    b.width = width
    b.segments = 2 if width >= 0.02 else 1
    b.limit_method = 'ANGLE'
    b.angle_limit = math.radians(30)
    b.harden_normals = True
    b.miter_outer = 'MITER_ARC'
    bpy.ops.object.modifier_apply(modifier=b.name)
    w = o.modifiers.new('wn', 'WEIGHTED_NORMAL')
    w.keep_sharp = True
    w.weight = 50
    bpy.ops.object.modifier_apply(modifier=w.name)


# ---------------------------------------------------------------------------------- casco
DECK = 1.55
HALF_UP = 1.75          # casco de cima (sobre as esteiras)
HALF_LOW = 1.05         # casco de baixo (entre as esteiras)
SKIRT = (1.75, 1.88)    # as saias (de dentro pra fora)
WHEELS_X = [2.55, 1.71, 0.86, 0.05, -0.76, -1.58, -2.40]
WHEEL_R, WHEEL_Z = 0.29, 0.36
TRACK_Y, TRACK_W = 1.425, 0.64


def hull():
    p = Part('SM_LL_USATank_Hull')
    # o casco de baixo (entre as esteiras) e o de cima (sobre elas), em perfil de lado
    p.prism_xz('Paint', [(3.6, 0.5), (3.95, 1.15), (3.15, DECK), (-3.85, DECK), (-3.95, 1.45), (-3.95, 0.55), (-3.8, 0.45)],
               -HALF_LOW, HALF_LOW)
    p.prism_xz('Paint', [(3.83, 0.95), (3.95, 1.15), (3.15, DECK), (-3.85, DECK), (-3.95, 1.45), (-3.95, 0.95)],
               -HALF_UP, HALF_UP)
    # as saias: 6 placas por lado (fresta de 3 cm) e a da frente em cunha; a grade do motor atrás
    xs = [3.35, 2.45, 1.55, 0.65, -0.25, -1.15, -2.05]
    for s in (-1, 1):
        y0, y1 = (SKIRT[0], SKIRT[1]) if s > 0 else (-SKIRT[1], -SKIRT[0])
        for i in range(len(xs) - 1):
            p.box('Paint', xs[i + 1] + 0.015, xs[i] - 0.015, y0, y1, 0.62, 1.45)
        p.prism_xz('Paint', [(3.38, 1.45), (3.92, 1.2), (3.6, 0.72), (3.38, 0.62)], y0, y1)       # a da frente
        # a faixa azul segmentada (cor do jogador) nas placas do meio
        for i in (1, 2, 3, 4):
            yo = (SKIRT[1] + 0.006) if s > 0 else -(SKIRT[1] + 0.006)
            p.box('Team', xs[i + 1] + 0.08, xs[i] - 0.08, min(yo, yo - 0.012 * s), max(yo, yo - 0.012 * s), 1.02, 1.2, bevel=0.004)
        # retalho azul em diagonal na placa da frente (a ficha)
        yo = (SKIRT[1] + 0.006) if s > 0 else -(SKIRT[1] + 0.006)
        p.obox('Team', (3.55, yo, 1.0), (0.32, 0.012, 0.42), pitch=-35, bevel=0.004)
        # a caixa da grade do motor (lado de trás) com as aletas
        gy0, gy1 = (1.72, 1.86) if s > 0 else (-1.86, -1.72)
        p.box('Paint', -3.0, -2.08, gy0, gy1, 0.72, 1.5)
        for k in range(10):
            z = 0.8 + k * 0.066
            yo = gy1 + 0.01 if s > 0 else gy0 - 0.01
            p.obox('Dark', (-2.54, yo, z), (0.86, 0.03, 0.035), roll=40 * s, bevel=0.0)
        # a traseira das saias
        p.box('Paint', -3.92, -3.02, y0, y1, 0.8, 1.45)
        # paralamas da frente e de trás (abas de borracha)
        p.box('Dark', 3.7, 3.9, min(y0, y1), max(y0, y1), 0.55, 0.62, bevel=0.005)
    # o deck: grades do motor (aletas), a lona azul amarrada e as escotilhas
    for k in range(9):
        x = -2.55 - k * 0.14
        p.obox('Dark', (x, 0.0, DECK + 0.015), (0.05, 1.9, 0.03), roll=0, pitch=35, bevel=0.0)
    p.box('Paint', -3.85, -2.45, -1.6, -1.05, DECK, DECK + 0.08)
    p.box('Paint', -3.85, -2.45, 1.05, 1.6, DECK, DECK + 0.08)
    # a lona: um fardo baixo com volume e três tiras escuras. É a maior peça na cor do jogador (a ficha
    # usa_mbt_player_color: a lona azul no deck e caindo na traseira); da câmera do jogo (61 m) é ela que lê
    # (teste usa_player_color_game_scale_test: só as faixas finas sumiam)
    p.box('Team', -3.98, -3.9, -0.95, 0.95, 0.9, DECK + 0.1, bevel=0.03)        # a ponta da lona caída na traseira
    p.loft('Team', [[(-2.5, -1.02, DECK + 0.02), (-2.5, 1.02, DECK + 0.02), (-3.85, 1.02, DECK + 0.02), (-3.85, -1.02, DECK + 0.02)],
                     [(-2.47, -1.06, DECK + 0.12), (-2.47, 1.06, DECK + 0.14), (-3.88, 1.06, DECK + 0.11), (-3.88, -1.06, DECK + 0.13)],
                     [(-2.6, -0.92, DECK + 0.24), (-2.6, 0.92, DECK + 0.26), (-3.75, 0.92, DECK + 0.22), (-3.75, -0.92, DECK + 0.25)]],
           bevel=0.04)
    for x in (-2.75, -3.15, -3.6):
        p.box('Dark', x - 0.04, x + 0.04, -1.1, 1.1, DECK + 0.02, DECK + 0.265, bevel=0.005)
    # caixas de ferramentas e de carga no deck, ao lado da torre
    for s in (-1, 1):
        p.box('Paint', -1.9, -0.6, 1.45 * s - 0.2, 1.45 * s + 0.2, DECK, DECK + 0.22) if s > 0 else \
            p.box('Paint', -1.9, -0.6, -1.65, -1.25, DECK, DECK + 0.22)
        p.box('Paint', 2.2, 2.9, 1.35 * s - 0.25, 1.35 * s + 0.25, DECK - 0.02, DECK + 0.1)
    # escotilha do motorista, periscópios, faróis e ganchos
    p.cyl('Paint', (2.75, 0.0, DECK + 0.04), 'z', 0.32, 0.08, seg=20)
    for k in (-0.2, 0.0, 0.2):
        p.obox('Lens', (2.98, k, DECK + 0.07), (0.08, 0.14, 0.06), pitch=20, bevel=0.0)
    for s in (-1, 1):
        p.box('Dark', 3.72, 3.92, 1.45 * s - 0.12, 1.45 * s + 0.12, 1.18, 1.32, bevel=0.01)
        p.obox('Lens', (3.93, 1.45 * s, 1.25), (0.02, 0.18, 0.08), bevel=0.0)
        p.box('Metal', 3.9, 4.05, 0.7 * s - 0.08, 0.7 * s + 0.08, 0.72, 0.86, bevel=0.01)      # ganchos
        p.box('Metal', -4.08, -3.93, 0.8 * s - 0.08, 0.8 * s + 0.08, 0.7, 0.84, bevel=0.01)
        p.box('Dark', -3.97, -3.95, 1.35 * s - 0.1, 1.35 * s + 0.1, 1.25, 1.35, bevel=0.0)     # lanternas
    # a placa de trás com aletas de escape
    for k in range(6):
        p.obox('Dark', (-3.97, 0.0, 0.85 + k * 0.08), (0.03, 1.5, 0.035), roll=0, pitch=-30, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- torre
def turret_plan(dx=0.0, dw=0.0, nose=0.0):
    side = [(2.27 - nose, 0.5 - dw * 0.5), (1.85 - nose * 0.4, 1.25 - dw), (1.34, 1.5 - dw), (-1.56, 1.45 - dw),
            (-2.0, 1.25 - dw), (-2.39 + dx, 1.0 - dw)]
    return side + [(x, -w) for x, w in reversed(side)]


def turret():
    p = Part('SM_LL_USATank_Turret')
    rings = []
    for z, kw in ((1.62, dict(dx=0.12, dw=0.22, nose=0.18)), (1.86, dict()), (2.48, dict(dx=0.06, dw=0.12, nose=0.4))):
        rings.append([(x, y, z) for x, y in turret_plan(**kw)])
    p.loft('Paint', rings, bevel=0.035)
    p.cyl('Dark', (0.1, 0.0, 1.58), 'z', 1.15, 0.08, seg=40, bevel=0.0)                    # o anel da torre
    # os retalhos azuis nas faces de lado, atrás e no teto (cor do jogador)
    for s in (-1, 1):
        p.box('Team', 0.95, 1.12, 1.5 * s - 0.012 if s > 0 else -1.512, 1.5 * s + 0.012 if s > 0 else -1.488, 1.9, 2.42, bevel=0.003)
        p.box('Team', -1.62, -1.45, 1.457 * s - 0.012 if s > 0 else -1.469, 1.457 * s + 0.012 if s > 0 else -1.445, 1.9, 2.42, bevel=0.003)
        p.box('Team', 0.05, 0.62, 1.05 * s - 0.2, 1.05 * s + 0.2, 2.48, 2.495, bevel=0.003)  # o retalho no teto (maior: lê a 61 m)
        # lançadores de defesa ativa: 2×2 caixas na face e atrás
        for (x0, yb) in ((0.75, 1.5), (-1.95, 1.4)):
            for i in range(2):
                for j in range(2):
                    p.box('Paint', x0 + i * 0.34, x0 + i * 0.34 + 0.3, (yb + 0.02) * s if s > 0 else -(yb + 0.3),
                          (yb + 0.3) * s if s > 0 else -(yb + 0.02), 1.92 + j * 0.27, 2.16 + j * 0.27, bevel=0.015)
                    yf = (yb + 0.305) if s > 0 else -(yb + 0.305)
                    p.box('Dark', x0 + i * 0.34 + 0.06, x0 + i * 0.34 + 0.24, yf - 0.004, yf + 0.004,
                          1.97 + j * 0.27, 2.11 + j * 0.27, bevel=0.0)
        # lança-fumaça: 4 tubos curtos em leque na frente da bochecha
        for k in range(4):
            p.cyl('Dark', (1.62 - k * 0.11, 1.42 * s, 2.52), (0.6, 0.55 * s, 0.6), 0.045, 0.22, seg=10)
    # o retalho grande no teto de trás da torre (o teste de escala da cor do jogador, 11/10: na câmera do jogo o que
    # lê é o teto da torre, que olha pra cima; a lona do motor fica baixa e um tanque tapa o do outro na fila)
    p.box('Team', -1.52, -0.92, -0.35, 1.0, 2.48, 2.497, bevel=0.003)
    # o mantelete
    p.box('Paint', 2.15, 2.45, -0.42, 0.42, 1.82, 2.36, bevel=0.03)
    p.cyl('Dark', (2.5, 0.0, 2.08), 'x', 0.21, 0.22, seg=28)
    # a caixa do sensor (estação de armas remota) com a lente azul e a metralhadora
    p.cyl('Paint', (0.25, 0.55, 2.55), 'z', 0.34, 0.14, seg=24)
    p.box('Paint', -0.05, 0.55, 0.3, 0.82, 2.62, 3.02, bevel=0.025)
    p.box('Paint', 0.0, 0.42, 0.33, 0.78, 3.02, 3.18, bevel=0.02)
    p.box('Lens', 0.55, 0.57, 0.38, 0.6, 2.72, 2.9, bevel=0.0)
    p.box('Dark', 0.55, 0.57, 0.64, 0.76, 2.72, 2.86, bevel=0.0)
    p.cyl('Metal', (0.95, 0.55, 2.82), 'x', 0.025, 0.8, seg=10)
    # a mira do atirador
    p.box('Paint', 1.2, 1.62, -0.98, -0.62, 2.48, 2.8, bevel=0.02)
    p.box('Lens', 1.62, 1.64, -0.92, -0.68, 2.55, 2.74, bevel=0.0)
    # a cúpula octogonal do comandante com a escotilha
    p.cyl('Paint', (-0.45, -0.55, 2.56), 'z', 0.48, 0.16, seg=8, bevel=0.015)
    p.cyl('Paint', (-0.45, -0.55, 2.66), 'z', 0.36, 0.06, seg=24)
    for k in range(6):
        a = math.radians(k * 60 + 30)
        p.obox('Lens', (-0.45 + 0.47 * math.cos(a), -0.55 + 0.47 * math.sin(a), 2.58), (0.03, 0.14, 0.05),
               yaw=k * 60 + 30, bevel=0.0)
    # a escotilha do municiador e as tampas de ventilação
    p.cyl('Paint', (-0.6, 0.55, 2.51), 'z', 0.32, 0.05, seg=24)
    p.cyl('Paint', (-1.2, -0.05, 2.5), 'z', 0.12, 0.05, seg=14)
    p.box('Paint', 0.6, 0.95, -0.25, 0.15, 2.48, 2.53, bevel=0.01)
    # o mastro do sensor e as antenas
    p.cyl('Paint', (-1.65, -0.85, 2.85), 'z', 0.08, 0.75, seg=12)
    p.cyl('Paint', (-1.65, -0.85, 3.55), 'z', 0.055, 0.7, seg=12)
    p.box('Paint', -1.78, -1.52, -0.98, -0.72, 3.9, 4.15, bevel=0.015)
    p.cyl('Lens', (-1.51, -0.85, 4.02), 'x', 0.06, 0.02, seg=12)
    for (x, y, hgt) in ((-2.05, 0.95, 1.5), (-2.05, -0.55, 1.3), (-1.0, 0.95, 1.1)):
        p.cyl('Metal', (x, y, 2.48 + hgt / 2), 'z', 0.012, hgt, seg=6)
        p.cyl('Dark', (x, y, 2.5), 'z', 0.04, 0.06, seg=10)
    # o cesto atrás (tubos) com a carga (caixas e sacos)
    for s in (-1, 1):
        p.box('Metal', -2.62, -2.58, 0.98 * s - 0.02, 0.98 * s + 0.02, 1.8, 2.3, bevel=0.0)
    for z in (1.85, 2.08, 2.3):
        p.box('Metal', -2.62, -2.58, -1.0, 1.0, z - 0.02, z + 0.02, bevel=0.0)
        for s in (-1, 1):
            p.box('Metal', -2.62, -2.3, 1.0 * s - 0.02, 1.0 * s + 0.02, z - 0.02, z + 0.02, bevel=0.0)
    p.box('Paint', -2.56, -2.3, -0.85, -0.2, 1.75, 2.12, bevel=0.02)
    p.box('Team', -2.56, -2.32, 0.0, 0.7, 1.75, 2.0, bevel=0.06)          # o fardo do cesto, na cor do jogador
    p.box('Dark', -2.56, -2.34, -0.15, 0.0, 1.75, 1.98, bevel=0.03)
    return p.finish()


# ---------------------------------------------------------------------------------- cano
def barrel():
    p = Part('SM_LL_USATank_Barrel')
    p.cyl('Paint', (3.3, 0.0, 2.08), 'x', 0.13, 1.4, seg=28)         # a luva térmica de trás
    p.cyl('Paint', (4.6, 0.0, 2.08), 'x', 0.11, 1.4, seg=28)         # a da frente
    p.cyl('Paint', (3.75, 0.0, 2.08), 'x', 0.165, 0.5, seg=28)       # o evacuador
    for x in (2.75, 3.4, 4.05, 4.65, 5.25):                          # as braçadeiras
        p.cyl('Metal', (x, 0.0, 2.08), 'x', 0.125 if x > 4.0 else 0.145, 0.05, seg=28, bevel=0.005)
    p.cyl('Paint', (5.45, 0.0, 2.08), 'x', 0.1, 0.38, seg=28)
    p.cyl('Dark', (5.62, 0.0, 2.08), 'x', 0.11, 0.06, seg=28)        # a boca
    p.cyl('Dark', (5.64, 0.0, 2.08), 'x', 0.065, 0.02, seg=20)
    p.box('Metal', 5.5, 5.58, -0.03, 0.03, 2.18, 2.26, bevel=0.005)   # a referência da boca
    for x, w in ((4.15, 0.09), (5.05, 0.05), (5.15, 0.05)):           # os anéis azuis (cor do jogador)
        p.cyl('Team', (x, 0.0, 2.08), 'x', 0.118, w, seg=28, bevel=0.003)
    return p.finish()


# ---------------------------------------------------------------------------------- rodagem
def wheels():
    p = Part('SM_LL_USATank_Wheels')
    for s in (-1, 1):
        y = TRACK_Y * s
        for x in WHEELS_X:
            for dy in (-0.15, 0.15):
                p.cyl('Paint', (x, y + dy, WHEEL_Z), 'y', WHEEL_R, 0.2, seg=28, bevel=0.02)
                p.cyl('Dark', (x, y + dy + 0.105 * (1 if dy > 0 else -1), WHEEL_Z), 'y', WHEEL_R - 0.03, 0.02, seg=28, bevel=0.0)
            p.cyl('Metal', (x, y + 0.27 * s, WHEEL_Z), 'y', 0.09, 0.08, seg=12)
        # roda-guia (frente) e motriz (atrás, com dentes)
        p.cyl('Paint', (3.53, y, 0.7), 'y', 0.27, 0.5, seg=28, bevel=0.02)
        p.cyl('Metal', (-3.48, y, 0.6), 'y', 0.24, 0.5, seg=24, bevel=0.01)
        for k in range(12):
            a = math.radians(k * 30)
            p.obox('Metal', (-3.48 + 0.29 * math.cos(a), y, 0.6 + 0.29 * math.sin(a)), (0.09, 0.42, 0.07),
                   pitch=-math.degrees(a), bevel=0.0)
        for x in (1.3, -0.5, -2.0):                                     # roletes de cima
            p.cyl('Dark', (x, y, 0.98), 'y', 0.1, 0.3, seg=14)
    return p.finish()


def track_path():
    """O contorno da esteira: o envoltório convexo das rodas (círculos com a espessura do elo)."""
    pts = []
    circles = [(-3.48, 0.6, 0.36), (3.53, 0.7, 0.33)] + [(x, WHEEL_Z, WHEEL_R + 0.06) for x in WHEELS_X] + \
              [(x, 0.98, 0.16) for x in (1.3, -0.5, -2.0)]
    for cx, cz, r in circles:
        for k in range(64):
            a = 2 * math.pi * k / 64
            pts.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    pts = sorted(set(pts))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for q in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    hull_ = lower[:-1] + upper[:-1]
    # reamostra a cada 17 cm (o elo)
    seg = list(zip(hull_, hull_[1:] + hull_[:1]))
    total = sum(math.dist(a, b) for a, b in seg)
    n = int(total / 0.17)
    step = total / n
    out, acc, i = [], 0.0, 0
    t = 0.0
    for a, b in seg:
        L = math.dist(a, b)
        while t <= L:
            u = t / L
            out.append(((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u), math.atan2(b[1] - a[1], b[0] - a[0])))
            t += step
        t -= L
    return out


def track(side):
    s = 1 if side == 'r' else -1
    p = Part(f'SM_LL_USATank_Track_{side}')
    y = TRACK_Y * s
    for (x, z), ang in track_path():
        deg = math.degrees(ang)
        # a normal pra fora do contorno (o elo é mais grosso do lado de fora: a garra)
        nx, nz = math.sin(ang), -math.cos(ang)
        p.obox('Track', (x, y, z), (0.15, TRACK_W, 0.05), pitch=deg, bevel=0.008)
        p.obox('Track', (x + nx * 0.035, y, z + nz * 0.035), (0.04, TRACK_W * 0.92, 0.03), pitch=deg, bevel=0.004)
        p.obox('Metal', (x - nx * 0.04, y, z - nz * 0.04), (0.08, 0.08, 0.05), pitch=deg, bevel=0.005)   # a guia
    return p.finish()


# ---------------------------------------------------------------------------------- carcaça
def wreck():
    """O carro queimado das carcaças da noite (uma picape): chassi, cabine amassada sem vidro,
    caçamba aberta, rodas sem pneu, tudo carbonizado com manchas de ferrugem."""
    p = Part('SM_LL_Wreck')
    p.box('Char', -2.4, 2.4, -0.95, 0.95, 0.35, 0.95, bevel=0.04)                  # o chassi
    p.prism_xz('Char', [(2.4, 0.95), (2.45, 0.75), (1.5, 0.95)], -0.95, 0.95)       # o capô baixo
    p.loft('Char', [[(1.45, -0.9, 0.95), (1.45, 0.9, 0.95), (-0.25, 0.9, 0.95), (-0.25, -0.9, 0.95)],
                    [(0.95, -0.78, 1.62), (0.9, 0.82, 1.55), (-0.2, 0.82, 1.6), (-0.2, -0.78, 1.66)]], bevel=0.03)
    for s_ in (-1, 1):                                                              # as laterais da caçamba, tortas
        p.obox('Rust', (-1.3, 0.9 * s_, 1.15), (2.0, 0.06, 0.42), roll=8 * s_, bevel=0.01)
    p.obox('Rust', (-2.35, 0.0, 1.05), (0.06, 1.8, 0.35), pitch=-20, bevel=0.01)
    p.obox('Char', (0.55, 0.0, 1.0), (0.9, 1.7, 0.04), pitch=-12, bevel=0.0)         # o teto caído
    for x in (1.55, -1.55):
        for s_ in (-1, 1):
            p.cyl('Metal', (x, 0.92 * s_, 0.36), 'y', 0.3, 0.16, seg=16, bevel=0.01)
            p.cyl('Dark', (x, 0.98 * s_, 0.36), 'y', 0.18, 0.04, seg=12, bevel=0.0)
    p.obox('Rust', (1.9, 0.6, 1.0), (0.5, 0.4, 0.05), yaw=25, pitch=-30, bevel=0.0)   # lataria solta
    p.obox('Char', (-0.8, -1.4, 0.05), (0.9, 0.5, 0.05), yaw=40, bevel=0.0)
    return p.finish()


# ---------------------------------------------------------------------------------- picape do GLA
def pickup(armed=True):
    """A picape técnica do GLA da arte (day_game_cam): cabine dupla curta, caçamba com a metralhadora
    no pedestal apontando por cima da cabine, para-choque de cano, rodas grandes. 5,6 m, frente em +X.
    armed=False: a picape civil do cenário (sem a arma, a blindagem e a munição: a técnica parada na vila
    parecia unidade inimiga vista de cima; pedido do WorldForge, 11/10)."""
    p = Part('SM_LL_Pickup' if armed else 'SM_LL_PickupCivil')
    p.box('Paint', -2.75, 2.75, -0.92, 0.92, 0.55, 0.95, bevel=0.05)                    # a carroceria (até a cintura)
    p.prism_xz('Paint', [(2.75, 0.95), (2.85, 0.62), (2.75, 0.55), (1.35, 0.55), (1.35, 1.12), (2.6, 1.02)],
               -0.9, 0.9, bevel=0.04)                                                    # o capô caindo pra frente
    p.loft('Paint', [[(1.35, -0.9, 1.1), (1.35, 0.9, 1.1), (-0.55, 0.9, 1.1), (-0.55, -0.9, 1.1)],
                     [(0.75, -0.8, 1.82), (0.75, 0.8, 1.82), (-0.5, 0.8, 1.84), (-0.5, -0.8, 1.84)]], bevel=0.04)   # a cabine
    # os vidros (escuros, um pouco pra fora da lata): para-brisa, laterais e o de trás
    p.prism_xz('Lens', [(1.3, 1.16), (0.8, 1.76), (0.72, 1.76), (1.22, 1.16)], -0.74, 0.74, bevel=0.0)
    for s_ in (-1, 1):
        p.box('Lens', -0.35, 0.95, 0.885 * s_ - 0.02, 0.885 * s_ + 0.02, 1.2, 1.68, bevel=0.0)
    p.box('Lens', -0.6, -0.56, -0.65, 0.65, 1.25, 1.7, bevel=0.0)
    # a caçamba: o piso, as laterais e a tampa, com a borda de cima
    p.box('Paint', -2.75, -0.6, -0.92, 0.92, 0.95, 1.0, bevel=0.01)
    for s_ in (-1, 1):
        p.box('Paint', -2.75, -0.6, 0.86 * s_ - 0.05, 0.86 * s_ + 0.05, 1.0, 1.42, bevel=0.015)
    p.box('Paint', -2.8, -2.7, -0.9, 0.9, 0.95, 1.4, bevel=0.015)
    # para-choque de cano e o quebra-mato
    p.box('Dark', 2.82, 3.0, -0.95, 0.95, 0.45, 0.62, bevel=0.02)
    for s_ in (-0.55, 0.55):
        p.box('Dark', 2.9, 2.98, s_ - 0.04, s_ + 0.04, 0.62, 1.05, bevel=0.0)
    p.box('Dark', 2.9, 2.98, -0.6, 0.6, 1.0, 1.06, bevel=0.0)
    p.box('Dark', -2.95, -2.78, -0.9, 0.9, 0.48, 0.6, bevel=0.02)
    for s_ in (-0.6, 0.6):                                                               # faróis e lanternas
        p.box('Lens', 2.8, 2.87, s_ - 0.17, s_ + 0.17, 0.78, 0.9, bevel=0.0)
        p.box('Lens', -2.82, -2.77, s_ - 0.1, s_ + 0.1, 1.05, 1.25, bevel=0.0)
    # as rodas: pneu grosso e aro
    for x in (1.75, -1.75):
        for s_ in (-1, 1):
            p.cyl('Track', (x, 0.86 * s_, 0.45), 'y', 0.45, 0.32, seg=24, bevel=0.02)
            p.cyl('Metal', (x, 1.02 * s_, 0.45), 'y', 0.24, 0.04, seg=16, bevel=0.0)
    if not armed:
        p.box('Cloth', -2.55, -0.8, -0.8, 0.8, 1.0, 1.35, bevel=0.06)     # a carga da caçamba coberta com lona
        p.box('Dark', -1.9, -1.3, 0.2, 0.75, 1.35, 1.6, bevel=0.02)        # e umas caixas por cima
        return p.finish()
    # a metralhadora no pedestal da caçamba, a blindagem e a caixa de munição
    p.cyl('Dark', (-1.4, 0.0, 1.45), 'z', 0.07, 1.0, seg=12, bevel=0.0)
    p.box('Dark', -1.62, -1.18, -0.16, 0.16, 1.9, 2.12, bevel=0.02)
    p.cyl('Metal', (-0.55, 0.0, 2.05), 'x', 0.045, 1.4, seg=10, bevel=0.0)
    p.cyl('Dark', (-1.0, 0.0, 2.04), 'x', 0.09, 0.5, seg=12, bevel=0.0)
    p.obox('Paint', (-1.05, 0.0, 2.1), (0.06, 0.8, 0.5), pitch=10, bevel=0.01)
    p.box('Dark', -1.6, -1.25, 0.2, 0.45, 2.0, 2.18, bevel=0.01)
    p.box('Dark', -2.4, -1.9, -0.7, -0.2, 1.0, 1.3, bevel=0.02)                        # caixas na caçamba
    p.box('Cloth', -2.5, -1.9, 0.15, 0.75, 1.0, 1.2, bevel=0.03)
    p.cyl('Track', (-2.2, 0.0, 1.38), 'z', 0.38, 0.22, seg=20, bevel=0.01)               # o estepe deitado
    return p.finish()


# ---------------------------------------------------------------------------------- miudezas
# O que a arte espalha em volta das casas (caixotes, tambores, sacos de areia, caixas de munição):
# os slots usam os materiais das casas (Wood, Metal, Cloth, Dark) na Unreal.
def crate(p, c, size, yaw=0.0):
    """Um caixote de tábuas: a caixa, as ripas das bordas um pouco pra fora e a tampa."""
    x, y, z = c
    sx, sy, sz = size
    p.obox('Wood', (x, y, z + sz / 2), (sx, sy, sz), yaw=yaw, bevel=0.015)
    for dz in (0.06, sz - 0.06):
        p.obox('Wood', (x, y, z + dz), (sx + 0.03, sy + 0.03, 0.08), yaw=yaw, bevel=0.008)


def props():
    out = []
    p = Part('SM_LL_Prop_Crates')           # a pilha de caixotes da frente da H1
    COLORS.update({'Wood': (118, 84, 52)})
    crate(p, (0.0, 0.0, 0.0), (1.2, 0.85, 0.75))
    crate(p, (1.3, 0.1, 0.0), (1.0, 0.8, 0.7), yaw=6)
    crate(p, (0.6, 0.0, 0.75), (1.1, 0.8, 0.6), yaw=-4)
    crate(p, (0.3, 1.1, 0.0), (0.8, 0.8, 0.55), yaw=20)
    out.append(p.finish())
    p = Part('SM_LL_Prop_Barrels')           # tambores de 200 l, um deitado
    COLORS.update({'Metal': (95, 95, 92), 'Dark': (40, 40, 38)})
    for (x, y) in ((0.0, 0.0), (0.66, 0.1), (0.3, 0.6)):
        p.cyl('Metal', (x, y, 0.45), 'z', 0.29, 0.9, seg=20, bevel=0.01)
        for z in (0.2, 0.7):
            p.cyl('Dark', (x, y, z), 'z', 0.3, 0.04, seg=20, bevel=0.0)
    p.cyl('Metal', (1.2, -0.4, 0.29), 'x', 0.29, 0.9, seg=20, bevel=0.01)
    out.append(p.finish())
    p = Part('SM_LL_Prop_Sandbags')          # a mureta de sacos de areia em arco, três fiadas
    COLORS.update({'Cloth': (150, 128, 92)})
    for row in range(3):
        n = 7 - row
        for i in range(n):
            a = math.radians(-50 + 100 * (i + 0.5 * row) / 7)
            r = 2.0
            p.obox('Cloth', (r * math.sin(a), r * math.cos(a) - r, 0.1 + row * 0.19), (0.62, 0.34, 0.2),
                   yaw=-math.degrees(a), bevel=0.06)
    out.append(p.finish())
    p = Part('SM_LL_Prop_Ammo')              # caixas de munição verde-escuras empilhadas
    for i, (x, y, z) in enumerate(((0, 0, 0), (0.55, 0, 0), (0.25, 0.02, 0.32), (0.0, 0.45, 0))):
        p.obox('Dark', (x, y, z + 0.16), (0.5, 0.3, 0.32), yaw=3 * i, bevel=0.02)
        p.obox('Metal', (x, y, z + 0.33), (0.2, 0.06, 0.03), yaw=3 * i, bevel=0.0)
    out.append(p.finish())
    return out


# ---------------------------------------------------------------------------------- dozer
def dozer():
    """O Dozer da referência de perto (close_ground_light_gemini), só pra compor a cena e a sombra (é rascunho
    na arte, não é alvo de modelo): esteiras, capô, cabine com grade, lâmina amarela nos braços, ripper atrás
    e a lona azul do lado. ~7,4 m da lâmina ao ripper, frente em +X."""
    COLORS.update({'Blade': (196, 160, 40)})
    p = Part('SM_LL_Dozer')
    for s_ in (-1, 1):                                                              # as esteiras e as rodas
        y0, y1 = (1.05, 1.75) if s_ > 0 else (-1.75, -1.05)
        p.box('Track', -1.9, 1.9, y0, y1, 0.0, 1.0, bevel=0.05)
        for x in (-1.9, 1.9):
            p.cyl('Track', (x, (y0 + y1) / 2, 0.5), 'y', 0.5, y1 - y0, seg=20, bevel=0.02)
        p.cyl('Metal', (-1.9, 1.78 * s_, 0.5), 'y', 0.36, 0.06, seg=16, bevel=0.0)
        p.cyl('Metal', (1.9, 1.78 * s_, 0.5), 'y', 0.3, 0.06, seg=16, bevel=0.0)
        for x in (-1.0, -0.3, 0.4, 1.1):
            p.cyl('Metal', (x, 1.78 * s_, 0.32), 'y', 0.2, 0.05, seg=12, bevel=0.0)
        p.box('Blade', -1.6, 3.0, 0.9 * s_ - 0.12, 0.9 * s_ + 0.12, 0.55, 0.85, bevel=0.03)     # o braço da lâmina
    p.box('Paint', -0.3, 2.1, -1.0, 1.0, 1.0, 2.05, bevel=0.06)                   # o capô
    p.box('Dark', 2.1, 2.16, -0.8, 0.8, 1.1, 1.9, bevel=0.0)                      # a grade do radiador
    p.cyl('Dark', (1.3, 0.45, 2.55), 'z', 0.1, 1.0, seg=12, bevel=0.0)             # o escapamento
    p.box('Paint', -2.0, -0.2, -1.15, 1.15, 1.0, 3.25, bevel=0.06)                # a cabine
    p.box('Paint', -2.1, -0.1, -1.25, 1.25, 3.25, 3.4, bevel=0.03)                # o teto
    p.box('Lens', -0.18, -0.15, -0.85, 0.85, 2.2, 3.0, bevel=0.0)                 # os vidros
    for s_ in (-1, 1):
        p.box('Lens', -1.6, -0.5, 1.17 * s_ - 0.02, 1.17 * s_ + 0.02, 2.3, 3.0, bevel=0.0)
        for k in range(9):                                                           # a grade de proteção da cabine
            x = -1.95 + k * 0.22
            p.box('Dark', x, x + 0.04, 1.3 * s_ - 0.02, 1.3 * s_ + 0.02, 2.0, 3.3, bevel=0.0)
    p.box('Team', -1.7, -0.6, 1.17, 1.4, 1.2, 2.0, bevel=0.06)                    # a lona azul do lado
    # a lâmina curva (três faixas inclinadas) com as laterais, e os cilindros
    for k, (z0, z1, x0, tilt) in enumerate(((0.05, 0.6, 3.05, 15), (0.6, 1.2, 3.12, 0), (1.2, 1.75, 3.05, -18))):
        p.obox('Blade', (x0, 0.0, (z0 + z1) / 2), (0.12, 4.0, z1 - z0 + 0.04), pitch=tilt, bevel=0.02)
    for s_ in (-1, 1):
        p.box('Blade', 2.75, 3.15, 1.98 * s_ - 0.04, 1.98 * s_ + 0.04, 0.05, 1.75, bevel=0.01)
        p.cyl('Metal', (2.3, 0.75 * s_, 1.6), (0.9, 0.0, -0.25), 0.07, 1.2, seg=10, bevel=0.0)
    # o ripper atrás: o quadro e o dente
    p.box('Blade', -2.7, -2.1, -0.8, 0.8, 0.9, 1.25, bevel=0.03)
    p.obox('Blade', (-2.75, 0.0, 0.5), (0.18, 0.18, 1.1), pitch=-20, bevel=0.02)
    return p.finish()


# ---------------------------------------------------------------------------------- paraquedas
def chute():
    """O paraquedas do sinalizador da noite: a cúpula de 16 gomos (o pano estufa entre as costuras e a
    barra faz o recorte de vieira), as cordas até o sinalizador 6 m abaixo e a lata dele.
    Origem no sinalizador; a barra da cúpula a 6 m, 3 m de raio (medido na arte: ~6 m de diâmetro)."""
    N, R, DOME, DROP = 16, 3.0, 1.7, 6.0
    COLORS.update({'Chute': (235, 235, 230), 'Cord': (200, 200, 195)})
    b = bmesh.new()
    rings = []
    steps = 12
    for i in range(steps + 1):
        ph = 0.12 + (math.pi / 2 - 0.12) * i / steps                    # do furo do topo até a barra
        ring = []
        for k in range(N * 4):
            th = 2 * math.pi * k / (N * 4)
            fr = (k % 4) / 4.0                                          # 0 na costura, 0,5 no meio do gomo
            bulge = 1 + 0.06 * math.sin(math.pi * fr) * math.sin(ph)   # o pano estufa entre as costuras
            r = R * math.sin(ph) * bulge
            z = DROP + DOME * math.cos(ph)
            if i == steps:
                z += 0.22 * (1 - math.sin(math.pi * fr))                # a vieira da barra (a costura levanta)
            ring.append(b.verts.new(V(r * math.cos(th), r * math.sin(th), z)))
        rings.append(ring)
    n = N * 4
    b.faces.new(list(reversed(rings[0])))                               # o topo fechado (o furo é pequeno)
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for k in range(n):
            j = (k + 1) % n
            b.faces.new([r0[k], r0[j], r1[j], r1[k]])
    bmesh.ops.recalc_face_normals(b, faces=b.faces)
    me = bpy.data.meshes.new('canopy')
    b.to_mesh(me)
    b.free()
    for poly in me.polygons:
        poly.use_smooth = True
    canopy = bpy.data.objects.new('canopy', me)
    bpy.context.scene.collection.objects.link(canopy)
    canopy.data.materials.append(mat('Chute'))
    # as cordas (uma por costura, metade delas: lê melhor de longe) e a lata do sinalizador
    p = Part('cords')
    for k in range(0, N, 2):
        th = 2 * math.pi * k / N
        top = Vector((R * math.cos(th), R * math.sin(th), DROP + 0.22))
        d = top - Vector((0, 0, 0.2))
        mid = (top + Vector((0, 0, 0.2))) / 2
        p.cyl('Cord', (mid.x, mid.y, mid.z), (d.x, d.y, d.z), 0.022, d.length, seg=6, bevel=0.0)
    p.cyl('Dark', (0, 0, 0.0), 'z', 0.07, 0.4, seg=12, bevel=0.0)
    cords = p.finish()
    select_only(canopy)
    cords.select_set(True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = 'SM_LL_Chute'
    return o


def export(o):
    select_only(o)
    path = os.path.join(OUT, o.name + '.glb')
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_normals=True, export_texcoords=False)
    print('LOOKTANK', o.name, len(o.data.polygons), 'faces ->', path)


def main():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    only = os.environ.get('LOOK_TANK_ONLY')     # ex.: "props" pra refazer só as miudezas
    builds = {'hull': hull, 'turret': turret, 'barrel': barrel, 'wheels': wheels, 'track_l': lambda: track('l'),
              'track_r': lambda: track('r'), 'wreck': wreck, 'chute': chute, 'pickup': pickup, 'props': props,
              'dozer': dozer, 'pickup_civil': lambda: pickup(armed=False)}
    for key, build in builds.items():
        if only and key not in only.split(','):
            continue
        res = build()
        for o in (res if isinstance(res, list) else [res]):
            export(o)
            bpy.data.objects.remove(o, do_unlink=True)


main()
