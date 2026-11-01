# Atelier: gerar imagem no GPT e no Gemini sem copiar e colar

Pedido do Felipe (01/11/2026): hoje cada imagem é copiar o prompt, anexar, esperar, baixar e mandar pra
sessão, uma por vez, quase sempre no GPT e no Gemini. Ele quer um app nativo leve e rápido:

- vários pedidos ao mesmo tempo, com o status de cada um à vista;
- ramos no mesmo pedido pra outras visões (vários Gemini, um GPT e um Gemini, etc.);
- o Claude dentro, pra remontar o prompt e regerar ali mesmo;
- as sessões pedindo imagem sozinhas (prompt + anexos) e recebendo o resultado.

**Mockup (fase 1, só a tela):** `captures/atelier/mockup/index.html`, publicado como artifact. Roda
com dados de exemplo e as imagens reais (disparo do tanque r1 a r4, Technical, ícones do HUD,
War Factory). A geração é simulada.

## Estudo curto: de onde veio cada decisão

| Referência | O que eles resolvem | Como entrou no Atelier |
|---|---|---|
| Midjourney (web, Create) | a fila mora na própria lista; ação na imagem já vira pedido novo (Vary, Remix, usar como referência) | cada quadro tem escolher, reprovar, ramificar e anexar; nova rodada num clique |
| Krea Nodes, Flora, Weavy, Freepik Spaces | ramificar um prompt em vários modelos num canvas de nós; com volume o canvas vira bagunça | ramos em faixas fixas: visão = prompt próprio, modelos dentro; rodada nova grande, histórico pequeno ao lado |
| Lightroom, Photo Mechanic (triagem) | escolher centenas de fotos só no teclado | P escolhe, X reprova, setas andam, Enter amplia, C compara |
| LMArena (imagens) | lado a lado, escolher rápido | Comparar sempre com o alvo na posição 0; modo Deslizar A/B |
| Linear, Raycast | Ctrl K pra tudo, nada espera o mouse | Ctrl K, N, B, /, Ctrl Enter |
| GitHub Actions (matriz) | um trabalho × N variações, status por célula | o lote: um prompt × N itens × modelos |

Regras da casa que viraram tela:

- **Ancorar na raiz:** o anexo raiz tem a âncora; anexo tirado de uma rodada aparece tracejado
  (encadear deriva).
- **Aprovou, registrou:** Aprovar copia pra `docs/aprovados/<área>/`, anota no `APROVADOS.md` com a
  frase dele e devolve pra sessão dona.
- **Prompt completo:** cada ramo guarda o prompt inteiro; a diferença pra raiz aparece como + e −.
- **UI sem legenda:** status pela forma (fila tracejada, gerando com relógio e barra na cor do
  modelo, GPT revelando a prévia, falha em vermelho com "Tentar de novo", escolhida em dourado).

## Como fica o app de verdade (proposta)

- **Tauri 2** (WebView2 do Windows): uns 10 MB, abre em menos de 1 s, e a tela do mockup vira a
  tela do app. Electron faria o mesmo com 150 MB e mais memória.
- **Núcleo local com a fila:** paralelo por provedor, tenta de novo sozinho, grava em
  `captures/concept/<pasta>/<ramo>_r<N>_<modelo>.webp` e um `manifest.json` por pedido (prompt,
  anexos, modelo, custo, tempo), pra refazer qualquer rodada igual.
- **Claude dentro:** `claude -p` (o Claude Code sem janela, na assinatura dele) monta e reescreve o
  prompt, compara com o alvo e propõe a próxima rodada como diferença.
- **Sessões:** um servidor MCP `atelier` no `.mcp.json` do projeto. Toda sessão ganha ferramentas pra
  pedir (prompt, anexos, modelos, pasta), ver o status e esperar o resultado ou a escolha dele.
  Limite de gasto por dia: acima dele, pedido de sessão espera o ok.
- **GPT:** a API de imagem da OpenAI aceita várias imagens de referência e manda prévias parciais
  durante a geração (por isso o quadro do GPT "revela" no mockup). **Gemini:** aceita várias
  imagens, sem prévia.

## Custo por imagem (nov/2026, fontes de terceiros; conferir na página oficial antes de fechar)

- GPT Image 2: cobrado por token, perto de US$ 0,19 numa imagem alta 1536×1024 (estimativa).
- Gemini 3 Pro Image (Nano Banana Pro): US$ 0,134 até 2K; 0,24 em 4K.
- Gemini 3.1 Flash Image (Nano Banana 2): US$ 0,067 em 1K.
- O ChatGPT pago não cobre a API. Pro GPT existe o Codex CLI logado na conta do ChatGPT (anotado em
  05/10, falta conferir se gera imagem hoje). O Gemini pela API é pago desde a primeira imagem.

Fontes: [preços de imagem 2026, IntuitionLabs](https://intuitionlabs.ai/articles/ai-image-generation-pricing-google-openai),
[Nano Banana API pricing](https://www.myarchitectai.com/blog/nano-banana-api-pricing),
[gpt-image-2, OpenRouter](https://openrouter.ai/openai/gpt-image-2),
[preço normalizado por imagem, ago/2026](https://invideo.io/blog/ai-image-model-pricing/).

## Estado em 01/11 (fase 2: funciona de verdade)

- **Abrir:** `Atelier.exe` na área de trabalho, o app nativo (Tauri 2, `Atelier/app/src-tauri/`):
  liga o serviço (`Atelier/core/atelier.mjs`, porta 8140) se ele não estiver rodando e abre a tela
  servida por ele (`Atelier/ui/`). Mudar a tela não pede recompilar o .exe; só o `main.rs` pede
  (`npx @tauri-apps/cli@2 build --no-bundle` em `Atelier/app`, ~10 MB). O soltar arquivo fica com a
  página (o tratador de arrastar do Tauri está desligado) e link de janela nova vai pro navegador.
  `Atelier/atelier.ps1` ainda abre pelo Edge em modo app, como reserva.
- **Ícone:** o tanque do ícone do jogo em duotone dourado (a cor do app) numa placa escura, com
  marcas de enquadramento de visor nos cantos; 48 px pra baixo usa uma versão com o tanque maior e os
  cantos mais grossos. Gerado por `Atelier/app/icon/make_icon.py` a partir de `game_icon.webp`.
- **Tela cheia e copiar:** clicar num resultado ou num anexo abre em tela cheia; **Copiar** (ou
  Ctrl C) põe a imagem na área de transferência como PNG.
- **GPT pela conta do ChatGPT, sem chave:** o Codex CLI (logado na conta dele) roda `codex exec
  --json -i <anexos>` com o prompt pela entrada; a imagem cai em
  `~/.codex/generated_images/<thread_id>/` e vai pra pasta do pedido. Medido: 44 e 53 s por imagem,
  1672×941 (o mesmo tamanho do ChatGPT), 2 em paralelo. Não custa por imagem; gasta o limite do
  plano.
- **Configurações** (botão de controles no topo, ou clicar num modelo "sem chave"): o GPT sai pela
  conta do ChatGPT (Codex) ou pela chave da API, à escolha dele; as chaves da OpenAI e do Gemini se
  colam ali, com **Testar** (lista os modelos com a chave, sem gerar imagem) e Apagar. Ficam em
  `%APPDATA%\Atelier\settings.json`, fora do repositório (`core/settings.mjs`); a tela só recebe os
  4 últimos caracteres. A variável de ambiente vale se não houver chave salva.
- **Pasta do pedido:** um seletor, não texto: a lista das pastas de `captures/concept` (a mais
  nova primeiro, com miniatura, quantidade e data, com busca), "Nova pasta" com o próximo número no
  topo, e **Procurar…** abre o seletor de pasta do Windows (`core/pick-folder.ps1`).
- **Arrastar arquivos:** com o pedido novo aberto, solta em qualquer lugar da janela e vira anexo;
  sem janela aberta, soltar na mesa anexa ao pedido aberto; fora disso não faz nada (antes o
  navegador abria a imagem no lugar do app).
- **O Claude abre fechado** (pedido dele); abre pelo botão Claude, pela tecla / ou quando ele pede
  "Claude propõe" num ramo novo.
- **Gemini sem chave não existe** (pesquisa de 01/11): o Gemini CLI gera imagem só pela extensão
  nanobanana, que também pede chave; os atalhos por login do Google são proxies não oficiais (contra
  os termos). A chave do AI Studio sai sem cartão; o Pro não tem cota grátis na API.
- **Claude dentro:** `claude -p` na assinatura dele, com o pedido, os prompts e os caminhos das
  imagens (ele abre pelo Read). Responde em ~35 s com a crítica e o prompt inteiro da próxima
  rodada, que vira r2 num clique.
- **Sessões:** `.mcp.json` registra o servidor `atelier` (`Atelier/core/mcp.mjs`, MCP à mão, sem
  dependência). Ferramentas: `atelier_generate`, `atelier_wait`, `atelier_status`,
  `atelier_round`, `atelier_branch`, `atelier_list`. A ponte liga o serviço se ele estiver
  parado. Sessão nova pede pra aprovar o servidor uma vez.
- **Dados:** cada pedido em `captures/atelier/requests/<id>.json` (e uma cópia `atelier.json` na
  pasta do pedido); anexos copiados em `<pasta>/_anexos/` na ordem; saída
  `<ramo>_r<N>_<modelo>.png`. O que estava gerando volta pra fila se o PC reiniciar.
- **Aprovar:** copia as escolhidas pra `docs/aprovados/<área>/`; a sessão dona recebe pelo
  `atelier_wait(until="pick")` e anota no `APROVADOS.md` com a receita.

## Pendente

- Ele: a chave do Gemini, se quiser as duas visões automáticas (~US$ 0,07 a 0,13 por imagem).
- Depois do ok da tela: empacotar no Tauri e o "Claude monta o prompt" no pedido novo.
