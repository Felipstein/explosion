// Tira do multiplayer.html aprovado (docs/concept/ui/menu, rodado no Chrome pelo export_multiplayer_maps.mjs) as imagens dos mapas da
// lista de salas, iguais ao thumb() da página e em 2×: o ícone da linha (40 px → 80, o renderMinimap em 72 → 144) e a prévia da sala
// escolhida (380 px → 760, o chão em 420 → 840, 86% do quadro, com a moldura de 1,5 px). As fichas dos jogadores a tela nativa desenha
// por cima. Define window.__exportMp(porta): as imagens vão por POST pro receptor (a resposta do Runtime.evaluate não aguenta a prévia).
// O giro é o da sala e da partida (225°, 06/11). Os ids são próprios (900+): o cache da página (MAPS[id]) devolvia o SALT FLATS no
// lugar do BROKEN HILLS.
(() => {
  window.__exportMp = async sink => {
    const list = [{ id: 900, real: 'World_TestSalt2', terr: 'DESERT', cond: 'DAY' }, { id: 901, real: 'World_TestDunesSim', terr: 'DESERT', cond: 'DUSK' },
      { id: 902, real: 'World_TestHills', terr: 'DESERT', cond: 'LOW SUN' }];
    const post = async (name, c) => fetch(`http://127.0.0.1:${sink}/${name}`, { method: 'POST', mode: 'no-cors', body: await new Promise(r => c.toBlob(r, 'image/png')) });
    const names = [];
    for (const m of list) {
      const M = await getMap(m);
      for (const [size, flatN, frame, suf] of [[80, 144, false, 'mpicon'], [760, 840, true, 'mppv']]) {
        const c = document.createElement('canvas'); c.width = c.height = size; const g = c.getContext('2d');
        g.imageSmoothingQuality = 'high';
        g.translate(size / 2, size / 2); g.rotate(Math.PI * 1.25);
        const s2 = size / Math.SQRT2 * (frame ? .86 : 1);
        g.drawImage(renderMinimap(M, flatN), -s2 / 2, -s2 / 2, s2, s2);
        if (frame) { g.strokeStyle = 'rgba(230,220,190,.35)'; g.lineWidth = 3; g.strokeRect(-s2 / 2, -s2 / 2, s2, s2); }
        await post(`${m.real}_${suf}.png`, c);
        names.push(`${m.real}_${suf}.png (${M.starts.length} bases)`);
      }
    }
    return names.join('\n');
  };
  return true;
})()
