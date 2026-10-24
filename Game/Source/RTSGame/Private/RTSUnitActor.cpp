#include "RTSUnitActor.h"
#include "BlastFxDirector.h"
#include "Components/DecalComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTSConvert.h"
#include "RTSEffect.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSTreadDust.h"
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
	// o empurrão do acerto na mola (o mesmo impulso do tranco do tiro, ~1,5° de pico): o casco cede pro lado do tiro
	constexpr float HitKickDeg = 1.2f;
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
	/** (22/10) A carcaça de veículo afunda até o topo ficar isto abaixo do chão (a inclinação de 2–3° levanta uma ponta ~20 cm). */
	constexpr float WreckSinkMarginCm = 30.0f;

	// (v4, 23/10) a torre solta com física de corpo rígido (RTSLooseTurret.h). O lançamento é o aprovado da v2 (BlastLab, 13/10:
	// 12,5 m/s pra cima, 3 m/s pro lado, 300°/s, a saída 0,067 s depois do clarão; a 18 m/s ela caía em cima do casco), agora
	// com ±15% na velocidade e ±20% no giro por unidade: a faixa dá 6–10,5 m de subida, dentro dos ~2 andares (15–20 m) dos
	// relatos do "jack-in-the-box" (docs/EXPLOSOES.md) e legível na câmera de cima
	constexpr float TurretUpCmS = 1250.0f;
	constexpr float TurretSideCmS = 300.0f;
	constexpr float TurretSpinDegS = 300.0f;
	constexpr float TurretSpeedJitter = 0.15f;
	constexpr float TurretSpinJitter = 0.20f;
	constexpr float TurretDelayS = 0.067f;
	/** O centro da torre pousada de pé fica ~60 cm acima do chão (o anel a 1,54 m, o centro a 2,16 m): a queda prevista. */
	constexpr float TurretRestComCm = 60.0f;
	/** A folga pedida na queda prevista (3,5 m, a da v2) e o corredor depois dela (no protótipo ela anda 1,8–3 m de mediana
	 *  depois do 1º toque): a folga vale na queda, no meio e no fim do corredor. */
	constexpr float TurretClearanceCm = 350.0f;
	constexpr float TurretSlideCorridorCm = 300.0f;
	/** Os obstáculos da torre: as unidades, carcaças, prédios e torres pousadas até 20 m da queda prevista. */
	constexpr float TurretObstacleRadiusCm = 2000.0f;
	/** A carcaça tirada logo depois da morte espera a torre parar no máximo até aqui, em segundos de VOO da própria torre (o
	 *  relógio dela: para na pausa e anda em câmera lenta num tranco; no tempo real, abaixo de ~12 qps a carcaça afundava com
	 *  a torre ainda rolando, a revisão de 23/10): o teto de 10 s de voo do RTSLooseTurret e folga. Só um apoio que não para
	 *  de mexer passaria disso. */
	constexpr float TurretWreckWaitMaxS = 12.0f;
	// o Rn do chão (a tabela da Rocscience no RocFall): solo mole e talude 0,28–0,33 (a areia e a terra batida, 0,30), asfalto
	// 0,37–0,42 (0,40) e rocha 0,33–0,37 (0,35)
	constexpr float SandRn = 0.30f;
	constexpr float AsphaltRn = 0.40f;
	constexpr float RockRn = 0.35f;

	/** -TurretPhys=0: o voo da v2 (pro A/B com a mesma história, tools/compare.py e vídeo). */
	bool TurretPhysicsOn()
	{
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("TurretPhys="), V); return V != 0; }();
		return bOn;
	}

	/** (23/10, v4) -WheelDust=0: as rodas sem a poeira e sem a marca no chão (o A/B do custo; só depuração). */
	bool WheelDustOn()
	{
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("WheelDust="), V); return V != 0; }();
		return bOn;
	}

	TAutoConsoleVariable<int32> CVarTurretDebug(TEXT("rts.TurretDebug"), 0,
		TEXT("(v4) A torre solta: 1 desenha os pontos de contato (verde solto, vermelho encostado; parada, por 8 s)."));

	const FRTSBakedMap* BakedMapOf(const UWorld* World)
	{
		const URTSMatchSubsystem* Match = World ? World->GetSubsystem<URTSMatchSubsystem>() : nullptr;
		return Match ? Match->BakedMap() : nullptr;
	}

	/** O chão da torre solta: o mapa assado (bilinear de 2 m, o mesmo em que os cascos ficam), lido a cada passo; o Rn pelo tipo
	 *  de chão (o surface.png do WorldForge). Sem o mapa (fora da partida), um plano na altura dada. */
	class FBakedTurretGround final : public IRTSLooseTurretGround
	{
	public:
		FBakedTurretGround(const FRTSBakedMap* InMap, float InFlatZ) : Map(InMap), FlatZ(InFlatZ) {}
		virtual float HeightCm(const FVector2D& Cm) const override
		{
			return Map ? Map->GroundZCm(Cm) : FlatZ;
		}
		virtual FVector Normal(const FVector2D& Cm) const override
		{
			return Map ? Map->GroundNormal(Cm) : FVector::UpVector;
		}
		virtual float Restitution(const FVector2D& Cm) const override
		{
			if (!Map)
			{
				return SandRn;
			}
			const FVector4f Kind = Map->SurfaceAt(Cm);
			return SandRn + FMath::Clamp(Kind.X, 0.0f, 1.0f) * (AsphaltRn - SandRn) + FMath::Clamp(Kind.Z, 0.0f, 1.0f) * (RockRn - SandRn);
		}

	private:
		const FRTSBakedMap* Map = nullptr;
		float FlatZ = 0.0f;
	};

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
	TurretYawOffsetDeg = Visual.TurretYawOffsetDeg;
	if (Visual.TurretBoxM.IsValid)
	{
		TurretBodyBoxCm = FBox(Visual.TurretBoxM.Min * 100.0, Visual.TurretBoxM.Max * 100.0);
	}
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
	if (!bStructure)
	{
		SetupWheels(Visual);
		SetupLauncher(Visual);
	}
}

void ARTSUnitActor::SetupWheels(const FRTSUnitVisual& Visual)
{
	// (23/10, v4; Plugins/RTSTreadDust/INTEGRACAO.md "As rodas") a geometria sai das malhas wheel_* (as peças vêm no quadro do veículo: o
	// centro da caixa dá o eixo, a meia altura o raio, e o lado de dentro da caixa é a borracha, porque o cubo e o aro saem pra fora); a
	// largura do pneu e a física, do bloco "wheels" (a arte final traz rodas novas: a geometria acompanha sozinha)
	if (Visual.WheelTyreWidthM <= 0.0f || !WheelDustOn())
	{
		return;
	}
	float RearX = TNumericLimits<float>::Max(), FrontX = -TNumericLimits<float>::Max(), InnerY = TNumericLimits<float>::Max(), Radius = 0.0f;
	int32 Count = 0;
	for (const UStaticMeshComponent* Part : HullParts)
	{
		const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
		if (!Mesh || !Mesh->GetName().StartsWith(TEXT("wheel")))
		{
			continue;
		}
		const FBox B = Mesh->GetBoundingBox();
		const FVector Center = B.GetCenter();
		RearX = FMath::Min(RearX, float(Center.X));
		FrontX = FMath::Max(FrontX, float(Center.X));
		InnerY = FMath::Min(InnerY, float(Center.Y >= 0.0 ? B.Min.Y : -B.Max.Y));
		Radius = FMath::Max(Radius, float(B.GetExtent().Z));
		++Count;
	}
	if (Count < 2 || InnerY <= 0.0f || Radius <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS: %s tem o bloco \"wheels\" mas não as malhas wheel_*: sem a poeira e a marca das rodas"), *Visual.Key);
		return;
	}
	TyreWidthCm = Visual.WheelTyreWidthM * 100.0f;
	WheelRearXCm = RearX;
	WheelFrontXCm = FrontX;
	WheelHalfGaugeCm = InnerY + 0.5f * TyreWidthCm;
	TyreRadiusCm = Radius;
	WheelDustScale = Visual.WheelDustScale;
	WheelDustAmount = Visual.WheelDustAmount;
	WheelDustSpacing = Visual.WheelDustSpacing;
	WheelDustTrailS = FMath::Max(Visual.WheelDustTrailS, 0.0f);
	TyreMarkDark = Visual.WheelMarkDark;
	TyreMarkDepth = Visual.WheelMarkDepth;
	TyreMarkLug = Visual.WheelMarkLug;
	TyreMarkSegmentK = FMath::Max(Visual.WheelMarkSegmentM, 0.25f);   // × o trecho de 1 m da lagarta
	bWheelDust = true;
	UE_LOG(LogTemp, Log, TEXT("RTS: %s, as rodas: eixo de trás em %.2f m, o da frente em %.2f m, bitola %.2f m, pneu de %.2f × %.2f m; a poeira × %.2f de lado e × %.2f de massa, a marca × %.2f de escuro e × %.2f de fundo"),
		*Visual.Key, RearX / 100.0f, FrontX / 100.0f, 2.0f * WheelHalfGaugeCm / 100.0f, 2.0f * Radius / 100.0f, TyreWidthCm / 100.0f,
		WheelDustScale, WheelDustAmount, TyreMarkDark, TyreMarkDepth);
}

void ARTSUnitActor::SetupLauncher(const FRTSUnitVisual& Visual)
{
	// (23/10, v4) o lançador que levanta (o TEL do míssil de cruzeiro: o berço dos tubos se levanta pela dobradiça de trás até o ângulo de
	// tiro). A malha do bloco "launcher" sai do pivô da torre pra um pivô na dobradiça (filho dele: gira com a torre e levanta no Y, o
	// pitch da Unreal: + levanta a boca, que olha pra +X). A mesa e as vigas (a outra peça da torre) ficam
	if (Visual.LauncherMesh.IsEmpty())
	{
		return;
	}
	UStaticMeshComponent* Pod = nullptr;
	for (UStaticMeshComponent* Part : TurretParts)
	{
		if (Part && Part->GetStaticMesh() && Part->GetStaticMesh()->GetName() == Visual.LauncherMesh)
		{
			Pod = Part;
			break;
		}
	}
	if (!Pod)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS: %s: o lançador %s não está nas peças da torre (ela só gira)"), *Visual.Key, *Visual.LauncherMesh);
		return;
	}
	LauncherHingeCm = Visual.LauncherHingeCm;
	LauncherTubesCm = Visual.LauncherTubesCm;
	LauncherFirePitchDeg = Visual.LauncherFirePitchDeg;
	LauncherRaiseDps = FMath::Max(Visual.LauncherRaiseDps, 1.0f);
	LauncherAccelDps2 = FMath::Max(Visual.LauncherAccelDps2, 1.0f);
	LauncherBrakeDps2 = FMath::Max(Visual.LauncherBrakeDps2, 1.0f);
	LauncherLowerDps = FMath::Max(Visual.LauncherLowerDps, 1.0f);
	LauncherLowerDelayS = Visual.LauncherLowerDelayS;
	LauncherStopHz = FMath::Max(Visual.LauncherStopHz, 0.5f);
	LauncherStopZeta = FMath::Clamp(Visual.LauncherStopZeta, 0.05f, 2.0f);
	ElevPivot = NewObject<USceneComponent>(this);
	ElevPivot->SetupAttachment(TurretPivot);
	ElevPivot->SetRelativeLocation(LauncherHingeCm - TurretPivotCm);
	ElevPivot->RegisterComponent();
	LauncherParts.Add(Pod);
	// as pontas dos mísseis (as malhas do bloco "noses", peças da torre: a cor, a névoa, a carcaça e a torre que voa já passam por
	// elas): levantam com o berço, e cada uma é do tubo com a boca mais perto do centro dela (o Y do desenho espelha no jogo)
	LauncherNoses.Init(nullptr, LauncherTubesCm.Num());
	for (UStaticMeshComponent* Part : TurretParts)
	{
		const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
		if (!Mesh || !Visual.LauncherNoseMeshes.Contains(Mesh->GetName()))
		{
			continue;
		}
		LauncherParts.Add(Part);
		const FVector Center = Mesh->GetBoundingBox().GetCenter();
		int32 Best = INDEX_NONE;
		for (int32 K = 0; K < LauncherTubesCm.Num(); ++K)
		{
			if (!LauncherNoses[K] && (Best == INDEX_NONE || FVector::DistSquared(Center, LauncherTubesCm[K]) < FVector::DistSquared(Center, LauncherTubesCm[Best])))
			{
				Best = K;
			}
		}
		if (Best != INDEX_NONE)
		{
			LauncherNoses[Best] = Part;
		}
	}
	for (UStaticMeshComponent* Part : LauncherParts)
	{
		Part->AttachToComponent(ElevPivot, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeLocation(-LauncherHingeCm);
	}
	if (Visual.LauncherNoseMeshes.Num() && LauncherParts.Num() == 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS: %s: as pontas dos mísseis do lançador não estão nas peças da torre (ficam no tubo depois do tiro)"), *Visual.Key);
	}
}

void ARTSUnitActor::SetLauncherAim(bool bEngaged, float AimSeconds, float PreAttackSeconds, bool bWeaponReady)
{
	bLauncherAim = bEngaged;
	// o relógio da simulação: subindo (há alvo) ou caindo (a volta da torre); parado (cheio, ou entre os passos que ele desce) segue
	// como estava
	const float Aim = FMath::Max(AimSeconds, 0.0f);
	if (Aim < LauncherAimS - 1.0e-4f && !bLauncherAimFalling)
	{
		bLauncherAimFalling = true;
		UE_LOG(LogTemp, Log, TEXT("RTS: o lançador %u: o relógio da mira começou a cair em %.2f s (o lançador a %.1f°)"), SimId, Aim, LauncherPitch);
	}
	else if (Aim > LauncherAimS + 1.0e-4f && bLauncherAimFalling)
	{
		bLauncherAimFalling = false;
		UE_LOG(LogTemp, Log, TEXT("RTS: o lançador %u: o relógio da mira voltou a subir em %.2f s (o lançador a %.1f°)"), SimId, Aim, LauncherPitch);
	}
	LauncherAimS = Aim;
	LauncherPreAttackS = FMath::Max(PreAttackSeconds, 0.0f);
	bLauncherSimClock |= AimSeconds > 0.0f;
	// a recarga da rajada (o ClipReloadTime): com todos os tubos atirados, a arma sem espera = os mísseis de volta nos tubos (com a
	// rajada parada no meio, o alvo perdido entre os tiros, o tubo vazio fica vazio: a simulação guarda o que sobrou)
	const uint32 All = LauncherTubesCm.Num() > 0 ? (1u << FMath::Min(LauncherTubesCm.Num(), 31)) - 1u : 0u;
	if (bWeaponReady && All != 0u && (LauncherFired & All) == All)
	{
		LauncherFired = 0u;
		ShowLauncherNoses();
	}
}

int32 ARTSUnitActor::NoteLauncherShot(uint32 MissileId, const FVector& SimCm, const FVector& SimDir, FVector& OutMouthCm, FVector& OutMouthDir)
{
	OutMouthCm = SimCm;
	OutMouthDir = SimDir;
	const int32 Tube = LauncherTubeOnSide(SimCm);
	if (Tube == INDEX_NONE || !LauncherTubeWorld(Tube, OutMouthCm, OutMouthDir))
	{
		return INDEX_NONE;
	}
	FLaunchBlend& Blend = LaunchBlends[LaunchBlendNext];
	LaunchBlendNext = (LaunchBlendNext + 1) % UE_ARRAY_COUNT(LaunchBlends);
	Blend.Id = MissileId;
	Blend.OffsetCm = OutMouthCm - SimCm;
	Blend.Dir = OutMouthDir;
	UE_LOG(LogTemp, Log, TEXT("RTS: o lançador %u atirou o míssil %u do tubo %d: a boca desenhada a %.0f cm do ponto da simulação (o casco inclinado %.1f°, o lançador a %.1f°)"),
		SimId, MissileId, Tube, Blend.OffsetCm.Size(), Body ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Body->GetUpVector().Z, -1.0, 1.0))) : 0.0, LauncherPitch);
	// a rajada nova com todos atirados: a recarga acabou e o tiro saiu no mesmo passo (o quadro não viu a arma pronta)
	const uint32 All = (1u << FMath::Min(LauncherTubesCm.Num(), 31)) - 1u;
	if ((LauncherFired & All) == All)
	{
		LauncherFired = 0u;
	}
	LauncherFired |= 1u << Tube;
	LauncherTube = Tube;
	ShowLauncherNoses();
	return Tube;
}

void ARTSUnitActor::ShowLauncherNoses()
{
	for (int32 K = 0; K < LauncherNoses.Num(); ++K)
	{
		if (LauncherNoses[K])
		{
			LauncherNoses[K]->SetVisibility((LauncherFired & (1u << K)) == 0u);
		}
	}
}

void ARTSUnitActor::StepLauncher(float DeltaSeconds)
{
	if (!ElevPivot || DeltaSeconds <= 0.0f)
	{
		return;
	}
	// o ângulo pedido, pelo relógio da mira da simulação (o AIM do TurretAI do ZH): com o relógio subindo (ou cheio), o de tiro desde o
	// começo da mira, e o cilindro faz o perfil dele (45° em 0,75 s, a média do TurretPitchRate 60 do ZH, e o berço assentado antes do
	// tiro em 1 s: os 0,25 s do PreAttackDelay; pedir "60°/s × relógio" deixava o cilindro, que acelera, uns 10° atrás da rampa, e o
	// tiro saía com o berço a 42°); caindo, sem alvo depois da volta da torre, o de tiro × a fração do relógio: começa a descer junto
	// com ele, 2 s depois do último alvo, e deita com ele em zero, na metade da velocidade dele (o "60°/s × relógio" ficava em cima mais
	// 0,5 s, o relógio de 1,0 a 0,75 s). Sem o relógio lá, em cima enquanto mira e até LauncherLowerDelayS depois do último alvo (a
	// rajada e a recarga dela ficam em cima)
	LauncherSinceAim = bLauncherAim ? 0.0f : LauncherSinceAim + DeltaSeconds;
	float Target = LauncherSinceAim < LauncherLowerDelayS ? LauncherFirePitchDeg : 0.0f;
	if (bLauncherSimClock)
	{
		Target = bLauncherAimFalling && LauncherPreAttackS > 0.0f
			? LauncherFirePitchDeg * FMath::Clamp(LauncherAimS / LauncherPreAttackS, 0.0f, 1.0f)
			: (LauncherAimS > 0.0f ? LauncherFirePitchDeg : 0.0f);
	}
	if (Target == LauncherAct && LauncherActRate == 0.0f && FMath::Abs(LauncherPitch - Target) < 1.0e-3f && FMath::Abs(LauncherRate) < 1.0e-3f)
	{
		if (bLauncherUp && Target == 0.0f)
		{
			bLauncherUp = false;
			UE_LOG(LogTemp, Log, TEXT("RTS: o lançador %u deitou (o relógio da mira em %.2f s)"), SimId, LauncherAimS);
		}
		return;                                    // parado no lugar (deitado ou em cima)
	}
	bLauncherUp |= LauncherPitch > 1.0f;
	// o cilindro hidráulico: acelera, corre na velocidade da válvula (a do ZH, TurretPitchRate 60°/s), freia no fim do curso e para no
	// ângulo; descer é o mesmo perfil na metade da velocidade (a válvula segura o peso). O berço segue o cilindro numa mola dura (o
	// batente): a inércia passa ~1,4° na freada e volta (o protótipo: scratchpad/noite/trucks/v2/proto/elev2.py, o mesmo em 60, 30 e
	// 15 qps). Subpassos de até 1/120 s; embaixo, o apoio do berço segura (não passa de 0 e devolve 20 % da velocidade)
	const bool bUp = Target > LauncherAct;
	const float Slow = bUp ? 1.0f : LauncherLowerDps / LauncherRaiseDps;
	const float VMax = bUp ? LauncherRaiseDps : LauncherLowerDps;
	const float Acc = LauncherAccelDps2 * Slow, Brake = LauncherBrakeDps2 * Slow;
	const float W = 2.0f * PI * LauncherStopHz;
	const int32 N = FMath::Clamp(FMath::CeilToInt(DeltaSeconds * 120.0f), 1, 16);
	const float H = DeltaSeconds / float(N);
	for (int32 K = 0; K < N; ++K)
	{
		const float Err = Target - LauncherAct;
		const float VWant = FMath::Abs(Err) < 1.0e-4f ? 0.0f : FMath::Sign(Err) * FMath::Min(VMax, FMath::Sqrt(2.0f * Brake * FMath::Abs(Err)));
		const bool bSpeeding = FMath::Abs(VWant) > FMath::Abs(LauncherActRate) && VWant * LauncherActRate >= 0.0f;
		const float Lim = (bSpeeding ? Acc : Brake) * H;
		LauncherActRate += FMath::Clamp(VWant - LauncherActRate, -Lim, Lim);
		float Next = LauncherAct + LauncherActRate * H;
		if (Err != 0.0f && (Target - Next) * Err <= 0.0f)
		{
			Next = Target;                             // chegou: o cilindro para no curso
			LauncherActRate = 0.0f;
		}
		LauncherAct = Next;
		const float A = W * W * (LauncherAct - LauncherPitch) - 2.0f * LauncherStopZeta * W * LauncherRate;
		LauncherRate += A * H;
		LauncherPitch += LauncherRate * H;
		if (LauncherPitch < 0.0f)
		{
			LauncherPitch = 0.0f;
			LauncherRate = LauncherRate < 0.0f ? -0.2f * LauncherRate : LauncherRate;
		}
	}
	ElevPivot->SetRelativeRotation(FRotator(LauncherPitch, 0.0f, 0.0f));
}

bool ARTSUnitActor::LauncherTubeWorld(int32 Index, FVector& OutCm, FVector& OutDir) const
{
	if (!ElevPivot || !LauncherTubesCm.IsValidIndex(Index))
	{
		return false;
	}
	const FTransform Tf = ElevPivot->GetComponentTransform();
	OutCm = Tf.TransformPosition(LauncherTubesCm[Index] - LauncherHingeCm);
	OutDir = Tf.GetUnitAxis(EAxis::X);
	return true;
}

int32 ARTSUnitActor::LauncherTubeOnSide(const FVector& SimCm) const
{
	if (!ElevPivot || !TurretPivot || LauncherTubesCm.Num() == 0)
	{
		return INDEX_NONE;
	}
	// o lado da torre no chão: a simulação solta cada tubo a ±o meio da bitola dos tubos à direita do giro da torre, no plano; o
	// desenho tem o mesmo giro, e o lado não muda com a elevação (o eixo dela é o Y) nem, no chão, com o casco inclinado (a boca alta
	// escorrega de lado no morro, ~3,3 m × o seno da inclinação: a boca mais perto podia ser a do outro tubo)
	const FTransform Tf = TurretPivot->GetComponentTransform();
	const FVector Right = Tf.GetUnitAxis(EAxis::Y);
	const FVector2D Right2 = FVector2D(Right.X, Right.Y).GetSafeNormal();
	const double Side = FVector2D::DotProduct(FVector2D(SimCm.X, SimCm.Y) - FVector2D(Tf.GetLocation().X, Tf.GetLocation().Y), Right2);
	const double ScaleY = Tf.GetScale3D().Y;
	int32 Best = 0;
	for (int32 K = 1; K < LauncherTubesCm.Num(); ++K)
	{
		if (FMath::Abs(Side - (LauncherTubesCm[K].Y - TurretPivotCm.Y) * ScaleY) < FMath::Abs(Side - (LauncherTubesCm[Best].Y - TurretPivotCm.Y) * ScaleY))
		{
			Best = K;
		}
	}
	return Best;
}

bool ARTSUnitActor::GetLaunchBlend(uint32 MissileId, FVector& OutOffsetCm, FVector& OutDir) const
{
	for (const FLaunchBlend& Blend : LaunchBlends)
	{
		if (Blend.Id == MissileId && MissileId != 0u)
		{
			OutOffsetCm = Blend.OffsetCm;
			OutDir = Blend.Dir;
			return true;
		}
	}
	return false;
}

float ARTSUnitActor::LauncherTopCm() const
{
	// os cantos da caixa do lançador (o berço e as pontas) girados na dobradiça (o mesmo giro do ElevPivot, no quadro do casco)
	FBox B(ForceInit);
	for (const UStaticMeshComponent* Part : LauncherParts)
	{
		if (Part && Part->GetStaticMesh())
		{
			B += Part->GetStaticMesh()->GetBoundingBox();
		}
	}
	if (!B.IsValid)
	{
		return 0.0f;
	}
	const float Sn = FMath::Sin(FMath::DegreesToRadians(LauncherPitch)), Cs = FMath::Cos(FMath::DegreesToRadians(LauncherPitch));
	float Top = 0.0f;
	for (const double X : { B.Min.X, B.Max.X })
	{
		for (const double Z : { B.Min.Z, B.Max.Z })
		{
			Top = FMath::Max(Top, float(LauncherHingeCm.Z + (X - LauncherHingeCm.X) * Sn + (Z - LauncherHingeCm.Z) * Cs));
		}
	}
	return Top;
}

bool ARTSUnitActor::FillDustSource(FRTSTreadSource& S, FVector2D& OutSandRearCm, FVector2D& OutSandFrontCm) const
{
	if (bStructure || bDead)
	{
		return false;
	}
	if (TracksCm.X > 0.0)
	{
		// a lagarta: o padrão do plugin (o tanque aprovado) e os pontos da areia de sempre (o pé de trás e a frente das rodas no chão)
		OutSandRearCm = FVector2D(-240.0, 142.5);
		OutSandFrontCm = FVector2D(255.0, 142.5);
		return true;
	}
	if (!bWheelDust)
	{
		return false;
	}
	S.WheelHalfGaugeM = WheelHalfGaugeCm / 100.0;
	S.WheelRearXM = WheelRearXCm / 100.0;
	S.WheelFrontXM = WheelFrontXCm / 100.0;
	S.WheelRadiusM = TyreRadiusCm / 100.0;
	S.WheelHalfWidthM = 0.5 * TyreWidthCm / 100.0;
	S.DustScale = WheelDustScale;
	S.DustAmount = WheelDustAmount;
	S.DustSpacing = WheelDustSpacing;
	// as caixas do veículo (as mesmas do mapa do sol): o rastro em comprimentos dele, o ar que ele empurra e a sombra dele na poeira
	FVector2D Center;
	float HalfLen = 0.0f, HalfWidth = 0.0f, HullTop = 0.0f, Height = 0.0f;
	GetShadowBoxCm(Center, HalfLen, HalfWidth, HullTop, Height);
	S.TrailLenM = 2.0 * HalfLen / 100.0;
	S.TrailTimeS = WheelDustTrailS;
	S.HalfLenM = HalfLen / 100.0;
	S.HalfWidthM = HalfWidth / 100.0;
	S.HullTopM = HullTop / 100.0;
	S.HeightM = Height / 100.0;
	S.FrontalM2 = 2.0 * S.HalfWidthM * S.HullTopM;
	OutSandRearCm = FVector2D(WheelRearXCm, WheelHalfGaugeCm);
	OutSandFrontCm = FVector2D(WheelFrontXCm, WheelHalfGaugeCm);
	return true;
}

FVector2D ARTSUnitActor::GetTyreMarksCm(float& OutDark, float& OutDepth, float& OutWidthK, float& OutLugK, float& OutSegmentK) const
{
	if (!bWheelDust || bStructure)
	{
		return FVector2D::ZeroVector;
	}
	OutDark = TyreMarkDark;
	OutDepth = TyreMarkDepth;
	OutWidthK = TyreWidthCm / 64.0f;               // o perfil do relevo foi feito pra lagarta de 64 cm do tanque
	OutLugK = TyreMarkLug;
	OutSegmentK = TyreMarkSegmentK;
	return FVector2D(2.0f * WheelHalfGaugeCm, TyreWidthCm);
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
		if (TurretParts.Num())
		{
			// (16/10) a torre do prédio gira com a mira da simulação (a caixa da Patriot): antes ficava parada na pose do
			// modelo, e o míssil saía pela ponta oposta às bocas
			TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg + TurretYawOffsetDeg), 0.0f));
		}
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
	TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg + TurretYawOffsetDeg), 0.0f));
	TurretPivot->SetRelativeLocation(TurretPivotCm + FVector(-RecoilCm, 0.0f, 0.0f));
	StepLauncher(DeltaSeconds);   // (23/10, v4) o lançador que levanta (o Tomahawk)
}

void ARTSUnitActor::EnsureShadowBoxes() const
{
	if (bShadowBoxReady)
	{
		return;
	}
	// as peças saem com a origem no centro do veículo (a roda e a torre voltam pra ela pelo pivô): a caixa de cada malha
	// já está no quadro do casco
	const auto Gather = [](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FBox& Out)
	{
		Out = FBox(EForceInit::ForceInit);
		for (const UStaticMeshComponent* P : Parts)
		{
			if (P && P->GetStaticMesh())
			{
				Out += P->GetStaticMesh()->GetBoundingBox();
			}
		}
	};
	Gather(HullParts, ShadowHullBox);
	Gather(TurretParts, ShadowTurretBox);
	bShadowBoxReady = true;
}

void ARTSUnitActor::GetShadowBoxCm(FVector2D& OutCenter, float& OutHalfLen, float& OutHalfWidth, float& OutHullTop, float& OutHeight) const
{
	EnsureShadowBoxes();
	const FVector Sc = Body ? Body->GetComponentScale() : FVector::OneVector;
	if (!ShadowHullBox.IsValid)
	{
		// sem malha (não devia): o círculo da unidade
		OutCenter = FVector2D::ZeroVector;
		OutHalfLen = RadiusCm;
		OutHalfWidth = 0.5f * RadiusCm;
		OutHullTop = OutHeight = 150.0f;
		return;
	}
	const FVector C = ShadowHullBox.GetCenter() * Sc;
	const FVector E = ShadowHullBox.GetExtent() * Sc;
	OutCenter = FVector2D(C.X, C.Y);
	OutHalfLen = float(E.X);
	OutHalfWidth = float(E.Y);
	OutHullTop = float(ShadowHullBox.Max.Z * Sc.Z);
	// a carcaça cuja torre voou fica só com o casco (22/10: também depois que ela pousa; antes a caixa da torre voltava pro
	// casco quando ela assentava, e toda carcaça fazia sombra mais alta que o casco que tem)
	const bool bTurret = TurretParts.Num() > 0 && TurretPivot && !TurretPivot->IsUsingAbsoluteLocation() && ShadowTurretBox.IsValid;
	OutHeight = bTurret ? FMath::Max(OutHullTop, float(ShadowTurretBox.Max.Z * Sc.Z)) : OutHullTop;
	if (bTurret && LauncherPitch > 0.5f)
	{
		OutHeight = FMath::Max(OutHeight, LauncherTopCm() * float(Sc.Z));   // (23/10, v4) o lançador levantado faz a sombra mais alta
	}
	// (22/10) a carcaça afundando: a caixa desce junto (o que está abaixo do chão não faz sombra)
	if (bDead && !bStructure && SinkCm > 0.0f)
	{
		OutHullTop = FMath::Max(OutHullTop - SinkCm, 0.0f);
		OutHeight = FMath::Max(OutHeight - SinkCm, 0.0f);
	}
}

bool ARTSUnitActor::GetCollisionBoxCm(FTransform& OutFrame, FBox& OutBox) const
{
	EnsureShadowBoxes();
	if (!Body || !ShadowHullBox.IsValid)
	{
		return false;
	}
	const FTransform Frame = Body->GetComponentTransform();
	const FVector Sc = Frame.GetScale3D();
	FBox Hull = ShadowHullBox;
	// a torre no lugar: a planta do casco até o topo dela (o cano virado fica de fora; a torre que voou, também)
	if (TurretParts.Num() > 0 && TurretPivot && !TurretPivot->IsUsingAbsoluteLocation() && ShadowTurretBox.IsValid)
	{
		Hull.Max.Z = FMath::Max(Hull.Max.Z, ShadowTurretBox.Max.Z);
	}
	OutBox = FBox(Hull.Min * Sc, Hull.Max * Sc);
	OutFrame = FTransform(Frame.GetRotation(), Frame.GetLocation());
	return true;
}

bool ARTSUnitActor::GetLooseTurretBoxCm(FTransform& OutFrame, FBox& OutBox, bool bOnlyResting) const
{
	if (!Loose || !TurretPivot || (bOnlyResting && !Loose->IsAsleep()))
	{
		return false;
	}
	OutFrame = FTransform(TurretPivot->GetComponentQuat(), TurretPivot->GetComponentLocation());
	OutBox = Loose->BodyBoxLocal();
	return true;
}

bool ARTSUnitActor::IsLooseTurretMoving() const
{
	return Loose && !Loose->IsAsleep() && Loose->FlightSeconds() < TurretWreckWaitMaxS;
}

FVector ARTSUnitActor::HullForward() const
{
	const FVector F = Body ? Body->GetForwardVector() : GetActorForwardVector();
	const FVector G(F.X, F.Y, 0.0);
	return G.IsNearlyZero() ? FVector::ForwardVector : G.GetSafeNormal();
}

FVector ARTSUnitActor::TurretForward() const
{
	if (!TurretPivot || TurretParts.Num() == 0)
	{
		return HullForward();
	}
	// o pivô gira com a mira + o ajuste da malha (TurretYawOffsetDeg): a mira é o pivô sem o ajuste
	const FVector F = FRotator(0.0f, -TurretYawOffsetDeg, 0.0f).RotateVector(TurretPivot->GetForwardVector());
	const FVector G(F.X, F.Y, 0.0);
	return G.IsNearlyZero() ? HullForward() : G.GetSafeNormal();
}

FVector ARTSUnitActor::MuzzleLocation() const
{
	FVector Mouth, Dir;
	if (LauncherTubeWorld(LauncherTube, Mouth, Dir))
	{
		return Mouth;                              // (23/10, v4) a boca do tubo do lançador, na pose de agora (levantado no tiro)
	}
	return TurretPivot->GetComponentTransform().TransformPosition(FVector(MuzzleForwardCm, 0.0f, MuzzleHeightCm));
}

FVector ARTSUnitActor::MuzzleDirection() const
{
	FVector Mouth, Dir;
	if (LauncherTubeWorld(LauncherTube, Mouth, Dir))
	{
		return Dir;
	}
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

void ARTSUnitActor::PlayHit(const FVector2D& FromCm)
{
	if (bStructure || bDead)
	{
		return;
	}
	// o empurrão vai de quem atirou pro alvo; no casco: de frente levanta o nariz (como o recuo do próprio tiro), de lado
	// deita pro lado de lá
	const FVector Push = FVector(FVector2D(GetActorLocation()) - FromCm, 0.0f).GetSafeNormal();
	if (Push.IsNearlyZero())
	{
		return;
	}
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Push);
	SuspPitchRate += HitKickDeg * -Local.X;
	SuspRollRate += HitKickDeg * Local.Y;
}

void ARTSUnitActor::PlayBump(float PitchKickDeg, float RollKickDeg)
{
	if (bStructure || bDead)
	{
		return;
	}
	SuspPitchRate += PitchKickDeg;
	SuspRollRate += RollKickDeg;
}

void ARTSUnitActor::PlayFire(bool bGunRecoil)
{
	// (23/10, v4) o lançador que levanta: o tubo deste tiro já veio da simulação (NoteLauncherShot, antes daqui, em todo tiro)
	// (v4, 23/10) o míssil sai da caixa sem recuo: o Tomahawk (veículo com torre) deslizava a mesa e as caixas 30 cm pra trás
	// do caminhão e balançava o casco como o canhão do tanque
	if (bGunRecoil)
	{
		RecoilCm = 30.0f;
	}
	if (bGunRecoil && !bStructure)
	{
		// o tranco do tiro: o canhão empurra o casco pra trás, pro lado de onde a torre aponta (o prédio, a Patriot, não
		// balança)
		const float Rel = FMath::DegreesToRadians(TurretPivot->GetRelativeRotation().Yaw);
		SuspPitchRate += FireKickDeg * FMath::Cos(Rel);
		SuspRollRate -= FireKickDeg * FMath::Sin(Rel);
	}
	if (UBlastFxDirector::ShotFxEnabled())
	{
		return;   // (14/10) o sopro da boca é do BlastFx (MuzzleBlast, chamado pela partida); -ShotFx=0 volta pro provisório
	}
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
	// (16/10, o Fluxo) o tanque ferido com os limiares do ZH: o fio de fumaça clara abaixo de 70%, o jato de chama
	// pelo anel da torre e a fumaça grossa abaixo de 35%; a BlastFx só age nas passagens (e respeita o -ShotFx=0)
	if (!bDead && !bStructure)
	{
		if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
		{
			Fx->VehicleDamage(this, 1.0f - Damage);
		}
	}
	if (bDead || !bStructure || FMath::Abs(Damage - DamageShown) < 0.02f)
	{
		return;
	}
	DamageShown = Damage;
	SetPartsFloat(DataDamage, Damage);
}

void ARTSUnitActor::SetSilhouette(int32 Stencil)
{
	SilhouetteStencil = FMath::Clamp(Stencil, 0, 127);
	ApplyStencil();
}

void ARTSUnitActor::SetXRay(bool bOn)
{
	if (bOn != bXRayOn)
	{
		bXRayOn = bOn;
		ApplyStencil();
	}
}

void ARTSUnitActor::SetUnitMask(bool bOn)
{
	bUnitMask = bOn;
	ApplyStencil();
}

void ARTSUnitActor::ApplyStencil()
{
	// todas as peças (13/10, o Felipe: as caixas da carga fora da lista tapavam o caminhão e o contorno pintava de
	// vermelho em cima delas); o bit 128 é a máscara da poeira, os bits baixos a silhueta (1 + dono, só seu e aliado)
	// (16/10) a silhueta só com a unidade escondida de verdade (SetXRay); a máscara da poeira sempre
	const int32 Stencil = (bUnitMask ? 128 : 0) | (bXRayOn ? SilhouetteStencil : 0);
	for (TArray<TObjectPtr<UStaticMeshComponent>>* List : { &HullParts, &TurretParts, &SpinParts, &CargoParts })
	{
		for (UStaticMeshComponent* Part : *List)
		{
			if (Part)
			{
				Part->SetRenderCustomDepth(Stencil > 0);
				Part->SetCustomDepthStencilValue(Stencil);
			}
		}
	}
	if (Scaffold)
	{
		Scaffold->SetRenderCustomDepth(bUnitMask);
		Scaffold->SetCustomDepthStencilValue(bUnitMask ? 128 : 0);
	}
}

void ARTSUnitActor::SetFogHidden(bool bHide)
{
	if (bHide != bFogHidden)
	{
		bFogHidden = bHide;
		SetActorHiddenInGame(bHide);
	}
	// (v4, 23/10) a carcaça de uma morte que ninguém viu aparece (a partida chama todo quadro): a torre aparece junto, e as
	// marcas guardadas das batidas dela vão pro chão agora
	if (!bHide && bWreckKnown && UnseenTurretImpacts.Num() > 0)
	{
		StampUnseenTurretImpacts();
	}
}

void ARTSUnitActor::StampUnseenTurretImpacts()
{
	if (URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr)
	{
		for (const FRTSTurretImpact& Hit : UnseenTurretImpacts)
		{
			// a batida é de antes: só a marca (no ponto à vista agora, ou quando ele aparecer), sem o empurrão na poeira
			Match->TurretImpact(Hit.AtCm, Hit.SlideDir, Hit.NormalSpeedCmS, Hit.SpreadCm, false, Hit.TangentSpeedCmS);
		}
	}
	UnseenTurretImpacts.Reset();
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
	bUnitMask = false;
	SetSilhouette(0);   // a carcaça não tem silhueta nem máscara (a poeira passa por cima dela)
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
	if (ElevPivot && LauncherParts.Num())
	{
		// (23/10, v4) o lançador levantado cai no berço na morte (a hidráulica perde a pressão; o clarão da explosão cobre o quadro): a
		// torre solta voa com as malhas deitadas, a forma que a física dela conhece (o tubo que atirou segue vazio)
		for (UStaticMeshComponent* Part : LauncherParts)
		{
			Part->AttachToComponent(TurretPivot, FAttachmentTransformRules::KeepRelativeTransform);
			Part->SetRelativeLocationAndRotation(-TurretPivotCm, FRotator::ZeroRotator);
		}
		LauncherAct = LauncherActRate = LauncherPitch = LauncherRate = LauncherAimS = 0.0f;
		bLauncherAim = bLauncherAimFalling = false;
	}
	// (v4, 23/10) a torre voa em toda morte (a v2 aprovada) e agora com física de corpo rígido: cai no chão de verdade, quica,
	// rola e para em qualquer face estável (de pé, de lado, de ponta-cabeça), encostada na inclinação. -TurretPhys=0 = a v2
	if (TurretParts.Num())
	{
		if (TurretPhysicsOn())
		{
			LaunchLooseTurret();
		}
		else
		{
			LaunchTurretV2();
		}
	}
}

void ARTSUnitActor::LaunchTurretV2()
{
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

void ARTSUnitActor::LaunchLooseTurret()
{
	UWorld* World = GetWorld();
	if (!World || !TurretPivot)
	{
		return;
	}
	// a forma: o corpo da ficha (turret_box_m: o casco fundido sem o mastro, a estação de armas e as antenas, que quebram) ou,
	// sem ele, a caixa das malhas sem o cano; o cano pela malha "Barrel" (a mesma regra da boca). As peças saem com a origem no
	// centro do tanque: as caixas já estão no quadro das malhas
	FRTSLooseTurretShape Shape;
	Shape.Scale = float(TurretPivot->GetComponentScale().X);
	for (const UStaticMeshComponent* Part : TurretParts)
	{
		const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		if (Mesh->GetName().Contains(TEXT("Barrel")))
		{
			Shape.Barrel += Mesh->GetBoundingBox();
		}
		else
		{
			Shape.Body += Mesh->GetBoundingBox();
		}
	}
	if (TurretBodyBoxCm.IsValid)
	{
		Shape.Body = TurretBodyBoxCm;
	}
	if (!Shape.Body.IsValid)
	{
		return;
	}
	Loose = MakeUnique<FRTSLooseTurret>();
	Loose->Init(Shape);
	// o pivô vai pro centro de massa (antes ficava no chão do casco, ~2 m abaixo do corpo: ela girava em volta dele, e o rastro
	// de fuligem do BlastFx, que segue o pivô, saía 2 m abaixo da torre)
	const FTransform PivotXf = TurretPivot->GetComponentTransform();
	const FVector ComLocal = Loose->CenterOfMassLocal();
	const FVector X0 = PivotXf.TransformPosition(ComLocal - TurretPivotCm);
	const FQuat Q0 = PivotXf.GetRotation();
	for (UStaticMeshComponent* Part : TurretParts)
	{
		Part->SetRelativeLocation(-ComLocal);
	}
	TurretPivot->SetUsingAbsoluteLocation(true);
	TurretPivot->SetUsingAbsoluteRotation(true);
	TurretPivot->SetWorldLocationAndRotation(X0, Q0);

	const FRTSBakedMap* Map = BakedMapOf(World);
	const FBakedTurretGround Ground(Map, float(GetActorLocation().Z));
	DeathSlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(Ground.Normal(FVector2D(GetActorLocation().X, GetActorLocation().Y)).Z), -1.0f, 1.0f)));
	// o sorteio só visual, fixo por unidade: o lado e o giro pelo hash de sempre (o da v2) e, novos, ±15% na velocidade e ±20%
	// no giro por outro sal
	const uint32 H = CosmeticHash(SimId);
	uint32 Seed = CosmeticHash(SimId ^ 0x7A11E7u) | 1u;
	const float Up = TurretUpCmS * (1.0f + TurretSpeedJitter * (2.0f * Random01(Seed) - 1.0f));
	const float SideSpeed = TurretSideCmS * (1.0f + TurretSpeedJitter * (2.0f * Random01(Seed) - 1.0f));
	const float SpinDeg = TurretSpinDegS * (1.0f + TurretSpinJitter * (2.0f * Random01(Seed) - 1.0f));
	// o lado, como na v2: perpendicular à câmera (±20°), o primeiro de 5 (os dois lados, as diagonais pra trás e pra trás) com
	// 3,5 m de folga até a peça grande mais perto, senão o de maior folga
	FVector View(1.0, 0.0, 0.0);
	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		View = (GetActorLocation() - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal2D();
	}
	if (View.IsNearlyZero())
	{
		View = FVector(1.0, 0.0, 0.0);
	}
	FVector Side = FVector::CrossProduct(FVector::UpVector, View).GetSafeNormal();
	Side = ((H & 1u) ? Side : -Side).RotateAngleAxis(float((H >> 1) % 41u) - 20.0f, FVector::UpVector);
	// a queda prevista sai do voo de verdade: z(t) = z0 + U·t − g·t²/2 = o chão lá + 60 cm (antes 2·S·U/g, com o chão do casco)
	const FVector2D From2(X0.X, X0.Y);
	const auto LandingOf = [&](const FVector& Dir)
	{
		FVector2D Land = From2;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			const float Drop = float(X0.Z) - (Ground.HeightCm(Land) + TurretRestComCm);
			const float Root = FMath::Sqrt(FMath::Max(Up * Up + 2.0f * GravityCm * Drop, 0.0f));
			const float FallS = (Up + Root) / GravityCm;
			Land = From2 + FVector2D(Dir.X, Dir.Y) * double(SideSpeed * FallS);
		}
		return Land;
	};
	// as caixas por perto (a unidade, a carcaça, o prédio e a torre já solta de outro tanque), lidas uma vez: pra folga e pros
	// obstáculos
	struct FNearBox
	{
		ARTSUnitActor* Actor = nullptr;
		bool bTurret = false;
		FBox2D Plan = FBox2D(ForceInit);
		float TallCm = 0.0f;
	};
	TArray<FNearBox> Near;
	for (TActorIterator<ARTSUnitActor> It(World); It; ++It)
	{
		ARTSUnitActor* Other = *It;
		if (Other == this || FVector::Dist2D(Other->GetActorLocation(), X0) > 6000.0)
		{
			continue;
		}
		for (int32 Which = 0; Which < 2; ++Which)
		{
			FTransform Frame;
			FBox Box;
			if (Which == 0 ? !Other->GetCollisionBoxCm(Frame, Box) : !Other->GetLooseTurretBoxCm(Frame, Box, false))
			{
				continue;
			}
			const FBox InWorld = Box.TransformBy(Frame);
			FNearBox& N = Near.AddDefaulted_GetRef();
			N.Actor = Other;
			N.bTurret = Which == 1;
			N.Plan = FBox2D(FVector2D(InWorld.Min.X, InWorld.Min.Y), FVector2D(InWorld.Max.X, InWorld.Max.Y));
			N.TallCm = float(InWorld.Max.Z - InWorld.Min.Z);
		}
	}
	const auto DistanceTo = [](const FBox2D& Plan, const FVector2D& P)
	{
		const FVector2D C = Plan.GetCenter();
		const FVector2D E = Plan.GetExtent();
		const double Dx = FMath::Max(FMath::Abs(P.X - C.X) - E.X, 0.0);
		const double Dy = FMath::Max(FMath::Abs(P.Y - C.Y) - E.Y, 0.0);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	};
	// a folga num ponto: até a peça grande mais perto (mais de 0,8 m de altura e 1,5 m de lado, como na v2); e a célula da grade
	// que recusa a queda: água, pedra/palmeira/barreira e peça da vila (as casas e os muros não são colisores da torre)
	const auto ClearanceAt = [&](const FVector2D& P)
	{
		if (Map && Map->CellReason.Num() == Map->Logic.Width * Map->Logic.Height)
		{
			const Sim::Cell GridCell = Map->Logic.CellOf(RTS::ToSim(P));
			const uint8 Why = Map->Logic.InBounds(GridCell.X, GridCell.Y) ? Map->CellReason[Map->Logic.Index(GridCell.X, GridCell.Y)] : uint8(2);
			if (Why == 1 || Why == 3 || Why == 4)
			{
				return -1.0;
			}
		}
		double Best = 1.0e9;
		for (const FNearBox& N : Near)
		{
			if (N.TallCm >= 80.0f && N.Plan.GetExtent().GetMax() >= 75.0)
			{
				Best = FMath::Min(Best, DistanceTo(N.Plan, P));
			}
		}
		return Best;
	};
	// a folga do lado: na queda prevista, no meio e no fim do corredor de 3 m depois dela (ela ainda anda depois do 1º toque)
	const FVector Dirs[] = { Side, -Side, (Side + View).GetSafeNormal(), (-Side + View).GetSafeNormal(), View };
	FVector2D Lands[5];
	int32 Pick = -1;
	int32 BestI = 0;
	double BestC = -2.0;
	for (int32 I = 0; I < 5; ++I)
	{
		Lands[I] = LandingOf(Dirs[I]);
		double C = 1.0e9;
		for (const double Along : { 0.0, 0.5, 1.0 })
		{
			C = FMath::Min(C, ClearanceAt(Lands[I] + FVector2D(Dirs[I].X, Dirs[I].Y) * (Along * TurretSlideCorridorCm)));
		}
		if (Pick < 0 && C >= TurretClearanceCm)
		{
			Pick = I;
		}
		if (C > BestC)
		{
			BestC = C;
			BestI = I;
		}
	}
	const int32 Chosen = Pick >= 0 ? Pick : BestI;
	const FVector Dir = Dirs[Chosen];
	LooseLandPredicted = Lands[Chosen];
	// os obstáculos: o próprio casco (arma quando ela sai de cima dele) e as caixas até 20 m da queda prevista
	LooseObstacles.Reset();
	LooseObstacleActors.Reset();
	LooseObstacleIsTurret.Reset();
	LooseObstacles.AddDefaulted();
	LooseObstacleActors.Add(this);
	LooseObstacleIsTurret.Add(false);
	for (const FNearBox& N : Near)
	{
		if (DistanceTo(N.Plan, LooseLandPredicted) <= TurretObstacleRadiusCm)
		{
			LooseObstacles.AddDefaulted();
			LooseObstacleActors.Add(N.Actor);
			LooseObstacleIsTurret.Add(N.bTurret);
		}
	}
	RefreshLooseObstacles();
	// o giro: o de sempre (cambalhota e rolamento em 2 eixos) com a regra do cano pra cima: o sinal do giro horizontal que
	// levanta a boca (no protótipo, sem ela 2 de 12 batiam o cano no chão a 0,15 s)
	const float F1 = float((H >> 8) & 0xFFu) / 255.0f;
	const float F2 = float((H >> 16) & 0xFFu) / 255.0f;
	FVector W0 = FVector(FMath::Lerp(0.6f, 1.0f, F1) * ((H & 0x10000000u) ? 1.0f : -1.0f), FMath::Lerp(0.4f, 0.8f, F2), 0.1f).GetSafeNormal()
		* FMath::DegreesToRadians(SpinDeg);
	const FVector MuzzleArm = Q0.RotateVector(Loose->MuzzlePoint());
	if (!MuzzleArm.IsNearlyZero() && FVector::CrossProduct(W0, MuzzleArm).Z < 0.0)
	{
		W0.X = -W0.X;
		W0.Y = -W0.Y;
	}
	Loose->Launch(X0, Q0, FVector(0.0, 0.0, Up) + Dir * SideSpeed, W0, TurretDelayS);
}

void ARTSUnitActor::RefreshLooseObstacles()
{
	// a unidade viva anda e a carcaça é empurrada: a caixa de cada uma é relida a cada quadro (a que sumiu, desliga)
	for (int32 I = 0; I < LooseObstacles.Num(); ++I)
	{
		FRTSTurretObstacle& O = LooseObstacles[I];
		const ARTSUnitActor* Other = LooseObstacleActors.IsValidIndex(I) ? LooseObstacleActors[I].Get() : nullptr;
		O.bActive = Other && (LooseObstacleIsTurret[I] ? Other->GetLooseTurretBoxCm(O.Frame, O.Box) : Other->GetCollisionBoxCm(O.Frame, O.Box));
	}
}

void ARTSUnitActor::TickLooseTurret(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	// o relógio das explosões: com a partida pausada a torre para no ar, como a bola e a fuligem
	const UBlastFxDirector* Fx = UBlastFxDirector::Get(this);
	const float Dt = Fx ? Fx->SimDelta(DeltaSeconds) : DeltaSeconds;
	if (Dt <= 0.0f || !World)
	{
		return;
	}
	// a física roda também escondida na névoa e no fantasma: ela nunca congela no ar
	RefreshLooseObstacles();
	const FBakedTurretGround Ground(BakedMapOf(World), float(GetActorLocation().Z));
	Loose->Advance(Dt, Ground, LooseObstacles);
	TurretPivot->SetWorldLocationAndRotation(Loose->ShownLocation(), Loose->ShownRotation());
	if (Loose->Impacts.Num() > 0)
	{
		// as batidas fortes: o empurrão na poeira à vista e a marca no chão (o ponto na névoa: a marca espera a área aparecer).
		// A morte que ninguém viu guarda as batidas até a carcaça aparecer (SetFogHidden): a torre segue escondida com ela, e a
		// marca nascendo do nada no chão à vista entregava a morte na névoa
		if (!bWreckKnown)
		{
			UnseenTurretImpacts.Append(Loose->Impacts);
		}
		else if (URTSMatchSubsystem* Match = World->GetSubsystem<URTSMatchSubsystem>())
		{
			for (const FRTSTurretImpact& Hit : Loose->Impacts)
			{
				Match->TurretImpact(Hit.AtCm, Hit.SlideDir, Hit.NormalSpeedCmS, Hit.SpreadCm, !bFogHidden, Hit.TangentSpeedCmS);
			}
		}
		Loose->Impacts.Reset();
	}
	if (Loose->Scrapes.Num() > 0)
	{
		// (23/10, a areia do acerto) a torre deslizando ou rolando na areia: a poeira da raspada, só da carcaça conhecida e à vista (a
		// raspada da morte que ninguém viu não volta depois: a poeira já teria assentado, e não deixa marca)
		if (bWreckKnown && !bFogHidden)
		{
			if (URTSMatchSubsystem* Match = World->GetSubsystem<URTSMatchSubsystem>())
			{
				for (const FRTSTurretScrape& Sc : Loose->Scrapes)
				{
					Match->TurretScrape(Sc.AtCm, Sc.Dir, Sc.SpeedCmS, Sc.Index);
				}
			}
		}
		Loose->Scrapes.Reset();
	}
	if (CVarTurretDebug.GetValueOnGameThread() > 0)
	{
		TArray<FVector> Pts;
		TArray<bool> Touching;
		Loose->GetDebugPoints(Pts, Touching);
		for (int32 I = 0; I < Pts.Num(); ++I)
		{
			DrawDebugPoint(World, Pts[I], 8.0f, Touching.IsValidIndex(I) && Touching[I] ? FColor::Red : FColor::Green, false, Loose->IsAsleep() ? 8.0f : -1.0f);
		}
	}
	if (Loose->IsAsleep())
	{
		OnLooseTurretAsleep();
	}
}

void ARTSUnitActor::OnLooseTurretAsleep()
{
	// uma linha por torre, e outra a cada vez que ela para de novo depois de acordar (a prova do líder pelo log: a folga do
	// ponto mais baixo tem de ficar entre −0,5 e +0,5 cm)
	const FVector Rest = Loose->ShownLocation();
	const FVector Up = Loose->ShownRotation().GetAxisZ();
	const TCHAR* Pose = Up.Z > 0.7 ? TEXT("de pé") : (Up.Z < -0.7 ? TEXT("de ponta-cabeça") : TEXT("de lado"));
	const FVector2D Hull(GetActorLocation().X, GetActorLocation().Y);
	const int32 StepCount = FMath::Max(Loose->Steps, 1);
	UE_LOG(LogTemp, Log, TEXT("RTS torre %u: caiu a %.1f m do casco (prevista %.1f m; o 1º toque aos %.2f s de voo, a %.1f m), %d batidas (a maior %.1f m/s), %s aos %.2f s, %s, folga do ponto mais baixo %+.2f cm, encostou no casco ou em unidades %d vezes (a maior %.1f m/s; parada em %s, acordou %d vezes), chão sob o casco %.0f°, %d passos, %.0f µs (%.1f µs por passo)"),
		SimId, FVector2D::Distance(FVector2D(Rest.X, Rest.Y), Hull) / 100.0, FVector2D::Distance(LooseLandPredicted, Hull) / 100.0,
		Loose->FirstTouchS, FVector2D::Distance(FVector2D(Loose->FirstTouchAt.X, Loose->FirstTouchAt.Y), Hull) / 100.0,
		Loose->Hits, Loose->MaxHitCmS / 100.0f, Loose->IsCapped() ? TEXT("parou no teto") : TEXT("dormiu"), Loose->RestS, Pose,
		Loose->RestGapCm, Loose->ObstacleTouches, Loose->MaxObstacleHitCmS / 100.0f,
		Loose->IsRestingOnObstacle() ? TEXT("obstáculo") : TEXT("só chão"), Loose->Wakes, DeathSlopeDeg, StepCount,
		Loose->SolveSeconds * 1.0e6, Loose->SolveSeconds * 1.0e6 / double(StepCount));
	// a carcaça já afundando (a tirada logo depois da morte): a torre desce no resto do afundar, a partir de agora
	if (WreckSinkP > 0.0f && !bSinkTurret)
	{
		ArmTurretSink(WreckSinkP);
	}
}

void ARTSUnitActor::SetWreckSink(float P01)
{
	// só desce (a simulação nunca volta o afundar; o fantasma da névoa só para)
	P01 = FMath::Clamp(P01, WreckSinkP, 1.0f);
	if (!bDead || bStructure || !Body || P01 <= WreckSinkP)
	{
		return;
	}
	if (WreckSinkP <= 0.0f)
	{
		// o começo: a pose de onde o casco desce; na v2 (-TurretPhys=0) a torre que ainda voava (a carcaça tirada pelo teto logo
		// depois da morte) para onde está
		EnsureShadowBoxes();
		SinkBodyLoc = Body->GetRelativeLocation();
		SinkBodyQ = Body->GetRelativeRotation().Quaternion();
		if (!Loose && TurretParts.Num() > 0 && TurretPivot && TurretPivot->IsUsingAbsoluteLocation())
		{
			bTurretFlying = false;
			bTurretSettling = false;
			ArmTurretSink(0.0f);
		}
	}
	// (v4) a torre com física desce a partir de quando parou: a que ainda cai ou rola segue a física e começa quando dormir
	// (OnLooseTurretAsleep), no resto do afundar; ela nunca afunda saindo do ar
	if (Loose && Loose->IsAsleep() && !bSinkTurret)
	{
		ArmTurretSink(WreckSinkP);
	}
	WreckSinkP = P01;
	// devagar no começo e no fim (o smoothstep: o pico é 1,5× a média, ~0,5 m/s com o casco de ~2,1 m em 7 s, o ritmo do
	// SinkRate do ZH), até o topo do casco ficar abaixo do chão
	const float Ease = P01 * P01 * (3.0f - 2.0f * P01);
	const float HullTopCm = ShadowHullBox.IsValid ? float(ShadowHullBox.Max.Z * Body->GetComponentScale().Z) : 250.0f;
	SinkCm = (HullTopCm + WreckSinkMarginCm) * Ease;
	// inclina 2–3° pra um lado fixo por unidade (uma ponta afunda antes), como o prédio que desaba
	const uint32 H = CosmeticHash(SimId ^ 0x5157u);
	const float LeanDeg = (2.0f + float(H % 11u) * 0.1f) * ((H & 0x800u) ? 1.0f : -1.0f) * Ease;
	Body->SetRelativeLocationAndRotation(SinkBodyLoc - FVector(0.0, 0.0, SinkCm), SinkBodyQ * FRotator(LeanDeg, 0.0f, -LeanDeg * 0.5f).Quaternion());
	if (bSinkTurret)
	{
		// o resto do afundar da carcaça, a partir de quando a torre começou a descer (também devagar no começo e no fim)
		const float TurretP = SinkTurretFromP >= 0.999f ? 1.0f : FMath::Clamp((P01 - SinkTurretFromP) / (1.0f - SinkTurretFromP), 0.0f, 1.0f);
		const float TurretEase = TurretP * TurretP * (3.0f - 2.0f * TurretP);
		TurretPivot->SetWorldLocation(SinkTurretLoc - FVector(0.0, 0.0, SinkTurretCm * TurretEase));
	}
}

void ARTSUnitActor::ArmTurretSink(float FromP)
{
	if (!TurretPivot || TurretParts.Num() == 0)
	{
		return;
	}
	bSinkTurret = true;
	SinkTurretFromP = FMath::Clamp(FromP, 0.0f, 1.0f);
	SinkTurretLoc = TurretPivot->GetComponentLocation();
	// a torre caída pode estar de ponta-cabeça ou de lado: desce até o topo dela no mundo (o mastro e as antenas também) ficar
	// abaixo do chão, como o casco. O chão: o menor sob os pontos em que ela encostou (v4); na v2, o do casco
	float TopZ = float(SinkTurretLoc.Z);
	for (const UStaticMeshComponent* Part : TurretParts)
	{
		if (Part)
		{
			TopZ = FMath::Max(TopZ, float(Part->Bounds.Origin.Z + Part->Bounds.BoxExtent.Z));
		}
	}
	const float BaseZ = Loose ? Loose->RestGroundZ : float(GetActorLocation().Z);
	SinkTurretCm = FMath::Max(TopZ - BaseZ, 0.0f) + WreckSinkMarginCm;
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
	if (Loose)
	{
		if (!Loose->IsAsleep())
		{
			TickLooseTurret(DeltaSeconds);
		}
		else if (Loose->IsRestingOnObstacle() && !bSinkTurret)
		{
			// parada encostada num obstáculo (o próprio casco, outra unidade, outra torre, um prédio), relido a cada quadro: se
			// ele mexer ou sumir (a unidade viva que sai, a carcaça empurrada, a torre ou o prédio que afunda), ela acorda e
			// cai, em vez de ficar parada no ar. Com o afundar armado ela desce com a carcaça e não olha mais
			RefreshLooseObstacles();
			if (Loose->WakeIfSupportMoved(LooseObstacles))
			{
				TickLooseTurret(DeltaSeconds);
			}
		}
	}
	else if (bTurretFlying)
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
		// o quadro segue enquanto a torre mexe ou está parada encostada num obstáculo (o acordar acima)
		const bool bTurretTicks = Loose ? (!Loose->IsAsleep() || (Loose->IsRestingOnObstacle() && !bSinkTurret)) : (bTurretFlying || bTurretSettling);
		if (!bTurretTicks)
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
