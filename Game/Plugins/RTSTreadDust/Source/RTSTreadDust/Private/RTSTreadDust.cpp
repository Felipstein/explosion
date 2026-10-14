#include "RTSTreadDust.h"

#include "Async/ParallelFor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Pawn.h"
#include "TextureResource.h"
#include "UObject/UObjectIterator.h"
#include "Components/DirectionalLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// A mesma conta do FXLab/Source/FXLab/Private/FXTread.cpp (o caminho "layers"), sem o laboratório (a volta, a câmera
// dos alvos, a medida, o A/B, a grade do asfalto lida do Landscape). Quando a poeira mudar no FXLab, recopiar aqui.

namespace
{
	double SmoothStep(double X, double Lo, double Hi)
	{
		if (X <= Lo) return 0.0;
		if (X >= Hi) return 1.0;
		const double K = (X - Lo) / (Hi - Lo);
		return K * K * (3.0 - 2.0 * K);
	}

	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
		}
		return J;
	}

	FString PluginResources()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RTSTreadDust"));
		return Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Resources") : FString();
	}

	const FVector2D Corners[4] = { { -0.5, -0.5 }, { 0.5, -0.5 }, { 0.5, 0.5 }, { -0.5, 0.5 } };
}

bool FRTSTreadProfile::Load(const FString& Path)
{
	const TSharedPtr<FJsonObject> J = ReadJson(Path);
	if (!J)
	{
		return false;
	}
	J->TryGetStringField(TEXT("bake"), Bake);
	J->TryGetNumberField(TEXT("softDist"), SoftDist);
	J->TryGetNumberField(TEXT("loadTau"), LoadTau);
	J->TryGetNumberField(TEXT("hullLen"), HullLen);
	J->TryGetNumberField(TEXT("windX"), WindX);
	J->TryGetNumberField(TEXT("windY"), WindY);
	const TSharedPtr<FJsonObject>* Sand = nullptr;
	if (J->TryGetObjectField(TEXT("sand"), Sand) && Sand)
	{
		(*Sand)->TryGetNumberField(TEXT("r"), TintR);
		(*Sand)->TryGetNumberField(TEXT("g"), TintG);
		(*Sand)->TryGetNumberField(TEXT("b"), TintB);
		(*Sand)->TryGetNumberField(TEXT("albedo"), Albedo);
		(*Sand)->TryGetNumberField(TEXT("density"), Density);
	}
	Layers.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
	if (J->TryGetArrayField(TEXT("layers"), Ls) && Ls)
	{
		for (const TSharedPtr<FJsonValue>& V : *Ls)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O) continue;
			FRTSTreadLayer L;
			O->TryGetStringField(TEXT("name"), L.Name);
			const TPair<const TCHAR*, double*> F[] = {
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
				{ TEXT("followLoad"), &L.FollowLoad }, { TEXT("loadGain"), &L.LoadGain }, { TEXT("shadow"), &L.Shadow },
				{ TEXT("baseShade"), &L.BaseShade }, { TEXT("scaleSlow"), &L.ScaleSlow }, { TEXT("vFull"), &L.VFull },
			};
			for (const auto& Fd : F)
			{
				O->TryGetNumberField(Fd.Key, *Fd.Value);
			}
			Layers.Add(L);
		}
	}
	return Layers.Num() > 0;
}

bool FRTSTreadAtlas::Load(const FString& Path)
{
	const TSharedPtr<FJsonObject> J = ReadJson(Path);
	if (!J)
	{
		return false;
	}
	Cols = J->GetIntegerField(TEXT("cols"));
	Rows = J->GetIntegerField(TEXT("rows"));
	Frames = J->GetIntegerField(TEXT("frames"));
	SideM = J->GetNumberField(TEXT("side_m"));
	const TArray<TSharedPtr<FJsonValue>>& Pv = J->GetArrayField(TEXT("pivot"));
	Pivot = FVector2D(Pv[0]->AsNumber(), Pv[1]->AsNumber());
	Times.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("times_s"))) Times.Add(V->AsNumber());
	BallBottom.Reset();
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("ball_bottom"))) BallBottom.Add(V->AsNumber());
	BBox.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Bb = nullptr;
	if (J->TryGetArrayField(TEXT("bbox"), Bb) && Bb)
	{
		for (const TSharedPtr<FJsonValue>& V : *Bb)
		{
			const TArray<TSharedPtr<FJsonValue>>& Q = V->AsArray();
			BBox.Add(Q.Num() == 4 ? FVector4(Q[0]->AsNumber(), Q[1]->AsNumber(), Q[2]->AsNumber(), Q[3]->AsNumber()) : FVector4(0, 0, 1, 1));
		}
	}
	Cut8.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Ct = nullptr;
	if (J->TryGetArrayField(TEXT("cut8"), Ct) && Ct)
	{
		for (const TSharedPtr<FJsonValue>& V : *Ct)
		{
			const TArray<TSharedPtr<FJsonValue>>& Q = V->AsArray();
			TArray<FVector2D>& Poly = Cut8.AddDefaulted_GetRef();
			for (int32 I = 0; I + 1 < Q.Num(); I += 2) Poly.Add(FVector2D(Q[I]->AsNumber(), Q[I + 1]->AsNumber()));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
	if (J->TryGetArrayField(TEXT("light_scale"), Ls) && Ls && Ls->Num() == 2)
	{
		LightScale = FVector2D((*Ls)[0]->AsNumber(), (*Ls)[1]->AsNumber());
	}
	return Frames > 1 && Times.Num() == Frames && BallBottom.Num() == Frames;
}

double FRTSTreadAtlas::FrameAt(double BakeT) const
{
	if (Times.Num() < 2 || BakeT <= Times[0])
	{
		return 0.0;
	}
	// busca binária (os tempos crescem)
	int32 Lo = 0, Hi = Times.Num() - 1;
	if (BakeT >= Times[Hi])
	{
		return double(Hi);
	}
	while (Hi - Lo > 1)
	{
		const int32 Mid = (Lo + Hi) / 2;
		(Times[Mid] < BakeT ? Lo : Hi) = Mid;
	}
	return Lo + (BakeT - Times[Lo]) / FMath::Max(Times[Hi] - Times[Lo], 1e-6);
}

ARTSTreadDust::ARTSTreadDust()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;        // depois da câmera do quadro: os cartões viram pra ela
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	CardMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DustCards"));
	ShadowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DustShadow"));
	for (UProceduralMeshComponent* M : { CardMesh.Get(), ShadowMesh.Get() })
	{
		M->SetupAttachment(Root);
		M->SetCastShadow(false);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		M->bUseAsyncCooking = false;
	}
	CardMesh->SetTranslucentSortPriority(1);                 // a sombra (0) por baixo, os cartões por cima
}

void ARTSTreadDust::BeginPlay()
{
	Super::BeginPlay();
	Init();
}

bool ARTSTreadDust::Init()
{
	// na 1ª chamada (o BeginPlay, ou o Advance de quem criou o ator no meio do BeginPlay dele)
	if (bInit)
	{
		return bReady;
	}
	bInit = true;
	SetActorLocation(FVector::ZeroVector);
	RngState = uint32(Seed);
	const FString Res = PluginResources();
	if (!P.Load(Res / TEXT("tread_v12.json")))
	{
		UE_LOG(LogTemp, Error, TEXT("RTSTreadDust: falta o perfil %s"), *(Res / TEXT("tread_v12.json")));
	}
	if (!Atlas.Load(Res / (P.Bake + TEXT(".atlas.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("RTSTreadDust: falta o atlas %s"), *(Res / (P.Bake + TEXT(".atlas.json"))));
	}
	// as folhas: as HDR (16 bits por canal) ou, com -TreadDustSheets=norm8, as normalizadas em BC7 (o A/B da compressão)
	FString Sheets;
	FParse::Value(FCommandLine::Get(), TEXT("-TreadDustSheets="), Sheets);
	const bool bNorm8 = Sheets == TEXT("norm8");
	const FString Dir = FString::Printf(TEXT("/RTSTreadDust/%s%s/"), *P.Bake, bNorm8 ? TEXT("_norm8") : TEXT(""));
	UTexture* SixPos = LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixPos.T_SixPos")));
	UTexture* SixNeg = LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixNeg.T_SixNeg")));
	const FLinearColor Grid(Atlas.Cols, Atlas.Rows, Atlas.Frames, 0.0f);
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/RTSTreadDust/M_TreadDust.M_TreadDust")))
	{
		CardMid = UMaterialInstanceDynamic::Create(M, this);
		CardMid->SetTextureParameterValue(TEXT("SixPos"), SixPos);
		CardMid->SetTextureParameterValue(TEXT("SixNeg"), SixNeg);
		CardMid->SetVectorParameterValue(TEXT("Grid"), Grid);
		CardMid->SetVectorParameterValue(TEXT("LightScale"), bNorm8 ? FLinearColor(Atlas.LightScale.X, Atlas.LightScale.Y, 0.0f, 0.0f) : FLinearColor(1.0f, 1.0f, 0.0f, 0.0f));
		CardMid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(P.TintR, P.TintG, P.TintB, 1.0f));
		CardMid->SetScalarParameterValue(TEXT("Density"), P.Density);
		CardMid->SetScalarParameterValue(TEXT("Albedo"), P.Albedo);
		CardMid->SetScalarParameterValue(TEXT("EmitGain"), 0.0f);
		CardMid->SetScalarParameterValue(TEXT("SoftDist"), P.SoftDist);
		CardMid->SetScalarParameterValue(TEXT("SceneLight"), 1.0f);       // a luz do mapa (o sol e o céu da receita do Look)
		CardMid->SetScalarParameterValue(TEXT("ClipAlpha"), 1.0f / 1024.0f);
		CardMesh->SetMaterial(0, CardMid);
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/RTSTreadDust/M_TreadDustShadow.M_TreadDustShadow")))
	{
		ShadowMid = UMaterialInstanceDynamic::Create(M, this);
		ShadowMid->SetTextureParameterValue(TEXT("SixPos"), SixPos);
		ShadowMid->SetVectorParameterValue(TEXT("Grid"), Grid);
		ShadowMesh->SetMaterial(0, ShadowMid);
	}
	if (!CardMid || !SixPos)
	{
		UE_LOG(LogTemp, Error, TEXT("RTSTreadDust: falta o material ou a folha em /RTSTreadDust (rodar o empacotamento do FXLab)"));
	}
	FindSun();
	bReady = Atlas.Frames > 1 && P.Layers.Num() > 0 && CardMid;
	return bReady;
}

void ARTSTreadDust::Advance(double Dt)
{
	if (Init() && bEnabled)
	{
		if (Sun.IsValid())
		{
			SunDir = -Sun->GetComponentTransform().GetUnitAxis(EAxis::X);
		}
		const double C0 = FPlatformTime::Seconds();
		Step(Dt);
		StepMs += (FPlatformTime::Seconds() - C0) * 1000.0;
	}
}

void ARTSTreadDust::FindSun()
{
	// o sol do mapa: o que ilumina a atmosfera (o diretor do Look); senão o primeiro
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		UDirectionalLightComponent* C = It->FindComponentByClass<UDirectionalLightComponent>();
		if (C && (!Sun.IsValid() || C->IsUsedAsAtmosphereSunLight()))
		{
			Sun = C;
		}
	}
}

double ARTSTreadDust::Rand()
{
	// mulberry32 (o mesmo do FXLab)
	RngState += 0x6D2B79F5u;
	uint32 X = RngState;
	X = (X ^ (X >> 15)) * (X | 1u);
	X ^= X + (X ^ (X >> 7)) * (X | 61u);
	return double(X ^ (X >> 14)) / 4294967296.0;
}

double ARTSTreadDust::SunVisAt(const FVector& Pm, const FVector& ToSun) const
{
	// o tanque em duas caixas (o casco com as lagartas e a torre); só os tanques perto do ponto
	struct FHullBox { double X0, X1, Y0, Y1, Z0, Z1; };
	static const FHullBox Boxes[2] = { { -3.95, 3.95, -1.88, 1.88, 0.0, 1.55 }, { -2.55, 2.27, -1.5, 1.5, 1.55, 2.48 } };
	const double CotEl = ToSun.Z > 0.05 ? FMath::Sqrt(FMath::Max(0.0, 1.0 - ToSun.Z * ToSun.Z)) / ToSun.Z : 1e6;
	for (const FRTSTreadSource& S : Sources)
	{
		const FVector D = Pm - S.PosM;
		if (D.Z > 2.48 && ToSun.Z > 0.0) continue;
		const double Reach = 4.37 + FMath::Max(0.0, 2.48 - D.Z) * CotEl;
		if (D.X * D.X + D.Y * D.Y > Reach * Reach) continue;
		const FVector F = S.FwdM, R(S.FwdM.Y, -S.FwdM.X, 0.0);
		const double O[3] = { FVector::DotProduct(D, F), FVector::DotProduct(D, R), D.Z };
		const double V[3] = { FVector::DotProduct(ToSun, F), FVector::DotProduct(ToSun, R), ToSun.Z };
		for (const FHullBox& B : Boxes)
		{
			const double Lo[3] = { B.X0, B.Y0, B.Z0 }, Hi[3] = { B.X1, B.Y1, B.Z1 };
			double T0 = 0.0, T1 = 1e9;
			bool bHit = true;
			for (int32 A = 0; A < 3 && bHit; ++A)
			{
				if (FMath::Abs(V[A]) < 1e-9) { bHit = O[A] >= Lo[A] && O[A] <= Hi[A]; continue; }
				double Ta = (Lo[A] - O[A]) / V[A], Tb = (Hi[A] - O[A]) / V[A];
				if (Ta > Tb) Swap(Ta, Tb);
				T0 = FMath::Max(T0, Ta);
				T1 = FMath::Min(T1, Tb);
				bHit = T0 <= T1;
			}
			if (bHit) return 0.0;
		}
	}
	return 1.0;
}

void ARTSTreadDust::Emit(const FRTSTreadSource& Src, double Dt)
{
	const FVector F = Src.FwdM;
	const FVector R(F.Y, -F.X, 0.0);
	const FVector2D& Ld = Load.FindOrAdd(Src.Id, FVector2D(Src.Sand[0], Src.Sand[1]));
	for (int32 Li = 0; Li < P.Layers.Num(); ++Li)
	{
		const FRTSTreadLayer& L = P.Layers[Li];
		// os presets: a nuvem sempre; o jato da média pra cima; a cortina da alta pra cima (a sombra é do ultra, no Draw)
		if ((L.Name == TEXT("K1") && Quality < 1) || (L.Name == TEXT("C") && Quality < 2))
		{
			continue;
		}
		const double SpacingMul = Quality == 0 && L.Name == TEXT("K2") ? 2.0 : 1.0;
		for (int32 K = 0; K < 2; ++K)
		{
			const double SideSign = K == 0 ? -1.0 : 1.0;
			const double Vk = Src.Speed + SideSign * Src.YawRate * 1.425;   // a lagarta de dentro da curva anda menos
			const double DirK = Vk < 0.0 ? -1.0 : 1.0;
			const double V = FMath::Abs(Vk);
			const double Ramp = L.V1 > L.V0 ? FMath::Clamp((V - L.V0) / (L.V1 - L.V0), 0.0, 1.0) : (V > 0.3 ? 1.0 : 0.0);
			const bool bAlong = L.XMax > -2.40;
			const double Wr = FMath::Clamp(Src.Sand[K], 0.0, 1.0), Wf = FMath::Clamp(Src.SandFront[K], 0.0, 1.0);
			const double W = bAlong ? FMath::Max(Wr, Wf) : Wr;
			const double Lk = K == 0 ? Ld.X : Ld.Y;
			const double Ground = L.FollowLoad > 0.5 ? W + L.LoadGain * FMath::Max(0.0, Lk - W) : W;
			const double Amount = Ramp * Ground;
			const int64 Key = int64(Src.Id) * 64 + Li * 2 + K;
			double& Ac = LayerAcc.FindOrAdd(Key);
			if (Amount < 0.025)
			{
				Ac = 0.0;
				continue;
			}
			Ac += V * Dt;
			double& Next = LayerNext.FindOrAdd(Key, L.Spacing * SpacingMul);
			while (Ac >= Next / Amount)
			{
				Ac -= Next / Amount;
				Next = L.Spacing * SpacingMul * Rnd(1.0, FMath::Max(1.0, L.SpacingJit));
				const double X = Rnd(L.XMin, L.XMax) * DirK;
				const double Y = SideSign * L.YOff + Rnd(-L.YJit, L.YJit);
				if (bAlong)
				{
					const double Wx = FMath::Lerp(Wr, Wf, FMath::Clamp((X - -2.40) / (2.55 - -2.40), 0.0, 1.0));
					if (Rand() * W > Wx) continue;
				}
				FCard Cd;
				Cd.P = Src.PosM + F * X + R * Y + FVector(0.0, 0.0, L.Z0);
				Cd.V = F * Vk * L.Inherit - F * DirK * L.Kick + R * (Rnd(-1.0, 1.0) * L.Side);
				Cd.V.Z += L.Up * Rnd(0.6, 1.3);
				Cd.Life = L.Life * (1.0 + Rnd(-L.LifeJit, L.LifeJit));
				Cd.S = Rnd(1.0 - L.ScaleJit, 1.0 + L.ScaleJit) * (L.FollowLoad > 0.5 ? FMath::Lerp(0.5, 1.0, W) : 1.0);
				if (L.ScaleSlow < 1.0 && L.VFull > L.V1)
				{
					Cd.S *= FMath::Lerp(L.ScaleSlow, 1.0, FMath::Clamp((V - L.V1) / (L.VFull - L.V1), 0.0, 1.0));
				}
				Cd.Rot = Rnd(-L.RotMax, L.RotMax);
				Cd.Flip = L.Flip != 0.0 ? L.Flip : (Rand() < 0.5 ? -1.0 : 1.0);
				Cd.Bake0 = L.BakeStart + Rnd(0.0, L.BakeStartJit);
				Cd.A = FMath::Min(1.0, Amount);
				Cd.bDouble = SpacingMul > 1.0;
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

void ARTSTreadDust::Step(double Dt)
{
	TMap<int32, int32> SrcIndex;
	SrcIndex.Reserve(Sources.Num());
	for (int32 I = 0; I < Sources.Num(); ++I)
	{
		const FRTSTreadSource& S = Sources[I];
		SrcIndex.Add(S.Id, I);
		Visible.Add(S.Id, S.bVisible);
		Odo.FindOrAdd(S.Id) += FMath::Abs(S.Speed) * Dt;
		FVector2D& Ld = Load.FindOrAdd(S.Id, FVector2D(S.Sand[0], S.Sand[1]));
		const double Dk = FMath::Exp(-Dt / FMath::Max(P.LoadTau, 1e-3));
		Ld.X = FMath::Max(S.Sand[0], Ld.X * Dk);
		Ld.Y = FMath::Max(S.Sand[1], Ld.Y * Dk);
		Emit(S, Dt);
	}
	++StepCount;
	// cada cartão só mexe nele mesmo (as fontes, os metros andados e o sol só são lidos): os núcleos dividem a lista;
	// com poucos cartões o custo de dividir passa do ganho
	ParallelFor(Cards.Num(), [&](int32 Index)
	{
		FCard& C = Cards[Index];
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const int32 CardIndex = Index + 1;
		C.Age += Dt;
		const double Dk = FMath::Exp(-L.Drag * Dt);
		C.V = FVector(C.V.X * Dk + P.WindX * (1.0 - Dk), C.V.Y * Dk + P.WindY * (1.0 - Dk), C.V.Z * Dk - L.Sink * (1.0 - Dk));
		FVector Extra = FVector::ZeroVector;
		if (L.WakeRise > 0.0 || L.WakePull > 0.0 || L.WakeGrow > 0.0)
		{
			if (const int32* Si = SrcIndex.Find(C.Src))
			{
				const FRTSTreadSource& S = Sources[*Si];
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
		if (C.Sun < 0.0 || (StepCount + CardIndex) % 3 == 0)
		{
			const double Half = Atlas.SideM * L.Scale * C.S * 0.25;
			const double Sv = 0.5 * (SunVisAt(C.P + FVector(0.0, 0.0, 0.3), SunDir) + SunVisAt(C.P + FVector(0.0, 0.0, 0.3 + Half), SunDir));
			C.Sun = C.Sun < 0.0 ? Sv : C.Sun + (Sv - C.Sun) * (1.0 - FMath::Exp(-3.0 * Dt / 0.15));
			// a sombra do cenário (casa, árvore, relevo) no pé, no meio e no topo do cartão
			const double Top = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * FMath::Min(C.Age / FMath::Max(C.Life, 1e-3), 1.0)) * C.Wake;
			const double Hs[3] = { 0.3, 0.3 + 0.4 * Top, 0.3 + 0.8 * Top };
			for (int32 Hi = 0; Hi < 3; ++Hi)
			{
				const double Mv = MapSunVis(C.P + FVector(0.0, 0.0, Hs[Hi]));
				C.Map[Hi] = C.Map[Hi] < 0.0 ? Mv : C.Map[Hi] + (Mv - C.Map[Hi]) * (1.0 - FMath::Exp(-3.0 * Dt / 0.15));
			}
		}
	}, Cards.Num() < 512 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	Cards.RemoveAll([this](const FCard& C)
	{
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		if (L.MaskEnd > 0.0)
		{
			const double D = (Odo.FindRef(C.Src) - C.Odo0) / P.HullLen;
			if (D >= L.MaskEnd || 1.0 - SmoothStep(D, L.MaskDense, L.MaskEnd) < 0.05) return true;
		}
		return C.Age >= C.Life;
	});
}

void ARTSTreadDust::PushMesh(UProceduralMeshComponent* Mesh, int32 Slot, TArray<FVector>& V, TArray<FVector>& N, TArray<FVector2D>& U0,
	TArray<FVector2D>& U1, TArray<FVector2D>& U2, TArray<FVector2D>& U3, TArray<FLinearColor>& Cl, int32 VertsPerCard)
{
	// a seção é recriada só quando cresce (folga de 256 cartões); nos outros quadros só os vértices mudam
	const int32 K = FMath::Max(VertsPerCard, 3);
	const int32 Cards0 = V.Num() / K;
	const bool bGrow = Cards0 > MeshCap[Slot] || MeshVerts[Slot] != K || !Mesh->GetProcMeshSection(0);
	const int32 Cap = bGrow ? ((Cards0 / 256) + 1) * 256 : MeshCap[Slot];
	const FVector Far = V.Num() ? V[0] : FVector::ZeroVector;
	for (int32 Q = Cards0; Q < Cap; ++Q)
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
			const int32 B = Q * K;
			for (int32 I = 1; I + 1 < K; ++I) Tri.Append({ B, B + I, B + I + 1 });
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

void ARTSTreadDust::Draw(const FVector& CamM, const FVector& CamRight, const FVector& CamUp, const FVector& CamForward)
{
	struct FOut { const FCard* C; double D2; };
	TArray<FOut> Order;
	Order.Reserve(Cards.Num());
	// fora da tela não entra (nem na ordem nem na malha): a esfera do cartão contra a pirâmide da câmera, com folga
	// pro pivô no pé e pra sombra no chão (esticada pra longe do sol, só no ultra)
	const double SecH = FMath::Sqrt(1.0 + TanHalfH * TanHalfH), SecV = FMath::Sqrt(1.0 + TanHalfV * TanHalfV);
	const double SdZ = FMath::Max(SunDir.Z, 1e-3);
	const double CotSun = FMath::Min(FMath::Sqrt(FMath::Max(0.0, 1.0 - SunDir.Z * SunDir.Z)) / SdZ, 6.0);
	for (const FCard& C : Cards)
	{
		if (!Visible.FindRef(C.Src) && Visible.Contains(C.Src))
		{
			continue;                              // a névoa de guerra
		}
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const double K = C.Age / FMath::Max(C.Life, 1e-3);
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake;
		const double Rad = Size * (1.5 + (Quality >= 3 && L.Shadow > 0.0 ? CotSun : 0.0));
		const FVector Vc = C.P - CamM;
		const double Z = FVector::DotProduct(Vc, CamForward);
		if (Z < -Rad || FMath::Abs(FVector::DotProduct(Vc, CamRight)) - Z * TanHalfH > Rad * SecH
			|| FMath::Abs(FVector::DotProduct(Vc, CamUp)) - Z * TanHalfV > Rad * SecV)
		{
			continue;
		}
		Order.Add({ &C, Vc.SizeSquared() });
	}
	Order.Sort([](const FOut& A, const FOut& B) { return A.D2 > B.D2; });

	const FVector Origin100 = GetActorLocation();
	const FVector ToCam = -CamForward;
	const FVector Sd = SunDir;
	const FVector Sh = FVector(Sd.X, Sd.Y, 0.0).GetSafeNormal();
	const FVector SA = -Sh, SR(-SA.Y, SA.X, 0.0);
	const double Cot = FMath::Min(FMath::Sqrt(FMath::Max(0.0, 1.0 - Sd.Z * Sd.Z)) / FMath::Max(Sd.Z, 1e-3), 6.0);
	const bool bOct = Atlas.Cut8.Num() == Atlas.Frames;
	const int32 VertsPerCard = bOct ? 8 : 4;
	// cada cartão escreve os seus vértices num lugar fixo (o da ordem): os núcleos dividem a montagem
	const int32 N = Order.Num();
	const bool bShadowOn = Quality >= 3 && ShadowMid && !Sh.IsNearlyZero();
	TArray<int32> ShadowSlot;
	ShadowSlot.SetNumUninitialized(N);
	int32 NumShadow = 0;
	for (int32 I = 0; I < N; ++I)
	{
		ShadowSlot[I] = bShadowOn && P.Layers[Order[I].C->Layer].Shadow > 0.0 ? NumShadow++ : -1;
	}
	TArray<FVector> V, Nrm, SV, SN;
	TArray<FVector2D> U0, U1, U2, U3, SU0, SU1;
	TArray<FLinearColor> Cl, SCl;
	const int32 Nv = N * VertsPerCard, Ns = NumShadow * 4;
	V.SetNumUninitialized(Nv); Nrm.SetNumUninitialized(Nv); U0.SetNumUninitialized(Nv); U1.SetNumUninitialized(Nv);
	U2.SetNumZeroed(Nv); U3.SetNumUninitialized(Nv); Cl.SetNumUninitialized(Nv);
	SV.SetNumUninitialized(Ns); SN.SetNumUninitialized(Ns); SU0.SetNumUninitialized(Ns); SU1.SetNumUninitialized(Ns); SCl.SetNumUninitialized(Ns);
	ParallelFor(N, [&](int32 I)
	{
		const FCard& C = *Order[I].C;
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const double K = C.Age / FMath::Max(C.Life, 1e-3);
		double A = L.Alpha * C.A * SmoothStep(K, 0.0, FMath::Max(L.Attack, 1e-3)) * (1.0 - SmoothStep(K, L.Decay, 1.0));
		if (L.MaskEnd > 0.0)
		{
			A *= 1.0 - SmoothStep((Odo.FindRef(C.Src) - C.Odo0) / P.HullLen, L.MaskDense, L.MaskEnd);
		}
		A = FMath::Min(1.0, A);
		if (C.bDouble)
		{
			A = 1.0 - (1.0 - A) * (1.0 - A);       // a baixa: o cartão cobre o que os dois sobrepostos cobririam
		}
		const double Frame = Atlas.FrameAt(C.Bake0 + C.Age * L.BakeRate);
		const int32 Fi = FMath::Clamp(FMath::FloorToInt32(Frame), 0, Atlas.Frames - 1), Fj = FMath::Min(Fi + 1, Atlas.Frames - 1);
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake;
		const double PvX = Atlas.Pivot.X;
		const double PvY = L.PivotBottom > 0.5 ? FMath::Lerp(Atlas.BallBottom[Fi], Atlas.BallBottom[Fj], FMath::Clamp(Frame - Fi, 0.0, 1.0)) : Atlas.Pivot.Y;
		const double CardSun = C.Sun < 0.0 ? 1.0 : C.Sun;
		const double Erode = L.Erode * SmoothStep(K, 0.35, 1.0);
		// o espelho: sorteado, ou (orient) o lado cheio da gravação pro lado de trás do tanque na tela
		double Flip = C.Flip, XScale = 1.0;
		if (L.Orient > 0.5 && !C.Back.IsNearlyZero())
		{
			const double Bx = FVector::DotProduct(C.Back, CamRight);
			Flip = (Bx >= 0.0 ? 1.0 : -1.0) * (C.Flip < 0.0 ? -1.0 : 1.0);
			XScale = FMath::Max(FMath::Abs(Bx), FMath::Abs(CamForward.Z));
		}
		const double Cs = FMath::Cos(C.Rot), Sn = FMath::Sin(C.Rot);
		const FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp;
		const FVector L3(FVector::DotProduct(Sd, R) * Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, ToCam));
		const FLinearColor Lc(L3.X * 0.5 + 0.5, L3.Y * 0.5 + 0.5, L3.Z * 0.5 + 0.5, 1.0f);
		const FVector2D Pivot(Flip > 0 ? PvX - 0.5 : 0.5 - PvX, PvY - 0.5);
		FVector Pc = C.P;
		double Sz = Size;
		if (L.CamOffset > 0.0)
		{
			const double Dc = FVector::Dist(CamM, C.P);
			const double Off = FMath::Min(L.CamOffset, Dc * 0.5);
			Pc = C.P + (CamM - C.P) / FMath::Max(Dc, 1e-3) * Off;
			Sz *= (Dc - Off) / FMath::Max(Dc, 1e-3);
		}
		// a caixa do alfa dos dois quadros misturados e o octógono (o mesmo visual, menos pixel)
		FVector2D Qs[4] = { Corners[0], Corners[1], Corners[2], Corners[3] };
		if (Atlas.BBox.Num() == Atlas.Frames)
		{
			const FVector4 &Ba = Atlas.BBox[Fi], &Bb = Atlas.BBox[Fj];
			const double U0b = FMath::Min(Ba.X, Bb.X), V0b = FMath::Min(Ba.Y, Bb.Y), U1b = FMath::Max(Ba.Z, Bb.Z), V1b = FMath::Max(Ba.W, Bb.W);
			const double X0 = Flip < 0 ? 0.5 - U1b : U0b - 0.5, X1 = Flip < 0 ? 0.5 - U0b : U1b - 0.5;
			Qs[0] = FVector2D(X0, V0b - 0.5); Qs[1] = FVector2D(X1, V0b - 0.5); Qs[2] = FVector2D(X1, V1b - 0.5); Qs[3] = FVector2D(X0, V1b - 0.5);
		}
		TArray<FVector2D, TInlineAllocator<8>> Pts;
		if (bOct)
		{
			const TArray<FVector2D>& Poly = Atlas.Cut8[Fi];
			for (int32 Pi = 0; Pi < FMath::Min(Poly.Num(), 8); ++Pi)    // 8 vértices fixos: o lugar de cada cartão na malha
			{
				Pts.Add(FVector2D(Flip < 0 ? 0.5 - Poly[Pi].X : Poly[Pi].X - 0.5, Poly[Pi].Y - 0.5));
			}
			while (Pts.Num() < 8) Pts.Add(Pts.Num() ? Pts.Last() : FVector2D::ZeroVector);
		}
		else
		{
			for (const FVector2D& Q : Qs) Pts.Add(Q);
		}
		const double BoxY0 = Qs[0].Y, BoxY1 = Qs[2].Y;
		auto Hof = [&](double Y) { return FMath::Clamp((Y + 0.5 - PvY) / FMath::Max(1.0 - PvY, 1e-3), 0.0, 1.0); };
		const double H0 = Hof(BoxY0), H1 = Hof(BoxY1);
		int32 W = I * VertsPerCard;
		for (const FVector2D& Q : Pts)
		{
			const FVector2D Qp = Q - Pivot;
			V[W] = (Pc + (R * (Qp.X * XScale) + Up * Qp.Y) * Sz) * 100.0 - Origin100;
			Nrm[W] = ToCam;
			U0[W] = FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5);
			U1[W] = FVector2D(Frame, A);
			double SunVis = CardSun;
			const double Hh = FMath::Lerp(H0, H1, FMath::Clamp((Q.Y - BoxY0) / FMath::Max(BoxY1 - BoxY0, 1e-6), 0.0, 1.0));
			if (L.BaseShade < 1.0)
			{
				SunVis *= FMath::Lerp(L.BaseShade, 1.0, Hh);
			}
			// o cenário: pé (0), meio (0,5), topo (1) pela altura do vértice na bola. Vai no UV2.x (1 = sombra), que corta só
			// o sol no material: a pedra longe tapa o sol mas não o céu (a sombra do tanque, no UV3.y, tapa os dois)
			const double MapVis = C.Map[0] < 0.0 ? 1.0 : Hh < 0.5 ? FMath::Lerp(C.Map[0], C.Map[1], Hh * 2.0) : FMath::Lerp(C.Map[1], C.Map[2], Hh * 2.0 - 1.0);
			U2[W] = FVector2D(1.0 - MapVis, 0.0);
			U3[W] = FVector2D(Erode, SunVis);
			Cl[W] = Lc;
			++W;
		}
		// a sombra da nuvem no chão (só no ultra): o cartão deitado, esticado pra longe do sol
		if (ShadowSlot[I] >= 0)
		{
			int32 Ws = ShadowSlot[I] * 4;
			for (const FVector2D& Q : Qs)
			{
				const FVector2D Qp = Q - Pivot;
				const FVector Gp(C.P.X, C.P.Y, C.GroundZ + 0.05);
				SV[Ws] = (Gp + SR * (Qp.X * Size) + SA * (Qp.Y * Size * Cot)) * 100.0 - Origin100;
				SN[Ws] = FVector::UpVector;
				SU0[Ws] = FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5);
				SU1[Ws] = FVector2D(Frame, A * L.Shadow * CardSun * (C.Map[0] >= 0.0 ? C.Map[0] : 1.0));   // na sombra da casa, não escurece de novo
				SCl[Ws] = FLinearColor::White;
				++Ws;
			}
		}
	}, N < 256 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	const bool bEmpty = V.Num() == 0;
	if (!bEmpty || !bMeshEmpty[0])
	{
		PushMesh(CardMesh, 0, V, Nrm, U0, U1, U2, U3, Cl, VertsPerCard);
	}
	bMeshEmpty[0] = bEmpty;
	const bool bSEmpty = SV.Num() == 0;
	if (!bSEmpty || !bMeshEmpty[1])
	{
		TArray<FVector2D> E2, E3;
		PushMesh(ShadowMesh, 1, SV, SN, SU0, SU1, E2, E3, SCl, 4);
	}
	bMeshEmpty[1] = bSEmpty;
}

void ARTSTreadDust::CaptureMapShadows()
{
	bShadowDirty = false;
	ShadowSun = SunDir;
	ShadowDepth.Reset();
	for (FCard& C : Cards)
	{
		C.Map[0] = C.Map[1] = C.Map[2] = -1.0;   // os cartões no ar pegam o mapa novo na hora, sem fade
	}
	UWorld* World = GetWorld();
	if (!World || SunDir.Z < 0.02)
	{
		return;                                   // o sol no horizonte (ou a noite): sem mapa
	}
	const double T0 = FPlatformTime::Seconds();
	// o que faz sombra e não anda: o terreno, as casas, as árvores, as pedras (as unidades ficam com a sombra das caixas)
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Show;
	FBox Bounds(ForceInit);
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Pc = *It;
		if (!Pc || Pc->GetWorld() != World || !Pc->IsRegistered() || !Pc->IsVisible() || !Pc->CastShadow || Pc->Bounds.SphereRadius > 200000.0)
		{
			continue;
		}
		const AActor* Ow = Pc->GetOwner();
		if (Ow == this || (Ow && (Ow->IsA<APawn>() || MapShadowIgnoreTags.ContainsByPredicate([Ow](const FName& Tg) { return Ow->ActorHasTag(Tg); }))))
		{
			continue;
		}
		Show.Add(Pc);
		Bounds += Pc->Bounds.GetBox();
	}
	if (Show.Num() == 0 || !Bounds.IsValid)
	{
		return;
	}
	// a câmera do sol: ortográfica, olhando pra onde a luz anda, cobrindo o cenário inteiro
	const FVector F = -SunDir;
	const FRotator Rot = FRotationMatrix::MakeFromX(F).Rotator();
	const FRotationMatrix Rm(Rot);
	const FVector Ax = Rm.GetScaledAxis(EAxis::Y), Ay = Rm.GetScaledAxis(EAxis::Z);
	FVector Corner[8];
	Bounds.GetVertices(Corner);
	double Lo[3] = { 1e30, 1e30, 1e30 }, Hi[3] = { -1e30, -1e30, -1e30 };
	for (const FVector& P0 : Corner)
	{
		const double D[3] = { FVector::DotProduct(P0, Ax), FVector::DotProduct(P0, Ay), FVector::DotProduct(P0, F) };
		for (int32 K = 0; K < 3; ++K)
		{
			Lo[K] = FMath::Min(Lo[K], D[K]);
			Hi[K] = FMath::Max(Hi[K], D[K]);
		}
	}
	ShadowW = FMath::Max(Hi[0] - Lo[0], Hi[1] - Lo[1]) + 2000.0;
	ShadowX = Ax;
	ShadowY = Ay;
	ShadowF = F;
	ShadowO = Ax * (0.5 * (Lo[0] + Hi[0])) + Ay * (0.5 * (Lo[1] + Hi[1])) + F * (Lo[2] - 5000.0);
	if (!ShadowRT)
	{
		ShadowRT = NewObject<UTextureRenderTarget2D>(this);
		ShadowRT->RenderTargetFormat = RTF_R32f;
		ShadowRT->ClearColor = FLinearColor(1e9f, 1e9f, 1e9f, 1.0f);
		ShadowRT->InitAutoFormat(ShadowRes, ShadowRes);
		ShadowRT->UpdateResourceImmediate(true);
	}
	if (!ShadowCapture)
	{
		ShadowCapture = NewObject<USceneCaptureComponent2D>(this);
		ShadowCapture->bCaptureEveryFrame = false;
		ShadowCapture->bCaptureOnMovement = false;
		ShadowCapture->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
		ShadowCapture->ProjectionType = ECameraProjectionMode::Orthographic;
		ShadowCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		ShadowCapture->TextureTarget = ShadowRT;
		ShadowCapture->RegisterComponent();
	}
	ShadowCapture->TextureTarget = ShadowRT;      // o alvo é refeito a cada mapa (o anterior foi devolvido à placa)
	ShadowCapture->OrthoWidth = ShadowW;
	ShadowCapture->ShowOnlyComponents = Show;
	ShadowCapture->SetWorldLocationAndRotation(ShadowO, Rot);
	ShadowCapture->CaptureScene();
	FTextureRenderTargetResource* Res = ShadowRT->GameThread_GetRenderTargetResource();
	ShadowDepth.SetNumUninitialized(ShadowRes * ShadowRes);
	const int32 Strip = 512;
	for (int32 Y0 = 0; Y0 < ShadowRes; Y0 += Strip)
	{
		// em faixas: a imagem inteira em FLinearColor seriam 256 MB de memória temporária
		TArray<FLinearColor> Px;
		FReadSurfaceDataFlags Flags(RCM_MinMax);
		if (!Res || !Res->ReadLinearColorPixels(Px, Flags, FIntRect(0, Y0, ShadowRes, Y0 + Strip)) || Px.Num() != ShadowRes * Strip)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: não li o mapa de sombra do cenário"));
			ShadowDepth.Reset();
			return;
		}
		for (int32 I = 0; I < Px.Num(); ++I)
		{
			ShadowDepth[Y0 * ShadowRes + I] = Px[I].R;
		}
	}
	if (ShadowRT)
	{
		ShadowRT->ReleaseResource();             // o mapa fica na CPU; a placa devolve os 64 MB
		ShadowRT = nullptr;
	}
	// a calibração: um ponto do chão que o sol vê tem, no mapa, a própria distância ao sol (mais uma constante, se a
	// Unreal mover o plano de perto da câmera ortográfica); de quebra acha pra que lado a imagem cresce (4 jeitos)
	TArray<FVector> Ground;
	for (int32 Gy = 0; Gy < 9; ++Gy)
	{
		for (int32 Gx = 0; Gx < 9; ++Gx)
		{
			const double Qx = FMath::Lerp(Bounds.Min.X, Bounds.Max.X, 0.2 + 0.075 * Gx), Qy = FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, 0.2 + 0.075 * Gy);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, FVector(Qx, Qy, Bounds.Max.Z + 100.0), FVector(Qx, Qy, Bounds.Min.Z - 100.0), ECC_Visibility))
			{
				Ground.Add(Hit.ImpactPoint);
			}
		}
	}
	int32 BestHits = -1;
	for (int32 Vr = 0; Vr < 4; ++Vr)
	{
		const double Su = (Vr & 1) ? -1.0 : 1.0, Sv = (Vr & 2) ? -1.0 : 1.0;
		TArray<double> Off;
		for (const FVector& G : Ground)
		{
			const FVector D = G - ShadowO;
			const double Fu = (0.5 + Su * FVector::DotProduct(D, Ax) / ShadowW) * ShadowRes - 0.5;
			const double Fv = (0.5 + Sv * FVector::DotProduct(D, Ay) / ShadowW) * ShadowRes - 0.5;
			const double Dm = ShadowDepthAt(Fu, Fv);
			if (Dm < 1e8)
			{
				Off.Add(Dm - FVector::DotProduct(D, F));
			}
		}
		if (Off.Num() < 8)
		{
			continue;
		}
		TArray<double> Sorted = Off;
		Sorted.Sort();
		const double Med = Sorted[Sorted.Num() / 2];
		int32 Hits = 0;
		for (double O : Off)
		{
			Hits += FMath::Abs(O - Med) < 100.0 ? 1 : 0;   // o chão aberto bate em cm; quem está na sombra de pedra fica de fora
		}
		if (Hits > BestHits)
		{
			BestHits = Hits;
			ShadowSu = Su;
			ShadowSv = Sv;
			ShadowCal = Med;
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TreadShadowDump")))
	{
		// depuração: a profundidade (float32, linha a linha), a base e os pontos do chão, em Saved/
		FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(ShadowDepth.GetData()), ShadowDepth.Num() * sizeof(float)),
			*(FPaths::ProjectSavedDir() / TEXT("shadow_depth.raw")));
		FString Meta = FString::Printf(TEXT("res %d\nw %.3f\no %.3f %.3f %.3f\nx %.6f %.6f %.6f\ny %.6f %.6f %.6f\nf %.6f %.6f %.6f\n"), ShadowRes, ShadowW,
			ShadowO.X, ShadowO.Y, ShadowO.Z, Ax.X, Ax.Y, Ax.Z, Ay.X, Ay.Y, Ay.Z, F.X, F.Y, F.Z);
		for (const FVector& G : Ground)
		{
			Meta += FString::Printf(TEXT("g %.2f %.2f %.2f\n"), G.X, G.Y, G.Z);
		}
		FFileHelper::SaveStringToFile(Meta, *(FPaths::ProjectSavedDir() / TEXT("shadow_meta.txt")));
	}
	UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: mapa de sombra do cenário com %d peças, %.0f m de lado (%.0f cm por pixel), calibração %.1f cm, orientação (%+.0f, %+.0f), %d de %d pontos do chão batem, %.0f ms"),
		Show.Num(), ShadowW / 100.0, ShadowW / ShadowRes, ShadowCal, ShadowSu, ShadowSv, BestHits, Ground.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
	if (BestHits < Ground.Num() / 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: o mapa de sombra não bate com o chão; sem sombra do cenário"));
		ShadowDepth.Reset();
	}
}

double ARTSTreadDust::MapSunVis(const FVector& Pm) const
{
	if (ShadowDepth.Num() == 0)
	{
		return 1.0;
	}
	// 3 × 3 amostras a cada 2 pixels (~0,6 m), cada uma bilinear: a borda da sombra na poeira é larga (o raio de vista
	// atravessa metros de nuvem) e o valor não pula quando o cartão anda
	const FVector D = Pm * 100.0 - ShadowO;
	const double Fu = (0.5 + ShadowSu * FVector::DotProduct(D, ShadowX) / ShadowW) * ShadowRes - 0.5;
	const double Fv = (0.5 + ShadowSv * FVector::DotProduct(D, ShadowY) / ShadowW) * ShadowRes - 0.5;
	const double Z = FVector::DotProduct(D, ShadowF) + ShadowCal - 30.0;   // 30 cm de folga: o pé do cartão encosta no chão
	double Sum = 0.0;
	for (int32 Dy = -2; Dy <= 2; Dy += 2)
	{
		for (int32 Dx = -2; Dx <= 2; Dx += 2)
		{
			Sum += FMath::Clamp((ShadowDepthAt(Fu + Dx, Fv + Dy) - Z) / 50.0 + 0.5, 0.0, 1.0);
		}
	}
	return Sum / 9.0;
}

double ARTSTreadDust::ShadowDepthAt(double Fu, double Fv) const
{
	// bilinear (fora do mapa: longe, ou seja, sol)
	const int32 U0 = FMath::FloorToInt32(Fu), V0 = FMath::FloorToInt32(Fv);
	if (U0 < 0 || V0 < 0 || U0 + 1 >= ShadowRes || V0 + 1 >= ShadowRes)
	{
		return 1e9;
	}
	const double Tu = Fu - U0, Tv = Fv - V0;
	const float* Row0 = &ShadowDepth[V0 * ShadowRes + U0];
	const float* Row1 = Row0 + ShadowRes;
	return FMath::Lerp(FMath::Lerp(double(Row0[0]), double(Row0[1]), Tu), FMath::Lerp(double(Row1[0]), double(Row1[1]), Tu), Tv);
}

void ARTSTreadDust::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerCameraManager* Pcm = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Pcm || !Init())
	{
		return;
	}
	if (!Sun.IsValid())
	{
		FindSun();
	}
	if (Sun.IsValid())
	{
		SunDir = -Sun->GetComponentTransform().GetUnitAxis(EAxis::X);
	}
	if (bShadowDirty || FVector::DotProduct(SunDir, ShadowSun) < FMath::Cos(FMath::DegreesToRadians(0.5)))
	{
		CaptureMapShadows();
	}
	const double C0 = FPlatformTime::Seconds();
	const double Dt = FMath::Min(double(DeltaSeconds), 0.1);   // um engasgo não joga a poeira longe
	if (bEnabled && !bExternalClock)
	{
		Step(Dt);
	}
	// a abertura da câmera (o FOV da Unreal é o horizontal) e a proporção da tela, pro corte do que está fora
	FVector2D Vp(16.0, 9.0);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Vp);
	}
	TanHalfH = FMath::Tan(FMath::DegreesToRadians(Pcm->GetFOVAngle() * 0.5));
	TanHalfV = TanHalfH * (Vp.X > 1.0 ? Vp.Y / Vp.X : 9.0 / 16.0);
	const FRotationMatrix Rm(Pcm->GetCameraRotation());
	Draw(Pcm->GetCameraLocation() / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
	LastMs = StepMs + (FPlatformTime::Seconds() - C0) * 1000.0;
	StepMs = 0.0;
}
