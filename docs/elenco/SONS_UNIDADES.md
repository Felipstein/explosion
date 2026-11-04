# Os sons das unidades (04/11/2026)

O pedido (04/11): "precisamos gerar os unit portrait de todos e os variados sons pra cada, começamos pela USA" e "preciso entender o
que é cada uma". Cada unidade tem a seção **"8. O som"** na ficha dela (`docs/elenco/<facção>/<chave>.md`): o temperamento da voz e as
falas. Aqui ficam os momentos em que cada som toca, as decisões e o jeito de gerar.

## As decisões dele
- **UM padrão de voz no jogo inteiro** (04/11, depois de eu pôr chiado na EVA, ruído de obra no Dozer e rádio limpo no M1: "MANTÉM
  PADRÃO NO JOGO, senão eu vou escutar vozes com diferentes tipos de efeitos e vou ficar com dor de cabeça"). Como os AAA fazem:
  toda voz é gravada LIMPA e seca (a descrição da voz nunca fala de rádio, chiado, ruído, sala ou efeito: só a pessoa e o jeito
  de falar); o "rádio" é UM tratamento só, o mesmo pra todas as vozes, aplicado pelo jogo (o submix de voz da Unreal) e, até o
  sistema de som existir, pela mesma ferramenta local pra todas. As aprovadas ficam como estão: o M1 e o Dozer ("não vai tirar
  ruído do Dozer, porque eu já tinha aprovado, porque não tinha ruído"); a 1ª EVA (pedida com chiado) saiu e a limpa foi aprovada.
- **Selecionar = só a voz** (04/11: "acho que só a voz da unidade na hora que seleciona mesmo"). Nada de efeito de máquina por baixo:
  o teste com a turbina do M1 saiu (`captures/audio/04_m1_selecao_montada/`, as versões A, B e C).
- **Gerar só 1** de cada (voz, fala, efeito): as variações saíam iguais e custam (`feedback-som-gerar-so-1`).
- **Selecionar construção = som baixo, curto e abafado** (04/11, a War Factory com a sirene e o clang: "tomei um susto"): a
  oficina ouvida através da parede, sem sirene, sem alarme, sem pancada de metal; toca toda vez que se clica no prédio.
- O ouvido é dele: o Claude não ouve; descreve o que é cada som e onde toca, e ele escolhe no Atelier (a mesa de ouvir).

## Os momentos de cada unidade (o M1 de exemplo)
| # | Som | Quando toca |
|---|---|---|
| **A voz** (o comandante, o temperamento da ficha) | | |
| 1 | Fala de seleção | você clica nela (sorteia 1 de 4) |
| 2 | Fala de movimento | você manda andar |
| 3 | Fala de ataque | você manda atacar |
| 4 | Fala sob fogo | ela toma tiro (no máximo 1 a cada ~10 s) |
| 5 | Fala de pronto | sai da fábrica |
| **O corpo** (no mundo, perto da câmera) | | |
| 6 | Motor parado | loop, parada |
| 7 | Motor andando | loop, sobe com a velocidade |
| 8 | Esteiras / rodas | loop, andando |
| 9 | Giro da torre | enquanto a torre vira |
| **A arma** | | |
| 10 | Tiro | cada tiro |
| 11 | Recarga | depois do tiro |
| 12 | Impacto no alvo | o acerto (ou o ricochete) |
| **A morte** | | |
| 13 | Morte | destruída |
| 14 | Carcaça queimando | loop, depois |

## A seleção das construções (a receita que deu certo, 04/11)
A War Factory levou 6 voltas; a aprovada ("tá pode ser, tá no rumo") saiu assim, e o Command Center e a Barracks já nasceram nela:
1. **UM conceito por pedido: o ambiente do prédio**, o que mais existe gravado ("Heavy industrial factory floor ambience, large machines
   running steadily in a big hall, deep mechanical hum and rhythmic machinery, heard from a distance. Calm and soft. Professionally
   recorded field recording."), `duration: 2.0`, `influence: 0.5`, 1 variação.
2. **O envelope aqui** (`Game/Tools/sound_envelope.py`, no Blender): entra macio em 0,35 s, segura até 0,7 s, some até o silêncio em
   2 s, pico em −16 dBFS. O gerador entrega no volume cheio, começando e acabando de repente ("começa já alto e termina alto").
3. **Detalhe pedido por ele vira mistura local**, uma fonte por pedido: a Barracks = 3 passadas da marcha + no tempo da 4ª o "alto!" da
   esquadra (a cereja: "só 3 e com uma cereja no bolo no final pra acabar bem"), 3 dB acima
   (`captures/audio/18_barracks_selecao_3_passos_e_o_alto/mix_barracks.py`).

O que deu errado (não repetir): a cena com 3 a 4 fontes num pedido só (veio a sirene, a pancada, ou só uma delas: "só tem um som de uma
panela batendo"); "ratchet"/"chain rattle"/"beep" (racham em cima: o "grilo", 5 a 7 kHz); "no siren, no alarm" (trouxe); o conserto no
espectro de um som que ele já odiou (o filtro tira o grilo, mas o resto fica sem graça). Antes de mandar: medir as faixas por tempo
(a faixa de 5 a 22 kHz nunca acima do resto; o começo e o fim sem degrau).

## A interface (o HUD, igual nas 4 facções)
O ZH (`MiscAudio.ini`) tem poucos sons de interface, e é o certo (simples): **o clique** (`GUIClickSound`, todo botão), **o "não pode"**
(`NoCanDoSound`: sem dinheiro, lugar inválido, sem pré-requisito), **o dinheiro entrando e saindo** (`MoneyDepositSound`/
`MoneyWithdrawSound`, o tique do contador) e **o prédio desligando e religando** (`BuildingDisabled`/`BuildingEnabled`: sem energia).
O nosso: o clique = o botão de pressão do console militar (o HUD é o console), seco, 90 ms, −14 dBFS (abaixo das vozes); recorte do
transiente local (`Game/Tools/sound_click.py`); o 1º teste em `captures/audio/16_hud_o_clique_o_botao_do_console/`.

## Os avisos da partida (o estudo do ZH, 04/11)
O ZH decide tudo num arquivo (`Eva.ini`, https://raw.githubusercontent.com/TheSuperHackers/GeneralsGamePatch/main/Patch104pZH/GameFilesOriginalZH/Data/INI/Eva.ini):
cada aviso tem **prioridade** (o maior fala, só um por vez), **o intervalo mínimo** entre dois iguais e **a validade** (se não falou em
5 s, some). A voz da base da USA no "base sob ataque" é a do Ranger; o resto é o anunciante. Os nossos, com os números dele:
| Aviso | Prioridade | Intervalo | Como |
|---|---|---|---|
| Prédio perdido | 8 | 30 s | o ZH da USA não fala (o "base sob ataque" já cobriu) |
| Base sob ataque | 7 | 30 s | voz + o pulso no minimapa + Espaço leva lá (já existe no jogo: "Unit under attack (Space to go there)") |
| Aliado sob ataque | 6 | 30 s | voz |
| Superarma inimiga detectada / lançada; a nossa pronta / lançada | 4–5 | 30 s | voz (as 4 superarmas, `docs/SUPERARMAS.md`) |
| General promovido (os poderes) | 4 | — | voz (quando os poderes entrarem) |
| Prédio sendo capturado / capturado | 4 | — | voz |
| Dinheiro insuficiente | 3 | a cada clique | voz curta + o "não pode" |
| Melhoria pronta | 3 | — | voz |
| Pouca energia | 2 | 120 s | voz + o prédio desligando |
| Unidade perdida | 1 | 15 s | voz |
| Unidade sob ataque | — | — | **sem voz**: só o tom do radar (`RadarNotifyUnitUnderAttack`) e o pulso no minimapa |
| Prédio sob ataque | — | — | **sem voz**: o tom do radar mais forte (`RadarNotifyStructureUnderAttack`) |
"Unidade pronta" e "obra pronta" são a fala de pronto da própria unidade ou do prédio (já nas fichas).

### A EVA (o nome do C&C pro anunciante; o padrão escolhido em 04/11: "escolhe um padrão pra USA e começa")
- **USA: a operadora da base (APROVADA 04/11, "USA EVA" na biblioteca).** Mulher, humana, voz limpa: calma, seca, frases curtas, nunca grita. Mulher pela
  tradição da EVA do C&C; humana porque o tom
  do jogo é sério (nada de robô). A mesma voz fala as falas do Command Center: a base tem uma voz só.
- O pedido da voz aprovada (1 candidata): "American female military operations officer, early thirties, calm, crisp and clipped,
  authoritative and never shouting, precise diction, short measured phrases; she reports alerts to the base commander. General American
  accent. Clean, dry studio recording." (`captures/audio/20_usa_a_eva_limpa_a_operadora_da_base/`; a 1ª, com o chiado pedido, reprovada:
  `19_`). As falas: `21_usa_as_falas_da_eva/` (`kind: "speech"`, `voice: "USA EVA"`, 1 de cada).
- As falas da USA: "Our base is under attack." / "Allied base under attack." / "Unit lost." / "Insufficient funds." / "Low power." /
  "Upgrade complete."; as superarmas (`docs/SUPERARMAS.md` §3.1): a nossa "Global Strike ready." e "Launch confirmed."; as do inimigo
  "Enemy [nome] detected." e "[nome] launched!", SÓ quando os nomes fecharem (pedido dele). Depois, quando existirem:
  o general promovido e a captura de prédio.
- As outras facções: cada uma com a sua EVA (o ZH também: o `Eva.ini` tem um som por facção em cada aviso).
- **As outras 3 (04/11).** A 1ª proposta (as 4 mulheres, "porque as unidades são todas homens") foi REPROVADA: "por que todas são
  mulheres? a USA eu entendi, mas o restante podia ser homem"; "e quem disse que todas as unidades são homens?" (a premissa era
  minha e errada: a ficha do Patriot pede uma operadora). A voz de cada unidade sai da ficha dela, homem ou mulher. As 3 mulheres:
  `captures/audio/24_`, `25_`, `26_`. Agora, homens, a facção na pessoa, voz limpa, um tratamento de som só pro jogo:
  - **China: APROVADA A MULHER** ("China eu fico com a da mulher"; "China EVA" na biblioteca; a descrição dela na 1ª proposta acima
    não existe mais no texto: "Chinese female army staff officer, mid-thirties, composed and solemn, with the poised, evenly
    weighted diction of a state news anchor; full, rounded mid-range voice, steady unhurried pace, each phrase ending with calm
    finality, serious and never shouting. Light Mandarin accent, fluent English. Clean, dry studio recording." (`24_`); as falas
    em `31_`). O homem, reprovado: o locutor do Estado (fim dos 40, barítono, solene): "Chinese male
    state broadcaster serving as the army command voice, late forties, deep resonant baritone, solemn and ceremonial, perfectly even
    diction, slow uniform cadence, every phrase ending with calm finality; he speaks for the collective, proud and unshakable, never
    shouting. Light Mandarin accent, fluent English. Clean, dry studio recording." (`27_`)
  - **Rússia: APROVADO o oficial de serviço do estado-maior** ("Russia EVA" na biblioteca; as falas em `32_`) (meio dos 50, baixo, frio, o mais lento, lê como livro de registro): "Russian
    male general staff duty officer, mid-fifties, low bass voice, cold, level and unhurried; he reads alerts like entries in a
    logbook, few words, flat matter-of-fact intonation, a brief pause before the key word, unshakable, never shouting. Light Russian
    accent, fluent English. Clean, dry studio recording." (`28_`)
  - **GLA: o mais velho da célula (a 1ª, reprovada pelas palavras: "quanto brother no GLA, cadê o warrior, cadê as outras palavras
    que dão peso"; a 2ª em `30_`, as falas com lar, guerreiros, fiéis, "A warrior has fallen.", sem "brother").** O pedido dele: "tem que ter jogo de cintura, não é unidades, guerra, são GUERREIROS, FIÉIS,
    AMIGOS, não é base, é LAR, tem que ter um peso". Fala com os seus como pai; malandro e firme; as falas dizem lar, irmãos,
    guerreiros ("They are attacking our home!", "Our brothers are under attack!", "We lost a brother.", "We do not have the money,
    brother.", "Upgrade ready, our warriors grow stronger.", "Our home is ready.", "We are here to stay."); sem "pouca energia" (a
    GLA não tem usina): "Arab male resistance elder, early fifties, deep, warm and weathered voice with natural gravel, heavy with
    conviction; street-smart, adaptable and unflappable, a hint of wry humor even under fire; he speaks to his fighters like a
    father to his sons, intimate and loyal, never shouting, never fanatical. Light Arabic accent, fluent English. Clean, dry studio
    recording." (`29_`)
  - **GLA, a 3ª ideia (04/11): uma pessoa da célula por aviso.** "O GLA pode ter vários tipos diferentes de voz; o icônico C&C ZH é
    gostoso de ouvir porque o 'our base is under attack' era numa voz realmente sentindo dor, mas já diferente das outras; tenta
    cada um uma voz um pouco diferente, com diferentes pesos, sentindo peso nas frases" (no ZH o "base sob ataque" da GLA é a voz
    do Rebel, `Eva.ini`). A GLA é uma rede de gente, não uma sala: base sob ataque = o ferido (fala com dor; `33_`), aliado = o
    vigia no telhado (`34_`), unidade perdida = o amigo veterano de luto ("A warrior has fallen."; `35_`), sem dinheiro = o
    contrabandista malandro (`36_`), melhoria = o armeiro velho e orgulhoso (`37_`); o mais velho da célula (`30_`) com as falas do
    Command Center. Cada amostra: a fala clara primeiro e mais 2–3 no mesmo peso (as variações do aviso). As descrições e as
    amostras inteiras: `captures/audio/<pasta>/atelier.json` e o script em `docs/elenco/sons/eva_gla_voices_req.py`.
  - Antes de gerar: a amostra de cada uma leva TODOS os avisos e as falas do Command Center (recortar da amostra), e as fichas que
    pedem efeito embutido ficam limpas (o "eco do alto-falante" do CC da China, o "chiado do rádio velho" do CC da GLA).

## No jogo (04/11, a 1ª versão: "os sons aprovados pode colocar no jogo")
- **O caminho:** `Game/Tools/audio_pack.py` (no Blender) pega os sons APROVADOS (os recortes e as amostras em `captures/audio/`, os
  aprovados em `docs/aprovados/sons/`), nivela as falas no mesmo padrão (a fala em −20 dBFS de RMS, o pico até −1) e grava os WAV em
  `Game/GameAudio/` (fora do Content: o `Content/Audio` é o som do menu, da sessão das Telas) e o mapa `Game/Config/RTS/Sounds.json`.
  Mudou um som: rodar o `audio_pack.py` de novo (nunca editar o JSON à mão).
- **Quem toca:** o `URTSAudio` (`Game/Source/RTSGame/Public/RTSAudio.h`, `Private/RTSAudio.cpp`), um subsistema do mundo. Lê os WAV
  e toca num `USoundWaveProcedural` (sem .uasset), cada som na classe de volume do menu (o clique no `SC_UI`, as falas e a EVA no
  `SC_Voice`, o prédio no `SC_Effects`: as opções de volume valem). Os ganchos: o `ARTSPlayerController::OnButton` (o clique; produzir
  ou construir sem dinheiro chama a EVA), o clique que seleciona uma unidade sua (a voz, mesmo já selecionada) e as ordens do
  `URTSMatchSubsystem` (andar, atacar, construir, consertar: a fala do capitão, o tipo que mais tem no grupo).
- **O que toca:** o clique do HUD; a seleção = a voz da unidade (o M1, o Dozer) ou o som do prédio (a War Factory, a Barracks); as
  ordens; a unidade nova saindo da fábrica (a fala de pronto); a EVA da facção do jogador nos avisos do minimapa (a base sob
  ataque, a unidade perdida), na falta de energia e na falta de dinheiro. As regras do ZH (`Eva.ini`): um aviso por vez, a maior
  prioridade primeiro, o intervalo entre dois iguais (base 30 s, unidade 15 s, energia 120 s, dinheiro 1,5 s) e a validade na fila
  (5 s). A EVA fala por cima: as falas das unidades abaixam 6 dB enquanto ela fala. Uma fala de unidade por vez (a nova corta a velha).
- **Testado (04/11 noite):** a cena `som_teste_usa` (a voz ao selecionar o Dozer e o M1, o som da War Factory e da Barracks, a
  "pouca energia" na usina derrubada sem dizer "unidade perdida" pelo prédio, a "unidade perdida" no Dozer morto, a fala de pronto do M1
  treinado) e a partida automática inteira (`game.ps1 view -Folder som_base`, 400 s, 0 falhas: o clique 10×, as falas de andar e
  atacar do M1 8×, a obra do Dozer 3×, a "base sob ataque" 2× com 34 s entre elas, a "unidade perdida" 6× sempre ≥15 s, a "pouca
  energia" 1×).
- **O teste pelo log:** cada som que toca escreve `RTS som: <arquivo>` no log do jogo. `rts.Audio 0` desliga, `rts.AudioVolume` muda
  o volume geral.
- **Falta:** o "aliado sob ataque" (o jogo ainda não avisa o ataque ao aliado), a "melhoria pronta" (ainda não há melhorias), as
  superarmas (os nomes), o tom do radar sem voz (unidade e prédio sob ataque), as vozes das outras unidades e prédios, e os sons do
  corpo (motor, esteira, tiro, morte), que pedem o som no mundo (posicional). **No pacote:** a pasta `Game/GameAudio` tem de ir junto
  (o `RuntimeDependencies` no `RTSGame.Build.cs`, na hora da versão).

## O que existe
- **M1:** a voz "USA M1 Commander" (a candidata 1, guardada na biblioteca da ElevenLabs) e as 20 falas
  (`captures/audio/03_m1_as_falas/`); o efeito da turbina (`01_m1_selecao_a_turbina/`, não usado na seleção).

## Como gerar (o Atelier, serviço 127.0.0.1:8140)
- a voz: `kind: "voice"`, a descrição do temperamento da ficha, `models: {"vdesign": 1}`, a frase de amostra com as falas;
  guardar com `POST /api/voice/save`;
- **as falas saem da AMOSTRA da voz** (04/11: "por que você gerou uma por uma de novo?"): a frase de amostra do Voice Design leva
  TODAS as falas daquela voz (até 1000 caracteres); aprovada a voz, as falas são recortadas da própria amostra nas pausas
  (`captures/audio/23_usa_eva_falas_da_amostra_aprovada/cut_eva.py`). O que faltar: UM pedido `kind: "speech"` com todas as
  que faltam num texto só, e recortar. NUNCA 1 pedido por fala: gasta e o eleven_v3 em frase curta muda a voz (3 de 6 da EVA
  vieram com chiado: o fundo em −39 dB contra −88 da amostra);
- os efeitos: `kind: "sound"`, `variations: 1`, a duração e o loop quando é motor.
- **O prompt do efeito** (04/11, 2 erros na War Factory, o Felipe: "a ElevenLabs consegue, você que não está sabendo pedir"; o guia
  oficial https://elevenlabs.io/docs/help-center/product/core-capabilities/sound-effects/how-do-i-prompt-for-sound-effects.md):
  - UM som por pedido, curto e concreto ("hydraulic press stamping a steel plate"); a cena com várias fontes (a oficina inteira)
    sai errada: gerar cada camada separada e misturar depois (de graça, no Blender, o `aud`);
  - os termos de gravação e de áudio: "professionally recorded foley", "close-mic", "dry", impact, whoosh;
  - NUNCA "no X" / "without X": ele lê a palavra (a sirene e o grilo vieram daí);
  - o influence alto (~0,7) pra seguir o pedido à letra.
- **Consertar antes de gerar de novo** (04/11): quando um pedaço está bom e só uma camada estraga (o "grilo" da War Factory 1 era um
  zumbido de tom fixo, 446 Hz e harmônicos), tirar a camada aqui no espectro (o script do conserto: medir as faixas, achar os
  picos, baixar só eles; `captures/audio/10_war_factory_selecao_sem_grilo/`), sem gastar outra geração.
A ferramenta MCP antiga só conhece `image` e `sound`; a voz e a fala vão direto no `POST /api/requests` do serviço.
