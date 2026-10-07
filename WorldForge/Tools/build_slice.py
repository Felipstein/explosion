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
        'map': '/Game/Maps/Slice_Desert', 'seed': 3, 'biome': 1, 'road': 7.5,
        'layers': [
            ('plain', 'Sand', 'sandy_gravel_02', 2.5, (1.0, 1.0, 1.0), {'far': ('aerial_beach_01', 45.0, (1.32, 1.02, 0.72))}),
            ('plain', 'Gravel', 'gravelly_sand', 2.5, (1.04, 1.0, 0.96), None),   # perto da areia: mancha, não sujeira
            ('plain', 'Dirt', 'dry_ground_01', 4.0, (1.18, 0.98, 0.78), None),
            ('rock', 'Rock', 'cliff_side', 3.6, (0.95, 0.95, 0.95), None),
            ('plain', 'Cracked', 'mud_cracked_dry_03', 3.0, (0.72, 0.68, 0.64), None),
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
            ('Pebble', 1.4, (1.0, 2.2), 'pebbles', 120),
            ('Tuft', 0.55, (2.5, 5.0), 'tufts_dry', 180),
            ('Branch', 4.0, (0.8, 1.4), 'branches', 120),
            ('Shrub', 2.2, (1.0, 2.2), 'shrubs', 300),
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
            ('plain', 'Scree', 'gravelly_sand', 2.5, (0.52, 0.56, 0.62), None),   # cascalho gelado aparecendo
            # trilha e bases: barro frio. A snow_04 é foto de sulcos de pneu paralelos;
            # alinhada ao mundo (não à estrada) as listras cruzavam o acostamento na
            # diagonal e viravam "dentes" (visto em 06/10)
            ('plain', 'Mud', 'mud_forest', 2.35, (0.85, 0.85, 0.92), None),
            ('rock', 'Rock', 'cliff_side', 3.6, (0.62, 0.68, 0.80), 0.6),
            ('plain', 'Ice', 'gen_ice', 6.0, (1.0, 1.0, 1.0), None),
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
            ('plain', 'Grass', 'leafy_grass', 2.0, (0.45, 0.72, 0.55),
             # manchas de ~70 m entre verde mais escuro e capim seco amarelado: no mapa
             # inteiro, da câmera de jogo (190 m, sem touceiras), o 0,9–1,2 de antes
             # deixava o gramado de um verde só (06/10)
             {'far': ('gen_moss', 9.0, (2.2, 2.0, 2.4)), 'hue': ((0.82, 0.97, 0.8), (1.32, 1.14, 0.66), 70.0)}),
            ('plain', 'Litter', 'forest_leaves_02', 3.0, (0.72, 0.68, 0.58), None),   # folhas de outono: menos laranja
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
    objs = import_file(path, folder, name)
    tex = unreal.load_asset(folder + '/' + name) if not objs else objs[0]
    if kind == 'normal':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif kind == 'masks':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
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


def ground_material(name, S, specs, cracks):
    """Landscape: camadas misturadas pela altura da textura, uma mistura por canal.

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

    def layer(lname, s, tile_m, tint, extra=None):
        extra = extra or {}
        far = extra.get('far')
        a = g.sample(s['diff'], uv(tile_m), 'color')
        b = g.sample(s['diff'], uv(tile_m * 3.7, 0.37), 'color')
        diff = g.node(unreal.MaterialExpressionLinearInterpolate, -400, const_alpha=0.35)
        g.link(a, 'RGB', diff, 'A')
        g.link(b, 'RGB', diff, 'B')
        nor = g.sample(s['nor'], uv(tile_m), 'normal')
        arm = g.sample(s['arm'], uv(tile_m), 'masks')
        disp = g.sample(s['disp'], uv(tile_m), 'masks')
        base = g.mul(diff, '', g.const3(tint, -300), '', -200)
        if far is not None:
            # a foto aérea por cima da de perto (o grão de perto vira ruído de
            # longe). Metade das duas sempre e só um pouco mais da aérea ao longe:
            # a troca de 25 a 85 m fazia um anel em volta da câmera que andava com
            # ela (visto no passeio de 05/10)
            fs, ftile, ftint = far
            fb = g.mul(g.sample(S[fs]['diff'], uv(ftile, 0.21), 'color'), 'RGB', g.const3(ftint, -300), '', -200)
            t = g.custom('return 0.45 + 0.3 * saturate((D - 1000.0) / 14000.0);', [('D', depth, '')], F1)
            mix = g.node(unreal.MaterialExpressionLinearInterpolate, -100)
            g.link(base, '', mix, 'A')
            g.link(fb, '', mix, 'B')
            g.link(t, '', mix, 'Alpha')
            base = mix
        base = g.mul(base, '', macro, '', 0)
        if 'hue' in extra:
            # variação de matiz em manchas largas (a foto aérea como ruído)
            ha, hb, hs = extra['hue']
            n = g.sample(S['aerial_beach_01']['diff'], uv(hs, 0.57), 'color', -1200)
            base = g.custom('return C * lerp(A, B, smoothstep(0.35, 0.65, N.g));',
                            [('C', base, ''), ('A', g.const3(ha, -300), ''), ('B', g.const3(hb, -300), ''), ('N', n, 'RGB')], F3, 100)
        return {'name': lname, 'base': (base, ''), 'nor': (nor, 'RGB'), 'rough': (arm, 'G'), 'ao': (arm, 'R'), 'disp': (disp, 'R')}

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

    layers = []
    for kind, lname, sid, tile, tint, extra in specs:
        if kind == 'rock':
            layers.append(rock_layer(lname, S[sid], tile, tint, extra))
        elif kind == 'asphalt':
            layers.append(asphalt_layer(lname, S[sid], tile, tint, extra))
        else:
            layers.append(layer(lname, S[sid], tile, tint, extra))
    for channel, prop, y in (('base', unreal.MaterialProperty.MP_BASE_COLOR, 0),
                             ('nor', unreal.MaterialProperty.MP_NORMAL, 600),
                             ('rough', unreal.MaterialProperty.MP_ROUGHNESS, 1200),
                             ('ao', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 1800)):
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
        g.out(blend, '', prop)
    return g.finish(name)


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
    c = g.custom('\n'.join([
        'float3 base = C * T;',
        'base = lerp(base, dot(base, float3(0.3, 0.59, 0.11)).xxx, Desat);',
        'float up = saturate((VN.z + (Nt.y * 2.0 - 1.0) * 0.25 - 0.45) * 3.5);',
        'float s = Snow * up;',
        'Rough = lerp(Ar.g, 0.65, s);',
        'return lerp(base, float3(0.82, 0.85, 0.9), s);']),
        [('C', col, 'RGB'), ('T', tint, ''), ('VN', wn, ''), ('Nt', nor, 'RGB'), ('Snow', snow, ''), ('Ar', arm, 'RGB'), ('Desat', desat, '')],
        F3, -300, extra=[('Rough', F1)])
    g.out(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
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


def model(aid, sname, plant=False, nanite=True, tint=(1.0, 1.0, 1.0), snow=0.0, green=False, desat=0.0):
    """Modelo do Poly Haven: FBX (importado uma vez por execução) + material da
    fatia (cor, normal, ARM, alfa, tom, neve). Devolve (malha, materiais): o
    material entra só no componente instanciado da fatia, então a mesma malha
    serve ao deserto, à neve e à floresta sem uma pisar na outra."""
    src = os.path.join(ASSETS, aid)
    files = [os.path.join(src, f) for f in os.listdir(src)]
    folder = DEST + '/Models/' + aid
    if aid not in _MESHES:
        objs = import_file(find(files, '.fbx'), folder)
        meshes = [o for o in objs if isinstance(o, unreal.StaticMesh)]
        if not meshes:
            meshes = [unreal.load_asset(a) for a in eal.list_assets(folder, recursive=False)
                      if isinstance(unreal.load_asset(a), unreal.StaticMesh)]
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

def decals_along(points, material, half_len_cm, half_width_cm, offsets_cm, every=1, dash=None, skip_road=False):
    """Decalques ao longo de uma linha (cm): um por ponto (a cada `every`),
    deslocados de lado (offsets), virados ao longo dela. `dash` = (liga, período)
    em pontos, pra faixas tracejadas."""
    n = 0
    for i in range(1, len(points) - 1, every):
        if dash and (i % dash[1]) >= dash[0]:
            continue
        a, b = points[i - 1], points[i + 1]
        dx, dy = b.x - a.x, b.y - a.y
        ln = math.hypot(dx, dy) or 1.0
        dx, dy = dx / ln, dy / ln
        yaw = math.degrees(math.atan2(dy, dx))
        for off in offsets_cm:
            loc = unreal.Vector(points[i].x - dy * off, points[i].y + dx * off, points[i].z)
            if skip_road and lib.slice_road_distance(unreal.Vector2D(loc.x, loc.y)) < 420.0:
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


def road_paint(paint_mat, road_half_m):
    """Tracejado central (3 m pintados a cada 12 m) e faixas de borda contínuas,
    por trecho de asfalto: sem tinta nos entroncamentos nem nas pontas que se
    desfazem em terra (C++, SliceRoad com bMarksOnly)."""
    n = 0
    edge = (road_half_m - 0.35) * 100.0
    for pts in runs(lib.slice_road(1.0, True)):
        if len(pts) < 3:
            continue
        n += decals_along(pts, paint_mat, 150.0, 7.0, (0.0,), every=1, dash=(1, 12))
        n += decals_along(pts, paint_mat, 110.0, 6.0, (-edge, edge), every=2)
    log('road paint decals', n)


def water_plane():
    """Lago (floresta): o material translúcido do app, na cota d'água do gerador."""
    z = lib.slice_water_level()
    if z < -1e8:
        return
    mat = unreal.load_asset('/Game/Materials/M_Water')
    plane = unreal.load_asset('/Engine/BasicShapes/Plane.Plane')
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, z), unreal.Rotator(0, 0, 0))
    a.set_actor_label('Water')
    a.set_actor_scale3d(unreal.Vector(260, 260, 1))
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
                lib.add_instances(group, mesh, sc.transforms, True, cull, mats)
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
    ground = ground_material('M_SliceGround_' + tag, S, layer_specs, cracks)
    tt = os.path.join(ASSETS, 'TireTracks001')
    tfiles = [os.path.join(tt, f) for f in os.listdir(tt)]
    decal = decal_material('M_TireTrack_' + tag,
                           {'color': texture(find(tfiles, '_Color'), DEST + '/Decals', 'color'),
                            'rough': texture(find(tfiles, '_Roughness'), DEST + '/Decals', 'masks'),
                            'disp': texture(find(tfiles, '_Displacement'), DEST + '/Decals', 'masks')}, cfg['track_color'])
    snow, rt, ct, ds = cfg['snow'], cfg['rock_tint'], cfg['cliff_tint'], cfg.get('rock_desat', 0.0)
    meshes = {
        'boulders_big': [model(a, tag, tint=rt, snow=snow, desat=ds) for a in ('namaqualand_boulder_02', 'namaqualand_boulder_03', 'namaqualand_boulder_04')],
        'boulders_small': [model(a, tag, tint=rt, snow=snow, desat=ds) for a in ('namaqualand_boulder_05', 'namaqualand_boulder_06',
                                                                                   'namaqualand_boulders_01', 'namaqualand_rocks_01')],
        'pebbles': [model('namaqualand_stones_01', tag, tint=rt, snow=snow, desat=ds)],
        'cliffs': [model('namaqualand_cliff_01', tag, tint=ct, snow=snow, desat=ds), model('namaqualand_cliff_02', tag, tint=ct, snow=snow, desat=ds)],
        'branches': [model('dry_branches_medium_01', tag, plant=True, nanite=False, snow=snow)],
        'tufts_dry': [model('grass_medium_01', tag, plant=True, nanite=False, snow=snow),
                      model('grass_medium_02', tag, plant=True, nanite=False, snow=snow)],
        'shrubs': [model('wild_rooibos_bush', tag, plant=True, nanite=False), model('searsia_lucida', tag, plant=True, nanite=False)],
    }
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
    layers = [layer_info(tag, s[1]) for s in layer_specs]

    if eal.does_asset_exist(cfg['map']):
        eal.delete_asset(cfg['map'])
    levels.new_level(cfg['map'])
    environment(cfg['light'])
    c = unreal.Vector2D(1e9, 1e9) if center is None else unreal.Vector2D(center[0], center[1])
    landscape, err = lib.build_slice_landscape(cfg['seed'], 2, cfg['biome'], 1024.0, c, grid[0], grid[1], grid[2],
                                               ground, layers, cfg['road'])
    if err:
        log('landscape error:', err)
    for road in runs(lib.slice_road(1.0)):
        mid = road[len(road) // 2]
        # pra vista da estrada nas fotos
        log('road run %d m: middle %.0f %.0f, ends %.0f %.0f / %.0f %.0f' % (len(road), mid.x / 100.0, mid.y / 100.0,
            road[0].x / 100.0, road[0].y / 100.0, road[-1].x / 100.0, road[-1].y / 100.0))
    scatter(cfg, meshes)
    tire_tracks(decal)
    if cfg['road'] > 0 and cfg.get('paint', True):
        road_paint(paint_material(cracks), cfg['road'] * 0.5)
    if cfg['road'] > 0:
        roadside(meshes, cfg['road'] * 0.5)
    if cfg.get('water'):
        water_plane()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.ForgeSliceGameMode)
    levels.save_current_level()
    eal.save_directory('/Game/', only_if_is_dirty=True, recursive=True)
    log('slice saved', tag, cfg['map'])


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
    cfg = dict(SLICES[sname])
    with open(path, encoding='utf-8') as f:
        cfg['map'] = json.load(f).get('map', '/Game/Maps/World_Full')
    comps, quads = 16, 127
    log('world', sname, 'size', size, 'quad %.3f m' % (size / (comps * quads)), cfg['map'])
    # materiais e camadas por bioma (as prévias guardadas de biomas diferentes não se pisam)
    build(sname, tag='world_' + sname, cfg=cfg, grid=(comps, quads, size / (comps * quads)), center=(0.0, 0.0))
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


# SLICE=desert,snow,forest (padrão: desert); SLICE=world monta o mapa do app
# (WORLD_SETTINGS = o JSON dos controles); SLICE=none só carrega as funções
for _s in os.environ.get('SLICE', 'desert').split(','):
    if _s in SLICES:
        build(_s)
    elif _s == 'world':
        build_world(os.environ['WORLD_SETTINGS'])
