"""Monta as fatias de qualidade (~250 m) a partir do gerador do WorldForge.

Rodar sem abrir o editor:
  powershell -File WorldForge/Tools/forge.ps1 slice [desert|snow|forest]   (padrão: desert)
Antes: python WorldForge/Tools/fetch_assets.py (assets CC0 baixados) e
python WorldForge/Tools/make_slice_textures.py (texturas geradas por código).

Um recorte do mapa gerado vira Landscape da Unreal com camadas pintadas pelas
máscaras do gerador, por PAPEL (o nome muda com o bioma):
  0 base · 1 manchas · 2 trilha · 3 rocha · 4 molhado/seco · 5 asfalto (com estrada)
Objetos espalhados pelas mesmas máscaras e posições; marcas de pneu ao longo
das trilhas do gerador; estrada de asfalto opcional ao longo da trilha mais
longa (pista aplainada, borda gasta, buracos, faixas pintadas).

Pesquisa (docs/WORLDFORGE.md, 05/10/2026): mistura por altura (Mishkinis),
duas escalas + variação larga contra repetição, splats/decalques como no Age
of Empires IV e Company of Heroes, espalhamento por mapas de densidade como no
Horizon Zero Dawn, sol baixo (sombra legível pro RTS), neve no que olha pra
cima (as "capas" de neve de Uncharted 4 / Red Dead 2: máscara pela normal).
"""
import json
import math
import os
import sys

import unreal

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
ASSETS = os.path.join(PROJECT, 'Intermediate', 'Assets')
DEST = '/Game/Slice'
COMPONENTS, QUADS, QUAD_M = 8, 63, 0.5          # 505×505 vértices, 252 m

WARM, CLIFF = (1.18, 0.97, 0.80), (1.40, 1.00, 0.68)     # rochas da Namaqualand no tom do deserto
COLD, COLD_CLIFF = (0.92, 0.95, 1.02), (0.78, 0.84, 0.95)

# ---- as fatias -------------------------------------------------------------------
# camadas: (tipo, nome, superfície, tile m, tinta, extra)
#   plain: extra = (superfície de longe, tile, tinta) ou None
#   rock: triplanar · asphalt: rachaduras + poeira por cima
SLICES = {
    'desert': {
        'map': '/Game/Maps/Slice_Desert', 'seed': 3, 'biome': 1, 'road': 10.0,   # (7,5 → 10 m em 13/10: a de duas faixas das favoritas)
        'layers': [
            # a areia v1 do Look (aprovada 12/10; Game/Tools/look_materials/sand_desert_v1.json):
            # albedo físico 0,49/0,40/0,27, desenho da textura a 25%, ondulação 0,25. Medida na luz
            # desert_golden_hour_v2_aprovada, que a versão final usa (ALookDirector); no painel, com
            # a luz leve dele, lê mais clara (combinado com o Look, 12/10)
            # a v2 do Look (13/10, a "C" do Felipe): a v1 + zonas pelo terreno + ondulações simuladas no vento do
            # mapa. SAND=v1 monta a v1 (ondulação de seno 0,25, sem zonas); SAND=classic a de antes (fim do arquivo)
            ('sand_v2', 'Sand', 'sandy_gravel_02', 2.5, (0.49, 0.40, 0.27), {'mottle': 0.25, 'ripple_tex': 1.0}),
            # terra e cascalho derivados da areia, como na maquete do Look (a terra = areia ×
            # 0,63/0,57/0,50): o cascalho é a mesma areia 15% mais escura, o grão mais marcado e sem
            # ondulação; a terra rachada (dry_ground_01, média linear 0,237/0,199/0,146) tingida pra
            # dar a média da terra da maquete (0,309/0,228/0,135)
            ('sand_v1', 'Gravel', 'gravelly_sand', 2.5, (0.417, 0.34, 0.23), {'mottle': 0.45, 'ripple': 0.0}),
            ('plain', 'Dirt', 'dry_ground_01', 4.0, (1.30, 1.149, 0.922), None),
            ('rock', 'Rock', 'cliff_side', 3.6, (0.95, 0.95, 0.95), None),
            # a crosta da salina (bacia seca): a lama rachada clareada pra sal; clara
            # mas não branca (o branco puro apagaria a USA, que é clara)
            # a crosta da salina (bacia seca): textura gerada (make_slice_textures.py salt),
            # placas poligonais com bordas brancas; a foto de lama rachada clareada
            # ficava cinza-esverdeada e lia como sujeira (revisão de arte de 07/10)
            ('plain', 'Salt', 'gen_salt', 16.0, (0.9, 0.87, 0.82), {'vary': True}),   # um tom abaixo: o branco forte apagava a USA
            # asfalto quebrado escaneado (2,2 m) com a foto aérea de 30 m por cima contra a
            # repetição; as rachaduras geradas e a areia soprada continuam por cima
            ('asphalt', 'Asphalt', 'road_damaged_2', 2.2, (0.85, 0.85, 0.85), {'dust': (0.30, 0.20, 0.12), 'far': ('aerial_asphalt_01', 30.0, (0.9, 0.9, 0.9))}),
        ],
        # Sol a 20°: sombra de ~2,7× a altura, longa pra desenhar o relevo e
        # curta pra não cobrir unidades. Poeira (Mie) e céu claro.
        'light': dict(elev=20.0, az=-60.0, mie=0.012, mie_abs=0.0015, fog=0.008, falloff=0.35,
                      sky=1.3, sky_lux=9000.0, trans=0.55, bias=0.3),
        'snow': 0.0, 'rock_tint': WARM, 'cliff_tint': CLIFF,
        'track_color': (0.22, 0.135, 0.075),
        'scatter': [
            ('RockLarge', 1.0, (0.6, 1.4), 'boulders_big', 0),
            ('RockSmall', 1.3, (0.5, 1.5), 'boulders_small', 260),
            # os tamanhos da maquete do Look (sand_desert_v2.json): pedrinha 0,8–1,8, capim 2,4–4, graveto 0,3–0,6
            ('Pebble', 1.4, (0.8, 1.8), 'pebbles', 120),
            # (maiores e menos: sem a sombra, o tufo de 2,4–4× virava raminho claro quase invisível a 50–100 m;
            # a touceira tem de ler de cima, 13/10)
            ('Tuft', 0.8, (3.2, 5.0), 'tufts_dry', 180),
            ('Branch', 4.0, (0.3, 0.6), 'branches', 120),
            ('Shrub', 2.2, (1.0, 2.2), 'shrubs', 300),
            # o oásis: as árvores de copa do gerador em volta da água e o capim verde
            ('TreeBroadleaf', 0, (1.0, 1.0), 'broadleaf', 0),
            # (13/10, o Felipe: com 1,1 m e 2,6–4,2× o capim do oásis sumiu, "ficou raso"; sem a sombra comprida
            # dá pra voltar a cheio: touceiras largas e mais juntas, 1–1,6 m de roda e 0,5–0,9 m de altura)
            ('OasisGrass', 0.85, (3.6, 5.0), 'tufts_green', 160),
            # mato verde fechado debaixo das palmeiras (o arbusto da floresta; o seco
            # do deserto ali lia como areia até a água, revisão de arte de 07/10)
            ('OasisGrass', 1.6, (0.9, 1.6), 'oasis_shrubs', 220),   # (mais moita verde e maior: o oásis cheio da arte)
        ],
        'cliffs': True,
    },
    'snow': {
        'map': '/Game/Maps/Slice_Snow', 'seed': 5, 'biome': 2, 'road': 6.0, 'paint': False,
        'gen': {'forests': 0.7},     # os controles do painel desta fatia de exemplo
        'layers': [
            # a foto de neve tem albedo médio 0,39 (medido); neve fresca reflete 0,8–0,9:
            # ×2,1. A neve gerada (0,82) entra por cima contra a repetição de 2 m
            # (a aérea do Poly Haven é neve com barro: deixava tudo bege)
            ('plain', 'Snow', 'snow_02', 2.0, (2.1, 2.1, 2.05), {'far': ('gen_snow', 8.0, (1.0, 1.0, 1.0))}),
            ('plain', 'Scree', 'gravelly_sand', 2.5, (0.52, 0.56, 0.62), {'desat': 0.75}),   # cascalho gelado aparecendo (sem saturação: a foto marrom virava barro)
            # trilha e bases: barro frio. A snow_04 é foto de sulcos de pneu paralelos;
            # alinhada ao mundo (não à estrada) as listras cruzavam o acostamento na
            # diagonal e viravam "dentes" (visto em 06/10)
            ('plain', 'Mud', 'mud_forest', 2.35, (0.85, 0.85, 0.92), None),
            ('rock', 'Rock', 'cliff_side', 3.6, (0.62, 0.68, 0.80), 0.6),
            ('plain', 'Ice', 'gen_ice', 24.0, (1.0, 1.0, 1.0), None),   # placas de ~2,7 m (em 6 m o lago virava mosaico miúdo)
            ('asphalt', 'Asphalt', 'asphalt_snow', 2.0, (1.0, 1.0, 1.0), {'dust': (0.72, 0.75, 0.80)}),   # estrada com neve soprada
        ],
        # Manhã de inverno: sol baixo (18°; a 14° a luz ficava dourada demais), céu claro, névoa baixa.
        'light': dict(elev=18.0, az=-45.0, mie=0.004, mie_abs=0.0004, fog=0.02, falloff=0.25,
                      sky=1.4, sky_lux=11000.0, trans=0.5, bias=0.0),
        'snow': 1.0, 'rock_tint': COLD, 'cliff_tint': COLD_CLIFF, 'rock_desat': 0.6,
        'track_color': (0.20, 0.19, 0.18),      # neve pisada vira lama cinza-marrom (Company of Heroes 2)
        'scatter': [
            # as árvores do gerador, uma por uma (posição, tipo e tamanho de WorldTrees.h;
            # o pinheiro de 23 m fica em ~13 m pra não esconder as unidades):
            # o mapa em qualidade real tem as mesmas árvores que o painel mostra
            ('TreeConifer', 0, (1.0, 1.0), 'conifers', 0),
            ('RockLarge', 1.0, (0.5, 1.0), 'rocks_moss', 0),
            ('RockSmall', 1.6, (0.5, 1.4), 'boulders_small', 260),
            ('Tuft', 1.0, (2.0, 4.0), 'tufts_dry', 150),
            ('Branch', 3.0, (0.8, 1.4), 'branches', 120),
            ('Deadwood', 9.0, (0.8, 1.3), 'deadwood', 200),
        ],
        'cliffs': True,
    },
    'forest': {
        'map': '/Game/Maps/Slice_Forest', 'seed': 42, 'biome': 0, 'road': 0.0,
        'gen': {'forests': 0.85},    # os controles do painel desta fatia de exemplo
        'layers': [
            # grama com manchas largas mais secas e mais escuras (o campo de Manor
            # Lords não é um verde só); folhiço sob a copa; terra batida clara
            # escaneadas do Poly Haven, no tamanho real de cada foto (API /info)
            # a foto de grama é palha na média (linear 0,32/0,24/0,10, medido): puxa pro
            # verde; a grama gerada (×2) por cima contra a repetição de 2 m
            ('plain', 'Grass', 'leafy_grass', 2.0, (0.47, 0.68, 0.53),
             # manchas de ~70 m entre verde mais escuro e capim seco amarelado: no mapa
             # inteiro, da câmera de jogo (190 m, sem touceiras), o 0,9–1,2 de antes
             # deixava o gramado de um verde só (06/10)
             {'far': ('gen_moss', 9.0, (2.2, 2.0, 2.4)), 'hue': ((0.82, 0.97, 0.8), (1.32, 1.14, 0.66), 70.0)}),
            # folhas velhas na sombra: mais escuras e pouco saturadas (o laranja de
            # outono em anel em volta dos bosques lia como contorno de seleção, 07/10)
            ('plain', 'Litter', 'forest_leaves_02', 3.0, (0.55, 0.52, 0.46), {'desat': 0.45}),
            ('plain', 'Trail', 'forest_ground_04', 3.15, (1.0, 1.0, 1.0), None),
            ('rock', 'Rock', 'cliff_side', 3.6, (0.75, 0.78, 0.75), 0.75),   # o paredão laranja do deserto vira granito
            ('plain', 'Mud', 'mud_forest', 2.35, (1.0, 1.0, 1.0), None),
        ],
        # Tarde de verão: sol a 32° (a copa faz sombra salpicada), céu azul limpo.
        'light': dict(elev=32.0, az=-55.0, mie=0.005, mie_abs=0.0005, fog=0.01, falloff=0.3,
                      sky=1.2, sky_lux=14000.0, trans=0.7, bias=0.6),
        'snow': 0.0, 'rock_tint': (0.85, 0.88, 0.85), 'cliff_tint': (0.85, 0.88, 0.85), 'rock_desat': 0.75,
        'track_color': (0.12, 0.09, 0.06),
        'scatter': [
            # as árvores do gerador (a floresta temperada mistura folha larga e pinheiro)
            ('TreeBroadleaf', 0, (1.0, 1.0), 'broadleaf', 0),
            ('TreeConifer', 0, (1.0, 1.0), 'conifers_green', 0),
            ('RockLarge', 1.0, (0.5, 1.0), 'rocks_moss', 0),        # rochas de 2–3 m com musgo
            ('RockSmall', 3.5, (0.2, 0.5), 'rocks_moss', 220),
            ('Tuft', 0.6, (1.5, 3.2), 'tufts_green', 160),    # campo de grama: ~100 mil touceiras (Nanite)
            ('Shrub', 1.8, (0.7, 1.3), 'shrubs_green', 260),         # arbustos de 1–2 m
            ('Undergrowth', 1.3, (0.9, 1.6), 'ferns', 140),          # samambaias de 0,5–1 m sob a mata
            ('Undergrowth', 2.1, (2.0, 3.5), 'sprouts', 100),        # mudas de 0,3–1,3 m
            ('Deadwood', 8.0, (0.8, 1.3), 'deadwood', 200),          # tocos e troncos caídos
            ('Branch', 3.0, (0.8, 1.4), 'branches', 120),
        ],
        'cliffs': True,
        'water': True,
    },
}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
lib = unreal.WorldForgeEditorLib


def log(*a):
    unreal.log('FORGE ' + ' '.join(str(x) for x in a))


# ---- importação ----------------------------------------------------------------

def import_file(path, folder, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', path)
    t.set_editor_property('destination_path', folder)
    if name:
        t.set_editor_property('destination_name', name)
    t.set_editor_property('automated', True)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('save', False)
    asset_tools.import_asset_tasks([t])
    return [unreal.load_asset(p) for p in t.get_editor_property('imported_object_paths')]


_TEX = {}


def texture(path, folder, kind):
    """kind: color (sRGB), normal, masks (linear: ARM, altura, alfa). Uma vez por execução."""
    key = (path, folder)
    if key in _TEX:
        return _TEX[key]
    name = 'T_' + os.path.splitext(os.path.basename(path))[0]
    # já importada e a foto não mudou: usa a que existe, sem regravar. Regravar
    # toda vez falhava com o app aberto (a textura em uso fica com o arquivo
    # preso: "Error Code 32"), e o "ver em qualidade real" não montava (07/10)
    pkg = os.path.join(PROJECT, 'Content', folder[len('/Game/'):].replace('/', os.sep), name + '.uasset')
    if eal.does_asset_exist(folder + '/' + name) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(path):
        tex = unreal.load_asset(folder + '/' + name)
        _TEX[key] = tex
        return tex
    objs = import_file(path, folder, name)
    tex = unreal.load_asset(folder + '/' + name) if not objs else objs[0]
    if kind == 'normal':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif kind == 'masks':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property('srgb', False)
    elif kind == 'gray':
        # (16/10, a luz assada da ondulação do Look: um canal linear de 16 bits; o G8 guarda 1/255, o BC1 dava degrau de 3%)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
        tex.set_editor_property('srgb', False)
    eal.save_loaded_asset(tex)
    _TEX[key] = tex
    return tex


def find(folder_files, key):
    for f in folder_files:
        if key in os.path.basename(f):
            return f
    return None


def surface(aid):
    """Superfície PBR: as do Poly Haven (pasta = id) ou as geradas (gen_<nome>)."""
    src = os.path.join(ASSETS, 'Generated', aid[4:]) if aid.startswith('gen_') else os.path.join(ASSETS, aid)
    files = [os.path.join(src, f) for f in os.listdir(src)]
    folder = DEST + '/Surfaces/' + aid
    # o Poly Haven não é constante no nome dos mapas (diff/diffuse/col, disp/height)
    def first(*keys):
        return next(f for f in (find(files, k) for k in keys) if f)
    return {
        'diff': texture(first('_diff_', '_diffuse_', '_col_'), folder, 'color'),
        'nor': texture(first('_nor_dx_'), folder, 'normal'),
        'arm': texture(first('_arm_'), folder, 'masks'),
        'disp': texture(first('_disp_', '_height_'), folder, 'masks'),
    }


# ---- materiais ----------------------------------------------------------------

def shared_material(name, folder=None):
    """(16/10) O material que não depende do mapa (a faixa pintada, a rocha dos monólitos, as palmeiras, a água do
    bioma, o chão da vista ao vivo): se já existe, é reusado. Apagar e recriar a cada montagem falhava com qualquer
    Unreal usando o material (a vista do WorldForge, ou o jogo, que linka a Slice): o arquivo preso não sai e o
    create_asset volta None (a montagem do World_TestSandV2Dune, 16/10 02:20). SHARED_REBUILD=1 refaz todos (quando
    o código de um deles mudar)."""
    path = (folder or DEST + '/Materials') + '/' + name
    if os.environ.get('SHARED_REBUILD', '0') != '1' and eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return None


class Graph:
    def __init__(self, name, folder=DEST + '/Materials'):
        path = folder + '/' + name
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        self.m = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        self.failed = []
        self.y = 0

    def node(self, cls, x=0, y=None, **props):
        if y is None:
            self.y += 120
            y = self.y
        e = mel.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, out, b, inp):
        if not mel.connect_material_expressions(a, out, b, inp):
            self.failed.append('%s.%s -> %s.%s' % (a.get_name(), out, b.get_name(), inp))

    def out(self, a, pin, prop):
        if not mel.connect_material_property(a, pin, prop):
            self.failed.append('%s.%s -> %s' % (a.get_name(), pin, prop))

    def mul(self, a, a_pin, b, b_pin='', x=0):
        n = self.node(unreal.MaterialExpressionMultiply, x)
        self.link(a, a_pin, n, 'A')
        self.link(b, b_pin, n, 'B')
        return n

    def const3(self, rgb, x=0):
        return self.node(unreal.MaterialExpressionConstant3Vector, x, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))

    def mask(self, src, pin, ch, x=-100):
        # ComponentMask nasce com R e G ligados: sempre dizer os quatro canais
        m = self.node(unreal.MaterialExpressionComponentMask, x, r=ch == 'r', g=ch == 'g', b=ch == 'b', a=ch == 'a')
        self.link(src, pin, m, '')
        return m

    def custom(self, code, inputs, out_type, x=-300, extra=()):
        c = self.node(unreal.MaterialExpressionCustom, x)
        ins = []
        for n, _, _ in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property('input_name', n)
            ins.append(ci)
        c.set_editor_property('inputs', ins)
        c.set_editor_property('output_type', out_type)
        c.set_editor_property('code', code)
        outs = []
        for n, t in extra:
            co = unreal.CustomOutput()
            co.set_editor_property('output_name', n)
            co.set_editor_property('output_type', t)
            outs.append(co)
        if outs:
            c.set_editor_property('additional_outputs', outs)
        for n, src, pin in inputs:
            self.link(src, pin, c, n)
        return c

    def sample(self, tex, uv, kind, x=-600):
        st = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
              'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
              'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        s = self.node(unreal.MaterialExpressionTextureSample, x, texture=tex, sampler_type=st,
                      # amostrador compartilhado: o terreno usa dezenas de texturas
                      sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        if uv is not None:
            self.link(uv, '', s, 'UVs')
        return s

    def finish(self, label):
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        log(label, 'failed links:', self.failed or 'none')
        return self.m


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4


# A pedra com desvio dinâmico (14/10, a otimização do chão, ROCK_BRANCH=1): a mesma conta da WorldAlignedTexture e
# da WorldAlignedNormal da 5.8 (copiada do HLSL que a Unreal gera delas, UWorldForgeEditorLib::MaterialHlsl), num nó
# só e com dois cortes que não mudam o pixel:
# - Gate = 0: a pedra não pode aparecer ali (peso 0 e outra camada dominante, ≥ 0,53; a mistura por altura dá à pedra
#   0,0001 contra ≥ 0,06 da dominante: menos de meio nível de 8 bits). Nada é amostrado.
# - as projeções de lado com peso zero (a WorldAlignedTexture amostra as três e mistura por saturate(3·|N|−1): no chão
#   com menos de ~48° de inclinação as duas de lado entram com zero).
# Dentro do desvio as derivadas vêm de fora (SampleGrad): o ddx/ddy dentro de um "if" que varia por pixel é indefinido.
ROCK_BRANCH_HLSL = r"""
#define SS(T) GetMaterialSharedSampler(T##Sampler, View.MaterialTextureBilinearWrapedSampler)
#define RB_SG(T, UV, D) T.SampleGrad(SS(T), UV, D.xy, D.zw)
#define RB_ID(x) (x)
#define RB_TRI(OUT, T, PROC, A, B, C, GA, GB, GC) { OUT = 0; if (wz < 1.0) { OUT = PROC(RB_SG(T, A, GA)).rgb; if (wx > 0.0) OUT = lerp(OUT, PROC(RB_SG(T, B, GB)).rgb, wx); } if (wz > 0.0) OUT = lerp(OUT, PROC(RB_SG(T, C, GC)).rgb, wz); }
FWSVector3 wp = GetWorldPosition_NoMaterialOffsets(Parameters);
FWSVector3 p1 = WSDivide(wp, MaterialFloat3(-S1, -S1, -S1));
FWSVector3 p2 = WSDivide(wp, MaterialFloat3(-S2, -S2, -S2));
float2 a1 = WSApplyAddressMode(MakeWSVector(WSGetX(p1), WSGetZ(p1)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float2 b1 = WSApplyAddressMode(MakeWSVector(WSGetY(p1), WSGetZ(p1)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float2 c1 = WSApplyAddressMode(MakeWSVector(WSGetX(p1), WSGetY(p1)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float2 a2 = WSApplyAddressMode(MakeWSVector(WSGetX(p2), WSGetZ(p2)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float2 b2 = WSApplyAddressMode(MakeWSVector(WSGetY(p2), WSGetZ(p2)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float2 c2 = WSApplyAddressMode(MakeWSVector(WSGetX(p2), WSGetY(p2)), LWCADDRESSMODE_WRAP, LWCADDRESSMODE_WRAP);
float4 ga1 = float4(ddx(a1), ddy(a1)), gb1 = float4(ddx(b1), ddy(b1)), gc1 = float4(ddx(c1), ddy(c1));
float4 ga2 = float4(ddx(a2), ddy(a2)), gb2 = float4(ddx(b2), ddy(b2)), gc2 = float4(ddx(c2), ddy(c2));
float3 Np = Parameters.TangentToWorld[2];
float wx = saturate(lerp((0.0 - 1.0), (1.0 + 1.0), abs(Np.r)));
float wz = saturate(lerp((0.0 - 1.0), (1.0 + 1.0), abs(Np.b)));
Nout = 0; Rough = 0; AO = 0; H = 0;
float3 col = 0;
[branch] if (Gate > 0.5)
{
    float3 d1, d2, m, h;
    RB_TRI(d1, Diff, ProcessMaterialColorTextureLookup, a1, b1, c1, ga1, gb1, gc1)
    RB_TRI(d2, Diff, ProcessMaterialColorTextureLookup, a2, b2, c2, ga2, gb2, gc2)
    RB_TRI(m, Arm, RB_ID, a1, b1, c1, ga1, gb1, gc1)
    RB_TRI(h, Disp, RB_ID, a1, b1, c1, ga1, gb1, gc1)
    col = lerp(d1, d2, 0.5);
    Rough = m.g;
    AO = m.r;
    H = h.r;
    // a WorldAlignedNormal, conta por conta
    float3 N1 = normalize(Np);
    float3 T1 = cross(N1, normalize(float3(0.0, 0.0, 1.0)));
    float q = dot(T1, T1);
    float3 t1 = select((abs(q - 0.000001) > 0.00001), select((q >= 0.000001), float4(normalize(T1), 0.0), float4(0, 0, 0, 1.0)), float4(0, 0, 0, 1.0)).rgb;
    float3 B1 = cross(T1, N1);
    q = dot(B1, B1);
    float3 bb1 = select((abs(q - 0.000001) > 0.00001), select((q >= 0.000001), float4(normalize(B1), 0.0), float4(0, 0, 0, 1.0)), float4(0, 0, 0, 1.0)).rgb;
    float3 nA = UnpackNormalMap(RB_SG(Nor, a1, ga1)).rgb * float3(float2(select((dot(Np, float3(0.0, 1.0, 0.0)) >= 0.0), -1.0, 1.0), -1.0), 1.0);
    float3 nB = UnpackNormalMap(RB_SG(Nor, b1, gb1)).rgb * float3(float2(select((dot(Np, float3(1.0, 0.0, 0.0)) >= 0.0), 1.0, -1.0), -1.0), 1.0);
    float3 W27 = mul(float3(0.0, 0.0, 1.0), Parameters.TangentToWorld);
    float3 mm = lerp(nA, nB, saturate(lerp((0.0 - 0.0), (0.0 + 1.0), abs(W27.r))).r);
    float3 side = ((t1 * ((float3)mm.r)) + (bb1 * ((float3)mm.g))) + ((N1 * ((float3)mm.b)) + float3(0.0, 0.0, 0.0));
    float3 T2 = cross(N1, normalize(float3(0.0, 1.0, 0.0)));
    q = dot(T2, T2);
    float3 t2 = select((abs(q - 0.000001) > 0.00001), select((q >= 0.000001), float4(normalize(T2), 0.0), float4(0, 0, 0, 1.0)), float4(0, 0, 0, 1.0)).rgb;
    float3 B2 = cross(T2, N1);
    q = dot(B2, B2);
    float3 bb2 = select((abs(q - 0.000001) > 0.00001), select((q >= 0.000001), float4(normalize(B2), 0.0), float4(0, 0, 0, 1.0)), float4(0, 0, 0, 1.0)).rgb;
    float3 nC = UnpackNormalMap(RB_SG(Nor, c1, gc1)).rgb * float3(float2(select((dot(Np, float3(0.0, 0.0, 1.0)) >= 0.0), 1.0, -1.0), -1.0), 1.0);
    float3 top = ((t2 * ((float3)nC.r)) + (bb2 * ((float3)nC.g))) + ((N1 * ((float3)nC.b)) + float3(0.0, 0.0, 0.0));
    Nout = mul((float3x3)(Parameters.TangentToWorld), lerp(side, top, saturate(lerp((0.0 - 0.0), (0.0 + 1.0), abs(W27.b))).r));
}
return col;
"""


# O asfalto sem desenho repetido (16/10, o Felipe na hora dourada: "olha quanto padronizado ficou esses detalhes [...] não
# tá natural", docs/WORLDFORGE.md item 10; a 2ª opinião pela literatura). A foto road_damaged_2 (4K, ladrilho de 2,2 m) tem
# poucos buracos marcados, e repetida eles viravam grade; o sol baixo acende o relevo. Dois passos, só na laje (ASPH_HEX=1):
# 1. a base em hexágonos girados (Mikkelsen, "Practical Real-Time Hex-Tiling", JCGT 2022; o TriangleGrid e o Gain3 do
#    código dele): cada vértice da grade gira a foto ao acaso e lê uma janela SEM buraco (os centros livres que o
#    Tools/asphalt_hex_prep.py acha na altura); um só peso pras 4 texturas (cor, normal, ARM, altura), pela altura com o
#    expoente 7 e o contraste 0,6; a normal mistura como derivada, girada de volta com a célula; o mip pelas derivadas do
#    UV contínuo (SampleGrad), sem costura;
# 2. os buracos como carimbos (o texture bombing, GPU Gems cap. 20): os 4 recortados da mesma foto (o atlas do prep), um
#    sorteio por célula de 6 m, girados, espelhados e na escala 0,8–1,2; a chance pelas zonas do asfalto v2 do Look
#    (ASPHALT_V2_CODE, item 8: boa 0,03, cansada 0,25, abandonada 0,5) e 60% a mais perto da beira.
# Os amostradores são os compartilhados do mundo (o mesmo filtro das outras camadas; o terreno tem limite de 16).
ASPH_HEX_HLSL = r"""
#define HX_SS(T) GetMaterialSharedSampler(T##Sampler, View.MaterialTextureBilinearWrapedSampler)
#define HX_SC(T) GetMaterialSharedSampler(T##Sampler, View.MaterialTextureBilinearClampedSampler)
static const float2 HX_CEN[@NC@] = { @CEN@ };
static const float HX_SZ[4] = { @SZ@ };
const float TILE = @TILE@;
const float RATE = @RATE@;
float2 uv = WP.xy / (TILE * 100.0);
// as derivadas do UV vêm de fora (os nós DDX/DDY da Unreal): no Nanite elas são analíticas, e o ddx dentro do Custom
// escolhia mips borrados (a 1ª prova, dirt_gasta_67_hex: o detalhe fino caiu 27% no sol a 20°)
float2 DX = DUx, DY = DUy;
// 1. a grade de triângulos (TriangleGrid): os 3 vértices em volta e os pesos baricêntricos
float2 gq = uv * RATE * 3.4641016;
float2 sk = float2(gq.x - 0.57735027 * gq.y, 1.15470054 * gq.y);
float2 bid = floor(sk);
float3 tt = float3(frac(sk), 0.0);
tt.z = 1.0 - tt.x - tt.y;
float s = step(0.0, -tt.z);
float s2 = 2.0 * s - 1.0;
float3 w = float3(-tt.z * s2, s - tt.y * s2, s - tt.x * s2);
float2 vx[3];
vx[0] = bid + float2(s, s);
vx[1] = bid + float2(s, 1.0 - s);
vx[2] = bid + float2(1.0 - s, s);
float3 cc[3];
float2 dd[3];
float3 aa[3];
float3 hv = 0;
[unroll] for (int i = 0; i < 3; i++)
{
    float2 v = vx[i];
    float2 cuv = float2(v.x + 0.5 * v.y, 0.8660254 * v.y) / (3.4641016 * RATE);
    float hA = frac(sin(dot(v, float2(127.1, 311.7))) * 43758.5453);
    float hB = frac(sin(dot(v, float2(269.5, 183.3))) * 43758.5453);
    float an = hA * 6.2831853;
    float cs = cos(an), sn = sin(an);
    float2x2 R = float2x2(cs, -sn, sn, cs);
    float2 st = mul(uv - cuv, R) + HX_CEN[min((int)(hB * @NC@.0), @NC@ - 1)];
    float2 gx = mul(DX, R), gy = mul(DY, R);
    cc[i] = ProcessMaterialColorTextureLookup(D.SampleGrad(HX_SS(D), st, gx, gy)).rgb;
    float3 nt = UnpackNormalMap(Nn.SampleGrad(HX_SS(Nn), st, gx, gy)).rgb;
    dd[i] = mul(R, -nt.xy / max(nt.z, 0.125));
    aa[i] = Ar.SampleGrad(HX_SS(Ar), st, gx, gy).rgb;
    hv[i] = Hh.SampleGrad(HX_SS(Hh), st, gx, gy).r;
}
// o peso: a mais alta ganha (0,6) e o expoente 7; depois o contraste (Gain3, r = 0,6)
float3 W = lerp(1.0, hv, 0.6) * pow(max(w, 0.0), 7.0);
W /= max(W.x + W.y + W.z, 1e-5);
float gk = log(1.0 - 0.6) / log(0.5);
float3 sg = 2.0 * step(0.5, W);
float3 mg = 2.0 * (1.0 - sg);
float3 gr = 0.5 * sg + 0.25 * mg * pow(max(0.0, sg + W * mg), gk);
W = gr / (gr.x + gr.y + gr.z);
float3 col = W.x * cc[0] + W.y * cc[1] + W.z * cc[2];
float3 arm = W.x * aa[0] + W.y * aa[1] + W.z * aa[2];
// a normal e a altura com a transição larga (o expoente 3, sem o ganho; o artigo usa r = 0,5 nas normais): com a cor
// nítida nas três, a 67 m a textura vem num mip liso e a troca brusca entre duas cópias giradas virava uma dobra na borda
// de cada célula, e o sol rasante acendia um "paralelepípedo" de ~40 cm (a prova perf_hex, 17/10)
float3 Wn = pow(max(w, 0.0), 3.0);
Wn /= max(Wn.x + Wn.y + Wn.z, 1e-5);
// a mistura que preserva a variância (Burley, "On Histogram-Preserving Blending for Randomized Texture Tiling", JCGT 2019;
// Heitz e Neyret 2018): a média de três normais giradas achata o relevo na transição larga (a prova dirt_gasta_67_hex2:
// o detalhe fino caiu de 10,5 pra 7,7 no sol a 20°); dividir pela raiz da soma dos pesos ao quadrado devolve o contraste.
// A derivada do asfalto tem média ~0 (o chão plano), então não precisa tirar a média antes
float2 dv = (Wn.x * dd[0] + Wn.y * dd[1] + Wn.z * dd[2]) / max(sqrt(dot(Wn, Wn)), 0.25);
float h = dot(Wn, hv);
// 2. o buraco da célula de 6 m (um sorteio por célula; o carimbo nunca passa da borda dela)
float2 pm = WP.xy * 0.01;
float2 cell = floor(pm / 6.0);
float q0 = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float q1 = frac(sin(dot(cell, float2(39.346, 11.135))) * 24634.6345);
float q2 = frac(sin(dot(cell, float2(73.156, 52.235))) * 15731.7431);
float q3 = frac(sin(dot(cell, float2(26.651, 91.733))) * 32462.3197);
float q4 = frac(q0 * 17.31 + q3 * 3.7);
float q5 = frac(q1 * 13.17 + q2 * 5.3);
float2 zc = (cell + 0.5) * 6.0;
float zA = saturate((Beach.SampleLevel(HX_SS(Beach), zc / 35.0 + 0.13, 0).r - 0.262) / 0.04);
float zB = saturate((Beach.SampleLevel(HX_SS(Beach), zc / 55.0 + 0.47, 0).g - 0.226) / 0.03);
float tired = smoothstep(0.4, 0.75, zA);
float aband = smoothstep(0.5, 0.85, zB) * (1.0 - 0.4 * tired);
float2 pc = zc + (float2(q4, q5) - 0.5) * 3.8;
float4 rt = RD.SampleLevel(HX_SC(RD), pc / S + 0.5, 0);
float edge = 0.5 * rt.a * 25.5 - abs((rt.b - 0.5) * 20.0);
float chance = (0.03 + 0.22 * tired + 0.47 * aband) * (1.0 + 0.6 * (1.0 - smoothstep(0.3, 2.5, edge)));
Pot = 0.0;
[branch] if (q0 < chance)
{
    int ks = min((int)(q1 * 4.0), 3);
    float sz = HX_SZ[ks] * (0.8 + 0.4 * q2);
    float an = q3 * 6.2831853;
    float ca = cos(an), sa = sin(an);
    float fl = frac(q2 * 7.77) < 0.5 ? -1.0 : 1.0;
    float2 d = pm - pc;
    float2 lq = float2((ca * d.x + sa * d.y) * fl, -sa * d.x + ca * d.y) / sz;
    if (abs(lq.x) < 0.5 && abs(lq.y) < 0.5)
    {
        float2 at = (float2(ks % 2, ks / 2) + lq + 0.5) * 0.5;
        float2 dxm = DX * TILE, dym = DY * TILE;
        float2 gx = float2((ca * dxm.x + sa * dxm.y) * fl, -sa * dxm.x + ca * dxm.y) * (0.5 / sz);
        float2 gy = float2((ca * dym.x + sa * dym.y) * fl, -sa * dym.x + ca * dym.y) * (0.5 / sz);
        float4 pd = PD.SampleGrad(HX_SS(PD), at, gx, gy);
        float m = pd.a;
        float3 pn = UnpackNormalMap(PN.SampleGrad(HX_SS(PN), at, gx, gy)).rgb;
        float2 pdv = -pn.xy / max(pn.z, 0.125);
        pdv.x *= fl;
        pdv = float2(ca * pdv.x - sa * pdv.y, sa * pdv.x + ca * pdv.y);
        col = lerp(col, ProcessMaterialColorTextureLookup(pd).rgb, m);
        dv = lerp(dv, pdv, m);
        arm = lerp(arm, PA.SampleGrad(HX_SS(PA), at, gx, gy).rgb, m);
        h = lerp(h, PH.SampleGrad(HX_SS(PH), at, gx, gy).r, m);
        Pot = m;
    }
}
Nrm = normalize(float3(-dv, 1.0));
Rough = arm.g;
Ao = arm.r;
Disp = h;
return col;
"""


def asphalt_hex_code():
    """O ASPH_HEX_HLSL com os centros livres e o tamanho dos buracos do Tools/asphalt_hex.json (o asphalt_hex_prep.py);
    None sem o arquivo ou sem os atlas."""
    path = os.path.join(PROJECT, 'Tools', 'asphalt_hex.json')
    if not os.path.exists(path) or not os.path.exists(os.path.join(PROJECT, 'Intermediate', 'Assets', 'road_damaged_2_pot', 'AsphPot_D.png')):
        return None
    with open(path, encoding='utf-8') as f:
        j = json.load(f)
    cen = ', '.join('float2(%.4f, %.4f)' % (c[0], c[1]) for c in j['centers'])
    sz = ', '.join('%.3f' % s for s in (j['stamp_m'] + [j['stamp_m'][-1]] * 4)[:4])
    return (ASPH_HEX_HLSL.replace('@NC@', str(len(j['centers']))).replace('@CEN@', cen).replace('@SZ@', sz)
            .replace('@TILE@', '%.3f' % j['tile_m']).replace('@RATE@', '%.3f' % j['rate']))


# A areia v1 do Look (Game/Tools/look_materials/sand_desert_v1_ground.hlsl, a parte da areia com
# SALB = 1; a normal sai no espaço tangente do terreno: vn + nxy·0,8 vira (nxy·0,8, 1)). O
# amostrador é o compartilhado do mundo (GetMaterialSharedSampler), como as outras camadas.
SAND_V1_HLSL = r"""
#define SS(T) GetMaterialSharedSampler(T##Sampler, View.MaterialTextureBilinearWrapedSampler)
float2 p = WP.xy * 0.01;
// o desenho: a areia fina pisada (gravelly_sand a 1,3 m e a 4,1 m girada 37 graus) e a foto de praia
float3 g1 = GravD.Sample(SS(GravD), p / 1.3).rgb;
float3 g2 = GravD.Sample(SS(GravD), float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 4.1 + 0.37).rgb;
float sl = dot(lerp(g1, g2, 0.4), float3(0.2126, 0.7152, 0.0722)) / 0.2;
float mv8 = saturate((Beach.Sample(SS(Beach), p / 8.0 + 0.71).g - 0.22) / 0.04);
float mv40 = saturate((Beach.Sample(SS(Beach), p / 40.0 + 0.29).r - 0.26) / 0.05);
float mv200 = saturate((Beach.Sample(SS(Beach), p / 200.0 + 0.53).g - 0.22) / 0.04);
float macro3 = 1.0 + 0.2 * ((mv8 - 0.5) + (mv40 - 0.5) + (mv200 - 0.5)) / 1.5;   // +-10% em 8, 40 e 200 m
float3 sand = Albedo * lerp(1.0, sl, Mottle) * macro3;
float gao = GravM.Sample(SS(GravM), p / 1.3).r;
sand *= lerp(1.0, gao * gao, 0.7);   // as frestinhas entre os graos
// o relevo fino: grao (x1,35), montinhos a 6 m em manchas de 25 m e a 2,3 m girada
float2 n1 = (GravN.Sample(SS(GravN), p / 1.3).rg * 2.0 - 1.0) * 1.35;
float2 n2 = BeachN.Sample(SS(BeachN), p / 6.0 + 0.19).rg * 2.0 - 1.0;
float2 n3 = BeachN.Sample(SS(BeachN), float2(0.6 * p.x + 0.8 * p.y, -0.8 * p.x + 0.6 * p.y) / 2.3 + 0.61).rg * 2.0 - 1.0;
float rp = smoothstep(0.35, 0.75, saturate((Beach.Sample(SS(Beach), p / 25.0 + 0.83).g - 0.22) / 0.04));
float2 nxy = n1 + n2 * lerp(0.25, 0.9, rp) + n3 * 0.45;
// as ondulacoes de vento (Journey, GDC 2013; Zucconi): 11 cm, 35 cm e 80 cm, crista assimetrica,
// entortadas por ruido, apagadas abaixo de ~4 px (fwidth), so na normal
if (Ripple > 0.0) {
    float2 wd = normalize(float2(0.8, 0.6));
    float ang = (saturate((Beach.Sample(SS(Beach), p / 600.0 + 0.11).r - 0.26) / 0.05) - 0.5) * 0.52;
    wd = float2(wd.x * cos(ang) - wd.y * sin(ang), wd.x * sin(ang) + wd.y * cos(ang));
    float rmask = smoothstep(0.3, 0.55, saturate((Beach.Sample(SS(Beach), p / 180.0 + 0.47).g - 0.22) / 0.04));
    float2 rip = float2(0.0, 0.0);
    [unroll] for (int b = 0; b < 3; b++) {
        float lam = (b == 0) ? 0.11 : ((b == 1) ? 0.35 : 0.8);
        float A = (b == 0) ? 0.005 : ((b == 1) ? 0.02 : 0.03);
        float nW = saturate((Beach.Sample(SS(Beach), p / (120.0 * lam) + 0.3 * b).r - 0.26) / 0.05);
        float2 q = p + 0.4 * lam * (2.0 * nW - 1.0) * float2(1.0, 0.7);
        float nB = saturate((Beach.Sample(SS(Beach), p / 60.0 + 0.6 + 0.2 * b).g - 0.22) / 0.04);
        float phi = 6.2832 * dot(q, wd) / lam + 9.4 * nB;
        float psi = phi - 0.55 * sin(phi);
        float fade = 1.0 - smoothstep(0.8, 1.6, fwidth(phi));
        float dh = A * fade * cos(psi) * (1.0 - 0.55 * cos(phi)) * 6.2832 / lam;
        rip += -dh * wd;
    }
    nxy += rip * rmask * Ripple / 0.8;
}
float3 sM = SandM.Sample(SS(SandM), p / 2.5).rgb;
Rough = sM.g;
AO = sM.r;
Nrm = normalize(float3(nxy * 0.8, 1.0));
#undef SS
return sand;
"""


# A areia v2 do Look (13/10, o Felipe escolheu a "C"; Game/Tools/look_materials/sand_desert_v2*): a v1 + o
# mapa de zonas (liso, ondulado, pedregoso, revirado; o resto é o pisado) + as ondulações simuladas (Nishimori &
# Ouchi, T_LL_Ripples) só nas zonas delas, embaralhadas em hexágono (Mikkelsen 2022). As zonas aqui saem do
# terreno do mapa, não de uma textura da cena: a mesma conta do FWorldDetail::ZonesAt (o mesmo ruído, a mesma
# semente fixa), com as regras do Look (pedregoso na encosta e perto da pedra, liso no plano pisado, ondulado no
# aberto). A trilha da esteira fica de fora (no jogo vem do caderno da memória do chão).
def _zones_fbm(out, src, fields):
    """O fbm de ruído de valor do Look (3 oitavas, a escala cai à metade, a semente sobe 1), sem função (o nó
    Custom não aceita função): laços desenrolados. fields = [(escala, semente)]. O C++ (WorldNoise::ZoneFbm)
    faz a mesma conta."""
    n = len(fields)
    sc = ', '.join('%.4f' % f[0] for f in fields)
    sd = ', '.join('%du' % f[1] for f in fields)
    return r"""
float %(out)s[%(n)d];
{
    float scs[%(n)d] = { %(sc)s };
    uint sds[%(n)d] = { %(sd)s };
    [unroll] for (int k = 0; k < %(n)d; k++)
    {
        float acc = 0.0, amp = 1.0, sc = scs[k];
        [unroll] for (int o = 0; o < 3; o++)
        {
            float2 f = %(src)s / sc;
            float2 fi = floor(f);
            float2 t = f - fi;
            t = t * t * (3.0 - 2.0 * t);
            int2 c = (int2)fi;
            uint sd = sds[k] + (uint)o;
            float hv[4];
            [unroll] for (int j = 0; j < 4; j++)
            {
                int2 cc = c + int2(j & 1, j >> 1);
                uint h = (uint)cc.x * 374761393u + (uint)cc.y * 668265263u + sd * 2246822519u;
                h = (h ^ (h >> 13)) * 1274126177u;
                h = h ^ (h >> 16);
                hv[j] = (float)(h & 0x00FFFFFFu) / 16777216.0;
            }
            acc += amp * lerp(lerp(hv[0], hv[1], t.x), lerp(hv[2], hv[3], t.x), t.y);
            sc *= 0.5;
            amp *= 0.5;
        }
        %(out)s[k] = acc / 1.75;
    }
}
""" % dict(out=out, src=src, n=n, sc=sc, sd=sd)


SAND_V2_ZONES_HLSL = _zones_fbm('zw', 'p', [(9.0, 301), (9.0, 302)])

SAND_V2_HLSL = r"""
#define SS(T) GetMaterialSharedSampler(T##Sampler, View.MaterialTextureBilinearWrapedSampler)
float2 p = WP.xy * 0.01;
""" + SAND_V2_ZONES_HLSL + r"""
// as zonas (0..1): liso, ondulado, pedregoso, revirado. Formas de 3-15 m com a borda entortada (7 m a 9 m)
float2 wq = p + (float2(zw[0], zw[1]) - 0.5) * 7.0;
""" + _zones_fbm('zf', 'wq', [(11.0, 311), (6.5, 312), (14.0, 313), (4.5, 314)]) + _zones_fbm('zn', 'p', [(0.8, 401)]) + r"""
float slope = smoothstep(0.03, 0.25, 1.0 - saturate(VNrm.z));
float trod = saturate(Trod);
float rock = saturate(Rock);
float open = saturate(1.0 - trod - rock);
float nA = zf[0] + 0.12 * trod - 0.10 * slope;
float nB = zf[1] + 0.15 * slope + 0.10 * rock;
float nC = zf[2] + 0.04 * open + 0.25 * saturate(Far) - 0.15 * trod - 0.15 * slope;   // longe do pisado a ondulacao domina
float nD = zf[3];
// a borda rasgada, não macia (o Look, 13/10): um ruído fino (0,8 m, +-0,06) antes de um limiar estreito (0,03)
float fe = (zn[0] - 0.5) * 0.45;
float zs = smoothstep(0.595, 0.625, nA + fe);
float zg = smoothstep(0.615, 0.645, nB + fe) * (1.0 - zs);
float zr = smoothstep(0.575, 0.605, nC + fe) * (1.0 - zs) * (1.0 - zg);
float zd = smoothstep(0.695, 0.725, nD + fe) * (1.0 - zr);
// a cor: a v1 (albedo fisico, o desenho da textura fraco, +-10% em 8/40/200 m)
float3 g1 = GravD.Sample(SS(GravD), p / 1.3).rgb;
float3 g2 = GravD.Sample(SS(GravD), float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y) / 4.1 + 0.37).rgb;
float sl = dot(lerp(g1, g2, 0.4), float3(0.2126, 0.7152, 0.0722)) / 0.2;
float mv8 = saturate((Beach.Sample(SS(Beach), p / 8.0 + 0.71).g - 0.22) / 0.04);
float mv40 = saturate((Beach.Sample(SS(Beach), p / 40.0 + 0.29).r - 0.26) / 0.05);
float mv200 = saturate((Beach.Sample(SS(Beach), p / 200.0 + 0.53).g - 0.22) / 0.04);
float macro3 = 1.0 + 0.2 * ((mv8 - 0.5) + (mv40 - 0.5) + (mv200 - 0.5)) / 1.5;
float3 sand = Albedo * lerp(1.0, sl, Mottle) * macro3;
float gao = GravM.Sample(SS(GravM), p / 1.3).r;
sand *= lerp(1.0, gao * gao, 0.7 * (1.0 - 0.6 * zs) + 0.3 * zg);   // o liso tem menos fresta; o pedregoso mais
// pouco tom (o Look, 13/10: com x0,88 e x1,07 as zonas liam como manchas de agua seca a 100 m; a diferenca
// fica no grao, na fresta e no espalhamento)
sand *= lerp(1.0, 1.03, zs);                                          // o compactado um pouco mais claro
sand *= lerp(float3(1, 1, 1), float3(0.96, 0.95, 0.93), zg);          // o pedregoso um pouco mais escuro e frio
sand *= lerp(float3(1, 1, 1), float3(0.94, 0.92, 0.89), zd);          // a revirada (solta, um pouco umida)
// o relevo fino: grao, montinhos a 6 m em manchas e a 2,3 m girada; o liso alisa, o pedregoso marca
float2 n1 = (GravN.Sample(SS(GravN), p / 1.3).rg * 2.0 - 1.0) * 1.35;
float2 n2 = BeachN.Sample(SS(BeachN), p / 6.0 + 0.19).rg * 2.0 - 1.0;
float2 n3 = BeachN.Sample(SS(BeachN), float2(0.6 * p.x + 0.8 * p.y, -0.8 * p.x + 0.6 * p.y) / 2.3 + 0.61).rg * 2.0 - 1.0;
float rp = smoothstep(0.35, 0.75, saturate((Beach.Sample(SS(Beach), p / 25.0 + 0.83).g - 0.22) / 0.04));
float2 nxy = n1 * lerp(1.0, 0.35, zs) * lerp(1.0, 1.4, zg) + (n2 * lerp(0.25, 0.9, rp) + n3 * 0.45) * lerp(1.0, 0.25, zs);
// as ondulacoes simuladas: o ladrilho de 10 m no espaco do vento, em hexagonos sem girar, so na zona delas
{
    float wy = radians(WindYaw);
    float cw = cos(wy), sw = sin(wy);
    float2 q = float2(cw * p.x + sw * p.y, -sw * p.x + cw * p.y) / 10.0;
    float2 dqx = ddx(q), dqy = ddy(q);
    float2 sk = float2(q.x + q.y * 0.57735, q.y * 1.1547) * 2.2;
    float2 bs = floor(sk), fr = frac(sk);
    float2 v1, v2, v3; float3 w;
    if (fr.x + fr.y < 1.0) { v1 = bs; v2 = bs + float2(1, 0); v3 = bs + float2(0, 1); w = float3(1.0 - fr.x - fr.y, fr.x, fr.y); }
    else { v1 = bs + float2(1, 1); v2 = bs + float2(0, 1); v3 = bs + float2(1, 0); w = float3(fr.x + fr.y - 1.0, 1.0 - fr.x, 1.0 - fr.y); }
    w = w * w * w; w /= (w.x + w.y + w.z);
    float2 o1 = frac(sin(float2(dot(v1, float2(127.1, 311.7)), dot(v1, float2(269.5, 183.3)))) * 43758.5453);
    float2 o2 = frac(sin(float2(dot(v2, float2(127.1, 311.7)), dot(v2, float2(269.5, 183.3)))) * 43758.5453);
    float2 o3 = frac(sin(float2(dot(v3, float2(127.1, 311.7)), dot(v3, float2(269.5, 183.3)))) * 43758.5453);
    float4 Rp = Ripples.SampleGrad(SS(Ripples), q + o1, dqx, dqy) * w.x + Ripples.SampleGrad(SS(Ripples), q + o2, dqx, dqy) * w.y
              + Ripples.SampleGrad(SS(Ripples), q + o3, dqx, dqy) * w.z;
    float2 tn = Rp.gb * 2.0 - 1.0;
    tn.y = -tn.y;
    float2 wn = float2(cw * tn.x - sw * tn.y, sw * tn.x + cw * tn.y);
    float m2 = saturate((Beach.Sample(SS(Beach), p / 9.0 + 0.77).r - 0.262) / 0.04);
    float fw = max(length(fwidth(p)), 1e-4);
    float kT = zr * lerp(0.6, 1.0, m2) * (1.0 - smoothstep(0.03, 0.06, fw)) * RipTex;
    nxy += wn / 0.8 * 2.2 * kT;
    sand *= lerp(1.0, Rp.a, kT);
}
float3 sM = SandM.Sample(SS(SandM), p / 2.5).rgb;
Rough = sM.g;
AO = sM.r;
Nrm = normalize(float3(nxy * 0.8, 1.0));
#undef SS
return sand;
"""


def look_asphalt_code():
    """O ASPHALT_CODE do Game/Tools/look_asphalt.py (o asfalto da volta 15 do Look, aprovado pelo Felipe em 13/10), lido
    sem rodar o arquivo; só ASCII. None se não estiver lá. Desligado por padrão (ASPHALT=1 liga): 13/10, o Felipe viu
    no mapa com a luz aprovada do jogo e achou o de antes melhor ("senti uma regressão"); o tom AG do Look é da luz da
    bancada dele, e na v2_aprovada o asfalto ficou frio e acinzentado, com a textura mais ruidosa."""
    import unicodedata
    if os.environ.get('ASPHALT', '0') != '1':
        return None
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_asphalt.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    a = src.find('ASPHALT_CODE = r"""')
    b = src.find('"""', a + 20)
    if a < 0 or b < 0:
        return None
    code = src[a + len('ASPHALT_CODE = r"""'):b]
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*')).encode('ascii', 'ignore').decode('ascii')


def look_asphalt_v2_code():
    """O ASPHALT_V2_CODE do Game/Tools/look_asphalt.py (13/10, pedido do Felipe: o asfalto "em zonas", como a areia
    v2): o asfalto de antes do mapa como base e os defeitos de pavimento por cima (FHWA, relativos à cor de baixo:
    zonas boa/cansada/abandonada, trilha de roda polida com jacaré, a linha de óleo, trincas em bloco, longitudinal e
    transversais, a desagregação da beira, remendos e a areia do lado do vento). Desligado até o A/B com ele
    (ASPHALT2=1 liga)."""
    import unicodedata
    if os.environ.get('ASPHALT2', '1') != '1':   # (aprovado pelo Felipe em 13/10: o v2d com o óleo v4; ASPHALT2=0 volta)
        return None
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_asphalt.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    a = src.find('ASPHALT_V2_CODE = r"""')
    b = src.find('"""', a + 24)
    if a < 0 or b < 0:
        return None
    code = src[a + len('ASPHALT_V2_CODE = r"""'):b]
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*')).encode('ascii', 'ignore').decode('ascii')


def look_disp_code():
    """O DISP_CODE do Game/Tools/look_mf.py (13/10, a cratera do jogo igual à prova, combinado entre o Look e o jogo): o
    deslocamento Nanite no próprio chão (a esteira, a cratera e a terra revirada). DispOn = 0 devolve 0,5 (nada mexe).
    None sem o arquivo. DISP=0 monta sem ele."""
    import unicodedata
    if os.environ.get('DISP', '1') == '0':
        return None
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_mf.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    a = src.find('DISP_CODE = r"""')
    b = src.find('"""', a + 18)
    if a < 0 or b < 0:
        return None
    code = src[a + len('DISP_CODE = r"""'):b]
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*').replace('±', '+-')).encode('ascii', 'ignore').decode('ascii')


def look_ground_memory_code():
    """O CODE do Game/Tools/look_mf.py (a memória do chão do Look), lido do arquivo sem rodar o main() dele;
    só ASCII (o compilador de shader). None se o arquivo não estiver lá."""
    import unicodedata
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_mf.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    a = src.find('CODE = r"""')
    b = src.find('"""', a + 12)
    if a < 0 or b < 0:
        return None
    code = src[a + len('CODE = r"""'):b]
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*')).encode('ascii', 'ignore').decode('ascii')


def look_paint_v2_code():
    """O PAINT_V2_CODE do Game/Tools/look_asphalt.py (a faixa pintada v2 do Look, 16/10: RECEITAS §7), lido sem rodar o
    arquivo; só ASCII. None se não estiver lá. Vai na laje (BuildRoadSlabs) com a posição na pista do T_RoadFrame."""
    import unicodedata
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_asphalt.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    a = src.find('PAINT_V2_CODE = r"""')
    if a < 0:
        return None
    b = src.find('"""', a + len('PAINT_V2_CODE = r"""'))
    code = src[a + len('PAINT_V2_CODE = r"""'):b]
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*')).encode('ascii', 'ignore').decode('ascii')


SLAB_PAINT = False


def look_ground_memory_code_v3():
    """O code_v3() do Game/Tools/look_mf.py (a normal de iluminação da v3, o Look 16/10): o arquivo é executado até
    antes do main() (só constantes e funções) e a função devolve o CODE; só ASCII. None se faltar."""
    import unicodedata
    src_path = os.path.join(os.path.dirname(PROJECT), 'Game', 'Tools', 'look_mf.py')
    if not os.path.exists(src_path):
        return None
    with open(src_path, encoding='utf-8') as f:
        src = f.read()
    i = src.find('def main():')
    if i < 0:
        return None
    ns = {'__name__': 'look_mf_v3'}
    exec(compile(src[:i], src_path, 'exec'), ns)
    if 'code_v3' not in ns:
        return None
    code = ns['code_v3']()
    return unicodedata.normalize('NFKD', code.replace('°', ' graus').replace('×', 'x').replace('·', '*')).encode('ascii', 'ignore').decode('ascii')


def ground_material(name, S, specs, cracks, biome='desert', live=False, slab=False):
    """Landscape: camadas misturadas pela altura da textura, uma mistura por canal.

    live=True: o mesmo chão pra vista ao vivo do app (malha comum, sem
    Landscape): os pesos das camadas vêm de duas texturas do mapa (WeightsA =
    camadas 0–3, WeightsB = 4–5, feitas em runtime por FWorldDetail) e a
    mistura pela altura é a conta da própria Unreal
    (MaterialExpressionLandscapeLayerBlend.cpp, LB_HeightBlend): peso' =
    clamp(lerp(−1, 1, peso) + altura, 0,0001, 1), média ponderada por peso'.
    Com as camadas de depuração do app por cima (OverlayMap).

    Uma mistura por canal (e não "atributos de material"): na 5.8 o
    LandscapeLayerBlend em atributos falhava em parte das combinações de
    camadas ("Arithmetic between types float3 and float2") — a causa era o
    ComponentMask nascendo com R e G ligados; por canal ficou mais simples.
    """
    g = Graph(name)
    wp = g.node(unreal.MaterialExpressionWorldPosition, -1800)
    xy = g.node(unreal.MaterialExpressionComponentMask, -1650, r=True, g=True, b=False, a=False)
    g.link(wp, '', xy, '')
    depth = g.node(unreal.MaterialExpressionPixelDepth, -1800)

    def uv(tile_m, offset=0.0):
        d = g.node(unreal.MaterialExpressionDivide, -1450, const_b=tile_m * 100.0)
        g.link(xy, '', d, 'A')
        if offset:
            a = g.node(unreal.MaterialExpressionAdd, -1300, const_b=offset)
            g.link(d, '', a, 'A')
            return a
        return d

    # variação larga de tom (manchas de ~150 m): quebra a repetição vista de cima
    macro_s = g.sample(S['aerial_beach_01']['diff'], uv(150.0, 0.13), 'color', -1200)
    macro = g.custom('return lerp(0.82, 1.18, saturate((R - 0.45) / 0.25));', [('R', g.mask(macro_s, 'RGB', 'r'), '')], F1, -900)

    WAT = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture')
    WAN = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal')

    def triplanar(tex, tile_m, normal=False):
        obj = g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                     (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
        f = g.node(unreal.MaterialExpressionMaterialFunctionCall, -500)
        f.set_material_function(WAN if normal else WAT)
        g.link(obj, '', f, 'TextureObject')
        g.link(g.const3((tile_m * 100.0,) * 3, -700), '', f, 'TextureSize')
        return f

    def rock_layer(lname, s, tile_m, tint, desat=None):
        if os.environ.get('ROCK_BRANCH', '0') == '1' and not live:
            return rock_layer_branch(lname, s, tile_m, tint, desat)
        diff = triplanar(s['diff'], tile_m)
        # a mesma rocha numa escala 2,7× maior, meio a meio: o estrato do ladrilho
        # de 3,6 m repetia em listras paralelas regulares na parede (06/10)
        diff2 = triplanar(s['diff'], tile_m * 2.7)
        mixd = g.node(unreal.MaterialExpressionLinearInterpolate, -350, const_alpha=0.5)
        g.link(diff, 'XYZ Texture', mixd, 'A')
        g.link(diff2, 'XYZ Texture', mixd, 'B')
        nor = triplanar(s['nor'], tile_m, normal=True)
        arm = triplanar(s['arm'], tile_m)
        disp = triplanar(s['disp'], tile_m)
        base = g.mul(g.mul(mixd, '', g.const3(tint, -300), '', -200), '', macro, '', 0)
        if desat:
            base = g.custom('return lerp(C, dot(C, float3(0.3, 0.59, 0.11)).xxx, %f);' % desat, [('C', base, '')], F3, 100)
        return {'name': lname, 'base': (base, ''), 'nor': (nor, 'XYZ Texture'), 'rough': (g.mask(arm, 'XYZ Texture', 'g'), ''),
                'ao': (g.mask(arm, 'XYZ Texture', 'r'), ''), 'disp': (g.mask(disp, 'XYZ Texture', 'r'), '')}

    def rock_layer_branch(lname, s, tile_m, tint, desat):
        """A rock_layer com desvio dinâmico (ROCK_BRANCH_HLSL): a mesma saída, só calculada onde a pedra pode
        aparecer. O portão olha os pesos das outras camadas (LandscapeLayerSample, a mesma leitura da mistura)."""
        def tobj(tex, normal=False):
            return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                          (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
        others = [sp[1] for sp in specs if sp[1] != lname]
        def lsample(n):
            return g.node(unreal.MaterialExpressionLandscapeLayerSample, -900, parameter_name=n, preview_weight=0.0)
        gate = g.custom('\n'.join(['float o = 0.0;'] + ['o = max(o, O%d);' % i for i in range(len(others))] +
                                   ['return (W > 0.0 || o < 0.53) ? 1.0 : 0.0;']),
                        [('W', lsample(lname), '')] + [('O%d' % i, lsample(n), '') for i, n in enumerate(others)], F1, -700)
        c = g.custom(ROCK_BRANCH_HLSL,
                     [('Gate', gate, ''), ('S1', g.node(unreal.MaterialExpressionConstant, -700, r=tile_m * 100.0), ''),
                      ('S2', g.node(unreal.MaterialExpressionConstant, -700, r=tile_m * 2.7 * 100.0), ''),
                      ('Diff', tobj(s['diff']), ''), ('Nor', tobj(s['nor'], True), ''), ('Arm', tobj(s['arm']), ''),
                      ('Disp', tobj(s['disp']), '')],
                     F3, -400, extra=[('Nout', F3), ('Rough', F1), ('AO', F1), ('H', F1)])
        base = g.mul(g.mul(c, '', g.const3(tint, -300), '', -200), '', macro, '', 0)
        if desat:
            base = g.custom('return lerp(C, dot(C, float3(0.3, 0.59, 0.11)).xxx, %f);' % desat, [('C', base, '')], F3, 100)
        log('rock', lname, 'com desvio dinamico (portao: %s)' % ', '.join(others))
        return {'name': lname, 'base': (base, ''), 'nor': (c, 'Nout'), 'rough': (c, 'Rough'), 'ao': (c, 'AO'), 'disp': (c, 'H')}

    def layer(lname, s, tile_m, tint, extra=None):
        extra = extra or {}
        far = extra.get('far')
        hx = None
        # (17/10, o Felipe: "aprovado") a base em hexágonos sem buraco e os buracos como carimbos (o item 10, ASPH_HEX_HLSL) é o
        # padrão da laje; ASPH_HEX=0 volta o ladrilho de antes
        if slab and lname == 'Asphalt' and os.environ.get('ASPH_HEX', '1') == '1':
            code = asphalt_hex_code()
            if code:
                pot = os.path.join(PROJECT, 'Intermediate', 'Assets', 'road_damaged_2_pot')
                pot_tex = '/Game/Slice/Surfaces/road_damaged_2_pot'
                def hobj(tex, kind):
                    return g.node(unreal.MaterialExpressionTextureObject, -1300, texture=tex,
                                  sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                                                'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                                                'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind])
                huv = uv(tile_m)
                hdx = g.node(unreal.MaterialExpressionDDX, -1300)
                g.link(huv, '', hdx, 'Value')
                hdy = g.node(unreal.MaterialExpressionDDY, -1300)
                g.link(huv, '', hdy, 'Value')
                hx = g.custom(code, [('WP', wp, ''), ('DUx', hdx, ''), ('DUy', hdy, ''),
                                     ('D', hobj(s['diff'], 'color'), ''), ('Nn', hobj(s['nor'], 'normal'), ''),
                                     ('Ar', hobj(s['arm'], 'masks'), ''), ('Hh', hobj(s['disp'], 'masks'), ''),
                                     ('PD', hobj(texture(os.path.join(pot, 'AsphPot_D.png'), pot_tex, 'color'), 'color'), ''),
                                     ('PN', hobj(texture(os.path.join(pot, 'AsphPot_N.png'), pot_tex, 'normal'), 'normal'), ''),
                                     ('PA', hobj(texture(os.path.join(pot, 'AsphPot_ARM.png'), pot_tex, 'masks'), 'masks'), ''),
                                     ('PH', hobj(texture(os.path.join(pot, 'AsphPot_H.png'), pot_tex, 'masks'), 'masks'), ''),
                                     ('Beach', hobj(S['aerial_beach_01']['diff'], 'color'), ''),
                                     ('RD', g.node(unreal.MaterialExpressionTextureObjectParameter, -1300, parameter_name='RoadDir',
                                                   texture=unreal.load_asset('/Game/Textures/T_WaterN')), ''),
                                     ('S', g.node(unreal.MaterialExpressionScalarParameter, -1300, parameter_name='InfoSize', default_value=1024.0), '')],
                              F3, -700, extra=[('Nrm', F3), ('Rough', F1), ('Ao', F1), ('Disp', F1), ('Pot', F1)])
                log('asphalt', 'hex sem buraco + buracos carimbados (ASPH_HEX)')
            else:
                log('asphalt', 'ASPH_HEX sem o Tools/asphalt_hex.json ou os atlas: rode o asphalt_hex_prep.py')
        if hx is not None:
            diff, nor = hx, None
        else:
            a = g.sample(s['diff'], uv(tile_m), 'color')
            b = g.sample(s['diff'], uv(tile_m * 3.7, 0.37), 'color')
            diff = g.node(unreal.MaterialExpressionLinearInterpolate, -400, const_alpha=0.35)
            g.link(a, 'RGB', diff, 'A')
            g.link(b, 'RGB', diff, 'B')
            nor = g.sample(s['nor'], uv(tile_m), 'normal')
        if extra.get('vary'):
            # anti-ladrilho: as duas escalas trocam em manchas de ~60 m (a foto
            # aérea como ruído), na cor e no relevo; a 35% fixo a grade de placas
            # da salina repetia a cada 8 m vista de cima (07/10)
            vn = g.sample(S['aerial_beach_01']['diff'], uv(60.0, 0.71), 'color', -1200)
            va = g.custom('return smoothstep(0.38, 0.62, N.r);', [('N', vn, 'RGB')], F1, -500)
            g.link(va, '', diff, 'Alpha')
            nb = g.sample(s['nor'], uv(tile_m * 3.7, 0.37), 'normal')
            nmix = g.node(unreal.MaterialExpressionLinearInterpolate, -400)
            g.link(nor, 'RGB', nmix, 'A')
            g.link(nb, 'RGB', nmix, 'B')
            g.link(va, '', nmix, 'Alpha')
            base0 = g.mul(diff, '', g.const3(tint, -300), '', -200)
            base0 = g.mul(base0, '', macro, '', 0)
            arm = g.sample(s['arm'], uv(tile_m), 'masks')
            disp = g.sample(s['disp'], uv(tile_m), 'masks')
            return {'name': lname, 'base': (base0, ''), 'nor': (nmix, ''), 'rough': (arm, 'G'), 'ao': (arm, 'R'), 'disp': (disp, 'R')}
        if hx is None:
            arm = g.sample(s['arm'], uv(tile_m), 'masks')
            disp = g.sample(s['disp'], uv(tile_m), 'masks')
        base = g.mul(diff, '', g.const3(tint, -300), '', -200)
        if far is not None:
            # a foto aérea por cima da de perto (o grão de perto vira ruído de
            # longe). Metade das duas sempre e só um pouco mais da aérea ao longe:
            # a troca de 25 a 85 m fazia um anel em volta da câmera que andava com
            # ela (visto no passeio de 05/10)
            fs, ftile, ftint = far
            # (13/10, o teste do Look pedido pelo Felipe: a foto aérea do asfalto é um estacionamento com arcos de pneu e
            # uma trinca reta, e a 30 m os mesmos arcos voltavam ao longo da rua. AerBlur = 1 fica só com a mancha de cor
            # dela: o mip 7 (~1 m por texel) de duas amostras giradas em escalas diferentes. Parâmetro do material: o
            # A/B sai da mesma montagem, -ForgeLandscapeParam=AerBlur=1; AERBLUR=1 muda o padrão)
            ftex = g.node(unreal.MaterialExpressionTextureObject, -1300, texture=S[fs]['diff'],
                          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            fblur = g.node(unreal.MaterialExpressionScalarParameter, -1300, parameter_name='AerBlur',
                           default_value=1.0 if os.environ.get('AERBLUR', '0') == '1' else 0.0)
            fsample = g.custom('float2 p = P.xy * 0.01;\n'
                               'float2 pa2 = float2(0.8 * p.x - 0.6 * p.y, 0.6 * p.x + 0.8 * p.y);\n'
                               'float3 sharp = T.Sample(TSampler, p / %.1f + 0.21).rgb;\n'
                               'float3 soft = lerp(T.SampleLevel(TSampler, p / %.1f + 0.21, 7.0).rgb, T.SampleLevel(TSampler, pa2 / %.1f + 0.63, 7.0).rgb, 0.5);\n'
                               'return lerp(sharp, soft, saturate(B));' % (ftile, ftile, ftile * 47.0 / 30.0),
                               [('T', ftex, ''), ('P', wp, ''), ('B', fblur, '')], F3, -1100)
            fb = g.mul(fsample, '', g.const3(ftint, -300), '', -200)
            t = g.custom('return 0.45 + 0.3 * saturate((D - 1000.0) / 14000.0);', [('D', depth, '')], F1)
            mix = g.node(unreal.MaterialExpressionLinearInterpolate, -100)
            g.link(base, '', mix, 'A')
            g.link(fb, '', mix, 'B')
            g.link(t, '', mix, 'Alpha')
            base = mix
        base = g.mul(base, '', macro, '', 0)
        if 'desat' in extra:
            base = g.custom('return lerp(C, dot(C, float3(0.3, 0.59, 0.11)).xxx, %f);' % extra['desat'], [('C', base, '')], F3, 100)
        if 'hue' in extra:
            # variação de matiz em manchas largas (a foto aérea como ruído)
            ha, hb, hs = extra['hue']
            n = g.sample(S['aerial_beach_01']['diff'], uv(hs, 0.57), 'color', -1200)
            base = g.custom('return C * lerp(A, B, smoothstep(0.35, 0.65, N.g));',
                            [('C', base, ''), ('A', g.const3(ha, -300), ''), ('B', g.const3(hb, -300), ''), ('N', n, 'RGB')], F3, 100)
        if hx is not None:
            return {'name': lname, 'base': (base, ''), 'nor': (hx, 'Nrm'), 'rough': (hx, 'Rough'), 'ao': (hx, 'Ao'), 'disp': (hx, 'Disp')}
        return {'name': lname, 'base': (base, ''), 'nor': (nor, 'RGB'), 'rough': (arm, 'G'), 'ao': (arm, 'R'), 'disp': (disp, 'R')}

    def sand_v1_layer(lname, s, tile_m, albedo, extra):
        """A areia do deserto v1 do Look (aprovada pelo Felipe em 12/10, "uma das melhores até
        agora"): Game/Tools/look_materials/sand_desert_v1_ground.hlsl, o caminho da areia
        (SALB = 1) sem a planta da maquete, e os números de sand_desert_v1.json. O albedo é o
        da física (medido na receita desert_golden_hour); a textura só dá o desenho. Amostrador
        compartilhado (o do mundo, igual às outras camadas): o terreno tem limite de 16."""
        def tobj(tex, kind):
            st = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                  'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                  'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
            return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex, sampler_type=st)
        grav, beach = S['gravelly_sand'], S['aerial_beach_01']
        code = SAND_V1_HLSL
        c = g.custom(code, [('WP', wp, ''), ('Albedo', g.const3(albedo, -500), ''),
                            ('Mottle', g.node(unreal.MaterialExpressionConstant, -500, r=extra.get('mottle', 0.25)), ''),
                            ('Ripple', g.node(unreal.MaterialExpressionConstant, -500, r=extra.get('ripple', 0.25)), ''),
                            ('GravD', tobj(grav['diff'], 'color'), ''), ('GravN', tobj(grav['nor'], 'normal'), ''),
                            ('GravM', tobj(grav['arm'], 'masks'), ''), ('Beach', tobj(beach['diff'], 'color'), ''),
                            ('BeachN', tobj(beach['nor'], 'normal'), ''), ('SandM', tobj(s['arm'], 'masks'), '')],
                     F3, -150, extra=[('Nrm', F3), ('Rough', F1), ('AO', F1)])
        disp = g.sample(s['disp'], uv(tile_m), 'masks')
        return {'name': lname, 'base': (c, ''), 'nor': (c, 'Nrm'), 'rough': (c, 'Rough'), 'ao': (c, 'AO'), 'disp': (disp, 'R')}

    def sand_v2_layer(lname, s, tile_m, albedo, extra):
        """A areia v2 do Look: a v1 + as zonas pelo terreno + as ondulações simuladas no vento do mapa."""
        def tobj(tex, kind):
            st = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                  'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                  'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
            return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex, sampler_type=st)
        grav, beach = S['gravelly_sand'], S['aerial_beach_01']
        if GROUND_V3 and not live:
            _tobj = tobj
            def tobj(tex, kind):
                if kind == 'gray':
                    return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE)
                return _tobj(tex, kind)
        # o que o terreno diz: pisado (terra + asfalto) e pedra
        # e o deserto aberto (FWorldDetail::OpenAt): no B dos pesos ao vivo, no G da textura de informação do mapa
        if live:
            trod = g.custom('return W.b + V.g;', [('W', live_weights, ''), ('V', live_weights, 'WB')], F1, -500)
            rock = g.mask(live_weights, '', 'a', -500)
            far = g.mask(live_weights, 'WB', 'b', -500)
        else:
            def lsample(n):
                return g.node(unreal.MaterialExpressionLandscapeLayerSample, -700, parameter_name=n, preview_weight=0.0)
            trod = g.custom('return D + A;', [('D', lsample('Dirt'), ''), ('A', lsample('Asphalt'), '')], F1, -500)
            rock = lsample('Rock')
            itex = g.node(unreal.MaterialExpressionTextureObjectParameter, -700, parameter_name='WaterInfo',
                          texture=unreal.load_asset('/Game/Textures/T_WaterN'))
            isize = g.node(unreal.MaterialExpressionScalarParameter, -700, parameter_name='InfoSize', default_value=1024.0)
            far = g.custom('return T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0).g * step(0.0, -(T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0).r - 0.5));',
                           [('T', itex, ''), ('S', isize, ''), ('P', wp, '')], F1, -500)
        # o vento do mapa: a coleção do jogo (/Game/Materials/MPC_Wind, Wind = (x, y pra onde sopra, m/s, rajada);
        # a cópia do arquivo dela mora aqui, com os mesmos identificadores), a mesma da fumaça e da poeira
        wv = g.node(unreal.MaterialExpressionCollectionParameter, -800)
        wv.set_editor_property('collection', unreal.load_asset('/Game/Materials/MPC_Wind'))
        wv.set_editor_property('parameter_name', 'Wind')
        wind = g.custom('return degrees(atan2(W.y, W.x));', [('W', wv, '')], F1, -700)
        v3 = GROUND_V3 and not live
        code = SAND_V2_HLSL
        if v3:
            a3 = '// as ondulacoes simuladas: o ladrilho de 10 m no espaco do vento'
            b3 = """    float kT = zr * lerp(0.6, 1.0, m2) * (1.0 - smoothstep(0.03, 0.06, fw)) * RipTex;
    nxy += wn / 0.8 * 2.2 * kT;
    sand *= lerp(1.0, Rp.a, kT);"""
            assert code.count(a3) == 1 and code.count(b3) == 1
            code = code.replace(a3, 'RipLit = 1.0;\n' + a3)
            code = code.replace(b3, """    // a v3 (16/10, o Look, CHAO_V3_ESTUDO 5.1): a luz assada da ondulacao (RippleLit, linear, com mips) nas mesmas
    // 3 amostras; sem o corte pelo fw (a mip segura a distancia), sem a normal da ondulacao e sem a sombra velha na cor
    float rl = (RipLitT.SampleGrad(SS(RipLitT), q + o1, dqx, dqy).r * w.x + RipLitT.SampleGrad(SS(RipLitT), q + o2, dqx, dqy).r * w.y
              + RipLitT.SampleGrad(SS(RipLitT), q + o3, dqx, dqy).r * w.z) * 1.25;
    RipLit = lerp(1.0, rl, zr * lerp(0.6, 1.0, m2) * RipTex);""")
        c = g.custom(code, [('WP', wp, ''), ('VNrm', g.node(unreal.MaterialExpressionVertexNormalWS, -700), ''),
                                    ('Trod', trod, ''), ('Rock', rock, ''), ('Far', far, ''), ('WindYaw', wind, ''),
                                    ('Albedo', g.const3(albedo, -500), ''),
                                    ('Mottle', g.node(unreal.MaterialExpressionConstant, -500, r=extra.get('mottle', 0.25)), ''),
                                    ('RipTex', g.node(unreal.MaterialExpressionConstant, -500, r=extra.get('ripple_tex', 1.0)), ''),
                                    ('GravD', tobj(grav['diff'], 'color'), ''), ('GravN', tobj(grav['nor'], 'normal'), ''),
                                    ('GravM', tobj(grav['arm'], 'masks'), ''), ('Beach', tobj(beach['diff'], 'color'), ''),
                                    ('BeachN', tobj(beach['nor'], 'normal'), ''), ('SandM', tobj(s['arm'], 'masks'), ''),
                                    ('Ripples', tobj(ripples_texture(), 'masks'), '')]
                     + ([('RipLitT', tobj(riplit_texture(), 'gray'), '')] if v3 else []),
                     F3, -150, extra=[('Nrm', F3), ('Rough', F1), ('AO', F1)] + ([('RipLit', F1)] if v3 else []))
        disp = g.sample(s['disp'], uv(tile_m), 'masks')
        out = {'name': lname, 'base': (c, ''), 'nor': (c, 'Nrm'), 'rough': (c, 'Rough'), 'ao': (c, 'AO'), 'disp': (disp, 'R')}
        if v3:
            out['riplit'] = (c, 'RipLit')
        return out

    look_asph = look_asphalt_code() if (not live and biome == 'desert') else None
    look_asph2 = look_asphalt_v2_code() if (not live and biome == 'desert') else None

    def asphalt_v2_layer(lname, s, tile_m, tint, extra):
        """O asfalto de antes (asphalt_layer) com os defeitos do Look por cima (ASPHALT_V2_CODE). A rua vem da
        textura RoadDir (RoadDirPixels): a direção, a distância com sinal até o eixo e a meia largura."""
        L = asphalt_layer(lname, s, tile_m, tint, extra)
        look_tex = '/Game/Slice/Look'
        cracks_d = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Cracks.png'), look_tex, 'masks')
        cracks_n = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Cracks_N.png'), look_tex, 'normal')
        # o atlas dos carimbos de óleo (o óleo v4 do Look, 13/10: Game/Tools/look_oil.py; linear, máscara)
        oil_atlas = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Oil.png'), look_tex, 'masks')
        def tobj(tex, st):
            return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                          sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                                        'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                                        'masks': unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}[st])
        rdt = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, parameter_name='RoadDir',
                     texture=unreal.load_asset('/Game/Textures/T_WaterN'))
        rsize = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='InfoSize', default_value=1024.0)
        road = g.custom('float4 t = T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0);\n'
                        'float2 e = t.rg * 2.0 - 1.0;\n'
                        'float a = 0.5 * atan2(e.y, e.x) + 0.785398;\n'
                        'Across = (t.b - 0.5) * 20.0;\n'
                        'Width = t.a * 25.5;\n'
                        'Edge = 0.5 * Width - abs(Across);\n'
                        'return float2(cos(a), sin(a));',
                        [('T', rdt, ''), ('S', rsize, ''), ('P', wp, '')], F2, -500,
                        extra=[('Across', F1), ('Width', F1), ('Edge', F1)])
        wv2 = g.node(unreal.MaterialExpressionCollectionParameter, -800)
        wv2.set_editor_property('collection', unreal.load_asset('/Game/Materials/MPC_Wind'))
        wv2.set_editor_property('parameter_name', 'Wind')
        wdir = g.custom('return normalize(W.xy + float2(1e-5, 0.0));', [('W', wv2, '')], F2, -600)
        c = g.custom(look_asph2, [('WP', wp, ''), ('Depth', g.node(unreal.MaterialExpressionPixelDepth, -700), ''),
                                  ('RD', road, ''), ('Across', road, 'Across'), ('Width', road, 'Width'), ('Edge', road, 'Edge'),
                                  ('Base', L['base'][0], L['base'][1]), ('Rough', L['rough'][0], L['rough'][1]),
                                  ('N', L['nor'][0], L['nor'][1]), ('WindDir', wdir, ''),
                                  ('Beach', tobj(S['aerial_beach_01']['diff'], 'color'), ''),
                                  ('Cracks', tobj(cracks_d, 'masks'), ''), ('CracksN', tobj(cracks_n, 'normal'), ''),
                                  ('OilAtlas', tobj(oil_atlas, 'masks'), ''),
                                  # a chance da poça por célula de 14 m (o óleo v4b do Look: 0,07; com 1 toda mancha vira poça,
                                  # pra foto, OILPUDDLE=1: o parâmetro em tempo de jogo não pega no chão Nanite)
                                  ('OilPuddle', g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='OilPuddle',
                                                       default_value=1.0 if os.environ.get('OILPUDDLE', '0') == '1' else 0.07), '')],
                     F3, -150, extra=[('Nout', F3), ('Rout', F1)])
        L['base'] = (c, '')
        L['nor'] = (c, 'Nout')
        L['rough'] = (c, 'Rout')
        return L

    def asphalt_look_layer(lname, s, tile_m, tint, extra, sand_base):
        """O asfalto da volta 15 do Look (Game/Tools/look_asphalt.py): a película de areia, as rachaduras grossas
        com areia dentro, os remendos, o óleo e as faixas de pneu ao longo da pista (a direção vem da textura
        RoadDir, RoadDirPixels). A altura pra mistura das camadas fica a da foto do asfalto."""
        L = layer(lname, s, tile_m, tint, {'far': extra['far']} if 'far' in extra else None)
        surf = lambda sid: S[sid]
        look_tex = '/Game/Slice/Look'
        cracks_d = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Cracks.png'), look_tex, 'masks')
        cracks_n = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Cracks_N.png'), look_tex, 'normal')
        def tobj(tex, st):
            return g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                          sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                                        'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                                        'masks': unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}[st])
        rdt = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, parameter_name='RoadDir',
                     texture=unreal.load_asset('/Game/Textures/T_WaterN'))
        rsize = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='InfoSize', default_value=1024.0)
        # o ângulo dobrado e girado de 45° (RoadDirPixels) de volta à direção
        rd = g.custom('float2 e = T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0).rg * 2.0 - 1.0;\n'
                      'float a = 0.5 * atan2(e.y, e.x) + 0.785398;\n'
                      'return float2(cos(a), sin(a));',
                      [('T', rdt, ''), ('S', rsize, ''), ('P', wp, '')], F2, -500)
        rd_ = surf('road_damaged_2')
        c = g.custom(look_asph, [('WP', wp, ''), ('Depth', g.node(unreal.MaterialExpressionPixelDepth, -700), ''),
                                 ('Sand', sand_base[0], sand_base[1]), ('RD', rd, ''),
                                 ('AG', g.const3((0.216, 0.396, 0.648), -500), ''),
                                 ('AsphD', tobj(rd_['diff'], 'color'), ''), ('AsphN', tobj(rd_['nor'], 'normal'), ''),
                                 ('AsphM', tobj(rd_['arm'], 'masks'), ''),
                                 ('AerAsph', tobj(S['aerial_asphalt_01']['diff'], 'color'), ''),
                                 ('Beach', tobj(S['aerial_beach_01']['diff'], 'color'), ''),
                                 ('Cracks', tobj(cracks_d, 'masks'), ''), ('CracksN', tobj(cracks_n, 'normal'), '')],
                     F3, -150, extra=[('Nxy', F2), ('Rough', F1), ('AO', F1)])
        # a normal: a perturbação no plano somada à de cima (o Look: vn + float3(Nxy, 0)·0,8)
        nrm = g.custom('return normalize(float3(N * 0.8, 1.0));', [('N', c, 'Nxy')], F3, -100)
        L['base'] = (c, '')
        L['nor'] = (nrm, '')
        L['rough'] = (c, 'Rough')
        L['ao'] = (c, 'AO')
        return L

    def asphalt_layer(lname, s, tile_m, tint, extra):
        dust = extra['dust']
        L = layer(lname, s, tile_m, tint, {'far': extra['far']} if 'far' in extra else None)
        # Rachaduras (máscara gerada) em 10 e 23 m: no ladrilho de 6 m a trinca
        # tinha ~1 cm e sumia da câmera de RTS (um pixel ≈ 3–4 cm a 40 m). Terra
        # dentro da trinca, remendos só na escala larga (na de 6 m repetiam como
        # retângulos) e areia soprada em manchas, como na arte conceitual.
        ca = g.sample(cracks['diff'], uv(10.0), 'color')
        cb = g.sample(cracks['diff'], uv(23.0, 0.43), 'color')
        noise = g.sample(S['aerial_beach_01']['diff'], uv(14.0, 0.71), 'color')
        c = g.custom('\n'.join([
            'float crack = saturate(max(A.r, B.r));',
            'float3 col = Base * (1.0 - 0.8 * crack);',
            'col = lerp(col, Dust * 0.9, saturate(A.g * 0.6));',              # terra dentro da rachadura
            'col *= lerp(1.0, 0.8, B.b);',                                     # remendo de asfalto novo
            'float d = smoothstep(0.45, 0.7, N.r);',
            'col = lerp(col, Dust * 1.25, 0.55 * d);',                         # areia soprada
            'Rough = saturate(R + 0.12 * crack + 0.15 * d);',
            'return col;']),
            [('A', ca, 'RGB'), ('B', cb, 'RGB'), ('N', noise, 'RGB'), ('Base', L['base'][0], ''),
             ('Dust', g.const3(dust, -500), ''), ('R', L['rough'][0], L['rough'][1])], F3, -150, extra=[('Rough', F1)])
        L['base'] = (c, '')
        L['rough'] = (c, 'Rough')
        return L

    # a memória do chão do jogo (o plugin RTSLook), só no Landscape do deserto (a vista ao vivo não tem cadernos)
    # (carga direta: o does_asset_exist dava falso no commandlet, o registro ainda não varreu o conteúdo do
    # plugin, e a MF ficava de fora calada; o jogo achou: o UseCrater não existia no material, 13/10)
    # A conta da memória do chão vem do Game/Tools/look_mf.py (o CODE dele, lido na hora) montada AQUI no material,
    # não pela chamada da MF_LookGroundMemory: chamada pela função, o material compilado no commandlet perdia a ligação
    # das entradas ao reabrir ("Missing function input 'BaseColor'", o chão do jogo em xadrez, 13/10). Os parâmetros
    # têm os nomes de lá (o jogo põe por SetLandscapeMaterial*ParameterValue). GROUND_MEMORY=0 monta sem ela.
    ground_memory = None
    if not live and biome == 'desert' and os.environ.get('GROUND_MEMORY', '1') != '0':
        ground_memory = look_ground_memory_code_v3() if GROUND_V3 else look_ground_memory_code()
        log('ground memory', 'ok (inline)' if ground_memory else 'MISSING Game/Tools/look_mf.py')
    live_weights = None
    if live:
        placeholder = unreal.load_asset('/Game/Textures/T_WaterN')
        wa = g.node(unreal.MaterialExpressionTextureObjectParameter, 300, 3000, parameter_name='WeightsA', texture=placeholder)
        wb = g.node(unreal.MaterialExpressionTextureObjectParameter, 300, 3100, parameter_name='WeightsB', texture=placeholder)
        wsize = g.node(unreal.MaterialExpressionScalarParameter, 300, 3200, parameter_name='InfoSize', default_value=1024.0)
        live_weights = g.custom('\n'.join([
            'float2 uv = P.xy / (S * 100.0) + 0.5;',
            'WB = B.SampleLevel(BSampler, uv, 0);',
            'return A.SampleLevel(ASampler, uv, 0);']),
            [('A', wa, ''), ('B', wb, ''), ('S', wsize, ''), ('P', wp, '')], F4, 400, extra=[('WB', F4)])
    layers = []
    for kind, lname, sid, tile, tint, extra in specs:
        if kind == 'rock':
            layers.append(rock_layer(lname, S[sid], tile, tint, extra))
        elif kind == 'sand_v1':
            layers.append(sand_v1_layer(lname, S[sid], tile, tint, extra or {}))
        elif kind == 'sand_v2':
            layers.append(sand_v2_layer(lname, S[sid], tile, tint, extra or {}))
        elif kind == 'asphalt':
            if look_asph2:
                layers.append(asphalt_v2_layer(lname, S[sid], tile, tint, extra))
                log('asphalt', 'Look v2 em zonas sobre o de antes (inline)')
            elif look_asph and layers:
                layers.append(asphalt_look_layer(lname, S[sid], tile, tint, extra, layers[0]['base']))
                log('asphalt', 'Look volta 15 (inline)')
            else:
                layers.append(asphalt_layer(lname, S[sid], tile, tint, extra))
        else:
            layers.append(layer(lname, S[sid], tile, tint, extra))
    blends = {}
    if live:
        weights = live_weights
        n = len(layers)
        for channel, prop, y in (('base', unreal.MaterialProperty.MP_BASE_COLOR, 0),
                                 ('nor', unreal.MaterialProperty.MP_NORMAL, 600),
                                 ('rough', unreal.MaterialProperty.MP_ROUGHNESS, 1200),
                                 ('ao', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 1800)):
            vec = channel in ('base', 'nor')
            code = ['float w[6] = { W.r, W.g, W.b, W.a, V.r, V.g };',
                    'float sum = 0.0;',
                    ('float3' if vec else 'float') + ' acc = 0;']
            for i in range(n):
                code.append('{ float m = clamp(lerp(-1.0, 1.0, w[%d]) + H%d, 0.0001, 1.0); acc += L%d * m; sum += m; }' % (i, i, i))
            code.append('return acc / sum;')
            inputs = [('W', weights, ''), ('V', weights, 'WB')]
            for i, L in enumerate(layers):
                inputs.append(('L%d' % i, L[channel][0], L[channel][1]))
                inputs.append(('H%d' % i, L['disp'][0], L['disp'][1]))
            blend = g.custom('\n'.join(code), inputs, F3 if vec else F1, 600)
            if channel in ('base', 'rough'):
                blends[channel] = blend
            else:
                g.out(blend, '', prop)
    if slab:
        # a estrada com volume (14/10, v3: a laje, UWorldForgeEditorLib::BuildRoadSlabs): o mesmo asfalto v2 aprovado do
        # chão, sem a mistura das camadas (a malha é só asfalto). A face da quebra (a saia da borda, normal de lado)
        # escurece, como no kit do Look ("a face da quebra escura"); depois a areia molhada e a memória do chão por cima,
        # iguais às do Landscape (a cratera e a esteira não somem em cima da estrada)
        # (16/10) a areia por cima da laje: a cor do vértice R (BuildRoadSlabs) mistura a camada de areia do próprio chão
        # (a mesma conta e as mesmas texturas pelo mundo: a emenda com o Landscape não aparece)
        L = next(x for x in layers if x['name'] == 'Asphalt')
        Sd = next((x for x in layers if x['name'] == 'Sand'), None)
        # (17/10, o Felipe: "ta aprovado") a faixa v2 do Look pintada na laje é o padrão: só existe onde há asfalto e sai
        # gasta (o decalque passava por cima da areia onde a laje quebrou, o item 9); PAINT_V2=0 volta o decalque
        paint_code = look_paint_v2_code() if os.environ.get('PAINT_V2', '1') == '1' else None
        if paint_code:
            # (16/10) a faixa pintada v2 do Look: a posição na pista do T_RoadFrame (16 bits; o RoadDir de 8 bits não dava a
            # faixa de 15 cm); a cor da areia do mapa pro encardido; a trinca do asfalto v2 cortando a faixa
            rft = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, 4200, parameter_name='RoadFrame',
                         texture=unreal.load_asset('/Game/Textures/T_WaterN'))
            rfs = g.node(unreal.MaterialExpressionScalarParameter, -900, 4300, parameter_name='InfoSize', default_value=1024.0)
            rf = g.custom('return T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0);', [('T', rft, ''), ('S', rfs, ''), ('P', wp, '')], F4, -700)
            crk = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Cracks.png'), '/Game/Slice/Look', 'masks')
            crk_obj = g.node(unreal.MaterialExpressionTextureObject, -900, 4400, texture=crk,
                             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
            dust = Sd['base'] if Sd else (g.const3((0.49, 0.40, 0.27), -900), '')
            pc = g.custom(paint_code, [('WP', wp, ''), ('RF', rf, ''), ('Base', L['base'][0], L['base'][1]), ('Rough', L['rough'][0], L['rough'][1]),
                                       ('N', L['nor'][0], L['nor'][1]), ('Dust', dust[0], dust[1]), ('Cracks', crk_obj, '')],
                          F3, 300, extra=[('Nout', F3), ('Rout', F1)])
            L = dict(L)
            L['base'], L['nor'], L['rough'] = (pc, ''), (pc, 'Nout'), (pc, 'Rout')
            global SLAB_PAINT
            SLAB_PAINT = True
            log('road slab paint', 'PAINT_V2 do Look na laje')
        vn = g.node(unreal.MaterialExpressionVertexNormalWS, 400, 0)
        vc = g.node(unreal.MaterialExpressionVertexColor, 400, 100)
        # (16/10, o Felipe: "a gente 'suja' a estrada usando a textura que tá ao lado dela") a sujeira por cima da laje é o
        # chão do lado (UWorldForgeEditorLib::BuildRoadSlabs, a cor do vértice: R a película, G/B/A o cascalho, a terra e o
        # sal; a areia é o resto), misturado pela altura como no Landscape, com as mesmas texturas pelo mundo (a emenda com o
        # chão não aparece). A película entra primeiro nas trincas do asfalto (o asfalto baixo da altura dele)
        dl = [next((x for x in layers if x['name'] == nm), None) for nm in ('Sand', 'Gravel', 'Dirt', 'Salt')]
        dl = [x if x is not None else (Sd or L) for x in dl]
        dirt = {}
        for ch, typ in (('base', F3), ('nor', F3), ('rough', F1), ('ao', F1)):
            # (a 1ª: VC.a pela saída RGB do nó, o shader não compilava e a laje saía em xadrez no jogo: o A vem à parte)
            code = ['float w[4] = { saturate(1.0 - VC.g - VC.b - VA), VC.g, VC.b, VA };', 'float sum = 0.0;',
                    ('float3' if typ == F3 else 'float') + ' acc = 0;']
            ins = [('VC', vc, ''), ('VA', vc, 'A')]
            for i, x in enumerate(dl):
                code.append('{ float m = clamp(lerp(-1.0, 1.0, w[%d]) + H%d, 0.0001, 1.0) * step(0.004, w[%d]); acc += L%d * m; sum += m; }' % (i, i, i, i))
                ins += [('L%d' % i, x[ch][0], x[ch][1]), ('H%d' % i, x['disp'][0], x['disp'][1])]
            code.append('return acc / max(sum, 0.0001);')
            dirt[ch] = g.custom('\n'.join(code), ins, typ, 450)
        dmask = g.custom('return saturate((VC.r - 0.5) * 2.5 + 0.5 + (0.5 - HA) * 1.2) * step(0.004, VC.r);',
                         [('VC', vc, ''), ('HA', L['disp'][0], L['disp'][1])], F1, 450)
        # (16/10, a comparação com os game shots aprovados, road_town_battle, road_battle e street_battle: neles o asfalto
        # inteiro tem um véu de poeira em manchas, a beira tem sempre uma linha fina de poeira (a forma mais comum da areia
        # na estrada, pela pesquisa) e as trincas têm areia dentro) o véu: manchas largas (~10 m) fracas, a faixa de ~1,5 m
        # da beira e a linha de ~40 cm na beira; a cor é a da sujeira (o chão do lado, a cor do
        # vértice); por baixo da película dos trechos. O UV 1 traz a distância à beira (BuildRoadSlabs)
        # (a distância à beira pela textura RoadDir do mapa, a mesma conta do asfalto v2 do Look: o UV 1 da malha não chegava
        # ao material no jogo, a 1ª prova saiu sem a faixa da beira)
        rdt_s = g.node(unreal.MaterialExpressionTextureObjectParameter, 300, 3500, parameter_name='RoadDir',
                       texture=unreal.load_asset('/Game/Textures/T_WaterN'))
        rsize_s = g.node(unreal.MaterialExpressionScalarParameter, 300, 3600, parameter_name='InfoSize', default_value=1024.0)
        redge = g.custom('float4 t = T.SampleLevel(TSampler, P.xy / (S * 100.0) + 0.5, 0);\n'
                         'return 0.5 * t.a * 25.5 - abs((t.b - 0.5) * 20.0);',
                         [('T', rdt_s, ''), ('S', rsize_s, ''), ('P', g.node(unreal.MaterialExpressionWorldPosition, 300, 3700), '')], F1, 400)
        vnoise = g.sample(S['aerial_beach_01']['diff'], uv(11.0, 0.37), 'color')
        veil = g.custom('\n'.join([
            'float dd = max(Ed, 0.0);',
            'float edge = 1.0 - smoothstep(0.0, 1.6, dd);',
            'float edl = 1.0 - smoothstep(0.05, 0.45, dd);',   # (não 'line': palavra reservada do HLSL)
            'float patches = smoothstep(0.4, 0.75, Nz.r);',
            'float v = 0.20 * patches + 0.30 * edge + 0.35 * edl;',
            'v *= 0.7 + 0.3 * Nz.g;',
            # (16/10, a 2ª prova: as trincas cheias pela altura do asfalto viraram bolinhas de areia repetidas a cada ~1 m,
            # o desenho do ladrilho da altura; nos game shots as trincas são escuras, sem areia: saiu)
            'return v;']), [('Ed', redge, ''), ('Nz', vnoise, 'RGB')], F1, 450)
        # (17/10, o Felipe: "já tá aprovado") o véu é o padrão; ROAD_VEIL=0 volta a sujeira de 16/10 sem ele
        if os.environ.get('ROAD_VEIL', '1') != '1':
            veil = g.node(unreal.MaterialExpressionConstant, 300, 3800, r=0.0)
        blends['base'] = g.custom('float3 a = B * lerp(0.45, 1.0, smoothstep(0.35, 0.85, VN.z)); a = lerp(a, SB * 0.92, 0.55 * Vl); return lerp(a, SB, M);',
                                  [('B', L['base'][0], L['base'][1]), ('VN', vn, ''), ('SB', dirt['base'], ''), ('M', dmask, ''), ('Vl', veil, '')], F3, 500)
        blends['nor'] = g.custom('return normalize(lerp(N, SN, M));',
                                 [('N', L['nor'][0], L['nor'][1]), ('SN', dirt['nor'], ''), ('M', dmask, '')], F3, 500)
        blends['rough'] = g.custom('return lerp(lerp(R, SR, 0.5 * Vl), SR, M);',
                                   [('R', L['rough'][0], L['rough'][1]), ('SR', dirt['rough'], ''), ('M', dmask, ''), ('Vl', veil, '')], F1, 500)
        ao = g.custom('return lerp(A, SA, M);', [('A', L['ao'][0], L['ao'][1]), ('SA', dirt['ao'], ''), ('M', dmask, '')], F1, 500)
        if GROUND_V3 and ground_memory is not None:
            blends['ao'] = ao
            blends['riplit'] = g.node(unreal.MaterialExpressionConstant, 400, 2100, r=1.0)
        else:
            g.out(ao, '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        if ground_memory is None:
            g.out(blends['nor'], '', unreal.MaterialProperty.MP_NORMAL)
    v3_mem = GROUND_V3 and not live and ground_memory is not None
    if v3_mem:
        one = g.node(unreal.MaterialExpressionConstant, 400, 2100, r=1.0)
        for L in layers:
            L.setdefault('riplit', (one, ''))
    for channel, prop, y in (() if (live or slab) else (('base', unreal.MaterialProperty.MP_BASE_COLOR, 0),
                             ('nor', unreal.MaterialProperty.MP_NORMAL, 600),
                             ('rough', unreal.MaterialProperty.MP_ROUGHNESS, 1200),
                             ('ao', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 1800))
                             + ((('riplit', None, 2100),) if v3_mem else ())):
        blend = g.node(unreal.MaterialExpressionLandscapeLayerBlend, 600, y)
        entries = []
        for i, L in enumerate(layers):
            e = unreal.LayerBlendInput()
            e.set_editor_property('layer_name', L['name'])
            e.set_editor_property('blend_type', unreal.LandscapeLayerBlendType.LB_HEIGHT_BLEND)
            e.set_editor_property('preview_weight', 1.0 if i == 0 else 0.0)
            entries.append(e)
        blend.set_editor_property('layers', entries)
        for L in layers:
            g.link(L[channel][0], L[channel][1], blend, 'Layer ' + L['name'])
            g.link(L['disp'][0], L['disp'][1], blend, 'Height ' + L['name'])
        if channel in ('base', 'rough', 'riplit') or (channel == 'nor' and ground_memory is not None) or (channel == 'ao' and v3_mem):
            blends[channel] = blend
        else:
            g.out(blend, '', prop)
    # chão molhado: a faixa de ~3 m acima da linha d'água e o fundo debaixo dela
    # ficam mais escuros e mais lisos (a areia molhada da praia). A distância
    # vem da textura da água do mapa (WaterInfo, MakeWaterInfoTexture); WaterOn
    # = 0 no material base, 1 na instância de cada mapa com água
    info_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, 300, 2400, parameter_name='WaterInfo',
                      texture=unreal.load_asset('/Game/Textures/T_WaterN'))
    info_size = g.node(unreal.MaterialExpressionScalarParameter, 300, 2500, parameter_name='InfoSize', default_value=1024.0)
    water_on = g.node(unreal.MaterialExpressionScalarParameter, 300, 2600, parameter_name='WaterOn', default_value=0.0)
    # O A da textura é o tom do chão longe d'água (128 = neutro). Abaixo, úmido
    # (a lama da borda da salina): chão molhado escurece ~40% e fica mais liso
    # (Lekner & Dorf 1988; Lagarde, "Water drop 2b", 2013). Acima, leito lavado
    # (o fundo dos leitos secos): ~45% mais claro e menos saturado (o leito ativo,
    # sem verniz do deserto, tem albedo ~1,3–1,5× o do chão velho; a 25% o leito
    # sumia na vista geral, 07/10)
    wet = g.custom('\n'.join([
        'float2 uv = P.xy / (S * 100.0) + 0.5;',
        'float4 I = Tex.SampleLevel(TexSampler, uv, 0);',
        'float d = (I.r - 0.5) * 64.0;',
        'float t = I.a * 2.0 - 1.0;',
        'float wash = On * saturate(t - 0.01);',
        'float w = On * max(1.0 - smoothstep(0.0, 3.0, -d), 0.75 * saturate(-t - 0.01));',
        'Rough = lerp(R, R * 0.45, w);',
        'float3 c = lerp(B, B * 0.6, w) * (1.0 + 0.45 * wash);',
        # o oásis verde (13/10, o capim do oásis: o verde vem do chão, com moitas por cima; OASIS2=1): a terra úmida
        # verde-oliva de 2 a 16 m da água, em manchas largas (a foto aérea de praia como ruído)
        'float n = N.SampleLevel(NSampler, P.xy * 0.01 / 9.0 + 0.4, 0).g;',
        'float gr = On * Oa * saturate(-d) * (1.0 - smoothstep(2.0, 16.0, -d)) * lerp(0.45, 1.0, smoothstep(0.215, 0.245, n));',
        'c *= lerp(float3(1.0, 1.0, 1.0), float3(0.62, 0.74, 0.44), gr);',
        'Rough = lerp(Rough, Rough * 0.85, gr);',
        'return lerp(c, dot(c, float3(0.3, 0.59, 0.11)).xxx, 0.3 * wash);']) if biome != 'forest' else '\n'.join([
        # floresta: o tom é a umidade do capim: acima, seco (palha: mais amarelo e
        # claro); abaixo, viçoso (verde mais escuro e frio). A beira d'água continua
        # o chão molhado
        'float2 uv = P.xy / (S * 100.0) + 0.5;',
        'float4 I = Tex.SampleLevel(TexSampler, uv, 0);',
        'float d = (I.r - 0.5) * 64.0;',
        'float t = I.a * 2.0 - 1.0;',
        'float dry = On * saturate(t - 0.01);',
        'float lush = On * saturate(-t - 0.01);',
        'float w = On * (1.0 - smoothstep(0.0, 3.0, -d));',
        'Rough = lerp(R, R * 0.45, w);',
        'float3 c = lerp(B, B * 0.6, w);',
        # (mais forte: com 1,22/0,72 as manchas secas sumiam de cima, rev11 de 07/10)
        'c *= lerp(float3(1.0, 1.0, 1.0), float3(1.38, 1.14, 0.5), dry);',
        'return c * lerp(float3(1.0, 1.0, 1.0), float3(0.66, 0.84, 0.72), lush);']),
        [('Tex', info_tex, ''), ('S', info_size, ''), ('On', water_on, ''), ('P', wp, ''),
         ('B', blends['base'], ''), ('R', blends['rough'], '')]
        + ([('Oa', g.node(unreal.MaterialExpressionConstant, 300, 2700, r=1.0 if os.environ.get('OASIS2', '1') == '1' else 0.0), ''),
            ('N', g.node(unreal.MaterialExpressionTextureObject, 300, 2750, texture=S['aerial_beach_01']['diff'],
                         sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR), '')] if biome != 'forest' else []),
        F3, 800, extra=[('Rough', F1)])
    if live:
        # as camadas de depuração do app (passagem, áreas) por cima, como no M_Terrain
        ov = g.node(unreal.MaterialExpressionTextureObjectParameter, 900, 2800, parameter_name='OverlayMap', texture=unreal.load_asset('/Game/Textures/T_WaterN'))
        ov_on = g.node(unreal.MaterialExpressionScalarParameter, 900, 2900, parameter_name='OverlayOpacity', default_value=0.0)
        ov_size = g.node(unreal.MaterialExpressionScalarParameter, 900, 3000, parameter_name='MapSize', default_value=1024.0)
        final = g.custom('\n'.join([
            'float4 o = O.SampleLevel(OSampler, P.xy / (S * 100.0) + 0.5, 0);',
            'return lerp(C, o.rgb * 0.6, o.a * On);']),
            [('O', ov, ''), ('On', ov_on, ''), ('S', ov_size, ''), ('P', wp, ''), ('C', wet, '')], F3, 1100)
        g.out(final, '', unreal.MaterialProperty.MP_BASE_COLOR)
    elif ground_memory is not None:
        # a memória do chão do jogo (a conta do Look, look_mf.py): a esteira afundada, a cratera e o avental, pelos
        # cadernos que o jogo entrega ao Landscape (ReliefRangeCm = 0 desliga e devolve a entrada). Depois de tudo
        # misturado (cor linear, normal tangente, rugosidade); a brasa no emissivo. A RippleMask fica de fora por
        # enquanto (a esteira passa por cima da ondulação; de longe não erra)
        black = unreal.load_asset('/Engine/EngineResources/Black')
        ins = [('WP', wp, ''), ('BC', wet, ''), ('N', blends['nor'], ''), ('Rg', wet, 'Rough'),
               # o chão de cada pixel pra cratera v2 (13/10, o Look): 1 = asfalto (o peso da camada, já com a beira de
               # 1,2 m e os buracos), 0 = areia/terra (a estrada de terra pinta a Dirt; a Salt também fica 0)
               ('Surf', g.node(unreal.MaterialExpressionConstant, 1000, r=1.0) if slab else
                g.node(unreal.MaterialExpressionLandscapeLayerSample, 1000, parameter_name='Asphalt', preview_weight=0.0), '')]
        # (as texturas que o CODE lê; GroundTrackMid entrou em 13/10: o queimado no caderno do meio, que a câmera
        # do RTS vê; antes ele sumia com o pixel de 10–20 cm)
        mem_nodes = {}
        for pname, pin in (('GroundRelief', 'Relief'), ('GroundReliefMid', 'ReliefMid'), ('GroundTrack', 'Track'),
                           ('GroundTrackMid', 'TrackMid'), ('GroundCrater', 'Crater'), ('GroundCraterMid', 'CraterMid')):
            t = g.node(unreal.MaterialExpressionTextureObjectParameter, 1000, parameter_name=pname,
                       sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                       sampler_source=unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
            if black:
                t.set_editor_property('texture', black)
            ins.append((pin, t, ''))
            mem_nodes[pin] = t
        for pname, pin in (('DetailBounds', 'DB'), ('MidBounds', 'MB')):
            v = g.node(unreal.MaterialExpressionVectorParameter, 1000, parameter_name=pname, default_value=unreal.LinearColor(0, 0, 0, 0))
            ins.append((pin, v, 'RGBA'))   # (a saída '' é só o RGB: o .zw não compilava no SM6)
            mem_nodes[pin] = v
        for pname, pin, dv in (('ReliefRangeCm', 'Range', 0.0), ('ReliefTexel', 'Texel', 1.0 / 2048.0), ('UseCrater', 'UseCrater', 0.0),
                               ('HeatNits', 'HeatNits', 3000.0),   # (o padrão do Look, 13/10; o jogo manda 3000)
                               ('MarkRamps', 'MarkRamps', 0.0),   # (13/10: as marcas de cratera viram rampas quando o jogo ligar; 0 = como antes)
                               ('UseRubber', 'UseRubber', 0.0)):   # (16/10, o Look: o B do relevo vira a borracha da esteira no asfalto quando o jogo ligar; 0 = a sombra de antes)
            sp = g.node(unreal.MaterialExpressionScalarParameter, 1000, parameter_name=pname, default_value=dv)
            ins.append((pin, sp, ''))
            mem_nodes[pin] = sp
        if GROUND_V3:
            # a v3 (16/10): os cadernos de luz do jogo (GroundSunLit: R a luz até 1,2, G o SkyAO; -RTSGroundV3 liga o
            # UseSunLit), a RipLit da areia e o sol em espaço tangente (a direção da luz da atmosfera, mundo → tangente)
            for pname, pin in (('GroundSunLit', 'SunLitT'), ('GroundSunLitMid', 'SunLitMidT')):
                t = g.node(unreal.MaterialExpressionTextureObjectParameter, 1000, parameter_name=pname,
                           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                           sampler_source=unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
                if black:
                    t.set_editor_property('texture', black)
                ins.append((pin, t, ''))
            ins.append(('UseSunLit', g.node(unreal.MaterialExpressionScalarParameter, 1000, parameter_name='UseSunLit', default_value=0.0), ''))
            ins.append(('RipLit', blends['riplit'], ''))
            ld = g.node(unreal.MaterialExpressionSkyAtmosphereLightDirection, 900, 3500)
            lt = g.node(unreal.MaterialExpressionTransform, 1100, 3500,
                        transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                        transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
            g.link(ld, '', lt, '')
            ins.append(('Lt', lt, ''))
            ins.append(('V3On', g.node(unreal.MaterialExpressionConstant, 1000, r=1.0), ''))
        mem = g.custom(ground_memory, ins, F3, 1300, extra=[('Nout', F3), ('Rout', F1), ('Mask', F1), ('Emi', F3)]
                       + ([('AOout', F1)] if GROUND_V3 else []))
        if GROUND_V3:
            g.out(g.custom('return A * B;', [('A', blends['ao'], ''), ('B', mem, 'AOout')], F1, 1500), '',
                  unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
            log('ground v3', 'RippleLit', os.environ.get('RIPLIT', 'patch'), 'e a normal de iluminacao (code_v3)')
        # o deslocamento Nanite da cratera (13/10, o Look e o jogo): o DISP_CODE do look_mf.py no pino Displacement,
        # magnitude 200 cm e centro 0,5 (sem multiplicar no grafo: UE-370671, o terreno fantasma). DispOn = 0 (padrão)
        # devolve 0,5: nada mexe até o jogo ligar
        disp_code = look_disp_code() if not slab else None   # (a laje não tessela: 12 cm de malha de verdade)
        if disp_code:
            farm = texture(os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'downloads', 'farm_soil', 'farm_soil_disp_2k.jpg'),
                           '/Game/Slice/Look', 'masks')
            farm_obj = g.node(unreal.MaterialExpressionTextureObject, 1000, texture=farm,
                              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
            din = [('WP', wp, ''), ('Relief', mem_nodes['Relief'], ''), ('ReliefMid', mem_nodes['ReliefMid'], ''),
                   ('Crater', mem_nodes['Crater'], ''), ('CraterMid', mem_nodes['CraterMid'], ''), ('FarmH', farm_obj, ''),
                   ('DB', mem_nodes['DB'], 'RGBA'), ('MB', mem_nodes['MB'], 'RGBA'), ('Range', mem_nodes['Range'], ''),
                   ('UseCrater', mem_nodes['UseCrater'], ''),
                   ('DispOn', g.node(unreal.MaterialExpressionScalarParameter, 1000, parameter_name='DispOn', default_value=0.0), '')]
            dnode = g.custom(disp_code, din, F1, 1500)
            # (13/10, os buracos no Landscape com a tesselação: os testes do Look e do jogo, uma variação por vez no mapa
            # de teste. DISPTEST=const liga uma constante 0,5 no lugar do código; DISPMAG muda a magnitude; o fade do
            # deslocamento fica desligado: com ele o Landscape liga a reserva de raster por cluster, LandscapeRender.cpp)
            if os.environ.get('DISPTEST', '') == 'const':
                dnode = g.node(unreal.MaterialExpressionConstant, 1500, r=0.5)
            g.m.set_editor_property('enable_tessellation', True)
            # DISPFADE="início,fim" (px do deslocamento máximo na tela, 13/10, o Look): o fade de distância do
            # deslocamento; além do fim o cluster vai pra reserva de raster e não tessela (NaniteCullingCommon.ush:522),
            # e sobra fila de patches pro perto (o estouro de MaxCandidatePatches no 1080p)
            fade = os.environ.get('DISPFADE', '')
            g.m.set_editor_property('enable_displacement_fade', bool(fade))
            if fade:
                fs, fe = (float(v) for v in fade.split(','))
                g.m.set_editor_property('displacement_fade_range', unreal.DisplacementFadeRange(start_size_pixels=fs, end_size_pixels=fe))
            mag = float(os.environ.get('DISPMAG', '200'))
            g.m.set_editor_property('displacement_scaling', unreal.DisplacementScaling(magnitude=mag, center=0.5))
            log('ground displacement', ('ok' if lib.connect_material_displacement(dnode, '') else 'FALHOU a ligacao')
                + ' (%s, %.0f cm, %s)' % (os.environ.get('DISPTEST', 'codigo'), mag, ('fade ' + fade + ' px') if fade else 'sem fade'))
        g.out(mem, '', unreal.MaterialProperty.MP_BASE_COLOR)
        g.out(mem, 'Nout', unreal.MaterialProperty.MP_NORMAL)
        g.out(mem, 'Rout', unreal.MaterialProperty.MP_ROUGHNESS)
        g.out(mem, 'Emi', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        g.out(wet, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if live or ground_memory is None:
        g.out(wet, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    if not live:
        # o chão é Nanite e usa a instância deste mapa: sem a marca, a instância
        # caía no material padrão (o chão sumia, 07/10)
        mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    return g.finish(name)


# (16/10, o Felipe: "a graminha morta deitada na trilha", "o arbusto amassado"; o estudo em WORLDFORGE.md §12.8) o capim
# e a moita deitam onde os tanques passam: o material lê as marcas da batalha que o jogo grava no chão (GroundBattle,
# RGBA8 a 50 cm, o mapa inteiro; R o tráfego, que satura em ~32 passadas, B o queimado largo; BattleBounds = mín. e
# tamanho em cm), as mesmas que o Landscape e a laje recebem; o jogo liga o UseTrample. A planta gira pela base pro
# lado da trilha (a direção pelo gradiente do R) e amassa; a pisada desbota e o queimado escurece. TRAMPLE=1 liga na
# montagem (o mapa de teste; o do jogo só depois da prova e da palavra dele)
TRAMPLE = os.environ.get('TRAMPLE', '0') == '1'
TRAMPLE_WPO = r"""
if (U < 0.5) return float3(0.0, 0.0, 0.0);
float2 sz = max(B.zw, float2(1.0, 1.0));
float2 uv = (OP.xy - B.xy) / sz;
if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return float3(0.0, 0.0, 0.0);
float r0 = Tex.SampleLevel(TexSampler, uv, 0).r;
// (uma passada de tanque ja deita o capim, como no real: o R sobe ~1/32 por passada; 2 passadas = deitado)
float t = smoothstep(0.004, 0.065, r0);
if (t < 0.001) return float3(0.0, 0.0, 0.0);
float2 du = float2(100.0, 0.0) / sz, dv = float2(0.0, 100.0) / sz;
float2 gr = float2(Tex.SampleLevel(TexSampler, uv + du, 0).r - Tex.SampleLevel(TexSampler, uv - du, 0).r,
                   Tex.SampleLevel(TexSampler, uv + dv, 0).r - Tex.SampleLevel(TexSampler, uv - dv, 0).r);
float ph = Rnd * 6.2832;
// ao longo da trilha (de lado ao gradiente do trafego), cada tufo pro seu lado, com ate ~25 graus de acaso
float2 along = dot(gr, gr) > 1e-8 ? normalize(float2(-gr.y, gr.x)) : float2(cos(ph), sin(ph));
along *= frac(Rnd * 7.13) < 0.5 ? -1.0 : 1.0;
float a = (frac(Rnd * 3.71) - 0.5) * 0.9;
along = float2(along.x * cos(a) - along.y * sin(a), along.x * sin(a) + along.y * cos(a));
float3 d = WP - OP;
float h = max(d.z, 0.0);
float ang = t * 1.35;
float3 nd;
nd.xy = d.xy * (1.0 + 0.3 * t) + along * h * sin(ang);
nd.z = h * cos(ang) * (1.0 - 0.3 * t);
return nd - float3(d.xy, h);
"""
TRAMPLE_COLOR = r"""
if (U > 0.5)
{
    float2 sz = max(B.zw, float2(1.0, 1.0));
    float2 uv = (OP.xy - B.xy) / sz;
    if (uv.x >= 0.0 && uv.y >= 0.0 && uv.x <= 1.0 && uv.y <= 1.0)
    {
        float4 m = Tex.SampleLevel(TexSampler, uv, 0);
        float trod = smoothstep(0.004, 0.065, m.r);
        float burn = smoothstep(0.1, 0.6, m.b);
        base = lerp(base, dot(base, float3(0.3, 0.59, 0.11)) * float3(1.08, 0.97, 0.78), 0.5 * trod);
        base *= lerp(1.0, 0.16, burn);
    }
}
"""


def prop_material(name, plant, defaults):
    """Mestre dos modelos escaneados: cor, normal, ARM, alfa nas plantas e neve
    no que olha pra cima (parâmetro Snow; a máscara usa a normal já com o
    relevo da textura, então a neve assenta nas saliências)."""
    g = Graph(name)
    if plant:
        g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        g.m.set_editor_property('two_sided', True)
        g.m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)

    def param(pname, default, kind):
        return g.node(unreal.MaterialExpressionTextureSampleParameter2D, -800, parameter_name=pname, texture=default,
                      sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                                    'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                                    'masks': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind])
    col = param('BaseColorTex', defaults['color'], 'color')
    nor = param('NormalTex', defaults['normal'], 'normal')
    arm = param('ARMTex', defaults['masks'], 'masks')
    tint = g.node(unreal.MaterialExpressionVectorParameter, -800, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    snow = g.node(unreal.MaterialExpressionScalarParameter, -800, parameter_name='Snow', default_value=0.0)
    desat = g.node(unreal.MaterialExpressionScalarParameter, -800, parameter_name='Desat', default_value=0.0)
    wn = g.node(unreal.MaterialExpressionVertexNormalWS, -800)
    # cota d'água do mapa (cm), por instância (AddInstances): sem neve do nível
    # do mar até ~1 m acima (a onda lava); a pedra submersa com neve virava mancha
    # verde-limão no fundo do mar (07/10). Sem água, o padrão desliga o corte
    wl = g.node(unreal.MaterialExpressionPerInstanceCustomData, -800, data_index=0, const_default_value=-1.0e7)
    wz = g.node(unreal.MaterialExpressionWorldPosition, -800)
    trample = plant and TRAMPLE
    tr_ins = []
    if trample:
        tr_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, parameter_name='GroundBattle',
                        texture=unreal.load_asset('/Engine/EngineResources/Black'))
        tr_b = g.node(unreal.MaterialExpressionVectorParameter, -900, parameter_name='BattleBounds', default_value=unreal.LinearColor(0, 0, 0, 0))
        tr_u = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='UseTrample', default_value=0.0)
        tr_op = g.node(unreal.MaterialExpressionObjectPositionWS, -900)
        tr_ins = [('Tex', tr_tex, ''), ('B', tr_b, 'RGBA'), ('U', tr_u, ''), ('OP', tr_op, '')]
    c = g.custom('\n'.join([
        'float3 base = C * T;',
        'base = lerp(base, dot(base, float3(0.3, 0.59, 0.11)).xxx, Desat);'] + ([TRAMPLE_COLOR] if trample else []) + [
        'float up = saturate((VN.z + (Nt.y * 2.0 - 1.0) * 0.25 - 0.45) * 3.5);',
        'float s = Snow * up * saturate((Wp.z - Wl - 30.0) / 70.0);',
        'Rough = lerp(Ar.g, 0.65, s);',
        'return lerp(base, float3(0.82, 0.85, 0.9), s);']),
        [('C', col, 'RGB'), ('T', tint, ''), ('VN', wn, ''), ('Nt', nor, 'RGB'), ('Snow', snow, ''), ('Ar', arm, 'RGB'), ('Desat', desat, ''),
         ('Wl', wl, ''), ('Wp', wz, '')] + tr_ins,
        F3, -300, extra=[('Rough', F1)])
    if trample:
        rnd = g.node(unreal.MaterialExpressionPerInstanceRandom, -900)
        wpo = g.custom(TRAMPLE_WPO, tr_ins + [('WP', wz, ''), ('Rnd', rnd, '')], F3, -300)
        g.out(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        log('trample', name, 'o capim e a moita deitam na trilha (UseTrample)')
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if plant:
        # a luz do tufo pelo chão (o jeito dos jogos grandes com capim visto de longe: a normal da folha puxada
        # pra cima, a do terreno; Sousa/Crytek "Vegetation Procedural Animation and Shading", GPU Gems 3 cap. 16,
        # e o capim do Ghost of Tsushima, GDC 2021). Com a folha fina contra o sol baixo, cada tufo virava um risco
        # escuro (o Felipe, 13/10). NormalUp 0 = a normal da folha (igual a antes); a normal sai no mundo, e aí a
        # Unreal não inverte a face de trás: o TwoSidedSign faz isso na folha, e o "pra cima" vale pros dois lados
        g.m.set_editor_property('tangent_space_normal', False)
        nup = g.node(unreal.MaterialExpressionScalarParameter, -800, parameter_name='NormalUp', default_value=0.0)
        tw = g.node(unreal.MaterialExpressionTransform, -500)
        tw.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
        tw.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        g.link(nor, 'RGB', tw, '')
        sign = g.node(unreal.MaterialExpressionTwoSidedSign, -500)
        nw = g.custom('return normalize(lerp(N * S, float3(0.0, 0.0, 1.0), U));', [('N', tw, ''), ('S', sign, ''), ('U', nup, '')], F3, -300)
        g.out(nw, '', unreal.MaterialProperty.MP_NORMAL)
    else:
        g.out(nor, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    g.out(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(arm, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if plant:
        alpha = param('AlphaTex', defaults['masks'], 'masks')
        g.out(alpha, 'R', unreal.MaterialProperty.MP_OPACITY_MASK)
        sss = g.mul(c, '', g.const3((0.6, 0.55, 0.3), -300), '', -150)
        g.out(sss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    return g.finish(name)


# ---- árvores do Procedural Vegetation Editor ---------------------------------------
# As amostras que vêm com a Unreal 5.8 (plugin ProceduralVegetationEditor): um
# pinheiro de ~22 m, uma árvore de folha larga de ~11 m e um arbusto, em malha
# esquelética Nanite com os raminhos em "assembly". Viram malha estática
# instanciável (WorldForgeEditorLib::VegetationToStaticMesh).

PVE = '/ProceduralVegetationEditor/SampleAssets/StarterContent/'
TREES = {
    'conifer': PVE + 'ConiferTree_01/PVE_Conifer_01',
    'broadleaf': PVE + 'DeciduousTree_01/PVE_Deciduous_Tree_01',
    'shrub': PVE + 'Deciduous_Shrub_01/PVE_Deciduous_Shrub_01',
}


def tree(key):
    """A árvore como malha estática (convertida uma vez; TREES=1 refaz, TREE_BAKE=1
    assa os raminhos na malha em vez da assembly)."""
    path = DEST + '/Trees/SM_' + key
    if eal.does_asset_exist(path) and os.environ.get('TREES') != '1':
        return unreal.load_asset(path)
    mesh = lib.vegetation_to_static_mesh(unreal.load_asset(TREES[key]), path, os.environ.get('TREE_BAKE') == '1')
    eal.save_loaded_asset(mesh)
    return mesh


def tree_fade(key):
    """Cópia da árvore que nunca vira voxels (forma "área"): o voxel do Nanite só
    desenha com o material padrão, e o recorte perto do cursor não vale nele. O
    jogo troca pra ela só as árvores perto do cursor (ForgeTreeFade). TREE_FADE=1
    refaz só as cópias."""
    path = DEST + '/Trees/SM_' + key + '_Fade'
    if eal.does_asset_exist(path) and os.environ.get('TREES') != '1' and os.environ.get('TREE_FADE') != '1':
        return unreal.load_asset(path)
    mesh = lib.vegetation_to_static_mesh(unreal.load_asset(TREES[key]), path, os.environ.get('TREE_BAKE') == '1', True)
    eal.save_loaded_asset(mesh, False)
    return mesh


_MASTERS = {}
WATER_BIOME = 'forest'   # o bioma da montagem em curso (o material da água)
WATER_INFO = None        # a textura da água do mapa em curso (MakeWaterInfoTexture)
WATER_INFO_SIZE = 1024.0
MAP_NAME = ''            # o nome curto do mapa em curso (assets por mapa)


# O chão da v3 (16/10, o Look: docs/CHAO_V3_ESTUDO.md §4.3, §5.1, §10). GROUNDV3=1 monta o chão com a luz do sol
# assada: a ondulação lê a RippleLit (a luz linear da ondulação pro sol e o vento do mapa, ÷ 1,25) no lugar da normal
# e da sombra velha, e a memória do chão usa o code_v3() do look_mf.py (a normal de iluminação, N'·L = luz assada × N·L;
# o SkyAO no AO). RIPLIT escolhe a textura: patch (a proposta do Look, padrão), phys ou game (o A/B dele).
# (16/10, o Felipe aprovou as trilhas v3 com a areia 3: "um baita avanço, tá aprovado"; o World_TestGroundV3Dune vira a
# base) ligado por padrão no deserto (build: ground_v3_for); GROUNDV3=0|1 no ambiente ainda manda. Neve e floresta
# ficam no chão de antes até o A/B delas
GROUND_V3 = os.environ.get('GROUNDV3', '0') == '1'


def ground_v3_for(sname):
    global GROUND_V3
    env = os.environ.get('GROUNDV3', '')
    GROUND_V3 = env == '1' if env in ('0', '1') else sname == 'desert'
    log('ground v3', sname, GROUND_V3)


def riplit_texture():
    kind = os.environ.get('RIPLIT', 'patch')
    src = os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures_v3', 'T_LL_RippleLit_%s.png' % kind)
    return texture(src, DEST + '/Surfaces/ripples', 'gray')


def ripples_texture():
    """As ondulações simuladas do Look (Game/Tools/look_ripples.py; o PNG fica em captures/look/textures): R altura,
    GB normal, A a sombra do sol assada pra um sol a 155° do vento (o vento do mapa sai daí: FWorldMap::WindYaw)."""
    src = os.path.join(os.path.dirname(PROJECT), 'captures', 'look', 'textures', 'T_LL_Ripples.png')
    return texture(src, DEST + '/Surfaces/ripples', 'masks')


def fade_collection():
    """A coleção que o jogo atualiza com a câmera e o ponto do chão sob o cursor
    (ForgeCameraPawn::UpdateTreeFade): a árvore some em volta do cursor."""
    path = DEST + '/Trees/MPC_TreeFade'
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mpc = asset_tools.create_asset('MPC_TreeFade', DEST + '/Trees', unreal.MaterialParameterCollection,
                                   unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in ('TreeFadeCamera', 'TreeFadeCursor'):
        v = unreal.CollectionVectorParameter()
        v.set_editor_property('parameter_name', name)
        v.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 0))   # raio 0: desligado
        params.append(v)
    mpc.set_editor_property('vector_parameters', params)
    eal.save_loaded_asset(mpc)
    return mpc


def tree_master(snow):
    """Cópia do mestre de folhagem do PVE com a árvore transparente perto do
    cursor (AddOcclusionFadeToMaterial) e, na neve, a neve por cima. Criada uma
    vez e reaproveitada (apagar não dá enquanto um mapa usa)."""
    key = 'snow' if snow else 'fade'
    if key not in _MASTERS:
        # nome com versão: o mestre não pode ser apagado enquanto um mapa usa, então
        # uma mudança na transparência (ou na neve) ganha um mestre novo
        ver = '5' if snow else '4'
        dst = DEST + '/Trees/' + ('MA_Foliage_Trees_Snow' if snow else 'MA_Foliage_Trees_Fade') + ver
        if eal.does_asset_exist(dst):
            m = unreal.load_asset(dst)
        else:
            m = eal.duplicate_asset('/ProceduralVegetationEditor/SampleAssets/Materials/MasterMaterials/MA_Foliage_Trees', dst)
            if snow:
                lib.add_snow_to_material(m)
            lib.add_occlusion_fade_to_material(m, fade_collection())
            eal.save_loaded_asset(m)
        _MASTERS[key] = m
    return _MASTERS[key]


def tree_materials(mesh, snow=0.0):
    """Os materiais da árvore apontando pro mestre com a transparência (e a neve).
    Só grava se criou ou mudou: o gerador aberto usa estes materiais nas árvores
    e o Windows trava o arquivo (a montagem falhava ao regravar)."""
    master = tree_master(snow > 0)
    blend = unreal.BlendMode.BLEND_OPAQUE if os.environ.get('TREE_OPAQUE') == '1' else unreal.BlendMode.BLEND_MASKED
    out = []
    for sm in mesh.get_editor_property('static_materials'):
        mi = sm.material_interface
        dst = DEST + '/Trees/' + mi.get_name() + ('_Snow' if snow > 0 else '_WF')
        if eal.does_asset_exist(dst):
            c = unreal.load_asset(dst)
            same = c.get_editor_property('parent') == master and \
                c.get_editor_property('base_property_overrides').get_editor_property('blend_mode') == blend and \
                (snow <= 0 or abs(mel.get_material_instance_scalar_parameter_value(c, 'Snow') - snow) < 1e-4)
        else:
            c, same = eal.duplicate_asset(mi.get_path_name(), dst), False
        if not same:
            mel.set_material_instance_parent(c, master)
            # os do PVE forçam "opaco" (folhas de geometria); a transparência perto
            # do cursor precisa do "masked"
            o = c.get_editor_property('base_property_overrides')
            o.set_editor_property('override_blend_mode', True)
            o.set_editor_property('blend_mode', blend)
            c.set_editor_property('base_property_overrides', o)
            mel.update_material_instance(c)
            if snow > 0:
                mel.set_material_instance_scalar_parameter_value(c, 'Snow', snow)
            eal.save_loaded_asset(c, False)   # trocar o pai não marca o pacote como sujo
        out.append(c)
    return out


def decal_material(name, tex, color):
    """Marca de pneu: a faixa inteira do pneu afunda e escurece o chão; a banda
    de rodagem (altura da foto) varia por dentro.

    U ao longo da trilha (a foto já é comprida em U), V atravessado. A normal
    da foto deixava a marca preta; pegar só o fundo dos sulcos deixava a marca
    tracejada.
    """
    g = Graph(name)
    g.m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc = g.node(unreal.MaterialExpressionTextureCoordinate, -1000)
    col = g.sample(tex['color'], tc, 'color')
    rough = g.sample(tex['rough'], tc, 'masks')
    disp = g.sample(tex['disp'], tc, 'masks')
    c = g.custom('\n'.join([
        'float band = smoothstep(0.08, 0.28, UV.y) * smoothstep(0.92, 0.72, UV.y);',
        'float ends = smoothstep(0.0, 0.02, UV.x) * smoothstep(1.0, 0.98, UV.x);',
        'float tread = saturate((0.75 - D) * 1.5);',
        'return band * ends * (0.62 + 0.3 * tread);']), [('D', disp, 'R'), ('UV', tc, '')], F1)
    g.out(g.mul(g.mask(col, 'RGB', 'g'), '', g.const3(color, -400), '', -150), '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(rough, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(c, '', unreal.MaterialProperty.MP_OPACITY)
    return g.finish(name)


def paint_material(cracks):
    """Faixa pintada no asfalto: tinta branca gasta, falhando onde a pista
    rachou e onde o pneu passa; pontas dos traços suaves."""
    m = shared_material('M_RoadPaint')
    if m:
        return m
    g = Graph('M_RoadPaint')
    g.m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tc0 = g.node(unreal.MaterialExpressionTextureCoordinate, -1000)
    wp = g.node(unreal.MaterialExpressionWorldPosition, -1100)
    xy = g.node(unreal.MaterialExpressionComponentMask, -1000, r=True, g=True, b=False, a=False)
    g.link(wp, '', xy, '')
    d6 = g.node(unreal.MaterialExpressionDivide, -900, const_b=600.0)
    g.link(xy, '', d6, 'A')
    d1 = g.node(unreal.MaterialExpressionDivide, -900, const_b=120.0)
    g.link(xy, '', d1, 'A')
    cr = g.sample(cracks['diff'], d6, 'color')
    wear = g.sample(cracks['nor'], d1, 'normal')
    c = g.custom('\n'.join([
        'float2 t = UV;',                                   # x ao longo, y atravessado
        'float ends = smoothstep(0.0, 0.03, t.x) * smoothstep(1.0, 0.97, t.x);',
        'float sides = smoothstep(0.0, 0.15, t.y) * smoothstep(1.0, 0.85, t.y);',
        'float gone = saturate(Cr.r * 2.0) + saturate(abs(W.x) * 6.0 - 0.6);',
        'return saturate(ends * sides * (0.9 - gone));']),
        [('UV', tc0, ''), ('Cr', cr, 'RGB'), ('W', wear, 'RGB')], F1)
    g.out(g.const3((0.62, 0.61, 0.56), -300), '', unreal.MaterialProperty.MP_BASE_COLOR)
    r = g.node(unreal.MaterialExpressionConstant, -300, r=0.55)
    g.out(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(c, '', unreal.MaterialProperty.MP_OPACITY)
    return g.finish('road paint decal')


# ---- modelos ------------------------------------------------------------------

_MESHES = {}


# ---- palmeiras (glTF da sessão de arte) ---------------------------------------
# Uma malha, dois materiais: 0 = folhagem opaca com a cor na cor de vértice, 1 =
# tronco com textura de 512 px. Os dois com o MESMO vento (balanço pela altura),
# senão a copa descola do tronco. Vento no modelo da Crytek (Sousa, GPU Gems 3,
# cap. 16): balanço principal da planta inteira, crescendo com o quadrado da
# altura, em duas senoides de período ~3 e ~7,5 s fora de fase por instância, e
# o tremor dos folíolos por cima (só na folhagem). De cima, 15–25 cm no topo de
# uma tamareira de 13 m: vivo sem virar balanço de desenho animado

PALM_SRC = os.path.join(ASSETS, 'date_palm')


# ---- os monólitos de arenito (o kit provisório: Tools/blender_monoliths.py) ----------------------------
MONO_SRC = os.path.join(os.path.dirname(PROJECT), 'captures', 'worldforge', 'monoliths')
MONO_HLSL = r"""
// as camadas do arenito pela altura do mundo (todas as pecas com as mesmas, e isso le como geologia: a 2a
// opiniao de 12/10, Cordonnier 2018); a textura da 3 planos so da o desenho; o que olha pra cima pega areia
float z = WP.z * 0.01;
float b1 = frac(z / 2.7 + 0.31 * sin(WP.x * 0.0011) + 0.27 * sin(WP.y * 0.0013));
float b2 = frac(z / 7.3 + 0.17);
float band = lerp(0.86, 1.1, smoothstep(0.15, 0.4, b1) * (1.0 - smoothstep(0.75, 0.95, b1)));
float3 tone = lerp(float3(1.0, 1.0, 1.0), float3(1.12, 1.0, 0.86), smoothstep(0.55, 0.8, b2));   // faixas mais claras e amarelas
float3 c = C * Tint * band * tone;
// so o topo plano pega areia (nos labios estreitos das camadas a areia virava listra clara, 13/10)
float up = smoothstep(0.86, 0.97, N.z);
c = lerp(c, Sand, up * 0.7);
Rough = lerp(0.85, 0.95, up);
return c;
"""


def monolith_material(S):
    """M_MonolithRock: a rocha do penhasco nos 3 planos (3,6 m e 9,7 m, como a camada de rocha do chão) com as
    camadas do arenito pela altura e areia no que olha pra cima. A normal fica a da textura nos 3 planos."""
    m = shared_material('M_MonolithRock')
    if m:
        return m
    g = Graph('M_MonolithRock')
    WAT = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture')
    WAN = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal')

    def tri(tex, tile_m, normal=False):
        obj = g.node(unreal.MaterialExpressionTextureObject, -700, texture=tex,
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                     (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
        f = g.node(unreal.MaterialExpressionMaterialFunctionCall, -500)
        f.set_material_function(WAN if normal else WAT)
        g.link(obj, '', f, 'TextureObject')
        g.link(g.const3((tile_m * 100.0,) * 3, -700), '', f, 'TextureSize')
        return f
    rock = S['cliff_side']
    d1, d2 = tri(rock['diff'], 3.6), tri(rock['diff'], 9.7)
    mix = g.node(unreal.MaterialExpressionLinearInterpolate, -350, const_alpha=0.5)
    g.link(d1, 'XYZ Texture', mix, 'A')
    g.link(d2, 'XYZ Texture', mix, 'B')
    c = g.custom(MONO_HLSL, [('C', mix, ''), ('Tint', g.const3(CLIFF, -500), ''), ('Sand', g.const3((0.42, 0.33, 0.22), -500), ''),
                             ('WP', g.node(unreal.MaterialExpressionWorldPosition, -700), ''),
                             ('N', g.node(unreal.MaterialExpressionVertexNormalWS, -700), '')], F3, -150, extra=[('Rough', F1)])
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(tri(rock['nor'], 3.6, True), 'XYZ Texture', unreal.MaterialProperty.MP_NORMAL)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    return g.finish('monolith material')


def monolith_meshes(S):
    """O kit em /Game/Slice/Monoliths/<SM_Mono_x>/<SM_Mono_x>, Nanite, com o M_MonolithRock na malha (a vista ao vivo
    e o jogo usam a malha como está). Reimporta só se o .glb mudou. {nome: malha}."""
    out = {}
    files = sorted(f for f in (os.listdir(MONO_SRC) if os.path.isdir(MONO_SRC) else []) if f.startswith('SM_Mono_') and f.endswith('.glb'))
    if not files:
        log('monolith kit missing', MONO_SRC)
        return out
    mat = monolith_material(S)
    for f in files:
        n = os.path.splitext(f)[0]
        folder = DEST + '/Monoliths/' + n
        path = folder + '/' + n
        pkg = os.path.join(PROJECT, 'Content', folder[len('/Game/'):].replace('/', os.sep), n + '.uasset')
        src = os.path.join(MONO_SRC, f)
        if not (eal.does_asset_exist(path) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(src)):
            objs = import_file(src, folder)
            mesh = next((o for o in objs if isinstance(o, unreal.StaticMesh)), None)
            if mesh is None:
                log('monolith import failed', f)
                continue
            if mesh.get_path_name().split('.')[0] != path:
                eal.rename_asset(mesh.get_path_name().split('.')[0], path)
            # os materiais e texturas do importador não servem
            for a in eal.list_assets(folder, recursive=True):
                o = unreal.load_asset(a)
                if isinstance(o, (unreal.MaterialInterface, unreal.Texture)):
                    eal.delete_asset(a.split('.')[0])
        mesh = unreal.load_asset(path)
        if not mesh.get_editor_property('nanite_settings').enabled:
            lib.enable_nanite(mesh)
        for k in range(len(mesh.get_editor_property('static_materials'))):
            mesh.set_material(k, mat)
        eal.save_loaded_asset(mesh)
        out[n] = mesh
    log('monolith kit', len(out), 'meshes')
    return out


def place_monoliths(S):
    """Os monólitos do mapa (FWorldMap::Monoliths): a malha no lugar, girada e na escala."""
    names, xs = lib.slice_monoliths()
    if not names:
        return
    kit = monolith_meshes(S)
    group = lib.spawn_group_actor('Monoliths')
    by = {}
    for n, x in zip(names, xs):
        by.setdefault(n, []).append(x)
    for n, ts in by.items():
        if n in kit:
            lib.add_instances(group, kit[n], ts, True, 0, [kit[n].get_material(0)])
    log('monoliths placed', len(names))


# (16/10, o tanque derruba a palmeira; WORLDFORGE.md §12.8) a palmeira caída é um StaticMeshComponent que o jogo gira
# pela fórmula do ZH (o pivô na quebra) e marca nos CustomPrimitiveData: 1 o instante do tombo (o relógio do Time), 2 a
# direção da queda (rad, no espaço da malha), 3 = 1 caída, 4 o instante do impacto (0 = ainda não). O material:
# - na queda a copa atrasa (o tronco enverga ao contrário da queda, mais no meio do caminho);
# - no impacto ela chicoteia e amortece (~1 s);
# - deitada, nada passa do chão: a copa achata e abre por cima dele (o chão fica 0,8 m abaixo do pivô, a altura da
#   quebra; ou o plano do chão em volta da copa que o jogo põe em 5, 6 e 7: z = a + b·x + c·y, cm do mundo);
# - o vento some aos poucos conforme deita.
PALM_FALL_HLSL = r"""
float fallen = step(0.5, C3);
float tilt = 1.0 - saturate(Up.z);                      // 0 em pe, 1 deitada
float h = saturate(L.z / 1300.0);                         // a altura ao longo do tronco (13 m)
float3 off = Wind * (1.0 - fallen * saturate(tilt * 2.0));
if (fallen > 0.5)
{
    float2 fd2 = dot(Up.xy, Up.xy) > 1e-6 ? normalize(Up.xy) : float2(cos(C2), sin(C2));
    float3 fd = float3(fd2, 0.0);
    // a copa atrasa no meio da queda (o tronco enverga pra tras; o pico em tilt 0,5)
    float lag = 4.0 * tilt * (1.0 - tilt) * (1.0 - step(0.97, tilt));
    off += -fd * 140.0 * h * h * lag;
    // o chicote depois do impacto: oscila e amortece em ~1 s
    if (C4 > 0.0)
    {
        float dt = max(T - C4, 0.0);
        float osc = exp(-3.2 * dt) * sin(13.0 * dt);
        off += float3(0.0, 0.0, -1.0) * 90.0 * h * h * osc + fd * 40.0 * h * h * osc;
    }
    // nada abaixo do chao: o que passaria (a copa) achata numa camada fina rente ao chão
    // (16/10, a palmeira_quebra2 e 3: o empurrão pro lado media a distância à reta do tronco pelo pé; a tamareira é torta e
    // a copa fica toda de um lado dessa reta, então ela inteira escorregava ~1 m e saía da ponta. Só sobe, sem empurrão:
    // a copa fica presa na ponta. O chão pelo pivô a 0,8 m vezes a escala da palmeira, a do eixo Z do objeto)
    float g = OP.z - 80.0 * length(UpR) + 12.0;
    // (16/10 noite, a palmeira_quebra_f4: no mapa de dunas a copa cai num chão mais alto que o do pé e as folhas de baixo
    // ficavam dentro da duna; a copa saía com 6 folhas) o chão de verdade em volta da copa, quando o jogo dá: o plano
    // z = a + b·x + c·y (cm do mundo) nos CustomPrimitiveData 5, 6 e 7. Tudo zero: a conta pelo pivô, como antes
    if (abs(C5) + abs(C6) + abs(C7) > 0.0) g = C5 + C6 * W.x + C7 * W.y + 12.0;
    float z = W.z + off.z;
    if (z < g)
    {
        // (16/10, a palmeira_quebra5: subir só 85% deixava as folhas de baixo, ~4 m abaixo, ainda ~60 cm enterradas, e a
        // copa caída saía rala, 6 a 8 folhas à vista. Todas sobem até o chão e deitam numa camada de até 6 cm, a mais
        // funda por cima, como a folha que dobra debaixo da copa)
        float d = g - z;
        off.z += d + 6.0 * (1.0 - exp(-d / 200.0));
    }
}
return off;
"""


PALM_BREAK_HLSL = r"""
// (16/10, a 2ª quebra: Tools/blender_palm_break.py) o UV 3 diz o que é: x = a distância ao centro da tampa (o anel marrom e
// os feixes mais densos na borda), y (o glTF vira o V) = 0..0,4 a tampa (a fresta entre os tufos), 1 a lasca, 2 o tufo
float rr = U3.x;
float flag = 1.0 - U3.y;
float2 p = U0;
float2 g = p * 180.0;
float2 i = floor(g);
float best = 9.0, pick = 0.0;
for (int dy = -1; dy <= 1; dy++)
{
    for (int dx = -1; dx <= 1; dx++)
    {
        float2 c = i + float2(dx, dy);
        float2 h = frac(sin(float2(dot(c, float2(127.1, 311.7)), dot(c, float2(269.5, 183.3)))) * 43758.5453);
        float d = length(g - c - h);
        if (d < best) { best = d; pick = h.x; }
    }
}
float dens = 0.35 + 0.55 * smoothstep(0.3, 0.95, rr);
float dots = (1.0 - smoothstep(0.15, 0.3, best)) * step(pick, dens);
float mot = 0.5 + 0.25 * sin(p.x * 37.0 + 1.3) * sin(p.y * 29.0 - 0.7) + 0.25 * sin((p.x + p.y) * 61.0);
float3 straw = lerp(float3(0.30, 0.21, 0.11), float3(0.46, 0.37, 0.22), smoothstep(0.35, 0.65, mot));
float3 cap = lerp(straw, float3(0.12, 0.08, 0.045), dots * 0.75);
cap = lerp(cap, float3(0.16, 0.11, 0.06), 0.85 * smoothstep(0.0, 0.4, flag));
cap = lerp(cap, float3(0.11, 0.065, 0.035), smoothstep(0.84, 0.93, rr));
float streak = 0.5 + 0.5 * sin(p.x * 565.0 + 3.0 * sin(p.y * 20.0));
float3 fib = float3(0.42, 0.33, 0.19) * lerp(0.7, 1.05, streak);
float3 tuft = float3(0.20, 0.12, 0.06) * lerp(0.7, 1.1, streak);
float3 col = lerp(cap, fib, smoothstep(0.45, 0.6, flag));
return lerp(col, tuft, smoothstep(1.4, 1.6, flag));
"""


def palm_break_material():
    """M_PalmBreakV2: a fibra rasgada da palmeira (a tampa em tufos, o lado de dentro das lascas e o tufo caído), pela
    quebra real (o estudo de 16/10): a palha manchada com os pontos escuros dos feixes, mais densos na borda, o anel
    marrom de fora, as frestas escuras entre os tufos, as fibras ao longo nas lascas e o tufo cor de ferrugem. A 1ª (a
    M_PalmBreak, pela posição no mundo) foi reprovada junto com a malha em estrela."""
    m = shared_material('M_PalmBreakV2')
    if m:
        return m
    g = Graph('M_PalmBreakV2')
    u0 = g.node(unreal.MaterialExpressionTextureCoordinate, -700, coordinate_index=0)
    u3 = g.node(unreal.MaterialExpressionTextureCoordinate, -700, coordinate_index=3)
    c = g.custom(PALM_BREAK_HLSL, [('U0', u0, ''), ('U3', u3, '')], F3, -300)
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.node(unreal.MaterialExpressionConstant, -300, r=0.85), '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    return g.finish('palm break material')


def palm_material(name, foliage, tex=None, fade=False, fallen=False):
    """fade=True: a versão da cópia _Fade (masked pelo MPC_TreeFade); a original fica opaca. fallen=True: a da
    palmeira caída (PALM_FALL_HLSL)."""
    m = shared_material(name)
    if m:
        return m
    g = Graph(name)
    g.m.set_editor_property('two_sided', True)
    if foliage:
        g.m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        vc = g.node(unreal.MaterialExpressionVertexColor, -800)
        # (a 1ª saída da cor de vértice não tem nome). Um tom abaixo e mais verde:
        # com o sol baixo e a luz atravessando a folha, de cima a copa lia quase
        # branca (07/10)
        col = g.custom('return C.rgb * float3(0.82, 0.9, 0.74);', [('C', vc, '')], F3, -500)
        g.out(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
        sss = g.mul(col, '', g.const3((0.55, 0.6, 0.3), -500), '', -300)
        g.out(sss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        g.out(g.node(unreal.MaterialExpressionConstant, -500, r=0.6), '', unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        if tex is not None:
            t = g.node(unreal.MaterialExpressionTextureSample, -700, texture=tex,
                       sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            g.out(t, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            g.out(g.const3((0.25, 0.2, 0.15), -500), '', unreal.MaterialProperty.MP_BASE_COLOR)
        g.out(g.node(unreal.MaterialExpressionConstant, -500, r=0.85), '', unreal.MaterialProperty.MP_ROUGHNESS)
    lp = g.node(unreal.MaterialExpressionLocalPosition, -900)
    rnd = g.node(unreal.MaterialExpressionPerInstanceRandom, -900)
    tm = g.node(unreal.MaterialExpressionTime, -900)
    wp = g.node(unreal.MaterialExpressionWorldPosition, -900)
    wind = g.custom('\n'.join([
        'float h = saturate(L.z / 1400.0);',
        'float ph = R * 6.2832;',
        'float sway = 0.6 * sin(T * 2.1 + ph) + 0.4 * sin(T * 0.84 + 1.7 * ph);',
        'float3 off = float3(0.8, 0.6, 0.0) * sway * 18.0 * h * h;',
        'off += F * float3(0.0, 0.0, 4.0) * h * sin(T * 4.3 + dot(W.xy, float2(0.031, 0.047)));',
        'return off;']),
        [('L', lp, ''), ('R', rnd, ''), ('T', tm, ''), ('W', wp, ''), ('F', g.node(unreal.MaterialExpressionConstant, -900, r=1.0 if foliage else 0.0), '')],
        F3, -500)
    if fallen:
        up = g.node(unreal.MaterialExpressionTransform, -700)
        up.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
        up.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        g.link(g.const3((0.0, 0.0, 1.0), -900), '', up, '')
        upn = g.custom('return normalize(V);', [('V', up, '')], F3, -600)
        # (na 5.8 o dado do primitivo entra por um parâmetro escalar com "use custom primitive data", não por nó próprio)
        cpd = [g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name=nm, default_value=0.0,
                      use_custom_primitive_data=True, primitive_data_index=i) for nm, i in (('ToppleDir', 2), ('Fallen', 3), ('ImpactTime', 4),
                                                                                       ('CrownGroundA', 5), ('CrownGroundB', 6), ('CrownGroundC', 7))]
        wind = g.custom(PALM_FALL_HLSL, [('Wind', wind, ''), ('Up', upn, ''), ('UpR', up, ''), ('L', lp, ''), ('T', tm, ''), ('W', wp, ''),
                                          ('OP', g.node(unreal.MaterialExpressionObjectPositionWS, -900), ''),
                                          ('C2', cpd[0], ''), ('C3', cpd[1], ''), ('C4', cpd[2], ''),
                                          ('C5', cpd[3], ''), ('C6', cpd[4], ''), ('C7', cpd[5], '')], F3, -400)
    g.out(wind, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    m = g.finish(name)
    # o recorte perto do cursor, como nas árvores (o jogo pediu, 12/10: as palmeiras tapavam os
    # tanques, pior com a vila): a máscara pelo MPC_TreeFade, só no material da cópia _Fade. Masked
    # no Nanite vai pro raster programável: em toda palmeira custava +2,6 ms na vila (medida do jogo)
    if fade and lib.add_occlusion_fade_to_opacity_mask(m, fade_collection()):
        eal.save_loaded_asset(m)
    return m


def texs_of(folder):
    return [t for t in (unreal.load_asset(a) for a in eal.list_assets(folder, recursive=True)) if isinstance(t, unreal.Texture2D)]


def palm_meshes(names):
    """As palmeiras pedidas (nomes dos .glb), importadas uma vez (reimporta só se
    o .glb mudou), em Nanite, com os materiais de vento. [] se faltar o pacote."""
    out = []
    for n in names:
        src = os.path.join(PALM_SRC, n + '.glb')
        if not os.path.exists(src):
            log('palm missing', src)
            return []
        folder = DEST + '/Palms/' + n
        mesh_path = folder + '/' + n
        pkg = os.path.join(PROJECT, 'Content', folder[len('/Game/'):].replace('/', os.sep), n + '.uasset')
        stamp = os.path.join(PROJECT, 'Saved', 'WorldForge', 'palm_import_v2_%s' % n)   # v2: Nanite com área preservada
        if not (eal.does_asset_exist(mesh_path) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(src) and os.path.exists(stamp)):
            objs = import_file(src, folder)
            mesh = next((o for o in objs if isinstance(o, unreal.StaticMesh)), None)
            if mesh is None:
                log('palm import failed', src, [o.get_name() for o in objs])
                return []
            if mesh.get_path_name().split('.')[0] != mesh_path:
                eal.rename_asset(mesh.get_path_name().split('.')[0], mesh_path)
            mesh = unreal.load_asset(mesh_path)
            lib.enable_nanite(mesh, True)   # folhagem: preservar a área (a copa some de longe sem isso)
            eal.save_loaded_asset(mesh)
            open(stamp, 'w').close()
            log('palm imported', n)
        mesh = unreal.load_asset(mesh_path)
        # a cópia que o recorte perto do cursor usa (ForgeTreeFade e o jogo: SM_x → SM_x_Fade na
        # mesma pasta). A palmeira não vira voxels: a cópia é a mesma malha
        fade_path = mesh_path + '_Fade'
        if not eal.does_asset_exist(fade_path) or os.path.getmtime(pkg) > os.path.getmtime(pkg.replace('.uasset', '_Fade.uasset')):
            if eal.does_asset_exist(fade_path):
                eal.delete_asset(fade_path)
            eal.duplicate_asset(mesh_path, fade_path)
            log('palm fade copy', fade_path)
        # a cópia leva os materiais masked dela (o fantasma do recorte usa os "_Fade" da malha)
        fade_mesh = unreal.load_asset(fade_path)
        fade_mats = [palm_material('M_' + n + '_Foliage_Fade', True, fade=True),
                     palm_material('M_' + n + '_Trunk_Fade', False, texs_of(folder)[0] if texs_of(folder) else None, fade=True)]
        for k in range(min(len(fade_mats), len(fade_mesh.get_editor_property('static_materials')))):
            fade_mesh.set_material(k, fade_mats[k])
        eal.save_loaded_asset(fade_mesh, False)
        # os materiais vão no componente (AddInstances), não na malha: assim a
        # malha não é regravada a cada montagem (com o app aberto ela fica presa)
        texs = [unreal.load_asset(a) for a in eal.list_assets(folder, recursive=True)]
        texs = [t for t in texs if isinstance(t, unreal.Texture2D)]
        mats = [palm_material('M_' + n + '_Foliage', True), palm_material('M_' + n + '_Trunk', False, texs[0] if texs else None)]
        out.append((mesh, mats))
    return out


PALM_BREAK_SRC = os.path.join(ASSETS, 'date_palm_break')


def palm_break_meshes(names):
    """O toco e o tronco quebrado (Tools/blender_palm_break.py): <palmeira>_Stump e <palmeira>_Fallen na pasta da
    palmeira (o jogo procura pelo nome, ao lado da malha dela). Os materiais vão na malha: a folhagem e o tronco da
    palmeira (no _Fallen, as versões da queda, PALM_FALL_HLSL) e o M_PalmBreak na quebra. Reimporta só se o .glb mudou."""
    brk = palm_break_material()
    for n in names:
        folder = DEST + '/Palms/' + n
        texs = texs_of(folder)
        for part in ('Stump', 'Fallen'):
            src = os.path.join(PALM_BREAK_SRC, '%s_%s.glb' % (n, part))
            if not os.path.exists(src):
                continue
            mesh_path = '%s/%s_%s' % (folder, n, part)
            pkg = os.path.join(PROJECT, 'Content', folder[len('/Game/'):].replace('/', os.sep), '%s_%s.uasset' % (n, part))
            if not (eal.does_asset_exist(mesh_path) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(src)):
                tmp = folder + '/_break_import'
                objs = import_file(src, tmp)
                mesh = next((o for o in objs if isinstance(o, unreal.StaticMesh)), None)
                if mesh is None:
                    log('palm break import failed', src)
                    continue
                if eal.does_asset_exist(mesh_path):
                    eal.delete_asset(mesh_path)
                eal.rename_asset(mesh.get_path_name().split('.')[0], mesh_path)
                for a in eal.list_assets(tmp, recursive=True):
                    eal.delete_asset(a.split('.')[0])
                eal.delete_directory(tmp)
                mesh = unreal.load_asset(mesh_path)
                lib.enable_nanite(mesh, part == 'Fallen')   # (a copa caída: preservar a área, como a palmeira)
            mesh = unreal.load_asset(mesh_path)
            fallen = part == 'Fallen'
            # (_Fallen3, 16/10: a copa presa na ponta, só sobe ao bater no chão; o nome novo refaz o material)
            # (_Fallen4, 16/10: a copa inteira à vista, as folhas de baixo deitam rente ao chão em vez de ficar enterradas)
            # (_Fallen5, 16/10 noite: o plano do chão em volta da copa, se o jogo der, nos CustomPrimitiveData 5–7)
            fol = palm_material('M_' + n + '_Foliage' + ('_Fallen5' if fallen else ''), True, fallen=fallen)
            trk = palm_material('M_' + n + '_Trunk' + ('_Fallen5' if fallen else ''), False, texs[0] if texs else None, fallen=fallen)
            for k, sm in enumerate(mesh.get_editor_property('static_materials')):
                nm = str(sm.material_slot_name).lower() + ' ' + (sm.material_interface.get_name().lower() if sm.material_interface else '')
                mesh.set_material(k, brk if 'break' in nm else (trk if 'trunk' in nm else fol))
            eal.save_loaded_asset(mesh)
            log('palm break', mesh_path)


ROCK_BREAK_SRC = os.path.join(ASSETS, 'rock_break')


def rock_fresh_material():
    """M_RockFreshV2: a face nova da pedra quebrada pelo tanque (16/10): a pedra por dentro, sem a pátina. A 1ª (0,50/0,41/
    0,32, áspera 0,8) saiu no jogo clara e brilhante como plástico ao lado da pedra escaneada (escura, avermelhada): a
    face de quebra de arenito é um tom acima da casca, quente, e fosca; com o grão em manchas pela posição."""
    m = shared_material('M_RockFreshV2')
    if m:
        return m
    g = Graph('M_RockFreshV2')
    wp = g.node(unreal.MaterialExpressionWorldPosition, -700)
    c = g.custom('\n'.join([
        'float3 p = W * 0.01;',
        'float n = 0.5 + 0.25 * sin(p.x * 23.0 + 1.1) * sin(p.y * 19.0 - 0.3) + 0.25 * sin(p.z * 31.0 + p.x * 7.0);',
        'float gr = frac(sin(dot(floor(p * 90.0), float3(12.9898, 78.233, 37.719))) * 43758.5453);',
        'return float3(0.27, 0.17, 0.11) * lerp(0.82, 1.12, n) * lerp(0.9, 1.08, gr);']), [('W', wp, '')], F3, -300)
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.node(unreal.MaterialExpressionConstant, -300, r=0.95), '', unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.node(unreal.MaterialExpressionConstant, -300, r=0.0), '', unreal.MaterialProperty.MP_SPECULAR)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_NANITE)
    mel.set_base_material_usage(g.m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    return g.finish('rock fresh material')


def rock_break_meshes(aids):
    """A pedra pequena quebrada (Tools/blender_rock_break.py): <malha>_Broken ao lado da malha da pedra (o jogo procura
    pelo nome). O material 0 é o da própria malha (o jogo copia o do componente da pedra, com o tom do mapa) e o 1 é o
    M_RockFresh, a face da quebra. Reimporta só se o .glb mudou."""
    fresh = rock_fresh_material()
    for aid in aids:
        pick = os.path.join(PROJECT, 'Saved', 'WorldForge', 'model_pick_%s.txt' % aid)
        if not os.path.exists(pick):
            continue
        with open(pick, encoding='utf-8') as f:
            mesh_path = f.read().strip()
        base = unreal.load_asset(mesh_path)
        src = os.path.join(ROCK_BREAK_SRC, '%s_Broken.glb' % aid)
        if base is None or not os.path.exists(src):
            continue
        folder = mesh_path.rsplit('/', 1)[0]
        dst = mesh_path + '_Broken'
        pkg = os.path.join(PROJECT, 'Content', dst[len('/Game/'):].replace('/', os.sep) + '.uasset')
        if not (eal.does_asset_exist(dst) and os.path.exists(pkg) and os.path.getmtime(pkg) >= os.path.getmtime(src)):
            tmp = folder + '/_break_import'
            objs = import_file(src, tmp)
            mesh = next((o for o in objs if isinstance(o, unreal.StaticMesh)), None)
            if mesh is None:
                log('rock break import failed', src)
                continue
            if eal.does_asset_exist(dst):
                eal.delete_asset(dst)
            eal.rename_asset(mesh.get_path_name().split('.')[0], dst)
            for a in eal.list_assets(tmp, recursive=True):
                eal.delete_asset(a.split('.')[0])
            eal.delete_directory(tmp)
            lib.enable_nanite(unreal.load_asset(dst))
        mesh = unreal.load_asset(dst)
        rock_mat = base.get_material(0)
        for k, sm in enumerate(mesh.get_editor_property('static_materials')):
            nm = (str(sm.material_slot_name) + ' ' + (sm.material_interface.get_name() if sm.material_interface else '')).lower()
            mesh.set_material(k, fresh if 'fresh' in nm else rock_mat)
        eal.save_loaded_asset(mesh)
        log('rock break', dst)


def model(aid, sname, plant=False, nanite=True, tint=(1.0, 1.0, 1.0), snow=0.0, green=False, desat=0.0, normal_up=0.0):
    """Modelo do Poly Haven: FBX (importado uma vez por execução) + material da
    fatia (cor, normal, ARM, alfa, tom, neve). Devolve (malha, materiais): o
    material entra só no componente instanciado da fatia, então a mesma malha
    serve ao deserto, à neve e à floresta sem uma pisar na outra."""
    src = os.path.join(ASSETS, aid)
    files = [os.path.join(src, f) for f in os.listdir(src)]
    folder = DEST + '/Models/' + aid
    if aid not in _MESHES:
        # (16/10) já importada e o FBX não mudou: usa a que existe, sem regravar, como as texturas. Reimportar a cada
        # montagem regravava o pacote, e com qualquer Unreal usando a malha (o jogo linka a Slice; o Nanite segura o
        # arquivo pro streaming) a gravação falhava: "Error moving file", a montagem de 14/10 04:05
        fbx = find(files, '.fbx')
        have = [x.split('.')[0] for x in eal.list_assets(folder, recursive=False)]
        have = [x for x in have if isinstance(unreal.load_asset(x), unreal.StaticMesh)]
        def pkg_of(x):
            return os.path.join(PROJECT, 'Content', x[len('/Game/'):].replace('/', os.sep) + '.uasset')
        # a malha escolhida na importação fica anotada (a pasta pode ter várias: a mureta tem LOD0 a LOD5, e a escolha
        # vinha da ordem do importador): a mesma peça sempre
        pick = os.path.join(PROJECT, 'Saved', 'WorldForge', 'model_pick_%s.txt' % aid)
        picked = open(pick, encoding='utf-8').read().strip() if os.path.exists(pick) else ''
        fresh = picked in have and all(os.path.exists(pkg_of(x)) and os.path.getmtime(pkg_of(x)) >= os.path.getmtime(fbx) for x in have)
        if fresh:
            meshes = [unreal.load_asset(picked)]
        else:
            objs = import_file(fbx, folder)
            meshes = [o for o in objs if isinstance(o, unreal.StaticMesh)]
            if not meshes:
                meshes = [unreal.load_asset(a) for a in eal.list_assets(folder, recursive=False)
                          if isinstance(unreal.load_asset(a), unreal.StaticMesh)]
            with open(pick, 'w', encoding='utf-8') as f:
                f.write(meshes[0].get_path_name().split('.')[0])
        _MESHES[aid] = meshes[0]
    mesh = _MESHES[aid]
    if nanite and not mesh.get_editor_property('nanite_settings').enabled:
        lib.enable_nanite(mesh)
        eal.save_loaded_asset(mesh)
    # capim: a versão seca (dry_diff) no deserto e na neve, a verde na floresta
    color_file = find(files, '_dry_diff_') if (find(files, '_dry_diff_') and not green) else \
        next(f for f in files if '_diff_' in os.path.basename(f) and '_dry_diff_' not in os.path.basename(f))
    tex = {'color': texture(color_file, folder, 'color'),
           'normal': texture(find(files, '_nor_dx_'), folder, 'normal'),
           'masks': texture(find(files, '_arm_'), folder, 'masks')}
    name = 'MI_%s_%s%s' % (aid, sname, '_green' if green else '')
    if eal.does_asset_exist(folder + '/' + name):
        eal.delete_asset(folder + '/' + name)
    mi = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    plant = plant and find(files, '_alpha_') is not None   # galho sem alfa é geometria cheia (opaco)
    mel.set_material_instance_parent(mi, PLANT_MASTER if plant else ROCK_MASTER)
    mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', tex['color'])
    mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', tex['normal'])
    mel.set_material_instance_texture_parameter_value(mi, 'ARMTex', tex['masks'])
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'Snow', snow)
    mel.set_material_instance_scalar_parameter_value(mi, 'Desat', desat)
    if plant and normal_up > 0.0:
        mel.set_material_instance_scalar_parameter_value(mi, 'NormalUp', normal_up)
    if plant:
        mel.set_material_instance_texture_parameter_value(mi, 'AlphaTex', texture(find(files, '_alpha_'), folder, 'masks'))
    eal.save_loaded_asset(mi)
    return mesh, [mi] * len(mesh.get_editor_property('static_materials'))


def models(aid, sname, pattern='', **kw):
    """As malhas de um FBX que traz várias (conjuntos: rochas, samambaias), as que
    têm `pattern` no nome: [(malha, materiais)], todas com o material do conjunto."""
    mesh, mats = model(aid, sname, **kw)
    out = []
    for path in eal.list_assets(DEST + '/Models/' + aid, recursive=False):
        o = unreal.load_asset(path)
        if isinstance(o, unreal.StaticMesh) and pattern in o.get_name():
            if kw.get('nanite', True) and not o.get_editor_property('nanite_settings').enabled:
                lib.enable_nanite(o)
                eal.save_loaded_asset(o)
            out.append((o, [mats[0]] * len(o.get_editor_property('static_materials'))))
    log('set', aid, pattern, len(out), 'meshes')
    return out


def runs(pts):
    """Os trechos de uma lista de pontos do C++ separada por (0,0,-1e9)."""
    out, cur = [], []
    for p in list(pts) + [unreal.Vector(0, 0, -1e9)]:
        if p.z < -1e8:
            if cur:
                out.append(cur)
            cur = []
        else:
            cur.append(p)
    return out


def roadside(meshes, road_half_m):
    """Beira de estrada: dois trechos com muretas de concreto (barreira de posto
    de controle), uma tombada; contam história sem fechar a pista. No trecho
    mais longo de asfalto, fora dos entroncamentos."""
    rs = runs(lib.slice_road(1.0, True))
    pts = max(rs, key=len) if rs else []
    if len(pts) < 40 or not meshes.get('barriers'):
        return
    mesh, mats = meshes['barriers'][0]
    xs = []
    for frac, side in ((0.3, 1.0), (0.72, -1.0)):
        i0 = int(len(pts) * frac)
        for k in range(5):
            i = min(len(pts) - 2, i0 + k * 2)
            a, b = pts[i - 1], pts[i + 1]
            dx, dy = b.x - a.x, b.y - a.y
            ln = math.hypot(dx, dy) or 1.0
            dx, dy = dx / ln, dy / ln
            off = side * (road_half_m + 1.3) * 100.0
            loc = unreal.Vector(pts[i].x - dy * off, pts[i].y + dx * off, lib.slice_height_at(unreal.Vector2D(pts[i].x - dy * off, pts[i].y + dx * off)))
            yaw = math.degrees(math.atan2(dy, dx)) + (k * 37 % 13 - 6)
            roll = 90.0 if k == 3 else 0.0      # uma tombada
            xs.append(unreal.Transform(loc, unreal.Rotator(roll=roll, pitch=0.0, yaw=yaw), unreal.Vector(1, 1, 1)))
    lib.add_instances(lib.spawn_group_actor('Roadside'), mesh, xs, True, 0, mats)
    log('roadside barriers', len(xs))


def layer_info(sname, name):
    path = DEST + '/Layers/LI_%s_%s' % (sname, name)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    li = asset_tools.create_asset('LI_%s_%s' % (sname, name), DEST + '/Layers', unreal.LandscapeLayerInfoObject, None)
    lib.set_layer_info_name(li, name)
    eal.save_loaded_asset(li)
    return li


# ---- luz -----------------------------------------------------------------------

def physical_ev100(sun_elevation_deg, sun_lux, sky_lux, transmittance):
    e = sun_lux * math.sin(math.radians(sun_elevation_deg)) * transmittance + sky_lux
    return math.log2(e * 100.0 / 250.0)


def environment(L):
    elev, az = L['elev'], math.radians(L['az'])
    to_sun = unreal.Vector(math.cos(az) * math.cos(math.radians(elev)), math.sin(az) * math.cos(math.radians(elev)),
                           math.sin(math.radians(elev)))
    rot = unreal.MathLibrary.make_rot_from_x(unreal.Vector(-to_sun.x, -to_sun.y, -to_sun.z))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000), rot)
    sun.set_actor_label('Sun')
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    lc.set_editor_property('intensity', 120000.0)
    lc.set_editor_property('atmosphere_sun_light', True)
    lc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property('light_source_angle', 0.53)          # diâmetro do sol: sombra dura no contato
    lc.set_editor_property('contact_shadow_length', 0.03)       # capim e pedrinhas
    sky = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky.set_actor_label('Atmosphere')
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    sc.set_editor_property('mie_scattering_scale', L['mie'])
    sc.set_editor_property('mie_absorption_scale', L['mie_abs'])
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
    sl.set_actor_label('SkyLight')
    slc = sl.get_component_by_class(unreal.SkyLightComponent)
    slc.set_editor_property('real_time_capture', True)
    slc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property('intensity', L['sky'])
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -500))
    fog.set_actor_label('HeightFog')
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', L['fog'])
    fc.set_editor_property('fog_height_falloff', L['falloff'])
    ev = physical_ev100(elev, 120000.0, L['sky_lux'], L['trans'])
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label('PostProcess')
    pp.set_editor_property('unbound', True)
    s = pp.get_editor_property('settings')
    for prop, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ('auto_exposure_min_brightness', ev),
                        ('auto_exposure_max_brightness', ev),
                        ('auto_exposure_apply_physical_camera_exposure', False),
                        ('auto_exposure_bias', L['bias']),
                        ('bloom_intensity', 0.25),
                        ('local_exposure_shadow_contrast_scale', 0.6),
                        ('local_exposure_highlight_contrast_scale', 0.9)):
        s.set_editor_property('override_' + prop, True)
        s.set_editor_property(prop, value)
    pp.set_editor_property('settings', s)
    for a in (sun, sky, sl, fog, pp):
        a.set_folder_path('Environment')
    log('environment EV100 %.2f, sun %.0f°' % (ev, elev))


# ---- montagem -------------------------------------------------------------------

def decals_along(points, material, half_len_cm, half_width_cm, offsets_cm, every=1, dash=None, skip_road=False, keep=None):
    """Decalques ao longo de uma linha (cm): um por ponto (a cada `every`),
    deslocados de lado (offsets), virados ao longo dela. `dash` = (liga, período)
    em pontos, pra faixas tracejadas."""
    n = 0
    for i in range(1, len(points) - 1, every):
        if dash and (i % dash[1]) >= dash[0]:
            continue
        # (offsets_cm pode ser uma função do ponto: a borda da rua da vila, que alarga)
        offs = offsets_cm(points[i]) if callable(offsets_cm) else offsets_cm
        a, b = points[i - 1], points[i + 1]
        dx, dy = b.x - a.x, b.y - a.y
        ln = math.hypot(dx, dy) or 1.0
        dx, dy = dx / ln, dy / ln
        yaw = math.degrees(math.atan2(dy, dx))
        for off in offs:
            loc = unreal.Vector(points[i].x - dy * off, points[i].y + dx * off, points[i].z)
            if skip_road and lib.slice_road_distance(unreal.Vector2D(loc.x, loc.y)) < 420.0:
                continue
            if keep is not None and not keep(loc, dx, dy):
                continue
            # Rotator do Python é (roll, pitch, yaw): por nome. Pitch -90 projeta pra
            # baixo; aí o Z local fica ao longo da linha e o Y atravessado. O
            # shader tira o U do Z e o V do Y (DeferredDecal.usf: DecalVector.zyx)
            da = actors.spawn_actor_from_class(unreal.DecalActor, loc, unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw))
            dc = da.get_component_by_class(unreal.DecalComponent)
            dc.set_editor_property('decal_material', material)
            # meias medidas: X fundo, Y largura, Z comprimento
            dc.set_editor_property('decal_size', unreal.Vector(60.0, half_width_cm, half_len_cm))
            da.set_folder_path('Decals')
            n += 1
    return n


def tire_tracks(decal_mat):
    """Marcas de pneu nas estradas de terra (bitola de ~1,8 m); fora do asfalto."""
    n = 0
    for line in runs(lib.slice_tracks(2.0)):
        n += decals_along(line, decal_mat, 100.0, 24.0, (-90.0, 90.0), skip_road=True)
    log('tire track decals', n)


def on_slab(loc, dx, dy):
    """A faixa da beira só onde a laje existe (14/10, a 1ª foto da laje: a faixa a 0,55 m da beira nominal ficava solta na
    areia onde a beira quebrou ou a areia cobriu): o pedaço de 2,2 m inteiro, as duas pontas e o meio, 15 cm pra dentro."""
    for k in (-100.0, 0.0, 100.0):
        w = lib.slice_slab_at(unreal.Vector2D(loc.x + dx * k, loc.y + dy * k))
        if w.x < 0.0 or w.y > -0.15 or w.z > 0.16:
            return False
    return True


SLAB_ON = False


def road_paint(paint_mat, road_half_m):
    """Tracejado central (3 m pintados a cada 9 m, a maquete do Look, 13/10) e faixas de borda
    contínuas a 0,55 m da beira (a meia largura de cada ponto: a rua da vila alarga), por trecho de
    asfalto: sem tinta nos entroncamentos nem nas pontas que se desfazem em terra (C++, SliceRoad
    com bMarksOnly)."""
    n = 0
    def edges(p):
        h = lib.slice_road_half(unreal.Vector2D(p.x, p.y)) or road_half_m * 100.0
        e = h - 55.0
        return (-e, e)
    for pts in runs(lib.slice_road(1.0, True)):
        if len(pts) < 3:
            continue
        n += decals_along(pts, paint_mat, 153.0, 7.0, (0.0,), every=1, dash=(1, 9))
        n += decals_along(pts, paint_mat, 110.0, 6.0, edges, every=2, keep=on_slab if SLAB_ON else None)
    log('road paint decals', n)


# Absorção e espalhamento da água por metro (R, G, B): Pope & Fry 1997 (água
# pura) + o Kd de Jerlov por tipo de mar (pesquisa de 07/10). O nó do Single
# Layer Water multiplica a profundidade em CENTÍMETROS (SingleLayerWaterShading.ush):
# divide por 100.
WATER_OPTICS = {
    'desert': ((0.30, 0.06, 0.02), (0.001, 0.002, 0.004)),   # tropical/deserto (Jerlov I–IA): turquesa
    # temperado de costa limpa (Jerlov IB–II, o Mediterrâneo das calanques do alvo):
    # azul-marinho e turquesa no raso; o II–III de antes (azul absorvido 0,08) dava
    # um verde-azulado uniforme (07/10)
    'forest': ((0.33, 0.07, 0.04), (0.002, 0.004, 0.007)),
    # mar polar no inverno: quase sem plâncton, água clara (Jerlov I–II); azul-marinho
    # no fundo e turquesa no raso, como nos alvos (o verde-cinza costeiro de antes,
    # Jerlov 3–5, ficava um caldo verde com as pedras do fundo verde-limão, 07/10)
    'snow': ((0.35, 0.07, 0.03), (0.002, 0.003, 0.006)),
}


def ocean_material(sname):
    """Água no modelo Single Layer Water da Unreal (o mesmo do plugin Water):
    cor pela absorção na profundidade, por canal, e o fundo aparecendo no raso.
    O translúcido de antes era uma chapa cinza (o reflexo do céu dominava).
    A câmera de RTS olha 30–40° da vertical: reflete só ~3% (Specular 0,255 =
    água; o padrão 0,5 reflete o dobro) e a rugosidade das marolas menores que
    um pixel fica em 0,12–0,35 (Cox–Munk; 0,04 era espelho). O Opacity é a
    espuma por cima: 0 no mar aberto (o padrão 1 apaga a água inteira, e era o
    "escuro" do teste de 05/10)."""
    absorb, scatter = WATER_OPTICS.get(sname, WATER_OPTICS['forest'])
    m = shared_material('M_Ocean_' + sname)
    if m:
        return m
    g = Graph('M_Ocean_' + sname)
    g.m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    g.m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    normal = unreal.load_asset('/Game/Textures/T_WaterN')
    wp = g.node(unreal.MaterialExpressionWorldPosition, -1400)
    xy = g.node(unreal.MaterialExpressionComponentMask, -1250, r=True, g=True, b=False, a=False)
    g.link(wp, '', xy, '')
    depth = g.node(unreal.MaterialExpressionPixelDepth, -1400)

    def ripple(tile_m, sx, sy):
        d = g.node(unreal.MaterialExpressionDivide, -1100, const_b=tile_m * 100.0)
        g.link(xy, '', d, 'A')
        pan = g.node(unreal.MaterialExpressionPanner, -950, speed_x=sx, speed_y=sy)
        g.link(d, '', pan, 'Coordinate')
        return g.sample(normal, pan, 'normal', -800)

    # marolas de 2,5 e 4 m cruzadas (~90°), a ~0,4–0,6 m/s; mais lisas ao longe
    # (vistas de cima, marolas menores que meio metro serrilham)
    a = ripple(2.5, 0.16, 0.05)
    b = ripple(4.0, -0.04, 0.13)
    info_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, -900, parameter_name='WaterInfo', texture=normal)
    info_size = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='InfoSize', default_value=1024.0)
    surf = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='Surf', default_value=0.0)
    water_on = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='WaterOn', default_value=0.0)
    if sname == 'snow':
        # gelo flutuando na costa do mar de inverno (o elemento-assinatura dos dois
        # alvos de neve). Pela nomenclatura da OMM (WMO No. 259): "brash" (cacos
        # < 2 m) colado na pedra e placas ("ice cakes", < 20 m) até ~10–30 m da
        # costa, mais raras pra fora. Voronoi em duas escalas: F2 − F1 é a distância
        # à borda da célula, a fresta entre placas; a célula vira gelo se o sorteio
        # dela cair abaixo da cobertura daquela distância. Na beira de cada placa,
        # um halo turquesa claro meio transparente (o gelo submerso, como nos alvos).
        # Só no mar (I.b), parado (de cima, placa andando lia como textura correndo).
        floe = g.custom('\n'.join([
            'float2 uv = P.xy / (S * 100.0) + 0.5;',
            'float4 I = Tex.SampleLevel(TexSampler, uv, 0);',
            'float d = (I.r - 0.5) * 64.0;',
            'float cn = 0.5 + 0.25 * (sin(P.x / 3100.0 + 1.3 * sin(P.y / 4700.0)) + sin(P.y / 2700.0 + 1.1 * sin(P.x / 3900.0)));',
            'float reach = 8.0 + 22.0 * cn;',
            # borda das placas recortada em 3 escalas (~0,1–0,5 m): Voronoi puro dava
            # polígonos de régua, "desenho vetorial" (revisão de 07/10)
            'float wob = 0.05 * sin(P.x / 53.0 + 2.0 * sin(P.y / 71.0)) + 0.04 * sin(P.y / 37.0 + 1.7 * sin(P.x / 29.0))',
            '    + 0.07 * sin(P.x / 170.0 + 1.9 * sin(P.y / 210.0)) + 0.05 * sin(P.y / 130.0 + 1.3 * sin(P.x / 190.0));',
            'float ice = 0.0, halo = 0.0, rim = 0.0, tone = 0.0, sub = 0.0;',
            'for (int k = 0; k < 2; k++)',
            '{',
            '    float cell = k == 0 ? 800.0 : 240.0;',
            # placas até `reach`, rareando; cacos colados na costa e alguns soltos mais longe
            '    float cov = k == 0 ? 0.6 * (1.0 - smoothstep(0.25 * reach, reach, d))',
            '        : 0.9 * (1.0 - smoothstep(1.0, 3.0 + 5.0 * cn, d)) + 0.18 * (1.0 - smoothstep(0.5 * reach, 1.3 * reach, d));',
            '    float2 q = P.xy / cell + 0.35 * float2(sin(P.y / 1900.0), cos(P.x / 2300.0));',
            '    float2 ip = floor(q), fp = frac(q);',
            '    float f1 = 9.0, f2 = 9.0, id = 0.0, sz = 0.0;',
            '    for (int j = -1; j <= 1; j++)',
            '    for (int i = -1; i <= 1; i++)',
            '    {',
            '        float2 gg = float2(i, j);',
            '        float2 h = ip + gg + 17.0 * k;',
            '        float2 r = frac(sin(float2(dot(h, float2(127.1, 311.7)), dot(h, float2(269.5, 183.3)))) * 43758.5453);',
            '        float2 v = gg + 0.15 + 0.7 * r - fp;',
            '        float dd = dot(v, v);',
            '        if (dd < f1) { f2 = f1; f1 = dd; id = frac(r.x * 7.31 + r.y * 3.17); sz = frac(r.x * 13.7 + r.y * 5.3); }',
            '        else if (dd < f2) { f2 = dd; }',
            '    }',
            '    float e = sqrt(f2) - sqrt(f1) + wob;',
            '    float on = step(id, cov);',
            # cada placa encolhe um tanto (fresta de ~0,5–3 m): tamanhos variados e água entre elas
            '    float t = 0.05 + 0.2 * sz;',
            '    float plate = on * smoothstep(t, t + 0.04, e);',
            '    rim = max(rim, plate * (1.0 - smoothstep(t + 0.04, t + 0.16, e)));',
            # ~1 em 5 placas meio afundada (a onda passa por cima): turquesa e meio transparente
            '    float sb = step(frac(sz * 7.9 + id * 3.3), 0.2);',
            '    sub = plate > ice ? sb : sub;',
            '    halo = max(halo, on * (1.0 - plate) * smoothstep(t - 0.12, t, e));',
            '    tone = plate > ice ? id / max(cov, 0.01) : tone;',
            '    ice = max(ice, plate);',
            '}',
            'float m = On * Surf * I.b * step(0.0, d);',
            # placa: neve por cima (clara, com ondulação leve) e gelo velho exposto
            # (azul-acinzentado); a beira molhada clara-turquesa (o gelo afunda na borda)
            # (neve das placas um tom abaixo do branco: de perto lia como recorte branco puro)
            'float3 snow = float3(0.76, 0.81, 0.86);',
            'Col = lerp(snow, float3(0.5, 0.64, 0.7), smoothstep(0.6, 0.95, tone)) * lerp(0.88, 1.0, frac(tone * 5.3));',
            # borda azul-acinzentada (a neve afina e o gelo aparece); a afundada, turquesa
            'Col = lerp(Col, float3(0.6, 0.73, 0.8), 0.75 * rim);',
            'Col = lerp(Col, float3(0.34, 0.6, 0.64), sub);',
            # por fora, o gelo submerso: turquesa claro meio transparente
            # perto da pedra, entre os cacos, lama de gelo (slush): cinza-claro meio opaco
            'float slush = 1.0 - smoothstep(1.0, 6.0, d);',
            'Halo = m * max(halo * 0.3, slush * 0.55 * (1.0 - ice));',
            'return m * ice * lerp(1.0, 0.5, sub);']),
            [('Tex', info_tex, ''), ('S', info_size, ''), ('Surf', surf, ''), ('On', water_on, ''), ('P', wp, '')],
            F1, -700, extra=[('Col', F3), ('Halo', F1)])
        fm, fm_pin = floe, ''
        fc, fc_pin = floe, 'Col'
        fh, fh_pin = floe, 'Halo'
    else:
        fm = fh = g.node(unreal.MaterialExpressionConstant, -700, r=0.0)
        fm_pin = fh_pin = ''
        fc, fc_pin = g.const3((0.82, 0.86, 0.9), -700), ''
    # a placa de gelo é chata e fosca (sem as marolas)
    n = g.custom('float3 N = normalize(float3(A.xy + B.xy, A.z * B.z));\n'
                 'float k = lerp(0.55, 0.2, saturate(D / 25000.0)) * (1.0 - Fm);\n'
                 'return normalize(float3(N.xy * k, 1.0));',
                 [('A', a, 'RGB'), ('B', b, 'RGB'), ('D', depth, ''), ('Fm', fm, fm_pin)], F3, -500)
    g.out(n, '', unreal.MaterialProperty.MP_NORMAL)
    rough = g.custom('return lerp(lerp(0.12, 0.35, saturate(D / 25000.0)), 0.6, Fm);', [('D', depth, ''), ('Fm', fm, fm_pin)], F1, -500)
    g.out(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    # SkyReflection (pedido do Look, 11/10): o peso do reflexo do céu e da névoa
    # (1 = o de hoje); a receita da luz baixa quando o horizonte dourado lavava o lago
    sky_refl = g.node(unreal.MaterialExpressionScalarParameter, -700, parameter_name='SkyReflection', default_value=1.0)
    spec = g.custom('return 0.255 * saturate(K);', [('K', sky_refl, '')], F1, -500)
    g.out(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    # espuma: a linha d'água (1–3 m, sempre visível) e a arrebentação rala até
    # ~14 m da costa no mar (só com borda de mar: Surf), quebradas pelo ruído
    # que anda com as marolas (a espuma é o Opacity do Single Layer Water);
    # o gelo da costa (inverno) entra por cima
    foam = g.custom('\n'.join([
        'float2 uv = P.xy / (S * 100.0) + 0.5;',
        'float4 I = Tex.SampleLevel(TexSampler, uv, 0);',
        'float d = (I.r - 0.5) * 64.0;',
        'float n = saturate(0.5 + 0.5 * (A.x + B.y));',
        'float shore = (1.0 - smoothstep(0.1, 0.8 + 1.4 * n, d)) * smoothstep(0.45, 0.7, n) * lerp(0.3, 0.6, I.b);',   # (line é palavra do HLSL); fina, quebrada, meio transparente; no lago só um fio
        'float band = Surf * I.b * (1.0 - smoothstep(2.0, 14.0, d)) * smoothstep(0.66, 0.82, n) * 0.45;',
        'float f = On * saturate(shore + band) * step(0.0, d);',
        'float o = saturate(f * (1.0 - Fm) + Fm + Fh);',
        'Col = (float3(0.85, 0.86, 0.84) * f * (1.0 - Fm) + FC * Fm + float3(0.5, 0.78, 0.82) * Fh) / max(o, 0.001);',
        'return o;']),
        [('Tex', info_tex, ''), ('S', info_size, ''), ('Surf', surf, ''), ('On', water_on, ''), ('P', wp, ''),
         ('A', a, 'RGB'), ('B', b, 'RGB'), ('Fm', fm, fm_pin), ('FC', fc, fc_pin), ('Fh', fh, fh_pin)], F1, -500,
        extra=[('Col', F3)])
    g.out(foam, 'Col', unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(foam, '', unreal.MaterialProperty.MP_OPACITY)
    outn = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -200)
    # a cor própria do corpo d'água (pedido do Look, 11/10): WaterBodyColor entra por
    # cima do espalhamento de Jerlov na medida de WaterBodyStrength (0 = o de hoje);
    # 0,02/cm de espalhamento já dá a cor lida de cima mesmo com reflexo forte
    body = g.node(unreal.MaterialExpressionVectorParameter, -900, parameter_name='WaterBodyColor', default_value=unreal.LinearColor(0.05, 0.35, 0.4, 1))
    body_k = g.node(unreal.MaterialExpressionScalarParameter, -900, parameter_name='WaterBodyStrength', default_value=0.0)
    scat = g.custom('return lerp(S, C.rgb * 0.02, saturate(K));',
                    [('S', g.const3(tuple(v / 100.0 for v in scatter), -700), ''), ('C', body, ''), ('K', body_k, '')], F3, -500)
    g.link(g.const3(tuple(v / 100.0 for v in absorb), -500), '', outn, 'AbsorptionCoefficients')
    g.link(scat, '', outn, 'ScatteringCoefficients')
    g.link(g.node(unreal.MaterialExpressionConstant, -500, r=0.1), '', outn, 'PhaseG')
    return g.finish('ocean material ' + sname)


def map_instance(parent, name, textures, scalars):
    """Instância de material deste mapa (a textura da água é por mapa)."""
    path = DEST + '/Water/' + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = asset_tools.create_asset(name, DEST + '/Water', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in textures.items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def water_plane(size_m):
    """Água (lagos, mar): o material translúcido do app, na cota d'água do
    gerador, cobrindo o terreno inteiro (size_m). Com borda de mar, segue até o
    horizonte (o plano de 260 m de antes cobria só o miolo: no mapa inteiro o
    lago ficava sem água)."""
    z = lib.slice_water_level()
    if z < -1e8:
        return
    mat = ocean_material(WATER_BIOME) if os.environ.get('WATER_OLD') != '1' else unreal.load_asset('/Game/Materials/M_Water')
    if WATER_INFO is not None and os.environ.get('WATER_OLD') != '1':
        mat = map_instance(mat, 'MI_Ocean_' + MAP_NAME, {'WaterInfo': WATER_INFO},
                           {'InfoSize': WATER_INFO_SIZE, 'WaterOn': 1.0, 'Surf': 1.0 if lib.slice_has_sea() else 0.25})
    # a água só onde existe água (13/10, docs/WORLDFORGE.md §12.3, a P1): uma malha por corpo, na cota dele, no
    # lugar do plano global (a água aparecia embaixo do mapa pelos buracos do chão). WATER_PLANE=1 volta o plano
    if os.environ.get('WATER_PLANE', '0') != '1' and hasattr(lib, 'spawn_water_bodies'):
        paths = lib.spawn_water_bodies(mat, DEST + '/Water', MAP_NAME)
        for p in paths:
            eal.save_loaded_asset(unreal.load_asset(p))
        log('water bodies', len(paths), 'malhas na cota', z)
        if paths:
            return
    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, z), unreal.Rotator(0, 0, 0))
    a.set_actor_label('Water')
    span = size_m * (8.0 if lib.slice_has_sea() else 1.05)
    a.set_actor_scale3d(unreal.Vector(span, span, 1))
    a.static_mesh_component.set_static_mesh(plane)
    a.static_mesh_component.set_material(0, mat)
    a.static_mesh_component.set_cast_shadow(False)
    log('water plane at z', z)


def scatter(cfg, meshes):
    group = lib.spawn_group_actor('Scatter')
    for kind, spacing, (smin, smax), key, cull in cfg['scatter']:
        variants = meshes[key]
        # o C++ sorteia a malha de cada lugar e assenta pela caixa dela (pedras:
        # base medida, inclinação da encosta, enterradas; SliceScatterMeshes)
        per = lib.slice_scatter_meshes(kind, spacing, smin, smax, [m for m, _ in variants], 0.0)
        n = 0
        for (mesh, mats), sc in zip(variants, per):
            if sc.transforms:
                # o capim não projeta sombra (o jeito dos RTS grandes: a folha fina com o sol baixo deixava um
                # risco escuro comprido em cada tufo, 13/10); a sombra de contato da luz ainda assenta o pé
                # (só o capim; as moitas do oásis, mais cheias, seguem com sombra)
                if TRAMPLE and PLANT_MASTER is not None and any(isinstance(m, unreal.MaterialInstanceConstant) and m.get_editor_property('parent') == PLANT_MASTER for m in mats):
                    # (16/10) a etiqueta pro jogo entregar as marcas da batalha (o capim e a moita que deitam na trilha)
                    lib.add_instances(group, mesh, sc.transforms, key not in ('tufts_dry', 'tufts_green'), cull, mats, 'WorldForgeTrample')
                else:
                    lib.add_instances(group, mesh, sc.transforms, key not in ('tufts_dry', 'tufts_green'), cull, mats)
                n += len(sc.transforms)
        log('scatter', kind, n)
    if cfg.get('cliffs'):
        # paredões escaneados cobrindo os cortes do terreno: em pé ao longo da
        # parede, costas na encosta, altura pelo desnível (C++)
        for (mesh, mats), spacing in ((meshes['cliffs'][0], 9.0), (meshes['cliffs'][1], 13.0)):
            xs = lib.slice_scatter_meshes('Cliff', spacing, 0.9, 1.25, [mesh], 0.0)[0].transforms
            lib.add_instances(group, mesh, xs, True, 0, mats)
            log('scatter Cliff', mesh.get_name(), len(xs))


def build(sname, tag=None, cfg=None, grid=(COMPONENTS, QUADS, QUAD_M), center=None):
    """Monta uma fatia (ou o mapa inteiro: build_world). `tag` dá nome aos assets
    (a fatia por padrão), `cfg` troca a configuração, `grid` o tamanho do terreno."""
    global ROCK_MASTER, PLANT_MASTER
    ground_v3_for(sname)
    tag = tag or sname
    if cfg is None:
        # fatia: os controles do painel (os padrões do app + os da fatia) num JSON,
        # o mesmo caminho do "Ver em qualidade real" (o mapa inteiro já traz o dele)
        cfg = SLICES[sname]
        gen = {'seed': cfg['seed'], 'players': 2, 'size': 1024, 'biome': cfg['biome'], 'relief': 0.5,
               'plateaus': 0.5, 'water': 0.35, 'erosion': 0.5, 'forests': 0.5, 'resources': 0.5}
        gen.update(cfg.get('gen', {}))
        path = os.path.join(PROJECT, 'Saved', 'WorldForge', 'slice_%s.json' % sname)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            json.dump(gen, f)
        lib.load_world_settings(path)
    sids = {'aerial_beach_01', 'asphalt_04'} | {s[2] for s in cfg['layers']} | {s[5]['far'][0] for s in cfg['layers'] if isinstance(s[5], dict) and 'far' in s[5]}
    S = {aid: surface(aid) for aid in sorted(sids)}
    cracks = surface('gen_asphalt_cracks')
    defaults = {'color': S['asphalt_04']['diff'], 'normal': S['asphalt_04']['nor'], 'masks': S['asphalt_04']['arm']}
    # mestres por fatia: refazer uma fatia não mexe nos materiais das outras
    ROCK_MASTER = prop_material('M_ScanRock_' + tag, False, defaults)
    PLANT_MASTER = prop_material('M_ScanPlant_' + tag, True, defaults)
    layer_specs = [s for s in cfg['layers'] if s[0] != 'asphalt' or cfg['road'] > 0]
    ground = ground_material('M_SliceGround_' + tag, S, layer_specs, cracks, sname)
    if tag.startswith('world_'):
        # o chão e a água da vista ao vivo do app (as mesmas camadas do mapa inteiro)
        if not shared_material('M_LiveGround_' + sname):
            ground_material('M_LiveGround_' + sname, S, layer_specs, cracks, sname, live=True)
        ocean_material(sname)
    tt = os.path.join(ASSETS, 'TireTracks001')
    tfiles = [os.path.join(tt, f) for f in os.listdir(tt)]
    decal = decal_material('M_TireTrack_' + tag,
                           {'color': texture(find(tfiles, '_Color'), DEST + '/Decals', 'color'),
                            'rough': texture(find(tfiles, '_Roughness'), DEST + '/Decals', 'masks'),
                            'disp': texture(find(tfiles, '_Displacement'), DEST + '/Decals', 'masks')}, cfg['track_color'])
    meshes, cfg = make_meshes(sname, tag, cfg)
    if tag.startswith('world_'):
        write_live_manifest(sname, cfg, meshes)
    layers = [layer_info(tag, s[1]) for s in layer_specs]

    if eal.does_asset_exist(cfg['map']):
        eal.delete_asset(cfg['map'])
    levels.new_level(cfg['map'])
    environment(cfg['light'])
    c = unreal.Vector2D(1e9, 1e9) if center is None else unreal.Vector2D(center[0], center[1])
    # a textura da água deste mapa: primeiro vazia (terra), a instância do chão
    # já com ela; depois do Landscape (que gera o mapa) a textura é preenchida no
    # mesmo objeto. Trocar o material do Landscape depois de pronto apagava as
    # camadas (sem placa de vídeo o Landscape não refaz as camadas)
    global WATER_INFO, WATER_INFO_SIZE, MAP_NAME
    MAP_NAME = cfg['map'].split('/')[-1]
    WATER_INFO_SIZE = grid[0] * grid[1] * grid[2]
    info_path = DEST + '/Water/T_WaterInfo_' + MAP_NAME
    lib.reset_slice_map()
    WATER_INFO = lib.make_water_info_texture(info_path, WATER_INFO_SIZE)
    ground_mi = map_instance(ground, 'MI_Ground_' + MAP_NAME, {'WaterInfo': WATER_INFO},
                             {'InfoSize': WATER_INFO_SIZE, 'WaterOn': 1.0})
    landscape, err = lib.build_slice_landscape(cfg['seed'], 2, cfg['biome'], 1024.0, c, grid[0], grid[1], grid[2],
                                               ground_mi, layers, cfg['road'])
    # (com água ou com salina: o C++ deixa a textura vazia quando não há nenhuma)
    WATER_INFO = lib.make_water_info_texture(info_path, WATER_INFO_SIZE)
    eal.save_loaded_asset(unreal.load_asset(info_path))
    # a direção da pista (o asfalto do Look: as faixas de pneu ao longo dela)
    road_dir_path = DEST + '/Water/T_RoadDir_' + MAP_NAME
    road_dir = lib.make_road_dir_texture(road_dir_path, WATER_INFO_SIZE)
    eal.save_loaded_asset(unreal.load_asset(road_dir_path))
    mel.set_material_instance_texture_parameter_value(ground_mi, 'RoadDir', road_dir)
    mel.update_material_instance(ground_mi)
    eal.save_loaded_asset(ground_mi)
    # (16/10, o Felipe pelo Look: "gostei da laje 3D, tá aprovado") a laje é o padrão do deserto; ROAD_SLAB=0 tira
    if os.environ.get('ROAD_SLAB', '1') != '0' and sname == 'desert' and cfg['road'] > 0:
        # a estrada com volume (14/10, v3): a laje de 12 cm com a borda quebrada onde a estrada é gasta ou destruída
        # (16/10, a sujeira na beira: o chão do lado, então as camadas soltas também: areia, cascalho, terra e sal)
        slab_mat = ground_material('M_RoadSlab_' + tag, S, [sp for sp in layer_specs if sp[0] == 'asphalt' or sp[1] in ('Sand', 'Gravel', 'Dirt', 'Salt')],
                                   cracks, sname, slab=True)
        # a posição na pista em 16 bits (a faixa pintada nova do Look, quando entrar no material da laje: o parâmetro RoadFrame)
        road_frame_path = DEST + '/Water/T_RoadFrame_' + MAP_NAME
        road_frame = lib.make_road_frame_texture(road_frame_path, WATER_INFO_SIZE)
        eal.save_loaded_asset(unreal.load_asset(road_frame_path))
        slab_mi = map_instance(slab_mat, 'MI_RoadSlab_' + MAP_NAME, {'WaterInfo': WATER_INFO, 'RoadDir': road_dir, 'RoadFrame': road_frame},
                               {'InfoSize': WATER_INFO_SIZE, 'WaterOn': 1.0})
        log('road slab', lib.build_road_slabs(slab_mi, DEST + '/RoadSlab', MAP_NAME))
        global SLAB_ON
        SLAB_ON = True
    if landscape:
        lib.build_landscape_nanite(landscape)
    if err:
        log('landscape error:', err)
    for road in runs(lib.slice_road(1.0)):
        mid = road[len(road) // 2]
        # pra vista da estrada nas fotos
        log('road run %d m: middle %.0f %.0f, ends %.0f %.0f / %.0f %.0f' % (len(road), mid.x / 100.0, mid.y / 100.0,
            road[0].x / 100.0, road[0].y / 100.0, road[-1].x / 100.0, road[-1].y / 100.0))
    scatter(cfg, meshes)
    if sname == 'desert':
        place_monoliths(S)
    tire_tracks(decal)
    if cfg['road'] > 0 and cfg.get('paint', True) and not SLAB_PAINT:
        road_paint(paint_material(cracks), cfg['road'] * 0.5)
    if cfg['road'] > 0:
        roadside(meshes, cfg['road'] * 0.5)
    global WATER_BIOME
    WATER_BIOME = sname
    water_plane(grid[0] * grid[1] * grid[2])
    village_hash = lib.slice_village_hash()
    if village_hash:
        # sem casa no mapa (o jogo desenha pelo JSON): só a etiqueta com a impressão digital
        marker = lib.spawn_group_actor('WorldForgeVillage')
        marker.set_editor_property('tags', [unreal.Name('WorldForgeVillage'), unreal.Name('village_hash:' + village_hash)])
        log('village hash', village_hash)
    # o vento do mapa (pra onde sopra, graus): a versão final põe no MPC_Wind (as ondulações da areia) e o
    # jogo lê do map.json da exportação (o rastro que enche, as ondulações)
    wind = lib.slice_wind_yaw()
    wmarker = lib.spawn_group_actor('WorldForgeWind')
    wmarker.set_editor_property('tags', [unreal.Name('WorldForgeWind'), unreal.Name('wind_yaw:%.1f' % wind),
                                         unreal.Name('wind_speed:%.1f' % lib.slice_wind_speed())])
    if cfg.get('map', '').startswith('/Game/Maps/'):
        out = os.path.join(PROJECT, 'Saved', 'WorldForge', 'exports', cfg['map'].split('/')[-1])
        os.makedirs(out, exist_ok=True)
        with open(os.path.join(out, 'map.json'), 'w', encoding='utf-8') as f:
            json.dump({'map': cfg['map'].split('/')[-1], 'wind_yaw': round(wind, 1), 'wind_speed': round(lib.slice_wind_speed(), 1),
                       'about': 'wind_yaw: degrees, where the wind blows to (0 = +X); the sand ripples are baked for a sun 155 deg from it'}, f, indent=1)
    log('wind yaw', wind)
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.ForgeSliceGameMode)
    levels.save_current_level()
    eal.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('slice saved', tag, cfg['map'])


def oasis2_scatter(cfg):
    """OASIS2=1 (13/10, o capim do oásis): o verde vem do chão; menos tufo fino e mais moita compacta."""
    if os.environ.get('OASIS2', '1') != '1':   # (aprovado pelo Felipe em 13/10; OASIS2=0 volta)
        return cfg
    cfg = dict(cfg)
    out = []
    for kind, spacing, sc, key, cull in cfg['scatter']:
        if kind == 'OasisGrass' and key == 'tufts_green':
            spacing = spacing * 1.9
        elif kind == 'OasisGrass' and key == 'oasis_shrubs':
            spacing, sc = 1.3, (1.0, 1.8)
        out.append((kind, spacing, sc, key, cull))
    cfg['scatter'] = out
    log('oasis2', 'menos tufo fino, mais moita compacta e o verde no chao')
    return cfg


def sand_albedo(cfg, a):
    """A areia v2 com outro albedo (o tipo de mapa): o cascalho é a mesma areia ×0,85 (como na v2), a terra e a
    cor da esteira seguem a razão contra a classic de hoje (0,49/0,40/0,27)."""
    k = tuple(a[i] / (0.49, 0.40, 0.27)[i] for i in range(3))
    def retint(l):
        if l[1] == 'Sand':
            return (l[0], l[1], l[2], l[3], tuple(a), l[5])
        if l[1] == 'Gravel':
            return (l[0], l[1], l[2], l[3], tuple(0.85 * v for v in a), l[5])
        if l[1] == 'Dirt':
            return (l[0], l[1], l[2], l[3], tuple(l[4][i] * k[i] for i in range(3)), l[5])
        return l
    cfg = dict(cfg)
    cfg['layers'] = [retint(l) for l in cfg['layers']]
    cfg['track_color'] = tuple(cfg['track_color'][i] * k[i] for i in range(3))
    log('desert sand albedo', a)
    return cfg


def build_world(path):
    """"Ver em qualidade real": o mapa inteiro do app com os controles do painel
    (JSON gravado pelo app), no acabamento da fatia do bioma. O nível vai pra
    "map" do JSON (o app usa /Game/Maps/Worlds/W_<impressão digital dos
    controles>, a prévia guardada); sem ele, /Game/Maps/World_Full.
    Terreno de 16×16 componentes de 127 quads (2032 quads por lado, ~0,5 m no mapa de 1 km)."""
    res = lib.load_world_settings(path)
    ok, biome, size = res if len(res) == 3 else (True,) + tuple(res)
    if not ok:
        log('world settings not found', path)
        return
    sname = {0: 'forest', 1: 'desert', 2: 'snow'}[int(biome)]
    cfg = oasis2_scatter(dict(SLICES[sname]))
    with open(path, encoding='utf-8-sig') as f:   # (o JSON gravado pelo PowerShell vem com BOM)
        wj = json.load(f)
    cfg['map'] = wj.get('map', '/Game/Maps/World_Full')
    # a areia por tipo de mapa (13/10, o Look: "tipos de mapa, não regra"): o albedo da areia v2 (SAR/SAG/SAB do
    # sand_desert_v2_ground) e, na mesma razão, o cascalho, a terra e a cor da esteira, pra não misturar duas
    # areias. Os monólitos (o AlUla) saem na areia de arenito com óxido de ferro, 0,45/0,26/0,15 (a medida do Look
    # contra a r1_raid_slabs); SANDALB="r,g,b" força outra
    # (16/10) a areia do mapa ("sand" no JSON, o painel; o Felipe: "vai de 3", as três como opção por mapa): 3 a cor de
    # duna (o padrão do deserto dourado), 2 a amarela (a v2 da maquete), 1 a clássica. SAND=v2|v1|classic e SANDALB no
    # ambiente ainda mandam (os mapas de teste)
    pick = int(wj.get('sand', 3))
    sand = os.environ.get('SAND') or ('classic' if pick == 1 else 'v2')
    if sname == 'desert' and sand == 'classic' and not os.environ.get('SAND'):
        cfg['layers'] = SAND_CLASSIC + [l for l in cfg['layers'] if l[1] not in ('Sand', 'Gravel', 'Dirt')]
        log('desert sand: classic (o mapa)')
    dune = sname == 'desert' and ((pick == 3 and not os.environ.get('SAND')) or int(wj.get('relief_type', 0)) == 1)
    alb = os.environ.get('SANDALB', '') or ('0.45,0.26,0.15' if dune else '')
    alb_tag = ''
    if alb and sname == 'desert' and sand == 'v2':
        cfg = sand_albedo(cfg, tuple(float(v) for v in alb.split(',')))
        alb_tag = '_sa' + ''.join('%02d' % round(float(v) * 100) for v in alb.split(','))
    comps, quads = 16, 127
    log('world', sname, 'size', size, 'quad %.3f m' % (size / (comps * quads)), cfg['map'])
    # materiais e camadas por bioma (as prévias guardadas de biomas diferentes não se pisam)
    # (a areia escolhida tem material próprio: o World_TestSalt2 do jogo na classic não pisa nos mapas de teste na v2)
    tag = 'world_' + sname + ('' if sname != 'desert' or sand == 'v2' else '_' + sand) + alb_tag + os.environ.get('TAG_SUFFIX', '')
    build(sname, tag=tag, cfg=cfg, grid=(comps, quads, size / (comps * quads)), center=(0.0, 0.0))
    # a vila (Villages nos controles): o JSON que o jogo lê pra montar a grade e o
    # desenho das casas (o chão aplainado e as ruas já estão no Landscape)
    map_name = cfg['map'].split('/')[-1]
    village = lib.slice_village_json(map_name)
    if village:
        out = os.path.join(PROJECT, 'Saved', 'WorldForge', 'exports', map_name)
        os.makedirs(out, exist_ok=True)
        with open(os.path.join(out, 'village.json'), 'w', encoding='utf-8') as f:
            f.write(village)
        log('village json', os.path.join(out, 'village.json'))
    # a grade do tipo de chão pro jogo (13/10: no empacotado a camada do Landscape some): asfalto, terra batida e rocha
    # a 1 m por pixel, do lado do village.json (o game.ps1 worlds copia)
    out = os.path.join(PROJECT, 'Saved', 'WorldForge', 'exports', map_name)
    os.makedirs(out, exist_ok=True)
    meta = lib.make_surface_png(os.path.join(out, 'surface.png'), 1.0)
    if meta:
        with open(os.path.join(out, 'surface.json'), 'w', encoding='utf-8') as f:
            f.write(meta)
        log('surface', os.path.join(out, 'surface.png'))
    # a água por corpo (13/10, docs/WORLDFORGE.md §12.3, a P0: só dados): a grade de 4 m da simulação
    # (grid.png/json, o jogo), o campo do material (water.exr e T_Water_<mapa>, o Look)
    if hasattr(lib, 'make_water_exports'):
        water_path = DEST + '/Water/T_Water_' + map_name
        water = lib.make_water_exports(out, water_path, map_name)
        if water:
            eal.save_loaded_asset(unreal.load_asset(water_path))
        log('water', water or 'FALHOU')
    prune_previews(12)


def prune_previews(keep):
    """Guarda só as `keep` prévias mais novas (/Game/Maps/Worlds): cada uma tem ~100 MB."""
    folder = os.path.join(PROJECT, 'Content', 'Maps', 'Worlds')
    if not os.path.isdir(folder):
        return
    maps = sorted((f for f in os.listdir(folder) if f.endswith('.umap')),
                  key=lambda f: os.path.getmtime(os.path.join(folder, f)), reverse=True)
    for f in maps[keep:]:
        name = os.path.splitext(f)[0]
        eal.delete_asset('/Game/Maps/Worlds/' + name)
        log('preview pruned', name)


def make_meshes(sname, tag, cfg):
    """As malhas do espalhamento do bioma (com os materiais desta montagem),
    por chave da tabela de espalhamento. Devolve (meshes, cfg): as palmeiras
    trocam a árvore de copa do oásis na tabela."""
    snow, rt, ct, ds = cfg['snow'], cfg['rock_tint'], cfg['cliff_tint'], cfg.get('rock_desat', 0.0)
    meshes = {
        'boulders_big': [model(a, tag, tint=rt, snow=snow, desat=ds) for a in ('namaqualand_boulder_02', 'namaqualand_boulder_03', 'namaqualand_boulder_04')],
        'boulders_small': [model(a, tag, tint=rt, snow=snow, desat=ds) for a in ('namaqualand_boulder_05', 'namaqualand_boulder_06',
                                                                                   'namaqualand_boulders_01', 'namaqualand_rocks_01')],
        # todas as variações do conjunto (antes só a 1ª malha do FBX: os tufos e as pedrinhas saíam todos
        # iguais e liam como pontinhos repetidos; o Look, 13/10)
        'pebbles': models('namaqualand_stones_01', tag, 'LOD0', tint=rt, snow=snow, desat=ds),
        'cliffs': [model('namaqualand_cliff_01', tag, tint=ct, snow=snow, desat=ds), model('namaqualand_cliff_02', tag, tint=ct, snow=snow, desat=ds)],
        # o capim seco e os gravetos com o tom da maquete do Look no deserto (sand_desert_v2.json: capim
        # ×2,3/2,0/1,25 desat 0,15; gravetos ×2,0/1,85/1,6 desat 0,3)
        'branches': models('dry_branches_medium_01', tag, '', plant=True, nanite=False, snow=snow,
                           **({'tint': (2.0, 1.85, 1.6), 'desat': 0.3, 'normal_up': 0.5} if sname == 'desert' else {})),
        'tufts_dry': models('grass_medium_01', tag, 'LOD0', plant=True, nanite=False, snow=snow,
                            **({'tint': (2.3, 2.0, 1.25), 'desat': 0.15, 'normal_up': 0.75} if sname == 'desert' else {}))
                     + ([] if sname == 'desert' else [model('grass_medium_02', tag, plant=True, nanite=False, snow=snow)]),
        'shrubs': models('wild_rooibos_bush', tag, '', plant=True, nanite=False, **({'normal_up': 0.5} if sname == 'desert' else {}))
                  + models('searsia_lucida', tag, 'LOD0', plant=True, nanite=False, **({'normal_up': 0.5} if sname == 'desert' else {})),
    }
    if sname == 'desert':
        # o oásis: copa verde e capim verde (as mesmas malhas da floresta)
        broad = tree('broadleaf')
        tree_fade('broadleaf')
        meshes['broadleaf'] = [(broad, tree_materials(broad))]
        # tamareiras (PlantCatalog, CC BY 4.0, preparadas pela sessão de arte:
        # Intermediate/Assets/date_palm). A ordem conta (SliceScatterMeshes,
        # TreePalm): touceira e madura junto da água, a adulta de 18 m mais longe
        palms = palm_meshes(['SM_DatePalm_OasisCluster', 'SM_DatePalm_Mature', 'SM_DatePalm_Urban'])
        if palms and os.environ.get('PALM_BREAK', '1') == '1':
            # (16/10) o toco e o tronco quebrado pra o jogo trocar quando o tanque derruba (só malhas novas: o mapa não muda)
            try:
                palm_break_meshes(['SM_DatePalm_OasisCluster', 'SM_DatePalm_Mature', 'SM_DatePalm_Urban'])
                rock_break_meshes(['namaqualand_boulder_05', 'namaqualand_boulder_06'])
            except Exception as ex:
                log('palm/rock break FALHOU (a montagem segue)', ex)
        if palms:
            meshes['palms'] = palms
            cfg = dict(cfg)
            cfg['scatter'] = [('TreePalm', 0, (1.0, 1.0), 'palms', 0) if s[0] == 'TreeBroadleaf' else s for s in cfg['scatter']]
        # o capim verde do oásis: as folhas finas viravam riscos escuros e pixel espalhado de cima (o Felipe,
        # 13/10): a luz pelo chão (NormalUp), um verde mais vivo e só as touceiras baixas e largas do conjunto
        # (large 32×33×15 cm, mid ~20×20×18). O grass_medium_02 (12×12×16, fino e alto) aumentado 4–5× era o
        # risco vertical visto da câmera inclinada; a folha larga também deixa de ser mais fina que um pixel
        meshes['tufts_green'] = [x for x in models('grass_medium_01', tag, 'LOD0', plant=True, nanite=True, green=True,
                                                   tint=(1.15, 1.2, 0.9), normal_up=0.75)
                                 if '_large_' in x[0].get_name() or '_mid_' in x[0].get_name()]
        meshes['oasis_shrubs'] = models('shrub_02', tag, '_LOD0', plant=True, normal_up=0.5)
    if sname == 'forest':
        # o campo de grama denso só aguenta em Nanite (100 mil touceiras)
        meshes['tufts_green'] = [model('grass_medium_01', tag, plant=True, nanite=True, green=True),
                                 model('grass_medium_02', tag, plant=True, nanite=True, green=True)]
        broad, shrub = tree('broadleaf'), tree('shrub')
        tree_fade('broadleaf'), tree_fade('conifer')
        meshes['broadleaf'] = [(broad, tree_materials(broad))]
        meshes['conifers_green'] = [(tree('conifer'), tree_materials(tree('conifer')))]
        meshes['shrubs_green'] = [(shrub, [])] + models('shrub_02', tag, '_LOD0', plant=True)
        meshes['ferns'] = models('fern_02', tag, plant=True)
        meshes['sprouts'] = models('shrub_01', tag, '_LOD0', plant=True)
    if sname in ('forest', 'snow'):
        meshes['rocks_moss'] = models('rock_moss_set_01', tag, snow=snow)
        meshes['deadwood'] = [model('tree_stump_01', tag, snow=snow), model('dead_tree_trunk', tag, snow=snow)]
    if sname == 'snow':
        conifer = tree('conifer')
        tree_fade('conifer')
        meshes['conifers'] = [(conifer, tree_materials(conifer, snow))]
    if sname == 'desert':
        meshes['barriers'] = models('concrete_road_barrier', tag, '_LOD0')
    return meshes, cfg


def write_live_manifest(sname, cfg, meshes):
    """A tabela do espalhamento do bioma pra vista ao vivo do app
    (Saved/WorldForge/live_<bioma>.json): por tipo, o espaçamento, a escala, a
    distância de corte e as malhas (caminho, materiais, caixa em cm). As regras
    de cada tipo estão no C++ (FWorldDetail::ScatterCandidates); aqui fica só o
    que é asset, uma fonte só pros dois."""
    def mesh_entry(mesh, mats):
        box = mesh.get_bounding_box()
        return {'mesh': mesh.get_path_name(), 'materials': [m.get_path_name() if m else '' for m in (mats or [])],
                'bounds': [box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z]}
    entries = []
    for kind, spacing, (smin, smax), key, cull in cfg['scatter']:
        entries.append({'kind': kind, 'spacing': spacing, 'smin': smin, 'smax': smax, 'cull': cull,
                        'meshes': [mesh_entry(m, mats) for m, mats in meshes[key]]})
    if cfg.get('cliffs'):
        for (mesh, mats), spacing in ((meshes['cliffs'][0], 9.0), (meshes['cliffs'][1], 13.0)):
            entries.append({'kind': 'Cliff', 'spacing': spacing, 'smin': 0.9, 'smax': 1.25, 'cull': 0,
                            'meshes': [mesh_entry(mesh, mats)]})
    path = os.path.join(PROJECT, 'Saved', 'WorldForge', 'live_%s.json' % sname)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump({'biome': sname, 'scatter': entries}, f, indent=1)
    log('live manifest', path, len(entries))


def build_live(sname):
    """Só os materiais da vista ao vivo do app (chão e água) de um bioma, sem
    montar mapa (SLICE=live: os três)."""
    cfg = SLICES[sname]
    sids = {'aerial_beach_01', 'asphalt_04'} | {s[2] for s in cfg['layers']} | {s[5]['far'][0] for s in cfg['layers'] if isinstance(s[5], dict) and 'far' in s[5]}
    S = {aid: surface(aid) for aid in sorted(sids)}
    cracks = surface('gen_asphalt_cracks')
    layer_specs = [s for s in cfg['layers'] if s[0] != 'asphalt' or cfg['road'] > 0]
    ground_material('M_LiveGround_' + sname, S, layer_specs, cracks, sname, live=True)
    ocean_material(sname)
    eal.save_directory(DEST + '/Materials', only_if_is_dirty=True, recursive=True)
    log('live materials', sname)


# as areias do deserto (o Felipe, 13/10: "ambas são boas, não é substituível"): SAND=v2 a do Look
# (padrão), SAND=v1 a v1 dela, SAND=classic a de antes (sandy_gravel_02 com a foto aérea por cima, a terra e o
# cascalho com o tom próprio). O material é um só por bioma, então a escolhida vale pra montagem toda
SAND_CLASSIC = [
    ('plain', 'Sand', 'sandy_gravel_02', 2.5, (1.0, 1.0, 1.0), {'far': ('aerial_beach_01', 45.0, (1.32, 1.02, 0.72))}),
    ('plain', 'Gravel', 'gravelly_sand', 2.5, (1.04, 1.0, 0.96), None),   # perto da areia: mancha, não sujeira
    ('plain', 'Dirt', 'dry_ground_01', 4.0, (1.3, 0.98, 0.7), None),   # quente: a foto puxa pro oliva
]
if os.environ.get('SAND', 'v2') == 'v1':
    SLICES['desert'] = dict(SLICES['desert'])
    SLICES['desert']['layers'] = [('sand_v1', 'Sand', 'sandy_gravel_02', 2.5, (0.49, 0.40, 0.27), {'mottle': 0.25, 'ripple': 0.25})
                                  if l[1] == 'Sand' else l for l in SLICES['desert']['layers']]
    log('desert sand: v1')
if os.environ.get('SAND', 'v2') == 'classic':
    SLICES['desert'] = dict(SLICES['desert'])
    SLICES['desert']['layers'] = SAND_CLASSIC + [l for l in SLICES['desert']['layers'] if l[1] not in ('Sand', 'Gravel', 'Dirt')]
    log('desert sand: classic')

# SLICE=desert,snow,forest (padrão: desert); SLICE=world monta o mapa do app
# (WORLD_SETTINGS = o JSON dos controles); SLICE=none só carrega as funções
for _s in os.environ.get('SLICE', 'desert').split(','):
    if _s in SLICES:
        build(_s)
    elif _s == 'world':
        build_world(os.environ['WORLD_SETTINGS'])
    elif _s == 'live':
        for _b in ('desert', 'snow', 'forest'):
            build_live(_b)
    elif _s == 'manifest':
        for _b in ('desert', 'snow', 'forest'):
            _cfg = SLICES[_b]
            _sids = {'aerial_beach_01', 'asphalt_04'} | {x[2] for x in _cfg['layers']}
            _S = {aid: surface(aid) for aid in sorted(_sids)}
            _d = {'color': _S['asphalt_04']['diff'], 'normal': _S['asphalt_04']['nor'], 'masks': _S['asphalt_04']['arm']}
            ROCK_MASTER = prop_material('M_ScanRock_world_' + _b, False, _d)
            PLANT_MASTER = prop_material('M_ScanPlant_world_' + _b, True, _d)
            _m, _c = make_meshes(_b, 'world_' + _b, dict(_cfg))
            write_live_manifest(_b, _c, _m)
        eal.save_directory(DEST, only_if_is_dirty=True, recursive=True)
