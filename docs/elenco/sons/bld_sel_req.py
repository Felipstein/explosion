# (04/11) a seleção das construções pela receita aprovada na War Factory: UM conceito, o ambiente do prédio (o que mais existe gravado),
# influence 0,5, 2 s; depois o envelope local (Game/Tools/sound_envelope.py). Ordem dele: o Command Center (o primeiro que se seleciona)
# e depois a Barracks
import json
import urllib.request
REQ = [
    ('Command Center, seleção (a sala de operações)',
     'Military command center operations room ambience: low steady hum of generators and electronic equipment racks, soft ventilation, '
     'calm and quiet, heard from inside a bunker. Professionally recorded field recording.'),
    ('Barracks, seleção (a marcha no pátio)',
     'Soldiers marching in cadence on a gravel drill yard, steady rhythmic boots, heard from a distance through the barracks wall. '
     'Professionally recorded field recording.'),
]
for title, prompt in REQ:
    body = {'title': title, 'session': 'Elenco', 'kind': 'sound', 'models': {'sfx': 1}, 'variations': 1, 'duration': 2.0, 'influence': 0.5,
            'prompt': prompt}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
    print(d['id'], d['folder'])
