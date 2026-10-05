# BlastLab — instruções do projeto

Projeto do Felipe: um **simulador de explosões volumétricas** nível AAA
(explosões, fumaça e interação, pensando num RTS estilo C&C). Este
repositório é o simulador e as referências; o jogo não é montado aqui.
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
- `reference/webgl/`: o simulador **WebGL** original, congelado na tag
  `v6-fumaca-interacao`. É a referência visual aprovada. Não mexer no JS.
- `docs/`: leia nesta ordem quando precisar de contexto:
  1. `docs/STATUS.md`: estado atual (o topo) e o histórico.
  2. `docs/MIGRACAO.md`: plano de migração, números calibrados, lições.
  3. `docs/ARCHITECTURE.md`: como cada sistema do WebGL foi feito, com
     referências (caminhos `js/...` = `reference/webgl/js/...`).
- `captures/` (fora do git): renders, capturas e testes.

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
