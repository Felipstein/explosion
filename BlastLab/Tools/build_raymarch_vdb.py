"""Uma sequência OpenVDB (EmberGen, Mantaflow...) no formato do raymarch próprio (BRM1).

O teste de 09/10 ("mudar quem grava"): a maciez de perto vem da gravação do
WebGL (0.4 m por voxel, a simulação espalha os redemoinhos pequenos), não da luz
nem da tela. Este script põe outra gravação no MESMO renderizador, lado a lado
com a nossa (-RmCompare=<nome> no app), pra ver se uma simulação mais fina tira
a maciez antes de simular a Carga Pesada em cada ferramenta.

Mesmo formato do build_raymarch_bake.py (ver lá): 'BRM1' · cabeçalho JSON ·
por quadro A (res³ RGBA8: √(fuligem/4), T/1.6, √(poeira/2.5), transmitância do
céu) · F ((res/2)³ RGBA8: √combustível, velocidade) · M ((res/4)³ R8: macro).
- fuligem, temperatura e combustível: os mesmos fatores do convert_pyro_vdb.py
  (07/10, ajustados contra o vídeo: density × 0.35, calor × 0.40 + chama × 0.55);
- transmitância do céu: até o topo, reto pra cima, como o volume de luz do
  WebGL (js/fluid.js shLight, canal y), mesma extinção e mesma erosão média;
- velocidade: o Mantaflow grava (m/s); os VDBs da JangaFX não trazem e ela
  fica zero (sem mistura com movimento entre os quadros, que então são mais
  densos: 90 em 3.6 s);
- o domínio (m locais, escala 1 da Carga Pesada) escolhe o tamanho: 30.75 m põe
  a bola de gasolina da JangaFX (17.1 m na escala da arte) do tamanho da Carga
  Pesada nos mesmos instantes; 0 = o tamanho real do VDB (o Mantaflow, em metros).
Os nomes dos canais dizem a fonte: EmberGen (density, flames, temperature) ou
Mantaflow (density, flame, temperature, velocity; o calor dele vai a ~1, com os
fatores do convert_pyro_vdb.py pra ele: calor × 0.30 + chama × 1.3).

Rodar (no Python do Blender, que traz openvdb e numpy):
  blender -b --factory-startup -P BlastLab/Tools/build_raymarch_vdb.py -- <pasta_vdb> <nome> [res] [domínio_m] [quadros] [qps] [fator_calor]
(a pasta do Mantaflow é a cache/data dele). Sai em BlastLab/Intermediate/Raymarch/<nome>.brm2 (em blocos,
brm2.py; a densa intermediária .bin é apagada no fim).
"""
import glob
import json
import os
import re
import struct
import sys

import numpy as np
import openvdb as vdb

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import brm2  # noqa: E402

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
SRC = os.path.abspath(argv[0])
NAME = argv[1]
RES = int(argv[2]) if len(argv) > 2 else 0
DOMAIN = float(argv[3]) if len(argv) > 3 else 0.0
FRAMES = int(argv[4]) if len(argv) > 4 else 90
FPS = float(argv[5]) if len(argv) > 5 else 36.0
# a temperatura de cada programa vem numa unidade: um fator que iguala o calor mediano da fumaça aos
# 0,6 s ao da gravação do WebGL (0,72; 09/10: EmberGen da JangaFX × 4, Mantaflow da carga × 0,5)
TEMP_K = float(argv[6]) if len(argv) > 6 else 1.0
CURVE = 1.4                 # quadros mais densos no começo (js/bake.js timeOfFrame)
TOOLS = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(TOOLS, '..'))
OUT = os.path.join(PROJECT, 'Intermediate', 'Raymarch')

# convert_pyro_vdb.py (07/10): EmberGen e Mantaflow
SOOT_K = 0.35
TEMP = {'flames': (0.40, 0.55), 'flame': (0.30, 1.3)}
# BlastRaymarch.cpp (os do WebGL)
SOOT_EXT, DUST_EXT, ERODE_MEAN = 13.5, 5.5, 0.042 * 0.5
VEL_MAX = 40.0


def read(path, names):
    grids, _ = vdb.readAll(path)
    by = {g.name: g for g in grids}
    return {n: by.get(n) for n in names}


def resample(a, sizes):
    """[z][y][x] → tamanhos por eixo, linear separável entre centros."""
    for axis in range(3):
        m, n = a.shape[axis], sizes[axis]
        if m == n:
            continue
        x = (np.arange(n) + 0.5) * m / n - 0.5
        i0 = np.clip(np.floor(x).astype(int), 0, m - 1)
        i1 = np.clip(i0 + 1, 0, m - 1)
        t = np.clip(x - np.floor(x), 0.0, 1.0).astype(np.float32)
        shape = [1] * a.ndim
        shape[axis] = -1
        t = t.reshape(shape)
        a = np.take(a, i0, axis=axis) * (1 - t) + np.take(a, i1, axis=axis) * t
    return a.astype(np.float32)


def macro(a, r):
    """O mesmo de build_raymarch_bake.py: máximo de max(fuligem, poeira) em gamma
    no bloco e meio bloco de folga pra cada lado."""
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


def main():
    files = sorted(glob.glob(os.path.join(SRC, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
    if not files:
        sys.exit('sem .vdb em ' + SRC)
    # a união das caixas ativas → um cubo com o pé no chão (convert_pyro_vdb.py EmberGenSource)
    lo, hi = [10 ** 9] * 3, [-10 ** 9] * 3
    names, voxel = set(), 0.0
    for f in files[::max(1, len(files) // 16)] + files[-1:]:
        grids, _ = vdb.readAll(f)
        names |= {g.name for g in grids}
        g = next((g for g in grids if g.name == 'density'), None)
        if g is None or g.activeVoxelCount() == 0:
            continue
        voxel = g.transform.voxelSize()[0]
        a, b = g.evalActiveVoxelBoundingBox()
        lo = [min(lo[i], a[i]) for i in range(3)]
        hi = [max(hi[i], b[i]) for i in range(3)]
    flame = 'flames' if 'flames' in names else 'flame'
    temp_heat, temp_flame = TEMP[flame]
    has_vel = 'velocity' in names
    lo[2] = min(lo[2], 0)
    dims = [hi[i] - lo[i] + 1 for i in range(3)]
    side = max(dims)
    n = RES if RES > 0 else -(-side // 8) * 8          # 0 = a resolução nativa da caixa (múltiplo de 8)
    nz, ny, nx = (max(1, int(round(n * d / side))) for d in (dims[2], dims[1], dims[0]))
    ox, oy = (n - nx) // 2, (n - ny) // 2
    domain = DOMAIN if DOMAIN > 0 else side * voxel
    vm = domain / n
    dur = (len(files) - 1) / FPS
    print(f'VDB2BRM {len(files)} quadros a {FPS:g} qps ({dur:.2f} s), canais {sorted(names)}, caixa {dims}, '
          f'cubo {side} → {n}³, domínio {domain:.2f} m ({vm * 100:.1f} cm por voxel)')

    def cube(a):
        """[x][y][z](+canais) do VDB → [z][y][x] da Unreal (y = −y), reamostrado e no pé do cubo."""
        c = np.zeros((n, n, n) + a.shape[3:], dtype=np.float32)
        a = np.ascontiguousarray(np.transpose(a, (2, 1, 0) + tuple(range(3, a.ndim)))[:, ::-1])
        c[:nz, oy:oy + ny, ox:ox + nx] = resample(a, (nz, ny, nx))
        return c

    def fields(path):
        g = read(path, ['density', flame, 'temperature', 'velocity'])
        out = []
        for name in ('density', flame, 'temperature'):
            a = np.zeros(dims, dtype=np.float32)
            if g[name] is not None:
                g[name].copyToArray(a, ijk=tuple(lo))
            out.append(cube(a))
        vel = None
        if has_vel and g['velocity'] is not None:
            a = np.zeros(dims + [3], dtype=np.float32)
            g['velocity'].copyToArray(a, ijk=tuple(lo))
            vel = cube(a)
            vel[..., 1] *= -1.0
        return out + [vel]

    r = 4
    nm, nf = n // r, n // 2
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, NAME + '.bin')
    tmp = path + '.tmp'
    bounds, light_pos, light_col = [], [], []
    with open(tmp, 'wb') as body:
        for q in range(FRAMES):
            t = dur * (q / (FRAMES - 1)) ** CURVE
            src = files[min(len(files) - 1, int(round(t * FPS)))]
            den, fl, heat, vel = fields(src)
            soot = np.clip(den * SOOT_K, 0.0, 4.0)
            temp = np.clip((heat * temp_heat + fl * temp_flame) * TEMP_K, 0.0, 1.6)
            fuel = np.clip(fl, 0.0, 1.0)
            empty = (soot < 0.003) & (fuel < 0.05)
            soot[empty] = 0.0
            # transmitância até o topo, do centro do voxel (fluid.js shLight, y)
            ext = SOOT_EXT * np.maximum(soot - ERODE_MEAN / (1.0 + soot * 9.0), 0.0)
            above = np.cumsum(ext[::-1], axis=0)[::-1] - 0.5 * ext
            sky = np.exp(-np.minimum(above * vm, 60.0))
            A = np.zeros((n, n, n, 4), dtype=np.uint8)
            A[..., 0] = np.round(np.sqrt(soot / 4.0) * 255)
            A[..., 1] = np.round(temp / 1.6 * 255)
            A[..., 3] = np.round(np.clip(sky, 0.0, 1.0) * 255)
            A[empty, :3] = 0
            F = np.full((nf, nf, nf, 4), 128, dtype=np.uint8)
            fh = np.sqrt(fuel).reshape(nf, 2, nf, 2, nf, 2).mean(axis=(1, 3, 5))
            F[..., 0] = np.round(np.clip(fh, 0.0, 1.0) * 255)
            if vel is not None:
                # BlastRaymarch.usf BakeVel: (byte/255·2 − 1)·velMax, canais g, b, a = x, y, z da Unreal
                vh = vel.reshape(nf, 2, nf, 2, nf, 2, 3).mean(axis=(1, 3, 5))
                F[..., 1:] = np.round(np.clip(vh / VEL_MAX * 0.5 + 0.5, 0.0, 1.0) * 255)
            body.write(A.tobytes())
            body.write(F.tobytes())
            body.write(macro(A, r).tobytes())
            # caixa justa e a luz do fogo (centro da emissão, peso fogo · T⁴), m locais, eixos da Unreal
            occ = ~empty
            if occ.any():
                ext3 = []
                for axis in (2, 1, 0):
                    i = np.where(occ.any(axis=tuple(j for j in range(3) if j != axis)))[0]
                    ext3.append((int(i.min()), int(i.max()) + 1))
                bmin = [-domain / 2 + ext3[0][0] * vm, -domain / 2 + ext3[1][0] * vm, ext3[2][0] * vm]
                bmax = [-domain / 2 + ext3[0][1] * vm, -domain / 2 + ext3[1][1] * vm, ext3[2][1] * vm]
                bounds.append([round(v, 4) for v in bmin + bmax])
            else:
                bounds.append([0.0] * 6)
            w = fuel * temp ** 4
            tot = float(w.sum())
            if tot > 1e-6:
                zz, yy, xx = np.indices(w.shape, sparse=True)
                c = [float((w * xx).sum() / tot) + 0.5, float((w * yy).sum() / tot) + 0.5, float((w * zz).sum() / tot) + 0.5]
                light_pos.append([round((c[0] - n / 2) * vm, 5), round((c[1] - n / 2) * vm, 5), round(c[2] * vm, 5)])
            else:
                light_pos.append([0.0, 0.0, 2.5])
            light_col.append(tot)
            print(f'VDB2BRM quadro {q} ({t:.2f} s ← {os.path.basename(src)}): {100 * occ.mean():.2f}% ocupado')
    # a potência da luz: pico 83 com a cor quente do WebGL (convert_pyro_vdb.py finish)
    peak = max(light_col) or 1.0
    light_col = [[round(83.0 * c / peak, 6), round(83.0 * c / peak * 0.587, 6), round(83.0 * c / peak * 0.277, 6)] for c in light_col]
    head = {
        'source': os.path.relpath(SRC, os.path.join(PROJECT, '..')).replace('\\', '/'),
        'res': n, 'fuelRes': nf, 'macroRes': nm, 'frames': FRAMES,
        'duration': dur, 'curve': CURVE, 'domain': domain, 'velMax': VEL_MAX,
        'bounds': bounds, 'lightPos': light_pos, 'lightCol': light_col,
    }
    hb = json.dumps(head, separators=(',', ':')).encode('utf-8')
    hb += b' ' * ((-len(hb)) % 16)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sI', b'BRM1', len(hb)))
        f.write(hb)
        with open(tmp, 'rb') as body:
            while True:
                chunk = body.read(1 << 26)
                if not chunk:
                    break
                f.write(chunk)
    os.remove(tmp)
    # em blocos esparsos (o formato que o jogo lê); a densa sai
    head1, frames1 = brm2.read_brm1(path)
    out = os.path.join(OUT, NAME + '.brm2')
    counts = brm2.write(out, head1, frames1)
    os.remove(path)
    print('VDB2BRM pronto: %s, %d quadros, %.0f MB, blocos por quadro até %d' % (out, FRAMES, os.path.getsize(out) / 2 ** 20, max(counts)))


main()
