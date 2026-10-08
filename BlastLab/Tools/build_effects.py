"""Materiais das camadas em volta das explosões (ABlastEffects, docs/EXPLOSOES.md fase 1).

Rodar: powershell -File BlastLab/Tools/blast.ps1 build-explosion (roda este
depois dos materiais do avião/míssil) ou blast.ps1 py Tools\\build_effects.py.

- M_GroundRing: o anel de poeira deitado no chão que vai até o raio do dano
  (todo RTS tem um: é o que comunica a área do dano). Translúcido e iluminado
  (de dia e de noite), a frente rasgada por um ruído angular.
- M_Scorch: a marca queimada (decal): o miolo da cratera e riscos radiais até
  onde a bola de fogo e os jatos chegaram (as artes conceituais e as fotos).

O clarão usa o M_Glow do traçante, as faíscas o M_Emissive e os pedaços o
M_Base do cenário.
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = lambda *a: unreal.log('EFFECTS ' + ' '.join(str(x) for x in a))

RING = '/Game/Materials/M_GroundRing'
SCORCH = '/Game/Materials/M_Scorch'
STAR = '/Game/Materials/M_FlashStar'
GLARE = '/Game/Materials/M_FireGlare'

# O ofuscamento da bola branca (0.1–0.5 s; avaliação da rodada 1 de 07/10: no
# vídeo ela ofusca e borra o entorno, a nossa tinha a borda nítida de algodão):
# halo aditivo suave, sem borda, com o brilho pela curva da emissão (C++).
GLARE_HLSL = r'''
float2 p = (UV - 0.5) * 2.0;
float r2 = dot(p, p);
float g = (exp(-r2 * 5.0) * 0.6 + exp(-r2 * 18.0) * 0.4) * saturate(1.0 - r2);
return float4(g.xxx, 0.0);
'''

# ruído angular sem emenda em ±π: hash de N setores (mod N) suavizado
ANGULAR_NOISE = r'''
struct FAngular
{
    float Hash(float i, float seed) { return frac(sin(i * 12.9898 + seed * 78.233) * 43758.5453); }
    float Noise(float th, float n, float seed)
    {
        float a = (th / 6.2831853 + 0.5) * n;
        float i0 = floor(a);
        float f = a - i0;
        float h0 = Hash(fmod(i0, n), seed);
        float h1 = Hash(fmod(i0 + 1.0, n), seed);
        return lerp(h0, h1, f * f * (3.0 - 2.0 * f));
    }
};
FAngular Ang;
'''

RING_HLSL = ANGULAR_NOISE + r'''
// Col: cor da poeira do chão (rgb) e opacidade; P.x: a frente (fração do meio
// plano), P.y: semente
float2 p = (UV - 0.5) * 2.0;
float r = length(p);
float th = atan2(p.y, p.x);
float n = 0.65 * Ang.Noise(th, 40.0, P.y * 31.0) + 0.35 * Ang.Noise(th, 9.0, P.y * 17.0);
float front = clamp(P.x, 0.001, 0.86) * (0.9 + 0.2 * n);
float inside = 1.0 - smoothstep(front, front + 0.03, r);
float w = 0.2 * front + 0.01;
// a poeira mais densa logo atrás da frente; dentro, um véu ralo
float band = smoothstep(front - w, front - 0.2 * w, r) * (1.0 - smoothstep(front, front + 0.02, r));
float veil = 0.35 * (1.0 - smoothstep(0.3 * front, front, r));
return float4(Col.rgb, saturate((band * (0.7 + 0.3 * n) + veil) * Col.a) * inside);
'''

SCORCH_HLSL = ANGULAR_NOISE + r'''
// D (cor do decal): r = semente, g = opacidade (o fim da vida), b = crescimento,
// a = luminância da fuligem do chão
float2 p = (UV - 0.5) * 2.0;
float r = length(p);
float th = atan2(p.y, p.x);
float g = D.b;
float seed = D.r * 97.0;
// até onde cada setor chega (largo) e os riscos (finos e mais escuros). O raio
// do decal é o do dano: o miolo queimado vai a ~40% dele e os riscos até a borda
// (o primeiro corte, miolo de 10–36% e véu de 35%, mal se via de cima)
float reach = g * (0.55 + 0.45 * Ang.Noise(th, 9.0, seed));
float fine = Ang.Noise(th, 72.0, seed + 3.1);
float rays = pow(fine, 1.4) * (1.0 - smoothstep(0.35 * reach, reach, r));
float core = 1.0 - smoothstep(0.2 * g, 0.45 * g, r);
float haze = 0.75 * (1.0 - smoothstep(0.15 * reach, 0.9 * reach, r));
float soot = saturate(max(core, 0.95 * rays) + haze);
return float4(D.a * float3(1.0, 0.92, 0.85), soot * D.g);
'''


# A estrela do clarão (docs/CONCEITO_EXPLOSOES.md §4d A4: "estrela de raios
# visível 0–0.13 s"; o TankStruckLenzflare do Zero Hour; o vídeo do Veo):
# raios finos de comprimentos sorteados em três famílias (longos e poucos,
# médios, curtos e muitos) + o miolo estourado. Sprite aditivo virado pra
# câmera (UMaterialBillboardComponent no AExplosionVolume); o brilho e o
# sorteio vêm por parâmetro da instância.
STAR_HLSL = ANGULAR_NOISE + r'''
float2 p = (UV - 0.5) * 2.0;
float r = length(p);
float th = atan2(p.y, p.x);
float s = Seed * 53.0;
float rays = 0.0;
// família: número de raios, alcance máximo, finura
float3 fam[3] = { float3(14.0, 1.0, 40.0), float3(36.0, 0.7, 60.0), float3(90.0, 0.4, 90.0) };
[unroll] for (int k = 0; k < 3; k++) {
    float n = fam[k].x;
    float a = (th / 6.2831853 + 0.5) * n;
    float i = floor(a + 0.5);
    float d = abs(a - i);                                   // distância angular ao centro do raio, em raios
    float h = frac(sin(fmod(i, n) * 12.9898 + s + k * 7.1) * 43758.5453);
    float len = fam[k].y * (0.35 + 0.65 * h);
    float core = exp(-d * d * fam[k].z * (1.0 + 6.0 * r));   // o raio afina pra ponta
    rays += core * saturate(1.0 - r / len) * (0.5 + 0.5 * h);
}
float glow = exp(-r * r * 60.0) * 3.0 + exp(-r * r * 9.0) * 0.4;
return float4((rays * 0.6 + glow).xxx, 0.0);
'''


def fresh(path, name):
    # reaproveitado e limpo (apagar falha com o simulador aberto)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return asset_tools.create_asset(name, '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())


def custom(m, code, inputs, x, y):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    c.set_editor_property('code', code)
    return c


def mask(m, x, y, rgb):
    k = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    for c, v in zip('rgba', (rgb, rgb, rgb, not rgb)):
        k.set_editor_property(c, v)
    return k


def ring_material():
    m = fresh(RING, 'M_GroundRing')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # iluminado por pixel como o chão (sol com sombra, céu): com a iluminação
    # volumétrica dos translúcidos o plano saía escuro (06/10)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 120)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('use_custom_primitive_data', True)
    col.set_editor_property('primitive_data_index', 0)        # 0–2 cor, 3 opacidade
    par = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 260)
    par.set_editor_property('parameter_name', 'Ring')
    par.set_editor_property('use_custom_primitive_data', True)
    par.set_editor_property('primitive_data_index', 4)        # 4 frente, 5 semente
    c = custom(m, RING_HLSL, ('UV', 'Col', 'P'), -550, 100)
    rgb, a = mask(m, -250, 60, True), mask(m, -250, 180, False)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(col, 'RGBA', c, 'Col'),
          mel.connect_material_expressions(par, 'RGBA', c, 'P'),
          mel.connect_material_expressions(c, '', rgb, ''),
          mel.connect_material_expressions(c, '', a, ''),
          mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(a, '', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('ring material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('ring material ok')


def scorch_material():
    m = fresh(SCORCH, 'M_Scorch')
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    dc = mel.create_material_expression(m, unreal.MaterialExpressionDecalColor, -900, 120)
    c = custom(m, SCORCH_HLSL, ('UV', 'D'), -550, 60)
    rgb, a = mask(m, -250, 20, True), mask(m, -250, 140, False)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 260)
    rough.set_editor_property('r', 0.95)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(dc, 'RGBA', c, 'D'),       # o pino padrão é RGB
          mel.connect_material_expressions(c, '', rgb, ''),
          mel.connect_material_expressions(c, '', a, ''),
          mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(a, '', unreal.MaterialProperty.MP_OPACITY),
          mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)]
    if not all(ok):
        raise RuntimeError('scorch material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('scorch material ok')


def star_material(path=STAR, name='M_FlashStar', hlsl=STAR_HLSL, color=(1.0, 0.86, 0.64)):
    m = fresh(path, name)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    seed = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 120)
    seed.set_editor_property('parameter_name', 'Seed')
    lum = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 220)
    lum.set_editor_property('parameter_name', 'Intensity')            # cd/m² no miolo (o C++ anima)
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 320)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('default_value', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    c = custom(m, hlsl, ('UV', 'Seed'), -550, 60)
    rgb = mask(m, -350, 60, True)
    tint = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 60)
    out = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -80, 120)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(seed, '', c, 'Seed'),
          mel.connect_material_expressions(c, '', rgb, ''),
          mel.connect_material_expressions(rgb, '', tint, 'A'),
          mel.connect_material_expressions(col, '', tint, 'B'),
          mel.connect_material_expressions(tint, '', out, 'A'),
          mel.connect_material_expressions(lum, '', out, 'B'),
          mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('star material: failed links %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log(name, 'ok')


if __name__ == '__main__':
    ring_material()
    scorch_material()
    star_material()
    # o halo da bola: branco-amarelado quente (o miolo estourado do vídeo)
    star_material(GLARE, 'M_FireGlare', GLARE_HLSL, (1.0, 0.8, 0.55))
    log('done')
