"""A cortina de grãos v2 do strike_chao (23/10, tarde): a resposta à crítica do diretor à v1 (strike_gas2/view_v1.png: "a
estrutura está certa, a coroa de dedos e o anel da base, mas os dedos são tubos lisos de borracha, pétalas de flor vistas de
cima; não é areia grossa e pesada").

  python ejecta_strike2.py <pasta> [chave=valor ...]   → <pasta>/static.npz e <pasta>/snap_QQ.npz (o estado dos grãos em cada
                                                         instante da gravação) e <pasta>/prova.png (de lado e de cima)

Por que a v1 saía lisa (medido 23/10 no ej_f1 e no código): 13 raios, cada um uma nuvem gaussiana de ~6 mil parcelas com a
velocidade sorteada ±20% e espalhada por 3 pontos de rastro e um borrão que crescia com a idade: um tubo de densidade suave e
contínua, da base à ponta; e a extinção vinha quase toda dos finos (40–200 µm têm 10–50× a área por massa da areia grossa),
borrados junto. A areia de verdade é bimodal: grumos opacos de borda dura e um véu fino translúcido em volta.

O que a v2 faz (as fontes no estudo do impacto no chão, tasks/womd17wkt.output):
1. Jatos hierárquicos (Frost 2012, arXiv 1110.3090: o leito de partículas fratura em jatos de forças diferentes): 'jatos'
   primários (número primo) com o peso lognormal (poucos dominantes), a força, o ângulo (50–64°) e a largura (0,8–2,4°) de
   cada um sorteados; cada jato se divide em 1–3 sub-jatos tortos (os dedos com sub-dedos).
2. Grumos (arXiv 2111.10108: as colisões inelásticas juntam os grãos da cortina em grumos): as parcelas de cada sub-jato e de
   cada pedaço do lençol saem em grumos de ~'grumo' parcelas com a mesma velocidade e o mesmo rumo (contas num colar, com
   vazios entre eles). Enquanto o grumo está inteiro ele freia como um torrão de 5–16 mm (balístico, cai pesado); ele se
   desfaz entre 'quebra_min' e 'quebra_max' s depois de sair, e cada grão passa a frear pelo próprio tamanho: o grosso segue
   balístico e cai, o fino freia em poucos metros e fica pairando (o véu claro que se solta do dedo; DNEG em Duna: "a
   superfície do grumo se desfaz antes do miolo").
3. O lençol (a cortina contínua entre os dedos): a densidade em volta varia por harmônicos primos sorteados (nunca liso),
   o ângulo cai de ~56° no miolo a ~46° na borda (Tsujido 2014: ~50° perto do centro, ~40° perto da borda).
4. O jato escuro central (a detonação de contato: "um jato central quase vertical, de gás e areia fina escura"): parcelas do
   miolo a 72–86°, inclinadas pra frente com o mergulho, rápidas, escuras (terra úmida de baixo e a fuligem da carga).
5. O ar: a térmica da bola subindo no eixo e a entrada rente ao chão que a alimenta (o tronco de poeira), o vento do choque
   rente ao chão pra fora, a onda de base (a cortina que desaba empurra o ar pra fora rente ao chão) e a turbulência (modos
   de Fourier sem divergência, de 1,5 a 10 m): só o fino a segue (o grosso tem inércia), então o véu enrola e o dedo não.
6. Os torrões que caem levantam um puff de areia fina onde batem (os impactos secundários; realtimevfx, "debris clumps and
   dust").
7. O arrasto de Schiller e Naumann por tamanho de grão e o gás empoeirado (onde a carga de grãos passa da massa do ar, o ar
   anda junto), como na v1.
A densidade fica no desenho (strike_grid.py): o grosso e os grumos inteiros sem borrão (a borda dura, a extinção física de
cada grão: a areia do miolo do dedo satura), o fino solto borrado pela idade (o véu translúcido), a parte escura num canal à
parte (a montagem põe na fuligem).
Os instantes: os da gravação da bola clássica que vai dentro (r96_v0: 56 quadros, 6 s, curva 1,7).
Eixos: metros, o impacto na origem, Z pra cima, o míssil vindo de −X pra +X (a frente é +X).
"""
import os
import sys
import time

import numpy as np

argv = sys.argv[1:]
OUT = argv[0]
P = dict(parcelas=360000, raio=1.75, k=0.78, mu=0.41, vmax=32.0, massa=6000.0,
         jatos=23, jatos_frac=0.47, coluna_frac=0.05, frente=0.18,
         el_min=50.0, el_max=64.0, abre_min=0.8, abre_max=2.4,
         grumo=70.0, d_min=0.25e-3, d_max=3.0e-3, fino_frac=0.07, d_fino_min=40e-6, d_fino_max=160e-6,
         torrao_min=5e-3, torrao_max=16e-3, quebra_min=0.25, quebra_max=1.3,
         turb=3.0, turb_tau=2.5, turb_modos=32, subida=6.0, sub_tau=2.0, puxa=1.5, sopro=6.0, onda=4.0,
         puff=0.25, escava=0.4, semente=29, quadros=56, duracao=6.0, curva=1.7, sub=12, celula=0.6, ate=99.0,
         coluna_v0=22.0, coluna_v1=34.0, bola_r=3.0, bola_w=7.0)
for a in argv[1:]:
    if '=' in a:
        k, v = a.split('=', 1)
        P[k] = type(P[k])(float(v)) if k in P else float(v)

G = 9.81
RHO_P, RHO_A, NU = 2650.0, 1.2, 1.5e-5
rng = np.random.default_rng(int(P['semente']))
N = int(P['parcelas'])
R = P['raio']
FR = P['frente']


def area_x(n, x0, x1):
    """x/Ra uniforme na área do anel entre x0 e x1."""
    return np.sqrt(rng.uniform(x0 * x0, x1 * x1, n))


def v_law(x):
    return P['k'] * np.sqrt(G * R) * x ** (-1.0 / P['mu'])


def clumps_of(order, mean):
    """order: índices já ordenados (vizinhos = mesmo grumo) → id do grumo de cada um (tamanhos lognormais em volta de 'mean')."""
    ids = np.empty(len(order), np.int64)
    i, c = 0, 0
    while i < len(order):
        s = max(4, int(rng.lognormal(np.log(mean), 0.6)))
        ids[order[i:i + s]] = c
        i += s
        c += 1
    return ids, c


def build():
    nJ = int(N * P['jatos_frac'])
    nC = int(N * P['coluna_frac'])
    nS = N - nJ - nC
    az = np.zeros(N)
    el = np.zeros(N)
    v0 = np.zeros(N)
    x = np.zeros(N)
    kind = np.concatenate([np.zeros(nJ, np.int8), np.ones(nS, np.int8), np.full(nC, 2, np.int8)])
    clump = np.zeros(N, np.int64)
    dark = np.zeros(N, np.float32)
    ncl = 0
    cl_dv, cl_daz, cl_del, cl_dt = [], [], [], []

    # 1) os jatos: primários com peso lognormal, cada um em 1–3 sub-jatos
    nj = int(P['jatos'])
    azj = 2 * np.pi * (np.arange(nj) + rng.uniform(-0.4, 0.4, nj)) / nj
    wj = rng.lognormal(0.0, 0.7, nj) * (1.0 + FR * np.cos(azj))
    rank = np.argsort(np.argsort(-wj)) / max(nj - 1, 1)               # 0 = o mais forte
    sj = 1.12 - 0.32 * rank + rng.normal(0, 0.04, nj)
    elj = rng.uniform(P['el_min'], P['el_max'], nj) + 3.0 * (1.0 - rank) - 1.5
    sgj = np.radians(rng.uniform(P['abre_min'], P['abre_max'], nj))
    subs = []
    for j in range(nj):
        ns = 1 + min(rng.poisson(1.1), 2)
        ww = rng.dirichlet(np.full(ns, 1.5))
        for s in range(ns):
            if s == 0:
                subs.append((azj[j], elj[j], sj[j], sgj[j], wj[j] * ww[s]))
            else:
                subs.append((azj[j] + rng.choice([-1, 1]) * rng.uniform(1.6, 3.5) * sgj[j], elj[j] + rng.uniform(-5, 4),
                             sj[j] * rng.uniform(0.82, 1.06), sgj[j] * rng.uniform(0.5, 0.9), wj[j] * ww[s]))
    sw = np.array([s[4] for s in subs])
    pick = rng.choice(len(subs), nJ, p=sw / sw.sum())
    xs = area_x(nJ, 0.28, 0.80)
    o = 0
    for si, (a0, e0, s0, sg, _) in enumerate(subs):
        idx = np.nonzero(pick == si)[0]
        if not len(idx):
            continue
        idx = idx[np.argsort(xs[idx])]                                # vizinhos de velocidade = o mesmo grumo
        ids, nc = clumps_of(np.arange(len(idx)), P['grumo'])
        g_dv = 1.0 + rng.normal(0, 0.07, nc)
        g_daz = rng.normal(0, 0.75 * sg, nc)
        g_del = rng.normal(0, np.radians(1.3), nc)
        g_dt = rng.uniform(0, 0.025, nc)
        xx = xs[idx]
        az[idx] = a0 + g_daz[ids] + rng.normal(0, 0.3 * sg, len(idx))
        el[idx] = np.radians(e0) + g_del[ids] + rng.normal(0, np.radians(0.5), len(idx))
        v0[idx] = np.minimum(v_law(xx) * rng.uniform(0.97, 1.03, len(idx)), P['vmax'] * rng.uniform(0.9, 1.0, len(idx))) * s0 * g_dv[ids]
        x[idx] = xx
        clump[idx] = ncl + ids
        cl_dt.append(g_dt)
        ncl += nc
        dark[idx] = np.where(xx < 0.42, 0.25, 0.0)
        o += len(idx)

    # 2) o lençol: a densidade em volta por harmônicos primos, mais pra frente; o ângulo cai do miolo pra borda
    sidx = np.arange(nJ, nJ + nS)
    ks = np.array([3, 5, 7, 11, 13, 17, 19, 23, 29, 31])
    ak = rng.uniform(0.12, 0.38, len(ks))
    ph = rng.uniform(0, 2 * np.pi, len(ks))
    cand = rng.uniform(0, 2 * np.pi, nS * 4)
    f = 1.0 + (ak[None, :] * np.cos(ks[None, :] * cand[:, None] + ph[None, :])).sum(1)
    f = np.maximum(f, 0.12) * (1.0 + FR * np.cos(cand))
    keep = rng.uniform(0, f.max(), len(cand)) < f
    a_s = cand[keep][:nS]
    if len(a_s) < nS:
        a_s = np.concatenate([a_s, rng.uniform(0, 2 * np.pi, nS - len(a_s))])
    xs = area_x(nS, 0.32, 1.0)
    order = np.lexsort((xs, np.floor(a_s / np.radians(2.0))))
    ids, nc = clumps_of(order, 0.6 * P['grumo'])
    g_dv = 1.0 + rng.normal(0, 0.06, nc)
    g_daz = rng.normal(0, np.radians(0.5), nc)
    g_del = rng.normal(0, np.radians(1.5), nc)
    g_dt = rng.uniform(0, 0.03, nc)
    az[sidx] = a_s + g_daz[ids]
    el[sidx] = np.radians(56.0 - 10.0 * (xs - 0.32) / 0.68 + rng.normal(0, 2.0, nS)) + g_del[ids]
    v0[sidx] = np.minimum(v_law(xs) * rng.uniform(0.95, 1.05, nS), P['vmax'] * rng.uniform(0.85, 1.0, nS)) * g_dv[ids] * (1.0 + FR * np.cos(a_s))
    x[sidx] = xs
    clump[sidx] = ncl + ids
    cl_dt.append(g_dt)
    ncl += nc

    # 3) o jato escuro central: o miolo, quase na vertical, inclinado pra frente
    cidx = np.arange(nJ + nS, N)
    xs = area_x(nC, 0.05, 0.28)
    order = np.argsort(rng.uniform(0, 1, nC))
    ids, nc = clumps_of(order, 0.6 * P['grumo'])
    g_dv = 1.0 + rng.normal(0, 0.10, nc)
    g_daz = rng.uniform(0, 2 * np.pi, nc)
    g_el = np.radians(rng.uniform(70.0, 84.0, nc))
    g_dt = rng.uniform(0, 0.04, nc)
    az[cidx] = g_daz[ids]
    el[cidx] = g_el[ids] + rng.normal(0, np.radians(1.0), nC)
    v0[cidx] = rng.uniform(P['coluna_v0'], P['coluna_v1'], nC) * g_dv[ids]
    x[cidx] = xs
    clump[cidx] = ncl + ids
    cl_dt.append(g_dt)
    ncl += nc
    dark[cidx] = 0.35

    # a direção (com a inclinação pra frente do jato central: ~14°)
    vel = np.stack([v0 * np.cos(el) * np.cos(az), v0 * np.cos(el) * np.sin(az), v0 * np.sin(el)], 1)
    vel[cidx, 0] += 0.25 * v0[cidx] * np.sin(el[cidx])
    r0 = R * x
    pos = np.stack([r0 * np.cos(az), r0 * np.sin(az), np.full(N, 0.15)], 1)
    pos[cidx, 2] = 0.4
    # os grãos: grosso log-uniforme, a fração fina pela massa (o jato central mais fino)
    d = np.exp(rng.uniform(np.log(P['d_min']), np.log(P['d_max']), N))
    ff = np.where(kind == 2, 0.2, P['fino_frac'])
    fine = rng.uniform(0, 1, N) < ff
    d = np.where(fine, np.exp(rng.uniform(np.log(P['d_fino_min']), np.log(P['d_fino_max']), N)), d)
    # os grumos: o arrasto do torrão inteiro e a hora em que ele se desfaz
    c_deff = np.exp(rng.uniform(np.log(P['torrao_min']), np.log(P['torrao_max']), ncl))
    c_brk = rng.uniform(P['quebra_min'], P['quebra_max'], ncl)
    c_dt = np.concatenate(cl_dt)
    is_col = np.zeros(ncl, bool)
    is_col[np.unique(clump[cidx])] = True
    c_brk = np.where(is_col, rng.uniform(0.15, 0.6, ncl), c_brk)
    t0 = P['escava'] * x ** 2 + c_dt[clump] + rng.uniform(0, 0.008, N)
    t0 = np.where(kind == 2, rng.uniform(0.0, 0.03, N), t0)
    tb = t0 + c_brk[clump]
    deff = c_deff[clump]
    print(f'GRAOS {N} parcelas: {nJ} nos jatos ({nj} primários, {len(subs)} sub-jatos), {nS} no lençol, {nC} no jato central, '
          f'{ncl} grumos; v0 p50 {np.median(v0):.1f} p95 {np.percentile(v0, 95):.1f} m/s', flush=True)
    return pos, vel, d, deff, t0, tb, kind, dark


def modes(nm):
    """Os modos da turbulência: k aleatório (λ de 1,5 a 10 m), a direção ⟂ k (sem divergência), a amplitude ∝ λ^(1/3)."""
    lam = np.exp(rng.uniform(np.log(1.5), np.log(10.0), nm))
    kd = rng.normal(size=(nm, 3))
    kd /= np.linalg.norm(kd, axis=1, keepdims=True)
    kv = kd * (2 * np.pi / lam)[:, None]
    e = np.cross(kd, rng.normal(size=(nm, 3)))
    e /= np.linalg.norm(e, axis=1, keepdims=True)
    a = lam ** (1.0 / 3.0)
    a *= np.sqrt(2.0 / (a * a).sum())                                    # RMS 1 por componente ~
    om = 2 * np.pi / (lam / 1.5)                                        # o giro do redemoinho: λ/(1,5 m/s)
    return kv, e * a[:, None], om, rng.uniform(0, 2 * np.pi, nm)


def main():
    os.makedirs(OUT, exist_ok=True)
    t_start = time.time()
    pos, vel, d, deff, t0, tb, kind, dark = build()
    mass = np.full(N, P['massa'] / N, np.float32)
    kv, ea, om, phs = modes(int(P['turb_modos']))
    alive = np.ones(N, bool)
    puffed = np.zeros(N, bool)
    nq = int(P['quadros'])
    times = np.array([P['duracao'] * (q / (nq - 1)) ** P['curva'] for q in range(nq)])
    np.savez(os.path.join(OUT, 'static.npz'), d0=d, t0=t0, tb=tb, kind=kind, dark=dark, times=times, params=str(P))
    fps = 30.0
    dt = 1.0 / fps / int(P['sub'])
    CELL = P['celula']
    half = 30.0
    cn = np.array([int(2 * half / CELL), int(2 * half / CELL), int(45.0 / CELL)])
    m_par = P['massa'] / N

    def air(p, t, turb_on):
        xx, yy, zz = p[:, 0], p[:, 1], p[:, 2]
        rr = np.hypot(xx, yy) + 1e-6
        ex, ey = xx / rr, yy / rr
        # a térmica da bola no eixo (a clássica a 0,55: o topo a ~5 m aos 0,3 s, ~11 m a 1,3 s, ~16 m a 2,1 s)
        rb = P['bola_r'] * (1.0 + 0.4 * t)
        hb = P['bola_r'] + P['bola_w'] * min(t, 2.5)
        w = P['subida'] * np.exp(-t / P['sub_tau']) * np.exp(-(rr / rb) ** 2) * (1.0 / (1.0 + np.exp((zz - hb) / 2.0)))
        # a entrada rente ao chão que alimenta a térmica (o tronco)
        s_in = np.clip((t - 0.3) / 0.7, 0, 1) * np.exp(-t / 4.0)
        ur = -P['puxa'] * s_in * np.exp(-zz / 1.5) * (rr / (rr + 3.0)) * np.exp(-(rr / 9.0) ** 2)
        # o vento do choque, rente ao chão pra fora
        ur += P['sopro'] * np.exp(-t / 0.6) * np.exp(-zz / 1.2) * np.clip(rr / 4.0, 0, 1) * np.exp(-(rr / 14.0) ** 2)
        # a onda de base: a cortina desabando empurra o ar pra fora rente ao chão
        so = np.clip((t - 0.8) / 1.2, 0, 1) * np.exp(-max(t - 2.0, 0.0) / 3.0)
        ur += P['onda'] * so * np.exp(-zz / 2.5) * np.clip((rr - 3.0) / 3.0, 0, 1) * np.exp(-(rr / 22.0) ** 2)
        u = np.stack([ur * ex, ur * ey, w], 1)
        if turb_on is not None and turb_on.any():
            q = p[turb_on]
            amp = P['turb'] * (0.35 + 0.65 * np.exp(-t / P['turb_tau'])) * np.exp(-(rr[turb_on] / 18.0) ** 2)
            ph = q @ kv.T - om[None, :] * t + phs[None, :]
            u[turb_on] += (np.cos(ph) @ ea) * amp[:, None]
        return u

    def coupled(p, v, ub):
        ci = np.floor((p + [half, half, 0.0]) / CELL).astype(np.int64)
        ok = np.all((ci >= 0) & (ci < cn), axis=1)
        flat = np.where(ok, np.ravel_multi_index(tuple(np.clip(ci, 0, cn - 1).T), cn), 0)
        nn = int(np.prod(cn))
        M = np.bincount(flat[ok], minlength=nn) * m_par
        MV = np.stack([np.bincount(flat[ok], weights=v[ok, k], minlength=nn) * m_par for k in range(3)], 1)
        L = M[flat] / CELL ** 3
        vbar = MV[flat] / np.maximum(M[flat], 1e-9)[:, None]
        c = np.where(ok, L / (L + RHO_A), 0.0)[:, None]
        return ub + c * (vbar - ub)

    t = 0.0
    q = 0
    gvec = np.array([0.0, 0.0, -G])
    nsteps = int(round(min(P['duracao'], P['ate']) / dt)) + 2
    for step in range(nsteps):
        while q < nq and times[q] <= t + 0.5 * dt:
            on = alive & (t >= t0)
            idx = np.nonzero(on)[0].astype(np.int32)
            np.savez(os.path.join(OUT, f'snap_{q:02d}.npz'), idx=idx, pos=pos[idx].astype(np.float32), vel=vel[idx].astype(np.float16),
                     d=d[idx].astype(np.float32), m=mass[idx], t=np.float32(t))
            if q % 8 == 0 or q == nq - 1:
                zt = pos[idx, 2]
                rr = np.hypot(pos[idx, 0], pos[idx, 1])
                print(f'GRAOS q{q:02d} t {t:.2f} s: no ar {len(idx)}, topo p99 {np.percentile(zt, 99) if len(idx) else 0:.1f} m, '
                      f'R p90 {np.percentile(rr, 90) if len(idx) else 0:.1f} m, inteiros {(t < tb[idx]).mean() if len(idx) else 0:.2f}, '
                      f'{time.time() - t_start:.0f} s', flush=True)
            q += 1
        if q >= nq or t > P['ate']:
            break
        go = alive & (t >= t0)
        if go.any():
            gi = np.nonzero(go)[0]
            p, v = pos[gi], vel[gi]
            intact = t < tb[gi]
            du = np.where(intact, deff[gi], d[gi])
            turb_on = (~intact) & (d[gi] < 0.5e-3)
            u = coupled(p, v, air(p, t, turb_on))
            vr = v - u
            sp = np.linalg.norm(vr, axis=1) + 1e-9
            Re = sp * du / NU
            Cd = np.where(Re < 1000.0, 24.0 / np.maximum(Re, 1e-6) * (1 + 0.15 * Re ** 0.687), 0.44)
            kk = 0.75 * (RHO_A / RHO_P) * Cd * sp / du
            v = (v + dt * (gvec + kk[:, None] * u)) / (1.0 + dt * kk[:, None])
            p = p + v * dt
            # o chão: o fino fica rente (rola com o ar), o grosso pousa; os torrões rápidos levantam um puff
            low = p[:, 2] <= 0.05
            if low.any():
                fine_g = d[gi] < 0.2e-3
                rest = low & fine_g
                p[rest, 2] = 0.05
                v[rest, 2] = np.maximum(v[rest, 2], 0.0)
                land = low & ~fine_g
                if land.any():
                    li = np.nonzero(land)[0]
                    fast = np.linalg.norm(v[li], axis=1) > 5.0
                    pf = fast & (~puffed[gi[li]]) & (rng.uniform(0, 1, len(li)) < P['puff'])
                    pu = li[pf]
                    if len(pu):
                        hv = v[pu, :2]
                        hn = np.linalg.norm(hv, axis=1, keepdims=True) + 1e-6
                        sp2 = rng.uniform(2.0, 4.5, len(pu))[:, None]
                        th = rng.uniform(-0.9, 0.9, len(pu))
                        c, s = np.cos(th)[:, None], np.sin(th)[:, None]
                        hdir = hv / hn
                        hdir = np.concatenate([c * hdir[:, :1] - s * hdir[:, 1:], s * hdir[:, :1] + c * hdir[:, 1:]], 1)
                        v[pu, :2] = hdir * sp2
                        v[pu, 2] = rng.uniform(1.0, 3.0, len(pu))
                        p[pu, 2] = 0.1
                        gp = gi[pu]
                        d[gp] = np.exp(rng.uniform(np.log(50e-6), np.log(130e-6), len(pu)))
                        mass[gp] *= 0.25
                        tb[gp] = t
                        puffed[gp] = True
                    dead = li[~pf]
                    alive[gi[dead]] = False
            pos[gi], vel[gi] = p, v
            # a borda: fora da caixa de trabalho some
            far = (np.abs(pos[gi, 0]) > half) | (np.abs(pos[gi, 1]) > half) | (pos[gi, 2] > 44.0)
            alive[gi[far]] = False
        t += dt
    print(f'GRAOS pronto em {time.time() - t_start:.0f} s, {q} instantes, {P}', flush=True)


if __name__ == '__main__':
    main()
