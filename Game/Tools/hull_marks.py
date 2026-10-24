"""A marca do tiro no casco (sessão Fluxo, 16/10): a função de material MF_HullMarks.

Rodar na Unreal do jogo (sem janela, segurando a vez na fila da placa):
  UnrealEditor-Cmd Game/RTSGame.uproject -run=pythonscript -script=Game/Tools/hull_marks.py -nullrhi

O estudo com fontes: docs/ESTUDO_MARCA_QUENTE.md. A divisão:
- o BlastFx (UBlastFxDirector::MarkHull) grava ONDE e QUANDO cada peça do tanque levou o tiro: os dados de primitiva
  12–35 da peça, 6 vagas de 4 números (x, y, z no espaço da malha em cm; a hora do acerto em s + 100 000 × o tipo:
  0 canhão, 1 míssil);
- esta função diz COMO fica: o material das unidades (o M_UnitDust do RTSLook) passa a cor, o metálico e a rugosidade
  por ela e soma o emissivo. Sem acerto (as vagas zeradas), a função não muda nada.

Como fica (pra ler de cima a 67 m, 1 px ≈ 3,3 cm; em tamanho real a cicatriz teria 2–3 px):
- a fuligem da tinta queimada em volta: o canhão Ø 40 cm escurecendo até 0,25; o míssil Ø 90 cm até 0,2, manchado;
  a borda recortada (o respingo não é redondo); fica a vida do tanque e passa pra carcaça;
- o metal nu no meio (só o canhão): Ø 12 cm, aço cinza metálico;
- o brilho: corpo negro numa mancha de Ø 10 cm, T = 850 + 950·e^(−t/1,0) K e I = 8·e^(−t/0,7) × a tinta ao sol (0,5 do
  branco da tela, pelo EyeAdaptationInverse: não depende da exposição do mapa), cortando em 3,5 s. De dia o real some em
  menos de 1 s (1300 K = 4% da tinta ao sol): os segundos de laranja são leitura de jogo, não física.
"""
import unreal

FOLDER = '/Game/Materials'
NAME = 'MF_HullMarks'
BASE = 12           # o 1º dado de primitiva das marcas (o BlastFx grava 12–35; 0–9 são do jogo)
SLOTS = 6
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

CODE = r"""
float3 bc = BC; float mt = Mt; float rg = Rg;
float darken = 1.0; float metal = 0.0; float3 emi = float3(0.0, 0.0, 0.0);
float4 H[6] = { H0, H1, H2, H3, H4, H5 };
[unroll] for (int i = 0; i < 6; ++i)
{
    float4 h = H[i];
    if (h.w <= 0.0) continue;
    float kind = floor(h.w / 100000.0);
    float t = Tm - (h.w - kind * 100000.0);
    if (t < 0.0) continue;                       // de outra partida (o relógio do mundo voltou)
    float3 v = P - h.xyz;
    float d = length(v);
    float3 dv = v / max(d, 0.001);
    // a borda recortada: o raio varia com a direção (duas ondas por marca, a fase pela hora do acerto)
    float s = frac(h.w * 0.6180339);
    float wob = 1.0 + 0.18 * sin(dot(dv, float3(5.1, 3.7, 4.3)) + s * 40.0) + 0.10 * sin(dot(dv, float3(-9.3, 11.2, 7.1)) + s * 90.0);
    bool missile = kind > 0.5;
    float R = (missile ? 45.0 : 20.0) * wob;
    float r = d / R;
    // o míssil manchado: a fuligem em placas (o estilhaço picando a tinta)
    float blot = missile ? 0.75 + 0.25 * sin(dot(v, float3(0.31, 0.27, 0.22)) + s * 17.0) : 1.0;
    darken = min(darken, lerp(missile ? 0.2 : 0.25, 1.0, saturate(smoothstep(0.25, 1.0, r) / blot)));
    if (!missile)
    {
        metal = max(metal, 1.0 - smoothstep(0.7, 1.0, d / 6.0));
        if (t < 3.5)
        {
            float T = 850.0 + 950.0 * exp(-t / 1.0);
            float I = 8.0 * exp(-t / 0.7) * (1.0 - smoothstep(2.5, 3.5, t));
            // a cor do corpo negro (Helland, em linear): o vermelho cheio, o verde subindo com a temperatura, sem azul < 1900 K
            float g = pow(saturate((99.47 * log(T / 100.0) - 161.12) / 255.0), 2.2);
            float m = 1.0 - smoothstep(0.4, 1.0, d / 5.0);
            emi += float3(1.0, g, 0.0) * (I * 0.5 * m);
        }
    }
}
Mout = lerp(mt, 1.0, metal);
Rout = lerp(rg, 0.35, metal);
Emi = emi * EInv;
return lerp(bc * darken, float3(0.45, 0.45, 0.46), metal);
"""


def main():
    path = f'{FOLDER}/{NAME}'
    mf = unreal.load_asset(path)
    if mf is not None:
        mel.delete_all_material_expressions_in_function(mf)
    else:
        mf = tools.create_asset(NAME, FOLDER, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    mf.set_editor_property('description', 'A marca do tiro no casco (BlastFx): lê os dados de primitiva 12-35 e muda cor, metálico, rugosidade e emissivo.')
    mf.set_editor_property('expose_to_library', True)
    E = lambda cls, x, y: mel.create_material_expression_in_function(mf, cls, x, y)

    def fin(name, typ, y, prio):
        e = E(unreal.MaterialExpressionFunctionInput, -1400, y)
        e.set_editor_property('input_name', name)
        e.set_editor_property('input_type', typ)
        e.set_editor_property('sort_priority', prio)
        return e

    bc_in = fin('BaseColor', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3, -400, 0)
    m_in = fin('Metallic', unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -250, 1)
    r_in = fin('Roughness', unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, -100, 2)
    c = E(unreal.MaterialExpressionCustom, -600, 0)
    c.set_editor_property('code', CODE)
    c.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property('description', 'BlastFx hull marks')
    names = ['BC', 'Mt', 'Rg', 'P', 'Tm', 'EInv'] + [f'H{i}' for i in range(SLOTS)]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Mout', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('Rout', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                 ('Emi', unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    c.set_editor_property('additional_outputs', outs)
    mel.connect_material_expressions(bc_in, '', c, 'BC')
    mel.connect_material_expressions(m_in, '', c, 'Mt')
    mel.connect_material_expressions(r_in, '', c, 'Rg')
    # a posição no espaço da malha (cm): a mesma em que o BlastFx grava o acerto
    lp = E(unreal.MaterialExpressionLocalPosition, -1100, 100)
    mel.connect_material_expressions(lp, '', c, 'P')
    tm = E(unreal.MaterialExpressionTime, -1100, 200)
    mel.connect_material_expressions(tm, '', c, 'Tm')
    ea = E(unreal.MaterialExpressionEyeAdaptationInverse, -1100, 300)
    mel.connect_material_expressions(ea, '', c, 'EInv')
    y = 420
    for i in range(SLOTS):
        v = E(unreal.MaterialExpressionVectorParameter, -1100, y)
        v.set_editor_property('parameter_name', f'HullMark{i}')
        v.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))
        v.set_editor_property('use_custom_primitive_data', True)
        v.set_editor_property('primitive_data_index', BASE + 4 * i)
        ok = mel.connect_material_expressions(v, 'RGBA', c, f'H{i}')
        if not ok:
            ap = E(unreal.MaterialExpressionAppendVector, -900, y)
            mel.connect_material_expressions(v, '', ap, 'A')
            mel.connect_material_expressions(v, 'A', ap, 'B')
            ok = mel.connect_material_expressions(ap, '', c, f'H{i}')
        unreal.log(f'HULLMARKS H{i} (dado {BASE + 4 * i}) ligado: {ok}')
        y += 120

    def fout(name, src, pin, yy, prio):
        o = E(unreal.MaterialExpressionFunctionOutput, -200, yy)
        o.set_editor_property('output_name', name)
        o.set_editor_property('sort_priority', prio)
        mel.connect_material_expressions(src, pin, o, '')
    fout('BaseColor', c, '', -200, 0)
    fout('Metallic', c, 'Mout', -50, 1)
    fout('Roughness', c, 'Rout', 100, 2)
    fout('Emissive', c, 'Emi', 250, 3)
    mel.update_material_function(mf)
    eal.save_loaded_asset(mf)
    unreal.log(f'HULLMARKS {path} pronta')


main()
