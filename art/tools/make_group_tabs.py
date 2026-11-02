"""As abas dos grupos (1 a 0) desenhadas pro tamanho delas, em vetor, por facção (01/11, noite).

A arte das abas (`skin/<facção>/group_on|off.png`, 229×102) é larga e cheia de detalhe fino; encolhida numa casa de 48×46 virava borrão
(ele, da GLA: "ficou muito pequeno de detalhes [...] ruim de ver"). Aqui cada aba é um SVG de 48×46 no material da facção: o aro de metal
em degradê, a face, o fio de luz por dentro (aceso = o grupo existe) e os enfeites da facção em ponto grande (os rebites da GLA, os
parafusos da Rússia, as cantoneiras douradas da China), nítidos em qualquer escala.

Uso: python art/tools/make_group_tabs.py   (grava captures/concept/hud/recursos2/a/[<facção>/]tab_on.svg e tab_off.svg)
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'captures/concept/hud/recursos2/a')

FAC = {
    'usa':    dict(rim=('#7d8b99', '#2b343e', '#56626e'), face='#0b1520', on='#49aaff', off='#3a4652', deco='none'),
    'china':  dict(rim=('#3a3434', '#0d0b0b', '#2a2525'), face='#0d0b0b', on='#f04832', off='#5e2018', deco='gold'),
    'gla':    dict(rim=('#c08a4c', '#5e3f1f', '#9a6c38'), face='#2c2f1d', on='#ffd27a', off='#75603a', deco='rivets'),
    'russia': dict(rim=('#8b949d', '#2b3036', '#646d76'), face='#15191d', on='#e6f0ff', off='#4c545c', deco='bolts'),
}


def chamfer(x0, y0, x1, y1, c):
    return f'M{x0+c},{y0} L{x1-c},{y0} L{x1},{y0+c} L{x1},{y1-c} L{x1-c},{y1} L{x0+c},{y1} L{x0},{y1-c} L{x0},{y0+c} Z'


def tab(p, lit):
    line = p['on'] if lit else p['off']
    glow = (f'<path d="{chamfer(4.5, 4.5, 43.5, 41.5, 5)}" fill="none" stroke="{p["on"]}" stroke-width="3.2" opacity=".45" filter="url(#b)"/>'
            if lit else '')
    deco = ''
    if p['deco'] == 'rivets':
        for x, y in ((6.2, 6.2), (41.8, 6.2), (6.2, 39.8), (41.8, 39.8)):
            deco += (f'<circle cx="{x}" cy="{y+0.5}" r="2" fill="#000" opacity=".55"/>'
                     f'<circle cx="{x}" cy="{y}" r="1.7" fill="url(#rv)"/>')
    elif p['deco'] == 'bolts':
        for x, y in ((6.4, 6.4), (41.6, 6.4), (6.4, 39.6), (41.6, 39.6)):
            deco += (f'<polygon points="{x-1.6},{y} {x-0.8},{y-1.4} {x+0.8},{y-1.4} {x+1.6},{y} {x+0.8},{y+1.4} {x-0.8},{y+1.4}" '
                     f'fill="#aab3bc" stroke="#20252a" stroke-width=".5"/>')
    elif p['deco'] == 'gold':
        for x0, y0, x1, y1 in ((1.2, 6.2, 6.2, 1.2), (41.8, 1.2, 46.8, 6.2), (1.2, 39.8, 6.2, 44.8), (41.8, 44.8, 46.8, 39.8)):
            deco += f'<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y1}" stroke="#e2bd62" stroke-width="1.6" stroke-linecap="round"/>'
    r0, r1, r2 = p['rim']
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 46" preserveAspectRatio="none">
<defs>
<linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{r0}"/><stop offset=".55" stop-color="{r1}"/><stop offset="1" stop-color="{r2}"/></linearGradient>
<linearGradient id="f" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".07"/><stop offset=".5" stop-color="#fff" stop-opacity="0"/></linearGradient>
<radialGradient id="rv" cx=".35" cy=".35" r=".7"><stop offset="0" stop-color="#ffe2a8"/><stop offset=".6" stop-color="#b07a3c"/><stop offset="1" stop-color="#4a3018"/></radialGradient>
<filter id="b" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="1.4"/></filter>
</defs>
<path d="{chamfer(0.5, 0.5, 47.5, 45.5, 7)}" fill="#000" opacity=".7"/>
<path d="{chamfer(1, 1, 47, 45, 6.6)}" fill="url(#g)"/>
<path d="{chamfer(1.6, 1.6, 46.4, 44.4, 6.2)}" fill="none" stroke="#fff" stroke-opacity=".18" stroke-width=".8"/>
<path d="{chamfer(3.6, 3.6, 44.4, 42.4, 5)}" fill="#000" opacity=".8"/>
<path d="{chamfer(4.2, 4.2, 43.8, 41.8, 4.6)}" fill="{p["face"]}"/>
<path d="{chamfer(4.2, 4.2, 43.8, 41.8, 4.6)}" fill="url(#f)"/>
{glow}<path d="{chamfer(4.8, 4.8, 43.2, 41.2, 4.3)}" fill="none" stroke="{line}" stroke-width="{1.3 if lit else 1}" opacity="{1 if lit else .8}"/>
{deco}
</svg>'''


if __name__ == '__main__':
    for fac, p in FAC.items():
        d = OUT if fac == 'usa' else os.path.join(OUT, fac)
        for lit, name in ((True, 'tab_on.svg'), (False, 'tab_off.svg')):
            open(os.path.join(d, name), 'w', encoding='utf-8').write(tab(p, lit))
        print(fac)
