"""As gravações do raymarch em blocos esparsos (BRM2), e a conversão das BRM1 densas.

Por quê (09/10): uma gravação fina (192–320³, a do EmberGen, a do Blender, a da
Unreal) ocupa GB no formato denso (BRM1), na RAM e no disco, e não cabe no jogo.
A fumaça ocupa uma fração pequena da caixa: guardada em blocos de 8³ voxels só
onde ela está (com 1 voxel de borda copiado do vizinho, pra filtragem trilinear
não emendar errado: o mesmo esquema do Sparse Volume Texture da Unreal e do
NanoVDB), sobra ~19% nas gravações finas e ~40% na de 96 (medido em 09/10).

BRM2: 'BRM2' · uint32 tamanho do cabeçalho · cabeçalho JSON (o do BRM1 + 'brick',
'frameBricks', 'frameOffsets') · por quadro:
  P  (res/8)³ int32: índice do bloco na lista do quadro, −1 = vazio (x mais rápido, z pra cima)
  M  (res/4)³ R8: a ocupação macro do BRM1, como era (pula o espaço vazio)
  A  K × 10³ RGBA8: os blocos de A (√fuligem/4, T/1.6, √poeira/2.5, céu) com a borda
  F  K × 6³ RGBA8: os mesmos blocos em meia resolução (4³ + borda): combustível, velocidade
Um bloco entra se ele ou um vizinho tem fuligem, calor ou poeira (o ruído de detalhe
desloca a amostra até ~0.4 m: a dilatação de um bloco cobre).

Rodar: python BlastLab/Tools/brm2.py <arquivo.bin> [...]  → <arquivo>.brm2 ao lado
"""
import json
import os
import struct
import sys

import numpy as np

BRICK = 8


def bricks_of(A, F):
    """Um quadro denso (A [n][n][n][4], F [n/2]³[4], uint8) → (P, blocos A, blocos F)."""
    n = A.shape[0]
    m = n // BRICK
    occ = (A[..., 0] > 0) | (A[..., 1] > 0) | (A[..., 2] > 0)
    bo = occ.reshape(m, BRICK, m, BRICK, m, BRICK).any(axis=(1, 3, 5))
    keep = bo.copy()
    for ax in range(3):
        for s in (1, -1):
            sh = np.roll(bo, s, ax)
            # sem dar a volta no domínio
            idx = [slice(None)] * 3
            idx[ax] = 0 if s == 1 else -1
            sh[tuple(idx)] = False
            keep |= sh
    zs, ys, xs = np.nonzero(keep)              # ordem z, y, x (a do P)
    P = np.full((m, m, m), -1, dtype='<i4')
    P[zs, ys, xs] = np.arange(len(zs), dtype='<i4')
    # borda: o voxel vizinho de verdade; na beira do domínio, o da beira (o shader prende no centro do último)
    Ap = np.pad(A, ((1, 1), (1, 1), (1, 1), (0, 0)), mode='edge')
    Fp = np.pad(F, ((1, 1), (1, 1), (1, 1), (0, 0)), mode='edge')
    b, h = BRICK, BRICK // 2
    ab = np.empty((len(zs), b + 2, b + 2, b + 2, 4), dtype=np.uint8)
    fb = np.empty((len(zs), h + 2, h + 2, h + 2, 4), dtype=np.uint8)
    for k, (z, y, x) in enumerate(zip(zs, ys, xs)):
        ab[k] = Ap[z * b:z * b + b + 2, y * b:y * b + b + 2, x * b:x * b + b + 2]
        fb[k] = Fp[z * h:z * h + h + 2, y * h:y * h + h + 2, x * h:x * h + h + 2]
    return P, ab, fb


def write(path, head, frames):
    """frames: iterável de (A, F, M) densos (uint8). Grava BRM2 (o cabeçalho vai no fim do arquivo
    temporário e é reescrito na frente: os tamanhos por quadro só se sabem depois)."""
    n = head['res']
    if n % BRICK or head['fuelRes'] * 2 != n:
        raise ValueError('res %d: precisa ser múltiplo de %d com combustível na metade' % (n, BRICK))
    tmp = path + '.tmp'
    counts, sizes = [], []
    with open(tmp, 'wb') as body:
        for A, F, M in frames:
            P, ab, fb = bricks_of(A, F)
            data = P.tobytes() + M.tobytes() + ab.tobytes() + fb.tobytes()
            body.write(data)
            counts.append(int(ab.shape[0]))
            sizes.append(len(data))
    head = dict(head, format='BRM2', brick=BRICK, frameBricks=counts)
    offs, o = [], 0
    for s in sizes:
        offs.append(o)
        o += s
    head['frameOffsets'] = offs
    hb = json.dumps(head, separators=(',', ':')).encode('utf-8')
    hb += b' ' * ((-len(hb)) % 16)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sI', b'BRM2', len(hb)))
        f.write(hb)
        with open(tmp, 'rb') as body:
            while True:
                chunk = body.read(1 << 26)
                if not chunk:
                    break
                f.write(chunk)
    os.remove(tmp)
    return counts


def read_brm1(path):
    """(cabeçalho, gerador de (A, F, M)) de uma BRM1 densa."""
    f = open(path, 'rb')
    magic, h = struct.unpack('<4sI', f.read(8))
    if magic != b'BRM1':
        raise ValueError('não é BRM1: ' + path)
    head = json.loads(f.read(h))
    n, nf, nm = head['res'], head['fuelRes'], head['macroRes']

    def frames():
        for _ in range(head['frames']):
            A = np.frombuffer(f.read(n ** 3 * 4), np.uint8).reshape(n, n, n, 4)
            F = np.frombuffer(f.read(nf ** 3 * 4), np.uint8).reshape(nf, nf, nf, 4)
            M = np.frombuffer(f.read(nm ** 3), np.uint8)
            yield A, F, M
        f.close()
    return head, frames()


if __name__ == '__main__':
    for src in sys.argv[1:]:
        head, frames = read_brm1(src)
        dst = os.path.splitext(src)[0] + '.brm2'
        counts = write(dst, head, frames)
        n = head['res']
        print('%s: %d quadros, %d³ → %.0f MB (era %.0f MB), blocos por quadro %d–%d' % (
            dst, len(counts), n, os.path.getsize(dst) / 2 ** 20, os.path.getsize(src) / 2 ** 20, min(counts), max(counts)))
