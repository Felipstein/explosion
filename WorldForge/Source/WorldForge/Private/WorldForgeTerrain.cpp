#include "WorldForgeTerrain.h"

#include "Generation/WorldTrees.h"
#include "Generation/WorldDetail.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Generation/WorldPalette.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
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
	WaterBodies = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterBodies"));
	WaterBodies->SetupAttachment(Root);
	WaterBodies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterBodies->SetCastShadow(false);
}

bool AWorldForgeTerrain::BuildWaterBodies(UMaterialInterface* Material)
{
	WaterBodies->ClearAllMeshSections();
	if (!Live.IsValid() || !Live->Detail.IsValid() || !Live->Detail->GMap.IsValid())
	{
		return false;
	}
	FWorldDetail& D = *Live->Detail;
	const double T0 = FPlatformTime::Seconds();
	D.BuildWaterField(2.0f);   // (o "qualidade real" usa 1 m; aqui 2 m basta e custa um quarto)
	const float Half = D.GMap->Half();
	int32 Section = 0;
	for (const FWorldDetail::FWaterBody& B : D.GWaterBodies)
	{
		const bool bSea = B.Kind == FWorldDetail::WkSea;
		const float CellM = bSea ? 4.0f : 2.0f;
		TArray<FIntPoint> Cells;
		D.WaterBodyCells(B.Id, CellM, 2.0f * CellM, Cells);
		if (Cells.Num() == 0)
		{
			continue;
		}
		const float Z = B.LevelMm / 10.0f;   // cm
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FProcMeshTangent> Tan;
		TMap<FIntPoint, int32> Corners;
		auto Add = [&](float X, float Y)
		{
			V.Add(FVector(X * 100.0, Y * 100.0, Z));
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(X, Y));
			Tan.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
			return V.Num() - 1;
		};
		auto CornerAt = [&](int32 X, int32 Y)
		{
			if (const int32* Found = Corners.Find(FIntPoint(X, Y))) return *Found;
			return Corners.Add(FIntPoint(X, Y), Add(-Half + X * CellM, -Half + Y * CellM));
		};
		// a mesma ordem da malha do "qualidade real" (a face pra cima da Unreal)
		auto Quad = [&](int32 C11, int32 C10, int32 C00, int32 C01)
		{
			T.Append({ C11, C10, C00, C11, C00, C01 });
		};
		for (const FIntPoint& C : Cells)
		{
			Quad(CornerAt(C.X + 1, C.Y + 1), CornerAt(C.X + 1, C.Y), CornerAt(C.X, C.Y), CornerAt(C.X, C.Y + 1));
		}
		if (bSea)
		{
			const float H = Half, F = 8.0f * Half;
			auto Rect = [&](float X0, float Y0, float X1, float Y1) { Quad(Add(X1, Y1), Add(X1, Y0), Add(X0, Y0), Add(X0, Y1)); };
			Rect(-F, -F, F, -H);
			Rect(-F, H, F, F);
			Rect(-F, -H, -H, H);
			Rect(H, -H, F, H);
		}
		WaterBodies->CreateMeshSection(Section, V, T, N, UV, TArray<FColor>(), Tan, false);
		WaterBodies->SetMaterial(Section, Material);
		++Section;
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE water bodies na vista: %d corpos em %.0f ms"), Section, 1000.0 * (FPlatformTime::Seconds() - T0));
	return Section > 0;
}

void AWorldForgeTerrain::ShowMap(TSharedPtr<const FWorldMap> InMap, TSharedPtr<FWorldLive> InLive)
{
	Map = InMap;
	Live = InLive;
	if (!Map.IsValid())
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	++SceneVersion;
	BuildLiveMaterials();
	BuildDataTextures();
	BuildGroundMesh();
	BuildProps();
	BuildLiveScatter();
	BuildVillage();
	BuildMonoliths();
	BuildMinimap();

	// água: plano de 1 m da engine esticado no mapa, na cota do nível d'água
	const FWorldMap& M = *Map;
	float MinH = TNumericLimits<float>::Max();
	for (float H : M.Height) MinH = FMath::Min(MinH, H);
	const bool bWater = M.WaterLevel > MinH;
	WaterPlane->SetVisibility(bWater);
	WaterPlane->SetRelativeLocation(FVector(0, 0, M.WaterLevel * 100.0));
	// borda de mar: a água segue até além da mesa (o mar até o horizonte)
	const bool bSea = M.Frame == EWorldFrame::Coast || M.Frame == EWorldFrame::Island;
	WaterPlane->SetRelativeScale3D(FVector(M.Half() * (bSea ? 8.0 : 2.0), M.Half() * (bSea ? 8.0 : 2.0), 1.0));
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
	WaterPlane->SetMaterial(0, LiveWaterMID ? static_cast<UMaterialInterface*>(LiveWaterMID) : static_cast<UMaterialInterface*>(WaterMID));
	// a água só onde existe água (a P1): com a vista ao vivo, as malhas por corpo no lugar do plano
	const bool bBodies = bWater && BuildWaterBodies(LiveWaterMID ? static_cast<UMaterialInterface*>(LiveWaterMID) : static_cast<UMaterialInterface*>(WaterMID));
	WaterPlane->SetVisibility(bWater && !bBodies);
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
	if (LiveGroundMID)
	{
		LiveGroundMID->SetTextureParameterValue(TEXT("OverlayMap"), OverlayMap);
		LiveGroundMID->SetScalarParameterValue(TEXT("OverlayOpacity"), Overlay ? 1.0f : 0.0f);
		LiveGroundMID->SetScalarParameterValue(TEXT("MapSize"), float(M.N) * M.Cell);
	}
}

bool AWorldForgeTerrain::HasLiveScatter() const
{
	return Live.IsValid() && Live->Scatter.Num() > 0;
}

void AWorldForgeTerrain::BuildLiveScatter()
{
	for (UInstancedStaticMeshComponent* C : LiveComponents)
	{
		if (C) C->DestroyComponent();
	}
	LiveComponents.Reset();
	LiveInfo.Reset();
	LiveCellIndex.Reset();
	if (!HasLiveScatter())
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	int32 Total = 0;
	for (int32 KI = 0; KI < Live->Scatter.Num(); ++KI)
	{
		const FWorldLive::FKind& K = Live->Scatter[KI];
		for (int32 MI = 0; MI < K.Meshes.Num(); ++MI)
		{
			const FWorldLive::FMesh& LM = K.Meshes[MI];
			TMap<FIntPoint, TArray<FTransform>> Cells;
			TMap<FIntPoint, TArray<FVector2f>> CellOrigins;
			for (int32 XI = 0; XI < LM.Xforms.Num(); ++XI)
			{
				const FTransform& X = LM.Xforms[XI];
				const FIntPoint Key = LiveCellOf(X.GetLocation());
				Cells.FindOrAdd(Key).Add(X);
				CellOrigins.FindOrAdd(Key).Add(LM.Origins.IsValidIndex(XI) ? LM.Origins[XI] : FVector2f(X.GetLocation().X / 100.0, X.GetLocation().Y / 100.0));
			}
			for (TPair<FIntPoint, TArray<FTransform>>& Cell : Cells)
			{
				const int32 CI = MakeLiveComponent(KI, MI, Cell.Key);
				if (CI == INDEX_NONE)
				{
					break;   // sem a malha
				}
				FillLiveComponent(CI, Cell.Value, MoveTemp(CellOrigins[Cell.Key]));
				Total += Cell.Value.Num();
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE live scatter: %d instâncias em %d componentes, %.0f ms"), Total, LiveComponents.Num(), 1000.0 * (FPlatformTime::Seconds() - T0));
}

FIntPoint AWorldForgeTerrain::LiveCellOf(const FVector& Cm)
{
	constexpr double CellCm = 12800.0;   // blocos de 128 m: a câmera descarta o bloco inteiro fora da vista
	return FIntPoint(FMath::FloorToInt(Cm.X / CellCm), FMath::FloorToInt(Cm.Y / CellCm));
}

int32 AWorldForgeTerrain::MakeLiveComponent(int32 KindIndex, int32 MeshIndex, FIntPoint Cell)
{
	const FWorldLive::FKind& K = Live->Scatter[KindIndex];
	const FWorldLive::FMesh& LM = K.Meshes[MeshIndex];
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *LM.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh)
	{
		return INDEX_NONE;
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	// o capim não projeta sombra (o mesmo do "qualidade real": o risco escuro comprido de cada tufo, 13/10)
	if (K.Kind == TEXT("Tuft") || K.Kind == TEXT("OasisGrass"))
	{
		C->SetCastShadow(false);
	}
	if (K.Cull > 0.0f)
	{
		// miúdo: some de longe e fica fora do campo de distância e da GI
		C->SetCullDistances(0, int32(K.Cull * 100.0f));
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
	}
	for (int32 I = 0; I < LM.Materials.Num(); ++I)
	{
		if (UMaterialInterface* Mat = LM.Materials[I].IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *LM.Materials[I], nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			C->SetMaterial(I, Mat);
		}
	}
	// a cota d'água em cada instância: o material das pedras tira a neve do que
	// fica debaixo d'água (o mesmo do "qualidade real", AddInstances)
	if (Map.IsValid() && Map->WaterLevel > -100.0f)
	{
		C->SetNumCustomDataFloats(1);
	}
	C->SetupAttachment(Root);
	C->RegisterComponent();
	const int32 CI = LiveComponents.Add(C);
	FLiveInfo& Info = LiveInfo.AddDefaulted_GetRef();
	Info.Kind = K.Kind;
	Info.Radius = FMath::Max(0.5f * FMath::Max(LM.Bounds.GetSize().X, LM.Bounds.GetSize().Y), 30.0f);
	Info.KindIndex = KindIndex;
	Info.MeshIndex = MeshIndex;
	Info.Cell = Cell;
	LiveCellIndex.Add(FIntVector(KindIndex * 64 + MeshIndex, Cell.X, Cell.Y), CI);
	return CI;
}

void AWorldForgeTerrain::FillLiveComponent(int32 CI, const TArray<FTransform>& Xforms, TArray<FVector2f>&& Origins)
{
	UInstancedStaticMeshComponent* C = LiveComponents[CI];
	C->ClearInstances();
	C->AddInstances(Xforms, false, false, false);
	if (C->NumCustomDataFloats > 0 && Map.IsValid())
	{
		for (int32 I = 0; I < C->GetInstanceCount(); ++I)
		{
			C->SetCustomDataValue(I, 0, Map->WaterLevel * 100.0f, false);
		}
		C->MarkRenderStateDirty();
	}
	LiveInfo[CI].Origins = MoveTemp(Origins);
}

void AWorldForgeTerrain::ReplaceLiveRegion(const FBox2f& Region, const TArray<FWorldLive::FKind>& New)
{
	// as peças novas do pedaço, por (tipo, malha, bloco de 128 m)
	struct FAdd { TArray<FTransform> X; TArray<FVector2f> O; };
	TMap<FIntVector, FAdd> Adds;
	for (int32 KI = 0; KI < New.Num(); ++KI)
	{
		for (int32 MI = 0; MI < New[KI].Meshes.Num(); ++MI)
		{
			const FWorldLive::FMesh& LM = New[KI].Meshes[MI];
			for (int32 XI = 0; XI < LM.Xforms.Num() && XI < LM.Origins.Num(); ++XI)
			{
				const FIntPoint Cell = LiveCellOf(LM.Xforms[XI].GetLocation());
				FAdd& A = Adds.FindOrAdd(FIntVector(KI * 64 + MI, Cell.X, Cell.Y));
				A.X.Add(LM.Xforms[XI]);
				A.O.Add(LM.Origins[XI]);
			}
		}
	}
	// os blocos com peça do pedaço (pelo ponto sorteado: a movida pode estar
	// noutro bloco) ou que ganham peça: refeitos com as de fora + as novas
	int32 Touched = 0, Before = 0, After = 0;
	for (int32 CI = 0; CI < LiveComponents.Num(); ++CI)
	{
		UInstancedStaticMeshComponent* C = LiveComponents[CI];
		if (!C) continue;
		FLiveInfo& Info = LiveInfo[CI];
		const FIntVector Key(Info.KindIndex * 64 + Info.MeshIndex, Info.Cell.X, Info.Cell.Y);
		bool bHit = false;
		for (const FVector2f& O : Info.Origins) { if (Region.IsInside(O)) { bHit = true; break; } }
		FAdd* A = Adds.Find(Key);
		if (!bHit && !A) continue;
		TArray<FTransform> Keep;
		TArray<FVector2f> KeepO;
		for (int32 I = 0; I < C->GetInstanceCount() && I < Info.Origins.Num(); ++I)
		{
			if (Region.IsInside(Info.Origins[I])) { ++Before; continue; }
			FTransform X;
			C->GetInstanceTransform(I, X, false);
			Keep.Add(X);
			KeepO.Add(Info.Origins[I]);
		}
		if (A)
		{
			After += A->X.Num();
			Keep.Append(A->X);
			KeepO.Append(A->O);
			Adds.Remove(Key);
		}
		FillLiveComponent(CI, Keep, MoveTemp(KeepO));
		++Touched;
	}
	// bloco que não tinha esse tipo e agora tem
	for (TPair<FIntVector, FAdd>& A : Adds)
	{
		const int32 CI = MakeLiveComponent(A.Key.X / 64, A.Key.X % 64, FIntPoint(A.Key.Y, A.Key.Z));
		if (CI == INDEX_NONE) continue;
		After += A.Value.X.Num();
		FillLiveComponent(CI, A.Value.X, MoveTemp(A.Value.O));
		++Touched;
	}
	++SceneVersion;
	UE_LOG(LogTemp, Display, TEXT("FORGE peças refeitas em (%.0f, %.0f)–(%.0f, %.0f): %d → %d, %d blocos"),
		Region.Min.X, Region.Min.Y, Region.Max.X, Region.Max.Y, Before, After, Touched);
}

namespace
{
	bool IsRockKind(const FString& K)
	{
		return K.StartsWith(TEXT("Rock")) || K == TEXT("Pebble") || K == TEXT("Deadwood");
	}
}

bool AWorldForgeTerrain::FindPiece(FVector2f P, float RadiusM, int32& BestC, int32& BestI, const TCHAR* KindPrefix) const
{
	// a peça com o melhor "acerto": distância ao centro dividida pelo tamanho
	// dela (o clique na copa de uma árvore pega a árvore, não o capim do chão)
	const FVector Pc(P.X * 100.0, P.Y * 100.0, 0.0);
	float Best = TNumericLimits<float>::Max();
	BestC = INDEX_NONE;
	BestI = INDEX_NONE;
	for (int32 CI = 0; CI < LiveComponents.Num(); ++CI)
	{
		const UInstancedStaticMeshComponent* C = LiveComponents[CI];
		if (!C || !LiveInfo.IsValidIndex(CI) || (KindPrefix && !LiveInfo[CI].Kind.StartsWith(KindPrefix)))
		{
			continue;
		}
		const FBox B = C->Bounds.GetBox();
		if (Pc.X < B.Min.X - RadiusM * 100.0 || Pc.X > B.Max.X + RadiusM * 100.0 || Pc.Y < B.Min.Y - RadiusM * 100.0 || Pc.Y > B.Max.Y + RadiusM * 100.0)
		{
			continue;
		}
		const float R = LiveInfo[CI].Radius;
		for (int32 I = 0; I < C->GetInstanceCount(); ++I)
		{
			FTransform X;
			C->GetInstanceTransform(I, X, false);
			const float D = FVector2f::Distance(FVector2f(X.GetLocation().X, X.GetLocation().Y), FVector2f(Pc.X, Pc.Y));
			const float Size = FMath::Max(R * float(X.GetMaximumAxisScale()), 50.0f);
			if (D > FMath::Max(RadiusM * 100.0f, Size)) continue;
			const float Score = D / Size;
			if (Score < Best)
			{
				Best = Score;
				BestC = CI;
				BestI = I;
			}
		}
	}
	return BestC != INDEX_NONE;
}

bool AWorldForgeTerrain::RemoveNearest(FVector2f P, float RadiusM, FRemoved& Out)
{
	int32 BestC, BestI;
	if (!FindPiece(P, RadiusM, BestC, BestI))
	{
		return false;
	}
	UInstancedStaticMeshComponent* C = LiveComponents[BestC];
	Out.Component = BestC;
	C->GetInstanceTransform(BestI, Out.Xform, false);
	Out.Kind = LiveInfo[BestC].Kind;
	Out.Origin = LiveInfo[BestC].Origins.IsValidIndex(BestI) ? LiveInfo[BestC].Origins[BestI] : P;
	C->RemoveInstance(BestI);   // o ISM tira na ordem (os índices de depois descem um)
	if (LiveInfo[BestC].Origins.IsValidIndex(BestI))
	{
		LiveInfo[BestC].Origins.RemoveAt(BestI);
	}
	++SceneVersion;   // a transparência das árvores varre de novo
	return true;
}

void AWorldForgeTerrain::UpdateGroundRegion(int32 RI0, int32 RJ0, int32 RI1, int32 RJ1)
{
	if (!Map.IsValid())
	{
		return;
	}
	const FWorldMap& M = *Map;
	const int32 Chunks = FMath::DivideAndRoundUp(M.N - 1, ChunkCells);
	auto Normal = [&M](int32 I, int32 J)
	{
		const int32 I0 = FMath::Max(I - 1, 0), I1 = FMath::Min(I + 1, M.N - 1);
		const int32 J0 = FMath::Max(J - 1, 0), J1 = FMath::Min(J + 1, M.N - 1);
		const float DX = (M.Height[M.Index(I1, J)] - M.Height[M.Index(I0, J)]) / ((I1 - I0) * M.Cell);
		const float DY = (M.Height[M.Index(I, J1)] - M.Height[M.Index(I, J0)]) / ((J1 - J0) * M.Cell);
		return FVector(-DX, -DY, 1.0).GetSafeNormal();
	};
	const int32 C0 = FMath::Clamp((RI0 - 1) / ChunkCells, 0, Chunks - 1), C1 = FMath::Clamp((RI1 + 1) / ChunkCells, 0, Chunks - 1);
	const int32 D0 = FMath::Clamp((RJ0 - 1) / ChunkCells, 0, Chunks - 1), D1 = FMath::Clamp((RJ1 + 1) / ChunkCells, 0, Chunks - 1);
	for (int32 CJ = D0; CJ <= D1; ++CJ)
	{
		for (int32 CI = C0; CI <= C1; ++CI)
		{
			const int32 I0 = CI * ChunkCells, J0 = CJ * ChunkCells;
			const int32 I1 = FMath::Min(I0 + ChunkCells, M.N - 1), J1 = FMath::Min(J0 + ChunkCells, M.N - 1);
			TArray<FVector> V;
			TArray<FVector> Nrm;
			TArray<FVector2D> UV;
			TArray<FProcMeshTangent> Tan;
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					const FVector2f P = M.CellPos(I, J);
					V.Add(FVector(P.X, P.Y, M.Height[M.Index(I, J)]) * 100.0);
					const FVector N = Normal(I, J);
					Nrm.Add(N);
					UV.Add(FVector2D(P.X, P.Y));
					Tan.Add(FProcMeshTangent(FVector(N.Z, 0.0, -N.X).GetSafeNormal(), false));
				}
			}
			Ground->UpdateMeshSection(CJ * Chunks + CI, V, Nrm, UV, {}, Tan);
		}
	}
}

void AWorldForgeTerrain::LiftPieces(FVector2f MinM, FVector2f MaxM, TFunctionRef<float(FVector2f)> DeltaAtM)
{
	const FBox Area(FVector(MinM.X * 100.0, MinM.Y * 100.0, -1e9), FVector(MaxM.X * 100.0, MaxM.Y * 100.0, 1e9));
	for (UInstancedStaticMeshComponent* C : LiveComponents)
	{
		if (!C || !C->Bounds.GetBox().Intersect(Area))
		{
			continue;
		}
		bool bChanged = false;
		for (int32 I = 0; I < C->GetInstanceCount(); ++I)
		{
			FTransform X;
			C->GetInstanceTransform(I, X, false);
			const FVector L = X.GetLocation();
			if (L.X < Area.Min.X || L.X > Area.Max.X || L.Y < Area.Min.Y || L.Y > Area.Max.Y) continue;
			const float D = DeltaAtM(FVector2f(L.X / 100.0, L.Y / 100.0));
			if (FMath::Abs(D) < 0.001f) continue;
			X.SetLocation(L + FVector(0, 0, D * 100.0));
			C->UpdateInstanceTransform(I, X, false, false, true);
			bChanged = true;
		}
		if (bChanged)
		{
			C->MarkRenderStateDirty();
		}
	}
}

void AWorldForgeTerrain::RepaintRegion(FVector2f MinM, FVector2f MaxM)
{
	if (!Live.IsValid() || !Live->Detail.IsValid() || Live->WeightRes < 2 || !WeightsATex || !WeightsBTex)
	{
		return;
	}
	FWorldLive& L = *Live;
	const int32 R = L.WeightRes;
	const float T = L.SizeM / R;
	const int32 I0 = FMath::Clamp(FMath::FloorToInt((MinM.X + 0.5f * L.SizeM) / T - 0.5f), 0, R - 1);
	const int32 I1 = FMath::Clamp(FMath::CeilToInt((MaxM.X + 0.5f * L.SizeM) / T - 0.5f), 0, R - 1);
	const int32 J0 = FMath::Clamp(FMath::FloorToInt((MinM.Y + 0.5f * L.SizeM) / T - 0.5f), 0, R - 1);
	const int32 J1 = FMath::Clamp(FMath::CeilToInt((MaxM.Y + 0.5f * L.SizeM) / T - 0.5f), 0, R - 1);
	if (I1 < I0 || J1 < J0)
	{
		return;
	}
	const FWorldMap& D = *L.Detail->GMap;
	auto B8 = [](float V) { return uint8(FMath::Clamp(V, 0.0f, 1.0f) * 255.0f + 0.5f); };
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			const FVector2f P((I + 0.5f) * T - 0.5f * L.SizeM, (J + 0.5f) * T - 0.5f * L.SizeM);
			float W[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
			if (D.IsInside(P)) L.Detail->DesertWeights(D, P, W);
			const int32 C = J * R + I;
			L.WeightsA[C] = FColor(B8(W[0]), B8(W[1]), B8(W[2]), B8(W[3]));
			L.WeightsB[C] = FColor(B8(W[4]), B8(W[5]), 0, 255);
			if (L.Detail->WCacheA.IsValidIndex(C))
			{
				L.Detail->WCacheA[C] = L.WeightsA[C];
				L.Detail->WCacheB[C] = L.WeightsB[C];
			}
		}
	}
	// só o retângulo vai pra placa (a textura inteira de 1 m tem 1–2 milhões de texels)
	FUpdateTextureRegion2D* RegionA = new FUpdateTextureRegion2D(I0, J0, I0, J0, I1 - I0 + 1, J1 - J0 + 1);
	FUpdateTextureRegion2D* RegionB = new FUpdateTextureRegion2D(*RegionA);
	WeightsATex->UpdateTextureRegions(0, 1, RegionA, R * 4, 4, reinterpret_cast<uint8*>(L.WeightsA.GetData()),
		[](uint8*, const FUpdateTextureRegion2D* Rg) { delete Rg; });
	WeightsBTex->UpdateTextureRegions(0, 1, RegionB, R * 4, 4, reinterpret_cast<uint8*>(L.WeightsB.GetData()),
		[](uint8*, const FUpdateTextureRegion2D* Rg) { delete Rg; });
}

bool AWorldForgeTerrain::FindLivePiece(const FString& KindPrefix, FVector& OutCm) const
{
	float Best = TNumericLimits<float>::Max();
	for (int32 CI = 0; CI < LiveComponents.Num(); ++CI)
	{
		const UInstancedStaticMeshComponent* C = LiveComponents[CI];
		if (!C || !LiveInfo.IsValidIndex(CI) || !LiveInfo[CI].Kind.StartsWith(KindPrefix)) continue;
		for (int32 I = 0; I < C->GetInstanceCount(); ++I)
		{
			FTransform X;
			C->GetInstanceTransform(I, X, true);
			const float D = X.GetLocation().Size2D();
			if (D < Best)
			{
				Best = D;
				OutCm = X.GetLocation();
			}
		}
	}
	return Best < TNumericLimits<float>::Max();
}

int32 AWorldForgeTerrain::LiveInstanceCount() const
{
	int32 N = 0;
	for (const UInstancedStaticMeshComponent* C : LiveComponents) N += C ? C->GetInstanceCount() : 0;
	return N;
}

bool AWorldForgeTerrain::PickPiece(FVector2f P, float RadiusM, FPicked& Out, const TCHAR* KindPrefix) const
{
	int32 BestC, BestI;
	if (!FindPiece(P, RadiusM, BestC, BestI, KindPrefix) || !LiveInfo[BestC].Origins.IsValidIndex(BestI))
	{
		return false;
	}
	const UInstancedStaticMeshComponent* C = LiveComponents[BestC];
	Out.Component = BestC;
	Out.Kind = LiveInfo[BestC].Kind;
	Out.Origin = LiveInfo[BestC].Origins[BestI];
	C->GetInstanceTransform(BestI, Out.Start, false);
	// o lugar da peça como o sorteio grava (FWorldDetail::PlaceMeshes): a pedra
	// pelo centro da caixa, o resto pelo pé
	FVector Pv = Out.Start.GetLocation();
	if (IsRockKind(Out.Kind) && C->GetStaticMesh())
	{
		Pv = Out.Start.TransformPosition(C->GetStaticMesh()->GetBounds().Origin);
	}
	Out.Pivot = FVector2f(Pv.X / 100.0, Pv.Y / 100.0);
	Out.RadiusCm = FMath::Max(LiveInfo[BestC].Radius * float(Out.Start.GetMaximumAxisScale()), 50.0f);
	return true;
}

void AWorldForgeTerrain::PlacePicked(const FPicked& Pk, FVector2f To, float YawDeg)
{
	if (!LiveComponents.IsValidIndex(Pk.Component) || !LiveComponents[Pk.Component] || !Map.IsValid())
	{
		return;
	}
	const int32 I = LiveInfo[Pk.Component].Origins.Find(Pk.Origin);
	if (I == INDEX_NONE)
	{
		return;
	}
	// gira em volta do lugar dela, leva até To e sobe/desce com o chão
	const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
	const FVector Pv(Pk.Pivot.X * 100.0, Pk.Pivot.Y * 100.0, Pk.Start.GetLocation().Z);
	const float Lift = Map->HeightAt(To) - Map->HeightAt(Pk.Pivot);
	FTransform X = Pk.Start;
	X.SetRotation(Spin * Pk.Start.GetRotation());
	X.SetLocation(Pv + Spin.RotateVector(Pk.Start.GetLocation() - Pv) + FVector((To.X - Pk.Pivot.X) * 100.0, (To.Y - Pk.Pivot.Y) * 100.0, Lift * 100.0));
	LiveComponents[Pk.Component]->UpdateInstanceTransform(I, X, false, true, true);
}

void AWorldForgeTerrain::SetPieceTransform(int32 Component, FVector2f Origin, const FTransform& X)
{
	if (!LiveComponents.IsValidIndex(Component) || !LiveComponents[Component])
	{
		return;
	}
	const int32 I = LiveInfo[Component].Origins.Find(Origin);
	if (I != INDEX_NONE)
	{
		LiveComponents[Component]->UpdateInstanceTransform(I, X, false, true, true);
		++SceneVersion;
	}
}

void AWorldForgeTerrain::Restore(const FRemoved& R)
{
	if (!LiveComponents.IsValidIndex(R.Component) || !LiveComponents[R.Component])
	{
		return;
	}
	UInstancedStaticMeshComponent* C = LiveComponents[R.Component];
	const int32 I = C->AddInstance(R.Xform, false);
	if (C->NumCustomDataFloats > 0 && Map.IsValid())
	{
		C->SetCustomDataValue(I, 0, Map->WaterLevel * 100.0f, true);
	}
	LiveInfo[R.Component].Origins.Add(R.Origin);
	++SceneVersion;
}

UMaterialInterface* AWorldForgeTerrain::GroundMaterial() const
{
	return LiveGroundMID ? static_cast<UMaterialInterface*>(LiveGroundMID) : static_cast<UMaterialInterface*>(GroundMID);
}

void AWorldForgeTerrain::BuildLiveMaterials()
{
	// o chão e a água de verdade: os materiais do "qualidade real" do bioma
	// (Tools/build_slice.py, SLICE=live) com as texturas deste mapa. Sem eles
	// (projeto sem montar), fica o rascunho
	LiveGroundMID = nullptr;
	LiveWaterMID = nullptr;
	if (!Live.IsValid())
	{
		return;
	}
	const FWorldMap& M = *Map;
	const TCHAR* Biome = M.Settings.Biome == EWorldBiome::Desert ? TEXT("desert") : (M.Settings.Biome == EWorldBiome::Winter ? TEXT("snow") : TEXT("forest"));
	UMaterialInterface* GroundBase = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Slice/Materials/M_LiveGround_%s.M_LiveGround_%s"), Biome, Biome));
	UMaterialInterface* WaterBase = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Slice/Materials/M_Ocean_%s.M_Ocean_%s"), Biome, Biome));
	if (!GroundBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("FORGE chão de verdade não encontrado (%s): rode forge.ps1 slice live"), Biome);
		return;
	}
	WeightsATex = MakeTexture(Live->WeightRes, Live->WeightRes, Live->WeightsA, false, TEXT("WeightsA"));
	WeightsBTex = MakeTexture(Live->WeightRes, Live->WeightRes, Live->WeightsB, false, TEXT("WeightsB"));
	WaterInfoTex = MakeTexture(Live->WaterRes, Live->WaterRes, Live->Water, false, TEXT("WaterInfo"));
	LiveGroundMID = UMaterialInstanceDynamic::Create(GroundBase, this);
	LiveGroundMID->SetTextureParameterValue(TEXT("WeightsA"), WeightsATex);
	LiveGroundMID->SetTextureParameterValue(TEXT("WeightsB"), WeightsBTex);
	LiveGroundMID->SetTextureParameterValue(TEXT("WaterInfo"), WaterInfoTex);
	LiveGroundMID->SetScalarParameterValue(TEXT("InfoSize"), Live->SizeM);
	// o vento do mapa (as ondulações da areia v2): o material do chão é um só por bioma, o vento é por mapa
	// (a coleção do vento do jogo, /Game/Materials/MPC_Wind: Wind = (x, y pra onde sopra, m/s, rajada))
	if (UMaterialParameterCollection* Wind = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Wind.MPC_Wind"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		const float Yaw = FMath::DegreesToRadians(Map->WindYaw);
		UKismetMaterialLibrary::SetVectorParameterValue(this, Wind, TEXT("Wind"), FLinearColor(FMath::Cos(Yaw), FMath::Sin(Yaw), Map->WindSpeed, 0.3f));
	}
	LiveGroundMID->SetScalarParameterValue(TEXT("WaterOn"), 1.0f);
	if (WaterBase)
	{
		LiveWaterMID = UMaterialInstanceDynamic::Create(WaterBase, this);
		LiveWaterMID->SetTextureParameterValue(TEXT("WaterInfo"), WaterInfoTex);
		LiveWaterMID->SetScalarParameterValue(TEXT("InfoSize"), Live->SizeM);
		LiveWaterMID->SetScalarParameterValue(TEXT("WaterOn"), 1.0f);
		LiveWaterMID->SetScalarParameterValue(TEXT("Surf"), Live->bSea ? 1.0f : 0.25f);
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
			if (UMaterialInterface* GM = GroundMaterial())
			{
				Ground->SetMaterial(Section, GM);
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
	if (UMaterialInterface* GM = GroundMaterial())
	{
		Ground->SetMaterial(Section, GM);
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
	const bool bLive = HasLiveScatter();
	for (const FWorldProp& P : M.Props)
	{
		if (bLive) break;   // as árvores e pedras de verdade vêm do espalhamento ao vivo
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
		{
			// os materiais que as fatias deixaram: com neve no inverno (MI_..._Snow) e,
			// nos outros, com a árvore transparente perto do cursor (MI_..._WF)
			const TArray<FStaticMaterial>& Mats = Pair.Key->GetStaticMaterials();
			for (int32 I = 0; I < Mats.Num(); ++I)
			{
				if (!Mats[I].MaterialInterface) continue;
				const FString Name = Mats[I].MaterialInterface->GetName() + (bWinter ? TEXT("_Snow") : TEXT("_WF"));
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

void AWorldForgeTerrain::BuildMonoliths()
{
	if (!Map.IsValid() || Map->Monoliths.Num() == 0)
	{
		return;
	}
	TMap<UStaticMesh*, TArray<FTransform>> ByMesh;
	int32 Missing = 0;
	for (const FWorldMonolith& Mo : Map->Monoliths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Slice/Monoliths/%s/%s.%s"), *Mo.Mesh, *Mo.Mesh, *Mo.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) { ++Missing; continue; }
		ByMesh.FindOrAdd(Mesh).Add(FTransform(FRotator(0.0f, Mo.Yaw, 0.0f), FVector(Mo.Pos.X * 100.0, Mo.Pos.Y * 100.0, Mo.Z * 100.0), FVector(Mo.Scale)));
	}
	for (const TPair<UStaticMesh*, TArray<FTransform>>& Pair : ByMesh)
	{
		// o material da malha (M_MonolithRock)
		UHierarchicalInstancedStaticMeshComponent* C = MakeInstances(*FString::Printf(TEXT("Monoliths_%d"), PropComponents.Num()), Pair.Key, nullptr, true);
		C->AddInstances(Pair.Value, false, true, false);
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE monólitos na vista: %d (%d malhas), %d sem malha (rodar o build_slice com o kit)"), Map->Monoliths.Num() - Missing, ByMesh.Num(), Missing);
}

void AWorldForgeTerrain::BuildVillage()
{
	if (!Live.IsValid() || !Live->Detail.IsValid() || Live->Detail->GVillage.Num() == 0)
	{
		return;
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* PropMat = LoadMaterial(PropMaterialPath);
	if (!Cube || !PropMat)
	{
		return;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PropMat, this);
	MID->SetScalarParameterValue(TEXT("Snow"), 0.0f);
	// a cor de cada tipo (provisória: o reboco de barro claro das artes, muro um
	// pouco mais escuro, portão de madeira, carcaça preta, picape de cor gasta)
	auto TintOf = [](const FString& T) -> FLinearColor
	{
		// (o reboco de barro da arte: bege areia, mais escuro que a caixa branca da 1ª
		// foto, que estourava no sol; a casa um pouco mais clara que o chão)
		if (T == TEXT("civ_house_2story")) return FLinearColor(0.8f, 0.66f, 0.5f);
		if (T.StartsWith(TEXT("civ_house"))) return FLinearColor(0.74f, 0.6f, 0.45f);
		if (T == TEXT("civ_wall")) return FLinearColor(0.64f, 0.51f, 0.37f);
		if (T == TEXT("civ_wall_gate")) return FLinearColor(0.55f, 0.42f, 0.3f);
		if (T == TEXT("civ_car_burnt")) return FLinearColor(0.16f, 0.15f, 0.14f);
		if (T == TEXT("civ_pickup")) return FLinearColor(0.95f, 0.88f, 0.72f);
		if (T == TEXT("civ_pole")) return FLinearColor(0.32f, 0.27f, 0.22f);
		if (T == TEXT("civ_sandbags")) return FLinearColor(1.0f, 0.88f, 0.62f);
		if (T == TEXT("civ_barrels")) return FLinearColor(0.38f, 0.48f, 0.58f);
		if (T == TEXT("civ_ammo")) return FLinearColor(0.42f, 0.48f, 0.3f);
		return FLinearColor(0.9f, 0.68f, 0.42f);   // caixotes
	};
	// o kit civil do Look (Tools/import_village.py, o mesmo caminho e material do jogo): a
	// malha no centro da peça, no piso, girada 90 × Rot (frente em +Y no 0); sem a malha, a caixa
	TMap<FString, UStaticMesh*> Meshes;
	auto MeshOf = [&Meshes](const FString& Type) -> UStaticMesh*
	{
		if (UStaticMesh** Found = Meshes.Find(Type)) return *Found;
		const FString Name = TEXT("SM_") + Type;
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s/StaticMeshes/%s.%s"), *Name, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Village/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		Meshes.Add(Type, Mesh);
		return Mesh;
	};
	TMap<UStaticMesh*, TArray<FTransform>> Kit;
	int32 KitPieces = 0;
	TArray<FTransform> Xs;
	TArray<FLinearColor> Tints;
	for (const FWorldDetail::FVillagePiece& V : Live->Detail->GVillage)
	{
		if (UStaticMesh* Mesh = MeshOf(V.Type))
		{
			// (o giro livre das miudezas, carcaças e postes da vila v3; o resto, 90 × Rot)
			const float Yaw = V.HasYaw() ? V.YawDeg : 90.0f * V.Rot;
			Kit.FindOrAdd(Mesh).Add(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(V.Center.X * 100.0, V.Center.Y * 100.0, V.Floor * 100.0)));
			++KitPieces;
			continue;
		}
		// a caixa: a pegada da malha no mundo (já girada) e a altura dela, assentada
		// no piso; um pouco enterrada (o piso é o platô, a borda do terreno se mexe)
		const FVector Size(V.Size.X, V.Size.Y, V.Height + 0.3f);
		const FVector Loc(V.Center.X * 100.0, V.Center.Y * 100.0, (V.Floor + 0.5f * V.Height - 0.15f) * 100.0);
		Xs.Add(FTransform(FRotator::ZeroRotator, Loc, Size));
		// variação de tom por peça (cada casa um pouco diferente, como as artes)
		const float Var = 0.9f + 0.2f * float(GetTypeHash(FIntPoint(V.Cell0.X, V.Cell0.Y)) % 100u) / 100.0f;
		Tints.Add(TintOf(V.Type) * (V.Type.StartsWith(TEXT("civ_house")) ? Var : 1.0f));
	}
	for (const TPair<UStaticMesh*, TArray<FTransform>>& Pair : Kit)
	{
		// sem material por cima: os da malha (M_Structure, as MI_Civ*)
		UHierarchicalInstancedStaticMeshComponent* C = MakeInstances(*FString::Printf(TEXT("Village_%d"), PropComponents.Num()), Pair.Key, nullptr, true);
		C->AddInstances(Pair.Value, false, true, false);
	}
	if (Xs.Num())
	{
		UHierarchicalInstancedStaticMeshComponent* C = MakeInstances(*FString::Printf(TEXT("Village_%d"), PropComponents.Num()), Cube, MID, true);
		C->AddInstances(Xs, false, true, false);
		for (int32 K = 0; K < Xs.Num(); ++K)
		{
			C->SetCustomDataValue(K, 0, Tints[K].R);
			C->SetCustomDataValue(K, 1, Tints[K].G);
			C->SetCustomDataValue(K, 2, Tints[K].B);
		}
		C->MarkRenderStateDirty();
	}
	// os fios dos postes: a catenária (≈ a parábola da flecha) em 10 pedaços retos por vão, caixas finas
	// de 4,5 cm (o revisor: a 80 m o pixel tem ~4 cm, o fio de 2 cm de verdade pisca), escuras e sem sombra
	TArray<FTransform> Wires;
	for (const FWorldDetail::FVillageWire& W : Live->Detail->GVillageWires)
	{
		constexpr int32 Seg = 10;
		FVector Prev = W.A;
		for (int32 K = 1; K <= Seg; ++K)
		{
			const float T = float(K) / Seg;
			const FVector Q = FMath::Lerp(W.A, W.B, T) - FVector(0.0f, 0.0f, 4.0f * W.Sag * T * (1.0f - T));
			const FVector D = Q - Prev;
			Wires.Add(FTransform(D.Rotation(), 50.0 * (Prev + Q), FVector(D.Size(), 0.045f, 0.045f)));
			Prev = Q;
		}
	}
	if (Wires.Num())
	{
		UHierarchicalInstancedStaticMeshComponent* C = MakeInstances(*FString::Printf(TEXT("Village_%d"), PropComponents.Num()), Cube, MID, false);
		C->AddInstances(Wires, false, true, false);
		for (int32 K = 0; K < Wires.Num(); ++K)
		{
			C->SetCustomDataValue(K, 0, 0.035f);
			C->SetCustomDataValue(K, 1, 0.032f);
			C->SetCustomDataValue(K, 2, 0.03f);
		}
		C->MarkRenderStateDirty();
	}
	UE_LOG(LogTemp, Display, TEXT("FORGE vila na vista: %d peças do kit do Look (%d malhas), %d caixas provisórias, %d fios"), KitPieces, Kit.Num(), Xs.Num(), Live->Detail->GVillageWires.Num());
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
