# (05/11) A animação da prancha da mira dos poderes (mira.html): grava quadro a quadro pelo Edge (o #t congela o instante) e monta o
# GIF das 6 formas, a 1/2, pra conferir em movimento. Os quadros ficam em captures/concept/cursores/gif_mira (rascunho).
import os, subprocess
from PIL import Image

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
URL = "file:///" + os.path.join(HERE, "mira.html").replace("\\", "/")
FPS, SECONDS = 10, 6.0
OUT = os.path.join(ROOT, "captures", "concept", "cursores", "gif_mira")
os.makedirs(OUT, exist_ok=True)


def shot(t):
    path = os.path.join(OUT, f"m_{t:05.2f}.png")
    if not os.path.exists(path):
        subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=1600,1420",
                        f"--screenshot={path}", f"{URL}#t={t:.3f}"], capture_output=True)
    return path


frames = []
for k in range(int(SECONDS * FPS)):
    im = Image.open(shot(k / FPS)).convert("RGB").crop((0, 100, 1600, 1420))
    frames.append(im.resize((im.width // 2, im.height // 2), Image.LANCZOS))
pal = [f.convert("P", palette=Image.ADAPTIVE, colors=220) for f in frames]
gif = os.path.join(ROOT, "captures", "concept", "cursores", "mira_poderes.gif")
pal[0].save(gif, save_all=True, append_images=pal[1:], duration=int(1000 / FPS), loop=0)
print(gif, len(frames), "quadros")
