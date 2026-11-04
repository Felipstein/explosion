# (04/11) a Barracks: "ao invés de 4 passadas só 3, e com uma cereja no bolo no final pra acabar bem". A cereja = o "alto!" da ordem
# unida: a esquadra para junta, a última batida de bota mais pesada, no tempo do 4º passo. Um som só, literal
import json
import urllib.request
body = {'title': 'Barracks, a cereja (o alto da esquadra)', 'session': 'Elenco', 'kind': 'sound', 'models': {'sfx': 1}, 'variations': 1,
        'duration': 0.8, 'influence': 0.7,
        'prompt': ('A squad of soldiers coming to a halt in unison: one final heavy boot stomp together on a gravel drill yard, with a short '
                   'echo off a building wall. Professionally recorded foley.')}
r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
print(d['id'], d['folder'])
