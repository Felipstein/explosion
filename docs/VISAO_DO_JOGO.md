# Visão do jogo

Definida pelo Felipe em 24/10/2026, respondendo ao rascunho (`docs/VISAO_DO_JOGO_RASCUNHO.md`, com o estudo e as
fontes). Toda decisão de design, arte e som parte daqui. Mudou alguma coisa: atualizar este arquivo primeiro.

## O jogo numa frase

**O Zero Hour de hoje, num campo de batalha que muda de verdade.** É um C&C ZH (base, ritmo, facções, nostalgia), mas
a guerra transforma o mapa e muda o rumo da partida.

Nas palavras dele: "é um C&C ZH, mas tem coisa que quero que mude o rumo da partida, os prédios destruindo perfeitamente
com vida onde exatamente explodiu [...] mudanças, mudanças!"

## O nome (04/11)

**Ground Zero, por enquanto** ("então vamos de Ground Zero por enquanto, mas salva a lista"). Antes não havia nome: o projeto
era "RTS", e os créditos tinham `[NOME DO JOGO]`. O "FOUR ARMIES. ONE MAP." e o "OPERATIONS · THEATER 04" do menu eram
provisórios e saíram (ele: "brega", "sem criatividade").

**A régua dele:** nomes como Command & Conquer e Zero Hour. Jargão militar de verdade, curto, com peso e duplo sentido ("Zero
Hour" é o minuto em que o ataque começa).

**Ground Zero (marco zero)** é o ponto no chão debaixo da explosão nuclear (Trinity, Hiroshima e Nagasaki, 1945), de onde se
mede a destruição. É o nosso pilar: a nuke molda o chão e o mapa muda pra sempre. O "Zero" conversa com o Zero Hour.
- Cuidados: a expressão é comum (filme, música, livro), o que dificulta a marca e a busca; existe o *Metal Gear Solid V: Ground
  Zeroes* (2014); e ela carrega o 11 de setembro.
- **Falta conferir marca registrada e jogos com o mesmo nome antes de fechar.**

**A lista guardada** (ele: "todas as ideias foram fodas"):
1. **Scorched Earth:** a doutrina da terra arrasada, e o nosso mapa que queima e fica queimado.
2. **Ground Zero:** o escolhido, por enquanto.
3. **Shock & Awe** ("choque e pavor"):
   - É a doutrina *Rapid Dominance* (Ullman e Wade, 1996): vencer pelo espetáculo de força que paralisa.
   - Ficou famosa com o bombardeio de Bagdá em 2003, e por isso carrega o Iraque.
   - A Sony tentou registrar o nome pra videogame um dia depois da invasão e desistiu.
4. **Breaking Ground:** começar algo grande, e o chão se partindo sob a guerra.
5. **Blast Radius:** o raio da explosão, a nossa assinatura (o projeto nasceu como BlastLab).
6. **Fire Mission:** o pedido de tiro da artilharia pelo rádio.

## O campo que muda (o pilar que é só nosso)

A lista dele, pra virar protótipo no laboratório (`docs/` do laboratório e `Game/Config/RTS/LabScenes.json`):

- **O prédio destrói com vida, a partir de onde a explosão bateu.** O desabamento nasce do ponto do impacto; a ruína fica
  e muda a rua (cobertura, caminho fechado).
- **A bomba nuclear molda o chão.** Cratera de verdade, o relevo muda, o cenário em volta fica outro.
- **A vegetação queima e fica queimada.** Plantações e palmeiras pegam fogo e o mapa fica feio, preto, pelo resto da
  partida.
- **A construção aplaina o chão.** Onde um prédio é erguido, o terreno fica plano; quando ele cai, o plano fica.
- **O trator cria rampas.** O Dozer muda o relevo (uma rampa na montanha abre um caminho novo).
- **A represa estoura e a água sobe.** Como a missão da represa (a 1ª da GLA no Generals): a hidrelétrica quebra, a água
  invade e destrói a cidade.
- **E mais:** o que a gente pensar pra arrepiar, sempre com a física decidindo (cratera, ruína, carcaça, fumaça, fogo,
  poeira, água) e lendo bem de cima.

Regra técnica: o que muda a partida (caminho, cobertura, visão, água) mora na simulação determinística; o resto é desenho.

## As decisões dele (24/10)

| # | Tema | Decisão |
|---|---|---|
| 1 | A frase | **(b) o campo que muda**, com o sonho de criança guardado pra depois: "comande de cima, lute de dentro" (ver abaixo). |
| 2 | Nomes e mundo | **Bem perto do real, com poucas escapadas**, no estilo do mod Contra 009 (ex.: um tanque chinês gigante). Não 100% real (risco, como o War Thunder já viveu), não fictício. |
| 3 | Tom | **Sério, como o ZH**: nostalgia do Generals + a guerra de hoje, sem muita ficção. O peso maior é a nostalgia. |
| 4 | Entrar no tanque | **Momento especial**: olhar a visão e controlar rapidinho no WASD, bonito. Pra isso a câmera deitada (o game shot inclinado) tem que funcionar bem. |
| 5 | A base | **A normal do ZH**, com fortificação que dá bônus. Ainda vamos estudar muito. |
| 6 | Tropas | **Tudo**: blindados, infantaria, ar, mar e o que mais a gente destravar. Vai levar o tempo que levar. |
| 7 | Elenco | **Do tamanho do ZH pra cima.** O jogo é grande. |
| 8 | Generais | **Nenhum agora (como o Generals)**; depois o jogo cresce como o Zero Hour, com generais. |
| 9 | Superarma | **Liga/desliga na sala e com limites.** Estudar: o ZH deixa devastar (várias, sem limite, sem custo) e outros RTS pesam o custo por superarma. |
| 10 | Público | **Tudo**: skirmish e co-op contra a IA, LAN e, um dia, ranqueado. Como o C&C ZH e os Red Alert. |
| 11 | Campanha | **Sem campanha** por enquanto. |
| 12 | Arte final | **No site de assets dele** (assgets.vercel.app): o painel por facção (USA, China, GLA, Civis), já com as trilhas sonoras ("The Final Breach", "The Last Advance", "Strike Coordinates", "Iron Wing Horizon"). |

## O sonho de criança (depois)

"Comande de cima, lute de dentro": construir a base, pôr as unidades e, quando quiser, entrar na luta como num
Battlefield; um tipo de C&C Renegade, em que dá pra entrar nas construções e mandar gerar uma unidade. Fica guardado; o
primeiro passo é o momento especial do item 4.

## A ordem de produção

1. Esta definição.
2. Protótipos do "campo que muda" no laboratório, um de cada vez, em vídeo.
3. O elenco fechado e o conceito (os guias `docs/CONCEITO_UNIDADES.md` e `docs/CONCEITO_CONSTRUCOES.md`), com os nomes
   perto do real.
4. A arte e os modelos finais (o site de assets).
5. O som e as vozes. O som que serve a qualquer design (explosões por tamanho, impacto por tipo de chão, canhão, míssil,
   cliques, vento) pode vir antes.
6. A IA treinada de verdade no fim da v5, jogável contra humano.
