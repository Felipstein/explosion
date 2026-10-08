"""Materiais dos destroços da morte de tanque (ABlastDebris, BlastDebris.cpp).

Rodar: powershell -File BlastLab/Tools/blast.ps1 py Tools\\build_debris.py

- M_DebrisGlow: a cabeça dos cometas, aditiva e virada pra câmera: miolo
  estourado e um halo curto. A cor do vértice (8 bits) leva a cor e o apagar;
  o brilho absoluto (cd/m²) é o Intensity.
- M_TrailSmoke: a fumaça dos rastros, translúcida e iluminada por pixel (sol
  com sombra e céu, como o chão). A fita tem perfil redondo na largura e a
  borda comida pelas bolhas do Worley (a erosão do Nubis), que andam ao longo
  do rastro em metros. Cor do vértice: r opacidade, g fogo, b albedo, a semente.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_effects import custom, fresh, mask, mel  # noqa: E402

log = lambda *a: unreal.log('DEBRIS ' + ' '.join(str(x) for x in a))

GLOW = '/Game/Materials/M_DebrisGlow'
TRAIL = '/Game/Materials/M_TrailSmoke'
WORLEY = '/Game/Explosions/T_DetailWorley'      # build_explosion.py (make_noise.py worley)

GLOW_HLSL = r'''
float2 p = (UV - 0.5) * 2.0;
float r2 = dot(p, p);
float g = (exp(-r2 * 40.0) + 0.12 * exp(-r2 * 6.0)) * saturate(1.0 - r2);
return float4(VC.rgb * g, 0.0);
'''

TRAIL_HLSL = r'''
float v = UV.y * 2.0 - 1.0;
// perfil redondo na largura (o tubo de fumaça visto de lado)
float prof = exp(-v * v * 2.5) * (1.0 - smoothstep(0.8, 1.0, abs(v)));
// as bolhas do Worley (oitavas 0.625/0.25/0.125, as do Nubis) ao longo do rastro
// em metros e na largura, devagar no tempo; cada rastro na sua semente
float3 q = float3(UV.x * 0.6, v * 0.9, VC.a * 37.0 + Time * 0.15);
float n = dot(Texture3DSampleLevel(Worley, WorleySampler, q, 0).rgb, float3(0.625, 0.25, 0.125));
// a opacidade é a do vértice (≤ ~0.6: fumaça, não tubo) vezes o perfil; o
// Worley come primeiro a borda rala (onde o perfil cai) e deixa fiapos no meio
float a = VC.r * prof * smoothstep(0.25, 0.75, n + 0.35 * prof);
return float4(VC.b, VC.g, a, 0.0);
'''


def glow_material():
    m = fresh(GLOW, 'M_DebrisGlow')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -900, 120)
    lum = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 260)
    lum.set_editor_property('parameter_name', 'Intensity')
    # metal em brasa a ~1900 K: ~2·10⁵ cd/m² (a cor do vértice dá o tom e o apagar)
    lum.set_editor_property('default_value', 2.0e5)
    c = custom(m, GLOW_HLSL, ('UV', 'VC'), -550, 60)
    rgb = mask(m, -350, 60, True)
    out = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 120)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(vc, '', c, 'VC'),
          mel.connect_material_expressions(c, '', rgb, ''),
          mel.connect_material_expressions(rgb, '', out, 'A'),
          mel.connect_material_expressions(lum, '', out, 'B'),
          mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('debris glow material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_DebrisGlow ok')


def trail_material():
    m = fresh(TRAIL, 'M_TrailSmoke')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # iluminada por pixel como o chão (sol com sombra, céu)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('two_sided', True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    vc = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -1100, 120)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1100, 260)
    wor = mel.create_material_expression(m, unreal.MaterialExpressionTextureObject, -1100, 360)
    wor.set_editor_property('texture', unreal.load_asset(WORLEY))
    c = custom(m, TRAIL_HLSL, ('UV', 'VC', 'Time', 'Worley'), -750, 60)
    # a cor do vértice sai como rgb + a separados: junta num float4
    vc4 = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -950, 140)
    mel.connect_material_expressions(vc, '', vc4, 'A')
    mel.connect_material_expressions(vc, 'A', vc4, 'B')
    # saídas: x albedo, y fogo, z opacidade
    def comp(ch, y):
        k = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -500, y)
        for name in 'rgba':
            k.set_editor_property(name, name == ch)
        mel.connect_material_expressions(c, '', k, '')
        return k
    alb, fire, op = comp('r', 0), comp('g', 120), comp('b', 240)
    # fumaça quase neutra, um pouco quente (a fuligem e os óxidos)
    tint = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -500, -140)
    tint.set_editor_property('constant', unreal.LinearColor(0.95, 0.92, 0.88, 1.0))
    base = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, -60)
    # o fogo logo atrás da peça: laranja de ~1300 K, ~3·10⁴ cd/m² no mais quente
    flame = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 380)
    flame.set_editor_property('parameter_name', 'FlameColor')
    flame.set_editor_property('default_value', unreal.LinearColor(1.0, 0.42, 0.1, 1.0))
    glum = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 500)
    glum.set_editor_property('parameter_name', 'FlameLum')
    glum.set_editor_property('default_value', 3.0e4)
    e1 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 380)
    e2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 420)
    zero = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 560)
    one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 640)
    one.set_editor_property('r', 1.0)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(vc4, '', c, 'VC'),
          mel.connect_material_expressions(tm, '', c, 'Time'),
          mel.connect_material_expressions(wor, '', c, 'Worley'),
          mel.connect_material_expressions(alb, '', base, 'A'),
          mel.connect_material_expressions(tint, '', base, 'B'),
          mel.connect_material_expressions(flame, '', e1, 'A'),
          mel.connect_material_expressions(fire, '', e1, 'B'),
          mel.connect_material_expressions(e1, '', e2, 'A'),
          mel.connect_material_expressions(glum, '', e2, 'B'),
          mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY),
          mel.connect_material_property(e2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
          mel.connect_material_property(zero, '', unreal.MaterialProperty.MP_SPECULAR),
          mel.connect_material_property(one, '', unreal.MaterialProperty.MP_ROUGHNESS)]
    if not all(ok):
        raise RuntimeError('trail material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_TrailSmoke ok')


if __name__ == '__main__':
    glow_material()
    trail_material()
    log('done')
