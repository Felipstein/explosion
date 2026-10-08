"""O flipbook com luz de 6 direções (07/10, noite): o método dos jogos pra fogo e fumaça.

Rodar: powershell -File BlastLab/Tools/blast.ps1 py Tools\\build_sixway.py

Por que (docs/STATUS.md, 07/10): a fumaça em volume de verdade (Heterogeneous
Volumes) só fica bonita a ~400 ms por quadro na 1660; pra caber, saía uma mancha
borrada e quadriculada. Os jogos fazem o detalhe offline (a simulação renderizada
em alta resolução) e tocam o resultado em cartões. Pra não virar adesivo, a luz
não vem cozida: a fumaça é renderizada sob uma luz branca vinda de cada um dos 6
eixos da câmera de bake, e o material reacende com a luz da cena (Unity
SixWayLighting.hlsl; Skull & Bones, Mederic 2018; O3DE).

Os dados (captures/concept/41_sixway/<conjunto>/, assados no Blender pela sessão
de arte: bake_sixway_g1b.py + pack_recon_g1b.py, simulação Mantaflow g1b):
- atlasA = (direita, cima, trás, alfa), atlasB = (esquerda, baixo, frente, fogo),
  8×8 quadros a 24 qps; a luz em gama 1/2.2 normalizada por Lmax, pré-multiplicada
  pelo alfa; o fogo em log2 (E0, Emax), a cor dele numa rampa de 64 tons
  (luminância 1, atlas_meta.json emissive_lut);
- atlasMV = vetores de movimento em UV da célula por quadro: 0.5 + mv·MV_SCALE;
- os eixos são os da câmera de bake (40° de inclinação, como a de RTS), o centro
  de cada célula fica 10 m acima do pé, 28 m de lado (sixway_meta.json).
Cada luz de bake tinha irradiância π: o mapa é a resposta a uma luz unitária, e a
luz do sol (lux) entra como E/π, já em cd/m² como o resto da cena.
"""
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_effects import custom, mel  # noqa: E402
from ue_util import PROJECT  # noqa: E402

log = lambda *a: unreal.log('SIXWAY ' + ' '.join(str(x) for x in a))

FOLDER = '/Game/Explosions/SixWay'
MATERIAL = FOLDER + '/M_SixWay'
SOURCE = os.path.normpath(os.path.join(PROJECT, '..', 'captures', 'concept', '41_sixway'))
SETS = {'p1': 'SixWayP1'}

# O fogo: o bake o pôs na escala de uma cena de hora dourada com sol de 4 W/m²
# (sol a 11°). Nas nossas unidades o sol a 11° dá ~4e4 lux (o de 128 000 lux no
# zênite atravessando ~5 massas de ar): a radiância do bake vira cd/m² × 1e4.
# A/B pelo parâmetro EmissionGain.
EMISSION_GAIN = 1.0e4
# O céu: cada luz de bake tinha irradiância π vinda de um eixo; um céu uniforme
# de radiância L, dividido entre os 6 eixos, dá (4π/6)·L por eixo: a resposta é
# Y·(4/6)·L (o GI do Unity, (4π/6)·Σ, com o π da luz de bake)
SKY_GAIN = 4.0 / 6.0
# no chão e em quem a fumaça encosta, a borda some em vez de cortar reto
DEPTH_FADE_CM = 150.0
# o albedo da areia que rebate o sol pra barriga da fumaça (o deserto do campo:
# areia clara ~0.35–0.45, mais vermelha que azul)
GROUND = (0.42, 0.33, 0.24)


def sixway_hlsl(meta):
    lut = ',\n'.join('float3(%.5f, %.5f, %.5f)' % tuple(c) for c in meta['emissive_lut'])
    return r'''
static const float3 LUT[64] = {
%s
};
float2 cell = 1.0 / float2(Cols, Rows);
float2 c0 = float2(fmod(Frame0, Cols), floor(Frame0 / Cols));
float2 c1 = float2(fmod(Frame1, Cols), floor(Frame1 / Cols));
float2 uv = clamp(UV, 0.002, 0.998);
// Vetores de movimento (Lozar 2015): o quadro de baixo anda pra frente até o
// instante, o de cima volta; sem eles, 24 qps esticados piscam
float2 m0 = (Texture2DSampleLevel(MV, MVSampler, (c0 + uv) * cell, 0).rg - 0.5) / MVScale;
float2 m1 = (Texture2DSampleLevel(MV, MVSampler, (c1 + uv) * cell, 0).rg - 0.5) / MVScale;
float2 u0 = clamp(uv - m0 * FrameAlpha * MVAmount, 0.002, 0.998);
float2 u1 = clamp(uv + m1 * (1.0 - FrameAlpha) * MVAmount, 0.002, 0.998);
float4 a = lerp(Texture2DSample(A, ASampler, (c0 + u0) * cell), Texture2DSample(A, ASampler, (c1 + u1) * cell), FrameAlpha);
float4 b = lerp(Texture2DSample(B, BSampler, (c0 + u0) * cell), Texture2DSample(B, BSampler, (c1 + u1) * cell), FrameAlpha);
// a resposta de cada eixo a uma luz unitária (já vezes o alfa)
float3 Yp = pow(saturate(a.rgb), 2.2) * Lmax;     // direita, cima, trás
float3 Yn = pow(saturate(b.rgb), 2.2) * Lmax;     // esquerda, baixo, frente
// o sol no espaço do cartão: x direita, y cima, z rumo à câmera (a vista da
// Unreal tem z pra dentro da tela)
float3 d = normalize(float3(SunView.x, SunView.y, -SunView.z));
float3 w = d * d;
float T = w.x * (d.x > 0.0 ? Yp.x : Yn.x) + w.y * (d.y > 0.0 ? Yp.y : Yn.y) + w.z * (d.z > 0.0 ? Yn.z : Yp.z);
float3 lit = T * Tint * SunIllum * (DirectGain / 3.14159265);
// o chão aceso pelo sol devolve luz pra barriga e pros lados da fumaça (a
// captura do céu da Unreal não tem o chão: sem isto o lado de baixo saía preto
// no deserto). Radiância da areia: albedo · E · sen(elevação) / π
float3 ground = GroundColor * SunIllum * saturate(SunWorldZ) / 3.14159265;
// o céu nos 6 eixos (radiância da luz do céu), cada um no mapa daquele lado; os
// laterais veem meio chão
lit += (Yp.x * (SkyR + 0.5 * ground) + Yn.x * (SkyL + 0.5 * ground) + Yp.y * SkyU + Yn.y * (SkyD + ground)
      + Yn.z * (SkyF + 0.5 * ground) + Yp.z * (SkyB + 0.5 * ground)) * Tint * SkyGain;
// o fogo: log2 de volta à radiância, a cor pela rampa (luminância 1)
float ey = E0 * (exp2(b.a * log2(1.0 + Emax / E0)) - 1.0);
float3 fire = ey * LUT[min(int(b.a * 255.0 / 4.0), 63)] * EmissionGain;
// depuração (-SixWayDebug=n): 1 o alfa, 2 a soma dos 6 mapas, 3 o peso do sol (opacos, em cinza)
if (DebugView > 0.5)
{
    float v = DebugView < 1.5 ? a.a : DebugView < 2.5 ? dot(Yp + Yn, 1.0) / (6.0 * Lmax) : T / Lmax;
    return float4(v.xxx * 3000.0, 1.0);
}
return float4((lit + fire) * Fade, a.a * Fade);
''' % lut


def import_png(path, asset, vectors=False):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', FOLDER)
    task.set_editor_property('destination_name', asset)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    t = unreal.load_asset('%s/%s' % (FOLDER, asset))
    if not t:
        raise RuntimeError('import failed: ' + path)
    # canais empacotados e independentes: BC7 (o BC1/BC3 põe as cores numa reta);
    # os vetores sem compressão. Nada de sRGB: a gama do bake é decodificada no shader
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP if vectors
                          else unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property('srgb', False)
    t.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    log(asset, t.blueprint_get_size_x(), 'x', t.blueprint_get_size_y())
    return t


def build_material(tex, meta):
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(MATERIAL):
        m = unreal.load_asset(MATERIAL)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SixWay', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)    # alfa pré-multiplicado
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)    # a luz é a do 6-way
    m.set_editor_property('two_sided', True)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    failed = []

    def link(a, sa, b, eb):
        if not mel.connect_material_expressions(a, sa, b, eb):
            failed.append('%s.%s -> %s.%s' % (a.get_name(), sa, b.get_name(), eb))

    tint = [sum(meta['tint'][k][i] for k in meta['tint']) / len(meta['tint']) for i in range(3)]
    inputs = ('UV', 'A', 'B', 'MV', 'Cols', 'Rows', 'Frame0', 'Frame1', 'FrameAlpha', 'Fade', 'Lmax', 'Emax', 'E0', 'MVScale', 'MVAmount',
              'EmissionGain', 'SkyGain', 'DirectGain', 'DebugView', 'Tint', 'GroundColor', 'SunView', 'SunWorldZ', 'SunIllum',
              'SkyR', 'SkyL', 'SkyU', 'SkyD', 'SkyF', 'SkyB')
    c = custom(m, sixway_hlsl(meta), inputs, -400, 0)
    link(E(unreal.MaterialExpressionTextureCoordinate, -1200, -600), '', c, 'UV')
    for i, (name, t) in enumerate((('A', tex['A']), ('B', tex['B']), ('MV', tex['MV']))):
        p = E(unreal.MaterialExpressionTextureObjectParameter, -1200, -480 + 120 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        p.set_editor_property('texture', t)
        link(p, '', c, name)
    scalars = (('Cols', 8.0), ('Rows', 8.0), ('Frame0', 0.0), ('Frame1', 0.0), ('FrameAlpha', 0.0), ('Fade', 1.0),
               ('Lmax', meta['Lmax']), ('Emax', meta['Emax']), ('E0', meta['E0']), ('MVScale', meta['MV_SCALE']), ('MVAmount', 1.0),
               ('EmissionGain', EMISSION_GAIN), ('SkyGain', SKY_GAIN), ('DirectGain', 1.0), ('DebugView', 0.0))
    for i, (name, value) in enumerate(scalars):
        p = E(unreal.MaterialExpressionScalarParameter, -1200, -100 + 70 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        link(p, '', c, name)
    tp = E(unreal.MaterialExpressionVectorParameter, -1200, 900)
    tp.set_editor_property('parameter_name', 'Tint')
    tp.set_editor_property('default_value', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    link(tp, '', c, 'Tint')
    # a areia do campo (o chão que rebate o sol): albedo médio do deserto
    gp = E(unreal.MaterialExpressionVectorParameter, -1200, 980)
    gp.set_editor_property('parameter_name', 'GroundColor')
    gp.set_editor_property('default_value', unreal.LinearColor(GROUND[0], GROUND[1], GROUND[2], 1.0))
    link(gp, '', c, 'GroundColor')
    # o sol: direção (mundo → vista) e iluminância depois da atmosfera
    sun_dir = E(unreal.MaterialExpressionSkyAtmosphereLightDirection, -1500, 1000)
    sun_dir.set_editor_property('light_index', 0)
    sun_z = E(unreal.MaterialExpressionComponentMask, -1300, 940)
    sun_z.set_editor_property('b', True)
    link(sun_dir, '', sun_z, '')
    link(sun_z, '', c, 'SunWorldZ')
    to_view = E(unreal.MaterialExpressionTransform, -1300, 1000)
    to_view.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    to_view.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_VIEW)
    link(sun_dir, '', to_view, '')
    link(to_view, '', c, 'SunView')
    sun_e = E(unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1300, 1100)
    sun_e.set_editor_property('light_index', 0)
    link(sun_e, '', c, 'SunIllum')
    # o céu nos 6 eixos do cartão (a vista: x direita, y cima, −z rumo à câmera)
    for i, (name, axis) in enumerate((('SkyR', (1, 0, 0)), ('SkyL', (-1, 0, 0)), ('SkyU', (0, 1, 0)),
                                      ('SkyD', (0, -1, 0)), ('SkyF', (0, 0, -1)), ('SkyB', (0, 0, 1)))):
        k = E(unreal.MaterialExpressionConstant3Vector, -1700, 1250 + 110 * i)
        k.set_editor_property('constant', unreal.LinearColor(axis[0], axis[1], axis[2], 0.0))
        tw = E(unreal.MaterialExpressionTransform, -1500, 1250 + 110 * i)
        tw.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW)
        tw.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        link(k, '', tw, '')
        s = E(unreal.MaterialExpressionSkyLightEnvMapSample, -1300, 1250 + 110 * i)
        link(tw, '', s, 'Direction')
        link(s, '', c, name)
    rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -60)
    for ch in 'rgb':
        rgb.set_editor_property(ch, True)
    alpha = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, 80)
    alpha.set_editor_property('a', True)
    link(c, '', rgb, '')
    link(c, '', alpha, '')
    fade = E(unreal.MaterialExpressionDepthFade, 50, 80)
    fade.set_editor_property('fade_distance_default', DEPTH_FADE_CM)
    link(alpha, '', fade, 'Opacity')
    ok = mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR) and \
        mel.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
    if failed or not ok:
        raise RuntimeError('M_SixWay: failed links %s' % failed)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    log('M_SixWay ok')


if __name__ == '__main__':
    meta = None
    textures = None
    for name, tag in SETS.items():
        d = os.path.join(SOURCE, name)
        meta = json.load(open(os.path.join(d, 'atlas_meta.json'), encoding='utf-8'))
        textures = {k: import_png(os.path.join(d, 'atlas%s_512.png' % k), 'T_%s_%s' % (tag, k), vectors=(k == 'MV'))
                    for k in ('A', 'B', 'MV')}
    build_material(textures, meta)
    log('done')
