# O bloco das tabelas do SRTSOptions::Construct (os itens de cada aba) gerado das tabelas do mockup, sem transcrever à mão.
# Uso: node gen_options_tables.mjs > options_tables.json && python gen_options_tables.py options_tables.json > bloco.inc
#      (o bloco entra no lugar das linhas "{ FItem& I = Add(...) }" do SRTSOptions.cpp)
import json, sys
d = json.load(open(sys.argv[1], encoding='utf-8'))
BS = chr(92)


def q(s):
    return 'TEXT("' + s.replace(BS, BS + BS).replace('"', BS + '"') + '")'


for ti, n in enumerate(['GFX', 'SND', 'CTL', 'GAMEO', 'ACC']):
    print('\t// ' + n)
    for x in d[n]:
        opts = '{ ' + ', '.join(q(o) for o in x['o']) + ' }' if 'o' in x else 'TArray<const TCHAR*>()'
        defv = x['v'] if 'v' in x else x['p'][3]
        st = '\t{ FItem& I = Add(%d, %s, %s, %s, %s, %d, %s);' % (ti, q(x['g']), q(x['k']), q(x['n']), opts, defv, q(x.get('d', '')))
        extra = []
        if 's' in x: extra.append('I.Min = %d; I.Max = %d;' % (x['s'][0], x['s'][1]))
        if 'p' in x: extra.append('I.Preset = { ' + ', '.join(str(v) for v in x['p']) + ' };')
        if x.get('fixed'): extra.append('I.bFixed = true;')
        if 'na' in x: extra.append('I.Na = { ' + ', '.join(str(v) for v in x['na']) + ' };')
        if x.get('w'): extra.append('I.W = %sf;' % float(x['w']))
        if x.get('m'): extra.append('I.M = %sf;' % float(x['m']))
        if x.get('ini'): extra.append('I.Ini = %s;' % q(x['ini']))
        print(st + (' ' + ' '.join(extra) if extra else '') + ' }')
