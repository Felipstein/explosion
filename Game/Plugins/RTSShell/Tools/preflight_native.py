# Confere à parte (cl.exe com as opções da última compilação do plugin RTSShell, o .obj no scratchpad) os arquivos novos do menu
# nativo, antes de pôr no jogo. Nunca rodar durante uma compilação do jogo.
import os, re, subprocess, sys
NS = os.path.dirname(os.path.abspath(__file__))
P = r'C:\Users\luisf\explosion\Game\Plugins\RTSShell'
RSP = os.path.join(P, r'Intermediate\Build\Win64\x64\UnrealEditor\Development\RTSShell\RTSShellUI.cpp.obj.rsp')
CL = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\HostX64\x64\cl.exe'
UE = r'C:\Program Files\Epic Games\UE_5.8\Engine'
OUT = os.path.join(NS, 'obj'); os.makedirs(OUT, exist_ok=True)
fw = lambda p: p.replace(os.sep, '/')
files = sys.argv[1:] or ['SRTSMenu.cpp', 'RTSShell.cpp']
lines = open(RSP, encoding='utf-8-sig').read().splitlines()
fails = 0
for f in files:
    src = os.path.join(NS, f)
    new = ['"' + fw(src) + '"']
    for l in lines[1:]:
        if l.startswith('/Fo'):
            new.append('/Fo"' + fw(os.path.join(OUT, f + '.obj')) + '"'); continue
        if l.startswith('/experimental:log') or l.startswith('/sourceDependencies'):
            continue
        new.append(l)
    new += ['/I"' + fw(NS) + '"', '/I"' + fw(os.path.join(P, r'Source\RTSShell\Private')) + '"', '/I"' + fw(os.path.join(UE, r'Source\Runtime\Json\Public')) + '"',
            '/I"' + fw(os.path.join(UE, r'Source\Runtime\RHI\Public')) + '"']
    rp = os.path.join(OUT, f + '.rsp'); open(rp, 'w', encoding='utf-8').write('\n'.join(new))
    r = subprocess.run([CL, '@' + rp], cwd=os.path.join(UE, 'Source'), capture_output=True, text=True, encoding='utf-8', errors='replace')
    print(f, 'código', r.returncode)
    for l in (r.stdout + r.stderr).splitlines():
        if re.search(r'error|warning C', l):
            print('  ' + l[-260:])
    fails += r.returncode != 0
print('falhas:', fails)
sys.exit(1 if fails else 0)
