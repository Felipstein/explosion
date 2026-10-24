"""A folha A/B dos vídeos do jogo (23/10 noite): os mesmos instantes de cada vídeo (-Record, 30 qps), lado a lado, inteiros ou ampliados
(o recorte no meio da tela, onde a câmera da cena olha) — a prova "ampliada, lado a lado" do CLAUDE.md.

  python ab_sheet.py <saida.jpg> <tempos_s> <recorte: 1 = inteiro, 0.5 = o meio, 2× ampliado; ou x0,y0,x1,y1 em fração> rótulo=video.mp4 ...
"""
import os
import subprocess
import sys

import imageio_ffmpeg
from PIL import Image, ImageDraw

FF = imageio_ffmpeg.get_ffmpeg_exe()
out, times, crop = sys.argv[1], [float(t) for t in sys.argv[2].split(',')], sys.argv[3]
vids = [a.split('=', 1) for a in sys.argv[4:]]
tmp = os.path.join(os.path.dirname(os.path.abspath(out)), '_frame.png')
W_OUT = int(os.environ.get('ABW', '640'))
cols = []
for label, mp4 in vids:
    tiles = []
    off = 0.0
    if '@' in label:
        label, o = label.split('@')
        off = float(o)
    for t0 in times:
        t = t0 + off
        if os.path.exists(tmp):
            os.remove(tmp)
        subprocess.run([FF, '-y', '-loglevel', 'error', '-ss', f'{t:.3f}', '-i', mp4, '-frames:v', '1', tmp], check=True)
        im = Image.open(tmp).convert('RGB')
        w, h = im.size
        if ',' in crop:
            x0, y0, x1, y1 = (float(v) for v in crop.split(','))
        else:
            f = float(crop)
            x0, y0, x1, y1 = 0.5 - f / 2, 0.5 - f / 2, 0.5 + f / 2, 0.5 + f / 2
        im = im.crop((int(x0 * w), int(y0 * h), int(x1 * w), int(y1 * h)))
        im = im.resize((W_OUT, int(W_OUT * im.height / im.width)), Image.LANCZOS)
        ImageDraw.Draw(im).text((6, 4), f'{label}  t={t0:.1f}s', fill=(255, 255, 0))
        tiles.append(im)
    cols.append(tiles)
tw, th = cols[0][0].size
sheet = Image.new('RGB', (tw * len(cols) + 4 * (len(cols) - 1), th * len(times)), (0, 0, 0))
for i, c in enumerate(cols):
    for j, im in enumerate(c):
        sheet.paste(im, (i * (tw + 4), j * th))
sheet.save(out, quality=90)
if os.path.exists(tmp):
    os.remove(tmp)
print('AB', out, sheet.size)
