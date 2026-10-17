"""O asfalto sem desenho repetido (16/10, o Felipe na hora dourada: "olha quanto padronizado ficou esses detalhes [...] não tá
natural"; docs/WORLDFORGE.md, o item 10). Roda fora da Unreal (python, numpy, scipy, Pillow), antes da montagem.

A textura base (road_damaged_2, 4K, ladrilho de 2,2 m) tem poucos buracos marcados; repetida, eles viram grade. O material
da laje passa a amostrar a foto em hexágonos girados (Mikkelsen, "Practical Real-Time Hex-Tiling", JCGT 2022) e cada vértice
da grade lê uma janela da foto SEM buraco; os buracos voltam como carimbos soltos, recortados da mesma foto. Este script
prepara as duas coisas:

1. os centros livres: pontos da foto (UV 0..1) cujo disco de raio R não toca nenhum buraco, na foto repetida (toroidal).
   R = o alcance de um vértice do hexágono (0,2887 / RATE do ladrilho) mais uma folga;
2. o atlas dos buracos (2×2, 512 px cada): cor + máscara (alfa), normal, ARM e altura, com o lado de cada quadrado em metros.

Saída: Intermediate/Assets/road_damaged_2_pot/AsphPot_{D,N,ARM,H}.png e Tools/asphalt_hex.json (lido pelo build_slice).
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Intermediate', 'Assets', 'road_damaged_2', 'road_damaged_2_')
OUT = os.path.join(ROOT, 'Intermediate', 'Assets', 'road_damaged_2_pot')
JSON = os.path.join(ROOT, 'Tools', 'asphalt_hex.json')

TILE_M = 2.2          # o ladrilho da camada asphalt do deserto (build_slice.py, SLICES['desert'])
RATE = 1.6            # vértices do hexágono por ladrilho: alcance 0,2887/1,6 = 0,18 do ladrilho (~40 cm)
MARGIN = 0.015        # folga do disco (o filtro e o mip)
N_CENTERS = 12
MIN_SEP = 0.10        # distância mínima entre dois centros (UV): janelas diferentes, não a mesma repetida
THR = 0.06            # buraco: a altura 6% abaixo (ou 9% acima, a borda) da média local de ~10 cm
STAMP_PX = 512
N_STAMPS = 4


def load(name, mode):
    return np.asarray(Image.open(SRC + name).convert(mode)).astype(np.float32) / 255.0


def torus_blur(a, sigma):
    """Desfoque gaussiano na foto repetida (as bordas emendam)."""
    return ndimage.gaussian_filter(a, sigma, mode='wrap')


def main():
    disp = load('disp_4k.jpg', 'L')
    n = disp.shape[0]
    k = n // 1024
    d = disp.reshape(1024, k, 1024, k).mean(axis=(1, 3))      # 1024 px: ~2,1 mm por pixel, sobra pra achar buraco
    diff = d - torus_blur(d, 1024 * 0.045)
    mask = (diff < -THR) | (diff > 1.5 * THR)
    mask = ndimage.binary_opening(mask, iterations=2)
    lab, nl = ndimage.label(mask)
    sizes = ndimage.sum(mask, lab, range(1, nl + 1))
    keep = [i + 1 for i, s in enumerate(sizes) if s > 250]   # > ~11 cm²: o resto é grão
    holes = np.isin(lab, keep)

    # 1) os centros livres: a distância até o buraco mais perto na foto repetida (3×3)
    free = ndimage.distance_transform_edt(np.tile(~holes, (3, 3)))[1024:2048, 1024:2048] / 1024.0
    r_need = 0.2887 / RATE + MARGIN
    cand = np.argwhere(free > r_need)
    if len(cand) == 0:
        sys.exit('nenhum ponto livre com raio %.3f: baixe o RATE' % r_need)
    rng = np.random.default_rng(16)
    cand = cand[rng.permutation(len(cand))[:20000]]
    uv = (cand[:, ::-1] + 0.5) / 1024.0                       # (x, y) = (u, v)
    score = free[cand[:, 0], cand[:, 1]]
    centers = [uv[np.argmax(score)]]
    while len(centers) < N_CENTERS:
        c = np.array(centers)
        dd = np.abs(uv[:, None, :] - c[None, :, :])
        dd = np.minimum(dd, 1.0 - dd)
        sep = np.sqrt((dd ** 2).sum(-1)).min(axis=1)
        ok = sep >= MIN_SEP
        if not ok.any():
            break
        i = np.argmax(np.where(ok, sep + 0.3 * score, -1.0))
        centers.append(uv[i])
    print('centros livres: %d (raio %.3f do ladrilho = %.0f cm; %.1f%% da foto livre)' %
          (len(centers), r_need, r_need * TILE_M * 100, 100.0 * (free > r_need).mean()))

    # 2) os buracos: agrupa os pedaços vizinhos (o amendoim tem o lóbulo e a meia-lua) e fica com os 4 maiores
    grp, ng = ndimage.label(ndimage.binary_dilation(holes, iterations=int(1024 * 0.03)))
    areas = ndimage.sum(holes, grp, range(1, ng + 1))
    order = [i + 1 for i in np.argsort(-areas)[:N_STAMPS]]
    full = {
        'D': np.asarray(Image.open(SRC + 'diff_4k.jpg').convert('RGB')),
        'N': np.asarray(Image.open(SRC + 'nor_dx_4k.jpg').convert('RGB')),
        'ARM': np.asarray(Image.open(SRC + 'arm_4k.jpg').convert('RGB')),
        'H': np.asarray(Image.open(SRC + 'disp_4k.jpg').convert('L')),
    }
    atlas = {key: Image.new('RGBA' if key == 'D' else ('L' if key == 'H' else 'RGB'), (2 * STAMP_PX, 2 * STAMP_PX))
             for key in full}
    sizes_m = []
    for si, g in enumerate(order):
        ys, xs = np.nonzero(grp == g)
        cy, cx = (ys.min() + ys.max()) / 2.0, (xs.min() + xs.max()) / 2.0
        side = 1.4 * max(ys.max() - ys.min(), xs.max() - xs.min()) + 30   # 20% de folga de cada lado (a mancha em volta)
        sizes_m.append(float(side / 1024.0 * TILE_M))
        # o recorte na foto 4K, com a volta do ladrilho (np.take em modo wrap)
        s4 = side * k
        idx_y = (np.round(cy * k - s4 / 2 + np.arange(int(s4)))).astype(int)
        idx_x = (np.round(cx * k - s4 / 2 + np.arange(int(s4)))).astype(int)
        ox, oy = (si % 2) * STAMP_PX, (si // 2) * STAMP_PX
        # a máscara: o grupo (com a borda e a mancha em volta) suavizado, e zero perto da borda do quadrado
        m = (grp == g).astype(np.float32)
        m = np.take(np.take(m, (idx_y // k) % 1024, axis=0, mode='wrap'), (idx_x // k) % 1024, axis=1, mode='wrap')
        m = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).resize((STAMP_PX, STAMP_PX), Image.BILINEAR)).astype(np.float32) / 255.0
        m = ndimage.gaussian_filter(m, STAMP_PX * 0.025)
        m = np.clip((m - 0.08) / 0.5, 0.0, 1.0)
        yy, xx = np.mgrid[0:STAMP_PX, 0:STAMP_PX]
        rr = np.maximum(np.abs(yy - STAMP_PX / 2 + 0.5), np.abs(xx - STAMP_PX / 2 + 0.5)) / (STAMP_PX / 2)
        m *= np.clip((1.0 - rr) / 0.12, 0.0, 1.0)
        for key, arr in full.items():
            crop = np.take(np.take(arr, idx_y % n, axis=0, mode='wrap'), idx_x % n, axis=1, mode='wrap')
            im = Image.fromarray(crop).resize((STAMP_PX, STAMP_PX), Image.LANCZOS)
            if key == 'D':
                im = im.convert('RGBA')
                im.putalpha(Image.fromarray((m * 255).astype(np.uint8)))
            atlas[key].paste(im, (ox, oy))
        print('buraco %d: %.2f m de lado (área do buraco %.2f m²)' %
              (si, sizes_m[-1], areas[g - 1] / 1024.0 ** 2 * TILE_M ** 2))
    os.makedirs(OUT, exist_ok=True)
    for key, im in atlas.items():
        im.save(os.path.join(OUT, 'AsphPot_%s.png' % key))   # (o build_slice importa como T_AsphPot_*)
    with open(JSON, 'w', encoding='utf-8') as f:
        json.dump({'tile_m': TILE_M, 'rate': RATE, 'radius_uv': r_need,
                   'centers': [[round(float(c[0]), 4), round(float(c[1]), 4)] for c in centers],
                   'stamp_m': [round(s, 3) for s in sizes_m]}, f, indent=1)
    print('ok:', OUT, JSON)


if __name__ == '__main__':
    main()
