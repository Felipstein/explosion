"""Manda a janela do EmberGen pra trás de todas, sem ativar (o Felipe pode estar usando o PC): o desk.py fotografa
e clica por mensagem mesmo com a janela coberta. uso: python art/tools/eg_back.py"""
import ctypes, sys
sys.path.insert(0, r'C:\Users\luisf\explosion-lab\tools\desk')
import desk
h = desk.find('EmberGen')
HWND_BOTTOM, SWP_NOSIZE, SWP_NOMOVE, SWP_NOACTIVATE = 1, 0x1, 0x2, 0x10
ctypes.windll.user32.SetWindowPos(h, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE)
print('atrás', h)
