# Ideias de jogabilidade (pra depois da base jogável)

Ideias do Felipe guardadas pra não perder. Não entram agora: a base jogável vem primeiro
(`docs/BASE_JOGAVEL.md`). Cada ideia guarda as palavras dele e as primeiras notas de como fazer.

---

## 1. Mureta que protege o tanque (09/10/2026)

**A ideia, nas palavras dele:** "se o seu tanque estiver atrás de uma mureta, a probabilidade do
tanque inimigo um pouco mais distante acertar a mureta ao invés de você é enorme. Mas é claro que
um tiro de tanque é hit kill na mureta e ela vai desabar, mas você não toma dano, te dando uma
vantagem."

Veio da foto de perto do jogo (`docs/concept/scenes/desert_golden_hour/best/close_game_cam_gpt.webp`): o tanque do GLA
atirando de trás do muro de barro do quintal.

**Como funcionaria:**
- **A mureta é um objeto do jogo.** Muro de quintal, mureta de barro e talvez sacos de areia ocupam
  quadrados da grade de 4 m e têm vida.
- **O tiro que cruza a mureta pode parar nela,** quando a linha do tiro passa por ela antes de chegar
  no tanque. Quanto mais longe o atirador, maior a chance, porque o tiro chega mais "rasante".
- **Um tiro de tanque derruba a mureta,** que vira escombro baixo. O tanque atrás não leva dano
  daquele tiro, mas fica descoberto pro próximo.
- **O resultado:** ficar atrás da mureta "compra" um tiro. É vantagem tática sem ser invencível.

**Notas pra quando for fazer:**
- **A chance sai da simulação,** com o sorteio próprio dela, igual nos dois PCs (multiplayer em
  lockstep, `docs/JOGO.md`). Nunca da física da Unreal.
- **Referências de gênero:**
  - Company of Heroes: cobertura verde e amarela reduz a chance de acerto, e o muro é destrutível;
  - Zero Hour: a infantaria se esconde em prédio.
  - Medir o equilíbrio: a mureta não pode virar parede infinita.
- **Liga com a arte:** o muro de quintal com portão já tem ficha aprovada
  (`docs/concept/buildings/civil/civ_wall_gate_*.webp`). Precisa do escombro (`_Rubble`) e da queda
  (`docs/PRODUCAO_3D.md` §6).
- **Prima da carcaça e da cratera como cobertura** (`docs/BASE_JOGAVEL.md`, decisões 10 e 24). As
  três seriam o mesmo sistema de cobertura.
- **Leitura de cima:** o jogador precisa ver que o tanque está coberto (um ícone ou a borda do anel
  de seleção) e ver a mureta caindo.

---

## 2. Fortificar a base: pátio, estrada e muro com portão (11/10/2026)

**A ideia, nas palavras dele:** veio da imagem do ataque à base (`docs/concept/scenes/special_base_assault/`),
com muro de verdade e chão de concreto: "isso pode ser uma feature magnífica de arrepiar na construção de uma
base! Pode ser um C&C ZH clássico, continua sendo, mas também a possibilidade de construir chão, os muros
certos, e construir estradas (algo rápido com pouco custo) só pra melhorar o visual e o fluxo, otimizar a base".

**A proposta da sessão de arte (pra discutir; nada decidido):**

O ZH continua igual: prédios, Dozer, caminhão. Por cima, o Dozer ganha três obras, **arrastadas como linha ou
área na grade de 4 m**.

**Retorno do Felipe (11/10):** a velocidade da estrada é interessante, mas os benefícios da 1ª versão eram fracos.
Ele concordou com o resto (as invenções, as facções, os riscos, o muro só perto dos prédios, a regra
clássica/completa). Versão 2, com benefícios que mudam a partida:

**O Felipe aprovou a direção da versão 2 e a linha dupla da fábrica (11/10: "tá me fazendo arrepiar de novo").**

| Obra | O benefício forte | A decisão que cria |
|---|---|---|
| **Estrada: o alcance da base** | A rede de estradas **estende onde você pode construir**: a estrada leva a base até o petróleo e o suprimento. Tudo nela anda ~25% mais rápido, os caminhões também | Expandir puxando estrada. Cortar a estrada do inimigo com artilharia **corta a expansão e a logística dele** |
| **Pátio: o pátio de manobra** | O veículo parado no seu pátio, fora de combate, **se conserta sozinho devagar**. O prédio em cima dele constrói mais rápido | Recuar pra base pra consertar (o momento clássico de "volta, conserta, volta pra luta"). Pátio grande custa, e é o primeiro alvo de um ataque |
| **Muro: a fortaleza** | Defesa construída **na linha do muro ganha alcance** (está no alto). A infantaria **ocupa as torres do muro**. Cada facção tem uma melhoria-assinatura do muro | Romper o muro vira o momento da partida: a brecha, os tanques entrando (a imagem do ataque à base) |

**A melhoria-assinatura do muro, por facção:**
- **USA:** o portão vira **barreira de energia** por alguns segundos, e nada passa.
- **China:** **muralha com ameias**: a infantaria lá em cima atira com proteção.
- **GLA:** **muro-armadilha**: quando o inimigo rompe, a brecha explode na cara dele.

**O que deixa isso de arrepiar (as invenções):**
1. **A estrada nasce do tráfego.**
   - O caminho que as suas unidades mais usam vira trilha de terra sozinho (só visual, sem bônus).
   - Com um clique, o Dozer pavimenta a trilha.
   - Você descobre o fluxo da sua base vendo o chão.
   - Junta com a ideia da economia, "estrada que nasce do tráfego dos caminhões" (`docs/ECONOMIA.md`).
2. **A guerra rasga a base.**
   - A artilharia abre cratera na estrada e no pátio; o trecho perde o bônus até o Dozer consertar.
   - A logística vira alvo: cortar a estrada do caminhão inimigo é assédio de verdade.
3. **A planta da base.**
   - Desenhar o pátio, os muros e as estradas como fantasma; os Dozers executam na ordem.
   - Salvar a planta e reusar na próxima partida (como os planos do Factorio).
   - Corta o clique repetido.
4. **A base conta a história.**
   - Começa como acampamento de terra e vira fortaleza pavimentada, murada e iluminada à noite.
   - O jogador sente o progresso, e o inimigo lê de longe quem investiu em defesa.
5. **Regra de partida** (as "gamerules" do gerador): Fortificação **clássica** (só prédios, como o ZH) ou **completa**.

**A cara de cada facção** (o kit de arte):
- **USA:** placas de concreto pré-moldado (os muros em T) com faixa azul; asfalto com faixa amarela; pátio de concreto liso com marcação.
- **China:** muro de tijolo e pedra com ameias, uma Grande Muralha em miniatura; pátio de lajota vermelha; estrada de pedra.
- **GLA:** sacos de areia, gabião, sucata e pneus; estrada de terra batida; pátio de lona e areia batida. Assinatura: o muro de sucata que se reconstrói sozinho, devagar.

**Riscos e como segurar:**
- **Muro em todo lugar (o jogo travado na defesa):**
  - o muro só pode ser construído perto dos seus prédios;
  - o custo sobe com o comprimento total de muro;
  - a artilharia e as superarmas passam por cima.
- **Clique demais:** arrastar a linha, a planta da base e a fila do Dozer com Shift.
- **Caminho das unidades:**
  - o muro e o portão entram no mapa de passagem da simulação, na grade de 4 m (determinístico, serve pro lockstep);
  - a estrada só baixa o custo do caminho.
- **Desempenho:**
  - o chão (estrada, pátio, trilha, cratera) é camada do chão, na grade;
  - os muros são peças repetidas (instâncias).

**O que pede da arte e dos modelos:**
- um kit modular por facção na grade de 4 m: reta, canto, T, ponta e portão, cada um com 3 estados de dano, mais o escombro;
- a estrada, o pátio e a trilha como camadas do chão.
- Os modelos são do Felipe; as fichas de conceito, da sessão de arte.

**Perguntas pro Felipe:**
(As perguntas da 1ª versão: muro só perto dos prédios e regra clássica/completa, ele concordou.)
1. **A versão 2 dos benefícios** chegou lá?
2. **Gerar a arte conceitual:** a mesma base nas três fases (acampamento de terra; pátio e estradas; fortaleza murada à noite), pra ver o arrepio antes de decidir?

---

## 3. Linha de montagem dupla da War Factory (11/10/2026)

**A ideia, nas palavras dele:** "upgrade do war factory (sei lá como isso e de onde), mas é capaz de produzir até
2 veículos ao mesmo tempo e produção um pouco mais rápida (para alguns veículos mais pesados continua 1 produção
única)".

**A proposta da sessão de arte:**
- **Onde se compra:** na própria War Factory, como as melhorias de unidade do ZH. **Mas não é só construir o prédio de tecnologia:** precisa da **patente de combate** (ideia 4). Pedido do Felipe (11/10): "pra chegar no nível 2 não é simples, tem que desbloquear... eu quero que as pessoas precisem ir pra luta".
- **O que faz:**
  - duas linhas de produção ao mesmo tempo pros veículos leves e médios, e a produção ~15% mais rápida;
  - os pesados (a classe do Overlord, os superpesados) **ocupam as duas linhas**: continua um de cada vez.
- **A decisão que cria:** é a alternativa a construir uma 2ª fábrica.
  - Sai mais barata e ocupa menos espaço.
  - **Concentra o risco:** se essa fábrica cai, você perde as duas linhas de uma vez. Eficiência contra segurança.
- **O arrepio visual:** a fábrica **cresce no mapa**. Ganha o 2º portão ou o anexo, e os dois portões soltam veículo ao mesmo tempo.
  - **USA:** o anexo robotizado, com os braços mecânicos (como na imagem do ataque à base do GPT).
  - **China:** o "segundo turno": mais galpão, mais luz acesa à noite, fumaça das duas chaminés.
  - **GLA:** uma 2ª garagem de sucata soldada do lado, improvisada.
- **No HUD** (`docs/concept/hud/DECISOES.md`): a fila no cartão mostra **duas casas ativas** no começo, cada uma com o seu relógio.
- **Na simulação:** duas filas paralelas no mesmo prédio. É determinístico e serve pro lockstep.

**Perguntas pro Felipe:**
1. Comprar na própria fábrica (com o prédio de tecnologia) está bom?
2. Os números de partida: 2 linhas e +15% de velocidade?
3. Quais veículos contam como pesados (só os superpesados, ou o tanque pesado de cada facção também)?

---

## 4. A tecnologia se conquista na luta: a patente de combate (11/10/2026)

**A ideia, nas palavras dele:** "pra chegar no nível 2 da War Factory não é simples, tem que desbloquear, igual algumas
coisas no C&C ZH clássico. Inclusive eu gosto do mod Contra 009: lá não é simplesmente você evoluir no seu canto,
construir o tech center e tal; lá você tem que fazer algo pra chegar, ganhar promoção. Eu quero uma mecânica parecida
no nosso jogo, pras pessoas precisarem ir pra luta!"

**A proposta da sessão de arte (pra discutir):**

1. **Uma moeda só: a patente do general.**
   - No ZH, a patente sobe com a experiência de combate e libera os poderes. Aqui ela também **libera a tecnologia grande**: a War Factory nível 2, a melhoria-assinatura do muro, as unidades de topo.
   - O prédio de tecnologia continua existindo, mas **sozinho não basta**: é prédio **e** patente.
   - Já está no HUD aprovado: as estrelas, a barra de experiência e o botão Promote no painel do comandante.
2. **De onde vem a experiência (só da luta, nunca de ficar em casa):**
   - **Destruir valor inimigo:** o dano causado, pesado pelo custo do que foi destruído. Prédio vale mais que unidade.
   - **Segurar pontos do mapa:** prédios neutros pra capturar (ideia abaixo) dão experiência enquanto você os segura.
   - **Construir e economizar em casa não dá nada.** Quem fica no canto não sobe.
3. **Contra a bola de neve (quem perde a primeira luta não fica preso pra sempre):**
   - quem **toma** dano em combate também aprende um pouco (~1/3 do que daria causar);
   - **perder lutando** ainda conta; só a passividade fica pra trás.
4. **Os prédios neutros de tecnologia no mapa:** a luta tem lugar pra acontecer.
   - Ex.: um laboratório abandonado no meio do mapa. Quem captura e segura ganha experiência contínua, e alguns liberam algo único (a planta da linha dupla, um poder).
   - Todo mundo briga pelo mesmo ponto: o "C&C" de disputar o centro.
   - Entra nas regras do gerador do WorldForge (`docs/concept/AMBIENTES.md` §4) como uma regra: prédios neutros nenhum / poucos / muitos.
5. **O sucateio (a assinatura do GLA, e talvez de todos):**
   - a carcaça do inimigo destruído pode ser recolhida e vira experiência ou peça;
   - quem venceu a luta e ficou com o campo leva o prêmio.
   - Junta com a ideia da economia, "rebocar e reaproveitar o tanque inimigo" (`docs/ECONOMIA.md`).

**Exemplo de escada (ponto de partida, medir):**

| Patente | O que libera |
|---|---|
| 1 | começo |
| 2 | 1º poder do general; muro com a melhoria-assinatura |
| 3 | **War Factory nível 2 (linha dupla)**; unidades médias de topo |
| 4 | 2º poder; superpesados |
| 5 | superarma e o poder máximo |

**Riscos:**
- **O jogador defensivo** fica sem tecnologia. O segurar pontos do mapa dá a ele um jeito de subir sem atacar a base inimiga.
- **A partida de IA fácil** sobe devagar. Os números se ajustam por medida.
- **Clareza:** o jogador precisa ver de onde veio a experiência: um "+XP" pequeno na barra quando destrói algo ou segura um ponto.

**Decisão do Felipe (11/10):** "sim, pode ser; por enquanto a patente mesmo".
- A patente é a chave da tecnologia grande (prédio **e** patente), por enquanto.
- Os prédios neutros de tecnologia no mapa: sim.
- O sucateio fica **só pro GLA**, como no ZH (11/10).

---

## 5. O engenheiro pesado: um Dozer mais poderoso (11/10/2026)

**A ideia, nas palavras dele:** veio do caminhão 6×6 de engenharia da folha de ideias do Dozer
(`captures/concept/43_usa_base/dozer_ideas_gpt.webp`, desenho 4): "pode ser uma versão de dozer mais poderoso que a
gente pode pensar na jogabilidade pra deixar o negócio mais gostosinho".

**Propostas da sessão de arte (pra discutir):**
- **Ele é a unidade das obras da ideia 2:**
  - o muro, o pátio e a estrada saem mais rápido com ele;
  - só ele constrói a melhoria-assinatura do muro.
- **Ele conserta o mundo:**
  - tapa a cratera da estrada e do pátio;
  - tira o escombro do muro caído;
  - ergue a ponte destruída (se o mapa tiver).
- **Ele monta a base avançada:** um posto de obra longe da base, que estende o alcance de construção enquanto ele estiver lá (casa com a estrada como alcance da base, ideia 2).
- **Ele custa e exige:** precisa da patente (ideia 4), sai da War Factory, e é blindado o bastante pra trabalhar perto da luta.
- **O Dozer comum continua** o construtor barato do começo, como no ZH.
