"""Adapta os passes EmberGen ao formato de atlas do FXLab, sem alterar a simulação.

Executar com o Python do Blender (numpy + OpenImageIO). Entrada: pasta exportada,
saída: pasta nova em captures/fxlab/bake. A iluminação deve estar no espaço da câmera.
TLR = topo/esquerda/direita; BBF = baixo/trás/frente (documentação JangaFX).
"""
import argparse
import json
from pathlib import Path
import sys
import numpy as np
import OpenImageIO as oiio

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--side', type=float, required=True)
p.add_argument('--premultiplied-light', action='store_true')
args = p.parse_args(sys.argv[sys.argv.index('--') + 1:])

def read(suffix):
    files = list(args.source.glob('*' + suffix + '.png'))
    if len(files) != 1:
        raise ValueError(f'Esperado um arquivo {suffix}: {files}')
    inp = oiio.ImageInput.open(str(files[0]))
    arr = inp.read_image(format=oiio.FLOAT)
    inp.close()
    return np.asarray(arr)

beauty = read('render_all')
tlr = read('six_point_1_tlr')
bbf = read('six_point_2_bbf')
assert beauty.shape[2] == 4, 'Exportar Render All com alfa'
assert tlr.shape[:2] == bbf.shape[:2] == beauty.shape[:2]
h, w = beauty.shape[:2]
assert h == w and w % 8 == 0
alpha = beauty[:, :, 3:4]
pos = np.concatenate((tlr[:, :, 2:3], tlr[:, :, 0:1], bbf[:, :, 2:3], alpha), axis=2)
neg = np.concatenate((tlr[:, :, 1:2], bbf[:, :, 0:1], bbf[:, :, 1:2], alpha), axis=2)
if not args.premultiplied_light:
    pos[:, :, :3] *= alpha
    neg[:, :, :3] *= alpha
emit = np.zeros_like(pos)
emit[:, :, 3:4] = alpha
args.output.mkdir(parents=True, exist_ok=True)
for name, data in [('six_pos', pos), ('six_neg', neg), ('emit', emit)]:
    out = oiio.ImageOutput.create(str(args.output / (name + '.exr')))
    spec = oiio.ImageSpec(w, h, 4, oiio.HALF)
    spec.channelnames = ['R', 'G', 'B', 'A']
    out.open(str(args.output / (name + '.exr')), spec)
    out.write_image(np.ascontiguousarray(data))
    out.close()
# Pivô medido no primeiro quadro não vazio: mantém a origem da emissão fixa.
res = w // 8
pivot = [0.0, 0.0]
for f in range(64):
    a = alpha[(f // 8)*res:(f // 8+1)*res, (f % 8)*res:(f % 8+1)*res, 0]
    if a.sum() > 1:
        yy, xx = np.indices(a.shape)
        pivot = [float(((xx + .5)*a).sum()/a.sum()/res-.5), float(.5-((yy+.5)*a).sum()/a.sum()/res)]
        break
meta = dict(cols=8, rows=8, frames=64, res=res, times_s=[i/15 for i in range(64)],
            side_m=args.side, pivot=pivot, ball_bottom=[0.0]*64, fire_end=0,
            source='EmberGen sand_impact_dust_v01', camera_elev_deg=31.5,
            notes='Estudo inicial; escala de composição, não medida física da referência.')
(args.output/'atlas.json').write_text(json.dumps(meta, indent=2), encoding='utf-8')
print(json.dumps({'output': str(args.output), 'pivot': pivot, 'resolution': w}))
