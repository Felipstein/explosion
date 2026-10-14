"""Lista os atores de um mapa (classe, rótulo, posição em m, tamanho em m) que batem com um filtro.
Uso: FXLAB_LIST_MAP=/Game/Maps/World_TestSalt2 FXLAB_LIST_FILTER=road,asphalt,street,village UnrealEditor-Cmd ... -run=pythonscript -script=list_map.py"""
import os
import unreal

m = os.environ.get('FXLAB_LIST_MAP', '/Game/Maps/World_TestSalt2')
flt = [x.strip().lower() for x in os.environ.get('FXLAB_LIST_FILTER', '').split(',') if x.strip()]
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(m)
acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
counts = {}
for a in acts:
    cls = a.get_class().get_name()
    counts[cls] = counts.get(cls, 0) + 1
    lab = a.get_actor_label()
    txt = (cls + ' ' + lab).lower()
    if flt and not any(f in txt for f in flt):
        continue
    o, e = a.get_actor_bounds(False)
    print('FXLAB_ACT', cls, '|', lab, '| pos', round(o.x / 100, 1), round(o.y / 100, 1), round(o.z / 100, 1), '| meia', round(e.x / 100, 1), round(e.y / 100, 1), round(e.z / 100, 1), '| tags', [str(t) for t in a.tags])
print('FXLAB_CLASSES', sorted(counts.items(), key=lambda kv: -kv[1])[:40])
