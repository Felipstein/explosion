"""Muda campos de nós no EmberGen já aberto (por mensagem: não mexe no mouse do Felipe) e, se pedido, exporta
o VDB. Base: o desk.py e o eg.py da sessão do BlastLab (C:\\Users\\luisf\\explosion-lab\\tools\\desk) e o
eg_rodada.py dela; a diferença é rolar o Node Details pro topo antes de cada campo (sem isso o campo cai em
outra linha quando o painel ficou rolado de uma leitura anterior).

uso:
  python art/tools/eg_set.py no_x,no_y:aba_x,aba_y,campo_x,campo_y=valor [...] [--export PASTA N STRIDE]
  (coordenadas do Node Details em TELA CHEIA; aba 0,0 = não clicar em aba; nós no grafo normal)
Confere cada campo numa tira em captures/embergen/_set_<x>_<y>.png.
"""
import os
import sys
import time

sys.path.insert(0, r'C:\Users\luisf\explosion-lab\tools\desk')
import desk  # noqa: E402
import eg  # noqa: E402

SHOTS = r'C:\Users\luisf\explosion\captures\embergen'
args = sys.argv[1:]
export = None
if '--export' in args:
    i = args.index('--export')
    export = args[i + 1:i + 4]
    args = args[:i]


def cheia():
    desk.click(eg.H, 1685, 469); time.sleep(1)
    if eg.layout() != 'detalhes':
        desk.click(eg.H, 1685, 469); time.sleep(1)


def topo():
    for _ in range(4):
        desk.scroll(eg.H, 700, 500, 10); time.sleep(0.3)
    time.sleep(0.4)


for m in args:
    alvo, val = m.split('=')
    no, campo = alvo.split(':')
    nx, ny = map(int, no.split(','))
    ax, ay, fx, fy = map(int, campo.split(','))
    eg.normal(); eg.no(nx, ny); cheia()
    if ax > 0:
        eg.aba(ax, ay); time.sleep(0.6)
    topo()
    ok = eg.set_(fx, fy, val)
    eg.foto().crop((0, fy - 14, 700, fy + 14)).save(os.path.join(SHOTS, '_set_%d_%d.png' % (fx, fy)))
    print('campo', m, 'ok' if ok else 'FALHOU')

if export:
    pasta, nfr, stride = export
    os.makedirs(pasta, exist_ok=True)
    eg.normal(); eg.no(1060, 445); cheia()
    eg.aba(104, 152); time.sleep(0.5)
    topo()
    print('pasta', eg.set_(450, 216, pasta), 'n', eg.set_(385, 352, nfr), 'stride', eg.set_(385, 382, stride))
    for p in os.listdir(pasta):
        if p.endswith('.vdb'):
            os.remove(os.path.join(pasta, p))
    t0 = time.time()
    desk.click(eg.H, 321, 276); time.sleep(8)
    if not any(p.endswith('.vdb') for p in os.listdir(pasta)):
        desk.click(eg.H, 321, 276); time.sleep(5)
    while sum(p.endswith('.vdb') for p in os.listdir(pasta)) < int(nfr) and time.time() - t0 < 900:
        time.sleep(5)
    time.sleep(3)
    print('arquivos', sum(p.endswith('.vdb') for p in os.listdir(pasta)), 'em', round(time.time() - t0), 's')
eg.normal()
