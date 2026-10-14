"""O alvo de perto: o tanque T1 do gameplay_v2 seguido numa janela de 366 × 206 px (o mesmo tamanho de casco na tela
da câmera -Cam=v2close), ampliada pra 1280 × 720 com lanczos, a 30 qps. O rastreio é o do estudo
(captures/fxlab/tread_study/video-frames/track_v2_995_440.json, quadros a 12 qps).
Uso: python tread_v2crop.py <saida.mp4> [segundos=5]"""
import json
import os
import subprocess
import sys
import tempfile

import imageio_ffmpeg
import numpy as np
from PIL import Image

repo = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
out = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0
tr = json.load(open(os.path.join(repo, 'captures/fxlab/tread_study/video-frames/track_v2_995_440.json')))
ts = np.array([p['f'] / 12.0 for p in tr]); xs = np.array([p['x'] for p in tr]); ys = np.array([p['y'] for p in tr])
ff = imageio_ffmpeg.get_ffmpeg_exe()
tmp = tempfile.mkdtemp()
subprocess.run([ff, '-y', '-loglevel', 'error', '-t', str(secs), '-i', os.path.join(repo, 'captures/concept/33_video/gameplay_v2.mp4'),
                '-vf', 'fps=30,scale=1280:720', os.path.join(tmp, 'f%04d.png')], check=True)
W, H = 366, 206
for i, f in enumerate(sorted(os.listdir(tmp))):
    t = i / 30.0
    cx, cy = np.interp(t, ts, xs), np.interp(t, ts, ys)
    im = Image.open(os.path.join(tmp, f)).convert('RGB')
    x0 = int(min(max(cx - W / 2, 0), 1280 - W)); y0 = int(min(max(cy - H / 2, 0), 720 - H))
    im.crop((x0, y0, x0 + W, y0 + H)).resize((1280, 720), Image.LANCZOS).save(os.path.join(tmp, f))
subprocess.run([ff, '-y', '-loglevel', 'error', '-framerate', '30', '-i', os.path.join(tmp, 'f%04d.png'), '-c:v', 'libx264',
                '-pix_fmt', 'yuv420p', '-crf', '18', out], check=True)
for f in os.listdir(tmp):
    os.remove(os.path.join(tmp, f))
os.rmdir(tmp)
print('ok', out)
