"""Pinta uma peça gerada por IA com as NOSSAS vistas aprovadas (projeção).

Uso: blender -b --factory-startup -P blender_project_views.py -- <config.json>

O gerador 3D acerta o formato mas inventa a pintura de tudo que a vista de
frente não mostra. Aqui cada vista ortográfica aprovada (frente, trás,
esquerda, direita, cima) é projetada sobre a malha pelo lado dela: cada
ponto pega a cor da vista pra onde a superfície está virada (peso pela
normal). O resultado é gravado (bake) numa textura nova no UV da própria
malha, e a peça é exportada em GLB com essa textura.

config.json:
{
  "glb": ".../peca.glb", "out_dir": "...",
  "front_axis": "-Y",              # pra onde a frente da peça aponta no Blender
  "views": {                       # imagem + caixa do objeto na imagem (x0,y0,x1,y1 em px)
    "front": ["img.png", [x0,y0,x1,y1]], "back": [...], "left": [...],
    "right": [...], "top": [...]   # top: frente apontando pra ESQUERDA da imagem
  },
  "bottom_color": [0.18, 0.18, 0.17], "size": 2048
}
Convenções das vistas (as mesmas do Gemini): front = olhando a frente;
left = lado esquerdo da peça, frente pra esquerda da imagem; right = lado
direito, frente pra direita; back = olhando a traseira; top = de cima,
frente pra esquerda.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

cfg = json.load(open(sys.argv[sys.argv.index('--') + 1]))
os.makedirs(cfg['out_dir'], exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=cfg['glb'])
ob = [o for o in sc.objects if o.type == 'MESH'][0]

# 1. deixa a peça com a frente em +X, aplicando a rotação na malha
yaw = {'+X': 0, '-X': math.pi, '+Y': -math.pi / 2, '-Y': math.pi / 2}[cfg['front_axis']]
bpy.context.view_layer.objects.active = ob
for o in sc.objects:
    o.select_set(o == ob)
mw = Matrix.Rotation(yaw, 4, 'Z') @ ob.matrix_world
ob.parent = None
ob.matrix_world = mw
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
vs = [v.co for v in ob.data.vertices]
mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
print(f'PROJ bbox min={tuple(round(c, 3) for c in mn)} max={tuple(round(c, 3) for c in mx)}')

# 2. material de projeção
# frente em +X, cima +Z; esquerda da peça = +Y (olhando pra +X com cima +Z)
# cada vista: (eixo horizontal da imagem, sinal), (eixo vertical, sinal), (eixo da normal, sinal)
#   u cresce pra direita da imagem, v cresce pra cima
VIEWS = {
    'front': (('y', -1), ('z', 1), ('x', 1)),    # olhando a frente: direita da imagem = esquerda da peça (+Y)... ver nota
    'back': (('y', 1), ('z', 1), ('x', -1)),
    'left': (('x', -1), ('z', 1), ('y', 1)),     # frente (+X) pra esquerda da imagem
    'right': (('x', 1), ('z', 1), ('y', -1)),    # frente pra direita
    'top': (('x', -1), ('y', -1), ('z', 1)),     # frente pra esquerda; esquerda da peça (+Y) pra baixo
}
# nota da frente: olhando a frente da peça, o lado esquerdo dela aparece à
# DIREITA da imagem; esquerda = +Y, então u cresce com +Y → sinal +1
VIEWS['front'] = (('y', 1), ('z', 1), ('x', 1))
VIEWS['back'] = (('y', -1), ('z', 1), ('x', -1))

mat = bpy.data.materials.new('M_Projected')
mat.use_nodes = True
nt = mat.node_tree
N = nt.nodes
L = nt.links
for n in list(N):
    N.remove(n)
geo = N.new('ShaderNodeNewGeometry')
sep_p = N.new('ShaderNodeSeparateXYZ')
L.new(geo.outputs['Position'], sep_p.inputs[0])
sep_n = N.new('ShaderNodeSeparateXYZ')
L.new(geo.outputs['Normal'], sep_n.inputs[0])
AX = {'x': 0, 'y': 1, 'z': 2}
POWER = 4.0


def math_node(op, a, b=None, c=None):
    m = N.new('ShaderNodeMath')
    m.operation = op
    for i, v in enumerate((a, b, c)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            m.inputs[i].default_value = v
        else:
            L.new(v, m.inputs[i])
    return m.outputs[0]


def weight(axis, sign):
    s = math_node('MULTIPLY', sep_n.outputs[AX[axis]], float(sign))
    s = math_node('MAXIMUM', s, 0.0)
    return math_node('POWER', s, POWER)


acc = None
wsum = None
for name, ((ha, hs), (va, vs_), (na, ns)) in VIEWS.items():
    if name not in cfg['views']:
        continue
    path, (x0, y0, x1, y1) = cfg['views'][name]
    img = bpy.data.images.load(path)
    W, H = img.size
    # coordenada 0..1 ao longo do eixo (sinal decide o sentido)
    lo, hi = mn[AX[ha]], mx[AX[ha]]
    if hs > 0:
        u = math_node('MULTIPLY_ADD', sep_p.outputs[AX[ha]], 1 / (hi - lo), -lo / (hi - lo))
    else:
        u = math_node('MULTIPLY_ADD', sep_p.outputs[AX[ha]], -1 / (hi - lo), hi / (hi - lo))
    lo, hi = mn[AX[va]], mx[AX[va]]
    if vs_ > 0:
        v = math_node('MULTIPLY_ADD', sep_p.outputs[AX[va]], 1 / (hi - lo), -lo / (hi - lo))
    else:
        v = math_node('MULTIPLY_ADD', sep_p.outputs[AX[va]], -1 / (hi - lo), hi / (hi - lo))
    # para a caixa do objeto na imagem (v da imagem do Blender cresce pra cima)
    u = math_node('MULTIPLY_ADD', u, (x1 - x0) / W, x0 / W)
    v = math_node('MULTIPLY_ADD', v, (y1 - y0) / H, (H - y1) / H)
    comb = N.new('ShaderNodeCombineXYZ')
    L.new(u, comb.inputs[0])
    L.new(v, comb.inputs[1])
    tex = N.new('ShaderNodeTexImage')
    tex.image = img
    tex.extension = 'EXTEND'
    L.new(comb.outputs[0], tex.inputs['Vector'])
    w = weight(na, ns)
    # fundo branco da vista (peças finas como antenas, ou fora da silhueta):
    # essa vista não conta; quem pinta é outra vista ou a cor original
    sepc = N.new('ShaderNodeSeparateColor')
    L.new(tex.outputs['Color'], sepc.inputs[0])
    mn_rgb = math_node('MINIMUM', math_node('MINIMUM', sepc.outputs[0], sepc.outputs[1]), sepc.outputs[2])
    bg = math_node('MULTIPLY_ADD', mn_rgb, 1 / 0.05, -0.93 / 0.05)   # 0 até 0,93; 1 a partir de 0,98
    bg = math_node('MINIMUM', math_node('MAXIMUM', bg, 0.0), 1.0)
    w = math_node('MULTIPLY', w, math_node('SUBTRACT', 1.0, bg))
    sc_ = N.new('ShaderNodeVectorMath')
    sc_.operation = 'SCALE'
    L.new(tex.outputs['Color'], sc_.inputs[0])
    L.new(w, sc_.inputs['Scale'])
    if acc is None:
        acc, wsum = sc_.outputs[0], w
    else:
        add = N.new('ShaderNodeVectorMath')
        add.operation = 'ADD'
        L.new(acc, add.inputs[0])
        L.new(sc_.outputs[0], add.inputs[1])
        acc = add.outputs[0]
        wsum = math_node('ADD', wsum, w)

# embaixo: cor lisa escura
wb = weight('z', -1)
bc = N.new('ShaderNodeRGB')
bc.outputs[0].default_value = (*cfg.get('bottom_color', [0.18, 0.18, 0.17]), 1)
sb = N.new('ShaderNodeVectorMath')
sb.operation = 'SCALE'
L.new(bc.outputs[0], sb.inputs[0])
L.new(wb, sb.inputs['Scale'])
add = N.new('ShaderNodeVectorMath')
add.operation = 'ADD'
L.new(acc, add.inputs[0])
L.new(sb.outputs[0], add.inputs[1])
acc = add.outputs[0]
wsum = math_node('ADD', wsum, wb)

# cor original do gerador com peso pequeno: só aparece onde nenhuma vista
# pintou (antenas, frestas)
orig_img = None
for m in ob.data.materials:
    if m and m.use_nodes:
        b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if b and b.inputs['Base Color'].is_linked:
            src = b.inputs['Base Color'].links[0].from_node
            if src.type == 'TEX_IMAGE':
                orig_img = src.image
if orig_img is not None:
    uvn = N.new('ShaderNodeUVMap')
    uvn.uv_map = ob.data.uv_layers.active.name
    ot = N.new('ShaderNodeTexImage')
    ot.image = orig_img
    L.new(uvn.outputs['UV'], ot.inputs['Vector'])
    so = N.new('ShaderNodeVectorMath')
    so.operation = 'SCALE'
    L.new(ot.outputs['Color'], so.inputs[0])
    so.inputs['Scale'].default_value = 0.02
    add = N.new('ShaderNodeVectorMath')
    add.operation = 'ADD'
    L.new(acc, add.inputs[0])
    L.new(so.outputs[0], add.inputs[1])
    acc = add.outputs[0]
    wsum = math_node('ADD', wsum, 0.02)
wsum = math_node('MAXIMUM', wsum, 1e-4)
div = N.new('ShaderNodeVectorMath')
div.operation = 'DIVIDE'
L.new(acc, div.inputs[0])
comb = N.new('ShaderNodeCombineXYZ')
for i in range(3):
    L.new(wsum, comb.inputs[i])
L.new(comb.outputs[0], div.inputs[1])
emit = N.new('ShaderNodeEmission')
L.new(div.outputs[0], emit.inputs['Color'])
out = N.new('ShaderNodeOutputMaterial')
L.new(emit.outputs[0], out.inputs['Surface'])

# imagem de destino do bake (nó ativo)
size = cfg.get('size', 2048)
target = bpy.data.images.new('baked_basecolor', size, size)
tnode = N.new('ShaderNodeTexImage')
tnode.image = target
N.active = tnode
for n in N:
    n.select = False
tnode.select = True

old_mats = list(ob.data.materials)
ob.data.materials.clear()
ob.data.materials.append(mat)

# 3. bake
sc.render.engine = 'CYCLES'
sc.cycles.device = 'CPU'
sc.cycles.samples = 1
sc.render.bake.margin = 8
bpy.ops.object.bake(type='EMIT')
target.filepath_raw = os.path.join(cfg['out_dir'], 'basecolor_projected.png')
target.file_format = 'PNG'
target.save()
print('PROJ baked', target.filepath_raw)

# 4. material final: cor projetada + normal/metal/rugosidade do gerador
final = old_mats[0] if old_mats else bpy.data.materials.new('M_Final')
ob.data.materials.clear()
ob.data.materials.append(final)
fn = final.node_tree.nodes
bsdf = next(n for n in fn if n.type == 'BSDF_PRINCIPLED')
bt = fn.new('ShaderNodeTexImage')
bt.image = target
final.node_tree.links.new(bt.outputs['Color'], bsdf.inputs['Base Color'])
glb_out = os.path.join(cfg['out_dir'], 'projected.glb')
bpy.ops.export_scene.gltf(filepath=glb_out, export_format='GLB', use_selection=False)
print('PROJ exported', glb_out)
