#include "WorldForgeEditorLib.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Generation/WorldDetail.h"
#include "Generation/WorldImport.h"
#include "Generation/WorldSettingsJson.h"
#include "Generation/WorldGenerator.h"
#include "Generation/WorldNoise.h"
#include "Generation/WorldTrees.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "MaterialShared.h"
#include "Materials/MaterialInstance.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeEditTypes.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Algo/AnyOf.h"
#include "ImageUtils.h"
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
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// o mundo de verdade do último recorte gerado (estradas, máscaras, pesos do
	// chão, altura): as outras funções (espalhamento, trilhas, água) leem dele
	FWorldDetail GD;
	// as edições à mão do mapa em curso (LoadWorldSettings)
	TSharedPtr<FWorldEdits> GEditsOverride;
	FString GImportOverride;   // mapa trazido de fora (WorldImport), no lugar do gerador

	/** O mapa dos controles: o importado quando há, senão o do gerador. */
	TSharedPtr<FWorldMap> MapOf(const FWorldGenSettings& S)
	{
		if (!GImportOverride.IsEmpty())
		{
			TSharedPtr<FWorldEdits> Ignored;
			FString Note;
			if (TSharedPtr<FWorldMap> M = WorldImport::Load(GImportOverride, Ignored, Note))
			{
				return M;
			}
		}
		return FWorldGenerator::Generate(S);
	}
	TOptional<FWorldGenSettings> GSettingsOverride;

	/** O asset do caminho: o que já existe (carregado do disco se preciso) ou um novo. Só o FindObject não acha o
	 *  que está no disco e não foi carregado, e o NewObject por cima dele dava "carga inválida" no bulk data da
	 *  textura (o ensure do T_RoadDir_World_TestAsph2 e do T_Water_World_TestSalt2, 13/10). */
	template <typename T>
	T* LoadOrCreateAsset(const FString& AssetPath)
	{
		const FString Name = FPackageName::GetShortName(AssetPath);
		if (FPackageName::DoesPackageExist(AssetPath))
		{
			if (T* Existing = LoadObject<T>(nullptr, *(AssetPath + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Existing;
			}
		}
		UPackage* Package = CreatePackage(*AssetPath);
		T* Obj = FindObject<T>(Package, *Name);
		if (!Obj)
		{
			Obj = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Obj);
		}
		return Obj;
	}

	/** As alturas e os pesos das camadas do Landscape (GD já montado), em 8 bits somando 255 (o resto vai pra
	 *  camada base). */
	void SliceHeightsAndWeights(int32 N, float QuadSizeM, int32 NumLayers, TArray<uint16>& Heights, TArray<TArray<uint8>>& Weights)
	{
		const FWorldMap& M = *GD.GMap;
		Heights.SetNumUninitialized(N * N);
		Weights.SetNum(NumLayers);
		for (TArray<uint8>& L : Weights) L.SetNumUninitialized(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const FVector2f P = GD.GCenter + FVector2f(X * QuadSizeM - GD.GHalf, Y * QuadSizeM - GD.GHalf);
				const float H = GD.SliceHeight(M, P);
				float W[6];
				GD.DesertWeights(M, P, W);
				if (GD.GSlab && NumLayers == 6 && W[5] > 0.0f)
				{
					// a laje (BuildRoadSlabs) é o asfalto; o chão debaixo e nos buracos dela é a areia. A estrada "colada no
					// chão" (16/10) fica com o asfalto do chão
					const FWorldDetail::FRoadHit Hr = GD.RoadAt(P);
					if (Hr.Road == INDEX_NONE || GD.SlabOn(Hr)) W[5] = 0.0f;
				}
				// alturas: 32768 = 0 m; com escala Z de 100 cada unidade vale 1/128 m
				// (LANDSCAPE_ZSCALE): ±256 m com precisão de ~8 mm
				Heights[Y * N + X] = uint16(FMath::Clamp(FMath::RoundToInt(32768.0f + H * 128.0f), 0, 65535));
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
	}

	/** Por componente (os vértices da borda contam pros dois vizinhos): o peso máximo de cada camada. */
	void ComponentLayerMax(int32 N, int32 Components, int32 QuadsPerComponent, const TArray<TArray<uint8>>& Weights, TArray<TArray<uint8>>& OutMax)
	{
		OutMax.SetNum(Components * Components);
		for (int32 CY = 0; CY < Components; ++CY)
		{
			for (int32 CX = 0; CX < Components; ++CX)
			{
				TArray<uint8>& Mx = OutMax[CY * Components + CX];
				Mx.Init(0, Weights.Num());
				for (int32 K = 0; K < Weights.Num(); ++K)
				{
					for (int32 Y = CY * QuadsPerComponent; Y <= (CY + 1) * QuadsPerComponent; ++Y)
					{
						for (int32 X = CX * QuadsPerComponent; X <= (CX + 1) * QuadsPerComponent; ++X)
						{
							Mx[K] = FMath::Max(Mx[K], Weights[K][Y * N + X]);
						}
					}
				}
			}
		}
	}

	/** A limpeza das camadas por componente (14/10, a otimização do chão: a Unreal só tira a camada do componente
	 *  quando o peso é zero exato, LandscapeEdit.cpp Import, IsLayerAllZero; 1/255 já põe a camada inteira no
	 *  shader daquele pedaço). Camada com peso máximo ≤ Cull (de 255) no componente sai dele, e o peso vai pra
	 *  camada dominante de cada vértice (a soma continua 255). Devolve quantas alocações saíram. */
	int32 CullComponentLayers(int32 N, int32 Components, int32 QuadsPerComponent, int32 Cull, TArray<TArray<uint8>>& Weights)
	{
		if (Cull <= 0)
		{
			return 0;
		}
		TArray<TArray<uint8>> Mx;
		ComponentLayerMax(N, Components, QuadsPerComponent, Weights, Mx);
		int32 Removed = 0;
		for (int32 CY = 0; CY < Components; ++CY)
		{
			for (int32 CX = 0; CX < Components; ++CX)
			{
				for (int32 K = 0; K < Weights.Num(); ++K)
				{
					const uint8 V = Mx[CY * Components + CX][K];
					if (V == 0 || V > Cull)
					{
						continue;
					}
					++Removed;
					for (int32 Y = CY * QuadsPerComponent; Y <= (CY + 1) * QuadsPerComponent; ++Y)
					{
						for (int32 X = CX * QuadsPerComponent; X <= (CX + 1) * QuadsPerComponent; ++X)
						{
							const int32 I = Y * N + X;
							const uint8 W = Weights[K][I];
							if (W == 0)
							{
								continue;
							}
							int32 Best = -1;
							for (int32 J = 0; J < Weights.Num(); ++J)
							{
								if (J != K && (Best < 0 || Weights[J][I] > Weights[Best][I])) Best = J;
							}
							Weights[K][I] = 0;
							Weights[Best][I] = uint8(FMath::Min(255, Weights[Best][I] + W));
						}
					}
				}
			}
		}
		return Removed;
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
	// a principal: asfalto com a 6ª camada (deserto, neve); sem ela (floresta),
	// estrada de terra de 6 m. As de terra têm 4,4 m (pista simples de estrada
	// de serviço: 3,5–4,5 m, Keller & Sherar 2003)
	const bool bPaved = NumLayers == 6 && RoadWidthM > 0.0f;
	GD.Edits = GEditsOverride;   // as edições à mão do app (apagados, pintura, altura); a altura entra no Setup
	GD.Setup(MapOf(S), FVector2f(CenterM.X, CenterM.Y), 0.5f * (N - 1) * QuadSizeM, bPaved ? 0.5f * RoadWidthM : 3.0f, bPaved);
	GD.BuildWeightCache(1.0f);   // o espalhamento sorteia pela grade de 1 m, como a vista ao vivo

	TArray<uint16> Heights;
	TArray<TArray<uint8>> Weights;
	SliceHeightsAndWeights(N, QuadSizeM, NumLayers, Heights, Weights);
	const int32 Cull = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("LAYER_CULL")));
	if (Cull > 0)
	{
		const int32 Removed = CullComponentLayers(N, Components, QuadsPerComponent, Cull, Weights);
		UE_LOG(LogTemp, Display, TEXT("FORGE layer cull <= %d/255: %d alocacoes fora"), Cull, Removed);
	}

	// o mesmo caminho do "New Landscape" do editor (LandscapeEditorDetailCustomization_NewLandscape.cpp)
	const FVector Scale(QuadSizeM * 100.0, QuadSizeM * 100.0, 100.0);
	const FVector Location(-GD.GHalf * 100.0, -GD.GHalf * 100.0, 0.0);
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
	UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape %dx%d verts, %.0f m, seed %d, center (%.0f, %.0f)"), N, N, 2.0f * GD.GHalf, S.Seed, GD.GCenter.X, GD.GCenter.Y);
	return Landscape;
}

FWorldForgeScatter UWorldForgeEditorLib::SliceScatter(FName Kind, float SpacingM, float ScaleMin, float ScaleMax, float SinkM, float YawOffset)
{
	FWorldForgeScatter Out;
	TArray<FWorldDetail::FScatterCand> Cands;
	GD.ScatterCandidates(Kind.ToString(), SpacingM, ScaleMin, ScaleMax, SinkM, YawOffset, Cands);
	if (!GD.GMap.IsValid())
	{
		return Out;
	}
	for (const FWorldDetail::FScatterCand& C : Cands)
	{
		WorldNoise::FRng Rng(uint64(C.Seed) * 7u + 1u);
		const FVector2f L = C.P - GD.GCenter;
		const float Z = GD.SliceHeight(*GD.GMap, C.P);
		const FRotator R(Rng.Range(-C.Tilt, C.Tilt), C.Yaw, Rng.Range(-C.Tilt, C.Tilt));
		Out.Transforms.Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - C.Sink * C.Scale) * 100.0), FVector(C.Scale)));
	}
	return Out;
}

TArray<FWorldForgeScatter> UWorldForgeEditorLib::SliceScatterMeshes(FName Kind, float SpacingM, float ScaleMin, float ScaleMax,
	const TArray<UStaticMesh*>& Meshes, float SinkM)
{
	TArray<FWorldForgeScatter> Out;
	Out.SetNum(Meshes.Num());
	if (!GD.GMap.IsValid() || Meshes.Num() == 0)
	{
		return Out;
	}
	TArray<FWorldDetail::FScatterCand> Cands;
	GD.ScatterCandidates(Kind.ToString(), SpacingM, ScaleMin, ScaleMax, SinkM, 0.0f, Cands);
	TArray<FBox> Bounds;
	for (const UStaticMesh* Mesh : Meshes)
	{
		Bounds.Add(Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit));
	}
	TArray<TArray<FTransform>> Xs;
	GD.PlaceMeshes(Kind.ToString(), Cands, Bounds, Xs);
	for (int32 I = 0; I < Out.Num(); ++I)
	{
		Out[I].Transforms = MoveTemp(Xs[I]);
	}
	return Out;
}

namespace
{
	/**
	 * Pontos das estradas (cm, cota do leito) dentro do recorte, a cada StepM,
	 * com (0,0,−1e9) separando os trechos. bMarks: só onde cabe pintura e mureta
	 * (fora dos entroncamentos e a mais de 50 m de uma ponta solta).
	 */
	TArray<FVector> RoadPoints(float StepM, bool bPaved, bool bMarks)
	{
		TArray<FVector> Out;
		const int32 Step = FMath::Max(1, FMath::RoundToInt(StepM));   // a linha já tem 1 m entre pontos
		for (int32 R = 0; R < GD.GRoads.Num(); ++R)
		{
			const FWorldDetail::FRoad& Road = GD.GRoads[R];
			if (Road.bPaved != bPaved) continue;
			bool bRun = false;
			for (int32 I = 0; I < Road.P.Num(); I += Step)
			{
				const FVector2f L = Road.P[I] - GD.GCenter;
				bool bKeep = FMath::Abs(L.X) < GD.GHalf && FMath::Abs(L.Y) < GD.GHalf;
				if (bKeep && bMarks)
				{
					// (na escala do desfazer de cada ponta: 50 m na do gerador, ~4,5 m na desenhada)
					const float End = 45.0f * FMath::Min(Road.bOpen[0] ? float(I) / Road.Fade[0] : 1e9f, Road.bOpen[1] ? float(Road.P.Num() - 1 - I) / Road.Fade[1] : 1e9f);
					const FWorldDetail::FRoadHit Other = GD.RoadAt(Road.P[I], R);
					bKeep = End > 50.0f && (Other.Road == INDEX_NONE || Other.D > Other.Half() + Road.Half + 4.0f);
				}
				if (bKeep)
				{
					Out.Add(FVector(L.X * 100.0, L.Y * 100.0, Road.Z[I] * 100.0));
					bRun = true;
				}
				else if (bRun)
				{
					Out.Add(FVector(0, 0, -1e9));
					bRun = false;
				}
			}
			if (bRun) Out.Add(FVector(0, 0, -1e9));
		}
		return Out;
	}
}

TArray<FVector> UWorldForgeEditorLib::SliceTracks(float StepM)
{
	// as marcas de pneu vão nas estradas de terra (a rede do recorte), na cota
	// do chão (o leito pode estar um pouco abaixo da média do relevo)
	if (!GD.GMap.IsValid()) return TArray<FVector>();
	TArray<FVector> Out = RoadPoints(StepM, false, false);
	for (FVector& P : Out)
	{
		if (P.Z > -1e8) P.Z = GD.SliceHeight(*GD.GMap, GD.ToMap(FVector2D(P.X, P.Y))) * 100.0;
	}
	return Out;
}

float UWorldForgeEditorLib::SliceHeightAt(FVector2D PointCm)
{
	return GD.GMap.IsValid() ? GD.SliceHeight(*GD.GMap, GD.ToMap(PointCm)) * 100.0f : 0.0f;
}

FString UWorldForgeEditorLib::SliceVillageJson(const FString& MapName)
{
	return GD.GVillage.Num() ? GD.VillageJson(MapName) : FString();
}

FString UWorldForgeEditorLib::SliceVillageHash()
{
	return GD.GVillage.Num() ? FString::Printf(TEXT("%08x"), GD.VillageHash()) : FString();
}

float UWorldForgeEditorLib::SliceWindYaw()
{
	return GD.GMap.IsValid() ? GD.GMap->WindYaw : -68.0f;
}

void UWorldForgeEditorLib::SliceMonoliths(TArray<FString>& OutMeshes, TArray<FTransform>& OutTransforms)
{
	OutMeshes.Reset();
	OutTransforms.Reset();
	if (!GD.GMap.IsValid()) return;
	for (const FWorldMonolith& Mo : GD.GMap->Monoliths)
	{
		OutMeshes.Add(Mo.Mesh);
		OutTransforms.Add(FTransform(FRotator(0.0f, Mo.Yaw, 0.0f), FVector(Mo.Pos.X * 100.0, Mo.Pos.Y * 100.0, Mo.Z * 100.0), FVector(Mo.Scale)));
	}
}

float UWorldForgeEditorLib::SliceWindSpeed()
{
	return GD.GMap.IsValid() ? GD.GMap->WindSpeed : 5.0f;
}

float UWorldForgeEditorLib::SliceRoadEndDistance(FVector2D PointCm)
{
	const FWorldDetail::FRoadHit Hit = GD.RoadAt(GD.ToMap(PointCm), INDEX_NONE, true);
	return Hit.Road != INDEX_NONE ? FMath::Min(Hit.End, 1e6f) : 0.0f;
}

float UWorldForgeEditorLib::SliceWaterLevel()
{
	return GD.GMap.IsValid() ? GD.GMap->WaterLevel * 100.0f : -1e9f;
}

bool UWorldForgeEditorLib::SliceHasSea()
{
	return GD.GMap.IsValid() && (GD.GMap->Frame == EWorldFrame::Coast || GD.GMap->Frame == EWorldFrame::Island);
}

void UWorldForgeEditorLib::ResetSliceMap()
{
	GD.GMap.Reset();
}

bool UWorldForgeEditorLib::DebugSliceMaps(const FString& Folder)
{
	if (!GSettingsOverride.IsSet())
	{
		return false;
	}
	const FWorldGenSettings S = GSettingsOverride.GetValue();
	// as estradas como no mapa inteiro (build_slice.py: deserto 7,5 m e neve 6 m asfaltadas; floresta de terra)
	float RoadHalf;
	bool bPaved;
	FWorldDetail::MainRoadOf(S.Biome, RoadHalf, bPaved);
	GD.Edits = GEditsOverride;
	GD.Setup(MapOf(S), FVector2f::ZeroVector, 0.5f * S.MapSize, RoadHalf, bPaved);
	const FWorldMap& M = *GD.GMap;
	const int32 R = FMath::Clamp(FMath::RoundToInt(S.MapSize), 64, 2048);
	TArray<FColor> Shade, Basin, Wadi, W012, W345, ToneImg;
	for (TArray<FColor>* A : { &Shade, &Basin, &Wadi, &W012, &W345, &ToneImg }) A->SetNumUninitialized(R * R);
	const bool bSaltFlat = GD.GSaltDist.Num() == M.N * M.N && Algo::AnyOf(GD.GSaltDist, [](float D) { return D > -23.5f; });
	const FVector3f Sun = FVector3f(-0.5f, -0.6f, 0.62f).GetSafeNormal();
	ParallelFor(R, [&](int32 J)
	{
		for (int32 I = 0; I < R; ++I)
		{
			// linha 0 da imagem = norte (y alto), como o mapa visto de cima
			const FVector2f P(((I + 0.5f) / R - 0.5f) * S.MapSize, (0.5f - (J + 0.5f) / R) * S.MapSize);
			const int32 C = J * R + I;
			const float H = M.HeightAt(P);
			const bool bWet = H < M.WaterLevel;
			const float L = FMath::Clamp(FVector3f::DotProduct(M.NormalAt(P), Sun), 0.0f, 1.0f);
			const uint8 G = uint8(FMath::Clamp(40.0f + 215.0f * L, 0.0f, 255.0f));
			Shade[C] = bWet ? FColor(30, 60, uint8(90 + 0.5f * G), 255) : FColor(G, G, G, 255);
			const FVector2f Gr = M.ToGrid(P);
			const int32 V = M.Index(FMath::Clamp(FMath::RoundToInt(Gr.X), 0, M.N - 1), FMath::Clamp(FMath::RoundToInt(Gr.Y), 0, M.N - 1));
			const uint8 B = M.Basin.Num() == M.N * M.N ? M.Basin[V] : 0;
			const FWorldDetail::FSaltShape Sh = GD.SaltShapeAt(M, P);
			const FColor BasinColors[4] = { FColor(G / 3, G / 3, G / 3, 255), FColor(255, 255, 255, 255), FColor(40, 120, 255, 255), FColor(255, 140, 0, 255) };
			Basin[C] = BasinColors[FMath::Min<int32>(B, 3)];
			Basin[C].G = uint8(FMath::Max<float>(Basin[C].G, 255.0f * Sh.Crust * 0.6f));
			const float Fl = M.Flow.Num() == M.N * M.N ? M.Flow[V] : 0.0f;
			Wadi[C] = FColor(uint8(255.0f * GD.WadiAt(M, P)), uint8(255.0f * Fl), uint8(G / 2), 255);
			float W[6];
			GD.DesertWeights(M, P, W);
			W012[C] = FColor(uint8(255.0f * W[0]), uint8(255.0f * W[1]), uint8(255.0f * W[2]), 255);
			W345[C] = FColor(uint8(255.0f * W[3]), uint8(255.0f * W[4]), uint8(255.0f * W[5]), 255);
			// tom: vermelho = + (lavado / seco), azul = − (úmido / viçoso)
			const float T = GD.GroundToneAt(M, P, bSaltFlat);
			ToneImg[C] = FColor(uint8(G / 3 + 170.0f * FMath::Max(T, 0.0f)), uint8(G / 3), uint8(G / 3 + 170.0f * FMath::Max(-T, 0.0f)), 255);
		}
	});
	bool bOk = true;
	auto Save = [&](const TCHAR* Name, const TArray<FColor>& Pixels)
	{
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(R, R, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		bOk &= FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Folder, Name));
	};
	IFileManager::Get().MakeDirectory(*Folder, true);
	Save(TEXT("shade.png"), Shade);
	Save(TEXT("basin.png"), Basin);
	Save(TEXT("wadi.png"), Wadi);
	Save(TEXT("w012_sand_gravel_dirt.png"), W012);
	Save(TEXT("w345_rock_salt_asphalt.png"), W345);
	Save(TEXT("tone.png"), ToneImg);
	// a vila (o JSON que o jogo lê), pra conferir a planta
	if (GD.GVillage.Num())
	{
		FFileHelper::SaveStringToFile(GD.VillageJson(TEXT("debug")), *(Folder / TEXT("village.json")));
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE debug maps %s (%d px, %.0f m)"), *Folder, R, S.MapSize);
	return bOk;
}

TArray<FVector2D> UWorldForgeEditorLib::SliceCandidates(FName Kind, float SpacingM)
{
	TArray<FVector2D> Out;
	TArray<FWorldDetail::FScatterCand> Cands;
	GD.ScatterCandidates(Kind.ToString(), SpacingM, 1.0f, 1.0f, 0.0f, 0.0f, Cands);
	for (const FWorldDetail::FScatterCand& C : Cands)
	{
		Out.Add(FVector2D(C.P.X, C.P.Y));
	}
	return Out;
}

void UWorldForgeEditorLib::BuildLandscapeNanite(ALandscape* Landscape)
{
	// Chão em Nanite (Landscape > Nanite > Enable Nanite, o mesmo caminho do
	// painel do editor): o chão comum entupia a fila das sombras virtuais ("[VSM]
	// Non-Nanite Marking Job Queue overflow") e era o maior pedaço da tela sem o
	// detalhe automático por distância (preto no F3). Depois do material: trocar
	// o material refaz a malha Nanite (~3 min no mapa inteiro)
	UWorld* World = Landscape ? Landscape->GetWorld() : nullptr;
	FBoolProperty* NaniteProp = FindFProperty<FBoolProperty>(ALandscapeProxy::StaticClass(), TEXT("bEnableNanite"));
	if (!World || !NaniteProp)
	{
		return;
	}
	NaniteProp->SetPropertyValue_InContainer(Landscape, true);
	FPropertyChangedEvent Changed(NaniteProp);
	Landscape->PostEditChangeProperty(Changed);
	if (ULandscapeSubsystem* LS = World->GetSubsystem<ULandscapeSubsystem>())
	{
		ALandscapeProxy* Proxy = Landscape;
		LS->BuildNanite(UE::Landscape::EBuildFlags::WriteFinalLog, MakeArrayView(&Proxy, 1));
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape Nanite: %s"), Landscape->IsNaniteMeshUpToDate() ? TEXT("built") : TEXT("NOT built"));
}

void UWorldForgeEditorLib::SetLandscapeMaterial(ALandscape* Landscape, UMaterialInterface* Material)
{
	if (!Landscape || !Material)
	{
		return;
	}
	Landscape->Modify();
	Landscape->LandscapeMaterial = Material;
	if (FProperty* Prop = FindFProperty<FProperty>(ALandscapeProxy::StaticClass(), TEXT("LandscapeMaterial")))
	{
		FPropertyChangedEvent Changed(Prop);
		Landscape->PostEditChangeProperty(Changed);
	}
}

FString UWorldForgeEditorLib::SliceLayerStats(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
	int32 Components, int32 QuadsPerComponent, float QuadSizeM, int32 NumLayers, float RoadWidthM)
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
	const int32 N = Components * QuadsPerComponent + 1;
	const bool bPaved = NumLayers == 6 && RoadWidthM > 0.0f;
	GD.Edits = GEditsOverride;
	GD.Setup(MapOf(S), FVector2f(CenterM.X, CenterM.Y), 0.5f * (N - 1) * QuadSizeM, bPaved ? 0.5f * RoadWidthM : 3.0f, bPaved);
	GD.BuildWeightCache(1.0f);
	TArray<uint16> Heights;
	TArray<TArray<uint8>> Weights;
	SliceHeightsAndWeights(N, QuadSizeM, NumLayers, Heights, Weights);
	TArray<TArray<uint8>> Mx;
	ComponentLayerMax(N, Components, QuadsPerComponent, Weights, Mx);
	const int32 NC = Components * Components;
	FString Out = FString::Printf(TEXT("%d componentes, %d camadas"), NC, NumLayers);
	// por camada: em quantos componentes entra (peso > 0) e quantos desses têm o máximo ≤ 1, 3, 8, 16 (de 255)
	for (int32 K = 0; K < NumLayers; ++K)
	{
		int32 In = 0, T1 = 0, T3 = 0, T8 = 0, T16 = 0;
		for (int32 C = 0; C < NC; ++C)
		{
			const uint8 V = Mx[C][K];
			In += V > 0;
			T1 += V > 0 && V <= 1;
			T3 += V > 0 && V <= 3;
			T8 += V > 0 && V <= 8;
			T16 += V > 0 && V <= 16;
		}
		Out += FString::Printf(TEXT("\n  camada %d: em %d; max<=1 em %d, <=3 em %d, <=8 em %d, <=16 em %d"), K, In, T1, T3, T8, T16);
	}
	// camadas por componente com cada limite
	for (int32 T : { 0, 1, 3, 8, 16 })
	{
		TArray<int32> Hist;
		Hist.Init(0, NumLayers + 1);
		int32 Total = 0;
		for (int32 C = 0; C < NC; ++C)
		{
			int32 Cnt = 0;
			for (int32 K = 0; K < NumLayers; ++K) Cnt += Mx[C][K] > T;
			Hist[Cnt]++;
			Total += Cnt;
		}
		Out += FString::Printf(TEXT("\n  limite %2d: media %.2f camadas por componente;"), T, float(Total) / NC);
		for (int32 I = 1; I <= NumLayers; ++I) Out += FString::Printf(TEXT(" %d->%d"), I, Hist[I]);
	}
	return Out;
}

FString UWorldForgeEditorLib::MaterialHlsl(UMaterialInterface* Material)
{
	if (!Material)
	{
		return TEXT("sem material");
	}
	// (no commandlet sem placa o material não tem recurso: um recurso novo só pra tradução, sem compilar shader)
	UMaterial* Base = Material->GetMaterial();
	TUniquePtr<FMaterialResource> R = MakeUnique<FMaterialResource>();
	R->SetMaterial(Base, Cast<UMaterialInstance>(Material), SP_PCD3D_SM6, EMaterialQualityLevel::High);
	FString Out;
	if (!R->GetMaterialExpressionSource(Out))
	{
		return TEXT("sem tradução");
	}
	return Out;
}

namespace
{
	// o _hash2 do look_ground (o sorteio por placa do Voronoi)
	float SlabHash(FVector2f C, float K)
	{
		const float V = FMath::Sin(C.X * 127.1f + C.Y * 311.7f + K * 74.7f) * 43758.5453f;
		return V - FMath::FloorToFloat(V);
	}

	/** O Voronoi das placas da borda (look_ground.voronoi): F1, F2, a célula e o centro dela. */
	void SlabVoronoi(FVector2f P, float Size, float& F1, float& F2, FVector2f& Cell, FVector2f& O)
	{
		const FVector2f G = P / Size;
		const FVector2f I(FMath::FloorToFloat(G.X), FMath::FloorToFloat(G.Y));
		F1 = F2 = 9e9f;
		for (int32 Dj = -1; Dj <= 1; ++Dj)
		{
			for (int32 Di = -1; Di <= 1; ++Di)
			{
				const FVector2f C = I + FVector2f(float(Di), float(Dj));
				const FVector2f Q((C.X + SlabHash(C, 11.0f)) * Size, (C.Y + SlabHash(C, 12.0f)) * Size);
				const float D = FVector2f::Distance(P, Q);
				if (D < F1)
				{
					F2 = F1;
					F1 = D;
					Cell = C;
					O = Q;
				}
				else
				{
					F2 = FMath::Min(F2, D);
				}
			}
		}
	}
}

FVector UWorldForgeEditorLib::SliceSlabAt(FVector2D PointCm)
{
	if (!GD.GMap.IsValid())
	{
		return FVector(-1.0, 0.0, 0.0);
	}
	const FVector2f P = GD.ToMap(PointCm);
	const FWorldDetail::FSlabSample S = GD.SlabSample(*GD.GMap, P, GD.RoadAt(P));
	return S.bPaved ? FVector(S.Wear, S.In, S.Drift) : FVector(-1.0, 0.0, 0.0);
}

FString UWorldForgeEditorLib::BuildRoadSlabs(UMaterialInterface* Material, const FString& AssetDir, const FString& MapName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !GD.GMap.IsValid() || !GD.GSlab)
	{
		return TEXT("sem laje (ROAD_SLAB=1 e o deserto com asfalto)");
	}
	const FWorldMap& M = *GD.GMap;
	// a grade: o passo fino de 6 cm (o kit do Look: 4–6 cm) só na faixa da beira onde há placa quebrada; 12 cm na beira
	// limpa (só o contorno); o miolo em células de 48 cm (o Look, 14/10: "no miolo 25–50 cm basta, a coroa de 2,5 cm não
	// aparece"). Ladrilhos de 64 células grossas (30,72 m)
	constexpr float Fine = 0.06f;
	constexpr int32 Sub = 8;                      // passos finos por célula grossa
	constexpr float Coarse = Fine * Sub;          // 0,48 m
	constexpr int32 TileCells = 64;
	constexpr float TileM = Coarse * TileCells;   // 30,72 m

	// os ladrilhos que as estradas de asfalto tocam
	TSet<FIntPoint> Tiles;
	for (const FWorldDetail::FRoad& R : GD.GRoads)
	{
		if (!R.bPaved || R.Style == 1)
		{
			continue;
		}
		const float Reach = R.MaxHalf() + 3.0f;
		for (const FVector2f& Pt : R.P)
		{
			const int32 X0 = FMath::FloorToInt((Pt.X - Reach) / TileM), X1 = FMath::FloorToInt((Pt.X + Reach) / TileM);
			const int32 Y0 = FMath::FloorToInt((Pt.Y - Reach) / TileM), Y1 = FMath::FloorToInt((Pt.Y + Reach) / TileM);
			for (int32 Y = Y0; Y <= Y1; ++Y) for (int32 X = X0; X <= X1; ++X) Tiles.Add(FIntPoint(X, Y));
		}
	}

	// o ponto da laje: a altura (m) e se é laje. Fora dela (borda, placa que falta, areia por cima) o vértice desce 6 cm
	// por baixo do chão e o degrau sai como a saia da malha (a maquete: z = chão − 0,06)
	struct FSlabVert { float Z = 0.0f; bool bOn = false; bool bNear = false; float Wear = 0.0f; float Sand = 0.0f; };
	auto Eval = [&M](FVector2f P) -> FSlabVert
	{
		FSlabVert V;
		const FWorldDetail::FRoadHit Hit = GD.RoadAt(P);
		const FWorldDetail::FSlabSample S = GD.SlabSample(M, P, Hit);
		const float Ground = GD.SliceHeight(M, P);
		V.Z = Ground - 0.06f;
		V.Wear = S.Wear;
		if (!S.bPaved || S.In > 1.5f)
		{
			return V;
		}
		V.bNear = true;
		if (S.A < 0.5f)
		{
			return V;
		}
		const float Dd = FMath::Max(-S.In, 0.0f);
		float Top = Hit.Z + FWorldDetail::SlabThick + FWorldDetail::SlabCrown * FMath::SmoothStep(0.0f, 4.0f, Dd);
		// o desgaste (look_ground.EDGE_TYPES): inteira = sem quebra; gasta = quebra em trechos de 5–15 m; destruída = a
		// borda toda. Os números de cada tipo misturados pelo desgaste
		const float TW = FMath::SmoothStep(0.30f, 0.40f, S.Wear), TR = FMath::SmoothStep(0.62f, 0.72f, S.Wear);
		const float Patches = FMath::SmoothStep(-0.35f, -0.15f, WorldNoise::Fbm(P.X / 9.0f, P.Y / 9.0f, 951u, 3));
		const float Break = TW * FMath::Lerp(Patches, 1.0f, TR);
		const float Broken = FMath::Lerp(0.9f, 1.4f, TR), Miss = FMath::Lerp(0.2f, 0.38f, TR), DropP = FMath::Lerp(0.4f, 0.55f, TR);
		const float Zone = (1.0f - FMath::SmoothStep(0.2f, Broken, Dd)) * Break;
		if (Zone > 0.0f)
		{
			float F1, F2;
			FVector2f Cell, O;
			SlabVoronoi(P, 0.5f, F1, F2, Cell, O);
			const float H1 = SlabHash(Cell, 1.0f), H2 = SlabHash(Cell, 2.0f), H3 = SlabHash(Cell, 3.0f), H4 = SlabHash(Cell, 4.0f);
			if (H1 < Miss * Zone && Dd < 0.9f)
			{
				return V;   // a placa que falta: aparece a areia de baixo
			}
			if (H2 < DropP * Zone)
			{
				// afundou e inclinou (a fresta de 1,5–5 cm entre as placas fica fora da geometria: é menor que o pixel
				// do jogo e tremia com o TSR, o Look 14/10)
				const float Tilt = ((P.X - O.X) * FMath::Cos(H4 * 6.283f) + (P.Y - O.Y) * FMath::Sin(H4 * 6.283f)) * 0.08f;
				Top -= 0.02f + 0.06f * H3 + Tilt * Zone;
			}
		}
		// a areia por cima (16/10, a 1ª versão escondia a laje debaixo do chão onde o monte passava do topo, e o corte
		// seguia os triângulos de 0,5 m do Landscape: a beira da areia saía em dentes): como na maquete do Look
		// (look_ground.road_slab, sand_up), a laje sobe com a areia e o material pinta a areia (a cor do vértice R: 0 o
		// asfalto, 1 a areia; a película começa 10 cm abaixo do topo)
		// (16/10, a 1ª prova no jogo: com a altura do chão aqui, a laje inteira saía cor de areia, porque debaixo do miolo o
		// chão sobe de propósito até 1,5 cm do topo, pro tanque. A areia que cobre é só o monte encostado)
		const float SandTop = Hit.Z + S.Drift - FWorldDetail::SlabSandDrop;
		V.Sand = FMath::Clamp((SandTop - (Top - 0.10f)) / 0.12f, 0.0f, 1.0f);
		V.Z = FMath::Max(Top, SandTop + 0.015f);   // (1,5 cm: o Landscape interpolado não fura a laje no côncavo)
		V.bOn = true;
		return V;
	};

	int32 NumMeshes = 0, TotalTris = 0, TotalVerts = 0;
	TArray<FIntPoint> TileList = Tiles.Array();
	TileList.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
	for (const FIntPoint& T : TileList)
	{
		const FVector2f Origin(T.X * TileM, T.Y * TileM);
		// os vértices finos por índice global (o mesmo ponto nos dois ladrilhos vizinhos sai igual)
		TMap<FIntPoint, FSlabVert> Cache;
		auto At = [&](FIntPoint G) -> const FSlabVert&
		{
			if (const FSlabVert* F = Cache.Find(G))
			{
				return *F;
			}
			return Cache.Add(G, Eval(FVector2f(G.X * Fine, G.Y * Fine)));
		};
		TArray<FVector3f> Pos;
		TArray<float> SandV;
		TMap<FIntPoint, int32> Index;
		TArray<int32> Tri;
		auto Vert = [&](FIntPoint G) -> int32
		{
			if (const int32* F = Index.Find(G))
			{
				return *F;
			}
			const FSlabVert& V = At(G);
			Pos.Add(FVector3f((G.X * Fine - Origin.X) * 100.0f, (G.Y * Fine - Origin.Y) * 100.0f, V.Z * 100.0f));
			SandV.Add(V.Sand);
			return Index.Add(G, Pos.Num() - 1);
		};
		// uma célula (canto G00, lado em passos finos N): só se algum canto é laje
		auto Cell = [&](FIntPoint G00, int32 N)
		{
			const FIntPoint G10 = G00 + FIntPoint(N, 0), G01 = G00 + FIntPoint(0, N), G11 = G00 + FIntPoint(N, N);
			if (!(At(G00).bOn || At(G10).bOn || At(G01).bOn || At(G11).bOn))
			{
				return;
			}
			const int32 A = Vert(G11), B = Vert(G10), C = Vert(G00), D = Vert(G01);
			// a face pra cima na ordem da Unreal (+x+y, +x−y, −x−y, −x+y, a mesma da água)
			Tri.Append({ A, B, C, A, C, D });
		};
		const FIntPoint Base(FMath::RoundToInt(Origin.X / Fine), FMath::RoundToInt(Origin.Y / Fine));
		for (int32 CY = 0; CY < TileCells; ++CY)
		{
			for (int32 CX = 0; CX < TileCells; ++CX)
			{
				const FIntPoint G00 = Base + FIntPoint(CX * Sub, CY * Sub);
				const FSlabVert V00 = At(G00);   // (cópias: o At cresce o mapa e invalida referência)
				const FSlabVert V10 = At(G00 + FIntPoint(Sub, 0));
				const FSlabVert V01 = At(G00 + FIntPoint(0, Sub));
				const FSlabVert V11 = At(G00 + FIntPoint(Sub, Sub));
				if (!(V00.bNear || V10.bNear || V01.bNear || V11.bNear))
				{
					continue;
				}
				// o miolo: os quatro cantos na laje e longe da beira e das placas quebradas (uma célula grossa)
				const FVector2f C0 = FVector2f(G00.X * Fine, G00.Y * Fine) + FVector2f(0.5f * Coarse, 0.5f * Coarse);
				const FWorldDetail::FRoadHit Hc = GD.RoadAt(C0);
				const FWorldDetail::FSlabSample Sc = GD.SlabSample(M, C0, Hc);
				const float Band = FMath::SmoothStep(0.30f, 0.40f, Sc.Wear) > 0.0f ? 1.9f : 0.9f;   // a beira que pode quebrar
				if (V00.bOn && V10.bOn && V01.bOn && V11.bOn && Sc.A > 0.95f && -Sc.In > Band + Coarse && Sc.Drift < 0.05f
					&& V00.Sand + V10.Sand + V01.Sand + V11.Sand == 0.0f)
				{
					Cell(G00, Sub);
					continue;
				}
				// a beira: 6 cm onde pode quebrar, 12 cm na beira limpa
				const int32 Step = (FMath::Max(FMath::Max(V00.Wear, V10.Wear), FMath::Max(V01.Wear, V11.Wear)) > 0.3f) ? 1 : 2;
				for (int32 J = 0; J < Sub; J += Step)
				{
					for (int32 I = 0; I < Sub; I += Step)
					{
						Cell(G00 + FIntPoint(I, J), Step);
					}
				}
			}
		}
		if (Tri.Num() == 0)
		{
			continue;
		}
		// as normais pela malha (a média das faces, por área): a saia da beira sai de lado e o material escurece a quebra
		TArray<FVector3f> Nrm;
		Nrm.Init(FVector3f::ZeroVector, Pos.Num());
		for (int32 K = 0; K < Tri.Num(); K += 3)
		{
			const FVector3f& PA = Pos[Tri[K]];
			const FVector3f N = FVector3f::CrossProduct(Pos[Tri[K + 2]] - PA, Pos[Tri[K + 1]] - PA);
			Nrm[Tri[K]] += N;
			Nrm[Tri[K + 1]] += N;
			Nrm[Tri[K + 2]] += N;
		}
		FMeshDescription Desc;
		FStaticMeshAttributes Attr(Desc);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSgn = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUv = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		const FPolygonGroupID Group = Desc.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Asphalt");
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(Pos.Num());
		for (int32 K = 0; K < Pos.Num(); ++K)
		{
			const FVertexID V = Desc.CreateVertex();
			VPos[V] = Pos[K];
			const FVertexInstanceID VI = Desc.CreateVertexInstance(V);
			FVector3f N = Nrm[K].GetSafeNormal();
			if (N.Z < 0.0f) N = -N;   // (a ordem das faces: a laje olha pra cima)
			VNrm[VI] = N.IsNearlyZero() ? FVector3f(0, 0, 1) : N;
			// a tangente no +X do mundo (o UV é o XY do mundo em metros, como o Landscape): a normal das camadas do chão
			// (espaço tangente do terreno) cai igual na laje
			FVector3f Tx = FVector3f(1, 0, 0) - VNrm[VI] * VNrm[VI].X;
			VTan[VI] = Tx.IsNearlyZero() ? FVector3f(0, 1, 0) : Tx.GetSafeNormal();
			VSgn[VI] = 1.0f;
			VUv[VI] = FVector2f(Origin.X + Pos[K].X / 100.0f, Origin.Y + Pos[K].Y / 100.0f);
			VCol[VI] = FVector4f(SandV[K], 0.0f, 0.0f, 1.0f);   // R: a areia por cima (o material da laje mistura a areia do chão)
			Inst.Add(VI);
		}
		for (int32 K = 0; K < Tri.Num(); K += 3)
		{
			const FVertexInstanceID Q[3] = { Inst[Tri[K]], Inst[Tri[K + 1]], Inst[Tri[K + 2]] };
			Desc.CreatePolygon(Group, Q);
		}
		const FString Name = FString::Printf(TEXT("SM_RoadSlab_%s_%d"), *MapName, NumMeshes);
		const FString AssetPath = AssetDir / Name;
		UStaticMesh* Mesh = LoadOrCreateAsset<UStaticMesh>(AssetPath);
		Mesh->Modify();
		Mesh->SetNumSourceModels(1);
		FStaticMeshSourceModel& Model = Mesh->GetSourceModel(0);
		Model.BuildSettings.bRecomputeNormals = false;
		Model.BuildSettings.bRecomputeTangents = false;
		Model.BuildSettings.bGenerateLightmapUVs = false;
		TArray<FStaticMaterial> Materials;
		Materials.Emplace(Material, FName(TEXT("Asphalt")), FName(TEXT("Asphalt")));
		Mesh->SetStaticMaterials(Materials);
		Mesh->GetSectionInfoMap().Clear();
		Mesh->GetSectionInfoMap().Set(0, 0, FMeshSectionInfo(0));
		Mesh->CreateMeshDescription(0, MoveTemp(Desc));
		Mesh->CommitMeshDescription(0);
		FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
		Nanite.bEnabled = true;
		Mesh->SetNaniteSettings(Nanite);
		Mesh->Build(true);
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();

		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(Origin.X * 100.0, Origin.Y * 100.0, 0.0)));
		if (Actor)
		{
			Actor->SetActorLabel(FString::Printf(TEXT("RoadSlab_%d"), NumMeshes));
			Actor->SetFolderPath(TEXT("RoadSlab"));
			UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
			Comp->SetMobility(EComponentMobility::Static);
			Comp->SetStaticMesh(Mesh);
			Comp->SetMaterial(0, Material);
			Comp->SetCastShadow(true);
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// (14/10, a 1ª foto: o asfalto da laje saiu liso, sem as rachaduras. O material lê as texturas pela posição
			// no mundo e o streaming estima o mip pelo UV da malha; na laje ele pedia mips baixos. As mesmas texturas do
			// chão, inteiras)
			Comp->bForceMipStreaming = true;
			// o jogo acha as lajes pela etiqueta (os parâmetros da memória do chão vão também nelas)
			Actor->Tags.Add(FName(TEXT("WorldForgeRoadSlab")));
		}
		++NumMeshes;
		TotalTris += Tri.Num() / 3;
		TotalVerts += Pos.Num();
	}
	const FString Out = FString::Printf(TEXT("%d ladrilhos, %d vértices, %d triângulos"), NumMeshes, TotalVerts, TotalTris);
	UE_LOG(LogTemp, Display, TEXT("FORGE road slab %s"), *Out);
	return Out;
}

FString UWorldForgeEditorLib::LandscapeLayerReport(ALandscape* Landscape)
{
	ULandscapeInfo* Info = Landscape ? Landscape->GetLandscapeInfo() : nullptr;
	if (!Info)
	{
		return TEXT("sem Landscape");
	}
	// por camada: componentes onde está alocada, e quantos desses têm o peso máximo ≤ 0, ≤ 3 e ≤ 8 (de 255)
	struct FStat { int32 Alloc = 0, Max0 = 0, Max3 = 0, Max8 = 0; };
	TMap<FName, FStat> PerLayer;
	TMap<int32, int32> CountHist, CountHist3;   // componentes por nº de camadas (todas / só as com peso > 3)
	int32 NumComp = 0;
	// o FColor da fonte é BGRA: o canal R do alocador mora no byte 2
	static const int32 ChannelOffset[4] = { 2, 1, 0, 3 };
	for (const TPair<FIntPoint, ULandscapeComponent*>& It : Info->XYtoComponentMap)
	{
		ULandscapeComponent* C = It.Value;
		if (!C)
		{
			continue;
		}
		++NumComp;
		const TArray<FWeightmapLayerAllocationInfo>& Alloc = C->GetWeightmapLayerAllocations();
		const TArray<UTexture2D*>& Tex = C->GetWeightmapTextures();
		int32 Real = 0;
		for (const FWeightmapLayerAllocationInfo& A : Alloc)
		{
			const FName Name = A.LayerInfo ? A.LayerInfo->GetLayerName() : FName(TEXT("?"));
			FStat& S = PerLayer.FindOrAdd(Name);
			++S.Alloc;
			int32 MaxW = 255;
			if (Tex.IsValidIndex(A.WeightmapTextureIndex) && Tex[A.WeightmapTextureIndex] && A.WeightmapTextureChannel < 4)
			{
				FTextureSource& Src = Tex[A.WeightmapTextureIndex]->Source;
				const int64 Pixels = int64(Src.GetSizeX()) * Src.GetSizeY();
				const uint8* Data = Src.LockMipReadOnly(0);
				if (Data && Pixels > 0)
				{
					MaxW = 0;
					const uint8* P = Data + ChannelOffset[A.WeightmapTextureChannel];
					for (int64 I = 0; I < Pixels; ++I)
					{
						MaxW = FMath::Max<int32>(MaxW, P[I * 4]);
					}
				}
				Src.UnlockMip(0);
			}
			S.Max0 += MaxW == 0;
			S.Max3 += MaxW <= 3;
			S.Max8 += MaxW <= 8;
			Real += MaxW > 3;
		}
		CountHist.FindOrAdd(Alloc.Num())++;
		CountHist3.FindOrAdd(Real)++;
	}
	FString Out = FString::Printf(TEXT("%d componentes"), NumComp);
	for (const TPair<FName, FStat>& It : PerLayer)
	{
		Out += FString::Printf(TEXT("\n  %s: alocada em %d; peso max 0 em %d, <=3 em %d, <=8 em %d"), *It.Key.ToString(),
			It.Value.Alloc, It.Value.Max0, It.Value.Max3, It.Value.Max8);
	}
	CountHist.KeySort(TLess<int32>());
	CountHist3.KeySort(TLess<int32>());
	Out += TEXT("\n  camadas por componente (alocadas):");
	for (const TPair<int32, int32>& It : CountHist) Out += FString::Printf(TEXT(" %d->%d"), It.Key, It.Value);
	Out += TEXT("\n  camadas por componente (peso > 3):");
	for (const TPair<int32, int32>& It : CountHist3) Out += FString::Printf(TEXT(" %d->%d"), It.Key, It.Value);
	return Out;
}

UTexture2D* UWorldForgeEditorLib::MakeWaterInfoTexture(const FString& AssetPath, float SizeM)
{
	// sem mapa gerado (ou sem água): 16×16 de "terra longe d'água". Serve de
	// lugar pra a instância de material do chão existir antes do Landscape
	// (trocar o material do Landscape depois refaz as camadas, e sem placa de
	// vídeo elas saíam vazias: o chão sumia, 07/10)
	// (sem água também: o A da textura é o tom do chão — salina, leitos secos)
	if (!GD.GMap.IsValid())
	{
		const FString Name = FPackageName::GetShortName(AssetPath);
		UTexture2D* Tex = LoadOrCreateAsset<UTexture2D>(AssetPath);
		TArray<FColor> Dry;
		Dry.Init(FColor(0, 0, 0, 128), 16 * 16);   // A = 128: tom neutro
		Tex->Modify();
		Tex->Source.Init(16, 16, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Dry.GetData()));
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_VectorDisplacementmap;
		Tex->MipGenSettings = TMGS_NoMipmaps;
		Tex->PostEditChange();
		Tex->MarkPackageDirty();
		return Tex;
	}
	TArray<FColor> Pixels;
	int32 R = 0;
	GD.WaterInfoPixels(SizeM, Pixels, R);
	const FString Name = FPackageName::GetShortName(AssetPath);
	UTexture2D* Tex = LoadOrCreateAsset<UTexture2D>(AssetPath);
	Tex->Modify();
	Tex->Source.Init(R, R, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Tex->SRGB = false;
	Tex->CompressionSettings = TC_VectorDisplacementmap;   // sem compressão: a distância tem de ser exata
	Tex->MipGenSettings = TMGS_NoMipmaps;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;
	Tex->Filter = TF_Bilinear;
	Tex->PostEditChange();
	Tex->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE water info texture %s: %dx%d, %.0f m"), *Name, R, R, SizeM);
	return Tex;
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
	const TSharedPtr<FWorldMap> Map = MapOf(S);
	const FWorldMap& M = *Map;
	// os pesos dependem do bioma e da estrada: zera o que sobrou do recorte
	// anterior (montando várias fatias seguidas, o deserto pegava a estrada da neve)
	GD.GForest = S.Biome == EWorldBiome::Temperate;
	GD.GWinter = S.Biome == EWorldBiome::Winter;
	GD.GFrozen = GD.GWinter && M.WaterLevel > -100.0f;
	GD.GRoads.Reset();
	GD.GRoadGrid.Reset();
	GD.BuildCanopy(M);
	GD.BuildRockRing(M);
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
					GD.DesertWeights(M, P, W);
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
			// a estrada principal (asfalto no deserto e na neve) atravessando o
			// miolo do recorte: as fatias de exemplo são "com estrada"
			// (pelo meio: a distância do centro do recorte até a principal, fora
			// das bases, onde ela acaba)
			float RoadNear = 1e9f;
			for (int32 T = 0; T < M.Tracks.Num(); ++T)
			{
				if (!M.TrackKinds.IsValidIndex(T) || M.TrackKinds[T] != EWorldTrack::Main) continue;
				for (const FVector2f& Q : M.Tracks[T])
				{
					bool bBase = false;
					for (const FWorldStart& St : M.Starts) bBase |= FVector2f::Distance(Q, St.Pos) < M.BaseRadius + 20.0f;
					if (!bBase) RoadNear = FMath::Min(RoadNear, FVector2f::Distance(Q, FVector2f(CX, CY)));
				}
			}
			const float Road = 1.0f - FMath::SmoothStep(15.0f, 70.0f, RoadNear);
			const float Score = Entropy + (Dirt > 0.04f ? 0.3f : 0.0f) + (Rock > 0.05f ? 0.3f : 0.0f) + (bLake ? 0.6f : 0.0f)
				+ 1.6f * Road - 2.0f * FMath::Max(0.0f, Wet - 0.1f);
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
		// a sombra guardada mesmo com o vento (14/10, a otimização: o WPO da planta invalidava as páginas da sombra
		// virtual todo quadro; Rigid = só o transform invalida, como o Fortnite faz nos prédios). Pedra e paredão não
		// mexem, então não muda nada pra eles; a palmeira e a moita balançam e a sombra fica parada (a 77–157 m não se vê).
		// Só com SHADOW_RIGID=1 até o A/B na luz do jogo (a v2 congelada não muda)
		if (FPlatformMisc::GetEnvironmentVariable(TEXT("SHADOW_RIGID")) == TEXT("1"))
		{
			C->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Rigid;
		}
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
		// cota d'água do mapa (cm) em cada instância: o material dos modelos tira
		// a neve do que fica abaixo dela (a pedra submersa, prop_material)
		const bool bWater = GD.GMap.IsValid() && GD.GMap->WaterLevel > -100.0f;
		if (bWater)
		{
			C->SetNumCustomDataFloats(1);
		}
		C->SetupAttachment(Actor->GetRootComponent());
		Actor->AddInstanceComponent(C);
		C->RegisterComponent();
		C->AddInstances(Cell.Value, false, true, false);
		if (bWater)
		{
			const float WaterCm = GD.GMap->WaterLevel * 100.0f;
			for (int32 I = 0; I < C->GetInstanceCount(); ++I)
			{
				C->SetCustomDataValue(I, 0, WaterCm, false);
			}
			C->MarkRenderStateDirty();
		}
	}
	return true;
}

bool UWorldForgeEditorLib::EnableNanite(UStaticMesh* Mesh, bool bFoliage)
{
	if (!Mesh)
	{
		return false;
	}
	Mesh->Modify();
	FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
	Settings.bEnabled = true;
	if (bFoliage)
	{
		Settings.ShapePreservation = ENaniteShapePreservation::PreserveArea;
	}
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

TArray<FVector> UWorldForgeEditorLib::SliceRoad(float StepM, bool bMarksOnly)
{
	return GD.GMap.IsValid() ? RoadPoints(StepM, true, bMarksOnly) : TArray<FVector>();
}

float UWorldForgeEditorLib::SliceRoadDistance(FVector2D PointCm)
{
	const FWorldDetail::FRoadHit Hit = GD.RoadAt(GD.ToMap(PointCm), INDEX_NONE, true);
	return Hit.Road != INDEX_NONE ? Hit.D * 100.0f : 1e9f;
}

UTexture2D* UWorldForgeEditorLib::MakeRoadDirTexture(const FString& AssetPath, float SizeM)
{
	TArray<FColor> Pixels;
	int32 R = 16;
	if (GD.GMap.IsValid())
	{
		GD.RoadDirPixels(SizeM, Pixels, R);
	}
	else
	{
		// θ = 0 (o ângulo dobrado e girado: cos(−90°) = 0, sen(−90°) = −1)
		Pixels.Init(FColor(128, 0, 0, 255), 16 * 16);
	}
	const FString Name = FPackageName::GetShortName(AssetPath);
	UTexture2D* Tex = LoadOrCreateAsset<UTexture2D>(AssetPath);
	Tex->Modify();
	Tex->Source.Init(R, R, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Tex->SRGB = false;
	Tex->CompressionSettings = TC_VectorDisplacementmap;
	Tex->MipGenSettings = TMGS_NoMipmaps;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;
	Tex->Filter = TF_Bilinear;
	Tex->PostEditChange();
	Tex->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE road dir texture %s: %dx%d, %.0f m"), *Name, R, R, SizeM);
	return Tex;
}

FString UWorldForgeEditorLib::SliceDuneStats()
{
	if (!GD.GMap.IsValid() || !GD.GDunes)
	{
		return TEXT("sem dunas (DUNE_POCKETS=1)");
	}
	const FWorldMap& M = *GD.GMap;
	// a inclinação nas dunas a cada 0,5 m (a malha do Landscape), pela altura final do chão (SliceHeight)
	int32 N = 0, Over34 = 0, Over31 = 0, Face = 0, Wind = 0, Max5 = 0, SteepCut = 0, SteepJoin = 0, SteepOwn = 0;
	float MaxDeg = 0.0f;
	TArray<int32> Hist;
	Hist.Init(0, 46);
	const float Half = M.Half();
	// (16/10 à tarde: as dunas agora encostam; por duna contava o ponto duas vezes) cada célula de 32 m da grade das dunas
	// uma vez só, a 0,5 m
	for (const TPair<FIntPoint, TArray<int32>>& Cell : GD.GDuneGrid)
	{
		for (float Y = Cell.Key.Y * 32.0f + 0.25f; Y < (Cell.Key.Y + 1) * 32.0f; Y += 0.5f)
		{
			for (float X = Cell.Key.X * 32.0f + 0.25f; X < (Cell.Key.X + 1) * 32.0f; X += 0.5f)
			{
				const FVector2f P(X, Y);
				if (FMath::Abs(X) > Half || FMath::Abs(Y) > Half || GD.DuneAt(M, P) < 0.05f) continue;
				const float Gx = (GD.SliceHeight(M, P + FVector2f(0.5f, 0.0f)) - GD.SliceHeight(M, P - FVector2f(0.5f, 0.0f))) / 1.0f;
				const float Gy = (GD.SliceHeight(M, P + FVector2f(0.0f, 0.5f)) - GD.SliceHeight(M, P - FVector2f(0.0f, 0.5f))) / 1.0f;
				const float Deg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Gx * Gx + Gy * Gy)));
				++N;
				MaxDeg = FMath::Max(MaxDeg, Deg);
				Over34 += Deg > 34.0f;
				if (Deg > 34.0f)
				{
					// de onde vem o íngreme: o corte da borda (DuneAllowed < 1), duas dunas juntas, ou a duna sozinha
					int32 NHit = 0;
					GD.DuneRaw(M, P, &NHit);
					const float Al = GD.DuneAllowed(M, P);
					(Al < 0.98f ? SteepCut : (NHit >= 2 ? SteepJoin : SteepOwn))++;
					if (Al >= 0.98f && NHit < 2 && SteepOwn <= 12)
					{
						UE_LOG(LogTemp, Display, TEXT("FORGE steep %.1f graus em %.1f %.1f: z %.2f"), Deg, P.X, P.Y, GD.DuneAt(M, P));
					}
				}
				Over31 += Deg > 31.5f;
				Face += Deg > 25.0f;
				Wind += Deg > 5.0f && Deg <= 16.0f;
				Max5 += Deg <= 5.0f;
				Hist[FMath::Clamp(FMath::FloorToInt(Deg), 0, 45)]++;
			}
		}
	}
	FString Out = FString::Printf(TEXT("%d dunas, %d pontos (%.0f m2): max %.1f graus; >34: %.2f%%, >31,5: %.2f%%, face (>25): %.1f%%, barlavento (5-16): %.1f%%, plano (<=5): %.1f%%; hist"),
		GD.GDuneInst.Num(), N, 0.25f * N, MaxDeg, 100.0f * Over34 / FMath::Max(N, 1), 100.0f * Over31 / FMath::Max(N, 1), 100.0f * Face / FMath::Max(N, 1),
		100.0f * Wind / FMath::Max(N, 1), 100.0f * Max5 / FMath::Max(N, 1));
	for (int32 K = 0; K < 46; K += 3) Out += FString::Printf(TEXT(" %d:%d"), K, Hist[K] + (K + 1 < 46 ? Hist[K + 1] : 0) + (K + 2 < 46 ? Hist[K + 2] : 0));
	Out += FString::Printf(TEXT("; >34 por causa: borda %d, juntas %d, sozinha %d"), SteepCut, SteepJoin, SteepOwn);
	return Out;
}

UTexture2D* UWorldForgeEditorLib::MakeRoadFrameTexture(const FString& AssetPath, float SizeM)
{
	TArray<FFloat16Color> Pixels;
	int32 R = 16;
	if (GD.GMap.IsValid())
	{
		GD.RoadFramePixels(SizeM, Pixels, R);
	}
	else
	{
		Pixels.Init(FFloat16Color(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)), 16 * 16);
	}
	UTexture2D* Tex = LoadOrCreateAsset<UTexture2D>(AssetPath);
	Tex->Modify();
	Tex->Source.Init(R, R, 1, 1, TSF_RGBA16F, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Tex->SRGB = false;
	Tex->CompressionSettings = TC_HDR;
	Tex->MipGenSettings = TMGS_NoMipmaps;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;
	Tex->Filter = TF_Bilinear;
	Tex->PostEditChange();
	Tex->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE road frame texture %s: %dx%d RGBA16F, %.0f m"), *FPackageName::GetShortName(AssetPath), R, R, SizeM);
	return Tex;
}

bool UWorldForgeEditorLib::ConnectMaterialDisplacement(UMaterialExpression* From, const FString& FromOutputName)
{
	return From && UMaterialEditingLibrary::ConnectMaterialProperty(From, FromOutputName, MP_Displacement);
}

FString UWorldForgeEditorLib::MakeSurfacePng(const FString& PngPath, float CellM)
{
	if (!GD.GMap.IsValid() || CellM <= 0.0f)
	{
		return FString();
	}
	const FWorldMap& M = *GD.GMap;
	const float Half = M.Half();
	const int32 Res = FMath::Clamp(FMath::CeilToInt(2.0f * Half / CellM), 16, 4096);
	if (GD.GWaterRes < 2)
	{
		GD.BuildWaterField(1.0f);   // a margem molhada (o A) vem do campo d'água
	}
	TArray<FColor> Px;
	Px.SetNumUninitialized(Res * Res);
	ParallelFor(Res, [&](int32 J)
	{
		for (int32 I = 0; I < Res; ++I)
		{
			const FVector2f P(-Half + (I + 0.5f) * CellM, -Half + (J + 0.5f) * CellM);
			float W[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
			GD.ScatterWeights(M, P, W);
			auto B8 = [](float V) { return uint8(FMath::Clamp(V, 0.0f, 1.0f) * 255.0f + 0.5f); };
			Px[J * Res + I] = FColor(B8(W[5]), B8(W[2]), B8(W[3]), B8(GD.WetGroundAt(M, P)));
		}
	});
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Res, Res, TArrayView64<const FColor>(Px.GetData(), Px.Num()), Png);
	if (Png.Num() == 0 || !FFileHelper::SaveArrayToFile(Png, *PngPath))
	{
		return FString();
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE surface %s: %dx%d, %.1f m por pixel"), *PngPath, Res, Res, CellM);
	return FString::Printf(TEXT("{\n \"min_m\": [%.1f, %.1f],\n \"size_m\": %.1f,\n \"cell_m\": %.2f,\n \"res\": %d,\n \"row0\": \"min_y\",\n \"channels\": {\"r\": \"asphalt\", \"g\": \"dirt\", \"b\": \"rock\", \"a\": \"wet\"},\n \"about\": \"WorldForge layer weights 0..255 per pixel (Landscape painting weights: asphalt = paved road incl. broken edge and holes; dirt = dirt roads, shoulders, trodden village ground, damp oasis earth and salt-flat rims; rock = cliffs and banks; wet = 255 under water, the wet band up to ~1.2 m above the water within 8 m of the shore and the damp oasis earth, 0 dry). Pixel (i, j) center = min_m + ((i + 0.5), (j + 0.5)) * cell_m, row 0 at min y.\"\n}\n"),
		-Half, -Half, Res * CellM, CellM, Res);
}

FString UWorldForgeEditorLib::MakeWaterExports(const FString& OutDir, const FString& TextureAssetPath, const FString& MapName)
{
	if (!GD.GMap.IsValid())
	{
		return FString();
	}
	if (GD.GWaterRes < 2)
	{
		GD.BuildWaterField(1.0f);
	}
	if (GD.GWaterRes < 2)
	{
		return FString();
	}
	// a grade da simulação (4 m)
	TArray<FColor> Grid;
	int32 GridRes = 0;
	constexpr float GridCell = 4.0f;
	GD.WaterGridPixels(GridCell, Grid, GridRes);
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(GridRes, GridRes, TArrayView64<const FColor>(Grid.GetData(), Grid.Num()), Png);
	if (Png.Num() == 0 || !FFileHelper::SaveArrayToFile(Png, *(OutDir / TEXT("grid.png"))))
	{
		return FString();
	}
	// o hash (o jogo confere): FNV-1a 64 dos pixels em RGBA (a linha 0 primeiro) e de uma linha ASCII por
	// corpo, em ordem: "id,kind,level_mm,flow_x_mm_s,flow_y_mm_s\n"
	uint64 Hash = 14695981039346656037ull;
	auto Mix = [&Hash](uint8 B) { Hash ^= B; Hash *= 1099511628211ull; };
	for (const FColor& C : Grid)
	{
		Mix(C.R); Mix(C.G); Mix(C.B); Mix(C.A);
	}
	FString Bodies;
	int32 Count[5] = {};
	for (const FWorldDetail::FWaterBody& B : GD.GWaterBodies)
	{
		const FString Line = FString::Printf(TEXT("%d,%s,%d,%d,%d\n"), B.Id, FWorldDetail::WaterKindName(B.Kind), B.LevelMm, B.FlowMmS.X, B.FlowMmS.Y);
		for (const TCHAR Ch : Line) Mix(uint8(Ch));
		Bodies += FString::Printf(TEXT("%s  {\"id\": %d, \"kind\": \"%s\", \"level_mm\": %d%s, \"area_m2\": %d, \"max_depth_mm\": %d}"),
			Bodies.IsEmpty() ? TEXT("") : TEXT(",\n"), B.Id, FWorldDetail::WaterKindName(B.Kind), B.LevelMm,
			B.Kind == FWorldDetail::WkRiver ? *FString::Printf(TEXT(", \"flow_mm_s\": [%d, %d]"), B.FlowMmS.X, B.FlowMmS.Y) : TEXT(""),
			B.AreaM2, FMath::RoundToInt(B.MaxDepth * 1000.0f));
		++Count[FMath::Min<int32>(B.Kind, 4)];
	}
	int32 States[5] = {};
	for (const FColor& C : Grid) ++States[FMath::Min<int32>(C.R, 4)];
	const float Half = -GD.GWaterMin;
	const FString Json = FString::Printf(TEXT("{\n \"map\": \"%s\",\n \"version\": 1,\n \"min_m\": [%.1f, %.1f],\n \"cell_m\": %.0f,\n \"res\": %d,\n \"row0\": \"min_y\",\n")
		TEXT(" \"channels\": {\"r\": \"water state: 0 dry, 1 shore, 2 ford, 3 deep, 4 ice\", \"g\": \"body id (0 none)\", \"b\": \"reserved (0)\", \"a\": \"reserved (255)\"},\n")
		TEXT(" \"ford_max_depth_m\": %.2f,\n \"bodies\": [\n%s\n ],\n \"hash\": \"%016llx\",\n")
		TEXT(" \"hash_about\": \"FNV-1a 64 over the PNG pixels as RGBA bytes (row 0 first), then one ASCII line per body in order: id,kind,level_mm,flow_x_mm_s,flow_y_mm_s and a newline (flow 0 0 when not a river)\"\n}\n"),
		*MapName, -Half, -Half, GridCell, GridRes, FWorldDetail::WaterFordM, *Bodies, Hash);
	if (!FFileHelper::SaveStringToFile(Json, *(OutDir / TEXT("grid.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FString();
	}
	// o campo do material (1 m, RGBA16F): a distância com sinal (m, + na água, ±8), a profundidade (0–8 m),
	// a correnteza (m/s; zero até a P2)
	const int32 R = GD.GWaterRes;
	TArray<FFloat16Color> Field;
	Field.SetNumUninitialized(R * R);
	const float L = GD.GMap->WaterLevel;
	for (int32 C = 0; C < R * R; ++C)
	{
		FFloat16Color& O = Field[C];
		O.R = FFloat16(FMath::Clamp(GD.GWaterSd[C], -8.0f, 8.0f));
		O.G = FFloat16(GD.GWaterId[C] ? FMath::Clamp(L - GD.GWaterBed[C], 0.0f, 8.0f) : 0.0f);
		O.B = FFloat16(0.0f);
		O.A = FFloat16(0.0f);
	}
	const bool bExr = FImageUtils::SaveImageByExtension(*(OutDir / TEXT("water.exr")), FImageView(Field.GetData(), R, R));
	// a prévia de 8 bits pra conferir a olho (o EXR não abre em qualquer visor): R = 0,5 + distância/16,
	// G = profundidade/8, B = 255 na água
	{
		TArray<FColor> Prev;
		Prev.SetNumUninitialized(R * R);
		for (int32 C = 0; C < R * R; ++C)
		{
			auto B8 = [](float V) { return uint8(FMath::Clamp(V, 0.0f, 1.0f) * 255.0f + 0.5f); };
			Prev[C] = FColor(B8(0.5f + Field[C].R.GetFloat() / 16.0f), B8(Field[C].G.GetFloat() / 8.0f), GD.GWaterId[C] ? 255 : 0, 255);
		}
		TArray64<uint8> PrevPng;
		FImageUtils::PNGCompressImageArray(R, R, TArrayView64<const FColor>(Prev.GetData(), Prev.Num()), PrevPng);
		FFileHelper::SaveArrayToFile(PrevPng, *(OutDir / TEXT("water_preview.png")));
	}
	if (!TextureAssetPath.IsEmpty())
	{
		const FString Name = FPackageName::GetShortName(TextureAssetPath);
		UTexture2D* Tex = LoadOrCreateAsset<UTexture2D>(TextureAssetPath);
		Tex->Modify();
		Tex->Source.Init(R, R, 1, 1, TSF_RGBA16F, reinterpret_cast<const uint8*>(Field.GetData()));
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_HDR;   // RGBA16F sem compressão: a distância tem de ser exata
		Tex->MipGenSettings = TMGS_NoMipmaps;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		Tex->Filter = TF_Bilinear;
		Tex->PostEditChange();
		Tex->MarkPackageDirty();
	}
	const FString Sum = FString::Printf(TEXT("%d corpos (lago %d, oasis %d, rio %d, mar %d, gelo %d); grade %dx%d a %.0f m: seco %d, margem %d, vau %d, fundo %d, gelo %d; hash %016llx; exr %s"),
		GD.GWaterBodies.Num(), Count[0], Count[1], Count[2], Count[3], Count[4], GridRes, GridRes, GridCell,
		States[0], States[1], States[2], States[3], States[4], Hash, bExr ? TEXT("ok") : TEXT("FALHOU"));
	UE_LOG(LogTemp, Display, TEXT("FORGE water exports %s: %s"), *MapName, *Sum);
	return Sum;
}

TArray<FString> UWorldForgeEditorLib::SpawnWaterBodies(UMaterialInterface* Material, const FString& AssetDir, const FString& MapName)
{
	TArray<FString> Paths;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !GD.GMap.IsValid())
	{
		return Paths;
	}
	if (GD.GWaterRes < 2)
	{
		GD.BuildWaterField(1.0f);
	}
	const float Half = GD.GMap->Half();
	for (const FWorldDetail::FWaterBody& B : GD.GWaterBodies)
	{
		const bool bSea = B.Kind == FWorldDetail::WkSea;
		const float CellM = bSea ? 4.0f : 2.0f;
		TArray<FIntPoint> Cells;
		GD.WaterBodyCells(B.Id, CellM, 2.0f * CellM, Cells);
		if (Cells.Num() == 0)
		{
			continue;
		}
		// a origem do ator: o meio da caixa das células (o mar, o centro do mapa: a saia é simétrica)
		FIntPoint Lo(MAX_int32, MAX_int32), Hi(MIN_int32, MIN_int32);
		for (const FIntPoint& C : Cells)
		{
			Lo = Lo.ComponentMin(C);
			Hi = Hi.ComponentMax(C);
		}
		const FVector2f Ctr = bSea ? FVector2f::ZeroVector
			: FVector2f(-Half + 0.5f * (Lo.X + Hi.X + 1) * CellM, -Half + 0.5f * (Lo.Y + Hi.Y + 1) * CellM);
		const float Z = B.LevelMm / 1000.0f;

		FMeshDescription Desc;
		FStaticMeshAttributes Attr(Desc);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Sgn = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> Uvs = Attr.GetVertexInstanceUVs();
		const FPolygonGroupID Group = Desc.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Water");
		// um vértice por canto (a superfície é chata: normal pra cima e UV = o XY do mundo em metros)
		auto AddCorner = [&](FVector2f P) -> FVertexInstanceID
		{
			const FVertexID V = Desc.CreateVertex();
			Pos[V] = FVector3f((P.X - Ctr.X) * 100.0f, (P.Y - Ctr.Y) * 100.0f, 0.0f);
			const FVertexInstanceID VI = Desc.CreateVertexInstance(V);
			Nrm[VI] = FVector3f(0.0f, 0.0f, 1.0f);
			Tan[VI] = FVector3f(1.0f, 0.0f, 0.0f);
			Sgn[VI] = 1.0f;
			Uvs[VI] = FVector2f(P.X, P.Y);
			return VI;
		};
		TMap<FIntPoint, FVertexInstanceID> Corners;
		auto CornerAt = [&](int32 X, int32 Y) -> FVertexInstanceID
		{
			if (const FVertexInstanceID* Found = Corners.Find(FIntPoint(X, Y)))
			{
				return *Found;
			}
			return Corners.Add(FIntPoint(X, Y), AddCorner(FVector2f(-Half + X * CellM, -Half + Y * CellM)));
		};
		// a face pra cima na ordem da Unreal (+x+y, +x−y, −x−y, −x+y: a +Z do CreateCube)
		auto Quad = [&](FVertexInstanceID C11, FVertexInstanceID C10, FVertexInstanceID C00, FVertexInstanceID C01)
		{
			const FVertexInstanceID Q[4] = { C11, C10, C00, C01 };
			Desc.CreatePolygon(Group, Q);
		};
		for (const FIntPoint& C : Cells)
		{
			Quad(CornerAt(C.X + 1, C.Y + 1), CornerAt(C.X + 1, C.Y), CornerAt(C.X, C.Y), CornerAt(C.X, C.Y + 1));
		}
		if (bSea)
		{
			// a saia do mar até o horizonte (o plano de antes ia a 8× o meio lado), em volta do mapa
			const float H = Half, F = 8.0f * Half;
			auto Rect = [&](float X0, float Y0, float X1, float Y1)
			{
				Quad(AddCorner(FVector2f(X1, Y1)), AddCorner(FVector2f(X1, Y0)), AddCorner(FVector2f(X0, Y0)), AddCorner(FVector2f(X0, Y1)));
			};
			Rect(-F, -F, F, -H);
			Rect(-F, H, F, F);
			Rect(-F, -H, -H, H);
			Rect(H, -H, F, H);
		}
		const int32 Polys = Desc.Polygons().Num();

		const FString Name = FString::Printf(TEXT("SM_Water_%s_%d"), *MapName, B.Id);
		const FString AssetPath = AssetDir / Name;
		UStaticMesh* Mesh = LoadOrCreateAsset<UStaticMesh>(AssetPath);
		Mesh->Modify();
		Mesh->SetNumSourceModels(1);
		FStaticMeshSourceModel& Model = Mesh->GetSourceModel(0);
		Model.BuildSettings.bRecomputeNormals = false;
		Model.BuildSettings.bRecomputeTangents = false;
		Model.BuildSettings.bGenerateLightmapUVs = false;
		TArray<FStaticMaterial> Materials;
		Materials.Emplace(Material, FName(TEXT("Water")), FName(TEXT("Water")));
		Mesh->SetStaticMaterials(Materials);
		Mesh->GetSectionInfoMap().Clear();
		Mesh->GetSectionInfoMap().Set(0, 0, FMeshSectionInfo(0));
		Mesh->CreateMeshDescription(0, MoveTemp(Desc));
		Mesh->CommitMeshDescription(0);
		FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
		Nanite.bEnabled = false;   // a água (Single Layer Water) não passa pelo Nanite
		Mesh->SetNaniteSettings(Nanite);
		Mesh->Build(true);
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();

		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(Ctr.X * 100.0, Ctr.Y * 100.0, Z * 100.0)));
		if (!Actor)
		{
			continue;
		}
		Actor->SetActorLabel(FString::Printf(TEXT("Water_%s_%d"), FWorldDetail::WaterKindName(B.Kind), B.Id));
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		Comp->SetStaticMesh(Mesh);
		Comp->SetMaterial(0, Material);
		Comp->SetCastShadow(false);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// o Look lê por instância (um material só): a cota, o tipo, a turbidez e a escala da espuma (neutras)
		Comp->SetDefaultCustomPrimitiveDataFloat(0, Z);
		Comp->SetDefaultCustomPrimitiveDataFloat(1, float(B.Kind));
		Comp->SetDefaultCustomPrimitiveDataFloat(2, 0.0f);
		Comp->SetDefaultCustomPrimitiveDataFloat(3, 1.0f);
		Paths.Add(AssetPath);
		UE_LOG(LogTemp, Display, TEXT("FORGE water body %d (%s): %d células de %.0f m, %d polígonos, cota %.2f m"),
			B.Id, FWorldDetail::WaterKindName(B.Kind), Cells.Num(), CellM, Polys, Z);
	}
	return Paths;
}

float UWorldForgeEditorLib::SliceRoadHalf(FVector2D PointCm)
{
	const FWorldDetail::FRoadHit Hit = GD.RoadAt(GD.ToMap(PointCm), INDEX_NONE, true);
	return Hit.Road != INDEX_NONE ? Hit.Half() * 100.0f : 0.0f;
}

UStaticMesh* UWorldForgeEditorLib::VegetationToStaticMesh(USkeletalMesh* Source, const FString& AssetPath, bool bBakeParts, bool bNoVoxels)
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
	UStaticMesh* Out = LoadOrCreateAsset<UStaticMesh>(AssetPath);
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
	if (bNoVoxels)
	{
		// "área", a técnica antiga de folhagem (as peças da assembly herdam a forma
		// da árvore, NaniteHelper.cpp InheritAssemblySettings)
		Nanite.ShapePreservation = ENaniteShapePreservation::PreserveArea;
	}
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

	// Neve assenta no que olha pra cima e falha em manchas de ~3 m (o vento
	// varre). A agulha tem dois lados: vale a normal do lado que se vê (a do
	// vértice × TwoSidedSign). Só a do vértice deixava metade das agulhas sem
	// neve, e a árvore só ficava branca de longe, nos voxels (teste de 06/10:
	// com mais detalhe ela ficava verde). Rugosidade da neve ~0.65.
	auto* Code = Cast<UMaterialExpressionCustom>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCustom::StaticClass(), 2700, 200));
	Code->OutputType = CMOT_Float3;
	Code->Code = TEXT(
		"float n = frac(sin(dot(floor(P.xy / 300.0), float2(12.9898, 78.233))) * 43758.5453);\n"
		"float up = saturate((N.z * Parameters.TwoSidedSign - 0.1) * 1.8) * lerp(0.75, 1.0, n);\n"
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
	S.Frame = J->HasField(TEXT("frame")) ? int32(J->GetNumberField(TEXT("frame"))) : -1;
	WorldSettingsJson::ReadVillage(*J, S);
	GSettingsOverride = S;
	GImportOverride.Reset();
	J->TryGetStringField(TEXT("imported"), GImportOverride);
	// as edições à mão (apagados), gravadas pelo app junto dos controles
	GEditsOverride.Reset();
	FString EditsPath;
	if (J->TryGetStringField(TEXT("edits"), EditsPath))
	{
		FString EditsText;
		TSharedPtr<FWorldEdits> E = MakeShared<FWorldEdits>();
		if (FFileHelper::LoadFileToString(EditsText, *EditsPath) && E->FromJson(EditsText))
		{
			GEditsOverride = E;
			UE_LOG(LogTemp, Display, TEXT("FORGE world edits: %d apagados (%s)"), E->Deleted.Num(), *EditsPath);
		}
	}
	OutBiome = int32(S.Biome);
	OutMapSize = S.MapSize;
	UE_LOG(LogTemp, Display, TEXT("FORGE world settings: seed %d, %d players, %.0f m, biome %d"), S.Seed, S.Players, S.MapSize, OutBiome);
	return true;
}

/** O recorte perto do cursor (a máscara: 1 fora do cilindro câmera → cursor; a sombra fica). */
static UMaterialExpression* MakeOcclusionFadeMask(UMaterial* Material, UMaterialParameterCollection* Collection)
{
	auto New = [Material](UClass* Class, int32 X, int32 Y) { return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y); };

	auto Param = [&](const TCHAR* Name, int32 Y)
	{
		auto* P = Cast<UMaterialExpressionCollectionParameter>(New(UMaterialExpressionCollectionParameter::StaticClass(), 3400, Y));
		P->Collection = Collection;
		P->ParameterName = Name;
		P->ParameterId = Collection->GetParameterId(Name);
		return P;
	};
	UMaterialExpression* Cam = Param(TEXT("TreeFadeCamera"), 700);
	UMaterialExpression* Cur = Param(TEXT("TreeFadeCursor"), 800);
	UMaterialExpression* WorldPos = New(UMaterialExpressionWorldPosition::StaticClass(), 3400, 900);
	UMaterialExpression* Screen = New(UMaterialExpressionScreenPosition::StaticClass(), 3400, 1000);

	// distância do pixel até o segmento câmera → cursor; dentro do cilindro, e só
	// entre os dois, o pixel some em pontilhado (ruído de gradiente intercalado).
	// Os materiais das árvores do PVE são opacos (folhas de geometria; a máscara
	// deles não vale): fora do cilindro a máscara é 1, igual ao opaco, e as cópias
	// do WorldForge ligam o modo "masked" (build_slice.py, tree_materials)
	auto* Code = Cast<UMaterialExpressionCustom>(New(UMaterialExpressionCustom::StaticClass(), 3700, 700));
	Code->OutputType = CMOT_Float1;
	Code->Code = TEXT(
		"float R = U.w;\n"
		"if (R <= 0) return 1.0;\n"
		"float3 ab = U.xyz - C.xyz;\n"
		"float t = saturate(dot(P - C.xyz, ab) / max(dot(ab, ab), 1.0));\n"
		"float d = length(P - (C.xyz + ab * t));\n"
		"float fade = (1.0 - smoothstep(R * 0.55, R, d)) * step(t, 0.995);\n"
		"float2 q = floor(Pix) + 5.588238 * (float)(View.StateFrameIndexMod8);\n"
		"float n = frac(52.9829189 * frac(dot(q, float2(0.06711056, 0.00583715))));\n"
		"return fade > n ? 0.0 : 1.0;");
	Code->Inputs.Empty();
	for (const TCHAR* InputName : { TEXT("C"), TEXT("U"), TEXT("P"), TEXT("Pix") })
	{
		FCustomInput& In = Code->Inputs.AddDefaulted_GetRef();
		In.InputName = InputName;
	}
	Code->Inputs[0].Input.Connect(0, Cam);
	Code->Inputs[1].Input.Connect(0, Cur);
	Code->Inputs[2].Input.Connect(0, WorldPos);
	Code->Inputs[3].Input.Connect(1, Screen);   // PixelPosition
	Code->RebuildOutputs();

	// a sombra da árvore fica (a árvore some só da vista, não do chão): o Nanite
	// rasteriza a sombra com SHADOW_DEPTH_SHADER, que o ShadowPassSwitch lê
	auto* One = Cast<UMaterialExpressionConstant>(New(UMaterialExpressionConstant::StaticClass(), 3700, 1000));
	One->R = 1.0f;
	auto* Switch = Cast<UMaterialExpressionShadowReplace>(New(UMaterialExpressionShadowReplace::StaticClass(), 4000, 700));
	Switch->Default.Connect(0, Code);
	Switch->Shadow.Connect(0, One);
	return Switch;
}

bool UWorldForgeEditorLib::AddOcclusionFadeToMaterial(UMaterial* Material, UMaterialParameterCollection* Collection)
{
	UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
	if (!Data || !Collection || !Material->bUseMaterialAttributes || !Data->MaterialAttributes.Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE fade: %s does not use material attributes"), Material ? *Material->GetName() : TEXT("null"));
		return false;
	}
	UMaterialExpression* Source = Data->MaterialAttributes.Expression;
	const int32 SourceOutput = Data->MaterialAttributes.OutputIndex;
	auto New = [Material](UClass* Class, int32 X, int32 Y) { return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y); };
	UMaterialExpression* Switch = MakeOcclusionFadeMask(Material, Collection);

	auto* Set = Cast<UMaterialExpressionSetMaterialAttributes>(New(UMaterialExpressionSetMaterialAttributes::StaticClass(), 4300, 400));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->ConnectInputAttribute(MP_OpacityMask, Switch, 0);
	Data->MaterialAttributes.Connect(0, Set);
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE fade added to %s"), *Material->GetName());
	return true;
}

bool UWorldForgeEditorLib::AddOcclusionFadeToOpacityMask(UMaterial* Material, UMaterialParameterCollection* Collection)
{
	UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
	if (!Data || !Collection || Material->bUseMaterialAttributes || Data->OpacityMask.Expression)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE fade: %s já tem máscara ou usa atributos"), Material ? *Material->GetName() : TEXT("null"));
		return false;
	}
	Data->OpacityMask.Connect(0, MakeOcclusionFadeMask(Material, Collection));
	Material->BlendMode = BLEND_Masked;
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("FORGE fade (máscara) added to %s"), *Material->GetName());
	return true;
}
