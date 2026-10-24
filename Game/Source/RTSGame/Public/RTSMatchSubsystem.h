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
class URTSGroundMemory; class FRTSMissileSmoke; class FRTSHitLife; struct FRTSHitLifeCtx;   // (numa linha só: o GENERATED_BODY não muda de linha)
class ARTSDockActor;
struct FRTSBakedMap;
namespace Sim
{
	struct MatchSetup;
	struct Command;
	struct SimEvent;
}
class UInstancedStaticMeshComponent;
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
	bool bMoving = false;
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
	bool bHasRally = false;
	FVector2D RallyCm = FVector2D::ZeroVector;
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
	TArray<uint16> Requires; int32 RequiresRank = 0; int32 MaxPerPlayer = 0; float AirliftPeriodS = 0.0f;   // (v4, 22/10) a patente que o tipo exige (0 = nenhuma), o teto por jogador (0 = sem) e o intervalo do avião de carga da zona de lançamento (s; 0 = não é zona); numa linha só: o GENERATED_BODY abaixo não muda de linha (o cc_check usa o .generated.h da última compilação)
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
	void OrderRepair(const TArray<uint32>& Dozers, uint32 Structure);
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
		enum class EKind : uint8 { Promoted, StrikeIncoming, AirliftInbound };
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
	};
	TMap<uint32, FWreckPose> WreckPoses;
	/** (22/10) A carcaça some (o item 4 da v4): os eventos da simulação, o fim (tira das listas e destrói) e os fantasmas da
	 *  névoa (no máximo 32, os mais velhos somem sem aviso). */
	void OnWreckSinking(const Sim::SimEvent& E);
	void OnWreckRemoved(const Sim::SimEvent& E);
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
private:
	/** A memória do chão: esteiras, queimados e crateras (só desenho; -RTSNoGround desliga, pro A/B). */
	UPROPERTY()
	TObjectPtr<URTSGroundMemory> GroundMemory;
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

	/** Segura a partida até a cena estar pronta (a tela de carregar continua até lá). */
	void UpdateWarmup();
	/** O comando local: pela fila (sozinho) ou pela rede (LAN). */
	void Issue(const Sim::Command& C);
	/** Um passo com os comandos que vieram da rede. */
	void StepNet();
	void TickNet(float DeltaTime);
	class URTSNetSession* Net() const;

	bool bNet = false;
	uint8 LocalSlot = 0;
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
