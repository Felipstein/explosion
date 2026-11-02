"""Recorta os 5 ícones de patente da USA (folha do GPT sobre verde, 01/11) e conserta o rank 4.

O GPT desenhou o rank 4 (General) com 3 estrelas; o certo são 4. O conserto: as 3 estrelas saem (o fundo do disco é
refeito por difusão a partir da borda) e entram 4 estrelas em 2x2, copiadas do próprio rank 5 (o mesmo ouro e a mesma luz).

Uso: python art/tools/cut_rank_icons.py <folha> [<folha_4_e_5>]
(01/11: o 4 e o 5 detalhados foram reprovados, "muito detalhe em pouco espaço"; com a segunda folha, o 4 e o 5 saem dela,
simples, no tamanho dos ranks 1 a 3, e o conserto de cima não é usado)
Saída: docs/concept/hud/kit/skin/rank_usa_1..5.png (discos com transparência fora do círculo)
"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')


def key_green(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    alpha = np.clip(1.0 - (spill - 40) / 80.0, 0, 1)
    g2 = np.where(spill > 0, np.maximum(r, b), g)
    return np.dstack([r, g2, b, alpha * 255])


def gold_mask(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (r > 110) & (r > b * 1.6) & (g > b * 1.15)


def diffuse_fill(img, hole, iters=400):
    """Preenche o buraco espalhando a cor da borda (difusão simples), e devolve um grão parecido com o do disco."""
    out = img.copy()
    r, g, b = img[..., 0], img[..., 1], img[..., 2]
    known = ~hole & ~((r > b * 1.15) & (r > 55))            # só o fundo azul do disco serve de fonte (nada de ouro)
    out[hole] = 0
    for _ in range(iters):
        blur = np.stack([ndimage.uniform_filter(out[..., c] * known, 5) for c in range(3)], -1)
        w = ndimage.uniform_filter(known.astype(float), 5)[..., None]
        fill = blur / np.maximum(w, 1e-6)
        upd = hole & (w[..., 0] > 0)
        out[upd] = fill[upd]
        known = known | upd
    return out


def main(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float)
    rgba = key_green(a)
    lab, n = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    objs = sorted([s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) > 150], key=lambda s: s[1].start)
    icons = [rgba[s].copy() for s in objs][:5]

    # o rank 4: tirar as 3 estrelas e pôr 4 (2x2), copiadas do rank 5
    r4, r5 = icons[3], icons[4]
    H, W = r4.shape[:2]

    def stars_of(img, lo=400, hi=1600, ymin=0.0, ymax=1.0):
        """As estrelas: peças douradas soltas, do tamanho de uma estrela, no miolo (o louro e a águia são peças grandes)."""
        lab, n = ndimage.label(gold_mask(img[..., :3]))
        out = []
        for i, sl in enumerate(ndimage.find_objects(lab), 1):
            sz = int((lab[sl] == i).sum()); h, w = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
            cy = (sl[0].start + sl[0].stop) / 2 / img.shape[0]
            if lo <= sz <= hi and 0.7 < w / max(h, 1) < 1.4 and img.shape[1] * 0.25 < (sl[1].start + sl[1].stop) / 2 < img.shape[1] * 0.75 and ymin <= cy <= ymax:
                out.append((i, sl, lab))
        return out

    old = stars_of(r4, ymin=0.42, ymax=0.64)
    hole = np.zeros((H, W), bool)
    for i, sl, lab in old:
        hole |= lab == i
    hole = ndimage.binary_dilation(hole, iterations=8)
    r4[..., :3] = diffuse_fill(r4[..., :3], hole)
    grain = np.random.default_rng(4).normal(0, 3.0, hole.shape)[..., None]
    r4[..., :3] = np.where(hole[..., None], np.clip(r4[..., :3] + grain, 0, 255), r4[..., :3])

    # o carimbo: a estrela da direita do rank 5 (inteira, sem encostar no louro)
    s5 = sorted(stars_of(r5), key=lambda t: -(t[1][1].start))
    i, sl, lab5 = s5[0]
    pad = 3
    y0, y1, x0, x1 = max(sl[0].start - pad, 0), sl[0].stop + pad, max(sl[1].start - pad, 0), sl[1].stop + pad
    stamp = r5[y0:y1, x0:x1, :3]
    smask = ndimage.binary_dilation(lab5[y0:y1, x0:x1] == i, iterations=1).astype(float)
    smask = ndimage.gaussian_filter(smask, 0.8)[..., None]
    sh, sw = stamp.shape[:2]
    for px, py in ((W * 0.5 - 35, H * 0.47), (W * 0.5 + 35, H * 0.47), (W * 0.5 - 35, H * 0.665), (W * 0.5 + 35, H * 0.665)):
        ox, oy = int(round(px - sw / 2)), int(round(py - sh / 2))
        reg = r4[oy:oy + sh, ox:ox + sw, :3]
        r4[oy:oy + sh, ox:ox + sw, :3] = reg * (1 - smask) + stamp * smask
    print('rank 4: tiradas', len(old), 'estrelas; carimbo', sw, 'x', sh)
    icons[3] = r4

    os.makedirs(OUT, exist_ok=True)
    for i, ic in enumerate(icons, 1):
        Image.fromarray(ic.clip(0, 255).astype('uint8'), 'RGBA').save(os.path.join(OUT, f'rank_usa_{i}.png'))
        print(f'rank_usa_{i}', ic.shape[1], ic.shape[0])


def from_second_sheet(path, size):
    """O 4 e o 5 simples (a folha só com eles), no tamanho dos ranks 1 a 3."""
    a = np.asarray(Image.open(path).convert('RGB')).astype(float)
    rgba = key_green(a)
    lab, n = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    objs = sorted([s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) > 150], key=lambda s: s[1].start)[:2]
    for k, s in zip((4, 5), objs):
        im = Image.fromarray(rgba[s].clip(0, 255).astype('uint8'), 'RGBA')
        w = size[0]; h = round(im.height * w / im.width)
        im.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, f'rank_usa_{k}.png'))
        print(f'rank_usa_{k}', w, h, '(da folha 4 e 5)')


if __name__ == '__main__':
    main(sys.argv[1])
    if len(sys.argv) > 2:
        from_second_sheet(sys.argv[2], Image.open(os.path.join(OUT, 'rank_usa_1.png')).size)
