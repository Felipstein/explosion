"""A planta da maquete do look (LookLab): ruas, casas e muros, num lugar só.

Sem Unreal e sem Blender (só dados): lido pelo look_lab.py (a cena, dentro da Unreal),
pelo look_houses.py (as casas, no Blender) e pelo look_ground.py (o chão em relevo).

Medida na arte desert_golden_hour/best/day_game_cam.webp: a câmera desentortada pra
vista de cima e posta na escala do nosso tanque (o casco da arte = 7,9 m). Metros, eixos
da Unreal: X ao longo da rua dos tanques, Y pra direita, Z pra cima.
"""
K = 1.25    # a primeira medida (câmera a 21 m) × K = a escala do tanque (câmera a 26,25 m)

# a câmera da arte (medida: 29° de inclinação, 65° de campo horizontal, giro de 45° das ruas)
CAM = {'x': -31.53 * K, 'y': 26.66 * K, 'z': 21.0 * K, 'yaw': -45.33, 'pitch': -29.0, 'hfov': 65.0}
ART = 'docs/concept/scenes/desert_golden_hour/best/day_game_cam.webp'
# as variantes (mapa -> receita): o jogo de hoje, a receita do dourado e, só pra comparar de perto, o mesmo
# dourado com o sol da arte close_game_cam_gpt (a câmera vem na receita)
VARIANTS = {'Before': 'game_today', 'Golden': 'desert_golden_hour', 'Close': 'desert_golden_close'}
# o que a arte tem e a maquete não (a explosão e a poeira dela): fora das contas do look_match
EXCLUDE = [(200, 40, 660, 330)]
# o chão em relevo: a região (x0, y0, lado) em metros
GROUND_REGION = (-75.0, -170.0, 230.0)
# o asfalto cinza velho, mais escuro que a areia (crítica da sessão de arte, 11/10: lia como concreto claro)
# a areia (estudo de 12/10): o albedo em linear, bege-amarelado, luminância 0,37 (a areia de deserto da tabela PBR
# da Epic); a textura só dá o desenho. A cor da arte já vem acesa pelo sol laranja: não se copia pro albedo.
SAND_ALBEDO = (0.49, 0.4, 0.27)    # (0,44/0,36/0,24 da tabela saiu 20% escura contra a arte de perto)
SAND_RIPPLE_TEX = 1.0   # as ondulações simuladas (look_ripples.py), em manchas e embaralhadas
WIND_YAW = -158.3       # pra onde o vento sopra (graus): o sol fica do lado de onde vem o vento (155°), as cristas riscam
SAND_RIPPLES = 0.0      # (a ondulação de seno do shader: desligada)
# (antes: SAND_RIPPLES = 0.25     # fracas: na arte de perto a areia é pisada, quase sem ondulação regular (1,0 virou listras)
GROUND_TINT = {'asphalt': (0.8, 0.84, 0.9), 'sand': (1.0, 1.0, 1.0)}   # areia menos vermelha: o alvo das sete e da arte de perto (matiz 29–30°, 12/10)
WRECKS = []        # carcaças queimando (a noite tem)
LIGHTS = []        # luzes pontuais da cena (fogo, sinalizador)

ROAD = {
    'b_half': 6.5 * K,              # a transversal (ao longo de Y), centro em X = 0
    'b_end': -12.0,                 # daqui pra cima ela vira a rua de cima (estreita, depois terra)
    'a_left': (0.0, 7.5 * K),       # a rua dos tanques (X < 0): centro em Y, meia largura
    'a_right': (-4.5 * K, 8.5 * K), # a rua das picapes (X > 0): desencontrada como na arte
    'corner': 4.0 * K,              # raio do meio-fio arredondado das esquinas
    # a rua de cima: passa pela explosão (4, −19), a faixa clara no alto da arte (26, −71) e (38, −98)
    'upper': [(0.0, -12.0), (6.0, -28.0), (12.0, -50.0), (20.0, -70.0), (34.0, -95.0), (48.0, -135.0)],
    'upper_half': 5.0,              # meia largura dela depois do estreitamento
    'dirt_from': -26.0,             # o asfalto dela acaba entre −26 e −50 (vira terra batida)
    'dirt_to': -50.0,
}

# As casas: centro, planta, altura do topo da mureta, lado da porta e os detalhes.
HOUSES = {
    'H11': dict(x=-13.85, y=20.3, sx=7.7, sy=7.8, h=4.5, door='+Y', ex={'roofbox': (-1, -1), 'beams': '+Y', 'seed': 11}),
    'H1': dict(x=-6.95, y=-23.75, sx=11.9, sy=14.1, h=4.5, door='+Y', ex={'roofbox': (1, 1), 'tank': (-1, 1), 'seed': 1}),
    'H2': dict(x=-0.15, y=-51.2, sx=7.3, sy=9.8, h=7.5, door='+Y', ex={'seed': 2, 'stories': 2}),
    'H2b': dict(x=-8.4, y=-49.75, sx=8.8, sy=12.5, h=4.5, door='+Y', ex={'beams': '-X', 'seed': 3}),
    'H3': dict(x=-2.8, y=-86.4, sx=8.0, sy=6.2, h=5.0, door='+Y', ex={'seed': 4}),
    'H4': dict(x=-27.8, y=-16.75, sx=12.4, sy=10.3, h=4.5, door='+Y', ex={'canopy': '+Y', 'roofbox': (-1, 1), 'seed': 5}),
    'H5': dict(x=26.3, y=-51.5, sx=8.4, sy=12.0, h=7.5, door='+Y', ex={'seed': 6, 'stories': 2, 'tank': (1, -1)}),
    'H6': dict(x=33.1, y=-39.5, sx=9.6, sy=10.4, h=4.5, door='+Y', ex={'roofbox': (1, -1), 'seed': 7}),
    'H7': dict(x=67.35, y=-25.35, sx=12.5, sy=10.5, h=5.0, door='+Y', ex={'seed': 8, 'tank': (-1, -1)}),
    'H8': dict(x=42.9, y=-20.35, sx=11.0, sy=6.7, h=4.5, door='+Y', ex={'beams': '+Y', 'seed': 9}),
    'H10': dict(x=33.0, y=20.3, sx=10.8, sy=11.4, h=6.5, door='-X', ex={'seed': 10, 'stories': 2, 'roofbox': (1, 1)}),
    'H12': dict(x=-26.0, y=19.5, sx=7.0, sy=7.0, h=4.5, door='+Y', ex={'seed': 12, 'tank': (1, 1)}),
    # a fileira do fundo que a arte corta no alto (medidas em 11/10, o pedido "mais casas" do Felipe):
    # a casa grande entre a H6 e a H7 e a da direita da H7
    'H13': dict(x=70.4, y=-45.1, sx=10.0, sy=9.0, h=5.0, door='+Y', ex={'seed': 13, 'roofbox': (-1, 1)}),
    'H14': dict(x=98.9, y=-17.0, sx=8.0, sy=7.0, h=4.5, door='+Y', ex={'seed': 14, 'tank': (1, -1)}),
}
YARDS = {   # quintal murado com um quartinho
    'Y9': dict(x=26.65, y=-20.95, sx=10.1, sy=7.9, h=2.4, ex={'seed': 19}),
    'Y13': dict(x=26.8, y=-72.7, sx=7.0, sy=6.0, h=2.3, ex={'seed': 23}),     # o quintal das duas palmeiras do fundo
}
WALL_MESHES = {'W6': (6.0, 2.3), 'W10': (10.0, 2.2)}     # trecho de muro: (comprimento ao longo de X, altura)
WALLS = [  # (malha, x, y, giro, escala ao longo)
    ('W6', -20.1, 16.6, 0.0, 0.8),
    ('W10', -28.8, -10.3, 0.0, 1.44),
    ('W10', -17.3, -12.6, 0.0, 0.86),
    ('W6', -13.4, -14.6, 90.0, 0.7),
]


# o que pousa no chão (a altura vem do relevo: look_ground.py escreve placements.json)
# medidos na arte na altura do casco (11/10): duas colunas, de frente pro cruzamento; na arte são
# 2 tanques (T1, T4) e 3 blindados de rodas (T2, T3, T5): a maquete põe o tanque em todos
TANKS = [(-16.65, 3.42), (-21.71, -1.58), (-20.18, 8.89), (-30.39, 1.08), (-29.29, 7.45),
         # as outras facções (crítica da sessão de arte, 11/10: o deserto apaga o GLA, medido):
         # o tanque da China (oliva) e as cinco picapes técnicas do GLA (medidas na arte em 11/10: a frente
         # e a traseira de cada uma, desprojetadas na altura da carroceria)
         (13.4, 14.8, -109.0, 'china'),
         (20.06, -22.93, 119.4, 'gla'), (15.89, -13.12, 98.4, 'gla'), (20.11, -5.42, -177.8, 'gla'),
         (25.42, -0.12, -164.7, 'gla'), (20.2, 6.54, -153.6, 'gla')]
PALMS = [  # (x, y, malha, escala, giro)
    (-11.3, 12.0, 'SM_DatePalm_OasisCluster', 0.6, 30.0),
    (-8.1, 30.9, 'SM_DatePalm_Mature', 1.0, 75.0),
    # as duas altas do canto de cima (copa a ~10 m na arte) e a baixa atrás da casa H4, com o pé escondido
    # pela mureta: a copa dela (a ~5,5 m) faz a sombra recortada no telhado da H4 (pedido do Felipe, 11/10)
    (-18.6, -45.1, 'SM_DatePalm_Mature', 0.8, 10.0),
    (-17.2, -42.4, 'SM_DatePalm_Urban', 0.62, 140.0),
    (-21.6, -24.5, 'SM_DatePalm_OasisCluster', 0.85, 200.0),
    # a sombra recortada no telhado da H4: na arte a sombra da palmeira não fecha com o sol do chão (a IA
    # desenhou as duas de cabeça); com o nosso sol (baixo, vindo de +X) ela só cai no telhado se a copa
    # estiver ~8 m acima e ao lado direito da casa: a palmeira entre a H4 e a H1, atrás do muro
    (-17.4, -17.5, 'SM_DatePalm_Mature', 0.7, 300.0),
    (22.8, -72.1, 'SM_DatePalm_Mature', 0.95, 40.0), (25.8, -70.5, 'SM_DatePalm_Urban', 0.7, 260.0),   # as duas finas do fundo
    (50.0, -40.6, 'SM_DatePalm_Mature', 1.0, 200.0),
    (43.0, -32.0, 'SM_DatePalm_OasisCluster', 0.8, 90.0),
]
SHRUBS = [  # (x, y, malha, escala, giro)
    (-15.5, 11.8, 'searsia_lucida/searsia_lucida_b_LOD0', 1.3, 0.0),
    (-9.0, 15.0, 'wild_rooibos_bush/wild_rooibos_bush_a', 1.5, 60.0),
    (9.9, 12.4, 'wild_rooibos_bush/wild_rooibos_bush_c', 1.4, 200.0),
    (-9.3, 25.5, 'searsia_lucida/searsia_lucida_e_LOD0', 1.2, 120.0),
    (-15.1, -12.0, 'wild_rooibos_bush/wild_rooibos_bush_b', 1.6, 300.0),
    (15.6, 16.9, 'dry_branches_medium_01/dry_branches_medium_01_a', 1.3, 30.0),
    (-19.4, 13.75, 'dry_branches_medium_01/dry_branches_medium_01_b', 1.3, 250.0),
    (18.75, 10.0, 'namaqualand_stones_01/namaqualand_stones_01_a_LOD0', 1.5, 0.0),
    (-8.5, 21.0, 'namaqualand_stones_01/namaqualand_stones_01_c_LOD0', 1.5, 80.0),
    (24.0, 8.5, 'searsia_lucida/searsia_lucida_c_LOD0', 1.2, 10.0),
    (-36.0, -12.5, 'searsia_lucida/searsia_lucida_a_LOD0', 1.3, 200.0),
]
POLES = [(9.0, 22.3, 0.0), (59.4, -17.8, 0.0), (90.4, -17.8, 0.0)]
# as miudezas da arte em volta das casas (look_tank.py, slots com os materiais das casas): (malha, x, y, giro)
# medidas na arte em 11/10: os caixotes na frente da H1 e da H11, as caixas do quintal do GLA, a munição
# encostada no muro da H4 e os tambores
PROPS = [('Crates', -14.0, -21.7, 10.0), ('Crates', 21.8, -13.5, -30.0), ('Ammo', -33.2, -7.3, 15.0),
         ('Crates', -14.9, 13.2, 60.0), ('Barrels', 14.9, 20.0, 0.0), ('Barrels', -11.4, -14.8, 30.0),
         ('Ammo', 24.0, -16.0, -10.0)]

# A explosão da arte (pedido do Felipe, 11/10): a existente que mais combina, a Carga Pesada do
# BlastLab (o raymarch aprovado em 08/10, as 3 gravações DA_HeavyCharge_v0..v2 do jogo), parada
# no instante em que a arte mostra: bola de fogo ainda acesa e a coluna escura subindo.
# (x, y, gravação, idade em s, tamanho W^(1/3), semente)
EXPLOSIONS = [(3.5, -15.5, 'DA_HeavyCharge_v2', 0.5, 1.0, 0.37)]

# o que se espalha sozinho (look_ground.py sorteia pela planta): malha, pasta e o material do deserto
SCATTER = {
    # as malhas escaneadas estão em tamanho de foto (tufo de 15–30 cm, pedra de 8 cm): o look_ground
    # escala pra o que as artes mostram (tufo de 0,6–1 m, entulho de 30–60 cm)
    'tuft': [f'grass_medium_01/grass_medium_01_{v}_LOD0' for v in
             ('large_a', 'large_b', 'large_c', 'mid_a', 'mid_b', 'tall_a', 'tall_b', 'tall_c')],
    'pebble': [f'namaqualand_stones_01/namaqualand_stones_01_{v}_LOD0' for v in 'abcde'],
    # as touceiras secas escuras das artes (o tufo de capim sozinho some na areia)
    'bush': [f'wild_rooibos_bush/wild_rooibos_bush_{v}' for v in 'abcde']
            + [f'searsia_lucida/searsia_lucida_{v}_LOD0' for v in 'abcdefg'],
}
SCATTER_MI = {'grass_medium_01': 'MI_grass_medium_01_desert', 'namaqualand_stones_01': 'MI_namaqualand_stones_01_desert',
              'wild_rooibos_bush': 'MI_wild_rooibos_bush_desert', 'searsia_lucida': 'MI_searsia_lucida_desert'}


def footprints():
    """Retângulos no chão (x0, x1, y0, y1) de tudo que tem parede: casas, quintais e muros."""
    out = []
    for d in list(HOUSES.values()) + list(YARDS.values()):
        out.append((d['x'] - d['sx'] / 2, d['x'] + d['sx'] / 2, d['y'] - d['sy'] / 2, d['y'] + d['sy'] / 2))
    for mesh, x, y, yaw, sc in WALLS:
        ln = WALL_MESHES[mesh][0] * sc
        hx, hy = (ln / 2, 0.18) if abs(yaw) < 45 else (0.18, ln / 2)
        out.append((x - hx, x + hx, y - hy, y + hy))
    return out
