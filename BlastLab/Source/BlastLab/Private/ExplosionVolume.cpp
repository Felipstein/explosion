#include "ExplosionVolume.h"

#include "BlastRaymarchSubsystem.h"

#include "BattleSmokeSim.h"
#include "BlastLabEnvironment.h"
#include "BlastLabSimulation.h"
#include "Components/HeterogeneousVolumeComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Texture.h"
#include "ExplosionBake.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Scalability.h"
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
	const FName ParamBakeNormal(TEXT("BakeNormal"));
	const FName ParamVelFrame0(TEXT("VelFrame0"));
	const FName ParamVelFrame1(TEXT("VelFrame1"));
	const FName ParamVelFrames(TEXT("VelFrames"));
	const FName ParamVelRes(TEXT("VelRes"));
	const FName ParamSkirtRadius(TEXT("SkirtRadius"));
	const FName ParamSkirtHeight(TEXT("SkirtHeight"));
	const FName ParamSkirtDust(TEXT("SkirtDust"));
	const FName ParamSkirtTint(TEXT("SkirtTint"));

	// Unidade de iluminância do WebGL (js/celestial.js SUN_LUX): cor 1 = 128 000.
	constexpr float LuxPerUnit = 128000.0f;
	// Clarão da detonação (js/main.js env.flashGain/flashTau; js/instances.js
	// lights(): flashGain·s²·exp(−t/flashTau), cor 1 : 0.86 : 0.64).
	constexpr float FlashGain = 180.0f;
	// 35 ms: com 60 ms (10/10) a luz sem sombra ficava à vista atravessando os objetos (o Felipe achou);
	// mais longa só quando ela fizer sombra (a etapa 2 do clarão)
	constexpr float FlashTau = 0.035f;
	// O clarão tem luz própria (FlashLight) que some em ~0.15 s (§4d A4: "o
	// chão volta ao normal até 0.15 s"); o alcance vai até 1% do ambiente.
	// A estrela de raios: 0–0.13 s, ~30 m de lado na Carga Pesada (2–3× a bola
	// do vídeo do Veo), miolo a ~20× o branco da exposição do dia (estoura).
	const TCHAR* StarPath = TEXT("/Game/Materials/M_FlashStar.M_FlashStar");
	// (30 m / τ 45 ms / 4e4 só aparecia no 1º quadro da câmera baixa; 45 m / 2e5
	// cobria a tela inteira em branco; o vídeo tem a estrela de ~2–3× a bola)
	constexpr float StarSizeCm = 2400.0f, StarEnd = 0.16f, StarTau = 0.09f, StarLum = 6.0e4f;   // τ 60 ms: sumia aos 0.10 s
	// A luz gravada no WebGL saiu com o brilho comprimido pelo expoente 0.44 (o
	// do material de lá): relativa ao pico, ela caía só 3× até 0.9 s, e o chão
	// ficava laranja ~1.5 s. Desfeita a compressão (o material usa a curva
	// física desde 07/10), cai ~14× até 0.9 s e ~60× até 1.2 s, perto da física
	// (bola de 2000 K que cai ~100× até 1 s, docs/CONCEITO_EXPLOSOES pesquisa 02)
	constexpr float LightCurveK = 1.0f / 0.44f;

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
	// Cenário: 4 (era 50, o do WebGL). Com 50 o pico da bola (0.14–0.28 s) dava
	// ~5e8 cd e o chão estourava em creme por ~0.5 s; uma bola de ~10 m a 2000 K
	// dá π·R²·L_v = π·25·465 000 ≈ 4e7 cd (docs/CONCEITO_EXPLOSOES pesquisa 02;
	// medido 07/10, captures/ue/_star.jpg). O clarão fica 2–5× o pico da bola.
	constexpr float SceneLightFactor = 4.0f, SceneLightRadiusCm = 400.0f;
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
	// Abaixo do ultra (07/10: a morte de tanque rodava a 11 qps na 1660, os
	// volumes custavam 111 ms de placa). As simulações novas têm voxel de ~9 cm
	// (192³ em 17 m) contra 40 cm da Carga Pesada, e o cache de luz de cada
	// volume custava 10–15 ms por luz. Então: a célula do cache tem ao menos
	// MinLightCellCm (ainda mais fina que os 80 cm da Carga Pesada aprovada) e a
	// luz do fogo de dentro vai só pelo material (FireFill), sem a luz com sombra,
	// que era um cache inteiro em cada volume que ela alcançava. O ultra fica
	// como era: o visual máximo, mesmo travando.
	constexpr float MinLightCellCm = 45.0f;
	// o passo mínimo da marcha (cm no mundo): o voxel da bola inteira da morte de tanque
	constexpr float MinStepCm = 9.0f;
	bool FullDetail() { return Scalability::GetQualityLevels().EffectsQuality >= 4; }
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

	// medição: -ExplosionParams=Nome=valor;Nome=valor sobrepõe parâmetros escalares
	// do M_Explosion (A/B do detalhe e da cor com a mesma história, sem recompilar)
	TArray<TPair<FName, float>> CommandLineParams()
	{
		TArray<TPair<FName, float>> Out;
		FString S;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ExplosionParams="), S, false))
		{
			TArray<FString> Items;
			S.ParseIntoArray(Items, TEXT(";"));
			for (const FString& Item : Items)
			{
				FString Key, Value;
				if (Item.Split(TEXT("="), &Key, &Value))
				{
					Out.Emplace(FName(*Key), FCString::Atof(*Value));
				}
			}
		}
		return Out;
	}

	// A saia de poeira rente ao chão (a "terra indo pros lados" da arte alvo;
	// M_Explosion, build_explosion.py SKIRT). Em metros LOCAIS da Carga Pesada
	// (escala junto com a explosão): a frente sai rápida e freia,
	// R = Rmax·(1 − e^−t/τR); a altura cresce mais devagar; a poeira assenta
	// depois de ~1.5 s e some antes da gravação acabar (sem estalo no fim).
	// Rmax = 15 m: o anel denso de 14–27 m dos 20 kg (docs/EXPLOSOES.md, Chão).
	// Medição: -Skirt=Rmax,τR,Hmax,τH,poeira,início do assentamento,τ
	struct FSkirt { float RadiusM = 0.0f, HeightM = 0.0f, Dust = 0.0f; };

	FSkirt SkirtAt(float T, float Duration, float HeightMul, float LifeMul)
	{
		static const TArray<float> P = []
		{
			// poeira 0.35 e assentando de 0.8 s com τ 1.5 s: com 0.7, ou assentando
			// de 1–1.5 s, aos 2.5 s ela cobria o chão inteiro (esconderia as unidades)
			TArray<float> V = { 15.0f, 0.45f, 5.0f, 0.8f, 0.35f, 0.8f, 1.5f };
			FString S;
			if (FParse::Value(FCommandLine::Get(), TEXT("-Skirt="), S, false))
			{
				TArray<FString> Items;
				S.ParseIntoArray(Items, TEXT(","));
				for (int32 i = 0; i < FMath::Min(Items.Num(), V.Num()); ++i)
				{
					V[i] = FCString::Atof(*Items[i]);
				}
			}
			return V;
		}();
		FSkirt Out;
		if (T <= 0.0f || HeightMul <= 0.0f)
		{
			return Out;
		}
		Out.RadiusM = P[0] * (1.0f - FMath::Exp(-T / P[1]));
		Out.HeightM = HeightMul * P[2] * (1.0f - FMath::Exp(-T / P[3]));
		Out.Dust = P[4] * FMath::SmoothStep(0.0f, 0.08f, T) * FMath::Exp(-FMath::Max(T - LifeMul * P[5], 0.0f) / (LifeMul * P[6]))
			* (1.0f - FMath::SmoothStep(Duration - 1.5f, Duration, T));
		return Out;
	}

	// Pedido bloqueante (a CPU espera o quadro do volume chegar inteiro) só nas
	// gravações que precisam de todo quadro completo e igual: o Movie Render Queue
	// e o teste A/B de passo fixo (-benchmark). Ao vivo, pedidos assíncronos, que
	// buscam os próximos quadros adiantado (o bloqueante não busca:
	// SparseVolumeTextureStreamingManager.cpp, Request). Bloqueando sempre, o
	// streaming gastava 17–34 ms de CPU por quadro na thread de render durante a
	// morte de tanque (medido 07/10) e era o que travava o jogo.
	bool BlockingStreaming()
	{
		static const bool bBlocking = FParse::Param(FCommandLine::Get(), TEXT("benchmark"))
			|| FCString::Strifind(FCommandLine::Get(), TEXT("MoviePipelineConfig=")) != nullptr;
		return bBlocking;
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
		// A sombra volumétrica (a fumaça bloqueando a luz) só na de dentro. A do
		// cenário fica no centro da bola: com a fuligem opaca (07/10) o próprio
		// volume a apagava e o chão a 10–14 m acendia só 1.4× (medido; a física,
		// 4e7 cd a ~13 m, pede ~4.5×). A bola é a superfície que brilha, não uma
		// lâmpada atrás dela.
		L->SetCastVolumetricShadow(bVolume);
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
	Volume->bIssueBlockingRequests = BlockingStreaming();
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

	// o clarão: só o cenário (canal 0), sem sombra, sem Lumen e sem névoa
	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(Foot);
	FlashLight->SetMobility(EComponentMobility::Movable);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(0.0f);
	FlashLight->SetCastShadows(false);
	FlashLight->SetIndirectLightingIntensity(0.0f);
	FlashLight->SetVolumetricScatteringIntensity(0.0f);
	FlashLight->LightingChannels = Channels(true, false);
	FlashLight->SetLightColor(FLinearColor(1.0f, 0.86f, 0.64f));
	FlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));
	FlashLight->SetVisibility(false);

	FlashStar = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("FlashStar"));
	FlashStar->SetupAttachment(Foot);
	FlashStar->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));
	FlashStar->SetCastShadow(false);
	FlashStar->SetVisibility(false);
}

void AExplosionVolume::BeginPlay()
{
	Super::BeginPlay();
	// a estrela de lente saiu (07/10, segunda opinião da sessão de arte): grudada
	// na tela, ela não é do mundo; no alvo os "raios" são faíscas de verdade e o
	// que acende é o chão e os vizinhos. -FlashStar traz de volta pra comparar.
	static const bool bStar = FParse::Param(FCommandLine::Get(), TEXT("FlashStar"));
	UMaterialInterface* Star = bStar ? LoadObject<UMaterialInterface>(nullptr, StarPath) : nullptr;
	if (Star)
	{
		StarMaterial = UMaterialInstanceDynamic::Create(Star, this);
		StarMaterial->SetScalarParameterValue(TEXT("Seed"), FMath::FRand());
		FlashStar->AddElement(StarMaterial, nullptr, false, StarSizeCm, StarSizeCm, nullptr);
	}
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
	if (LoopPeriod > 0.0f)
	{
		return Age > LoopLife;
	}
	return Bake && BakeTime() > Bake->Duration();
}

void AExplosionVolume::FramesAt(float T, int32& F0, int32& F1, float& Alpha) const
{
	const int32 N = Bake ? Bake->FrameTimes.Num() : 0;
	if (LoopPeriod > 0.0f && N > 1 && T > Bake->FrameTimes.Last())
	{
		// a emenda: do último quadro de volta pro primeiro
		F0 = N - 1;
		F1 = 0;
		Alpha = FMath::Clamp((T - Bake->FrameTimes.Last()) / FMath::Max(LoopPeriod - Bake->FrameTimes.Last(), 1e-3f), 0.0f, 1.0f);
		return;
	}
	Bake->FramesAt(T, F0, F1, Alpha);
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
	const FVector WantScale(SizeScale, SizeScale, SizeScale * HeightScale);
	if (!GetActorScale3D().Equals(WantScale, 1e-4))
	{
		SetActorScale3D(WantScale);
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
	FramesAt(BakeTime(), F0, F1, Alpha);
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
	FramesAt(FMath::Min((Age + AheadS) / FMath::Max(TimeScale > 0.0f ? TimeScale : SizeScale, 0.01f), Bake->Duration()), F0, F1, Alpha);
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
	FramesAt(T, F0, F1, Alpha);
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
	// medição: -ExplosionLod=<0..2> fixa o nível e -ExplosionLightDS=<f> a resolução da luz
	static const int32 ForcedLod = [] { int32 L = -1; FParse::Value(FCommandLine::Get(), TEXT("-ExplosionLod="), L); return L; }();
	static const float ForcedLightDS = [] { float V = -1.0f; FParse::Value(FCommandLine::Get(), TEXT("-ExplosionLightDS="), V); return V; }();
	if (ForcedLod >= 0)
	{
		Lod = FMath::Clamp(ForcedLod, 0, 2);
	}
	FDetail D = Details[Lod];
	if (!FullDetail() && Bake->Resolution > 0)
	{
		// em meios: a escala da carcaça muda a cada quadro e cada troca recria o volume no render
		const float VoxelCm = Bake->DomainCm * SizeScale / Bake->Resolution;
		D.LightingDownsample = FMath::Max(D.LightingDownsample, FMath::CeilToFloat(2.0f * MinLightCellCm / VoxelCm) * 0.5f);
		// e o passo da marcha em cm: uma gravação tocada pequena (as bolas do fogo
		// da carcaça, 0.45 da escala) marchava em passos de ~4 cm
		const float Floor = FMath::CeilToFloat(4.0f * MinStepCm / VoxelCm) * 0.25f;
		if (D.Step < Floor)
		{
			D.ShadowStep *= Floor / D.Step;
			D.Step = Floor;
		}
	}
	if (ForcedLightDS > 0.0f)
	{
		D.LightingDownsample = ForcedLightDS;
	}
	if (Volume->StepFactor != D.Step || Volume->ShadowStepFactor != D.ShadowStep || Volume->LightingDownsampleFactor != D.LightingDownsample)
	{
		Volume->StepFactor = D.Step;
		Volume->ShadowStepFactor = D.ShadowStep;
		Volume->LightingDownsampleFactor = D.LightingDownsample;
		Volume->MarkRenderStateDirty();
	}
	static bool bLodLogged = false;
	if (!bLodLogged && Age > 0.3f && bOnScreen)
	{
		bLodLogged = true;
		UE_LOG(LogTemp, Display, TEXT("BlastLab: explosão a %.0f%% da meia tela, nível %d (passo %.1f, luz /%.1f)"), ScreenFrac * 100.0f, Lod, D.Step, D.LightingDownsample);
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
	// o raymarch portado do WebGL desenha esta (UBlastRaymarchSubsystem, -Raymarch=1)
	const bool bRaymarch = UBlastRaymarchSubsystem::Handles(this);
	Volume->SetVisibility(bAlive && bOnScreen && !bHideVolume && !bRaymarch);
	if (!bAlive || bHideVolume || bRaymarch)
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
	FramesAt(T, F0, F1, Alpha);
	const uint32 Key = GetTypeHash(this);
	// a resolução inteira (mip 0), salvo quem pede menos (StreamMip: as bolas
	// pequenas do fogo da carcaça). Pedir o mip 1 pras distantes travava o jogo
	// 2–3.5 s esperando o disco (medido 05/10), mas era com o pedido bloqueante;
	// ao vivo os pedidos agora são assíncronos
	const float Mip = StreamMip;
	if (!bOnScreen)
	{
		// fora da tela: nada de material nem caixa, só os quadros seguem chegando
		// (sem travar), pra ela voltar inteira quando a câmera virar. (Nas gravações
		// bloqueantes isto mistura os dois tipos de pedido na mesma textura, o que
		// a Unreal não garante; lá a câmera é fixa e a explosão fica na tela.)
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
	// a gravação em baixa resolução: a normal da fumaça pro deslocamento piroclástico
	if (Bake->Normal)
	{
		MID->SetTextureParameterValue(ParamBakeNormal, Bake->Normal);
		MID->SetScalarParameterValue(ParamVelFrame0, float(F0));
		MID->SetScalarParameterValue(ParamVelFrame1, float(F1));
		MID->SetScalarParameterValue(ParamVelFrames, float(Bake->FrameTimes.Num()));
		MID->SetScalarParameterValue(ParamVelRes, float(Bake->VelocityResolution));
	}
	// o volume marcha em voxels LOCAIS: com o ator escalado, a profundidade
	// óptica e o brilho acumulado precisam crescer com o tamanho (no WebGL o
	// passo era em metros do mundo e isso saía de graça)
	// (o ganho da fuligem por quadro: as simulações que dissolvem a fumaça; ExplosionBake.h)
	float Gain = Bake->SootGain.IsValidIndex(F1) ? FMath::Lerp(Bake->SootGain[F0], Bake->SootGain[F1], Alpha) : 1.0f;
	if (EarlyEnd > 0.0f)
	{
		Gain *= FMath::Lerp(EarlyThickness, 1.0f, FMath::SmoothStep(0.7f * EarlyEnd, EarlyEnd, T));
	}
	MID->SetScalarParameterValue(ParamDensity, SizeScale * Opacity * Thickness * Gain);
	static const float EmissionTest = CommandLineScale(TEXT("-ExplosionEmission="));
	MID->SetScalarParameterValue(ParamEmission, SizeScale * Opacity * Thickness * Gain * EmissionTest);
	const FSkirt Skirt = SkirtAt(T, Bake->Duration(), SkirtHeight, SkirtLife);
	if (!bDustTintSet)
	{
		// a cor da poeira é a do chão do campo (areia, neve, terra)
		if (const ABlastLabEnvironment* Env = ABlastLabEnvironment::Find(this))
		{
			MID->SetVectorParameterValue(ParamSkirtTint, Env->DustTint);
		}
		bDustTintSet = true;
	}
	MID->SetScalarParameterValue(ParamSkirtRadius, Skirt.RadiusM);
	MID->SetScalarParameterValue(ParamSkirtHeight, Skirt.HeightM);
	MID->SetScalarParameterValue(ParamSkirtDust, Skirt.Dust);
	// a linha de comando (A/B) primeiro; os desta explosão (o fogo, a coluna) por cima
	static const TArray<TPair<FName, float>> Overrides = CommandLineParams();
	for (const TPair<FName, float>& P : Overrides)
	{
		MID->SetScalarParameterValue(P.Key, P.Value);
	}
	for (const TPair<FName, float>& P : MaterialParams)
	{
		MID->SetScalarParameterValue(P.Key, P.Value);
	}
	const float Dip = FMath::SmoothStep(0.5f, 0.7f, T) * (1.0f - FMath::SmoothStep(1.1f, 1.3f, T));
	if (TempBase > 0.0f)
	{
		MID->SetScalarParameterValue(TEXT("TempScale"), TempBase * FMath::Lerp(1.0f, TempDip, Dip)
			* FMath::Lerp(1.0f, TempLate, FMath::SmoothStep(1.5f, 2.0f, T)));
	}
	if (EmissionBase > 0.0f)
	{
		MID->SetScalarParameterValue(TEXT("EmissionBoost"), EmissionBase * FMath::Lerp(1.0f, EmissionDip, Dip));
	}

	// caixa marchada justa: mesma escala do quadro gravado (1 voxel = domínio/resolução), deslocada pro canto
	if (!Bake->BoxMin.IsValidIndex(F0) || !Bake->BoxSize.IsValidIndex(F0))
	{
		return;
	}
	FIntVector Min = Bake->BoxMin[F0];
	FIntVector Size = Bake->BoxSize[F0];
	if (F1 < F0 && Bake->BoxMin.IsValidIndex(F1) && Bake->BoxSize.IsValidIndex(F1))
	{
		// na emenda do loop a caixa do último quadro não cobre o primeiro: a união
		const FIntVector Hi(FMath::Max(Min.X + Size.X, Bake->BoxMin[F1].X + Bake->BoxSize[F1].X),
			FMath::Max(Min.Y + Size.Y, Bake->BoxMin[F1].Y + Bake->BoxSize[F1].Y), FMath::Max(Min.Z + Size.Z, Bake->BoxMin[F1].Z + Bake->BoxSize[F1].Z));
		Min = FIntVector(FMath::Min(Min.X, Bake->BoxMin[F1].X), FMath::Min(Min.Y, Bake->BoxMin[F1].Y), FMath::Min(Min.Z, Bake->BoxMin[F1].Z));
		Size = Hi - Min;
	}
	if (Skirt.Dust > 0.0f)
	{
		// a caixa marchada cresce pra caber a saia (frente com as bolhas e o topo)
		const float VoxM = Bake->DomainCm / 100.0f / FMath::Max(Bake->Resolution, 1);
		const int32 C = Bake->Resolution / 2;
		const int32 Rv = FMath::CeilToInt32((Skirt.RadiusM * 1.25f + 1.0f) / VoxM);
		const int32 Hv = FMath::CeilToInt32((Skirt.HeightM * 2.6f + 1.0f) / VoxM);      // o tronco do meio sobe até ~2.5× a altura
		const FIntVector Lo(FMath::Clamp(FMath::Min(Min.X, C - Rv), 0, Bake->Resolution), FMath::Clamp(FMath::Min(Min.Y, C - Rv), 0, Bake->Resolution), 0);
		const FIntVector Hi(FMath::Clamp(FMath::Max(Min.X + Size.X, C + Rv), 0, Bake->Resolution), FMath::Clamp(FMath::Max(Min.Y + Size.Y, C + Rv), 0, Bake->Resolution),
			FMath::Clamp(FMath::Max(Min.Z + Size.Z, Hv), 0, Bake->Resolution));
		Min = Lo;
		Size = Hi - Lo;
	}
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
	const bool bOn = bLights && Bake && Bake->LightColor.Num() && Age >= 0.0f && !IsFinished() && Opacity > 0.0f;
	FireLight->SetVisibility(bOn);
	FireLightVolume->SetVisibility(bOn);
	const float Tau = FlashTauS > 0.0f ? FlashTauS : FlashTau;
	const bool bFlashNow = bFlash && bOn && T < 8.0f * Tau;
	FlashLight->SetVisibility(bFlashNow);
	FlashStar->SetVisibility(bOn && T < StarEnd && StarMaterial != nullptr);
	if (!bOn)
	{
		return;
	}
	const float Area = SizeScale * SizeScale;      // a luz vai com a área (W^(2/3))
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	const float Ambient = FMath::Max(Sim ? Sim->AmbientLux() : 52000.0f, 1.0f);
	if (bFlashNow)
	{
		// o clarão: pico no 1º quadro e queda de 35 ms (2–3 quadros), luz própria
		const float FlashCandela = FlashGain * FMath::Exp(-T / Tau) * SceneLightFactor * LuxPerUnit * Area;
		FlashLight->SetIntensity(FlashCandela);
		if (FlashHeightCm > 0.0f)
		{
			// a escala do ator leva junto: em cm locais
			FlashLight->SetRelativeLocation(FVector(0.0f, 0.0f, FlashHeightCm / FMath::Max(SizeScale, 0.01f)));
		}
		const float Reach = FMath::Clamp(FMath::Sqrt(FlashCandela / (LightCutoff * Ambient)) * 100.0f, MinSceneReachCm * SizeScale, 30000.0f * SizeScale);
		if (FMath::Abs(FlashLight->AttenuationRadius - Reach) > 0.1f * Reach)
		{
			FlashLight->SetAttenuationRadius(Reach);
		}
	}
	if (StarMaterial && T < StarEnd)
	{
		StarMaterial->SetScalarParameterValue(TEXT("Intensity"), StarLum * FMath::Exp(-T / StarTau));
	}
	int32 F0, F1;
	float Alpha;
	FramesAt(T, F0, F1, Alpha);
	const FVector Position = FMath::Lerp(Bake->LightPosition[F0], Bake->LightPosition[F1], Alpha);
	FLinearColor Color = FMath::Lerp(Bake->LightColor[F0], Bake->LightColor[F1], Alpha);
	// a curva da luz gravada levada à física (LightCurveK, acima)
	if (BakeLightPeak < 0.0f)
	{
		BakeLightPeak = 0.0f;
		for (const FLinearColor& C : Bake->LightColor)
		{
			BakeLightPeak = FMath::Max(BakeLightPeak, FMath::Max3(C.R, C.G, C.B));
		}
	}
	const float RawPeak = FMath::Max3(Color.R, Color.G, Color.B);
	if (BakeLightPeak > 0.0f && RawPeak > 0.0f)
	{
		Color *= FMath::Pow(RawPeak / BakeLightPeak, LightCurveK - 1.0f);
	}
	const float Peak = FMath::Max3(Color.R, Color.G, Color.B);
	const FLinearColor Tint = Peak > 1e-6f ? FLinearColor(Color.R / Peak, Color.G / Peak, Color.B / Peak) : FLinearColor::Black;
	static const float VolumeLightTest = CommandLineScale(TEXT("-ExplosionVolumeLight="));
	const float VolumeCandela = VolumeLightFactor * VolumeLightTest * LuxPerUnit * Area * LightScale * VolumeLightScale;
	// A luz de dentro com sombra é um cache de luz por volume que ela alcança
	// (~6 ms na morte de tanque, 07/10): só no ultra (FullDetail). Abaixo dele
	// fica só o preenchimento sem sombra do material, na fração do WebGL.
	// Medição: -VolumeLightMode=0 sem ela, 1 com ela, 2 sem ela e com a luz
	// inteira no preenchimento
	static const int32 ForcedLightMode = [] { int32 M = -1; FParse::Value(FCommandLine::Get(), TEXT("-VolumeLightMode="), M); return M; }();
	const int32 VolumeLightMode = ForcedLightMode >= 0 ? ForcedLightMode : (FullDetail() ? 1 : 0);
	const float FillFraction = VolumeLightMode == 2 ? 1.0f : FireFillFraction;
	UMaterialInstanceDynamic* MID = bOnScreen ? VolumeMaterial() : nullptr;
	if (MID)
	{
		// a parte sem sombra, no material: posição no canto do domínio (m) e intensidade em cd
		const FVector Corner(-0.5f * Bake->DomainCm, -0.5f * Bake->DomainCm, 0.0f);
		const FVector FireLocalM = (Position - Corner) * 0.01f;
		MID->SetVectorParameterValue(TEXT("FireLocalM"), FLinearColor(FireLocalM.X, FireLocalM.Y, FireLocalM.Z, 0.0f));
		MID->SetVectorParameterValue(TEXT("FireFill"), Color * (VolumeCandela * FillFraction));
		MID->SetScalarParameterValue(TEXT("SizeScale"), SizeScale);
	}
	const float SceneCandela = Peak * SceneLightFactor * LuxPerUnit * Area * LightScale;
	for (UPointLightComponent* L : { FireLight.Get(), FireLightVolume.Get() })
	{
		const bool bVolume = L == FireLightVolume;
		if (bVolume && VolumeLightMode != 1)
		{
			if (L->IsVisible())
			{
				L->SetVisibility(false);
			}
			continue;
		}
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
			Reach = FMath::Clamp(FMath::Sqrt(SceneCandela / (LightCutoff * Ambient)) * 100.0f,
				MinSceneReachCm * SizeScale, 30000.0f * SizeScale);
		}
		if (FMath::Abs(L->AttenuationRadius - Reach) > 0.1f * Reach)
		{
			L->SetAttenuationRadius(Reach);
		}
		// Sombra só quando se vê: a do cenário, se o chão perto da explosão está
		// na tela e ela não é um ponto; a de dentro, se o volume está na tela.
		// O fogo em loop (a carcaça queima 2 min) para de fazer sombra no cenário
		// depois dos primeiros LoopShadowS abaixo do ultra: uma luz pontual com
		// sombra virtual custava ~2–3 ms de placa o tempo todo que ela queimava
		// (07/10), e uma batalha tem várias carcaças. No começo ela fica: a luz
		// mora dentro do casco, e sem a sombra dele ela acendia de creme o rastro
		// escuro da torre (A/B de 07/10, captures/ue/_ab_desempenho.jpg)
		constexpr float LoopShadowS = 6.0f;
		// Numa barragem, abaixo do ultra só as mais importantes na tela (o orçamento
		// de detalhe, LodFloor 0: as 3 maiores) fazem sombra; as outras acendem o chão
		// sem ela. 12 Cargas Pesadas eram 12 luzes pontuais com sombra virtual: ~8 ms
		// de luzes + ~8 ms de sombras e uma travada de 2.4 s quando nasciam (08/10,
		// deserto, captures/ue/RM_barr12)
		const bool bShadow = bVolume ? bOnScreen
			: (bSceneShadow && bShadowRegionOnScreen && ScreenFrac > MinShadowFraction && (LoopPeriod <= 0.0f || FullDetail() || Age < LoopShadowS)
				&& (FullDetail() || LodFloor == 0));
		if (L->CastShadows != bShadow)
		{
			L->SetCastShadows(bShadow);
		}
	}
}
