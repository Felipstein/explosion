#include "WorldForgeEditorLib.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Generation/WorldGenerator.h"
#include "Generation/WorldNoise.h"
#include "Generation/WorldTrees.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// o último recorte gerado: as outras funções (espalhamento, trilhas) leem dele
	TSharedPtr<FWorldMap> GMap;
	FVector2f GCenter = FVector2f::ZeroVector;   // centro do recorte no mapa gerado (m)
	float GHalf = 0.0f;                           // meio lado do recorte (m)

	FVector2f ToMap(FVector2D Cm) { return GCenter + FVector2f(Cm.X / 100.0, Cm.Y / 100.0); }

	// estrada (opcional): linha central no mapa (m, passo de 1 m), cota suavizada, meia largura
	bool GFrozen = false;   // inverno: o lago vira uma placa de gelo plana (camada "molhado" = gelo)
	TArray<FVector2f> GRoad;
	TArray<float> GRoadZ;
	float GRoadHalf = 0.0f;
	// grade de 8 m com os trechos da estrada perto de cada célula: o mapa inteiro
	// tem milhões de vértices e a estrada, mil trechos — sem a grade, horas
	constexpr float RoadCell = 8.0f;
	TMap<FIntPoint, TArray<int32>> GRoadGrid;
	float GRoadReach = 0.0f;   // além disto a distância não importa (devolve 1e9)

	FIntPoint RoadCellOf(FVector2f P) { return FIntPoint(FMath::FloorToInt(P.X / RoadCell), FMath::FloorToInt(P.Y / RoadCell)); }

	/**
	 * Distância (m) até a linha central da estrada, e a cota dela no ponto mais
	 * perto (1e9 longe dela). OutEnd: metros até a ponta mais perto da estrada
	 * (a linha tem um ponto por metro).
	 */
	float RoadDistance(FVector2f P, float* OutZ = nullptr, float* OutEnd = nullptr)
	{
		float Best = 1e9f, BestZ = 0.0f, BestEnd = 0.0f;
		const TArray<int32>* Near = GRoadGrid.Find(RoadCellOf(P));
		if (!Near)
		{
			if (OutZ) *OutZ = 0.0f;
			if (OutEnd) *OutEnd = 0.0f;
			return Best;
		}
		for (const int32 K : *Near)
		{
			const FVector2f A = GRoad[K - 1], AB = GRoad[K] - A;
			const float T = FMath::Clamp(((P - A) | AB) / FMath::Max(AB | AB, 1e-6f), 0.0f, 1.0f);
			const float D = FVector2f::Distance(P, A + AB * T);
			if (D < Best)
			{
				Best = D;
				BestZ = FMath::Lerp(GRoadZ[K - 1], GRoadZ[K], T);
				BestEnd = FMath::Min(float(K - 1) + T, float(GRoad.Num() - K) - T);
			}
		}
		if (OutZ) *OutZ = BestZ;
		if (OutEnd) *OutEnd = BestEnd;
		return Best;
	}

	/**
	 * Estrada ao longo da trilha mais longa que cruza o recorte: a trilha do
	 * gerador (grade de 4 m) é suavizada em ~30 m, reamostrada a cada metro, e
	 * a cota acompanha o relevo suavizado em ~50 m (corta morro, aterra vale).
	 */
	void BuildRoad(const FWorldMap& M, float HalfWidth)
	{
		GRoad.Reset();
		GRoadZ.Reset();
		GRoadGrid.Reset();
		GRoadHalf = HalfWidth;
		if (HalfWidth <= 0.0f) return;
		const TArray<FVector2f>* BestLine = nullptr;
		int32 BestCount = 0;
		for (const TArray<FVector2f>& Line : M.Tracks)
		{
			int32 C = 0;
			for (const FVector2f& P : Line)
			{
				const FVector2f L = P - GCenter;
				C += (FMath::Abs(L.X) < GHalf + 30.0f && FMath::Abs(L.Y) < GHalf + 30.0f) ? 1 : 0;
			}
			if (C > BestCount) { BestCount = C; BestLine = &Line; }
		}
		if (!BestLine || BestCount < 8) return;
		TArray<FVector2f> In;
		for (const FVector2f& P : *BestLine)
		{
			const FVector2f L = P - GCenter;
			if (FMath::Abs(L.X) < GHalf + 60.0f && FMath::Abs(L.Y) < GHalf + 60.0f) In.Add(P);
		}
		// suaviza (média móvel de ±4 pontos, duas passadas) e reamostra a cada 1 m
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			TArray<FVector2f> Sm = In;
			for (int32 I = 0; I < In.Num(); ++I)
			{
				FVector2f Sum = FVector2f::ZeroVector;
				int32 C = 0;
				for (int32 K = FMath::Max(0, I - 4); K <= FMath::Min(In.Num() - 1, I + 4); ++K) { Sum += In[K]; ++C; }
				Sm[I] = Sum / float(C);
			}
			In = Sm;
		}
		for (int32 I = 1; I < In.Num(); ++I)
		{
			const float Len = FVector2f::Distance(In[I - 1], In[I]);
			for (float T = 0.0f; T < Len; T += 1.0f) GRoad.Add(FMath::Lerp(In[I - 1], In[I], T / Len));
		}
		TArray<float> Raw;
		for (const FVector2f& P : GRoad) Raw.Add(M.HeightAt(P));
		GRoadZ.SetNum(Raw.Num());
		for (int32 I = 0; I < Raw.Num(); ++I)
		{
			float Sum = 0.0f;
			int32 C = 0;
			for (int32 K = FMath::Max(0, I - 25); K <= FMath::Min(Raw.Num() - 1, I + 25); ++K) { Sum += Raw[K]; ++C; }
			GRoadZ[I] = Sum / float(C) + 0.08f;   // o asfalto fica 8 cm acima do chão em volta
		}
		// cada trecho entra nas células que ficam a menos do alcance dele
		GRoadReach = HalfWidth + 12.0f;
		for (int32 K = 1; K < GRoad.Num(); ++K)
		{
			const FVector2f Lo(FMath::Min(GRoad[K - 1].X, GRoad[K].X) - GRoadReach, FMath::Min(GRoad[K - 1].Y, GRoad[K].Y) - GRoadReach);
			const FVector2f Hi(FMath::Max(GRoad[K - 1].X, GRoad[K].X) + GRoadReach, FMath::Max(GRoad[K - 1].Y, GRoad[K].Y) + GRoadReach);
			const FIntPoint A = RoadCellOf(Lo), B = RoadCellOf(Hi);
			for (int32 Y = A.Y; Y <= B.Y; ++Y)
			{
				for (int32 X = A.X; X <= B.X; ++X)
				{
					GRoadGrid.FindOrAdd(FIntPoint(X, Y)).Add(K);
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice road %d m, half width %.1f m"), GRoad.Num(), HalfWidth);
	}

	/**
	 * Pesos das camadas num ponto do mapa (somam 1), por papel — o nome vem do
	 * bioma (Python): 0 base (areia, neve, grama), 1 manchas (cascalho, neve
	 * suja, folhiço), 2 trilha (terra batida, lama), 3 rocha, 4 molhado/seco
	 * (terra rachada, gelo, lama), 5 asfalto (só com estrada).
	 */
	// os controles do painel do app ("Ver em qualidade real"): valem no lugar dos parâmetros
	TOptional<FWorldGenSettings> GSettingsOverride;

	bool GForest = false;   // floresta: o folhiço (camada de manchas) vai onde a mata fecha
	bool GWinter = false;   // neve: sem as dunas do deserto

	// cobertura das copas na grade do mapa (2 m): quanto do ponto fica sob uma
	// árvore do gerador. O folhiço, o sub-bosque e a madeira morta seguem as
	// árvores de verdade (as do painel), não um ruído à parte.
	TArray<float> GCanopy;

	void BuildCanopy(const FWorldMap& M)
	{
		GCanopy.Init(0.0f, M.N * M.N);
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != EWorldPropType::Tree) continue;
			const WorldTrees::EKind Kind = WorldTrees::KindOf(M.Settings.Biome, P.Variant);
			const float R = 0.5f * WorldTrees::MeshSize(Kind).Y * WorldTrees::ScaleOf(P, M.Settings.Biome) + 1.5f;   // copa + a beira
			const FVector2f G = M.ToGrid(P.Pos);
			const int32 Cells = FMath::CeilToInt(R / M.Cell) + 1;
			for (int32 J = FMath::Max(0, int32(G.Y) - Cells); J <= FMath::Min(M.N - 1, int32(G.Y) + Cells); ++J)
			{
				for (int32 I = FMath::Max(0, int32(G.X) - Cells); I <= FMath::Min(M.N - 1, int32(G.X) + Cells); ++I)
				{
					const float D = FVector2f::Distance(FVector2f(I, J), G) * M.Cell;
					float& C = GCanopy[M.Index(I, J)];
					C = FMath::Max(C, 1.0f - FMath::SmoothStep(0.6f * R, R, D));
				}
			}
		}
	}

	/** Mata no ponto: > 0 sob as copas das árvores do gerador (−0,5 no aberto). */
	float WoodAt(const FWorldMap& M, FVector2f P)
	{
		if (GCanopy.Num() != M.N * M.N) return -0.5f;
		const FVector2f G = M.ToGrid(P);
		const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
		const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
		const float FX = X - I0, FY = Y - J0;
		const float C = FMath::Lerp(FMath::Lerp(GCanopy[M.Index(I0, J0)], GCanopy[M.Index(I0 + 1, J0)], FX),
			FMath::Lerp(GCanopy[M.Index(I0, J0 + 1)], GCanopy[M.Index(I0 + 1, J0 + 1)], FX), FY);
		return C - 0.5f;
	}

	void DesertWeights(const FWorldMap& M, FVector2f P, float W[6])
	{
		// camadas do gerador (interpoladas): R terra, G vegetação, B margem, A rocha
		const FVector2f G = M.ToGrid(P);
		auto Bil = [&M, &G](int32 Ch)
		{
			const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
			const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
			const float FX = X - I0, FY = Y - J0;
			auto At = [&M, Ch](int32 A, int32 B)
			{
				const FColor& C = M.Layers[M.Index(A, B)];
				return (Ch == 0 ? C.R : Ch == 1 ? C.G : Ch == 2 ? C.B : C.A) / 255.0f;
			};
			return FMath::Lerp(FMath::Lerp(At(I0, J0), At(I0 + 1, J0), FX), FMath::Lerp(At(I0, J0 + 1), At(I0 + 1, J0 + 1), FX), FY);
		};
		const float Dirt = Bil(0), Lush = Bil(1), Rock = Bil(3);
		float Shore = Bil(2);
		if (GFrozen && M.HeightAt(P) < M.WaterLevel + 0.15f) Shore = 1.0f;   // a placa de gelo inteira
		// escoamento interpolado: o do vértice mais perto fazia quadrados de 2 m de lama
		float Flow = 0.0f;
		if (M.Flow.Num() == M.N * M.N)
		{
			const float X = FMath::Clamp(G.X, 0.0f, float(M.N - 1)), Y = FMath::Clamp(G.Y, 0.0f, float(M.N - 1));
			const int32 I0 = FMath::Min(int32(X), M.N - 2), J0 = FMath::Min(int32(Y), M.N - 2);
			const float FX = X - I0, FY = Y - J0;
			Flow = FMath::Lerp(FMath::Lerp(M.Flow[M.Index(I0, J0)], M.Flow[M.Index(I0 + 1, J0)], FX),
				FMath::Lerp(M.Flow[M.Index(I0, J0 + 1)], M.Flow[M.Index(I0 + 1, J0 + 1)], FX), FY);
		}
		// manchas largas (ruído) pra variar cascalho × areia
		const float Patch = WorldNoise::Fbm(P.X / 45.0f, P.Y / 45.0f, 911u, 4);
		const float Fine = WorldNoise::Fbm(P.X / 7.0f, P.Y / 7.0f, 913u, 3);
		float Rk = FMath::SmoothStep(0.25f, 0.7f, Rock);
		// terra batida; na floresta só o miolo dos trechos de terra (as bases e as
		// trilhas), o resto é grama — campo de terra grande lia como deserto
		float Dr = (GForest ? FMath::SmoothStep(0.5f, 0.85f, Dirt + 0.1f * Fine) : FMath::SmoothStep(0.2f, 0.6f, Dirt + 0.1f * Fine)) * (1.0f - Rk);
		// terra rachada: leitos secos (margem dos lagos e por onde a água escorre)
		float Cr = FMath::Clamp(FMath::Max(Shore, FMath::SmoothStep(0.45f, 0.8f, Flow)) + 0.15f * Fine, 0.0f, 1.0f) * (1.0f - Rk) * (1.0f - Dr);
		// cascalho: manchas e o pé das encostas
		float Gv = FMath::Clamp(FMath::SmoothStep(0.05f, 0.45f, Patch + 0.3f * Lush - 0.15f) + 0.6f * FMath::SmoothStep(0.05f, 0.3f, Rock), 0.0f, 1.0f)
			* (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		if (GForest)
		{
			// folhiço sob a copa, com a borda do bosque quebrada pelo ruído fino
			// só onde a mata fecha (árvore solta ganhava um disco laranja de folhas)
			Gv = FMath::SmoothStep(0.05f, 0.35f, WoodAt(M, P) + 0.08f * Fine) * (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		}
		float Sd = FMath::Max(0.0f, 1.0f - Rk - Dr - Cr - Gv);
		const float Sum = Sd + Gv + Dr + Rk + Cr;
		W[0] = Sd / Sum; W[1] = Gv / Sum; W[2] = Dr / Sum; W[3] = Rk / Sum; W[4] = Cr / Sum; W[5] = 0.0f;
		if (GRoad.Num() > 1)
		{
			// asfalto com borda gasta e buracos (o chão de baixo aparece), e o
			// acostamento de terra/cascalho dos dois lados
			float EndM;
			const float D = RoadDistance(P, nullptr, &EndM);
			const float Edge = 0.35f * WorldNoise::Fbm(P.X / 2.5f, P.Y / 2.5f, 921u, 3);
			// nas pontas o asfalto se desfaz em buracos até virar terra (sem "toco")
			const float Ends = FMath::SmoothStep(0.0f, 45.0f, EndM);
			const float Holes = FMath::SmoothStep(0.42f - 0.5f * (1.0f - Ends), 0.62f - 0.5f * (1.0f - Ends), WorldNoise::Fbm(P.X / 4.0f, P.Y / 4.0f, 923u, 4));
			// borda com 1,2 m de transição: mais fina que o espaçamento do terreno
			// (0,5 m) a borda virava escadinha na diagonal (visto na neve em 06/10)
			const float A = (1.0f - FMath::SmoothStep(GRoadHalf - 0.6f, GRoadHalf + 0.6f, D + Edge)) * (1.0f - Holes) * FMath::SmoothStep(0.0f, 0.25f, Ends);
			const float Sh = (1.0f - A) * (1.0f - FMath::SmoothStep(GRoadHalf + 0.5f, GRoadHalf + 3.5f, D + 1.5f * Edge));
			for (int32 K = 0; K < 5; ++K) W[K] *= (1.0f - A - Sh);
			W[2] += Sh * 0.6f;
			W[1] += Sh * 0.4f;
			W[5] = A;
		}
	}

	/** Altura do recorte (m): o relevo do gerador + ondulação baixa da areia (dunas de ~6 e 20 m). */
	float SliceHeight(const FWorldMap& M, FVector2f P)
	{
		float W[6];
		DesertWeights(M, P, W);
		float H = M.HeightAt(P);
		if (GFrozen) H = FMath::Max(H, M.WaterLevel);   // gelo plano na cota d'água
		// ondulação na camada base: dunas no deserto; na neve e na grama só um
		// rolado largo (a de 6 m, com o sol baixo, virava "caixa de ovo" na neve)
		if (GWinter || GForest) H += W[0] * 0.18f * WorldNoise::Fbm(P.X / 28.0f, P.Y / 28.0f, 919u, 2);
		else H += W[0] * (0.12f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 917u, 3) + 0.25f * WorldNoise::Fbm(P.X / 20.0f, P.Y / 20.0f, 919u, 2));
		if (GRoad.Num() > 1)
		{
			// a estrada aplaina o chão (corte e aterro) com rampa suave nas laterais
			float Z;
			const float D = RoadDistance(P, &Z);
			H = FMath::Lerp(H, Z, 1.0f - FMath::SmoothStep(GRoadHalf + 0.8f, GRoadHalf + 7.0f, D));
		}
		return H;
	}
}

ALandscape* UWorldForgeEditorLib::BuildSliceLandscape(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
	int32 Components, int32 QuadsPerComponent, float QuadSizeM, UMaterialInterface* Material,
	const TArray<ULandscapeLayerInfoObject*>& LayerInfos, float RoadWidthM, FString& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = TEXT("sem mundo do editor");
		return nullptr;
	}
	const int32 NumLayers = LayerInfos.Num();
	if (NumLayers != 5 && NumLayers != 6)
	{
		Error = TEXT("esperava 5 camadas (base, manchas, trilha, rocha, molhado) e a 6ª opcional (asfalto)");
		return nullptr;
	}

	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	if (GSettingsOverride.IsSet())
	{
		S = GSettingsOverride.GetValue();
	}
	const int32 N = Components * QuadsPerComponent + 1;                // vértices por lado
	if (CenterM.X > 1e8)
	{
		CenterM = SuggestSliceCenter(Seed, Players, Biome, MapSize, (N - 1) * QuadSizeM);
	}
	GMap = FWorldGenerator::Generate(S);
	const FWorldMap& M = *GMap;
	GFrozen = S.Biome == EWorldBiome::Winter && M.WaterLevel > -100.0f;
	GForest = S.Biome == EWorldBiome::Temperate;
	GWinter = S.Biome == EWorldBiome::Winter;
	BuildCanopy(M);
	GCenter = FVector2f(CenterM.X, CenterM.Y);
	GHalf = 0.5f * (N - 1) * QuadSizeM;
	BuildRoad(M, NumLayers == 6 ? 0.5f * RoadWidthM : 0.0f);

	// alturas: 32768 = 0 m; com escala Z de 100 cada unidade vale 1/128 m
	// (LANDSCAPE_ZSCALE): ±256 m com precisão de ~8 mm
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(N * N);
	TArray<TArray<uint8>> Weights;
	Weights.SetNum(NumLayers);
	for (TArray<uint8>& L : Weights) L.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const FVector2f P = GCenter + FVector2f(X * QuadSizeM - GHalf, Y * QuadSizeM - GHalf);
			const float H = SliceHeight(M, P);
			float W[6];
			DesertWeights(M, P, W);
			Heights[Y * N + X] = uint16(FMath::Clamp(FMath::RoundToInt(32768.0f + H * 128.0f), 0, 65535));
			// pesos em 8 bits somando 255 (o resto vai pra camada base)
			int32 Acc = 0;
			for (int32 K = 1; K < NumLayers; ++K)
			{
				const int32 V = FMath::RoundToInt(W[K] * 255.0f);
				Weights[K][Y * N + X] = uint8(V);
				Acc += V;
			}
			Weights[0][Y * N + X] = uint8(FMath::Clamp(255 - Acc, 0, 255));
		}
	}

	// o mesmo caminho do "New Landscape" do editor (LandscapeEditorDetailCustomization_NewLandscape.cpp)
	const FVector Scale(QuadSizeM * 100.0, QuadSizeM * 100.0, 100.0);
	const FVector Location(-GHalf * 100.0, -GHalf * 100.0, 0.0);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));
	TArray<FLandscapeImportLayerInfo> Layers;
	for (int32 K = 0; K < NumLayers; ++K)
	{
		FLandscapeImportLayerInfo& L = Layers.AddDefaulted_GetRef();
		L.LayerName = LayerInfos[K]->GetLayerName();
		L.LayerInfo = LayerInfos[K];
		L.LayerData = MoveTemp(Weights[K]);
	}
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(Layers));
	Landscape->Import(FGuid::NewGuid(), 0, 0, N - 1, N - 1, 1, QuadsPerComponent, HeightDataPerLayers, TEXT(""),
		MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		Error = TEXT("Landscape sem LandscapeInfo depois do Import");
		return Landscape;
	}
	Landscape->SetActorLabel(TEXT("SliceLandscape"));
	Info->UpdateLayerInfoMap(Landscape);
	for (ULandscapeLayerInfoObject* LI : LayerInfos)
	{
		Landscape->AddTargetLayer(LI->GetLayerName(), FLandscapeTargetLayerSettings(LI));
		const int32 Index = Info->GetLayerInfoIndex(LI->GetLayerName());
		if (Info->Layers.IsValidIndex(Index))
		{
			Info->Layers[Index].LayerInfoObj = LI;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape %dx%d verts, %.0f m, seed %d, center (%.0f, %.0f)"), N, N, 2.0f * GHalf, S.Seed, GCenter.X, GCenter.Y);
	return Landscape;
}

FWorldForgeScatter UWorldForgeEditorLib::SliceScatter(FName Kind, float SpacingM, float ScaleMin, float ScaleMax, float SinkM, float YawOffset)
{
	FWorldForgeScatter Out;
	if (!GMap.IsValid())
	{
		return Out;
	}
	const FWorldMap& M = *GMap;
	const uint32 Seed = WorldNoise::Hash(uint32(Kind.GetNumber()) ^ GetTypeHash(Kind.ToString()));
	const FString K = Kind.ToString();
	auto Add = [&](FVector2f P, float Yaw, float Scale, float Sink, float Tilt, WorldNoise::FRng& Rng)
	{
		const FVector2f L = P - GCenter;
		if (FMath::Abs(L.X) > GHalf - 1.0f || FMath::Abs(L.Y) > GHalf - 1.0f) return;
		if (GRoad.Num() > 1 && RoadDistance(P) < GRoadHalf + (K == TEXT("Pebble") ? 0.3f : 1.8f)) return;   // pista limpa
		const float Z = SliceHeight(M, P);
		const FRotator R(Rng.Range(-Tilt, Tilt), Yaw + YawOffset, Rng.Range(-Tilt, Tilt));
		Out.Transforms.Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - (Sink + SinkM) * Scale) * 100.0), FVector(Scale)));
	};
	if (K == TEXT("TreeBroadleaf") || K == TEXT("TreeConifer"))
	{
		// as árvores do gerador, uma por uma: a mesma posição, tipo e tamanho que
		// o painel mostra (WorldTrees.h). Spacing e escala daqui não valem.
		const WorldTrees::EKind Want = K == TEXT("TreeConifer") ? WorldTrees::EKind::Conifer : WorldTrees::EKind::Broadleaf;
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != EWorldPropType::Tree || WorldTrees::KindOf(M.Settings.Biome, P.Variant) != Want) continue;
			WorldNoise::FRng Rng(uint64(P.Variant) * 104729u + uint64(FMath::Abs(P.Pos.X * 31.0f + P.Pos.Y * 7.0f)));
			Add(P.Pos, P.Yaw, WorldTrees::ScaleOf(P, M.Settings.Biome), 0.3f, 2.0f, Rng);
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
	}
	if (K == TEXT("Undergrowth") || K == TEXT("Deadwood"))
	{
		// sub-bosque (samambaia, musgo) e madeira morta (toco, tronco caído):
		// dentro da mata e na borda dela, fora de trilha e rocha
		const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
		const bool bDead = K == TEXT("Deadwood");
		for (int32 V = 0; V < G; ++V)
		{
			for (int32 U = 0; U < G; ++U)
			{
				WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
				const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
				float W[6];
				DesertWeights(M, P, W);
				const bool bFlat = M.NormalAt(P).Z > 0.8f && M.HeightAt(P) > M.WaterLevel + 0.3f;
				const float Wood = WoodAt(M, P);
				const float Clump = WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, Seed + 7u, 3);
				const float Prob = (bDead ? 0.35f : 0.75f) * FMath::SmoothStep(-0.15f, 0.15f, Wood)
					* (bDead ? 1.0f : FMath::SmoothStep(-0.2f, 0.2f, Clump)) * (1.0f - W[2]) * (1.0f - W[3]) * (bFlat ? 1.0f : 0.0f);
				if (Rng.Float() >= Prob) continue;
				Add(P, Rng.Range(0.0f, 360.0f), FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), bDead ? 0.1f : 0.05f, bDead ? 4.0f : 8.0f, Rng);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
	}
	if (K == TEXT("RockLarge") || K == TEXT("Shrub"))
	{
		// as posições do gerador (rochas e "árvores" do deserto viram arbustos)
		const EWorldPropType Type = K == TEXT("RockLarge") ? EWorldPropType::Rock : EWorldPropType::Tree;
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != Type) continue;
			WorldNoise::FRng Rng(uint64(P.Variant) * 7919u + uint64(FMath::Abs(P.Pos.X * 13.0f + P.Pos.Y)));
			// arbusto no lugar da árvore do deserto: o tamanho da regra das árvores (WorldTrees.h) × ScaleMin
			const float Sc = Type == EWorldPropType::Tree ? WorldTrees::ScaleOf(P, M.Settings.Biome) * ScaleMin : FMath::Lerp(ScaleMin, ScaleMax, Rng.Float());
			Add(P.Pos, P.Yaw, Sc, 0.15f, Type == EWorldPropType::Rock ? 12.0f : 4.0f, Rng);
		}
	}
	if (K == TEXT("Cliff"))
	{
		// paredões escaneados nos cortes do terreno: onde a encosta é íngreme,
		// virados pra baixo dela, um a cada ~Spacing metros ao longo do corte
		const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
		for (int32 V = 0; V < G; ++V)
		{
			for (int32 U = 0; U < G; ++U)
			{
				WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
				const FVector2f P = GCenter + FVector2f((U + 0.3f + 0.4f * Rng.Float()) * SpacingM - GHalf, (V + 0.3f + 0.4f * Rng.Float()) * SpacingM - GHalf);
				const float E = 1.0f;
				const FVector2f Grad((M.HeightAt(P + FVector2f(E, 0)) - M.HeightAt(P - FVector2f(E, 0))) / (2 * E),
					(M.HeightAt(P + FVector2f(0, E)) - M.HeightAt(P - FVector2f(0, E))) / (2 * E));
				if (Grad.Size() < 1.0f) continue;   // mais de ~45°: corte de platô
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Grad.Y, -Grad.X));
				Add(P, Yaw, FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), 0.0f, 3.0f, Rng);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
		return Out;
	}
	// grade com sorteio por célula, densidade pelas camadas
	const int32 G = FMath::Max(1, FMath::RoundToInt(2.0f * GHalf / SpacingM));
	for (int32 V = 0; V < G; ++V)
	{
		for (int32 U = 0; U < G; ++U)
		{
			WorldNoise::FRng Rng(WorldNoise::Hash(U, V, Seed));
			const FVector2f P = GCenter + FVector2f((U + Rng.Float()) * SpacingM - GHalf, (V + Rng.Float()) * SpacingM - GHalf);
			float W[6];
			DesertWeights(M, P, W);
			const FVector3f N = M.NormalAt(P);
			const float Clump = WorldNoise::Fbm(P.X / 18.0f, P.Y / 18.0f, Seed + 3u, 3);
			float Prob = 0.0f;
			if (K == TEXT("Pebble")) Prob = 0.15f + 0.85f * W[1] + 0.4f * W[2] + 0.5f * W[4];
			else if (K == TEXT("RockSmall")) Prob = (0.05f + 0.7f * W[1] + 0.9f * FMath::SmoothStep(0.05f, 0.4f, W[3])) * (N.Z > 0.6f ? 1.0f : 0.2f);
			else if (K == TEXT("Tuft") && GForest) Prob = (0.25f + 0.75f * FMath::SmoothStep(-0.35f, 0.25f, Clump)) * (0.9f * W[0] + 0.2f * W[1] + 0.12f * W[2]) * (1.0f - 0.7f * W[2]) * (N.Z > 0.8f ? 1.0f : 0.0f);
			else if (K == TEXT("Tuft")) Prob = FMath::SmoothStep(-0.05f, 0.35f, Clump) * (0.3f * W[0] + 0.8f * W[1] + 0.2f * W[4]) * (1.0f - W[2]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			else if (K == TEXT("Shrub")) Prob = 0.25f * FMath::SmoothStep(0.1f, 0.5f, Clump) * (W[0] + W[1]) * (1.0f - W[2]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			else if (K == TEXT("Branch")) Prob = 0.05f * (W[0] + W[1]) * (N.Z > 0.85f ? 1.0f : 0.0f);
			if (Rng.Float() >= FMath::Clamp(Prob, 0.0f, 1.0f)) continue;
			const float Sc = FMath::Lerp(ScaleMin, ScaleMax, FMath::Square(Rng.Float()));   // mais pequenos que grandes
			Add(P, Rng.Range(0.0f, 360.0f), Sc, K == TEXT("Tuft") ? 0.02f : 0.2f, K == TEXT("Tuft") ? 6.0f : 25.0f, Rng);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice scatter %s: %d"), *K, Out.Transforms.Num());
	return Out;
}

TArray<FVector> UWorldForgeEditorLib::SliceTracks(float StepM)
{
	// as trilhas do gerador, reamostradas e suavizadas, só dentro do recorte;
	// cada trilha termina com um ponto (0,0,-1e9) separando da próxima
	TArray<FVector> Out;
	if (!GMap.IsValid())
	{
		return Out;
	}
	const FWorldMap& M = *GMap;
	for (const TArray<FVector2f>& Line : M.Tracks)
	{
		if (Line.Num() < 2) continue;
		// suaviza o zigue-zague da grade de 4 m (média móvel)
		TArray<FVector2f> Smooth;
		for (int32 I = 0; I < Line.Num(); ++I)
		{
			FVector2f Sum = FVector2f::ZeroVector;
			int32 C = 0;
			for (int32 K = FMath::Max(0, I - 3); K <= FMath::Min(Line.Num() - 1, I + 3); ++K) { Sum += Line[K]; ++C; }
			Smooth.Add(Sum / float(C));
		}
		bool bAny = false;
		float Carry = 0.0f;
		for (int32 I = 1; I < Smooth.Num(); ++I)
		{
			const FVector2f A = Smooth[I - 1], B = Smooth[I];
			const float Len = FVector2f::Distance(A, B);
			for (float T = Carry; T < Len; T += StepM)
			{
				const FVector2f P = FMath::Lerp(A, B, T / Len);
				const FVector2f L = P - GCenter;
				if (FMath::Abs(L.X) < GHalf && FMath::Abs(L.Y) < GHalf)
				{
					Out.Add(FVector(L.X * 100.0, L.Y * 100.0, SliceHeight(M, P) * 100.0));
					bAny = true;
				}
				Carry = T + StepM - Len;
			}
		}
		if (bAny) Out.Add(FVector(0, 0, -1e9));
	}
	return Out;
}

float UWorldForgeEditorLib::SliceHeightAt(FVector2D PointCm)
{
	return GMap.IsValid() ? SliceHeight(*GMap, ToMap(PointCm)) * 100.0f : 0.0f;
}

float UWorldForgeEditorLib::SliceRoadEndDistance(FVector2D PointCm)
{
	float EndM = 0.0f;
	return GRoad.Num() > 1 && RoadDistance(ToMap(PointCm), nullptr, &EndM) < 1e8f ? EndM : 0.0f;
}

float UWorldForgeEditorLib::SliceWaterLevel()
{
	return GMap.IsValid() ? GMap->WaterLevel * 100.0f : -1e9f;
}

FVector2D UWorldForgeEditorLib::SuggestSliceCenter(int32 Seed, int32 Players, int32 Biome, float MapSize, float SizeM)
{
	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	if (GSettingsOverride.IsSet())
	{
		S = GSettingsOverride.GetValue();
	}
	const TSharedPtr<FWorldMap> Map = FWorldGenerator::Generate(S);
	const FWorldMap& M = *Map;
	// os pesos dependem do bioma e da estrada: zera o que sobrou do recorte
	// anterior (montando várias fatias seguidas, o deserto pegava a estrada da neve)
	GForest = S.Biome == EWorldBiome::Temperate;
	GWinter = S.Biome == EWorldBiome::Winter;
	GFrozen = GWinter && M.WaterLevel > -100.0f;
	GRoad.Reset();
	GRoadZ.Reset();
	GRoadGrid.Reset();
	GRoadHalf = 0.0f;
	BuildCanopy(M);
	const float Half = 0.5f * SizeM, Margin = Half + 0.07f * MapSize;   // fora da moldura de montanhas
	FVector2D Best(0, 0);
	float BestScore = -1.0f;
	for (float CY = -M.Half() + Margin; CY <= M.Half() - Margin; CY += 32.0f)
	{
		for (float CX = -M.Half() + Margin; CX <= M.Half() - Margin; CX += 32.0f)
		{
			// média dos pesos num reticulado de 12 m dentro do quadrado
			float Sum[5] = { 0, 0, 0, 0, 0 };
			int32 Count = 0, Water = 0;
			for (float Y = -Half; Y <= Half; Y += 12.0f)
			{
				for (float X = -Half; X <= Half; X += 12.0f)
				{
					const FVector2f P(CX + X, CY + Y);
					float W[6];
					DesertWeights(M, P, W);
					for (int32 K = 0; K < 5; ++K) Sum[K] += W[K];
					Water += M.HeightAt(P) < M.WaterLevel ? 1 : 0;
					++Count;
				}
			}
			// variedade = entropia da mistura; trilha e rocha presentes valem mais
			float Entropy = 0.0f;
			for (int32 K = 0; K < 5; ++K)
			{
				const float F = Sum[K] / Count;
				if (F > 1e-4f) Entropy -= F * FMath::Loge(F);
			}
			const float Dirt = Sum[2] / Count, Rock = Sum[3] / Count, Wet = float(Water) / Count;
			// fora do deserto, um lago no recorte vale mais (o rio congelado do
			// Company of Heroes 2 é a cara do inverno; a lagoa, a da mata)
			const bool bLake = S.Biome != EWorldBiome::Desert && Wet > 0.03f && Wet < 0.22f;
			const float Score = Entropy + (Dirt > 0.04f ? 0.3f : 0.0f) + (Rock > 0.05f ? 0.3f : 0.0f) + (bLake ? 0.6f : 0.0f)
				- 2.0f * FMath::Max(0.0f, Wet - 0.1f);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = FVector2D(CX, CY);
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice center (%.0f, %.0f) score %.2f"), Best.X, Best.Y, BestScore);
	return Best;
}

AActor* UWorldForgeEditorLib::SpawnGroupActor(const FString& Label)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"), RF_Transactional);
	A->SetRootComponent(Root);
	A->AddInstanceComponent(Root);
	Root->RegisterComponent();
	A->SetActorLabel(Label);
	return A;
}

bool UWorldForgeEditorLib::AddInstances(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCastShadow, float CullDistanceM,
	const TArray<UMaterialInterface*>& Materials)
{
	if (!Actor || !Mesh)
	{
		return false;
	}
	// Em blocos de 64 m: um componente por malha e por bloco, como a vegetação
	// e o PCG da Unreal particionam (Instanced Foliage Actor, grade do PCG).
	// A câmera descarta o bloco inteiro fora da vista (CPU e sombras), e o modo
	// teste (F2) mostra bloco a bloco o que a câmera desenha.
	constexpr double CellCm = 6400.0;
	TMap<FIntPoint, TArray<FTransform>> Cells;
	for (const FTransform& X : Transforms)
	{
		const FVector L = X.GetLocation();
		Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / CellCm), FMath::FloorToInt(L.Y / CellCm))).Add(X);
	}
	for (const TPair<FIntPoint, TArray<FTransform>>& Cell : Cells)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transactional);
		C->SetStaticMesh(Mesh);
		C->SetMobility(EComponentMobility::Static);
		C->SetCastShadow(bCastShadow);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CullDistanceM > 0.0f)
		{
			// miúdo (capim, seixo, galho, muda: o que some de longe) fica fora da
			// iluminação global e do campo de distância — só o grande (terreno,
			// rocha, árvore) precisa estar lá; o campo de distância custava ~5 ms
			// por quadro na floresta (perfil de 06/10)
			C->SetCullDistances(0, int32(CullDistanceM * 100.0f));
			C->bAffectDistanceFieldLighting = false;
			C->bAffectDynamicIndirectLighting = false;
			C->bAffectIndirectLightingWhileHidden = false;
		}
		for (int32 I = 0; I < Materials.Num(); ++I)
		{
			if (Materials[I])
			{
				C->SetMaterial(I, Materials[I]);
			}
		}
		C->SetupAttachment(Actor->GetRootComponent());
		Actor->AddInstanceComponent(C);
		C->RegisterComponent();
		C->AddInstances(Cell.Value, false, true, false);
	}
	return true;
}

bool UWorldForgeEditorLib::EnableNanite(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
	Settings.bEnabled = true;
	Mesh->SetNaniteSettings(Settings);
	Mesh->Build();
	Mesh->PostEditChange();
	return Mesh->IsNaniteEnabled();
}

void UWorldForgeEditorLib::SetLayerInfoName(ULandscapeLayerInfoObject* LayerInfo, FName LayerName)
{
	if (LayerInfo)
	{
		LayerInfo->SetLayerName(LayerName, true);
	}
}

TArray<FVector> UWorldForgeEditorLib::SliceRoad(float StepM)
{
	TArray<FVector> Out;
	if (!GMap.IsValid() || GRoad.Num() < 2)
	{
		return Out;
	}
	const int32 Step = FMath::Max(1, FMath::RoundToInt(StepM));   // a linha já tem 1 m entre pontos
	for (int32 I = 0; I < GRoad.Num(); I += Step)
	{
		const FVector2f L = GRoad[I] - GCenter;
		if (FMath::Abs(L.X) < GHalf && FMath::Abs(L.Y) < GHalf)
		{
			Out.Add(FVector(L.X * 100.0, L.Y * 100.0, GRoadZ[I] * 100.0));
		}
	}
	return Out;
}

float UWorldForgeEditorLib::SliceRoadDistance(FVector2D PointCm)
{
	return GRoad.Num() > 1 ? RoadDistance(ToMap(PointCm)) * 100.0f : 1e9f;
}

UStaticMesh* UWorldForgeEditorLib::VegetationToStaticMesh(USkeletalMesh* Source, const FString& AssetPath, bool bBakeParts)
{
	if (!Source)
	{
		return nullptr;
	}
	FMeshDescription Mesh;
	if (!Source->CloneMeshDescription(0, Mesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE vegetation: %s has no mesh description"), *Source->GetName());
		return nullptr;
	}
	FStaticMeshAttributes Attributes(Mesh);
	Attributes.Register();

	// materiais: os mesmos da árvore, na mesma ordem (a assembly já aponta pra eles)
	TArray<FStaticMaterial> Materials;
	for (const FSkeletalMaterial& M : Source->GetMaterials())
	{
		Materials.Emplace(M.MaterialInterface, M.MaterialSlotName, M.ImportedMaterialSlotName);
	}
	auto MaterialIndexOf = [&Materials](UMaterialInterface* Mat, FName Slot) -> int32
	{
		for (int32 I = 0; I < Materials.Num(); ++I)
		{
			if ((Mat && Materials[I].MaterialInterface == Mat) || (!Mat && Materials[I].ImportedMaterialSlotName == Slot))
			{
				return I;
			}
		}
		return INDEX_NONE;
	};

	// pose de referência no espaço da malha (os ossos vêm em ordem: pai antes do filho)
	const FReferenceSkeleton& Ref = Source->GetRefSkeleton();
	const TArray<FTransform>& LocalPose = Ref.GetRefBonePose();
	TArray<FTransform> Pose;
	Pose.SetNum(LocalPose.Num());
	for (int32 B = 0; B < LocalPose.Num(); ++B)
	{
		const int32 Parent = Ref.GetParentIndex(B);
		Pose[B] = Parent >= 0 ? LocalPose[B] * Pose[Parent] : LocalPose[B];
	}

	const FNaniteAssemblyData& SourceAssembly = Source->GetNaniteSettings().NaniteAssemblyData;
	TArray<UStaticMesh*> Parts;
	TArray<TArray<int32>> PartRemap;   // material local da peça → material da árvore
	for (const FNaniteAssemblyPart& Part : SourceAssembly.Parts)
	{
		UObject* Object = Part.MeshObjectPath.TryLoad();
		UStaticMesh* Static = Cast<UStaticMesh>(Object);
		if (!Static && Object)
		{
			FString PartName = Object->GetName();
			PartName.RemoveFromStart(TEXT("SKM_"));
			const FString Folder = FPackageName::GetLongPackagePath(Object->GetOutermost()->GetName());
			Static = LoadObject<UStaticMesh>(nullptr, *(Folder / PartName + TEXT(".") + PartName));
		}
		TArray<int32> Remap;
		if (Static)
		{
			const TArray<FStaticMaterial>& PartMaterials = Static->GetStaticMaterials();
			for (int32 L = 0; L < PartMaterials.Num(); ++L)
			{
				int32 Index = MaterialIndexOf(PartMaterials[L].MaterialInterface, PartMaterials[L].ImportedMaterialSlotName);
				if (Index == INDEX_NONE)
				{
					Index = Materials.Emplace(PartMaterials[L].MaterialInterface, PartMaterials[L].MaterialSlotName, PartMaterials[L].ImportedMaterialSlotName);
				}
				Remap.Add(Index);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("FORGE vegetation part %s -> %s"), *Part.MeshObjectPath.ToString(), Static ? *Static->GetPathName() : TEXT("(missing)"));
		Parts.Add(Static);
		PartRemap.Add(Remap);
	}

	auto NodeTransform = [&Pose](const FNaniteAssemblyNode& Node) -> FTransform
	{
		FTransform T(Node.Transform);
		if (Node.TransformSpace == ENaniteAssemblyNodeTransformSpace::BoneRelative && Node.BoneInfluences.Num() > 0)
		{
			const FNaniteAssemblyBoneInfluence* Best = &Node.BoneInfluences[0];
			for (const FNaniteAssemblyBoneInfluence& I : Node.BoneInfluences)
			{
				Best = I.BoneWeight > Best->BoneWeight ? &I : Best;
			}
			if (Pose.IsValidIndex(Best->BoneIndex))
			{
				T = T * Pose[Best->BoneIndex];
			}
		}
		return T;
	};

	// os grupos de polígonos da malha apontam pros materiais pelo nome
	TMap<int32, FPolygonGroupID> GroupOfMaterial;
	{
		TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		for (const FPolygonGroupID G : Mesh.PolygonGroups().GetElementIDs())
		{
			const int32 Index = MaterialIndexOf(nullptr, SlotNames[G]);
			GroupOfMaterial.Add(Index == INDEX_NONE ? G.GetValue() : Index, G);
		}
	}

	int64 BakedTriangles = 0;
	FNaniteAssemblyData Assembly;
	if (bBakeParts)
	{
		TArray<TOptional<FMeshDescription>> PartMeshes;
		PartMeshes.SetNum(Parts.Num());
		for (const FNaniteAssemblyNode& Node : SourceAssembly.Nodes)
		{
			if (!Parts.IsValidIndex(Node.PartIndex) || !Parts[Node.PartIndex])
			{
				continue;
			}
			TOptional<FMeshDescription>& PartMesh = PartMeshes[Node.PartIndex];
			if (!PartMesh.IsSet())
			{
				const FMeshDescription* Desc = Parts[Node.PartIndex]->GetMeshDescription(0);
				if (!Desc)
				{
					continue;
				}
				PartMesh = *Desc;
				FStaticMeshAttributes(*PartMesh).Register();
			}
			const TArray<int32>& Remap = PartRemap[Node.PartIndex];
			const TArray<FStaticMaterial>& PartMaterials = Parts[Node.PartIndex]->GetStaticMaterials();
			FStaticMeshOperations::FAppendSettings Append;
			Append.MeshTransform = NodeTransform(Node);
			Append.PolygonGroupsDelegate = FAppendPolygonGroupsDelegate::CreateLambda(
				[&](const FMeshDescription& From, FMeshDescription& To, PolygonGroupMap& Map)
				{
					const TPolygonGroupAttributesConstRef<FName> FromNames = FStaticMeshConstAttributes(From).GetPolygonGroupMaterialSlotNames();
					TPolygonGroupAttributesRef<FName> ToNames = FStaticMeshAttributes(To).GetPolygonGroupMaterialSlotNames();
					for (const FPolygonGroupID G : From.PolygonGroups().GetElementIDs())
					{
						int32 Local = G.GetValue();
						for (int32 L = 0; L < PartMaterials.Num(); ++L)
						{
							if (PartMaterials[L].ImportedMaterialSlotName == FromNames[G])
							{
								Local = L;
							}
						}
						const int32 Final = Remap.IsValidIndex(Local) ? Remap[Local] : 0;
						FPolygonGroupID* Target = GroupOfMaterial.Find(Final);
						if (!Target)
						{
							const FPolygonGroupID New = To.CreatePolygonGroup();
							ToNames[New] = Materials[Final].ImportedMaterialSlotName;
							Target = &GroupOfMaterial.Add(Final, New);
						}
						Map.Add(G, *Target);
					}
				});
			FStaticMeshOperations::AppendMeshDescription(*PartMesh, Mesh, Append);
			BakedTriangles += PartMesh->Triangles().Num();
		}
	}
	else
	{
		for (int32 P = 0; P < Parts.Num(); ++P)
		{
			FNaniteAssemblyPart& Part = Assembly.Parts.AddDefaulted_GetRef();
			Part.MeshObjectPath = FSoftObjectPath(Parts[P]);
			Part.MaterialRemap = PartRemap[P];
		}
		for (const FNaniteAssemblyNode& Node : SourceAssembly.Nodes)
		{
			if (Parts.IsValidIndex(Node.PartIndex) && Parts[Node.PartIndex])
			{
				FNaniteAssemblyNode& Out = Assembly.Nodes.AddDefaulted_GetRef();
				Out.PartIndex = Node.PartIndex;
				Out.TransformSpace = ENaniteAssemblyNodeTransformSpace::Local;
				Out.Transform = FTransform3f(NodeTransform(Node));
			}
		}
	}

	// seções: uma por grupo de polígonos, na ordem, com o material pelo nome
	TArray<int32> SectionMaterials;
	{
		TPolygonGroupAttributesRef<FName> SlotNames = FStaticMeshAttributes(Mesh).GetPolygonGroupMaterialSlotNames();
		for (const FPolygonGroupID G : Mesh.PolygonGroups().GetElementIDs())
		{
			const int32 Index = MaterialIndexOf(nullptr, SlotNames[G]);
			SectionMaterials.Add(Index == INDEX_NONE ? 0 : Index);
		}
	}
	const int32 Triangles = Mesh.Triangles().Num();

	// a malha estática nova (ou a de antes, refeita)
	const FString Name = FPackageName::GetShortName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	UStaticMesh* Out = FindObject<UStaticMesh>(Package, *Name);
	if (!Out)
	{
		Out = NewObject<UStaticMesh>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Out);
	}
	Out->Modify();
	Out->SetNumSourceModels(1);
	FStaticMeshSourceModel& Model = Out->GetSourceModel(0);
	Model.BuildSettings.bRecomputeNormals = false;
	Model.BuildSettings.bRecomputeTangents = false;
	Model.BuildSettings.bGenerateLightmapUVs = false;
	Out->SetStaticMaterials(Materials);
	Out->GetSectionInfoMap().Clear();
	for (int32 S = 0; S < SectionMaterials.Num(); ++S)
	{
		Out->GetSectionInfoMap().Set(0, S, FMeshSectionInfo(SectionMaterials[S]));
	}
	Out->CreateMeshDescription(0, MoveTemp(Mesh));
	Out->CommitMeshDescription(0);
	FMeshNaniteSettings Nanite = Source->GetNaniteSettings();
	Nanite.bEnabled = true;
	Nanite.NaniteAssemblyData = Assembly;
	Out->SetNaniteSettings(Nanite);
	Out->Build(true);
	Out->PostEditChange();
	Out->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE vegetation %s: %d triangles (%lld baked from parts), %d parts, %d assembly nodes (source %d), %d materials, extent %s"),
		*Name, Triangles, BakedTriangles, Parts.Num(), Assembly.Nodes.Num(), SourceAssembly.Nodes.Num(), Materials.Num(), *Out->GetBounds().BoxExtent.ToString());
	return Out;
}

bool UWorldForgeEditorLib::AddSnowToMaterial(UMaterial* Material)
{
	UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
	if (!Data || !Material->bUseMaterialAttributes || !Data->MaterialAttributes.Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE snow: %s does not use material attributes"), Material ? *Material->GetName() : TEXT("null"));
		return false;
	}
	UMaterialExpression* Source = Data->MaterialAttributes.Expression;
	const int32 SourceOutput = Data->MaterialAttributes.OutputIndex;

	auto* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionBreakMaterialAttributes::StaticClass(), 2400, 0));
	Break->MaterialAttributes.Connect(SourceOutput, Source);
	auto OutputOf = [Break](const TCHAR* OutputName)
	{
		for (int32 I = 0; I < Break->Outputs.Num(); ++I)
		{
			if (Break->Outputs[I].OutputName == FName(OutputName))
			{
				return I;
			}
		}
		return 0;
	};
	auto* Snow = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), 2400, 300));
	Snow->ParameterName = TEXT("Snow");
	Snow->DefaultValue = 0.0f;
	UMaterialExpression* Normal = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVertexNormalWS::StaticClass(), 2400, 400);
	UMaterialExpression* WorldPos = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionWorldPosition::StaticClass(), 2400, 500);

	// Neve assenta no que olha pra cima (a normal do vértice: nas folhas do PVE
	// ela aponta pra fora da copa, então o topo da copa embranquece) e falha em
	// manchas de ~3 m (o vento varre). Rugosidade da neve ~0.65.
	auto* Code = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), 2700, 200));
	Code->OutputType = CMOT_Float3;
	Code->Code = TEXT(
		"float n = frac(sin(dot(floor(P.xy / 300.0), float2(12.9898, 78.233))) * 43758.5453);\n"
		"float up = saturate((N.z - 0.1) * 1.8) * lerp(0.75, 1.0, n);\n"
		"float s = saturate(Snow * up);\n"
		"Rough = lerp(R, 0.65, s);\n"
		"return lerp(C, float3(0.80, 0.83, 0.88), s);");
	Code->Inputs.Empty();
	for (const TCHAR* InputName : { TEXT("C"), TEXT("R"), TEXT("N"), TEXT("P"), TEXT("Snow") })
	{
		FCustomInput& In = Code->Inputs.AddDefaulted_GetRef();
		In.InputName = InputName;
	}
	Code->Inputs[0].Input.Connect(OutputOf(TEXT("BaseColor")), Break);
	Code->Inputs[1].Input.Connect(OutputOf(TEXT("Roughness")), Break);
	Code->Inputs[2].Input.Connect(0, Normal);
	Code->Inputs[3].Input.Connect(0, WorldPos);
	Code->Inputs[4].Input.Connect(0, Snow);
	FCustomOutput& Rough = Code->AdditionalOutputs.AddDefaulted_GetRef();
	Rough.OutputName = TEXT("Rough");
	Rough.OutputType = CMOT_Float1;
	Code->RebuildOutputs();

	auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionSetMaterialAttributes::StaticClass(), 3000, 0));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->ConnectInputAttribute(MP_BaseColor, Code, 0);
	Set->ConnectInputAttribute(MP_Roughness, Code, 1);
	Data->MaterialAttributes.Connect(0, Set);
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE snow added to %s (break outputs: base %d, rough %d)"), *Material->GetName(), OutputOf(TEXT("BaseColor")), OutputOf(TEXT("Roughness")));
	return true;
}

bool UWorldForgeEditorLib::LoadWorldSettings(const FString& JsonPath, int32& OutBiome, float& OutMapSize)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE world settings: could not read %s"), *JsonPath);
		return false;
	}
	FWorldGenSettings S;
	S.Seed = int32(J->GetNumberField(TEXT("seed")));
	S.Players = int32(J->GetNumberField(TEXT("players")));
	S.MapSize = float(J->GetNumberField(TEXT("size")));
	S.Biome = EWorldBiome(FMath::Clamp(int32(J->GetNumberField(TEXT("biome"))), 0, 2));
	S.Relief = float(J->GetNumberField(TEXT("relief")));
	S.Plateaus = float(J->GetNumberField(TEXT("plateaus")));
	S.Water = float(J->GetNumberField(TEXT("water")));
	S.Erosion = float(J->GetNumberField(TEXT("erosion")));
	S.Forests = float(J->GetNumberField(TEXT("forests")));
	S.Resources = float(J->GetNumberField(TEXT("resources")));
	GSettingsOverride = S;
	OutBiome = int32(S.Biome);
	OutMapSize = S.MapSize;
	UE_LOG(LogTemp, Display, TEXT("FORGE world settings: seed %d, %d players, %.0f m, biome %d"), S.Seed, S.Players, S.MapSize, OutBiome);
	return true;
}
