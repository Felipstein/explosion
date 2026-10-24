# O jogo: como ele funciona por dentro (proposta de 07/10/2026)

Resposta às perguntas do Felipe de 07/10: como funciona um RTS desses, o menu,
a entrada na partida, a rede, o skirmish, como os mundos carregam e onde cada
jogador começa. **Nada aqui é código ainda; é o plano.**

**Como foi feito:**
- seis pesquisas em paralelo:
  - o código do Zero Hour que a EA liberou;
  - a rede dos RTS modernos;
  - a arquitetura na Unreal 5.8;
  - menu e fluxo de telas;
  - mundos e posições iniciais;
  - o que já temos no repositório;
- um arquiteto juntou tudo;
- dois revisores tentaram derrubar cada afirmação: um de rede e engine, outro de escopo e jogabilidade.

As afirmações sobre a Unreal foram conferidas no fonte da 5.8 instalada. Os relatórios brutos ficaram fora do git.

**Legenda:**
- **[NC]**: não confirmado;
- **[E]**: estimativa a medir.

**Regra do código do Zero Hour (clean-room):** o código da EA (GPL v3, `github.com/electronicarts/CnC_Generals_Zero_Hour`) e o do TheSuperHackers servem só pra **ler e entender** como o jogo funcionava e quais números usava. Nada é copiado nem traduzido pro nosso código. O EULA da Unreal proíbe misturar a engine com código GPL, e o Vanguard já seguia essa regra (`newcnc/CLAUDE.md` §3).

---

## Parte 1. As respostas, em termos do que você vai ver

### 1. Como funciona um jogo desse

O jogo tem duas metades que não se misturam.

**O juiz (a simulação):**
- É um programa pequeno e exato que guarda tudo o que importa pra partida:
  - onde cada unidade está;
  - quanta vida tem;
  - quem acertou quem;
  - o dinheiro;
  - o que está sendo construído.
- Ele anda em passos fixos, 30 por segundo.
- Todo PC da partida roda **o mesmo juiz, com os mesmos cliques, e chega no mesmo resultado, bit por bit**.
- Assim o Zero Hour, o Age of Empires, o StarCraft II e o Beyond All Reason funcionam.

**O artista (a Unreal):**
- Desenha o que o juiz decidiu: o tanque andando suave entre um passo e outro, a explosão do BlastLab, a fumaça, a luz, o som.
- O desenho nunca volta pro juiz.
- Por isso cada PC pode desenhar no seu preset (baixa até ultra) sem atrapalhar ninguém. A explosão linda é só dele.

**Por que assim:**
- Na rede só viajam **os cliques** (poucos kB/s), nunca a posição de 2000 unidades.
- O tamanho do exército não fica limitado pela internet.
- O replay sai de graça: é a lista de cliques.

### 2. O menu principal

**O que você vê:** uma cena 3D bonita do jogo atrás dos botões, como o fundo do ZH (o "ShellMap"). Lá ele era uma partida de verdade rodando atrás do menu.
- **Começamos com uma cena fixa.** A batalha ao vivo vem quando o jogo existir; aí ela pode ser um replay rodando.
- **Botões:**
  - Skirmish;
  - Multiplayer;
  - Replays;
  - Opções;
  - Sair.
- Campanha e Criador de mundos entram quando existirem.
- **Opções pro jogador:** preset (Auto, Baixa, Média, Alta, Ultra), resolução, limite de quadros, volumes, teclas e idioma. O resto fica na gaveta de debug.
- **O "Auto" mede a nossa própria cena.** O detector da Unreal mede contra uma placa de 2012 e jogaria a 1660 no máximo (`GPUBenchmark.cpp:442-450`, cortes 18/42/115 em `BaseScalability.ini:24`).
- **A 1ª explosão não trava.** Os shaders do jogo compilam antes da partida, como o BlastLab já faz na abertura (de 140 para 0 ms, `docs/STATUS.md`). No editor, o BlastLab mediu que o shader só compila no **primeiro desenho** (`BlastLabSimulation.cpp:866-868`). Então o aquecimento desenha cada efeito invisível na frente da câmera, atrás da tela de carregamento. No jogo empacotado, o pré-cache de PSO da Unreal adianta parte disso quando o material carrega (`PostLoad`); quanto ele cobre, a gente mede.

### 3. Entrar no jogo

```
Menu → Configurar partida / Lobby → Carregando → 3-2-1 → PARTIDA → Resultado → (replay salvo) → Menu
```

**Lobby:**
- mapa (com o minimapa e as bases clicáveis);
- por vaga: humano, IA (fácil, média ou brutal), aberta ou fechada;
- facção, cor e time;
- dinheiro inicial (v4, 22/10: feito, de 10 000 a 50 000 de 5 000 em 5 000, no menu e na sala; vai nas regras da montagem; a IA usa o banco grande em mais fábricas);
- regras (ex.: sem superarmas).

Enquanto vocês escolhem, o mapa já começa a carregar ou a ser gerado.

**Carregando:**
- uma barra por jogador, na cor dele, como no ZH;
- só sai quando **todos** chegam a 100%;
- se alguém some por 60 s, aparece "Fulano carregando" e dá pra expulsar.

**3-2-1:** todos começam no mesmo passo. Você nasce com o Centro de Comando pronto e os dozers em volta, com a câmera na sua base.

**Esc na partida:**
- no skirmish, pausa;
- no multiplayer não pausa, e o botão vira "Render-se".

**Fim:** tela de resultado com as estatísticas, e o replay é salvo sozinho.

### 4. A rede multiplayer

**Quem hospeda:**
- um dos jogadores é o "anfitrião" e repassa os cliques;
- não precisamos de servidor nosso pra jogar, e isso é de propósito:
  - o online do ZH morreu com o fim do GameSpy em 2014;
  - o do Stormgate parou em abril de 2026, quando a empresa que hospedava saiu do ramo.
- **O tráfego passa pelo relay da Steam**, que esconde o IP de cada um e cifra.
- Também dá pra jogar em LAN e IP direto.

**O clique:**
- **Na hora:** o marcador no chão, a voz da unidade e a seleção.
- **A unidade começa a andar ~0,15–0,25 s depois [E],** o tempo de o clique chegar em todos.
- O Age of Empires mediu que até 250 ms ninguém nota (Bettner & Terrano, GDC 2001).
- **No 1º marco medimos do clique até a imagem**, sem chute.

**Quando alguém trava ou cai:**
- O jogo mostra **quem** está atrasando. O ZH desacelerava todo mundo sem dizer quem era.
- Se alguém cai: tela de espera de até 60 s e votação pra seguir. Nas partidas em time, o exército passa pro aliado.

**Se os PCs discordarem (desync):**
- Cada PC confere uma "impressão digital" do juiz a cada passo.
- Se uma diverge, a partida para com um relatório dizendo **onde e quando** divergiu.
- O ZH só conferia a cada 3,3 s e o desync era o pesadelo da comunidade.

**Replays e observadores:** saem da lista de cliques.

### 5. O skirmish

O skirmish é **a mesma partida do multiplayer**, só que os outros jogadores são IA e a "rede" fica dentro do próprio PC. É um caminho de código só: tudo que testamos no skirmish vale pro online.

**A IA é um jogador virtual:**
- olha o estado do jogo e dá **ordens como um humano**: constrói, treina, ataca;
- não tem atalho nem visão extra;
- as dificuldades vêm de bônus declarados, como "rico/pobre" no ZH.

**Por quê:**
- ela roda num PC só, sem custar processamento nos outros;
- não vira fonte de desync;
- entra no replay como qualquer jogador;
- funciona igual no multiplayer com IA.

**Bônus:** IA contra IA sem tela vira teste automático de balanceamento e de desync.

### 6. Como os mundos carregam

**Mapas oficiais:**
- feitos no WorldForge em qualidade real (Landscape + Nanite), ajustados à mão e empacotados no jogo;
- abrem como um mapa normal da Unreal;
- se um amigo não tem o mapa, o anfitrião manda.

**Mapa aleatório:**
- cada PC gera o mapa pela **semente** em ~1 s, durante o lobby;
- os PCs conferem a impressão digital;
- se não bater, o anfitrião manda o mapa lógico (~0,4 MB, medido nos mapas salvos);
- o OpenRA faz exatamente assim.

**O que você precisa saber:**
- **A Unreal não deixa criar Landscape nem Nanite de chão com o jogo rodando.** Só no editor (`#if WITH_EDITOR` em `LandscapeProxy.h`; `StaticMesh.cpp:8923`).
- Então **hoje o chão do mapa aleatório sai um pouco abaixo do oficial.** Penhascos, pedras, árvores e prédios ficam iguais, porque são peças prontas.
- A "vista ao vivo real" do WorldForge (`docs/WORLDFORGE.md` §11) já usa esse caminho de jogo. Dá pra medir a diferença lado a lado **agora**, antes de decidir qualquer coisa.

### 7. Onde cada um começa, e como o gerador decide

O gerador passa a receber **os times**, não só o número de jogadores. A simetria sai daí:

| Partida | Como as bases ficam | Justa por construção? |
|---|---|---|
| 1v1, 2v2, 3v3, 4v4 | mapa girado 180°, aliados lado a lado (como os 2v2 do StarCraft II) | sim |
| 4 jogadores cada um por si, ou 4 times | girado 90° | sim |
| 8 cada um por si | 4 giros + espelho | sim |
| 3, 5 ou 6 | girado em 120°/72°/60° numa área redonda | não: só aceito se as medidas baterem |

**O mapa só é aceito se passar nas medidas da literatura** (Togelius et al. 2010; Liapis, Yannakakis & Togelius 2013):
- toda base e todo recurso alcançáveis por todos (no mapa de ilhas: por mar e ar, igual pra todos);
- espaço de construção suficiente em volta da base;
- recursos próprios à mesma distância pra todos e "seguros" (mais perto de você que do inimigo);
- distância de rush dentro de uma janela (depois, em segundos de tanque, como o StarCraft II mede);
- gargalos com pelo menos 2 tanques de largura.

**Nota pra escolher a semente:** os mapas aprovados ganham nota e a grade de "9 sementes" do WorldForge mostra os melhores primeiro.

**No lobby:**
- **Sorteio**, pela regra do ZH:
  - o 1º jogador cai num lugar qualquer;
  - cada time novo vai pro ponto mais longe;
  - o aliado vai pro ponto mais perto do seu time.
- **Ou escolha no minimapa**, se o anfitrião deixar.
- No mapa aleatório o anfitrião pode esconder a prévia.

**Hoje:** o WorldForge já faz bases simétricas exatas pra 2 e 4 jogadores e só confere a ligação por terra (`WorldGenerator.cpp:266-285`).

### 8. "Como os RTS fazem aquelas nuvens volumétricas?"

**As nuvens do céu:**
- É um volume de ruído 3D moldado por um "mapa do clima". Pra cada pixel, a placa caminha um raio dentro dele somando densidade e luz (Schneider/Guerrilla, Horizon Zero Dawn, SIGGRAPH 2015/2017; Hillaire, SIGGRAPH 2020, que é o da Unreal).
- **O truque é não calcular tudo a cada quadro:**
  - a Guerrilla atualiza 1 pixel de cada bloco 4×4 por quadro e reconstrói o resto com os quadros anteriores, ~2 ms no PS4;
  - a Unreal traça em resolução reduzida e reconstrói.
- **Na câmera de cima elas saem quase de graça.** O raio bate no chão antes e para cedo (`VolumetricCloud.usf`).
- O que você vê de cima é **a sombra das nuvens correndo pelo campo**. A nuvem cheia aparece na câmera deitada, a cinematográfica.
- Nuvem baixa entre a câmera e as unidades é defeito (cobre a batalha).

**As "nuvens" de fumaça e explosão:**
- São a mesma técnica, o volume marchado, e são **a parte cara de verdade**.
- Hoje, uma morte de tanque custa ~12 ms de volume mais ~7 ms de sombras e luzes na 1660 (`docs/STATUS.md`).
- Os jogos resolvem com "herói × multidão": poucas em detalhe total, as outras em versão barata. O BlastLab já tem o limite de 3 em detalhe total.

---

## Parte 2. Decisões suas (o resto é técnico e fica comigo)

1. **Onde nasce o jogo.** Achei o **Vanguard** (`C:\Users\luisf\newcnc`, julho de 2026).
   - **O que ele tem:** MVP com economia, construção, dozer, energia, defesa, HUD, menu simples (Começar/Sair) e torre girando. São 84 commits e 32 decisões escritas (ADRs).
   - **Onde ele fica longe:** só China, só single-player, nações reais.
   - **O problema de rede:** os veículos andam pelo movimento de personagem da Unreal, com navegação e desvio de multidão (`UnitActor.cpp:150`, `UnitAIController.cpp:8`). Isso **não dá o mesmo resultado em dois PCs**, então o multiplayer no estilo do ZH exige refazer o coração dele.
   - **Recomendo um projeto novo pro jogo, com o juiz exato desde o 1º dia.** Do Vanguard vêm as regras, os números de balanceamento tirados do ZH e as decisões de controle, câmera e HUD. Do BlastLab vêm os efeitos; do WorldForge, os mapas.
   - **A alternativa:** continuar o Vanguard trocando o coração. O trabalho de refazer a simulação é o mesmo; sobra o resto dele, que é pequeno (~13 mil linhas no total; o menu tem só Começar e Sair).
2. **Espaço em disco.** O C: tem ~21 GB livres. Um projeto novo, mais compilar e empacotar o jogo, aperta.
   - O grosso está em `captures/` (61 GB) e no cache de shaders da Unreal (40 GB).
   - Recomendo um SSD a mais, ou mover `captures/` pra outro disco.
   - Não apago nada sem você mandar.
3. **Tamanho das partidas na 1ª versão.** Recomendo 1v1, 2v2 e 4 cada um por si, que o gerador já faz de forma justa. 3v3, 4v4 e 8 jogadores (o ZH aceitava 8) vêm depois. O código nasce pronto pra 8.

**Mais pra frente, sem pressa:**
- loja (Steam, Epic ou as duas);
- campanha;
- guerra naval já na 1ª versão ou depois;
- mapa aleatório no ranqueado.

Nenhuma delas trava o começo.

**Regra que vou seguir no jogo (do seu pedido de hoje):**
- **beleza máxima no Ultra**, mesmo travando na 1660;
- **60 qps no preset que a placa do jogador escolher**, como os jogos modernos;
- quando gráfico e jogabilidade brigarem (quadros, atraso do clique, número de unidades), **a jogabilidade ganha** e cortamos custo visual medido.

---

## Parte 3. A arquitetura técnica

### 3.1 As camadas

| Camada | O que faz | Como | Já temos |
|---|---|---|---|
| **Sim (o juiz)** | unidades, armas, dano, economia, construção, caminhos, névoa lógica, regras, vitória | módulo C++ só com `Core`, sem UObject; ponto fixo; RNG próprio; 30 passos/s; entrada **só** por comandos | o modelo é o núcleo `WorldForge/Source/WorldForge/*/Generation`, que já só usa Core |
| **Ponte** | relógio da sim (acumulador, Fiedler, "Fix Your Timestep!"), sim numa thread própria, **3 estados guardados** (anterior, atual, sendo escrito) com troca atômica, mais eventos (tiro, impacto, morte) | `UTickableWorldSubsystem` | — |
| **Apresentação** | interpola entre o anterior e o atual; unidades instanciadas, efeitos, áudio, névoa desenhada, câmera | ISM/Nanite; Niagara; o BlastLab dispara pelo evento | câmera (`BlastLabCamera`), explosões, fumaça do campo, destroços |
| **Rede** | transporte, turnos, atraso de comando, impressão digital | protocolo próprio atrás de uma interface | — |
| **Front-end** | menu, lobby, carregamento, opções, replays | `UGameInstanceSubsystem`, CommonUI, MoviePlayer | painéis Slate do BlastLab/WorldForge viram a gaveta de debug |
| **Conteúdo** | mapas, dados das unidades | WorldForge; dados de design convertidos pra ponto fixo na carga | `Generation/`, `WorldForgeExport.cpp` |
| **IA** | jogador virtual que só emite comandos | thread própria; modo síncrono pros testes | — |

**Descartadas, com o motivo verificado:**
- **Unidades como Actors replicados pela Unreal.** A banda cresce com o número de unidades. 2000 unidades × ~10 B × 10 Hz dão ~200 kB/s por cliente, o dobro do padrão `MaxClientRate=100000`. O Iris vem desligado na 5.8 (`IrisConfig.cpp:15`) e não muda o modelo.
- **Mass Entity como sim.** Usa float, depende de Engine/CoreUObject e não garante ordem. Fica pro desenho e pro LOD de multidão.
- **Chaos no movimento e no dano.** Só tem determinismo "melhorado" e local, em float e multithread (`PhysicsSettingsCore.h:47`). Destroços e cascas ficam no visual.

### 3.2 O que tem de existir desde o 1º dia (senão é reescrever depois)

1. **Sim sem UObject.** Entra só comando; a Unreal só lê.
2. **Ponto fixo** com matemática e RNG próprios (ver 3.3).
   - Multiplicação com intermediário de 128 bits, porque estouro de inteiro com sinal é comportamento indefinido em C++.
   - Arredondamento definido na divisão de negativos.
   - O script de verificação, rodado antes de cada entrega, recusa `float`, `double`, `FMath`, `FName`, `TMap`/`TSet` sobre coisas da Unreal e `PointerHash` na pasta da sim.
   - O índice do `FName` depende da ordem de criação em cada processo (`NameTypes.h:93,138`).
3. **IDs inteiros** atribuídos em ordem estável na carga.
4. **Estado em arrays contíguos (SoA) com um serializador só.** O mesmo código serve pra save, impressão digital, snapshot de reconexão e relatório de desync, e deixa a porta aberta pro rollback.
5. **Comandos binários com o passo marcado.** O log de comandos é o replay, com cabeçalho de versão.
6. **Seleção replicada como comando.** O grupo é criado uma vez e as ordens seguintes valem pro grupo, com adiciona/remove. Uma ordem com 300 IDs explícitos (1200 B) não cabe num pacote: o EOS aceita 1170 B (`eos_p2p_types.h:14`) e o ZH tinha 1094 úteis (PR #2277 do TheSuperHackers). O ZH resolvia com grupos. Comando maior que um pacote é fragmentado e remontado, com limite.
7. **A sim valida todo comando:** dono, custo, pré-requisito, recarga. No lockstep, um cliente modificado manda ordem impossível, e todas as máquinas executariam.
8. **Impressão digital a cada passo,** um hash combinado de 64 bits no pacote. Os hashes por sistema ficam no log local e só são pedidos na divergência. Cinco hashes de 8 B a 30 Hz já seriam 1,2 kB/s, mais que o tráfego inteiro. O Factorio confere a cada tick só uma checagem rápida; o estado inteiro, só em depuração (FFF #55).
9. **RNG da sim separado do cosmético.** O Vanguard já faz isso.
10. **Nada de "jogador local" dentro da sim.** Foi por isso que o ZH ficou sem replay de campanha.
11. **Design "por segundo",** com o passo como constante.
12. **Executor da sim sem tela,** pra replays de referência e IA × IA.
13. **Transporte atrás de interface,** com toda leitura de pacote checando limites. O ZH tinha um estouro de buffer na leitura de arquivo (#1981).

### 3.3 Ponto fixo ou float exato? Decisão: ponto fixo

| | Ponto fixo (int64, 16 bits de fração) | Float exato |
|---|---|---|
| Quem fez | OpenRA (todo em inteiros); SC2 segundo fóruns [NC] | ZH (`typedef float Real`), SupCom (IEEE estrito), BAR/Recoil (streflop, testado x86 × ARM) |
| Mesmo resultado em qualquer compilação/CPU | sim, por construção | só com o mesmo binário, flags fixas, sem FMA e matemática própria |
| Risco medido aqui | nenhum | o jogo da Unreal compila com `/fp:fast` e o editor com `/fp:precise` (`VCToolChain.cs`). O `Sin/Cos/Exp/Pow` da `FMath` chama a biblioteca C, que escolhe versão com ou sem FMA3 conforme a CPU (doc da Microsoft). O ZH deu 100% de desync no Windows ARM emulado (#397) |
| Custo | raiz, seno e atan2 próprios (tabela/polinômio); converter os dados de design | disciplina permanente e testes entre CPUs |

**Escolha:**
- **Ponto fixo**, porque o resultado não depende de compilador, flag nem CPU.
- O OpenRA prova que um RTS inteiro cabe nisso.
- A sim é pequena; o visual continua em float.
- Resolução de 1/65536 m num mapa de 1,5 km dá 27 bits, e o produto cabe em 64.

### 3.4 Rede: os números

| Jogo | Modelo | Passos/s | Fonte |
|---|---|---|---|
| Age of Empires 1/2 | lockstep P2P, turno ~200 ms | — | Bettner & Terrano, GDC 2001 |
| **C&C ZH** | lockstep, o host repassa ("packet router") | 30 | `GameCommon.h:72`, `ConnectionManager.cpp` |
| Supreme Commander | lockstep, hash 1×/s | 10 | Forrest Smith, 2011 |
| Spring/BAR | lockstep via servidor que repassa | 30 | `GlobalConstants.h` (`GAME_SPEED`) |
| StarCraft II | lockstep via servidor | 16 por segundo de jogo (22,4 no "Faster") | Liquipedia |
| Stormgate (UE5) | sim própria (SnowPlay), servidor de 64 Hz com rollback; se é bit-exato [NC] | 64 | Cosmocover, 13/08/2024 |
| Planetary Annihilation | cliente-servidor com streaming | 10 | ChronoCam; ~1 Mbit/s por jogador |

**Nossa partida:**
- **30 passos/s.** É o mesmo do ZH, do Spring e do Vanguard.
- **Uma rodada da rede por passo.** Turno de k passos só se a medida pedir.
- **Atraso de comando adaptativo, pelo caminho real.**
  - Com o anfitrião repassando, o clique faz dois saltos (A → anfitrião → B), mais o relay.
  - Atraso = teto((ida A→anfitrião + ida anfitrião→B no pior par + jitter p95) × 30), com folga.
  - Mínimo de 4 passos (133 ms), como a comunidade do ZH corrigiu (#1482: usar o máximo real e arredondar pra cima; #1489).
  - A mudança de atraso é um comando marcado pra um passo futuro.
- **Ritmo:**
  - se um PC não aguenta 33 ms por passo, a partida inteira desacelera e mostra quem é;
  - o ZH usava o fps de desenho (limitado a 30) porque lógica e desenho eram acoplados; aqui não são.
- **Atraso do clique até a imagem: medido no M1, sem chute.**
  - Somam o atraso de comando, a espera até o próximo passo (0–33 ms), a interpolação (1 passo) e o pipeline da Unreal (`r.OneFrameThreadLag=1`, fila da GPU).
  - Na LAN isso dá ~200–250 ms [E].
  - Testar `r.GTSyncType` e desenhar o passo mais novo com extrapolação curta.
- **Reenvio pelo tempo de ida e volta medido,** não fixo em 2 s como no ZH.
- **Cifra própria (AEAD) com chave combinada no lobby,** igual em todo transporte.
  - O SDR da Steam cifra, mas LAN e IP direto não têm relay.
  - No EOS a cifra não está documentada (`SocketSubsystemEOS.cpp`: `RequiresEncryptedPackets() { return false; }`).
- **Aperto de mão de versão no lobby:** hash do executável e de todo o conteúdo lógico, como o ZH conferia o CRC do exe e dos ini.

**Infraestrutura:**
- **1º: LAN e IP direto.**
- **Depois: relay.** As duas opções:
  - **Steam SDR:** IP oculto e cifrado; exige Steam e App ID; dá pra testar com o App ID 480.
  - **EOS P2P com `EOS_RC_ForceRelays`:** gratuito, e aceita login pela Steam (`EOS_ECT_STEAM_SESSION_TICKET`), sem conta Epic.
  - A escolha é técnica e fica pra quando o online chegar. O transporte é trocável.
- **Ranqueado, mais tarde:** um "servidor de turnos" que ordena os comandos e pode rodar a sim pra arbitrar desync e servir snapshot de reconexão.
- **Rollback:** fica fora da 1ª versão pelo custo.
  - Voltar 3–4 passos é re-simular 3–4 passos num quadro; com uma sim de ~8 ms por passo, isso dá 24–32 ms [E].
  - A porta fica aberta pelo estado em SoA.
- **Maphack** é o ponto fraco do lockstep (todo PC sabe tudo). Pro ranqueado, anti-cheat (o EAC é gratuito) mais análise de replay. Servidor que filtra a visão é decisão de orçamento, bem depois.

### 3.5 IA

- **Dentro da sim, em todos os PCs:** regras, gatilhos de missão e vitória.
- **Fora dela:** o jogador-IA. Ele:
  - lê o mesmo estado que o desenho lê;
  - roda num PC só;
  - emite comandos marcados com o passo.
- **Duas regras:**
  - a IA **nunca segura o passo**: comando atrasado entra no próximo passo livre;
  - nos testes sem tela ela roda **síncrona**, sem thread, pra que IA × IA dê a mesma partida toda vez.
- **No ZH era diferente:** a IA rodava dentro da sim e os scripts dela entravam no CRC, "since the game will go out of sync if they change" (`GlobalData.cpp:1010`).
- **IA v0:** lista de construção por facção, times com temporizador e ataques. É a ideia do ZH, escrita do zero.

### 3.6 Mundos

**O que roda com o jogo aberto (5.8, conferido):**
- malha própria (`ProceduralMesh`/`DynamicMesh`) com colisão;
- texturas virtuais (RVT);
- PCG em tempo de jogo;
- instâncias de peças **já prontas** com Nanite e Lumen.

**O que não roda:**
- Landscape (`ALandscapeProxy::Import` e `FLandscapeEditDataInterface` estão atrás de `WITH_EDITOR`);
- Nanite em malha criada com o jogo aberto. O build rápido, o único fora do editor, não monta Nanite nem distance field (UE-346969). Infiro que esse chão fica fora do Lumen por software.

**Opções a medir pro chão do mapa aleatório:**
- a malha do WorldForge ao vivo + RVT;
- blocos de chão Nanite prontos, deformados por textura de altura (Nanite Tessellation, ligado por padrão na 5.8) [NC].

**O pacote canônico que o gerador entrega pra partida** (a sim só lê isto):
- alturas em 16 bits;
- **grade lógica de 4 m em inteiros**: terra, água navegável, só ar, níveis, rampas, zonas de construção, bloqueios;
- bases com time e ordem;
- recursos com tipo;
- objetos que bloqueiam;
- versão do gerador e hash.

O desenho também parte desse pacote, pra o chão nunca mostrar penhasco onde a sim tem rampa.

**Pedidos pra sessão do WorldForge** (por mensagem, quando você aprovar):
1. `FTeamLayout` no lugar de `Players`, e as restrições de justiça da Parte 1, item 7.
2. A grade lógica em inteiros, com hash, num módulo só Core.
   - Hoje `Sin/Cos/Loge/Pow/Exp` da biblioteca C entram no gerador: `WorldGenerator.cpp:86,276,833,1719,1831` e `ErosionFilter.cpp:17,52`.
   - E o jogo compila com `/fp:fast`.
3. A medida lado a lado "vista ao vivo × Landscape": mesma semente, câmera do jogo, ms e imagem.

**Tamanhos:**
- O mapa de 1 km salvo pelo editor mede 73–340 MB (`WorldForge/Content/Maps/Worlds`). O cozido não foi medido.
- O pacote lógico mede ~0,35–0,4 MB (`height16.png` de 334–342 KB + `map.json` de ~2 KB).

### 3.7 Desempenho: o que já foi medido e o que falta

**Medido na 1660:**
- **Cena do BlastLab sem explosão:** ~15 ms de GPU a 720p (TSR 3 ms, Lumen ~5 ms).
- **Morte de tanque no Alta, câmera de RTS:** 32 ms no quadro mediano, 41 ms no p95 (~31 qps).
- **Memória de vídeo:** pico de 5,1 de 6 GB.
- **WorldForge, deserto inteiro com costa:** 20–25 ms na vista de jogo.

**O que isso quer dizer:** **hoje a batalha roda a ~30 qps no Alta da 1660.** 60 qps nela é a Média ou cortes medidos. Uma tabela de orçamento por sistema só vale depois da **batalha de referência** (M0 abaixo).

**O que testar na 1660 antes de reservar pro Ultra:**
- **MegaLights.** Na 5.8 ele aceita traçado por software (`MegaLights.cpp:541-544`). Pode baratear justamente os ~7 ms de luzes e sombras das explosões.
- **Lumen Lite.** Beta na 5.8; a Epic diz "2× mais rápido", a medir aqui.

**Ultra que roda na 1660.** O Ultra tem de abrir na 1660, mesmo lento, pra você ver e aprovar. Ray tracing de hardware vira um "Ultra+" opcional.

**Câmera do jogo.** Fica fixada como regra de jogabilidade: inclinação alta de RTS, faixa de zoom e giro limitado (o Vanguard fixou 37,5°, como o ZH, ADR-0003). O modo cinematográfico, deitado até o horizonte, é separado e fica fora do orçamento de jogo.

**Número de unidades.** O alvo de unidades sai dos replays do ZH: o pico por jogador em 1v1 e em 4 jogadores, com margem. Não é número chutado. Ele decide se 30 passos/s cabe em ~8 ms por passo no PC mínimo.

---

## Parte 4. A ordem (marcos pequenos, cada um com o que você clica e vê)

| Marco | O que você vê | Pronto quando | Tamanho [E] |
|---|---|---|---|
| **M0. Batalha de referência** (no BlastLab, em paralelo; é a frente das explosões) | um mapa do WorldForge na câmera do jogo, ~50 tanques, 10 mortes ao mesmo tempo, em cada preset | ms e GB por preset medidos; você aprova contra a versão aprovada; a tabela de orçamento sai daqui | médio |
| **M1a. O juiz** | uma janela: um tanque obedece ao clique, anda e atira; "replay" repete igual | replay bate a impressão digital passo a passo; desync forçado é pego em ≤ atraso + 1 passo e o relatório aponta o sistema | médio |
| **M1b. Dois PCs** | duas janelas (ou dois PCs na LAN), você clica numa e as duas andam iguais | 30 min sem desync a 0, 100 e 200 ms com 2% de perda; clique → imagem medido | médio |
| **M1c. Jogo empacotado** | o jogo instalado conversando com o editor | 1º cook do projeto; o mesmo juiz dá o mesmo resultado nos dois (prova do ponto fixo) | pequeno, mas precisa de disco |
| **M2. Fatia jogável** | uma partida de verdade contra IA simples, num mapa oficial: 1 facção, ~3 unidades, construção, névoa | dá vontade de jogar de novo (critério seu); unidades no alvo medido | grande |
| **M3. Telas** | menu com cena 3D, lobby, carregando, 3-2-1, resultado, lista de replays | o fluxo inteiro sem travada de shader na 1ª execução (`-clearPSODriverCache`) | médio |
| **M4. Online** | partida com amigo pela internet, até 4 jogadores, queda e volta | relay, aperto de versão, votação, reconexão por snapshot | grande |
| **M5. Mapa aleatório no jogo** | a semente vira mapa no lobby, igual em todos os PCs | hash bate em Intel e AMD; lado a lado com o oficial aprovado por você | médio |

**Depois:**
- economia completa;
- as 3 facções;
- superarmas e poderes de general;
- guerra naval;
- campanha;
- áudio de batalha;
- HUD final.

**Frentes que não podem ser esquecidas** (cada uma ganha plano próprio quando chegar a vez):
- **Produção da arte 3D.** O caminho do tanque USA (`docs/CONCEITO_UNIDADES.md` §10–11) vira linha de produção pras outras unidades e prédios.
- **Áudio de batalha:**
  - limite de sons simultâneos;
  - ouvinte na câmera de RTS;
  - vozes e narrador;
  - especificação em `docs/CONCEITO_SOM.md`.
- **HUD e controles:** barra de comando, minimapa, grupos, atalhos. O Vanguard já tem ADR-0004 e ADR-0021.
- **Legibilidade medida:** quanto fumaça, explosão e névoa podem cobrir das unidades.
- **Assets grandes no controle de versão** (Git LFS ou outro), quando o projeto do jogo nascer.

---

## Parte 5. O que não está confirmado

- O netcode de AoE IV, Company of Heroes 3, ZeroSpace e Tempest Rising (não há fonte técnica pública).
- Se o Stormgate e o SC2 são bit-exatos em ponto fixo.
- O tamanho cozido dos nossos mapas.
- Se a água do plugin Water pode ser criada com o jogo aberto.
- `PrepareMapChange` com World Partition.
- O assert do cache de PSO empacotado na 5.8 (relato de fórum).
- Nanite em malha gerada com o jogo aberto via plugin de terceiros (afirmação do vendedor).

## Fontes principais

**Código liberado:**
- `github.com/electronicarts/CnC_Generals_Zero_Hour`: `GeneralsMD/Code/GameEngine/Source/GameNetwork/` (ConnectionManager, FrameData, Network), `Common/Recorder.cpp`, `GameClient/GUI/Shell/Shell.cpp`, `GameClient/GUI/LoadScreen.cpp`, `GameLogic/System/GameLogic.cpp` (`populateRandomStartPosition`), `Common/GlobalData.cpp`.
- TheSuperHackers/GeneralsGameCode: issues e PRs #397, #1482, #1489, #1981, #2277.
- OpenRA: `ClassicMapGenerator.cs`, `MapPreview.cs`.
- Beyond All Reason / RecoilEngine: `GlobalConstants.h`, `rts/lib/streflop`.

**Talks e artigos:**
- Bettner & Terrano, "1500 Archers on a 28.8", GDC 2001.
- Forrest Smith, "Synchronous RTS Engines and a Tale of Desyncs", 2011.
- Glenn Fiedler, "Fix Your Timestep!" (2004) e "Floating Point Determinism".
- Factorio FFF #55.
- Schneider, "The Real-time Volumetric Cloudscapes of Horizon Zero Dawn", SIGGRAPH 2015.
- Hillaire, "Physically Based and Scalable Atmospheres and Volumetric Clouds", SIGGRAPH 2020.
- Emerson, "Crowd Pathfinding and Steering Using Flow Field Tiles", *Game AI Pro* 2013.

**Mapas:**
- Togelius et al., "Multiobjective Exploration of the StarCraft Map Space", CIG 2010.
- Liapis, Yannakakis & Togelius, "Generating Map Sketches for Strategy Games", 2013.
- AoE2 RMS (`docs.racket-lang.org/aoe2-rms`).
- AoE IV, "Placing Player Starting Positions" e patch 14681.
- TL.net, tamanhos e simetria de mapas do SC2.

**Plataformas:**
- Steamworks Steam Datagram Relay.
- Epic Online Services (`onlineservices.epicgames.com`, SDK em `Engine/Source/ThirdParty/EOSSDK`).
- Lançamento do Stormgate (Cosmocover, 13/08/2024).
- Wccftech, 01/04/2026 (Hathora).

**Unreal 5.8 (fonte instalado):**
- `LandscapeProxy.h`, `StaticMesh.cpp:8923`, `VCToolChain.cs`;
- `MegaLights.cpp:541`, `IrisConfig.cpp:15`;
- `CommonActivatableWidgetContainer.h`, `ShaderPipelineCache.cpp`, `PSOPrecacheSettings.cpp`;
- `GPUBenchmark.cpp`, `RenderingThread.cpp` (`r.OneFrameThreadLag`, `r.GTSyncType`);
- `NameTypes.h`, `eos_p2p_types.h`.

---

## Parte 6. Passo 1: o que já roda (08/10/2026)

**O que você clica:** o jogo abre no menu, com o deserto do WorldForge ao fundo
passeando devagar. "Jogar Skirmish" → você (azul) contra a IA (vermelho), 6
tanques USA de cada lado, no `World_TestSalt2`. Como ainda não há base, a
largada fica no caminho entre as duas bases, nos pontos a 520 m um do outro
pelo caminho (`skirmish_gap_m` no `Maps.json`). Uma linha reta pro meio caía em
cima do maciço (teste t2). A IA parte em 8 s, e o contato vem entre ~30 s e ~65 s.
Na entrada: fade de 1,2 s, o objetivo por 7 s e uma seta vermelha na borda
apontando o inimigo mais perto quando ele não está na tela. Ganha
quem destruir todos os tanques do outro. O resultado aparece 3 s depois do
golpe final, com tempo, destruídos e perdidos, e o replay fica salvo em
`Game/Saved/Replays`. "Voltar ao menu" fecha o ciclo.

```
powershell -File Game\Tools\game.ps1 setup     # 1ª vez: compila, traz mapa/tanque/explosões, monta os materiais
powershell -File Game\Tools\game.ps1 view      # abre no menu (alta); view ultra pro visual máximo
powershell -File Game\Tools\game.ps1 view -Folder t1          # teste automático: joga sozinho, fotos e log
powershell -File Game\Tools\game.ps1 view -Folder t2 -Record  # + vídeo da batalha (batalha.mp4)
powershell -File Game\Tools\simtest.ps1        # a simulação sem a Unreal, em 3 compilações
```

**Controles** (Zero Hour / Vanguard ADR-0003 e 0004):
- **Botão esquerdo:** clique seleciona; arrastar faz a caixa. Com seleção, clique no chão = andar, clique no inimigo = atacar. Shift soma ou tira; dois cliques pegam os iguais na tela.
- **Botão direito:** clique rápido desseleciona; arrastar rola (mais longe = mais rápido).
- **Os dois botões juntos (v4, 23/10):** esquerdo + direito, em qualquer ordem, arrastando, puxam a linha do ponto apertado até o cursor. Enquanto puxa, cada unidade aparece no chão como um anel com um risquinho pra frente, no lugar exato onde vai parar (a conta da simulação, `World::PreviewLineSlots`). Da esquerda pra direita na tela, a frente fica de costas pra câmera; ao contrário, vira pra ela. Soltar os dois manda; o `Esc`, ou soltar um e segurar o outro, desiste. Com o `F` vai em formação, com o `A` armado vai atacando (anéis vermelhos). O esquerdo arrastado sozinho é sempre a caixa, mesmo com tropa selecionada.
- **Teclas:**
  - `A` = a próxima ordem é ataque andando; `S` = parar;
  - `Ctrl+número` grava um grupo, o número chama, `Shift+número` soma, dois toques levam a câmera até ele;
  - ↑↓ e a borda da tela rolam; ←→ e o botão do meio arrastado giram; dois cliques no meio voltam ao padrão;
  - roda, `=` `−` `PgUp` `PgDn` = zoom; `Alt`+meio inclina (o modo cinematográfico);
  - `Esc` pausa; `F5`–`F8` = baixa/média/alta/ultra; `F10` = linha de depuração (passo, impressão digital, ms).

**Como está feito:**
- **`Game/Source/SimCore`, o juiz.** C++ puro em ponto fixo, 30 passos/s: comandos, alvos, caminho A* na grade de 4 m, movimento, separação, torre, projétil, dano em duas fases, vitória, IA de skirmish, replay e impressão digital.
  - `simtest` compila os mesmos arquivos fora da Unreal em `/O2 /fp:fast`, `/Od /fp:precise` e `/O2 /fp:strict /arch:AVX2`, e as três dão o mesmo hash. Também confere o replay.
  - Números do tanque: Crusader do ZH 1.04 (`Game/Config/RTS/Units.json`), com 1 unidade do ZH = 0,2633 m.
- **`Game/Source/RTSGame`, a Unreal desenhando.**
  - A ponte (`RTSMatchSubsystem`) interpola entre o passo anterior e o atual.
  - Unidades: as peças do tanque do BlastLab, com a torre girando pela simulação, inclinação no relevo, recuo do tiro, carcaça queimada e às vezes a torre voando.
  - Efeitos: traçante curto, clarão e impacto.
  - Câmera do ZH, controle, HUD em canvas com letra do Slate, menu, pausa, resultado e tela de carregamento.
- **`Game/Source/BlastFx`, a explosão do BlastLab.** A morte de tanque é a Carga Pesada aprovada a 0,55, com o diretor de orçamento: as 3 maiores na tela em detalhe total. O halo de ofuscamento sai, porque na câmera alta ele atravessa o chão.
- **Mapa e assets, por link físico do mesmo disco (0 byte):** o mapa e as fatias do WorldForge, o tanque e as explosões do BlastLab (`game.ps1 worlds`).
  - A grade lógica sai do mapa ao carregar (`RTSMapBaker`, 0,12 s): alturas do Landscape a cada 2 m, penhasco acima de 0,78, água abaixo de −1,5 m e pedras grandes pela lista do `Maps.json`.

**Medido (08/10, GTX 1660, alta, 1600×900, teste t1):**
- Quadro mediano de 21 ms (~47 qps), p95 de 62 ms. Os picos vêm das explosões e da 1ª vez de cada material.
- A simulação custa 0,03–0,07 ms por passo com 12 tanques.
- No teste sem a Unreal, 800 tanques ficam em ~0,3 ms por passo (fora a correção do teste de escala).

**Ajustes depois das revisões (08/10, manhã):**
- **Partida:**
  - segue rodando depois do fim, sem congelar no golpe final;
  - o resultado aparece 3 s depois, no terço de cima, com o relógio parado.
- **Comportamento das unidades:**
  - quem está parado vai ajudar o vizinho que leva tiro (até 20 m);
  - a perseguição tem coleira medida de onde começou e depois volta;
  - o alvo inalcançável é solto por 3 s;
  - a ordem que não leva a lugar nenhum desiste (era o impasse do t3);
  - o empurrão entre tanques fica suave no encosto e forte quando um está dentro do outro.
- **IA:** reenvia a ordem só pros tanques sem alvo.
- **Câmera e controles (Vanguard ADR-0003):**
  - ←→ e o botão do meio giram, com velocidade pela distância;
  - arrasto do direito: 180 px = 1×, teto de 2,5×;
  - borda de 16 px, 1,6×, e a vertical ×1,25;
  - zoom pelo teclado e Shift+número;
  - câmera presa 50 m pra dentro da borda.
- **HUD:**
  - anel 30% maior que o raio de colisão;
  - o anel acende sob o mouse;
  - o cursor muda: mira sobre inimigo com seleção, mão sobre os seus.
- **Explosões:** a configuração do volume aprovada no BlastLab (sem filtro estocástico, céu do Lumen no cache de luz, 96 passos no alta, volumes ligados no baixa). O aquecimento é desenhado no centro da vista até os shaders acabarem.
- **Peso do tanque:**
  - a suspensão do `CrusaderLocomotor` (5° de teto, mola 0,05/0,8 na arfagem e 0,025/0,3 na rolagem, fatores 0,3 e 0,33), só no desenho;
  - as rodas giram pela distância andada;
  - o tiro dá um tranco no casco.

**Teste t4 (08/10, 08:36): a partida não acabava.** A batalha durou ~20 s e matou 7 dos 12. Depois, um tanque inimigo ficou parado na largada até o tempo do teste acabar.
- **Causa:** pelo replay (`simtest.ps1 -Inspect <replay> -Id 10 -At 300`), ele nasceu numa célula livre isolada no alto de um paredão. A formação da largada não olha o terreno, e a simulação só garantia "célula andável", não "ligada ao resto".
- **Correção, na simulação:** ao montar a partida, cada tipo de unidade ganha o mapa de regiões ligadas (inundação pelos 4 lados, porque o caminho não corta quina). A região do exército é onde nasce a maioria do jogador; quem nasceu fora dela vai pra célula mais perto dentro.
  - Base inteira numa ilha fica na ilha (pros mapas com mar).
  - Teste fixo no `simtest` ("onde nasce"): platô isolado + ilha.
  - `SimVersion` 2: replays antigos são recusados, porque a regra mudou o resultado.
- **Teste:** o aviso de impasse agora só conta depois do primeiro tiro.
- **`game.ps1 view -Folder`** respeita a trava da placa (`docs/GPU.md`): espera, segura, renova a cada 5 min e solta.
- **`simtest.ps1`** acha o compilador sem o `vswhere`, que saiu junto com o VS 2022 (o VS 18, da Unreal, continua).

**Achado no vídeo do t4: a morte de tanque sai como uma nuvem branca estourada, não a bola de fogo do BlastLab.**
- Os canais de luz e a escala do sol batem: 128 000 lux no BlastLab e 120 000 no WorldForge, ambos físicos.
- A diferença é a curva de tom. O BlastLab troca o tonemapper da Unreal pelo `M_TonemapWebGL` (`BlastLab/Tools/build_scene.py`), e a explosão foi aprovada nele. O mapa do WorldForge usa o tonemapper padrão, com exposição fixa no EV físico.
- Pôr a curva do BlastLab no jogo muda também o visual do mapa, então é decisão sua com as sessões do BlastLab e do WorldForge.
- O teste é um A/B com a mesma explosão: no TestRange do BlastLab e no deserto do jogo, com e sem o `M_TonemapWebGL`.

**Movimento em grupo, o próximo da jogabilidade.** O ZH (`AIGroup.cpp`, lido como referência, sem copiar):
- **Clique dentro do grupo:** os tanques se juntam em volta do ponto.
- **4 veículos ou mais (`MinVehiclesForGroup`):** andam em 3 colunas (2 se forem menos de 5), alinhadas pelo último trecho do caminho do grupo. As vagas no destino ficam a 32 unidades = 8,4 m e cada tanque "reserva" a sua.
- **Grupo pequeno:** cada um mantém a distância que tinha do centro, limitada a 6 raios.

**Feito (08/10, depois do t5), em `World::GroupSlots`:** os três casos, com as vagas a 2,34 raios (8,43 m pro nosso tanque).
- As colunas usam o último trecho do caminho do centro até o ponto, e cada um vai pra coluna do seu lado (ninguém cruza o vizinho).
- Toda vaga fica na mesma região ligada do ponto: nunca cai do outro lado do penhasco nem na água.
- Duas mudanças nossas:
  - as fileiras também ficam a 8,4 m (no ZH, com menos de 5, elas encostam e o ajuste de destino separa);
  - grupo de mais de 12 vira um bloco quase quadrado, em vez de 3 colunas de centenas de metros.
- Teste fixo no `simtest` ("vagas do grupo"): 6 tanques → 3 colunas, 8,4 m entre as vagas, a frente no ponto. Clique dentro do grupo → todos a até 8,4 m do ponto.

Os anéis de seleção (1,3× o raio) ainda se sobrepõem com os tanques a 8 m.

**Teste t6 (08/10, 10:36, tempo real, com as colunas).** Vitória em 87 s; quadro mediano de 22 ms e p95 de 31 ms (no t1 o p95 era 62 ms), 40 de 8865 acima de 50 ms. Dois problemas:
- **"Voltar ao menu" falhou no teste, não no jogo.** O painel entra 3 s depois do golpe final, e o teste clicava em 2,5 s. Agora o passo do resultado espera o painel inteiro (3,8 s e o botão existir).
- **O primeiro minuto saiu cru:** chão em xadrez (material compilando), pedras em malha grosseira (Nanite sem as páginas finas) e tanques sem textura. O PC estava ocupado e o cache de shaders (Zen) levou 20 s pra subir; nos t4 e t5, com o cache quente, a partida já abria bonita.
  - **Correção, como os jogos fazem:** a tela de carregar continua até a cena estar pronta. Ela espera os shaders na fila zerarem, o carregamento assíncrono acabar e as texturas pararem de faltar (ou de chegar, se o pool encher). Depois disso, segura mais 1 s pro Nanite buscar as páginas finas pelo que é desenhado atrás da tela.
  - O teto é de 25 s. A simulação não anda e as ordens não valem enquanto isso. O log diz quanto levou ("RTS cena pronta em ...").

**Névoa de guerra e minimapa (08/10, tarde; a lista viva é `docs/BASE_JOGAVEL.md` §0).** O plano passou por três revisores (Unreal, lockstep, jogabilidade do ZH) antes do código.
- **Na simulação** (`Sim/Vision.h`):
  - visão por time em inteiros, na grade de 4 m: cada unidade carimba um disco, com contador por célula (o esquema de revelar e desfazer do ZH);
  - revelar vai além de mirar (o ZH separa ShroudClearingRange de VisionRange): Sight + maior raio + 1 célula, então ninguém atira no que o time não vê;
  - quem sai de uma célula deixa a visão velha 1 s (ZH `UnlookPersistDuration`), quem morre também, e quem atira de dentro da névoa aparece 1 s pra quem levou o tiro;
  - ordem de ataque num alvo que sumiu vira ataque andando até onde ele estava;
  - `MatchSetup.bFogOfWar` vem do menu, e o comando `SetFog` (o F9) só vale com `bAllowDebugCommands` (skirmish local);
  - a visão entra na impressão digital (as células carimbadas e os olhos que ficam);
  - a IA de teste continua vendo tudo: IA fica pra depois, o Felipe.
  - **Testes:** discos, explorado, olho que fica, SetFog em bytes e recusado online, time inválido recusado. Numa batalha 12 × 12 com névoa: 187 tiros, 0 em alvo invisível, e 105 conferências da visão contra uma refeita do zero, todas certas. As 3 compilações dão o mesmo hash. 800 tanques: ~1 ms por passo.
- **No desenho:**
  - **A passada (módulo `RTSShaders`, `Game/Shaders/Private/FogOfWar.usf`)** roda depois da luz do opaco e antes dos volumes e da translucência, como o W3DShroud do ZH. Assim a explosão e o traçante não pegam a névoa do chão atrás deles; um pós-processo comum cortaria a bola ao meio na borda da visão.
    - **Níveis do ZH:** 0 nunca visto, 127/255 já visto, 255 à vista, andando 1 nível por segundo. Eles vão como sRGB, e o hardware entrega o multiplicador linear.
    - **Borda:** B-spline com 4 leituras (Sigg & Hadwiger), com um ruído lento na escala da célula.
  - **Exposição local:** a câmera do jogo usa só a grade bilateral (`LocalExposureBlurredLuminanceBlend = 0`). Senão a névoa escura clareava o que está à vista. A/B contra o aprovado com `-RTSLEBlend=0.6`.
  - **Na névoa somem:** a unidade inimiga (com sombra e luz, 1 s de carência), o anel, a barra, o clique, o clarão e a luz de quem atira de dentro dela, o impacto e a explosão. O traçante nasce onde entra na visão. A carcaça só aparece depois de vista, e sem o fogo enquanto ninguém olha.
  - **Vazamentos fechados no HUD:** "Inimigo N" vira "Destruídos K"; a seta aponta só inimigo à vista, ou a base inimiga até ela ser explorada.
  - **No fim da partida o mapa se revela,** como no ZH.
- **Minimapa:**
  - **Captura:** foto de cima ortográfica em HDR, tirada uma vez na tela de carregar, sem as unidades e antes de a névoa ligar. A exposição usa a chave de Reinhard (média geométrica → 18%, com o bias do mapa) e a curva é a ACES de Narkowicz.
  - **Formato:** losango girado pelo giro padrão da câmera, então "pra frente" fica pra cima e a caixa da câmera, em pé. Os pontos têm a cor do jogador, e a caixa da câmera, as cores do ZH.
  - **Clique do ZH:** esquerdo com seleção manda andar (ou atacar andando com o A); sem seleção, ou com o direito, a câmera pula e, segurando, arrasta. O minimapa fica com o gesto inteiro: não desseleciona nem rola.
  - **"Sob ataque" do ZH:** o triângulo que gira e fecha, com 4 s de vida e no máximo 1 a cada 10 s por 66 m; o Espaço leva a câmera até lá.
- **Liga e desliga:** F9, o botão na pausa e o botão "Névoa de guerra" no menu principal.
- **Caixa vazia não mexe na seleção** (ZH `SelectionXlat::onMouseLeftClick`): antes, um clique que escorregava uns 6 px virava caixa vazia e desfazia o grupo. Caixa também não dá ordem, como no ZH.
- **O teste automático bloqueia o mouse e o teclado de verdade** (um `IInputProcessor` deixa passar só a entrada sintética). O t8 e o t9 falharam porque o PC estava em uso enquanto a janela do teste abria na frente. Desde o t10 (08/10): 0 falhas.
- **`game.ps1`** usa a fila da placa (`C:\Users\luisf\.gpu_queue`).

**Pendente, na ordem:**
1. **Pedido pra sessão do WorldForge (ela estava fechada):** a grade de passabilidade canônica exportada junto com o mapa (`pass.png` + obstáculos + versão), no lugar da tirada do mapa ao carregar.
2. **Esteira:** as rodas giram, mas a esteira ainda não anda (falta animar a textura dela).
3. **Som.** Nenhum ainda; precisa de assets (o pacote GDC da Sonniss, com o seu OK pra baixar).
4. **Picos de quadro:** aquecer os materiais dos efeitos antes da partida, e medir a morte de tanque no orçamento.
5. **Do revisor do juiz:** sequência por jogador nos comandos (pro online), faixas no `ReadSetup`, script que recusa float no SimCore e clang-cl no simtest.
