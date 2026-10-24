# Base jogável: a lista (pra nunca esquecer)

Começada em 08/10/2026 a partir do levantamento de todas as sessões (7 leitores, mais a pesquisa
de como os AAA fazem, mais um crítico de completude) e revisada por mais 3 críticos (contra o
levantamento, os documentos do projeto e o vídeo-alvo). **Toda sessão que mexer num item marca aqui.**

**Legenda:** `[ ]` a fazer · `[~]` em andamento ou feito sem aprovação · `[x]` aprovado pelo Felipe.
Entre parênteses: quem faz.

**O que é a base:**
- Multiplayer de verdade, com 2 ou mais jogadores em rede.
- Cada jogador tem uma **War Factory gerando tanque**, **sem economia**.
- **Névoa de guerra** e **minimapa**.
- **Tudo visual na versão final**: modelos, explosões, água, trilhas, efeitos e detalhes.

Economia, construção livre e unidades novas ficam pra depois, só quando o Felipe pedir.

**O alvo:**
- **Em movimento:** `captures/concept/33_video/gameplay_v2.mp4` (folha `gameplay_v2_sheet.jpg`), a confirmar com o Felipe. Mostra a War Factory USA com tanques no pátio e saindo, poeira atrás de cada tanque, anel de poeira na explosão, fumaça preta e carcaças queimando.
- **Parado:** a foto oficial `docs/concept/scenes/desert_golden_hour/more/game_shot_usa_vs_gla.webp` e as folhas do diretor de arte, `captures/game/quadro_alvo_vs_jogo.jpg` e `captures/game/morte_alvo_vs_jogo.jpg`.
- **Os 7 defeitos da 1ª gameplay** (diretor de arte, 08/10):
  1. morte branca;
  2. os dois lados iguais;
  3. anéis grossos;
  4. desfile;
  5. tiro sem clarão nem poeira;
  6. chão que não guarda nada;
  7. fumaça cobrindo as unidades.

**Quem é quem:**

| Sessão | Cuida de |
|---|---|
| "RTS game architecture e gráficos" | o jogo, `Game/` |
| "C&C Zero Hour conceitual modernizado" | arte, diretor de arte e o **FXLab** (o fogo da carcaça). O tiro e o arsenal foram **pausados** pelo Felipe nela: vão pras sessões de explosão |
| "Fluxo de explosão realista AAA" | BlastLab |
| "Gerador de mundo RTS" | WorldForge |
| Gráficos (nova, 09/10) | o look do quadro inteiro (luz, céu, névoa, exposição, curva de cor, pós, padrão de material) pra ficar idêntico aos game shots; vira plugin comum pro jogo, o "ver na versão final" do WorldForge e o BlastLab. Entrada: `docs/GRAFICOS.md` |
| "Projeto complexo" | ExplosionLab, noutro repositório: `C:\Users\luisf\explosion-lab` |

---

## 0. Prioridade, em cima de tudo

- [~] **Minimapa, que responde pela névoa** (jogo, começado em 08/10)
  - [x] **Feito e testado pelo Felipe (08/10, "ficou bom"), commit `e927a02`:** foto de cima na tela de carregar, losango alinhado com a câmera padrão, pontos na cor do jogador, caixa da câmera (cores do ZH), névoa, clique do ZH (o minimapa fica com o gesto inteiro), o "sob ataque" do ZH e o Espaço. Detalhes em `docs/JOGO.md` Parte 6.
  - **O que mostra:**
    - o mapa pronto visto de cima: captura ortográfica do mapa final, porque o `minimap.png` do WorldForge é o do rascunho;
    - as unidades e a fábrica como pontos na cor do jogador;
    - o retângulo do que a câmera vê, que é um trapézio na câmera inclinada.
  - **Névoa no minimapa:** o nunca visto fica preto e o explorado escurecido. Inimigo só aparece quando visível, e o prédio inimigo já visto fica como "fantasma". Usa a mesma textura de visão da névoa da tela.
  - **Clique:** o clique move a câmera. Com unidades selecionadas, o clique ou o arrasto no minimapa dá a ordem (andar, ataque andando).
  - **Alertas:** pisca "sob ataque" no ponto do minimapa e da borda da tela; a fábrica avisa quando o tanque fica pronto.
  - **Decisão do Felipe:** sempre ligado, ou só com radar, como no ZH?
- [~] **Névoa de guerra** (jogo, começada em 08/10): simulação e desenho
  - [x] **Na simulação, feito e testado pelo Felipe (08/10), commit `e927a02`:** visão por time em inteiros (`Vision.h`), disco de 39,5 m carimbado como no ZH, explorado, a visão do morto fica 1 s, ordem de ataque em alvo invisível recusada, o comando `SetFog` de depuração (entra no replay e na impressão digital). A IA de teste **não** foi mexida (o Felipe: IA fica pra depois).
  - [x] **No desenho, feito e testado pelo Felipe (08/10), commit `e927a02`:** a passada própria que escurece só o opaco, antes das explosões (como o W3DShroud do ZH), com os níveis do ZH (0, 127/255, 255; 1 nível por segundo), a borda suave, a exposição local só bilateral, o que some na névoa e os vazamentos do HUD fechados. No fim, o mapa se revela.
  - [x] **Liga e desliga de debug** (pedido do Felipe, testado em 08/10): F9, botão na pausa e no menu principal. A regra vai pela fila só no skirmish local (`bAllowDebugCommands`); online, vira regra da sala.
  - **Na simulação:** visão por time numa grade de 4 m, em inteiros, com dois estados: vista agora e já explorada. Ela é igual em todos os PCs por causa do lockstep.
    - Só se mira no que é visível.
    - A IA só usa o que vê; hoje ela lê o mapa inteiro (`SkirmishAI.cpp`).
    - A visão continua um instante depois que a unidade morre.
  - **No desenho:**
    - uma textura de visão do jogador local, com transição de 0,5–1 s;
    - borda suave sem cara de grade borrada (desfoque separável mais ruído na borda);
    - um pós-processo que escurece e tira a cor (a técnica do ZH, do League of Legends e do SC2, nas fontes).
  - **O que esconder:**
    - **Unidades:** inimigo na névoa some com anel e barra.
    - **Prédios:** a fábrica inimiga já vista fica "fantasma", sem seleção.
    - **Efeitos e luzes:** o tiro escondido não mostra o clarão; o traçante só aparece a partir de onde entra na área visível. A luz de clarão e de fogo escondido não acende o chão visível.
    - **Som:** a batalha escondida fica muda (`docs/CONCEITO_SOM.md`).
    - **Marcas:** carcaça, cratera e marca de esteira feitas na névoa só aparecem quando a área é revista.
  - **Vazamentos de hoje a fechar:**
    - o cursor mira o inimigo escondido (`PickUnit`);
    - o "Inimigo N" no topo conta todos;
    - a seta pro inimigo e o "siga a seta vermelha";
    - a sombra e o Lumen de unidade escondida.
  - Efeitos e luzes de explosão fora da visão: o jogo zera a opacidade e as luzes (o raymarch desenha todo volume visível).
  - Mais vazamentos: explosão escondida não mexe na exposição, no bloom nem projeta sombra de fumaça; o orçamento das explosões conta só o que o jogador vê; a fábrica fantasma fica congelada (chaminé, portão e bandeira parados); o pós-processo da névoa entra depois do passo do raymarch.
  - Maphack: no lockstep todo PC sabe tudo e a névoa é só desenho. Pro ranqueado, anti-cheat e análise de replay (depois).
- [ ] **O que falta da névoa e do minimapa** (depende de outras peças):
  - a fábrica inimiga "fantasma" e o aviso "tanque pronto", que vêm com a War Factory;
  - a batalha escondida muda, que vem com o som;
  - o custo da passada da névoa, a medir com a placa livre.
- [ ] **Minimapa também na sala de espera:** mapa com as bases e a escolha da largada.
- [~] **Multiplayer de verdade** (jogo, 10/10: LAN pronta, sem o teste do Felipe). Feito: menu MULTIPLAYER (LAN), sala com até 8, cor por jogador, névoa do anfitrião, a montagem do anfitrião pra todos, lockstep com a impressão digital, volta de todos pra sala, mapa com 4 bases (`docs/MULTIPLAYER.md` Parte 7). Em 11/10: times na sala (aliados lado a lado, vitória do time), IA nas vagas, espectador de quem cai, névoa em 3 tipos (explorada, preta, desligada), a seta "enemy base" fora. Em 13/10 (N5, sem o teste do Felipe): a pausa pra todos, render-se e a queda com espera de 60 s e votação, como no ZH (`docs/MULTIPLAYER.md` Parte 7). Falta: reconexão, barras de carregamento, sinal no mapa, dar dinheiro, o online (último do jogo). Ver a seção 1.
- [~] **A v4 (22–23/10, a Arte fazendo sozinha; sem o teste do Felipe, sem commit).** O pedido dele em 6 itens. O design e as
  contas em `docs/V4_ARSENAL.md`:
  - [~] a fumaça do Patriot sem as faixas cinza (os materiais da fumaça coletados pelo GC);
  - [~] a poeira das lagartas fraquinha, lisa e sem a linha (`docs/POEIRA_DAS_LAGARTAS.md` §v4);
  - [~] o dinheiro inicial de 10 a 50 mil no menu e na sala da LAN;
  - [~] as carcaças afundam e somem aos ~48 s (a marca no chão fica);
  - [~] a Supply Drop Zone (o avião a cada 70 s), a patente do general, o Cruise Missile Strike, o Tomahawk, o dano em área;
  - [~] (23/10 à noite, a sessão sim, simulação 20, sem o teste do Felipe) **vender** (o botão Sell de todo prédio pronto; a zona de lançamento só tem ele: não produz, sem ponto de encontro): a fila volta na hora, a energia, a defesa e os poderes param já, o prédio desmonta 6 s como a obra ao contrário e some sem explodir com o "+$" de 50% do custo (o SellPercentage do ZH); **o conserto do Dozer** (o cursor da chave sobre o prédio avariado, o clique manda; 2% da vida cheia por segundo, de graça, um Dozer por prédio, como o DozerAIUpdate do ZH; o conserto sozinho do Dozer à toa ficou de fora: mexeria nos Dozers da IA); **o caminhão e o Tomahawk derrubam o poste e a palmeira** pelo choque (½·m·v² contra ~40 kJ do poste e ~60 kJ da palmeira; o carro eles seguem contornando); **a mira do Tomahawk** (o relógio `pre_attack_s` de 1 s desde o alvo no alcance, o míssil nascendo na boca do tubo levantado a 45°). As cenas: `v4_vender`, `v4_conserto`, `v4_cliques` (o conserto e o vender pelos cliques: "conserto_*" e "vender_*"; saíram do teste da base, que vendia a única usina dela), `v4_linha`, `v4_caminhao_postes`, `v4_tomahawk_palmeira`. Na revisão da noite: **quem atira parado segura o lugar** (o `moveAllies` do ZH: o aliado que chega contorna em vez de empurrar; o reforço empurrava o Tomahawk do cerco pra dentro do alcance da Patriot), e os testes da IA rodam com a mira de 1 s do jogo; a linha nunca passa da ponta puxada (o que não cabe no passo mínimo vai pra fileira de trás);
  - [~] a IA usando tudo (20 partidas IA × IA: mediana 15,9 min, antes 5,4–6,5);
  - [~] o sol a 18° (receita `uma_i1_s18c` no `World_TestSalt2`).
- [~] **War Factory funcionando, com a mecânica original do ZH** (jogo, 10/10, sem o teste do Felipe): Command Center + Dozer, energia, construção, depósitos e caminhão, a fila da fábrica, a carga que cai, vitória por base, modelos provisórios. O que está feito: `docs/ECONOMIA.md` Parte 5. Ver a seção 1.

---

## 1. O jogo (sessão "RTS game architecture e gráficos")

### A noite de 16/10 (o pedido dele: "quero me arrepiar"; tudo sem commit, esperando a palavra dele)
- [~] **A água do lago com a sombra filtrada.** Os blocos riscados na água sumiram, sem custo medível. É configuração do projeto (`DefaultEngine.ini`), com a causa achada pelo Look e pelo WorldForge. A/B no jogo: `captures/game/ab_agua_vsm.jpg`.
- [~] **O casco cede ao tiro:** inclina ~1,5° na mola da suspensão, pro lado de lá do disparo (o BAR faz; a pesquisa do Fluxo). O prédio não.
- [~] **A Patriot no jogo, provada na cena `patriot_defesa`:**
  - gira a caixa, solta a rajada e derruba 1 dos 3 tanques antes de cair (3 tanques ganham de 1 Patriot, como no ZH);
  - o radar foi pro alto do mastro;
  - fotos em `captures/game/patriot2/`.
- [~] **A v3 do chão, a parte do jogo** (`docs/CHAO_V3_ESTUDO.md` §7, com a chave `-RTSGroundV3`):
  - a luz do sol assada no momento em que a esteira passa ou o tiro cai (`GroundSunLit.usf`): a varredura no rumo do sol numa grade fina (2×2 no Alta, 4×4 no Ultra), a oclusão do céu e a fila com cota por quadro;
  - o perfil novo da trilha: sulco de 4 cm, bordinha de 3 × 20 cm, faixa de altura de ±10 cm;
  - as cenas `trilha_arcos` e `areia_aberta` (§6.1).
  - O material que lê isso é do Look e está em andamento. A interface é `GroundSunLit`/`GroundSunLitMid` (RG16F) e `UseSunLit`.
  - **A 1ª medida deu +1,2 ms** (34,1 × 32,9 ms). A causa: um retângulo que cobria a batalha inteira era refeito a cada quadro, com 7 m de sombra pra todo lado. Agora vai uma caixa por pedaço novo de trilha ou cratera, juntando as vizinhas. A trilha (que muda ~20 cm) estende a sombra só ~1,4 m; o contexto de 7 m do lado do sol continua, pra sombra da cratera vizinha.
    - **Medido depois (2 pares na cena `chao`, Alta):** +0,2 ms de mediana (33,4 × 33,2 e 33,6 × 33,4 ms), e o p95 não piora (42,8 × 43,2 e 44,3 × 45,5). Dentro da meta de ≤ +0,3 ms.
  - **Os torrões fora da sombra do sol** (a decisão (b) do Look, 16/10). Um torrão de 5 cm a 8° de sol faz 35 cm de triângulo preto: o "pontinho preto" já reprovado. As pedras ≥ 9 cm já fazem a sombra delas de malha.
    - A altura que faz sombra sai à parte (`bake_craters.py` grava `crater_*_sun.png`: bacia, borda e halo); o atlas aprovado saiu idêntico, byte a byte.
    - O caderno de altura das crateras passou a dois canais: R a cheia (a MF, sem mudança), G a do sol.
    - O sol lê o G; o escurecimento de contato (SkyAO) lê a cheia.
    - A prova a 8°: `captures/game/v3_chao/sombra_torroes_prova.png`.
  - **Um defeito antigo nas crateras, achado pela luz assada:** o forno borrava o X com `np.roll`, que dá a volta. As ~8 colunas das bordas de cada variante do atlas traziam a cratera do outro lado: até 15 cm, escuro e claro.
    - Girado no caderno, virava uma linha fina na borda do carimbo, a ~7 m do centro, desde a v2. Com o sol a 8°, pontilhados de ~2 m em volta das crateras.
    - Corrigido no carimbo (`GroundCrater.usf`): a borda vai a zero nos últimos 35 cm; a cratera nunca chega lá. O atlas aprovado não mudou.
    - Antes × depois: `captures/game/v3_chao3/pontilhado_antes_depois.png`.
  - **A curva da trilha** (§5.2; Liu 2009): pelo raio de cada lagarta, de 30 m (nada) a 6 m ou o pivô (inteira), a fita alarga até ~1 m (×1,55) e o sulco vai a ~7 cm (×1,75). A bordinha de fora da curva dobra e a de dentro some (a terra raspada vai pra fora). De ré, o lado de fora inverte.
    - A folga no caderno de 10 bits: o perfil grava a faixa ÷ 1,75 e a bordinha ÷ 2, e o relevo multiplica de volta. O caderno é linear (o alvo da Unreal já nasce com `bForceLinearGamma`, medido texel a texel), então a trilha reta sai igual.
    - A margem do retângulo sujo do relevo cobria só 72 cm do meio da lagarta, e o perfil v3 vai a 85 cm (1,32 m na curva): corrigida.
    - O Look julgou na câmera do jogo (`captures/game/v3d_trilha/06_cena_239s.png`, World_TestGroundV3Dune): "lê bem", sem degrau visível, com o alisamento de 3 m. A trilha virou sulco, com o lado da sombra e o aceso, como a folha C. Falta o Felipe ver.
  - **A garra** (o Look pediu, 16/10): assada só na altura do sol, sem normal, sob a faixa; 1 cm de pico a pico, no passo de 19,39 cm (64 m ÷ 330, sem emenda na volta do U), pelo andado da lagarta (um caderno G16 novo, só no detalhe, +8 MB); some com o frescor².
    - A 8° de sol, a bordinha de 3 cm mais o sulco de 4 cm fazem ~50 cm de sombra, que cobre o fundo do sulco: a garra aparece como dente na borda da sombra, discreta (`captures/game/v3_garra1/garra_contraste.png`).
    - Ajuste sem compilar: `rts.Ground.ClawCm` (0 desliga). O Look vai olhar parado e em vídeo, por causa da cintilação.
  - **O halo revirado de 50 cm** (Prose 1985): no A do relevo (o revolvido), de 0,5 a 0 além da faixa e da bordinha; a MF do Look faz o tom e a rugosidade (`captures/game/v3_garra1/halo_antes_depois.png`).
  - **O custo na cena de trilhas do Look** (`trilha_arcos_groundv3dune`, alta, 9.000 quadros, antes da garra): 27,0 ms com a v3 contra 26,7 ms sem, +0,3 ms. O Look tinha medido +0,6 antes do contexto curto: sem cratera a até 7 m do lado do sol, a varredura da trilha começa só 1,4 m antes. `rts.Ground.SunLitJobs 0` desliga a varredura, pra medir separado do material.
  - **As mips dos cadernos** (o Look pediu, 16/10): o SunLit e o relevo da v3 nascem com a cadeia de mips, refeita por compute só no que mudou, até a mip 6, uma vez por quadro (`GroundMips.usf`). A MF do Look lê o SunLit com o Sample normal; o relevo ela troca de 4 leituras à mão pra 1 Sample. A cratera fica sem mip por enquanto.
    - O custo, A/B no mesmo binário (`-Cvars 'rts.Ground.Mips 0'`): com os dois cadernos a cada quadro, +0,4 ms (35,2 × 34,8). Com um caderno por quadro (o detalhe e o meio alternados) e até 4 retângulos, nenhum medível (34,5 × 34,6).
    - O `game.ps1` ganhou `-Cvars 'nome valor, ...'`, que entra no `-ExecCmds` (pros A/B de qualquer sessão).
  - **A v3 liga pelo mapa** (o Look e o WorldForge pediram, 16/10): o mapa com `"ground_v3": true` no `Maps.json` liga a v3 sozinho (o World_TestGroundV3 e o World_TestGroundV3Dune já marcados; o TestSalt2 novo, quando o WorldForge remontar com o GROUNDV3); `-RTSNoGroundV3` desliga pro A/B, e `-RTSGroundV3` segue ligando em qualquer mapa.
  - **A luz da trilha velha:** o sulco perde até 45% em minutos, mas a luz só era refeita onde havia marca nova. Agora uma volta lenta refaz um ladrilho de 512 a cada 0,5 s, com a fila vazia (o detalhe inteiro em ~32 s, o meio em ~8 s). O sol que muda agora também é conferido com a fila vazia.
- [~] **As cenas de teste:**
  - a câmera que anda (`camera.keys`);
  - a ordem pra uma unidade só (`"unit"`);
  - o prédio num ponto fixo (`"structures"`);
  - a câmera da cena mais perto que o zoom do jogador (o Fluxo, pros game shots);
  - os 24 cenários da poeira da Arte (`poeira_*`);
  - o `"map"` da cena agora troca o mapa (os mapas de teste do WorldForge: a laje, a areia v2, o chão v3). Antes era lido e ignorado: o skirmish abria sempre o World_TestSalt2 (o Look e o WorldForge acharam, 16/10).
- [~] **As lajes da estrada do WorldForge com a memória do chão** (`PushToSlabs`), pra esteira e cratera aparecerem na estrada.
- [x] **Medido e descartado:** o TSR em FP16 (não ganha na 1660).
- [x] **O viés −0,5 das sombras no Alta, de novo com a água filtrada** (16/10, o Felipe escolheu: "ligue no alta"; em `DefaultScalability.ini` [ShadowQuality@3]): −0,8 ms (32,2 × 33,0 ms). A água ficou quase igual, com as listras da sombra um pouco mais riscadas no ampliado (`captures/game/ab_agua_b05.jpg` e `ab_agua_b05_zoom_a.jpg`). Não liguei: é escolha de gosto do Felipe, um A/B pra ele.

### Mecânica
- [~] **A defesa de base da USA (a Patriot)** (14/10 de madrugada, a noite da v3; o Felipe aprovou o modelo: "top, manda pros cara botar no jogo"):
  - **Os números são do ZH**, de `AmericaPatriotBattery`, `PatriotMissileWeapon` e `PatriotMissileLocomotor`, × 0,2633 m:
    - custa 1000, leva 25 s e gasta 3 de energia;
    - tem 1000 de vida e vê a 95 m;
    - dispara 4 mísseis de 30 (explosão) a cada 250 ms e recarrega em 2 s;
    - o alcance é 59 m, o míssil voa a 105 m/s e a torre gira a 180°/s;
    - exige usina pronta e, sem energia, fica parada (o POWERED).
  - **A simulação 13:** prédio que atira e nunca persegue; a rajada (`clip_size`, `clip_reload_s`); o `needs_power`; prédio fora de ordem de andar.
    - O simtest prova com e sem energia: 16 tiros matam o tanque e ela não sai do lugar; sem energia, zero tiros.
    - As 3 compilações dão o mesmo FINAL_HASH, 0a533cde3fa59f62.
  - **O modelo:** `game.ps1 patriot` importa para `/Game/Structures/USA_Patriot`. A caixa lançadora é a torre e o radar gira, parando sem energia.
  - **No jogo:** ela está no menu do Dozer, com o botão `usa_missile_defense` do kit da Arte. A cena de teste é `patriot_defesa`.
  - **A IA:** monta a 1ª defesa depois da fábrica, a 45 m da base rumo ao inimigo, e a 2ª com $3000 sobrando. A defesa não entra mais na onda de ataque, e a onda que cai para 2 tanques volta a juntar, com a próxima 2 tanques maior (até 14).
  - **O que ficou:** robô contra robô com defesa termina empatado em 15 min, porque os depósitos secam e a IA não vai buscar outro (a do ZH vai). É item da IA pra v3. O tiro antiaéreo entra quando houver avião.
  - [~] **16/10 (sessão do jogo), a IA expande:** com os depósitos perto da base secos (nenhum com caixa ao alcance de 184 m do caminhão de nenhum centro dela), ela faz um centro novo no depósito com caixa mais perto de casa, até 3.
    - Enquanto espera o centro, a fábrica para de gastar (com a fila de 2 tanques sempre cheia, o dinheiro nunca chegava no preço).
    - O caminhão parado sem depósito vai pro com caixa mais perto dele, do lado de casa primeiro.
    - Ela faz 2 caminhões por centro que ainda tem depósito.
    - **Simulação 14.** O teste novo (os depósitos da base com 60 caixas, sem guerra, 20 min) dá 3 e 2 centros, a renda de 11.700 a 41.700 e 48 e 34 tanques; as 3 compilações dão o mesmo FINAL_HASH.
    - O empate de 15 min com a defesa continua: ali os depósitos ainda não secam em 15 min, e o empate vem da guerra (as ondas contra as Patriots), não da economia. É o próximo item da IA.
- [ ] **Rede:**
  - **Conexão:** transporte UDP (LAN e IP direto primeiro) e sala de espera (vagas, cor, time, mapa).
  - **Mesma partida nos dois PCs:**
    - aperto de versão: executável, `Units.json`, grade e mapa;
    - a semente e a montagem vêm do anfitrião;
    - os dois começam no mesmo passo.
  - **Ritmo:** atraso de comando adaptativo e sequência por jogador nos comandos.
  - **Dessincronia:** troca da impressão digital e relatório de dessincronia.
  - **Queda:** quem cai é esperado.
- [ ] **Critério de pronto da rede:** 30 min sem dessincronia a 0, 100 e 200 ms com 2% de perda; dessincronia forçada pega em ≤ atraso + 1 passo, com o sistema apontado; clique→imagem medido (~200–250 ms na LAN).
- [ ] **Segurança e ritmo:** cifra própria com chave combinada na sala (LAN e IP direto não têm relay); se um PC não aguenta, a partida desacelera e mostra quem atrasa; quem cai tem espera de até 60 s e votação.
- [ ] **Carregamento em rede:** uma barra por jogador na cor dele; só começa com todos em 100%.
- [ ] **O resto do revisor do juiz:** faixas no `ReadSetup`, script que recusa float/double/FMath no SimCore, clang-cl no simtest; com o 2º PC, o mesmo hash em Intel e AMD.
- [ ] **Como o 2º PC recebe o conteúdo:** o mapa (262 MB), as fatias e os assets por link estão fora do git (o `Game/Content` tem 2,3 GB e só 2 arquivos versionados), e o GitHub recusa arquivo acima de 100 MB: pacote cozido ou Git LFS.
- [ ] **Câmera em rede:** o Alt+meio desce até 12° (cinematográfico); travar na faixa de jogo no multiplayer?
- [~] **Pausar, retomar, render-se e sair viram comandos da rede** (13/10, sem o teste do Felipe): a pausa é o anfitrião parar de fechar passo, então todos param no mesmo; render-se é o comando `Resign`; quem sai ou cai é rendido pelo anfitrião.
- [ ] **HUD pra N jogadores.** Hoje é "Seus tanques / Inimigo", com a cor do jogador 1 fixa.
- [ ] **Grade igual nos dois PCs:**
  - conferir o hash da grade na sala de espera;
  - plano B: o anfitrião manda a grade (~0,4 MB);
  - a versão final é a grade canônica do WorldForge (seção 4).
- [ ] **Como testar com uma placa só:**
  - 2º jogador sem placa (`-nullrhi`) ou a simulação pura fazendo o jogador remoto. Duas Unreal com janela derrubam a 1660 (`docs/GPU.md`).
  - Pro Felipe testar clicando: 2º PC, 1º jogo empacotado (marco M1c, nunca feito) e o aviso do firewall, que só ele aceita.
- [~] **War Factory na simulação** (feito 10/10, `docs/ECONOMIA.md` Parte 5):
  - prédio com área ocupada na grade, vida, dono e armadura de estrutura;
  - posta na base de cada jogador (as largadas do `Maps.json` hoje nem são usadas).
- [~] **Fila de produção** (feita 10/10 com custo, como no ZH; a porta e a saída pela faixa da fábrica):
  - comandos pra produzir, cancelar e marcar o ponto de encontro;
  - tempo por tanque (conferir o BuildTime do Crusader no ZH 1.04) e teto de fila;
  - a porta abre antes, o tanque nasce dentro e sai até a frente (UnitCreatePoint / NaturalRallyPoint no ZH), e espera se a saída estiver bloqueada;
  - a saída cai na região ligada;
  - bandeira e linha do ponto de encontro com a fábrica selecionada;
  - porcentagem da produção.
- [~] **Fábrica destruída** (feito 10/10): para de produzir e **devolve o dinheiro da fila** (ZH `ProductionUpdate.cpp:1130`; a nota antiga "perde a fila" estava errada, revisão de 10/10).
- [~] **Vitória com base** (feito 10/10, a regra do ZH: perde quem fica sem prédio). Com fábrica produzindo sem economia, a partida pode não acabar.
- [ ] **IA com fábrica:** usa a fila e o ponto de encontro. No multiplayer, roda no anfitrião mandando comandos como um jogador. Decidir se a base tem vagas de IA.
- [~] **Carcaça com física** (o Felipe, 13/10: "a carcaça não tem que ter opção, tem que ter física"; feito no mesmo dia, sem o teste dele). Ela fica no mapa e não tem opção na sala:
  - **Na simulação** (versão 12, igual em todo PC): o veículo morto vira carcaça no lugar.
    - O vivo que encosta empurra; ela pesa o mesmo que o vivo, então quem empurra anda à metade da velocidade.
    - A batida nunca é reta (±9°), então ela gira e escorrega pro lado. No teste, andou 5,7 m e girou 18°, e o tanque passou.
    - Contra penhasco ou prédio ela trava, e aí segura o tanque.
    - O casco é dois círculos no eixo, e cabem até 256 carcaças no mapa.
  - **No desenho:** o modelo, o fogo e as brasas vão junto, e a carcaça arrastada risca o chão.
  - **Os testes:** `simtest` (`[carcaca]`) e o cenário `carcaca_empurra`.
- [ ] **Vegetação baixa** amassa ou dobra quando o tanque passa (jogo + WorldForge, dono do material da vegetação).
- [ ] **Visão pelo relevo.** Se o morro tapar a visão, a simulação precisa da altura por célula, e isso entra no pedido da grade ao WorldForge. Se a visão for um círculo como no ZH, não precisa.
- [ ] **Direção do tiro** pros efeitos na blindagem: o desenho tira da posição do atirador, sem mexer na simulação.
- [ ] **Separar o impacto:** "acertou tanque vivo" contra "caiu no chão", e o efeito pelo tipo de chão (areia, rocha, asfalto; água dá respingo). Hoje os dois são a mesma esfera.
- [ ] **Ligar no jogo o que os efeitos novos pedem:** o plugin Niagara, o ProceduralMeshComponent e o Json (fogo do FXLab, destroços do BlastLab) e um módulo de shader global carregado cedo (raymarch).
- [ ] **Evento único, como o FXList do ZH:** efeito, pulso de luz, tremor, som e marca saem do mesmo evento; clarão em t = 0, som e tremor em d/343 s do ponto de mira.
- [~] **Ctrl força o tiro, como no C&C clássico** (pedido do Felipe, 13/10 de manhã; feito na mesma manhã, sem o teste dele): segurando Ctrl, o clique manda atirar em qualquer coisa:
  - no chão, que cava a cratera da arma onde o tiro cai;
  - na própria unidade, no aliado ou na casa civil.
  - Serve também pra ele ver a explosão e a cratera.
  - É comando da simulação, em ponto fixo e igual nos dois PCs (o ataque ao chão é um alvo-ponto). O cursor de mira muda com o Ctrl.
  - **Feito:**
    - os comandos `AttackGround` (o ponto) e `ForceAttack` (qualquer dono), na simulação versão 9;
    - no simtest, o colega leva tiro em 4 passos, e o tiro no chão chega 8 vezes no ponto sem ferir ninguém, com o mesmo hash nas 3 compilações;
    - no jogo, Ctrl + clique esquerdo: na unidade ou no prédio, de qualquer dono, ataca; no chão, atira no ponto até mandarem outra coisa. O tiro no chão cava a cratera da arma;
    - o cenário de teste é o `tiro_chao`.
- [ ] **v3, pedidos de 16/10 de manhã (o Felipe, antes de dormir: "pra fazerem depois que acabarem o que precisam fazer"; ele não estará pra decidir, então fazer como na noite de 16/10: estudo com fontes, 2ª opinião, prova no jogo lado a lado, A/B pra ele escolher, registrado aqui).** A regra dele pra todos: "tudo que tem um pouco mais de complexidade tem que ser construído pensando num jogo AAA que vai deixar arrepiando, a beleza, a construção e tudo relacionado".
  1. **As torres de defesa da base funcionando, atacando inimigos:** bem pensadas, construídas, estruturadas, pesquisadas, analisadas e discutidas. (A Patriot já existe no jogo; vale pra ela e pras próximas defesas.) **Dono:** o jogo (a regra na simulação, a IA, o alvo); o Fluxo (o disparo, o míssil, o acerto); a Arte (o HUD e os ícones).
     - **O Felipe, 16/10 às 10h:** "ele não atira projéteis igual tank, ele atira mísseis, mísseis projetando até fumaças e com explosão quando encosta no tanque, precisa pensar nisso, simular bem cada coisa; o BlastLab tinha simulado já fumaças na mão bonitas, mas tem que melhorar". **O jogo:** o míssil de verdade na simulação (sai da caixa, acelera, faz a curva atrás do alvo e explode no contato), com a posição de cada míssil a cada passo pro desenho seguir. **O Fluxo:** o corpo do míssil, a chama, o rastro de fumaça e a explosão no tanque.
     - **O Felipe, 16/10 à tarde (de novo):** "não esqueça que o Patriot solta mísseis com fumaça e não borrões brancos igual tiro de tanque ... o BlastLab fazia um míssil modelado bem provisório com uma fumaça gerada atrás bem simulada, bem bonita, que era até interagível com aviões: onde passava empurrava a fumaça; mas eu não tô falando pra pegar nada de lá nem enviesar, tô falando pra ser inteligente, fazer algo bem pensado, bem estruturado, bem consultado, bem inspirado, algoritmos, matemática, bem simulado, bem gerado".
       - **O rastro tem de ser fumaça simulada**, que espalha, deriva com o vento e é empurrada por quem passa (os veículos e as explosões). Não pode ser um risco branco.
       - **O dono, combinado com o Fluxo (16/10):** o rastro inteiro é do jogo (`RTSMissileSmoke`); o Fluxo fica com a chama, a luz, o lançamento, o acerto, a ogiva e o laser, e desliga as fitas dele quando a fumaça do jogo está ligada (`SetExternalMissileSmoke`).
       - **O jogo, 16/10 à tarde (escrito e conferido no espelho; na compilação 28, sem commit):** o estudo com as fontes em `docs/FUMACA_DO_MISSIL.md`.
         - **Os puffs gaussianos** soltos a cada 0,45 m do caminho da simulação. A fumaça por metro é a vazão / a velocidade (5,5× mais densa perto da caixa).
         - **A física:** o jato que abre; a subida do gás quente (a térmica em linha de Briggs); o vento do mapa no perfil logarítmico; os redemoinhos grandes sem divergência; o espalhamento de Richardson–Obukhov. A densidade cai sozinha, pela massa que se conserva.
         - **Quem empurra:** o tanque (o ar em volta do casco e a esteira), o tiro e o outro míssil (o túnel), a ogiva e a morte (a bola de fogo abre a fumaça e o anel sobe e suga) e a boca do canhão.
         - **A luz na unidade da cena:** o sol e o céu de verdade; a sombra de um puff no outro; o contraluz; a sombra no chão tão escura quanto a dos prédios.
         - **Na frente de uma unidade,** a fumaça fica com no máximo τ 0,5.
         - **A nuvem branca da ré da caixa** na largada.
         - **A vida pelo preset:** 5, 8, 12 e 16 s. `-RTSNoMissileSmoke` volta pras fitas, pro A/B.
       - **O 1º vídeo (compilação 28, `fumaca_atravessa1`, 14:13):**
         - **Já lê como rastro de verdade:** puffs com borda em couve-flor, curva e deriva.
         - **Os defeitos:**
           - virou cortina em cima das duas baterias e do nosso tanque aos 6–8 s;
           - a luz saiu chapada (o lado de costas pro sol não escurecia);
           - a sombra no chão não apareceu (o quadrado ficava debaixo das dunas de verdade);
           - custou 4,5 ms de CPU com 1.635 puffs.
       - **A v2 (compilação 29):**
         - menos densa (τ 1,6; até 3× perto da caixa) e mais curta (3; 4,5; 6; 8 s pelo preset, a turbulência acelerando de 2 a 5 s);
         - a nuvem da largada menor; na frente de unidade e de prédio, no máximo τ 0,25;
         - a sombra dentro da espessura do rastro;
         - a sombra no chão pela profundidade da cena, que cai certa nas dunas e escurece o tanque que passa por baixo;
         - os puffs com sorteio de lado e de tamanho;
         - a luz em meio quadro e o custo de cada parte no log.
       - **A v3/v4 (compilação 30, 15:14; os materiais consertados às 15:47):**
         - o sol lido da luz do mapa pela CPU (o log: elevação 8°, (72 323; 31 702; 8 516) lux);
         - a malha mais barata;
         - `rts.MissileSmoke.Debug` 1/2.
         - **O conserto dos materiais:** das 15:14 às 15:47 eles não compilavam (um `.w` num parâmetro float3), e quem testou míssil nessa janela viu a grade cinza.
         - **A prova da sombra (`fumaca_sombra`, o rastro de través ao sol):** ela cai ao lado do rastro, esticada pelo sol baixo. Na força normal é sutil (o céu do fim de tarde enche a sombra); com `Debug 2`, 4× mais escura, fica clara. As imagens: `captures/game/entrega_v3/fumaca_sombra_normal_x_4x.jpg`.
         - **O custo medido:** 2,3–3,2 ms de CPU com ~1.300 puffs no ultra (a malha 0,9–1,9). Ainda alto: o passo seguinte é a simulação na GPU.
       - **A compilação 31 (17:21), pelo teste do Fluxo (patriot_v2):**
         - **A caixa da Patriot estava ao contrário:** o modelo tem as bocas no −X da malha. A ficha visual ganhou `turret_yaw_offset_deg` (180 na Patriot), e o míssil, as tampas e o clarão saem pela boca.
         - **A leitura pela soma:** o limite de τ 0,25 valia por puff, e o rastro virava um banco de nuvem em cima dos tanques. Agora o τ é somado no raio da câmera até cada unidade e rareia na proporção. No `patriot31` os tanques ficam limpos; a bateria aparece por baixo da nuvem da largada.
       - **A luz do mundo (16/10 à noite, compilações 33–35; sem commit), pela regra nova do CLAUDE.md** (toda fumaça tem volume, recebe a sombra do mapa, faz sombra e é acesa pelo fogo e pelas explosões):
         - **As luzes perto** (o `RTSSceneLights` do Fluxo: as explosões, o fogo, os tiros):
           - somadas por puff a cada quadro, com a mesma conta que acende o chão;
           - vão pro material na cor do vértice (RGBE), com a direção na normal;
           - a mesma fase e as mesmas oitavas do sol.
           - O "calor da explosão" que brilhava sozinho saiu.
           - **Medido no `fumaca_predio2_dbg3`:** até ~296 mil lux num puff, 4× o sol; milhares de puffs acesos acima de 1% do sol.
         - **A sombra do mapa** (o `RTSSunShadow` da Arte, trazido pro jogo na 33):
           - lida por pixel na fumaça, multiplica o sol em todas as oitavas;
           - a sombra da fumaça no chão não escurece de novo o chão que já está na sombra (a sombra dupla).
           - **A prova:** `captures/game/fumaca_predio2_dbg3`, com o `rts.MissileSmoke.Debug 3`, onde vermelho é sombra. A nuvem da largada no pé da Patriot, dentro da sombra da usina, fica vermelha; o resto do rastro, ao sol.
         - **A ponte do jogo pro mapa do sol:**
           - todos os veículos por `SetVehicles` a cada quadro: lagarta, roda e carcaça, com a caixa pelas malhas;
           - os prédios prontos e já vistos por `AddShadowCaster`;
           - as áreas que mudam por `RefreshShadowArea`: a palmeira e o poste depois de deitar, o carro depois de amassar, a pedra partida.
       - **A v6 do visual (17/10 de madrugada, compilação 44 + material; sem commit; o padrão é a v6, a v5 fica no `rts.MissileSmoke.Look 5`):**
         - **O porquê:** o Felipe pediu um vídeo de lado ("a fumaça tá ruim de ver nessa imagem, bem ruim"), e o vídeo mostrou uma parede branca larga e lisa.
         - **O estudo do detalhe** (fontes no `docs/FUMACA_DO_MISSIL.md`) achou três causas:
           - o ruído próprio de cada quadrado, girando, que vira média com ~5 sobrepostos;
           - a normal de esfera lisa;
           - o céu igual em cima e embaixo, e o "pó" do lado errado.
         - **A v6:**
           - o ruído preso ao mundo, no plano da vista e levado pelo vento: os quadrados sobrepostos leem o mesmo detalhe;
           - a normal das bolotas grandes (o gradiente do ruído);
           - a erosão de borda pelo limiar;
           - o céu pelo hemisfério da normal, com o chão embaixo;
           - o miolo grosso aceso como superfície;
           - o rastro com 3/4 da vida e menos engorda.
         - **As voltas:** a 6a riscava, com o ruído no chão esticado nos quadrados em pé; a 6b/6c ficavam em fiapos, com o ruído fino serrilhando sem mip; a 6d ficou boa.
         - **A prova:** `captures/game/entrega_v3/fumaca_missil_v5_x_v6.jpg` e os vídeos `fumaca_missil_de_lado_v5_antes.mp4` e `_v6_nova.mp4`. A v6 sai mais fina, com bolotas sombreadas e os tanques limpos; ainda é mais macia que a couve-flor do vídeo de referência.
       - **O diesel (16/10 à noite, compilações 34–36; sem commit):** o estudo e o plano em `docs/DIESEL_DO_TANQUE.md`.
         - A fuligem é um tipo de puff na mesma fumaça, com a física, a luz, a sombra e a leitura.
         - **O motor de cada veículo:** o acelerador, o turbo que atrasa e as marchas. A baforada sai na arrancada e nas trocas.
         - **A ficha** (`visual.exhaust` no Units.json): o tanque, o Dozer (as duas chaminés) e o caminhão.
         - **As voltas até aparecer (compilações 36–40):**
           - a gravação começava depois da arrancada;
           - o cruzeiro estava no número de um caminhão moderno;
           - a cena tinha os veículos indo pra longe da câmera, e a regra de leitura cortava a fuligem que cobria a traseira deles (o registro mostrou o τ de 2,4 virando 0,57);
           - a esteira do próprio veículo dilui o gás em ~0,5 s.
         - **Hoje:** o diesel velho e sujo (k ~8 na baforada, ~1,7 no cruzeiro [E]) e o albedo medido do diesel (0,20).
         - **A prova:** `captures/game/diesel_lado2`, com os veículos cruzando de lado. Atrás do Dozer sai a nuvem marrom-escura da fuligem com a poeira; o tanque na sombra da usina solta a fumaça acinzentada, acesa só pelo céu.
         - **A leitura honesta:** com o sol de 8° deste mapa a areia fica fraca (o sol rasante), e a fuligem, que é volume, pega o sol cheio. Ela lê marrom-escura, não preta como no vídeo de referência. Mais preta que isso é escolha de gosto, fora da física.
         - **A decisão do Felipe (17/10, ~01h):** "já que os tanques tecnológicos e o trator da USA não têm diesel, então pode tirar; a gente foca nisso quando chega nos tanques mais pesados a diesel". Saíram os três `visual.exhaust` do Units.json (o tanque, o Dozer e o caminhão).
           - O sistema fica no código, parado sem ficha.
           - Volta com a China e a GLA, com a fuligem que se vê (mais preta e por cima da poeira); ele disse que não via ("só vejo poeira atrás").
     - **Feito no jogo (16/10, a simulação 15, compilado às 10:31; sem commit):**
       - **O voo:** o míssil sai de uma das 4 caixas inclinado 35° a 20 m/s, as 4 da rajada em leque; acelera a 120 m/s² até 110 m/s; guia a 200°/s mirando onde o tanque vai estar; explode no contato.
       - **O teste fora do jogo:** 16 voos, ~0,7 s cada, subindo ~5 m, 16 acertos; igual nas 3 compilações.
       - **A ponte pro desenho:** `GetMissiles()`, cada míssil interpolado no quadro.
       - **O provisório:** um risco quente por passo, até o desenho do Fluxo, que já propôs `MissileLaunch`, `MissileMove` e `MissileHit` (o item 4 dele, depois do tiro na areia).
       - **A cena:** `patriot_missil`, no World_TestSandV2Dune.
  2. **O tanque passando por cima dos carros civis na cidade e esmagando**, amassando, "bem bonito e de arrepiar" (o esmagar do ZH, `CrushableLevel`). **Dono:** o jogo (a regra na simulação, em ponto fixo, e o evento); o Look (o carro amassado: a malha e o material da carcaça, achatado e torcido); o Fluxo (o som visual: vidro, poeira, faísca do metal).
  3. **O tanque passando por árvores, arbustos e pedras:** derruba a árvore, quebra ou amassa; amassa o arbusto no chão; quebra a pedra pequena ou sobe por cima da grande. **Dono:** o jogo (o evento e o "derrubado" na simulação); o WorldForge (as árvores, os arbustos e as pedras do mapa, a queda e o tombo); o Look (a vegetação amassada e deitada, o capim que fica morto deitado na trilha).
  4. **Cada chão com o seu atributo, sob o tanque** (o pedido de 16/10 continua aqui): a areia macia, empurrada e desabando; a terra bem marcada, com a graminha morta deitada; o barro bem moldado; **o asfalto: a lagarta não afunda e não levanta areia; deixa a marca de borracha** (a análise de como o asfalto marca de verdade: a borracha escura nas curvas e no pivô, a poeira assentada que a lagarta limpa, o rastro de areia trazida de fora). **Dono:** o jogo (o perfil e o que levanta por tipo, pelo `surface.png`); o Look (a cor e a rugosidade de cada marca no material, a borracha no asfalto); a Arte (a poeira por tipo: nenhuma no asfalto limpo, já para no asfalto).
     - **Andamento (16/10, manhã):** o formato combinado entre o jogo e o Look.
       - **O jogo (escrito, compilação pendente):** o sulco, a bordinha e a garra por tipo, pelo surface.png (areia 1, terra 0,5, pedra 0,06, asfalto 0; o molhado vira barro). No asfalto R = 0,5, sem sulco. O B do relevo vira a borracha em todo chão (0,3 no reto, até 1 no pivô). `rts.Ground.Surface 0` desliga.
       - **O WorldForge:** a entrada `UseRubber` no material (padrão 0).
       - **O Look:** a borracha no code_v3 do `look_mf.py` (albedo ~0,035, rugosidade 0,6, some com a idade), onde a Surf diz asfalto e na laje. Liga quando o jogo puser UseRubber = 1 com o B novo.
       - **Falta:** a prova no jogo depois da compilação, a areia trazida de fora pro asfalto (depois) e os blocos da sapata na borracha.
     - **O jogo, 16/10 (os itens 2, 3 e 5):**
       - **O estudo:** com o fonte do ZH e a 2ª opinião, em `docs/ESMAGAR_E_DERRUBAR.md`.
       - **A simulação 16** (testada fora do jogo, igual nas 3 compilações):
         - o tanque esmaga o carro e derruba a palmeira, e passa por cima no caminho; o caminhão e o Dozer contornam (o Dozer derruba palmeira);
         - o freio leve: 85% voltando em 0,5 s;
         - a palmeira cai pra frente e desvia do prédio.
       - **O provisório do desenho:** o carro achata e torce; a palmeira cai pela vara que tomba e fica deitada; o tanque dá o tranco.
       - **Pela 2ª opinião:** os postes, as caixas, as pedras e os arbustos ficam só no desenho, porque não mudam o caminho. Vêm depois.
       - **Mais, 16/10 (compilando):**
         - o poste cai pela vara quando o tanque bate, e o fio arrebenta com o clarão azul e as faíscas;
         - as peças pequenas da vila (caixa, barril, pneu, caixa d'água, antena, varal, cerca) amassam sob qualquer veículo.
         - **O desenho:** os dois só no desenho, pelas posições do passo.
         - **O teste esm_carro1 (antes disso):** o carro achatou; o tanque passou por dentro do poste e das caixas, e é isso que estes dois consertam.
       - **16/10 à tarde, com o WorldForge e o Fluxo (sem commit):**
         - **O fio (compilação 28):** o vão do poste derrubado vira fio solto, o plugin `RTSWorldProps` do WorldForge (Verlet: balança, arrebenta acima de 1,5× o comprimento e deita no chão), com a ponta seguindo a cruzeta do poste que cai.
           - **O arco provisório:** o clarão pequeno que pisca e o metal quente laranja caindo, na quebra e na ponta viva no chão, em rajadas como o religador da rede (de novo a +0,6 s e +2,2 s); a marquinha queimada na areia.
           - **O arco final** é do Fluxo (`docs/ESTUDO_ESMAGAR_E_POSTE.md`), trocando o `WireArc`.
         - **A palmeira (compilação 28):** o toco fica de pé e o tronco cai pela quebra a 0,8 m até a ponta tocar o chão; a copa atrasa, chicoteia no impacto e assenta pelo material do WorldForge (CustomPrimitiveData 1–4). A cena é a `palmeira_toco` (World_TestTrample).
         - **A pedra pequena (compilação 29):** a do RockSmall parte no lugar sob o tanque e o Dozer (a `_Broken` do WorldForge), com o pó fino e um tranco. A grande fica.
         - **O carro (compilação 29):** o efeito do Fluxo (`CarCrushStart`/`CarCrushEnd`: o vidro que estoura e pisca ao sol, as lanternas, os pneus, a poeira e o raspão), pela passagem da lagarta; o pisca-alerta em metade dos carros.
         - **O capim (compilação 28):** as plantas da montagem (etiqueta `WorldForgeTrample`) leem o caderno da batalha e a trilha fresca: o WorldForge deita e escurece pelo material (`UseTrample`).
       - **A defesa (o item 1, a simulação 17, compilando):**
         - a ajuda entre as baterias, o AssistedTargeting do ZH, com o laser vermelho provisório entre elas;
         - a defesa larga o alvo que os mísseis no ar já vão matar.
         - **O teste:** duas Patriot, 8 tiros cada, o tanque cai; igual nas 3 compilações.
       - **As cenas:** `esmagar_carro` e `derrubar_palmeira`.
       - **O estado:** compilando, sem commit.
  4b. **O sulco por tipo de chão (o item 4), 16/10:**
     - **A regra:** por pixel, pelo surface.png do mapa. A areia fica como a trilha aprovada; a terra rasa e nítida; o barro fundo e espremido; a pedra quase nada; o asfalto sem sulco, com a borracha no B do relevo (o formato do Look: forte na curva e no pivô).
     - **A tabela com as fontes:** em `docs/CHAO_V3_ESTUDO.md` §12.
     - **O desligar:** `rts.Ground.Surface 0`.
     - **O estado:** compilando, sem commit.
  5. **O tanque derrubando os postes de energia da cidade, com o choque elétrico:** "bem realista, bem pensado, bem colocado, bem desenhado, bonito e realista de AAA, de arrepiar, não algo mal feito". (Já estava pedido em 13/10, item de baixo.) **Dono:** o jogo (o empurrão e o evento); o WorldForge (os postes e os fios com física); o Fluxo e a Arte (o arco, as faíscas e o clarão, pelo EmberGen ou pelo FXLab).
- [ ] **A câmera olha pra cima, como num FPS, na partida** (o Felipe, 16/10: "pode olhar pra cima também, como se fosse um FPS, é o que eu vou permitir na gameplay do jogo, igual como o WorldForge funciona hoje quando seguro o scroll e giro"). Hoje a inclinação livre para em 3° abaixo do horizonte (`MinFreePitchDeg`). **Dono:** o jogo (a câmera). **O Look acompanha:** com o céu inteiro à vista (até o zênite, que antes nunca aparecia), o azul do alto, as nuvens em cima do mapa e o sol entram na conta; a exposição ao deitar e o céu destravado passam a valer pra olhar pra cima também.
- [~] **A torre do tanque morto com física de verdade** (o Felipe, 16/10 de manhã, "pode ser um dos últimos"): hoje ela "voa pra cima, gira estranho, cai estranho e depois fica voando, não cai no chão, não tem física nenhuma". Tem que interagir com o chão e com o que estiver em volta, cair, quicar, rolar e parar, como no mundo real. **Dono:** o jogo (o arremesso da torre é o `RTSUnitActor`); o Fluxo (o rastro de fumaça e as faíscas da torre voando, `BlastDebris.cpp:610`).
  - **O jogo, v4 (23/10, o Felipe de novo: "ela só voa e fica voando dps"):** a torre virou um corpo rígido de verdade (`RTSLooseTurret.h/.cpp`: impulsos sequenciais do Box2D, a inércia da caixa da torre, o chão do mapa assado sob cada ponto, a restituição que cai com a velocidade da batida e o atrito da areia), com o pivô no centro de massa (o rastro de fuligem sai do corpo), a marca no chão nas batidas fortes e o afundar a partir de onde ela parou. A cena `torre_fisica` (Scenarios.json) e o A/B `-TurretPhys=0` (o voo da v2). Da revisão: parada encostada num obstáculo, ela acorda e cai quando ele mexe ou some, em vez de ficar no ar; a carcaça tirada logo depois da morte espera a torre parar pra afundar com ela; a marca da batida de uma morte que ninguém viu espera a carcaça aparecer. **O estado:** no código, sem compilar nem provar no jogo, sem commit. O sopro de poeira em volume da batida é a fase 2 (no BlastFx).
- [ ] **Pra depois, quando for o foco (o Felipe, 16/10 de manhã; a conversa no Look):**
  - **O prédio que se degrada e desmorona pelo lado do dano:** o lado que tomou o tiro queima e cai primeiro; o míssil no canto derruba aquele canto; a poeira e a fumaça do desabamento bem bonitas. "Bem realista ... pra arrepiar, e não de qualquer jeito". Depende do modelo dele (feito pra fraturar: paredes e lajes em partes, interior); o caminho técnico é o prédio pré-fraturado (Chaos), o dano pelo ponto do impacto e a fumaça gravada (EmberGen). Conversar com ele quando chegar o modelo.
  - **A água de filme:** o tiro na água, as explosões e a nuke na água, os navios. "Um fluido MUITO TOP ... efeitos de filme". Entra na etapa da água (a física e a mecânica dela). O caminho dos AAA: a superfície reagindo em tempo real e as explosões grandes gravadas; a simulação de líquido pede uma ferramenta nova (o LiquiGen da JangaFX ou o Houdini), decisão e custo dele.
  - **O WorldForge como criador de mundos com o clima e o céu:** a hora do sol, o céu, as nuvens, a tempestade, a chuva, por mapa, como um criador de mundos de RTS. Sem tempestade no meio da partida (decisão dele). Depois do primeiro mapa jogável.
  - **Civis e pássaros:** provisórios por script estão aprovados ("pode ser provisório o civil, não tem problema, está aprovado"). Os civis correndo da batalha, os pássaros levantando voo com a explosão.
  - **As marcas que contam a batalha (o Felipe: "faça então", 16/10; o estudo em `docs/MARCAS_DA_BATALHA.md`):** a ordem: 1) a batalha no minimapa e na tela do fim (jogo, ~0 ms); 2) os furos da memória (jogo); 3) a textura do mapa inteiro com o caminho batido, o combustível, o queimado largo e a densidade de crateras (o jogo grava, o Look lê no material); 4) a carcaça pegando poeira com a idade (Look). **Perguntas pra ele:** a ruína do prédio andável e lenta ou bloqueada? A carcaça pode ser destruída? O Dozer limpa carcaça e entulho? Acumulam durante a partida (as crateras, o queimado e as trilhas já existem; faltam as carcaças empurráveis, o entulho, as cápsulas e os destroços que ficam até o fim). O mapa começar destruído seria uma opção do WorldForge, outra coisa.
    - **O jogo, 16/10 (compilado às 12:09; sem commit):**
      - **O caderno de acúmulo do mapa inteiro** (`GroundBattle`, RGBA8 2048² a 50 cm, com `BattleBounds`, no Landscape e nas lajes):
        - R o tráfego, 1/32 por passada;
        - G o combustível fresco, desbotando em ~5 min;
        - B o queimado largo;
        - A a densidade de crateras.
      - **P1:** a batalha no minimapa, uma camada a cada segundo por cima da foto; só o que o jogador viu.
      - **P2.1:** o tiro no chão e o acerto no casco na névoa ficam guardados e aparecem quando a área é vista.
      - **A cena:** `batalha_marcas`, 8 contra 8 por 60 s; no fim ela grava o caderno e a camada.
      - **Falta:**
        - a tela do fim: feita (compilações 28 e 30). O campo de batalha fica à direita do resultado, acima do painel do HUD: a foto do mapa com as marcas. A prova: `patriot_fim2`.
        - o grosso no chão (P2.2);
        - a leitura do Look no code_v3 (depois do céu);
        - o resto dos furos (P2.3 a P2.5).
  - **O céu (os testes de 16/10):**
    - **A sombra das nuvens:** "n vi nada" (o Felipe). Não era a ligação, era o tamanho: a mancha de 150–220 m é maior que a tela do jogo (~95 m), então a tela inteira ficava na luz ou na sombra.
      - O ZH (código liberado) usava ~83 m andando a 3 m/s.
      - Em teste (Look): 60 m e 40 m, 30% de área na sombra, 5 m/s, com vídeo (`docs/CEU_E_NUVENS.md` §11.2).
    - **O sol na tela:** o −1 e o −1,6 EV não bastam (15% estourado no melhor; o chão de costas pro sol escurece) e o disco do sol não aparece.
      - Em teste (Look): a exposição local comprimindo só o céu com a câmera deitada.
      - Dois testes pra achar o que tapa o disco (a nuvem ou a névoa).
    - **A vista acima das nuvens:** mostra o mapa como ilha no vazio e pede terreno até o horizonte (WorldForge).
- [ ] **v3: o tanque derruba poste e árvore** (pedido do Felipe, 13/10 de manhã: "depois que acabar tudo"; fora da v2, a decisão dele à noite):
  - o tanque empurra e derruba os postes e as árvores (as palmeiras);
  - o WorldForge vai criar os fios com física, então o poste que cai puxa o fio, que balança e arrebenta;
  - "física bonita e efeito de choquinho bem bonito": as faíscas e o arco no fio que arrebenta. O efeito de energia, realista, pode vir do EmberGen ou ser pedido à sessão do Look.
  - **A divisão:** os fios e as peças são do WorldForge; o empurrão, a queda e o evento na batalha são do jogo.
  - **O cuidado:**
    - a queda é desenho (Chaos), igual em todo PC pelo evento;
    - o "derrubado" que muda o caminho (o tanque passa por cima, como o esmagar do ZH) é da simulação, em ponto fixo, senão a partida diverge na LAN.
- [x] Simulação determinística, skirmish, colunas do grupo e nascer fora de platô isolado (commit `e7de5ce`).
- [x] **Tela de carregar que espera a cena pronta** (t7: 4,1 s) e a correção do teste do resultado (commit `e927a02`).

### Visual feito no jogo
- [ ] **Cor do jogador no tanque:**
  - máscara no material e cor por dono; a máscara vem da arte;
  - o `M_UnitPBR` é compartilhado com o BlastLab: combinar, reimportar com o app fechado e respeitar a trava da placa;
  - a carcaça perde a cor.
- [ ] **O raio-X do tanque só quando ele some de verdade** (o Felipe, 16/10 à tarde, vendo as gravações: "qualquer coisinha que o nosso tanque anda já renderiza aquele xray azul ... até mesmo uma pequena duna de areia, arbustos no chão, o que estraga um pouco o visual bonito da gameplay; ... só exibir raio-X se o nosso tanque ficar completamente oculto atrás de algo, se a visibilidade dele ficar bem fraca"). **Dono:** o jogo.
  - **O plano:** a cada ~0,15 s, a fração escondida de cada tanque seu ou aliado, por raios da câmera até 8 pontos do casco, contra o chão, as casas, os prédios e as copas. Capim, arbusto, poste e caixa não contam. O contorno liga com ≥ 70% escondido e desliga abaixo de 50%, sem piscar.
  - **Feito (compilação 27, 13:01; sem commit):** na duna (`poeira_rec_lado`) e na rua da vila (`vila_rua`) o contorno não acende mais.
  - **A prova (15:15):** o tanque escondido inteiro atrás do sobrado acende o contorno (`raiox_atras_baixa3`, câmera a 35°); o da rua, à vista, não. Com a câmera alta (55°) o mesmo tanque aparece por cima da casa e fica apagado. As imagens estão em `captures/game/entrega_v3/raiox_*.jpg`.
- [ ] **Seleção final:**
  - anel fino no chão, desenhado na cena: o tanque fica por cima do anel, não o anel por cima do tanque;
  - só no selecionado e sob o mouse; inimigo sem anel (só sob o mouse ou quando vira alvo);
  - barra pequena, de tamanho fixo na tela;
  - o tanque selecionado pisca na cor, como no ZH.
- [~] **Traçante** (jogo, 13/10, sem aprovação: curto, fino constante na tela, laranja, nasce 1,5 m à frente da boca; clarão da boca pequeno e laranja, sem as bolas brancas da luz nova. Ainda fraco: o traçante quase some e o clarão é um disco macio, não um estouro):
  - curto e laranja, com espessura constante na tela;
  - nasce um pouco à frente da boca, some no impacto e é aditivo;
  - o ZH tem o efeito Tracer com Speed, Length e Width.
- [~] **Marcas de esteira de verdade** (jogo, 11/10, `RTSGroundMemory`; o Felipe gostou em 12/10, "salva esse modelo": `captures/game/esteira_v1_salva/`; o chão que afunda é do Look, `docs/CHAO_DEFORMAVEL.md`):
  - **Feito:** uma fita por lagarta, com as garras de 17 cm em fase, que some em ~5 min. Ela é riscada num caderno do mapa (50 cm por pixel) e num de detalhe em volta da câmera (4,7 cm por pixel), e um decal só pinta tudo: o custo não cresce com as marcas.
  - **12/10:** aparecem no jogo (cenário rápido "vila"), duas por tanque, com as garras e a curva da manobra. Custam +0,2 ms na mediana e +0,9 no p95, mais ~80 MB de memória de vídeo.
  - **Falta:** a borda de areia levantada; areia marca forte, rocha não marca e estrada marca de leve (precisa do tipo de chão do WorldForge); riscar no escuro e mostrar quando a área for revista; ver em vídeo.
  - **13/10 à noite, o Felipe:** no jogo a lagarta "não modela" (risco chapado ou nada). O alvo é a folha `captures/look/areia_C_final.jpg` (a trilha fresca + a velha cruzando, meio coberta pelo vento). **Dono do visual: o Look** (a MF `look_mf.py` lendo os cadernos do relevo). Prova: no jogo, ao lado da folha. Estado em `docs/CHAO_DEFORMAVEL.md` (o topo). **14/10, madrugada: o Felipe tirou da v2** ("pra v2 não vai ter mais nenhum ajuste, isso aí tudo vai ser pra v3"). Lagarta e cratera no chão vão pra v3 com o plano de relevo por pixel (`docs/CHAO_DEFORMAVEL.md`, a decisão no topo).
- [ ] **Esteira andando:** textura correndo pela velocidade (as rodas já giram).
- [~] **Chão que guarda a batalha** (jogo, 11–13/10, a mesma memória do chão; estudo, revisão e números em `docs/CHAO_DEFORMAVEL.md`):
  - **13/10, depois do estudo e de duas críticas (o revisor e o Look):**
    - a morte do tanque deixa o **avental** em volta do casco (terra queimada e combustível, brasa nos primeiros ~30 s, areia espirrada e torrões);
    - as explosões sem carcaça em cima deixam a **cratera** (bacia de ~2,6 m com sombra dentro, halo escuro, borda quebrada, raios e torrões).
  - **Como é feito:** os atlas assados por `Game/Tools/bake_craters.py` (6 variantes de cada), carimbados por um compute no caderno.
  - **13/10, o Felipe: "deixe as 3, cada uma pra uma coisa":**
    - a A (0,75×) é o tiro de tanque: 20% dos tiros passam raspando e cavam 6–12 m além do alvo (`impact_crater` e `near_miss` no visual do Units.json), e todo tiro no chão (Ctrl) cava;
    - a B (1×) são as explosões do prédio que cai;
    - a C (1,3×) é o tanque explodindo (o avental C) e, quando existir, o míssil pesado.
    - A cena `vila` mostra as três lado a lado antes da luta.
  - **A altura:** vai numa folha R16F própria, com a sombra do sol dentro da bacia.
  - **Os destroços:** 5–8 peças por morte (`game.ps1 debris`).
  - **A cor final:** é da MF do Look, embutida no material do chão do World_TestSalt2 desde 13/10 (o WorldForge monta com o CODE do `look_mf.py`).
    - O jogo liga as instâncias dinâmicas do Landscape, inclusive na malha Nanite: sem elas, os parâmetros não chegavam.
    - O jogo manda os cadernos de detalhe e do meio (`GroundTrackMid`, pra o queimado ficar visível na câmera do RTS) e a brasa a 3000 nits.
    - O decal de piso sai sozinho quando o chão lê o caderno do meio.
  - **Na névoa:** a morte que acontece fora da vista guarda o avental e os destroços, que aparecem quando a área é vista, junto da carcaça.
- ~~**Árvore transparente perto do cursor**~~: **saiu em 13/10** (o Felipe nunca aprovou pro jogo; vai ter tanque escondido nas árvores, `docs/IDEIAS_JOGABILIDADE.md` ideia 6). A árvore nunca entrega unidade escondida.
- [~] **Silhueta da unidade** (jogo, 13/10, sem a foto aprovada; **só as suas e as dos aliados**: o Felipe, 13/10 à noite, "nenhum tanque inimigo pode ser exibido atrás das coisas", senão a mecânica de se esconder atrás da árvore vai pro lixo): atrás de prédio, casa ou palmeira, a unidade aparece na cor do time, meio transparente e com a borda mais forte, como no SC2 (custom depth com o dono no stencil + o pós-processo `M_UnitSilhouette`; `-RTSNoSilhouette` desliga). Dentro da fumaça ainda não: a fumaça não grava profundidade.
- [~] **Contato com o chão** (jogo, 12/10: mancha macia de cantos redondos sob cada tanque, `M_UnitContact`; ainda fraca): sombra de contato e uma mancha escura macia sob cada tanque. A 140 px, a fresta escura embaixo do casco é o que diz "pesado".
- [~] **A sala só com IA não começava** (14/10, o Felipe sozinho com 3 IAs: "Waiting for the other players"): o começo exigia ao menos um PC de fora. Corrigido no `RTSNetSession` e compilado (03:14), **fora do pacote que ele mandou pro pai** (o zip das 03:1x). Entra no próximo pacote, junto com o Prewarm da 1ª explosão (Fluxo) e as pastas que faltaram no cozimento (a poeira, o material da pedra da cratera), depois do roteiro do `docs/MULTIPLAYER.md`.
- [x] **Travamento da placa no meio da partida** (14/10, sessão do jogo; caiu 2× no jogo do Felipe com DXGI_ERROR_DEVICE_HUNG no `HeightCS`): o relevo da memória do chão. Quando a esteira ou a marca nova ficava toda fora do caderno, na diagonal (briga longe da câmera), o retângulo saía com largura e altura negativas, a "área" dava positiva e o compute ia com bilhões de grupos. Agora largura e altura são conferidas (`RTSGroundMemory::DirtyRect`, `RTSGroundRelief::Compute/StampMarks`), e os grafos têm nome (`RTSGroundRelief`, `RTSGroundMarks`) nas migalhas de um travamento.
- [~] **Tremor de câmera** (jogo, 13/10, sem teste do Felipe): só nas mortes vistas até 70 m do foco, caindo com o quadrado da distância; ruído de ~25 Hz apagando em ~0,35 s, 18 cm no tanque e 35 cm no prédio (2–4 px na câmera do jogo, então o clique quase não sente).
  - **14/10, o "mergulho" (o Felipe: "toda vez que explode um tanque minha câmera fica assim por 1 segundo"):** era o tremor. A câmera é a raiz do ator, e o deslocamento "relativo" dela era a posição no mundo; em toda morte vista, a vista ia pro centro do mapa, no chão e de lado, enquanto durava o tremor. Agora o tremor entra no olho antes de posicionar a câmera. Prova na cena `mergulho` (Scenarios.json), antes e depois no mesmo instante: `captures/game/mergulho_antes_depois.jpg`. O controle loga "RTS vista fora da câmera" se a vista sair da câmera de novo.
- [~] **Estados de dano** (jogo, 13/10, sem a foto aprovada; **14/10: fora dos veículos**. O Felipe viu a camuflagem pintada por cima e disse "um negócio porco"; volta quando for bem feito, e o prédio segue com as marcas): as marcas no material (o M_Structure) crescem a partir dos 70% de vida, como o modelo danificado do ZH: placas de tinta queimada e lascada, e a fuligem abaixo de 50%. A carcaça ganhou a receita do Look medida na street_battle: a tinta queimada, a fuligem embaixo e no anel da torre, a ferrugem de calor nas quinas, a cinza no teto, e a brasa no buraco da torre apagando em 30 s. A luz do fogo saiu do teto do casco (o "ponto de lâmpada"). As fumaças finas e escuras vêm do FXLab. **13/10 de madrugada:** nada disso aparecia no jogo. O ruído das manchas era a foto da praia, que no verde linear só vai de 0,22 a 0,25 e nunca passava dos limiares; o asfalto do miolo virava a média na distância da câmera.
  - Agora o ruído é procedural, no shader (`FRTSNoise` no `import_provisional.py`).
  - A carcaça é mosqueada, em rampas largas: a 1ª versão em recortes virou camuflagem urbana. Preview fora da placa na mediana 0,33 do alvo.
  - O dano vem em lascas de ~0,35 m: ~5% aos 70% da vida, ~25% perto da morte.
  - Cena de teste: `game.ps1 view -Scenario carcaca`.
- [~] **Fábrica viva no desenho** (10/10, provisório: ventiladores girando, luzes que acendem e apagam com a energia, a obra subindo com andaime e solda; falta portão, ponte rolante e braço ligados à produção): um ator do prédio anima portão, ponte rolante, braço e ventiladores e acende as luzes, ligado aos eventos de produção; mais a entrada do prédio no `import_units.py` e o link de Buildings no `game.ps1`.
- [ ] **Paleta das cores dos jogadores** (8, como no ZH): testada no deserto e no zoom mais afastado; as mesmas no tanque, na fábrica, no minimapa e no HUD; cor única por vaga na sala (dois USA sairiam azuis).
- [ ] **Lado legível sem o anel no inimigo:** no USA contra USA sobra só a cor em detalhes pequenos. Testar no zoom máximo (81,6 m); se não der, o inimigo ganha uma marca mínima na cor.
- [ ] **Sujeira acumulada:** o tanque que andou e lutou fica empoeirado (CoH3).
- [~] **A maquete do Look vira o jogo** (ideia do Felipe, 11/10, "surreal e fenomenal"; "não só o tanque: a casa, a rua, o look atmosférico, tudo"; ainda pensando, provável centro da v2, porque os modelos finais à mão podem não chegar nela). Do lado do jogo:
  - **Tanque:** escala de 7,9 m, pivô no chão, torre separada (gira e recua), cor do jogador (hoje os dois lados saem iguais), carcaça queimada.
  - **Casas e rua no mapa jogável:** a casa vira obstáculo na grade da simulação (o caminho desvia, não se constrói em cima). A grade tem que sair igual em todos os PCs (o hash da grade da LAN).
  - **Look (luz, céu, névoa, pós):** a passada da névoa de guerra, a foto do minimapa e a leitura das unidades na câmera de RTS continuam valendo.
  - **Custo:** medir em tempo real na 1660 com 50 tanques (o da maquete é render de cinema, com aquecimento).
  - Combinar com a sessão do Look: ela faz o visual, e o jogo leva pro mapa jogável.
  - **Feito em 11/10 (sem a aprovação do Felipe):**
    - **O tanque do Look é o USA_MBT do jogo** (`game.ps1 looktank`). Usa os GLBs dela em `captures/look/tank_glb`, o material provisório dos prédios e o slot Team com a cor do jogador; a torre gira em x 0,10 m. Teste base com 0 falhas e o custo igual (23,0 ms).
    - **Ainda fraco:** lê quase branco e a cor do time sai pequena. Espera o M_UnitDust e a receita do plugin RTSLook.
    - **O plugin RTSLook:** o desenho dela está revisado (`docs/GRAFICOS.md` §8). A cor do time vai no CPD 0..3 e o queimado no CPD 6.
    - **Casas e rua:** o formato do WorldForge (JSON com a pegada na grade de 4 m) foi combinado.
    - **A vila subiu na frente** (decisão do Felipe em 11/10: "o mapa do Look foi sensacional, é eles mesmo que eu quero"). Peças: o kit civil aprovado (1 andar 8×8, 2 andares 8×8, varanda 8×12, comprida 16×8, muro com portão), a rua e as miudezas do Look.
      - O WorldForge põe a vila no World_TestSalt2 e exporta o JSON, sem tapar as 4 largadas (~88 m de raio), os 6 depósitos e o caminho entre as bases.
      - O Look faz as malhas e o material.
      - O jogo monta a simulação e o desenho a partir do JSON.
- [ ] **Perspectiva aérea na câmera do jogo:** o terço de cima da tela clareia e esquenta com a distância, como no alvo; igual em todos os presets e sem tapar unidade (com o WorldForge).
- [ ] **Céu na câmera deitada** (Alt+meio): pôr do sol com nuvens como no alvo v3; nuvem nunca entre a câmera e as unidades.
- [~] **Telas de resultado e 3-2-1 pra N jogadores:** anéis e barras do mundo somem quando o painel abre (hoje uma barra atravessa o "VITÓRIA"), letra nítida.
  - 16/10 (sessão do jogo): as barras de vida e os anéis do mundo não são mais desenhados quando o painel do resultado abre (`RTSHUD.cpp`).
    - No HUD novo, a lista dos jogadores do espectador caía em cima do cartão do comandante: foi pro alto, embaixo do dinheiro, e sai quando o painel do resultado abre.
    - Visto na cena nova `resultado` (1 tanque contra 1, a derrota em segundos): `captures/game/resultado3/05_cena_018s.png` (o espectador) e `06_cena_022s.png` (o DEFEAT limpo).
    - Faltam o 3-2-1 e a letra.
- [ ] **Menu e opções:** botões Multiplayer e Opções; preset, resolução, limite de quadros, volumes e teclas; preset **Auto** que mede a nossa cena (o detector da Unreal joga a 1660 no máximo, e a meta de 60 qps depende dele).
  - [x] 14/10 (sessão do jogo): a linha **QUALITY** (Low/Medium/High/Ultra) no Esc e no menu principal, além do F5–F8; fica guardada no `GameUserSettings.ini` e a 1ª vez num PC abre na Medium (a que rodou no notebook de 2014 na v1). Falta o resto da tela de opções e o Auto.
- [ ] **Áudio do lado do código:** ouvinte no ponto de mira a ~12 m, limite de cópias por som, mixagem que abaixa o resto, batalha escondida muda.
  - 16/10 (sessão do jogo): conferido, o projeto não tem nenhum som ainda (nenhum .wav nem SoundWave em Game, FXLab ou BlastLab). Os sons precisam vir de fora, e baixar pede o OK do Felipe. Pergunta pra ele: de onde (biblioteca livre, comprada ou gerada).
- [ ] **Legibilidade medida como critério:** quanto fumaça, explosão e névoa cobrem de uma unidade; o tanque contra cada chão (ΔE ≥ 20 ou contraste 3:1). A salina clara apaga a USA cinza-clara.
- [~] **HUD final** (o foco do jogo desde 09/10; o plano, com as fontes, está em `docs/HUD.md`: a barra do ZH modernizada, em Slate). Já feito: o texto nítido e a saída dos anéis do chão.
  - **13/10, o HUD novo do HUDLab no jogo** (pra o Felipe testar jogando): o plugin `RTSHud` da arte em `Game/Plugins`, ligado por `RTSHUDNew.cpp`.
    - Liga com `-NewHud`; na partida, **F4 troca** entre o novo e o de hoje.
    - O minimapa continua o do jogo (orientação, névoa, clique), desenhado no buraco da peça nova.
    - Os botões de sempre seguem pelos mesmos ids. Os grupos, a fila, Stop, Attack Move e as teclas das casas (QWERTYU / ZXCVBNM; o R posicionando prédio segue girando) funcionam; poderes e patente ainda não existem na partida.
  - painel da fábrica (fila, botão do tanque), sala de espera e tela de conexão;
  - cursor próprio e fonte final (a licença vai no `CREDITOS.md`);
  - tela de carregar com arte.
  - A arte de referência vem da sessão de arte.
- [~] **Vento único** (jogo, 13/10): `/Game/Materials/MPC_Wind`, vetor `Wind` = (x, y, m/s, rajada), com x, y a direção pra onde sopra; o jogo preenche no começo da partida pelo Maps.json (`wind_yaw`, `wind_speed`). A memória do chão já usa. Falta cada sessão ler: a fumaça (explosões), a areia (Look), as palmeiras (WorldForge) e as bandeiras. O contrato já foi mandado pra elas.
- [ ] **Efeitos sem colisão com o clique:** destroço, cartão de fumaça, carcaça e cratera não podem roubar o clique de mover nem dar tranco na câmera.
- [ ] **Antisserrilhado com efeito translúcido e linha fina:** cartões de fumaça borram quando a câmera anda, e traçante e anel cintilam. Ajustar o TSR.
- [ ] **Justiça entre presets:** o que esconde unidade (fumaça, poeira) tem de ser igual em todos os presets, senão quem joga no baixo vê mais.
- [~] **A vila do WorldForge no jogo** (12/10): o jogo lê o village.json (cópia pelo game.ps1 worlds, hash conferido com a etiqueta do mapa). As peças que bloqueiam fecham as células da pegada na grade (242 peças, 279 células). O jogo desenha cada peça com o kit civil do Look (/Game/Village, game.ps1 village; caixa provisória do tamanho certo quando faltar a malha). Teste "vila_rua": a coluna atravessa a vila pela rua desviando das casas. Falta: conferir o lado dos portões e a folga das picapes na beira da rua (com o WorldForge), as casas tomáveis e destruíveis (ZH) e o pacote novo da família.
- [~] **Desfile** (11/10, estudo de SC2, AoE4, BAR, SupCom, 0 A.D. e OpenRA, pelo código):
  - **Feito:** a área de reunião da fábrica. Os tanques param em fileiras escalonadas de 4 viradas pra longe da fábrica, com um tremido fixo de ±0,3 m e ±4°. O grupo chega virado pra direção da marcha (simulação versão 7, teste "ponto de encontro").
  - **Falta:**
    - as vagas do grupo sem cruzamento (húngaro até ~24, por N e nunca por tempo; acima disso, troca dos pares que se cruzam);
    - o mais forte na frente quando houver mais tipos;
    - [x] **a formação segurando F** (simulação versão 8, teste "formação com F"): o grupo vai no passo do mais lento, mantendo a forma, como o Felipe decidiu em 11/10 ("segurando uma tecla, como no C&C; sem segurar, cada um na sua velocidade"). Grupo espalhado demais é apertado até um bloco. Falta testar no jogo;
    - [x] **as torres em leque** (simulação versão 8): as da ponta olham até 25° pra fora e as do meio pra frente; em fila indiana, alternam esquerda e direita. Vale andando e parado, e na área de reunião também (testes "ponto de encontro" e "formação com F"). Falta ver no jogo;
    - [~] **a linha com os dois botões + a prévia** (v4, 23/10; substitui o arraste com direção de 12/10, em que o esquerdo arrastado com tropa selecionada virava a linha quando o Felipe queria a caixa). Esquerdo + direito juntos, arrastando, em qualquer ordem: a linha vai do ponto apertado até o cursor, com a fileira da frente em cima dela. Da esquerda pra direita, a frente fica de costas pra câmera; ao contrário, vira pra ela (a regra do Total War). Enquanto puxa, cada unidade aparece como um anel no chão com um risquinho pra frente, no lugar exato onde vai parar: é a mesma conta do comando (`World::PreviewLineSlots`, teste "prévia da linha" na simulação). Soltar os dois manda. O Esc, ou soltar um e segurar o outro, desiste. O esquerdo arrastado sozinho é sempre a caixa, e o direito sozinho rola. Funciona com o F e com o ataque andando (os anéis ficam vermelhos). No teste da partida, os passos são "linha_previa", "linha_ordem", "linha_chegada", "linha_esc", "linha_solta_um", "acorde_parado", "direito_rola" e "caixa_com_tropa". Falta o teste dele, clicando. Limites conhecidos:
      - [~] (v4, 23/10 à noite, simulação 20, sem o teste do Felipe) **a linha espalhada:** com menos unidades do que a linha comporta, elas ficam numa fileira só, de ponta a ponta da linha puxada (o BAR e o Total War); com mais, as fileiras cheias vão de ponta a ponta, nunca abaixo do passo mínimo (2,34 raios), e a de trás fica no meio. Antes juntavam no meio no passo mínimo (3 tanques numa linha de 60 m ocupavam 17 m). A prévia segue sendo a vaga da ordem, bit a bit (simtest "linha espalhada"; a cena `v4_linha`). (Revisão da noite) Cabem floor(L / passo mínimo) + 1 por fileira: nenhuma vaga passa da ponta da linha puxada (6 tanques em 40 m: 5 de ponta a ponta e 1 atrás; antes a fileira de 6 passava 1 m de cada ponta), e a de trás só desencontra até a ponta;
      - a linha de lado em relação à marcha: os tanques entram no mesmo corredor.
  - A fila da IA pela praia (t7) é a mesma coisa, no caminho.

### Desempenho e estabilidade
- [ ] **Aquecer todos os efeitos antes da partida:** tiro, impacto, carcaça, poeira, marcas no chão e fábrica. No lockstep, a travada de um PC segura todos. Cache de PSO no jogo empacotado.
- [ ] **Diretor de orçamento pra todos os efeitos:** poeira do andar e fumaça de dano somem primeiro; tiro, impacto e morte ficam sempre (as prioridades do ZH).
- [ ] **M0, a batalha de referência:**
  - 50 tanques e 10 mortes juntas, medido por preset;
  - medir ms e memória de vídeo: mapa ~2,2 GB, mais explosões, cartões, fábrica e névoa, contra 6 GB (a placa caiu a 4,6 GB em 07/10);
  - meta: 60 qps no preset que a placa escolhe;
  - o plano põe no BlastLab (`docs/JOGO.md` Parte 4); falta o OK do Felipe pra pedir.
- [ ] **Medir MegaLights e Lumen Lite na 1660** antes de fechar os 60 qps: podem cortar os ~7 ms de luz e sombra das explosões.
- [ ] **Medir o mapa por preset no jogo.** O "ultra" do jogo é o cine (16× os raios do Lumen): rever.
- [ ] **Água igual à revisada:** no jogo, os reflexos do Lumen estão desligados até o alta. Fazer o A/B.
- [ ] **Congelar a versão dos assets.** Os links físicos quebram calados quando outra sessão regrava um arquivo, e no multiplayer os dois PCs precisam do mesmo conteúdo.
- [x] **Usar a fila da placa** (`C:\Users\luisf\.gpu_queue\gpu_queue.ps1`, que o `docs/GPU.md` prefere) no `game.ps1`: o teste e o jogo do Felipe seguram a placa até fechar; sem a fila, cai na trava (commit `e927a02`).
- [ ] Cada peça aprovada que chegar entra com folha lado a lado contra o alvo, **em vídeo**.

---

## 2. Arte e FXLab (sessão "C&C Zero Hour conceitual modernizado")

- [ ] **War Factory USA em 3D** (não existe; é o maior gargalo). O Felipe decidiu modelar **à mão**, ele mesmo no Blender (08/10; a modelagem por script da arte foi rejeitada: "massa de modelar"). Antes, ele aprende numa casa civil.
  - [~] ficha de produção feita (`docs/PRODUCAO_3D.md`, 08/10) + modelo-base do Blender (`art/tools/make_template.py` → `captures/art3d/RTS_Base.blend`), cobrindo: escala pelo tanque de 7,9 m (largura do prédio e do vão, 5–6 tanques é o limite), planta encaixada na grade de 4 m (vira a área ocupada na simulação, que nenhum documento define), pivô no chão, altura que não tape unidade na câmera de 37,5°, vista de cima primeiro, orçamento de triângulos e textura;
  - a folha tem a 3/4 de trás; faltam a elevação de trás e o lado oposto;
  - peças separadas: os **2 ventiladores grandes do teto** (o maior elemento visto de cima), ponte rolante amarela, braço robótico, bandeira e domo;
  - **portão:** a arte aprovada é um galpão aberto, sem portão. A regra do ZH (porta abre, tanque sai) pede portão novo (enrolável?) ou outro sinal (luz, a ponte baixando o tanque). Isso muda arte aprovada: decisão do Felipe;
  - luzes de serviço, postes e balizas acesas (sem neon), interior do galpão iluminado;
  - estados: inteiro, danificado e escombro;
  - máscara da cor do jogador: a regra já manda lona no teto (`CONCEITO_CONSTRUCOES.md`), e a vista de cima da folha já tem painéis azuis. Medir de novo na vista de cima antes de inventar peça;
  - os objetos do lote: postes, piso com faixas;
  - a versão destruída (alvo 4.5), se a vitória for pela fábrica.
- [ ] **Tanque USA final** (o de hoje não foi aprovado pelo Felipe; o nível "leve" de ~49 mil triângulos foi escolha da arte, decisão dele):
  - fiel à ficha: hoje a frente da lagarta e a roda motriz saem em blocos lisos, e o azul fica apagado na câmera alta;
  - máscara da cor: a lona do cesto é a maior área vista de cima; passar no teste de visão de longe;
  - texturas de dano e queimado, a carcaça e as antenas.
- [ ] **Se for USA contra GLA:** tanque GLA e Oficina GLA em 3D (as fichas já estão aprovadas).
- [ ] **Fatia vertical do deserto** (proposta da arte, aceita pelo Felipe em 12/10). Entra **quando as frentes atuais das outras sessões terminarem**, sem atropelar ninguém.
  - **O que é:** um canto do mapa do deserto, na câmera do jogo, **lado a lado com `docs/concept/scenes/desert_golden_hour/best/town_battle_top_gemini.webp`**. É a prova de que o jogo chega nos game shots.
  - **Os modelos:** provisórios **caprichados e fiéis às fichas aprovadas** (War Factory, tanque, Dozer e Chinook da USA, mais o tanque e a picape do GLA). Quina dura, a proporção e a silhueta da ficha, material com desgaste, o pó subindo de baixo pra cima e a cor do time. Os finais do Felipe substituem depois, sem mudar o lugar.
    - **Dozer (13/10, sessão Modelos 3D):** refeito por script pela arte aprovada (`Game/Tools/model_usa_dozer.py`, medido contra o tanque), aprovado pelo Felipe e no jogo (`game.ps1 dozer`, `Units.json`, sirene amarela que gira e acende só na obra). Mapa dos provisórios: `docs/MODELOS_PROVISORIOS.md`.
    - **Por que provisório serve:** na câmera de 61 m o tanque tem ~120 px. No teste do Dozer, o rebite virou granulado e o que leu foi a silhueta, a proporção, o material e a luz (`captures/concept/43_usa_base/dozer_game_scale_test.jpg`).
  - **Quem faz cada peça:**
    - os provisórios: quem já faz (o jogo, com `Game/Tools/blender_provisional.py`; o look, com o tanque);
    - a conferência: a arte, com a folha "provisório × ficha" na câmera do jogo;
    - o chão: o WorldForge (alvo `close_ground_light_gemini.webp`);
    - a luz: o look (a receita das sete);
    - uma explosão: as sessões de explosão.
  - **A prova:** a folha lado a lado e um vídeo. O Felipe julga.
- [~] **Fogo da carcaça do FXLab** ("o melhor até agora", salvo; **não aprovado como final**, Felipe 09/10), levar pro jogo:
  - no deserto com a luz do jogo ficou igual ao aprovado (14:39 de 08/10; o Felipe: "fez um ótimo trabalho com o fogo"); parados: o vídeo no deserto e a medida com 1+1 e 3+3;
  - ciclo de vida: pico, cauda e apagar (hoje solta sem fim);
  - emissor em Niagara (hoje é C++ com malha procedural), textura comprimida (BC6H/BC7) e várias carcaças ao mesmo tempo;
  - caminhos: o cozimento e o preset moram em `captures/` e no FXLab, fora do git e do pacote (e `captures/` é candidata a apagar pra liberar disco: perder é perder o aprovado); o sol é achado pela tag "GameSun", que o mapa do jogo não tem; o material 6-way vira etapa do `build_content.py`;
  - duração (a proposta era 2 min) e o "espalhar pelo capim" que o Felipe perguntou;
  - ela leva quando o Felipe mandar.
- [ ] **Clarão de boca do canhão** (dono: sessões de explosão; o arsenal foi tirado da arte): chama curta, luz no casco e no chão, fumaça saindo do cano e poeira levantada no chão à frente (o ZH empilha 5–6 camadas).
- [ ] **Faíscas quando o tiro acerta o tanque** (alvo 4.2; dono: sessões de explosão): leque de riscos quentes, clarão no metal, chapinhas e uma baforada cinza. **É o efeito que mais aparece no jogo**, porque o tiro quase sempre acerta.
- [ ] **Tiro na areia** (alvo 4.1): **pausado pelo Felipe** na arte (~14:56 de 08/10); fica com as sessões de explosão e não se retoma no FXLab sem ele pedir.
- [~] **Poeira do andar** atrás das lagartas: cor pelo tipo de chão, baixa, some em 3–6 s e é a primeira a sair no preset baixo.
  - **No jogo desde 14/10, de madrugada, sem o teste do Felipe:** o plugin RTSTreadDust da arte (a v12 do FXLab), em `Game/Plugins/RTSTreadDust`, com o passo a passo no `INTEGRACAO.md` dele.
    - O jogo manda a cada quadro os veículos de lagarta perto da câmera: a posição desenhada, a velocidade com sinal, o giro, a areia pelo `surface.png` do WorldForge (para no asfalto) e a névoa.
    - O preset vai pelo F5–F8; `-RTSNoDust` desliga.
    - O custo: na `chao` (12 tanques, alta), a mediana subiu de ~25 pra 31 ms.
  - 13/10, a arte: feita no FXLab (navegador e Unreal, mesma lógica) contra as cenas aprovadas; o estudo e o como estão em `docs/FXLAB.md`;
  - pro jogo: o ator `AFXTreadDust` (um só pra todas as unidades, `SetSources()` por quadro);
  - 13/10 à noite, a arte: a v12 (o caminho escolhido, rastro longo) empacotada no plugin `FXLab/Plugins/RTSTreadDust` (`ARTSTreadDust`, presets 0–3), com o passo a passo pro jogo em `FXLab/Plugins/RTSTreadDust/INTEGRACAO.md`; falta a sessão do jogo ligar (fontes de `SurfaceAt`, névoa, preset);
  - 14/10, o Felipe reprovou a v12 no jogo ("exagerada, jogo de 2012, entra dentro dos tanques"; quer a cor exata do chão e o teste posição por posição);
  - 16/10 madrugada, a arte: o redesenho medido contra o game shot r1 na mesma pose, imagem sobre imagem (`docs/POEIRA_DAS_LAGARTAS.md`, "16/10"):
    - a v20b: a cor do chão capturada, a máscara de unidade (bit 128), menor, pálida e macia;
    - o miolo bate com o alvo (0,90 da areia);
    - as folhas BC7: 150 → 19 MB;
    - a sombra do cenário.
    Pronta pra recopiar no jogo (pedido à sessão do jogo). O comprimento (curto do alvo × longo de 13/10) vai em A/B pra ele;
    - 16/10 ~06h, a sessão do jogo: copiada pro `Game/Plugins/RTSTreadDust` com a vez da compilação (Source com o Build.cs novo, `Resources/tread.json`, os materiais, as folhas BC7 `tread_kick_v2c_norm8`). A folha HDR e o `tread_v12.json` velho ficaram como estavam; nada foi apagado. A arte prova posição por posição com os `poeira_*`;
    - 16/10 06:49, a v21 (sem a névoa que lia como discos pálidos; a sombra do cenário calibrada pelo espalhamento do chão Nanite) e o `tread_longo.json` (o rastro longo de 13/10, pro A/B dele: `-TreadDustProfile=<arquivo>`). Na `chao`: 32,7 ms de mediana (a sombra do cenário "encaixava" 80 de 81 pontos, mas com 314 m de espalhamento: a orientação errada, achado do jogo e confirmado pela arte; corrigido às 07:04; às 07:14 a calibração nova da arte acha a orientação certa (+1, −1: mediana 32 cm, espalhamento 181 cm, contra metros nas outras) e liga a sombra com 50 de 81 pontos, mas a folga bate no teto de 400 cm; com a arte) (a v12 dava 33,2–33,4). O custo medido pela arte no FXLab, 40 tanques: alta +4,4 ms na v12 → +1,1 ms na nova;
  - 13/10, o Felipe: a poeira tem de ser como a das gameplays em vídeo, gravada no EmberGen; a feita à mão fica de plano B. A arte simulou no EmberGen (`docs/FXLAB.md`): a versão EmberGen é a ativa (`FXLab/tread.json`); falta o Felipe ver o vídeo e a ponte do jogo.
- [ ] **Chaminés da fábrica:** vapor branco na USA (fumaça preta é a da Oficina GLA), acima do nível das unidades, lendo o vento único.
- [ ] **Cratera e queimado** com textura de verdade (escaneada, não ruído); destroços que ficam.
- [ ] **Fumaça de dano** pro tanque ferido, a mesma da carcaça, menor.
- [ ] **Calor** (distorção sobre fogo e tiro) e **brasas** subindo da carcaça e da cratera.
- [ ] **Respingo** de tiro na água.
- [~] **Kit civil do deserto** pros mapas (pedido do Felipe pra 1ª gameplay jogável): casas de vila modulares, veículos civis abandonados, postes, cercas e sacos de areia.
  - fichas aprovadas (08/10): casa de 1 andar, 2 andares, varanda, comprida e muro com portão, em `docs/concept/buildings/civil/` (tiradas da cena do jogo); referência de todas as casas das cenas em `docs/concept/towns/`;
  - kit pronto: os pagos da Fab foram descartados (custo) e os grátis do Sketchfab achados ruins (09/10, `captures/art3d/kits/`); **o Felipe modela à mão**, começando pela casa de 1 andar (`captures/art3d/civ_house_1story/`, com as 5 vistas);
  - 6 variações a mais (quartinho no teto, ruína, pátio, fortificada, mesquita, loja): **adiadas** por ele.
- [ ] **Ícone do tanque** pro botão do painel da fábrica e o **retrato da unidade** (unit portrait) do tanque, o que aparece no painel quando ele está selecionado.
- [ ] **Se for USA contra GLA, além dos modelos:** as bandeirinhas verdes (cor do jogador do GLA) balançando no vento; a morte do GLA com outra assinatura (sucata, rodas, fumaça preta oleosa); a Oficina solta o tanque por rampa subterrânea (outra saída); o amarelo do GLA some no deserto (fica pra interface); a máscara da Oficina marcada à mão.
- [~] **Alinhar a sessão de arte com esta lista** (09/10): a arte leu a lista; o 1º teste é uma War Factory por lado, sem Centro de Comando, Dozer nem suprimento (decisão do Felipe, 08/10).
- [~] **Arte de referência do HUD, do minimapa e da névoa** (explorado contra nunca visto). Pedida em 09/10 com as medidas do ZH (`captures/concept/hud/PEDIDO.md`). Os prompts dos 3 estados já estão com o Felipe; a aprovada vai pra `docs/concept/hud/`.
- [ ] **Som**, a especificação, que está adiada:
  - tiro, impacto, morte, motor e esteira;
  - fábrica produzindo e "unidade pronta";
  - interface, música, ambiente (vento do deserto), voz das unidades e locutor;
  - o pacote da Sonniss não tem música nem voz.
- [ ] **Créditos e licenças:**
  - o site do Hunyuan usado;
  - declarar IA na Steam (inclui a pintura do Gemini);
  - o que sai do EmberGen em teste.
- [ ] **Documentos:**
  - o FXLab entra no `CLAUDE.md` e no `README.md` (sessão nova não sabe que existe);
  - o §10 do `CONCEITO_UNIDADES.md` está desatualizado.

## 3. BlastLab (sessão "Fluxo de explosão realista AAA")

- [ ] **Levar pro jogo a bola de fogo aprovada em 08/10** (o raymarch, commit `317aca6`):
  - como peça comum aos projetos, um plugin, e não mais uma cópia;
  - os dados (3 × 213 MB) num lugar que vai no pacote.
  - Hoje o jogo desenha com o volume antigo rejeitado.
- [ ] **Morte de tanque completa:** clarão, anel de poeira, bola laranja, destroços e faíscas-cometa, torre voando com rastro e coluna preta, numa família só de fumaça.
- [ ] **Refazer o clarão branco** sem lavar a tela nem tapar unidades: bola HDR, bloom e luz curta. Já está na fila dela.
- [ ] **A fumaça não cobre as unidades:** sobe e sai do nível delas em segundos (defeito 7).
- [ ] **Batalha grande:** mais de 16 explosões juntas, e o tranco com 12.
- [ ] Atualizar a memória `project-raymarch-portado`, que ainda diz "não aprovado".
- [~] **A fila real dela:** 1º o detalhe fino da fumaça de perto (mexe no `BlastRaymarch.usf` que o jogo vai levar, então portar antes de fechar gera cópia velha); 2º o clarão.
  - 09/10: as bolhas de perto entraram (commit `bedd4e7`); 128³, sombra fina e borda afiada não mudaram nada visível. A maciez vem da simulação gravada, então agora é **"mudar quem grava"**: a mesma explosão do WebGL, EmberGen, Blender (e depois Unreal e Houdini) tocada no mesmo raymarch, lado a lado (`-RmCompare=`, `Tools/build_raymarch_vdb.py`). Em curso.
- [ ] **Anel de poeira da morte como peça própria:** o raymarch não tem a saia de poeira; levar só a bola perde o anel do alvo (claro, rente ao chão, some em ~1,5 s). Candidato: o cartão deitado `shockwave_top` do FXLab. Hoje o volume antigo deixa uma rosca escura no nível das unidades por dezenas de segundos.
- [ ] **Morte de tanque:** custa 32/41 ms na câmera de RTS (precisa caber nos 60 qps com várias juntas) e ainda está no volume antigo no próprio BlastLab (duas fumaças na mesma batalha).
- [ ] **Variação da morte por sorteio:** explode já ou queima 2 s antes, sub-explosões, 4 sementes; senão 10 mortes iguais se repetem.
- [ ] **A coluna da JangaFX** (aprovada, "tá boa") está linkada no jogo e nada usa: entra na família única ou sai.
- [ ] **Sombra da fumaça no chão** (light function no sol, 0,06 ms) e a **deformação da fumaça** pelo que passa (aprovada) ainda não existem no jogo.
- [ ] **Onda de reação em d/343 s** (fase 3b): bandeira chicoteia, poeira cai do telhado da fábrica, tanque vizinho balança, palmeira curva.
- [ ] **A ferramenta de A/B oficial** (Seq_Explosion pelo Movie Render Queue) sai lavada; hoje a comparação é ao vivo.

## 4. WorldForge (sessão "Gerador de mundo RTS", parada desde 07/10 esperando o Felipe)

- [ ] **Mapa oficial congelado,** com nome e versão: o jogo copia em vez de linkar.
- [ ] **Grade de passagem canônica:** `pass.png`, obstáculos, versão e hash, tirada da altura final.
- [~] **O céu e as nuvens da câmera deitada** (v3, Look, noite de 16/10; relatório com as fotos do jogo: https://claude.ai/artifact/UfuypytFapk71Tzzhngk9Y; estudo `docs/CEU_E_NUVENS.md` §11):
  - o céu preto ao deitar era a névoa de guerra (corrigido pela sessão do jogo);
  - o "céu destravado" (só com o topo da tela perto do horizonte): a névoa 2,5× mais rasa e a perspectiva aérea ×1,5; a câmera normal idêntica, custo 0 (receita `sky_b1`);
  - as nuvens do material da Epic, ligadas só ao deitar: a recomendada `sky_e4_douradas` (+3,8 ms deitada, 0 na normal); o nosso material próprio falhou e ficou de lado;
  - o mapa de sombra das nuvens só quando usado (ganho de 2,9 ms);
  - **falta:** a escolha do Felipe (estilo das nuvens), o lago branco com a câmera deitada, a receita aprovada ganhar os blocos `sky`/`clouds`.
- [~] **O chão v3: areia, trilha, garra** (Look + jogo + WorldForge, noite de 16/10; `docs/CHAO_V3_ESTUDO.md` §10; o mesmo relatório):
  - a areia aprovada nunca esteve no mapa (era a classic); a v2 pura sai lavada; a **v2 com a cor de duna** tem a cor de hoje com a textura nova (`World_TestSandV2Dune`);
  - a trilha com a luz assada (o sol parado vira textura, a normal de iluminação na MF `code_v3()`), o perfil v3, a curva, o halo e a **garra de 2 cm**: vira sulco em vez de fio; `World_TestGroundV3Dune` com `-RTSGroundV3`; custo ~+0,6 ms com 6 tanques;
  - **falta:** a escolha do Felipe, a nitidez do sulco e a ondulação visível no zoom do jogo, a cratera com paralaxe, os 1,5 ms do mapa novo.
- [ ] **A estrada com volume** (v3; 14/10 o Felipe procurou na vila do jogo e não achou, ficou só na maquete): a laje de 12 cm com a borda quebrada, nos 3 tipos (intacta com areia na beira, gasta, destruída) escolhidos por trecho no WorldForge. Protótipo e imagem: `docs/RECEITAS_DO_LOOK.md` §6 e `docs/aprovados/look/2026-10-13_estrada_volume_3tipos.jpg`. Donos: Look (o kit: perfil, borda, material), WorldForge (o traçado e o leito rebaixado), jogo (a altura na simulação). A faixa pintada nova (§7, "em teste") também não foi pro jogo.
  - **14/10, madrugada (v3):** o WorldForge leva a malha da laje pro mapa com o kit do Look (`road_slab`, 13/10 15:23), o leito rebaixado e o material do asfalto v2 com a memória do chão. A prova vai num mapa de teste (A/B a 100 m e a 67 m na luz do jogo, contra a v2 e a folha dos 3 tipos, mais um vídeo pro tremor); o mapa do jogo só muda com o "aprovado" do Felipe.
  - **16/10, noite (v3, sem commit):** pronta no mapa de teste `World_TestV3`; a prova no jogo espera a compilação que troca o mapa pela cena.
    - **Feito:**
      - a laje nos 3 desgastes (a vila inteira, trechos gastos e destruídos);
      - a areia da beira que cobre a laje (a própria laje sobe e pinta a areia do chão, sem os dentes de 0,5 m da 1ª versão);
      - as rachaduras do asfalto v2 inteiras (o streaming pedia mip baixo);
      - a faixa da beira só onde há laje;
      - o chão debaixo do miolo levantado, pro tanque andar em cima sem mudar o jogo.
    - **O custo:** o passe do chão cai (6,86 → 5,56 ms numa vista da estrada; 4,44 → 3,13 ms na vila, junto com a pedra com desvio), porque o Landscape deixa de calcular a camada de asfalto. A laje tem 4 milhões de triângulos.
    - **No jogo:**
      - a memória do chão nas lajes (`PushToSlabs`, o jogo);
      - os cenários `laje_{gasta,destr,vila}_{100,67}_{v3,v2}`.
    - **A prova no jogo (16/10, 05:50):** a folha `docs/aprovados/mundos/2026-10-16_laje_ab_para_escolher.jpg`, v2 × v3 na mesma cena, a 100 e a 67 m, com a folha dos 3 tipos do Look embaixo.
      - Na 1ª folha a areia da beira cobria meia pista e a estrada perdia a leitura, ao contrário dos alvos 12 e 14. Ficou só na borda: o centro da faixa 1 m pra fora, sand_over 0,4.
    - **A faixa pintada v2 do Look na laje** (`World_TestV3Paint`):
      - o `PAINT_V2_CODE` com a posição na pista em 16 bits (`T_RoadFrame`, RGBA16F a 1 m);
      - a da beira contínua seguindo a curva, o eixo tracejado, gasta em trechos, creme;
      - sem os decalques;
      - a folha `docs/aprovados/mundos/2026-10-16_faixa_v2_na_laje_para_escolher.jpg`. A 67 m a diferença é sutil.
    - **O que ele escolhe:** v2 × v3 (com ou sem a faixa nova) e a quantidade de areia na beira.
    - **Atenção nas comparações:** as folhas de 16/10 (a laje, a faixa e as dunas) foram fotografadas no Alta com o viés da sombra antigo (−1,5), nos dois lados. Desde 16/10 de manhã o Alta usa −0,5 (escolha dele): A/B novo sempre com os dois lados fotografados de novo.
- [~] **A pedra do chão com desvio dinâmico** (16/10, `ROCK_BRANCH=1`):
  - a mesma conta da Unreal, só onde a pedra pode aparecer;
  - −1,0 ms no passe do chão na vista de cima;
  - a olho nu, igual à v2 lado a lado. Detalhes no `WORLDFORGE.md` §12.5.
  - No `World_TestV3`.
- [~] **A areia encostada nas paredes da vila** (16/10, `WALL_SAND`):
  - a da maquete (35 cm) sumiu na câmera do jogo; a forte (75 cm, rampa de ~1,8 m) também quase não aparece (folha em `docs/aprovados/mundos/2026-10-16_areia_paredes_ab_para_escolher.jpg`).
  - **A minha recomendação: não seguir nesse formato.** Os montes do r1_raid_slabs são grandes e com crista nítida, e o Landscape a 0,5 m por vértice não desenha essa crista. O caminho é um carimbo de malha (a duna de parede como peça, com a crista) ou o chão com tesselação; fica pra decisão dele.
- [x] **Os bolsões de dunas** (16/10, aprovadas: "ok ta aprovado"; o padrão do deserto, `DUNE_POCKETS=0` tira; antes `World_TestV3Dunes`; o estudo com fontes no `WORLDFORGE.md` §12.7):
  - a 1ª versão (barcanas desenhadas por fórmula) foi reprovada duas vezes: "muito padronizada e única" e "ainda tá muito padronizado, não tem nada natural aí";
  - **o passo atrás (16/10 à tarde): a areia simulada** (`DuneSim.h`, o modelo de lajes de Werner/Paris 2019; a bancada em `WorldForge/Tools/dunesim`). O vento leva a areia num ladrilho de 1 km e as dunas nascem sozinhas, cada uma diferente. As formas entram nos bolsões inteiras, cada uma uma vez só, com as vizinhas dela no mesmo arranjo;
  - a 1ª foto no jogo (a duna achatada a 0,45, face de ~17°) quase não mostrava a duna: agora a duna inteira em meia escala (largura e altura juntas), a face a 30°;
  - medido no mapa do jogo: 45 dunas de até 2,6 m de crista, ~7% do chão onde pode (o campo esparso real: 8–12%), os dois lados com a mesma área de duna (3.327 × 3.245 m²), máximo de 32,6°, nada acima de 34°;
  - cada forma entra no máximo duas vezes, a segunda espelhada e a 200 m ou mais da primeira;
  - o mapa de teste é o `World_TestDunesSim` (nome novo: o `World_TestV3Dunes` ficava preso pelos testes abertos), a base nova (areia 3 + chão v3) só com as dunas a mais; o A/B no jogo é `dunas_sim_67/100` × `dunas_base_67/100`.
- [~] **Os mapas de teste do WorldForge no jogo** (16/10, com o ok do jogo):
  - `World_TestV3`, `World_TestSandV2` (a areia v2 pro Look) e `World_TestGroundV3` (o chão v3 do Look);
  - no `Maps.json`, no laço do `game.ps1 worlds` e nos cenários.
- [x] **O World_TestSalt2 na base nova** (16/10, o Felipe pelo Look: a areia 3, "vai de 3", e as trilhas v3, "um baita avanço, tá aprovado"):
  - remontado com a areia de duna e o chão v3, e ligado no jogo com `"ground_v3": true` no `Maps.json`;
  - no jogo bate com o World_TestGroundV3Dune: diferença média de 0,9/255 (`captures/game/salt2v3_look_zoom` × `gv3d_look_zoom`);
  - o `captures/dist` não mudou.
- [~] **A laje no mapa do jogo e o acabamento por estrada** (16/10, o Felipe pelo Look: "gostei da laje 3D, tá aprovado, mas lembre-se que tinha mais de uma variação" e "também não descartei a original que é colada no chão"):
  - a laje virou o padrão do deserto no WorldForge (`ROAD_SLAB=0` tira);
  - o painel ganhou, por estrada, o "Acabamento do asfalto": Auto (o desgaste por trecho, a vila inteira), Colada (sem laje, o asfalto v2 no chão), Inteira, Gasta e Destruída. Vai nas edições do mapa (`"style"` na estrada); testado em `WorldForge/Tools/test_road_style.py`;
  - a faixa continua o decalque (a da laje aprovada); a faixa nova do Look espera a palavra dele;
  - o World_TestSalt2 com a laje: na fila com o ok do jogo (16/10), com a conferência do tanque em cima da laje.
- [~] **O tanque derruba e amassa: o desenho do WorldForge** (16/10; o estudo e o que já está feito no `WORLDFORGE.md` §12.8; a regra e o evento são do jogo, `docs/ESMAGAR_E_DERRUBAR.md`):
  - o capim e a moita deitando na trilha (`TRAMPLE=1`, o mapa `World_TestTrample`; o jogo liga o `UseTrample`);
  - o toco e o tronco quebrado das palmeiras, com a copa que chicoteia e assenta no chão;
  - o fio solto que balança, arrebenta e deita (o plugin `RTSWorldProps`);
  - falta: a prova no jogo, a pedra pequena quebrando e o acabamento fino (o tronco estalando).
- [x] **As três areias no painel do WorldForge** (16/10, o Felipe: "pq não deixar as 3 opções no WorldForge e no jogo?"):
  - "Areia (no acabamento)" no deserto: Duna (o padrão), Amarela (a v2, pra sol alto) e Clássica (reserva);
  - vai no JSON do mapa (`"sand": 1|2|3`); a montagem e o jogo usam a do mapa.
- [ ] **Lote plano da War Factory em cada base,** sem seixo nem marola, com a estrada chegando na porta e a saída marcada.
- [ ] **Tipo de chão por camada** (areia, cascalho, terra, rocha, salina, asfalto): dá a cor da poeira e dos torrões.
- [ ] **"Memória do chão"** no material do terreno, pros rastros de esteira, o queimado e a cratera.
- [ ] **Simetria:** as pedras pequenas não ficam iguais entre os dois lados (o deslocamento não gira com o mapa).
- [ ] **Mapa de 4 bases** se for 2v2 ou cada um por si. Já existe o candidato a mapa oficial 1v1, o "AlUla" (`docs/concept/maps/alula_1v1_candidate.webp`).
- [ ] **Mapa sem campos de recurso,** já que não há economia.
- [ ] **Minimapa do mapa pronto,** ou captura feita pelo jogo.
- [ ] **Aprovação da fase 1,** que o mapa do jogo já usa: oásis, tamareiras, salina, água e borda de serra. O Felipe nunca viu, e não tem commit.
- [ ] **Chão do deserto:** está mais vazio e liso que o alvo. Faltam pedregulho e cascalho em grupos.
- [ ] **Folhagem** lendo o vento único.
- [ ] **As marcas de pneu fixas** (~1.900 decalques, aprovadas) vão conviver com a esteira dinâmica: decidir se ficam. A avaliação de arte as chamou de "marca de esteira" por engano.
- [ ] **Times no mapa** (`FTeamLayout`, pedido 1 do `JOGO.md`) pros mapas por time e de 4 bases, com as regras de justiça.
- [ ] **Oclusão igual por jogador:** com o giro de 180° e a câmera sempre do mesmo lado, o que fica na frente de uma base fica atrás da outra. Aceite: oclusão ≤ 3% e igual entre jogadores (±5%).
- [ ] Se o mapa oficial for o AlUla, ele depende da fase 2 do WorldForge (~2 semanas).
- [ ] O minimapa do mapa pronto tem dois donos possíveis (o jogo captura ou o WorldForge exporta): **o jogo captura** (decidido aqui em 08/10, porque já está fazendo).

---

## 5. Decisões do Felipe (travam o resto)

1. **Curva de tom e luz únicas,** a mais urgente.
   - A explosão foi aprovada na curva do BlastLab, e o mapa e o fogo da carcaça na padrão; por isso a morte sai branca.
   - Junto: quem é dono da exposição e do bloom no jogo.
2. **Uma família só de explosão:** o raymarch do BlastLab, os cartões do FXLab, o ExplosionLab ou uma costura medida entre eles.
3. **Modelos finais:** o Felipe decidiu fazer à mão (08/10). Falta: quem modela, ele no Blender ou a arte por script.
4. **Lados:** USA contra USA, só com a cor, ou USA contra GLA como no vídeo (o dobro de modelos).
5. **Vitória:** perde quem perde a fábrica?
6. **Rede:** 1 contra 1 por LAN ou IP primeiro? Tem um 2º PC? O relay da Steam fica pra depois (precisa de App ID).
7. **Névoa:** no estilo do ZH (preto no nunca visto, cinza no explorado)? Explosão dentro da névoa aparece? A coluna de fumaça alta vira pista?
8. **Minimapa:** sempre ligado ou só com radar?
9. **Fábrica:** fila de quantos? Teto de tanques? Cada um começa com tanques além da fábrica?
10. **Carcaça:** já decidido (06/10): fica como cobertura nos testes e vira opção no jogo. Falta: se bloqueia na simulação.
11. **Som** entra nesta base? Se sim, OK pra baixar o pacote da Sonniss.
12. **Mapa:** o World_TestSalt2 (1024 m, ~2:50 até o contato) ou um 1v1 do tamanho do ZH (700–800 m)?
13. **Quem integra o visual no jogo.**
14. **Liberar a sessão do WorldForge** pros pedidos da seção 4.
15. **O "manda"** pra sessão de arte levar o fogo da carcaça pro jogo.
16. **Commits pendentes:**
    - `FXLab/` (o fogo aprovado só existe lá);
    - `docs/concept/`, os `CONCEITO_*.md` e o `CREDITOS.md`;
    - a fase 1 do WorldForge;
    - as duas correções do jogo.
17. **O Vanguard** (`C:\Users\luisf\newcnc`) continua como referência de leitura?
18. **Sol do jogo:** elevação, direção em relação à câmera e cor, junto da curva de tom. Cada peça foi aprovada sob um sol: o fogo da carcaça a 10°, o mapa a 20°, o tiro bate com a arte a ~30°, as explosões às 16 h com poeira no ar. No alvo a luz é contraluz quente, com sombras pra baixo-esquerda e preenchidas; no jogo o sol vem da esquerda e a sombra sai escura e azulada.
19. **Quem monta o cenário de luz de referência** que vira o plugin comum de ambiente (BlastLab, WorldForge e jogo): **decidido (09/10), a sessão nova de gráficos** (`docs/GRAFICOS.md`).
20. **Geração automática** pra arte (codex login / APIs), sem colar cada prompt.
21. **EmberGen pago** (hoje em teste) só se as gravações CC0 da JangaFX não bastarem.
22. **O que é "trilhas":** marca de esteira, rastro de poeira ou traçante?
23. **WorldForge:** os pedidos do jogo entram antes ou depois do que já está na fila dela (ferramentas de edição, Desert Fury)?
24. **Cratera como cobertura**, com o veículo passando mais devagar (mexe na simulação). Mesmo sistema da **mureta que protege o tanque** (ideia de 09/10): as ideias de jogabilidade pra depois ficam em `docs/IDEIAS_JOGABILIDADE.md`.
25. **Teste final da base visual:** um roteiro (tanque morre a ~55 m da fábrica, ao pôr do sol), gravado em vídeo com som e comparado ao vídeo do Veo.

---

## 6. Riscos e cuidados (o que já mordeu ou vai morder)

- **Uma placa pra todos.**
  - O teste do Felipe é dele: ninguém roda nada por cima.
  - Duas Unreal com janela derrubam a 1660.
  - Seguir a trava ou a fila (`docs/GPU.md`).
- **No lockstep, engasgo vira travada de rede:** o p95 do quadro e o aquecimento passam a ser critério de rede.
- **Medição contaminada:** cozimentos na CPU de outras sessões dão número falso.
- **Memória de vídeo:** a placa tem 6 GB, e ninguém mediu tudo junto.
- **Disco:** ~32 GB livres (08/10, 15h). O jogo empacotado, os caches, a Sonniss (~7,5 GB) e os dados das explosões disputam esse espaço.
- **Links físicos** quebram calados: congelar versões.
- **Uma árvore de trabalho só, com 5 sessões:** o commit de uma leva o trabalho das outras (já aconteceu no `317aca6`). Nunca usar checkout ou stash.
- **3–4 cópias da explosão divergindo:** decidir o plugin comum.
- **Gargalo de aprovação:** as explosões andam uma coisa de cada vez, e o que a base precisa está no fim da fila delas.
- **Presets diferentes** dão vantagem no multiplayer se o corte esconder ou mostrar unidade.
- **Licenças:** IA declarada; o código do ZH só pra ler (copiar números serve, código não).
- **O repositório é PÚBLICO no GitHub** (Felipstein/explosion): cada commit publica. Commitar o `FXLab/` publica o `.ember` feito no EmberGen em teste; os arquivos do Tripo não podem entrar; som da Sonniss solto no repositório é redistribuição (conferir a licença). Créditos: o `ThirdPartyNotices.txt` (o MIT do céu e da curva do WebGL, se ela for pro jogo).
- **A câmera do jogo é 37,5° (ZH) e as artes-alvo foram pedidas a ~55°:** a folha alvo × jogo compara câmeras diferentes, e a regra de altura do prédio foi pensada a 55°.
- **Fechar processo só pelo PID:** o teste de rede abre dois jogos ao lado das Unreal das outras sessões.

---

## 7. Como os AAA fazem (fontes do levantamento de 08/10)

| Item | Técnica | Referência |
|---|---|---|
| Clarão de boca | 5–6 camadas curtas: clarão em cone, luz curta sem sombra, fumaça do cano, poeira no chão, calor, recuo | ZH `FXList.cpp` (LightPulse, Tracer); ARL-TR-909 (clarão do 120 mm) |
| Traçante | partícula alinhada à velocidade, comprimento de 1/60–1/30 s de voo, aditiva, espessura constante | ZH `TracerFXNugget`; Niagara Velocity Aligned |
| Impacto na blindagem | faíscas, flash, estilhaços, marca no casco | CoH3 (dano por zona); arte 4.2 |
| Poeira do andar | partículas nas duas lagartas, na razão da velocidade, prioridade baixa | ZH `W3DTankDraw` (TreadDebris), `ParticleSys.h` (DUST_TRAIL) |
| Marcas de esteira | tira de quads que some em 300 s; ou decal; ou render target / RVT | ZH `W3DTerrainTracks`; Rise of the Tomb Raider (GPU Pro 7) |
| Cratera e queimado | decal no relevo com limite e saída do mais velho | ZH `addScorch` (500); BAR `gfx_decals_gl4` |
| Cor do jogador | máscara num canal mais cor por instância | ZH houseColor; SC2 (alfa do difuso); Custom Primitive Data |
| Seleção e barras | barra só selecionado ou sob o mouse, 3 px; anel no chão sob o modelo | ZH `Drawable.cpp`; SC2; AoE IV |
| Névoa | textura de 1 texel por célula, 1 s de transição, visível/explorado/nunca; desfoque separável | ZH `W3DShroud`; Riot, "A Story of Fog and War"; Liquipedia SC2 |
| Fábrica | porta abre, espera e fecha; unidade nasce dentro e sai até a frente; intervalo entre saídas | ZH `ProductionUpdate`, `QueueProductionExitUpdate` |
| Água | Single Layer Water; respingo; ondinhas por render target | Unreal SLW; `NiagaraDataInterfaceWater` |
| Som | limite de vozes, prioridade, microfone no ponto de mira, HDR de áudio | ZH `GameAudio.cpp`; DICE GDC 2009; Diablo IV GDC 2024 |
| Contato com o chão | SSAO, sombras translúcidas, sombra de contato | SC2 SIGGRAPH 2008 |
| Silhueta | profundidade e estêncil próprios, contorno só onde está tapado | SC2 3.0; Tom Looman (Custom Depth) |
| Dano | estados aos 50% e 10% de vida, efeitos presos no modelo | ZH `BodyModule`, `BoneFXUpdate` |
| Tremor | "trauma" ao quadrado com ruído; clique sem tremor | ZH `W3DView`; Eiserloh GDC 2016 |
| Orçamento | prioridade por sistema de partículas, cortes por preset | ZH `GameLOD`; Niagara Effect Type |
| Coerência | tudo aprovado sob a mesma curva, exposição e luz da cena do jogo | `docs/JOGO.md` (achado do t4); unity 6-way |

O bruto do levantamento (status com prova, por área) está no journal do workflow `wf_5c0196e0-291`
da sessão do jogo, e o resumo das decisões já tomadas está nas memórias do projeto.
