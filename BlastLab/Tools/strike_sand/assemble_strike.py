"""A gravação strike_chao v2 (23/10, tarde): a bola clássica aprovada + a areia em gás do Mantaflow + a cortina de grãos, numa
gravação BRM2 só (o formato do raymarch; o jogo troca o corpo do acerto inteiro por ela: BlastFxDirector SpawnWarhead, bOwn).

  python assemble_strike.py <saida.brm2> <r96_v0.brm2> <pasta_mf_npz> <pasta_graos> [chave=valor ...]

Por que a bola clássica dentro (e não um fogo novo do Mantaflow): o Felipe, 23/10, "a explosão é bonita, falta a interação com
o chão". A bola e a fuligem ficam as aprovadas (a r96_v0 na escala da clássica, 0,55, a espessura 1/0,55, no relógio dela:
os mesmos 56 instantes, 6 s, curva 1,7; nenhum quadro do fogo é inventado), e o que é novo é o chão. Os três materiais:
- o fogo e a fuligem: a bola (canais da fuligem, do calor e do combustível);
- a areia: a cortina de grãos (strike_grid.py: os grumos e o grosso duros, o fino solto borrado) + a areia em gás do Mantaflow
  (a onda de base, o véu do choque, o tronco), no canal da poeira; a poeira baixa da própria bola entra a 'poeira_bola';
- a terra escura (o jato central e o miolo dos jatos: a terra úmida de baixo e a fuligem da carga) no canal da fuligem, fria.
A velocidade da meia resolução (a mistura com movimento entre quadros do raymarch) é a média das três pesada pela extinção.
O cubo: 'dominio' m (40), 128³, o impacto no meio, o chão no z = 0; os eixos da Unreal (o y da simulação virado, como o
build_raymarch_vdb.py faz); o míssil vem de −X e vai pra +X (o jogo gira pelo rumo do míssil: RaymarchYawRad).
"""
import json
import os
import sys

import numpy as np

sys.path.insert(0, r'C:\Users\luisf\explosion\BlastLab\Tools')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import brm2  # noqa: E402
from brm2_downsample import read_brm2, macro  # noqa: E402
import strike_grid as sg  # noqa: E402

SOOT_EXT, DUST_EXT, ERODE_MEAN, VEL_MAX = 13.5, 5.5, 0.042 * 0.5, 40.0
argv = sys.argv[1:]
DST, BALL, MFDIR, GDIR = argv[:4]
P = dict(dominio=40.0, res=128, escala_bola=0.55, poeira_bola=0.6, mf_fator=3.0, mf_gama=0.8, ganho_grosso=0.06, ganho_fino=0.06,
         escura=1.0, limiar=0.004, so_ate=99, bola=1.0, raio_de=16.0, raio_ate=19.2, topo_de=33.0, topo_ate=38.0, bloco_min=0.25, pot_grosso=1.0, ref_grosso=0.6)
for a in argv[4:]:
    k, v = a.split('=', 1)
    P[k] = float(v)
n = int(P['res'])
D = P['dominio']
vm = D / n
nf = n // 2


def box_matrix(n_src, v_src, o_src, n_dst, v_dst, o_dst):
    W = np.zeros((n_dst, n_src), np.float32)
    for i in range(n_dst):
        a, b = o_dst + i * v_dst, o_dst + (i + 1) * v_dst
        j0 = max(int(np.floor((a - o_src) / v_src)), 0)
        j1 = min(int(np.ceil((b - o_src) / v_src)), n_src)
        for j in range(j0, j1):
            s0, s1 = o_src + j * v_src, o_src + (j + 1) * v_src
            W[i, j] = max(0.0, min(b, s1) - max(a, s0)) / v_dst
    return W


def resample_zyx(a, Wz, Wy, Wx):
    """a [z][y][x](+canais) → [Z][Y][X] pelas matrizes de área."""
    a = np.tensordot(Wz, a, axes=([1], [0]))
    a = np.tensordot(Wy, a, axes=([1], [1])).swapaxes(0, 1)
    a = np.tensordot(Wx, a, axes=([1], [2]))
    return np.moveaxis(a, 0, 2)


bhead, bframes = read_brm2(BALL)
Db = bhead['domain'] * P['escala_bola']
nb, nbf = bhead['res'], bhead['fuelRes']
# a base: a bola clássica (r96_v0: 56 quadros, 6 s, curva 1,7) ou uma gravação do Mantaflow já convertida (warhead_chao: a bola, a
# fuligem e a areia em gás dela; escala_bola=1, poeira_bola=1, o MF '-'); os grãos têm de estar nos instantes dela
NQ, DUR, CURVE = bhead['frames'], float(bhead['duration']), float(bhead['curve'])
WA = (box_matrix(nb, Db / nb, 0.0, n, vm, 0.0), box_matrix(nb, Db / nb, -Db / 2, n, vm, -D / 2), box_matrix(nb, Db / nb, -Db / 2, n, vm, -D / 2))
WF = (box_matrix(nbf, Db / nbf, 0.0, nf, D / nf, 0.0), box_matrix(nbf, Db / nbf, -Db / 2, nf, D / nf, -D / 2),
      box_matrix(nbf, Db / nbf, -Db / 2, nf, D / nf, -D / 2))
thick = 1.0 / P['escala_bola']
st = sg.load(GDIR)
times = np.array([DUR * (q / (NQ - 1)) ** CURVE for q in range(NQ)])
assert len(st['times']) == NQ and np.allclose(st['times'], times, atol=1e-3), 'os grãos nos instantes da base'
bounds, light_pos, light_col, stats = [], [], [], []
# o recorte suave onde o raymarch já apaga (BlastRaymarch.usf BoxFade: o círculo de 72% a 98% do meio-lado e os 20% de cima): o que
# fica lá fora não se vê no jogo e só gasta blocos do atlas
_c = (np.arange(n) + 0.5) * vm - D / 2
_r = np.sqrt(_c[None, :] ** 2 + _c[:, None] ** 2)
_k = np.clip((_r - P['raio_de']) / (P['raio_ate'] - P['raio_de']), 0.0, 1.0)
_zk = np.clip(((np.arange(n) + 0.5) * vm - P['topo_de']) / (P['topo_ate'] - P['topo_de']), 0.0, 1.0)
CROP = ((1.0 - _k * _k * (3.0 - 2.0 * _k))[None, :, :] * (1.0 - _zk * _zk * (3.0 - 2.0 * _zk))[:, None, None]).astype(np.float32)


def to_unreal(a, vec=False):
    """[z][y][x] da simulação → o y da Unreal (y = −y): a ordem do y invertida e, nos vetores, o vy com o sinal trocado."""
    a = a[:, ::-1].copy()
    if vec:
        a[..., 1] *= -1.0
    return a


def gen():
    for q, (A0, F0) in enumerate(bframes):
        if q > P['so_ate']:
            break
        # a bola clássica (as grandezas físicas; a espessura da clássica: ×1/0,55 na escala dela)
        soot_b = 4.0 * (A0[..., 0] / 255.0) ** 2
        temp_b = A0[..., 1] / 255.0 * 1.6
        dust_b = 2.5 * (A0[..., 2] / 255.0) ** 2
        sfuel_b = F0[..., 0] / 255.0
        vel_b = (F0[..., 1:].astype(np.float32) / 255.0 * 2.0 - 1.0) * VEL_MAX
        soot = resample_zyx(soot_b, *WA) * thick * P['bola']
        temp = resample_zyx(temp_b, *WA) * (P['bola'] > 0)
        dust = resample_zyx(dust_b, *WA) * thick * P['poeira_bola'] * P['bola']
        sfuel = resample_zyx(sfuel_b, *WF) * (P['bola'] > 0)
        ext_b = SOOT_EXT * soot_b + DUST_EXT * dust_b
        ext_bhalf = ext_b.reshape(nbf, 2, nbf, 2, nbf, 2).mean(axis=(1, 3, 5))
        ext_bh = resample_zyx(ext_bhalf, *WF) * thick * P['bola']
        vel_bh = resample_zyx(vel_b * ext_bhalf[..., None], *WF) * thick * P['bola']
        # a areia em gás do Mantaflow
        if MFDIR != '-':
            mz = np.load(os.path.join(MFDIR, f'mf_{q:02d}.npz'))
            den = np.maximum(mz['den'].astype(np.float32), 0.0)
            dust_mf = to_unreal(np.clip(P['mf_fator'] * den ** P['mf_gama'], 0.0, 2.5))
            vel_mf = to_unreal(mz['vel'].astype(np.float32), True)
        else:
            dust_mf = np.zeros((n, n, n), np.float32)
            vel_mf = np.zeros((nf, nf, nf, 3), np.float32)
        ext_mfh = DUST_EXT * dust_mf.reshape(nf, 2, nf, 2, nf, 2).mean(axis=(1, 3, 5))
        # os grãos
        sand_g, dark_g, Wg, MVg, tg = sg.fields(GDIR, st, q, n, D, P['ganho_grosso'], P['ganho_fino'], pot=P['pot_grosso'], c_ref=P['ref_grosso'])
        assert abs(tg - times[q]) < 0.01, (q, tg, times[q])
        sand_g, dark_g = to_unreal(sand_g), to_unreal(dark_g)
        Wg, MVg = to_unreal(Wg), to_unreal(MVg, True)
        dark_soot = dark_g * (DUST_EXT / SOOT_EXT) * P['escura']   # a mesma extinção, no canal da fuligem
        soot = np.clip((soot + dark_soot) * CROP, 0.0, 4.0)
        dust = np.clip((dust + dust_mf + sand_g) * CROP, 0.0, 2.5)
        fuel = np.clip(sfuel, 0.0, 1.0)
        empty = (soot < P['limiar']) & (dust < P['limiar']) & (temp < 0.02)
        # (o orçamento do atlas: BlastRaymarch.h AtlasSide, o quadro mais cheio das gravações carregadas × 32) o bloco de 8³ quase vazio
        # (a extinção somada abaixo de 'bloco_min' m², a profundidade óptica média < ~0,04 de lado a lado) sai inteiro: a chuva de areia
        # rala espalhada pela caixa custava um bloco e os 26 vizinhos por grão; ~98% da extinção fica no pico (medido na v2b)
        ext_v = SOOT_EXT * soot + DUST_EXT * dust
        bsum = ext_v.reshape(n // 8, 8, n // 8, 8, n // 8, 8).sum(axis=(1, 3, 5)) * vm ** 3
        hot = (temp > 0.02).reshape(n // 8, 8, n // 8, 8, n // 8, 8).any(axis=(1, 3, 5))
        drop = (bsum < P['bloco_min']) & ~hot
        empty |= np.repeat(np.repeat(np.repeat(drop, 8, 0), 8, 1), 8, 2)
        soot[empty] = 0.0
        dust[empty] = 0.0
        temp[empty] = 0.0
        # a velocidade: a média das três pesada pela extinção
        ext_gh = DUST_EXT * Wg
        wsum = ext_bh + ext_mfh + ext_gh
        vsum = vel_bh + ext_mfh[..., None] * vel_mf + DUST_EXT * MVg
        vel = np.where(wsum[..., None] > 1e-5, vsum / np.maximum(wsum, 1e-5)[..., None], 0.0)
        ext = SOOT_EXT * np.maximum(soot - ERODE_MEAN / (1.0 + soot * 9.0), 0.0) + DUST_EXT * dust
        above = np.cumsum(ext[::-1], axis=0)[::-1] - 0.5 * ext
        sky = np.exp(-np.minimum(above * vm, 60.0))
        A = np.zeros((n, n, n, 4), np.uint8)
        A[..., 0] = np.round(np.sqrt(soot / 4.0) * 255)
        A[..., 1] = np.round(np.clip(temp / 1.6, 0, 1) * 255)
        A[..., 2] = np.round(np.sqrt(dust / 2.5) * 255)
        A[..., 3] = np.round(np.clip(sky, 0.0, 1.0) * 255)
        F = np.full((nf, nf, nf, 4), 128, np.uint8)
        F[..., 0] = np.round(np.clip(sfuel, 0, 1) * 255)
        F[..., 1:] = np.round(np.clip(vel / VEL_MAX * 0.5 + 0.5, 0.0, 1.0) * 255)
        occ = (A[..., 0] > 0) | (A[..., 1] > 0) | (A[..., 2] > 0)
        if occ.any():
            e = []
            for axis in (2, 1, 0):
                i = np.where(occ.any(axis=tuple(k for k in range(3) if k != axis)))[0]
                e.append((int(i.min()), int(i.max()) + 1))
            bounds.append([round(v, 4) for v in [-D / 2 + e[0][0] * vm, -D / 2 + e[1][0] * vm, e[2][0] * vm,
                                                  -D / 2 + e[0][1] * vm, -D / 2 + e[1][1] * vm, e[2][1] * vm]])
        else:
            bounds.append([0.0] * 6)
        lp = bhead['lightPos'][q]
        light_pos.append([round(lp[0] * P['escala_bola'], 5), round(lp[1] * P['escala_bola'], 5), round(lp[2] * P['escala_bola'], 5)])
        light_col.append(bhead['lightCol'][q])
        stats.append((q, float(times[q]), float(soot.sum() * vm ** 3), float(dust.sum() * vm ** 3), float(sand_g.sum() * vm ** 3),
                      float(dust_mf.sum() * vm ** 3), float((dust >= 2.49).mean() * 100)))
        if q % 6 == 0 or q == NQ - 1:
            print('MONTA q%02d t %.2f s: fuligem Σ %.0f, poeira Σ %.0f (grãos %.0f, gás %.0f), saturada %.3f%%' % stats[-1], flush=True)
        yield A, F, macro(A, 4)


last = min(int(P['so_ate']), NQ - 1)
head = {'source': '%s x%.2f + Mantaflow (areia em gas: %s) + graos (ejecta_strike2.py)' % (os.path.basename(BALL), P['escala_bola'], MFDIR if MFDIR == '-' else 'mf'),
        'res': n, 'fuelRes': nf, 'macroRes': n // 4, 'frames': last + 1,
        'duration': float(times[last]), 'curve': CURVE, 'domain': D, 'velMax': VEL_MAX,
        'bounds': bounds, 'lightPos': light_pos, 'lightCol': light_col, 'lightPhysical': bhead.get('lightPhysical', False), 'loop': False,
        'layers': {'ball': os.path.basename(BALL), 'ballScale': P['escala_bola'], 'mf': os.path.basename(os.path.normpath(MFDIR)),
                   'grains': os.path.basename(os.path.normpath(GDIR)), 'params': P}}
counts = brm2.write(DST, head, gen())
print('MONTA pronto: %s, %d quadros, blocos por quadro %d–%d, %.0f MB' % (DST, len(counts), min(counts), max(counts), os.path.getsize(DST) / 2 ** 20))
