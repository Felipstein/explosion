"""A função de material do chão com memória (o plugin RTSLook): MF_LookGroundMemory.

Rodar na Unreal do jogo (sem janela; o look.ps1 mf chama):
  UnrealEditor-Cmd Game/RTSGame.uproject -run=pythonscript -script=Game/Tools/look_mf.py -nullrhi

A divisão (12/10, combinada com a sessão do jogo): o jogo escreve ONDE e QUANDO o chão foi mexido (os cadernos de relevo
em volta da câmera: R altura com sinal, G idade, B sol, A revolvido; e o caderno da esteira com a garra fina); esta função
diz COMO o chão fica. O material do chão (o do WorldForge, build_slice.py ground_material) chama a função com a cor, a
normal (espaço tangente) e a rugosidade dele, e usa as saídas. Os parâmetros (os nomes que o jogo põe por
SetLandscapeMaterial*ParameterValue): GroundRelief, GroundReliefMid, GroundTrack (texturas), DetailBounds, MidBounds
(cm: mín X, mín Y, largura, altura) e ReliefRangeCm, ReliefTexel. Sem os cadernos (ReliefRangeCm = 0), a função não muda nada.

Como fica (o estudo e a 2ª opinião de 12/10; a marca na areia lê pela LUZ, não pela profundidade):
- a normal da altura pela diferença finita (o sulco e a bordinha pegando o sol rasante), mais a garra fina da esteira;
- a sombra do sol já calculada no carimbo (B), macia;
- a areia revolvida fresca um pouco mais escura e quente (a umidade de baixo), secando com a idade; a compactada mais lisa;
- a saída RippleMask: onde a ondulação da areia some (a esteira fresca apagou; volta com a idade);
- detalhe → meio → nada pelo tamanho do pixel no chão.
"""
import unreal

FOLDER = '/RTSLook/Materials'
NAME = 'MF_LookGroundMemory'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

CODE = r"""
float3 bc = BC; float3 nT = N; float rough = Rg;
Mask = 0.0;
if (Range <= 0.0) { Nout = nT; Rout = rough; return bc; }
float2 p = WP.xy;
float2 uvD = (p - DB.xy) / max(DB.zw, 1.0);
float2 uvM = (p - MB.xy) / max(MB.zw, 1.0);
float inD = step(0.0, uvD.x) * step(uvD.x, 1.0) * step(0.0, uvD.y) * step(uvD.y, 1.0);
float inM = step(0.0, uvM.x) * step(uvM.x, 1.0) * step(0.0, uvM.y) * step(uvM.y, 1.0);
float fw = length(fwidth(p));                                   // cm por pixel no chão
float wD = inD * (1.0 - smoothstep(6.0, 12.0, fw));              // o detalhe enquanto o pixel é menor que ~9 cm
float wM = inM * (1.0 - smoothstep(20.0, 40.0, fw)) * (1.0 - wD);
float w = wD + wM;
if (w <= 0.001) { Nout = nT; Rout = rough; return bc; }
float tD = Texel, tM = Texel;
float cmD = DB.z * Texel, cmM = MB.z * Texel;                    // cm por texel
float4 Rd = Relief.Sample(ReliefSampler, uvD);
float4 Rm = ReliefMid.Sample(ReliefMidSampler, uvM);
float4 Rr = (Rd * wD + Rm * wM) / w;
// a normal da altura (diferença finita no caderno de cada nível)
float hxD = (Relief.Sample(ReliefSampler, uvD + float2(tD, 0)).r - Relief.Sample(ReliefSampler, uvD - float2(tD, 0)).r) * 2.0 * Range / (2.0 * cmD);
float hyD = (Relief.Sample(ReliefSampler, uvD + float2(0, tD)).r - Relief.Sample(ReliefSampler, uvD - float2(0, tD)).r) * 2.0 * Range / (2.0 * cmD);
float hxM = (ReliefMid.Sample(ReliefMidSampler, uvM + float2(tM, 0)).r - ReliefMid.Sample(ReliefMidSampler, uvM - float2(tM, 0)).r) * 2.0 * Range / (2.0 * cmM);
float hyM = (ReliefMid.Sample(ReliefMidSampler, uvM + float2(0, tM)).r - ReliefMid.Sample(ReliefMidSampler, uvM - float2(0, tM)).r) * 2.0 * Range / (2.0 * cmM);
float2 g = (float2(hxD, hyD) * wD + float2(hxM, hyM) * wM) / w;
// a garra fina da esteira (o caderno de 4,7 cm, R = a fita com as garras): ~0,5 cm de relevo, só no detalhe
float tT = Texel * 0.5;
float gx = (Track.Sample(TrackSampler, uvD + float2(tT, 0)).r - Track.Sample(TrackSampler, uvD - float2(tT, 0)).r);
float gy = (Track.Sample(TrackSampler, uvD + float2(0, tT)).r - Track.Sample(TrackSampler, uvD - float2(0, tT)).r);
g += float2(gx, gy) * (-0.5) / (2.0 * cmD * 0.5) * wD;
// a cratera e o queimado (13/10, o alvo street_battle_usa_china_gemini): GroundTrack B = a altura da cratera com sinal
// (CraterRangeCm), A = o ejetado (a areia clara espirrada e os torrões), G = a fuligem com a borda definida
float hc = 0.0, ej = 0.0, soot = 0.0;
if (CRange > 0.0) {
    float4 Tk = Track.Sample(TrackSampler, uvD);
    hc = (Tk.b - 0.5) * 2.0 * CRange * wD;
    float cx = (Track.Sample(TrackSampler, uvD + float2(tT, 0)).b - Track.Sample(TrackSampler, uvD - float2(tT, 0)).b) * 2.0 * CRange / (2.0 * cmD * 0.5);
    float cy = (Track.Sample(TrackSampler, uvD + float2(0, tT)).b - Track.Sample(TrackSampler, uvD - float2(0, tT)).b) * 2.0 * CRange / (2.0 * cmD * 0.5);
    g += float2(cx, cy) * wD;
    ej = Tk.a * wD;
    soot = Tk.g * wD;
}
float k = w;                                                     // some no longe
nT = normalize(nT + float3(-g.x, -g.y, 0.0) * k);
// a luz e a areia mexida
float age = Rr.g, stir = Rr.a, vis = Rr.b;
bc *= lerp(1.0, vis, 0.85 * k);
if (CRange > 0.0) {
    bc *= lerp(float3(1, 1, 1), float3(0.8, 0.74, 0.68), saturate(-hc / 15.0));   // o fundo da cratera: areia revirada, úmida
    bc *= lerp(1.0, 1.12, saturate(hc / 6.0));                                      // a borda levantada: areia clara
    bc = lerp(bc, bc * float3(1.16, 1.12, 1.05), ej * 0.7);                         // o ejetado espirrado por cima (claro)
    float lum = dot(bc, float3(0.3, 0.59, 0.11));
    bc = lerp(bc, lerp(bc, lum.xxx, 0.6) * 0.45, soot * 0.75);                     // a fuligem: pouca, sem preto chapado
    rough = lerp(rough, 0.95, ej * 0.5);
}
float fresh = stir * (1.0 - age) * k;
bc *= lerp(float3(1.0, 1.0, 1.0), float3(0.84, 0.8, 0.76), fresh * 0.8);   // revolvida fresca: mais escura e quente; seca com a idade
float h = (Rr.r - 0.5) * 2.0 * Range;
rough = lerp(rough, rough * 0.85, saturate(-h / 2.0) * k);                   // o fundo do sulco compactado, mais liso
Mask = saturate(fresh + saturate(-hc / 3.0) + ej);                            // a ondulação some na esteira fresca e na cratera
Nout = nT; Rout = rough;
return bc;
"""


def main():
    path = f'{FOLDER}/{NAME}'
    # (o registro do commandlet nem sempre conhece o conteúdo do plugin: reaproveita a função e limpa os nós)
    mf = unreal.load_asset(path)
    if mf is not None:
        mel.delete_all_material_expressions_in_function(mf)
    else:
        mf = tools.create_asset(NAME, FOLDER, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    mf.set_editor_property('description', 'O chão com memória (RTSLook): lê os cadernos de relevo do jogo e muda cor, normal e rugosidade.')
    mf.set_editor_property('expose_to_library', True)
    E = lambda cls, x, y: mel.create_material_expression_in_function(mf, cls, x, y)

    def fin(name, typ, y, prio):
        e = E(unreal.MaterialExpressionFunctionInput, -1400, y)
        e.set_editor_property('input_name', name)
        e.set_editor_property('input_type', typ)
        e.set_editor_property('sort_priority', prio)
        return e

    bc_in = fin('BaseColor', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3, -400, 0)
    n_in = fin('Normal', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3, -250, 1)
    r_in = fin('Roughness', unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -100, 2)
    c = E(unreal.MaterialExpressionCustom, -600, 0)
    c.set_editor_property('code', CODE)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('description', 'RTSLook ground memory')
    names = ['WP', 'BC', 'N', 'Rg', 'Relief', 'ReliefMid', 'Track', 'DB', 'MB', 'Range', 'Texel', 'CRange']
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Nout', unreal.CustomMaterialOutputType.CMOT_FLOAT3), ('Rout', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                 ('Mask', unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    c.set_editor_property('additional_outputs', outs)
    wp = E(unreal.MaterialExpressionWorldPosition, -1100, 100)
    mel.connect_material_expressions(wp, '', c, 'WP')
    mel.connect_material_expressions(bc_in, '', c, 'BC')
    mel.connect_material_expressions(n_in, '', c, 'N')
    mel.connect_material_expressions(r_in, '', c, 'Rg')
    black = unreal.load_asset('/Engine/EngineResources/Black')
    y = 200
    for pname, pin in (('GroundRelief', 'Relief'), ('GroundReliefMid', 'ReliefMid'), ('GroundTrack', 'Track')):
        t = E(unreal.MaterialExpressionTextureObjectParameter, -1100, y)
        t.set_editor_property('parameter_name', pname)
        if black:
            t.set_editor_property('texture', black)
        t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        t.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)   # (o Landscape tem limite de 16 amostradores)
        mel.connect_material_expressions(t, '', c, pin)
        y += 120
    for pname, pin in (('DetailBounds', 'DB'), ('MidBounds', 'MB')):
        v = E(unreal.MaterialExpressionVectorParameter, -1100, y)
        v.set_editor_property('parameter_name', pname)
        v.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
        mel.connect_material_expressions(v, '', c, pin)
        y += 120
    for pname, pin, dv in (('ReliefRangeCm', 'Range', 0.0), ('ReliefTexel', 'Texel', 1.0 / 2048.0), ('CraterRangeCm', 'CRange', 0.0)):
        sp = E(unreal.MaterialExpressionScalarParameter, -1100, y)
        sp.set_editor_property('parameter_name', pname)
        sp.set_editor_property('default_value', dv)
        mel.connect_material_expressions(sp, '', c, pin)
        y += 100

    def fout(name, src, pin, yy, prio):
        o = E(unreal.MaterialExpressionFunctionOutput, -200, yy)
        o.set_editor_property('output_name', name)
        o.set_editor_property('sort_priority', prio)
        mel.connect_material_expressions(src, pin, o, '')
    fout('BaseColor', c, '', -200, 0)
    fout('Normal', c, 'Nout', -50, 1)
    fout('Roughness', c, 'Rout', 100, 2)
    fout('RippleMask', c, 'Mask', 250, 3)
    mel.update_material_function(mf)
    eal.save_loaded_asset(mf)
    unreal.log(f'LOOKMF {path} pronta')


main()
