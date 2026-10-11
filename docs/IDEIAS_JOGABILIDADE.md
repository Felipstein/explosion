# Ideias de jogabilidade (pra depois da base jogável)

Ideias do Felipe guardadas pra não perder. Não entram agora: a base jogável vem primeiro
(`docs/BASE_JOGAVEL.md`). Cada ideia guarda as palavras dele e as primeiras notas de como fazer.

---

## 1. Mureta que protege o tanque (09/10/2026)

**A ideia, nas palavras dele:** "se o seu tanque estiver atrás de uma mureta, a probabilidade do
tanque inimigo um pouco mais distante acertar a mureta ao invés de você é enorme. Mas é claro que
um tiro de tanque é hit kill na mureta e ela vai desabar, mas você não toma dano, te dando uma
vantagem."

Veio da foto de perto do jogo (`docs/concept/scenes/desert_golden_hour/best/close_game_cam_gpt.webp`): o tanque do GLA
atirando de trás do muro de barro do quintal.

**Como funcionaria:**
- **A mureta é um objeto do jogo.** Muro de quintal, mureta de barro e talvez sacos de areia ocupam
  quadrados da grade de 4 m e têm vida.
- **O tiro que cruza a mureta pode parar nela,** quando a linha do tiro passa por ela antes de chegar
  no tanque. Quanto mais longe o atirador, maior a chance, porque o tiro chega mais "rasante".
- **Um tiro de tanque derruba a mureta,** que vira escombro baixo. O tanque atrás não leva dano
  daquele tiro, mas fica descoberto pro próximo.
- **O resultado:** ficar atrás da mureta "compra" um tiro. É vantagem tática sem ser invencível.

**Notas pra quando for fazer:**
- **A chance sai da simulação,** com o sorteio próprio dela, igual nos dois PCs (multiplayer em
  lockstep, `docs/JOGO.md`). Nunca da física da Unreal.
- **Referências de gênero:**
  - Company of Heroes: cobertura verde e amarela reduz a chance de acerto, e o muro é destrutível;
  - Zero Hour: a infantaria se esconde em prédio.
  - Medir o equilíbrio: a mureta não pode virar parede infinita.
- **Liga com a arte:** o muro de quintal com portão já tem ficha aprovada
  (`docs/concept/buildings/civil/civ_wall_gate_*.webp`). Precisa do escombro (`_Rubble`) e da queda
  (`docs/PRODUCAO_3D.md` §6).
- **Prima da carcaça e da cratera como cobertura** (`docs/BASE_JOGAVEL.md`, decisões 10 e 24). As
  três seriam o mesmo sistema de cobertura.
- **Leitura de cima:** o jogador precisa ver que o tanque está coberto (um ícone ou a borda do anel
  de seleção) e ver a mureta caindo.
