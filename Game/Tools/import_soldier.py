# (30/10, o soldado vestido) Traz pro jogo o que o Tools/soldier_dress.py fez no Blender: a farda pintada (T_Soldier_<F>_<slot>, sRGB, o
# alfa = a máscara do time) e as peças rígidas (SM_Soldier_<F>_<osso>, com a cor nos vértices, sem Nanite: peças de 10–30 cm), em
# /Game/Characters/Soldier/<F>. O soldier_<f>.json diz o osso e a origem de cada peça; o Units.json leva isso no "soldier" da unidade
# (o game.ps1 não mexe no Units.json: quem roda confere e escreve; este script só imprime o trecho).
#   SOLDIER_DIR=<pasta do soldier_dress> SOLDIER_FACTION=usa  game.ps1 py Tools/import_soldier.py
import json
import os

import unreal

SRC = os.environ.get('SOLDIER_DIR', os.path.join(unreal.Paths.project_dir(), 'Saved', 'SoldierDress'))
FAC = os.environ.get('SOLDIER_FACTION', 'usa').lower()
DEST = '/Game/Characters/Soldier/' + FAC.upper()
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(*a):
    unreal.log('SOLDIER ' + ' '.join(str(x) for x in a))


def imp(path, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths') or [])


meta = json.load(open(os.path.join(SRC, 'soldier_%s.json' % FAC), encoding='utf-8'))
paint = []
for slot, fname in sorted(meta['textures'].items(), key=lambda kv: int(kv[0])):
    name = os.path.splitext(fname)[0]
    imp(os.path.join(SRC, fname), name)
    t = unreal.load_asset(DEST + '/' + name)
    t.set_editor_property('srgb', True)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    eal.save_loaded_asset(t)
    paint.append(DEST + '/' + name + '.' + name)
    log('textura', t.get_path_name())
pieces = []
for p in meta['pieces']:
    name = p['mesh']
    paths = imp(os.path.join(SRC, name + '.glb'), name)
    mesh = None
    for q in paths:
        a = unreal.load_asset(q)
        if isinstance(a, unreal.StaticMesh):
            mesh = a
        elif isinstance(a, (unreal.MaterialInterface, unreal.Texture)):
            eal.delete_asset(q.split('.')[0])   # o material do importador: o jogo põe o M_Soldier
    if mesh is None:
        raise RuntimeError('SOLDIER %s: sem malha estática no GLB' % name)
    s = mesh.get_editor_property('nanite_settings')
    s.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', s)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    log('peça', mesh.get_path_name(), 'caixa (cm) %.1f..%.1f %.1f..%.1f %.1f..%.1f' % (b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, b.max.z))
    pieces.append({'mesh': mesh.get_path_name(), 'bone': p['bone'], 'origin_cm': [round(v, 2) for v in p['origin_cm']]})
out = {'paint': paint, 'pieces': pieces}
with open(os.path.join(SRC, 'soldier_%s_units.json' % FAC), 'w', encoding='utf-8') as f:
    json.dump(out, f, indent=1)
log('pronto', json.dumps(out))
