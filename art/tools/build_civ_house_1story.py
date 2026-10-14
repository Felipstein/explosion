"""Casa civil de 1 andar (docs/concept/buildings/civil/civ_house_1story_gemini.webp), modelada por
script no arquivo de trabalho captures/art3d/civ_house_1story/civ_house_1story.blend, do jeito
editável: uma malha simples e limpa (que se mexe à mão) com o barro feito por modificadores ao
vivo por cima. Nada é aplicado: mexeu na forma, o barro se refaz.

Medidas tiradas da ficha com régua em metros (8 m = a largura da frente; a vista de cima e a de
trás batem entre si, a de frente põe a caixinha do teto em outro lugar: vale a de cima):
- casa 8 x 8 m, 4,5 m até o topo da borda, laje a 3,9 m (borda de 0,6 m, 0,3 m de espessura);
- paredes de 40 cm com o cômodo oco (sólido fechado, docs/PRODUCAO_3D.md §6), janelas e porta
  afundadas 25 cm (não atravessam);
- porta 1,15 x 2,6 m (centro x = +1,43), batente de pedra com verga e degrau;
- janela da frente 0,75 x 1,1 m (centro x = -1,98, peitoril 1,45), uma atrás e uma na direita com
  moldura saliente e peitoril (vista 3/4);
- 5 pontas de viga quadradas a 3,55 m na frente;
- caixinha oca no teto (1,1 x 0,9 m) no canto de trás à esquerda e um alçapão baixo.

O barro (modificadores do corpo, nesta ordem):
  Remesh 7 cm → Smooth (quinas e topo moles) → Displace "Ondas" (parede torta, ±4 cm, 1,4 m)
  → Displace "Reboco" (mão do pedreiro, ±1 cm) → Decimate (~35 mil tri) → Boolean "Danos"
  (as lascas: os objetos da coleção "Danos (cortes)"; arrastar um muda onde está quebrado; a face
  da lasca pega o tijolo, M_Civ_Brick).
O grão fino do reboco vai na textura (normal), não na malha.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/build_civ_house_1story.py
Rodar de novo refaz a casa; o rascunho do Felipe fica guardado (coleção escondida).
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
FOLDER = os.path.join(REPO, 'captures', 'art3d', 'civ_house_1story')
FILE = os.path.join(FOLDER, 'civ_house_1story.blend')
NAME = 'SM_Civ_House_1Story'
DAMAGE = 'Danos (cortes, nao exportar)'

W = D = 8.0
H = 4.5
ROOF = 3.9
WALL = 0.4
RIM = 0.3
RECESS = 0.25
DOOR = dict(cx=1.43, w=1.15, h=2.6)
WIN_FRONT = dict(cx=-1.98, w=0.75, h=1.1, sill=1.45)
WIN_BACK = dict(cx=0.0, w=0.75, h=0.95, sill=1.45)
WIN_RIGHT = dict(cy=-1.9, w=0.6, h=1.2, sill=1.2)
BEAMS_X = (-2.7, -1.25, 0.2, 1.65, 3.1)
BEAM_Z = 3.55
ROOFBOX = (-3.15, -2.05, 0.1, 1.0)
RNG = random.Random(11)

# as lascas: (centro x, y, z, raio) — quinas, borda do teto e pé da parede; a da quina
# frente-direita no meio da altura é a da ficha
CHIPS = [
    (4.0, -4.0, 2.35, 0.33), (3.55, 4.0, 4.55, 0.5), (-2.4, -4.0, 4.6, 0.32), (-4.0, -4.0, 0.25, 0.38),
    (-4.0, 1.4, 4.55, 0.3), (-4.0, 4.0, 1.7, 0.28), (4.0, 2.6, 4.6, 0.26), (2.55, -4.0, 3.15, 0.16),
]


def srgb(r, g, b):
    f = lambda c: (c / 255) / 12.92 if c / 255 <= 0.04045 else ((c / 255 + 0.055) / 1.055) ** 2.4
    return (f(r), f(g), f(b), 1.0)


def new_obj(name, bm, col):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    col.objects.link(o)
    return o


def add_box(bm, x0, x1, y0, y1, z0, z1, mat=0, rot_z=0.0, pivot=None):
    r = bmesh.ops.create_cube(bm, size=1.0)
    vs = r['verts']
    for v in vs:
        v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), (y0 + y1) / 2 + v.co.y * (y1 - y0), (z0 + z1) / 2 + v.co.z * (z1 - z0)))
    if rot_z or pivot:
        c = pivot or Vector(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
        bmesh.ops.rotate(bm, verts=vs, cent=c, matrix=rot_z)
    for f in {f for v in vs for f in v.link_faces}:
        f.material_index = mat
    return vs


def box_obj(name, x0, x1, y0, y1, z0, z1, col):
    bm = bmesh.new()
    add_box(bm, x0, x1, y0, y1, z0, z1)
    return new_obj(name, bm, col)


def apply_mod(o, mod):
    for x in bpy.context.view_layer.objects:
        x.select_set(x is o)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)


def boolean(o, other, op):
    m = o.modifiers.new('bool', 'BOOLEAN')
    m.operation = op
    m.solver = 'EXACT'
    m.object = other
    apply_mod(o, m)
    bpy.data.objects.remove(other, do_unlink=True)


def principled(m):
    m.use_nodes = True
    for n in m.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            return n
    b = m.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    out = next((n for n in m.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None) or m.node_tree.nodes.new('ShaderNodeOutputMaterial')
    m.node_tree.links.new(b.outputs['BSDF'], out.inputs['Surface'])
    return b


def material(name, rgba, rough=0.9, dirt=False, grain=0.0):
    """Material de prévia (no jogo vira textura escaneada, docs/PRODUCAO_3D.md §5)."""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type != 'OUTPUT_MATERIAL':
            nt.nodes.remove(n)
    b = principled(m)
    N, L = nt.nodes, nt.links
    b.inputs['Roughness'].default_value = rough
    m.diffuse_color = rgba                                 # a cor no modo sólido do Blender
    tc = N.new('ShaderNodeTexCoord')
    mottle = N.new('ShaderNodeTexNoise')
    mottle.inputs['Scale'].default_value = 0.45
    mottle.inputs['Detail'].default_value = 6.0
    L.new(tc.outputs['Object'], mottle.inputs['Vector'])
    ramp = N.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].color = tuple(c * 0.86 for c in rgba[:3]) + (1.0,)
    ramp.color_ramp.elements[1].color = tuple(min(1.0, c * 1.08) for c in rgba[:3]) + (1.0,)
    ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[1].position = 0.3, 0.7
    L.new(mottle.outputs['Fac'], ramp.inputs['Fac'])
    col = ramp.outputs['Color']
    if dirt:
        geo = N.new('ShaderNodeNewGeometry')
        sep = N.new('ShaderNodeSeparateXYZ')
        L.new(geo.outputs['Position'], sep.inputs['Vector'])
        mr = N.new('ShaderNodeMapRange')
        mr.inputs['From Min'].default_value, mr.inputs['From Max'].default_value = 0.0, 1.3
        mr.inputs['To Min'].default_value, mr.inputs['To Max'].default_value = 0.62, 1.0
        L.new(sep.outputs['Z'], mr.inputs['Value'])
        mul = N.new('ShaderNodeMix')
        mul.data_type, mul.blend_type = 'RGBA', 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        L.new(col, mul.inputs['A'])
        g = N.new('ShaderNodeCombineColor')
        for ch in ('Red', 'Green', 'Blue'):
            L.new(mr.outputs['Result'], g.inputs[ch])
        L.new(g.outputs['Color'], mul.inputs['B'])
        col = mul.outputs['Result']
    L.new(col, b.inputs['Base Color'])
    if grain:
        fine = N.new('ShaderNodeTexNoise')
        fine.inputs['Scale'].default_value = 14.0
        fine.inputs['Detail'].default_value = 10.0
        L.new(tc.outputs['Object'], fine.inputs['Vector'])
        bump = N.new('ShaderNodeBump')
        bump.inputs['Strength'].default_value = grain
        bump.inputs['Distance'].default_value = 0.01
        L.new(fine.outputs['Fac'], bump.inputs['Height'])
        L.new(bump.outputs['Normal'], b.inputs['Normal'])
    return m


def noise_tex(name, size, depth=2):
    t = bpy.data.textures.get(name) or bpy.data.textures.new(name, 'CLOUDS')
    t.noise_scale = size
    t.noise_depth = depth
    return t


# ---- o corpo: malha simples (o que se edita) -----------------------------------------------

def build_base(col):
    hw, hd = W / 2, D / 2
    shell = box_obj(NAME, -hw, hw, -hd, hd, 0.0, H, col)
    boolean(shell, box_obj('well', -hw + RIM, hw - RIM, -hd + RIM, hd - RIM, ROOF, H + 1.0, col), 'DIFFERENCE')
    boolean(shell, box_obj('room', -hw + WALL, hw - WALL, -hd + WALL, hd - WALL, 0.1, ROOF - 0.3, col), 'DIFFERENCE')
    d, wf, wb, wr = DOOR, WIN_FRONT, WIN_BACK, WIN_RIGHT
    cuts = [
        (d['cx'] - d['w'] / 2, d['cx'] + d['w'] / 2, -hd - 0.3, -hd + RECESS, -0.1, d['h']),
        (wf['cx'] - wf['w'] / 2, wf['cx'] + wf['w'] / 2, -hd - 0.3, -hd + RECESS, wf['sill'], wf['sill'] + wf['h']),
        (wb['cx'] - wb['w'] / 2, wb['cx'] + wb['w'] / 2, hd - RECESS, hd + 0.3, wb['sill'], wb['sill'] + wb['h']),
        (hw - RECESS, hw + 0.3, wr['cy'] - wr['w'] / 2, wr['cy'] + wr['w'] / 2, wr['sill'], wr['sill'] + wr['h']),
    ]
    for i, c in enumerate(cuts):
        boolean(shell, box_obj(f'cut{i}', *c, col), 'DIFFERENCE')
    x0, x1, y0, y1 = ROOFBOX
    boolean(shell, box_obj('roofbox', x0, x1, y0, y1, ROOF - 0.05, 4.85, col), 'UNION')
    boolean(shell, box_obj('roofbox_hole', x0 + 0.14, x1 - 0.14, y0 + 0.14, y1 - 0.14, 4.35, 5.2, col), 'DIFFERENCE')
    boolean(shell, box_obj('hatch', -2.05, -1.5, 1.7, 2.25, ROOF - 0.05, ROOF + 0.12, col), 'UNION')
    # taipa: parede ~2 cm mais grossa no pé que no topo
    for v in shell.data.vertices:
        k = 1.0 + 0.005 * max(0.0, 1.0 - v.co.z / H)
        v.co.x *= k
        v.co.y *= k
    # malha limpa: junta as faces planas que o recorte dividiu
    bm = bmesh.new()
    bm.from_mesh(shell.data)
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1.0), verts=bm.verts, edges=bm.edges)
    bm.to_mesh(shell.data)
    bm.free()
    return shell


def damage_cutters(scene, brick):
    col = bpy.data.collections.get(DAMAGE)
    if col:
        for o in list(col.objects):
            bpy.data.objects.remove(o, do_unlink=True)
    else:
        col = bpy.data.collections.new(DAMAGE)
        scene.collection.children.link(col)
    for i, (x, y, z, r) in enumerate(CHIPS):
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
        for v in bm.verts:                                   # pedra quebrada, não bola
            v.co *= RNG.uniform(0.7, 1.15)
        bmesh.ops.scale(bm, vec=Vector((RNG.uniform(0.8, 1.3), RNG.uniform(0.8, 1.3), RNG.uniform(0.6, 1.0))), verts=bm.verts)
        o = new_obj(f'Dano_{i + 1}', bm, col)
        o.location = (x, y, z)
        o.rotation_euler = (RNG.uniform(0, 3), RNG.uniform(0, 3), RNG.uniform(0, 3))
        o.data.materials.append(brick)
        o.display_type = 'WIRE'
        o.hide_render = True
    return col


def clay_stack(shell, damage_col):
    """O barro por cima da malha simples: tudo ao vivo, nada aplicado."""
    for m in list(shell.modifiers):
        shell.modifiers.remove(m)
    m = shell.modifiers.new('Barro: malha uniforme', 'REMESH')
    m.mode, m.voxel_size, m.use_smooth_shade = 'VOXEL', 0.07, True
    m = shell.modifiers.new('Barro: quinas moles', 'SMOOTH')
    m.factor, m.iterations = 0.6, 5
    m = shell.modifiers.new('Barro: parede torta', 'DISPLACE')
    m.texture, m.strength, m.mid_level, m.texture_coords = noise_tex('Ondas', 1.4, 2), 0.08, 0.5, 'LOCAL'
    m = shell.modifiers.new('Barro: reboco a mao', 'DISPLACE')
    m.texture, m.strength, m.mid_level, m.texture_coords = noise_tex('Reboco', 0.35, 2), 0.02, 0.5, 'LOCAL'
    m = shell.modifiers.new('Leve (~35 mil tri)', 'DECIMATE')
    m.ratio = 0.3
    m = shell.modifiers.new('Danos (lascas)', 'BOOLEAN')
    m.operation, m.operand_type, m.collection, m.solver = 'DIFFERENCE', 'COLLECTION', damage_col, 'EXACT'
    m.material_mode = 'TRANSFER'
    for m in shell.modifiers:
        m.show_on_cage = False
        m.show_in_editmode = True


# ---- madeira e pedra: objetos simples, cada um com nome -------------------------------------

def trim_obj(name, col, parts, mats, bev=0.012):
    """parts: [(x0, x1, y0, y1, z0, z1, índice do material)]"""
    bm = bmesh.new()
    for p in parts:
        add_box(bm, *p[:6], mat=p[6])
    o = new_obj(name, bm, col)
    for mt in mats:
        o.data.materials.append(mt)
    if bev:
        m = o.modifiers.new('Quinas', 'BEVEL')
        m.width, m.segments, m.limit_method = bev, 2, 'ANGLE'
    return o


def build_trims(col, mats, shell):
    hw, hd = W / 2, D / 2
    out = []
    d = DOOR
    x0, x1 = d['cx'] - d['w'] / 2, d['cx'] + d['w'] / 2
    face = -hd - 0.02
    fw = 0.2
    out.append(trim_obj(NAME + '_DoorFrame', col, [
        (x0 - fw, x0, face - 0.07, face + 0.1, 0.0, d['h'] + 0.02, 0),
        (x1, x1 + fw, face - 0.07, face + 0.1, 0.0, d['h'] + 0.02, 0),
        (x0 - fw - 0.06, x1 + fw + 0.06, face - 0.09, face + 0.1, d['h'], d['h'] + 0.26, 0)], [mats['stone']], 0.02))
    out.append(trim_obj(NAME + '_Step', col, [(x0 - fw - 0.04, x1 + fw + 0.04, face - 0.42, face + 0.08, 0.0, 0.13, 0)], [mats['stone']], 0.025))
    back = -hd + RECESS - 0.02
    planks = []
    pw = d['w'] / 5
    for i in range(5):
        dz, dy = RNG.uniform(-0.015, 0.0), RNG.uniform(-0.008, 0.008)
        planks.append((x0 + i * pw + 0.006, x0 + (i + 1) * pw - 0.006, back - 0.06 + dy, back + dy, 0.02, d['h'] - 0.02 + dz, 0))
    for z in (0.35, d['h'] - 0.45):
        planks.append((x0 + 0.05, x1 - 0.05, back - 0.09, back - 0.055, z, z + 0.12, 0))
    out.append(trim_obj(NAME + '_Door', col, planks, [mats['wood']], 0.006))

    def window(name, w, h, sill, at, axis, s, stone=False):
        """vidro escuro no fundo do recorte + caixilho de madeira com montante; at = centro na parede"""
        def b(a0, a1, n0, n1, z0, z1, mat):
            n_back = s * (hd - RECESS) if axis == 'y' else s * (hw - RECESS)
            nn0, nn1 = sorted((n_back + s * n0, n_back + s * n1))
            if axis == 'y':
                return (at + a0, at + a1, nn0, nn1, z0, z1, mat)
            return (nn0, nn1, at + a0, at + a1, z0, z1, mat)
        t = 0.05
        parts = [b(-w / 2, w / 2, -0.01, 0.0, sill, sill + h, 1)]
        for a0, a1, z0, z1 in ((-w / 2, -w / 2 + t, sill, sill + h), (w / 2 - t, w / 2, sill, sill + h),
                               (-w / 2, w / 2, sill, sill + t), (-w / 2, w / 2, sill + h - t, sill + h), (-0.02, 0.02, sill, sill + h)):
            parts.append(b(a0, a1, -0.07, -0.01, z0, z1, 0))
        if stone:                                            # moldura saliente e peitoril (a da lateral, vista 3/4)
            f = 0.12
            parts += [b(-w / 2 - f, -w / 2, RECESS - 0.08, RECESS + 0.08, sill - 0.08, sill + h + 0.1, 2),
                      b(w / 2, w / 2 + f, RECESS - 0.08, RECESS + 0.08, sill - 0.08, sill + h + 0.1, 2),
                      b(-w / 2 - f - 0.04, w / 2 + f + 0.04, RECESS - 0.08, RECESS + 0.1, sill + h, sill + h + 0.15, 2),
                      b(-w / 2 - f - 0.04, w / 2 + f + 0.04, RECESS - 0.08, RECESS + 0.16, sill - 0.12, sill, 2)]
        return trim_obj(name, col, parts, [mats['wood'], mats['dark'], mats['stone']], 0.005)

    out.append(window(NAME + '_WindowFront', WIN_FRONT['w'], WIN_FRONT['h'], WIN_FRONT['sill'], WIN_FRONT['cx'], 'y', -1))
    out.append(window(NAME + '_WindowBack', WIN_BACK['w'], WIN_BACK['h'], WIN_BACK['sill'], WIN_BACK['cx'], 'y', 1))
    out.append(window(NAME + '_WindowRight', WIN_RIGHT['w'], WIN_RIGHT['h'], WIN_RIGHT['sill'], WIN_RIGHT['cy'], 'x', 1, stone=True))
    # pontas de viga quadradas (como na ficha), cada uma com um torto e um comprimento
    bm = bmesh.new()
    for x in BEAMS_X:
        outlen = RNG.uniform(0.24, 0.34)
        s = RNG.uniform(0.13, 0.17)
        add_box(bm, x - s / 2, x + s / 2, -hd - outlen, -hd + 0.3, BEAM_Z - s * 0.45, BEAM_Z + s * 0.45,
                rot_z=__import__('mathutils').Matrix.Rotation(RNG.uniform(-0.05, 0.05), 3, 'Z'), pivot=Vector((x, -hd, BEAM_Z)))
    beams = new_obj(NAME + '_Beams', bm, col)
    beams.data.materials.append(mats['wood'])
    m = beams.modifiers.new('Quinas', 'BEVEL')
    m.width, m.segments, m.limit_method = 0.015, 2, 'ANGLE'
    out.append(beams)
    for o in out:
        o.parent = shell                                     # mexeu a casa, os detalhes vão junto
    return out


def main():
    bpy.ops.wm.open_mainfile(filepath=FILE)
    scene = bpy.context.scene
    model = bpy.data.collections['Modelo']
    draft = bpy.data.collections.get('Rascunho_Felipe (nao exportar)')
    if not draft:
        draft = bpy.data.collections.new('Rascunho_Felipe (nao exportar)')
        scene.collection.children.link(draft)
    for o in list(model.objects):
        if o.name.startswith(NAME):
            bpy.data.objects.remove(o, do_unlink=True)
        else:
            for c in list(o.users_collection):
                c.objects.unlink(o)
            draft.objects.link(o)
    draft.hide_render = True
    lc = bpy.context.view_layer.layer_collection.children.get(draft.name)
    if lc:
        lc.hide_viewport = True
    mats = {
        'plaster': material('M_Civ_Plaster', srgb(176, 154, 124), 0.95, dirt=True, grain=0.35),
        'stone': material('M_Civ_Stone', srgb(168, 158, 140), 0.9, grain=0.25),
        'wood': material('M_Civ_Wood', srgb(82, 64, 48), 0.85, grain=0.2),
        'dark': material('M_Civ_Dark', srgb(20, 18, 17), 0.5),
        'brick': material('M_Civ_Brick', srgb(140, 100, 72), 0.95, grain=0.5),
    }
    shell = build_base(model)
    shell.data.materials.clear()
    shell.data.materials.append(mats['plaster'])
    dmg = damage_cutters(scene, mats['brick'])
    clay_stack(shell, dmg)
    build_trims(model, mats, shell)
    bpy.ops.wm.save_as_mainfile(filepath=FILE)
    deps = bpy.context.evaluated_depsgraph_get()
    ev = shell.evaluated_get(deps).to_mesh()
    tri = sum(len(p.vertices) - 2 for p in ev.polygons)
    base = len(shell.data.polygons)
    print(f'HOUSE base editável: {base} faces, {len(shell.data.vertices)} vértices; com o barro: {tri} tri; '
          f'{tuple(round(v, 2) for v in shell.dimensions)} m; salvo {FILE}')


main()
