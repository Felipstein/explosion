# (03/11) As animações dos alertas pra ele comparar: grava a prancha alertas.html quadro a quadro (o #t congela o instante) e
# monta três GIFs: o "sob ataque" (hoje × unidade × base), as setas da borda (A, B, C, ampliadas) e o catálogo do minimapa.
import os, subprocess
from PIL import Image, ImageDraw, ImageFont

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
URL = "file:///" + os.path.join(HERE, "alertas.html").replace("\\", "/")
FPS, SECONDS = 12, 6.5
OUT = os.path.join(HERE, "gif_alertas")
os.makedirs(OUT, exist_ok=True)

def shot(tag, t, size, extra=""):
    path = os.path.join(OUT, f"{tag}_{t:05.2f}.png")
    if not os.path.exists(path):
        subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", f"--window-size={size}", f"--screenshot={path}",
                        f"{URL}#t={t:.3f}{extra}"], capture_output=True)
    return path

times = [k / FPS for k in range(int(SECONDS * FPS))]
small = [shot("p", t, "1600,2400") for t in times]
big = [shot("b", t, "1700,4200", "&big") for t in times]

F = ImageFont.truetype("segoeuib.ttf", 17)
def save(frames, name):
    pal = [f.convert("P", palette=Image.ADAPTIVE, colors=220) for f in frames]
    pal[0].save(os.path.join(HERE, name), save_all=True, append_images=pal[1:], duration=int(1000 / FPS), loop=0)

# os recortes (na prancha de 1600 × 2400: a seção 1 e o catálogo; na grande: as setas)
im0 = Image.open(small[0]).convert("RGB")
def rows_of(im, x, need):
    nb = [0 if all(abs(a - b) < 6 for a, b in zip(im.getpixel((x, y)), (21, 23, 26))) else 1 for y in range(im.height)]
    tops, y = [], 1
    while y < im.height - need:
        if nb[y] and not nb[y - 1] and sum(nb[y:y + need]) > 0.95 * need:
            tops.append(y); y += need
        else:
            y += 1
    return tops
sec1 = rows_of(im0, 200, 420)[0]     # os três minimapas a 1,5× (495 × 435)
cat = [t for t in rows_of(im0, 150, 280) if t > sec1 + 600][:3]   # as linhas do catálogo (330 × 290)
print("seção 1", sec1, "catálogo", cat)

def sec1_frame(p):
    im = Image.open(p).convert("RGB")
    names = ["hoje", "novo · unidade sob ataque", "novo · base sob ataque"]
    o = Image.new("RGB", (3 * 505 + 10, 435 + 40), (21, 23, 26)); d = ImageDraw.Draw(o)
    for k in range(3):
        x = 20 + k * (495 + 14)
        d.text((10 + k * 505, 8), names[k], fill=(233, 230, 223), font=F)
        o.paste(im.crop((x, sec1, x + 495, sec1 + 435)), (10 + k * 505, 34))
    return o
save([sec1_frame(p) for p in small], "alertas_sob_ataque.gif")

CATN = ["obra pronta", "unidade pronta", "sua superarma pronta", "superarma inimiga", "sinal do aliado", "invisível descoberto",
        "doca acabou", "a carga que cai", "primeiro contato", "unidade perdida", "o calor da batalha", "sem energia, radar cai"]
def cat_frame(p):
    im = Image.open(p).convert("RGB")
    o = Image.new("RGB", (4 * 340 + 10, 3 * 316 + 10), (21, 23, 26)); d = ImageDraw.Draw(o)
    for r, top in enumerate(cat):
        for c in range(4):
            x = 20 + c * (330 + 14)
            d.text((10 + c * 340, 6 + r * 316), CATN[r * 4 + c], fill=(233, 230, 223), font=F)
            o.paste(im.crop((x, top, x + 330, top + 290)), (10 + c * 340, 30 + r * 316))
    return o
save([cat_frame(p) for p in small], "alertas_catalogo.gif")

bi0 = Image.open(big[0]).convert("RGB")
frs = [t for t in rows_of(bi0, 400, 880) if t > 600][:3]
print("setas", frs)
def arrow_frame(p):
    im = Image.open(p).convert("RGB")
    names = ["A · a seta com a ficha", "B · a seta só", "C · o brilho na borda"]
    o = Image.new("RGB", (3 * 480 + 10, 2 * 250 + 44), (21, 23, 26)); d = ImageDraw.Draw(o)
    for k, top in enumerate(frs):
        x = 10 + k * 480
        d.text((x, 8), names[k], fill=(233, 230, 223), font=F)
        o.paste(im.crop((20, top + 60, 255, top + 180)).resize((470, 240), Image.LANCZOS), (x, 34))
        o.paste(im.crop((1050, top + 70, 1285, top + 190)).resize((470, 240), Image.LANCZOS), (x, 34 + 250))
    return o
save([arrow_frame(p) for p in big], "alertas_setas.gif")
print("ok")
