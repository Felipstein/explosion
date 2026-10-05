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

Referência WebGL: `python reference\webgl\devserver.py 8129` e abrir
http://localhost:8129.

## Ferramentas (`BlastLab/Tools`)

| Comando / script | O que faz |
|---|---|
| `blast.ps1 setup` | compila o C++ e monta o campo de teste e a explosão |
| `blast.ps1 compile` | compila os módulos C++ |
| `blast.ps1 build-scene` | monta o campo de teste (cenário, sol, céu, exposição, tonemap) |
| `blast.ps1 build-explosion` | converte a explosão gravada e põe no campo, com sequência e luzes |
| `blast.ps1 view [low\|medium\|high\|epic\|cine]` | abre o simulador; `-Folder x` faz o teste automático (telas + qps) |
| `blast.ps1 render <Seq>` | renderiza uma sequência em `captures/ue/`; `-Folder`, `-Cvars` pra A/B; `-ProfileGpu` mede a GPU |
| `convert_bake.py` | explosão gravada no WebGL → quadros pro volume da Unreal |
| `make_ground.py`, `make_noise.py` | texturas do chão e do ruído de detalhe |
| `compare.py` | folha WebGL × Unreal nos mesmos instantes |
| `sheet.py` | folha de quadros de um render |
| `gpu_profile.py` | tempos de GPU por passe do último `-ProfileGpu` |

## Documentos

- `docs/STATUS.md`: onde estamos e o histórico.
- `docs/MIGRACAO.md`: WebGL → Unreal, sistema por sistema, números calibrados.
- `docs/ARCHITECTURE.md`: como cada sistema do WebGL funciona, com as referências.
