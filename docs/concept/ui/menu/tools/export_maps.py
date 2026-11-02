# Exporta os mapas do skirmish pro protótipo da tela (só lê Game/Config/RTS).
# Pra cada mapa: maps/<nome>.png (512², R asfalto, G terra, B rocha, A água) e maps/<nome>.json
# (bases, docas de suprimento, peças da vila, palmeiras e postes, em metros com o centro do mapa em 0,0).
import json, os, sys
import numpy as np
from PIL import Image

RTS = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..', 'Game', 'Config', 'RTS')
OUT = os.path.join(os.path.dirname(__file__), '..', 'maps')
PICK = {'World_TestSalt2': 'SALT FLATS', 'World_TestHills': 'BROKEN HILLS', 'World_TestDunesSim': 'DUNE SEA'}

maps = {m['name']: m for m in json.load(open(os.path.join(RTS, 'Maps.json'), encoding='utf-8'))['maps']}
os.makedirs(OUT, exist_ok=True)
index = []
for name, title in PICK.items():
    m = maps[name]
    surf = np.array(Image.open(os.path.join(RTS, 'Surface', name + '.png')).convert('RGBA')).astype(np.float32)
    grid = np.array(Image.open(os.path.join(RTS, 'Grid', name + '.png')).convert('RGBA'))
    water = np.kron((grid[..., 0] >= 2).astype(np.float32), np.ones((4, 4))) * 255     # vau e fundo (o raso da margem fica de fora)
    # 1024 → 512: a estrada pelo máximo (não some a linha fina), o resto pela média
    def pool(a, f):
        return f(f(a.reshape(512, 2, 512, 2), axis=3), axis=1)
    # a água vai num arquivo à parte: no canal de transparência o navegador zera a cor do que é transparente
    out = np.stack([pool(surf[..., 0], np.max), pool(surf[..., 1], np.mean), pool(surf[..., 2], np.mean)], -1)
    Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGB').save(os.path.join(OUT, name + '.png'))
    wet = pool(water, np.max)
    Image.fromarray(wet.clip(0, 255).astype(np.uint8), 'L').save(os.path.join(OUT, name + '_water.png'))
    v = json.load(open(os.path.join(RTS, 'Villages', name + '.json'), encoding='utf-8'))
    pieces = []
    for p in v['pieces']:
        w, d = p.get('size_m', [p['cells'][0] * 4, p['cells'][1] * 4])
        if p.get('rot', 0) % 2: w, d = d, w
        pieces.append([round(p['x'], 1), round(p['y'], 1), round(w, 1), round(d, 1), round(p.get('height_m', 2.0), 1), 1 if p['type'].startswith('civ_') else 0])
    data = {'name': name, 'title': title, 'size_m': m['size_m'], 'starts': m['starts'],
            'docks': [[d['x'], d['y'], d['size_m'][0], d['size_m'][1]] for d in v.get('supply_docks', [])],
            'pieces': pieces, 'palms': v.get('palms', []), 'poles': [[p['x'], p['y']] for p in v.get('poles', [])]}
    json.dump(data, open(os.path.join(OUT, name + '.json'), 'w', encoding='utf-8'), separators=(',', ':'))
    index.append({'name': name, 'title': title, 'bases': len(m['starts']), 'docks': len(data['docks'])})
    print(name, title, 'bases', len(m['starts']), 'docas', len(data['docks']), 'peças', len(pieces), 'água %.3f' % (wet > 0).mean())
json.dump(index, open(os.path.join(OUT, 'index.json'), 'w', encoding='utf-8'), indent=1)
