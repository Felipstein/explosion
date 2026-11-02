#pragma once

// Dados de design lidos dos JSON em Game/Config/RTS (relidos a cada partida, pra
// ajustar sem recompilar): as unidades viram Sim::Ruleset; o desenho de cada
// unidade e as informações do mapa ficam aqui do lado da Unreal.

#include "CoreMinimal.h"
#include "Sim/Defs.h"

class FJsonObject;

/** Peça que gira sozinha (ventilador, antena de radar): a origem da malha é o eixo (Z). */
struct FRTSSpinPart
{
	FString Mesh;
	FVector AtCm = FVector::ZeroVector;
	float Rpm = 30.0f;
	/** (v5, 24/10) O rotor da aeronave ("rotor": true): gira pelo motor da simulação (UnitView::Rotor, 0..1), não pela energia; o eixo
	 *  ("axis": "x", "y" ou "z"; o rotor de cauda gira no Y da peça). */
	bool bRotor = false;
	int32 Axis = 2;
	/** (24/10, escopo B do ar) O disco borrado do rotor no giro cheio: as pás ("blades"; 0 = sem disco) e o raio da ponta ("radius_m"). */
	int32 Blades = 0;
	float RadiusCm = 0.0f;
	/** A faixa pintada da ponta das pás (m; "tip_m") e a corda (m; "chord_m"): o anel claro e a cobertura média do disco. */
	float TipCm = 0.0f;
	float ChordCm = 30.0f;
	/** (25/10, v5) A peça gira na torre ("on_turret": o prato do radar do Linebacker, no mastro da torre): o pivô vai com o giro da torre.
	 *  No veículo de chão sem lâmina, a peça gira sempre no rpm dela (o radar de busca). */
	bool bOnTurret = false;
};

struct FRTSUnitVisual
{
	FString Key;
	FString DisplayName;
	/** Malhas que andam com o casco (casco, esteiras, rodas). */
	TArray<FString> HullMeshes;
	/** Malhas que giram com a torre (torre, cano). */
	TArray<FString> TurretMeshes;
	float MuzzleHeightM = 2.0f;
	/** (30/10) A boca da arma à frente do eixo da torre (m; 0 = a ponta da malha "Barrel"): o fuzil do soldado. */
	float MuzzleForwardM = 0.0f;
	/** (30/10, simulação 26) O soldado ("infantry" na unidade): as pernas que balançam no quadril ("legs": as malhas, o quadril
	 *  com o y de cada lado e o ciclo de passo em m), o corpo inteiro que gira pra mira (a torre) e a morte do ZH (cai, fica, some). */
	bool bInfantry = false;
	FString LegMeshes[2];
	FVector LegHipCm = FVector::ZeroVector;
	float StrideCm = 360.0f;
	/** (30/10, a infantaria nova; docs/ESTUDO_INFANTARIA_VISUAL.md §8) O soldado de verdade ("soldier" no visual): o corpo esqueletal com os
	 *  clipes de fuzil do modelo da Unreal 5.8 (RTSSoldierAnim), a escala, o fuzil na mão, a cor da farda e a da facção (capacete e
	 *  colete, como nos game shots). Vazio = o provisório de caixas. */
	/** (30/10, o Felipe: "a bandeira da Technical tá pequena, mal feita e não tem simulação do vento") A bandeira no topo do mastro ("flag" no
	 *  visual): o ponto do mastro (m, no quadro da malha já na escala) e o tamanho do pano (m). O pano vira pro vento aparente (o do mapa
	 *  menos o andar do veículo), ondula com ele e cai quando ele some (RTSUnitActor::StepFlag). */
	bool bFlag = false;
	FVector FlagMastCm = FVector::ZeroVector;   // (cm: o "mast_m" do json vem em m)
	FVector2D FlagSizeM = FVector2D(1.0, 0.65);
	/** (01/11, o Felipe: "tudo que tem bandeira tem que ter física [...] hoje nosso Technical é o único com bandeira com física que balança
	 *  no vento que o mapa oferece") as outras bandeiras do modelo ("flags": [{"mast_m", "size_m", "turret"}...]: o model_run_detail.py tira
	 *  o pano da malha e anota o mastro de cada uma), com a mesma física da "flag" */
	TArray<FVector> ExtraFlagMastCm;
	TArray<FVector2D> ExtraFlagSizeM;
	TArray<bool> ExtraFlagTurret;
	TArray<int32> ExtraFlagEmblem;   // (0 nenhum, 1 o círculo da GLA, 2 a estrela da China: o M_FlagCloth desenha no pano)
	FString SoldierMesh, SoldierRifle;
	float SoldierScale = 1.25f;
	FLinearColor SoldierCloth = FLinearColor(0.32f, 0.29f, 0.22f), SoldierTeam = FLinearColor(0.12f, 0.2f, 0.45f);
	/** (30/10, o soldado vestido: Tools/soldier_dress.py e import_soldier.py) a farda pintada de cada material do corpo ("paint") e as peças
	 *  rígidas presas nos ossos ("pieces": o colete, o capacete, a mochila, as bolsas), com a origem de cada uma no espaço do corpo (cm).
	 *  Vazio = o corpo do modelo com a tinta e o capacete de esfera. */
	struct FSoldierPiece
	{
		FString Mesh;
		FName Bone;
		FVector OriginCm = FVector::ZeroVector;
	};
	TArray<FString> SoldierPaint;
	TArray<FSoldierPiece> SoldierPieces;
	/** (30/10, item 37) O míssil que se vê ("missile_model": a malha de Tools/missile_models.py, a ponta na origem, e o comprimento em m: a
	 *  chama e a fumaça saem da tubeira). Vazio = só a chama, como antes. */
	FString MissileMesh;
	float MissileLengthM = 0.0f;
	/** o rastro do motor ("trail_width", "trail_density" × o do Patriot; "trail_s": só nos primeiros segundos, o reforçador; 0 = o voo todo) */
	float MissileTrailWidthK = 1.0f, MissileTrailDensityK = 1.0f, MissileTrailS = 0.0f;
	/** O eixo da torre no casco (m, x e y): as malhas vêm com a origem do tanque, a torre gira aqui. */
	FVector2D TurretPivotM = FVector2D::ZeroVector; float TurretYawOffsetDeg = 0.0f;   // (16/10) a frente da malha da torre fora do +X (a caixa da Patriot: 180)
	/** (v4, 23/10) O corpo da torre que voa na morte, no quadro das malhas (m): a caixa do casco fundido, sem o cano (vem da malha
	 *  "Barrel"), o mastro, a estação de armas e as antenas, que quebram ("turret_box_m": [[x, y, z mín], [x, y, z máx]]). Sem o
	 *  campo, a caixa das malhas da torre sem o cano (com o mastro, de ponta-cabeça ela ficaria no ar sobre ele). */
	FBox TurretBoxM = FBox(EForceInit::ForceInit);
	/** Lagarta (m): a bitola (centro a centro) e a largura de cada uma; zero = sem lagarta (não risca o chão). */
	FVector2D TracksM = FVector2D::ZeroVector;
	/** (01/11, a esteira que anda) O passo do elo da esteira (m, "track_step_m": a volta ÷ os elos, do Tools/model_run_detail.py); zero =
	 *  a esteira parada (o modelo sem a tangente do elo no UV2/UV3, como o tanque aprovado da USA). */
	float TrackStepM = 0.0f;
	/** (01/11, o Felipe: "as construções vão poder manipular o chão [...] se uma construção dessa é posta e a construção é finalizada,
	 *  ela recorta parte do chão pra ficar funcional, inclusive outras serão assim") os retângulos do chão do mapa que o prédio pronto
	 *  recorta ("ground_cut_m": [[x0, y0, x1, y1]...], no quadro do prédio; cm aqui): o M_SliceGround abre o buraco lendo o
	 *  MPC_GroundCut (Tools/ground_cut.py). E o caminho do veículo que nasce dele subindo do túnel ("tunnel_exit": {"path_m": [[x, z]...],
	 *  "speed_mps"}: x pra frente no eixo do prédio, z abaixo do chão; o último ponto liga na saída da simulação). */
	TArray<FVector4> GroundCutsCm;
	TArray<FVector2D> TunnelPathCm;
	float TunnelSpeedCm = 400.0f;
	/** (31/10, o teste de leitura) A tinta só desta unidade ("paint_tint": a cor base linear), por cima da compartilhada (o TankPaint dos
	 *  veículos da USA); A ≤ 0 = sem (a de todos). */
	FLinearColor PaintTint = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/** (simulação 35) O teto do transporte nas escotilhas ("hatch_z_m", cm acima do chão): a cintura do soldado que atira de dentro. */
	float HatchZCm = 0.0f;
	/** (25/10, v5) Onde a lagarta toca o chão (m, x de trás e x da frente; "track_contact_m"): a areia sai daí (sem o campo, os pontos do
	 *  tanque aprovado, −2,40 e 2,55). */
	FVector2D TrackContactM = FVector2D::ZeroVector;
	/** (25/10, v5) As células de lançamento vertical no casco ("vls_m": a boca de cada uma, m no quadro das malhas): o míssil desenhado
	 *  sai da célula da vez, pra cima, e emenda no voo da simulação (o Linebacker). */
	TArray<FVector> VlsCm;
	/** (25/10, v5) A arma leve de tiro rápido ("light_gun": o canhão antiaéreo, a metralhadora): o clarão e o traçante pequenos do jogo, sem
	 *  o sopro de canhão de tanque, sem o recuo da torre, sem a marca no chão; no alvo, as faíscas (no ar, o estouro do antiaéreo). */
	bool bLightGun = false;
	/** (01/11) O lançador de caixa fechada ("launch_canister": a Patriot): a saída com o clarão, as tampas, o jato de ré e a nuvem no chão.
	 *  Sem ela, o lançador aberto (o casulo, o tubo): a ignição pequena na tubeira. */
	bool bLaunchCanister = false;
	/** O tamanho do clarão de boca da arma leve (cm; 0 = 30) e o comprimento do risco do traçante (cm; 0 = 260). */
	float LightGunFlashCm = 0.0f;
	float LightGunTracerCm = 0.0f;
	/** (30/10) A dispersão da rajada no chão (cm; o semieixo ao longo do tiro, a elipse do docs/ESTUDO_IMPACTOS.md A; 0 = 250). */
	float LightGunSpreadCm = 0.0f;
	/** A cratera do tiro dela (13/10): 0 = A, 1 = B, 2 = C, −1 = nenhuma; e a parte dos tiros que passa raspando
	 *  e cava o chão além do alvo (só desenho, pelo hash do evento). */
	int32 ImpactCrater = -1;
	float NearMissShare = 0.0f;
	/** (v4, 23/10) A munição de ogiva grande ("impact_blast": "classic", o míssil do Cruise Missile Strike): no acerto, além do
	 *  clarão do míssil, a explosão clássica em volume (a dos prédios, sem destroços). */
	bool bHeavyImpact = false;
	/** (23/10) A carga da ogiva do míssil desta arma, em kg de TNT ("warhead_kg", só desenho: a bola, a fuligem e o anel de areia do
	 *  acerto pela escala de Hopkinson-Cranz a partir da Carga Pesada, UBlastFxDirector::MissileHit); 0 = sem explosão em volume. */
	float WarheadKg = 0.0f;
	/** (30/10, item 38) A carga da ogiva no acerto no ar ("air_warhead_kg"; 0 = a WarheadKg): a Patriot de estilhaço estoura maior no ar. */
	float AirWarheadKg = 0.0f;
	/** (23/10, a revisão) O visual da ogiva ("warhead_look": "classic"): a fuligem, o sol, o céu e o chão da explosão clássica (a
	 *  metade leve do Cruise Missile Strike, ao lado da pesada clássica), sem o anel de areia; o padrão é o da morte de tanque. */
	bool bWarheadClassicLook = false;
	/**
	 * (23/10, a explosão com vida; BlastFxHitLife.h) A munição ("hit_life", só desenho): a inclinação do leque dos estilhaços pra frente
	 * (fragment_tilt_deg, Gurney: atan(Vm/Vf); a Patriot 34°, o Tomahawk 8°), o peso do que o míssil traz na direção da explosão (carry,
	 * w: 0,8 a Patriot, 0,5 os subsônicos), a carga que joga a terra no chão (ejecta_kg; 0 = a da ogiva) e a cratera do míssil no chão com a
	 * vida (crater: a letra do atlas; mark_m: o raio da marca, 4,2 = a de sempre; crater_depth: a profundidade, 0,7 = a de sempre). Sem o
	 * bloco, os padrões; com -HitLifeV=1 nada disto vale.
	 */
	float FragmentTiltDeg = 20.0f;
	float HitCarry = 0.5f;
	float EjectaKg = 0.0f;
	/** (23/10, a areia do acerto) A carga que joga a areia no chão ("sand_kg" no hit_life; 0 = a da ogiva): o ataque de mísseis, 150 (o
	 *  Tomahawk de verdade no chão, a escala da gravação strike_chao; RTSHitLife.cpp: o véu, a onda de base e a poeira que fica). */
	float SandKg = 0.0f;
	int32 LifeCrater = -1;
	float LifeMarkM = 4.2f;
	float LifeCraterDepth = 0.7f;
	/** (23/10, a vida) A massa do veículo (t; "mass_t" no visual): o tranco do acerto ∝ W/M (60 t, o tanque, quando falta). */
	float MassT = 0.0f;
	/** (30/10, o Felipe: a morte da Technical, do Stryker e do Linebacker "extremamente forte, parecendo um tanque de vdd") O tamanho da
	 *  morte ("death_scale" no visual; 1 = o tanque de 60 t, a aprovada): a bola, o anel, os destroços, o fogo da carcaça, o empurrão na
	 *  poeira e o tremor. docs/ESTUDO_IMPACTOS.md D: a bola pela raiz cúbica da massa que queima. */
	float DeathScale = 1.0f;
	/** (30/10, a assinatura de cada morte; docs/ESTUDO_IMPACTOS.md D e J) "death_kind" no visual: 0 = o tanque (a de sempre), 1 = a gasolina
	 *  (a picape: a bola mais amarela e clara), 2 = o diesel (o Stryker: a fumaça preta), 3 = a munição (o Linebacker: os estouros e o míssil torto). */
	uint8 DeathKind = 0;
	/** (30/10, o jato do EmberGen: FXLab/EmberGen/m1_morte_jato_v01.ember) A gravação do jato de fogo que sai pelo anel da torre depois que
	 *  ela voa ("death_jet": {"bake", "scale", "base_m"}): a munição queimando no carrossel do tanque sem painel de alívio (o T-55/T-72; o
	 *  M1 tem os painéis e fica com o fogo da carcaça). base_m = a altura da boca do jato dentro da gravação. Vazio = sem jato. */
	FName DeathJetBake;
	float DeathJetScale = 1.0f;
	float DeathJetBaseM = 0.0f;
	/**
	 * (23/10, v4) As rodas ("wheels", só desenho; o caminhão de suprimento e o Tomahawk): a largura do pneu (m; zero = sem a poeira e sem
	 * a marca das rodas) e a física delas contra a lagarta do tanque aprovado. A poeira (Plugins/RTSTreadDust/INTEGRACAO.md, "As rodas"):
	 * o lado do puff (dust_scale), a massa ÷ o lado² (dust_amount) e o espaçamento (dust_spacing). A marca no chão (RTSGroundMemory): o escuro da passada (mark_dark), o
	 * fundo do sulco e da bordinha (mark_depth) e as garras por metro (mark_lug), × os da lagarta. Os eixos, o raio e a bitola saem das
	 * malhas wheel_* (a arte final traz as dela: acompanha sozinho).
	 */
	float WheelTyreWidthM = 0.0f;
	float WheelDustScale = 1.0f;
	float WheelDustAmount = 1.0f;
	float WheelDustSpacing = 1.0f;
	/** (23/10) O rastro da poeira da roda no tempo (s; "dust_trail_s"; zero = só em comprimentos do veículo): o comprimento dele vira
	 *  max(o do veículo, a velocidade × isto). */
	float WheelDustTrailS = 0.0f;
	float WheelMarkDark = 1.0f;
	float WheelMarkDepth = 1.0f;
	float WheelMarkLug = 1.0f;
	/** (23/10) O trecho da marca do pneu nas retas (m; "mark_segment_m"; o da lagarta é 1 m). */
	float WheelMarkSegmentM = 1.0f;
	/**
	 * (23/10, v4) O lançador que levanta ("launcher", só desenho; o Tomahawk): a malha que levanta (o nome de uma das "turret"; vazio = a
	 * torre só gira), a dobradiça (cm no quadro das malhas; o eixo é o Y), o ângulo de tiro, a hidráulica (a velocidade e a aceleração de
	 * subir, a freada do fim do curso, a velocidade de descer e a espera depois do último alvo), o batente (a mola do berço no cilindro: Hz
	 * e amortecimento; a passada de ~1,4° no fim) e as bocas dos tubos deitados (cm no quadro das malhas; o míssil sai da levantada).
	 * LauncherNoseMeshes ("noses"): os nomes das malhas da torre que são as pontas dos mísseis na boca dos tubos (uma por tubo; levantam
	 * com o berço e a do tubo que atirou some até a recarga da rajada, o ProjectileBoneFeedback do ZH; vazio = sem as pontas à parte).
	 */
	FString LauncherMesh;
	TArray<FString> LauncherNoseMeshes;
	FVector LauncherHingeCm = FVector::ZeroVector;
	float LauncherFirePitchDeg = 45.0f;
	float LauncherRaiseDps = 60.0f;
	float LauncherAccelDps2 = 180.0f;
	float LauncherBrakeDps2 = 360.0f;
	float LauncherLowerDps = 30.0f;
	float LauncherLowerDelayS = 2.0f;
	float LauncherStopHz = 2.5f;
	float LauncherStopZeta = 0.35f;
	TArray<FVector> LauncherTubesCm;
	/** (01/11) os cilindros de elevação em 3 estágios ("ram": cyl, mid, rod, as malhas da torre; base_m, o pino na mesa; tip_m, o olhal
	 *  do berço deitado): o jogo aponta as peças da base pro olhal a cada quadro e desliza o estágio do meio metade da extensão. */
	FString LauncherRamCyl, LauncherRamMid, LauncherRamRod;
	FVector LauncherRamBaseCm = FVector::ZeroVector;
	FVector LauncherRamTipCm = FVector::ZeroVector;
	/** (25/10, v5) O lançador é o berço de canhões ("guns": true, o Linebacker): os tubos são as bocas dos canos (o tiro alterna entre
	 *  eles), o míssil não sai deles. "aim_air": a elevação segue a altura da aeronave mirada (de rest_pitch_deg, a pose da malha, até
	 *  max_pitch_deg acima dela); sem a aeronave, fire_pitch_deg. */
	bool bLauncherGuns = false;
	bool bLauncherAimAir = false;
	float LauncherRestPitchDeg = 0.0f;
	float LauncherMaxPitchDeg = 75.0f;
	float RadiusM = 3.0f;
	/** Prédio: as peças que giram, e o andaime da obra (só aparece enquanto constrói). */
	TArray<FRTSSpinPart> SpinParts;
	FString ScaffoldMesh;
	/** Dozer: a lâmina, que sobe e desce quando ele trabalha (gira no Y do pivô). */
	FString BladeMesh;
	FVector BladeAtCm = FVector::ZeroVector;
	/** Caminhão: uma caixa em cada vaga; mostra quantas ele leva. */
	FString CargoMesh;
	TArray<FVector> CargoSlotsCm;
	/** (30/10) Prédio: os pontos de fogo do telhado (cm, no quadro da malha do casco), o "osso" de fogo do ZH; o Tools/roof_points.py
	 *  mede no modelo. Abaixo de 35% de vida o fogo simulado acende neles. */
	TArray<FVector> FirePointsCm;
	/** (31/10, os estados de dano do prédio: Tools/model_damage.py; docs/ESTUDO_DEGRADACAO_PREDIOS.md) as malhas do danificado (abaixo de
	 *  70% da vida), do muito danificado (35%) e da ruína (o prédio morto), e os pontos de fogo de cada estado (cm, no quadro do casco).
	 *  Vazio = como antes (a malha não troca; a morte deixa o entulho de pedras). */
	FString DamagedMesh, WreckedMesh, RuinMesh;
	/** (01/11) O veículo: a torre de cada estado ("turret_damaged", "turret_wrecked"; Tools/model_damage_vehicle.py). */
	FString TurretDamagedMesh, TurretWreckedMesh;
	TArray<FVector> FirePointsDamagedCm, FirePointsWreckedCm;
	/** (16/10) O diesel (docs/DIESEL_DO_TANQUE.md): as saídas do escape no quadro do casco (m) e pra onde o gás sai; sem
	 *  saída não solta fuligem (a turbina, o prédio). O motor: a potência, a massa, o rolamento na areia, a carga do
	 *  trabalho (o Dozer na obra), as marchas, o σ do jato e a velocidade do gás; a velocidade e o giro máximos da unidade. */
	TArray<FVector> ExhaustAtM;
	TArray<FVector> ExhaustDir;
	float ExhaustPowerKw = 0.0f;
	float ExhaustMassT = 0.0f;
	float ExhaustRoll = 0.12f;
	float ExhaustWorkLoad = 0.0f;
	int32 ExhaustGears = 4;
	float ExhaustJetSigmaM = 0.4f;
	float ExhaustExitMs = 25.0f;
	float ExhaustMaxSpeedMs = 8.0f;
	float ExhaustTurnDps = 90.0f;
	/** (v4, 22/10) A zona de lançamento: a altura (m acima do chão) em que o avião de carga passa ("airlift.altitude_m"). */
	float AirliftAltitudeM = 70.0f;
	/** (v4) O visual veio do "fallback" (a malha do modelo provisório ainda não existe no projeto). */
	bool bFallbackVisual = false;
	/** (v5, 24/10) A aeronave (o "layer": "air"; docs/AEREO.md §4.3, só desenho): o jato (inclina na curva pela física) ou o helicóptero
	 *  (abaixa o nariz acelerando); o trem de pouso ("gear": o nome da malha, que some no voo); os mísseis à vista ("ammo": os nomes das
	 *  malhas, um por tiro do pente da 1ª arma: o que saiu some até rearmar); a altura de cruzeiro (m). */
	bool bAir = false;
	bool bJet = false;
	FString GearMesh;
	TArray<FString> AmmoMeshes;
	/** (24/10, escopo B) De qual arma é o pente dos mísseis à vista ("ammo_weapon": 1 ou 2; o Apache mostra os Hellfires da 2ª). */
	int32 AmmoWeapon = 1;
	/** (24/10, escopo B) A rampa de trás que abre carregando e descarregando ("ramp": a malha com a origem na dobradiça, a dobradiça
	 *  em cm no quadro das malhas, o ângulo aberto (graus; + = a ponta desce) e o tempo de abrir). */
	FString RampMesh;
	FVector RampHingeCm = FVector::ZeroVector;
	float RampOpenDeg = 39.3f;
	float RampOpenS = 2.0f;
	/** (24/10, escopo B) O pós-combustor ("afterburner": as bocas dos bocais em cm no quadro das malhas, a chama sai pra −X; o raio da
	 *  boca, o comprimento da chama cheia e o brilho, × 60.000 cd/m²) e a luz dele (candelas no máximo). */
	TArray<FVector> AfterburnerCm;
	float AfterburnerRadiusCm = 30.0f;
	float AfterburnerLengthCm = 320.0f;
	float AfterburnerRadiance = 1.0f;
	float AfterburnerLightCd = 60000.0f;
	float AirCruiseAltM = 0.0f;
	/** (v5) O alcance de ar (m; o maior das armas que atiram no ar: o círculo do HUD) e o de chão. */
	float AirRangeM = 0.0f;
	float GroundRangeM = 0.0f;
};

/** (v4, 22/10) O desenho de um poder do general (Units.json "powers"): o nome e o ícone do HUD (icon:<nome>), na ordem de
 *  Rules.Powers. */
struct FRTSPowerVisual
{
	FString Key;
	FString DisplayName;
	FString Icon;
};

/** O desenho do depósito de suprimentos (o mesmo em todo mapa) e da pilha de caixas caídas. */
struct FRTSDockVisual
{
	FString Mesh;
	FString CrateMesh;
	/** Onde ficam as caixas do depósito, já na ordem em que somem (as de cima primeiro, espalhado). */
	TArray<FVector> CrateSlotsCm;
	/** A pilha que cai do caminhão destruído: caixa por caixa (posição e giro em graus no W). */
	TArray<FVector4> PileSlotsCm;
};

/** Uma ordem do cenário: no segundo AtS de simulação, o jogador manda todas as unidades dele. */
struct FRTSScenarioOrder
{
	float AtS = 0.0f;
	int32 Player = 0;
	/** "line" (o arraste com direção), "move" ou "attack_move"; (v12) "build": o Dozer ("unit") começa o prédio Key ("structure") com o
	 *  centro em to_m, virado facing_deg (de 90 em 90). */
	FString Type;
	FString Key;
	FVector2D ToM = FVector2D::ZeroVector;
	float FacingDeg = 0.0f;
	float WidthM = 0.0f;
	/** A cratera da ordem "scar": 0 = A, 1 = B, 2 = C. */
	int32 MarkSize = 1;
	/** O grupo em formação, no passo do mais lento (o F segurado). */
	bool bFormation = false;
	/** ≥ 0: a ordem vai só pra N-ésima unidade do jogador (na ordem da criação), não pro exército todo (14/10: a estrela
	 *  de 8 rumos da poeira, um tanque por ordem). Na "use_power" (v4), o índice do poder. */
	int32 Unit = -1;
};

/** Um prédio pronto num ponto fixo do cenário (14/10: a poeira rente à parede da War Factory, o pedido da Arte). */
struct FRTSScenarioStructure
{
	FString Key;
	int32 Player = 0;
	FVector2D AtM = FVector2D::ZeroVector;
};

/** (16/10) Uma unidade do cenário num ponto exato ("units"), virada pra FacingDeg: a cunha dos vídeos da poeira. */
struct FRTSScenarioUnit
{
	FString Key;
	int32 Player = 0;
	FVector2D AtM = FVector2D::ZeroVector;
	float FacingDeg = 0.0f;
};

/** Um ponto da câmera que anda no cenário (14/10: o céu na câmera deitada do Look, e as 3 câmeras da poeira). */
struct FRTSScenarioCameraKey
{
	float AtS = 0.0f;
	float YawDeg = 45.0f;
	float DistanceM = 100.4f;
	/** < 0 = a curva do zoom do jogo. */
	float PitchDeg = -1.0f;
	/** (16/10) > 0: a vista acima do horizonte (graus; até o MaxLookUpDeg da câmera), no lugar da inclinação. */
	float LookUpDeg = 0.0f;
};

/** O teste rápido (12/10): a partida pronta, com semente fixa, ordens em passos fixos e câmera parada. */
struct FRTSScenario
{
	FString Name;
	FString Map;
	int32 Seed = 1;
	/** "off", "black" ou "explored". */
	FString Fog = TEXT("off");
	/** (v4) > 0: o dinheiro inicial da cena ("money"), no lugar do escolhido no menu (× 5 com o -RTSFast). */
	int32 Money = 0;
	/** (v4, 22/10) A patente de largada de cada jogador ("start_rank": um número pra todos, ou a lista por jogador, 1..5);
	 *  vazio ou sem o jogador = 1. */
	TArray<int32> StartRanks;
	/** (v4, 23/10) Quem é a IA na cena ("ai": a lista por jogador, true/false); vazio = ninguém (o de sempre das cenas). */
	TArray<bool> AIPlayers;
	TArray<FVector2D> StartsM;
	TArray<TPair<FString, int32>> UnitsPerPlayer;
	TArray<FRTSScenarioOrder> Orders;
	/** Prédios prontos em pontos fixos ("structures"), além das unidades da largada. */
	TArray<FRTSScenarioStructure> Structures;
	/** As unidades em pontos exatos ("units"), depois das da largada (a ordem "unit" conta as da largada primeiro). */
	TArray<FRTSScenarioUnit> Units;
	FVector2D CameraFocusM = FVector2D::ZeroVector;
	float CameraDistanceM = 95.0f;
	float CameraYawDeg = 45.0f;
	/** A câmera desliza nesta velocidade (m/s) a partir do foco: o teste do cintilado em movimento (13/10). */
	FVector2D CameraPanMS = FVector2D::ZeroVector;
	/** A inclinação forçada (graus; < 0 = a curva do zoom do jogo): só pra foto lado a lado com a maquete do Look,
	 *  numa câmera que o jogador não tem (13/10). */
	float CameraPitchDeg = -1.0f;
	/** (16/10) > 0: a câmera do começo olhando acima do horizonte (graus), no lugar da inclinação. */
	float CameraLookUpDeg = 0.0f;
	/** A câmera que anda (camera.keys): interpolada pelo relógio da simulação, depois da câmera do começo. O sol fica
	 *  onde a câmera do começo o pôs, então girar por aqui muda o lado do sol na foto. */
	TArray<FRTSScenarioCameraKey> CameraKeys;
	/** (16/10) A câmera que segue a N-ésima unidade do jogador FollowPlayer (a mesma conta da ordem "unit"; < 0 =
	 *  nenhuma), com o giro, a distância e a inclinação da câmera do cenário; o foco alcança a unidade em FollowLagS. */
	int32 FollowUnit = -1;
	/** (16/10) Onde o chão da unidade seguida cai na tela (0,5 0,5 = o centro; a pose do gameplay_v3 da Arte). */
	FVector2D FollowScreen = FVector2D(0.5, 0.5);
	/** (16/10) O sol a SunRelDeg da frente da câmera da cena (0 = reto à frente, + = à direita), no lugar do sol do jogo. */
	bool bSunRel = false;
	float SunRelDeg = 0.0f;
	int32 FollowPlayer = 0;
	float FollowLagS = 0.3f;
	/** Falso: as fotos sem o HUD (a comparação com a maquete). */
	bool bHud = true;
	TArray<float> ShotsS;
	float EndS = 30.0f;
	/** (v5, 24/10) A cena do laboratório ("lab": { ... }, Config/RTS/LabScenes.json, a que o painel grava): a partida abre no
	 *  laboratório, e o bloco (a receita, o sol, o relógio, as ops pelo relógio da simulação) é lido pelo FRTSLab. */
	bool bLab = false;
	TSharedPtr<FJsonObject> LabBlock;
};

struct FRTSMapInfo
{
	FString Name;
	FString Title;
	float SizeM = 1024.0f;
	float CellM = 4.0f;
	float SampleM = 2.0f;
	float MaxSlope = 0.78f;
	float WaterBelowM = -1.5f;
	TArray<FVector2D> StartsM;
	/** A receita do look (plugin RTSLook, /RTSLook/Recipes/DA_Look_<nome>): o ambiente e a hora. Vazio = a luz do mapa. */
	FString LookRecipe;
	/** (29/10) A versão do chão ("ground_version": 1 = o de agora, 0 = o de 29/10 de manhã), a chave GroundV da coleção MPC_Ground no
	 *  material do chão do WorldForge; o F12 troca na partida (rts.Ground.Before). */
	int32 GroundVersion = 1;
	/** O skirmish de só tanques começa no caminho entre as bases, a esta distância (pelo caminho) um do outro; 0 = nas bases. */
	float SkirmishGapM = 0.0f;
	TArray<FString> ObstacleMeshes;
	/** (16/10) As malhas de árvore que o tanque derruba ("tree_meshes"; sem o campo, a DatePalm): viram objeto da
	 *  simulação (fecham só a célula do tronco, pra quem não esmaga), não mais a copa bloqueada. */
	TArray<FString> TreeMeshes;
	/** A vila do WorldForge (Config/RTS/<arquivo>, o village.json que ele exporta ao montar o mapa): as peças que
	 *  bloqueiam viram células fechadas na grade. Vazio = mapa sem vila. */
	FString VillageFile;
	/** O tipo de chão do WorldForge (Config/RTS/<arquivo>.png, o surface.png que ele exporta ao montar o mapa, com o
	 *  .json ao lado): R asfalto, G terra batida, B rocha, a 1 m. Só desenho (a poeira, o sulco). Vazio = tudo areia. */
	FString SurfaceFile;
	/** A grade da simulação do WorldForge (Config/RTS/<arquivo>.png, o grid.png com o .json ao lado; 13/10): a água por
	 *  célula de 4 m (R 0 seco, 1 margem, 2 vau, 3 fundo, 4 gelo; G o corpo) e os corpos. Vazio = a água pela altura. */
	FString GridFile;
	/** Pra onde o vento sopra (graus; 0 = +X): enche o rastro do lado de onde ele vem e é a mesma direção das
	 *  ondulações da areia do Look (que lê daqui). */
	float WindYawDeg = 0.0f;
	/** (16/10) O mapa montado com o chão v3 (o GROUNDV3 do WorldForge, "ground_v3": true): a v3 da memória do chão liga
	 *  sozinha (a luz assada, a garra, as mips); -RTSNoGroundV3 desliga pro A/B. */
	bool bGroundV3 = false;
	/** (v5, 24/10) "lab": true: o mapa só do laboratório e dos testes (o Lab_Flat, o chão plano), fora do menu do skirmish; o
	 *  LAB do menu abre o primeiro deles que existir. */
	bool bLabOnly = false;
	/** A velocidade do vento (m/s, "wind_speed"): a fumaça, a poeira, as bandeiras e as palmeiras (MPC_Wind). */
	float WindSpeedMs = 4.0f;
	/** Unidades de cada jogador no começo do skirmish: (chave, quantidade). */
	TArray<TPair<FString, int32>> StartingUnits;
	/** Com base (ZH): o prédio de largada (o Command Center pronto); vazio = skirmish de só tanques. */
	FString BaseStructure;
	/** Os depósitos de suprimento (até o WorldForge exportar os dele): centro (m), tamanho e caixas. */
	TArray<FVector2D> DocksM;
	FVector2D DockSizeM = FVector2D(26.33, 32.12);
	int32 DockBoxes = 400;
};

/** (31/10, o elenco das 3 facções: docs/ELENCO.md) Uma facção jogável: a chave ("usa", "china", "gla"; o começo das chaves das
 *  unidades), o nome na tela, o prédio de largada e as unidades de largada (o PlayerTemplate.ini do ZH). */
struct FRTSFactionInfo
{
	FString Key;
	FString Name;
	FString BaseStructure;
	TArray<TPair<FString, int32>> StartingUnits;
};

struct FRTSGameData
{
	Sim::Ruleset Rules;
	/** (31/10) As facções jogáveis (Units.json "factions"), na ordem do menu. */
	TArray<FRTSFactionInfo> Factions;
	const FRTSFactionInfo* FindFaction(const FString& Key) const
	{
		return Factions.FindByPredicate([&](const FRTSFactionInfo& F) { return F.Key == Key; });
	}
	TArray<FRTSUnitVisual> Visuals;     // mesma ordem de Rules.Units
	/** (v4) O nome e o ícone de cada poder do general, na ordem de Rules.Powers. */
	TArray<FRTSPowerVisual> PowerVisuals;
	TArray<FRTSMapInfo> Maps;
	FRTSDockVisual DockVisual;
	/** (v4) O dinheiro inicial: as escolhas do menu e da sala (Units.json "starting_money_choices", em ordem), o padrão
	 *  ("starting_money", sem o -RTSFast) e o fator do -RTSFast (×5): a montagem leva o escolhido × MoneyScale. */
	TArray<int32> StartingMoneyChoices;
	int32 DefaultStartingMoney = 10000;
	int32 MoneyScale = 1;

	/** Um cenário de teste rápido (Config/RTS/Scenarios.json; v5: depois o LabScenes.json, as cenas do laboratório). Falso se
	 *  não existir. */
	static bool LoadScenario(const FString& Name, struct FRTSScenario& Out, FString& OutError);

	/** Lê Units.json e Maps.json. Falso (com o motivo em OutError) se faltar arquivo ou campo. */
	bool Load(FString& OutError);

	const FRTSMapInfo* FindMap(const FString& Name) const;
	const FRTSUnitVisual* FindVisual(int32 DefIndex) const { return Visuals.IsValidIndex(DefIndex) ? &Visuals[DefIndex] : nullptr; }
	const FRTSPowerVisual* FindPowerVisual(int32 PowerIndex) const { return PowerVisuals.IsValidIndex(PowerIndex) ? &PowerVisuals[PowerIndex] : nullptr; }
};
