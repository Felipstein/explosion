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
	/** A areia do acabamento (FWorldGenSettings::Sand): não gera o mapa de novo, só vale no "Ver em qualidade real". */
	void SetSand(int32 V) { Settings.Sand = V; }
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
		FString EditsFile;   // as edições à mão (Saved/WorldForge/edits/...), vazio sem
		FString ImportFolder;   // mapa trazido de fora (WorldImport), vazio: do gerador
	};
	const TArray<FSavedMap>& GetSavedMaps();
	/** Abre um mapa guardado: os controles e as edições à mão dele. */
	void OpenSavedMap(int32 Index);
	/** Abre um mapa trazido de fora (Saved/WorldForge/imported/<nome>): o relevo,
	 *  as texturas e as peças dele no lugar do gerador, com o nosso acabamento. */
	bool OpenImported(const FString& Folder, const FString& EditsFile = FString());
	/** O mapa na tela é importado (o título; vazio: do gerador) e o que ficou de fora. */
	const FString& GetImportTitle() const { return ImportTitle; }
	const FString& GetImportNote() const { return ImportNote; }
	/** Lê a lista de novo do arquivo (o teste devolve a lista como estava). */
	void ReloadSavedMaps() { SavedMaps.Reset(); bSavedLoaded = false; GetSavedMaps(); }
	/** Põe os controles de agora na lista (nome: bioma + semente). */
	void AddSavedMap();

	/** Avisa o painel quando um mapa novo aparece. */
	DECLARE_MULTICAST_DELEGATE(FOnMapShown);
	FOnMapShown OnMapShown;

private:
	void OnGenerated(int32 Ticket, TSharedPtr<FWorldMap> Map, TSharedPtr<struct FWorldLive> Live);

public:
	/** Pincel de chão: uma pincelada em P (m) com a camada e o raio do pincel.
	 *  bStart: começo do arrasto (o desfazer volta o arrasto inteiro). */
	void EditPaint(FVector2f P, bool bStart);
	/** Camada do pincel (0–5, na ordem do bioma) e raio (m). */
	int32 BrushLayer = 0;
	float BrushRadius = 6.0f;
	/** Pincel de altura: aplica em P (m) por DeltaSeconds (segurando, sobe/desce
	 *  contínuo). Modo: 0 subir, 1 baixar, 2 aplainar (na altura do começo), e os de 16/10 (o Felipe: "é questão de
	 *  pincelar e naturalizar, morros e descidas"): 3 morro, 4 vale, 5 suavizar, 6 naturalizar. */
	void EditHeight(FVector2f P, bool bStart, float DeltaSeconds);
	int32 HeightMode = 0;
	/** O pincel de altura repete nos outros lados (a simetria do mapa: o ranqueado justo). 1 = sim. */
	int32 HeightMirror = 1;
	/** Fim do arraste do pincel (chão ou altura): o pedaço é sorteado de novo
	 *  (capim, pedra, árvore com as regras de verdade no chão novo). */
	void EditStrokeEnd();
	/** Quantas vezes um pedaço foi refeito (teste). */
	int32 GetRegionRefreshes() const { return RegionRefreshes; }

	/** Edição à mão: apaga a peça do espalhamento mais perto de P (m). Fica
	 *  guardada (FWorldEdits) e vale pro "qualidade real" e pra gerar de novo. */
	bool EditDeleteAt(FVector2f P);
	/** Desfaz a última edição. */
	bool EditUndo();
	int32 EditCount() const;
	/** Ferramenta de edição: 0 olhar (o clique não mexe no mapa), 1 apagar peça,
	 *  2 pintar chão, 3 altura, 4 estrada, 5 mover peça. */
	int32 EditTool = 0;

	/** Mover peça: grava o lugar novo (m) e o giro (graus, somado) de uma peça
	 *  pega (o arraste já mostrou na vista); vale pro "qualidade real". */
	void EditMovePiece(const struct FMovePick& Pick, FVector2f To, float YawDeg);

	// ---- estradas à mão (ferramenta 4) ----
	// A estrada desenhada entra na rede do gerador (FWorldDetail::BuildRoads):
	// escava, aterra e entronca como as dele; a do gerador escolhida vira
	// desenhada na primeira mudança (os pontos de controle saem do traçado dela).
	// Cada mudança refaz o mundo ao vivo (~4 s, fora do game thread).
	/** Tipo da estrada nova: asfalto ou terra (no bioma sem asfalto, sempre terra). */
	bool RoadPaved = false;
	/** Pontos (m) da estrada sendo desenhada: o clique põe, Enter ou duplo clique termina. */
	TArray<FVector2f> RoadDraft;
	/** Termina a estrada desenhada (2+ pontos): entra na rede e o mundo é refeito. */
	void RoadFinishDraft();
	/** Escolhe a estrada que passa perto de P (m); false: nenhuma ali. */
	bool RoadSelectAt(FVector2f P);
	void RoadDeselect();
	bool HasRoadSelection() const { return bRoadSelected; }
	bool IsSelectedRoadPaved() const;
	/** O bioma tem asfalto (deserto, neve; a floresta só tem terra). */
	bool BiomeHasPavedRoads() const;
	/** Os pontos de controle da escolhida (arrastáveis). */
	const TArray<FVector2f>& GetRoadHandles() const { return RoadHandles; }
	/** O traçado de verdade da escolhida no mundo ao vivo (null: nenhuma, ou o mundo sendo refeito). */
	const TArray<FVector2f>* SelectedRoadLine() const;
	/** Move um ponto de controle (fim do arraste) e refaz o mundo. */
	void RoadMoveHandle(int32 Handle, FVector2f To);
	/** Troca o tipo da escolhida (e o da próxima). */
	void RoadSetPaved(bool bPaved);
	/** O acabamento do asfalto da escolhida (FWorldEdits::FRoadEdit::Style; 0 na do gerador ainda não mexida). */
	int32 GetSelectedRoadStyle() const;
	/** Troca o acabamento da escolhida (vale no "Ver em qualidade real": a laje é de lá). */
	void RoadSetStyle(int32 Style);
	/** A sujeira na beira da escolhida (FWorldEdits::FRoadEdit::Dirt; 0 automática). */
	int32 GetSelectedRoadDirt() const;
	/** Troca a sujeira na beira da escolhida (a laje e a estrada colada). */
	void RoadSetDirt(int32 Dirt);
	void RoadDeleteSelected();

private:
	bool bRoadSelected = false;
	int32 RoadSelUser = INDEX_NONE;                  // índice em Edits->Roads (desenhada à mão)
	FVector2f RoadSelProbe = FVector2f::ZeroVector;  // um ponto da do gerador, longe das outras
	TArray<FVector2f> RoadHandles;
	/** Índice da escolhida na rede do mundo ao vivo (INDEX_NONE: não está lá). */
	int32 FindSelectedRoad() const;
	void RoadPushUndo();
	/** A do gerador escolhida vira desenhada (sai a dela, entra uma com os pontos de controle). */
	void RoadMakeUser();

private:
	TSharedPtr<struct FWorldEdits> Edits;
	TArray<TSharedPtr<struct FEditUndo>> UndoStack;
	// o mapa importado em uso: entra no lugar do gerador enquanto os controles
	// forem os dele (mexeu num controle: volta pro gerador)
	TSharedPtr<FWorldMap> ImportedMap;
	FWorldGenSettings ImportedSettings;
	FString ImportFolder, ImportTitle, ImportNote;
	/** Refaz a altura final e as peças de um retângulo (m) da vista ao vivo. */
	void RefreshRegion(FVector2f MinM, FVector2f MaxM);
	int32 RegionRefreshes = 0;
	/** Soma desvios de altura (por vértice) no mapa ao vivo e refaz malha, peças e chão. */
	void ApplyHeightDeltas(const TMap<FIntPoint, float>& Deltas);   // (ponteiro: o tipo é do .cpp)
	/** O mesmo nas K cópias giradas do mapa (K = 1: só aqui); as que se encostam (perto do centro) viram uma só, sem somar. */
	void ApplyHeightDeltasMirrored(const TMap<FIntPoint, float>& Deltas, int32 K);
	void RefreshRegionMirrored(FVector2f MinM, FVector2f MaxM, int32 K);
	float HeightAccum = 0.0f;
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
