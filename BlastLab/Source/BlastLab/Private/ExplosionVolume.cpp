#include "ExplosionVolume.h"

#include "BattleSmokeSim.h"
#include "BlastLabSimulation.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Texture.h"
#include "ExplosionBake.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

namespace
{
	const FName ParamFrame(TEXT("SparseVolumeTexture"));
	const FName ParamNextFrame(TEXT("SparseVolumeTextureNext"));
	const FName ParamFrameAlpha(TEXT("FrameAlpha"));
	const FName ParamFrameInterval(TEXT("FrameInterval"));
	const FName ParamBoxMin(TEXT("BoxMin"));
	const FName ParamBoxSize(TEXT("BoxSize"));
	const FName ParamTime(TEXT("Time"));
	const FName ParamSeed(TEXT("Seed"));
	const FName ParamDensity(TEXT("DensityScale"));
	const FName ParamEmission(TEXT("EmissionScale"));
	const FName ParamWarp(TEXT("Warp"));

	// Unidade de iluminância do WebGL (js/celestial.js SUN_LUX): cor 1 = 128 000.
	constexpr float LuxPerUnit = 128000.0f;
	// Clarão da detonação (js/main.js env.flashGain/flashTau; js/instances.js
	// lights(): flashGain·s²·exp(−t/flashTau), cor 1 : 0.86 : 0.64).
	constexpr float FlashGain = 180.0f;
	constexpr float FlashTau = 0.035f;

	// Duas luzes, como no WebGL, que usava uma queda pro cenário e outra dentro
	// do próprio volume. Uma luz só, a 1/d², iluminava a fumaça colada nela 15×
	// mais que o WebGL e a bola de fogo estourava pra branco (a luz do fogo É a
	// emissão do volume; contá-la de novo de perto é dupla contagem).
	//  - cenário: 50·cor/d² ao longe (scene.js: cor/(s² + 0.02·d²)); não toca o
	//    volume (Volumetric Scattering Intensity 0 — o Heterogeneous Volume
	//    multiplica a luz por ele, HeterogeneousVolumesLightingUtils.ush);
	//  - volume: 20·cor/d² ao longe (instances.js: cor/(1 + 0.05·d²)), canal 1,
	//    que só as explosões escutam; raio de fonte √20 m pra saturar perto do
	//    centro como lá.
	constexpr float SceneLightFactor = 50.0f, SceneLightRadiusCm = 400.0f;
	constexpr float VolumeLightFactor = 20.0f, VolumeLightRadiusCm = 447.0f;
	// teto da sombra da luz do fogo no volume (instances.js tauCap 2.2): essa
	// fração vai sem sombra pelo material (FireFill), o resto pela luz com sombra
	const float FireFillFraction = FMath::Exp(-2.2f);

	// Importância (UpdateSignificance). Níveis de detalhe pela fração da meia
	// altura da tela que a fumaça ocupa, com 15% de histerese: passo do volume,
	// passo da sombra e resolução da iluminação dele (padrões da Unreal: 1, 2, 2).
	struct FDetail { float Step, ShadowStep, LightingDownsample, MinFraction; };
	const FDetail Details[] = { { 1.0f, 2.0f, 2.0f, 0.30f }, { 1.5f, 3.0f, 2.0f, 0.10f }, { 2.0f, 4.0f, 4.0f, 0.0f } };
	constexpr float Hysteresis = 0.15f;
	// a luz do cenário termina onde ficaria abaixo dessa fração do ambiente
	constexpr float LightCutoff = 0.01f;
	constexpr float MinSceneReachCm = 2000.0f;
	// a sombra dela: o chão a até 40 m da explosão, se ela ocupa ao menos 10% da meia tela
	constexpr float ShadowRegionCm = 4000.0f;
	constexpr float MinShadowFraction = 0.10f;

	// medição: -ExplosionEmission=<k> e -ExplosionVolumeLight=<k> multiplicam a
	// emissão e a luz de dentro do volume (separar quem faz o brilho da bola de fogo)
	float CommandLineScale(const TCHAR* Key)
	{
		float Value = 1.0f;
		FParse::Value(FCommandLine::Get(), Key, Value);
		return Value;
	}

	FLightingChannels Channels(bool C0, bool C1)
	{
		FLightingChannels C;
		C.bChannel0 = C0;
		C.bChannel1 = C1;
		C.bChannel2 = false;
		return C;
	}

	void SetupLight(UPointLightComponent* L, bool bVolume)
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.0f);
		L->SetAttenuationRadius(30000.0f);
		// As duas com sombra. A de dentro precisa: o volume só sombreia a luz
		// pela própria fumaça se ela fizer sombra (o mapa de sombra volumétrico,
		// HeterogeneousVolumesLiveShadingPipeline.cpp bCreateShadowMap). Sem isso
		// a fuligem do miolo era iluminada por inteiro, a bola de fogo virava
		// uma bola creme 2× mais clara que o WebGL (lá fireT, o cache de luz, faz
		// esse papel) e estourava à noite. Ela só alcança o canal 1, então o mapa
		// de sombra dela só tem os volumes.
		L->SetCastShadows(true);
		L->SetCastVolumetricShadow(true);
		L->SetVolumetricScatteringIntensity(bVolume ? 1.0f : 0.0f);
		L->LightingChannels = Channels(!bVolume, bVolume);
	}
}

AExplosionVolume::AExplosionVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // depois do Sequencer, que anima Age nas sequências de teste

	Foot = CreateDefaultSubobject<USceneComponent>(TEXT("Foot"));
	RootComponent = Foot;

	Volume = CreateDefaultSubobject<UHeterogeneousVolumeComponent>(TEXT("HeterogeneousVolumeComponent"));
	Volume->SetupAttachment(Foot);
	// o tick do volume reescreveria o quadro e a caixa inteira a cada quadro
	Volume->PrimaryComponentTick.bStartWithTickEnabled = false;
	Volume->bIssueBlockingRequests = true;               // nada de quadro faltando (render e simulador)
	// as explosões escutam só o canal 1: sol e lua estão no 0 e no 1, a luz do fogo
	// de dentro no 1, a do cenário só no 0. O cache de luz do volume faz uma
	// passada por luz que alcança o canal dele (HeterogeneousVolumes.cpp
	// GatherLights não olha a intensidade volumétrica): sem isso a luz de cenário
	// de cada explosão custava uma passada em todas as outras.
	Volume->LightingChannels = Channels(false, true);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Foot);
	SetupLight(FireLight, false);
	FireLightVolume = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLightVolume"));
	FireLightVolume->SetupAttachment(Foot);
	SetupLight(FireLightVolume, true);
}

void AExplosionVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorScale3D(FVector(SizeScale));
	if (Bake)
	{
		// caixa local [0, resolução] voxels, com o pé no centro da base
		Volume->SetRelativeLocation(FVector(-0.5f * Bake->DomainCm, -0.5f * Bake->DomainCm, 0.0f));
	}
	UpdateVolume(BakeTime());
	UpdateLights(BakeTime());
}

bool AExplosionVolume::IsFinished() const
{
	return Bake && BakeTime() > Bake->Duration();
}

UMaterialInstanceDynamic* AExplosionVolume::VolumeMaterial() const
{
	// a instância dinâmica que o volume criou vem depois do material base
	// (UHeterogeneousVolumeComponent::GetUsedMaterials)
	TArray<UMaterialInterface*> Materials;
	Volume->GetUsedMaterials(Materials);
	UMaterialInstanceDynamic* MID = nullptr;
	for (UMaterialInterface* M : Materials)
	{
		MID = MID ? MID : Cast<UMaterialInstanceDynamic>(M);
	}
	return MID;
}

void AExplosionVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bSelfAdvance)
	{
		const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
		Age += Sim ? Sim->SimDelta(DeltaSeconds) : DeltaSeconds;
	}
	if (!FMath::IsNearlyEqual(GetActorScale3D().X, SizeScale))
	{
		SetActorScale3D(FVector(SizeScale));
	}
	UpdateSignificance(BakeTime());
	UpdateVolume(BakeTime());
	UpdateLights(BakeTime());
}

FBox AExplosionVolume::SmokeBoxWorld() const
{
	const FVector Base = GetActorLocation();
	if (!Bake || Bake->Resolution <= 0)
	{
		return FBox(Base, Base);
	}
	int32 F0, F1;
	float Alpha;
	Bake->FramesAt(BakeTime(), F0, F1, Alpha);
	const FVector Corner(-0.5f * Bake->DomainCm, -0.5f * Bake->DomainCm, 0.0f);
	FVector Min = Corner, Max = Corner + FVector(Bake->DomainCm);
	if (Bake->BoxMin.IsValidIndex(F0) && Bake->BoxSize.IsValidIndex(F0))
	{
		const float Vox = Bake->DomainCm / Bake->Resolution;
		Min = Corner + FVector(Bake->BoxMin[F0]) * Vox;
		Max = Min + FVector(Bake->BoxSize[F0]) * Vox;
	}
	return FBox(Base + Min * SizeScale, Base + Max * SizeScale);
}

void AExplosionVolume::SetWarp(UTexture* Warp)
{
	if (WarpTexture != Warp)
	{
		WarpTexture = Warp;
		bWarpDirty = true;
	}
}

float AExplosionVolume::SootHeightWorld(float AheadS) const
{
	const float Base = GetActorLocation().Z;
	if (!Bake || !Bake->SootHeight.Num())
	{
		return Base + 0.5f * (Bake ? Bake->DomainCm : 3800.0f) * SizeScale;
	}
	int32 F0, F1;
	float Alpha;
	Bake->FramesAt(FMath::Min((Age + AheadS) / FMath::Max(SizeScale, 0.01f), Bake->Duration()), F0, F1, Alpha);
	const float H0 = Bake->SootHeight.IsValidIndex(F0) ? Bake->SootHeight[F0] : 0.0f;
	const float H1 = Bake->SootHeight.IsValidIndex(F1) ? Bake->SootHeight[F1] : H0;
	return Base + FMath::Lerp(H0, H1, Alpha) * SizeScale;
}

void AExplosionVolume::UpdateSignificance(float T)
{
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	if (!Sim || !Bake || Opacity <= 0.0f)
	{
		bOnScreen = bShadowRegionOnScreen = true;      // o aquecimento de shaders desenha sempre
		ScreenFrac = 1.0f;
		Lod = NaturalLod = 0;
		return;
	}
	// a esfera da fumaça neste quadro: a caixa gravada, no mundo
	int32 F0, F1;
	float Alpha;
	Bake->FramesAt(T, F0, F1, Alpha);
	FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, 0.5f * Bake->DomainCm * SizeScale);
	float Radius = 0.87f * Bake->DomainCm * SizeScale;
	if (Bake->BoxMin.IsValidIndex(F0) && Bake->BoxSize.IsValidIndex(F0) && Bake->Resolution > 0)
	{
		const float Vox = Bake->DomainCm / Bake->Resolution;                // cm por voxel, antes da escala do ator
		const FVector Min = FVector(Bake->BoxMin[F0]) * Vox, Size = FVector(Bake->BoxSize[F0]) * Vox;
		const FVector Corner(-0.5f * Bake->DomainCm, -0.5f * Bake->DomainCm, 0.0f);
		Center = GetActorTransform().TransformPosition(Corner + Min + 0.5f * Size);
		Radius = 0.5f * Size.Size() * SizeScale;
	}
	bool bIn = true;
	ScreenFrac = Sim->ScreenFraction(Center, Radius * 1.1f, bIn);       // 10% de margem: nada estala na borda
	bOnScreen = bIn;
	// a sombra da luz do cenário só se vê no chão perto da explosão (os riscos dos escombros)
	Sim->ScreenFraction(GetActorLocation(), ShadowRegionCm * SizeScale, bIn);
	bShadowRegionOnScreen = bIn;

	// nível de detalhe com histerese: subir pede a fração (1 + h)× acima do limiar, descer (1 − h)× abaixo
	auto LevelFor = [](float F) { return F >= Details[0].MinFraction ? 0 : (F >= Details[1].MinFraction ? 1 : 2); };
	const int32 Up = LevelFor(ScreenFrac / (1.0f + Hysteresis));
	const int32 Down = LevelFor(ScreenFrac / (1.0f - Hysteresis));
	NaturalLod = Up < NaturalLod ? Up : (Down > NaturalLod ? Down : NaturalLod);
	Lod = FMath::Clamp(FMath::Max(NaturalLod, LodFloor), 0, 2);
	const FDetail& D = Details[Lod];
	if (Volume->StepFactor != D.Step || Volume->ShadowStepFactor != D.ShadowStep || Volume->LightingDownsampleFactor != D.LightingDownsample)
	{
		Volume->StepFactor = D.Step;
		Volume->ShadowStepFactor = D.ShadowStep;
		Volume->LightingDownsampleFactor = D.LightingDownsample;
		Volume->MarkRenderStateDirty();
	}
}

void AExplosionVolume::UpdateVolume(float T)
{
	UMaterialInstanceDynamic* MID = VolumeMaterial();
	if (!Bake || !Bake->Volume || !MID)
	{
		return;
	}
	const bool bAlive = Age >= 0.0f && !IsFinished();
	Volume->SetVisibility(bAlive && bOnScreen);
	if (!bAlive)
	{
		return;
	}
	if (bWarpDirty)
	{
		MID->SetTextureParameterValue(ParamWarp, WarpTexture.Get());
		bWarpDirty = false;
	}

	int32 F0, F1;
	float Alpha;
	Bake->FramesAt(T, F0, F1, Alpha);
	const uint32 Key = GetTypeHash(this);
	// sempre a resolução inteira (mip 0): pedir a metade (mip 1) pras distantes
	// travava o jogo 2–3.5 s esperando o disco (dado nunca carregado; medido 05/10)
	const float Mip = 0.0f;
	if (!bOnScreen)
	{
		// fora da tela: nada de material nem caixa, só os quadros seguem chegando
		// (sem travar), pra ela voltar inteira quando a câmera virar
		USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Bake->Volume, Key, 0.0f, float(F0), Mip, false, false);
		USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Bake->Volume, Key + 1, 0.0f, float(F1), Mip, false, false);
		return;
	}
	USparseVolumeTextureFrame* Lower = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Bake->Volume, Key, 0.0f, float(F0), Mip, Volume->bIssueBlockingRequests != 0, false);
	USparseVolumeTextureFrame* Upper = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(Bake->Volume, Key + 1, 0.0f, float(F1), Mip, Volume->bIssueBlockingRequests != 0, false);
	if (!Lower || !Upper)
	{
		return;
	}
	MID->SetSparseVolumeTextureParameterValue(ParamFrame, Lower);
	MID->SetSparseVolumeTextureParameterValue(ParamNextFrame, Upper);
	MID->SetScalarParameterValue(ParamFrameAlpha, Alpha);
	MID->SetScalarParameterValue(ParamFrameInterval, Bake->FrameTimes[F1] - Bake->FrameTimes[F0]);
	MID->SetScalarParameterValue(ParamTime, T);
	MID->SetScalarParameterValue(ParamSeed, Seed);
	// o volume marcha em voxels LOCAIS: com o ator escalado, a profundidade
	// óptica e o brilho acumulado precisam crescer com o tamanho (no WebGL o
	// passo era em metros do mundo e isso saía de graça)
	MID->SetScalarParameterValue(ParamDensity, SizeScale * Opacity);
	static const float EmissionTest = CommandLineScale(TEXT("-ExplosionEmission="));
	MID->SetScalarParameterValue(ParamEmission, SizeScale * Opacity * EmissionTest);

	// caixa marchada justa: mesma escala do quadro gravado (1 voxel = domínio/resolução), deslocada pro canto
	if (!Bake->BoxMin.IsValidIndex(F0) || !Bake->BoxSize.IsValidIndex(F0))
	{
		return;
	}
	FIntVector Min = Bake->BoxMin[F0];
	FIntVector Size = Bake->BoxSize[F0];
	if (WarpTexture)
	{
		// deformada: a fumaça empurrada pode sair da caixa gravada (até o |D|
		// máximo); a caixa cresce isso, dentro do domínio
		const float VoxLocalM = Bake->DomainCm / 100.0f / FMath::Max(Bake->Resolution, 1);
		const int32 Pad = FMath::CeilToInt32(BattleSmoke::WarpMaxLocalM / FMath::Max(VoxLocalM, 1e-3f));
		const FIntVector Lo(FMath::Max(Min.X - Pad, 0), FMath::Max(Min.Y - Pad, 0), FMath::Max(Min.Z - Pad, 0));
		const FIntVector Hi(FMath::Min(Min.X + Size.X + Pad, Bake->Resolution), FMath::Min(Min.Y + Size.Y + Pad, Bake->Resolution),
			FMath::Min(Min.Z + Size.Z + Pad, Bake->Resolution));
		Min = Lo;
		Size = Hi - Lo;
	}
	const FTransform Base = Lower->GetFrameTransform();
	FTransform Box = Base;
	Box.SetTranslation(Base.GetTranslation() + Base.GetScale3D() * FVector(Min));
	MID->SetVectorParameterValue(ParamBoxMin, FLinearColor(Min.X, Min.Y, Min.Z, 0.0f));
	MID->SetVectorParameterValue(ParamBoxSize, FLinearColor(Size.X, Size.Y, Size.Z, 0.0f));
	if (Volume->VolumeResolution != Size || !Volume->FrameTransform.Equals(Box))
	{
		Volume->VolumeResolution = Size;
		Volume->FrameTransform = Box;
		Volume->UpdateBounds();
		Volume->MarkRenderStateDirty();     // o proxy guarda a resolução e a transformação do quadro
	}
}

void AExplosionVolume::UpdateLights(float T)
{
	const bool bOn = Bake && Bake->LightColor.Num() && Age >= 0.0f && !IsFinished() && Opacity > 0.0f;
	FireLight->SetVisibility(bOn);
	FireLightVolume->SetVisibility(bOn);
	if (!bOn)
	{
		return;
	}
	int32 F0, F1;
	float Alpha;
	Bake->FramesAt(T, F0, F1, Alpha);
	const FVector Position = FMath::Lerp(Bake->LightPosition[F0], Bake->LightPosition[F1], Alpha);
	FLinearColor Color = FMath::Lerp(Bake->LightColor[F0], Bake->LightColor[F1], Alpha);
	if (T < 8.0f * FlashTau)
	{
		const float K = FlashGain * FMath::Exp(-T / FlashTau);
		Color += FLinearColor(K, 0.86f * K, 0.64f * K, 0.0f);
	}
	const float Peak = FMath::Max3(Color.R, Color.G, Color.B);
	const FLinearColor Tint = Peak > 1e-6f ? FLinearColor(Color.R / Peak, Color.G / Peak, Color.B / Peak) : FLinearColor::Black;
	const float Area = SizeScale * SizeScale;      // a luz vai com a área (W^(2/3))
	static const float VolumeLightTest = CommandLineScale(TEXT("-ExplosionVolumeLight="));
	const float VolumeCandela = VolumeLightFactor * VolumeLightTest * LuxPerUnit * Area;
	UMaterialInstanceDynamic* MID = bOnScreen ? VolumeMaterial() : nullptr;
	if (MID)
	{
		// a parte sem sombra, no material: posição no canto do domínio (m) e intensidade em cd
		const FVector Corner(-0.5f * Bake->DomainCm, -0.5f * Bake->DomainCm, 0.0f);
		const FVector FireLocalM = (Position - Corner) * 0.01f;
		MID->SetVectorParameterValue(TEXT("FireLocalM"), FLinearColor(FireLocalM.X, FireLocalM.Y, FireLocalM.Z, 0.0f));
		MID->SetVectorParameterValue(TEXT("FireFill"), Color * (VolumeCandela * FireFillFraction));
		MID->SetScalarParameterValue(TEXT("SizeScale"), SizeScale);
	}
	const float SceneCandela = Peak * SceneLightFactor * LuxPerUnit * Area;
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	for (UPointLightComponent* L : { FireLight.Get(), FireLightVolume.Get() })
	{
		const bool bVolume = L == FireLightVolume;
		L->SetRelativeLocation(Position);             // a escala do ator leva junto
		L->SetLightColor(Tint);                        // cor linear (bSRGB = true: guardada em sRGB, devolvida linear)
		L->SetIntensity(bVolume ? Peak * VolumeCandela * (1.0f - FireFillFraction) : SceneCandela);
		// raio da fonte e alcance só quando mudam: cada troca recria a luz no render
		const float Source = (bVolume ? VolumeLightRadiusCm : SceneLightRadiusCm) * SizeScale;
		if (!FMath::IsNearlyEqual(L->SourceRadius, Source, 1.0f))
		{
			L->SetSourceRadius(Source);
		}
		// A de dentro só alcança a própria caixa (e quem estiver colado nela). A
		// do cenário termina onde ficaria abaixo de 1% do ambiente da hora: de
		// dia, perto; à noite, longe (o clarão ilumina o campo, como deve).
		// Antes eram 300 m sempre: cada explosão, até atrás da câmera, era uma
		// passada de luz com sombra na tela inteira (medido 05/10: 7 atrás da
		// câmera custavam ~12 ms).
		float Reach = Bake->DomainCm * SizeScale;
		if (!bVolume)
		{
			const float Ambient = FMath::Max(Sim ? Sim->AmbientLux() : 52000.0f, 1.0f);
			Reach = FMath::Clamp(FMath::Sqrt(SceneCandela / (LightCutoff * Ambient)) * 100.0f,
				MinSceneReachCm * SizeScale, 30000.0f * SizeScale);
		}
		if (FMath::Abs(L->AttenuationRadius - Reach) > 0.1f * Reach)
		{
			L->SetAttenuationRadius(Reach);
		}
		// Sombra só quando se vê: a do cenário, se o chão perto da explosão está
		// na tela e ela não é um ponto; a de dentro, se o volume está na tela.
		const bool bShadow = bVolume ? bOnScreen : (bShadowRegionOnScreen && ScreenFrac > MinShadowFraction);
		if (L->CastShadows != bShadow)
		{
			L->SetCastShadows(bShadow);
		}
	}
}
