"""M_FXSixWayDust (13/10, otimização A4): o M_FXSixWay sem a folha da emissão (o fogo). Na poeira a emissão é sempre
zero, mas o material do fogo lê a T_Emit nos dois quadros misturados em todo pixel: 2 das 6 leituras de folha. Esta
cópia tem o MESMO cálculo com E = 0 (o resultado é idêntico na poeira); o M_FXSixWay do fogo e do tiro não muda.
Uso: UnrealEditor-Cmd FXLab.uproject -run=pythonscript -script=FXLab/Tools/make_dust_material.py -nullrhi -unattended"""
import builtins
import os
import runpy

import unreal

builtins.FXLAB_NO_BUILD = True
ns = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'setup_fxlab.py'))
mel, eal, log = ns['mel'], ns['eal'], ns['log']

src = ns['SIXWAY_HLSL']
old = 'float3 E = lerp(Texture2DSample(EmitTex, EmitTexSampler, a).rgb, Texture2DSample(EmitTex, EmitTexSampler, b).rgb, t);'
assert old in src, 'o HLSL do M_FXSixWay mudou: conferir a linha da emissão'
HLSL = src.replace(old, 'float3 E = 0;                                  // a poeira não tem fogo: a folha da emissão não é lida')
# UV2.x (o fogo, sem uso na poeira): a sombra do cenário (casa, árvore, pedra) no plugin do jogo. Corta só o sol: o céu
# continua chegando (a pedra longe tapa o sol, não o céu inteiro; a sombra do próprio tanque, que tapa os dois, segue no UV3.y)
old_col = 'float3 col = (SunC * sunR * T + SkyC'
assert old_col in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a linha da cor'
HLSL = HLSL.replace(old_col, 'float3 col = (SunC * sunR * T * (1.0 - UV2.x) + SkyC')
# LightScale: as folhas normalizadas pra 8 bits (bake_norm8.py) guardam a luz × ganho; aqui volta (as HDR: 1, 1)
old_nn = 'float4 Nn = lerp(Texture2DSample(SixNeg, SixNegSampler, a), Texture2DSample(SixNeg, SixNegSampler, b), t);'
assert old_nn in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a leitura da SixNeg'
HLSL = HLSL.replace(old_nn, old_nn + '\nPp.rgb *= LightScale.x; Nn.rgb *= LightScale.y;')

# o plugin da poeira do jogo (pack_tread_dust.py) monta a mesma cópia em /RTSTreadDust/M_TreadDust
DEST = globals().get('DUST_DEST', '/Game/FX')
NAME = globals().get('DUST_NAME', 'M_FXSixWayDust')
tiny = unreal.load_asset(f'{DEST}/T_Tiny')
m = ns['new_material'](f'{DEST}/{NAME}')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
m.set_editor_property('enable_responsive_aa', True)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
failed = []
link = ns['linker'](m, failed)
names = ['UV0', 'UV1', 'UV2', 'UV3', 'VC', 'SixPos', 'SixNeg', 'Grid', 'SunCol', 'SkyCol', 'EmitGain', 'Density', 'EmberCut',
         'Albedo', 'Tint', 'Fade', 'SceneSun', 'SceneSky', 'SceneLight', 'EmitScale', 'ClipAlpha', 'LightScale']
c = ns['custom'](m, -300, 0, 'SixWayDust', HLSL, names, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
ns['scene_light_inputs'](m, c, link, -900, 1000)
for i in range(4):
    tc = E(unreal.MaterialExpressionTextureCoordinate, -900, -400 + i * 60)
    tc.set_editor_property('coordinate_index', i)
    link(tc, '', c, f'UV{i}')
vc = E(unreal.MaterialExpressionVertexColor, -900, -120)
link(vc, '', c, 'VC')
for i, pname in enumerate(('SixPos', 'SixNeg')):
    to = E(unreal.MaterialExpressionTextureObjectParameter, -900, -40 + i * 120)
    to.set_editor_property('parameter_name', pname)
    to.set_editor_property('texture', tiny)
    to.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(to, '', c, pname)
for i, (pname, val) in enumerate((('Grid', unreal.LinearColor(8, 6, 48, 0)), ('SunCol', unreal.LinearColor(1, 1, 1, 0)),
                                   ('SkyCol', unreal.LinearColor(0.3, 0.4, 0.6, 0)), ('Tint', unreal.LinearColor(1, 1, 1, 1)),
                                   ('LightScale', unreal.LinearColor(1, 1, 0, 0)))):
    v = E(unreal.MaterialExpressionVectorParameter, -900, 340 + i * 100)
    v.set_editor_property('parameter_name', pname)
    v.set_editor_property('default_value', val)
    link(v, '', c, pname)
for i, (pname, val) in enumerate((('EmitGain', 0.0), ('Density', 2.6), ('EmberCut', 0.9), ('Albedo', 1.11), ('ClipAlpha', 1.0 / 1024.0))):
    s = E(unreal.MaterialExpressionScalarParameter, -900, 640 + i * 60)
    s.set_editor_property('parameter_name', pname)
    s.set_editor_property('default_value', val)
    link(s, '', c, pname)
soft = E(unreal.MaterialExpressionScalarParameter, -900, 960)
soft.set_editor_property('parameter_name', 'SoftDist')
soft.set_editor_property('default_value', 35.0)
mx = E(unreal.MaterialExpressionMax, -700, 960)
mx.set_editor_property('const_b', 0.1)
link(soft, '', mx, 'A')
df = E(unreal.MaterialExpressionDepthFade, -500, 960)
link(mx, '', df, 'FadeDistance')
link(df, '', c, 'Fade')
rgb = E(unreal.MaterialExpressionComponentMask, -100, -40)
for ch, on in (('r', True), ('g', True), ('b', True), ('a', False)):
    rgb.set_editor_property(ch, on)
link(c, '', rgb, '')
al = E(unreal.MaterialExpressionComponentMask, -100, 60)
for ch, on in (('r', False), ('g', False), ('b', False), ('a', True)):
    al.set_editor_property(ch, on)
link(c, '', al, '')
ok = [mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
      mel.connect_material_property(al, '', unreal.MaterialProperty.MP_OPACITY)]
mel.recompile_material(m)
eal.save_loaded_asset(m)
log(f'{DEST}/{NAME}', ok, 'falhas:', failed)
