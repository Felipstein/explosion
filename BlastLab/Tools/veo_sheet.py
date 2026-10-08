"""Folha quadro a quadro: o vídeo do Veo contra as capturas do BlastLab.

Cada linha é uma pasta de captura (teste automático com -AutoShots); a de cima
é o vídeo-alvo no MESMO instante depois do clarão. Serve pra ver rodada a
rodada se a explosão chega no vídeo.

    python BlastLab/Tools/veo_sheet.py rts saida.jpg captures/ue/cmp_rts_d captures/ue/cmp_rts_e
    python BlastLab/Tools/veo_sheet.py low saida.jpg captures/ue/cmp_low_d captures/ue/cmp_low_e

As capturas precisam dos parâmetros combinados (docs/CONCEITO_EXPLOSOES.md §4d):
câmera de RTS -Cam 80,-0.62,0.96,2,0.58 e câmera baixa -Cam 70,-0.62,0.08,12,0.58,
-Hour 16, -benchmark -fps=30 (instantes exatos).
"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
VIDEO_DIR = os.path.join(REPO, 'captures', 'concept', '33_video')
CACHE = os.path.join(REPO, 'captures', 'concept', '36_blastlab_vs_target', 'veo_frames')
TIMES = ['0', '0.07', '0.13', '0.20', '0.30', '0.40', '0.50', '0.60', '0.80', '1.00', '1.30', '1.50', '2.00', '3.00', '4.00']
# vídeo, instante do clarão (s), recorte no vídeo 1280×720, recorte na captura 1600×900 (sem o painel)
CAMS = {
    'rts': ('gameplay_v1.mp4', 4.60, (180, 66, 940, 494), (350, 127, 1250, 633), 'CAMERA DE RTS'),
    'low': ('gameplay_v3.mp4', 5.12, (260, 0, 1280, 574), (0, 0, 1390, 782), 'CAMERA BAIXA'),
    # cena herói (Range_Desert_Units, tanques de verdade): -Cam 38,-0.62,0.06,6,0.58 contra a câmera baixa do vídeo
    'hero': ('gameplay_v3.mp4', 5.12, (260, 0, 1280, 574), (0, 0, 1390, 782), 'CENA HEROI'),
}
TW, TH, LAB = 384, 216, 26


def ffmpeg():
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def veo_frame(cam, t):
    video, flash, _, _, _ = CAMS[cam]
    out = os.path.join(CACHE, f'{cam}_{t}.png')
    if not os.path.exists(out):
        os.makedirs(CACHE, exist_ok=True)
        subprocess.run([ffmpeg(), '-v', 'error', '-y', '-ss', f'{flash + float(t):.3f}',
                        '-i', os.path.join(VIDEO_DIR, video), '-frames:v', '1', out], check=True)
    return Image.open(out).convert('RGB')


def main():
    cam, out, folders = sys.argv[1], sys.argv[2], sys.argv[3:]
    _, _, vbox, obox, title = CAMS[cam]
    vcam = 'low' if cam == 'hero' else cam
    rows = 1 + len(folders)
    band = rows * (TH + 4) + LAB + 8
    font = ImageFont.truetype('arialbd.ttf', 20)
    small = ImageFont.truetype('arial.ttf', 18)
    sheet = Image.new('RGB', (5 * TW + 6 * 4 + 140, 3 * band + 40), (16, 16, 16))
    d = ImageDraw.Draw(sheet)
    names = ['VIDEO'] + [os.path.basename(os.path.normpath(f)) for f in folders]
    d.text((8, 8), f'{title}: video do Veo x ' + ' x '.join(names[1:]) + ', mesmo instante depois do clarao',
           fill=(255, 210, 90), font=font)
    for i, t in enumerate(TIMES):
        r, c = divmod(i, 5)
        x, y = 140 + c * (TW + 4), 40 + r * band
        d.text((x + 4, y + 2), f't = {float(t):.2f} s', fill=(230, 230, 230), font=small)
        ims = [veo_frame(vcam, t).crop(vbox)]
        stamp = t if '.' in t else t + '.00'
        for f in folders:
            path = os.path.join(f, f'sim_{stamp}.png')
            ims.append(Image.open(path).convert('RGB').crop(obox) if os.path.exists(path)
                       else Image.new('RGB', (obox[2] - obox[0], obox[3] - obox[1]), (40, 40, 40)))
        for j, im in enumerate(ims):
            sheet.paste(im.resize((TW, TH), Image.LANCZOS), (x, y + LAB + j * (TH + 4)))
        if c == 0:
            for j, n in enumerate(names):
                col = (140, 230, 140) if j == 0 else (255, 170, 90) if j == len(names) - 1 else (170, 170, 170)
                d.text((6, y + LAB + j * (TH + 4) + TH // 2 - 10), n[:12], fill=col, font=font)
    sheet.save(out, quality=88)
    print(out, sheet.size)


main()
