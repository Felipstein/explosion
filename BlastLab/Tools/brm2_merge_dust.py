"""A poeira do chão como camada da explosão: mistura uma gravação de poeira (canal b) na de uma explosão.

Por quê (11/10): nos AAA a explosão é uma pilha de camadas feitas à parte (fogo, fumaça, poeira, destroços;
Battlefront II, Back 4 Blood: pesquisa de 11/10). O nosso desenho marcha cada explosão sozinha e põe uma na
frente da outra, então duas gravações no mesmo lugar (a bola e a saia de poeira) se cruzariam errado. O
formato já tem o canal de poeira na mesma grade da bola: a camada de poeira entra nele, aqui, fora do jogo,
e no jogo continua sendo uma gravação só (custo de desenho só onde há poeira, sem explosão a mais).

- tempo: o quadro da explosão no instante t (a curva dela) lê a poeira no mesmo t (a curva da poeira),
  depois do atraso (a poeira levanta com o choque, não antes);
- espaço: as duas com o pé no chão e o centro no meio; a poeira reamostrada na grade da explosão
  pelo tamanho em metros de cada uma (domínio × escala);
- a poeira soma à que a explosão já tem (√ no byte: soma na grandeza física), e a transmitância do céu
  (canal a) é refeita com as duas (a poeira também faz sombra, como no conversor);
- caixa justa e ocupação macro refeitas.

Rodar: python BlastLab/Tools/brm2_merge_dust.py <explosao.brm2> <poeira.brm2> <saida.brm2> [escala_poeira=1] [atraso_s=0] [ganho=1]
"""
import json
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import brm2  # noqa: E402
from brm2_downsample import read_brm2, macro  # noqa: E402

SOOT_EXT, DUST_EXT, ERODE_MEAN = 13.5, 5.5, 0.042 * 0.5     # os do conversor (build_raymarch_vdb.py)


def times(head):
    n, dur, curve = head['frames'], head['duration'], head['curve']
    return np.array([dur * (q / (n - 1)) ** curve for q in range(n)])


def resample_dust(D, src_dom, dst_n, dst_dom):
    """poeira [z][y][x] numa caixa de src_dom m → a grade dst_n³ de dst_dom m (pé no chão, centro no meio),
    trilinear por eixo (a de destino é mais grossa ou igual: média por caixa onde encolhe)."""
    sn = D.shape[0]
    src_vm, dst_vm = src_dom / sn, dst_dom / dst_n
    out = D
    for axis in range(3):
        c = (np.arange(dst_n) + 0.5) * dst_vm                        # centro do voxel de destino (m)
        if axis > 0:
            c = c - dst_dom / 2 + src_dom / 2                        # x, y: centrados
        x = c / src_vm - 0.5
        i0 = np.floor(x).astype(int)
        t = (x - i0).astype(np.float32)
        valid = (i0 >= -1) & (i0 < sn)
        i0c, i1c = np.clip(i0, 0, sn - 1), np.clip(i0 + 1, 0, sn - 1)
        shape = [1] * 3
        shape[axis] = -1
        a = np.take(out, i0c, axis=axis) * (1 - t.reshape(shape)) + np.take(out, i1c, axis=axis) * t.reshape(shape)
        a *= valid.reshape(shape)
        out = a
    return out.astype(np.float32)


def main():
    src, dust_path, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    scale = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
    delay = float(sys.argv[5]) if len(sys.argv) > 5 else 0.0
    gain = float(sys.argv[6]) if len(sys.argv) > 6 else 1.0
    head, frames = read_brm2(src)
    dhead, dframes = read_brm2(dust_path)
    dust_frames = [A[..., 2].astype(np.float32) / 255.0 for A, F in dframes]   # √(poeira/2.5)
    dust_frames = [2.5 * d ** 2 for d in dust_frames]
    n, dom = head['res'], head['domain']
    vm = dom / n
    te, td = times(head), times(dhead)
    src_dom = dhead['domain'] * scale
    out_head = {k: v for k, v in head.items() if k not in ('format', 'brick', 'frameBricks', 'frameOffsets')}
    out_head['dustLayer'] = {'source': os.path.basename(dust_path), 'scale': scale, 'delay': delay, 'gain': gain}
    bounds = []

    def gen():
        for q, (A, F) in enumerate(frames):
            t = te[q] - delay
            A = A.copy()
            add = np.zeros((n, n, n), np.float32)
            if t >= 0.0:
                j = int(np.clip(np.searchsorted(td, t) - 1, 0, len(td) - 2))
                w = float(np.clip((t - td[j]) / max(td[j + 1] - td[j], 1e-6), 0.0, 1.0))
                d = dust_frames[j] * (1 - w) + dust_frames[j + 1] * w
                add = resample_dust(d, src_dom, n, dom) * gain
            soot = 4.0 * (A[..., 0] / 255.0) ** 2
            dust = np.clip(2.5 * (A[..., 2] / 255.0) ** 2 + add, 0.0, 2.5)
            A[..., 2] = np.round(np.sqrt(dust / 2.5) * 255)
            # o céu refeito com as duas (o mesmo cálculo do conversor; o A[...,3] da explosão vinha só com a dela)
            ext = SOOT_EXT * np.maximum(soot - ERODE_MEAN / (1.0 + soot * 9.0), 0.0) + DUST_EXT * dust
            above = np.cumsum(ext[::-1], axis=0)[::-1] - 0.5 * ext
            A[..., 3] = np.round(np.clip(np.exp(-np.minimum(above * vm, 60.0)), 0.0, 1.0) * 255)
            occ = (A[..., 0] > 0) | (A[..., 1] > 0) | (A[..., 2] > 0)
            if occ.any():
                e = []
                for axis in (2, 1, 0):
                    i = np.where(occ.any(axis=tuple(k for k in range(3) if k != axis)))[0]
                    e.append((int(i.min()), int(i.max()) + 1))
                bounds.append([round(v, 4) for v in [-dom / 2 + e[0][0] * vm, -dom / 2 + e[1][0] * vm, e[2][0] * vm,
                                                      -dom / 2 + e[0][1] * vm, -dom / 2 + e[1][1] * vm, e[2][1] * vm]])
            else:
                bounds.append([0.0] * 6)
            yield A, F, macro(A, 4)

    # a caixa de cada quadro só se sabe depois de escrever: o cabeçalho leva a lista preenchida pelo gerador
    out_head['bounds'] = bounds
    counts = brm2.write(dst, out_head, gen())
    print('%s: %d quadros, blocos por quadro %d–%d, %.0f MB' % (dst, len(counts), min(counts), max(counts), os.path.getsize(dst) / 2 ** 20))


if __name__ == '__main__':
    main()
