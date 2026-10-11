"""Uma gravação BRM2 em metade da resolução (192³ → 96³), pra medir e comparar o custo.

Por quê (10/10): o Carro-Bomba (EmberGen, 192³) custa 15 ms por quadro na marcha
contra 1,3 ms de três Cargas Pesadas (96³). Esta versão separa as duas causas
(o detalhe 8× mais fino e a fumaça que enche a caixa inteira) sem os VDBs de origem.

A média de 2³ voxels é feita na grandeza física, não no byte gravado: fuligem e
poeira estão em √ (A = √(fuligem/4), √(poeira/2.5)), então volta ao quadrado, tira a média e
codifica de novo; calor e transmitância do céu são lineares. Na meia resolução (F), o
combustível já é a média de √ na conversão original (build_raymarch_vdb.py) e a
velocidade é linear: média direta do byte. Caixa, luz e duração ficam as da original.

Rodar: python BlastLab/Tools/brm2_downsample.py <entrada.brm2> <saída.brm2>
"""
import json
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import brm2  # noqa: E402

B = brm2.BRICK


def read_brm2(path):
    f = open(path, 'rb')
    magic, h = struct.unpack('<4sI', f.read(8))
    if magic != b'BRM2':
        raise ValueError('não é BRM2: ' + path)
    head = json.loads(f.read(h))
    base = 8 + h
    n, nf, nm = head['res'], head['fuelRes'], head['macroRes']
    m = n // B

    def frames():
        for k, off in zip(head['frameBricks'], head['frameOffsets']):
            f.seek(base + off)
            P = np.frombuffer(f.read(m ** 3 * 4), '<i4').reshape(m, m, m)
            f.read(nm ** 3)                                   # o macro antigo: refeito na resolução nova
            ab = np.frombuffer(f.read(k * (B + 2) ** 3 * 4), np.uint8).reshape(k, B + 2, B + 2, B + 2, 4)
            fb = np.frombuffer(f.read(k * (B // 2 + 2) ** 3 * 4), np.uint8).reshape(k, B // 2 + 2, B // 2 + 2, B // 2 + 2, 4)
            zs, ys, xs = np.nonzero(P >= 0)
            idx = P[zs, ys, xs]
            A6 = np.zeros((m, m, m, B, B, B, 4), np.uint8)
            A6[zs, ys, xs] = ab[idx, 1:-1, 1:-1, 1:-1]
            h = B // 2
            F6 = np.full((m, m, m, h, h, h, 4), 128, np.uint8)
            F6[..., 0] = 0
            F6[zs, ys, xs] = fb[idx, 1:-1, 1:-1, 1:-1]
            A = A6.transpose(0, 3, 1, 4, 2, 5, 6).reshape(n, n, n, 4)
            F = F6.transpose(0, 3, 1, 4, 2, 5, 6).reshape(nf, nf, nf, 4)
            yield A, F
        f.close()
    return head, frames()


def half(a):
    s = a.shape
    return a.reshape(s[0] // 2, 2, s[1] // 2, 2, s[2] // 2, 2, *s[3:]).mean(axis=(1, 3, 5))


def macro(a, r):
    """O de build_raymarch_vdb.py (ver lá): máximo de max(fuligem, poeira) no bloco e meio bloco de folga."""
    m = np.maximum(a[..., 0], a[..., 2])
    n = m.shape[0]
    nm = n // r
    lo, hi = -(r // 2), r + r // 2
    for axis in range(3):
        out = []
        for k in range(nm):
            i = np.clip(np.arange(k * r + lo, k * r + hi + 1), 0, n - 1)
            out.append(np.take(m, i, axis=axis).max(axis=axis))
        m = np.stack(out, axis=axis)
    return np.ascontiguousarray(m.astype(np.uint8))


def down(A, F):
    a = A.astype(np.float32) / 255.0
    soot = half(4.0 * a[..., 0] ** 2)
    dust = half(2.5 * a[..., 2] ** 2)
    temp = half(a[..., 1])
    sky = half(a[..., 3])
    A2 = np.empty(soot.shape + (4,), np.uint8)
    A2[..., 0] = np.round(np.sqrt(soot / 4.0) * 255)
    A2[..., 1] = np.round(temp * 255)
    A2[..., 2] = np.round(np.sqrt(dust / 2.5) * 255)
    A2[..., 3] = np.round(sky * 255)
    F2 = np.round(half(F.astype(np.float32))).astype(np.uint8)
    return A2, F2, macro(A2, 4)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    head, frames = read_brm2(src)
    n = head['res'] // 2
    out = {k: v for k, v in head.items() if k not in ('format', 'brick', 'frameBricks', 'frameOffsets')}
    out.update(res=n, fuelRes=n // 2, macroRes=n // 4, downsampledFrom=os.path.basename(src))
    out.setdefault('lightPhysical', 'reference/webgl' not in head.get('source', ''))
    counts = brm2.write(dst, out, (down(A, F) for A, F in frames))
    print('%s: %d quadros, %d³ → %.0f MB, blocos por quadro %d–%d' % (
        dst, len(counts), n, os.path.getsize(dst) / 2 ** 20, min(counts), max(counts)))


if __name__ == '__main__':
    main()
