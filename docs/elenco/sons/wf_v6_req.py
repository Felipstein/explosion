# (04/11) War Factory, seleção, 6ª: a 5ª saiu só a pancada ("só tem um som de uma panela batendo numa outra"); o zumbido nem veio (o
# gerador faz bem UMA coisa por pedido). Agora UM conceito só, o que mais existe gravado: o ambiente de fábrica (as máquinas trabalhando)
import json
import urllib.request
body = {'title': 'War Factory, seleção (o ambiente da fábrica)', 'session': 'Elenco', 'kind': 'sound', 'models': {'sfx': 1}, 'variations': 1,
        'duration': 2.0, 'influence': 0.5,
        'prompt': ('Heavy industrial factory floor ambience, large machines running steadily in a big hall, deep mechanical hum and '
                   'rhythmic machinery, heard from a distance. Calm and soft. Professionally recorded field recording.')}
req = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
print(json.loads(urllib.request.urlopen(req, timeout=30).read().decode())['id'])
