# Explosão volumétrica AAA — instruções do projeto

Projeto do Felipe: o visual de explosões, fumaça e interação para um RTS no
estilo C&C (só o visual, sem lógica de jogo). Converse em **português**.

> **Primeira vez no Windows?** Se o Felipe pedir pra restaurar a sessão do
> Mac, siga `RESTAURAR-SESSAO.md`: ele traz de volta a conversa inteira e as
> memórias do projeto.

## Onde o projeto está

- Até 03/10/2026 foi feito em **WebGL2 puro** (esta pasta). A tag
  `v6-fumaca-interacao` é a **referência final** do WebGL.
- **Decidido migrar pra Unreal Engine** (Windows 11). Leia primeiro, nesta
  ordem:
  1. `MIGRACAO.md`: plano de migração, sistema por sistema, números
     calibrados, lições e problemas em aberto.
  2. `STATUS.md`: estado atual (o topo tem a decisão) e o histórico.
  3. `ARCHITECTURE.md`: como cada sistema foi feito, com referências.
- Rodar a referência WebGL: `python devserver.py 8129` na raiz e abrir
  http://localhost:8129 (no Mac era `python3`). O `devserver.py` também grava
  capturas em `captures/` via POST /save.

## Como trabalhar com o Felipe (regras combinadas)

- **Commit só com aprovação explícita.** Implementar, testar você mesmo,
  reportar e esperar o "aprovado" (ou "pode commitar"). Commit = versão
  aprovada. Ele já usou tags por marco (v1…v6).
- **Pesquisar antes de mudar o visual.** Toda mudança de aparência começa por
  como os jogos AAA e a literatura (GDC, SIGGRAPH, papers, código liberado)
  resolvem; plano curto com as fontes; só então código. Ele interrompe quando
  vê constantes chutadas.
- **Não pedir confirmação a cada passo.** O plano vai junto do resultado.
  Pergunte só quando a decisão for genuinamente dele. Se ele rejeitar algo,
  desfaça sem discutir.
- **Medir antes de ajustar**, e testar quadro a quadro você mesmo. Compare
  A/B com história idêntica. **Veja em movimento** (vídeo), não só em fotos:
  foto escondeu a fumaça "dura".
- **Jogabilidade antes do realismo**: câmera de RTS vista de cima; efeito que
  cobre unidades ou polui a leitura da batalha é defeito, mesmo sendo físico.
- **Poucas configurações pro jogador**: presets baixa/média/alta/ultra
  bastam; opções detalhadas são ferramenta de debug.
- Ele testa **clicando no app**, não pelo console. Antes de dizer "pronto",
  reproduza com o fluxo real. Fale com ele em termos do que ele vê (nomes da
  UI, "a fumaça que fica"), não em nomes internos.
- Se você deixar o app/editor num modo de teste (pausado, câmera presa),
  restaure antes de devolver.
