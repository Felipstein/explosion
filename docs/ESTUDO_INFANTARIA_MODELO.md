# Estudo: o modelo do soldado (01/11/2026)

**O pedido:** a infantaria provisória foi reprovada duas vezes ("tá achando que é jogo de lego? faz direito, faz detalhado,
faz bem pensado, com carinho"). Aqui: como os RTS fazem o soldado, os caminhos pra nós e um caminho escolhido. Os números de
jogo da infantaria estão em `docs/ESTUDO_INFANTARIA.md`; o alvo de forma é a folha dos 9 tipos
(`captures/concept/90_infantaria_9_tipos_folha_de_ideias/raiz_r1_gpt.png`).

## 0. O ponto de partida, medido

- **Corpo:** `SKM_Manny_Simple`, **48.705 vértices e 92.178 faces**, 2 materiais (`Game/Saved/SoldierDress/fac_usa/dress.log`).
  Mais pesado que qualquer soldado de RTS, e liso.
- **Roupa:** farda pintada na textura 1024 do corpo + 3 a 5 malhas rígidas por soldado, uma por osso (`soldier_dress.py`,
  `export_pieces`).
- **Por que parece lego:**
  1. Formas primitivas (caixa arredondada, cilindro), sem chanfro, costura nem dobra.
  2. O pano não segue o corpo: a barra do casaco em duas metades presas nas coxas abre um rasgo a cada passo.
  3. Nariz e orelha de caixa num boneco sem traço.

  Ser rígido não é o defeito: capacete, placa e tubo são rígidos em qualquer AAA. O defeito é a forma e o pano.
- **Tela:** ~50 px de altura a 67 m e ~170 px a 20 m.

## 1. Como os RTS fazem

Os estúdios não publicam orçamento de polígono. O que tem fonte:

| Jogo | O que se sabe | Roupa | Leitura de cima |
|---|---|---|---|
| **ZH** (2003) | Guia da época: ~100/400 polígonos (baixo/alto) [ppm] | Uma malha com pele (`_SKN`) no esqueleto (`_SKL`) [cnclabs] | Cor do time no modelo inteiro, proporção exagerada |
| **Dawn of War** (2004) | Cada unidade "como um personagem do Half-Life" (da ordem de mil) [bit-tech] | Malha com pele | Silhueta e cor do time |
| **StarCraft II** (2010) | < 10 mil polígonos, texturas até 1024 [hive] | Malha com pele, máscara do time | Silhueta pensada na vista 3/4 de cima, pouco detalhe de superfície, unidades um pouco maiores e exageradas [eurogamer] |
| **CoH3** (2023) | Não publicado | "Paper Doll": roupa e equipamento por peças modulares, pra variar o soldado [sixthaxis] | Realista; aceita unidade difícil de ver [creativebloq] |
| **Tempest Rising** (2025, Unreal) | Não publicado | Conceito de todos antes do modelo | O equipamento real (caixa de ferramentas, tanque de combustível) faz a silhueta; a cor marca a facção [creativebloq]. Jogadores reclamam de silhuetas parecidas [steam] |
| **WARNO, Steel Division, Broken Arrow, Men of War** | Nada público | — | Soldado realista e pequeno; quem informa é a interface (etiqueta, ícone) |

No mercado, o soldado de jogo pronto tem 2,5 a 5 mil triângulos e textura 2048 [unity]; o de vitrine, 19,8 mil e 4K [polycount].

**O padrão AAA:**
1. **Uma malha com pesos.** O pano tem pesos misturados entre os ossos. O equipamento duro fica na **mesma** malha, com 100%
   no osso dele. O corpo sob a roupa é apagado. Casaco solto ganha ossos com física, só perto da câmera. Peças separadas
   (Paper Doll) servem pra variar e são juntadas numa malha (na Unreal, o Mutable) [mutable].
2. **Dobras no normal map**, assado de uma versão alta (AAA: Marvelous + ZBrush; nós: o pano simulado do Blender) [blender-bake].
3. **1 material** por soldado (2 no máximo), máscara do time, **3 a 4 LODs**.
4. **Leitura:** silhueta pela carga, contraste contra o chão, cor do time numa faixa, proporção levemente heroica. Os volumes
   saem da folha, medidos.

## 2. O orçamento (minha conta; medir no jogo)

- **Triângulos:** LOD0 12–15 mil com a arma (20 m), LOD1 6 mil, LOD2 2,5 mil (câmera do jogo), LOD3 1 mil. A 50 px, 2,5 mil já
  desenham a silhueta; os 92 mil de hoje dão ~120 triângulos por pixel.
- **Textura:** 2048² BaseColor (time no alfa) + Normal + ORM ≈ 13 MB por tipo → **~120 MB os 9**.
- **200 soldados na 1660:**
  - **Geometria:** 200 × 2,5 mil = 0,5 milhão de triângulos com pele, pouco pra placa (hoje, se o Manny entra sem LOD, 18
    milhões).
  - **Chamadas de desenho:** com 1 malha e 1 material, ~4 passes × 200 ≈ 800; hoje (2 seções + 3 a 5 peças) ≈ 4–5 mil, menos
    o que a Unreal instancia sozinha.
  - **CPU:** sobra a animação. A saída é o Animation Budget Allocator [aba] e, longe, o Instanced Skinned Mesh da 5.8
    (experimental) [iskm].

Uma malha só deixa o soldado **mais bonito e mais barato**.

## 3. Os caminhos

### (a) Roupa costurada por script no Blender, pesada no esqueleto do Manny

A farda e o casacão nascem como **casca do corpo**: a região escolhida pelos pesos, afastada pela folga, refeita em
quadriláteros. O que cai solto passa pelo **pano simulado do Blender na CPU** [blender-cloth]. O equipamento é modelado com
chanfro. Uma versão alta é assada em normal map e AO. Os pesos vêm do corpo (Data Transfer) [blender-dt]. Sai um FBX por tipo,
**no mesmo esqueleto**: as animações tocam sem retarget.

- **Qualidade:** a de um bom RTS (CoH/Tempest a 67 m). A 20 m, o limite é o rosto.
- **Esforço:** base do script ~4–6 dias, depois ~1 dia por tipo (estimativa).
- **Risco:** médio (afinar o pano por script; deformação de ombro e virilha).
- **Baixar:** nada obrigatório.

### (b) MetaHuman (UE 5.8) + roupa

Corpo e rosto de cinema, grátis abaixo de US$ 1 milhão/ano [mh-license]. **Não há farda militar:** a roupa é o mesmo trabalho
do (a), em Chaos Outfit [mh-outfit]. Na câmera do jogo serviriam o corpo LOD3 (1,5 mil vértices) e a cabeça LOD5–7 [mh-lod].
São vários componentes por personagem mais o AnimBP do rosto. A multidão da 5.8 é experimental [mh-crowd], e o esqueleto pede
retarget do Manny [mh-skel].

- **Qualidade:** a mais alta de perto; a 67 m o rosto tem 6 px.
- **Esforço:** alto.
- **Risco:** médio-alto.
- **Baixar:** os dados vêm da nuvem da Epic; o editor roda na placa.

### (c) Modelos gratuitos

- **Quaternius** (CC0): estilizado, proporção de desenho [quaternius].
- **Kenney** (CC0): blocado, literalmente o lego.
- **Poly Haven / ambientCG** (CC0): sem personagens; **servem as texturas de pano e couro**.
- **Mixamo:** grátis e sem royalties em jogo, sem redistribuir o arquivo cru [mixamo]; personagens da época do Fuse, esqueleto
  próprio, conta Adobe.
- **Sketchfab / Fab grátis:** estilo varia por autor; ninguém tem o casacão chinês, a túnica da GLA e o 6B47 juntos.

**Veredito:** serve de apoio (texturas), não de soldado.

### (d) 3D gerado por IA, local

| Modelo | Placa pedida | Na 1660 (6 GB) |
|---|---|---|
| Hunyuan3D-2 mini/2mv | 6 GB forma; 16 GB forma + textura [hy] | **Só a forma**, no limite |
| Hunyuan3D-2GP (descarga na RAM) | forma < 6 GB, ~24 GB de RAM [hy-gp] | Só a forma, devagar |
| Hunyuan3D-2.1 | 10 GB forma, 29 GB total [hy] | Não |
| TRELLIS / TRELLIS.2 | ≥ 16 GB / ~24 GB [trellis] | Não |
| Stable Fast 3D / TripoSG | 7 GB / > 8 GB [sf3d] | Não |

A 1660 não tem tensor cores nem bf16: é lento e não testado. A saída é uma malha densa, com o fuzil colado na mão, sem UV, sem
textura e sem pesos. Precisaria de uma folha em pose A por tipo (Atelier) e de todo o trabalho do (a) por cima. A licença do
Hunyuan exclui a UE, o Reino Unido e a Coreia; o Brasil pode [hy-lic]. Seriam alguns GB de pesos + PyTorch.

**Veredito:** experimento futuro, como versão alta pra assar.

### (e) Outros

- **MPFB2 / MakeHuman** (extensão do Blender, 43,8 MB, código GPL-3, assets CC0) [mpfb]: cabeça e corpo realistas com
  topologia de animação; roupas CC0 genéricas [mh-assets]. **Bom pra cabeça, mãos e orelhas.**
- **Mutable** (vem na Unreal): variar o esquadrão depois [mutable].
- **Robust Weight Transfer** (SIGGRAPH Asia 2023, MIT): pesos do corpo pro pano solto; pede o `libigl` [rwt].

## 4. A recomendação: o caminho (a)

É o único que entrega os 9 tipos num estilo só, no esqueleto e nas animações de hoje, mais barato que o atual, sem comprar
nada e controlado por script (A/B, refazer, ajustar número). O (b) e o (d) gastam onde não se vê; o (c) não tem os nossos tipos.

**Passos:**
1. **Medir a folha:** por tipo, cabeça, ombro, casaco, colete e carga em proporção da altura. Vira a tabela do script.
2. **Corpo de baixo:** o Manny reduzido a ~10 mil faces, sem o que a roupa cobre.
3. **Pano:** casca + folga + quadriláteros. Casacão, túnica, calça larga, turbante e lenço passam pelo pano simulado
   (encolhimento pra dobra). Costura, bainha, bolso e gola em geometria.
4. **Equipamento:** capacetes (ACH, gorro de pele, 6B47), colete, bolsas, Javelin, RPG, foguetes, tripé, pá, saco. Chanfro e
   painel, encostado no pano; rígido no osso, na mesma malha.
5. **Alta → baixa:** normal map e AO assados (Cycles na CPU).
6. **Pesos:** Data Transfer + suavização. Abaixo da virilha, mistura coxa esquerda, coxa direita e pélvis pela distância da
   linha do meio.
7. **Textura 2048:** camuflagem em 3D (OCP, Type 07, EMR; já existe no script), sujeira por AO e curvatura, máscara do time,
   um material mestre.
8. **Exportar:** um skeletal mesh por tipo; LODs pela redução da Unreal (100/40/18/7%).
9. **Deformação em vídeo:** andar, correr, agachar, mirar e morrer, caçando rasgo e pano atravessado.
10. **Prova no jogo:** a 67 m e a 20 m, ao lado da folha, A/B com o atual.

**Ordem:** primeiro o fuzileiro USA (testa a linha inteira). Depois o chinês (o casacão: testa o pano solto). Só então os outros 7.

**Permissão dele (nada obrigatório):**

| O quê | De onde | Licença | Tamanho | Pra quê |
|---|---|---|---|---|
| MPFB2 + "MakeHuman system assets" | extensions.blender.org, static.makehumancommunity.org | GPL-3 (código), CC0 (assets) | 43,8 MB + pacote (não informado) | cabeça, mãos e orelhas |
| `libigl` (pip, no Python do Blender) | PyPI | C++ em MPL-2.0; os bindings aparecem como GPL-3 (conferir; é ferramenta, não vai no jogo) | ~13–20 MB | pesos do pano solto |
| 3 a 5 texturas 2K de pano, lona e couro | polyhaven.com, ambientcg.com | CC0 | ~10–30 MB | detalhe do tecido |

Nada disso usa a placa (Blender sem janela, Cycles na CPU). Só a prova no jogo passa pela trava (`docs/GPU.md`).

## Fontes

- [ppm] https://ppmforums.com/topic-31519/generals-zero-hour-questions
- [cnclabs] https://www.cnclabs.com/modding/generals/infantry-modeling-tutorial/page2.aspx
- [bit-tech] https://bit-tech.net/reviews/gaming/pc/warhammer_40000_dawn_of_war_developer_qanda/1/
- [hive] https://www.hiveworkshop.com/threads/seeking-advise-sc2-modeling.208759 e https://wiki.hiveworkshop.com/index.php/How_to_make_StarCraft_II_Assets
- [eurogamer] https://www.blizzplanet.com/blog/comments/eurogamer_interviews_samwise_didier
- [sixthaxis] https://www.thesixthaxis.com/2022/04/22/company-of-heroes-3-dev-diary-trailer-art-and-authenticity/
- [creativebloq] https://www.creativebloq.com/3d/video-game-design/how-tempest-rising-uses-unreal-engine-5-to-channel-the-heart-of-90s-strategy-games-without-losing-its-soul
- [steam] https://steamcommunity.com/app/1486920/discussions/0/3911997462235606909
- [unity] https://marketplace.unity.com/packages/3d/characters/humanoids/humans/ww2-german-infantry-soldier-83826
- [polycount] https://polycount.com/discussion/comment/2649601/
- [mutable] https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-layouts
- [aba] https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-budget-allocator-in-unreal-engine
- [iskm] https://forums.unrealengine.com/t/animbank-and-instanced-skinned-mesh-component/2734825
- [blender-cloth] https://docs.blender.org/manual/en/latest/physics/cloth/index.html
- [blender-dt] https://docs.blender.org/manual/en/latest/modeling/modifiers/edit/data_transfer.html
- [blender-bake] https://docs.blender.org/manual/en/latest/render/cycles/baking.html
- [mh-license] https://www.cgchannel.com/2025/06/you-can-now-sell-metahumans-or-use-them-in-unity-or-godot/
- [mh-outfit] https://dev.epicgames.com/documentation/metahuman/asset-format-and-structure-requirements-for-metahumans-on-fab?application_version=5.6
- [mh-lod] https://dev.epicgames.com/documentation/metahuman/platform-support-and-lod-specifications-for-metahumans
- [mh-crowd] https://gamesbeat.com/epic-games-launches-metahuman-5-8-to-create-real-time-game-character-crowds/ e https://dev.epicgames.com/documentation/metahuman/metahuman-crowds-in-unreal-engine
- [mh-skel] https://mocaponline.com/blogs/mocap-news/ue5-animation-retargeting
- [quaternius] https://quaternius.com/packs/universalbasecharacters.html
- [mixamo] https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html
- [hy] https://github.com/Tencent-Hunyuan/Hunyuan3D-2
- [hy-gp] https://pypi.org/project/mmgp/ e https://codersera.com/blog/set-up-hunyuan3d-2-on-windows-a-step-by-step-guide/
- [hy-lic] https://huggingface.co/tencent/Hunyuan3D-2/blob/main/LICENSE
- [trellis] https://docs.clore.ai/guides/3d-generation/trellis-3d e https://comfyui-wiki.com/en/models/trellis/trellis-2
- [sf3d] https://huggingface.co/stabilityai/stable-fast-3d e https://www.promptlayer.com/models/triposg
- [mpfb] https://extensions.blender.org/add-ons/mpfb/ e https://www.cgchannel.com/2025/03/check-out-open-source-blender-character-generation-plugin-mpfb-2
- [mh-assets] https://static.makehumancommunity.org/assets/assetpacks.html
- [rwt] https://github.com/rin-23/RobustSkinWeightsTransferCode e https://pypi.org/project/libigl/
