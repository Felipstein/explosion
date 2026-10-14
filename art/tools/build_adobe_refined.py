"""Reconstrucao fiel da ficha; executar no Blender. Preserva a cena original."""
import bpy, bmesh, math, os, random, json
from mathutils import Vector, Quaternion

ROOT = r'C:\Users\luisf\explosion'
OUT = ROOT + r'\captures\art3d\civ_house_1story'
# Reutiliza apenas os construtores de solidos, sem executar o gerador antigo.
source = open(ROOT + r'\art\tools\build_civ_house_1story.py', encoding='utf-8').read()
lib = {'__file__': ROOT + r'\art\tools\build_civ_house_1story.py'}
exec(source[:source.index('\ndef main():')], lib)
def apply_safe(o,mod):
    bpy.context.view_layer.update()
    for x in bpy.context.view_layer.objects:
        if x: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active=o
    bpy.ops.object.modifier_apply(modifier=mod.name)
lib['apply_mod']=apply_safe
scene = bpy.data.scenes.new('Adobe | Reference rebuild')
bpy.context.window.scene = scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
col = bpy.data.collections.new('House | 8m x 8m')
scene.collection.children.link(col)
lib['WIN_BACK']['cx'] = 0.0
lib['WIN_RIGHT'].update(cy=-0.8, w=0.74, h=2.45, sill=0.05)
lib['RIM'] = 0.25
lib['ROOF'] = 4.12
lib['RECESS'] = 0.32
lib['ROOFBOX'] = (-3.15,-2.15,-0.2,0.7)
shell = lib['build_base'](col)
shell.name = 'SM_Civ_AdobeHouse_A'
# Pequenas quinas gastas; nenhuma suavizacao grande de massinha.
m = shell.modifiers.new('Fine edge wear 20mm','BEVEL'); m.width=.02; m.segments=2
lib['apply_mod'](shell,m)
m = shell.modifiers.new('Surface tessellation','REMESH'); m.mode='VOXEL'; m.voxel_size=.075; m.use_smooth_shade=True
lib['apply_mod'](shell,m)
m = shell.modifiers.new('Remove voxel steps','SMOOTH'); m.factor=.35; m.iterations=2
lib['apply_mod'](shell,m)
for name,size,strength in [('Hand laid wall',.85,.035),('Broken plaster edges',.13,.014)]:
    m=shell.modifiers.new(name,'DISPLACE'); m.texture=lib['noise_tex']('Adobe '+name,size,2); m.strength=strength; m.texture_coords='GLOBAL'
    lib['apply_mod'](shell,m)
m=shell.modifiers.new('Game surface','DECIMATE'); m.ratio=.12
lib['apply_mod'](shell,m)
# Planta exata, origem no centro e no chao.
xs=[v.co.x for v in shell.data.vertices]; ys=[v.co.y for v in shell.data.vertices]
for v in shell.data.vertices:
    v.co.x=(v.co.x-min(xs))/(max(xs)-min(xs))*8-4
    v.co.y=(v.co.y-min(ys))/(max(ys)-min(ys))*8-4
    v.co.z=max(0,v.co.z)

def surface(name, base, wood=False):
    mat=bpy.data.materials.new(name); mat.use_nodes=True; mat.diffuse_color=(*base,1)
    n=mat.node_tree.nodes; l=mat.node_tree.links; p=next(x for x in n if x.type=='BSDF_PRINCIPLED')
    p.inputs['Roughness'].default_value=.91
    p.inputs['Specular IOR Level'].default_value=.22
    geo=n.new('ShaderNodeNewGeometry')
    vec=geo.outputs['Position']
    if wood:
        mp=n.new('ShaderNodeVectorMath'); mp.operation='MULTIPLY'; mp.inputs[1].default_value=(16,16,.65); l.new(vec,mp.inputs[0]); vec=mp.outputs[0]
    def noise(scale,detail):
        a=n.new('ShaderNodeTexNoise'); a.inputs['Scale'].default_value=scale; a.inputs['Detail'].default_value=detail; a.inputs['Roughness'].default_value=.76; l.new(vec,a.inputs['Vector']); return a
    broad=noise(.7,5); mid=noise(7,5); fine=noise(95,3)
    mix=n.new('ShaderNodeMixRGB'); mix.blend_type='MULTIPLY'; mix.inputs[0].default_value=.65; l.new(broad.outputs['Fac'],mix.inputs[1]); l.new(mid.outputs['Fac'],mix.inputs[2])
    ramp=n.new('ShaderNodeValToRGB'); ramp.color_ramp.elements[0].position=.14; ramp.color_ramp.elements[0].color=(*(c*.42 for c in base),1); ramp.color_ramp.elements[1].position=.64; ramp.color_ramp.elements[1].color=(*(c*1.28 for c in base),1)
    l.new(mix.outputs[0],ramp.inputs[0])
    sep=n.new('ShaderNodeSeparateXYZ'); l.new(geo.outputs['Position'],sep.inputs[0])
    dirt=n.new('ShaderNodeMapRange'); dirt.inputs['From Min'].default_value=0; dirt.inputs['From Max'].default_value=1.25; dirt.inputs['To Min'].default_value=.45; dirt.inputs['To Max'].default_value=1
    l.new(sep.outputs['Z'],dirt.inputs['Value'])
    dm=n.new('ShaderNodeMixRGB'); dm.blend_type='MULTIPLY'; dm.inputs[0].default_value=.8; l.new(ramp.outputs[0],dm.inputs[1]); l.new(dirt.outputs[0],dm.inputs[2]); l.new(dm.outputs[0],p.inputs['Base Color'])
    prev=None
    for tex,dist,strength in [(mid,.07,.65),(fine,.014,.7)]:
        b=n.new('ShaderNodeBump'); b.inputs['Distance'].default_value=dist if not wood else dist*.4; b.inputs['Strength'].default_value=strength; l.new(tex.outputs['Fac'],b.inputs['Height'])
        if prev: l.new(prev,b.inputs['Normal'])
        prev=b.outputs['Normal']
    l.new(prev,p.inputs['Normal'])
    return mat

plaster=surface('M_Adobe_Lime_Earth',(.43,.345,.235))
stone=surface('M_Adobe_Door_Sandstone',(.39,.345,.265))
wood=surface('M_Adobe_Aged_Timber',(.065,.049,.034),True)
dark=surface('M_Adobe_Recess',(.009,.011,.012))
roof=surface('M_Adobe_Roof_Dust',(.32,.305,.265))
shell.data.materials.clear(); shell.data.materials.append(plaster); shell.data.materials.append(roof)
for p in shell.data.polygons:
    if p.center.z>4 and p.normal.z>.65: p.material_index=1
details=lib['build_trims'](col,{'stone':stone,'wood':wood,'dark':dark},shell)
# As janelas precisam de fundo escuro na frente do reboco que fecha o rebaixo.
for name,box in [('WindowFrontShadow',(-2.355,-1.605,-3.715,-3.68,1.45,2.55)),('WindowRearShadow',(-.375,.375,3.68,3.715,1.45,2.4)),('SideDoorShadow',(3.68,3.715,-1.17,-.43,.05,2.5))]:
    o=lib['box_obj'](name,*box,col); o.data.materials.append(dark); o.parent=shell
# Soleira da porta lateral da vista em perspectiva.
o=lib['box_obj']('SideDoorThreshold',3.87,4.19,-1.31,-.29,0,.12,col); o.data.materials.append(stone); o.parent=shell

# Apresentacao separada, sem alterar a cena anterior.
stage=bpy.data.collections.new('Studio | do not export'); scene.collection.children.link(stage)
def obj(name,data):
    o=bpy.data.objects.new(name,data); stage.objects.link(o); return o
def camera(name,loc,target,ortho):
    d=bpy.data.cameras.new(name); d.type='ORTHO'; d.ortho_scale=ortho
    o=obj(name,d); o.location=loc; o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler(); return o
scene.camera=camera('Camera_Reference',(12,-16,12),(0,0,2),13.3)
camera('Camera_Front',(0,-20,2.5),(0,0,2.5),10)
camera('Camera_Top',(0,0,20),(0,0,0),10)
sun=obj('Sun',bpy.data.lights.new('Sun','SUN')); sun.data.energy=3; sun.data.angle=.09; sun.rotation_euler=(math.radians(28),math.radians(-24),math.radians(-28))
area=obj('Sky fill',bpy.data.lights.new('Sky fill','AREA')); area.location=(0,-4,12); area.data.energy=350; area.data.shape='DISK'; area.data.size=10
scene.world=bpy.data.worlds.new('Neutral studio'); scene.world.use_nodes=True; bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND'); bg.inputs[0].default_value=(.35,.38,.42,1); bg.inputs[1].default_value=.45
scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=48; scene.cycles.use_denoising=True
scene.render.resolution_x=1200; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=True
scene.view_settings.view_transform='AgX'
scene.render.filepath=OUT+r'\refined_perspective.png'
for o in bpy.context.view_layer.objects: o.select_set(False)
shell.select_set(True); bpy.context.view_layer.objects.active=shell
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
        area.spaces.active.overlay.show_overlays=False
# Copia dos marcadores de escala para inspecao opcional, escondida no render.
scalecol=bpy.data.collections.new('Scale references | optional'); scene.collection.children.link(scalecol)
for name in ['Humano_1.8m','Tanque_USA_7.9m']:
    original=bpy.data.objects.get(name)
    if original:
        cp=original.copy(); scalecol.objects.link(cp); cp.location.x+=10; cp.hide_render=True
scalecol.hide_viewport=True
shell['footprint_m']='8 x 8'; shell['reference']='ref_sheet_full.png'; shell['front_axis']='-Y'
bpy.ops.wm.save_as_mainfile(filepath=OUT+r'\civ_house_refined.blend')
print('CREATED',shell.name,'triangles',sum(len(p.vertices)-2 for o in col.objects if o.type=='MESH' for p in o.data.polygons),'dimensions',list(shell.dimensions))
