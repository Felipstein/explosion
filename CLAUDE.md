# BlastLab — instruções do projeto

Projeto do Felipe: um **simulador de explosões volumétricas** nível AAA
(explosões, fumaça e interação, pensando num RTS estilo C&C). Este
repositório é a casa do jogo inteiro: o jogo (`Game/`), o simulador de
explosões, o criador de mundos, as artes conceituais e as referências.
Converse em **português**. Nomes de código, pastas e assets em **inglês**
(en-US); comentários e documentos em português.

## Onde está cada coisa

Ver `README.md` (mapa do repositório e como rodar). Em resumo:

- `BlastLab/`: o simulador, um projeto **Unreal Engine 5.8** (Windows 11,
  GTX 1660). `BlastLab/Tools/blast.ps1` faz tudo pela linha de comando
  (montar, renderizar, abrir, medir). Tudo do `Content/` é gerado pelas
  ferramentas: mude o script, não o asset.
- `WorldForge/`: o **criador de mundos** do RTS (Unreal 5.8, pedido de
  05/10/2026): gerador por semente + app com painel. `Tools/forge.ps1` faz
  tudo (setup, view, teste `-Folder`/`-UITest`). Contexto em `docs/WORLDFORGE.md`.
  A meta é uma ferramenta moderna nível AAA, não um clone do WorldBuilder.
- `Game/`: o **jogo** (Unreal 5.8, desde 08/10/2026): menu, skirmish local
  contra IA. A partida é uma simulação própria em C++ puro e ponto fixo
  (`Source/SimCore`, determinística pro multiplayer lockstep); a Unreal só
  desenha (`Source/RTSGame`, `Source/BlastFx` = a explosão do BlastLab).
  `Tools/game.ps1` faz tudo (setup, view, teste `-Folder`, vídeo `-Record`);
  `Tools/simtest.ps1` testa a simulação sem a Unreal. Arquitetura em `docs/JOGO.md`.
  **A lista viva da base jogável** (multiplayer, War Factory, névoa, minimapa, visual
  final; o que é de cada sessão) é `docs/BASE_JOGAVEL.md`: quem mexer num item marca lá.
- `reference/webgl/`: o simulador **WebGL** original, congelado na tag
  `v6-fumaca-interacao`. É a referência visual aprovada. Não mexer no JS.
- `docs/`: leia nesta ordem quando precisar de contexto:
  1. `docs/STATUS.md`: estado atual (o topo) e o histórico.
  2. `docs/MIGRACAO.md`: plano de migração, números calibrados, lições.
  3. `docs/ARCHITECTURE.md`: como cada sistema do WebGL foi feito, com
     referências (caminhos `js/...` = `reference/webgl/js/...`).
  - Arte conceitual do jogo (imagens geradas no GPT/Gemini): `docs/CONCEITO_UNIDADES.md`
    e `docs/CONCEITO_CONSTRUCOES.md` são os guias pra criar unidades e
    construções novas no padrão de cada facção. Leia antes de gerar qualquer
    prompt. As aprovadas ficam em `docs/concept/`.
  - **O visual aprovado e como foi feito** (areia, cratera, vila, carcaça, asfalto, estrada):
    `docs/RECEITAS_DO_LOOK.md`. Leia antes de mexer em qualquer visual. Quem faz algo que
    o Felipe aprova anota lá o caminho, os números e os arquivos.
- `captures/` (fora do git): renders, capturas e testes (a arte conceitual
  fica em `captures/concept/`).

## Como trabalhar com o Felipe (regras combinadas)

- **Commit só com aprovação explícita.** Implementar, testar você mesmo,
  reportar e esperar o "aprovado" (ou "pode commitar"). Commit = versão
  aprovada. Ele usa tags por marco (v1…v6).
- **Pesquisar antes de mudar o visual.** Toda mudança de aparência começa por
  como os jogos AAA e a literatura (GDC, SIGGRAPH, papers, código liberado,
  o próprio fonte da Unreal) resolvem; plano curto com as fontes; só então
  código. Ele interrompe quando vê constantes chutadas. Em decisões grandes,
  vale uma segunda opinião (agents/workflow) antes de fechar o plano.
- **Beleza primeiro, depois desempenho**: o alvo é o visual máximo (a 1660
  pode travar nele); presets menores servem PCs fracos. Mesmo assim, meça o
  custo e diga.
- **Não pedir confirmação a cada passo.** O plano vai junto do resultado.
  Pergunte só quando a decisão for genuinamente dele. Se ele rejeitar algo,
  desfaça sem discutir.
- **Medir antes de ajustar**, e testar quadro a quadro você mesmo. Compare
  A/B com história idêntica (`blast.ps1 render ... -Folder`, `Tools/compare.py`).
  **Veja em movimento** (vídeo), não só em fotos.
- **Jogabilidade antes do realismo**: câmera de RTS vista de cima; efeito que
  cobre unidades ou polui a leitura da batalha é defeito, mesmo sendo físico.
- **Poucas configurações pro jogador**: presets baixa/média/alta/ultra
  bastam; opções detalhadas são ferramenta de debug.
- **Respostas curtas**: ele quer saber o que muda pra ele, sem textão técnico.
- Ele testa **clicando no app** (o simulador). Antes de dizer "pronto",
  reproduza com o fluxo real. Fale em termos do que ele vê, não em nomes
  internos.
- Se você deixar o app/editor num modo de teste (pausado, câmera presa),
  restaure antes de devolver.
- **A placa de vídeo é uma só (GTX 1660), e várias sessões usam.** Antes de abrir
  qualquer Unreal com janela ou render na placa, siga a trava `captures/.gpu_lock`
  (`docs/GPU.md`): se existe e tem menos de 20 min, espere; senão crie, use e apague.
  As sessões se coordenam por ela, sem pedir ao Felipe pra liberar.
