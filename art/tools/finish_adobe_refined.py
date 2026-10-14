import bpy,bmesh,math,random,os,json
from mathutils import Vector
s=bpy.context.scene
h=next(o for o in s.objects if o.name.startswith('SM_Civ_AdobeHouse_A'))
col=h.users_collection[0]
if not h.get('finish_applied'):
    for o in list(col.objects):
        if 'Window' in o.name and o.type=='MESH':
            indices={i for p in o.data.polygons if p.material_index==0 for i in p.vertices}
            for i in indices:
                v=o.data.vertices[i]
                if 'Front' in o.name: v.co.y-=.14
                elif 'Back' in o.name: v.co.y+=.14
                elif 'Right' in o.name: v.co.x+=.14
        if 'Beams' in o.name:
            for v in o.data.vertices:
                if v.co.y < -4: v.co.y=-4+(v.co.y+4)*.58
    # Pequenas lascas do reboco nas quinas, sem crateras nem tijolo exposto.
    rng=random.Random(105)
    for i,(x,y,z,r) in enumerate([(3.99,-3.99,2.05,.105),(3.99,-3.99,2.19,.075),(-3.99,-3.99,.18,.085),(-1.6,-3.98,4.49,.07),(2.65,-3.98,4.49,.065),(3.99,1.4,4.48,.09)]):
        bm=bmesh.new(); bmesh.ops.create_icosphere(bm,subdivisions=1,radius=r)
        for v in bm.verts: v.co*=rng.uniform(.7,1.2); v.co+=Vector((x,y,z))
        me=bpy.data.meshes.new('WearCut'); bm.to_mesh(me); bm.free()
        cut=bpy.data.objects.new('WearCut',me); col.objects.link(cut)
        bpy.context.view_layer.update(); bpy.context.view_layer.objects.active=h
        mod=h.modifiers.new('Local plaster chip','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=cut
        bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.data.objects.remove(cut,do_unlink=True)
    # Pequena ondulacao no topo, visivel na silhueta e sem arredondar a parede.
    for v in h.data.vertices:
        if 4.36<v.co.z<4.6:
            v.co.z+=.017*math.sin(v.co.x*3.7+v.co.y*1.8)+.009*math.sin(v.co.y*8)
    h['finish_applied']=True
sun=next(o for o in s.objects if o.type=='LIGHT' and o.data.type=='SUN')
sun.rotation_euler=(math.radians(48),math.radians(-30),math.radians(-28)); sun.data.color=(1,.88,.72); sun.data.energy=3.2
bg=next(n for n in s.world.node_tree.nodes if n.type=='BACKGROUND'); bg.inputs[1].default_value=.65
s.view_settings.look='AgX - Medium High Contrast'
s.view_settings.exposure=.35
s.render.film_transparent=False
# Todas as malhas recebem UV editavel para o proximo bake/export.
for o in col.objects:
    if o.type!='MESH' or o.data.uv_layers: continue
    bpy.context.view_layer.update()
    for ob in bpy.context.selected_objects: ob.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active=o
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.015); bpy.ops.object.mode_set(mode='OBJECT')
bpy.context.view_layer.objects.active=h
for o in bpy.context.selected_objects: o.select_set(False)
h.select_set(True)
s.camera=next(o for o in s.objects if o.type=='CAMERA' and o.name.startswith('Camera_Reference'))
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
bm=bmesh.new();bm.from_mesh(h.data)
report={'footprint_m':[round(h.dimensions.x,4),round(h.dimensions.y,4)],'height_including_vent_m':round(h.dimensions.z,3),'origin':list(h.location),'shell_nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'mesh_triangles':sum(len(p.vertices)-2 for o in col.objects if o.type=='MESH' for p in o.data.polygons),'material_delivery':'Editable Blender procedural materials. No game texture bake included.'}
bm.free()
open(os.path.join(os.path.dirname(bpy.data.filepath),'refined_report.json'),'w').write(json.dumps(report,indent=2))
print(report)
