"""O teste do flipbook (07/10): a bola de fogo pré-renderizada no Cycles tocando num cartão.

Rodar: powershell -File BlastLab/Tools/blast.ps1 py Tools\\build_flipbook.py

A sessão de arte renderiza a simulação do EmberGen com a luz da cena
(Tools/blender_flipbook.py) em captures/concept/38_flipbook/<nome>/atlas.exr:
quadros em grade, RGBA em float com alfa PRÉ-MULTIPLICADO. Aqui:
- cada atlas vira T_<Nome>_Atlas (HDR float16, sem mip: os quadros são vizinhos
  na grade e o mip misturaria um com o outro na borda);
- M_FlipbookCard: translúcido AlphaComposite (o modo do alfa pré-multiplicado),
  sem luz (a luz já vem cozida), emissivo = rgb × Gain e opacidade = a,
  misturando os dois quadros em volta do instante (o C++ põe Frame0, Frame1,
  FrameAlpha e Fade; ABlastFlipbook).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_effects import custom, mel  # noqa: E402
from ue_util import PROJECT  # noqa: E402

log = lambda *a: unreal.log('FLIPBOOK ' + ' '.join(str(x) for x in a))

FOLDER = '/Game/Explosions/Flipbook'
CARD = FOLDER + '/M_FlipbookCard'
SOURCE = os.path.normpath(os.path.join(PROJECT, '..', 'captures', 'concept', '38_flipbook'))
ATLASES = {'gas_hero': 'GasHero', 'gas_rts': 'GasRts',
           # a luz da prévia (sol a 15°, 35° pro lado da câmera), pro teste justo contra ela
           'gas_hero_side': 'GasHeroSide'}

# Ganho: a radiância do Cycles → cd/m² da nossa cena. O sol do render da
# sessão de arte tem energia 5 (W/m², perpendicular) e o nosso 128 000 lux
# (js/celestial.js SUN_LUX): 128 000 / 5. Medido no atlas gas_hero: a bola aos
# 0.3–0.7 s tem mediana 0.2–0.8 → 5–20 mil cd/m² (estourada contra o cinza
# médio de ~630 da cena, como no vídeo); a casca aos 1.3–1.5 s 0.01–0.02 → abaixo
# dele, com brasas a 4–12 mil. -FlipbookGain=x no simulador pra A/B.
#
# Com os atlas separados (fumaça iluminada + fogo): a fumaça no ganho que casa
# a casca aos 1.5–2 s com a fumaça volumétrica (6 400: L 75/68 contra 77/67;
# captures/ue/_ab_flipbook_gain.jpg); o físico (25 600) lavava a casca de
# cinza-claro. O fogo com o próprio ganho (EmissionGain), calibrado em A/B.
GAIN = 6400.0
EMISSION_GAIN = 6400.0

CARD_HLSL = r'''
float2 cell = float2(1.0 / Cols, 1.0 / Rows);
float2 c0 = float2(fmod(Frame0, Cols), floor(Frame0 / Cols));
float2 c1 = float2(fmod(Frame1, Cols), floor(Frame1 / Cols));
// meio texel pra dentro: a borda do quadro não lê o vizinho
float2 uv = clamp(UV, 0.002, 0.998);
float4 a = Texture2DSampleLevel(Atlas, AtlasSampler, (c0 + uv) * cell, 0);
float4 b = Texture2DSampleLevel(Atlas, AtlasSampler, (c1 + uv) * cell, 0);
// a emissão em atlas próprio (o fogo, sem alfa: soma luz), com o próprio ganho:
// um ganho só não servia à fumaça e ao fogo (A/B de 07/10)
float3 ea = Texture2DSampleLevel(Emission, EmissionSampler, (c0 + uv) * cell, 0).rgb;
float3 eb = Texture2DSampleLevel(Emission, EmissionSampler, (c1 + uv) * cell, 0).rgb;
float4 s = lerp(a, b, FrameAlpha);
float3 e = lerp(ea, eb, FrameAlpha);
// a borda da fumaça densa sai quase binária no alfa: a potência > 1 deixa a
// franja rala mais transparente (a cor pré-multiplicada vai na mesma proporção)
float k = s.a > 1e-4 ? pow(s.a, AlphaPow) / s.a : 0.0;
return float4((s.rgb * k * Gain + e * EmissionGain) * Fade, s.a * k * Fade);
'''


def import_exr(path, asset):
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
        raise RuntimeError('atlas import failed: ' + path)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
    t.set_editor_property('srgb', False)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property('never_stream', True)
    t.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    log(asset, t.blueprint_get_size_x(), 'x', t.blueprint_get_size_y())
    return t


DISPLAY_CARD = FOLDER + '/M_FlipbookCardDisplay'
DISPLAY_SETS = {'gas_hero_agx': 'GasHeroAgx'}

# O cartão de "cor de tela" (teste de 07/10): a sessão de arte renderiza o
# quadro já com o AgX do Blender (a prévia que o Felipe gostou), em sRGB de 8
# bits e alfa reto. O nosso tonemapper (M_TonemapWebGL, build_scene.py: ACES do
# Hill + 45% de preservação de matiz + contraste 1.06 + saturação 1.16)
# desatura os claros pro bege mais cedo que o AgX; aqui o material acha, pelo
# inverso fechado, a radiância que ELE transforma de volta na cor da prévia
# (ida e volta em Python: erro de poucos % no laranja, levemente mais vermelho). Sem a
# vinheta e o bloom (pequenos no meio da tela). A emissão sai na escala da cena
# pelo inverso da exposição (EyeAdaptationInverse). EM ABERTO (07/10): na cena
# o cartão sai ~3 EV mais claro que o atlas (rodada agx2); multiplicar por
# OneOverPreExposure piorou muito (agx3, tela branca). Medir a exposição do
# tonemapper (TONEMAP_DEBUG em build_scene.py) antes de mexer.
DISPLAY_HLSL = r'''
// Inverso fechado do tonemapper (a 1ª versão iterava e divergia nos laranjas
// saturados: pontos verdes e roxos na bola). Desfaz, na ordem inversa: a
// saturação 1.16, o contraste 1.06, a matriz de saída do ACES do Hill, a
// racional (equação do 2º grau por canal) e a matriz de entrada. A mistura de
// 45% de preservação de matiz fica de fora (o laranja sai um pouco mais
// saturado que no AgX).
const float3x3 RRT_INV = float3x3(1.764741, -0.675778, -0.088963, -0.147028, 1.160252, -0.013224, -0.036337, -0.162436, 1.198773);
const float3x3 ODT_INV = float3x3(0.643038, 0.311187, 0.045775, 0.059269, 0.931436, 0.009295, 0.005962, 0.063929, 0.930118);
const float3 L709 = float3(0.2126, 0.7152, 0.0722);
float2 cell = float2(1.0 / Cols, 1.0 / Rows);
float2 c0 = float2(fmod(Frame0, Cols), floor(Frame0 / Cols));
float2 c1 = float2(fmod(Frame1, Cols), floor(Frame1 / Cols));
float2 uv = clamp(UV, 0.002, 0.998);
float4 s = lerp(Texture2DSampleLevel(Atlas, AtlasSampler, (c0 + uv) * cell, 0),
                Texture2DSampleLevel(Atlas, AtlasSampler, (c1 + uv) * cell, 0), FrameAlpha);
// a cor alvo na tela, linear (o amostrador já tira o sRGB); o branco puro o ACES
// só alcança no infinito: teto 0.97
float3 d = clamp(s.rgb, 0.0, 0.97);
float l = dot(d, L709);
float3 c = max(l + (d - l) / 1.16, 0.0);
c = pow(c, 1.0 / 1.06);
float3 v = clamp(mul(ODT_INV, c), 0.0, 0.98);
// v = (x(x + A) − B) / (x(C x + D) + E)  →  (vC − 1)x² + (vD − A)x + (vE + B) = 0
const float A = 0.0245786, B = 0.000090537, C = 0.983729, D = 0.4329510, E = 0.238081;
float3 qa = v * C - 1.0, qb = v * D - A, qc = v * E + B;
float3 x = (-qb - sqrt(max(qb * qb - 4.0 * qa * qc, 0.0))) / (2.0 * qa);
x = max(mul(RRT_INV, max(x, 0.0)), 0.0);
return float4(x, s.a * Fade);
'''


def import_display(name, tag):
    path = os.path.join(SOURCE, name, 'atlas.png')
    if not os.path.exists(path):
        log('sem atlas de tela ainda:', name)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', FOLDER)
    task.set_editor_property('destination_name', 'T_%s_Atlas' % tag)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    t = unreal.load_asset('%s/T_%s_Atlas' % (FOLDER, tag))
    if not t:
        raise RuntimeError('display atlas import failed: ' + path)
    # cor de tela em sRGB, alfa reto; BC7 segura o degradê do alfa
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property('srgb', True)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property('never_stream', True)
    t.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    log('T_%s_Atlas' % tag, t.blueprint_get_size_x(), 'x', t.blueprint_get_size_y())
    return t


def display_material(default_atlas):
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(DISPLAY_CARD):
        m = unreal.load_asset(DISPLAY_CARD)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_FlipbookCardDisplay', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)       # alfa reto
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    try:
        m.set_editor_property('use_translucency_vertex_fog', False)
    except Exception:
        pass
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    uv = E(unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    tex = E(unreal.MaterialExpressionTextureObjectParameter, -1000, 120)
    tex.set_editor_property('parameter_name', 'Atlas')
    tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tex.set_editor_property('texture', default_atlas or unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    params = {}
    for i, (name, value) in enumerate((('Cols', 8.0), ('Rows', 6.0), ('Frame0', 0.0), ('Frame1', 0.0), ('FrameAlpha', 0.0), ('Fade', 1.0))):
        p = E(unreal.MaterialExpressionScalarParameter, -1000, 260 + 90 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        params[name] = p
    c = custom(m, DISPLAY_HLSL, ('UV', 'Atlas', 'Cols', 'Rows', 'Frame0', 'Frame1', 'FrameAlpha', 'Fade'), -600, 100)
    rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -350, 60)
    for k in 'rgb':
        rgb.set_editor_property(k, True)
    alpha = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -350, 200)
    alpha.set_editor_property('a', True)
    inv = E(unreal.MaterialExpressionEyeAdaptationInverse, -180, 60)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'), mel.connect_material_expressions(tex, '', c, 'Atlas')]
    for name, p in params.items():
        ok.append(mel.connect_material_expressions(p, '', c, name))
    # o nome do pino de entrada varia com a versão
    pin = next((n for n in ('LightValue', 'Light Value', 'LightValueInput', '') if mel.connect_material_expressions(rgb, '', inv, n)), None)
    ok.append(pin is not None)
    ok += [mel.connect_material_expressions(c, '', rgb, ''),
           mel.connect_material_expressions(c, '', alpha, ''),
           mel.connect_material_property(inv, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
           mel.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('display card material: failed links %s' % ok)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    log('M_FlipbookCardDisplay ok')


def import_atlas(name, tag):
    """A fumaça (com alfa) em T_<Tag>_Atlas e, se vier separada, a emissão em
    T_<Tag>_Emission. Sem os separados, o atlas único (fumaça e fogo juntos)."""
    # os separados vêm em pastas irmãs: <nome>_lit (só a fumaça iluminada, com o
    # alfa) e <nome>_emit (só o fogo, atenuado pela fumaça da frente)
    lit = os.path.join(SOURCE, name + '_lit', 'atlas.exr')
    emit = os.path.join(SOURCE, name + '_emit', 'atlas.exr')
    if os.path.exists(lit) and os.path.exists(emit):
        t = import_exr(lit, 'T_%s_Atlas' % tag)
        import_exr(emit, 'T_%s_Emission' % tag)
        return t
    single = os.path.join(SOURCE, name, 'atlas.exr')
    if not os.path.exists(single):
        log('sem atlas ainda:', name)
        return None
    return import_exr(single, 'T_%s_Atlas' % tag)


def card_material(default_atlas):
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(CARD):
        m = unreal.load_asset(CARD)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_FlipbookCard', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    # A névoa no cartão: o render do Cycles já tem o ar entre a câmera e a bola
    # (céu quente); a névoa da Unreal por cima lavava o cartão (o halo pálido da
    # 1ª rodada). -- só se a propriedade existir na versão
    for prop in ('apply_fogging', 'use_translucency_vertex_fog'):
        try:
            m.set_editor_property(prop, False)
            log('sem névoa no cartão:', prop)
        except Exception:
            pass
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    uv = E(unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    texs = {}
    for i, (name, default) in enumerate((('Atlas', default_atlas or unreal.load_asset('/Engine/EngineResources/DefaultTexture')),
                                         ('Emission', unreal.load_asset('/Engine/EngineResources/Black')))):
        t = E(unreal.MaterialExpressionTextureObjectParameter, -1000, 120 + 120 * i)
        t.set_editor_property('parameter_name', name)
        t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        t.set_editor_property('texture', default)
        texs[name] = t
    params = {}
    for i, (name, value) in enumerate((('Cols', 8.0), ('Rows', 6.0), ('Frame0', 0.0), ('Frame1', 0.0), ('FrameAlpha', 0.0),
                                        ('Fade', 1.0), ('Gain', GAIN), ('EmissionGain', EMISSION_GAIN), ('AlphaPow', 1.0))):
        p = E(unreal.MaterialExpressionScalarParameter, -1000, 400 + 90 * i)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        params[name] = p
    inputs = ('UV', 'Atlas', 'Emission', 'Cols', 'Rows', 'Frame0', 'Frame1', 'FrameAlpha', 'Fade', 'Gain', 'EmissionGain', 'AlphaPow')
    c = custom(m, CARD_HLSL, inputs, -600, 100)
    rgb = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -350, 60)
    rgb.set_editor_property('r', True)
    rgb.set_editor_property('g', True)
    rgb.set_editor_property('b', True)
    alpha = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -350, 200)
    alpha.set_editor_property('a', True)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV')]
    for name, t in texs.items():
        ok.append(mel.connect_material_expressions(t, '', c, name))
    for name, p in params.items():
        ok.append(mel.connect_material_expressions(p, '', c, name))
    ok += [mel.connect_material_expressions(c, '', rgb, ''),
           mel.connect_material_expressions(c, '', alpha, ''),
           mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
           mel.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('flipbook card material: failed links %s' % ok)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    log('M_FlipbookCard ok')

if __name__ == '__main__':
    atlases = [import_atlas(n, t) for n, t in ATLASES.items()]
    card_material(next((a for a in atlases if a), None))
    shown = [import_display(n, t) for n, t in DISPLAY_SETS.items()]
    display_material(next((a for a in shown if a), None))
    log('done')
