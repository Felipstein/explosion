"""O triângulo do aviso do HUD (o do limpo aprovado: vermelho forte, cantos arredondados, aro escuro fino e o
"!" preto e grosso), liso: desenhado 8× maior e reduzido, pra não serrilhar como o triângulo feito no Canvas.

Saída: docs/concept/hud/kit/skin/alert_icon_red.png e alert_icon_amber.png (128×112, com transparência).
Uso: python art/tools/make_alert_icons.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
FONT = os.path.join(ROOT, 'HUDLab/Fonts/BarlowSemiCondensed-Bold.ttf')
W, H, SS = 128, 112, 8

# a cor do corpo (em cima, embaixo) e do aro, por aviso
COLORS = {
    'red': ((248, 78, 70), (206, 30, 28), (70, 6, 6)),
    'amber': ((255, 196, 72), (226, 140, 18), (78, 42, 4)),
}


def rounded_triangle(size, pts, r):
    """A máscara do triângulo com os cantos arredondados: o triângulo encolhido de r e engordado de r."""
    m = Image.new('L', size, 0)
    d = ImageDraw.Draw(m)
    cx = sum(p[0] for p in pts) / 3.0
    cy = sum(p[1] for p in pts) / 3.0
    # encolhe na direção do centro o bastante pra o arredondado de raio r caber dentro do triângulo original
    k = 2.0 * r
    inner = []
    for x, y in pts:
        dx, dy = cx - x, cy - y
        n = (dx * dx + dy * dy) ** 0.5
        inner.append((x + dx / n * k, y + dy / n * k))
    d.polygon(inner, fill=255)
    d.line(inner + [inner[0]], fill=255, width=int(2 * r), joint='curve')
    for x, y in inner:
        d.ellipse((x - r, y - r, x + r, y + r), fill=255)
    return m


def make(name, top, bottom, rim):
    S = (W * SS, H * SS)
    tri = [(W * SS / 2, 4 * SS), (W * SS - 4 * SS, H * SS - 6 * SS), (4 * SS, H * SS - 6 * SS)]
    outer = rounded_triangle(S, tri, 12 * SS)
    # o corpo: o mesmo triângulo, 5 px pra dentro (o aro escuro fica em volta)
    cx = sum(p[0] for p in tri) / 3.0
    cy = sum(p[1] for p in tri) / 3.0
    body_pts = [(x + (cx - x) * 0.085, y + (cy - y) * 0.085) for x, y in tri]
    body = rounded_triangle(S, body_pts, 10 * SS)

    img = Image.new('RGBA', S, rim + (0,))
    img.putalpha(outer)
    grad = Image.new('RGBA', S)
    gd = ImageDraw.Draw(grad)
    for y in range(S[1]):
        t = y / (S[1] - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        gd.line((0, y, S[0], y), fill=c + (255,))
    img.paste(grad, (0, 0), body)

    # o "!" preto, grosso, um pouco abaixo do meio (o centro do triângulo é mais baixo que o da caixa)
    font = ImageFont.truetype(FONT, int(68 * SS))
    d = ImageDraw.Draw(img)
    l, t_, r_, b = d.textbbox((0, 0), '!', font=font)
    tx = W * SS / 2 - (l + r_) / 2
    ty = H * SS * 0.63 - (t_ + b) / 2
    d.text((tx, ty), '!', font=font, fill=(24, 6, 6, 255))

    img.resize((W, H), Image.LANCZOS).save(os.path.join(OUT, 'alert_icon_%s.png' % name))
    print('alert_icon_%s' % name)


for k, (top, bottom, rim) in COLORS.items():
    make(k, top, bottom, rim)
