# Põe no índice do git SÓ as minhas mudanças nos arquivos compartilhados: a versão do HEAD + os ganchos do apply_shared.py
# (as mudanças das outras sessões, ainda sem commit, ficam de fora). "check" só confere.
import os, subprocess, sys, importlib.util
ST = os.path.dirname(os.path.abspath(__file__))
REPO = r'C:\Users\luisf\explosion'
mode = sys.argv[1] if len(sys.argv) > 1 else 'check'
sys.argv = ['x', 'none']
spec = importlib.util.spec_from_file_location('ap', os.path.join(ST, 'apply_shared.py')); ap = importlib.util.module_from_spec(spec); spec.loader.exec_module(ap)
for rel, pairs in ap.PATCHES.items():
    path = 'Game/' + rel.replace('\\', '/')
    raw = subprocess.run(['git', 'show', 'HEAD:' + path], cwd=REPO, capture_output=True).stdout
    bom = raw.startswith(b'\xef\xbb\xbf'); crlf = b'\r\n' in raw
    s = raw.decode('utf-8-sig').replace('\r\n', '\n')
    bad = [a[:50] for a, b in pairs if s.count(a) != 1]
    if bad:
        print('NO HEAD FALTA:', path, bad); continue
    for a, b in pairs:
        s = s.replace(a, b)
    out = (b'\xef\xbb\xbf' if bom else b'') + (s.replace('\n', '\r\n') if crlf else s).encode('utf-8')
    if mode == 'stage':
        h = subprocess.run(['git', 'hash-object', '-w', '--stdin'], cwd=REPO, input=out, capture_output=True).stdout.decode().strip()
        subprocess.run(['git', 'update-index', '--cacheinfo', '100644,' + h + ',' + path], cwd=REPO, check=True)
        print('no índice:', path)
    else:
        print('ok:', path)
