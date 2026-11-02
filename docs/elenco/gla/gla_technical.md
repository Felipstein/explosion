# Technical (GLA, a picape armada)

## 1. Ficha rápida

| | |
|---|---|
| Papel | o raide: rápido, barato, caçador de soldado e de coletor |
| Custo e tempo | $500, 5 s |
| Vida e blindagem | 180, tanque |
| Velocidade | **23,7 m/s** (o veículo de chão mais rápido, junto com o Rocket Buggy); acelera 26,3, freia 13,2; giro 180°/s, a metralhadora 240°/s |
| Visão | 39,5 m |
| Arma | metralhadora pesada: 10 de arma leve a cada 0,2 s, alcance 39,5 m, o traçante a 160 m/s (só no chão) |
| Esmagador | nível 1: o soldado na frente dela morre |
| Requer | Arms Dealer |

## 2. A ideia

**A picape com a metralhadora na caçamba e a bandeira no mastro.** A arma da guerrilha moderna: uma picape civil, uma metralhadora
pesada no tripé, a bandeira verde batendo atrás da cabine. Chega antes de todo mundo, atira, some.

- **A palavra de cima:** a METRALHADORA no tripé na caçamba + a BANDEIRA verde.
- **Inspiração real:** as "technicals" das guerras da África e do Oriente Médio (o nome vem da Somália dos anos 90): a picape
  civil (a Hilux é a mais comum) com a metralhadora pesada DShK de 12,7 mm no tripé. Fictícia, não cópia: a Technical v5 do dono,
  modelada no Blender (`gla_technical_v5.blend`), a ficha `docs/concept/units/gla_technical_sheet.webp`.
- **Por que existe:** a GLA é rápida e esperta. A Technical é o raide do minuto 2, a caça à picape e ao caminhão inimigo, a
  resposta à infantaria em massa.

## 3. Como funciona

- **No jogo:** 10 a cada 0,2 s = **50/s em soldado**, **12,5/s em tanque** (arma leve 25%), **25/s em prédio** (50%); não atira no ar.
  Mata um Red Guard (120) em ~2,2 s, um Ranger (180) em ~3,4 s, um caminhão (300) em 24 s, um Inferno ou um Rocket Buggy (120) em
  ~10 s.
- **A supressão (no jogo):** a rajada dela suprime um soldado em ~1,2 s e prende em ~2,7 s (a conta da simulação 29: o soldado
  preso rasteja a 25% e recarrega a 200%); os soldados a até 4 m do alvo levam metade da bala (a rajada varre o grupo).
- **A blindagem:** a do caminhão (tanque, 25% de arma leve). No ZH era a TruckArmor (50% de arma leve), que aqui não existe.
- **Forte contra:** infantaria, coletor (caminhão, picape), artilharia sem escolta (o Inferno e o Tomahawk), o Worker e o Dozer.
- **Fraca contra:** tanque (3 tiros de canhão a matam em 4 s), a Quad Cannon (25/s no veículo: ~7 s), o RPG e o Tank Hunter (5
  foguetes). O gattling da China faz só 10% nela (o Gattling Tank leva 48 s): quem a para é o canhão e o foguete.
- **O contra dela:** o tanque, o RPG, o Tank Hunter, a Quad Cannon. **Ela é o contra de:** o Red Guard, o Ranger, o Rebel, o
  caminhão, a artilharia sozinha.

## 4. Em batalha

- **Temperamento: o ousado.** Atrevido, rápido, provocador. Entra rindo e sai antes do tanque virar a torre.
- **O ritmo:** o motor gritando, a frenagem, a rajada longa (TA-TA-TA-TA), a arrancada; nunca parada.
- **O jogador bom** usa 4 a 6 em bando: entra pela lateral, mata os coletores, foge antes do tanque chegar; contra infantaria em
  campo aberto, gira em volta dela atirando (a metralhadora gira 240°/s).
- **A IA** a põe na mistura da fábrica pelo que falta.
- **Sozinha e ferida:** foge rápido (é a única coisa que ela faz melhor que todos); abaixo de 35%, a chama.

## 5. A vida

- **No jogo:** a bandeira de 1 × 0,65 m no mastro vira pro vento aparente, ondula e cai sem vento (`StepFlag`: o vento do mapa); a
  metralhadora gira 240°/s; o traçante e o clarão da boca na ponta do cano; as 4 rodas, a poeira e a marca; a morte de gasolina.
- **Ideia (parada):** o motor tremendo a carroceria; o cano da metralhadora apontado pro alto quando ela está ociosa (pose de
  vigia).
- **Ideia (andando):** a carroceria balança e a metralhadora balança junto no tripé; a poeira alta (é a mais rápida); na curva a
  traseira escorrega.
- **Ideia (atirando):** as cápsulas grandes de 12,7 mm voando da caçamba; a fumaça fina saindo da boca.

## 6. O visual

- **De cima:** a picape de 6,3 m (escala 1,3 da malha) com o capô, a cabine, a caçamba de tábuas e a metralhadora no tripé no meio da
  caçamba, a bandeira verde atrás da cabine.
- **Assinatura:** a metralhadora na caçamba e a bandeira.
- **Cores:** o areia sujo com ferrugem e lama (a camuflagem do alvo r1); o verde do jogador na bandeira.
- **Nunca:** blindagem pesada; pintura limpa; cinza ou branco (confunde com a USA).

## 7. Tiro, projétil, impacto, destruição

- **A boca:** o clarão grande pra uma metralhadora (0,39 m), seco, piscando a cada tiro; a fumaça cinza fina.
- **O projétil:** o traçante de 3,9 m a 160 m/s, alaranjado, no leque largo (3 m de espalhado: a picape balança).
- **O impacto:**
  - no soldado: a linha de poeira andando até ele; ele abaixa e rasteja (a supressão);
  - no tanque: faísca e "ping", pouco;
  - no prédio: o reboco pulando em pontos;
  - no chão: os esguichos de areia.
- **A morte (no jogo):** a morte de gasolina (`death_kind gasoline`, `death_scale` 0,4: a bola rápida e amarela, menor que a de um
  tanque); a bandeira some no fogo; a carcaça fica 48 s e vira sucata ($75 em ~6 s).
- **A morte (ideia):** a metralhadora voa da caçamba e cai do lado; a bandeira queima no mastro antes de sumir.

## 8. O som

- **Seleção (variantes):** o motor acelerando no vazio duas vezes; a metralhadora sendo engatilhada (o "clack-clack" pesado); a buzina
  curta; o grito de comemoração.
- **As falas** (inglês; o atirador da caçamba, voz jovem, eufórica, gritando por cima do motor):
  - selecionada: "Technical!" / "Ready to ride!" / "Where's the fight?" / "Fast and angry!" / "Point me, I go!"
  - andando: "Floor it!" / "Hold on!" / "Faster, faster!" / "On our way!"
  - atacando: "Light them up!" / "Spray them!" / "Keep shooting!" / "They can't catch us!"
  - sob fogo: "Tank! Get us out!" / "Drive, drive!" / "Too hot, pull back!"
  - pronta: "Technical, ready to raid."
- **O corpo:** o motor de picape velho (agudo, gritando no alto giro); os pneus na areia; a carroceria batendo; a bandeira estalando no
  vento.
- **A arma:** a metralhadora pesada (grave e lenta pra uma metralhadora: TA-TA-TA, cada tiro separado, 5 por segundo no jogo); as
  cápsulas grandes tilintando na caçamba.
- **A morte:** a gasolina estourando (rápido, agudo, "fwoomp"), a munição pipocando curta, a lataria fina queimando.

## 9. Ideias pra depois

- **A Technical da sucata:** como no ZH, cada Technical nova sai com uma de 3 armas, conforme a sucata recolhida: a metralhadora
  (padrão), o canhão sem recuo (antitanque) ou o lançador de foguetes. Aqui, a arma depende da última carcaça que o Worker desmanchou
  (desmanchou tanque: sai com o canhão).
- **O salto:** a 23 m/s, a Technical passa por cima de uma duna ou de um barranco e voa 1 a 2 s (sem atirar), o momento bonito da
  câmera deitada.
- **O resgate do atirador:** quando a Technical morre, o atirador às vezes pula fora e vira um Rebel (25% de chance), a GLA que não
  desperdiça gente.

## 10. O prompt do HUD

Em `../PROMPTS_HUD.md#gla_technical` (feito no fim, com o modelo provisório como anexo de forma).
