# O carro amassado pelo tanque (o estudo do Look, 16/10)

Dono do visual: o Look (a malha e o material; `docs/ESMAGAR_E_DERRUBAR.md` é a regra do jogo). Os carros provisórios saem
do `Game/Tools/look_civil_props.py` (o `wreck()` do `look_tank.py`), então as variantes amassadas saem do mesmo script.
O ator do jogo procura `<malha>_Crushed`, `<malha>_CrushedFront` (a frente = +Y da malha) e `<malha>_CrushedBack`, na
pasta do kit da vila, com a mesma origem (o centro da pegada, no chão) e o mesmo giro; sem elas fica o provisório do
jogo (achata a 42% ou 70% e torce 3–5°).

## O que os jogos fazem (fontes no fim)
- **O ZH** (o código da EA: `CrushDie.cpp`, `PhysicsUpdate.cpp`):
  - os 3 pontos do carro;
  - esmaga depois que o tanque passou do ponto ("3/8 do carro" de cada vez);
  - troca seca do modelo, sem animação: `_d1` frente, `_d2` trás, `_d` os dois;
  - o `FX_CarCrush` (som, fumaça e destroços);
  - um lado já amassado + outro esmagar = o total.
- **Battlefield V:** o pneu murcha e a suspensão desce antes da lataria amassar.
- **GTA V:** o deslocamento no shader com uma máscara por vértice (o verde da cor do vértice = o quanto o vértice cede; a cabine rígida).
- **TRAIL OUT:** morph nos amassados grandes e deformação por vértice nas mossas.
- **Wreckfest e BeamNG:** soft body, caro demais pra dezenas de carros.

## Os caminhos
| caminho | a favor | contra |
|---|---|---|
| o de hoje (escala Z + giro) | normal, sombra e Lumen certos | brinquedo encolhido: roda vira elipse, teto e chassi descem igual, não marca a lagarta |
| **variantes prontas (ZH)** | a melhor forma final pelo custo; estático; casa com a troca do jogo | troca seca; não sabe onde a lagarta passou |
| WPO procedural (dados da sim) | animável, cada carro diferente | malha subdividida, normais, sombra virtual invalidando, Lumen não vê, M_Structure é de todos |
| morph em canais de UV (Epic) | formas à mão, Nanite | 2 formas no máximo (4 UV do Nanite) |
| VAT | amassado simulado | ~2 MB por variante |

## A física (o que tem de aparecer)
- **Os números:** o M1A2 pisa a 15,4 psi, ~10,8 t/m² sob a lagarta de 0,64 × 4,57 m. O teto aguenta ~3× o peso do carro (FMVSS 216), ~4 t, e cede inteiro.
- **O resultado:**
  - sob a faixa da lagarta, o carro fica com 0,3–0,5 m;
  - fora dela, o teto desce como placa presa nas colunas;
  - **o teto desce muito mais que o chassi.**
- **A chapa dobra e estufa, não encolhe:** a saliência a ≈ (2/π)·√(L·(H−L)). Um painel de 0,6 m comprimido a 50% estufa ~19 cm.
- **As rodas:** abrem 10–25° pra fora e descem 10–15 cm; o pneu estoura primeiro.
- **O que lê de cima** (o carro tem 38–66 × 17–29 px na câmera do jogo):
  1. a sombra encolhendo;
  2. a faixa achatada da lagarta com borda viva;
  3. o teto afundado;
  4. o pó e a fuligem por baixo do tanque;
  5. o tranco do tanque (já existe).
  - Enrugado fino e vidro não leem; a carcaça é queimada.

## A recomendação pro nosso caso (a decidir com o Felipe quando entrar na fila)
1. **Agora, no formato que o jogo já procura:** as 3 variantes geradas pelo mesmo script, com a malha subdividida (painéis de ~20–25 cm, laços de aresta na cintura e nas colunas, 4–6 mil vértices).
   - A forma pela física: cabine −85%, corpo −25%, a faixa da lagarta a 0,3–0,5 m, as colunas tombando 10–25 cm, as portas estufando, as rodas abertas.
   - Duas ou três sementes por variante, pra não repetir.
   - A torção pro lado da lagarta fica com o giro do ator, que o jogo já faz.
2. **Depois, se a troca seca não bastar no vídeo:** o WPO dirigido pelos dados da sim, só nas instâncias do carro esmagável, atrás de um static switch.
   - As manchas usando a posição sem o offset, a sombra em Auto durante e Rigid depois, sem o Lumen de campo de distância.
   - A medir com 30 carros.
3. **Antes de mostrar:** a folha lado a lado com fotos reais de carro esmagado por tanque e com os `_d` do ZH, e o vídeo.

## As fontes
- O ZH (código da EA):
  - https://raw.githubusercontent.com/electronicarts/CnC_Generals_Zero_Hour/main/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Die/CrushDie.cpp
  - https://raw.githubusercontent.com/electronicarts/CnC_Generals_Zero_Hour/main/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/PhysicsUpdate.cpp
- Battlefield V: https://mp1st.com/news/more-battlefield-5-destruction-being-added-watch-a-car-being-pancaked-by-a-tank
- GTA V:
  - https://docs.sollumz.org/2.8/documentation/fragments.yft/vehicle-shaders
  - https://gtamods.com/wiki/Vehicles.meta
- Unreal (Epic):
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/static-mesh-morph-targets-in-unreal-engine
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine
- A física:
  - https://www.globalsecurity.org/military/systems/ground/m1-specs.htm
  - https://www.nhtsa.gov/sites/nhtsa.dot.gov/files/roof_crush_final_rule.pdf
