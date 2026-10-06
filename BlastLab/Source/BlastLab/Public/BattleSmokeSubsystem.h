#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "BattleSmokeSim.h"
#include "BattleSmokeSubsystem.generated.h"

class AActor;
class AExplosionVolume;
class UHeterogeneousVolumeComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureRenderTargetVolume;
class UVolumeTexture;

/**
 * O ar do campo de batalha: o fluido de reference/webgl/js/battlesmoke.js em
 * compute shaders (FBattleSmokeSim), mexido pelo que passa (avião, míssil,
 * disparo) e pelas explosões.
 *
 * Ele faz duas coisas:
 *  - DEFORMA as explosões gravadas que algo atravessa ou empurra (até
 *    MaxWarps de uma vez): a gravação continua tocando do começo ao fim, com o
 *    detalhe, a cor e o movimento dela, mas lida num ponto deslocado pelo vento
 *    do que passa e com o túnel que o corpo abre (BattleSmoke.usf MainWarp). O
 *    WebGL trocava a explosão por esta fumaça aos 3.3 s, e a troca se via: outra
 *    grade, outra cor, a nuvem presa ~15 s no ar;
 *  - leva a fumaça própria do que passa (o rastro do propelente do míssil),
 *    desenhada pelo mesmo Heterogeneous Volume das explosões.
 * Sem nada passando e sem rastro no ar, nada é simulado nem desenhado.
 */
UCLASS()
class BLASTLAB_API UBattleSmokeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBattleSmokeSubsystem* Get(const UObject* WorldContext);

	/**
	 * A explosão passa a ser deformada pelo que passa (até o fim dela). Falso se
	 * não há slot livre ou ela não tem a velocidade gravada.
	 */
	bool AttachWarp(AExplosionVolume* Explosion);

	/** O ponto (cm) está dentro do campo coberto pela grade. */
	bool Covers(const FVector& PositionCm) const;

	/** Algo atravessando o campo neste quadro (posições em m, eixos da Unreal). */
	void AddDisturber(const FBattleSmokeDisturber& D);

	/** Explosão nova: a expansão empurra a fumaça das outras pra fora (battlesmoke.js addBlast). */
	void AddBlast(const FVector& PositionCm, float Scale);

	/** Raio do pulso de expansão de uma explosão de escala Scale (m). */
	static float BlastRadiusM(float Scale) { return 9.0f * Scale; }

	/**
	 * Esteira de um avião que começa agora em StartM (m) voando em Dir: o par de
	 * vórtices (circulação Gamma, espaçamento B0) que fica no caminho, desce e
	 * enfraquece. Devolve o id pra estender enquanto ele voa.
	 */
	int32 BeginWake(const FVector& StartM, const FVector& Dir, float Speed, const FVector& Right, float B0, float Gamma);
	void ExtendWake(int32 Id, float LengthM);

	/** Caixa da fumaça (cm, mundo), lida da GPU; falso se o campo está vazio. */
	bool SmokeBox(FBox& Out) const;

	/** Altura média da fuligem no campo (cm, mundo), lida da GPU; falso se não há. */
	bool SootHeight(float& OutCm) const;

	bool IsIdle() const { return bIdle; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBattleSmokeSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void UpdateBox(const FIntVector& Min, const FIntVector& Max);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetVolume> FieldTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetVolume> VelTarget;

	UPROPERTY(Transient)
	TObjectPtr<UVolumeTexture> Noise;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FieldActor;

	UPROPERTY(Transient)
	TObjectPtr<UHeterogeneousVolumeComponent> Volume;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TSharedPtr<FBattleSmokeSim, ESPMode::ThreadSafe> Sim;
	TArray<FBattleSmokeDisturber> Disturbers;

	// explosões sendo deformadas: a saída de cada slot vai pro material dela
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTargetVolume>> WarpTargets;
	TWeakObjectPtr<AExplosionVolume> WarpOwners[BattleSmoke::MaxWarps];
	uint32 WarpResetMask = 0;
	struct FBlast
	{
		FBattleSmokeBlast Blast;
		float Until = 0.0f;
	};
	TArray<FBlast> Blasts;
	struct FWakeRecord
	{
		int32 Id = 0;
		FBattleSmokeWake Wake;
	};
	TArray<FWakeRecord> Wakes;       // as mais novas no fim
	int32 NextWakeId = 1;

	float Time = 0.0f;
	float MassEstimate = 0.0f;       // fumaça própria no campo (rastro de míssil; cai com a dissipação)
	bool bIdle = true;
	bool bNeedReset = true;
	FIntVector BoxMin = FIntVector(0), BoxMax = FIntVector(-1);    // células finas, inclusiva
};
