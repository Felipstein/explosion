"""A planta da cena dos tanques no deserto aberto (LookLab, 13/10): a referência que o Felipe mandou na conversa
(tanques oliva no deserto aberto, um explodindo com a torre voando; ondulação de vento por todo o chão, trilhas de
esteira cruzando, o chão queimado com brasa em volta da carcaça, coluna de fumaça preta, sol baixo dourado).

Pedido dele: a mesma cena "pica" no nível da cena de perto (tanque + Dozer), pra a sessão das explosões testar a
explosão num cenário perto dos game shots. A imagem veio só na conversa (não ficou salva): a câmera e as posições são
lidas a olho dela (o tanque de 7,9 m como régua). O efeito (a bola, a fumaça, a torre voando) é da sessão das explosões;
aqui fica o chão queimado com as brasas, a carcaça e a luz.
"""
MAP_PREFIX = 'LL_DesertTanks'
VARIANTS = {'Tanks': 'desert_tanks'}

# a câmera: alta, olhando pra baixo ~40°, um campo médio (cabem ~55 m de largura no meio da tela)
# volta 2 (13/10): mais perto, ~33 m de largura no foco (7, −2) (a 1ª, com 60 m, perdia a ondulação e o detalhe da cena de perto)
CAM = {'x': -25.7, 'y': -38.3, 'z': 38.2, 'yaw': 48.0, 'pitch': -38.0, 'hfov': 30.0}
ART = 'docs/concept/scenes/desert_golden_hour/best/close_ground_light_gemini.webp'     # (sem arquivo da referência: só pro look_match não quebrar)
EXCLUDE = []
GROUND_REGION = (-60.0, -60.0, 120.0)
SCATTER_REGION = (-55, 55, -55, 55)
ROAD = {'kind': 'straight', 'line': [(-900.0, 900.0), (900.0, 900.0)], 'half': 4.0}     # sem rua
SAND_ALBEDO = (0.49, 0.4, 0.27)
SAND_RIPPLE_TEX = 1.0
WIND_YAW = 173.0        # o vento a 155° do sol (o sol a 48 − 80 = −32°: o vento sopra pra −187° = 173°), as cristas riscam com o sol de lado (volta 2)
SAND_RIPPLES = 0.0
GROUND_TINT = {'sand': (1.0, 1.0, 1.0)}
# o deserto aberto: a ondulação domina (a referência: quase todo o chão), o liso e o pedregoso em manchas
ZONES = {'smooth': (0.62, 0.72), 'gravel': (0.66, 0.76), 'ripple': (0.40, 0.50), 'stir': (0.70, 0.78)}
HOUSES = {}
YARDS = {}
WALL_MESHES = {'W6': (6.0, 2.3), 'W10': (10.0, 2.2)}
WALLS = []

# os tanques oliva (o da referência é o T-55 da China: oliva escuro, sem camuflagem), andando em diagonal pela tela;
# o do meio é a carcaça (queimada) onde a explosão acontece
# volta 2: em linha, andando em diagonal pela tela (rumo 172°, ~55° fora do eixo da câmera), a carcaça no meio da linha
TANKS = [(7.0, 12.2, 175.0, 'olive'), (10.7, 2.5, 170.0, 'olive'), (9.3, -14.4, 173.0, 'olive'),
         (21.2, -2.0, 168.0, 'olive'), (5.6, -4.8, 160.0, 'burnt')]
WX, WY = 5.6, -4.8      # a carcaça
DOZERS = []
PALMS = []
SHRUBS = []
POLES = []
EXPLOSIONS = []
WRECKS = []
LIGHTS = []
PROPS = []
# as brasas e o queimado em volta da carcaça (o efeito de look-dev do look_lab.night_fx: chamas pequenas, luz quente,
# queimado e destroços carbonizados): (x, y, z da base, tamanho das chamas, candelas, raio)
# (volta 2: a luz 4× mais fraca: com 0,5 Mcd o casco saía laranja em vez de queimado)
FIRES = [(WX, WY, 1.2, 0.8, 0.05e6, 5.0), (WX + 2.5, WY - 3.5, 0.1, 0.35, 0.03e6, 3.0), (WX - 3.5, WY + 2.5, 0.1, 0.3, 0.025e6, 3.0),
         (WX + 4.0, WY + 2.0, 0.1, 0.25, 0.02e6, 2.5)]
BLAST = (0, 0, 0, 0, 0, 0)
SMOKE = []
SCORCH = [(WX, WY, 7.0), (WX + 3.5, WY - 4.0, 3.0), (WX - 4.5, WY + 3.0, 2.6), (WX + 5.0, WY + 2.5, 2.2)]
TRACERS = []
FLARE = None


def debris():
    """Os pedaços carbonizados em volta da carcaça (semente fixa)."""
    import math
    import random
    r = random.Random(41)
    out = []
    for _ in range(32):       # (volta 2: eram 70 e liam como pontilhado preto)
        a = r.uniform(0, 6.283); d = 8.0 * r.random() ** 0.7
        out.append((WX + d * math.cos(a), WY + d * math.sin(a), r.uniform(0, 360), r.uniform(0.8, 2.5), r.randint(0, 9)))
    return out


DEBRIS = debris()

TRACKS_FROM_TANKS = True
TRACK_RELIEF = True
# as trilhas cruzando o chão em várias direções e idades (a referência: linhas retas e curvas, umas por cima das outras)
TRACKS_EXTRA = [([(-50.0 + 2.0 * i, 30.0 - 1.4 * i) for i in range(50)], 0.15),
                ([(-45.0 + 1.9 * i, 45.0 - 1.9 * i) for i in range(50)], 0.45),
                ([(-20.0 + 1.5 * i, -50.0 + 1.7 * i + 0.01 * i * i) for i in range(50)], 0.7),
                ([(10.0 + 1.2 * i, -50.0 + 2.0 * i) for i in range(45)], 0.3)]
SCATTER_DENSITY = 0.7
PEBBLE_FIELD = 0.08
PEBBLE_SCALE = (0.9, 2.2)
BUSH_FRACTION = 0.02
TWIG_FIELD = 0.03
SCATTER_TINT = {'grass_medium_01': (2.3, 2.0, 1.25, 0.15), 'dry_branches_medium_01': (2.0, 1.85, 1.6, 0.3)}

SCATTER = {
    'tuft': [f'grass_medium_01/grass_medium_01_{v}_LOD0' for v in
             ('large_a', 'large_b', 'large_c', 'mid_a', 'mid_b', 'tall_a', 'tall_b', 'tall_c')],
    'pebble': [f'namaqualand_stones_01/namaqualand_stones_01_{v}_LOD0' for v in 'abcde'],
    'bush': [f'wild_rooibos_bush/wild_rooibos_bush_{v}' for v in 'abcde'],
    'twig': [f'dry_branches_medium_01/dry_branches_medium_01_{v}' for v in 'ab'],
}
SCATTER_MI = {'grass_medium_01': 'MI_grass_medium_01_desert', 'namaqualand_stones_01': 'MI_namaqualand_stones_01_desert',
              'wild_rooibos_bush': 'MI_wild_rooibos_bush_desert', 'dry_branches_medium_01': 'MI_dry_branches_medium_01_desert'}


def footprints():
    return []
