"""O efeito no jogo contra a arte (14/10, o Felipe: "compare com os game shots, use sobreposição de imagem").

  python BlastLab/Tools/fx_vs_art.py <video.mp4> <arte.webp> <saida.jpg> [--offset 0.10] [--max 4] [--crop x0,y0,x1,y1]

Acha no vídeo do jogo os picos de brilho (o clarão de cada acerto ou disparo), pega o quadro `offset` segundos depois
de cada pico (o leque de faíscas já aberto) e monta, por linha: o quadro do jogo | a arte | os dois sobrepostos a 50%.
O recorte (--crop, em pixels do vídeo) enquadra o tanque como na arte; sem ele, o quadro inteiro.
"""
import subprocess
import sys

import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFont


def frames(path, w=320, h=180):
    ff = imageio_ffmpeg.get_ffmpeg_exe()
    raw = subprocess.run([ff, '-loglevel', 'error', '-i', path, '-vf', f'scale={w}:{h}', '-f', 'rawvideo', '-pix_fmt', 'gray', '-'],
                         capture_output=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(-1, h, w).astype(float)


def grab(path, t):
    ff = imageio_ffmpeg.get_ffmpeg_exe()
    raw = subprocess.run([ff, '-loglevel', 'error', '-ss', f'{t:.3f}', '-i', path, '-frames:v', '1', '-f', 'image2pipe', '-vcodec', 'png', '-'],
                         capture_output=True).stdout
    from io import BytesIO
    return Image.open(BytesIO(raw)).convert('RGB')


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    video, art_path, out = args[0], args[1], args[2]
    opt = lambda k, d: sys.argv[sys.argv.index(k) + 1] if k in sys.argv else d
    offset = float(opt('--offset', '0.10'))
    nmax = int(opt('--max', '4'))
    crop = opt('--crop', '')
    fps = 30.0
    F = frames(video)
    # o clarão: o percentil 99,9 do brilho de cada quadro sobe de repente
    top = np.percentile(F.reshape(len(F), -1), 99.9, axis=1)
    jump = np.diff(top, prepend=top[0])
    peaks = []
    for k in np.argsort(-jump):
        if jump[k] < 8:
            break
        if all(abs(k - p) > 20 for p in peaks):
            peaks.append(int(k))
        if len(peaks) >= nmax:
            break
    peaks.sort()
    art = Image.open(art_path).convert('RGB')
    W = 800
    rows = []
    font = ImageFont.truetype('arial.ttf', 22)
    for k in peaks:
        t = k / fps + offset
        g = grab(video, t)
        if crop:
            x0, y0, x1, y1 = map(int, crop.split(','))
            g = g.crop((x0, y0, x1, y1))
        H = int(W * g.height / g.width)
        g = g.resize((W, H))
        a = art.resize((W, H))
        mix = Image.blend(g, a, 0.5)
        row = Image.new('RGB', (W * 3, H + 30), (20, 20, 20))
        for i, im in enumerate((g, a, mix)):
            row.paste(im, (i * W, 30))
        d = ImageDraw.Draw(row)
        for i, lab in enumerate((f'jogo, {t:.2f} s', 'a arte', 'sobrepostos (50%)')):
            d.text((i * W + 8, 4), lab, fill=(255, 230, 120), font=font)
        rows.append(row)
    if not rows:
        print('nenhum clarão achado')
        return
    sheet = Image.new('RGB', (rows[0].width, sum(r.height for r in rows)))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.height
    sheet.save(out, quality=88)
    print(out, 'picos (s):', [round(k / fps, 2) for k in peaks])


if __name__ == '__main__':
    main()
