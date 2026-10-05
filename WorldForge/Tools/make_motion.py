"""Junta os quadros do passeio de câmera (motion_NNN.png) numa animação WebP.

  python WorldForge/Tools/make_motion.py captures/worldforge/<pasta> [largura]
Sem ffmpeg nesta máquina: o Pillow grava WebP animado (30 qps, com perda leve).
"""
import glob
import os
import sys

from PIL import Image

folder = sys.argv[1]
width = int(sys.argv[2]) if len(sys.argv) > 2 else 960
frames = sorted(glob.glob(os.path.join(folder, 'motion_*.png')))
if not frames:
    sys.exit('sem quadros em ' + folder)
ims = []
for f in frames:
    im = Image.open(f).convert('RGB')
    ims.append(im.resize((width, int(width * im.height / im.width)), Image.LANCZOS))
out = os.path.join(folder, 'motion.webp')
ims[0].save(out, save_all=True, append_images=ims[1:], duration=33, loop=0, quality=82, method=4)
print('motion', out, len(ims), 'quadros', round(os.path.getsize(out) / 1e6, 1), 'MB')
