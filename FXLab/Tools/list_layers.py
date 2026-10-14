import unreal
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/World_TestSalt2')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.LandscapeProxy):
        print('FXLAB_LS', a.get_actor_label(), a.get_editor_property('landscape_material').get_path_name() if a.get_editor_property('landscape_material') else None)
        try:
            for li in a.get_editor_property('editor_layer_settings'):
                print('FXLAB_LAYER', li.get_editor_property('layer_info_obj').get_path_name() if li.get_editor_property('layer_info_obj') else None)
        except Exception as e:
            print('FXLAB_ERR', e)
        try:
            print('FXLAB_TARGETS', [str(x) for x in a.get_editor_property('target_layers').keys()])
        except Exception as e:
            print('FXLAB_ERR2', e)
