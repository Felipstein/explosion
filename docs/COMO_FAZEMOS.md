# Como fazemos as coisas bem feitas (03/11/2026)

O pedido do Felipe (03/11): "anota no lugar o jeito que você faz TUDO até agora, modelagem, melhorias, tudo, pra ninguém nunca
esquecer como fazer as coisas tão bem feitas, porque você parou de fazer coisas que pareciam de um jogo lego e realmente fez coisas
boas ultimamente". Este é o caminho que deu certo, do conceito ao jogo, com o que deu errado pelo caminho. Quem for criar unidade,
prédio, peça de mapa ou melhoria visual lê isto antes; os guias de cada área (abaixo) têm o detalhe.

## 1. O que o Felipe aprova (a régua)

- **Uma palavra por peça.** "Qual objeto é este prédio?" tem de ter resposta de uma palavra antes de qualquer detalhe: o radar, o
  reator, o poço, o alçapão, a torre vermelha. Prédio com "muitas coisas" e nenhuma assinatura é reprovado (`docs/CONCEITO_CONSTRUCOES.md` §3b).
- **Icônico como o C&C ZH, mas original.** "Algo que todo mundo lembra, todo mundo reconhece [...] mas não pode ser igual ao ZH, porque
  vai ser cópia, tem que pensar." E (03/11): "claro que não é pra copiar, é pra refazer melhor, e original".
  - Partir de um ícone conhecido do gênero funciona: o QG da USA saiu do QG do ZH mudado; o Shtab v3 saiu do espírito do pátio
    soviético do Red Alert com uma assinatura nossa, a cúpula de cebola do Kremlin virando radome.
  - Do ícone fica o ESPÍRITO; a forma é nossa.
- **Da mesma família.** Cada facção tem a sua língua e tudo dela fala essa língua (`docs/CONCEITO_CONSTRUCOES.md` §2; a Rússia: concreto
  cinza, telhado verde, mastro listrado, radome, as faixas azuis). O que "parece de outro jogo" é reprovado na hora: escuro, sujo,
  ficção científica, ou fora da luz e do material dos game shots.
- **Autêntico.** O objeto existe no mundo de hoje (o poço do contrabando, o T-55 enterrado, o radar). Reprovado o que inventa coisa de
  fora do jogo: o ônibus no quartel, a caixa d'água de ferro, a estátua do herói.
- **Nada de lego.** Nada de caixas com quinas vivas, superfícies lisas e sem textura. O chanfro, o desgaste, a junta, o rebite, a lona com
  dobra de verdade: o detalhe AAA (`docs/ESTUDO_DETALHE_AAA.md`), sem poluir a leitura de cima.
- **Jogabilidade antes do realismo.** A câmera é alta. O que é alto não tapa unidade. O estoque, o dinheiro e a vida têm de ser lidos
  de cima: o telhado que esconde as caixas do Supply é defeito.
- **O tanque domina a escala.** O apoio tem ~60% do tanque; os prédios têm ~3 comprimentos de tanque; o QG é o maior.
- **A base acompanha a forma.** Nada de laje quadrada sobrando embaixo de um prédio redondo.
- **Sem areia pintada na textura** (02/11): a poeira vem do mundo, não do material.

## 2. Do zero ao conceito aprovado

1. **Estudo curto antes** (`feedback-estudar-rts-antes`):
   - o ZH pelos arquivos (`docs/ECONOMIA.md`, os INI);
   - os outros RTS (Red Alert, Rise of the Reds, StarCraft, Warcraft, Age);
   - o real (a foto, o lugar, o veículo).
   Uma página no `docs/elenco/` com o que se reprovou antes (`docs/APROVADOS.md`) e a ideia em uma frase.
2. **A folha de ideias no Atelier** (`atelier_generate`; o GPT pela conta, o Gemini pela API quando tem cota):
   - 4 desenhos numa imagem, cada um com a assinatura que EU defino no prompt (não deixar o gerador inventar);
   - os anexos: o game shot de estilo (`docs/aprovados/alvos/r1_raid_slabs_gemini.webp`, a raiz), a família aprovada da facção
     (foto do jogo) e o tanque da facção pra escala;
   - nunca encadear referências (2 pelo 1, 3 pelo 2);
   - "items" com nome curto (o nome longo dá ENOENT); a descrição vai no prompt;
   - 1 imagem primeiro: a cota é curta.
3. **Mostrar tudo, com a minha crítica de diretor de arte**, sem passar pano. Quando ele escolhe (ou deixa comigo), a imagem vai pra
   `docs/concept/buildings/` e a linha pro `docs/APROVADOS.md` no mesmo turno.
4. **Duas voltas sem acertar: outra abordagem.** Nunca ficar girando a mesma ideia. No Shtab: v1 prisão, v2 casa civil, 4 ícones "de
   outro jogo". A 4ª volta perguntou o rumo (o Rise of the Reds e o Red Alert) e saiu a original no espírito do Red Alert.

## 3. Do conceito ao modelo (Blender por script)

- **Um script por peça:** `Game/Tools/model_<chave>.py` com o cabeçalho explicando a palavra, a inspiração real, as medidas e as peças.
  - Os exemplos bons mais recentes: `model_gla_supply_stash_v5.py`, `model_gla_bunker.py`,
    `model_china_propaganda_center_v5.py` (a Torre Vermelha), `model_usa_command_center_v5.py`.
- **Convenção:** metros, Unreal X frente, Y direita, Z cima, origem no chão no centro do lote (o `model_lib` vira pro Blender).
- **As peças:**
  - `SM_<Nome>_Base`, com tudo que é fixo;
  - `_Scaffold`, o andaime da obra;
  - uma malha por peça que se mexe, com o pivô na origem dela: `_Turret`, `_Radar`, `_Wheel`, a esteira `wheel_*`;
  - as bandeiras pelo `flag(...)`: o `model_run_detail` tira o pano e grava o mastro no `flags.json`.
- **Os materiais por slot:** o nome do material é a família ("Plaster", "Crate", "Player", "Concrete", "Roof"...), e o
  `import_provisional.py` liga cada slot à instância da facção (os mapas `*_SLOTS`). Material novo = slot novo no `SLOTS`.
- **A cor do jogador** só onde a facção decide: as faixas, a lona e as bandeiras. A faixa da Torre Vermelha é sempre vermelha; a
  cor do time fica nas colunas.
- **O detalhe AAA, sempre pelo executor:**
  `blender -b --factory-startup --python Game/Tools/model_run_detail.py -- <vehicle|building> Game/Tools/model_<chave>.py captures/game/models/<chave>`.
  Ele faz:
  - a grade (14 cm nos veículos, 40 cm nos prédios);
  - o UV em metros;
  - as máscaras na cor de vértice (a quina, a oclusão, a fresta), que o material do jogo lê pro desgaste, a lavagem e a lasca.
  - Duas armadilhas:
    - **a face de mais de 4 lados** precisa virar triângulos antes da grade (consertado em 03/11 no `model_detail.grid`). Senão a
      exportação abre a face em leque com triângulos de até 8 m, e a sujeira de um canto pinta o triângulo inteiro: foi o "polígono de
      terra" na frente do M1;
    - **o exportador glTF do Blender 5.2** só leva a cor de vértice pro 1º material. O conserto (`_VCMapAll` no `model_lib.py`) liga
      com `RTS_VC_FIX=1`.
- **A folha de conferência antes de mostrar:** `Game/Tools/model_preview.py` (Cycles na CPU) e a folha com o conceito, o nosso em
  3/4 e a câmera do jogo com o tanque USA de régua. Medir contra o tanque e contra o conceito, peça por peça; corrigir o que está longe
  ANTES de mostrar.
- **Os agentes:** o modelo grande vai pra um agente com um briefing completo:
  - a palavra, o conceito, a família e as regras desta página;
  - os exemplos, o executor e a folha;
  - o que não tocar (Units.json, import, cenários, commit).
  Eu reviso a folha com rigor e integro no jogo.

## 4. Do modelo ao jogo

1. **O dano:** `Game/Tools/model_damage.py <pasta> <prefixo> <facção>`. Faz o `_Damaged`, o `_Wrecked`, a `_Ruin` e os pontos de fogo
   de cada estado. A laje que sobra só aparece onde o telhado está na altura H.
2. **A malha do tiro:** `python Game/Tools/elenco_hitmesh.py <chave>`. O tiro bate na fachada, não na caixa. Rodar depois do Units.json.
3. **Os dados:** o Units.json só pelo `units_json_io.load_units/save_units` (nunca `json.dump`), com os ajudantes do `elenco_units.py`
   (`structure`, `building_visual`, `damage_states`, `model_flags`).
   - Os números saem do ZH (os INI) e da proporção medida, nunca da frase dele.
   - O prédio que guarda tropa usa o bloco `transport` (o bunker da GLA: 3 vagas, a tropa de pé no ninho).
4. **A importação:** a entrada no `FACTION_MODELS` do `import_provisional.py` e depois
   `rebuild_queue.ps1 -NoMaterials -Keys <chaves>` (pela fila da placa).
   - ~64 s por malha pesada; em lotes, soltando a fila entre eles.
   - Sem `-NoMaterials` o passo dos materiais roda antes: o material é vivo pros outros, conferir antes.
5. **O código:** o C++ só compila pela fila (`compile_queue.ps1`).
   - Nunca editar fonte enquanto alguém compila (`gpu_queue.ps1 status`, nenhum UnrealBuildTool rodando).
   - A simulação se testa sem a Unreal (`Game/Tools/simtest.ps1`): as 3 compilações dão o mesmo FINAL_HASH.
   - Regra nova = teste novo no `Game/Tests/Sim/SimTests.cpp`.
6. **A prova no jogo:**
   - o cenário de laboratório no `Scenarios.json` (uma lista; as ops `camera`, `shot`, `sun`, `pause`, `spawn`, `enter`, `kill`...);
   - rodar com `game.ps1 view -Scenario <nome> -Folder <pasta> [-Record]`.
   - Conferir AMPLIADO, em movimento quando mexe, e em A/B na mesma corrente (o A e o B no mesmo dia; o jogo muda).
   - Exemplos:
     - a bandeira na sombra: o sol girando, medido no linear contra o concreto;
     - a frente do tanque: as mesmas vistas, antes e depois.
7. **Mostrar a imagem na conversa, não o caminho**, com o que mudou pra ele, curto.

## 5. O que não se faz

- Não mudar o que foi aprovado sem A/B (a versão aprovada é o piso).
- Não polir o provisório de outra sessão nem desfazer o que "o responsável" vai resolver.
- Não furar a fila da placa porque ele pediu pra ver. "Quero ver" não é "agora".
- Não matar processo pelo nome (só pelo PID).
- Não pedir confirmação a cada passo: ele decide gosto e rumo; o resto se decide com o estudo e se mostra pronto.
- Não deixar nada sem commit: "commita e pusha, o objetivo é ficar nada sem commitar e pushar", só os meus arquivos, com push.

## 6. Onde está cada guia

| Assunto | Guia |
|---|---|
| Prédios (a fórmula, a bíblia das facções, os prompts) | `docs/CONCEITO_CONSTRUCOES.md` |
| Unidades | `docs/CONCEITO_UNIDADES.md` |
| O detalhe AAA (a grade, as máscaras, a escala de 2 px) | `docs/ESTUDO_DETALHE_AAA.md` |
| O visual aprovado (areia, cratera, vila, asfalto, estrada) | `docs/RECEITAS_DO_LOOK.md` |
| A lista viva do elenco | `docs/elenco/TAREFAS.md` |
| Tudo que ele aprovou ou reprovou, com imagem | `docs/APROVADOS.md` |
| O kit civil | `docs/KIT_CIVIL_V2.md` |
| O Supply Dock | `docs/elenco/SUPPLY_DOCK_ESTUDO.md` |
| A fumaça e a luz | `docs/ESTUDO_LUZ_NA_FUMACA.md` |
| A placa (a fila) | `docs/GPU.md` |
