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
// (14/10, a máscara da poeira da Arte: toda unidade viva grava o bit 128; a silhueta só lê os bits 1–127, o 1 + dono
// de quem é seu ou aliado, então o inimigo continua sem aparecer atrás das coisas)
float st = fmod(round(ST.r), 128.0);
// a unidade está atrás de algo opaco: a profundidade dela passa da da cena (30 cm de folga: a lagarta
// encostada no chão não conta; a 1ª versão comparava ao contrário e nunca acendia, base1 de 13/10)
float hidden = (st > 0.5 && d > z + 30.0) ? 1.0 : 0.0;
float3 c = st < 1.5 ? C1.rgb : (st < 2.5 ? C2.rgb : (st < 3.5 ? C3.rgb : (st < 4.5 ? C4.rgb :
           (st < 5.5 ? C5.rgb : (st < 6.5 ? C6.rgb : (st < 7.5 ? C7.rgb : C8.rgb))))));
// a borda: onde o vizinho não é da unidade, mais forte (a forma lê de longe)
float edge = (fmod(round(N1.r), 128.0) != st || fmod(round(N2.r), 128.0) != st || fmod(round(N3.r), 128.0) != st || fmod(round(N4.r), 128.0) != st) ? 1.0 : 0.0;
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


# A fumaça do míssil (16/10, docs/FUMACA_DO_MISSIL.md; o C++ em RTSMissileSmoke.cpp). Cada puff é um quadrado virado pra
# câmera com o centro gaussiano: UV0 o canto (−1..1), UV1 (τ do centro, semente), UV2 (a transmitância até o sol e até o
# céu, da grade de densidade da CPU), UV3 (o brilho do gás quente, a idade). A luz é toda aqui, na unidade da cena: a luz
# direcional principal (lux) e o céu (a SkyLight), então a exposição é a mesma do resto (sem fator chutado).
SMOKE_BASIS_HLSL = r"""
float2 q = UV0;
float r2 = saturate(dot(q, q));
// (16/10, as luzes perto) o jogo novo manda a vista por parâmetro (a normal do vértice leva a direção das luzes); o velho
// manda a vista no vértice
bool bCpuView = LocalLightsV1 > 0.5 && dot(CamToCpu.rgb, CamToCpu.rgb) > 0.5;
float3 T = normalize(bCpuView ? CamRightCpu.rgb : TWS);
float3 Nn = normalize(bCpuView ? CamToCpu.rgb : NWS);
float3 B = cross(T, Nn);
// (17/10, a v7d) o rastro sombreado como tubo: o rumo dele (a tangente do vértice, posta pelo C++) projetado na tela, e o
// canto sem o componente ao longo dele; sem rumo (a nuvem da largada) o rumo projetado é ~0 e o puff fica esfera
float2 qTube = q;
Perp2 = dot(q, q);
if (LookV7 > 0.5 && bCpuView)
{
	float2 ax = float2(dot(TWS, T), dot(TWS, B));
	float al = length(ax);
	if (al > 0.2)
	{
		ax /= al;
		qTube = q - 0.85 * saturate((al - 0.2) / 0.3) * dot(q, ax) * ax;
		// (17/10, a v7f) a distância de través (o canto sem o componente ao longo do rastro), pro corte do contorno
		float2 qp = q - dot(q, ax) * ax;
		Perp2 = lerp(dot(q, q), dot(qp, qp), saturate((al - 0.2) / 0.3));
	}
}
float r2t = saturate(dot(qTube, qTube));
float3 Ns = normalize(qTube.x * T + qTube.y * B + sqrt(1.0 - r2t) * Nn);
Hgt = 0.5;
if (LookV6 < 0.5)
{
	return Ns;
}
// (17/10, a v6) a altura de ruído presa ao mundo (m), levada pelo vento do mapa (WindCpu: a direção e a velocidade): a mesma
// em todos os quadrados que se sobrepõem. As 3 escalas: as bolotas (~5 m), o fBm (~2 m) e as bolhas do Worley (~1 m)
// (a v6b: no chão o padrão esticava em riscos nos quadrados em pé; no plano da vista, os eixos direita e cima da câmera, ele
// fica redondo, e os quadrados sobrepostos no mesmo ponto da tela leem o mesmo ruído: a profundidade some na projeção)
// (17/10, a v7) o ruído levado pela própria fumaça: o deslocamento médio dos puffs visíveis, somado na CPU (o puff a 2–10 m
// anda mais devagar que o vento de 10 m e sobe: na v6 o detalhe escorregava por dentro do rastro)
float3 Wv = LookV7 > 0.5 ? NoiseOffCpu.rgb * 100.0 : float3(WindCpu.xy, 0.0) * (WindCpu.z * Time * 100.0);
float2 w0 = float2(dot(Pabs - Wv, T), dot(Pabs - Wv, B)) * 0.01;
// (a v6d: o ruído fino lido sem mip a ~2 cm por texel serrilhava em fiapos a 67 m; agora com mip, as escalas finas em 14 m e
// 8 m, e o relevo só das bolotas grandes)
float hR = Texture2DSample(Noise, NoiseSampler, w0 / 24.0).r;
float h0 = 0.6 * hR + 0.25 * Texture2DSample(Noise, NoiseSampler, w0 / 14.0 + 0.37).g + 0.15 * Texture2DSample(Noise, NoiseSampler, w0 / 8.0 + 0.71).b;
float hT = Texture2DSample(Noise, NoiseSampler, (w0 + float2(0.5, 0.0)) / 24.0).r;
float hB = Texture2DSample(Noise, NoiseSampler, (w0 + float2(0.0, 0.5)) / 24.0).r;
Hgt = h0;
if (LookV7 > 0.5)
{
	// (17/10, a v7b) a borda: o Worley de ~0,5 m (o mosaico de 4 m, com mip) entra na altura que mexe no corte; sem ele a
	// bolota sozinha saía um disco redondo
	float hEdge = Texture2DSample(Noise, NoiseSampler, w0 / 4.0 + 0.53).b;
	Hgt = 0.55 * h0 + 0.45 * hEdge;
}
// a normal de billow: a esfera inclinada contra o gradiente da altura (0,35 m entre as amostras; o ganho dá ~0,3 de
// inclinação numa bolota típica) [E]
return normalize(Ns - 4.0 * ((hT - hR) * T + (hB - hR) * B));
"""

SMOKE_HLSL = r"""
float2 q = UV0;
float r2 = dot(q, q);
Opacity = 0.0;
if (r2 >= 1.0)
{
	return float3(0.0, 0.0, 0.0);
}
float tauC = UV1.x;
float seed = UV1.y;
float sunT = UV2.x;
float skyT = UV2.y;
float glow = UV3.x;
float age = UV3.y;
// (16/10) o tipo: a "chama" negativa é a fuligem do diesel (Bond e Bergstrom 2006: o albedo de espalhamento simples da
// fuligem fresca é 0,20–0,30, o do diesel MEDIDO 0,17 ± 0,05 e 0,20 ± 0,01: aqui o do diesel, ω_G = 0,20; a absorção
// ∝ 1/λ, então o azul absorve mais: R/G/B 0,85/1/1,22 da absorção; a fração pra trás ~0,17, g ~0,5). A do míssil é o
// Al2O3 branco (0,97, g 0,7)
bool soot = glow < -0.5;
if (soot && Debug > 3.5)
{
	// (16/10) a depuração 4: a fuligem em magenta (pra achar onde ela nasce e se é desenhada)
	Opacity = max(1.0 - exp(-tauC * exp(-2.42 * r2)), 0.3 * (1.0 - r2));
	return float3(1.0, 0.0, 1.0) * 2.0e4;
}
float3 albedo3 = soot ? float3(0.227, 0.2, 0.17) : float3(0.97, 0.97, 0.97);
float omegaMs = soot ? 0.2 : 0.97;
float gFwd = soot ? 0.5 : 0.7;
glow = max(glow, 0.0);
// o detalhe: o Perlin-Worley (R) e o fBm fino (G) do ruído que se repete, girando devagar e escolhido pela semente
float ang = seed * 6.2831853 + age * 0.12;
float cs = cos(ang);
float sn = sin(ang);
float2 p = float2(cs * q.x - sn * q.y, sn * q.x + cs * q.y);
float2 o = frac(seed * float2(7.13, 3.71));
float n1 = Texture2DSampleLevel(Noise, NoiseSampler, p * 0.32 + o + age * float2(0.012, 0.007), 0).r;
float n2 = Texture2DSampleLevel(Noise, NoiseSampler, p * 0.85 + o.yx - age * float2(0.03, 0.02), 0).g;
float n = n1 * 0.7 + n2 * 0.3;
// a gaussiana do puff projetada (o quadrado tem 2,2 σ de meio lado: e^(−ρ²/2σ²) = e^(−2,42 r²)); o ruído come mais na borda
float prof = exp(-2.42 * r2);
float erode = lerp(0.55 + 0.9 * n, 2.0 * n, saturate(r2 * 1.3));
float tau = tauC * prof * erode;
bool v6 = LookV6 > 0.5;
if (v6 && LookV7 > 0.5)
{
	// (17/10, a v7) o contorno visível (Roberts 1923; Gifford 1959): a borda que se vê é onde a coluna de fumaça passa de um
	// valor fixo τ*, que sobe com a idade. A CPU manda o corte relativo ao centro deste puff (UV1.y = τ*/τ da linha no centro
	// dele): cada quadrado corta no mesmo contorno absoluto, então a borda da soma sai nítida e o rastro encolhe e se parte até
	// sumir, em vez de engordar e desbotar. A interface turbulenta (Westerweel 2005) pela altura presa ao mundo: o corte desce
	// onde ela é alta (a bolota) e sobe onde é baixa, mais forte depois do jato (~0,8 s); a rampa até 1,5 τ* tira o estalo
	float cut0 = UV1.y;
	float ce = cut0 * lerp(1.0, 0.45 + 1.1 * (1.0 - Hgt), saturate(age / 0.8));
	// (17/10, a v7f) o corte pelo perfil de través: somado ao longo do rastro dá o contorno da linha, sem pescoço entre
	// os vizinhos (sem rumo, o perfil de través é o próprio perfil: τ0 (perfil − corte), o de antes)
	float profP = exp(-2.42 * Perp2);
	float xc = profP - ce;
	tau = tauC * prof * saturate(1.0 - ce / max(profP, 1e-3)) * saturate(xc / max(0.5 * ce, 0.02));
}
else if (v6)
{
	// (17/10, a v6) o chapéu no puff novo (p de 2,2 caindo pra 1 em 4 s) e a erosão por limiar pela altura presa ao mundo:
	// a borda come onde ela é baixa (mais forte com a idade), o miolo fica denso
	// (a v6c: com p 2,2 cada puff virava um disco de borda dura, e as bordas de muitos discos ao longo do rastro faziam fiapos
	// paralelos; o chapéu fica leve, p 1,35, e quem recorta a borda é a erosão)
	float pHat = lerp(1.35, 1.0, saturate(age / 4.0));
	prof = exp(-pow(max(2.42 * r2, 1e-4), pHat));
	float kEro = lerp(0.55, 0.85, saturate(age / 4.0));
	float cut = kEro * (1.0 - Hgt);
	tau = tauC * saturate((prof - cut) / max(1.0 - cut, 0.05));
}
float alpha = 1.0 - exp(-tau);
// o sol da CPU (SunDirCpu, SunLuxCpu: a luz do mapa lida no jogo) quando vale; senão o nó da luz principal
// o parâmetro de vetor entra como float3 (sem o w): vale quando a iluminância da CPU não é zero
bool bCpuSun = dot(SunLuxCpu.rgb, float3(1.0, 1.0, 1.0)) > 0.0;
float3 Ln = normalize(bCpuSun ? SunDirCpu.rgb : L);
float3 Esun0 = bCpuSun ? SunLuxCpu.rgb : E;
float3 Vn = normalize(V);
float NdL = dot(N, Ln);
// o caminho até o sol: os outros puffs (a CPU) mais a espessura do rastro aqui (os vizinhos somam ~3× o τ do puff; 0 do
// lado do sol, ~3 τC do outro: o 1º vídeo saiu chapado com só o próprio puff)
float tauSun = -log(max(sunT, 1e-4)) + 1.5 * tauC * (1.0 - NdL);
float cosT = dot(-Ln, Vn);
// Henyey–Greenstein de dois lobos (pra frente g 0,7 a 85%, pra trás g −0,2: o Al2O3 de mícrons espalha pra frente) e o
// espalhamento múltiplo por oitavas (Wrenninge, Kulla e Lundqvist 2013; Hillaire 2016): a, b e c caindo pela metade
float3 S = float3(0.0, 0.0, 0.0);
float a = 1.0;
float b = 1.0;
float c = 1.0;
[unroll] for (int i = 0; i < 3; ++i)
{
	float g1 = gFwd * c;
	float g2 = -0.2 * c;
	float h1 = (1.0 - g1 * g1) / pow(max(1.0 + g1 * g1 - 2.0 * g1 * cosT, 1e-4), 1.5);
	float h2 = (1.0 - g2 * g2) / pow(max(1.0 + g2 * g2 - 2.0 * g2 * cosT, 1e-4), 1.5);
	S += a * (0.85 * h1 + 0.15 * h2) / 12.566371 * exp(-b * tauSun);
	a *= 0.5 * omegaMs;   // cada espalhamento a mais leva o albedo de novo: a fuligem quase não tem o 2º (~6%)
	b *= 0.5;
	c *= 0.5;
}
// o "pó" (Beer–Powder, Schneider 2015): a borda fina do lado de costas pro sol escurece (o miolo da nuvem)
float powder = lerp(1.0, saturate(2.0 * (1.0 - exp(-2.0 * tau))), 0.5 * saturate(-NdL));
if (v6)
{
	// (17/10, a v6) o pó como no Schneider: as bordas finas escuras do lado aceso, com a vista de costas pro sol
	powder = lerp(1.0, saturate(2.0 * (1.0 - exp(-2.0 * tau))), 0.6 * saturate(0.5 - 0.5 * cosT));
}
float3 Lsun = albedo3 * Esun0 * S * powder;
if (v6)
{
	// (17/10, a v6) o miolo grosso se acende como superfície (a nuvem grossa: ρ E cos/π, com a sombra dos outros puffs): a
	// bolota tem o lado do sol e o lado da sombra; o fino continua pela fase (o contraluz prateado)
	float3 Lamb = albedo3 * Esun0 * saturate(NdL) * sunT / 3.14159265;
	Lsun = lerp(Lsun, Lamb, saturate((tau - 0.8) / 2.5));
}
// (16/10) a sombra do mapa (o mapa do sol comum, RTSSunShadow): no ponto do quadrado (o meio do puff na linha da vista), a
// faixa de altura que a corda do puff atravessa (a conta da poeira da Arte: o quanto a corda anda na altura da sombra), a
// borda de ~0,15 m; multiplica o sol em todas as oitavas (a 2ª revisão do estudo: fora delas a fumaça na sombra da casa
// ainda pegaria sol pelas oitavas)
if (SunValid > 0.5)
{
	FRTSSunShadow Sm;
	Sm.CamToMapCm = SunToMap;
	Sm.Axis = SunAxis;
	Sm.TanE = SunTanE;
	Sm.Window = SunWindow;
	float dq = length(ddx(UV0)) + length(ddy(UV0));
	float dp = length(ddx(Pcr)) + length(ddy(Pcr));
	float sigM = dp / max(dq, 1e-5) / 220.0;                    // o meio lado do quadrado é 2,2 σ (cm)
	float3 Dv = -Vn;
	float chordF = abs(Dv.z + SunTanE * dot(Dv.xy, SunAxis.xy));
	float halfM = sigM * sqrt(saturate(1.0 - r2)) * chordF;
	float Vsun = RTSSunVisibilityPCF(SunHMap, SunHMapSampler, SunTMap, SunTMapSampler, Sm, Pabs, halfM, 0.15, SunMapTexels);
	Lsun *= Vsun;
	if (Debug > 2.5)
	{
		// (16/10) a depuração 3: onde o mapa do sol diz sombra a fumaça fica vermelha (o sol que sobra no verde e no azul)
		Opacity = alpha;
		return float3(1.0, Vsun, Vsun) * 2.0e4;
	}
}
// (16/10) as luzes perto (as explosões, o fogo, os tiros; RTSSceneLights somadas por puff no jogo): a iluminância em RGBE
// na cor do vértice (Ward 1991) e a direção na normal do vértice; a mesma fase de dois lobos e as mesmas oitavas do sol,
// com a sombra até a luz pela do céu (o combinado no docs/ESTUDO_LUZ_NA_FUMACA.md) mais a espessura do próprio rastro
float3 Lloc = float3(0.0, 0.0, 0.0);
if (LocalLightsV1 > 0.5 && dot(CamToCpu.rgb, CamToCpu.rgb) > 0.5 && VCA > 0.0)
{
	float3 El = (VC * 255.0 + 0.5) * exp2(VCA * 255.0 - 136.0);
	float3 Wl = normalize(LD);
	float NdLl = dot(N, Wl);
	float tauLoc = -log(max(skyT, 1e-4)) + 1.5 * tauC * (1.0 - NdLl);
	float cosL = dot(-Wl, Vn);
	float3 Sl = float3(0.0, 0.0, 0.0);
	float al = 1.0;
	float bl = 1.0;
	float cl = 1.0;
	[unroll] for (int j = 0; j < 3; ++j)
	{
		float g1 = gFwd * cl;
		float g2 = -0.2 * cl;
		float h1 = (1.0 - g1 * g1) / pow(max(1.0 + g1 * g1 - 2.0 * g1 * cosL, 1e-4), 1.5);
		float h2 = (1.0 - g2 * g2) / pow(max(1.0 + g2 * g2 - 2.0 * g2 * cosL, 1e-4), 1.5);
		Sl += al * (0.85 * h1 + 0.15 * h2) / 12.566371 * exp(-bl * tauLoc);
		al *= 0.5 * omegaMs;
		bl *= 0.5;
		cl *= 0.5;
	}
	float powderL = lerp(1.0, saturate(2.0 * (1.0 - exp(-2.0 * tau))), 0.5 * saturate(-NdLl));
	Lloc = albedo3 * El * Sl * powderL;
}
float3 Lsky = albedo3 * skyT * (0.5 * SkyN + 0.5 * SkyUp);
if (v6)
{
	// (17/10, a v6) o céu pelo hemisfério da normal e o chão por baixo (a areia, albedo ~0,42 [E], acesa pelo sol e pelo céu):
	// a barriga fica com o tom do chão, não com o do céu
	float3 Gnd = 0.42 * (Esun0 * saturate(Ln.z) + 3.14159265 * SkyUp) / 3.14159265;
	Lsky = albedo3 * skyT * (SkyN * saturate(0.55 + 0.45 * N.z) + Gnd * saturate(0.45 - 0.45 * N.z));
}
// o gás ainda incandescente logo atrás da chama e o calor de uma explosão perto [E: ~2·10⁵ cd/m², laranja do Al2O3 quente]
float3 Lglow = glow * float3(1.0, 0.52, 0.22) * 2.0e5 * prof;
Opacity = alpha;
return Lsun + Lsky + Lloc + Lglow;
"""

SMOKE_SHADOW_HLSL = r"""
// a sombra pela profundidade da cena: o ponto atrás deste pixel (o chão de verdade ou o tanque), a distância dele ao raio
// do sol que passa pelo centro do puff e a gaussiana (o mesmo perfil do sprite); tudo em cm, relativo à câmera
float tauC = UV1.x;
float seed = UV1.y;
float3 Ps = Pq * (SD.x / max(PD.x, 1.0));
float3 C = Pq + float3(UV2.x, UV2.y, UV3.x);
float sigma = max(UV3.y, 1.0);
// o parâmetro de vetor entra como float3 (sem o w): vale quando a iluminância da CPU não é zero
bool bCpuSun = dot(SunLuxCpu.rgb, float3(1.0, 1.0, 1.0)) > 0.0;
float3 Ln = normalize(bCpuSun ? SunDirCpu.rgb : L);
float3 Esun0 = bCpuSun ? SunLuxCpu.rgb : E;
if (Debug > 0.5 && Debug < 1.5)
{
	return float3(0.6, 1.0, 0.6);   // a cobertura dos quadrados (depuração)
}
float3 Dv = C - Ps;
float along = dot(Dv, Ln);
if (along <= 0.0 || tauC <= 0.0)
{
	return float3(1.0, 1.0, 1.0);
}
float3 perp = Dv - Ln * along;
float d2 = dot(perp, perp) / (sigma * sigma);
if (d2 > 10.0)
{
	return float3(1.0, 1.0, 1.0);
}
float2 o = frac(seed * float2(7.13, 3.71));
float n = Texture2DSampleLevel(Noise, NoiseSampler, perp.xy / (4.4 * sigma) + o, 0).r;
float r2 = d2 / 4.84;
float tau = tauC * exp(-0.5 * d2) * lerp(0.55 + 0.9 * n, 2.0 * n, saturate(r2 * 1.3));
if (LookV7 > 0.5)
{
	// (17/10, a v7) o mesmo contorno da fumaça (UV1.y é o corte relativo, não a semente)
	float xc = exp(-0.5 * d2) - seed;
	tau = tauC * max(xc, 0.0) * saturate(xc / max(0.5 * seed, 0.02));
}
// o chão debaixo perde o sol direto pela coluna do puff (Beer–Lambert); o céu continua: a sombra fica tão escura quanto
// a dos prédios na mesma luz (a razão sombra/sol da cena)
float Tsh = exp(-tau * (Debug > 1.5 ? 4.0 : 1.0));
float3 Esun = Esun0 * saturate(Ln.z);
float3 Esky = SkyUp * 3.14159265;
if (dot(Esky, float3(1.0, 1.0, 1.0)) < 1e-2 * dot(Esun, float3(1.0, 1.0, 1.0)))
{
	Esky = SkyIrrCpu.rgb;   // (17/10) o céu da CPU: aqui o motor não manda o dele
}
// (16/10) o chão na sombra de uma casa já perdeu o sol: a sombra do rastro ali não escurece de novo (a sombra dupla). O ponto
// em coordenada absoluta, 15 cm pro lado do sol (o viés de sombra: o teto do tanque não se sombreia sozinho)
float Vg = 1.0;
if (SunValid > 0.5)
{
	FRTSSunShadow Sm;
	Sm.CamToMapCm = SunToMap;
	Sm.Axis = SunAxis;
	Sm.TanE = SunTanE;
	Sm.Window = SunWindow;
	Vg = RTSSunVisibilityPCF(SunHMap, SunHMapSampler, SunTMap, SunTMapSampler, Sm, Ps + CamPos + Ln * 15.0, 0.0, 0.1, SunMapTexels);
}
// a parte do sol na luz que chegava ao chão (0 na sombra da casa, 1 sem céu): a sombra do rastro tira só ela
float3 fsun = saturate(Vg * Esun / max(Esky + Vg * Esun, 1e-6 * (Esun + 1.0)));
return 1.0 - (1.0 - Tsh) * fsun;
"""


def _fx_fresh(folder, name):
    path = folder + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def _custom(m, x, y, code, inputs, out_type, extra=()):
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property('output_type', out_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n, t in extra:
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    if outs:
        c.set_editor_property('additional_outputs', outs)
    c.set_editor_property('code', code)
    return c


def _uv(m, index, x, y):
    t = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, x, y)
    t.set_editor_property('coordinate_index', index)
    return t


# (16/10) o mapa do sol comum (o plugin RTSSunShadow da Arte): as entradas dos nós Custom que leem ele por pixel
SUN_MAP_INPUTS = ('SunValid', 'SunHMap', 'SunTMap', 'SunToMap', 'SunAxis', 'SunWindow', 'SunTanE', 'SunMapTexels')


def _sun_map_params(m, custom, default_tex, x, y):
    ok = True
    for k, name in enumerate(('SunValid', 'SunTanE', 'SunMapTexels')):
        p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y + 100 * k)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', 0.0)
        ok = mel.connect_material_expressions(p, '', custom, name) and ok
    for k, name in enumerate(('SunToMap', 'SunAxis', 'SunWindow')):
        p = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y + 300 + 120 * k)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
        ok = mel.connect_material_expressions(p, '', custom, name) and ok
    for k, name in enumerate(('SunHMap', 'SunTMap')):
        p = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, x, y + 700 + 120 * k)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('texture', default_tex)   # linear (o ruído): o tipo do amostrador bate com o mapa do jogo
        p.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        ok = mel.connect_material_expressions(p, '', custom, name) and ok
    return ok


def missile_smoke():
    import os
    folder = '/Game/FX/MissileSmoke'
    png = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools', 'data', 'T_SmokeNoise.png')
    if not os.path.exists(png):
        raise RuntimeError('falta o ruído: python Game/Tools/make_smoke_noise.py (%s)' % png)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', png)
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', 'T_SmokeNoise')
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    asset_tools.import_asset_tasks([task])
    noise = unreal.load_asset(folder + '/T_SmokeNoise')
    noise.set_editor_property('srgb', False)
    noise.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(noise)

    # a fumaça
    m = _fx_fresh(folder, 'M_MissileSmoke')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', True)
    uv = [_uv(m, k, -1400, 200 * k) for k in range(4)]
    tws = mel.create_material_expression(m, unreal.MaterialExpressionVertexTangentWS, -1400, 900)
    nws = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -1400, 1000)
    # (16/10) as luzes perto: a vista por parâmetro, a cor do vértice (RGBE) e a normal do vértice (a direção delas)
    llv = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1400, 1300)
    llv.set_editor_property('parameter_name', 'LocalLightsV1')
    llv.set_editor_property('default_value', 1.0)
    camr = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1400, 1400)
    camr.set_editor_property('parameter_name', 'CamRightCpu')
    camr.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    camto = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1400, 1550)
    camto.set_editor_property('parameter_name', 'CamToCpu')
    camto.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    vcol = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -1400, 1700)
    basis = _custom(m, -1100, 900, SMOKE_BASIS_HLSL, ('UV0', 'TWS', 'NWS', 'CamRightCpu', 'CamToCpu', 'LocalLightsV1', 'Pabs', 'Time', 'WindCpu', 'LookV6', 'Noise',
                                                     'LookV7', 'NoiseOffCpu'),
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, (('Hgt', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                                                  ('Perp2', unreal.CustomMaterialOutputType.CMOT_FLOAT1)))
    # (17/10, a v6) o tempo, o vento do mapa (posto pelo jogo) e a chave da versão
    mtime = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1400, 2400)
    mwind = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1400, 2500)
    mwind.set_editor_property('parameter_name', 'WindCpu')
    mwind.set_editor_property('default_value', unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
    mv6 = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1400, 2650)
    mv6.set_editor_property('parameter_name', 'LookV6')
    mv6.set_editor_property('default_value', 1.0)
    # (17/10, a v7) a chave (0 no jogo velho) e o deslocamento do ruído (m), postos pelo jogo
    mv7 = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1400, 2800)
    mv7.set_editor_property('parameter_name', 'LookV7')
    mv7.set_editor_property('default_value', 0.0)
    mnoff = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1400, 2950)
    mnoff.set_editor_property('parameter_name', 'NoiseOffCpu')
    mnoff.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    sky_n = mel.create_material_expression(m, unreal.MaterialExpressionSkyLightEnvMapSample, -800, 900)
    sky_up = mel.create_material_expression(m, unreal.MaterialExpressionSkyLightEnvMapSample, -800, 1100)
    up = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -1100, 1150)
    up.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    one = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -1100, 1250)
    one.set_editor_property('r', 1.0)
    # (17/10 noite) sem o nó do sol principal (MainDirectionalLight): ele não existe na variação do Lumen dos translúcidos
    # (FLumenTranslucencyRadianceCacheMarkPS), e o material inteiro caía no padrão da Unreal, cinza e opaco: a fumaça sumia e
    # os quads da sombra viravam as faixas cinza atrás do Patriot (o pacote cozinhava assim sempre; o editor, depois da troca
    # de qualidade). O sol vem do jogo (SunDirCpu, SunLuxCpu, o shader já preferia eles); estes só preenchem as entradas
    sun_l = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -800, 1300)
    sun_l.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    sun_e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -800, 1380)
    sun_e.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -800, 1450)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -800, 1550)
    tex.set_editor_property('parameter_name', 'Noise')
    tex.set_editor_property('texture', noise)
    sdir = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, 1700)
    sdir.set_editor_property('parameter_name', 'SunDirCpu')
    sdir.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    slux = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, 1850)
    slux.set_editor_property('parameter_name', 'SunLuxCpu')
    slux.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    smoke = _custom(m, -400, 400, SMOKE_HLSL, ('UV0', 'UV1', 'UV2', 'UV3', 'N', 'V', 'L', 'E', 'SkyN', 'SkyUp', 'Noise', 'SunDirCpu', 'SunLuxCpu',
                                               'VC', 'VCA', 'LD', 'CamToCpu', 'LocalLightsV1') + SUN_MAP_INPUTS + ('Pcr', 'Pabs', 'Debug', 'Hgt', 'LookV6', 'LookV7', 'Perp2'),
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, (('Opacity', unreal.CustomMaterialOutputType.CMOT_FLOAT1),))
    smoke.set_editor_property('include_file_paths', ['/Plugin/RTSSunShadow/Private/SunShadow.ush'])
    pcr = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1400, 2000)
    pcr.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    pabs = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1400, 2100)
    sun_map_ok = _sun_map_params(m, smoke, noise, -1100, 2200)
    mdbg = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1400, 2250)
    mdbg.set_editor_property('parameter_name', 'Debug')
    mdbg.set_editor_property('default_value', 0.0)
    fade = mel.create_material_expression(m, unreal.MaterialExpressionDepthFade, -150, 600)
    fade.set_editor_property('fade_distance_default', 80.0)
    ok = [mel.connect_material_expressions(uv[0], '', basis, 'UV0'),
          mel.connect_material_expressions(tws, '', basis, 'TWS'),
          mel.connect_material_expressions(nws, '', basis, 'NWS'),
          mel.connect_material_expressions(camr, '', basis, 'CamRightCpu'),
          mel.connect_material_expressions(camto, '', basis, 'CamToCpu'),
          mel.connect_material_expressions(llv, '', basis, 'LocalLightsV1'),
          mel.connect_material_expressions(vcol, '', smoke, 'VC'),
          mel.connect_material_expressions(vcol, 'A', smoke, 'VCA'),
          mel.connect_material_expressions(nws, '', smoke, 'LD'),
          mel.connect_material_expressions(camto, '', smoke, 'CamToCpu'),
          mel.connect_material_expressions(llv, '', smoke, 'LocalLightsV1'),
          mel.connect_material_expressions(basis, '', sky_n, 'Direction'),
          mel.connect_material_expressions(one, '', sky_n, 'Roughness'),
          mel.connect_material_expressions(up, '', sky_up, 'Direction'),
          mel.connect_material_expressions(one, '', sky_up, 'Roughness'),
          mel.connect_material_expressions(uv[0], '', smoke, 'UV0'),
          mel.connect_material_expressions(uv[1], '', smoke, 'UV1'),
          mel.connect_material_expressions(uv[2], '', smoke, 'UV2'),
          mel.connect_material_expressions(uv[3], '', smoke, 'UV3'),
          mel.connect_material_expressions(basis, '', smoke, 'N'),
          mel.connect_material_expressions(cam, '', smoke, 'V'),
          mel.connect_material_expressions(sun_l, '', smoke, 'L'),
          mel.connect_material_expressions(sun_e, '', smoke, 'E'),
          mel.connect_material_expressions(sky_n, '', smoke, 'SkyN'),
          mel.connect_material_expressions(sky_up, '', smoke, 'SkyUp'),
          mel.connect_material_expressions(tex, '', smoke, 'Noise'),
          mel.connect_material_expressions(sdir, '', smoke, 'SunDirCpu'),
          mel.connect_material_expressions(slux, '', smoke, 'SunLuxCpu'),
          mel.connect_material_expressions(pcr, '', smoke, 'Pcr'),
          mel.connect_material_expressions(pabs, '', smoke, 'Pabs'),
          mel.connect_material_expressions(mdbg, '', smoke, 'Debug'),
          mel.connect_material_expressions(pabs, '', basis, 'Pabs'),
          mel.connect_material_expressions(mtime, '', basis, 'Time'),
          mel.connect_material_expressions(mwind, '', basis, 'WindCpu'),
          mel.connect_material_expressions(mv6, '', basis, 'LookV6'),
          mel.connect_material_expressions(tex, '', basis, 'Noise'),
          mel.connect_material_expressions(basis, 'Hgt', smoke, 'Hgt'),
          mel.connect_material_expressions(basis, 'Perp2', smoke, 'Perp2'),
          mel.connect_material_expressions(mv6, '', smoke, 'LookV6'),
          mel.connect_material_expressions(mv7, '', basis, 'LookV7'),
          mel.connect_material_expressions(mnoff, '', basis, 'NoiseOffCpu'),
          mel.connect_material_expressions(mv7, '', smoke, 'LookV7'),
          sun_map_ok,
          mel.connect_material_expressions(smoke, 'Opacity', fade, 'Opacity'),
          mel.connect_material_property(smoke, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
          mel.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError('M_MissileSmoke: ligações falharam %s' % ok)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    log('M_MissileSmoke ok')

    # a sombra no chão
    s = _fx_fresh(folder, 'M_MissileSmokeShadow')
    s.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    s.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MODULATE)
    s.set_editor_property('two_sided', True)
    s.set_editor_property('disable_depth_test', True)   # a sombra pela profundidade da cena: o próprio shader acha o ponto
    suv = [_uv(s, k, -1100, 200 * k) for k in range(4)]
    spq = mel.create_material_expression(s, unreal.MaterialExpressionWorldPosition, -1100, 900)
    spq.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    ssd = mel.create_material_expression(s, unreal.MaterialExpressionSceneDepth, -1100, 1000)
    spd = mel.create_material_expression(s, unreal.MaterialExpressionPixelDepth, -1100, 1100)
    ssky = mel.create_material_expression(s, unreal.MaterialExpressionSkyLightEnvMapSample, -800, 600)
    sup = mel.create_material_expression(s, unreal.MaterialExpressionConstant3Vector, -1100, 600)
    sup.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    sone = mel.create_material_expression(s, unreal.MaterialExpressionConstant, -1100, 700)
    sone.set_editor_property('r', 1.0)
    # (17/10 noite) sem o nó do sol principal, pelo mesmo motivo da fumaça (o sol vem do jogo)
    ssun_l = mel.create_material_expression(s, unreal.MaterialExpressionConstant3Vector, -800, 800)
    ssun_l.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    ssun_e = mel.create_material_expression(s, unreal.MaterialExpressionConstant3Vector, -800, 880)
    ssun_e.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    stex = mel.create_material_expression(s, unreal.MaterialExpressionTextureObjectParameter, -800, 950)
    stex.set_editor_property('parameter_name', 'Noise')
    stex.set_editor_property('texture', noise)
    ssdir = mel.create_material_expression(s, unreal.MaterialExpressionVectorParameter, -800, 1100)
    ssdir.set_editor_property('parameter_name', 'SunDirCpu')
    ssdir.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    sslux = mel.create_material_expression(s, unreal.MaterialExpressionVectorParameter, -800, 1250)
    sslux.set_editor_property('parameter_name', 'SunLuxCpu')
    sslux.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    sdbg = mel.create_material_expression(s, unreal.MaterialExpressionScalarParameter, -800, 1400)
    sdbg.set_editor_property('parameter_name', 'Debug')
    sdbg.set_editor_property('default_value', 0.0)
    sh = _custom(s, -400, 300, SMOKE_SHADOW_HLSL, ('UV0', 'UV1', 'UV2', 'UV3', 'Pq', 'SD', 'PD', 'L', 'E', 'SkyUp', 'Noise', 'SunDirCpu', 'SunLuxCpu', 'Debug')
                 + SUN_MAP_INPUTS + ('CamPos', 'LookV7', 'SkyIrrCpu'), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sskyc = mel.create_material_expression(s, unreal.MaterialExpressionVectorParameter, -800, 1700)
    sskyc.set_editor_property('parameter_name', 'SkyIrrCpu')
    sskyc.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    sv7 = mel.create_material_expression(s, unreal.MaterialExpressionScalarParameter, -800, 1550)
    sv7.set_editor_property('parameter_name', 'LookV7')
    sv7.set_editor_property('default_value', 0.0)
    sh.set_editor_property('include_file_paths', ['/Plugin/RTSSunShadow/Private/SunShadow.ush'])
    scam = mel.create_material_expression(s, unreal.MaterialExpressionCameraPositionWS, -1100, 1300)
    sun_map_ok_s = _sun_map_params(s, sh, noise, -1100, 1500)
    ok = [mel.connect_material_expressions(suv[0], '', sh, 'UV0'),
          mel.connect_material_expressions(suv[1], '', sh, 'UV1'),
          mel.connect_material_expressions(suv[2], '', sh, 'UV2'),
          mel.connect_material_expressions(suv[3], '', sh, 'UV3'),
          mel.connect_material_expressions(spq, '', sh, 'Pq'),
          mel.connect_material_expressions(ssd, '', sh, 'SD'),
          mel.connect_material_expressions(spd, '', sh, 'PD'),
          mel.connect_material_expressions(sup, '', ssky, 'Direction'),
          mel.connect_material_expressions(sone, '', ssky, 'Roughness'),
          mel.connect_material_expressions(ssun_l, '', sh, 'L'),
          mel.connect_material_expressions(ssun_e, '', sh, 'E'),
          mel.connect_material_expressions(ssky, '', sh, 'SkyUp'),
          mel.connect_material_expressions(stex, '', sh, 'Noise'),
          mel.connect_material_expressions(ssdir, '', sh, 'SunDirCpu'),
          mel.connect_material_expressions(sslux, '', sh, 'SunLuxCpu'),
          mel.connect_material_expressions(sdbg, '', sh, 'Debug'),
          mel.connect_material_expressions(scam, '', sh, 'CamPos'),
          mel.connect_material_expressions(sv7, '', sh, 'LookV7'),
          mel.connect_material_expressions(sskyc, '', sh, 'SkyIrrCpu'),
          sun_map_ok_s,
          mel.connect_material_property(sh, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    if not all(ok):
        raise RuntimeError('M_MissileSmokeShadow: ligações falharam %s' % ok)
    mel.recompile_material(s)
    unreal.EditorAssetLibrary.save_loaded_asset(s)
    log('M_MissileSmokeShadow ok')


# BUILD_CONTENT_ONLY=silhouette,glow (14/10): só esses, sem refazer o resto (de noite, sem ninguém pra conferir os outros)
_only = [x.strip() for x in __import__('os').environ.get('BUILD_CONTENT_ONLY', '').split(',') if x.strip()]
for _name, _fn in (('emissive', emissive_material), ('glow', glow_material), ('ground_memory', ground_memory_material),
                   ('unit_contact', unit_contact_material), ('silhouette', silhouette_material), ('wind', wind_collection),
                   ('missile_smoke', missile_smoke)):
    if not _only or _name in _only:
        _fn()
log('done')
