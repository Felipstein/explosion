#include "Generation/WorldDetail.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* BiomeName(EWorldBiome Biome)
	{
		return Biome == EWorldBiome::Desert ? TEXT("desert") : (Biome == EWorldBiome::Winter ? TEXT("snow") : TEXT("forest"));
	}

	/** A tabela do espalhamento do bioma (build_slice.py, write_live_manifest). */
	TArray<FWorldLive::FKind> LoadScatterTable(EWorldBiome Biome)
	{
		TArray<FWorldLive::FKind> Out;
		const FString Path = FPaths::ProjectSavedDir() / TEXT("WorldForge") / FString::Printf(TEXT("live_%s.json"), BiomeName(Biome));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("FORGE sem a tabela do espalhamento ao vivo (%s): monte um mapa em qualidade real desse bioma"), *Path);
			return Out;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& E : Root->GetArrayField(TEXT("scatter")))
		{
			const TSharedPtr<FJsonObject> O = E->AsObject();
			FWorldLive::FKind& K = Out.AddDefaulted_GetRef();
			K.Kind = O->GetStringField(TEXT("kind"));
			K.Spacing = O->GetNumberField(TEXT("spacing"));
			K.ScaleMin = O->GetNumberField(TEXT("smin"));
			K.ScaleMax = O->GetNumberField(TEXT("smax"));
			K.Cull = O->GetNumberField(TEXT("cull"));
			for (const TSharedPtr<FJsonValue>& MV : O->GetArrayField(TEXT("meshes")))
			{
				const TSharedPtr<FJsonObject> MO = MV->AsObject();
				FWorldLive::FMesh& M = K.Meshes.AddDefaulted_GetRef();
				M.Mesh = MO->GetStringField(TEXT("mesh"));
				for (const TSharedPtr<FJsonValue>& Mat : MO->GetArrayField(TEXT("materials")))
				{
					M.Materials.Add(Mat->AsString());
				}
				const TArray<TSharedPtr<FJsonValue>>& B = MO->GetArrayField(TEXT("bounds"));
				if (B.Num() == 6)
				{
					M.Bounds = FBox(FVector(B[0]->AsNumber(), B[1]->AsNumber(), B[2]->AsNumber()), FVector(B[3]->AsNumber(), B[4]->AsNumber(), B[5]->AsNumber()));
				}
			}
		}
		return Out;
	}
}

float FWorldLive::SpacingOf(const FKind& K)
{
	const bool bThin = K.Kind == TEXT("Tuft") || K.Kind == TEXT("OasisGrass") || K.Kind == TEXT("Undergrowth");
	return K.Spacing * (bThin ? 1.5f : 1.0f);
}

void FWorldLive::ScatterRegion(const FBox2f& Region, TArray<FKind>& Out)
{
	Out.Reset();
	if (!Detail.IsValid())
	{
		return;
	}
	Out = Scatter;
	for (FKind& K : Out)
	{
		for (FMesh& M : K.Meshes)
		{
			M.Xforms.Reset();
			M.Origins.Reset();
		}
	}
	Detail->bScatterRegion = true;
	Detail->ScatterRegion = Region;
	ParallelFor(Out.Num(), [this, &Out](int32 Index)
	{
		FKind& K = Out[Index];
		TArray<FWorldDetail::FScatterCand> Cands;
		Detail->ScatterCandidates(K.Kind, SpacingOf(K), K.ScaleMin, K.ScaleMax, 0.0f, 0.0f, Cands);
		TArray<FBox> Bounds;
		for (const FMesh& M : K.Meshes) Bounds.Add(M.Bounds);
		TArray<TArray<FTransform>> Xs;
		TArray<TArray<FVector2f>> Os;
		Detail->PlaceMeshes(K.Kind, Cands, Bounds, Xs, &Os);
		for (int32 I = 0; I < K.Meshes.Num() && I < Xs.Num(); ++I)
		{
			K.Meshes[I].Xforms = MoveTemp(Xs[I]);
			K.Meshes[I].Origins = MoveTemp(Os[I]);
		}
	});
	Detail->bScatterRegion = false;
}

TSharedPtr<FWorldLive> FWorldLive::Build(const TSharedPtr<FWorldMap>& Map, TSharedPtr<const FWorldEdits> Edits)
{
	TSharedPtr<FWorldLive> L = MakeShared<FWorldLive>();
	L->Detail = MakeShared<FWorldDetail>();
	L->Edits = Edits.IsValid() ? MakeShared<FWorldEdits>(*Edits) : MakeShared<FWorldEdits>();
	L->Detail->Edits = L->Edits;
	L->Display = Map;
	float RoadHalf;
	bool bPaved;
	FWorldDetail::MainRoadOf(Map->Settings.Biome, RoadHalf, bPaved);
	L->Detail->Setup(MakeShared<FWorldMap>(*Map), FVector2f::ZeroVector, Map->Half(), RoadHalf, bPaved);
	L->SizeM = 2.0f * Map->Half();
	L->bSea = L->Detail->HasSea();
	L->Detail->WeightPixels(L->SizeM, 1.0f, L->WeightsA, L->WeightsB, L->WeightRes);
	// a mesma grade serve de atalho pro espalhamento (FWorldDetail::ScatterWeights)
	L->Detail->WCacheA = L->WeightsA;
	L->Detail->WCacheB = L->WeightsB;
	L->Detail->WCacheRes = L->WeightRes;
	L->Detail->WCacheTexel = 1.0f;
	L->Detail->WCacheSize = L->SizeM;
	L->Detail->WaterInfoPixels(L->SizeM, L->Water, L->WaterRes);

	// o espalhamento: as mesmas regras e malhas do "qualidade real", um tipo por
	// tarefa. O capim, o sub-bosque e o capim do oásis ficam mais ralos (1,5× o
	// espaçamento, ~44% das touceiras): na vista do mapa inteiro a diferença não
	// aparece e o app fica leve
	L->Scatter = LoadScatterTable(Map->Settings.Biome);
	ParallelFor(L->Scatter.Num(), [&L](int32 Index)
	{
		FKind& K = L->Scatter[Index];
		TArray<FWorldDetail::FScatterCand> Cands;
		L->Detail->ScatterCandidates(K.Kind, SpacingOf(K), K.ScaleMin, K.ScaleMax, 0.0f, 0.0f, Cands);
		TArray<FBox> Bounds;
		for (const FMesh& M : K.Meshes) Bounds.Add(M.Bounds);
		TArray<TArray<FTransform>> Xs;
		TArray<TArray<FVector2f>> Os;
		L->Detail->PlaceMeshes(K.Kind, Cands, Bounds, Xs, &Os);
		for (int32 I = 0; I < K.Meshes.Num() && I < Xs.Num(); ++I)
		{
			K.Meshes[I].Xforms = MoveTemp(Xs[I]);
			K.Meshes[I].Origins = MoveTemp(Os[I]);
		}
	});

	// por último a altura final no próprio mapa (o espalhamento acima já usa a
	// altura final pelo SliceHeight da cópia)
	const FWorldMap& D = *L->Detail->GMap;
	TArray<float> H;
	H.SetNumUninitialized(Map->N * Map->N);
	ParallelFor(Map->N, [&](int32 J)
	{
		for (int32 I = 0; I < Map->N; ++I)
		{
			H[Map->Index(I, J)] = L->Detail->SliceHeight(D, Map->CellPos(I, J));
		}
	});
	Map->Height = MoveTemp(H);
	return L;
}
