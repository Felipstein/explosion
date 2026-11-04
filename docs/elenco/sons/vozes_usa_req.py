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
TAIL = ' Clean, dry studio recording.'
# a pessoa de cada ficha (o temperamento em português na ficha; aqui em inglês, sem efeito nenhum)
# (04/11, a 2ª volta) "o Stryker já começou a ficar muito repetido e parecido, sem variação na voz, tudo parecendo igual [...] presta
# atenção no que eu falei" (o que ele pediu na GLA: "cada um uma voz um pouco diferente, com diferentes pesos, sentindo peso nas frases").
# O elenco da USA, lado a lado, pra nenhuma voz cair em cima da outra (idade, timbre, sotaque, energia), e cada voz com a VARIAÇÃO
# de tom entre as falas (calma ao ser selecionada, firme andando, forte atacando, tensa sob fogo). Já aprovados: o M1 (fim dos 30,
# médio, firme, americano neutro), o Dozer (40 e tantos, rouco, operário), o Ranger (20 e poucos, grave baixo, disciplinado), o Javelin
# (30, calmo, técnico, humor seco); a EVA (mulher, operadora). A ficha de cada um (docs/elenco/usa/) dá o temperamento; o resto é contraste.
RANGE = (' His delivery changes a lot with each line: relaxed and conversational when acknowledging, focused when moving, punchy and loud '
         'when attacking, strained and urgent under fire, proud when reporting ready.')
VOICES = {
    'usa_stryker': 'American male US Army Stryker vehicle commander, mid twenties, bright quick tenor, fast-talking and cocky with a grin in '
                   'his voice, wisecracking, New York Bronx accent.' + RANGE,
    'usa_avenger': ('American female US Army air-defense battery commander, late twenties, sharp clear alto, fast, aggressive and '
                    'hyper-alert, always scanning the sky, clipped and intense, Midwestern accent. Her delivery changes a lot with each '
                    'line: crisp when acknowledging, quick when moving, fierce when engaging, urgent under fire.'),
    'usa_paladin': 'American male US Army artillery section chief, mid fifties, very deep gravelly bass, slow, solemn and heavy, every word '
                   'deliberate, like a sermon, soft Texas drawl.' + RANGE,
    'usa_tomahawk': 'American male US Navy missile fire-control technician, late thirties, flat dry nasal voice, precise and mechanical, '
                    'reads numbers and grids like a machine, barely any emotion except a cold satisfaction on launch, Boston accent.' + RANGE,
    'usa_comanche': 'American male US Army attack helicopter gunner, early thirties, low cold voice, quiet and almost whispered, laconic '
                    'and menacing, slow Louisiana drawl, bored calm right before the kill.' + RANGE,
    'usa_raptor': 'American male US Air Force fighter pilot, mid thirties, smooth charismatic baritone, cocky and playful, cool Californian '
                  'swagger, a confident half-laugh in his voice.' + RANGE,
    'usa_chinook': 'American male US Army heavy-lift helicopter pilot, mid fifties, warm husky voice, easygoing and unhurried, dry folksy '
                   'humor, Kentucky drawl.' + RANGE,
    'usa_supply_truck': 'American male US Army truck driver, early twenties, high energetic voice, cheerful, chatty and always in a hurry, '
                        'thick Georgia Southern accent.' + RANGE,
    'usa_ranger': 'American male US Army Ranger squad leader, mid twenties, young, firm and low voice, disciplined and focused, short '
                  'clipped orders, steady under fire. General American accent.',
    'usa_javelin': 'American male US Army anti-tank gunner, early thirties, calm and technical, measured delivery with a light dry humor, '
                   'never rushed. General American accent.',
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
        # as reticências entre as falas: a pausa clara pro recorte e a fala seguinte começa de novo (o tom não emenda)
        sample = ' ... '.join(l if l[-1] in '.!?' else l + '.' for l in ls)[:1000]
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
