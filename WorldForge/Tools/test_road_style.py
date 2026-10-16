"""Teste: o acabamento do asfalto por estrada (16/10; FWorldEdits::FRoadEdit::Style) vale no caminho do "qualidade
real". Uma estrada de asfalto desenhada no deserto aberto do mapa de teste, uma vez com cada acabamento; na metade
dela, a laje (SliceSlabAt) tem de sumir na "colada" (1) e o desgaste tem de ser o pedido nas outras.

  powershell -File WorldForge/Tools/forge.ps1 py Tools\\test_road_style.py
"""
import json
import os

import unreal

lib = unreal.WorldForgeEditorLib
SAVED = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'Saved', 'WorldForge'))
base = os.path.join(SAVED, 'world_test_salt2.json')
tmp_settings = os.path.join(SAVED, 'test_style_settings.json')
tmp_edits = os.path.join(SAVED, 'test_style_edits.json')
folder = os.path.join(SAVED, 'test_style_debug')
A, B = (-430.0, -10.0), (-250.0, -10.0)

with open(base, encoding='utf-8-sig') as f:
    settings = json.load(f)

want = {0: 'automático', 1: 'sem laje', 2: 'inteira (0,10)', 3: 'gasta (0,52)', 4: 'destruída (0,85)'}
ok = True
for style in range(5):
    with open(tmp_edits, 'w', encoding='utf-8') as f:
        json.dump({'roads': [{'paved': True, 'style': style, 'fade': [False, False], 'points': [list(A), list(B)]}]}, f)
    s = dict(settings)
    s['edits'] = tmp_edits
    with open(tmp_settings, 'w', encoding='utf-8') as f:
        json.dump(s, f)
    lib.load_world_settings(tmp_settings)
    lib.debug_slice_maps(folder)
    mid = unreal.Vector2D((A[0] + B[0]) * 50.0, (A[1] + B[1]) * 50.0)
    v = lib.slice_slab_at(mid)
    wear = v.x
    good = (wear < 0) if style == 1 else ((wear >= 0 and abs(wear - {2: 0.1, 3: 0.52, 4: 0.85}[style]) < 1e-3) if style >= 2 else wear >= 0)
    ok = ok and good
    print('FORGE style %d (%s): desgaste %.3f, beira %.2f -> %s' % (style, want[style], wear, v.y, 'ok' if good else 'ERRO'))
print('FORGE style teste', 'ok' if ok else 'FALHOU')
