"""Materiais das unidades com Nanite: as instâncias do importador glTF trocam de pai.

Uso: blast.ps1 py Tools\\fix_unit_materials.py   (o import_units.py também chama)

O Interchange cria as instâncias filhas de /InterchangeAssets/gltf/M_Default,
que é conteúdo do plugin (não dá pra mexer) e não tem a marca de uso "Nanite":
a Unreal desenha a malha Nanite com o material padrão (cinza-branco) — foi o
que apareceu nos tanques da cena herói (07/10/2026). Aqui:
- cria /Game/Units/Materials/M_UnitPBR (cor × fator, normal, rugosidade,
  metal) com a marca de Nanite;
- pra cada instância do importador em /Game/Units, cria uma instância nossa
  (MI_<nome>) com as mesmas texturas e fatores e troca nas malhas.
"""
import unreal

ROOT = '/Game/Units'
# A reimportação do Interchange não atualiza uma instância de material que já existe
# (07/10/2026: a esteira ficou com a textura branca e o metal 0,6 da versão antiga e
# saiu como um tubo branco em volta do tanque). Quando a textura do importador é a
# padrão do motor, usa a textura com o nome certo na pasta Textures da peça; e os
# fatores das peças feitas por script ficam aqui.
OVERRIDES = {'M_TrackBelt': {'MetallicFactor': 0.0, 'RoughnessFactor': 0.85}}
PARENT = '/Game/Units/Materials/M_UnitPBR'
mel = unreal.MaterialEditingLibrary
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = lambda *a: unreal.log('UNITMAT ' + ' '.join(str(x) for x in a))


def parent_material():
    if lib.does_asset_exist(PARENT):
        return unreal.load_asset(PARENT)
    m = tools.create_asset('M_UnitPBR', PARENT.rsplit('/', 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_nanite', True)

    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)
    bc = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    bc.set_editor_property('parameter_name', 'BaseColorTexture')
    bf = node(unreal.MaterialExpressionVectorParameter, -700, 0)
    bf.set_editor_property('parameter_name', 'BaseColorFactor')
    bf.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = node(unreal.MaterialExpressionMultiply, -350, -100)
    mel.connect_material_expressions(bc, 'RGB', mul, 'A')
    mel.connect_material_expressions(bf, '', mul, 'B')
    mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    nm = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, 250)
    nm.set_editor_property('parameter_name', 'NormalTexture')
    nm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    nm.set_editor_property('texture', unreal.load_asset('/Engine/EngineMaterials/DefaultNormal'))
    mel.connect_material_property(nm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    for name, default, prop, y in (('RoughnessFactor', 0.72, unreal.MaterialProperty.MP_ROUGHNESS, 500),
                                   ('MetallicFactor', 0.0, unreal.MaterialProperty.MP_METALLIC, 600)):
        s = node(unreal.MaterialExpressionScalarParameter, -350, y)
        s.set_editor_property('parameter_name', name)
        s.set_editor_property('default_value', default)
        mel.connect_material_property(s, '', prop)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    log('parent created', PARENT)
    return m


def main():
    parent = parent_material()
    meshes = [unreal.load_asset(p) for p in lib.list_assets(ROOT, recursive=True)]
    meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
    swap = {}
    for p in lib.list_assets(ROOT, recursive=True):
        mi = unreal.load_asset(p)
        if not isinstance(mi, unreal.MaterialInstanceConstant) or mi.get_name().startswith('MI_'):
            continue
        base = mi.get_base_material()
        if base and base.get_path_name() == parent.get_path_name():
            continue
        folder = p.rsplit('/', 1)[0]
        name = 'MI_' + mi.get_name().split('.')[0]
        dest = f'{folder}/{name}'
        new = unreal.load_asset(dest) if lib.does_asset_exist(dest) else \
            tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(new, parent)
        for tn in mel.get_texture_parameter_names(mi):
            tex = mel.get_material_instance_texture_parameter_value(mi, tn)
            low = str(tn).lower()
            if tex and 'basecolor' in low:
                mel.set_material_instance_texture_parameter_value(new, 'BaseColorTexture', tex)
            elif tex and 'normal' in low:
                mel.set_material_instance_texture_parameter_value(new, 'NormalTexture', tex)
        for sn in mel.get_scalar_parameter_names(mi):
            low = str(sn).lower()
            v = mel.get_material_instance_scalar_parameter_value(mi, sn)
            if 'roughness' in low and 'texture' not in low:
                mel.set_material_instance_scalar_parameter_value(new, 'RoughnessFactor', v)
            elif 'metallic' in low and 'texture' not in low:
                mel.set_material_instance_scalar_parameter_value(new, 'MetallicFactor', v)
        textures_dir = folder.rsplit('/', 1)[0] + '/Textures'
        for param, keys in (('BaseColorTexture', ('albedo', 'basecolor', 'color')), ('NormalTexture', ('normal',))):
            cur = mel.get_material_instance_texture_parameter_value(new, param)
            if cur is None or cur.get_path_name().startswith(('/Engine', '/Interchange')):
                for tp in lib.list_assets(textures_dir, recursive=False) if lib.does_directory_exist(textures_dir) else []:
                    t = unreal.load_asset(tp)
                    if isinstance(t, unreal.Texture2D) and any(k in t.get_name().lower() for k in keys) and                             not (param == 'BaseColorTexture' and 'normal' in t.get_name().lower()):
                        mel.set_material_instance_texture_parameter_value(new, param, t)
                        log('fallback', name, param, t.get_name())
                        break
        for k, v in OVERRIDES.get(mi.get_name(), {}).items():
            mel.set_material_instance_scalar_parameter_value(new, k, v)
        lib.save_loaded_asset(new)
        swap[mi.get_path_name()] = new
        log('instance', dest, 'from', mi.get_name(), 'textures', [str(t) for t in mel.get_texture_parameter_names(mi)])
    for mesh in meshes:
        changed = False
        for i, sm in enumerate(mesh.get_editor_property('static_materials')):
            mat = sm.get_editor_property('material_interface')
            if mat and mat.get_path_name() in swap:
                mesh.set_material(i, swap[mat.get_path_name()])
                changed = True
        if changed:
            lib.save_loaded_asset(mesh)
            log('mesh', mesh.get_path_name())


main()
