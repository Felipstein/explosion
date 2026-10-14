#include "RTSDebris.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// as peças (Tools/blender_provisional.py, debris): o nome e se faz sombra (as lascas pequenas não fazem)
	struct FPiece
	{
		const TCHAR* Name;
		bool bShadow;
	};
	const FPiece Pieces[] = {
		{ TEXT("SM_Debris_Wheel"), true },
		{ TEXT("SM_Debris_Track"), true },
		{ TEXT("SM_Debris_Plate"), true },
		{ TEXT("SM_Debris_Hatch"), true },
		{ TEXT("SM_Debris_Box"), true },
		{ TEXT("SM_Debris_Chunk1"), false },
		{ TEXT("SM_Debris_Chunk2"), true },
		{ TEXT("SM_Debris_Chunk3"), false },
	};
	constexpr double CellCm = 6400.0;
	// o teto: ~250 mortes; passou disso, as novas ficam sem destroço (o queimado e a carcaça ficam)
	constexpr int32 MaxPieces = 1500;
	// o casco do tanque USA (meio comprimento e meia largura, cm, com folga): nenhuma peça cai em cima dele
	constexpr double HullHalfLengthCm = 430.0;
	constexpr double HullHalfWidthCm = 215.0;
	// as pedras das crateras (as escaneadas da maquete do Look, com o material escuro da cratera dela): até 14 graúdas
	// (de 20 cm pra cima) e ~150 miúdas por cratera; o teto à parte, porque a batalha cava muito mais cratera que mata tanque
	const TCHAR* const StoneMeshes[] = { TEXT("a"), TEXT("b"), TEXT("c"), TEXT("d"), TEXT("e") };
	constexpr int32 MaxStones = 8000;
	constexpr int32 MaxSmallStones = 60000;
	// as miúdas (8–20 cm) só no zoom mais perto (a câmera fica a 67–118 m do foco): a 67 m o meio da tela fica a ~67 m e a
	// borda de baixo a ~55 m; no zoom padrão (95 m) já passam de 80 m e somem
	constexpr float SmallStoneStartCullCm = 7000.0f;
	constexpr float SmallStoneEndCullCm = 8000.0f;
}

ARTSDebrisField::ARTSDebrisField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ARTSDebrisField::LoadMeshes()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	for (const FPiece& P : Pieces)
	{
		// (o importador de GLB põe as malhas numa subpasta com o nome do arquivo)
		const FString Path = FString::Printf(TEXT("/Game/Structures/debris/SM_Debris/StaticMeshes/%s.%s"), P.Name, P.Name);
		Meshes.Add(LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
		MeshMaterials.Add(nullptr);
		MeshShadow.Add(P.bShadow);
	}
	StoneFirst = Meshes.Num();
	UMaterialInterface* StoneMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LookLab/Materials/MI_LL_namaqualand_stones_01_desert_crater.MI_LL_namaqualand_stones_01_desert_crater"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	// o tom na luz do jogo, do Config/RTS/Ground/stones.json quando existe (14/10: o Look ajusta sem compilar; a proposta
	// dele é o tom da areia do mapa, ~0,65× o valor, pouca dessaturação e a rugosidade 0,7: na contraluz a pedra escura virava
	// pontinho preto, e é a quina acesa que diz "pedra"); sem o arquivo, o tom de 13/10
	FLinearColor Tint(0.45f, 0.38f, 0.30f);
	float Desat = 0.45f;
	float Roughness = -1.0f;
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Ground"), TEXT("stones.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* T = nullptr;
			if (J->TryGetArrayField(TEXT("tint"), T) && T->Num() >= 3)
			{
				Tint = FLinearColor(float((*T)[0]->AsNumber()), float((*T)[1]->AsNumber()), float((*T)[2]->AsNumber()));
			}
			double V = 0.0;
			if (J->TryGetNumberField(TEXT("desat"), V)) { Desat = float(V); }
			if (J->TryGetNumberField(TEXT("roughness"), V)) { Roughness = float(V); }
			if (J->TryGetNumberField(TEXT("min_cm"), V)) { MinStoneCm = float(V); }
		}
	}
	if (StoneMat)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(StoneMat, this);
		MID->SetVectorParameterValue(TEXT("Tint"), Tint);
		MID->SetScalarParameterValue(TEXT("Desat"), Desat);
		if (Roughness >= 0.0f)
		{
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			MID->SetScalarParameterValue(TEXT("RoughnessMul"), Roughness);
		}
		StoneMat = MID;
	}
	for (const TCHAR* V : StoneMeshes)
	{
		const FString Name = FString::Printf(TEXT("namaqualand_stones_01_%s_LOD0"), V);
		Meshes.Add(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Slice/Models/namaqualand_stones_01/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
		MeshMaterials.Add(StoneMat);
		MeshShadow.Add(true);
	}
	int32 Found = 0;
	for (const UStaticMesh* M : Meshes)
	{
		Found += M ? 1 : 0;
		const FVector E = M ? M->GetBounds().BoxExtent : FVector(10.0);
		// (a maior dimensão: escalada pela largura do XY, a pedra estreita e alta ficava em pé como laje, vila_v6)
		MeshWidthCm.Add(float(FMath::Max(2.0 * FMath::Max3(E.X, E.Y, E.Z), 1.0)));
		MeshExtent.Add(E);
		MeshOrigin.Add(M ? M->GetBounds().Origin : FVector::ZeroVector);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS destroços: %d de %d peças (game.ps1 debris e as pedras da cratera)%s"), Found, Meshes.Num(),
		StoneMat ? TEXT("") : TEXT("; sem o material escuro da cratera do Look: a pedra fica com o dela"));
}

FTransform ARTSDebrisField::StoneTransform(int32 K, const FVector& S, FRandomStream& R, TFunctionRef<float(const FVector2D&)> GroundZ) const
{
	// deitada (13/10, o Look: em pé, a pedra lia laje e dente): o eixo mais curto da malha em pé, a face maior pra baixo
	// (uma das duas, no sorteio), e a altura em 0,5–0,7 da maior dimensão quando a malha for mais alta que isso; a escala
	// pela maior dimensão, inclinada ±8°, um quarto da altura enterrado
	const FVector E = MeshExtent[K];
	const int32 Short = E.X <= E.Y && E.X <= E.Z ? 0 : (E.Y <= E.Z ? 1 : 2);
	const double Long = FMath::Max(E.GetMax(), 0.1);
	const double Min = FMath::Max(E[Short], 0.1);
	const float Sc = float(S.Z) / MeshWidthCm[K] * R.FRandRange(0.85f, 1.15f);
	FVector Scale(Sc);
	Scale[Short] *= FMath::Min(1.0, double(R.FRandRange(0.5f, 0.7f)) * Long / Min);
	FQuat Q = Short == 0 ? FQuat(FVector::YAxisVector, HALF_PI) : (Short == 1 ? FQuat(FVector::XAxisVector, HALF_PI) : FQuat::Identity);
	if (R.FRand() < 0.5f)
	{
		Q = FQuat(FVector::XAxisVector, PI) * Q;
	}
	Q = FRotator(R.FRandRange(-8.0f, 8.0f), R.FRandRange(0.0f, 360.0f), R.FRandRange(-8.0f, 8.0f)).Quaternion() * Q;
	const double Height = 2.0 * Min * Scale[Short];
	const FVector2D P(S.X, S.Y);
	const FVector Center(P.X, P.Y, GroundZ(P) + 0.25 * Height);
	return FTransform(Q, Center - Q.RotateVector(Scale * MeshOrigin[K]), Scale);
}

void ARTSDebrisField::AddStones(const TArray<FVector>& Large, const TArray<FVector>& Small, uint32 Seed, TFunctionRef<float(const FVector2D&)> GroundZ)
{
	LoadMeshes();
	if (!GetWorld())
	{
		return;
	}
	FRandomStream R(static_cast<int32>(Seed));
	const double Now = GetWorld()->GetTimeSeconds();
	for (const bool bSmall : { false, true })
	{
		const TArray<FVector>& Stones = bSmall ? Small : Large;
		int32& Count = bSmall ? SmallStonesPlaced : StonesPlaced;
		if (Count + Stones.Num() > (bSmall ? MaxSmallStones : MaxStones))
		{
			continue;
		}
		for (const FVector& S : Stones)
		{
			const int32 K = StoneFirst + R.RandRange(0, UE_ARRAY_COUNT(StoneMeshes) - 1);
			if (float(S.Z) < MinStoneCm)
			{
				continue;   // menor que ~9 cm: a 67 m o pixel tem 8–15 cm e a pedrinha cintila (fica no material da cratera)
			}
			if (!Meshes.IsValidIndex(K) || !Meshes[K])
			{
				continue;
			}
			FPending Q;
			Q.AtSeconds = Now + R.FRandRange(0.3f, 0.9f);   // quando cai, depois do estouro
			Q.Mesh = K;
			Q.Transform = StoneTransform(K, S, R, GroundZ);
			Q.bSmall = bSmall;
			Pending.Add(Q);
			++Count;
		}
	}
	SetActorTickEnabled(!Pending.IsEmpty());
}

void ARTSDebrisField::AddWreck(const FVector2D& AtCm, float HullYawDeg, const FVector2D& FromCm, TFunctionRef<float(const FVector2D&)> GroundZ)
{
	LoadMeshes();
	if (Placed + Pending.Num() >= MaxPieces || !GetWorld())
	{
		return;
	}
	const uint32 Seed = HashCombine(GetTypeHash(FMath::RoundToInt(AtCm.X)), GetTypeHash(FMath::RoundToInt(AtCm.Y)) ^ 0x2545F491u);
	FRandomStream R(static_cast<int32>(Seed));
	FVector2D Down = AtCm - FromCm;
	Down = Down.SizeSquared() > 100.0 * 100.0 ? Down.GetSafeNormal() : FVector2D(1.0, 0.0).GetRotated(R.FRandRange(0.0f, 360.0f));
	const float BaseDeg = FMath::RadiansToDegrees(FMath::Atan2(Down.Y, Down.X));
	const double Yaw = FMath::DegreesToRadians(double(HullYawDeg));
	const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Side(-Fwd.Y, Fwd.X);
	// a lista: sempre uma roda e uma chapa; as outras peças por sorteio; as lascas completam 5–8
	TArray<int32> Kinds = { 0, 2 };
	if (R.FRand() < 0.5f) { Kinds.Add(0); }
	if (R.FRand() < 0.7f) { Kinds.Add(1); }
	if (R.FRand() < 0.4f) { Kinds.Add(3); }
	if (R.FRand() < 0.4f) { Kinds.Add(4); }
	const int32 N = 5 + R.RandRange(0, 3);
	while (Kinds.Num() < N)
	{
		Kinds.Add(5 + R.RandRange(0, 2));
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (const int32 K : Kinds)
	{
		if (!Meshes.IsValidIndex(K) || !Meshes[K])
		{
			continue;
		}
		// 70% pra longe de quem atirou (±75°), o resto pra qualquer lado; fora do casco
		const float Deg = R.FRand() < 0.7f ? BaseDeg + R.FRandRange(-75.0f, 75.0f) : R.FRandRange(-180.0f, 180.0f);
		const FVector2D Dir = FVector2D(1.0, 0.0).GetRotated(Deg);
		double Dist = R.FRandRange(450.0f, 900.0f);
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const FVector2D L = Dir * Dist;
			if (FMath::Abs(FVector2D::DotProduct(L, Fwd)) > HullHalfLengthCm || FMath::Abs(FVector2D::DotProduct(L, Side)) > HullHalfWidthCm)
			{
				break;
			}
			Dist += 80.0;
		}
		const FVector2D P = AtCm + Dir * Dist;
		// meio enterrada (2–6 cm), torta de leve, girada à toa
		const FVector Loc(P.X, P.Y, GroundZ(P) - R.FRandRange(2.0f, 6.0f));
		const FRotator Rot(R.FRandRange(-7.0f, 7.0f), R.FRandRange(0.0f, 360.0f), R.FRandRange(-7.0f, 7.0f));
		FPending Q;
		Q.AtSeconds = Now + R.FRandRange(0.9f, 1.6f);   // quando pousa
		Q.Mesh = K;
		Q.Transform = FTransform(Rot, Loc, FVector(R.FRandRange(0.9f, 1.15f)));
		Pending.Add(Q);
	}
	SetActorTickEnabled(!Pending.IsEmpty());
}

void ARTSDebrisField::Place(const FPending& P)
{
	const FVector L = P.Transform.GetLocation();
	const FIntVector Key(FMath::FloorToInt(L.X / CellCm), FMath::FloorToInt(L.Y / CellCm), P.Mesh + (P.bSmall ? 1000 : 0));
	int32* Index = CellComponents.Find(Key);
	if (!Index)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Static);
		C->SetStaticMesh(Meshes[P.Mesh]);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(MeshShadow[P.Mesh] && !P.bSmall);
		if (P.bSmall)
		{
			C->SetCullDistances(int32(SmallStoneStartCullCm), int32(SmallStoneEndCullCm));
		}
		C->SetReceivesDecals(false);   // a memória do chão não pinta por cima
		if (UMaterialInterface* Mat = MeshMaterials[P.Mesh])
		{
			for (int32 S = 0; S < Meshes[P.Mesh]->GetStaticMaterials().Num(); ++S)
			{
				C->SetMaterial(S, Mat);
			}
		}
		// queimado como a carcaça (o M_Structure, dado 6 = 2: fuligem e tinta queimada, sem brasa): a chapa de
		// tinta clara lia como quadradinho branco no chão (vila_cr2, 13/10)
		C->SetCustomPrimitiveDataFloat(6, 2.0f);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		Index = &CellComponents.Add(Key, Components.Add(C));
	}
	Batch.FindOrAdd(*Index).Add(P.Transform);
	++Placed;
}

void ARTSDebrisField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 I = Pending.Num() - 1; I >= 0; --I)
	{
		if (Pending[I].AtSeconds <= Now)
		{
			Place(Pending[I]);
			Pending.RemoveAtSwap(I);
		}
	}
	// todas as que pousaram neste quadro de uma vez por componente (14/10, o Look: uma a uma, cada entrada invalidava o
	// cache da sombra virtual da célula de novo)
	for (TPair<int32, TArray<FTransform>>& It : Batch)
	{
		Components[It.Key]->AddInstances(It.Value, false, true);
	}
	Batch.Reset();
	if (Pending.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}
