# (04/11) a GLA com VÁRIAS vozes: "o GLA pode ter vários tipos diferentes de voz; o icônico C&C ZH é gostoso de ouvir porque o 'our base
# is under attack' era numa voz realmente sentindo dor, mas já diferente das outras; tenta cada um uma voz um pouco diferente, com
# diferentes pesos, sentindo peso nas frases". (No ZH o "base sob ataque" da GLA é a voz do Rebel, `Eva.ini`.) A GLA é uma rede de gente,
# não uma sala: cada aviso, uma pessoa da célula, com o peso dela. O mais velho (30_) fica com as falas do Command Center. 1 candidata cada;
# a amostra leva a fala clara primeiro e mais 2-3 no mesmo peso (as variações do aviso, recortadas da amostra)
import json
import urllib.request
TAIL = ' Light Arabic accent, fluent English. Clean, dry studio recording.'
V = [
    ('GLA, base sob ataque (o ferido)',
     'Young Arab male resistance fighter, mid-twenties, wounded and speaking through real pain, strained, breathless and raw, his voice '
     'breaking under the effort but still clear, desperate to warn his people.' + TAIL,
     'They are attacking our home! They are inside the walls, hold them back! Our home is burning, everyone, to the walls!'),
    ('GLA, aliado sob ataque (o vigia)',
     'Arab male lookout on a rooftop, early twenties, fast and urgent, sharp and alert, tense voice pitched up by adrenaline, short '
     'bursts, calling out what he sees.' + TAIL,
     'Our allies are under attack! They are hitting our friends in the east! Our allies need help, move, move!'),
    ('GLA, unidade perdida (o amigo)',
     'Arab male veteran fighter, mid-forties, low, heavy and quiet voice weighed down by grief, slow, every word carrying loss, controlled '
     'and dignified, never crying.' + TAIL,
     'A warrior has fallen. We lost one of ours. Another of the faithful is gone. His name stays with us, in our home.'),
    ('GLA, sem dinheiro (o contrabandista)',
     'Arab male smuggler and quartermaster of the cell, late thirties, dry, wry and street-smart, a sly half-smile in his voice, relaxed '
     'and unbothered, counting every coin.' + TAIL,
     'We need more money. The box is empty, my friend. Not enough money for that. Bring me more and we talk.'),
    ('GLA, melhoria pronta (o armeiro)',
     'Arab male arms maker and mechanic, mid-fifties, deep, proud and satisfied voice, rough from years of smoke and metal, slow and '
     'confident, like a craftsman showing his work.' + TAIL,
     'Upgrade ready. Our warriors grow stronger. The new weapons are ready for the faithful. Take them, they will not fail you.'),
]
for title, desc, sample in V:
    assert len(sample) >= 100, (title, len(sample))
    body = {'title': title, 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1}, 'prompt': desc, 'sample': sample}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
    print(d['id'], d['folder'])
