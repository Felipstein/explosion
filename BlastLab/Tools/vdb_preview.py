"""A folha de prova de uma cache OpenVDB (16/10 noite): de lado (XZ) e de cima (XY), a fumaça em cinza e a chama em laranja.

  blender -b --factory-startup -P BlastLab/Tools/vdb_preview.py -- <pasta_vdb> <saida.npz> [quadros: 1,5,10,...]
  python BlastLab/Tools/vdb_preview.py <saida.npz> <saida.png>

A 1ª forma (no Python do Blender, que tem o openvdb) soma a densidade e a chama ao longo de Y e de Z; a 2ª monta a folha.
"""
import glob
import os
import re
import sys

import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]

if argv[0].endswith('.npz'):
    from PIL import Image, ImageDraw
    z = np.load(argv[0], allow_pickle=True)
    tiles = []
    for k in range(len(z['frames'])):
        side_d, side_f, top_d, top_f = z['sd'][k], z['sf'][k], z['td'][k], z['tf'][k]

        def shade(dd, ff):
            T = np.exp(-dd * z['vox'] * 2.0)
            fire = 1.0 - np.exp(-ff * z['vox'] * 3.0)
            rgb = np.stack([T * 0.92 + (1 - T) * 0.35, T * 0.92 + (1 - T) * 0.35, T * 0.95 + (1 - T) * 0.37], -1)
            rgb = rgb * (1 - fire[..., None]) + fire[..., None] * np.array([1.0, 0.62, 0.2])
            return (rgb.clip(0, 1) * 255).astype(np.uint8)
        a = shade(side_d.T[::-1], side_f.T[::-1])          # [z][x], o alto em cima
        b = shade(top_d.T, top_f.T)                       # [y][x]
        im = Image.fromarray(np.concatenate([a, np.full((2, a.shape[1], 3), 128, np.uint8), b], 0))
        im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
        ImageDraw.Draw(im).text((4, 4), f'q{int(z["frames"][k])}', fill=(200, 0, 0))
        tiles.append(im)
    cols = 4
    W, H = tiles[0].width, tiles[0].height
    sheet = Image.new('RGB', (W * cols, H * ((len(tiles) + cols - 1) // cols)), 'white')
    for k, im in enumerate(tiles):
        sheet.paste(im, ((k % cols) * W, (k // cols) * H))
    sheet.save(argv[1])
    print('folha', argv[1])
else:
    import openvdb as vdb
    src, out = argv[0], argv[1]
    files = sorted(glob.glob(os.path.join(src, '*.vdb')), key=lambda f: int(re.findall(r'(\d+)\.vdb$', f)[0]))
    want = [int(x) for x in argv[2].split(',')] if len(argv) > 2 else list(range(1, len(files) + 1, max(1, len(files) // 12)))
    lo, hi = [10 ** 9] * 3, [-10 ** 9] * 3
    vox = 0.1
    for f in files:
        g = next((g for g in vdb.readAll(f)[0] if g.name == 'density'), None)
        if g is not None and g.activeVoxelCount():
            vox = g.transform.voxelSize()[0]
            a, b = g.evalActiveVoxelBoundingBox()
            lo = [min(lo[i], a[i]) for i in range(3)]
            hi = [max(hi[i], b[i]) for i in range(3)]
    dims = [hi[i] - lo[i] + 1 for i in range(3)]
    sd, sf, td, tf, fr = [], [], [], [], []
    for n in want:
        f = next((f for f in files if int(re.findall(r'(\d+)\.vdb$', f)[0]) == n), None)
        if f is None:
            continue
        grids = {g.name: g for g in vdb.readAll(f)[0]}
        arr = {}
        for name in ('density', 'flame'):
            a = np.zeros(dims, np.float32)
            if name in grids:
                grids[name].copyToArray(a, ijk=tuple(lo))
            arr[name] = a
        sd.append(arr['density'].sum(1)); sf.append(arr['flame'].sum(1))
        td.append(arr['density'].sum(2)); tf.append(arr['flame'].sum(2))
        fr.append(n)
    np.savez_compressed(out, sd=np.array(sd), sf=np.array(sf), td=np.array(td), tf=np.array(tf), frames=np.array(fr), vox=vox)
    print('prova', out, dims, vox)
