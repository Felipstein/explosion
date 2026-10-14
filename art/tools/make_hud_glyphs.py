"""Os sinais pequenos dos cartões do topo de 8, lisos (desenhados 8× maiores e reduzidos, como o triângulo do aviso):

- spy_badge_ring.png e spy_badge_eye.png: o selo da espionagem no canto do emblema (o aro âmbar, que o tempo
  escurece, e o miolo escuro com o olho âmbar por cima);
- defeated_x.png: o X vermelho do derrotado, com o contorno escuro;
- radiation.png e radiation_red.png: o trevo radioativo das superarmas (amarelo; vermelho quando falta pouco),
  como na peça aprovada top_bar_8_players: duas pás em cima, uma embaixo.

Saída: docs/concept/hud/kit/skin/ (64×64, com transparência).
Uso: python art/tools/make_hud_glyphs.py
"""
import os
from PIL import Image, ImageDraw, ImageChops

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
N, SS = 64, 8
S = N * SS
AMBER = (255, 176, 46)
AMBER_DARK = (150, 88, 10)


def disc(d, cx, cy, r, fill):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=fill)


def spy_badge():
    """Em duas camadas: o aro (o tempo da espionagem escurece ele no sentido do relógio) e o miolo com o olho,
    que vai por cima e fica sempre aceso."""
    c = S / 2
    ring = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    disc(d, c, c, 31 * SS, (0, 0, 0, 230))          # a sombra
    disc(d, c, c, 29 * SS, AMBER_DARK + (255,))     # o aro, escuro por fora
    disc(d, c, c, 27 * SS, AMBER + (255,))          # o aro âmbar
    disc(d, c, c, 23 * SS, (0, 0, 0, 0))            # o furo (o miolo vem na outra camada)
    ring.resize((N, N), Image.LANCZOS).save(os.path.join(OUT, 'spy_badge_ring.png'))

    eye = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(eye)
    disc(d, c, c, 23.5 * SS, (22, 16, 10, 255))     # o miolo escuro
    # o olho: a amêndoa é a interseção de dois círculos grandes, deslocados pra cima e pra baixo
    R, off = 25 * SS, 17 * SS
    a = Image.new('L', (S, S), 0); ImageDraw.Draw(a).ellipse((c - R, c - off - R, c + R, c - off + R), fill=255)
    b = Image.new('L', (S, S), 0); ImageDraw.Draw(b).ellipse((c - R, c + off - R, c + R, c + off + R), fill=255)
    almond = ImageChops.multiply(a, b)
    eye.paste(Image.new('RGBA', (S, S), AMBER + (255,)), (0, 0), almond)
    disc(d, c, c, 6.5 * SS, (22, 16, 10, 255))      # a pupila
    disc(d, c - 2.2 * SS, c - 2.2 * SS, 2.0 * SS, AMBER + (255,))  # o brilho
    eye.resize((N, N), Image.LANCZOS).save(os.path.join(OUT, 'spy_badge_eye.png'))
    print('spy_badge_ring, spy_badge_eye')


def defeated_x():
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = 10 * SS
    for width, color in ((15 * SS, (20, 0, 0, 235)), (9 * SS, (236, 52, 44, 255))):
        d.line((m, m, S - m, S - m), fill=color, width=width)
        d.line((S - m, m, m, S - m), fill=color, width=width)
        # as pontas redondas
        for x, y in ((m, m), (S - m, S - m), (S - m, m), (m, S - m)):
            disc(d, x, y, width / 2, color)
    img.resize((N, N), Image.LANCZOS).save(os.path.join(OUT, 'defeated_x.png'))
    print('defeated_x')


def radiation(name, body, rim):
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    disc(d, c, c, 31 * SS, (0, 0, 0, 220))          # a sombra
    disc(d, c, c, 29.5 * SS, rim + (255,))          # o aro
    disc(d, c, c, 27.5 * SS, body + (255,))         # o disco
    R = 24 * SS
    ink = (16, 12, 8, 255)
    # as três pás de 60°: em cima à esquerda, em cima à direita e embaixo (ângulos do PIL: 0° = direita, horário)
    for mid in (210, 330, 90):
        d.pieslice((c - R, c - R, c + R, c + R), mid - 30, mid + 30, fill=ink)
    disc(d, c, c, 7.5 * SS, body + (255,))          # o vão entre o miolo e as pás
    disc(d, c, c, 4.5 * SS, ink)                    # o miolo
    img.resize((N, N), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))
    print(name)


spy_badge()
defeated_x()
radiation('radiation', (255, 204, 40), (120, 84, 8))
radiation('radiation_red', (246, 66, 54), (110, 14, 10))
