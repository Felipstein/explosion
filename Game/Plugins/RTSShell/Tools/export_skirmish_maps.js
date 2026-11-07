// Tira do mockup aprovado do skirmish (rodado no Chrome pelo export_skirmish_maps.mjs) o que é imagem pronta de cada mapa de verdade:
// o losango da sala (a sombra, o chão desenhado pelo renderMinimap, a vinheta e as duas molduras; os pontos cardeais ficam pro jogo) e
// a miniatura do navegador, os dois em 2× pra sair nítido em qualquer tela; e a ficha do mapa (as bases, as docas do meio, o tipo).
// Define window.__exportOne(i, porta) (um mapa por chamada: as imagens por POST pro receptor, volta a ficha e o tempo de cada passo) e devolve
// quantos mapas há. Mapa novo = rodar de novo.
(() => {
  window.__exportList = CAT.filter(c => c.real);
  window.__exportOne = async (i, sink) => {
    const e = window.__exportList[i], ms = {}, t = () => performance.now();
    let t0 = t();
    const M = await getMap(e); ms.map = t() - t0; t0 = t();
    const S = 2, D = DIA * S, side = HALF * Math.SQRT2 * S;
    const flat = renderMinimap(M, 1024); ms.render = t() - t0; t0 = t();   // a mesma do mockup
    const c = document.createElement('canvas'); c.width = c.height = D;
    const g = c.getContext('2d');
    g.translate(D / 2, D / 2); g.rotate(Math.PI * 1.25);
    g.shadowColor = 'rgba(0,0,0,.7)'; g.shadowBlur = 40 * S; g.fillStyle = '#000'; g.fillRect(-side / 2, -side / 2, side, side); g.shadowBlur = 0;
    g.imageSmoothingQuality = 'high'; g.drawImage(flat, -side / 2, -side / 2, side, side);
    const vg = g.createRadialGradient(0, 0, side * .3, 0, 0, side * .75); vg.addColorStop(0, 'rgba(0,0,0,0)'); vg.addColorStop(1, 'rgba(0,0,0,.45)');
    g.fillStyle = vg; g.fillRect(-side / 2, -side / 2, side, side);
    g.strokeStyle = 'rgba(230,220,190,.35)'; g.lineWidth = 1.5 * S; g.strokeRect(-side / 2, -side / 2, side, side);
    g.strokeStyle = 'rgba(230,220,190,.12)'; g.lineWidth = 1 * S; g.strokeRect(-side / 2 - 8 * S, -side / 2 - 8 * S, side + 16 * S, side + 16 * S);
    const diamond = c.toDataURL('image/png'); ms.bake = t() - t0; t0 = t();
    // a miniatura (o thumbOf em 2×: 400 px, o chão em 320, as bases em ponto branco)
    const tc = document.createElement('canvas'); tc.width = tc.height = 400;
    const h = tc.getContext('2d'), fl2 = renderMinimap(M, 320), s2 = 400 / Math.SQRT2;
    h.translate(200, 200); h.rotate(Math.PI * 1.25); h.drawImage(fl2, -s2 / 2, -s2 / 2, s2, s2);
    const sr = (M.starts.length > 8 ? 4.5 : 6.5) * 2;
    for (const [x, y] of M.starts) {
      const u = (x + 512) / 1024 * s2 - s2 / 2, v = (y + 512) / 1024 * s2 - s2 / 2;
      h.fillStyle = 'rgba(0,0,0,.55)'; h.beginPath(); h.arc(u - 2, v - 2, sr + 4.4, 0, 7); h.fill();
      h.fillStyle = '#fff'; h.beginPath(); h.arc(u, v, sr, 0, 7); h.fill(); h.strokeStyle = 'rgba(10,12,11,.9)'; h.lineWidth = 4; h.stroke();
    }
    const thumb = tc.toDataURL('image/png'); ms.thumb = t() - t0;
    // as imagens vão por POST pro receptor do export_skirmish_maps.mjs; volta só o pequeno (os tempos e a ficha)
    const post = async (name, url) => fetch(`http://127.0.0.1:${sink}/${name}`, { method: 'POST', mode: 'no-cors', body: await (await fetch(url)).blob() });
    await post(`${e.real}_diamond.png`, diamond);
    await post(`${e.real}_thumb.png`, thumb);
    const meta = { real: e.real, name: e.name, players: e.players, size: e.size, terr: e.terr, cond: e.cond, src: e.src, plays: e.plays, date: e.date,
      naval: !!e.naval, starts: M.starts, docks: M.docks.filter(([x, y]) => Math.hypot(x, y) < 260) };
    return JSON.stringify({ ms, meta });
  };
  return window.__exportList.length;
})()
