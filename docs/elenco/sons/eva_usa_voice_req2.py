# (04/11) a EVA da USA de novo, LIMPA: o padrão do jogo é a voz gravada limpa e seca, e o rádio é um efeito só, igual pra todos, aplicado
# pelo jogo. A descrição sem rádio, sem chiado, sem ruído: só a pessoa e o jeito de falar. 1 candidata
import json
import urllib.request
body = {'title': 'USA, a EVA limpa (a operadora da base)', 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1},
        'prompt': ('American female military operations officer, early thirties, calm, crisp and clipped, authoritative and never shouting, '
                   'precise diction, short measured phrases; she reports alerts to the base commander. General American accent. '
                   'Clean, dry studio recording.'),
        'sample': ('Our base is under attack. Unit lost. Insufficient funds. Low power. Upgrade complete. Global Strike ready. '
                   'Launch confirmed. Enemy Nuclear Missile detected. SCUD Storm launched! Command post, listening.')}
r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
print(d['id'], d['folder'])
