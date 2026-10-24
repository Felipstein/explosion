#pragma once

// Teste automático do fluxo inteiro, do jeito que o Felipe joga: cliques,
// arrastes e teclas sintéticos entregues ao Slate (o mesmo caminho do mouse de
// verdade, como o FForgeUITest do WorldForge). Menu → "Jogar Skirmish" → caixa
// de seleção → ordem de andar → ataque andando → batalha até o fim → "Voltar ao
// menu". Fotografa cada passo e confere no log ("RTS TEST ok/FALHOU ...").
//
// Linha de comando: -RTSAutoTest=skirmish -RTSCapture=<pasta>  (Game/Tools/game.ps1 view -Folder x)
// -RTSAutoTest=base: a partida com base do ZH, antes do exército: seleciona o Dozer, constrói a
// usina, o centro de suprimentos (perto do depósito) e a fábrica pelos botões do painel e com o
// clique no chão, espera a 1ª entrega, produz tanques e marca o ponto de encontro.

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RTSAutoTest.generated.h"

class ARTSPlayerController;
class FRTSTestInputGate;
class URTSMatchSubsystem;
struct FRTSUnitState;

UCLASS()
class RTSGAME_API URTSAutoTest : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	/** (16/10) A câmera que segue uma unidade na cena: quem e onde o foco está. */
	uint32 FollowId = 0;
	/** (16/10) A câmera da cena já entrou (a gravação começa daí, sem os segundos da câmera normal com o HUD). */
	bool bSceneCameraSet = false;
	FVector2D FollowCm = FVector2D::ZeroVector;
	bool bFollowInit = false;

	struct FStep
	{
		FString Name;
		TFunction<void()> Act;
		TFunction<bool()> Ready;
		TFunction<FString()> Check;    // vazio = ok
		float MinWait = 1.0f;
		float Timeout = 30.0f;
		bool bShot = true;
	};

	/** Entrada sintética em fila, um evento por quadro (o controle lê o mouse a cada quadro). */
	struct FInputEvent
	{
		enum class EType : uint8 { Move, Down, Up, KeyDown, KeyUp } Type = EType::Move;
		FVector2D Pos = FVector2D::ZeroVector;   // pixels da viewport
		FKey Key;
	};

	void Build();
	/** Os passos da base (ZH): constrói, coleta e produz antes do exército sair. */
	void BuildBase();
	enum class ESpot : uint8 { Behind, NearDock, Front };
	/** A sequência de construir um prédio: câmera, Dozer, botão, lugar, obra, pronto. */
	void AddBuildSteps(const FString& Label, const FString& Key, ESpot Spot);
	int32 DefIndex(const FString& Key) const;
	bool OwnOf(int32 Def, TArray<FRTSUnitState>& Out) const;
	bool ScreenOf(const FVector2D& Cm, FVector2D& Out, float UpCm = 150.0f) const;
	/** Onde cabe o prédio (a simulação responde): espiral a partir do ponto, só o que aparece na tela. */
	bool FindSpot(int32 Def, int32 Dir, const FVector2D& Anchor, float MinRadiusCm, float StartAngle, FVector2D& OutCm) const;
	FVector2D SpotAnchor(ESpot Spot, float& OutMinRadiusCm, float& OutAngle) const;
	int32 NearestDockToBase() const;
	/** A base inimiga viva mais perto da minha (com 4 jogadores, todos contra todos). */
	FVector2D EnemyBaseCm() const;
	/** A minha cor na sala (-1 sem sala). */
	int32 MyLobbyColor() const;
	void Finish();
	void PumpInput();
	void QueueClick(const FVector2D& Pixel, FKey Button = EKeys::LeftMouseButton);
	void QueueDrag(const FVector2D& From, const FVector2D& To);
	/** (v4, 23/10) O acorde da linha: o 1º botão desce em From, o 2º desce, 8 movimentos até To e, com bRelease, sobe o esquerdo
	 *  e depois o direito (um evento por quadro, como a mão). bRightFirst: o direito desce antes. */
	void QueueChord(const FVector2D& From, const FVector2D& To, bool bRightFirst, bool bRelease);
	void QueueUp(const FVector2D& At, FKey Button);
	void QueueKey(FKey K);
	FVector2D ToSlate(const FVector2D& Pixel) const;
	FVector2D ViewportFrac(const FVector2D& Frac) const;
	FVector2D ButtonCenter(FName Id) const;
	ARTSPlayerController* PC() const;
	URTSMatchSubsystem* Match() const;
	class URTSNetSession* NetSession() const;
	bool OwnCentroid(FVector2D& OutCm) const;
	void Shot(const FString& Name);

	FString Folder;
	FString ScriptName;
	bool bBase = false;
	/** -RTSScenario: o teste rápido (a partida pronta, fotos em segundos fixos de simulação). */
	bool bScenario = false;
	void BuildScenario();
	/** (v4, 23/10) -RTSAutoTest=aivsai (com -RTSSelfAI): a partida longa IA x IA no jogo; a câmera corre pro último ataque de
	 *  mísseis e pra última luta e fotografa (-RTSTestMinutes=, padrão 20). */
	bool bAIvsAI = false;
	void BuildAIvsAI();
	bool WatchTick();
	float WatchNextShot = 0.0f;
	float WatchShotAt = -1.0f;
	FString WatchShotName;
	int32 WatchStrikeSerial = 0;
	int32 WatchCycle = 0;
	TArray<float> WatchBurst;
	bool bWatchStarted = false;
	bool bLan = false;
	/** lanclient: a nossa tela entra na sala de um anfitrião sem tela (o robô joga por ele). */
	bool bLanClient = false;
	/** Quantos jogos na sala (-RTSNetPlayers=), a névoa esperada (-RTSTestNoFog) e o arquivo que
	 *  abre a sala pros robôs depois da cor escolhida (-RTSNetGate=). */
	int32 NetPlayers = 2;
	bool bExpectFog = true;
	FString GateFile;
	int32 ColorBefore = -1;
	int32 RallyTry = 0;
	/** -RTSNetAI=<n>: IAs que o anfitrião põe na sala; -RTSTestTeams: 2 contra 2 (a gente e a IA de um lado). */
	int32 NetAIs = 0;
	bool bTeams = false;
	/** (v4) -RTSTestMoney=<n>: o dinheiro inicial escolhido pelos botões (o menu ou a sala) e conferido na partida (0 = não mexe). */
	int32 TestMoney = 0;
	bool bSpectatorLogged = false;
	/** O botão da cor da minha linha na sala. */
	FName MyColorButton() const;
	TArray<FStep> Steps;
	int32 Index = 0;
	int32 Phase = 0;
	float Timer = 0.0f;
	float TotalTime = 0.0f;
	int32 Failures = 0;
	TArray<FInputEvent> Pending;
	/** Quadros desde que a fila de entrada esvaziou: com 3, o controle já leu o último evento (ele chega no quadro seguinte). */
	int32 InputQuietFrames = 0;
	TSet<FKey> Pressed;
	TSharedPtr<FRTSTestInputGate> Gate;
	bool bDummyInjecting = false;
	FVector2D LastSlatePos = FVector2D::ZeroVector;

	// medições
	TArray<float> FrameMs;
	FVector2D CentroidBefore = FVector2D::ZeroVector;
	FVector2D MinimapTarget = FVector2D::ZeroVector;
	int32 SelectionBefore = 0;
	double MinimapOrderOffCm = 1e12;
	float NextBattleShot = 0.0f;
	float NextPush = 0.0f;
	bool bStallLogged = false;
	float NextFollow = 0.0f;
	int32 BattleShots = 0;
	float NextReinforce = 0.0f;
	float NextWave = 0.0f;
	int32 MoneyBefore = 0;
	// (v4, 23/10) a linha dos dois botões: as vagas da prévia (quem e onde), a frente, e a câmera, as ordens, a seleção e os
	// destinos de antes de cada passo
	TArray<uint32> LineIds;
	TArray<FVector2D> LineSlotsCm;
	float LineFacingDeg = 0.0f;
	FVector2D LineCamCm = FVector2D::ZeroVector;
	int32 LineOrders = 0;
	TArray<uint32> LineSelection;
	int32 LineFrameStart = 0;
	bool bLineQueued = false;
	float LineStillFor = 0.0f;
	float LineLastTimer = 0.0f;
	TMap<uint32, FVector2D> LineOrderPoints;
	int32 VitrineShots = 0;
	float RetryAt = 0.0f;
	// (v4, 23/10) o conserto do Dozer e o vender pelos cliques (o cenário com "clicks": "repair_sell", o v4_cliques): a usina
	// avariada (pelos próprios tanques, o Ctrl), o Dozer, a vida quando o conserto começou e o dinheiro quando a venda começou
	uint32 RepairPlant = 0;
	uint32 RepairDozer = 0;
	int32 RepairHpStart = 0;
	int32 SellMoneyStart = 0;
	/** Os passos do conserto e do vender (BuildScenario; fora da base, que segue igual pros outros donos). */
	void AddRepairAndSellSteps();
	bool bActive = false;
	/** -RTSRecord: grava uma foto por quadro do combate (com -benchmark -fps=30, passo fixo) pra virar vídeo. */
	bool bRecord = false;
	bool bRecordDone = false;
	int32 RecordFrames = 0;
	float RecordTail = 0.0f;
	bool BattleFocus(FVector2D& OutCm) const;
};
