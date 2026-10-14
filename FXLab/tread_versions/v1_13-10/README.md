# Poeira das lagartas, versão 1 (13/10/2026), guardada

O Felipe pediu pra guardar as duas como estão ("não ficaram tão ruins") antes do estudo novo mudar o efeito.

- `tread_ember.json`: a versão EmberGen (cozimento `tread_puff_v3`, o toro do `captures/embergen/tread/tread_puff_v1.ember`).
- `tread_handmade.json`: o plano B, feito à mão (cozimento `dust_impact4`, Dust Impact da JangaFX).
- `FXTread.h`, `FXTread.cpp`, `fxtread.js`: o código que desenhava as duas nesse dia (já com a leitura do asfalto
  pelas camadas de edição do terreno).
- Os mesmos ajustes também estão em `FXLab/tread_v1_ember.json` e `FXLab/tread_v1_handmade.json`, pra rodar direto:
  `fxlab.ps1 shot ... -Extra "-Fx=tread ... -TreadProfile=tread_v1_ember.json"`.
  O código novo entra como opção no perfil: com esses dois arquivos, a poeira sai como nesse dia.
- Vídeos: `captures/fxlab/tread_gamemap_lado_a_lado.mp4` (EmberGen à esquerda, plano B à direita),
  `tread_gamemap_eg.mp4`, `tread_gamemap_hand.mp4`; a folha é `captures/fxlab/tread_gamemap_compare.jpg`.
  Esses vídeos saíram antes do conserto do asfalto: neles a areia ainda levanta em cima da rua.
