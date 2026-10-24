"""Um quadro do strike_chao só de areia (23/10 noite): os grãos (sandgrid) + a areia em gás do Mantaflow, sem a bola, na grade e
nos eixos da gravação do jogo (o y da Unreal), com o recorte suave da caixa e o corte dos blocos quase vazios.

Os parâmetros (P):
- g_coarse, g_fine, blur_c, pot, c_ref: a mistura dos grãos (sandgrid.mix);
- mf_fator, mf_gama: a areia em gás (a onda de base, o véu do choque, o tronco) = fator·den^gama;
- pe, pe_h, pe_r: a massa no pé: × (1 + pe·e^(−z/pe_h)) na areia em gás, só até pe_r m do centro (suave): é a onda de base de
  verdade (a cortina que desaba vira uma corrente de gravidade rente ao chão, carga de 0,05–0,5 kg/m³; Glasstone §6.70);
- escura: × a parte escura (a terra úmida de baixo e o jato central), no canal da fuligem;
- limiar, bloco_min: o vazio (o voxel e o bloco de 8³ quase vazio saem inteiros).
"""
import os

import numpy as np

import sandgrid as sg

SOOT_EXT, DUST_EXT, ERODE_MEAN, VEL_MAX = 13.5, 5.5, 0.042 * 0.5, 40.0
DEFAULT = dict(dominio=40.0, res=128, g_coarse=0.33, g_fine=0.035, blur_c=0.0, pot=1.5, c_ref=0.6, mf_fator=1.0, mf_gama=1.1,
               pe=0.0, pe_h=2.0, pe_r=14.0, escura=1.0, limiar=0.012, bloco_min=0.3, raio_de=16.0, raio_ate=19.2, topo_de=33.0,
               topo_ate=38.0, alto_z0=0.0)


class Field:
    def __init__(self, gdir, mfdir, cache, **kw):
        self.P = dict(DEFAULT)
        self.P.update(kw)
        self.gdir, self.mfdir, self.cache = gdir, mfdir, cache
        self.st = sg.load_static(gdir)
        self.times = self.st['times']
        n = int(self.P['res'])
        D = self.P['dominio']
        self.n, self.D, self.vm = n, D, D / n
        c = (np.arange(n) + 0.5) * self.vm - D / 2
        r = np.sqrt(c[None, :] ** 2 + c[:, None] ** 2)
        k = np.clip((r - self.P['raio_de']) / (self.P['raio_ate'] - self.P['raio_de']), 0.0, 1.0)
        zk = np.clip(((np.arange(n) + 0.5) * self.vm - self.P['topo_de']) / (self.P['topo_ate'] - self.P['topo_de']), 0.0, 1.0)
        self.crop = ((1.0 - k * k * (3.0 - 2.0 * k))[None, :, :] * (1.0 - zk * zk * (3.0 - 2.0 * zk))[:, None, None]).astype(np.float32)
        z = (np.arange(n) + 0.5) * self.vm
        kr = np.clip((r - self.P['pe_r']) / 4.0, 0.0, 1.0)
        self.foot = (1.0 + self.P['pe'] * np.exp(-z / self.P['pe_h'])[:, None, None] * (1.0 - kr * kr * (3 - 2 * kr))[None]).astype(np.float32)

    @staticmethod
    def to_unreal(a, vec=False):
        a = a[:, ::-1].copy()
        if vec:
            a[..., 1] *= -1.0
        return a

    def frame(self, q):
        """→ soot (a parte escura, unidades da fuligem), dust, vel (meia resolução, m/s), sand_g, dust_mf; eixos da Unreal."""
        P, n, vm = self.P, self.n, self.vm
        nf = n // 2
        c = sg.components(self.gdir, self.st, q, n, self.D, cache=self.cache)
        sand_g, dark_g, Wg, MVg = sg.mix(c, vm, P['g_coarse'], P['g_fine'], P['blur_c'], P['pot'], P['c_ref'])
        if P['alto_z0'] > 0:
            # (a v4d, depois da prova no jogo) a cortina alta rala: a folha do cone se abre subindo (a densidade ∝ 1/z² longe do pé), e o
            # grumo que sobe inteiro deixava fios densos até 25–30 m que, na câmera de RTS, escondiam as bolas de fogo do ataque (o
            # aprovado). Só os grãos; o pé e a onda de base ficam
            att = (1.0 / (1.0 + ((np.arange(n) + 0.5) * vm / P['alto_z0']) ** 2)).astype(np.float32)[:, None, None]
            sand_g = sand_g * att
            dark_g = dark_g * att
        if self.mfdir and self.mfdir != '-':
            mz = np.load(os.path.join(self.mfdir, f'mf_{q:02d}.npz'))
            den = np.maximum(mz['den'].astype(np.float32), 0.0)
            dust_mf = np.clip(P['mf_fator'] * den ** P['mf_gama'], 0.0, 2.5) * self.foot
            vel_mf = mz['vel'].astype(np.float32)
        else:
            dust_mf = np.zeros((n, n, n), np.float32)
            vel_mf = np.zeros((nf, nf, nf, 3), np.float32)
        # o Mantaflow e os grãos estão nos eixos da simulação: vira os dois juntos
        dust_mf = self.to_unreal(dust_mf)
        vel_mf = self.to_unreal(vel_mf, True)
        sand_g, dark_g = self.to_unreal(sand_g), self.to_unreal(dark_g)
        Wg, MVg = self.to_unreal(Wg), self.to_unreal(MVg, True)
        soot = np.clip(dark_g * (DUST_EXT / SOOT_EXT) * P['escura'] * self.crop, 0.0, 4.0)
        dust = np.clip((dust_mf + sand_g) * self.crop, 0.0, 2.5)
        empty = (soot < P['limiar']) & (dust < P['limiar'])
        ext_v = SOOT_EXT * soot + DUST_EXT * dust
        bsum = ext_v.reshape(n // 8, 8, n // 8, 8, n // 8, 8).sum(axis=(1, 3, 5)) * vm ** 3
        drop = bsum < P['bloco_min']
        empty |= np.repeat(np.repeat(np.repeat(drop, 8, 0), 8, 1), 8, 2)
        soot[empty] = 0.0
        dust[empty] = 0.0
        ext_mfh = DUST_EXT * dust_mf.reshape(nf, 2, nf, 2, nf, 2).mean(axis=(1, 3, 5))
        wsum = ext_mfh + DUST_EXT * Wg
        vsum = ext_mfh[..., None] * vel_mf + DUST_EXT * MVg
        vel = np.where(wsum[..., None] > 1e-5, vsum / np.maximum(wsum, 1e-5)[..., None], 0.0).astype(np.float32)
        return soot.astype(np.float32), dust.astype(np.float32), vel, float(c['t'])

    def encode(self, soot, dust, vel):
        """→ A (n³ RGBA8: √fuligem/4, T, √poeira/2,5, o céu de cima) e F (meia resolução: combustível 0, a velocidade)."""
        n, vm = self.n, self.vm
        ext = SOOT_EXT * np.maximum(soot - ERODE_MEAN / (1.0 + soot * 9.0), 0.0) + DUST_EXT * dust
        above = np.cumsum(ext[::-1], axis=0)[::-1] - 0.5 * ext
        sky = np.exp(-np.minimum(above * vm, 60.0))
        A = np.zeros((n, n, n, 4), np.uint8)
        A[..., 0] = np.round(np.sqrt(soot / 4.0) * 255)
        A[..., 2] = np.round(np.sqrt(dust / 2.5) * 255)
        A[..., 3] = np.round(np.clip(sky, 0.0, 1.0) * 255)
        nf = n // 2
        F = np.full((nf, nf, nf, 4), 128, np.uint8)
        F[..., 0] = 0
        F[..., 1:] = np.round(np.clip(vel / VEL_MAX * 0.5 + 0.5, 0.0, 1.0) * 255)
        return A, F
