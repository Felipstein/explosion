"""Gera o modelo-base do Blender pra modelar peças do jogo (docs/PRODUCAO_3D.md §9).

Uso (sem abrir o Blender):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python art/tools/make_template.py

Sai em captures/art3d/RTS_Base.blend (fora do git: leva o tanque, ~20 MB) e uma prévia
captures/art3d/RTS_Base_preview.png renderizada da câmera do jogo (Cycles na CPU).
Mude este script, não o arquivo.

O que vem na cena:
- a grade da simulação: quadrados de 4 m (Game/Config/RTS/Maps.json, cell_m), no chão Z = 0;
- o tanque USA com 7,9 m (as peças de captures/concept/30_to_3d/usa_tank/parts_v2), frente pra −Y;
- um humano de 1,8 m;
- a planta do 1º modelo: caixa 8 × 8 × 4 m (a casa de adobe, §10), só em arame;
- a câmera do jogo (Game/Config/DefaultGame.ini: 37,5° de inclinação, campo horizontal de 50°,
  61,1 m de altura) e a de zoom perto (31,6 m), olhando a planta na diagonal como no jogo;
- o sol dos game shots e o céu de poeira do deserto dourado (light());
- o chão de areia de duna do jogo (sand_material()).
"""
import math
import os

import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
TANK = os.path.join(REPO, 'captures', 'concept', '30_to_3d', 'usa_tank', 'parts_v2')
OUT = os.path.join(REPO, 'captures', 'art3d')
TEX = os.path.join(OUT, 'textures')
TEX_SRC = os.path.join(REPO, 'WorldForge', 'Intermediate', 'Assets')   # as 4K do Poly Haven (CC0)
CELL_M = 4.0
TANK_LEN_M = 7.9
PITCH_DEG, FOV_DEG = 37.5, 50.0
CAM_AZ_DEG = 225.0                             # de onde as câmeras olham: na diagonal, do lado da frente (−Y)
HEIGHTS = {'Camera_Jogo': 61.1, 'Camera_Perto': 31.6}

# O sol da receita aprovada do deserto dourado (Game/Tools/look_recipes/desert_golden_hour_v2_aprovada.json):
# 20° de altura, em contraluz de três-quartos, 42° à direita da frente da câmera (as sombras caem pra baixo
# e pra esquerda da tela, como em todos os game shots do deserto), disco de 0,8°.
SUN_ELEV_DEG, SUN_FROM_CAMERA_DEG, SUN_ANGLE_DEG = 20.0, -42.0, 0.8
# O sol e o céu saem do mesmo modelo de atmosfera do Blender (MULTIPLE_SCATTERING) com a poeira do deserto
# (aerosol 10): a cor e a força do sol (W/m²) foram medidas no próprio céu, com o disco menos sem o disco,
# num chão branco (24/10). Por isso o sol é a luz já filtrada pela atmosfera, laranja a 20°.
SKY_AEROSOL = 10.0
SUN_RGB_W = (101.95, 74.31, 41.90)
# O que leva o quadro pra foto aprovada do jogo (docs/aprovados/look/2026-10-17_ceu_uma_luz.jpg, medida a
# areia no sol e na sombra dos tanques): a luz do céu reforçada como o skylight 4,0 da receita, e o
# ganho quente dela no balanço de branco. Medido (24/10): sol 195/160/130 contra 205/153/113 do jogo,
# sombra 150/122/97 contra 156/122/95.
SKY_STRENGTH = 2.5
EXPOSURE, WHITE_BALANCE_K = -3.6, 9500.0


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 1.0
    s.render.resolution_x, s.render.resolution_y = 1920, 1080


def collection(name):
    c = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(c)
    return c


def link(ob, col):
    for c in ob.users_collection:
        c.objects.unlink(ob)
    col.objects.link(ob)


def material(name, rgb, rough=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1.0)
    b.inputs['Roughness'].default_value = rough
    return m


def sand_textures():
    """As texturas da areia em 2K (as 4K pesam 60 MB): o grão do gravelly_sand e os montinhos do
    aerial_beach_01. A normal vira o padrão OpenGL do Blender (o verde invertido). Só faz o que falta."""
    os.makedirs(TEX, exist_ok=True)
    for name, flip in (('gravelly_sand_diff', False), ('gravelly_sand_nor', True), ('gravelly_sand_arm', False),
                       ('aerial_beach_01_diff', False), ('aerial_beach_01_nor', True)):
        dst = os.path.join(TEX, name + ('_gl' if flip else '') + '_2k.jpg')
        if os.path.exists(dst):
            continue
        src_name = name + ('_dx' if flip else '') + '_4k.jpg'
        img = bpy.data.images.load(os.path.join(TEX_SRC, name.rsplit('_', 1)[0], src_name))
        img.scale(2048, 2048)
        if flip:
            px = np.empty(2048 * 2048 * 4, dtype=np.float32)
            img.pixels.foreach_get(px)
            px[1::4] = 1.0 - px[1::4]
            img.pixels.foreach_set(px)
        img.save(filepath=dst, quality=92)
        bpy.data.images.remove(img)


def sand_material():
    """A areia de duna do deserto dourado (docs/RECEITAS_DO_LOOK.md, a v2 "duna" escolhida em 16/10): o albedo
    físico 0,45/0,26/0,15 e a textura só no desenho, como em Game/Tools/look_materials/sand_desert_v2.json:
    o grão do gravelly_sand a 1,3 m (a cor ±25% em volta da média dela, a normal ×1,35, a oclusão
    lerp(1, ao², 0,7)), os montinhos do aerial_beach_01 a 6 m (normal ×0,6) e a variação larga dele a 40 m."""
    sand_textures()
    m = bpy.data.materials.new('M_Areia')
    m.use_nodes = True
    nt = m.node_tree
    N, L = nt.nodes, nt.links
    bsdf = N['Principled BSDF']
    bsdf.inputs['Roughness'].default_value = 0.95
    co = N.new('ShaderNodeTexCoord')

    def tex(name, size_m, color=True):         # o chão tem 1 unidade = 1 m: a coordenada do objeto já é em metros
        mp = N.new('ShaderNodeMapping')
        mp.inputs['Scale'].default_value = (1 / size_m,) * 3
        L.new(co.outputs['Object'], mp.inputs['Vector'])
        t = N.new('ShaderNodeTexImage')
        t.image = bpy.data.images.load(os.path.join(TEX, name), check_existing=True)
        if not color:
            t.image.colorspace_settings.name = 'Non-Color'
        L.new(mp.outputs['Vector'], t.inputs['Vector'])
        return t

    def op(kind, a, b):
        n = N.new('ShaderNodeMath')
        n.operation = kind
        for i, v in enumerate((a, b)):
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                L.new(v, n.inputs[i])
        return n.outputs[0]

    def lum(t, mean):                          # o brilho da textura sobre a média dela (medida): 1 = o tom médio
        bw = N.new('ShaderNodeRGBToBW')
        L.new(t.outputs['Color'], bw.inputs[0])
        return op('DIVIDE', bw.outputs[0], mean)

    def lerp1(v, k):                           # 1 + (v − 1)·k
        return op('ADD', op('MULTIPLY', op('SUBTRACT', v, 1.0), k), 1.0)

    grain = lerp1(lum(tex('gravelly_sand_diff_2k.jpg', 1.3), 0.1851), 0.25)
    broad = lum(tex('aerial_beach_01_diff_2k.jpg', 40.0), 0.2440)
    arm = N.new('ShaderNodeSeparateColor')
    L.new(tex('gravelly_sand_arm_2k.jpg', 1.3, color=False).outputs['Color'], arm.inputs[0])
    ao = lerp1(op('POWER', arm.outputs[0], 2.0), 0.7)
    albedo = N.new('ShaderNodeVectorMath')
    albedo.operation = 'SCALE'
    albedo.inputs[0].default_value = (0.45, 0.26, 0.15)
    L.new(op('MULTIPLY', op('MULTIPLY', grain, broad), ao), albedo.inputs['Scale'])
    L.new(albedo.outputs[0], bsdf.inputs['Base Color'])

    # as duas normais somadas sobre a do chão: n = normalize(n1 + n2 − N)
    vec = []
    for name, size_m, strength in (('gravelly_sand_nor_gl_2k.jpg', 1.3, 1.35), ('aerial_beach_01_nor_gl_2k.jpg', 6.0, 0.6)):
        nm = N.new('ShaderNodeNormalMap')
        nm.inputs['Strength'].default_value = strength
        L.new(tex(name, size_m, color=False).outputs['Color'], nm.inputs['Color'])
        vec.append(nm.outputs[0])
    geo = N.new('ShaderNodeNewGeometry')
    out = None
    for kind, a, b in (('ADD', vec[0], vec[1]), ('SUBTRACT', None, geo.outputs['Normal']), ('NORMALIZE', None, None)):
        n = N.new('ShaderNodeVectorMath')
        n.operation = kind
        L.new(a if out is None else out, n.inputs[0])
        if b is not None:
            L.new(b, n.inputs[1])
        out = n.outputs[0]
    L.new(out, bsdf.inputs['Normal'])
    return m


def grid(col):
    # 200 m de lado em quadrados de 4 m: só as linhas (arame), mais um chão de areia logo abaixo
    n = int(200 / CELL_M)
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=n, y_subdivisions=n, size=200.0, location=(0, 0, 0.005))
    g = bpy.context.active_object
    g.name = 'Grade_4m'
    g.display_type = 'WIRE'
    g.hide_render = True
    link(g, col)
    bpy.ops.mesh.primitive_plane_add(size=400.0, location=(0, 0, 0))
    floor = bpy.context.active_object
    floor.name = 'Chao_Areia'
    floor.data.materials.append(sand_material())
    floor.hide_select = True
    link(floor, col)


def tank(col):
    before = set(bpy.data.objects)
    for f in sorted(os.listdir(TANK)):
        if f.endswith('.glb'):
            bpy.ops.import_scene.gltf(filepath=os.path.join(TANK, f))
    parts = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    for o in [o for o in bpy.data.objects if o not in before]:
        link(o, col)
    # caixa do conjunto: escala pra 7,9 m no lado comprido, no chão, centrado
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in parts for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    size = mx - mn
    root = bpy.data.objects.new('Tanque_USA_7.9m', None)
    col.objects.link(root)
    root.location = ((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z)
    bpy.context.view_layer.update()
    for o in [o for o in bpy.data.objects if o not in before and o.parent is None and o is not root]:
        o.parent = root
        o.matrix_parent_inverse = root.matrix_world.inverted()
    s = TANK_LEN_M / max(size.x, size.y)
    root.scale = (s, s, s)
    # o cano: de que lado do centro ele está → gira pra frente (−Y)
    barrel = [o for o in parts if 'arrel' in o.name]
    if barrel:
        bc = sum((barrel[0].matrix_world @ Vector(c) for c in barrel[0].bound_box), Vector()) / 8
        d = Vector((bc.x - root.location.x, bc.y - root.location.y, 0))
        root.rotation_euler[2] = math.atan2(-1, 0) - math.atan2(d.y, d.x)
    root.location = (12.0, 0.0, 0.0)          # ao lado da planta, sem encostar
    for o in parts:
        o.hide_select = True


def human(col):
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=1.8, location=(6.0, -6.0, 0.9))
    h = bpy.context.active_object
    h.name = 'Humano_1.8m'
    h.data.materials.append(material('M_Humano', (0.6, 0.15, 0.1)))
    link(h, col)


def footprint(col):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 2.0))
    b = bpy.context.active_object
    b.name = 'Planta_8x8x4m'
    b.scale = (8.0, 8.0, 4.0)
    b.display_type = 'WIRE'
    b.hide_render = True
    link(b, col)


def cameras(col):
    target = bpy.data.objects.new('Alvo_Camera', None)
    col.objects.link(target)
    target.location = (4.0, 0.0, 0.0)          # entre a planta e o tanque
    p = math.radians(PITCH_DEG)
    az = math.radians(CAM_AZ_DEG)
    for name, h in HEIGHTS.items():
        cam = bpy.data.cameras.new(name)
        cam.sensor_fit = 'HORIZONTAL'
        cam.angle = math.radians(FOV_DEG)
        cam.clip_end = 2000.0
        ob = bpy.data.objects.new(name, cam)
        col.objects.link(ob)
        d = h / math.sin(p)
        ob.location = target.location + Vector((math.cos(az) * math.cos(p), math.sin(az) * math.cos(p), math.sin(p))) * d
        t = ob.constraints.new('TRACK_TO')
        t.target = target
        t.track_axis = 'TRACK_NEGATIVE_Z'
        t.up_axis = 'UP_Y'
    bpy.context.scene.camera = bpy.data.objects['Camera_Jogo']


def light(col):
    # a direção do sol (azimute, anti-horário a partir de +X): a frente da câmera girada pro lado do sol
    cam_forward = CAM_AZ_DEG - 180.0
    az = math.radians(cam_forward + SUN_FROM_CAMERA_DEG)
    el = math.radians(SUN_ELEV_DEG)
    sun = bpy.data.lights.new('Sol', 'SUN')
    sun.energy = max(SUN_RGB_W)
    sun.color = [v / max(SUN_RGB_W) for v in SUN_RGB_W]
    sun.angle = math.radians(SUN_ANGLE_DEG)
    ob = bpy.data.objects.new('Sol', sun)
    col.objects.link(ob)
    # girar só em Y (altura: 90° − Y) e em Z (direção): o céu segue o sol pelos drivers abaixo
    ob.rotation_euler = (0.0, math.pi / 2 - el, az)
    w = bpy.data.worlds.new('Ceu_Deserto')
    w.use_nodes = True
    nt = w.node_tree
    sky = nt.nodes.new('ShaderNodeTexSky')
    sky.sky_type = 'MULTIPLE_SCATTERING'
    sky.sun_disc = False                       # o sol é a luz "Sol"; o disco do céu contaria duas vezes
    sky.aerosol_density = SKY_AEROSOL
    nt.links.new(sky.outputs['Color'], nt.nodes['Background'].inputs['Color'])
    nt.nodes['Background'].inputs['Strength'].default_value = SKY_STRENGTH
    # no céu do Blender o sol de rotação 0 fica em +Y e a rotação gira pro +X (medido): rotação = 90° − azimute
    for prop, axis in (('sun_elevation', 1), ('sun_rotation', 2)):
        fc = sky.driver_add(prop)
        v = fc.driver.variables.new()
        v.name = 'a'
        v.targets[0].id = ob
        v.targets[0].data_path = f'rotation_euler[{axis}]'
        fc.driver.expression = 'pi / 2 - a'
    s = bpy.context.scene
    s.world = w
    s.render.engine = 'CYCLES'
    s.cycles.device = 'GPU'                    # sem placa configurada nas preferências, o Cycles usa a CPU
    s.cycles.samples = 128
    s.view_settings.view_transform = 'AgX'
    s.view_settings.exposure = EXPOSURE
    s.view_settings.use_white_balance = True
    s.view_settings.white_balance_temperature = WHITE_BALANCE_K


def main():
    clear()
    ref = collection('Referencia (nao exportar)')
    grid(ref)
    tank(ref)
    human(ref)
    footprint(ref)
    cameras(ref)
    light(ref)
    collection('Modelo')                      # o que você modela vai aqui
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, 'RTS_Base.blend')
    bpy.ops.wm.save_as_mainfile(filepath=path)
    print('TEMPLATE salvo', path)
    # prévia da câmera do jogo (CPU, rápida), pra conferir a escala sem abrir o Blender
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.device = 'CPU'
    s.cycles.samples = 16
    s.render.resolution_percentage = 50
    s.render.filepath = os.path.join(OUT, 'RTS_Base_preview.png')
    bpy.ops.render.render(write_still=True)
    print('TEMPLATE prévia', s.render.filepath)


main()
