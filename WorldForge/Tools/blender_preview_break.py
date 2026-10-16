"""A foto de conferência do toco e do tronco quebrado (16/10): os dois .glb lado a lado, de perto na quebra, com sol.
A quebra com o material do UE refeito em nós (a palha com os pontos dos feixes pelo UV 0, o anel marrom e o tipo pelo
UV 1: a tampa, a lasca, o tufo), no Cycles da CPU (a placa é dos testes). Duas fotos: de perto e do alto (a câmera do jogo
mais perto que a de 67 m).
  blender -b -P blender_preview_break.py -- <stump.glb> <fallen.glb> <saída.png> [<saída do alto.png>]"""
import bpy, sys, math, bmesh
from mathutils import Vector
argv = sys.argv[sys.argv.index('--') + 1:]
stump, fallen, out = argv[0], argv[1], argv[2]
out_top = argv[3] if len(argv) > 3 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=stump)
s_objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.import_scene.gltf(filepath=fallen)
f_objs = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o not in s_objs]
# o tronco caído deitado ao lado do toco, com a quebra virada pra câmera
for o in f_objs:
    o.parent = None
    o.rotation_mode = 'XYZ'
    o.rotation_euler = (math.radians(-90), 0, math.radians(8))
    o.location = (1.3, 0.2, 0.25)


def break_nodes(m, uvn0, uvn1):
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out_n = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = 0.85
    nt.links.new(bsdf.outputs[0], out_n.inputs[0])
    u0 = nt.nodes.new('ShaderNodeUVMap'); u0.uv_map = uvn0
    u1 = nt.nodes.new('ShaderNodeUVMap'); u1.uv_map = uvn1
    sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(u1.outputs[0], sep.inputs[0])
    sep0 = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(u0.outputs[0], sep0.inputs[0])
    vor = nt.nodes.new('ShaderNodeTexVoronoi'); vor.inputs['Scale'].default_value = 180.0
    nt.links.new(u0.outputs[0], vor.inputs['Vector'])

    def math_node(op, a, b=None, clamp=False):
        n = nt.nodes.new('ShaderNodeMath'); n.operation = op; n.use_clamp = clamp
        for i, x in enumerate((a, b)):
            if x is None:
                continue
            if isinstance(x, (int, float)):
                n.inputs[i].default_value = x
            else:
                nt.links.new(x, n.inputs[i])
        return n.outputs[0]

    def ss(e0, e1, x):
        n = nt.nodes.new('ShaderNodeMapRange'); n.interpolation_type = 'SMOOTHSTEP'
        n.inputs['From Min'].default_value = e0; n.inputs['From Max'].default_value = e1
        nt.links.new(x, n.inputs['Value'])
        return n.outputs[0]

    def mixc(a, b, t):
        n = nt.nodes.new('ShaderNodeMix'); n.data_type = 'RGBA'
        nt.links.new(t, n.inputs['Factor'])
        for sock, x in ((n.inputs[6], a), (n.inputs[7], b)):
            if isinstance(x, tuple):
                sock.default_value = x
            else:
                nt.links.new(x, sock)
        return n.outputs[2]

    rr, flag = sep.outputs[0], sep.outputs[1]
    # os pontos dos feixes: mais densos na borda (o valor do ponto sorteado contra a densidade)
    dot = math_node('SUBTRACT', 1.0, ss(0.15, 0.3, vor.outputs['Distance']))
    cellv = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(vor.outputs['Color'], cellv.inputs[0])
    dens = math_node('ADD', 0.35, math_node('MULTIPLY', 0.55, ss(0.3, 0.95, rr)))
    keep = math_node('LESS_THAN', cellv.outputs[0], dens)
    dots = math_node('MULTIPLY', dot, keep)
    # a palha manchada (a fibra oxida em manchas largas) com os pontos dos feixes
    mot = nt.nodes.new('ShaderNodeTexNoise'); mot.inputs['Scale'].default_value = 9.0
    nt.links.new(u0.outputs[0], mot.inputs['Vector'])
    straw = mixc((0.30, 0.21, 0.11, 1.0), (0.46, 0.37, 0.22, 1.0), ss(0.35, 0.65, mot.outputs['Fac']))
    cap = mixc(straw, (0.12, 0.08, 0.045, 1.0), math_node('MULTIPLY', dots, 0.75))
    cap = mixc(cap, (0.16, 0.11, 0.06, 1.0), math_node('MULTIPLY', ss(0.0, 0.4, flag), 0.85))   # a fresta entre os tufos
    cap = mixc(cap, (0.11, 0.065, 0.035, 1.0), ss(0.84, 0.93, rr))
    # as fibras ao longo (a lasca e o tufo): faixas pelo atravessado, onduladas ao longo
    wav = math_node('SINE', math_node('MULTIPLY', sep0.outputs[1], 20.0))
    ph = math_node('ADD', math_node('MULTIPLY', sep0.outputs[0], 565.0), math_node('MULTIPLY', wav, 3.0))
    streak = math_node('ADD', 0.5, math_node('MULTIPLY', 0.5, math_node('SINE', ph)))
    fib = mixc((0.50 * 0.7, 0.40 * 0.7, 0.24 * 0.7, 1.0), (0.50 * 1.05, 0.40 * 1.05, 0.24 * 1.05, 1.0), streak)
    tuft = mixc((0.20 * 0.7, 0.12 * 0.7, 0.06 * 0.7, 1.0), (0.20 * 1.1, 0.12 * 1.1, 0.06 * 1.1, 1.0), streak)
    col = mixc(cap, fib, ss(0.45, 0.6, flag))
    col = mixc(col, tuft, ss(1.4, 1.6, flag))
    nt.links.new(col, bsdf.inputs['Base Color'])


for o in s_objs + f_objs:
    for m in o.data.materials:
        if m and 'break' in m.name.lower():
            m.use_nodes = True
            break_nodes(m, o.data.uv_layers[0].name, o.data.uv_layers[-1].name)
scn = bpy.context.scene
scn.render.engine = 'CYCLES'
scn.cycles.device = 'CPU'
scn.cycles.samples = 48
scn.cycles.use_denoising = True
scn.render.resolution_x, scn.render.resolution_y = 1600, 900
w = bpy.data.worlds.new('w'); scn.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.62, 0.75, 1.0)
w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); scn.collection.objects.link(sun)
sun.data.energy = 4.0; sun.rotation_euler = (math.radians(50), 0, math.radians(35))
gp = bpy.data.objects.new('ground', bpy.data.meshes.new('g'))
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=20); bm.to_mesh(gp.data); bm.free()
mat = bpy.data.materials.new('sand'); mat.use_nodes = True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.45, 0.33, 0.2, 1)
gp.data.materials.append(mat); scn.collection.objects.link(gp)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); scn.collection.objects.link(cam); scn.camera = cam


def shot(loc, look, lens, path):
    cam.location = Vector(loc)
    d = Vector(look) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = lens
    scn.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print('PREVIEW', path)


shot((0.65, -1.9, 2.5), (0.6, 0.15, 0.7), 35, out)
if out_top:
    # do alto, como o jogo perto (a 67 m a palmeira inteira tem ~60 px; aqui a quebra com uns 40 px)
    scn.render.resolution_x, scn.render.resolution_y = 960, 540
    shot((0.6, -9.0, 12.0), (0.6, 0.0, 0.6), 50, out_top)
