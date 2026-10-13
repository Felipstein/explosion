"""A planta da maquete da noite (LookLab): a arte desert_night/night_game_cam.webp.

Sem Unreal e sem Blender (só dados), lida por look_lab.py, look_houses.py, look_ground.py e
look_match.py quando LOOK_SCENE=desert_night (Tools/look_scene.py).

A câmera saiu dos pontos de fuga (11/10): as linhas da estrada e as que cruzam fecham em ângulo
reto com a câmera sem rolar, a 38° de inclinação, 66° de campo horizontal e girada 45° da
estrada (o giro da câmera do jogo). A altura, 31 m, saiu do casco dos tanques (7,9 m). É a
câmera do jogo no zoom perto (31,6 m a 37,5°), com a lente mais aberta.
Metros, eixos da Unreal: X ao longo da estrada (pro fundo, à esquerda na tela), Y à direita.
Tudo foi medido na arte desentortada pra vista de cima, na altura certa de cada peça (o casco a
1,4 m, o telhado a 4 m, o chão a 0).
"""
MAP_PREFIX = 'LL_DesertNight'
VARIANTS = {'Night': 'desert_night'}

CAM = {'x': 0.0, 'y': 0.0, 'z': 31.0, 'yaw': 45.0, 'pitch': -38.0, 'hfov': 66.0}
ART = 'docs/concept/scenes/desert_night/night_game_cam.webp'
# o que a arte tem e a maquete não: helicópteros, soldados, o paraquedas em si e as faíscas
EXCLUDE = [(60, 100, 210, 165), (225, 55, 330, 115), (840, 185, 1010, 265), (600, 140, 820, 280),
           (790, 80, 880, 175), (430, 230, 660, 345)]
GROUND_REGION = (-15.0, -15.0, 135.0)
SCATTER_REGION = (-8, 85, -8, 75)
# a noite (refeita em 11/10 depois da crítica do Felipe e da sessão de arte): quem ilumina a batalha são as
# fontes locais (fogo, sinalizador, explosão); entre elas o chão cai pro escuro azul; a lua só dá o brilho frio
# de topo e de borda. Medido na arte, longe das fontes: areia Y 0,12–0,22, azul (r 0,12–0,18, b 0,19–0,30).
# A areia laranja vira cinza no material só à noite (a cor vem da luz).
# o chão azul-petróleo da arte (medido por região, 11/10): metade do azul no material, metade na lua; assim o fogo e o
# sinalizador ainda mostram a areia quente embaixo deles (com o azul todo no material, a poça do sinalizador saía gelo)
GROUND_TINT = {'sand': (0.36, 0.56, 0.82), 'asphalt': (0.38, 0.52, 0.62)}   # (−10%: a crítica pediu mais escuro; −22% passou)
HOUSE_TINT = (1.25, 1.18, 1.0)      # o reboco à noite: a lua azul-ciano deixa a parede azul demais (b* −17 contra −8) e escura
SCATTER_DENSITY = 0.4               # a areia da arte à noite é limpa: poucos tufos e pedrinhas
# os telhados da arte da noite são bem mais escuros que a areia acesa (L* 26 contra 44): o concreto escuro
HOUSE_ROOF = (0.85, (0.09, 0.088, 0.085))   # medido 11/10: a rua bem mais escura que a areia

# a estrada: uma reta só (duas faixas, tracejado no meio e as linhas da borda)
ROAD = {'kind': 'straight', 'line': [(-80.0, 22.6), (180.0, 22.6)], 'half': 4.5}   # medido nas linhas da arte: y de 18 a 27

# as casas (giradas como na arte): telhado medido a 4 m; altura do topo da mureta
HOUSES = {
    'N1': dict(x=40.5, y=48.8, sx=10.2, sy=8.6, h=4.6, yaw=-38.0, door='+Y', ex={'roofbox': (1, -1), 'seed': 21}),
    'N2': dict(x=40.6, y=65.0, sx=11.5, sy=7.8, h=4.6, yaw=-34.0, door='+Y', ex={'seed': 22, 'tank': (-1, 1)}),
    'N3': dict(x=8.5, y=46.7, sx=9.5, sy=7.2, h=4.4, yaw=-19.0, door='-X', ex={'seed': 23, 'beams': '+Y'}),
    # a fileira de cima da arte (medida no telhado a 4 m)
    'N4': dict(x=68.1, y=41.8, sx=10.2, sy=6.5, h=4.6, yaw=-53.0, door='+Y', ex={'seed': 24}),
    'N5': dict(x=58.6, y=47.4, sx=13.3, sy=10.0, h=4.8, yaw=-57.0, door='+Y', ex={'seed': 25, 'roofbox': (-1, 1)}),
    'N6': dict(x=34.6, y=76.7, sx=9.3, sy=8.1, h=4.6, yaw=-49.0, door='+Y', ex={'seed': 26, 'tank': (1, 1)}),
}
YARDS = {}
WALL_MESHES = {'W6': (6.0, 2.3), 'W10': (10.0, 2.2)}
WALLS = [  # (malha, x, y, giro, escala ao longo): o muro comprido do quintal de cima e o canto de baixo
    ('W10', 33.0, 42.3, -5.0, 1.0), ('W10', 43.0, 41.4, -5.0, 1.0), ('W10', 53.0, 40.5, -5.0, 1.0),
    ('W10', 63.0, 39.6, -5.0, 1.0), ('W10', 72.5, 38.8, -5.0, 0.9),
    ('W6', 18.5, 9.0, 60.0, 0.9),
]

# os veículos: (x, y, giro, tinta). Na arte a USA é cor de areia e a China verde-oliva; o
# tanque do meio (USA) está pegando fogo. Na arte há também blindados de rodas: a maquete
# põe o tanque em todos.
TANKS = [
    (47.0, 21.9, 163.0, 'usa_night'), (40.4, 18.2, 156.0, 'usa_night'), (44.0, 8.6, 154.0, 'usa_night'),
    (34.2, 13.4, 155.0, 'usa_night'), (38.0, 6.2, 154.0, 'usa_night'),
    (14.7, 36.8, -20.0, 'china_night'), (12.4, 31.3, -15.0, 'china_night'), (7.3, 37.1, -12.0, 'china_night'),
    (9.2, 26.3, -16.0, 'china_night'), (3.7, 31.3, -10.0, 'china_night'),
    (23.8, 26.2, 158.0, 'usa_night'),
]
PALMS = [
    (64.0, 54.0, 'SM_DatePalm_Mature', 1.0, 30.0), (52.0, 78.0, 'SM_DatePalm_Mature', 1.1, 80.0),
    (24.0, 76.0, 'SM_DatePalm_Urban', 1.0, 150.0), (20.0, 84.0, 'SM_DatePalm_Mature', 1.0, 200.0),
    (-4.0, 8.0, 'SM_DatePalm_OasisCluster', 0.8, 20.0),
]
SHRUBS = []
POLES = [(30.0, 36.0, 0.0)]
# as miudezas (look_tank.py): os sacos de areia da China na frente da linha, os caixotes e tambores junto das casas
# e do muro quebrado, a munição atrás dos tanques
PROPS = [('Sandbags', 18.0, 33.0, 70.0), ('Sandbags', 15.5, 25.0, 95.0), ('Crates', 31.5, 45.5, 10.0),
         ('Barrels', 46.5, 44.5, 0.0), ('Ammo', 9.0, 41.0, 20.0), ('Crates', 4.5, 43.5, -15.0),
         ('Ammo', 50.0, 17.0, -30.0), ('Barrels', 52.0, 26.0, 40.0)]
TRACKS_FROM_TANKS = True     # as marcas de esteira na areia atrás de cada tanque (look_ground.py)

# A noite não usa a Carga Pesada (saía pálida): os efeitos são de look-dev da sessão de gráficos, só pra a
# maquete ter a luz e a cor certas (o efeito final continua com as sessões de explosão).
EXPLOSIONS = []
WRECKS = [  # (x, y, giro, escala): o carro queimado (look_tank.py)
    (42.0, 30.6, 20.0, 1.0), (22.0, 15.4, 70.0, 1.0), (15.3, 16.0, 110.0, 1.0),
]
# os fogos: (x, y, z da base, tamanho das chamas em m, candelas da luz, raio da luz em m). As carcaças, o tanque
# do meio, a porta da casa N1 e uma brasa no campo de destroços: ~5 poças laranja como na arte
FIRES = [
    (42.0, 30.6, 0.9, 2.0, 1.2e6, 11.0), (22.0, 15.4, 0.9, 2.0, 1.2e6, 11.0), (15.3, 16.0, 0.9, 1.7, 1.0e6, 10.0),
    (37.9, 48.8, 0.3, 1.3, 0.7e6, 9.0), (33.5, 9.0, 0.2, 0.8, 0.35e6, 6.0),
]
# a explosão do tanque do meio (o núcleo de look-dev): centro, raio da bola, candelas
BLAST = (26.7, 26.1, 1.8, 3.2, 1.2e7, 22.0)
# fumaça: (x, y, altura da coluna, largura, deriva x, deriva y)
# (na arte a fumaça sobe quase reta na tela e abre; o vento leva um pouco pro fundo, +X)
SMOKE = [(42.0, 30.6, 12.0, 2.2, 2.5, -0.6), (22.0, 15.4, 10.0, 2.0, 2.0, -0.5), (15.3, 16.0, 9.0, 1.9, 2.0, -0.5),
         (26.7, 26.1, 14.0, 3.0, 3.0, -0.8), (37.9, 48.8, 7.0, 1.6, 1.5, -0.4)]
# queimados no chão: (x, y, raio)
SCORCH = [(42.0, 30.6, 4.5), (22.0, 15.4, 4.2), (15.3, 16.0, 4.0), (26.7, 26.1, 6.5), (33.5, 9.0, 3.5),
          (30.0, 4.0, 3.0), (36.5, 2.5, 2.6), (12.0, 22.0, 3.2), (48.0, 30.0, 2.8), (18.0, 30.0, 2.4),
          # o chão da luta coberto de queimado e cratera (crítica da arte, 11/10: "o escuro é o padrão")
          (30.5, 20.0, 3.4), (20.0, 22.5, 3.0), (35.5, 24.5, 3.2), (28.5, 32.5, 2.8), (11.0, 28.5, 2.6),
          (45.0, 15.0, 3.0), (25.0, 9.5, 2.6), (7.5, 20.0, 2.4), (39.0, 38.0, 2.6), (16.5, 38.5, 2.2),
          (50.5, 23.5, 2.4), (4.0, 26.0, 2.0)]
# traçantes: (x0, y0, x1, y1, z): da China pra USA e de volta, cruzando a estrada
TRACERS = [(12.0, 30.5, 20.0, 27.8, 1.8), (10.0, 33.0, 18.5, 30.5, 2.0), (36.0, 17.0, 28.0, 21.0, 1.7),
           (40.0, 13.0, 31.0, 17.5, 1.9), (14.0, 26.0, 22.0, 24.6, 1.6)]
# luzes que não são fogo: o sinalizador de paraquedas (7 m acima do meio da poça de luz da arte)
# (a luz fica acima da chama de verdade: a poça larga da arte; o truque de cinema)
LIGHTS = [
    (21.0, 49.0, 2.8, (255, 226, 196), 1.7e7, 10.0, True),     # o disco do sinalizador: baixo e curto (borda marcada), branco-quente
]
# o sinalizador a 1,6 m do chão, a cúpula 6 m acima (medido na arte: a cúpula em (805,100), as cordas
# fechando em (812,185), 6 m de diâmetro)
FLARE = (21.0, 49.0, 1.6)


def debris():
    """Os destroços: pedaços escuros em volta dos fogos, da explosão e espalhados pelo campo (semente fixa)."""
    import random
    r = random.Random(11)
    out = []
    centers = [(26.7, 26.1, 10.0, 75), (42.0, 30.6, 6.0, 24), (22.0, 15.4, 6.0, 24), (15.3, 16.0, 5.0, 18),
               (32.0, 8.0, 6.0, 22), (12.0, 22.0, 5.0, 14), (30.5, 20.0, 5.0, 18), (35.5, 24.5, 4.0, 14)]
    for cx, cy, rad, n in centers:
        for _ in range(n):
            a = r.uniform(0, 6.283)
            d = rad * r.random() ** 0.7
            out.append((cx + d * __import__('math').cos(a), cy + d * __import__('math').sin(a), r.uniform(0, 360),
                        r.uniform(1.5, 4.5), r.randint(0, 9)))
    for _ in range(28):      # soltos pelo campo: poucos (a arte deixa a areia limpa longe das explosões)
        out.append((r.uniform(-5, 60), r.uniform(0, 45), r.uniform(0, 360), r.uniform(0.8, 2.0), r.randint(0, 9)))
    return out


DEBRIS = debris()

SCATTER = {
    'tuft': [f'grass_medium_01/grass_medium_01_{v}_LOD0' for v in
             ('large_a', 'large_b', 'large_c', 'mid_a', 'mid_b', 'tall_a', 'tall_b', 'tall_c')],
    'pebble': [f'namaqualand_stones_01/namaqualand_stones_01_{v}_LOD0' for v in 'abcde'],
    'bush': [f'wild_rooibos_bush/wild_rooibos_bush_{v}' for v in 'abcde']
            + [f'searsia_lucida/searsia_lucida_{v}_LOD0' for v in 'abcdefg'],
}
SCATTER_MI = {'grass_medium_01': 'MI_grass_medium_01_desert', 'namaqualand_stones_01': 'MI_namaqualand_stones_01_desert',
              'wild_rooibos_bush': 'MI_wild_rooibos_bush_desert', 'searsia_lucida': 'MI_searsia_lucida_desert'}


def footprints():
    """Retângulos no chão (x0, x1, y0, y1) das casas (a caixa que contém a casa girada) e dos muros."""
    import math
    out = []
    for d in HOUSES.values():
        a = math.radians(d.get('yaw', 0.0))
        hx = abs(math.cos(a)) * d['sx'] / 2 + abs(math.sin(a)) * d['sy'] / 2
        hy = abs(math.sin(a)) * d['sx'] / 2 + abs(math.cos(a)) * d['sy'] / 2
        out.append((d['x'] - hx, d['x'] + hx, d['y'] - hy, d['y'] + hy))
    for mesh, x, y, yaw, sc in WALLS:
        ln = WALL_MESHES[mesh][0] * sc
        a = math.radians(yaw)
        hx = abs(math.cos(a)) * ln / 2 + abs(math.sin(a)) * 0.18
        hy = abs(math.sin(a)) * ln / 2 + abs(math.cos(a)) * 0.18
        out.append((x - hx, x + hx, y - hy, y + hy))
    return out
