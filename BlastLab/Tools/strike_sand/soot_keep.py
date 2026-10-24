"""As gravações dedicadas das ogivas com a fuligem já afinada dentro (23/10 noite), e a variante espelhada.

  python soot_keep.py <entrada.brm2> <saida.brm2> [keep=0.104] [poeira=1] [espelho=0] [frio_de=0.20] [frio_ate=0.40]

Por quê. No jogo (BlastFxDirector UpdateWarheads) a fuligem de uma ogiva afina depois da bola acesa pra SootKeep (0,18 na
Patriot, 0,104 no Tomahawk: a leitura do alvo, medida), mas o jogo só tem uma opacidade por volume: ela multiplica o corpo
inteiro, a fuligem E a areia (BlastRaymarch.usf, InstMisc.z em F.rb). Nas gravações dedicadas a areia que o choque levanta está
dentro (o canal da poeira): afinar o corpo apagava a areia no pico dela. Aqui a fuligem afina nos dados, só ela, e o jogo toca a
dedicada com a opacidade inteira (SootKeep 1).

O relógio é o da própria gravação, não o da Carga Pesada: a dedicada de lado e a de teto têm a câmera lenta de 0,5 no começo, e
o fogo delas acaba em outro instante. Pelo calor gravado: o volume quente (T/1,6 > 0,3) por quadro; o afinar vai de quando ele
cai pra 50% do pico até quando cai pra 10% (a regra do jogo: de a bola esfriar, 0,8 s, até o fim da fase A, 1,36 s da Carga
Pesada). E voxel a voxel só a fuligem fria afina (o fogo que ainda queima fica inteiro): × (1 − (1 − keep)·S(t)·frio(T)).
O keep é o do Tomahawk, a carga de referência das gravações (12 kg): a Patriot, tocada a 0,69, fica com a fuligem um pouco mais
rala que a regra dela (0,104 contra 0,18), do lado seguro da leitura.

espelho=1: o y virado (a mão do lobo da warhead_lado: o acerto que corre pro outro lado ao longo da face). O céu de cima (canal 4)
é refeito com a fuligem nova.
"""
import os
import sys

import numpy as np

sys.path.insert(0, r'C:\Users\luisf\explosion\BlastLab\Tools')
import brm2  # noqa: E402
from brm2_downsample import read_brm2, macro  # noqa: E402

SOOT_EXT, DUST_EXT, ERODE_MEAN = 13.5, 5.5, 0.042 * 0.5
src, dst = sys.argv[1], sys.argv[2]
P = dict(keep=0.104, poeira=1.0, espelho=0.0, frio_de=0.20, frio_ate=0.40, quente=0.3)
for a in sys.argv[3:]:
    k, v = a.split('=', 1)
    P[k] = float(v)


def sstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


head, frames = read_brm2(src)
n, D = head['res'], head['domain']
vm = D / n
ts = np.array([head['duration'] * (q / (head['frames'] - 1)) ** head['curve'] for q in range(head['frames'])])
hot = []
for A, F in frames:
    hot.append(float((A[..., 1] > P['quente'] * 255).sum()) * vm ** 3)
hot = np.array(hot)
qp = int(np.argmax(hot))
after = np.arange(len(hot)) > qp
t50 = ts[np.nonzero(after & (hot < 0.5 * hot[qp]))[0][0]] if (after & (hot < 0.5 * hot[qp])).any() else ts[-1]
t10 = ts[np.nonzero(after & (hot < 0.1 * hot[qp]))[0][0]] if (after & (hot < 0.1 * hot[qp])).any() else ts[-1]
if t10 <= t50:
    t10 = t50 + 0.05
print('KEEP %s: o volume quente no pico %.1f m3 aos %.2f s; afina de %.2f s (50%%) a %.2f s (10%%) pra %.3f' % (
    os.path.basename(src), hot[qp], ts[qp], t50, t10, P['keep']), flush=True)
mir = P['espelho'] > 0.5


def gen():
    _, fr = read_brm2(src)
    for q, (A, F) in enumerate(fr):
        a = A.astype(np.float32) / 255.0
        soot = 4.0 * a[..., 0] ** 2
        temp = a[..., 1]
        dust = 2.5 * a[..., 2] ** 2 * P['poeira']
        cold = 1.0 - sstep(P['frio_de'], P['frio_ate'], temp)
        soot = soot * (1.0 - (1.0 - P['keep']) * sstep(t50, t10, ts[q]) * cold)
        A2 = np.empty_like(A)
        A2[..., 0] = np.round(np.sqrt(np.clip(soot, 0, 4) / 4.0) * 255)
        A2[..., 1] = A[..., 1]
        A2[..., 2] = np.round(np.sqrt(np.clip(dust, 0, 2.5) / 2.5) * 255)
        s2 = 4.0 * (A2[..., 0] / 255.0) ** 2
        d2 = 2.5 * (A2[..., 2] / 255.0) ** 2
        ext = SOOT_EXT * np.maximum(s2 - ERODE_MEAN / (1.0 + s2 * 9.0), 0.0) + DUST_EXT * d2
        above = np.cumsum(ext[::-1], axis=0)[::-1] - 0.5 * ext
        A2[..., 3] = np.round(np.clip(np.exp(-np.minimum(above * vm, 60.0)), 0, 1) * 255)
        F2 = F.copy()
        if mir:
            A2 = A2[:, ::-1].copy()
            F2 = F2[:, ::-1].copy()
            F2[..., 2] = 255 - F2[..., 2]
        if q % 15 == 0 or q == len(ts) - 1:
            print('KEEP q%02d t %.2f s: fuligem Σ %.1f → %.1f, poeira Σ %.1f' % (q, ts[q], float((4.0 * a[..., 0] ** 2).sum() * vm ** 3),
                                                                            float(s2.sum() * vm ** 3), float(d2.sum() * vm ** 3)), flush=True)
        yield A2, F2, macro(A2, 4)


out = {k: v for k, v in head.items() if k not in ('format', 'brick', 'frameBricks', 'frameOffsets')}
if mir:
    out['bounds'] = [[b[0], -b[4], b[2], b[3], -b[1], b[5]] for b in head['bounds']]
    out['lightPos'] = [[p[0], -p[1], p[2]] for p in head['lightPos']]
out['sootKeep'] = {'from': os.path.basename(src), 'keep': P['keep'], 'thinFromS': round(float(t50), 3), 'thinToS': round(float(t10), 3),
                   'cold': [P['frio_de'], P['frio_ate']], 'dustGain': P['poeira'], 'mirrorY': mir}
counts = brm2.write(dst, out, gen())
print('KEEP pronto: %s, %d quadros, blocos por quadro até %d, %.0f MB' % (dst, len(counts), max(counts), os.path.getsize(dst) / 2 ** 20))
