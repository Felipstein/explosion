#include "BlastLabMover.h"

#include "BattleSmokeSim.h"
#include "BattleSmokeSubsystem.h"
#include "BlastLabSimulation.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr float G = 9.81f;
	constexpr float Rho = 1.225f;
	// radiância e iluminância do WebGL → cd/m² e lux (js/celestial.js SUN_LUX)
	constexpr float LuxPerUnit = 128000.0f;

	// Os aviões: o que importa pro ar é o peso, a envergadura, a velocidade e o
	// empuxo (a esteira Γ = W/(ρ·U·b0), o jato) e o volume que o corpo varre.
	struct FAircraftSpec
	{
		float Speed, Mass, Span, Thrust;   // m/s, kg, m, N
		float Approach;                    // m antes (e depois) do alvo
		float NozzleBack, NozzleWidth;     // bocal equivalente dos motores: m atrás do centro, m de diâmetro
		float JetExit;                     // velocidade do jato em relação ao avião (m/s)
		float BodyRadius;                  // meia espessura do que varre o ar (m)
		float WingHalfSpan;                // > 0: asa voadora, o corpo varre uma placa dessa meia largura
	};
	// movers.js MOVER_SPECS: avião de ataque ao solo (classe A-10 / Su-25), 20 t,
	// 16 m de envergadura, 140 m/s em passagem baixa; dois turbofans TF34 de 40 kN
	// somados num bocal de 1.7 m, jato a ~300 m/s em relação a ele
	constexpr FAircraftSpec PlaneSpec = { 140.0f, 20000.0f, 16.0f, 80000.0f, 260.0f, 7.2f, 1.7f, 300.0f, 1.8f, 0.0f };
	// Bombardeiro pesado (classe B-2 Spirit, Northrop): asa voadora de 52.4 m de
	// envergadura, 21 m de comprimento, ~150 t em missão (171 t no máximo), quatro
	// F118 de 77 kN com a saída no dorso, ~9 m atrás do centro. Passagem baixa e
	// lenta (130 m/s, ~250 nós): Γ = 150 t·g/(ρ·130·41 m) ≈ 225 m²/s, 2.5× a do
	// A-10, o par a 41 m um do outro, mais largo que a nuvem; o centro da asa tem
	// ~2.6 m de espessura.
	constexpr FAircraftSpec BomberSpec = { 130.0f, 150000.0f, 52.4f, 308000.0f, 260.0f, 9.0f, 2.8f, 300.0f, 1.3f, 26.2f };
	constexpr float PlaneAlt = 34.0f;                // altura padrão sem fumaça pra mirar

	const FAircraftSpec& Spec(EBlastMover Kind) { return Kind == EBlastMover::Bomber ? BomberSpec : PlaneSpec; }
	bool IsAircraft(EBlastMover Kind) { return Kind == EBlastMover::Plane || Kind == EBlastMover::Bomber; }
	namespace MissileSpec
	{
		// míssil terra-ar: 3 m, sai do tubo a 80 m/s e acelera (~15 g) até 450 m/s
		// (empuxo de ~10 kN: um míssil de ~70 kg a 15 g)
		constexpr float V0 = 80.0f, Accel = 150.0f, VMax = 450.0f, Thrust = 10000.0f, Life = 4.0f, Standoff = 70.0f;
	}
	namespace ShellSpec
	{
		// tanque: munição de 120 mm a ~1000 m/s com traçante
		constexpr float Speed = 1000.0f, Life = 1.4f, Standoff = 90.0f;
	}

	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");               // 1 m
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");   // 1 m de diâmetro e de altura, centrado
	const TCHAR* BasePath = TEXT("/Game/Materials/M_Base.M_Base");               // build_scene.py
	const TCHAR* EmissivePath = TEXT("/Game/Materials/M_Emissive.M_Emissive");   // build_movers.py

	// pintura militar fosca e aço escuro (scene.js ids 4 e 3: metal 0.15 e 0.6)
	const FLinearColor Paint(0.17f, 0.19f, 0.18f);
	const FLinearColor Canopy(0.08f, 0.09f, 0.10f);
	const FLinearColor Stealth(0.11f, 0.115f, 0.12f);     // o cinza-chumbo do B-2
	const FLinearColor Engine(0.10f, 0.10f, 0.10f);
	const FLinearColor MissileBody(0.55f, 0.56f, 0.52f);
	const FLinearColor Fins(0.40f, 0.40f, 0.38f);
	// radiância (unidade do WebGL) da chama do motor e do traçante
	const FLinearColor FlameRadiance(40.0f, 22.0f, 8.0f);
	const FLinearColor TracerRadiance(60.0f, 18.0f, 5.0f);
	constexpr float TracerWidthM = 0.14f, TracerMinPixels = 3.0f;
	// O brilho em volta do traçante (M_Glow): o halo que o bloom do WebGL dava
	// a ele. Medido no WebGL (bloom ligado − desligado, o traçante a ~90 m de
	// dia): vai até ~96 px da linha (~6 m) e soma ~0.21 (radiância do render ×
	// m) por metro de traçante, 2.5% da luz do núcleo. (Não os 0.364 da força
	// nominal, 7 × 0.052: a média de Karis do pré-filtro corta quase tudo de uma
	// linha fina e clara.) Perfil cos² visto de lado: Σ na largura = 0.667·diâmetro.
	constexpr float TracerGlowShare = 0.21f / (60.0f * 0.14f), TracerGlowM = 6.0f;
	const FLinearColor TracerGlowRadiance = TracerRadiance * (TracerGlowShare * TracerWidthM / (TracerGlowM * 0.667f));
	const TCHAR* GlowPath = TEXT("/Game/Materials/M_Glow.M_Glow");             // build_movers.py
	// Luz da chama: ~0.5 MW visíveis (motor de ~2.5 MW térmicos) contra ~200 MW
	// da bola de fogo de um disparo de tanque, 0.3% da luz de pico da explosão.
	// Basta pra fumaça do próprio rastro brilhar laranja perto do bocal.
	const FLinearColor FlameLightColor(0.30f, 0.16f, 0.06f);
	// como a luz do cenário das explosões: 50·cor/d² (scene.js cor/(s² + 0.02·d²));
	// no volume 20·cor/d² (instances.js cor/(s² + 0.05·d²)): 0.4 da do cenário
	constexpr float SceneLightFactor = 50.0f, VolumeShare = 0.4f, FlameRadiusCm = 200.0f;

	FBattleSmokeDisturber Disturber(const FVector& A, float Radius, const FVector& B, int32 Kind, const FVector& Vel, float Strength,
		const FVector& D = FVector::ZeroVector, float DW = 0.0f)
	{
		FBattleSmokeDisturber Out;
		Out.A = FVector4f(FVector3f(A), Radius);
		Out.B = FVector4f(FVector3f(B), float(Kind));
		Out.C = FVector4f(FVector3f(Vel), Strength);
		Out.D = FVector4f(FVector3f(D), DW);
		return Out;
	}
}

ABlastLabMover::ABlastLabMover()
{
	PrimaryActorTick.bCanEverTick = true;
	// antes dos subsistemas: o fluido e a entrega antecipada deste quadro já veem a passagem
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

ABlastLabMover* ABlastLabMover::Launch(UWorld* World, EBlastMover Kind, const FVector& TargetCm, const FVector& Side, float AltM)
{
	if (!World)
	{
		return nullptr;
	}
	const FVector T = TargetCm / 100.0;
	const float H = FMath::Max(AltM >= 0.0f ? AltM : PlaneAlt, 12.0f);
	FVector P, D;
	float Speed = 0.0f, Life = 1.0f;
	switch (Kind)
	{
	case EBlastMover::Plane:
	case EBlastMover::Bomber:
		// em linha reta pela altura da fumaça, de 260 m antes a 260 m depois
		P = FVector(T.X, T.Y, H) - Side * Spec(Kind).Approach;
		D = Side;
		Speed = Spec(Kind).Speed;
		Life = 2.0f * Spec(Kind).Approach / Spec(Kind).Speed;
		break;
	case EBlastMover::Missile:
		// do chão, de um lado, subindo pela nuvem
		P = FVector(T.X, T.Y, 1.5f) - Side * MissileSpec::Standoff;
		D = (FVector(T.X, T.Y, H) - P).GetSafeNormal();
		Speed = MissileSpec::V0;
		Life = MissileSpec::Life;
		break;
	default:
		// o tanque atira de longe num alvo acima da fumaça
		P = FVector(T.X, T.Y, 2.6f) - Side * ShellSpec::Standoff;
		D = (FVector(T.X, T.Y, H) - P).GetSafeNormal();
		Speed = ShellSpec::Speed;
		Life = ShellSpec::Life;
		break;
	}
	const FTransform Start(D.Rotation(), P * 100.0);
	ABlastLabMover* M = World->SpawnActorDeferred<ABlastLabMover>(StaticClass(), Start, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!M)
	{
		return nullptr;
	}
	M->Kind = Kind;
	M->Pos = M->Prev = P;
	M->Dir = D;
	M->Speed = Speed;
	M->Life = Life;
	M->FinishSpawning(Start);
	if (IsAircraft(Kind))
	{
		// a esteira de vórtices que ele deixa no caminho (fica depois que ele passa)
		if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(World))
		{
			const float B0 = 0.25f * PI * Spec(Kind).Span;
			const float Gamma = Spec(Kind).Mass * G / (Rho * Speed * B0);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal();
			M->WakeId = Battle->BeginWake(P, D, Speed, Right, B0, Gamma);
		}
	}
	M->Build();
	M->UpdateGeometry();
	return M;
}

float ABlastLabMover::TimeToTarget(EBlastMover Kind, float AltM)
{
	const float H = FMath::Max(AltM, 12.0f);
	switch (Kind)
	{
	case EBlastMover::Plane:
	case EBlastMover::Bomber:
		return Spec(Kind).Approach / Spec(Kind).Speed;
	case EBlastMover::Missile:
	{
		// d = v0·t + a·t²/2 (o teto de 450 m/s só chega depois de 2.5 s)
		const float D = FMath::Sqrt(FMath::Square(MissileSpec::Standoff) + FMath::Square(H - 1.5f));
		const float A = 0.5f * MissileSpec::Accel;
		return (-MissileSpec::V0 + FMath::Sqrt(MissileSpec::V0 * MissileSpec::V0 + 4.0f * A * D)) / (2.0f * A);
	}
	default:
		return FMath::Sqrt(FMath::Square(ShellSpec::Standoff) + FMath::Square(H - 2.6f)) / ShellSpec::Speed;
	}
}

UStaticMeshComponent* ABlastLabMover::AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& CenterM, const FVector& SizeM,
	bool bAlongForward, const FLinearColor& Color, float Rough, float Metal, bool bEmissive, float YawDeg)
{
	if (!Mesh || !Material)
	{
		return nullptr;
	}
	// eixos locais: x = frente, y = direita, z = cima (o ator aponta pra onde voa)
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Material);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(!bEmissive);
	C->SetRelativeLocation(CenterM * 100.0);
	if (bAlongForward)
	{
		// o cilindro da Unreal fica em pé (z): deitado ao longo da frente
		C->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
		C->SetRelativeScale3D(FVector(SizeM.Y, SizeM.Z, SizeM.X));
	}
	else
	{
		C->SetRelativeRotation(FRotator(0.0f, YawDeg, 0.0f));
		C->SetRelativeScale3D(SizeM);
	}
	// M_Base: cor + rugosidade, metal · M_Emissive: radiância
	C->SetCustomPrimitiveDataVector4(0, FVector4(Color.R, Color.G, Color.B, Rough));
	C->SetCustomPrimitiveDataFloat(4, Metal);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

void ABlastLabMover::Build()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, BasePath);
	UMaterialInterface* Emissive = LoadObject<UMaterialInterface>(nullptr, EmissivePath);
	// (centro e tamanho em m: frente, direita, cima) — as peças de movers.js _geom
	switch (Kind)
	{
	case EBlastMover::Plane:
		AddPart(Cube, Base, FVector(0.0f, 0.0f, 0.0f), FVector(15.0f, 1.7f, 1.7f), false, Paint, 0.55f, 0.15f, false);     // fuselagem
		AddPart(Cube, Base, FVector(8.2f, 0.0f, -0.1f), FVector(1.8f, 1.1f, 1.1f), false, Paint, 0.55f, 0.15f, false);    // nariz
		AddPart(Cube, Base, FVector(8.9f, 0.0f, 0.6f), FVector(1.6f, 0.9f, 0.5f), false, Canopy, 0.2f, 0.6f, false);      // cabine
		AddPart(Cube, Base, FVector(-0.5f, 0.0f, -0.2f), FVector(3.2f, 16.0f, 0.3f), false, Paint, 0.55f, 0.15f, false);  // asa
		AddPart(Cube, Base, FVector(-6.8f, 0.0f, 0.4f), FVector(1.8f, 6.0f, 0.2f), false, Paint, 0.55f, 0.15f, false);    // profundor
		AddPart(Cube, Base, FVector(-6.6f, 0.0f, 1.9f), FVector(2.2f, 0.25f, 2.6f), false, Paint, 0.55f, 0.15f, false);   // deriva
		for (const float S : { -1.6f, 1.6f })
		{
			// motores: 3.2 m de comprimento a partir de 5.6 m atrás
			AddPart(Cylinder, Base, FVector(-4.0f, S, 1.1f), FVector(3.2f, 1.2f, 1.2f), true, Engine, 0.5f, 0.6f, false);
		}
		break;
	case EBlastMover::Bomber:
	{
		// asa voadora em blocos: o miolo, a cabine e as duas meias-asas
		// enflechadas a 33° (bordo de ataque do nariz à ponta, 26.2 m pro lado)
		AddPart(Cube, Base, FVector(1.0f, 0.0f, 0.0f), FVector(17.0f, 11.0f, 2.6f), false, Stealth, 0.6f, 0.15f, false);    // miolo
		AddPart(Cube, Base, FVector(5.5f, 0.0f, 1.5f), FVector(6.0f, 4.0f, 1.0f), false, Stealth, 0.4f, 0.15f, false);     // cabine
		AddPart(Cube, Base, FVector(8.0f, 0.0f, 1.6f), FVector(1.6f, 2.6f, 0.4f), false, Canopy, 0.2f, 0.6f, false);      // para-brisa
		for (const float S : { -1.0f, 1.0f })
		{
			AddPart(Cube, Base, FVector(-2.5f, S * 15.0f, -0.2f), FVector(9.0f, 23.0f, 1.2f), false, Stealth, 0.6f, 0.15f, false, S * 33.0f);
			// saídas dos motores no dorso, em fenda
			AddPart(Cube, Base, FVector(-7.0f, S * 4.5f, 1.2f), FVector(2.0f, 3.0f, 0.5f), false, Engine, 0.5f, 0.6f, false);
		}
		break;
	}
	case EBlastMover::Missile:
		AddPart(Cylinder, Base, FVector(-0.1f, 0.0f, 0.0f), FVector(3.0f, 0.28f, 0.28f), true, MissileBody, 0.45f, 0.15f, false);
		// aletas em cruz
		AddPart(Cube, Base, FVector(-1.35f, 0.0f, 0.0f), FVector(0.45f, 0.05f, 0.9f), false, Fins, 0.5f, 0.15f, false);
		AddPart(Cube, Base, FVector(-1.35f, 0.0f, 0.0f), FVector(0.45f, 0.9f, 0.05f), false, Fins, 0.5f, 0.15f, false);
		// chama do motor: cone de luz atrás do bocal, cintilando (UpdateGeometry)
		Flame = AddPart(Cylinder, Emissive, FVector(-2.3f, 0.0f, 0.0f), FVector(1.4f, 0.22f, 0.22f), true, FlameRadiance, 1.0f, 0.0f, true);
		FlameLight = NewObject<UPointLightComponent>(this);
		FlameLight->SetMobility(EComponentMobility::Movable);
		FlameLight->SetupAttachment(RootComponent);
		FlameLight->SetRelativeLocation(FVector(-220.0f, 0.0f, 0.0f));
		FlameLight->SetIntensityUnits(ELightUnits::Candelas);
		FlameLight->SetLightColor(FlameLightColor / FlameLightColor.R);
		FlameLight->SetSourceRadius(FlameRadiusCm);
		// sem sombra (é pequena e passa rápido); o cenário (canal 0) e os volumes (canal 1)
		FlameLight->SetCastShadows(false);
		FlameLight->SetVolumetricScatteringIntensity(VolumeShare);
		FlameLight->LightingChannels.bChannel0 = true;
		FlameLight->LightingChannels.bChannel1 = true;
		FlameLight->RegisterComponent();
		AddInstanceComponent(FlameLight);
		break;
	default:
		// traçante: o rastro que o olho integra num quadro, fino e brilhante (UpdateGeometry)
		Tracer = AddPart(Cube, Emissive, FVector::ZeroVector, FVector(1.0f, TracerWidthM, TracerWidthM), false, TracerRadiance, 1.0f, 0.0f, true);
		TracerGlow = AddPart(Cylinder, LoadObject<UMaterialInterface>(nullptr, GlowPath), FVector::ZeroVector,
			FVector(1.0f, TracerGlowM, TracerGlowM), true, TracerGlowRadiance, 1.0f, 0.0f, true);
		break;
	}
}

void ABlastLabMover::Send(const FBattleSmokeDisturber& D, float CrossRadiusM)
{
	if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
	{
		Battle->AddDisturber(D);
	}
	if (UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
	{
		// a entrega antecipada olha o trecho varrido com a folga do raio (battlesmoke.js _crossed)
		Sim->AddMoverSegment(FVector(D.B.X, D.B.Y, D.B.Z) * 100.0, FVector(D.A.X, D.A.Y, D.A.Z) * 100.0, CrossRadiusM * 100.0f);
	}
}

void ABlastLabMover::Disturb()
{
	const FVector Vel = Dir * Speed;
	switch (Kind)
	{
	case EBlastMover::Plane:
	case EBlastMover::Bomber:
	{
		const FAircraftSpec& S = Spec(Kind);
		// "direita" orientada pra que o par sopre PRA BAIXO entre os vórtices
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
		// Corpo: o ar perto é arrastado (camada limite, esteira) e a projeção faz
		// o resto do escoamento contornar; o túnel na fumaça é a fração que o
		// corpo varre. A asa voadora varre uma placa da envergadura inteira (tipo 3).
		if (S.WingHalfSpan > 0.0f)
		{
			Send(Disturber(Pos, S.BodyRadius, Prev, 3, Vel, 0.2f, Right, S.WingHalfSpan), S.WingHalfSpan);
		}
		else
		{
			Send(Disturber(Pos, S.BodyRadius, Prev, 1, Vel, 0.2f), S.BodyRadius);
		}
		// Jato dos motores (somados num bocal equivalente): sai a ~300 m/s em
		// relação ao avião; mais atrás, o que manda é a quantidade de movimento
		// que o empuxo deixa no ar (o jato em BattleSmoke.usf)
		const FVector Nozzle = Pos - Dir * S.NozzleBack;
		Send(Disturber(Nozzle, 1.0f, Nozzle - Dir * 150.0f, 6, Vel, S.Thrust / Speed / Rho,
			FVector(S.JetExit - Speed, 0.0f, 0.0f), S.NozzleWidth), S.NozzleWidth);
		// Esteira: Γ = W/(ρ·U·b0). A-10: 20 t a 140 m/s → Γ ≈ 91 m²/s, o par
		// desce a ~1.1 m/s. Bombardeiro: Γ ≈ 225 m²/s, o par a 41 m. Esta vai pro
		// fluido do campo (o rastro do míssil); a que deforma as explosões é a do
		// caminho inteiro (BeginWake), calculada no ponto.
		const float B0 = 0.25f * PI * S.Span;
		const float Gamma = S.Mass * G / (Rho * Speed * B0);
		Send(Disturber(Pos, 2.0f, Prev, 4, Vel, Gamma, Right, B0), B0);
		// a esteira que deforma as explosões é a do caminho inteiro (BeginWake), calculada no ponto
		if (UBattleSmokeSubsystem* Battle = UBattleSmokeSubsystem::Get(this))
		{
			Battle->ExtendWake(WakeId, Age * Speed);
		}
		break;
	}
	case EBlastMover::Missile:
	{
		// o escape sai da cauda; o trecho varrido neste quadro é onde a pluma está.
		// Fumaça do propelente com ~2.5 m já ao nascer: com 0.9 m de raio o
		// rastro tinha uma célula de largura e saía pontilhado na diagonal.
		const FVector Tail = Pos - Dir * 1.8f;
		const FVector TailPrev = Prev - Dir * 1.8f;
		Send(Disturber(Tail, 1.3f, TailPrev, 5, Vel, 0.08f), 1.3f);
		// jato do motor-foguete: ~2 km/s em relação ao bocal de 25 cm
		Send(Disturber(Tail, 1.0f, Tail - Dir * 150.0f, 6, Vel, MissileSpec::Thrust / Speed / Rho,
			FVector(2000.0f - Speed, 0.0f, 0.0f), 0.25f), 1.0f);
		break;
	}
	default:
		// 120 mm a Mach 3: o choque de proa e a esteira turbulenta misturam um
		// tubo de ~2 m de diâmetro logo atrás dele
		Send(Disturber(Pos, 1.1f, Prev, 2, Vel, 1.0f), 1.1f);
		break;
	}
}

void ABlastLabMover::UpdateGeometry()
{
	SetActorLocation(Pos * 100.0);
	if (Flame)
	{
		const float Flick = 0.8f + 0.4f * FMath::FRand();
		Flame->SetRelativeLocation(FVector((-1.6f - 0.7f * Flick) * 100.0f, 0.0f, 0.0f));
		Flame->SetRelativeScale3D(FVector(0.22f, 0.22f, 1.4f * Flick));
		if (FlameLight)
		{
			const float Candela = SceneLightFactor * LuxPerUnit * FlameLightColor.R * Flick;
			FlameLight->SetIntensity(Candela);
			const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
			// alcance: até ficar abaixo de 1% do ambiente da hora (como a luz das explosões)
			const float Ambient = FMath::Max(Sim ? Sim->AmbientLux() : 52000.0f, 1.0f);
			const float Reach = FMath::Clamp(FMath::Sqrt(Candela / (0.01f * Ambient)) * 100.0f, 2000.0f, 15000.0f);
			if (FMath::Abs(FlameLight->AttenuationRadius - Reach) > 0.1f * Reach)
			{
				FlameLight->SetAttenuationRadius(Reach);
			}
		}
	}
	if (Tracer)
	{
		const float SegL = FMath::Min(float(FVector::Dist(Pos, Prev)), 24.0f);
		Tracer->SetVisibility(SegL >= 0.05f);
		Tracer->SetRelativeLocation(FVector(-0.5f * SegL * 100.0f, 0.0f, 0.0f));
		// Espessura mínima de ~3 pixels do render, com a radiância dividida na
		// mesma proporção (a luz por metro de rastro, e o bloom dela, ficam os
		// mesmos). Com 0.14 m a 90 m o traço tinha ~1.5 pixel: o TSR, que pesa
		// as amostras por 1/(1 + brilho) contra os vagalumes, o apagava a um
		// pontilhado pálido sem bloom (medido 05/10; com FXAA ele aparecia).
		float Width = TracerWidthM;
		if (const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this))
		{
			const float DistM = float(FVector::Dist(Pos - Dir * (0.5f * SegL), Sim->ViewPoint() / 100.0));
			Width = FMath::Max(Width, TracerMinPixels * DistM * Sim->RadiansPerPixel());
		}
		Tracer->SetRelativeScale3D(FVector(FMath::Max(SegL, 0.05f), Width, Width));
		const FLinearColor R = TracerRadiance * (TracerWidthM / Width);
		Tracer->SetCustomPrimitiveDataVector4(0, FVector4(R.R, R.G, R.B, 1.0f));
		if (TracerGlow)
		{
			TracerGlow->SetVisibility(SegL >= 0.05f);
			TracerGlow->SetRelativeLocation(FVector(-0.5f * SegL * 100.0f, 0.0f, 0.0f));
			TracerGlow->SetRelativeScale3D(FVector(TracerGlowM, TracerGlowM, FMath::Max(SegL, 0.05f)));
		}
	}
}

void ABlastLabMover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// anda no tempo da simulação: a pausa e a câmera lenta valem pra ele também
	const UBlastLabSimulation* Sim = UBlastLabSimulation::Get(this);
	const float Dt = Sim ? Sim->SimDelta(DeltaTime) : DeltaTime;
	Prev = Pos;
	if (Dt > 0.0f)
	{
		Age += Dt;
		if (Kind == EBlastMover::Missile)
		{
			Speed = FMath::Min(MissileSpec::V0 + MissileSpec::Accel * Age, MissileSpec::VMax);
		}
		Pos += Dir * Speed * Dt;
		Disturb();
	}
	if (Age >= Life)
	{
		Destroy();
		return;
	}
	UpdateGeometry();
}
