#include "RTSUnitActor.h"
#include "Components/DecalComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "RTSEffect.h"
#include "RTSGameData.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UStaticMesh* LoadMesh(const FString& PackagePath)
	{
		const FString Name = FPaths::GetBaseFilename(PackagePath);
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** "…/wheel_*" vira wheel_001, wheel_002… até não existir mais. */
	TArray<FString> ExpandPattern(const FString& Path)
	{
		TArray<FString> Out;
		if (!Path.EndsWith(TEXT("*")))
		{
			Out.Add(Path);
			return Out;
		}
		const FString Prefix = Path.LeftChop(1);
		for (int32 N = 1; N < 100; ++N)
		{
			const FString Candidate = FString::Printf(TEXT("%s%03d"), *Prefix, N);
			if (!LoadMesh(Candidate))
			{
				break;
			}
			Out.Add(Candidate);
		}
		return Out;
	}

	/** Sorteio só visual, fixo por unidade (não toca a simulação). */
	uint32 CosmeticHash(uint32 Id)
	{
		uint32 X = Id * 0x9E3779B9u;
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15;
		return X;
	}

	constexpr float GravityCm = 980.0f;

	// Suspensão do CrusaderLocomotor (Locomotor.ini do ZH 1.04): a mola e o amortecimento
	// por passo de 1/30 s, o teto de inclinação e os fatores de aceleração. A mola é só no
	// desenho: o ZH também balança o tanque sem mexer na lógica (Appearance = TREADS).
	constexpr float SuspStep = 1.0f / 30.0f;
	constexpr float AccelPitchLimitDeg = 5.0f;     // AccelerationPitchLimit / DecelerationPitchLimit
	constexpr float PitchStiffness = 0.05f;
	constexpr float PitchDamping = 0.8f;
	constexpr float RollStiffness = 0.025f;
	constexpr float RollDamping = 0.3f;
	constexpr float ForwardAccelPitchFactor = 0.3f;
	constexpr float LateralAccelRollFactor = 0.33f;
	/** Graus de impulso por m/s de velocidade ganha num passo (a escala é nossa: o fator é do ZH). */
	constexpr float ImpulseDegPerMps = 1.0f;
	/** O tranco do tiro: o canhão empurra o casco pra trás (graus por passo). */
	constexpr float FireKickDeg = 1.2f;
	constexpr float WreckFireSeconds = 30.0f;

	// a obra e o prédio (dados de primitiva do M_Structure, Game/Tools/import_provisional.py)
	constexpr int32 DataPlayerColor = 0;   // 0–3
	constexpr int32 DataPower = 4;
	constexpr int32 DataCut = 5;           // metros acima do pivô
	constexpr int32 DataBurnt = 6;         // carcaça: 1 veículo queimado, 2 prédio (o M_Structure e o M_UnitDust do RTSLook)
	constexpr int32 DataHeat = 7;          // o miolo quente da carcaça: 1 → 0 em WreckFireSeconds
	constexpr int32 DataEmbers = 8;        // os pontinhos de brasa depois: 1 → 0 em EmberSeconds
	constexpr float EmberSeconds = 20.0f;
	constexpr int32 DataDamage = 9;        // 1 − a vida: as marcas de dano no material
	/** A solda na obra: faíscas por segundo e a luz de cada uma (pequena: é detalhe, não clarão). */
	constexpr float SparksPerSecond = 14.0f;
	constexpr float SparkCandela = 7.0e4f;
	/** O prédio destruído afunda até esta fração da altura (o ZH derruba o modelo no chão). */
	constexpr float StructureSinkFraction = 0.42f;
	constexpr float StructureSinkSeconds = 3.5f;

	uint32 NextRandom(uint32& Seed)
	{
		Seed ^= Seed << 13; Seed ^= Seed >> 17; Seed ^= Seed << 5;
		return Seed;
	}

	float Random01(uint32& Seed)
	{
		return float(NextRandom(Seed) & 0xFFFFFF) / float(0x1000000);
	}

	/** O material da obra de um material da base (MI_Body → MI_Body_Build) e o caminho de volta. */
	UMaterialInterface* BuildTwin(UMaterialInterface* M, bool bBuild)
	{
		if (!M)
		{
			return nullptr;
		}
		const FString Name = M->GetName();
		const bool bIsBuild = Name.EndsWith(TEXT("_Build"));
		if (bIsBuild == bBuild || !Name.StartsWith(TEXT("MI_")))
		{
			return M;
		}
		const FString Twin = bBuild ? Name + TEXT("_Build") : Name.LeftChop(6);
		const FString Path = FString::Printf(TEXT("/Game/Structures/Materials/%s.%s"), *Twin, *Twin);
		UMaterialInterface* Out = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		return Out ? Out : M;
	}
}

ARTSUnitActor::ARTSUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// (14/10, a poeira da arte) a unidade fica fora do mapa de sombra do cenário que o RTSTreadDust tira no começo
	Tags.Add(TEXT("RTSUnit"));
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(Body);
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Body);
	// a luz quente da carcaça acima do anel da torre: a 1,8 m e 400 mil cd ela encostava no teto do casco e
	// passava do sol ali (o "ponto de lâmpada" da 1ª gameplay); o miolo quente agora é do material
	FireLight->SetRelativeLocation(FVector(0, 0, 320));
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetIntensity(0.0f);
	FireLight->SetAttenuationRadius(1600.0f);
	FireLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.15f));
	FireLight->SetCastShadows(false);
	FireLight->SetVisibility(false);
}

void ARTSUnitActor::AddParts(const TArray<FString>& Paths, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out)
{
	for (const FString& Pattern : Paths)
	{
		for (const FString& Path : ExpandPattern(Pattern))
		{
			UStaticMesh* Mesh = LoadMesh(Path);
			if (!Mesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS: malha não encontrada: %s"), *Path);
				continue;
			}
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
			USceneComponent* Attach = Parent;
			if (Mesh->GetName().StartsWith(TEXT("wheel")))
			{
				// a roda gira no centro dela: um pivô ali, e a malha deslocada de volta
				const FBox Box = Mesh->GetBoundingBox();
				USceneComponent* Pivot = NewObject<USceneComponent>(this);
				Pivot->SetupAttachment(Parent);
				Pivot->SetRelativeLocation(Box.GetCenter());
				Pivot->RegisterComponent();
				Part->SetRelativeLocation(-Box.GetCenter());
				Attach = Pivot;
				WheelPivots.Add(Pivot);
				WheelRadiusCm.Add(FMath::Max(Box.GetExtent().Z, 10.0f));
			}
			Part->SetupAttachment(Attach);
			Part->SetStaticMesh(Mesh);
			Part->SetReceivesDecals(false);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetCanEverAffectNavigation(false);
			Part->RegisterComponent();
			Out.Add(Part);
		}
	}
}

UStaticMeshComponent* ARTSUnitActor::AddPart(UStaticMesh* Mesh, USceneComponent* Parent)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetReceivesDecals(false);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCanEverAffectNavigation(false);
	Part->RegisterComponent();
	return Part;
}

void ARTSUnitActor::Setup(uint32 InSimId, const FRTSUnitVisual& Visual)
{
	SimId = InSimId;
	SparkSeed = CosmeticHash(InSimId) | 1u;
	RadiusCm = Visual.RadiusM * 100.0f;
	MuzzleHeightCm = Visual.MuzzleHeightM * 100.0f;
	AddParts(Visual.HullMeshes, Body, HullParts);
	AddParts(Visual.TurretMeshes, TurretPivot, TurretParts);
	TurretPivotCm = FVector(Visual.TurretPivotM.X * 100.0f, Visual.TurretPivotM.Y * 100.0f, 0.0f);
	TracksCm = Visual.TracksM * 100.0;
	TurretPivot->SetRelativeLocation(TurretPivotCm);
	for (UStaticMeshComponent* Part : TurretParts)
	{
		Part->SetRelativeLocation(-TurretPivotCm);
	}
	// o que gira sozinho: um pivô no eixo, a malha com a origem nele
	for (const FRTSSpinPart& Spin : Visual.SpinParts)
	{
		if (UStaticMesh* Mesh = LoadMesh(Spin.Mesh))
		{
			USceneComponent* Pivot = NewObject<USceneComponent>(this);
			Pivot->SetupAttachment(Body);
			Pivot->SetRelativeLocation(Spin.AtCm);
			Pivot->RegisterComponent();
			SpinPivots.Add(Pivot);
			SpinParts.Add(AddPart(Mesh, Pivot));
			SpinRpm.Add(Spin.Rpm);
			SpinAngleDeg.Add(float(CosmeticHash(InSimId + SpinPivots.Num()) % 360));
		}
	}
	if (!Visual.ScaffoldMesh.IsEmpty())
	{
		if (UStaticMesh* Mesh = LoadMesh(Visual.ScaffoldMesh))
		{
			Scaffold = AddPart(Mesh, Body);
			Scaffold->SetVisibility(false);
		}
	}
	if (!Visual.BladeMesh.IsEmpty())
	{
		if (UStaticMesh* Mesh = LoadMesh(Visual.BladeMesh))
		{
			BladePivot = NewObject<USceneComponent>(this);
			BladePivot->SetupAttachment(Body);
			BladePivot->SetRelativeLocation(Visual.BladeAtCm);
			BladePivot->RegisterComponent();
			HullParts.Add(AddPart(Mesh, BladePivot));
		}
	}
	if (!Visual.CargoMesh.IsEmpty())
	{
		if (UStaticMesh* Mesh = LoadMesh(Visual.CargoMesh))
		{
			for (const FVector& Slot : Visual.CargoSlotsCm)
			{
				UStaticMeshComponent* Crate = AddPart(Mesh, Body);
				Crate->SetRelativeLocation(Slot);
				Crate->SetVisibility(false);
				CargoParts.Add(Crate);
			}
		}
	}
	// prédio: a área e a altura (a obra corta até ela; as faíscas correm na borda)
	bStructure = !Visual.ScaffoldMesh.IsEmpty();
	if (HullParts.Num() && HullParts[0]->GetStaticMesh())
	{
		const FBox Box = HullParts[0]->GetStaticMesh()->GetBoundingBox();
		HeightCm = FMath::Max(100.0f, float(Box.Max.Z));
		FootprintCm = FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y));
	}
	if (!bStructure && FootprintCm.bIsValid)
	{
		// o contato com o chão (12/10): a mancha escura macia sob o casco (M_UnitContact), um pouco maior que a
		// planta; o decal olha pra baixo (o X dele é a profundidade) e anda e gira com o casco
		static TWeakObjectPtr<UMaterialInterface> ContactMat;
		if (!ContactMat.IsValid())
		{
			ContactMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UnitContact.M_UnitContact"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (ContactMat.IsValid())
		{
			UDecalComponent* Contact = NewObject<UDecalComponent>(this, TEXT("Contact"));
			Contact->SetupAttachment(Body);
			const FVector2D C = FootprintCm.GetCenter();
			const FVector2D E = FootprintCm.GetExtent();
			Contact->SetRelativeLocation(FVector(C.X, C.Y, 0.0));
			Contact->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
			// 90 cm além do casco: a parte que a câmera de cima vê (embaixo do casco, ela fica tapada)
			Contact->DecalSize = FVector(150.0, E.Y + 90.0, E.X + 90.0);
			Contact->SetDecalMaterial(ContactMat.Get());
			Contact->SetFadeScreenSize(0.0f);
			Contact->SortOrder = 5;   // por cima da memória do chão
			Contact->RegisterComponent();
		}
	}
	// a boca do cano: a ponta da malha do cano (as peças saem no lugar, com origem no centro do tanque)
	for (const UStaticMeshComponent* Part : TurretParts)
	{
		if (Part->GetStaticMesh() && Part->GetStaticMesh()->GetName().Contains(TEXT("Barrel")))
		{
			MuzzleForwardCm = Part->GetStaticMesh()->GetBoundingBox().Max.X - TurretPivotCm.X;
		}
	}
}

void ARTSUnitActor::SetPlayerColor(const FLinearColor& Color)
{
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetCustomPrimitiveDataVector4(DataPlayerColor, FVector4(Color.R, Color.G, Color.B, 1.0f));
	}
}

void ARTSUnitActor::SetPartsFloat(int32 Index, float Value)
{
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetCustomPrimitiveDataFloat(Index, Value);
	}
}

void ARTSUnitActor::SetBuildLook(bool bBuild)
{
	if (bBuild == bBuildLook)
	{
		return;
	}
	bBuildLook = bBuild;
	// a obra usa o material que corta acima da altura feita (com a solda no corte); pronto,
	// volta o opaco (o recorte custa no Nanite: só enquanto constrói)
	for (UStaticMeshComponent* Part : HullParts)
	{
		for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
		{
			Part->SetMaterial(Slot, BuildTwin(Part->GetMaterial(Slot), bBuild));
		}
	}
	if (Scaffold)
	{
		Scaffold->SetVisibility(bBuild);
	}
	for (UStaticMeshComponent* Spin : SpinParts)
	{
		Spin->SetVisibility(!bBuild);
	}
	if (!bBuild)
	{
		SetPartsFloat(DataCut, 1000.0f);
		LastCutSent = -1.0f;
	}
}

void ARTSUnitActor::SetStructureState(float InBuildFraction, bool bInPowered)
{
	if (bDead)
	{
		return;
	}
	const bool bWasBuilding = BuildFraction < 1.0f;
	BuildFraction = FMath::Clamp(InBuildFraction, 0.0f, 1.0f);
	bPowered = bInPowered;
	const bool bBuilding = BuildFraction < 1.0f;
	SetBuildLook(bBuilding);
	if (bBuilding)
	{
		// a altura feita, em metros acima do pivô (a laje já vem pronta: ela é o alicerce)
		const float Cut = FMath::Lerp(0.3f, HeightCm / 100.0f + 0.5f, BuildFraction);
		if (FMath::Abs(Cut - LastCutSent) > 0.005f)
		{
			SetPartsFloat(DataCut, Cut);
			LastCutSent = Cut;
		}
		PowerShown = 0.0f;
		PowerOnClock = 0.0f;
	}
	else if (bWasBuilding)
	{
		PowerOnClock = 0.0f;   // inauguração: as luzes acendem piscando
	}
}

void ARTSUnitActor::SetCargo(int32 Boxes)
{
	if (Boxes == CargoShown)
	{
		return;
	}
	CargoShown = Boxes;
	for (int32 K = 0; K < CargoParts.Num(); ++K)
	{
		CargoParts[K]->SetVisibility(K < Boxes);
	}
}

void ARTSUnitActor::SetWorking(bool bInWorking)
{
	bWorking = bInWorking;
}

void ARTSUnitActor::TickStructure(float DeltaSeconds)
{
	// as luzes: acendem piscando como lâmpada de galpão (0,9 s), apagam em 0,3 s
	const bool bLit = bPowered && BuildFraction >= 1.0f;
	if (bLit)
	{
		PowerOnClock += DeltaSeconds;
		const float T = PowerOnClock;
		PowerShown = T >= 0.9f ? 1.0f : (FMath::Frac(T * 7.3f + 0.13f * FMath::Sin(T * 41.0f)) > 0.45f ? FMath::Min(1.0f, 0.35f + T) : 0.05f);
	}
	else
	{
		PowerShown = FMath::Max(0.0f, PowerShown - DeltaSeconds / 0.3f);
		if (BuildFraction >= 1.0f)
		{
			PowerOnClock = 0.0f;
		}
	}
	if (FMath::Abs(PowerShown - LastPowerSent) > 0.01f)
	{
		SetPartsFloat(DataPower, PowerShown);
		LastPowerSent = PowerShown;
	}
	// o que gira: devagar até parar sem energia (o radar do ZH desliga no apagão)
	for (int32 K = 0; K < SpinPivots.Num(); ++K)
	{
		SpinAngleDeg[K] = FMath::Fmod(SpinAngleDeg[K] + SpinRpm[K] * 6.0f * PowerShown * DeltaSeconds, 360.0f);
		SpinPivots[K]->SetRelativeRotation(FRotator(0.0f, SpinAngleDeg[K], 0.0f));
	}
	// a solda na obra: faíscas na borda do corte
	if (BuildFraction < 1.0f && !bFogHidden)
	{
		SparkClock += DeltaSeconds * SparksPerSecond;
		while (SparkClock >= 1.0f)
		{
			SparkClock -= 1.0f;
			const float Side = Random01(SparkSeed) * 4.0f;
			const float Along = FMath::Frac(Side);
			const FVector2D Lo = FootprintCm.Min * 0.8;
			const FVector2D Hi = FootprintCm.Max * 0.8;
			FVector2D P;
			switch (int32(Side))
			{
			case 0: P = FVector2D(FMath::Lerp(Lo.X, Hi.X, Along), Lo.Y); break;
			case 1: P = FVector2D(Hi.X, FMath::Lerp(Lo.Y, Hi.Y, Along)); break;
			case 2: P = FVector2D(FMath::Lerp(Hi.X, Lo.X, Along), Hi.Y); break;
			default: P = FVector2D(Lo.X, FMath::Lerp(Hi.Y, Lo.Y, Along)); break;
			}
			// a solda é nas paredes: acima delas (mastro, antena) não solta faísca no ar
			const float Z = FMath::Clamp(LastCutSent * 100.0f - 10.0f, 30.0f, HeightCm * 0.65f);
			const FVector At = Body->GetComponentTransform().TransformPosition(FVector(P.X, P.Y, Z));
			// o brilho e, em metade delas, a gota caindo: um risco de 25 cm (o traçante de 6 m virava laser, 10/10;
			// só o brilho de 14 cm ficou fraco demais pra ver de cima)
			ARTSEffect::SpawnFlash(GetWorld(), At, 24.0f, SparkCandela, 0.09f, FLinearColor(0.6f, 0.3f, 0.08f));   // sem estourar pra branco (12/10)
			if (Random01(SparkSeed) < 0.6f)
			{
				const FVector Drop(Random01(SparkSeed) * 140.0f - 70.0f, Random01(SparkSeed) * 140.0f - 70.0f, -60.0f - Random01(SparkSeed) * 120.0f);
				ARTSEffect::SpawnStreak(GetWorld(), At, At + Drop, 520.0f, 25.0f, 3.5f, FLinearColor(6.0f, 3.0f, 0.9f));
			}
		}
	}
}

void ARTSUnitActor::SetPose(const FVector& Location, float HullYawDeg, float TurretYawWorldDeg, const FVector& GroundNormal, float DeltaSeconds)
{
	if (bDead)
	{
		return;
	}
	if (bStructure)
	{
		// prédio não balança nem inclina: fica no nível do lote (a laje desce até 3 m)
		SetActorLocation(Location);
		Body->SetWorldRotation(FRotator(0.0f, HullYawDeg, 0.0f));
		return;
	}
	// o Dozer: a lâmina desce e raspa quando ele trabalha, sobe quando anda
	if (BladePivot)
	{
		BladeAmount = FMath::FInterpTo(BladeAmount, bWorking ? 1.0f : 0.0f, DeltaSeconds, 3.0f);
		BladeClock += DeltaSeconds;
		const float Pitch = BladeAmount * (4.0f + 5.0f * FMath::Sin(BladeClock * 2.4f)) - (1.0f - BladeAmount) * 3.0f;
		BladePivot->SetRelativeRotation(FRotator(Pitch, 0.0f, 0.0f));
		// a sirene amarela (Modelos 3D, 13/10, pedido do Felipe): a luz que gira ('spin' do Units.json) só roda na obra,
		// e a cúpula (slot Beacon, que pisca pelo material) acende pelo DataPower, que acompanha a lâmina
		for (int32 K = 0; K < SpinPivots.Num(); ++K)
		{
			SpinAngleDeg[K] = FMath::Fmod(SpinAngleDeg[K] + SpinRpm[K] * 6.0f * BladeAmount * DeltaSeconds, 360.0f);
			SpinPivots[K]->SetRelativeRotation(FRotator(0.0f, SpinAngleDeg[K], 0.0f));
		}
		if (FMath::Abs(BladeAmount - LastPowerSent) > 0.01f)
		{
			SetPartsFloat(DataPower, BladeAmount);
			LastPowerSent = BladeAmount;
		}
	}
	// velocidade e aceleração do casco (do desenho interpolado), pra rodas e suspensão
	const FVector Forward = FRotator(0.0f, HullYawDeg, 0.0f).Vector();
	const FVector Right = FRotator(0.0f, HullYawDeg + 90.0f, 0.0f).Vector();
	float ForwardAccel = 0.0f;
	float LateralAccel = 0.0f;
	if (bHasLastPose && DeltaSeconds > 1e-4f)
	{
		const FVector Velocity = (Location - LastLocation) / DeltaSeconds;
		ForwardAccel = float(FVector::DotProduct(Velocity - LastVelocity, Forward)) / 100.0f;   // m/s ganhos neste quadro
		// na curva, o peso vai pra fora: aceleração centrípeta = v · ω
		const float Speed = float(FVector::DotProduct(Velocity, Forward)) / 100.0f;
		LateralAccel = Speed * FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(LastYawDeg, HullYawDeg) / DeltaSeconds);
		LastVelocity = Velocity;
		// rodas: giram pela distância andada no chão
		const float Travel = float(FVector::DotProduct(Location - LastLocation, Forward));
		if (WheelPivots.Num())
		{
			WheelAngleDeg = FMath::Fmod(WheelAngleDeg + FMath::RadiansToDegrees(Travel / WheelRadiusCm[0]), 360.0f);
			for (int32 K = 0; K < WheelPivots.Num(); ++K)
			{
				const float Scale = WheelRadiusCm[0] / WheelRadiusCm[K];
				WheelPivots[K]->SetRelativeRotation(FRotator(-WheelAngleDeg * Scale, 0.0f, 0.0f));
			}
		}
	}
	LastLocation = Location;
	LastYawDeg = HullYawDeg;
	bHasLastPose = true;
	StepSuspension(DeltaSeconds, ForwardAccel, LateralAccel);

	SetActorLocation(Location);
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal);
	TiltSmoothed = FQuat::Slerp(TiltSmoothed, Tilt, 1.0f - FMath::Exp(-DeltaSeconds * 8.0f));
	// arrancou: o nariz sobe (pitch +); freou: desce; na curva, rola pra fora
	const FQuat Yaw = FRotator(0.0f, HullYawDeg, 0.0f).Quaternion();
	const FQuat Sway = FRotator(SuspPitch, 0.0f, SuspRoll).Quaternion();
	Body->SetWorldRotation(TiltSmoothed * Yaw * Sway);
	TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg), 0.0f));
	TurretPivot->SetRelativeLocation(TurretPivotCm + FVector(-RecoilCm, 0.0f, 0.0f));
}

FVector ARTSUnitActor::MuzzleLocation() const
{
	return TurretPivot->GetComponentTransform().TransformPosition(FVector(MuzzleForwardCm, 0.0f, MuzzleHeightCm));
}

FVector ARTSUnitActor::MuzzleDirection() const
{
	return TurretPivot->GetForwardVector();
}

void ARTSUnitActor::StepSuspension(float DeltaSeconds, float ForwardAccel, float LateralAccel)
{
	// a mola do ZH roda em passos de 1/30 s: ângulo-alvo pela aceleração, depois
	// velocidade = velocidade·amortecimento + (alvo − ângulo)·rigidez
	SuspClock += DeltaSeconds;
	// arrancada e freada são instantâneas (Acceleration 1000 no ZH): viram impulso na mola
	// (o nariz sobe ao arrancar, desce ao frear); a curva é contínua: vira ângulo-alvo de rolagem
	SuspPitchRate += FMath::Clamp(ForwardAccel * ForwardAccelPitchFactor * ImpulseDegPerMps, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	const float PitchTarget = 0.0f;
	const float RollTarget = FMath::Clamp(LateralAccel * LateralAccelRollFactor * 0.1f, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	int32 Steps = 0;
	while (SuspClock >= SuspStep && Steps < 4)
	{
		SuspClock -= SuspStep;
		++Steps;
		SuspPitchRate = SuspPitchRate * PitchDamping + (PitchTarget - SuspPitch) * PitchStiffness;
		SuspRollRate = SuspRollRate * RollDamping + (RollTarget - SuspRoll) * RollStiffness;
		SuspPitch = FMath::Clamp(SuspPitch + SuspPitchRate, -AccelPitchLimitDeg, AccelPitchLimitDeg);
		SuspRoll = FMath::Clamp(SuspRoll + SuspRollRate, -AccelPitchLimitDeg, AccelPitchLimitDeg);
	}
	if (Steps == 4)
	{
		SuspClock = 0.0f;
	}
}

void ARTSUnitActor::PlayFire()
{
	RecoilCm = 30.0f;
	// o tranco do tiro: o canhão empurra o casco pra trás, pro lado de onde a torre aponta
	const float Rel = FMath::DegreesToRadians(TurretPivot->GetRelativeRotation().Yaw);
	SuspPitchRate += FireKickDeg * FMath::Cos(Rel);
	SuspRollRate -= FireKickDeg * FMath::Sin(Rel);
	// o clarão de boca provisório (o de verdade é da sessão de explosões): miolo amarelo-laranja, borda laranja.
	// Com 3,0/2,0/0,9 (~380 mil nits) virava uma bola branca chapada na exposição do look (vila_tiro1, 12/10)
	ARTSEffect::SpawnFlash(GetWorld(), MuzzleLocation() + MuzzleDirection() * 60.0f, 60.0f, 1.5e6f, 0.09f, FLinearColor(0.35f, 0.16f, 0.04f));
}

void ARTSUnitActor::SetDamage(float Damage)
{
	// as marcas de dano (o M_Structure: placas de tinta queimada e de fuligem crescendo com o dano, a partir
	// dos 70% de vida, como o modelo danificado do ZH); só quando mudar, pra não mexer no dado à toa
	// (14/10, o Felipe: no veículo as placas liam camuflagem pintada por cima, "um negócio porco"; fora até ter um bem
	// feito; o prédio segue com elas)
	if (bDead || !bStructure || FMath::Abs(Damage - DamageShown) < 0.02f)
	{
		return;
	}
	DamageShown = Damage;
	SetPartsFloat(DataDamage, Damage);
}

void ARTSUnitActor::SetSilhouette(int32 Stencil)
{
	// todas as peças do veículo (13/10, o Felipe: as caixas da carga fora da lista tapavam o caminhão e o contorno
	// pintava de vermelho em cima delas)
	for (TArray<TObjectPtr<UStaticMeshComponent>>* List : { &HullParts, &TurretParts, &SpinParts, &CargoParts })
	{
		for (UStaticMeshComponent* Part : *List)
		{
			if (Part)
			{
				Part->SetRenderCustomDepth(Stencil > 0);
				Part->SetCustomDepthStencilValue(FMath::Clamp(Stencil, 0, 255));
			}
		}
	}
}

void ARTSUnitActor::SetFogHidden(bool bHide)
{
	if (bHide != bFogHidden)
	{
		bFogHidden = bHide;
		SetActorHiddenInGame(bHide);
	}
}

void ARTSUnitActor::SetWreckLive(bool bLive)
{
	if (bLive != bWreckLive)
	{
		bWreckLive = bLive;
		if (bDead)
		{
			FireLight->SetVisibility(bLive && bStructure && DeathAge < WreckFireSeconds);
		}
	}
}

void ARTSUnitActor::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathAge = 0.0f;
	SetSilhouette(0);   // a carcaça não tem silhueta
	Char();
	// o clarão e a bola são da explosão (BlastFx, a Carga Pesada aprovada); aqui só a carcaça
	FireLight->SetVisibility(bWreckLive && bStructure);   // (v2: o veículo sem fogo; o prédio com)
	if (bStructure)
	{
		// a obra destruída fica do tamanho que estava (o recorte continua); o andaime cai
		if (Scaffold)
		{
			Scaffold->SetVisibility(false);
		}
		SetPartsFloat(DataPower, 0.0f);
		FireLight->SetRelativeLocation(FVector(0.0f, 0.0f, HeightCm * 0.4f));
		FireLight->SetAttenuationRadius(FMath::Max(2400.0f, FootprintCm.GetSize().Size() * 1.2f));
		return;
	}
	// a torre voa em ~1 de cada 3 mortes (munição detonando), não em toda (Vanguard ADR-0027)
	const uint32 H = CosmeticHash(SimId);
	// a v2 (13/10, a do vídeo aprovado do BlastLab, BlastLabSimulation.cpp): a torre voa em toda morte, sobe a 12,5 m/s e
	// vai pro lado a 3 m/s (cai a ~7,7 m), em cambalhota e rolamento a 300°/s. O lado é perpendicular à câmera (±20°), o
	// primeiro de 5 (os dois lados, as diagonais pra trás e pra trás) com 3,5 m de folga até a peça grande mais perto
	// (mais de 0,8 m de altura e 1,5 m de lado, até 40 m, fora este), senão o de maior folga: sem isso ela caía encostada
	// no tanque vizinho
	if (TurretParts.Num())
	{
		constexpr float TurretUp = 1250.0f;
		constexpr float TurretSide = 300.0f;
		constexpr float TurretSpinDeg = 300.0f;
		FVector View(1.0, 0.0, 0.0);
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			View = (GetActorLocation() - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal2D();
		}
		FVector Side = FVector::CrossProduct(FVector::UpVector, View).GetSafeNormal();
		Side = ((H & 1u) ? Side : -Side).RotateAngleAxis(float((H >> 1) % 41u) - 20.0f, FVector::UpVector);
		const FVector P0 = TurretPivot->GetComponentLocation();
		const float LandCm = TurretSide * 2.0f * TurretUp / GravityCm;
		const auto Clearance = [&](const FVector& Dir)
		{
			const FVector Land = P0 + Dir * LandCm;
			double Best = 1e9;
			for (TActorIterator<ARTSUnitActor> It(GetWorld()); It; ++It)
			{
				if (*It == this)
				{
					continue;
				}
				FVector C, E;
				It->GetActorBounds(false, C, E);
				if (E.Z < 40.0 || FMath::Max(E.X, E.Y) < 75.0 || FVector::Dist2D(C, Land) > 4000.0)
				{
					continue;
				}
				const double Dx = FMath::Max(FMath::Abs(Land.X - C.X) - E.X, 0.0);
				const double Dy = FMath::Max(FMath::Abs(Land.Y - C.Y) - E.Y, 0.0);
				Best = FMath::Min(Best, FMath::Sqrt(Dx * Dx + Dy * Dy));
			}
			return Best;
		};
		const FVector Dirs[] = { Side, -Side, (Side + View).GetSafeNormal(), (-Side + View).GetSafeNormal(), View };
		int32 Pick = -1;
		int32 BestI = 0;
		double BestC = -1.0;
		for (int32 I = 0; I < 5; ++I)
		{
			const double C = Clearance(Dirs[I]);
			if (Pick < 0 && C >= 350.0)
			{
				Pick = I;
			}
			if (C > BestC)
			{
				BestC = C;
				BestI = I;
			}
		}
		const FVector Dir = Dirs[Pick >= 0 ? Pick : BestI];
		bTurretFlying = true;
		TurretVelocity = FVector(0.0f, 0.0f, TurretUp) + Dir * TurretSide;
		const float F1 = float((H >> 8) & 0xFFu) / 255.0f;
		const float F2 = float((H >> 16) & 0xFFu) / 255.0f;
		TurretSpin = FVector(FMath::Lerp(0.6f, 1.0f, F1) * ((H & 0x10000000u) ? 1.0f : -1.0f), FMath::Lerp(0.4f, 0.8f, F2), 0.1f).GetSafeNormal()
			* FMath::DegreesToRadians(TurretSpinDeg);
		TurretQ = TurretPivot->GetComponentQuat();
		TurretPivot->SetUsingAbsoluteLocation(true);
		TurretPivot->SetUsingAbsoluteRotation(true);
	}
}

void ARTSUnitActor::Char()
{
	// o queimado por dado de primitiva (6), o que o M_UnitDust do plugin RTSLook lê: escurece e tira
	// o brilho sem instância dinâmica (combinado com a sessão de gráficos, 11/10)
	SetPartsFloat(DataBurnt, bStructure ? 2.0f : 1.0f);
	SetPartsFloat(DataHeat, 0.0f);   // (v2, 13/10: o veículo só carbonizado, sem o miolo quente)
	SetPartsFloat(DataEmbers, 0.0f);
	// carcaça queimada: escurece a cor e tira o brilho (parâmetros do M_UnitPBR)
	auto Burn = [](UStaticMeshComponent* Part)
	{
		for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(Slot))
			{
				MID->SetVectorParameterValue(TEXT("BaseColorFactor"), FLinearColor(0.07f, 0.064f, 0.058f, 1.0f));
				MID->SetScalarParameterValue(TEXT("RoughnessFactor"), 0.95f);
				MID->SetScalarParameterValue(TEXT("MetallicFactor"), 0.0f);
				// o material dos provisórios (o tanque do Look e os prédios): os mesmos, com outro nome
				MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.07f, 0.064f, 0.058f, 1.0f));
				MID->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
				MID->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
				MID->SetScalarParameterValue(TEXT("PlayerMask"), 0.0f);
			}
		}
	};
	for (UStaticMeshComponent* Part : HullParts)
	{
		Burn(Part);
	}
	for (UStaticMeshComponent* Part : TurretParts)
	{
		Burn(Part);
	}
	for (UStaticMeshComponent* Part : SpinParts)
	{
		Burn(Part);
	}
	if (bStructure)
	{
		// o M_Structure não tem os parâmetros do M_UnitPBR: a cor e o brilho vêm da instância dinâmica
		auto BurnStructure = [](UStaticMeshComponent* Part)
		{
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(Slot))
				{
					MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.035f, 0.032f, 0.029f, 1.0f));
					MID->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
					MID->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
					MID->SetScalarParameterValue(TEXT("PlayerMask"), 0.0f);
					MID->SetScalarParameterValue(TEXT("EmissiveNits"), 0.0f);
					MID->SetScalarParameterValue(TEXT("Weld"), 0.0f);
				}
			}
		};
		for (UStaticMeshComponent* Part : HullParts)
		{
			BurnStructure(Part);
		}
		for (UStaticMeshComponent* Part : SpinParts)
		{
			BurnStructure(Part);
		}
	}
}

void ARTSUnitActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RecoilCm = FMath::Max(0.0f, RecoilCm - DeltaSeconds * 120.0f);
	if (!bDead)
	{
		if (bStructure)
		{
			TickStructure(DeltaSeconds);
		}
		return;
	}
	DeathAge += DeltaSeconds;
	if (bStructure && DeathAge < StructureSinkSeconds + 0.1f)
	{
		// desaba: afunda no chão (mais rápido no começo) e inclina um pouco pro lado
		const float T = FMath::Clamp(DeathAge / StructureSinkSeconds, 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Square(1.0f - T);
		SinkCm = HeightCm * StructureSinkFraction * Ease;
		const uint32 H = CosmeticHash(SimId);
		const float Lean = (float(H % 5) - 2.0f) * 1.2f * Ease;
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, -SinkCm));
		Body->SetRelativeRotation(FRotator(Lean, Body->GetRelativeRotation().Yaw, -Lean * 0.6f));
	}
	if (bTurretFlying)
	{
		TurretVelocity.Z -= GravityCm * DeltaSeconds;
		FVector P = TurretPivot->GetComponentLocation() + TurretVelocity * DeltaSeconds;
		const float SpinRad = TurretSpin.Size() * DeltaSeconds;
		if (SpinRad > 0.0f)
		{
			TurretQ = FQuat(TurretSpin.GetSafeNormal(), SpinRad) * TurretQ;
		}
		const float Floor = float(GetActorLocation().Z) + 60.0f;
		if (P.Z < Floor && TurretVelocity.Z < 0.0f)
		{
			// quica uma vez, sem força, e fica; a pose estável mais perto: o eixo de cima pra cima ou pra baixo (uma
			// torre larga e chata não fica de lado), mantendo o giro em volta da vertical
			P.Z = Floor;
			TurretVelocity = FVector(TurretVelocity.X * 0.4f, TurretVelocity.Y * 0.4f, -TurretVelocity.Z * 0.2f);
			TurretSpin *= 0.35f;
			if (TurretVelocity.Size() < 250.0f)
			{
				bTurretFlying = false;
				bTurretSettling = true;
				TurretSettle = 0.3f;
				const FVector Up = TurretQ.GetUpVector();
				TurretRestQ = FQuat::FindBetweenNormals(Up, Up.Z >= 0.0f ? FVector::UpVector : -FVector::UpVector) * TurretQ;
			}
		}
		TurretPivot->SetWorldLocationAndRotation(P, TurretQ);
	}
	else if (bTurretSettling)
	{
		TurretSettle = FMath::Max(TurretSettle - DeltaSeconds, 0.0f);
		TurretQ = FQuat::Slerp(TurretQ, TurretRestQ, FMath::Clamp(DeltaSeconds / FMath::Max(TurretSettle + DeltaSeconds, 1e-3f), 0.0f, 1.0f));
		TurretPivot->SetWorldRotation(TurretQ);
		bTurretSettling = TurretSettle > 0.0f;
	}
	// a v2 (13/10, o Felipe pelo vídeo aprovado do BlastLab): o veículo fica só carbonizado, sem fogo, luz nem brasa; o
	// prédio segue com o fogo que tremula e se apaga (a tremida de ruído, 1–3 Hz: o seno lia como lâmpada piscando)
	if (!bStructure)
	{
		if (!bTurretFlying && !bTurretSettling)
		{
			SetActorTickEnabled(false);
		}
		return;
	}
	const float Fade = FMath::Clamp(1.0f - DeathAge / WreckFireSeconds, 0.0f, 1.0f);
	const float Flicker = 0.75f + 0.25f * (0.5f + 0.5f * FMath::PerlinNoise1D(DeathAge * 2.1f + float(GetUniqueID() % 97)));
	FireLight->SetIntensity(6.0e4f * Fade * Fade * Flicker);
	if (Fade <= 0.0f)
	{
		FireLight->SetVisibility(false);
	}
	if (DeathAge > WreckFireSeconds)
	{
		SetActorTickEnabled(false);
	}
}
