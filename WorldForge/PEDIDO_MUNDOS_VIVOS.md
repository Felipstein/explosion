# Pedido: mundos vivos (06/10/2026)

Do Felipe, passado pela sessão de arte conceitual. Complementa o
`IDEIAS_DIRECAO_DE_ARTE.md` (o terreno conta a história, o mapa AlUla, o
marco desenhado à mão). **O pedido dele, nas palavras dele:** agora é hora
de melhorar a geração automática, "igual Minecraft, igual jogos fodas que
geram mundos lindos, cheios de detalhes": estradas, sal, ruas, montanhas
melhores e mais variadas, mapas com mar em volta como no C&C. Estudar muito
como os jogos fazem, com mais de uma opinião (agents), discutir, e inventar
algo novo se for preciso.

---

## 1. O que falta hoje (olhando as 3 capturas que ele mandou: deserto, verde, verde)

A base é boa: os lagos com praia, as bases marcadas, a mata em bosques, a luz.
O que denuncia "gerado" é isto:

1. **Os penhascos são "minhocas".** Paredes finas em fita que serpenteiam pelo
   mapa como curvas de nível, sem um platô de verdade em cima e sem massa. Na
   natureza o relevo tem corpo: serra, mesa, encosta, vale. Uma fita de 3 m de
   largura com 400 m de comprimento não existe.
2. **Não tem hierarquia de escala.** Tudo tem o mesmo tamanho: lagos, cristas,
   bosques. Um mapa bonito tem 1 ou 2 formas grandes que organizam tudo (uma
   serra, um rio, uma costa), algumas médias e muitas pequenas.
3. **A água não faz sentido.** Os lagos são bolhas soltas, iguais, com a mesma
   praia em volta, sem rio entrando nem saindo. Água corre do alto pro baixo e
   termina no mar.
4. **A borda do mapa é um muro.** Paredão reto dos dois lados. No C&C o mapa
   termina em **costa** (praia, falésia, ilhotas, mar aberto) ou em serra
   natural, não em muro.
5. **Não tem gente.** Nenhuma estrada, vila, ponte, fazenda, poço, ruína. O
   que torna o mapa do C&C memorável é a camada humana (a cidade no meio, a
   ponte disputada, a refinaria).
6. **O chão é um tom só,** com árvores salpicadas por igual.

---

## 2. Como um mundo bonito é composto (a ordem que os bons geradores seguem)

Cada camada nasce da anterior, e por isso o resultado parece real:

1. **Forma grande (geologia):** continente e costa, uma serra principal,
   planaltos e vales. Poucas formas, grandes, que se leem de cima.
2. **Água (hidrologia):** rios descendo das serras pro mar pelo caminho mais
   baixo, meandros na planície, lagos onde o rio encontra uma bacia, delta ou
   estuário na costa. O rio vem **antes** do relevo fino: o vale é cavado pelo
   rio, não o contrário.
3. **Erosão leve:** encostas com sulcos, tálus (pedra caída) ao pé dos
   penhascos, leques aluviais na saída dos vales. Leve, porque RTS precisa de
   níveis legíveis.
4. **Clima e bioma:** umidade perto da água e do lado do vento, secura do
   outro lado; transições (capim alto, arbusto, mata) em vez de bordas duras.
   Sal: salinas (deserto de sal, crosta branca rachada) em bacias secas sem
   saída, como o Salar de Uyuni ou os chotts do Saara.
5. **Gente (assentamentos):** vilas onde a vida é possível — junto ao rio, no
   cruzamento de estradas, na costa (porto), na saída do vale. Cidade maior no
   centro disputado.
6. **Estradas:** ligam vilas, bases e recursos; seguem o terreno (curvas de
   nível, evitam encosta forte, sobem em zigue-zague), cruzam o rio em
   **pontes** (que viram gargalos de jogo). Estrada principal asfaltada, as
   secundárias de terra, trilhas.
7. **Uso do solo:** fazendas em faixas perto da água e das vilas, poços de
   petróleo e oleodutos, cercas, muros de pedra, ruínas, carcaças de guerra.
8. **Espalhamento:** vegetação e pedras pelos motivos de antes (água, sombra,
   pé de penhasco), em bosques com borda, não em confete.
9. **Jogabilidade por cima de tudo:** simetria, rotas, rampas, gargalos,
   expansões. O gerador atual já pensa nisso; as camadas de cima não podem
   quebrar.

---

## 3. Como os jogos e a pesquisa fazem (pra estudar a fundo)

- **Minecraft (1.18+):** o terreno sai de campos de ruído com significado
  ("continentalidade" decide mar ou terra; "erosão" decide plano ou
  montanhoso; "picos e vales"), combinados por curvas desenhadas à mão. O
  bioma sai de temperatura e umidade. A lição: **cada ruído controla uma
  coisa que tem nome**. (Apresentação do Henrik Kniberg sobre a geração do
  1.18.)
- **Far Cry 5 (GDC 2018, "Procedural World Building"):** Houdini gera
  estradas, rios, cercas, vegetação e escombros a partir das decisões do
  artista; o artista desenha a estrutura, o procedural preenche.
- **Ghost Recon Wildlands (GDC 2017, ferramentas de terreno):** estradas
  procedurais que se encaixam no relevo, vilas e um país inteiro com poucos
  artistas.
- **Pesquisa:**
  - Génevaux et al., SIGGRAPH 2013, "Terrain Generation Using Procedural
    Models Based on Hydrology": a rede de rios primeiro, o relevo depois.
  - Galin et al., Eurographics 2010, "Procedural Generation of Roads":
    estrada como caminho de menor custo (inclinação, rio, curvatura).
  - Emilien et al., 2012, "Procedural Generation of Villages on Arbitrary
    Terrains": vilas que se adaptam ao relevo.
  - Red Blob Games, "Polygonal Map Generation for Games": costa, elevação,
    rios e biomas num grafo; leitura obrigatória e simples.
  - Azgaar's Fantasy Map Generator (código aberto): rios, cidades, rotas e
    países gerados juntos.
- **RTS:** Age of Empires (scripts de mapa aleatório: costa, florestas,
  recursos com regras de distância), Tempest Rising 2025 (mapas com marco
  central), Company of Heroes 3 (vilas, estradas e cobertura legíveis de cima),
  C&C Generals (cidade no centro, pontes, refinarias, costa).
- **Unreal 5.8 pronto pra usar:** plugin **Water** (oceano em volta do mapa,
  rios e lagos por spline, com costa e espuma), **Landscape splines** pra
  estradas que escavam o terreno, **PCG** (com Biome Core) pra espalhar vilas,
  cercas e vegetação por regras.

---

## 4. O que pedimos

1. **Estudar a fundo**, com mais de uma opinião (agents de pesquisa e de
   crítica, discutindo entre si), como os geradores bons funcionam e o que dá
   pra trazer pro WorldForge. Se nada pronto servir, inventar.
2. **Propor o plano** pro Felipe (curto, com as fontes), antes do código.
3. **Prioridade sugerida** (o que mais muda o visual por esforço):
   1. costa e mar em volta do mapa (plugin Water) no lugar do muro;
   2. relevo com corpo e hierarquia (serra, planalto, vale) no lugar das
      "minhocas";
   3. rios ligando serra, lagos e mar, com pontes;
   4. estradas e vilas (a camada humana);
   5. sal, fazendas, petróleo, ruínas, e o espalhamento pelos motivos.
4. **Medir contra referências:** pôr o mapa gerado ao lado de mapas reais do
   C&C e das artes conceituais (`docs/concept/maps/`), na mesma câmera.

A sessão de arte conceitual pode gerar imagens-alvo (o "mapa ideal" visto
de cima, em cada bioma) se ajudar: é só pedir.

---

## 5. Bugs e falhas nas pedras (capturas do Felipe, 06/10/2026)

Capturas em `captures/concept/worldforge_feedback/rocks_*.webp`.

| Captura | O que está errado |
|---|---|
| `rocks_1_fins_on_cliff` | Os paredões escaneados saem **deitados de lado, como barbatanas** projetadas pra fora do penhasco, finas e flutuando sobre o vazio. O paredão do Landscape atrás aparece liso, marrom e com a textura esticada. |
| `rocks_2_slabs_in_grass` | Lascas de pedra **espetadas no gramado em ângulos aleatórios**, como papel, com frestas e sombras de vão embaixo; o gramado sobe por cima das pedras. |
| `rocks_3_spire_in_hole` | Uma pedra grande **dentro de um buraco**: aparece o vazio preto embaixo (o terreno foi cortado e a malha não tapa). Grama encostada direto na rocha. |
| `rocks_4_boulder_pile` | A pilha de rochas é bonita, mas **flutua sobre o gramado com a borda saliente** e sombra de vão; **pontas verdes do terreno atravessam as rochas** por cima; sem pedregulho nem terra na base. |

**Padrões por trás:**
1. **Orientação:** peças de penhasco escaneado têm um "em pé" próprio. Hoje
   giram livremente (inclusive deitando). Penhasco: em pé, com as costas
   enterradas na encosta e a face virada pra fora (pela normal da encosta),
   girando só em torno do eixo vertical e com inclinação pequena.
2. **Assentamento:** testar vários pontos da base contra o terreno (não só o
   centro) e **enterrar** 20–30% da peça. Nada de borda saliente sobre o vazio.
   Se o terreno abaixo cai demais, não coloca a peça ali.
3. **Interpenetração:** o terreno não pode atravessar a rocha. Ou rebaixar o
   terreno sob a peça, ou só colocar onde a peça cobre o relevo por inteiro.
4. **Mistura com o chão (o que mais denuncia):** em volta de toda rocha,
   uma faixa de terra e cascalho no chão (máscara pela distância à rocha) e
   pedregulhos menores na base (tálus). Na Unreal: Runtime Virtual Texture
   pra a rocha "beber" a cor do terreno na base, e a técnica de mistura
   malha/terreno por RVT ou campo de distância.
5. **Composição em grupos:** rochas em conjuntos (uma grande + médias + pequenas
   encostadas), não peças soltas. Pedregulho isolado no gramado (os pontinhos
   marrons) só onde faz sentido: ao pé de encosta, em leito seco.
6. **Paredão do Landscape:** encosta íngreme toda coberta por peças de rocha
   (o Landscape liso não deve aparecer), ou material triplanar de rocha forte
   que não estica, e grama só onde a encosta é suave (máscara pela inclinação).
7. **Buracos:** onde o terreno é cortado pra caber uma malha, a malha tem de
   tapar por inteiro; senão não corta.
