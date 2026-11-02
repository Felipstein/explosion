"""O soldado de verdade (01/11/2026, a 3ª volta da infantaria; o estudo: docs/ESTUDO_INFANTARIA_MODELO.md, o caminho (a)).

O pedido dele, depois de reprovar duas vezes o boneco com peças: "tá achando que é jogo de lego? faz direito, faz detalhado, faz bem
pensado, com carinho". O defeito medido: formas primitivas sem chanfro nem dobra, o pano que não segue o corpo (a barra presa nas coxas
rasgava a cada passo) e o boneco liso (o Manny com os músculos à mostra e sem rosto).

Aqui o soldado é UMA malha com pesos no esqueleto do Manny (as animações tocam sem retarget):
1. O rosto esculpido na própria malha da cabeça (4.831 vértices): o nariz, a sobrancelha, a órbita, a maçã do rosto, os lábios, o queixo
   e o maxilar, por deslocamentos suaves (gaussianos) no quadro da cabeça medida.
2. A farda É o corpo: a pele das regiões vestidas é inflada pela folga do pano (a camisa 1 a 1,6 cm, a calça 1,2 a 1,8 cm, a bota, a
   luva), alisada (o pano esconde o músculo: o peito e o abdome do boneco somem) e dobrada (as dobras do cotovelo, do joelho, a calça
   franzida em cima da bota, o pano juntando acima do cinto). Como os vértices são os do corpo, os pesos são os dele: a farda deforma
   exatamente como a pele, sem rasgo. O punho, a barra e o cano da bota saem do degrau entre as folgas.
3. (etapa 2) O equipamento com chanfro, encostado no pano, na mesma malha.

Rodar (Blender sem janela, só CPU):
  blender -b --factory-startup --python Game/Tools/soldier_outfit.py -- <SKM_Manny_Simple.fbx> <pasta de saída> <facção> <tipo>
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
FBX = ARGS[0]
OUT = os.path.abspath(ARGS[1])
FACTION = (ARGS[2] if len(ARGS) > 2 else 'usa').lower()
ROLE = (ARGS[3] if len(ARGS) > 3 else 'rifle').lower()
TAG = FACTION if ROLE == 'rifle' else FACTION + '_' + ROLE
os.makedirs(OUT, exist_ok=True)
TEX = 2048


def log(*a):
    print('OUTFIT', *a, flush=True)


# (01/11) o kit de cada tipo: a folga da camisa/casaco e da manga, a camuflagem, o equipamento e a cabeça
KITS = {
    ('usa', 'rifle'): dict(shirt_off=0.010, sleeve_off=0.015, camo=True, gear='usa', head='ach'),
    ('china', 'rifle'): dict(shirt_off=0.022, sleeve_off=0.024, camo=False, gear='china', head='ushanka'),
    # (01/11, ele: "nos GLA não tem que ter bagulho militar, os caras são rebeldes") a roupa civil: a túnica solta (kameez) e a calça larga
    # (01/11) a Rússia: a EMR pixelada, o colete 6B45 (placas grandes, gola alta, virilha, ombreiras), o 6B47 com os óculos, a balaclava
    ('russia', 'rifle'): dict(shirt_off=0.012, sleeve_off=0.016, camo='emr', gear='russia', head='6b47', balaclava=True),
    ('gla', 'rifle'): dict(shirt_off=0.018, sleeve_off=0.022, camo=False, gear='gla', head='wrap', pants_off=0.030, bare_hands=True),
}
K = KITS.get((FACTION, ROLE), KITS[('usa', 'rifle')])


# ----------------------------------------------------------------------------------------------------------------------------------
# o corpo
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX, automatic_bone_orientation=False, ignore_leaf_bones=False)
ARM = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
BODY = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
ME = BODY.data
MW = BODY.matrix_world
MWI = MW.inverted()
# (01/11) os sulcos do manequim são costuras com os vértices repetidos (o alisado não atravessa): solda por posição (0,5 mm; a malha em
# cm), os pesos e as UVs ficam (as UVs são por canto)
_bm = bmesh.new()
_bm.from_mesh(ME)
_n0 = len(_bm.verts)
bmesh.ops.remove_doubles(_bm, verts=_bm.verts, dist=0.05)
_bm.to_mesh(ME)
_bm.free()
print('OUTFIT soldados %d vértices repetidos' % (_n0 - len(ME.vertices)), flush=True)


def bh(name):
    return np.array(tuple(ARM.matrix_world @ ARM.data.bones[name].head_local))


GROUPS = {g.index: g.name for g in BODY.vertex_groups}
DOM = np.array([GROUPS[max(v.groups, key=lambda g: g.weight).group] if len(v.groups) else '-' for v in ME.vertices])
P0 = np.array([tuple(MW @ v.co) for v in ME.vertices], dtype=np.float64)          # o corpo em metros, no mundo
ME.calc_loop_triangles()
N0 = np.array([tuple((MW.to_3x3() @ v.normal).normalized()) for v in ME.vertices], dtype=np.float64)
FWD = np.array((0.0, -1.0, 0.0))   # a frente do corpo (o soldier_dress confere pelos dedos do pé)

Z_ANKLE = (bh('foot_l')[2] + bh('foot_r')[2]) / 2.0
Z_KNEE = (bh('calf_l')[2] + bh('calf_r')[2]) / 2.0
Z_PELVIS = bh('pelvis')[2]
Z_NECK = bh('neck_01')[2]
HP = P0[DOM == 'head']
HEAD_C = (HP.min(0) + HP.max(0)) / 2.0
HEAD_TOP = float(HP[:, 2].max())
HF = HP[HP[:, 1] < HEAD_C[1] - 0.03]
HEAD_CHIN = float(HF[:, 2].min())
HEAD_H = HEAD_TOP - HEAD_CHIN
Z_EYE = HEAD_CHIN + 0.47 * HEAD_H
Z_NOSE = HEAD_CHIN + 0.30 * HEAD_H
Z_MOUTH = HEAD_CHIN + 0.19 * HEAD_H
FRONT_Y = float(HP[:, 1].min())   # a ponta da frente da cabeça
log('cabeça %.3f m, olhos %.3f, nariz %.3f, boca %.3f, frente y %.3f' % (HEAD_H, Z_EYE, Z_NOSE, Z_MOUTH, FRONT_Y))


def starts(prefixes):
    m = np.zeros(len(DOM), dtype=bool)
    for p in prefixes:
        m |= np.char.startswith(DOM.astype(str), p)
    return m


def g2(x, s):
    return np.exp(-0.5 * (x / s) ** 2)


# ----------------------------------------------------------------------------------------------------------------------------------
# 1. o rosto esculpido (deslocamentos no quadro da cabeça: x pro lado, f pra frente, z pra cima)
def sculpt_face(P):
    head = (DOM == 'head') | starts(['neck'])
    x = P[:, 0] - HEAD_C[0]
    z = P[:, 2]
    f = -(P[:, 1] - HEAD_C[1])                       # a frente positiva
    front = np.clip((f - 0.02) / 0.05, 0.0, 1.0)     # só a face da frente
    d = np.zeros(len(P))                             # o deslocamento pra frente (m)
    side = np.zeros(len(P))                          # pro lado (m), com o sinal de x
    up = np.zeros(len(P))
    # o nariz: a ponte saindo da sobrancelha e a ponta embaixo, as asas
    zn0, zn1 = Z_EYE - 0.004, Z_NOSE + 0.004
    t = np.clip((zn0 - z) / max(zn0 - zn1, 1e-3), 0.0, 1.0)
    bridge = g2(x, 0.0075 + 0.006 * t) * (np.clip((zn0 + 0.008 - z) / 0.012, 0, 1)) * g2(z - (zn0 + zn1) / 2, (zn0 - zn1) * 0.62)
    d += 0.024 * bridge * (0.35 + 0.65 * t)
    d += 0.010 * g2(x, 0.009) * g2(z - (Z_NOSE + 0.008), 0.008)                     # a ponta
    d += 0.007 * (g2(x - 0.016, 0.006) + g2(x + 0.016, 0.006)) * g2(z - (Z_NOSE + 0.004), 0.006)   # as asas
    # a sobrancelha (o arco) e a órbita funda embaixo dela
    brow = g2(z - (Z_EYE + 0.017), 0.0075) * (g2(x - 0.03, 0.022) + g2(x + 0.03, 0.022))
    d += 0.009 * brow
    orb = g2(z - Z_EYE, 0.010) * (g2(x - 0.031, 0.014) + g2(x + 0.031, 0.014))
    d -= 0.008 * orb
    # o globo do olho: a pálpebra de cima saindo um pouco dentro da órbita
    d += 0.003 * g2(z - (Z_EYE + 0.004), 0.004) * (g2(x - 0.031, 0.009) + g2(x + 0.031, 0.009))
    # a maçã do rosto
    cheek = g2(z - (Z_EYE - 0.026), 0.014) * (g2(x - 0.045, 0.014) + g2(x + 0.045, 0.014))
    d += 0.006 * cheek
    side += 0.004 * cheek * np.sign(x)
    # os lábios (o de cima e o de baixo), o sulco entre eles e o do nariz à boca
    d += 0.0055 * g2(x, 0.017) * g2(z - (Z_MOUTH + 0.005), 0.0045)
    d += 0.0060 * g2(x, 0.016) * g2(z - (Z_MOUTH - 0.007), 0.0050)
    d -= 0.0025 * g2(x, 0.022) * g2(z - Z_MOUTH, 0.0018)
    d -= 0.0020 * g2(x, 0.004) * g2(z - (Z_NOSE - 0.012), 0.006)
    # o queixo e o maxilar (o ângulo da mandíbula pros lados)
    d += 0.008 * g2(x, 0.018) * g2(z - (HEAD_CHIN + 0.018), 0.011)
    jaw = g2(z - (HEAD_CHIN + 0.035), 0.016) * g2(np.abs(x) - 0.055, 0.012) * np.clip((f + 0.02) / 0.04, 0, 1)
    side += 0.006 * jaw * np.sign(x)
    # a testa: um pouco mais cheia; as têmporas um pouco fundas
    d += 0.004 * g2(z - (Z_EYE + 0.05), 0.02) * g2(x, 0.04)
    side -= 0.003 * g2(z - (Z_EYE + 0.02), 0.015) * g2(np.abs(x) - 0.068, 0.01) * np.sign(x)
    k = head & (front > 0.0)
    P[k, 1] -= (d * front)[k]
    P[k, 0] += (side * front)[k]
    P[k, 2] += (up * front)[k]
    log('rosto esculpido: %d vértices, o nariz sai %.1f cm' % (int(k.sum()), 100 * float((d * front)[k].max())))
    return P


# ----------------------------------------------------------------------------------------------------------------------------------
# 2. a farda: as regiões, a folga, o alisado e as dobras
def regions():
    ds = DOM.astype(str)
    z = P0[:, 2]
    hands = starts(['hand', 'index', 'middle', 'ring', 'pinky', 'thumb'])
    feet = starts(['foot', 'ball'])
    boot = feet | (starts(['calf']) & (z < Z_ANKLE + 0.16))
    legs = starts(['thigh', 'calf']) | (DOM == 'pelvis')
    pants = legs & ~boot
    arms = starts(['upperarm', 'lowerarm', 'clavicle'])
    torso = starts(['spine'])
    neck = starts(['neck'])
    head = DOM == 'head'
    shirt = arms | torso | (neck & (z < Z_NECK + 0.02))
    return dict(hands=hands, boot=boot, pants=pants, shirt=shirt, head=head | (neck & ~shirt))


def smooth_region(P, mask, iters, lam=0.5):
    """O alisado laplaciano só nas regiões vestidas (o pano esconde o músculo), com os vizinhos pela malha."""
    edges = np.array([tuple(e.vertices) for e in ME.edges], dtype=np.int64)
    n = len(P)
    for _ in range(iters):
        acc = np.zeros_like(P)
        cnt = np.zeros(n)
        np.add.at(acc, edges[:, 0], P[edges[:, 1]])
        np.add.at(acc, edges[:, 1], P[edges[:, 0]])
        np.add.at(cnt, edges[:, 0], 1)
        np.add.at(cnt, edges[:, 1], 1)
        avg = acc / np.maximum(cnt, 1)[:, None]
        P[mask] += lam * (avg[mask] - P[mask])
    return P


def smooth_taubin(P, mask, iters, lam=0.5, mu=-0.53):
    for _ in range(iters):
        P = smooth_region(P, mask, 1, lam)
        P = smooth_region(P, mask, 1, mu)
    return P


def vertex_normals(P):
    tris = np.array([tuple(t.vertices) for t in ME.loop_triangles], dtype=np.int64)
    a, b, c = P[tris[:, 0]], P[tris[:, 1]], P[tris[:, 2]]
    fn = np.cross(b - a, c - a)
    N = np.zeros_like(P)
    for k in range(3):
        np.add.at(N, tris[:, k], fn)
    N /= np.maximum(np.linalg.norm(N, axis=1), 1e-9)[:, None]
    return N


def bone_axis_t(P, b0, b1):
    """A posição ao longo do osso (0 na cabeça, 1 na ponta) e o ângulo em volta dele."""
    a, b = bh(b0), bh(b1)
    ax = b - a
    L = np.linalg.norm(ax)
    u = ax / L
    t = (P - a) @ u / L
    r = (P - a) - np.outer((P - a) @ u, u)
    ref = np.cross(u, np.array((0.0, 0.0, 1.0)))
    if np.linalg.norm(ref) < 1e-3:
        ref = np.cross(u, np.array((1.0, 0.0, 0.0)))
    ref /= np.linalg.norm(ref)
    ref2 = np.cross(u, ref)
    ang = np.arctan2(r @ ref2, r @ ref)
    return t, ang, L


def dress(P):
    R = regions()
    N = N0.copy()
    z = P[:, 2]
    off = np.zeros(len(P))
    off[R['shirt']] = K['shirt_off']
    sleeve = starts(['upperarm', 'lowerarm'])
    off[sleeve] = K['sleeve_off']
    off[R['pants']] = K.get('pants_off', 0.016)
    thigh = starts(['thigh'])
    off[thigh] = K.get('pants_off', 0.024) + 0.004        # a calça larga na coxa (o corte de combate; a do rebelde, mais)
    off[DOM == 'pelvis'] = K.get('pants_off', 0.024) + 0.004
    off[R['boot']] = 0.011
    off[starts(['ball']) | (starts(['foot']) & (P[:, 1] < bh('foot_l')[1] - 0.02))] = 0.015   # o bico da bota
    off[R['hands']] = 0.0 if K.get('bare_hands') else 0.004
    # (01/11) a folga muda devagar de uma região pra outra (o degrau camisa/manga e coxa/canela fazia o anel do manequim no ombro e
    # no joelho); o degrau fica só onde a roupa tem (o punho na luva, a calça no cano da bota)
    cloth_m = R['shirt'] | R['pants']
    offv = off[:, None].copy()
    offv = smooth_region(offv, cloth_m, 30, 0.5)
    off = offv[:, 0]
    # o alisado: o pano esconde o músculo (o peito, o abdome, a coxa) e os sulcos de painel do manequim (o joelho, o cotovelo, o
    # ombro): Taubin (λ/μ, sem encolher o volume), mais forte no tronco
    P = smooth_taubin(P, R['shirt'] & starts(['spine']), 40)
    P = smooth_taubin(P, R['shirt'] | R['pants'] | R['boot'], 45)
    P = smooth_taubin(P, (DOM == 'pelvis') | (starts(['thigh']) & (P0[:, 2] > Z_PELVIS - 0.18)), 60)   # a virilha: pano, não anatomia
    P = smooth_taubin(P, R['hands'], 6)
    N = vertex_normals(P)
    # as dobras: no cotovelo e no joelho em anéis (o pano junta na dobra da junta), a calça franzida em cima da bota, o pano juntando
    # acima do cinto, as diagonais do ombro ao peito; ruído fino em tudo
    fold = np.zeros(len(P))
    for side in ('l', 'r'):
        t, ang, L = bone_axis_t(P, 'lowerarm_' + side, 'hand_' + side)
        m = starts(['lowerarm']) & np.char.endswith(DOM.astype(str), '_' + side)
        fold[m] += 0.004 * np.sin(t[m] * 38.0 + 0.7 * np.sin(ang[m] * 2.0)) * g2(t[m] - 0.05, 0.22)
        t, ang, L = bone_axis_t(P, 'upperarm_' + side, 'lowerarm_' + side)
        m = starts(['upperarm']) & np.char.endswith(DOM.astype(str), '_' + side)
        fold[m] += 0.0035 * np.sin(t[m] * 30.0 + 0.9 * np.sin(ang[m] * 2.0 + 1.0)) * g2(t[m] - 0.92, 0.2)
        t, ang, L = bone_axis_t(P, 'calf_' + side, 'foot_' + side)
        m = starts(['calf']) & np.char.endswith(DOM.astype(str), '_' + side) & R['pants']
        fold[m] += 0.0045 * np.sin(t[m] * 34.0 + 0.8 * np.sin(ang[m] * 2.0)) * (g2(t[m] - 0.02, 0.15) + 1.3 * g2(t[m] - 0.8, 0.1))
        t, ang, L = bone_axis_t(P, 'thigh_' + side, 'calf_' + side)
        m = starts(['thigh']) & np.char.endswith(DOM.astype(str), '_' + side)
        fold[m] += 0.0035 * np.sin(ang[m] * 5.0 + t[m] * 3.0) * g2(t[m] - 0.5, 0.3)          # as dobras compridas da coxa
        fold[m] += 0.004 * np.sin(t[m] * 36.0) * g2(t[m] - 0.97, 0.07)                      # o joelho
    belt_z = Z_PELVIS + 0.09
    m = R['shirt'] & (np.abs(z - (belt_z + 0.05)) < 0.06)
    fold[m] += 0.004 * np.sin(P[m, 0] * 70.0 + 2.0 * np.sin(z[m] * 40.0)) * g2(z[m] - (belt_z + 0.04), 0.03)
    m = R['shirt'] & starts(['spine_04', 'spine_05', 'clavicle'])
    fold[m] += 0.0025 * np.sin((P[m, 0] * 0.8 + z[m]) * 55.0)
    noise = 0.0012 * (np.sin(P[:, 0] * 131.0 + P[:, 2] * 77.0) * np.sin(P[:, 1] * 113.0 - P[:, 2] * 91.0))
    clothed = R['shirt'] | R['pants']
    fold[clothed] += noise[clothed]
    P += N * (off + fold)[:, None]
    # (01/11) a calça entre as pernas: na frente e atrás, a pele da bacia e do alto da coxa perto do meio vai pro plano das coxas naquela
    # altura (o pano passa reto de uma coxa pra outra: a forma do manequim some), suave pros lados
    zc0 = Z_PELVIS + 0.02
    band = (DOM == 'pelvis') | (starts(['thigh']) & (P[:, 2] > Z_PELVIS - 0.22))
    xm = float(bh('pelvis')[0])
    for side in (-1.0, 1.0):                       # -1 a frente (−Y), +1 atrás
        sel = band & (np.sign(P[:, 1] - float(bh('pelvis')[1])) == side) & (P[:, 2] < zc0)
        if not sel.any():
            continue
        zs = P[sel, 2]
        for z0 in np.arange(Z_PELVIS - 0.22, zc0, 0.01):
            m = sel & (np.abs(P[:, 2] - z0) < 0.006)
            th = m & (np.abs(P[:, 0] - xm) > 0.07) & (np.abs(P[:, 0] - xm) < 0.13)
            if not th.any():
                continue
            ext = float(P[th, 1].min()) if side < 0 else float(P[th, 1].max())
            mid = m & (np.abs(P[:, 0] - xm) < 0.09)
            w = np.clip(1.0 - np.abs(P[mid, 0] - xm) / 0.09, 0.0, 1.0) ** 0.7
            w *= np.clip((zc0 - P[mid, 2]) / 0.04, 0.0, 1.0)
            P[mid, 1] = P[mid, 1] * (1.0 - w) + (ext - side * 0.004) * w
    P = smooth_taubin(P, band, 10)   # as emendas das fatias somem
    # o cinto: uma faixa apertada (o pano sobe por cima e por baixo dela)
    m = R['shirt'] | R['pants']
    P[m] -= N[m] * (0.004 * g2(z[m] - belt_z, 0.012))[:, None]
    log('farda: camisa %d, calça %d, bota %d, luva %d vértices' % tuple(int(R[k].sum()) for k in ('shirt', 'pants', 'boot', 'hands')))
    return P, R


# ----------------------------------------------------------------------------------------------------------------------------------
# a pintura (etapa 1: as cores chapadas das regiões, a camuflagem OCP em 3D, a pele com o rosto)
COL = {
    'usa': dict(cloth=(0.21, 0.17, 0.115), spot1=(0.15, 0.115, 0.075), spot2=(0.15, 0.15, 0.10), boots=(0.20, 0.15, 0.09),
                gloves=(0.09, 0.08, 0.06), skin=(0.42, 0.29, 0.21), hair=(0.05, 0.035, 0.025), lips=(0.30, 0.17, 0.13)),
    # (01/11) a China: o casacão de lã verde-escuro (a bíblia: a China é a mais escura), a bota preta, a luva de lã verde
    # (01/11) a GLA: a roupa civil de cores misturadas e gastas (a túnica areia, a calça cinza-oliva), o couro, o lenço claro quadriculado
    'gla': dict(cloth=(0.24, 0.21, 0.155), spot1=(0.21, 0.185, 0.135), spot2=(0.27, 0.24, 0.18), boots=(0.13, 0.09, 0.06),
                gloves=(0.36, 0.24, 0.17), skin=(0.36, 0.24, 0.17), hair=(0.025, 0.02, 0.016), lips=(0.26, 0.15, 0.11),
                pants=(0.14, 0.14, 0.11), vest=(0.07, 0.06, 0.05), leather=(0.12, 0.08, 0.05), sash=(0.20, 0.07, 0.045),
                wrap=(0.50, 0.47, 0.40), wrap_dark=(0.18, 0.15, 0.12), mag=(0.05, 0.045, 0.04)),
    'russia': dict(cloth=(0.10, 0.105, 0.085), spot1=(0.055, 0.065, 0.045), spot2=(0.13, 0.11, 0.08), spot3=(0.03, 0.03, 0.028),
                   boots=(0.03, 0.03, 0.028), gloves=(0.05, 0.05, 0.045), skin=(0.42, 0.31, 0.24), hair=(0.05, 0.04, 0.03),
                   lips=(0.30, 0.17, 0.13), balaclava=(0.045, 0.05, 0.04), armor=(0.085, 0.09, 0.07), dark=(0.03, 0.03, 0.03),
                   lens=(0.02, 0.025, 0.03), mag=(0.05, 0.045, 0.04)),
    'china': dict(cloth=(0.065, 0.088, 0.048), spot1=(0.058, 0.08, 0.043), spot2=(0.072, 0.095, 0.052), boots=(0.035, 0.032, 0.03),
                  gloves=(0.055, 0.065, 0.042), skin=(0.43, 0.31, 0.21), hair=(0.02, 0.018, 0.016), lips=(0.30, 0.17, 0.13),
                  fur=(0.075, 0.06, 0.045), leather=(0.09, 0.06, 0.035), brass=(0.35, 0.26, 0.10), star=(0.45, 0.03, 0.02)),
}


def camo(p, k=9.0):
    x, y, z = p[:, 0] * k, p[:, 1] * k, p[:, 2] * k
    a = np.sin(x * 1.3 + np.sin(z * 0.7) * 2.0) + np.sin(y * 1.1 + z * 0.9 + 1.7) + 0.6 * np.sin(z * 2.3 + x * 0.8)
    b = np.sin(x * 0.9 - y * 1.4 + 4.1) + np.sin(z * 1.7 - x * 0.6 + 0.4) + 0.5 * np.sin(y * 2.6 + 2.2)
    return a, b


def paint(R):
    C = COL.get(FACTION, COL['usa'])
    # a cor por vértice primeiro (a textura vem na etapa 2): o bastante pra conferir a forma
    col = np.tile(np.array(C['cloth']), (len(P0), 1))
    clothed = R['shirt'] | R['pants']
    # o OCP: as manchas grandes (marrom-claro e oliva), as médias e as pequenas escuras por cima, contínuas no corpo (o ruído em 3D)
    a, b = camo(P0, 9.0)
    a2, b2 = camo(P0 + 0.37, 17.0)
    a3, _ = camo(P0 - 0.21, 31.0)
    if K['camo'] == 'emr':
        q = np.floor(P0 / 0.025) * 0.025 + 0.0125
        a, b = camo(q, 11.0)
        a2, b2 = camo(q + 0.37, 21.0)
        a3, _ = camo(q - 0.21, 37.0)
        col[clothed & (a3 > 1.6)] = C['spot3']
    if not K['camo']:
        a = a * 0.0 - 9.0
        b = b * 0.0 - 9.0
        a2 = a2 * 0.0 - 9.0
        b2 = b2 * 0.0 - 9.0
        a3 = a3 * 0.0 - 9.0
        wear = 0.5 + 0.5 * np.sin(P0[:, 0] * 23.0 + np.sin(P0[:, 2] * 17.0) * 2.0) * np.sin(P0[:, 1] * 19.0 + P0[:, 2] * 11.0)
        col[clothed] = col[clothed] * (0.9 + 0.2 * wear[clothed, None])
    col[clothed & (a > 0.8)] = C['spot1']
    col[clothed & (b > 1.0) & ~(a > 0.8)] = C['spot2']
    col[clothed & (a2 > 1.3)] = np.array(C['spot1']) * 0.85
    col[clothed & (b2 > 1.5)] = np.array(C['spot2']) * 1.08
    col[clothed & (a3 > 1.7)] = np.array(C['cloth']) * 0.72
    # o uso: o joelho e o cotovelo claros e gastos, o pó subindo da bota
    z = P0[:, 2]
    knee = clothed & (np.abs(z - Z_KNEE) < 0.06)
    col[knee] = col[knee] * 1.12
    dusty = np.clip((Z_KNEE + 0.1 - z) / 0.5, 0.0, 1.0)
    col = col * (1.0 - 0.3 * dusty[:, None]) + np.array((0.33, 0.27, 0.18)) * (0.3 * dusty[:, None])
    if 'pants' in C:
        col[R['pants']] = np.array(C['pants']) * (0.9 + 0.2 * (0.5 + 0.5 * np.sin(P0[R['pants'], 0] * 23.0)))[:, None]
    col[R['boot']] = C['boots']
    col[R['hands']] = C['skin'] if K.get('bare_hands') else C['gloves']
    skin = R['head']
    col[skin] = C['skin']
    x = P0[:, 0] - HEAD_C[0]
    z = P0[:, 2]
    f = -(P0[:, 1] - HEAD_C[1])
    face = skin & (f > 0.03)
    for sx in (-1.0, 1.0):
        col[face & (((x - sx * 0.031) / 0.012) ** 2 + ((z - Z_EYE) / 0.0055) ** 2 < 1.0)] = (0.05, 0.035, 0.03)
        col[face & (np.abs(z - (Z_EYE + 0.017)) < 0.0045) & (np.abs(x - sx * 0.031) < 0.02)] = C['hair']
    col[face & (np.abs(z - Z_MOUTH) < 0.006) & (np.abs(x) < 0.019)] = C['lips']
    # o cabelo curto na nuca e nos lados (o capacete cobre o resto)
    col[skin & (f < 0.0) & (z > Z_EYE - 0.03)] = C['hair']
    ca = ME.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    for i, c in enumerate(col):
        ca.data[i].color = (c[0], c[1], c[2], 1.0)
    return col


# ----------------------------------------------------------------------------------------------------------------------------------
def apply_positions(P):
    for i, v in enumerate(ME.vertices):
        v.co = MWI @ Vector(tuple(P[i]))
    ME.update()


def check_sheet(tag):
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 24
    w = bpy.data.worlds.new('w')
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.55, 0.45, 0.32, 1.0)
    w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sc.collection.objects.link(sun)
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50.0), 0.0, math.radians(35.0))
    mat = bpy.data.materials.new('chk')
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes['Principled BSDF']
    bsdf.inputs['Roughness'].default_value = 0.85
    bsdf.inputs['Specular IOR Level'].default_value = 0.15
    vc = nt.nodes.new('ShaderNodeVertexColor')
    vc.layer_name = 'Col'
    nt.links.new(vc.outputs['Color'], bsdf.inputs['Base Color'])
    for k in range(len(ME.materials)):
        ME.materials[k] = mat
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    shots = []
    cz = float((P0[:, 2].min() + P0[:, 2].max()) / 2.0)
    views = [('frente', (0.0, -4.2, cz), cz, 50, 480, 720), ('lado', (4.2, 0.0, cz), cz, 50, 480, 720),
             ('cima', (0.0, -2.0, cz + 4.0), cz, 50, 480, 720),
             ('rosto', (HEAD_C[0] + 0.18, -0.75, Z_EYE + 0.02), Z_EYE - 0.01, 85, 600, 600),
             ('perfil', (HEAD_C[0] + 0.75, HEAD_C[1] - 0.05, Z_EYE + 0.02), Z_EYE - 0.01, 85, 600, 600)]
    for tag2, loc, tz, lens, rx, ry in views:
        sc.render.resolution_x, sc.render.resolution_y = rx, ry
        cam.location = Vector(loc)
        tgt = Vector((HEAD_C[0], HEAD_C[1], tz)) if tag2 in ('rosto', 'perfil') else Vector((0.0, 0.0, tz))
        cam.rotation_euler = (tgt - cam.location).to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = lens
        p = os.path.join(OUT, 'check_%s_%s.png' % (tag, tag2))
        sc.render.filepath = p
        bpy.ops.render.render(write_still=True)
        shots.append(p)
    log('conferência', shots)


MEAN = r'C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/MetaHuman/MetaHumanAnimator/Content/MeshFitting/Template/mean.obj'


def boundary_loops(bm):
    loops = []
    seen = set()
    for e in bm.edges:
        if not e.is_boundary or e.index in seen:
            continue
        ring = []
        stack = [e]
        while stack:
            ee = stack.pop()
            if ee.index in seen:
                continue
            seen.add(ee.index)
            ring.append(ee)
            for vv in ee.verts:
                for e2 in vv.link_edges:
                    if e2.is_boundary and e2.index not in seen:
                        stack.append(e2)
        pts = np.array([tuple(vv.co) for ee in ring for vv in ee.verts])
        loops.append((len(ring), pts.mean(0), pts))
    return loops


def human_head():
    """(01/11) A cabeça humana: a cabeça média do MetaHuman (o molde do ajuste de rosto do MetaHuman Animator, que vem com a Unreal
    5.8: Content/MeshFitting/Template/mean.obj, 24 mil vértices, com a pálpebra, a íris, a orelha e o lábio) no lugar da cabeça do
    manequim (que tem os sulcos de painel e nenhum traço). Os contornos marcados do molde (curves_config.json: as pálpebras, a íris, as
    sobrancelhas, os lábios, o sulco do queixo) dão a medida e a pintura. Do queixo ao alto 0,25 m (o manequim tem 0,263: a cabeça
    um pouco grande lê melhor de cima), o pescoço no centro do pescoço do corpo, cortada por dentro da gola; os pesos: a cabeça no
    osso head, o pescoço passando pro neck_02 e o neck_01 pela altura."""
    tdir = os.path.dirname(MEAN)
    cfg = json.load(open(os.path.join(tdir, 'curves_config.json'), encoding='utf-8'))['data']

    def ids(name):
        v = cfg[name]
        return [v['vID']] if 'vID' in v else list(v.get('vIDs') or v.get('ids') or [])

    bpy.ops.wm.obj_import(filepath=MEAN, forward_axis='NEGATIVE_Z', up_axis='Y')
    hd = [o for o in bpy.context.selected_objects if o.type == 'MESH'][0]
    hd.name = 'HumanHead'
    hm = hd.data
    hm.transform(hd.matrix_world)
    hd.matrix_world = Matrix.Identity(4)
    V = np.array([tuple(v.co) for v in hm.vertices]) / 100.0     # cm -> m
    top = float(V[:, 2].max())
    fold = V[ids('pt_mentolabial_fold_l')[0]] * 0.5 + V[ids('pt_mentolabial_fold_r')[0]] * 0.5
    # o queixo (o mento): no perfil do meio, abaixo do sulco do lábio, o ponto mais baixo em que a frente ainda está a menos de 1,5 cm
    # da ponta do queixo (abaixo dele a pele vira pra trás, pro pescoço)
    mid = V[np.abs(V[:, 0] - fold[0]) < 0.004]
    zs = np.arange(fold[2], fold[2] - 0.09, -0.002)
    prof = [(z, float(mid[np.abs(mid[:, 2] - z) < 0.0015][:, 1].min())) for z in zs if (np.abs(mid[:, 2] - z) < 0.0015).any()]
    tip = min(y for z, y in prof if z > fold[2] - 0.05)
    chin = min(z for z, y in prof if y < tip + 0.015)
    k = 0.25 / max(top - chin, 1e-3)
    W = V * k
    W[:, 2] += HEAD_TOP - W[:, 2].max()
    # o pescoço do molde no centro do pescoço do corpo, na altura do pé do pescoço
    zc = Z_NECK + 0.03
    mh = W[np.abs(W[:, 2] - zc) < 0.01]
    nb = P0[starts(['neck']) & (np.abs(P0[:, 2] - zc) < 0.015)]
    if len(mh) and len(nb):
        d = (nb[:, :2].min(0) + nb[:, :2].max(0)) / 2.0 - (mh[:, :2].min(0) + mh[:, :2].max(0)) / 2.0
        W[:, 0] += d[0]
        W[:, 1] += d[1]
    for n, v in enumerate(hm.vertices):
        v.co = Vector(tuple(W[n]))
    log('cabeça humana: do queixo ao alto %.3f m no molde, escala %.3f' % (top - chin, k))
    # a pintura antes do corte (os índices do molde)
    C = COL.get(FACTION, COL['usa'])
    col = np.tile(np.array(C['skin']), (len(W), 1))
    fx = W[:, 0]
    fz = W[:, 2]

    def inside(poly):
        """O ponto (x, z) dentro do polígono (x, z) do contorno: o teste do raio, vetorizado."""
        x, z = fx, fz
        c = np.zeros(len(W), dtype=bool)
        n = len(poly)
        for a in range(n):
            x1, z1 = poly[a]
            x2, z2 = poly[(a + 1) % n]
            hit = ((z1 > z) != (z2 > z)) & (x < (x2 - x1) * (z - z1) / ((z2 - z1) + 1e-12) + x1)
            c ^= hit
        return c

    front = W[:, 1] < np.percentile(W[:, 1], 45)
    def curve(name):
        return W[ids(name)]
    for sd in ('l', 'r'):
        up = curve('crv_eyelid_upper_' + sd)
        lo = curve('crv_eyelid_lower_' + sd)
        poly = [tuple(p) for p in up[:, [0, 2]]] + [tuple(p) for p in lo[::-1][:, [0, 2]]]
        eye = inside(poly) & front
        ce = np.concatenate([up, lo]).mean(0)
        wdt = float(np.ptp(np.concatenate([up, lo])[:, 0]))
        rr = np.hypot(fx - ce[0], fz - ce[2])
        col[eye] = (0.56, 0.53, 0.50)                                         # o branco
        col[eye & (rr < 0.26 * wdt)] = (0.15, 0.10, 0.06)                     # a íris
        col[eye & (rr < 0.11 * wdt)] = (0.02, 0.018, 0.016)                   # a pupila
        for cv in (up, lo):                                                    # a linha dos cílios
            for q in cv:
                col[front & (np.hypot(fx - q[0], fz - q[2]) < 0.0016)] = (0.03, 0.022, 0.018)
        bu = curve('crv_brow_upper_' + sd)
        bl = curve('crv_brow_lower_' + sd)
        poly = [tuple(p) for p in bu[:, [0, 2]]] + [tuple(p) for p in bl[::-1][:, [0, 2]]]
        col[inside(poly) & front] = np.array(C['skin']) * 0.4 + np.array(C['hair']) * 0.6   # (01/11, ele: "a cor pintada na pele ficou ruim")
    lu = np.concatenate([curve('crv_lip_upper_outer_l'), curve('crv_lip_upper_outer_r')[::-1]])
    ll = np.concatenate([curve('crv_lip_lower_outer_l'), curve('crv_lip_lower_outer_r')[::-1]])
    ang = lambda P_: np.arctan2(P_[:, 2] - lu.mean(0)[2], P_[:, 0] - lu.mean(0)[0])
    lips = np.concatenate([lu, ll])
    order = np.argsort(ang(lips))
    lips_in = inside([tuple(p) for p in lips[order][:, [0, 2]]]) & front
    col[lips_in] = np.array(C['skin']) * 0.45 + np.array(C['lips']) * 0.55
    # a barba de três dias (o queixo, o buço e a mandíbula) e o cabelo curto (os lados e a nuca; o alto fica pro capacete)
    mz = float(lips[:, 2].min())
    beard = front & (fz < float(lu[:, 2].max()) + 0.004) & ~lips_in & (fz > chin * k + (HEAD_TOP - top * k) - 0.05)
    col[beard] = col[beard] * 0.8 + np.array(C['hair']) * 0.2
    # o cabelo curto (o corte militar): o couro do alto pra trás da linha do cabelo (~4,5 cm acima da sobrancelha na frente), os lados
    # acima da orelha e a nuca até o pé da orelha
    earL, earR = curve('crv_ear_outer_helix_l'), curve('crv_ear_outer_helix_r') if 'crv_ear_outer_helix_r' in cfg else curve('ear_outer_helix_r')
    ear_top = float(max(earL[:, 2].max(), earR[:, 2].max()))
    ear_bot = float(min(earL[:, 2].min(), earR[:, 2].min()))
    ear_y = float((earL[:, 1].mean() + earR[:, 1].mean()) / 2.0)
    brow_top = float(max(curve('crv_brow_upper_l')[:, 2].max(), curve('crv_brow_upper_r')[:, 2].max()))
    hy = W[:, 1]
    hairline = brow_top + 0.045 - 0.05 * np.clip((hy - float(W[:, 1].min())) / 0.06, 0.0, 1.0)
    hair = (fz > hairline) | ((hy > ear_y) & (fz > ear_bot - 0.005)) | ((hy > ear_y - 0.03) & (fz > ear_top - 0.005))
    hair &= ~np.isin(np.arange(len(W)), ids('crv_ear_outer_helix_l'))
    hair &= ~np.isin(np.arange(len(W)), ids('crv_ear_outer_helix_l') + ids('ear_outer_helix_r'))
    col[hair] = np.array(C['hair']) * 1.15
    if K.get('balaclava'):
        bl = np.concatenate([curve('crv_brow_lower_l'), curve('crv_brow_lower_r')])
        el = np.concatenate([curve('crv_eyelid_lower_l'), curve('crv_eyelid_lower_r')])
        xs = np.concatenate([curve('crv_eyelid_upper_l'), curve('crv_eyelid_upper_r')])[:, 0]
        SLIT.update(z_hi=float(bl[:, 2].mean()) + 0.008, z_lo=float(el[:, 2].min()) - 0.012, x0=float(xs.min()) - 0.016, x1=float(xs.max()) + 0.016)
    ca = hm.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    for n in range(len(W)):
        ca.data[n].color = (float(col[n, 0]), float(col[n, 1]), float(col[n, 2]), 1.0)
    # corta abaixo do pescoço (por dentro da gola)
    cut = Z_NECK - 0.01
    bm = bmesh.new()
    bm.from_mesh(hm)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < cut], context='VERTS')
    bm.to_mesh(hm)
    bm.free()
    zh, zn2, zn1 = bh('head')[2], bh('neck_02')[2], bh('neck_01')[2]
    gh, g2_, g1 = hd.vertex_groups.new(name='head'), hd.vertex_groups.new(name='neck_02'), hd.vertex_groups.new(name='neck_01')
    for v in hm.vertices:
        z = v.co.z
        wh = float(np.clip((z - zn2) / max(zh - zn2, 1e-3), 0.0, 1.0))
        w1 = float(np.clip((zn2 - z) / max(zn2 - zn1, 1e-3), 0.0, 1.0))
        w2 = max(0.0, 1.0 - wh - w1)
        if wh > 0:
            gh.add([v.index], wh, 'REPLACE')
        if w2 > 0:
            g2_.add([v.index], w2, 'REPLACE')
        if w1 > 0:
            g1.add([v.index], w1, 'REPLACE')
    log('cabeça humana: %d vértices depois do corte' % len(hm.vertices))
    for sd in ('l', 'r'):
        lid = np.concatenate([curve('crv_eyelid_upper_' + sd), curve('crv_eyelid_lower_' + sd)])
        ce = lid.mean(0)
        wdt = float(np.ptp(lid[:, 0]))
        r = 0.52 * wdt
        ec = Vector((ce[0], ce[1] + 0.62 * r, ce[2]))
        ebm = bmesh.new()
        bmesh.ops.create_uvsphere(ebm, u_segments=20, v_segments=14, radius=r)
        eye = bpy.data.objects.new('Eye_' + sd, bpy.data.meshes.new('Eye_' + sd))
        ebm.to_mesh(eye.data)
        ebm.free()
        bpy.context.scene.collection.objects.link(eye)
        eye.data.transform(Matrix.Translation(ec))
        eye.vertex_groups.new(name='head').add([v.index for v in eye.data.vertices], 1.0, 'REPLACE')
        ca = eye.data.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
        for v in eye.data.vertices:
            fwd = -(v.co.y - ec.y) / r
            c = (0.02, 0.018, 0.016) if fwd > 0.95 else ((0.13, 0.085, 0.05) if fwd > 0.86 else (0.52, 0.50, 0.47))
            ca.data[v.index].color = (c[0], c[1], c[2], 1.0)
        HEADS.append(eye)
    HEADS.append(hd)
    return hd



# ----------------------------------------------------------------------------------------------------------------------------------
# 3. o equipamento (01/11, a 3ª volta): modelado com chanfro, encostado no pano, pesado pelo corpo (Data Transfer) e juntado na malha
GEAR = []          # (objeto, cor RGB, o osso único ou None = os pesos do corpo)


def body_bvh_now():
    bm = bmesh.new()
    bm.from_mesh(ME)
    bm.transform(MW)
    t = BVHTree.FromBMesh(bm)
    bm.free()
    return t


def hit(origin, direction, dist=1.0, tree=None):
    h = (tree or BVH2).ray_cast(Vector(origin), Vector(direction).normalized(), dist)
    return h[0], h[1]


def part_bvh(mask):
    """A superfície (vestida) só das faces de uma parte do corpo (o tronco sem os braços, uma perna): os raios de medida não batem no
    braço da pose em A."""
    bm = bmesh.new()
    bm.from_mesh(ME)
    bm.transform(MW)
    bm.verts.ensure_lookup_table()
    kill = [f for f in bm.faces if not all(mask[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=kill, context='FACES')
    t = BVHTree.FromBMesh(bm)
    bm.free()
    return t


def mk(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def modapply(ob, kind, **kw):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    m = ob.modifiers.new('m', kind)
    for k, v in kw.items():
        setattr(m, k, v)
    bpy.ops.object.modifier_apply(modifier='m')


def rbox(name, size, center, bevel, seg=3, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=seg, profile=0.5, affect='EDGES')
    if rot is not None:
        bmesh.ops.rotate(bm, verts=bm.verts, cent=Vector((0, 0, 0)), matrix=rot)
    for v in bm.verts:
        v.co += Vector(center)
    return mk(name, bm)


def panel(name, center, wdt, hgt, chamfer, nx, ny, bend_r, facing, thick, bevel):
    """A placa/o painel no peito ou nas costas: a grade de quadriláteros dentro do contorno (o corte de ombro em cima: as quinas
    chanfradas), curvada em volta de um eixo vertical (o peito é redondo), de frente pra 'facing' (−1 a frente, +1 as costas),
    com a espessura e o chanfro das bordas."""
    bm = bmesh.new()
    rows = []
    for j in range(ny + 1):
        v = j / ny
        zz = (v - 0.5) * hgt
        cut = max(0.0, (v - (1.0 - chamfer / hgt))) / (chamfer / hgt) if chamfer > 0 else 0.0
        half = wdt / 2.0 - chamfer * cut
        row = []
        for i in range(nx + 1):
            u = i / nx
            x = (u - 0.5) * 2.0 * half
            ang = x / bend_r
            px = bend_r * math.sin(ang)
            row.append(bm.verts.new((center[0] + px, center[1] - facing * bend_r * (1.0 - math.cos(ang)), center[2] + zz)))   # as bordas voltam pro corpo
        rows.append(row)
    for j in range(ny):
        for i in range(nx):
            f = bm.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
    bm.normal_update()
    ob = mk(name, bm)
    modapply(ob, 'SOLIDIFY', thickness=thick, offset=0.0)
    modapply(ob, 'BEVEL', width=bevel, segments=3, limit_method='ANGLE')
    return ob


def surface_y(x, z, facing):
    """A pele (já vestida) na frente (facing −1) ou nas costas (+1) do ponto (x, z)."""
    o = (x, facing * 0.8, z)
    h, n = hit(o, (0.0, -facing, 0.0), 1.6, BVH_T)
    return (h.y if h is not None else facing * 0.12)


def strap_path(pts, width, thick, name):
    bm = bmesh.new()
    prev = None
    for i, p in enumerate(pts):
        tg = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = tg.cross(Vector((1.0, 0.0, 0.0)))
        if side.length < 1e-3:
            side = tg.cross(Vector((0.0, 1.0, 0.0)))
        side.normalize()
        nrm = side.cross(tg).normalized()
        a = Vector((1.0, 0.0, 0.0)) * (width / 2.0)
        vs = [bm.verts.new(p - a), bm.verts.new(p + a), bm.verts.new(p + a + nrm * thick), bm.verts.new(p - a + nrm * thick)]
        if prev:
            for k in range(4):
                bm.faces.new((prev[k], prev[(k + 1) % 4], vs[(k + 1) % 4], vs[k]))
        prev = vs
    return mk(name, bm)


def molle(name, x0, x1, z, y_of, facing, rows=3, pitch=0.025, depth=0.003):
    """As tiras de MOLLE: fitas de 2,5 cm em fileiras, saltadas da superfície, com as costuras (os vãos) a cada 3,8 cm."""
    obs = []
    for r in range(rows):
        zz = z - r * pitch
        xs = np.arange(x0, x1, 0.038)
        for k, xa in enumerate(xs[:-1]):
            xb = min(xa + 0.034, x1)
            cx = (xa + xb) / 2.0
            y = y_of(cx, zz) + facing * 0.002
            obs.append(rbox('%s_%d_%d' % (name, r, k), (xb - xa, 0.004, 0.022), (cx, y, zz), 0.001, 1))
    return obs


def gear_usa():
    C = dict(coyote=(0.17, 0.135, 0.085), coyote_d=(0.13, 0.10, 0.065), dark=(0.035, 0.035, 0.032), mag=(0.05, 0.05, 0.045),
             helmet=(0.20, 0.16, 0.10), lens=(0.02, 0.022, 0.025), strap=(0.12, 0.095, 0.06), webbing=(0.15, 0.12, 0.075))
    s3 = bh('spine_03')
    s5 = bh('spine_05')
    cx = float(s3[0])
    # as placas da frente e de trás (SAPI: 25 × 32 cm, o corte de ombro), 2,5 cm de placa + a capa
    zc = float(s3[2] + 0.11)
    yf = surface_y(cx, zc, -1.0)
    yb = surface_y(cx, zc, 1.0)
    front = panel('PlateF', (cx, yf - 0.032, zc), 0.27, 0.33, 0.055, 14, 16, 0.24, -1.0, 0.035, 0.007)
    GEAR.append((front, C['coyote'], None))
    back = panel('PlateB', (cx, yb + 0.032, zc), 0.27, 0.35, 0.04, 14, 16, 0.26, 1.0, 0.035, 0.007)
    GEAR.append((back, C['coyote'], None))
    # a cinta lateral (o cummerbund): a faixa de 17 cm em volta da cintura alta, encolhida no corpo a 1,3 cm, com a espessura
    zlo, zhi = float(s3[2] - 0.08), float(s3[2] + 0.07)
    bm = bmesh.new()
    seg, rings = 64, 6
    grid = []
    for j in range(rings + 1):
        z = zlo + (zhi - zlo) * j / rings
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            q = h + n * 0.016 if h is not None else o
            row.append(bm.verts.new(q))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    cum = mk('Cummerbund', bm)
    modapply(cum, 'SOLIDIFY', thickness=0.012, offset=1.0)
    modapply(cum, 'SUBSURF', levels=1)
    GEAR.append((cum, C['coyote'], None))
    # os ombros: a alça acolchoada da frente pra trás por cima de cada ombro
    for sd in (-1.0, 1.0):
        x = cx + sd * 0.085
        pts = []
        for ang in np.linspace(-75, 75, 11):
            a = math.radians(ang)
            o = Vector((x, float(s5[1]) + math.sin(a) * 0.5, float(s5[2]) - 0.02 + math.cos(a) * 0.5))
            h, n = hit(o, Vector((x, float(s5[1]), float(s5[2]) - 0.02)) - o, 0.7, BVH_T)
            if h is not None:
                pts.append(h + n * 0.012)
        if len(pts) > 4:
            st = strap_path(pts, 0.055, 0.016, 'Shoulder_%d' % int(sd))
            modapply(st, 'SUBSURF', levels=1)
            GEAR.append((st, C['coyote'], None))
    yfront = lambda x: yf - 0.032 - 0.0175 + 0.24 * (1.0 - math.cos((x - cx) / 0.24))
    # o MOLLE na placa da frente (embaixo dos porta-carregadores ficam só as de cima) e na de trás
    for o in molle('MolleF', cx - 0.12, cx + 0.12, zc + 0.13, lambda x, z: yfront(x), -1.0, rows=1):
        GEAR.append((o, C['webbing'], None))
    yback = lambda x: yb + 0.032 + 0.0175 - 0.26 * (1.0 - math.cos((x - cx) / 0.26))
    for o in molle('MolleB', cx - 0.12, cx + 0.12, zc + 0.12, lambda x, z: yback(x), 1.0, rows=6):
        GEAR.append((o, C['webbing'], None))
    # os porta-carregadores: 3 na frente embaixo, com a aba elástica e o topo do carregador saindo
    for k in (-1, 0, 1):
        x = cx + k * 0.083
        yp = yfront(x) - 0.024
        pouch = rbox('MagPouch_%d' % k, (0.078, 0.045, 0.12), (x, yp, zc - 0.07), 0.009, 3)
        GEAR.append((pouch, C['coyote'], None))
        mag = rbox('MagTop_%d' % k, (0.06, 0.026, 0.03), (x, yp, zc - 0.07 + 0.07), 0.005, 2)
        GEAR.append((mag, C['mag'], None))
        flap = rbox('MagFlap_%d' % k, (0.07, 0.05, 0.014), (x, yp, zc - 0.07 + 0.05), 0.004, 2)
        GEAR.append((flap, C['coyote_d'], None))
    # a bolsa de mapa (admin) em cima, o rádio do lado esquerdo com a antena, o kit de primeiros socorros atrás
    adm = rbox('Admin', (0.17, 0.03, 0.10), (cx, yfront(cx) - 0.016, zc + 0.065), 0.008, 3)
    GEAR.append((adm, C['coyote'], None))
    h, n = hit((cx + 0.6, float(s3[1]) + 0.02, zc - 0.03), (-1.0, 0.0, 0.0), 0.8, BVH_T)
    sx = (h.x if h is not None else cx + 0.17) + 0.04
    radio = rbox('Radio', (0.05, 0.08, 0.15), (sx, float(s3[1]) + 0.02, zc - 0.03), 0.01, 3)
    GEAR.append((radio, C['coyote'], None))
    ant = rbox('Antenna', (0.007, 0.007, 0.32), (sx, float(s3[1]) + 0.04, zc + 0.19), 0.003, 1)
    GEAR.append((ant, C['dark'], None))
    ifak = rbox('Ifak', (0.16, 0.06, 0.10), (cx, yback(cx) + 0.03, zc - 0.11), 0.012, 3)
    GEAR.append((ifak, C['coyote'], None))
    # o cinto de combate (acolchoado, 5 cm) com a fivela e 2 bolsas
    zb = float(Z_PELVIS + 0.09)
    bm = bmesh.new()
    grid = []
    for j in range(3):
        z = zb - 0.025 + 0.025 * j
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            q = h + n * 0.008 if h is not None else o
            row.append(bm.verts.new(q))
        grid.append(row)
    for j in range(2):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    belt = mk('Belt', bm)
    modapply(belt, 'SOLIDIFY', thickness=0.012, offset=1.0)
    GEAR.append((belt, C['strap'], None))
    yfb = surface_y(cx, zb, -1.0)
    buckle = rbox('Buckle', (0.06, 0.012, 0.045), (cx, yfb - 0.018, zb), 0.004, 2)
    GEAR.append((buckle, C['dark'], None))
    for sd in (-1.0, 1.0):
        h, n = hit((cx + sd * 0.6, float(s3[1]) + 0.03, zb - 0.03), (-sd, 0.0, 0.0), 0.8, BVH_T)
        if h is not None:
            bp = rbox('BeltPouch_%d' % int(sd), (0.05, 0.11, 0.12), (h.x + sd * 0.03, h.y, zb - 0.04), 0.012, 3)
            GEAR.append((bp, C['coyote'], None))
    # as joelheiras: a concha encolhida no joelho (a frente da perna, ±8 cm), com a espessura e o chanfro
    for sd in ('l', 'r'):
        kn = bh('calf_' + sd)
        bm = bmesh.new()
        grid = []
        for j in range(9):
            z = float(kn[2]) - 0.07 + 0.14 * j / 8
            row = []
            for k in range(11):
                a = math.radians(-70 + 140 * k / 10)
                o = Vector((float(kn[0]) + 0.4 * math.sin(a), float(kn[1]) - 0.4 * math.cos(a), z))
                h, n = hit(o, Vector((float(kn[0]), float(kn[1]), z)) - o, 0.6, BVH_LEG[sd])
                row.append(bm.verts.new(h + n * 0.006 if h is not None else o))
            grid.append(row)
        for j in range(8):
            for k in range(10):
                bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
        kp = mk('Knee_' + sd, bm)
        modapply(kp, 'SOLIDIFY', thickness=0.018, offset=1.0)
        modapply(kp, 'BEVEL', width=0.005, segments=2, limit_method='ANGLE')
        GEAR.append((kp, C['coyote_d'], 'calf_' + sd))
    # os bolsos da manga (com a aba) e o da calça (o cargo, com a aba), encostados no pano
    for sd in ('l', 'r'):
        ua, la = bh('upperarm_' + sd), bh('lowerarm_' + sd)
        c = ua + (la - ua) * 0.35
        out = 1.0 if c[0] > cx else -1.0
        h, n = hit((float(c[0]) + out * 0.4, float(c[1]), float(c[2])), (-out, 0.0, 0.0), 0.8)
        if h is not None:
            pk = rbox('SleevePocket_' + sd, (0.012, 0.09, 0.11), (h.x + out * 0.004, h.y, h.z), 0.004, 2)
            GEAR.append((pk, C['coyote'], None))
            fl = rbox('SleeveFlap_' + sd, (0.014, 0.095, 0.03), (h.x + out * 0.008, h.y, h.z + 0.045), 0.003, 2)
            GEAR.append((fl, C['coyote_d'], None))
        th, kn = bh('thigh_' + sd), bh('calf_' + sd)
        c = th + (kn - th) * 0.55
        out = 1.0 if c[0] > cx else -1.0
        h, n = hit((float(c[0]) + out * 0.4, float(c[1]), float(c[2])), (-out, 0.0, 0.0), 0.8)
        if h is not None:
            cp = rbox('Cargo_' + sd, (0.025, 0.13, 0.17), (h.x + out * 0.008, h.y, h.z), 0.01, 3)
            GEAR.append((cp, C['coyote'], None))
            fl = rbox('CargoFlap_' + sd, (0.03, 0.135, 0.035), (h.x + out * 0.012, h.y, h.z + 0.07), 0.006, 2)
            GEAR.append((fl, C['coyote_d'], None))
    log('equipamento USA: %d peças' % len(GEAR))


def helmet_ach():
    """O capacete (ACH/ECH): a casca a 1,8 cm da cabeça (a espuma e o casco), cortada na sobrancelha e acima da orelha, com a borda
    arredondada; a capa coyote; os trilhos dos lados; o suporte do visor noturno na frente; a jugular; os óculos de proteção."""
    hv = [o for o in HEADS if o.name == 'HumanHead'][0]
    HV = np.array([tuple(v.co) for v in hv.data.vertices])
    top = float(HV[:, 2].max())
    c = (HV.min(0) + HV.max(0)) / 2.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=64, v_segments=40, radius=0.2)
    for v in bm.verts:
        v.co += Vector((c[0], c[1], top - 0.09))
    hel = mk('Helmet', bm)
    sw = hel.modifiers.new('sw', 'SHRINKWRAP')
    sw.target = hv
    sw.wrap_method = 'NEAREST_SURFACEPOINT'
    sw.offset = 0.019
    bpy.ops.object.select_all(action='DESELECT')
    hel.select_set(True)
    bpy.context.view_layer.objects.active = hel
    bpy.ops.object.modifier_apply(modifier='sw')
    modapply(hel, 'CORRECTIVE_SMOOTH', factor=0.9, iterations=10, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    brow = float(HV[np.argmin(HV[:, 1])][2]) + 0.03                 # o pé da testa
    bm = bmesh.new()
    bm.from_mesh(hel.data)
    def cut_z(v):
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        fr = max(0.0, -d.y / max(d.length, 1e-6))
        return (top - 0.125) + (brow + 0.012 - (top - 0.125)) * fr ** 1.5
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < cut_z(v)], context='VERTS')
    edge = [v for v in bm.verts if v.is_boundary]
    for v in edge:
        v.co.z = cut_z(v)
    for _ in range(3):
        bmesh.ops.smooth_vert(bm, verts=edge, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(hel.data)
    bm.free()
    modapply(hel, 'SOLIDIFY', thickness=0.008, offset=1.0)
    modapply(hel, 'BEVEL', width=0.003, segments=2, limit_method='ANGLE')
    GEAR.append((hel, (0.20, 0.16, 0.10), 'head'))
    dg = bpy.context.evaluated_depsgraph_get()
    T_HEL = BVHTree.FromObject(hel, dg)
    T_HEAD = BVHTree.FromObject(hv, dg)
    # os trilhos (ARC) nos lados e o suporte do visor (a placa e o braço) na frente
    for sd in (-1.0, 1.0):
        h, n = hit((c[0] + sd * 0.5, c[1], top - 0.075), (-sd, 0.0, 0.0), 0.8, T_HEL)
        if h is not None:
            rail = rbox('Rail_%d' % int(sd), (0.012, 0.13, 0.016), (h.x + sd * 0.006, h.y + 0.01, h.z), 0.003, 1)
            GEAR.append((rail, (0.06, 0.055, 0.045), 'head'))
    h, n = hit((c[0], c[1] - 0.5, top - 0.055), (0.0, 1.0, 0.0), 0.8, T_HEL)
    if h is not None:
        GEAR.append((rbox('NvgShroud', (0.05, 0.016, 0.035), (c[0], h.y - 0.006, h.z), 0.004, 2), (0.05, 0.05, 0.045), 'head'))
        GEAR.append((rbox('NvgArm', (0.03, 0.028, 0.02), (c[0], h.y - 0.026, h.z - 0.02), 0.005, 2), (0.05, 0.05, 0.045), 'head'))
    # os óculos de proteção: a faixa curva das lentes na altura dos olhos, a 1,4 cm do rosto
    ey = [o for o in HEADS if o.name.startswith('Eye_')]
    if ey:
        ez = float(np.mean([o.data.vertices[0].co.z for o in ey]))
        ez = float(np.mean([np.mean([v.co.z for v in o.data.vertices]) for o in ey]))
        pts = []
        for ang in np.linspace(-62, 62, 13):
            a = math.radians(ang)
            o = Vector((c[0] + 0.4 * math.sin(a), c[1] - 0.4 * math.cos(a), ez))
            h, n = hit(o, Vector((c[0], c[1], ez)) - o, 0.6, T_HEAD)
            if h is not None:
                pts.append(h + n * 0.012)
        if len(pts) > 5:
            bm = bmesh.new()
            prev = None
            for p in pts:
                vs = [bm.verts.new(p + Vector((0, 0, -0.02))), bm.verts.new(p + Vector((0, 0, 0.018)))]
                if prev:
                    bm.faces.new((prev[0], vs[0], vs[1], prev[1]))
                prev = vs
            gl = mk('Glasses', bm)
            modapply(gl, 'SOLIDIFY', thickness=0.003, offset=0.0)
            GEAR.append((gl, (0.02, 0.022, 0.025), 'head'))
    log('capacete pronto')


def surface_patch(name, tree, center, out, u, v, ur, vr, nu, nv, offset, thick):
    """Um remendo de quadriláteros que segue a superfície: cada ponto do quadro (center + u·a + v·b, a em ur, b em vr) é projetado de
    fora pra dentro (contra 'out') na superfície da árvore e afastado 'offset' pela normal; com a espessura. As abas, as bandas, os
    lenços: a peça curva que encosta, nunca a caixa."""
    center, out, u, v = Vector(center), Vector(out).normalized(), Vector(u).normalized(), Vector(v).normalized()
    bm = bmesh.new()
    grid = []
    for j in range(nv + 1):
        row = []
        b = vr[0] + (vr[1] - vr[0]) * j / nv
        for i in range(nu + 1):
            a = ur[0] + (ur[1] - ur[0]) * i / nu
            q = center + u * a + v * b
            h, n, _, _ = tree.ray_cast(q + out * 0.4, -out, 0.8)
            row.append(bm.verts.new(h + n * offset if h is not None else q + out * offset))
        grid.append(row)
    for j in range(nv):
        for i in range(nu):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    ob = mk(name, bm)
    modapply(ob, 'SOLIDIFY', thickness=thick, offset=1.0)
    modapply(ob, 'BEVEL', width=min(thick * 0.45, 0.006), segments=2, limit_method='ANGLE')
    modapply(ob, 'SUBSURF', levels=1)
    return ob


def coat_skirt(color, z_top, z_bot, flare, slit_top, folds=9):
    """(01/11) A barra do casacão: UMA saia em volta das duas pernas (não duas metades), da cintura até abaixo do joelho, aberta na frente
    a partir da virilha (o casaco de verdade), com as dobras de lã (mais fundas na barra) e a bainha que não é reta. Os pesos: perto da
    cintura no pelvis; descendo, cada lado na sua coxa (a saia estica entre as pernas no passo, como o pano). Junta na malha."""
    legs = (DOM == 'pelvis') | starts(['thigh', 'calf'])
    PL = P                                   # o corpo vestido (os índices do DOM: antes de tirar a cabeça do manequim)
    cx = float(bh('pelvis')[0])
    rings, seg = 14, 72
    shape = []
    for j in range(rings + 1):
        t = j / rings
        z = z_top + (z_bot - z_top) * t
        sl = legs & (np.abs(PL[:, 2] - z) < 0.025)
        Q = PL[sl] if sl.any() else PL[legs]
        rx = float(np.abs(Q[:, 0] - cx).max()) + 0.02 + flare * t
        yf, yb = float(Q[:, 1].min()), float(Q[:, 1].max())
        shape.append((z, t, rx, (yf + yb) / 2.0, (yb - yf) / 2.0 + 0.02 + flare * t))
    bm = bmesh.new()
    grid = []
    gap = math.radians(4.0)
    for z, t, rx, yc, ry in shape:
        row = []
        slit = z < slit_top
        a0 = -math.pi / 2.0 + (gap if slit else 0.0)
        a1 = a0 + 2.0 * math.pi - (2.0 * gap if slit else 0.0)
        for k in range(seg + 1):
            a = a0 + (a1 - a0) * k / seg
            ca, sa = math.cos(a), math.sin(a)
            fold = 1.0 + 0.055 * t * math.sin(a * folds + 1.3 * math.sin(a * 3.0))
            x = cx + rx * fold * math.copysign(abs(ca) ** (2.0 / 2.4), ca)
            y = yc + ry * fold * math.copysign(abs(sa) ** (2.0 / 2.4), sa)
            zz = z + 0.02 * math.sin(a * 5.0) * t ** 4
            row.append(bm.verts.new((x, y, zz)))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
    ob = mk('CoatSkirt', bm)
    modapply(ob, 'SOLIDIFY', thickness=0.009, offset=1.0)
    modapply(ob, 'SUBSURF', levels=1)
    # os pesos
    tl = float(bh('thigh_l')[0]) - cx
    gp, gl, gr = ob.vertex_groups.new(name='pelvis'), ob.vertex_groups.new(name='thigh_l'), ob.vertex_groups.new(name='thigh_r')
    for v in ob.data.vertices:
        h = float(np.clip((Z_PELVIS - v.co.z) / 0.28, 0.0, 1.0))
        sx = float(np.clip((v.co.x - cx) / 0.10, -1.0, 1.0)) * (1.0 if tl > 0 else -1.0)
        wl = h * (0.5 + 0.5 * sx)
        wr = h * (0.5 - 0.5 * sx)
        gp.add([v.index], 1.0 - h, 'REPLACE')
        if wl > 0:
            gl.add([v.index], wl, 'REPLACE')
        if wr > 0:
            gr.add([v.index], wr, 'REPLACE')
    GEAR.append((ob, color, 'custom'))
    return ob


def gear_china():
    C = COL['china']
    s3 = bh('spine_03')
    cx = float(s3[0])
    coat_skirt(tuple(k * 0.86 for k in C['cloth']), Z_PELVIS + 0.07, Z_KNEE - 0.12, 0.09, Z_PELVIS - 0.12)
    # a gola dobrada do casacão (larga, de lã) e as dragonas
    n1 = bh('neck_01')
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=40, radius1=0.135, radius2=0.10, depth=0.07)
    for v in bm.verts:
        v.co = Vector((n1[0] + v.co.x, n1[1] + 0.005 + v.co.y * 0.95, n1[2] - 0.02 + v.co.z))
    collar = mk('Collar', bm)
    modapply(collar, 'SOLIDIFY', thickness=0.012, offset=1.0)
    modapply(collar, 'SUBSURF', levels=1)
    GEAR.append((collar, C['cloth'], None))
    for sd in (-1.0, 1.0):
        h, n = hit((cx + sd * 0.5, float(s3[1]), float(bh('neck_01')[2]) - 0.03), (-sd, 0.0, -0.4), 0.8, BVH_T)
        h2, n2 = hit((cx + sd * 0.16, float(s3[1]), float(bh('neck_01')[2]) + 0.4), (0.0, 0.0, -1.0), 0.8, BVH_T)
        if h2 is not None:
            GEAR.append((rbox('Epaulette_%d' % int(sd), (0.11, 0.05, 0.012), (h2.x, h2.y, h2.z + 0.006), 0.004, 2), C['cloth'], None))
    # as duas fileiras de botões de latão no peito e os bolsos do peito com aba
    for row_x in (-0.055, 0.055):
        for k in range(4):
            z = Z_PELVIS + 0.16 + k * 0.085
            h, n = hit((cx + row_x, -0.8, z), (0.0, 1.0, 0.0), 1.6, BVH_T)
            if h is not None:
                bm = bmesh.new()
                bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=0.009)
                b = mk('Button', bm)
                b.data.transform(Matrix.Translation(h + n * 0.004))
                GEAR.append((b, C['brass'], None))
    for sd in (-1.0, 1.0):
        z = float(s3[2]) + 0.1
        h, n = hit((cx + sd * 0.1, -0.8, z), (0.0, 1.0, 0.0), 1.6, BVH_T)
        if h is not None:
            GEAR.append((rbox('ChestPocket_%d' % int(sd), (0.10, 0.016, 0.12), (h.x, h.y - 0.006, h.z), 0.006, 2), C['cloth'], None))
            GEAR.append((rbox('ChestFlap_%d' % int(sd), (0.105, 0.02, 0.035), (h.x, h.y - 0.01, h.z + 0.05), 0.005, 2), C['spot1'], None))
    # o cinto largo de couro com a fivela e as 3 cartucheiras na frente, o cantil no quadril
    zb = float(Z_PELVIS + 0.09)
    bm = bmesh.new()
    grid = []
    seg = 64
    for j in range(3):
        z = zb - 0.028 + 0.028 * j
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            row.append(bm.verts.new(h + n * 0.01 if h is not None else o))
        grid.append(row)
    for j in range(2):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    belt = mk('Belt', bm)
    modapply(belt, 'SOLIDIFY', thickness=0.008, offset=1.0)
    GEAR.append((belt, C['leather'], None))
    hb, nb = hit((cx, -0.8, zb), (0.0, 1.0, 0.0), 1.6, BVH_T)
    yb = hb.y if hb is not None else -0.12
    GEAR.append((rbox('Buckle', (0.065, 0.012, 0.055), (cx, yb - 0.016, zb), 0.005, 2), C['brass'], None))
    for k in (-1.5, -0.5, 0.5, 1.5):
        x = cx + k * 0.075
        if abs(k) < 1.0:
            continue
        h, n = hit((x, -0.8, zb - 0.01), (0.0, 1.0, 0.0), 1.6, BVH_T)
        if h is not None:
            GEAR.append((rbox('Ammo_%d' % int(k * 2), (0.07, 0.045, 0.085), (h.x, h.y - 0.03, zb - 0.01), 0.01, 3), C['leather'], None))
            GEAR.append((rbox('AmmoFlap_%d' % int(k * 2), (0.072, 0.05, 0.022), (h.x, h.y - 0.032, zb + 0.03), 0.006, 2), C['leather'], None))
    h, n = hit((cx - 0.6, float(s3[1]) + 0.04, zb - 0.06), (1.0, 0.0, 0.0), 0.8, BVH_T)
    if h is not None:
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=0.055, radius2=0.055, depth=0.16)
        cant = mk('Canteen', bm)
        cant.data.transform(Matrix.Translation(Vector((h.x - 0.045, h.y, zb - 0.1))) @ Matrix.Rotation(math.radians(90), 4, 'Y') @ Matrix.Scale(1.0, 4))
        modapply(cant, 'BEVEL', width=0.01, segments=3, limit_method='ANGLE')
        GEAR.append((cant, (0.10, 0.12, 0.07), None))
    log('equipamento China: %d peças' % len(GEAR))


def ushanka():
    """O gorro de pele com as abas (o do Red Guard): a casca grossa a 3,5 cm da cabeça, o pelo pelo ruído (o volume irregular que lê de
    cima), a aba da frente dobrada pra cima com a estrela, as abas dos lados caídas até o queixo, cortado na testa."""
    C = COL['china']
    hv = [o for o in HEADS if o.name == 'HumanHead'][0]
    HV = np.array([tuple(v.co) for v in hv.data.vertices])
    top = float(HV[:, 2].max())
    c = (HV.min(0) + HV.max(0)) / 2.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=72, v_segments=44, radius=0.22)
    for v in bm.verts:
        v.co += Vector((c[0], c[1], top - 0.09))
    cap = mk('Ushanka', bm)
    sw = cap.modifiers.new('sw', 'SHRINKWRAP')
    sw.target = hv
    sw.wrap_method = 'NEAREST_SURFACEPOINT'
    sw.offset = 0.032
    bpy.ops.object.select_all(action='DESELECT')
    cap.select_set(True)
    bpy.context.view_layer.objects.active = cap
    bpy.ops.object.modifier_apply(modifier='sw')
    modapply(cap, 'CORRECTIVE_SMOOTH', factor=0.9, iterations=12, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    brow = float(HV[np.argmin(HV[:, 1])][2]) + 0.035
    bm = bmesh.new()
    bm.from_mesh(cap.data)
    def cut_z(v):
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        fr = max(0.0, -d.y / max(d.length, 1e-6))
        return (top - 0.11) + (brow - (top - 0.11)) * fr ** 1.4
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < cut_z(v)], context='VERTS')
    edge = [v for v in bm.verts if v.is_boundary]
    for v in edge:
        v.co.z = cut_z(v)
    # o pelo: o ruído saindo da casca (2 escalas)
    for v in bm.verts:
        p = v.co
        n_ = math.sin(p.x * 150.0 + math.sin(p.z * 110.0)) * math.sin(p.y * 140.0 + p.z * 70.0) + 0.4 * math.sin(p.x * 260.0 - p.z * 230.0)
        d = Vector((p.x - c[0], p.y - c[1], p.z - (top - 0.09))).normalized()
        v.co = p + d * (0.0035 * n_)
    bm.to_mesh(cap.data)
    bm.free()
    modapply(cap, 'CORRECTIVE_SMOOTH', factor=0.5, iterations=3, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    modapply(cap, 'SOLIDIFY', thickness=0.012, offset=1.0)
    modapply(cap, 'SUBSURF', levels=1)
    GEAR.append((cap, C['fur'], 'head'))
    dg = bpy.context.evaluated_depsgraph_get()
    T_HEAD = BVHTree.FromObject(hv, dg)
    # a aba da frente dobrada pra cima: a faixa grossa de pelo na testa, com a estrela
    T_CAP = BVHTree.FromObject(cap, bpy.context.evaluated_depsgraph_get())
    # a aba da frente dobrada pra cima: a faixa curva que segue o gorro na testa (o arco de ±65°), 5 cm de alto, 1,8 cm de pelo, a partir
    # da borda de verdade do gorro na frente (onde o raio não acha o gorro, a testa + a espessura do pelo)
    CV = np.array([tuple(v.co) for v in cap.data.vertices])
    fr = CV[(np.abs(CV[:, 0] - c[0]) < 0.03) & (CV[:, 1] < c[1])]
    zr = float(fr[:, 2].min()) if len(fr) else brow
    bm = bmesh.new()
    grid = []
    for j in range(5):
        z = zr - 0.012 + 0.052 * j / 4
        row = []
        for k in range(25):
            a = math.radians(-65 + 130 * k / 24)
            o = Vector((c[0] + 0.5 * math.sin(a), c[1] - 0.5 * math.cos(a), z))
            d = (Vector((c[0], c[1], z)) - o).normalized()
            h, n, _, _ = T_CAP.ray_cast(o, d, 0.8)
            if h is None:
                h, n, _, _ = T_HEAD.ray_cast(o, d, 0.8)
                h = h + n * 0.03 if h is not None else None
            row.append(bm.verts.new(h + n * 0.004 if h is not None else o))
        grid.append(row)
    for j in range(4):
        for k in range(24):
            bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
    band = mk('FrontFlap', bm)
    modapply(band, 'SOLIDIFY', thickness=0.018, offset=1.0)
    modapply(band, 'BEVEL', width=0.006, segments=2, limit_method='ANGLE')
    modapply(band, 'SUBSURF', levels=1)
    GEAR.append((band, tuple(k * 1.12 for k in C['fur']), 'head'))
    T_BAND = BVHTree.FromObject(band, bpy.context.evaluated_depsgraph_get())
    h, n, _, _ = T_BAND.ray_cast(Vector((c[0], c[1] - 0.5, zr + 0.014)), Vector((0.0, 1.0, 0.0)), 0.8)
    if h is not None:
        bm = bmesh.new()
        r1, r2 = 0.016, 0.0065
        verts = []
        for k in range(10):
            a = math.pi / 2 + k * math.pi / 5
            r = r1 if k % 2 == 0 else r2
            verts.append(bm.verts.new((r * math.cos(a), 0.0, r * math.sin(a))))
        bm.faces.new(verts)
        star = mk('Star', bm)
        star.data.transform(Matrix.Translation(h + Vector((0.0, -0.003, 0.0))))
        modapply(star, 'SOLIDIFY', thickness=0.004, offset=0.0)
        GEAR.append((star, C['star'], 'head'))
    # as abas dos lados caídas até o queixo: a placa curva de pelo que segue a cabeça (cobre a orelha)
    for sd in (-1.0, 1.0):
        flap = surface_patch('EarFlap_%d' % int(sd), T_HEAD, (c[0] + sd * 0.07, c[1] + 0.008, top - 0.165), (sd, 0.0, 0.0),
                             (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (-0.05, 0.05), (-0.075, 0.055), 8, 10, 0.024, 0.016)
        GEAR.append((flap, C['fur'], 'head'))
    log('gorro de pele pronto')


def tunic_skirt(color, z_top, z_bot, flare, slit_top, gaps_deg=(0.0, 180.0), folds=11):
    """(01/11) A barra da túnica (o kameez): a saia em volta das duas pernas, da cintura até o joelho, aberta dos DOIS LADOS a partir da
    virilha (a frente e as costas soltas, como a túnica de verdade), com as dobras do algodão. Os pesos como os do casacão."""
    legs = (DOM == 'pelvis') | starts(['thigh', 'calf'])
    PL = P
    cx = float(bh('pelvis')[0])
    rings, seg = 12, 80
    gap = math.radians(5.0)
    shape = []
    for j in range(rings + 1):
        t = j / rings
        z = z_top + (z_bot - z_top) * t
        sl = legs & (np.abs(PL[:, 2] - z) < 0.025)
        Q = PL[sl] if sl.any() else PL[legs]
        rx = float(np.abs(Q[:, 0] - cx).max()) + 0.02 + flare * t
        yf, yb = float(Q[:, 1].min()), float(Q[:, 1].max())
        shape.append((z, t, rx, (yf + yb) / 2.0, (yb - yf) / 2.0 + 0.02 + flare * t))
    tl = float(bh('thigh_l')[0]) - cx
    for part in (0, 1):                     # a frente e as costas, cada uma entre as duas fendas
        a0 = math.radians(gaps_deg[1] if part == 0 else gaps_deg[0]) + gap
        a1 = a0 + math.pi - 2.0 * gap
        bm = bmesh.new()
        grid = []
        for z, t, rx, yc, ry in shape:
            row = []
            closed = z >= slit_top
            b0, b1 = (a0 - gap, a1 + gap) if closed else (a0, a1)
            for k in range(seg // 2 + 1):
                a = b0 + (b1 - b0) * k / (seg // 2)
                ca, sa = math.cos(a), math.sin(a)
                fold = 1.0 + 0.045 * t * math.sin(a * folds + 1.1 * math.sin(a * 3.0 + part))
                x = cx + rx * fold * math.copysign(abs(ca) ** (2.0 / 2.3), ca)
                y = yc + ry * fold * math.copysign(abs(sa) ** (2.0 / 2.3), sa)
                zz = z + 0.015 * math.sin(a * 6.0 + part) * t ** 4
                row.append(bm.verts.new((x, y, zz)))
            grid.append(row)
        for j in range(rings):
            for k in range(seg // 2):
                bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
        ob = mk('Tunic_%d' % part, bm)
        modapply(ob, 'SOLIDIFY', thickness=0.006, offset=1.0)
        modapply(ob, 'SUBSURF', levels=1)
        gp, gl, gr = ob.vertex_groups.new(name='pelvis'), ob.vertex_groups.new(name='thigh_l'), ob.vertex_groups.new(name='thigh_r')
        for v in ob.data.vertices:
            h = float(np.clip((Z_PELVIS - v.co.z) / 0.25, 0.0, 1.0))
            sx = float(np.clip((v.co.x - cx) / 0.10, -1.0, 1.0)) * (1.0 if tl > 0 else -1.0)
            wl = h * (0.5 + 0.5 * sx)
            wr = h * (0.5 - 0.5 * sx)
            gp.add([v.index], 1.0 - h, 'REPLACE')
            if wl > 0:
                gl.add([v.index], wl, 'REPLACE')
            if wr > 0:
                gr.add([v.index], wr, 'REPLACE')
        GEAR.append((ob, color, 'custom'))


def gear_gla():
    C = COL['gla']
    s3 = bh('spine_03')
    cx = float(s3[0])
    tunic_skirt('tunic', Z_PELVIS + 0.06, Z_KNEE + 0.02, 0.06, Z_PELVIS - 0.10)
    # o colete civil aberto (o "waistcoat" de lã escura): a casca do tronco a 1,3 cm, aberta na frente, do peito à cintura
    zlo, zhi = float(Z_PELVIS + 0.06), float(bh('neck_01')[2] - 0.07)
    bm = bmesh.new()
    grid = []
    seg, rings = 64, 12
    for j in range(rings + 1):
        z = zlo + (zhi - zlo) * j / rings
        row = []
        for k in range(seg + 1):
            a = math.radians(-90 + 9) + math.radians(360 - 18) * k / seg
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            row.append(bm.verts.new(h + n * 0.013 if h is not None else o))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][k + 1], grid[j + 1][k + 1], grid[j + 1][k]))
    vest = mk('Waistcoat', bm)
    modapply(vest, 'SOLIDIFY', thickness=0.008, offset=1.0)
    modapply(vest, 'BEVEL', width=0.003, segments=2, limit_method='ANGLE')
    modapply(vest, 'SUBSURF', levels=1)
    GEAR.append((vest, C['vest'], None))
    # a faixa de pano na cintura (o nó do lado)
    zb = float(Z_PELVIS + 0.07)
    bm = bmesh.new()
    grid = []
    for j in range(3):
        z = zb - 0.03 + 0.03 * j
        row = []
        for k in range(64):
            a = 2.0 * math.pi * k / 64
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            row.append(bm.verts.new(h + n * 0.016 if h is not None else o))
        grid.append(row)
    for j in range(2):
        for k in range(64):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % 64], grid[j + 1][(k + 1) % 64], grid[j + 1][k]))
    sash = mk('Sash', bm)
    modapply(sash, 'SOLIDIFY', thickness=0.008, offset=1.0)
    modapply(sash, 'SUBSURF', levels=1)
    GEAR.append((sash, C['sash'], None))
    h, n = hit((cx + 0.6, float(s3[1]), zb), (-1.0, 0.0, 0.0), 0.8, BVH_T)
    if h is not None:
        GEAR.append((rbox('SashKnot', (0.03, 0.06, 0.05), (h.x + 0.025, h.y, zb), 0.015, 3), C['sash'], None))
        GEAR.append((rbox('SashTail', (0.012, 0.05, 0.16), (h.x + 0.03, h.y + 0.01, zb - 0.1), 0.006, 2), C['sash'], None))
    # a bandoleira de couro (do ombro esquerdo ao quadril direito) com as 3 bolsas de carregador de AK
    ul = bh('upperarm_l')
    tr = bh('thigh_r')
    x0, x1 = cx + 0.75 * (ul[0] - cx), cx + 1.6 * (tr[0] - cx)
    pts = []
    top = hit((x0, float(s3[1]), float(bh('neck_01')[2]) + 0.4), (0.0, 0.0, -1.0), 0.8, BVH_T)[0]
    if top is not None:
        pts.append(top + Vector((0.0, 0.0, 0.025)))
    for i in range(16):
        t = i / 15.0
        x = x0 * (1.0 - t) + x1 * t
        z = (float(bh('neck_01')[2]) - 0.03) * (1.0 - t) + (Z_PELVIS + 0.02) * t
        h, n = hit((x, -0.8, z), (0.0, 1.0, 0.0), 1.6, BVH_T)
        if h is not None:
            pts.append(h + Vector((0.0, -0.028, 0.0)))
    if len(pts) > 5:
        st = strap_path(pts, 0.05, 0.008, 'Bandolier')
        GEAR.append((st, C['leather'], None))
        for k, i in enumerate((6, 9, 12)):
            if i < len(pts):
                p_ = pts[i]
                GEAR.append((rbox('MagPouch_%d' % k, (0.075, 0.035, 0.09), (p_.x, p_.y - 0.02, p_.z), 0.008, 3), C['leather'], None))
                GEAR.append((rbox('MagTop_%d' % k, (0.035, 0.022, 0.03), (p_.x, p_.y - 0.02, p_.z + 0.055), 0.004, 2), C['mag'], None))
    log('equipamento GLA: %d peças' % len(GEAR))


def head_wrap():
    """O lenço do rebelde (o shemagh): o pano enrolado na cabeça (a casca a 2 cm com as dobras em volta, o volume que lê de cima), a ponta
    caindo atrás, e a parte que cobre o rosto do nariz pra baixo e o pescoço (só os olhos de fora). Claro com o quadriculado escuro."""
    C = COL['gla']
    hv = [o for o in HEADS if o.name == 'HumanHead'][0]
    HV = np.array([tuple(v.co) for v in hv.data.vertices])
    top = float(HV[:, 2].max())
    c = (HV.min(0) + HV.max(0)) / 2.0
    dg = bpy.context.evaluated_depsgraph_get()
    T_HEAD = BVHTree.FromObject(hv, dg)
    eyes = [o for o in HEADS if o.name.startswith('Eye_')]
    ez = float(np.mean([np.mean([v.co.z for v in o.data.vertices]) for o in eyes])) if eyes else top - 0.12
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=72, v_segments=44, radius=0.22)
    for v in bm.verts:
        v.co += Vector((c[0], c[1], top - 0.09))
    w = mk('Wrap', bm)
    sw = w.modifiers.new('sw', 'SHRINKWRAP')
    sw.target = hv
    sw.wrap_method = 'NEAREST_SURFACEPOINT'
    sw.offset = 0.028
    bpy.ops.object.select_all(action='DESELECT')
    w.select_set(True)
    bpy.context.view_layer.objects.active = w
    bpy.ops.object.modifier_apply(modifier='sw')
    bm = bmesh.new()
    bm.from_mesh(w.data)
    # o rosto aberto dos olhos (a faixa entre a testa e o nariz), o resto coberto até o pescoço
    kill = []
    for v in bm.verts:
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        fr = max(0.0, -d.y / max(d.length, 1e-6))
        if v.co.z < float(bh('neck_01')[2]) + 0.02:
            kill.append(v)
        elif fr > 0.55 and ez - 0.022 < v.co.z < ez + 0.03:
            kill.append(v)
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    # as dobras do pano enrolado: os sulcos em volta da cabeça (descendo na diagonal) e mais volume no alto
    for v in bm.verts:
        p = v.co
        ang = math.atan2(p.y - c[1], p.x - c[0])
        r = Vector((p.x - c[0], p.y - c[1], p.z - (top - 0.09))).normalized()
        wrinkle = 0.004 * math.sin((p.z - 0.25 * ang * 0.05) * 160.0 + ang * 2.0)
        bulk = 0.02 * max(0.0, (p.z - (ez + 0.03)) / 0.12)
        v.co = p + r * (wrinkle + bulk)
    bm.to_mesh(w.data)
    bm.free()
    modapply(w, 'CORRECTIVE_SMOOTH', factor=0.5, iterations=4, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    modapply(w, 'SOLIDIFY', thickness=0.006, offset=1.0)
    modapply(w, 'SUBSURF', levels=1)
    GEAR.append((w, 'shemagh', 'head'))
    # a ponta caindo atrás do pescoço
    tail = surface_patch('WrapTail', T_HEAD, (c[0], c[1] + 0.07, float(bh('neck_01')[2]) + 0.05), (0.0, 1.0, 0.0),
                         (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (-0.06, 0.06), (-0.12, 0.03), 6, 8, 0.03, 0.006)
    GEAR.append((tail, 'shemagh', None))
    log('lenço pronto')


def emr_color(p, C):
    """A cor da EMR num ponto (a mesma conta da farda, pras peças com capa: o capacete, o colete)."""
    q = np.floor(np.array(p) / 0.025) * 0.025 + 0.0125
    a, b = camo(q[None, :], 11.0)
    a3, _ = camo(q[None, :] - 0.21, 37.0)
    if a3[0] > 1.6:
        return C['spot3']
    if a[0] > 0.8:
        return C['spot1']
    if b[0] > 1.0:
        return C['spot2']
    return C['cloth']


def gear_russia():
    C = COL['russia']
    s3 = bh('spine_03')
    cx = float(s3[0])
    zc = float(s3[2] + 0.10)
    yf = surface_y(cx, zc, -1.0)
    yb = surface_y(cx, zc, 1.0)
    # o 6B45: as placas grandes (29 × 36 cm), a capa na cor da armadura
    GEAR.append((panel('PlateF', (cx, yf - 0.036, zc), 0.30, 0.37, 0.05, 14, 16, 0.24, -1.0, 0.04, 0.008), C['armor'], None))
    GEAR.append((panel('PlateB', (cx, yb + 0.036, zc), 0.30, 0.38, 0.04, 14, 16, 0.26, 1.0, 0.04, 0.008), C['armor'], None))
    # a cinta lateral
    zlo, zhi = float(s3[2] - 0.09), float(s3[2] + 0.08)
    bm = bmesh.new()
    seg, rings = 64, 7
    grid = []
    for j in range(rings + 1):
        z = zlo + (zhi - zlo) * j / rings
        row = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            o = Vector((cx + 0.6 * math.cos(a), float(s3[1]) + 0.6 * math.sin(a), z))
            h, n = hit(o, Vector((cx, float(s3[1]), z)) - o, 0.8, BVH_T)
            row.append(bm.verts.new(h + n * 0.018 if h is not None else o))
        grid.append(row)
    for j in range(rings):
        for k in range(seg):
            bm.faces.new((grid[j][k], grid[j][(k + 1) % seg], grid[j + 1][(k + 1) % seg], grid[j + 1][k]))
    cum = mk('Cummerbund', bm)
    modapply(cum, 'SOLIDIFY', thickness=0.016, offset=1.0)
    modapply(cum, 'SUBSURF', levels=1)
    GEAR.append((cum, C['armor'], None))
    # a gola alta de proteção (o anel grosso e alargado em volta do pescoço) e a placa da virilha pendurada na frente
    n1 = bh('neck_01')
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=48, radius1=0.15, radius2=0.105, depth=0.10)
    for v in bm.verts:
        v.co = Vector((n1[0] + v.co.x, n1[1] + 0.01 + v.co.y * 0.92, n1[2] - 0.01 + v.co.z))
    collar = mk('Collar', bm)
    modapply(collar, 'SOLIDIFY', thickness=0.022, offset=1.0)
    modapply(collar, 'BEVEL', width=0.006, segments=2, limit_method='ANGLE')
    modapply(collar, 'SUBSURF', levels=1)
    GEAR.append((collar, C['armor'], None))
    zg = float(Z_PELVIS - 0.02)
    groin = panel('Groin', (cx, surface_y(cx, zg, -1.0) - 0.022, zg), 0.17, 0.16, 0.0, 8, 8, 0.2, -1.0, 0.02, 0.006)
    GEAR.append((groin, C['armor'], 'pelvis'))
    # as ombreiras: a placa curva no alto de cada braço
    for sd in ('l', 'r'):
        ua, la = bh('upperarm_' + sd), bh('lowerarm_' + sd)
        c0 = ua + (la - ua) * 0.22
        out = Vector(((1.0 if c0[0] > cx else -1.0), 0.0, 0.55)).normalized()
        sp = surface_patch('Shoulder_' + sd, BVH2, tuple(c0), tuple(out), (0.0, 1.0, 0.0), tuple(out.cross(Vector((0.0, 1.0, 0.0)))),
                           (-0.075, 0.075), (-0.07, 0.07), 8, 8, 0.012, 0.016)
        GEAR.append((sp, C['armor'], None))
    # o MOLLE na frente, 4 porta-carregadores (2 × 2), a granada, o rádio e a mochila pequena
    yfront = lambda x: yf - 0.036 - 0.02 + 0.24 * (1.0 - math.cos((x - cx) / 0.24))
    for o in molle('MolleF', cx - 0.13, cx + 0.13, zc + 0.15, lambda x, z: yfront(x), -1.0, rows=2):
        GEAR.append((o, C['dark'], None))
    for r_ in range(2):
        for k in (-1.5, -0.5, 0.5, 1.5):
            x = cx + k * 0.066
            z = zc - 0.075 + r_ * 0.0
            if r_ == 1:
                continue
            GEAR.append((rbox('MagPouch_%d' % int(k * 2), (0.062, 0.046, 0.13), (x, yfront(x) - 0.024, zc - 0.07), 0.009, 3), C['armor'], None))
            GEAR.append((rbox('MagTop_%d' % int(k * 2), (0.05, 0.028, 0.03), (x, yfront(x) - 0.024, zc + 0.005), 0.005, 2), C['mag'], None))
    GEAR.append((rbox('Grenade', (0.06, 0.05, 0.08), (cx + 0.11, yfront(cx + 0.11) - 0.026, zc + 0.08), 0.012, 3), C['armor'], None))
    h, n = hit((cx - 0.6, float(s3[1]) + 0.02, zc - 0.03), (1.0, 0.0, 0.0), 0.8, BVH_T)
    if h is not None:
        GEAR.append((rbox('Radio', (0.05, 0.08, 0.15), (h.x - 0.045, float(s3[1]) + 0.02, zc - 0.03), 0.01, 3), C['armor'], None))
        GEAR.append((rbox('Antenna', (0.007, 0.007, 0.36), (h.x - 0.045, float(s3[1]) + 0.04, zc + 0.2), 0.003, 1), C['dark'], None))
    ybk = yb + 0.036 + 0.02 - 0.26 * (1.0 - math.cos(0.0))
    GEAR.append((rbox('Pack', (0.26, 0.14, 0.32), (cx, ybk + 0.075, zc - 0.03), 0.035, 3), C['armor'], None))
    log('equipamento Rússia: %d peças' % len(GEAR))


def helmet_6b47():
    """O 6B47: a casca grande a 2,4 cm (a capa EMR), cortada na sobrancelha e acima da orelha, a faixa elástica dos óculos em volta e
    os óculos em cima da aba na frente (o que dá a cara do soldado russo de hoje)."""
    C = COL['russia']
    hv = [o for o in HEADS if o.name == 'HumanHead'][0]
    HV = np.array([tuple(v.co) for v in hv.data.vertices])
    top = float(HV[:, 2].max())
    c = (HV.min(0) + HV.max(0)) / 2.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=64, v_segments=40, radius=0.21)
    for v in bm.verts:
        v.co += Vector((c[0], c[1], top - 0.09))
    hel = mk('Helmet', bm)
    sw = hel.modifiers.new('sw', 'SHRINKWRAP')
    sw.target = hv
    sw.wrap_method = 'NEAREST_SURFACEPOINT'
    sw.offset = 0.024
    bpy.ops.object.select_all(action='DESELECT')
    hel.select_set(True)
    bpy.context.view_layer.objects.active = hel
    bpy.ops.object.modifier_apply(modifier='sw')
    modapply(hel, 'CORRECTIVE_SMOOTH', factor=0.9, iterations=10, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    eyes = [o for o in HEADS if o.name.startswith('Eye_')]
    ez = float(np.mean([np.mean([v.co.z for v in o.data.vertices]) for o in eyes])) if eyes else top - 0.12
    bm = bmesh.new()
    bm.from_mesh(hel.data)
    def cut_z(v):
        d = Vector((v.co.x - c[0], v.co.y - c[1], 0.0))
        fr = max(0.0, -d.y / max(d.length, 1e-6))
        return (top - 0.15) + (ez + 0.035 - (top - 0.15)) * fr ** 1.5
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < cut_z(v)], context='VERTS')
    edge = [v for v in bm.verts if v.is_boundary]
    for v in edge:
        v.co.z = cut_z(v)
    for _ in range(3):
        bmesh.ops.smooth_vert(bm, verts=edge, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(hel.data)
    bm.free()
    modapply(hel, 'SOLIDIFY', thickness=0.009, offset=1.0)
    modapply(hel, 'BEVEL', width=0.003, segments=2, limit_method='ANGLE')
    GEAR.append((hel, 'emr', 'head'))
    dg = bpy.context.evaluated_depsgraph_get()
    T_HEL = BVHTree.FromObject(hel, dg)
    # a faixa dos óculos: o anel escuro em volta do casco a ~5 cm da borda da frente
    zb = ez + 0.055
    bm = bmesh.new()
    grid = []
    for j in range(2):
        z = zb - 0.012 + 0.024 * j
        row = []
        for k in range(48):
            a = 2.0 * math.pi * k / 48
            o = Vector((c[0] + 0.5 * math.cos(a), c[1] + 0.5 * math.sin(a), z + 0.03 * max(0.0, math.sin(a))))
            h, n, _, _ = T_HEL.ray_cast(o, (Vector((c[0], c[1], o.z)) - o).normalized(), 0.8)
            row.append(bm.verts.new(h + n * 0.003 if h is not None else o))
        grid.append(row)
    for k in range(48):
        bm.faces.new((grid[0][k], grid[0][(k + 1) % 48], grid[1][(k + 1) % 48], grid[1][k]))
    band = mk('GoggleBand', bm)
    modapply(band, 'SOLIDIFY', thickness=0.004, offset=1.0)
    GEAR.append((band, C['dark'], 'head'))
    # os óculos na frente, em cima da aba: as duas lentes num corpo só
    h, n, _, _ = T_HEL.ray_cast(Vector((c[0], c[1] - 0.5, zb)), Vector((0.0, 1.0, 0.0)), 0.8)
    if h is not None:
        GEAR.append((rbox('GoggleBody', (0.15, 0.03, 0.05), (c[0], h.y - 0.012, zb), 0.012, 3), C['dark'], 'head'))
        for sd in (-1.0, 1.0):
            GEAR.append((rbox('GoggleLens_%d' % int(sd), (0.055, 0.012, 0.035), (c[0] + sd * 0.035, h.y - 0.028, zb), 0.008, 2), C['lens'], 'head'))
    log('6B47 pronto')


def weld_gear():
    """O equipamento na malha do corpo: os pesos pelo corpo (Data Transfer, a face mais perto) ou o osso único, a cor nos vértices."""
    for ob, col, bone in GEAR:
        if bone == 'custom':
            pass                         # os pesos já vieram (a saia do casacão)
        elif bone is None:
            dt = ob.modifiers.new('dt', 'DATA_TRANSFER')
            dt.object = BODY
            dt.use_vert_data = True
            dt.data_types_verts = {'VGROUP_WEIGHTS'}
            dt.vert_mapping = 'POLYINTERP_NEAREST'
            dt.layers_vgroup_select_src = 'ALL'
            dt.layers_vgroup_select_dst = 'NAME'
            bpy.ops.object.select_all(action='DESELECT')
            ob.select_set(True)
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.datalayout_transfer(modifier='dt')
            bpy.ops.object.modifier_apply(modifier='dt')
        else:
            g = ob.vertex_groups.new(name=bone)
            g.add([v.index for v in ob.data.vertices], 1.0, 'REPLACE')
        ca = ob.data.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
        for v in ob.data.vertices:
            if col == 'emr':
                cc = emr_color(tuple(v.co), COL['russia'])
            elif col == 'shemagh':
                # o shemagh: o pano claro com a grade escura (as faixas de ~1 cm a cada 3 cm, nos dois sentidos, e a borda mais escura)
                g = (abs(math.sin(v.co.x * 105.0 + v.co.z * 30.0)) < 0.22) or (abs(math.sin(v.co.z * 105.0 - v.co.x * 20.0)) < 0.22)
                cc = (0.20, 0.10, 0.08) if g else COL['gla']['wrap']
            elif col == 'tunic':
                wear = 0.5 + 0.5 * math.sin(v.co.x * 23.0 + math.sin(v.co.z * 17.0) * 2.0) * math.sin(v.co.y * 19.0 + v.co.z * 11.0)
                dust = max(0.0, min(1.0, (Z_KNEE + 0.15 - v.co.z) / 0.3))
                base = np.array(COL['gla']['cloth']) * (0.86 + 0.18 * wear)
                cc = tuple(base * (1.0 - 0.3 * dust) + np.array((0.33, 0.27, 0.18)) * 0.3 * dust)
            else:
                cc = col
            ca.data[v.index].color = (cc[0], cc[1], cc[2], 1.0)
        ob.data.materials.clear()

def team_faces_mask():
    """A faixa do time no braço esquerdo (o distintivo do ombro, no terço de cima do braço), nos índices do corpo."""
    m = np.zeros(len(DOM), dtype=bool)
    up = bh('upperarm_l')
    lo = bh('lowerarm_l')
    ax = lo - up
    L = float(np.linalg.norm(ax))
    t = ((P0 - up) @ ax) / max(L * L, 1e-6)
    arm = starts(['upperarm']) & np.char.endswith(DOM.astype(str), '_l')
    m[arm & (t > 0.16) & (t < 0.30)] = True
    return m


def set_materials_for_export():
    """Os dois slots: VC (a cor nos vértices) e VC_Team (a cor do time no lugar da cor: o jogo põe TeamSlot 1). O time pelo grupo
    TeamBand (marcado no corpo antes de tirar a cabeça: o grupo sobrevive à junção), que sai antes de exportar."""
    g = BODY.vertex_groups.get('TeamBand')
    team_v = np.zeros(len(ME.vertices), dtype=bool)
    if g is not None:
        for v in ME.vertices:
            for gg in v.groups:
                if gg.group == g.index and gg.weight > 0.5:
                    team_v[v.index] = True
        BODY.vertex_groups.remove(g)
    vc = bpy.data.materials.get('VC') or bpy.data.materials.new('VC')
    vt = bpy.data.materials.get('VC_Team') or bpy.data.materials.new('VC_Team')
    ME.materials.clear()
    ME.materials.append(vc)
    ME.materials.append(vt)
    for poly in ME.polygons:
        poly.material_index = 1 if all(team_v[i] for i in poly.vertices) else 0


def bake_ao():
    """(01/11) A oclusão de ambiente assada nos vértices (Cycles na CPU) e multiplicada na cor: a sombra embaixo dos porta-carregadores,
    entre a placa e o pano, nas dobras, no pescoço embaixo do capacete. É o que tira o "chapado" de longe (a 67 m a fresta vira o
    contraste que separa as peças)."""
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 48
    w = bpy.data.worlds.get('wao') or bpy.data.worlds.new('wao')
    sc.world = w
    tmp = bpy.data.materials.new('ao_tmp')
    tmp.use_nodes = True
    keep = [m for m in ME.materials]
    ME.materials.clear()
    ME.materials.append(tmp)
    ao = ME.color_attributes.new('AO', 'FLOAT_COLOR', 'POINT')
    ME.color_attributes.active_color = ao
    bpy.ops.object.select_all(action='DESELECT')
    BODY.select_set(True)
    bpy.context.view_layer.objects.active = BODY
    sc.render.bake.target = 'VERTEX_COLORS'
    sc.cycles.bake_type = 'AO'
    sc.render.bake.margin = 0
    bpy.ops.object.bake(type='AO', target='VERTEX_COLORS')
    col = ME.color_attributes['Col']
    n = len(ME.vertices)
    A = np.zeros(n * 4, dtype=np.float32)
    ao.data.foreach_get('color', A)
    Cc = np.zeros(n * 4, dtype=np.float32)
    col.data.foreach_get('color', Cc)
    A = A.reshape(n, 4)[:, 0]
    Cc = Cc.reshape(n, 4)
    k = 0.35 + 0.65 * np.clip(A, 0.0, 1.0) ** 0.8
    Cc[:, :3] *= k[:, None]
    col.data.foreach_set('color', Cc.ravel())
    ME.color_attributes.remove(ao)
    ME.color_attributes.active_color = ME.color_attributes['Col']
    ME.materials.clear()
    for m in keep:
        ME.materials.append(m)
    log('oclusão assada: média %.2f, mínimo %.2f' % (float(A.mean()), float(A.min())))


def export_fbx():
    path = os.path.join(OUT, 'SKM_Soldier_%s.fbx' % TAG.upper())
    bpy.ops.object.select_all(action='DESELECT')
    ARM.select_set(True)
    BODY.select_set(True)
    bpy.context.view_layer.objects.active = ARM
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                             bake_anim=False, use_armature_deform_only=False, mesh_smooth_type='FACE', apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y', primary_bone_axis='Y',
                             secondary_bone_axis='X', armature_nodetype='NULL', colors_type='SRGB', use_tspace=False)
    log('exportado', path, len(ME.vertices), 'vértices', len(ME.polygons), 'faces')
    return path


SLIT = {}


def balaclava():
    """(01/11, ele: "a cor pintada na pele ficou ruim") a balaclava de verdade: o pano de malha encolhido a 4 mm da cabeça inteira, com a
    fresta dos olhos (a medida pelos contornos do rosto), a trama fina da malha e o pescoço até dentro da gola; não tinta na pele."""
    C = COL.get(FACTION, COL['usa'])
    hv = [o for o in HEADS if o.name == 'HumanHead'][0]
    HV = np.array([tuple(v.co) for v in hv.data.vertices])
    top = float(HV[:, 2].max())
    c = (HV.min(0) + HV.max(0)) / 2.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=96, v_segments=64, radius=0.25)
    for v in bm.verts:
        v.co = Vector((c[0] + v.co.x, c[1] + v.co.y, top - 0.13 + v.co.z * 1.25))
    ob = mk('Balaclava', bm)
    sw = ob.modifiers.new('sw', 'SHRINKWRAP')
    sw.target = hv
    sw.wrap_method = 'NEAREST_SURFACEPOINT'
    sw.offset = 0.004
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier='sw')
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    kill = []
    for v in bm.verts:
        fr = v.co.y < c[1] - 0.03
        if v.co.z < float(bh('neck_01')[2]) - 0.01:
            kill.append(v)
        elif fr and SLIT and SLIT['z_lo'] < v.co.z < SLIT['z_hi'] and SLIT['x0'] < v.co.x < SLIT['x1']:
            kill.append(v)
    bmesh.ops.delete(bm, geom=kill, context='VERTS')
    for v in bm.verts:   # a trama da malha: o canelado fino e vertical
        v.co.x += 0.0006 * math.sin(v.co.x * 900.0)
    bm.to_mesh(ob.data)
    bm.free()
    modapply(ob, 'CORRECTIVE_SMOOTH', factor=0.6, iterations=4, smooth_type='LENGTH_WEIGHTED', use_only_smooth=True)
    modapply(ob, 'SOLIDIFY', thickness=0.003, offset=1.0)
    zh, zn2, zn1 = bh('head')[2], bh('neck_02')[2], bh('neck_01')[2]
    gh, g2_, g1 = ob.vertex_groups.new(name='head'), ob.vertex_groups.new(name='neck_02'), ob.vertex_groups.new(name='neck_01')
    for v in ob.data.vertices:   # os pesos da cabeça humana (a cabeça no head, o pescoço passando pro neck_02 e o neck_01)
        z = v.co.z
        wh = float(np.clip((z - zn2) / max(zh - zn2, 1e-3), 0.0, 1.0))
        w1 = float(np.clip((zn2 - z) / max(zn2 - zn1, 1e-3), 0.0, 1.0))
        w2 = max(0.0, 1.0 - wh - w1)
        if wh > 0:
            gh.add([v.index], wh, 'REPLACE')
        if w2 > 0:
            g2_.add([v.index], w2, 'REPLACE')
        if w1 > 0:
            g1.add([v.index], w1, 'REPLACE')
    GEAR.append((ob, C.get('balaclava', (0.045, 0.05, 0.04)), 'custom'))
    log('balaclava de pano pronta')


HEADS = []
P = P0.copy()
P, R = dress(P)
apply_positions(P)
paint(R)
BODY.vertex_groups.new(name='TeamBand').add(np.where(team_faces_mask())[0].tolist(), 1.0, 'REPLACE')
BVH_T = part_bvh(starts(['spine', 'pelvis', 'clavicle', 'neck']))
BVH_LEG = {sd: part_bvh(starts(['thigh_', 'calf_']) & np.char.endswith(DOM.astype(str), '_' + sd)) for sd in ('l', 'r')}
# tira a cabeça do manequim (fica o pescoço, por dentro da gola) e põe a humana
bm = bmesh.new()
bm.from_mesh(ME)
headv = set(np.where(DOM == 'head')[0].tolist())
bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(v.index in headv for v in f.verts)], context='FACES')
bm.to_mesh(ME)
bm.free()
human_head()
BVH2 = body_bvh_now()
if K['gear'] == 'china':
    gear_china()
elif K['gear'] == 'gla':
    gear_gla()
elif K['gear'] == 'russia':
    gear_russia()
else:
    gear_usa()
if K['head'] == 'ushanka':
    ushanka()
elif K['head'] == 'wrap':
    head_wrap()
elif K['head'] == '6b47':
    if K.get('balaclava'):
        balaclava()
    helmet_6b47()
else:
    helmet_ach()
weld_gear()
bpy.ops.object.select_all(action='DESELECT')
for o in HEADS:
    o.data.materials.clear()
    o.select_set(True)
for o, _, _ in GEAR:
    o.select_set(True)
BODY.select_set(True)
bpy.context.view_layer.objects.active = BODY
bpy.ops.object.join()
ME = BODY.data
bake_ao()
set_materials_for_export()
export_fbx()
check_sheet(TAG)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'outfit_%s.blend' % TAG))
log('pronto', TAG)
