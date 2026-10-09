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
#include "RTSMatchSubsystem.generated.h"

class ARTSUnitActor;
class URTSFogOfWar;
struct FRTSBakedMap;
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
};

UCLASS()
class RTSGAME_API URTSMatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	URTSMatchSubsystem();
	virtual ~URTSMatchSubsystem() override;

	/** Monta a partida no mapa carregado: você (jogador 0) contra uma IA. */
	bool StartSkirmish(const FString& MapName, FString& OutError);
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
	void OrderMove(const TArray<uint32>& Units, const FVector2D& TargetCm, bool bAttackMove);
	void OrderAttack(const TArray<uint32>& Units, uint32 TargetId);
	void OrderStop(const TArray<uint32>& Units);

	// --- leitura pro desenho, a câmera e o HUD
	uint8 LocalPlayer() const { return 0; }
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
	void SaveReplay();
	void IssueGroupIfChanged(const TArray<uint32>& Units);
	void UpdateFog(float DeltaTime);

	/** O estado da partida (simulação, fila, IA, replay). Ponteiro cru: o tipo só existe no .cpp
	 *  (o código gerado da Unreal não aceita TUniquePtr de tipo incompleto). */
	FRTSMatchState* State = nullptr;

	UPROPERTY()
	TMap<uint32, TObjectPtr<ARTSUnitActor>> Views;
	UPROPERTY()
	TArray<TObjectPtr<ARTSUnitActor>> Wrecks;
	/** Materiais e malhas dos efeitos presos na memória: o GC não os descarrega entre rajadas (o tiro seguinte travaria recarregando). */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> KeepLoaded;
	UPROPERTY()
	TObjectPtr<URTSFogOfWar> Fog;
	bool bFogDisplay = true;
	bool bFogPassOn = false;
	uint32 LastVisionVersion = MAX_uint32;
	bool bLastFogShown = true;
	TArray<FVector2D> DamageAlerts;

	/** Segura a partida até a cena estar pronta (a tela de carregar continua até lá). */
	void UpdateWarmup();

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
