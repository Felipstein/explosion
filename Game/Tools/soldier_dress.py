"""O soldado vestido (30/10, a infantaria: "nos game shots tem várias imagens lindas com unidades, infantaria, segue aqueles modelos").

O corpo é o do modelo da Unreal 5.8 (SKM_Manny_Simple, exportado pelo Tools/export_manny.py), no MESMO esqueleto: as animações do
modelo tocam nele sem retarget, e nada aqui mexe nos ossos. O que este script faz, no Blender, sem nada comprado nem baixado:

1. A farda pintada nas UVs do corpo (uma textura por material do corpo): a camuflagem da facção em blocos (o ruído em 3D, contínuo
   nas costuras), a bota, a luva, a joelheira, o cinto, a pele do rosto; o alfa é a máscara da cor do time (a faixa do ombro).
2. As peças por cima, rígidas, cada uma presa a um osso (o colete com os porta-carregadores, o capacete com a capa, a mochila, as
   bolsas do cinto), modeladas em volta do corpo na pose de repouso, com a cor nos vértices (o alfa = a cor do time).
3. Um JSON com cada peça: o osso e a origem dela no espaço do corpo da Unreal (cm), pro jogo prender (RTSUnitActor).

Os game shots (docs/concept/scenes/desert_golden_hour/best: street_battle_usa_china, road_town_battle): de cima, o soldado lê pela
silhueta volumosa (capacete, colete, mochila) e pela cor chapada da farda contra a areia, em grupos; o rosto não se vê. Por isso o
volume vem das peças e o detalhe miúdo fica na pintura.

Uso (Blender sem janela):
  blender -b -P Game/Tools/soldier_dress.py -- <SKM_Manny_Simple.fbx> <pasta de saída> <facção: usa | gla | china>
Saem: T_Soldier_<F>_<slot>.png, SM_Soldier_<F>_<peça>.glb, soldier_<f>.json e uma folha de conferência (soldier_<f>_check.png).

Os eixos: a Unreal entrega o FBX ao Blender em (x, −y, z) e o Blender devolve o GLB à Unreal em (x, −y, z) (o mesmo do
export_gla_technical.py): as peças modeladas em volta do corpo importado caem no lugar dele lá. O script confere a frente do corpo
(os dedos do pé) e para se ela não estiver em −Y.
"""
import bpy
import bmesh
import json
import math
import os
import sys

import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
FBX = ARGS[0] if len(ARGS) > 0 else ''
OUT = ARGS[1] if len(ARGS) > 1 else os.path.dirname(FBX)
FACTION = (ARGS[2] if len(ARGS) > 2 else 'usa').lower()
# (01/11) o tipo: rifle (o fuzileiro), at (o antitanque), worker (o trabalhador da GLA). O nome das saídas: a facção no fuzileiro (as de
# antes), a facção e o tipo nos outros (T_Soldier_CHINA_AT_0, SM_Soldier_CHINA_AT_head, soldier_china_at.json)
ROLE = (ARGS[3] if len(ARGS) > 3 else 'rifle').lower()
TAG = FACTION if ROLE == 'rifle' else FACTION + '_' + ROLE
TEX = 1024


def log(*a):
    print('SOLDIER', *a, flush=True)


# As fardas (albedo linear, medido contra a areia do jogo ~0,49/0,40/0,27 e o tanque USA ~0,36/0,31/0,22). A camuflagem: a base e duas
# manchas (o OCP do Exército americano: tons de areia, oliva e marrom em blocos suaves; o deserto chinês: areia e marrom; a GLA:
# roupa civil e militar misturada, cáqui e oliva lisos).
FACTIONS = {
    # (30/10, 2ª volta: de cima o soldado na farda de 0,34 sumia na areia de 0,49; nos game shots a infantaria lê escura, em
    # silhueta) a farda ~15% e o equipamento ~25% mais escuros, a bota e a luva bem mais (o contorno das mãos e dos pés)
    # (31/10, 3ª volta, medido na câmera do jogo de 67 m contra o r1_raid_slabs: lá o soldado fica a 0,12–0,16 do brilho da areia, em
    # silhueta escura; o nosso ia de 0,15 a 0,87, as partes acesas da cor da areia, e sumia) ~45% mais escuro, o oliva e o coyote do OCP
    # na sombra: a farda ~0,15, o equipamento ~0,11
    # (01/11, a distinção das facções) a USA no coyote (o marrom-areia do OCP e do equipamento): o matiz quente, a mesma luminância
    'usa': dict(
        cloth=(0.19, 0.15, 0.10), spot1=(0.14, 0.11, 0.07), spot2=(0.17, 0.16, 0.11), camo=0.55,
        boots=(0.10, 0.08, 0.055), gloves=(0.07, 0.065, 0.05), skin=(0.40, 0.28, 0.21), knee=(0.10, 0.085, 0.06),
        belt=(0.09, 0.08, 0.06), vest=(0.17, 0.135, 0.085), pouch=(0.15, 0.12, 0.075), helmet=(0.18, 0.14, 0.09),
        pack=(0.16, 0.125, 0.08), helmet_kind='ach', vest_kind='plate', has_pack=True),
    # (31/10, o elenco das 3 facções: docs/ELENCO.md) a China e a GLA na mesma régua da 3ª volta da USA (o soldado escuro, em silhueta,
    # a 0,12–0,16 do brilho da areia na câmera do jogo): a China no oliva escuro da bíblia (as unidades dela são as mais escuras,
    # docs/CONCEITO_UNIDADES.md §3: a farda ~0,13, o equipamento ~0,10); a GLA de roupa civil e militar misturada, o cáqui ~0,21 e a
    # calça oliva ~0,12, com o pano claro na cabeça (~0,30) que faz a silhueta dela diferente de cima
    # (01/11) a China no verde fechado (sem o amarelo do oliva) e o gorro de pele com as abas (o Red Guard do ZH)
    'china': dict(
        cloth=(0.065, 0.10, 0.05), spot1=(0.045, 0.075, 0.035), spot2=(0.09, 0.12, 0.06), camo=0.6,
        boots=(0.06, 0.055, 0.045), gloves=(0.07, 0.07, 0.055), skin=(0.44, 0.33, 0.24), knee=(0.06, 0.085, 0.045),
        belt=(0.07, 0.08, 0.05), vest=(0.06, 0.09, 0.045), pouch=(0.055, 0.08, 0.04), helmet=(0.075, 0.09, 0.05),
        pack=(0.06, 0.085, 0.045), helmet_kind='ushanka', vest_kind='plate', has_pack=True),
    'gla': dict(
        cloth=(0.21, 0.19, 0.14), spot1=(0.21, 0.19, 0.14), spot2=(0.21, 0.19, 0.14), camo=0.0,
        pants=(0.12, 0.12, 0.085), boots=(0.08, 0.065, 0.05), gloves=(0.40, 0.28, 0.20), skin=(0.40, 0.28, 0.20),
        knee=(0.12, 0.12, 0.085), belt=(0.09, 0.08, 0.06), vest=(0.11, 0.11, 0.075), pouch=(0.10, 0.10, 0.07),
        helmet=(0.42, 0.38, 0.30), pack=(0.14, 0.12, 0.09), helmet_kind='wrap', vest_kind='rig', has_pack=False, scarf=True),
    # (01/11) a Rússia com as peças próprias (eram as da China): o cinza-esverdeado com as manchas marrons do EMR, o capacete grande com a
    # capa e a faixa dos óculos (o 6B47), a gola alta do colete (o 6B45)
    'russia': dict(
        cloth=(0.09, 0.10, 0.08), spot1=(0.06, 0.065, 0.05), spot2=(0.12, 0.10, 0.07), camo=0.6,
        boots=(0.05, 0.05, 0.045), gloves=(0.06, 0.06, 0.05), skin=(0.42, 0.31, 0.24), knee=(0.07, 0.075, 0.06),
        belt=(0.07, 0.07, 0.06), vest=(0.08, 0.085, 0.07), pouch=(0.075, 0.08, 0.065), helmet=(0.09, 0.095, 0.075),
        pack=(0.075, 0.08, 0.065), helmet_kind='6b47', vest_kind='plate', has_pack=True, collar=True),
}
# (01/11, o kit por tipo) o que muda do fuzileiro da facção em cada tipo, pela forma, não pela cor: o corpo (o casacão até o joelho da
# China, a túnica solta da GLA), a cabeça (o gorro de pele, o capacete com os óculos, o turbante, o pano escuro) e a carga nas costas
# (o tubo reserva do Javelin, os 3 foguetes acima dos ombros do Tank Hunter, as 3 ogivas do RPG, o tripé do Kornet, o saco e a pá do
# trabalhador). Na câmera do jogo o soldado tem ~50 px de altura e 16 px de ombro: só lê o que muda a silhueta em ~10 px (o casaco, a
# carga acima do ombro, o tubo de 1 m), não o detalhe de 3 cm
KITS = {
    ('usa', 'rifle'): dict(back='pack', face='glasses'),
    ('usa', 'at'): dict(back='tube', face='glasses'),
    ('china', 'rifle'): dict(vest_kind='none', coat='long', coat_color=(0.05, 0.075, 0.04), back=None, belt_front=True),
    ('china', 'at'): dict(helmet_kind='steel_goggles', vest_kind='rig', back='rockets3'),
    # (01/11, ele: "nos GLA não tem que ter bagulho militar, os caras são rebeldes") a GLA de roupa civil: a túnica e a calça de pano,
    # o colete civil aberto (sem placa nem porta-carregador), a bandoleira de couro cruzada no peito, a faixa de pano na cintura, o
    # sapato velho de couro (sem coturno nem joelheira), o lenço cobrindo o rosto (o rebelde) e a barba (o trabalhador)
    ('gla', 'worker'): dict(helmet_kind='turban', helmet=(0.50, 0.48, 0.42), vest_kind='none', coat='tunic',
                            coat_color=(0.25, 0.27, 0.28), scarf=False, back='sack', face='beard', knee=None,
                            boots=(0.13, 0.09, 0.06), belt=(0.17, 0.07, 0.045), pants=(0.20, 0.18, 0.14)),
    ('gla', 'rifle'): dict(coat='tunic', coat_color=(0.23, 0.21, 0.16), back=None, vest_kind='waistcoat', vest=(0.07, 0.06, 0.05),
                           bandolier=True, face='veil', veil=(0.46, 0.43, 0.36), scarf=False, knee=None,
                           boots=(0.12, 0.085, 0.06), belt=(0.17, 0.07, 0.045)),
    ('gla', 'at'): dict(helmet=(0.075, 0.07, 0.06), scarf=False, coat='tunic', coat_color=(0.17, 0.16, 0.13), back='warheads3',
                        vest_kind='waistcoat', vest=(0.11, 0.09, 0.06), strap=True, face='veil', veil=(0.075, 0.07, 0.06), knee=None,
                        boots=(0.12, 0.085, 0.06), belt=(0.10, 0.12, 0.07), pants=(0.10, 0.10, 0.09)),
    ('russia', 'rifle'): dict(back='pack', groin=True, face='balaclava'),
    ('russia', 'at'): dict(back='tripod', groin=True, face='balaclava'),
}
F = dict(FACTIONS[FACTION])
F.update(KITS.get((FACTION, ROLE), {}))
F.setdefault('back', 'pack' if F['has_pack'] else None)


# ----------------------------------------------------------------------------------------------------------------------------------
# o corpo
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX, automatic_bone_orientation=False, ignore_leaf_bones=False)
ARM = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
BODY = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
log('corpo', BODY.name, len(BODY.data.vertices), 'vértices', len(BODY.data.polygons), 'faces;', 'materiais',
    [m.name if m else None for m in BODY.data.materials])


def bone_head(name):
    b = ARM.data.bones.get(name)
    return ARM.matrix_world @ b.head_local if b else None


def bone_tail(name):
    b = ARM.data.bones.get(name)
    return ARM.matrix_world @ b.tail_local if b else None


MW = BODY.matrix_world
VERTS = np.array([tuple(MW @ v.co) for v in BODY.data.vertices], dtype=np.float64)
NORMS = np.array([tuple((MW.to_3x3() @ v.normal).normalized()) for v in BODY.data.vertices], dtype=np.float64)
H = VERTS[:, 2].max() - VERTS[:, 2].min()
log('altura %.3f (unidade do Blender)' % H)
# o corpo em metros: o FBX da Unreal chega em cm com a escala 0,01 no objeto (matrix_world já aplica); se vier em cm, divide
UNIT = 1.0 if H < 5.0 else 0.01
VERTS *= UNIT
FWD = Vector(tuple((bone_head('ball_l') - bone_head('foot_l')).normalized()))
log('frente do pé', tuple(round(c, 2) for c in FWD))
if FWD.y > -0.5:
    raise SystemExit('SOLDIER a frente do corpo não está em −Y no Blender (%s): os eixos mudaram, conferir antes de seguir' % (tuple(FWD),))

# o osso de cada vértice (o de maior peso)
GROUPS = {g.index: g.name for g in BODY.vertex_groups}
DOM = []
for v in BODY.data.vertices:
    best, bw = '', -1.0
    for g in v.groups:
        if g.weight > bw:
            best, bw = GROUPS.get(g.group, ''), g.weight
    DOM.append(best)
DOM = np.array(DOM)


def bh(name):
    return np.array(tuple(bone_head(name))) * UNIT


def bt(name):
    return np.array(tuple(bone_tail(name))) * UNIT


Z_ANKLE = (bh('foot_l')[2] + bh('foot_r')[2]) / 2.0
Z_KNEE = (bh('calf_l')[2] + bh('calf_r')[2]) / 2.0
Z_PELVIS = bh('pelvis')[2]
Z_SPINE2 = bh('spine_02')[2]
Z_NECK = bh('neck_01')[2]
# (01/11, ele: "sem rosto ainda") a cabeça medida: o centro, o alto, o queixo (o ponto mais baixo da frente) e as alturas do rosto pelas
# proporções de sempre (os olhos na metade da cabeça, o pé do nariz a ~30%, a boca a ~19% do queixo)
_HP = VERTS[np.array([d == 'head' for d in DOM])]
HEAD_C = (_HP.min(0) + _HP.max(0)) / 2.0
HEAD_TOP = float(_HP[:, 2].max())
_HF = _HP[_HP[:, 1] < HEAD_C[1] - 0.03]
HEAD_CHIN = float(_HF[:, 2].min()) if len(_HF) else HEAD_TOP - 0.23
HEAD_H = HEAD_TOP - HEAD_CHIN
Z_EYE = HEAD_CHIN + 0.47 * HEAD_H
Z_NOSE = HEAD_CHIN + 0.30 * HEAD_H
Z_MOUTH = HEAD_CHIN + 0.19 * HEAD_H
log('alturas: tornozelo %.2f joelho %.2f pélvis %.2f spine_02 %.2f pescoço %.2f' % (Z_ANKLE, Z_KNEE, Z_PELVIS, Z_SPINE2, Z_NECK))
log('cabeça: alto %.3f queixo %.3f (%.3f m) olhos %.3f nariz %.3f boca %.3f' % (HEAD_TOP, HEAD_CHIN, HEAD_H, Z_EYE, Z_NOSE, Z_MOUTH))


# ----------------------------------------------------------------------------------------------------------------------------------
# 1. a farda pintada
def camo(p):
    """A camuflagem em blocos suaves (o ruído em 3D: contínuo nas costuras da UV). p em metros, n×3."""
    x, y, z = p[:, 0] * 9.0, p[:, 1] * 9.0, p[:, 2] * 9.0
    a = np.sin(x * 1.3 + np.sin(z * 0.7) * 2.0) + np.sin(y * 1.1 + z * 0.9 + 1.7) + 0.6 * np.sin(z * 2.3 + x * 0.8)
    b = np.sin(x * 0.9 - y * 1.4 + 4.1) + np.sin(z * 1.7 - x * 0.6 + 0.4) + 0.5 * np.sin(y * 2.6 + 2.2)
    return a, b


def region_colors(idx, pos):
    """A cor (RGB linear) e a máscara do time de cada ponto: idx = o vértice mais perto (o osso dele), pos = o ponto (m)."""
    n = len(idx)
    col = np.tile(np.array(F['cloth']), (n, 1))
    team = np.zeros(n)
    dom = DOM[idx]
    z = pos[:, 2]
    if F['camo'] > 0.0:
        a, b = camo(pos)
        col[a > 0.9] = F['spot1']
        col[(b > 1.1) & ~(a > 0.9)] = F['spot2']
    legs = np.char.startswith(dom.astype(str), 'thigh') | np.char.startswith(dom.astype(str), 'calf') | (dom == 'pelvis')
    if 'pants' in F:
        col[legs] = F['pants']
    if F.get('coat'):
        # (01/11) o casaco/a túnica: o tronco, os braços (a GLA de manga arregaçada: o antebraço fica na pele) e as coxas até a barra
        hem = Z_KNEE - 0.06 if F['coat'] == 'long' else Z_KNEE + 0.02
        ds = dom.astype(str)
        fore = np.char.startswith(ds, 'lowerarm')
        body = ~np.char.startswith(ds, 'calf') & ~np.char.startswith(ds, 'foot') & ~np.char.startswith(ds, 'ball') & (z > hem)
        body &= ~((dom == 'head') | np.char.startswith(ds, 'neck'))
        if F['coat'] == 'tunic':
            body &= ~fore
        col[body] = F['coat_color']
        if F['coat'] == 'tunic':
            col[fore & ~np.char.startswith(ds, 'hand')] = F['skin']
    feet = np.char.startswith(dom.astype(str), 'foot') | np.char.startswith(dom.astype(str), 'ball') | (z < Z_ANKLE + 0.13)
    col[feet] = F['boots']
    hands = np.zeros(n, dtype=bool)
    for k in ('hand', 'index', 'middle', 'ring', 'pinky', 'thumb'):
        hands |= np.char.startswith(dom.astype(str), k)
    col[hands] = F['gloves']
    head = (dom == 'head') | np.char.startswith(dom.astype(str), 'neck')
    col[head & (z > Z_NECK - 0.02)] = F['skin']
    # (01/11, ele: "sem rosto ainda") o rosto pintado: a órbita escura dos olhos, a sobrancelha, a boca; a barba do trabalhador da GLA; a
    # balaclava do russo (só a fresta dos olhos)
    face = head & (pos[:, 1] < HEAD_C[1] - 0.02) & (z > Z_NECK - 0.02)
    fx = pos[:, 0] - HEAD_C[0]
    hair = F.get('hair', (0.035, 0.028, 0.022))
    if F.get('face') == 'balaclava':
        col[head & (z > Z_NECK - 0.04)] = (0.05, 0.055, 0.045)
        col[face & (np.abs(z - Z_EYE) < 0.019) & (np.abs(fx) < 0.06)] = F['skin']
    for sx in (-1.0, 1.0):
        col[face & (((fx - sx * 0.032) / 0.017) ** 2 + ((z - Z_EYE) / 0.0105) ** 2 < 1.0)] = (0.035, 0.028, 0.024)
        if F.get('face') != 'balaclava':
            col[face & (np.abs(z - (Z_EYE + 0.022)) < 0.0055) & (np.abs(fx - sx * 0.033) < 0.022)] = hair
    if F.get('face') != 'balaclava':
        col[face & (np.abs(z - Z_MOUTH) < 0.0035) & (np.abs(fx) < 0.021)] = np.array(F['skin']) * 0.55
    if F.get('face') == 'beard':
        col[head & (pos[:, 1] < HEAD_C[1] + 0.035) & (z < Z_MOUTH + 0.006) & (z > Z_NECK - 0.03)] = hair
        col[face & (np.abs(z - (Z_MOUTH + 0.012)) < 0.006) & (np.abs(fx) < 0.027)] = hair
    # a joelheira: na frente do joelho (y < joelho: a frente é −Y), uma faixa de ~12 cm
    knee = legs & (np.abs(z - Z_KNEE) < 0.065) & (pos[:, 1] < (bh('calf_l')[1] - 0.02))
    if F.get('knee') is not None:
        col[knee] = F['knee']
    # o cinto: a faixa de 5 cm em volta da cintura
    belt = (np.abs(z - (Z_PELVIS + 0.09)) < 0.025) & ~hands
    col[belt] = F['belt']
    # a faixa do time no braço (o distintivo do ombro): no terço de cima do braço, por fora
    for side in ('l', 'r'):
        up = bh('upperarm_' + side)
        lo = bh('lowerarm_' + side)
        axis = lo - up
        L = np.linalg.norm(axis)
        t = ((pos - up) @ axis) / max(L * L, 1e-6)
        ds = dom.astype(str)
        arm = np.char.startswith(ds, 'upperarm') & np.char.endswith(ds, '_' + side)
        team[arm & (t > 0.16) & (t < 0.34)] = 1.0
    # (01/11, o acabamento: "feitas de qualquer jeito") a trama do pano (~2 cm) e a mancha de uso larga, pra cor não ficar chapada, e o pó da
    # areia subindo do pé até o joelho (o soldado anda no deserto)
    cloth = ~(head & (z > Z_NECK - 0.02)) & ~hands
    weave = np.sin(pos[:, 0] * 310.0) * np.sin(pos[:, 2] * 290.0) + 0.6 * np.sin(pos[:, 1] * 330.0 + pos[:, 2] * 150.0)
    wear = np.sin(pos[:, 0] * 23.0 + np.sin(pos[:, 2] * 17.0) * 2.0) * np.sin(pos[:, 1] * 19.0 + pos[:, 2] * 11.0)
    k = 1.0 + 0.04 * weave + 0.08 * wear
    col[cloth] *= k[cloth, None]
    dusty = np.clip((Z_KNEE + 0.05 - z) / (Z_KNEE + 0.15 - Z_ANKLE), 0.0, 1.0) ** 1.5
    col = col * (1.0 - 0.35 * dusty[:, None]) + np.array((0.30, 0.24, 0.16)) * (0.35 * dusty[:, None])
    return col, team


def paint_textures():
    me = BODY.data
    uv = me.uv_layers.active.data
    me.calc_loop_triangles()
    slots = sorted({t.material_index for t in me.loop_triangles})
    files = {}
    vpos = VERTS
    for slot in slots:
        img = np.zeros((TEX, TEX, 4), dtype=np.float32)
        filled = np.zeros((TEX, TEX), dtype=bool)
        for tri in me.loop_triangles:
            if tri.material_index != slot:
                continue
            uvs = np.array([tuple(uv[l].uv) for l in tri.loops]) * TEX
            vs = np.array(tri.vertices)
            p3 = vpos[vs]
            x0, y0 = np.floor(uvs.min(0)).astype(int)
            x1, y1 = np.ceil(uvs.max(0)).astype(int)
            x0, y0 = max(x0, 0), max(y0, 0)
            x1, y1 = min(x1, TEX - 1), min(y1, TEX - 1)
            if x1 < x0 or y1 < y0:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            P = np.stack([gx.ravel(), gy.ravel()], 1)
            a, b, c = uvs
            v0, v1 = b - a, c - a
            d = v0[0] * v1[1] - v1[0] * v0[1]
            if abs(d) < 1e-9:
                continue
            w = P - a
            l1 = (w[:, 0] * v1[1] - v1[0] * w[:, 1]) / d
            l2 = (v0[0] * w[:, 1] - w[:, 0] * v0[1]) / d
            l0 = 1.0 - l1 - l2
            inside = (l0 >= -0.02) & (l1 >= -0.02) & (l2 >= -0.02)
            if not inside.any():
                continue
            L = np.stack([l0, l1, l2], 1)[inside]
            pos = L @ p3
            near = vs[np.argmax(L, 1)]
            col, team = region_colors(near, pos)
            px = (P[inside, 0]).astype(int)
            py = (P[inside, 1]).astype(int)
            img[py, px, :3] = col
            img[py, px, 3] = team
            filled[py, px] = True
        # a borda: estende 6 px pra fora das ilhas (a costura não puxa o preto do fundo nos mips)
        for _ in range(6):
            src = img.copy()
            nf = filled.copy()
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                sh = np.roll(np.roll(filled, dy, 0), dx, 1)
                take = sh & ~nf
                img[take] = np.roll(np.roll(src, dy, 0), dx, 1)[take]
                nf |= take
            filled = nf
        # linear → sRGB pro PNG (a Unreal lê como sRGB); o alfa fica linear
        rgb = np.clip(img[:, :, :3], 0.0, 1.0)
        srgb = np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.power(rgb, 1.0 / 2.4) - 0.055)
        out = np.concatenate([srgb, img[:, :, 3:4]], 2)
        name = 'T_Soldier_%s_%d' % (TAG.upper(), slot)
        im = bpy.data.images.new(name, TEX, TEX, alpha=True)
        im.pixels.foreach_set(out[::1].astype(np.float32).ravel())
        path = os.path.join(OUT, name + '.png')
        im.filepath_raw = path
        im.file_format = 'PNG'
        im.save()
        files[slot] = path
        log('textura', path, 'cobertura %.1f%%' % (100.0 * filled.mean()))
    return files


# ----------------------------------------------------------------------------------------------------------------------------------
# 2. as peças
PIECES = []   # (objeto, osso, cor por vértice)


def set_color(obj, rgb, team=0.0):
    me = obj.data
    if not me.color_attributes.get('Col'):
        me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    ca = me.color_attributes['Col']
    for d in ca.data:
        d.color = (rgb[0], rgb[1], rgb[2], team)


def jitter_color(obj, amount, cell):
    """(01/11) A variação da cor por canto, pelo lugar (o pelo do gorro: tufos de ~2 cm; as manchas da capa: ~5 cm), ±amount."""
    me = obj.data
    ca = me.color_attributes['Col']
    for poly in me.polygons:
        c = obj.matrix_world @ poly.center
        h = math.sin(c.x / cell * 12.9898 + c.y / cell * 78.233 + c.z / cell * 37.719) * 43758.5453
        k = 1.0 + amount * (2.0 * (h - math.floor(h)) - 1.0)
        for li in poly.loop_indices:
            col = ca.data[li].color
            ca.data[li].color = (col[0] * k, col[1] * k, col[2] * k, col[3])


def color_where(obj, test, rgb, team):
    """Pinta os cantos cujas faces passam no teste (o centro da face em metros). team > 0: a face vai pro material 1 (o slot "Team" do
    GLB, que o jogo pinta na cor do jogador; o alfa da cor do vértice não chega no material)."""
    me = obj.data
    ca = me.color_attributes['Col']
    if team > 0.0 and len(me.materials) < 2:
        while len(me.materials) < 1:
            me.materials.append(bpy.data.materials.get('M_Soldier') or bpy.data.materials.new('M_Soldier'))
        me.materials.append(bpy.data.materials.get('M_SoldierTeam') or bpy.data.materials.new('M_SoldierTeam'))
    for poly in me.polygons:
        c = obj.matrix_world @ poly.center
        if test(c):
            for li in poly.loop_indices:
                ca.data[li].color = (rgb[0], rgb[1], rgb[2], team)
            if team > 0.0:
                poly.material_index = 1


def new_obj(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def body_bvh(groups=None):
    bm = bmesh.new()
    me = BODY.data
    for v in me.vertices:
        bm.verts.new(Vector(tuple(VERTS[v.index])))
    bm.verts.ensure_lookup_table()
    for p in me.polygons:
        if groups is None or any(DOM[i] in groups for i in p.vertices):
            try:
                bm.faces.new([bm.verts[i] for i in p.vertices])
            except ValueError:
                pass
    t = BVHTree.FromBMesh(bm)
    bm.free()
    return t


BVH = body_bvh()


def surface(origin, direction, dist=1.0):
    hit = BVH.ray_cast(Vector(origin), Vector(direction).normalized(), dist)
    return hit[0]


def rounded_box(name, size, center, bevel, seg=3):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=seg, profile=0.5, affect='EDGES')
    for v in bm.verts:
        v.co += Vector(center)
    return new_obj(name, bm)


def part_target(name, test):
    """Um objeto só com as faces do corpo que passam no teste (os ossos dos vértices): o alvo de encolher as peças por cima."""
    bm = bmesh.new()
    me = BODY.data
    vmap = {}
    for p in me.polygons:
        if not all(test(i) for i in p.vertices):
            continue
        vs = []
        for i in p.vertices:
            if i not in vmap:
                vmap[i] = bm.verts.new(Vector(tuple(VERTS[i])))
            vs.append(vmap[i])
        try:
            bm.faces.new(vs)
        except ValueError:
            pass
    ob = new_obj(name, bm)
    ob.hide_render = True
    return ob


def shrink_onto(ob, target, offset, mode='OUTSIDE_SURFACE'):
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    m = ob.modifiers.new('wrap', 'SHRINKWRAP')
    m.target = target
    m.wrap_method = 'NEAREST_SURFACEPOINT'
    m.wrap_mode = mode
    m.offset = offset
    bpy.ops.object.modifier_apply(modifier='wrap')
    ob.select_set(False)


def apply_mod(ob, kind, **props):
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    m = ob.modifiers.new('m', kind)
    for k, v in props.items():
        setattr(m, k, v)
    bpy.ops.object.modifier_apply(modifier='m')
    ob.select_set(False)


def helmet():
    """O capacete moldado na cabeça: uma esfera encolhida sobre o crânio a ~2 cm (o casco e o forro do ACH/ECH), cortada na
    sobrancelha na frente e abaixo da orelha dos lados e atrás, com a aba levemente aberta. O lenço da GLA desce até o pescoço."""
    head = np.where(DOM == 'head')[0]
    P = VERTS[head]
    lo, hi = P.min(0), P.max(0)
    c = (lo + hi) / 2.0
    r = (hi - lo) / 2.0
    top = hi[2]
    tgt = part_target('HeadTarget', lambda i: DOM[i] == 'head')
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=64, v_segments=40, radius=float(max(r)) * 1.8)
    for v in bm.verts:
        v.co += Vector((c[0], c[1], c[2]))
    ob = new_obj('Helmet', bm)
    kind = F['helmet_kind']
    wrapish = kind in ('wrap', 'turban')
    wrap = {'wrap': 0.012, 'turban': 0.035, 'ushanka': 0.05, '6b47': 0.03, 'steel_goggles': 0.026}.get(kind, 0.022)   # (01/11) o gorro de pele é grosso; o 6B47 é grande
    shrink_onto(ob, tgt, wrap)
    apply_mod(ob, 'CORRECTIVE_SMOOTH', factor=0.8, iterations=6, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    # o corte: a sobrancelha na frente (−Y), a orelha dos lados, a nuca atrás
    brow = top - 0.105
    side = top - 0.135 if not wrapish else top - 0.22
    if F['helmet_kind'] == 'ushanka':
        brow, side = top - 0.085, top - 0.215    # a testa livre e as abas descendo até o queixo dos lados
    elif F['helmet_kind'] == '6b47':
        brow, side = top - 0.10, top - 0.155     # o casco mais fundo dos lados
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    kill = []
    for v in bm.verts:
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        front = max(0.0, -d.y / max(d.length, 1e-6))          # 1 olhando pra frente, 0 dos lados e atrás
        cut = side + (brow - side) * (front ** 1.5)
        if wrapish:
            cut = side + (top - 0.075 - side) * (front ** 3.0)   # o rosto aberto
        if v.co.z < cut:
            kill.append(v)
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    # (2ª volta: a borda saía em degraus, o corte vértice a vértice) a borda vai pra curva do corte: cada vértice dela desce até a altura
    # do corte no ângulo dele, e a aba de baixo abre ~6 mm pra fora
    def cut_at(v):
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        front = max(0.0, -d.y / max(d.length, 1e-6))
        if wrapish:
            return side + (top - 0.075 - side) * (front ** 3.0)
        return side + (brow - side) * (front ** 1.5)
    edge = [v for v in bm.verts if v.is_boundary]
    for v in edge:
        v.co.z = cut_at(v)
    for _ in range(3):
        bmesh.ops.smooth_vert(bm, verts=edge, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    for v in edge:
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        if d.length > 1e-6:
            v.co += d.normalized() * 0.006
    bm.to_mesh(ob.data)
    bm.free()
    apply_mod(ob, 'SOLIDIFY', thickness=0.010, offset=1.0)
    set_color(ob, F['helmet'])
    if F['helmet_kind'] in ('ushanka', '6b47'):
        jitter_color(ob, 0.22 if F['helmet_kind'] == 'ushanka' else 0.15, 0.02 if F['helmet_kind'] == 'ushanka' else 0.05)
    if F['helmet_kind'] == 'ushanka':
        # a aba da frente dobrada pra cima (a faixa de pele grossa na testa) e a estrela do time no meio dela
        hit = surface((c[0], c[1] - 0.5, top - 0.06), (0.0, 1.0, 0.0))
        yf = (hit.y if hit is not None else c[1] - r[1]) - 0.01
        band = rounded_box('Ushanka_band', (0.23, 0.06, 0.065), (c[0], yf, top - 0.075), 0.026, 3)
        set_color(band, tuple(min(1.0, k * 1.25) for k in F['helmet']))
        jitter_color(band, 0.22, 0.02)
        PIECES.append((band, 'head'))
        star = rounded_box('Ushanka_star', (0.03, 0.012, 0.03), (c[0], yf - 0.026, top - 0.075), 0.004, 1)
        set_color(star, F['helmet'])
        color_where(star, lambda p: True, F['helmet'], 1.0)
        PIECES.append((star, 'head'))
        # o topo redondo e mais largo que a cabeça (o "chapéu" que lê de cima: o disco de pele) e as abas caídas dos lados, até o queixo
        cbm = bmesh.new()
        bmesh.ops.create_cone(cbm, cap_ends=True, segments=32, radius1=0.15, radius2=0.13, depth=0.065)
        for v in cbm.verts:
            v.co = Vector((c[0] + v.co.x, c[1] + v.co.y * 1.08, top + 0.012 + v.co.z))
        crown = new_obj('Ushanka_crown', cbm)
        apply_mod(crown, 'SUBSURF', levels=2)
        set_color(crown, F['helmet'])
        jitter_color(crown, 0.22, 0.02)
        PIECES.append((crown, 'head'))
        for sx in (-1.0, 1.0):
            hit = surface((c[0] + sx * 0.5, c[1], top - 0.15), (-sx, 0.0, 0.0))
            xf = (hit.x if hit is not None else c[0] + sx * r[0]) + sx * (wrap + 0.012)
            flap = rounded_box('Ushanka_flap_%d' % int(sx), (0.032, 0.115, 0.17), (xf - sx * 0.004, c[1] + 0.01, top - 0.19), 0.03, 3)
            set_color(flap, F['helmet'])
            jitter_color(flap, 0.22, 0.02)
            PIECES.append((flap, 'head'))
    elif kind in ('6b47', 'steel_goggles'):
        # a faixa dos óculos em volta do casco e os óculos na frente, em cima da aba (o que dá a cara do soldado russo de hoje)
        color_where(ob, lambda p: abs(p.z - (top - 0.075)) < 0.014, (0.035, 0.035, 0.035), 0.0)
        hit = surface((c[0], c[1] - 0.5, top - 0.06), (0.0, 1.0, 0.0))
        yf = (hit.y if hit is not None else c[1] - r[1]) - wrap - 0.012
        for sx in (-1.0, 1.0):
            lens = rounded_box('Goggle_%d' % int(sx), (0.06, 0.03, 0.04), (c[0] + sx * 0.035, yf, top - 0.06), 0.012, 2)
            set_color(lens, (0.03, 0.03, 0.035))
            PIECES.append((lens, 'head'))
    if kind == 'ach':
        # a faixa do time em volta do casco (a fita da unidade), fina, um pouco abaixo do meio
        color_where(ob, lambda p: abs(p.z - (top - 0.06)) < 0.011, F['helmet'], 1.0)
        # o suporte do visor noturno na frente (o que dá a cara do capacete moderno)
        hit = surface((c[0], c[1] - 0.5, top - 0.075), (0.0, 1.0, 0.0))
        yf = (hit.y if hit is not None else c[1] - r[1]) - wrap - 0.012
        nvg = rounded_box('Helmet_nvg', (0.045, 0.018, 0.032), (c[0], yf, top - 0.075), 0.005, 2)
        set_color(nvg, (0.10, 0.10, 0.09))
        PIECES.append((nvg, 'head'))
    if kind == 'turban':
        # (01/11) o turbante do trabalhador: o pano enrolado alto e redondo em cima (o volume que lê de cima, maior que a cabeça)
        tbm = bmesh.new()
        bmesh.ops.create_uvsphere(tbm, u_segments=24, v_segments=12, radius=1.0)
        for v in tbm.verts:
            v.co = Vector((c[0] + v.co.x * 0.125, c[1] + v.co.y * 0.135, top + 0.01 + v.co.z * 0.075))
        dome = new_obj('Turban_dome', tbm)
        set_color(dome, F['helmet'])
        jitter_color(dome, 0.10, 0.03)
        PIECES.append((dome, 'head'))
    if wrapish:
        # a ponta do lenço caindo atrás do pescoço
        tail = rounded_box('Wrap_tail', (0.12, 0.03, 0.14), (c[0], c[1] + r[1] + 0.005, top - 0.24), 0.012, 2)
        set_color(tail, F['helmet'])
        PIECES.append((tail, 'head'))
    PIECES.append((ob, 'head'))
    bpy.data.objects.remove(tgt)
    log('capacete', kind, 'cabeça %.3f×%.3f×%.3f m' % tuple(2 * r), len(ob.data.vertices), 'vértices')


def vest():
    """O colete moldado no tronco: um tubo encolhido sobre o peito e as costas a ~3 cm (a placa e o forro), alisado (sem os
    músculos), do fim do esterno até a cintura; as alças por cima dos ombros (o que se vê de cima); os porta-carregadores na frente."""
    if F['vest_kind'] == 'none':
        return
    if F['vest_kind'] == 'waistcoat':
        waistcoat()
        return
    torso_bones = {'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_l', 'clavicle_r', 'pelvis'}
    tgt = part_target('TorsoTarget', lambda i: DOM[i] in torso_bones)
    s3 = bh('spine_03')
    z_lo = Z_SPINE2 - 0.05
    z_hi = Z_NECK - 0.11
    seg, rings = 48, 10
    bm = bmesh.new()
    grid = []
    for j in range(rings + 1):
        z = z_lo + (z_hi - z_lo) * j / rings
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            row.append(bm.verts.new((s3[0] + 0.28 * math.cos(a), s3[1] + 0.22 * math.sin(a), z)))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    ob = new_obj('Vest', bm)
    off = 0.03 if F['vest_kind'] == 'plate' else 0.022
    shrink_onto(ob, tgt, off)
    apply_mod(ob, 'SMOOTH', factor=1.0, iterations=12, use_x=True, use_y=True, use_z=False)
    shrink_onto(ob, tgt, off, mode='OUTSIDE')
    apply_mod(ob, 'SOLIDIFY', thickness=0.016, offset=1.0)
    apply_mod(ob, 'SUBSURF', levels=1)
    set_color(ob, F['vest'])
    PIECES.append((ob, 'spine_03'))
    # as alças: por cima de cada ombro, da borda de cima da frente à de trás (raios no corpo de fora pra dentro, no plano YZ)
    z_top = Z_NECK - 0.035
    for sd in (-1.0, 1.0):
        x = s3[0] + sd * 0.095
        pts = []
        for ang in (-80.0, -55.0, -28.0, 0.0, 28.0, 55.0, 80.0):
            a = math.radians(ang)
            o = Vector((x, s3[1] + math.sin(a) * 0.6, z_top - 0.10 + math.cos(a) * 0.6))
            hit = BVH.ray_cast(o, (Vector((x, s3[1], z_top - 0.10)) - o).normalized(), 1.0)
            if hit[0] is not None:
                pts.append(hit[0] + hit[1] * (off + 0.006))
        if len(pts) < 4:
            continue
        sbm = bmesh.new()
        hw = 0.026
        prev = None
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            sdir = Vector((1.0, 0.0, 0.0))
            n = t.cross(sdir).normalized()
            vs = [sbm.verts.new(p - sdir * hw), sbm.verts.new(p + sdir * hw), sbm.verts.new(p + sdir * hw + n * 0.01), sbm.verts.new(p - sdir * hw + n * 0.01)]
            if prev:
                for k in range(4):
                    sbm.faces.new((prev[k], prev[(k + 1) % 4], vs[(k + 1) % 4], vs[k]))
            prev = vs
        strap = new_obj('Strap_%d' % int(sd), sbm)
        set_color(strap, F['vest'])
        PIECES.append((strap, 'spine_03'))
    # os porta-carregadores: 3 na frente embaixo (o colete USA/China), 4 menores no colete de bolsos da GLA
    chest = bh('spine_04')
    n = 3 if F['vest_kind'] == 'plate' else 4
    w = 0.075 if F['vest_kind'] == 'plate' else 0.06
    zc = z_lo + 0.08
    for k in range(n):
        x = chest[0] + (k - (n - 1) / 2.0) * (w + 0.008)
        hit = surface((x, chest[1] - 0.6, zc), (0.0, 1.0, 0.0))
        if hit is None:
            continue
        pc = rounded_box('Pouch_%d' % k, (w, 0.045, 0.12), (x, hit.y - off - 0.016 - 0.020, zc), 0.008, 2)
        set_color(pc, F['pouch'])
        PIECES.append((pc, 'spine_03'))
    if F['vest_kind'] == 'plate':
        hit = surface((chest[0], chest[1] - 0.6, zc + 0.14), (0.0, 1.0, 0.0))
        if hit is not None:
            pc = rounded_box('Admin', (0.17, 0.035, 0.09), (chest[0], hit.y - off - 0.016 - 0.015, zc + 0.14), 0.008, 2)
            set_color(pc, F['pouch'])
            PIECES.append((pc, 'spine_03'))
    bpy.data.objects.remove(tgt)
    log('colete', F['vest_kind'], len(ob.data.vertices), 'vértices')


def neck_ring():
    """(01/11) O xale grosso do rebelde (o shemagh claro com o quadriculado escuro) ou a gola alta do colete russo: um anel em volta da
    base do pescoço, preso no peito."""
    if not (F.get('scarf') or F.get('collar')):
        return
    n = bh('neck_01')
    bm = bmesh.new()
    big, tube = (0.10, 0.045) if F.get('scarf') else (0.095, 0.03)
    bmesh.ops.create_cone(bm, cap_ends=False, segments=32, radius1=big + tube, radius2=big + tube * 0.6, depth=tube * 2.2)
    for v in bm.verts:
        v.co = Vector((n[0] + v.co.x, n[1] + v.co.y * 0.92, n[2] - 0.035 + v.co.z))
    ob = new_obj('NeckRing', bm)
    apply_mod(ob, 'SOLIDIFY', thickness=0.02, offset=1.0)
    apply_mod(ob, 'SUBSURF', levels=1)
    if F.get('scarf'):
        set_color(ob, (0.46, 0.43, 0.36))
        # o quadriculado do shemagh: as faixas escuras em grade
        color_where(ob, lambda p: (int((p.x + 5.0) / 0.03) + int((p.z + 5.0) / 0.03)) % 3 == 0, (0.16, 0.13, 0.10), 0.0)
    else:
        set_color(ob, F['vest'])
    PIECES.append((ob, 'spine_03'))
    log('anel do pescoço', 'xale' if F.get('scarf') else 'gola')


def face_pieces():
    """(01/11, ele: "sem rosto ainda") O nariz e as orelhas (o volume que a pintura não dá) e o que cada um usa na cara: os óculos de
    proteção do americano, o lenço do rebelde cobrindo do nariz pra baixo (o rosto do GLA: só os olhos de fora)."""
    kind = F['helmet_kind']
    face = F.get('face')
    skin = (0.05, 0.055, 0.045) if face == 'balaclava' else F['skin']
    hit = surface((HEAD_C[0], HEAD_C[1] - 0.5, Z_NOSE + 0.016), (0.0, 1.0, 0.0))
    if hit is not None and face != 'veil':
        nose = rounded_box('Nose', (0.024, 0.026, 0.042), (HEAD_C[0], hit.y - 0.008, Z_NOSE + 0.018), 0.0095, 2)
        set_color(nose, skin)
        PIECES.append((nose, 'head'))
    if kind not in ('wrap', 'turban', 'ushanka'):
        for sx in (-1.0, 1.0):
            h = surface((HEAD_C[0] + sx * 0.5, HEAD_C[1] + 0.005, Z_EYE - 0.014), (-sx, 0.0, 0.0))
            if h is None:
                continue
            ear = rounded_box('Ear_%d' % int(sx), (0.014, 0.032, 0.052), (h.x + sx * 0.004, HEAD_C[1] + 0.008, Z_EYE - 0.014), 0.006, 2)
            set_color(ear, skin)
            PIECES.append((ear, 'head'))
    if face == 'glasses':
        h = surface((HEAD_C[0], HEAD_C[1] - 0.5, Z_EYE), (0.0, 1.0, 0.0))
        if h is not None:
            g = rounded_box('Glasses', (0.125, 0.014, 0.03), (HEAD_C[0], h.y - 0.014, Z_EYE + 0.002), 0.006, 2)
            set_color(g, (0.03, 0.03, 0.032))
            PIECES.append((g, 'head'))
    if face == 'veil':
        # o lenço: uma casca encolhida no rosto e no pescoço a ~1,4 cm, do pé do nariz... do meio do nariz pra baixo, solta embaixo do queixo
        tgt = part_target('VeilTarget', lambda i: DOM[i] == 'head' or str(DOM[i]).startswith('neck'))
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=32, radius=0.35)
        for v in bm.verts:
            v.co += Vector((HEAD_C[0], HEAD_C[1], (HEAD_CHIN + Z_EYE) / 2.0))
        ob = new_obj('Veil', bm)
        shrink_onto(ob, tgt, 0.014)
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        kill = [v for v in bm.verts if not (v.co.z < Z_EYE - 0.024 and v.co.z > Z_NECK - 0.05 and v.co.y < HEAD_C[1] + 0.045)]
        bmesh.ops.delete(bm, geom=kill, context='VERTS')
        bm.to_mesh(ob.data)
        bm.free()
        apply_mod(ob, 'CORRECTIVE_SMOOTH', factor=0.8, iterations=4, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
        apply_mod(ob, 'SOLIDIFY', thickness=0.008, offset=1.0)
        set_color(ob, F['veil'])
        if F['veil'][0] > 0.3:
            color_where(ob, lambda p: (int((p.x + 5.0) / 0.03) + int((p.z + 5.0) / 0.03)) % 3 == 0, (0.16, 0.13, 0.10), 0.0)
        PIECES.append((ob, 'head'))
        bpy.data.objects.remove(tgt)
    log('rosto', face)


def waistcoat():
    """(01/11) O colete civil aberto do rebelde (sem placa, sem bolso de carregador): o tubo de pano encolhido no tronco a ~1,2 cm, do
    meio da barriga ao peito, com a frente aberta no meio."""
    torso_bones = {'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_l', 'clavicle_r', 'pelvis'}
    tgt = part_target('TorsoTarget', lambda i: DOM[i] in torso_bones)
    s3 = bh('spine_03')
    z_lo = Z_SPINE2 - 0.12
    z_hi = Z_NECK - 0.07
    seg, rings = 48, 10
    bm = bmesh.new()
    grid = []
    for j in range(rings + 1):
        z = z_lo + (z_hi - z_lo) * j / rings
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            row.append(bm.verts.new((s3[0] + 0.28 * math.cos(a), s3[1] + 0.22 * math.sin(a), z)))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    ob = new_obj('Waistcoat', bm)
    shrink_onto(ob, tgt, 0.012)
    apply_mod(ob, 'SMOOTH', factor=1.0, iterations=8, use_x=True, use_y=True, use_z=False)
    shrink_onto(ob, tgt, 0.012, mode='OUTSIDE')
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    kill = [v for v in bm.verts if v.co.y < s3[1] and abs(v.co.x - s3[0]) < 0.05]
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    bm.to_mesh(ob.data)
    bm.free()
    apply_mod(ob, 'SOLIDIFY', thickness=0.01, offset=1.0)
    apply_mod(ob, 'SUBSURF', levels=1)
    set_color(ob, F['vest'])
    jitter_color(ob, 0.08, 0.05)
    PIECES.append((ob, 'spine_03'))
    bpy.data.objects.remove(tgt)
    log('colete civil aberto')


def bandolier():
    """(01/11) A bandoleira de couro do rebelde (do ombro esquerdo ao quadril direito, com os carregadores em bolsinhas) ou a alça da bolsa
    do RPG (a mesma diagonal, sem as bolsinhas): o que se vê de cima é a faixa cruzando o peito e passando por cima do ombro."""
    if not (F.get('bandolier') or F.get('strap')):
        return
    cl = bh('upperarm_l')   # (o ombro: a clavícula nasce no meio do peito)
    tr = bh('thigh_r')
    cx = float(HEAD_C[0])
    x0, x1 = cx + 0.75 * (cl[0] - cx), cx + 1.6 * (tr[0] - cx)
    pts = []
    for i in range(14):
        t = i / 13.0
        x = x0 * (1.0 - t) + x1 * t
        z = (Z_NECK - 0.015) * (1.0 - t) + (Z_PELVIS + 0.05) * t
        h = surface((x, HEAD_C[1] - 0.6, z), (0.0, 1.0, 0.0))
        if h is not None:
            pts.append(h + Vector((0.0, -0.022, 0.0)))
    top = surface((x0, HEAD_C[1], Z_NECK + 0.3), (0.0, 0.0, -1.0))
    if top is not None:
        pts.insert(0, top + Vector((0.0, 0.0, 0.02)))
    if len(pts) < 4:
        return
    sbm = bmesh.new()
    hw = 0.024
    prev = None
    for i, p in enumerate(pts):
        tg = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        sdir = tg.cross(Vector((0.0, -1.0, 0.0))).normalized()
        n = sdir.cross(tg).normalized()
        vs = [sbm.verts.new(p - sdir * hw), sbm.verts.new(p + sdir * hw), sbm.verts.new(p + sdir * hw + n * 0.008), sbm.verts.new(p - sdir * hw + n * 0.008)]
        if prev:
            for k in range(4):
                sbm.faces.new((prev[k], prev[(k + 1) % 4], vs[(k + 1) % 4], vs[k]))
        prev = vs
    strap = new_obj('Bandolier', sbm)
    set_color(strap, (0.12, 0.08, 0.05))
    PIECES.append((strap, 'spine_03'))
    if F.get('bandolier'):
        for i in range(2, len(pts) - 2, 2):
            p = pts[i]
            pc = rounded_box('Mag_%d' % i, (0.04, 0.026, 0.05), (p.x, p.y - 0.016, p.z), 0.008, 2)
            set_color(pc, (0.10, 0.065, 0.04))
            PIECES.append((pc, 'spine_03'))
    log('bandoleira' if F.get('bandolier') else 'alça da bolsa', len(pts), 'pontos')


def coat_skirt():
    """(01/11) A barra do casacão (até abaixo do joelho, a China) ou da túnica (até o joelho, a GLA): um tubo de seção quase oval em volta
    das duas pernas, mais largo descendo (a roda do pano), em duas metades presas cada uma na sua coxa. Parado lê como uma peça só (as
    metades se cruzam 10° na frente e atrás); andando abre na frente e atrás como o casaco de verdade (a fenda), sem atravessar a perna."""
    if not F.get('coat'):
        return
    pel = bh('pelvis')
    cx = float(pel[0])
    z_top = Z_PELVIS + 0.065
    z_bot = Z_KNEE - 0.08 if F['coat'] == 'long' else Z_KNEE + 0.01
    flare = 0.075 if F['coat'] == 'long' else 0.045
    legs = np.array([str(d).startswith('thigh') or d == 'pelvis' for d in DOM])
    rings, seg, n_exp = 8, 40, 2.6
    shape = []
    for j in range(rings + 1):
        t = j / rings
        z = z_top + (z_bot - z_top) * t
        sl = legs & (np.abs(VERTS[:, 2] - z) < 0.03)
        P = VERTS[sl] if sl.any() else VERTS[legs]
        rx = float(np.abs(P[:, 0] - cx).max()) + 0.022 + flare * t
        yf, yb = float(P[:, 1].min()), float(P[:, 1].max())
        shape.append((z, rx, (yf + yb) / 2.0, (yb - yf) / 2.0 + 0.022 + flare * t, t))
    tl = float(bh('thigh_l')[0]) - cx
    for side in (1.0, -1.0):
        bone = 'thigh_l' if (side > 0) == (tl > 0) else 'thigh_r'
        a0, a1 = (-100.0, 100.0) if side > 0 else (80.0, 280.0)
        bm = bmesh.new()
        grid = []
        for z, rx, yc, ry, t in shape:
            row = []
            for k in range(seg + 1):
                a = math.radians(a0 + (a1 - a0) * k / seg)
                ca, sa = math.cos(a), math.sin(a)
                # (01/11, o acabamento) as dobras do pano caindo (mais fundas na barra) e a bainha que não é reta
                fold = 1.0 + (0.05 if F['coat'] == 'long' else 0.035) * t * math.sin(a * 9.0 + 1.3 * math.sin(a * 3.0 + side))
                x = cx + rx * fold * math.copysign(abs(ca) ** (2.0 / n_exp), ca)
                y = yc + ry * fold * math.copysign(abs(sa) ** (2.0 / n_exp), sa)
                zz = z + (0.018 * math.sin(a * 5.0 + 2.0 * side) * t ** 4)
                row.append(bm.verts.new((x, y, zz)))
            grid.append(row)
        for j in range(rings):
            for k in range(seg):
                bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
        ob = new_obj('Skirt_%s' % bone, bm)
        apply_mod(ob, 'SOLIDIFY', thickness=0.012, offset=1.0)
        apply_mod(ob, 'SUBSURF', levels=1)
        set_color(ob, F['coat_color'])
        jitter_color(ob, 0.06, 0.06)
        PIECES.append((ob, bone))
    if F['coat'] == 'long' or F.get('belt_front'):
        # o cinto largo por cima do casaco e as cartucheiras na frente (o fuzileiro sem colete)
        bbm = bmesh.new()
        bmesh.ops.create_cone(bbm, cap_ends=False, segments=40, radius1=1.0, radius2=1.0, depth=0.055)
        z0 = Z_PELVIS + 0.09
        trunk = np.array([d in ('pelvis', 'spine_01', 'spine_02') or str(d).startswith('thigh') for d in DOM])   # (sem as mãos do A)
        sl = trunk & (np.abs(VERTS[:, 2] - z0) < 0.03)
        P = VERTS[sl]
        rx = float(np.abs(P[:, 0] - cx).max()) + 0.035
        yf, yb = float(P[:, 1].min()), float(P[:, 1].max())
        for v in bbm.verts:
            v.co = Vector((cx + v.co.x * rx, (yf + yb) / 2.0 + v.co.y * ((yb - yf) / 2.0 + 0.035), z0 + v.co.z))
        belt = new_obj('CoatBelt', bbm)
        apply_mod(belt, 'SOLIDIFY', thickness=0.012, offset=1.0)
        set_color(belt, F['belt'])
        PIECES.append((belt, 'pelvis'))
        for k in (-1.5, -0.5, 0.5, 1.5):
            x = cx + k * 0.07
            pc = rounded_box('BeltPouch_%d' % int(k * 2), (0.06, 0.04, 0.08), (x, yf - 0.05, z0 - 0.005), 0.008, 2)
            set_color(pc, F['pouch'])
            PIECES.append((pc, 'pelvis'))
    log('casaco', F['coat'], 'de %.2f a %.2f m' % (z_top, z_bot))


def groin_plate():
    """(01/11) A placa da virilha do colete russo (o volume do 6B45 embaixo do colete)."""
    if not F.get('groin'):
        return
    pel = bh('pelvis')
    hit = surface((pel[0], pel[1] - 0.6, Z_PELVIS - 0.02), (0.0, 1.0, 0.0))
    y = (hit.y if hit is not None else pel[1] - 0.12) - 0.035
    ob = rounded_box('Groin', (0.2, 0.03, 0.17), (pel[0], y, Z_PELVIS - 0.01), 0.012, 2)
    set_color(ob, F['vest'])
    PIECES.append((ob, 'pelvis'))


def cylinder(name, a, b, r, seg=16, r_end=None):
    """Um cilindro (ou tronco de cone) de a até b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r if r_end is None else r_end, depth=d.length)
    ob = new_obj(name, bm)
    q = d.normalized().to_track_quat('Z', 'Y')
    ob.data.transform(Matrix.Translation((a + b) / 2.0) @ q.to_matrix().to_4x4())
    return ob


def back_load():
    """(01/11) A carga nas costas de cada tipo, a assinatura dele vista de cima."""
    kind = F.get('back')
    back = bh('spine_04')
    hit = surface((back[0], back[1] + 0.5, back[2] - 0.05), (0.0, -1.0, 0.0))
    yb = (hit.y if hit is not None else back[1] + 0.12) + (0.03 if F['vest_kind'] != 'none' else 0.0) + 0.016
    zt = Z_NECK
    if kind == 'pack':
        ob = rounded_box('Pack', (0.27, 0.16, 0.34), (back[0], yb + 0.085, back[2] - 0.07), 0.035, 3)
        set_color(ob, F['pack'])
        PIECES.append((ob, 'spine_03'))
    elif kind == 'tube':
        # o Javelin reserva: o tubo de 1,1 m na diagonal, do quadril direito ao ombro esquerdo, por cima de uma mochila pequena
        ob = rounded_box('Pack', (0.24, 0.12, 0.26), (back[0], yb + 0.065, back[2] - 0.1), 0.03, 3)
        set_color(ob, F['pack'])
        PIECES.append((ob, 'spine_03'))
        a = (back[0] + 0.30, yb + 0.20, back[2] - 0.50)
        b = (back[0] - 0.30, yb + 0.20, back[2] + 0.44)
        tube = cylinder('SpareTube', a, b, 0.07, 20)
        set_color(tube, (0.10, 0.11, 0.085))
        PIECES.append((tube, 'spine_03'))
        for k, p in enumerate((a, b)):
            cap = cylinder('TubeCap_%d' % k, Vector(p) - (Vector(b) - Vector(a)).normalized() * 0.05 * (1 - 2 * k), Vector(p) + (Vector(b) - Vector(a)).normalized() * 0.04 * (1 - 2 * k), 0.085, 20)
            set_color(cap, (0.06, 0.06, 0.05))
            PIECES.append((cap, 'spine_03'))
    elif kind == 'rockets3':
        # o Tank Hunter: a armação nas costas com 3 foguetes de pé, as pontas ~25 cm acima dos ombros
        frame = rounded_box('RocketFrame', (0.30, 0.06, 0.40), (back[0], yb + 0.04, back[2] - 0.08), 0.01, 1)
        set_color(frame, (0.05, 0.05, 0.045))
        PIECES.append((frame, 'spine_03'))
        for k, dx in enumerate((-0.10, 0.0, 0.10)):
            z0, z1 = back[2] - 0.30, zt + 0.24 + (0.04 if dx == 0.0 else 0.0)
            body = cylinder('Rocket_%d' % k, (back[0] + dx, yb + 0.11, z0), (back[0] + dx, yb + 0.11, z1), 0.05, 16)
            set_color(body, (0.06, 0.08, 0.045))
            PIECES.append((body, 'spine_03'))
            nose = cylinder('RocketNose_%d' % k, (back[0] + dx, yb + 0.11, z1), (back[0] + dx, yb + 0.11, z1 + 0.13), 0.062, 16, 0.012)
            set_color(nose, (0.12, 0.13, 0.09))
            PIECES.append((nose, 'spine_03'))
    elif kind == 'warheads3':
        # o RPG: a bolsa de lona com 3 granadas do RPG-7 (o cabo e a ogiva cônica verde) acima dos ombros
        bag = rounded_box('RpgBag', (0.28, 0.14, 0.30), (back[0], yb + 0.075, back[2] - 0.12), 0.04, 3)
        set_color(bag, (0.20, 0.18, 0.13))
        PIECES.append((bag, 'spine_03'))
        for k, (dx, tilt) in enumerate(((-0.09, -0.08), (0.0, 0.0), (0.09, 0.08))):
            z0 = back[2] - 0.05
            z1 = zt + 0.10
            p0 = Vector((back[0] + dx, yb + 0.09, z0))
            p1 = Vector((back[0] + dx + tilt * 0.4, yb + 0.11, z1))
            stem = cylinder('RpgStem_%d' % k, p0, p1, 0.022, 12)
            set_color(stem, (0.10, 0.10, 0.08))
            PIECES.append((stem, 'spine_03'))
            dirv = (p1 - p0).normalized()
            w0 = p1
            w1 = p1 + dirv * 0.11
            war = cylinder('RpgWar_%d' % k, w0, w1, 0.042, 16, 0.046)
            set_color(war, (0.11, 0.14, 0.07))
            PIECES.append((war, 'spine_03'))
            cone = cylinder('RpgCone_%d' % k, w1, w1 + dirv * 0.16, 0.046, 16, 0.008)
            set_color(cone, (0.11, 0.14, 0.07))
            PIECES.append((cone, 'spine_03'))
    elif kind == 'tripod':
        # o Kornet: o tubo do míssil de pé nas costas e o tripé dobrado ao lado (3 pernas finas), por cima da mochila
        ob = rounded_box('Pack', (0.26, 0.15, 0.30), (back[0], yb + 0.08, back[2] - 0.09), 0.035, 3)
        set_color(ob, F['pack'])
        PIECES.append((ob, 'spine_03'))
        tube = cylinder('KornetTube', (back[0] - 0.08, yb + 0.20, back[2] - 0.45), (back[0] - 0.05, yb + 0.20, zt + 0.40), 0.075, 20)
        set_color(tube, (0.09, 0.10, 0.08))
        PIECES.append((tube, 'spine_03'))
        for k, dx in enumerate((0.08, 0.12, 0.16)):
            leg = cylinder('TripodLeg_%d' % k, (back[0] + dx, yb + 0.17, back[2] - 0.40), (back[0] + dx - 0.05, yb + 0.17, zt + 0.25), 0.014, 8)
            set_color(leg, (0.05, 0.05, 0.045))
            PIECES.append((leg, 'spine_03'))
        head = rounded_box('TripodHead', (0.12, 0.08, 0.08), (back[0] + 0.09, yb + 0.17, zt + 0.26), 0.01, 1)
        set_color(head, (0.05, 0.05, 0.045))
        PIECES.append((head, 'spine_03'))
    elif kind == 'sack':
        # o trabalhador: o saco grande no ombro direito e a pá atravessada nas costas
        sbm = bmesh.new()
        bmesh.ops.create_uvsphere(sbm, u_segments=20, v_segments=12, radius=1.0)
        cxs = back[0] - 0.12
        for v in sbm.verts:
            v.co = Vector((cxs + v.co.x * 0.17, yb + 0.14 + v.co.y * 0.15, zt - 0.02 + v.co.z * 0.24))
        sack = new_obj('Sack', sbm)
        apply_mod(sack, 'SUBSURF', levels=1)
        set_color(sack, (0.34, 0.28, 0.18))
        jitter_color(sack, 0.12, 0.05)
        PIECES.append((sack, 'spine_03'))
        a = (back[0] + 0.28, yb + 0.06, back[2] - 0.45)
        b = (back[0] - 0.05, yb + 0.06, zt + 0.30)
        stick = cylinder('Shovel', a, b, 0.016, 8)
        set_color(stick, (0.22, 0.15, 0.08))
        PIECES.append((stick, 'spine_03'))
        blade = rounded_box('ShovelBlade', (0.18, 0.02, 0.24), (a[0] + 0.03, a[1], a[2] - 0.08), 0.01, 1)
        set_color(blade, (0.10, 0.10, 0.10))
        PIECES.append((blade, 'spine_03'))
    if kind:
        log('costas', kind)


def belt_pouches():
    pel = bh('pelvis')
    for side in (-1.0, 1.0):
        x = pel[0] + side * 0.17
        hit = surface((x + side * 0.4, pel[1], Z_PELVIS + 0.06), (-side, 0.0, 0.0))
        if hit is None:
            continue
        ob = rounded_box('Hip_%d' % int(side), (0.04, 0.10, 0.11), (hit.x + side * 0.025, pel[1] + 0.01, Z_PELVIS + 0.04), 0.008, 2)
        set_color(ob, F['pouch'])
        PIECES.append((ob, 'pelvis'))


def export_pieces():
    """Junta as peças por osso, uma malha por osso (SM_Soldier_<F>_<osso>.glb), com a origem na cabeça do osso."""
    by_bone = {}
    for ob, bone in PIECES:
        by_bone.setdefault(bone, []).append(ob)
    meta = {'faction': TAG, 'pieces': []}
    for bone, obs in by_bone.items():
        bpy.ops.object.select_all(action='DESELECT')
        for ob in obs:
            ob.select_set(True)
        bpy.context.view_layer.objects.active = obs[0]
        if len(obs) > 1:
            bpy.ops.object.join()
        ob = bpy.context.view_layer.objects.active
        name = 'SM_Soldier_%s_%s' % (TAG.upper(), bone)
        ob.name = name
        ob.data.name = name
        # a origem na cabeça do osso (o jogo prende a peça ali e põe a diferença de giro da pose de repouso)
        o = Vector(tuple(bh(bone)))
        ob.data.transform(Matrix.Translation(-o))
        ob.location = (0.0, 0.0, 0.0)
        mat = bpy.data.materials.get('M_Soldier') or bpy.data.materials.new('M_Soldier')
        if len(ob.data.materials) == 0:
            ob.data.materials.append(mat)
        else:
            ob.data.materials[0] = mat
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        path = os.path.join(OUT, name + '.glb')
        bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format='GLB', export_vertex_color='ACTIVE',
                                  export_all_vertex_colors=False, export_materials='PLACEHOLDER', export_apply=True)
        ob.data.transform(Matrix.Translation(o))
        # a origem no espaço do corpo da Unreal (cm): (x, −y, z) × 100
        meta['pieces'].append({'mesh': name, 'bone': bone, 'origin_cm': [o.x * 100.0, -o.y * 100.0, o.z * 100.0],
                               'verts': len(ob.data.vertices)})
        log('peça', name, 'no osso', bone, len(ob.data.vertices), 'vértices')
    return meta


def check_sheet():
    """A folha de conferência: o corpo com a farda e as peças, de frente, de lado, de costas e de cima (o Cycles na CPU: sem a placa)."""
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 16
    sc.render.resolution_x, sc.render.resolution_y = 480, 720
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.45, 0.32, 1.0)
    w.node_tree.nodes['Background'].inputs[1].default_value = 0.7
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sc.collection.objects.link(sun)
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50.0), 0.0, math.radians(35.0))

    def principled(name, color_node):
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        nt.nodes.clear()
        out = nt.nodes.new('ShaderNodeOutputMaterial')
        bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
        bsdf.inputs['Roughness'].default_value = 0.85
        src = color_node(nt)
        nt.links.new(src, bsdf.inputs['Base Color'])
        nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
        return mat

    for slot, path in TEXFILES.items():
        def tex_node(nt, p=path):
            t = nt.nodes.new('ShaderNodeTexImage')
            t.image = bpy.data.images.load(p)
            t.image.alpha_mode = 'CHANNEL_PACKED'
            return t.outputs['Color']
        if slot < len(BODY.data.materials):
            BODY.data.materials[slot] = principled('body_%d' % slot, tex_node)

    def vc_node(nt):
        v = nt.nodes.new('ShaderNodeVertexColor')
        v.layer_name = 'Col'
        return v.outputs['Color']
    vmat = principled('pieces', vc_node)
    for ob, _ in PIECES:
        if len(ob.data.materials) == 0:
            ob.data.materials.append(vmat)
        for k in range(len(ob.data.materials)):
            ob.data.materials[k] = vmat
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cz = float((VERTS[:, 2].min() + VERTS[:, 2].max()) / 2.0)
    shots = []
    for tag, loc in (('frente', (0.0, -4.2, cz)), ('lado', (4.2, 0.0, cz)), ('costas', (0.0, 4.2, cz)), ('cima', (0.0, -2.0, cz + 4.0))):
        cam.location = Vector(loc)
        d = Vector((0.0, 0.0, cz)) - cam.location
        cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = 50
        p = os.path.join(OUT, 'check_%s_%s.png' % (TAG, tag))
        sc.render.filepath = p
        bpy.ops.render.render(write_still=True)
        shots.append(p)
    log('conferência', shots)


TEXFILES = paint_textures()
helmet()
vest()
neck_ring()
back_load()
coat_skirt()
groin_plate()
face_pieces()
bandolier()
if F['vest_kind'] in ('plate', 'rig'):
    belt_pouches()
for ob, _ in PIECES:
    pass
try:
    check_sheet()
except Exception as e:   # a conferência não segura a saída
    log('conferência falhou:', e)
META = export_pieces()
META['textures'] = {str(k): os.path.basename(v) for k, v in TEXFILES.items()}
with open(os.path.join(OUT, 'soldier_%s.json' % TAG), 'w', encoding='utf-8') as f:
    json.dump(META, f, indent=1, ensure_ascii=False)
log('pronto', os.path.join(OUT, 'soldier_%s.json' % TAG))
