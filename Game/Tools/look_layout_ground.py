"""A planta da maquete de perto (LookLab): a arte desert_golden_hour/best/close_ground_light_gemini.webp.

Pedido do Felipe (11/10): a cena idêntica à referência ("o chão, tudo"), pra ver a versão final do jogo
contra a expectativa: areia aberta (sem rua), um tanque da USA, o Dozer do lado (rascunho na arte, aqui só
pra compor a cena e a sombra), tufos secos, pedrinhas e marcas de esteira, e o sol na mesma posição da arte
(da esquerda e um pouco de trás da câmera, baixo).

A câmera saiu dos pontos do tanque na arte (a boca do cano, o mantelete, as quinas do casco, a cúpula),
com o tanque provisório (casco de 7,9 m) na origem, frente em +X: o resolvedor em
captures/look/ground_cam.txt (Nelder-Mead nos 7 pontos).
"""
MAP_PREFIX = 'LL_DesertGround'
VARIANTS = {'Ground': 'desert_golden_ground'}

# medida nos 7 pontos do tanque (erro rms 33 px: o tanque da arte não tem as proporções exatas do provisório):
# teleobjetiva (21° de campo), 27 m de altura, 36,5° de inclinação, olhando o lado esquerdo do tanque
CAM = {'x': 16.23, 'y': -33.48, 'z': 27.17, 'yaw': 119.3, 'pitch': -36.5, 'hfov': 20.9}
ART = 'docs/concept/scenes/desert_golden_hour/best/close_ground_light_gemini.webp'
EXCLUDE = [(380, 120, 1024, 572)]       # o Dozer da arte é rascunho: fora das contas do ponto a ponto
GROUND_REGION = (-40.0, -40.0, 80.0)
SCATTER_REGION = (-35, 35, -35, 35)
# sem rua: a "estrada reta" bem longe, fora da planta
ROAD = {'kind': 'straight', 'line': [(-500.0, 500.0), (500.0, 500.0)], 'half': 4.0}
# a areia: o alvo de perto é amarelo-bege (matiz 29–30°, sat 0,3); o material dourado sai vermelho (24°)
# a areia (estudo de 12/10): o albedo em linear, bege-amarelado, luminância 0,37 (a areia de deserto da tabela PBR
# da Epic); a textura só dá o desenho. A cor da arte já vem acesa pelo sol laranja: não se copia pro albedo.
SAND_ALBEDO = (0.49, 0.4, 0.27)    # (0,44/0,36/0,24 da tabela saiu 20% escura contra a arte de perto)
SAND_RIPPLE_TEX = 1.0   # as ondulações simuladas (look_ripples.py), em manchas e embaralhadas
WIND_YAW = -140.7       # pra onde o vento sopra (graus): o sol fica do lado de onde vem o vento (155°), as cristas riscam
SAND_RIPPLES = 0.0      # (a ondulação de seno do shader: desligada)
# (antes: SAND_RIPPLES = 0.25     # fracas: na arte de perto a areia é pisada, quase sem ondulação regular (1,0 virou listras)
GROUND_TINT = {'sand': (1.0, 1.0, 1.0)}
HOUSES = {}
YARDS = {}
WALL_MESHES = {'W6': (6.0, 2.3), 'W10': (10.0, 2.2)}
WALLS = []

# o tanque da USA na origem, de frente pra +X (a câmera vê o lado esquerdo dele)
TANKS = [(0.0, 0.0, 0.0, 'usa')]
# o Dozer: (x, y, giro), ao lado do tanque, mais perto da câmera (medido depois na projeção)
DOZERS = [(-2.6, -6.5, 0.0)]      # a lâmina e o ripper desprojetados na arte: paralelo ao tanque, do lado de cá
PALMS = []
SHRUBS = []
POLES = []
EXPLOSIONS = []
WRECKS = []
LIGHTS = []
PROPS = []
TRACKS_FROM_TANKS = True          # o rastro das esteiras na areia atrás do tanque (a arte tem rastros cruzando o chão)
# o mapa de zonas (os limiares do ruído de cada zona: quanto maior, mais rara): o alvo tem pedaços diferentes em cada lugar
ZONES = {'smooth': (0.56, 0.66), 'gravel': (0.58, 0.68), 'ripple': (0.55, 0.63), 'stir': (0.67, 0.75)}
TRACK_RELIEF = True               # a trilha com relevo assado (sulco raso, sapatas, bordinha; look_ground.track_relief)
import os as _os
# as trilhas no quadro da arte (as linhas que cruzam o alto à direita, desprojetadas: de (−7,0; 6,1) a (−11,8; −1,4)):
# (pontos do meio do tanque, idade 0..1). B = uma trilha fresca; C = + uma velha cruzando, já meio coberta pelo vento
_L1 = [(-3.5 - 0.53 * i, 11.6 - 0.85 * i) for i in range(28)]
_L2 = [(8.2 - 0.33 * i + 0.004 * i * i, 5.5 - 0.95 * i) for i in range(20)]   # a velha: cruza o chão aberto à esquerda e embaixo
_TX = _os.environ.get('LOOK_TRACKS_EXTRA', 'C')
TRACKS_EXTRA = [] if _TX == '0' else ([(_L1, 0.05)] if _TX == 'B' else [(_L1, 0.05), (_L2, 0.75)])
SCATTER_DENSITY = 1.5             # touceiras de capim seco (a arte tem bem mais que a 1ª tentativa)
PEBBLE_FIELD = 0.12               # pedrinhas soltas pelo chão, poucas
PEBBLE_SCALE = (0.8, 1.8)         # e pequenas (a pedra escaneada tem 8 cm: aqui 6 a 15 cm; 1ª tentativa saiu pedregulho)
BUSH_FRACTION = 0.03              # quase sem arbusto verde: na arte é capim seco
TWIG_FIELD = 0.06                 # e gravetos secos soltos na areia
# o tom das plantas (Tint, Desat) por cima do material do deserto: o capim seco oliva-amarelado claro, o graveto desbotado
SCATTER_TINT = {'grass_medium_01': (2.3, 2.0, 1.25, 0.15), 'dry_branches_medium_01': (2.0, 1.85, 1.6, 0.3)}

SCATTER = {
    'tuft': [f'grass_medium_01/grass_medium_01_{v}_LOD0' for v in
             ('large_a', 'large_b', 'large_c', 'mid_a', 'mid_b', 'tall_a', 'tall_b', 'tall_c')],
    'pebble': [f'namaqualand_stones_01/namaqualand_stones_01_{v}_LOD0' for v in 'abcde'],
    'bush': [f'wild_rooibos_bush/wild_rooibos_bush_{v}' for v in 'abcde']
            + [f'searsia_lucida/searsia_lucida_{v}_LOD0' for v in 'abcdefg'],
    'twig': [f'dry_branches_medium_01/dry_branches_medium_01_{v}' for v in 'ab'],
}
SCATTER_MI = {'grass_medium_01': 'MI_grass_medium_01_desert', 'namaqualand_stones_01': 'MI_namaqualand_stones_01_desert',
              'wild_rooibos_bush': 'MI_wild_rooibos_bush_desert', 'searsia_lucida': 'MI_searsia_lucida_desert',
              'dry_branches_medium_01': 'MI_dry_branches_medium_01_desert'}


def footprints():
    """Sem casas nem muros: o espalhamento só desvia dos veículos (look_ground.py já desvia dos tanques)."""
    return [(-6.2, 1.0, -8.4, -4.6)]        # a pegada do Dozer (a caixa em volta dele)
