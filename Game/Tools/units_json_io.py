"""Lê e grava o Game/Config/RTS/Units.json sem atropelar a formatação dele (31/10/2026, o elenco das 3 facções:
docs/ELENCO.md).

O arquivo é escrito à mão e por várias sessões, num estilo que não é regular (tabulação; às vezes a lista de números
numa linha, às vezes uma por linha; "0.10" e "0.1"). Regravar com o json.dumps trocava a formatação inteira (um diff de
milhares de linhas que atropela o trabalho das outras sessões). Aqui a gravação reaproveita o TEXTO de cada chave de cima e
de cada unidade que não mudou (pela chave "key"), e só escreve no estilo do arquivo (tabulação, lista de números numa
linha) o que é novo ou mudou.

Uso:
    from units_json_io import load_units, save_units
    d = load_units()           # o dicionário, na ordem do arquivo
    ...                        # mexer
    save_units(d)              # o texto de antes é a referência da formatação
"""
import json
import os

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Config', 'RTS', 'Units.json')


def _scalar(v):
    return isinstance(v, (int, float, bool)) or v is None


def dumps(v, level=0):
    tab = '\t'
    if isinstance(v, dict):
        if not v:
            return '{}'
        items = [f'{tab * (level + 1)}{json.dumps(k, ensure_ascii=False)}: {dumps(x, level + 1)}' for k, x in v.items()]
        return '{\n' + ',\n'.join(items) + '\n' + tab * level + '}'
    if isinstance(v, list):
        if not v:
            return '[]'
        if all(_scalar(x) for x in v):
            return '[' + ', '.join(json.dumps(x, ensure_ascii=False) for x in v) + ']'
        items = [tab * (level + 1) + dumps(x, level + 1) for x in v]
        return '[\n' + ',\n'.join(items) + '\n' + tab * level + ']'
    return json.dumps(v, ensure_ascii=False)


def _skip_ws(t, i):
    while i < len(t) and t[i] in ' \t\r\n':
        i += 1
    return i


def _value_end(t, i):
    """O fim (exclusivo) do valor JSON que começa em t[i]."""
    c = t[i]
    if c == '"':
        i += 1
        while t[i] != '"':
            i += 2 if t[i] == '\\' else 1
        return i + 1
    if c in '{[':
        depth = 0
        while True:
            c = t[i]
            if c == '"':
                i = _value_end(t, i)
                continue
            if c in '{[':
                depth += 1
            elif c in '}]':
                depth -= 1
                if depth == 0:
                    return i + 1
            i += 1
    while i < len(t) and t[i] not in ',}] \t\r\n':
        i += 1
    return i


def _top_slices(t):
    """{chave de cima: o texto do valor} e, pra "units", a lista dos textos de cada unidade."""
    out, units = {}, []
    i = _skip_ws(t, 0)
    assert t[i] == '{'
    i += 1
    while True:
        i = _skip_ws(t, i)
        if t[i] == '}':
            break
        k_end = _value_end(t, i)
        key = json.loads(t[i:k_end])
        i = _skip_ws(t, k_end)
        assert t[i] == ':'
        i = _skip_ws(t, i + 1)
        v_end = _value_end(t, i)
        out[key] = t[i:v_end]
        if key == 'units':
            j = i + 1
            while True:
                j = _skip_ws(t, j)
                if t[j] == ']':
                    break
                e = _value_end(t, j)
                units.append(t[j:e])
                j = _skip_ws(t, e)
                if t[j] == ',':
                    j += 1
        i = _skip_ws(t, v_end)
        if t[i] == ',':
            i += 1
    return out, units


def load_units(path=PATH):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def render(d, ref_text):
    """O texto novo: o pedaço de antes onde o valor não mudou, o estilo do arquivo onde mudou."""
    ref = json.loads(ref_text)
    top, unit_texts = _top_slices(ref_text)
    ref_units = {u.get('key'): (u, txt) for u, txt in zip(ref.get('units', []), unit_texts)}
    parts = []
    for k, v in d.items():
        if k == 'units':
            items = []
            for u in v:
                old = ref_units.get(u.get('key'))
                items.append('\t\t' + (old[1] if old and old[0] == u else dumps(u, 2)))
            parts.append('\t"units": [\n' + ',\n'.join(items) + '\n\t]')
        elif k in ref and ref[k] == v:
            parts.append(f'\t{json.dumps(k, ensure_ascii=False)}: {top[k]}')
        else:
            parts.append(f'\t{json.dumps(k, ensure_ascii=False)}: {dumps(v, 1)}')
    return '{\n' + ',\n'.join(parts) + '\n}\n'


def save_units(d, path=PATH, ref_text=None):
    if ref_text is None:
        with open(path, encoding='utf-8') as f:
            ref_text = f.read()
    text = render(d, ref_text)
    assert json.loads(text) == d, 'a gravação mudou o conteúdo'
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
