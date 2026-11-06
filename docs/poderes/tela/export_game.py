"""A árvore de poderes de cada facção (as que a sessão das superarmas monta em docs/poderes/tela/<facção>.js) vira o arquivo que o HUD
em C++ e o jogo leem: Game/Config/RTS/GeneralTrees.json (04/11, a tela do general no jogo).

Os .js continuam sendo a fonte: mudou a árvore, roda de novo. O ícone de cada poder segue a mesma regra do make_tela.py:
<facção>_<poder>_<sufixo>.png em docs/poderes/tela/icones (o nome inteiro primeiro, depois sem o número do nível: a10_1 -> a10), na
ordem esc (o escolhido por ele) > gemini > gpt; sem nenhum, o provisório que o .js aponta. "approved" diz se todos são os escolhidos.

Uso: python docs/poderes/tela/export_game.py
"""
import ast
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.abspath(os.path.join(HERE, '..', '..'))
ROOT = os.path.dirname(DOCS)
OUT = os.path.join(ROOT, 'Game', 'Config', 'RTS', 'GeneralTrees.json')
ICONS = os.path.join(HERE, 'icones')

# as patentes de cada exército (a sigla e o nome), as do protótipo aprovado do HUD (docs/concept/hud/prototipo/index.html, RANKS_BY)
RANKS = {
    'usa': [['BG', 'Brigadier General'], ['MG', 'Major General'], ['LTG', 'Lieutenant General'], ['GEN', 'General'], ['GA', 'General of the Army']],
    'china': [['SHJ', 'Shao Jiang'], ['ZHJ', 'Zhong Jiang'], ['SGJ', 'Shang Jiang'], ['DJ', 'Da Jiang'], ['YS', 'Yuan Shuai']],
    'russia': [['GM', 'General-Mayor'], ['GL', 'General-Leytenant'], ['GP', 'General-Polkovnik'], ['GA', 'General Armii'], ['MSL', 'Marshal']],
    'gla': [['CL', 'Cell Leader'], ['FC', 'Field Commander'], ['WL', 'Warlord'], ['HW', 'High Warlord'], ['SW', 'Supreme Warlord']],
}
# os pontos que cada patente dá (o PTS_PER_RANK do protótipo: 9 no total, um terço da árvore)
POINTS_PER_RANK = [1, 1, 2, 2, 3]


def strip_comments(src):
    """Tira os // fora das strings (as descrições têm aspas e barras)."""
    out, i, q = [], 0, None
    while i < len(src):
        c = src[i]
        if q:
            out.append(c)
            if c == '\\' and i + 1 < len(src):
                out.append(src[i + 1]); i += 2; continue
            if c == q:
                q = None
        elif c in '\'"':
            q = c; out.append(c)
        elif src.startswith('//', i):
            while i < len(src) and src[i] != '\n':
                i += 1
            continue
        else:
            out.append(c)
        i += 1
    return ''.join(out)


def js_array(src, name):
    i = src.index(f'const {name}')
    i = src.index('[', i)
    depth, q, j = 0, None, i
    while j < len(src):
        c = src[j]
        if q:
            if c == '\\':
                j += 2; continue
            if c == q:
                q = None
        elif c in '\'"':
            q = c
        elif c in '[{':
            depth += 1
        elif c in ']}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    lit = src[i:j + 1]
    lit = re.sub(r'([{,]\s*)([A-Za-z_]\w*)\s*:', r"\1'\2':", lit)
    lit = re.sub(r'\btrue\b', 'True', re.sub(r'\bfalse\b', 'False', lit))
    return ast.literal_eval(lit)


def icon_for(fac, node):
    nid = node['id']
    for suffix in ('esc', 'gemini', 'gpt'):
        for cand in (nid, re.sub(r'_?\d+$', '', nid)):
            if os.path.exists(os.path.join(ICONS, f'{fac}_{cand}_{suffix}.png')):
                return f'doc:poderes/tela/icones/{fac}_{cand}_{suffix}', suffix == 'esc'
    ic = node.get('ic', '')
    if ic.startswith('../../../../'):
        return 'doc:' + ic[len('../../../../'):], False      # (a partir de docs/concept/hud/prototipo/a: 4 níveis = docs/)
    return f'doc:concept/hud/prototipo/a/{ic}', False


def main():
    out = {'_about': 'Gerado por docs/poderes/tela/export_game.py a partir de docs/poderes/tela/<facção>.js. Não editar à mão.',
           'points_per_rank': POINTS_PER_RANK, 'factions': {}}
    for fac in ('usa', 'china', 'gla', 'russia'):
        path = os.path.join(HERE, f'{fac}.js')
        if not os.path.exists(path):
            continue
        src = strip_comments(open(path, encoding='utf-8').read())
        rows = js_array(src, 'ROWS')
        nodes = js_array(src, 'NODES')
        all_esc = True
        out_nodes = []
        for n in nodes:
            ic, esc = icon_for(fac, n)
            all_esc &= esc
            out_nodes.append({'id': n['id'], 'name': n['n'], 'level': n.get('lv', ''), 'rank': int(n['rank']), 'row': int(n['row']),
                              'cost': int(n['cost']), 'cooldown': n.get('cd', ''), 'desc': n.get('d', ''), 'req': n.get('req', ''),
                              'type': n.get('t', ''), 'icon': ic})
        out['factions'][fac] = {'approved_icons': all_esc, 'rows': max(len(rows), 1 + max(n['row'] for n in out_nodes)),
                                'ranks': RANKS[fac], 'nodes': out_nodes}
        print(fac, len(out_nodes), 'poderes,', out['factions'][fac]['rows'], 'linhas,', 'ícones escolhidos' if all_esc else 'ícones ainda não escolhidos')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(os.path.relpath(OUT, ROOT))


if __name__ == '__main__':
    main()
