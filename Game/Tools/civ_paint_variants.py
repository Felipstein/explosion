"""As variantes de cor dos carros do kit civil v2 (01/11/2026; docs/KIT_CIVIL_V2.md): a mesma malha, só o slot da lataria trocado
dentro do GLB (o nome do material e a cor de conferência no JSON do glTF; a malha, o UV e as máscaras ficam iguais, byte a byte).

O jogo hoje não pinta o kit civil por instância (CIVIL_SLOTS do import_provisional.py: um material por slot), então a variedade da rua
vem de variantes: o branco (Paint → TankPaint), o bege (GlaPaint) e o oliva (Crate). A cor desbotada vermelha das picapes do
road_town_battle_gemini pede um slot novo (ver docs/KIT_CIVIL_V2.md §4).

  python Game/Tools/civ_paint_variants.py            (todas as da tabela VARIANTS)
"""
import json
import os
import struct

R = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
V = os.path.join(R, 'captures', 'game', 'models', 'civ_v2')


def _lin(r, g, b):
    return [r, g, b, 1.0]


# a cor de conferência (o baseColorFactor, linear) de cada slot: a do SLOTS do import_provisional.py
SLOT_COLOR = {'Paint': _lin(0.40, 0.405, 0.38), 'GlaPaint': _lin(0.40, 0.31, 0.14), 'Crate': _lin(0.10, 0.10, 0.045)}

# (pasta, GLB de origem, o slot da lataria nele, [(sufixo, slot novo)...]); só o material da lataria troca (o galão oliva, o
# Crate da carga, fica como está)
VARIANTS = [
    ('sedan', 'SM_civ_sedan', 'Paint', [('beige', 'GlaPaint'), ('olive', 'Crate')]),
    ('sedan', 'SM_civ_car_burnt', 'Paint', [('beige', 'GlaPaint')]),
    ('hatch', 'SM_civ_hatch', 'GlaPaint', [('white', 'Paint'), ('olive', 'Crate')]),
    ('pickup', 'SM_civ_pickup', 'Paint', [('beige', 'GlaPaint'), ('olive', 'Crate')]),
    ('van', 'SM_civ_van', 'Paint', [('beige', 'GlaPaint')]),
    ('truck', 'SM_civ_truck', 'Paint', [('beige', 'GlaPaint'), ('olive', 'Crate')]),
]


def repaint(src, dst, old_slots, new_slot):
    b = open(src, 'rb').read()
    magic, ver, total = struct.unpack('<III', b[:12])
    jlen, jtype = struct.unpack('<II', b[12:20])
    j = json.loads(b[20:20 + jlen])
    rest = b[20 + jlen:]
    n = 0
    for m in j.get('materials', []):
        base = m.get('name', '').split('.')[0]
        if base in old_slots:
            m['name'] = new_slot
            m.setdefault('pbrMetallicRoughness', {})['baseColorFactor'] = SLOT_COLOR[new_slot]
            n += 1
    js = json.dumps(j, separators=(',', ':')).encode('utf-8')
    js += b' ' * ((4 - len(js) % 4) % 4)
    out = struct.pack('<III', magic, ver, 12 + 8 + len(js) + len(rest)) + struct.pack('<II', len(js), jtype) + js + rest
    open(dst, 'wb').write(out)
    return n


if __name__ == '__main__':
    for folder, name, body, vs in VARIANTS:
        src = os.path.join(V, folder, name + '.glb')
        if not os.path.exists(src):
            print('FALTA', src)
            continue
        for suf, slot in vs:
            dst = os.path.join(V, folder, '%s_%s.glb' % (name, suf))
            n = repaint(src, dst, (body,), slot)
            print('VARIANTE', os.path.basename(dst), n, 'material(is) ->', slot)
