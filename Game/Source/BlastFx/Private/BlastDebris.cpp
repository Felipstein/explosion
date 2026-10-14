// Cópia do BlastLab (BlastDebris, 14/10/2026): ver BlastDebris.h.
#include "BlastDebris.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "BlastFxGround.h"
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
	const float Keep = Eq >= 3 ? 1.0f : (Eq == 2 ? 0.5f : 0.25f);
	const int32 PlateCount = FMath::Max(4, FMath::RoundToInt(NumPlates * Keep));
	const int32 CometCount = FMath::Max(2, FMath::RoundToInt(NumComets * Keep));
	const int32 EmberCount = FMath::RoundToInt(NumEmbers * Keep);
	BuildShapes();
	const FVector O = ToM(OriginCm);
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

	// as malhas: as chapas têm topologia fixa (criadas uma vez, só os vértices mudam)
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
	PlateMesh->SetCustomPrimitiveDataVector4(0, FVector4(PlateColor.R, PlateColor.G, PlateColor.B, PlateRough));
	PlateMesh->SetCustomPrimitiveDataFloat(4, PlateMetal);
	UpdatePlates();
	SetLifeSpan(120.0f);
}

void ABlastDebris::Integrate(float Dt)
{
	const UWorld* W = GetWorld();
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
		const float Floor = float(BlastFxGround::HeightAtCached(W, P.P.X * 100.0, P.P.Y * 100.0) / 100.0) + PlateThickM;
		if (P.P.Z < Floor && P.V.Z < 0.0f)
		{
			P.P.Z = Floor;
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
	for (FComet& C : Comets)
	{
		C.Age += Dt;
		const bool bBurning = C.Age < C.Burn;
		if (bBurning)
		{
			C.V += (FVector(0.0f, 0.0f, -G) - CometDrag * C.V.Size() * C.V) * Dt;
			C.P += C.V * Dt;
			const float Floor = float(BlastFxGround::HeightAtCached(W, C.P.X * 100.0, C.P.Y * 100.0) / 100.0) + 0.05f;
			if (C.P.Z < Floor)
			{
				C.P.Z = Floor;
				C.Burn = FMath::Min(C.Burn, C.Age + 0.15f);      // apaga ao bater na areia
				C.V = FVector::ZeroVector;
			}
		}
		SetTrailHead(C.Trail, ToCm(C.P), bBurning);
	}
	for (FSpark& S : Sparks)
	{
		S.Age += Dt;
		if (S.Age < 0.0f || S.Age > S.Life)
		{
			continue;
		}
		S.V += (FVector(0.0f, 0.0f, -G * (1.0f - S.Lift)) - S.Drag * S.V.Size() * S.V) * Dt;
		S.P += S.V * Dt;
		const float Floor = float(BlastFxGround::HeightAtCached(W, S.P.X * 100.0, S.P.Y * 100.0) / 100.0) + 0.03f;
		if (S.P.Z < Floor)
		{
			// no chão: a fagulha fica e apaga em ~0.3 s; o espinho morre
			S.P.Z = Floor;
			S.V = FVector::ZeroVector;
			S.Life = FMath::Min(S.Life, S.Age + (S.bEmber ? 0.3f : 0.0f));
		}
	}
	for (FTrail& Tr : Trails)
	{
		Tr.Age += Dt;
		for (FTrailPoint& Pt : Tr.Points)
		{
			Pt.Age += Dt;
			Pt.P.Z += TrailRise * Dt;
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
		// apaga no último terço da queima
		const float Fade = 1.0f - FMath::SmoothStep(0.65f * C.Burn, C.Burn, C.Age);
		const FVector ToCam = (CamM - C.P).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(ToCam, FVector::UpVector).GetSafeNormal() * (0.5f * CometHeadM);
		const FVector Up = FVector::CrossProduct(Right, ToCam).GetSafeNormal() * (0.5f * CometHeadM);
		for (int32 k = 0; k < 4; ++k)
		{
			const float Sx = (k == 1 || k == 2) ? 1.0f : -1.0f, Sy = k >= 2 ? 1.0f : -1.0f;
			V.Add(ToCm(C.P + Right * Sx + Up * Sy));
			N.Add(ToCam);
			UV.Add(FVector2D(0.5f + 0.5f * Sx, 0.5f + 0.5f * Sy));
			Col.Add(FMath::Lerp(EmberColor, CometHeadColor, SparkScaleNow()) * Fade);   // com a bola menor, laranja (a referência da torre voando: pedaços em chamas)            // o brilho absoluto vem do material (Intensity: a cor do vértice é de 8 bits)
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
		if (S.bEmber)
		{
			Glow *= 0.6f + 0.4f * FMath::Sin(S.Age * 25.0f + S.Seed);     // pisca
		}
		const FVector ToCam = (CamM - S.P).GetSafeNormal();
		FVector Along = S.V - ToCam * FVector::DotProduct(S.V, ToCam);   // a velocidade vista da câmera
		const float Streak = 0.5f * Along.Size() * (1.0f / 30.0f);
		Along = Along.IsNearlyZero() ? FVector::CrossProduct(ToCam, FVector::UpVector).GetSafeNormal() : Along.GetSafeNormal();
		const FVector Across = FVector::CrossProduct(ToCam, Along).GetSafeNormal() * S.Size;
		const FVector Len = Along * (S.Size + Streak);
		for (int32 k = 0; k < 4; ++k)
		{
			const float Sx = (k == 1 || k == 2) ? 1.0f : -1.0f, Sy = k >= 2 ? 1.0f : -1.0f;
			V.Add(ToCm(S.P + Len * Sx + Across * Sy));
			N.Add(ToCam);
			// u fixo no meio: o perfil do material só varia na largura (risco inteiro, não um ponto)
			UV.Add(FVector2D(0.5f, 0.5f + 0.5f * Sy));
			Col.Add((S.bEmber ? EmberColor : FMath::Lerp(EmberColor, SpikeColor, SparkScaleNow())) * FMath::Clamp(Glow, 0.0f, 1.0f));
		}
	}
	if (V.Num() == GlowVerts)
	{
		GlowMesh->UpdateMeshSection_LinearColor(0, V, N, UV, Col, TArray<FProcMeshTangent>());
		return;
	}
	for (int32 q = 0; q < NumQuads; ++q)
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
			// a massa por metro fica: opacidade ~ largura inicial / largura; some no fim da vida
			const float Opacity = Tr.Opacity0 * FMath::Clamp((Tr.Width0 + 0.25f) / (Width + 0.25f), 0.0f, 1.0f)
				* (1.0f - FMath::SmoothStep(0.55f * Tr.Life, Tr.Life, Pt.Age))
				* FMath::SmoothStep(0.0f, 0.04f, Pt.Age + (i == TrailSamples - 1 ? 0.0f : 0.02f));
			// fogo: o gás logo atrás da peça ainda brilha (some em ~0.25 s de idade)
			const float Glow = Tr.GlowS > 0.0f && Tr.Age < Tr.GlowS ? FMath::Exp(-Pt.Age / 0.25f) * (1.0f - FMath::SmoothStep(0.6f * Tr.GlowS, Tr.GlowS, Tr.Age)) : 0.0f;
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
	// A fita é refeita a cada quadro (já pequena: TrailSamples pontos por rastro).
	// Só atualizar os vértices (UpdateMeshSection) mudava a luz dela: o rastro
	// escuro da torre saía como um tubo creme aceso (A/B de 07/10, rodadas
	// ab_close_final × ab_close_rebuild). Nas cabeças, que não recebem luz, vale
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
	}
	else
	{
		TrailMesh->ClearMeshSection(0);
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
		UMaterialInstanceDynamic* GM = GlowBase && Glow != 1.0f ? UMaterialInstanceDynamic::Create(GlowBase, this) : nullptr;
		if (GM)
		{
			GM->SetScalarParameterValue(TEXT("Intensity"), 2.0e5f * Glow);
		}
		GlowMesh->SetMaterial(0, GM ? GM : GlowBase);
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
		}
	}
	// Cada parte só vai pra placa enquanto muda (14/10, otimização sem mudar a imagem). Antes as 32 chapas deitadas
	// eram reenviadas todo quadro por 2 minutos, e como fazem sombra a sombra virtual redesenhava as páginas delas;
	// as cabeças, faíscas e rastros apagados também. A última atualização de cada parte é a do estado final.
	if (Dt > 0.0f)
	{
		T += Dt;
		UpdateFollows(Dt);
		Integrate(Dt);
		if (!bPlatesSettled)
		{
			UpdatePlates();
			bPlatesSettled = !Plates.ContainsByPredicate([](const FPlate& P) { return !P.bRest; });
		}
	}
	const FVector Cam = CameraM(GetWorld());
	if (!bGlowsDone)
	{
		UpdateGlows(Cam);
		bGlowsDone = !Comets.ContainsByPredicate([](const FComet& C) { return C.Age < C.Burn; })
			&& !Sparks.ContainsByPredicate([](const FSpark& S) { return S.Age <= S.Life; });
	}
	if (!bTrailsDone)
	{
		UpdateTrails(Cam);
		bTrailsDone = !Trails.ContainsByPredicate([](const FTrail& Tr) { return Tr.bEmitting || Tr.Points.Num() > 0; })
			&& !Follows.ContainsByPredicate([](const FFollow& F) { return !F.bDone; });
	}
	if (bPlatesSettled && bGlowsDone && bTrailsDone)
	{
		SetActorTickEnabled(false);
	}
}
