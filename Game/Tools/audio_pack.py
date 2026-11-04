# (04/11/2026) O pacote de som do jogo: os sons APROVADOS (docs/APROVADOS.md, a área Elenco; docs/elenco/SONS_UNIDADES.md) viram os WAV
# do jogo em Game/GameAudio/ e o mapa Game/Config/RTS/Sounds.json, que o URTSAudio (Source/RTSGame/Private/RTSAudio.cpp) lê. O pedido dele
# (04/11): "os sons aprovados pode colocar no jogo". Tudo sai daqui: mudou um som, roda de novo (nunca editar o Sounds.json à mão).
#   blender -b --factory-startup --python Game/Tools/audio_pack.py
# O nível (o mesmo padrão pra todas as vozes, a regra de 04/11 "mantém padrão no jogo"): as falas (unidades e EVA) com a mesma
# intensidade da fala (o RMS dos trechos com voz, acima de −40 dB, em −20 dBFS; o pico no máximo −1 dBFS); o clique e a seleção dos
# prédios já saem nivelados das ferramentas (−14 e −16 dBFS de pico) e entram como estão. Mono 16 bits 44,1 kHz (o som de interface
# e de voz não é posicional). As fontes: as amostras e os recortes em captures/audio/ (as receitas: docs/elenco/SONS_UNIDADES.md) e os
# aprovados em docs/aprovados/sons/.
import json
import os
import wave

import aud
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CAP = os.path.join(REPO, 'captures', 'audio')
OUT = os.path.join(REPO, 'Game', 'GameAudio')
R = 44100


def src(*p):
    return os.path.join(CAP, *p)


CATS = {'selecionado': 'select', 'andando': 'move', 'atacando': 'attack', 'pronto': 'ready'}


def doc_pools(fac, key, folder, skip=()):
    """(04/11) As falas de uma unidade recortadas da amostra aprovada (captures/audio/<pasta>/falas/NN_<fala>.wav, Game/Tools/voice_cut.py
    --texts), nos grupos da ficha (docs/elenco/<facção>/<chave>.md, a seção 8: selecionado, andando, atacando, pronto). skip = os
    pedaços que saíram fora da curva no recorte (a duração contra o texto)."""
    import re
    text = open(os.path.join(REPO, 'docs', 'elenco', fac, key + '.md'), encoding='utf-8').read()
    talk = text.split('## 8.', 1)[1].split(chr(10) + '## ', 1)[0].split('**As falas**', 1)[1].split('- **O corpo', 1)[0]
    files = sorted(os.listdir(src(folder, 'falas')))
    pools, n = {}, 0
    for cat, body in re.findall(r'^\s+- ([^:\n]+):(.*?)(?=^\s+- [^:\n]+:|\Z)', talk, re.S | re.M):
        for _ in re.findall(r'"([^"]+)"', body):
            n += 1
            f = next(x for x in files if x.startswith('%02d_' % n))
            if cat.strip() in CATS and not any(k in f for k in skip):
                pools.setdefault(CATS[cat.strip()], []).append(src(folder, 'falas', f))
    assert n == len(files), (key, n, len(files))   # cada fala da ficha tem o seu pedaço recortado
    return pools


# --- o que entra (a chave do jogo -> os arquivos; cada lista é sorteada sem repetir a última) ---
UI = {'click': [os.path.join(REPO, 'docs', 'aprovados', 'sons', '2026-11-04_hud_clique_APROVADO.wav')]}
STRUCTURES = {
    'usa_war_factory': {'select': [os.path.join(REPO, 'docs', 'aprovados', 'sons', '2026-11-04_usa_war_factory_selecao_NO_RUMO.wav')]},
    'usa_barracks': {'select': [os.path.join(REPO, 'docs', 'aprovados', 'sons', '2026-11-04_usa_barracks_selecao_APROVADA.wav')]},
}
M1 = sorted(os.listdir(src('03_m1_as_falas')))
DZ = sorted(os.listdir(src('07_dozer_as_falas')))
M1 = [f for f in M1 if f.endswith('.mp3')]
DZ = [f for f in DZ if f.endswith('.mp3')]
UNITS = {
    # a ficha de cada um (docs/elenco/usa/usa_mbt.md e usa_dozer.md, a seção 8): as falas na ordem dos grupos
    'usa_mbt': {'select': [src('03_m1_as_falas', f) for f in M1[0:4]], 'move': [src('03_m1_as_falas', f) for f in M1[4:8]],
                'attack': [src('03_m1_as_falas', f) for f in M1[8:12]], 'ready': [src('03_m1_as_falas', f) for f in M1[16:20]]},
    'usa_dozer': {'select': [src('07_dozer_as_falas', f) for f in DZ[0:4]], 'move': [src('07_dozer_as_falas', f) for f in DZ[4:8]],
                  'build': [src('07_dozer_as_falas', f) for f in DZ[8:12]], 'repair': [src('07_dozer_as_falas', f) for f in DZ[12:16]],
                  'ready': [src('07_dozer_as_falas', f) for f in DZ[20:24]]},
    # (04/11) as vozes aprovadas ("o Ranger e o Javelin tá até que aprovado"), as falas recortadas da amostra pelo texto; no Ranger,
    # "Hostiles down." e "Take cover!" saíram emendadas no recorte e ficam fora
    'usa_ranger': doc_pools('usa', 'usa_ranger', '39_usa_ranger_a_voz', skip=('hostiles_down', 'take_cover')),
    'usa_javelin': doc_pools('usa', 'usa_javelin', '40_usa_javelin_a_voz'),
    # (04/11) as 8 vozes da 2ª volta, cada uma com a sua identidade (docs/elenco/sons/vozes_usa_req.py): "todos aprovados"
    'usa_stryker': doc_pools('usa', 'usa_stryker', '58_usa_stryker_a_voz'),
    'usa_avenger': doc_pools('usa', 'usa_avenger', '59_usa_avenger_a_voz'),
    'usa_paladin': doc_pools('usa', 'usa_paladin', '60_usa_paladin_a_voz'),
    'usa_tomahawk': doc_pools('usa', 'usa_tomahawk', '61_usa_tomahawk_a_voz'),
    'usa_comanche': doc_pools('usa', 'usa_comanche', '62_usa_comanche_a_voz'),
    'usa_raptor': doc_pools('usa', 'usa_raptor', '63_usa_raptor_a_voz'),
    'usa_chinook': doc_pools('usa', 'usa_chinook', '64_usa_chinook_a_voz'),
    'usa_supply_truck': doc_pools('usa', 'usa_supply_truck', '65_usa_supply_truck_a_voz'),
}
E_USA, E_CHN, E_RUS, E_GLA = (src('23_usa_eva_falas_da_amostra_aprovada'), src('31_china_eva_falas_da_amostra_aprovada'),
                              src('32_russia_eva_falas_da_amostra_aprovada'), src('38_gla_eva_falas_das_amostras'))
EVA = {
    'usa': {'base_attack': [E_USA + '/01_our_base_is_under_attack.wav'], 'ally_attack': [E_USA + '/02_allied_base_under_attack.wav'],
            'unit_lost': [E_USA + '/03_unit_lost.wav'], 'money': [E_USA + '/04_insufficient_funds.wav'],
            'low_power': [E_USA + '/05_low_power.wav'], 'upgrade': [E_USA + '/06_upgrade_complete.wav']},
    'china': {'base_attack': [E_CHN + '/01_our_base_is_under_attack.wav'], 'ally_attack': [E_CHN + '/02_allied_base_under_attack.wav'],
              'unit_lost': [E_CHN + '/03_unit_lost.wav'], 'money': [E_CHN + '/04_funds_insufficient.wav'],
              'low_power': [E_CHN + '/05_low_power.wav'], 'upgrade': [E_CHN + '/06_upgrade_complete.wav']},
    'russia': {'base_attack': [E_RUS + '/01_base_under_fire.wav'], 'ally_attack': [E_RUS + '/02_allied_base_under_fire.wav'],
               'unit_lost': [E_RUS + '/03_unit_lost.wav'], 'money': [E_RUS + '/04_not_enough_funds.wav'],
               'low_power': [E_RUS + '/05_low_power.wav'], 'upgrade': [E_RUS + '/06_upgrade_complete.wav']},
    # a GLA: uma pessoa da célula por aviso, com o mais velho de vez em quando ("pode usar todas, variando"); sem energia (sem usina)
    'gla': {'base_attack': [E_GLA + '/base_attack_1_they_are_attacking_our_home.wav', E_GLA + '/base_attack_2_inside_the_walls.wav',
                            E_GLA + '/base_attack_3_our_home_is_burning.wav', E_GLA + '/elder_base_attack.wav'],
            'ally_attack': [E_GLA + '/ally_attack_1_our_allies_are_under_attack.wav', E_GLA + '/ally_attack_2_hitting_our_friends.wav',
                            E_GLA + '/ally_attack_3_our_allies_need_help.wav', E_GLA + '/elder_ally_attack.wav'],
            'unit_lost': [E_GLA + '/unit_lost_1_a_warrior_has_fallen.wav', E_GLA + '/unit_lost_2_we_lost_one_of_ours.wav',
                          E_GLA + '/unit_lost_3_another_of_the_faithful.wav', E_GLA + '/unit_lost_4_his_name_stays_with_us.wav',
                          E_GLA + '/elder_unit_lost.wav'],
            'money': [E_GLA + '/money_1_we_need_more_money.wav', E_GLA + '/money_2_the_box_is_empty.wav',
                      E_GLA + '/money_3_not_enough_money.wav', E_GLA + '/money_4_bring_me_more.wav', E_GLA + '/elder_money.wav'],
            'upgrade': [E_GLA + '/upgrade_1_upgrade_ready.wav', E_GLA + '/upgrade_2_our_warriors_grow_stronger.wav',
                        E_GLA + '/upgrade_3_new_weapons_ready.wav', E_GLA + '/elder_upgrade.wav']},
}
# as regras dos avisos: as do ZH (Eva.ini 1.04: a prioridade, o intervalo mínimo entre dois iguais e a validade na fila)
EVA_RULES = {'base_attack': [7, 30.0, 5.0], 'ally_attack': [6, 30.0, 5.0], 'upgrade': [3, 0.0, 5.0], 'money': [3, 1.5, 2.0],
             'low_power': [2, 120.0, 5.0], 'unit_lost': [1, 15.0, 5.0]}
VOICE_KINDS = ('units', 'eva')


def load(p):
    s = aud.Sound(p)
    if int(s.specs[0]) != R:
        s = s.resample(R, True)
    d = np.asarray(s.data(), dtype=np.float32)
    return d.mean(axis=1) if d.ndim == 2 else d


def level_speech(x):
    N = int(0.02 * R)
    w = np.array([np.sqrt(np.mean(x[i:i + N] ** 2)) for i in range(0, max(1, len(x) - N), N)])
    act = w[w > 10 ** (-40 / 20)]
    rms = float(np.sqrt(np.mean(act ** 2))) if len(act) else float(np.sqrt(np.mean(x ** 2)) + 1e-9)
    y = x * (10 ** (-20 / 20) / max(rms, 1e-9))
    pk = float(np.abs(y).max())
    return y * min(1.0, 10 ** (-1 / 20) / max(pk, 1e-9))


def write(rel, x):
    p = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with wave.open(p, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(R)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())
    return rel.replace('\\', '/')


def pack(group, table, voice):
    out = {}
    for key, kinds in table.items():
        out[key] = {}
        for kind, files in kinds.items():
            rels = []
            for i, f in enumerate(files):
                x = load(f)
                rels.append(write(os.path.join(group, key, f'{kind}_{i + 1}.wav'), level_speech(x) if voice else x))
            out[key][kind] = rels
    return out


data = {'_about': ('(gerado por Game/Tools/audio_pack.py, não editar à mão) Os sons aprovados do jogo: os caminhos são relativos a '
                   'Game/GameAudio/. ui = a interface (igual nas 4 facções); structures = o som de selecionar o prédio; units = as falas '
                   'de cada unidade (select ao selecionar, move/attack nas ordens, ready ao sair da fábrica); eva = os avisos da '
                   'partida por facção; eva_rules = [prioridade, intervalo mínimo s, validade na fila s] (o Eva.ini do ZH). '
                   'Receitas: docs/elenco/SONS_UNIDADES.md.'),
        'ui': pack('ui', {'hud': UI}, False)['hud'],
        'structures': pack('structures', STRUCTURES, False),
        'units': pack('units', UNITS, True),
        'eva': pack('eva', EVA, True),
        'eva_rules': EVA_RULES}
with open(os.path.join(REPO, 'Game', 'Config', 'RTS', 'Sounds.json'), 'w', encoding='utf-8', newline='\n') as f:
    json.dump(data, f, ensure_ascii=False, indent=1)
n = sum(len(v) for g in ('structures', 'units', 'eva') for k in data[g].values() for v in k.values()) + len(data['ui']['click'])
print('AUDIO_PACK ok', n, 'arquivos')
