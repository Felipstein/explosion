# A abertura em vídeo pra conferência (04/11): os quadros do jogo (native_shot.ps1 -Boot, 30 qps, renderizados com o passo fixo)
# com o som montado nos instantes que o jogo escreveu no log: o relé, a subida (termina no golpe), o golpe e a música entrando nele
# no volume do menu (0,55), como o jogo toca.
#   python boot_video.py <pasta dos quadros> <log do jogo> <saída.mp4>
import os, re, subprocess, sys, wave
import numpy as np
import imageio_ffmpeg

FF = imageio_ffmpeg.get_ffmpeg_exe()
SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
AUDIO = os.path.join(HERE, '..', 'Resources', 'Audio')
MUSIC = os.path.join(HERE, '..', 'Resources', 'UI', 'snd', 'Main_Menu_1.mp3')
RISER_S = 1.58   # o mesmo do SRTSMenu.cpp

frames, log, out = sys.argv[1:4]
txt = open(log, encoding='utf-8', errors='replace').read()
n = int(re.search(r'quadros da abertura: (\d+)', txt).group(1))
relay, reveal = map(float, re.search(r'o relé em ([\d.]+) s, o golpe em ([\d.]+) s', txt).groups())


def wav(name):
    with wave.open(os.path.join(AUDIO, name + '.wav')) as w:
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32767.0


mix = np.zeros((int(SR * n / 30) + SR, 2))


def put(x, at, gain=1.0):
    i = int(SR * at)
    k = min(len(x), len(mix) - i)
    if k > 0:
        mix[i:i + k] += x[:k] * gain


put(wav('UI_Boot_Relay'), relay)
put(wav('UI_Boot_Riser'), reveal - RISER_S)
put(wav('UI_Boot_Reveal'), reveal)
raw = subprocess.run([FF, '-v', 'quiet', '-i', MUSIC, '-t', '20', '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
put(np.frombuffer(raw, np.float32).reshape(-1, 2), reveal, 0.55)
mix = mix[: int(SR * n / 30)]
tmp = out + '.wav'
with wave.open(tmp, 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((np.clip(mix, -1, 1) * 32767).astype('<i2').tobytes())
subprocess.run([FF, '-v', 'error', '-y', '-framerate', '30', '-i', os.path.join(frames, 'boot_%03d.png'), '-i', tmp, '-c:v', 'libx264', '-pix_fmt',
                'yuv420p', '-crf', '18', '-c:a', 'aac', '-b:a', '192k', '-shortest', out], check=True)
os.remove(tmp)
print(f'vídeo: {out} ({n} quadros, o relé em {relay:.2f} s, o golpe em {reveal:.2f} s)')
