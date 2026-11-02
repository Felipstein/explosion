"""O recorte do chão pelos prédios (01/11/2026): o chão do mapa abre o buraco onde o prédio pronto pede (a rampa do túnel do Arms Dealer).

O Felipe: "as construções vão poder manipular o chão, isso é regra de jogo e mecânica: se uma construção dessa é posta e a construção é
finalizada, ela recorta parte do chão pra ficar funcional, inclusive outras serão assim".

Como: o material do Landscape fica mascarado e lê até 8 retângulos girados de uma coleção (Cut0..7 = o centro e a meia-largura em
cm, Rot0..7 = o cosseno e o seno do giro, CutCount). A coleção é a MPC_Ground, que o chão já usa: um material só aceita 2 coleções, e
o chão já tem a MPC_Ground e a MPC_Wind (01/11 de manhã a 3ª, a MPC_GroundCut, derrubou a compilação do chão de todos os mapas: o
xadrez; desfeito). Os parâmetros entram somados aos que a MPC_Ground já tem (o GroundV), sem tirar nenhum. O jogo liga o retângulo quando a obra termina (RTSUnitActor::
UpdateGroundCut, pelo "ground_cut_m" do visual) e desliga quando o prédio some. O Landscape é uma peça só (Nanite): o recorte por pedaço
não existe em tempo de jogo, e editar o relevo em jogo também não (docs/CHAO_DEFORMAVEL.md); o custo é o raster programável do chão
(medir com o A/B: o mapa com e sem esta máscara, a mesma cena).

Rodar (na fila da placa, com o jogo fechado: ele segura os materiais):
  powershell -File Game/Tools/game.ps1 py Tools/ground_cut.py          (cria a coleção e põe a máscara nos mestres dos mapas do jogo)
  GROUND_CUT_UNDO=1 ...                                                 (tira a máscara: o A/B do custo)

O build_slice.py do WorldForge chama add_ground_cut() no mestre que ele monta: refazer um mapa não perde o recorte.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
MPC_PATH = '/Game/Materials/MPC_Ground'
MAX = 8
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
CODE = r"""
// (01/11, Tools/ground_cut.py) o chão some dentro dos retângulos dos prédios prontos (o buraco da rampa do túnel)
float keep = 1.0;
float4 A[8] = { A0, A1, A2, A3, A4, A5, A6, A7 };
float4 R[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };
int n = (int)min(N, 8.0);
[loop] for (int i = 0; i < n; ++i)
{
    float2 d = WP.xy - A[i].xy;
    float2 l = float2(d.x * R[i].x + d.y * R[i].y, -d.x * R[i].y + d.y * R[i].x);
    if (abs(l.x) < A[i].z && abs(l.y) < A[i].w) keep = 0.0;
}
return keep;
"""


def log(*a):
    unreal.log('GROUNDCUT ' + ' '.join(str(x) for x in a))


def collection():
    """A coleção do chão com os parâmetros do recorte SOMADOS aos que ela já tem (os nomes que o jogo escreve; nada sai)."""
    mpc = unreal.load_asset(MPC_PATH)
    if not mpc:
        raise RuntimeError('sem ' + MPC_PATH + ' (a coleção do chão do WorldForge)')
    vecs = list(mpc.get_editor_property('vector_parameters'))
    have = {str(v.get_editor_property('parameter_name')) for v in vecs}
    for k in range(MAX):
        for name, default in (('Cut%d' % k, unreal.LinearColor(0.0, 0.0, 0.0, 0.0)), ('Rot%d' % k, unreal.LinearColor(1.0, 0.0, 0.0, 0.0))):
            if name in have:
                continue
            v = unreal.CollectionVectorParameter()
            v.set_editor_property('parameter_name', name)
            v.set_editor_property('default_value', default)
            vecs.append(v)
    scal = list(mpc.get_editor_property('scalar_parameters'))
    if 'CutCount' not in {str(s.get_editor_property('parameter_name')) for s in scal}:
        s = unreal.CollectionScalarParameter()
        s.set_editor_property('parameter_name', 'CutCount')
        s.set_editor_property('default_value', 0.0)
        scal.append(s)
    mpc.set_editor_property('vector_parameters', vecs)
    mpc.set_editor_property('scalar_parameters', scal)
    eal.save_loaded_asset(mpc)
    return mpc


def cut_node(m):
    """O nó do recorte, se o mestre já tem (o que liga na máscara de opacidade)."""
    e = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_OPACITY_MASK)
    if isinstance(e, unreal.MaterialExpressionCustom) and e.get_editor_property('description') == 'GroundCut':
        return e
    return None


def add_ground_cut(m, mpc=None):
    """Põe a máscara do recorte num mestre de chão (o Landscape): o nó, a leitura da coleção e o modo mascarado."""
    mpc = mpc or collection()
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 4200)
    c.set_editor_property('code', CODE)
    c.set_editor_property('output_type', F1)
    c.set_editor_property('description', 'GroundCut')
    ins = []
    for n in ['WP', 'N'] + ['A%d' % k for k in range(MAX)] + ['R%d' % k for k in range(MAX)]:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    c.set_editor_property('inputs', ins)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -800, 4200)
    assert mel.connect_material_expressions(wp, '', c, 'WP'), 'WP'
    y = 4300
    for pin, pname in [('N', 'CutCount')] + [('A%d' % k, 'Cut%d' % k) for k in range(MAX)] + [('R%d' % k, 'Rot%d' % k) for k in range(MAX)]:
        e = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -800, y)
        e.set_editor_property('collection', mpc)
        e.set_editor_property('parameter_name', pname)
        assert mel.connect_material_expressions(e, '', c, pin), pin
        y += 60
    assert mel.connect_material_property(c, '', unreal.MaterialProperty.MP_OPACITY_MASK), 'OpacityMask'
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('opacity_mask_clip_value', 0.5)


def root_material(mi):
    while mi and isinstance(mi, unreal.MaterialInstance):
        mi = mi.get_editor_property('parent')
    return mi


def main():
    repo = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    maps = json.load(open(os.path.join(repo, 'Game', 'Config', 'RTS', 'Maps.json'), encoding='utf-8'))
    maps = maps.get('maps', maps) if isinstance(maps, dict) else maps
    names = [m['name'] for m in maps if isinstance(m, dict) and m.get('name')]
    undo = os.environ.get('GROUND_CUT_UNDO') == '1'
    mpc = None if undo else collection()
    done = set()
    for name in names:
        mi = None
        for folder in ('/Game/Slice/Water', '/Game/LabMap'):
            mi = unreal.load_asset('%s/MI_Ground_%s' % (folder, name))
            if mi:
                break
        m = root_material(mi)
        if not m:
            log('sem o chão de', name)
            continue
        path = m.get_path_name()
        if path in done:
            continue
        done.add(path)
        node = cut_node(m)
        if undo:
            if not node:
                continue
            mel.delete_material_expression(m, node)
            m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
            log('tirei de', path)
        elif node:
            log('já tem', path)
            continue
        else:
            add_ground_cut(m, mpc)
            log('pus em', path, '(o mapa', name + ')')
        mel.recompile_material(m)
        eal.save_loaded_asset(m)
    log('done')


if __name__ == '__main__':
    main()
