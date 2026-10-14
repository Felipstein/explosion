"""A folha das silhuetas do vdb_measure.py: cada quadro numa linha, de lado (X × Z) e de frente (Y × Z),
numa tela de 8 × 3 m com o chão embaixo e uma régua a cada metro.
Uso: python tread_vdb_sheet.py <silhuetas.npz> <saida.png> [voxel_m=0.05]"""
import sys
import numpy as np
from PIL import Image, ImageDraw

d = np.load(sys.argv[1])
dx = float(sys.argv[3]) if len(sys.argv) > 3 else 0.05
ks = sorted(int(k.split('_')[1]) for k in d.files if k.startswith('side_'))
nh, nz = int(8.0 / dx), int(3.0 / dx)


def pane(img, o_h, o_z):
    # o centro da nuvem (média pesada) no meio do painel; a altura é absoluta (z 0 = o chão)
    w = img.sum(axis=1)
    cx = int(round((w * np.arange(len(w))).sum() / max(w.sum(), 1e-6)))
    c = np.zeros((nz, nh), np.float32)
    for i in range(img.shape[0]):
        h = i - cx + nh // 2
        if not 0 <= h < nh:
            continue
        for j in range(img.shape[1]):
            z = j + int(o_z)
            if 0 <= z < nz:
                c[nz - 1 - z, h] = img[i, j]
    Z = 3
    im = Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8)).convert('RGB').resize((nh * Z, nz * Z), Image.NEAREST)
    dr = ImageDraw.Draw(im)
    for m in range(-4, 5):
        x = (nh // 2 + int(m / dx)) * Z
        dr.line([(x, nz * Z - 10), (x, nz * Z)], fill=(200, 60, 60))
    for m in range(1, 3):
        y = (nz - 1 - int(m / dx)) * Z
        dr.line([(0, y), (14, y)], fill=(200, 60, 60))
    return im


rows = []
for k in ks:
    x0, y0, z0, fps = d['o_%d' % k]
    a, b = pane(d['side_%d' % k], x0, z0), pane(d['front_%d' % k], y0, z0)
    r = Image.new('RGB', (a.width * 2 + 10, a.height + 16), (30, 30, 30))
    r.paste(a, (0, 16)); r.paste(b, (a.width + 10, 16))
    ImageDraw.Draw(r).text((4, 2), 't = %.2f s   lado (comprimento × altura)  |  frente (largura × altura); régua 1 m' % (k / fps), fill=(255, 220, 160))
    rows.append(r)
sheet = Image.new('RGB', (rows[0].width, sum(r.height for r in rows)), (20, 20, 20))
y = 0
for r in rows:
    sheet.paste(r, (0, y)); y += r.height
sheet.save(sys.argv[2])
print('folha', sys.argv[2])
