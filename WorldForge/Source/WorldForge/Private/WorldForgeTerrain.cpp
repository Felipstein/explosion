#include "WorldForgeTerrain.h"

#include "Generation/WorldTrees.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Generation/WorldPalette.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Generation/WorldNoise.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldForgeMeshes.h"

namespace
{
	const TCHAR* TerrainMaterialPath = TEXT("/Game/Materials/M_Terrain.M_Terrain");
	const TCHAR* WaterMaterialPath = TEXT("/Game/Materials/M_Water.M_Water");
	const TCHAR* PropMaterialPath = TEXT("/Game/Materials/M_Prop.M_Prop");
	constexpr int32 ChunkCells = 64;          // células por pedaço da malha (65×65 vértices)
	constexpr float SkirtDepth = 40.0f;       // m abaixo do ponto mais baixo: a "maquete" tem paredes

	UMaterialInterface* LoadMaterial(const TCHAR* Path)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("FORGE material missing: %s (rode forge.ps1 build)"), Path);
		}
		return M;
	}

	UTexture2D* MakeTexture(int32 W, int32 H, const TArray<FColor>& Pixels, bool bSRGB, const TCHAR* Name)
	{
		// nome único: com o mesmo nome a Unreal reconstrói o objeto antigo no
		// mesmo lugar e o material continua preso ao recurso velho (a partir do
		// 2º mapa o chão caía na textura padrão e ficava cinza)
		UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(Name)),
			TConstArrayView64<uint8>(reinterpret_cast<const uint8*>(Pixels.GetData()), int64(Pixels.Num()) * 4));
		T->SRGB = bSRGB;
		T->Filter = TF_Bilinear;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->UpdateResource();
		return T;
	}
}

AWorldForgeTerrain::AWorldForgeTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(Root);
	Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // o raio do mouse usa a grade de altura
	Ground->bUseAsyncCooking = true;
	WaterPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Water"));
	WaterPlane->SetupAttachment(Root);
	WaterPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterPlane->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		WaterPlane->SetStaticMesh(Plane.Object);
	}
}

void AWorldForgeTerrain::ShowMap(TSharedPtr<const FWorldMap> InMap)
{
	Map = InMap;
	if (!Map.IsValid())
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	BuildDataTextures();
	BuildGroundMesh();
	BuildProps();
	BuildMinimap();

	// água: plano de 1 m da engine esticado no mapa, na cota do nível d'água
	const FWorldMap& M = *Map;
	float MinH = TNumericLimits<float>::Max();
	for (float H : M.Height) MinH = FMath::Min(MinH, H);
	const bool bWater = M.WaterLevel > MinH;
	WaterPlane->SetVisibility(bWater);
	WaterPlane->SetRelativeLocation(FVector(0, 0, M.WaterLevel * 100.0));
	WaterPlane->SetRelativeScale3D(FVector(M.Half() * 2.0, M.Half() * 2.0, 1.0));
	if (!WaterMID)
	{
		if (UMaterialInterface* WM = LoadMaterial(WaterMaterialPath))
		{
			WaterMID = UMaterialInstanceDynamic::Create(WM, this);
			WaterPlane->SetMaterial(0, WaterMID);

		}
	}
	if (WaterMID)
	{
		// cor da água por bioma (a funda é a do minimapa; a rasa, mais clara e verde)
		const FWorldPalette P = FWorldPalette::For(M.Settings.Biome);
		WaterMID->SetVectorParameterValue(TEXT("Deep"), P.Water * 0.6f);
		WaterMID->SetVectorParameterValue(TEXT("Shallow"), P.Water * 2.4f + FLinearColor(0.01f, 0.03f, 0.02f));
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE shown in %.0f ms (%d props)"), 1000.0 * (FPlatformTime::Seconds() - T0), M.Props.Num());
}

void AWorldForgeTerrain::BuildDataTextures()
{
	const FWorldMap& M = *Map;
	DataMap = MakeTexture(M.N, M.N, M.Layers, false, TEXT("DataMap"));

	// camada de depuração: passabilidade (penhasco vermelho, água azul) e
	// territórios (cor do jogador mais perto pelo caminho)
	TArray<FColor> Over;
	Over.SetNumUninitialized(M.N * M.N);
	for (int32 K = 0; K < Over.Num(); ++K)
	{
		FColor C(0, 0, 0, 0);
		if (Overlay == 1)
		{
			if (M.Pass[K] == EWorldPass::Cliff) C = FColor(255, 30, 20, 200);
			else if (M.Pass[K] == EWorldPass::Water) C = FColor(20, 80, 255, 120);
			else C = FColor(40, 255, 60, 60);
		}
		else if (Overlay == 2 && M.Territory.IsValidIndex(K) && M.Territory[K] >= 0)
		{
			C = PlayerColor(M.Territory[K]).ToFColor(false);   // linear (o material lê sem conversão)
			C.A = 110;
		}
		Over[K] = C;
	}
	OverlayMap = MakeTexture(M.N, M.N, Over, false, TEXT("OverlayMap"));

	if (!GroundMID)
	{
		if (UMaterialInterface* TM = LoadMaterial(TerrainMaterialPath))
		{
			GroundMID = UMaterialInstanceDynamic::Create(TM, this);
		}
	}
	if (GroundMID)
	{
		const FWorldPalette P = FWorldPalette::For(M.Settings.Biome);
		GroundMID->SetTextureParameterValue(TEXT("DataMap"), DataMap);
		GroundMID->SetTextureParameterValue(TEXT("OverlayMap"), OverlayMap);
		GroundMID->SetScalarParameterValue(TEXT("OverlayOpacity"), Overlay ? 1.0f : 0.0f);
		// UV do mapa = (x + metade) / lado, com meia célula de borda (centro do texel)
		GroundMID->SetScalarParameterValue(TEXT("MapSize"), float(M.N) * M.Cell);
		GroundMID->SetVectorParameterValue(TEXT("Lush"), P.Lush);
		GroundMID->SetVectorParameterValue(TEXT("Dry"), P.Dry);
		GroundMID->SetVectorParameterValue(TEXT("Dirt"), P.Dirt);
		GroundMID->SetVectorParameterValue(TEXT("Sand"), P.Sand);
		GroundMID->SetVectorParameterValue(TEXT("Rock"), P.Rock);
		GroundMID->SetScalarParameterValue(TEXT("Snow"), P.Snow);
	}
}

void AWorldForgeTerrain::SetOverlay(int32 Mode)
{
	Overlay = Mode;
	if (Map.IsValid())
	{
		BuildDataTextures();
	}
}

void AWorldForgeTerrain::BuildGroundMesh()
{
	const FWorldMap& M = *Map;
	Ground->ClearAllMeshSections();
	const int32 Chunks = FMath::DivideAndRoundUp(M.N - 1, ChunkCells);
	float MinH = TNumericLimits<float>::Max();
	for (float H : M.Height) MinH = FMath::Min(MinH, H);
	const float Bottom = MinH - SkirtDepth;

	auto Normal = [&M](int32 I, int32 J)
	{
		const int32 I0 = FMath::Max(I - 1, 0), I1 = FMath::Min(I + 1, M.N - 1);
		const int32 J0 = FMath::Max(J - 1, 0), J1 = FMath::Min(J + 1, M.N - 1);
		const float DX = (M.Height[M.Index(I1, J)] - M.Height[M.Index(I0, J)]) / ((I1 - I0) * M.Cell);
		const float DY = (M.Height[M.Index(I, J1)] - M.Height[M.Index(I, J0)]) / ((J1 - J0) * M.Cell);
		return FVector(-DX, -DY, 1.0).GetSafeNormal();
	};

	int32 Section = 0;
	for (int32 CJ = 0; CJ < Chunks; ++CJ)
	{
		for (int32 CI = 0; CI < Chunks; ++CI)
		{
			const int32 I0 = CI * ChunkCells, J0 = CJ * ChunkCells;
			const int32 I1 = FMath::Min(I0 + ChunkCells, M.N - 1), J1 = FMath::Min(J0 + ChunkCells, M.N - 1);
			const int32 W = I1 - I0 + 1, H = J1 - J0 + 1;
			TArray<FVector> V;
			TArray<FVector> Nrm;
			TArray<FVector2D> UV;
			TArray<FProcMeshTangent> Tan;
			TArray<int32> Tri;
			V.Reserve(W * H);
			Nrm.Reserve(W * H);
			UV.Reserve(W * H);
			Tan.Reserve(W * H);
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					const FVector2f P = M.CellPos(I, J);
					V.Add(FVector(P.X, P.Y, M.Height[M.Index(I, J)]) * 100.0);
					const FVector N = Normal(I, J);
					Nrm.Add(N);
					UV.Add(FVector2D(P.X, P.Y));      // metros: o material projeta as texturas por eles
					Tan.Add(FProcMeshTangent(FVector(N.Z, 0.0, -N.X).GetSafeNormal(), false));
				}
			}
			Tri.Reserve((W - 1) * (H - 1) * 6);
			for (int32 J = 0; J < H - 1; ++J)
			{
				for (int32 I = 0; I < W - 1; ++I)
				{
					const int32 A = J * W + I, B = A + 1, C = A + W, D = C + 1;
					// diagonal alternada (sem o "serrilhado" de todas na mesma direção)
					if (((I0 + I) + (J0 + J)) & 1)
					{
						Tri.Append({ A, C, B, B, C, D });
					}
					else
					{
						Tri.Append({ A, C, D, A, D, B });
					}
				}
			}
			Ground->CreateMeshSection(Section, V, Tri, Nrm, UV, {}, Tan, false);
			if (GroundMID)
			{
				Ground->SetMaterial(Section, GroundMID);
			}
			++Section;
		}
	}

	// paredes da maquete: da borda do terreno até abaixo do ponto mais baixo
	TArray<FVector> V;
	TArray<FVector> Nrm;
	TArray<FVector2D> UV;
	TArray<FProcMeshTangent> Tan;
	TArray<int32> Tri;
	auto Wall = [&](TFunctionRef<FIntPoint(int32)> Cell, FVector Out)
	{
		const int32 Base = V.Num();
		for (int32 K = 0; K < M.N; ++K)
		{
			const FIntPoint C = Cell(K);
			const FVector2f P = M.CellPos(C.X, C.Y);
			V.Add(FVector(P.X, P.Y, M.Height[M.Index(C.X, C.Y)]) * 100.0);
			V.Add(FVector(P.X, P.Y, Bottom) * 100.0);
			Nrm.Append({ Out, Out });
			UV.Append({ FVector2D(P.X, P.Y), FVector2D(P.X, P.Y) });
			Tan.Append({ FProcMeshTangent(FVector(0, 0, 1), false), FProcMeshTangent(FVector(0, 0, 1), false) });
		}
		for (int32 K = 0; K < M.N - 1; ++K)
		{
			const int32 A = Base + 2 * K, B = A + 1, C = A + 2, D = A + 3;
			// a ordem certa depende do lado: confere pela normal da face
			const FVector FaceN = ((V[C] - V[A]) ^ (V[B] - V[A]));
			if ((FaceN | Out) >= 0) Tri.Append({ A, B, C, C, B, D });
			else Tri.Append({ A, C, B, C, D, B });
		}
	};
	const int32 Last = M.N - 1;
	Wall([](int32 K) { return FIntPoint(K, 0); }, FVector(0, -1, 0));
	Wall([Last](int32 K) { return FIntPoint(K, Last); }, FVector(0, 1, 0));
	Wall([](int32 K) { return FIntPoint(0, K); }, FVector(-1, 0, 0));
	Wall([Last](int32 K) { return FIntPoint(Last, K); }, FVector(1, 0, 0));
	Ground->CreateMeshSection(Section, V, Tri, Nrm, UV, {}, Tan, false);
	if (GroundMID)
	{
		Ground->SetMaterial(Section, GroundMID);
	}
}

UHierarchicalInstancedStaticMeshComponent* AWorldForgeTerrain::MakeInstances(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, bool bShadows)
{
	UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(Name), RF_Transient);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Material);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadows);
	C->NumCustomDataFloats = 3;   // tinta por instância (cor do jogador, variação das árvores)
	C->RegisterComponent();
	PropComponents.Add(C);
	return C;
}

void AWorldForgeTerrain::BuildProps()
{
	for (UHierarchicalInstancedStaticMeshComponent* C : PropComponents)
	{
		if (C) C->DestroyComponent();
	}
	PropComponents.Reset();

	UMaterialInterface* PropMat = LoadMaterial(PropMaterialPath);
	UMaterialInstanceDynamic* MID = PropMat ? UMaterialInstanceDynamic::Create(PropMat, this) : nullptr;
	const FWorldMap& M = *Map;
	const FWorldPalette Pal = FWorldPalette::For(M.Settings.Biome);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Snow"), Pal.Snow);
	}

	// as malhas não dependem do mapa: geradas uma vez
	constexpr int32 Variants = 3;
	using WorldForgeMeshes::EKind;
	if (PropMeshes.Num() == 0)
	{
		for (int32 K = 0; K < int32(EKind::Num); ++K)
		{
			for (int32 V = 0; V < Variants; ++V)
			{
				PropMeshes.Add(WorldForgeMeshes::Build(EKind(K), uint32(V + 1), PropMat, this));
			}
		}
	}
	auto MeshOf = [this](EKind Kind, int32 Variant) { return PropMeshes[int32(Kind) * Variants + Variant % Variants].Get(); };

	// um componente instanciado por malha
	TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> Comps;
	TMap<UStaticMesh*, TArray<FTransform>> Xforms;
	TMap<UStaticMesh*, TArray<float>> Tints;
	auto Add = [&](UStaticMesh* Mesh, const FTransform& X, const FLinearColor& Tint)
	{
		Xforms.FindOrAdd(Mesh).Add(X);
		TArray<float>& T = Tints.FindOrAdd(Mesh);
		T.Append({ Tint.R, Tint.G, Tint.B });
	};

	// Árvores: o tipo e o tamanho da regra única (WorldTrees.h), os mesmos da
	// qualidade real. Com as árvores de verdade já montadas (/Game/Slice/Trees,
	// pelas fatias), elas mesmas — o app desenha o Nanite bem mais grosso
	// (r.Nanite.MaxPixelsPerEdge, AWorldForgeGameMode); sem elas, as simples
	// em código, esticadas pro mesmo tamanho.
	const bool bDesert = M.Settings.Biome == EWorldBiome::Desert;
	const bool bWinter = M.Settings.Biome == EWorldBiome::Winter;
	UStaticMesh* RealTree[2] = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Slice/Trees/SM_broadleaf.SM_broadleaf"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Slice/Trees/SM_conifer.SM_conifer"), nullptr, LOAD_NoWarn | LOAD_Quiet) };
	TMap<UStaticMesh*, TArray<FTransform>> RealXforms;
	auto ProxySize = [](UStaticMesh* Mesh)
	{
		const FBoxSphereBounds B = Mesh->GetBounds();
		return FVector2f((B.Origin.Z + B.BoxExtent.Z) / 100.0f, 2.0f * FMath::Max(B.BoxExtent.X, B.BoxExtent.Y) / 100.0f);
	};
	for (const FWorldProp& P : M.Props)
	{
		const FVector Loc(P.Pos.X * 100.0, P.Pos.Y * 100.0, P.Z * 100.0);
		const FTransform X(FRotator(0, P.Yaw, 0), Loc, FVector(P.Scale));
		const float Var = 0.85f + 0.3f * (P.Variant % 7) / 6.0f;
		if (P.Type == EWorldPropType::Tree)
		{
			const WorldTrees::EKind Kind = WorldTrees::KindOf(M.Settings.Biome, P.Variant);
			const float Scale = WorldTrees::ScaleOf(P, M.Settings.Biome);
			UStaticMesh* Real = Kind == WorldTrees::EKind::Shrub ? nullptr : RealTree[Kind == WorldTrees::EKind::Conifer ? 1 : 0];
			if (Real)
			{
				RealXforms.FindOrAdd(Real).Add(FTransform(FRotator(0, P.Yaw, 0), Loc, FVector(Scale)));
				continue;
			}
			UStaticMesh* Proxy = MeshOf(Kind == WorldTrees::EKind::Conifer ? EKind::Conifer : EKind::Broadleaf, P.Variant);
			const FVector2f Want = WorldTrees::MeshSize(Kind) * Scale, Have = ProxySize(Proxy);
			const float SXY = Want.Y / FMath::Max(Have.Y, 0.1f), SZ = Want.X / FMath::Max(Have.X, 0.1f);
			FLinearColor Tint(Var, Var, Var);
			if (bDesert) Tint = FLinearColor(1.6f * Var, 1.25f * Var, 0.8f * Var);
			Add(Proxy, FTransform(FRotator(0, P.Yaw, 0), Loc, FVector(SXY, SXY, SZ)), Tint);
		}
		else
		{
			const FLinearColor Tint = bDesert ? FLinearColor(1.5f * Var, 1.0f * Var, 0.7f * Var) : FLinearColor(Var, Var, Var);
			Add(MeshOf(EKind::Rock, P.Variant), X, Tint);
		}
	}
	for (const FWorldResource& R : M.Resources)
	{
		// aglomerados de cristal espalhados no disco do campo (padrão fixo pela posição)
		WorldNoise::FRng Rng(uint64(FMath::Abs(R.Pos.X * 131.0f + R.Pos.Y * 17.0f)) + 7);
		const int32 Count = FMath::RoundToInt(16.0f * R.Amount * FMath::Square(R.Radius / 12.0f));
		for (int32 K = 0; K < Count; ++K)
		{
			const float A = Rng.Range(0, 2 * PI);
			const float D = R.Radius * FMath::Sqrt(Rng.Float()) * 0.9f;
			const FVector2f Q = R.Pos + FVector2f(FMath::Cos(A), FMath::Sin(A)) * D;
			const float S = Rng.Range(1.0f, 2.0f) * (1.0f - 0.45f * D / R.Radius);   // aglomerados de 2–5 m: legíveis de cima
			const FTransform X(FRotator(0, Rng.Range(0, 360), 0), FVector(Q.X * 100.0, Q.Y * 100.0, M.HeightAt(Q) * 100.0), FVector(S));
			Add(MeshOf(EKind::Crystal, K), X, FLinearColor(1, 1, 1));
		}
	}
	for (const FWorldStart& S : M.Starts)
	{
		const FTransform X(FRotator(0, 0, 0), FVector(S.Pos.X * 100.0, S.Pos.Y * 100.0, S.Z * 100.0), FVector(1.0));
		Add(MeshOf(EKind::BasePad, 0), X, PlayerColor(S.Player));
	}

	for (auto& Pair : Xforms)
	{
		const bool bShadows = true;
		UHierarchicalInstancedStaticMeshComponent* C = MakeInstances(*FString::Printf(TEXT("Props_%d"), PropComponents.Num()), Pair.Key, MID, bShadows);
		C->AddInstances(Pair.Value, false, true, false);
		const TArray<float>& T = Tints[Pair.Key];
		for (int32 K = 0; K < Pair.Value.Num(); ++K)
		{
			C->SetCustomDataValue(K, 0, T[3 * K + 0]);
			C->SetCustomDataValue(K, 1, T[3 * K + 1]);
			C->SetCustomDataValue(K, 2, T[3 * K + 2]);
		}
		C->MarkRenderStateDirty();
	}
	for (auto& Pair : RealXforms)
	{
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Pair.Key);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		if (bWinter)
		{
			// os materiais com neve que a fatia de neve deixou (MI_..._Snow)
			const TArray<FStaticMaterial>& Mats = Pair.Key->GetStaticMaterials();
			for (int32 I = 0; I < Mats.Num(); ++I)
			{
				if (!Mats[I].MaterialInterface) continue;
				const FString Name = Mats[I].MaterialInterface->GetName() + TEXT("_Snow");
				if (UMaterialInterface* Snow = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Slice/Trees/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					C->SetMaterial(I, Snow);
				}
			}
		}
		C->RegisterComponent();
		C->AddInstances(Pair.Value, false, true, false);
		PropComponents.Add(C);
	}
}

void AWorldForgeTerrain::BuildMinimap()
{
	// 256×256: a cor do chão (mesma mistura do material) × relevo sombreado,
	// água por cima, depois recursos e bases
	const FWorldMap& M = *Map;
	const FWorldPalette Pal = FWorldPalette::For(M.Settings.Biome);
	constexpr int32 S = MinimapSize;
	TArray<FColor>& Px = MinimapPixels;
	Px.SetNumUninitialized(S * S);
	const FVector3f Sun = FVector3f(-0.5f, -0.6f, 0.62f).GetSafeNormal();
	float MinH = TNumericLimits<float>::Max();
	for (float H : M.Height) MinH = FMath::Min(MinH, H);
	for (int32 Y = 0; Y < S; ++Y)
	{
		for (int32 X = 0; X < S; ++X)
		{
			// a imagem é vista de cima com +X do mundo pra cima e +Y pra direita
			const FVector2f P((0.5f - (Y + 0.5f) / S) * M.Half() * 2.0f, ((X + 0.5f) / S - 0.5f) * M.Half() * 2.0f);
			const FVector2f G = M.ToGrid(P);
			const int32 I = FMath::Clamp(FMath::RoundToInt(G.X), 0, M.N - 1), J = FMath::Clamp(FMath::RoundToInt(G.Y), 0, M.N - 1);
			const float H = M.HeightAt(P);
			FLinearColor C = Pal.Blend(M.Layers[M.Index(I, J)]);
			const float Shade = FMath::Clamp(FVector3f::DotProduct(M.NormalAt(P), Sun) * 1.25f, 0.25f, 1.25f);
			C *= Shade * 2.2f;
			if (H < M.WaterLevel)
			{
				const float Depth = FMath::Clamp((M.WaterLevel - H) / 6.0f, 0.0f, 1.0f);
				C = FMath::Lerp(C * 0.6f, Pal.Water * 3.0f, 0.55f + 0.4f * Depth);
			}
			Px[Y * S + X] = C.ToFColor(true);
		}
	}
	auto Dot = [&](FVector2f P, float RadiusPx, FColor Color)
	{
		const float X = (P.Y / (M.Half() * 2.0f) + 0.5f) * S, Y = (0.5f - P.X / (M.Half() * 2.0f)) * S;
		for (int32 DY = -int32(RadiusPx) - 1; DY <= int32(RadiusPx) + 1; ++DY)
		{
			for (int32 DX = -int32(RadiusPx) - 1; DX <= int32(RadiusPx) + 1; ++DX)
			{
				const int32 PX = int32(X) + DX, PY = int32(Y) + DY;
				if (PX >= 0 && PY >= 0 && PX < S && PY < S && DX * DX + DY * DY <= RadiusPx * RadiusPx)
				{
					Px[PY * S + PX] = Color;
				}
			}
		}
	};
	for (const FWorldResource& R : M.Resources)
	{
		Dot(R.Pos, FMath::Max(2.0f, R.Radius / (M.Half() * 2.0f) * S), FColor(60, 230, 90));
	}
	for (const FWorldStart& St : M.Starts)
	{
		Dot(St.Pos, 6.0f, FColor::White);
		Dot(St.Pos, 4.5f, PlayerColor(St.Player).ToFColor(true));
	}
	Minimap = MakeTexture(S, S, Px, true, TEXT("Minimap"));
}

bool AWorldForgeTerrain::RayHit(const FVector& O, const FVector& D, FVector& Out) const
{
	if (!Map.IsValid())
	{
		return false;
	}
	const FWorldMap& M = *Map;
	auto Above = [&](double T)
	{
		const FVector P = O + D * T;
		return P.Z > FMath::Max(M.HeightAt(FVector2f(P.X, P.Y)), M.WaterLevel);
	};
	// marcha em passos de uma célula e refina por bisseção
	const double Step = M.Cell;
	double T = 0.0;
	for (int32 K = 0; K < 8000; ++K, T += Step)
	{
		if (!Above(T + Step))
		{
			double A = T, B = T + Step;
			for (int32 It = 0; It < 16; ++It)
			{
				const double Mid = 0.5 * (A + B);
				(Above(Mid) ? A : B) = Mid;
			}
			Out = O + D * B;
			return true;
		}
	}
	return false;
}
