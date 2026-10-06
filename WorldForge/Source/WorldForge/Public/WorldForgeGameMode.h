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
 *   -Seed=N -Players=2|4 -Size=768|1024|1536 -Biome=0|1|2 -Forests=0..1   controles iniciais
 *   -ForgeCapture=<pasta>   teste automático: gera, tira fotos de vistas fixas,
 *                           grava os tempos no log e fecha
 *   -ForgeSeeds=1,2,3       sementes do teste automático (padrão: a -Seed)
 *   -ForgeUITest            com -ForgeCapture: testa o painel e a câmera com
 *                           cliques e teclas sintéticos (ForgeUITest.h)
 *   -ForgeRealQuality       teste do botão "Ver em qualidade real" (dispara sozinho;
 *                           com -ForgeCapture, o mapa montado é que tira as fotos)
 * Opção de URL ?resume: volta com os controles do último "Ver em qualidade real".
 */
UCLASS()
class WORLDFORGE_API AWorldForgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWorldForgeGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
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

	/**
	 * "Ver em qualidade real": grava os controles em Saved/WorldForge/world_settings.json,
	 * monta o mapa inteiro no acabamento das fatias num processo à parte (o editor
	 * sem janela rodando Tools/build_slice.py com SLICE=world; uns minutos) e abre
	 * /Game/Maps/World_Full quando termina. O app continua usável enquanto monta.
	 */
	bool StartRealQuality();
	/** 0 parada, 1 montando, 2 pronta (abrindo), -1 falhou; OutSeconds = tempo desde o começo. */
	int32 GetRealQualityState(double& OutSeconds) const;

	/**
	 * Prévia guardada: o nível em qualidade real fica em /Game/Maps/Worlds/W_<chave>,
	 * e a chave é a impressão digital (MD5) dos controles + do script de
	 * acabamento (Tools/build_slice.py) + PipelineVersion. Mesmos controles e
	 * mesmo acabamento: abre na hora. Mudou qualquer coisa: a chave muda e monta
	 * de novo — o mesmo esquema do cache de shaders da Unreal (DDC).
	 */
	static FString PreviewMapOf(const FWorldGenSettings& S);
	static bool HasPreview(const FWorldGenSettings& S);

	/** Mapas guardados na lista do painel (Saved/WorldForge/maps.json; começa com os 3 de exemplo). */
	struct FSavedMap
	{
		FString Name;
		FWorldGenSettings Settings;
	};
	const TArray<FSavedMap>& GetSavedMaps();
	/** Põe os controles de agora na lista (nome: bioma + semente). */
	void AddSavedMap();

	/** Avisa o painel quando um mapa novo aparece. */
	DECLARE_MULTICAST_DELEGATE(FOnMapShown);
	FOnMapShown OnMapShown;

private:
	void OnGenerated(int32 Ticket, TSharedPtr<FWorldMap> Map);
	void CaptureStep();

	FWorldGenSettings Settings;
	TArray<FSavedMap> SavedMaps;
	bool bSavedLoaded = false;
	FString RealMap;   // o nível que a montagem em andamento vai abrir
	FProcHandle RealProc;
	double RealStart = 0.0;
	double RealEnd = 0.0;
	int32 RealState = 0;
	bool bAutoRealQuality = false;   // -ForgeRealQuality (teste)
	double AutoRealAt = 0.0;
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
