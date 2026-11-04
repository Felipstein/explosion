# (04/11, item 22 T0 com o PDO) as cenas do teste no World_TestHills: o zero (o chão inteiro some?), o recorte do Arms Dealer e o custo.
# A espera do shader: as fotos com "after_real_s" durante a pausa (2, 45 e 100 s), e um "shots_s" logo depois de cada pausa longa
# (cada passo do teste automático tem 120 s; sem ele a cena_fim estourava).
import io
import json
import time

P = r'C:\Users\luisf\explosion\Game\Config\RTS\Scenarios.json'
MAP = 'World_TestHills'
BASE = [{'key': 'gla_command_center', 'player': 0, 'at_m': [222.0, 308.0]},
        {'key': 'gla_arms_dealer', 'player': 0, 'at_m': [258.0, 308.0]}]
STARTS = [[282.6, 335.7], [-282.6, -335.7]]
ALTO = {'op': 'camera', 'yaw_deg': 45.0, 'distance_m': 32.0, 'pitch_deg': 45.0, 'focus_m': [254.0, 308.0]}
BAIXO = {'op': 'camera', 'yaw_deg': 45.0, 'distance_m': 26.0, 'pitch_deg': 22.0, 'focus_m': [254.0, 308.0]}
TOPO = {'op': 'camera', 'yaw_deg': 45.0, 'distance_m': 40.0, 'pitch_deg': 75.0, 'focus_m': [258.0, 300.0]}
CHAO = {'op': 'camera', 'yaw_deg': 45.0, 'distance_m': 80.0, 'pitch_deg': 45.0, 'focus_m': [0.0, 0.0]}


def shots(t, names_after, hold):
    ops = [{'t_s': t, 'op': 'shot', 'name': n, 'after_real_s': a} for n, a in names_after]
    ops.append({'t_s': t, 'op': 'pause', 'real_s': hold})
    return ops


def at(t, op):
    o = dict(op)
    o['t_s'] = t
    return o


def scen(name, about, structures, starts, camera, shots_s, end_s, ops):
    return {'name': name, 'about': about, 'map': MAP, 'seed': 31, 'fog': 'off', 'hud': False, 'starts_m': starts,
            'units_per_player': [], 'structures': structures, 'units': [], 'camera': camera, 'shots_s': shots_s, 'end_s': end_s,
            'lab': {'panel': False, 'ops': ops}}


zero = scen('t0p_zero', '(04/11, item 22 T0 com o PDO) o recorte ZERADO no World_TestHills: o chão inteiro tem de sumir (Nanite, sem, Nanite de novo)',
            [], [[-282.6, -335.7], [282.6, 335.7]], {'focus_m': [0.0, 0.0], 'distance_m': 80.0, 'yaw_deg': 45.0, 'pitch_deg': 45.0},
            [2.0, 3.05, 3.25], 3.6,
            shots(3.0, [('z1_nanite_02s', 2), ('z1_nanite_45s', 45), ('z1_nanite_100s', 100)], 101)
            + [{'t_s': 3.1, 'op': 'cvar', 'name': 'landscape.RenderNanite', 'value': '0'}]
            + shots(3.15, [('z2_semnanite_05s', 5), ('z2_semnanite_40s', 40)], 41)
            + [{'t_s': 3.3, 'op': 'cvar', 'name': 'landscape.RenderNanite', 'value': '1'}]
            + shots(3.35, [('z3_nanite_volta_05s', 5)], 6))

corte = scen('t0p_corte', '(04/11, item 22 T0 com o PDO) o recorte de verdade do Arms Dealer no World_TestHills: alto, baixo, de cima; com e sem o Nanite',
             BASE, STARTS, {'focus_m': [254.0, 308.0], 'distance_m': 32.0, 'yaw_deg': 45.0, 'pitch_deg': 45.0},
             [2.0, 3.05, 3.45], 3.9,
             [at(3.0, ALTO)]
             + shots(3.0, [('c1_alto_02s', 2), ('c1_alto_45s', 45), ('c1_alto_100s', 100)], 101)
             + [at(3.1, BAIXO)] + shots(3.15, [('c2_baixo', 3)], 4)
             + [at(3.2, TOPO)] + shots(3.25, [('c3_topo', 3)], 4)
             + [{'t_s': 3.3, 'op': 'cvar', 'name': 'landscape.RenderNanite', 'value': '0'}]
             + shots(3.35, [('c4_topo_semnanite_05s', 5), ('c4_topo_semnanite_40s', 40)], 41)
             + [at(3.5, ALTO)] + shots(3.55, [('c5_alto_semnanite', 3)], 4)
             + [at(3.6, BAIXO)] + shots(3.65, [('c6_baixo_semnanite', 3)], 4)
             + [{'t_s': 3.7, 'op': 'cvar', 'name': 'landscape.RenderNanite', 'value': '1'}])

custo = scen('t0p_custo', '(04/11, item 22 T0 com o PDO) o custo: 40 s na câmera do jogo sobre a base do Arms Dealer, sem pausa; no fim as fotos de antes/depois',
             BASE, STARTS, {'focus_m': [258.0, 308.0], 'distance_m': 80.0, 'yaw_deg': 45.0},
             [20.0], 42.0,
             [at(36.0, ALTO), {'t_s': 37.0, 'op': 'shot', 'name': 'corte_alto'},
              at(37.5, BAIXO), {'t_s': 38.5, 'op': 'shot', 'name': 'corte_baixo'},
              at(39.0, CHAO), {'t_s': 40.5, 'op': 'shot', 'name': 'chao'}])

for k in range(5):
    try:
        d = json.loads(io.open(P, encoding='utf-8').read())
        S = d['scenarios']
        for sc in (zero, corte, custo):
            names = [s.get('name') for s in S]
            if sc['name'] in names:
                S[names.index(sc['name'])] = sc
            else:
                S.append(sc)
        io.open(P, 'w', encoding='utf-8', newline='').write(json.dumps(d, indent=2, ensure_ascii=False) + '\n')
        print('cenas t0p_zero, t0p_corte, t0p_custo em', MAP)
        break
    except OSError as e:
        print('de novo:', e)
        time.sleep(2)
