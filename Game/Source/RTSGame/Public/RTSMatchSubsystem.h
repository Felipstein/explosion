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
class URTSGroundMemory;
class ARTSDockActor;
struct FRTSBakedMap;
namespace Sim
{
	struct MatchSetup;
	struct Command;
}
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
	int32 Cargo = 0;
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
	TArray<uint16> Requires;
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
	NoMoney,
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
	URTSFogOfWar* GetFog() const { return Fog; }
	/** Onde cada jogador começou (todo mundo conhece pelo mapa): a seta da base inimiga. */
	FVector2D PlayerStartCm(uint8 Player) const;
	/** Posições das suas unidades que levaram tiro desde a última chamada (o "sob ataque" do minimapa). */
	void TakeDamageAlerts(TArray<FVector2D>& Out);

	void SetPaused(bool bInPaused);
	bool IsPaused() const { return bPaused; }

	// --- ordens do jogador local (viram comandos marcados com o passo)
	/** bFormation: o jogador segura o F (o grupo anda junto, no passo do mais lento, com o desenho que tinha).
	 *  bFacing: o arraste do esquerdo (a linha de frente virada pra FacingDeg, com WidthM de largura; 0 = a do grupo). */
	void OrderMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bAttackMove, bool bFormation = false,
		bool bFacing = false, float FacingDeg = 0.0f, float WidthM = 0.0f);
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
	const FRTSBakedMap* BakedMap() const;
	FVector2D LocalStartCm() const;
	ERTSMatchResult Result() const { return MatchResult; }
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
	};
	TMap<uint32, FWreckPose> WreckPoses;
	void UpdateWreckPoses();
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
	/** As mortes na névoa (13/10, a revisão da cratera): o avental e os destroços esperam a área ser vista, e
	 *  aparecem junto da carcaça (sem isso a carcaça surgia no chão limpo). */
	struct FHiddenMark
	{
		FVector2D Cm = FVector2D::ZeroVector;
		FVector2D From = FVector2D::ZeroVector;
		float YawDeg = 0.0f;
	};
	TArray<FHiddenMark> HiddenMarks;
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
	double LastStepMs = 0.0;
	TArray<uint32> LastSentGroup;
	int32 ShotsFired = 0;
	int32 Lost = 0;
	int32 Killed = 0;
	double StartWorldSeconds = 0.0;
	double EndWorldSeconds = -1.0;
	double LastDeathWorldSeconds = 0.0;
};
