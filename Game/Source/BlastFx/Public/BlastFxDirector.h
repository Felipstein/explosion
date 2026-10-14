#pragma once

// O diretor das explosões no jogo: o que o UBlastLabSimulation faz pras
// explosões no simulador, sem o resto do laboratório (painel, armas de teste,
// passagens de avião). Dá às explosões o relógio, a vista da câmera (pra saber
// o quanto cada uma ocupa da tela) e o orçamento de detalhe; dispara a morte de
// tanque; aquece os shaders na abertura.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlastFxDirector.generated.h"

class AExplosionVolume;
class UExplosionBake;
class UMaterialInterface;

namespace BlastFx
{
	/** O quanto a deformação pelo que passa empurra a fumaça (BattleSmokeSim.h no BlastLab). Sem campo no jogo ainda. */
	constexpr float WarpMaxLocalM = 8.0f;
}

UCLASS()
class BLASTFX_API UBlastFxDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBlastFxDirector* Get(const UObject* WorldContext);

	/** O relógio das explosões (para com a partida pausada). */
	float SimDelta(float DeltaSeconds) const { return bPaused ? 0.0f : DeltaSeconds; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	/** Fração da meia altura da tela que a esfera ocupa (0 fora da vista), como no BlastLab. */
	float ScreenFraction(const FVector& Center, float Radius, bool& bInView) const;
	float AmbientLux() const { return 52000.0f; }

	/** A cor da poeira que a explosão levanta (a areia do deserto). */
	FLinearColor DustTint = FLinearColor(0.80f, 0.68f, 0.55f);

	/**
	 * A morte de um tanque: a Carga Pesada aprovada (uma das 3 variantes) na
	 * proporção medida contra o vídeo da cena herói (bola 0.55, poeira com 40% da
	 * altura e metade da vida; BlastLabSimulation.cpp Detonate). Variant escolhe a
	 * gravação e o ruído (sorteio só visual).
	 */
	AExplosionVolume* TankDeath(const FVector& Ground, uint32 Variant, bool bClassic = false);

	/** Desenha cada gravação invisível na frente da câmera por uns quadros: o shader compila antes da 1ª morte. */
	void Prewarm(const FVector& At);
	bool IsWarm() const { return bWarm; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	bool LoadAssets();
	AExplosionVolume* Spawn(const FVector& Ground, UExplosionBake* Bake, float SizeScale, float Seed);
	void UpdateView();

	UPROPERTY() TArray<TObjectPtr<UExplosionBake>> Bakes;
	UPROPERTY() TObjectPtr<UMaterialInterface> VolumeMaterial;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Alive;
	UPROPERTY() TArray<TObjectPtr<AExplosionVolume>> Warmups;

	bool bPaused = false;
	bool bWarm = false;
	float WarmClock = 0.0f;

	bool bHasView = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	FVector ViewRight = FVector::RightVector;
	FVector ViewUp = FVector::UpVector;
	float TanHalfX = 1.0f;
	float TanHalfY = 1.0f;
};
