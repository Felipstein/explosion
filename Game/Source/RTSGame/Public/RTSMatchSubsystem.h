#pragma once

// A ponte entre a simulação e a Unreal numa partida (docs/JOGO.md §3.1):
//  - roda o SimCore em passos fixos de 1/30 s (acumulador, Fiedler "Fix Your Timestep!");
//  - guarda o passo anterior e o atual e entrega a pose interpolada pro desenho;
//  - leva os cliques do jogador local e as ordens da IA pela fila de comandos;
//  - transforma os eventos (tiro, impacto, morte) em efeitos;
//  - grava o replay em Saved/Replays.
// Nada aqui decide a partida.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSGameData.h"
#include "RTSMatchSubsystem.generated.h"

class ARTSUnitActor;
class URTSFogOfWar;
class URTSGroundMemory; class FRTSMissileSmoke; class FRTSHitLife; struct FRTSHitLifeCtx; class FRTSLab; struct FRTSLabFxRequest;   // (numa linha só: o GENERATED_BODY não muda de linha; v5: o laboratório, RTSLab.h)
class ARTSDockActor;
struct FRTSBakedMap;
namespace Sim
{
	struct MatchSetup;
	struct Command;
	struct SimEvent;
}
class UInstancedStaticMeshComponent; class UStaticMesh;   // (30/10: numa linha só, o GENERATED_BODY não muda de linha)
struct FRTSUnitVisual;
struct FRTSMatchState;

UENUM()
enum class ERTSMatchResult : uint8
{
	Playing,
	Victory,
	Defeat,
	Draw,
};

/** Uma unidade como o desenho e o HUD veem (já interpolada). */
/** (16/10) Um míssil em voo (a Patriot), interpolado no quadro, no chão de verdade: pro corpo, a chama e o rastro de
 *  fumaça (o BlastFx do Fluxo). O voo é da simulação (igual em todo PC); Id = o do disparo e do acerto. */
/** (16/10) O desenho provisório de um carro esmagado ou de uma árvore caindo (os donos refazem: o Look, o WorldForge). */
struct FRTSPropFx
{
	int32 Prop = -1;
	TWeakObjectPtr<AActor> Proxy;
	bool bTree = false;
	FTransform Base;
	// a árvore: a vara que tomba em volta do pé
	FVector Pivot = FVector::ZeroVector;
	FVector Axis = FVector::ForwardVector;
	float Theta = 0.0f;
	float Omega = 0.0f;
	float MaxTheta = 1.52f;
	float LengthM = 10.0f;
	// o carro: o achatado e a torção indo pro alvo
	float Squash = 1.0f, SquashTo = 1.0f;
	float Tilt = 0.0f, TiltTo = 0.0f;
	float Roll = 0.0f, RollTo = 0.0f;
	bool bDone = false; bool bImpacted = false;
	/** O nome da malha inteira (pra achar a amassada do Look). */
	FString BaseMesh;
};

/** (16/10) Um poste da vila e os fios dele (só desenho: o tanque derruba pelas posições da simulação). */
struct FRTSPole
{
	int32 Piece = -1;
	FVector2D PosCm = FVector2D::ZeroVector;
	float HeightM = 8.0f;
	bool bDown = false; bool bAirArcPending = false;   // (17/10) caiu à vista com fio inteiro: falta o clarão no ar a ~17°
	TArray<FVector> Sockets;
	TArray<int32> Wires;
};

/** (16/10) Uma peça pequena da vila que amassa sob o veículo (só desenho). */
struct FRTSSmallProp
{
	int32 Piece = -1;
	FVector2D PosCm = FVector2D::ZeroVector;
	float RadiusCm = 60.0f;
	float SquashTo = 0.3f;
	bool bCrushed = false;
};

struct FRTSWire
{
	int32 PoleA = -1;
	int32 SocketA = -1;
	int32 PoleB = -1;
	int32 SocketB = -1;
	TArray<int32> Segments;
	bool bCut = false; int32 Handle = -1; int32 Follow = -1; int32 Follow2 = -1; FVector EndA = FVector::ZeroVector; FVector EndB = FVector::ZeroVector; float SagCm = 0.0f;
};

struct FRTSMissile
{
	uint32 Id = 0;
	uint32 Shooter = 0;
	FVector PosCm = FVector::ZeroVector;
	/** A direção do voo (unitária). */
	FVector Dir = FVector::ForwardVector;
	float SpeedCmS = 0.0f;
	/** Segundos desde o disparo (o impulso da saída, a chama mais forte no começo). */
	float AgeS = 0.0f;
	/** (01/11) Saindo do tubo no lançamento suave, com o motor de voo ainda apagado: sem a chama, a luz e o rastro. */
	bool bCoast = false;
};

struct FRTSUnitState
{
	uint32 Id = 0;
	uint8 Owner = 0;
	uint16 Def = 0;
	FVector2D PosCm = FVector2D::ZeroVector;
	float HullYawDeg = 0.0f;
	float TurretYawDeg = 0.0f;
	int32 Hp = 0;
	int32 MaxHp = 1;
	bool bMoving = false; uint8 Posture = 0;   // (simulação 29) o soldado: 0 de pé, 1 ajoelhado (suprimido), 2 encolhido (preso pelo fogo); numa linha só (o GENERATED_BODY abaixo não muda de linha)
	// economia e construção
	bool bStructure = false;
	/** A obra: 0 recém-marcada, 1 pronta. */
	float BuildFraction = 1.0f;
	/** Lado pra onde o prédio olha (0 +X, 1 +Y, 2 −X, 3 −Y). */
	int32 Dir = 0;
	int32 QueueSize = 0;
	int32 QueueHead = -1;
	float QueueFraction = 0.0f;
	/** O que o Dozer ou o caminhão está fazendo (Sim::UnitTask). */
	uint8 Task = 0;
	int32 Cargo = 0; bool bSelling = false; float SellFraction = 0.0f; int32 AimTicks = 0; float AimFraction = 0.0f;   // (v4, 23/10) vender: sendo vendido e quanto da desmontagem já andou (0..1); a mira antes do tiro (o lançador): os passos do relógio da simulação e a fração dele (0..1, entre os passos), o mesmo número do tiro; numa linha só (o GENERATED_BODY abaixo não muda de linha)
	bool bHasRally = false; bool bAir = false; bool bAirborne = false; float AltCm = 0.0f; float AirPitchDeg = 0.0f; float AirRollDeg = 0.0f; float Rotor01 = 0.0f; uint8 Flight = 0; int32 Clip = 0; int32 ClipSize = 0; int32 Clip2 = 0; int32 ClipSize2 = 0; float RearmFraction = 0.0f; bool bPatrol = false; bool bReturning = false; bool bStarving = false; uint32 AirBase = 0; int32 AirPad = -1;   // (v5, 24/10) o ar (docs/AEREO.md §4.3): a aeronave, no ar agora, a altura sobre o chão (cm, entre os passos), a inclinação que a física pede (graus: o nariz e a rolagem), o rotor (0..1), o estado de voo (Sim::FlightState), a munição das 2 armas (os pontinhos), o rearme na vaga (0..1), a patrulha, a volta, sem Airfield, e a vaga (o Airfield e o índice); numa linha só (o GENERATED_BODY abaixo não muda de linha)
	FVector2D RallyCm = FVector2D::ZeroVector; uint32 Inside = 0; int32 InsideSlot = -1; uint32 Boarding = 0; int32 Passengers = 0; int32 TransportSlots = 0; bool bUnloading = false; bool bAiming = false;   // (simulação 35) o transporte: em quem o soldado está (0 = no mapa) e a vaga dele, indo entrar em quem; no transporte, quantos leva e as vagas, desembarcando; e mirando alguém agora; numa linha só (o GENERATED_BODY abaixo não muda de linha)
};

/** Um tipo de unidade ou prédio como o HUD vê (os números do Units.json). */
struct FRTSDefInfo
{
	FString Key;
	FString Name;
	bool bStructure = false;
	bool bArmed = false;
	int32 Cost = 0;
	float BuildSeconds = 0.0f;
	int32 Energy = 0;
	FVector2D SizeM = FVector2D::ZeroVector;
	int32 SupplyBoxes = 0;
	bool bSupplyCenter = false;
	TArray<uint16> Produces;
	TArray<uint16> Constructs;
	TArray<uint16> Requires; int32 RequiresRank = 0; int32 MaxPerPlayer = 0; float AirliftPeriodS = 0.0f; bool bAir = false; bool bJet = false; float AirRangeM = 0.0f; float GroundRangeM = 0.0f; int32 Pads = 0; bool bHelipad = false;   // (v5, 24/10) o ar: a aeronave, o jato, o alcance de ar e de chão (m; o círculo do HUD), as vagas e o heliponto do prédio   // (v4, 22/10) a patente que o tipo exige (0 = nenhuma), o teto por jogador (0 = sem) e o intervalo do avião de carga da zona de lançamento (s; 0 = não é zona); numa linha só: o GENERATED_BODY abaixo não muda de linha (o cc_check usa o .generated.h da última compilação)
};

/** Um depósito de suprimentos (ou a pilha que um caminhão destruído soltou). */
struct FRTSDockState
{
	FVector2D PosCm = FVector2D::ZeroVector;
	FVector2D HalfCm = FVector2D::ZeroVector;
	int32 Boxes = 0;
	int32 StartBoxes = 0;
	bool bPile = false;
};

/** Por que o prédio não cabe (o mesmo que a simulação responde: Sim::PlaceResult). */
UENUM()
enum class ERTSPlace : uint8
{
	Ok,
	NotAStructure,
	OutOfMap,
	NearEdge,
	Blocked,
	NotFlat,
	NearSupplies,
	BlocksExit,
	Occupied,
	NotVisible,
	MissingPrereq,
	NoMoney, LimitReached,   // (v4) LimitReached: o teto do tipo por jogador (a zona de lançamento: 2)
};

/** O "+$300" que sobe do centro de suprimentos quando o caminhão entrega (só o do jogador local). */
struct FRTSMoneyPopup
{
	FVector2D PosCm = FVector2D::ZeroVector;
	int32 Amount = 0;
};

/** "Fulano foi derrotado": o aviso pra todos (a cor, quando). */
struct FRTSDefeatNotice
{
	uint8 Player = 0;
	double At = 0.0;
};

UCLASS()
class RTSGAME_API URTSMatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	URTSMatchSubsystem();
	virtual ~URTSMatchSubsystem() override;

	/**
	 * Monta a partida no mapa carregado. Sozinho: você (jogador 0) contra uma IA. Na LAN
	 * (URTSNetSession): o anfitrião monta com todos humanos e manda a montagem; o cliente usa a
	 * que chegou (GivenSetup) e só a grade do desenho sai do próprio mapa.
	 */
	bool StartSkirmish(const FString& MapName, FString& OutError, const Sim::MatchSetup* GivenSetup = nullptr);
	/** Cliente da LAN: a montagem ainda não chegou (a partida começa quando chegar). */
	void AwaitNetSetup(const FString& MapName) { bNetAwaitSetup = true; NetMapName = MapName; }
	bool IsAwaitingNetSetup() const { return bNetAwaitSetup; }
	/** Partida em rede local. */
	bool IsNetMatch() const { return bNet; }
	/** A cena pronta, esperando os outros jogadores (a LAN). */
	bool IsWaitingForPlayers() const;
	/** A partida em rede parada (a pausa de alguém, ou a espera por quem sumiu). */
	bool IsNetPaused() const;
	/** Render-se (13/10): tudo do jogador local cai, e ele vira espectador. */
	void OrderResign();
	bool IsRunning() const;
	/** A cena acabou de carregar (shaders, texturas): só então a partida começa a contar. */
	bool IsReady() const { return bReady; }

	// --- névoa de guerra e minimapa (docs/BASE_JOGAVEL.md §0)
	/** A névoa está sendo desenhada (ligada e a partida rolando; no fim o mapa se revela, como no ZH). */
	bool IsFogShown() const;
	/** O liga/desliga de depuração (F9, botão da pausa): troca o desenho na hora e a regra pela fila. */
	void ToggleFogReveal();
	bool IsFogOn() const { return bFogDisplay; }
	/** A unidade aparece pro jogador local (as suas sempre; inimigo só à vista, com 1 s de carência). */
	bool IsUnitShown(uint32 Id) const;
	/** O ponto está à vista agora (pros efeitos: tiro, impacto, explosão). */
	bool IsPointShown(const FVector2D& Cm) const;
	/** (v4, 23/10) A torre solta da carcaça batendo no chão (RTSLooseTurret, só desenho): à vista, o empurrão na poeira e a
	 *  marca; na névoa, a marca espera a área aparecer. A velocidade de aproximação em cm/s, o espalhamento dos pontos em cm;
	 *  bWreckShown: a batida é de agora, com a carcaça (e a torre) à vista do jogador local; falso = só a marca (a guardada
	 *  pelo ator da morte que ninguém viu, mandada quando a carcaça aparece: a marca sozinha não entrega a morte na névoa).
	 *  (23/10, a areia do acerto) À vista, a areia da batida na poeira em volume (FRTSHitLife::OnTurretImpact; TangentSpeedCmS =
	 *  a velocidade no plano antes da batida, cm/s: a batida oblíqua joga a areia a jusante). */
	void TurretImpact(const FVector& AtCm, const FVector2D& SlideDir, float NormalSpeedCmS, float SpreadCm, bool bWreckShown, float TangentSpeedCmS = 0.0f);
	/** (23/10, a areia do acerto) A torre solta deslizando ou rolando na areia, à vista (RTSLooseTurret: um ponto a cada 0,5 m de contato):
	 *  a poeira da raspada (FRTSHitLife::OnTurretScrape). */
	void TurretScrape(const FVector& AtCm, const FVector2D& Dir, float SpeedCmS, int32 Index);
	URTSFogOfWar* GetFog() const { return Fog; }
	/** Onde cada jogador começou (todo mundo conhece pelo mapa): a seta da base inimiga. */
	FVector2D PlayerStartCm(uint8 Player) const;
	/** Posições das suas unidades que levaram tiro desde a última chamada (o "sob ataque" do minimapa). */
	void TakeDamageAlerts(TArray<FVector2D>& Out);

	void SetPaused(bool bInPaused);
	bool IsPaused() const { return bPaused; }

	// --- ordens do jogador local (viram comandos marcados com o passo)
	/** bFormation: o jogador segura o F (o grupo anda junto, no passo do mais lento, com o desenho que tinha).
	 *  bFacing: a linha dos dois botões (v4, 23/10: esquerdo + direito arrastando; a linha de frente virada pra FacingDeg, com
	 *  WidthM de largura, a fileira da frente em TargetCm; até uma vaga de largura, a do grupo). */
	void OrderMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bAttackMove, bool bFormation = false,
		bool bFacing = false, float FacingDeg = 0.0f, float WidthM = 0.0f);
	/** (v4, 23/10) Uma vaga da prévia da linha: a unidade, o ponto (cm) e o raio dela (cm) pro anel. */
	struct FLineSlot
	{
		uint32 Id = 0;
		FVector2D Cm = FVector2D::ZeroVector;
		float RadiusCm = 0.0f;
	};
	/** As vagas exatas que OrderMove(Units, TargetCm, ..., bFacing = true, FacingDeg, WidthM) daria agora: a mesma conta da
	 *  simulação (Sim::World::PreviewLineSlots), com o mesmo grupo e o mesmo arredondamento da ordem. Falso = nenhuma vaga. */
	bool PreviewLineMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bFormation, float FacingDeg, float WidthM, TArray<FLineSlot>& Out) const;
	/** O teste: o ponto de destino que a simulação deu à unidade (cm). */
	bool GetOrderPointCm(uint32 Id, FVector2D& Out) const;
	/** bForced: o tiro forçado do C&C (Ctrl), que vale contra a própria unidade e o aliado. */
	void OrderAttack(const TArray<uint32>& Units, uint32 TargetId, bool bForced = false);
	/** O tiro forçado no chão (Ctrl no chão): o grupo vai até o alcance e atira no ponto até mandarem outra coisa. */
	void OrderAttackGround(const TArray<uint32>& Units, const FVector2D& PointCm);
	void OrderStop(const TArray<uint32>& Units);
	// economia e construção (docs/ECONOMIA.md)
	void OrderProduce(uint32 Structure, uint16 Def);
	void OrderCancelProduce(uint32 Structure, int32 Index);
	void OrderBuild(const TArray<uint32>& Dozers, uint16 Def, int32 Dir, const FVector2D& Cm);
	void OrderResumeBuild(const TArray<uint32>& Dozers, uint32 Foundation);
	void OrderCancelBuild(uint32 Structure);
	void OrderRally(const TArray<uint32>& Structures, const FVector2D& Cm);
	void OrderGather(const TArray<uint32>& Trucks, int32 Dock);
	/** (v4, 23/10) Vender os prédios (o Sell do ZH): só os do jogador local, prontos e ainda não vendidos (a simulação confere de novo). */
	void OrderSell(const TArray<uint32>& Structures);
	/** (v4) O Dozer mais perto do grupo conserta o prédio (o DozerAIUpdate do ZH). */
	/** (simulação 35, 01/11, o pedido do Felipe: "atalho de Alt [...] uma FILA de tarefas") o Alt segurado: toda ordem que entra na fila
	 *  sai com a marca (Issue); o desenho da linha lê a fila de cada unidade (GetOrderPath). Kind: 0 andar, 1 atacar, 2 o trabalho do Dozer */
	struct FRTSOrderStep { FVector Cm = FVector::ZeroVector; uint8 Kind = 0; };
	void SetQueueOrders(bool bOn) { bQueueOrders = bOn; }
	bool IsQueueingOrders() const { return bQueueOrders; }
	double LastQueuedOrderSeconds() const { return LastQueuedOrderS; }
	void OrderTrimQueue(const TArray<uint32>& Units);
	/** (simulação 35) O transporte: os soldados do grupo entram em Transport (o do jogador local, com vaga); os transportes do grupo
	 *  desembarcam todos; e se a seleção tem soldado que cabe em Transport (o clique vira "entrar"). */
	void OrderEnter(const TArray<uint32>& Units, uint32 Transport);
	void OrderUnload(const TArray<uint32>& Units);
	/** (01/11) Tira só o passageiro Passenger do transporte Transport (a casa dele no painel); e quem está dentro, na ordem das vagas. */
	void OrderUnloadOne(uint32 Transport, uint32 Passenger);
	void GetPassengers(uint32 Transport, TArray<uint32>& Out) const;
	bool CanEnter(const TArray<uint32>& Units, uint32 Transport) const;
	bool GetOrderPath(uint32 Id, TArray<FRTSOrderStep>& Out) const;
	/** (01/11, o botão do trabalhador) Os construtores do jogador sem trabalho (sem obra, conserto ou sucata, sem ordem e sem fila), na ordem do Id. */
	void GetIdleBuilders(uint8 Player, TArray<uint32>& Out) const;
	void OrderRepair(const TArray<uint32>& Dozers, uint32 Structure); void OrderSalvage(const TArray<uint32>& Dozers, uint32 Scrap); uint32 SalvageAt(const FVector2D& Cm) const;   // (simulação 30) a sucata: o Dozer desmancha a carcaça ou o entulho em Cm (0 = nada ali)
	/** (v5, 24/10) O ar: a patrulha que volta em PointCm (só as aeronaves armadas do grupo) e o Voltar à base (o jato pousa e rearma,
	 *  o helicóptero de ataque conserta no heliponto). */
	void OrderPatrol(const TArray<uint32>& Units, const FVector2D& PointCm);
	void OrderReturnToBase(const TArray<uint32>& Units);
	/** (v5, 25/10) A guarda (G): a área em PointCm com o raio RadiusCm (0 = o do grupo), ou a escolta de Target (a unidade ou o prédio,
	 *  do jogador ou do aliado; o ponto é ignorado). Todas as unidades do grupo, do chão e do ar. */
	void OrderGuard(const TArray<uint32>& Units, const FVector2D& PointCm, uint32 Target, float RadiusCm);
	/** (v5, 25/10) O raio da guarda sem arraste pro grupo (m): o maior alcance das armas dele (World::GuardRadiusOf), o que a área do
	 *  HUD desenha antes do arraste. */
	float GuardRadiusM(const TArray<uint32>& Units) const;
	/** (v5, 25/10) A unidade está de guarda? O centro (cm; a escolta: onde o protegido está), o raio (cm) e o protegido (0 = a área). */
	bool GetGuard(uint32 Id, FVector2D& OutCenterCm, float& OutRadiusCm, uint32& OutTarget) const;
	/** (v5, 25/10) As vagas que sobram pra fila do Airfield (os jatos dele e os da fila contam); −1 = não é Airfield. O botão do
	 *  jato trava com 0 (o CANMAKE_PARKING_PLACES_FULL do ZH). */
	int32 AirfieldPadsLeft(uint32 Structure) const;
	/** (v5) Alguma unidade do grupo consegue atacar o alvo (a camada: o tanque não mira o helicóptero no ar)? */
	bool CanAnyTarget(const TArray<uint32>& Units, uint32 Target) const;
	/** (v5) O Airfield como o HUD vê: cada vaga (e o heliponto, a última) com quem está nela e como. State: 0 livre, 1 reservada
	 *  (a aeronave no ar ou no táxi), 2 estacionada rearmando ou consertando, 3 estacionada pronta. */
	struct FAirPadInfo
	{
		int32 State = 0;
		uint16 Def = 0;
		float Health = 1.0f;
		int32 Clip = 0;
		int32 ClipSize = 0;
		float Rearm = 0.0f;
	};
	struct FAirfieldInfo
	{
		TArray<FAirPadInfo> Pads;
		bool bHelipad = false;
		FAirPadInfo Helipad;
		/** A fila de jatos parada sem vaga ("Runways full"). */
		bool bRunwaysFull = false;
	};
	bool GetAirfield(uint32 Id, FAirfieldInfo& Out) const;
	/** (v4) O jogador local pode vender o prédio agora? E quanto volta ($). */
	bool CanSell(uint32 Structure) const;
	int32 SellRefund(uint32 Structure) const;
	/** (v4) Um Dozer do jogador local pode consertar o prédio agora (dele ou do aliado, pronto, avariado, sem estar sendo vendido)? */
	bool CanRepair(uint32 Structure) const;

	// --- (v4, 23/10) a patente e os poderes do general (o bloco da frente do HUD): o pedido do poder, o que o HUD lê e os
	// avisos que nascem dos eventos. Os tipos ficam aninhados aqui (um struct novo antes da classe mudaria a linha do
	// GENERATED_BODY, e o cc_check usa o .generated.h da última compilação)
	/** Um poder do general como o HUD vê, pro jogador local: os números do Units.json ("powers") e o relógio da simulação. */
	struct FPowerInfo
	{
		FString Key;
		FString Name;
		/** O ícone do HUD ("icon:artillery_strike"); None sem ícone nos dados. */
		FName Icon;
		int32 RequiresRank = 0;
		int32 UpgradeRank = 0;
		float CooldownS = 0.0f;
		/** Os mísseis e o raio do girassol na patente de agora (crescem sozinhos na UpgradeRank). */
		int32 Missiles = 0;
		float RadiusM = 0.0f;
		/** Até onde o estilhaço de cada míssil ainda chega, além do ponto de mira (m; a arma da munição). */
		float SplashM = 0.0f;
		bool bUnlocked = false;
		bool bUpgraded = false;
		/** Tem um prédio pronto e vivo que liga os poderes (o Command Center). */
		bool bGranter = false;
		/** Pode usar agora (o ponto confere à parte, no PowerRefusal). */
		bool bReady = false;
		/** O que falta da espera, em segundos e em fração (0 = pronto). */
		float ReadyInS = 0.0f;
		float Remaining = 0.0f;
	};
	/** O avião de carga de uma zona de lançamento pronta (o cartão da seleção). */
	struct FAirliftInfo
	{
		/** O relógio já anda (a simulação começa a contar quando a obra fica pronta). */
		bool bStarted = false;
		/** Segundos até o próximo pouso, o intervalo deste (o 1º conta da obra pronta) e a fração andada (0..1). */
		float LeftS = 0.0f;
		float PeriodS = 0.0f;
		float Progress = 0.0f;
		/** Os paletes do próximo pouso (a rota que amadurece: 3, 4, 5, 5...) e quanto valem. */
		int32 NextPallets = 0;
		int32 NextValue = 0;
		/** O aviso já saiu: o avião está no ar e pousa mesmo sem energia. */
		bool bInbound = false;
		/** Sem energia, antes do aviso: o relógio e a empilhadeira param. */
		bool bPaused = false;
		/** Caixas no pátio esperando a empilhadeira. */
		int32 BoxesOnPad = 0;
	};
	/** Um aviso do HUD que nasce de um evento da simulação: a patente nova (RankUp), o ataque chegando perto do que é seu
	 *  (PowerUsed do inimigo) e o avião de carga a caminho (AirliftInbound). */
	struct FHudNotice
	{
		enum class EKind : uint8 { Promoted, StrikeIncoming, AirliftInbound, AirNoRunway };   // (v5) a aeronave sem Airfield
		EKind Kind = EKind::Promoted;
		/** Segundos de mundo em que chegou. */
		double At = 0.0;
		/** A patente nova; o poder usado; os paletes do avião. */
		int32 Value = 0;
		/** Onde: o ponto do ataque, a zona. */
		FVector2D Cm = FVector2D::ZeroVector;
		float RadiusCm = 0.0f;
		uint8 Player = 0;
	};
	/** O poder Power (o índice em Rules.Powers) no ponto PointCm, pela fila de comandos (o lockstep); a simulação confere tudo. */
	void OrderUsePower(int32 Power, const FVector2D& PointCm);
	/** O XP inteiro e a patente (1..5) do jogador; o XP que a patente Rank pede (a 1 pede 0). */
	int32 PlayerXp(uint8 Player) const;
	int32 PlayerRank(uint8 Player) const;
	int32 RankXpThreshold(int32 Rank) const;
	/** Quantos poderes o general tem (os dados) e como está cada um pro jogador local. */
	int32 PowerCount() const;
	bool GetPowerInfo(int32 Power, FPowerInfo& Out) const;
	/** O poder da casa Slot da coluna (de baixo pra cima: os que a patente do jogador local já ligou, na ordem dos dados) e
	 *  da tecla F1 + Slot; −1 = nenhum. */
	int32 PowerAtSlot(int32 Slot) const;
	/** Por que o jogador local não pode usar o poder agora; com PointCm, também naquele ponto (fora do mapa, nunca
	 *  explorado). Vazio = pode (o mesmo que a simulação confere). */
	FText PowerRefusal(int32 Power, const FVector2D* PointCm = nullptr) const;
	/** A zona de lançamento Id do jogador local: o relógio do avião. Falso se não é zona, é de outro ou é obra. */
	bool GetAirlift(uint32 Id, FAirliftInfo& Out) const;
	/** O que a patente Rank libera, pelos dados: os poderes que ligam ou crescem nela e as unidades que ela destrava. */
	FText UnlocksAtRank(int32 Rank) const;
	/** Os avisos dos últimos 20 s, o mais novo no fim. */
	const TArray<FHudNotice>& HudNotices() const { return HudNoticeList; }
private:
	/** Junta os avisos do HUD dos eventos do passo (chamado logo depois do HandleEvents, nos dois caminhos do passo). */
	void HandleHudEvents();
	TArray<FHudNotice> HudNoticeList;
public:

	// --- leitura da economia pro HUD
	int32 Money(uint8 Player) const;
	int32 PowerMade(uint8 Player) const;
	int32 PowerUsed(uint8 Player) const;
	/** Ritmo da produção e da obra pela energia: 1 normal, 0,5 metade (ZH). */
	float ProductionSpeed(uint8 Player) const;
	int32 PlayerCount() const;
	bool IsDefeated(uint8 Player) const;
	/** O time de cada jogador e se é aliado do local (o mesmo time, sem ser ele). */
	uint8 TeamOf(uint8 Player) const;
	bool IsAllyOfLocal(uint8 Owner) const;
	/** O jogador local já caiu: assiste (não manda mais nada). */
	bool IsLocalDefeated() const;
	/** O time local inteiro caiu (ou era só ele): o espectador vê o mapa todo. Antes disso, só a
	 *  visão do time (escolha do Felipe, 11/10: não dá pra "soprar" pro aliado na mesma sala). */
	bool IsSpectatingAll() const;
	/**
	 * (25/10, o vento ao vivo) O vento único do mapa: pra onde sopra (graus, 0 = +X) e a velocidade a 10 m (m/s). Nasce do Maps.json
	 * (wind_yaw/wind_speed) e quem muda no meio (o painel do laboratório, rts.Wind) passa por aqui: o MPC_Wind (as palmeiras, as
	 * bandeiras), o BlastFx (a nuvem da ogiva, a areia que fica, a coluna da carcaça, a fumaça dos destroços), o rastro do míssil e a
	 * poeira em volume (as lagartas, os acertos, o rotor) leem dele. Só desenho: a simulação não vê o vento.
	 */
	void SetWind(float YawDeg, float SpeedMs);
	float GetWindYawDeg() const { return WindYawDeg; }
	float GetWindSpeedMs() const { return WindSpeedMs; }
	/** O vento do Maps.json desta partida (o "volta pro do mapa" do painel). */
	float GetMapWindYawDeg() const { return MapWindYawDeg; }
	float GetMapWindSpeedMs() const { return MapWindSpeedMs; }
	/** Os avisos de derrota (o HUD mostra os recentes). */
	const TArray<FRTSDefeatNotice>& DefeatNotices() const { return Defeats; }
	/** Onde foi a última morte à vista (o Espaço do espectador). */
	bool LastActionCm(FVector2D& Out) const { Out = LastAction; return bHasLastAction; }
	/** O nome da cor do jogador ("Blue", "Red"...): os avisos da partida (o nome do PC não diz nada). */
	static FText PlayerColorName(uint8 Player);
	int32 DefCount() const;
	bool GetDef(uint16 Def, FRTSDefInfo& Out) const;
	/** O jogador local tem os prédios que o tipo pede? */
	bool HasPrerequisites(uint16 Def) const;
	void GetQueue(uint32 Structure, TArray<uint16>& Out) const;
	int32 DockCount() const;
	/** $ por caixa (o valor do depósito na dica do mouse). */
	int32 BoxValue() const;
	bool GetDock(int32 Index, FRTSDockState& Out) const;
	/** O fantasma: onde o prédio fica de fato (centro e meio lado, cm) e se o jogador local pode pôr ali. */
	ERTSPlace CanPlace(uint16 Def, int32 Dir, const FVector2D& Cm, FVector2D& OutCenterCm, FVector2D& OutHalfCm) const;
	static FString PlaceReason(ERTSPlace Result);
	void TakeMoneyPopups(TArray<FRTSMoneyPopup>& Out);

	// --- leitura pro desenho, a câmera e o HUD
	uint8 LocalPlayer() const { return LocalSlot; }
	/** (31/10, o elenco das 3 facções) A facção de cada jogador ("usa", "china", "gla"; vazio fora da partida). */
	FString FactionOf(uint8 Player) const { return PlayerFactionKeys.IsValidIndex(Player) ? PlayerFactionKeys[Player] : FString(TEXT("usa")); }
	bool IsEnemyOfLocal(uint8 Owner) const;
	bool GetUnit(uint32 Id, FRTSUnitState& Out) const;
	void GetUnits(TArray<FRTSUnitState>& Out) const;
	const FRTSUnitVisual* VisualOf(uint16 Def) const;
	ARTSUnitActor* ViewOf(uint32 Id) const;
	/** (16/10) A batalha no minimapa (a camada do caderno de acúmulo; nula sem a memória do chão). */
	class UTextureRenderTarget2D* GetBattleOverlay() const;
	/** (16/10) Os mísseis em voo agora (interpolados entre os passos da simulação), pro desenho do míssil. */
	void GetMissiles(TArray<FRTSMissile>& Out) const;
	const FRTSBakedMap* BakedMap() const;
	FVector2D LocalStartCm() const;
	ERTSMatchResult Result() const { return MatchResult; }
	/** (v4, 23/10) O teste IA x IA: o último ataque de mísseis (qualquer jogador) e o último acerto de tiro, em cm, com o tempo
	 *  de partida deles; StrikeSerial conta os ataques (o teste vê quando sai um novo). */
	bool GetLastStrike(FVector2D& OutCm, float& OutAtS) const { OutCm = LastStrikeCm; OutAtS = LastStrikeAtS; return LastStrikeAtS >= 0.0f; }
	bool GetLastHit(FVector2D& OutCm, float& OutAtS) const { OutCm = LastHitCm; OutAtS = LastHitAtS; return LastHitAtS >= 0.0f; }
	int32 GetStrikeSerial() const { return StrikeSerial; }
	/** Tempo de partida (para no fim: o resultado mostra o tempo da vitória). */
	float MatchSeconds() const;
	/** Segundos de mundo desde que a partida começou / acabou (entrada, objetivo, letreiro do fim). */
	float SecondsSinceStart() const;
	float SecondsSinceEnd() const;
	int32 UnitsLost() const { return Lost; }
	int32 UnitsKilled() const { return Killed; }
	/** Cor de cada jogador no HUD (anel, barra de vida). */
	static FLinearColor PlayerColor(uint8 Player);
	/** As 8 cores da sala (o índice que cada um escolhe). */
	static FLinearColor PaletteColor(uint8 Color);
	/** Linha de depuração (passo, hash, ms por passo). */
	FString DebugLine() const;
	int32 GetShotsFired() const { return ShotsFired; }
	/** Escreve no log o estado de cada unidade (ordem, alvo, caminho): o teste chama no impasse. */
	void LogUnits(const TCHAR* Why) const;
	double LastDeathSeconds() const { return LastDeathWorldSeconds; }
	double GetLastStepMs() const { return LastStepMs; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

private:
	void StepOnce();
	void HandleEvents();
	void UpdateViews(float DeltaTime);
	void SpawnView(uint32 Id);
	/** Os depósitos e as pilhas caídas: aparecem e esvaziam com o estoque. */
	void UpdateDocks();
	/** O prédio destruído: a explosão principal e as secundárias espalhadas pela área. */
	void StructureDeath(const ARTSUnitActor& View, const FVector2D& CenterCm, uint32 Id);
	/** (30/10, o B do chão; docs/COMPARACAO_GAMESHOTS.md item 1) As pedras soltas do terreno (-RTSTerrainStones). */
	void PlaceTerrainStones();
	void SaveReplay();
	void IssueGroupIfChanged(const TArray<uint32>& Units);
	void UpdateFog(float DeltaTime);
	/** O que sobrou do derrotado cai em onda a partir do último prédio (BAR; e as explosões não saem todas no mesmo quadro). */
	void UpdateDying();
	void Die(ARTSUnitActor& View, const FVector2D& Cm, uint32 Id, uint8 Owner);

	/** O estado da partida (simulação, fila, IA, replay). Ponteiro cru: o tipo só existe no .cpp
	 *  (o código gerado da Unreal não aceita TUniquePtr de tipo incompleto). */
	FRTSMatchState* State = nullptr;

	UPROPERTY()
	TMap<uint32, TObjectPtr<ARTSUnitActor>> Views;
	/** (v5, 24/10) As aeronaves caindo (a simulação segue o corpo até o chão: Sim::World::CrashView): o ator e o dono, até o AirCrash. */
	TMap<uint32, TObjectPtr<ARTSUnitActor>> Falling;
	TMap<uint32, uint8> FallingOwner;
	void UpdateFalling(float DeltaTime);
	/** (24/10, o escopo B do ar; docs/AEREO.md §3.10) O desenho do ar na partida (no fim do RTSMatchSubsystem.cpp, "o ar: o desenho"):
	 *  o vento do rotor levantando a poeira em volume pelo tipo de chão (os puffs de impacto do RTSTreadDust, o jato de parede radial)
	 *  e empurrando a fumaça; o pós-combustor e a rampa; a fumaça e o fogo da queda; a sombra da aeronave no ar no mapa do sol. */
	void UpdateAirFx(float DeltaTime);
	void AddAirSmoke(struct FRTSSmokeFrame& Frame);
	void UpdateAirLook(uint32 Id, ARTSUnitActor& A, const FRTSUnitState& U);
	bool AirToSun(FVector& OutToSun);
	/** O vento do rotor de cada helicóptero: o que falta soltar (puffs) e o sorteio; os cartões vivos (o teto do preset). */
	struct FRotorWash { double Acc = 0.0; double LingerAcc = 0.0; uint32 Seed = 1; };
	TMap<uint32, FRotorWash> RotorWash;
	TArray<double> WashUntil;
	/** O fogo de verdade (a gravação do EmberGen em loop no raymarch, UBlastFxDirector::FireLoop): o da queda (o ator segue o motor) e o
	 *  da carcaça (nasce dentro da fumaça da morte), com a entrada, a saída e a coluna de fuligem por cima (RTSMissileSmoke). */
	struct FAirFire
	{
		TWeakObjectPtr<class AExplosionVolume> V;
		uint32 Id = 0;
		double Start = 0.0, End = 0.0;
		FVector FootCm = FVector::ZeroVector;
		float Scale = 0.17f;
		uint32 Variant = 0;
		bool bFalling = false;
		int32 FireId = -1;      // (25/10) o fogo simulado da carcaça (blast.Fire.Version 2; UBlastFxDirector::StartFire)
	};
	TArray<FAirFire> AirFires;
	FAirFire* FindAirFire(uint32 Id, bool bFalling);
	/** A 2ª bola do jato que cai (o combustível espirrando pra frente, alguns metros adiante e um instante depois). */
	struct FJetBlast { double At = 0.0; FVector Ground = FVector::ZeroVector; uint32 Variant = 0; };
	TArray<FJetBlast> JetBlasts;
	/** (25/10, v5) A arma leve e o estouro no ar (o Linebacker e o Stryker; no fim do RTSMatchSubsystem.cpp): o ponto do tiro na vítima
	 *  desenhada (pelo hash do tiro: o traçante e o acerto no mesmo lugar), o traçante pequeno, o estouro do antiaéreo (o clarão, as
	 *  faíscas e a baforada) e a baforada escura em volume no ar (um ponto que fumega ~0,22 s no RTSMissileSmoke, AddAirSmoke). */
	struct FFlakPuff { FVector Cm = FVector::ZeroVector; double Start = 0.0; uint32 Id = 0; float SigmaM = 0.3f; float ExtM2 = 0.15f; };
	TArray<FFlakPuff> FlakPuffs;
	uint32 FlakSerial = 0;
	FVector LightGunAim(class ARTSUnitActor* Victim, const FVector& FromCm, uint32 Seed) const;
	void LightGunTracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, uint32 Seed, float LengthCm);
	void FlakBurst(const FVector& AtCm, const FVector& FromCm, uint32 Seed);
	void AirBurstSmoke(const FVector& AtCm, float SigmaM, float ExtM2, uint32 Seed);
	/** (30/10, a metralhadora pelo fenômeno real: docs/ESTUDO_IMPACTOS.md G e docs/LINGUAGEM_DO_COMBATE.md) a rajada desenhada: cada tiro
	 *  da simulação da arma leve num alvo no chão (ou no chão) vira 2 balas desenhadas (a DShK e a M2 a ~10 tiros/s); a maioria cai na
	 *  areia em volta do alvo (as 3 primeiras da rajada caminham até ele) e ~1 em 3 bate no casco; o traçante em 1 de 4; o clarão e o
	 *  pulso de luz a cada bala (o pulso largo da metralhadora do ZH). Só o desenho: o dano é o da simulação. -BulletRounds=0 volta. */
	struct FBulletRound
	{
		double FireAt = 0.0, HitAt = 0.0;
		TWeakObjectPtr<ARTSUnitActor> Shooter;
		uint32 Target = 0;                       // a entidade (0 = o chão)
		FVector FromCm = FVector::ZeroVector;
		FVector ToCm = FVector::ZeroVector;      // onde ela cai (o casco ou a areia)
		uint32 Seed = 0;
		bool bHull = false, bTracer = false, bFired = false;
		bool bFlash = true;                      // (o custo) o clarão com luz na 1ª bala do par: 5 pulsos por segundo
	};
	TArray<FBulletRound> BulletRounds;
	struct FBulletBurst { double LastAt = -10.0; int32 Count = 0; };
	TMap<uint32, FBulletBurst> BulletBursts;
	TArray<double> BulletDustUntil;              // os puffs de areia das balas vivos no campo de poeira (o teto por preset)
	double BulletLogAt = 0.0;                    // (rts.BulletLog) o registro da rajada
	int32 BulletLogHull = 0, BulletLogGround = 0, BulletLogPuffs = 0;
	/** (30/10, a obra com vida) a próxima solda de cada obra (o relógio do mundo) e as faíscas e a poeira (UpdateConstructionFx). */
	TMap<uint32, double> ConstructionSparkAt;
	/** (30/10, 2ª volta) a solda de cada obra: o ponto, até quando o soldador fica nele e o próximo estalo (a sequência de estalos). */
	struct FWeld { FVector Cm = FVector::ZeroVector; FVector Out = FVector::ForwardVector; double Until = -1.0; double NextPop = 0.0; };
	TMap<uint32, FWeld> Welds;
	/** (30/10, 3ª volta) a última subida da lâmina de cada Dozer que já virou terra caindo (ARTSUnitActor::GetBladeLifts). */
	TMap<uint32, uint32> BladeLiftsSeen;
	void UpdateConstructionFx();
	/** (simulação 25, 30/10) os tiros que ricochetearam (o ShotRicochet chega antes do WeaponFired): o traçante e o acerto deles têm a cara do ricochete. */
	TSet<uint32> RicochetShots;
	/** (simulação 26, 30/10) os soldados esmagados neste passo (o UnitCrushed sai antes do UnitDied): a morte deles fica achatada. */
	TSet<uint32> CrushedIds;
	static bool BulletRoundsOn();
	static float BulletDustMass();
	void QueueBulletRounds(class ARTSUnitActor* Shooter, uint32 ShooterId, uint32 TargetId, class ARTSUnitActor* Victim, const FVector& GroundAimCm, float SpeedCmPerS, uint32 Seed);
	void UpdateBulletRounds();
	/** (30/10, a infantaria: "sangue SIM") as manchas de sangue no chão (o decal M_BloodDecal, aceso pela luz e pela sombra do mapa): o
	 *  respingo além de quem levou bala (Kind 1, no rumo dela) e a poça que cresce embaixo do corpo (Kind 0, seguindo o tronco enquanto ele
	 *  cai). Ficam LifeS, secam (perdem o brilho e escurecem) e somem nos últimos 4 s; teto de 96, a mais velha sai. */
	struct FBloodMark
	{
		TWeakObjectPtr<class UDecalComponent> Decal;
		TWeakObjectPtr<class UMaterialInstanceDynamic> Mid;
		TWeakObjectPtr<ARTSUnitActor> Body;
		double Born = 0.0;
		float GrowTauS = 0.1f, LifeS = 40.0f;
	};
	TArray<FBloodMark> BloodMarks;
	void AddBlood(const FVector& GroundCm, const FVector2D& Dir, float SizeCm, float Kind, float GrowTauS, uint32 Seed, ARTSUnitActor* FollowBody = nullptr);
	/** (30/10, item 37) o corpo de cada míssil em voo (a malha do atirador, guardada no 1º quadro: o atirador pode morrer antes do acerto). */
	struct FMissileLook
	{
		TWeakObjectPtr<UStaticMesh> Mesh;
		float LengthCm = 0.0f;
		float TrailWidthK = 1.0f, TrailDensityK = 1.0f, TrailS = 0.0f;
	};
	TMap<uint32, FMissileLook> MissileLooks;
	/** (01/11) Os mísseis do lançamento suave ainda com o motor apagado (a ignição no ar acontece quando saem daqui). */
	TSet<uint32> CoastingMissiles;
	const FMissileLook& MissileLookOf(const struct FRTSMissile& M);
	void UpdateBlood();
	FVector AirSunCached = FVector(0.0, 0.0, 1.0);
	double AirSunAt = -1.0;
	UPROPERTY()
	TArray<TObjectPtr<ARTSUnitActor>> Wrecks;
	/** A carcaça de veículo seguindo a da simulação (13/10, com física): a base no lugar da morte e o último desenho. */
	struct FWreckPose
	{
		TWeakObjectPtr<ARTSUnitActor> Actor;
		bool bInit = false;
		FVector2D BaseCm = FVector2D::ZeroVector;
		float BaseYaw = 0.0f;
		FQuat BaseRot = FQuat::Identity;
		float ZOff = 0.0f;
		FVector LocalOffset = FVector::ZeroVector;
		FVector2D LastCm = FVector2D::ZeroVector;
		float LastYaw = 0.0f;
		bool bMovedLogged = false;
		/** (22/10) O afundar desenhado (0..1). Saiu da simulação: o desenho termina de afundar a partir de RemovedFromP (a do
		 *  teto, ainda inteira, desce depressa) e aí some; na névoa vira fantasma parado até o lugar ser visto de novo. */
		float SinkP = 0.0f;
		double RemovedAt = -1.0;
		float RemovedFromP = 0.0f;
		bool bGhost = false;
		/** (31/10, a sucata) O passo em que o desenho viu o Dozer chegar no alcance (−1 = ninguém desmanchando agora) e se o fogo já é do
		 *  desmanche (UBlastFxDirector::WreckSalvage: desce com o casco e sai pelo fim simulado, até depois de a carcaça sumir). */
		int64 SalvageFrom = -1;
		bool bSalvaged = false;
	};
	TMap<uint32, FWreckPose> WreckPoses;
	/** (22/10) A carcaça some (o item 4 da v4): os eventos da simulação, o fim (tira das listas e destrói) e os fantasmas da
	 *  névoa (no máximo 32, os mais velhos somem sem aviso). */
	void OnWreckSinking(const Sim::SimEvent& E);
	void OnWreckRemoved(const Sim::SimEvent& E); void OnGroundClaimed(const Sim::SimEvent& E); void OnRuinCleared(const Sim::SimEvent& E);
	void FinishWreck(uint32 Id);
	TArray<uint32> WreckGhosts;
	/** As que a simulação tirou antes da morte desenhada (a onda do derrotado com mais de 64 de uma vez): afundam logo depois. */
	TSet<uint32> WreckGoneEarly;
	/** (16/10) Os carros esmagados e as árvores caídas sendo desenhados (o provisório), e de onde vem cada peça da vila. */
	TArray<FRTSPropFx> PropFx;
	TArray<FRTSPole> Poles;
	TArray<FRTSSmallProp> SmallProps;
	TArray<FRTSWire> WireFx;
	TWeakObjectPtr<UInstancedStaticMeshComponent> WireLayer;
	TMap<int32, TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>> VillagePieceInstance;
	void UpdateWreckPoses();
	/** (16/10) O raio-X: a fração escondida de cada unidade sua ou aliada (raios da câmera contra o que tapa de verdade). */
	void UpdateXRay(float DeltaSeconds);
	void BuildXRayGrid();
	float XRayClock = 0.0f;
	bool bXRayGridBuilt = false;
	int32 XRayW = 0, XRayH = 0;
	FVector2D XRayMinCm = FVector2D::ZeroVector;
	float XRayCellCm = 400.0f;
	float XRayMaxTopCm = 0.0f;
	/** O que tapa em cada célula: o sólido (o relevo, as casas e os muros) até SolidTop; a copa entre CanopyLo e CanopyHi. */
	TArray<float> XRaySolidTop;
	TArray<float> XRayCanopyLo;
	TArray<float> XRayCanopyHi;
	/** As peças da vila que tapam (as células e o topo), guardadas ao desenhar a vila pra grade do raio-X. */
	struct FXRayBlock
	{
		FIntRect Cells;
		float TopCm = 0.0f;
	};
	TArray<FXRayBlock> XRayVillage;
	/** (16/10) A foto dos tiros em voo depois do passo (pro desenho interpolar) e o provisório do voo do míssil. */
	void SnapshotShots();
	/** (16/10) O esmagar e o derrubar no desenho: o carro achata, a árvore cai (o provisório). */
	void OnPropCrushed(const Sim::SimEvent& E);
	void OnPropToppled(const Sim::SimEvent& E);
	FRTSPropFx* PropFxFor(int32 Prop);
	void UpdatePropFx(float DeltaSeconds);
	/** (16/10) Os fios soltos (o RTSWireSystem do WorldForge): a ponta segue a cruzeta do poste que cai; as rajadas do arco. */
	void UpdateWires();
	/** (16/10) O carro sendo esmagado: o fim antes da hora se o tanque parar ou morrer (o CarCrushEnd do Fluxo). */
	void UpdateCarPasses(float DeltaSeconds);
	/** (16/10) O arco do fio (o efeito é do Fluxo): Kind 0 = no ar (arrebentou, ou os condutores batendo no poste que
	 *  cai), 1 = a ponta viva no chão, 2 = a religada. */
	void WireArc(const FVector& PointCm, int32 Kind);
	bool bWireEventsBound = false;
	struct FWireBurst
	{
		double At = 0.0;
		FVector PointCm = FVector::ZeroVector;
		int32 Kind = 2;
		/** (17/10) O 2º clarão no ar: a cruzeta do poste caindo, lida na hora (o ponto anda). */
		int32 Pole = -1;
		int32 Socket = -1;
	};
	TArray<FWireBurst> WireBursts;
	TSet<int32> WireGrounded;
	/** (16/10, com o WorldForge) As pedras pequenas do mapa que partem sob o tanque (as que têm a malha _Broken). */
	struct FRTSRockSpot
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Ism;
		int32 Index = -1;
		FVector2D PosCm = FVector2D::ZeroVector;
		float RadiusCm = 50.0f;
		bool bBroken = false;
	};
	TArray<FRTSRockSpot> Rocks;
	TMap<FIntPoint, TArray<int32>> RockGrid;
	TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, TWeakObjectPtr<UInstancedStaticMeshComponent>> BrokenRockLayer;
	bool bRocksCollected = false;
	void CollectRocks();
	void BreakRock(int32 Rock);
	/** (01/11) o lote do prédio novo sem as pedras, as moitas e os tufos do mapa (as instâncias pequenas de ISM fora das unidades). */
	void ClearScatterUnder(uint32 Id);
	TMap<FIntPoint, TArray<TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>>> ScatterGrid;
	bool bScatterIndexed = false;
	/** (16/10) Os carros sendo esmagados agora (o efeito do Fluxo segue a passagem; acaba antes se o tanque parar ou morrer). */
	struct FCarPass
	{
		int32 Prop = -1;
		uint32 Tank = 0;
		double Until = 0.0;
		float Still = 0.0f;
	};
	TArray<FCarPass> CarPasses;
	/** (16/10) O míssil do BlastFx (o Fluxo) andando com o da simulação, a cada quadro. */
	void UpdateMissileFx();
	/** (16/10) A fumaça dos mísseis: o quadro (os mísseis, quem anda, os tiros, as unidades, a câmera) pro RTSMissileSmoke. */
	void UpdateMissileSmoke(float DeltaSeconds);
	/** (16/10) O mapa do sol comum de todas as fumaças (o RTSSunShadow da Arte): os veículos a cada quadro (lagarta, roda,
	 *  carcaças), os prédios prontos como cenário; as áreas que mudam vão no UpdatePropFx e no BreakRock. */
	void UpdateSunShadow();
	TSet<uint32> SunCasters;
	/** (16/10) O tanque derruba o poste (a cada passo, pelas posições da simulação) e os fios arrebentam com faísca. */
	void CheckPoles();
	void TopplePole(int32 Pole, const FVector2D& DirXY, float SpeedMs, bool bShown);
	/** A poeira das lagartas (13/10, o plugin RTSTreadDust da arte): um ator só, que recebe a cada quadro os veículos de
	 *  lagarta perto da câmera (só desenho; nada volta pra simulação). */
	TWeakObjectPtr<class ARTSTreadDust> TreadDust;
	/** (25/10) O vento de agora e o do mapa (SetWind). */
	float WindYawDeg = 0.0f, WindSpeedMs = 4.0f, MapWindYawDeg = 0.0f, MapWindSpeedMs = 4.0f;
	struct FDustMotion
	{
		FVector2D Cm = FVector2D::ZeroVector;
		float YawDeg = 0.0f;
		double Speed = 0.0;
		double YawRate = 0.0;
		bool bInit = false;
	};
	TMap<uint32, FDustMotion> DustMotion;
	int32 DustQuality = 3;
	void UpdateTreadDust(float DeltaTime);
	UPROPERTY()
	TArray<TObjectPtr<ARTSDockActor>> DockViews;
	/** As explosões secundárias do prédio que caiu (segundos de mundo, onde, variação). */
	struct FPendingBlast
	{
		double At = 0.0;
		FVector Ground = FVector::ZeroVector;
		uint32 Variant = 0;
	};
	TArray<FPendingBlast> PendingBlasts;
	/** As peças do derrotado esperando a vez na onda (o ator fica parado até explodir). */
	struct FDyingInfo
	{
		double At = 0.0;
		FVector2D Cm = FVector2D::ZeroVector;
		uint32 Id = 0;
		uint8 Owner = 0;
	};
	UPROPERTY()
	TArray<TObjectPtr<ARTSUnitActor>> DyingViews;
	TArray<FDyingInfo> DyingInfo;
	/** Por jogador: quando caiu (-1 = vivo), onde caiu o último prédio dele. */
	TArray<double> DefeatedAt;
	TArray<FVector2D> LastStructureDeath;
	TArray<FRTSDefeatNotice> Defeats;
	FVector2D LastAction = FVector2D::ZeroVector;
	bool bHasLastAction = false;
	/** A névoa "explorada" (o mapa aparece desde o começo; só desenho). */
	bool bFogExplored = false;
	/** Materiais e malhas dos efeitos presos na memória: o GC não os descarrega entre rajadas (o tiro seguinte travaria recarregando). */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> KeepLoaded;
	UPROPERTY()
	TObjectPtr<URTSFogOfWar> Fog;
	/** O cenário de teste rápido (-RTSScenario=<nome>, Config/RTS/Scenarios.json); as ordens já dadas. */
	bool bScenario = false;
	FRTSScenario Scenario;
	int32 NextScenarioOrder = 0;
	void IssueScenarioOrders(uint32 Now);
public:
	const FRTSScenario* GetScenario() const { return bScenario ? &Scenario : nullptr; }
	/** O teste: grava os cadernos da memória do chão na pasta. */
	void DumpGroundMemory(const FString& Folder);
	/** O preset: o chão que afunda de verdade (Alta e Ultra). */
	void SetGroundDisplacement(bool bOn);
	/** O preset da poeira das lagartas (0 baixa … 3 ultra; a nuvem cobre igual em todos). */
	void SetDustQuality(int32 Level);
	uint32 SimTick() const;

	// --- (v5, 24/10) o laboratório (RTSLab.h): a família Lab da simulação (só com MatchSetup::bLab; a LAN nunca tem), o
	// relógio, as IAs e os efeitos no cursor. Fora do laboratório nada disto é chamado e o jogo fica igual
	/** O laboratório desta partida (nulo fora dele). */
	FRTSLab* GetLab() const { return Lab; }
	/** A montagem é o laboratório (MatchSetup::bLab). */
	bool IsLab() const;
	/** Os comandos Lab (vão pela fila como qualquer ordem e entram no replay; a simulação confere tudo). YawDeg: pra onde nasce
	 *  virado (o prédio arredonda pro quarto de volta); bComplete: o prédio pronto (falso = a fundação que um Dozer termina). */
	void LabSpawn(uint16 Def, uint8 Player, const FVector2D& Cm, float YawDeg, bool bComplete);
	void LabRemove(uint32 Id);
	/** A vida vira Fraction (0..1) da cheia; 0 = a morte de sempre (a explosão, a carcaça). */
	void LabSetHealth(uint32 Id, float Fraction);
	void LabAddMoney(uint8 Player, int32 Amount);
	void LabSetRank(uint8 Player, int32 Rank);
	/** A ordem dada como outro jogador (o laboratório manda em todos): o grupo e a ordem (Type = Sim::CommandType: Move,
	 *  AttackMove, AttackGround, Attack, Stop) com o Player dele. */
	void LabOrderAs(uint8 Player, const TArray<uint32>& Units, uint8 Type, const FVector2D& Cm, uint32 Target, int32 Value = 0);
	/** O tipo nasce pelo LabSpawn (tem vida e não é a munição de um poder, como o míssil do ataque)? */
	bool LabCanSpawn(uint16 Def) const;
	/** O prédio cabe ali pelo LabSpawn (Sim::World::CanLabPlace: o chão vale, o dinheiro, a patente e a névoa não)? */
	ERTSPlace LabCanPlace(uint16 Def, int32 Dir, const FVector2D& Cm, bool bComplete, FVector2D& OutCenterCm, FVector2D& OutHalfCm) const;
	/** O relógio parado do laboratório (o passo segura; o desenho para pela dilatação do mundo, no FRTSLab) e o passo a passo. */
	void LabSetHold(bool bHold);
	bool IsLabHold() const { return bLabHold; }
	void LabStep(int32 Ticks);
	/** A IA de cada jogador (o jogador sem cérebro ganha um) e o log das decisões dela. */
	void LabSetAI(uint8 Player, bool bOn);
	bool LabIsAI(uint8 Player) const;
	const TArray<FString>& LabAILog() const { return LabAILines; }
	/** Joga como outro jogador (o laboratório): a câmera, a névoa, o HUD e a seleção passam a ser dele. */
	void LabPlayAs(uint8 Player);
	/** Um efeito do jogo no ponto (só desenho: as chamadas que os eventos já fazem; RTSLab.h, ERTSLabFx). */
	void LabEffect(const FRTSLabFxRequest& R);
	/** A semente e o mapa da montagem (a cena gravada do laboratório). */
	uint64 LabSeed() const;
	FString LabMapName() const;
	/** (v5) A peça civil do catálogo do laboratório (FRTSLab::PropDefs, Style = o índice) no ponto, virada pra YawDeg (a frente
	 *  da malha; a casa e o muro no quarto de volta), pelo LabSpawnProp; bChain = o poste ligado pelos fios ao poste anterior.
	 *  O desenho nasce do evento da simulação (LabPropPlaced), igual no replay. Falso: fora do laboratório ou a malha não existe. */
	bool LabSpawnProp(int32 Style, const FVector2D& Cm, float YawDeg, bool bChain);
	/** O meio tamanho (cm, no quadro da malha: X e Y dela) e a altura (cm) da peça do catálogo, pela malha. */
	static bool LabPropSize(int32 Style, FVector2D& OutHalfCm, float& OutHeightCm);
	/** O poste do laboratório mais novo (o que o próximo poste ligado encontra): a posição; falso sem nenhum. */
	bool LabLastPoleCm(FVector2D& OutCm) const;
private:
	FRTSLab* Lab = nullptr;
	TArray<bool> LabAIOn;
	TArray<FString> LabAILines;
	bool bLabHold = false;
	int32 LabPendingSteps = 0;
	/** (v5) O desenho da peça civil que a simulação pôs (o evento LabPropPlaced): a malha na camada do laboratório, e o carro, a
	 *  palmeira, a miudeza que amassa e o poste com os fios entram nas listas da vila (o esmagar, o tombar e o fio que arrebenta
	 *  são os de sempre). */
	void OnLabPropPlaced(const Sim::SimEvent& E);
	TWeakObjectPtr<AActor> LabPropActor;
	TMap<FString, TWeakObjectPtr<UInstancedStaticMeshComponent>> LabPropLayers;
	/** As peças do laboratório no VillagePieceInstance (acima das da vila) e o último poste dele (o índice em Poles). */
	int32 LabNextPiece = 1000000;
	int32 LabLastPole = -1;
	/** O acerto de ogiva do laboratório (o ramo do ProjectileHit no chão, com a munição escolhida). */
	void LabMissileHit(const FRTSLabFxRequest& R, bool bHeavy);
private:
	/** A memória do chão: esteiras, queimados e crateras (só desenho; -RTSNoGround desliga, pro A/B). */
	UPROPERTY()
	TObjectPtr<URTSGroundMemory> GroundMemory;
	/** (31/10, "os destroços [...] não ficando na física no buraco, e os tanques passando por cima e voando") O chão DESENHADO (cm): o
	 *  terreno assado mais a cratera e o avental que o afundar mostra (URTSGroundMemory::CraterHeightCmAt). Pra quem pousa ou anda
	 *  no desenho (as pedras, o entulho, os pedaços da carcaça, as rodas); a simulação fica no terreno assado. */
	float VisualGroundZCm(const FVector2D& P) const;
	/** (31/10) O veículo assentado no chão desenhado pelas rodas (o casco e a inclinação no buraco); infantaria pelo pé. */
	void SeatOnCraters(const FRTSUnitState& U, bool bInfantry, FVector& InOutGround, FVector& InOutNormal) const;
	/** (simulação 35) O soldado dentro do transporte: escondido; quem mira ou atirou há pouco sobe da cintura pra cima na escotilha da vaga
	 *  dele, virado pro alvo (o clarão sai da boca da arma dele, onde ele está). */
	void PlacePassenger(ARTSUnitActor& A, const FRTSUnitState& U, float DeltaTime);
	/** (16/10) A fumaça dos mísseis (os puffs simulados; só desenho). */
	FRTSMissileSmoke* MissileSmoke = nullptr;
	/** (23/10) A explosão com vida, do lado do jogo (RTSHitLife.h: o pulso do acerto de míssil e as reações; só desenho). */
	FRTSHitLife* HitLife = nullptr;
	/** O que a vida do acerto lê do jogo (o mundo, a poeira, o BlastFx, o chão, a areia, a névoa, as unidades perto). */
	void FillHitLifeCtx(FRTSHitLifeCtx& Out);
	/** A pausa em rede já mostrada (o efeito parado junto) e o log do passo onde parou. */
	bool bNetPausedShown = false;
	bool bNetFrozenLogged = false;
	/** Os testes da LAN (N5): -RTSNetBotQuitAt=<passo> (o robô some sem avisar), -RTSNetBotResignAt=<passo>,
	 *  -RTSNetTestPause=<passo> (pausa e volta 4 s depois) e -RTSNetTestVote (vota pra derrubar quem sumiu). */
	void UpdateNetTests(class URTSNetSession* N);
	double NetTestPausedAt = -1.0;
	bool bNetTestPauseDone = false;
	bool bNetTestResigned = false;
	/** A vila desenhada pelo village.json (uma camada de instâncias por malha; uma vez por mundo). */
	TWeakObjectPtr<AActor> VillageActor;
	/** Os destroços em volta das carcaças (13/10; só desenho, junto da memória do chão). */
	TWeakObjectPtr<class ARTSDebrisField> Debris;
	void SpawnVillage(const FRTSMapInfo& Info);
	/** De onde veio o último tiro em cada unidade (cm): a cratera da morte aponta pra longe do atirador. */
	TMap<uint32, FVector2D> LastHitFrom;
	/** O espalhamento do tiro no chão (só desenho), sorteado no disparo e usado no impacto, na mesma ordem em todo PC. */
	TMap<uint32, TArray<FVector2D>> GroundShotSpread;
	/** (v4, 23/10) A explosão grande do Cruise Missile Strike vai no 1º, 3º, 5º... míssil de cada ataque (só desenho, pelos
	 *  eventos e pelo estado da simulação: igual em todo PC). A ordem sai do ataque, não do Id do tiro: o Id vem do mesmo contador
	 *  dos tiros de todo mundo, e com a batalha em volta um ataque de 6 chegava a 5 grandes. Cada ataque no ar, na ordem do uso
	 *  (o PowerUsed): os pontos de mira da simulação (o girassol, StrikeAimPoint, em Fix cru) e quantos mísseis já saíram; o
	 *  WeaponFired do míssil acha o ataque pelo próximo ponto. */
	struct FStrikeSalvo
	{
		TArray<FInt64Point> Aims;
		int32 Fired = 0;
		/** O passo em que o último míssil já saiu, com 1 s de folga: o ataque sai da lista mesmo se um ponto não bater. */
		uint32 EndTick = 0;
	};
	TArray<FStrikeSalvo> StrikeSalvos;
	/** O míssil do ataque (o Id do tiro) → a ordem dele no ataque (0 = o 1º); sai no ProjectileHit dele. */
	TMap<uint32, int32> StrikeMissileOrder;
	/** As mortes na névoa (13/10, a revisão da cratera): o avental e os destroços esperam a área ser vista, e
	 *  aparecem junto da carcaça (sem isso a carcaça surgia no chão limpo). */
	struct FHiddenMark
	{
		FVector2D Cm = FVector2D::ZeroVector;
		FVector2D From = FVector2D::ZeroVector;
		float YawDeg = 0.0f;
		/** (22/10) A carcaça da morte (as peças grandes dos destroços vão com ela). */
		uint32 Id = 0;
	};
	TArray<FHiddenMark> HiddenMarks;
	/** (16/10, P2.1 das marcas da batalha) o tiro no chão e o acerto no casco dentro da névoa: carimbados quando a área aparece. */
	struct FHiddenScar
	{
		FVector2D At = FVector2D::ZeroVector;
		FVector2D From = FVector2D::ZeroVector;
		float RadiusCm = 0.0f;
		float Scorch = 0.0f;
		float Crater = 0.0f;
		int32 Size = 0;
	};
	TArray<FHiddenScar> HiddenScars;
	double NextHiddenCheck = 0.0;
	void RevealHiddenMarks();
	bool bFogDisplay = true;
	bool bFogPassOn = false;
	uint32 LastVisionVersion = MAX_uint32;
	bool bLastFogShown = true;
	TArray<FVector2D> DamageAlerts;
	TArray<FRTSMoneyPopup> MoneyPopups;
	/** (simulação 30) O entulho de cada prédio que caiu: a pose, a planta, a altura (o espirrado vai até ~metade dela além da parede) e a
	 *  casca queimada (o ator morto), pra tirar tudo quando a simulação tira o entulho (a obra em cima, a sucata). */
	TMap<uint32, TTuple<FTransform, FBox2D, float, TWeakObjectPtr<ARTSUnitActor>>> RuinAreas;
	/** (31/10, a sucata) O entulho que um Dozer desmancha agora: o passo em que ele encostou (o monte desce no ritmo da simulação). */
	TMap<uint32, int64> RuinSalvageFrom;

	/** Segura a partida até a cena estar pronta (a tela de carregar continua até lá). */
	void UpdateWarmup();
	/** O comando local: pela fila (sozinho) ou pela rede (LAN). */
	void Issue(const Sim::Command& C);
	/** Um passo com os comandos que vieram da rede. */
	void StepNet();
	void TickNet(float DeltaTime);
	class URTSNetSession* Net() const;

	bool bNet = false;
	bool bQueueOrders = false;          // (simulação 35) o Alt segurado
	double LastQueuedOrderS = -1.0e9;   // quando saiu a última ordem com a marca (a linha fica 1,5 s)
	uint8 LocalSlot = 0;
	/** (31/10) A facção de cada jogador, na ordem dos lugares (montada no StartSkirmish). */
	TArray<FString> PlayerFactionKeys;
	bool bNetAwaitSetup = false;
	FString NetMapName;
	/** -RTSNetBot: o robô de teste joga pelo jogador local, mandando os comandos pela rede. */
	bool bNetBot = false;

	double Accumulator = 0.0;
	bool bPaused = false;
	bool bReady = false;
	double WarmStartSeconds = 0.0;      // relógio de verdade (FPlatformTime)
	double SettledSince = -1.0;
	int32 LastWantingTextures = -1;
	double LastTextureProgress = 0.0;
	bool bReplaySaved = false;
	ERTSMatchResult MatchResult = ERTSMatchResult::Playing;
	FVector2D LastStrikeCm = FVector2D::ZeroVector;
	FVector2D LastHitCm = FVector2D::ZeroVector;
	float LastStrikeAtS = -1.0f;
	float LastHitAtS = -1.0f;
	int32 StrikeSerial = 0;
	double LastStepMs = 0.0;
	TArray<uint32> LastSentGroup;
	int32 ShotsFired = 0;
	int32 Lost = 0;
	int32 Killed = 0;
	double StartWorldSeconds = 0.0;
	double EndWorldSeconds = -1.0;
	double LastDeathWorldSeconds = 0.0;
};
