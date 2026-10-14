"""Teste: o mapa importado do Zero Hour (Tools/zh_import.py) no caminho do "qualidade real".

  powershell -File WorldForge/Tools/forge.ps1 py Tools\test_import.py

Grava os controles com "imported" (como o app grava), monta os mapas de
depuração (o chão do acabamento visto de cima) e confere que o relevo é o do
importado (o tamanho e a altura de um ponto).
"""
import json
import os

import unreal

lib = unreal.WorldForgeEditorLib
SAVED = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'Saved', 'WorldForge'))
folder = os.path.join(SAVED, 'imported', 'DesertFury')
meta = json.load(open(os.path.join(folder, 'map.json'), encoding='utf-8'))
settings = {'map': '/Game/Maps/World_Import', 'seed': 1, 'players': 2, 'size': meta['size_m'], 'biome': meta['biome'],
            'relief': 0.5, 'plateaus': 0.5, 'water': 0.35, 'erosion': 0.5, 'forests': 0.5, 'resources': 0.5, 'frame': 0,
            'imported': folder, 'edits': os.path.join(folder, 'edits.json')}
tmp = os.path.join(SAVED, 'test_import_settings.json')
with open(tmp, 'w', encoding='utf-8') as f:
    json.dump(settings, f)
lib.load_world_settings(tmp)
out = os.path.join(SAVED, 'test_import_debug')
ok_maps = lib.debug_slice_maps(out)
import array
n = meta['n']
h = array.array('f')
with open(os.path.join(folder, 'height.bin'), 'rb') as f:
    h.frombytes(f.read())
i, j = n // 3, n // 2
x, y = i * meta['cell'] - 0.5 * meta['size_m'], j * meta['cell'] - 0.5 * meta['size_m']
got = lib.slice_height_at(unreal.Vector2D(x * 100.0, y * 100.0)) / 100.0   # cm
want = h[j * n + i]
ok = ok_maps and abs(got - want) < 1.5
unreal.log('FORGE test import: %s, %.0f m; altura em (%.0f, %.0f): %.2f m (importado %.2f) -> %s'
           % (meta['title'], meta['size_m'], x, y, got, want, 'OK' if ok else 'FALHOU'))
