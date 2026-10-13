"""A prova da cratera v2 (LookLab, 13/10): a cratera de tiro de tanque na areia, no nível das referências que o Felipe
gerou (captures/concept/crater_refs/generated/sand_gpt.webp, a escala certa: ~meio tanque) e da street_battle.

O Felipe ficou desanimado ("o jogo não chega nesse nível"); a sessão de arte sugeriu a prova antes de tudo: um render
da cratera nova na mesma escala das referências. A cratera é o MESMO crater() do jogo (bake_craters.py, opção A, a do
tiro de tanque), a 2 cm por texel e com o relevo de verdade (look_ground.py: o pedaço de chão de 2,5 cm). O chão é o
deserto aberto da cena dos tanques; a câmera e o sol são os do jogo (rumo −45,33°, contraluz de três-quartos).
"""
from look_layout_tanks import *  # noqa: F401,F403  (o deserto aberto: chão, zonas, espalhamento)

MAP_PREFIX = 'LL_DesertCrater'
VARIANTS = {'Crater': 'desert_crater'}
ART = 'captures/concept/crater_refs/generated/sand_gpt.webp'

# a câmera da arte: o enquadramento da sand_gpt (~38 m de largura no foco, ~40° pra baixo), no rumo do jogo
CAM = {'x': -28.0, 'y': 28.3, 'z': 33.4, 'yaw': -45.33, 'pitch': -40.0, 'hfov': 40.0}

# o tanque USA de régua (7,9 m), em cima e à esquerda da cratera como na sand_gpt
TANKS = [(-1.49, -12.72, 15.0, 'usa')]
# a cratera: no meio-direita do quadro; o +X do carimbo pra longe de quem atirou (pra baixo e à direita da tela)
# (volta 2: a A tem ~⅓ do tanque e halo curto; a prova vai pro tamanho da sand_gpt (~meio tanque) e pros valores medidos
#  na street_battle, que a sessão do jogo põe no rebake)
CRATERS = [dict(x=2.13, y=2.11, yaw=74.7, seed=1, opt='A', params=dict(ra=2.0, depth=0.6, rim=0.16), v2_values=True, v3_relief=True)]

# o resto da cena dos tanques sai (sem carcaça, sem fogo, sem queimado)
FIRES = []
SCORCH = []
DEBRIS = []
TRACKS_EXTRA = [([(-30.0 + 1.6 * i, -25.0 + 0.9 * i) for i in range(40)], 0.5)]    # uma trilha velha atravessando

# os torrões (volta 4): as pedrinhas escaneadas escurecidas e menos saturadas, terra marrom-escura como na sand_gpt (eram
# as pedras avermelhadas do deserto)
SCATTER_TINT = dict(SCATTER_TINT, namaqualand_stones_01=(0.7, 0.6, 0.5, 0.4))
