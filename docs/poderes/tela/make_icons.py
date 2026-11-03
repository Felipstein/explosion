# Os ícones provisórios dos poderes novos (03/11): o fundo de metal do kit do HUD (o repair.png sem a chave) + um pictograma claro.
# python docs/poderes/tela/make_icons.py docs/concept/hud/prototipo/a docs/poderes/tela/pw
# São provisórios: a arte final dos ícones vem depois, no gerador de imagem, como o resto do HUD.
import math, random, sys
from PIL import Image, ImageDraw, ImageFilter
SRC = sys.argv[1]; OUT = sys.argv[2]
import os; os.makedirs(OUT, exist_ok=True)
S = 4  # supersample

def blank():
    im = Image.open(SRC + '/repair.png').convert('RGB')
    px = im.load(); rnd = random.Random(3)
    for y in range(36, 221):
        t = (y - 36) / 185
        c = [int(18 + (12 - 18) * t), int(28 + (18 - 28) * t), int(34 + (22 - 34) * t)]
        for x in range(36, 221):
            n = rnd.randint(-2, 2)
            px[x, y] = (c[0] + n, c[1] + n, c[2] + n)
    return im

def finish(shapes, name):
    W = 256 * S
    mask = Image.new('L', (W, W), 0); d = ImageDraw.Draw(mask)
    shapes(d, lambda v: v * S)
    base = blank().resize((W, W), Image.LANCZOS)
    shadow = mask.filter(ImageFilter.GaussianBlur(6 * S)).point(lambda a: int(a * 0.85))
    base.paste((0, 0, 0), (3 * S, 5 * S), shadow)
    # o metal claro: um degradê de cima pra baixo, como os ícones do kit
    grad = Image.new('RGB', (W, W))
    gd = ImageDraw.Draw(grad)
    for y in range(W):
        t = y / W; v = int(238 - 70 * t)
        gd.line([(0, y), (W, y)], fill=(v, v + 2, v + 4))
    base.paste(grad, (0, 0), mask)
    edge = mask.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.GaussianBlur(1 * S)).point(lambda a: min(255, a * 2))
    base.paste((60, 66, 72), (0, 0), edge.point(lambda a: int(a * 0.5)))
    base.resize((256, 256), Image.LANCZOS).save(f'{OUT}/{name}.png')

def poly(d, s, pts, cx=128, cy=128, rot=0.0, sc=1.0):
    c, sn = math.cos(rot), math.sin(rot)
    d.polygon([s(cx + (x * c - y * sn) * sc) if False else (s(cx + (x * c - y * sn) * sc), s(cy + (x * sn + y * c) * sc)) for (x, y) in pts], fill=255)

def mirror(half):  # metade direita (x >= 0) de cima pra baixo -> contorno inteiro
    return half + [(-x, y) for (x, y) in reversed(half)]

def a10(d, s):
    body = mirror([(0, -78), (7, -66), (8, -10), (78, -4), (80, 10), (8, 14), (8, 34), (26, 34), (28, 30), (34, 30), (34, 54), (8, 54), (5, 76), (0, 78)])
    poly(d, s, body, cy=124)
    for sx in (-1, 1):  # as duas turbinas em cima da fuselagem, atrás da asa
        d.ellipse([s(128 + sx * 16 - 8), s(124 + 14), s(128 + sx * 16 + 8), s(124 + 40)], fill=255)

def ac130(d, s):
    body = mirror([(0, -84), (10, -74), (11, -26), (92, -20), (92, -8), (11, -4), (10, 52), (40, 58), (40, 68), (6, 70), (0, 80)])
    poly(d, s, body, cy=118)
    for x in (-62, -34, 34, 62):
        d.rounded_rectangle([s(128 + x - 6), s(118 - 38), s(128 + x + 6), s(118 - 6)], radius=s(4), fill=255)
    # a órbita: o arco tracejado em volta
    for a in range(200, 340, 22):
        a0, a1 = math.radians(a), math.radians(a + 13)
        d.arc([s(40), s(36), s(216), s(212)], start=a, end=a + 13, fill=255, width=s(7))

def precision(d, s):
    bomb = mirror([(0, -86), (11, -72), (13, -10), (22, -2), (22, 8), (13, 2), (12, 10), (0, 18)])
    poly(d, s, bomb, cy=96, rot=0.0)
    # a mira embaixo
    cx, cy, r = 128, 178, 30
    d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], outline=255, width=s(8))
    for (x0, y0, x1, y1) in ((cx - r - 16, cy, cx - 12, cy), (cx + 12, cy, cx + r + 16, cy), (cx, cy - r - 14, cx, cy - 12), (cx, cy + 12, cx, cy + r + 14)):
        d.line([s(x0), s(y0), s(x1), s(y1)], fill=255, width=s(8))

def moab(d, s):
    bomb = mirror([(0, -96), (24, -84), (30, -54), (30, 12), (22, 34), (0, 40)])
    poly(d, s, bomb, cy=100)
    for (x, y) in ((-34, -82), (34, -82)):  # as aletas de grade no topo
        d.rectangle([s(128 + x - 9), s(100 + y - 6), s(128 + x + 9), s(100 + y + 26)], fill=255)
    for i, r in enumerate((44, 66, 88)):  # a onda embaixo
        d.arc([s(128 - r), s(176 - r * 0.45), s(128 + r), s(176 + r * 0.45)], start=200, end=340, fill=255, width=s(8 - i))

def awacs(d, s):
    body = mirror([(0, -82), (9, -70), (10, -20), (80, 18), (80, 28), (10, 10), (9, 54), (34, 70), (34, 78), (5, 76), (0, 84)])
    poly(d, s, body, cy=132)
    d.ellipse([s(128 - 46), s(124 - 21), s(128 + 46), s(124 + 21)], fill=0)      # o vão escuro em volta do disco
    d.ellipse([s(128 - 40), s(124 - 15), s(128 + 40), s(124 + 15)], fill=255)
    for i, r in enumerate((48, 64)):  # as ondas do radar em cima
        d.arc([s(128 - r), s(70 - r * 0.6), s(128 + r), s(70 + r * 0.6)], start=215, end=325, fill=255, width=s(7))

def naval(d, s):
    hull = [(-92, 150), (96, 150), (84, 172), (-80, 172)]
    d.polygon([(s(128 + x), s(y)) for (x, y) in hull], fill=255)
    d.rectangle([s(108), s(118), s(146), s(150)], fill=255)          # a superestrutura
    d.rectangle([s(120), s(96), s(130), s(118)], fill=255)           # o mastro
    for (cx, dirn) in ((70, -1), (170, 1), (196, 1)):               # as torres e os canos apontando pro alto
        d.rounded_rectangle([s(cx - 14), s(136), s(cx + 14), s(150)], radius=s(5), fill=255)
        d.line([s(cx), s(140), s(cx + dirn * 30), s(112)], fill=255, width=s(6))
    for i, r in enumerate((40, 62)):                                # a salva subindo
        d.arc([s(150 - r), s(96 - r * 0.7), s(150 + r), s(96 + r * 0.7)], start=200, end=300, fill=255, width=s(6 - i))

for name, f in (('usa_naval', naval), ('usa_a10', a10), ('usa_ac130', ac130), ('usa_precision', precision), ('usa_moab', moab), ('usa_awacs', awacs)):
    finish(f, name)
print('ok')
