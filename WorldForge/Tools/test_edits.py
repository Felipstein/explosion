"""Teste: as edições à mão (apagados) valem no caminho do "qualidade real".

  powershell -File WorldForge/Tools/forge.ps1 py Tools\\test_edits.py

Pega as palmeiras do mapa de teste do deserto, grava todas como apagadas,
lê de volta pelos controles (o mesmo LoadWorldSettings do "qualidade real")
e confere que o sorteio não devolve mais nenhuma. Depois apaga só metade.
"""
import json
import os

import unreal

lib = unreal.WorldForgeEditorLib
SAVED = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'Saved', 'WorldForge'))
base = os.path.join(SAVED, 'world_test_salt2.json')
tmp_settings = os.path.join(SAVED, 'test_edits_settings.json')
tmp_edits = os.path.join(SAVED, 'test_edits.json')
folder = os.path.join(SAVED, 'test_edits_debug')

with open(base, encoding='utf-8') as f:
    settings = json.load(f)


def run(deleted):
    s = dict(settings)
    if deleted is not None:
        with open(tmp_edits, 'w', encoding='utf-8') as f:
            json.dump({'deleted': [['TreePalm', x, y] for x, y in deleted]}, f)
        s['edits'] = tmp_edits
    with open(tmp_settings, 'w', encoding='utf-8') as f:
        json.dump(s, f)
    lib.load_world_settings(tmp_settings)
    lib.debug_slice_maps(folder)
    return [(p.x, p.y) for p in lib.slice_candidates('TreePalm', 0.0)]


all_palms = run(None)
none_left = run(all_palms)
half_left = run(all_palms[::2])
ok = len(all_palms) > 0 and len(none_left) == 0 and len(half_left) == len(all_palms) - len(all_palms[::2])
unreal.log('FORGE test edits: %d palmeiras; apagando todas sobram %d; apagando metade sobram %d -> %s'
           % (len(all_palms), len(none_left), len(half_left), 'OK' if ok else 'FALHOU'))


# ---- estradas: a tirada some e a desenhada entra (o mesmo BuildRoads do app) ----
def run_roads(edits):
    s = dict(settings)
    if edits is not None:
        with open(tmp_edits, 'w', encoding='utf-8') as f:
            json.dump(edits, f)
        s['edits'] = tmp_edits
    with open(tmp_settings, 'w', encoding='utf-8') as f:
        json.dump(s, f)
    lib.load_world_settings(tmp_settings)
    lib.debug_slice_maps(folder)


def dist_m(x, y):
    return lib.slice_road_distance(unreal.Vector2D(x * 100.0, y * 100.0)) / 100.0


run_roads(None)
# o trecho de asfalto mais comprido: o meio dele é o ponto da estrada tirada
segs, cur = [], []
for p in lib.slice_road(4.0):
    if p.z < -1e8:
        if cur: segs.append(cur)
        cur = []
    else:
        cur.append((p.x / 100.0, p.y / 100.0))
if cur: segs.append(cur)
longest = max(segs, key=len)
probe = longest[len(longest) // 2]
before = dist_m(*probe)
# um lugar longe do asfalto (procura numa grade de 40 m) pra estrada nova
far = None
for gx in range(-400, 401, 40):
    for gy in range(-400, 401, 40):
        if dist_m(gx, gy) > 80 and dist_m(gx + 60, gy) > 80:
            far = (gx, gy)
            break
    if far: break
run_roads({'removed_roads': [list(probe)]})
after_removed = dist_m(*probe)
new_road = [[far[0], far[1]], [far[0] + 30, far[1] + 8], [far[0] + 60, far[1]]]
run_roads({'roads': [{'paved': True, 'points': new_road}]})
on_new = dist_m(far[0] + 30, far[1] + 6)
ok = before < 0.5 and after_removed > 3.0 and on_new < 2.0
unreal.log('FORGE test roads: tirada: %.1f m -> %.1f m do ponto; desenhada em %s: %.1f m do meio -> %s'
           % (before, after_removed, far, on_new, 'OK' if ok else 'FALHOU'))


# ---- peça movida: o sorteio põe no lugar novo (a chave é o ponto sorteado) ----
def run_moved(moved):
    s = dict(settings)
    with open(tmp_edits, 'w', encoding='utf-8') as f:
        json.dump({'moved': moved}, f)
    s['edits'] = tmp_edits
    with open(tmp_settings, 'w', encoding='utf-8') as f:
        json.dump(s, f)
    lib.load_world_settings(tmp_settings)
    lib.debug_slice_maps(folder)
    return [(p.x, p.y) for p in lib.slice_candidates('TreePalm', 0.0)]


fx, fy = all_palms[0]
moved = run_moved([['TreePalm', fx, fy, fx + 12.0, fy - 5.0, 30.0]])
near = lambda pts, x, y: any(abs(a - x) < 0.05 and abs(b - y) < 0.05 for a, b in pts)
ok = len(moved) == len(all_palms) and near(moved, fx + 12.0, fy - 5.0) and not near(moved, fx, fy)
unreal.log('FORGE test moved: palmeira de (%.1f, %.1f) -> (%.1f, %.1f); %d palmeiras -> %s'
           % (fx, fy, fx + 12.0, fy - 5.0, len(moved), 'OK' if ok else 'FALHOU'))
