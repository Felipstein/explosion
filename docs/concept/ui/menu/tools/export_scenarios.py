# Os cenários de teste (Game/Config/RTS/Scenarios.json) e o nome dos mapas (Maps.json) pra tela do SCRIPT do protótipo.
# No jogo a tela lê o mesmo arquivo direto (o esquema da interface serve api/scenarios.json).
import json, os
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G = os.path.normpath(os.path.join(S, '..', '..', '..', '..', 'Game', 'Config', 'RTS'))
sc = json.load(open(os.path.join(G, 'Scenarios.json'), encoding='utf-8'))['scenarios']
maps = {m['name']: m.get('title', m['name']) for m in json.load(open(os.path.join(G, 'Maps.json'), encoding='utf-8'))['maps']}
out = []
for s in sc:
    units = sum(len(u.get('units', [])) if isinstance(u, dict) else 1 for u in s.get('units', [])) if isinstance(s.get('units'), list) else 0
    out.append({'name': s['name'], 'about': s.get('about', ''), 'map': s.get('map', ''), 'map_title': maps.get(s.get('map', ''), s.get('map', '')),
                'units': len(s.get('units', [])), 'structures': len(s.get('structures', [])), 'orders': len(s.get('orders', [])),
                'end_s': s.get('end_s', 0), 'lab': bool(s.get('lab')), 'hud': s.get('hud', True) is not False, 'fog': s.get('fog', '')})
os.makedirs(os.path.join(S, 'data'), exist_ok=True)
json.dump({'scenarios': out}, open(os.path.join(S, 'data', 'scenarios.json'), 'w', encoding='utf-8'), ensure_ascii=False)
print(len(out))
