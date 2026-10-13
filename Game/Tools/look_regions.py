"""A noite medida por região, longe das fontes de luz (11/10).

O ponto a ponto por material (look_match.py) mistura a areia ao luar com a areia acesa pelo fogo e,
à noite, isso puxou a areia pro neutro: a arte, longe do fogo, é azul-petróleo. Aqui eu meço caixas
escolhidas à mão, só de luar (e uma no sinalizador), nas duas imagens:

  python Game/Tools/look_regions.py <repo> <render.png>
"""
import sys
import numpy as np
from PIL import Image

REPO, RENDER = sys.argv[1], sys.argv[2]
# (nome, caixa na escala 2048×1144): luar puro, sem fogo nem sinalizador por perto
REGIONS = [
    ('rua ao luar (alto, esquerda)', (60, 40, 140, 90)),
    ('areia ao luar (acima da rua)', (300, 20, 420, 80)),
    ('areia ao luar (abaixo da rua)', (20, 250, 140, 330)),
    ('rua ao luar (embaixo, direita)', (1500, 1040, 1620, 1100)),
    ('areia do sinalizador', (1500, 330, 1620, 400)),
]


def lab(rgb):
    c = rgb / 255.0
    c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = M @ c / np.array([0.9505, 1.0, 1.089])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.array([116 * f[1] - 16, 500 * (f[0] - f[1]), 200 * (f[1] - f[2])])


art = Image.open(REPO + '/docs/concept/scenes/desert_night/night_game_cam.webp').convert('RGB').resize((2048, 1144), Image.LANCZOS)
ren = Image.open(RENDER).convert('RGB').resize((2048, 1144), Image.LANCZOS)
tot = []
for name, box in REGIONS:
    a = np.asarray(art.crop(box)).reshape(-1, 3).astype(float).mean(0)
    b = np.asarray(ren.crop(box)).reshape(-1, 3).astype(float).mean(0)
    la, lb = lab(a), lab(b)
    tot.append(np.linalg.norm(la - lb))
    print(f'{name:36s} arte L*{la[0]:5.1f} a*{la[1]:+5.1f} b*{la[2]:+6.1f} | maquete L*{lb[0]:5.1f} a*{lb[1]:+5.1f} b*{lb[2]:+6.1f} | ΔE {tot[-1]:4.1f}')
print(f'ΔE médio {np.mean(tot):.1f}')
