import bpy, os, sys
s=bpy.context.scene
s.render.engine='CYCLES'; s.cycles.device='CPU'; s.cycles.samples=32
s.render.threads_mode='FIXED'; s.render.threads=8
folder=os.path.dirname(bpy.data.filepath)
for label,camera in [('perspective','Camera_Reference'),('front','Camera_Front'),('top','Camera_Top')]:
    s.camera=next(o for o in s.objects if o.type=='CAMERA' and o.name.startswith(camera))
    s.render.filepath=os.path.join(folder,'refined_'+label+'.png')
    bpy.ops.render.render(write_still=True)
