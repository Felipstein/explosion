"""A prova rápida das variantes do strike_chao só de areia (23/10 noite), sem gravar a BRM2: os quadros quantizados como o jogo
lê (8 bits, √), desenhados pelo raymarch numpy do brm2_view.py (sol a 35° com sombra, céu, o chão com linhas de 5 m, o tanque de
7,9 × 3,6 × 2,4 m), e as medidas:
- a área opaca (τ > 2) de lado e de cima, a areia abaixo de 3 m (o pé), a areia acima de 12 m;
- a leitura dos tanques: a transmitância da câmera do jogo (40° de cima) até o teto de tanques a 8, 12 e 16 m do impacto, nos 4
  rumos, a média e a pior.

  python qview.py <saida.png> <tempos> <largura_m> <variante> [<variante> ...]
  variante = nome:gdir:mfdir:chave=valor,chave=valor
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'sims', 'tools'))
import sandfield  # noqa: E402
import brm2_view as bv  # noqa: E402

SUN = np.array([-0.45, -0.55, 0.70])
SUN /= np.linalg.norm(SUN)


def metrics(F, soot, dust):
    vm = F.vm
    n = F.n
    ext = 13.5 * soot + 5.5 * dust
    side = ext.sum(axis=1) * vm
    top = ext.sum(axis=0) * vm
    z = (np.arange(n) + 0.5) * vm
    prof = (5.5 * dust).sum(axis=(1, 2))
    tot = max(prof.sum(), 1e-6)
    # a leitura: o raio da câmera (40° de cima, olhando pra +Y; e os outros 3 rumos pelo giro do campo) até o teto do tanque
    pitch = np.radians(40.0)
    reads = []
    for r in (8.0, 12.0, 16.0):
        for k in range(8):
            a = 2 * np.pi * k / 8
            P0 = np.array([r * np.cos(a), r * np.sin(a), 2.4])
            dirc = np.array([0.0, -np.cos(pitch), np.sin(pitch)])           # do teto pra câmera
            ss = np.arange(0.0, 60.0, vm * 0.7)
            pts = P0[None] + ss[:, None] * dirc[None]
            ix = np.clip(((pts[:, 0] + F.D / 2) / vm).astype(int), 0, n - 1)
            iy = np.clip(((pts[:, 1] + F.D / 2) / vm).astype(int), 0, n - 1)
            iz = (pts[:, 2] / vm).astype(int)
            ok = (iz >= 0) & (iz < n) & (np.abs(pts[:, 0]) < F.D / 2) & (np.abs(pts[:, 1]) < F.D / 2)
            tau = ext[iz[ok], iy[ok], ix[ok]].sum() * vm * 0.7
            reads.append(np.exp(-tau))
    reads = np.array(reads).reshape(3, 8)
    return dict(opaco_lado=float((side > 2).sum() * vm * vm), opaco_cima=float((top > 2).sum() * vm * vm),
                visivel_cima=float((top > 0.3).sum() * vm * vm), pe=float(prof[z < 3].sum() / tot), alto=float(prof[z > 12].sum() / tot),
                ler8=float(reads[0].mean()), ler12=float(reads[1].mean()), ler16=float(reads[2].mean()), pior=float(reads.min()),
                blocos=int(((soot > 0) | (dust > 0)).reshape(n // 8, 8, n // 8, 8, n // 8, 8).any(axis=(1, 3, 5)).sum()))


def main():
    out = sys.argv[1]
    want = [float(x) for x in sys.argv[2].split(',')]
    width = float(sys.argv[3])
    views = os.environ.get('QVIEWS', 'rts').split(',')
    variants = []
    for spec in sys.argv[4:]:
        name, gdir, mfdir, kv = spec.split('|')
        kw = {}
        for a in filter(None, kv.split(',')):
            k, v = a.split('=')
            kw[k] = float(v)
        cache = os.path.join(HERE, '..', 'cache', os.path.basename(os.path.normpath(gdir)))
        variants.append((name, sandfield.Field(gdir, mfdir, cache, **kw)))
    cols = []
    allm = {}
    for name, F in variants:
        tiles = []
        for t in want:
            q = int(np.argmin(np.abs(F.times - t)))
            soot, dust, vel, tq = F.frame(q)
            A, _ = F.encode(soot, dust, vel)
            # o que o jogo lê (8 bits) de volta
            a = A.astype(np.float32) / 255.0
            soot_r, dust_r = 4.0 * a[..., 0] ** 2, 2.5 * a[..., 2] ** 2
            m = metrics(F, soot_r, dust_r)
            allm.setdefault(name, []).append((round(tq, 2), m))
            print(name, f't {tq:.2f}', json.dumps({k: (round(v, 2) if isinstance(v, float) else v) for k, v in m.items()}), flush=True)
            head = {'res': F.n, 'domain': F.D}
            sig, col, emi, vm, nf = bv.fields(A, head, (-14.0, 4.0), 1.0)
            for v in views:
                im = Image.fromarray(bv.render(sig, col, emi, vm, nf, v, SUN, width, out_w=int(os.environ.get('QW', '380'))))
                ImageDraw.Draw(im).text((5, 4), f'{name} {v} t={tq:.2f}s  opaco {m["opaco_lado"]:.0f}/{m["opaco_cima"]:.0f} m2  ler12 {m["ler12"]:.2f}',
                                        fill=(255, 255, 255))
                tiles.append(im)
        cols.append(tiles)
    W, H = cols[0][0].size
    sheet = Image.new('RGB', (W * len(cols), H * len(cols[0])), (20, 20, 20))
    for i, c in enumerate(cols):
        for j, im in enumerate(c):
            sheet.paste(im, (i * W, j * H))
    sheet.save(out)
    with open(os.path.splitext(out)[0] + '.json', 'w') as f:
        json.dump(allm, f, indent=1)
    print('FOLHA', out, sheet.size)


if __name__ == '__main__':
    main()
