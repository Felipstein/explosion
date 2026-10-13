"""As casas de adobe da maquete do look (LookLab), geradas no Blender.

Rodar (Blender sem janela, só CPU; o look.ps1 houses chama):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/look_houses.py -- <pasta de saída>

Uma casa por GLB (SM_LL_<nome>.glb), com o pivô no centro da planta, no chão. As medidas
saíram da arte day_game_cam desentortada pra vista de cima (câmera medida, escala do tanque
USA de 7,9 m de casco): planta, altura do topo da mureta e o lado da porta. São modelos
provisórios pra estudar a luz; as casas finais do jogo são do Felipe.

O método é o dos provisórios aprovados (quina dura, nunca remesh, suavizar ou ruído na malha;
o barro amolecido por voxel virou "massa de modelar" em 08/10):
- o corpo é um bloco só; a laje com a mureta sai cortando o "poço" do telhado, e portas,
  janelas e os dentes da mureta saem por boolean exato;
- todas as quinas com chanfro de 10 cm (2 segmentos), Harden Normals e Weighted Normal:
  a luz rasante desenha a quina, que é o que a câmera de RTS lê;
- os detalhes que dão escala, como nas casas da arte: verga de madeira em cada vão, peitoril,
  cinta na altura do 2º andar, pontas de viga, bica d'água na mureta, caixa da escada,
  caixa d'água e toldo;
- o fundo da janela (escuro), a porta, as vigas (madeira), a caixa d'água (metal) e o toldo
  (pano) têm material próprio: na Unreal cada nome vira um slot (look_lab.py liga).
O desgaste (manchas, terra no pé da parede, pó no telhado) fica no material da Unreal.

Eixos: o Blender exporta o glTF e a Unreal converte: +X fica +X e +Y do Blender vira -Y da
Unreal. As medidas abaixo estão na Unreal; a conversão é feita aqui (uy = -by).
"""
import math
import os
import random
import sys

import bmesh
import bpy

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'houses')
os.makedirs(OUT, exist_ok=True)

PARAPET = 0.55      # mureta acima da laje
RIM = 0.28          # espessura da mureta
RECESS = 0.25       # fundo de porta e janela
BEVEL = 0.10        # chanfro das quinas do corpo
TAPER = 0.08        # a parede de adobe se inclina pra dentro: 8 cm no topo (a casa nunca é um cubo perfeito)
_H = [4.5]          # a altura da casa em montagem (o recuo da parede depende da altura do ponto)

# a planta das casas, quintais e muros: Tools/look_layout.py (a mesma da cena e do chão)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from look_scene import HOUSES as _H, YARDS as _Y, WALL_MESHES  # noqa: E402
import look_scene  # noqa: E402
SKIRT = getattr(look_scene, 'SKIRT', 0.0)          # o kit civil: a saia abaixo da origem (o chão aplainado deixa degrau)
EXTRA = getattr(look_scene, 'EXTRA', [])

HOUSES = {n: (d['sx'], d['sy'], d['h'], d['door'], d['ex']) for n, d in _H.items()}
YARDS = {n: (d['sx'], d['sy'], d['h'], d['ex']) for n, d in _Y.items()}
WALLS = WALL_MESHES


def srgb(r, g, b):
    f = lambda c: (c / 255) / 12.92 if c / 255 <= 0.04045 else ((c / 255 + 0.055) / 1.055) ** 2.4
    return (f(r), f(g), f(b), 1.0)


MATS = {}


def mat(name, col):
    if name not in MATS:
        m = bpy.data.materials.new(name)
        m.diffuse_color = col
        MATS[name] = m
    return MATS[name]


PLASTER = lambda: mat('Plaster', srgb(186, 160, 124))
DARK = lambda: mat('Dark', srgb(18, 15, 13))
WOOD = lambda: mat('Wood', srgb(92, 62, 38))
METAL = lambda: mat('Metal', srgb(120, 118, 110))
CLOTH = lambda: mat('Cloth', srgb(150, 128, 98))


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for me in list(bpy.data.meshes):
        bpy.data.meshes.remove(me)


def box(bm, x0, x1, y0, y1, z0, z1):
    """Caixa nos eixos da Unreal (y vira -y do Blender)."""
    by0, by1 = -y1, -y0
    vs = [bm.verts.new((x, y, z)) for z in (z0, z1) for y in (by0, by1) for x in (x0, x1)]
    for f in [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]:
        bm.faces.new([vs[i] for i in f])


def obj_from(name, bm, material=None):
    me = bpy.data.meshes.new(name)
    # normais pra fora: com a volta trocada o boolean não cortava (10/10)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    if material:
        o.data.materials.append(material)
    return o


def boxes_obj(name, boxes, material):
    bm = bmesh.new()
    for b in boxes:
        box(bm, *b)
    return obj_from(name, bm, material)


def select_only(o):
    bpy.context.view_layer.update()
    for x in bpy.context.view_layer.objects:
        if x:
            x.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o


def apply(o, m):
    select_only(o)
    bpy.ops.object.modifier_apply(modifier=m.name)


def boolean(o, boxes, op):
    """Cada caixa como um sólido à parte; boolean exato (coplanar e quina dura)."""
    for i, b in enumerate(boxes):
        tool = boxes_obj(f'{o.name}_t{i}', [b], None)
        m = o.modifiers.new('b', 'BOOLEAN')
        m.operation = op
        m.object = tool
        m.solver = 'EXACT'
        apply(o, m)
        bpy.data.objects.remove(tool, do_unlink=True)


def harden(o, width, segments=2):
    """Chanfro nas quinas + normais duras: a quina pega a luz e a face continua plana."""
    b = o.modifiers.new('bevel', 'BEVEL')
    b.width = width
    b.segments = segments
    b.limit_method = 'ANGLE'
    b.angle_limit = math.radians(30)
    b.harden_normals = True
    b.miter_outer = 'MITER_ARC'
    apply(o, b)
    w = o.modifiers.new('wn', 'WEIGHTED_NORMAL')
    w.keep_sharp = True
    w.weight = 50
    apply(o, w)


def side_frame(side, sx, sy):
    """Um lado da casa: (eixo ao longo da parede, posição da face, sinal pra fora)."""
    if side in ('+Y', '-Y'):
        return 'x', (sy / 2 if side == '+Y' else -sy / 2), (1 if side == '+Y' else -1)
    return 'y', (sx / 2 if side == '+X' else -sx / 2), (1 if side == '+X' else -1)


def on_wall(side, sx, sy, a0, a1, z0, z1, d_in, d_out):
    """Caixa presa numa parede: de a0 a a1 ao longo dela, z0..z1, de d_in pra dentro até d_out pra fora.
    A face acompanha a inclinação da parede (recua TAPER × altura ÷ altura da casa)."""
    along, face, sgn = side_frame(side, sx, sy)
    face -= sgn * TAPER * min(max((z0 + z1) / 2, 0.0), _H[0]) / _H[0]
    p, q = face - sgn * d_in, face + sgn * d_out
    lo, hi = min(p, q), max(p, q)
    return (a0, a1, lo, hi, z0, z1) if along == 'x' else (lo, hi, a0, a1, z0, z1)


def chips(o, items):
    """Lascas: caixas giradas tiradas das quinas e da beirada da mureta (corte duro, depois chanfro)."""
    for i, (x, y, z, size, rz, rx) in enumerate(items):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=size)
        tool = obj_from(f'{o.name}_c{i}', bm, None)
        tool.location = (x, -y, z)
        tool.rotation_euler = (math.radians(rx), math.radians(rx * 0.6), math.radians(rz))
        m = o.modifiers.new('chip', 'BOOLEAN')
        m.operation = 'DIFFERENCE'
        m.object = tool
        m.solver = 'EXACT'
        apply(o, m)
        bpy.data.objects.remove(tool, do_unlink=True)


def house(name, sx, sy, h, door_side, ex):
    rng = random.Random(ex.get('seed', 0))
    stories = ex.get('stories', 1)
    slab = h - PARAPET
    _H[0] = h

    shell = boxes_obj('SM_LL_' + name, [(-sx / 2, sx / 2, -sy / 2, sy / 2, 0.0, h)], PLASTER())
    for v in shell.data.vertices:          # a parede inclinada: o topo recua TAPER
        if v.co.z > h - 1e-3:
            v.co.x *= (sx / 2 - TAPER) / (sx / 2)
            v.co.y *= (sy / 2 - TAPER) / (sy / 2)
    cuts = [(-sx / 2 + RIM, sx / 2 - RIM, -sy / 2 + RIM, sy / 2 - RIM, slab, h + 1.0)]   # o telhado com a mureta

    # dentes na mureta: trechos 10–18 cm mais baixos (a mureta de adobe nunca é reta)
    for side in ('+X', '-X', '+Y', '-Y'):
        along, face, sgn = side_frame(side, sx, sy)
        length = sx if along == 'x' else sy
        for _ in range(rng.randint(0, 2)):
            a = rng.uniform(-length / 2 + 0.8, length / 2 - 2.2)
            cuts.append(on_wall(side, sx, sy, a, a + rng.uniform(1.0, 2.4), h - rng.uniform(0.10, 0.18), h + 1.0, RIM + 0.05, 0.5))

    openings, woods, darks = [], [], []
    for side in ('+X', '-X', '+Y', '-Y'):
        along, face, sgn = side_frame(side, sx, sy)
        length = sx if along == 'x' else sy
        slots = max(1, int((length - 1.5) // 2.8))
        xs = [(-length / 2 + 0.9) + (i + 0.5) * (length - 1.8) / slots for i in range(slots)]
        door_at = None
        if side == door_side:
            door_at = rng.choice(xs) if slots > 1 else 0.0
            openings.append((side, door_at - 0.575, door_at + 0.575, 0.0, 2.35, 'door'))
        for floor in range(stories):
            base = 1.35 + floor * 3.1
            for a in xs:
                if door_at is not None and floor == 0 and abs(a - door_at) < 1.4:
                    continue
                if rng.random() < 0.35:
                    continue
                ww, wh = rng.choice(((0.75, 1.0), (0.9, 1.1), (0.6, 0.9)))
                openings.append((side, a - ww / 2, a + ww / 2, base, base + wh, 'shutter' if rng.random() < 0.3 else 'window'))
    # o reboco caído: placas rasas (3 cm) que acendem na borda com o sol rasante
    for side in ('+X', '-X', '+Y', '-Y'):
        along, face, sgn = side_frame(side, sx, sy)
        length = sx if along == 'x' else sy
        for _ in range(rng.randint(1, 3)):
            w, hh = rng.uniform(0.6, 1.8), rng.uniform(0.4, 1.2)
            a = rng.uniform(-length / 2 + 0.4, length / 2 - 0.4 - w)
            z = rng.uniform(0.2, slab - 0.3 - hh)
            cuts.append(on_wall(side, sx, sy, a, a + w, z, z + hh, 0.03, 0.5))
    for side, a0, a1, z0, z1, kind in openings:
        cuts.append(on_wall(side, sx, sy, a0, a1, z0, z1, RECESS, 0.5))
        plate = on_wall(side, sx, sy, a0, a1, z0, z1, RECESS + 0.02, -(RECESS - 0.04))
        (darks if kind == 'window' else woods).append(plate)
    boolean(shell, cuts, 'DIFFERENCE')

    adds = []
    if 'roofbox' in ex:   # a caixa da escada no telhado
        cx, cy = ex['roofbox']
        bw, bd = 2.2, 1.8
        x = cx * (sx / 2 - RIM - bw / 2 - 0.3)
        y = cy * (sy / 2 - RIM - bd / 2 - 0.3)
        adds.append((x - bw / 2, x + bw / 2, y - bd / 2, y + bd / 2, slab - 0.3, slab + 2.3))
    if stories > 1:       # a cinta entre os andares
        for side in ('+X', '-X', '+Y', '-Y'):
            along, face, sgn = side_frame(side, sx, sy)
            length = sx if along == 'x' else sy
            adds.append(on_wall(side, sx, sy, -length / 2 - 0.06, length / 2 + 0.06, 3.0, 3.18, 0.1, 0.06))
    for side, a0, a1, z0, z1, kind in openings:   # peitoril das janelas
        if kind != 'door':
            adds.append(on_wall(side, sx, sy, a0 - 0.1, a1 + 0.1, z0 - 0.09, z0, 0.1, 0.07))
    boolean(shell, adds, 'UNION')
    # lascas nas quinas verticais e na beirada da mureta
    hx, hy = sx / 2 - TAPER / 2, sy / 2 - TAPER / 2
    lasc = []
    for cx_, cy_ in ((hx, hy), (-hx, hy), (hx, -hy), (-hx, -hy)):
        for _ in range(rng.randint(0, 2)):
            lasc.append((cx_, cy_, rng.uniform(0.3, h - 0.2), rng.uniform(0.25, 0.55), rng.uniform(0, 90), rng.uniform(-20, 20)))
    for _ in range(rng.randint(2, 4)):
        side = rng.choice(('+X', '-X', '+Y', '-Y'))
        along, face, sgn = side_frame(side, sx, sy)
        length = sx if along == 'x' else sy
        a = rng.uniform(-length / 2, length / 2)
        f = face - sgn * TAPER
        x_, y_ = (a, f) if along == 'x' else (f, a)
        lasc.append((x_, y_, h, rng.uniform(0.3, 0.6), rng.uniform(0, 90), rng.uniform(-25, 25)))
    chips(shell, lasc)
    harden(shell, BEVEL)
    parts = [shell]

    def wood_piece(nm, bs, material=None, bevel=0.02):
        if bs:
            o = boxes_obj(nm, bs, material or WOOD())
            harden(o, bevel, 1)
            parts.append(o)

    lintels = []
    for side, a0, a1, z0, z1, kind in openings:   # verga de madeira em cada vão
        lintels.append(on_wall(side, sx, sy, a0 - 0.18, a1 + 0.18, z1, z1 + 0.15, 0.1, 0.05))
    wood_piece(name + '_lintels', lintels)
    parts.append(boxes_obj(name + '_dark', darks, DARK()))
    parts.append(boxes_obj(name + '_wood', woods, WOOD()))

    if 'beams' in ex:     # as pontas das vigas do teto saindo da fachada
        along, face, sgn = side_frame(ex['beams'], sx, sy)
        length = sx if along == 'x' else sy
        n = int(length // 1.25)
        bs = []
        for i in range(n):
            a = -length / 2 + 0.6 + i * (length - 1.2) / max(1, n - 1)
            bs.append(on_wall(ex['beams'], sx, sy, a - 0.09, a + 0.09, slab - 0.35, slab - 0.17, 0.05, 0.32))
        wood_piece(name + '_beams', bs)
    else:                 # bica d'água na mureta
        side = rng.choice(('+X', '-X', '+Y', '-Y'))
        along, face, sgn = side_frame(side, sx, sy)
        length = sx if along == 'x' else sy
        bs = []
        for a in (-length / 4, length / 4):
            bs.append(on_wall(side, sx, sy, a - 0.08, a + 0.08, slab + 0.02, slab + 0.16, 0.05, 0.45))
        wood_piece(name + '_spouts', bs)

    if 'tank' in ex:      # caixa d'água em cima de pés
        cx, cy = ex['tank']
        x = cx * (sx / 2 - RIM - 1.0)
        y = cy * (sy / 2 - RIM - 1.0)
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.55, depth=1.1, location=(x, -y, slab + 0.85))
        t = bpy.context.active_object
        t.name = name + '_tank'
        t.data.materials.append(METAL())
        harden(t, 0.03, 1)
        parts.append(t)
        legs = [(x + dx - 0.04, x + dx + 0.04, y + dy - 0.04, y + dy + 0.04, slab, slab + 0.32)
                for dx in (-0.38, 0.38) for dy in (-0.38, 0.38)]
        wood_piece(name + '_tankleg', legs, METAL(), 0.01)

    if 'canopy' in ex:    # o toldo na frente: quatro paus e uma esteira inclinada
        along, face, sgn = side_frame(ex['canopy'], sx, sy)
        length = (sx if along == 'x' else sy) * 0.6
        depth, z_hi, z_lo = 3.0, slab - 0.6, slab - 1.3
        poles = []
        for i in range(4):
            a = -length / 2 + i * length / 3
            poles.append(on_wall(ex['canopy'], sx, sy, a - 0.07, a + 0.07, 0.0, z_lo, -(depth - 0.07), depth + 0.07))
        wood_piece(name + '_poles', poles)
        bm5 = bmesh.new()
        if along == 'x':
            y0, y1 = face, face + sgn * depth
            vs = [bm5.verts.new((-length / 2 - 0.2, -y0, z_hi)), bm5.verts.new((length / 2 + 0.2, -y0, z_hi)),
                  bm5.verts.new((length / 2 + 0.2, -y1, z_lo)), bm5.verts.new((-length / 2 - 0.2, -y1, z_lo))]
        else:
            x0, x1 = face, face + sgn * depth
            vs = [bm5.verts.new((x0, length / 2 + 0.2, z_hi)), bm5.verts.new((x0, -length / 2 - 0.2, z_hi)),
                  bm5.verts.new((x1, -length / 2 - 0.2, z_lo)), bm5.verts.new((x1, length / 2 + 0.2, z_lo))]
        # as ripas: tiras de 12 cm a cada 26 cm, de cima a baixo do telheiro, em duas vigas
        p0, p1, p2, p3 = [v.co.copy() for v in vs]
        bm5.free()
        bm5 = bmesh.new()
        n = int((p0 - p1).length / 0.26)
        for i in range(n):
            a0, a1 = i / n, i / n + 0.12 / (p0 - p1).length
            q = [p0.lerp(p1, a0), p0.lerp(p1, a1), p3.lerp(p2, a1), p3.lerp(p2, a0)]
            f = bm5.faces.new([bm5.verts.new(c) for c in q])
            bmesh.ops.solidify(bm5, geom=[f], thickness=0.04)
        for t in (0.08, 0.92):      # as duas vigas por baixo
            a, b = p0.lerp(p3, t), p1.lerp(p2, t)
            d = (b - a).normalized()
            w = 0.07
            side = d.cross(__import__('mathutils').Vector((0, 0, 1))).normalized() * w
            q = [a - side, b - side, b + side, a + side]
            f = bm5.faces.new([bm5.verts.new(c - __import__('mathutils').Vector((0, 0, 0.05))) for c in q])
            bmesh.ops.solidify(bm5, geom=[f], thickness=0.12)
        parts.append(obj_from(name + '_canopy', bm5, WOOD()))

    select_only(shell)
    for p in parts:
        p.select_set(True)
    bpy.ops.object.join()
    return shell


def yard(name, sx, sy, hw, ex):
    rng = random.Random(ex.get('seed', 0))
    t = 0.35
    shell = boxes_obj('SM_LL_' + name, [(-sx / 2, sx / 2, -sy / 2, sy / 2, 0.0, hw)], PLASTER())
    cuts = [(-sx / 2 + t, sx / 2 - t, -sy / 2 + t, sy / 2 - t, -1.0, hw + 1.0),     # o pátio
            (-sx / 2 + 3.0, -sx / 2 + 4.6, sy / 2 - t - 0.5, sy / 2 + 0.5, -1.0, hw + 1.0)]   # o portão
    for _ in range(4):    # o topo do muro em degraus
        a = rng.uniform(-sx / 2, sx / 2 - 2.0)
        cuts.append((a, a + rng.uniform(1.2, 2.5), -sy / 2 - 1, sy / 2 + 1, hw - rng.uniform(0.15, 0.4), hw + 1.0))
    boolean(shell, cuts, 'DIFFERENCE')
    boolean(shell, [(-sx / 2, -sx / 2 + 3.2, -sy / 2, -sy / 2 + 3.0, 0.0, 2.9)], 'UNION')   # o quartinho do fundo
    harden(shell, 0.08)
    bm5 = bmesh.new()     # a esteira do telheiro
    x0, x1 = -sx / 2 + 3.2, -sx / 2 + 6.0
    y0, y1 = sy / 2 - 3.6, sy / 2 - 0.6
    vs = [bm5.verts.new((x0, -y0, 2.8)), bm5.verts.new((x1, -y0, 2.3)), bm5.verts.new((x1, -y1, 2.3)), bm5.verts.new((x0, -y1, 2.8))]
    f = bm5.faces.new(vs)
    bmesh.ops.solidify(bm5, geom=[f], thickness=0.05)
    cloth = obj_from(name + '_cloth', bm5, CLOTH())
    select_only(shell)
    cloth.select_set(True)
    bpy.ops.object.join()
    return shell


def wall(name, length, hw):
    rng = random.Random(len(name) * 7 + int(length))
    kit = name.startswith('civ_')
    shell = boxes_obj('SM_LL_' + name, [(-length / 2, length / 2, -0.18, 0.18, 0.0, hw)], PLASTER())
    cuts = []
    for _ in range(max(1, int(length // 3))):   # o topo em degraus
        a = rng.uniform(-length / 2, length / 2 - 1.2)
        cuts.append((a, a + rng.uniform(0.8, 2.0), -1, 1, hw - rng.uniform(0.12, 0.35), hw + 1))
    if name.endswith('gate'):                    # o portão: o vão de 1,8 m no meio
        cuts.append((-0.9, 0.9, -1, 1, -1, hw + 1))
    boolean(shell, cuts, 'DIFFERENCE')
    parts = []
    if kit:                                      # o pilarete na ponta +X (o padrão da ficha civ_wall_gate_gpt)
        parts.append(boxes_obj(name + '_pillar', [(length / 2 - 0.42, length / 2 - 0.02, -0.24, 0.24, 0.0, hw + 0.25)], PLASTER()))
    if name.endswith('gate'):
        # a folha do portão de chapa, na espessura do muro (a ficha do Gemini tinha uma ripa fina), e as ombreiras
        parts.append(boxes_obj(name + '_leaf', [(-0.88, 0.88, -0.05, 0.05, 0.05, hw - 0.25)], METAL()))
        parts.append(boxes_obj(name + '_jambs', [(-1.02, -0.9, -0.22, 0.22, 0.0, hw + 0.1), (0.9, 1.02, -0.22, 0.22, 0.0, hw + 0.1)], WOOD()))
    harden(shell, 0.07)
    if parts:
        select_only(shell)
        for p in parts:
            p.select_set(True)
        bpy.ops.object.join()
    return shell


def pole(name):
    """O poste de madeira (~8 m) com a cruzeta ao longo de X, os isoladores e a ponta dos fios."""
    o = boxes_obj('SM_LL_' + name, [(-0.13, 0.13, -0.13, 0.13, 0.0, 8.0)], WOOD())
    parts = [boxes_obj(name + '_arm', [(-1.1, 1.1, -0.07, 0.07, 7.25, 7.4)], WOOD()),
             boxes_obj(name + '_brace', [(-0.5, 0.5, -0.04, 0.04, 6.9, 7.0)], WOOD()),
             boxes_obj(name + '_ins', [(x - 0.05, x + 0.05, -0.05, 0.05, 7.4, 7.6) for x in (-0.95, -0.35, 0.35, 0.95)], DARK()),
             boxes_obj(name + '_wire', [(x - 0.012, x + 0.012, -1.0, 1.0, 7.56, 7.584) for x in (-0.95, -0.35, 0.35, 0.95)], DARK())]
    harden(o, 0.03)
    select_only(o)
    for p in parts:
        p.select_set(True)
    bpy.ops.object.join()
    return o


def finish_kit(o, footprint, ex):
    """O kit civil: a saia de SKIRT m abaixo do chão e o deslocamento do corpo dentro da planta (a varanda)."""
    if SKIRT > 0 and footprint:
        x0, x1, y0, y1 = footprint
        sk = boxes_obj(o.name + '_skirt', [(x0, x1, y0, y1, -SKIRT, 0.02)], PLASTER())
        select_only(o)
        sk.select_set(True)
        bpy.ops.object.join()
    off = ex.get('offset')
    if off:
        select_only(o)
        o.location = (off[0], -off[1], 0.0)      # (o Blender recebe Y invertido: V(x, y, z) = (x, -y, z))
        bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    return o


def export(o):
    select_only(o)
    path = os.path.join(OUT, o.name + '.glb')
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_normals=True, export_texcoords=False)
    print('LOOKHOUSE', o.name, len(o.data.polygons), 'faces ->', path)


def main():
    only = os.environ.get('LOOK_HOUSES')    # ex.: "H11,H1" pra refazer só algumas
    pick = lambda n: not only or n in only.split(',')
    for name, (sx, sy, h, door, ex) in HOUSES.items():
        if pick(name):
            clear()
            export(finish_kit(house(name, sx, sy, h, door, ex), (-sx / 2, sx / 2, -sy / 2, sy / 2), ex))
    for name, (sx, sy, hw, ex) in YARDS.items():
        if pick(name):
            clear()
            export(yard(name, sx, sy, hw, ex))
    for name, (ln, hw) in WALLS.items():
        if pick(name):
            clear()
            export(finish_kit(wall(name, ln, hw), (-ln / 2, ln / 2, -0.18, 0.18), {}))
    for name in EXTRA:
        if pick(name) and name.endswith('pole'):
            clear()
            export(finish_kit(pole(name), None, {}))


main()
