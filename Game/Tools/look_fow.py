"""A névoa de guerra no visual do Look (17/10; docs/CEU_E_NUVENS.md §12): os dois testes que o Felipe pediu pra câmera deitada.

Rodar na Unreal do jogo (sem janela):  game.ps1 py -Target Tools/look_fow.py

- rts.FogStyle 1, "a sombra de nuvem" (o Gemini C): NÃO é daqui. O sol já tem uma light function (a M_SmokeShadowLF do
  BlastFx, com a sombra da fumaça e a das nuvens) e só aceita uma: o termo fow mora nela (BlastLab/Tools/build_scene.py,
  SMOKE_SHADOW_HLSL; o Fluxo topou em 17/10) e o LookDirector põe os parâmetros no MID do sol. Fora da visão o sol some e fica
  a luz do céu; a poeira, o céu e as nuvens continuam por cima (a ordem do Beyond All Reason).
- M_FowBlur (pós, rts.FogStyle 2, "o desfoque"): o que está fora da visão sai desfocado, um pouco dessaturado e escurecido,
  e o escurecimento some com a névoa da distância (C' = k·C + (1−k)·névoa, a ideia da pesquisa: o horizonte fica com a poeira).

Os dois leem a textura de visão do jogo (RTS_FogVision, sRGB: o hardware devolve o nível linear da tela, 1 visto, 0,212 o
explorado, 0 o nunca visto) pelo retângulo do mapa, com a B-spline cúbica do FogOfWar.usf (a borda sem losango). Fora do mapa
vale o nível do explorado. O LookDirector cria a instância e passa os parâmetros (ALookDirector::SetFogOfWar).
"""
import unreal

FOLDER = '/RTSLook/Fow'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# a leitura da visão: a B-spline cúbica com 4 leituras bilineares (Sigg & Hadwiger, GPU Gems 2, cap. 20), como no FogOfWar.usf
VISION = r"""
float2 uvM = (WP.xy - MM.xy) * MM.zw;
float2 inside = saturate(uvM);
float2 ts = 1.0 / max(TX.xy, 1e-6);
float2 P = inside * ts - 0.5;
float2 I = floor(P); float2 F = P - I; float2 F2 = F * F; float2 F3 = F2 * F;
float2 W0 = (1.0 / 6.0) * (-F3 + 3.0 * F2 - 3.0 * F + 1.0);
float2 W1 = (1.0 / 6.0) * (3.0 * F3 - 6.0 * F2 + 4.0);
float2 W2 = (1.0 / 6.0) * (-3.0 * F3 + 3.0 * F2 + 3.0 * F + 1.0);
float2 W3 = (1.0 / 6.0) * F3;
float2 G0 = W0 + W1; float2 G1 = W2 + W3;
float2 H0 = I - 1.0 + W1 / G0; float2 H1 = I + 1.0 + W3 / G1;
float A = Vision.SampleLevel(VisionSampler, (float2(H0.x, H0.y) + 0.5) * TX.xy, 0).r;
float B = Vision.SampleLevel(VisionSampler, (float2(H1.x, H0.y) + 0.5) * TX.xy, 0).r;
float C = Vision.SampleLevel(VisionSampler, (float2(H0.x, H1.y) + 0.5) * TX.xy, 0).r;
float D = Vision.SampleLevel(VisionSampler, (float2(H1.x, H1.y) + 0.5) * TX.xy, 0).r;
float v = saturate(G0.y * (G0.x * A + G1.x * B) + G1.y * (G0.x * C + G1.x * D));
// fora do mapa: o nível do explorado, chegando nele da borda em 75 m (como no FogOfWar.usf)
if (any(uvM != inside))
{
    float2 edge = MM.xy + inside / max(MM.zw, 1e-9);
    v = lerp(v, 0.212, saturate(length(WP.xy - edge) / 7500.0));
}
// 0 = explorado (ou pior), 1 = visto; e entre o nunca visto e o explorado
float seen = saturate((v - 0.212) / (1.0 - 0.212));
float known = saturate(v / 0.212);
"""

# o nó da visão: o raio do desfoque (em UV da tela, redondo), o quanto não é visto e o quanto já foi visto
VIS_CODE = VISION + r"""
if (Dep > 5.0e6) return float4(0.0, 0.0, 0.0, 1.0);    // o céu (além de 50 km): nunca leva a névoa de guerra
float unseen = 1.0 - seen;
// (17/10, o Felipe: "muito muito desfocado, e o desfoque muito perto") o desfoque é só do longe: nítido até BlurNearCm da
// câmera, inteiro a partir de BlurFarCm (o chão e as unidades perto ficam nítidos mesmo fora da visão), e mais fraco
float r = BlurFrac * BlurScale * unseen * smoothstep(BlurNearCm, BlurFarCm, Dep);
return float4(r * View.ViewSizeAndInvSize.y * View.ViewSizeAndInvSize.z, r, unseen, known);
"""

# a mistura: as 12 amostras vêm de nós da cena no grafo (o SceneTextureLookup dentro do código lia zero na 5.8; o
# diagnóstico de 17/10 com raio 0 deu o mesmo escuro); a gradação do fora da visão (a do AoE IV: um tom frio, menos
# saturação e mais escuro) some com a névoa da distância, então o horizonte fica com a poeira dourada
MIX_CODE = r"""
float3 c = C0.rgb;
float unseen = VIS.z;
float known = VIS.w;
if (Dep > 5.0e6 || unseen < 0.002) return c;
float3 b = (c + T0.rgb + T1.rgb + T2.rgb + T3.rgb + T4.rgb + T5.rgb + T6.rgb + T7.rgb + T8.rgb + T9.rgb + T10.rgb + T11.rgb) / 13.0;
float haze = 1.0 - exp(-Dep / max(HazeCm, 1.0));
// (17/10, o passeio no modo preto) o nunca visto esconde o mapa (o preto do ZH, o azul-marinho opaco do AoE IV): a névoa
// da distância só o clareia bem longe (o horizonte da câmera deitada), não no chão perto
float hz = lerp(haze * haze * haze, haze, known);
float dark = lerp(lerp(NeverDark, UnseenDark, known), 1.0, hz);
float des = UnseenDesat * (1.0 - hz);
float lum = dot(b, float3(0.2126, 0.7152, 0.0722));
b = lerp(b, lum.xxx, des * unseen);
float3 tint = lerp(float3(1.0, 1.0, 1.0), Tint.rgb, 1.0 - hz);
b *= lerp(float3(1.0, 1.0, 1.0), dark * tint, unseen);
return b;
"""

POISSON = [(-0.326, -0.406), (-0.840, -0.074), (-0.696, 0.457), (-0.203, 0.621), (0.962, -0.195), (0.473, -0.480),
           (0.519, 0.767), (0.185, -0.893), (0.507, 0.064), (0.896, 0.412), (-0.322, -0.933), (-0.792, -0.598)]


def material(name, domain):
    path = f'{FOLDER}/{name}'
    # (reaproveita o asset e limpa os nós: apagar e recriar no mesmo commandlet devolve vazio)
    m = unreal.load_asset(path)       # (o registro do commandlet nem sempre conhece o conteúdo do plugin)
    if m is not None:
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', domain)
    return m


def custom(m, code, x, y, ins, out_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('code', code)
    c.set_editor_property('output_type', out_type)
    lst = []
    for n in ins:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        lst.append(ci)
    c.set_editor_property('inputs', lst)
    return c


def scalar(m, name, dv, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', dv)
    return e


def vector(m, name, dv, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', unreal.LinearColor(*dv))
    return e


def vision_inputs(m, c, y0):
    """WP, Vision, MM (o mínimo do mapa e 1/tamanho), TX (1/largura, 1/altura da textura de visão)."""
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, y0)
    mel.connect_material_expressions(wp, '', c, 'WP')
    t = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, y0 + 120)
    t.set_editor_property('parameter_name', 'FogVision')
    white = unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture')
    if white:
        t.set_editor_property('texture', white)
    t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_expressions(t, '', c, 'Vision')
    for name, pin, dv, yy in (('FowMapMinInvSize', 'MM', (0, 0, 1e-5, 1e-5), y0 + 260), ('FowTexel', 'TX', (1 / 256.0, 1 / 256.0, 0, 0), y0 + 380)):
        v = vector(m, name, dv, -900, yy)
        if not mel.connect_material_expressions(v, 'RGBA', c, pin):
            ap = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -700, yy)
            mel.connect_material_expressions(v, '', ap, 'A')
            mel.connect_material_expressions(v, 'A', ap, 'B')
            mel.connect_material_expressions(ap, '', c, pin)


def build_blur():
    """O pós da névoa de guerra do Look (rts.FogStyle 1 e 2; o LookDirector põe os valores de cada estilo no MID)."""
    m = material('M_FowBlur', unreal.MaterialDomain.MD_POST_PROCESS)
    loc = getattr(unreal.BlendableLocation, 'BL_SCENE_COLOR_BEFORE_BLOOM', None) or getattr(unreal.BlendableLocation, 'BL_BEFORE_BLOOM', None)
    if loc is not None:
        m.set_editor_property('blendable_location', loc)
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
    vis = custom(m, VIS_CODE, -1300, -300, ['WP', 'Vision', 'MM', 'TX', 'Dep', 'BlurFrac', 'BlurScale', 'BlurNearCm', 'BlurFarCm'], unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    vision_inputs(m, vis, -500)
    dep = E(unreal.MaterialExpressionSceneDepth, -1800, 100)
    mel.connect_material_expressions(dep, '', vis, 'Dep')
    bf = scalar(m, 'BlurFrac', 0.006, -1800, 200)
    mel.connect_material_expressions(bf, '', vis, 'BlurFrac')
    for name, dv, yy in (('BlurScale', 0.4, 300), ('BlurNearCm', 8000.0, 400), ('BlurFarCm', 40000.0, 500)):
        mel.connect_material_expressions(scalar(m, name, dv, -1800, yy), '', vis, name)
    rxy = E(unreal.MaterialExpressionComponentMask, -1000, -300)
    for ch, on in (('r', True), ('g', True), ('b', False), ('a', False)):
        rxy.set_editor_property(ch, on)
    mel.connect_material_expressions(vis, '', rxy, '')
    sp = E(unreal.MaterialExpressionScreenPosition, -1000, -150)
    mix = custom(m, MIX_CODE, -200, 0, ['C0', 'VIS', 'Dep'] + ['T%d' % k for k in range(12)] + ['UnseenDark', 'NeverDark', 'UnseenDesat', 'HazeCm', 'Tint'])
    st0 = E(unreal.MaterialExpressionSceneTexture, -500, -400)
    st0.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(st0, 'Color', mix, 'C0')
    mel.connect_material_expressions(vis, '', mix, 'VIS')
    mel.connect_material_expressions(dep, '', mix, 'Dep')
    for k, (ox, oy) in enumerate(POISSON):
        y = -200 + 110 * k
        cst = E(unreal.MaterialExpressionConstant2Vector, -900, y)
        cst.set_editor_property('r', ox)
        cst.set_editor_property('g', oy)
        mul = E(unreal.MaterialExpressionMultiply, -800, y)
        mel.connect_material_expressions(cst, '', mul, 'A')
        mel.connect_material_expressions(rxy, '', mul, 'B')
        add = E(unreal.MaterialExpressionAdd, -700, y)
        mel.connect_material_expressions(sp, 'ViewportUV', add, 'A')
        mel.connect_material_expressions(mul, '', add, 'B')
        st = E(unreal.MaterialExpressionSceneTexture, -600, y)
        st.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
        st.set_editor_property('filtered', True)
        if not mel.connect_material_expressions(add, '', st, 'UVs'):
            unreal.log_warning('LOOKFOW a UV da amostra %d não ligou' % k)
        mel.connect_material_expressions(st, 'Color', mix, 'T%d' % k)
    for name, dv, yy in (('UnseenDark', 0.75, 1200), ('NeverDark', 0.35, 1300), ('UnseenDesat', 0.5, 1400), ('HazeCm', 60000.0, 1500)):
        mel.connect_material_expressions(scalar(m, name, dv, -500, yy), '', mix, name)
    tint = vector(m, 'Tint', (1.0, 1.0, 1.0, 1.0), -500, 1600)
    mel.connect_material_expressions(tint, '', mix, 'Tint')
    mel.connect_material_property(mix, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    unreal.log(f'LOOKFOW M_FowBlur pronta (o lugar do pós: {loc})')


build_blur()
