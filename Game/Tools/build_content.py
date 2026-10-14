"""Materiais do jogo (rodar: powershell -File Game/Tools/game.ps1 content).

M_Emissive e M_Glow são os do BlastLab (BlastLab/Tools/build_movers.py): a cor
de cada peça É a radiância, por dado de primitiva (índices 0–2), × 128000 cd/m².
Servem pro clarão do tiro, o traçante, o impacto e o clarão da morte.

M_GroundMemoryDecal é o decal do mapa inteiro que pinta a memória do chão
(RTSGroundMemory.cpp): lê os dois cadernos (R esteira, G queimado, B cratera).
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


# A memória do chão (11/10). O DBuffer da Unreal mistura (não multiplica: o Modulate vira Translucent
# no DBuffer, DecalRenderingCommon.cpp), então a marca é um marrom bem escuro com pouca opacidade:
# escurece qualquer chão (areia, terra, grama) como um multiplicar. A normal é a do próprio chão
# (tirada da posição na tela) inclinada pelo relevo das marcas: o sulco da garra e a cratera pegam o
# sol baixo sem achatar a encosta embaixo. Normal no espaço do mundo (tangent_space_normal falso: o
# decal usa direto, MaterialTemplate.ush).
GROUND_HLSL = r'''
float2 uvC = (WP.xy - CB.xy) / CB.zw;
float2 uvD = (WP.xy - DB.xy) / max(DB.zw, 1.0);
// o detalhe só dentro do caderno de detalhe (some nos últimos 6% da borda) e enquanto um pixel da
// tela não cobre mais que ~2 pixels dele (longe vira o grosso, sem serrilhar as garras)
float2 inD = saturate(min(uvD, 1.0 - uvD) * 16.0);
float fw = max(length(ddx(uvD)), length(ddy(uvD))) / Tx.y;
float wD = (DB.z > 0.0 ? inD.x * inD.y : 0.0) * saturate(2.5 - fw);

float4 c = Texture2DSampleLevel(Coarse, CoarseSampler, uvC, 0);
float4 d = Texture2DSampleLevel(Detail, DetailSampler, uvD, 0);
float4 m = lerp(c, d, wD);
// (13/10) o G e o A em rampas de distância (bake_craters.py --ramps): dentro = 0,5 + 0,5·intensidade
if (MarkRamps > 0.5)
{
    m.g = m.g < 0.5 ? 0.0 : (m.g - 0.5) * 2.0;
    m.a = m.a < 0.5 ? 0.0 : (m.a - 0.5) * 2.0;
}
// a altura das crateras (13/10): R16F em cm, o grosso e o de detalhe (na resolução do relevo, Tx.z)
float hC = lerp(Texture2DSampleLevel(CraterC, CraterCSampler, uvC, 0).r, Texture2DSampleLevel(CraterD, CraterDSampler, uvD, 0).r, wD);

// relevo (cm): a esteira afunda TrackDepth; gradiente por diferenças centrais
float2 eC = float2(Tx.x, 0.0), eD = float2(Tx.y, 0.0), eR = float2(Tx.z, 0.0);
#define HGT(v) (-(v.r * TrackDepth))
float3 cx0 = Texture2DSampleLevel(Coarse, CoarseSampler, uvC - eC.xy, 0).rgb, cx1 = Texture2DSampleLevel(Coarse, CoarseSampler, uvC + eC.xy, 0).rgb;
float3 cy0 = Texture2DSampleLevel(Coarse, CoarseSampler, uvC - eC.yx, 0).rgb, cy1 = Texture2DSampleLevel(Coarse, CoarseSampler, uvC + eC.yx, 0).rgb;
float3 dx0 = Texture2DSampleLevel(Detail, DetailSampler, uvD - eD.xy, 0).rgb, dx1 = Texture2DSampleLevel(Detail, DetailSampler, uvD + eD.xy, 0).rgb;
float3 dy0 = Texture2DSampleLevel(Detail, DetailSampler, uvD - eD.yx, 0).rgb, dy1 = Texture2DSampleLevel(Detail, DetailSampler, uvD + eD.yx, 0).rgb;
float2 gC = float2(HGT(cx1) - HGT(cx0), HGT(cy1) - HGT(cy0)) / (2.0 * CB.z * Tx.x);
float2 gD = float2(HGT(dx1) - HGT(dx0), HGT(dy1) - HGT(dy0)) / (2.0 * max(DB.z, 1.0) * Tx.y);
float2 g = lerp(gC, gD, wD);
#undef HGT
// a cratera: o gradiente da altura própria dela (a bacia, a borda e os torrões)
float2 kC = float2(Texture2DSampleLevel(CraterC, CraterCSampler, uvC + eC.xy, 0).r - Texture2DSampleLevel(CraterC, CraterCSampler, uvC - eC.xy, 0).r,
                   Texture2DSampleLevel(CraterC, CraterCSampler, uvC + eC.yx, 0).r - Texture2DSampleLevel(CraterC, CraterCSampler, uvC - eC.yx, 0).r) / (2.0 * CB.z * Tx.x);
float2 kD = float2(Texture2DSampleLevel(CraterD, CraterDSampler, uvD + eR.xy, 0).r - Texture2DSampleLevel(CraterD, CraterDSampler, uvD - eR.xy, 0).r,
                   Texture2DSampleLevel(CraterD, CraterDSampler, uvD + eR.yx, 0).r - Texture2DSampleLevel(CraterD, CraterDSampler, uvD - eR.yx, 0).r) / (2.0 * max(DB.z, 1.0) * Tx.z);
g += lerp(kC, kD, wD);

// a normal do chão: das derivadas da posição (o decal reconstrói a posição pela profundidade)
float3 Ng = normalize(cross(ddx(WP), ddy(WP)));
Ng = Ng.z < 0.0 ? -Ng : Ng;
WNormal = normalize(Ng + float3(-g * Bump, 0.0));

// o piso até a MF do Look entrar no chão (ela pinta como fica; o decal só mistura uma cor por cima):
// R a esteira, G o escuro (terra revirada, fuligem), A o claro (a areia espirrada e os torrões), o fundo da bacia
float aT = saturate(m.r) * TrackDark;
float aS = saturate(m.g) * ScorchDark;
float aL = saturate(m.a) * 0.45;
float aP = saturate(-hC / 45.0) * 0.6;
Opacity = 1.0 - (1.0 - aT) * (1.0 - aS) * (1.0 - aL) * (1.0 - aP);
float3 colT = float3(0.13, 0.095, 0.06);      // areia revolvida (a de baixo da crosta), mais escura que a de cima
float3 colS = float3(0.035, 0.029, 0.024);   // a terra escura revirada e a fuligem
float3 colL = float3(0.52, 0.40, 0.27);      // a areia clara espirrada
float3 colP = float3(0.070, 0.050, 0.035);   // o fundo da bacia, úmido
float3 Col = (colT * aT + colS * aS + colL * aL + colP * aP) / (aT + aS + aL + aP + 1e-5);
// (13/10) com a MF do Look no chão: ela faz a esteira, o relevo e as marcas de perto, mas o escuro e o claro ela
// só lê do caderno de detalhe enquanto o pixel do chão é menor que ~9 cm (na câmera do RTS ele tem 10–20 cm, e o
// queimado sumia). O decal fica só com eles, no tom dela (o escuro = 0,42 × a areia meio sem cor, a 80%; o claro
// = a areia × 1,18/1,13/1,05, a 75%), e some onde ela já pinta. Quando a MF ler o caderno do meio, o jogo
// desliga o decal (RTSGroundMemory::PushToLandscape).
if (MarksOnly > 0.5)
{
    float fwG = length(fwidth(WP.xy));
    float inLook = DB.z > 0.0 ? step(0.0, uvD.x) * step(uvD.x, 1.0) * step(0.0, uvD.y) * step(uvD.y, 1.0) : 0.0;
    float wLook = (1.0 - smoothstep(6.0, 12.0, fwG)) * inLook;
    aS = saturate(m.g) * 0.8 * (1.0 - wLook);
    aL = saturate(m.a) * 0.75 * (1.0 - wLook);
    Opacity = 1.0 - (1.0 - aS) * (1.0 - aL);
    WNormal = normalize(Ng + float3(-lerp(kC, kD, wD) * Bump, 0.0));
    Col = (float3(0.168, 0.143, 0.120) * aS + float3(0.53, 0.37, 0.23) * aL) / (aS + aL + 1e-5);
}
// depuração (-RTSGroundDebug): 1 = o mapa inteiro vermelho translúcido com o caderno por cima em cor pura;
// 2 = o normal com a normal do chão sem inclinar; 3 = o normal com a opacidade forçada pela marca
if (DebugView > 3.5)
{
	// o relevo (12/10): 4 altura (vermelho sulco, azul bordinha, verde revolvido), 5 idade, 6 sol
	const float4 Rl = Texture2DSampleLevel(Relief, ReliefSampler, uvD, 0);
	const float Hc = (Rl.r - 0.5) * 2.0 * 4.0;
	Opacity = 0.85 * inD.x * inD.y;
	WNormal = Ng;
	if (DebugView < 4.5) { return float3(saturate(-Hc / 2.5), Rl.a * 0.5, saturate(Hc / 2.0)); }
	if (DebugView < 5.5) { return Rl.ggg; }
	return Rl.bbb;
}
if (DebugView > 0.5 && DebugView < 1.5)
{
	Opacity = 0.5;
	WNormal = Ng;
	return float3(0.6, 0.02, 0.02) + m * 2.0;
}
if (DebugView > 1.5 && DebugView < 2.5)
{
	WNormal = Ng;
}
if (DebugView > 2.5)
{
	Opacity = saturate((m.r + m.g + m.b) * 0.9);
}
return Col;
'''


def ground_memory_material():
    m = fresh('/Game/Materials/M_GroundMemoryDecal', 'M_GroundMemoryDecal')
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('tangent_space_normal', False)
    black = unreal.load_asset('/Engine/EngineResources/Black')
    nodes = {}

    def texobj(name, y):
        e = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('texture', black)
        return e

    def vec(name, y, v):
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', unreal.LinearColor(*v))
        return e

    def scalar(name, y, v):
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', v)
        return e

    nodes['WP'] = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, -200)
    nodes['Coarse'] = texobj('GroundCoarse', -100)
    nodes['Detail'] = texobj('GroundDetail', 0)
    nodes['Relief'] = texobj('GroundRelief', 50)
    nodes['CraterC'] = texobj('GroundCraterCoarse', 60)
    nodes['CraterD'] = texobj('GroundCraterDetail', 70)
    nodes['CB'] = vec('CoarseBounds', 100, (-51200.0, -51200.0, 102400.0, 102400.0))
    nodes['DB'] = vec('DetailBounds', 200, (0.0, 0.0, -1.0, -1.0))
    nodes['Tx'] = vec('Texels', 300, (1.0 / 2048.0, 1.0 / 4096.0, 1.0 / 2048.0, 0.0))
    nodes['TrackDepth'] = scalar('TrackDepthCm', 400, 4.0)
    nodes['Bump'] = scalar('Bump', 600, 6.0)
    nodes['TrackDark'] = scalar('TrackDark', 700, 0.75)
    nodes['ScorchDark'] = scalar('ScorchDark', 800, 0.85)
    nodes['DebugView'] = scalar('DebugView', 900, 0.0)
    nodes['MarksOnly'] = scalar('MarksOnly', 1000, 0.0)
    nodes['MarkRamps'] = scalar('MarkRamps', 1100, 0.0)

    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in nodes:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('WNormal', unreal.CustomMaterialOutputType.CMOT_FLOAT3), ('Opacity', unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    c.set_editor_property('additional_outputs', outs)
    c.set_editor_property('code', GROUND_HLSL)
    # os vetores entram inteiros (a saída '' do VectorParameter é só o RGB: o .zw falhava no SM6)
    vec4 = {'CB', 'DB', 'Tx'}
    ok = [mel.connect_material_expressions(e, 'RGBA' if n in vec4 else '', c, n) for n, e in nodes.items()]
    ok += [mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR),
           mel.connect_material_property(c, 'WNormal', unreal.MaterialProperty.MP_NORMAL),
           mel.connect_material_property(c, 'Opacity', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('M_GroundMemoryDecal: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_GroundMemoryDecal ok')


# O contato com o chão (12/10, BASE_JOGAVEL: "a fresta escura embaixo do casco é o que diz pesado"): a
# mancha escura macia sob cada veículo, o recurso clássico do RTS (Warcraft III, StarCraft II), num decal
# preso ao casco. Só a cor (a normal da areia fica): um retângulo de cantos redondos, escuro no meio e macio
# na borda, um pouco maior que a planta do casco.
CONTACT_HLSL = r'''
float2 p = UV * 2.0 - 1.0;
float2 q = abs(p) - (1.0 - Round);
float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - Round;
Opacity = (1.0 - smoothstep(-0.38, 0.0, d)) * Strength;   // cheio até perto da borda do casco, o degradê só pra fora
return float3(0.014, 0.012, 0.010);
'''


def unit_contact_material():
    m = fresh('/Game/Materials/M_UnitContact', 'M_UnitContact')
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -700, 0)
    rnd = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 100)
    rnd.set_editor_property('parameter_name', 'Round')
    rnd.set_editor_property('default_value', 0.45)
    st = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 200)
    st.set_editor_property('parameter_name', 'Strength')
    st.set_editor_property('default_value', 0.7)
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ('UV', 'Round', 'Strength'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Opacity')
    co.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    c.set_editor_property('additional_outputs', [co])
    c.set_editor_property('code', CONTACT_HLSL)
    ok = [mel.connect_material_expressions(uv, '', c, 'UV'),
          mel.connect_material_expressions(rnd, '', c, 'Round'),
          mel.connect_material_expressions(st, '', c, 'Strength'),
          mel.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR),
          mel.connect_material_property(c, 'Opacity', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('M_UnitContact: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_UnitContact ok')


# A silhueta da unidade escondida (13/10, BASE_JOGAVEL: "silhueta atrás da fábrica", como no StarCraft II e no
# CoH, que mostram a unidade atrás de prédio na cor do time): as peças da unidade gravam a profundidade num
# buffer próprio (custom depth) com o jogador no stencil (1 + dono); onde essa profundidade fica na frente da
# cena (a unidade está atrás de algo opaco), a cor do time por cima, meio transparente, com a borda mais forte.
# Depois da curva de tom (as cores são as da tela, as mesmas do HUD). A fumaça não grava profundidade: dentro
# dela a silhueta não aparece (fica pra depois).
SILHOUETTE_HLSL = r'''
float d = CD.r;
float z = SD.r;
float st = round(ST.r);
// a unidade está atrás de algo opaco: a profundidade dela passa da da cena (30 cm de folga: a lagarta
// encostada no chão não conta; a 1ª versão comparava ao contrário e nunca acendia, base1 de 13/10)
float hidden = (st > 0.5 && d > z + 30.0) ? 1.0 : 0.0;
float3 c = st < 1.5 ? C1.rgb : (st < 2.5 ? C2.rgb : (st < 3.5 ? C3.rgb : (st < 4.5 ? C4.rgb :
           (st < 5.5 ? C5.rgb : (st < 6.5 ? C6.rgb : (st < 7.5 ? C7.rgb : C8.rgb))))));
// a borda: onde o vizinho não é da unidade, mais forte (a forma lê de longe)
float edge = (round(N1.r) != st || round(N2.r) != st || round(N3.r) != st || round(N4.r) != st) ? 1.0 : 0.0;
return lerp(Scene.rgb, c, hidden * lerp(Fill, 0.9, edge));
'''


def silhouette_material():
    m = fresh('/Game/Materials/M_UnitSilhouette', 'M_UnitSilhouette')
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    loc = getattr(unreal.BlendableLocation, 'BL_SCENE_COLOR_AFTER_TONEMAPPING', None) or getattr(unreal.BlendableLocation, 'BL_AFTER_TONEMAPPING')
    m.set_editor_property('blendable_location', loc)
    ids = unreal.SceneTextureId
    nodes = {}

    def scene(name, tid, y, uv=None):
        e = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, y)
        e.set_editor_property('scene_texture_id', tid)
        if uv is not None:
            mel.connect_material_expressions(uv, '', e, 'UVs')
        nodes[name] = e

    scene('Scene', ids.PPI_POST_PROCESS_INPUT0, -300)
    scene('CD', ids.PPI_CUSTOM_DEPTH, -200)
    scene('SD', ids.PPI_SCENE_DEPTH, -100)
    scene('ST', ids.PPI_CUSTOM_STENCIL, 0)
    # os 4 vizinhos do stencil, a 1,5 px (a borda)
    sp = mel.create_material_expression(m, unreal.MaterialExpressionScreenPosition, -1500, 200)
    inv = mel.create_material_expression(m, unreal.MaterialExpressionViewSize, -1500, 400)
    rcp = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -1350, 400)
    rcp.set_editor_property('const_a', 1.5)
    mel.connect_material_expressions(inv, '', rcp, 'B')
    for k, (ox, oy) in enumerate(((1, 0), (-1, 0), (0, 1), (0, -1))):
        off = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -1200, 100 + k * 120)
        cst = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -1350, 100 + k * 120)
        cst.set_editor_property('r', float(ox))
        cst.set_editor_property('g', float(oy))
        mel.connect_material_expressions(rcp, '', off, 'A')
        mel.connect_material_expressions(cst, '', off, 'B')
        add = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -1050, 100 + k * 120)
        mel.connect_material_expressions(sp, 'ViewportUV', add, 'A')
        mel.connect_material_expressions(off, '', add, 'B')
        scene(f'N{k + 1}', ids.PPI_CUSTOM_STENCIL, 100 + k * 120, add)
    for k in range(8):
        e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 700 + k * 90)
        e.set_editor_property('parameter_name', f'Color{k + 1}')
        e.set_editor_property('default_value', unreal.LinearColor(0.2, 0.5, 1.0, 1.0))
        nodes[f'C{k + 1}'] = e
    f = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 1500)
    f.set_editor_property('parameter_name', 'Fill')
    f.set_editor_property('default_value', 0.42)
    nodes['Fill'] = f
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in nodes:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    c.set_editor_property('code', SILHOUETTE_HLSL)
    ok = [mel.connect_material_expressions(e, '', c, n) for n, e in nodes.items()]
    ok.append(mel.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    if not all(ok):
        raise RuntimeError('M_UnitSilhouette: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_UnitSilhouette ok')


# O vento único do mapa (13/10, BASE_JOGAVEL: "fumaça pra um lado e bandeira pro outro é defeito"): uma coleção
# só, que o jogo preenche no começo da partida pelo Maps.json (wind_yaw, wind_speed). Quem desenha lê daqui:
# Wind = (x, y, velocidade em m/s, rajada 0..1), com x, y a direção pra onde sopra (unitária, eixos da Unreal).
def wind_collection():
    path = '/Game/Materials/MPC_Wind'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        c = unreal.load_asset(path)
    else:
        c = asset_tools.create_asset('MPC_Wind', '/Game/Materials', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    # o material guarda o parâmetro da coleção pelo id (o GUID), não pelo nome: o parâmetro nasce uma vez só e
    # depois só o valor padrão muda (um CollectionVectorParameter novo sorteia outro id e todo material que lê a
    # coleção, os nossos e o chão do WorldForge, perderia o vento; o WorldForge achou, 13/10)
    default = unreal.LinearColor(0.866, 0.5, 4.0, 0.3)
    params = list(c.get_editor_property('vector_parameters'))
    found = [p for p in params if str(p.get_editor_property('parameter_name')) == 'Wind']
    if found:
        found[0].set_editor_property('default_value', default)
    else:
        v = unreal.CollectionVectorParameter()
        v.set_editor_property('parameter_name', 'Wind')
        v.set_editor_property('default_value', default)
        params.append(v)
    c.set_editor_property('vector_parameters', params)
    unreal.EditorAssetLibrary.save_loaded_asset(c)
    # o id não aparece pro Python: o texto exportado do parâmetro traz ele (pra conferir que não mudou)
    try:
        ids = [p.export_text() for p in c.get_editor_property('vector_parameters')]
    except Exception as e:
        ids = [f'(sem o texto: {e})']
    log('MPC_Wind ok', '(o parâmetro de antes: o id fica)' if found else '(parâmetro novo)', ids)


emissive_material()
glow_material()
ground_memory_material()
unit_contact_material()
silhouette_material()
wind_collection()
log('done')
