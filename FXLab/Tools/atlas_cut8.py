"""O recorte de 8 vértices de cada par de quadros (13/10, otimização A3): o polígono convexo de até 8 lados que cobre o
alfa dos quadros f e f+1 (os dois que o material mistura), com folga. O jogo desenha esse octógono em vez da caixa:
menos pixel pintado e o mesmo visual (só sai pixel de alfa zero). A técnica é a dos "particle cutouts" (Humus, Epic):
o fecho convexo da máscara reduzido a 8 vértices tirando, a cada passo, a aresta cuja remoção acrescenta menos área.
Grava "cut8": [[u0, v0, u1, v1, ...] por quadro] no atlas.json (v pra cima, fração do quadro).
Uso: python atlas_cut8.py <pasta_do_cozimento> [limiar=0.01] [folga_px=6]"""
import json
import os
import subprocess
import sys

import imageio_ffmpeg
import numpy as np

d = sys.argv[1]
thr = float(sys.argv[2]) if len(sys.argv) > 2 else 0.01
pad = int(sys.argv[3]) if len(sys.argv) > 3 else 6
meta = json.load(open(os.path.join(d, 'atlas.json')))
R, C, N = meta['res'], meta['cols'], meta['frames']
W, H = C * R, meta['rows'] * R
raw = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-loglevel', 'error', '-i', os.path.join(d, 'six_pos.exr'), '-f', 'rawvideo',
                      '-pix_fmt', 'rgba64le', '-'], capture_output=True).stdout
A = np.frombuffer(raw, np.uint16).reshape(H, W, 4)[..., 3].astype(np.float32) / 65535


def frame(k):
    r, c = divmod(k, C)
    return A[r * R:(r + 1) * R, c * R:(c + 1) * R]


def hull(pts):
    pts = sorted(set(map(tuple, pts)))
    if len(pts) < 3:
        return pts
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]          # anti-horário


def area(poly):
    x = np.array([p[0] for p in poly]); y = np.array([p[1] for p in poly])
    return 0.5 * abs(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def intersect(p1, p2, p3, p4):
    # a reta p1-p2 com a reta p3-p4
    d = (p1[0] - p2[0]) * (p3[1] - p4[1]) - (p1[1] - p2[1]) * (p3[0] - p4[0])
    if abs(d) < 1e-9:
        return None
    a = p1[0] * p2[1] - p1[1] * p2[0]
    b = p3[0] * p4[1] - p3[1] * p4[0]
    return ((a * (p3[0] - p4[0]) - (p1[0] - p2[0]) * b) / d, (a * (p3[1] - p4[1]) - (p1[1] - p2[1]) * b) / d)


def reduce(poly, k=8):
    poly = list(poly)
    while len(poly) > k:
        best, bi, bp = None, -1, None
        n = len(poly)
        for i in range(n):
            # tirar a aresta i (poly[i] → poly[i+1]): os vértices i e i+1 viram o encontro das arestas vizinhas
            a, b, c2, d2 = poly[i - 1], poly[i], poly[(i + 1) % n], poly[(i + 2) % n]
            q = intersect(a, b, c2, d2)
            if q is None:
                continue
            add = area([b, q, c2])
            # o encontro tem de ficar do lado de fora (o polígono continua convexo e cobrindo)
            if best is None or add < best:
                best, bi, bp = add, i, q
        if bi < 0:
            break
        n = len(poly)
        new = []
        for j in range(n):
            if j == bi:
                new.append(bp)
            elif j == (bi + 1) % n:
                continue
            else:
                new.append(poly[j])
        poly = new
    return poly


cuts, box_area, cut_area = [], [], []
for k in range(N):
    m = (frame(k) > thr) | (frame(min(k + 1, N - 1)) > thr)
    ys, xs = np.nonzero(m)
    if len(xs) == 0:
        cuts.append([0.5, 0.5] * 8); continue
    # os cantos dos pixels com folga (fecho de quadrados de lado 1 + 2·folga)
    pts = []
    for x, y in zip(xs, ys):
        pass
    # só a borda importa pro fecho: os extremos de cada linha
    rows = {}
    for x, y in zip(xs, ys):
        lo, hi = rows.get(y, (x, x))
        rows[y] = (min(lo, x), max(hi, x))
    for y, (lo, hi) in rows.items():
        for xx in (lo - pad, hi + 1 + pad):
            for yy in (y - pad, y + 1 + pad):
                pts.append((float(min(max(xx, 0), R)), float(min(max(yy, 0), R))))
    hl = hull(pts)
    p8 = reduce(hl, 8)
    while len(p8) < 8:
        p8.append(p8[-1])
    # pixel (x, y de cima) → (u, v de baixo)
    flat = []
    for x, y in p8:
        flat += [round(min(max(x / R, 0.0), 1.0), 4), round(min(max(1 - y / R, 0.0), 1.0), 4)]
    cuts.append(flat)
    x0, x1, y0, y1 = xs.min() - pad, xs.max() + 1 + pad, ys.min() - pad, ys.max() + 1 + pad
    box_area.append((min(x1, R) - max(x0, 0)) * (min(y1, R) - max(y0, 0)) / R / R)
    cut_area.append(area([(min(max(x, 0), R), min(max(y, 0), R)) for x, y in p8]) / R / R)
meta['cut8'] = cuts
json.dump(meta, open(os.path.join(d, 'atlas.json'), 'w'), indent=1)
print('ATLAS_CUT8 caixa %.1f %% do quadro, octógono %.1f %% (%.0f %% da caixa)' % (100 * np.mean(box_area), 100 * np.mean(cut_area),
                                                                                   100 * np.mean(cut_area) / np.mean(box_area)))
