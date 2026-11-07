# (06/11/2026, a trilha da partida, docs/TRILHA_DA_PARTIDA.md) Prepara as faixas escolhidas pro jogo e escreve Game/Config/RTS/Music.json.
# Pra cada faixa (a lista SEL, as decisões da §3 do documento):
#  1. o começo: tira o silêncio digital da frente (fica 20 ms), e 5 ms de entrada pra não estalar;
#  2. o fim: tira o silêncio do fim (fica 1 s); se depois de um silêncio de 2 s ou mais a faixa volta com um estouro (a russia_cold_line
#     voltava a −14 LUFS depois de 6 s calada), corta no silêncio; se a faixa acaba cortada a todo volume (as 4 da USA de 05/10), desce em
#     2 compassos até a barra do compasso (a curva de potência igual, cos²) e corta ali;
#  3. o volume percebido igual (EBU R128): batalha, ambas e tema em −16 LUFS, calma em −17,5 (o "volume" de cada faixa do ZH), vitória
#     −16, derrota −17; o pico real até −1 dBTP (um limitador com 2 ms de antecipação só onde passa);
#  4. a grade das batidas: o andamento e o 1º tempo forte (das faixas planejadas, o andamento do plano; das antigas, o medido), a entrada
#     (o trecho forte, pra começar uma batalha no meio da faixa) e a subida (4 compassos antes dela);
#  5. o WAV 44,1 kHz 16 bits (com dither) em captures/musica/pack/<facção>/MU_<id>.wav, que o import_music.py leva pra Unreal.
#   python Game/Tools/music/music_pack.py
import json
import os
import subprocess
import sys
import wave

import numpy as np
from scipy.signal import resample_poly

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, HERE)
import measure  # noqa: E402

SRC = os.path.join(ROOT, 'captures', 'musica')
OUT = os.path.join(SRC, 'pack')
PLANS = os.path.join(ROOT, 'docs', 'trilha', 'planos')
JSON_OUT = os.path.join(ROOT, 'Game', 'Config', 'RTS', 'Music.json')
SR = 44100
TARGET = {'battle': -16.0, 'both': -16.0, 'theme': -16.0, 'calm': -17.5, 'victory': -16.0, 'defeat': -17.0}
TP_LIMIT = -1.0
# (06/11) a calma toca mais baixa que o arquivo (o gain_db do Music.json, sem refazer o áudio): −17,5 − 1,5 = −19 LUFS, 3 dB abaixo da batalha.
# O tema que fica na calma (o da China) está a −16 no arquivo: −3. As 'both' (−16, tocam nas duas): −1,5, no meio
GAIN_BY_ROLE = {'calm': -1.5, 'theme': -3.0, 'both': -1.5}

# (facção, arquivo em captures/musica, id, papel, de onde veio). Os originais: o assgets dele (usa/china/gla, baixados em 06/11) e a
# ElevenLabs Music v2.5 (gerado/, os planos em docs/trilha/planos). Fora (TRILHA_DA_PARTIDA.md §3): Red_Steel_Foundations e
# Siege_of_the_Jade_Citadel (China sem instrumento chinês, fit 4), Colossus_of_Iron (metal ocidental) e Improvised_Steel (orquestra
# genérica) da GLA, e as v1 geradas que o ouvido reprovou
SEL = [
    # (06/11, ele: "muito louca, alta, animada, pra ser simplesmente uma partida em tempo normal [...] melhor trocar ela de momento") os temas
    # fortes saem da calma e da abertura e vão pra batalha: The Last Advance, Iron Directive, Calculated Perimeter e a Jade Command (a 'both')
    ('usa', 'usa/The_Last_Advance.mp3', 'usa_last_advance', 'battle'),
    ('usa', 'gerado/usa/usa_overwatch.mp3', 'usa_overwatch', 'calm'),
    # (06/11, ele depois de ouvir no jogo: "muito repetidas, péssimas, me enjoa"; os loops de 120 s de 05/10 saíram, 66 a 95% de cópia exata
    # pelo repetition.py) as novas da 3ª volta, pelo tema dele de referência
    ('usa', 'gerado/usa/usa_dawn_patrol.mp3', 'usa_dawn_patrol', 'calm'),
    ('usa', 'gerado/usa/usa_vanguard_ref.mp3', 'usa_vanguard', 'battle'),
    ('usa', 'gerado/usa/usa_steel_rain.mp3', 'usa_steel_rain', 'battle'),
    ('usa', 'usa/The_Final_Breach.mp3', 'usa_final_breach', 'battle'),
    ('usa', 'usa/Iron_Wing_Horizon.mp3', 'usa_iron_wing_horizon', 'battle'),
    ('usa', 'usa/Strike_Coordinates.mp3', 'usa_strike_coordinates', 'battle'),
    ('usa', 'gerado/usa/usa_victory.mp3', 'usa_victory', 'victory'),
    ('usa', 'gerado/usa/usa_defeat_v2.mp3', 'usa_defeat', 'defeat'),
    ('china', 'china/Jade_Garrison_March.mp3', 'china_jade_garrison_march', 'theme'),
    ('china', 'gerado/china/china_jade_dawn_v2.mp3', 'china_jade_dawn', 'calm'),
    ('china', 'gerado/china/china_red_lantern_v2.mp3', 'china_red_lantern', 'calm'),
    ('china', 'china/Beneath_the_Jade_Command.mp3', 'china_jade_command', 'battle'),
    ('china', 'china/Beneath_the_Iron_Banner.mp3', 'china_iron_banner', 'battle'),
    ('china', 'china/Banner_on_the_Ridge.mp3', 'china_banner_ridge', 'battle'),
    # (06/11, a 4ª volta) no lugar dos dois loops de 05/10 (Iron Assembly 72% e Red Dragon 84% de cópia): pouco instrumento chinês no
    # ouvido (cara 6), mas sem repetição (41 e 46%)
    ('china', 'gerado/china/china_dragon_ascent.mp3', 'china_dragon_ascent', 'battle'),
    ('china', 'gerado/china/china_iron_lotus.mp3', 'china_iron_lotus', 'battle'),
    ('china', 'gerado/china/china_victory_v2.mp3', 'china_victory', 'victory'),
    ('china', 'gerado/china/china_defeat.mp3', 'china_defeat', 'defeat'),
    ('gla', 'gla/Calculated_Perimeter.mp3', 'gla_calculated_perimeter', 'battle'),
    ('gla', 'gla/Tunnels_of_the_Vanguard_2026-10-05T161254.wav', 'gla_tunnels_vanguard', 'calm'),
    ('gla', 'gerado/gla/gla_black_market.mp3', 'gla_black_market', 'calm'),   # (4ª volta) no lugar da Scrap and Shadow (64%); pedida batalha, saiu calma
    ('gla', 'gla/Steel_Under_the_Dunes.mp3', 'gla_steel_dunes', 'both'),   # (4ª volta) nas duas rodas: a batalha da GLA ficou com menos faixas
    ('gla', 'gla/Shadow_of_the_Hijaz.mp3', 'gla_shadow_hijaz', 'both'),
    ('gla', 'gla/Sun_Scorched_Trenches.mp3', 'gla_sun_trenches', 'battle'),
    ('gla', 'gerado/gla/gla_victory.mp3', 'gla_victory', 'victory'),
    ('gla', 'gerado/gla/gla_defeat.mp3', 'gla_defeat', 'defeat'),
    ('russia', 'gerado/russia/russia_iron_directive.mp3', 'russia_iron_directive', 'battle'),
    ('russia', 'gerado/russia/russia_cold_line.mp3', 'russia_cold_line', 'calm'),
    ('russia', 'gerado/russia/russia_white_silence.mp3', 'russia_white_silence', 'calm'),
    ('russia', 'gerado/russia/russia_red_tide.mp3', 'russia_red_tide', 'battle'),
    ('russia', 'gerado/russia/russia_steel_avalanche.mp3', 'russia_steel_avalanche', 'battle'),
    ('russia', 'gerado/russia/russia_victory.mp3', 'russia_victory', 'victory'),
    ('russia', 'gerado/russia/russia_defeat_v2.mp3', 'russia_defeat', 'defeat'),
]


def decode(path):
    raw = subprocess.run([measure.FF, '-v', 'quiet', '-i', path, '-ac', '2', '-ar', str(SR), '-f', 'f32le', '-'], capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)


def st_curve(x):
    """O volume de 1 em 1 s (dBFS RMS em janela de 1 s, perto do short-term): pra achar silêncio e estouro (o LUFS integrado vem do ffmpeg)."""
    n = len(x) // SR
    m = x[:n * SR].mean(axis=1).reshape(n, SR)
    return 20 * np.log10(np.sqrt((m ** 2).mean(axis=1)) + 1e-9)


def ebur_I(x):
    tmp = os.path.join(OUT, '_tmp.wav')
    write_wav(tmp, x, dither=False)
    I = measure.ebur(tmp)['I']
    os.remove(tmp)
    return I


def true_peak_db(x):
    up = resample_poly(x, 4, 1, axis=0)
    return 20 * np.log10(np.abs(up).max() + 1e-12)


def limit(x, ceiling_db):
    """O limitador de antecipação: o ganho necessário por bloco de 1 ms (pelo pico com 4× de sobreamostragem), o mínimo nos 2 ms à frente
    (a antecipação) e a volta em 80 ms; só age onde o pico passa do teto."""
    c = 10 ** (ceiling_db / 20)
    up = np.abs(resample_poly(x, 4, 1, axis=0)).max(axis=1)
    blk = 44  # 1 ms
    nb = len(x) // blk + 1
    pk = np.zeros(nb)
    upb = 4 * blk
    for i in range(nb):
        seg = up[i * upb:(i + 1) * upb]
        pk[i] = seg.max() if len(seg) else 0
    need = np.minimum(1.0, c / np.maximum(pk, 1e-9))
    la = 2
    g = need.copy()
    for k in range(1, la + 1):
        g[:-k] = np.minimum(g[:-k], need[k:])
    rel = np.exp(-1.0 / 80.0)
    out = np.empty_like(g)
    cur = 1.0
    for i, v in enumerate(g):
        cur = v if v < cur else v + (cur - v) * rel
        out[i] = cur
    gs = np.repeat(out, blk)[:len(x)]
    return x * gs[:, None]


def write_wav(path, x, dither=True):
    y = x * 32767.0
    if dither:
        y = y + (np.random.default_rng(1).random(y.shape) - np.random.default_rng(2).random(y.shape))   # TPDF de 1 LSB
    y = np.clip(np.round(y), -32768, 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(y.tobytes())


def process(fac, rel, tid, role):
    src = os.path.join(SRC, rel)
    x = decode(src)
    log = []
    # 1. o começo
    a = np.abs(x).max(axis=1)
    on = np.where(a > 10 ** (-60 / 20))[0]
    lead = max(0, int(on[0]) - int(0.02 * SR)) if len(on) else 0
    x = x[lead:]
    if lead:
        log.append(f'começo: tirei {lead / SR:.2f} s de silêncio')
    # 2. o fim
    c = st_curve(x)
    med = float(np.median(c[c > -60])) if (c > -60).any() else -20
    tail0 = len(x) / SR
    # um silêncio de 2 s ou mais nos últimos 20 s e depois um estouro (acima de −40): corta no começo do silêncio
    gap_at = None
    for i in range(max(0, len(c) - 20), len(c) - 2):
        if c[i] < -50 and c[i + 1] < -50 and (c[i + 2:] > -40).any():
            gap_at = i
            break
    if gap_at is not None:
        x = x[:gap_at * SR]
        log.append(f'fim: silêncio em {gap_at} s e um estouro depois: cortei no silêncio (era {tail0:.1f} s)')
    a = np.abs(x).max(axis=1)
    on = np.where(a > 10 ** (-60 / 20))[0]
    if len(on) and len(x) - on[-1] > SR:
        cut = int(on[-1]) + SR
        log.append(f'fim: tirei {(len(x) - cut) / SR:.1f} s de silêncio')
        x = x[:cut]
    # a grade (do plano, ou medida)
    plan = None
    pp = os.path.join(PLANS, os.path.splitext(os.path.basename(rel))[0] + '.json')
    if rel.startswith('gerado/') and os.path.exists(pp):
        plan = json.load(open(pp, encoding='utf-8'))
    mono = x.mean(axis=1).astype(np.float32)
    m22 = resample_poly(mono, 1, 2).astype(np.float32)
    mag = measure.stft_mag(m22)
    tb = measure.tempo_and_beats(mag)
    bpm = float(plan['bpm']) if plan else float(tb['bpm'])
    beat = 60.0 / bpm
    bar = 4 * beat
    # o 1º tempo forte: das faixas planejadas, o começo (o plano começa no compasso 1) acertado pela fase medida se estiver perto
    down0 = tb['downbeat0'] % bar if not plan else 0.0
    if plan:
        ph = tb['beat0'] % beat
        if ph < 0.25 * beat:
            down0 = ph
    conf = float(tb['conf'])
    # o fim cortado a todo volume: desce 2 compassos até a barra e corta ali
    c = st_curve(x)
    n50 = int(0.05 * SR)
    last50 = 20 * np.log10(np.sqrt((x[-n50:] ** 2).mean()) + 1e-12)
    tail = float(np.mean(c[-3:])) if len(c) >= 3 else -60
    if tail > med - 4 and last50 > -40:
        end_s = len(x) / SR
        k = np.floor((end_s - down0) / bar)
        fade_end = down0 + k * bar
        if end_s - fade_end > 0.5 * bar or fade_end < 4 * bar:
            fade_end = end_s
        fade_len = min(2 * bar, 6.0)
        fs, fe = int((fade_end - fade_len) * SR), int(fade_end * SR)
        t = np.linspace(0, 1, fe - fs)
        x = x[:fe].copy()
        x[fs:fe] *= (np.cos(t * np.pi / 2) ** 2)[:, None]
        x = np.concatenate([x, np.zeros((int(0.8 * SR), 2))])
        log.append(f'fim cortado a todo volume ({tail:.0f} dB com a mediana em {med:.0f}): desci em {fade_len:.1f} s até {fade_end:.1f} s')
    # anti-estalo
    fi = int(0.005 * SR)
    x[:fi] *= np.linspace(0, 1, fi)[:, None]
    fo = int(0.02 * SR)
    x[-fo:] *= np.linspace(1, 0, fo)[:, None]
    # 3. o volume e o pico
    # (o integrado do original: o arquivo de teste é 16 bits e o original passa de 0 dBFS em alguns picos)
    I0 = measure.ebur(src)['I']
    x = x * 10 ** ((TARGET[role] - I0) / 20)
    for _ in range(2):
        tp = true_peak_db(x)
        if tp > TP_LIMIT:
            x = limit(x, TP_LIMIT - 0.3)
            log.append(f'limitador: o pico era {tp:+.1f} dBTP')
        I1 = ebur_I(x)
        if abs(I1 - TARGET[role]) <= 0.2:
            break
        x = x * 10 ** ((TARGET[role] - I1) / 20)
    I1 = ebur_I(x)
    tp1 = true_peak_db(x)
    # 4. a entrada e a subida
    dur = len(x) / SR
    if plan:
        secs = plan['sections']
        entry = secs[1]['start_s'] + down0 if len(secs) > 1 else 0.0
        sections = [{'name': s['name'], 'start_s': round(s['start_s'] + down0, 3)} for s in secs]
    else:
        e = measure.ebur(src)
        cur = np.array(e['curve'])
        lead_s = lead / SR
        st_ = np.where(cur > -45)[0]
        s0 = int(st_[0]) if len(st_) else 0
        medS = float(np.median(cur[s0:])) if len(cur) > s0 else -20
        full = np.where(cur[s0:] >= medS - 1)[0]
        full_at = (s0 + int(full[0]) if len(full) else 0) - lead_s
        # o short-term (3 s) atrasa: o trecho forte começa ~1,5 s antes; a barra de compasso mais perto antes dele
        f = max(0.0, full_at - 1.5)
        entry = down0 + np.floor((f - down0) / bar + 0.25) * bar if conf >= 6 else f
        entry = max(0.0, min(entry, dur * 0.5))
        sections = []
    build = max(0.0, entry - 4 * bar) if conf >= 6 or plan else max(0.0, entry - 8.0)
    dst = os.path.join(OUT, fac, f'MU_{tid}.wav')
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    write_wav(dst, x)
    return {'id': tid, 'faction': fac, 'role': role, 'asset': f'/Game/Audio/Music/Match/{fac}/MU_{tid}.MU_{tid}', 'source': rel,
            'dur_s': round(dur, 3), 'bpm': round(bpm, 2), 'downbeat_s': round(down0, 3), 'grid': bool(plan) or conf >= 6,
            'entry_s': round(float(entry), 3), 'build_s': round(float(build), 3), 'sections': sections, 'gain_db': 0.0,
            'lufs_before': round(I0, 1), 'lufs': round(I1, 1), 'true_peak_db': round(tp1, 1), 'tempo_conf': round(conf, 1), 'notes': log}


def main():
    os.makedirs(OUT, exist_ok=True)
    only = set(sys.argv[1:])
    old = json.load(open(JSON_OUT, encoding='utf-8')) if os.path.exists(JSON_OUT) and only else None
    tracks = []
    for fac, rel, tid, role in SEL:
        if only and tid not in only:
            prev = next((t for t in (old or {}).get('tracks', []) if t['id'] == tid), None)
            if prev:
                tracks.append(prev)
            continue
        t = process(fac, rel, tid, role)
        tracks.append(t)
        print(f"{tid:28} {role:7} {t['dur_s']:6.1f}s {t['lufs_before']:6.1f} > {t['lufs']:6.1f} LUFS tp {t['true_peak_db']:5.1f} | {t['bpm']:6.1f} bpm"
              f" grade {'sim' if t['grid'] else 'não'} 1º tempo {t['downbeat_s']:5.2f} entrada {t['entry_s']:6.1f} subida {t['build_s']:6.1f}"
              + ('' if not t['notes'] else ' | ' + '; '.join(t['notes'])), flush=True)
    doc = {
        '_comment': 'A trilha da partida (docs/TRILHA_DA_PARTIDA.md): gerado pelo Game/Tools/music/music_pack.py; os assets pelo import_music.py. '
                    'O URTSMusic (Game/Source/RTSGame) lê daqui. gain_db = o ajuste à mão de cada faixa (o volume do ZH), em dB.',
        'tracks': [dict(t, gain_db=GAIN_BY_ROLE.get(t['role'], 0.0)) for t in tracks],
    }
    with open(JSON_OUT, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write('\n')
    print('escrito', JSON_OUT)


if __name__ == '__main__':
    main()
