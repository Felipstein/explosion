# (04/11) as EVAs da China, da Rússia e da GLA de novo, HOMENS ("por que todas são mulheres? a USA eu entendi, mas o restante podia ser
# homem"). A GLA muda de alma: "tem que ter jogo de cintura, não é unidades, guerra, são GUERREIROS, FIÉIS, AMIGOS, não é base, é LAR,
# tem que ter um peso" = o mais velho da célula, que fala com os seus como pai, malandro e firme; as falas dizem lar, irmãos, guerreiros
# (o aviso claro primeiro). Voz LIMPA; 1 candidata cada; a amostra leva todas as falas daquela voz (recortar dela)
import json
import urllib.request
EVAS = [
    ('China, a EVA homem (a voz do Estado)',
     'Chinese male state broadcaster serving as the army command voice, late forties, deep resonant baritone, solemn and ceremonial, '
     'perfectly even diction, slow uniform cadence, every phrase ending with calm finality; he speaks for the collective, proud and '
     'unshakable, never shouting. Light Mandarin accent, fluent English. Clean, dry studio recording.',
     'Our base is under attack. All forces, respond. Allied base under attack. Unit lost. Funds insufficient. Low power. Upgrade complete. '
     'The State is listening. Command center established. Engineers dispatched. The command center is under attack!'),
    ('Rússia, a EVA homem (o oficial de serviço)',
     'Russian male general staff duty officer, mid-fifties, low bass voice, cold, level and unhurried; he reads alerts like entries in a '
     'logbook, few words, flat matter-of-fact intonation, a brief pause before the key word, unshakable, never shouting. Light Russian '
     'accent, fluent English. Clean, dry studio recording.',
     'Base under fire. Allied base under fire. Unit lost. Not enough funds. Low power. Upgrade complete. Staff is listening. Headquarters '
     'established. Engineer vehicle dispatched. Headquarters under fire!'),
    ('GLA, a EVA homem (o mais velho da célula)',
     'Arab male resistance elder, early fifties, deep, warm and weathered voice with natural gravel, heavy with conviction; street-smart, '
     'adaptable and unflappable, a hint of wry humor even under fire; he speaks to his fighters like a father to his sons, intimate and '
     'loyal, never shouting, never fanatical. Light Arabic accent, fluent English. Clean, dry studio recording.',
     'They are attacking our home! Our brothers are under attack! We lost a brother. We do not have the money, brother. Upgrade ready, '
     'our warriors grow stronger. The commander is listening. Our home is ready. We are here to stay. Another pair of hands.'),
]
for title, desc, sample in EVAS:
    body = {'title': title, 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1}, 'prompt': desc, 'sample': sample}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
    print(d['id'], d['folder'], len(sample))
