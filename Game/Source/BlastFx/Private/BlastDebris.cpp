// Cópia do BlastLab (BlastDebris, 14/10/2026): ver BlastDebris.h.
#include "BlastDebris.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "BlastFxDirector.h"
#include "BlastFxGround.h"
#include "BlastFxHitLife.h"
#include "BlastRaymarch.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "HAL/IConsoleManager.h"

// o custo no perfil (-csvGpuStats -csvCaptureFrames=N): BlastFxDebris/Tick, por quadro, somado em todos os destroços
CSV_DEFINE_CATEGORY(BlastFxDebris, true);

namespace
{
	// (a revisão da vida) o número de cada pedaço em brasa que solta o fio de fumaça em voo (o Id do ponto no RTSMissileSmoke)
	uint32 GEmberSmokeSerial = 0;
	const TCHAR* BasePath = TEXT("/Game/Materials/M_Base.M_Base");                 // build_scene.py
	const TCHAR* GlowPath = TEXT("/Game/Materials/M_DebrisGlow.M_DebrisGlow");     // build_debris.py
	const TCHAR* TrailPath = TEXT("/Game/Materials/M_TrailSmoke.M_TrailSmoke");    // build_debris.py

	constexpr float G = 9.81f;
	// Chapas (saias laterais, caixas de estiva, tampas): 0.3–1.2 m, lançadas a
	// 8–22 m/s, girando 1–3 voltas/s. Uma chapa fina cai planando: velocidade
	// terminal ~13 m/s (arrasto quadrático k = g/vt²). No vídeo do Veo são ~15
	// silhuetas pretas girando no quadro entre 0.2 e 2.5 s, de 0.5–1.5 m; o quadro
	// vê ~metade das que saem (as outras vão pra trás ou pra fora): 32. Com 16, na
	// folha lado a lado de 07/10 mal apareciam
	constexpr int32 NumPlates = 32, NumShapes = 6;
	constexpr float PlateDrag = G / (13.0f * 13.0f), PlateThickM = 0.03f;
	const FLinearColor PlateColor(0.035f, 0.031f, 0.028f);      // aço queimado, fuligem por cima
	constexpr float PlateRough = 0.6f, PlateMetal = 0.5f;
	// Cometas (pedaços de propelente e metal em brasa): cabeça branca-amarela,
	// 20–35 m/s, 15–65° acima do horizonte, queimam 1–2.5 s; densos e pequenos,
	// terminal ~30 m/s. No vídeo: 8–10, os rastros claros engordando até ~1 m.
	constexpr int32 NumComets = 9, NumSpikes = 60, NumEmbers = 150;
	const FLinearColor SpikeColor(1.0f, 0.86f, 0.62f), EmberColor(1.0f, 0.5f, 0.16f);
	constexpr float CometDrag = G / (30.0f * 30.0f);
	constexpr float CometHeadM = 0.55f;
	// a cabeça: a cor; o brilho (~2·10⁵ cd/m² no miolo, metal a ~1900 K) é o
	// Intensity do M_DebrisGlow
	const FLinearColor CometHeadColor(1.0f, 0.78f, 0.45f);
	// a fumaça sobe devagar (quente e leve) e engorda; a massa por metro fica,
	// então a opacidade cai com a largura
	constexpr float TrailRise = 0.35f, TrailEmitM = 0.12f;
	// o vento da fumaça parada (16/10; o jogo ainda não tem vento: fixo, uma brisa de ~0,8 m/s, como o vento do ZH que
	// deita as colunas de fumaça das carcaças sempre pro mesmo lado da tela)
	const FVector SmokeWind(0.55, -0.55, 0.0);
	// Desempenho (07/10): os rastros chegavam a ~15 mil vértices refeitos a cada
	// quadro (um ponto a cada 12 cm de voo). Cada rastro é reamostrado em
	// TrailSamples pontos ao longo do comprimento, e as cabeças e faíscas têm
	// topologia fixa (as mortas viram quads de tamanho zero): a seção delas nasce
	// uma vez e só os vértices mudam
	constexpr int32 TrailSamples = 48;

	// o tamanho da bola (-BlastScale): os espinhos encolhem e esquentam junto (Start)
	float SparkScaleCfg()
	{
		static const float V = [] { float S = 1.0f; FParse::Value(FCommandLine::Get(), TEXT("BlastScale="), S); return FMath::Clamp(S, 0.2f, 1.0f); }();
		return V;
	}

	FVector ToM(const FVector& Cm) { return Cm / 100.0; }
	// o rumo pro sol (o sol mais forte do mapa, uma vez por mundo): o brilho dos cacos de vidro
	FVector SunDirOf(const UWorld* World)
	{
		static TWeakObjectPtr<const UWorld> Cached;
		static FVector Dir = FVector(0.3, 0.3, 0.9).GetSafeNormal();
		if (Cached.Get() != World && World)
		{
			Cached = World;
			float Best = -1.0f;
			for (TActorIterator<ADirectionalLight> It(World); It; ++It)
			{
				const UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent());
				if (L && L->IsVisible() && L->Intensity > Best)
				{
					Best = L->Intensity;
					Dir = -L->GetDirection();
				}
			}
		}
		return Dir;
	}
	FVector ToCm(const FVector& M) { return M * 100.0; }

	FVector CameraM(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC && PC->PlayerCameraManager ? ToM(PC->PlayerCameraManager->GetCameraLocation()) : FVector::ZeroVector;
	}

	UProceduralMeshComponent* NewMesh(AActor* Owner, const TCHAR* Name, bool bShadow)
	{
		UProceduralMeshComponent* M = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->SetCastShadow(bShadow);
		M->bUseAsyncCooking = false;
		M->SetMobility(EComponentMobility::Movable);
		return M;
	}
}

ABlastDebris::ABlastDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	PlateMesh = NewMesh(this, TEXT("Plates"), true);
	RootComponent = PlateMesh;
	GlowMesh = NewMesh(this, TEXT("Glows"), false);
	GlowMesh->SetupAttachment(PlateMesh);
	TrailMesh = NewMesh(this, TEXT("Trails"), false);
	TrailMesh->SetupAttachment(PlateMesh);
}

void ABlastDebris::BuildShapes()
{
	// Chapa recortada de 1 m: polígono de 5–8 lados com raio sorteado (borda
	// rasgada), dobrada no meio (a chapa entorta ao arrancar) e com espessura:
	// face de cima, de baixo e a borda. Leque a partir do centro.
	Shapes.SetNum(NumShapes);
	for (FShape& S : Shapes)
	{
		const int32 N = Rand.RandRange(5, 8);
		const float Bend = Rand.FRandRange(-0.25f, 0.25f);
		const float Stretch = Rand.FRandRange(0.55f, 1.0f);
		TArray<FVector> Ring;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = (i + Rand.FRandRange(-0.3f, 0.3f)) / N * 2.0f * PI;
			const float R = 0.5f * Rand.FRandRange(0.55f, 1.0f);
			const float X = R * FMath::Cos(A), Y = R * FMath::Sin(A) * Stretch;
			Ring.Add(FVector(X, Y, Bend * FMath::Abs(X)));
		}
		const FVector Up(0.0f, 0.0f, PlateThickM * 0.5f);
		auto AddVert = [&S](const FVector& P, const FVector& Nrm) { S.Verts.Add(P); S.Normals.Add(Nrm); return S.Verts.Num() - 1; };
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float Sg = Side == 0 ? 1.0f : -1.0f;
			const int32 C = AddVert(FVector(0.0f, 0.0f, 0.0f) + Up * Sg, FVector(0.0f, 0.0f, Sg));
			const int32 First = S.Verts.Num();
			for (const FVector& P : Ring)
			{
				// a dobra inclina a normal pros lados
				const FVector Nrm = FVector(-Bend * FMath::Sign(P.X) * Sg, 0.0f, Sg).GetSafeNormal();
				AddVert(P + Up * Sg, Nrm);
			}
			for (int32 i = 0; i < N; ++i)
			{
				const int32 A = First + i, B = First + (i + 1) % N;
				if (Side == 0) { S.Tris.Append({ C, B, A }); }
				else { S.Tris.Append({ C, A, B }); }
			}
		}
		for (int32 i = 0; i < N; ++i)
		{
			const FVector P0 = Ring[i], P1 = Ring[(i + 1) % N];
			const FVector Out = FVector::CrossProduct(P1 - P0, FVector::UpVector).GetSafeNormal();
			const int32 A = AddVert(P0 + Up, Out), B = AddVert(P1 + Up, Out), C = AddVert(P1 - Up, Out), D = AddVert(P0 - Up, Out);
			S.Tris.Append({ A, C, B, A, D, C });
		}
	}
}

int32 ABlastDebris::NewTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0)
{
	FTrail& Tr = Trails.AddDefaulted_GetRef();
	Tr.Width0 = Width0M;
	Tr.Growth = GrowthMps;
	Tr.Life = LifeS;
	Tr.Albedo = Albedo;
	Tr.GlowS = GlowS;
	Tr.Opacity0 = Opacity0;
	Tr.Seed = Rand.FRand();
	return Trails.Num() - 1;
}

int32 ABlastDebris::AddFollowTrail(float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0)
{
	return NewTrail(Width0M, GrowthMps, LifeS, Albedo, GlowS, Opacity0);
}

// =============================================================================
// o tiro (14/10, a v3). Números de partida, a conferir contra a arte 4.2 e os game shots no jogo (o estudo de AAA
// desta noite: docs/TIRO_E_ACERTO.md). Tamanhos em metros, velocidades em m/s, a luz em candelas.

void ABlastDebris::EnsureLight(const FLinearColor& Color, float RadiusCm)
{
	if (!Light)
	{
		Light = NewObject<UPointLightComponent>(this, TEXT("ShotLight"));
		Light->SetupAttachment(PlateMesh);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);              // curta e pequena: sombra custaria mais que ela inteira
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetUseInverseSquaredFalloff(true);
		Light->RegisterComponent();
	}
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(RadiusCm);
	Light->SetIntensity(0.0f);
	LightAge = 0.0f;
}

void ABlastDebris::UpdateLight(float Dt)
{
	if (!Light || LightPeak <= 0.0f)
	{
		return;
	}
	LightAge += Dt;
	float I = 0.0f;
	if (T < FireUntil)
	{
		// o fogo da carcaça: a luz laranja tremendo (duas senoides fora de fase) e apagando nos últimos 3 s
		const float Fade = FMath::Clamp((FireUntil - T) / 3.0f, 0.0f, 1.0f);
		// (17/10) com a coluna em volume, a pulsação da chama de ~2 m: f ≈ 1,5/√D (~1,0 Hz no anel da torre, ~1,7 Hz nas
		// aberturas; docs/ESTUDO_CARCACA.md), não as senoides rápidas de antes
		const float Flicker = bWreckFlames ? 0.75f + 0.15f * FMath::Sin(T * 13.0f) + 0.1f * FMath::Sin(T * 31.0f + 1.7f)
			: 0.78f + 0.14f * FMath::Sin(T * 6.28f) + 0.08f * FMath::Sin(T * 10.7f + 1.7f);
		Light->SetWorldLocation(ToCm(FirePos + FVector(0.0f, 0.0f, 1.0f)));
		Light->SetIntensity(LightPeak * Flicker * Fade * ExpoComp);
		Light->SetVisibility(true);
		LightAge = 0.0f;
		return;
	}
	if (ArcLightUntil > 0.0f)
	{
		// o arco temporizado: a luz azul-branca piscando pelo quadro (0,35–1,0, 12% apagada), e apaga de vez no fim (o
		// religador abre)
		if (T < ArcLightUntil)
		{
			Light->SetWorldLocation(ToCm(LightAt));
			Light->SetIntensity(FMath::FRand() < 0.12f ? 0.0f : LightPeak * FMath::FRandRange(0.35f, 1.0f) * ExpoComp);
			Light->SetVisibility(true);
			LightAge = 0.0f;
			return;
		}
		ArcLightUntil = -1.0f;
		LightPeak = 0.0f;
		Light->SetIntensity(0.0f);
		return;
	}
	if (BlinkUntil > 0.0f)
	{
		// o pisca-alerta: 1,5 Hz (90 por minuto), meio ciclo aceso; no fim apaga de vez (sem a queda do clarão)
		if (T < BlinkUntil)
		{
			Light->SetWorldLocation(ToCm(LightAt));
			Light->SetIntensity(FMath::Fmod(T * 1.5f, 1.0f) < 0.5f ? LightPeak * ExpoComp : 0.0f);
			Light->SetVisibility(true);
			LightAge = 0.0f;
			return;
		}
		BlinkUntil = -1.0f;
		LightPeak = 0.0f;
		Light->SetIntensity(0.0f);
		return;
	}
	if (bLightHold)
	{
		// o míssil: acesa enquanto o motor queima, presa atrás do bocal (o diretor move a cada quadro)
		Light->SetWorldLocation(ToCm(LightAt));
		Light->SetIntensity(LightPeak * ExpoComp);
		Light->SetVisibility(true);
		LightAge = 0.0f;
		return;
	}
	if (Comets.IsValidIndex(LightFollow) && Comets[LightFollow].Age < Comets[LightFollow].Burn)
	{
		// o projétil: acesa enquanto voa, junto da cabeça
		I = LightPeak;
		Light->SetWorldLocation(ToCm(Comets[LightFollow].P));
		LightAge = 0.0f;
	}
	else
	{
		if (Comets.IsValidIndex(LightFollow))
		{
			LightTau = 0.03f;                       // chegou: apaga depressa
			LightAt = Comets[LightFollow].P;
			LightFollow = -1;
		}
		// (23/10) com LightHold 0 e LightPow 1 é o exp(−t/τ) de sempre; a ogiva com a bola em volume segura o pico e cai na forma
		// da curva da bola (StartMissileHit)
		const float Since = FMath::Max(LightAge - LightHold, 0.0f) / FMath::Max(LightTau, 0.005f);
		I = LightPeak * FMath::Exp(-(LightPow == 1.0f ? Since : FMath::Pow(Since, LightPow)));
		Light->SetWorldLocation(ToCm(LightAt));
	}
	if (I < 0.01f * LightPeak && LightFollow < 0)
	{
		LightPeak = 0.0f;
		I = 0.0f;
	}
	Light->SetIntensity(I * ExpoComp);
	Light->SetVisibility(I > 0.0f);
}

void ABlastDebris::StartImpact(const FVector& PointCm, const FVector& Normal, const FVector& ShellDir, int32 Seed)
{
	Rand.Initialize(Seed);
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const float Keep = Eq >= 3 ? 1.0f : (Eq == 2 ? 0.5f : 0.25f);
	BallScale = 1.0f;                               // as faíscas brancas-amarelas da arte, não as laranja da morte
	BuildShapes();
	const FVector O = ToM(PointCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector N = Normal.GetSafeNormal();
	const FVector D = ShellDir.GetSafeNormal();
	// o leque sai pro lado do ricochete (o projétil refletido na blindagem), aberto pra fora da chapa
	const FVector R = (D - 2.0f * FVector::DotProduct(D, N) * N).GetSafeNormal();
	const FVector Axis = (R * 0.5f + N * 0.35f + FVector(0.0f, 0.0f, 0.5f)).GetSafeNormal();   // (16/10: sobe como na 4.2)
	// o clarão no metal: o miolo branco (0–50 ms, 0,15–0,25 do tanque) e o brilho branco-amarelo que fica (até ~0,3 s,
	// ~0,5 do tanque: menor que o do ZH pra não tapar unidade)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + N * 0.15f;
		S.Life = 0.05f;
		S.Size = 0.8f;
		S.Bright = 1.0f;
		S.bRound = true;
	}
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + N * 0.3f;
		S.Life = 0.3f;
		S.Size = 0.9f;                              // (a 1,3 crescendo 3 m/s era uma bola de névoa branca de ~5 m)
		S.Grow = 1.5f;
		S.Bright = 0.6f;
		S.Color = FLinearColor(1.0f, 0.8f, 0.45f, 1.0f);
		S.bRound = true;
	}
	// os riscos do leque (a arte: dezenas de riscos brancos-amarelos de 1–3 m até ~1 tanque, em ~90°)
	// (16/10, contra a arte 4.2 no jogo: com 8–14 cm de largura e todos iguais, saindo de um ponto, eram barras brancas
	// numa estrela desenhada; a arte tem riscos finos e variados, pontinhos, esfriando pro laranja)
	const int32 Spikes = FMath::Max(16, FMath::RoundToInt(110 * Keep));
	for (int32 i = 0; i < Spikes; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + N * 0.05f + Rand.VRand() * Rand.FRandRange(0.0f, 0.15f);
		S.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(48.0f)) * Rand.FRandRange(15.0f, 35.0f);
		S.Life = Rand.FRandRange(0.12f, 0.45f);
		S.Size = Rand.FRandRange(0.012f, 0.025f);
		S.Bright = Rand.FRandRange(0.4f, 1.0f);
		S.Drag = G / FMath::Square(22.0f);
		S.MinPxK = 0.0004f;                         // ≥ 1,5–2 px a qualquer distância
		S.StreakT = Rand.FRandRange(1.0f / 30.0f, 1.0f / 10.0f);   // do pontinho ao risco de ~3 m
		S.bBounce = true;
		S.bCool = true;
		S.Color = FLinearColor(1.0f, 0.82f, 0.5f, 1.0f);   // amarelo-branco quente (a arte), não o branco puro
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// as fagulhas que caem piscando depois
	const int32 Embers = FMath::RoundToInt(30 * Keep);
	for (int32 i = 0; i < Embers; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.bEmber = true;
		S.P = O + N * 0.05f;
		S.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(70.0f)) * Rand.FRandRange(3.0f, 10.0f);
		S.Life = Rand.FRandRange(0.6f, 1.6f);
		S.Size = Rand.FRandRange(0.04f, 0.07f);
		S.Bright = Rand.FRandRange(0.12f, 0.25f);
		S.Drag = G / FMath::Square(6.0f);
		S.Lift = 0.3f;
		S.Age = -Rand.FRandRange(0.0f, 0.08f);
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// lascas: tinta, terra e metal (a arte: pedacinhos caindo embaixo do tanque)
	const int32 Chips = FMath::Max(2, FMath::RoundToInt(6 * Keep));
	for (int32 i = 0; i < Chips; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + N * 0.1f;
		P.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(55.0f)) * Rand.FRandRange(5.0f, 12.0f);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(10.0f, 25.0f);
		P.Size = Rand.FRandRange(0.15f, 0.25f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	BuildPlateSection();
	// a luz: a do ZH (255, 255, 128), raio ~1 tanque, cai em ~0,35 s
	EnsureLight(FLinearColor(1.0f, 0.78f, 0.45f), 800.0f);
	LightAt = O + N * 0.6f;
	LightPeak = 6.0e5f;                             // na referência (~1,9·10⁵ no jogo; a 8·10⁵ sem compensar lavava o casco de branco)
	LightTau = 0.12f;
	SetLifeSpan(20.0f);
}

void ABlastDebris::StartMuzzle(const FVector& MuzzleCm, const FVector& Dir, int32 Seed)
{
	// (16/10) a bola secundária agora é do raymarch (UBlastFxDirector::MuzzleBlast): fogo de verdade, que também tapa o
	// que está atrás; a aditiva fica só pro teste (-ShotFxAdditiveBall=1)
	static const bool bAdditiveFireball = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxAdditiveBall="), V); return V != 0; }();
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(MuzzleCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector D = Dir.GetSafeNormal();
	const FVector Side = FVector::CrossProduct(D, FVector::UpVector).GetSafeNormal();
	auto Flash = [&](const FVector& At, float SizeM, float Life, bool bHot)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = At;
		S.Size = SizeM;
		S.Life = Life;
		// o miolo branco-quente; as línguas laranja. Na exposição de referência (16/10, a compensação): 0,25 = os 0,08 que
		// liam laranja no jogo sem compensar (a 1,0 e a 0,25 sem compensar estouravam em branco)
		S.Bright = bHot ? 1.0f : 0.25f;
		S.bEmber = !bHot;                           // as línguas de fora laranja, o miolo branco-amarelo
		S.bRound = true;
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	};
	// o miolo e as línguas de fogo à frente da boca, menores e mais curtas pra longe; os jatos do freio de boca pros lados
	Flash(O + D * 0.3f, 1.0f, 0.066f, true);       // (16/10: a 0,7 m sumia na câmera de jogo de 67 m; a estrela da arte GPT é ~a frente da torre)
	Flash(O + D * 1.0f, 0.6f, 0.07f, false);
	Flash(O + D * 1.7f, 0.45f, 0.06f, false);
	Flash(O + D * 2.4f, 0.3f, 0.05f, false);
	Flash(O + D * 0.2f + Side * 0.5f, 0.32f, 0.05f, false);
	Flash(O + D * 0.2f - Side * 0.5f, 0.32f, 0.05f, false);
	// a bola secundária: o gás que reacende no ar à frente (15–150 ms, pico ~50 ms; ~0,5–0,75 do tanque, menor que os
	// 7–13 m do infravermelho, pela leitura; o estudo: docs/TIRO_E_ACERTO.md), laranja escurecendo; em 1 de 4 tiros, fraca
	if (bAdditiveFireball)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + D * 2.2f;
		S.Size = 0.6f;
		S.Grow = 14.0f;                             // 0,6 → ~2,7 m de meia largura em 0,15 s
		S.Life = 0.15f;
		S.Age = -0.015f;
		S.Bright = Rand.FRand() < 0.25f ? 0.025f : 0.05f;  // laranja (a 0,8 e a 0,12 ainda eram uma bola creme)
		S.Color = FLinearColor(1.0f, 0.45f, 0.12f, 1.0f);
		S.bRound = true;
	}
	// riscos à frente, num cone estreito (os grãos de pólvora acesos)
	for (int32 i = 0; i < 8; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + D * 0.3f;
		S.V = Rand.VRandCone(D, FMath::DegreesToRadians(12.0f)) * Rand.FRandRange(40.0f, 80.0f);
		S.Life = Rand.FRandRange(0.04f, 0.08f);
		S.Size = Rand.FRandRange(0.012f, 0.02f);
		S.Bright = Rand.FRandRange(0.3f, 0.6f);
		S.Drag = G / FMath::Square(25.0f);
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// a luz: a 1,5·10⁶ cd e 12 m lavava o chão de branco de dia; o ZH usa raio de 3,9 m, o WoT 1,5–8
	EnsureLight(FLinearColor(1.0f, 0.7f, 0.4f), 900.0f);
	LightAt = O + D * 1.0f;
	LightPeak = 2.5e5f;                             // a 6·10⁵ ainda lavava o chão de creme em volta da boca
	LightTau = 0.035f;
	SetLifeSpan(5.0f);
}

int32 ABlastDebris::StartSmoke(float Width0M, float GrowthMps, float LifeS, float Albedo, float Opacity0, float RiseMps)
{
	if (!Shapes.Num())
	{
		Rand.Initialize(int32(GetUniqueID()));
		BuildShapes();
	}
	const int32 Trail = NewTrail(Width0M, GrowthMps, LifeS, Albedo, 0.0f, Opacity0);
	Trails[Trail].Rise = RiseMps;
	Trails[Trail].Wind = SmokeWind;
	Trails[Trail].bEmitting = false;                // até o diretor pôr a cabeça
	bTrailsDone = false;
	SetActorTickEnabled(true);
	SetLifeSpan(0.0f);                              // vive enquanto o tanque estiver ferido; o diretor encerra
	return Trail;
}

void ABlastDebris::StartFlameJet(const FVector& PointCm, int32 Seed)
{
	// o jato de chama pelo anel da torre (WoT/War Thunder: a chama focada saindo; aqui 0,3–0,5 s): bolas laranja que
	// sobem e crescem, fagulhas, e a luz laranja que cai em ~0,4 s
	Rand.Initialize(Seed);
	if (!Shapes.Num())
	{
		BuildShapes();
	}
	const FVector O = ToM(PointCm);
	for (int32 i = 0; i < 5; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + FVector(Rand.FRandRange(-0.3f, 0.3f), Rand.FRandRange(-0.3f, 0.3f), 0.2f * i);
		S.V = FVector(Rand.FRandRange(-0.6f, 0.6f), Rand.FRandRange(-0.6f, 0.6f), Rand.FRandRange(4.0f, 7.0f));
		S.Size = Rand.FRandRange(0.35f, 0.55f);
		S.Grow = 1.5f;
		S.Life = Rand.FRandRange(0.3f, 0.5f);
		S.Age = -0.05f * i;
		S.Bright = 0.1f;                            // laranja (na referência; sem compensar, 0,18 e 0,05 estouravam em branco)
		S.Lift = 1.0f;
		S.bRound = true;
		S.Color = FLinearColor(1.0f, 0.42f, 0.1f, 1.0f);
	}
	for (int32 i = 0; i < 20; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.bEmber = true;
		S.P = O;
		S.V = Rand.VRandCone(FVector::UpVector, FMath::DegreesToRadians(35.0f)) * Rand.FRandRange(3.0f, 9.0f);
		S.Life = Rand.FRandRange(0.6f, 1.4f);
		S.Size = Rand.FRandRange(0.03f, 0.06f);
		S.Bright = Rand.FRandRange(0.12f, 0.25f);
		S.Drag = G / FMath::Square(6.0f);
		S.Lift = 0.3f;
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	bGlowsDone = false;
	EnsureLight(FLinearColor(1.0f, 0.55f, 0.2f), 900.0f);
	LightAt = O + FVector(0.0f, 0.0f, 1.0f);
	LightPeak = 6.0e5f;
	LightTau = 0.15f;
	LightAge = 0.0f;
	SetActorTickEnabled(true);
}

void ABlastDebris::StartDirt(const FVector& PointCm, const FVector& AwayDir, int32 Seed)
{
	// o jato de terra da 4.1: torrões escuros subindo num cone de ~30° inclinado pra longe do atirador, 6–15 m/s
	// (sobem 2–10 m e caem em ~1–2 s), que ficam na areia em volta da cratera
	Rand.Initialize(Seed);
	if (!Shapes.Num())
	{
		BuildShapes();
	}
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const float Keep = Eq >= 3 ? 1.0f : (Eq == 2 ? 0.5f : 0.25f);
	const FVector O = ToM(PointCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector Axis = (FVector::UpVector * 0.85f + AwayDir.GetSafeNormal2D() * 0.45f).GetSafeNormal();
	const int32 Count = FMath::Max(8, FMath::RoundToInt(28 * Keep));
	for (int32 i = 0; i < Count; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + FVector(Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(0.05f, 0.3f));
		P.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(32.0f)) * Rand.FRandRange(6.0f, 15.0f);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(8.0f, 20.0f);
		P.Size = Rand.FRandRange(0.08f, 0.22f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	bPlatesSettled = false;
	BuildPlateSection();
	SetActorTickEnabled(true);
	SetLifeSpan(12.0f);
}

void ABlastDebris::StartGroundImpact(const FVector& PointCm, const FVector& AwayDir, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const float Keep = Eq >= 3 ? 1.0f : (Eq == 2 ? 0.5f : 0.25f);
	const FVector O = ToM(PointCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector F = AwayDir.GetSafeNormal2D();
	// (23/10, a vida) o míssil no chão: a escala da carga (DirtK; tudo 1 = o tiro na areia de hoje, exato)
	const float Kd = FMath::Max(DirtK, 0.1f);
	const float Kv = FMath::Max(DirtSpeedK, 0.1f);
	const float Kn = FMath::Min(Kd * Kd, 2.0f);       // as quantidades × K², no teto de 2× (o custo dos rastros)
	// só a terra com bDirtOnly: o clarão, as faíscas e a luz são do acerto do míssil (StartMissileHit)
	if (!bDirtOnly)
	{
		// o clarão: a bola de fogo de 1–2,5 kg de TNT tem 1,8–2,7 m e dura < 1 ms (OSTI 1469550); esticado a 2 quadros
		// (o fórum RealtimeVFX: clarão de 2–3 quadros), branco-amarelo, e um brilho laranja que fica até ~0,1 s
		{
			FSpark& S = Sparks.AddDefaulted_GetRef();
			S.P = O + FVector(0.0f, 0.0f, 0.6f);
			S.Life = 0.066f;
			S.Size = 1.25f;
			S.Bright = 1.0f;
			S.Color = FLinearColor(1.0f, 0.86f, 0.62f, 1.0f);
			S.bRound = true;
		}
		{
			FSpark& S = Sparks.AddDefaulted_GetRef();
			S.P = O + FVector(0.0f, 0.0f, 0.8f);
			S.Life = 0.12f;
			S.Size = 1.0f;
			S.Grow = 4.0f;
			S.Bright = 0.35f;
			S.Color = FLinearColor(1.0f, 0.55f, 0.2f, 1.0f);
			S.bRound = true;
		}
		// as faíscas: 25–40 num cone de ~60° em volta da vertical puxado pra frente, 0,15–0,35 s (o estudo)
		const FVector SparkAxis = (FVector::UpVector * 0.8f + F * 0.45f).GetSafeNormal();
		const int32 Spikes = FMath::Max(8, FMath::RoundToInt(32 * Keep));
		for (int32 i = 0; i < Spikes; ++i)
		{
			FSpark& S = Sparks.AddDefaulted_GetRef();
			S.P = O + FVector(0.0f, 0.0f, 0.2f) + Rand.VRand() * Rand.FRandRange(0.0f, 0.3f);
			S.V = Rand.VRandCone(SparkAxis, FMath::DegreesToRadians(60.0f)) * Rand.FRandRange(10.0f, 24.0f);
			S.Life = Rand.FRandRange(0.15f, 0.35f);
			S.Size = Rand.FRandRange(0.012f, 0.022f);
			S.Bright = Rand.FRandRange(0.4f, 0.9f);
			S.Drag = G / FMath::Square(20.0f);
			S.MinPxK = 0.0004f;
			S.StreakT = Rand.FRandRange(1.0f / 30.0f, 1.0f / 12.0f);
			S.bCool = true;
			S.Color = FLinearColor(1.0f, 0.8f, 0.5f, 1.0f);
			S.Seed = Rand.FRandRange(0.0f, 100.0f);
		}
	}
	// o jato de terra (v7, 16/10; a arte 4.1): a terra lançada é balística (grãos e torrões a 13–22 m/s, que o gás não
	// carrega: a gravação da coluna saía uma bola marrom), num leque estreito (±20°) inclinado 15° pra longe do atirador,
	// até ~10 m. Os riscos escuros da 4.1 são o borrão do voo: o rastro só enquanto sobe rápido (0,35 s) e curto, reto,
	// sumindo antes de virar arco (os rastros longos de antes viravam fios arqueados, "cabelo", e se acumulavam nos tiros
	// seguidos). A cabeça é preta (somada não aparece): só o rastro, escuro (albedo 0,12 × o tom da terra do diretor)
	// (23/10, a vida) o míssil no chão: o eixo a DirtElevDeg do chão (a cortina de material lançado sai a 45–58° do chão em areia:
	// Tsujido/Arakawa; o realtimevfx: "a maior parte da força num cone de ~45°"), no rumo do míssil
	const float JetFromUpDeg = DirtElevDeg > 0.0f ? 90.0f - FMath::Clamp(DirtElevDeg, 10.0f, 85.0f) : 15.0f;
	const FVector JetAxis = (FVector::UpVector * FMath::Cos(FMath::DegreesToRadians(JetFromUpDeg)) + F * FMath::Sin(FMath::DegreesToRadians(JetFromUpDeg))).GetSafeNormal();
	// (v7b, chao_v7: os riscos saíam um feixe estreito de linhas duras, uma "vassoura"; a 4.1 é um leque largo e macio
	// que sobe ~10 m: mais aberto, mais alto, mais finos e ralos, engordando no voo)
	// (23/10, a vida) os riscos × K (não K²: são os de mais custo, um rastro cada) e mais grossos × K
	const int32 Sprays = FMath::Max(8, FMath::RoundToInt(40 * Keep * FMath::Min(Kd, 2.0f)));
	for (int32 i = 0; i < Sprays; ++i)
	{
		FComet& C = Comets.AddDefaulted_GetRef();
		C.P = O + FVector(Rand.FRandRange(-0.3f, 0.3f), Rand.FRandRange(-0.3f, 0.3f), Rand.FRandRange(0.1f, 0.4f)) * Kd;
		C.V = Rand.VRandCone(JetAxis, FMath::DegreesToRadians(28.0f)) * (Rand.FRandRange(12.0f, 20.0f) * Kv);   // (17/10, a "capadinha": ~0,7 da altura)
		C.Burn = Rand.FRandRange(0.3f, 0.55f);
		C.HeadM = 0.05f;
		C.Color = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		C.Trail = NewTrail(Rand.FRandRange(0.08f, 0.14f) * Kd, 1.6f, 0.3f, 0.12f, 0.0f, Rand.FRandRange(0.35f, 0.55f));
	}
	// os torrões: 45 de 12–45 cm (de cima, 4–14 px) no mesmo leque, mais uns largos e lentos; escuros (a areia de baixo,
	// úmida); caem em 1–3 s e ficam
	// (23/10, a vida) × K² na quantidade (teto 2×), × K no tamanho (até 0,8 m), as velocidades × DirtSpeedK
	PlateTint = FLinearColor(0.07f, 0.055f, 0.04f, 1.0f);
	PlateRoughNow = 0.95f;
	const int32 Clods = FMath::Max(10, FMath::RoundToInt(45 * Keep * Kn));
	const int32 FirstClod = Plates.Num();
	for (int32 i = 0; i < Clods; ++i)
	{
		const bool bWide = i % 4 == 0;
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + FVector(Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(0.05f, 0.3f)) * Kd;
		P.V = Rand.VRandCone(JetAxis, FMath::DegreesToRadians(bWide ? 45.0f : 22.0f)) * ((bWide ? Rand.FRandRange(4.5f, 8.5f) : Rand.FRandRange(8.5f, 17.0f)) * Kv);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(8.0f, 20.0f);
		P.Size = FMath::Min(Rand.FRandRange(0.12f, 0.45f) * Kd, 0.8f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	if (DirtPuffs > 0)
	{
		// (23/10, a vida) os N maiores levantam um puff de areia quando deitam (o realtimevfx: "debris clumps and dust"; a fila de pousos)
		TArray<int32> BySize;
		for (int32 i = FirstClod; i < Plates.Num(); ++i)
		{
			BySize.Add(i);
		}
		BySize.Sort([this](int32 A, int32 B) { return Plates[A].Size > Plates[B].Size; });
		for (int32 i = 0; i < FMath::Min(DirtPuffs, BySize.Num()); ++i)
		{
			Plates[BySize[i]].Land = 1;
		}
	}
	BuildPlateSection();
	if (!bDirtOnly)
	{
		// a luz: a do WoT pro HE na areia (raio 2 → 10 m, apagada em 0,22 s, laranja → pêssego → laranja-escuro); aqui
		// laranja, 9 m, caindo em ~0,08 s (1/e), na exposição de referência dos destroços
		EnsureLight(FLinearColor(1.0f, 0.55f, 0.25f), 900.0f);
		LightAt = O + FVector(0.0f, 0.0f, 1.0f);
		LightPeak = 4.0e5f;
		LightTau = 0.08f;
	}
	SetActorTickEnabled(true);
	SetLifeSpan(12.0f);
}

void ABlastDebris::StartMissile(const FVector& PosCm, const FVector& Dir, int32 Seed, bool bSmokeTrail)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector P = ToM(PosCm);
	const FVector D = Dir.GetSafeNormal();
	MissileT0 = T;
	// a chama: o miolo branco de ~0,35 m (nunca menos de ~3 px), a língua (1; 0,7; 0,35) de ~2 m atrás do bocal e o halo
	// fraco de ~2 m (o brilho de ponta do MissileExhaust do ZH); refeitos a cada quadro pelo diretor
	const float Sizes[3] = { 0.35f, 0.22f, 1.1f };
	const float Brights[3] = { 1.0f, 0.5f, 0.1f };
	const FLinearColor Cols[3] = { FLinearColor(1.0f, 0.95f, 0.85f, 1.0f), FLinearColor(1.0f, 0.7f, 0.35f, 1.0f), FLinearColor(1.0f, 0.75f, 0.45f, 1.0f) };
	for (int32 k = 0; k < 3; ++k)
	{
		MissileSpark[k] = Sparks.Num();
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = P - D * 0.3f;
		S.V = k == 1 ? -D * 60.0f : FVector::ZeroVector;   // a língua: o risco de ~2 m (60 m/s × 1/30 s)
		S.Life = 1.0e4f;                                   // até o fim do míssil (EndMissile)
		S.Size = Sizes[k];
		S.Bright = Brights[k];
		S.Color = Cols[k];
		S.bRound = k != 1;
		S.bPinned = true;
		S.MinPxK = k == 0 ? 0.00075f : 0.0f;
		S.StreakT = 1.0f / 30.0f;
	}
	// os rastros: o sopro grosso só nos primeiros 0,15 s (1,0 m, engorda 2 m/s, 1,2 s, opacidade 0,6) e o fio de cruzeiro
	// (0,5 m, engorda 1,2 m/s, 1,4 s, albedo 0,8, opacidade 0,5): 1,4 s limpa o céu entre as rajadas (o ciclo da torre é
	// 2,75 s); o fogo do gás só nos ~2–3 m de trás (τ 0,025 s a ~100 m/s; com os 0,25 s dos destroços, 25 m de fita acesa)
	// (16/10) sem as fitas quando o jogo desenha a fumaça simulada do rastro (RTSMissileSmoke, a pedido do Felipe: "fumaça e
	// não borrões brancos"; UBlastFxDirector::SetExternalMissileSmoke)
	if (bSmokeTrail)
	{
		MissileTrail[0] = NewTrail(1.0f, 2.0f, 1.2f, 0.8f, 0.0f, 0.6f);
		MissileTrail[1] = NewTrail(0.5f, 1.2f, 1.4f, 0.8f, 10.0f, 0.5f);
		Trails[MissileTrail[1]].GlowTau = 0.025f;
	}
	for (int32 k = 0; k < 2 && bSmokeTrail; ++k)
	{
		Trails[MissileTrail[k]].Rise = 0.3f;
		Trails[MissileTrail[k]].Wind = SmokeWind;
		SetTrailHead(MissileTrail[k], PosCm, true);
	}
	EnsureLight(FLinearColor(1.0f, 0.75f, 0.45f), 550.0f);
	LightPeak = 1.5e5f;
	LightAt = P - D * 0.5f;
	bLightHold = true;
	bGlowsDone = false;
	bTrailsDone = false;
	SetActorTickEnabled(true);
	SetLifeSpan(0.0f);
}

void ABlastDebris::UpdateMissile(const FVector& PosCm, const FVector& Dir)
{
	if (MissileT0 < 0.0f)
	{
		return;
	}
	const FVector P = ToM(PosCm);
	const FVector D = Dir.GetSafeNormal();
	const float Age = T - MissileT0;
	// a chama treme ±20% (est., 20–30 Hz) e nasce nos primeiros 2 quadros
	const float Flick = (0.8f + 0.2f * FMath::Sin(T * 151.0f + 1.3f) * FMath::Sin(T * 97.0f)) * FMath::Clamp(Age / 0.066f, 0.3f, 1.0f);
	const float Base[3] = { 1.0f, 0.5f, 0.1f };
	const FVector At[3] = { P - D * 0.25f, P - D * 1.2f, P - D * 0.5f };
	for (int32 k = 0; k < 3; ++k)
	{
		if (Sparks.IsValidIndex(MissileSpark[k]))
		{
			FSpark& S = Sparks[MissileSpark[k]];
			S.P = At[k];
			if (k == 1)
			{
				S.V = -D * 60.0f;
			}
			S.Bright = Base[k] * Flick;
		}
	}
	SetTrailHead(MissileTrail[0], ToCm(P - D * 0.6f), Age < 0.15f);
	SetTrailHead(MissileTrail[1], ToCm(P - D * 0.6f), true);
	LightAt = P - D * 0.5f;
	bGlowsDone = false;
}

void ABlastDebris::EndMissile()
{
	if (MissileT0 < 0.0f)
	{
		return;
	}
	MissileT0 = -1.0f;
	for (int32 k = 0; k < 3; ++k)
	{
		if (Sparks.IsValidIndex(MissileSpark[k]))
		{
			Sparks[MissileSpark[k]].Life = Sparks[MissileSpark[k]].Age;   // a chama apaga no quadro
		}
	}
	for (int32 k = 0; k < 2; ++k)
	{
		if (Trails.IsValidIndex(MissileTrail[k]))
		{
			Trails[MissileTrail[k]].bEmitting = false;
			Trails[MissileTrail[k]].GlowS = 0.0f;
		}
	}
	bLightHold = false;
	LightTau = 0.03f;                               // apaga depressa
	LightAge = 0.0f;
	SetLifeSpan(3.0f);
}

void ABlastDebris::StartLaunch(const FVector& MouthCm, const FVector& Dir, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(MouthCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector D = Dir.GetSafeNormal();
	// o clarão branco-amarelo de ~1,2 m por 2 quadros e a língua de ~1,5 m à frente por 0,06 s (o estudo)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + D * 0.3f;
		S.Life = 0.066f;
		S.Size = 0.6f;
		S.Bright = 1.0f;
		S.Color = FLinearColor(1.0f, 0.92f, 0.75f, 1.0f);
		S.bRound = true;
	}
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + D * 1.2f;
		S.Life = 0.06f;
		S.Size = 0.5f;
		S.Bright = 0.4f;
		S.Color = FLinearColor(1.0f, 0.7f, 0.35f, 1.0f);
		S.bRound = true;
	}
	// o jato de ré pela grelha de trás: 6–10 riscos laranja de 1,5–3 m por ~0,1 s
	for (int32 i = 0; i < 8; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O - D * 0.4f;
		S.V = Rand.VRandCone(-D, FMath::DegreesToRadians(14.0f)) * Rand.FRandRange(30.0f, 60.0f);
		S.Life = Rand.FRandRange(0.08f, 0.12f);
		S.Size = 0.03f;
		S.Bright = 0.6f;
		S.Color = FLinearColor(1.0f, 0.55f, 0.2f, 1.0f);
		S.Lift = 1.0f;
		S.StreakT = 1.0f / 20.0f;
		S.MinPxK = 0.0004f;
	}
	// a tampa da caixa: 2–3 pedaços de 0,3–0,45 m a 8–15 m/s, girando (patente US5993921: quebra em pedaços previstos)
	const FVector CapAxis = (D * 0.7f + FVector::UpVector * 0.5f).GetSafeNormal();
	for (int32 i = 0; i < 3; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + D * 0.2f;
		P.V = Rand.VRandCone(CapAxis, FMath::DegreesToRadians(35.0f)) * Rand.FRandRange(8.0f, 15.0f);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(12.0f, 25.0f);
		P.Size = Rand.FRandRange(0.3f, 0.45f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	BuildPlateSection();
	// a luz: 6·10⁵–1·10⁶ cd, τ 60 ms, raio 8 m
	EnsureLight(FLinearColor(1.0f, 0.8f, 0.55f), 800.0f);
	LightAt = O + D * 0.5f;
	LightPeak = 8.0e5f;
	LightTau = 0.06f;
	SetActorTickEnabled(true);
	SetLifeSpan(8.0f);
}

void ABlastDebris::StartMissileHit(const FVector& PointCm, const FVector& Normal, int32 Seed, float Scale, bool bGlowSprite)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(PointCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector N = Normal.GetSafeNormal();
	// (23/10) K = (W/4 kg)^⅓ (Hopkinson-Cranz a partir da receita da ogiva de 4 kg): os tamanhos, as distâncias e os tempos × K, as
	// velocidades iguais. Com K = 1 cada conta abaixo é a de antes, exata
	const float K = FMath::Max(Scale, 0.01f);
	// o clarão branco-azulado de ~2,5 m por 2 quadros (a ogiva explode do lado de fora da blindagem) e o brilho laranja
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + N * (0.3f * K);
		S.Life = 0.066f * K;
		S.Size = 1.25f * K;
		S.Bright = 1.0f;
		S.Color = FLinearColor(0.9f, 0.95f, 1.0f, 1.0f);
		S.bRound = true;
	}
	// (23/10) na ogiva v2 o brilho laranja em sprite sai: o fogo é um volume do raymarch (o corpo da ogiva, a morte ou a clássica),
	// que recebe a sombra do mapa, faz sombra e é aceso pela cena (a regra do CLAUDE.md: sprite chapado não é entrega, mesmo
	// provisório); (a 2ª revisão) sem vaga no raymarch também (ficam o clarão, as faíscas e a luz). Só a de antes o tem
	if (bGlowSprite)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + N * (0.5f * K);
		S.Life = 0.15f * K;
		S.Size = 1.0f * K;
		S.Grow = 2.0f;
		S.Bright = 0.35f;
		S.Color = FLinearColor(1.0f, 0.6f, 0.25f, 1.0f);
		S.bRound = true;
	}
	// as faíscas radiais (40–60, 0,2–0,5 s) e as fagulhas laranja (0,6–0,8 s)
	const FVector Axis = (N * 0.7f + FVector::UpVector * 0.4f).GetSafeNormal();
	// (23/10, a vida; BlastFxHitLife.h) com a direção do acerto: o eixo dos estilhaços é ℓ inclinado pra frente pelo leque da ogiva (Gurney:
	// os estilhaços saem num leque perpendicular ao míssil, inclinado pra onde ele ia por atan(Vm/Vf)); t = a parte de d ao longo da face
	const bool bAim = !HitAim.IsNearlyZero();
	const FVector Face = bAim && !HitNormal.IsNearlyZero() ? HitNormal.GetSafeNormal() : N;
	const FVector Dn = HitIncoming.GetSafeNormal();
	FVector Tf = Dn - Face * FVector::DotProduct(Dn, Face);
	Tf = Tf.SizeSquared() > 1.0e-4 ? Tf.GetSafeNormal() : FVector::ZeroVector;
	const FVector AimAxis = bAim ? (HitAim.GetSafeNormal() + Tf * FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(HitTiltDeg, 0.0f, 60.0f)))).GetSafeNormal() : Axis;
	if (bHitJets)
	{
		// (2) as faíscas em jatos (o fórum realtimevfx: "nunca espalhadas por igual"): 50·K² com teto 90; 55% em 3–7 jatos de 8–12° com o
		// centro dentro de 35–45° do eixo, 30% num leque rente à face (±15° do plano, a favor de t: o que a ogiva trazia), 15% voltando
		// (pra quem atirou). A vida 0,2–0,5 s sem o ×K: a faísca é a oxidação do ferro e não cresce com a carga (Kimura)
		const int32 Count = FMath::Min(90, FMath::RoundToInt32(50.0f * K * K));
		const int32 NumJets = Rand.RandRange(3, 7);
		const float Cone = FMath::DegreesToRadians(Rand.FRandRange(35.0f, 45.0f));
		TArray<FVector, TInlineAllocator<8>> Jets;
		TArray<float, TInlineAllocator<8>> JetHalf;
		for (int32 j = 0; j < NumJets; ++j)
		{
			Jets.Add(Rand.VRandCone(AimAxis, 0.8f * Cone));
			JetHalf.Add(FMath::DegreesToRadians(Rand.FRandRange(8.0f, 12.0f)));
		}
		// a base da face: t (ou qualquer um no plano, de frente) e o de lado
		FVector Ta = Tf.IsNearlyZero() ? FVector::CrossProduct(Face, FMath::Abs(Face.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal() : Tf;
		const FVector Tb = FVector::CrossProduct(Face, Ta).GetSafeNormal();
		const FVector Back = (Dn.IsNearlyZero() ? Face : -Dn);
		for (int32 i = 0; i < Count; ++i)
		{
			FSpark& S = Sparks.AddDefaulted_GetRef();
			S.P = O + Face * (0.1f * K) + Rand.VRand() * (Rand.FRandRange(0.0f, 0.3f) * K);
			const float U = Rand.FRand();
			FVector Dir;
			if (U < 0.55f)
			{
				const int32 j = Rand.RandRange(0, NumJets - 1);
				Dir = Rand.VRandCone(Jets[j], JetHalf[j]);
			}
			else if (U < 0.85f)
			{
				// rente à face: o rumo no plano (a favor de t: ±70°; de frente, pra todo lado) e de −5 a 15° pra fora
				const float Az = FMath::DegreesToRadians(Tf.IsNearlyZero() ? Rand.FRandRange(-180.0f, 180.0f) : Rand.FRandRange(-70.0f, 70.0f));
				const float El = FMath::DegreesToRadians(Rand.FRandRange(-5.0f, 15.0f));
				Dir = (Ta * FMath::Cos(Az) + Tb * FMath::Sin(Az)) * FMath::Cos(El) + Face * FMath::Sin(El);
			}
			else
			{
				Dir = Rand.VRandCone(Back, FMath::DegreesToRadians(40.0f));
				const float Into = float(FVector::DotProduct(Dir, Face));
				if (Into < 0.0f)
				{
					Dir -= Face * (2.0f * Into);         // o que voltaria pra dentro da chapa sai pelo reflexo
				}
			}
			S.V = Dir.GetSafeNormal() * Rand.FRandRange(15.0f, 35.0f);
			S.Life = Rand.FRandRange(0.2f, 0.5f);
			S.Size = Rand.FRandRange(0.012f, 0.025f);
			S.Bright = Rand.FRandRange(0.4f, 1.0f);
			S.Drag = G / FMath::Square(22.0f);
			S.MinPxK = 0.0004f;
			S.StreakT = Rand.FRandRange(1.0f / 30.0f, 1.0f / 10.0f);
			S.bBounce = true;
			S.bCool = true;
			S.Color = FLinearColor(1.0f, 0.85f, 0.55f, 1.0f);
			S.Seed = Rand.FRandRange(0.0f, 100.0f);
		}
	}
	else
	{
		for (int32 i = 0; i < 50; ++i)
		{
			FSpark& S = Sparks.AddDefaulted_GetRef();
			S.P = O + N * (0.1f * K) + Rand.VRand() * (Rand.FRandRange(0.0f, 0.3f) * K);
			S.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(75.0f)) * Rand.FRandRange(15.0f, 35.0f);
			S.Life = Rand.FRandRange(0.2f, 0.5f) * K;
			S.Size = Rand.FRandRange(0.012f, 0.025f);
			S.Bright = Rand.FRandRange(0.4f, 1.0f);
			S.Drag = G / FMath::Square(22.0f);
			S.MinPxK = 0.0004f;
			S.StreakT = Rand.FRandRange(1.0f / 30.0f, 1.0f / 10.0f);
			S.bBounce = true;
			S.bCool = true;
			S.Color = FLinearColor(1.0f, 0.85f, 0.55f, 1.0f);
			S.Seed = Rand.FRandRange(0.0f, 100.0f);
		}
	}
	for (int32 i = 0; i < 8; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.bEmber = true;
		S.P = O + N * (0.1f * K);
		S.V = Rand.VRandCone(bAim ? AimAxis : Axis, FMath::DegreesToRadians(70.0f)) * Rand.FRandRange(3.0f, 9.0f);
		S.Life = Rand.FRandRange(0.6f, 0.8f) * (bHitJets ? 1.0f : K);
		S.Size = Rand.FRandRange(0.04f, 0.07f);
		S.Bright = Rand.FRandRange(0.12f, 0.25f);
		S.Drag = G / FMath::Square(6.0f);
		S.Lift = 0.3f;
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// as lascas escuras (8–12 de 5–10 cm a 10–20 m/s); (23/10, a vida) com a direção, no eixo dos estilhaços
	for (int32 i = 0; i < 10; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + N * 0.1f;
		P.V = Rand.VRandCone(bAim ? AimAxis : Axis, FMath::DegreesToRadians(60.0f)) * Rand.FRandRange(10.0f, 20.0f);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(10.0f, 25.0f);
		P.Size = Rand.FRandRange(0.05f, 0.1f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	// (23/10, a vida; a camada Embers) os pedaços em brasa (o propelente e o metal que queimam 1–5 s; o FXFinal/BounceFX do ZH; os
	// pedaços com fumaça do War Thunder): 10–20 m/s no leque do míssil (o rumo dele no chão ±40°), 20–60° do chão, queimando 1–2,5 s.
	// (A revisão) O fio de fumaça em voo não é mais a fita das chapas (M_TrailSmoke: chapada, sem volume nem sombra; a regra do CLAUDE.md):
	// os HitEmberSmoke primeiros mandam o ponto pro diretor a cada quadro enquanto queimam, e o jogo solta a fuligem em volume no
	// RTSMissileSmoke (o mesmo do fiapo do pouso). Seguem caindo depois de apagar; ao pousar, a brasa fica piscando e o diretor recebe o
	// pouso (o puff de areia e o fiapo do jogo)
	if (HitEmbers > 0)
	{
		FVector Fwd(Dn.X, Dn.Y, 0.0);
		Fwd = Fwd.SizeSquared() > 1.0e-4 ? Fwd.GetSafeNormal() : FVector(Face.X, Face.Y, 0.0).GetSafeNormal();
		if (Fwd.IsNearlyZero())
		{
			Fwd = FVector::ForwardVector;
		}
		const float Az0 = float(FMath::Atan2(Fwd.Y, Fwd.X));
		for (int32 i = 0; i < HitEmbers; ++i)
		{
			FComet& C = Comets.AddDefaulted_GetRef();
			const float Az = Az0 + FMath::DegreesToRadians(Rand.FRandRange(-40.0f, 40.0f));
			const float El = FMath::DegreesToRadians(Rand.FRandRange(20.0f, 60.0f));
			C.P = O + Face * (0.15f * K);
			C.V = FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El)) * Rand.FRandRange(10.0f, 20.0f);
			C.Burn = Rand.FRandRange(1.0f, 2.5f);
			C.HeadM = Rand.FRandRange(0.12f, 0.2f);
			C.Color = FLinearColor(1.0f, 0.55f, 0.2f, 1.0f);
			C.Land = 1;
			if (i < HitEmberSmoke)
			{
				C.SmokeId = ++GEmberSmokeSerial ? GEmberSmokeSerial : ++GEmberSmokeSerial;   // nunca 0 (0 = sem fumaça)
			}
		}
	}
	BuildPlateSection();
	// a luz: 6–8·10⁵ cd, τ 120 ms, raio 10 m. (23/10) × K² na intensidade (a luz vai com a área, W^⅔: instances.js) e × K no alcance
	// e na queda: a do Tomahawk (12 kg, K 1,44) cai a 10% aos 0,40 s, quando cai a luz física da bola dele (a gravação da Carga
	// Pesada sem a compressão do WebGL: 10% aos 0,84 s × λ), e não mais aos 0,28 s da receita de 4 kg. Quando a bola em volume é da
	// ogiva, a forma da própria curva dela (o diretor dá o pico e a queda no relógio do corpo, FireLightHoldS/TauS): a luz cheia
	// enquanto a bola está no máximo, em vez de cair desde o 1º quadro, e acende as fumaças perto (RTSSceneLights) e as unidades. (A
	// 2ª revisão) Na morte e na clássica o diretor não põe: a bola é delas, com a luz delas, e aqui fica a queda do clarão
	EnsureLight(FLinearColor(1.0f, 0.8f, 0.55f), 1000.0f * K);
	// (23/10, a vida) com a direção, no centro da bola (onde o fogo está), não colada na chapa
	LightAt = HitLightAtCm.IsNearlyZero() ? O + N * (0.6f * K) : ToM(HitLightAtCm);
	LightPeak = 7.0e5f * (K * K);
	LightTau = 0.12f * K;
	if (FireLightTauS > 0.0f)
	{
		LightHold = FMath::Max(FireLightHoldS, 0.0f);
		LightTau = FireLightTauS;
		LightPow = 1.4f;
	}
	SetActorTickEnabled(true);
	SetLifeSpan(20.0f);
}

void ABlastDebris::StartHullGlints(const TArray<FVector>& PointsCm, const TArray<FVector>& NormalsW, int32 Seed)
{
	// (23/10, a vida; a camada Glints) os estilhaços batendo no aço: o clarão de impacto dura microssegundos (JHU), aqui 1–2 quadros, em
	// pontos de 0,15–0,3 m nas faces das peças viradas pra bola. Branco-amarelo, redondo, sem luz (são pequenos demais pra acender o chão);
	// um quadro de defasagem sorteado, pra não piscarem todos juntos
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	for (int32 i = 0; i < PointsCm.Num(); ++i)
	{
		const FVector Nn = NormalsW.IsValidIndex(i) ? NormalsW[i].GetSafeNormal() : FVector::UpVector;
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = ToM(PointsCm[i]) + Nn * 0.03f;
		S.Age = -Rand.FRandRange(0.0f, 1.0f / 30.0f);
		S.Life = Rand.FRandRange(1.0f, 2.0f) / 30.0f;
		S.Size = Rand.FRandRange(0.075f, 0.15f);
		S.Bright = Rand.FRandRange(0.7f, 1.0f);
		S.Color = FLinearColor(1.0f, 0.93f, 0.78f, 1.0f);
		S.bRound = true;
		S.bPinned = true;
	}
	BuildPlateSection();
	SetActorTickEnabled(true);
	SetLifeSpan(2.0f);
}

void ABlastDebris::Landed(const FVector& PosM, float SizeM, uint8 Kind, bool bHot, const FVector& VelMs)
{
	// (23/10, a vida) o pedaço da ogiva que caiu queimando fica 2–4 s piscando no chão, sem luz (a fagulha presa); o diretor recebe o pouso
	// (o jogo esvazia a fila: o respingo de areia pela energia da batida, VelMs, e, se ainda queimava, o fiapo de 3–4 s)
	if (bHot)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.bEmber = true;
		S.bPinned = true;
		S.P = PosM + FVector(0.0f, 0.0f, 0.04f);
		S.Life = Rand.FRandRange(2.0f, 4.0f);
		S.Size = Rand.FRandRange(0.06f, 0.1f);
		S.Bright = Rand.FRandRange(0.15f, 0.3f);
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
		bGlowsDone = false;
	}
	if (UBlastFxDirector* Director = UBlastFxDirector::Get(this))
	{
		BlastFxHitLife::FLanding L;
		L.PosCm = ToCm(PosM);
		L.SizeM = SizeM;
		L.Kind = Kind;
		L.bHot = bHot;
		L.VelMs = VelMs;
		Director->PushLanding(L);
	}
}

void ABlastDebris::StartLaser(const FVector& FromCm, const FVector& ToCm, float LifeS)
{
	Rand.Initialize(1);
	BallScale = 1.0f;
	BuildShapes();
	const FVector A = ToM(FromCm), B = ToM(ToCm);
	const float L = float(FVector::Dist(A, B));
	// o risco vermelho fino de A a B: a faísca presa no meio, com a velocidade que estica o borrão até as pontas
	FSpark& S = Sparks.AddDefaulted_GetRef();
	S.P = 0.5f * (A + B);
	S.V = (B - A).GetSafeNormal() * (L * 30.0f);
	S.StreakT = 1.0f / 30.0f;
	S.Life = LifeS;
	S.Size = 0.02f;
	S.Bright = 0.5f;
	S.Color = FLinearColor(1.0f, 0.08f, 0.03f, 1.0f);
	S.MinPxK = 0.0004f;
	S.bPinned = true;
	SetActorTickEnabled(true);
	SetLifeSpan(LifeS + 1.0f);
}

void ABlastDebris::StartGlass(const FVector& CenterCm, const FVector& OutDir, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(CenterCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector Out = OutDir.GetSafeNormal();
	// os cacos que piscam: 120 por janela no voo (0,4–0,9 s), 1–2 m/s pra fora e pra baixo, a normal sorteada girando 6–20
	// rad/s; no mínimo ~2 px; o branco do sol
	for (int32 i = 0; i < 120; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + FVector(Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.2f, 0.2f));
		S.V = (Out * Rand.FRandRange(0.5f, 2.0f) + Rand.VRand() * 0.6f) + FVector(0.0f, 0.0f, Rand.FRandRange(0.0f, 1.0f));
		S.Life = Rand.FRandRange(0.4f, 0.9f);
		S.Size = 0.01f;
		S.MinPxK = 0.0006f;
		S.Bright = 1.0f;
		S.Color = FLinearColor(0.95f, 0.98f, 1.0f, 1.0f);
		S.bRound = true;
		S.bGlint = true;
		S.GlintN = Rand.VRand();
		S.GlintSpin = Rand.VRand() * Rand.FRandRange(6.0f, 20.0f);
		S.Drag = G / FMath::Square(8.0f);
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// os blocos de cubos ainda grudados (20–40 de 3–10 cm, 1–2,5 m/s), verde-claros e lisos
	for (int32 i = 0; i < 24; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + FVector(Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.4f, 0.4f), Rand.FRandRange(-0.2f, 0.2f));
		P.V = Out * Rand.FRandRange(1.0f, 2.5f) + Rand.VRand() * 0.5f;
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(4.0f, 12.0f);
		P.Size = Rand.FRandRange(0.03f, 0.1f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	PlateTint = FLinearColor(0.55f, 0.66f, 0.64f, 1.0f);
	PlateRoughNow = 0.08f;
	BuildPlateSection();
	SetActorTickEnabled(true);
	SetLifeSpan(15.0f);
}

void ABlastDebris::StartChips(const FVector& Cm, const FVector& Dir, int32 Count, float SizeMin, float SizeMax, float SpeedMin, float SpeedMax, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(Cm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	const FVector Axis = (Dir.GetSafeNormal() + FVector(0.0f, 0.0f, 0.6f)).GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		P.P = O + Rand.VRand() * 0.15f;
		P.V = Rand.VRandCone(Axis, FMath::DegreesToRadians(45.0f)) * Rand.FRandRange(SpeedMin, SpeedMax);
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(6.0f, 18.0f);
		P.Size = Rand.FRandRange(SizeMin, SizeMax);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
	}
	BuildPlateSection();
	SetActorTickEnabled(true);
	SetLifeSpan(15.0f);
}

void ABlastDebris::StartScrape(const FVector& Cm, const FVector& Dir, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(Cm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	// aço contra aço dá pouca faísca abaixo de ~6 m/s (van Wingerden 2019): 5–12 de 0,1–0,3 s, laranja, fracas de dia
	const int32 Count = 5 + Rand.RandRange(0, 7);
	for (int32 i = 0; i < Count; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = O + Rand.VRand() * 0.3f;
		S.V = Rand.VRandCone((Dir.GetSafeNormal() + FVector(0.0f, 0.0f, 0.3f)).GetSafeNormal(), FMath::DegreesToRadians(35.0f)) * Rand.FRandRange(4.0f, 9.0f);
		S.Life = Rand.FRandRange(0.1f, 0.3f);
		S.Size = Rand.FRandRange(0.01f, 0.018f);
		S.MinPxK = 0.0004f;
		S.Bright = Rand.FRandRange(0.3f, 0.6f);
		S.bCool = true;
		S.Color = FLinearColor(1.0f, 0.7f, 0.35f, 1.0f);
		S.StreakT = 1.0f / 20.0f;
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	SetActorTickEnabled(true);
	SetLifeSpan(5.0f);
}

void ABlastDebris::StartBlink(const FVector& Cm, float DurationS)
{
	EnsureLight(FLinearColor(1.0f, 0.55f, 0.1f), 250.0f);
	LightAt = ToM(Cm);
	LightPeak = 3.0e3f;
	BlinkUntil = T + DurationS;
	SetActorTickEnabled(true);
	SetLifeSpan(DurationS + 1.0f);
}

FVector ABlastDebris::ArcGroundPoint(const FArc& Ac)
{
	const float A = Rand.FRandRange(0.0f, 2.0f * PI), R = Ac.JumpR * Rand.FRandRange(0.5f, 1.0f);
	return FVector(Ac.Anchor.X + R * FMath::Cos(A), Ac.Anchor.Y + R * FMath::Sin(A), Ac.GroundZ + 0.02f);
}

void ABlastDebris::StartArc(const FVector& PointCm, int32 Kind, int32 Seed)
{
	Rand.Initialize(Seed);
	BallScale = 1.0f;
	BuildShapes();
	const FVector O = ToM(PointCm);
	const float Ground = float(BlastFxGround::HeightAtCached(GetWorld(), PointCm.X, PointCm.Y) / 100.0);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = Ground;
	const bool bAir = Kind == 0, bTimed = Kind == 2;
	const FVector Anchor = bAir ? O : FVector(O.X, O.Y, FMath::Max(float(O.Z), Ground + 0.05f));
	// os arcos: no ar, 1–2 entre as pontas (0,1–0,6 m); no chão, da ponta do fio até a areia (o rápido: 1–2 de 0,2–0,6 m;
	// o temporizado: 2 que pulam, 0,3–1,5 m). O disparo rápido do religador corta em ~46 ms: 3–4 quadros
	const int32 Count = bTimed ? 2 : 1 + Rand.RandRange(0, 1);
	float LongestLife = 0.0f;
	for (int32 i = 0; i < Count; ++i)
	{
		FArc& Ac = Arcs.AddDefaulted_GetRef();
		Ac.Seed = Rand.RandRange(0, 1 << 20);
		Ac.Anchor = Anchor;
		Ac.GroundZ = Ground;
		Ac.Amp = Rand.FRandRange(0.12f, 0.2f);
		Ac.Halo = Rand.FRandRange(0.12f, 0.2f);
		if (bAir)
		{
			const FVector D = Rand.VRand();
			const float L = Rand.FRandRange(0.1f, 0.6f);
			Ac.A = O - D * (0.5f * L);
			Ac.B = O + D * (0.5f * L);
			Ac.Life = Rand.FRandRange(0.05f, 0.07f);
		}
		else
		{
			Ac.A = Anchor;
			Ac.JumpR = bTimed ? Rand.FRandRange(0.3f, 1.5f) : Rand.FRandRange(0.2f, 0.6f);
			Ac.B = ArcGroundPoint(Ac);
			Ac.Life = bTimed ? Rand.FRandRange(0.6f, 1.0f) : Rand.FRandRange(0.05f, 0.07f);
			Ac.NextJump = bTimed ? Rand.FRandRange(0.1f, 0.2f) : 1.0e9f;
		}
		LongestLife = FMath::Max(LongestLife, Ac.Life);
	}
	// as gotas de metal: alumínio de ~1,5 mm queimando e cobre fundido, branco-amarelas esfriando pro laranja, gravidade
	// cheia, quicam. O rápido: 40–80 de uma vez; o temporizado: ~30/s pela vida, nascendo nos pés do arco
	const int32 Drops = bTimed ? FMath::RoundToInt(30.0f * LongestLife) : 40 + Rand.RandRange(0, 40);
	for (int32 i = 0; i < Drops; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		if (bTimed)
		{
			const float A = Rand.FRandRange(0.0f, 2.0f * PI), R = Rand.FRandRange(0.0f, 1.0f);
			S.P = FVector(Anchor.X + R * FMath::Cos(A), Anchor.Y + R * FMath::Sin(A), Ground + 0.05f);
			S.Age = -Rand.FRandRange(0.0f, LongestLife);
			S.V = Rand.VRandCone(FVector::UpVector, FMath::DegreesToRadians(60.0f)) * Rand.FRandRange(2.0f, 5.0f);
		}
		else
		{
			S.P = Anchor + Rand.VRand() * 0.1f;
			const FVector Axis = bAir ? FVector(0.0f, 0.0f, 0.3f) + Rand.VRand() : FVector::UpVector;
			S.V = (bAir ? Axis.GetSafeNormal() : Rand.VRandCone(Axis, FMath::DegreesToRadians(70.0f))) * Rand.FRandRange(3.0f, 8.0f);
		}
		S.Life = Rand.FRandRange(0.6f, 1.5f);
		S.Size = 0.008f;
		S.MinPxK = 0.0005f;
		S.Bright = Rand.FRandRange(0.6f, 1.0f);
		// (17/10, o Felipe: "uns toques de azuis igual um choque") ~40% das faíscas azul-brancas (o arco aceso nelas), o resto o
		// metal derretido
		S.Color = Rand.FRand() < 0.4f ? FLinearColor(0.6f, 0.8f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.5f, 1.0f);
		S.bCool = true;
		S.bBounce = true;
		S.Drag = G / FMath::Square(14.0f);
		S.StreakT = 1.0f / 30.0f;
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}
	// a ponta do fio incandescente por ~0,8 s depois de cada arco
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = Anchor;
		S.V = FVector(0.0f, 0.0f, 0.01f);
		S.Life = LongestLife + 0.8f;
		S.Size = 0.025f;
		S.Bright = 0.5f;
		S.Color = FLinearColor(1.0f, 0.55f, 0.2f, 1.0f);
		S.bRound = true;
		S.bCool = true;
		S.bPinned = true;
	}
	// as lascas do isolador (porcelana branca) quando o fio arrebenta no ar
	if (bAir)
	{
		for (int32 i = 0; i < 8; ++i)
		{
			FPlate& P = Plates.AddDefaulted_GetRef();
			P.P = O + Rand.VRand() * 0.15f;
			P.V = Rand.VRand() * Rand.FRandRange(2.0f, 5.0f);
			P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
			P.Spin = Rand.VRand() * Rand.FRandRange(6.0f, 18.0f);
			P.Size = Rand.FRandRange(0.03f, 0.08f);
			P.Shape = Rand.RandRange(0, NumShapes - 1);
		}
		BuildPlateSection();
	}
	// a luz: azul-branca, no clarão 0,5–1× o sol a 3 m por 1–2 quadros; no temporizado 0,15–0,4× piscando
	EnsureLight(FLinearColor(0.5f, 0.68f, 1.0f), 1000.0f);   // (17/10) mais azul, o choque
	LightAt = Anchor;
	if (bTimed)
	{
		LightPeak = 2.5e5f;
		ArcLightUntil = T + LongestLife;
	}
	else
	{
		LightPeak = 6.0e5f;
		LightTau = 0.03f;
	}
	SetActorTickEnabled(true);
	SetLifeSpan(6.0f);
}

void ABlastDebris::StartWreckFire(float DurationS, bool bFlames)
{
	bWreckFlames = bFlames;
	if (!Shapes.Num())
	{
		Rand.Initialize(int32(GetUniqueID()));
		BuildShapes();
	}
	FireUntil = T + DurationS;
	FireAcc = 0.0f;
	EnsureLight(FLinearColor(1.0f, 0.5f, 0.18f), 700.0f);
	LightPeak = 1.5e5f;
	LightTau = 0.3f;
	bGlowsDone = false;
	SetActorTickEnabled(true);
	SetLifeSpan(0.0f);
}

void ABlastDebris::UpdateWreckFire(float Dt)
{
	if (T >= FireUntil || !bWreckFlames)
	{
		return;
	}
	// ~9 chamas por segundo, menos no fim; cada uma sobe 1–2 m/s, cresce e some em 0,5–0,8 s (as línguas de fogo
	// saindo do anel da torre e da escotilha)
	const float Fade = FMath::Clamp((FireUntil - T) / 3.0f, 0.0f, 1.0f);
	FireAcc += Dt * 9.0f * Fade;
	while (FireAcc >= 1.0f)
	{
		FireAcc -= 1.0f;
		int32 Slot = INDEX_NONE;
		for (const int32 K : FireSlots)
		{
			if (Sparks.IsValidIndex(K) && Sparks[K].Age > Sparks[K].Life)
			{
				Slot = K;
				break;
			}
		}
		if (Slot == INDEX_NONE)
		{
			if (FireSlots.Num() >= 14)
			{
				continue;
			}
			Slot = Sparks.AddDefaulted();
			FireSlots.Add(Slot);
		}
		FSpark& S = Sparks[Slot];
		S = FSpark();
		S.P = FirePos + FVector(Rand.FRandRange(-0.45f, 0.45f), Rand.FRandRange(-0.45f, 0.45f), Rand.FRandRange(0.0f, 0.2f));
		S.V = FVector(Rand.FRandRange(-0.3f, 0.3f), Rand.FRandRange(-0.3f, 0.3f), Rand.FRandRange(1.0f, 2.0f));
		S.Size = Rand.FRandRange(0.22f, 0.38f);
		S.Grow = 0.5f;
		S.Life = Rand.FRandRange(0.5f, 0.8f);
		S.Bright = Rand.FRandRange(0.08f, 0.15f);    // laranja sobre o casco escuro (na referência; sem compensar, 0,05 já era branco)
		S.Lift = 1.0f;
		S.bRound = true;
		S.Color = FLinearColor(1.0f, 0.38f, 0.08f, 1.0f);
	}
}

void ABlastDebris::StartTracer(const FVector& FromCm, const FVector& ToCm, float SpeedCmPerS, int32 Seed)
{
	Rand.Initialize(Seed);
	BuildShapes();
	const FVector A = ToM(FromCm), B = ToM(ToCm);
	const float Dist = float(FVector::Dist(A, B));
	const float Speed = FMath::Max(SpeedCmPerS * 0.01f, 1.0f);
	FComet& C = Comets.AddDefaulted_GetRef();
	C.P = A;
	C.V = (B - A) / FMath::Max(Dist / Speed, 0.01f);
	C.Burn = Dist / Speed;
	C.bStraight = true;
	C.HeadM = 0.35f;
	// o traçante laranja curto dos game shots (16/10, fx_disparo_v6: a cabeça a (1; 0,55; 0,25) somada à areia saía um
	// ponto branco, e o rastro aceso de 0,12 s a 105 m/s era uma linha branca de ~12 m)
	C.Color = FLinearColor(0.6f, 0.24f, 0.05f, 1.0f);
	// o risco: uma faísca na mesma velocidade, sem gravidade nem arrasto, esticada pelo borrão de 1/30 s (≈ 3,5 m a
	// 105 m/s, o que os jogos usam: 1/60–1/30 s de voo) e com ≥ 3 px de espessura; um rastro de 0,3 s a 105 m/s seria
	// uma linha de 30 m ("raio laser", reprovado)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.P = A;
		S.V = C.V;
		S.Life = C.Burn;
		S.Size = 0.06f;
		S.Bright = 0.4f;                            // o risco laranja saturado ~1,5–2× a areia na tela (a 0,15 e (1; 0,62; 0,3) era creme)
		S.Lift = 1.0f;                              // sem gravidade
		S.MinPxK = 0.00075f;
		S.Color = FLinearColor(1.0f, 0.4f, 0.1f, 1.0f);
		S.bTrail = true;
	}
	// a fumacinha do traçante: curta e fraca, só o rastro quente logo atrás
	C.Trail = NewTrail(0.08f, 0.3f, 0.08f, 0.6f, 0.0f, 0.12f);   // sem fogo no rastro (era a linha branca)
	EnsureLight(FLinearColor(1.0f, 0.6f, 0.3f), 700.0f);
	LightFollow = 0;
	LightAt = B;
	LightPeak = 2.0e5f;
	LightTau = 1.0f;
	SetLifeSpan(5.0f);
}

float ABlastDebris::SparkScaleNow() const
{
	return BallScale > 0.0f ? FMath::Clamp(BallScale, 0.2f, 1.0f) : SparkScaleCfg();
}

int32 ABlastDebris::FollowComponent(USceneComponent* Comp, float Width0M, float GrowthMps, float LifeS, float Albedo, float GlowS, float Opacity0)
{
	if (!Comp)
	{
		return -1;
	}
	FFollow& F = Follows.AddDefaulted_GetRef();
	F.Trail = NewTrail(Width0M, GrowthMps, LifeS, Albedo, GlowS, Opacity0);
	F.Comp = Comp;
	F.Last = Comp->GetComponentLocation();
	SetTrailHead(F.Trail, F.Last, false);
	return F.Trail;
}

void ABlastDebris::UpdateFollows(float Dt)
{
	// a torre do jogo: a fumaça sai enquanto ela anda a mais de 1 m/s (o voo; o jogo pode arremessá-la um pouco
	// depois de chamar a morte) e para de vez depois de 0,3 s parada (assentou) ou aos 6 s: a carcaça empurrada
	// depois não solta rastro de novo
	for (FFollow& F : Follows)
	{
		const USceneComponent* C = F.Comp.Get();
		if (F.bDone || !C)
		{
			if (!F.bDone)
			{
				F.bDone = true;
				SetTrailHead(F.Trail, F.Last, false);
			}
			continue;
		}
		const FVector P = C->GetComponentLocation();
		const bool bMoving = FVector::Dist(P, F.Last) > 100.0 * Dt;
		F.Last = P;
		F.bFlew |= bMoving;
		F.Still = bMoving ? 0.0f : F.Still + Dt;
		F.bDone = (F.bFlew && F.Still > 0.3f) || T > 6.0f;
		SetTrailHead(F.Trail, P, bMoving && !F.bDone);
	}
}

void ABlastDebris::SetTrailHead(int32 Trail, const FVector& HeadCm, bool bEmitting)
{
	if (Trails.IsValidIndex(Trail))
	{
		Trails[Trail].Head = ToM(HeadCm);
		Trails[Trail].bEmitting = bEmitting;
		if (bEmitting && bTrailsDone)
		{
			bTrailsDone = false;                    // a fumaça do ferido volta a soltar: o tique religa
			SetActorTickEnabled(true);
		}
	}
}

void ABlastDebris::Start(const FVector& OriginCm, int32 Seed)
{
	Rand.Initialize(Seed);
	// Os presets menores (14/10; a sessão do jogo: o Média roda num notebook de 2014): o custo dos destroços é de
	// processador (~0,6 ms por morte com 11 juntas, perfil CSV), então a placa não o corta. No Média (EffectsQuality 2)
	// metade das chapas, cometas, espinhos e fagulhas; no Baixa (≤ 1) um quarto; Alta e Ultra inteiros. Decidido aqui, no
	// nascimento: nada some no meio. A silhueta fica (a bola, a torre e o rastro dela).
	static const IConsoleVariable* EffectsQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.EffectsQuality"));
	const int32 Eq = EffectsQuality ? EffectsQuality->GetInt() : 3;
	const float Keep = (Eq >= 3 ? 1.0f : (Eq == 2 ? 0.5f : 0.25f)) * FMath::Clamp(DetailK, 0.1f, 1.0f);
	const int32 PlateCount = FMath::Max(4, FMath::RoundToInt(NumPlates * Keep));
	const int32 CometCount = FMath::Max(2, FMath::RoundToInt(NumComets * Keep));
	const int32 EmberCount = FMath::RoundToInt(NumEmbers * Keep);
	BuildShapes();
	const FVector O = ToM(OriginCm);
	OriginM = O;
	bHasOrigin = true;
	GroundRefM = -1.0e9f;
	for (int32 i = 0; i < PlateCount; ++i)
	{
		FPlate& P = Plates.AddDefaulted_GetRef();
		const float Az = Rand.FRandRange(0.0f, 2.0f * PI);
		const float El = FMath::DegreesToRadians(Rand.FRandRange(25.0f, 80.0f));
		const float Speed = Rand.FRandRange(8.0f, 22.0f);
		P.P = O + FVector(Rand.FRandRange(-1.5f, 1.5f), Rand.FRandRange(-1.0f, 1.0f), Rand.FRandRange(-0.3f, 0.5f));
		P.V = FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El)) * Speed;
		P.Q = FQuat(Rand.VRand(), Rand.FRandRange(0.0f, 2.0f * PI));
		P.Spin = Rand.VRand() * Rand.FRandRange(6.0f, 20.0f);
		P.Size = Rand.FRandRange(0.4f, 1.4f);
		P.Shape = Rand.RandRange(0, NumShapes - 1);
		// um terço solta um fio de fumaça escura (estava em brasa ou com óleo). 13/10: 6 cm e albedo 0,12 eram fios pretos
		// de arame dentro da nuvem marrom; agora nasce com 15 cm, engorda 0,5 m/s e é fuligem (0,3)
		if (Rand.FRand() < 0.33f)
		{
			P.Trail = NewTrail(0.15f, 0.5f, 0.8f, 0.3f, 0.0f, 0.45f);
		}
	}
	for (int32 i = 0; i < CometCount; ++i)
	{
		FComet& C = Comets.AddDefaulted_GetRef();
		const float Az = (i + Rand.FRandRange(-0.35f, 0.35f)) / CometCount * 2.0f * PI;   // espalhados em volta
		const float El = FMath::DegreesToRadians(Rand.FRandRange(15.0f, 65.0f));
		const float Speed = Rand.FRandRange(20.0f, 35.0f);
		C.P = O + FVector(0.0f, 0.0f, Rand.FRandRange(0.0f, 0.6f));
		C.V = FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El)) * Speed;
		C.Burn = Rand.FRandRange(1.0f, 2.5f);
		// rastro claro (propelente: óxidos e vapor, albedo ~0.6), fino na cabeça.
		// No vídeo ele se vê ~6–10 m atrás da cabeça (~0.3–0.5 s a 20–30 m/s) e se
		// desfaz: vida 0.9 s, engorda 0.4 m/s, opacidade ≤ 0.6 (a 1ª versão, 2.6 s
		// e opaca, virou tentáculos de 40 m). Mais grosso desde 13/10 (0,12 m → 0,35 m, engorda 1,2 m/s): nos alvos
		// 3/19 os rastros dos destroços são fumaça de ~0,5–1 m, e com 12 cm eram riscos de laser
		C.Trail = NewTrail(0.35f, 1.2f, 0.9f, 0.6f, 0.0f, 0.5f);
	}
	// Espinhos: a estrela de riscos do vídeo (0.05–0.5 s), dezenas de pedaços em
	// brasa a 40–90 m/s que o olho integra em riscos; pequenos, freiam rápido
	// (terminal ~20 m/s) e apagam em 0.12–0.35 s. Fagulhas: as que caem piscando
	// em volta depois (1–3 s), lentas e leves (terminal ~6 m/s, meio empuxo).
	// Meta do painel da sessão de arte: 40–80 espinhos + 100–200 fagulhas.
	// Com a bola menor (-BlastScale, 13/10: a morte de tanque medida contra os alvos é 0,35–0,5 da gravação) os espinhos
	// encolhem junto: a estrela de riscos brancos de 3 m feita pra bola de 9 m virava "palitos" em volta da bola de 4 m;
	// e mais laranja, como as faíscas da referência da torre voando
	const float SparkScale = SparkScaleNow();
	const int32 Spikes = FMath::RoundToInt(NumSpikes * SparkScale * Keep);
	for (int32 i = 0; i < Spikes + EmberCount; ++i)
	{
		FSpark& S = Sparks.AddDefaulted_GetRef();
		S.bEmber = i >= Spikes;
		const float Az = Rand.FRandRange(0.0f, 2.0f * PI);
		const float El = FMath::DegreesToRadians(S.bEmber ? Rand.FRandRange(20.0f, 85.0f) : Rand.FRandRange(3.0f, 80.0f));
		const float Speed = S.bEmber ? Rand.FRandRange(4.0f, 18.0f) : Rand.FRandRange(40.0f, 90.0f) * SparkScale;
		S.P = O + FVector(Rand.FRandRange(-1.0f, 1.0f), Rand.FRandRange(-0.6f, 0.6f), Rand.FRandRange(-0.3f, 0.8f));
		S.V = FVector(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El)) * Speed;
		S.Life = S.bEmber ? Rand.FRandRange(1.0f, 3.0f) : Rand.FRandRange(0.12f, 0.35f);
		S.Size = S.bEmber ? Rand.FRandRange(0.05f, 0.1f) : Rand.FRandRange(0.06f, 0.12f);
		S.Bright = S.bEmber ? Rand.FRandRange(0.06f, 0.18f) : Rand.FRandRange(0.6f, 1.0f);
		S.Drag = G / FMath::Square(S.bEmber ? 6.0f : 20.0f);
		S.Lift = S.bEmber ? 0.5f : 0.0f;
		S.Age = S.bEmber ? -Rand.FRandRange(0.0f, 0.3f) : 0.0f;     // as fagulhas saem nos primeiros 0.3 s
		S.Seed = Rand.FRandRange(0.0f, 100.0f);
	}

	BuildPlateSection();
	SetLifeSpan(120.0f);
}

void ABlastDebris::BuildPlateSection()
{
	// as malhas: as chapas têm topologia fixa (criadas uma vez, só os vértices mudam)
	if (!Plates.Num())
	{
		bPlatesSettled = true;
		return;
	}
	TArray<FVector> V, N;
	TArray<int32> Tri;
	for (const FPlate& P : Plates)
	{
		const FShape& S = Shapes[P.Shape];
		const int32 Base = V.Num();
		V.Append(S.Verts);
		N.Append(S.Normals);
		for (int32 Ix : S.Tris)
		{
			Tri.Add(Base + Ix);
		}
	}
	PlateVerts = V.Num();
	PlateMesh->CreateMeshSection_LinearColor(0, V, Tri, N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	bPlateSection = true;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, BasePath))
	{
		PlateMesh->SetMaterial(0, Base);
	}
	const FLinearColor Pc = PlateTint.R >= 0.0f ? PlateTint : PlateColor;
	PlateMesh->SetCustomPrimitiveDataVector4(0, FVector4(Pc.R, Pc.G, Pc.B, PlateRoughNow >= 0.0f ? PlateRoughNow : PlateRough));
	PlateMesh->SetCustomPrimitiveDataFloat(4, PlateTint.R >= 0.0f ? 0.0f : PlateMetal);
	UpdatePlates();
}

void ABlastDebris::Integrate(float Dt)
{
	const UWorld* W = GetWorld();
	if (bHasOrigin && GroundRefM < -1.0e8f)
	{
		GroundRefM = float(BlastFxGround::HeightAtCached(W, OriginM.X * 100.0, OriginM.Y * 100.0) / 100.0);
	}
	auto FloorAt = [this, W](const FVector& P) -> float
	{
		return bHasOrigin && FVector::DistSquared2D(P, OriginM) < 36.0 ? GroundRefM
			: float(BlastFxGround::HeightAtCached(W, P.X * 100.0, P.Y * 100.0) / 100.0);
	};
	for (FArc& Ac : Arcs)
	{
		Ac.Age += Dt;
		if (Ac.Age <= Ac.Life && Ac.Age >= Ac.NextJump)
		{
			// o arco pula pela areia (o pé queima o ponto e acha outro): a cada 0,1–0,2 s
			Ac.B = ArcGroundPoint(Ac);
			Ac.NextJump += Rand.FRandRange(0.1f, 0.2f);
		}
	}
	for (FPlate& P : Plates)
	{
		if (P.bRest)
		{
			continue;
		}
		P.V += (FVector(0.0f, 0.0f, -G) - PlateDrag * P.V.Size() * P.V) * Dt;
		P.P += P.V * Dt;
		const float SpinRad = P.Spin.Size() * Dt;
		if (SpinRad > 0.0f)
		{
			P.Q = FQuat(P.Spin.GetSafeNormal(), SpinRad) * P.Q;
		}
		const float Floor = FloorAt(P.P) + PlateThickM;
		if (P.P.Z < Floor && P.V.Z < 0.0f)
		{
			P.P.Z = Floor;
			if (P.Land == 1)
			{
				// (23/10, a areia do acerto) o torrão grande levanta o respingo de areia no 1º toque, com a energia da batida (antes no
				// repouso, já sem energia: o puff saía do tamanho do torrão parado)
				P.Land = 0;
				Landed(P.P, P.Size, 1, false, P.V);
			}
			if (++P.Bounces > 1 || P.V.Size() < 4.0f)
			{
				// deitada no chão, com a guinada que tinha (não fica de pé enterrada)
				const float Yaw = P.Q.Rotator().Yaw;
				P.Q = FRotator(Rand.FRandRange(-6.0f, 6.0f), Yaw, Rand.FRandRange(-6.0f, 6.0f)).Quaternion();
				P.bRest = true;
			}
			else
			{
				P.V = FVector(P.V.X * 0.45f, P.V.Y * 0.45f, -P.V.Z * 0.25f);
				P.Spin *= 0.4f;
			}
		}
		if (Trails.IsValidIndex(P.Trail))
		{
			SetTrailHead(P.Trail, ToCm(P.P), !P.bRest && T < 1.4f);
		}
	}
	UBlastFxDirector* SmokeTo = nullptr;
	for (FComet& C : Comets)
	{
		C.Age += Dt;
		const bool bBurning = C.Age < C.Burn;
		if (bBurning && C.bStraight)
		{
			C.P += C.V * Dt;
		}
		else if (bBurning || C.Land == 1)
		{
			// (23/10, a vida) o pedaço em brasa da ogiva (Land 1) segue caindo depois de apagar, até pousar
			C.V += (FVector(0.0f, 0.0f, -G) - CometDrag * C.V.Size() * C.V) * Dt;
			C.P += C.V * Dt;
			const float Floor = FloorAt(C.P) + 0.05f;
			if (C.P.Z < Floor)
			{
				C.P.Z = Floor;
				if (C.Land == 1)
				{
					// (23/10, a vida) em vez de apagar ao bater: a brasa fica piscando no chão e o pouso vai pra fila do diretor
					C.Land = 2;
					Landed(C.P, 0.3f, 0, bBurning, C.V);
					C.Burn = FMath::Min(C.Burn, C.Age);
				}
				else
				{
					C.Burn = FMath::Min(C.Burn, C.Age + 0.15f);      // apaga ao bater na areia
				}
				C.V = FVector::ZeroVector;
			}
		}
		SetTrailHead(C.Trail, ToCm(C.P), bBurning);
		// (a revisão da vida) o pedaço em brasa que ainda voa queimando: o ponto pro fio de fumaça em volume do jogo (parou de queimar ou
		// pousou: some da lista e o fio fica envelhecendo)
		if (C.SmokeId != 0 && C.Land == 1 && C.Age < C.Burn)
		{
			SmokeTo = SmokeTo ? SmokeTo : UBlastFxDirector::Get(this);
			if (SmokeTo)
			{
				SmokeTo->PushEmberSmoke(C.SmokeId, ToCm(C.P));
			}
		}
	}
	for (FSpark& S : Sparks)
	{
		S.Age += Dt;
		if (S.Age < 0.0f || S.Age > S.Life)
		{
			continue;
		}
		if (S.bGlint && !S.GlintSpin.IsNearlyZero())
		{
			S.GlintN = FQuat(S.GlintSpin.GetSafeNormal(), S.GlintSpin.Size() * Dt).RotateVector(S.GlintN);
		}
		if (S.bPinned)
		{
			continue;                               // a chama e o laser: o ponto vem de fora, a velocidade só desenha o risco
		}
		S.V += (FVector(0.0f, 0.0f, -G * (1.0f - S.Lift)) - S.Drag * S.V.Size() * S.V) * Dt;
		S.P += S.V * Dt;
		const float Floor = FloorAt(S.P) + 0.03f;
		if (S.P.Z < Floor)
		{
			S.P.Z = Floor;
			if (S.bBounce && S.V.Z < 0.0f && S.V.Size() > 4.0f)
			{
				// o quique do metal quente na areia (a Epic: quique 0,4, atrito ~0,2): corre rasante e perde força; e
				// esfria: no chão é uma fagulha laranja, não um grão branco (16/10)
				S.V = FVector(S.V.X * 0.75f, S.V.Y * 0.75f, -S.V.Z * 0.4f);
				S.Bright *= 0.4f;
			}
			else
			{
				// no chão: a fagulha fica e apaga em ~0.3 s; o espinho morre
				S.V = FVector::ZeroVector;
				if (S.bGlint)
				{
					// o caco de vidro fica no chão ~2,5 s (deitado, só pisca quando o reflexo bate na câmera)
					if (!S.GlintSpin.IsZero())
					{
						S.GlintSpin = FVector::ZeroVector;
						S.Life = S.Age + 2.5f;
					}
				}
				else
				{
					S.Life = FMath::Min(S.Life, S.Age + (S.bEmber ? 0.3f : 0.0f));
				}
			}
		}
	}
	for (FTrail& Tr : Trails)
	{
		Tr.Age += Dt;
		for (FTrailPoint& Pt : Tr.Points)
		{
			Pt.Age += Dt;
			Pt.P.Z += (Tr.Rise >= 0.0f ? Tr.Rise : TrailRise) * Dt;
			// o vento empurra mais o que já subiu (rente ao casco o ar é parado): a coluna deita e se abre
			Pt.P += Tr.Wind * (Dt * FMath::Min(Pt.Age / 1.5f, 1.0f));
		}
		Tr.Points.RemoveAll([&Tr](const FTrailPoint& Pt) { return Pt.Age > Tr.Life; });
		if (Tr.bEmitting)
		{
			const bool bFar = !Tr.Points.Num() || FVector::Dist(Tr.Points.Last().P, Tr.Head) > TrailEmitM;
			if (bFar)
			{
				FTrailPoint Pt;
				Pt.P = Tr.Head;
				Pt.Dist = Tr.Points.Num() ? Tr.Points.Last().Dist + float(FVector::Dist(Tr.Points.Last().P, Tr.Head)) : 0.0f;
				Tr.Points.Add(Pt);
			}
		}
	}
}

void ABlastDebris::UpdatePlates()
{
	if (!bPlateSection)
	{
		return;
	}
	TArray<FVector> V, N;
	V.Reserve(PlateVerts);
	N.Reserve(PlateVerts);
	for (const FPlate& P : Plates)
	{
		const FShape& S = Shapes[P.Shape];
		const FVector Scale(P.Size, P.Size, 1.0f);       // a espessura fica
		for (int32 i = 0; i < S.Verts.Num(); ++i)
		{
			V.Add(ToCm(P.P + P.Q.RotateVector(S.Verts[i] * Scale)));
			N.Add(P.Q.RotateVector(S.Normals[i]));
		}
	}
	PlateMesh->UpdateMeshSection_LinearColor(0, V, N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
}

void ABlastDebris::UpdateGlows(const FVector& CamM)
{
	TArray<FVector> V, N;
	TArray<int32> Tri;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	const int32 NumQuads = Comets.Num() + Sparks.Num();
	V.Reserve(4 * NumQuads);
	N.Reserve(4 * NumQuads);
	UV.Reserve(4 * NumQuads);
	Col.Reserve(4 * NumQuads);
	// um quad apagado: os quatro cantos no mesmo ponto, sem cor
	auto Collapsed = [&](const FVector& P)
	{
		for (int32 k = 0; k < 4; ++k)
		{
			V.Add(ToCm(P));
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(0.5f, 0.5f));
			Col.Add(FLinearColor::Transparent);
		}
	};
	for (const FComet& C : Comets)
	{
		if (C.Age >= C.Burn)
		{
			Collapsed(C.P);
			continue;
		}
		// apaga no último terço da queima (o projétil só no fim do voo, quando chega)
		const float Fade = 1.0f - FMath::SmoothStep((C.bStraight ? 0.92f : 0.65f) * C.Burn, C.Burn, C.Age);
		const float HeadM = C.HeadM > 0.0f ? C.HeadM : CometHeadM;
		const FVector ToCam = (CamM - C.P).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(ToCam, FVector::UpVector).GetSafeNormal() * (0.5f * HeadM);
		const FVector Up = FVector::CrossProduct(Right, ToCam).GetSafeNormal() * (0.5f * HeadM);
		for (int32 k = 0; k < 4; ++k)
		{
			const float Sx = (k == 1 || k == 2) ? 1.0f : -1.0f, Sy = k >= 2 ? 1.0f : -1.0f;
			V.Add(ToCm(C.P + Right * Sx + Up * Sy));
			N.Add(ToCam);
			UV.Add(FVector2D(0.5f + 0.5f * Sx, 0.5f + 0.5f * Sy));
			Col.Add((C.Color.A >= 0.0f ? FLinearColor(C.Color.R, C.Color.G, C.Color.B, 1.0f) : FMath::Lerp(EmberColor, CometHeadColor, SparkScaleNow())) * Fade);   // com a bola menor, laranja (a referência da torre voando: pedaços em chamas)            // o brilho absoluto vem do material (Intensity: a cor do vértice é de 8 bits)
		}
	}
	// faíscas: o quad esticado ao longo da velocidade (o borrão de ~1/30 s que o
	// olho e a câmera integram), da largura da faísca; o perfil redondo do
	// material vira um risco
	for (const FSpark& S : Sparks)
	{
		if (S.Age < 0.0f || S.Age > S.Life)
		{
			Collapsed(S.P);
			continue;
		}
		float Glow = S.Bright * (1.0f - FMath::SmoothStep(0.6f * S.Life, S.Life, S.Age));
		if (S.bCool)
		{
			// esfriando, o metal escurece junto (o brilho do corpo negro cai muito mais rápido que a cor muda): o risco
			// branco-amarelo no começo, laranja apagado no fim, como na 4.2 (16/10: brancos do começo ao fim)
			Glow *= 1.0f - 0.75f * FMath::Clamp(S.Age / FMath::Max(S.Life, 0.01f), 0.0f, 1.0f);
		}
		if (S.bGlint)
		{
			// o reflexo do vidro, pela meia-direção entre o sol e a câmera. O real é estreito (1–2°) mas são milhares de
			// cubos; com 120 por janela o lóbulo vai a ~7° (cos^100): ~12% dos cacos que giram cruzam o reflexo, cada um
			// por ~1 quadro (cos^400 dava ~2 piscadas por janela)
			const FVector H = (SunDirOf(GetWorld()) + (CamM - S.P).GetSafeNormal()).GetSafeNormal();
			Glow *= FMath::Pow(FMath::Max(float(FVector::DotProduct(S.GlintN, H)), 0.0f), 100.0f);
		}
		if (S.bEmber)
		{
			// pisca; e no jogo mais fraca que no BlastLab (16/10, a morte de perto: somadas à areia clara do mapa saíam
			// riscos brancos caindo como chuva; na aprovada de 13/10 as fagulhas são pontos laranja). -DebrisEmberGlow= testa
			static const float EmberK = [] { float V = 1.0f; FParse::Value(FCommandLine::Get(), TEXT("DebrisEmberGlow="), V); return V; }();
			Glow *= (0.6f + 0.4f * FMath::Sin(S.Age * 25.0f + S.Seed)) * EmberK;
		}
		const FVector ToCam = (CamM - S.P).GetSafeNormal();
		FVector Along = S.V - ToCam * FVector::DotProduct(S.V, ToCam);   // a velocidade vista da câmera
		const float Streak = 0.5f * Along.Size() * S.StreakT;
		Along = Along.IsNearlyZero() ? FVector::CrossProduct(ToCam, FVector::UpVector).GetSafeNormal() : Along.GetSafeNormal();
		float SizeNow = S.Size + S.Grow * FMath::Max(S.Age, 0.0f);
		if (S.MinPxK > 0.0f)
		{
			// espessura mínima na tela (a câmera do jogo: ~20 px por metro a 100 m, 50° de campo; 0,00075 ≈ 3 px)
			SizeNow = FMath::Max(SizeNow, S.MinPxK * float(FVector::Dist(CamM, S.P)));
		}
		const FVector Across = FVector::CrossProduct(ToCam, Along).GetSafeNormal() * SizeNow;
		const FVector Len = Along * (SizeNow + Streak);
		// o borrão centrado no ponto (as faíscas) ou todo atrás dele (o traçante: centrado, metade do risco ia à frente da
		// cabeça, 16/10)
		const FVector Mid = S.bTrail ? S.P - Along * Streak : S.P;
		for (int32 k = 0; k < 4; ++k)
		{
			const float Sx = (k == 1 || k == 2) ? 1.0f : -1.0f, Sy = k >= 2 ? 1.0f : -1.0f;
			V.Add(ToCm(Mid + Len * Sx + Across * Sy));
			N.Add(ToCam);
			// u fixo no meio: o perfil do material só varia na largura (risco inteiro, não um ponto); o clarão redondo
			// varia nos dois (como a cabeça do cometa)
			UV.Add(S.bRound ? FVector2D(0.5f + 0.5f * Sx, 0.5f + 0.5f * Sy) : FVector2D(0.5f, 0.5f + 0.5f * Sy));
			FLinearColor Base = S.Color.A >= 0.0f ? FLinearColor(S.Color.R, S.Color.G, S.Color.B, 1.0f)
				: (S.bEmber ? EmberColor : FMath::Lerp(EmberColor, SpikeColor, SparkScaleNow()));
			if (S.bCool)
			{
				Base = FMath::Lerp(Base, EmberColor, FMath::Clamp(S.Age / FMath::Max(S.Life, 0.01f), 0.0f, 1.0f));
			}
			Col.Add(Base * FMath::Clamp(Glow, 0.0f, 1.0f));
		}
	}
	// os arcos: a fita em zigue-zague refeita todo quadro (o ponto médio deslocado, 4 níveis = 17 pontos, a amplitude
	// caindo pela metade por nível), o núcleo branco-azulado e o halo azul-violeta (as bandas do nitrogênio) a ~20%, e um
	// ramo de 1–2 quadros às vezes. Quantidade fixa de quads por arco (a seção só é refeita quando muda o número)
	for (const FArc& Ac : Arcs)
	{
		constexpr int32 NPts = 17, NBr = 9;
		const bool bOn = Ac.Age >= 0.0f && Ac.Age <= Ac.Life;
		// o piscar da rede (120 Hz) amostrado pelo quadro: 0,35–1,0, 12% dos quadros apagado, 1 em 8 com o verde do cobre
		FRandomStream Fr(Ac.Seed + int32(T * 240.0f));
		const float Flick = Fr.FRand() < 0.12f ? 0.0f : Fr.FRandRange(0.35f, 1.0f);
		const bool bGreen = Fr.FRand() < 0.125f;
		const bool bBranch = Fr.FRand() < 0.35f;
		FVector Pts[NPts], Br[NBr];
		Pts[0] = Ac.A;
		Pts[NPts - 1] = Ac.B;
		const float Len = float(FVector::Dist(Ac.A, Ac.B));
		const FVector Dir = (Ac.B - Ac.A).GetSafeNormal();
		float Amp = Ac.Amp * Len;
		for (int32 Step = (NPts - 1) / 2; Step >= 1; Step /= 2, Amp *= 0.5f)
		{
			for (int32 i = Step; i < NPts - 1; i += 2 * Step)
			{
				FVector Off = Fr.VRand();
				Off -= Dir * FVector::DotProduct(Off, Dir);
				Pts[i] = 0.5f * (Pts[i - Step] + Pts[i + Step]) + Off * Amp;
			}
		}
		// o ramo: de um ponto do meio, 30% do comprimento, desviado até 60°, 3 níveis
		const FVector BrDir = (Dir + Fr.VRand() * 1.2f).GetSafeNormal();
		Br[0] = Pts[3 + Fr.RandRange(0, 10)];
		Br[NBr - 1] = Br[0] + BrDir * (0.3f * Len);
		Amp = Ac.Amp * 0.3f * Len;
		for (int32 Step = (NBr - 1) / 2; Step >= 1; Step /= 2, Amp *= 0.5f)
		{
			for (int32 i = Step; i < NBr - 1; i += 2 * Step)
			{
				FVector Off = Fr.VRand();
				Off -= BrDir * FVector::DotProduct(Off, BrDir);
				Br[i] = 0.5f * (Br[i - Step] + Br[i + Step]) + Off * Amp;
			}
		}
		// (17/10, o Felipe: "toques de azuis igual um choque") o miolo branco-azulado e o halo azul-violeta mais forte
		const FLinearColor CoreCol = (bGreen ? FLinearColor(0.7f, 1.0f, 0.75f, 1.0f) : FLinearColor(0.72f, 0.86f, 1.0f, 1.0f)) * Flick;
		const FLinearColor HaloCol = FLinearColor(0.35f, 0.45f, 1.0f, 1.0f) * (0.35f * Flick);
		auto Seg = [&](const FVector& P0, const FVector& P1, float Half, const FLinearColor& Cl, bool bShow)
		{
			if (!bShow)
			{
				Collapsed(P0);
				return;
			}
			const FVector Mid = 0.5f * (P0 + P1);
			const FVector ToCam = (CamM - Mid).GetSafeNormal();
			FVector Along = P1 - P0;
			Along -= ToCam * FVector::DotProduct(Along, ToCam);
			Along = Along.IsNearlyZero() ? FVector::CrossProduct(ToCam, FVector::UpVector).GetSafeNormal() : Along.GetSafeNormal();
			// no mínimo ~2 px de largura (a câmera do jogo: 0,0005 × a distância)
			const float W = FMath::Max(Half, 0.0005f * float(FVector::Dist(CamM, Mid)));
			const FVector Across = FVector::CrossProduct(ToCam, Along).GetSafeNormal() * W;
			const FVector Ext = Along * (0.5f * W);     // as juntas se sobrepõem (sem frestas no zigue-zague)
			const FVector Q[4] = { P0 - Ext - Across, P1 + Ext - Across, P1 + Ext + Across, P0 - Ext + Across };
			for (int32 k = 0; k < 4; ++k)
			{
				V.Add(ToCm(Q[k]));
				N.Add(ToCam);
				UV.Add(FVector2D(0.5f, k >= 2 ? 1.0f : 0.0f));
				Col.Add(Cl);
			}
		};
		for (int32 i = 0; i < NPts - 1; ++i)
		{
			Seg(Pts[i], Pts[i + 1], Ac.Halo, HaloCol, bOn);
		}
		for (int32 i = 0; i < NPts - 1; ++i)
		{
			Seg(Pts[i], Pts[i + 1], Ac.Core, CoreCol, bOn);
		}
		for (int32 i = 0; i < NBr - 1; ++i)
		{
			Seg(Br[i], Br[i + 1], 0.6f * Ac.Halo, HaloCol, bOn && bBranch);
		}
		for (int32 i = 0; i < NBr - 1; ++i)
		{
			Seg(Br[i], Br[i + 1], 0.7f * Ac.Core, CoreCol * 0.7f, bOn && bBranch);
		}
	}
	if (V.Num() == GlowVerts)
	{
		// sem a conversão sRGB, como na criação (16/10: o padrão da atualização na Unreal 5.8 é converter, e do 2º quadro
		// em diante as cores saíam clareadas: o laranja 0,5 virava 0,73, faíscas creme; ProceduralMeshComponent.h)
		GlowMesh->UpdateMeshSection_LinearColor(0, V, N, UV, Col, TArray<FProcMeshTangent>(), false);
		return;
	}
	for (int32 q = 0; q < V.Num() / 4; ++q)
	{
		const int32 B = 4 * q;
		Tri.Append({ B, B + 1, B + 2, B, B + 2, B + 3 });
	}
	GlowMesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, TArray<FProcMeshTangent>(), false);
	GlowVerts = V.Num();
}

void ABlastDebris::UpdateTrails(const FVector& CamM)
{
	// fita de 3 vértices na largura (borda, meio, borda) virada pra câmera, com a
	// normal arredondada (o tubo de fumaça pega o sol de um lado). Cor do vértice:
	// r opacidade, g brilho do fogo, b albedo, a semente; UV: (metros ao longo, 0..1)
	TArray<FVector> V, N;
	TArray<int32> Tri;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	const int32 Count = Trails.Num() * TrailSamples * 3;
	V.Reserve(Count);
	N.Reserve(Count);
	UV.Reserve(Count);
	Col.Reserve(Count);
	TArray<FTrailPoint> Pts, Samples;
	for (const FTrail& Tr : Trails)
	{
		// os pontos e a cabeça (a fita encosta na peça que voa)
		Pts = Tr.Points;
		if (Tr.bEmitting && Pts.Num())
		{
			FTrailPoint H;
			H.P = Tr.Head;
			H.Dist = Pts.Last().Dist + float(FVector::Dist(Pts.Last().P, Tr.Head));
			Pts.Add(H);
		}
		if (Pts.Num() < 2)
		{
			// sem fumaça ainda (ou já desfeita): a fita apagada num ponto só
			const FVector At = ToCm(Pts.Num() ? Pts[0].P : Tr.Head);
			for (int32 i = 0; i < 3 * TrailSamples; ++i)
			{
				V.Add(At);
				N.Add(FVector::UpVector);
				UV.Add(FVector2D::ZeroVector);
				Col.Add(FLinearColor::Transparent);
			}
			continue;
		}
		// reamostrado em TrailSamples pontos igualmente espaçados ao longo do comprimento
		Samples.SetNum(TrailSamples);
		const float D0 = Pts[0].Dist, D1 = Pts.Last().Dist;
		int32 j = 0;
		for (int32 s = 0; s < TrailSamples; ++s)
		{
			const float D = FMath::Lerp(D0, D1, float(s) / float(TrailSamples - 1));
			while (j < Pts.Num() - 2 && Pts[j + 1].Dist < D)
			{
				++j;
			}
			const float Span = Pts[j + 1].Dist - Pts[j].Dist;
			const float A = Span > 1e-6f ? FMath::Clamp((D - Pts[j].Dist) / Span, 0.0f, 1.0f) : 0.0f;
			Samples[s].P = FMath::Lerp(Pts[j].P, Pts[j + 1].P, A);
			Samples[s].Age = FMath::Lerp(Pts[j].Age, Pts[j + 1].Age, A);
			Samples[s].Dist = D;
		}
		for (int32 i = 0; i < TrailSamples; ++i)
		{
			const FTrailPoint& Pt = Samples[i];
			const FVector Dir = (Samples[FMath::Min(i + 1, TrailSamples - 1)].P - Samples[FMath::Max(i - 1, 0)].P).GetSafeNormal();
			const FVector ToCam = (CamM - Pt.P).GetSafeNormal();
			FVector Side = FVector::CrossProduct(Dir, ToCam).GetSafeNormal();
			if (Side.IsNearlyZero())
			{
				Side = FVector::CrossProduct(FVector::UpVector, ToCam).GetSafeNormal();
			}
			const float Width = Tr.Width0 + Tr.Growth * Pt.Age;
			// a massa por metro fica: opacidade ~ largura inicial / largura; some no fim da vida. A fumaça parada (o tanque
			// ferido, a carcaça) afina pela raiz (16/10, fx_acerto_v9: a coluna de fonte parada é fumaça cada vez mais velha
			// a cada metro, e com a regra do rastro de peça voando ela sumia em ~1 m; a de cabeça girando aparecia)
			const float Thin = FMath::Clamp((Tr.Width0 + 0.25f) / (Width + 0.25f), 0.0f, 1.0f);
			const float Opacity = Tr.Opacity0 * (Tr.Rise >= 0.0f ? FMath::Sqrt(Thin) : Thin)
				* (1.0f - FMath::SmoothStep(0.55f * Tr.Life, Tr.Life, Pt.Age))
				* FMath::SmoothStep(0.0f, 0.04f, Pt.Age + (i == TrailSamples - 1 ? 0.0f : 0.02f));
			// fogo: o gás logo atrás da peça ainda brilha (some em ~0.25 s de idade)
			const float Glow = Tr.GlowS > 0.0f && Tr.Age < Tr.GlowS ? FMath::Exp(-Pt.Age / FMath::Max(Tr.GlowTau, 0.005f)) * (1.0f - FMath::SmoothStep(0.6f * Tr.GlowS, Tr.GlowS, Tr.Age)) : 0.0f;
			const FLinearColor C(Opacity, Glow, Tr.Albedo, Tr.Seed);
			for (int32 k = 0; k < 3; ++k)
			{
				const float S = k - 1.0f;
				V.Add(ToCm(Pt.P + Side * (0.5f * Width * S)));
				N.Add((ToCam * (k == 1 ? 1.0f : 0.6f) + Side * S).GetSafeNormal());
				UV.Add(FVector2D(Pt.Dist, 0.5f * k));
				Col.Add(C);
			}
		}
	}
	// Com o mesmo número de vértices, só atualizar (16/10, o custo de processador: recriar refazia a caixa, a colisão e
	// o desenho a cada quadro). O A/B de 07/10 (o rastro escuro da torre virava um tubo creme aceso só atualizando) era
	// a conversão sRGB: a atualização da Unreal 5.8 converte por padrão e a criação não (ProceduralMeshComponent.h);
	// aqui as duas vão sem converter
	if (V.Num() && V.Num() == TrailVerts)
	{
		TrailMesh->UpdateMeshSection_LinearColor(0, V, N, UV, Col, TArray<FProcMeshTangent>(), false);
		return;
	}
	for (int32 t = 0; t < Trails.Num(); ++t)
	{
		for (int32 i = 1; i < TrailSamples; ++i)
		{
			const int32 A = (t * TrailSamples + i - 1) * 3, B = A + 3;
			for (int32 k = 0; k < 2; ++k)
			{
				Tri.Append({ A + k, B + k, B + k + 1, A + k, B + k + 1, A + k + 1 });
			}
		}
	}
	// criar a seção já a limpa (e refaz a caixa, a colisão e o desenho): limpar antes fazia tudo isso duas vezes por
	// quadro e por destroço (fonte da Unreal 5.8, ProceduralMeshComponent.cpp; 14/10)
	if (Tri.Num())
	{
		TrailMesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, TArray<FProcMeshTangent>(), false);
		TrailVerts = V.Num();
		static const bool bLog = [] { int32 Lv = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxLog="), Lv); return Lv != 0; }();
		if (bLog && Trails.Num() && Trails[0].Rise >= 0.0f && FMath::Fmod(T, 2.0f) < 0.034f)
		{
			FBox B(V);
			float MaxOp = 0.0f;
			for (const FLinearColor& Cc : Col)
			{
				MaxOp = FMath::Max(MaxOp, Cc.R);
			}
			UE_LOG(LogTemp, Display, TEXT("BlastFx fumaça: %s %d vértices, caixa %s .. %s, opacidade máx %.2f, material %s, visível %d"),
				*GetName(), V.Num(), *B.Min.ToString(), *B.Max.ToString(), MaxOp,
				TrailMesh->GetMaterial(0) ? *TrailMesh->GetMaterial(0)->GetName() : TEXT("-"), TrailMesh->IsVisible() ? 1 : 0);
		}
	}
	else
	{
		TrailMesh->ClearMeshSection(0);
		TrailVerts = -1;
	}
}

void ABlastDebris::Tick(float DeltaSeconds)
{
	CSV_SCOPED_TIMING_STAT(BlastFxDebris, Tick);
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	if (!GlowMesh->GetMaterial(0))
	{
		// -DebrisGlow= (13/10): o brilho das cabeças e faíscas (Intensity 2·10⁵ cd/m²) estourava em branco de dia (o
		// vermelho ~13× acima do branco da tela depois da exposição); a ~3·10⁴ elas ficam laranja
		UMaterialInterface* GlowBase = LoadObject<UMaterialInterface>(nullptr, GlowPath);
		static const float GlowCfg = [] { float V = -1.0f; FParse::Value(FCommandLine::Get(), TEXT("DebrisGlow="), V); return V; }();
		const float Glow = GlowCfg >= 0.0f ? GlowCfg : GlowScale;
		GlowMID = GlowBase ? UMaterialInstanceDynamic::Create(GlowBase, this) : nullptr;
		GlowNow = Glow;
		GlowMesh->SetMaterial(0, GlowMID ? GlowMID.Get() : GlowBase);
		// os rastros com a cor da fuligem da explosão (-RmSoot=r,g,b,albedo, 13/10): eram riscos brancos contra a nuvem
		// marrom, outra fumaça na mesma cena
		UMaterialInterface* TrailBase = LoadObject<UMaterialInterface>(nullptr, TrailPath);
		static const FLinearColor SootCfg = []
		{
			FLinearColor C(-1.0f, 0.0f, 0.0f, 1.0f);
			FString S;
			TArray<FString> P;
			if (FParse::Value(FCommandLine::Get(), TEXT("RmSoot="), S, false) && S.ParseIntoArray(P, TEXT(",")) == 4)
			{
				C = FLinearColor(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]), 1.0f);
			}
			return C;
		}();
		const FLinearColor SootTint = SootCfg.R >= 0.0f ? SootCfg : TrailTint;
		if (UMaterialInstanceDynamic* M = TrailBase ? UMaterialInstanceDynamic::Create(TrailBase, this) : nullptr)
		{
			M->SetVectorParameterValue(TEXT("SmokeTint"), SootTint);
			TrailMesh->SetMaterial(0, M);
			TrailMID = M;
		}
		ExpoApplied = -1.0f;
		// diagnóstico (-ShotFxTrailMat=1): a fita num material sólido, pra separar "não tem malha" de "o material some"
		static const bool bSolidTrail = [] { int32 Sv = 0; FParse::Value(FCommandLine::Get(), TEXT("ShotFxTrailMat="), Sv); return Sv != 0; }();
		if (bSolidTrail)
		{
			TrailMesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, BasePath));
		}
	}
	// O brilho compensado pela exposição (16/10; a regra do raymarch de 11/10, BlastRaymarch.cpp): os números dos
	// destroços valem na exposição da maquete do deserto onde a morte de tanque foi aprovada (EV100 13,55 travado, a
	// receita desert_golden_hour: 1/(1,2·2^13,55) = 6,95·10⁻⁵). No jogo a câmera estava 3,2× mais aberta (2,2·10⁻⁴ no
	// World_TestSalt2) e tudo o que soma luz saía branco: faíscas, chamas, o clarão, as luzes no chão.
	{
		static const float RefExpo = [] { float V = 6.95e-5f; FParse::Value(FCommandLine::Get(), TEXT("DebrisRefExpo="), V); return V; }();
		const float E = BlastRaymarch::LastViewExposure();
		ExpoComp = E > 0.0f && RefExpo > 0.0f ? FMath::Clamp(RefExpo / E, 1.0f / 16.0f, 4.0f) : 1.0f;
		if (ExpoApplied < 0.0f || FMath::Abs(ExpoComp / ExpoApplied - 1.0f) > 0.03f)
		{
			ExpoApplied = ExpoComp;
			if (GlowMID)
			{
				GlowMID->SetScalarParameterValue(TEXT("Intensity"), 2.0e5f * GlowNow * ExpoComp);
			}
			if (TrailMID)
			{
				TrailMID->SetScalarParameterValue(TEXT("FlameLum"), 3.0e4f * ExpoComp);
			}
		}
	}
	// Cada parte só vai pra placa enquanto muda (14/10, otimização sem mudar a imagem). Antes as 32 chapas deitadas
	// eram reenviadas todo quadro por 2 minutos, e como fazem sombra a sombra virtual redesenhava as páginas delas;
	// as cabeças, faíscas e rastros apagados também. A última atualização de cada parte é a do estado final.
	if (Dt > 0.0f)
	{
		T += Dt;
		UpdateFollows(Dt);
		UpdateWreckFire(Dt);
		{
			CSV_SCOPED_TIMING_STAT(BlastFxDebris, Integrate);   // (16/10, o custo de processador do tiro: medir por parte)
			Integrate(Dt);
		}
		{
			CSV_SCOPED_TIMING_STAT(BlastFxDebris, Light);
			UpdateLight(Dt);
		}
		if (!bPlatesSettled)
		{
			CSV_SCOPED_TIMING_STAT(BlastFxDebris, Plates);
			UpdatePlates();
			bPlatesSettled = !Plates.ContainsByPredicate([](const FPlate& P) { return !P.bRest; });
		}
	}
	const FVector Cam = CameraM(GetWorld());
	if (!bGlowsDone)
	{
		CSV_SCOPED_TIMING_STAT(BlastFxDebris, Glows);
		UpdateGlows(Cam);
		bGlowsDone = !Comets.ContainsByPredicate([](const FComet& C) { return C.Age < C.Burn; })
			&& !Arcs.ContainsByPredicate([](const FArc& Ac) { return Ac.Age <= Ac.Life; })
			&& !Sparks.ContainsByPredicate([](const FSpark& S) { return S.Age <= S.Life; });
	}
	if (!bTrailsDone)
	{
		CSV_SCOPED_TIMING_STAT(BlastFxDebris, Trails);
		UpdateTrails(Cam);
		bTrailsDone = !Trails.ContainsByPredicate([](const FTrail& Tr) { return Tr.bEmitting || Tr.Points.Num() > 0; })
			&& !Follows.ContainsByPredicate([](const FFollow& F) { return !F.bDone; });
	}
	if (T < FireUntil)
	{
		bGlowsDone = false;                         // o fogo da carcaça segue soltando chamas
	}
	// (23/10, a vida) o pedaço em brasa que ainda cai (apagado, sem cabeça nem rastro) segura o tique até pousar
	if (bPlatesSettled && bGlowsDone && bTrailsDone && LightPeak <= 0.0f && T >= FireUntil && T >= BlinkUntil
		&& !Comets.ContainsByPredicate([](const FComet& C) { return C.Land == 1; }))
	{
		SetActorTickEnabled(false);
	}
}
