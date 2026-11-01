# Ícone do Atelier a partir do ícone do jogo (game_icon.webp): o mesmo tanque em duotone dourado
# (a cor de destaque do app) numa placa escura, com marcas de enquadramento nos cantos, de visor de
# câmera. Gera icon-1024.png (fonte pro `tauri icon`) e favicon-64.png (a aba/janela do app).
import os
from PIL import Image, ImageDraw, ImageFilter
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
S = 1024

def duotone(im):
    a = np.asarray(im.convert('RGBA')).astype(np.float32) / 255
    lum = a[..., 0] * .2126 + a[..., 1] * .7152 + a[..., 2] * .0722
    lum = np.clip((lum - .06) / .8, 0, 1) ** .78
    # escuro → bronze → o dourado do app → creme
    stops = [(0, (14, 12, 10)), (.28, (58, 42, 22)), (.55, (160, 116, 52)), (.8, (228, 180, 92)), (1, (250, 234, 200))]
    out = np.zeros(a.shape[:2] + (3,), np.float32)
    for (x0, c0), (x1, c1) in zip(stops, stops[1:]):
        m = (lum >= x0) & (lum <= x1)
        t = ((lum - x0) / (x1 - x0))[..., None]
        out[m] = (np.array(c0) * (1 - t) + np.array(c1) * t)[m]
    rgba = np.dstack([out, a[..., 3] * 255]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(rgba, 'RGBA')

def tile(size):
    # placa escura com um brilho quente no meio
    y, x = np.mgrid[0:size, 0:size] / size
    r = np.sqrt((x - .5) ** 2 + (y - .46) ** 2)
    base = np.array((19, 20, 21), np.float32); glow = np.array((46, 38, 26), np.float32)
    k = np.clip(1 - r / .62, 0, 1)[..., None] ** 1.6
    rgb = (base * (1 - k) + glow * k).astype(np.uint8)
    img = Image.fromarray(rgb, 'RGB').convert('RGBA')
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=int(size * .2), fill=255)
    img.putalpha(mask)
    return img

def corners(draw, size, inset, length, width, color):
    i, L, w = inset, length, width
    for (x, y, dx, dy) in [(i, i, 1, 1), (size - i, i, -1, 1), (i, size - i, 1, -1), (size - i, size - i, -1, -1)]:
        draw.rectangle(sorted([x, x + dx * L])[:1] + [min(y, y + dy * w)] + [max(x, x + dx * L)] + [max(y, y + dy * w)], fill=color)
        draw.rectangle([min(x, x + dx * w), min(y, y + dy * L), max(x, x + dx * w), max(y, y + dy * L)], fill=color)

def compose(fill, inset, length, width):
    src = Image.open(os.path.join(HERE, 'game_icon.webp')).convert('RGBA')
    src = src.crop(src.getchannel('A').getbbox())
    tank = duotone(src)
    img = tile(S)
    box = int(S * fill)
    tank.thumbnail((box, box), Image.LANCZOS)
    tx, ty = (S - tank.width) // 2, int(S * .53 - tank.height / 2)
    # sombra suave embaixo do tanque, pra ele descolar da placa
    shadow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    sh = Image.new('RGBA', tank.size, (0, 0, 0, 170)); sh.putalpha(tank.getchannel('A').point(lambda v: v * .65))
    shadow.alpha_composite(sh, (tx, ty + int(S * .02)))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(S * .018)))
    img.alpha_composite(tank, (tx, ty))
    corners(ImageDraw.Draw(img), S, inset=int(S * inset), length=int(S * length), width=int(S * width), color=(228, 180, 92, 255))
    return img

big = compose(fill=.74, inset=.075, length=.15, width=.036)
# 48 px pra baixo: o tanque ocupa mais e os cantos afinam, senão vira mancha na barra de tarefas
small = compose(fill=.9, inset=.05, length=.13, width=.06)
big.save(os.path.join(HERE, 'icon-1024.png'))
small.resize((64, 64), Image.LANCZOS).save(os.path.join(HERE, 'favicon-64.png'))
sizes = [256, 128, 64, 48, 32, 24, 16]
frames = [(big if s > 48 else small).resize((s, s), Image.LANCZOS) for s in sizes]
frames[0].save(os.path.join(HERE, 'icon.ico'), sizes=[(s, s) for s in sizes], append_images=frames[1:])
print('ok')
