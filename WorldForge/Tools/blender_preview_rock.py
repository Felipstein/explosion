"""A foto de conferência da pedra quebrada (16/10): a pedra inteira (FBX) e a quebrada (GLB) lado a lado, de cima e
inclinado, com sol. blender -b -P blender_preview_rock.py -- <pedra.fbx> <quebrada.glb> <saída.png>"""
import bpy, sys, math, bmesh
from mathutils import Vector
argv = sys.argv[sys.argv.index('--') + 1:]
whole, broken, out = argv
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=whole)
w = [o for o in bpy.context.scene.objects if o.type == 'MESH']
keep = max(w, key=lambda x: len(x.data.vertices))
for x in list(bpy.context.scene.objects):
    if x is not keep:
        bpy.data.objects.remove(x, do_unlink=True)
keep.parent = None
keep.location.x -= 1.2
bpy.ops.import_scene.gltf(filepath=broken)
for x in bpy.context.scene.objects:
    if x.type == 'MESH' and x is not keep:
        x.parent = None
        x.location.x += 1.2
scn = bpy.context.scene
scn.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
scn.render.resolution_x, scn.render.resolution_y = 1600, 900
wd = bpy.data.worlds.new('w'); scn.world = wd; wd.use_nodes = True
wd.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.62, 0.75, 1.0)
wd.node_tree.nodes['Background'].inputs[1].default_value = 0.6
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); scn.collection.objects.link(sun)
sun.data.energy = 4.0; sun.rotation_euler = (math.radians(50), 0, math.radians(35))
gp = bpy.data.objects.new('ground', bpy.data.meshes.new('g'))
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=20); bm.to_mesh(gp.data); bm.free()
m = bpy.data.materials.new('sand'); m.use_nodes = True
m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.45, 0.33, 0.2, 1)
gp.data.materials.append(m); scn.collection.objects.link(gp)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); scn.collection.objects.link(cam); scn.camera = cam
cam.location = Vector((0.0, -3.4, 3.0))
d = Vector((0.0, 0.0, 0.2)) - cam.location
cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
cam.data.lens = 35
scn.render.filepath = out
bpy.ops.render.render(write_still=True)
print('PREVIEW', out)
