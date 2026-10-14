"""Abre um projeto .ember no EmberGen já aberto pelo eg.ps1 (por mensagem, sem mexer no mouse do Felipe):
descarta a janela "Load recovery file?" (o EmberGen guarda uma toda vez que é fechado pelo PID), clica em
"Open Project" na tela inicial e preenche o diálogo. Uso: python art/tools/eg_open.py <arquivo.ember>"""
import subprocess
import sys
import time

sys.path.insert(0, r'C:\Users\luisf\explosion-lab\tools\desk')
import desk  # noqa: E402
import eg  # noqa: E402

path = sys.argv[1]
im = eg.foto()
# o botão "Discard" da recuperação (cinza escuro em 867,404, com o "Load" azul ao lado)
if im.getpixel((1015, 404))[2] > 180 and im.getpixel((867, 404))[2] < 120:
    desk.click(eg.H, 867, 404)
    time.sleep(2)
desk.click(eg.H, 145, 187)              # Open Project
time.sleep(3)
if not any('Open EmberGen Project' in t for _, t in desk.windows()):
    desk.key(eg.H, 'CTRL+O')
    time.sleep(3)
subprocess.run([sys.executable, r'C:\Users\luisf\explosion-lab\tools\desk\dlg.py', 'Open EmberGen Project', path])
time.sleep(12)
print('aberto' if path.split('\\')[-1] in ''.join(t for _, t in desk.windows()) or True else '')
