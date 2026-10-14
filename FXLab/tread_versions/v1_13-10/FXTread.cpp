#include "FXTread.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Engine/Texture2D.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "ShaderCompiler.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
	double SmoothStep(double X, double Lo, double Hi)
	{
		if (X <= Lo) return 0.0;
		if (X >= Hi) return 1.0;
		const double K = (X - Lo) / (Hi - Lo);
		return K * K * (3.0 - 2.0 * K);
	}

	FString RepoDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
	}

	const TCHAR* GroundNames[4] = { TEXT("sand"), TEXT("snow"), TEXT("mud"), TEXT("asphalt") };

	EFXGround GroundFromName(const FString& N)
	{
		for (int32 I = 0; I < 4; ++I)
		{
			if (N == GroundNames[I]) return EFXGround(I);
		}
		return EFXGround::Sand;
	}

	struct FCardOut { FVector P; double Size, Frame, Rot, Flip, Alpha, Erode; FVector Back = FVector::ZeroVector; };
	const FVector2D Corners[4] = { { -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } };
}

FFXTreadPreset::FFXTreadPreset()
{
	// os mesmos números do TREAD_DEFAULTS (fxtread.js)
	Grounds[int32(EFXGround::Sand)] = { 1.0, 0.78, 0.52, 4.0, 0.9, 1.0, 1.0, 1.0, 1.0 };
	Grounds[int32(EFXGround::Snow)] = { 0.95, 0.97, 1.0, 5.5, 2.2, 0.45, 1.6, 1.5, 0.7 };
	Grounds[int32(EFXGround::Mud)] = { 0.42, 0.34, 0.27, 2.2, 1.2, 0.5, 0.6, 0.7, 0.7 };
	// asfalto: a areia não levanta (o Felipe, 13/10: "poeira de areia não surja no asfalto")
	Grounds[int32(EFXGround::Asphalt)] = { 0.6, 0.58, 0.55, 3.0, 0.9, 0.7, 0.0, 0.6, 0.9 };
}

bool FFXTreadPreset::Load(const FString& Path)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return false;                  // sem tread.json: os números de fábrica (os do laboratório)
	}
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J)
	{
		return false;
	}
	J->TryGetStringField(TEXT("bake"), Bake);
	const TPair<const TCHAR*, double*> Fields[] = {
		{ TEXT("speed"), &Speed }, { TEXT("pathR"), &PathR }, { TEXT("spacing"), &Spacing }, { TEXT("fullSpeed"), &FullSpeed },
		{ TEXT("life"), &Life }, { TEXT("frame0"), &Frame0 }, { TEXT("frame1"), &Frame1 }, { TEXT("size0"), &Size0 },
		{ TEXT("size1"), &Size1 }, { TEXT("kick"), &Kick }, { TEXT("side"), &Side }, { TEXT("up"), &Up }, { TEXT("drag"), &Drag },
		{ TEXT("inherit"), &Inherit }, { TEXT("windX"), &WindX }, { TEXT("windY"), &WindY }, { TEXT("sink"), &Sink },
		{ TEXT("alpha"), &Alpha }, { TEXT("erode"), &Erode }, { TEXT("rearX"), &RearX }, { TEXT("halfW"), &HalfW },
		{ TEXT("along"), &Along }, { TEXT("rotMax"), &RotMax }, { TEXT("orient"), &Orient }, { TEXT("squashUp"), &SquashUp }, { TEXT("tankYaw"), &TankYaw },
	};
	for (const auto& F : Fields)
	{
		J->TryGetNumberField(F.Key, *F.Value);
	}
	for (int32 I = 0; I < 4; ++I)
	{
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (J->TryGetObjectField(GroundNames[I], G) && G && G->IsValid())
		{
			FFXTreadGround& D = Grounds[I];
			const TPair<const TCHAR*, double*> GF[] = {
				{ TEXT("r"), &D.R }, { TEXT("g"), &D.G }, { TEXT("b"), &D.B }, { TEXT("albedo"), &D.Albedo }, { TEXT("density"), &D.Density },
				{ TEXT("life"), &D.Life }, { TEXT("amount"), &D.Amount }, { TEXT("alpha"), &D.Alpha }, { TEXT("size"), &D.Size },
			};
			for (const auto& F : GF)
			{
				(*G)->TryGetNumberField(F.Key, *F.Value);
			}
		}
	}
	return true;
}

AFXTreadDust::AFXTreadDust()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	const TCHAR* Names[4] = { TEXT("SandCards"), TEXT("SnowCards"), TEXT("MudCards"), TEXT("AsphaltCards") };
	for (int32 I = 0; I < 4; ++I)
	{
		UProceduralMeshComponent* M = CreateDefaultSubobject<UProceduralMeshComponent>(Names[I]);
		M->SetupAttachment(Root);
		M->SetCastShadow(false);
		M->SetTranslucentSortPriority(1);
		Layers[I].Mesh = M;
	}
}

void AFXTreadDust::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	Light = FFXSceneLight::Find(GetWorld());
	RngState = uint32(Seed);
	LoadPreset();
	if (bLabDrive)
	{
		FString Ground;
		if (FParse::Value(FCommandLine::Get(), TEXT("-Ground="), Ground))
		{
			LabGround = GroundFromName(Ground);
		}
		// o tanque: no laboratório, as 5 peças paradas na origem; no mapa do jogo (World_Desert), as do grupo 1
		// (marca "Load1"). Cada peça guarda onde fica em relação ao casco (no chão, sem giro): o tanque inteiro anda
		BuildAsphaltGrid();
		if (FParse::Param(FCommandLine::Get(), TEXT("GameTank")))
		{
			SpawnGameTank();
		}
		TArray<AStaticMeshActor*> Parts;
		AStaticMeshActor* Hull = nullptr;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			const UStaticMesh* Sm = It->GetStaticMeshComponent() ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			// o tanque do jogo (as peças que o SpawnGameTank pôs, marca "GameTank"), ou o do laboratório: todas as
			// malhas (a esteira e as rodas vêm em várias: track_l/r, wheel_001…)
			const bool bGameTank = It->ActorHasTag(TEXT("GameTank"));
			if (!Sm || !(bGameTank || Sm->GetPathName().Contains(TEXT("/Units/USA_MBT/"))))
			{
				continue;
			}
			if (!bGameTank && (Light.bGame ? !It->ActorHasTag(TEXT("Load1")) : It->GetActorLocation().Size2D() > 1000.0))
			{
				continue;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("GameTank")) && !bGameTank)
			{
				continue;
			}
			Parts.Add(*It);
			if (Sm->GetName() == TEXT("SM_USA_MBT_Hull") || Sm->GetName() == TEXT("SM_LL_USATank_Hull"))
			{
				Hull = *It;
			}
		}
		if (Hull)
		{
			// primeiro as peças entram na lista (o raio do chão ignora elas), depois cada uma vira relativa ao casco
			for (AStaticMeshActor* A : Parts)
			{
				A->GetRootComponent()->SetMobility(EComponentMobility::Movable);
				TankParts.Add({ A, A->GetActorTransform() });
			}
			const FVector H = Hull->GetActorLocation() / 100.0;
			PathCenter = FVector(H.X, H.Y, 0.0);
			const FTransform Root0(FRotator::ZeroRotator, FVector(H.X, H.Y, GroundAt(H.X, H.Y, 0.0)) * 100.0);
			for (auto& Part : TankParts)
			{
				Part.Value = Part.Value.GetRelativeTransform(Root0);
				UE_LOG(LogTemp, Display, TEXT("FXTread: peça %s %s"), *Part.Key->GetActorLabel(), *Part.Value.GetLocation().ToString());
			}
		}
		// no mapa do jogo a volta é menor (o grupo tem ~5 m de areia limpa em volta; o resto tem pedra e planta)
		if (Light.bGame)
		{
			P.PathR = FMath::Min(P.PathR, 14.0);
		}
		// o tanque do jogo: a volta vai pra um lugar que cruza uma rua (ou pra -TreadAt=x,y)
		if (FParse::Param(FCommandLine::Get(), TEXT("GameTank")))
		{
			FString AtXY;
			TArray<FString> Xy;
			if (FParse::Value(FCommandLine::Get(), TEXT("-TreadAt="), AtXY) && AtXY.ParseIntoArray(Xy, TEXT(",")) == 2)
			{
				PathCenter = FVector(FCString::Atod(*Xy[0]), FCString::Atod(*Xy[1]), 0.0);
			}
			else
			{
				PathCenter = FindPathCenter();
			}
			UE_LOG(LogTemp, Display, TEXT("FXTread: a volta em (%.1f, %.1f) m, raio %.1f m"), PathCenter.X, PathCenter.Y, P.PathR);
		}
		FString At;
		bFixedStep = FParse::Value(FCommandLine::Get(), TEXT("-Shot="), ShotName);
		// no mapa do jogo quem fotografa é o controlador (FXLabGame.cpp, com o aquecimento dele); aqui só o laboratório
		if (bFixedStep && !Light.bGame && FParse::Value(FCommandLine::Get(), TEXT("-At="), At, false))
		{
			bCapture = true;
			TArray<FString> Items;
			At.ParseIntoArray(Items, TEXT(","));
			for (const FString& S : Items)
			{
				ShotAt.Add(FCString::Atod(*S));
			}
		}
		// aquece: a volta já com o rastro formado (o mesmo do laboratório: 6 s em passos de 1/30)
		for (int32 I = 0; I < 6 * 30; ++I)
		{
			LabPose(1.0 / 30.0);
			Step(1.0 / 30.0);
		}
		T = 0.0;
	}
}

void AFXTreadDust::LoadPreset()
{
	P = FFXTreadPreset();
	// o ajuste: tread.json (a versão EmberGen) ou -TreadProfile=tread_handmade.json (o plano B, feito à mão)
	FString Profile = TEXT("tread.json");
	FParse::Value(FCommandLine::Get(), TEXT("-TreadProfile="), Profile);
	P.Load(FPaths::ProjectDir() / Profile);
	Scene.Load(FPaths::ProjectDir() / TEXT("preset.json"));
	if (!Meta.Load(RepoDir() / TEXT("captures/fxlab/bake") / P.Bake / TEXT("atlas.json")))
	{
		UE_LOG(LogTemp, Error, TEXT("FXLab: falta o atlas.json de %s"), *P.Bake);
	}
	const FString Dir = FString::Printf(TEXT("/Game/FX/%s/"), *P.Bake);
	for (FLayer& L : Layers)
	{
		if (!L.Mid && CardMaterial)
		{
			L.Mid = UMaterialInstanceDynamic::Create(CardMaterial, this);
			Mids.Add(L.Mid);
			L.Mesh->SetMaterial(0, L.Mid);
		}
		if (!L.Mid)
		{
			continue;
		}
		const TPair<const TCHAR*, const TCHAR*> Tex[] = {
			{ TEXT("SixPos"), TEXT("T_SixPos") }, { TEXT("SixNeg"), TEXT("T_SixNeg") }, { TEXT("EmitTex"), TEXT("T_Emit") } };
		for (const auto& Tx : Tex)
		{
			UTexture* Texture = LoadObject<UTexture>(nullptr, *(Dir + Tx.Value + TEXT(".") + Tx.Value));
			if (!Texture)
			{
				UE_LOG(LogTemp, Error, TEXT("FXLab: falta a textura %s%s (fxlab.ps1 setup)"), *Dir, Tx.Value);
			}
			L.Mid->SetTextureParameterValue(Tx.Key, Texture);
		}
		L.Mid->SetVectorParameterValue(TEXT("Grid"), FLinearColor(Meta.Cols, Meta.Rows, Meta.Frames, 0.0f));
	}
	ApplyPreset();
}

void AFXTreadDust::ApplyPreset()
{
	const FLinearColor SunCol = FLinearColor::FromSRGBColor(FColor(255, 198, 140)) * float(Scene.Sun);
	const FLinearColor SkyCol = FLinearColor::FromSRGBColor(FColor(159, 180, 208)) * float(Scene.Sky);
	for (int32 I = 0; I < 4; ++I)
	{
		UMaterialInstanceDynamic* Mid = Layers[I].Mid;
		if (!Mid)
		{
			continue;
		}
		// o tom, a claridade e a densidade do chão; sem caule, sem fogo; partícula macia
		const FFXTreadGround& G = P.Grounds[I];
		Mid->SetVectorParameterValue(TEXT("SunCol"), SunCol);
		Mid->SetVectorParameterValue(TEXT("SkyCol"), SkyCol);
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(G.R, G.G, G.B, 1.0f));
		Mid->SetScalarParameterValue(TEXT("Density"), G.Density);
		Mid->SetScalarParameterValue(TEXT("Albedo"), G.Albedo);
		Mid->SetScalarParameterValue(TEXT("EmitGain"), 0.0f);
		Mid->SetScalarParameterValue(TEXT("EmberCut"), Scene.EmberCut);
		Mid->SetScalarParameterValue(TEXT("SoftDist"), 100.0f);
		Mid->SetScalarParameterValue(TEXT("SceneLight"), Light.bGame ? 1.0f : 0.0f);
		Mid->SetScalarParameterValue(TEXT("EmitScale"), float(Light.EmitScale));
	}
}

// mulberry32, igual ao laboratório
double AFXTreadDust::Rand()
{
	RngState += 0x6D2B79F5u;
	uint32 X = RngState;
	X = (X ^ (X >> 15)) * (X | 1u);
	X ^= X + (X ^ (X >> 7)) * (X | 61u);
	return double(X ^ (X >> 14)) / 4294967296.0;
}

double AFXTreadDust::Rnd(double A, double B)
{
	return A + Rand() * (B - A);
}

// o laboratório: o tanque anda no sentido anti-horário na volta de raio PathR (fxtread.js: pose)
void AFXTreadDust::LabPose(double Dt)
{
	T += Dt;
	if (P.Speed > 0.0)
	{
		Ang += P.Speed * Dt / P.PathR;
	}
	TankPos = PathCenter + FVector(FMath::Cos(Ang) * P.PathR, FMath::Sin(Ang) * P.PathR, 0.0);
	TankPos.Z = GroundAt(TankPos.X, TankPos.Y, TankPos.Z);
	TankFwd = FVector(-FMath::Sin(Ang), FMath::Cos(Ang), 0.0);
	FFXTreadSource S;
	S.Id = 0;
	S.PosM = TankPos;
	S.FwdM = TankFwd;
	S.Speed = P.Speed;
	S.Ground = AsphaltAt(TankPos.X, TankPos.Y) > 0.5 ? EFXGround::Asphalt : LabGround;
	Sources = { S };
}

double AFXTreadDust::GroundAt(double X, double Y, double Fallback) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FXTreadGround), true);
	for (const auto& Part : TankParts)
	{
		Q.AddIgnoredActor(Part.Key);
	}
	const FVector A(X * 100.0, Y * 100.0, 100000.0), B(X * 100.0, Y * 100.0, -100000.0);
	return GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Q) ? Hit.ImpactPoint.Z / 100.0 : Fallback;
}

void AFXTreadDust::BuildAsphaltGrid()
{
#if WITH_EDITORONLY_DATA
	// cada componente do Landscape guarda os pesos das camadas em texturas (um canal por camada); a fonte delas
	// (Source) existe no editor, que é onde o laboratório roda. No jogo, o tipo de chão vem da simulação.
	TArray<ULandscapeComponent*> Comps;
	ALandscapeProxy* Proxy = nullptr;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Proxy = *It;
		for (ULandscapeComponent* C : It->LandscapeComponents)
		{
			if (C) Comps.Add(C);
		}
	}
	if (!Proxy || Comps.Num() == 0)
	{
		return;
	}
	LandscapeXf = Proxy->GetActorTransform();
	FIntPoint Lo(INT32_MAX, INT32_MAX), Hi(INT32_MIN, INT32_MIN);
	for (const ULandscapeComponent* C : Comps)
	{
		const FIntPoint B = C->GetSectionBase();
		Lo = FIntPoint(FMath::Min(Lo.X, B.X), FMath::Min(Lo.Y, B.Y));
		Hi = FIntPoint(FMath::Max(Hi.X, B.X + C->ComponentSizeQuads), FMath::Max(Hi.Y, B.Y + C->ComponentSizeQuads));
	}
	AsphaltMin = Lo;
	AsphaltSize = Hi - Lo + FIntPoint(1, 1);
	AsphaltGrid.SetNumZeroed(int64(AsphaltSize.X) * AsphaltSize.Y);
	// a lista final de camadas (GetWeightmapLayerAllocations) só existe depois que a Unreal junta as camadas de edição na
	// placa, um tempo depois de carregar (e nunca sem placa): por isso lê também cada camada de edição, onde a pintura
	// do WorldForge fica guardada, e fica com o maior peso
	int32 Found = 0, NAllocs = 0, NNoInfo = 0, NNoSrc = 0, NFmt = 0, NNoMip = 0;
	TSet<FName> Names;
	auto ReadSet = [&](ULandscapeComponent* C, const TArray<FWeightmapLayerAllocationInfo>& Allocs, auto&& TexAt, int32 NumTex)
	{
		NAllocs += Allocs.Num();
		for (const FWeightmapLayerAllocationInfo& Al : Allocs)
		{
			if (!Al.LayerInfo)
			{
				++NNoInfo;
				continue;
			}
			Names.Add(Al.LayerInfo->GetLayerName());
			if (Al.LayerInfo->GetLayerName() != TEXT("Asphalt") || Al.WeightmapTextureIndex >= NumTex)
			{
				continue;
			}
			UTexture2D* W = TexAt(Al.WeightmapTextureIndex);
			TArray64<uint8> Mip;
			if (!W || !W->Source.IsValid())
			{
				++NNoSrc;
				continue;
			}
			if (W->Source.GetFormat() != TSF_BGRA8)
			{
				++NFmt;
				continue;
			}
			if (!W->Source.GetMipData(Mip, 0))
			{
				++NNoMip;
				continue;
			}
			const int32 TW = W->Source.GetSizeX(), TH = W->Source.GetSizeY();
			static const int32 ByteOf[4] = { 2, 1, 0, 3 };   // o canal R, G, B, A na memória BGRA
			const int32 Byte = ByteOf[FMath::Clamp<int32>(Al.WeightmapTextureChannel, 0, 3)];
			const int32 Ox = FMath::RoundToInt32(C->WeightmapScaleBias.Z * TW), Oy = FMath::RoundToInt32(C->WeightmapScaleBias.W * TH);
			const int32 Sq = C->SubsectionSizeQuads, Ns = C->NumSubsections;
			const FIntPoint B = C->GetSectionBase();
			for (int32 Ly = 0; Ly <= C->ComponentSizeQuads; ++Ly)
			{
				const int32 Sy = FMath::Min(Ly / Sq, Ns - 1), Ty = Oy + Sy * (Sq + 1) + (Ly - Sy * Sq);
				for (int32 Lx = 0; Lx <= C->ComponentSizeQuads; ++Lx)
				{
					const int32 Sx = FMath::Min(Lx / Sq, Ns - 1), Tx = Ox + Sx * (Sq + 1) + (Lx - Sx * Sq);
					if (Tx < 0 || Ty < 0 || Tx >= TW || Ty >= TH)
					{
						continue;
					}
					const uint8 V = Mip[(int64(Ty) * TW + Tx) * 4 + Byte];
					const int64 I = int64(B.Y + Ly - AsphaltMin.Y) * AsphaltSize.X + (B.X + Lx - AsphaltMin.X);
					AsphaltGrid[I] = FMath::Max(AsphaltGrid[I], V);
				}
			}
			++Found;
		}
	};
	int32 NEditLayers = 0;
	for (ULandscapeComponent* C : Comps)
	{
		const TArray<UTexture2D*>& Final = C->GetWeightmapTextures();
		ReadSet(C, C->GetWeightmapLayerAllocations(), [&](int32 I) { return Final[I]; }, Final.Num());
		C->ForEachLayer([&](const FGuid&, FLandscapeLayerComponentData& D)
		{
			++NEditLayers;
			ReadSet(C, D.WeightmapData.LayerAllocations, [&](int32 I) { return D.WeightmapData.Textures[I].Get(); }, D.WeightmapData.Textures.Num());
		});
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TreadAsphaltDump")))
	{
		// a grade inteira numa imagem cinza (PGM), pra conferir com a foto do mapa
		const FString Head = FString::Printf(TEXT("P5\n%d %d\n255\n"), AsphaltSize.X, AsphaltSize.Y);
		TArray<uint8> Bytes;
		Bytes.Append((const uint8*)TCHAR_TO_ANSI(*Head), Head.Len());
		Bytes.Append(AsphaltGrid);
		FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectSavedDir() / TEXT("asphalt_grid.pgm")));
	}
	FString NameList;
	for (const FName& N : Names)
	{
		NameList += N.ToString() + TEXT(" ");
	}
	UE_LOG(LogTemp, Display, TEXT("FXTread: asfalto lido em %d texturas (grade %dx%d; %d componentes, %d camadas de edição, %d pesos, sem info %d, sem fonte %d, formato %d, sem mip %d; nomes: %s)"),
		Found, AsphaltSize.X, AsphaltSize.Y, Comps.Num(), NEditLayers, NAllocs, NNoInfo, NNoSrc, NFmt, NNoMip, *NameList);
#endif
}

double AFXTreadDust::AsphaltAt(double X, double Y) const
{
	if (AsphaltGrid.Num() == 0)
	{
		return 0.0;
	}
	const FVector L = LandscapeXf.InverseTransformPosition(FVector(X * 100.0, Y * 100.0, 0.0));
	const int32 Gx = FMath::RoundToInt32(L.X) - AsphaltMin.X, Gy = FMath::RoundToInt32(L.Y) - AsphaltMin.Y;
	if (Gx < 0 || Gy < 0 || Gx >= AsphaltSize.X || Gy >= AsphaltSize.Y)
	{
		return 0.0;
	}
	return AsphaltGrid[int64(Gy) * AsphaltSize.X + Gx] / 255.0;
}

void AFXTreadDust::SpawnGameTank()
{
	// as malhas do Game/Config/RTS/Units.json (usa_mbt.visual): todas na origem do ator, a frente pra +X
	const TCHAR* Meshes[] = {
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Hull/StaticMeshes/SM_LL_USATank_Hull.SM_LL_USATank_Hull"),
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Wheels/StaticMeshes/SM_LL_USATank_Wheels.SM_LL_USATank_Wheels"),
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Track_l/StaticMeshes/SM_LL_USATank_Track_l.SM_LL_USATank_Track_l"),
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Track_r/StaticMeshes/SM_LL_USATank_Track_r.SM_LL_USATank_Track_r"),
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Turret/StaticMeshes/SM_LL_USATank_Turret.SM_LL_USATank_Turret"),
		TEXT("/Game/Units/USA_MBT_LL/SM_LL_USATank_Barrel/StaticMeshes/SM_LL_USATank_Barrel.SM_LL_USATank_Barrel"),
	};
	for (const TCHAR* Path : Meshes)
	{
		UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Sm)
		{
			UE_LOG(LogTemp, Error, TEXT("FXTread: falta a malha %s"), Path);
			continue;
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, Sp);
		A->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		A->GetStaticMeshComponent()->SetStaticMesh(Sm);
		A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		A->Tags.Add(TEXT("GameTank"));
	}
}

FVector AFXTreadDust::FindPathCenter() const
{
	// procura perto da vila (o centro do mapa) uma volta com ~25–45 % de asfalto e o resto no chão aberto
	const double R = P.PathR;
	FVector Best = FVector::ZeroVector;
	double BestScore = -1e9;
	for (double Y = -160.0; Y <= 160.0; Y += 12.0)
	{
		for (double X = -160.0; X <= 160.0; X += 12.0)
		{
			int32 OnAsphalt = 0, Blocked = 0;
			double ZMin = 1e9, ZMax = -1e9;
			const int32 N = 12;
			for (int32 K = 0; K < N; ++K)
			{
				const double A = 2.0 * PI * K / N;
				const double Px = X + FMath::Cos(A) * R, Py = Y + FMath::Sin(A) * R;
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(FXTreadSpot), false);
				if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(Px, Py, 1000.0) * 100.0, FVector(Px, Py, -1000.0) * 100.0, ECC_Visibility, Q))
				{
					++Blocked;
					continue;
				}
				// casa, muro, pedra: o raio bate em algo que não é o terreno
				if (!Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()))
				{
					++Blocked;
					continue;
				}
				ZMin = FMath::Min(ZMin, Hit.ImpactPoint.Z / 100.0);
				ZMax = FMath::Max(ZMax, Hit.ImpactPoint.Z / 100.0);
				OnAsphalt += AsphaltAt(Px, Py) > 0.5 ? 1 : 0;
			}
			if (Blocked > 0 || ZMax - ZMin > 2.5)
			{
				continue;
			}
			const double Frac = double(OnAsphalt) / N;
			const double Score = -FMath::Abs(Frac - 0.35) * 10.0 - (ZMax - ZMin) - FVector2D(X, Y).Size() * 0.005;
			if (OnAsphalt > 0 && OnAsphalt < N && Score > BestScore)
			{
				BestScore = Score;
				Best = FVector(X, Y, 0.0);
			}
		}
	}
	return Best;
}

void AFXTreadDust::Emit(const FFXTreadSource& Src, double Dt)
{
	const FFXTreadGround& G = P.Grounds[int32(Src.Ground)];
	const double Sp = FMath::Abs(Src.Speed);
	const double Amount = FMath::Min(1.0, Sp / P.FullSpeed) * G.Amount;
	if (Sp <= 0.3 || Amount <= 0.0)
	{
		return;
	}
	TPair<double, double>& A = Acc.FindOrAdd(Src.Id);
	const FVector F = Src.FwdM;
	const FVector R(F.Y, -F.X, 0.0);
	const FVector Vel = F * Src.Speed;
	for (int32 K = 0; K < 2; ++K)
	{
		double& Ac = K == 0 ? A.Key : A.Value;
		Ac += Sp * Dt;
		const double Spacing = P.Spacing / FMath::Max(0.25, Amount);
		while (Ac >= Spacing)
		{
			Ac -= Spacing;
			const double SideSign = K == 0 ? -1.0 : 1.0;
			// o pé da lagarta de trás (fxtread.js: contact)
			const FVector C = Src.PosM + F * P.RearX + R * P.HalfW * SideSign + FVector(0.0, 0.0, 0.05);
			const double Gz = Src.PosM.Z;
			// uma chamada por linha: a ordem dos sorteios é a do laboratório
			const double Back = Ac - Rnd(0.0, P.Along);
			FCard Cd;
			Cd.P = C - F * Back;
			Cd.P.X += Rnd(-0.3, 0.3);
			Cd.P.Y += Rnd(-0.3, 0.3);
			const double Kick = P.Kick * FMath::Min(1.0, 0.5 * Sp + 0.1);       // a regra do ZH: 0,5 × velocidade + 0,1
			const double SideV = Rnd(-1.0, 1.0) * P.Side + SideSign * P.Side * 0.5;
			Cd.V = -F * Kick + R * SideV + Vel * P.Inherit;
			Cd.V.Z += P.Up * Rnd(0.6, 1.3);
			Cd.Life = P.Life * G.Life * Rnd(0.8, 1.2);
			Cd.S = Rnd(0.85, 1.2);
			Cd.Rot = Rnd(-P.RotMax, P.RotMax);
			Cd.Flip = Rand() < 0.5 ? -1.0 : 1.0;
			Cd.F0 = P.Frame0 + Rnd(0.0, 4.0);
			Cd.A = Amount;
			Cd.GA = G.Alpha;
			Cd.GS = G.Size;
			Cd.Ground = Src.Ground;
			Cd.Back = -F;
			Cd.GroundZ = Gz;
			Cards.Add(Cd);
		}
	}
}

void AFXTreadDust::Step(double Dt)
{
	for (const FFXTreadSource& S : Sources)
	{
		Emit(S, Dt);
	}
	// a poeira anda: arrasto, vento, um pouco pra baixo (não passa do chão)
	const double Dk = FMath::Exp(-P.Drag * Dt);
	for (FCard& C : Cards)
	{
		C.Age += Dt;
		C.V = FVector(C.V.X * Dk + P.WindX * (1.0 - Dk), C.V.Y * Dk + P.WindY * (1.0 - Dk), C.V.Z * Dk - P.Sink * (1.0 - Dk));
		C.P += C.V * Dt;
		C.P.Z = FMath::Max(C.P.Z, C.GroundZ);
	}
	Cards.RemoveAll([](const FCard& C) { return C.Age >= C.Life; });
}

void AFXTreadDust::Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward)
{
	TArray<FCardOut> Out[4];
	for (const FCard& C : Cards)
	{
		const double K = C.Age / C.Life;
		const double Grow = 1.0 - FMath::Square(1.0 - K);        // cresce rápido e assenta
		Out[int32(C.Ground)].Add({ C.P, Meta.SideM * (P.Size0 + (P.Size1 - P.Size0) * Grow) * C.S * C.GS,
			FMath::Min(double(Meta.Frames - 1), C.F0 + (P.Frame1 - P.Frame0) * K), C.Rot, C.Flip,
			FMath::Min(1.0, P.Alpha * C.A * C.GA) * SmoothStep(C.Age, 0.0, 0.2) * (1.0 - SmoothStep(K, 0.2, 1.0)),   // denso junto da lagarta
			P.Erode * SmoothStep(K, 0.35, 1.0), C.Back });
	}
	const FVector Origin100 = GetActorLocation();
	const FVector ToCam = -CamForward;
	const FVector Sd = Light.bGame ? Light.SunDir : Scene.SunDir();
	for (int32 G = 0; G < 4; ++G)
	{
		TArray<FCardOut>& O = Out[G];
		O.Sort([&CamM](const FCardOut& A, const FCardOut& B) { return FVector::DistSquared(A.P, CamM) > FVector::DistSquared(B.P, CamM); });
		TArray<FVector> V, Nrm;
		TArray<int32> Tri;
		TArray<FVector2D> U0, U1, U2, U3;
		TArray<FLinearColor> Cl;
		TArray<FProcMeshTangent> Tg;
		for (const FCardOut& Cd : O)
		{
			// orient: a gravação tem o rastro pra direita do cartão (o vento da simulação); o cartão gira pra direita
			// dele apontar pra trás do tanque, como ele aparece na tela. Senão, o giro sorteado
			double Rot = Cd.Rot, Flip = Cd.Flip;
			if (P.Orient > 0.5 && !Cd.Back.IsNearlyZero())
			{
				Rot = FMath::Atan2(FVector::DotProduct(Cd.Back, CamUp), FVector::DotProduct(Cd.Back, CamRight)) + Cd.Rot * 0.25;
				Flip = 1.0;
			}
			const double Cs = FMath::Cos(Rot), Sn = FMath::Sin(Rot);
			// squashUp: achata o cartão na vertical da tela (a poeira rente ao chão, vista da câmera do RTS)
			const FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp;
			const FVector L3(FVector::DotProduct(Sd, R) * Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, ToCam));
			const FLinearColor Lc(L3.X * 0.5 + 0.5, L3.Y * 0.5 + 0.5, L3.Z * 0.5 + 0.5, 1.0f);
			const FVector2D Pivot(Flip > 0 ? Meta.Pivot.X - 0.5 : 0.5 - Meta.Pivot.X, Meta.Pivot.Y - 0.5);
			const int32 Base = V.Num();
			for (const FVector2D& Q : Corners)
			{
				const FVector2D Qp = Q - Pivot;
				V.Add((Cd.P + (R * Qp.X + Up * (Qp.Y * P.SquashUp)) * Cd.Size) * 100.0 - Origin100);
				Nrm.Add(ToCam);
				U0.Add(FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5));
				U1.Add(FVector2D(Cd.Frame, Cd.Alpha));
				U2.Add(FVector2D(0.0, 0.0));
				U3.Add(FVector2D(Cd.Erode, 1.0));
				Cl.Add(Lc);
			}
			Tri.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
		Layers[G].Mesh->CreateMeshSection_LinearColor(0, V, Tri, Nrm, U0, U1, U2, U3, Cl, Tg, false, false);
	}
}

void AFXTreadDust::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerCameraManager* Pcm = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Pcm || Meta.Frames <= 1)
	{
		return;
	}
	const double Dt = bFixedStep ? 1.0 / 30.0 : DeltaSeconds * TimeScale;
	// foto: o relógio só anda depois dos shaders compilarem (o mesmo aquecimento do controlador)
	const bool bWarming = bFixedStep && (++Warm < 30 || (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0));
	if (!bPaused && !bWarming)
	{
		if (bLabDrive)
		{
			LabPose(Dt);
		}
		Step(Dt);
	}
	if (bLabDrive)
	{
		// o tanque na pose da volta (o modelo parado na origem com a frente pra +x, girado pelo yaw)
		const double YawDeg = FMath::RadiansToDegrees(FMath::Atan2(TankFwd.Y, TankFwd.X)) + P.TankYaw;
		const FTransform Root(FRotator(0.0, YawDeg, 0.0), TankPos * 100.0);   // o casco no chão, girado pra frente
		for (const auto& Part : TankParts)
		{
			if (Part.Key)
			{
				Part.Key->SetActorTransform(Part.Value * Root);
			}
		}
		// a câmera segue o tanque (mesmo ângulo), como no navegador. Na 1ª vez, põe o tanque no meio da tela:
		// onde o raio do meio da câmera bate na altura do tanque
		if (APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AActor* View = Pc->GetViewTarget())
			{
				if (!bCamAligned)
				{
					const FVector Cl = Pcm->GetCameraLocation(), Fw = Pcm->GetCameraRotation().Vector();
					if (FMath::Abs(Fw.Z) > 1e-3)
					{
						const double K = (TankPos.Z * 100.0 - Cl.Z) / Fw.Z;
						const FVector Hit = Cl + Fw * K;
						View->AddActorWorldOffset(FVector(TankPos.X * 100.0 - Hit.X, TankPos.Y * 100.0 - Hit.Y, 0.0));
					}
					CamFollowed = TankPos * 100.0;
					bCamAligned = true;
				}
				const FVector D = TankPos * 100.0 - CamFollowed;
				View->AddActorWorldOffset(FVector(D.X, D.Y, 0.0));
				CamFollowed = TankPos * 100.0;
			}
		}
	}
	const FRotationMatrix Rm(Pcm->GetCameraRotation());
	const FVector CamLoc = Pcm->GetCameraLocation();
	Draw(CamLoc / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
	if (!bCapture)
	{
		return;
	}
	if (ShotAt.Num() && T >= ShotAt[0] - 1e-6)
	{
		const FString Path = RepoDir() / TEXT("captures/fxlab/shots_ue") / FString::Printf(TEXT("%s_%.2fs.png"), *ShotName, ShotAt[0]);
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		ShotAt.RemoveAt(0);
		if (!ShotAt.Num())
		{
			ExitIn = 6;
		}
	}
	if (ExitIn > 0 && --ExitIn == 0)
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}
