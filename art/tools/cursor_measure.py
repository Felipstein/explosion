# Mede os cursores de art/cursors/cursors.html rasterizados em N px (o modo #raster=N da prancha, pelo Edge headless):
# a caixa de cada um, o centro de massa contra o ponto do clique e o borrão (a parte da borda meio transparente).
# Uso: python art/tools/cursor_measure.py [48]   (a imagem vai pra captures/concept/cursores/raster_<N>.png)
import os, re, subprocess, sys
from PIL import Image
import numpy as np

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BOARD = os.path.join(ROOT, "art", "cursors", "cursors.html")
OUT = os.path.join(ROOT, "captures", "concept", "cursores")


def measure(px):
    html = open(BOARD, encoding="utf-8").read()
    tpl = html[html.index('<template id="glyphs">'):html.index("</template>")]
    items = re.findall(r'<svg data-id="([^"]+)"[^>]*?data-hot="([\d.]+),([\d.]+)"', tpl)
    step = px + 16
    rows = (len(items) + 7) // 8
    os.makedirs(OUT, exist_ok=True)
    shot = os.path.join(OUT, f"raster_{px}.png")
    url = "file:///" + BOARD.replace("\\", "/") + f"#raster={px}"
    subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=1",
                    "--default-background-color=00000000", f"--window-size={8 * step + 16},{rows * step + 16}",
                    f"--screenshot={shot}", url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    im = np.asarray(Image.open(shot).convert("RGBA")).astype(float)
    res = {}
    for k, (cid, hx, hy) in enumerate(items):
        x0, y0 = (k % 8) * step + 8, (k // 8) * step + 8
        a = im[y0:y0 + px, x0:x0 + px, 3] / 255
        ys, xs = np.nonzero(a > 0.5)
        tot = a.sum()
        # o centro do pixel i fica em i + 0,5
        cx = (a * (np.arange(px)[None, :] + 0.5)).sum() / tot
        cy = (a * (np.arange(px)[:, None] + 0.5)).sum() / tot
        part, solid = ((a > 0.08) & (a < 0.92)).sum(), (a >= 0.92).sum()
        s = px / 64
        res[cid] = dict(w=xs.max() - xs.min() + 1, h=ys.max() - ys.min() + 1, dx=cx - float(hx) * s, dy=cy - float(hy) * s,
                        blur=part / max(1, part + solid),
                        edge=bool(xs.min() < 1 or ys.min() < 1 or xs.max() > px - 2 or ys.max() > px - 2))
    return res


if __name__ == "__main__":
    px = int(sys.argv[1]) if len(sys.argv) > 1 else 48
    for cid, m in measure(px).items():
        print(f"{cid:9s} {m['w']:3d}x{m['h']:<3d} centro - clique: dx={m['dx']:+5.1f} dy={m['dy']:+5.1f}   "
              f"borrão {m['blur'] * 100:3.0f}%  {'ENCOSTA NA BORDA' if m['edge'] else ''}")
