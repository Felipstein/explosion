"""(01/11, a 3ª volta da infantaria) Traz pro jogo o soldado de malha única do Tools/soldier_outfit.py: o FBX com o corpo vestido, a cabeça
humana e o equipamento, no MESMO esqueleto do Manny (o SK_Mannequin: as animações tocam sem retarget), com a cor nos vértices (os slots
VC e VC_Team; o jogo põe o M_Soldier pela cor dos vértices e a cor do time no VC_Team: RTSUnitActor).

  OUTFIT_FBX=<o .fbx> OUTFIT_TAG=usa  game.ps1 py Tools/import_outfit.py
  OUTFIT_LIST="usa=<fbx>;china_at=<fbx>;..."  game.ps1 py Tools/import_outfit.py      (vários numa abertura só)
Sai em /Game/Characters/Soldier/V3/<TAG>/SKM_Soldier_<TAG>. O importador clássico de FBX (o Interchange ignora as opções do FbxImportUI).
"""
import os

import unreal


def log(*a):
    unreal.log('OUTFIT ' + ' '.join(str(x) for x in a))


def import_one(FBX, TAG):
    TAG = TAG.upper()
    DEST = '/Game/Characters/Soldier/V3/' + TAG
    NAME = 'SKM_Soldier_' + TAG
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX False')
    skel = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
    if not skel:
        raise RuntimeError('OUTFIT sem o SK_Mannequin')
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('skeleton', skel)
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('create_physics_asset', False)
    sk = ui.get_editor_property('skeletal_mesh_import_data')
    sk.set_editor_property('import_morph_targets', False)
    sk.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    sk.set_editor_property('update_skeleton_reference_pose', False)
    sk.set_editor_property('use_t0_as_ref_pose', False)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', FBX)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', NAME)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    task.set_editor_property('options', ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths') or [])
    log('importado', paths)
    mesh = None
    for q in paths:
        a = unreal.load_asset(q)
        if isinstance(a, unreal.SkeletalMesh):
            mesh = a
    if not mesh:
        raise RuntimeError('OUTFIT o FBX não virou malha esquelética')
    b = mesh.get_bounds()
    log('malha', mesh.get_path_name(), 'esqueleto', mesh.get_editor_property('skeleton').get_path_name(),
        'caixa', b.origin, b.box_extent, 'materiais', [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')])
    # o material dos slots: o M_Soldier (o jogo troca por instâncias dinâmicas; aqui é só o que o editor mostra)
    ms = unreal.load_asset('/Game/Materials/M_Soldier')
    mats = mesh.get_editor_property('materials')
    for m in mats:
        m.set_editor_property('material_interface', ms)
    mesh.set_editor_property('materials', mats)
    # (02/11) os níveis de detalhe: a malha tem 70–100 mil vértices (o rosto do MetaHuman, as dobras, o equipamento) e o RTS põe dezenas
    # na tela; a redução da Unreal (o plugin SkeletalReduction) gera os LOD 1–3 com os padrões dela (metade dos triângulos a cada um)
    try:
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        ok = sub.regenerate_lod(mesh, 4, False, False)
        n = sub.get_lod_count(mesh)
        log('lod', ok, n, [sub.get_num_verts(mesh, k) for k in range(n)])
    except Exception as e:
        log('lod FALHOU', e)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log('pronto', mesh.get_path_name())


unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX False')
if os.environ.get('OUTFIT_LIST'):
    JOBS = [tuple(x.split('=', 1)) for x in os.environ['OUTFIT_LIST'].split(';') if x.strip()]
else:
    JOBS = [(os.environ.get('OUTFIT_TAG', 'usa'), os.environ['OUTFIT_FBX'])]
for tag, fbx in JOBS:
    try:
        import_one(fbx, tag)
    except Exception as e:   # um que falha não derruba os outros
        log('FALHOU', tag, e)
