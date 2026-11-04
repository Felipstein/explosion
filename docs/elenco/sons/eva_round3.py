# (04/11) "Rússia aprovado, China eu fico com a da mulher, aprovado" -> guarda as duas na biblioteca. "O GLA quero novo": "quanto brother
# no GLA, cadê o warrior, cadê as outras palavras que dão peso" -> as falas com o peso (lar, guerreiros, fiéis, caiu), "brother" nenhuma
# vez; o mesmo homem (o mais velho da célula), 1 candidata
import json
import urllib.request


def post(path, body):
    r = urllib.request.Request('http://127.0.0.1:8140' + path, data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    return json.loads(urllib.request.urlopen(r, timeout=60).read().decode())


print(post('/api/voice/save', {'key': 'mw1pgxlitjo:a:0:0', 'name': 'Russia EVA'}))
print(post('/api/voice/save', {'key': 'mw1pevw1ko1:a:0:0', 'name': 'China EVA'}))
d = post('/api/requests', {
    'title': 'GLA, a EVA homem 2 (o peso)', 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1},
    'prompt': ('Arab male resistance elder, early fifties, deep, warm and weathered voice with natural gravel, heavy with conviction and '
               'gravitas; street-smart, adaptable and unflappable, a hint of wry humor even under fire; he speaks to his fighters like a '
               'father, intimate and loyal, never shouting, never fanatical. Light Arabic accent, fluent English. Clean, dry studio recording.'),
    'sample': ('They are attacking our home! Our allies are under attack! A warrior has fallen. We need more money. Upgrade ready, our '
               'warriors grow stronger. The commander is listening. Our home stands. The faithful are with us. We are here to stay. '
               'Another pair of hands.')})
print(d['id'], d['folder'])
