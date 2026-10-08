#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlastRaymarch.h"
#include "BlastRaymarchSubsystem.generated.h"

class AExplosionVolume;
class UTexture;

/**
 * O lado do jogo do raymarch portado do WebGL (BlastRaymarch.h): carrega as
 * gravações da Carga Pesada na RAM, junta as explosões vivas que ele desenha,
 * decide quais quadros ficam na placa (um pool de slots, o menos usado sai) e
 * manda tudo pro render a cada quadro.
 *
 * Ligado por padrão (-Raymarch=0 volta pro Heterogeneous Volume): o volume da
 * Unreal fica escondido e as luzes, o clarão, a sombra no chão e a deformação
 * pelo que passa continuam do AExplosionVolume e do UBattleSmokeSubsystem. Vale
 * pra Carga Pesada e as armas feitas dela (granada a nuclear); os corpos da
 * EmberGen (morte de tanque) seguem no volume da Unreal.
 */
UCLASS()
class BLASTLAB_API UBlastRaymarchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Esta explosão é desenhada pelo raymarch (o volume da Unreal fica escondido). */
	static bool Handles(const AExplosionVolume* E);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBlastRaymarchSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	TSharedPtr<FBlastRaymarchExtension, ESPMode::ThreadSafe> Extension;
	BlastRaymarch::FBakeRef Bakes[3];

	UPROPERTY(Transient)
	TObjectPtr<UTexture> Noise;

	struct FSlot
	{
		int32 Variant = -1;
		int32 Frame = -1;
		uint64 LastUse = 0;
	};
	FSlot Slots[BlastRaymarch::MaxSlots];
	uint64 TickCount = 0;
	TSet<TWeakObjectPtr<AExplosionVolume>> WarpLogged;
	bool bWasEmpty = true;
};
