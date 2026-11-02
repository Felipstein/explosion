"""O pano de bandeira do jogo (01/11/2026): a malha leve que o material M_FlagCloth balança no vento do mapa, na placa (o WPO).

O Felipe: "tudo que tem bandeira tem que ter física, as bandeiras têm que ter física; hoje nosso Technical é o único com bandeira com
física que balança no vento que o mapa oferece" e "cuidado com otimização [...] vários jogadores, IAs, unidades". A da Technical refaz a
malha na CPU a cada quadro (RTSUnitActor::StepFlag); com dezenas de prédios de 3–6 bandeiras isso seria CPU à toa. Aqui o pano é uma
malha fixa e o balanço sai do material (o vento do MPC_Wind, o tamanho e a fase pelo dado de primitiva): zero CPU por quadro no prédio.

Rodar (Blender sem janela, só CPU):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python Game/Tools/model_flag_cloth.py -- captures/game/models/flag_cloth

A malha: 1 m × 1 m, do mastro (x = 0) pra ponta (x = 1 m), do topo (z = 0) pra baixo (z = −1 m), 24 × 14 quadrados (~4 cm num pano de
1 m: a onda lisa no zoom do jogo, a mesma grade do pano da Technical); o UV = (u, v) da grade. O material estica pro tamanho de cada uma.
"""
import os
import sys

import bmesh
import bpy

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.path.join(os.getcwd(), 'flag_cloth')
os.makedirs(OUT, exist_ok=True)
NU, NV = 24, 14

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)
me = bpy.data.meshes.new('SM_FlagCloth')
bm = bmesh.new()
uv = bm.loops.layers.uv.new('UV')
vs = [[bm.verts.new((i / NU, 0.0, -j / NV)) for i in range(NU + 1)] for j in range(NV + 1)]
for j in range(NV):
    for i in range(NU):
        f = bm.faces.new([vs[j][i], vs[j + 1][i], vs[j + 1][i + 1], vs[j][i + 1]])
        for lp in f.loops:
            lp[uv].uv = (lp.vert.co.x, 1.0 + lp.vert.co.z)
bm.to_mesh(me)
bm.free()
mat = bpy.data.materials.new('FlagCloth')
me.materials.append(mat)
o = bpy.data.objects.new('SM_FlagCloth', me)
bpy.context.scene.collection.objects.link(o)
bpy.context.view_layer.objects.active = o
o.select_set(True)
path = os.path.join(OUT, 'SM_FlagCloth.glb')
bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                          export_materials='EXPORT', export_normals=True, export_texcoords=True)
print('MODEL SM_FlagCloth', NU * NV * 2, 'tris ->', path)
