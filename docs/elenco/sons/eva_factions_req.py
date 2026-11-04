# (04/11) as EVAs da China, da Rússia e da GLA ("fico no aguardo das EVAs restantes"), pela proposta discutida (docs/elenco/SONS_UNIDADES.md,
# "A EVA"): a EVA é sempre mulher, a facção está na pessoa, voz LIMPA. 1 candidata cada. A amostra leva TODAS as falas daquela voz (os
# avisos e as do Command Center), pra recortar da amostra aprovada sem gerar de novo; as superarmas ficam fora (os nomes não fecharam);
# a GLA não tem energia (sem usina), então sem "pouca energia"
import json
import urllib.request
EVAS = [
    ('China, a EVA (a voz do Estado)',
     'Chinese female army staff officer, mid-thirties, composed and solemn, with the poised, evenly weighted diction of a state news anchor; '
     'full, rounded mid-range voice, steady unhurried pace, each phrase ending with calm finality, serious and never shouting. Light Mandarin '
     'accent, fluent English. Clean, dry studio recording.',
     'Our base is under attack. All forces, respond. Allied base under attack. Unit lost. Funds insufficient. Low power. Upgrade complete. '
     'The State is listening. Command center established. Engineers dispatched. The command center is under attack!'),
    ('Rússia, a EVA (a oficial de serviço)',
     'Russian female army staff duty officer, mid-forties, low alto voice, cold, level and unhurried; she reads alerts like entries in a '
     'logbook, few words, flat matter-of-fact intonation, a brief pause before the key word, never shouting. Light Russian accent, fluent '
     'English. Clean, dry studio recording.',
     'Base under fire. Allied base under fire. Unit lost. Not enough funds. Low power. Upgrade complete. Staff is listening. Headquarters '
     'established. Engineer vehicle dispatched. Headquarters under fire!'),
    ('GLA, a EVA (a voz da rede)',
     'Female resistance fighter, late twenties, low husky voice with natural grit, fast, urgent and controlled, direct and informal, '
     'intense and personal, as if speaking to the one person she trusts, never shouting. Light Arabic accent, fluent English. Clean, dry '
     'studio recording.',
     'They are attacking the base! Our allies are under attack! We lost a unit. Not enough money. Upgrade ready. The commander is '
     'listening. Command center ready. Worker sent. They found the compound!'),
]
for title, desc, sample in EVAS:
    body = {'title': title, 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1}, 'prompt': desc, 'sample': sample}
    r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    d = json.loads(urllib.request.urlopen(r, timeout=30).read().decode())
    print(d['id'], d['folder'], len(sample))
