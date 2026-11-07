# A trilha sonora da partida (06/11/2026, a sessão do HUD)

Pedido dele (06/11): "o que eu quero é os soundtrack [...] tem que ser muito bem feito, cada coisa; por exemplo, uma do nada para e
outra começa, me deixa com pé atrás [...] pra realmente ARREPIAR". Pode gerar trilhas novas e trocar as ruins ("eu quero algo ICÔNICO,
LINDO, MUITO BEM FEITO igual era no ZH").

## O estado (06/11)

- **Feito, sem commit, esperando o teste dele.**
- **35 faixas:** as 4 facções, cada uma com o tema, a calma, a batalha, a vitória e a derrota.
  - 13 vêm do assgets dele (USA, China e GLA).
  - 22 foram geradas na ElevenLabs Music v2.5: a Rússia inteira, as calmas e 2 batalhas da China, 1 calma da GLA, 2 calmas e 2
    batalhas da USA e os 8 fins.
- **O sistema no jogo:** `URTSMusic` (`Game/Source/RTSGame/Public/RTSMusic.h`), lido de `Game/Config/RTS/Music.json`, com os assets em
  `/Game/Audio/Music/Match/<facção>/MU_<id>`.
- **O teste:** as cenas `trilha_batalha` (o tema, a calma, a batalha e a volta) e `trilha_vitoria` (o fim). O vídeo com som:
  ```
  game.ps1 view -Scenario trilha_batalha -Folder <pasta> -Record -Sound -Extra '-NewHud -RTSFaction=<facção>'
  ```
- **Gasto na ElevenLabs:** ~9.700 créditos em 23 gerações. Uma faixa de 2 min custa ~900 créditos (~410 por minuto). Sobram 24.313 no
  mês.

## Como refazer (a receita inteira)

1. **Os originais.**
   - As do assgets: o painel de cada facção (`/factions/<facção>/assets`). Ele entra, a página dá os links assinados (2 h), e
     `scratchpad/music_download.py` baixa pra `captures/musica/<facção>/`.
   - As geradas: `node Game/Tools/music/compose.mjs docs/trilha/planos/<id>.json` (a chave é a do Atelier).
     - O áudio vai pra `captures/musica/gerado/<facção>/<id>.mp3`. Os originais das 22 em uso ficam guardados em `docs/trilha/audio/<facção>/`.
     - A receita (o plano, o song-id, os créditos) vai pra `docs/trilha/receitas/<id>.json`.
     - Os planos saem de `python Game/Tools/music/plans.py`: cada trecho é um número inteiro de compassos no andamento da faixa, então a
       grade das batidas e o começo de cada trecho são conhecidos de antemão.
2. **A medida.**
   - `python Game/Tools/music/measure.py <pasta> <saída.json>`: o EBU R128 pelo ffmpeg, o andamento, o começo e o fim, a banda, o clipe.
   - O ouvido: `node Game/Tools/music/listen.mjs <saída.json> <faixas...>`. O Gemini 3.1 Pro escuta e devolve a ficha (o estilo, a
     energia, o fim, os defeitos, a cara da facção, o gancho).
     - Os temas de fim são julgados como tema curto, não como faixa.
     - **Cuidado:** os segundos que o Gemini dá nos trechos não batem com a faixa (passam da duração). Ele serve pro caráter; a
       estrutura vem da medida.
3. **O pacote.** `python Game/Tools/music/music_pack.py` faz o começo, o fim, o volume, o pico e a grade; os números estão no cabeçalho
   do script. Ele escreve os WAV em `captures/musica/pack/` e o `Game/Config/RTS/Music.json`.
4. **A Unreal.** `powershell -File Game/Tools/music/import_music.ps1` importa na vez da fila.
   - A classe é a SC_Music (o MUSIC das opções) e o barramento, o SMX_Music.
   - O codec é o RAD Audio (deixa começar no meio da faixa).
   - Fica Force Inline (a busca no meio é instantânea).

## 1. Como os RTS fazem

- **Zero Hour, pelo código** (`newcnc/GeneralsGameCode`, `Core/GameEngine/Include/Common/GameMusic.h`, `MilesAudioManager.cpp`,
  `ScriptActions.cpp`, `PlayerTemplate.cpp`):
  - cada faixa tem o nome, o arquivo, **o volume próprio** (`MusicTrack::volume`: as faixas são igualadas na mão) e uma marca `ambient`;
  - cada facção tem a **música da tela de carregar** (`LoadScreenMusic`, o tema dela) e a **da tela do placar** (`ScoreScreenMusic`);
  - na partida as faixas tocam uma atrás da outra, e a troca de faixa pelo script tem **fade-out e fade-in** (`doMusicTrackChange`, os
    dois parâmetros); a música é um fluxo contínuo que nunca para nas cenas (o comentário em `MilesAudioManager.cpp`, ~2281).
- **C&C e Red Alert** (Klepacki): a trilha toca a partida inteira como uma jukebox; o jogador escolhe e pula na tela de música (o
  Remastered tem a lista inteira). O ícone é a faixa (Hell March, Act on Instinct): poucas, fortes, reconhecíveis.
- **C&C3, Red Alert 3, Supreme Commander, Age of Empires IV:** música **adaptativa**: calma (base, economia) e combate; a troca acontece
  no fim de uma frase musical, com cruzamento, e com folga (não pula a cada tiro).
- **StarCraft II:** uma lista por raça, sem humor; vitória e derrota com tema próprio.
- **A régua técnica** (o que a indústria segue):
  - volume percebido igual entre as faixas: EBU R128, a música de jogo em −16 a −18 LUFS integrados, o pico real abaixo de −1 dBTP;
  - cruzamento de potência igual (seno/cosseno), 2 a 4 s; nunca corte seco; a entrada da faixa nova num tempo forte quando dá;
  - a música abaixa sob a voz (ducking): −6 a −8 dB, ataque ~150 ms, volta ~600 ms;
  - sem repetir: o saco embaralhado por humor, a última faixa nunca volta logo.

## 2. O sistema no jogo (`URTSMusic`, os números no topo do `RTSMusic.cpp`)

- **O começo:** uma faixa sorteada da calma abre a partida, subindo em 3 s; o tema da facção entra no sorteio como as outras (06/11, ele:
  "lembre-se que a ordem das trilhas sonoras são aleatórias, senão é fácil de enjoar").
- **O calor do combate** (a cada 0,25 s):
  - **O que soma:** o dano que o jogador vê, em frações da vida de cada unidade.
    - O que é seu pesa 1,5; o do inimigo, 1; o do aliado, 0,6.
    - O prédio pesa o dobro por fração (a vida dele é enorme).
    - A unidade perdida soma 0,5 (a sua) ou 0,3 (a que você via).
    - A superarma lançada soma 1,5.
  - **Como some:** pela constante de 10 s.
- **Os humores:**
  - **A batalha entra** com o calor acima de 1,2 por 3 s seguidos, ou acima de 2,4 de uma vez.
  - **A calma volta** com o calor abaixo de 0,3 por 30 s, e só depois de 45 s de batalha.
  - As faixas "both" servem às duas e não trocam.
- **As trocas** (nunca corte):
  - **Calma → batalha:** no próximo tempo da faixa de agora, a velha desce em 1,5 s (cosseno) e a nova entra no trecho forte dela
    (`entry_s`), cheia em 0,35 s (seno).
  - **Batalha → calma:** se faltam até 50 s, a faixa acaba sozinha. Senão, desce em 8 s a partir da próxima barra de compasso. Depois
    vêm o respiro e a calma do começo.
  - **Entre duas do mesmo humor:** o fim natural e um respiro (4 a 9 s na calma, 1 a 2 na batalha). Na batalha, a nova entra pela
    subida (`build_s`, 4 compassos antes do trecho forte), em 2,5 s.
  - **Três tocadores:** uma troca logo depois da outra nunca corta a que ainda está descendo.
- **O relógio** (achado nos testes, conferido na gravação pela forma de onda com `scratchpad/find_offset.py`):
  - **Conta pelo relógio do áudio** (`FAudioDevice::GetInterpolatedAudioClock`), não pelo do Windows. Com o processador apertado, o mixer
    atrasa e a música atrasa junto.
  - **A posição na faixa conta do 1º aviso de que ela soa** (o 1º `OnAudioPlaybackPercent`), não do Play. No começo da partida a faixa
    soa 1,5 a 6 s depois do Play, com o jogo carregando. O valor do aviso não serve: com o começo no meio ele vem errado (16,8 com a
    faixa em 23,9). Só a hora dele vale.
  - **A demora até soar é medida em cada faixa** (a média das últimas), e a troca pra batalha dispara antes por ela. A entrada da nova
    cai em cima do tempo da velha.
  - **Toda faixa toca mesmo calada** (`VirtualizationMode = PlayWhenSilent`). A Unreal descartava a que começava subindo do zero, e o
    tema e a 1ª batalha "acabavam" ao começar (o 1º teste).
- **Sem repetir:** um saco embaralhado por humor, e as que tocaram por último vão pro fim do sorteio.
  - A memória fica gravada no GameUserSettings `[RTS] MusicRecent_<facção>_<humor>`: vale depois de fechar e abrir o jogo.
  - Guarda até uma a menos que a roda inteira; a roda toda toca antes de uma voltar.
  - O sorteio da Unreal usa a semente do relógio ao abrir o jogo (o `-benchmark` dos testes fixa a semente).
- **O fim da partida:** a música desce em 2 s e, 1,2 s depois, entra o tema da vitória ou da derrota da facção; depois, o silêncio.
  - Você caiu e a partida segue (o espectador): a derrota uma vez, depois a música volta pelo combate.
- **O abaixar:**
  - a EVA falando: −7 dB (`URTSAudio::IsEvaSpeaking`);
  - a superarma saindo: −8 dB por 6 s;
  - a pausa: −4 dB;
  - desce em 150 ms e volta em 700 ms.
- **O volume:** o MUSIC das opções (a classe SC_Music) e o `gain_db` de cada faixa no Music.json (o `volume` do ZH).
  - A calma toca 3 dB abaixo da batalha (06/11: o arquivo a −17,5 e o `gain_db` −1,5, ou seja, −19 LUFS no jogo).
  - O tema que fica na calma (o da China, gravado a −16) leva −3. As "both" levam −1,5, no meio.
- **O teste à mão:** `rts.Music.Mood 1` (calma) / `2` (batalha) / `0` (pelo combate). `rts.Music.Log 1` escreve cada troca no log.
  `-RTSNoMusic` desliga.

## 3. As faixas (o que ficou, a medida e o ouvido)

Colunas: o volume antes → depois (LUFS integrado); o andamento (bpm); a entrada (o trecho forte, em s); o Gemini (cara da facção /
gancho / produção, de 1 a 10); e o conserto que o pacote fez.

| facção | id | papel | de onde | duração | LUFS | bpm | entrada | Gemini | conserto |
|---|---|---|---|---|---|---|---|---|---|
| usa | usa_last_advance | battle (era o tema) | assgets | 179 s | -12.6 → -16.0 | 120 | 22.1 | 8/7/7 |  |
| usa | usa_overwatch | calm | gerada | 112 s | -14.4 → -17.5 | 100 | 19.2 | 7/7/6 |  |
| usa | usa_dawn_patrol | calm | gerada (3ª, ref. o tema) | 102 s | -17.8 → -17.4 | 96 | 10.0 | 8/6/7 | limitador |
| usa | usa_vanguard | battle | gerada (3ª, ref. o tema) | 114 s | -12.9 → -16.0 | 128 | 7.6 | 7/6/6 |  |
| usa | usa_steel_rain | battle | gerada (3ª, ref. o tema) | 92 s | -13.7 → -16.0 | 140 | 6.9 | 8/7/7 |  |
| usa | usa_final_breach | battle | assgets | 176 s | -11.7 → -16.0 | 150 | 15.8 | 8/7/7 |  |
| usa | usa_iron_wing_horizon | battle | assgets | 177 s | -12.4 → -16.0 | 110 | 26.2 | 7/7/6 |  |
| usa | usa_strike_coordinates | battle | assgets | 177 s | -11.5 → -16.0 | 140 | 2.1 | 7/7/6 |  |
| usa | usa_victory | victory | gerada | 17 s | -14.0 → -15.9 | 100 | | 7/6/6 |  |
| usa | usa_defeat | defeat | gerada (v2) | 17 s | -20.4 → -17.0 | 72 | | 10/8/9 |  |
| china | china_jade_garrison_march | theme | assgets | 177 s | -11.7 → -16.0 | 105 | 27.4 | 8/7/7 |  |
| china | china_jade_dawn | calm | gerada (v2) | 108 s | -14.4 → -17.4 | 92 | 20.9 | 7/6/6 | silêncio e estouro no fim: cortado no silêncio |
| china | china_red_lantern | calm | gerada (v2) | 113 s | -13.1 → -17.5 | 100 | 19.2 | 6/5/6 |  |
| china | china_jade_command | battle (era both) | assgets | 178 s | -11.6 → -16.0 | 140 | 27.4 | 7/7/7 |  |
| china | china_iron_banner | battle | assgets | 180 s | -12.1 → -16.0 | 124 | 17.4 | 7/7/6 |  |
| china | china_banner_ridge | battle | assgets | 178 s | -12.0 → -16.0 | 123 (sem grade) | 2.4 | 7/6/5 |  |
| china | china_dragon_ascent | battle | gerada (4ª) | 102 s | -13.2 → -16.0 | 136 | 7.1 | 6/7/6 |  |
| china | china_iron_lotus | battle | gerada (4ª) | 100 s | -11.8 → -15.9 | 118 | 8.1 | 6/7/7 |  |
| china | china_victory | victory | gerada (v2) | 18 s | -13.1 → -16.0 | 100 | | 6/6/5 |  |
| china | china_defeat | defeat | gerada | 17 s | -22.9 → -17.0 | 72 | | 10/9/9 |  |
| gla | gla_calculated_perimeter | battle (era o tema) | assgets | 177 s | -13.2 → -16.0 | 105 | 18.3 | 8/7/6 |  |
| gla | gla_black_market | calm | gerada (4ª) | 94 s | -15.0 → -17.5 | 126 | 7.6 | 7/5/6 |  |
| gla | gla_tunnels_vanguard | calm | assgets | 119 s | -20.8 → -17.6 | 104 | 2.7 | 7/5/6 | limitador (+2,5 dBTP com o ganho) |
| gla | gla_steel_dunes | both | assgets | 177 s | -12.9 → -17.5 | 105 (sem grade) | 26.4 | 6/6/5 |  |
| gla | gla_shadow_hijaz | both | assgets | 178 s | -13.1 → -16.0 | 136 (sem grade) | 16.4 | 7/6/6 |  |
| gla | gla_sun_trenches | battle | assgets | 150 s | -12.1 → -16.0 | 120 | 15.9 | 8/7/7 |  |
| gla | gla_victory | victory | gerada | 14 s | -14.8 → -16.0 | 100 | | 7/5/6 |  |
| gla | gla_defeat | defeat | gerada | 16 s | -16.3 → -17.0 | 72 | | 10/8/9 |  |
| russia | russia_iron_directive | battle (era o tema) | gerada | 116 s | -15.5 → -16.0 | 112 | 17.1 | 10/9/8 |  |
| russia | russia_cold_line | calm | gerada | 104 s | -14.4 → -17.6 | 96 | 20.0 | 8/8/8 | silêncio e estouro no fim: cortado no silêncio |
| russia | russia_white_silence | calm | gerada | 129 s | -14.2 → -17.4 | 88 | 21.8 | 9/8/7 |  |
| russia | russia_red_tide | battle | gerada | 131 s | -12.4 → -16.0 | 132 | 14.5 | 9/8/7 |  |
| russia | russia_steel_avalanche | battle | gerada | 124 s | -12.0 → -16.0 | 136 | 14.2 | 9/8/7 |  |
| russia | russia_victory | victory | gerada | 18 s | -14.5 → -15.9 | 100 | | 10/9/9 |  |
| russia | russia_defeat | defeat | gerada (v2) | 17 s | -19.0 → -17.0 | 72 | | 7/6/6 |  |

### O que ficou de fora, e por quê

- **Do assgets:**
  - As 5 de 120 s da USA de 05/10 (Command at Dawn, Forward Secure Command, Operation Dawn Watch, Tactical Alignment, Tactical
    Superiority): ele, depois de ouvir no jogo, "muito repetidas, péssimas, me enjoa". Eram loops (67 a 94% de cópia, §4).
  - `Red_Steel_Foundations` e `Siege_of_the_Jade_Citadel` (China): sem nenhum instrumento chinês, marcha ocidental genérica (cara da facção
    4).
  - `Colossus_of_Iron` (GLA): heavy metal ocidental puro, nada do deserto (cara da facção 3).
  - `Improvised_Steel` (GLA): orquestra genérica (cara da facção 3).
  - As 3 do menu ficam no menu.
- **Das geradas (a 1ª volta):**
  - `china_jade_dawn` e `china_red_lantern` v1: saíram orquestra ocidental. "Cinematic" e "warm strings" puxaram a orquestra; a v2 pede
    os instrumentos pelo nome e proíbe a orquestra e o coro.
  - `china_victory` v1: fanfarra ocidental.
  - `russia_defeat` v1: um coro a cappella solto.
  - `usa_defeat` v1: rock.
- **A medida que mais pesou:**
  - As 4 faixas de 120 s da USA de 05/10 acabavam cortadas a todo volume (os últimos 50 ms a −15 dB): era o "uma do nada para".
  - Duas geradas, depois de 6 s caladas, voltavam com um estouro a −14 LUFS.
  - Quase todas vinham quentes, de −11,5 a −13 LUFS, com o pico passando de 0 dBTP (até +1,6) e amostras cortadas.

### O que não está bom ainda (sem passar pano)

- **O ouvido do Gemini dá 6 a 7 de 10 pras faixas antigas.** O defeito que ele repete em quase todas é o grave e médio-grave embolados;
  o pacote não mexe na equalização.
- **As melhores são as geradas da Rússia** (cara da facção 9 e 10, gancho 8 e 9) **e os 4 temas de derrota.**
- **A China ainda tem a calma fraca:** a `china_red_lantern` v2 saiu sintetizador sem instrumento chinês (6/5/6).
- **A `russia_northern_hammer` é metal genérico** (6/7/6). Ficou pela variedade da batalha; dá pra trocar.

## 4. O histórico

- **06/11, o papel dos temas.** Ele, depois do vídeo da USA nova: "top, parabéns". E também: "a música principal [...] é uma música muito
  louca, alta, animada, pra ser simplesmente uma partida em tempo normal, e ela começa sempre a primeira na partida; acho que é melhor
  trocar ela de momento".
  - Ele chamou de "Final Breach", mas a que sempre abria era a The Last Advance (o tema). A conta foi feita por ela.
  - **Os temas fortes saíram da calma e da abertura e foram pra batalha:** a The Last Advance (USA), a Iron Directive (Rússia, a mais
    forte, energia 8), a Calculated Perimeter (GLA) e a Jade Command (China, era "both", energia 7).
  - **Ficou na calma** a Jade Garrison March da China (energia 6, a marcha mais contida).
  - **A calma abaixou 1,5 dB** (3 dB abaixo da batalha), pelo `gain_db`, sem refazer o áudio nem reimportar.
  - **Provado no jogo** (`captures/game/trilha5a`, `trilha5b`): duas partidas seguidas da USA, cada uma num processo novo. A 1ª abriu
    com a Overwatch e a 2ª com a Dawn Patrol. A memória ficou no GameUserSettings (`MusicRecent_usa_calm=usa_dawn_patrol`). A The Last
    Advance não abre mais, e o fim tocou a vitória nas duas.
  - **Os vídeos finais das 4 facções** (`captures/game/trilha4_*` USA, `trilha6_*` China, GLA e Rússia; cada um com a cópia leve
    `batalha_leve.mp4`, que abre no celular). Em todas:
    - a abertura sorteada da calma;
    - a batalha no tempo, entrando no trecho forte;
    - a faixa que acaba e a próxima que sobe;
    - a volta pra calma depois de 30 s sem tiro (a "both" da GLA segue tocando, sem troca).
    - Nenhuma faixa faltando.

- **06/11, a 4ª volta (os ícones).** Ele: "eu quero algo ICÔNICO [...] nível de onde o pessoal vai botar no YouTube e comentar 'THE
  GOAT'". E logo depois, no meio da geração: "não gasta muito, ainda tenho que gerar sons de superweapons e mais unidades e construções".
  - Os planos de 5 ícones (`plans.py`, "a 4ª volta"), cada um com a assinatura da facção nos primeiros compassos. A referência foi a
    melhor peça de cada facção (o song_id da geração, sem upload).
  - **Paradas no meio:** saíram 4 (~2.650 créditos). A da Rússia (`russia_hammer_and_steel`) a API recusou antes de gerar: a referência
    tem de ter até 30 s. O plano está corrigido e guardado, sem gerar.
  - **O que entrou:**
    - `china_dragon_ascent` e `china_iron_lotus` (batalha; 46% e 41% de cópia, cara 6, gancho 7; o ouvido quase não acha instrumento
      chinês), no lugar dos dois loops da China (Iron Assembly 72%, Red Dragon 84%).
    - `gla_black_market`: foi pedida batalha e saiu calma (21% de cópia, 7/6/5). Entrou no lugar da Scrap and Shadow (64%).
  - **O que foi reprovado:** `gla_ambush_alley`, com um canto de torcida de futebol de verdade dentro ("Fenerbahçe"): fora.
  - **Saíram sem substituta** (loops): GLA Dust and Shrapnel (80%) e Scorpion Road (74%), e Rússia Northern Hammer (72%). A GLA ganhou
    a Steel Dunes nas duas rodas (20%, serve às duas).
    - Ficam 3 batalhas na GLA e 2 na Rússia. Quando sobrar crédito, a Rússia ganha a `russia_hammer_and_steel` (o plano pronto).
  - **Sobram 6.255 créditos da ElevenLabs** até 06/12, pros sons dele.

- **06/11, depois do teste dele** ("a da USA ficou muito repetida algumas, muito ruim, péssimas, me enjoa de ouvir; a primeira é top, mas
  as outras sei lá"):
  - **A medida da repetição** (`Game/Tools/music/repetition.py`):
    - Como mede: janelas de 4 s, o croma e o timbre, comparadas com tudo o que tocou antes. "Cópia" é a parte da faixa quase igual a um
      trecho anterior (cosseno ≥ 0,985).
    - O que achou: as 5 de 120 s de 05/10 eram loops. Forward Secure Command 94% de cópia (o maior trecho repetido, 112 s), Tactical
      Superiority 87%, Tactical Alignment 84%, Operation Dawn Watch 72%, Command at Dawn 67%.
    - O tema que ele aprovou (The Last Advance) tem 43%. As novas têm de ficar no nível dele ou abaixo.
  - **Saíram as 5.** Ficaram o tema e as menos repetidas (Iron Wing Horizon 32%, Strike Coordinates 33%, Final Breach 57%, Overwatch
    24%).
  - **Entraram 3 novas** (`plans.py`, "a 3ª volta"), geradas com o tema dele de referência. O `upload.mjs` sobe a faixa e devolve o
    song_id, que vai no `conditioning_ref` dos trechos.
    - As regras contra o enjoo: trechos de 8 compassos com textura própria, o motivo sempre transformado, a subida de tom antes do
      clímax e "repetitive loop" proibido.
    - Resultado: usa_vanguard (batalha, 49% de cópia, 7/6/6; a A/B sem a referência deu 51% e 7/5/6), usa_dawn_patrol (calma, 22%,
      8/6/7) e usa_steel_rain (batalha, 38%, 8/7/7).
  - **O plano que a ElevenLabs "lê" do upload** (o `extract_composition_plan`) veio errado ("synthwave"): usar só o áudio de referência.
  - **Os créditos:** o upload e as 4 gerações custaram ~3 mil. O resto que sumiu no dia foram as vozes das unidades da sessão do Elenco.
    Sobram ~10 mil até 06/12.

- **06/11:**
  - O estudo (§1).
  - O download das 26 do assgets.
  - A medida e o ouvido das 29, com o menu.
  - A 1ª gerada (`russia_red_tide`, o teste da qualidade e do custo: 909 créditos por 2:12) e as outras 22.
  - O pacote, o sistema no jogo e as cenas de teste.
  - **O 1º teste** (`captures/game/trilha1_*`) achou dois defeitos meus:
    - A faixa que sobe do zero era descartada: o tema morria em 1,7 s, e depois vinham 7 s de silêncio.
    - A posição contada pelo Windows andava 30% na frente da música.
  - **O 2º e o 3º** (`trilha2_*`, `trilha3_*`): confirmados em ordem.
    - O tema abre subindo em 3 s.
    - O ataque vira batalha em 0,3 a 0,5 s, no tempo. A faixa nova entra no trecho forte (a Steel Avalanche no coro com os metais, aos
      14,2 s).
    - A EVA abaixa a música pra 0,45 e ela volta.
    - Com 30 s sem tiro, a batalha acaba sozinha ou desce.
    - A vitória da China: a música desce em 2 s e entra a fanfarra; depois, o silêncio.
    - Nenhum corte no som gravado.
    - A única queda brusca (aos 82 s da Rússia) é o jogo travado ~13 s com a máquina cheia: a mesa de som fica sem dados e cai tudo, não
      só a música.
