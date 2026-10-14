"""Depuração: imagens do mapa gerado (1 m por pixel), sem montar nada.

  $env:WORLD_SETTINGS = 'WorldForge\\Saved\\WorldForge\\world_test_coast.json'
  powershell -File WorldForge/Tools/forge.ps1 py Tools\\debug_maps.py

Grava em captures/worldforge/debug_<nome do JSON>/: relevo sombreado, bacias
(branco salina, azul oásis, laranja lagoa seca), leitos secos (R = leito, G =
fluxo), pesos das camadas do chão (w012 = areia/cascalho/terra, w345 =
rocha/salina/asfalto). O norte fica em cima.
PROFILE=x0,y0,x1,y1 (m) também imprime a altura a cada 2 m nessa linha.
"""
import os

import unreal

lib = unreal.WorldForgeEditorLib
path = os.environ['WORLD_SETTINGS']
name = os.path.splitext(os.path.basename(path))[0]
folder = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'captures', 'worldforge', 'debug_' + name))
res = lib.load_world_settings(path)
unreal.log('FORGE debug settings %s: %s' % (path, res))
unreal.log('FORGE debug maps ok: %s -> %s' % (lib.debug_slice_maps(folder), folder))
if os.environ.get('PROFILE'):
    x0, y0, x1, y1 = [float(v) for v in os.environ['PROFILE'].split(',')]
    n = int(max(abs(x1 - x0), abs(y1 - y0)) / 2.0)
    hs = []
    for k in range(n + 1):
        x, y = x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n
        hs.append('%.1f' % (lib.slice_height_at(unreal.Vector2D(x * 100.0, y * 100.0)) / 100.0))
    unreal.log('FORGE profile ' + ' '.join(hs))
