"""M_FXDustShadow: a sombra da nuvem de poeira no chão (docs/POEIRA_DAS_LAGARTAS.md, B3, camada S).

Um cartão deitado no chão por cartão de nuvem (como a partícula m_isGroundAligned do Zero Hour), esticado pra longe
do sol pela altura da nuvem × cot(elevação do sol). Ele escurece o chão (cor da tela × 1 − k·alfa) pela silhueta do
mesmo quadro do cozimento, borrada (mip +Blur): a sombra de poeira é macia. 13/10: era Modulate, que abre uma passada
de translucidez só pra ele (AfterDOFModulate); agora é alfa pré-multiplicado com cor 0, a mesma conta no passe da poeira.
UV0 = canto do cartão (y pra cima), UV1 = (quadro, força). Os parâmetros SixPos e Grid vêm do cozimento.

Uso (Unreal sem janela):
  UnrealEditor-Cmd.exe FXLab.uproject -run=pythonscript -script=FXLab/Tools/make_dust_shadow.py -nullrhi -unattended
"""
import builtins
import os
import runpy

import unreal

builtins.FXLAB_NO_BUILD = True
ns = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'setup_fxlab.py'))
mel, eal, log = ns['mel'], ns['eal'], ns['log']

HLSL = r'''
float cols = Grid.x, rows = Grid.y, frames = Grid.z;
float fr = UV1.x;
float f0 = floor(fr), f1 = min(f0 + 1.0, frames - 1.0), t = fr - f0;
float2 uv = UV0;
float2 a = float2((fmod(f0, cols) + uv.x) / cols, (floor(f0 / cols) + 1.0 - uv.y) / rows);
float2 b = float2((fmod(f1, cols) + uv.x) / cols, (floor(f1 / cols) + 1.0 - uv.y) / rows);
float al = lerp(Texture2DSampleBias(SixPos, SixPosSampler, a, Blur).a, Texture2DSampleBias(SixPos, SixPosSampler, b, Blur).a, t);
float s = saturate(al * UV1.y);
clip(s - ClipAlpha);
return MODULATE > 0.5 ? 1.0 - s : s;
'''

def build(path, modulate):
    m = ns['new_material'](path)
    m.set_editor_property('blend_mode', (unreal.BlendMode.BLEND_MODULATE if modulate else unreal.BlendMode.BLEND_ALPHA_COMPOSITE))
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []
    link = ns['linker'](m, failed)
    c = ns['custom'](m, -300, 0, 'DustShadow', HLSL.replace('MODULATE > 0.5', 'true' if modulate else 'false'), ['UV0', 'UV1', 'SixPos', 'Grid', 'Blur', 'ClipAlpha'], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    for i in range(2):
        tc = E(unreal.MaterialExpressionTextureCoordinate, -900, -200 + i * 60)
        tc.set_editor_property('coordinate_index', i)
        link(tc, '', c, f'UV{i}')
    to = E(unreal.MaterialExpressionTextureObjectParameter, -900, 0)
    to.set_editor_property('parameter_name', 'SixPos')
    to.set_editor_property('texture', unreal.load_asset(TINY))   # o MID troca: o material não segura folha grande
    to.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(to, '', c, 'SixPos')
    g = E(unreal.MaterialExpressionVectorParameter, -900, 140)
    g.set_editor_property('parameter_name', 'Grid')
    g.set_editor_property('default_value', unreal.LinearColor(8, 6, 48, 0))
    link(g, '', c, 'Grid')
    bl = E(unreal.MaterialExpressionScalarParameter, -900, 260)
    bl.set_editor_property('parameter_name', 'Blur')
    bl.set_editor_property('default_value', 3.0)
    link(bl, '', c, 'Blur')
    ca = E(unreal.MaterialExpressionScalarParameter, -900, 320)
    ca.set_editor_property('parameter_name', 'ClipAlpha')
    ca.set_editor_property('default_value', 1.0 / 1024.0)
    link(ca, '', c, 'ClipAlpha')
    if modulate:
        ok = [mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    else:
        zero = E(unreal.MaterialExpressionConstant, -100, 120)
        zero.set_editor_property('r', 0.0)
        ok = [mel.connect_material_property(zero, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
              mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY)]
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log(path, ok, 'falhas:', failed)

# o plugin da poeira do jogo (pack_tread_dust.py) passa DUST_SHADOWS = [('/RTSTreadDust/M_TreadDustShadow', False)]
TINY = globals().get('DUST_TINY', '/Game/FX/T_Tiny')
for path, modulate in globals().get('DUST_SHADOWS', [('/Game/FX/M_FXDustShadow', False),
                                                     ('/Game/FX/M_FXDustShadowMod', True)]):   # Modulate: o modo antigo, pro A/B de custo
    build(path, modulate)
