# Construções civis do deserto (aprovadas 08/10/2026)

Fichas tiradas da cena do jogo (`docs/concept/scenes/day_game_cam.webp`) com o prompt
"keep exactly the same style… do not add anything that is not in the attached image"
(`docs/PRODUCAO_3D.md` §10). Referência de todas as casas das cenas: `docs/concept/towns/`.

| Peça | Fichas | Escolha |
|---|---|---|
| Casa de 1 andar, 8 × 8 m | `civ_house_1story_gemini`, `civ_house_1story_gpt` | as duas |
| Casa de 2 andares, 8 × 8 m, ~7 m | `civ_house_2story_gemini` | Gemini |
| Casa com varanda de toldo, 8 × 12 m | `civ_house_porch_gemini`, `civ_house_porch_gpt` | as duas (Gemini 1º) |
| Casa comprida de 1 andar, 16 × 8 m | `civ_house_long_gemini` (base: vistas retas), `civ_house_long_gpt` | as duas |
| Muro de quintal com portão | `civ_wall_gate_gpt` (padrão: pilarete + trecho de 4 m + portão), `civ_wall_gate_gemini` (variação: barro de topo arredondado) | as duas |

**Textura: as fichas não viram textura.** Gemini e GPT são só referência de forma e de cor.
O jogo usa um conjunto só de materiais pra vila toda (reboco de barro escaneado, madeira, sujeira),
os mesmos em todas as casas, com a cor calibrada na cena aprovada. Assim as casas do Gemini e do
GPT saem idênticas no jogo, e a diferença de qualidade entre as fichas some.

Ao modelar: o portão do Gemini tem de ocupar a espessura toda do muro (na ficha é uma ripa
fina); o do GPT pode ser um pouco mais alto. Reboco, sujeira e tijolo vão na textura, não na malha.
