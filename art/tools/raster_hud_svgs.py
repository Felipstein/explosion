"""As peças em vetor do protótipo do HUD (docs/concept/hud/prototipo/a/) viram PNG no kit, pro HUD em C++ (o Canvas não desenha SVG).

Renderiza pelo Chrome sem janela, com fundo transparente, em Escala × o tamanho da peça (nítido no 4K), e grava em
docs/concept/hud/kit/skin/[<facção>/]<nome>.png. Hoje: as abas dos grupos (tab_on, tab_off; art/tools/make_group_tabs.py).

Uso: python art/tools/raster_hud_svgs.py
"""
import os
import subprocess
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(ROOT, 'docs/concept/hud/prototipo/a')
KIT = os.path.join(ROOT, 'docs/concept/hud/kit/skin')
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
SCALE = 4
# a peça, o tamanho em que o protótipo desenha (px da planta)
PIECES = {'tab_on': (48, 46), 'tab_off': (48, 46)}


def render(svg, w, h, out):
    html = ('<!doctype html><html><head><style>html,body{margin:0;background:transparent;overflow:hidden}'
            f'img{{display:block;width:{w}px;height:{h}px}}</style></head><body><img src="file:///{svg.replace(os.sep, "/")}"></body></html>')
    with tempfile.NamedTemporaryFile('w', suffix='.html', delete=False, encoding='utf-8') as f:
        f.write(html)
        page = f.name
    subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--default-background-color=00000000',
                    '--allow-file-access-from-files', f'--window-size={w},{h}', f'--screenshot={out}', 'file:///' + page.replace(os.sep, '/')],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    os.remove(page)


if __name__ == '__main__':
    for fac in ('', 'china', 'gla', 'russia'):
        for name, (w, h) in PIECES.items():
            svg = os.path.join(SRC, fac, name + '.svg')
            if not os.path.exists(svg):
                continue
            out = os.path.join(KIT, fac, name + '.png')
            render(svg, w * SCALE, h * SCALE, out)
            print(out, os.path.exists(out))
