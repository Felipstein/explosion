"""Grava o village.json de um mapa sem remontar o mapa (a vila sai igual: mesma semente).

  $env:WORLD_SETTINGS = 'WorldForge\Saved\WorldForge\world_test_salt2.json'
  powershell -File WorldForge/Tools/forge.ps1 py Tools\village_export.py

Vai pra Saved/WorldForge/exports/<mapa>/village.json, o mesmo lugar da montagem.
"""
import json
import os

import unreal

lib = unreal.WorldForgeEditorLib
path = os.environ['WORLD_SETTINGS']
with open(path, encoding='utf-8') as f:
    map_name = json.load(f).get('map', '/Game/Maps/World_Full').split('/')[-1]
lib.load_world_settings(path)
tmp = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'Saved', 'WorldForge', 'village_export_debug'))
lib.debug_slice_maps(tmp)
village = lib.slice_village_json(map_name)
out = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'Saved', 'WorldForge', 'exports', map_name))
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, 'village.json'), 'w', encoding='utf-8') as f:
    f.write(village)
unreal.log('FORGE village export %s: hash %s -> %s' % (map_name, lib.slice_village_hash(), out))
