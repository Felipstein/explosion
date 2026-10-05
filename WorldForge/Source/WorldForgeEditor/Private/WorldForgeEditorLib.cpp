#include "WorldForgeEditorLib.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Generation/WorldGenerator.h"
#include "Generation/WorldNoise.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// o último recorte gerado: as outras funções (espalhamento, trilhas) leem dele
	TSharedPtr<FWorldMap> GMap;
	FVector2f GCenter = FVector2f::ZeroVector;   // centro do recorte no mapa gerado (m)
	float GHalf = 0.0f;                           // meio lado do recorte (m)

	FVector2f ToMap(FVector2D Cm) { return GCenter + FVector2f(Cm.X / 100.0, Cm.Y / 100.0); }

	/** Pesos das 5 camadas do deserto num ponto do mapa (somam 1). */
	void DesertWeights(const FWorldMap& M, FVector2f P, float W[5])
	{
		// camadas do gerador (vértice mais perto): R terra, G vegetação, B margem, A rocha
		const FVector2f G = M.ToGrid(P);
		const int32 I = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), J = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
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
		const float Dirt = Bil(0), Lush = Bil(1), Shore = Bil(2), Rock = Bil(3);
		const float Flow = M.Flow.IsValidIndex(M.Index(I, J)) ? M.Flow[M.Index(I, J)] : 0.0f;
		// manchas largas (ruído) pra variar cascalho × areia
		const float Patch = WorldNoise::Fbm(P.X / 45.0f, P.Y / 45.0f, 911u, 4);
		const float Fine = WorldNoise::Fbm(P.X / 7.0f, P.Y / 7.0f, 913u, 3);
		float Rk = FMath::SmoothStep(0.25f, 0.7f, Rock);
		float Dr = FMath::SmoothStep(0.2f, 0.6f, Dirt + 0.1f * Fine) * (1.0f - Rk);
		// terra rachada: leitos secos (margem dos lagos e por onde a água escorre)
		float Cr = FMath::Clamp(FMath::Max(Shore, FMath::SmoothStep(0.45f, 0.8f, Flow)) + 0.15f * Fine, 0.0f, 1.0f) * (1.0f - Rk) * (1.0f - Dr);
		// cascalho: manchas e o pé das encostas
		float Gv = FMath::Clamp(FMath::SmoothStep(0.05f, 0.45f, Patch + 0.3f * Lush - 0.15f) + 0.6f * FMath::SmoothStep(0.05f, 0.3f, Rock), 0.0f, 1.0f)
			* (1.0f - Rk) * (1.0f - Dr) * (1.0f - Cr);
		float Sd = FMath::Max(0.0f, 1.0f - Rk - Dr - Cr - Gv);
		const float Sum = Sd + Gv + Dr + Rk + Cr;
		W[0] = Sd / Sum; W[1] = Gv / Sum; W[2] = Dr / Sum; W[3] = Rk / Sum; W[4] = Cr / Sum;
	}

	/** Altura do recorte (m): o relevo do gerador + ondulação baixa da areia (dunas de ~6 e 20 m). */
	float SliceHeight(const FWorldMap& M, FVector2f P)
	{
		float W[5];
		DesertWeights(M, P, W);
		return M.HeightAt(P) + W[0] * (0.12f * WorldNoise::Fbm(P.X / 6.0f, P.Y / 6.0f, 917u, 3) + 0.25f * WorldNoise::Fbm(P.X / 20.0f, P.Y / 20.0f, 919u, 2));
	}
}

ALandscape* UWorldForgeEditorLib::BuildSliceLandscape(int32 Seed, int32 Players, int32 Biome, float MapSize, FVector2D CenterM,
	int32 Components, int32 QuadsPerComponent, float QuadSizeM, UMaterialInterface* Material,
	const TArray<ULandscapeLayerInfoObject*>& LayerInfos, FString& Error)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Error = TEXT("sem mundo do editor");
		return nullptr;
	}
	if (LayerInfos.Num() != 5)
	{
		Error = TEXT("esperava 5 camadas: Sand, Gravel, Dirt, Rock, Cracked");
		return nullptr;
	}

	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	const int32 N = Components * QuadsPerComponent + 1;                // vértices por lado
	if (CenterM.X > 1e8)
	{
		CenterM = SuggestSliceCenter(Seed, Players, Biome, MapSize, (N - 1) * QuadSizeM);
	}
	GMap = FWorldGenerator::Generate(S);
	const FWorldMap& M = *GMap;
	GCenter = FVector2f(CenterM.X, CenterM.Y);
	GHalf = 0.5f * (N - 1) * QuadSizeM;

	// alturas: 32768 = 0 m; com escala Z de 100 cada unidade vale 1/128 m
	// (LANDSCAPE_ZSCALE): ±256 m com precisão de ~8 mm
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(N * N);
	TArray<TArray<uint8>> Weights;
	Weights.SetNum(5);
	for (TArray<uint8>& L : Weights) L.SetNumUninitialized(N * N);
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const FVector2f P = GCenter + FVector2f(X * QuadSizeM - GHalf, Y * QuadSizeM - GHalf);
			const float H = SliceHeight(M, P);
			float W[5];
			DesertWeights(M, P, W);
			Heights[Y * N + X] = uint16(FMath::Clamp(FMath::RoundToInt(32768.0f + H * 128.0f), 0, 65535));
			// pesos em 8 bits somando 255 (o resto vai pra areia)
			int32 Acc = 0;
			for (int32 K = 1; K < 5; ++K)
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
	for (int32 K = 0; K < 5; ++K)
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
	UE_LOG(LogTemp, Display, TEXT("FORGE slice landscape %dx%d verts, %.0f m, seed %d, center (%.0f, %.0f)"), N, N, 2.0f * GHalf, Seed, GCenter.X, GCenter.Y);
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
	auto Add = [&](FVector2f P, float Yaw, float Scale, float Sink, float Tilt, WorldNoise::FRng& Rng)
	{
		const FVector2f L = P - GCenter;
		if (FMath::Abs(L.X) > GHalf - 1.0f || FMath::Abs(L.Y) > GHalf - 1.0f) return;
		const float Z = SliceHeight(M, P);
		const FRotator R(Rng.Range(-Tilt, Tilt), Yaw + YawOffset, Rng.Range(-Tilt, Tilt));
		Out.Transforms.Add(FTransform(R, FVector(L.X * 100.0, L.Y * 100.0, (Z - (Sink + SinkM) * Scale) * 100.0), FVector(Scale)));
	};
	const FString K = Kind.ToString();
	if (K == TEXT("RockLarge") || K == TEXT("Shrub"))
	{
		// as posições do gerador (rochas e "árvores" do deserto viram arbustos)
		const EWorldPropType Type = K == TEXT("RockLarge") ? EWorldPropType::Rock : EWorldPropType::Tree;
		for (const FWorldProp& P : M.Props)
		{
			if (P.Type != Type) continue;
			WorldNoise::FRng Rng(uint64(P.Variant) * 7919u + uint64(FMath::Abs(P.Pos.X * 13.0f + P.Pos.Y)));
			Add(P.Pos, P.Yaw, FMath::Lerp(ScaleMin, ScaleMax, Rng.Float()), 0.15f, Type == EWorldPropType::Rock ? 12.0f : 4.0f, Rng);
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
			float W[5];
			DesertWeights(M, P, W);
			const FVector3f N = M.NormalAt(P);
			const float Clump = WorldNoise::Fbm(P.X / 18.0f, P.Y / 18.0f, Seed + 3u, 3);
			float Prob = 0.0f;
			if (K == TEXT("Pebble")) Prob = 0.15f + 0.85f * W[1] + 0.4f * W[2] + 0.5f * W[4];
			else if (K == TEXT("RockSmall")) Prob = (0.05f + 0.7f * W[1] + 0.9f * FMath::SmoothStep(0.05f, 0.4f, W[3])) * (N.Z > 0.6f ? 1.0f : 0.2f);
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

FVector2D UWorldForgeEditorLib::SuggestSliceCenter(int32 Seed, int32 Players, int32 Biome, float MapSize, float SizeM)
{
	FWorldGenSettings S;
	S.Seed = Seed;
	S.Players = Players;
	S.Biome = EWorldBiome(FMath::Clamp(Biome, 0, 2));
	S.MapSize = MapSize;
	const TSharedPtr<FWorldMap> Map = FWorldGenerator::Generate(S);
	const FWorldMap& M = *Map;
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
					float W[5];
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
			const float Score = Entropy + (Dirt > 0.04f ? 0.3f : 0.0f) + (Rock > 0.05f ? 0.3f : 0.0f) - 2.0f * FMath::Max(0.0f, Wet - 0.1f);
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

bool UWorldForgeEditorLib::AddInstances(AActor* Actor, UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCastShadow, float CullDistanceM)
{
	if (!Actor || !Mesh)
	{
		return false;
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transactional);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Static);
	C->SetCastShadow(bCastShadow);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CullDistanceM > 0.0f)
	{
		C->SetCullDistances(0, int32(CullDistanceM * 100.0f));
	}
	C->SetupAttachment(Actor->GetRootComponent());
	Actor->AddInstanceComponent(C);
	C->RegisterComponent();
	C->AddInstances(Transforms, false, true, false);
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
