# (04/11) o clique do HUD: "bem pensado, bem feito, simples". O HUD é o console militar (o ZH clássico modernizado, as chapas, o console
# que vive da energia): o clique = o botão de pressão pesado de um console militar, seco e curto. Um som só, literal (influence 0,7); o
# gerador entrega no mínimo 0,5 s, então o recorte do transiente (~90 ms) é local (Game/Tools/sound_click.py)
import json
import urllib.request
body = {'title': 'HUD, o clique (o botão do console)', 'session': 'Elenco', 'kind': 'sound', 'models': {'sfx': 1}, 'variations': 1,
        'duration': 0.5, 'influence': 0.7,
        'prompt': ('A single short tactile click of a heavy metal push button on a military control console, crisp, solid and dry, '
                   'close-mic. Professionally recorded foley.')}
r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
print(d['id'], d['folder'])
