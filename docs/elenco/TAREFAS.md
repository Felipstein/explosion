# As tarefas do elenco (a lista inteira, 01/11/2026)

O Felipe, 01/11: "agora sim, não esquece de nada". Tudo o que ele pediu desde o começo (o /goal de 31/10, o da queda de energia e os
pedidos de 01/11). Marcar aqui cada item que fecha: ✅ feito, 🔄 em curso, ⏳ falta, ❌ não começado.

## O estado (01/11, 13h45)

O Felipe, 13h45: "eu falei que era pra colocar na lista, e você começou a fazer todas elas [...] não esqueça de NADA". A ordem que ele
deu às 12h: **"faça o que você já ia fazer e depois isso aqui"**: primeiro o que estava em curso (o tanque B no jogo, as unidades B da
USA no jogo, a Rússia, o resto da lista), DEPOIS os pedidos novos da USA (38, 40, 41). Eu adiantei o 38 (os agentes), o 39 e o 40 fora
dessa ordem; o 41 só tem o estudo. Daqui pra frente, a ordem da seção "A ordem" no fim.

**O PC reiniciou às 13h50** (as sequências e os 2 agentes dos prédios morreram; a fila presa foi solta; retomado às 14h com o
que cada um deixou: a War Factory pronta sem as prévias, a usina e o quartel a refazer com o script mais novo).

**A energia caiu de novo às ~15h25** (retomado às 15h35: o agente do Pentágono retomado de onde parou; as prévias da War
Factory refeitas no fim da sequência).

**Em curso agora:** o 38 (2 agentes nos 5 prédios da USA), o 40 (a tinta de campanha no jogo, rodando o A/B), as fotos dos 3 aéreos B
no jogo (16), o canto do para-lama do tanque B mal encaixado (39, achado dele às 13h40).

**Depende dele:**
- decidir: a paleta da Rússia e a gaiola (FACCAO_RUSSIA §2); o caminho do recorte do chão (22, os 3 em `docs/BASE_JOGAVEL.md`); A ou B
  nas peças da USA (15, 16, 19: o tanque B ele aprovou; as outras 8 B já estão no jogo, com o caminho da A guardado); aprovar o kit civil
  (20); as escolhas da infantaria (17, o fim do `docs/ESTUDO_INFANTARIA_VISUAL.md`); as decisões do 27;
- gerar as imagens do HUD com os prompts prontos (9);
- ver: a esteira em vídeo (14), a galeria (8), a frente nova do tanque (39).

**Depois da paleta:** os prompts do HUD da Rússia. **De outra sessão:** a farda da infantaria russa (36); a cor da Rússia no HUD.

## A. O pedido original (o elenco)

1. ✅ Elenco mínimo jogável das 3 facções (31 peças novas: prédios, veículos, infantaria, centro de tecnologia).
2. ✅ Ideias além do ZH e do Contra (o reator que derrete, o Worker que vive da ruína, a picape da carga…) em `docs/ELENCO.md`.
3. ✅ Balanceado (as contas de quem mata quem, `docs/ELENCO.md` §8).
4. ✅ Superarmas de fora.
5. ✅ A IA jogando as 3 facções (simulação 33).
6. ✅ A ficha de cada peça (funcionamento, vida, os sons de andar/construir/selecionar/morrer, projétil, temperamento): `docs/elenco/`.
7. ✅ Testar tudo no jogo: as vitrines da China, da GLA (`elenco_gla_v4`) e da Rússia, os tours das 4 e as partidas IA × IA dos 6 pares
   (01/11; `docs/ELENCO.md` §8.3).
8. ✅ A galeria pra ele avaliar uma por uma (01/11 08:30; ✅ avaliada às 15h20: §G): https://claude.ai/artifact/GGMzyq4iRM1EfMDMt82N9D (65 peças: as 4 facções,
   as versões B da USA e o kit civil; o veredito de cada peça fica no banco da página, coleção `avaliacoes`, que a sessão lê de volta).
   Refazer: `python Game/Tools/elenco_gallery.py` e publicar de novo o `captures/game/elenco_galeria/index.html` (o mesmo link).
9. ✅ Os prompts do HUD das 30 peças novas de China, GLA e USA (`docs/elenco/PROMPTS_HUD.md`, 01/11 manhã; os anexos numerados em
   `captures/concept/_attach/hud_<chave>/`). ⏳ Ele gera as imagens. A Rússia fica pra depois da paleta escolhida (o prompt se ancora
   na raiz aprovada da facção, e a Rússia ainda não tem).
   (01/11 14h20, o Felipe: "vou pegar outra sessão pra nós irmos gerar os HUDs e os unit portraits") as imagens do HUD e os retratos
   das unidades ficam com essa outra sessão; esta segue na modelagem e no resto da lista.

## B. "Tudo AAA, nada de lego" (o pedido da queda de energia, 31/10)

10. ✅ O detalhe nos 13 prédios novos (ocos, placas, telha redonda, janela recuada, adobe com quina gasta).
11. ✅ O detalhe nos 11 veículos novos (esteira de elos com forma, pneus com blocos, canos torneados, lona, equipamento).
12. ✅ O material com a sombra de contato, a sujeira nas frestas, o desgaste nas quinas e a camuflagem (`M_Structure`, as máscaras na cor de vértice).
13. ✅ Os prédios degradando (danificado, muito danificado, ruína com lajes quebradas na cor do prédio e quinas de pé): `Game/Tools/model_damage.py`,
    nos 21 prédios das 3 facções; vistos de perto no jogo (01/11, `elenco_prova_perto`, `elenco_prova_vidro*`).
14. 🔄 A esteira correndo e as rodas dos tanques girando, cada lado com a sua: vista no jogo (01/11, `elenco_prova_perto`, o T-90M passando
    a 24 m): com o casco alinhado quadro a quadro o desenho de dentro das rodas muda (giram) e a esteira de baixo anda. ⏳ Ele ver no vídeo.
15. ✅ Os aviões da USA (Raptor, Comanche, Chinook, o avião de carga e a caixa do lançamento) no mesmo nível, como versão B
    (`model_*_v2.py`, 01/11 manhã); as folhas A × B em `captures/game/models/<chave>_v2/preview/` e na galeria. ⏳ Ele escolher.
    ✅ (01/11 12h50, depois de ele dizer "as novas que você fez todas ficaram muito boas") os 3 aéreos B no jogo (`/Game/Units/usa_*_v2`,
    a A anotada em `_mesh_B` no `Units.json`). 🔄 As fotos deles: o laboratório põe a aeronave já voando e a foto do chão saiu vazia; as
    cenas `usa_b_air_*` seguem cada uma: ✅ o Comanche e o Chinook voando no jogo (os rotores girando, o Chinook com a poeira do
    rotor indo buscar a carga no depósito; `captures/game/usa_b_air_comanche|chinook`); 🔄 o Raptor (cortado no reinício das 13h50).
16. ✅ Os outros modelos da USA (caminhão, Patriot, Drop Zone, Strategy Center, Stryker, Linebacker, Airfield) como versão B, idem.
    Fraco: de cima mudam pouco (a Drop Zone, o Raptor); o Linebacker B pede o `track_step_m` 0,1603 e as rodas `wheel_*`. Achados no A:
    o prato do Strategy Center estava 3,2 m no ar (✅ corrigido); o `vls_m` do Linebacker apontava pro lado da lona (✅ conferido na malha e corrigido: o sinal do y).
    ✅ (01/11 12h50) o caminhão, o Stryker e o Linebacker B no jogo, com o Dozer e o Tomahawk B (o tour `captures/game/usa_b_tour`: os 5 de
    terra certos). 🔄 Os prédios B (a Patriot, a Drop Zone, o Strategy Center, o Airfield): ✅ (01/11 14h40) no jogo com os 3 estados de
    dano, a superfície do tiro e o Raptor estacionado no Airfield (`captures/game/usa_b_predios`). Achado nas fotos: o Airfield e a Drop
    Zone destruídos levantavam ~30 pilares no meio da pista e na areia e uma laje enorme pendurada sobre o pátio (o destrutor supunha
    o lote inteiro de prédio). ✅ Consertado no `model_damage.py` (`tall_cover`: no lote que é mais pátio que prédio, a quina, a laje,
    os pilares e o monte só no que é prédio; os 21 prédios cheios não mudam). ✅ (16h10) refeitos e vistos no jogo
    (`captures/game/usa_b_predios_v2`): o hangar quebrado com a laje do próprio teto caída, a torre do Drop Zone sem a cabine, o pátio
    rachado com entulho, nenhum pilar na pista; o xadrez cinza a 60% era o shader compilando (sumiu na 2ª rodada).
17. 🔄 A infantaria (hoje o soldado de outra sessão, `docs/ESTUDO_INFANTARIA_VISUAL.md`): avaliar contra o nível novo e melhorar.
    ⚠ (01/11 18h50, o Felipe: "você tá atrasado") a sessão do jogo ("Análise do modelo técnico Blender") JÁ FEZ a infantaria em 30–31/10
    (`docs/PENDENCIAS_NOITE.md` itens 16, 33 e 36): o soldado vestido (a farda pintada nas UVs, o capacete moderno, as peças nos ossos),
    a escala 1,25×, as mortes que deitam (o boneco de pano no fim), o sangue, a cobertura, a supressão, as posturas, as pegadas, o tiro
    a 900 m/s. As decisões do meu estudo de 30/10 ficaram velhas. Regra dele: NADA comprado ("jamais vou comprar coisas aqui").
    🔄 O que sobra pra ele decidir: a cor do time (hoje uma por facção no capacete e no colete: USA azul-escuro, China vermelho, GLA
    verde, Rússia o MESMO vermelho e as mesmas peças da China): a cena `infantaria_4_faccoes` (as 4 lado a lado, de perto e no jogo).
    ✅ Avaliada (01/11, os tours, de perto): é a peça mais fraca do jogo ao lado dos prédios e veículos novos. O corpo é o manequim da
    Unreal com a farda lisa de uma cor só (sem camuflagem nem tom), as poses iguais e duras, e o fuzil de ficção científica do modelo da
    Unreal, cinza-claro, que no sol lia como brinquedo prateado. ✅ O fuzil no aço escuro de arma (`RTSUnitActor`; visto no jogo de perto,
    `captures/game/elenco_prova_tecnica_v2`). ⏳ O resto é o plano do estudo (§8: o corpo do MPFB2 ou o soldado comprado, a farda com camuflagem, o equipamento nos
    ossos, as animações de captura), que espera as decisões dele no fim do estudo (comprar ~US$ 60 × por script, a escala, a cor do time).
18. ✅ Os detalhes adiados, todos no código (01/11), vistos no jogo: o vidro quebrado (no material: as vidraças estouram e racham com o dano); a rachadura nascendo nas quinas dos vãos (no material: cresce primeiro na fresta das máscaras); os pedaços caindo (o StructureBreak de cada troca de estado solta 8–40 lascas que caem pela fachada, mais a poeira).
    (01/11 09h30, a prova de perto `elenco_prova_perto`, o quartel da China de 100% à ruína: ✅ as mordidas no telhado, o piso rachado,
    o fogo e a ruína queimada; ✅ os pedaços caindo, que saíam cinza-claros e liam como papel branco na areia: agora na cor da parede
    do prédio (`StructureBreak` com a cor da instância de concreto, reboco ou corpo; o A × B em `captures/game/elenco_prova_perto*`);
    ✅ o vidro quebrando (o Instituto russo, `elenco_prova_vidro*`: a estrela na vidraça aos 60%); ✅ a rachadura, que aos 60% cobria
    a fachada e o piso inteiros e lia como mosaico de pedra: agora nasce nas quinas dos vãos e perto dos tiros e só se espalha com o
    prédio quase caindo (o limiar com o dano ao quadrado; a escala fina só onde a tensão concentra; `import_provisional.py`, o M_Structure
    refeito). A 1ª partida depois de refazer os materiais mostra o xadrez no prédio danificado enquanto o shader compila: é a compilação.)

## C. Os pedidos de 01/11

19. ✅ O tanque principal da USA, o Tomahawk ("o de atirar longe") e o Dozer fiéis à ficha, como versão B (a aprovada fica guardada).
    - ✅ Os 3 B (01/11):
      - `model_usa_mbt.py`: 246 mil triângulos, batido contra a ficha nas 5 vistas;
      - `model_usa_tomahawk.py`: o HEMTT 8×8;
      - `model_usa_dozer_v2.py`.
      As folhas A × B estão em `captures/game/models/usa_mbt|usa_tomahawk_v2|usa_dozer_v2/preview/`. Os 3 entram na galeria.
    - ✅ (01/11 12h) **o tanque B aprovado** ("você refez o tanque e ficou super bom"; `docs/APROVADOS.md`); o Tomahawk e o Dozer B ainda
      sem a palavra dele. ✅ (01/11 12h40) o tanque B no jogo: importado em `/Game/Units/usa_mbt_v2` (a A guardada em
      `/Game/Units/USA_MBT_LL`, os caminhos dela anotados em `_mesh_B` no `Units.json`), as rodas uma a uma, a esteira que anda
      (passo 0,178), a caixa da torre medida na malha B; visto parado, atirando e andando (`captures/game/elenco_prova_tanque_usa`).
    - ✅ (01/11 12h50) o Tomahawk e o Dozer B no jogo (as rodas `wheel_*`, o passo da esteira do Dozer 0,211, o pneu do Tomahawk 0,30).
    - ✅ (01/11 15h20) o Tomahawk e o Dozer B APROVADOS na galeria.
    - Com o ok (feito acima, o que ele aprovou e o que ele elogiou):
      - importar o B;
      - no `Units.json`: as rodas `wheel_*`, a esteira `TankTrackRun` com `track_step_m` 0,178 e 0,211, a boca em x 5,80, a caixa da
        torre nova;
      - o Tomahawk passa a ter 8 rodas, com o pneu de 0,30.
20. 🔄 O kit civil dos game shots: ✅ modelado (01/11, `docs/KIT_CIVIL_V2.md`):
    - 5 carros e as carcaças, 6 casas e muros que substituem as de hoje e a ruína nova;
    - 15 miudezas;
    - a rua: o meio-fio, a calçada, o bueiro e a faixa.
    Entra na galeria. ⏳ Falta:
    - ele aprovar (depois copiar pra `captures/look/civil_glb/` guardando os de hoje);
    - as peças novas no gerador da vila (WorldForge);
      🔄 (01/11 19h50) 1ª parte: as 4 que a vila já usa (barris, caixas, sacos, poste) trocadas pelas v2 aprovadas (o mesmo nome; as de
      antes em `captures/look/civil_glb_antes_0111`) e as 20 novas importadas (prontas pro gerador); a foto A/B da vila (`vila_foto_perto`,
      `vila_rua_perto_salt2`). ⏳ 2ª parte: o gerador (`WorldForge/.../WorldVillage.cpp`, a tabela Kits) espalhar as novas, com estudo.
      🔄 (01/11 21h20) 2ª parte, o que não bloqueia: `Game/Tools/village_kit_v2.py` troca a MALHA das miudezas da maquete pelas do v2 no
      arquivo da vila (a caixa d'água em 3 jeitos: a de plástico, a na torre de ferro e o tanque deitado; o ar-condicionado, a antena, o
      varal, o toldo, os pneus, o entulho; 1 em 5 postes com o transformador); o type fica (a peça amassa igual), a simulação não muda.
      Determinístico (a mesma vila nos 2 PCs da LAN), confere a mureta e as vizinhas no teto (a que não cabe fica a da maquete: 10 no
      Salt2). O WorldForge é de outra sessão com trabalho aberto: o gerador não foi recompilado. Foto C pelo `chain45`. ⏳ As que bloqueiam
      (a cerca de chapa, a barreira, a caçamba, a barraca, o poste de luz) precisam da grade: ficam pro gerador (`KIT_CIVIL_V2.md` §3).
    - ✅ a carcaça queimada ainda lia como camuflagem: a lateral em carvão e a ferrugem só no alto (01/11, os 5 carros refeitos);
    - ✅ a casinha da escada igual em todas: a casa comprida com a escada de fora;
    - cortar ~25% dos carros grandes (os inteiros: a picape 55 mil, a van 64, o caminhão 70; o excesso é a grade do detalhe nas peças
      compridas, e com o Nanite pesa pouco: fica como está);
    - a mesquita e os prédios de cidade.
21. ✅ O túnel do Arms Dealer:
    - ✅ a rampa descendo de verdade até a boca do túnel;
    - ✅ o chão sem a laje quadrada (os muros nascem da areia, pedras no pé);
    - ✅ as luzes em pares simétricos;
    - ✅ o veículo subindo do túnel (`visual.tunnel_exit`), visto no vídeo `captures/game/elenco_gla_v4`; a mancha azul (a silhueta do
      veículo debaixo do chão) saiu.
22. ⏳ O prédio pronto recorta o chão do mapa, como mecânica do jogo (`Game/Tools/ground_cut.py`, `visual.ground_cut_m`). **Desfeito em 01/11
    de manhã:** a 3ª coleção de parâmetros (`MPC_GroundCut`) passou do limite de 2 por material, e o chão do mapa caiu no xadrez
    (todas as sessões). O conserto já está no código (01/11 manhã): os retângulos moram na `MPC_Ground` (o C++ do `RTSGroundCut::Push`
    e o `ground_cut.py`, que soma os parâmetros sem tirar o `GroundV`). ⏳ Compilar (UBT na fila), aplicar e o A/B do custo do
    terreno mascarado antes de ligar de vez (o `build_slice.py` só põe a máscara com `GROUND_CUT=1`).
    01/11 09h: compilado e aplicado; o chão compila, mas o buraco não abre (com e sem Nanite), e o custo não mudou. Diagnóstico e
    próximos passos em `docs/BASE_JOGAVEL.md` (o item do recorte). Desligado. A causa, confirmada no fonte da Unreal: o terreno Nanite tira
    a máscara de opacidade de propósito (e o WPO); buraco só pela camada de visibilidade pintada no editor. ⏳ A decisão dele entre os 3
    caminhos (o prédio que levanta em vez de cavar; o terreno sem Nanite; o poço raso de ~1 m).
    ✅ (01/11 19h20, o Felipe: "eu pedi pra cavar o chão, qual o problema pra cavar o chão?") a decisão: CAVAR de verdade. O plano:
    ~~(1) achar o 2º erro sem Nanite (2) medir (3) o rebaixo~~ CORRIGIDO (19h30, o Felipe: "nossas crateras não são pintadas [...] a cratera
    deforma o chão"): desde 31/10 a cratera AFUNDA de verdade (APROVADA: o deslocamento Nanite ligado no mapa do jogo, `DISPMAG`, o caderno
    do chão `URTSGroundMemory::CraterHeightCmAt`, os veículos descendo, `docs/APROVADOS.md` 31/10). O caminho certo: o prédio pronto escreve
    o rebaixo do lote dele no MESMO caderno do chão (como a cratera), sem máscara e sem desligar o Nanite; conferir a fundura máxima do
    deslocamento (a das crateras) contra a rampa do túnel do Arms Dealer e subir se precisar. O dono do caderno é a sessão do jogo/Look:
    a interface dele, com cuidado. ⏳ Na ordem, depois do que está rodando.
23. ✅ Bandeiras com física em tudo, num sistema só e na placa (`M_FlagCloth`, `SM_FlagCloth`, `visual.flags`; a Technical migrou): ✅ vistas ondulando no jogo (as 3 do Shtab, quadro a quadro, 01/11) e a da Technical andando (o pano esticado pra trás no vento do
    carro, `captures/game/elenco_prova_tecnica`). A pintura da Technical (a camuflagem urbana do atlas) é a dele no Blender: "a cor depois a gente vê".
24. ✅ Cada coisa já nasce pensada e otimizada (a regra; ver a memória `feedback-pensar-o-todo-nao-reagir`).

## D. O fechamento

25. ✅ Importar todos os modelos na Unreal e gerar o `Units.json` final (`Game/Tools/elenco_units.py`): as 4 facções importadas (01/11 08h).
26. 🔄 Os documentos: ✅ `docs/ELENCO.md` §8.3 e §9 e `docs/MODELOS_PROVISORIOS.md` (01/11); ⏳ `docs/APROVADOS.md` (o que ele aprovar).
27. ✅ **As decisões dele** (01/11 19h20):
    - ✅ a altura dos restos de parede na ruína: "tá bom assim" (APROVADA, `docs/APROVADOS.md`);
    - ✅ o desgaste por facção: ele pediu a campanha na USA (o item 40);
    - ✅ a camuflagem no material: os tanques da China aprovados na galeria;
    - ✅ o laser da Paladin: é a assinatura do Paladin v2 (o item 57; ele vê junto).

## D2. Achados de 01/11 de manhã (o que estava errado no jogo e ninguém tinha visto)

- ✅ 7 pivôs chutados no `elenco_units.py`: a lâmina do Dozer da China 2,8 m à frente (via-se solta no vídeo), o radar do Command
  Center da China, os alto-falantes da Propaganda, o prato da GLA, o radar do Gattling Tank, o laser da Paladin, o prato do Strategy
  Center. Agora saem das constantes dos scripts (conferidos contra o `preview.json`).
- ✅ O tiro nos prédios novos batia na caixa (no ar, na frente da fachada): faltava a superfície de tiro (`Config/RTS/Heights/*.tris.bin`)
  dos 24 prédios do elenco. `Game/Tools/elenco_hitmesh.py` gera, simplificada a 30 cm (0,1 a 4,6 MB por prédio).
- ✅ O chão do mapa em xadrez (o recorte, item 22): desfeito.
- ✅ Um teste de outra sessão preso 3 h no menu (o disco cheio) segurava a fila: fechado pelo PID.
- ✅ O cache de shaders da Unreal recusava gravar (o C: com 1,3 GB): o cache de 4,8 GB do instalador do Visual Studio foi movido
  (não apagado) pra `E:\movido_do_C_01-11\`. O C: ficou com ~6 GB. A paginação (23,5 GB) só volta no reinício.

## E. Depois de tudo (o pedido de 01/11 ao dormir)

28. ✅ (01/11 19h: a paleta A escolhida) A facção **Rússia** (o estudo escrito em 01/11, adiantado na espera das filas: `docs/FACCAO_RUSSIA.md`; faltam a paleta escolhida por ele e os primeiros modelos):
    - a paleta e a assinatura icônica (como as outras 3);
    - o que eles têm e o que focar;
    - as unidades e as construções principais;
    - os primeiros modelos.
    No padrão do `docs/ELENCO.md`, dos `docs/CONCEITO_*.md` e da `docs/VISAO_DO_JOGO.md`.

## F. A Rússia jogável (o pedido de 01/11, ~01:30: "já pode montar provisórios da Rússia no jogo também, a facção, com mesmo nível de balanceamento dos outros, sempre com detalhes e qualidade")

29. ✅ O material: os slots `RuPaint` (o aço frio com a camuflagem de 3 tons em manchas grandes: `Camo 2`, `CamoColor2`, `CamoScale`), `RuMetal`, `RuConcrete`, `RuRoof`, `RuRed`, `RuWhite`, e as famílias de slots da Rússia na importação.
30. ✅ O elenco no código (`elenco_units.py`, `russia()`), nos números do ELENCO §8.1:
    - 7 prédios: o Shtab, a usina a gás (+7, a bola de fogo do gás), o depósito, a Kazarma, a fábrica de telhado-serra, o Instituto, o posto Pantsir;
    - 7 veículos: o IMR-3M, o KamAZ (5 caixas), o T-90M (força 0,0186), o BMPT, o Pantsir-S1, o TOS-1A (a salva termobárica), o T-14 (0,0154);
    - 2 de infantaria: o Motostrelok e a equipe Kornet.
    A facção entra no menu quando os modelos existem.
31. ✅ Os 7 veículos (01/11 manhã): `model_russia_<chave>.py` + o kit `model_russia_vehicle_kit.py`; a folha em
    `captures/game/models/russia_sheet/russia_7_game.jpg`. A gaiola com a rede na cor do jogador, o Relikt em V, a camuflagem de 3 tons.
    O trator saiu no tamanho real (8,7 m = 0,89 do T-90M): vai na escala do jogo 0,78 (6,8 m, o apoio menor que o tanque).
32. ✅ Os 7 prédios (01/11 manhã): `model_russia_<chave>.py` + `model_russia_parts.py`; a folha em
    `captures/game/models/russia_predios_folha.jpg`. O painel pré-moldado, o telhado-serra, a chaminé listrada, a torre de Shukhov.
    Fraco: as vidraças da serra olham pra trás (no jogo a serra lê bem: as águas verdes viradas pra câmera); ✅ o quartel liso de cima
    (01/11 09h50: as 2 chaminés de ventilação com o chapéu, os 3 exaustores na água da frente, o mastro da antena e a parabólica;
    destrutor, tiro e importação refeitos).
33. ✅ os pivôs e as medidas no `RUSSIA_GEOM` e nos prédios (os pontos de fogo, o radar do Shtab, a ponte rolante, o Pantsir fixo);
    ✅ a facção só entra no menu com os modelos IMPORTADOS (antes: prédios invisíveis); ✅ o destrutor nos 7 prédios; ✅ importados
    (01/11 08:00) e a Rússia no menu.
    ✅ (01/11) os tetos planos e os rufos dos prédios saíam com a camuflagem dos tanques (lia como neve no telhado): os prédios usam
    o `RuSteel` (o mesmo aço frio, liso; `import_provisional.py`), trocado nas 32 malhas já importadas sem reimportar.
34. 🔄 A prova:
    - ✅ a vitrine da Rússia no jogo (`captures/game/elenco_russia_v1`, 44 s, 0 falhas, 30 qps) e o tour (16 fotos);
    - ✅ (01/11) os tours: o trator e o caminhão saíam andando sozinhos e a foto deles ficava vazia (China e GLA também): agora
      nascem de novo na hora da foto, com 1,5 s de pausa;
    - ✅ IA × IA sem placa (01/11 09h; o teste agora começa sem o botão do menu e grava o placar a cada minuto; os logs em
      `captures/game/aivsai_*.log`). 10 partidas de 15 min, dos dois lados:
      - a Rússia vence a China (as duas, aos 12,5–13,6 min) e a GLA (as duas, aos 10 min) e perde para a USA (as duas, ~12,7 min);
      - a China vence a GLA (as duas); a USA vence a GLA; USA × China sem vencedor aos 15 min (a USA na frente);
      - a ordem com a IA de hoje: **USA > Rússia > China > GLA**. A Rússia fica no meio (não domina todas). O lado não muda o resultado.
      - Achado: a GLA é a mais fraca nas partidas da IA (aos 2 min, $3 000–3 750 de exército contra $5 000–6 100 das outras). A causa
        não é número da GLA (por dólar ela está na régua): a IA divide a fábrica por igual entre os armados da lista (o 1º vale 2;
        `SkirmishAI.cpp`, "o armado de linha pela mistura") sem olhar o exército do inimigo, e metade da GLA é anti-infantaria (a Technical,
        tiro leve, e o Toxin Tractor, veneno: 25% contra a blindagem de tanque). A Rússia e a China gastam 2/3 em tanque. O conserto é da IA
        (a v5: a mistura pesando o dano contra a blindagem que o inimigo mais tem, como o ZH escolhe o contra), não dos números.
      - ✅ (01/11 10h) o contra na IA (`SkirmishAI.cpp`; os testes da simulação e o determinismo passam) e o torneio de novo: ninguém ganha
        tudo, a GLA sai do zero (1 vitória, 2 empates) e a Rússia fica um pouco acima (`docs/ELENCO.md` §7.5 e §8.3).
    - ✅ o lote limpo: o prédio tira as pedras, moitas e tufos do mapa do lote dele (o pátio do quartel russo saiu limpo);
    - ✅ a cena da paleta (`elenco_4_tanques`: os 4 tanques lado a lado, perto, no jogo, longe e de cima): de cima a Rússia sai a mais
      escura (L 0,17 contra USA 0,34, GLA 0,30, China 0,24, areia 0,55); se a paleta A ficar, clarear até ~0,30 (FACCAO_RUSSIA §2).
    ✅ (01/11 12h) o braço hidráulico do TOS-1A, que não acompanhava o bloco levantado: os 2 cilindros de elevação em 3 estágios
    telescópicos (o cilindro na mesa, o do meio, a haste no olhal do bloco), peças próprias que o jogo aponta uma pra outra a cada
    quadro, o do meio deslizando metade da extensão (de 0,90 a 2,01 m; `model_russia_tos1a.py` `rams()`, `visual.launcher.ram`,
    `ARTSUnitActor::UpdateRams`); visto no jogo, deitado, subindo e no alto (`captures/game/elenco_prova_tos`).
    Fraco, anotado: a lona do jogador
    é grande de propósito (§2 do FACCAO_RUSSIA: ~8–10% da vista de cima).
35. ✅ As fichas de texto em `docs/elenco/russia/` (16, com a linha "Estado") (o mesmo padrão das outras) e as fotos de botão do HUD (no fim, com os prompts).
36. ⏳ A infantaria da Rússia: hoje veste as peças da China na farda cinza-esverdeada (provisório); a farda própria é da sessão da infantaria.
37. ✅ (01/11 12h, o Felipe: "a textura ficou quadriculada [...] pixelada, tá longe de ser uma textura foda, detalhada, de um RTS AAA
    de arrepiar") a pintura dos veículos russos. A causa, no material (`import_provisional.py`, a camuflagem): a da Rússia herdou o
    código da digital da China, que arredonda a posição numa grade (4 cm) antes de escolher a cor: a borda da mancha sai em escada
    e seca, sem a borda borrada da tinta de pistola; e o tom por chapa no máximo (`PanelTone` 1,0) deixa os blocos vizinhos
    alternando claro e escuro, um tabuleiro. O plano: a mancha orgânica sem grade (o ruído com o domínio torcido), a borda
    borrada de ~3 cm sem serrilhado (pelo tamanho do pixel na tela), o tom por chapa mais baixo, e conferir a China (a digital
    dela é de propósito, a Type 07; ver se também lê como pixel de perto).
    ✅ Feito (01/11 12h20): a camuflagem de 3 tons da Rússia sem a grade (a mancha do ruído com o domínio torcido, a borda borrada de
    tinta de pistola, nunca mais fina que o pixel na tela) e o tom por chapa da tinta russa 1,0 → 0,45; o M_Structure refeito. Visto no
    TOS-1A e no T-90M de perto (`captures/game/elenco_prova_tos_v2`, `elenco_russia_tour`). A digital da China (16 cm) ficou como é.

38. 🔄 (01/11 12h, o Felipe: "refazer as construções iniciais da USA, ficaram todas quadradas, sem vida, sem qualidade [...] as novas
    que você fez todas ficaram muito boas, inclusive algumas unidades já iniciais da USA [...] você refez o tanque e ficou super bom,
    mas os outros continuaram no lego ainda"; "faça o que você já ia fazer e depois isso") os prédios iniciais da USA (o Command
    Center, a usina, o Supply Center, o quartel, a War Factory...) e as unidades antigas dela ainda "lego", no nível do elenco novo:
    o detalhe, as máscaras, os 3 estados de dano, a superfície do tiro, fiéis às fichas aprovadas da USA (`docs/concept/buildings/`).
    🔄 2 agentes modelando (`model_usa_<chave>_v2.py` → `captures/game/models/usa_<chave>_v2`): o Command Center, a usina e o Supply
    Center; o quartel e a War Factory. Correção dele (13h): da `r1_raid_slabs` só a War Factory é aprovada; o resto sai do estudo
    "uma palavra" de cada prédio. ⏳ Depois: os estados de dano, o tiro, a importação, o `Units.json` e as fotos.
    ✅ (16h50) o Supply Center, o quartel e a War Factory B no jogo com os 3 estados de dano, o tiro e as bandeiras
    (`captures/game/usa_base_predios`; a WF com os 2 ventiladores girando). Achado: o Supply Center destruído deixava 3 pilares soltos
    no pátio da frente (0,57 do lote coberto, logo acima do limite do pátio): o limite 0,55 → 0,65 e refazendo só ele.
    ⏳ As unidades antigas ainda "lego": a Paladin (vira o 41) e a infantaria (17).
    ❌ (01/11 14h10, o Felipe vendo as prévias) **o Command Center REPROVADO**: "ficou muito básico, parecido com a anterior, quero ideia
    nova, de arrepiar, foda, quero bem feito [...] bem pensadas". **A usina REPROVADA**: "não tem cara de power plant, quero outra
    variação, outra coisa bem feita". Os dois do zero: estudo novo (o que faz um QG e uma usina lerem de cima nos RTS e no real, a
    palavra de cada um), ideias diferentes das de hoje, e só então o modelo (`docs/APROVADOS.md`, as imagens). O Supply Center, o
    quartel e a War Factory: ainda sem a palavra dele.
    🔄 (14h15) o estudo novo em `docs/elenco/USA_CC_E_USINA.md`: o Command Center vira **o Pentágono** (5 alas em volta do pátio, a
    torre do radar de faces planas tipo SPY-6, o domo do satélite girando, o heliponto numa ala, o portão do Dozer na frente); a usina
    vira **a subestação com a torre de alta tensão** (a torre de treliça com os cabos, os transformadores com os isoladores, o
    microrreator aceso; a torre de resfriamento é da China). Um agente modelando os dois (`model_usa_command_center_v3.py`,
    `model_usa_power_plant_v3.py`); a integração no jogo comigo.
    ✅ (16h20) os dois modelados (a folha `captures/game/models/usa_cc_usina_v3_folha.jpg`): o Pentágono de 5 alas em dois anéis, o
    poço de luz, a torre com os 4 hexágonos do radar e o domo girando, o heliponto (só cenário), o portão do Dozer em +X, sem laje
    quadrada (a regra da galeria); a subestação com a torre de treliça de 11,8 m e os 3 cabos, os 2 transformadores, o microrreator em
    contêineres com a janela do núcleo azul (o slot novo `UsCore`; o `Core` quente das outras fica). 🔄 No jogo: o destrutor, o
    Units.json, o tiro, a importação e as fotos da base (`usa_base_predios_v3`).
    ❌ (17h35, o Felipe) **o Pentágono REPROVADO como Command Center** ("muito fraco, o command center tem que ser outra parada"; "o
    pentágono pode até ser o bagulho de inteligência, mas ainda tem que melhorar"). **A subestação NO RUMO** ("até gostei, mas faltou
    mais um aspecto que defina que é um power plant"). O combinado com ele (17h40, "pode, segue com essas"):
    - a usina: o **domo de contenção** (a cúpula baixa de concreto da usina nuclear, com o anel azul do núcleo aceso no pé) no lugar dos
      contêineres do reator, e **o pulso de luz azul correndo nos cabos** da torre enquanto gera (apaga sem energia);
    - o Command Center: **3–4 direções de conceito no Atelier** pra ele escolher antes de modelar (duas tentativas reprovadas: muda o
      método); o jogo segue com o Pentágono no lugar até o novo;
    - o Pentágono vira o prédio de inteligência (o 56: o Pentágono com a radomo grande no pátio e as menores nas alas).
    🔄 (18h20) a usina: o domo de contenção modelado (5,1 m, o anel do núcleo aceso, a escotilha, a galeria; o agente) e o pulso azul nos
    3 cabos (o slot novo `PowerLine` com o `Pulse` no material: duas faixas acesas correndo; apaga sem energia); no jogo depois da rodada
    dos materiais. ❌ (18h) as 4 direções do Command Center no Atelier reprovadas ("genéricos, nada icônico"); 🔄 a 3ª rodada, fora da
    curva (o avião do fim do mundo no hangar, o porta-aviões em terra, o caminhão que se desdobra, a águia): esperando ele escolher.
    ❌ (18h35) as 4 ideias "fora da curva" também reprovadas ("um lixo, parece jogo de criança; desiste dessa IA, pensa você, faz você,
    olha o C&C ZH, nem que copie quase tudo"). 🔄 O Command Center v4 = o do ZH quase copiado (a referência em
    `docs/concept/buildings/ref_zh/zh_usa_command_center.jpg`: o galpão de teto curvo, o hangar meia-lua com a estrela do Exército,
    a torre de treliça com o prato do radar, o bloco de rádio com as antenas), no nível AAA das aprovadas; um agente modelando.
    ✅ (20h) o v4 modelado (a comparação com o original na mesma pose: `captures/game/models/usa_command_center_v4/zh_x_v4.jpg`): a torre
    com o prato de 4 m girando, o galpão de teto curvo em 6 gomos com a faixa azul, o hangar meia-lua com a estrela do Exército no flanco e
    o portão aberto (a saída do Dozer em +X), o bloco de rádio com as 5 antenas, a cerca, a placa, as 3 estrelas no chão; sem laje no lote.
    Fraco (do agente): o teto curvo ainda lê "tubo branco"; a torre mais curta que a do ZH. 🔄 No jogo (`chain42`: o dano, o tiro, as fotos).
    (20h30, o Felipe: "você aprendeu a icônica forma [...] agora obviamente você vai fazer um modelo novo do zero, porque não se pode
    copiar de outro jogo") 🔄 o v5 ORIGINAL: a gramática do ZH com as nossas formas (`USA_CC_E_USINA.md` §6: o mastro tubular com o radar
    retangular girando, o salão com o teto em serra e a sala de operações em balanço, o pavilhão do portão seccional com o disco da
    estrela, o bloco de rádio); um agente modelando. O v4 entra no jogo só como provisório até o v5.
    ⚠ (18h20) o disco C: chegou a 0,11 GB: a pasta de rascunho de uma sessão parada desde 29/10 (5,3 GB) movida pro E:; voltou a 15,7 GB.
    ✅ (17h45) a base inteira da USA no jogo com os prédios novos (o Pentágono e a subestação v3 no lugar até os definitivos), os 3
    estados de dano funcionando (`captures/game/usa_base_predios_v3`). O Atelier falhou no conceito do Command Center (o GPT pelo
    Codex: erro 126; o Gemini: sem a chave); o pedido salvo em `captures/concept/67_usa_command_center_4_direcoes/` pra refazer pelo
    app. (17h50, o Felipe: "cuidado pra não gastar muito") no máximo 2 agentes por vez: os da Rússia e da China parados no começo e
    relançados um de cada vez quando os 2 de agora terminarem.
39. ✅ (01/11 12h50, o Felipe: "o tanque você fez ficou muito bom, mas a frente dele não tem uma descida ali, olha o gameshot pra você
    ver, é isso mesmo, de resto tá ficando tudo muito bom, parabéns") o tanque B da USA: o glacis, a chapa inclinada que desce do teto
    do casco até o nariz, como no game shot e na ficha (`model_usa_mbt.py`; o nariz era quase em pé e o glacis raso). Feito 13h20: o
    glacis desce do deck (1,60 m) a partir de x = 2,40 até o nariz a 1,16 m; a escotilha, a solda e a saia da frente seguem a rampa;
    no jogo em `captures/game/elenco_prova_tanque_usa_glacis` (antes: `elenco_prova_tanque_usa`).
    ✅ (13h40, o Felipe: "olha ali no canto, tem coisa mal encaixada, mal feita") o para-lama de cima da saia seguia reto a 1,47 m até o
    nariz, e o nariz agora desce a 1,19 m: 28 cm de barra solta no ar na frente. Agora a aba desce junto com o topo da saia da frente
    (feito; e a fileira de parafusos da chapa da frente, que também ficava no ar, segue o topo que desce). No jogo, de perto:
    `captures/game/elenco_prova_tanque_usa_paralama` (o resto do casco conferido nas 5 vistas do Blender).
40. 🔄 (01/11 13h, o Felipe: "as texturas da USA, eu acho que estão ruins ainda, olhe os game shots [...] as outras facções você
    fez muito bem as texturas, acho que a USA também merece, além de remodelar os modelos, uma texturização") a pintura da USA no
    nível da China e da GLA: o desgaste, a poeira no baixo, a sujeira nas frestas, o tom das chapas, medidos contra o tanque e a
    War Factory dos game shots (hoje a USA está "limpa" desde 13/10, sem desgaste pintado). Correção dele (01/11): na
    `r1_raid_slabs` só a War Factory é aprovada; os outros prédios da base ali são provisórios da imagem gerada.
    🔄 O estudo e o plano em `docs/elenco/TEXTURA_USA.md` (o valor da tinta já bate; falta o padrão: a junta escura, a areia subindo
    do baixo, o véu de pó, a lasca escura; a cor do jogador fica forte). O material novo (`DustCoat`, `Wash`, `Chip`) só nos veículos de
    terra da USA (`UsPaint`, `UsMetal`, `UsTrackRun`, `UsRubber`). ✅ As 2 voltas nos veículos no jogo (`usa_b_tour_A`, `_B`, `_B2`; os
    números no `TEXTURA_USA.md` §5): o baixo empoeirado e a tinta quente perto do alvo; parado pra ele ver. 🔄 Os prédios (17h): a War
    Factory B no jogo contra a do alvo (o alvo bege-quente, R/B 1,21, variação 0,59; o nosso cinza-frio, 1,08 e 0,34): a parede da USA
    (`Body`, só dela) com o véu, o pó no pé, a junta escura, o escorrido e a lasca; o A/B `usa_base_predios_v4` × `_v5` (rodando).
    ✅ (19h40) a 1ª volta nos prédios no jogo: o R/B 1,08 → 1,11 e a variação 0,35 → 0,38 (o alvo 1,21 e 0,59): pouco, porque boa parte da
    parede da USA é o slot Concrete (dividido com a China e a Rússia) e só o Body mudou. ⏳ A 2ª volta: um `UsConcrete` só dos prédios da USA
    com a mesma campanha (e mais forte), medido de novo contra a War Factory do alvo. A bandeira da USA saiu com as listras e o cantão.
41. 🔄 (01/11 13h30, o Felipe: "o Paladin e o Abrams não têm diferença nenhuma visualmente [...] eu bato o olho e não consegue
    ler, não consegue distinguir, isso é PÉSSIMO pra um RTS AAA onde cada unidade é ICÔNICA e AUTÊNTICA"; "até o de atirar pra cima
    [o Linebacker] tem esse probleminha na parte de baixo, só a torre que se distingue") cada blindado da USA com a silhueta própria
    de cima, no casco também, não só na torre: o Paladin (o pesado com o laser de defesa) e o Linebacker (o antiaéreo de lagarta,
    casco tipo Bradley) redesenhados contra o Abrams B, com a prova lado a lado na câmera do jogo (a folha "quem é quem").
    ✅ O estudo e a regra (`docs/elenco/SILHUETAS_USA.md`: cada um diferente em tamanho, contorno do casco, torre, assinatura,
    laterais e rodas). ⏳ Modelar o Paladin e o Linebacker novos, importar, a folha "quem é quem" e o teste cego de silhueta.
42. 🔄 (01/11 14h, o Felipe: "tem falhas na lagarta, olha ali, buracos entre os pedaços dos lados e no centro"; "termina o que você está
    fazendo e depois vai nisso, não faz que nem antes") a esteira do tanque B da USA: os vãos entre os elos (nos lados e no meio da
    sapata) que deixam ver através. Conferir também nas outras esteiras do elenco que usam o mesmo elo (o Dozer B, o Linebacker B,
    a Paladin, os da China e da Rússia). Na ordem: depois dos itens que estão rodando.
    🔄 (18h) a causa: o elo era só as 2 almofadas com o canal de 11 cm do dente-guia (o dente de 7,5 cm não fechava) e a fresta de 16% do
    passo entre os elos. O conserto na função de todas as esteiras (`model_vehicle_parts.track_links`, `web`): o CORPO de aço do elo
    inteiro na largura, por dentro das almofadas, e a fresta dele de 5% do passo (abre só na curva). O tanque B refeito e a prova no
    jogo (`elenco_prova_tanque_usa_esteira`); as outras esteiras (o Dozer e o Linebacker B, a China, a Rússia) ganham o conserto quando
    forem refeitas (o Paladin v2 e os tanques russos novos já nascem com ele).

43. 🔄 (01/11 14h35, o Felipe: "tem um problema que descobrimos no ícone, a estrela da Rússia e a da China são exatamente iguais...
    quem deve ficar com a estrela? o que o outro deve ter? [...] a outra sessão disse que a Rússia hoje em dia não usa mais estrela";
    "termina aí, depois pesquisa, ajusta, melhora") o emblema de cada facção: pesquisar o que cada uma usa hoje (as insígnias reais
    das forças armadas: a China segue com a estrela vermelha com 八一; a Rússia usa a estrela vermelha de borda nos aviões e a águia
    bicéfala / a faixa de São Jorge / a letra tática), decidir quem fica com a estrela e qual é o símbolo do outro, e trocar onde o
    emblema aparece (as bandeiras com física, os prédios, os veículos, o HUD é da outra sessão). Na ordem: depois do que está rodando.
    🔄 A pesquisa e a decisão em `docs/elenco/EMBLEMAS.md`: a estrela fica com a China (o emblema do ELP: a borda dourada e o 八一);
    a Rússia passa à águia bicéfala dourada (o emblema das Forças Armadas desde 1997) nos prédios e bandeiras, e à tricolor nos
    blindados. ⏳ A troca nos modelos e nas bandeiras na vez dele.
    🔄 (18h25) as bandeiras: o emblema 3 (a Rússia: a tricolor no quarto de cima junto ao mastro, o pano na cor do time) e o 4 (a USA: as
    13 listras e o cantão na cor do time com as estrelas; antes um pano azul liso) no `M_FlagCloth`; as 15 bandeiras da USA e da Rússia
    no `Units.json` e o `elenco_units.model_flags`. Entram na próxima rodada dos materiais. ⏳ A águia nos prédios russos e a estrela da
    China com a borda dourada e o 八一.
    🔄 (18h45) nos prédios russos a `ru_star` (`model_russia_parts.py`) virou o ESCUDO heráldico tricolor com a borda dourada (os slots
    `Navy` → `RuBlue` novo e `Gold` → `CnGold`): os prédios novos da Rússia (o Shtab e a Kazarma do agente) já nascem com ele; os outros
    (a fábrica, o Instituto, o Pantsir) trocam quando forem refeitos. ⏳ A estrela da China com a borda dourada e o 八一.

    ✅ (19h10) **APROVADAS as duas estrelas** (a imagem `docs/aprovados/modelos/2026-11-01_estrelas_russia_china_APROVADAS.jpg`): a da
    Rússia com as bordas branca, azul e vermelha (a insígnia das Forças Armadas russas de hoje) e a da China com a borda dourada e o 八一.
    Trocadas no código, no lugar central de cada facção: a `ru_star` dos prédios e a `star_outlined` dos veículos russos; a `star_wall`
    da China (vermelha fixa no slot Red → RuRed, a borda CnGold e os traços do 八一). ⏳ Refazer no Blender e importar os modelos que
    têm estrela (os 7 prédios e 5 veículos russos, os prédios e veículos da China) e conferir de perto; as estrelas próprias de
    alguns scripts da China (a do casco da usina, a da Propaganda v1) também passam pra `star_wall`.

## G. A galeria avaliada (01/11, 15h15–15h30: "eu li tudo e dei os feedbacks")

48 aprovadas (a lista e a imagem em `docs/APROVADOS.md`, 01/11), entre elas as versões B da USA (o Dozer, o Tomahawk, o caminhão, o
Stryker, o Linebacker, a Patriot, a Drop Zone, o Airfield, o Raptor, o Comanche, o Chinook, o avião de carga, a caixa do lançamento)
e o Javelin: ficam as do jogo. As 17 que voltam (a folha `docs/aprovados/modelos/2026-11-01_galeria_elenco_17_refazer_e_rumo.jpg`):

44. 🔄 **China, Gattling Cannon (REFAZER):** "parece outra coisa, um bunker talvez, dá pra reaproveitar pra outra coisa, mas pro
    gattling não; e o que ficou péssimo é a sua insistência em querer colocar sempre um quadrado de chão embaixo das coisas, o bagulho
    é redondo". A regra vale pra todos: a base acompanha a forma (o redondo pisa num redondo), nada de laje quadrada por padrão.
    (20h10) A v2: a metralhadora de 6 canos no reparo aberto que gira, a torre redonda de concreto com a faixa vermelha, o anel de sacos
    em 3 fiadas, o fosso, sem laje. Achado: na v1 os canos eram uma peça que o Units.json nunca ligou (a torre aparecia sem canos e lia
    como bunker); agora estão na torre. No jogo pelo `chain43`.
45. 🔄 **China, usina (NO RUMO):** "parece que no topo das usinas, por dentro, ficou vazando": a boca das torres de resfriamento
    (o furo/o fundo que aparece por dentro).
    (20h10) A causa: o lado de dentro da torre era uma face solta virada pro lado errado (a face de um lado só sumia e via-se a areia):
    agora parede com espessura e o fundo escuro (medido: 240 de 241 raios acertam o interior, antes 227 viam o chão). ⏳ As estrelas
    das torres ainda são as antigas (na cor do jogador): passar pra aprovada do ELP.
46. 🔄 **China, Propaganda Center (REFAZER).** (20h10) A v2: o salão redondo estilo Templo do Céu com o cacho de 16 cornetas gigantes
    girando, o telão vermelho e as 3 bandeiras; no jogo pelo `chain43`.
    ❌ (20h30) a v2 REPROVADA ("continua sem criatividade nenhuma, muda completamente a mente"). 🔄 A v3 com a cabeça nova
    (`docs/elenco/CHINA_PROPAGANDA.md`): a propaganda de hoje é a TV estatal: a emissora no "arco" da sede da CCTV (o laço que lê de
    cima), o telão de LED aceso, as cornetas e as antenas no topo, as faixas de slogan; o do ZH estudado (os octógonos empilhados com as
    placas). Um agente modelando.
47. ⏳ **Kit civil, carros e casas (NO RUMO):** "aqui já pecou [...] parece jogo de lego de novo, muitas quinas, sem detalhes"; as casas
    "quase, muitas quinas, parece lego, sem detalhes, sem texturas". As miudezas aprovadas. (Junta com o item 20.)
    🔄 (22h15) Um agente refazendo os carros e as casas (v3, em `captures/game/models/civ_v3/`, sem tocar a v2 nem as miudezas): a
    lataria amassada e desbotada com o interior pelo vidro, a frente própria do sedã e do hatch; a mureta comida, a quina gasta, o
    reboco caído mostrando o adobe, o escorrido, a parede fora de prumo; a mesma pegada (o gerador da vila não muda).
48. ⏳ **GLA, quartel (REFAZER).**
49. ⏳ **GLA, Palace (REFAZER):** "sai um pouco dessa de tudo ser mudado [murado?] assim".
50. ⏳ **GLA, Stinger Site (REFAZER).**
51. ⏳ **GLA, Supply Stash (REFAZER).**
52. ⏳ **GLA, Worker (NO RUMO):** "parece que ele tá segurando uma arma".
    (21h50) O Worker é o boneco da infantaria (`T_Soldier_GLA_WORKER`): fica com a sessão da infantaria (o Felipe vê com ela).
    **48 a 51, o plano (21h50):** `docs/elenco/GLA_PREDIOS.md`: sem muro em volta, uma forma por prédio: o quartel = a tenda beduína
    com o campo de treino; o Palace = o palácio aberto no pódio em cruz; o Stinger Site = o ninho em trevo sob a rede; o Supply Stash =
    o mercado sob a rede de camuflagem. (22h05) Um agente modelando os 4 (v2).
    (22h15) O Propaganda v3 pronto (o item 46): no jogo pelo `chain49`.
53. ⏳ **Rússia, Kazarma (NO RUMO):** "mude algumas coisas, parece um prédio normal".
54. ⏳ **Rússia, BMPT e T-14 (REFAZER):** "muito parecido com o outro tanque": a mesma regra do item 41 (cada blindado com a silhueta
    própria de cima, no casco também), contra o T-90M.
55. ⏳ **Rússia, Shtab (REFAZER):** "parece uma prisão, mas dá pra reaproveitar o modelo pra outra coisa".
56. 🔄 **USA, Strategy Center A e B (REFAZER):** "sem criatividade". (17h40) Junta com o Pentágono: o QG de inteligência = o Pentágono
    melhorado com as radomos (o agente termina o campo de radomos; depois a fusão). (17h55, o Felipe vendo no jogo) "o pentágono com a
    bola ficou muito bom, dá pra usar como o centro de inteligência, mas tem que melhorar, tirar o portão, mudar algumas coisas, mais
    icônico": o Strategy Center = o Pentágono sem o portão, a radomo grande dominante no pátio e 2 menores nas alas (o agente). A ideia nova em `docs/elenco/USA_STRATEGY_CENTER.md`: o campo de radomos
    (as "bolas de golfe" geodésicas brancas da inteligência americana) com o bunker da sala de guerra; um agente modelando (v3).
57. 🔄 **USA, Paladin (REFAZER):** "já falei sobre ele" (o item 41). O mesmo agente: `model_usa_paladin_v2.py` pela regra de
    `docs/elenco/SILHUETAS_USA.md`, com a folha "quem é quem" e o teste das silhuetas pretas.
    O Linebacker B foi aprovado na galeria; o "probleminha na parte de baixo" do item 41 fica como melhoria dele.

58. ⏳ (01/11 19h40, o Felipe) **o tanque da GLA (o Scorpion) do zero**: "tem bolhas que parece que está doente, não parece sacos de areia, o
    pneu também não está posicionado na diagonal como no game shot [...] parece que foi feito naquela versão antiga mal feita; precisa de
    uma modelagem nova, bem top, como você fez tudo até agora, nada de quinas, nada de jogo de lego, detalhes top, texturas top". O alvo:
    o game shot r1 (os tanques da GLA à esquerda) e a ficha `docs/concept/units/gla_mbt_sheet.webp`; os sacos de areia de verdade (a
    trama, a costura, o peso deitando), o pneu na diagonal na frente. Um agente quando vagar.
59. 🔄 (19h40, o Felipe: "o que é isso na frente do tanque da USA?") o glacis do tanque B: a sombra em cunha da torre facetada + 2 juntas
    em V desenhadas na chapa (riscos tortos). As juntas em V saíram; a chapa lisa com a junta reta no deck (como o M1). Refazendo.

60. 🔄 (01/11 21h40, o Felipe vendo a War Factory: "aproveitando a cena da tinta, a bandeira mal posicionada voando, e ela parece um png
    só, mal feita") **a bandeira de todo o jogo:**
    - **solta do mastro:** o M_FlagCloth prendia o pano no `ObjectPositionWS`, que na Unreal é o CENTRO DA CAIXA da malha (a doc da
      Epic), não o pivô: o pano de 1 × 1 m saía 50 cm pra ponta e 50 cm pra baixo, em todas (a Technical também). Agora o pivô do
      componente (o local (0, 0, 0) levado pro mundo). O mastro do modelo bate (a War Factory: o tubo em x 7,93–8,17, o mastro anotado
      em 8,05);
    - **o pano chapado:** 2 ondas em diagonal que crescem pra ponta (era 1,3 quase reta), a sombra na dobra, o tecido que deixa passar
      o sol (a folhagem de dois lados), o brilho de plástico tirado; o branco creme e o vermelho tijolo de campanha, a sujeira em
      manchas e o pó na barra; a bainha de lona no mastro; as listras, o cantão e as estrelas filtrados pelo pixel (longe viram o tom
      médio em vez de serrilhar). No jogo pelo `chain47` (A/B na mesma corrente, o vídeo do pano).
61. 🔄 (21h30) o Strategy Center v3 a 30%: a laje pendurada flutuava acima do pentágono (o destrutor punha a laje na altura do anel de
    dentro, por cima do anel de fora mais baixo e dos cantos vazios da caixa). O `model_damage.py` agora só faz a laje onde o telhado
    está mesmo naquela altura (vale pra todos os prédios); refeito pelo `chain46`.

**Item 38, o Command Center v5 (22h05):** o nosso do zero (`Game/Tools/model_usa_command_center_v5.py`, a gramática do ZH com as
formas próprias: o salão com o teto em serra e a sala de operações em balanço, o pavilhão de chapas com o portão e o disco da estrela,
o mastro estaiado com o radar retangular que gira, o bloco de rádio). A crítica minha antes de ele ver: o radar lê pequeno e o mastro
fino na câmera do jogo; o pavilhão branco de duas águas pode ler como celeiro. No jogo pelo `chain48` (depois da tinta).

**Item 40, a 2ª volta nos prédios (21h40):** a parede e o concreto da USA só dela (`UsBody`, `UsConcrete`; o `Body` e o `Concrete` são
do avião de carga e do canteiro também), com a MESMA luz medida em 29/10 contra o alvo (a parede ~0,28; subir pra 0,40 seria chute:
conferido antes) e o tom mais quente (o alvo R/B 1,21), o pó mais forte, a lavagem e o tom por placa no máximo (a variação 0,59 do
alvo). A/B no `chain47`.

**(19h50) A rodada grande dos itens 42 e 43** (`chain41`): os 20 modelos que já existem com a estrela aprovada e a esteira sem furos
(a Rússia: a fábrica, o Instituto, o Pantsir fixo, o T-90M, o TOS-1A, o Pantsir, o IMR; a China: o quartel, o QG, o depósito, a fábrica,
o Type 99, o Overlord, o Gattling Tank, o Inferno, o trator, o caminhão; a USA: o Dozer, o Linebacker, o Paladin v2), o destrutor dos
prédios, a importação e os tours.

## A ordem

Antes (manhã de 01/11): 22 e 23 → 13 → 25 → 7 e 21 no jogo → 19 e 20 → 15, 16 e 17 → 18 → 8, 9 e 26 (feitos ou parados nele).

Agora (13h45, pela ordem dele "o que já ia fazer, depois isso"):
1. Terminar o que está rodando: o A/B da tinta (40), as fotos dos aéreos (16), o canto do para-lama (39).
2. O que estava em curso antes dos pedidos novos: os prédios B da USA no jogo (16: Patriot, Drop Zone, Strategy Center, Airfield),
   a galeria atualizada (8: os B no jogo, o tanque com o glacis, a Rússia nova), o resto do kit civil que não depende dele (20: as peças
   no gerador da vila).
3. Os pedidos novos da USA: os 5 prédios dos agentes no jogo (38), a tinta dos prédios (40), o Paladin e o Linebacker (41), a
   esteira com vãos (42), o emblema da Rússia × China (43).
4. A galeria (§G), por facção: a USA (56, 57/41), a Rússia (54, 55, 53, 43), a China (44, 45, 46), a GLA (48 a 52), o civil (47).
5. Sempre: marcar aqui cada item que fecha e o que ele aprovar no `docs/APROVADOS.md`.
