"""As estatísticas de clima de um conjunto de imagens (11/10): o alvo novo do deserto dourado são as sete
"perfeitas" do Felipe (docs/concept/scenes/README.md), não a day_game_cam. Como elas têm câmeras e
sóis diferentes, o ponto a ponto por material não serve: comparo o clima do quadro inteiro.

  python Game/Tools/look_stats.py <repo> <render1.png> [<render2.png> ...]

Mede, em cada imagem: saturação média (HSV), calor (R − B médio), a curva de brilho (p5/p50/p95)
e a névoa (o brilho da faixa de cima sobre o da faixa de baixo). Imprime as sete, a faixa delas,
a day_game_cam e os renders.
"""
import os
import sys

import numpy as np
from PIL import Image

REPO = sys.argv[1]
BEST = os.path.join(REPO, 'docs', 'concept', 'scenes', 'desert_golden_hour', 'best')
SEVEN = ['town_battle_top_gemini', 'road_battle_gemini', 'road_town_battle_gemini', 'street_battle_usa_china_gemini',
         'china_vs_gla_gemini', 'usa_vs_china_gemini', 'day_tilt_cam']


def stats(path):
    a = np.asarray(Image.open(path).convert('RGB').resize((768, 430))).astype(np.float32) / 255
    mx, mn = a.max(-1), a.min(-1)
    sat = np.where(mx > 1e-3, (mx - mn) / np.maximum(mx, 1e-3), 0).mean()
    warm = (a[..., 0] - a[..., 2]).mean()
    L = a @ np.array([0.2126, 0.7152, 0.0722])
    p5, p50, p95 = np.percentile(L, [5, 50, 95])
    h = a.shape[0]
    haze = L[: h // 5].mean() / max(L[-h // 5:].mean(), 1e-3)
    return sat, warm, p5, p50, p95, haze


def row(name, s):
    print(f'{name:34s} sat {s[0]:.2f}  calor {s[1]:.3f}  p5/p50/p95 {s[2]:.2f}/{s[3]:.2f}/{s[4]:.2f}  névoa {s[5]:.2f}')


rows = []
for n in SEVEN:
    f = next((x for x in os.listdir(BEST) if x.startswith(n)), None)
    if f:
        s = stats(os.path.join(BEST, f))
        rows.append(s)
        row(n, s)
R = np.array(rows)
print('-' * 110)
row('AS SETE (média)', R.mean(0))
print(f'{"  faixa":34s} sat {R[:,0].min():.2f}–{R[:,0].max():.2f}  calor {R[:,1].min():.3f}–{R[:,1].max():.3f}  '
      f'p50 {R[:,3].min():.2f}–{R[:,3].max():.2f}  névoa {R[:,5].min():.2f}–{R[:,5].max():.2f}')
row('day_game_cam (o alvo antigo)', stats(os.path.join(BEST, 'day_game_cam.webp')))
print('-' * 110)
for p in sys.argv[2:]:
    row(os.path.basename(p), stats(p))
