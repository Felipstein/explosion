"""As nuvens do RTS (14/10, docs/CEU_E_NUVENS.md §7.2–7.3): o material da camada VolumetricCloud, em dois níveis.

Rodar na Unreal do jogo, sem janela (o plugin RTSLook ligado):
  UnrealEditor-Cmd.exe Game/RTSGame.uproject -run=pythonscript -script=Game/Tools/look_clouds.py -nullrhi
Grava /RTSLook/Clouds/M_RTSCloud_Hi (alta e ultra: 2 oitavas de dispersão, sombra marchada, a luz da areia na base) e
M_RTSCloud_Med (média: 1 oitava, a sombra pelo mapa de sombra da nuvem). A receita escolhe e ajusta pelos parâmetros.

O modelo (Schneider, "The Real-time Volumetric Cloudscapes of Horizon Zero Dawn", SIGGRAPH 2015):
- a cobertura no mapa, em km: ruído de valor em 2 escalas, vezes a máscara radial (as torres só a partir de InnerKm do
  centro do mapa: nada em cima da batalha, a regra do Horizon "o horizonte sempre épico");
- o perfil pela altura na camada: a laje (o altocúmulo) ou a torre (o cúmulo, base chata e topo redondo, com bigorna);
- a forma no Perlin-Worley da Unreal (T_Volume_PerlinWorley_Balanced) e a erosão de alta frequência só na casca;
- a nuvem fica parada no lugar (a luz do céu é capturada uma vez); o vento só anda o detalhe.
A extinção sai em 1/m (VolumetricCloud.usf: dtMeters), a altura na camada em cm.
"""
import unreal

FOLDER = '/RTSLook/Clouds'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# a densidade da nuvem: forma, perfil, erosão; devolve a extinção (1/m) e a densidade grossa (pro pulo do vazio)
CODE = r"""
float h = saturate(NormH);                           // o nó Cloud Sample Attributes (0 fora da nuvem)
float2 pkm = WP.xy * 1e-5 - CenterKm.xy;             // cm -> km, a partir do centro do mapa
float r = length(pkm);
// a cobertura: ruído de valor em 2 escalas (células de CellKm e CellKm/2,7), sem textura
float2 q1 = pkm / max(CellKm, 0.01), q2 = pkm / max(CellKm / 2.7, 0.01) + 31.7;
float2 i1 = floor(q1), f1 = frac(q1); f1 = f1 * f1 * (3.0 - 2.0 * f1);
float2 i2 = floor(q2), f2 = frac(q2); f2 = f2 * f2 * (3.0 - 2.0 * f2);
float n1 = lerp(lerp(frac(sin(dot(i1, float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i1 + float2(1, 0), float2(127.1, 311.7))) * 43758.5453), f1.x),
                lerp(frac(sin(dot(i1 + float2(0, 1), float2(127.1, 311.7))) * 43758.5453), frac(sin(dot(i1 + float2(1, 1), float2(127.1, 311.7))) * 43758.5453), f1.x), f1.y);
float n2 = lerp(lerp(frac(sin(dot(i2, float2(269.5, 183.3))) * 43758.5453), frac(sin(dot(i2 + float2(1, 0), float2(269.5, 183.3))) * 43758.5453), f2.x),
                lerp(frac(sin(dot(i2 + float2(0, 1), float2(269.5, 183.3))) * 43758.5453), frac(sin(dot(i2 + float2(1, 1), float2(269.5, 183.3))) * 43758.5453), f2.x), f2.y);
float radial = lerp(1.0, smoothstep(InnerKm, RampKm, r), Radial);   // Radial 0 = o céu todo (altocúmulo), 1 = só no horizonte
// o corredor sem torre na direção do sol (±CorridorDeg, borda de 8°): a 2ª opinião de 16/10
float cosA = dot(pkm / max(r, 1e-3), normalize(SunDir.xy + 1e-5));
float corridor = smoothstep(cos(radians(CorridorDeg + 8.0)), cos(radians(CorridorDeg)), cosA) * step(0.01, length(SunDir.xy));
float cov = saturate(Coverage * radial * (1.0 - corridor)) * saturate(0.35 + 0.65 * n1 + 0.3 * (n2 - 0.5));
// o perfil pela altura: Tower 0 = laje (sobe e desce rápido), 1 = torre (base chata, afina e arredonda pro topo, bigorna)
float slab = smoothstep(0.0, 0.15, h) * (1.0 - smoothstep(0.55, 1.0, h));
float tower = smoothstep(0.0, 0.05, h) * (1.0 - smoothstep(0.8, 1.0, h));
float prof = lerp(slab, tower, Tower);
float width = lerp(1.0, lerp(1.0, 0.55, h) + Anvil * smoothstep(0.72, 0.85, h) * (1.0 - smoothstep(0.92, 1.0, h)) * 0.9, Tower);
Cons = float4(cov * prof, 0, 0, 0);
float ext = 0.0;
if (cov * prof > 0.001)
{
    // a forma: Perlin-Worley com as oitavas de Worley (r = PW, gba = Worley), a base que some onde a cobertura é pouca
    float3 uvw = float3(pkm / max(ShapeKm, 0.01), h * HeightKm / max(ShapeKm, 0.01)) + float3(0.13, 0.71, 0.37);
    float4 s = Shape.SampleLevel(ShapeSampler, uvw, 0);
    float fbm = s.g * 0.625 + s.b * 0.25 + s.a * 0.125;
    float base = saturate((s.r - (fbm - 1.0)) / max(1.0 - (fbm - 1.0), 1e-3));        // remap(r, fbm-1, 1, 0, 1)
    float c = saturate((base - (1.0 - cov * width)) / max(cov * width, 1e-3)) * prof;
    if (c > 0.0 && ShadowDist <= 0.0)                 // a erosão não entra nas amostras de sombra (custo, a 2ª opinião)
    {
        // a erosão só na casca: alta frequência andando com o vento, mais fiapo na base e mais bolha no topo
        float3 uvd = float3(pkm / max(DetailKm, 0.01), h * HeightKm / max(DetailKm, 0.01)) + float3(Wind.xy * Time, 0.0) / max(DetailKm, 0.01);
        float3 d = Erosion.SampleLevel(ErosionSampler, uvd, 0).rgb;
        float dfbm = d.r * 0.625 + d.g * 0.25 + d.b * 0.125;
        float dmod = lerp(dfbm, 1.0 - dfbm, saturate(h * 4.0));
        c = saturate((c - dmod * Erode * (1.0 - c)) / max(1.0 - dmod * Erode * (1.0 - c), 1e-3));
    }
    ext = c * Density;
}
return float3(ext, ext, ext);
"""

# (16/10, a 2ª opinião) a extinção ≤ ~0,02/m: com passos de ~500 m, 0,05/m dá espessura ótica 26 num passo só e a nuvem
# vira binária (borda em escada, o lado da sombra preto); o detalhe ≥ 300 m; sem bigorna (lê como o cogumelo da nuke)
SCALARS = (
    ('Coverage', 0.45), ('CellKm', 9.0), ('Radial', 1.0), ('InnerKm', 15.0), ('RampKm', 40.0),
    ('Tower', 1.0), ('Anvil', 0.0), ('ShapeKm', 7.0), ('DetailKm', 0.4), ('HeightKm', 8.5),
    ('Erode', 0.45), ('Density', 0.015), ('CorridorDeg', 18.0),
    ('PhaseG', 0.8), ('PhaseG2', -0.5), ('PhaseBlend', 0.5),
    ('MSContribution', 0.5), ('MSOcclusion', 0.5), ('MSEccentricity', 0.5),
)


def build(name, octaves, raymarch_shadow, ground):
    path = f'{FOLDER}/{name}'
    m = unreal.load_asset(path)
    if m is not None:
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_VOLUME)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)       # o editor recusa outro no domínio Volume
    m.set_editor_property('used_with_volumetric_cloud', True)                  # sem a flag a nuvem não desenha (a 2ª opinião)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    c = E(unreal.MaterialExpressionCustom, -500, 0)
    c.set_editor_property('code', CODE)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('description', 'RTS cloud density')
    pins = ['WP', 'Time', 'NormH', 'ShadowDist', 'CenterKm', 'Wind', 'SunDir', 'Shape', 'Erosion'] + [n for n, _ in SCALARS[:13]]
    ins = []
    for n in pins:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Cons')
    co.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    c.set_editor_property('additional_outputs', [co])
    mel.connect_material_expressions(E(unreal.MaterialExpressionWorldPosition, -900, -300), '', c, 'WP')
    mel.connect_material_expressions(E(unreal.MaterialExpressionTime, -900, -200), '', c, 'Time')
    ca = E(unreal.MaterialExpressionCloudSampleAttribute, -900, -420)
    for pin_out, pin_in in (('NormAltitudeInLayer', 'NormH'), ('ShadowSampleDistance', 'ShadowDist')):
        ok = mel.connect_material_expressions(ca, pin_out, c, pin_in)
        unreal.log(f'LOOKCLOUDS {pin_out} -> {pin_in}: {ok}')
    y = -100
    for pname, dv in (('CenterKm', (0.0, 0.0, 0.0)), ('Wind', (0.004, 0.0015, 0.0)), ('SunDir', (0.0, 0.0, 0.0))):   # o vento em km/s (só o detalhe); o sol vem do LookDirector
        v = E(unreal.MaterialExpressionVectorParameter, -900, y)
        v.set_editor_property('parameter_name', pname)
        v.set_editor_property('default_value', unreal.LinearColor(dv[0], dv[1], dv[2], 0.0))
        mel.connect_material_expressions(v, '', c, pname)
        y += 100
    for tname, src in (('Shape', '/Engine/EngineSky/VolumetricClouds/T_Volume_PerlinWorley_Balanced'),
                       ('Erosion', '/Engine/EngineSky/VolumetricClouds/T_VolumeNoiseErosion32')):
        tex = unreal.load_asset(src)
        t = E(unreal.MaterialExpressionTextureObjectParameter, -900, y)
        t.set_editor_property('parameter_name', tname)
        if tex:
            t.set_editor_property('texture', tex)
        else:
            unreal.log_warning(f'LOOKCLOUDS {src} não carregou')
        mel.connect_material_expressions(t, '', c, tname)
        y += 120
    sp = {}
    for pname, dv in SCALARS:
        s = E(unreal.MaterialExpressionScalarParameter, -900, y)
        s.set_editor_property('parameter_name', pname)
        s.set_editor_property('default_value', dv)
        sp[pname] = s
        if pname in pins:
            mel.connect_material_expressions(s, '', c, pname)
        y += 80
    alb = E(unreal.MaterialExpressionVectorParameter, -300, -250)
    alb.set_editor_property('parameter_name', 'Albedo')
    alb.set_editor_property('default_value', unreal.LinearColor(0.97, 0.97, 0.97, 1.0))     # a nuvem quase não absorve (Hillaire 2020)
    mel.connect_material_property(alb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(c, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)       # no domínio Volume, o pino é a extinção
    o = E(unreal.MaterialExpressionVolumetricAdvancedMaterialOutput, -100, 200)
    o.set_editor_property('multi_scattering_approximation_octave_count', octaves)
    o.set_editor_property('ray_march_volume_shadow', raymarch_shadow)
    o.set_editor_property('ground_contribution', ground)
    o.set_editor_property('gray_scale_material', True)
    for pname, pin in (('PhaseG', 'PhaseG'), ('PhaseG2', 'PhaseG2'), ('PhaseBlend', 'PhaseBlend'),
                       ('MSContribution', 'MultiScatteringContribution'), ('MSOcclusion', 'MultiScatteringOcclusion'),
                       ('MSEccentricity', 'MultiScatteringEccentricity')):
        mel.connect_material_expressions(sp[pname], '', o, pin)
    mel.connect_material_expressions(c, 'Cons', o, 'ConservativeDensity')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    unreal.log(f'LOOKCLOUDS {path} pronta ({octaves} oitavas, sombra {"marchada" if raymarch_shadow else "pelo mapa"}, chão {ground})')


build('M_RTSCloud_Hi', 2, True, True)
build('M_RTSCloud_Med', 1, False, False)
