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

	/** -RmCompare=<gravação>[,<gravação>...]: cada clique detona a de sempre e, do lado, uma gêmea
	 *  por gravação (Intermediate/Raymarch/<gravação>.bin; -RmCompare=1 = r128_v0). Vazio = desligado. */
	static const TArray<FName>& CompareSources();
	static bool CompareMode() { return CompareSources().Num() > 0; }

	/**
	 * A luz do fogo gravada na própria gravação que esta explosão toca (RaymarchSource: a do
	 * EmberGen etc.), no instante T da gravação: posição em cm locais e cor nas unidades do
	 * WebGL. Falso pras de sempre (a luz delas é a do UExplosionBake, a mesma curva).
	 * bPhysical: a curva já é física (os conversores de VDB), sem a compressão 0.44 do WebGL.
	 */
	bool RecordedLight(const AExplosionVolume* E, float T, FVector& OutPosCm, FLinearColor& OutColor, bool& bPhysical) const;

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
	/** As gravações carregadas: 0–2 = as variantes da Carga Pesada; depois, a da comparação. O índice é a chave dos quadros no pool. */
	TArray<BlastRaymarch::FBakeRef> Sources;
	TMap<FName, int32> SourceOf;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> Noise;

	/** As bolhas do detalhe de perto (Worley 128³, make_noise.py). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> Worley;

	struct FSlot
	{
		int32 Variant = -1;
		int32 Frame = -1;
		uint64 LastUse = 0;
		int32 Bricks = 0;       // blocos do atlas que o quadro ocupa
	};
	FSlot Slots[BlastRaymarch::MaxSlots];
	int32 UsedBricks = 0;
	uint64 TickCount = 0;
	TSet<TWeakObjectPtr<AExplosionVolume>> WarpLogged;
	bool bWasEmpty = true;
	// a névoa do mapa aplicada na explosão (BlastRaymarchSubsystem.cpp, SubmitFog)
	void SubmitFog();
	TWeakObjectPtr<class UExponentialHeightFogComponent> FogComp;
	bool bFogSearched = false;
	// a sombra das explosões no chão (BlastRaymarchSubsystem.cpp, SubmitShadow)
	void SubmitShadow(bool bAny);
	bool bShadowSetup = false;
	UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> ShadowTarget;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ShadowFunction;
	TWeakObjectPtr<class UDirectionalLightComponent> ShadowSun;
};
