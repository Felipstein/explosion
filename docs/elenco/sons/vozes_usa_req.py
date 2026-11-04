# (04/11, preparado na noite, NÃO rodado) As vozes das outras unidades da USA, pela receita aprovada (docs/elenco/SONS_UNIDADES.md):
# 1 voz por unidade (`kind: "voice"`, 1 candidata), a pessoa da ficha (a seção "8. O som" de docs/elenco/usa/<chave>.md) em inglês e
# LIMPA (o padrão do jogo: nada de rádio, chiado, eco ou máscara na descrição; o tratamento é do jogo), e a amostra com TODAS as falas
# da ficha, pra recortar as falas da própria amostra aprovada (Game/Tools/voice_cut.py), sem gerar fala por fala. O pedido dele (04/11):
# "amanhã continuemos a gerar os sons das outras unidades e das construções".
#   python docs/elenco/sons/vozes_usa_req.py --dry      (mostra as amostras, não gera)
#   python docs/elenco/sons/vozes_usa_req.py usa_ranger usa_stryker ...   (gera as escolhidas, 1 voz cada)
import json
import os
import re
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
TAIL = ' General American accent. Clean, dry studio recording.'
# a pessoa de cada ficha (o temperamento em português na ficha; aqui em inglês, sem efeito nenhum)
VOICES = {
    'usa_ranger': 'American male US Army Ranger squad leader, mid twenties, young, firm and low voice, disciplined and focused, short clipped '
                  'orders, steady under fire.',
    'usa_javelin': 'American male US Army anti-tank gunner, early thirties, calm and technical, measured delivery with a light dry humor, '
                   'never rushed.',
    'usa_stryker': 'American male US Army Stryker vehicle commander, mid twenties, young, quick and confident, a little humor in his voice, '
                   'energetic but in control.',
    'usa_avenger': 'American male US Army air-defense battery commander, mid twenties, young, fast and aggressive, sharp and alert, always '
                   'watching the sky.',
    'usa_paladin': 'American male US Army artillery section chief, late forties, deep and solemn voice, unhurried and heavy, every word '
                   'deliberate.',
    'usa_tomahawk': 'American male US Navy missile launcher operator, mid thirties, dry and technical, flat precise delivery of someone '
                    'reading numbers and coordinates.',
    'usa_comanche': 'American male US Army attack helicopter gunner, early thirties, low and cold voice, rhythmic and almost bored, calm '
                    'before the kill.',
    'usa_raptor': 'American male US Air Force fighter pilot, mid thirties, calm and assured, slightly ironic, cool professional confidence.',
    'usa_chinook': 'American male US Army transport helicopter pilot, mid forties, deep and easygoing voice, relaxed with a dry humor.',
    'usa_supply_truck': 'American male US Army truck driver, late twenties, upbeat and cheerful, a folksy road-trip drawl, always in a hurry.',
}


def lines_of(key):
    """As falas da seção 8 da ficha: o texto entre aspas de cada grupo (selecionado, andando, atacando, sob fogo, pronto...)."""
    text = open(os.path.join(REPO, 'docs', 'elenco', 'usa', key + '.md'), encoding='utf-8').read()
    sec = text.split('## 8.', 1)[1].split('\n## ', 1)[0]
    talk = sec.split('**As falas**', 1)[1].split('- **O corpo', 1)[0] if '**As falas**' in sec else ''
    return re.findall(r'"([^"]+)"', talk)


def main():
    keys = [a for a in sys.argv[1:] if not a.startswith('--')] or list(VOICES)
    for k in keys:
        ls = lines_of(k)
        sample = ' '.join(l if l[-1] in '.!?' else l + '.' for l in ls)[:1000]
        if '--dry' in sys.argv:
            print(f'{k}: {len(ls)} falas, {len(sample)} caracteres: {sample[:160]}...')
            continue
        body = {'title': f'{k}, a voz', 'session': 'Elenco', 'kind': 'voice', 'models': {'vdesign': 1},
                'prompt': VOICES[k] + TAIL, 'sample': sample}
        r = urllib.request.Request('http://127.0.0.1:8140/api/requests', data=json.dumps(body).encode(),
                                   headers={'Content-Type': 'application/json'})
        d = json.loads(urllib.request.urlopen(r, timeout=60).read().decode())
        print(k, d['id'], d['folder'])


if __name__ == '__main__':
    main()
