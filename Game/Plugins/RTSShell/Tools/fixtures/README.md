# Os dados de mentira das fotos de teste das telas da rede

O jogo de verdade dá esses JSON pelo `api/lan.json` e pelo `api/lobby.json` (`RTSShellGame.cpp`). Nas fotos de teste as telas
nativas leem daqui, pelo `-RTSLanFake=<arquivo>` (a lista de salas) e pelo `-RTSLobbyFake=<arquivo>` (a sala da LAN). São os mesmos
dados do `mock_net.js` com que o mockup foi fotografado no Chrome, pra comparar pixel a pixel.

- `lan_two_rooms.json`: duas salas abertas na rede (Felipe com 2, Rafa com 1).
- `lobby_host.json`: a sala vista por quem hospeda (Felipe, você; Pai pronto; uma IA).
- `lobby_guest.json`: a mesma sala vista pelo Pai.

## 06/11: a sala nova da LAN e as telas da partida

- `lobby_host.json` e `lobby_guest.json` refeitos no estado do mockup no modo sala (`skirmish.html?mp=1`): cada vaga com a facção, a
  base, a cor das 24, o nível, o ping e o READY; o chat da sala (`chat`, `chatTotal`), as fechadas (`closed`) e a contagem (`countdown`).
- `match_*.json` (o `-RTSMatchFake=<arquivo>` das fotos das telas da partida, `match.html?m=loading|pause|result|net`): o `match.json`
  que o `RTSShellGame` dá, com a largada (`intro`) da tela de carregar. Os números são os do mockup (`screens.html`); o histórico do
  gráfico é sorteado com a mesma semente do mockup. `skirmish` e `lan` (o Esc), `victory` e `defeat`, `net_wait` e `net_sync`.
  `scene` (a cena do SCRIPT, sem jogadores na largada), `short` (a partida curta demais pro gráfico) e `setup_error` (06/11 à noite:
  a partida que não monta, o caso 7 do aviso, `m=net`).
