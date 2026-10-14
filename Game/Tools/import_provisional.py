"""Leva os modelos PROVISÓRIOS da base pra Unreal (rodar: game.ps1 models, que antes gera os GLB
com o Blender: Tools/blender_provisional.py).

- importa cada GLB de captures/game/prov em /Game/Structures/<pasta>/ com Nanite;
- monta os materiais da base: M_Structure (opaco) e M_StructureBuild (a obra: corta acima da
  altura construída, com a linha de solda acesa no corte e o lado de dentro mais escuro), e uma
  instância de cada um por nome de material (Body, Trim, ... os nomes que o Blender deu);
- troca os materiais do importador pelos nossos (os do importador não têm a marca de Nanite:
  fix_unit_materials.py do BlastLab, 07/10) e apaga os dele.

O material (estudo de 10/10, o mesmo método do reboco das casas da sessão de gráficos):
- sem UV: o concreto escaneado (asphalt_04, CC0, da fatia do WorldForge) projetado nos 3 planos
  do mundo, e o resto com a variação larga dele (o metal pintado não fica "de plástico");
- terra no pé da parede e poeira no que olha pra cima: o prédio senta no deserto (a regra de
  coerência: o mesmo pó da areia);
- faixa de perigo procedural (preto e amarelo na diagonal do mundo);
- a cor do jogador por dado de primitiva (índices 0–3), a energia (4: as luzes apagam sem
  energia ou em obra) e a altura da obra (5, metros acima do pivô).
Sem a fatia (Slice), cai na textura de ruído do motor.
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SRC = os.path.join(REPO, 'captures', 'game', 'prov')
ROOT = '/Game/Structures'
MATS = ROOT + '/Materials'

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = lambda *a: unreal.log('PROVISIONAL ' + ' '.join(str(x) for x in a))
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

# nome do material no Blender → (cor linear, rugosidade, metal, extras)
# cores pela bíblia (CONCEITO_CONSTRUCOES.md): metal claro cinza-branco, concreto claro, amarelo
# de pórtico, a cor do jogador só em faixas e lonas; luz quente, nunca neon
SLOTS = {
    'Body': ((0.50, 0.49, 0.46), 0.55, 0.0, {'Grime': 0.22}),
    'Trim': ((0.13, 0.13, 0.13), 0.45, 0.65, {'Grime': 0.15}),
    'Dark': ((0.025, 0.025, 0.025), 0.55, 0.3, {}),
    'Yellow': ((0.70, 0.40, 0.025), 0.5, 0.0, {'Grime': 0.25}),
    'Player': ((0.5, 0.5, 0.5), 0.55, 0.0, {'PlayerMask': 1.0, 'Grime': 0.18}),
    'Concrete': ((0.34, 0.33, 0.30), 0.92, 0.0, {'ConcreteMask': 1.0}),
    'Hazard': ((0.68, 0.42, 0.02), 0.6, 0.0, {'Stripes': 1.0, 'Grime': 0.2}),
    'Glass': ((0.008, 0.010, 0.013), 0.06, 0.0, {}),
    'Light': ((0.8, 0.7, 0.55), 0.4, 0.0, {'EmissiveColor': (1.0, 0.74, 0.46), 'EmissiveNits': 40000.0}),
    'Core': ((0.9, 0.86, 0.8), 0.3, 0.0, {'EmissiveColor': (1.0, 0.92, 0.80), 'EmissiveNits': 90000.0}),
    'Beacon': ((0.8, 0.4, 0.05), 0.4, 0.0, {'EmissiveColor': (1.0, 0.42, 0.05), 'EmissiveNits': 70000.0, 'Blink': 1.4}),
    'Crate': ((0.10, 0.10, 0.045), 0.8, 0.0, {'Grime': 0.25}),
    'Rubber': ((0.012, 0.012, 0.012), 0.92, 0.0, {}),
    'Paint': ((0.62, 0.62, 0.59), 0.6, 0.0, {'Grime': 0.2}),
    # o tanque do Look (look_tank.py, as cores dele em linear): a areia da USA, a cor do jogador
    # nas faixas, anéis e retalhos (slot Blue, que vira Team) e na lona (Cloth, mais escura)
    'TankPaint': ((0.552, 0.565, 0.527), 0.55, 0.0, {'Grime': 0.2}),
    'TankTeam': ((0.5, 0.5, 0.5), 0.5, 0.0, {'PlayerMask': 1.0, 'Grime': 0.15}),
    'TankCloth': ((0.5, 0.5, 0.5), 0.85, 0.0, {'PlayerMask': 1.0, 'Grime': 0.3}),
    'TankMetal': ((0.114, 0.114, 0.107), 0.45, 0.6, {'Grime': 0.15}),
    'TankTrack': ((0.036, 0.034, 0.030), 0.85, 0.3, {}),
    # o kit civil da vila (as casas de adobe do Look, look_houses.py: as cores dele em linear) até o
    # M_Plaster do plugin RTSLook chegar
    'CivPlaster': ((0.49, 0.35, 0.20), 0.9, 0.0, {'Grime': 0.3, 'ToneVariation': 1.0}),
    'CivWood': ((0.107, 0.048, 0.019), 0.8, 0.0, {'Grime': 0.2}),
    'CivMetal': ((0.188, 0.181, 0.155), 0.5, 0.5, {'Grime': 0.25}),
    'CivCloth': ((0.305, 0.216, 0.122), 0.9, 0.0, {'Grime': 0.2}),
    # as carcaças e miudezas do kit (os slots do tanque do Look: carvão e ferrugem)
    'CivChar': ((0.010, 0.008, 0.007), 0.92, 0.0, {}),
    'CivRust': ((0.068, 0.027, 0.013), 0.85, 0.2, {'Grime': 0.2}),
}
# o slot do Blender no tanque do Look → a instância nossa
TANK_SLOTS = {'Paint': 'TankPaint', 'Blue': 'TankTeam', 'Team': 'TankTeam', 'Cloth': 'TankCloth', 'Dark': 'Dark',
              'Metal': 'TankMetal', 'Lens': 'Glass', 'Track': 'TankTrack'}
TANK_SRC = os.path.join(REPO, 'captures', 'look', 'tank_glb')
TANK_PARTS = ['Hull', 'Turret', 'Barrel', 'Wheels', 'Track_l', 'Track_r']
TANK_ROOT = '/Game/Units/USA_MBT_LL'
# o Dozer provisório por script (sessão Modelos 3D, 13/10, aprovado pelo Felipe): Game/Tools/model_usa_dozer.py →
# captures/game/models/usa_dozer; os slots do tanque mais o amarelo da lâmina, a cúpula da sirene (pisca) e a lâmpada
DOZER_SRC = os.path.join(REPO, 'captures', 'game', 'models', 'usa_dozer')
DOZER_ROOT = '/Game/Units/USA_Dozer'
DOZER_SLOTS = dict(TANK_SLOTS, **{'Yellow': 'Yellow', 'Beacon': 'Beacon', 'Light': 'Light'})
# o caminhão de suprimento novo (Modelos 3D, 14/10, aprovado pelo Felipe): o casco, as 6 rodas e a caixa pequena da carga,
# com a mesma tabela de slots do Dozer
TRUCK_SRC = os.path.join(REPO, 'captures', 'game', 'models', 'usa_supply_truck')
TRUCK_ROOT = '/Game/Units/USA_SupplyTruck'
# a vila (12/10): o kit civil do Look em captures/look/civil_glb/SM_civ_<tipo>.glb, com os tipos do
# village.json do WorldForge; o jogo põe cada peça pelo JSON (RTSMatchSubsystem::SpawnVillage)
CIVIL_SRC = os.path.join(REPO, 'captures', 'look', 'civil_glb')
CIVIL_ROOT = '/Game/Village'
CIVIL_SLOTS = {'Plaster': 'CivPlaster', 'Wood': 'CivWood', 'Metal': 'CivMetal', 'Cloth': 'CivCloth', 'Dark': 'Dark',
               'Paint': 'TankPaint', 'Lens': 'Glass', 'Track': 'TankTrack', 'Blue': 'CivCloth', 'Team': 'CivCloth',
               'Char': 'CivChar', 'Rust': 'CivRust'}

CONCRETE = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_diff_4k'
CONCRETE_N = '/Game/Slice/Surfaces/asphalt_04/T_asphalt_04_nor_dx_4k'
STAIN = '/Game/Slice/Surfaces/aerial_beach_01/T_aerial_beach_01_diff_4k'
NOISE = '/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'
FLAT_N = '/Engine/EngineMaterials/DefaultNormal'

CODE = r"""
// As manchas da carcaça e do dano (13/10): ruído de valor 3D sem textura (o hash do Inigo Quilez), 3 oitavas.
// A foto da praia, limiarizada, dava chuvisco em qualquer mip (ela não tem mancha larga), e na distância do jogo
// o mip fixo serrilhava: a carcaça saía granulada como casca. N() sai esticado pela faixa medida em 400 mil pontos
// (p5/p95 0,30/0,70 → 0/1), então os limiares da receita do Look dão a cobertura dele: fuligem 23%, ferrugem
// 15%, cinza 21%.
struct FRTSNoise
{
    float H(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
    float V(float3 x)
    {
        float3 i = floor(x);
        float3 f = frac(x);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(H(i), H(i + float3(1, 0, 0)), f.x), lerp(H(i + float3(0, 1, 0)), H(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(H(i + float3(0, 0, 1)), H(i + float3(1, 0, 1)), f.x), lerp(H(i + float3(0, 1, 1)), H(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }
    float N(float3 x) { return saturate(((0.5 * V(x) + 0.25 * V(x * 2.03 + 17.1) + 0.125 * V(x * 4.01 + 31.7)) / 0.875 - 0.30) / 0.40); }
};
FRTSNoise Nz;
float3 n = normalize(N);
float3 w = pow(abs(n), 4.0); w /= (w.x + w.y + w.z);
float3 q = WP * 0.01;
float3 P = (WP - Obj) * 0.01;
float T = 2.4;
float3 c = Conc.Sample(ConcSampler, q.yz / T).rgb * w.x + Conc.Sample(ConcSampler, q.xz / T + 0.31).rgb * w.y
         + Conc.Sample(ConcSampler, q.xy / T + 0.57).rgb * w.z;
float lum = dot(c, float3(0.3, 0.59, 0.11));
float stain = Stain.Sample(StainSampler, q.yz / 9.0).g * w.x + Stain.Sample(StainSampler, q.xz / 9.0 + 0.2).g * w.y
            + Stain.Sample(StainSampler, q.xy / 9.0 + 0.4).g * w.z;
float3 base = lerp(BaseColor, PlayerColor.rgb * 0.45, PlayerMask);
// metal pintado: a variação larga do concreto, fraca (manchas de uso, não textura)
base *= lerp(1.0 - Grime, 1.0 + Grime * 0.35, smoothstep(0.55, 1.45, lum / 0.26));
// concreto: o escaneado puxado pra cor pedida
base = lerp(base, c / 0.26 * BaseColor, ConcreteMask);
// faixa de perigo: preto e amarelo a 45° no mundo (0,45 m cada)
if (Stripes > 0.5 && frac((P.x + P.y + P.z) / 0.9) < 0.5) base = float3(0.018, 0.017, 0.016);
base *= lerp(0.86, 1.04, saturate((stain - 0.15) / 0.2));
// cada casa um tom (13/10, o Felipe pela comparação com a maquete; a receita do Look, look_lab.py PLASTER_CODE): ~38%
// caiadas, ~30% ocre, o resto o reboco de hoje, e a faixa de sujeira até ~1,5 m. Na maquete o sorteio é pelo lugar da
// casa; aqui as casas são instâncias de uma malha só, então é o aleatório de cada instância (só o reboco liga: ToneVariation)
float toneFoot = 0.0;
// (13/10, a v2: o Felipe pelo Look, "a variação de cor das casas está forte demais": a metade do contraste; e o mesmo tom
// pra casa e muros do mesmo terreno, pelo dado da instância que o jogo grava do lote do village.json; sem ele, o
// aleatório da instância de antes)
if (ToneVariation > 0.5)
{
    float hsh = ILot > 0.0005 ? frac(ILot) : IRand;
    base *= lerp(float3(0.965, 0.975, 0.99), float3(1.035, 1.015, 0.985), hsh);
    float hsh2 = frac(hsh * 7.31 + 0.17);
    // (o reboco do jogo é mais claro que o da maquete, 0,49/0,35/0,20 contra 0,40/0,31/0,22: o mesmo creme a 55% quase não
    // lia na câmera; a 70% num creme mais claro dá o mesmo contraste relativo da maquete)
    base = lerp(base, float3(0.56, 0.50, 0.40), 0.35 * step(0.62, hsh2));          // caiada (creme)
    base *= lerp(float3(1.0, 1.0, 1.0), float3(1.05, 0.96, 0.85), step(hsh2, 0.3));   // ocre
    float sn = saturate((stain - 0.215) / 0.045);
    toneFoot = 1.0 - smoothstep(0.8 + 0.5 * sn, 1.6 + 0.6 * sn, P.z);
    base = lerp(base, base * float3(0.55, 0.48, 0.40), 0.9 * toneFoot);
}
// terra no pé (sobe até ~1 m, mais onde a mancha é forte) e o pó do deserto no que olha pra cima (no reboco com tom, a
// faixa de cima já é a sujeira do pé)
float foot = (1.0 - smoothstep(0.0, 0.7 + 1.4 * stain, P.z)) * (ToneVariation > 0.5 ? 0.0 : 1.0);
base = lerp(base, base * float3(0.74, 0.62, 0.48), 0.6 * foot);
float up = smoothstep(0.6, 0.95, n.z) * (1.0 - ConcreteMask);
base = lerp(base, float3(0.40, 0.31, 0.21), 0.16 * up * saturate(stain * 2.0));
Rough = lerp(lerp(Roughness, 0.95, ConcreteMask), 0.9, 0.5 * foot + 0.3 * up);
Metal = Metallic * (1.0 - 0.6 * foot);
// luzes: piscam (o giroflex), apagam sem energia ou em obra
float pulse = Blink > 0.0 ? (frac(Time * Blink) < 0.5 ? 1.0 : 0.06) : 1.0;
float3 emis = EmissiveColor.rgb * EmissiveNits * pulse * lerp(0.03, 1.0, saturate(Power));
// a obra: corta acima da altura construída; a linha de solda acesa no corte; o lado de dentro escuro
float d = Cut - P.z;
Mask = d >= 0.0 ? 1.0 : 0.0;
float weld = saturate(1.0 - d / 0.35) * (d >= 0.0 ? 1.0 : 0.0) * Building * Weld;
emis += float3(1.0, 0.52, 0.16) * 55000.0 * weld * weld;
base *= lerp(1.0, Face > 0.0 ? 1.0 : 0.45, Building);
// o dano no veículo vivo (13/10; os estados do ZH: danificado abaixo de 70% da vida, muito danificado abaixo de
// 35%; o dado de primitiva 9 = 1 − vida): as camadas da carcaça em placas de ~0,9 m que crescem com o dano, a
// tinta queimada e lascada primeiro, a fuligem de impacto depois
if (Damage > 0.2 && Burnt < 0.5)
{
    // (o ruído era a foto da praia, que no verde linear só vai de 0,22 a 0,25: nenhuma placa passava do limiar; com
    // o ruído novo em placas de 0,9 m o tanque vivo virou camuflagem, 13/10: lascas de ~0,35 m, de ~5% da
    // superfície aos 70% da vida a ~25% perto da morte; a fuligem de impacto abaixo de 50%, de ~5% a ~12%)
    float nD = Nz.N(q / 0.35 + 11.7);
    float k = saturate((Damage - 0.3) / 0.7);
    float chip = smoothstep(0.95 - 0.23 * k, 0.99 - 0.23 * k, nD);
    float k2 = saturate((Damage - 0.5) / 0.5);
    float sootD = smoothstep(0.95 - 0.13 * k2, 0.99 - 0.13 * k2, Nz.N(q / 0.5 + 3.3)) * step(0.5, Damage);
    base = lerp(base, float3(0.107, 0.069, 0.045), chip * 0.85);
    base = lerp(base, float3(0.016, 0.012, 0.010), sootD);
    Rough = lerp(Rough, 0.92, chip);
    Metal *= 1.0 - chip;
}
// a carcaça (13/10; dado de primitiva 6: 1 = veículo, 2 = prédio): a receita do Look medida na carcaça da
// street_battle, na volta 4 dele (look_lab.py, BURNT_CODE; o alvo dá p10/p50/p90 ≈ 0,03/0,33/0,60 na tela).
// Não é preta: marrom-carvão com variação forte. Tinta queimada 78,70,64 sRGB; fuligem 34,30,27 embaixo
// (até 0,6 m) e em manchas em ~25% do corpo, e no jogo também em volta do anel da torre, por onde a chama
// saiu; ferrugem de calor 104,70,48 em ~15%, mais nas quinas; cinza 165,155,142 em ~20% do teto.
// (13/10, no jogo: com limiar duro num ruído liso as placas viravam camuflagem urbana, branco, ferrugem e preto em
// recortes; aqui as mesmas cores em rampas largas, mosqueadas por uma oitava fina de 0,18 m, e a cinza só um pó
// raro no teto. Preview fora da placa em scratchpad/burnt, na mediana 0,33 do alvo.)
// As manchas vêm do ruído procedural (FRTSNoise, lá em cima), na escala da receita: 1,5 m e 0,8 m.
if (Burnt > 0.5)
{
    float3 Lp = LP * 0.01;
    float3 B0 = LMin * 0.01;
    float3 B1 = LMax * 0.01;
    float zr = Lp.z - B0.z;
    float ring = length(Lp.xy - float2(0.10, 0.0));
    float nA = Nz.N(q / 1.1);
    float nB = Nz.N(q / 0.6 + 5.3);
    float nF = Nz.N(q / 0.18 + 41.0);
    float edge = min(min(B1.x - Lp.x, Lp.x - B0.x), min(B1.y - Lp.y, Lp.y - B0.y));
    bool bVehicle = Burnt < 1.5;
    // a fuligem pela forma: no veículo embaixo até 0,6 m e na boca do anel; no prédio sobe até ~2,5 m
    float sootG = bVehicle ? max(1.0 - smoothstep(0.2, 0.6, zr), (1.0 - smoothstep(0.6, 1.3, ring)) * smoothstep(0.9, 1.4, zr))
                           : 1.0 - smoothstep(0.5, 2.5, zr);
    float soot = saturate(max(sootG * 0.85, (1.0 - smoothstep(0.0, 0.55, nA)) * 0.75));
    float rust = smoothstep(0.45, 1.0, nA + 0.08 * (1.0 - smoothstep(0.0, 0.6, edge))) * 0.6;
    float ash = smoothstep(0.6, 0.9, n.z) * smoothstep(0.8, 1.0, nB) * (1.0 - soot) * 0.25;
    base = float3(0.076, 0.061, 0.051);
    base = lerp(base, float3(0.140, 0.062, 0.030), rust);
    base = lerp(base, float3(0.016, 0.013, 0.011), soot);
    base = lerp(base, float3(0.30, 0.27, 0.23), ash);
    base *= 0.75 + 0.5 * nF;
    Rough = 0.93;
    Metal = 0.0;
    emis = 0.0;
    // o miolo quente: só no buraco do anel da torre (a torre voou), nas faces de cima e de dentro, em fresta
    // (ruído de ~0,5 m com limiar), no corpo negro de 1300 K esfriando a 900 K. Pico ~3000 nits (com o ev100
    // 14,3 do jogo lê laranja, não branco); I(t) = pico × calor² (o calor, o dado 7, cai de 1 a 0 em 30 s),
    // com a tremida de ruído; depois, 2–3 pontinhos a 5% (o dado 8, mais ~20 s)
    if (bVehicle)
    {
        float core = (1.0 - smoothstep(0.25, 1.1, ring)) * smoothstep(1.1, 1.5, zr) * saturate(n.z + 0.4);
        // (a fresta no ruído procedural de 0,5 m: o asfalto a 0,25 m virava a média na distância do jogo e o
        // miolo nunca acendia)
        float nC = Nz.N(q / 0.5 + 23.1);
        float crack = smoothstep(0.55, 0.75, nC);
        float flick = lerp(0.7, 1.0, Nz.N(float3(Time * 1.2, Obj.xy * 0.0007)));
        float3 bb = lerp(float3(1.0, 0.15, 0.02), float3(1.0, 0.35, 0.08), saturate(Heat));
        emis += bb * 3000.0 * Heat * Heat * core * crack * flick;
        emis += float3(1.0, 0.18, 0.03) * 150.0 * Embers * core * smoothstep(0.86, 0.9, nC);
    }
}
Emis = emis;
float2 nX = ConcN.Sample(ConcNSampler, q.yz / T).rg * 2.0 - 1.0;
float2 nY = ConcN.Sample(ConcNSampler, q.xz / T + 0.31).rg * 2.0 - 1.0;
float2 nZ = ConcN.Sample(ConcNSampler, q.xy / T + 0.57).rg * 2.0 - 1.0;
Nrm = normalize(n + ConcreteMask * 0.6 * (w.x * float3(0.0, nX.x, nX.y) + w.y * float3(nY.x, 0.0, nY.y) + w.z * float3(nZ.x, nZ.y, 0.0)));
return min(base, 0.9);
"""

SCALARS = [('Roughness', 0.6), ('Metallic', 0.0), ('PlayerMask', 0.0), ('ConcreteMask', 0.0), ('Stripes', 0.0),
           ('Grime', 0.0), ('EmissiveNits', 0.0), ('Blink', 0.0), ('Weld', 1.0), ('ToneVariation', 0.0)]
VECTORS = [('BaseColor', (0.5, 0.5, 0.5)), ('EmissiveColor', (0.0, 0.0, 0.0))]


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def fresh(name, folder, cls, factory):
    path = f'{folder}/{name}'
    if eal.does_asset_exist(path):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.Material):
            mel.delete_all_material_expressions(a)
        return a
    return tools.create_asset(name, folder, cls, factory)


def master(name, build):
    m = fresh(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('tangent_space_normal', False)
    m.set_editor_property('used_with_nanite', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED if build else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', build)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('output_type', F3)
    custom.set_editor_property('description', 'Structure')
    inputs = ['WP', 'N', 'Obj', 'LP', 'LMin', 'LMax', 'Conc', 'ConcN', 'Stain', 'Time', 'Face', 'PlayerColor', 'Power', 'Cut', 'Building',
              'Burnt', 'Heat', 'Embers', 'Damage', 'IRand', 'ILot'] + \
             [n for n, _ in SCALARS] + [n for n, _ in VECTORS]
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Rough', F1), ('Metal', F1), ('Emis', F3), ('Mask', F1), ('Nrm', F3)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', n)
        co.set_editor_property('output_type', t)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)

    y = -1200
    def node(cls, pin):
        nonlocal y
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        if pin:
            assert mel.connect_material_expressions(e, '', custom, pin), pin
        return e
    wp = node(unreal.MaterialExpressionWorldPosition, 'WP')
    node(unreal.MaterialExpressionVertexNormalWS, 'N')
    node(unreal.MaterialExpressionObjectPositionWS, 'Obj')
    node(unreal.MaterialExpressionPerInstanceRandom, 'IRand')   # (o tom de cada casa da vila, que são instâncias)
    # o tom do lote (13/10): o dado 0 da instância, que o jogo grava pelo lote do village.json (casa e muros iguais); 0 = sem
    lot = node(unreal.MaterialExpressionPerInstanceCustomData, 'ILot')
    lot.set_editor_property('data_index', 0)
    # a carcaça: a posição no espaço da peça e a caixa dela (a fuligem embaixo, a ferrugem perto das quinas)
    lp = node(unreal.MaterialExpressionTransformPosition, 'LP')
    lp.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    assert mel.connect_material_expressions(wp, '', lp, ''), 'LP in'
    lb = node(unreal.MaterialExpressionObjectLocalBounds, None)
    assert mel.connect_material_expressions(lb, 'Min', custom, 'LMin'), 'LMin'
    assert mel.connect_material_expressions(lb, 'Max', custom, 'LMax'), 'LMax' 
    node(unreal.MaterialExpressionTime, 'Time')
    node(unreal.MaterialExpressionTwoSidedSign, 'Face')
    for pin, path, fallback in (('Conc', CONCRETE, NOISE), ('ConcN', CONCRETE_N, FLAT_N), ('Stain', STAIN, NOISE)):
        tex = load(path) or load(fallback)
        e = node(unreal.MaterialExpressionTextureObject, None)
        e.set_editor_property('texture', tex)
        if pin == 'ConcN':
            e.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
        log(name, pin, tex.get_path_name() if tex else None)
    pc = node(unreal.MaterialExpressionVectorParameter, 'PlayerColor')
    pc.set_editor_property('parameter_name', 'PlayerColor')
    pc.set_editor_property('use_custom_primitive_data', True)
    pc.set_editor_property('primitive_data_index', 0)
    pc.set_editor_property('default_value', unreal.LinearColor(0.10, 0.45, 1.0, 1.0))
    for pin, index, default in (('Power', 4, 1.0), ('Cut', 5, 1000.0), ('Burnt', 6, 0.0), ('Heat', 7, 0.0), ('Embers', 8, 0.0), ('Damage', 9, 0.0)):
        s = node(unreal.MaterialExpressionScalarParameter, pin)
        s.set_editor_property('parameter_name', pin)
        s.set_editor_property('use_custom_primitive_data', True)
        s.set_editor_property('primitive_data_index', index)
        s.set_editor_property('default_value', default)
    b = node(unreal.MaterialExpressionConstant, 'Building')
    b.set_editor_property('r', 1.0 if build else 0.0)
    for n, default in SCALARS:
        s = node(unreal.MaterialExpressionScalarParameter, n)
        s.set_editor_property('parameter_name', n)
        s.set_editor_property('default_value', default)
    for n, default in VECTORS:
        v = node(unreal.MaterialExpressionVectorParameter, n)
        v.set_editor_property('parameter_name', n)
        v.set_editor_property('default_value', unreal.LinearColor(*default, 1.0))
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_BASE_COLOR),
          mel.connect_material_property(custom, 'Rough', P.MP_ROUGHNESS),
          mel.connect_material_property(custom, 'Metal', P.MP_METALLIC),
          mel.connect_material_property(custom, 'Emis', P.MP_EMISSIVE_COLOR),
          mel.connect_material_property(custom, 'Nrm', P.MP_NORMAL)]
    if build:
        ok.append(mel.connect_material_property(custom, 'Mask', P.MP_OPACITY_MASK))
    if not all(ok):
        raise RuntimeError(f'{name}: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material', name)
    return m


GHOST_CODE = r"""
float3 n = normalize(N);
float3 v = normalize(V);
float f = pow(1.0 - saturate(abs(dot(n, v))), 3.0);
// as linhas do holograma sobem devagar (uma a cada 0,6 m), e uma faixa clara varre de baixo pra cima
float z = (WP.z - Obj.z) * 0.01;
float scan = frac(z / 0.6 - Time * 0.5) < 0.1 ? 1.0 : 0.0;
float sweep = saturate(1.0 - abs(frac(Time * 0.35) * 14.0 - z) / 0.8);
Opa = saturate(0.10 + 0.55 * f + 0.15 * scan + 0.25 * sweep);
return Color.rgb * Nits * (0.22 + 1.5 * f + 0.45 * scan + 0.8 * sweep);
"""


def ghost_material():
    """O fantasma do prédio na hora de posicionar: o modelo como holograma (verde cabe, vermelho
    não; a cor vem pelo dado de primitiva 0–3). O ZH mostra o modelo transparente com a cor;
    aqui a borda acende (fresnel) e as linhas sobem, pra ler de cima por cima do chão claro."""
    m = fresh('M_StructureGhost', MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided', False)
    custom = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', GHOST_CODE)
    custom.set_editor_property('output_type', F3)
    ins = []
    for n in ('WP', 'N', 'V', 'Obj', 'Time', 'Color', 'Nits'):
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    co = unreal.CustomOutput()
    co.set_editor_property('output_name', 'Opa')
    co.set_editor_property('output_type', F1)
    custom.set_editor_property('additional_outputs', [co])
    y = -600
    for pin, cls in (('WP', unreal.MaterialExpressionWorldPosition), ('N', unreal.MaterialExpressionVertexNormalWS),
                     ('V', unreal.MaterialExpressionCameraVectorWS), ('Obj', unreal.MaterialExpressionObjectPositionWS),
                     ('Time', unreal.MaterialExpressionTime)):
        y += 110
        e = mel.create_material_expression(m, cls, -800, y)
        assert mel.connect_material_expressions(e, '', custom, pin), pin
    col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, y + 110)
    col.set_editor_property('parameter_name', 'GhostColor')
    col.set_editor_property('use_custom_primitive_data', True)
    col.set_editor_property('primitive_data_index', 0)
    col.set_editor_property('default_value', unreal.LinearColor(0.25, 1.0, 0.45, 1.0))
    assert mel.connect_material_expressions(col, '', custom, 'Color')
    nits = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, y + 220)
    nits.set_editor_property('parameter_name', 'Nits')
    nits.set_editor_property('default_value', 6000.0)
    assert mel.connect_material_expressions(nits, '', custom, 'Nits')
    P = unreal.MaterialProperty
    ok = [mel.connect_material_property(custom, '', P.MP_EMISSIVE_COLOR), mel.connect_material_property(custom, 'Opa', P.MP_OPACITY)]
    if not all(ok):
        raise RuntimeError(f'M_StructureGhost: ligações falharam {ok}')
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log('material M_StructureGhost')


def instances(parent, suffix):
    out = {}
    for slot, (color, rough, metal, extra) in SLOTS.items():
        name = f'MI_{slot}{suffix}'
        mi = fresh(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', parent)
        mel.clear_all_material_instance_parameters(mi)
        mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*color, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
        mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
        for k, v in extra.items():
            if isinstance(v, tuple):
                mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
            else:
                mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        out[slot] = mi
    return out


def import_glb(path, dest, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
    return list(task.get_editor_property('imported_object_paths') or [])


def import_meshes(files, dest_of, slot_of, opaque):
    for f in files:
        stem = os.path.splitext(os.path.basename(f))[0]
        paths = import_glb(f, dest_of(f), stem)
        assets = [unreal.load_asset(p) for p in paths]
        for a in assets:
            if not isinstance(a, unreal.StaticMesh):
                continue
            s = a.get_editor_property('nanite_settings')
            s.set_editor_property('enabled', True)
            a.set_editor_property('nanite_settings', s)
            # set_material, um por um (trocar a lista inteira de uma vez não ficava gravado: a malha
            # seguiu apontando pros materiais do importador, que são apagados abaixo; teste base1, 10/10)
            mats = a.get_editor_property('static_materials')
            missing = []
            for k, sm in enumerate(mats):
                slot = str(sm.get_editor_property('material_slot_name'))
                key = slot_of(slot.split('.')[0].split('_')[0])
                if key in opaque:
                    a.set_material(k, opaque[key])
                else:
                    missing.append(slot)
            eal.save_loaded_asset(a)
            wrong = [str(m.get_editor_property('material_interface').get_path_name()) for m in a.get_editor_property('static_materials')
                     if not m.get_editor_property('material_interface') or not m.get_editor_property('material_interface').get_path_name().startswith(MATS)]
            if wrong:
                raise RuntimeError(f'{a.get_path_name()}: materiais fora de {MATS}: {wrong}')
            b = a.get_bounding_box()
            log('mesh', a.get_path_name(), 'slots', len(mats), 'sem material:', missing,
                'caixa (m) %.2f..%.2f %.2f..%.2f %.2f..%.2f' % (b.min.x / 100, b.max.x / 100, b.min.y / 100, b.max.y / 100,
                                                            b.min.z / 100, b.max.z / 100))
        # os materiais e texturas do importador: não servem (sem Nanite) e ninguém mais usa
        for a, p in zip(assets, paths):
            if isinstance(a, (unreal.MaterialInterface, unreal.Texture)):
                eal.delete_asset(p.split('.')[0])


def existing_instances():
    """Os MI_<slot> que já existem em /Game/Structures/Materials, sem regravar nada (o jogo aberto segura o
    M_Structure.uasset; o modo 'só o Dozer' não precisa mexer nos materiais)."""
    out = {}
    for slot in SLOTS:
        a = unreal.load_asset(f'{MATS}/MI_{slot}')
        if a:
            out[slot] = a
    if not out:
        raise RuntimeError(f'sem materiais em {MATS}: rode game.ps1 models uma vez antes')
    return out


def main():
    if os.environ.get('PROV_TRUCK_ONLY'):
        opaque = existing_instances()
        files = sorted(glob.glob(os.path.join(TRUCK_SRC, 'SM_USA_Supply*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_USA_Supply*.glb em {TRUCK_SRC}: rode o Blender antes (model_usa_supply_truck.py)')
        import_meshes(files, lambda f: TRUCK_ROOT, lambda k: DOZER_SLOTS.get(k, k), opaque)
        log('done (o caminhão)')
        return
    if os.environ.get('PROV_DOZER_ONLY'):
        opaque = existing_instances()
        files = sorted(glob.glob(os.path.join(DOZER_SRC, 'SM_USA_Dozer_*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_USA_Dozer_*.glb em {DOZER_SRC}: rode o Blender antes (model_usa_dozer.py)')
        import_meshes(files, lambda f: DOZER_ROOT, lambda k: DOZER_SLOTS.get(k, k), opaque)
        log('done (o Dozer)')
        return
    opaque = instances(master('M_Structure', False), '')
    instances(master('M_StructureBuild', True), '_Build')
    ghost_material()
    if os.environ.get('PROV_MATERIALS_ONLY'):
        log('done (só os materiais)')
        return
    if os.environ.get('PROV_CIVIL_ONLY'):
        files = sorted(glob.glob(os.path.join(CIVIL_SRC, 'SM_civ_*.glb')))
        if not files:
            raise RuntimeError(f'nenhum SM_civ_*.glb em {CIVIL_SRC}: o kit civil vem da sessão do Look')
        # (13/10) as miudezas da maquete do Look (caixa d'água, ar-condicionado, toldo, entulho...): o village.json v3 do
        # WorldForge põe como peças sem bloqueio; os mesmos slots do kit civil (captures/look/kit_vila/manifest.json)
        files += sorted(glob.glob(os.path.join(TANK_SRC, 'SM_LL_Prop_*.glb')))
        import_meshes(files, lambda f: CIVIL_ROOT, lambda k: CIVIL_SLOTS.get(k, k), opaque)
        log('done (a vila)')
        return
    if os.environ.get('PROV_FOLDER'):
        # só uma pasta (game.ps1 debris: os destroços), sem reimportar os prédios
        files = sorted(glob.glob(os.path.join(SRC, os.environ['PROV_FOLDER'], '*.glb')))
        if not files:
            raise RuntimeError(f'nenhum GLB em {SRC}/{os.environ["PROV_FOLDER"]}')
        import_meshes(files, lambda f: f'{ROOT}/{os.path.basename(os.path.dirname(f))}', lambda k: k, opaque)
        log('done (só', os.environ['PROV_FOLDER'] + ')')
        return
    if os.environ.get('PROV_TANK_ONLY') is None:
        files = sorted(glob.glob(os.path.join(SRC, '*', '*.glb')))
        if not files:
            raise RuntimeError(f'nenhum GLB em {SRC}: rode o Blender antes (game.ps1 models)')
        import_meshes(files, lambda f: f'{ROOT}/{os.path.basename(os.path.dirname(f))}', lambda k: k, opaque)
    # o tanque da maquete do Look (pedido do Felipe, 11/10): só lê os GLBs dela (look.ps1 tank)
    tank = [os.path.join(TANK_SRC, f'SM_LL_USATank_{p}.glb') for p in TANK_PARTS]
    if all(os.path.exists(f) for f in tank):
        import_meshes(tank, lambda f: TANK_ROOT, lambda k: TANK_SLOTS.get(k, k), opaque)
    else:
        log('sem o tanque do Look em', TANK_SRC, '(look.ps1 tank): fica o de antes')
    log('done')


main()
