#!/usr/bin/env python3
"""
purkinje.py — gera as matrizes do Purkinje shift usadas em js/post.js.

Segue Patry, "Real-Time Samurai Cinema" (SIGGRAPH 2021 Advances, slides
164–168), que por sua vez segue Cao et al. 2008 ("Rod contributions to color
perception: linear with rod contrast") e Kirk & O'Brien 2011:

  M_ij = ∫ E_i(λ) I(λ) R_j(λ) dλ      i ∈ {L,M,S,R}, j ∈ {r,g,b}
  q    = M c
  g_i  = [1 + 0.33/m_i (q_i + k_i q_R)]^-1/2
  Δo   = K/S · B · diag(k) diag(m)^-1 · g · q_R
  Δc   = M̂^-1 A^-1 Δo

Dados (baixados na primeira execução):
  E_L,M,S  Smith & Pokorny 1975 (CVRL sp.csv, log10, energia)
  E_R      CIE 1951 V'(λ) escotópica (CVRL scvle_1.csv)
  I        CIE D65 (CVRL Illuminantd65.csv)
  R_rgb    espectros de reflectância de Smits 2000, tabelas do pbrt-v3

As curvas entram normalizadas no pico (é o que o slide chama de "receptor
response curve"); com elas a direção do shift no escuro sai
[-0.03, 0.26, 0.97], a mesma medida nas capturas do slide 172/173
([0.01, 0.16, 0.99]). A escala absoluta (PURK_S, em js/post.js) foi ajustada
pra reproduzir os ganhos do slide 173 a 0.05 lux.

Uso:  python3 tools/purkinje.py   (precisa de numpy)
"""
import os, re, urllib.request
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, '.purkinje-data')
SRC = {
    'sp.csv': 'http://www.cvrl.org/database/data/cones/sp.csv',
    'scvle_1.csv': 'http://www.cvrl.org/database/data/lum/scvle_1.csv',
    'd65.csv': 'http://www.cvrl.org/database/data/cie/Illuminantd65.csv',
    'spectrum.cpp': 'https://raw.githubusercontent.com/mmp/pbrt-v3/master/src/core/spectrum.cpp',
}

def fetch(name):
    os.makedirs(CACHE, exist_ok=True)
    p = os.path.join(CACHE, name)
    if not os.path.exists(p):
        urllib.request.urlretrieve(SRC[name], p)
    return open(p).read()

def csv(name):
    rows = [l.split(',') for l in fetch(name).splitlines() if l.strip() and l.strip()[0].isdigit()]
    return np.array([[float(v) for v in r] for r in rows])

def pbrt(name):
    src = fetch('spectrum.cpp')
    m = re.search(r'const Float ' + name + r'\[nRGB2SpectSamples\] = \{([^}]*)\}', src)
    return np.array([float(x) for x in m.group(1).replace('\n', ' ').split(',') if x.strip()])

lam = np.arange(380, 781, 1.0)
sp = csv('sp.csv')
cone = [np.interp(lam, sp[:, 0], 10 ** sp[:, i], left=0, right=0) for i in (1, 2, 3)]
rod = np.interp(lam, *csv('scvle_1.csv').T, left=0, right=0)
d65 = np.interp(lam, *csv('d65.csv').T)
lamS = pbrt('RGB2SpectLambda')
refl = [np.interp(lam, lamS, pbrt('RGBRefl2Spect' + n)) for n in ('Red', 'Green', 'Blue')]

E = [c / c.max() for c in cone] + [rod / rod.max()]
M = np.array([[np.trapezoid(e * d65 * r, lam) for r in refl] for e in E])
# escala: c = (1,1,1) tem luminância 1 (L+M em trolands = V(λ), Smith-Pokorny)
M /= (M[0] * 0.63721 + M[1] * 0.39242).sum()

m = np.array([0.63721, 0.39242, 1.6064])
k = np.array([0.2, 0.2, 0.29])
K, S, k3, rw, p = 45.0, 10.0, 0.6, 0.139, 0.6189
A = np.array([[-1, 1, 0], [-1, -1, 1], [1, 1, 0]], float)
B = np.array([[-(k3 + rw), 1 + k3 * rw, 0], [p * k3, (1 - p) * k3, 1], [p * S, (1 - p) * S, 0]])
Mh = M[:3]
G = (0.33 / m)[:, None] * (Mh + k[:, None] * M[3][None, :])   # q do ganho, por unidade de c
D = np.linalg.inv(Mh) @ np.linalg.inv(A) @ ((K / S) * B) @ np.diag(k / m)

def glsl_mat3(X):   # GLSL é column-major
    return 'mat3(' + ', '.join(f'{v:.6g}' for v in X.T.flatten()) + ')'
print('const mat3 PURK_G = ' + glsl_mat3(G) + ';')
print('const vec3 PURK_R = vec3(' + ', '.join(f'{v:.6g}' for v in M[3]) + ');')
print('const mat3 PURK_D = ' + glsl_mat3(D) + ';')
d = D @ np.ones(3)
print('// direção no escuro:', d / np.linalg.norm(d))
