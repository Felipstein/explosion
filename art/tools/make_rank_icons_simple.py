"""Os ícones das patentes 4 e 5 da USA, simplificados pro tamanho do HUD (~40 px), com as próprias peças da folha do GPT.

O Felipe (01/11): os ranks 1 a 3 do GPT ficaram bons; o 4 e o 5 (águia + louro + estrelas) eram "muito detalhe em pouco espaço".
A insígnia real de general dos EUA é só as estrelas, então:
- rank 4: 4 estrelas de ouro em arco, com o louro do rank 3 pintado de ouro;
- rank 5: as 5 estrelas de ouro em pentágono (a insígnia real de General of the Army), grandes, sem águia e sem louro;
- nos dois, o aro do disco em ouro (diz "patente alta" sem custar detalhe).
A estrela de ouro é a do rank 5 do GPT; o disco e o louro são os dos ranks 1 e 3.

Uso: python art/tools/make_rank_icons_simple.py <folha_gpt>
Saída: docs/concept/hud/kit/skin/rank_usa_4.png e rank_usa_5.png (os detalhados do GPT ficam em captures/concept/hud/kit/)
"""
import os
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
KEEP = os.path.join(ROOT, 'captures/concept/hud/kit')


def key_green(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    spill = g - np.maximum(r, b)
    alpha = np.clip(1.0 - (spill - 40) / 80.0, 0, 1)
    g2 = np.where(spill > 0, np.maximum(r, b), g)
    return np.dstack([r, g2, b, alpha * 255])


def lum(a):
    return a[..., 0] * .3 + a[..., 1] * .59 + a[..., 2] * .11


def silver(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (lum(a) > 95) & (np.abs(r - b) < 40)


def gold(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return (r > 110) & (r > b * 1.6) & (g > b * 1.15)


def diffuse_fill(img, hole, iters=500):
    out = img.copy()
    known = ~hole & ~silver(img) & ~gold(img)
    out[~known] = 0
    for _ in range(iters):
        blur = np.stack([ndimage.uniform_filter(out[..., c] * known, 5) for c in range(3)], -1)
        w = ndimage.uniform_filter(known.astype(float), 5)[..., None]
        upd = ~known & (w[..., 0] > 0) & hole
        out[upd] = (blur / np.maximum(w, 1e-6))[upd]
        known = known | upd
    grain = np.random.default_rng(5).normal(0, 3.0, hole.shape)[..., None]
    return np.where(hole[..., None], np.clip(out + grain, 0, 255), img)


def clean_disc(ic, region):
    """Refaz o miolo do disco liso (o azul do próprio disco, com o degradê e o grão), onde region é True."""
    cx, cy, R, d = disc_geometry(ic)
    px = ic[..., :3]
    bgm = (d < R - 14) & ~silver(px) & ~gold(px) & (lum(px) < 70) & (lum(px) > 8)
    base = np.median(px[bgm], axis=0)
    yy, xx = np.mgrid[0:ic.shape[0], 0:ic.shape[1]]
    shade = 1.0 + 0.16 * (-(xx - cx) - (yy - cy)) / (2 * R) - 0.18 * (d / R) ** 2
    grain = np.random.default_rng(7).normal(0, 2.6, d.shape)
    flat = np.clip(base[None, None, :] * shade[..., None] + grain[..., None], 0, 255)
    soft = ndimage.gaussian_filter(region.astype(float), 3)[..., None]
    ic[..., :3] = px * (1 - soft) + flat * soft
    return ic


def to_gold(px):
    """Pinta prata de ouro mantendo o brilho e o relevo (o ouro dos ranks altos do GPT)."""
    l = lum(px)[..., None] / 255.0
    g = np.array([255, 196, 72.0])
    return np.clip(l ** 0.85 * g * 1.18 + l ** 3 * 60, 0, 255)


def disc_geometry(ic):
    a = ic[..., 3] > 128
    ys, xs = np.where(a)
    cy, cx = (ys.min() + ys.max()) / 2, (xs.min() + xs.max()) / 2
    R = (xs.max() - xs.min()) / 2
    yy, xx = np.mgrid[0:ic.shape[0], 0:ic.shape[1]]
    return cx, cy, R, np.hypot(xx - cx, yy - cy)


def gold_rim(ic):
    cx, cy, R, d = disc_geometry(ic)
    rim = (d > R - 11) & silver(ic[..., :3])
    ic[..., :3] = np.where(rim[..., None], to_gold(ic[..., :3]), ic[..., :3])
    return ic


def stamp(ic, st, sm, cx, cy, scale):
    from PIL import Image as I
    h, w = st.shape[:2]
    nw, nh = int(round(w * scale)), int(round(h * scale))
    st2 = np.asarray(I.fromarray(st.astype('uint8')).resize((nw, nh), I.LANCZOS)).astype(float)
    sm2 = np.asarray(I.fromarray((sm[..., 0] * 255).astype('uint8')).resize((nw, nh), I.LANCZOS)).astype(float)[..., None] / 255
    ox, oy = int(round(cx - nw / 2)), int(round(cy - nh / 2))
    reg = ic[oy:oy + nh, ox:ox + nw, :3]
    ic[oy:oy + nh, ox:ox + nw, :3] = reg * (1 - sm2) + st2 * sm2


def main(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float)
    rgba = key_green(a)
    lab, n = ndimage.label(ndimage.binary_closing(rgba[..., 3] > 40, iterations=2))
    objs = sorted([s for s in ndimage.find_objects(lab) if (s[0].stop - s[0].start) > 150], key=lambda s: s[1].start)
    icons = [rgba[s].copy() for s in objs][:5]
    r1, r3, r5 = icons[0], icons[2], icons[4]

    # a estrela de ouro: a da direita do rank 5 (inteira, solta do louro)
    lab5, _ = ndimage.label(gold(r5[..., :3]))
    best = None
    for i, sl in enumerate(ndimage.find_objects(lab5), 1):
        sz = int((lab5[sl] == i).sum()); h, w = sl[0].stop - sl[0].start, sl[1].stop - sl[1].start
        cxs = (sl[1].start + sl[1].stop) / 2
        if 400 <= sz <= 1600 and 0.7 < w / h < 1.4 and cxs > r5.shape[1] * 0.55 and (best is None or cxs > best[2]):
            best = (i, sl, cxs)
    i, sl, _ = best
    p = 3
    st = r5[sl[0].start - p:sl[0].stop + p, sl[1].start - p:sl[1].stop + p, :3]
    sm = ndimage.gaussian_filter(ndimage.binary_dilation(lab5[sl[0].start - p:sl[0].stop + p, sl[1].start - p:sl[1].stop + p] == i, iterations=1).astype(float), 0.8)[..., None]
    sw = st.shape[1]

    # rank 4: o rank 3 sem as 3 estrelas de prata, o louro em ouro, 4 estrelas em arco, aro de ouro
    r4 = r3.copy()
    cx, cy, R, d = disc_geometry(r4)
    H, W = r4.shape[:2]
    yy = np.mgrid[0:H, 0:W][0]
    laurel = silver(r4[..., :3]) & (d < R - 14) & (yy > H * 0.55)
    clean_disc(r4, (d < R - 13) & (yy < H * 0.585))
    laurel = ndimage.binary_dilation(laurel, iterations=1) & (lum(r4[..., :3]) > 50)
    r4[..., :3] = np.where(laurel[..., None], to_gold(r4[..., :3]), r4[..., :3])
    sc4 = (W * 0.205) / sw
    for fx, fy in ((0.235, 0.47), (0.41, 0.335), (0.59, 0.335), (0.765, 0.47)):
        stamp(r4, st, sm, W * fx, H * fy, sc4)
    gold_rim(r4)

    # rank 5: o disco do rank 1 sem a estrela, as 5 estrelas de ouro em pentágono, grandes, aro de ouro
    r5n = r1.copy()
    cx, cy, R, d = disc_geometry(r5n)
    H, W = r5n.shape[:2]
    clean_disc(r5n, d < R - 13)
    sc5 = (W * 0.235) / sw
    rr = W * 0.255
    for k in range(5):
        ang = np.radians(-90 + k * 72)
        stamp(r5n, st, sm, cx + rr * np.cos(ang), cy + 2 + rr * np.sin(ang), sc5)
    gold_rim(r5n)

    for nm, ic in (('rank_usa_4', r4), ('rank_usa_5', r5n)):
        old = os.path.join(OUT, nm + '.png')
        if os.path.exists(old) and not os.path.exists(os.path.join(KEEP, nm + '_detalhado.png')):
            os.replace(old, os.path.join(KEEP, nm + '_detalhado.png'))
        Image.fromarray(ic.clip(0, 255).astype('uint8'), 'RGBA').save(old)
        print(nm, 'ok')


if __name__ == '__main__':
    main(sys.argv[1])
