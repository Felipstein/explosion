# Som das explosões: especificação (07/10/2026)

**Adiado** (Felipe, 07/10: "o foco é visual agora"). Fica pronto pra quando voltarmos.

Quem faz o quê: a **sessão de arte** escreve esta especificação, escolhe os
arquivos e mede o resultado. A **sessão das explosões** faz o gancho no evento e
a reprodução (MetaSound/SoundCue). Base: `captures/concept/34_explosion_study/05_arrepio_experiencia.md`.

## 1. O princípio: contraste e ordem

O arrepio vem do **contraste** e da **ordem**, não do volume.

**O contraste:** um leito calmo, depois o estalo, o soco grave e o ronco
rolando. Medi o áudio dos três vídeos do Veo: o estrondo sobe só ~7 dB acima
da batalha, sem silêncio antes nem pancada. É uma parede de barulho, então
**no som o Veo não é alvo**. O nosso tem que ser melhor que ele.

**A ordem:** o clarão sai em t = 0. O som, o tremor e a reação do mundo
chegam **juntos** em t = d/343 s, com d = distância da explosão ao microfone.

**As regras:**
- **Microfone no ponto de mira**, ~12 m acima do chão (regra do Zero Hour:
  50 unidades acima do terreno no ponto de mira). Por isso a explosão no
  centro da tela soa quase junto com o clarão, e uma na borda (~110 m) chega
  ~0,3 s depois.
- **Mixagem com hierarquia** (o "HDR" da DICE, versão barata): o evento mais
  alto abaixa o resto de −4 a −8 dB por 1–2 s.
- **Vozes e o anunciador nunca são cortados.** Eles ficam fora dessa regra.
- **Distância:** perto, meio e longe em crossfade. O ar come os agudos
  (~117 dB/km em 8 kHz), então explosão longe é ronco grave.
- **Repetição:** 6–9 variações por som, ±5% de tom, e um limite de cópias
  simultâneas por som (impacto pesado: 3).
- **Névoa de guerra** (decisão do Felipe, 07/10): a batalha comum escondida
  fica muda. Superarma e nuke soam no mapa inteiro, abafadas e sem a
  posição exata (no máximo a direção no estéreo).

## 2. Camadas de uma explosão

| Camada | Tempo após a chegada | Conteúdo |
|---|---|---|
| Estalo | 0–30 ms | transiente largo, metálico no tanque |
| Corpo | 50–400 ms | grave de 30 a 150 Hz, saturado (o grave aparece até em caixinha de som) |
| Destroços | 0,3–3 s | chapas e pedras caindo, cada som no lugar onde o pedaço cai |
| Cauda | 1–6 s | ronco do ambiente: no deserto rola longe, na cidade ecoa, na neve é seco |

## 3. Os sons da cena herói (tanque morre na câmera baixa)

Nomes de asset em en-US (`Content/Audio/...`, gerados por script).

| Instante | Som | Camadas e notas |
|---|---|---|
| sempre | `SFX_Amb_DesertBed` | vento, zumbido distante, batalha longe filtrada; o leito calmo que dá o contraste |
| −0,4 s | `SFX_TankCannon_Fire` | estampido + baque grave do tanque que atira (no lugar dele) |
| 0 | `SFX_Impact_ArmorPen` | estalo metálico curto do projétil furando a blindagem |
| d/343 | `SFX_Explosion_TankAmmo` | estalo + corpo + cauda do deserto; dispara o abaixar do resto (−6 dB, 1,5 s) |
| 0,5–1,5 s | `SFX_CookOff_Pop` | 2–3 sub-explosões sorteadas, mais fracas, cada uma com mini-tranco |
| 0,3–3 s | `SFX_Debris_Metal` | chapas tilintando e batendo no chão, posicionadas |
| ~3,5 s | `SFX_Debris_TurretLand` | baque pesado da torre caindo + areia |
| 3 s em diante | `SFX_Fire_WreckLoop` | crepitar contínuo da carcaça, com pipocos a cada 2–8 s |
| ~1 s | `VO_Radio_TargetDestroyed` | "Alvo destruído!" com filtro de rádio (300–3400 Hz); vozes ficam pra depois, entra um provisório |

## 4. De onde vêm os arquivos

- **Primeira opção: os pacotes grátis da Sonniss pra GDC** (2015–2026). São
  sons profissionais, sem royalties, uso comercial liberado e sem atribuição;
  o de 2026 tem ~7,5 GB ([rekkerd.org](https://rekkerd.org/sonniss-releases-gdc-2026-game-audio-bundle/),
  [gdc.sonniss.com](https://gdc.sonniss.com/)). Proibido usar pra treinar IA.
- **Freesound:** só arquivos CC0.
- **Antes de baixar:** a lista dos arquivos exatos, com fonte e tamanho, vai
  ao Felipe pra ele dar o OK. Nada é baixado sem isso.

## 5. Aceite (medido no vídeo da cena herói)

- O leito antes da explosão fica pelo menos **20 dB abaixo** do pico do
  estrondo. No Veo a diferença é ~7 dB.
- O estalo começa até 30 ms depois do instante de chegada; o corpo tem o pico
  dele entre 50 e 400 ms.
- O resto da mixagem abaixa visivelmente na forma de onda por 1–2 s, e as
  vozes não abaixam.
- A cauda fica audível por pelo menos 3 s.
- Duas explosões iguais seguidas não soam idênticas (variação).
