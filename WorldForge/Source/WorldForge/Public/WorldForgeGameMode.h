#pragma once

#include "GameFramework/GameModeBase.h"
#include "Generation/WorldMap.h"
#include "WorldForgeGameMode.generated.h"

class AWorldForgeTerrain;
class AForgeCameraPawn;
class FForgeUITest;

/**
 * O app do gerador: guarda os controles, gera o mapa fora do game thread
 * (o painel não trava enquanto gera) e mostra o resultado.
 *
 * Linha de comando:
 *   -Seed=N -Players=2|4 -Size=768|1024|1536 -Biome=0|1|2   controles iniciais
 *   -ForgeCapture=<pasta>   teste automático: gera, tira fotos de vistas fixas,
 *                           grava os tempos no log e fecha
 *   -ForgeSeeds=1,2,3       sementes do teste automático (padrão: a -Seed)
 *   -ForgeUITest            com -ForgeCapture: testa o painel e a câmera com
 *                           cliques e teclas sintéticos (ForgeUITest.h)
 */
UCLASS()
class WORLDFORGE_API AWorldForgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWorldForgeGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Pede um mapa novo com estes controles (descarta o que estiver em andamento). */
	void Generate(const FWorldGenSettings& NewSettings);
	const FWorldGenSettings& GetSettings() const { return Settings; }
	bool IsGenerating() const { return Pending > Shown; }
	/** Número do último mapa mostrado (cresce a cada mapa novo). */
	int32 GetShownTicket() const { return Shown; }

	AWorldForgeTerrain* GetTerrain() const { return Terrain; }
	/** Salva o mapa atual (alturas 16 bits, camadas, minimapa, JSON) e devolve a pasta. */
	FString SaveCurrentMap();

	/** Avisa o painel quando um mapa novo aparece. */
	DECLARE_MULTICAST_DELEGATE(FOnMapShown);
	FOnMapShown OnMapShown;

private:
	void OnGenerated(int32 Ticket, TSharedPtr<FWorldMap> Map);
	void CaptureStep();

	FWorldGenSettings Settings;
	int32 Pending = 0;    // número do último pedido
	int32 Shown = 0;      // número do último mapa mostrado
	bool bFirstMap = true;

	UPROPERTY(Transient)
	TObjectPtr<AWorldForgeTerrain> Terrain;

	// teste automático
	FString CaptureFolder;
	TArray<int32> CaptureSeeds;
	int32 CaptureSeedIndex = 0;
	int32 CaptureShot = 0;
	double CaptureWait = 0.0;
	TSharedPtr<FForgeUITest> UITest;
	double FrameSum = 0.0;     // tempo de quadro médio do teste (mapa já na tela)
	int32 FrameCount = 0;
};
