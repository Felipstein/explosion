# As fichas do elenco (uma por peça)

O pedido dele (31/10/2026): "uma pasta com arquivos textuais, tudo bem organizado, separado: o funcionamento, a descrição, a ideia,
a VIDA de cada unidade, base e construção, porque depois ainda vamos gerar sons pra cada um (andar, construir, quando seleciona, os
sons aleatórios da unidade e das bases), destruição, projétil, conceituais, imaginação, criatividade de cada um, os tiros, os
projéteis, as explosões, como ele se comporta em batalha, temperamento".

Cada peça do jogo tem uma ficha aqui. É a fonte pra quem for fazer **o som, a arte final, o efeito, a animação e o prompt do HUD**.
Os números de jogo vêm do `Game/Config/RTS/Units.json` (a verdade do jogo) e o equilíbrio e as decisões do `docs/ELENCO.md`; a
ficha conta a peça por dentro, do jeito que um diretor de arte e um designer de som precisam.

**O que já está aprovado visualmente (pra o som e o retrato):** `docs/elenco/ESTADO_VISUAL.md`, peça por peça.

## As pastas

| Pasta | O quê |
|---|---|
| `usa/` | a USA: vê tudo, acerta primeiro (clara, fria, tecnológica) |
| `china/` | a China: arrasa em massa (escura, massiva, disciplinada) |
| `gla/` | a GLA: vive da ruína (improvisada, sucata, guerrilha) |
| `russia/` | a Rússia: cava e queima, segura o chão e esmaga de longe (aço frio, pesada, fortificada) |
| `PROMPTS_HUD.md` | os prompts do botão e do cartão do HUD de cada peça nova, prontos pra colar |

O nome do arquivo é a chave do `Units.json` (`china_overlord.md`).

## O modelo de cada ficha (as seções, sempre nesta ordem)

1. **Ficha rápida:** a tabela: papel, custo, tempo, vida, blindagem, velocidade, visão, arma(s), requer, produz/constrói.
2. **A ideia:** o conceito em 2–3 frases; a palavra que se lê de cima; a inspiração real (perto do real, nunca a cópia); por que
   ela existe no jogo (o buraco que ela tapa no exército).
3. **Como funciona:** a mecânica, as armas com os números, o que ela faz bem e mal: **forte contra / fraca contra**, quem é o
   contra dela e de quem ela é o contra.
4. **Em batalha:** como se comporta, o ritmo, o temperamento (a personalidade numa palavra e no jeito de agir), como o jogador
   bom usa, como a IA usa, o que acontece quando ela fica sozinha.
5. **A vida:** o que se mexe e acende quando ela está parada, andando, atirando, ferida, sem energia; os detalhes que contam a
   história dela (fumaça, luzes, bandeira, peça que gira).
6. **O visual:** a silhueta de cima, o elemento-assinatura, as cores e a cor do jogador, o que nunca pode ter.
7. **Tiro, projétil, impacto, destruição:** a boca (o clarão, a fumaça, o recuo), o projétil em voo (rastro, velocidade, cor), o
   impacto em cada tipo de alvo (chão, tanque, prédio, soldado), a morte (como explode, a carcaça ou o entulho, o fogo que fica).
8. **O som:** o roteiro pro designer de som:
   - **seleção** (3–5 variantes aleatórias, sem repetir a mesma duas vezes seguidas) e **ordem** (andar, atacar);
   - **as falas** (em inglês, o jogo é em inglês; o tom do temperamento; 4–6 por situação: selecionado, andando, atacando, sob fogo,
     pronto);
   - **o corpo:** motor, esteira, passo, servo da torre, o zumbido do prédio, a construção (do andaime ao pronto);
   - **a arma:** o tiro, o projétil passando, o impacto, a cadência;
   - **a morte:** a explosão, os destroços, o fogo que fica;
   - o que dá a cara da facção no som (a USA: rádio limpo e servo elétrico; a China: metal pesado, alto-falante, coro; a GLA:
     motor velho, metal batendo, rádio chiado).
9. **Ideias pra depois:** criatividade que ainda não está no jogo: melhorias, habilidades, variações, interações com o mundo.
10. **O prompt do HUD:** o link pro prompt no `PROMPTS_HUD.md` (feito no fim, com o modelo provisório como anexo de forma).

## O que já está no jogo e o que é proposta

A ficha separa sempre: o que está nos dados e funciona (**no jogo**) do que é proposta (**ideia**). Nada de vender ideia como
feito: o som, a fala e o efeito descritos aqui são o pedido pra quem for fazer, não o que existe hoje.

A Rússia (01/11) está nos dados (`Units.json`, pelo `Game/Tools/elenco_units.py`, `russia()`) e nos modelos provisórios, mas só entra no
menu quando os modelos forem importados na Unreal (`TAREFAS.md` 33): nas fichas dela, **no jogo** quer dizer nos dados e no modelo, e a
linha "Estado" da ficha rápida diz isso.
