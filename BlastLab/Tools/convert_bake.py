"""Converte uma explosão gravada no WebGL (reference/webgl/assets/bake/r<res>-<hash>.bin)
em quadros densos que o módulo C++ (BlastLabEditor) transforma num Sparse
Volume Texture animado.

Formato de entrada (js/bake.js, pack()): gzip de
  uint32 'XBK1' · uint32 tamanho do cabeçalho · cabeçalho JSON ·
  posição da luz por camada (float32 ×3) · cor da luz por camada (float32 ×3) ·
  voxels (camadas × atlas RGBA8) · combustível+velocidade (camadas × atlas RGBA8, meia resolução)
Camada = variante·quadros + quadro; cada camada é um atlas 2D de fatias z
(js/volume.js, VolumeGrid). Voxel RGBA8: r² ·4 = fuligem, g ·1.6 = temperatura,
b² ·2.5 = poeira, a = transmitância do céu (não usada aqui: a Unreal sombreia
de verdade).

Saída (fora do git, em Intermediate/BakeRaw/r<res>_v<variante>/):
  fNNN_a.bin  RGBA8 = (r, g, b, 0) dos voxels, códigos originais
  fNNN_b.bin  RGBA16F = (código do combustível, vel x, vel y, vel z em m/s) nos eixos da Unreal
  meta.json   resolução, tamanho do domínio, tempos dos quadros, curva de luz, caixas
Ordem densa da Unreal (SparseVolumeTextureData.cpp, CreateFromDense):
índice = z·(X·Y) + y·X + x. Eixos: WebGL (x, y pra cima, z) → Unreal
(x, z, y pra cima), como no resto do projeto.

O que fica zero exato nos dois atributos é o que deixa o volume esparso
(blocos inteiros iguais ao valor de fundo não são guardados):
- A: zero onde não há fuligem nem poeira;
- B: a velocidade é a da simulação (ampliada da meia resolução com
  interpolação trilinear, como o WebGL lia) numa FAIXA em volta da fumaça, e
  zero só além dela. A mistura com vetores de movimento entre dois quadros
  lê a velocidade do quadro de baixo no ponto de destino; na borda que
  avança a fumaça entra onde antes não havia nada, e sem velocidade ali o
  quadro de cima aparecia como um fantasma à frente (a mesma razão de as
  ferramentas de flipbook dilatarem os vetores além da máscara). Em 16 bits
  com sinal, zero é parado de verdade — em 8 bits sem sinal o byte zero era
  −40 m/s em cada eixo.

Rodar: python BlastLab/Tools/convert_bake.py [res] [variante]   (sem variante: todas)
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
    """layer: bytes RGBA8 do atlas → array [z][y][x][4] nos eixos do WebGL."""
    tx, ty, w, h = atlas_layout(n)
    a = np.frombuffer(layer, dtype=np.uint8).reshape(h, w, 4)
    v = np.empty((n, n, n, 4), dtype=np.uint8)
    for z in range(n):
        cx, cy = z % tx, z // tx
        v[z] = a[cy * n:(cy + 1) * n, cx * n:(cx + 1) * n]
    return v


def upsample_trilinear(u, factor):
    """[z][y][x][c] de meia resolução → resolução cheia, interpolação
    trilinear entre centros de célula (a borda repete a última célula)."""
    for axis in range(3):
        n = u.shape[axis]
        x = (np.arange(n * factor) + 0.5) / factor - 0.5      # centro fino em coordenadas da grade grossa
        i0 = np.clip(np.floor(x).astype(int), 0, n - 1)
        i1 = np.clip(i0 + 1, 0, n - 1)
        t = np.clip(x - np.floor(x), 0, 1)
        t = np.where(x < 0, 0.0, np.where(x > n - 1, 1.0, t)).astype(np.float32)
        shape = [1] * u.ndim
        shape[axis] = -1
        t = t.reshape(shape)
        u = np.take(u, i0, axis=axis) * (1 - t) + np.take(u, i1, axis=axis) * t
    return u


def dilate(m, radius):
    """máscara booleana [z][y][x] dilatada por um cubo de ±`radius` voxels
    (separável: um eixo de cada vez, o cubo cobre a esfera do deslocamento)."""
    m = m.copy()
    for axis in range(3):
        for _ in range(radius):
            d = m.copy()
            d[(slice(None),) * axis + (slice(1, None),)] |= m[(slice(None),) * axis + (slice(None, -1),)]
            d[(slice(None),) * axis + (slice(None, -1),)] |= m[(slice(None),) * axis + (slice(1, None),)]
            m = d
    return m


def tight_box(m):
    """[min, máx) por eixo (x, y, z da Unreal) da fumaça numa máscara [z][y][x] já nos eixos da Unreal."""
    if not m.any():
        return None
    ext = []
    for axis in (2, 1, 0):                         # x, y, z
        i = np.where(m.any(axis=tuple(j for j in range(3) if j != axis)))[0]
        ext.append((int(i.min()), int(i.max()) + 1))
    return ext


# Margem da caixa marchada em volta da fumaça (voxels): a leitura trilinear e
# o deslocamento do ruído de detalhe passam um pouco da última célula com fumaça.
BOX_MARGIN = 2

# Faixa da velocidade em volta da fumaça, em voxels: maior que o maior
# deslocamento da fumaça entre dois quadros gravados (medido na conversão e
# impresso no fim; o WebGL media ≤ 5 células por intervalo).
VELOCITY_BAND = 8


def main():
    res = int(sys.argv[1]) if len(sys.argv) > 1 else 96
    variant = int(sys.argv[2]) if len(sys.argv) > 2 else None
    found = glob.glob(os.path.join(BAKES, 'r%d-*.bin' % res))
    if not found:
        sys.exit('não achei %s/r%d-*.bin' % (BAKES, res))
    with open(found[0], 'rb') as f:
        data = f.read()
    try:
        data = gzip.decompress(data)
    except OSError:
        pass                                   # asset salvo sem compressão
    magic, header = struct.unpack_from('<II', data, 0)
    assert magic == 0x314b4258, 'formato desconhecido'
    meta = json.loads(data[8:8 + header].decode('utf-8').rstrip(' \0'))
    n, nf = meta['res'], meta['fuelRes']
    frames, variants = meta['frames'], meta['variants']
    layers = frames * variants
    off = 8 + header
    fl = layers * 3 * 4
    light_pos = np.frombuffer(data, dtype='<f4', count=layers * 3, offset=off).reshape(layers, 3)
    light_color = np.frombuffer(data, dtype='<f4', count=layers * 3, offset=off + fl).reshape(layers, 3)
    off += 2 * fl
    _, _, w, h = atlas_layout(n)
    _, _, wf, hf = atlas_layout(nf)
    per_layer, per_layer_f = w * h * 4, wf * hf * 4
    vox0 = off
    fuel0 = off + layers * per_layer
    assert fuel0 + layers * per_layer_f == len(data), 'tamanho inesperado'
    for v in ([variant] if variant is not None else range(variants)):
        convert(found[0], meta, data, n, nf, frames, light_pos, light_color, per_layer, per_layer_f, vox0, fuel0, res, v)


def convert(source, meta, data, n, nf, frames, light_pos, light_color, per_layer, per_layer_f, vox0, fuel0, res, variant):
    out = os.path.join(PROJECT, 'Intermediate', 'BakeRaw', 'r%d_v%d' % (res, variant))
    os.makedirs(out, exist_ok=True)
    factor = n // nf
    domain_m = meta['domain'] if isinstance(meta.get('domain'), (int, float)) else 38.0
    voxel_m = domain_m / n
    times = [meta['duration'] * (q / (frames - 1)) ** meta['curve'] for q in range(frames)]
    occupancy, band_occupancy, displacement, tight = [], [], [], []
    for q in range(frames):
        c = variant * frames + q
        v = atlas_to_volume(data[vox0 + c * per_layer: vox0 + (c + 1) * per_layer], n)
        u = atlas_to_volume(data[fuel0 + c * per_layer_f: fuel0 + (c + 1) * per_layer_f], nf)
        u = upsample_trilinear(u.astype(np.float32) / 255.0, factor)
        empty = (v[..., 0] == 0) & (v[..., 2] == 0)
        band = dilate(~empty, VELOCITY_BAND)
        a = v.copy()
        a[..., 3] = 0
        a[empty] = 0
        # B: código do combustível e velocidade em m/s; vel do WebGL (x, y, z) → Unreal (x, z, y)
        vel = (u[..., 1:4] * 2.0 - 1.0) * meta['velMax']
        b = np.stack([u[..., 0], vel[..., 0], vel[..., 2], vel[..., 1]], axis=-1)
        b[~band] = 0
        b[empty, 0] = 0                                 # combustível só onde há fumaça
        if q + 1 < frames:                             # maior deslocamento até o próximo quadro, em voxels
            speed = np.linalg.norm(vel[~empty], axis=-1)
            displacement.append(float(np.percentile(speed, 99.5)) * (times[q + 1] - times[q]) / voxel_m
                                if speed.size else 0.0)
        # [z][y][x] do WebGL → [z][y][x] da Unreal: z_ue = y_w, y_ue = z_w
        a = np.ascontiguousarray(a.transpose(1, 0, 2, 3))
        b = np.ascontiguousarray(b.transpose(1, 0, 2, 3).astype('<f2'))
        a.tofile(os.path.join(out, 'f%03d_a.bin' % q))
        b.tofile(os.path.join(out, 'f%03d_b.bin' % q))
        occupancy.append(float((~empty).mean()))
        band_occupancy.append(float(band.mean()))
        tight.append(tight_box(np.ascontiguousarray((~empty).transpose(1, 0, 2))))

    # Caixa marchada do quadro f = união da fumaça de f e f+1 (a mistura com
    # vetores de movimento entre os dois nunca sai dela) + margem. O
    # Heterogeneous Volume marcha a caixa inteira, voxel a voxel, avaliando o
    # material em cada passo: o custo é proporcional ao volume dela, e a caixa
    # justa ocupa de ~1% a ~33% do domínio de 38 m.
    boxes = []
    for q in range(frames):
        pair = [c for c in (tight[q], tight[min(q + 1, frames - 1)]) if c]
        if not pair:
            boxes.append([[0, 0, 0], [1, 1, 1]])
            continue
        lo = [max(min(c[k][0] for c in pair) - BOX_MARGIN, 0) for k in range(3)]
        hi = [min(max(c[k][1] for c in pair) + BOX_MARGIN, n) for k in range(3)]
        boxes.append([lo, [hi[k] - lo[k] for k in range(3)]])

    variant_layers = slice(variant * frames, (variant + 1) * frames)
    info = {
        'source': os.path.relpath(source, REPO).replace('\\', '/'),
        'resolution': [n, n, n],
        'domain_m': domain_m,
        'frames': frames, 'variant': variant, 'duration_s': meta['duration'], 'curve': meta['curve'],
        'frame_times_s': times,
        # luz: posição relativa ao pé da explosão (m, WebGL) e cor/potência
        'light_pos_webgl': light_pos[variant_layers].tolist(),
        'light_color': light_color[variant_layers].tolist(),
        'occupancy': occupancy,
        'velocity_occupancy': band_occupancy,
        'velocity_band_voxels': VELOCITY_BAND,
        # por quadro: [mínimo, tamanho] em voxels, eixos x, y, z da Unreal
        'boxes': boxes,
        'encoding': {'soot': 'A.r^2*4', 'temperature': 'A.g*1.6', 'dust': 'A.b^2*2.5',
                     'fuel': 'B.r^2', 'velocity': 'B.gba in m/s (half float)'},
    }
    with open(os.path.join(out, 'meta.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f)
    print('converted', n, 'frames', frames, 'variant', variant, '->', out,
          '| mean occupancy %.1f%% (velocity %.1f%%)' % (100 * np.mean(occupancy), 100 * np.mean(band_occupancy)),
          '| displacement between frames (p99.5) max %.1f voxels, median %.1f' % (max(displacement), np.median(displacement)))


if __name__ == '__main__':
    main()
