"""Comparação ponto a ponto do look: a arte contra a maquete, material por material.

Rodar (Python do sistema, numpy + pillow; o look.ps1 match chama):
  python Game/Tools/look_match.py <repo> <variante> [<arte>]

A câmera da maquete é a da arte (medida), então a mesma planta (Tools/look_layout.py)
projetada na câmera diz, nas duas imagens, onde está cada material:
- o chão: asfalto e areia (a planta pintada, look_ground.py), separados em sol e sombra
  pelo brilho (o limiar de Otsu dentro de cada material, em cada imagem);
- as casas: o telhado (no sol), a parede virada pra câmera (+Y, na sombra) e a parede do
  lado (−X, sol rasante); desenhadas da mais longe pra mais perto (a da frente tapa);
- os tanques: o teto (casco e torre) e o lado virado pra câmera;
- o fundo: a faixa de cima (a névoa).
O que a arte tem e a maquete não (a explosão e a poeira dela) sai das contas (EXCLUDE).

Saída em captures/look/:
- match_<variante>.png: a folha (as amostras lado a lado, a curva de tom e a cor por
  faixa de brilho), com o desvio de cada uma;
- match_<variante>.json: os números (pra eu ajustar a receita sem chute).
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = sys.argv[1]
VARIANT = sys.argv[2] if len(sys.argv) > 2 else 'Golden'
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_scene  # noqa: E402
from look_scene import HOUSES, YARDS, TANKS, SUB, SUFFIX  # noqa: E402
ART = sys.argv[3] if len(sys.argv) > 3 else os.path.join(REPO, look_scene.ART)

CAP = os.path.join(REPO, 'captures', 'look')
W, H = 1024, 572
# a câmera da arte (a mesma do look_lab.py, Tools/look_scene.py), metros; inclinação pra baixo positiva
CAM = dict(look_scene.CAM)
CAM['pitch'] = -CAM['pitch']
# o que a arte tem e a maquete não (fora das contas nas duas imagens)
EXCLUDE = look_scene.EXCLUDE
GROUND_DIR = os.path.join(CAP, SUB, 'ground')


def basis():
    t, p = math.radians(CAM['pitch']), math.radians(CAM['yaw'])
    F = np.array([math.cos(t) * math.cos(p), math.cos(t) * math.sin(p), -math.sin(t)])
    R = np.array([-math.sin(p), math.cos(p), 0.0])
    U = np.array([math.sin(t) * math.cos(p), math.sin(t) * math.sin(p), math.cos(t)])
    return F, R, U


def project(P):
    F, R, U = basis()
    f = (W / 2) / math.tan(math.radians(CAM['hfov'] / 2))
    d = np.asarray(P, float) - np.array([CAM['x'], CAM['y'], CAM['z']])
    zc = d @ F
    return W / 2 + f * (d @ R) / zc, H / 2 - f * (d @ U) / zc, zc


def ground_at_pixels():
    F, R, U = basis()
    f = (W / 2) / math.tan(math.radians(CAM['hfov'] / 2))
    v, u = np.mgrid[0:H, 0:W].astype(float)
    ray = F[None, None] + R[None, None] * ((u - W / 2) / f)[..., None] - U[None, None] * ((v - H / 2) / f)[..., None]
    C = np.array([CAM['x'], CAM['y'], CAM['z']])
    t = -C[2] / np.minimum(ray[..., 2], -1e-6)
    return C[0] + t * ray[..., 0], C[1] + t * ray[..., 1]


def poly(draw, pts3, label):
    q = [project(p)[:2] for p in pts3]
    draw.polygon([(float(x), float(y)) for x, y in q], fill=label)


LABELS = {1: 'asfalto', 2: 'areia', 3: 'telhado', 4: 'parede da direita (+Y)', 5: 'parede da esquerda (−X)',
          6: 'tanque: teto', 7: 'tanque: lado', 8: 'fundo (névoa)'}


def label_image():
    lab = np.zeros((H, W), np.uint8)
    gx, gy = ground_at_pixels()
    G = json.load(open(os.path.join(GROUND_DIR, 'ground.json')))
    L = np.asarray(Image.open(os.path.join(GROUND_DIR, f'T_LL_Layout{SUFFIX}.png'))).astype(np.float32) / 255
    n = L.shape[0]
    iu = np.clip(((gx - G['x0']) / G['size'] * n).astype(int), 0, n - 1)
    iv = np.clip(((gy - G['y0']) / G['size'] * n).astype(int), 0, n - 1)
    lay = L[iv, iu]
    inside = (gx > G['x0']) & (gx < G['x0'] + G['size']) & (gy > G['y0']) & (gy < G['y0'] + G['size'])
    asph = inside & (lay[..., 0] > 0.9) & (lay[..., 1] < 0.1) & (lay[..., 3] < 0.1)
    sand = inside & (lay[..., 0] < 0.1) & (lay[..., 2] < 0.1)
    lab[asph] = 1
    lab[sand] = 2
    img = Image.fromarray(lab)
    d = ImageDraw.Draw(img)
    # casas e tanques, da mais longe pra mais perto (a da frente tapa a de trás)
    things = []
    for h in list(HOUSES.values()) + list(YARDS.values()):
        things.append(('house', h, project((h['x'], h['y'], h['h'] / 2))[2]))
    for t in TANKS:
        things.append(('tank', {'x': t[0], 'y': t[1], 'yaw': t[2] if len(t) > 2 else 0.0}, project((t[0], t[1], 1.5))[2]))
    def rot(o, pts):
        a = math.radians(o.get('yaw', 0.0))
        ca, sa = math.cos(a), math.sin(a)
        return [(o['x'] + (px - o['x']) * ca - (py - o['y']) * sa, o['y'] + (px - o['x']) * sa + (py - o['y']) * ca, pz)
                for px, py, pz in pts]

    for kind, o, _ in sorted(things, key=lambda t: -t[2]):
        if kind == 'house':
            x0, x1 = o['x'] - o['sx'] / 2, o['x'] + o['sx'] / 2
            y0, y1 = o['y'] - o['sy'] / 2, o['y'] + o['sy'] / 2
            h = o['h']
            poly(d, rot(o, [(x0, y1, 0), (x1, y1, 0), (x1, y1, h), (x0, y1, h)]), 4)             # +Y
            poly(d, rot(o, [(x0, y0, 0), (x0, y1, 0), (x0, y1, h), (x0, y0, h)]), 5)             # −X
            poly(d, rot(o, [(x0, y0, h), (x1, y0, h), (x1, y1, h), (x0, y1, h)]), 3)             # o topo
            m = 0.8                                                                             # a mureta fora do telhado
            for (a0, a1, b0, b1) in ((x0, x1, y0, y0 + m), (x0, x1, y1 - m, y1), (x0, x0 + m, y0, y1), (x1 - m, x1, y0, y1)):
                poly(d, rot(o, [(a0, b0, h), (a1, b0, h), (a1, b1, h), (a0, b1, h)]), 0)
        else:
            x, y = o['x'], o['y']
            sd = 1.85 if abs(((o['yaw'] + 180) % 360) - 180) < 90 else -1.85      # o lado que a câmera vê
            poly(d, rot(o, [(x - 3.9, y + sd, 0.4), (x + 3.9, y + sd, 0.4), (x + 3.9, y + sd, 1.5), (x - 3.9, y + sd, 1.5)]), 7)
            poly(d, rot(o, [(x - 3.85, y - 1.7, 1.55), (x + 3.15, y - 1.7, 1.55), (x + 3.15, y + 1.7, 1.55), (x - 3.85, y + 1.7, 1.55)]), 6)
            poly(d, rot(o, [(x - 1.5, y - 1.3, 2.48), (x + 1.3, y - 1.3, 2.48), (x + 1.3, y + 1.3, 2.48), (x - 1.5, y + 1.3, 2.48)]), 6)
    lab = np.asarray(img).copy()
    if look_scene.SCENE == 'desert_golden_hour':
        lab[0:34, :] = np.where(lab[0:34, :] == 2, 8, lab[0:34, :])      # a faixa de cima na areia = o fundo com névoa
    for x0, y0, x1, y1 in EXCLUDE:
        lab[y0:y1, x0:x1] = 0
    # sem as bordas (2 px): a amostra fica dentro do material
    out = np.zeros_like(lab)
    for k in LABELS:
        m = Image.fromarray(((lab == k) * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(5))
        out[np.asarray(m) > 0] = k
    return out


def srgb_to_lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def lab_of(rgb):
    """sRGB 0..1 -> CIE L*a*b* (D65)."""
    l = srgb_to_lin(rgb)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = l @ M.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def otsu(v):
    h, e = np.histogram(v, 64, (0, 1))
    c = (e[:-1] + e[1:]) / 2
    w = np.cumsum(h)
    m = np.cumsum(h * c)
    mt = m[-1]
    wb, wf = w, w[-1] - w
    ok = (wb > 0) & (wf > 0)
    s = np.zeros_like(c)
    s[ok] = (mt * wb[ok] / w[-1] - m[ok]) ** 2 / (wb[ok] * wf[ok])
    return c[np.argmax(s)]


def samples(img, lab):
    a = np.asarray(img.convert('RGB').resize((W, H), Image.LANCZOS)).astype(np.float64) / 255
    L = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
    out = {}
    for k, name in LABELS.items():
        m = lab == k
        if m.sum() < 40:
            continue
        if k in (1, 2):
            t = otsu(L[m])
            for tag, mm in (('no sol', m & (L > t)), ('na sombra', m & (L <= t))):
                if mm.sum() > 40:
                    out[f'{name} {tag}'] = a[mm].mean(0)
        else:
            out[name] = a[m].mean(0)
    return out, a, L


def curve(L, mask):
    return np.percentile(L[mask], [1, 5, 10, 25, 50, 75, 90, 95, 99])


def bands(a, L, mask):
    """A cor das sombras, dos meios e das luzes (Lab a*, b* médios em cada terço de brilho)."""
    lb = lab_of(a)
    q = np.percentile(L[mask], [33, 66])
    out = []
    for lo, hi in ((0, q[0]), (q[0], q[1]), (q[1], 1.01)):
        m = mask & (L >= lo) & (L < hi)
        out.append(lb[m].mean(0))
    return out


def main():
    lab = label_image()
    frames = sorted(f for f in os.listdir(os.path.join(CAP, f'Seq_{VARIANT}_Q')) if f.endswith('.png')) \
        if os.path.isdir(os.path.join(CAP, f'Seq_{VARIANT}_Q')) else []
    full = os.path.join(CAP, f'Seq_{VARIANT}')
    rnd_path = os.path.join(CAP, f'Seq_{VARIANT}_Q', frames[-1]) if frames else \
        os.path.join(full, sorted(f for f in os.listdir(full) if f.endswith('.png'))[44])
    art, rnd = Image.open(ART), Image.open(rnd_path)
    sa, aa, La = samples(art, lab)
    sr, ar, Lr = samples(rnd, lab)
    mask = np.ones((H, W), bool)
    for x0, y0, x1, y1 in EXCLUDE:
        mask[y0:y1, x0:x1] = False
    ca, cr = curve(La, mask), curve(Lr, mask)
    ba, br = bands(aa, La, mask), bands(ar, Lr, mask)

    # a folha
    F = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 20)
    Fr = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 17)
    keys = [k for k in sa if k in sr]
    rows = len(keys)
    im = Image.new('RGB', (1900, 120 + rows * 54 + 480), (18, 18, 18))
    d = ImageDraw.Draw(im)
    d.text((20, 14), f'PONTO A PONTO · arte × maquete ({VARIANT}), material por material', fill=(120, 230, 140), font=F)
    d.text((20, 44), 'amostra · ARTE | MAQUETE · brilho L* (arte → maquete) · cor a*/b* (arte → maquete) · ΔE (a distância de cor: < 5 = igual a olho, > 15 = outra cor)',
           fill=(200, 200, 200), font=Fr)
    res = {'curve_art': ca.tolist(), 'curve_rnd': cr.tolist(), 'samples': {}, 'bands_art': [b.tolist() for b in ba],
           'bands_rnd': [b.tolist() for b in br]}
    y = 84
    for k in keys:
        A, R = sa[k], sr[k]
        la, lr = lab_of(A[None])[0], lab_of(R[None])[0]
        dE = float(np.linalg.norm(la - lr))
        res['samples'][k] = {'art': A.tolist(), 'rnd': R.tolist(), 'lab_art': la.tolist(), 'lab_rnd': lr.tolist(), 'dE': dE}
        d.text((20, y + 12), k, fill=(235, 235, 235), font=Fr)
        d.rectangle((300, y, 420, y + 44), fill=tuple(int(255 * c) for c in A))
        d.rectangle((424, y, 544, y + 44), fill=tuple(int(255 * c) for c in R))
        col = (120, 230, 140) if dE < 5 else ((255, 200, 90) if dE < 12 else (255, 120, 110))
        d.text((570, y + 12), f'L* {la[0]:.0f} → {lr[0]:.0f}    a* {la[1]:+.0f} → {lr[1]:+.0f}    b* {la[2]:+.0f} → {lr[2]:+.0f}    ΔE {dE:.1f}',
               fill=col, font=Fr)
        y += 54
    # a curva de tom
    y += 16
    d.text((20, y), 'A CURVA DE TOM (o brilho nos percentis 1–99 do quadro, sem a explosão)', fill=(120, 230, 140), font=F)
    gx0, gy0, gw, gh = 40, y + 40, 700, 300
    d.rectangle((gx0, gy0, gx0 + gw, gy0 + gh), outline=(90, 90, 90))
    ps = [1, 5, 10, 25, 50, 75, 90, 95, 99]
    for c, colr in ((ca, (255, 200, 90)), (cr, (120, 180, 255))):
        pts = [(gx0 + gw * p / 100, gy0 + gh * (1 - v)) for p, v in zip(ps, c)]
        d.line(pts, fill=colr, width=3)
    d.text((gx0 + 10, gy0 + 10), 'arte', fill=(255, 200, 90), font=Fr)
    d.text((gx0 + 10, gy0 + 34), 'maquete', fill=(120, 180, 255), font=Fr)
    tx = gx0 + gw + 40
    d.text((tx, gy0), 'percentil    arte    maquete', fill=(200, 200, 200), font=Fr)
    for i, p in enumerate(ps):
        d.text((tx, gy0 + 26 + i * 24), f'{p:>3}%        {ca[i]:.2f}     {cr[i]:.2f}', fill=(235, 235, 235), font=Fr)
    bx = tx + 330
    d.text((bx, gy0), 'a cor por faixa de brilho (a*, b*)', fill=(200, 200, 200), font=Fr)
    for i, nm in enumerate(('sombras', 'meios', 'luzes')):
        d.text((bx, gy0 + 30 + i * 60), f'{nm}: arte a*{ba[i][1]:+.1f} b*{ba[i][2]:+.1f}', fill=(255, 200, 90), font=Fr)
        d.text((bx, gy0 + 54 + i * 60), f'         maquete a*{br[i][1]:+.1f} b*{br[i][2]:+.1f}', fill=(120, 180, 255), font=Fr)
    out = os.path.join(CAP, f'match_{VARIANT}.png')
    im.save(out)
    json.dump(res, open(os.path.join(CAP, f'match_{VARIANT}.json'), 'w'), indent=1)
    # a máscara, pra conferir
    pal = np.array([[0, 0, 0], [90, 90, 255], [255, 220, 80], [255, 120, 40], [200, 60, 200], [80, 220, 220],
                    [80, 255, 80], [0, 140, 0], [255, 255, 255]], np.uint8)
    m = Image.fromarray(pal[lab])
    Image.blend(Image.open(ART).convert('RGB').resize((W, H)), m, 0.5).save(os.path.join(CAP, f'match_{VARIANT}_mask.png'))
    print(out)
    for k, v in res['samples'].items():
        print(f'{k:32s} ΔE {v["dE"]:5.1f}  L* {v["lab_art"][0]:5.1f}→{v["lab_rnd"][0]:5.1f}  '
              f'a* {v["lab_art"][1]:+5.1f}→{v["lab_rnd"][1]:+5.1f}  b* {v["lab_art"][2]:+5.1f}→{v["lab_rnd"][2]:+5.1f}')
    print('curva arte    ', np.round(ca, 2).tolist())
    print('curva maquete ', np.round(cr, 2).tolist())
    for i, nm in enumerate(('sombras', 'meios', 'luzes')):
        print(f'{nm}: arte a*{ba[i][1]:+.1f} b*{ba[i][2]:+.1f} | maquete a*{br[i][1]:+.1f} b*{br[i][2]:+.1f}')


main()
