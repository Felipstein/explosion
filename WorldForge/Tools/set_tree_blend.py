# Teste A/B do custo da árvore em "masked" (a transparência perto do cursor) contra
# "opaco": troca só o modo das cópias dos materiais das árvores (MI_*_WF / _Snow).
# BLEND=opaque|masked; uso: UnrealEditor-Cmd ... -run=pythonscript -script=set_tree_blend.py
import os
import unreal

eal = unreal.EditorAssetLibrary
blend = unreal.BlendMode.BLEND_OPAQUE if os.environ.get('BLEND') == 'opaque' else unreal.BlendMode.BLEND_MASKED
for path in eal.list_assets('/Game/Slice/Trees', recursive=False):
    if not path.split('.')[0].endswith(('_WF', '_Snow')):
        continue
    mi = unreal.load_asset(path)
    if not isinstance(mi, unreal.MaterialInstanceConstant):
        continue
    o = mi.get_editor_property('base_property_overrides')
    o.set_editor_property('override_blend_mode', True)
    o.set_editor_property('blend_mode', blend)
    mi.set_editor_property('base_property_overrides', o)
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    unreal.log('FORGE blend %s -> %s' % (mi.get_name(), blend))
