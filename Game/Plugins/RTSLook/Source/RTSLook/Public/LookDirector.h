#pragma once

// O diretor do look (docs/GRAFICOS.md §8): aplica a receita do ambiente no mapa.
// - Assume o sol, o céu (SkyAtmosphere), a luz do céu e a névoa que o mapa já tem (os mapas do
//   WorldForge vêm com eles); só cria o que faltar. O pós é um componente ilimitado dele.
// - O sol fica fixo no mundo, escolhido pelo giro da câmera inicial do mapa.
// - A cada quadro, a névoa, a névoa volumétrica e a perspectiva aérea começam numa proporção da
//   distância da câmera ao foco (a regra de RTS): o foco fica limpo em qualquer zoom.
// - ApplyPreset(1..4) liga e desliga o que pesa (Lumen, névoa volumétrica), junto com o
//   Scalability do jogo (F5..F8).
// O mesmo que o environment() do Game/Tools/look_lab.py faz na maquete.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LookDirector.generated.h"

class ULookRecipe;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UVolumetricCloudComponent;
class UMaterialInstanceDynamic;

UCLASS()
class RTSLOOK_API ALookDirector : public AActor
{
	GENERATED_BODY()

public:
	ALookDirector();

	/** A receita do mapa (o ambiente e a hora). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TObjectPtr<ULookRecipe> Recipe;

	/** O giro da câmera inicial (graus), usado pro sol quando nenhuma câmera ILookCamera responde. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	float InitialCameraYaw = 45.0f;

	/** Aplica (de novo) a receita inteira: sol, céu, luz do céu, névoa e pós. */
	UFUNCTION(BlueprintCallable, Category = "Look")
	void ApplyRecipe();

	/** O preset de qualidade, 1 (baixa) a 4 (ultra), o mesmo nível do Scalability. */
	UFUNCTION(BlueprintCallable, Category = "Look")
	void ApplyPreset(int32 Level);

	/** O sol do mapa (a sombra da fumaça das explosões entra como função de luz nele). */
	UFUNCTION(BlueprintCallable, Category = "Look")
	UDirectionalLightComponent* GetSun() const { return Sun; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void AdoptOrSpawn();
	void ApplySun(float CameraYaw);
	void ApplyAtmosphere();
	void ApplySkyLight();
	void ApplyFog();
	void ApplyPost();
	void ApplyWater();
	void ApplyClouds();
	void UpdateFocus(float FocusDistanceCm);
	void UpdateTilt(float TopElevationDeg);
	void UpdateCloudSun();
	void UpdateCloudShadow();
	bool ReadCamera(float& OutDistanceCm, float& OutYaw) const;

	UPROPERTY(VisibleAnywhere, Category = "Look")
	TObjectPtr<UPostProcessComponent> Post;

	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(Transient) TObjectPtr<USkyAtmosphereComponent> Sky;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(Transient) TObjectPtr<UVolumetricCloudComponent> Cloud;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CloudMID;

	int32 PresetLevel = 4;
	float LastFocusCm = -1.0f;
	bool bSunPlaced = false;
	bool bCloudsAllowed = false;
	int8 CloudShown = -1;
	float LastAerialScale = -1.0f;
	float LastFalloff = -1.0f;
	/** 0 = a câmera normal, 1 = deitada (o céu à vista): a mistura do céu. */
	float SkyT = 0.0f;
	bool bRecapturePending = false;
	/** A função de luz do sol em que a sombra das nuvens já foi posta (a do Fluxo nasce depois do começo). */
	TWeakObjectPtr<UMaterialInstanceDynamic> CloudShadowMID;
	int8 CloudAbove = -1;
	/** A água do mapa (os MIDs que a receita pinta) e o último estado da câmera deitada aplicado nela e na exposição. */
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> WaterMIDs;
	float LastTiltT = -1.0f;
	float LastExpT = -1.0f;
};
