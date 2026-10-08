"""Tira de quadros recortada na explosão, uma linha por teste, com o alvo ao lado.

O recorte vem da posição que o teste automático registrou no log (AUTOTEST
screen x y px10m: o ponto a 5 m sobre o pé e quantos pixels valem 10 m ali) e
tem a mesma largura em metros que o recorte da arte: mesma escala, sem
escolher no olho.

    python BlastLab/Tools/strip.py saida.jpg captures/ue/teste_a captures/ue/teste_b \
        [--times 0.40,0.80,1.50,2.50] [--target docs/concept/explosions/target_ground_blast.webp]
"""
import argparse
import os

from PIL import Image, ImageDraw

TARGET = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'concept', 'explosions',
                      'target_ground_blast.webp')
# a explosão na arte (1024×572): o recorte em volta dela
TARGET_BOX = (290, 10, 630, 240)
# Largura desse recorte em metros: a bola de fogo de ~10 m (1.2 tanque) ocupa
# ~115 dos 340 px, ou seja ~30 m.
TARGET_WIDTH_M = 30.0
TILE = 380
ASPECT = (TARGET_BOX[2] - TARGET_BOX[0]) / (TARGET_BOX[3] - TARGET_BOX[1])


def explosion_box(folder):
    log = os.path.normpath(folder) + '.log'
    if not os.path.exists(log):
        return None
    with open(log, encoding='utf-8', errors='replace') as f:
        line = next((l for l in f if 'AUTOTEST screen' in l), None)
    if not line:
        return None
    x, y, px10 = (float(v) for v in line.split('AUTOTEST screen')[1].replace('px10m', '').split())
    half = px10 * TARGET_WIDTH_M / 10.0 / 2.0
    cy = y - half * 0.15                                   # a coluna sobe: um pouco mais de céu
    return int(x - half), int(cy - half / ASPECT), int(x + half), int(cy + half / ASPECT)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('folders', nargs='+')
    ap.add_argument('--times', default='0.40,0.80,1.50,2.50')
    ap.add_argument('--target', default=TARGET)
    a = ap.parse_args()
    times = a.times.split(',')
    h = int(TILE / ASPECT)
    sheet = Image.new('RGB', (TILE * (len(times) + 1), h * len(a.folders)), (16, 16, 16))
    draw = ImageDraw.Draw(sheet)
    target = Image.open(a.target).convert('RGB').crop(TARGET_BOX).resize((TILE, h), Image.LANCZOS)
    for r, folder in enumerate(a.folders):
        name, box = os.path.basename(os.path.normpath(folder)), explosion_box(folder)
        sheet.paste(target, (0, r * h))
        draw.text((6, r * h + 6), 'alvo', fill=(255, 220, 120))
        for i, t in enumerate(times):
            im = Image.open(os.path.join(folder, 'sim_%s.png' % t)).convert('RGB')
            if box:
                im = im.crop(box)
            sheet.paste(im.resize((TILE, h), Image.LANCZOS), ((i + 1) * TILE, r * h))
            draw.text(((i + 1) * TILE + 6, r * h + 6), '%s %s s' % (name, t), fill=(255, 220, 120))
    sheet.save(a.out, quality=90)
    print(a.out, sheet.size)


if __name__ == '__main__':
    main()
