# (04/11, item 22, docs/elenco/CAVAR_O_CHAO.md §6 T0/T1) o PDO pelo MakeMaterialAttributes, só no mestre do World_TestHills.
# O Python da 5.8 não expõe o MP_PixelDepthOffset (nem o MP_Displacement): o mestre passa pra "use material attributes" com um
# MakeMaterialAttributes, cada entrada religada nele (o deslocamento Nanite achado pelo nó Custom do DISP_CODE, que só alimenta o
# Displacement), e o PDO zerado vindo de um nó Custom no pino PixelDepthOffset dele.
# Modos (T0_MODE):
#   probe: só lê (o estado, as entradas, os nós soltos, a coleção) e grava o retrato em t0pdo/probe.json; faz a cópia de segurança
#          do mestre (duplicate_asset) se ainda não houver. NÃO mexe no mestre.
#   build: a partir do mestre original: MA + PDO + o recorte ZERADO -> salva -> copia o .uasset pra t0pdo/zero.uasset;
#          depois troca só o código do recorte pelo de verdade (ground_cut.CODE) -> salva -> t0pdo/cut.uasset.
#          (o .ps1 devolve o original por cópia de arquivo no fim da vez dele)
#   check: confere o mestre contra o probe.json (o desfazer): opaco, sem MA, as mesmas entradas.
import hashlib
import json
import os
import shutil
import sys

import unreal

sys.path.insert(0, r'C:\Users\luisf\explosion\Game\Tools')
import ground_cut as gc  # noqa: E402

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
S = r'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\12f1e755-ddf7-4e41-a834-371a3dfa6f7e\scratchpad\t0pdo'
MAP = 'World_TestHills'
MODE = os.environ.get('T0_MODE', 'probe')
BACKUP = '/Game/T0Backup/M_SliceGround_hills_t0bak'
FILE = r'C:\Users\luisf\explosion\Game\Content\Slice\Materials\M_SliceGround_world_desert_sa452615_hills.uasset'
log = lambda *a: unreal.log('T0P ' + ' '.join(str(x) for x in a))

# as entradas que o Python expõe e o pino do MakeMaterialAttributes de cada uma
PROPS = [('MP_BASE_COLOR', 'BaseColor'), ('MP_METALLIC', 'Metallic'), ('MP_SPECULAR', 'Specular'), ('MP_ROUGHNESS', 'Roughness'),
         ('MP_ANISOTROPY', 'Anisotropy'), ('MP_EMISSIVE_COLOR', 'EmissiveColor'), ('MP_OPACITY', 'Opacity'),
         ('MP_OPACITY_MASK', 'OpacityMask'), ('MP_NORMAL', 'Normal'), ('MP_TANGENT', 'Tangent'),
         ('MP_WORLD_POSITION_OFFSET', 'WorldPositionOffset'), ('MP_SUBSURFACE_COLOR', 'SubsurfaceColor'),
         ('MP_AMBIENT_OCCLUSION', 'AmbientOcclusion'), ('MP_REFRACTION', 'Refraction'),
         ('MP_FRONT_MATERIAL', None), ('MP_MATERIAL_ATTRIBUTES', None)]


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16]


def ground_mi(name):
    for folder in ('/Game/Slice/Water', '/Game/LabMap'):
        mi = unreal.load_asset('%s/MI_Ground_%s' % (folder, name))
        if mi:
            return mi
    return None


# o mestre tem de ser só do World_TestHills (o mesmo teste do t0_ground_cut.py)
maps = json.load(open(r'C:\Users\luisf\explosion\Game\Config\RTS\Maps.json', encoding='utf-8'))
maps = maps.get('maps', maps) if isinstance(maps, dict) else maps
roots = {}
for mp in maps:
    n = mp.get('name') if isinstance(mp, dict) else None
    r = gc.root_material(ground_mi(n)) if n else None
    if r:
        roots.setdefault(r.get_path_name(), []).append(n)
mi = ground_mi(MAP)
m = gc.root_material(mi)
if not m:
    raise RuntimeError('sem o chão de ' + MAP)
users = roots.get(m.get_path_name(), [])
log('mestre', m.get_path_name(), 'usado por', users, 'arquivo', sha(FILE))
if users != [MAP]:
    raise RuntimeError('o mestre de %s é dividido com %s: não mexo' % (MAP, users))


def state(m):
    st = {}
    for k in ('blend_mode', 'use_material_attributes', 'enable_tessellation', 'opacity_mask_clip_value', 'shading_model', 'two_sided',
              'enable_displacement_fade'):
        try:
            st[k] = str(m.get_editor_property(k))
        except Exception as e:  # noqa: BLE001
            st[k] = 'ERRO ' + str(e)
    try:
        ds = m.get_editor_property('displacement_scaling')
        st['displacement_scaling'] = '%s/%s' % (ds.get_editor_property('magnitude'), ds.get_editor_property('center'))
    except Exception as e:  # noqa: BLE001
        st['displacement_scaling'] = 'ERRO ' + str(e)
    return st


def inputs(m):
    out = {}
    for name, _pin in PROPS:
        p = getattr(unreal.MaterialProperty, name)
        n = mel.get_material_property_input_node(m, p)
        if n:
            out[name] = (n, mel.get_material_property_input_node_output_name(m, p))
    return out


def free_nodes(m, ins):
    """Os nós que nada consome (nem outro nó, nem uma entrada exposta): ou soltos, ou ligados numa entrada escondida."""
    exprs = list(mel.get_material_expressions(m))
    used = set()
    for e in exprs:
        for i in mel.get_inputs_for_material_expression(m, e):
            if i:
                used.add(i.get_name())
    for n, _o in ins.values():
        used.add(n.get_name())
    return exprs, [e for e in exprs if e.get_name() not in used]


def custom_inputs(e):
    try:
        return [str(ci.get_editor_property('input_name')) for ci in e.get_editor_property('inputs')]
    except Exception:  # noqa: BLE001
        return []


def describe(e):
    d = e.get_class().get_name()
    if isinstance(e, unreal.MaterialExpressionCustom):
        d += ' "%s" in=%s' % (e.get_editor_property('description'), custom_inputs(e))
    elif isinstance(e, unreal.MaterialExpressionCollectionParameter):
        d += ' ' + str(e.get_editor_property('parameter_name'))
    return d


st = state(m)
ins = inputs(m)
exprs, free = free_nodes(m, ins)
log('estado', st)
for k, (n, o) in ins.items():
    log('entrada', k, '<-', n.get_name(), describe(n), 'saída "%s"' % o)
log('nós', len(exprs), 'soltos/escondidos', len(free))
for e in free:
    log('  solto', e.get_name(), describe(e))
disp = [e for e in free if isinstance(e, unreal.MaterialExpressionCustom) and 'DispOn' in custom_inputs(e)]
log('o nó do deslocamento (Custom com DispOn, sem consumidor):', [e.get_name() for e in disp])
mpc = unreal.load_asset('/Game/Materials/MPC_Ground')
have = {str(v.get_editor_property('parameter_name')) for v in mpc.get_editor_property('vector_parameters')} | \
       {str(s.get_editor_property('parameter_name')) for s in mpc.get_editor_property('scalar_parameters')}
need = {'CutCount'} | {'Cut%d' % k for k in range(gc.MAX)} | {'Rot%d' % k for k in range(gc.MAX)}
log('MPC_Ground tem os parâmetros do recorte:', need <= have, sorted(need - have))

if MODE == 'probe':
    os.makedirs(S, exist_ok=True)
    json.dump({'state': st, 'inputs': {k: [n.get_name(), o] for k, (n, o) in ins.items()},
               'disp': [e.get_name() for e in disp], 'nodes': len(exprs), 'file': sha(FILE)},
              open(os.path.join(S, 'probe.json'), 'w'), indent=1)
    if not eal.does_asset_exist(BACKUP):
        dup = eal.duplicate_asset(m.get_path_name(), BACKUP)
        log('cópia de segurança', BACKUP, 'ok' if dup else 'FALHOU', eal.save_asset(BACKUP) if dup else '')
    else:
        log('cópia de segurança já existe', BACKUP)
    log('done')

elif MODE == 'check':
    ref = json.load(open(os.path.join(S, 'probe.json')))
    now = {k: [n.get_name(), o] for k, (n, o) in ins.items()}
    ok = (now == ref['inputs'] and st == ref['state'] and [e.get_name() for e in disp] == ref['disp'] and len(exprs) == ref['nodes'])
    log('CONFERE com o original:', 'SIM' if ok else 'NAO', '| arquivo', sha(FILE), 'original', ref['file'])
    if not ok:
        log('  agora', now, st, len(exprs))
        log('  antes', ref)
    log('done')

elif MODE in ('build', 'build_nopdo'):
    nopdo = MODE == 'build_nopdo'   # o controle: a máscara SEM o PDO (o Nanite tira a máscara? o chão fica?)
    ref = json.load(open(os.path.join(S, 'probe.json')))
    if sha(FILE) != ref['file']:
        raise RuntimeError('o mestre não é o original do probe: não mexo')
    if st['use_material_attributes'] != 'False' or 'MP_OPACITY_MASK' in ins or 'MP_FRONT_MATERIAL' in ins or 'MP_MATERIAL_ATTRIBUTES' in ins:
        raise RuntimeError('o mestre não está no estado esperado (MA, máscara ou Substrate já ligados)')
    tess = st['enable_tessellation'] == 'True'
    if tess and len(disp) != 1:
        raise RuntimeError('com a tesselação ligada e %d nós de deslocamento achados: não religo às cegas' % len(disp))
    make = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 2200, 0)
    pins = list(mel.get_material_expression_input_names(make))
    log('pinos do MakeMaterialAttributes', pins)
    want = {}
    for k, (n, o) in ins.items():
        pin = dict(PROPS)[k]
        if not mel.connect_material_expressions(n, o, make, pin):
            raise RuntimeError('não liguei %s (%s.%s) no pino %s' % (k, n.get_name(), o, pin))
        want[pin] = n.get_name()
    if tess:
        if not mel.connect_material_expressions(disp[0], '', make, 'Displacement'):
            raise RuntimeError('não liguei o deslocamento')
        want['Displacement'] = disp[0].get_name()
    if not nopdo:
        pdo = mel.create_material_expression(m, unreal.MaterialExpressionCustom, 1800, 4000)
        pdo.set_editor_property('code', '// (T0, item 22) o PDO zerado: segura a máscara no Landscape Nanite (LandscapeRender.cpp)\nreturn 0.0;')
        pdo.set_editor_property('output_type', gc.F1)
        pdo.set_editor_property('description', 'GroundCutPDO')
        if not mel.connect_material_expressions(pdo, '', make, 'PixelDepthOffset'):
            raise RuntimeError('não liguei o PDO')
        want['PixelDepthOffset'] = pdo.get_name()
    # o nó do recorte, como o ground_cut.add_ground_cut, mas ligado no pino OpacityMask do MakeMaterialAttributes
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, 1800, 4200)
    c.set_editor_property('code', '// T0: o chão inteiro some (o teste do modo mascarado)\nreturn 0.0;')
    c.set_editor_property('output_type', gc.F1)
    c.set_editor_property('description', 'GroundCut')
    cins = []
    for nm in ['WP', 'N'] + ['A%d' % k for k in range(gc.MAX)] + ['R%d' % k for k in range(gc.MAX)]:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', nm)
        cins.append(ci)
    c.set_editor_property('inputs', cins)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, 1400, 4200)
    assert mel.connect_material_expressions(wp, '', c, 'WP'), 'WP'
    y = 4300
    for pin, pname in [('N', 'CutCount')] + [('A%d' % k, 'Cut%d' % k) for k in range(gc.MAX)] + [('R%d' % k, 'Rot%d' % k) for k in range(gc.MAX)]:
        e = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, 1400, y)
        e.set_editor_property('collection', mpc)
        e.set_editor_property('parameter_name', pname)
        assert mel.connect_material_expressions(e, '', c, pin), pin
        y += 60
    if not mel.connect_material_expressions(c, '', make, 'OpacityMask'):
        raise RuntimeError('não liguei o recorte')
    want['OpacityMask'] = c.get_name()
    # confere pino por pino o que ficou ligado no MakeMaterialAttributes
    got = {}
    for pin, e in zip(pins, mel.get_inputs_for_material_expression(m, make)):
        if e:
            got[pin] = e.get_name()
    log('ligado no MakeMaterialAttributes', got)
    if got != want:
        raise RuntimeError('o MakeMaterialAttributes não ficou como devia: %s contra %s' % (got, want))
    if not mel.connect_material_property(make, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES):
        raise RuntimeError('não liguei o MakeMaterialAttributes no mestre')
    m.set_editor_property('use_material_attributes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('opacity_mask_clip_value', 0.5)
    log('compilação (zero):', mel.recompile_material(m))
    log('estado depois', state(m))
    assert eal.save_loaded_asset(m, False), 'save'
    shutil.copyfile(FILE, os.path.join(S, 'zeronopdo.uasset' if nopdo else 'zero.uasset'))
    log('zero salvo', 'SEM o PDO' if nopdo else 'com o PDO', sha(FILE))
    if not nopdo:
        c.set_editor_property('code', gc.CODE)
        log('compilação (recorte):', mel.recompile_material(m))
        assert eal.save_loaded_asset(m, False), 'save 2'
        shutil.copyfile(FILE, os.path.join(S, 'cut.uasset'))
        log('recorte salvo', sha(FILE))
    log('done')
