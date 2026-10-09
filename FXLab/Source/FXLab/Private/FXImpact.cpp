#include "FXImpact.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
	// tipos de primitiva (os mesmos números do laboratório: PRIM_KIND)
	enum : int32 { KFlash = 0, KSpark = 1, KChunk = 3, KCrater = 4 };

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

	struct FCardOut { FVector P; double Size, Frame, Rot, Flip, Alpha, Erode; bool bGround = false; double Stretch = 1.0; };
	struct FPrimOut { int32 Kind; FVector P, Dir = FVector(0, 0, 1); double Len = 0, W = 0, Size = 0, Gain = 0, Alpha = 1; FLinearColor Col = FLinearColor::Black; };

	const FVector2D Corners[4] = { { -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } };
}

bool FFXImpactPreset::Load(const FString& Path)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return false;
	}
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J)
	{
		return false;
	}
	J->TryGetStringField(TEXT("dustBake"), DustBake);
	J->TryGetStringField(TEXT("ringBake"), RingBake);
	J->TryGetStringField(TEXT("geyserBake"), GeyserBake);
	const TPair<const TCHAR*, double*> Fields[] = {
		{ TEXT("dirAz"), &DirAz }, { TEXT("flashSize"), &FlashSize }, { TEXT("flashTime"), &FlashTime }, { TEXT("flashGain"), &FlashGain },
		{ TEXT("flashLight"), &FlashLight }, { TEXT("sparkCount"), &SparkCount }, { TEXT("sparkSpeed"), &SparkSpeed },
		{ TEXT("sparkLife"), &SparkLife }, { TEXT("chunkCount"), &ChunkCount }, { TEXT("chunkSpeed"), &ChunkSpeed },
		{ TEXT("chunkSize"), &ChunkSize }, { TEXT("dustScale"), &DustScale }, { TEXT("dustSpeed"), &DustSpeed },
		{ TEXT("dustLife"), &DustLife }, { TEXT("ringScale"), &RingScale }, { TEXT("ringSpeed"), &RingSpeed },
		{ TEXT("ringLife"), &RingLife }, { TEXT("ringStretch"), &RingStretch }, { TEXT("ringShift"), &RingShift }, { TEXT("ringAlbedo"), &RingAlbedo }, { TEXT("geyserScale"), &GeyserScale }, { TEXT("geyserSpeed"), &GeyserSpeed }, { TEXT("geyserFade"), &GeyserFade },
		{ TEXT("craterRadius"), &CraterRadius }, { TEXT("sandR"), &SandR }, { TEXT("sandG"), &SandG }, { TEXT("sandB"), &SandB },
		{ TEXT("dustDensity"), &DustDensity }, { TEXT("dustAlbedo"), &DustAlbedo }, { TEXT("geyserAlbedo"), &GeyserAlbedo },
		{ TEXT("softDist"), &SoftDist }, { TEXT("originX"), &OriginX }, { TEXT("originY"), &OriginY }, { TEXT("gravity"), &Gravity },
	};
	for (const auto& F : Fields)
	{
		J->TryGetNumberField(F.Key, *F.Value);
	}
	return true;
}

AFXImpact::AFXImpact()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	auto MakeMesh = [this, Root](const TCHAR* Name, int32 SortPriority)
	{
		UProceduralMeshComponent* M = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->SetCastShadow(false);
		// a ordem do laboratório (renderOrder): anel por baixo, poeira, jato, primitivas por cima
		M->SetTranslucentSortPriority(SortPriority);
		return M;
	};
	RingCards = MakeMesh(TEXT("RingCards"), 0);
	DustCards = MakeMesh(TEXT("DustCards"), 1);
	GeyserCards = MakeMesh(TEXT("GeyserCards"), 2);
	Prims = MakeMesh(TEXT("Prims"), 3);
	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(Root);
	FlashLight->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(0.0f);
	FlashLight->SetAttenuationRadius(3000.0f);
	FlashLight->bUseInverseSquaredFalloff = true;
	FlashLight->SetLightColor(FColor(255, 138, 58));
	FlashLight->SetCastShadows(false);
}

void AFXImpact::BeginPlay()
{
	Super::BeginPlay();
	Ring.Mesh = RingCards;
	Dust.Mesh = DustCards;
	Geyser.Mesh = GeyserCards;
	Light = FFXSceneLight::Find(GetWorld());
	FString Fx;
	FParse::Value(FCommandLine::Get(), TEXT("-Fx="), Fx);
	FString At;
	const bool bShot = FParse::Value(FCommandLine::Get(), TEXT("-Shot="), ShotName);
	bFixedStep = bShot && (Light.bGame || Fx == TEXT("impact"));
	// no mapa do jogo quem fotografa é o controlador (vários tiros na tela)
	if (bShot && !Light.bGame && Fx == TEXT("impact") && FParse::Value(FCommandLine::Get(), TEXT("-At="), At, false))
	{
		bCapture = true;
		TArray<FString> Items;
		At.ParseIntoArray(Items, TEXT(","));
		for (const FString& S : Items)
		{
			ShotAt.Add(FCString::Atod(*S));
		}
	}
	RngState = uint32(Seed);
	LoadPreset(true);
	if (FirstShot > 0.0)
	{
		Refire = 5.0 - FirstShot;      // o repetidor de 5 s dispara o 1º
	}
	else
	{
		Fire();
	}
}

void AFXImpact::LoadLayer(FLayer& L, const FString& Bake)
{
	if (L.Bake == Bake && L.Mid)
	{
		return;
	}
	L.Bake = Bake;
	if (!L.Meta.Load(RepoDir() / TEXT("captures/fxlab/bake") / Bake / TEXT("atlas.json")))
	{
		UE_LOG(LogTemp, Error, TEXT("FXLab: falta o atlas.json de %s"), *Bake);
	}
	L.Fps = (L.Meta.Frames - 1) / FMath::Max(L.Meta.LastTime, 1e-3);
	if (!L.Mid && CardMaterial)
	{
		L.Mid = UMaterialInstanceDynamic::Create(CardMaterial, this);
		Mids.Add(L.Mid);
		L.Mesh->SetMaterial(0, L.Mid);
	}
	if (L.Mid)
	{
		const FString Dir = FString::Printf(TEXT("/Game/FX/%s/"), *Bake);
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
		L.Mid->SetVectorParameterValue(TEXT("Grid"), FLinearColor(L.Meta.Cols, L.Meta.Rows, L.Meta.Frames, 0.0f));
	}
}

void AFXImpact::LoadPreset(bool bForce)
{
	const FString Path = FPaths::ProjectDir() / TEXT("impact.json");
	const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*Path);
	if (!bForce && Stamp == PresetStamp)
	{
		return;
	}
	PresetStamp = Stamp;
	P.Load(Path);
	Scene.Load(FPaths::ProjectDir() / TEXT("preset.json"));
	if (bPlacedInWorld)
	{
		Origin = GetActorLocation() / 100.0;
	}
	else
	{
		Origin = FVector(P.OriginX, P.OriginY, 0.0);
		SetActorLocation(Origin * 100.0);
	}
	if (!PrimMid && PrimMaterial)
	{
		PrimMid = UMaterialInstanceDynamic::Create(PrimMaterial, this);
		Prims->SetMaterial(0, PrimMid);
	}
	LoadLayer(Ring, P.RingBake);
	LoadLayer(Dust, P.DustBake);
	LoadLayer(Geyser, P.GeyserBake);
	ApplyPreset();
}

void AFXImpact::ApplyPreset()
{
	const FLinearColor SunCol = FLinearColor::FromSRGBColor(FColor(255, 198, 140)) * float(Scene.Sun);
	const FLinearColor SkyCol = FLinearColor::FromSRGBColor(FColor(159, 180, 208)) * float(Scene.Sky);
	const FLinearColor Tint = GroundTint.A > 0.0f ? FLinearColor(GroundTint.R, GroundTint.G, GroundTint.B, 1.0f)
		: FLinearColor(P.SandR, P.SandG, P.SandB, 1.0f);
	for (FLayer* L : { &Ring, &Dust, &Geyser })
	{
		if (!L->Mid)
		{
			continue;
		}
		// poeira clara e rala cor de areia; o jato é escuro; sem caule; partícula macia
		L->Mid->SetVectorParameterValue(TEXT("SunCol"), SunCol);
		L->Mid->SetVectorParameterValue(TEXT("SkyCol"), SkyCol);
		L->Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
		L->Mid->SetScalarParameterValue(TEXT("Density"), P.DustDensity);
		L->Mid->SetScalarParameterValue(TEXT("Albedo"), L == &Geyser ? P.GeyserAlbedo : L == &Ring ? P.RingAlbedo : P.DustAlbedo);
		L->Mid->SetScalarParameterValue(TEXT("EmitGain"), 0.0f);
		L->Mid->SetScalarParameterValue(TEXT("EmberCut"), Scene.EmberCut);
		L->Mid->SetScalarParameterValue(TEXT("SoftDist"), L == &Ring ? 0.0 : P.SoftDist * 100.0);    // o anel é rente ao chão
		L->Mid->SetScalarParameterValue(TEXT("SceneLight"), Light.bGame ? 1.0f : 0.0f);
		L->Mid->SetScalarParameterValue(TEXT("EmitScale"), float(Light.EmitScale));
	}
	if (PrimMid)
	{
		PrimMid->SetVectorParameterValue(TEXT("SunCol"), SunCol);
		PrimMid->SetVectorParameterValue(TEXT("SkyCol"), SkyCol);
		PrimMid->SetScalarParameterValue(TEXT("SceneLight"), Light.bGame ? 1.0f : 0.0f);
		PrimMid->SetScalarParameterValue(TEXT("EmitScale"), float(Light.EmitScale));
	}
}

// mulberry32, igual ao laboratório
double AFXImpact::Rand()
{
	RngState += 0x6D2B79F5u;
	uint32 X = RngState;
	X = (X ^ (X >> 15)) * (X | 1u);
	X ^= X + (X ^ (X >> 7)) * (X | 61u);
	return double(X ^ (X >> 14)) / 4294967296.0;
}

double AFXImpact::Rnd(double A, double B)
{
	return A + Rand() * (B - A);
}

// direção num cone: eixo = vertical inclinado `tilt` graus pra frente; abertura `spread`
FVector AFXImpact::Cone(const FVector& Fwd, double TiltDeg, double SpreadDeg)
{
	const double Tilt = FMath::DegreesToRadians(TiltDeg), Sp = FMath::DegreesToRadians(SpreadDeg);
	const double A = Rnd(0.0, PI * 2.0);
	const double R = FMath::Sqrt(Rnd(0.0, 1.0)) * Sp;
	const FVector Ax(Fwd.X * FMath::Sin(Tilt), Fwd.Y * FMath::Sin(Tilt), FMath::Cos(Tilt));
	const FVector Side(-Fwd.Y, Fwd.X, 0.0);
	const FVector Up2(Ax.Y * Side.Z - Ax.Z * Side.Y, Ax.Z * Side.X - Ax.X * Side.Z, Ax.X * Side.Y - Ax.Y * Side.X);
	const double C = FMath::Cos(R), S = FMath::Sin(R);
	return Ax * C + (Side * FMath::Cos(A) + Up2 * FMath::Sin(A)) * S;
}

void AFXImpact::Fire()
{
	T = 0.0;
	Refire = 0.0;
	Parts.Reset();
	const double Az = FMath::DegreesToRadians(P.DirAz);
	const FVector Fwd(FMath::Cos(Az), FMath::Sin(Az), 0.0);
	// uma chamada por linha: a ordem dos sorteios é a do laboratório
	for (int32 i = 0; i < FMath::CeilToInt32(P.SparkCount); ++i)
	{
		FPart Q;
		const FVector D = Cone(Fwd, 10.0, 55.0);
		const double V = P.SparkSpeed * Rnd(0.55, 1.3);
		Q.P = Origin;
		Q.V = D * V;
		Q.Life = P.SparkLife * Rnd(0.5, 1.2);
		Q.W = 0.04;
		Parts.Add(Q);
	}
	for (int32 i = 0; i < FMath::CeilToInt32(P.ChunkCount); ++i)
	{
		FPart Q;
		Q.bSpark = false;
		const FVector D = Cone(Fwd, 12.0, 50.0);
		const double V = P.ChunkSpeed * Rnd(0.35, 1.3);
		Q.P = Origin + FVector(0.0, 0.0, 0.1);
		Q.V = D * V;
		Q.W = P.ChunkSize * Rnd(0.5, 1.6);
		Parts.Add(Q);
	}
	DustRot = Rnd(-0.1, 0.1);
	DustFlip = Rand() < 0.5 ? -1.0 : 1.0;
	RingRot = Rnd(-0.05, 0.05);
}

void AFXImpact::Step(double Dt)
{
	if (T < 0.0)
	{
		return;
	}
	T += Dt;
	for (FPart& Q : Parts)
	{
		if (!Q.bSpark && Q.Landed >= 0.0)
		{
			continue;
		}
		Q.V.Z -= P.Gravity * Dt;
		if (Q.bSpark)
		{
			Q.V *= (1.0 - 1.5 * Dt);
		}
		Q.P += Q.V * Dt;
		if (!Q.bSpark && Q.P.Z <= Origin.Z + Q.W * 0.3)
		{
			Q.P.Z = Origin.Z + Q.W * 0.3;
			Q.Landed = T;
		}
	}
}

void AFXImpact::Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward)
{
	TArray<FCardOut> RingOut, DustOut, GeyserOut;
	TArray<FPrimOut> PrimOut;
	double LightCd = 0.0;
	if (T >= 0.0)
	{
		FPrimOut Cr;
		Cr.Kind = KCrater;
		Cr.P = Origin + FVector(0.0, 0.0, 0.08);
		Cr.Size = P.CraterRadius * 2.0 * FMath::Min(1.0, T / 0.05);
		PrimOut.Add(Cr);
		const double Fl = FMath::Exp(-T / (P.FlashTime * 0.5)) * (T < P.FlashTime * 3.0 ? 1.0 : 0.0);
		if (Fl > 0.002)
		{
			FPrimOut F;
			F.Kind = KFlash;
			F.P = Origin + FVector(0.0, 0.0, 0.6);
			F.Size = P.FlashSize * (0.6 + 0.4 * FMath::Min(1.0, T / 0.04));
			F.Gain = P.FlashGain * Fl;
			PrimOut.Add(F);
		}
		LightCd = P.FlashLight * Fl;
		for (const FPart& Q : Parts)
		{
			if (Q.bSpark)
			{
				if (T > Q.Life) continue;
				const double Sp = Q.V.Size(), K = T / Q.Life;
				FPrimOut O;
				O.Kind = KSpark;
				O.P = Q.P;
				O.Dir = Sp > 1e-4 ? Q.V / Sp : FVector(0.0, 0.0, 1.0);
				O.Len = FMath::Max(0.15, Sp * 0.03);
				O.W = Q.W;
				O.Gain = 8.0 * (1.0 - K * K);
				PrimOut.Add(O);
			}
			else if (Q.Landed >= 0.0)
			{
				// a pedra voa dentro do jato gravado; aqui ela só aparece quando cai e fica no chão
				FPrimOut O;
				O.Kind = KChunk;
				O.P = Q.P;
				O.W = Q.W;
				O.Alpha = FMath::Min(1.0, (T - Q.Landed) / 0.1) * (1.0 - SmoothStep(T, 4.0, 5.0));
				O.Col = FLinearColor(0.045f, 0.038f, 0.032f);
				PrimOut.Add(O);
			}
		}
		{
			const FFXBakeMeta& M = Dust.Meta;
			const double Life = P.DustLife, K = FMath::Min(1.0, T / Life);
			if (T <= Life)
			{
				DustOut.Add({ Origin, M.SideM * P.DustScale, FMath::Min(double(M.Frames - 1), T * P.DustSpeed * Dust.Fps), DustRot, DustFlip,
					FMath::Min(1.0, T / 0.05) * (1.0 - SmoothStep(K, 0.8, 1.0)), 0.3 * SmoothStep(K, 0.5, 1.0) });
			}
		}
		{
			// deitado no chão, de câmera baixa ele vira uma faixa escura: some abaixo de ~25°
			const FFXBakeMeta& M = Ring.Meta;
			const double Life = P.RingLife, K = FMath::Min(1.0, T / Life);
			const double Hd = FMath::Sqrt(FMath::Square(CamM.X - Origin.X) + FMath::Square(CamM.Y - Origin.Y));
			const double Seen = SmoothStep(FMath::Atan2(CamM.Z - Origin.Z, Hd), 0.3, 0.55);
			if (T <= Life)
			{
				// deitado no chão (gravado de cima), um pouco acima pra não brigar com a areia
				// a poeira vai mais pra frente do tiro (o crescente da arte): esticada e deslocada na direção dele
				const double Az = FMath::DegreesToRadians(P.DirAz), Sh = P.RingShift * FMath::Min(1.0, T / 0.6);
				RingOut.Add({ Origin + FVector(FMath::Cos(Az) * Sh, FMath::Sin(Az) * Sh, 0.25), M.SideM * P.RingScale,
					FMath::Min(double(M.Frames - 1), T * P.RingSpeed * Ring.Fps), Az + RingRot, 1.0,
					FMath::Min(1.0, T / 0.08) * (1.0 - SmoothStep(K, 0.7, 1.0)) * Seen, 0.35 * SmoothStep(K, 0.5, 1.0), true, P.RingStretch });
			}
		}
		{
			// o cozimento abre pra direita do quadro; espelha se o tiro vai pra esquerda da tela
			const FFXBakeMeta& M = Geyser.Meta;
			const double F = T * P.GeyserSpeed * Geyser.Fps;
			const double Az = FMath::DegreesToRadians(P.DirAz);
			const double Flip = FMath::Cos(Az) * CamRight.X + FMath::Sin(Az) * CamRight.Y >= 0.0 ? 1.0 : -1.0;
			if (F <= M.Frames - 1)
			{
				GeyserOut.Add({ Origin, M.SideM * P.GeyserScale, F, 0.0, Flip, 1.0 - SmoothStep(F, (M.Frames - 1) * P.GeyserFade, (M.Frames - 1) * (P.GeyserFade + 0.4)), 0.0 });
			}
		}
		PrimOut.StableSort([&CamM](const FPrimOut& A, const FPrimOut& B)
		{
			if (A.Kind == KCrater) return B.Kind != KCrater;
			if (B.Kind == KCrater) return false;
			return FVector::DistSquared(A.P, CamM) > FVector::DistSquared(B.P, CamM);
		});
	}

	const FVector Origin100 = GetActorLocation();
	const FVector ToCam = -CamForward;
	const FVector Sd = SunDirection();
	// cartões 6-way (a mesma montagem do fogo da carcaça; sem caule, sem fogo)
	auto BuildCards = [&](FLayer& L, const TArray<FCardOut>& Out)
	{
		TArray<FVector> V, Nrm;
		TArray<int32> Tri;
		TArray<FVector2D> U0, U1, U2, U3;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tg;
		for (const FCardOut& Cd : Out)
		{
			const double Cs = FMath::Cos(Cd.Rot), Sn = FMath::Sin(Cd.Rot);
			FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp, Front = ToCam;
			if (Cd.bGround)
			{
				// o plano do chão (o laboratório: x e z do three = x e y da Unreal); a frente é o céu
				R = FVector(Cs, Sn, 0.0);
				Up = FVector(-Sn, Cs, 0.0);
				Front = FVector(0.0, 0.0, 1.0);
			}
			const FVector L3(FVector::DotProduct(Sd, R) * Cd.Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, Front));
			const FLinearColor Lc(L3.X * 0.5 + 0.5, L3.Y * 0.5 + 0.5, L3.Z * 0.5 + 0.5, 1.0f);
			const FVector2D Pivot(Cd.Flip > 0 ? L.Meta.Pivot.X - 0.5 : 0.5 - L.Meta.Pivot.X, L.Meta.Pivot.Y - 0.5);
			const int32 Base = V.Num();
			for (const FVector2D& Q : Corners)
			{
				const FVector2D Qp = Q - Pivot;
				V.Add((Cd.P + (R * Qp.X * Cd.Stretch + Up * Qp.Y) * Cd.Size) * 100.0 - Origin100);
				Nrm.Add(ToCam);
				U0.Add(FVector2D(Cd.Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5));
				U1.Add(FVector2D(Cd.Frame, Cd.Alpha));
				U2.Add(FVector2D(0.0, 0.0));
				U3.Add(FVector2D(Cd.Erode, 1.0));
				C.Add(Lc);
			}
			Tri.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
		L.Mesh->CreateMeshSection_LinearColor(0, V, Tri, Nrm, U0, U1, U2, U3, C, Tg, false, false);
	};
	BuildCards(Ring, RingOut);
	BuildCards(Dust, DustOut);
	BuildCards(Geyser, GeyserOut);
	// primitivas: a mesma montagem do vértice do laboratório (primMat.vertexShader)
	{
		TArray<FVector> V, Nrm;
		TArray<int32> Tri;
		TArray<FVector2D> U0, U1, U2, U3;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tg;
		for (const FPrimOut& Q : PrimOut)
		{
			const int32 Base = V.Num();
			for (const FVector2D& K : Corners)
			{
				FVector Wp;
				if (Q.Kind == KFlash || Q.Kind == KChunk)
				{
					const double S = Q.Kind == KFlash ? Q.Size : Q.W;
					Wp = Q.P + (CamRight * K.X + CamUp * K.Y) * S;
				}
				else if (Q.Kind == KSpark)
				{
					// risco: do ponto pra trás ao longo da velocidade, largura de frente pra câmera
					const FVector Side = FVector::CrossProduct(Q.Dir, ToCam).GetSafeNormal();
					Wp = Q.P + Q.Dir * (K.Y - 0.5) * Q.Len + Side * K.X * Q.W;
				}
				else
				{
					Wp = Q.P + FVector(K.X, K.Y, 0.0) * Q.Size;          // cratera: deitada no chão
				}
				V.Add(Wp * 100.0 - Origin100);
				Nrm.Add(ToCam);
				U0.Add(FVector2D(K.X + 0.5, K.Y + 0.5));
				U1.Add(FVector2D(Q.Kind, Q.Gain));
				U2.Add(FVector2D(Q.Alpha, 0.0));
				U3.Add(FVector2D(0.0, 0.0));
				C.Add(Q.Col);
			}
			Tri.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
		Prims->CreateMeshSection_LinearColor(0, V, Tri, Nrm, U0, U1, U2, U3, C, Tg, false, false);
	}
	FlashLight->SetIntensity(float(LightCd * Light.EmitScale));
}

void AFXImpact::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PresetCheck += DeltaSeconds;
	if (PresetCheck > 1.0 && !bFixedStep)
	{
		PresetCheck = 0.0;
		LoadPreset(false);             // o laboratório salvou um ajuste novo: pega sozinho
	}
	const APlayerCameraManager* Pcm = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Pcm || Dust.Meta.Frames <= 1)
	{
		return;
	}
	// captura: o passo do laboratório (1/30 exato); no laboratório o tiro repete a cada 5 s
	const double Dt = bFixedStep ? 1.0 / 30.0 : DeltaSeconds * TimeScale;
	if (!bPaused)
	{
		Step(Dt);
		Refire += Dt;
		if (!bCapture && bAutoRepeat && Refire > 5.0)
		{
			Fire();
		}
	}
	const FRotationMatrix Rm(Pcm->GetCameraRotation());
	Draw(Pcm->GetCameraLocation() / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
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
