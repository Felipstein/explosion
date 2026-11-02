#pragma once

// O desenho de uma unidade. A posição, o casco e a torre vêm da simulação
// (interpolados entre dois passos); o resto é só visual: inclinar no relevo,
// recuo do tiro, a morte com a carcaça e às vezes a torre voando.
// Prédio também é unidade (o ZH: tudo é Object): a obra subindo com o andaime e a solda,
// as luzes que acendem na inauguração, o que gira (ventilador, radar), a cor do dono.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSLooseTurret.h"
#include "RTSUnitActor.generated.h"

class UPointLightComponent; class UStaticMeshComponent;
struct FRTSUnitVisual; struct FRTSTreadSource;

UCLASS()
class RTSGAME_API ARTSUnitActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSUnitActor();

	void Setup(uint32 InSimId, const FRTSUnitVisual& Visual);
	/** Pose do quadro: Location no chão (cm), casco e torre em graus (torre no mundo), normal do chão. */
	void SetPose(const FVector& Location, float HullYawDeg, float TurretYawWorldDeg, const FVector& GroundNormal, float DeltaSeconds);

	FVector MuzzleLocation() const;
	/** O acerto (14/10, o pedido do Felipe "o tiro acertando no tanque"; a pesquisa do Fluxo: o BAR faz): o casco
	 *  inclina pro lado contrário de quem atirou e volta na mola da suspensão. Só desenho. */
	void PlayHit(const FVector2D& FromCm);
	/** (01/11, a leitura da batalha; docs/ESTUDO_LEITURA_E_DANO.md A4/A5) o pulso de luz da unidade: UMA luz por ator, reacesa a cada tiro (o
	 *  atirador) ou acerto (o metal que pisca), caindo em TauS (o LightPulse do ZH: o fuzil 2,6 m e 0,2 s, a metralhadora da picape 6,6 m e 0,5
	 *  s, o casco acertado 4 m e 0,5 s); sem sombra. O pico em cd, a luz no AtCm (o mundo). */
	void PulseLight(const FVector& AtCm, float LightRadiusCm, float PeakCd, float TauS, const FLinearColor& Color);
	/** (30/10) O peito do soldado (o osso spine_03, que segue a animação: a bala acerta ali e a poça nasce embaixo); sem corpo, 1,5 m. */
	FVector GetSoldierChestCm() const;
	/** (16/10) O tranco de subir no carro esmagado: o nariz e o lado (graus por segundo na mola da suspensão). */
	void PlayBump(float PitchKickDeg, float RollKickDeg);
	FVector MuzzleDirection() const;
	/** O tiro: o recuo do canhão e o tranco do casco (bGunRecoil; o lançador de mísseis, a Patriot e o Tomahawk, sem) e o
	 *  clarão de boca provisório (-ShotFx=0). */
	void PlayFire(bool bGunRecoil = true);
	/** Morre: vira carcaça (e a torre voa: desde a v4, 23/10, um corpo rígido de verdade, RTSLooseTurret.h). Fica no mapa até a
	 *  simulação tirar (SetWreckSink). */
	void Die();
	/** (30/10, simulação 26) O soldado. */
	bool IsInfantry() const { return bInfantry; }
	/** A morte do soldado (o ZH: cai e fica 3 s deitado, afunda e some aos 8 s; sem explosão, sem carcaça, sem sangue). bCrushed: o
	 *  veículo passou por cima (fica achatado na hora). */
	void DieInfantry(bool bCrushed);
	/** (22/10) A carcaça de veículo afundando (0 inteira … 1 sumiu), pela simulação, sem relógio próprio: o casco desce a
	 *  altura dele + 30 cm (devagar no começo e no fim) e inclina 2–3°; a torre solta, pousada longe, desce junto (v4: a partir
	 *  de quando ela parou, até o topo dela ficar abaixo do chão onde encostou; no ar ela nunca afunda). */
	void SetWreckSink(float P01);
	float GetWreckSink() const { return WreckSinkP; }
	/** (31/10, a sucata) O quanto o casco já desceu (cm): o fogo da carcaça desmanchada desce junto. */
	float GetWreckSinkCm() const { return SinkCm; }
	/** (31/10, a sucata) O prédio caído (a ruína e o que sobrou do corpo) desmanchado pelo Dozer (0 inteiro … 1 sumiu), pela
	 *  simulação: desce no chão a altura dele + 30 cm, devagar no começo e no fim; só desce. */
	void SetRuinSink(float P01);
	/** A carcaça à vista agora (SetWreckLive); a já vista e fora da vista é o "fantasma" parado da névoa. */
	bool IsWreckLive() const { return bWreckLive; }

	/** A cor do dono nas faixas e lonas (dado de primitiva 0–3 do M_Structure). */
	void SetPlayerColor(const FLinearColor& Color);
	/** Prédio: quanto da obra está feito (0..1; 1 = pronto) e se tem energia (luzes, giro). */
	void SetStructureState(float InBuildFraction, bool bInPowered);
	/** (31/10) As peças do prédio com os materiais da obra (os gêmeos _Build), a 1 % e todas cortadas, presas em Parent: o carregar da partida
	 *  desenha uma de cada prédio pra placa ter o material da obra pronto antes da 1ª obra. Devolve quantas. */
	static int32 WarmBuildLook(USceneComponent* Parent, const TArray<FString>& HullMeshes);
	/** Caminhão: quantas caixas leva. */
	void SetCargo(int32 Boxes);
	/** Dozer: trabalhando na obra (a lâmina mexe). */
	void SetWorking(bool bInWorking);
	/** (30/10, 3ª volta: o DozerDirtFall do ZH) quantas vezes a lâmina já subiu trabalhando (a terra cai dela nessa hora) e a borda
	 *  de cima dela no mundo, com a frente e a meia largura (cm). false sem lâmina. */
	uint32 GetBladeLifts() const { return BladeLifts; }
	bool GetBladeLip(FVector& OutLipCm, FVector& OutFwd, float& OutHalfWidthCm) const;
	bool IsStructure() const { return bStructure; }
	/** (31/10) o prédio pronto que vira ruína ao morrer (visual.damage_states.ruin): o entulho de pedras fica menor */
	bool HasRuinMesh() const { return bStructure && !RuinMeshPath.IsEmpty() && BuildFraction >= 1.0f; }
	/** (01/11, a fábrica com túnel) se a unidade que acabou de nascer em SpawnCm saiu desta fábrica (a frente dela, no eixo), começa a
	 *  subida do túnel na unidade e devolve true */
	bool TryStartTunnelExit(ARTSUnitActor& Unit, const FVector2D& SpawnCm) const;
	/** (01/11) a unidade sobe do túnel pelos pontos (mundo: x, y; z = cm abaixo do chão) e liga na posição da simulação */
	void StartTunnelExit(const TArray<FVector>& Points, float SpeedCm);
	/** A cara de obra (o material que corta e o andaime) ainda ligada: o desenho ainda não trocou pro prédio pronto. */
	bool IsBuildLookOn() const { return bBuildLook; }
	/** Altura do prédio (cm), pra a explosão e o fogo da morte. */
	float GetHeightCm() const { return HeightCm; }
	/** (30/10, a obra com vida) Quanto a obra já subiu (0–1) e a planta do prédio (cm, no quadro dele). */
	float GetBuildFraction() const { return BuildFraction; }
	FBox2D GetFootprintCm() const { return FootprintCm; }
	/** (30/10) Os pontos de fogo do telhado no mundo (cm), pela pose de agora da malha do casco (vazio sem eles). */
	void GetRoofFirePoints(TArray<FVector>& OutCm) const;
	/** (30/10) O tiro no prédio na geometria de verdade: marcha o raio (cm, no mundo) no relevo medido do prédio (Config/RTS/Heights,
	 *  o Tools/roof_points.py: a altura de cada célula de 0,5 m vista de cima) e dá o 1º toque, a normal (a parede ou o teto), a peça
	 *  e o ponto no espaço da malha (a marca). false sem relevo ou sem toque. */
	bool RayStructure(const FVector& FromCm, const FVector& ToCm, FVector& OutCm, FVector& OutNormal, UStaticMeshComponent*& OutPart, FVector& OutLocalCm) const;

	uint32 GetSimId() const { return SimId; }
	/** Névoa: some (com sombra e luz) ou volta. Só troca quando muda (recriar as peças custa). (v4) A carcaça de uma morte que
	 *  ninguém viu, quando aparece, leva pro chão as marcas guardadas da torre solta. */
	void SetFogHidden(bool bHide);
	/** As marcas de dano no material (0 = inteiro, 1 = quase morto). */
	void SetDamage(float Damage); void SetSoldierPosture(uint8 InPosture) { SoldierPosture = InPosture; }   // (simulação 29) 0 de pé, 1 ajoelhado, 2 encolhido
	/** A silhueta atrás de prédio (M_UnitSilhouette): o stencil é 1 + o dono; 0 desliga (carcaça, prédio). */
	void SetSilhouette(int32 Stencil);
	/** (16/10, o Felipe) o contorno atrás das coisas só com a unidade escondida de verdade (a partida mede e liga). */
	void SetXRay(bool bOn);
	bool IsXRayOn() const { return bXRayOn; }
	/** A máscara de unidade (14/10, pedido da Arte: a poeira nunca desenha sobre veículo nem prédio): o bit 128 do
	 *  stencil em toda unidade viva, junto com a silhueta (os bits 1–127). */
	void SetUnitMask(bool bOn);
	bool IsFogHidden() const { return bFogHidden; }
	/** Carcaça na névoa: aparece como foi vista, sem o fogo "ao vivo" enquanto ninguém olha. */
	void SetWreckLive(bool bLive);
	/** A carcaça já foi vista pelo jogador local (a de inimigo morto na névoa só aparece quando a área é revista). */
	bool bWreckKnown = false;
	/** Quando o jogador local viu esta unidade pela última vez (segundos de mundo). */
	double LastSeenSeconds = -1.0e9;
	bool IsDead() const { return bDead; }
	/** (01/11, a avaliação do item 10: "o prédio caindo por etapas não tá caindo por etapas") quanto do prédio já desceu (0..1) no tempo
	 *  T01 do desabamento (0..1 dos StructureSinkSeconds): 3 andares que cedem um depois do outro, cada queda acelerando (a gravidade) e
	 *  uma pausa curta entre eles (o pó do andar que cedeu). O entulho que cai (ARTSDebrisField::AddRubble) sai da altura que sobra. */
	static float CollapseSunk01(float T01);
	/** (01/11) O desabamento: começa CollapseDelayS depois da morte e dura CollapseSeconds (o entulho que cai usa os mesmos). */
	static constexpr float CollapseDelayS = 0.3f;   // (01/11, v35: a 1 s caía embaixo das explosões; elas agora vêm depois, no entulho)
	static constexpr float CollapseSeconds = 5.0f;
	/** (simulação 35) O soldado dentro do transporte: bInside esconde (sem clique, sem barra); Up01 = quanto ele está pra fora da escotilha
	 *  (0 dentro, escondido; 1 da cintura pra cima). Sentado, o passo da animação fica parado (o transporte anda, ele não). */
	void SetPassenger(bool bInside, float Up01);
	bool IsInsideTransport() const { return bInsideTransport; }
	/** (01/11, ele: "não gostei dos hominhos aparecendo [...] pode mantê-los dentro escondido") quem atira de dentro: a boca é a escotilha
	 *  da vaga dele, no teto (o clarão e o traçante saem dali), e o transporte em que está (pra o tiro aparecer quando ele aparece). */
	void SetPassengerMuzzle(uint32 Transport, const FVector& MouthCm, const FVector& Dir) { PassengerTransport = Transport; PassengerMouthCm = MouthCm; PassengerAimDir = Dir; }
	uint32 GetPassengerTransport() const { return bInsideTransport ? PassengerTransport : 0u; }
	/** (01/11) A escotilha da vaga Slot (LocalCm: o meio dela no casco, no teto) abre pro tiro de quem está dentro e fecha 2 s depois do
	 *  último; a tampa é uma chapa na tinta do casco que gira na dobradiça do lado do meio, e o vão escuro aparece embaixo. */
	void OpenHatch(int32 Slot, const FVector& LocalCm, const FVector2D& SizeCm);
	/** (01/11) O Javelin de dentro: o lançamento suave (o motor de ejeção joga o míssil pra cima, pela escotilha; o de voo acende no ar) e o voo
	 *  emenda no da simulação em LaunchBlendSeconds. */
	void NotePassengerShot(uint32 MissileId, const FVector& SimCm, const FVector& MouthCm, const FVector& Dir);
	float PassengerUp() const { return PassengerUp01; }
	double LastShotSeconds() const { return SoldierShotAt; }
	float SoldierScale() const { return SoldierBodyScale; }
	float GetRadiusCm() const { return RadiusCm; }
	/** A lagarta (cm): bitola e largura; zero = não risca o chão. */
	FVector2D GetTracksCm() const { return TracksCm; }
	/** (16/10) As caixas da sombra no mapa do sol comum (RTSSunShadow), das malhas: o centro delas no quadro do casco (cm;
	 *  x pra frente, y pra direita), o meio comprimento, a meia largura, o topo do casco e o topo de tudo (cm do chão). */
	void GetShadowBoxCm(FVector2D& OutCenter, float& OutHalfLen, float& OutHalfWidth, float& OutHullTop, float& OutHeight) const;
	/** A frente do casco e a da mira da torre, no chão (unitárias, mundo). */
	FVector HullForward() const;
	/** (16/10) O quadro do casco (inclina no relevo): as saídas do escape saem dele. */
	FTransform GetBodyTransform() const { return Body ? Body->GetComponentTransform() : GetActorTransform(); }
	/** (v4, 23/10) A caixa que a torre solta de outro tanque não atravessa: o quadro do casco (sem escala) e a caixa das malhas
	 *  do casco nele (cm), até o topo da torre se ela está no lugar. Falso sem malha. */
	bool GetCollisionBoxCm(FTransform& OutFrame, FBox& OutBox) const;
	/** (v4) A torre solta desta carcaça como caixa (o corpo, no quadro dela, cm): só a parada, a não ser com bOnlyResting falso. */
	bool GetLooseTurretBoxCm(FTransform& OutFrame, FBox& OutBox, bool bOnlyResting = true) const;
	/** (v4, 23/10) A torre solta ainda no ar ou rolando: a carcaça que a simulação tira logo depois da morte espera ela parar pra
	 *  afundar (as duas descem juntas, como na v2). Com teto: 12 s depois da morte (o teto de 10 s de voo da torre e a saída). */
	bool IsLooseTurretMoving() const;
	FVector TurretForward() const;

	/** (23/10, v4) A poeira das lagartas e das rodas (o FRTSTreadSource do plugin RTSTreadDust): falso = não levanta poeira (prédio,
	 *  carcaça, ou sem lagarta nem o bloco "wheels" do Units.json). A lagarta fica com o padrão do plugin (o tanque aprovado, intocado); a
	 *  roda preenche a geometria lida das malhas wheel_* (o eixo de trás e o da frente, o raio, a bitola), a física do bloco "wheels" (o
	 *  lado e a massa do puff) e as caixas do veículo (o ar que ele empurra, a sombra dele na poeira, o rastro em comprimentos dele).
	 *  OutSandRearCm / OutSandFrontCm: onde a ponte lê a areia, no quadro do casco (cm; x pra frente, y o meio da lagarta ou do pneu). */
	bool FillDustSource(FRTSTreadSource& S, FVector2D& OutSandRearCm, FVector2D& OutSandFrontCm) const;
	/** (23/10, v4) A marca do pneu na memória do chão: a bitola e a largura do pneu (cm; zero = não risca de pneu; a lagarta vem do
	 *  GetTracksCm) e o jeito dela contra o da lagarta: o escuro, o fundo do sulco, a largura do perfil, as garras por metro e o trecho
	 *  nas retas (URTSGroundMemory::FTrackStyle). */
	FVector2D GetTyreMarksCm(float& OutDark, float& OutDepth, float& OutWidthK, float& OutLugK, float& OutSegmentK) const;
	/** (23/10, v4) O lançador que levanta (o Tomahawk; o bloco "launcher"). AimSeconds = o relógio da mira da simulação em segundos
	 *  (AimTicks, entre os passos; o tiro sai com ele em PreAttackSeconds, o pre_attack_s, 1 s): sobe desde que há alvo no alcance,
	 *  segura com o alvo fora e, sem alvo, desce junto com o relógio quando ele começa a cair (a volta da torre, 2 s depois do último
	 *  alvo; o relógio cai na metade da velocidade). O ângulo pedido segue o AIM do ZH: subindo, o de tiro desde o começo da mira (o
	 *  cilindro acelera, corre e freia: 45° em 0,75 s, a média do TurretPitchRate 60 do ZH, e o berço passa ~1,1° no batente e assenta
	 *  antes do tiro em 1 s); caindo, o de tiro × a fração do relógio (chega deitado com ele em zero). Sem o relógio na simulação (o pre_attack_s zero), vale bEngaged (mira alguém, ou o tiro no chão, no
	 *  alcance), com o relógio daqui: sobe na hora e desce LauncherLowerDelayS depois do último alvo. bWeaponReady = a arma da simulação
	 *  sem espera (o Cooldown zero): com a rajada toda atirada, é a recarga pronta, e as pontas dos mísseis voltam nos tubos. Só desenho. */
	void SetLauncherAim(bool bEngaged, float AimSeconds = 0.0f, float PreAttackSeconds = 0.0f, bool bWeaponReady = true);
	bool HasLauncher() const { return ElevPivot != nullptr; }
	float GetLauncherPitchDeg() const { return LauncherPitch; }
	/** (23/10, v4) O tiro do lançador que levanta, chamado em todo tiro dele (também na névoa): o tubo é o do lado de onde a simulação
	 *  soltou o míssil MissileId (SimCm e SimDir, mundo; o lado da torre, que não muda com o casco inclinado nem com a elevação), a boca
	 *  do tiro de agora (MuzzleLocation) passa pra ele e a ponta do míssil dele some até a recarga da rajada (o
	 *  ProjectileBoneFeedbackEnabledSlots do Tomahawk do ZH: o míssil atirado não fica no tubo). OutMouthCm e OutMouthDir: a boca desse
	 *  tubo e a direção dele na pose desenhada (no chão inclinado o casco desenhado inclina e a boca sai do ponto da simulação, que é
	 *  plano: o clarão e a nuvem saem daqui, e o voo novo emenda daqui no da simulação, GetLaunchBlend); sem lançador, SimCm e SimDir.
	 *  Devolve o tubo (INDEX_NONE sem lançador). */
	int32 NoteLauncherShot(uint32 MissileId, const FVector& SimCm, const FVector& SimDir, FVector& OutMouthCm, FVector& OutMouthDir);
	/** (23/10, v4) O voo novo do míssil MissileId deste lançador: a diferença da boca desenhada pro ponto da simulação no disparo (cm) e
	 *  a direção do tubo desenhado. Quem desenha soma a diferença, sumindo em LaunchBlendSeconds (e a direção vai do tubo pra do voo):
	 *  no chão plano é zero. Falso pra outro míssil. */
	bool GetLaunchBlend(uint32 MissileId, FVector& OutOffsetCm, FVector& OutDir, bool* bOutSoft = nullptr, FVector* OutMouthCm = nullptr) const;
	static constexpr float LaunchBlendSeconds = 0.6f;
	/** (01/11) O lançamento suave (o Javelin de dentro do Stryker): o motor de ejeção leva o míssil pra fora a SoftEjectMps, sem chama, por
	 *  SoftEjectSeconds (~2,6 m); aí o de voo acende e ele vai pro voo da simulação até LaunchBlendSeconds. O Javelin de verdade acende o
	 *  motor de voo a uns metros do atirador (o "soft launch" da FGM-148) */
	static constexpr float SoftEjectSeconds = 0.22f;
	static constexpr float SoftEjectMps = 12.0f;
	/** (25/10, v5) O berço de canhões do antiaéreo (o "launcher" com "guns": o Linebacker): os tubos são as bocas dos canos (cada tiro
	 *  passa pro outro cano, PlayFire), o míssil não sai deles. A elevação pela aeronave mirada: PitchDeg = o ângulo do horizonte da
	 *  dobradiça até ela (graus; < −90 = sem aeronave: a de tiro). Só desenho. */
	bool LauncherIsGuns() const { return bLauncherGuns; }
	bool LauncherAimsAir() const { return bLauncherAimAir; }
	void SetGunAimPitch(float PitchDeg) { GunAimPitchDeg = PitchDeg; }
	/** A dobradiça dos canhões no mundo (cm), na pose de agora. */
	FVector GunHingeWorld() const;
	/** (25/10, v5) As células de lançamento vertical no casco ("vls_m"): o míssil MissileId sai da célula da vez, pra cima no casco
	 *  desenhado, e emenda no voo da simulação em LaunchBlendSeconds (GetLaunchBlend). Falso sem células. Só desenho. */
	bool HasVls() const { return VlsCm.Num() > 0; }
	bool NoteVlsShot(uint32 MissileId, const FVector& SimCm, FVector& OutMouthCm, FVector& OutMouthDir);
	/** (25/10) O míssil da aeronave sai do trilho desenhado: a peça do pente à vista (AmmoParts[AmmoIndex], a que some neste tiro: o
	 *  Hellfire do Apache, o míssil da asa do Raptor), na pose de agora, com o rumo dela; o voo emenda dali no da simulação em
	 *  LaunchBlendSeconds (GetLaunchBlend). Falso sem a peça. Só desenho. */
	bool NoteAmmoShot(uint32 MissileId, int32 AmmoIndex, const FVector& SimCm, FVector& OutMouthCm, FVector& OutMouthDir);
	/** (25/10, v5) A arma leve de tiro rápido ("light_gun": o antiaéreo e a metralhadora): o clarão (cm) e o risco do traçante (cm). */
	bool IsLightGun() const { return bLightGun; }
	/** (01/11) O lançador de caixa fechada (a Patriot): a saída grande; os outros, a ignição pequena na tubeira. */
	bool IsLaunchCanister() const { return bLaunchCanister; }
	float GetLightGunFlashCm() const { return LightGunFlashCm; }
	float GetDeathScale() const { return DeathScale; }
	/** (30/10) A assinatura da morte (Units.json visual.death_kind: 0 tanque, 1 gasolina, 2 diesel, 3 munição). */
	int32 GetDeathKind() const { return DeathKind; }
	/** (30/10) O jato de fogo da morte (visual.death_jet): a gravação, a escala e a altura da boca dentro dela. Vazio = sem jato. */
	FName GetDeathJetBake() const { return DeathJetBake; }
	float GetDeathJetScale() const { return DeathJetScale; }
	float GetDeathJetBaseM() const { return DeathJetBaseM; }
	/** (30/10) O anel da torre no mundo (cm): o pivô da torre na altura do pé dela, pelo casco (o Body, que inclina no relevo). */
	FVector GetTurretRingCm() const;
	/** (30/10) O rumo do casco (graus): o ator não gira (o casco é o Body), então o "local" do tranco sai deste rumo, não do ator. */
	float GetHullYawDeg() const;
	float GetLightGunTracerCm() const { return LightGunTracerCm; }
	/** (30/10) A dispersão da rajada no chão (cm, o semieixo ao longo do tiro). */
	float GetLightGunSpreadCm() const { return LightGunSpreadCm; }

	/** (v5, 24/10) A aeronave (docs/AEREO.md §4.3; o escopo A desenha a pose, o escopo B põe os efeitos): Location = a posição no ar
	 *  (cm, já com a altura da simulação sobre o chão de verdade), o rumo, a inclinação que a partida tirou da física (PitchDeg + o nariz
	 *  sobe; RollDeg + a asa direita desce), o rotor (0..1: o giro dos rotores, que sobe na decolagem e desce depois do pouso), o trem
	 *  baixado (bGearDown) e os mísseis à vista (AmmoShown; −1 = todos). Suaviza a inclinação no desenho. */
	void SetAirPose(const FVector& Location, float YawDeg, float PitchDeg, float RollDeg, float Rotor01, bool bGearDown, int32 AmmoShown, float DeltaSeconds);
	bool IsAircraft() const { return bAircraft; }
	/** (v5) A queda: o corpo segue a pose dada e a pá do rotor some quando ela voa (o evento AirBladeOff). */
	void SetBladeOff();
	/** (v5) A pose de agora da aeronave (o escopo B lê pra pôr o vento do rotor, o pós-combustor, o rastro da queda). */
	float GetAirPitchDeg() const { return AirPitch; }
	float GetAirRollDeg() const { return AirRoll; }
	float GetRotor01() const { return AirRotor; }
	/** (24/10, escopo B do ar; RTSUnitActorAir.cpp) O pós-combustor do jato (0..1: a corrida e a subida; a chama e a luz no chão sobem e
	 *  descem suaves aqui) e a rampa de trás (aberta carregando e descarregando; leva RampOpenS pra abrir). Só desenho. */
	void SetAfterburner(float Throttle01);
	void SetRampOpen(bool bOpen);
	/** Os rotores de sustentação (eixo Z) pro vento do rotor: o cubo de cada um (mundo, cm) e o raio (cm). Falso sem rotor. */
	bool GetLiftRotors(TArray<FVector>& OutHubsCm, TArray<float>& OutRadiiCm) const;
	/** A massa (t, do "mass_t" do visual): o empuxo pairando é o peso. */
	float GetAirMassT() const { return AirMassT; }
	/** A velocidade desenhada (cm/s, da pose de um quadro pro outro). */
	FVector GetAirVelocityCm() const { return AirVelCm; }
	/** O ponto do motor (mundo, cm): de onde sai a fumaça da queda e o fogo. */
	FVector GetEngineWorldCm() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** (01/11) o recorte do chão do prédio pronto (visual.ground_cut_m, no quadro do prédio, cm) e se está ligado no MPC_GroundCut; o
	 *  caminho do túnel (visual.tunnel_exit) e a subida da unidade que nasceu dele (os pontos no mundo, a distância andada, a inclinação) */
	TArray<FVector4> GroundCutsCm;
	bool bGroundCutOn = false;
	FTransform GroundCutAt;
	void UpdateGroundCut(bool bWant);
	TArray<FVector2D> TunnelPathCm;
	float TunnelSpeedCm = 400.0f;
	TArray<FVector> TunnelPts;
	float TunnelDoneCm = 0.0f;
	float TunnelRunCm = 400.0f;
	float TunnelPitchDeg = 0.0f;
	bool ApplyTunnelExit(FVector& Location, float DeltaSeconds);
public:
	/** (01/11) os recortes ligados no mundo (cm: centro, meia-largura, cos e sen do giro), na ordem; o MPC_GroundCut lê os 8 primeiros */
	static void GetGroundCuts(TArray<FVector4>& OutCenterHalf, TArray<FVector2D>& OutCosSin);

private:
	// (v5, 24/10) a aeronave: a inclinação suavizada, o rotor, o trem e os mísseis à vista (as malhas pelos nomes do Units.json)
	bool bAircraft = false;
	float AirPitch = 0.0f;
	float AirRoll = 0.0f;
	float AirRotor = 0.0f;
	TArray<TObjectPtr<UStaticMeshComponent>> GearParts;
	TArray<TObjectPtr<UStaticMeshComponent>> AmmoParts;
	TArray<bool> SpinIsRotor;
	TArray<int32> SpinAxis;
	int32 AmmoLast = -1;
	// (24/10, escopo B do ar) os discos borrados dos rotores (um por rotor com "blades": a malha procedural parada no cubo, o material
	// lê a opacidade, a fase das pás fantasma, as pás, o cubo, a ponta pintada e a cobertura pelos dados da primitiva), a pá que voou,
	// a rampa, o pós-combustor e o fogo da queda
	TArray<int32> SpinBlades;
	TArray<float> SpinRadiusCm, SpinTipCm, SpinChordCm;
	UPROPERTY() TArray<TObjectPtr<class UProceduralMeshComponent>> RotorDiscs;
	TArray<int32> RotorDiscSpin;
	TArray<float> RotorDiscPhase;
	/** (01/11, o A/B da leitura do Apache) -RTSRotorFreeze=1: o rotor I no giro cheio mostra a pá de verdade "congelada" pela câmera
	 *  (na pá fantasma do disco); devolve o ângulo desenhado (o do giro, sem a chave) */
	bool IsRotorFrozen(int32 SpinIndex, float RateDps) const;
	float RotorShownDeg(int32 SpinIndex, float SpinDeg, float RateDps) const;
	TArray<bool> RotorDiscBladeOff;
	float AirMassT = 0.0f;
	/** (29/10) A massa do veículo (t, o "mass_t" do visual) pro tranco do acerto de canhão: o casco gira ∝ impulso ÷ massa. 0 = a do tanque. */
	float HitMassT = 0.0f;
	float DeathScale = 1.0f;   // (30/10) o tamanho da morte (Units.json visual.death_scale)
	int32 DeathKind = 0;       // (30/10) a assinatura da morte (visual.death_kind)
	FName DeathJetBake;        // (30/10) o jato de fogo pelo anel da torre (visual.death_jet)
	float DeathJetScale = 1.0f, DeathJetBaseM = 0.0f;
	FVector AirVelCm = FVector::ZeroVector;
	FVector AirLastLoc = FVector::ZeroVector;
	bool bAirLastLoc = false;
	struct FFlyingBlade { TObjectPtr<UStaticMeshComponent> Part; FVector Vel = FVector::ZeroVector; FVector Axis = FVector::UpVector; float SpinDps = 0.0f; bool bLanded = false; };
	TArray<FFlyingBlade> FlyingBlades;
	UPROPERTY() TObjectPtr<USceneComponent> RampPivot;
	float RampOpenDeg = 39.3f, RampOpenS = 2.0f, RampAngle = 0.0f;
	bool bRampWant = false;
	UPROPERTY() TArray<TObjectPtr<class UProceduralMeshComponent>> Flames;
	UPROPERTY() TObjectPtr<class UPointLightComponent> FlameLight;
	float AfterburnerWant = 0.0f, AfterburnerShown = 0.0f, AfterburnerRadiance = 1.0f, AfterburnerLightCd = 60000.0f, AfterburnerLenCm = 320.0f;
	FVector EngineLocalCm = FVector::ZeroVector;
	void SetupAirFx(const FRTSUnitVisual& Visual);
	void TickAirFx(float DeltaSeconds);
	void TickFlyingBlades(float DeltaSeconds);
	void StopAirFx();
	void AddParts(const TArray<FString>& Paths, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out);
	/** A mola da suspensão (só no desenho), em passos fixos de 1/30 s como no ZH. */
	void StepSuspension(float DeltaSeconds, float ForwardDeltaV, float LateralAccel);
	void Char();
	/** Troca os materiais de todas as peças pelos da obra (MI_X_Build) ou de volta. */
	void SetBuildLook(bool bBuild);
	void SetPartsFloat(int32 Index, float Value);
	void TickStructure(float DeltaSeconds);
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, USceneComponent* Parent);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;          // inclina no relevo
	UPROPERTY() TObjectPtr<USceneComponent> TurretPivot;   // gira com a torre
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HullParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TurretParts;
	UPROPERTY() TObjectPtr<UPointLightComponent> FireLight;
	/** Cada roda num pivô no centro dela, pra girar no próprio eixo (as peças saem com a origem no centro do tanque). */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelPivots;
	TArray<float> WheelRadiusCm;
	float WheelAngleDeg = 0.0f;
	/** (01/11, a esteira que anda; docs/BASE_JOGAVEL.md "Esteira andando") as lagartas do casco (as malhas "_Track_"), o lado de cada uma
	 *  (o y do centro, cm), a fase do elo (0..1), o último valor mandado e o passo (cm); e o giro de cada roda da lagarta (cada uma anda
	 *  com o seu lado: no giro no lugar, um lado pra frente e o outro pra trás) */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TrackParts;
	TArray<float> TrackSideYCm;
	TArray<float> TrackPhase;
	TArray<float> TrackSent;
	TArray<float> WheelAngleEachDeg;
	float TrackStepCm = 0.0f;
	void StepTracks(float TravelCm, float DYawRad);

	// suspensão: ângulo e velocidade angular (graus, graus por passo de 1/30 s)
	float SuspPitch = 0.0f;
	float SuspPitchRate = 0.0f;
	float SuspRoll = 0.0f;
	float SuspRollRate = 0.0f;
	float SuspClock = 0.0f;
	bool bHasLastPose = false;
	FVector LastLocation = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	float LastYawDeg = 0.0f;

	uint32 SimId = 0;
	float RadiusCm = 300.0f;
	float MuzzleForwardCm = 500.0f;
	/** O eixo da torre no casco (cm): o pivô fica ali e as malhas da torre voltam pra origem do tanque. */
	FVector TurretPivotCm = FVector::ZeroVector;
	/** (16/10) A frente da malha da torre fora do +X (a ficha visual): somada ao giro da torre no desenho. */
	float TurretYawOffsetDeg = 0.0f;
	FVector2D TracksCm = FVector2D::ZeroVector;
	float MuzzleHeightCm = 200.0f;
	float RecoilCm = 0.0f;
	FQuat TiltSmoothed = FQuat::Identity;

	bool bDead = false;
	mutable bool bShadowBoxReady = false;
	mutable FBox ShadowHullBox = FBox(EForceInit::ForceInit);
	mutable FBox ShadowTurretBox = FBox(EForceInit::ForceInit);
	bool bFogHidden = false;
	bool bWreckLive = true;
	float DeathAge = 0.0f;
	bool bCharPending = false;   // (31/10) o queimado espera o fogo cobrir (Die → Tick)
	float DamageShown = 0.0f;
	/** (v4, 23/10) A torre solta com física de corpo rígido: o corpo (o pivô no centro de massa dela), os obstáculos que ela
	 *  enxerga (o próprio casco e as unidades, carcaças, prédios e torres pousadas até 20 m da queda prevista; o dono de cada
	 *  um, e se é a torre dele) e o caminho da v2 (-TurretPhys=0, pro A/B com a mesma história). */
	TUniquePtr<FRTSLooseTurret> Loose;
	TArray<FRTSTurretObstacle> LooseObstacles;
	TArray<TWeakObjectPtr<ARTSUnitActor>> LooseObstacleActors;
	TArray<bool> LooseObstacleIsTurret;
	/** Onde a escolha do lado previu a queda (cm) e a inclinação do chão sob o casco na morte (graus): pro log. */
	FVector2D LooseLandPredicted = FVector2D::ZeroVector;
	float DeathSlopeDeg = 0.0f;
	/** O corpo da torre da ficha (turret_box_m, cm no quadro das malhas): sem o mastro e as antenas; inválida = o das malhas. */
	FBox TurretBodyBoxCm = FBox(EForceInit::ForceInit);
	void LaunchLooseTurret();
	void TickLooseTurret(float DeltaSeconds);
	void RefreshLooseObstacles();
	void OnLooseTurretAsleep();
	/** As batidas da torre de uma morte que ninguém viu (no máximo 4): a marca espera a carcaça aparecer (SetFogHidden), senão
	 *  ela nascia do nada no chão à vista e entregava a morte na névoa. */
	TArray<FRTSTurretImpact> UnseenTurretImpacts;
	void StampUnseenTurretImpacts();
	/** O voo da v2 (13/10): sobe, quica uma vez num chão a 60 cm do Z do ator e assenta de pé ou de ponta-cabeça. */
	void LaunchTurretV2();
	bool bTurretFlying = false;
	FVector TurretVelocity = FVector::ZeroVector;
	/** O giro da torre voando (eixo × rad/s) e a pose; pousada, assenta em 0,3 s de pé ou de ponta-cabeça. */
	FVector TurretSpin = FVector::ZeroVector;
	FQuat TurretQ = FQuat::Identity;
	bool bTurretSettling = false;
	float TurretSettle = 0.0f;
	FQuat TurretRestQ = FQuat::Identity;

	// prédio, Dozer e caminhão
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> SpinPivots;
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> BeaconLights;   // (30/10) os fachos do giroflex do Dozer
	UPROPERTY() TObjectPtr<class UPointLightComponent> BeaconGlow;   // (31/10) a cúpula acesa do giroflex
	float BeaconPeakCd = 80000.0f;
	float BeaconGlowCd = 2500.0f;   // (01/11) a luz da cúpula acesa (a v3: 40 000 cd)
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SpinParts;
	TArray<float> SpinRpm;
	TArray<float> SpinAngleDeg;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scaffold;
	UPROPERTY() TObjectPtr<USceneComponent> BladePivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BladePart;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CargoParts;
	int32 SilhouetteStencil = 0;
	bool bXRayOn = false;
	bool bUnitMask = false;
	void ApplyStencil();
	bool bStructure = false;
	bool bBuildLook = false;
	float BuildFraction = 1.0f;
	/** (01/11) o canteiro da obra não começada (UpdateSite): as peças (as estacas e a fita, o tapume, o material) e o tipo de cada uma
	 *  (0 gabarito, 1 tapume, 2 material dentro do lote) e a escala de cada uma; o prédio já ficou pronto uma vez (vender não refaz o canteiro) */
	TArray<TObjectPtr<UStaticMeshComponent>> SiteParts;
	TArray<uint8> SiteKind;
	TArray<FVector> SiteScale;
	bool bSiteMade = false;
	bool bEverComplete = false;
	float SiteShownFraction = -1.0f;
	void UpdateSite();
	TArray<FVector> RoofFirePointsCm;
	/** (30/10, simulação 26) O soldado: as pernas no quadril (o passo pelo chão andado, sem patinar), a fase e o quanto anda, e a morte. */
	bool bInfantry = false;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> LegPivots;
	/** (30/10, a infantaria nova) o corpo de verdade (o modelo da Unreal 5.8 com os clipes de fuzil, RTSSoldierAnim), o fuzil na mão e
	 *  quando atirou por último (o estado de tiro da animação) */
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> SoldierMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SoldierRifle;
	/** (01/11) A arma do soldado é um lança-foguetes (/Game/Weapons/Launchers/: o Javelin, o RPG-7, o PF-98): vai no ombro, não na mão
	 *  como o fuzil; e quanto o tubo está no jeito de levar andando (0 apontado, 1 empinado no ombro) */
	bool bShoulderLauncher = false;
	float LauncherCarry = 0.0f;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SoldierHelmet;
	/** (30/10) as peças do soldado vestido presas nos ossos (o colete, o capacete, a mochila, as bolsas): FRTSUnitVisual::SoldierPieces */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SoldierPieceParts;
	/** (30/10) o corpo já virou boneco de pano (o fim do clipe da morte) */
	bool bSoldierRagdoll = false;
	/** (30/10, item 31) os furos do prédio ferido: quantos já abriram (as passagens de 70, 50, 35 e 20% da vida) */
	int32 HoleStage = 0;
	int32 CollapseStage = 0;   // (01/11) as etapas do desabamento que já soltaram o pó (ARTSUnitActor::Tick)
	void UpdateStructureHoles(float Health01);
	/** (31/10, os estados de dano: Tools/model_damage.py) o estado mostrado (0 inteiro, 1 danificado, 2 muito danificado), a malha do casco
	 *  de cada um (o caminho do pacote; vazio = não troca), o fogo de cada um e a ruína que fica no lugar do prédio morto */
	int32 StructState = 0;
	FString StructMeshPaths[3];
	TArray<FVector> StructFireCm[3];
	FString RuinMeshPath;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RuinPart;
	void UpdateStructureState(float Health01);
	/** (01/11) Os estados de dano do veículo: o casco e a torre de cada estado (0 inteiro, 1 abaixo de 70%, 2 abaixo de 35%). */
	FString TurretMeshPaths[3];
	void UpdateVehicleState(float Health01);
	bool OpenStructureHole(uint32 Seed, float HoleRadiusCm, FVector& OutCm, FVector& OutNormal);
	void ClearStructureHoles();
	/** (01/11) a cor dos pedaços que caem do furo: a da parede do prédio (o concreto, o reboco, o corpo). */
	FLinearColor WallTint() const;
	/** (30/10) a bandeira de pano no topo do mastro (visual.flag): a malha refeita a cada quadro no vento aparente (StepFlag) */
	/** (01/11, o Felipe: "tudo que tem bandeira tem que ter física"; "cuidado com otimização") todas as bandeiras da unidade (a da Technical
	 *  e as dos prédios, 3–6 na GLA): o pano fixo SM_FlagCloth que o M_FlagCloth balança na placa. A CPU só manda o vento quando ele muda
	 *  (SendFlagWind): a direção, o quanto levanta, o ritmo da onda e a fase de cada uma (a continuidade quando o ritmo muda) */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FlagMeshes;
	TArray<float> FlagOffset;
	float FlagRate = 0.0f;
	float FlagSentYaw = 1e9f, FlagSentLift = -1.0f, FlagSentRate = -1.0f;
	bool bFlagShown = true;
	void SendFlagWind(bool bForce);
	float FlagPhase = 0.0f, FlagYawWorld = 0.0f, FlagLift = 0.3f;
	FVector FlagLastPos = FVector::ZeroVector;
	double SoldierShotAt = -100.0;
	bool bInsideTransport = false; float PassengerUp01 = 0.0f; bool bPassengerHidden = false; float SoldierBodyScale = 1.25f;   // (simulação 35)
	uint32 PassengerTransport = 0; FVector PassengerMouthCm = FVector::ZeroVector; FVector PassengerAimDir = FVector::ForwardVector;   // (01/11)
	struct FHatchLid { TObjectPtr<UStaticMeshComponent> Lid; TObjectPtr<UStaticMeshComponent> Hole; FVector LocalCm = FVector::ZeroVector; FVector2D SizeCm = FVector2D::ZeroVector; float Open01 = 0.0f; double HoldUntil = -1.0; };
	TArray<FHatchLid> HatchLids;   // (01/11) as escotilhas que abrem pro tiro de dentro
	void StepHatches(float DeltaSeconds);
	UPROPERTY() TObjectPtr<UPointLightComponent> PulseLightComp;   // (01/11) o pulso da leitura (PulseLight)
	float PulsePeakCd = 0.0f; float PulseTauS = 0.2f; double PulseAt = -100.0;
public:
	double LastHitPulseS = -100.0;   // (01/11) o último pulso de acerto (o teto de 1 a cada 0,1 s)
private:
	void StepPulse();
	float SoldierSpeedCmS = 0.0f; uint8 SoldierPosture = 0;
	FVector LastSoldierPos = FVector::ZeroVector;
	float StrideCm = 360.0f;
	float StepPhase = 0.0f;
	float StepAmount = 0.0f;
	FVector LastPoseForStep = FVector::ZeroVector;
	bool bCrushedDeath = false;
	float FallSign = 1.0f;
	FQuat DeathBaseQ = FQuat::Identity;
	/** (30/10) O relevo do prédio visto de cima (Config/RTS/Heights/<malha>.json; o mesmo pra todos os prédios da mesma malha). */
	struct FHeightGrid { FVector2D MinCm = FVector2D::ZeroVector; float CellCm = 50.0f; int32 Nx = 0, Ny = 0; TArray<float> H; };
	TSharedPtr<const FHeightGrid> Heights;
	/** (30/10, o tiro no prédio pelo modelo) os triângulos da malha (Config/RTS/Heights/<malha>.tris.bin, do roof_points.py), em cm no
	 *  quadro da malha, com as células de 1 m em XY que cada um toca (o raio só testa os das células que atravessa) */
	struct FTriGrid { TArray<FVector3f> V; FVector2f MinCm = FVector2f::ZeroVector; float CellCm = 100.0f; int32 Nx = 0, Ny = 0; TArray<TArray<int32>> Cells; };
	TSharedPtr<const FTriGrid> Tris;
	bool bPowered = true;
	/** A luz mostrada (0..1): pisca ao acender como lâmpada de galpão, apaga em 0,3 s. */
	float PowerShown = 1.0f;
	float PowerOnClock = 1.0f;
	float LastPowerSent = -1.0f;
	float LastCutSent = -1.0f;
	/** (31/10) O andaime à vista (cm acima do pivô; começa fora de qualquer valor, pra 1ª vez sempre pôr) e a última vez que a obra subiu (s de
	 *  mundo): a solda só com o Dozer trabalhando. */
	float ScaffoldShownCm = -1000.0f;
	/** (01/11) O andaime em volta da obra inteira: os vãos (instâncias), a base de cada vão no 1º lance, quantos lances, e quantos estão à
	 *  vista (UpdateScaffoldRing). Sem a peça (SM_Site_ScaffoldBay), fica o andaime do modelo */
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> ScaffoldRing;
	TArray<FTransform> ScaffoldRingBays;
	int32 ScaffoldRingLifts = 0;
	int32 ScaffoldRingShown = -1;
	bool bScaffoldRingTried = false;
	void UpdateScaffoldRing(float ShownCm);
	double LastBuildRiseS = -1.0e9;
	float HeightCm = 1000.0f;
	FBox2D FootprintCm = FBox2D(FVector2D(-500.0), FVector2D(500.0));
	float SparkClock = 0.0f;
	uint32 SparkSeed = 1;
	bool bWorking = false;
	float BladeClock = 0.0f;
	float BladeAmount = 0.0f;
	uint32 BladeLifts = 0;
	int32 CargoShown = -1;
	/** O quanto o corpo desceu (cm): o prédio que desaba e, desde 22/10, a carcaça de veículo afundando. */
	float SinkCm = 0.0f;
	// (22/10) a carcaça afundando: o progresso, a pose de onde o casco desce, a torre solta (onde pousou, quanto desce e, v4, o
	// progresso da carcaça quando ela começou a descer: a que ainda caía começa quando para)
	float WreckSinkP = 0.0f;
	// (31/10, a sucata) o prédio caído desmanchado: o progresso, de onde desce e quanto
	float RuinSinkP = 0.0f;
	FVector RuinSinkBase = FVector::ZeroVector;
	float RuinSinkDepthCm = 0.0f;
	FVector SinkBodyLoc = FVector::ZeroVector;
	FQuat SinkBodyQ = FQuat::Identity;
	bool bSinkTurret = false;
	FVector SinkTurretLoc = FVector::ZeroVector;
	float SinkTurretCm = 0.0f;
	float SinkTurretFromP = 0.0f;
	void ArmTurretSink(float FromP);
	/** As caixas das malhas (casco e torre, no quadro do casco) pra sombra e pro afundar, lidas uma vez. */
	void EnsureShadowBoxes() const;

	// (23/10, v4) as rodas (o bloco "wheels"; Plugins/RTSTreadDust/INTEGRACAO.md "As rodas"): a geometria lida das malhas wheel_* no
	// Setup (cm, no quadro do casco) e a física da poeira e da marca
	bool bWheelDust = false;
	float WheelRearXCm = 0.0f, WheelFrontXCm = 0.0f, WheelHalfGaugeCm = 0.0f, TyreRadiusCm = 0.0f, TyreWidthCm = 0.0f;
	float WheelDustScale = 1.0f, WheelDustAmount = 1.0f, WheelDustSpacing = 1.0f, TyreMarkDark = 1.0f, TyreMarkDepth = 1.0f, TyreMarkLug = 1.0f;
	float TyreMarkSegmentK = 1.0f, WheelDustTrailS = 0.0f;
	void SetupWheels(const FRTSUnitVisual& Visual);
	/** (30/10) A farda, a cor da facção e o fuzil na mão do soldado de verdade. */
	void SetupSoldierLook(const FRTSUnitVisual& Visual);
	/** (30/10) O capacete no alto da cabeça do soldado (vivo ou caindo). */
	void PlaceSoldierHelmet();
	void SetupSoldierPaintTint(const FRTSUnitVisual& Visual);
	/** (30/10) A bandeira: cria o pano (no Setup) e o move no vento aparente a cada quadro. */
	void SetupFlag(const FRTSUnitVisual& Visual);
	/** (31/10, o teste de leitura) A tinta só desta unidade (FRTSUnitVisual::PaintTint) nas peças com a tinta compartilhada. */
	void ApplyPaintTint(const FRTSUnitVisual& Visual);
	void StepFlag(float DeltaSeconds);

	// (23/10, v4) o lançador que levanta (o bloco "launcher"): o pivô da elevação na dobradiça (filho do pivô da torre: levanta e gira
	// junto), as malhas que ele leva (o berço com os tubos e as pontas dos mísseis), as bocas dos tubos deitados (cm, no quadro das
	// malhas), a ponta do míssil de cada tubo e a hidráulica
	UPROPERTY() TObjectPtr<USceneComponent> ElevPivot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LauncherParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LauncherNoses;
	FVector LauncherHingeCm = FVector::ZeroVector;
	TArray<FVector> LauncherTubesCm;
	/** (01/11) os cilindros de elevação: o pivô no pino da mesa (filho do pivô da torre) com o cilindro e o estágio do meio, o pivô no
	 *  olhal do berço (filho do pivô da elevação) com a haste; onde ficam deitados (cm, no quadro das malhas) */
	UPROPERTY() TObjectPtr<USceneComponent> RamPivot;
	UPROPERTY() TObjectPtr<USceneComponent> RamRodPivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RamMid;
	FVector RamBaseCm = FVector::ZeroVector, RamTipCm = FVector::ZeroVector;
	void UpdateRams();
	float LauncherFirePitchDeg = 45.0f, LauncherRaiseDps = 60.0f, LauncherAccelDps2 = 180.0f, LauncherBrakeDps2 = 360.0f;
	float LauncherLowerDps = 30.0f, LauncherLowerDelayS = 2.0f, LauncherStopHz = 2.5f, LauncherStopZeta = 0.35f;
	/** O cilindro (graus e graus/s), o berço preso nele pela mola do batente (graus e graus/s), o tempo desde o último alvo (s), o
	 *  relógio da mira da simulação e o tempo cheio dele (s; bLauncherSimClock: ela tem o relógio, visto uma vez acima de zero;
	 *  bLauncherAimFalling: o relógio está caindo, sem alvo), o tubo do tiro de agora, os tubos já atirados (bits; as pontas
	 *  escondidas) e a arma da simulação sem espera. */
	float LauncherAct = 0.0f, LauncherActRate = 0.0f, LauncherPitch = 0.0f, LauncherRate = 0.0f, LauncherSinceAim = 1.0e3f, LauncherAimS = 0.0f;
	float LauncherPreAttackS = 0.0f;
	// (25/10, v5) o berço de canhões (as bocas alternam; a elevação pela aeronave, da pose da malha até o máximo), as células dos mísseis
	// (a próxima da vez), a arma leve e onde a lagarta toca o chão (cm; zero = os pontos do tanque)
	bool bLauncherGuns = false, bLauncherAimAir = false;
	float LauncherRestPitchDeg = 0.0f, LauncherMaxPitchDeg = 75.0f, GunAimPitchDeg = -1000.0f, LauncherHeldPitchDeg = 0.0f;
	TArray<FVector> VlsCm;
	int32 VlsNext = 0;
	bool bLightGun = false;
	bool bLaunchCanister = false;
	float LightGunFlashCm = 30.0f, LightGunTracerCm = 260.0f, LightGunSpreadCm = 250.0f;
	/** (30/10, o recuo do cano; docs/ESTUDO_IMPACTOS.md I) as peças do cano (a malha "Barrel" da torre), onde elas descansam, e quanto o cano
	 *  entrou na torre agora (cm; o tiro põe, volta na mola). Sem cano separado, recua a torre inteira (o de antes). */
	TArray<TWeakObjectPtr<UStaticMeshComponent>> BarrelParts;
	TArray<FVector> BarrelRestCm;
	float BarrelRecoilCm = 0.0f;
	FVector2D TrackContactCm = FVector2D::ZeroVector;
	int32 LauncherTube = 0;
	uint32 LauncherFired = 0u;
	bool bLauncherAim = false, bLauncherSimClock = false, bLauncherAimFalling = false, bLauncherUp = false;
	void SetupLauncher(const FRTSUnitVisual& Visual);
	void StepLauncher(float DeltaSeconds);
	/** A boca do tubo Index e a direção dele (mundo, cm), na pose de agora. */
	bool LauncherTubeWorld(int32 Index, FVector& OutCm, FVector& OutDir) const;
	/** O tubo do lado de SimCm (o lado da torre no chão; INDEX_NONE sem lançador). */
	int32 LauncherTubeOnSide(const FVector& SimCm) const;
	/** Os voos novos (os últimos 4 disparos): o míssil, a diferença da boca desenhada pro ponto da simulação e a direção do tubo. */
	/** (01/11) bSoft: o lançamento suave de dentro do transporte (NotePassengerShot), com a boca da escotilha no mundo (MouthCm) */
	struct FLaunchBlend { uint32 Id = 0; FVector OffsetCm = FVector::ZeroVector; FVector Dir = FVector::ForwardVector; FVector MouthCm = FVector::ZeroVector; bool bSoft = false; };
	FLaunchBlend LaunchBlends[4];
	int32 LaunchBlendNext = 0;
	/** As pontas dos mísseis: a do tubo atirado escondida (LauncherFired), as outras à vista. */
	void ShowLauncherNoses();
	/** O topo do lançador levantado (cm do chão, no quadro do casco): a sombra do veículo no mapa do sol cresce junto. */
	float LauncherTopCm() const;
};
