"""O kit provisório dos monólitos de arenito (AlUla/Hegra), em malha (fase 2 do relevo, 13/10).

Rodar (CPU, sem placa):
  "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" -b -P WorldForge/Tools/blender_monoliths.py -- <pasta> [preview]

Por que malha e não relevo: o pilar no heightfield saía tambor de bolo (parede reta, topo chato, mais
largo que alto); o pilar de verdade tem pescoço, lábio em cada camada dura, capa mais larga que o
pescoço e arco, e nada disso cabe num heightfield (Paris et al. 2019, "Terrain Amplification with
Implicit 3D Features"; Peytavie et al. 2009, "Arches"). Os jogos grandes fazem o marco em malha e o
procedimento só posiciona (Horizon, GDC 2017; Valley of the Ancient da Epic; a 2ª opinião de 12/10).

A forma (o hoodoo: a capa dura protege a coluna mole embaixo):
- a planta de cada camada é um polígono de juntas verticais (retas sorteadas tangentes ao contorno
  base, as mesmas ao longo da altura: as juntas do arenito descem retas) com o contorno arredondado;
- o raio sobe e desce pela altura: pé largo, pescoço a ~30%, corpo, capa mais larga e quebrada;
- as camadas (2–4 m): a dura sai como lábio de ~0,3–0,8 m com quina viva, a mole recua; é o que a
  câmera de RTS lê de longe (a sombra de cada lábio);
- sulcos verticais (a erosão da chuva) em algumas juntas;
- quina dura: sombreado plano nas faces viradas mais de 35°, nada de remesh.

Saída: SM_Mono_<nome>.glb (metros, Z pra cima, o pé em z = 0 no centro da planta) e monoliths.json com
a pegada de cada um (o raio da planta a 1 m de altura em 32 direções) e a altura, que o gerador usa pra
pôr no mapa sem sobrepor e pra máscara de passagem.
"""
import json
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector

ARGV = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = os.path.abspath(ARGV[0] if ARGV else os.path.join(os.getcwd(), 'captures', 'worldforge', 'monoliths'))
PREVIEW = len(ARGV) > 1 and ARGV[1] == 'preview'
os.makedirs(OUT, exist_ok=True)

# (nome, semente, altura m, raio base m, alongamento da planta, forma)
# (a arte: o pilar tem ~2–2,5× a altura da largura; o 1º kit saiu largo, 13/10)
KIT = [
    ('Pillar_A', 11, 30.0, 6.5, 1.25, 'hoodoo'),
    ('Pillar_B', 23, 26.0, 5.8, 1.5, 'hoodoo'),
    ('Pillar_C', 37, 34.0, 7.0, 1.15, 'mushroom'),
    ('Pillar_D', 41, 22.0, 5.5, 1.7, 'hoodoo'),
    ('Pillar_E', 53, 28.0, 7.5, 1.35, 'block'),
    ('Pillar_F', 67, 24.0, 6.8, 1.6, 'mushroom'),
    ('Marker', 79, 42.0, 22.0, 1.3, 'dome'),
    # o arco natural (a arte do AlUla tem um): a lâmina comprida com o túnel em elipse; a passagem embaixo fica
    # fechada no jogo (a pegada é a da lâmina inteira)
    ('Arch', 91, 24.0, 7.0, 3.2, 'arch'),
]
SEG = 120          # direções da planta
DZ = 0.35          # passo de altura (m)


def smooth01(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


class Shape:
    """O raio da planta r(θ, z) de uma peça: contorno base por juntas + perfil + camadas + sulcos."""

    def __init__(self, seed, height, radius, elong, kind):
        self.r = random.Random(seed)
        self.h, self.rad, self.el, self.kind = height, radius, elong, kind
        # juntas verticais: 7–11 retas (ângulo da normal, distância), o contorno é o polígono delas
        # arredondado (as juntas do arenito são retas e descem a peça inteira)
        n = self.r.randint(7, 11)
        base = [i * 2 * math.pi / n + self.r.uniform(-0.25, 0.25) for i in range(n)]
        self.joints = [(a, self.r.uniform(0.86, 1.0)) for a in base]
        self.yaw = self.r.uniform(0, math.pi)
        # sulcos: em 2–4 juntas, uma fenda vertical estreita
        self.gullies = [(self.r.choice(base) + self.r.uniform(-0.12, 0.12), self.r.uniform(0.06, 0.12), self.r.uniform(0.5, 1.4))
                        for _ in range(self.r.randint(2, 4))]
        # camadas: espessuras de 1,6–4 m, a dura (lábio) e a mole alternadas
        self.layers = []
        z = 0.0
        hard = self.r.random() < 0.5
        while z < self.h + 4.0:
            t = self.r.uniform(1.6, 4.0) if hard else self.r.uniform(1.0, 2.6)
            self.layers.append((z, z + t, hard, self.r.uniform(0.35, 0.8)))
            z += t
            hard = not hard
        self.noise = [self.r.uniform(-1, 1) for _ in range(64)]
        self.grid = [[self.r.uniform(-1, 1) for _ in range(64)] for _ in range(24)]   # ruído macio (ângulo × altura)
        self.cap_h = [self.r.uniform(-1, 1) for _ in range(12)]                        # o topo irregular

    def soft(self, th, z, per=12, zs=3.0):
        # ruído de valor periódico no ângulo (per células na volta) e em z (zs m por célula)
        u = (th % (2 * math.pi)) / (2 * math.pi) * per
        v = z / zs
        i0, j0 = int(u) % per, int(v)
        fu, fv = u - int(u), v - int(v)
        fu, fv = fu * fu * (3 - 2 * fu), fv * fv * (3 - 2 * fv)
        g = self.grid
        def at(i, j):
            return g[i % 24][j % 64]
        i1 = (i0 + 1) % per
        a, b = at(i0, j0), at(i1, j0)
        c, d = at(i0, j0 + 1), at(i1, j0 + 1)
        return (a + (b - a) * fu) * (1 - fv) + (c + (d - c) * fu) * fv

    def plan(self, th):
        """O contorno base (0..1 + pouco): a interseção das juntas, arredondada."""
        d = 1e9
        for a, k in self.joints:
            c = math.cos(th - a)
            if c > 0.2:
                d = min(d, k / c)
        d = min(d, 1.25)
        # o alongamento (oval) girado
        u = math.cos(th - self.yaw) * self.el
        v = math.sin(th - self.yaw)
        return d / math.sqrt(u * u + v * v) * self.el ** 0.5

    def profile(self, z):
        """O raio relativo pela altura."""
        t = z / self.h
        if self.kind == 'hoodoo':
            # pé largo, pescoço a 25–35%, corpo, capa mais larga no topo
            foot = 1.0 + 0.35 * (1.0 - smooth01(t / 0.18))
            neck = 1.0 - 0.22 * math.exp(-((t - 0.3) / 0.12) ** 2)
            cap = 1.0 + 0.12 * smooth01((t - 0.82) / 0.08)
            return foot * neck * cap * (1.0 - 0.1 * t)
        if self.kind == 'mushroom':
            # pescoço baixo e a cabeça larga (o cogumelo de arenito): a capa protege
            foot = 1.0 + 0.3 * (1.0 - smooth01(t / 0.15))
            neck = 1.0 - 0.28 * math.exp(-((t - 0.42) / 0.18) ** 2)
            head = 1.0 + 0.18 * math.exp(-((t - 0.8) / 0.12) ** 2)
            return foot * neck * head
        if self.kind in ('block', 'arch'):
            # bloco de mesa: parede quase reta com pé e o topo afinando nas quinas
            return (1.0 + 0.3 * (1.0 - smooth01(t / 0.15))) * (1.0 - 0.06 * t)
        # o marco: domo largo (Qasr al-Farid): parede alta e o topo arredondado
        return (1.0 + 0.2 * (1.0 - smooth01(t / 0.12))) * math.sqrt(max(0.0, 1.0 - max(0.0, t - 0.55) ** 2 / 0.45 ** 2))

    def layer_off(self, z):
        """O lábio da camada dura (m, pra fora) e o recuo da mole, com quina viva (0,15 m de chanfro)."""
        for z0, z1, hard, lip in self.layers:
            if z0 <= z < z1:
                e = min(z - z0, z1 - z) / 0.15
                k = smooth01(e)
                return (lip if hard else -0.5 * lip) * k
        return 0.0

    def top(self, th):
        """A altura do topo nessa direção: quebrado em blocos (a capa que caiu em pedaços)."""
        if self.kind == 'dome':
            return self.h
        # o topo varia macio com a direção (degrau por setor fazia vinco em cada emenda)
        u = (th % (2 * math.pi)) / (2 * math.pi) * 6
        i = int(u) % 6
        f = smooth01(u - int(u))
        a = self.cap_h[i] * (1 - f) + self.cap_h[(i + 1) % 6] * f
        return self.h * (1.0 + 0.07 * a)

    def shoulder(self, th, z):
        """O ombro arredondado do topo: o raio cai nos últimos ~3–5 m (o arenito arredonda a quina de cima)."""
        if self.kind == 'dome':
            return 1.0
        zt = self.top(th)
        w = 4.0
        d = max(0.0, z - (zt - w)) / w
        return math.sqrt(max(0.0, 1.0 - d * d))

    def radius(self, th, z):
        # a camada marca mais numas faces que noutras (onde o vento bate a dura sai; no abrigo fica lisa)
        strata = 0.25 + 0.75 * smooth01(0.5 + 0.9 * self.soft(th + 2.1, 0.0, 6, 100.0))
        # perto do topo a planta de juntas retas arredonda (as quinas das juntas fechavam em vincos no alto)
        near_top = smooth01((z - (self.top(th) - 5.0)) / 5.0) if self.kind != 'dome' else 0.0
        plan = self.plan(th) * (1.0 - near_top) + 0.95 * near_top
        dome = self.kind == 'dome'
        # o marco: camada fraca e calombo grande (com a camada cheia o domo virava bolo listrado, 13/10)
        r = self.rad * plan * self.profile(z) + self.layer_off(z) * strata * (1.0 - 0.7 * near_top) * (0.35 if dome else 1.0)
        # calombos grandes (o arenito desmancha em massas arredondadas de 5–8 m; no marco, de 10–14 m)
        r += (2.6 * self.soft(th + 0.7, z, 7, 12.0) if dome else 1.1 * self.soft(th + 0.7, z, 5, 7.0)) * smooth01(z / 4.0)
        # as fendas verticais fundas (as juntas abertas pela chuva): racham a peça em colunas e somem no
        # ombro do topo (subindo até o alto o pilar virava banana descascada)
        fade_top = 1.0 - smooth01((z - (self.top(th) - 6.0)) / 4.0) if self.kind != 'dome' else 1.0
        for a, w, depth in self.gullies:
            dd = math.atan2(math.sin(th - a), math.cos(th - a))
            r -= depth * 2.2 * max(0.0, 1.0 - abs(dd) / w) ** 1.5 * smooth01(z / 3.0) * fade_top
        # o inchaço macio da pedra (2,5–3 m), não o recorte fino (o fino virava papelão ondulado)
        r += 0.7 * self.soft(th, z, 12, 3.0) + 0.3 * self.soft(th + 1.3, z, 24, 1.2)
        r *= self.shoulder(th, z)
        return max(0.3, r)


def build(name, seed, height, radius, elong, kind):
    s = Shape(seed, height, radius, elong, kind)
    bm = bmesh.new()
    rings = []
    # cada direção sobe até o próprio topo (z normalizado por direção): o topo fecha arredondado, sem a
    # estrela de vincos do leque (o anel parava na altura mais baixa do topo)
    tops = [s.top(i * 2 * math.pi / SEG) for i in range(SEG)]
    nz = int(max(tops) / DZ) + 2
    for k in range(nz + 1):
        f = k / nz
        ring = []
        for i in range(SEG):
            th = i * 2 * math.pi / SEG
            z = -1.0 + f * (tops[i] + 1.0)
            r = s.radius(th, max(z, 0.0))
            ring.append(bm.verts.new((r * math.cos(th), r * math.sin(th), z)))
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(SEG):
            j = (i + 1) % SEG
            bm.faces.new((a[i], a[j], b[j], b[i]))
    top_ring = rings[-1]
    zc = sum(v.co.z for v in top_ring) / SEG
    c = bm.verts.new((0.0, 0.0, zc + (0.0 if kind != 'dome' else 0.6)))
    for i in range(SEG):
        j = (i + 1) % SEG
        bm.faces.new((top_ring[i], top_ring[j], c))
    bot = rings[0]
    cb = bm.verts.new((0.0, 0.0, bot[0].co.z))
    for i in range(SEG):
        j = (i + 1) % SEG
        bm.faces.new((bot[j], bot[i], cb))
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new('SM_Mono_' + name, me)
    bpy.context.collection.objects.link(ob)
    # quina dura: plano onde a face vira mais de 35°, liso no resto
    for p in me.polygons:
        p.use_smooth = True
    mod = ob.modifiers.new('Quina', 'EDGE_SPLIT')
    mod.split_angle = math.radians(35.0)
    mat = bpy.data.materials.get('Rock') or bpy.data.materials.new('Rock')
    me.materials.append(mat)
    # a pegada: o raio a 1 m de altura em 32 direções
    foot = [round(s.radius(i * 2 * math.pi / 32, 1.0), 2) for i in range(32)]
    return ob, {'name': 'SM_Mono_' + name, 'kind': kind, 'height_m': round(height, 1), 'foot_r_m': foot}


def box(name, x0, x1, y0, y1, z0, z1):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = x0 if v.co.x < 0 else x1
        v.co.y = y0 if v.co.y < 0 else y1
        v.co.z = z0 if v.co.z < 0 else z1
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob


def facade(ob, shape, side):
    """A fachada esculpida (o Qasr al-Farid de Hegra): o painel plano recortado na rocha, a porta funda, duas
    pilastras, o friso e os degraus no alto (a "escada" nabateia). side = +1/−1: a face +Y ou −Y (uma pra cada
    jogador; o mapa de 2 é o giro de 180°)."""
    th = math.pi / 2 if side > 0 else -math.pi / 2
    r = min(shape.radius(th, z) for z in (2.0, 8.0, 14.0))
    face = side * (r - 1.8)                      # o plano do painel, 1,8 m pra dentro da rocha mais rasa
    out = side * (r + 8.0)
    def cut(name, x0, x1, z0, z1, depth):
        y0, y1 = sorted((face - side * depth, out))
        c = box(name, x0, x1, y0, y1, z0, z1)
        m = ob.modifiers.new(name, 'BOOLEAN')
        m.operation = 'DIFFERENCE'
        m.solver = 'EXACT'
        m.object = c
        c.hide_render = True
        c.hide_set(True)
        return c
    def add(name, x0, x1, z0, z1, proud):
        y0, y1 = sorted((face - side * 0.5, face + side * proud))
        return box(name, x0, x1, y0, y1, z0, z1)
    W, H = 9.0, 26.0
    cuts = [cut('Facade_%d' % side, -W, W, 0.0, H, 0.0),
            cut('Door_%d' % side, -2.0, 2.0, 0.3, 6.5, 1.5)]
    parts = [add('PilL_%d' % side, -W + 0.4, -W + 1.6, 0.0, H - 3.2, 0.45),
             add('PilR_%d' % side, W - 1.6, W - 0.4, 0.0, H - 3.2, 0.45),
             add('Frieze_%d' % side, -W + 0.2, W - 0.2, H - 3.2, H - 2.0, 0.35),
             add('DoorFrame_%d' % side, -2.8, 2.8, 6.5, 7.9, 0.3),
             add('StepL1_%d' % side, -W + 0.2, -W + 2.6, H - 2.0, H - 0.8, 0.25),
             add('StepL2_%d' % side, -W + 0.2, -W + 1.4, H - 0.8, H + 0.4, 0.25),
             add('StepR1_%d' % side, W - 2.6, W - 0.2, H - 2.0, H - 0.8, 0.25),
             add('StepR2_%d' % side, W - 1.4, W - 0.2, H - 0.8, H + 0.4, 0.25)]
    return cuts, parts


def export(ob, path):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_apply=True, export_yup=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    manifest = []
    x = 0.0
    obs = []
    for spec in KIT:
        ob, info = build(*spec)
        if spec[5] == 'arch':
            # o túnel: um cilindro de seção elíptica atravessando a lâmina pelo lado curto (perpendicular ao
            # comprimento), de ~3 m a ~15 m de altura
            sh = Shape(spec[1], spec[2], spec[3], spec[4], spec[5])
            bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=1.0, depth=sh.rad * 4.0, location=(0, 0, 6.0))
            cyl = bpy.context.active_object
            # (a planta é estreita na direção do yaw e comprida na perpendicular: o túnel vai pelo lado estreito,
            # com a largura ~55% do meio-comprimento, deixando as duas pernas)
            cyl.scale = (spec[3] * math.sqrt(spec[4]) * 0.5, 8.0, 1.0)   # desce até o chão: duas pernas
            cyl.rotation_euler = (math.pi / 2, 0, sh.yaw - math.pi / 2)
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
            m = ob.modifiers.new('Tunnel', 'BOOLEAN')
            m.operation = 'DIFFERENCE'
            m.solver = 'EXACT'
            m.object = cyl
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.modifier_apply(modifier='Tunnel')
            bpy.data.objects.remove(cyl, do_unlink=True)
        if spec[5] == 'dome':
            # as duas fachadas: as peças em relevo juntam na malha depois dos recortes
            parts = []
            for side in (1, -1):
                _, p = facade(ob, Shape(spec[1], spec[2], spec[3], spec[4], spec[5]), side)
                parts += p
            bpy.context.view_layer.objects.active = ob
            for m in list(ob.modifiers):
                if m.type == 'BOOLEAN':
                    bpy.ops.object.modifier_apply(modifier=m.name)
            bpy.ops.object.select_all(action='DESELECT')
            for q in parts:
                q.data.materials.append(ob.data.materials[0])
                q.select_set(True)
            ob.select_set(True)
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.join()
        export(ob, os.path.join(OUT, info['name'] + '.glb'))
        manifest.append(info)
        ob.location.x = x
        x += spec[3] * 2.8 + 6.0
        obs.append(ob)
        print('MONO', info['name'], 'altura', info['height_m'], 'pegada', min(info['foot_r_m']), '-', max(info['foot_r_m']))
    with open(os.path.join(OUT, 'monoliths.json'), 'w', encoding='utf-8') as f:
        json.dump({'about': 'WorldForge/Tools/blender_monoliths.py: the provisional sandstone kit (meters, Z up, foot at z=0). foot_r_m = plan radius at 1 m height in 32 directions from +X.',
                   'pieces': manifest}, f, indent=1)
    if PREVIEW:
        preview(obs, x)


def preview(obs, width):
    """Uma foto do kit lado a lado, na câmera do jogo (37,5°), sol baixo, CPU (Cycles)."""
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 24
    sc.render.resolution_x, sc.render.resolution_y = 1600, 700
    mat = bpy.data.materials['Rock']
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.42, 0.22, 0.12, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.9
    bpy.ops.mesh.primitive_plane_add(size=600, location=(width * 0.5, 0, 0))
    ground = bpy.context.active_object
    gm = bpy.data.materials.new('Sand')
    gm.use_nodes = True
    gm.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value = (0.49, 0.40, 0.27, 1.0)
    ground.data.materials.append(gm)
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(70), 0, math.radians(-60))
    bpy.context.collection.objects.link(sun)
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.6, 0.7, 1.0)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6
    sc.world = world
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    cam.data.lens = 35
    d = width * 0.75
    cam.location = (width * 0.5 - d * 0.55, -d * 0.75, d * 0.62)
    direction = Vector((width * 0.5, 0, 12)) - cam.location
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.collection.objects.link(cam)
    sc.camera = cam
    sc.render.filepath = os.path.join(OUT, 'preview.png')
    bpy.ops.render.render(write_still=True)
    print('MONO preview', sc.render.filepath)


main()
