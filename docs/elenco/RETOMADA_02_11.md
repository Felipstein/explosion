# Onde parei (02/11, ~13h30): a sessão do Elenco pausada pelo limite de tokens

A lista inteira continua em `docs/elenco/TAREFAS.md` (as decisões de 02/11 no fim da §G). Os scripts das correntes estão em
`C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\12f1e755-ddf7-4e41-a834-371a3dfa6f7e\scratchpad\` (o `chainNN.ps1`
e a saída em `chainNN.out`).

## O que ficou rodando (processos do Windows, sem gastar token; conferir o `.out` de cada um)
1. **`chain51`**, a retomada da queda de energia, que estava nos 20 modelos do Blender, no 13º, o Type 99. Depois dele vêm:
   - o destrutor;
   - o Units.json (a China sem o Propaganda, a Rússia v2, o Strategy Center, o CC v5 e o Palace v2 da GLA);
   - as fotos A;
   - os materiais (a bandeira presa no mastro, a tinta da USA sem areia, a UsBody), seguidos da importação;
   - as fotos B e os tours (`*_B51`, `*_v51`).
   Se morreu, rodar de novo: os passos refeitos não estragam nada.
2. **`chain52`**, o A/B da máscara em todos os materiais (o defeito do exportador glTF; `model_lib.py`, `RTS_VC_FIX=1`) no tanque da USA.
   O de antes está guardado em `captures/game/models/usa_mbt_antes_vcfix`.
3. **`chain53`**, o tanque da GLA v2 no jogo e a volta de fotos (`tanque_gla_volta_v53`).
4. **`t0.ps1`**, o teste do recorte do chão (item 22) no World_TestHills, isolado.
   ⚠️ **Se ele não chegou no "desfaz", o chão desse mapa fica zerado:** rodar com `T0_MODE=undo` o
   `t0_ground_cut.py` (pela fila: `game.ps1 py <caminho>`).

## O que parei
- **O agente das 4 ideias novas** (o Herói do Megafone, os ônibus, a caixa d'água e o ferro-velho; `docs/elenco/IDEIAS_NOVAS_02_11.md`).
  - Retomar: mandar mensagem ao agente pelo id `a717dc00df978b9b3`, ou abrir um novo com o mesmo pedido.
  - Os scripts que ele já escreveu: `Game/Tools/model_china_propaganda_center_v4.py` e `model_gla_*_v3.py`, se existirem.

## O que falta, na ordem
1. Conferir as saídas das correntes acima e mostrar pra ele:
   - os prédios B da USA com a tinta nova;
   - a bandeira (fotos e vídeo);
   - o CC v5;
   - a Rússia v2;
   - o Palace v2;
   - o tanque da GLA v2;
   - o A/B da máscara.
2. As 4 ideias novas no jogo (o agente → o destrutor → o Units.json → importar).
3. O defeito do exportador: se o A/B melhorar, refazer todos os modelos com `RTS_VC_FIX=1`. Mostrar antes, porque muda a cara dos aprovados.
4. O item 22 (cavar o chão): o resultado do T0 e o plano de `docs/elenco/CAVAR_O_CHAO.md`.
5. O kit civil v3: o material, que não está no jogo. Precisa do escorrido escuro, do tijolo de adobe, da madeira gasta e das cores de carro.
6. O resto da `TAREFAS.md`.

## Decisões dele de 02/11 (não esquecer)
- **A tinta:** sem areia por enquanto (ele tem uma ideia nova); fica a textura detalhada.
- **A areia da esteira e dos pneus:** fica como está, porque a outra sessão vai trabalhar nisso.
- **Reprovados:** o Propaganda v3, o quartel v2, o Stinger v2 e o Stash v2. O Palace v2 fica.
- **Pedido de ver não é urgência:** não furar a fila sem ele pedir "agora".

## 03/11, a volta (outra conta)
- **As correntes 51, 52 e 53 morreram na pausa.** Pronto: os 15 primeiros modelos do Blender. O resto falhou na hora em que a sessão
  fechou: o console sumiu e nada chegou ao Units.json. A `chain54` refaz tudo o que faltava, numa corrente só:
  - os 5 modelos;
  - o destrutor;
  - os dados (o tanque da GLA v2 junto);
  - as fotos A;
  - os materiais e a importação;
  - as fotos B e os tours;
  - o A/B da máscara no tanque da USA.
- **O teste do recorte (T0) parou no "zero"** e deixou o chão do World_TestHills zerado. Desfeito pelo `t0_undo.ps1`, que confere com a
  foto `t0_desfeito`.
  - O que o T0 mostrou: sem Nanite, o chão não sumiu e ficou com o xadrez cinza, que é o material padrão da Unreal enquanto o shader
    compila. A foto saiu antes da compilação e o teste não decidiu nada.
  - Refazer com uma rodada de aquecimento antes da foto.
- **O agente das 4 ideias novas foi retomado.**
