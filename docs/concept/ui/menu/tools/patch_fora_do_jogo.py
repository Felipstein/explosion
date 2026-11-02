# Marca no protótipo o que NÃO vai pro jogo (02/11, o Felipe: "esconder no jogo não é o suficiente, tem que remover completamente,
# senão vai inflar as coisas com coisas desnecessárias"): a barra de telas e o painel de debug, os ganchos de gravação, os 100 mapas
# e as salas de mentira, a conversa de mentira da sala, e as telas da partida que ainda não estão ligadas no jogo (carregando, Esc,
# resultado, avisos de rede, primeira abertura). O export_game.py corta tudo entre /*fora-do-jogo*/ ... /*/fora-do-jogo*/ (CSS e JS)
# e <!--fora-do-jogo--> ... <!--/fora-do-jogo--> (HTML).
import os
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A, B = '/*fora-do-jogo*/', '/*/fora-do-jogo*/'
HA, HB = '<!--fora-do-jogo-->', '<!--/fora-do-jogo-->'


def patch(name, ops):
    p = os.path.join(S, name); s = open(p, encoding='utf-8').read()
    for kind, a, b in ops:
        assert s.count(a) == 1, (name, a[:60], s.count(a))
        if kind == 'wrap':        # embrulha do início de a até o início de b (b fica de fora)
            i = s.index(a); j = s.index(b, i); s = s[:i] + A + '\n' + s[i:j] + B + '\n' + s[j:]
        elif kind == 'hwrap':
            i = s.index(a); j = s.index(b, i); s = s[:i] + HA + '\n' + s[i:j] + HB + '\n' + s[j:]
        else:                     # troca
            s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


# ---------- as telas: no jogo só as opções
patch('screens.html', [
    ('wrap', "#proto { position: fixed;", ".cells { display: flex;"),   # a barra de telas
    ('rep', "body.clean #proto { display: none; }\n", ''),
    ('rep', ".tok { width: 34px;", A + "\n.tok { width: 34px;"),
    ('rep', ".tok img { width: 25px; height: 25px; }\n", ".tok img { width: 25px; height: 25px; }\n" + B + "\n"),
    ('wrap', ".cells { display: flex;", "\n</style>"),
    ('wrap', "/* o carregando */", "/* as opções:"),
    ('wrap', "/* os avisos de rede */", "</style>"),
    ('hwrap', '<div id="proto"><span>Tela</span>', '<div id="wrap"><div id="stage">'),
    ('hwrap', "  <!-- o carregando -->", "  <!-- as opções -->"),
    ('hwrap', "  <!-- os avisos de rede -->", '  <div class="corner c1">'),
    ('hwrap', '<script src="shared_maps.js"></script>', '<script>\nconst W = 1920'),
    ('rep', "if (Q.get('clean') || (window.ue && window.ue.shell)) document.body.classList.add('clean');   // no jogo, sem a barra do protótipo\n", ''),
    ('wrap', "const COLORS = { blue:", "let cur = Q.get('s')"),
    ('rep', " document.querySelectorAll('#proto [data-s]').forEach(b => b.classList.toggle('on', b.dataset.s === s && (!b.dataset.r || b.dataset.r === resVar)));",
     " " + A + "document.querySelectorAll('#proto [data-s]').forEach(b => b.classList.toggle('on', b.dataset.s === s && (!b.dataset.r || b.dataset.r === resVar)));" + B),
    ('wrap', "document.querySelectorAll('#proto [data-s]').forEach(b => b.onclick", "let resVar = "),
    ('wrap', "// ---------- o carregando:", "// ---------- as opções:"),
    ('wrap', "// ---------- os avisos de rede", "// ---------- o som"),
    ('rep', "if (Q.get('esc')) escMode = Q.get('esc'); if (Q.get('nv')) nv = Q.get('nv'); ",
     A + "if (Q.get('esc')) escMode = Q.get('esc'); if (Q.get('nv')) nv = Q.get('nv'); " + B),
])

# ---------- o menu: o painel de debug e os ganchos da gravação
patch('menu.html', [
    ('wrap', ".dbg { position: absolute;", ".ver { position: absolute;"),
    ('hwrap', '  <div class="dbg" id="dbg">', '  <div class="corner c1">'),
    ('rep', "if (window.ue && window.ue.shell) { document.getElementById('dbg').style.display = 'none'; }   // no jogo as telas da partida são as de verdade\n", ''),
    ('wrap', "document.getElementById('dbg').onclick = ", "\n"),
    ('wrap', "window.__step = (dt, n = 1) =>", "const t0 = +(Q.get('t') || 30);"),
    ('rep', "if (Q.get('hv')) window.__hover(+Q.get('hv'));\n", A + "\nif (Q.get('hv')) window.__hover(+Q.get('hv'));\n" + B + "\n"),
])

# ---------- o skirmish: só os 3 mapas de verdade; a conversa de mentira da sala e o passo da gravação ficam no protótipo
patch('skirmish.html', [
    ('wrap', "const W1 = ['SALT',", "const CAT = ["),
    ("rep", "  { id: 1, real: 'World_TestDunesSim', name: 'DUNE SEA', players: 4, size: 'M', terr: 'DESERT', cond: 'DUSK', src: 'OFFICIAL', plays: 4200, date: 998 }];\n",
     "  { id: 1, real: 'World_TestDunesSim', name: 'DUNE SEA', players: 4, size: 'M', terr: 'DESERT', cond: 'DUSK', src: 'OFFICIAL', plays: 4200, date: 998 },\n"
     "  { id: 2, real: 'World_TestHills', name: 'BROKEN HILLS', players: 4, size: 'M', terr: 'DESERT', cond: 'LOW SUN', src: 'OFFICIAL', plays: 3100, date: 997 }];\n"),
    ('wrap', "(() => { const r = rng(7); const used = new Set(CAT.map(c => c.name));", "// no jogo, só os mapas reais"),
    ('rep', "// no jogo, só os mapas reais (os que existem no jogo e têm o minimapa exportado em maps/)\n", "// a ponte do jogo (só existe dentro do jogo)\n"),
])
p = os.path.join(S, 'skirmish.html'); s = open(p, encoding='utf-8').read()
i = s.index("if (UE) { CAT.length = 0;"); j = s.index("const FAV = new Set([0]);", i); s = s[:i] + s[j:]   # o troca-catálogo não precisa mais
s = s.replace("(() => { const r = rng(7); const used = new Set(CAT.map(c => c.name));\n  for (let i = 2;", "(() => { const r = rng(7); const used = new Set(CAT.map(c => c.name));\n  for (let i = 3;")
open(p, 'w', encoding='utf-8').write(s)
patch('skirmish.html', [
    ('rep', "\n  if (IAMHOST && Math.random() < .7) setTimeout(() => say(['bora', 'fechou', 'sem rush hein', 'gg'][Math.floor(Math.random() * 4)], false, 'PAI'), 1400); } });",
     "\n  " + A + "if (IAMHOST && Math.random() < .7) setTimeout(() => say(['bora', 'fechou', 'sem rush hein', 'gg'][Math.floor(Math.random() * 4)], false, 'PAI'), 1400);" + B + " } });"),
    ('wrap', "  if (IAMHOST) { say('Room created · LAN · v0.5.2', true);", "  const r0 = render;"),
    ('wrap', "window.__step = (dt, n = 1) => { for (let i = 0; i < n; i++) frame(dt); return T; };", "\n"),
])

# ---------- o multiplayer: os mapas e as salas de mentira, e o entrar/criar de mentira
p = os.path.join(S, 'multiplayer.html'); s = open(p, encoding='utf-8').read()
a = "  { id: 1, real: 'World_TestDunesSim', name: 'DUNE SEA', players: 4, terr: 'DESERT', cond: 'DUSK' },\n"
assert s.count(a) == 1
i = s.index(a) + len(a); j = s.index("// as salas (de mentira", i)
fake_maps = s[i:j]                                  # as 4 de mentira, terminando em "}];\n"
s = s[:i].rstrip(',\n') + '];\n' + A + '\nMAPS_LIST.push(\n' + fake_maps.rstrip().rstrip(';').rstrip(']') + ');\n' + B + '\n' + s[j:]
a = "const ROOMS = {\n"; assert s.count(a) == 1
i = s.index(a); j = s.index("let src = Q.get('src')", i)
rooms = s[i + len(a):j]
s = s[:i] + "const ROOMS = { online: [], lan: [] };\n" + A + "\nObject.assign(ROOMS, {\n" + rooms.rstrip().rstrip(';').rstrip('}').rstrip() + "\n});\n" + B + "\n" + s[j:]
s = s.replace("// as salas (de mentira, pro protótipo): cheia, em jogo, versão diferente, amigo, senha\nconst P = ",
              "const P = ")
s = s.replace("const ROOMS = { online: [], lan: [] };\n", "const ROOMS = { online: [], lan: [] };\n// as salas de mentira do protótipo: cheia, em jogo, versão diferente, amigo, senha\n", 1)
a = "let join = function (r) {"; assert s.count(a) == 1
i = s.index(a); j = s.index("\n", i)
proto_join = s[i:j].replace("let join = function (r) {", "join = function (r) {", 1)
s = s[:i] + "let join = null;\n" + A + "\n" + proto_join + "\n" + B + s[j:]
a = "$('#host').onclick = () => { location.href = `skirmish.html?mp=1&host=FELIPE&room=FELIPE'S ROOM&src=${src === 'ip' ? 'online' : src}`; };\n"
assert s.count(a) == 1
s = s.replace(a, A + "\n" + a + B + "\n")
open(p, 'w', encoding='utf-8').write(s)
print('ok')
