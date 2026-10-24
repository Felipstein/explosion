# Multiplayer: o estudo (10/10/2026)

O pedido do Felipe: multiplayer online e em LAN, "da forma mais performática, bonita, sincronizada, perfeita e de baixíssimo custo". Vem depois de fechar a v1, entra na v1 como `v1-multiplayer` e abre a v2.

Três pesquisas em paralelo:
1. como os RTS de hoje fazem a rede, lendo o código de OpenRA, 0 A.D., Recoil/BAR e do ZH (TheSuperHackers), mais as fontes originais;
2. transporte, NAT, relay, salas e custo;
3. a auditoria do nosso código.

Depois, um revisor tentou derrubar o plano (Parte 6), e as correções dele já estão no texto. As fontes estão no fim. Nos números: [V] = verificado na fonte; [NV] = não verificado; [E] = estimativa, com a conta aberta.

---

## Resumo: o que muda pra você

- **Cada PC roda a mesma simulação, e pela rede só passam os cliques** (lockstep). É o que StarCraft II, Age of Empires, Beyond All Reason, Factorio e o próprio ZH fazem.
  - A rede gasta quase nada: ~1,5 kB/s por jogador.
  - Não precisa de servidor pago.
  - O replay é só a lista de cliques, e todo mundo vê exatamente a mesma batalha.
- **O anfitrião é o relógio da partida.** Ele carimba em que passo cada clique vale e manda o pacote de cada passo pra todos. Um clique atrasado vale um passo depois, mas nunca some e nunca dessincroniza. É o modelo do ZH, do Beyond All Reason e do Factorio.
- **O atraso do clique se ajusta à conexão de cada jogador, medida:** o link ruim de um não atrasa os outros. Estimativa do clique ao 1º quadro com movimento: ~90–120 ms na LAN, ~140–180 ms online em São Paulo. Até ~250 ms ninguém nota (Age of Empires mediu isso). O jogo esconde o atraso respondendo na hora com o som, a marca no chão e a seleção.
- **Sem rollback** (o "voltar no tempo" do Stormgate). Num RTS ele custa caro e ganha pouco; a conta está na Parte 1. A porta fica aberta.
- **Custo de servidor: zero ou quase.**
  - LAN e IP direto funcionam sem internet.
  - Online pelo **Epic Online Services**: salas e relay grátis, e o jogador não precisa de conta Epic.
  - A Epic não diz onde ficam os relays. Se o mais perto de São Paulo estiver longe (medir antes), o online passa por um **servidor de turnos nosso em São Paulo**: o mesmo código do anfitrião, sem tela, num VPS de ~US$ 5/mês.
  - **Steam** só se o jogo sair lá (US$ 100 uma vez pela loja; o relay dela é grátis).
- **Sincronia garantida e vigiada:** a nossa simulação já é exata (ponto fixo, o mesmo resultado em 3 compilações). Cada passo leva uma impressão digital de 8 bytes, e uma divergência é pega na hora, com relatório que aponta o sistema e a unidade.
- **O que falta é do lado da Unreal.** Hoje cada PC monta a própria partida (semente do relógio, posições em float, grade tirada do terreno). Isso passa a ser feito só pelo anfitrião, que manda pra todos. O formato já existe.

---

## Parte 1. Como os RTS de hoje fazem a rede

| Jogo | Arquitetura | Simulação | Atraso do clique | O que dá pra aprender |
|---|---|---|---|---|
| Age of Empires 1/2 (1997–99) | lockstep P2P, o anfitrião decide o ritmo | turno de 200 ms | adaptativo (sobe rápido, desce devagar) | "250 ms nem se nota"; atraso **constante** é melhor que variável [V 1] |
| C&C Zero Hour | tudo passa pelo anfitrião ("packet router"), com lista de reserva | 30 Hz | adaptativo: latência máxima × 1,1; mínimo de 4 passos (133 ms) desde 08/2025 | o PC mais lento puxa todos pra baixo; CRC só a cada 3,3 s [V 2] |
| Beyond All Reason (Recoil/Spring) | o servidor (anfitrião ou dedicado) é o relógio | 30 Hz | ida e volta + fila, sem atraso fixo | ritmo pela CPU mediana (alvo de 60%); hash todo passo; depurador de desync [V 18] |
| Factorio | o servidor carimba o passo de cada ação, UDP | 60 Hz | ida e volta, renegociado a cada 5 s | "camada de latência": movimento, seleção e construção aparecem na hora; ações "nunca ignoradas, só atrasadas" [V 23–26] |
| StarCraft II | lockstep via Battle.net | 16 (Normal) / 22,4 (Faster) | sem número oficial | "Recover Game" retoma pelo replay; "Rewind" [V 7, 8] |
| Warcraft III | lockstep via servidores | — | 250 ms artificiais até 2017; depois 100 ms | o atraso fixo foi o maior defeito [V 10] |
| OpenRA | servidor que repassa, em TCP | 25 Hz | ~360 ms fixos | TCP dá tranco a cada perda [V 16] |
| 0 A.D. | o anfitrião redistribui | turno de 200 ms | 600–800 ms fixos | o próprio código tem "TODO: variar pelo ping" [V 17] |
| Supreme Commander / FAF | P2P em malha; o FAF usa relay TURN | 10 Hz | 200–400 ms | ~5% das partidas longas dessincronizavam numa fase; sem reconexão [V 19–21] |
| Realms of Ruin (Frontier, GDC 2024) | lockstep com servidor dedicado | 16 Hz, ponto fixo (float dá erro de compilação) | buffer que cresce com rede ruim | "padding de struct é o desync nº 1"; rollback deixado pra depois [V 27] |
| Stormgate | sim própria + UE5, servidores alugados | 64 Hz | — | rollback anunciado; o online saiu do ar em abr/2026, quando o serviço de servidores fechou [V 12–15] |
| Planetary Annihilation | servidor manda o estado (não é lockstep) | 10 Hz | ida e volta | ~1 Mbit/s por jogador, 50–100× o lockstep; servidor pago [V 22] |

AoE2 DE, AoE4, CoH3, AoM Retold e Battle Aces não têm nada técnico público confiável.

**As técnicas dos melhores, da mais importante pra menos:**
1. **Determinismo por construção:** ponto fixo, iteração ordenada, nada de float. Nós já temos.
2. **Um relógio que carimba o passo de cada comando** (Spring, Factorio, ZH, OpenRA). É a base de tudo: observador, replay, reconexão, troca de anfitrião.
3. **Atraso adaptativo e medido**, que sobe na hora e desce devagar.
4. **UDP com redundância:** cada pacote leva todos os comandos ainda não confirmados, sem esperar reenvio. Assim 2% de perda passa sem tranco.
5. **Buffer de oscilação com ajuste fino do ritmo** (±10% no OpenRA): tira o engasgo sem subir o atraso.
6. **Impressão digital a cada passo**, mais uma ferramenta que acha o sistema que divergiu.
7. **Resposta local imediata:** som, marca, seleção, o fantasma do prédio (já temos).
8. **Ritmo pela CPU com folga, mostrando quem é o lento.**
9. **Relay** pra atravessar o roteador e esconder o IP.
10. **Snapshot + avanço rápido** pra reconexão e observador atrasado.
11. **Rollback:** o mais caro, e o último.

**Rollback no nosso caso** [E]: ~1 ms por passo com 800 tanques, ~2,5–3 ms com 2000. Esconder 150 ms é re-simular 5 passos: 12–15 ms por correção. Numa batalha de 8 jogadores, com 3–4 comandos por segundo cada, haveria correção quase todo quadro, e isso não cabe nos 16,7 ms de um quadro de 60 qps ao lado do desenho. RTS tem 1–4 comandos por segundo, não 60 como luta. **Decisão: sem rollback.** O snapshot do estado sai de qualquer jeito, pra reconexão. Reavaliar só se a medida online passar de ~250 ms.

**Os erros mais comuns, e o que nos protege:**

| Erro | Proteção |
|---|---|
| Float na simulação (AoE2 DE e AoE4 dessincronizavam no Linux e no Steam Deck) | Nosso SimCore não tem float. |
| Padding ou memória não inicializada no hash | O hash lê campo por campo. |
| Hash raro e sem ferramenta | Hash a cada passo, mais o despejo com comparação. |
| Atraso fixo | Atraso adaptativo. |
| Travar todos sem dizer quem | "Esperando o Fulano" na tela. |
| TCP | UDP com redundância. |
| Rajada de comandos derrubando a partida | Teto de comandos por passo. |
| Dessincronia sem relatório e sem volta | Relatório automático e reconexão. |
| Depender do servidor de terceiro (o GameSpy matou o online do ZH; o fim da Hathora, o do Stormgate) | O anfitrião é um jogador, o relay é trocável, e a LAN não depende de ninguém. |
| Engasgo local virando travada de rede | Simulação separada do desenho, aquecimento antes da partida, p95 do quadro como critério. |

---

## Parte 2. A conexão: LAN, internet e custo

| Opção | Atravessa o roteador? | Custo | Depende de | LAN sem internet | Cifra |
|---|---|---|---|---|---|
| **UDP nosso (LAN e IP direto)** | não (na LAN não precisa) | US$ 0 | nada | **sim** | a nossa |
| **Epic Online Services: P2P + salas + voz** | sim (ICE; se falhar, relay na AWS; com `ForceRelays` o IP fica escondido) | **grátis**, sem royalty | conta de dev da Epic (do Felipe); o jogador entra sem conta Epic (ID do aparelho, Steam, Discord...) | não | sim |
| **Steam: Messages + relay (SDR) + salas** | sim, relay por padrão, PoP em São Paulo | relay grátis; US$ 100 por app (volta depois de US$ 1.000 de vendas) | Steam aberta e o jogo comprado | não | sim |
| GameNetworkingSockets (Valve, aberto) | ICE próprio, sem o relay da Valve | US$ 0 + o nosso relay | nada | sim | sim |
| ENet / KCP | não | US$ 0 | nada | sim | **não** |
| Relay próprio num VPS | sempre | €0–150/mês conforme o volume; **na Europa, ~200 ms a mais pro brasileiro** | nós operamos | — | a nossa |

**Recomendação: três caminhos atrás de uma porta só.**
1. **LAN e IP direto** (o nosso UDP): a base, funciona sem internet e é onde a gente depura.
2. **EOS** pro online: grátis, em qualquer loja, sem servidor nosso.
3. **Steam** se lançar lá. Entra sem refazer nada.

Por cima de todos, o nosso protocolo:
- pacote de até ~1.100 B (o EOS aceita 1.170);
- redundância;
- a nossa cifra (ChaCha20-Poly1305 ou AES-GCM, do OpenSSL que já vem na Unreal), porque a LAN e o IP direto não têm cifra;
- aperto de mão de versão.

Não usar a replicação da Unreal (NetDriver): ela é feita pra servidor-cliente.

**Custo por mês** [E: 10 h por jogador por mês, 4 por partida]:

| Jogadores | Nosso plano (LAN + EOS [+ Steam]) | Relay próprio | TURN da Cloudflare |
|---|---|---|---|
| 0 | US$ 0 (+ US$ 100 uma vez, se for pra Steam) | €0 | US$ 0 |
| 1.000 | **US$ 0** | ~1 VPS (~€6) + o nosso tempo | US$ 0 (dentro do grátis) |
| 100.000 | **US$ 0** | €40–150 em 2–3 regiões | ~US$ 1.150 |

**As pegadinhas:**
- **EOS:**
  - o relógio do PC errado faz a conexão falhar calada (avisar o jogador);
  - não há garantia de relay em São Paulo [NV];
  - o "grátis pra sempre" é promessa da Epic;
  - sala pública pode expor o IP se não usar o canal secreto.
- **Steam:** o Nagle vem ligado (mandar sem espera); o ICE direto expõe o IP (desligar).
- **NAT simétrico e CGNAT** (comum no Brasil): o furo direto falha. O relay resolve. O IPv6 está em ~47–50% dos brasileiros (Internet Society/NIC.br; uma das pesquisas tinha dito 89%, o revisor corrigiu), e o roteador costuma bloquear a entrada IPv6: "IP direto" pela internet quase só funciona com porta aberta.
- **Firewall do Windows:** se o jogador cancelar o aviso na 1ª vez, o Windows bloqueia pra sempre. O instalador cria a regra.
- **UPnP:** só bônus, nunca requisito (falha no CGNAT e é risco de segurança).

---

## Parte 3. O nosso código

**Já está pronto** (auditoria do código):
- **Ponto fixo de verdade:** nenhum float, `unordered_map` ou relógio no SimCore; ordenações com desempate total; 3 compilações com o mesmo hash.
- **Comandos** binários com leitura que confere tudo (20 mil entradas de lixo no teste).
- **A simulação valida cada comando:** dono, unidade viva, dinheiro, pré-requisito, lugar da obra. Um cliente modificado não consegue ordem impossível com o que é dele.
- **Hash do estado a cada passo**, cobrindo todos os campos (~0,1 ms com 800 unidades).
- **A montagem da partida (`MatchSetup`)** já é serializável e tem hash: grade, regras, depósitos e jogadores. É exatamente o pacote que o anfitrião vai mandar.
- **O replay** (montagem + comandos + hash a cada 15 passos) serve pra reconexão por re-simulação: 30 min de partida re-simulam em ~1 min.
- **A Unreal só entra na simulação por dois lugares:** a montagem e o passo com os comandos.

**Os riscos, do mais grave pro menos:**
1. **Crítico: cada PC monta a própria partida.** A semente vem do relógio (`RTSMatchSubsystem.cpp:261`), as largadas saem de contas em `double` e os depósitos são realocados com seno e cosseno da biblioteca C. Dois PCs divergem no passo 0. → Só o anfitrião monta; os outros recebem os bytes e conferem o hash.
2. **Crítico: a grade tirada do terreno pode sair diferente em cada PC.** Comparações em float perto do limite e o que está carregado (`RTSMapBaker.cpp:91–213`). → A grade viaja dentro da montagem (~0,3 MB). A grade local fica só pro desenho, conferida pelo hash. A versão final é a grade exportada pelo WorldForge.
3. **Alto: a fila de hoje é local.** Não descarta duplicata e recarimba atrasado em silêncio (`Lockstep.h:30–59`). → O anfitrião carimba.
4. **Alto (trapaça): o campo `Player` do comando é confiado.** Um cliente modificado mandaria ordens pelo outro jogador. → A sessão carimba o jogador pela conexão, com teto de comandos por passo.
5. **Alto (trapaça): o F9 mostra o mapa todo** e a chave de depuração está sempre ligada (`RTSMatchSubsystem.cpp:265, 966`). → Desligar online.
6. **A IA:** a auditoria dizia "só no anfitrião"; o revisor mostrou que ela está no SimCore e só lê o estado (`SkirmishAI.h:20`), então rodar em todo PC é determinístico, como no ZH: sem tráfego, sem atraso, e não morre com o anfitrião. → **Em todos os PCs, com o estado dela no hash.**
7. **Médio:**
   - as regras são lidas do JSON em cada PC (→ hash no aperto de mão);
   - a pausa, o relógio e o início da partida são locais (→ viram comandos ou controle da sessão);
   - a lista do hash é feita à mão (→ teste que pega campo esquecido).
8. **Baixo:**
   - o ponto fixo foi testado só no MSVC (→ clang-cl no `simtest`);
   - dá pra mandar atacar alvo na névoa;
   - a leitura da montagem aceita valores fora de faixa;
   - um grupo de 500 unidades não cabe num pacote (→ IDs por diferença ou fragmentar).

---

## Parte 4. A proposta

**Modelo: lockstep com um servidor de turnos como relógio.** O servidor de turnos é um papel, separado do jogador: na LAN é o anfitrião; online, o anfitrião pelo relay da Epic ou um VPS em São Paulo, conforme a medição.
1. Cada jogador manda os seus comandos ao servidor de turnos, em UDP, no pacote do passo e também na hora do clique (~16 ms a menos), com todos os ainda não confirmados.
2. O servidor de turnos:
   - carimba o jogador (pela conexão) e o passo: **sempre o próximo ainda não selado**. É monotônico, então um `Move` nunca passa na frente do `SetGroup` do mesmo jogador;
   - descarta duplicatas;
   - aplica o teto de comandos por passo;
   - sela e manda a todos cada passo, com a impressão digital, levando os passos que aquele cliente ainda não confirmou (passo vazio custa só um contador "selado até").
3. Cada cliente toca **B_i passos atrás do último selado**: a folga dele contra a oscilação, medida (p99 da chegada + p95 do quadro local), que sobe na hora e desce 1 passo a cada ~5 s estável. Um buraco pede reenvio na hora.
4. O atraso que cada um sente é a ida e volta **dele** até o servidor de turnos, mais a folga **dele**: o link ruim só prejudica o próprio dono.
5. Pra ser justo, o anfitrião que joga se atrasa sozinho: os comandos dele valem a mediana do atraso dos outros.
6. Sem troca de anfitrião na v1: se ele sai, a partida acaba e o replay é salvo.

**Por que estrela, e não malha** (cada um manda pra todos)?
- Uma ordem só, carimbada num lugar só.
- O link ruim só atrasa o próprio dono; na malha, todos ficam presos à pior subida.
- A queda de um jogador é simples.
- É o modelo que funciona há anos no Spring, no Factorio e no ZH.

A conta da latência (revisor, com o desenho incluído, do clique ao 1º quadro com movimento):

| Caminho | Clique → movimento |
|---|---|
| LAN | ~90–120 ms |
| Online direto em São Paulo | ~140–160 ms |
| Relay da Epic em São Paulo | ~150–180 ms |
| Relay da Epic no leste dos EUA | **~360 ms** (aí entra o servidor de turnos em São Paulo, ~150 ms) |

Trapaça: o anfitrião que joga é quem mais pode trapacear (ele vê os comandos antes de selar). Cada cliente mede o próprio atraso de carimbo e acusa o fora do normal; o ranqueado é só no servidor neutro.

**Os números iniciais** [E, a confirmar com medida; versão do revisor]:

| Item | Valor |
|---|---|
| Simulação / envio | 30 Hz; pacote a cada passo e também na hora do clique |
| Folga de cada cliente (B_i) | p99 da chegada + p95 do quadro; sobe na hora, desce 1 passo a cada 5 s |
| Carimbo | sempre o próximo passo não selado |
| Atraso do anfitrião que joga | mediana(ida e volta + folga) dos outros |
| Redundância | nos dois sentidos, pacote de até 1.100 B (o EOS aceita 1.170); `SetGroup` grande em diferença com varint e fragmentado |
| Meta (p95, do clique ao quadro) | ≤ 150 ms na LAN; ≤ 250 ms online na região |
| Impressão digital | 64 bits a cada passo; hashes por sistema guardados ~10 s e pedidos só na divergência |
| Banda | ~3 kB/s por jogador com cabeçalhos; ~50 kB/s no anfitrião com 7 |
| Queda | aviso aos 5 s; espera até 60 s e votação |
| Ritmo | o anfitrião desacelera se algum PC passar de ~70% da CPU da simulação, e diz quem |
| Reconexão | v1: replay + re-simulação rápida, sem desenhar (1×1 de 20 min ≈ 11 s); v2: snapshot do anfitrião |
| Relógio | a rede numa thread própria, com timer fino; o passo local é dirigido pela folga (vários passos por quadro pra alcançar, com teto de tempo); um engasgo do desenho não trava a rede |
| Observador | o fluxo do anfitrião com atraso (0–30 s no casual) |
| Pausa | controle marcado pra um passo futuro, com cota por jogador |

**"Bonita", que é o que o jogador sente:**
- **Na hora do clique:** a marca, o som e a seleção saem na hora (já é assim). O fantasma do prédio também já é local. Dá pra mais, sem tocar a simulação: o motor "acordando" e a fumaça do escapamento na hora, a fila e o dinheiro "pendentes" no HUD, a obra "pendente" no lugar do fantasma.
- **Movimento:** o desenho segue interpolado entre os passos (já é assim); o atraso só aparece na largada.
- **Sem tranco:** o buffer de oscilação e o ajuste fino do ritmo; perda de 2% passa sem tranco.
- **Na sala:** o ping de cada jogador, pra ninguém ser surpreendido.
- **Quando alguém atrasa:** o nome dele na tela, nunca "a partida travou".

**Os componentes** (módulo novo `Game/Source/NetCore`, C++ puro como o SimCore, testado no `simtest` sem a Unreal):
- **`INetTransport`:** manda e recebe datagramas. Implementações: `Loopback` (testes com perda, oscilação, atraso, duplicata e reordenação), `UdpDirect` (LAN e IP direto, na Unreal), `EosP2P` e `SteamMessages` depois.
- **`ILobby`:** `LanBroadcast`, `EosLobby`, `SteamLobby`.
- **`Protocol`:**
  - aperto de mão: `Hello` (versões e hashes), `Welcome` ou `Reject`;
  - sala e carregamento: `Lobby`, `SetupChunk`, `LoadProgress`, `Start`;
  - o jogo: `Input`, `TickBundle`, `Ack`, `Control`;
  - dessincronia e volta: `DesyncAt`, `DumpChunk`, `Rejoin`.
- **`HostSession` / `ClientSession`:** sala, aperto de mão, a montagem em pedaços, a barreira "todos em 100%".
- **`TurnRelay`** (no anfitrião): o relógio e o carimbo.
- **`TurnBuffer`** (em todos): os pacotes de passo recebidos.
- **`DesyncDetector`:**
  - compara os hashes;
  - na divergência, volta à cópia do estado de até 150 passos atrás, re-simula e grava o despejo;
  - `ReplayInspect -Diff` aponta o sistema, a unidade e o campo.
- **`NetMatch`:** a fachada que o `RTSMatchSubsystem` usa no lugar da fila de hoje. O skirmish passa a usar o mesmo caminho (anfitrião + loopback sem atraso), então a rede é testada todo dia.
- **No SimCore:**
  - hashes por sistema;
  - o snapshot binário com `Load`, testado ida e volta (os dados fixos, como a grade e o rascunho do A*, separados do estado);
  - o **ID do build** (hash do binário do SimCore + regras + grade) no aperto de mão, recusando qualquer diferença;
  - o comando `Resign` (render-se) e `Drop` (só o servidor de turnos emite);
  - faixas na leitura da montagem;
  - clang-cl com UBSan no `simtest`, e a aritmética que pode estourar em `uint64`;
  - o estado da IA no hash.
- **A cifra própria sai da v1:** a LAN não precisa, e o online confere o que o EOS já faz no N8.

**Os marcos** (cada um testável; até o N3 com uma placa só; ordem do revisor):

| Marco | Entrega | Pronto quando |
|---|---|---|
| **N0. Juiz pronto pra rede** | hashes por sistema, snapshot com `Load`, ID do build, UBSan, `Resign`/`Drop`, a IA no hash, F9 e depuração desligados online | duas rodadas dão o mesmo snapshot; uma diferença forçada aparece no sistema certo |
| **N1. Rede de mentira** | NetCore + loopback, sem Unreal, com a folga por jogador | 2 e 8 jogadores, 30 min a 0/100/200 ms, com 2% de perda e **perda em rajada** de 100–300 ms, duplicata, reordenação e engasgo injetado: zero dessincronia; a forçada é pega na hora |
| **N2. Montagem e relógio** | `BuildSetup` só no anfitrião + `StartFromSetup` em todos; a rede numa thread própria; o passo dirigido pela folga; o skirmish passando pela rede local | skirmish igual ao de hoje; um engasgo de 500 ms no desenho não perde passo |
| **N3. LAN** | UDP na Unreal + `simpeer.exe` (o 2º jogador sem placa) + o medidor clique → movimento | 30 min sem dessincronia; o medidor dentro da meta |
| **N3b. O 2º PC** | jogo empacotado com o cache de PSO e a regra do firewall | o seu teste com 2 PCs na LAN |
| **N4. Sala** | sala na LAN, aperto de mão, barras de carregamento, início junto | versão ou conteúdo diferente recusado com o motivo |
| **N5. Pausa, render-se, queda** | os controles e o "esperando o Fulano" | pausar num PC para os dois no mesmo passo; queda com 60 s e votação |
| **Medição do EOS** | 1 dia: login, eco P2P entre dois PCs em São Paulo, com e sem relay forçado | decide: relay da Epic ou servidor de turnos em São Paulo |
| **N6. Volta pra partida** | reconexão por replay | cair no minuto 10, voltar, e o hash bater |
| **N8. Online** | EOS (ou o VPS): login sem conta, salas, relay | partida com amigo pela internet |
| **N7. Observador e N jogadores** | vaga de observador, HUD pra N | 1×1 com observador e 4 jogadores sem dessincronia |

**Como testar com uma placa só:**
1. a rede de mentira no `simtest` (8 jogadores num processo, 30 min de partida em ~1 min);
2. o `simpeer.exe` (o 2º jogador sem Unreal, por UDP em 127.0.0.1);
3. uma 2ª Unreal sem placa (`-nullrhi`);
4. no fim, o seu teste com 2 PCs.

---

## Parte 5. Decisões suas (pra discutir)

**Decidido em 10/10: só LAN agora; o online é a última coisa do jogo** ("por enquanto não precisamos online, só LAN"). A ordem fica N0 → N1 → N2 → N3 → N3b → N4 → N5 → N6 (reconexão) → N7 (observadores e N jogadores), tudo na LAN. A medição do EOS e o N8 vão pro fim do jogo. O transporte já nasce atrás da interface, então o online entra sem refazer nada. Os itens 3 e 4 abaixo ficam pra essa hora.

1. **Lockstep** (decidido em 10/10: "segue como você achar melhor").
2. **Sem rollback**, com o cosmético que esconde o atraso.
3. **Online pelo EOS.** Precisa que você crie a conta de desenvolvedor na Epic (grátis), porque só você pode aceitar os termos. Depois da medição, se o relay ficar longe, um VPS em São Paulo (~US$ 5/mês).
4. **Sala de amigos: direto quando der (mais rápido, o IP aparece pros amigos) ou sempre pelo relay (esconde o IP)?** A minha sugestão: direto entre amigos, e "esconder meu IP" como opção.
5. **Steam:** vai lançar lá? Muda só o 3º caminho, não o resto.
6. **Até 8 jogadores + observadores?**
7. **Ranqueado e anti-cheat** ficam pra depois. O maphack é o ponto fraco de todo lockstep: EAC grátis + análise de replay, ou o modo "servidor manda o estado" só pro ranqueado, em cima da mesma simulação.

---

## Parte 6. A revisão adversarial (10/10)

O revisor leu o plano e o código e tentou derrubar. **Veredito: fica de pé** (lockstep, ponto fixo, o anfitrião montando, sem rollback). As correções já entraram nas Partes 3 a 5:

| Gravidade | Falha | A correção |
|---|---|---|
| Crítica | O atraso D global prendia todos ao pior link, usava p95 (tranco o tempo todo a 30 pacotes/s) e esquecia o quadro local | A folga por jogador (p99 + quadro), o carimbo no próximo passo não selado, o anfitrião se atrasando sozinho |
| Crítica | Com o relay da Epic longe (leste dos EUA), a estrela dá ~360 ms | Medir antes do online; se passar de ~60 ms de ida e volta, servidor de turnos em São Paulo (~US$ 5/mês) |
| Alta | O passo anda pelo quadro da Unreal (`RTSMatchSubsystem.cpp:509–520`): um engasgo trava todos, e quem engasga mais de 267 ms fica atrás pra sempre | A rede numa thread própria, o passo dirigido pela folga |
| Alta | A versão é um número manual (`SimVersion = 4`) | O ID do build no aperto de mão |
| Alta | Redundância só na subida; 2% de perda uniforme é o caso fácil; um `SetGroup` de 500 IDs não cabe no pacote | Redundância nos dois sentidos + reenvio por buraco; teste com perda em rajada; IDs em diferença e fragmentação |
| Média | O anfitrião que joga é quem mais pode trapacear | Cada cliente mede o atraso de carimbo; ranqueado só no neutro |
| Média | A IA foi mal diagnosticada | Em todos os PCs, com o estado no hash |
| Média | A cópia do `World` leva junto a grade e o rascunho do A* | Dados fixos separados do estado; snapshot com `Load` |
| Média | Estouro de int64 é comportamento indefinido; o `_div128` derruba o processo | UBSan, `uint64` onde pode estourar, teste de estouro |
| Baixa | A banda, o IPv6 e a cifra estavam errados | ~3 kB/s por jogador; IPv6 ~50%; a cifra própria sai da v1 |

**O que ele confirmou:**
- não há float no SimCore;
- o hash cobre ~50 campos, inclusive o sorteio;
- o desenho não consegue mexer na simulação (só `Init` e `Step` alteram o estado);
- as regras já viajam dentro da montagem;
- a LAN não depende de ninguém;
- a reconexão por replay serve pra v1;
- o EOS é grátis, e o login por ID do aparelho existe pra PC sem conta.

**O que dá pra cortar** pra chegar mais rápido numa partida de LAN de verdade:
- a cifra própria;
- a troca de anfitrião;
- o atraso D global;
- os observadores, que ficam pra depois do online.

---

## Parte 7. A LAN que ficou pronta (10/10)

O primeiro corte: lockstep com o anfitrião como relógio, só na LAN, sem pausa, reconexão nem observador. Mora em `Game/Source/RTSGame/{Public,Private}/RTSNetSession.*`.

**Pra você:**
- O menu tem **MULTIPLAYER (LAN)**, e o skirmish local continua igual.
- Na tela da LAN você cria a sala (**HOST GAME**) ou entra numa das salas achadas na rede.
- A sala aceita até 8 jogadores. O mapa de hoje tem 4 bases, então dá 4 em casa, todos contra todos.
- Cada um troca a própria cor no botão **COLOR**, que só passa por cores livres.
- O anfitrião liga e desliga a névoa da partida, e os outros veem a escolha.
- O fim da partida mostra **RETURN TO LOBBY**: o anfitrião leva todos de volta pra mesma sala. Quem ainda está no resultado aparece marcado, e o anfitrião só começa outra com todos de volta.
- Na partida em rede não tem pausa nem F9 (o F9 mostraria o mapa todo).
- Se alguém cai ou o jogo sai de sincronia, aparece uma faixa na tela.

**Times, IA e espectador (11/10)** — decisões do Felipe a partir do estudo (ZH pelo código, SC2, AoE II/IV, WC3, BAR, RA1/RA2, Apex) e de um revisor:
- **Times na sala:**
  - Botão TEAM em cada linha ("–" sem time, 1 a 4). Cada um muda o seu, e o anfitrião ajusta qualquer linha e tem **RANDOM TEAMS**.
  - Não começa com um lado só. Quem está sem time conta como lado sozinho.
  - Na simulação, os times viram números contínuos (a simulação recusa time ≥ 8).
- **Aliados lado a lado:** o anfitrião escolhe a distribuição das largadas que deixa os aliados mais perto (força bruta). No mapa de 4 bases dá sul contra norte. A distribuição vai pronta na montagem.
- **Na partida:**
  - A visão é dividida entre aliados (a simulação já fazia por time) e a vitória é do time, incluindo o aliado que já caiu.
  - A faixa embaixo da barra de cima mostra você e os aliados num bloco azul à esquerda do relógio e os inimigos à direita, como pede `docs/concept/hud/DECISOES.md`. Cada um aparece pelo nome da cor; o derrotado fica cinza com um X vermelho.
  - A seta "enemy base" saiu (pedido do Felipe).
- **IA na sala:**
  - O anfitrião põe computador nas vagas (**+ AI**), com cor e time, e pode tirar (REMOVE).
  - A IA roda só no PC do anfitrião e manda as ordens pela rede como um jogador, carimbadas com a vaga dela. Assim dá 2 contra 2 com 3 PCs, ou a família toda contra a IA.
- **Espectador:**
  - Quem cai vê a câmera ir até o último prédio, o letreiro DEFEATED e a faixa SPECTATING. No lugar da barra de comando entra a lista dos jogadores: cor, time, vivo ou derrotado, unidades, prédios e dinheiro. Espaço leva à última morte à vista.
  - Enquanto o aliado luta, ele vê só a visão do time, pra não "soprar" pro aliado na mesma sala. Quando o time inteiro cai, ou num todos contra todos, vê o mapa todo.
  - É só desenho: a simulação dele continua igual e as ordens dele são recusadas.
  - Todos recebem o aviso "Red was defeated".
- **A base que cai** não explode inteira no mesmo quadro. O que sobra cai em onda a partir do último prédio, a ~35 m/s, como no BAR. Isso evita o engasgo.
- **Névoa em 3 tipos** (menu e sala):
  - **explorada** (o padrão): o mapa aparece desde o começo, escurecido fora da visão, e só as unidades inimigas fora da visão somem. É o C&C em skirmish e multiplayer; no AoE, "Explored".
  - **preta**: o que nunca foi visto fica escuro.
  - **desligada**.
- **A versão da rede** subiu (0002): um PC com o jogo velho não entra na sala nova.
- **Ficou pra discutir:** a onda de colapso com assinatura, as barras time contra time, o sinal no mapa que a IA aliada obedece, a "ajuda que voa" (dar dinheiro vai de avião e, se for abatido, vira a carga que cai) e a diplomacia com traição anunciada.

**Como funciona:**
- **A montagem é uma só.** O anfitrião monta a partida inteira: grade, regras, semente e jogadores (~330 KB). Ele manda em pedaços com confirmação. Ninguém mais monta; a grade de cada PC só serve pro desenho, e o log avisa se ela for diferente da do anfitrião.
- **Os passos.** O anfitrião fecha um passo a cada 1/30 s com os comandos que chegaram, carimbando o jogador pela vaga. Ele manda os passos ainda não confirmados de novo em todo pacote, pra cobrir a perda. O cliente manda os comandos com número de sequência até o anfitrião confirmar.
- **A impressão digital.** Todo pacote leva a impressão digital do estado. O anfitrião confere a de cada jogador, e cada cliente confere a do anfitrião. Uma diferença aparece na hora, com o passo.
- **A rodada.** Cada partida da sala tem um número, e pacote atrasado de outra rodada não entra.
- **As vagas.** Ao começar, as vagas ficam contínuas (0…N−1), e cada um recebe a sua no pacote da sala.

**A pausa, render-se e a queda (N5, 13/10, sem o teste do Felipe):**
- **Pausa pra todos:**
  - No menu (Esc), o botão **PAUSE GAME (everyone)** para a partida em todos os PCs. A tela mostra "PAUSED by Red" e o botão **RESUME**.
  - Qualquer um retoma, com a contagem "Resuming in 3".
  - Na LAN não há limite de pausas (em casa não precisa; o online ganha cota no N8).
  - **Como funciona:** o anfitrião é o relógio e para de fechar passo. Ninguém passa do último passo fechado, então todos param no mesmo passo e o log diz qual ("parado no passo N", com a impressão digital).
  - Parado, as ordens não ficam guardadas pra depois; o efeito das explosões para junto.
- **Render-se:** **SURRENDER** no menu, com um segundo clique pra confirmar. É o comando `Resign` da simulação (versão 11): tudo do jogador cai, como no ZH (`VictoryConditions::killPlayer`), e ele vira espectador. Vale no skirmish também.
- **A queda**, pelos números do ZH (`GlobalData.cpp`: `NetworkDisconnectTime` 5000 e `NetworkPlayerTimeoutTime` 60000 ms; `DisconnectManager.cpp`: cai quando os votos chegam a todos os outros):
  - **aos 5 s sem notícia:** a partida para, com "Waiting for Red", o tempo que falta e o botão **DROP Red**;
  - **se ele volta:** a contagem de 3 s e segue;
  - **aos 60 s, ou com o voto de todos os outros:** ele cai. O anfitrião manda o `Resign` pela vaga dele, então tudo dele cai igual em todo PC, e a partida segue.
  - **quem sai pelo menu** cai na hora, sem a espera.
  - **se o anfitrião some,** os outros veem "Connection lost": ele é o relógio, e a troca de anfitrião fica pro online.
- **A versão da rede** subiu (0003): o jogo velho não entra na sala nova.
- **Testado em 13–14/10, com 3 PCs na LAN de teste (o anfitrião com tela e 2 robôs sem placa):**
  - **pausa:** os 3 pararam no mesmo passo (602), com a mesma impressão digital, e voltaram juntos;
  - **render-se:** o robô se rendeu aos 30 s, virou espectador, e a partida seguiu;
  - **queda:** o robô sumiu sem avisar no passo 900. Aos 5 s o anfitrião parou a partida no passo 1056, os outros dois votaram, ele caiu "votado pelos outros", e a partida voltou no mesmo passo nos dois que ficaram;
  - **o engasgo do anfitrião:** um de 9 s derrubava todo mundo aos 8 s. Agora o cliente mostra "Waiting for the host" e espera até 60 s;
  - **a volta pra sala:** quem caiu ficava na lista, e o anfitrião esperava por ele. Agora sai da sala na volta.
- **Os testes:**
  - `-Extra '-RTSNetTestPause=<passo>'`: o anfitrião do teste pausa e volta 4 s depois;
  - `-BotExtra '-RTSNetBotResignAt=<passo>'`: o 1º robô se rende;
  - `-BotExtra '-RTSNetBotQuitAt=<passo>','-RTSNetTestVote'`: o 1º robô some sem avisar e o 2º vota;
  - `-RTSNetTestVote` no anfitrião: ele vota também.

**O dinheiro inicial** (v4, 22/10):
- O anfitrião escolhe na sala, de 10 000 a 50 000 (`[ - ] $20,000 [ + ]`); os outros vêem ao lado da névoa.
- O pacote da sala leva o valor (U32 depois da névoa; fora de 1 000 a 1 000 000 o pacote não entra). A partida leva nas regras da montagem (`Economy.StartingMoney`, × 5 no `-RTSFast`), então todos começam com o mesmo.
- **A versão da rede** subiu (0004): o jogo velho não entra na sala nova.
- Teste: `game.ps1 view -Folder X -Script lanhost -Money 50000` (o anfitrião clica até 50 000 e confere na partida); no `lanclient`, o robô anfitrião já cria a sala com o valor.

**Os testes com uma placa só** (`game.ps1 view -Folder X -Script lanhost|lanclient -Players 4 [-NoFog]`):
- Os outros jogadores são jogos sem placa (`-nullrhi`), e o robô do skirmish joga por eles pela rede.
- **lanhost:** a nossa tela cria a sala e fica vermelha no botão de cor. Só então os robôs entram, porque o teste abre a porta com um arquivo. Depois os 4 jogam a partida inteira e todos voltam pra sala.
- **lanclient:** o anfitrião é um robô sem placa. A nossa tela acha a sala na lista da rede, entra, troca de cor e joga. No fim, o anfitrião leva todos de volta.
- Cada jogo escreve no fim o passo, o resultado e o estado final. Os 4 têm que dar igual.
- **A sala só com IA** (`-Script lanhost -Players 4 -AI 3`, sem robô): o anfitrião sozinho com 3 IAs. Em 14/10 o Felipe achou a partida parada em "Waiting for the other players" (o começo exigia ao menos um PC de fora); corrigido no `RTSNetSession` e entra no roteiro.
- **O roteiro antes de mandar um pacote** (`-Packaged`, os robôs também do pacote):
  1. a cena com tela;
  2. a `longe` (a briga fora da câmera, o travamento de 14/10);
  3. a sala só com IA;
  4. 2 PCs pelo pacote;
  5. lanhost 3;
  6. lanclient 3;
  7. a pausa;
  8. o render-se;
  9. a queda.
  - Com 4 cópias no mesmo PC a memória acaba (o arquivo de paginação encheu o disco em 14/10): até 3 cópias.

---

## Fontes

[1] Bettner & Terrano, "1500 Archers on a 28.8" (GDC 2001) · [2] código do ZH, TheSuperHackers/GeneralsGameCode (`NetworkDefs.h`, `ConnectionManager.cpp`, `Connection.cpp`, `Network.cpp`) · [7] Liquipedia, Game Speed (SC2) · [8] Liquipedia, Replay Features; Blizzard, patch 3.13 · [10] Liquipedia, WC3 patch 1.28e · [12–15] Stormgate: wccftech, playstormgate.com, keengamer, techspot (servidores fora do ar, 2026) · [16] OpenRA (`Server.cs`, `OrderBuffer.cs`, `OrderManager.cs`) · [17] 0 A.D. (`TurnManager`, `NetServerTurnManager`) · [18] RecoilEngine (`GameServer.cpp`, `NetCommands.cpp`, `SyncDebugger.h`) · [19–21] Forrest Smith, "Synchronous RTS Engines and a Tale of Desyncs" 1 e 2; FAF java-ice-adapter · [22] Forrest Smith, ChronoCam do PA · [23–26] Factorio Friday Facts 83, 302, 55; wiki · [27] Pollard, "Cross-Platform Determinism", GDC 2024 · [30] Glenn Fiedler, "Deterministic Lockstep" · [31] Steam Datagram Relay · [32] Baughman & Levine, "Cheat-Proof Playout", INFOCOM 2001.

Conexão: EOS P2P, Lobby, licença e segurança (dev.epicgames.com, onlineservices.epicgames.com); os cabeçalhos do EOS SDK 1.19.1.2 e do Steamworks v164 que vêm na UE 5.8; Steamworks SDR, networking, taxa do app; GameNetworkingSockets, ENet, netcode, libdatachannel, coturn, KCP, msquic (GitHub); Hetzner e preços de VPS 2026; Oracle Always Free; Cloudflare Realtime; WonderNetwork (ping São Paulo–Frankfurt); regras do Firewall do Windows (Microsoft); IPv6 no Brasil (Internet Society/NIC.br).

Auditoria: o código do repositório, com arquivo e linha na Parte 3.
