"""Materiais do que atravessa o campo (avião, míssil, disparo: ABlastLabMover).

Rodar: powershell -File BlastLab/Tools/blast.ps1 build-explosion (roda este
depois da explosão) ou blast.ps1 py Tools\\build_movers.py.

A carcaça usa o M_Base do cenário (cor/rugosidade/metal por peça). Aqui só o
emissivo: a chama do motor do míssil e o traçante do disparo (scene.js id 5:
a cor da peça É a radiância, sem luz nenhuma).
"""
import unreal

EMISSIVE = '/Game/Materials/M_Emissive'
LUX_PER_UNIT = 128000.0        # radiância do WebGL → cd/m² (js/celestial.js SUN_LUX)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = lambda *a: unreal.log('MOVERS ' + ' '.join(str(x) for x in a))


def emissive_material():
    # reaproveitado e limpo (apagar falha com o simulador aberto)
    if unreal.EditorAssetLibrary.does_asset_exist(EMISSIVE):
        m = unreal.load_asset(EMISSIVE)
        mel.delete_all_material_expressions(m)
    else:
        m = asset_tools.create_asset('M_Emissive', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    # a chama cintila e o traçante pula ~17 m por quadro: sem isso a heurística
    # anti-cintilação do TSR apagava os dois (Material.h bHasPixelAnimation)
    m.set_editor_property('has_pixel_animation', True)
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property('parameter_name', 'Radiance')
    color.set_editor_property('use_custom_primitive_data', True)
    color.set_editor_property('primitive_data_index', 0)       # 0, 1, 2 = radiância (unidade do WebGL)
    color.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -300, 0)
    for c, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        rgb.set_editor_property(c, v)
    scale = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 0)
    scale.set_editor_property('const_b', LUX_PER_UNIT)
    ok = [mel.connect_material_expressions(color, '', rgb, ''),
          mel.connect_material_expressions(rgb, '', scale, 'A'),
          mel.connect_material_property(scale, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('emissive material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('emissive material ok')
    return m


GLOW = '/Game/Materials/M_Glow'


def glow_material():
    """O brilho em volta do traçante: aditivo, macio (mais forte no meio do tubo,
    some na borda: cos² entre a normal e a vista). O bloom da Unreal é feito
    depois do TSR, e o TSR apaga a linha fina e clara (pesa as amostras por
    1/(1 + brilho) contra vagalumes): o halo que o bloom do WebGL dava ao
    traçante não aparecia. Nos jogos o brilho do traçante é parte do efeito."""
    if unreal.EditorAssetLibrary.does_asset_exist(GLOW):
        m = unreal.load_asset(GLOW)
        mel.delete_all_material_expressions(m)
    else:
        m = asset_tools.create_asset('M_Glow', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided', False)
    # O brilho aparece num lugar novo a cada quadro (o projétil anda ~17 m por
    # quadro): o TSR o tratava como cintilação e apagava. "Responsive AA" é o
    # caso dele (Material.h: "partículas pequenas que se movem, como faíscas").
    m.set_editor_property('enable_responsive_aa', True)
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 0)
    color.set_editor_property('parameter_name', 'Radiance')
    color.set_editor_property('use_custom_primitive_data', True)
    color.set_editor_property('primitive_data_index', 0)
    color.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
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
    c.set_editor_property('code', 'float k = saturate(dot(normalize(N), normalize(V)));\n'
                                  'return Radiance.rgb * (k * k) * %r;' % LUX_PER_UNIT)
    ok = [mel.connect_material_expressions(color, '', c, 'Radiance'),
          mel.connect_material_expressions(normal, '', c, 'N'),
          mel.connect_material_expressions(cam, '', c, 'V'),
          mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('glow material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('glow material ok')
    return m


if __name__ == '__main__':
    emissive_material()
    glow_material()
    log('done')
