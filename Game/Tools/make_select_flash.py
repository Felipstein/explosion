# (04/11) O clarão da seleção (RTSSelectFlash, "na hora do clique que seleciona, o hover da cor, igual o C&C ZH, porém aqui eu quero
# bem feito"). As peças da unidade marcam o bit 64 do stencil (o mesmo buffer da silhueta: 1 + dono nos bits baixos, a poeira no 128)
# durante o clarão, e este pós-processo (antes do bloom) pinta só onde a própria unidade aparece (atrás de um prédio, não).
# (05/11, v5) O método do ZH, lido no código (Drawable::flashAsSelected + W3DScene.cpp): a cor do time passa por saturateRGB(c, 0,5)
# = 0,5·c − 0,25 e é SOMADA À LUZ (ao difuso de cada luz e ao ambiente) por 4 quadros, sem subida. Ou seja: a unidade é reacesa na
# cor do time, e a metade negativa tira a cor oposta (por isso no ZH ela fica da cor do time, em vez de lavar de branco). Aqui, no
# quadro já iluminado: a luz que já está em cada ponto (a luminância: a sombra continua sombra, o desenho da textura fica) passa pro
# matiz do time, e um pouco mais de luz, pra ler como luz e não como tinta.
#   Os números: a simulação na foto sem clarão (Game/Tools/flash_sim.py) dava 0,55 cinza-azulado e 0,85 bom; no jogo (a foto
#   selecao_flash_v5, o pico parado) o 0,85 saiu azul puro, quase tinta (a curva de tom do jogo satura mais que a simulação): 0,6.
#   A luz +25% é metade do clarão branco do ZH (1 + 2 × 0,25).
# Reprovados (05/11): (v4) o matiz a 0,55 + a linha do contorno pelos 8 vizinhos + Fresnel: na foto do pico não apareceu nada (o
# tanque igual ao sem clarão; o material compilou sem erro e ninguém achou por quê); o contorno é a opção D, que ele não escolheu.
# (v3) somar a cor por toda a unidade lavava o tanque de branco-azulado, como gelo. (v1) um material por cima das peças (overlay):
# a Unreal não desenha overlay em malha Nanite, e nada acendia.
# Rodar: UnrealEditor-Cmd RTSGame.uproject -run=pythonscript -script=Tools/make_select_flash.py -nullrhi (sem a placa)
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER, NAME = '/Game/UI', 'M_SelectFlash'
PATH = f'{FOLDER}/{NAME}'

FLASH = r"""
float on = fmod(floor(round(ST.r) / 64.0), 2.0);       // o bit 64: a peça está no clarão
float seen = CD.r <= SD.r + 30.0 ? 1.0 : 0.0;          // a unidade aparece ali (a profundidade dela é a da cena)
float k = A * on * seen;
if (k <= 0.0)
{
	return Scene.rgb;
}
float lum = dot(Scene.rgb, float3(0.2126, 0.7152, 0.0722));
// a luz que já está ali, no matiz do time (a sombra continua sombra), e um pouco mais dela
float3 hue = C.rgb / max(dot(C.rgb, float3(0.2126, 0.7152, 0.0722)), 0.05) * lum;
return lerp(Scene.rgb, hue, Tint * k) * (1.0 + Lift * k);
"""


def main():
    if eal.does_asset_exist(PATH):
        eal.delete_asset(PATH)
    m = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    loc = getattr(unreal.BlendableLocation, 'BL_SCENE_COLOR_BEFORE_BLOOM', None) or getattr(unreal.BlendableLocation, 'BL_BEFORE_BLOOM')
    m.set_editor_property('blendable_location', loc)
    ids = unreal.SceneTextureId
    nodes = {}
    for k, (name, tid) in enumerate((('Scene', ids.PPI_POST_PROCESS_INPUT0), ('CD', ids.PPI_CUSTOM_DEPTH), ('SD', ids.PPI_SCENE_DEPTH),
                                     ('ST', ids.PPI_CUSTOM_STENCIL))):
        e = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, -300 + k * 110)
        e.set_editor_property('scene_texture_id', tid)
        nodes[name] = e
    c = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 360)
    c.set_editor_property('parameter_name', 'FlashColor')
    c.set_editor_property('default_value', unreal.LinearColor(0.2, 0.5, 1.0, 1.0))
    nodes['C'] = c
    # o clarão (o jogo leva de 1 a 0 em 0,28 s), quanto do matiz do time e quanto de luz a mais no pico
    for k, (name, param, value) in enumerate((('A', 'Amount', 0.0), ('Tint', 'Tint', 0.6), ('Lift', 'Lift', 0.25))):
        e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 460 + k * 90)
        e.set_editor_property('parameter_name', param)
        e.set_editor_property('default_value', value)
        nodes[name] = e
    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property('description', 'SelectFlash')
    ins = []
    for n in nodes:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    cu.set_editor_property('inputs', ins)
    cu.set_editor_property('code', FLASH)
    ok = [mel.connect_material_expressions(e, '', cu, n) for n, e in nodes.items()]
    ok.append(mel.connect_material_property(cu, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    if not all(ok):
        raise RuntimeError('M_SelectFlash: ligações falharam %s' % ok)
    errs = mel.recompile_material(m)
    if errs:
        raise RuntimeError('M_SelectFlash: erros %s' % errs)
    eal.save_asset(PATH)
    unreal.log(f'M_SelectFlash pronto: {PATH}')


main()
