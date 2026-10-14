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
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS()
class RTSGAME_API URTSGroundMemory : public UObject
{
	GENERATED_BODY()

public:
	/** Os cadernos cobrindo o retângulo do mapa (cm), e o decal que pinta por cima do chão. */
	bool Init(UWorld* World, const FVector2D& InMapMinCm, const FVector2D& InMapSizeCm, float GroundZMinCm, float GroundZMaxCm, float WindYawDeg);
	/** A cada quadro, pra cada veículo de lagarta: risca a fita das duas lagartas por onde ele andou. */
	void TrackUnit(uint32 Id, const FVector2D& PosCm, float YawDeg, float GaugeCm, float WidthCm, bool bShown);
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
	};
	struct FTrackSide
	{
		FVector2D L = FVector2D::ZeroVector, R = FVector2D::ZeroVector, C = FVector2D::ZeroVector;
		float U = 0.0f;
		float Along = 0.0f;
		float Yaw = 0.0f;
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
		UTextureRenderTarget2D* Relief = nullptr;
		/** A altura das crateras (R16F, cm), na resolução do relevo (o grosso: na do caderno). */
		UTextureRenderTarget2D* Crater = nullptr;
		/** O carimbo do perfil, já filtrado no tamanho do texel deste caderno (sem serrilhar a bordinha). */
		UTexture2D* ProfileStamp = nullptr;
		/** A próxima linha do relevo a refazer (o relevo envelhece aos poucos, uma faixa por quadro). */
		int32 RollRow = 0;
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
	void UpdateRelief(FSheet& Sheet, const FIntRect& Rect);
	/** A direção e a elevação do sol, da luz direcional do mapa (o diretor do Look adota a dele). */
	void ReadSun();
	/** As texturas e os limites pro material do chão (o Landscape: a função do Look lê pelos nomes). Cada pedaço
	 *  do Landscape recebe as texturas quando aparece (o streaming carrega depois do começo); bBounds manda os
	 *  limites pra todos. Achando o parâmetro UseCrater (a MF do Look no chão), desliga o decal de piso. */
	void PushToLandscape(bool bBounds);
	TSet<TWeakObjectPtr<AActor>> PushedProxies;
	bool bDispOn = false;
	bool bDispForced = false;
	float PushClock = 0.0f;
	bool bLookGround = false;
	bool Touches(const FSheet& Sheet, const FVector2f& P, float MarginCm) const;

	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CoarseTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailProfile;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailRelief;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidTarget;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidProfile;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidRelief;
	UPROPERTY() TObjectPtr<UTexture2D> TrackStamp;
	UPROPERTY() TObjectPtr<UTexture2D> BlotStamp;
	UPROPERTY() TObjectPtr<UTexture2D> CraterStamp;
	UPROPERTY() TObjectPtr<UTexture2D> ProfileStampDetail;
	UPROPERTY() TObjectPtr<UTexture2D> ProfileStampMid;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CoarseCrater;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> DetailCrater;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> MidCrater;
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
