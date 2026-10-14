# Estudo de impacto na areia — 08/10/2026

## Alvo

Referência principal: `captures/concept/35_explosion_arts/4.1_sand_impact_gemini.webp`, escolhida para iniciar o conjunto enviado pelo usuário. A escolha ainda não representa aprovação de um resultado.

O usuário quer efeito animado, textura do chão, iluminação e ambientação. Modelos de veículos ficam fora deste estudo.

## Critérios visuais

- Clarão compacto na origem, sem dominar a duração do impacto.
- Jato de terra escura inclinado, com filamentos e torrões individualmente legíveis.
- Poeira ocre baixa, larga e irregular, abrindo em torno do impacto; evitar fumaça preta de combustível.
- Torrões sobem, perdem velocidade e caem; poeira perde definição e se dispersa.
- Solo de areia com ondulações, pequenos fragmentos e cratera de borda irregular.
- Luz quente lateral e sombras compridas; manter detalhe nas áreas iluminadas.
- Comparar na câmera elevada de RTS, em movimento e em quadros equivalentes.

Uma imagem fixa define aparência, mas não determina o tempo do efeito. O timing será uma hipótese de trabalho a validar em vídeo.

## Integração existente

`docs/FXLAB.md` documenta um impacto anterior baseado nesta mesma arte, com `FXLab/impact.json`, `FXLab/web/fximpact.js` e `FXImpact.cpp`. Usar essa implementação como comparação de partida; não sobrescrever seus ajustes ao preparar o estudo EmberGen.

## Estado real

EmberGen 1.2.12 acessível pela interface. Preset de fábrica `artillery_explosion_1.ember` aberto para inspeção, sem ajustes artísticos realizados. Não é um resultado criado para a referência.

Gerenciador de licença: EmberGen desativado, "No license or trial found", Trial "Not started". Solicitada escolha do usuário antes de iniciar o teste e consumir sua janela de exportação. Nenhum projeto personalizado foi salvo ou exportado ainda.

## Fontes

- https://docs.jangafx.com/embergen/pages/references/node_list.html
- https://docs.jangafx.com/embergen/pages/references/ui_reference.html
- https://jangafx.com/software/embergen/try
