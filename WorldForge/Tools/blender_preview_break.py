"""A foto de conferência do toco e do tronco quebrado (16/10): os dois .glb lado a lado, de perto na quebra, com sol.
  blender -b -P blender_preview_break.py -- <stump.glb> <fallen.glb> <saída.png>"""
import bpy, sys, math
from mathutils import Vector
argv = sys.argv[sys.argv.index('--') + 1:]
stump, fallen, out = argv
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=stump)
s_objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.import_scene.gltf(filepath=fallen)
f_objs = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o not in s_objs]
# o tronco caído deitado ao lado do toco, com a quebra virada pra câmera
for o in f_objs:
    o.parent = None
    o.rotation_mode = 'XYZ'
    o.rotation_euler = (math.radians(-90), 0, 0)
    o.location = (1.5, 0.3, 0.45)
scn = bpy.context.scene
scn.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
scn.render.resolution_x, scn.render.resolution_y = 1600, 900
w = bpy.data.worlds.new('w'); scn.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.62, 0.75, 1.0)
w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); scn.collection.objects.link(sun)
sun.data.energy = 4.0; sun.rotation_euler = (math.radians(50), 0, math.radians(35))
gp = bpy.data.objects.new('ground', bpy.data.meshes.new('g'))
import bmesh
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=20); bm.to_mesh(gp.data); bm.free()
mat = bpy.data.materials.new('sand'); mat.use_nodes = True
mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.45, 0.33, 0.2, 1)
gp.data.materials.append(mat); scn.collection.objects.link(gp)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); scn.collection.objects.link(cam); scn.camera = cam
cam.location = Vector((0.7, -2.6, 2.1))
d = Vector((0.75, 0.1, 0.6)) - cam.location
cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
cam.data.lens = 35
scn.render.filepath = out
bpy.ops.render.render(write_still=True)
print('PREVIEW', out)
