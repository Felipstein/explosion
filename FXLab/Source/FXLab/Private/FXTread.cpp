#include "FXTread.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "LookDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Engine/Texture2D.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ProceduralMeshComponent.h"
#include "RTSTreadDust.h"
#include "RHI.h"
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

	struct FCardOut
	{
		FVector P; double Size, Frame, Rot, Flip, Alpha, Erode; FVector Back = FVector::ZeroVector;
		double PivotX = -1.0, PivotY = -1.0, CamOffset = 0.0, Orient = -1.0;   // −1: os do perfil (a poeira de 13/10)
		double ShadowK = 0.0, GroundZ = 0.0, BaseShade = 1.0, Sun = 1.0;
	};
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
		{ TEXT("softDist"), &SoftDist }, { TEXT("loadTau"), &LoadTau }, { TEXT("hullLen"), &HullLen },
	};
	for (const auto& F : Fields)
	{
		J->TryGetNumberField(F.Key, *F.Value);
	}
	Layers.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
	if (J->TryGetArrayField(TEXT("layers"), Ls) && Ls)
	{
		for (const TSharedPtr<FJsonValue>& V : *Ls)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O)
			{
				continue;
			}
			FFXTreadLayer L;
			O->TryGetStringField(TEXT("name"), L.Name);
			const TPair<const TCHAR*, double*> LF[] = {
				{ TEXT("xMin"), &L.XMin }, { TEXT("xMax"), &L.XMax }, { TEXT("yOff"), &L.YOff }, { TEXT("yJit"), &L.YJit }, { TEXT("z0"), &L.Z0 },
				{ TEXT("spacing"), &L.Spacing }, { TEXT("spacingJit"), &L.SpacingJit }, { TEXT("life"), &L.Life }, { TEXT("lifeJit"), &L.LifeJit },
				{ TEXT("maskDense"), &L.MaskDense }, { TEXT("maskEnd"), &L.MaskEnd }, { TEXT("bakeStart"), &L.BakeStart },
				{ TEXT("bakeStartJit"), &L.BakeStartJit }, { TEXT("bakeRate"), &L.BakeRate }, { TEXT("scale"), &L.Scale },
				{ TEXT("scaleJit"), &L.ScaleJit }, { TEXT("growTo"), &L.GrowTo }, { TEXT("alpha"), &L.Alpha }, { TEXT("attack"), &L.Attack },
				{ TEXT("decay"), &L.Decay }, { TEXT("inherit"), &L.Inherit }, { TEXT("drag"), &L.Drag }, { TEXT("kick"), &L.Kick },
				{ TEXT("side"), &L.Side }, { TEXT("up"), &L.Up }, { TEXT("sink"), &L.Sink }, { TEXT("wakeRise"), &L.WakeRise },
				{ TEXT("wakePull"), &L.WakePull }, { TEXT("wakeGrow"), &L.WakeGrow }, { TEXT("wakeLen"), &L.WakeLen },
				{ TEXT("orient"), &L.Orient }, { TEXT("flip"), &L.Flip }, { TEXT("rotMax"), &L.RotMax }, { TEXT("pivotBottom"), &L.PivotBottom },
				{ TEXT("camOffset"), &L.CamOffset }, { TEXT("erode"), &L.Erode }, { TEXT("v0"), &L.V0 }, { TEXT("v1"), &L.V1 },
				{ TEXT("followLoad"), &L.FollowLoad }, { TEXT("loadGain"), &L.LoadGain }, { TEXT("shadow"), &L.Shadow }, { TEXT("baseShade"), &L.BaseShade }, { TEXT("scaleSlow"), &L.ScaleSlow }, { TEXT("vFull"), &L.VFull },
			};
			for (const auto& F : LF)
			{
				O->TryGetNumberField(F.Key, *F.Value);
			}
			Layers.Add(L);
		}
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
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->SetTranslucentSortPriority(1);
		Layers[I].Mesh = M;
	}
	ShadowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DustShadow"));
	ShadowMesh->SetupAttachment(Root);
	ShadowMesh->SetCastShadow(false);
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCanEverAffectNavigation(false);
}

void AFXTreadDust::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(FVector::ZeroVector);
	Light = FFXSceneLight::Find(GetWorld());
	RngState = uint32(Seed);
	LoadPreset();
	if (P.Layers.Num() && FParse::Param(FCommandLine::Get(), TEXT("TreadPlugin")))
	{
		PluginDust = GetWorld()->SpawnActor<ARTSTreadDust>();
		PluginDust->SetExternalClock(true);
		int32 Q = 3;
		FParse::Value(FCommandLine::Get(), TEXT("-TreadQuality="), Q);
		PluginDust->SetQuality(Q);
	}
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
		// a câmera dos alvos (docs/POEIRA_DAS_LAGARTAS.md, E): r1 = a r1_raid_slabs (o alvo principal); v2 = o
		// gameplay_v2 de perto (onde a poeira nasce); v2close = a mesma pose, com o campo fechado 3,5×
		FString CamName;
		FParse::Value(FCommandLine::Get(), TEXT("-Cam="), CamName);
		if (CamName == TEXT("r1"))
		{
			bRig = true;
			RigPitch = 28.0; RigFov = 50.0; RigRel = 80.0; SunRel = -41.0; SunEl = 25.0; RigFrac = 0.16; P.Speed = 7.9;
		}
		else if (CamName == TEXT("v2") || CamName == TEXT("v2close"))
		{
			bRig = true;
			RigPitch = 30.5; RigFov = 50.0; RigRel = -52.0; SunRel = -22.0; SunEl = 20.0; RigFrac = 135.0 / 1280.0; P.Speed = 2.5;
		}
		FParse::Value(FCommandLine::Get(), TEXT("-CamPitch="), RigPitch);
		FParse::Value(FCommandLine::Get(), TEXT("-CamRel="), RigRel);
		FParse::Value(FCommandLine::Get(), TEXT("-CamFrac="), RigFrac);
		FParse::Value(FCommandLine::Get(), TEXT("-SunRel="), SunRel);
		FParse::Value(FCommandLine::Get(), TEXT("-SunEl="), SunEl);
		FParse::Value(FCommandLine::Get(), TEXT("-TreadSpeed="), P.Speed);
		FParse::Value(FCommandLine::Get(), TEXT("-TreadTanks="), NumTanks);
		FString GridStr;
		if (FParse::Value(FCommandLine::Get(), TEXT("-TreadGrid="), GridStr))
		{
			FString A, B;
			if (GridStr.Split(TEXT("x"), &A, &B))
			{
				GridCols = FCString::Atoi(*A);
				GridRows = FCString::Atoi(*B);
				NumTanks = GridCols * GridRows;
			}
		}
		for (int32 I = 0; I < FMath::Max(NumTanks, 1); ++I)
		{
			// desencontrados: o hodômetro de cada um começa noutro ponto e a velocidade varia ±10 % (a mesma semente)
			TankOdoOffset.Add(Rnd(0.0, 20.0));
			TankSpeedMul.Add(Rnd(0.9, 1.1));
		}
		FParse::Value(FCommandLine::Get(), TEXT("-TreadAB="), ABKey);
		FParse::Value(FCommandLine::Get(), TEXT("-TreadMeasure="), MeasureName);
		FString PathKind;
		FParse::Value(FCommandLine::Get(), TEXT("-TreadPath="), PathKind);
		bLine = PathKind == TEXT("line") || PathKind == TEXT("toasphalt") || PathKind == TEXT("shade") || bRig;
		if (bLine)
		{
			// a reta: 6 s de aquecimento + 6 s gravados, com folga
			const double Len = P.Speed * 12.0 + 10.0;
			FString AtXY;
			TArray<FString> Xy;
			double Heading = 0.0;
			if (FParse::Value(FCommandLine::Get(), TEXT("-TreadAt="), AtXY) && AtXY.ParseIntoArray(Xy, TEXT(",")) == 2
				&& FParse::Value(FCommandLine::Get(), TEXT("-TreadHeading="), Heading))
			{
				LineDir = FVector(FMath::Cos(FMath::DegreesToRadians(Heading)), FMath::Sin(FMath::DegreesToRadians(Heading)), 0.0);
				LineStart = FVector(FCString::Atod(*Xy[0]), FCString::Atod(*Xy[1]), 0.0);
			}
			else if (!FindPathLine(Len, LineStart, LineDir, PathKind == TEXT("toasphalt"), PathKind == TEXT("shade")))
			{
				UE_LOG(LogTemp, Error, TEXT("FXTread: não achei reta só de areia de %.0f m"), Len);
			}
			UE_LOG(LogTemp, Display, TEXT("FXTread: reta em (%.1f, %.1f) m, rumo %.0f graus, %.1f m/s"), LineStart.X, LineStart.Y,
				FMath::RadiansToDegrees(FMath::Atan2(LineDir.Y, LineDir.X)), P.Speed);
		}
		// no mapa do jogo a volta é menor (o grupo tem ~5 m de areia limpa em volta; o resto tem pedra e planta)
		if (Light.bGame)
		{
			P.PathR = FMath::Min(P.PathR, 14.0);
		}
		// o tanque do jogo: a volta vai pra um lugar que cruza uma rua (ou pra -TreadAt=x,y)
		if (FParse::Param(FCommandLine::Get(), TEXT("GameTank")) && !bLine)
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
		if (bLine)
		{
			T = -6.0 - 1.0 / 30.0;
		}
		for (int32 I = 0; I < 6 * 30; ++I)
		{
			LabPose(1.0 / 30.0);
			StepDust(1.0 / 30.0);
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
	UMaterialInterface* DustMat = P.Layers.Num() ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_FXSixWayDust.M_FXSixWayDust")) : nullptr;
	for (FLayer& L : Layers)
	{
		if (DustMat)
		{
			// a poeira em camadas: o material sem a folha do fogo (a T_Emit nem é carregada)
			if (!L.MidDust)
			{
				L.MidDust = UMaterialInstanceDynamic::Create(DustMat, this);
				Mids.Add(L.MidDust);
			}
			for (const TCHAR* Tn : { TEXT("SixPos"), TEXT("SixNeg") })
			{
				L.MidDust->SetTextureParameterValue(Tn, LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_") + Tn + TEXT(".T_") + Tn)));
			}
			L.MidDust->SetVectorParameterValue(TEXT("Grid"), FLinearColor(Meta.Cols, Meta.Rows, Meta.Frames, 0.0f));
			if (bDustMat || ABKey != TEXT("dustmat"))
			{
				L.Mesh->SetMaterial(0, L.MidDust);
				if (ABKey != TEXT("dustmat"))
				{
					continue;       // sem A/B, o material do fogo nem é montado
				}
			}
		}
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
	if (!ShadowMaterial)
	{
		ShadowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_FXDustShadow.M_FXDustShadow"));
	}
	if (ShadowMaterial && ShadowMesh)
	{
		if (!ShadowMid)
		{
			ShadowMid = UMaterialInstanceDynamic::Create(ShadowMaterial, this);
			Mids.Add(ShadowMid);
			ShadowMesh->SetMaterial(0, ShadowMid);
		}
		ShadowMid->SetTextureParameterValue(TEXT("SixPos"), LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixPos.T_SixPos"))));
		ShadowMid->SetVectorParameterValue(TEXT("Grid"), FLinearColor(Meta.Cols, Meta.Rows, Meta.Frames, 0.0f));
		// a sombra no modo antigo (Modulate), pro A/B de custo
		if (UMaterialInterface* Mod = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_FXDustShadowMod.M_FXDustShadowMod")))
		{
			ShadowMidMod = UMaterialInstanceDynamic::Create(Mod, this);
			ShadowMidMod->SetTextureParameterValue(TEXT("SixPos"), LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixPos.T_SixPos"))));
			ShadowMidMod->SetVectorParameterValue(TEXT("Grid"), FLinearColor(Meta.Cols, Meta.Rows, Meta.Frames, 0.0f));
		}
	}
	ApplyPreset();
}

void AFXTreadDust::ApplyPreset()
{
	const FLinearColor SunCol = FLinearColor::FromSRGBColor(FColor(255, 198, 140)) * float(Scene.Sun);
	const FLinearColor SkyCol = FLinearColor::FromSRGBColor(FColor(159, 180, 208)) * float(Scene.Sky);
	for (int32 I = 0; I < 8; ++I)
	{
		UMaterialInstanceDynamic* Mid = I < 4 ? Layers[I].Mid : Layers[I - 4].MidDust;
		if (!Mid)
		{
			continue;
		}
		// o tom, a claridade e a densidade do chão; sem caule, sem fogo; partícula macia
		const FFXTreadGround& G = P.Grounds[I % 4];
		Mid->SetVectorParameterValue(TEXT("SunCol"), SunCol);
		Mid->SetVectorParameterValue(TEXT("SkyCol"), SkyCol);
		Mid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(G.R, G.G, G.B, 1.0f));
		Mid->SetScalarParameterValue(TEXT("Density"), G.Density);
		Mid->SetScalarParameterValue(TEXT("Albedo"), G.Albedo);
		Mid->SetScalarParameterValue(TEXT("EmitGain"), 0.0f);
		Mid->SetScalarParameterValue(TEXT("EmberCut"), Scene.EmberCut);
		Mid->SetScalarParameterValue(TEXT("SoftDist"), float(P.SoftDist));
		// descarta o pixel quase transparente (não lê nem escreve o alvo); só na poeira em camadas
		Mid->SetScalarParameterValue(TEXT("ClipAlpha"), P.Layers.Num() && bClip ? 1.0f / 1024.0f : -1.0f);
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
	if (bLine)
	{
		TankPos = LineStart + LineDir * P.Speed * T;
		TankPos.Z = GroundAt(TankPos.X, TankPos.Y, TankPos.Z);
		TankFwd = LineDir;
	}
	else
	{
		if (P.Speed > 0.0)
		{
			Ang += P.Speed * Dt / P.PathR;
		}
		TankPos = PathCenter + FVector(FMath::Cos(Ang) * P.PathR, FMath::Sin(Ang) * P.PathR, 0.0);
		TankPos.Z = GroundAt(TankPos.X, TankPos.Y, TankPos.Z);
		TankFwd = FVector(-FMath::Sin(Ang), FMath::Cos(Ang), 0.0);
	}
	FFXTreadSource S;
	S.Id = 0;
	S.PosM = TankPos;
	S.FwdM = TankFwd;
	S.Speed = P.Speed;
	S.Ground = AsphaltAtNearest(TankPos.X, TankPos.Y) > 0.5 ? EFXGround::Asphalt : LabGround;
	if (P.Layers.Num())
	{
		// a poeira em camadas lê o chão no pé de trás de cada lagarta, com peso contínuo (o chão do tanque fica o tipo,
		// pra cor); o lado da lagarta esquerda é −R
		const FVector R(TankFwd.Y, -TankFwd.X, 0.0);
		for (int32 K = 0; K < 2; ++K)
		{
			const FVector Foot = TankPos + TankFwd * -2.40 + R * (1.425 * (K == 0 ? -1.0 : 1.0));
			S.Sand[K] = 1.0 - AsphaltAt(Foot.X, Foot.Y);
			const FVector Front = TankPos + TankFwd * 2.55 + R * (1.425 * (K == 0 ? -1.0 : 1.0));
			S.SandFront[K] = 1.0 - AsphaltAt(Front.X, Front.Y);
		}
		S.Ground = LabGround;
	}
	Sources = { S };
	// a formação: os outros tanques (só a poeira; a malha é a do primeiro) atrás e do lado, na mesma reta
	if (NumTanks != 1)
	{
		Sources.Reset();
		const FVector Rr(TankFwd.Y, -TankFwd.X, 0.0);
		for (int32 I = 0; I < NumTanks; ++I)
		{
			FFXTreadSource T2 = S;
			T2.Id = I;
			if (GridCols > 0)
			{
				T2.PosM = TankPos + Rr * (10.0 * (I % GridCols - 0.5 * (GridCols - 1))) - TankFwd * (12.0 * (I / GridCols));
			}
			else
			{
				T2.PosM = TankPos + Rr * (12.0 * (I % 5 - 2)) - TankFwd * (15.0 * (I / 5));
			}
			if (TankSpeedMul.IsValidIndex(I))
			{
				T2.Speed *= TankSpeedMul[I];
			}
			T2.PosM.Z = GroundAt(T2.PosM.X, T2.PosM.Y, TankPos.Z);
			Sources.Add(T2);
		}
	}
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

double AFXTreadDust::AsphaltAtNearest(double X, double Y) const
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

double AFXTreadDust::AsphaltAt(double X, double Y) const
{
	if (AsphaltGrid.Num() == 0)
	{
		return 0.0;
	}
	// bilinear: o peso contínuo não pisca na borda serrilhada da rua
	const FVector L = LandscapeXf.InverseTransformPosition(FVector(X * 100.0, Y * 100.0, 0.0));
	const double Fx = L.X - AsphaltMin.X, Fy = L.Y - AsphaltMin.Y;
	const int32 Gx = FMath::FloorToInt32(Fx), Gy = FMath::FloorToInt32(Fy);
	if (Gx < 0 || Gy < 0 || Gx + 1 >= AsphaltSize.X || Gy + 1 >= AsphaltSize.Y)
	{
		return 0.0;
	}
	const double Tx = Fx - Gx, Ty = Fy - Gy;
	auto W = [this](int32 X0, int32 Y0) { return AsphaltGrid[int64(Y0) * AsphaltSize.X + X0] / 255.0; };
	return FMath::Lerp(FMath::Lerp(W(Gx, Gy), W(Gx + 1, Gy), Tx), FMath::Lerp(W(Gx, Gy + 1), W(Gx + 1, Gy + 1), Tx), Ty);
}

bool AFXTreadDust::FindPathLine(double Len, FVector& Start, FVector& Dir, bool bToAsphalt, bool bShade) const
{
	// bShade (-TreadPath=shade): a reta passa pela sombra de árvore ou casa nos 5 s gravados (parte na sombra, parte no
	// sol), com o sol da câmera do alvo (que gira com o rumo): o teste da sombra do mapa em cima da poeira
	// bToAsphalt (-TreadPath=toasphalt): a frente do tanque entra no asfalto 2 s depois do começo da gravação e
	// segue nele por 15 m; antes disso, só areia (o teste da areia presa, E4)
	const double SCross = P.Speed * 8.0 + 2.0;
	// perto do centro (a vila), uma reta de Len m em que todo raio bate no terreno, sem asfalto, quase plana, e com
	// 6 m livres de cada lado (a poeira não atravessa casa nem pedra). O começo devolvido é a pose em t = 0
	// os obstáculos: árvore, pedra, casa (muitas não têm colisão pro raio, então vão pelos limites): o centro no
	// chão e o raio, em m. O capim (raio < 1 m) não conta
	struct FObst { FVector2D C; double R; FBox B; };
	TArray<FObst> Obst;
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* Sm = *It;
		if (!Sm || Sm->GetWorld() != GetWorld() || !Sm->IsRegistered() || !Sm->IsVisible() || Sm->GetOwner() == this)
		{
			continue;
		}
		if (Sm->GetOwner() && Sm->GetOwner()->ActorHasTag(TEXT("GameTank")))
		{
			continue;
		}
		if (UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Sm))
		{
			const UStaticMesh* Mesh = Ism->GetStaticMesh();
			const double Rad = Mesh ? Mesh->GetBounds().SphereRadius * 0.01 * 0.5 : 0.0;
			if (Rad < 1.0)
			{
				continue;
			}
			for (int32 I = 0; I < Ism->GetInstanceCount(); ++I)
			{
				FTransform Xf;
				if (Ism->GetInstanceTransform(I, Xf, true))
				{
					Obst.Add({ FVector2D(Xf.GetLocation()) * 0.01, Rad * Xf.GetMaximumAxisScale(), Mesh->GetBoundingBox().TransformBy(Xf) });
				}
			}
			continue;
		}
		const double Rad = Sm->Bounds.SphereRadius * 0.01 * 0.5;
		if (Rad >= 1.0 && Rad < 60.0)
		{
			Obst.Add({ FVector2D(Sm->Bounds.Origin) * 0.01, Rad, Sm->Bounds.GetBox() });
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FXTread: %d obstáculos pra reta"), Obst.Num());
	// pra sombra, só o que tem mais de 3 m de altura (árvore, casa); o capim e a pedra baixa não contam
	TArray<const FObst*> Tall;
	for (const FObst& O : Obst)
	{
		if (O.B.Max.Z - O.B.Min.Z > 300.0)
		{
			Tall.Add(&O);
		}
	}
	double BestScore = -1e18;
	bool bFound = false;
	for (double Y = -240.0; Y <= 240.0; Y += 20.0)
	{
		for (double X = -240.0; X <= 240.0; X += 20.0)
		{
			for (int32 Hd = 0; Hd < 24; ++Hd)
			{
				const double A = 2.0 * PI * Hd / 24.0;
				const FVector D(FMath::Cos(A), FMath::Sin(A), 0.0), Sd(-D.Y, D.X, 0.0);
				bool bOk = true;
				double ZMin = 1e9, ZMax = -1e9;
				for (double S = 0.0; S <= Len && bOk; S += 2.0)
				{
					for (int32 Side = -1; Side <= 1; ++Side)
					{
						const FVector Q = FVector(X, Y, 0.0) + D * S + Sd * (6.0 * Side);
						FHitResult Hit;
						FCollisionQueryParams Qp(SCENE_QUERY_STAT(FXTreadLine), false);
						const double Asph = AsphaltAt(Q.X, Q.Y);
						const bool bWantAsphalt = bToAsphalt && Side == 0 && S >= SCross + 1.0 && S <= SCross + 15.0;
						const bool bFreeZone = bToAsphalt && S > SCross - 4.0 && S < SCross + 1.0;    // a borda serrilhada
						const bool bGroundOk = bWantAsphalt ? Asph > 0.9 : (bFreeZone || (bToAsphalt && S > SCross) || Asph <= 0.02);
						if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(Q.X, Q.Y, 1000.0) * 100.0, FVector(Q.X, Q.Y, -1000.0) * 100.0, ECC_Visibility, Qp)
							|| !Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent()) || !bGroundOk)
						{
							bOk = false;
							break;
						}
						ZMin = FMath::Min(ZMin, Hit.ImpactPoint.Z / 100.0);
						ZMax = FMath::Max(ZMax, Hit.ImpactPoint.Z / 100.0);
					}
				}
				if (!bOk || ZMax - ZMin > 3.0)
				{
					continue;
				}
				// nada a menos de 8 m da reta (o tanque tem 1,9 m de meia largura; a poeira abre 3 m de cada lado)
				const FVector2D A0(X, Y), Dd(D.X, D.Y);
				for (const FObst& O : Obst)
				{
					const double S = FMath::Clamp(FVector2D::DotProduct(O.C - A0, Dd), 0.0, Len);
					if (FVector2D::Distance(A0 + Dd * S, O.C) < (bToAsphalt || bShade ? 4.0 : 8.0) + O.R)
					{
						bOk = false;
						break;
					}
				}
				if (!bOk)
				{
					continue;
				}
				double Score = -(ZMax - ZMin) * 4.0 - FVector2D(X, Y).Size() * 0.02;
				if (bShade)
				{
					// o sol desta reta (o da câmera do alvo); a sombra pelas caixas dos obstáculos, a 1,5 m do chão no rastro
					const double SunYaw = FMath::DegreesToRadians(FMath::RadiansToDegrees(A) - RigRel + SunRel);
					const double El = FMath::DegreesToRadians(SunEl);
					const FVector ToSun(FMath::Cos(El) * FMath::Cos(SunYaw), FMath::Cos(El) * FMath::Sin(SunYaw), FMath::Sin(El));
					int32 Shaded = 0, N = 0;
					for (double S = P.Speed * 6.0; S <= P.Speed * 11.0; S += 1.0, ++N)
					{
						const FVector Q = (FVector(X, Y, 0.0) + D * S) * 100.0 + FVector(0.0, 0.0, (ZMin + 1.5) * 100.0);
						for (const FObst* O : Tall)
						{
							if (FVector2D::DistSquared(FVector2D(Q) * 0.01, O->C) < 60.0 * 60.0
								&& FMath::LineBoxIntersection(O->B, Q, Q + ToSun * 6000.0, ToSun * 6000.0))
							{
								++Shaded;
								break;
							}
						}
					}
					const double Frac = N ? double(Shaded) / N : 0.0;
					if (Frac < 0.2 || Frac > 0.7)
					{
						continue;
					}
					Score += 100.0 * FMath::Min(Frac, 1.0 - Frac);
				}
				if (Score > BestScore)
				{
					BestScore = Score;
					Dir = D;
					Start = FVector(X, Y, 0.0) + D * (P.Speed * 6.0 + 2.0);   // a pose em t = 0, depois do aquecimento
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

void AFXTreadDust::ApplyRig()
{
	APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0);
	ACameraActor* Cam = Pc ? Cast<ACameraActor>(Pc->GetViewTarget()) : nullptr;
	if (!Cam)
	{
		return;
	}
	// a frente da câmera no chão: o rumo do tanque girado RigRel graus pra trás (o tanque fica RigRel à direita dela)
	const double TankYawDeg = FMath::RadiansToDegrees(FMath::Atan2(TankFwd.Y, TankFwd.X));
	const double CamYaw = TankYawDeg - RigRel;
	const FRotator Rot(-RigPitch, CamYaw, 0.0);
	const FRotationMatrix Rm(Rot);
	const FVector Fw = Rm.GetScaledAxis(EAxis::X), Rt = Rm.GetScaledAxis(EAxis::Y), Upv = Rm.GetScaledAxis(EAxis::Z);
	const FVector Aim = (TankPos + FVector(0.0, 0.0, 1.0)) * 100.0;
	if (!bRigDistSolved)
	{
		// a distância que dá ao casco (7,93 m, da frente à traseira) a mesma fração da largura da tela do alvo
		const double TanH = FMath::Tan(FMath::DegreesToRadians(RigFov * 0.5));
		for (int32 It = 0; It < 4; ++It)
		{
			const FVector C = Aim - Fw * RigDist * 100.0;
			auto U = [&](const FVector& Pw)
			{
				const FVector D = Pw - C;
				return FVector2D(FVector::DotProduct(D, Rt), FVector::DotProduct(D, Upv)) / FVector::DotProduct(D, Fw);
			};
			const double Frac = (U(Aim + TankFwd * 396.5) - U(Aim - TankFwd * 396.5)).Size() / (2.0 * TanH);
			RigDist *= Frac / RigFrac;
		}
		bRigDistSolved = true;
		UE_LOG(LogTemp, Display, TEXT("FXTread: câmera do alvo a %.1f m, %.1f graus, campo %.1f, tanque a %.0f graus da vista; sol a %.0f graus da vista e %.0f de altura"),
			RigDist, RigPitch, RigFov, RigRel, SunRel, SunEl);
	}
	FString CamName;
	FParse::Value(FCommandLine::Get(), TEXT("-Cam="), CamName);
	const double Fov = CamName == TEXT("v2close")
		? 2.0 * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(RigFov * 0.5)) / 3.5)) : RigFov;
	Cam->SetActorLocationAndRotation(Aim - Fw * RigDist * 100.0, Rot);
	Cam->GetCameraComponent()->SetFieldOfView(float(Fov));
	// o sol: SunRel graus da frente da câmera, SunEl de altura; a luz anda ao contrário (como o ApplySun do Look)
	for (TActorIterator<ALookDirector> It(GetWorld()); It; ++It)
	{
		if (UDirectionalLightComponent* Sun = It->GetSun())
		{
			Sun->SetWorldRotation(FRotator(-SunEl, CamYaw + SunRel + 180.0, 0.0));
			Light.SunDir = -Sun->GetComponentTransform().GetUnitAxis(EAxis::X);
		}
		break;
	}
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
				OnAsphalt += AsphaltAtNearest(Px, Py) > 0.5 ? 1 : 0;
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

double AFXTreadDust::SunVisAt(const FVector& Pm, const FVector& ToSun) const
{
	// o tanque em duas caixas (Game/Tools/look_tank.py): o casco com as lagartas (x ±3,95, y ±1,88, z 0..1,55) e a
	// torre (x −2,55..+2,27, y ±1,5, z 1,55..2,48); um raio do ponto pro sol, no referencial de cada tanque
	struct FBox { double X0, X1, Y0, Y1, Z0, Z1; };
	static const FBox Boxes[2] = { { -3.95, 3.95, -1.88, 1.88, 0.0, 1.55 }, { -2.55, 2.27, -1.5, 1.5, 1.55, 2.48 } };
	// o tanque só faz sombra perto dele: até a meia diagonal do casco (4,37 m) mais a sombra da torre (2,48 m de
	// altura × cot da elevação do sol); com o sol quase no horizonte não corta
	const double CotEl = ToSun.Z > 0.05 ? FMath::Sqrt(FMath::Max(0.0, 1.0 - ToSun.Z * ToSun.Z)) / ToSun.Z : 1e6;
	for (const FFXTreadSource& S : Sources)
	{
		const FVector F = S.FwdM, R(S.FwdM.Y, -S.FwdM.X, 0.0);
		const FVector D = Pm - S.PosM;
		if (D.Z > 2.48 && ToSun.Z > 0.0)
		{
			continue;
		}
		const double Reach = 4.37 + FMath::Max(0.0, 2.48 - D.Z) * CotEl;
		if (D.X * D.X + D.Y * D.Y > Reach * Reach)
		{
			continue;
		}
		const double O[3] = { FVector::DotProduct(D, F), FVector::DotProduct(D, R), D.Z };
		const double V[3] = { FVector::DotProduct(ToSun, F), FVector::DotProduct(ToSun, R), ToSun.Z };
		for (const FBox& B : Boxes)
		{
			const double Lo[3] = { B.X0, B.Y0, B.Z0 }, Hi[3] = { B.X1, B.Y1, B.Z1 };
			double T0 = 0.0, T1 = 1e9;
			bool bHit = true;
			for (int32 A = 0; A < 3 && bHit; ++A)
			{
				if (FMath::Abs(V[A]) < 1e-9)
				{
					bHit = O[A] >= Lo[A] && O[A] <= Hi[A];
					continue;
				}
				double Ta = (Lo[A] - O[A]) / V[A], Tb = (Hi[A] - O[A]) / V[A];
				if (Ta > Tb) Swap(Ta, Tb);
				T0 = FMath::Max(T0, Ta);
				T1 = FMath::Min(T1, Tb);
				bHit = T0 <= T1;
			}
			if (bHit)
			{
				return 0.0;
			}
		}
	}
	return 1.0;
}

double AFXTreadDust::FrameAt(double BakeT) const
{
	const TArray<double>& Tm = Meta.Times;
	if (Tm.Num() < 2)
	{
		return FMath::Clamp(BakeT / FMath::Max(Meta.LastTime, 1e-3) * (Meta.Frames - 1), 0.0, double(Meta.Frames - 1));
	}
	if (BakeT <= Tm[0])
	{
		return 0.0;
	}
	for (int32 I = 1; I < Tm.Num(); ++I)
	{
		if (BakeT <= Tm[I])
		{
			return (I - 1) + (BakeT - Tm[I - 1]) / FMath::Max(Tm[I] - Tm[I - 1], 1e-6);
		}
	}
	return double(Tm.Num() - 1);
}

void AFXTreadDust::EmitLayers(const FFXTreadSource& Src, double Dt)
{
	const FFXTreadGround& G = P.Grounds[int32(Src.Ground)];
	const FVector F = Src.FwdM;
	const FVector R(F.Y, -F.X, 0.0);
	const FVector2D& Ld = Load.FindOrAdd(Src.Id, FVector2D(Src.Sand[0], Src.Sand[1]));
	for (int32 Li = 0; Li < P.Layers.Num(); ++Li)
	{
		const FFXTreadLayer& L = P.Layers[Li];
		for (int32 K = 0; K < 2; ++K)
		{
			const double SideSign = K == 0 ? -1.0 : 1.0;
			// cada lagarta anda v ∓ ω·1,425 (no giro no lugar uma vai pra frente e a outra pra trás: a fonte de cada uma
			// é a roda do lado pra onde ela anda)
			const double Vk = Src.Speed + SideSign * Src.YawRate * 1.425;
			const double DirK = Vk < 0.0 ? -1.0 : 1.0;
			const double V = FMath::Abs(Vk);
			const double Ramp = L.V1 > L.V0 ? FMath::Clamp((V - L.V0) / (L.V1 - L.V0), 0.0, 1.0) : (V > 0.3 ? 1.0 : 0.0);
			// o chão: no pé de trás pras camadas que nascem atrás; quem nasce ao longo da lagarta (a cortina) usa o maior
			// dos dois pra abrir a porta e sorteia cada cartão pelo peso no ponto onde ele nasce (a areia entre o pé e a frente)
			const bool bAlong = L.XMax > -2.40;
			const double Wr = FMath::Clamp(Src.Sand[K], 0.0, 1.0), Wf = FMath::Clamp(Src.SandFront[K], 0.0, 1.0);
			const double W = bAlong ? FMath::Max(Wr, Wf) : Wr;
			const double Lk = K == 0 ? Ld.X : Ld.Y;
			// a nuvem e a cortina param na hora no asfalto; o jato segue a areia presa, que pinga menos de 1 s
			const double Ground = L.FollowLoad > 0.5 ? W + L.LoadGain * FMath::Max(0.0, Lk - W) : W;
			const double Amount = Ramp * Ground * G.Amount;
			double& Ac = LayerAcc.FindOrAdd(int64(Src.Id) * 64 + Li * 2 + K);
			if (Amount < 0.025)
			{
				Ac = 0.0;
				continue;
			}
			Ac += V * Dt;
			// o espaçamento sorteado fica guardado até sair o cartão (não depende do qps)
			double& Next = LayerNext.FindOrAdd(int64(Src.Id) * 64 + Li * 2 + K, L.Spacing);
			while (Ac >= Next / Amount)
			{
				Ac -= Next / Amount;
				Next = L.Spacing * Rnd(1.0, FMath::Max(1.0, L.SpacingJit));
				const double X = Rnd(L.XMin, L.XMax) * DirK;
				const double Y = SideSign * L.YOff + Rnd(-L.YJit, L.YJit);
				if (bAlong)
				{
					const double Wx = FMath::Lerp(Wr, Wf, FMath::Clamp((X - -2.40) / (2.55 - -2.40), 0.0, 1.0));
					if (Rand() * W > Wx)
					{
						continue;              // esse pedaço da lagarta está no asfalto
					}
				}
				FCard Cd;
				Cd.P = Src.PosM + F * X + R * Y + FVector(0.0, 0.0, L.Z0);
				Cd.V = F * Vk * L.Inherit - F * DirK * L.Kick + R * (Rnd(-1.0, 1.0) * L.Side);
				Cd.V.Z += L.Up * Rnd(0.6, 1.3);
				Cd.Life = L.Life * (1.0 + Rnd(-L.LifeJit, L.LifeJit));
				// no asfalto, o que pinga é menor (a areia presa)
				Cd.S = Rnd(1.0 - L.ScaleJit, 1.0 + L.ScaleJit) * (L.FollowLoad > 0.5 ? FMath::Lerp(0.5, 1.0, W) : 1.0);
				if (L.ScaleSlow < 1.0 && L.VFull > L.V1)
				{
					// a nuvem cresce com a velocidade (a esteira do casco mistura mais quanto mais rápido)
					Cd.S *= FMath::Lerp(L.ScaleSlow, 1.0, FMath::Clamp((V - L.V1) / (L.VFull - L.V1), 0.0, 1.0));
				}
				Cd.Rot = Rnd(-L.RotMax, L.RotMax);
				Cd.Flip = L.Flip != 0.0 ? L.Flip : (Rand() < 0.5 ? -1.0 : 1.0);
				Cd.Bake0 = L.BakeStart + Rnd(0.0, L.BakeStartJit);
				Cd.A = FMath::Min(1.0, Amount);
				Cd.GA = G.Alpha;
				Cd.GS = G.Size;
				Cd.Ground = Src.Ground;
				Cd.Back = -F * DirK;
				Cd.GroundZ = Src.PosM.Z;
				Cd.Layer = Li;
				Cd.Src = Src.Id;
				Cd.Odo0 = Odo.FindOrAdd(Src.Id);
				Cards.Add(Cd);
			}
		}
	}
}

void AFXTreadDust::Step(double Dt)
{
	for (const FFXTreadSource& S : Sources)
	{
		if (P.Layers.Num())
		{
			// os metros andados e a areia presa de cada lagarta: enche na hora na areia, esvazia com τ no asfalto
			if (!Odo.Contains(S.Id) && TankOdoOffset.IsValidIndex(S.Id))
			{
				Odo.Add(S.Id, TankOdoOffset[S.Id]);
			}
			Odo.FindOrAdd(S.Id) += FMath::Abs(S.Speed) * Dt;
			FVector2D& Ld = Load.FindOrAdd(S.Id, FVector2D(S.Sand[0], S.Sand[1]));
			const double Dk = FMath::Exp(-Dt / FMath::Max(P.LoadTau, 1e-3));
			Ld.X = FMath::Max(S.Sand[0], Ld.X * Dk);
			Ld.Y = FMath::Max(S.Sand[1], Ld.Y * Dk);
			EmitLayers(S, Dt);
			// -TreadLog: o chão e a areia presa de cada lagarta, quadro a quadro (o teste do asfalto, E4)
			static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("TreadLog"));
			if (bLog && T >= 0.0)
			{
				int32 N[3] = { 0, 0, 0 };
				for (const FCard& Cc : Cards)
				{
					if (Cc.Src == S.Id && Cc.Age < Dt * 1.5 && Cc.Layer >= 0 && Cc.Layer < 3)
					{
						++N[Cc.Layer];
					}
				}
				const FString Line = FString::Printf(TEXT("%.4f;%.3f;%.3f;%.3f;%.3f;%d;%d;%d\n"), T, S.Sand[0], S.Sand[1], Ld.X, Ld.Y, N[0], N[1], N[2]);
				FFileHelper::SaveStringToFile(Line, *(FPaths::ProjectSavedDir() / TEXT("tread_log.csv")), FFileHelper::EEncodingOptions::AutoDetect,
					&IFileManager::Get(), FILEWRITE_Append);
			}
		}
		else
		{
			Emit(S, Dt);
		}
	}
	if (P.Layers.Num())
	{
		++StepCount;
		TMap<int32, int32> SrcIndex;
		SrcIndex.Reserve(Sources.Num());
		for (int32 I = 0; I < Sources.Num(); ++I)
		{
			SrcIndex.Add(Sources[I].Id, I);
		}
		const FVector ToSun = Light.bGame ? Light.SunDir : Scene.SunDir();
		int32 CardIndex = 0;
		for (FCard& C : Cards)
		{
			const FFXTreadLayer& L = P.Layers[C.Layer];
			++CardIndex;
			C.Age += Dt;
			const double Dk = FMath::Exp(-L.Drag * Dt);
			C.V = FVector(C.V.X * Dk + P.WindX * (1.0 - Dk), C.V.Y * Dk + P.WindY * (1.0 - Dk), C.V.Z * Dk - L.Sink * (1.0 - Dk));
			FVector Extra = FVector::ZeroVector;
			// a esteira do casco: perto do tanque andando, a poeira sobe, vai pro meio e cresce (× velocidade)
			if (L.WakeRise > 0.0 || L.WakePull > 0.0 || L.WakeGrow > 0.0)
			{
				if (const int32* Si = SrcIndex.Find(C.Src))
				{
					const FFXTreadSource& S = Sources[*Si];
					const double Behind = Odo.FindRef(S.Id) - C.Odo0;
					const double V = FMath::Abs(S.Speed);
					if (V > 0.3 && Behind < L.WakeLen * P.HullLen)
					{
						const double Fall = 1.0 - Behind / (L.WakeLen * P.HullLen);
						const FVector Rr(S.FwdM.Y, -S.FwdM.X, 0.0);
						const double Lat = FVector::DotProduct(C.P - S.PosM, Rr);
						Extra = FVector(0.0, 0.0, L.WakeRise * V * Fall) - Rr * (Lat * L.WakePull * V * Fall);
						C.Wake += L.WakeGrow * V * Fall * Dt;
					}
				}
			}
			C.P += (C.V + Extra) * Dt;
			C.P.Z = FMath::Max(C.P.Z, C.GroundZ);
			// o sol que chega na nuvem (a sombra do tanque): no pé e no meio do cartão, suavizado em ~0,15 s (sem piscar);
			// cada cartão é reamostrado a cada 3 passos (a suavização já é bem mais lenta que isso)
			if (C.Sun < 0.0 || (StepCount + CardIndex) % 3 == 0)
			{
				const double Half = Meta.SideM * L.Scale * C.S * 0.25;
				const double Sv = 0.5 * (SunVisAt(C.P + FVector(0.0, 0.0, 0.3), ToSun) + SunVisAt(C.P + FVector(0.0, 0.0, 0.3 + Half), ToSun));
				C.Sun = C.Sun < 0.0 ? Sv : C.Sun + (Sv - C.Sun) * (1.0 - FMath::Exp(-3.0 * Dt / 0.15));
			}
		}
		// a máscara de comprimento acabou: o cartão sai antes da vida (economia)
		Cards.RemoveAll([this](const FCard& C)
		{
			const FFXTreadLayer& L = P.Layers[C.Layer];
			// a máscara de comprimento: sai quando o cartão já está abaixo de 5 % (ainda pagaria o pixel inteiro)
			if (L.MaskEnd > 0.0)
			{
				const double D = (Odo.FindRef(C.Src) - C.Odo0) / P.HullLen;
				if (D >= L.MaskEnd || 1.0 - SmoothStep(D, L.MaskDense, L.MaskEnd) < 0.05)
				{
					return true;
				}
			}
			return C.Age >= C.Life;
		});
		return;
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

void AFXTreadDust::StepDust(double Dt)
{
	if (!PluginDust)
	{
		Step(Dt);
		return;
	}
	TArray<FRTSTreadSource> Out;
	Out.Reserve(Sources.Num());
	for (const FFXTreadSource& S : Sources)
	{
		FRTSTreadSource& R = Out.AddDefaulted_GetRef();
		R.Id = S.Id;
		R.PosM = S.PosM;
		R.FwdM = S.FwdM;
		R.Speed = S.Speed;
		R.YawRate = S.YawRate;
		for (int32 K = 0; K < 2; ++K)
		{
			R.Sand[K] = S.Sand[K];
			R.SandFront[K] = S.SandFront[K];
		}
	}
	PluginDust->SetSources(Out);
	PluginDust->Advance(Dt);
}

void AFXTreadDust::ApplyAB(bool bState)
{
	bABState = bState;
	if (ABKey == TEXT("shadow")) bShadowOn = bState;
	else if (ABKey == TEXT("k2")) bK2On = bState;
	else if (ABKey == TEXT("cut8")) bCut8 = bState;
	else if (ABKey == TEXT("dustmat"))
	{
		bDustMat = bState;
		for (FLayer& L : Layers)
		{
			if (L.Mesh && L.Mid && L.MidDust)
			{
				L.Mesh->SetMaterial(0, bState ? L.MidDust : L.Mid);
			}
		}
	}
	else if (ABKey == TEXT("clip"))
	{
		bClip = bState;
		ApplyPreset();
		if (ShadowMid) ShadowMid->SetScalarParameterValue(TEXT("ClipAlpha"), bClip ? 1.0f / 1024.0f : -1.0f);
	}
	else if (ABKey == TEXT("shadowmod") && ShadowMesh && ShadowMid && ShadowMidMod)
	{
		bShadowModulate = bState;
		ShadowMesh->SetMaterial(0, bState ? static_cast<UMaterialInterface*>(ShadowMidMod.Get()) : static_cast<UMaterialInterface*>(ShadowMid));
	}
}

void AFXTreadDust::PushMesh(UProceduralMeshComponent* Mesh, int32 Slot, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& U0,
	TArray<FVector2D>& U1, TArray<FVector2D>& U2, TArray<FVector2D>& U3, TArray<FLinearColor>& Cl, int32 VertsPerCard)
{
	const double P0 = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { LastPushMs += (FPlatformTime::Seconds() - P0) * 1000.0; };
	const int32 K = FMath::Max(VertsPerCard, 3);
	// os quads ocupam 4 vértices cada, sempre na mesma ordem (0-1-2, 0-2-3): com folga de 256 quads, a seção é
	// recriada só quando cresce; nos outros quadros só os vértices mudam (UpdateMeshSection não recria o proxy de
	// render nem a colisão). Os quads de sobra ficam encolhidos num ponto com alfa 0
	const int32 Quads = V.Num() / K;
	const bool bGrow = Quads > MeshCap[Slot] || MeshVerts[Slot] != K || !Mesh->GetProcMeshSection(0);
	const int32 Cap = bGrow ? ((Quads / 256) + 1) * 256 : MeshCap[Slot];
	const FVector Far = V.Num() ? V[0] : FVector::ZeroVector;
	for (int32 Q = Quads; Q < Cap; ++Q)
	{
		for (int32 Kv = 0; Kv < K; ++Kv)
		{
			V.Add(Far); N.Add(FVector::UpVector); U0.Add(FVector2D::ZeroVector); U1.Add(FVector2D::ZeroVector);
			if (U2.Num()) U2.Add(FVector2D::ZeroVector);
			if (U3.Num()) U3.Add(FVector2D::ZeroVector);
			Cl.Add(FLinearColor::Transparent);
		}
	}
	TArray<FProcMeshTangent> Tg;
	if (bGrow)
	{
		TArray<int32> Tri;
		Tri.Reserve(Cap * (K - 2) * 3);
		for (int32 Q = 0; Q < Cap; ++Q)
		{
			// leque: (0, i, i + 1); com 4 é o 0-1-2, 0-2-3 de sempre
			const int32 B = Q * K;
			for (int32 I = 1; I + 1 < K; ++I)
			{
				Tri.Append({ B, B + I, B + I + 1 });
			}
		}
		Mesh->CreateMeshSection_LinearColor(0, V, Tri, N, U0, U1, U2, U3, Cl, Tg, false, false);
		MeshCap[Slot] = Cap;
		MeshVerts[Slot] = K;
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, V, N, U0, U1, U2, U3, Cl, Tg, false);
	}
}

void AFXTreadDust::Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward)
{
	LastPushMs = 0.0;
	TArray<FCardOut> Out[4];
	for (TArray<FCardOut>& O4 : Out)
	{
		O4.Reserve(Cards.Num());
	}
	for (const FCard& C : Cards)
	{
		if (C.Layer >= 0 && P.Layers.IsValidIndex(C.Layer))
		{
			const FFXTreadLayer& L = P.Layers[C.Layer];
			if (!bK2On && L.Name == TEXT("K2"))
			{
				continue;
			}
			const double K = C.Age / FMath::Max(C.Life, 1e-3);
			// a curva do WARNO: entra em Attack da vida, segura, e cai de Decay até o fim
			double A = L.Alpha * C.A * C.GA * SmoothStep(K, 0.0, FMath::Max(L.Attack, 1e-3)) * (1.0 - SmoothStep(K, L.Decay, 1.0));
			if (L.MaskEnd > 0.0)
			{
				const double D = (Odo.FindRef(C.Src) - C.Odo0) / P.HullLen;
				A *= 1.0 - SmoothStep(D, L.MaskDense, L.MaskEnd);
			}
			const double Frame = FrameAt(C.Bake0 + C.Age * L.BakeRate);
			const int32 Fi = FMath::Clamp(FMath::FloorToInt32(Frame), 0, Meta.Frames - 1);
			FCardOut O;
			O.P = C.P;
			O.Size = Meta.SideM * L.Scale * C.S * C.GS * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake;
			O.Frame = Frame;
			O.Rot = C.Rot;
			O.Flip = C.Flip;
			O.Alpha = FMath::Min(1.0, A);
			O.Erode = L.Erode * SmoothStep(K, 0.35, 1.0);
			O.Back = C.Back;
			O.PivotX = Meta.Pivot.X;
			const int32 Fj = FMath::Min(Fi + 1, Meta.Frames - 1);
			O.PivotY = L.PivotBottom > 0.5 && Meta.BallBottom.IsValidIndex(Fj)
				? FMath::Lerp(Meta.BallBottom[Fi], Meta.BallBottom[Fj], FMath::Clamp(Frame - Fi, 0.0, 1.0)) : Meta.Pivot.Y;
			O.Sun = C.Sun < 0.0 ? 1.0 : C.Sun;
			O.CamOffset = L.CamOffset;
			O.Orient = L.Orient;
			O.ShadowK = L.Shadow;
			O.BaseShade = L.BaseShade;
			O.GroundZ = C.GroundZ;
			Out[int32(C.Ground)].Add(O);
			continue;
		}
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
	// a sombra no chão: cada cartão de nuvem deitado, o "pra cima" dele indo pra longe do sol, esticado pela
	// altura × cot(elevação) (a sombra de uma nuvem de altura h com o sol a e graus tem h·cot e de comprimento)
	TArray<FVector> SV, SN;
	TArray<int32> STri;
	TArray<FVector2D> SU0, SU1;
	TArray<FLinearColor> SCl;
	TArray<FProcMeshTangent> STg;
	const FVector Sh = FVector(Sd.X, Sd.Y, 0.0).GetSafeNormal();
	const FVector SA = -Sh, SR(-SA.Y, SA.X, 0.0);
	const double Cot = FMath::Min(FMath::Sqrt(FMath::Max(0.0, 1.0 - Sd.Z * Sd.Z)) / FMath::Max(Sd.Z, 1e-3), 6.0);
	for (int32 G = 0; G < 4; ++G)
	{
		TArray<FCardOut>& O = Out[G];
		O.Sort([&CamM](const FCardOut& A, const FCardOut& B) { return FVector::DistSquared(A.P, CamM) > FVector::DistSquared(B.P, CamM); });
		TArray<FVector> V, Nrm;
		TArray<int32> Tri;
		TArray<FVector2D> U0, U1, U2, U3;
		TArray<FLinearColor> Cl;
		TArray<FProcMeshTangent> Tg;
		int32 VertsPerCardOut = 4;
		for (const FCardOut& Cd : O)
		{
			// orient: a gravação tem o rastro pra direita do cartão (o vento da simulação); o cartão gira pra direita
			// dele apontar pra trás do tanque, como ele aparece na tela. Senão, o giro sorteado
			double Rot = Cd.Rot, Flip = Cd.Flip, XScale = 1.0;
			if (Cd.Orient >= 0.0)
			{
				// a poeira em camadas: o lado mais cheio da gravação (Flip do perfil: −1 = esquerda) vai pro lado de trás
				// do tanque na tela, por espelho (girar viraria a nuvem de cabeça pra baixo); quanto menos o rastro corre
				// de lado na tela, mais estreito o cartão (até o seno da inclinação da câmera)
				if (Cd.Orient > 0.5 && !Cd.Back.IsNearlyZero())
				{
					const double Bx = FVector::DotProduct(Cd.Back, CamRight);
					Flip = (Bx >= 0.0 ? 1.0 : -1.0) * (Cd.Flip < 0.0 ? -1.0 : 1.0);
					XScale = FMath::Max(FMath::Abs(Bx), FMath::Abs(CamForward.Z));
				}
			}
			else if (P.Orient > 0.5 && !Cd.Back.IsNearlyZero())
			{
				Rot = FMath::Atan2(FVector::DotProduct(Cd.Back, CamUp), FVector::DotProduct(Cd.Back, CamRight)) + Cd.Rot * 0.25;
				Flip = 1.0;
			}
			const double Cs = FMath::Cos(Rot), Sn = FMath::Sin(Rot);
			// squashUp: achata o cartão na vertical da tela (a poeira rente ao chão, vista da câmera do RTS)
			const FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp;
			const FVector L3(FVector::DotProduct(Sd, R) * Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, ToCam));
			const FLinearColor Lc(L3.X * 0.5 + 0.5, L3.Y * 0.5 + 0.5, L3.Z * 0.5 + 0.5, 1.0f);
			const double PvX = Cd.PivotX >= 0.0 ? Cd.PivotX : Meta.Pivot.X, PvY = Cd.PivotY >= 0.0 ? Cd.PivotY : Meta.Pivot.Y;
			const FVector2D Pivot(Flip > 0 ? PvX - 0.5 : 0.5 - PvX, PvY - 0.5);
			// pra câmera: o cartão vem CamOffset m pelo raio da câmera e encolhe na mesma conta (o mesmo tamanho na tela);
			// assim o pé dele não entra no chão nem some na partícula macia
			FVector Pc = Cd.P;
			double Sz = Cd.Size;
			if (Cd.CamOffset > 0.0)
			{
				const double Dc = FVector::Dist(CamM, Cd.P);
				const double Off = FMath::Min(Cd.CamOffset, Dc * 0.5);
				Pc = Cd.P + (CamM - Cd.P) / FMath::Max(Dc, 1e-3) * Off;
				Sz *= (Dc - Off) / FMath::Max(Dc, 1e-3);
			}
			const int32 Base = V.Num();
			// a poeira em camadas desenha só a caixa do alfa dos dois quadros que se misturam (atlas_bbox.py): o mesmo
			// visual com ~1/3 dos pixels. A caixa vem em coordenadas de textura; o canto do cartão segue o espelho
			FVector2D Qs[4] = { Corners[0], Corners[1], Corners[2], Corners[3] };
			if (Cd.Orient >= 0.0 && Meta.BBox.Num() == Meta.Frames)
			{
				const int32 F0 = FMath::Clamp(FMath::FloorToInt32(Cd.Frame), 0, Meta.Frames - 1), F1 = FMath::Min(F0 + 1, Meta.Frames - 1);
				const FVector4 &A = Meta.BBox[F0], &B = Meta.BBox[F1];
				const double U0b = FMath::Min(A.X, B.X), V0b = FMath::Min(A.Y, B.Y), U1b = FMath::Max(A.Z, B.Z), V1b = FMath::Max(A.W, B.W);
				const double X0 = Flip < 0 ? 0.5 - U1b : U0b - 0.5, X1 = Flip < 0 ? 0.5 - U0b : U1b - 0.5;
				Qs[0] = FVector2D(X0, V0b - 0.5); Qs[1] = FVector2D(X1, V0b - 0.5); Qs[2] = FVector2D(X1, V1b - 0.5); Qs[3] = FVector2D(X0, V1b - 0.5);
			}
			// o octógono (atlas_cut8.py) no lugar da caixa: −24 % de pixel, o mesmo visual (só sai pixel de alfa zero).
			// A luz de cada vértice sai da mesma função linear da caixa (o resultado não muda)
			TArray<FVector2D, TInlineAllocator<8>> Pts;
			const int32 Fc = FMath::Clamp(FMath::FloorToInt32(Cd.Frame), 0, Meta.Frames - 1);
			const bool bOct = bCut8 && Cd.Orient >= 0.0 && Meta.Cut8.Num() == Meta.Frames && Meta.Cut8[Fc].Num() == 8;
			if (bOct)
			{
				for (const FVector2D& Uv : Meta.Cut8[Fc])
				{
					Pts.Add(FVector2D(Flip < 0 ? 0.5 - Uv.X : Uv.X - 0.5, Uv.Y - 0.5));
				}
			}
			else
			{
				for (const FVector2D& Q : Qs) Pts.Add(Q);
			}
			const double BoxY0 = Qs[0].Y, BoxY1 = Qs[2].Y;
			auto Hof = [&](double Y) { return FMath::Clamp((Y + 0.5 - PvY) / FMath::Max(1.0 - PvY, 1e-3), 0.0, 1.0); };
			const double H0 = Hof(BoxY0), H1 = Hof(BoxY1);
			VertsPerCardOut = Pts.Num();
			for (const FVector2D& Q : Pts)
			{
				const FVector2D Qp = Q - Pivot;
				V.Add((Pc + (R * (Qp.X * XScale) + Up * (Qp.Y * P.SquashUp)) * Sz) * 100.0 - Origin100);
				Nrm.Add(ToCam);
				U0.Add(FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5));
				U1.Add(FVector2D(Cd.Frame, Cd.Alpha));
				U2.Add(FVector2D(0.0, 0.0));
				// a poeira em camadas: o sol que passa por este canto do cartão (a sombra do tanque); a de 13/10 fica 1
				double SunVis = Cd.Orient >= 0.0 ? Cd.Sun : 1.0;
				if (Cd.BaseShade < 1.0)
				{
					// o pé da nuvem, junto do chão, recebe menos sol que o topo (a própria nuvem e o chão tapam): de
					// BaseShade no chão (o pivô) a 1 no topo do cartão
					const double Hh = FMath::Lerp(H0, H1, FMath::Clamp((Q.Y - BoxY0) / FMath::Max(BoxY1 - BoxY0, 1e-6), 0.0, 1.0));
					SunVis *= FMath::Lerp(Cd.BaseShade, 1.0, Hh);
				}
				U3.Add(FVector2D(Cd.Erode, SunVis));
				Cl.Add(Lc);
			}
			Tri.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
			if (Cd.ShadowK > 0.0 && ShadowMid && bShadowOn && !Sh.IsNearlyZero())
			{
				const int32 Sb = SV.Num();
				for (const FVector2D& Q : Qs)
				{
					const FVector2D Qp = Q - Pivot;
					const FVector Gp(Cd.P.X, Cd.P.Y, Cd.GroundZ + 0.05);
					SV.Add((Gp + SR * (Qp.X * Cd.Size) + SA * (Qp.Y * Cd.Size * Cot)) * 100.0 - Origin100);
					SN.Add(FVector::UpVector);
					SU0.Add(FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5));
					SU1.Add(FVector2D(Cd.Frame, Cd.Alpha * Cd.ShadowK * Cd.Sun));     // dentro da sombra do tanque não escurece de novo
					SCl.Add(FLinearColor::White);
				}
				STri.Append({ Sb, Sb + 1, Sb + 2, Sb, Sb + 2, Sb + 3 });
			}
		}
		const bool bEmpty = V.Num() == 0;
		if (!bEmpty || !bMeshEmpty[G])
		{
			PushMesh(Layers[G].Mesh, G, V, Nrm, U0, U1, U2, U3, Cl, VertsPerCardOut);
		}
		bMeshEmpty[G] = bEmpty;
	}
	if (ShadowMesh && (SV.Num() || !bMeshEmpty[4]))
	{
		bMeshEmpty[4] = SV.Num() == 0;
		TArray<FVector2D> Empty2, Empty3;
		PushMesh(ShadowMesh, 4, SV, SN, SU0, SU1, Empty2, Empty3, SCl);
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
		const double C0 = FPlatformTime::Seconds();
		if (bLabDrive)
		{
			LabPose(Dt);
		}
		StepDust(Dt);
		LastStepMs = (FPlatformTime::Seconds() - C0) * 1000.0;
		LastCpuMs = LastStepMs;
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
		if (bRig)
		{
			ApplyRig();
		}
		else if (APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
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
	const double D0 = FPlatformTime::Seconds();
	Draw(CamLoc / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
	LastDrawMs = (FPlatformTime::Seconds() - D0) * 1000.0;
	LastCpuMs += LastDrawMs;
	if (!MeasureName.IsEmpty() && !bWarming && !ABKey.IsEmpty())
	{
		// A/B no mesmo processo: 3 s pra assentar, depois a chave troca a cada 2 s (o 1º meio segundo de cada troca não
		// conta: a placa entrega o tempo com atraso); 12 pares; a diferença é a das medianas de cada par
		MeasT = MeasT < 0.0 ? 0.0 : MeasT + DeltaSeconds;
		if (MeasT > 3.0)
		{
			// a cena congela (a simulação e a câmera param): só a chave muda entre A e B, o fundo é o mesmo
			bPaused = true;
			const int32 Slot = int32(FMath::FloorToInt32((MeasT - 3.0) / 2.0));
			const bool bState = (Slot % 2) == 1;
			if (bState != bABState || Slot == 0 && ABClock == 0.0)
			{
				if (Slot > 0 && Slot % 2 == 0 && ABGpu[0].Num() && ABGpu[1].Num())
				{
					auto Med = [](TArray<float> V) { V.Sort(); return V.Num() ? V[V.Num() / 2] : 0.0f; };
					ABPairs.Add(Med(ABGpu[1]) - Med(ABGpu[0]));
				}
				if (Slot % 2 == 0)
				{
					ABGpu[0].Reset(); ABGpu[1].Reset();
				}
				ApplyAB(bState);
				ABClock = MeasT;
			}
			if (MeasT - ABClock > 0.5)
			{
				ABGpu[bState ? 1 : 0].Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
				ABCpu[bState ? 1 : 0].Add(float(LastCpuMs));
			}
			if (ABPairs.Num() >= 12)
			{
				TArray<double> S = ABPairs;
				S.Sort();
				const double Med = S[S.Num() / 2];
				double Mean = 0.0;
				for (double X : S) Mean += X;
				Mean /= S.Num();
				auto MedF = [](TArray<float> V) { V.Sort(); return V.Num() ? V[V.Num() / 2] : 0.0f; };
				const FString Out = FString::Printf(TEXT("%s A/B '%s' (%d tanques): placa ligado − desligado: mediana dos pares %+.2f ms, média %+.2f ms, mín %+.2f máx %+.2f (%d pares); CPU da poeira desligado %.2f ms, ligado %.2f ms\n"),
					*MeasureName, *ABKey, NumTanks, Med, Mean, S[0], S.Last(), S.Num(), MedF(ABCpu[0]), MedF(ABCpu[1]));
				FFileHelper::SaveStringToFile(Out, *(RepoDir() / TEXT("captures/fxlab") / (TEXT("ab_tread_") + MeasureName + TEXT(".txt"))));
				UE_LOG(LogTemp, Display, TEXT("FXTread: %s"), *Out);
				MeasureName.Reset();
				FGenericPlatformMisc::RequestExit(false);
			}
		}
	}
	if (!MeasureName.IsEmpty() && !bWarming && ABKey.IsEmpty())
	{
		// 3 s pra assentar, depois 10 s de amostras; o resultado vai pra captures/fxlab/measure_tread_<nome>.txt
		MeasT = MeasT < 0.0 ? 0.0 : MeasT + DeltaSeconds;
		if (MeasT > 3.0)
		{
			MeasMs.Add(DeltaSeconds * 1000.0f);
			MeasGpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
			MeasCpu.Add(float(PluginDust ? PluginDust->LastMs : LastCpuMs));
			MeasStep.Add(float(LastStepMs));
			MeasDraw.Add(float(LastDrawMs - LastPushMs));
			MeasPush.Add(float(LastPushMs));
		}
		if (MeasT > 13.0)
		{
			auto Stat = [](TArray<float> V, float Q) { V.Sort(); return V.Num() ? V[FMath::Clamp(int32(Q * (V.Num() - 1)), 0, V.Num() - 1)] : 0.0f; };
			int32 NCards = PluginDust ? PluginDust->NumCards() : Cards.Num();
			const FString Out = FString::Printf(TEXT("%s: %d tanques, %d cartões; quadro mediana %.2f ms p95 %.2f ms; placa mediana %.2f ms p95 %.2f ms; poeira na CPU (Step + Draw) mediana %.2f ms p95 %.2f ms [Step %.2f, montar %.2f, malha %.2f] (%d amostras)\n"),
				*MeasureName, NumTanks, NCards, Stat(MeasMs, 0.5f), Stat(MeasMs, 0.95f), Stat(MeasGpu, 0.5f), Stat(MeasGpu, 0.95f),
				Stat(MeasCpu, 0.5f), Stat(MeasCpu, 0.95f), Stat(MeasStep, 0.5f), Stat(MeasDraw, 0.5f), Stat(MeasPush, 0.5f), MeasMs.Num());
			FFileHelper::SaveStringToFile(Out, *(RepoDir() / TEXT("captures/fxlab") / (TEXT("measure_tread_") + MeasureName + TEXT(".txt"))));
			UE_LOG(LogTemp, Display, TEXT("FXTread: %s"), *Out);
			MeasureName.Reset();
			FGenericPlatformMisc::RequestExit(false);
		}
	}
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
