#include "RTSTreadDust.h"
#include "RTSDustField.h"
#include "RTSSunShadow.h"

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
#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// A mesma conta do FXLab/Source/FXLab/Private/FXTread.cpp (o caminho "layers"), sem o laboratório (a volta, a câmera
// dos alvos, a medida, o A/B, a grade do asfalto lida do Landscape). Quando a poeira mudar no FXLab, recopiar aqui.

namespace RTSTreadDustVolume
{
	bool LoadKickOnce(const FString& Name);      // RTSTreadDustVolume.cpp
	double ImpactGain();                         // RTSTreadDustVolume.cpp (rts.TreadDust.ImpactGain)
}

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

	// (23/10, a explosão com vida do jogo) os puffs de impacto não são de veículo nenhum: a fonte deles fica fora da lista (a névoa já foi
	// conferida por quem chama, nenhum veículo é "o próprio", a odometria não vale: ARTSTreadDust::ImpactSrcId) e as camadas deles, se o
	// perfil não as tem (o perfil é a cópia do FXLab: pack_tread_dust.py), nascem com estes padrões
	const TCHAR* const ImpactLayerNames[3] = { TEXT("impact"), TEXT("impact_surge"), TEXT("impact_linger") };
	FRTSTreadLayer DefaultImpactLayer(int32 Kind)
	{
		// a camada "impact": o véu de areia do choque, os pontinhos, os pousos e a torre (sai a 2–4 m/s e para em 0,5–1 m: o arrasto 4; o
		// BF3 e o cEssex: nasce a ~50%, entra rápido, fica baixa e assenta) e o sumiço pela densidade da v26 (as bordas primeiro). A
		// "impact_surge": a onda de base do ataque sem a gravação (a cortina desaba e o anel rola rente ao chão como corrente de gravidade:
		// a frente para em R_L, v0 = arrasto·(R_L − r0): o arrasto 0,8 dá 3–9 m/s de saída). A "impact_linger": a poeira que fica depois da
		// gravação do ataque (rola devagar, arrasto 0,3, abre 1,6×, envelhece a gravação 5× mais devagar: a nuvem rala que paira minutos, no
		// tempo do jogo comprimido). As três derivam no vento do mapa (windGain 1; a K2 das lagartas fica com 0, a aprovada)
		FRTSTreadLayer L;
		const int32 K = FMath::Clamp(Kind, 0, 2);
		L.Name = ImpactLayerNames[K];
		L.XMin = 0.0; L.XMax = 0.0; L.YOff = 0.0; L.YJit = 0.0; L.Z0 = 0.0;
		L.Spacing = 1.0; L.SpacingJit = 1.0; L.Life = 4.0; L.LifeJit = 0.0; L.MaskDense = 0.0; L.MaskEnd = 0.0;
		L.BakeStart = K == 2 ? 1.0 : 0.1; L.BakeStartJit = 0.25; L.BakeRate = K == 2 ? 0.2 : 1.0; L.Scale = 1.0; L.ScaleJit = 0.0;
		L.GrowTo = K == 0 ? 1.3 : 1.6;
		L.Alpha = 1.0; L.Attack = 0.05; L.Decay = K == 1 ? 0.4 : 0.5; L.Inherit = 0.0; L.Drag = K == 0 ? 4.0 : (K == 1 ? 0.8 : 0.3);
		L.Kick = 0.0; L.Side = 0.0; L.Up = 0.0; L.Sink = 0.0;
		L.Orient = 0.0; L.Flip = 0.0; L.RotMax = 0.1; L.PivotBottom = 1.0; L.CamOffset = 0.0; L.Erode = 0.0;
		L.Shadow = 0.15; L.BaseShade = 0.3; L.BaseShadeWorld = 1.6; L.SunSide = 0.0; L.WindGain = 1.0;
		L.ErodeFade = 3.0; L.GrowFade = 0.15; L.RiseFade = 0.1; L.FadeTau = 1.2; L.FadeAlphaFrom = 0.9;
		return L;
	}
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
	double MipBiasOld = 0.0;
	if (J->TryGetNumberField(TEXT("mipBias"), MipBiasOld))
	{
		MipMin = MipBiasOld + 1.0;                 // os perfis de antes (bias sobre o nível da distância, ~1 na pose do r1)
	}
	J->TryGetNumberField(TEXT("mipMin"), MipMin);
	J->TryGetNumberField(TEXT("paleK"), PaleK);
	J->TryGetNumberField(TEXT("flat6"), Flat6);
	J->TryGetNumberField(TEXT("dustSat"), DustSat);
	const TSharedPtr<FJsonObject>* Sand = nullptr;
	if (J->TryGetObjectField(TEXT("sand"), Sand) && Sand)
	{
		(*Sand)->TryGetNumberField(TEXT("r"), TintR);
		(*Sand)->TryGetNumberField(TEXT("g"), TintG);
		(*Sand)->TryGetNumberField(TEXT("b"), TintB);
		(*Sand)->TryGetNumberField(TEXT("albedo"), Albedo);
		(*Sand)->TryGetNumberField(TEXT("density"), Density);
		(*Sand)->TryGetNumberField(TEXT("groundR"), GroundR);
		(*Sand)->TryGetNumberField(TEXT("groundG"), GroundG);
		(*Sand)->TryGetNumberField(TEXT("groundB"), GroundB);
	}
	const TSharedPtr<FJsonObject>* Inter = nullptr;
	if (J->TryGetObjectField(TEXT("interact"), Inter) && Inter)
	{
		double On = 0.0;
		(*Inter)->TryGetNumberField(TEXT("on"), On);
		bInteract = On > 0.5;
		(*Inter)->TryGetNumberField(TEXT("b0"), WakeB0);
		(*Inter)->TryGetNumberField(TEXT("bubbleH"), BubbleH);
		FString Model;
		if ((*Inter)->TryGetStringField(TEXT("blastModel"), Model))
		{
			BlastModel = Model == TEXT("kg") ? 1 : 0;
		}
		(*Inter)->TryGetNumberField(TEXT("blastGain"), BlastGain);
		(*Inter)->TryGetNumberField(TEXT("blastT"), BlastT);
	}
	const TSharedPtr<FJsonObject>* Shd = nullptr;
	if (J->TryGetObjectField(TEXT("shadow"), Shd) && Shd)
	{
		double Mode = ShadowMode;
		(*Shd)->TryGetNumberField(TEXT("mode"), Mode);
		ShadowMode = FMath::Clamp(FMath::RoundToInt32(Mode), 0, 2);
		(*Shd)->TryGetNumberField(TEXT("occ"), ShadowOcc);
		(*Shd)->TryGetNumberField(TEXT("texel"), ShadowTexelM);
		(*Shd)->TryGetNumberField(TEXT("soft"), ShadowSoftM);
		(*Shd)->TryGetNumberField(TEXT("chord"), ShadowChord);
		(*Shd)->TryGetNumberField(TEXT("sunSide"), ShadowSunSide);
		(*Shd)->TryGetNumberField(TEXT("sky"), ShadowSky);
	}
	const TSharedPtr<FJsonObject>* Vol = nullptr;
	if (J->TryGetObjectField(TEXT("volume"), Vol) && Vol)
	{
		(*Vol)->TryGetNumberField(TEXT("density"), VolDensity);
		(*Vol)->TryGetNumberField(TEXT("erode"), VolErode);
		(*Vol)->TryGetNumberField(TEXT("detail"), VolDetail);
		(*Vol)->TryGetNumberField(TEXT("albedoMax"), VolAlbedoMax);
		(*Vol)->TryGetNumberField(TEXT("sat"), VolSat);
		(*Vol)->TryGetStringField(TEXT("kick"), VolKick);
		(*Vol)->TryGetStringField(TEXT("kickCoarse"), VolKickCoarse);
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
				{ TEXT("baseShadeWorld"), &L.BaseShadeWorld }, { TEXT("sunSide"), &L.SunSide }, { TEXT("sunSideWidth"), &L.SunSideWidth }, { TEXT("spacingPoisson"), &L.SpacingPoisson }, { TEXT("bakeJitFrames"), &L.BakeJitFrames }, { TEXT("scaleLogMin"), &L.ScaleLogMin },
				{ TEXT("scaleLogMax"), &L.ScaleLogMax }, { TEXT("rotFreeAfter"), &L.RotFreeAfter }, { TEXT("erodeFromBirth"), &L.ErodeFromBirth },
				{ TEXT("erodeFade"), &L.ErodeFade }, { TEXT("growFade"), &L.GrowFade }, { TEXT("riseFade"), &L.RiseFade },
				{ TEXT("fadeTau"), &L.FadeTau }, { TEXT("fadeAlphaFrom"), &L.FadeAlphaFrom }, { TEXT("windGain"), &L.WindGain },
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

void ARTSTreadDust::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bVolumeOn)
	{
		RTSDustField::SubmitFrame(FVector::ZeroVector, SunDir, Clock, 0.0f, TArray<FRTSDustPuff>());   // o campo fica vazio
		bVolumeOn = false;
	}
	Super::EndPlay(Reason);
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
	// o perfil: o do pacote, ou -TreadDustProfile=<caminho> (o A/B de perfis no FXLab)
	FString ProfilePath = Res / TEXT("tread.json");
	FParse::Value(FCommandLine::Get(), TEXT("-TreadDustProfile="), ProfilePath);
	if (!P.Load(ProfilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("RTSTreadDust: falta o perfil %s"), *ProfilePath);
	}
	UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: perfil %s"), *ProfilePath);
	// (23/10) as camadas dos puffs de impacto do jogo (AddImpactPuffs): as do perfil, ou os padrões, no fim da lista (as chaves das camadas
	// dos veículos não mudam)
	if (P.Layers.Num() > 0)
	{
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			ImpactLayers[Kind] = P.Layers.IndexOfByPredicate([Kind](const FRTSTreadLayer& L) { return L.Name == ImpactLayerNames[Kind]; });
			if (ImpactLayers[Kind] < 0)
			{
				ImpactLayers[Kind] = P.Layers.Add(DefaultImpactLayer(Kind));
			}
		}
	}
	// o A/B da interação: -TreadDustInteract=0/1 e -TreadDustBlast=aprovado|kg
	int32 InterArg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("-TreadDustInteract="), InterArg))
	{
		P.bInteract = InterArg > 0;
	}
	FString BlastArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("-TreadDustBlast="), BlastArg))
	{
		P.BlastModel = BlastArg == TEXT("kg") ? 1 : 0;
	}
	// o A/B da sombra: -TreadShadowMode=0|1|2 (0 = a de antes), -TreadShadowSunSide=, -TreadShadowChord=, -TreadShadowOcc=
	FParse::Value(FCommandLine::Get(), TEXT("-TreadShadowMode="), P.ShadowMode);
	P.ShadowMode = FMath::Clamp(P.ShadowMode, 0, 2);
	FParse::Value(FCommandLine::Get(), TEXT("-TreadShadowSunSide="), P.ShadowSunSide);
	FParse::Value(FCommandLine::Get(), TEXT("-TreadShadowChord="), P.ShadowChord);
	FParse::Value(FCommandLine::Get(), TEXT("-TreadShadowOcc="), P.ShadowOcc);
	FParse::Value(FCommandLine::Get(), TEXT("-TreadShadowSky="), P.ShadowSky);
	UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: sombra no modo %d (%s), céu na sombra %.2f (0 o aprovado, 1 o de volume; tapado %.2f), corda %.2f, lado do sol × %.2f"), P.ShadowMode,
		P.ShadowMode == 2 ? TEXT("por pixel") : P.ShadowMode == 1 ? TEXT("por cartão") : TEXT("a de antes"),
		P.ShadowSky, P.ShadowOcc, P.ShadowChord, P.ShadowSunSide);
	if (P.bInteract)
	{
		UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: interação ligada (esteira b0 %.2f m, bolha %.1f H, pulso %s × %.2f em %.2f s)"),
			P.WakeB0, P.BubbleH, P.BlastModel == 1 ? TEXT("Kinney-Graham") : TEXT("aprovado"), P.BlastGain, P.BlastT);
	}
	if (!Atlas.Load(Res / (P.Bake + TEXT(".atlas.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("RTSTreadDust: falta o atlas %s"), *(Res / (P.Bake + TEXT(".atlas.json"))));
	}
	// as folhas: as normalizadas em BC7 (19 MB de placa, iguais no olho às HDR: o A/B de 14/10) ou, com
	// -TreadDustSheets=hdr, as de 16 bits por canal (150 MB; só pra comparar)
	FString Sheets;
	FParse::Value(FCommandLine::Get(), TEXT("-TreadDustSheets="), Sheets);
	const bool bNorm8 = Sheets != TEXT("hdr");
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
		// LightScale: xy o ganho das folhas norm8 (as HDR: 1), z o mip mínimo do perfil (a folha mais borrada, menos bolinho miúdo)
		CardMid->SetVectorParameterValue(TEXT("LightScale"), bNorm8 ? FLinearColor(Atlas.LightScale.X, Atlas.LightScale.Y, P.MipMin, 0.0f) : FLinearColor(1.0f, 1.0f, P.MipMin, 0.0f));
		CardMid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(P.TintR, P.TintG, P.TintB, 1.0f));
		CardMid->SetScalarParameterValue(TEXT("Density"), P.Density);
		CardMid->SetScalarParameterValue(TEXT("Albedo"), P.Albedo);
		CardMid->SetScalarParameterValue(TEXT("EmitGain"), P.Flat6);   // a poeira não tem fogo: o EmitGain leva o contraste das 6 luzes
		CardMid->SetScalarParameterValue(TEXT("SoftDist"), P.SoftDist);
		CardMid->SetScalarParameterValue(TEXT("SceneLight"), 1.0f);       // a luz do mapa (o sol e o céu da receita do Look)
		CardMid->SetScalarParameterValue(TEXT("ClipAlpha"), 1.0f / 1024.0f);
		// os A/B de teste: -TreadGroundRule=0 (a cor do VC sem a regra de Hapke), -TreadUnitMask=0 (sem a máscara de unidade)
		float GroundRule = 1.0f, UnitMask = 1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("-TreadGroundRule="), GroundRule);
		FParse::Value(FCommandLine::Get(), TEXT("-TreadUnitMask="), UnitMask);
		CardMid->SetScalarParameterValue(TEXT("GroundRule"), GroundRule);
		CardMid->SetScalarParameterValue(TEXT("EmitScale"), P.PaleK);
		CardMid->SetScalarParameterValue(TEXT("EmberCut"), P.DustSat);   // a poeira não tem fogo: o EmberCut leva a saturação relativa   // a poeira não tem fogo: o EmitScale leva o expoente do pálido
		CardMid->SetScalarParameterValue(TEXT("UnitMask"), UnitMask);
		CardMid->SetScalarParameterValue(TEXT("ShadowOcc"), P.ShadowOcc);
		CardMid->SetScalarParameterValue(TEXT("ShadowSoft"), P.ShadowSoftM);
		CardMid->SetScalarParameterValue(TEXT("ShadowSky"), P.ShadowSky);
		CardMid->SetScalarParameterValue(TEXT("ShadowMode"), 0.0f);
		ProbeShadowMaterial(M);
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
	// a gravação da poeira em volume (rts.TreadDust.Path 1; sem ela a poeira fica nos cartões)
	RTSTreadDustVolume::LoadKickOnce(P.VolKick);
	RTSTreadDustVolume::LoadKickOnce(P.VolKickCoarse);
	bReady = Atlas.Frames > 1 && P.Layers.Num() > 0 && CardMid;
	return bReady;
}

void ARTSTreadDust::SetDustVisible(bool bOn)
{
	bCardsVisible = bOn;
	CardMesh->SetVisibility(bOn && !bVolumeOn);
	ShadowMesh->SetVisibility(bOn && !bVolumeOn);
}

void ARTSTreadDust::SetMaterialScalar(FName Name, float Value)
{
	if (Init() && CardMid)
	{
		CardMid->SetScalarParameterValue(Name, Value);
	}
}

void ARTSTreadDust::UseNorm8Sheets(bool bOn)
{
	if (!Init() || !CardMid)
	{
		return;
	}
	const FString Dir = FString::Printf(TEXT("/RTSTreadDust/%s%s/"), *P.Bake, bOn ? TEXT("_norm8") : TEXT(""));
	UTexture* SixPos = LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixPos.T_SixPos")));
	UTexture* SixNeg = LoadObject<UTexture>(nullptr, *(Dir + TEXT("T_SixNeg.T_SixNeg")));
	if (!SixPos || !SixNeg)
	{
		return;
	}
	CardMid->SetTextureParameterValue(TEXT("SixPos"), SixPos);
	CardMid->SetTextureParameterValue(TEXT("SixNeg"), SixNeg);
	CardMid->SetVectorParameterValue(TEXT("LightScale"), bOn ? FLinearColor(Atlas.LightScale.X, Atlas.LightScale.Y, P.MipMin, 0.0f) : FLinearColor(1.0f, 1.0f, P.MipMin, 0.0f));
	if (ShadowMid)
	{
		ShadowMid->SetTextureParameterValue(TEXT("SixPos"), SixPos);
	}
}

void ARTSTreadDust::Advance(double Dt)
{
	if (Init() && bEnabled && !bPaused)
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
	const bool bFogHides = ShadowModeNow() >= 1;   // nos modos novos o veículo sob a névoa não faz sombra (não entrega a posição)
	for (const FRTSTreadSource& S : Sources)
	{
		if (bFogHides && !S.bVisible) continue;
		// (23/10) o veículo de rodas: as mesmas duas caixas na escala dele (o tanque, 1: as de sempre)
		const bool bW = S.WheelHalfGaugeM > 0.0;
		const double Kx = bW ? S.HalfLenM / 3.95 : 1.0, Ky = bW ? S.HalfWidthM / 1.88 : 1.0, Kz = bW ? S.HeightM / 2.48 : 1.0;
		const FVector D = Pm - S.PosM;
		if (D.Z > 2.48 * Kz && ToSun.Z > 0.0) continue;
		const double Reach = 4.37 * FMath::Max(Kx, Ky) + FMath::Max(0.0, 2.48 * Kz - D.Z) * CotEl;
		if (D.X * D.X + D.Y * D.Y > Reach * Reach) continue;
		const FVector F = S.FwdM, R(S.FwdM.Y, -S.FwdM.X, 0.0);
		const double O[3] = { FVector::DotProduct(D, F), FVector::DotProduct(D, R), D.Z };
		const double V[3] = { FVector::DotProduct(ToSun, F), FVector::DotProduct(ToSun, R), ToSun.Z };
		for (const FHullBox& B : Boxes)
		{
			const double Lo[3] = { B.X0 * Kx, B.Y0 * Ky, B.Z0 * Kz }, Hi[3] = { B.X1 * Kx, B.Y1 * Ky, B.Z1 * Kz };
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
	// (23/10, as rodas; INTEGRACAO.md "As rodas") o caminhão de suprimento e o lançador soltam a MESMA nuvem aprovada do tanque (a gravação,
	// a luz, o sumiço), da roda de trás de cada lado, com a física do pneu que o jogo preenche: o lado e a massa do puff (DustScale,
	// DustAmount) e o rastro em comprimentos dele (TrailLenM). O que nasce ao longo da lagarta (a cortina) não sai da roda
	const bool bWheels = Src.WheelHalfGaugeM > 0.0;
	for (int32 Li = 0; Li < P.Layers.Num(); ++Li)
	{
		const FRTSTreadLayer& L = P.Layers[Li];
		// os presets: a nuvem sempre; o jato da média pra cima; a cortina da alta pra cima (a sombra é do ultra, no Draw)
		if ((L.Name == TEXT("K1") && Quality < 1) || (L.Name == TEXT("C") && Quality < 2))
		{
			continue;
		}
		if (IsImpactLayer(Li))
		{
			continue;                                // (23/10) as camadas dos acertos de míssil não saem dos veículos (AddImpactPuffs)
		}
		if (bWheels && L.XMax > -2.40)
		{
			continue;                                // (23/10) a cortina (nasce ao longo da lagarta) não sai da roda
		}
		const double SpacingMul = Quality == 0 && L.Name == TEXT("K2") ? 2.0 : 1.0;
		for (int32 K = 0; K < 2; ++K)
		{
			const double SideSign = K == 0 ? -1.0 : 1.0;
			// a lagarta (ou a roda) de dentro da curva anda menos: ±ω·a meia bitola
			const double Vk = Src.Speed + SideSign * Src.YawRate * (bWheels ? Src.WheelHalfGaugeM : 1.425);
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
			// (23/10) a roda: menos puffs por metro, cada um mais denso (o tanque, ×1)
			const double SpacingSrc = bWheels ? FMath::Max(Src.DustSpacing, 0.1) : 1.0;
			double& Ac = LayerAcc.FindOrAdd(Key);
			if (Amount < 0.025)
			{
				Ac = 0.0;
				continue;
			}
			Ac += V * Dt;
			double& Next = LayerNext.FindOrAdd(Key, L.Spacing * SpacingMul * SpacingSrc);
			while (Ac >= Next / Amount)
			{
				Ac -= Next / Amount;
				// o espaçamento: exponencial com a mesma média (Poisson, sem período) ou o sorteio uniforme de antes
				Next = L.SpacingPoisson > 0.5 ? L.Spacing * SpacingMul * SpacingSrc * -FMath::Loge(FMath::Max(Rand(), 1e-4))
					: L.Spacing * SpacingMul * SpacingSrc * Rnd(1.0, FMath::Max(1.0, L.SpacingJit));
				// a roda: o quarto de trás do pneu de trás (pra frente) ou o da frente do pneu da frente (de ré), como a motriz e a guia
				// da lagarta (x da motriz de −3,48 a −3,84: o raio dela), e o meio do pneu ± metade da meia largura (a lagarta: ±0,16 de 0,32)
				const double X = bWheels
					? (DirK > 0.0 ? Rnd(Src.WheelRearXM - Src.WheelRadiusM, Src.WheelRearXM) : Rnd(Src.WheelFrontXM, Src.WheelFrontXM + Src.WheelRadiusM))
					: Rnd(L.XMin, L.XMax) * DirK;
				const double Yj = bWheels ? 0.5 * Src.WheelHalfWidthM : L.YJit;
				const double Y = SideSign * (bWheels ? Src.WheelHalfGaugeM : L.YOff) + Rnd(-Yj, Yj);
				if (bAlong)
				{
					const double Wx = FMath::Lerp(Wr, Wf, FMath::Clamp((X - -2.40) / (2.55 - -2.40), 0.0, 1.0));
					if (Rand() * W > Wx) continue;
				}
				FCard Cd;
				Cd.P = Src.PosM + F * X + R * Y + FVector(0.0, 0.0, L.Z0);
				// a cor: a do chão onde nasce; o jato que pinga no asfalto leva a da areia presa (a de onde foi pega)
				const int64 TrackKey = int64(Src.Id) * 2 + K;
				if (W >= 0.5 || L.FollowLoad < 0.5)
				{
					Cd.Col = GroundAlbedoAt(Cd.P);
					PickedCol.Add(TrackKey, Cd.Col);
				}
				else
				{
					const FLinearColor* Pc = PickedCol.Find(TrackKey);
					Cd.Col = Pc ? *Pc : GroundAlbedoAt(Cd.P);
				}
				Cd.V = F * Vk * L.Inherit - F * DirK * L.Kick + R * (Rnd(-1.0, 1.0) * L.Side);
				Cd.V.Z += L.Up * Rnd(0.6, 1.3);
				Cd.Life = L.Life * (1.0 + Rnd(-L.LifeJit, L.LifeJit));
				const double S0 = L.ScaleLogMax > L.ScaleLogMin && L.ScaleLogMin > 0.0
					? FMath::Exp(Rnd(FMath::Loge(L.ScaleLogMin), FMath::Loge(L.ScaleLogMax))) : Rnd(1.0 - L.ScaleJit, 1.0 + L.ScaleJit);
				Cd.S = S0 * (L.FollowLoad > 0.5 ? FMath::Lerp(0.5, 1.0, W) : 1.0);
				if (L.ScaleSlow < 1.0 && L.VFull > L.V1)
				{
					Cd.S *= FMath::Lerp(L.ScaleSlow, 1.0, FMath::Clamp((V - L.V1) / (L.VFull - L.V1), 0.0, 1.0));
				}
				Cd.Rot = Rnd(-L.RotMax, L.RotMax);
				Cd.Flip = L.Flip != 0.0 ? L.Flip : (Rand() < 0.5 ? -1.0 : 1.0);
				Cd.Bake0 = L.BakeStart + Rnd(0.0, L.BakeStartJit);
				if (L.BakeJitFrames > 0.0 && Atlas.Times.Num() > 1)
				{
					// a defasagem da gravação: ±N quadros (cada cartão numa fase diferente da mesma simulação)
					const double FrameDt = (Atlas.Times.Last() - Atlas.Times[0]) / (Atlas.Times.Num() - 1);
					Cd.Bake0 = FMath::Max(0.0, Cd.Bake0 + Rnd(-L.BakeJitFrames, L.BakeJitFrames) * FrameDt);
				}
				Cd.RotFree = Rnd(-PI, PI);
				Cd.A = FMath::Min(1.0, Amount);
				Cd.Len = P.HullLen;
				if (bWheels)
				{
					// o puff da roda: o lado e a massa dele (a profundidade óptica do miolo vai com A; a massa, com A·S²)
					Cd.S *= Src.DustScale;
					Cd.A *= Src.DustAmount;
					Cd.Len = Src.TrailLenM > 0.0 ? Src.TrailLenM : P.HullLen;
					// o rastro no tempo (a poeira assenta em segundos, não em metros): o mais rápido deixa o rastro mais longo, com o puff
					// da mesma idade (só no comprimento, o caminhão a 10,5 m/s perdia a poeira com 0,8 s, nova e pequena)
					Cd.Len = FMath::Max(Cd.Len, V * Src.TrailTimeS);
				}
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

void ARTSTreadDust::AddBlast(const FRTSDustBlast& B)
{
	if (!B.bVisible || B.Scale <= 0.0)
	{
		return;                                  // sob a névoa: não empurra (não entrega a posição)
	}
	FBlastEv E;
	E.Pos = FVector2D(B.PosM.X, B.PosM.Y);
	E.T0 = Clock;
	// a bola do pulso aprovado (WebGL v6, BlastLab PassVelocity): 9 m × escala; dentro dela a poeira fica coberta pela
	// explosão desenhada e não é empurrada
	E.R = 9.0 * B.Scale;
	// a carga equivalente pela lei de Hopkinson (o raio cresce com W^(1/3)): a Carga Pesada (escala 1) = 20 kg, pelo raio
	// de 5 psi de 12,5 m (STATUS.md); explosão no chão = carga × 1,8 (UFC 3-340-02)
	E.W = 20.0 * B.Scale * B.Scale * B.Scale * 1.8;
	const FVector2D D(B.DirM.X, B.DirM.Y);
	if (B.HalfConeDeg < 179.0 && !D.IsNearlyZero())
	{
		// o cone da boca, com a borda macia de ±10° [E] (até a lei angular de Westine 1969)
		E.Dir = D.GetSafeNormal();
		E.CosLo = FMath::Cos(FMath::DegreesToRadians(FMath::Min(B.HalfConeDeg + 10.0, 180.0)));
		E.CosHi = FMath::Cos(FMath::DegreesToRadians(FMath::Max(B.HalfConeDeg - 10.0, 0.0)));
	}
	// o alcance: onde o deslocamento cai abaixo de 1 cm (1/3 de pixel na câmera perto do jogo)
	E.Reach = E.R;
	while (E.Reach < 400.0 && BlastXi(E, E.Reach) * P.BlastGain >= 0.01)
	{
		E.Reach += 0.5;
	}
	Blasts.Add(E);
}

void ARTSTreadDust::AddImpactPuffs(const TArray<FImpactPuff>& Puffs, uint32 PuffSeed)
{
	// (23/10, a explosão com vida do jogo; o jogo monta o véu, os pontinhos, o pouso e a onda de base) cada puff
	// vira um cartão da camada de impacto, com a cor do chão onde nasce; o sorteio à parte (a poeira das lagartas não muda)
	if (!Init() || !bEnabled || Puffs.IsEmpty())
	{
		return;
	}
	FRandomStream Rng(int32(PuffSeed & 0x7fffffffu));
	for (const FImpactPuff& Ip : Puffs)
	{
		const int32 Kind = FMath::Clamp(Ip.Layer, 0, 2);
		const int32 Li = ImpactLayers[ImpactLayers[Kind] >= 0 ? Kind : 0];
		if (!P.Layers.IsValidIndex(Li) || Ip.Mass <= 0.0 || Ip.SizeM <= 0.0)
		{
			continue;
		}
		const FRTSTreadLayer& L = P.Layers[Li];
		FCard Cd;
		Cd.P = Ip.PosM + FVector(0.0, 0.0, L.Z0);
		Cd.GroundZ = Ip.PosM.Z;
		Cd.V = Ip.VelM;
		Cd.Life = FMath::Max(Ip.LifeS, 0.1);
		Cd.Age = -FMath::Max(Ip.DelayS, 0.0);
		Cd.S = Ip.SizeM / FMath::Max(Atlas.SideM * L.Scale, 1e-3);
		Cd.Rot = L.RotMax * (2.0 * double(Rng.FRand()) - 1.0);
		Cd.Flip = L.Flip != 0.0 ? L.Flip : (Rng.FRand() < 0.5f ? -1.0 : 1.0);
		Cd.Bake0 = (Ip.BakeT0 >= 0.0 ? Ip.BakeT0 : L.BakeStart) + L.BakeStartJit * double(Rng.FRand());
		Cd.RotFree = PI * (2.0 * double(Rng.FRand()) - 1.0);
		// (23/10, a areia do acerto) a massa sem o corte em 1: no volume ela é linear (o corte fica só nos cartões, no Draw)
		Cd.A = FMath::Clamp(Ip.Mass, 0.0, 16.0);
		Cd.Back = FVector::ZeroVector;
		Cd.Col = GroundAlbedoAt(Cd.P);
		Cd.Layer = Li;
		Cd.Src = ImpactSrcId;
		Cd.Odo0 = 0.0;
		Cd.Len = P.HullLen;
		Cards.Add(Cd);
	}
}

int32 ARTSTreadDust::NumImpactCards() const
{
	int32 N = 0;
	for (const FCard& C : Cards)
	{
		N += C.Src == ImpactSrcId ? 1 : 0;
	}
	return N;
}

void ARTSTreadDust::SetWind(double YawDeg, double SpeedMs)
{
	// (23/10) o vento único do mapa (o mesmo do MPC_Wind, da fumaça e do BlastFx): só as camadas com windGain derivam nele
	const double R = FMath::DegreesToRadians(YawDeg);
	MapWind = FVector2D(FMath::Cos(R), FMath::Sin(R)) * FMath::Max(SpeedMs, 0.0);
}

double ARTSTreadDust::BlastXi(const FBlastEv& B, double R) const
{
	// o deslocamento total (m) do ar a R metros da explosão
	if (P.BlastModel == 1)
	{
		// Kinney & Graham 1985: o impulso positivo I/W^(1/3) [bar·ms/kg^(1/3)] por Z = R/W^(1/3); ξ = I/(ρ0·a0)
		const double W3 = FMath::Pow(B.W, 1.0 / 3.0);
		const double Z = FMath::Max(R, 0.1) / W3;
		const double Ib = 0.067 * FMath::Sqrt(1.0 + FMath::Pow(Z / 0.23, 4.0)) / (Z * Z * FMath::Pow(1.0 + FMath::Pow(Z / 1.55, 3.0), 1.0 / 3.0));
		return Ib * W3 * 100.0 / (1.225 * 340.0);   // 1 bar·ms = 100 Pa·s
	}
	// o aprovado: fonte de volume incompressível, u = 18 m/s × (Rb/R)² fora da bola por 0,15 s (WebGL battlesmoke.js
	// addBlast, BlastLab BattleSmoke.usf); 18 m/s ≈ 1,1 psi por Rankine–Hugoniot
	const double Rr = FMath::Max(R, B.R);
	return 18.0 * 0.15 * (B.R * B.R) / (Rr * Rr);
}

FVector2D ARTSTreadDust::BlastShift(const FCard& C, double T0, double T1) const
{
	FVector2D Sum = FVector2D::ZeroVector;
	const FVector2D Pc(C.P.X, C.P.Y);
	for (const FBlastEv& B : Blasts)
	{
		const FVector2D Dv = Pc - B.Pos;
		const double R = Dv.Size();
		if (R > B.Reach || R < 1e-3)
		{
			continue;
		}
		// chega em R/340 s e anda por BlastT com o perfil smoothstep (velocidade ξ·6s(1−s)/T): desliza e para, sem tranco;
		// a diferença do smoothstep no passo dá o deslocamento exato, qualquer que seja o passo
		const double Ta = B.T0 + R / 340.0;
		const double S0 = FMath::Clamp((T0 - Ta) / FMath::Max(P.BlastT, 1e-3), 0.0, 1.0);
		const double S1 = FMath::Clamp((T1 - Ta) / FMath::Max(P.BlastT, 1e-3), 0.0, 1.0);
		if (S1 <= S0)
		{
			continue;
		}
		const FVector2D Rh = Dv / R;
		double Wt = SmoothStep(R, 0.8 * B.R, B.R);        // nada dentro da bola desenhada
		if (B.CosLo > -1.5)
		{
			Wt *= SmoothStep(FVector2D::DotProduct(Rh, B.Dir), B.CosLo, B.CosHi);
		}
		const double Sm0 = S0 * S0 * (3.0 - 2.0 * S0), Sm1 = S1 * S1 * (3.0 - 2.0 * S1);
		Sum += Rh * (BlastXi(B, R) * P.BlastGain * Wt * (Sm1 - Sm0));
	}
	return Sum;
}

FVector2D ARTSTreadDust::AirAt(const FCard& C, const FRTSTreadLayer& L, const TMap<int32, int32>& SrcIndex) const
{
	// o ar dos OUTROS veículos no centro do cartão (m/s, no plano); a poeira do próprio veículo fica como foi aprovada
	FVector2D U2 = FVector2D::ZeroVector;
	const FVector2D Pc(C.P.X, C.P.Y);
	const double Zc = FMath::Max(C.P.Z - C.GroundZ, 0.0);
	const double B0 = FMath::Max(P.WakeB0, 0.3);
	const double Ln2 = 0.69314718;
	for (const auto& It : SrcIndex)
	{
		if (It.Key == C.Src)
		{
			continue;
		}
		const FRTSTreadSource& S = Sources[It.Value];
		const double Ramp = SrcRamp.FindRef(S.Id);
		const double Uv = FMath::Abs(S.Speed);
		const FVector2D Pos(S.PosM.X, S.PosM.Y);
		if (Ramp <= 0.0 || FVector2D::DistSquared(Pc, Pos) > 45.0 * 45.0)
		{
			continue;                            // invisível (névoa) ou longe: 45 m cobre a proa e as migalhas
		}
		if (Uv > 0.3)
		{
			// a proa: o corpo de Rankine no semiespaço (o chão é o plano de simetria). Fonte Q = U·A recuada ε do nariz e
			// sumidouro −Q no fim da bolha (traseira + BubbleH·H), os dois suavizados por ε = √(A/2π), a distância de
			// estagnação do mesmo corpo: sem teto nem constante solta (a revisão de 16/10: o pico cai a ~0,4 U, sem trator).
			// No referencial do chão sobra só a perturbação (Batchelor 1967)
			const FVector2D Dm = FVector2D(S.FwdM.X, S.FwdM.Y).GetSafeNormal() * (S.Speed >= 0.0 ? 1.0 : -1.0);
			const double A = FMath::Max(S.FrontalM2, 0.5);
			const double Eps = FMath::Sqrt(A / (2.0 * PI)), Eps2 = Eps * Eps;
			const double Q = Uv * A;
			const FVector2D Src = Pos + Dm * (S.HalfLenM - Eps);
			const FVector2D Snk = Pos - Dm * (S.HalfLenM + P.BubbleH * S.HeightM);
			const FVector2D Ds = Pc - Src, Dk = Pc - Snk;
			const double Rs = Ds.SizeSquared() + Zc * Zc + Eps2, Rk = Dk.SizeSquared() + Zc * Zc + Eps2;
			U2 += (Ds / (Rs * FMath::Sqrt(Rs)) - Dk / (Rk * FMath::Sqrt(Rk))) * (Q / (2.0 * PI) * Ramp);
		}
		// a esteira pelas migalhas: o ponto mais perto do caminho da traseira, gaussiana de meia largura b0, amplitude
		// ancorada no Inherit do perfil (na linha da lagarta, 1,425 m, o ar anda Inherit·U, como a poeira do próprio
		// tanque) e o decaimento pelo Drag do perfil [J] (a física dá t^(−2/3), mais longa: mudaria o rastro aprovado)
		const TArray<FCrumb>* Cr = Crumbs.Find(S.Id);
		if (Cr && Cr->Num() >= 2)
		{
			// a esteira de cada trecho contínuo (a ré e a meia-volta abrem um trecho novo): a migalha mais perto de cada
			// um, e os trechos somam (o modelo é linear); assim a ida e a volta pelo mesmo caminho não piscam
			double BestD2 = 1e30, BestT = 0.0, BestU = 0.0;
			FVector2D BestDir = FVector2D::ZeroVector;
			auto Flush = [&]()
			{
				const double Age = FMath::Max(Clock - BestT, 0.0);
				if (BestD2 < 25.0 * B0 * B0 && BestU > 0.3)
				{
					const double Du0 = L.Inherit * BestU * FMath::Exp(Ln2 * 1.425 * 1.425 / (B0 * B0));
					U2 += BestDir * (Du0 * FMath::Exp(-Ln2 * BestD2 / (B0 * B0)) * FMath::Exp(-L.Drag * Age) * Ramp);
				}
				BestD2 = 1e30;
			};
			for (int32 K = 0; K + 1 < Cr->Num(); ++K)
			{
				const FCrumb& A0 = (*Cr)[K];
				const FCrumb& A1 = (*Cr)[K + 1];
				if (A1.bBreak)
				{
					Flush();                             // o trecho acabou: a migalha seguinte começa outro
					continue;
				}
				const FVector2D Seg = A1.Pos - A0.Pos;
				const double Len2 = Seg.SizeSquared();
				const double Tt = Len2 > 1e-6 ? FMath::Clamp(FVector2D::DotProduct(Pc - A0.Pos, Seg) / Len2, 0.0, 1.0) : 0.0;
				const double D2 = FVector2D::DistSquared(Pc, A0.Pos + Seg * Tt);
				if (D2 < BestD2)
				{
					BestD2 = D2;
					BestT = FMath::Lerp(A0.T, A1.T, Tt);
					BestU = FMath::Lerp(A0.U, A1.U, Tt);
					BestDir = (A0.Dir * (1.0 - Tt) + A1.Dir * Tt).GetSafeNormal();
				}
			}
			Flush();
		}
	}
	return U2;
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
		if (S.bEmitsDust)
		{
			Emit(S, Dt);                         // o veículo de roda e a carcaça só fazem sombra e empurram o ar
		}
	}
	++StepCount;
	const double Clock0 = Clock, Clock1 = Clock + Dt;
	if (P.bInteract)
	{
		// a rampa de 0,15 s quando um veículo entra ou sai da névoa (ou da lista): o empurrão não liga num quadro
		for (const FRTSTreadSource& S : Sources)
		{
			double& Rm = SrcRamp.FindOrAdd(S.Id, 0.0);
			Rm = FMath::Clamp(Rm + (S.bVisible ? 1.0 : -1.0) * Dt / 0.15, 0.0, 1.0);
			// as migalhas da esteira: a traseira (no sentido do movimento) a cada b0/2 andados; vivem 3,4 s (aí a esteira
			// desloca menos de 1 cm no resto da vida)
			TArray<FCrumb>& Cr = Crumbs.FindOrAdd(S.Id);
			const double Uv = FMath::Abs(S.Speed);
			if (Uv > 0.3)
			{
				const FVector2D Dm = FVector2D(S.FwdM.X, S.FwdM.Y).GetSafeNormal() * (S.Speed >= 0.0 ? 1.0 : -1.0);
				const FVector2D Rear = FVector2D(S.PosM.X, S.PosM.Y) - Dm * S.HalfLenM;
				if (Cr.Num() == 0 || FVector2D::Distance(Cr.Last().Pos, Rear) >= 0.5 * P.WakeB0)
				{
					// ré, meia-volta ou o salto da traseira (a frente e a traseira trocam na ré): trecho novo
					const bool bBreak = Cr.Num() > 0 && (FVector2D::DotProduct(Dm, Cr.Last().Dir) < 0.0
						|| FVector2D::Distance(Cr.Last().Pos, Rear) > 2.0 * P.WakeB0);
					FCrumb& Nc = Cr.AddDefaulted_GetRef();
					Nc.bBreak = bBreak;
					Nc.Pos = Rear;
					Nc.Dir = Dm;
					Nc.U = Uv;
					Nc.T = Clock1;
				}
			}
			int32 Old = 0;
			while (Old < Cr.Num() && Clock1 - Cr[Old].T > 3.4)
			{
				++Old;
			}
			if (Old > 0)
			{
				Cr.RemoveAt(0, Old);
			}
		}
		for (auto It = Crumbs.CreateIterator(); It; ++It)
		{
			if (!SrcIndex.Contains(It.Key()))
			{
				It.RemoveCurrent();              // o veículo saiu da lista (morreu ou ficou longe)
			}
		}
		for (auto It = SrcRamp.CreateIterator(); It; ++It)
		{
			if (!SrcIndex.Contains(It.Key()))
			{
				It.RemoveCurrent();              // quem volta pra lista entra com a rampa do 0; quem sai corta seco (morreu ou fora da tela)
			}
		}
		Blasts.RemoveAll([this, Clock0](const FBlastEv& B) { return Clock0 > B.T0 + B.Reach / 340.0 + P.BlastT; });
	}
	else
	{
		Blasts.Reset();
	}
	// cada cartão só mexe nele mesmo (as fontes, os metros andados e o sol só são lidos): os núcleos dividem a lista;
	// com poucos cartões o custo de dividir passa do ganho
	const int32 ShMode = ShadowModeNow();
	const URTSSunShadowSubsystem* SunMap = SunShadow();
	ParallelFor(Cards.Num(), [&](int32 Index)
	{
		FCard& C = Cards[Index];
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const int32 CardIndex = Index + 1;
		if (C.Age < 0.0)
		{
			C.Age += Dt;                             // (23/10) o puff de impacto que ainda não nasceu (a chegada do choque): parado e invisível
			return;
		}
		C.Age += Dt;
		const double Dk = FMath::Exp(-L.Drag * Dt);
		// o vento: o do perfil (todas as camadas, 0 no aprovado) mais o do mapa × o windGain da camada (23/10: as dos acertos; a K2, 0)
		const double Wx = P.WindX + MapWind.X * L.WindGain, Wy = P.WindY + MapWind.Y * L.WindGain;
		C.V = FVector(C.V.X * Dk + Wx * (1.0 - Dk), C.V.Y * Dk + Wy * (1.0 - Dk), C.V.Z * Dk - L.Sink * (1.0 - Dk));
		FVector Extra = FVector::ZeroVector;
		if (L.WakeRise > 0.0 || L.WakePull > 0.0 || L.WakeGrow > 0.0)
		{
			if (const int32* Si = SrcIndex.Find(C.Src))
			{
				const FRTSTreadSource& S = Sources[*Si];
				const double Behind = Odo.FindRef(S.Id) - C.Odo0;
				const double V = FMath::Abs(S.Speed);
				if (V > 0.3 && Behind < L.WakeLen * C.Len)
				{
					const double Fall = 1.0 - Behind / (L.WakeLen * C.Len);
					const FVector Rr(S.FwdM.Y, -S.FwdM.X, 0.0);
					const double Lat = FVector::DotProduct(C.P - S.PosM, Rr);
					Extra = FVector(0.0, 0.0, L.WakeRise * V * Fall) - Rr * (Lat * L.WakePull * V * Fall);
					C.Wake += L.WakeGrow * V * Fall * Dt;
				}
			}
		}
		C.P += (C.V + Extra) * Dt;
		if (P.bInteract)
		{
			// o ar dos outros veículos e das explosões: soma na posição, só no plano (nada sobe; o alfa não muda)
			const FVector2D Air = AirAt(C, L, SrcIndex) * Dt + (Blasts.Num() ? BlastShift(C, Clock0, Clock1) : FVector2D::ZeroVector);
			C.P.X += Air.X;
			C.P.Y += Air.Y;
		}
		C.P.Z = FMath::Max(C.P.Z, C.GroundZ);
		if (C.Sun < 0.0 || C.Map[0] < 0.0 || (StepCount + CardIndex) % 3 == 0)
		{
			const double Half = Atlas.SideM * L.Scale * C.S * 0.25;
			const double Sv = 0.5 * (SunVisAt(C.P + FVector(0.0, 0.0, 0.3), SunDir) + SunVisAt(C.P + FVector(0.0, 0.0, 0.3 + Half), SunDir));
			C.Sun = C.Sun < 0.0 ? Sv : C.Sun + (Sv - C.Sun) * (1.0 - FMath::Exp(-3.0 * Dt / 0.15));
			// a sombra do cenário (casa, árvore, relevo) no pé, no meio e no topo do cartão
			const double Top = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * FMath::Min(C.Age / FMath::Max(C.Life, 1e-3), 1.0)) * C.Wake;
			const double Hs[3] = { 0.3, 0.3 + 0.4 * Top, 0.3 + 0.8 * Top };
			for (int32 Hi = 0; Hi < 3; ++Hi)
			{
				const double Mv = ShMode == 2 && SunMap ? SunMap->SunVisibility(C.P, C.P.Z - C.GroundZ + Hs[Hi]) : MapSunVis(C.P + FVector(0.0, 0.0, Hs[Hi]));
				C.Map[Hi] = C.Map[Hi] < 0.0 ? Mv : C.Map[Hi] + (Mv - C.Map[Hi]) * (1.0 - FMath::Exp(-3.0 * Dt / 0.15));
			}
		}
	}, Cards.Num() < 512 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	if (Blasts.Num() > 0 && FParse::Param(FCommandLine::Get(), TEXT("TreadBlastLog")))
	{
		// depuração do empurrão: quantos cartões a explosão alcança e o maior deslocamento deste passo
		int32 NIn = 0;
		double MaxShift = 0.0, MinR = 1e9;
		for (const FCard& C : Cards)
		{
			const double Rb = FVector2D::Distance(FVector2D(C.P.X, C.P.Y), Blasts[0].Pos);
			MinR = FMath::Min(MinR, Rb);
			NIn += Rb <= Blasts[0].Reach ? 1 : 0;
			MaxShift = FMath::Max(MaxShift, BlastShift(C, Clock0, Clock1).Size());
		}
		UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: explosão: relógio %.3f→%.3f, T0 %.3f, R %.2f m, alcance %.1f m, %d de %d cartões no alcance, o mais perto a %.1f m, maior empurrão no passo %.3f m (interação %d)"),
			Clock0, Clock1, Blasts[0].T0, Blasts[0].R, Blasts[0].Reach, NIn, Cards.Num(), MinR, MaxShift, P.bInteract ? 1 : 0);
	}
	Clock = Clock1;
	Cards.RemoveAll([this](const FCard& C)
	{
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		if (L.MaskEnd > 0.0)
		{
			const double D = (Odo.FindRef(C.Src) - C.Odo0) / C.Len;
			if (D >= L.MaskEnd || (L.FadeTau <= 0.0 && 1.0 - SmoothStep(D, L.MaskDense, L.MaskEnd) < 0.05)) return true;
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
		Mesh->CreateMeshSection_LinearColor(0, V, Tri, N, U0, U1, U2, U3, Cl, Tg, false, true);
		MeshCap[Slot] = Cap;
		MeshVerts[Slot] = K;
	}
	else
	{
		Mesh->UpdateMeshSection_LinearColor(0, V, N, U0, U1, U2, U3, Cl, Tg, true);
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
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake * (1.0 + L.GrowFade) + L.RiseFade;
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
	// a mesma poeira numa sombra esticada pelo sol baixo escurece cada ponto do chão menos, na proporção do esticamento;
	// calibrada no sol do game shot r1 (25°, cot 2,14): com o sol do jogo a 8° a sombra vira mancha de óleo sem isso (16/10)
	const double ShadowThin = FMath::Min(1.0, 2.14 / FMath::Max(Cot, 1e-3));
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
	TMap<int32, int32> SrcAt;
	for (int32 Si = 0; Si < Sources.Num(); ++Si)
	{
		SrcAt.Add(Sources[Si].Id, Si);
	}
	const FVector2D SunH = FVector2D(SunDir.X, SunDir.Y).GetSafeNormal();
	// a sombra (o perfil "shadow"): 0 = o cenário no UV2.x e o tanque no UV3.y (a de antes); 1 = os dois no UV2.x, que o
	// material troca pelo céu de volume; 2 = por pixel, o UV2.x leva a meia corda da nuvem (m) e o material lê as alturas
	const int32 ShMode = ShadowModeNow();
	const double ChordCap = 0.25 / FMath::Max(FMath::Abs(CamForward.Z), 0.3);
	ParallelFor(N, [&](int32 I)
	{
		const FCard& C = *Order[I].C;
		const FRTSTreadLayer& L = P.Layers[C.Layer];
		const double K = C.Age / FMath::Max(C.Life, 1e-3);
		// (23/10) o puff de impacto leva massa (A até 16, linear no volume); o cartão não passa de opaco: corta antes do andamento (a
		// entrada e o sumiço ficam com a forma de sempre, em vez de opaco até o fim e sumindo num instante)
		const double Ca = C.Src == ImpactSrcId ? FMath::Min(1.0, C.A * RTSTreadDustVolume::ImpactGain()) : C.A;
		double A = L.Alpha * Ca * SmoothStep(K, 0.0, FMath::Max(L.Attack, 1e-3)) * (1.0 - SmoothStep(K, L.Decay, 1.0));
		double Mf = L.MaskEnd > 0.0 ? SmoothStep((Odo.FindRef(C.Src) - C.Odo0) / C.Len, L.MaskDense, L.MaskEnd) : 0.0;
		if (L.FadeTau > 0.0)
		{
			Mf = FMath::Max(Mf, SmoothStep(K, 0.6, 1.0));   // tanque parado: o sumiço anda pela idade
		}
		// o crescimento do sumiço com freio (rápido no começo, como a esteira: ~58% na 1ª metade)
		const double Gf = 1.0 + L.GrowFade * (1.0 - (1.0 - Mf) * (1.0 - Mf));
		double Kd = 1.0;                         // a densidade do cartão (vai no VC.a): 1 = a gravação
		if (L.FadeTau > 0.0)
		{
			Kd = FMath::Exp(-L.FadeTau * Mf) / (Gf * Gf);
			A *= 1.0 - SmoothStep(Mf, L.FadeAlphaFrom, 1.0);
		}
		else
		{
			A *= 1.0 - Mf;
		}
		A = FMath::Min(1.0, A);
		if (C.bDouble)
		{
			A = 1.0 - (1.0 - A) * (1.0 - A);       // a baixa: o cartão cobre o que os dois sobrepostos cobririam
		}
		const double Frame = Atlas.FrameAt(C.Bake0 + C.Age * L.BakeRate);
		const int32 Fi = FMath::Clamp(FMath::FloorToInt32(Frame), 0, Atlas.Frames - 1), Fj = FMath::Min(Fi + 1, Atlas.Frames - 1);
		const double Size = Atlas.SideM * L.Scale * C.S * (1.0 + (L.GrowTo - 1.0) * K) * C.Wake * Gf;
		const double PvX = Atlas.Pivot.X;
		const double PvY = L.PivotBottom > 0.5 ? FMath::Lerp(Atlas.BallBottom[Fi], Atlas.BallBottom[Fj], FMath::Clamp(Frame - Fi, 0.0, 1.0)) : Atlas.Pivot.Y;
		const double CardSun = C.Sun < 0.0 ? 1.0 : C.Sun;
		// a erosão pelo ruído (o material): devagar no começo e rápido no fim (o estudo de 16/10: E = 3·m^2,5)
		const double Erode = FMath::Max(L.Erode * (L.ErodeFromBirth > 0.5 ? 1.0 : SmoothStep(K, 0.35, 1.0)), L.ErodeFade * FMath::Pow(Mf, 2.5));
		// o espelho: sorteado, ou (orient) o lado cheio da gravação pro lado de trás do tanque na tela
		double Flip = C.Flip, XScale = 1.0;
		if (L.Orient > 0.5 && !C.Back.IsNearlyZero())
		{
			const double Bx = FVector::DotProduct(C.Back, CamRight);
			Flip = (Bx >= 0.0 ? 1.0 : -1.0) * (C.Flip < 0.0 ? -1.0 : 1.0);
			XScale = FMath::Max(FMath::Abs(Bx), FMath::Abs(CamForward.Z));
		}
		double CardRot = C.Rot;
		if (L.RotFreeAfter > 0.0)
		{
			// depois de rotFreeAfter cascos o cartão vai girando até o giro livre (sem pular)
			const double Behind = (Odo.FindRef(C.Src) - C.Odo0) / C.Len;
			CardRot = FMath::Lerp(C.Rot, C.RotFree, SmoothStep(Behind, L.RotFreeAfter, L.RotFreeAfter + 0.35));
		}
		const double Cs = FMath::Cos(CardRot), Sn = FMath::Sin(CardRot);
		const FVector R = Cs * CamRight + Sn * CamUp, Up = -Sn * CamRight + Cs * CamUp;
		const FVector L3(FVector::DotProduct(Sd, R) * Flip, FVector::DotProduct(Sd, Up), FVector::DotProduct(Sd, ToCam));
		const FVector2D Pivot(Flip > 0 ? PvX - 0.5 : 0.5 - PvX, PvY - 0.5);
		FVector Pc = C.P + FVector(0.0, 0.0, L.RiseFade * Mf);   // a subida do sumiço (só no desenho)
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
			const FVector Vm = Pc + (R * (Qp.X * XScale) + Up * Qp.Y) * Sz;
			V[W] = Vm * 100.0 - Origin100;
			Nrm[W] = L3;                                       // a direção do sol no cartão (o material lê o VertexNormalWS)
			U0[W] = FVector2D(Flip < 0 ? 0.5 - Q.X : Q.X + 0.5, Q.Y + 0.5);
			U1[W] = FVector2D(Frame, A);
			double SunVis = ShMode == 0 ? CardSun : 1.0;
			const double Hh = FMath::Lerp(H0, H1, FMath::Clamp((Q.Y - BoxY0) / FMath::Max(BoxY1 - BoxY0, 1e-6), 0.0, 1.0));
			if (L.SunSide > 0.0)
			{
				// o lado do rastro virado pro sol: a distância do vértice ao eixo do veículo, medida na direção do sol
				if (const int32* Si = SrcAt.Find(C.Src))
				{
					const FRTSTreadSource& S = Sources[*Si];
					const FVector2D Rr(S.FwdM.Y, -S.FwdM.X);
					const double Lat = FVector2D::DotProduct(FVector2D(Vm.X - S.PosM.X, Vm.Y - S.PosM.Y), Rr) * FVector2D::DotProduct(Rr, SunH);
					const double Sv = 0.5 + 0.5 * FMath::Clamp(Lat / FMath::Max(L.SunSideWidth, 0.1), -1.0, 1.0);
					const double Side = ShMode == 0 ? L.SunSide : FMath::Clamp(L.SunSide * P.ShadowSunSide, 0.0, 1.0);
					SunVis *= FMath::Lerp(1.0 - Side, 1.0, Sv);
				}
			}
			if (L.BaseShade < 1.0)
			{
				// a base escura: pela altura do vértice acima do chão (a nuvem se faz sombra perto do chão, uma faixa só) ou,
				// sem baseShadeWorld, pela base de cada cartão (com muitos cartões em alturas diferentes vira anéis escuros)
				const double Hb = L.BaseShadeWorld > 0.0 ? FMath::Clamp((Vm.Z - C.GroundZ) / L.BaseShadeWorld, 0.0, 1.0) : Hh;
				SunVis *= FMath::Lerp(L.BaseShade, 1.0, Hb);
			}
			// o cenário: pé (0), meio (0,5), topo (1) pela altura do vértice na bola. Vai no UV2.x (1 = sombra), que corta só
			// o sol no material: a pedra longe tapa o sol mas não o céu (a sombra do tanque, no UV3.y, tapa os dois)
			const double MapVis = C.Map[0] < 0.0 ? 1.0 : Hh < 0.5 ? FMath::Lerp(C.Map[0], C.Map[1], Hh * 2.0) : FMath::Lerp(C.Map[1], C.Map[2], Hh * 2.0 - 1.0);
			const double U2x = ShMode == 0 ? 1.0 - MapVis : ShMode == 1 ? 1.0 - MapVis * CardSun : Size * FMath::Min(P.ShadowChord, ChordCap);
			U2[W] = FVector2D(U2x, Vm.Z - C.GroundZ);    // y: a altura do vértice acima do chão (a máscara de unidade)
			U3[W] = FVector2D(Erode, SunVis);
			Cl[W] = FLinearColor(C.Col.R, C.Col.G, C.Col.B, float(Kd));   // o albedo do chão onde nasceu (sRGB) e a densidade
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
				SU1[Ws] = FVector2D(Frame, A * L.Shadow * ShadowThin * CardSun * (C.Map[0] >= 0.0 ? C.Map[0] : 1.0)   // na sombra da casa, não escurece de novo
					* (L.FadeTau > 0.0 ? 1.0 - FMath::Pow(Mf, 1.7) : 1.0));                                    // e some junto com a nuvem
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

void ARTSTreadDust::CaptureGroundColor()
{
	bGroundDirty = false;
	GroundCol.Reset();
	UWorld* World = GetWorld();
	if (!World || FParse::Param(FCommandLine::Get(), TEXT("TreadNoGround")))
	{
		return;                                   // sem captura: a poeira usa o albedo da reserva do perfil
	}
	const double T0 = FPlatformTime::Seconds();
	// só o terreno (sem unidade, prédio, capim ou pedra): a poeira sai da camada solta do chão
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Show;
	FBox Bounds(ForceInit);
	// todo componente desenhado do ator do terreno: com o Landscape em Nanite quem desenha é o LandscapeNaniteComponent,
	// não o ULandscapeComponent (só ele na lista = nada desenhado, a captura preta de 16/10)
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Lc = *It;
		if (Lc && Lc->GetWorld() == World && Lc->IsRegistered() && Lc->IsVisible() && Lc->GetOwner() && Lc->GetOwner()->IsA<ALandscapeProxy>())
		{
			Show.Add(Lc);
			if (Lc->IsA<ULandscapeComponent>())
			{
				Bounds += Lc->Bounds.GetBox();
			}
		}
	}
	if (Show.Num() == 0 || !Bounds.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: sem terreno pra captura da cor do chão; a poeira usa a cor da reserva"));
		return;
	}
	// de cima, ortográfica, cobrindo o terreno inteiro: a BaseColor do GBuffer (o albedo, sem luz nem exposição)
	if (FParse::Param(FCommandLine::Get(), TEXT("TreadGroundDebug")))
	{
		const FRotator Tries[3] = { FRotator(-90.0, 0.0, 0.0), FRotator(-80.0, 0.0, 0.0), FRotationMatrix::MakeFromX(-SunDir).Rotator() };
		for (int32 Ti = 0; Ti < 3; ++Ti)
		{
			UTextureRenderTarget2D* Dt = NewObject<UTextureRenderTarget2D>(this);
			Dt->RenderTargetFormat = RTF_R32f;
			Dt->ClearColor = FLinearColor(-1.0f, 0, 0, 0);
			Dt->InitAutoFormat(256, 256);
			Dt->UpdateResourceImmediate(true);
			USceneCaptureComponent2D* Dc = NewObject<USceneCaptureComponent2D>(this);
			Dc->bCaptureEveryFrame = false;
			Dc->bCaptureOnMovement = false;
			Dc->ProjectionType = ECameraProjectionMode::Orthographic;
			Dc->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			Dc->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
			Dc->RegisterComponent();
			Dc->TextureTarget = Dt;
			Dc->OrthoWidth = FMath::Max(Bounds.GetSize().X, Bounds.GetSize().Y);
			Dc->ShowOnlyComponents = Show;
			const FVector Fd = FRotationMatrix(Tries[Ti]).GetScaledAxis(EAxis::X);
			Dc->SetWorldLocationAndRotation(Bounds.GetCenter() - Fd * (Bounds.GetSize().Size() + 5000.0), Tries[Ti]);
			Dc->CaptureScene();
			TArray<FLinearColor> Px;
			Dt->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Px, FReadSurfaceDataFlags(RCM_MinMax));
			int32 Hit = 0;
			float Mn = 1e30f, Mx = -1e30f;
			for (const FLinearColor& Pv : Px)
			{
				if (Pv.R > 0.0f && Pv.R < 1e7f) { ++Hit; Mn = FMath::Min(Mn, Pv.R); Mx = FMath::Max(Mx, Pv.R); }
			}
			UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: depuração da captura do chão %d (giro %s): %d de %d pixels com profundidade, %.0f a %.0f cm"),
				Ti, *Tries[Ti].ToString(), Hit, Px.Num(), Mn, Mx);
			Dc->DestroyComponent();
		}
	}
	const FRotator Rot(-90.0, 0.0, 0.0);
	const FRotationMatrix Rm(Rot);
	GroundX = Rm.GetScaledAxis(EAxis::Y);
	GroundY = Rm.GetScaledAxis(EAxis::Z);
	const FVector Size = Bounds.GetSize();
	GroundW = FMath::Max(Size.X, Size.Y) + 1000.0;
	GroundO = FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 10000.0);
	UTextureRenderTarget2D* Rt = NewObject<UTextureRenderTarget2D>(this);
	Rt->RenderTargetFormat = RTF_RGBA16f;
	Rt->ClearColor = FLinearColor::Black;
	Rt->InitAutoFormat(GroundRes, GroundRes);
	Rt->UpdateResourceImmediate(true);
	if (!GroundCapture)
	{
		GroundCapture = NewObject<USceneCaptureComponent2D>(this);
		GroundCapture->bCaptureEveryFrame = false;
		GroundCapture->bCaptureOnMovement = false;
		GroundCapture->ProjectionType = ECameraProjectionMode::Orthographic;
		GroundCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		GroundCapture->RegisterComponent();
	}
	GroundCapture->TextureTarget = Rt;
	GroundCapture->OrthoWidth = GroundW;
	GroundCapture->ShowOnlyComponents = Show;
	GroundCapture->SetWorldLocationAndRotation(GroundO, Rot);
	// 1º a BaseColor do GBuffer; se vier preta (aconteceu no mapa de teste, 16/10), o terreno sem luz (unlit: a cor da
	// cena é a própria cor base), sem névoa, céu nem exposição automática
	FLinearColor Sum = FLinearColor::Black;
	FString Mode = TEXT("BaseColor do GBuffer");
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		if (Attempt == 0)
		{
			GroundCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
		}
		else
		{
			Mode = TEXT("cena sem luz");
			GroundCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;
			GroundCapture->ShowFlags.SetLighting(false);
			GroundCapture->ShowFlags.SetFog(false);
			GroundCapture->ShowFlags.SetAtmosphere(false);
			GroundCapture->ShowFlags.SetVolumetricFog(false);
			GroundCapture->ShowFlags.SetEyeAdaptation(false);
			GroundCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
			GroundCapture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
			GroundCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
			GroundCapture->PostProcessSettings.AutoExposureBias = 0.0f;
			GroundCapture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			GroundCapture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		}
		GroundCapture->CaptureScene();
		FTextureRenderTargetResource* Res = Rt->GameThread_GetRenderTargetResource();
		GroundCol.SetNumUninitialized(GroundRes * GroundRes);
		const int32 Strip = 512;
		Sum = FLinearColor::Black;
		for (int32 Y0 = 0; Y0 < GroundRes; Y0 += Strip)
		{
			TArray<FLinearColor> Px;
			FReadSurfaceDataFlags Flags(RCM_MinMax);
			if (!Res || !Res->ReadLinearColorPixels(Px, Flags, FIntRect(0, Y0, GroundRes, Y0 + Strip)) || Px.Num() != GroundRes * Strip)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: não li a captura da cor do chão; a poeira usa a cor da reserva"));
				GroundCol.Reset();
				return;
			}
			for (int32 I = 0; I < Px.Num(); ++I)
			{
				const FLinearColor Cv(FMath::Clamp(Px[I].R, 0.0f, 1.0f), FMath::Clamp(Px[I].G, 0.0f, 1.0f), FMath::Clamp(Px[I].B, 0.0f, 1.0f), 1.0f);
				GroundCol[Y0 * GroundRes + I] = Cv.ToFColor(true);   // sRGB em 8 bits: o linear perde as terras escuras
				Sum += Cv;
			}
		}
		if (Sum.R + Sum.G + Sum.B > 1e-3f * GroundRes * GroundRes)
		{
			break;
		}
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: a captura '%s' veio preta"), *Mode);
	}
	if (Sum.R + Sum.G + Sum.B <= 1e-3f * GroundRes * GroundRes)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: a cor do chão veio preta nas duas capturas; a poeira usa a cor da reserva"));
		GroundCol.Reset();
		Rt->ReleaseResource();
		GroundCapture->TextureTarget = nullptr;
		return;
	}
	Rt->ReleaseResource();
	GroundCapture->TextureTarget = nullptr;
	if (FParse::Param(FCommandLine::Get(), TEXT("TreadGroundDump")))
	{
		FFileHelper::CreateBitmap(*(FPaths::ProjectSavedDir() / TEXT("ground_color.bmp")), GroundRes, GroundRes, GroundCol.GetData());
	}
	Sum /= float(GroundRes * GroundRes);
	for (FCard& C : Cards)
	{
		C.Col = GroundAlbedoAt(C.P);
	}
	PickedCol.Reset();
	UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: cor do chão capturada (%s), %d peças do terreno, %.0f m de lado (%.0f cm por pixel), albedo médio (%.3f, %.3f, %.3f), %.0f ms"),
		*Mode, Show.Num(), GroundW / 100.0, GroundW / GroundRes, Sum.R, Sum.G, Sum.B, (FPlatformTime::Seconds() - T0) * 1000.0);
}

FLinearColor ARTSTreadDust::GroundAlbedoAt(const FVector& Pm) const
{
	if (GroundCol.Num() == 0)
	{
		return FLinearColor(P.GroundR, P.GroundG, P.GroundB);
	}
	// a média de ~1,5 m em volta (3 × 3 pontos), em linear: o rastro leva a cor do chão, não a de um grão
	const FVector D = Pm * 100.0 - GroundO;
	const double Fu = (0.5 + FVector::DotProduct(D, GroundX) / GroundW) * GroundRes - 0.5;
	const double Fv = (0.5 - FVector::DotProduct(D, GroundY) / GroundW) * GroundRes - 0.5;
	const int32 Step = FMath::Max(1, FMath::RoundToInt32(75.0 / (GroundW / GroundRes)));
	FLinearColor Acc = FLinearColor::Black;
	int32 N = 0;
	for (int32 Dy = -1; Dy <= 1; ++Dy)
	{
		for (int32 Dx = -1; Dx <= 1; ++Dx)
		{
			const int32 U = FMath::RoundToInt32(Fu) + Dx * Step, V = FMath::RoundToInt32(Fv) + Dy * Step;
			if (U >= 0 && V >= 0 && U < GroundRes && V < GroundRes)
			{
				const FColor& Gc = GroundCol[V * GroundRes + U];
				if (Gc.R + Gc.G + Gc.B > 0)                     // preto = fresta entre pedaços do terreno, fora da média
				{
					Acc += FLinearColor(Gc);                    // FColor sRGB → linear
					++N;
				}
			}
		}
	}
	return N ? Acc / float(N) : FLinearColor(P.GroundR, P.GroundG, P.GroundB);
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
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Show, ShowTerrain;
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
		if (Ow && Ow->IsA<ALandscapeProxy>())
		{
			ShowTerrain.Add(Pc);
		}
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
	ShadowCapture->SetWorldLocationAndRotation(ShadowO, Rot);
	// duas fotos do sol, a mesma câmera: o terreno sozinho e depois tudo. O relevo em Nanite desenhado sai até ~0,5 m fora
	// da colisão, o que com o sol a 8° vira ~4 m ao longo do raio (a folga de 400 cm de 16/10, que apagava a sombra da casa
	// nos primeiros 4 m atrás da parede). Com o terreno à parte, um pixel onde a foto de tudo bate antes do terreno é casa,
	// pedra ou árvore, e essa comparação é exata (as duas fotos desenham o mesmo relevo): a casa fica com a folga pequena,
	// só o terreno com a grande
	ShadowDepth.Init(-1e9f, ShadowRes * ShadowRes);
	// -TreadShadowOneMap: o A/B com o mapa único de antes (tudo comparado como terreno, com a folga grande)
	const bool bOneMap = FParse::Param(FCommandLine::Get(), TEXT("TreadShadowOneMap"));
	if (ShowTerrain.Num() > 0 && !bOneMap)
	{
		ShadowCapture->ShowOnlyComponents = ShowTerrain;
		ShadowCapture->CaptureScene();
		if (!ReadShadowRT([this](int32 I, float D) { ShadowDepth[I] = -D; }))
		{
			ShadowDepth.Reset();
			return;
		}
	}
	ShadowCapture->ShowOnlyComponents = Show;
	ShadowCapture->CaptureScene();
	ShadowObjTexels = 0;
	const float SepCm = 20.0f;                     // à frente do terreno por mais que isso: é obstáculo
	if (!ReadShadowRT([this, SepCm, bOneMap](int32 I, float D)
		{
			if (bOneMap)
			{
				ShadowDepth[I] = D < 1e8f ? -D : -1e9f;
			}
			else if (D < 1e8f && D < -ShadowDepth[I] - SepCm)
			{
				ShadowDepth[I] = D;
				++ShadowObjTexels;
			}
		}))
	{
		ShadowDepth.Reset();
		return;
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
	// a orientação: a convenção da câmera ortográfica da Unreal (+1, −1: u pela direita, v pra baixo), provada no FXLab
	// (calibração de 3 a 6 cm). A constante e a folga saem só dos pontos do chão iluminados (os que caem na sombra de algo
	// têm, por definição, a profundidade bem menor). Com o relevo em Nanite do jogo o chão desenhado sai fora da colisão
	// onde os raios batem; a folga cobre isso (16/10). O log traz as 4 orientações (diagnóstico).
	int32 BestHits = -1;
	double BestMad = 0.0;
	for (int32 Vr = 0; Vr < 4; ++Vr)
	{
		const double Su = (Vr & 1) ? -1.0 : 1.0, Sv = (Vr & 2) ? 1.0 : -1.0;   // Vr 0 = (+1, −1), a convenção
		TArray<double> Off;
		for (const FVector& G : Ground)
		{
			const FVector D = G - ShadowO;
			const double Fu = (0.5 + Su * FVector::DotProduct(D, Ax) / ShadowW) * ShadowRes - 0.5;
			const double Fv = (0.5 + Sv * FVector::DotProduct(D, Ay) / ShadowW) * ShadowRes - 0.5;
			const double Dm = TerrainDepthAt(Fu, Fv);   // só onde os 4 pixels são terreno (o raio que bate na casa não conta)
			if (Dm < 1e8)
			{
				Off.Add(Dm - FVector::DotProduct(D, F));
			}
		}
		// os iluminados: o desvio acima de −5 m (mais fundo que isso é sombra de casa, árvore ou morro)
		TArray<double> Lit;
		for (double O : Off)
		{
			if (O > -500.0)
			{
				Lit.Add(O);
			}
		}
		Lit.Sort();
		const double Med = Lit.Num() ? Lit[Lit.Num() / 2] : 0.0;
		TArray<double> Dev;
		for (double O : Lit)
		{
			Dev.Add(FMath::Abs(O - Med));
		}
		Dev.Sort();
		const double Mad = Dev.Num() ? Dev[Dev.Num() / 2] : 0.0;
		const double P75 = Dev.Num() ? Dev[(Dev.Num() * 3) / 4] : 0.0;
		UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: orientação (%+.0f, %+.0f): %d pontos, %d iluminados, mediana %.0f cm, espalhamento %.0f cm, p75 %.0f cm"),
			Su, Sv, Off.Num(), Lit.Num(), Med, Mad, P75);
		if (Vr == 0)
		{
			BestHits = Lit.Num();
			BestMad = P75;
			ShadowSu = Su;
			ShadowSv = Sv;
			ShadowCal = FMath::Abs(Med) < 300.0 ? Med : 0.0;
		}
	}
	ShadowBias = FMath::Clamp(1.5 * BestMad, 30.0, 400.0);
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
	UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: mapa de sombra do cenário com %d peças (%d do terreno), %.0f m de lado (%.0f cm por pixel), calibração %.1f cm, espalhamento %.0f cm (folga do terreno %.0f cm, dos obstáculos %.0f cm, %.1f%% dos pixels), orientação (%+.0f, %+.0f), %d de %d pontos do chão batem, %.0f ms"),
		Show.Num(), ShowTerrain.Num(), ShadowW / 100.0, ShadowW / ShadowRes, ShadowCal, BestMad, ShadowBias, ShadowBiasObj, 100.0 * ShadowObjTexels / FMath::Max(1, ShadowDepth.Num()),
		ShadowSu, ShadowSv, BestHits, Ground.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
	if (BestHits < Ground.Num() / 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: o mapa de sombra não bate com o chão; sem sombra do cenário"));
		ShadowDepth.Reset();
	}
}

bool ARTSTreadDust::ReadShadowRT(TFunctionRef<void(int32, float)> Each)
{
	FTextureRenderTargetResource* Res = ShadowRT ? ShadowRT->GameThread_GetRenderTargetResource() : nullptr;
	const int32 Strip = 512;
	for (int32 Y0 = 0; Y0 < ShadowRes; Y0 += Strip)
	{
		// em faixas: a imagem inteira em FLinearColor seriam 256 MB de memória temporária
		TArray<FLinearColor> Px;
		FReadSurfaceDataFlags Flags(RCM_MinMax);
		if (!Res || !Res->ReadLinearColorPixels(Px, Flags, FIntRect(0, Y0, ShadowRes, Y0 + Strip)) || Px.Num() != ShadowRes * Strip)
		{
			UE_LOG(LogTemp, Warning, TEXT("RTSTreadDust: não li o mapa de sombra do cenário"));
			return false;
		}
		for (int32 I = 0; I < Px.Num(); ++I)
		{
			Each(Y0 * ShadowRes + I, Px[I].R);
		}
	}
	return true;
}

double ARTSTreadDust::MapSunVis(const FVector& Pm) const
{
	if (ShadowDepth.Num() == 0)
	{
		return 1.0;
	}
	// 5 amostras na horizontal (o eixo X da câmera do sol, deitado: −2 a +2 pixels, ~0,6 m pra cada lado), cada uma
	// comparando os 4 pixels vizinhos (pesos bilineares): a borda da sombra na poeira é larga (o raio de vista atravessa
	// metros de nuvem) e o valor não pula quando o cartão anda. Só na horizontal: com o sol baixo o eixo Y da imagem é
	// quase vertical, e a amostra 2 pixels abaixo é um raio ~0,6 m mais baixo, que bate no chão antes do ponto e
	// sombreava o pé da poeira sobre chão plano (a revisão de 16/10). O pixel de obstáculo (positivo) compara com a folga
	// pequena; o de terreno (negativo), com a calibração e a folga dele
	const FVector D = Pm * 100.0 - ShadowO;
	const double Fu = (0.5 + ShadowSu * FVector::DotProduct(D, ShadowX) / ShadowW) * ShadowRes - 0.5;
	const double Fv = (0.5 + ShadowSv * FVector::DotProduct(D, ShadowY) / ShadowW) * ShadowRes - 0.5;
	const double Zp = FVector::DotProduct(D, ShadowF);
	const double ZObj = Zp - ShadowBiasObj, ZTer = Zp + ShadowCal - ShadowBias;
	auto Lit = [&](int32 U, int32 V)
	{
		if (U < 0 || V < 0 || U >= ShadowRes || V >= ShadowRes)
		{
			return 1.0;                               // fora do mapa: sol
		}
		const float S = ShadowDepth[V * ShadowRes + U];
		return S >= 0.0f ? FMath::Clamp((S - ZObj) / 50.0 + 0.5, 0.0, 1.0) : FMath::Clamp((-S - ZTer) / 50.0 + 0.5, 0.0, 1.0);
	};
	double Sum = 0.0;
	const int32 V0 = FMath::FloorToInt32(Fv);
	const double Tv = Fv - V0;
	for (int32 Dx = -2; Dx <= 2; ++Dx)
	{
		const double X = Fu + Dx;
		const int32 U0 = FMath::FloorToInt32(X);
		const double Tu = X - U0;
		Sum += FMath::Lerp(FMath::Lerp(Lit(U0, V0), Lit(U0 + 1, V0), Tu), FMath::Lerp(Lit(U0, V0 + 1), Lit(U0 + 1, V0 + 1), Tu), Tv);
	}
	return Sum / 5.0;
}

double ARTSTreadDust::TerrainDepthAt(double Fu, double Fv) const
{
	// a profundidade do terreno, bilinear, só onde os 4 pixels são terreno visto pelo sol (senão 1e9: não serve pra calibrar)
	const int32 U0 = FMath::FloorToInt32(Fu), V0 = FMath::FloorToInt32(Fv);
	if (U0 < 0 || V0 < 0 || U0 + 1 >= ShadowRes || V0 + 1 >= ShadowRes)
	{
		return 1e9;
	}
	const float* Row0 = &ShadowDepth[V0 * ShadowRes + U0];
	const float* Row1 = Row0 + ShadowRes;
	if (Row0[0] >= 0.0f || Row0[1] >= 0.0f || Row1[0] >= 0.0f || Row1[1] >= 0.0f || Row0[0] < -1e8f || Row0[1] < -1e8f || Row1[0] < -1e8f || Row1[1] < -1e8f)
	{
		return 1e9;
	}
	const double Tu = Fu - U0, Tv = Fv - V0;
	return -FMath::Lerp(FMath::Lerp(double(Row0[0]), double(Row0[1]), Tu), FMath::Lerp(double(Row1[0]), double(Row1[1]), Tu), Tv);
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
	// a cor do chão: só com os shaders prontos (no meio da compilação o terreno sai com o material padrão, cinza: 16/10);
	// no jogo empacotado os shaders já vêm prontos
	bool bShadersReady = ++TickCount > 5;
#if WITH_EDITOR
	bShadersReady = bShadersReady && (!GShaderCompilingManager || GShaderCompilingManager->GetNumRemainingJobs() == 0);
#endif
	if (bGroundDirty && bShadersReady)
	{
		CaptureGroundColor();
	}
	// o modo 2 lê o mapa do sol comum (o RTSSunShadow monta e mantém); os outros, o mapa de antes, desta poeira
	const URTSSunShadowSubsystem* Sun2 = SunShadow();
	const bool bCommonMap = P.ShadowMode >= 2 && Quality >= 1 && bShadowMatOk && Sun2 && !Sun2->HasGivenUp();
	if (!bCommonMap && (bShadowDirty || FVector::DotProduct(SunDir, ShadowSun) < FMath::Cos(FMath::DegreesToRadians(0.5))))
	{
		CaptureMapShadows();
	}
	const double C0 = FPlatformTime::Seconds();
	const double Dt = FMath::Min(double(DeltaSeconds), 0.1);   // um engasgo não joga a poeira longe
	if (bEnabled && !bExternalClock && !bPaused)
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
	if (CardMid)
	{
		UpdateShadowParams(Pcm->GetCameraLocation() / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
		CardMid->SetScalarParameterValue(TEXT("DustClock"), float(Clock));   // o ruído da erosão sobe pelo relógio do plugin (pausa e A/B iguais)
		// a origem do ruído no plano da câmera (cm, módulo o ladrilho de 9,6 m): o material soma à posição relativa à câmera
		const FVector Cp = Pcm->GetCameraLocation();
		CardMid->SetVectorParameterValue(TEXT("NoiseOfs"), FLinearColor(float(FMath::Fmod(FVector::DotProduct(Cp, Rm.GetScaledAxis(EAxis::Y)), 960.0)),
			float(FMath::Fmod(FVector::DotProduct(Cp, Rm.GetScaledAxis(EAxis::Z)), 960.0)), 0.0f, 0.0f));
	}
	// a poeira em volume (rts.TreadDust.Path 1): os cartões somem e o raymarch das explosões desenha os puffs
	const bool bVol = IsVolumePath();
	if (bVol != bVolumeOn)
	{
		bVolumeOn = bVol;
		CardMesh->SetVisibility(bCardsVisible && !bVol);
		ShadowMesh->SetVisibility(bCardsVisible && !bVol);
		UE_LOG(LogTemp, Display, TEXT("RTSTreadDust: a poeira %s"), bVol ? TEXT("em VOLUME (o raymarch das explosões, RTSDustField)") : TEXT("nos cartões 6-way"));
		if (!bVol)
		{
			RTSDustField::SubmitFrame(FVector::ZeroVector, SunDir, Clock, 0.0f, TArray<FRTSDustPuff>());
		}
	}
	if (bVol)
	{
		SubmitVolume(Pcm->GetCameraLocation() / 100.0, Rm.GetScaledAxis(EAxis::X));
	}
	else
	{
		Draw(Pcm->GetCameraLocation() / 100.0, Rm.GetScaledAxis(EAxis::Y), Rm.GetScaledAxis(EAxis::Z), Rm.GetScaledAxis(EAxis::X));
	}
	LastMs = StepMs + (FPlatformTime::Seconds() - C0) * 1000.0;
	StepMs = 0.0;
}
