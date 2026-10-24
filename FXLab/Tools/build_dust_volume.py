"""A densidade do chute da poeira das lagartas como VOLUME (16/10 noite, o redesenho: a poeira vira volume no raymarch
das explosões, com a luz do BlastLab; docs/POEIRA_DAS_LAGARTAS.md). Lê a MESMA gravação do EmberGen que fez as folhas
6-way aprovadas da v26 (captures/vdb/tread_dust/kick_v2, os quadros do tread_kick_v2c.atlas.json) e grava um arquivo
pequeno pro plugin (Resources/dust_kick_v2.rtsdust):
- cabeçalho 'RTSD' + tamanho (uint32) + JSON (quadros, voxels, origem local, tempos, ladrilhos, caixa de cada quadro);
- os quadros em float16, ladrilhados em XY numa textura 3D (cols × rows quadros), z de baixo pra cima, comprimidos com
  zlib (17/10: quase tudo é ar; o de 0,05 m cai de 42 MB pra 1,8 MB; o 'zlib' do cabeçalho é o tamanho descomprimido).
A grade: x, y em [−2,4; 2,4] m e z em [0; 2,4] m (o domínio do cozimento, side_m 4,8, o chão em z = 0), a 0,1 m: a
média de 2×2×2 voxels da gravação (0,05 m), que guarda a massa. A densidade fica nas unidades do EmberGen (a mesma que o
cozimento usou: α = 1 − e^(−6·Σρ²·dx), porque o Principled Volume do Blender multiplica a grade de novo; medido 17/10).
Rodar (o Python do Blender, que traz o openvdb):
  blender -b --factory-startup -P FXLab/Tools/build_dust_volume.py -- [pasta_vdb] [atlas.json] [saída]"""
import json
import os
import struct
import sys
import zlib

import numpy as np
import openvdb

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
SRC = argv[0] if len(argv) > 0 else os.path.join(ROOT, 'captures', 'vdb', 'tread_dust', 'kick_v2')
ATLAS = argv[1] if len(argv) > 1 else os.path.join(ROOT, 'FXLab', 'Plugins', 'RTSTreadDust', 'Resources', 'tread_kick_v2c.atlas.json')
OUT = argv[2] if len(argv) > 2 else os.path.join(ROOT, 'FXLab', 'Plugins', 'RTSTreadDust', 'Resources', 'dust_kick_v2.rtsdust')

atlas = json.load(open(ATLAS, encoding='utf-8'))
frames = atlas['sim_frames']
times = atlas['times_s']
# DUSTVOL_FRAMES=all (22/10, v4): todos os quadros da simulação (30 por segundo) em vez dos 48 da folha, que ficam de 0,033 s
# no começo a 0,2 s no fim; com eles a poeira andava aos pulos ("a poeira parece com um FPS diferente do jogo", o Felipe)
if os.environ.get('DUSTVOL_FRAMES') == 'all':
    frames = sorted(k for k in (int(f[3:-4]) for f in os.listdir(SRC) if f.startswith('cb_') and f.endswith('.vdb')) if k >= 1)
    times = [round((k - 1) / 30.0, 6) for k in frames]
# DUSTVOL_RHO2=1: grava a média de ρ² (o filtro certo de σ ∝ ρ²; o shader lê sem elevar, 'rho2' no cabeçalho)
RHO2 = os.environ.get('DUSTVOL_RHO2') == '1'
HALF = 2.4                     # m: o domínio em x e y vai de −2,4 a 2,4
ZTOP = 2.4                     # m: o chão em 0; a nuvem vai até ~1,2 m na escala 1
DX = float(os.environ.get('DUSTVOL_DX', '0.1'))   # m por voxel (0,1 = a média 2x2x2 da gravação; 0,05 = a gravação inteira)
NX, NZ = int(round(2 * HALF / DX)), int(round(ZTOP / DX))   # 48 × 48 × 24
COLS = 8
ROWS = (len(frames) + COLS - 1) // COLS


def load(k):
    grids, meta = openvdb.readAll(os.path.join(SRC, 'cb_%d.vdb' % k))
    g = next(x for x in grids if x.name == 'density')
    dxv = g.transform.voxelSize()[0]
    o = g.transform.indexToWorld((0, 0, 0))
    # a grade fina (0,05 m) do domínio, depois a média de 2×2×2
    nf = int(round(2 * HALF / dxv)), int(round(ZTOP / dxv))
    fine = np.zeros((nf[0], nf[0], nf[1]), np.float32)
    (x0, y0, z0), (x1, y1, z1) = g.evalActiveVoxelBoundingBox()
    if x1 >= x0:
        a = np.zeros((x1 - x0 + 1, y1 - y0 + 1, z1 - z0 + 1), np.float32)
        g.copyToArray(a, ijk=(x0, y0, z0))
        # índice da gravação → índice da grade fina (a origem da gravação em −4,8, −4,8, 0)
        ox = int(round((o[0] + HALF) / dxv * -1))
        oy = int(round((o[1] + HALF) / dxv * -1))
        oz = int(round(o[2] / dxv))
        i0, j0, k0 = x0 - ox, y0 - oy, z0 + oz
        si = slice(max(i0, 0), min(i0 + a.shape[0], nf[0]))
        sj = slice(max(j0, 0), min(j0 + a.shape[1], nf[0]))
        sk = slice(max(k0, 0), min(k0 + a.shape[2], nf[1]))
        fine[si, sj, sk] = a[si.start - i0:si.stop - i0, sj.start - j0:sj.stop - j0, sk.start - k0:sk.stop - k0]
        lost = float(a.sum() - fine.sum())
    else:
        lost = 0.0
    f = int(round(DX / dxv))
    coarse = (fine * fine if RHO2 else fine).reshape(NX, f, NX, f, NZ, f).mean(axis=(1, 3, 5))
    return coarse, lost / max(float(fine.sum()), 1e-9)


vols, boxes, maxes = [], [], []
for n, k in enumerate(frames):
    v, lost = load(k)
    vols.append(v)
    nz = np.argwhere(v > 1e-3)
    if len(nz):
        lo = nz.min(0) * DX + np.array([-HALF, -HALF, 0.0])
        hi = (nz.max(0) + 1) * DX + np.array([-HALF, -HALF, 0.0])
    else:
        lo = hi = np.zeros(3)
    boxes.append([round(float(x), 3) for x in list(lo) + list(hi)])
    maxes.append(float(v.max()))
    print('DUSTVOL quadro %d (sim %d) max %.3f caixa %s perdido fora do domínio %.4f' % (n, k, v.max(), boxes[-1], lost))

tex = np.zeros((NZ, ROWS * NX, COLS * NX), np.float16)        # z, y, x (a ordem da memória da textura 3D)
for n, v in enumerate(vols):
    c, r = n % COLS, n // COLS
    tex[:, r * NX:(r + 1) * NX, c * NX:(c + 1) * NX] = np.transpose(v, (2, 1, 0)).astype(np.float16)
header = {
    'version': 1, 'source': os.path.basename(SRC), 'frames': len(frames), 'times_s': times, 'sim_frames': frames,
    'res': [NX, NX, NZ], 'voxel_m': DX, 'origin_m': [-HALF, -HALF, 0.0], 'tiles': [COLS, ROWS],
    'tex_size': [COLS * NX, ROWS * NX, NZ], 'format': 'R16F', 'box_m': boxes, 'max': maxes,
    'density': 'EmberGen density (o cozimento: alpha = 1 - exp(-6 * sum(rho^2) * dx))',
    'zlib': int(tex.nbytes),
}
if RHO2:
    header['rho2'] = 1
    header['density'] = 'mean(rho^2) por voxel (sigma = 6 * valor * density, sem elevar)'
hj = json.dumps(header).encode('utf-8')
with open(OUT, 'wb') as f:
    f.write(b'RTSD')
    f.write(struct.pack('<I', len(hj)))
    f.write(hj)
    f.write(zlib.compress(tex.tobytes(), 6))
print('DUSTVOL gravado', OUT, '%.2f MB' % (os.path.getsize(OUT) / 1e6), 'textura', header['tex_size'])
