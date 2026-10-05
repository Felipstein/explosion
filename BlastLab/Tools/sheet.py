"""Folha de quadros de uma sequência renderizada (captures/ue/<pasta>/).

  python BlastLab/Tools/sheet.py Seq_Explosion 0.2,0.5,1,2,3,4.5,6 [saida.jpg] [recorte x0,y0,x1,y1]
Tempos em segundos (30 qps). Recorte em frações da imagem.
"""
import glob
import os
import sys

from PIL import Image, ImageDraw

TOOLS = os.path.dirname(os.path.abspath(__file__))
CAPTURES = os.path.abspath(os.path.join(TOOLS, '..', '..', 'captures', 'ue'))
FPS = 30


def main():
    folder = sys.argv[1]
    times = [float(t) for t in sys.argv[2].split(',')]
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(CAPTURES, folder + '_sheet.jpg')
    rect = [float(v) for v in sys.argv[4].split(',')] if len(sys.argv) > 4 else [0, 0, 1, 1]
    ims = []
    for t in times:
        f = glob.glob(os.path.join(CAPTURES, folder, '*.%04d.jpeg' % int(round(t * FPS))))[0]
        im = Image.open(f).convert('RGB')
        w, h = im.size
        im = im.crop((int(rect[0] * w), int(rect[1] * h), int(rect[2] * w), int(rect[3] * h)))
        ims.append((t, im))
    width = 380
    height = round(width * ims[0][1].height / ims[0][1].width)
    sheet = Image.new('RGB', (width * len(ims), height), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for i, (t, im) in enumerate(ims):
        sheet.paste(im.resize((width, height), Image.LANCZOS), (i * width, 0))
        d.rectangle((i * width, 0, i * width + 70, 14), fill=(0, 0, 0))
        d.text((i * width + 3, 2), 't=%.1fs' % t, fill=(255, 255, 255))
    sheet.save(out, quality=90)
    print(out)


if __name__ == '__main__':
    main()
