"""Materiais do jogo (rodar: powershell -File Game/Tools/game.ps1 content).

M_Emissive e M_Glow são os do BlastLab (BlastLab/Tools/build_movers.py): a cor
de cada peça É a radiância, por dado de primitiva (índices 0–2), × 128000 cd/m².
Servem pro clarão do tiro, o traçante, o impacto e o clarão da morte.
"""
import unreal

LUX_PER_UNIT = 128000.0

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = lambda *a: unreal.log('GAMECONTENT ' + ' '.join(str(x) for x in a))


def fresh(path, name):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return asset_tools.create_asset(name, '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())


def radiance_param(m, x, y):
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    color.set_editor_property('parameter_name', 'Radiance')
    color.set_editor_property('use_custom_primitive_data', True)
    color.set_editor_property('primitive_data_index', 0)
    color.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    return color


def emissive_material():
    m = fresh('/Game/Materials/M_Emissive', 'M_Emissive')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # o traçante pula metros por quadro: sem isso a anti-cintilação do TSR o apaga
    m.set_editor_property('has_pixel_animation', True)
    color = radiance_param(m, -500, 0)
    rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -300, 0)
    for c, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(c, v)
    scale = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 0)
    scale.set_editor_property('const_b', LUX_PER_UNIT)
    ok = [mel.connect_material_expressions(color, '', rgb, ''),
          mel.connect_material_expressions(rgb, '', scale, 'A'),
          mel.connect_material_property(scale, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('M_Emissive: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_Emissive ok')


def glow_material():
    m = fresh('/Game/Materials/M_Glow', 'M_Glow')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided', False)
    m.set_editor_property('enable_responsive_aa', True)
    color = radiance_param(m, -700, 0)
    normal = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -700, 250)
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -700, 350)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 100)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ('Radiance', 'N', 'V'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    # mais forte no meio, some na borda (cos² entre a normal e a vista)
    c.set_editor_property('code', 'float k = saturate(dot(normalize(N), normalize(V)));\n'
                                  'return Radiance.rgb * (k * k) * %r;' % LUX_PER_UNIT)
    ok = [mel.connect_material_expressions(color, '', c, 'Radiance'),
          mel.connect_material_expressions(normal, '', c, 'N'),
          mel.connect_material_expressions(cam, '', c, 'V'),
          mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('M_Glow: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_Glow ok')


emissive_material()
glow_material()
log('done')
