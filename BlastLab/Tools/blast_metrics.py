"""Números da explosão num teste automático, contra os do alvo (docs/CONCEITO_EXPLOSOES.md §4c).

Recorta a explosão pela posição que o teste registrou (AUTOTEST screen x y
px10m) numa janela de LARGURA metros e mede, por instante:
- p5, mediana e p95 da luminância (sRGB 0–1): sem preto, o p5 fica alto (alvo 0.09–0.18);
- a saturação média dos 10% mais escuros: fumaça preta neutra ~0.4, "bronze" 0.9–1.0;
- a fração do fogo (pixels claros e saturados) com matiz 60–90°, o "limão"
  (alvo < 1%; combinado em 07/10: o amarelo-claro de 52–60° do núcleo é físico);
- o grão de TV: nas partes claras, o desvio da cor de cada pixel da média 5×5
  (1/255; quanto menor, mais limpo).

    python BlastLab/Tools/blast_metrics.py captures/ue/teste_a [captures/ue/teste_b ...] [--times ...] [--width 24]
"""
import argparse
import colorsys
import os

import numpy as np
from PIL import Image


def screen(folder):
    with open(os.path.normpath(folder) + '.log', encoding='utf-8', errors='replace') as f:
        line = next((l for l in f if 'AUTOTEST screen' in l), None)
    if not line:
        return None
    return [float(v) for v in line.split('AUTOTEST screen')[1].replace('px10m', '').split()]


def measure(im, box):
    a = np.asarray(im.crop(box), np.float32) / 255.0
    px = a.reshape(-1, 3)
    lum = px @ np.array([0.2126, 0.7152, 0.0722])
    mx, mn = px.max(1), px.min(1)
    sat = (mx - mn) / np.maximum(mx, 1e-4)
    fire = (mx > 0.6) & (sat > 0.3)
    hue = np.array([colorsys.rgb_to_hsv(*p)[0] * 360.0 for p in px[fire]]) if fire.any() else np.zeros(0)
    lemon = float(((hue >= 60) & (hue <= 90)).mean()) if len(hue) else 0.0
    # a parte escura (os 10% mais escuros): a saturação dela separa fumaça preta
    # neutra (~0.4 no alvo) de "bronze" (0.9–1.0) e de sombra no chão
    dark = lum <= np.percentile(lum, 10)
    dark_sat = float(sat[dark].mean())
    # o "grão de TV": nas partes claras (lum > 0.7), o quanto a cor de cada pixel
    # foge da média 5×5 em volta (croma R−G e G−B, em 1/255)
    h, w = a.shape[:2]
    chroma = np.stack([a[..., 0] - a[..., 1], a[..., 1] - a[..., 2]], -1)
    pad = np.pad(chroma, ((2, 2), (2, 2), (0, 0)), mode='edge')
    blur = sum(pad[dy:dy + h, dx:dx + w] for dy in range(5) for dx in range(5)) / 25.0
    bright = (a @ np.array([0.2126, 0.7152, 0.0722])) > 0.7
    grain = float(np.abs(chroma - blur)[bright].mean() * 255) if bright.any() else 0.0
    return np.percentile(lum, 5), np.median(lum), np.percentile(lum, 95), dark_sat, lemon, float(fire.mean()), grain


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('folders', nargs='+')
    ap.add_argument('--times', default='0.20,0.40,0.80,1.00,1.50,2.00,3.00,4.00')
    ap.add_argument('--width', type=float, default=24.0, help='largura da janela em metros')
    a = ap.parse_args()
    times = a.times.split(',')
    for folder in a.folders:
        s = screen(folder)
        print(os.path.basename(os.path.normpath(folder)))
        print('   t      p5    med   p95   sat-escuro  limão  fogo  grão')
        for t in times:
            path = os.path.join(folder, 'sim_%s.png' % t)
            if not os.path.exists(path) or not s:
                continue
            x, y, px10 = s
            half = px10 * a.width / 10.0 / 2.0
            cy = y - half * 0.3
            box = (int(x - half), int(cy - half * 0.75), int(x + half), int(cy + half * 0.75))
            p5, med, p95, dark_sat, lemon, fire, grain = measure(Image.open(path).convert('RGB'), box)
            print('  %5s  %5.2f  %5.2f  %5.2f    %5.2f    %5.1f%%  %4.0f%%  %4.1f' % (t, p5, med, p95, dark_sat, 100 * lemon, 100 * fire, grain))


if __name__ == '__main__':
    main()
