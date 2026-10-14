// Cópia do BlastLab (commit a14c45a, 10/10/2026): a ponte até o plugin comum que a sessão do
// BlastLab vai fazer (docs/BASE_JOGAVEL.md). A original continua sendo a referência: mudanças
// de aparência nascem lá e são trazidas pra cá.
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
class BLASTFX_API UBlastRaymarchSubsystem : public UTickableWorldSubsystem
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
	/** As gravações carregadas: 0–2 = as variantes da Carga Pesada. O índice é a chave dos quadros no pool. */
	TArray<BlastRaymarch::FBakeRef> Sources;
	/** As gravações próprias pelo nome (a eg_tq_v2 da morte de tanque, 13/10): o índice delas em Sources. */
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

	// O encaixe na cena (do BlastLab, 13/10; no jogo desde 14/10): sem os dois a explosão parecia "um PNG em cima do
	// cenário" (o Felipe). A névoa do mapa aplicada na explosão (SubmitFog)
	void SubmitFog();
	TWeakObjectPtr<class UExponentialHeightFogComponent> FogComp;
	bool bFogSearched = false;
	// e a sombra das explosões no chão, nos tanques e nas casas (SubmitShadow)
	void SubmitShadow(bool bAny);
	bool bShadowSetup = false;
	UPROPERTY(Transient) TObjectPtr<class UTextureRenderTarget2D> ShadowTarget;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ShadowFunction;
	TWeakObjectPtr<class UDirectionalLightComponent> ShadowSun;
};
