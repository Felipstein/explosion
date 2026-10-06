"""Quanto de espalhamento múltiplo o céu do WebGL tem a mais, hora a hora.

O céu do WebGL (reference/webgl/js/atmosphere.js) é o modelo de Hillaire
(EGSR 2020), o mesmo da Sky Atmosphere da Unreal, mas a LUT de espalhamento
múltiplo dele tem um desvio: nos raios que descem, raySphere() devolve a
interseção de SAÍDA da Terra, então o raio atravessa o planeta com ar ao nível
do mar (a densidade é lida com max(h, 0)) e volta cheio de luz espalhada. O
termo Ψms sai ~2× maior de dia e ~1.1–1.4× no crepúsculo. O céu azul-claro que
foi aprovado é esse.

A Unreal tem o fator pra isso (USkyAtmosphereComponent::MultiScatteringFactor,
"Art Direction"): multiplica o Ψms dela. Aqui se calcula, pra cada hora da
tabela do WebGL (webgl_time_of_day.json), a razão entre o Ψms do WebGL (com o
desvio reproduzido) e o de Hillaire correto, somando sol e lua (a lua do
render: 2.2e-3 do sol) e pesando as alturas pela densidade do ar — a Unreal
tem um fator só pras duas luzes.

Conferido contra a LUT do próprio WebGL lida na GPU: Ψms no chão com o sol a
25° = (0.0872, 0.1001, 0.1321) aqui e (0.0872, 0.1001, 0.1321) lá.

Uso: python Tools/sky_ms_table.py  → Tools/webgl_sky_ms.json (~1 min)
"""
import json
import os

import numpy as np

TOOLS = os.path.dirname(os.path.abspath(__file__))

# atmosphere.js MEDIUM (km, 1/km)
Rg, Rt = 6360.0, 6460.0
BETA_R = np.array([5.802, 13.558, 33.1]) * 1e-3
HR, HM = 8.0, 1.2
BETA_M_S, BETA_M_E = 3.996e-3, 4.4e-3
BETA_O = np.array([0.650, 1.881, 0.085]) * 1e-3
GROUND_ALBEDO = np.array([0.12, 0.11, 0.10])
MOON = 2.2e-3                               # celestial.js MOON_ILLUMINANCE (lua do render)
LUMA = np.array([0.2126, 0.7152, 0.0722])


def medium(h):
    h = np.maximum(h, 0.0)[..., None]
    dR, dM = np.exp(-h / HR), np.exp(-h / HM)
    dO = np.maximum(0.0, 1.0 - np.abs(h - 25.0) / 15.0)
    sR = BETA_R * dR
    sM = (BETA_M_S * dM)[..., 0]
    return sR, sM, sR + BETA_M_E * dM + BETA_O * dO


def ray_sphere_far(r, mu, R):
    """A raiz de saída (a única que o raySphere do WebGL devolve)."""
    b = r * r * (mu * mu - 1.0) + R * R
    return np.where(b < 0, -1.0, np.maximum(-r * mu + np.sqrt(np.maximum(b, 0.0)), 0.0))


def ray_sphere_near(r, mu, R):
    b = r * r * (mu * mu - 1.0) + R * R
    return np.where(b < 0, -1.0, np.maximum(-r * mu - np.sqrt(np.maximum(b, 0.0)), 0.0))


def hits_ground(r, mu):
    return (mu < 0) & (r * r * (mu * mu - 1.0) + Rg * Rg >= 0)


def transmittance(r, mu, n=48):
    r, mu = np.broadcast_arrays(np.asarray(r, float), np.asarray(mu, float))
    rf, mf = r.ravel(), mu.ravel()
    d = ray_sphere_far(rf, mf, Rt)
    t = (np.arange(n) + 0.5)[:, None] / n * d[None, :]
    rr = np.sqrt(np.maximum(t * t + 2 * rf * mf * t + rf * rf, 0.0))
    _, _, ext = medium(rr - Rg)
    tau = (ext * (d / n)[None, :, None]).sum(0)
    T = np.exp(-tau).reshape(r.shape + (3,))
    return np.where(hits_ground(r, mu)[..., None], 0.0, T)


def psi_ms(r, muS, webgl, dirs=48, steps=18):
    """Ψms de Hillaire (2020, seção 4) no ponto (r, muS), iluminância 1.
    webgl=True reproduz atmosphere.js: o raio que desce atravessa a Terra."""
    sun = np.array([np.sqrt(max(1 - muS * muS, 0.0)), muS, 0.0])
    o = np.array([0.0, r, 0.0])
    i = np.arange(dirs) + 0.5
    ct = 1 - 2 * i / dirs
    st = np.sqrt(np.maximum(1 - ct * ct, 0.0))
    D = np.stack([np.cos(i * 2.39996323) * st, ct, np.sin(i * 2.39996323) * st], -1)
    mu = D[:, 1]
    g = hits_ground(r, mu)
    to_ground = ray_sphere_far(r, mu, Rg) if webgl else ray_sphere_near(r, mu, Rg)
    tmax = np.where(g, to_ground, ray_sphere_far(r, mu, Rt))
    dt = tmax / steps
    L = np.zeros((dirs, 3)); fms = np.zeros((dirs, 3)); tp = np.ones((dirs, 3))
    for s in range(steps):
        p = o + D * ((s + 0.5) * dt)[:, None]
        rr = np.linalg.norm(p, axis=1)
        sR, sM, ext = medium(rr - Rg)
        sc = sR + sM[:, None]
        ext = np.maximum(ext, 1e-9)
        stepT = np.exp(-ext * dt[:, None])
        muSl = (p @ sun) / rr
        # dentro da Terra (só no WebGL) o sol "chega" se estiver acima do horizonte local
        sunT = transmittance(np.maximum(rr, Rg), muSl)
        sunT = np.where(((rr < Rg) & (muSl < 0))[:, None], 0.0, sunT)
        fms += tp * (sc - sc * stepT) / ext
        S = sunT * sc / (4 * np.pi)
        L += tp * (S - S * stepT) / ext
        tp *= stepT
    pg = o + D * tmax[:, None]
    muSg = (pg @ sun) / np.linalg.norm(pg, axis=1)
    Tg = transmittance(np.full(dirs, Rg), muSg)
    L += np.where((g & (muSg > 0))[:, None], tp * GROUND_ALBEDO * muSg[:, None] * Tg / np.pi, 0.0)
    return L.mean(0) / np.maximum(1 - fms.mean(0), 1e-4)


def main():
    with open(os.path.join(TOOLS, 'webgl_time_of_day.json'), encoding='utf-8') as f:
        rows = json.load(f)['rows']                    # hours, sunAlt, moonAlt, ...
    heights = np.array([0.1, 1.0, 2.5, 5.0, 8.0, 12.0])
    weights = np.exp(-heights / HR) * np.gradient(heights)     # ar espalhando em cada camada
    grid = np.linspace(-1.0, 1.0, 81)                          # cos do zênite do astro
    lum = {}
    for webgl in (False, True):
        lum[webgl] = np.array([[psi_ms(Rg + h, m, webgl) @ LUMA for m in grid] for h in heights])

    def column(alt_deg, webgl):
        m = np.sin(np.radians(alt_deg))
        return np.array([np.interp(m, grid, lum[webgl][k]) for k in range(len(heights))]) @ weights

    out = []
    for r in rows:
        hours, sun_alt, moon_alt = r[0], r[1], r[2]
        ref = column(sun_alt, False) + MOON * column(moon_alt, False)
        web = column(sun_alt, True) + MOON * column(moon_alt, True)
        out.append([hours, round(float(web / max(ref, 1e-30)), 4)])
    with open(os.path.join(TOOLS, 'webgl_sky_ms.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump({'source': 'Tools/sky_ms_table.py: Ψms do WebGL (atmosphere.js, com a Terra atravessada) / Ψms de '
                             'Hillaire 2020, sol + lua, alturas pesadas pela densidade do ar',
                   'columns': ['hours', 'multiScatteringFactor'], 'rows': out}, f, indent=0)
    for h in (5.35, 8.5, 12.6, 15.0, 17.45, 18.0, 18.55, 19.0, 20.0, 22.5, 0.0):
        k = min(out, key=lambda o: abs(o[0] - h))
        print('%5.2fh  fator %.2f' % (k[0], k[1]))


if __name__ == '__main__':
    main()
