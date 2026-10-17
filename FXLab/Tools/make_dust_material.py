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

# DUST_GROUND (o plugin do jogo): a direção do sol vem do normal do vértice e o VC traz o albedo do chão onde a poeira
# nasceu (sRGB). A cor segue a regra de Hapke: o pó fino espalha com o albedo de espalhamento simples
# w = 1 − ((1 − A)/(1 + A))² (mais claro e mais pálido que o chão, que escurece pelas sombras entre os grãos); a parte
# densa fica com a cor do chão, o véu com a do grão (Hapke 1981; Bowers & Hanks 1965). Albedo vira o ganho G, um só pra
# todo chão (a normalização da luz do cozimento), calibrado na luz do jogo. GroundRule = 0 volta à cor do VC sem Hapke.
if globals().get('DUST_GROUND', False):
    old_l = 'float3 L = VC.rgb * 2.0 - 1.0;'
    assert old_l in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a linha da direção do sol'
    HLSL = HLSL.replace(old_l, 'float3 L = normalize(Nw);                      // a direção do sol no cartão (o normal do vértice)')
    old_c = '* dk * Albedo * Tint.rgb'
    assert old_c in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a cor'
    HLSL = HLSL.replace('float3 col = (', '''float3 Vs = saturate(VC.rgb);
float3 Ag = lerp(pow((Vs + 0.055) / 1.055, 2.4), Vs / 12.92, step(Vs, 0.04045));   // o albedo do chão (sRGB no vértice)
float3 Gq = (1.0 - Ag) / (1.0 + Ag);
float3 Ws = 1.0 - Gq * Gq;
float3 Wy = float3(0.2126, 0.7152, 0.0722);
float3 Cf = Ws * dot(Ag, Wy) / max(dot(Ws, Wy), 1e-4);
float3 DustAlbedo = Albedo * lerp(Ag, lerp(Cf, Ag, pow(saturate(alpha), max(EmitScale, 0.01))), GroundRule);   // EmitScale (sem uso na poeira: não tem fogo) leva o expoente do pálido
DustAlbedo = lerp(dot(DustAlbedo, Wy).xxx, DustAlbedo, saturate(EmberCut));   // EmberCut (sem uso na poeira) leva a saturação relativa (1 = como a regra dá)   // EmitScale (sem uso na poeira: não tem fogo) leva o expoente do pálido
float3 col = (''', 1)
    HLSL = HLSL.replace(old_c, '* dk * DustAlbedo')
    # o mip mínimo: a folha lida mais borrada (o mip de cima): os bolinhos miúdos da gravação viram montes macios, a forma
    # grande fica (o "menos barulhento" do game shot, 14/10). O nível é max(o da distância, LightScale.z): o borrado tem o
    # mesmo tamanho no mundo em todo zoom e o octógono (atlas_cut8.py com lod) cobre o alfa espalhado (16/10)
    old_pp = 'float4 Pp = lerp('
    assert old_pp in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a leitura da SixPos'
    HLSL = HLSL.replace(old_pp, 'float DustLod = max(SixPos.CalculateLevelOfDetail(SixPosSampler, a), LightScale.z);\n' + old_pp, 1)
    for tex in ('SixPos', 'SixNeg'):
        for uvn in ('a', 'b'):
            old_s = f'Texture2DSample({tex}, {tex}Sampler, {uvn})'
            assert old_s in HLSL, 'o HLSL do M_FXSixWay mudou: conferir ' + old_s
            HLSL = HLSL.replace(old_s, f'Texture2DSampleLevel({tex}, {tex}Sampler, {uvn}, DustLod)')   # z: o mip mínimo
    # o contraste das 6 luzes puxado pro neutro (a média das 6): tira o sombreado miúdo de cada bolo (a "couve-flor"),
    # a nuvem lê como uma massa (a 2ª opinião de 16/10). EmitGain (sem uso na poeira) leva a fração, 0 = como gravado
    import re as _re
    m_sun = _re.search(r'float sunR = [^;]*;', HLSL)
    assert m_sun, 'o HLSL do M_FXSixWay mudou: conferir o sunR'
    HLSL = HLSL.replace(m_sun.group(0), m_sun.group(0) + '\nsunR = lerp(sunR, (Pp.r + Pp.g + Pp.b + Nn.r + Nn.g + Nn.b) / 6.0, saturate(EmitGain));')
    # o sumiço pela densidade (16/10, o estudo do BlastLab: "a dissipação multiplica a DENSIDADE, não o alfa"): o VC.a
    # (1 = a gravação; o plugin baixa no fim do rastro) multiplica a profundidade óptica, α = 1 − (1 − a)^(Density·k).
    # As partes finas e as bordas somem primeiro e o miolo denso por último, em vez de esmaecer por igual
    old_a = 'float alpha = 1.0 - pow(max(1.0 - Pp.a, 0.0), Density);'
    assert old_a in HLSL, 'o HLSL do M_FXSixWay mudou: conferir o alfa'
    # e a erosão pelo ruído, como na fumaça do WebGL e do BlastLab (volumeRender.js, BlastRaymarch.usf): na profundidade
    # óptica τ = −ln(1 − a), um ruído de Worley comum a todos os cartões (ancorado no mundo, no plano da câmera, subindo
    # 0,3 m/s pelo relógio do plugin) tira E·(0,5 − 0,5n + 0,1h): come primeiro o ralo, a borda vira fiapo e o topo vai
    # antes da base. E vem no UV3.x (o plugin: 3·m^2,5 no sumiço); a densidade ±34% pelo mesmo ruído só durante o sumiço.
    # Sem sumiço (VC.a = 1 e E = 0) a conta é a de antes, bit a bit. O min(a, 0,98): com a = 1 o τ é infinito e o miolo
    # ficava opaco até o fim (a revisão de 16/10)
    old_er = 'float erode = UV3.x;'
    assert old_er in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a erosão'
    HLSL = HLSL.replace(old_er, 'float erode = 0.0;                             // o UV3.x agora é a erosão pelo ruído (abaixo)')
    HLSL = HLSL.replace(old_a, '''float alpha = 1.0 - pow(max(1.0 - Pp.a, 0.0), Density);
float Ef = max(UV3.x, 0.0), Kd = saturate(VCA);     // o VC chega só com o RGB: o alfa (a densidade) vem à parte
if (Ef > 0.0 || Kd < 0.999)
{
    float2 nuv = (float2(dot(CamRel, CamR), dot(CamRel, CamU)) + NoiseOfs.xy) * (0.01 / 9.6) - float2(0.0, DustClock * 0.3 / 9.6);
    float nz = Texture2DSample(ErodeNoise, ErodeNoiseSampler, nuv).r * 2.0 - 1.0;
    float tau0 = -log(1.0 - min(Pp.a, 0.98));
    float tauE = max(tau0 * Density * Kd * (1.0 + 0.34 * nz * saturate(Ef)) - Ef * (0.5 - 0.5 * nz + 0.1 * uv.y), 0.0);
    alpha = 1.0 - exp(-tauE);
}''')
    # a sombra na poeira (16/10, o Felipe: "as sombras do mapa não projetam na poeira, ela sempre brilha"; o desenho e a
    # revisão em docs/POEIRA_DAS_LAGARTAS.md). ShadowMode 0 = a conta de antes, bit a bit. 1 e 2: o sol vezes a sombra
    # mais o céu ABSOLUTO na sombra, como a Unreal e o Frostbite (ancorar no aceso deixava a poeira na sombra escura como
    # fumaça). Ao sol a cor é a de antes; na sombra o céu é o de um volume (na base 6-way com pesos L², o céu uniforme dá
    # (2/3)·Pp.g + (1/3)·lados, ~1,8× o skyR, a placa deitada vê 1), menos o pedaço tapado pelo obstáculo (ShadowOcc),
    # mais o rebote da areia embaixo, acesa só pelo céu (Ag × (2/3)·Nn.g + lados/3).
    # No 2 a sombra é por pixel: o mapa de altura da sombra (alinhado ao sol: u no sentido da luz) e a grade dos tanques
    # dão até que altura acima do chão desenhado o ponto está na sombra; a corda da nuvem que o raio de vista atravessa
    # (UV2.x m, mais fina onde a poeira é rala) vira uma faixa de altura, cortada no chão, e o sol é a fração dela acima da
    # sombra (1 exato sem sombra: a areia aberta não muda). A copa faz uma faixa solta (B, A): o sol passa por baixo dela
    old_col = 'float3 col = (SunC * sunR * T * (1.0 - UV2.x) + SkyC * skyR * lerp(0.5, 1.0, T)) * dk * DustAlbedo + E * EmitGain * UV2.x * EmitScale;'
    assert old_col in HLSL, 'o HLSL do M_FXSixWay mudou: conferir a linha da cor da poeira'
    HLSL = HLSL.replace(old_col, '''float3 col;
if (ShadowMode < 0.5)
{
    ''' + old_col.replace('float3 col = ', 'col = ') + '''   // modo 0: a linha de antes, igual
}
else
{
    float shV = 1.0 - UV2.x;                   // 1: a sombra por cartão (o cenário e o tanque)
    if (ShadowMode > 1.5)
    {
        // o mapa do sol COMUM de todas as fumaças (o plugin RTSSunShadow, a mesma função do raymarch das explosões):
        // a corda da nuvem que o raio de vista atravessa vira a faixa de altura em volta do ponto
        FRTSSunShadow shS;
        shS.CamToMapCm = HCam;
        shS.Axis = HAx;
        shS.TanE = HTan;
        shS.Window = TBox;
        float3 shD = normalize(CamRel);
        float shF = abs(shD.z + HTan * dot(shD.xy, HAx.xy));           // o quanto a corda anda na altura da sombra
        shV = RTSSunVisibility(HMap, HMapSampler, TMap, TMapSampler, shS, CamRel, shF * UV2.x * sqrt(saturate(alpha)), ShadowSoft);
    }
    float skyS = lerp(0.5, 1.0, T);
    float3 litC = SunC * sunR * T + SkyC * skyR * skyS;
    float3 skyN = 2.0 / 3.0 - SkyW;                                      // o peso do lado de lá de cada eixo
    float skyP = dot(SkyW, Pp.rgb) + dot(skyN, Nn.rgb);                  // o céu visto por um volume
    float bncP = dot(skyN, Pp.rgb) + dot(SkyW, Nn.rgb);                  // a areia embaixo, acesa pelo céu
    // ShadowSky 0 = o céu do visual aprovado (o mesmo peso de quando está ao sol: na sombra só sai o sol); 1 = o céu de
    // um volume, menos o tapado, mais o rebote (a física da revisão). Medido na câmera do jogo (16/10, poeira ÷ chão em
    // volta; ao sol 1,16): 0 dá 0,71 na sombra (mancha escura), 1 dá 1,36 (brilha: o defeito que o Felipe apontou), o
    // padrão 0,5 dá 1,03 (o tom do chão na sombra, dentro da faixa física da revisão, 0,75–1,1)
    float3 shdC = SkyC * lerp((skyR * skyS).xxx, skyP * (1.0 - ShadowOcc) * skyS + Ag * bncP, ShadowSky);
    col = lerp(min(shdC, litC), litC, shV) * dk * DustAlbedo;           // a sombra nunca clareia (a 2ª revisão)
}''')
    # o UV2.y passa a ser a altura do vértice acima do chão do cartão (m): a linha do fundo da bola sai
    old_keep = 'float keep = smoothstep(bb - 0.08, bb, uv.y);'
    assert old_keep in HLSL, 'o HLSL do M_FXSixWay mudou: conferir o keep'
    HLSL = HLSL.replace(old_keep, 'float keep = 1.0;')
    # a máscara de unidade: o teto do alfa onde o ponto atrás é casco ou torre (estêncil 128, acima da motriz)
    old_ret = 'return float4(col * va, alpha * va) * Fade;'
    assert old_ret in HLSL, 'o HLSL do M_FXSixWay mudou: conferir o retorno'
    HLSL = HLSL.replace(old_ret, '''float3 dirP = normalize(CamRel);
float depthP = SceneDepthBehind / max(dot(dirP, ViewFwd), 1e-3);
float hBehind = UV2.y * 100.0 - (CamRel.z - dirP.z * depthP);       // cm acima do chão do cartão, o ponto opaco atrás
float onUnit = step(127.5, Stencil) * smoothstep(60.0, 96.0, hBehind) * UnitMask;
float aOut = alpha * va;
float aCap = lerp(aOut, min(aOut, 0.1), onUnit);
float kCap = aOut > 1e-5 ? aCap / aOut : 1.0;
return float4(col * va * kCap, aCap) * Fade;''')

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
if globals().get('DUST_GROUND', False):
    names += ['Nw', 'GroundRule', 'CamRel', 'ViewFwd', 'SceneDepthBehind', 'Stencil', 'UnitMask', 'NoiseOfs', 'CamR', 'CamU',
              'ErodeNoise', 'DustClock', 'VCA', 'ShadowMode', 'ShadowOcc', 'ShadowSoft', 'HTan', 'HCam', 'HAx', 'TBox', 'SkyW', 'ShadowSky', 'HMap', 'TMap']
c = ns['custom'](m, -300, 0, 'SixWayDust', HLSL, names, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
if globals().get('DUST_GROUND', False):
    # a função do mapa do sol comum (o plugin RTSSunShadow registra /Plugin/RTSSunShadow cedo, no PostConfigInit)
    c.set_editor_property('include_file_paths', ['/Plugin/RTSSunShadow/Private/SunShadow.ush'])
ns['scene_light_inputs'](m, c, link, -900, 1000)
for i in range(4):
    tc = E(unreal.MaterialExpressionTextureCoordinate, -900, -400 + i * 60)
    tc.set_editor_property('coordinate_index', i)
    link(tc, '', c, f'UV{i}')
vc = E(unreal.MaterialExpressionVertexColor, -900, -120)
link(vc, '', c, 'VC')
if globals().get('DUST_GROUND', False):
    nw = E(unreal.MaterialExpressionVertexNormalWS, -900, -180)
    link(nw, '', c, 'Nw')
    link(vc, 'A', c, 'VCA')                      # a densidade do cartão (o plugin: o sumiço pela profundidade óptica)
    gr = E(unreal.MaterialExpressionScalarParameter, -900, 1040)
    gr.set_editor_property('parameter_name', 'GroundRule')
    gr.set_editor_property('default_value', 1.0)
    link(gr, '', c, 'GroundRule')
    # a máscara de unidade: a posição do pixel relativa à câmera, a frente da câmera, a profundidade opaca e o estêncil
    wp = E(unreal.MaterialExpressionWorldPosition, -1300, 1100)
    wp.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    link(wp, '', c, 'CamRel')
    fw = E(unreal.MaterialExpressionConstant3Vector, -1500, 1180)
    fw.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 0))
    tf = E(unreal.MaterialExpressionTransform, -1300, 1180)
    tf.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW)
    tf.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(fw, '', tf, '')
    link(tf, '', c, 'ViewFwd')
    sd = E(unreal.MaterialExpressionSceneTexture, -1300, 1260)
    sd.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_SCENE_DEPTH)
    sdm = E(unreal.MaterialExpressionComponentMask, -1100, 1260)
    for ch, on in (('r', True), ('g', False), ('b', False), ('a', False)):
        sdm.set_editor_property(ch, on)
    link(sd, 'Color', sdm, '')
    link(sdm, '', c, 'SceneDepthBehind')
    st = E(unreal.MaterialExpressionSceneTexture, -1300, 1340)
    st.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    stm = E(unreal.MaterialExpressionComponentMask, -1100, 1340)
    for ch, on in (('r', True), ('g', False), ('b', False), ('a', False)):
        stm.set_editor_property(ch, on)
    link(st, 'Color', stm, '')
    link(stm, '', c, 'Stencil')
    um = E(unreal.MaterialExpressionScalarParameter, -900, 1100)
    um.set_editor_property('parameter_name', 'UnitMask')
    um.set_editor_property('default_value', 1.0)
    link(um, '', c, 'UnitMask')
    # a erosão pelo ruído: a posição do pixel no mundo, os eixos da câmera, a textura e o relógio do plugin
    # a posição no mundo sem o LWC: a relativa à câmera (CamRel) mais a da câmera nos eixos dela, módulo o ladrilho de
    # 9,6 m (o plugin manda a cada quadro em NoiseOfs, cm)
    no = E(unreal.MaterialExpressionVectorParameter, -1300, 1420)
    no.set_editor_property('parameter_name', 'NoiseOfs')
    no.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
    link(no, '', c, 'NoiseOfs')
    for k, (ax, nm) in enumerate(((unreal.LinearColor(1, 0, 0, 0), 'CamR'), (unreal.LinearColor(0, 1, 0, 0), 'CamU'))):
        cv = E(unreal.MaterialExpressionConstant3Vector, -1500, 1500 + k * 80)
        cv.set_editor_property('constant', ax)
        tv = E(unreal.MaterialExpressionTransform, -1300, 1500 + k * 80)
        tv.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW)
        tv.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        link(cv, '', tv, '')
        link(tv, '', c, nm)
    en = E(unreal.MaterialExpressionTextureObjectParameter, -900, 1180)
    en.set_editor_property('parameter_name', 'ErodeNoise')
    en.set_editor_property('texture', unreal.load_asset(globals().get('DUST_NOISE', f'{DEST}/T_Tiny')))
    en.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    link(en, '', c, 'ErodeNoise')
    dc = E(unreal.MaterialExpressionScalarParameter, -900, 1260)
    dc.set_editor_property('parameter_name', 'DustClock')
    dc.set_editor_property('default_value', 0.0)
    link(dc, '', c, 'DustClock')
    # a sombra (o plugin manda tudo): o modo, o céu tapado, a borda (m), a tangente do sol; a câmera no quadro do mapa
    # (cm), o eixo da luz e 1/largura, a janela dos tanques (cm e 1/largura); as duas texturas de altura
    for k, (pname, val) in enumerate((('ShadowMode', 0.0), ('ShadowOcc', 0.15), ('ShadowSoft', 0.18), ('HTan', 0.14), ('ShadowSky', 0.5))):
        sp = E(unreal.MaterialExpressionScalarParameter, -900, 1700 + k * 60)
        sp.set_editor_property('parameter_name', pname)
        sp.set_editor_property('default_value', val)
        link(sp, '', c, pname)
    for k, pname in enumerate(('HCam', 'HAx', 'TBox', 'SkyW')):
        vp = E(unreal.MaterialExpressionVectorParameter, -900, 1950 + k * 100)
        vp.set_editor_property('parameter_name', pname)
        vp.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
        link(vp, '', c, pname)
    for k, pname in enumerate(('HMap', 'TMap')):
        tp = E(unreal.MaterialExpressionTextureObjectParameter, -900, 2260 + k * 120)
        tp.set_editor_property('parameter_name', pname)
        tp.set_editor_property('texture', tiny)
        tp.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        link(tp, '', c, pname)
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
