# A tela de poderes de uma facção, montada a partir do protótipo aprovado do HUD (docs/concept/hud/prototipo/index.html) com a
# árvore da facção (docs/poderes/tela/<facção>.js). O protótipo não é tocado: a página nova aponta pra ele pelo <base>.
#
#   python docs/poderes/tela/make_tela.py usa              -> docs/poderes/tela/poderes_usa.html
#   python docs/poderes/tela/make_tela.py usa --shot 3 drone,para1,para2 a10_1
#       e tira a foto 1920x1080 no Chrome sem janela: patente 3, os poderes já comprados e o poder no detalhe
#       -> docs/poderes/tela/poderes_usa.jpg
#
# Na página: ?rk=<patente>&gen=1 abre a tela; ?have=id,id marca o que já foi comprado; ?sel=id mostra o detalhe.
import os, re, subprocess, sys
from urllib.parse import quote

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
PROTO = os.path.join(ROOT, 'docs', 'concept', 'hud', 'prototipo', 'index.html')
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'

# (03/11) as famílias do ícone (docs/poderes/ICONES.md §3.1): comando = o botão em relevo; passivo = a placa chapada, sem relevo
# e sem sombra; desbloqueio = a foto do botão de construir, com o filete de aço. O subtipo do comando (golpe, olhos, tropas, apoio)
# vem da própria imagem, sem marca. (O padrão anterior, a moldura com forma e o selo colorido por tipo, foi reprovado em 03/11.)
FAMILY_CSS = '''
.iw { position: relative; display: inline-block; flex: none; line-height: 0; }
.iw img { display: block; }
.f-cmd::after { content: ''; position: absolute; inset: 0; border-radius: 4px; pointer-events: none;
  box-shadow: inset 2px 2px 0 rgba(255,255,255,.38), inset -2px -2px 0 rgba(0,0,0,.65), inset 0 0 0 1px rgba(0,0,0,.8); }
.f-cmd img { border-radius: 4px; }
.f-pas img { border-radius: 1px; box-shadow: none !important; }
.f-unl img { border-radius: 2px; box-shadow: 0 0 0 1px #9aa6b2, 0 2px 4px rgba(0,0,0,.6) !important; }
#pcolx { position: absolute; left: 0; top: 0; pointer-events: none; z-index: 5; }
#pcolx .iw { position: absolute; }
#pcolx .iw img { width: 100%; height: 100%; object-fit: cover; }
'''
FAM_JS = "${(n => n.t === 'pas' ? 'pas' : n.t === 'unl' ? 'unl' : 'cmd')(%s)}"

# (03/11, em teste) a tela calma: "meus olhos ainda cansam ao ver essa tela". O estado aparece uma vez só (a ficha do custo ou o ✓), sem
# moldura acesa em todo cartão (18 retângulos de borda forte em grade fazem a vista vibrar); o dourado só no cartão do mouse ou
# selecionado; o nível sem a plaquinha colorida; as linhas entre níveis finas e cinza, sem o tracejado dourado.
CALM_CSS = '''
#gen .trk .pw, #gen .trk .pw.buy, #gen .trk .pw.own, #gen .trk .pw.poor, #gen .trk .pw.req { box-shadow: inset 0 0 0 1px #26313d !important;
  background: #0f161e !important; animation: none !important; }
#gen .trk .pw.buy:hover, #gen .trk .pw.sel { box-shadow: inset 0 0 0 2px #f6c850 !important; background: #142130 !important; }
#gen .trk .pw.own .nm { color: #e6ecf3; }
#gen .pw .nm .lvi, #gen .pw.own .nm .lvi { background: none !important; box-shadow: none !important; color: #8c9aa8 !important; padding: 0 2px; }
#gen .trk svg.lines line { stroke: #34414f !important; stroke-dasharray: none !important; stroke-width: 2px !important; }
#gen .trk { gap: 9px !important; }
'''

def build(faction, style=None, column=None, icons=None, tree_name=None, calm=False, layout=None):
    src = open(PROTO, encoding='utf-8').read()
    tree = open(os.path.join(HERE, f'{tree_name or faction}.js'), encoding='utf-8').read()
    a = src.index('const ROWS = [['); b = src.index('const HAVE = {};')
    out = src[:a] + tree.rstrip() + '\n' + src[b:]
    out = out.replace("['drone', 'para1', 'arty1'].forEach(k => HAVE[k] = 1)", "[].forEach(k => HAVE[k] = 1)")
    out = re.sub(r'<title>.*?</title>', f'<title>Poderes: {faction.upper()}</title>', out, count=1)
    out = out.replace('<head>', '<head>\n<base href="../../concept/hud/prototipo/">', 1)
    # a família do ícone: o invólucro com a classe (comando, passivo, desbloqueio), na grade e no detalhe
    def wrap(v):
        return '<span class="iw f-' + FAM_JS % v + '"><img src="a/${' + v + '.ic}.png" alt=""></span>'
    for a_, b_ in (('${corner}<img src="a/${n.ic}.png" alt="">', '${corner}' + wrap('n')),
                   ('<div class="top"><img src="a/${n.ic}.png" alt="">', '<div class="top">' + wrap('n'))):
        assert a_ in out, 'o protótipo mudou: ' + a_
        out = out.replace(a_, b_, 1)
    out = out.replace('</head>', '<style>' + FAMILY_CSS + '</style>\n</head>', 1)
    if calm:
        out = out.replace('</head>', '<style>' + CALM_CSS + '</style>\n</head>', 1)
    # (03/11, em teste) os dois jeitos de dar direção à tela: a escada (escada.js/.css) e a linha do tempo com a fronteira
    if layout == 'escada':
        out = out.replace('</head>', '<style>' + open(os.path.join(HERE, 'escada.css'), encoding='utf-8').read() + '</style>\n</head>', 1)
        out = out.replace("if (q.get('gen')) setTimeout(genOpen, 400);",
                          open(os.path.join(HERE, 'escada.js'), encoding='utf-8').read() + "\nif (q.get('gen')) setTimeout(genOpen, 400);", 1)
    elif layout == 'linha':
        a_ = "h += `<div class=\"cell ${r > lv ? 'fut' : ''}\">"
        if a_ in out: out = out.replace(a_, "h += `<div class=\"cell ${r > lv ? 'fut' : r === lv ? 'cur' : ''}\">", 1)
        out = out.replace('</head>', '<style>#gen .trk .cell.fut { opacity: .38; } #gen .trk .rh.fut { opacity: .38; }'
                          ' #gen .trk .cell.cur { background: rgba(246,200,80,.05); border-radius: 4px; }'
                          ' #gen .trk .rh.cur { box-shadow: inset 0 -4px 0 #f6c850, inset 0 0 0 1px #4a4027 !important; background: #18160f; }'
                          '</style>\n</head>', 1)
    # as linhas sem rótulo (decisão dele em 03/11: o tipo se lê no ícone): some a coluna dos rótulos, a grade fica com as 5 patentes
    # e tantas linhas quanto a árvore tiver
    rows = re.search(r"const ROWS = \[(.*?)\];", tree, re.S).group(1)
    if "['', '']" in rows and not re.search(r"\['[^']+'", rows):
        lanes = rows.count('[')
        out = out.replace('</head>', '<style>#gen .trk { grid-template-columns: repeat(5, 1fr); grid-template-rows: 58px '
                          f'repeat({lanes}, 1fr); }} #gen .trk .rl, #gen .trk > svg + div {{ display: none; }}'
                          ' #gen .trk .pw { max-width: 190px; }</style>\n</head>', 1)
    # o estilo de ícone em teste: troca o ícone dos 3 poderes da folha de estilos (A-10, AWACS, Naval)
    if style:
        est = '../../../../poderes/tela/estilos/' + style
        for ids, pw in ((('a10_1', 'a10_2'), 'a10'), (('awacs',), 'awacs'), (('naval1', 'naval2', 'naval3'), 'naval')):
            for i in ids:
                n0 = out.index('const NODES')
                out = out[:n0] + re.sub(r"(\{ id: '%s',[^\n]*? ic: ')[^']*'" % i, lambda m: m.group(1) + f"{est}_{pw}'", out[n0:], count=1)
    if icons:
        ico = os.path.join(HERE, 'icones')
        for i, pw in (('drone', 'drone'), ('desig', 'desig'), ('a10_1', 'a10'), ('a10_2', 'a10'), ('ac130', 'ac130'), ('naval1', 'naval'),
                      ('naval2', 'naval'), ('naval3', 'naval'), ('moab', 'moab'), ('para1', 'para'), ('para2', 'para'), ('para3', 'para'),
                      ('seal', 'seal'), ('fob', 'fob'), ('repair1', 'repair'), ('repair2', 'repair'), ('angel', 'angel'), ('awacs', 'awacs')):
            if os.path.exists(os.path.join(ico, f'usa_{pw}_{icons}.png')):
                n0 = out.index('const NODES')    # só na árvore: o protótipo tem outra lista com os mesmos ids (o MENU antigo)
                out = out[:n0] + re.sub(r"(\{ id: '%s',[^\n]*? ic: ')[^']*'" % i, lambda m: m.group(1) + f"../../../../poderes/tela/icones/usa_{pw}_{icons}'", out[n0:], count=1)
    hook = "if (q.get('gen')) setTimeout(genOpen, 400);"
    # a coluna de poderes do HUD (a da esquerda, desenhada no fundo): o ícone por cima de cada casa; o relógio embaixo continua
    if column:
        # cada casa é o id de um poder da árvore, ou "img:<caminho a partir de docs/poderes/tela>" (ex.: o ícone da superarma)
        def src(nid):
            return f'../../../poderes/tela/{nid[4:]}' if nid.startswith('img:') else f'a/${{node(\'{nid}\').ic}}.png'
        slots = ''.join(f'<span class="iw f-cmd" style="left:37px;top:{459 + 70 * k}px;width:56px;height:40px">'
                        f'<img src="{src(nid)}" alt=""></span>'
                        for k, nid in enumerate(column))
        out = out.replace(hook, "{ const pc = document.createElement('div'); pc.id = 'pcolx'; pc.innerHTML = `" + slots
                          + "`; $('#stage').appendChild(pc); }\n" + hook, 1)
    assert hook in out, 'o protótipo mudou: achar de novo onde a tela do general abre'
    out = out.replace(hook, hook + """
// (poderes) ?have=id,id (o que já foi comprado) e ?sel=id (o poder no detalhe), pra foto
if (q.get('have') || q.get('sel')) setTimeout(() => { (q.get('have') || '').split(',').filter(Boolean).forEach(k => HAVE[k] = 1); selId = q.get('sel') || null; hubPts(); genRender(); }, 700);""", 1)
    tag = style or (icons and 'icones_' + icons)
    if tree_name: tag = (tag + '_' if tag else '') + tree_name
    if calm: tag = (tag + '_' if tag else '') + 'calmo'
    if layout: tag = (tag + '_' if tag else '') + layout
    dst = os.path.join(HERE, f'poderes_{faction}{"_" + tag if tag else ""}.html')
    open(dst, 'w', encoding='utf-8').write(out)
    return dst

def shot(page, rank, have, sel, gen=True):
    png = os.path.splitext(page)[0] + '.png'
    url = 'file:///' + page.replace('\\', '/') + f'?rk={rank}{"&gen=1" if gen else ""}&spy=0&have={have}&sel={sel}'
    prof = os.path.join(os.environ.get('TEMP', HERE), 'poderes_chrome_shot')
    subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--allow-file-access-from-files',
                    '--window-size=1920,1080', '--virtual-time-budget=4000', f'--user-data-dir={prof}',
                    f'--screenshot={png}', url], check=False, capture_output=True)
    from PIL import Image                     # o jpg leve fica; o png de 1,3 MB sai
    jpg = os.path.splitext(png)[0] + '.jpg'
    Image.open(png).convert('RGB').save(jpg, quality=88); os.remove(png)
    return jpg

if __name__ == '__main__':
    f = sys.argv[1]
    style = sys.argv[sys.argv.index('--style') + 1] if '--style' in sys.argv else None
    column = sys.argv[sys.argv.index('--column') + 1].split(',') if '--column' in sys.argv else None
    icons = sys.argv[sys.argv.index('--icons') + 1] if '--icons' in sys.argv else None
    tree_name = sys.argv[sys.argv.index('--tree') + 1] if '--tree' in sys.argv else None
    layout = sys.argv[sys.argv.index('--layout') + 1] if '--layout' in sys.argv else None
    page = build(f, style, column, icons, tree_name, '--calmo' in sys.argv, layout); print(os.path.relpath(page, ROOT))
    if '--shot' in sys.argv:
        i = sys.argv.index('--shot')
        rank, have, sel = (sys.argv[i + 1:i + 4] + ['3', '', ''])[:3]
        print(os.path.relpath(shot(page, rank, have, sel, '--nogen' not in sys.argv), ROOT))
