# Os sons das telas prontos pro jogo (04/11): lê os efeitos gerados no Atelier (ElevenLabs, 1 variação cada, os prompts no
# atelier.json de cada pasta), acerta o nível, as rampas e monta a subida da abertura. Saída em Resources/Audio (WAV 48 kHz, 16
# bits), que o import_menu_audio.py leva pra /Game/Audio. O gerador entrega tudo no volume cheio e começando de repente; os níveis
# são relativos ao clique do HUD (−14 dBFS, docs/elenco/SONS_UNIDADES.md): o hover bem abaixo dele, os golpes acima.
#   python Game/Plugins/RTSShell/Tools/make_menu_sounds.py
import os, subprocess, wave
import numpy as np
import imageio_ffmpeg

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'captures', 'audio')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Resources', 'Audio')
SR = 48000


def load(folder):
    raw = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-v', 'quiet', '-i', os.path.join(SRC, folder, 'raiz_r1_sfx.mp3'),
                          '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.float32).reshape(-1, 2).copy()


def ramp(x, attack_ms, release_s):
    """A entrada sem estalo (alguns ms) e a saída até o silêncio (a cauda descendo em cosseno)."""
    n = len(x)
    a = max(1, int(SR * attack_ms / 1000))
    x[:a] *= np.linspace(0, 1, a)[:, None]
    r = min(n, int(SR * release_s))
    x[n - r:] *= (0.5 + 0.5 * np.cos(np.linspace(0, np.pi, r)))[:, None]
    return x


def level(x, peak_db):
    return x * (10 ** (peak_db / 20) / (np.abs(x).max() + 1e-9))


def save(name, x):
    os.makedirs(OUT, exist_ok=True)
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2')
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    print(f'{name}: {len(x) / SR:.2f} s, pico {20 * np.log10(np.abs(x).max() + 1e-9):.1f} dBFS')


def lowpass(x, fc):
    """Passa-baixa de 2ª ordem (o módulo do Butterworth), sem defasar: tira o sopro de cima (a regra do 'grilo' do SONS_UNIDADES)."""
    f = np.fft.rfftfreq(len(x), 1 / SR)
    h = 1 / np.sqrt(1 + (f / fc) ** 4)
    return np.stack([np.fft.irfft(np.fft.rfft(x[:, c]) * h, len(x)) for c in range(x.shape[1])], axis=1).astype(np.float32)


# o hover APROVADO (04/11, "agora sim"): o tique digital do console no jeito do ZH (o GUIBlip dele, medido: 35 ms, um bip tão curto
# que vira "tsk", baixo). A ElevenLabs entrega ~90 ms de bip: fica só nos 50 ms do começo, com a entrada de 2 ms e a saída de 25 ms.
# O nível: 6 dB acima da música na faixa dele (hover_preview.py), −17 dBFS. Os reprovados antes dele (a tecla, o sopro, o Lá
# afinado, os três desenhados, o deslizar, o sonar) estão em docs/APROVADOS.md
hover = load('69_menu_o_som_do_hover_6_o_tique_digital')[: int(SR * 0.05)]
save('UI_Menu_Hover', level(ramp(hover, 2, 0.025), -17))

# a entrada no skirmish e no multiplayer: o golpe grave com a cauda escura (4 s), enquanto a música abaixa
enter = load('50_entrada_no_skirmish_e_no_multiplayer')
save('UI_Menu_Enter', level(ramp(enter, 2, 1.2), -6))

# a abertura: o estalo do relé (o começo do "sistema ligando"), a subida (a cauda da revelação ao contrário, a técnica do cinema:
# o som que cresce e desemboca no golpe) e o golpe da revelação, que é quando a música entra
reveal = load('52_abertura_a_revelacao_do_menu')
save('UI_Boot_Reveal', level(ramp(reveal, 2, 0.6), -6))
power = load('51_abertura_o_sistema_ligando')
save('UI_Boot_Relay', level(ramp(power[: int(SR * 0.9)], 2, 0.5), -12))
# só a cauda ao contrário (a revelação fica cheia no 1º segundo e depois cai: o reverso do corpo cheio seria 1 s de som parado)
riser = reveal[int(SR * 0.9):][::-1].copy()
save('UI_Boot_Riser', level(ramp(riser, 300, 0.01), -12))
