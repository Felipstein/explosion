# O ícone do .exe do jogo (06/11, ele: "eu fiz esse icon um tempo atrás, eu gosto dele, até o Atelier tem ele, mas não é pra usar
# no menu [...] dá até pra colocar no .exe do jogo"). A arte é a dele: o tanque de chapas enferrujadas em Atelier/app/icon/game_icon.webp
# (o mesmo de onde sai o ícone dourado do Atelier, Atelier/app/icon/make_icon.py). Aqui ele vai com as cores dele, fundo transparente,
# centrado num quadrado. A Unreal usa Game/Build/Windows/Application.ico no .exe do pacote (o editor não muda).
#   python Game/Tools/make_exe_icon.py
import os
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'Atelier', 'app', 'icon', 'game_icon.webp')
OUT = os.path.join(ROOT, 'Game', 'Build', 'Windows', 'Application.ico')
SIZES = [256, 128, 64, 48, 32, 24, 16]

im = Image.open(SRC).convert('RGBA')
im = im.crop(im.getbbox())                       # só o tanque
side = int(max(im.size) * 1.04)                  # um respiro de 2% de cada lado
sq = Image.new('RGBA', (side, side), (0, 0, 0, 0))
sq.paste(im, ((side - im.width) // 2, (side - im.height) // 2), im)
frames = []
for s in SIZES:
    f = sq.resize((s, s), Image.Resampling.LANCZOS)
    if s <= 48:
        # pequeno, o detalhe das chapas vira lama: um pouco de nitidez segura a silhueta
        f = f.filter(ImageFilter.UnsharpMask(radius=1.0, percent=60, threshold=2))
    frames.append(f)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
frames[0].save(OUT, format='ICO', sizes=[(s, s) for s in SIZES], append_images=frames[1:])
sq.resize((256, 256), Image.Resampling.LANCZOS).save(os.path.join(os.path.dirname(OUT), 'Application_256.png'))
print(OUT, os.path.getsize(OUT), 'bytes')
