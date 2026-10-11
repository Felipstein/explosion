"""Leva os modelos PROVISÓRIOS da base pra Unreal (rodar: game.ps1 models, que antes gera os GLB
com o Blender: Tools/blender_provisional.py).

- importa cada GLB de captures/game/prov em /Game/Structures/<pasta>/ com Nanite;
- monta os materiais da base: M_Structure (opaco) e M_StructureBuild (a obra: corta acima da
  altura construída, com a linha de solda acesa no corte e o lado de dentro mais escuro), e uma
  instância de cada um por nome de material (Body, Trim, ... os nomes que o Blender deu);
- troca os materiais do importador pelos nossos (os do importador não têm a marca de Nanite:
  fix_unit_materials.py do BlastLab, 07/10) e apaga os dele.

O material (estudo de 10/10, o mesmo método do reboco das casas da sessão de gráficos):
- sem UV: o concreto escaneado (asphalt_04, CC0, da fatia do WorldForge) projetado nos 3 planos
  do mundo, e o resto com a variação larga dele (o metal pintado não fica "de plástico");
- terra no pé da parede e poeira no que olha pra cima: o prédio senta no deserto (a regra de
  coerência: o mesmo pó da areia);
- faixa de perigo procedural (preto e amarelo na diagonal do mundo);
- a cor do jogador por dado de primitiva (índices 0–3), a energia (4: as luzes apagam sem
  energia ou em obra) e a altura da obra (5, metros acima do pivô).
Sem a fatia (Slice), cai na textura de ruído do motor.
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SRC = os.path.join(REPO, 'captures', 'game', 'prov')
ROOT = '/Game/Structures'
MATS = ROOT + '/Materials'

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = lambda *a: unreal.log('PROVISIONAL ' + ' '.join(str(x) for x in a))
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

# nome do material no Blender → (cor linear, rugosidade, metal, extras)
# cores pela bíblia (CONCEITO_CONSTRUCOES.md): metal claro cinza-branco, concreto claro, amarelo
# de pórtico, a cor do jogador só em faixas e lonas; luz quente, nunca neon
SLOTS = {
    'Body': ((0.50, 0.49, 0.46), 0.55, 0.0, {'Grime': 0.22}),
    'Trim': ((0.13, 0.13, 0.13), 0.45, 0.65, {'Grime': 0.15}),
    'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3, {}),
    'Yellow': ((0.70, 0.40, 0.025), 0.5, 0.0, {'Grime': 0.25}),
    'Player': ((0.5, 0.5, 0.5), 0.55, 0.0, {'PlayerMask': 1.0, 'Grime': 0.18}),
    'Concrete': ((0.34, 0.33, 0.30), 0.92, 0.0, {'ConcreteMask': 1.0}),
    'Hazard': ((0.68, 0.42, 0.02), 0.6, 0.0, {'Stripes': 1.0, 'Grime': 0.2}),
    'Glass': ((0.008, 0.010, 0.013), 0.06, 0.0, {}),
    'Light': ((0.8, 0.7, 0.55), 0.4, 0.0, {'EmissiveColor': (1.0, 0.74, 0.46), 'EmissiveNits': 40000.0}),
    'Core': ((0.9, 0.86, 0.8), 0.3, 0.0, {'EmissiveColor': (1.0, 0.92, 0.80), 'EmissiveNits': 90000.0}),
    'Beacon': ((0.8, 0.4, 0.05), 0.4, 0.0, {'EmissiveColor': (1.0, 0.42, 0.05), 'EmissiveNits': 70000.0, 'Blink': 1.4}),
    'Crate': ((0.10, 0.10, 0.045), 0.8, 0.0, {'Grime': 0.25}),
    'Rubber': ((0.012, 0.012, 0.012), 0.92, 0.0, {}),
    'Paint': ((0.62, 0.62, 0.59), 0.6, 0.0, {'Grime': 0.2}),
}

CONCRETE = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_diff_4k'
CONCRETE_N = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_nor_dx_4k'
STAIN = '/Game/Slice/Surfaces/aerial_beach_01/T_aerial_beach_01_diff_4k'
NOISE = '/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'
FLAT_N = '/Engine/EngineMaterials/DefaultNormal'

CODE = r"""
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float3 q = WP * 0.01;
float3 P = (WP - Obj) * 0.01;
float T = 2.4;
float3 c = Conc.Sample(ConcSampler, q.yz / T).rgb * w.x + Conc.Sample(ConcSampler, q.xz / T + 0.31).rgb * w.y
         + Conc.Sample(ConcSampler, q.xy / T + 0.57).rgb * w.z;
float lum = dot(c, float3(0.3, 0.59, 0.11));
float stain = Stain.Sample(StainSampler, q.yz / 9.0).g * w.x + Stain.Sample(StainSampler, q.xz / 9.0 + 0.2).g * w.y
            + Stain.Sample(StainSampler, q.xy / 9.0 + 0.4).g * w.z;
float3 base = lerp(BaseColor, PlayerColor.rgb * 0.45, PlayerMask);
// metal pintado: a variação larga do concreto, fraca (manchas de uso, não textura)
base *= lerp(1.0 - Grime, 1.0 + Grime * 0.35, smoothstep(0.55, 1.45, lum / 0.26));
// concreto: o escaneado puxado pra cor pedida
base = lerp(base, c / 0.26 * BaseColor, ConcreteMask);
// faixa de perigo: preto e amarelo a 45° no mundo (0,45 m cada)
if (Stripes > 0.5 && frac((P.x + P.y + P.z) / 0.9) < 0.5) base = float3(0.018, 0.017, 0.016);
base *= lerp(0.86, 1.04, saturate((stain - 0.15) / 0.2));
// terra no pé (sobe até ~1 m, mais onde a mancha é forte) e o pó do deserto no que olha pra cima
float foot = 1.0 - smoothstep(0.0, 0.7 + 1.4 * stain, P.z);
base = lerp(base, base * float3(0.74, 0.62, 0.48), 0.6 * foot);
float up = smoothstep(0.6, 0.95, n.z) * (1.0 - ConcreteMask);
base = lerp(base, float3(0.40, 0.31, 0.21), 0.16 * up * saturate(stain * 2.0));
Rough = lerp(lerp(Roughness, 0.95, ConcreteMask), 0.9, 0.5 * foot + 0.3 * up);
Metal = Metallic * (1.0 - 0.6 * foot);
// luzes: piscam (o giroflex), apagam sem energia ou em obra
float pulse = Blink > 0.0 ? (frac(Time * Blink) < 0.5 ? 1.0 : 0.06) : 1.0;
float3 emis = EmissiveColor.rgb * EmissiveNits * pulse * lerp(0.03, 1.0, saturate(Power));
// a obra: corta acima da altura construída; a linha de solda acesa no corte; o lado de dentro escuro
float d = Cut - P.z;
Mask = d >= 0.0 ? 1.0 : 0.0;
float weld = saturate(1.0 - d / 0.35) * (d >= 0.0 ? 1.0 : 0.0) * Building * Weld;
emis += float3(1.0, 0.52, 0.16) * 55000.0 * weld * weld;
base *= lerp(1.0, Face > 0.0 ? 1.0 : 0.45, Building);
Emis = emis;
float2 nX = ConcN.Sample(ConcNSampler, q.yz / T).rg * 2.0 - 1.0;
float2 nY = ConcN.Sample(ConcNSampler, q.xz / T + 0.31).rg * 2.0 - 1.0;
float2 nZ = ConcN.Sample(ConcNSampler, q.xy / T + 0.57).rg * 2.0 - 1.0;
Nrm = normalize(n + ConcreteMask * 0.6 * (w.x * float3(0.0, nX.x, nX.y) + w.y * float3(nY.x, 0.0, nY.y) + w.z * float3(nZ.x, nZ.y, 0.0)));
return min(base, 0.9);
"""

SCALARS = [('Roughness', 0.6), ('Metallic', 0.0), ('PlayerMask', 0.0), ('ConcreteMask', 0.0), ('Stripes', 0.0),
           ('Grime', 0.0), ('EmissiveNits', 0.0), ('Blink', 0.0), ('Weld', 1.0)]
VECTORS = [('BaseColor', (0.5, 0.5, 0.5)), ('EmissiveColor', (0.0, 0.0, 0.0))]


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def fresh(name, folder, cls, factory):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.Material):
            mel.delete_all_material_expressions(a)
        return a
    return tools.create_asset(name, folder, cls, factory)


def master(name, build):
    m = fresh(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    m.set_editor_property('used_with_nanite', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED if build else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', build)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('output_type', F3)
    custom.set_editor_property('description', 'Structure')
    inputs = ['WP', 'N', 'Obj', 'Conc', 'ConcN', 'Stain', 'Time', 'Face', 'PlayerColor', 'Power', 'Cut', 'Building'] + \
             [n for n, _ in SCALARS] + [n for n, _ in VECTORS]
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Rough', F1), ('Metal', F1), ('Emis', F3), ('Mask', F1), ('Nrm', F3)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)

    y = -1200
    def node(cls, pin):
        nonlocal y
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        if pin:
            assert mel.connect_material_expressions(e, '', custom, pin), pin
        return e
    node(unreal.MaterialExpressionWorldPosition, 'WP')
    node(unreal.MaterialExpressionVertexNormalWS, 'N')
    node(unreal.MaterialExpressionObjectPositionWS, 'Obj')
    node(unreal.MaterialExpressionTime, 'Time')
    node(unreal.MaterialExpressionTwoSidedSign, 'Face')
    for pin, path, fallback in (('Conc', CONCRETE, NOISE), ('ConcN', CONCRETE_N, FLAT_N), ('Stain', STAIN, NOISE)):
        tex = load(path) or load(fallback)
        e = node(unreal.MaterialExpressionTextureObject, None)
        e.set_editor_property('texture', tex)
        if pin == 'ConcN':
            e.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
        log(name, pin, tex.get_path_name() if tex else None)
    pc = node(unreal.MaterialExpressionVectorParameter, 'PlayerColor')
    pc.set_editor_property('parameter_name', 'PlayerColor')
    pc.set_editor_property('use_custom_primitive_data', True)
    pc.set_editor_property('primitive_data_index', 0)
    pc.set_editor_property('default_value', unreal.LinearColor(0.10, 0.45, 1.0, 1.0))
    for pin, index, default in (('Power', 4, 1.0), ('Cut', 5, 1000.0)):
        s = node(unreal.MaterialExpressionScalarParameter, pin)
        s.set_editor_property('parameter_name', pin)
        s.set_editor_property('use_custom_primitive_data', True)
        s.set_editor_property('primitive_data_index', index)
        s.set_editor_property('default_value', default)
    b = node(unreal.MaterialExpressionConstant, 'Building')
    b.set_editor_property('r', 1.0 if build else 0.0)
    for n, default in SCALARS:
        s = node(unreal.MaterialExpressionScalarParameter, n)
        s.set_editor_property('parameter_name', n)
        s.set_editor_property('default_value', default)
    for n, default in VECTORS:
        v = node(unreal.MaterialExpressionVectorParameter, n)
        v.set_editor_property('parameter_name', n)
        v.set_editor_property('default_value', unreal.LinearColor(*default, 1.0))
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_BASE_COLOR),
          mel.connect_material_property(custom, 'Rough', P.MP_ROUGHNESS),
          mel.connect_material_property(custom, 'Metal', P.MP_METALLIC),
          mel.connect_material_property(custom, 'Emis', P.MP_EMISSIVE_COLOR),
          mel.connect_material_property(custom, 'Nrm', P.MP_NORMAL)]
    if build:
        ok.append(mel.connect_material_property(custom, 'Mask', P.MP_OPACITY_MASK))
    if not all(ok):
        raise RuntimeError(f'{name}: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material', name)
    return m


GHOST_CODE = r"""
float3 n = normalize(N);
float3 v = normalize(V);
float f = pow(1.0 - saturate(abs(dot(n, v))), 3.0);
// as linhas do holograma sobem devagar (uma a cada 0,6 m), e uma faixa clara varre de baixo pra cima
float z = (WP.z - Obj.z) * 0.01;
float scan = frac(z / 0.6 - Time * 0.5) < 0.1 ? 1.0 : 0.0;
float sweep = saturate(1.0 - abs(frac(Time * 0.35) * 14.0 - z) / 0.8);
Opa = saturate(0.10 + 0.55 * f + 0.15 * scan + 0.25 * sweep);
return Color.rgb * Nits * (0.22 + 1.5 * f + 0.45 * scan + 0.8 * sweep);
"""


def ghost_material():
    """O fantasma do prédio na hora de posicionar: o modelo como holograma (verde cabe, vermelho
    não; a cor vem pelo dado de primitiva 0–3). O ZH mostra o modelo transparente com a cor;
    aqui a borda acende (fresnel) e as linhas sobem, pra ler de cima por cima do chão claro."""
    m = fresh('M_StructureGhost', MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', GHOST_CODE)
    custom.set_editor_property('output_type', F3)
    ins = []
    for n in ('WP', 'N', 'V', 'Obj', 'Time', 'Color', 'Nits'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Opa')
    co.set_editor_property('output_type', F1)
    custom.set_editor_property('additional_outputs', [co])
    y = -600
    for pin, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                     ('V', unreal.MaterialExpressionCameraVectorWS), ('Obj', unreal.MaterialExpressionObjectPositionWS),
                     ('Time', unreal.MaterialExpressionTime)):
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, y + 110)
    col.set_editor_property('parameter_name', 'GhostColor')
    col.set_editor_property('use_custom_primitive_data', True)
    col.set_editor_property('primitive_data_index', 0)
    col.set_editor_property('default_value', unreal.LinearColor(0.25, 1.0, 0.45, 1.0))
    assert mel.connect_material_expressions(col, '', custom, 'Color')
    nits = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, y + 220)
    nits.set_editor_property('parameter_name', 'Nits')
    nits.set_editor_property('default_value', 6000.0)
    assert mel.connect_material_expressions(nits, '', custom, 'Nits')
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_EMISSIVE_COLOR), mel.connect_material_property(custom, 'Opa', P.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError(f'M_StructureGhost: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material M_StructureGhost')


def instances(parent, suffix):
    out = {}
    for slot, (color, rough, metal, extra) in SLOTS.items():
        name = f'MI_{slot}{suffix}'
        mi = fresh(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', parent)
        mel.clear_all_material_instance_parameters(mi)
        mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*color, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
        mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
        for k, v in extra.items():
            if isinstance(v, tuple):
                mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
            else:
                mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[slot] = mi
    return out


def import_glb(path, dest, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths') or [])


def main():
    opaque = instances(master('M_Structure', False), '')
    instances(master('M_StructureBuild', True), '_Build')
    ghost_material()
    if os.environ.get('PROV_MATERIALS_ONLY'):
        log('done (só os materiais)')
        return
    files = sorted(glob.glob(os.path.join(SRC, '*', '*.glb')))
    if not files:
        raise RuntimeError(f'nenhum GLB em {SRC}: rode o Blender antes (game.ps1 models)')
    for f in files:
        folder = os.path.basename(os.path.dirname(f))
        stem = os.path.splitext(os.path.basename(f))[0]
        paths = import_glb(f, f'{ROOT}/{folder}', stem)
        assets = [unreal.load_asset(p) for p in paths]
        for a in assets:
            if not isinstance(a, unreal.StaticMesh):
                continue
            s = a.get_editor_property('nanite_settings')
            s.set_editor_property('enabled', True)
            a.set_editor_property('nanite_settings', s)
            # set_material, um por um (trocar a lista inteira de uma vez não ficava gravado: a malha
            # seguiu apontando pros materiais do importador, que são apagados abaixo; teste base1, 10/10)
            mats = a.get_editor_property('static_materials')
            missing = []
            for k, sm in enumerate(mats):
                slot = str(sm.get_editor_property('material_slot_name'))
                key = slot.split('.')[0].split('_')[0]
                if key in opaque:
                    a.set_material(k, opaque[key])
                else:
                    missing.append(slot)
            eal.save_loaded_asset(a)
            wrong = [str(m.get_editor_property('material_interface').get_path_name()) for m in a.get_editor_property('static_materials')
                     if not m.get_editor_property('material_interface') or not m.get_editor_property('material_interface').get_path_name().startswith(MATS)]
            if wrong:
                raise RuntimeError(f'{a.get_path_name()}: materiais fora de {MATS}: {wrong}')
            b = a.get_bounding_box()
            log('mesh', a.get_path_name(), 'slots', len(mats), 'sem material:', missing,
                'caixa (m) %.2f..%.2f %.2f..%.2f %.2f..%.2f' % (b.min.x / 100, b.max.x / 100, b.min.y / 100, b.max.y / 100,
                                                            b.min.z / 100, b.max.z / 100))
        # os materiais e texturas do importador: não servem (sem Nanite) e ninguém mais usa
        for a, p in zip(assets, paths):
            if isinstance(a, (unreal.MaterialInterface, unreal.Texture)):
                eal.delete_asset(p.split('.')[0])
    log('done')


main()
