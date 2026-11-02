# O kit civil v2 na vila do jogo (01/11/2026, item 20 do docs/elenco/TAREFAS.md): as miudezas aprovadas ("as miudezas aprovadas",
# a galeria de 01/11) no lugar das da maquete do Look, no arquivo da vila que o jogo lê (Config/RTS/Villages/<mapa>.json).
#
# Por que aqui e não no gerador: a vila sai do WorldForge (WorldVillage.cpp, a tabela Kits), que é de outra sessão e está com trabalho
# aberto; recompilar ele agora levaria o trabalho dos outros pela metade. A troca é só de MALHA (o "mesh" da peça): o "type" fica, então
# a peça segue amassando como antes (o SquashByType do RTSMatchSubsystem.cpp lê o type), e nada muda na simulação (as miudezas não
# bloqueiam; a grade do mapa e o hash da LAN não leem a malha). Quando o gerador ganhar a tabela nova (a proposta em docs/KIT_CIVIL_V2.md
# §3), este passo sai. Rodar de novo depois de gerar a vila: é idempotente (o "kit": "v2" no topo do arquivo).
#
# A escolha de cada peça é determinística (o hash do id e da posição): os dois PCs da LAN veem a mesma vila.
#   python Tools/village_kit_v2.py [mapa ...]       (sem mapa: todos os de Config/RTS/Villages)
import hashlib
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VILLAGES = ROOT / 'Config' / 'RTS' / 'Villages'

# a pegada medida de cada GLB do v2 (captures/game/models/civ_v2, a caixa da malha: W ao longo de X, D ao longo de Y, H), m
SIZE = {
    'SM_civ_water_tank': (1.32, 1.14, 1.38),
    'SM_civ_water_tank_stand': (1.44, 1.44, 2.58),
    'SM_civ_tank_horizontal': (0.87, 1.76, 1.53),
    'SM_civ_roof_ac': (0.85, 0.55, 0.66),
    'SM_civ_dish': (0.92, 0.64, 1.09),
    'SM_civ_laundry': (2.60, 0.16, 1.65),
    'SM_civ_awning_wall': (3.10, 1.66, 2.51),
    'SM_civ_tires': (1.46, 1.18, 0.80),
    'SM_civ_rubble': (3.18, 3.07, 1.14),
    'SM_civ_pole_transformer': (2.26, 1.01, 8.02),
}
# o tipo da maquete -> as malhas do v2 com o peso de cada (a caixa d'água tem 3 jeitos nos game shots: a de plástico no chão da laje, a na
# torre de ferro e o tanque deitado)
SWAP = {
    'LL_Prop_WaterTank': [('SM_civ_water_tank', 0.45), ('SM_civ_water_tank_stand', 0.30), ('SM_civ_tank_horizontal', 0.25)],
    'LL_Prop_AC': [('SM_civ_roof_ac', 1.0)],
    'LL_Prop_Dish': [('SM_civ_dish', 1.0)],
    'LL_Prop_Laundry': [('SM_civ_laundry', 1.0)],
    'LL_Prop_Awning': [('SM_civ_awning_wall', 1.0)],
    'LL_Prop_Tires': [('SM_civ_tires', 1.0)],
    'LL_Prop_Rubble0': [('SM_civ_rubble', 1.0)],
}
TRANSFORMER_EVERY = 5   # 1 a cada ~5 postes leva o transformador (os mesmos isoladores: os fios ligam igual)
PARAPET = 0.45          # a mureta da laje (look_houses.py): a peça do teto fica dentro dela


def unit(*parts):
    """Um número em [0, 1) fixo pela peça (o mesmo em qualquer PC)."""
    h = hashlib.sha1('|'.join(str(p) for p in parts).encode('utf-8')).digest()
    return int.from_bytes(h[:8], 'little') / 2.0 ** 64


def radius(mesh):
    w, d, _ = SIZE[mesh]
    return 0.5 * math.hypot(w, d)


def apply(path):
    text = path.read_text(encoding='utf-8')
    v = json.loads(text)
    if v.get('kit') == 'v2':
        return f'{path.name}: já no v2'
    pieces = v['pieces']
    houses = [p for p in pieces if str(p['type']).startswith('civ_house')]
    roof = [p for p in pieces if 'z' in p]   # as peças em cima da laje
    count = {}

    def house_of(p):
        for h in houses:
            if abs(p['x'] - h['x']) <= 0.5 * h['size_m'][0] and abs(p['y'] - h['y']) <= 0.5 * h['size_m'][1]:
                return h
        return None

    def fits_roof(p, mesh):
        """A peça nova cabe na laje: dentro da mureta e sem encostar nas outras do mesmo teto (com a pegada nova delas)."""
        h = house_of(p)
        if h is None:
            return True
        r = radius(mesh)
        if abs(p['x'] - h['x']) + r > 0.5 * h['size_m'][0] - PARAPET or abs(p['y'] - h['y']) + r > 0.5 * h['size_m'][1] - PARAPET:
            return False
        for o in roof:
            if o is p or abs(o.get('z', 0) - p['z']) > 0.3:
                continue
            ro = radius(o['mesh']) if o['mesh'] in SIZE else 0.5 * max(o['size_m'])
            if math.hypot(o['x'] - p['x'], o['y'] - p['y']) < r + ro + 0.15:
                return False
        return True

    changed = {}   # id -> (malha, size_m, height_m)
    for p in pieces:
        opts = SWAP.get(p['type'])
        if not opts:
            continue
        u = unit(v.get('map'), p['id'], p['x'], p['y'])
        acc = 0.0
        pick = opts[-1][0]
        for mesh, w in opts:
            acc += w
            if u < acc:
                pick = mesh
                break
        if 'z' in p and not fits_roof(p, pick):
            # o maior não cabe: o menor da lista; se nem ele cabe, fica a da maquete
            pick = next((m for m, _ in sorted(opts, key=lambda o: radius(o[0])) if fits_roof(p, m)), None)
            if pick is None:
                count['ficou a da maquete'] = count.get('ficou a da maquete', 0) + 1
                continue
        w, d, hgt = SIZE[pick]
        p['mesh'] = pick   # (a pegada nova vale pro teste das próximas do mesmo teto)
        size = [w, d] if p.get('rot', 0) % 2 == 0 else [d, w]
        changed[p['id']] = (pick, size, hgt)
        count[pick] = count.get(pick, 0) + 1
    poles = sorted((p for p in pieces if p['type'] == 'civ_pole'), key=lambda p: p['id'])
    first = int(unit(v.get('map'), 'poste') * TRANSFORMER_EVERY)
    for i, p in enumerate(poles):
        if i % TRANSFORMER_EVERY == first:
            changed[p['id']] = ('SM_civ_pole_transformer', None, None)
            count['SM_civ_pole_transformer'] = count.get('SM_civ_pole_transformer', 0) + 1
    # a troca linha por linha (o arquivo tem uma peça por linha, com o formato do WorldForge): o resto do arquivo fica igual, byte a byte
    out = []
    for line in text.split('\n'):
        m = re.match(r'\s*\{"id": (\d+),', line)
        if m and int(m.group(1)) in changed:
            mesh, size, hgt = changed[int(m.group(1))]
            line = re.sub(r'"mesh": "[^"]*"', f'"mesh": "{mesh}"', line, count=1)
            if size:
                line = re.sub(r'"size_m": \[[^\]]*\]', f'"size_m": [{size[0]:.2f}, {size[1]:.2f}]', line, count=1)
                line = re.sub(r'"height_m": [0-9.]+', f'"height_m": {hgt:.2f}', line, count=1)
        out.append(line)
        if line.startswith(' "about": '):
            out.append(' "kit": "v2",')
            out.append(' "kit_about": "(01/11) Game/Tools/village_kit_v2.py: as miudezas do kit civil v2 no lugar das da maquete (só a malha; o '
                       'type fica, pro amasso). Sai quando o WorldVillage.cpp ganhar a tabela nova (docs/KIT_CIVIL_V2.md §3).",')
    new = '\n'.join(out)
    json.loads(new)   # (confere antes de gravar)
    path.write_text(new, encoding='utf-8', newline='')
    return f'{path.name}: ' + ', '.join(f'{k} {n}' for k, n in sorted(count.items()))


def main():
    names = sys.argv[1:]
    files = [VILLAGES / f'{n}.json' for n in names] if names else sorted(f for f in VILLAGES.glob('*.json') if not f.name.endswith('.map.json'))
    for f in files:
        print(apply(f))


if __name__ == '__main__':
    main()
