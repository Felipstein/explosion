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

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
PROTO = os.path.join(ROOT, 'docs', 'concept', 'hud', 'prototipo', 'index.html')
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'

def build(faction):
    src = open(PROTO, encoding='utf-8').read()
    tree = open(os.path.join(HERE, f'{faction}.js'), encoding='utf-8').read()
    a = src.index('const ROWS = [['); b = src.index('const HAVE = {};')
    out = src[:a] + tree.rstrip() + '\n' + src[b:]
    out = out.replace("['drone', 'para1', 'arty1'].forEach(k => HAVE[k] = 1)", "[].forEach(k => HAVE[k] = 1)")
    out = re.sub(r'<title>.*?</title>', f'<title>Poderes: {faction.upper()}</title>', out, count=1)
    out = out.replace('<head>', '<head>\n<base href="../../concept/hud/prototipo/">', 1)
    hook = "if (q.get('gen')) setTimeout(genOpen, 400);"
    assert hook in out, 'o protótipo mudou: achar de novo onde a tela do general abre'
    out = out.replace(hook, hook + """
// (poderes) ?have=id,id (o que já foi comprado) e ?sel=id (o poder no detalhe), pra foto
if (q.get('have') || q.get('sel')) setTimeout(() => { (q.get('have') || '').split(',').filter(Boolean).forEach(k => HAVE[k] = 1); selId = q.get('sel') || null; hubPts(); genRender(); }, 700);""", 1)
    dst = os.path.join(HERE, f'poderes_{faction}.html')
    open(dst, 'w', encoding='utf-8').write(out)
    return dst

def shot(page, rank, have, sel):
    png = os.path.splitext(page)[0] + '.png'
    url = 'file:///' + page.replace('\\', '/') + f'?rk={rank}&gen=1&spy=0&have={have}&sel={sel}'
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
    page = build(f); print(os.path.relpath(page, ROOT))
    if '--shot' in sys.argv:
        i = sys.argv.index('--shot')
        rank, have, sel = (sys.argv[i + 1:i + 4] + ['3', '', ''])[:3]
        print(os.path.relpath(shot(page, rank, have, sel), ROOT))
