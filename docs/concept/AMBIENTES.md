# Ambientes do jogo: da arte conceitual ao WorldForge e ao look

Como os mundos do jogo nascem, de onde vêm as referências e pra onde cada uma vai. Escrito pela
sessão de arte em 10/10/2026, a pedido do Felipe. O Felipe aprova cada imagem.

## 1. A arquitetura: peças que se combinam

**Não existe mapa pronto.** No WorldForge (`docs/WORLDFORGE.md`), **qualquer pessoa monta o mapa à mão**:
- pinta o chão com as texturas do bioma;
- espalha a vegetação e os prédios;
- escolhe a hora do dia e a atmosfera.

**Qualquer combinação tem de ficar bonita.** Por isso o ambiente é feito de três peças independentes:

| Peça | O que é | Quem faz no projeto |
|---|---|---|
| **Kit do bioma** | O chão (as camadas de textura), a vegetação, as rochas, os prédios civis, a água desse bioma | Os kits no WorldForge (`M_LiveGround_<bioma>`, `M_Ocean_<bioma>`, a tabela de espalhamento por bioma); os prédios, na produção 3D (`docs/PRODUCAO_3D.md`) |
| **Hora** | A predefinição de luz: sol, céu, exposição, cor | As receitas de luz da sessão do look (`Game/Tools/look_recipes/<pasta>.json`, `docs/GRAFICOS.md`) |
| **Atmosfera e clima** | Névoa, poeira no ar, neve caindo, chuva, tempestade de areia | A sessão do look, junto com a hora |

O look vira **plugin comum** e serve a três:
- o jogo;
- o botão "ver na versão final" do WorldForge;
- o BlastLab.

O painel de montar do WorldForge continua com a luz leve.

## 2. O caminho de cada imagem

1. **A arte gera** no Gemini e no GPT. As rodadas ficam em `captures/concept/42_environments/<mundo>/`, fora do git.
2. **O Felipe aprova.** A aprovada vai pra `docs/concept/scenes/<bioma>_<hora>/`:
   - `best/` é o alvo de jogo (quase sempre o Gemini);
   - `more/` é a beleza e a vista de perto (quase sempre o GPT).
3. **A sessão do look** cria a receita de luz com o mesmo nome da pasta e mede a maquete contra a `best/`.
4. **A sessão do WorldForge** recebe por mensagem o que cada imagem ensina de chão, vegetação, prédios e água, e monta ou ajusta o kit do bioma. O WorldForge é só leitura pra arte.

**O que cada imagem aprovada ensina** (a arte anota ao aprovar):
- o chão e a cor dele;
- a vegetação;
- os prédios e o elemento-assinatura;
- a água;
- a luz e a atmosfera;
- a leitura das facções naquele chão.

## 3. Os mundos e onde cada um encaixa no WorldForge

| Mundo | Pasta da cena | No WorldForge hoje | O que a arte entrega |
|---|---|---|---|
| Deserto | `desert_golden_hour/`, `desert_night/` | Bioma existe (fatia e mapa inteiro) | O alvo nº 1 (`day_game_cam`), já usado pelo look |
| Neve | `snow_golden_hour/`, `snow_snowfall/` (aprovadas) | Bioma existe (fatia de neve, gelo na costa, riacho congelado) | O alvo de beleza da fatia; vila russa com igreja de cúpula |
| Floresta e campo | `forest_golden_hour/` (aprovada) | Bioma existe (fatia de floresta) | O alvo de beleza; vila europeia com igreja de pedra, campo, cerca viva |
| Selva | `jungle_golden_hour/` (aprovada) | **Bioma novo** | Kit novo: terra vermelha, rio barrento, arrozal alagado, palafitas, bambu |
| Estepe | `steppe_golden_hour/` (aprovada) | **Bioma novo** | Kit novo: capim dourado, terra nua, poço de petróleo, galpão soviético |
| Naval | `naval_day/` (aprovada: ilha e frotas) | As bordas **Costa** e **Ilha** e o mar por bioma já existem | O alvo do mar jogável: raso turquesa = desembarque, fundo azul = só navio, coluna d'água branca |
| Cidades | `city_modern_noon/`, `city_europe_after_rain/`, `city_asia_night/`, `city_port_dusk/` (aprovadas) | Prédios ainda são caixa provisória | **Kits de prédios:** cidade moderna, europeia antiga, asiática de neon, porto industrial |

## 4. O gerador automático: o mundo lindo por semente

**O pedido do Felipe (10/10):**
- o gerador automático do WorldForge **junta tudo** (recursos, ambientação, atmosfera, estruturas, fluxos) e gera mundos de arrepiar;
- tem **regras que se ligam e desligam, como as gamerules do Minecraft**: gerar cidades (no centro ou espalhadas), vilas, petróleo, estepes grandes, pontes e milhares de outras;
- "esperando que vocês discutam e pensem melhor ainda do que eu".

**O que já existe** (`docs/WORLDFORGE.md` §10, "Mundos vivos"):
- o gerador sorteia um **tipo de mapa** (ou o jogador escolhe), e o tipo decide o que existe e quanto;
- os eixos: borda, mar, relevo, água doce, marco, uso do solo por bioma e prédios como dado;
- o desenho moldura → espinha → centro e o contrato de câmera (oclusão, sombra);
- as conferências automáticas, entre elas o contraste das facções contra o chão (3:1 ou ΔE ≥ 20);
- no jogo, a mesma semente gera o mesmo mapa em todos os PCs (`docs/JOGO.md`, M5).

**A proposta da arte** (pra discutir com a sessão do WorldForge; nada decidido):

1. **Três camadas de controle:**
   - o **tipo**, o arquétipo sorteado;
   - as **regras**, cada uma com Nada / Pouco / Muito / Aleatório;
   - a **semente**.
   - A semente com as regras vira um **código curto pra compartilhar**, como as sementes do Minecraft. Na partida online, o mesmo código dá o mesmo mapa em todos os PCs.
2. **As regras, por grupo:**

   | Grupo | Regras |
   |---|---|
   | Terreno | borda (serra, dunas, mar, ilhas); relevo (planície, colinas, mesas, cânion); estepes ou planícies grandes |
   | Água | mar (nenhum, um lado, metade, ilhas); rio; lago; delta; gelo |
   | Gente | cidade (nenhuma, no centro, espalhada, metrópole); vilas; estradas (rala, densa); pontes; fazendas; porto; aeroporto civil |
   | Recursos | petróleo (poços, refinaria); suprimento; a carga que cai (`docs/ECONOMIA.md`) |
   | Estado do mundo | intacto, guerra recente, devastado: muda ruínas, crateras, carcaças, fogo. É o "nem toda cidade abandonada" |
   | Bioma | o principal e o de transição |
   | Hora e clima | hora fixa ou ciclo; limpo, nublado, neblina, chuva, neve caindo, tempestade de areia; o clima muda ou não durante a partida |

3. **Beleza garantida, a ideia que nenhum gerador tem:** as imagens aprovadas viram **conferência automática**.
   - O gerador tira a foto da câmera do jogo do mapa que acabou de gerar.
   - Compara a cor, o brilho e o contraste com a `best/` daquele bioma e hora, do jeito que o `look_compare` do look já faz.
   - A combinação que sai feia é ajustada ou trocada antes de chegar ao jogador.
   - É assim que "qualquer combinação fica bonita" vira regra, e não sorte.
4. **A história do mapa:** cada mapa sorteia um porquê que amarra a decoração:
   - cidade evacuada;
   - front congelado, com trincheiras e crateras velhas;
   - refinaria em chamas;
   - vila em festa pega de surpresa.
   - Sem história, sai mistura, e mistura de sistemas é o defeito nº 1 que já medimos.
5. **Transição de biomas:** o bioma principal muda pelo relevo e pela água:
   - deserto que vira estepe;
   - floresta com neve no alto da serra;
   - selva com mangue na costa.
   - É o que faz um mundo parecer um lugar, e não uma textura só.
6. **Estação do ano** sobre o mesmo kit: verão, outono (a floresta laranja) e inverno (neve leve). Triplica a variedade sem fazer kit novo.
7. **Foto do mapa:** o gerador tira sozinho a foto de câmera deitada, pro lobby e pra tela de carregamento.

**Como a arte entra:** cada mundo aprovado vira kit (seção 3), cada hora vira receita, e cada `best/` vira o alvo da conferência de beleza.

## 5. As decisões até agora

- **O Gemini é o alvo de jogo** (câmera alta, escala de RTS). **O GPT é a beleza** (câmera baixa, "fantasia"). Vale em todos os mundos, como no deserto.
- **Receita da imagem bonita** (`docs/CONCEITO_UNIDADES.md` §6.4 e §8):
  - prompt curto, uma história só (dois exércitos e uma explosão);
  - "cinematic lighting, Unreal Engine 5 look";
  - só as fichas das unidades anexadas.
  - **Anexar a cena do deserto enviesa** (a primeira neve saiu "deserto pintado de branco", até o dourado).
- **Duas trilhas:**
  - **A, o mundo livre:** a beleza, que define cada mundo. Vem primeiro.
  - **B, a mesma cena:** o cruzamento do deserto em outro bioma ou hora. Prova que as peças combinam e serve de maquete pro look. Só vem depois da A aprovada, anexando a aprovada pro estilo e a cena do deserto só pra posição das coisas.
- **A luz tem de combinar com o mundo:** golden hour não é pra todos. A neve ficou bonita com luz fria e limpa e saiu feia com dourado.
- **As facções não trocam de pintura por mapa:** o amarelo do GLA e o cinza da USA são a identidade de cada uma. O que muda por bioma:
  - a camada de cima (neve acumulada, lama nas esteiras);
  - a força da cor do jogador.
  - Recomendação da arte; o Felipe ainda pode pedir camuflagem como enfeite.
- **Leitura das facções medida nas imagens:**
  - deserto e estepe apagam o GLA;
  - cidade cinza apaga a USA;
  - na neve com luz fria a USA ainda se lê (sombra e azul) e a China escura brilha;
  - na terra vermelha da selva todas se leem.
- **Variação das facções:** cada imagem sorteia a dupla (USA × GLA, USA × China, China × GLA). Não se refaz o mesmo mundo com cada dupla.

## 6. O estado (10/10/2026)

- **Aprovados (13 cenas, folha em `captures/concept/worlds_atlas.jpg`):**
  - deserto (golden hour e noite);
  - neve (sol baixo e neve caindo, "sensacional", "mais foda ainda");
  - floresta ("sensacional"), selva, estepe ("sensacional", mas laranja demais; o look testa outras horas na maquete);
  - naval (assalto à ilha e frota contra frota, "todas tops");
  - cidades: moderna ao meio-dia, europeia depois da chuva, asiática de neon à noite, porto no anoitecer.
  - chuva: campo sob chuva, monção na selva, tempestade no mar. **A chuva em risco fino, com céu cinza claro, não mata a leitura** (regra pro look).
- **O pedido mais forte do Felipe:** a **cidade de neon à noite rodando no jogo**, com a explosão acendendo o quarteirão ("eu quero muito isso rodando!").
- **O que o motor tem de fazer pra chegar lá** (pra as sessões do look e das explosões):
  - a explosão como **fonte de luz** que acende o chão, as fachadas e as unidades em volta;
  - **chão molhado que reflete** o neon e o fogo;
  - luzes da cidade (janelas, letreiros, postes) acesas à noite;
  - **neve que levanta**: o rastro de pó atrás das esteiras e a nuvem branca no impacto;
  - **neve caindo** e casas pegando fogo;
  - **coluna d'água** branca no tiro que cai no mar e espuma atrás dos barcos.
  - **Quem faz o quê** (combinado com a sessão do look, 10/10):
    - **luz e material** (a explosão acendendo a cena, o chão molhado, as janelas e letreiros, a lua): o look;
    - **a explosão em si:** as sessões de explosão; o plugin do look entrega a luz da cena pra elas;
    - **os efeitos** (poeira de neve das esteiras, nuvem branca do impacto, coluna d'água, espuma, neve e chuva caindo): ainda sem dono. O Felipe decide.
- **Na fila:**
  - costa mediterrânea, desfiladeiro, passo alpino com represa, tundra com lago congelado, refinaria à noite;
  - os climas: tempestade de areia, nevasca, chuva de monção, neblina;
  - as outras horas de cada mundo aprovado.
- **Depois de cada mundo aprovado:**
  - o kit (câmera deitada, facções sob a luz, vista de perto, noite se tiver);
  - a mensagem pra sessão do WorldForge com o que a imagem ensina.
