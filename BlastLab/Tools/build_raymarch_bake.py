"""A explosão gravada do WebGL no formato do raymarch próprio (BlastRaymarch).

O renderizador de explosões que a Unreal traz (Heterogeneous Volumes) saiu
borrado e caro na GTX 1660 (07/10: buffer de 536×304 em alta, 412 ms em
qualidade cheia). O do WebGL (reference/webgl/js/instances.js) fazia a mesma
explosão nítida em ~5 ms. Este script prepara os dados pra o porte dele
(Shaders/Private/BlastRaymarch.usf), lendo o arquivo ORIGINAL do WebGL: a
conversão antiga (convert_bake.py) jogava fora a transmitância do céu, que a
marcha do WebGL usa.

Entrada: reference/webgl/assets/bake/r<res>-<hash>.bin (js/bake.js pack()).
Saída (fora do git): Intermediate/Raymarch/r<res>_v<variante>.bin
  'BRM1' · uint32 tamanho do cabeçalho · cabeçalho JSON (com folga) ·
  por quadro: A · F · M
  A  res³ RGBA8: √(fuligem/4), temperatura/1.6, √(poeira/2.5), transmitância do céu
  F  (res/2)³ RGBA8: combustível (código do WebGL), velocidade x, y, z (byte/255·2−1 → ±velMax m/s)
  M  (res/4)³ R8: ocupação macro (máx. de fuligem e poeira em gamma, blocos
     de 4 voxels dilatados meio bloco pra cada lado, como js/bake.js _macroShader)
Ordem densa: x mais rápido, depois y, depois z (pra cima). Eixos: WebGL (x, y
pra cima, z) → Unreal (x, z, y). Cabeçalho em metros LOCAIS (Carga Pesada,
escala 1), origem no pé da explosão: caixa justa por quadro, posição e cor da
luz gravada por quadro (cor nas unidades do WebGL: 1 = iluminância do sol).

Rodar: python BlastLab/Tools/build_raymarch_bake.py [res]
"""
import glob
import gzip
import json
import math
import os
import struct
import sys

import numpy as np

TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
REPO = os.path.abspath(os.path.join(PROJECT, '..'))
BAKES = os.path.join(REPO, 'reference', 'webgl', 'assets', 'bake')
OUT = os.path.join(PROJECT, 'Intermediate', 'Raymarch')


def atlas_layout(n):
    """O mesmo layout de tiles do VolumeGrid (js/volume.js)."""
    best = None
    for tx in range(1, n + 1):
        ty = -(-n // tx)
        if tx * ty < n:
            continue
        w, h = tx * n, ty * n
        score = abs(math.log(w / h)) + (tx * ty - n) * 0.01
        if best is None or score < best[0]:
            best = (score, tx, ty, w, h)
    return best[1:]


def atlas_to_volume(layer, n):
    """bytes RGBA8 do atlas → [z][y][x][4] nos eixos do WebGL."""
    tx, ty, w, h = atlas_layout(n)
    a = np.frombuffer(layer, dtype=np.uint8).reshape(h, w, 4)
    v = np.empty((n, n, n, 4), dtype=np.uint8)
    for z in range(n):
        cx, cy = z % tx, z // tx
        v[z] = a[cy * n:(cy + 1) * n, cx * n:(cx + 1) * n]
    return v


def to_ue(v):
    """[z][y][x] do WebGL → [z][y][x] da Unreal: z_ue = y_w, y_ue = z_w."""
    return np.ascontiguousarray(v.transpose(1, 0, 2, *range(3, v.ndim)))


def macro(a, r):
    """Ocupação macro (js/bake.js _macroShader): máximo de max(fuligem, poeira)
    em gamma nos voxels [m·R − R/2, m·R + R + R/2] de cada eixo (o bloco e meio
    bloco de folga pra cada lado), com a borda repetida. Separável: o máximo
    de uma caixa é o máximo por eixo."""
    m = np.maximum(a[..., 0], a[..., 2])
    n = m.shape[0]
    nm = n // r
    lo, hi = -(r // 2), r + r // 2                  # inclusivo
    for axis in range(3):
        out = []
        for k in range(nm):
            i = np.clip(np.arange(k * r + lo, k * r + hi + 1), 0, n - 1)
            out.append(np.take(m, i, axis=axis).max(axis=axis))
        m = np.stack(out, axis=axis)
    return np.ascontiguousarray(m.astype(np.uint8))


def main():
    res = int(sys.argv[1]) if len(sys.argv) > 1 else 96
    found = glob.glob(os.path.join(BAKES, 'r%d-*.bin' % res))
    if not found:
        sys.exit('não achei %s/r%d-*.bin' % (BAKES, res))
    with open(found[0], 'rb') as f:
        data = f.read()
    try:
        data = gzip.decompress(data)
    except OSError:
        pass
    magic, header = struct.unpack_from('<II', data, 0)
    assert magic == 0x314b4258, 'formato desconhecido'
    meta = json.loads(data[8:8 + header].decode('utf-8').rstrip(' \0'))
    n, nf = meta['res'], meta['fuelRes']
    frames, variants = meta['frames'], meta['variants']
    layers = frames * variants
    off = 8 + header
    fl = layers * 3 * 4
    light_pos = np.frombuffer(data, dtype='<f4', count=layers * 3, offset=off).reshape(layers, 3)
    light_col = np.frombuffer(data, dtype='<f4', count=layers * 3, offset=off + fl).reshape(layers, 3)
    off += 2 * fl
    _, _, w, h = atlas_layout(n)
    _, _, wf, hf = atlas_layout(nf)
    per_layer, per_layer_f = w * h * 4, wf * hf * 4
    vox0, fuel0 = off, off + layers * per_layer
    assert fuel0 + layers * per_layer_f == len(data), 'tamanho inesperado'
    domain = meta['domain'] if isinstance(meta.get('domain'), (int, float)) else 38.0
    bounds = np.asarray(meta['bounds'], dtype=np.float64).reshape(layers, 6)
    r = max(1, n // max(8, n >> 2))
    nm = n // r
    os.makedirs(OUT, exist_ok=True)
    for v in range(variants):
        frames_out = []
        for q in range(frames):
            c = v * frames + q
            a = to_ue(atlas_to_volume(data[vox0 + c * per_layer: vox0 + (c + 1) * per_layer], n))
            u = atlas_to_volume(data[fuel0 + c * per_layer_f: fuel0 + (c + 1) * per_layer_f], nf)
            # velocidade do WebGL (x, y, z) → Unreal (x, z, y): canais (r, g, a, b)
            u = to_ue(u[..., [0, 1, 3, 2]])
            frames_out.append((a.tobytes(), u.tobytes(), macro(a, r).tobytes()))
        b = bounds[v * frames:(v + 1) * frames]
        lp = light_pos[v * frames:(v + 1) * frames]
        lc = light_col[v * frames:(v + 1) * frames]
        head = {
            'source': os.path.relpath(found[0], REPO).replace('\\', '/'),
            'res': n, 'fuelRes': nf, 'macroRes': nm, 'frames': frames,
            'duration': meta['duration'], 'curve': meta['curve'], 'domain': domain,
            'velMax': meta['velMax'],
            # caixa justa (x, z, y do WebGL → x, y, z da Unreal), m locais
            'bounds': [[round(float(x), 4) for x in (k[0], k[2], k[1], k[3], k[5], k[4])] for k in b],
            'lightPos': [[round(float(x), 5) for x in (k[0], k[2], k[1])] for k in lp],
            'lightCol': [[round(float(x), 6) for x in k] for k in lc],
        }
        hb = json.dumps(head, separators=(',', ':')).encode('utf-8')
        hb += b' ' * ((-len(hb)) % 16)
        path = os.path.join(OUT, 'r%d_v%d.bin' % (res, v))
        with open(path, 'wb') as f:
            f.write(struct.pack('<4sI', b'BRM1', len(hb)))
            f.write(hb)
            for fa, ff, fm in frames_out:
                f.write(fa)
                f.write(ff)
                f.write(fm)
        occ = [float((np.frombuffer(fm, np.uint8) > 1).mean()) for _, _, fm in frames_out]
        print('%s: %d quadros, %.0f MB, macro ocupado %.1f%%–%.1f%%' % (
            path, frames, os.path.getsize(path) / 2 ** 20, 100 * min(occ), 100 * max(occ)))


if __name__ == '__main__':
    main()
