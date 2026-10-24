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
	/** (16/10) O tranco de subir no carro esmagado: o nariz e o lado (graus por segundo na mola da suspensão). */
	void PlayBump(float PitchKickDeg, float RollKickDeg);
	FVector MuzzleDirection() const;
	/** O tiro: o recuo do canhão e o tranco do casco (bGunRecoil; o lançador de mísseis, a Patriot e o Tomahawk, sem) e o
	 *  clarão de boca provisório (-ShotFx=0). */
	void PlayFire(bool bGunRecoil = true);
	/** Morre: vira carcaça (e a torre voa: desde a v4, 23/10, um corpo rígido de verdade, RTSLooseTurret.h). Fica no mapa até a
	 *  simulação tirar (SetWreckSink). */
	void Die();
	/** (22/10) A carcaça de veículo afundando (0 inteira … 1 sumiu), pela simulação, sem relógio próprio: o casco desce a
	 *  altura dele + 30 cm (devagar no começo e no fim) e inclina 2–3°; a torre solta, pousada longe, desce junto (v4: a partir
	 *  de quando ela parou, até o topo dela ficar abaixo do chão onde encostou; no ar ela nunca afunda). */
	void SetWreckSink(float P01);
	float GetWreckSink() const { return WreckSinkP; }
	/** A carcaça à vista agora (SetWreckLive); a já vista e fora da vista é o "fantasma" parado da névoa. */
	bool IsWreckLive() const { return bWreckLive; }

	/** A cor do dono nas faixas e lonas (dado de primitiva 0–3 do M_Structure). */
	void SetPlayerColor(const FLinearColor& Color);
	/** Prédio: quanto da obra está feito (0..1; 1 = pronto) e se tem energia (luzes, giro). */
	void SetStructureState(float InBuildFraction, bool bInPowered);
	/** Caminhão: quantas caixas leva. */
	void SetCargo(int32 Boxes);
	/** Dozer: trabalhando na obra (a lâmina mexe). */
	void SetWorking(bool bInWorking);
	bool IsStructure() const { return bStructure; }
	/** A cara de obra (o material que corta e o andaime) ainda ligada: o desenho ainda não trocou pro prédio pronto. */
	bool IsBuildLookOn() const { return bBuildLook; }
	/** Altura do prédio (cm), pra a explosão e o fogo da morte. */
	float GetHeightCm() const { return HeightCm; }

	uint32 GetSimId() const { return SimId; }
	/** Névoa: some (com sombra e luz) ou volta. Só troca quando muda (recriar as peças custa). (v4) A carcaça de uma morte que
	 *  ninguém viu, quando aparece, leva pro chão as marcas guardadas da torre solta. */
	void SetFogHidden(bool bHide);
	/** As marcas de dano no material (0 = inteiro, 1 = quase morto). */
	void SetDamage(float Damage);
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
	bool GetLaunchBlend(uint32 MissileId, FVector& OutOffsetCm, FVector& OutDir) const;
	static constexpr float LaunchBlendSeconds = 0.6f;

	virtual void Tick(float DeltaSeconds) override;

private:
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
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SpinParts;
	TArray<float> SpinRpm;
	TArray<float> SpinAngleDeg;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scaffold;
	UPROPERTY() TObjectPtr<USceneComponent> BladePivot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CargoParts;
	int32 SilhouetteStencil = 0;
	bool bXRayOn = false;
	bool bUnitMask = false;
	void ApplyStencil();
	bool bStructure = false;
	bool bBuildLook = false;
	float BuildFraction = 1.0f;
	bool bPowered = true;
	/** A luz mostrada (0..1): pisca ao acender como lâmpada de galpão, apaga em 0,3 s. */
	float PowerShown = 1.0f;
	float PowerOnClock = 1.0f;
	float LastPowerSent = -1.0f;
	float LastCutSent = -1.0f;
	float HeightCm = 1000.0f;
	FBox2D FootprintCm = FBox2D(FVector2D(-500.0), FVector2D(500.0));
	float SparkClock = 0.0f;
	uint32 SparkSeed = 1;
	bool bWorking = false;
	float BladeClock = 0.0f;
	float BladeAmount = 0.0f;
	int32 CargoShown = -1;
	/** O quanto o corpo desceu (cm): o prédio que desaba e, desde 22/10, a carcaça de veículo afundando. */
	float SinkCm = 0.0f;
	// (22/10) a carcaça afundando: o progresso, a pose de onde o casco desce, a torre solta (onde pousou, quanto desce e, v4, o
	// progresso da carcaça quando ela começou a descer: a que ainda caía começa quando para)
	float WreckSinkP = 0.0f;
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

	// (23/10, v4) o lançador que levanta (o bloco "launcher"): o pivô da elevação na dobradiça (filho do pivô da torre: levanta e gira
	// junto), as malhas que ele leva (o berço com os tubos e as pontas dos mísseis), as bocas dos tubos deitados (cm, no quadro das
	// malhas), a ponta do míssil de cada tubo e a hidráulica
	UPROPERTY() TObjectPtr<USceneComponent> ElevPivot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LauncherParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LauncherNoses;
	FVector LauncherHingeCm = FVector::ZeroVector;
	TArray<FVector> LauncherTubesCm;
	float LauncherFirePitchDeg = 45.0f, LauncherRaiseDps = 60.0f, LauncherAccelDps2 = 180.0f, LauncherBrakeDps2 = 360.0f;
	float LauncherLowerDps = 30.0f, LauncherLowerDelayS = 2.0f, LauncherStopHz = 2.5f, LauncherStopZeta = 0.35f;
	/** O cilindro (graus e graus/s), o berço preso nele pela mola do batente (graus e graus/s), o tempo desde o último alvo (s), o
	 *  relógio da mira da simulação e o tempo cheio dele (s; bLauncherSimClock: ela tem o relógio, visto uma vez acima de zero;
	 *  bLauncherAimFalling: o relógio está caindo, sem alvo), o tubo do tiro de agora, os tubos já atirados (bits; as pontas
	 *  escondidas) e a arma da simulação sem espera. */
	float LauncherAct = 0.0f, LauncherActRate = 0.0f, LauncherPitch = 0.0f, LauncherRate = 0.0f, LauncherSinceAim = 1.0e3f, LauncherAimS = 0.0f;
	float LauncherPreAttackS = 0.0f;
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
	struct FLaunchBlend { uint32 Id = 0; FVector OffsetCm = FVector::ZeroVector; FVector Dir = FVector::ForwardVector; };
	FLaunchBlend LaunchBlends[4];
	int32 LaunchBlendNext = 0;
	/** As pontas dos mísseis: a do tubo atirado escondida (LauncherFired), as outras à vista. */
	void ShowLauncherNoses();
	/** O topo do lançador levantado (cm do chão, no quadro do casco): a sombra do veículo no mapa do sol cresce junto. */
	float LauncherTopCm() const;
};
