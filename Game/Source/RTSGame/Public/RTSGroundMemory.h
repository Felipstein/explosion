#pragma once

// A memória do chão (11/10, o defeito 6 da 1ª gameplay: "o chão não guarda nada").
// O chão guarda o que passou por ele: a esteira de cada lagarta (some em ~5 min, como no ZH) e o
// queimado e a cratera de cada tiro e morte (ficam a partida toda, esticados na direção de onde veio
// o tiro: o leque aponta pra longe do atirador, como na análise de cratera de artilharia).
//
// Como (estudo 11/10: ZH TerrainTracks, World of Tanks, CoH, BAR, Batman/neve, Spintires):
// - a esteira é uma fita contínua por lagarta (o padrão dos jogos de tanque), com as garras em fase
//   pelo caminho andado;
// - tudo é riscado em "cadernos" (texturas) que um decal só, do tamanho do mapa, lê e pinta por cima
//   do chão: o custo não cresce com o número de marcas (o CoH2 perdia qps ao longo da partida com um
//   decal por marca);
// - dois cadernos: o grosso cobre o mapa inteiro (50 cm por pixel, pro longe) e o de detalhe cobre
//   ~190 m em volta da câmera (~5 cm por pixel, o tamanho do pixel da tela no zoom do jogo). Quando a
//   câmera anda, o de detalhe se redesenha da lista de marcas, que fica guardada.
// Só desenho: cada PC risca o que vê (1ª versão, como o BAR; riscar no escuro e mostrar quando a área
// for revista é a 2ª etapa).
//
// O relevo (12/10, divisão com a sessão do Look: o jogo faz onde e quando, o Look faz como fica): o caderno
// de detalhe e um do meio (384 m em volta da câmera, ~19 cm por pixel, pro longe) ganham o perfil (a fita
// nova redesenha o chão por cima da velha; o frescor desbota com o tempo) e o relevo (GroundRelief.usf: altura com sinal, idade, sol,
// revolvido), que a função de material do Look (MF_LookGroundMemory) lê dentro do chão do Landscape. O
// estudo e a revisão estão em docs/CHAO_DEFORMAVEL.md.

#include "CoreMinimal.h"
#include "RTSGroundRelief.h"
#include "UObject/Object.h"
#include "RTSGroundMemory.generated.h"

class AActor;
class UCanvas;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS()
class RTSGAME_API URTSGroundMemory : public UObject
{
	GENERATED_BODY()

public:
	/** Os cadernos cobrindo o retângulo do mapa (cm), e o decal que pinta por cima do chão. */
	/** bMapGroundV3: o mapa veio com o chão v3 (o Maps.json): a v3 liga sem a chave. */
	bool Init(UWorld* World, const FVector2D& InMapMinCm, const FVector2D& InMapSizeCm, float GroundZMinCm, float GroundZMaxCm, float WindYawDeg,
		bool bMapGroundV3 = false);
	/** (23/10, v4) O jeito da marca: o padrão é a lagarta (o tanque aprovado, intocado). O pneu (o caminhão de suprimento e o Tomahawk; o
	 *  bloco "wheels" do Units.json) rola e compacta em vez de revirar a areia com a garra: a passada escurece menos (Dark), o sulco e a
	 *  bordinha são mais rasos (Depth; o revolvido vem deles), o perfil vai na largura do pneu (WidthK = a largura ÷ os 64 cm da lagarta
	 *  pra que ele foi feito) e as garras da banda são mais miúdas (LugK: garras por metro × as do M1, de 19,4 cm). */
	struct FTrackStyle
	{
		float Dark = 1.0f;
		float Depth = 1.0f;
		float WidthK = 1.0f;
		float LugK = 1.0f;
		/** (23/10, v4) O trecho da fita nas retas × o da lagarta (1 m; a curva parte a cada 6° do mesmo jeito): o pneu, 2 (metade das
		 *  fitas guardadas e redesenhadas; o caminhão de suprimento passa a partida inteira no mesmo vaivém). */
		float SegmentK = 1.0f;
	};
	/** A cada quadro, pra cada veículo de lagarta (ou de rodas, com o Style do pneu): risca a fita dos dois lados por onde ele andou. */
	void TrackUnit(uint32 Id, const FVector2D& PosCm, float YawDeg, float GaugeCm, float WidthCm, bool bShown, const FTrackStyle& Style = FTrackStyle());
	void ForgetUnit(uint32 Id);
	/** Tiro ou explosão: o queimado (Crater 0, o carimbo somado) ou a cratera do atlas (Crater > 0: a bacia, o halo
	 *  escuro, a borda, os raios e os torrões), esticados pra longe de quem atirou (From; sem atirador, giro sorteado). */
	void AddScar(const FVector2D& AtCm, const FVector2D& FromCm, float RadiusCm, float Scorch, float Crater, int32 Size = 1);
	/** A cratera nova (13/10): as pedras dela no mundo (x, y, largura em cm) e um sorteio, pro campo de destroços. */
	TFunction<void(const TArray<FVector>& Large, const TArray<FVector>& Small, uint32 Seed)> OnCraterStones;
	/** A morte do tanque (13/10): o avental de terra queimada e combustível em volta do casco (a carcaça fica e cobre o
	 *  miolo), a brasa, a areia espirrada e os torrões, orientado pelo casco. */
	void AddWreck(const FVector2D& AtCm, float HullYawDeg, const FVector2D& FromCm);
	/** O chão afunda de verdade (13/10, com o Look e o WorldForge): o deslocamento Nanite do Landscape, a MF lendo a altura
	 *  da cratera e do sulco nos cadernos (DispOn no material). Ligado no Alta e no Ultra; -RTSDisp=0/1 força, pro A/B. */
	void SetDisplacement(bool bOn);
	/** (16/10) A batalha no minimapa: a camada pintada (cor e alfa) do caderno de acúmulo, refeita a cada segundo; ela
	 *  cobre o mapa inteiro (os limites do caderno grosso). */
	UTextureRenderTarget2D* GetBattleOverlay() const { return BattleOverlay; }
	/** (16/10) O tipo de chão do mapa (o surface.png do WorldForge: R asfalto, G terra, B pedra, A molhado; Res × Res a
	 *  CellM metros, a linha 0 no −Y): o sulco, a bordinha e a garra de cada chão (a v3; rts.Ground.Surface 0 desliga). */
	void SetSurface(const TArray<FColor>& Pixels, int32 Res, const FVector2D& MinM, float CellM);
	/** Grava as marcas novas (uma passada por caderno), desbota a esteira e segue a câmera. */
	void Update(float DeltaSeconds, const FVector2D& FocusCm, float YawDeg);
	void Shutdown();
	/** O teste: grava os dois cadernos como imagem na pasta (pra ver se as marcas foram riscadas). */
	void DebugDump(const FString& Folder);

private:
	struct FTrackQuad
	{
		FVector2f L0, R0, L1, R1;
		float U0 = 0.0f, U1 = 0.0f;
		/** O andado desde o começo da fita (m, até 10): as pontas somem macias em 1,2 m. */
		float Along0 = 0.0f, Along1 = 0.0f;
		float Strength = 0.0f;
		float Time = 0.0f;
		/** (16/10, a v3) a curva no começo e no fim da fita (0 reto ou raio ≥ 30 m, 1 raio ≤ 6 m ou o pivô) e o lado de
		 *  fora: +1 o L (o lado −Lat), −1 o R, 0 reto. */
		float Curve0 = 0.0f, Curve1 = 0.0f;
		int8 Outer = 0;
		/** (23/10) O pneu (FTrackStyle): o fundo do sulco e da bordinha e a largura do perfil, × os da lagarta (1 = a lagarta). */
		float Depth = 1.0f, WidthK = 1.0f;
	};
	struct FTrackSide
	{
		FVector2D L = FVector2D::ZeroVector, R = FVector2D::ZeroVector, C = FVector2D::ZeroVector;
		float U = 0.0f;
		float Along = 0.0f;
		float Yaw = 0.0f;
		/** A curva alisada da última fita (a próxima começa dela: a largura não pula de uma fita pra outra). */
		float Curve = 0.0f;
		/** A curvatura com sinal (1/m; + = o lado de fora é o L), alisada pela distância andada (~3 m). */
		float Kappa = 0.0f;
		bool bValid = false;
	};
	struct FTrackPair
	{
		FTrackSide Side[2];
	};
	struct FScar
	{
		FVector2f At, Dir;
		float RadiusCm = 0.0f;
		float Stretch = 1.0f;
		float Scorch = 0.0f;
		float Crater = 0.0f;
		/** 0 = o queimado somado (Canvas), 1 = a cratera do atlas, 2 = o avental da morte do tanque. */
		uint8 Kind = 0;
		/** O tamanho da cratera (13/10, o Felipe: "cada uma pra uma coisa"): 0 = A, a pequena (o tiro de tanque),
		 *  1 = B, a média (o prédio que cai), 2 = C, a grande (o míssil pesado). */
		uint8 Size = 1;
		int32 Variant = 0;
		float Scale = 1.0f;
		float Mirror = 1.0f;
		float Time = 0.0f;
	};
	/** Um caderno: a textura e o pedaço do mundo que ela cobre; o de detalhe e o do meio têm também o
	 *  perfil e o relevo (na mesma janela) e andam com a câmera. As texturas são donas dos UPROPERTY abaixo. */
	struct FSheet
	{
		UTextureRenderTarget2D* Target = nullptr;
		UTextureRenderTarget2D* Profile = nullptr;
		/** (16/10, a garra da v3) o andado da lagarta sob a faixa (G16: o U da fita em m ÷ AlongRangeM); só o detalhe. */
		UTextureRenderTarget2D* Along = nullptr;
		UTextureRenderTarget2D* Relief = nullptr;
		/** A altura das crateras (RG16F, cm: R a cheia, G a que faz sombra do sol, sem os torrões), na resolução do relevo
		 *  (o grosso: na do caderno). */
		UTextureRenderTarget2D* Crater = nullptr;
		/** O carimbo do perfil, já filtrado no tamanho do texel deste caderno (sem serrilhar a bordinha). */
		UTexture2D* ProfileStamp = nullptr;
		/** A luz do sol assada (a v3, -RTSGroundV3): R SunLit, G SkyAO (RG16F, a janela da esteira); nulo sem a chave. */
		UTextureRenderTarget2D* SunLit = nullptr;
		/** A grade fina da luz: SS × SS amostras por texel (2 no Alta, 4 no Ultra). */
		int32 SunLitSS = 1;
		/** A próxima linha do relevo a refazer (o relevo envelhece aos poucos, uma faixa por quadro). */
		int32 RollRow = 0;
		/** O próximo ladrilho da volta lenta da luz assada (a trilha velha: o relevo envelhece, a luz acompanha). */
		int32 SunLitRollTile = 0;
		/** (16/10) onde há cratera neste caderno, em 64 × 64 casas (a trilha sozinha muda ~20 cm de altura: a luz dela só
		 *  precisa do contexto longo, 7 m do lado do sol, perto de cratera, que faz sombra de 1 m de fundo). */
		uint64 CraterRows[64] = {};
		/** (16/10, a v3) o que mudou desde as últimas mips, em texels de cada caderno (vazio = nada). */
		TArray<FIntRect, TInlineAllocator<8>> ReliefMipDirty;
		TArray<FIntRect, TInlineAllocator<8>> SunLitMipDirty;
		FVector2D MinCm = FVector2D::ZeroVector;
		FVector2D SizeCm = FVector2D(1.0, 1.0);
		int32 Res = 0;
		double AheadCm = 0.0;
		double SlackCm = 0.0;
		bool bPlaced = false;
		FRTSReliefParams Relief_;
		const TCHAR* Name = TEXT("");
	};

	/** Os carimbos: 0 = esteira, 1 = queimado (borda irregular e riscos de fuligem), 2 = cratera (bacia lisa). */
	UTexture2D* MakeStamp(int32 W, int32 H, int32 Kind);
	UTextureRenderTarget2D* MakeTarget(int32 Res);
	/** O carimbo do perfil da esteira (versão C do Look), ±60 cm em volta do meio da fita: R faixa, G bordinha
	 *  (divididos pela cobertura: a mistura translúcida dá a conta do Look), B frescor, A cobertura. */
	UTexture2D* MakeProfileStamp(float TexelCm);
	/** O relevo: R8G8B8A8 com escrita da placa (UAV); o perfil: RGB10A2 (o frescor desbota sem travar no 8 bits). */
	UTextureRenderTarget2D* MakeReliefTarget(int32 Res);
	UTextureRenderTarget2D* MakeProfileTarget(int32 Res);
	UTextureRenderTarget2D* MakeAlongTarget(int32 Res);
	/** O andado das fitas no caderno Along (a garra da luz assada lê). */
	void DrawAlong(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices) const;
	/** A altura das crateras: R16F com escrita da placa, em cm (zero = o chão). */
	UTextureRenderTarget2D* MakeCraterTarget(int32 Res);
	/** Lê um atlas de marcas (Config/RTS/Ground/<tipo>_<opção>.png, 16 bits, e o .json com a escala). */
	bool LoadAtlas(const FString& Kind, const FString& Option, FRTSGroundMarkAtlas& Out, TArray<TArray<FVector3f>>* OutClods = nullptr);
	/** As crateras e os aventais da lista num caderno (o compute do atlas; bAged: a brasa já apagando). */
	void StampMarks(FSheet& Sheet, const TArray<int32>& Indices, bool bAged);
	void DrawTracks(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices, bool bAged) const;
	void DrawScars(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices) const;
	/** O perfil: cada fita nova redesenha (translúcido, pela cobertura) o chão por cima da velha, com o frescor dela (1 = agora). */
	void DrawProfile(UCanvas* Canvas, const FSheet& Sheet, const TArray<int32>& Indices, bool bAged) const;
	void Redraw(FSheet& Sheet);
	void Recenter(FSheet& Sheet, const FVector2D& CenterCm);
	/** O retângulo (em texels do relevo) que as marcas novas tocaram; vazio se nenhuma. */
	FIntRect DirtyRect(const FSheet& Sheet) const;
	/** bLight: a luz assada também (a v3); o envelhecer aos poucos não refaz a luz. */
	void UpdateRelief(FSheet& Sheet, const FIntRect& Rect, bool bLight = true);
	/** A luz assada (16/10, a v3 do chão): os retângulos a refazer (em texels do caderno de luz, até 512), com uma cota
	 *  por quadro: o recentrar do caderno pede o caderno inteiro, que sai em vários quadros. */
	struct FSunLitJob
	{
		FSheet* Sheet = nullptr;
		FIntRect Rect;
		float WriteDropCm = 100.0f;
		/** A queda mais funda que pode fazer sombra ali (a varredura começa MaxDrop ÷ tan antes, do lado do sol). */
		float ContextDropCm = 100.0f;
	};
	TArray<FSunLitJob> SunLitQueue;
	/** WriteDropCm: o quanto a altura mudou (a trilha ~20 cm; a cratera e o redesenho, tudo). */
	void QueueSunLit(FSheet& Sheet, const FIntRect& ReliefRect, float WriteDropCm = 100.0f, float ContextDropCm = 100.0f);
	/** As casas de cratera do caderno: marca o carimbo; e se alguma cai no retângulo (texels do relevo) mais ExtraCm. */
	void MarkCraterCells(FSheet& Sheet, const FVector2D& AtCm, double RadiusCm) const;
	bool CraterNear(const FSheet& Sheet, const FIntRect& ReliefRect, double ExtraCm) const;
	/** O contexto do lado do sol pra um retângulo de trilha: o longo com cratera perto, o curto (a trilha) sem. */
	float ContextFor(const FSheet& Sheet, const FIntRect& ReliefRect) const;
	/** As marcas novas deste quadro, uma caixa por fita ou cicatriz (as vizinhas juntas): a luz refaz só onde mudou. */
	void QueueSunLitNew(FSheet& Sheet);
	void RunSunLit(float DeltaSeconds);
	/** As mips do relevo e da luz assada no que mudou no quadro de antes (a v3; os cadernos nascem com a cadeia). */
	void FlushMips();
	bool SunLitRollFlip = false;
	UTextureRenderTarget2D* MakeSunLitTarget(int32 Res);
	FVector2f SunLitSunDir = FVector2f(0.0f, 0.0f);
	float SunLitSunTan = 0.0f;
	/** A volta lenta: um ladrilho de 512 da luz refeito a cada SunLitRollSeconds, com a fila vazia. */
	float SunLitRollClock = 0.0f;
	/** A direção e a elevação do sol, da luz direcional do mapa (o diretor do Look adota a dele). */
	void ReadSun();
	/** As texturas e os limites pro material do chão (o Landscape: a função do Look lê pelos nomes). Cada pedaço
	 *  do Landscape recebe as texturas quando aparece (o streaming carrega depois do começo); bBounds manda os
	 *  limites pra todos. Achando o parâmetro UseCrater (a MF do Look no chão), desliga o decal de piso. */
	void PushToLandscape(bool bBounds);
	TSet<TWeakObjectPtr<AActor>> PushedProxies;
	/** As lajes da estrada do WorldForge (etiqueta WorldForgeRoadSlab, 16/10): malhas Nanite com o mesmo CODE da MF e os
	 *  mesmos nomes de parâmetro do chão; cada uma ganha uma instância dinâmica e os cadernos, pra esteira e a cratera
	 *  aparecerem na estrada também. */
	void PushToSlabs(bool bBounds);
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SlabMIDs;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> PushedSlabs;
	/** (16/10, o WorldForge: o capim deitado no caminho) os ISM das plantas da montagem (etiqueta WorldForgeTrample) com o
	 *  caderno da batalha e a trilha fresca; as instâncias dinâmicas ficam no SlabMIDs. */
	void PushToTrample(bool bBounds);
	TSet<TWeakObjectPtr<UPrimitiveComponent>> PushedTrample;
	bool bDispOn = false;
	bool bDispForced = false;
	float PushClock = 0.0f;
	bool bLookGround = false;
	bool Touches(const FSheet& Sheet, const FVector2f& P, float MarginCm) const;

	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CoarseTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailProfile;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailAlong;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailRelief;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidProfile;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidRelief;
	UPROPERTY() TObjectPtr<UTexture2D> TrackStamp;
	UPROPERTY() TObjectPtr<UTexture2D> BlotStamp;
	/** (16/10) O caderno de acúmulo do mapa inteiro (docs/MARCAS_DA_BATALHA.md): R o tráfego, G o combustível fresco, B o
	 *  queimado largo, A a densidade de crateras; e a camada dele pro minimapa. */
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> BattleTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> BattleOverlay;
	float BattleOverlayClock = 0.0f;
	bool bBattleDirty = false;
	void DrawBattle(UCanvas* Canvas, const FVector2D& Size, bool bDecay);
	/** (16/10) O tipo de chão do mapa na placa (o relevo e a garra leem). */
	UPROPERTY() TObjectPtr<UTexture2D> SurfaceTex;
	FVector2f SurfaceMinCm = FVector2f(0.0f, 0.0f);
	float SurfaceSizeCm = 0.0f;
	UPROPERTY() TObjectPtr<UTexture2D> CraterStamp;
	UPROPERTY() TObjectPtr<UTexture2D> ProfileStampDetail;
	UPROPERTY() TObjectPtr<UTexture2D> ProfileStampMid;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CoarseCrater;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailCrater;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidCrater;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailSunLit;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidSunLit;
	UPROPERTY() TObjectPtr<UTexture2D> CraterAtlasTex[3];
	UPROPERTY() TObjectPtr<UTexture2D> WreckAtlasTex;
	UPROPERTY() TObjectPtr<UTexture2D> BlotAtlasTex;
	/** As três crateras (A, B, C do Game/Tools/bake_craters.py), uma por tamanho. */
	FRTSGroundMarkAtlas CraterAtlases[3];
	FRTSGroundMarkAtlas WreckAtlas;
	/** A mancha do queimado sem cratera (o tiro que chamusca, a pegada do prédio), pelo mesmo carimbo das crateras;
	 *  sem ela, o carimbo somado do Canvas de antes. */
	FRTSGroundMarkAtlas BlotAtlas;
	/** Os pedaços 3D de cada variante de cada cratera (u, v, largura em cm, no espaço do carimbo; o JSON do atlas). */
	TArray<TArray<FVector3f>> CraterClods[3];
	/** O G e o A dos cadernos estão em rampas (os atlas vieram com "encoding": "ramp"): o chão (MarkRamps) decodifica. */
	bool bMarkRamps = false;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DecalMID;
	UPROPERTY() TObjectPtr<AActor> DecalActor;
	TWeakObjectPtr<UWorld> WorldRef;
	FSheet Coarse;
	FSheet Detail;
	FSheet Mid;
	bool bDetailPlaced = false;
	FVector2f WindDir = FVector2f(1.0f, 0.0f);
	FVector2f SunDir = FVector2f(1.0f, 0.0f);
	float SunTan = 0.577f;
	bool bReliefCycle = false;

	TMap<uint32, FTrackPair> Tracks;
	// as marcas guardadas (anel: a mais velha sai quando enche)
	TArray<FTrackQuad> Quads;
	int32 QuadHead = 0;
	TArray<FScar> Scars;
	int32 ScarHead = 0;
	TArray<int32> NewQuads;
	TArray<int32> NewScars;

	float Clock = 0.0f;
	bool bDebugCycle = false;
	int32 DebugMode = -1;
	float DecayClock = 0.0f;
};
