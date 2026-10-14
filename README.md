# BlastLab

Simulador de explosões volumétricas nível AAA: bola de fogo, fumaça e a
interação delas com o mundo (avião, míssil, disparo), pensado pra câmera de
um RTS. Explosões de verdade em volume (marchadas em 3D), não flipbooks.

## Mapa do repositório

```
BlastLab/                  o simulador (Unreal Engine 5.8)
  Source/BlastLab/         código do simulador (C++): ExplosionVolume, ExplosionDemo
  Source/BlastLabEditor/   ferramentas de importação chamadas pelo Python
  Tools/                   ferramentas pra montar, testar e criar explosões
  Config/                  configuração do projeto
  Content/                 assets (todos gerados pelas Tools)
Game/                      o jogo (Unreal Engine 5.8): menu + skirmish; docs/JOGO.md
  Source/SimCore/          a partida: simulação determinística em C++ puro (ponto fixo)
  Source/RTSGame/          a Unreal desenhando: câmera, unidades, HUD, telas, teste automático
  Source/BlastFx/          a explosão do BlastLab no jogo
  Tools/                   game.ps1 (compilar, montar, abrir, testar, gravar vídeo), simtest.ps1
WorldForge/                o criador de mundos do RTS (Unreal Engine 5.8): docs/WORLDFORGE.md
  Source/WorldForge/       Generation/ (o gerador, só Core) + terreno, câmera RTS, painel
  Tools/                   forge.ps1 (compilar, montar, abrir, testar), texturas, materiais
reference/webgl/           o simulador WebGL original (referência visual aprovada)
docs/                      estado, plano de migração, arquitetura
captures/                  renders e testes (fora do git)
```

## Rodar

Pré-requisitos: Unreal Engine 5.8 em `C:\Program Files\Epic Games\UE_5.8`,
Visual Studio com C++, Python 3 com `numpy` e `pillow`.

```powershell
powershell -File BlastLab\Tools\blast.ps1 setup        # compila e monta tudo do zero (primeira vez)
powershell -File BlastLab\Tools\blast.ps1 view         # abre o simulador (gráfico máximo)
powershell -File BlastLab\Tools\blast.ps1 editor       # abre o editor da Unreal
```

O jogo: `powershell -File Game\Tools\game.ps1 setup` na primeira vez, depois
`powershell -File Game\Tools\game.ps1 view` (abre no menu; "Jogar Skirmish"
começa a partida contra a IA). Controles no rodapé da tela; detalhes em `docs/JOGO.md`.

Criador de mundos: `WorldForge\WorldForge.cmd` (ou `powershell -File
WorldForge\Tools\forge.ps1 setup` na primeira vez). Detalhes em `docs/WORLDFORGE.md`.

Referência WebGL: `python reference\webgl\devserver.py 8129` e abrir
http://localhost:8129.

### No simulador

| Ação | O quê |
|---|---|
| clique no terreno | detona a arma escolhida no painel |
| botão esquerdo arrastado | orbita a câmera |
| roda | zoom (8–800 m) |
| botão direito arrastado | rola o mapa (estilo C&C Generals) |
| `P` | pausa · `[` `]` câmera lenta/rápida · `\` volta a 1× |
| `,` `.` | hora do dia −/+10 min · `T` ciclo automático |
| `1`–`4` | qualidade baixa/média/alta/ultra · `H` esconde o painel · espaço detona no centro |
| `A` `M` `D` `B` | passa avião (A-10), míssil, disparo ou o bombardeiro B-2 pela última explosão |

O painel tem os **campos** de teste: o de caixas (o layout do WebGL) e os de
chão de verdade, feitos das fatias do WorldForge: deserto com estrada, neve e
floresta com grama. Eles são montados com `blast.ps1 worlds`, que copia as
fatias do WorldForge (fora do git, como lá). A troca leva a hora do dia junto.

As camadas em volta de cada explosão (clarão, anel de poeira, destroços,
torrões, faíscas, marca queimada) seguem a receita da arma em
`BlastLab/Config/BlastRecipes.json`: edite, salve e detone de novo (o
simulador relê o arquivo sozinho). Plano das explosões em `docs/EXPLOSOES.md`.

Na primeira abertura o painel mostra "preparando": a Unreal está compilando os
shaders das explosões (das próximas vezes é rápido).

## Ferramentas (`BlastLab/Tools`)

| Comando / script | O que faz |
|---|---|
| `blast.ps1 setup` | compila o C++ e monta o campo de teste e a explosão |
| `blast.ps1 compile` | compila os módulos C++ |
| `blast.ps1 build-scene` | monta o campo de teste (cenário, sol, céu, exposição, tonemap) |
| `blast.ps1 build-explosion` | converte a explosão gravada e põe no campo, com sequência e luzes; a fumaça do campo e os materiais do avião/míssil/disparo (`build_movers.py`) |
| `blast.ps1 worlds` | traz as fatias do WorldForge e monta os campos com chão de verdade (`Range_Desert`, `Range_Snow`, `Range_Forest`; `build_worlds.py`) |
| `blast.ps1 view [baixa\|media\|alta\|ultra]` | abre o simulador; `-Map Range_Desert` abre outro campo; `-Folder x` faz o teste automático (telas + qps); `-GameArgs '-AutoPass=plane:4.5 -benchmark -fps=60'` passa avião/míssil/disparo em passo fixo |
| `blast.ps1 render <Seq>` | renderiza uma sequência em `captures/ue/`; `-Folder`, `-Cvars` pra A/B; `-ProfileGpu` mede a GPU |
| `convert_bake.py` | explosão gravada no WebGL → quadros pro volume da Unreal |
| `make_ground.py`, `make_noise.py` | texturas do chão e do ruído de detalhe |
| `compare.py` | folha WebGL × Unreal nos mesmos instantes |
| `concept_sheet.py` | folha lado a lado com a arte conceitual (todo visual novo passa por ela) |
| `sheet.py` | folha de quadros de um render |
| `gpu_profile.py` | tempos de GPU por passe do último `-ProfileGpu` |

## Documentos

- `docs/STATUS.md`: onde estamos e o histórico.
- `docs/MIGRACAO.md`: WebGL → Unreal, sistema por sistema, números calibrados.
- `docs/ARCHITECTURE.md`: como cada sistema do WebGL funciona, com as referências.
- `docs/WORLDFORGE.md`: o criador de mundos (pesquisa, proposta, como gera, medições).
- `docs/concept/AMBIENTES.md`: os ambientes do jogo (biomas, horas, climas), da arte conceitual ao WorldForge e ao look.
