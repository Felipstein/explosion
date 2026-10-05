"""Folha WebGL × Unreal nos mesmos instantes.

  python BlastLab/Tools/compare.py <saida.jpg> [pasta ...] [--webgl=prefixo]
WebGL: captures/<prefixo><t>.jpg (tempos com 2 casas, ex. webgl_exp_0.40.jpg),
capturadas no navegador com a mesma câmera da sequência.
Unreal: captures/ue/<pasta>/*.<quadro>.jpeg (30 qps), uma linha por pasta —
várias pastas = teste A/B (blast.ps1 render ... -Folder <nome>).
"""
import glob
import os
import sys

from PIL import Image, ImageDraw

TOOLS = os.path.dirname(os.path.abspath(__file__))
CAPTURES = os.path.abspath(os.path.join(TOOLS, '..', '..', 'captures'))
W, H = 300, 260


def crop(im):
    w, h = im.size
    ww = h * W / H
    x0 = w / 2 - ww / 2
    return im.crop((int(x0), 0, int(x0 + ww), h)).resize((W, H), Image.LANCZOS)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    options = dict(a[2:].split('=', 1) for a in sys.argv[1:] if a.startswith('--'))
    out = args[0]
    folders = args[1:] or ['Seq_Explosion']
    prefix = options.get('webgl', 'webgl_exp_')
    refs = sorted(glob.glob(os.path.join(CAPTURES, prefix + '*.jpg')),
                  key=lambda a: float(os.path.basename(a)[len(prefix):-4]))
    times = [float(os.path.basename(a)[len(prefix):-4]) for a in refs]
    rows = [('WebGL', None)] + [(f, f) for f in folders]
    sheet = Image.new('RGB', (W * len(times), (H + 4) * len(rows) - 4), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for i, (t, ref) in enumerate(zip(times, refs)):
        for j, (name, folder) in enumerate(rows):
            if folder is None:
                im = Image.open(ref)
            else:
                b = glob.glob(os.path.join(CAPTURES, 'ue', folder, '*.%04d.jpeg' % round(t * 30)))
                if not b:
                    continue
                im = Image.open(b[0])
            y = j * (H + 4)
            sheet.paste(crop(im.convert('RGB')), (i * W, y))
            label = '%s %.2fs' % (name, t)
            d.rectangle((i * W, y, i * W + 6 * len(label) + 6, y + 13), fill=(0, 0, 0))
            d.text((i * W + 3, y + 1), label, fill=(255, 255, 255))
    sheet.save(out, quality=90)
    print(out)


if __name__ == '__main__':
    main()
