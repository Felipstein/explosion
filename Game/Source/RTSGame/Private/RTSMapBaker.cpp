#include "RTSMapBaker.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "LandscapeProxy.h"
#include "RTSGameData.h"
#include "RTSConvert.h"
#include "Dom/JsonObject.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr float NoGround = -100000.0f;

	bool IsObstacleMesh(const UStaticMesh* Mesh, const TArray<FString>& Keywords)
	{
		if (!Mesh)
		{
			return false;
		}
		const FString Path = Mesh->GetPathName();
		for (const FString& K : Keywords)
		{
			if (Path.Contains(K))
			{
				return true;
			}
		}
		return false;
	}
}

float FRTSBakedMap::GroundZCm(const FVector2D& Cm) const
{
	if (Samples <= 1)
	{
		return 0.0f;
	}
	const double GX = FMath::Clamp((Cm.X / 100.0 - OriginM.X) / SampleM, 0.0, double(Samples - 1) - 1e-3);
	const double GY = FMath::Clamp((Cm.Y / 100.0 - OriginM.Y) / SampleM, 0.0, double(Samples - 1) - 1e-3);
	const int32 X = int32(GX);
	const int32 Y = int32(GY);
	const float FX = float(GX - X);
	const float FY = float(GY - Y);
	auto H = [this](int32 I, int32 J) { return HeightsM[J * Samples + I]; };
	const float Top = FMath::Lerp(H(X, Y), H(X + 1, Y), FX);
	const float Bottom = FMath::Lerp(H(X, Y + 1), H(X + 1, Y + 1), FX);
	return FMath::Lerp(Top, Bottom, FY) * 100.0f;
}

FVector4f FRTSBakedMap::SurfaceAt(const FVector2D& Cm) const
{
	if (SurfaceRes <= 1 || Surface.Num() != SurfaceRes * SurfaceRes)
	{
		return FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	}
	// o centro do pixel (i, j) em min + (i + 0,5, j + 0,5)·cell
	const double GX = FMath::Clamp((Cm.X / 100.0 - SurfaceMinM.X) / SurfaceCellM - 0.5, 0.0, double(SurfaceRes - 1) - 1e-3);
	const double GY = FMath::Clamp((Cm.Y / 100.0 - SurfaceMinM.Y) / SurfaceCellM - 0.5, 0.0, double(SurfaceRes - 1) - 1e-3);
	const int32 X = int32(GX);
	const int32 Y = int32(GY);
	const float FX = float(GX - X);
	const float FY = float(GY - Y);
	auto C = [this](int32 I, int32 J)
	{
		const FColor& P = Surface[J * SurfaceRes + I];
		return FVector4f(P.R, P.G, P.B, bSurfaceWet ? P.A : 0) / 255.0f;
	};
	const FVector4f Top = FMath::Lerp(C(X, Y), C(X + 1, Y), FX);
	const FVector4f Bottom = FMath::Lerp(C(X, Y + 1), C(X + 1, Y + 1), FX);
	return FMath::Lerp(Top, Bottom, FY);
}

namespace
{
	// o surface.png do WorldForge e o .json ao lado (min_m, cell_m, res; a linha 0 no −Y)
	void LoadSurface(const FRTSMapInfo& Info, FRTSBakedMap& Out)
	{
		if (Info.SurfaceFile.IsEmpty())
		{
			return;
		}
		const FString Png = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), Info.SurfaceFile);
		TArray<uint8> Bytes;
		FString Text;
		TSharedPtr<FJsonObject> Meta;
		if (!FFileHelper::LoadFileToArray(Bytes, *Png) || !FFileHelper::LoadFileToString(Text, *FPaths::ChangeExtension(Png, TEXT("json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Meta) || !Meta.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS chão: não li o tipo de chão %s (e o .json; game.ps1 worlds copia do WorldForge): tudo vira areia"), *Png);
			return;
		}
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Image = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Raw;
		if (!Image.IsValid() || !Image->SetCompressed(Bytes.GetData(), Bytes.Num()) || !Image->GetRaw(ERGBFormat::RGBA, 8, Raw)
			|| Image->GetWidth() != Image->GetHeight() || Raw.Num() != Image->GetWidth() * Image->GetHeight() * 4)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS chão: o tipo de chão %s não é um PNG RGBA quadrado: tudo vira areia"), *Png);
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
		double Cell = 1.0;
		FString Row0;
		Meta->TryGetNumberField(TEXT("cell_m"), Cell);
		Meta->TryGetStringField(TEXT("row0"), Row0);
		if (!Meta->TryGetArrayField(TEXT("min_m"), Min) || Min->Num() < 2 || Cell <= 0.0 || (!Row0.IsEmpty() && Row0 != TEXT("min_y")))
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS chão: o %s não tem min_m/cell_m (ou a linha 0 não é o −Y): tudo vira areia"), *FPaths::GetCleanFilename(FPaths::ChangeExtension(Png, TEXT("json"))));
			return;
		}
		const TSharedPtr<FJsonObject>* Channels = nullptr;
		Out.bSurfaceWet = Meta->TryGetObjectField(TEXT("channels"), Channels) && (*Channels)->HasField(TEXT("a"));
		Out.SurfaceRes = int32(Image->GetWidth());
		Out.SurfaceCellM = float(Cell);
		Out.SurfaceMinM = FVector2D((*Min)[0]->AsNumber(), (*Min)[1]->AsNumber());
		Out.Surface.SetNumUninitialized(Out.SurfaceRes * Out.SurfaceRes);
		int32 Paved = 0;
		for (int32 I = 0; I < Out.Surface.Num(); ++I)
		{
			Out.Surface[I] = FColor(Raw[I * 4], Raw[I * 4 + 1], Raw[I * 4 + 2], Raw[I * 4 + 3]);
			Paved += Raw[I * 4] >= 128 ? 1 : 0;
		}
		UE_LOG(LogTemp, Log, TEXT("RTS chão: tipo de chão %s, %d×%d de %.1f m, %.1f%% asfalto"), *Info.SurfaceFile, Out.SurfaceRes, Out.SurfaceRes, Cell,
			100.0 * Paved / FMath::Max(Out.Surface.Num(), 1));
	}
}

namespace
{
	// a grade da simulação do WorldForge (grid.png e grid.json, 13/10): a água por célula e os corpos, com o hash dele
	// conferido (FNV-1a 64 dos pixels RGBA, linha 0 primeiro, e uma linha "id,kind,level_mm,flow_x,flow_y\n" por corpo)
	bool LoadWaterGrid(const FRTSMapInfo& Info, int32 Cells, const FVector2D& OriginM, Sim::SimMap& M)
	{
		if (Info.GridFile.IsEmpty())
		{
			return false;
		}
		const FString Png = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), Info.GridFile);
		TArray<uint8> Bytes;
		FString Text;
		TSharedPtr<FJsonObject> Meta;
		if (!FFileHelper::LoadFileToArray(Bytes, *Png) || !FFileHelper::LoadFileToString(Text, *FPaths::ChangeExtension(Png, TEXT("json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Meta) || !Meta.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS água: não li a grade %s (e o .json; game.ps1 worlds copia do WorldForge): a água vem da altura"), *Png);
			return false;
		}
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Image = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Raw;
		if (!Image.IsValid() || !Image->SetCompressed(Bytes.GetData(), Bytes.Num()) || !Image->GetRaw(ERGBFormat::RGBA, 8, Raw)
			|| Image->GetWidth() != Cells || Image->GetHeight() != Cells || Raw.Num() != Cells * Cells * 4)
		{
			UE_LOG(LogTemp, Error, TEXT("RTS água: a grade %s não é um PNG RGBA de %d×%d (as células do mapa): a água vem da altura"), *Png, Cells, Cells);
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
		double Cell = 0.0;
		FString Row0;
		FString Want;
		Meta->TryGetNumberField(TEXT("cell_m"), Cell);
		Meta->TryGetStringField(TEXT("row0"), Row0);
		Meta->TryGetStringField(TEXT("hash"), Want);
		if (!Meta->TryGetArrayField(TEXT("min_m"), Min) || Min->Num() < 2 || !FMath::IsNearlyEqual(Cell, double(Info.CellM)) || Row0 != TEXT("min_y")
			|| !FMath::IsNearlyEqual((*Min)[0]->AsNumber(), OriginM.X) || !FMath::IsNearlyEqual((*Min)[1]->AsNumber(), OriginM.Y))
		{
			UE_LOG(LogTemp, Error, TEXT("RTS água: a grade %s não bate com a do mapa (canto, célula ou linha 0): a água vem da altura"), *Png);
			return false;
		}
		// o hash do WorldForge, pela mesma conta
		uint64 Fnv = 0xcbf29ce484222325ull;
		auto Eat = [&Fnv](uint8 B) { Fnv = (Fnv ^ B) * 0x100000001b3ull; };
		for (const uint8 B : Raw)
		{
			Eat(B);
		}
		TArray<Sim::WaterBody> Bodies;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Meta->TryGetArrayField(TEXT("bodies"), List))
		{
			static const TCHAR* Kinds[] = { TEXT("lake"), TEXT("oasis"), TEXT("river"), TEXT("sea"), TEXT("ice") };
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString Kind;
				int32 Id = 0, Level = 0, FlowX = 0, FlowY = 0;
				O->TryGetNumberField(TEXT("id"), Id);
				O->TryGetStringField(TEXT("kind"), Kind);
				O->TryGetNumberField(TEXT("level_mm"), Level);
				O->TryGetNumberField(TEXT("flow_x_mm_s"), FlowX);
				O->TryGetNumberField(TEXT("flow_y_mm_s"), FlowY);
				const FString Line = FString::Printf(TEXT("%d,%s,%d,%d,%d\n"), Id, *Kind, Level, FlowX, FlowY);
				for (const TCHAR C : Line)
				{
					Eat(uint8(C));
				}
				Sim::WaterBody B;
				B.Id = uint8(FMath::Clamp(Id, 0, 255));
				for (uint8 K = 0; K < UE_ARRAY_COUNT(Kinds); ++K)
				{
					B.Kind = Kind == Kinds[K] ? K : B.Kind;
				}
				B.LevelMm = Level;
				B.FlowXMmS = FlowX;
				B.FlowYMmS = FlowY;
				Bodies.Add(B);
			}
		}
		const FString Got = FString::Printf(TEXT("%016llx"), (unsigned long long)Fnv);
		if (Got != Want.ToLower())
		{
			UE_LOG(LogTemp, Error, TEXT("RTS água: o hash da grade %s não confere (%s no arquivo, %s na conta): a água vem da altura"), *Png, *Want, *Got);
			return false;
		}
		M.Water.resize(size_t(Cells) * size_t(Cells));
		M.WaterBodyId.resize(M.Water.size());
		for (int32 I = 0; I < Cells * Cells; ++I)
		{
			M.Water[size_t(I)] = Raw[I * 4] <= uint8(Sim::WaterState::Ice) ? Raw[I * 4] : uint8(Sim::WaterState::Dry);
			M.WaterBodyId[size_t(I)] = Raw[I * 4 + 1];
		}
		M.WaterBodies.assign(Bodies.GetData(), Bodies.GetData() + Bodies.Num());
		UE_LOG(LogTemp, Log, TEXT("RTS água: a grade %s (hash %s), %d corpos"), *Info.GridFile, *Got, Bodies.Num());
		return true;
	}
}

FVector FRTSBakedMap::GroundNormal(const FVector2D& Cm) const
{
	const float D = SampleM * 100.0f;
	const float DX = GroundZCm(Cm + FVector2D(D, 0)) - GroundZCm(Cm - FVector2D(D, 0));
	const float DY = GroundZCm(Cm + FVector2D(0, D)) - GroundZCm(Cm - FVector2D(0, D));
	return FVector(-DX, -DY, 2.0f * D).GetSafeNormal();
}

bool RTSMapBaker::Bake(UWorld* World, const FRTSMapInfo& Info, FRTSBakedMap& Out, FString& OutError)
{
	const double Start = FPlatformTime::Seconds();
	Out = FRTSBakedMap();

	TArray<ALandscapeProxy*> Landscapes;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Landscapes.Add(*It);
	}
	if (Landscapes.IsEmpty())
	{
		OutError = TEXT("o mapa não tem Landscape");
		return false;
	}

	// 1. alturas a cada SampleM (a mesma grade de 2 m do WorldForge)
	const float Half = Info.SizeM * 0.5f;
	Out.SampleM = Info.SampleM;
	Out.Samples = FMath::RoundToInt(Info.SizeM / Info.SampleM) + 1;
	Out.OriginM = FVector2D(-Half, -Half);
	Out.HeightsM.SetNumUninitialized(Out.Samples * Out.Samples);
	int32 Missing = 0;
	for (int32 J = 0; J < Out.Samples; ++J)
	{
		for (int32 I = 0; I < Out.Samples; ++I)
		{
			const FVector P((Out.OriginM.X + I * Info.SampleM) * 100.0, (Out.OriginM.Y + J * Info.SampleM) * 100.0, 0.0);
			float Z = NoGround;
			for (const ALandscapeProxy* L : Landscapes)
			{
				if (const TOptional<float> H = L->GetHeightAtLocation(P))
				{
					Z = H.GetValue() / 100.0f;
					break;
				}
			}
			Missing += Z == NoGround ? 1 : 0;
			Out.HeightsM[J * Out.Samples + I] = Z;
		}
	}

	// 2. células de 4 m: penhasco pela inclinação (diferença central, como o gerador), água pela altura
	const int32 CellsPerSide = FMath::RoundToInt(Info.SizeM / Info.CellM);
	const int32 Step = FMath::Max(1, FMath::RoundToInt(Info.CellM / Info.SampleM));
	Sim::SimMap& M = Out.Logic;
	M.Resize(CellsPerSide, CellsPerSide, Sim::Fix::FromRatio(FMath::RoundToInt(Info.CellM * 1000), 1000),
		Sim::Vec2::Make(Sim::Fix::FromInt(-FMath::RoundToInt(Half)), Sim::Fix::FromInt(-FMath::RoundToInt(Half))));
	auto H = [&Out](int32 I, int32 J)
	{
		I = FMath::Clamp(I, 0, Out.Samples - 1);
		J = FMath::Clamp(J, 0, Out.Samples - 1);
		return Out.HeightsM[J * Out.Samples + I];
	};
	auto IsCliff = [&](int32 I, int32 J)
	{
		const float GX = (H(I + 1, J) - H(I - 1, J)) / (2.0f * Info.SampleM);
		const float GY = (H(I, J + 1) - H(I, J - 1)) / (2.0f * Info.SampleM);
		return GX * GX + GY * GY > Info.MaxSlope * Info.MaxSlope || H(I, J) == NoGround;
	};
	// a água do WorldForge, quando ele exporta a grade (a água pela altura fica de reserva)
	Out.bWaterGrid = LoadWaterGrid(Info, CellsPerSide, Out.OriginM, M);
	int32 GridShore = 0;
	int32 GridFord = 0;
	int32 GridIce = 0;
	Out.CellReason.Init(0, M.Width * M.Height);
	// a menor e a maior altura de cada célula, em decímetros: a regra de construir no plano (ZH)
	M.MinDm.assign(M.Pass.size(), 0);
	M.MaxDm.assign(M.Pass.size(), 0);
	for (int32 CY = 0; CY < CellsPerSide; ++CY)
	{
		for (int32 CX = 0; CX < CellsPerSide; ++CX)
		{
			// as amostras dentro da célula (3×3 com célula de 4 m e amostra de 2 m)
			int32 Cliffs = 0;
			int32 Count = 0;
			float Lo = TNumericLimits<float>::Max();
			float Hi = TNumericLimits<float>::Lowest();
			for (int32 DJ = 0; DJ <= Step; ++DJ)
			{
				for (int32 DI = 0; DI <= Step; ++DI)
				{
					Cliffs += IsCliff(CX * Step + DI, CY * Step + DJ) ? 1 : 0;
					++Count;
					const float Z = H(CX * Step + DI, CY * Step + DJ);
					Lo = FMath::Min(Lo, Z);
					Hi = FMath::Max(Hi, Z);
				}
			}
			M.MinDm[size_t(M.Index(CX, CY))] = int16(FMath::Clamp(FMath::FloorToInt(Lo * 10.0f), -32000, 32000));
			M.MaxDm[size_t(M.Index(CX, CY))] = int16(FMath::Clamp(FMath::CeilToInt(Hi * 10.0f), -32000, 32000));
			const float Center = H(CX * Step + Step / 2, CY * Step + Step / 2);
			uint8 Pass = uint8(Sim::CellPass::Open);
			const Sim::WaterState W = M.WaterAt(CX, CY);
			GridShore += W == Sim::WaterState::Shore ? 1 : 0;
			GridFord += W == Sim::WaterState::Ford ? 1 : 0;
			GridIce += W == Sim::WaterState::Ice ? 1 : 0;
			// com a grade: só o fundo é água (a margem, o vau e o gelo passam, mesmo abaixo da altura da água)
			if (Out.bWaterGrid ? W == Sim::WaterState::Deep : Center < Info.WaterBelowM)
			{
				Pass = uint8(Sim::CellPass::Water);
				++Out.WaterCells;
				Out.CellReason[M.Index(CX, CY)] = 1;
			}
			else if (Cliffs * 2 >= Count)
			{
				Pass = uint8(Sim::CellPass::Blocked);
				++Out.CliffCells;
				Out.CellReason[M.Index(CX, CY)] = 2;
			}
			M.Pass[M.Index(CX, CY)] = Pass;
		}
	}

	// 3. pedras grandes, paredões, palmeiras e barreiras viram célula bloqueada
	auto BlockFootprint = [&](const FBox& Bounds)
	{
		const FVector C = Bounds.GetCenter();
		const FVector E = Bounds.GetExtent() * 0.85f;   // a caixa é maior que a pedra
		if (E.X < 100.0f && E.Y < 100.0f)
		{
			return;   // menor que 2 m: o tanque passa por cima
		}
		const Sim::Cell Lo = M.CellOf(Sim::Vec2::Make(Sim::Fix::FromInt(FMath::FloorToInt((C.X - E.X) / 100.0f)), Sim::Fix::FromInt(FMath::FloorToInt((C.Y - E.Y) / 100.0f))));
		const Sim::Cell Hi = M.CellOf(Sim::Vec2::Make(Sim::Fix::FromInt(FMath::CeilToInt((C.X + E.X) / 100.0f)), Sim::Fix::FromInt(FMath::CeilToInt((C.Y + E.Y) / 100.0f))));
		for (int32 Y = FMath::Max(Lo.Y, 0); Y <= FMath::Min(Hi.Y, M.Height - 1); ++Y)
		{
			for (int32 X = FMath::Max(Lo.X, 0); X <= FMath::Min(Hi.X, M.Width - 1); ++X)
			{
				// dentro da elipse inscrita na caixa
				const Sim::Vec2 CellCenter = M.CellCenter(Sim::Cell{ X, Y });
				const float DX = (float(CellCenter.X.ToDouble()) * 100.0f - C.X) / E.X;
				const float DY = (float(CellCenter.Y.ToDouble()) * 100.0f - C.Y) / E.Y;
				if (DX * DX + DY * DY <= 1.0f && M.Pass[M.Index(X, Y)] == uint8(Sim::CellPass::Open))
				{
					M.Pass[M.Index(X, Y)] = uint8(Sim::CellPass::Blocked);
					++Out.ObstacleCells;
					Out.CellReason[M.Index(X, Y)] = 3;
				}
			}
		}
	};
	// (16/10) a árvore que o tanque derruba: um objeto da simulação no pé do tronco, com a altura dela (não fecha a copa)
	const auto AddTree = [&Out](const FVector& Base, float HeightCm, UInstancedStaticMeshComponent* Ism, int32 Index)
	{
		Sim::PropSetup P;
		P.Kind = Sim::PropKind::Tree;
		P.Pos = RTS::ToSim(FVector2D(Base.X, Base.Y));
		P.HalfLength = Sim::Fix::FromMilli(400);
		P.HalfWidth = Sim::Fix::FromMilli(400);
		P.Height = Sim::Fix::FromMilli(FMath::Clamp(FMath::RoundToInt(HeightCm * 10.0f), 2000, 30000));
		Out.Props.Add(P);
		Out.PropPiece.Add(-1);
		Out.PropIsm.Add(Ism);
		Out.PropIsmIndex.Add(Index);
		++Out.TreeProps;
	};
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* Comp : Meshes)
		{
			if (IsObstacleMesh(Comp->GetStaticMesh(), Info.TreeMeshes))
			{
				if (UInstancedStaticMeshComponent* TreeIsm = Cast<UInstancedStaticMeshComponent>(Comp))
				{
					const FBox Local = TreeIsm->GetStaticMesh()->GetBoundingBox();
					for (int32 K = 0; K < TreeIsm->GetInstanceCount(); ++K)
					{
						FTransform T;
						if (TreeIsm->GetInstanceTransform(K, T, /*bWorldSpace=*/true))
						{
							AddTree(T.GetLocation(), float(Local.TransformBy(T).GetSize().Z), TreeIsm, K);
						}
					}
				}
				else
				{
					AddTree(Comp->GetComponentLocation(), float(Comp->Bounds.GetBox().GetSize().Z), nullptr, -1);
				}
				continue;
			}
			if (!IsObstacleMesh(Comp->GetStaticMesh(), Info.ObstacleMeshes))
			{
				continue;
			}
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Comp))
			{
				const FBox Local = ISM->GetStaticMesh()->GetBoundingBox();
				for (int32 K = 0; K < ISM->GetInstanceCount(); ++K)
				{
					FTransform T;
					if (ISM->GetInstanceTransform(K, T, /*bWorldSpace=*/true))
					{
						BlockFootprint(Local.TransformBy(T));
					}
				}
			}
			else
			{
				BlockFootprint(Comp->Bounds.GetBox());
			}
		}
	}

	// 4. a vila do WorldForge: a pegada de cada peça que bloqueia, já na grade de 4 m dele (o mesmo canto
	// [-512, -512] da nossa); o quintal murado vira ilha fechada (os muros bloqueiam, o meio não)
	if (!Info.VillageFile.IsEmpty())
	{
		FString Text;
		TSharedPtr<FJsonObject> Village;
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), Info.VillageFile);
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Village) || !Village.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS vila: não li %s (game.ps1 worlds copia do WorldForge): o mapa fica sem as casas na grade"), *Path);
		}
		else
		{
			Village->TryGetStringField(TEXT("hash"), Out.VillageHash);
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(TEXT("WorldForgeVillage")))
				{
					for (const FName& Tag : It->Tags)
					{
						const FString T = Tag.ToString();
						if (T.StartsWith(TEXT("village_hash:")))
						{
							Out.VillageHashInMap = T.RightChop(13);
						}
					}
				}
			}
			if (Out.VillageHash != Out.VillageHashInMap)
			{
				// o JSON e o mapa montado são de montagens diferentes: as casas desenhadas e as da grade não batem
				UE_LOG(LogTemp, Error, TEXT("RTS vila: o JSON (%s) não é o do mapa montado (%s): rode o game.ps1 worlds depois de remontar"),
					*Out.VillageHash, Out.VillageHashInMap.IsEmpty() ? TEXT("sem etiqueta") : *Out.VillageHashInMap);
			}
			const TArray<TSharedPtr<FJsonValue>>* Origin = nullptr;
			const bool bSameGrid = Village->TryGetArrayField(TEXT("origin_m"), Origin) && Origin->Num() >= 2
				&& FMath::IsNearlyEqual((*Origin)[0]->AsNumber(), double(Out.OriginM.X)) && FMath::IsNearlyEqual((*Origin)[1]->AsNumber(), double(Out.OriginM.Y));
			const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
			if (!bSameGrid)
			{
				UE_LOG(LogTemp, Error, TEXT("RTS vila: a grade do JSON não começa no canto do mapa (%.0f, %.0f): fica sem as casas"), Out.OriginM.X, Out.OriginM.Y);
			}
			else if (Village->TryGetArrayField(TEXT("pieces"), Pieces))
			{
				int32 Blocking = 0;
				for (const TSharedPtr<FJsonValue>& V : *Pieces)
				{
					const TSharedPtr<FJsonObject> P = V->AsObject();
					bool bBlocks = false;
					const TArray<TSharedPtr<FJsonValue>>* CellAt = nullptr;
					const TArray<TSharedPtr<FJsonValue>>* CellsWH = nullptr;
					if (!P.IsValid() || !P->TryGetBoolField(TEXT("blocks"), bBlocks) || !bBlocks
						|| !P->TryGetArrayField(TEXT("cell"), CellAt) || CellAt->Num() < 2 || !P->TryGetArrayField(TEXT("cells"), CellsWH) || CellsWH->Num() < 2)
					{
						continue;
					}
					// (16/10) o carro civil e a carcaça: o tanque esmaga (um objeto da simulação: a célula fica esmagável, fechada
					// só pra quem não esmaga); o comprimento do carro vai no +Y da malha (a porta), girado pelo yaw_deg
					const FString PieceType = P->GetStringField(TEXT("type"));
					if (PieceType == TEXT("civ_pickup") || PieceType == TEXT("civ_car_burnt"))
					{
						const TArray<TSharedPtr<FJsonValue>>* SizeM = nullptr;
						double CarYaw = 0.0, CarHeight = 1.5;
						int32 CarRot = 0, PieceId = -1;
						P->TryGetNumberField(TEXT("rot"), CarRot);
						CarYaw = 90.0 * CarRot;
						P->TryGetNumberField(TEXT("yaw_deg"), CarYaw);
						P->TryGetNumberField(TEXT("height_m"), CarHeight);
						P->TryGetNumberField(TEXT("id"), PieceId);
						double Len = 4.6, Wid = 2.0;
						if (P->TryGetArrayField(TEXT("size_m"), SizeM) && SizeM->Num() >= 2)
						{
							Len = FMath::Max((*SizeM)[0]->AsNumber(), (*SizeM)[1]->AsNumber());
							Wid = FMath::Min((*SizeM)[0]->AsNumber(), (*SizeM)[1]->AsNumber());
						}
						Sim::PropSetup Car;
						Car.Kind = Sim::PropKind::Car;
						Car.Pos = RTS::ToSim(FVector2D(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y"))) * 100.0);
						Car.Facing = Sim::Angle::FromMilliDeg(RTS::Milli(CarYaw + 90.0));
						Car.HalfLength = Sim::Fix::FromMilli(RTS::Milli(Len * 0.5));
						Car.HalfWidth = Sim::Fix::FromMilli(RTS::Milli(Wid * 0.5));
						Car.Height = Sim::Fix::FromMilli(RTS::Milli(CarHeight));
						Out.Props.Add(Car);
						Out.PropPiece.Add(PieceId);
						Out.PropIsm.Add(nullptr);
						Out.PropIsmIndex.Add(-1);
						++Out.CarProps;
						continue;
					}
					++Blocking;
					const int32 I0 = int32((*CellAt)[0]->AsNumber()), J0 = int32((*CellAt)[1]->AsNumber());
					const int32 W = int32((*CellsWH)[0]->AsNumber()), Hh = int32((*CellsWH)[1]->AsNumber());
					for (int32 Y = FMath::Max(J0, 0); Y < FMath::Min(J0 + Hh, M.Height); ++Y)
					{
						for (int32 X = FMath::Max(I0, 0); X < FMath::Min(I0 + W, M.Width); ++X)
						{
							if (M.Pass[M.Index(X, Y)] == uint8(Sim::CellPass::Open))
							{
								M.Pass[M.Index(X, Y)] = uint8(Sim::CellPass::Blocked);
								++Out.VillageCells;
								Out.CellReason[M.Index(X, Y)] = 4;
							}
						}
					}
				}
				UE_LOG(LogTemp, Log, TEXT("RTS vila: %d peças, %d bloqueiam, %d células fechadas (hash %s)"), Pieces->Num(), Blocking, Out.VillageCells, *Out.VillageHash);
			}
		}
	}

	if (Out.bWaterGrid)
	{
		UE_LOG(LogTemp, Log, TEXT("RTS água: %d células de fundo, %d de margem, %d de vau, %d de gelo"), Out.WaterCells, GridShore, GridFord, GridIce);
	}
	// (16/10) os objetos em ordem fixa (o tipo, depois a posição): o índice é o do evento, igual em todo PC
	{
		TArray<int32> Order;
		for (int32 K = 0; K < Out.Props.Num(); ++K)
		{
			Order.Add(K);
		}
		Order.Sort([&Out](int32 A, int32 B)
		{
			const Sim::PropSetup& Pa = Out.Props[A];
			const Sim::PropSetup& Pb = Out.Props[B];
			if (Pa.Kind != Pb.Kind) { return uint8(Pa.Kind) < uint8(Pb.Kind); }
			if (Pa.Pos.X.Raw != Pb.Pos.X.Raw) { return Pa.Pos.X.Raw < Pb.Pos.X.Raw; }
			if (Pa.Pos.Y.Raw != Pb.Pos.Y.Raw) { return Pa.Pos.Y.Raw < Pb.Pos.Y.Raw; }
			return A < B;
		});
		TArray<Sim::PropSetup> Props;
		TArray<int32> Piece, IsmIndex;
		TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> Ism;
		for (const int32 K : Order)
		{
			Props.Add(Out.Props[K]);
			Piece.Add(Out.PropPiece[K]);
			Ism.Add(Out.PropIsm[K]);
			IsmIndex.Add(Out.PropIsmIndex[K]);
		}
		Out.Props = MoveTemp(Props);
		Out.PropPiece = MoveTemp(Piece);
		Out.PropIsm = MoveTemp(Ism);
		Out.PropIsmIndex = MoveTemp(IsmIndex);
		UE_LOG(LogTemp, Log, TEXT("RTS esmagar: %d carros e %d árvores no mapa viram objetos da simulação (o tanque esmaga e derruba)"), Out.CarProps, Out.TreeProps);
	}
	M.ComputeClearance();
	LoadSurface(Info, Out);
	Out.BakeSeconds = FPlatformTime::Seconds() - Start;
	UE_LOG(LogTemp, Log, TEXT("RTS mapa %s: %d×%d células de %.0f m; penhasco %d, água %d, pedras %d, sem chão %d amostras; %.2f s; hash %016llx"),
		*Info.Name, M.Width, M.Height, Info.CellM, Out.CliffCells, Out.WaterCells, Out.ObstacleCells, Missing, Out.BakeSeconds,
		(unsigned long long)M.Hash());
	return true;
}
