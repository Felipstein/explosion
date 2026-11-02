#include "RTSUnitActor.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "BlastFxDirector.h"
#include "Components/DecalComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "RTSSoldierAnim.h"
#include "AnimationRuntime.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RTSConvert.h"
#include "RTSEffect.h"
#include "RTSGameData.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSTreadDust.h"
#include "Scalability.h"
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
	constexpr int32 DataStructure = 11;    // (31/10) 1 = prédio: a rachadura e a fuligem em V do M_Structure (no lugar das pintas do veículo)
	/** A solda na obra: faíscas por segundo e a luz de cada uma (pequena: é detalhe, não clarão). */
	constexpr float SparksPerSecond = 14.0f;
	constexpr float SparkCandela = 7.0e4f;
	// (31/10, a revisão de vídeo do fx_obra_em_cima) o andaime vai um nível à frente da parede (o lift de 2 m do Tools/blender_provisional.py)
	// e a solda solta faísca até meio segundo depois da última subida da obra (com pouca energia ela sobe a cada 2 passos, 67 ms)
	constexpr float ScaffoldLeadCm = 200.0f;
	constexpr double WeldHoldS = 0.5;
	/** O prédio destruído afunda até esta fração da altura (o ZH derruba o modelo no chão). */
	// (30/10, o entulho) o colapso em panqueca deixa ~1/5 da altura (o monte do ARTSDebrisField::AddRubble): o prédio desce 80%
	// (antes 42%, a casca inteira em pé no meio do entulho). (31/10, a conferência da v30: a bandeira da War Factory ficava de pé,
	// sozinha no entulho) a altura é o topo da malha, com o mastro: os 20% que sobravam eram o pano da bandeira, não a laje. Agora o
	// prédio sem ruína desce inteiro, como o que tem (o monte do entulho é a panqueca)
	constexpr float StructureSinkFraction = 1.05f;
	constexpr float StructureSinkSeconds = 6.0f;   // (01/11) o atraso de 1 s e o desabamento de 5 s (ARTSUnitActor::CollapseSeconds)
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

	/** O material da obra de um material da base (MI_Body → MI_Body_Build) e o caminho de volta. (30/10) Suffix "_Holed": o do prédio furado. */
	UMaterialInterface* BuildTwin(UMaterialInterface* M, bool bBuild, const TCHAR* Suffix = TEXT("_Build"))
	{
		if (!M)
		{
			return nullptr;
		}
		const FString Name = M->GetName();
		const bool bIsBuild = Name.EndsWith(Suffix);
		if (bIsBuild == bBuild || !Name.StartsWith(TEXT("MI_")))
		{
			return M;
		}
		const FString Twin = bBuild ? Name + Suffix : Name.LeftChop(FCString::Strlen(Suffix));
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
	HitMassT = Visual.MassT;
	DeathScale = FMath::Clamp(Visual.DeathScale, 0.2f, 1.5f);
	DeathKind = Visual.DeathKind;
	DeathJetBake = Visual.DeathJetBake;
	DeathJetScale = Visual.DeathJetScale;
	DeathJetBaseM = Visual.DeathJetBaseM;
	RadiusCm = Visual.RadiusM * 100.0f;
	MuzzleHeightCm = Visual.MuzzleHeightM * 100.0f;
	AddParts(Visual.HullMeshes, Body, HullParts);
	RoofFirePointsCm = Visual.FirePointsCm;
	GroundCutsCm = Visual.GroundCutsCm;   // (01/11) o recorte do chão e o túnel
	TunnelPathCm = Visual.TunnelPathCm;
	TunnelSpeedCm = Visual.TunnelSpeedCm;
	// (01/11) a esteira que anda: as lagartas do casco (a malha do elenco traz a tangente do elo; a fase vai no dado 10, M_StructureTrack)
	TrackParts.Reset();
	TrackSideYCm.Reset();
	TrackPhase.Reset();
	TrackSent.Reset();
	TrackStepCm = Visual.TrackStepM * 100.0f;
	if (TrackStepCm > 1.0f)
	{
		for (UStaticMeshComponent* Part : HullParts)
		{
			const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
			if (Mesh && Mesh->GetName().Contains(TEXT("_Track_")))
			{
				TrackParts.Add(Part);
				TrackSideYCm.Add(float(Mesh->GetBoundingBox().GetCenter().Y));
				TrackPhase.Add(0.0f);
				TrackSent.Add(-1.0f);
			}
		}
	}
	// (31/10) os estados de dano do prédio: a malha do casco (a 1ª) de cada estado e o fogo dele
	StructMeshPaths[0] = Visual.HullMeshes.Num() ? Visual.HullMeshes[0] : FString();
	StructMeshPaths[1] = Visual.DamagedMesh;
	StructMeshPaths[2] = Visual.WreckedMesh;
	TurretMeshPaths[0] = Visual.TurretMeshes.Num() ? Visual.TurretMeshes[0] : FString();   // (01/11) o veículo: a torre de cada estado
	TurretMeshPaths[1] = Visual.TurretDamagedMesh;
	TurretMeshPaths[2] = Visual.TurretWreckedMesh;
	StructFireCm[0] = Visual.FirePointsCm;
	StructFireCm[1] = Visual.FirePointsDamagedCm.Num() ? Visual.FirePointsDamagedCm : Visual.FirePointsCm;
	StructFireCm[2] = Visual.FirePointsWreckedCm.Num() ? Visual.FirePointsWreckedCm : StructFireCm[1];
	RuinMeshPath = Visual.RuinMesh;
	if (!Visual.ScaffoldMesh.IsEmpty() && HullParts.Num() && HullParts[0] && HullParts[0]->GetStaticMesh())   // (o bStructure só vem mais abaixo)
	{
		// (30/10) o relevo medido (um por malha, guardado pro jogo inteiro)
		static TMap<FString, TSharedPtr<const FHeightGrid>> Cache;
		const FString Name = HullParts[0]->GetStaticMesh()->GetName();
		if (const TSharedPtr<const FHeightGrid>* Found = Cache.Find(Name))
		{
			Heights = *Found;
		}
		else
		{
			FString Text;
			TSharedPtr<FJsonObject> J;
			const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Hs = nullptr;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Heights"), Name + TEXT(".json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid()
				&& J->TryGetArrayField(TEXT("min_cm"), Min) && Min->Num() == 2 && J->TryGetArrayField(TEXT("h_cm"), Hs))
			{
				TSharedPtr<FHeightGrid> G = MakeShared<FHeightGrid>();
				G->MinCm = FVector2D((*Min)[0]->AsNumber(), (*Min)[1]->AsNumber());
				G->CellCm = float(J->GetNumberField(TEXT("cell_cm")));
				G->Nx = int32(J->GetNumberField(TEXT("nx")));
				G->Ny = int32(J->GetNumberField(TEXT("ny")));
				G->H.Reserve(Hs->Num());
				for (const TSharedPtr<FJsonValue>& V : *Hs)
				{
					G->H.Add(float(V->AsNumber()));
				}
				if (G->H.Num() == G->Nx * G->Ny && G->CellCm > 1.0f)
				{
					Heights = G;
				}
			}
			Cache.Add(Name, Heights);
		}
		// (30/10) os triângulos do modelo: o acerto do tiro na superfície de verdade (RayStructure)
		static TMap<FString, TSharedPtr<const FTriGrid>> TriCache;
		if (const TSharedPtr<const FTriGrid>* FoundT = TriCache.Find(Name))
		{
			Tris = *FoundT;
		}
		else
		{
			TArray<uint8> Raw;
			if (FFileHelper::LoadFileToArray(Raw, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RTS"), TEXT("Heights"), Name + TEXT(".tris.bin")))
				&& Raw.Num() >= 36 && Raw.Num() % 36 == 0)
			{
				TSharedPtr<FTriGrid> G = MakeShared<FTriGrid>();
				const int32 NumV = Raw.Num() / 12;
				G->V.SetNumUninitialized(NumV);
				FMemory::Memcpy(G->V.GetData(), Raw.GetData(), Raw.Num());
				FVector2f Lo(FLT_MAX), Hi(-FLT_MAX);
				for (const FVector3f& P : G->V)
				{
					Lo = FVector2f(FMath::Min(Lo.X, P.X), FMath::Min(Lo.Y, P.Y));
					Hi = FVector2f(FMath::Max(Hi.X, P.X), FMath::Max(Hi.Y, P.Y));
				}
				G->MinCm = Lo;
				G->Nx = FMath::Max(1, FMath::CeilToInt32((Hi.X - Lo.X) / G->CellCm));
				G->Ny = FMath::Max(1, FMath::CeilToInt32((Hi.Y - Lo.Y) / G->CellCm));
				G->Cells.SetNum(G->Nx * G->Ny);
				for (int32 T3 = 0; T3 < NumV / 3; ++T3)
				{
					const FVector3f& A = G->V[T3 * 3];
					const FVector3f& B = G->V[T3 * 3 + 1];
					const FVector3f& C = G->V[T3 * 3 + 2];
					const int32 I0 = FMath::Clamp(FMath::FloorToInt32((FMath::Min3(A.X, B.X, C.X) - Lo.X) / G->CellCm), 0, G->Nx - 1);
					const int32 I1 = FMath::Clamp(FMath::FloorToInt32((FMath::Max3(A.X, B.X, C.X) - Lo.X) / G->CellCm), 0, G->Nx - 1);
					const int32 J0 = FMath::Clamp(FMath::FloorToInt32((FMath::Min3(A.Y, B.Y, C.Y) - Lo.Y) / G->CellCm), 0, G->Ny - 1);
					const int32 J1 = FMath::Clamp(FMath::FloorToInt32((FMath::Max3(A.Y, B.Y, C.Y) - Lo.Y) / G->CellCm), 0, G->Ny - 1);
					for (int32 I = I0; I <= I1; ++I)
					{
						for (int32 J = J0; J <= J1; ++J)
						{
							G->Cells[I * G->Ny + J].Add(T3);
						}
					}
				}
				Tris = G;
			}
			TriCache.Add(Name, Tris);
		}
	}
	AddParts(Visual.TurretMeshes, TurretPivot, TurretParts);
	// (30/10) o cano é peça separada (SM_..._Barrel): o recuo é dele, dentro da torre (o WeaponRecoilBone = Barrel do ZH)
	for (const TObjectPtr<UStaticMeshComponent>& Part : TurretParts)
	{
		if (Part && Part->GetStaticMesh() && Part->GetStaticMesh()->GetName().Contains(TEXT("Barrel")))
		{
			BarrelParts.Add(Part.Get());
			BarrelRestCm.Add(Part->GetRelativeLocation());
		}
	}
	TurretPivotCm =FVector(Visual.TurretPivotM.X * 100.0f, Visual.TurretPivotM.Y * 100.0f, 0.0f);
	// (30/10, simulação 26) o soldado: o corpo com o fuzil é a torre (gira inteiro pra mira) e as pernas balançam no quadril, presas nela
	bInfantry = Visual.bInfantry;
	if (bInfantry)
	{
		StrideCm = Visual.StrideCm;
		for (int32 K = 0; K < 2; ++K)
		{
			if (UStaticMesh* Mesh = Visual.LegMeshes[K].IsEmpty() ? nullptr : LoadMesh(Visual.LegMeshes[K]))
			{
				USceneComponent* Hip = NewObject<USceneComponent>(this);
				Hip->SetupAttachment(TurretPivot);
				Hip->SetRelativeLocation(FVector(Visual.LegHipCm.X, K == 0 ? Visual.LegHipCm.Y : -Visual.LegHipCm.Y, Visual.LegHipCm.Z));
				Hip->RegisterComponent();
				TurretParts.Add(AddPart(Mesh, Hip));
				LegPivots.Add(Hip);
			}
		}
		// (30/10, o Felipe: "a infantaria tá horrível [...] muito quadradinhos, sem detalhes"; docs/ESTUDO_INFANTARIA_VISUAL.md §8) o corpo de
		// verdade no lugar das caixas: o modelo humano da Unreal 5.8 com os clipes de fuzil (RTSSoldierAnim: o passo pela velocidade, o tiro, a
		// morte pela direção), o fuzil na mão, a farda e a cor da facção no capacete e no colete (os game shots: a GLA de verde, a USA de azul)
		if (!Visual.SoldierMesh.IsEmpty())
		{
			if (USkeletalMesh* Skm = LoadObject<USkeletalMesh>(nullptr, *Visual.SoldierMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				SoldierMesh = NewObject<USkeletalMeshComponent>(this);
				SoldierMesh->SetupAttachment(TurretPivot);
				SoldierMesh->SetSkeletalMesh(Skm);
				SoldierMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				SoldierMesh->SetAnimInstanceClass(URTSSoldierAnimInstance::StaticClass());
				SoldierMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));   // o modelo da Unreal olha pra +Y
				SoldierMesh->SetRelativeScale3D(FVector(Visual.SoldierScale));
				SoldierBodyScale = Visual.SoldierScale;   // (simulação 35) a cintura na escotilha do transporte
				SoldierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SoldierMesh->SetReceivesDecals(false);
				SoldierMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
				SoldierMesh->RegisterComponent();
				SetupSoldierLook(Visual);
				if (URTSSoldierAnimInstance* Anim = Cast<URTSSoldierAnimInstance>(SoldierMesh->GetAnimInstance()))
				{
					Anim->BodyScale = Visual.SoldierScale;
				}
				for (UStaticMeshComponent* Part : TurretParts)
				{
					if (Part)
					{
						Part->SetVisibility(false);
						Part->SetCastShadow(false);
					}
				}
				for (UStaticMeshComponent* Part : HullParts)
				{
					if (Part)
					{
						Part->SetVisibility(false);
						Part->SetCastShadow(false);
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS soldado: falta o corpo %s (Game/Content/Characters/Mannequins)"), *Visual.SoldierMesh);
			}
		}
	}
	if (Visual.MuzzleForwardM > 0.0f)
	{
		MuzzleForwardCm = Visual.MuzzleForwardM * 100.0f;   // (30/10) a ponta do fuzil (sem a malha "Barrel")
	}
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
	// (30/10, o Felipe: "o cano tá voando, ficou afastado da torreta") o repouso do cano é o de agora, já no pivô da torre: guardado antes
	// (logo depois do AddParts, na origem), o recuo devolvia o cano à origem a cada quadro, à frente do mantelete o quanto o pivô fica
	// à frente do centro (10 cm no M1, ~40 cm no tanque da GLA)
	for (int32 K = 0; K < BarrelParts.Num(); ++K)
	{
		if (const UStaticMeshComponent* Barrel = BarrelParts[K].Get())
		{
			BarrelRestCm[K] = Barrel->GetRelativeLocation();
		}
	}
	// o que gira sozinho: um pivô no eixo, a malha com a origem nele
	for (const FRTSSpinPart& Spin : Visual.SpinParts)
	{
		if (UStaticMesh* Mesh = LoadMesh(Spin.Mesh))
		{
			USceneComponent* Pivot = NewObject<USceneComponent>(this);
			// (25/10, v5) a peça da torre (o prato do radar no mastro): o pivô vai com o giro dela
			Pivot->SetupAttachment(Spin.bOnTurret ? TurretPivot.Get() : Body.Get());
			Pivot->SetRelativeLocation(Spin.bOnTurret ? Spin.AtCm - TurretPivotCm : Spin.AtCm);
			Pivot->RegisterComponent();
			SpinPivots.Add(Pivot);
			SpinParts.Add(AddPart(Mesh, Pivot));
			SpinRpm.Add(Spin.Rpm);
			SpinAngleDeg.Add(float(CosmeticHash(InSimId + SpinPivots.Num()) % 360));
			SpinIsRotor.Add(Spin.bRotor);   // (v5) o rotor da aeronave gira pelo motor da simulação
			SpinAxis.Add(Spin.Axis);
			SpinBlades.Add(Spin.Blades);    // (24/10, escopo B) o disco borrado
			SpinRadiusCm.Add(Spin.RadiusCm);
			SpinTipCm.Add(Spin.TipCm);
			SpinChordCm.Add(Spin.ChordCm);
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
			// (30/10, o Felipe: "essa luz em cima amarela piscando tem que ter uma luz mais forte, refletir a luz nas coisas, não tão forte
			// também, olha o tamanho dela") o giroflex de verdade: dois fachos âmbar opostos girando com a cúpula. (31/10, item 30) Na força
			// de um de verdade: o de halogênio de 55 W com o refletor parabólico que gira (o Federal Signal Sentry, SAE J845 classe 1) dá
			// ~80 000 cd por clarão, já pela cúpula âmbar, num facho estreito (~±6° em volta do pico, até ~12° na borda); o facho quase na
			// horizontal, 8° pra baixo (pega o chão a partir de ~8 m, o pico a ~20 m, e as coisas em volta na altura da cabine). O sol do
			// jogo é o de verdade (120 000 lux): de dia o facho no chão a 20 m (~200 lux) quase some, como na vida; no fim de tarde e na
			// sombra ele lê. Com sombra no Alta e no Ultra (a cabine, a lâmina e a obra recortam o facho; -RTSBeaconShadow=0 tira pro A/B
			// do custo). A luz nasce 20 cm à frente do eixo, no rumo do facho: de dentro, a própria cúpula taparia
			if (SpinPivots.Num() > 0 && BeaconLights.Num() == 0)
			{
				int32 ShadowOpt = Scalability::GetQualityLevels().ShadowQuality >= 2 ? 1 : 0;
				FParse::Value(FCommandLine::Get(), TEXT("RTSBeaconShadow="), ShadowOpt);
				// (31/10, a avaliação: "giroflex tá com luz muito fraca, tem que melhorar a luz refletida nas coisas em volta") o facho de
				// 12° a 8° pra baixo só tocava o chão a ~20 m (a ~200 lux) e passava por cima de tudo perto. Agora o facho do refletor que
				// abre na vertical (o miolo ±10°, a borda até 25°) e desce 12°: varre a obra, os veículos e o chão a partir de ~5 m; o pico
				// ×2,5 (o refletor maior do giroflex de obra, que se lê da câmera do jogo; de dia ainda é o sol que manda). E a cúpula acesa:
				// a lente âmbar espalha um pouco pra todo lado e acende a cabine, a lâmina e o chão logo em volta. A/B: -RTSBeaconV=1 (a de antes)
				// (01/11, v3) a conta pra ler de dia, a câmera do jogo com a exposição fixa do sol (EV100 14): o facho só aparece na areia ao sol se
				// somar ~15% da luz dela (0,15 × 120 000 lux = 18 000 lux) a ~8 m, o meio da varredura: I = E·d² ≈ 1,2·10⁶ cd no pico (6× a v2; o
				// giroflex de verdade não faz isso de dia: é a leitura do jogo, a jogabilidade antes do realismo). A cúpula igual: a cabine e a
				// lâmina a ~1,5 m só ficam laranja ao sol com ~18 000 lux, ~4·10⁴ cd. A/B: -RTSBeaconV=2 (a v2) e 1 (a de antes)
				int32 BeaconV = 3;
				FParse::Value(FCommandLine::Get(), TEXT("RTSBeaconV="), BeaconV);
				const bool bV2 = BeaconV >= 2;
				BeaconPeakCd = BeaconV >= 3 ? 1.2e6f : (bV2 ? 200000.0f : 80000.0f);
				BeaconGlowCd = BeaconV >= 3 ? 40000.0f : 2500.0f;
				for (int32 K = 0; K < 2; ++K)
				{
					USpotLightComponent* L = NewObject<USpotLightComponent>(this);
					L->SetupAttachment(SpinPivots[0]);
					const float Yaw = K == 0 ? 0.0f : 180.0f;
					L->SetRelativeLocation(FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector(20.0f, 0.0f, 8.0f)));
					L->SetRelativeRotation(FRotator(BeaconV >= 3 ? -25.0f : (bV2 ? -12.0f : -8.0f), Yaw, 0.0f));   // (01/11) a v3 varre o chão em volta
					L->SetIntensityUnits(ELightUnits::Candelas);
					L->SetIntensity(0.0f);
					L->SetLightColor(FLinearColor(1.0f, 0.55f, 0.1f));
					L->SetInnerConeAngle(BeaconV >= 3 ? 14.0f : (bV2 ? 10.0f : 6.0f));
					L->SetOuterConeAngle(BeaconV >= 3 ? 32.0f : (bV2 ? 25.0f : 12.0f));
					L->SetAttenuationRadius(bV2 ? 4000.0f : 3500.0f);
					L->SetCastShadows(ShadowOpt != 0);
					L->RegisterComponent();
					BeaconLights.Add(L);
				}
				if (bV2)
				{
					// a cúpula: uma luz pequena, sem sombra, de alcance curto (6 m), que acompanha a lâmina
					BeaconGlow = NewObject<UPointLightComponent>(this);
					BeaconGlow->SetupAttachment(SpinPivots[0]);
					BeaconGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));
					BeaconGlow->SetIntensityUnits(ELightUnits::Candelas);
					BeaconGlow->SetIntensity(0.0f);
					BeaconGlow->SetLightColor(FLinearColor(1.0f, 0.55f, 0.1f));
					BeaconGlow->SetAttenuationRadius(BeaconV >= 3 ? 800.0f : 600.0f);
					BeaconGlow->SetSourceRadius(10.0f);
					BeaconGlow->SetCastShadows(false);
					BeaconGlow->RegisterComponent();
				}
				// (01/11, a medida da v36: de dia o facho no chão soma só 3–4%, e o que se vê de cima é a própria cúpula) a lente âmbar a 70 mil
				// nits é ~5× a areia ao sol (~15 mil): a 88 m, um ponto de 2 px sem halo. A/B: -RTSBeaconDomeNits=300000 (o brilho abre o halo
				// do bloom que lê de cima; o slot Beacon de cada peça que gira)
				float DomeNits = 0.0f;
				if (FParse::Value(FCommandLine::Get(), TEXT("RTSBeaconDomeNits="), DomeNits) && DomeNits > 0.0f)
				{
					for (TArray<TObjectPtr<UStaticMeshComponent>>* List : { &SpinParts, &HullParts })
					{
						for (UStaticMeshComponent* Part : *List)
						{
							const TArray<FName> Names = Part ? Part->GetMaterialSlotNames() : TArray<FName>();
							for (int32 K = 0; K < Names.Num(); ++K)
							{
								if (Names[K].ToString().Contains(TEXT("Beacon")))
								{
									if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Part->GetMaterial(K), this))
									{
										M->SetScalarParameterValue(TEXT("EmissiveNits"), DomeNits);
										Part->SetMaterial(K, M);
									}
								}
							}
						}
					}
				}
			}
			BladePivot = NewObject<USceneComponent>(this);
			BladePivot->SetupAttachment(Body);
			BladePivot->SetRelativeLocation(Visual.BladeAtCm);
			BladePivot->RegisterComponent();
			BladePart = AddPart(Mesh, BladePivot);
			HullParts.Add(BladePart);
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
	if (bStructure)
	{
		Tags.AddUnique(TEXT("RTSStructure"));   // (30/10) o BlastFx distingue o prédio (o tiro de canhão nele não solta faísca de metal)
		SetPartsFloat(DataStructure, 1.0f);
	}
	if (HullParts.Num() && HullParts[0]->GetStaticMesh())
	{
		const FBox Box = HullParts[0]->GetStaticMesh()->GetBoundingBox();
		HeightCm = FMath::Max(100.0f, float(Box.Max.Z));
		FootprintCm = FBox2D(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y));
	}
	if (!bStructure && FootprintCm.bIsValid && !bInfantry)   // (30/10) o soldado lê pela sombra do sol (a mancha seria de 10 m)
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
	ApplyPaintTint(Visual);
	SetupFlag(Visual);   // (01/11) os prédios também (as bandeiras da GLA)
	if (!bStructure)
	{
		SetupWheels(Visual);
		SetupLauncher(Visual);
		// (25/10, v5) as células dos mísseis, a arma leve e onde a lagarta toca o chão (o Linebacker e o Stryker)
		VlsCm = Visual.VlsCm;
		bLightGun = Visual.bLightGun;
		bLaunchCanister = Visual.bLaunchCanister;
		LightGunFlashCm = Visual.LightGunFlashCm > 0.0f ? Visual.LightGunFlashCm : 30.0f;
		LightGunTracerCm = Visual.LightGunTracerCm > 0.0f ? Visual.LightGunTracerCm : 260.0f;
		LightGunSpreadCm = Visual.LightGunSpreadCm > 0.0f ? Visual.LightGunSpreadCm : 250.0f;
		TrackContactCm = Visual.TrackContactM * 100.0;
	}
	// (v5, 24/10) a aeronave: o trem (some no voo) e os mísseis à vista (o pente da 1ª arma), pelos nomes das malhas do casco
	bAircraft = Visual.bAir;
	if (bAircraft)
	{
		for (UStaticMeshComponent* Part : HullParts)
		{
			const FString Name = Part && Part->GetStaticMesh() ? Part->GetStaticMesh()->GetName() : FString();
			if (!Visual.GearMesh.IsEmpty() && Name == Visual.GearMesh)
			{
				GearParts.Add(Part);
			}
			if (Visual.AmmoMeshes.Contains(Name))
			{
				AmmoParts.Add(Part);
			}
		}
		AmmoParts.Sort([](const UStaticMeshComponent& A, const UStaticMeshComponent& B) { return A.GetStaticMesh()->GetName() < B.GetStaticMesh()->GetName(); });
		SetupAirFx(Visual);   // (24/10, escopo B: os discos dos rotores, a rampa, o pós-combustor; RTSUnitActorAir.cpp)
	}
}

void ARTSUnitActor::SetAirPose(const FVector& Location, float YawDeg, float PitchDeg, float RollDeg, float Rotor01, bool bGearDown, int32 AmmoShown, float DeltaSeconds)
{
	// (v5, 24/10) a pose da aeronave: a posição e o rumo vêm da simulação (interpolados pela partida); a inclinação suaviza aqui (o
	// escopo B refina: a mola, o nariz na subida). O rotor gira pelo motor da simulação (rpm × Rotor01), cada um no eixo dele
	if (bDead && !bAircraft)
	{
		return;
	}
	const float K = 1.0f - FMath::Exp(-DeltaSeconds * 4.0f);
	AirPitch = FMath::Lerp(AirPitch, PitchDeg, K);
	AirRoll = FMath::Lerp(AirRoll, RollDeg, K);
	AirRotor = Rotor01;
	// (24/10, escopo B) a velocidade desenhada (a pá que voa sai com ela; o vento do rotor a lê), suavizada
	if (bAirLastLoc && DeltaSeconds > 1.0e-4f)
	{
		AirVelCm = FMath::Lerp(AirVelCm, (Location - AirLastLoc) / DeltaSeconds, 1.0f - FMath::Exp(-DeltaSeconds * 8.0f));
	}
	AirLastLoc = Location;
	bAirLastLoc = true;
	SetActorLocation(Location);
	Body->SetWorldRotation(FRotator(AirPitch, YawDeg, AirRoll));
	for (int32 I = 0; I < SpinPivots.Num(); ++I)
	{
		if (!SpinIsRotor.IsValidIndex(I) || !SpinIsRotor[I])
		{
			continue;
		}
		SpinAngleDeg[I] = FMath::Fmod(SpinAngleDeg[I] + SpinRpm[I] * 6.0f * Rotor01 * DeltaSeconds, 360.0f);
		const float A = RotorShownDeg(I, SpinAngleDeg[I], FMath::Abs(SpinRpm[I]) * 6.0f * Rotor01);
		const int32 Axis = SpinAxis.IsValidIndex(I) ? SpinAxis[I] : 2;
		SpinPivots[I]->SetRelativeRotation(Axis == 0 ? FRotator(0.0f, 0.0f, A) : (Axis == 1 ? FRotator(A, 0.0f, 0.0f) : FRotator(0.0f, A, 0.0f)));
	}
	for (UStaticMeshComponent* Gear : GearParts)
	{
		if (Gear && Gear->IsVisible() != bGearDown && !bFogHidden)
		{
			Gear->SetVisibility(bGearDown);
		}
	}
	if (AmmoParts.Num() && AmmoShown != AmmoLast && !bFogHidden)
	{
		AmmoLast = AmmoShown;
		for (int32 I = 0; I < AmmoParts.Num(); ++I)
		{
			if (AmmoParts[I])
			{
				AmmoParts[I]->SetVisibility(AmmoShown < 0 || I < AmmoShown);
			}
		}
	}
	TickAirFx(DeltaSeconds);   // (24/10, escopo B) os discos, a rampa, o pós-combustor, o fogo da queda
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
	bLauncherGuns = Visual.bLauncherGuns;
	bLauncherAimAir = Visual.bLauncherAimAir;
	LauncherRestPitchDeg = Visual.LauncherRestPitchDeg;
	LauncherMaxPitchDeg = FMath::Max(Visual.LauncherMaxPitchDeg, 0.0f);
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
	// (01/11) os cilindros de elevação em 3 estágios (o TOS-1A): o cilindro e o estágio do meio num pivô no pino da mesa, a haste num pivô
	// no olhal do berço; UpdateRams aponta os dois um pro outro a cada quadro (antes o cilindro ficava deitado na mesa e a haste subia com
	// o bloco: os dois se soltavam na pose de tiro)
	if (!Visual.LauncherRamCyl.IsEmpty() && !Visual.LauncherRamRod.IsEmpty())
	{
		const auto FindPart = [this](const FString& Name) -> UStaticMeshComponent*
		{
			for (UStaticMeshComponent* Part : TurretParts)
			{
				if (Part && Part->GetStaticMesh() && Part->GetStaticMesh()->GetName() == Name)
				{
					return Part;
				}
			}
			return nullptr;
		};
		UStaticMeshComponent* Cyl = FindPart(Visual.LauncherRamCyl);
		UStaticMeshComponent* Rod = FindPart(Visual.LauncherRamRod);
		RamMid = Visual.LauncherRamMid.IsEmpty() ? nullptr : FindPart(Visual.LauncherRamMid);
		if (Cyl && Rod)
		{
			RamBaseCm = Visual.LauncherRamBaseCm;
			RamTipCm = Visual.LauncherRamTipCm;
			RamPivot = NewObject<USceneComponent>(this);
			RamPivot->SetupAttachment(TurretPivot);
			RamPivot->SetRelativeLocation(RamBaseCm - TurretPivotCm);
			RamPivot->RegisterComponent();
			RamRodPivot = NewObject<USceneComponent>(this);
			RamRodPivot->SetupAttachment(ElevPivot);
			RamRodPivot->SetRelativeLocation(RamTipCm - LauncherHingeCm);
			RamRodPivot->RegisterComponent();
			for (UStaticMeshComponent* Part : { Cyl, RamMid.Get() })
			{
				if (Part)
				{
					Part->AttachToComponent(RamPivot, FAttachmentTransformRules::KeepRelativeTransform);
					Part->SetRelativeLocation(-RamBaseCm);
				}
			}
			Rod->AttachToComponent(RamRodPivot, FAttachmentTransformRules::KeepRelativeTransform);
			Rod->SetRelativeLocation(-RamTipCm);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS: %s: os cilindros do lançador não estão nas peças da torre (ficam parados)"), *Visual.Key);
		}
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
	Blend = FLaunchBlend();
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
	// (25/10, v5) o antiaéreo: com a aeronave mirada, os canos sobem até a altura dela (do horizonte da dobradiça, menos a pose da malha),
	// num passo de 2° (o cilindro não persegue o tremor do alvo); sem ela, o de tiro enquanto mira e a espera depois do último alvo
	if (bLauncherAimAir && bLauncherAim && GunAimPitchDeg > -90.0f)
	{
		Target = FMath::Clamp(2.0f * FMath::RoundToFloat(0.5f * (GunAimPitchDeg - LauncherRestPitchDeg)), 0.0f, LauncherMaxPitchDeg);
		LauncherHeldPitchDeg = Target;
	}
	else if (bLauncherAimAir && Target > 0.0f && LauncherHeldPitchDeg > 0.0f)
	{
		Target = LauncherHeldPitchDeg;   // perdeu o alvo agora: os canos ficam onde estavam até a espera acabar
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
	UpdateRams();
}

void ARTSUnitActor::UpdateRams()
{
	// o olhal gira com o berço em volta da dobradiça (no plano XZ das malhas; o pitch da Unreal leva +X pra +Z); o cilindro aponta do pino
	// pro olhal e a haste do olhal pro pino (o mesmo ângulo no mundo, menos o do berço, de quem ela é filha); o estágio do meio desliza
	// pelo eixo metade do que o pistão esticou
	if (!RamPivot || !RamRodPivot)
	{
		return;
	}
	const float Th = FMath::DegreesToRadians(LauncherPitch);
	const FVector2D H(LauncherHingeCm.X, LauncherHingeCm.Z);
	const FVector2D T0 = FVector2D(RamTipCm.X, RamTipCm.Z) - H;
	const FVector2D T = H + FVector2D(T0.X * FMath::Cos(Th) - T0.Y * FMath::Sin(Th), T0.X * FMath::Sin(Th) + T0.Y * FMath::Cos(Th));
	const FVector2D B(RamBaseCm.X, RamBaseCm.Z);
	const FVector2D D0 = FVector2D(RamTipCm.X, RamTipCm.Z) - B;
	const FVector2D D = T - B;
	const float Phi = FMath::RadiansToDegrees(FMath::Atan2(float(D.Y), float(D.X)) - FMath::Atan2(float(D0.Y), float(D0.X)));
	RamPivot->SetRelativeRotation(FRotator(Phi, 0.0f, 0.0f));
	RamRodPivot->SetRelativeRotation(FRotator(Phi - LauncherPitch, 0.0f, 0.0f));
	if (UStaticMeshComponent* Mid = RamMid.Get())
	{
		const FVector2D U0 = D0.GetSafeNormal();
		const float Slide = float(D.Size() - D0.Size()) * 0.5f;
		Mid->SetRelativeLocation(-RamBaseCm + FVector(U0.X, 0.0, U0.Y) * Slide);
	}
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

bool ARTSUnitActor::GetLaunchBlend(uint32 MissileId, FVector& OutOffsetCm, FVector& OutDir, bool* bOutSoft, FVector* OutMouthCm) const
{
	for (const FLaunchBlend& Blend : LaunchBlends)
	{
		if (Blend.Id == MissileId && MissileId != 0u)
		{
			OutOffsetCm = Blend.OffsetCm;
			OutDir = Blend.Dir;
			if (bOutSoft)
			{
				*bOutSoft = Blend.bSoft;
			}
			if (OutMouthCm)
			{
				*OutMouthCm = Blend.MouthCm;
			}
			return true;
		}
	}
	return false;
}

bool ARTSUnitActor::NoteVlsShot(uint32 MissileId, const FVector& SimCm, FVector& OutMouthCm, FVector& OutMouthDir)
{
	// (25/10, v5) as células de lançamento vertical (o Linebacker): a da vez, em rodízio (a simulação solta da torre; a célula é do
	// casco), a boca no casco desenhado e o rumo pra cima dele; o voo emenda no da simulação em LaunchBlendSeconds
	OutMouthCm = SimCm;
	OutMouthDir = FVector::UpVector;
	if (VlsCm.Num() == 0 || !Body)
	{
		return false;
	}
	const FTransform Tf = Body->GetComponentTransform();
	OutMouthCm = Tf.TransformPosition(VlsCm[VlsNext % VlsCm.Num()]);
	OutMouthDir = Tf.GetUnitAxis(EAxis::Z);
	VlsNext = (VlsNext + 1) % VlsCm.Num();
	FLaunchBlend& Blend = LaunchBlends[LaunchBlendNext];
	LaunchBlendNext = (LaunchBlendNext + 1) % UE_ARRAY_COUNT(LaunchBlends);
	Blend = FLaunchBlend();
	Blend.Id = MissileId;
	Blend.OffsetCm = OutMouthCm - SimCm;
	Blend.Dir = OutMouthDir;
	return true;
}

bool ARTSUnitActor::NoteAmmoShot(uint32 MissileId, int32 AmmoIndex, const FVector& SimCm, FVector& OutMouthCm, FVector& OutMouthDir)
{
	// (25/10) o míssil da aeronave: a simulação solta a 0,55 m do eixo (os tubos alternando); o desenho, do trilho da peça que some (o
	// Hellfire no casulo de fora, a ~2 m do eixo), no meio dela e no rumo dela (o nariz e a rolagem da pose); o voo emenda no da simulação
	OutMouthCm = SimCm;
	OutMouthDir = GetActorForwardVector();
	const UStaticMeshComponent* Part = AmmoParts.IsValidIndex(AmmoIndex) ? AmmoParts[AmmoIndex].Get() : nullptr;
	if (!Part)
	{
		return false;
	}
	// (01/11, a avaliação: "os foguetes do apache estão saindo meio errado [...] atrás da asa") o corpo desenhado do míssil tem a PONTA no
	// ponto do voo (ABlastDebris::UpdateMissile): o voo começa na ponta do míssil guardado (o +X da peça), não no meio dele, senão o
	// míssil inteiro nascia meio corpo pra trás
	const FBox B = Part->GetStaticMesh() ? Part->GetStaticMesh()->GetBoundingBox() : FBox(FVector::ZeroVector, FVector::ZeroVector);
	OutMouthCm = Part->GetStaticMesh() ? Part->GetComponentTransform().TransformPosition(FVector(B.Max.X, B.GetCenter().Y, B.GetCenter().Z)) : Part->Bounds.Origin;
	OutMouthDir = Part->GetComponentTransform().GetUnitAxis(EAxis::X);
	FLaunchBlend& Blend = LaunchBlends[LaunchBlendNext];
	LaunchBlendNext = (LaunchBlendNext + 1) % UE_ARRAY_COUNT(LaunchBlends);
	Blend = FLaunchBlend();
	Blend.Id = MissileId;
	Blend.OffsetCm = OutMouthCm - SimCm;
	Blend.Dir = OutMouthDir;
	UE_LOG(LogTemp, Log, TEXT("RTS ar: a aeronave %u atirou o míssil %u do trilho %d: a %.0f cm do ponto da simulação"), SimId, MissileId, AmmoIndex,
		Blend.OffsetCm.Size());
	return true;
}

FVector ARTSUnitActor::GunHingeWorld() const
{
	return ElevPivot ? ElevPivot->GetComponentLocation() : (TurretPivot ? TurretPivot->GetComponentLocation() + FVector(0.0, 0.0, MuzzleHeightCm) : GetActorLocation());
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
		if (TrackContactCm.X < TrackContactCm.Y)
		{
			// (25/10, v5) a lagarta menor (o Linebacker): o pé de trás e a frente das rodas dela, na bitola dela
			OutSandRearCm = FVector2D(TrackContactCm.X, 0.5 * TracksCm.X);
			OutSandFrontCm = FVector2D(TrackContactCm.Y, 0.5 * TracksCm.X);
		}
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
	// (31/10, item 35: a bandeira saía preta) o pano é malha procedural, fora do laço das peças: a mesma cor pelo mesmo dado
	for (UStaticMeshComponent* FlagMesh : FlagMeshes)
	{
		if (FlagMesh)
		{
			FlagMesh->SetCustomPrimitiveDataVector4(DataPlayerColor, FVector4(Color.R, Color.G, Color.B, 1.0f));
		}
	}
	// (30/10) o soldado vestido: a faixa do ombro e a do capacete na cor do jogador (o M_Soldier)
	const auto Team = [&Color](UPrimitiveComponent* C)
	{
		for (int32 K = 0; C && K < C->GetNumMaterials(); ++K)
		{
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(K)))
			{
				M->SetVectorParameterValue(TEXT("TeamColor"), Color);
			}
		}
	};
	if (SoldierPieceParts.Num() > 0)
	{
		Team(SoldierMesh);
		for (UStaticMeshComponent* Piece : SoldierPieceParts)
		{
			Team(Piece);
		}
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

int32 ARTSUnitActor::WarmBuildLook(USceneComponent* Parent, const TArray<FString>& HullMeshes)
{
	// (31/10) as peças do prédio como o AddParts e o SetBuildLook põem (os gêmeos da obra, móvel, sem decalque nem colisão), a 1 % e todas
	// cortadas (o dado 5 mil metros abaixo do pivô): ninguém vê, mas a placa prepara o material da obra (o RTSMatchSubsystem, no carregar)
	AActor* WarmOwner = Parent ? Parent->GetOwner() : nullptr;
	if (!WarmOwner)
	{
		return 0;
	}
	int32 Made = 0;
	for (const FString& Pattern : HullMeshes)
	{
		for (const FString& Path : ExpandPattern(Pattern))
		{
			UStaticMesh* Mesh = LoadMesh(Path);
			if (!Mesh)
			{
				continue;
			}
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(WarmOwner);
			Part->SetupAttachment(Parent);
			Part->SetStaticMesh(Mesh);
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				Part->SetMaterial(Slot, BuildTwin(Part->GetMaterial(Slot), true));
			}
			Part->SetCustomPrimitiveDataFloat(DataCut, -1000.0f);
			Part->SetRelativeScale3D(FVector(0.01));
			Part->SetReceivesDecals(false);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetCanEverAffectNavigation(false);
			Part->RegisterComponent();
			++Made;
		}
	}
	return Made;
}

void ARTSUnitActor::SetStructureState(float InBuildFraction, bool bInPowered)
{
	if (bDead)
	{
		return;
	}
	const bool bWasBuilding = BuildFraction < 1.0f;
	// (31/10) a obra andou: a simulação só anda a obra com o Dozer no alcance (World::UpdateConstruction); a solda vai por isso (TickStructure)
	if (bWasBuilding && FMath::Clamp(InBuildFraction, 0.0f, 1.0f) > BuildFraction && GetWorld())
	{
		LastBuildRiseS = GetWorld()->GetTimeSeconds();
	}
	BuildFraction = FMath::Clamp(InBuildFraction, 0.0f, 1.0f);
	bPowered = bInPowered;
	const bool bBuilding = BuildFraction < 1.0f;
	bEverComplete |= !bBuilding;
	UpdateSite();   // (01/11) o canteiro
	UpdateGroundCut(!bBuilding);   // (01/11) o prédio pronto recorta o chão; a obra e a venda (a obra voltando) não
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
		// (31/10, a revisão de vídeo do fx_obra_em_cima: "o andaime aparece em altura total antes do Dozer chegar") o andaime sobe com a
		// obra, ScaffoldLeadCm à frente da parede, saindo do chão inteiro (desce em vez de encolher: os níveis não achatam); com a obra em 0
		// (o Dozer ainda não chegou) fica só a laje
		if (Scaffold && Scaffold->GetStaticMesh())
		{
			const float TopCm = float(Scaffold->GetStaticMesh()->GetBoundingBox().Max.Z);
			const float ShownCm = BuildFraction > 0.0f ? FMath::Min(Cut * 100.0f + ScaffoldLeadCm, TopCm) : 0.0f;
			if (FMath::Abs(ShownCm - ScaffoldShownCm) > 1.0f)
			{
				UpdateScaffoldRing(ShownCm);   // (01/11) o andaime em volta da obra inteira; com ele, o do modelo não aparece
				Scaffold->SetVisibility(ShownCm > 0.0f && !ScaffoldRing);
				Scaffold->SetRelativeLocation(FVector(0.0, 0.0, double(ShownCm - TopCm)));
				ScaffoldShownCm = ShownCm;
			}
		}
		PowerShown = 0.0f;
		PowerOnClock = 0.0f;
	}
	else if (bWasBuilding)
	{
		if (ScaffoldRing)
		{
			ScaffoldRing->SetVisibility(false);   // (01/11) o prédio pronto: o andaime sai
		}
		PowerOnClock = 0.0f;   // inauguração: as luzes acendem piscando
	}
}

void ARTSUnitActor::UpdateScaffoldRing(float ShownCm)
{
	// (01/11, ele: "os andaimes não estão em volta da construção, estão dentro, no centro") o andaime em volta da caixa inteira do casco (o
	// anexo, as chaminés e os tanques dentro dele), 30 cm fora da parede, em vãos iguais de ~3 m por lado (a sobra dividida), lance de 2 m;
	// à vista os lances que começam abaixo da altura do andaime de agora (ShownCm, que já anda ScaffoldLeadCm à frente da obra)
	if (!bScaffoldRingTried)
	{
		bScaffoldRingTried = true;
		static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("RTSScaffoldRing="), V); return V != 0; }();
		UStaticMesh* Bay = bOn ? LoadMesh(TEXT("/Game/Structures/site_props/SM_Site_ScaffoldBay/StaticMeshes/SM_Site_ScaffoldBay")) : nullptr;
		if (!Bay || !HullParts.Num() || !HullParts[0] || !HullParts[0]->GetStaticMesh())
		{
			return;
		}
		const FBox Box = HullParts[0]->GetStaticMesh()->GetBoundingBox();
		const FVector S = HullParts[0]->GetRelativeScale3D();
		const FVector2D Lo(Box.Min.X * S.X - 30.0, Box.Min.Y * S.Y - 30.0), Hi(Box.Max.X * S.X + 30.0, Box.Max.Y * S.Y + 30.0);
		// no sentido horário visto de cima: a esquerda de cada lado é pra fora (o +Y da peça)
		const FVector2D C[4] = { Lo, FVector2D(Lo.X, Hi.Y), Hi, FVector2D(Hi.X, Lo.Y) };
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D A = C[K], B = C[(K + 1) % 4];
			const float Len = float(FVector2D::Distance(A, B));
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 300.0f));
			const FVector2D Dir = (B - A) / double(Len);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(Dir.Y), float(Dir.X)));
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2D P = A + Dir * double(Len * I / N);
				ScaffoldRingBays.Add(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(P.X, P.Y, 0.0), FVector(Len / (300.0f * N), 1.0f, 1.0f)));
			}
		}
		ScaffoldRingLifts = FMath::Max(1, FMath::CeilToInt32((HeightCm + 100.0f) / 200.0f));
		ScaffoldRing = NewObject<UInstancedStaticMeshComponent>(this);
		ScaffoldRing->SetupAttachment(Body);
		ScaffoldRing->SetStaticMesh(Bay);
		ScaffoldRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ScaffoldRing->SetCanEverAffectNavigation(false);
		ScaffoldRing->RegisterComponent();
	}
	if (!ScaffoldRing)
	{
		return;
	}
	const int32 Want = ShownCm > 0.0f ? FMath::Clamp(FMath::CeilToInt32(ShownCm / 200.0f), 1, ScaffoldRingLifts) : 0;
	if (Want == ScaffoldRingShown)
	{
		return;
	}
	ScaffoldRingShown = Want;
	ScaffoldRing->ClearInstances();
	for (int32 Lift = 0; Lift < Want; ++Lift)
	{
		for (const FTransform& T : ScaffoldRingBays)
		{
			FTransform U = T;
			U.AddToTranslation(FVector(0.0, 0.0, 200.0 * Lift));
			ScaffoldRing->AddInstance(U);
		}
	}
	ScaffoldRing->SetVisibility(Want > 0);
}

void ARTSUnitActor::UpdateSite()
{
	// (01/11, ele: "o placeholder da construção não iniciada ainda tá ruim, tinha que ser muros de construção [...] quando o Dozer chega e
	// TOMA INICIATIVA, começa o progresso (>0%), aí o negócio realmente começa construção [...] faça de arrepiar, sem parecer jogo low
	// poly, sem parecer jogo de lego") as três fases do canteiro de verdade: o gabarito e o material chegam antes da máquina; com a obra
	// andando o gabarito sai e o material vai sendo usado; no fim o canteiro todo sai. Só a obra que nunca ficou pronta (vender não refaz)
	static const bool bOn = [] { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("RTSSite="), V); return V != 0; }();
	if (!bOn || !bStructure || !Body || !FootprintCm.bIsValid)
	{
		return;
	}
	const bool bWant = !bEverComplete && BuildFraction < 1.0f;
	if (!bWant && !bSiteMade)
	{
		return;
	}
	if (bWant && !bSiteMade)
	{
		bSiteMade = true;
		static const bool bZebra = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("RTSSiteZebra="), V); return V != 0; }();
		const auto Mesh = [this](const TCHAR* Name)
		{
			return LoadMesh(FString::Printf(TEXT("/Game/Structures/site_props/SM_Site_%s/StaticMeshes/SM_Site_%s"), Name, Name));
		};
		UStaticMesh* Tape = Mesh(TEXT("Tape"));
		UStaticMesh* Stake = Mesh(TEXT("Stake"));
		UStaticMesh* Hoard = Mesh(bZebra ? TEXT("HoardingZebra") : TEXT("Hoarding"));
		const auto Put = [this](UStaticMesh* M, const FVector& At, float YawDeg, const FVector& Scale, uint8 Kind)
		{
			if (!M)
			{
				return;
			}
			UStaticMeshComponent* C = AddPart(M, Body);
			C->SetRelativeLocation(At);
			C->SetRelativeRotation(FRotator(0.0f, YawDeg, 0.0f));
			C->SetRelativeScale3D(Scale);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SiteParts.Add(C);
			SiteKind.Add(Kind);
			SiteScale.Add(Scale);
		};
		const FVector2D Lo = FootprintCm.Min, Hi = FootprintCm.Max;
		const FVector2D Size = Hi - Lo;
		const uint32 H = CosmeticHash(SimId);
		// (1) o gabarito: a fita em lances de ~4 m nos 4 lados do contorno (cada lance traz as 2 estacas das pontas), e uma estaca solta com
		// a fita amarrada em cada canto
		const FVector2D Corners[4] = { Lo, FVector2D(Hi.X, Lo.Y), Hi, FVector2D(Lo.X, Hi.Y) };
		for (int32 K = 0; K < 4; ++K)
		{
			const FVector2D A = Corners[K], B = Corners[(K + 1) % 4];
			const float Len = float(FVector2D::Distance(A, B));
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 400.0f));
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(B.Y - A.Y), float(B.X - A.X)));
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2D M = FMath::Lerp(A, B, (I + 0.5) / N);
				Put(Tape, FVector(M.X, M.Y, 0.0), Yaw, FVector(Len / (400.0f * N), 1.0f, 1.0f), 0);
			}
			Put(Stake, FVector(A.X, A.Y, 0.0), float((H >> (K * 3)) % 360u), FVector(1.0f), 0);
		}
		// (2) o tapume: atrás (o lado −X, o oposto da porta do prédio virado pra +X) e na lateral +Y até a metade, 1 m pra fora do contorno,
		// a frente das chapas (a faixa e a placa) pra fora. Em lances de ~4 m
		const auto Wall = [&](const FVector2D& A, const FVector2D& B, float OutYaw)
		{
			const float Len = float(FVector2D::Distance(A, B));
			const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 400.0f));
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(B.Y - A.Y), float(B.X - A.X)));
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2D M = FMath::Lerp(A, B, (I + 0.5) / N);
				Put(Hoard, FVector(M.X, M.Y, 0.0), Yaw + OutYaw, FVector(Len / (400.0f * N), 1.0f, 1.0f), 1);
			}
		};
		// (01/11) 2,2 m pra fora do contorno: o andaime em volta da obra ocupa até ~1,3 m da parede
		Wall(FVector2D(Lo.X - 220.0, Hi.Y + 220.0), FVector2D(Lo.X - 220.0, Lo.Y - 220.0), 0.0f);
		Wall(FVector2D(Lo.X - 220.0, Hi.Y + 220.0), FVector2D(Lo.X + 0.5 * Size.X, Hi.Y + 220.0), 180.0f);
		// (3) o material dentro do lote, perto da frente e do lado −Y (o caminho do Dozer fica livre): os paletes de bloco e de cimento, o
		// feixe de vergalhão ao longo da borda, os montes de areia e de brita. Lote pequeno (< 9 m): só um palete e a areia
		const bool bSmall = Size.X < 900.0 || Size.Y < 900.0;
		const FVector2D C = 0.5 * (Lo + Hi);
		const float J = float(H % 1000u) / 1000.0f;
		Put(Mesh(TEXT("Blocks")), FVector(Hi.X - 150.0, Lo.Y + 160.0, 0.0), 8.0f + 20.0f * J, FVector(1.0f), 2);
		Put(Mesh(TEXT("Sand")), FVector(C.X + 0.15 * Size.X, C.Y + 0.2 * Size.Y, 0.0), 360.0f * J, FVector(bSmall ? 0.6f : 1.0f), 2);
		if (!bSmall)
		{
			Put(Mesh(TEXT("Blocks")), FVector(Hi.X - 150.0, Lo.Y + 300.0, 0.0), -6.0f + 10.0f * J, FVector(1.0f), 2);
			Put(Mesh(TEXT("Cement")), FVector(Hi.X - 290.0, Lo.Y + 170.0, 0.0), 90.0f + 15.0f * J, FVector(1.0f), 2);
			Put(Mesh(TEXT("Rebar")), FVector(C.X, Lo.Y + 90.0, 0.0), 2.0f, FVector(FMath::Min(1.0f, float(Size.X) / 750.0f), 1.0f, 1.0f), 2);
			Put(Mesh(TEXT("Gravel")), FVector(C.X - 0.2 * Size.X, C.Y + 0.25 * Size.Y, 0.0), 120.0f * J, FVector(1.0f), 2);
		}
	}
	// as fases: o prédio (a laje inclusive) só aparece com a obra andando; o gabarito só antes; o tapume até o fim; o material encolhe e some
	// enquanto a obra o usa (o de dentro do lote antes de as paredes passarem dele)
	if (FMath::Abs(BuildFraction - SiteShownFraction) < 0.01f && SiteShownFraction >= 0.0f)
	{
		return;
	}
	SiteShownFraction = BuildFraction;
	const bool bMarking = bWant && BuildFraction <= 0.0f;
	for (UStaticMeshComponent* Part : HullParts)
	{
		if (Part && Part != Scaffold)
		{
			Part->SetVisibility(!bMarking);
		}
	}
	for (int32 K = 0; K < SiteParts.Num(); ++K)
	{
		UStaticMeshComponent* C = SiteParts[K];
		if (!C)
		{
			continue;
		}
		bool bShow = bWant;
		FVector S = SiteScale[K];
		if (SiteKind[K] == 0)
		{
			bShow = bMarking;
		}
		else if (SiteKind[K] == 2)
		{
			const float Left = 1.0f - FMath::Clamp(BuildFraction / 0.35f, 0.0f, 1.0f);   // usado até 35% da obra
			bShow = bWant && Left > 0.05f;
			S.Z *= FMath::Max(Left, 0.05f);
		}
		C->SetVisibility(bShow);
		C->SetRelativeScale3D(S);
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

bool ARTSUnitActor::GetBladeLip(FVector& OutLipCm, FVector& OutFwd, float& OutHalfWidthCm) const
{
	const UStaticMesh* Mesh = BladePart ? BladePart->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	// a borda de cima, no meio da largura, na face da frente da caixa da malha (no quadro da peça: x pra frente, z pra cima)
	const FBox B = Mesh->GetBoundingBox();
	const FTransform& T = BladePart->GetComponentTransform();
	OutLipCm = T.TransformPosition(FVector(B.Max.X, 0.5f * (B.Min.Y + B.Max.Y), B.Max.Z));
	OutFwd = T.TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal2D();
	OutHalfWidthCm = 0.5f * float(B.Max.Y - B.Min.Y) * float(T.GetScale3D().Y);
	return !OutFwd.IsNearlyZero();
}

void ARTSUnitActor::SetWorking(bool bInWorking)
{
	bWorking = bInWorking;
}

void ARTSUnitActor::TickStructure(float DeltaSeconds)
{
	StepFlag(DeltaSeconds);   // (01/11) as bandeiras do prédio no vento do mapa
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
	// a solda na obra: faíscas na borda do corte. (31/10, a revisão de vídeo: "as faíscas de solda começam sem ninguém") só com a obra
	// subindo agora (o Dozer trabalhando: SetStructureState)
	if (BuildFraction < 1.0f && !bFogHidden && GetWorld() && GetWorld()->GetTimeSeconds() - LastBuildRiseS < WeldHoldS)
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

void ARTSUnitActor::SetPose(const FVector& InLocation, float HullYawDeg, float TurretYawWorldDeg, const FVector& GroundNormal, float DeltaSeconds)
{
	if (bDead)
	{
		return;
	}
	FVector Location = InLocation;
	if (bStructure)
	{
		// prédio não balança nem inclina: fica no nível do lote (a laje desce até 3 m)
		SetActorLocation(Location);
		Body->SetWorldRotation(FRotator(0.0f, HullYawDeg, 0.0f));
		if (bGroundCutOn && !Body->GetComponentTransform().Equals(GroundCutAt, 1.0))
		{
			UpdateGroundCut(false);   // (o prédio ligou o recorte antes da 1ª pose: refaz no lugar dele)
			UpdateGroundCut(true);
		}
		if (TurretParts.Num())
		{
			// (16/10) a torre do prédio gira com a mira da simulação (a caixa da Patriot): antes ficava parada na pose do
			// modelo, e o míssil saía pela ponta oposta às bocas
			TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg + TurretYawOffsetDeg), 0.0f));
		}
		return;
	}
	// (01/11) a unidade que nasceu da fábrica com túnel: sobe do fundo pela rampa até a posição da simulação
	ApplyTunnelExit(Location, DeltaSeconds);
	// o Dozer: a lâmina desce e raspa quando ele trabalha, sobe quando anda
	if (BladePivot)
	{
		BladeAmount = FMath::FInterpTo(BladeAmount, bWorking ? 1.0f : 0.0f, DeltaSeconds, 3.0f);
		const float LastTurn = FMath::FloorToFloat(BladeClock * 2.4f / UE_TWO_PI);
		BladeClock += DeltaSeconds;
		// (30/10, 3ª volta) a lâmina passando a subir (o seno cruzando o zero pra cima, a subida mais rápida): a terra cai dela
		if (BladeAmount > 0.7f && FMath::FloorToFloat(BladeClock * 2.4f / UE_TWO_PI) != LastTurn)
		{
			++BladeLifts;
		}
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
			for (USpotLightComponent* L : BeaconLights)
			{
				if (L)
				{
					L->SetIntensity(BeaconPeakCd * BladeAmount);   // (31/10) o pico do facho (a v1: o Sentry de 55 W, 80 000 cd)
				}
			}
			if (BeaconGlow)
			{
				BeaconGlow->SetIntensity(BeaconGlowCd * BladeAmount);
			}
		}
	}
	else if (!bAircraft)
	{
		// (25/10, v5) o veículo de chão sem lâmina: a peça que gira (o prato do radar do Linebacker) roda sempre no rpm dela
		for (int32 K = 0; K < SpinPivots.Num(); ++K)
		{
			SpinAngleDeg[K] = FMath::Fmod(SpinAngleDeg[K] + SpinRpm[K] * 6.0f * DeltaSeconds, 360.0f);
			SpinPivots[K]->SetRelativeRotation(FRotator(0.0f, SpinAngleDeg[K], 0.0f));
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
		// (01/11) o giro do casco neste quadro: cada lado da lagarta anda Travel − giro × y (no giro no lugar, um pra frente, o outro pra trás)
		const float DYawRad = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(LastYawDeg, HullYawDeg));
		if (WheelPivots.Num() && TrackParts.Num())
		{
			// as rodas da lagarta: cada uma rola o que o lado dela andou, pelo raio dela
			if (WheelAngleEachDeg.Num() != WheelPivots.Num())
			{
				WheelAngleEachDeg.Init(0.0f, WheelPivots.Num());
			}
			for (int32 K = 0; K < WheelPivots.Num(); ++K)
			{
				const float Side = float(WheelPivots[K]->GetRelativeLocation().Y);
				WheelAngleEachDeg[K] = FMath::Fmod(WheelAngleEachDeg[K] + FMath::RadiansToDegrees((Travel - DYawRad * Side) / WheelRadiusCm[K]), 360.0f);
				WheelPivots[K]->SetRelativeRotation(FRotator(-WheelAngleEachDeg[K], 0.0f, 0.0f));
			}
		}
		else if (WheelPivots.Num())
		{
			WheelAngleDeg = FMath::Fmod(WheelAngleDeg + FMath::RadiansToDegrees(Travel / WheelRadiusCm[0]), 360.0f);
			for (int32 K = 0; K < WheelPivots.Num(); ++K)
			{
				const float Scale = WheelRadiusCm[0] / WheelRadiusCm[K];
				WheelPivots[K]->SetRelativeRotation(FRotator(-WheelAngleDeg * Scale, 0.0f, 0.0f));
			}
		}
		StepTracks(Travel, DYawRad);
	}
	LastLocation = Location;
	LastYawDeg = HullYawDeg;
	bHasLastPose = true;
	StepSuspension(DeltaSeconds, ForwardAccel, LateralAccel);
	if (bInfantry)
	{
		SuspPitch = SuspRoll = 0.0f;   // (o soldado não tem mola de casco)
	}

	SetActorLocation(Location);
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal);
	TiltSmoothed = FQuat::Slerp(TiltSmoothed, Tilt, 1.0f - FMath::Exp(-DeltaSeconds * 8.0f));
	// arrancou: o nariz sobe (pitch +); freou: desce; na curva, rola pra fora
	const FQuat Yaw = FRotator(0.0f, HullYawDeg, 0.0f).Quaternion();
	const FQuat Sway = FRotator(SuspPitch + TunnelPitchDeg, 0.0f, SuspRoll).Quaternion();
	Body->SetWorldRotation(TiltSmoothed * Yaw * Sway);
	TurretPivot->SetRelativeRotation(FRotator(0.0f, FRotator::NormalizeAxis(TurretYawWorldDeg - HullYawDeg + TurretYawOffsetDeg), 0.0f));
	// (30/10, o Felipe: "quando ele atira olhando pro lado a torreta vai pra trás no sentido do corpo dele") o recuo vai pra trás
	// na linha do cano (a mira no quadro do casco), não no eixo do casco
	const float AimRelRad = FMath::DegreesToRadians(TurretYawWorldDeg - HullYawDeg);
	TurretPivot->SetRelativeLocation(TurretPivotCm - RecoilCm * FVector(FMath::Cos(AimRelRad), FMath::Sin(AimRelRad), 0.0f));
	if (bInfantry && !SoldierMesh && LegPivots.Num() == 2 && DeltaSeconds > 1e-4f)
	{
		// (30/10, simulação 26) o passo: a fase anda com o chão (2π por ciclo de StrideCm: a perna não patina), o balanço de ±32° (a
		// coxa de quem corre) entra e sai em ~0,15 s com o andar, e o corpo sobe e desce duas vezes por ciclo (3 cm × 1,25)
		const float Moved = float(FVector::Dist2D(Location, LastPoseForStep));
		LastPoseForStep = Location;
		StepPhase = FMath::Fmod(StepPhase + Moved / FMath::Max(StrideCm, 50.0f) * UE_TWO_PI, UE_TWO_PI);
		StepAmount = FMath::FInterpTo(StepAmount, Moved / DeltaSeconds > 60.0f ? 1.0f : 0.0f, DeltaSeconds, 7.0f);
		const float Swing = 32.0f * StepAmount * FMath::Sin(StepPhase);
		LegPivots[0]->SetRelativeRotation(FRotator(Swing, 0.0f, 0.0f));
		LegPivots[1]->SetRelativeRotation(FRotator(-Swing, 0.0f, 0.0f));
		TurretPivot->AddRelativeLocation(FVector(0.0f, 0.0f, 3.75f * StepAmount * (FMath::Abs(FMath::Sin(StepPhase)) - 0.5f)));
	}
	StepFlag(DeltaSeconds);   // (30/10) a bandeira no vento aparente
	// (30/10, a infantaria nova) o corpo de verdade: a velocidade no chão e o tiro pra animação, e o fuzil na mão direita apontando pra
	// mira (o modelo do fuzil tem o cano em +Y: girado pro +X da torre)
	if (SoldierMesh && DeltaSeconds > 1e-4f)
	{
		const float Moved = float(FVector::Dist2D(Location, LastSoldierPos));
		LastSoldierPos = Location;
		SoldierSpeedCmS = bInsideTransport ? 0.0f : FMath::FInterpTo(SoldierSpeedCmS, Moved / DeltaSeconds, DeltaSeconds, 10.0f);   // (simulação 35) na escotilha não anda
		if (URTSSoldierAnimInstance* Anim = Cast<URTSSoldierAnimInstance>(SoldierMesh->GetAnimInstance()))
		{
			Anim->SpeedCmS = SoldierSpeedCmS;
			// (01/11) o antitanque ajoelha pra atirar (o tubo no ombro, o corpo baixo: a pose da arma pesada, diferente do fuzileiro em pé)
			// enquanto está no combate (o último tiro há menos de 2,5 s); em pé de novo quando para de atirar
			const bool bAtKneel = bShoulderLauncher && !bInsideTransport && SoldierPosture == 0 && GetWorld()
				&& GetWorld()->GetTimeSeconds() - SoldierShotAt < 2.5;
			Anim->Posture = bAtKneel ? 1 : SoldierPosture;
			Anim->bFiring = GetWorld() && GetWorld()->GetTimeSeconds() - SoldierShotAt < 0.7;
			Anim->ShotAgeS = GetWorld() ? float(GetWorld()->GetTimeSeconds() - SoldierShotAt) : 1.0e3f;
		}
		PlaceSoldierHelmet();
		if (SoldierRifle && bShoulderLauncher && SoldierRifle->GetStaticMesh())
		{
			// (01/11, ele: "as infantarias com armas pesadas tipo as bazucas estão na mesma posição de armas leves dos outros") o tubo
			// apoiado em cima do ombro direito (o ponto de equilíbrio a 40% de trás pra frente: o tubo passa atrás do ombro, como no
			// RPG-7, no Javelin e no Carl Gustaf), apontado pra mira e 3° pra cima; andando, empinado ~25° no ombro (o jeito de levar o
			// tubo comprido), voltando a apontar no tiro. O modelo tem o cano em +Y, como o fuzil
			const FBox B = SoldierRifle->GetStaticMesh()->GetBoundingBox();
			const FVector S = SoldierRifle->GetRelativeScale3D();
			const FVector Balance(B.GetCenter().X, B.Min.Y + 0.4 * (B.Max.Y - B.Min.Y), B.Min.Z + 0.45 * (B.Max.Z - B.Min.Z));   // (o pé do tubo: medido, o Javelin −6..24 cm)
			const FVector Shoulder = SoldierMesh->GetBoneLocation(TEXT("upperarm_r"));
			const FVector Neck = SoldierMesh->GetBoneLocation(TEXT("neck_01"));
			const bool bShooting = GetWorld() && GetWorld()->GetTimeSeconds() - SoldierShotAt < 1.5;
			const float Carry = bShooting ? 0.0f : FMath::Clamp((SoldierSpeedCmS - 60.0f) / 120.0f, 0.0f, 1.0f);
			LauncherCarry = FMath::FInterpTo(LauncherCarry, Carry, DeltaSeconds, 4.0f);
			const FQuat Q = FRotator(0.0f, TurretPivot->GetComponentRotation().Yaw - 90.0f, 0.0f).Quaternion()
				* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(FMath::Lerp(3.0f, 25.0f, LauncherCarry)));
			const FVector At = Shoulder + FVector(Neck.X - Shoulder.X, Neck.Y - Shoulder.Y, 0.0) * 0.3 + FVector(0.0, 0.0, 10.0 * S.Z);   // (o alto do ombro ~10 cm acima da junta)
			SoldierRifle->SetWorldLocationAndRotation(At - Q.RotateVector(Balance * S), Q);
		}
		else if (SoldierRifle)
		{
			const FVector Hand = SoldierMesh->GetBoneLocation(TEXT("hand_r"));
			SoldierRifle->SetWorldLocationAndRotation(Hand, (TurretPivot->GetComponentRotation() + FRotator(0.0f, -90.0f, 0.0f)).Quaternion());
		}
	}
	// (30/10) o cano entra na torre, no eixo dele (no quadro da torre: o modelo pode vir girado TurretYawOffsetDeg)
	if (BarrelParts.Num())
	{
		const float Off = FMath::DegreesToRadians(-TurretYawOffsetDeg);
		const FVector Axis(FMath::Cos(Off), FMath::Sin(Off), 0.0f);
		for (int32 K = 0; K < BarrelParts.Num(); ++K)
		{
			if (UStaticMeshComponent* Barrel = BarrelParts[K].Get())
			{
				Barrel->SetRelativeLocation(BarrelRestCm[K] - Axis * BarrelRecoilCm);
			}
		}
	}
	StepLauncher(DeltaSeconds);   // (23/10, v4) o lançador que levanta (o Tomahawk)
	StepHatches(DeltaSeconds);    // (01/11) as escotilhas do tiro de dentro
	StepPulse();                  // (01/11) o pulso de luz da leitura
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
	if (bInsideTransport && PassengerTransport != 0)
	{
		return PassengerMouthCm;   // (01/11) de dentro do transporte: a escotilha aberta
	}
	FVector Mouth, Dir;
	if (LauncherTubeWorld(LauncherTube, Mouth, Dir))
	{
		return Mouth;                              // (23/10, v4) a boca do tubo do lançador, na pose de agora (levantado no tiro)
	}
	if (SoldierRifle && SoldierRifle->GetStaticMesh() && SoldierMesh)
	{
		// (31/10, o Felipe: "a luz do tiro não segue nunca a saída da arma, ele toma dano, agacha, continua no mesmo lugar") o soldado:
		// a ponta do cano do fuzil na mão, na pose de agora (em pé, ajoelhado, encolhido, no tranco), não o pivô na altura de quem está
		// em pé. O modelo do fuzil tem o cano em +Y; a boca na ponta dele, na altura do cano (a ~70% da altura da malha: o carregador e a
		// coronha descem abaixo dele)
		const FBox B = SoldierRifle->GetStaticMesh()->GetBoundingBox();
		const FVector Local(B.GetCenter().X, B.Max.Y, B.Min.Z + 0.7 * (B.Max.Z - B.Min.Z));
		return SoldierRifle->GetComponentTransform().TransformPosition(Local);
	}
	return TurretPivot->GetComponentTransform().TransformPosition(FVector(MuzzleForwardCm, 0.0f, MuzzleHeightCm));
}

FVector ARTSUnitActor::MuzzleDirection() const
{
	if (bInsideTransport && PassengerTransport != 0)
	{
		return PassengerAimDir;   // (01/11)
	}
	FVector Mouth, Dir;
	if (LauncherTubeWorld(LauncherTube, Mouth, Dir))
	{
		return Dir;
	}
	if (SoldierRifle && SoldierMesh)
	{
		return SoldierRifle->GetComponentTransform().TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal();   // (31/10) o cano do fuzil
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
	if (SoldierMesh)
	{
		// (30/10) o soldado: o tranco do corpo (o clipe de reação), não a mola do casco
		if (URTSSoldierAnimInstance* Anim = Cast<URTSSoldierAnimInstance>(SoldierMesh->GetAnimInstance()))
		{
			Anim->NoteHit(HashCombine(SimId, GetTypeHash(FromCm)));
		}
		return;
	}
	const FVector Push = FVector(FVector2D(GetActorLocation()) - FromCm, 0.0f).GetSafeNormal();
	if (Push.IsNearlyZero())
	{
		return;
	}
	// (30/10) no quadro do casco: o ator não gira (só o Body), e o tranco ia pelos eixos do mundo (o acerto de lado num tanque virado
	// pro norte levantava o nariz)
	const FVector Local = FRotator(0.0f, GetHullYawDeg(), 0.0f).UnrotateVector(Push);
	// (29/10, a vida do acerto) o giro do casco ∝ impulso ÷ massa: o tiro de 120 mm (~14 000 kg·m/s) sacode a picape de 2,5 t muito mais
	// que o tanque de 60 t. Pela raiz da razão de massa (a física pura daria ~50× na picape: ela capotaria), com teto de 4×
	const float MassK = FMath::Clamp(FMath::Sqrt(60.0f / FMath::Max(HitMassT > 0.0f ? HitMassT : 60.0f, 1.0f)), 1.0f, 4.0f);
	SuspPitchRate += HitKickDeg * MassK * -Local.X;
	SuspRollRate += HitKickDeg * MassK * Local.Y;
}

FVector ARTSUnitActor::GetSoldierChestCm() const
{
	if (SoldierMesh)
	{
		const int32 Bone = SoldierMesh->GetBoneIndex(TEXT("spine_03"));
		if (Bone != INDEX_NONE)
		{
			return SoldierMesh->GetBoneLocation(TEXT("spine_03"));
		}
	}
	return GetActorLocation() + FVector(0.0, 0.0, 150.0);
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

float ARTSUnitActor::GetHullYawDeg() const
{
	return Body ? float(Body->GetComponentRotation().Yaw) : float(GetActorRotation().Yaw);
}

void ARTSUnitActor::ApplyPaintTint(const FRTSUnitVisual& Visual)
{
	// (31/10, o teste de leitura; o Felipe: "o helicóptero achei meio escondido") a tinta de uma unidade só, por cima da compartilhada
	// (o MI_TankPaint de todos os veículos da USA): a cor base na instância dinâmica de cada peça com ela (a carcaça queimada troca
	// depois na mesma instância). A cor do jogador, o metal e o vidro ficam
	if (Visual.PaintTint.A <= 0.0f)
	{
		return;
	}
	const FLinearColor Base(Visual.PaintTint.R, Visual.PaintTint.G, Visual.PaintTint.B, 1.0f);
	int32 Tinted = 0;
	auto Tint = [&Base, &Tinted](UStaticMeshComponent* Part)
	{
		for (int32 Slot = 0; Part && Slot < Part->GetNumMaterials(); ++Slot)
		{
			const UMaterialInterface* M = Part->GetMaterial(Slot);
			if (M && M->GetName().Contains(TEXT("TankPaint")))
			{
				if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(Slot))
				{
					MID->SetVectorParameterValue(TEXT("BaseColor"), Base);
					++Tinted;
				}
			}
		}
	};
	for (UStaticMeshComponent* P : HullParts)
	{
		Tint(P);
	}
	for (UStaticMeshComponent* P : TurretParts)
	{
		Tint(P);
	}
	UE_LOG(LogTemp, Log, TEXT("RTS %s: a tinta própria (%.3f, %.3f, %.3f) em %d peças"), *Visual.Key, Base.R, Base.G, Base.B, Tinted);
}

void ARTSUnitActor::SetupFlag(const FRTSUnitVisual& Visual)
{
	if (!Body)
	{
		return;
	}
	// (01/11) a bandeira da Technical ("flag") e as do modelo ("flags": os prédios e os veículos), todas no mesmo pano e no mesmo vento
	TArray<FVector> Masts;
	TArray<FVector2D> SizesCm;
	TArray<bool> OnTurret;
	TArray<int32> Emblems;
	if (Visual.bFlag)
	{
		Masts.Add(Visual.FlagMastCm);
		SizesCm.Add(Visual.FlagSizeM * 100.0);
		OnTurret.Add(false);
		Emblems.Add(0);
	}
	for (int32 K = 0; K < Visual.ExtraFlagMastCm.Num(); ++K)
	{
		const bool bTurret = Visual.ExtraFlagTurret.IsValidIndex(K) && Visual.ExtraFlagTurret[K] && TurretPivot;
		Masts.Add(bTurret ? Visual.ExtraFlagMastCm[K] - TurretPivotCm : Visual.ExtraFlagMastCm[K]);
		SizesCm.Add((Visual.ExtraFlagSizeM.IsValidIndex(K) ? Visual.ExtraFlagSizeM[K] : FVector2D(1.0, 0.65)) * 100.0);
		OnTurret.Add(bTurret);
		Emblems.Add(Visual.ExtraFlagEmblem.IsValidIndex(K) ? Visual.ExtraFlagEmblem[K] : 0);
	}
	if (!Masts.Num())
	{
		return;
	}
	UStaticMesh* Cloth = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Structures/Flag/SM_FlagCloth/StaticMeshes/SM_FlagCloth.SM_FlagCloth"));
	if (!Cloth)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS: sem o SM_FlagCloth (Tools/import_provisional.py, os materiais): %s fica sem bandeira"), *Visual.Key);
		return;
	}
	for (int32 K = 0; K < Masts.Num(); ++K)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Cloth);
		C->SetupAttachment(OnTurret[K] ? TurretPivot.Get() : Body.Get());
		C->SetRelativeLocation(Masts[K]);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetBoundsScale(3.0f);   // (o pano sai da caixa da malha de 1 m pelo vento: sem isso some na borda da tela)
		C->RegisterComponent();
		C->SetCustomPrimitiveDataFloat(5, float(SizesCm[K].X));
		C->SetCustomPrimitiveDataFloat(6, float(SizesCm[K].Y));
		C->SetCustomPrimitiveDataFloat(11, float(Emblems[K]));
		FlagMeshes.Add(C);
		FlagOffset.Add(0.37f * float(K));   // (cada bandeira na sua fase: não batem juntas)
	}
	// a cor: a do casco, se já chegou (o SetPlayerColor pinta depois também)
	for (UStaticMeshComponent* Part : HullParts)
	{
		const TArray<float>& Cpd = Part ? Part->GetCustomPrimitiveData().Data : TArray<float>();
		if (Cpd.Num() >= DataPlayerColor + 4)
		{
			for (UStaticMeshComponent* C : FlagMeshes)
			{
				C->SetCustomPrimitiveDataVector4(DataPlayerColor, FVector4(Cpd[DataPlayerColor], Cpd[DataPlayerColor + 1], Cpd[DataPlayerColor + 2], 1.0f));
			}
			break;
		}
	}
	FlagLastPos = GetActorLocation();
	if (const URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr)
	{
		FlagYawWorld = FMath::DegreesToRadians(Match->GetWindYawDeg());
		FlagLift = FMath::Clamp(Match->GetWindSpeedMs() / 7.0f, 0.12f, 1.0f);
		FlagRate = 0.6f + 0.32f * Match->GetWindSpeedMs();
	}
	SendFlagWind(true);
}

void ARTSUnitActor::SendFlagWind(bool bForce)
{
	// (01/11) o vento do pano vai pros dados de primitiva só quando muda (o prédio: uma vez; o veículo: quando acelera ou vira). A fase de
	// cada pano continua sem pular quando o ritmo muda: fase = tempo × ritmo + deslocamento, e o deslocamento absorve a troca
	if (!bForce && FMath::Abs(FMath::FindDeltaAngleRadians(FlagSentYaw, FlagYawWorld)) < FMath::DegreesToRadians(1.5f)
		&& FMath::Abs(FlagSentLift - FlagLift) < 0.02f && FMath::Abs(FlagSentRate - FlagRate) < 0.04f)
	{
		return;
	}
	const float T = GetWorld() ? float(GetWorld()->GetTimeSeconds()) : 0.0f;
	for (int32 K = 0; K < FlagMeshes.Num(); ++K)
	{
		UStaticMeshComponent* C = FlagMeshes[K];
		if (!C)
		{
			continue;
		}
		if (FlagSentRate >= 0.0f)
		{
			FlagOffset[K] = FMath::Fmod(T * FlagSentRate + FlagOffset[K] - T * FlagRate, 1000.0f);
		}
		C->SetCustomPrimitiveDataFloat(7, FlagYawWorld);
		C->SetCustomPrimitiveDataFloat(8, FlagLift);
		C->SetCustomPrimitiveDataFloat(9, FlagRate);
		C->SetCustomPrimitiveDataFloat(10, FlagOffset[K]);
	}
	FlagSentYaw = FlagYawWorld;
	FlagSentLift = FlagLift;
	FlagSentRate = FlagRate;
}

void ARTSUnitActor::StepFlag(float DeltaSeconds)
{
	if (!FlagMeshes.Num() || !Body)
	{
		return;
	}
	const URTSMatchSubsystem* Match = GetWorld() ? GetWorld()->GetSubsystem<URTSMatchSubsystem>() : nullptr;
	const float WYaw = FMath::DegreesToRadians(Match ? Match->GetWindYawDeg() : 0.0f);
	const float WSpeed = Match ? Match->GetWindSpeedMs() : 4.0f;
	if (bStructure)
	{
		// o prédio: o pano só aparece pronto (o mastro ainda não subiu na obra) e só recebe o vento do mapa se ele mudar
		const bool bShow = !bBuildLook && !bDead;
		if (bShow != bFlagShown)
		{
			bFlagShown = bShow;
			for (UStaticMeshComponent* C : FlagMeshes)
			{
				if (C)
				{
					C->SetVisibility(bShow);
				}
			}
		}
		FlagYawWorld = WYaw;
		FlagLift = FMath::Clamp(WSpeed / 7.0f, 0.12f, 1.0f);
		FlagRate = 0.6f + 0.32f * WSpeed;
		SendFlagWind(false);
		return;
	}
	// o veículo: o vento aparente (o do mapa menos o andar dele); o pano aponta pra onde ele vai
	FVector2D App = FVector2D(FMath::Cos(WYaw), FMath::Sin(WYaw)) * WSpeed;
	const FVector Pos = GetActorLocation();
	if (DeltaSeconds > 1e-4f)
	{
		App -= FVector2D(Pos - FlagLastPos) / 100.0 / DeltaSeconds;
	}
	FlagLastPos = Pos;
	const float S = float(App.Size());
	if (S > 0.3f)
	{
		// o pano gira pro vento em ~0,3 s (não pula de um quadro pro outro na curva)
		const float Want = FMath::Atan2(float(App.Y), float(App.X));
		FlagYawWorld += FMath::FindDeltaAngleRadians(FlagYawWorld, Want) * FMath::Min(1.0f, DeltaSeconds / 0.3f);
	}
	// o quanto o pano levanta: pendurado sem vento, na horizontal a partir de ~7 m/s (a bandeira de tecido de bandeira, ~150 g/m²); a onda
	// corre do mastro pra ponta mais rápido com mais vento (Strouhal: a 5 m/s uma bandeira de 1 m bate ~2 vezes por segundo)
	FlagLift = FMath::FInterpTo(FlagLift, FMath::Clamp(S / 7.0f, 0.12f, 1.0f), DeltaSeconds, 2.0f);
	FlagRate = FMath::FInterpTo(FlagRate, 0.6f + 0.32f * S, DeltaSeconds, 2.0f);
	SendFlagWind(false);
}

void ARTSUnitActor::PlaceSoldierHelmet()
{
	if (!SoldierHelmet || !SoldierMesh)
	{
		return;
	}
	// no alto da cabeça, no eixo do pescoço (o osso da cabeça fica na nuca): a calota ~11 cm acima dele na escala do corpo, de pé ou caindo
	const FVector Head = SoldierMesh->GetBoneLocation(TEXT("head"));
	const FVector Neck = SoldierMesh->GetBoneLocation(TEXT("neck_01"));
	const FVector Up = (Head - Neck).GetSafeNormal(1e-3, FVector::UpVector);
	const FVector Fwd = FVector::VectorPlaneProject(TurretPivot->GetForwardVector(), Up).GetSafeNormal(1e-3, FVector::ForwardVector);
	SoldierHelmet->SetWorldLocationAndRotation(Head + Up * (11.0f * SoldierMesh->GetRelativeScale3D().Z) + Fwd * 2.0f,
		FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat());
}

void ARTSUnitActor::SetupSoldierLook(const FRTSUnitVisual& Visual)
{
	// (30/10, o soldado vestido; Tools/soldier_dress.py) a farda pintada nas UVs do corpo e as peças rígidas presas nos ossos, modeladas em
	// volta do corpo na pose de repouso: cada uma vai pro osso dela com a diferença da pose de repouso (a origem dela no espaço do corpo
	// relativa ao osso), e anda com ele (o capacete cai com a cabeça). O M_Soldier: a cor da textura (o corpo) ou dos vértices (as peças),
	// a do time onde o alfa marca (a faixa do ombro e a do capacete)
	static TWeakObjectPtr<UMaterialInterface> SoldierMat;
	if (!SoldierMat.IsValid())
	{
		SoldierMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Soldier.M_Soldier"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	// (01/11, a 3ª volta: o soldado de verdade, Tools/soldier_outfit.py) a malha única (o corpo vestido, a cabeça humana e o equipamento
	// juntos, no esqueleto do Manny) com a cor nos vértices: os slots "VC" e "VC_Team" (a faixa do ombro e a do capacete na cor do time)
	bool bVertexColor = false;
	if (const USkeletalMesh* Sk = Cast<USkeletalMesh>(SoldierMesh->GetSkinnedAsset()))
	{
		for (const FSkeletalMaterial& Sm : Sk->GetMaterials())
		{
			bVertexColor |= Sm.MaterialSlotName.ToString().StartsWith(TEXT("VC"));
		}
	}
	if (SoldierMat.IsValid() && bVertexColor)
	{
		const USkeletalMesh* Sk = Cast<USkeletalMesh>(SoldierMesh->GetSkinnedAsset());
		for (int32 Slot = 0; Slot < SoldierMesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SoldierMat.Get(), this))
			{
				const bool bTeam = Sk && Sk->GetMaterials().IsValidIndex(Slot) && Sk->GetMaterials()[Slot].MaterialSlotName.ToString().Contains(TEXT("Team"));
				M->SetScalarParameterValue(TEXT("UseTex"), 0.0f);
				M->SetScalarParameterValue(TEXT("TeamSlot"), bTeam ? 1.0f : 0.0f);
				M->SetVectorParameterValue(TEXT("TeamColor"), Visual.SoldierTeam);
				SoldierMesh->SetMaterial(Slot, M);
			}
		}
	}
	else if (SoldierMat.IsValid() && Visual.SoldierPieces.Num() > 0)
	{
		for (int32 Slot = 0; Slot < SoldierMesh->GetNumMaterials() && Slot < Visual.SoldierPaint.Num(); ++Slot)
		{
			UTexture2D* Paint = LoadObject<UTexture2D>(nullptr, *Visual.SoldierPaint[Slot], nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (UMaterialInstanceDynamic* M = Paint ? UMaterialInstanceDynamic::Create(SoldierMat.Get(), this) : nullptr)
			{
				M->SetTextureParameterValue(TEXT("Paint"), Paint);
				M->SetScalarParameterValue(TEXT("UseTex"), 1.0f);
				M->SetVectorParameterValue(TEXT("TeamColor"), Visual.SoldierTeam);
				SoldierMesh->SetMaterial(Slot, M);
			}
		}
		const FReferenceSkeleton& Ref = SoldierMesh->GetSkinnedAsset()->GetRefSkeleton();
		for (const FRTSUnitVisual::FSoldierPiece& P : Visual.SoldierPieces)
		{
			UStaticMesh* Mesh = LoadMesh(P.Mesh);
			const int32 Bone = Ref.FindBoneIndex(P.Bone);
			if (!Mesh || Bone == INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning, TEXT("RTS soldado: a peça %s (osso %s) não carregou"), *P.Mesh, *P.Bone.ToString());
				continue;
			}
			const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone);
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(Mesh);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetReceivesDecals(false);
			C->SetupAttachment(SoldierMesh, P.Bone);
			C->SetRelativeTransform(FTransform(P.OriginCm).GetRelativeTransform(BoneCS));
			// a cor dos vértices; o slot "Team" do GLB (a faixa do capacete) na cor do jogador
			for (int32 K = 0; K < FMath::Max(Mesh->GetStaticMaterials().Num(), 1); ++K)
			{
				if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SoldierMat.Get(), this))
				{
					const bool bTeam = Mesh->GetStaticMaterials().IsValidIndex(K) && Mesh->GetStaticMaterials()[K].MaterialSlotName.ToString().Contains(TEXT("Team"));
					M->SetScalarParameterValue(TEXT("UseTex"), 0.0f);
					M->SetScalarParameterValue(TEXT("TeamSlot"), bTeam ? 1.0f : 0.0f);
					M->SetVectorParameterValue(TEXT("TeamColor"), Visual.SoldierTeam);
					C->SetMaterial(K, M);
				}
			}
			C->RegisterComponent();
			SoldierPieceParts.Add(C);
		}
	}
	else
	{
		SetupSoldierPaintTint(Visual);
	}
	if (!Visual.SoldierRifle.IsEmpty())
	{
		bShoulderLauncher = Visual.SoldierRifle.Contains(TEXT("/Launchers/"));
		if (UStaticMesh* Rifle = LoadMesh(Visual.SoldierRifle))
		{
			SoldierRifle = NewObject<UStaticMeshComponent>(this);
			SoldierRifle->SetupAttachment(TurretPivot);
			SoldierRifle->SetStaticMesh(Rifle);
			// (01/11, a avaliação da infantaria contra o nível novo) o fuzil do modelo da Unreal é cinza-claro de ficção científica e no
			// sol lia como brinquedo prateado: o aço oxidado escuro de arma (o mesmo do tanque, fosco e metálico)
			if (UMaterialInterface* Gun = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Structures/Materials/MI_TankMetal.MI_TankMetal"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				for (int32 K = 0; K < SoldierRifle->GetNumMaterials(); ++K)
				{
					SoldierRifle->SetMaterial(K, Gun);
				}
			}
			SoldierRifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SoldierRifle->SetRelativeScale3D(FVector(Visual.SoldierScale));
			SoldierRifle->RegisterComponent();
		}
	}
}

void ARTSUnitActor::SetupSoldierPaintTint(const FRTSUnitVisual& Visual)
{
	// a farda nas pernas e na cabeça e a cor da facção no tronco (o colete; o modelo da Unreal tem dois materiais, o "Paint Tint" de cada:
	// Game/Tools/soldier_probe.py). O logotipo do modelo some (a tinta dele igual à farda)
	for (int32 Slot = 0; Slot < SoldierMesh->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* M = SoldierMesh->CreateDynamicMaterialInstance(Slot))
		{
			const FLinearColor C = Slot == 1 ? Visual.SoldierTeam : Visual.SoldierCloth;
			M->SetVectorParameterValue(TEXT("Paint Tint"), C);
			M->SetVectorParameterValue(TEXT("LogoTint"), C);
			M->SetScalarParameterValue(TEXT("MetalPaintMetallic"), 0.0f);
			M->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.85f);   // tecido e colete foscos (o modelo vem com tinta de metal)
		}
	}
	// o capacete (os game shots: o capacete na cor da facção é o que mais lê de cima; o modelo da Unreal tem a cabeça lisa): a calota de um
	// capacete de combate (ECH/IHPS ~26 × 28 cm, ~13 cm de altura) com a tinta do colete
	if (UStaticMesh* Dome = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		SoldierHelmet = NewObject<UStaticMeshComponent>(this);
		SoldierHelmet->SetupAttachment(TurretPivot);
		SoldierHelmet->SetStaticMesh(Dome);
		SoldierHelmet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SoldierHelmet->SetRelativeScale3D(FVector(0.29f, 0.27f, 0.20f) * Visual.SoldierScale);
		if (SoldierMesh->GetNumMaterials() > 1)
		{
			SoldierHelmet->SetMaterial(0, SoldierMesh->GetMaterial(1));
		}
		SoldierHelmet->RegisterComponent();
	}
}

FVector ARTSUnitActor::GetTurretRingCm() const
{
	// o pé da torre (a caixa medida dela no quadro da malha) no pivô; pelo Body, com a escala da malha e a inclinação do relevo
	const float Z = TurretBodyBoxCm.IsValid ? float(TurretBodyBoxCm.Min.Z) : 0.0f;
	const FVector Local(TurretPivotCm.X, TurretPivotCm.Y, Z);
	return Body ? Body->GetComponentTransform().TransformPosition(Local) : GetActorTransform().TransformPosition(Local);
}

void ARTSUnitActor::PlayFire(bool bGunRecoil)
{
	if (SoldierMesh && GetWorld())
	{
		SoldierShotAt = GetWorld()->GetTimeSeconds();   // (30/10) a animação de tiro do soldado
	}
	// (23/10, v4) o lançador que levanta: o tubo deste tiro já veio da simulação (NoteLauncherShot, antes daqui, em todo tiro)
	// (v4, 23/10) o míssil sai da caixa sem recuo: o Tomahawk (veículo com torre) deslizava a mesa e as caixas 30 cm pra trás
	// do caminhão e balançava o casco como o canhão do tanque
	if (bGunRecoil && bLauncherGuns && LauncherTubesCm.Num() > 1)
	{
		LauncherTube = (LauncherTube + 1) % LauncherTubesCm.Num();   // (25/10, v5) os canos alternam: o clarão e o traçante saem do da vez
	}
	if (bGunRecoil && bLightGun)
	{
		return;   // (25/10, v5) a arma leve: sem o recuo de 30 cm da torre e sem o tranco do canhão de tanque; o clarão é da partida
	}
	if (bGunRecoil)
	{
		// (30/10, o Felipe: "recua só um pouco o tanque, óbvio, tipo a física normal, a torreta também, mas o cano entra bem mais pra
		// dentro") o cano entra na torre: o curso do 120 mm é ~30 cm, aqui 55 cm (×1,8 pra ler a 75 m, onde 30 cm são ~6 px); a torre só
		// estremece (6 cm). Sem cano separado, a torre inteira recua como antes
		if (BarrelParts.Num())
		{
			BarrelRecoilCm = 55.0f;
			RecoilCm = 6.0f;
		}
		else
		{
			RecoilCm = 30.0f;
		}
	}
	if (bGunRecoil && !bStructure)
	{
		// o tranco do tiro: o canhão empurra o casco pra trás, pro lado de onde a torre aponta (o prédio, a Patriot, não
		// balança). (30/10, "no vídeo o tanque nem sequer sofre a inércia") o dobro: com 1,2 o casco balançava ~1,5° e de cima isso é
		// ~1 px; o impulso real (~20 kN·s num tanque de 60 t, ~0,3 m/s) põe o casco ~2–3° na mola
		const float Rel = FMath::DegreesToRadians(TurretPivot->GetRelativeRotation().Yaw);
		SuspPitchRate += 2.0f * FireKickDeg * FMath::Cos(Rel);
		// (30/10, o Felipe: "a inércia no tanque [o M1] é imperceptível, não tomba um pouco pro lado"; o da GLA, atirando pra frente, "dá
		// uma inércia boa") a mola da rolagem amortece 0,3 por passo e a do arfar 0,8: o mesmo impulso de lado somava ~1,4× ele no ângulo e
		// de frente ~5×. O tiro de lado leva 3,5× pra o casco tombar o mesmo tanto, macio, e voltar
		SuspRollRate -= 3.5f * 2.0f * FireKickDeg * FMath::Sin(Rel);
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
	// pelo anel da torre e a fumaça grossa abaixo de 35%; a BlastFx só age nas passagens (e respeita o -ShotFx=0). (v12) O soldado não:
	// o ferido acendia o fogo do motor e o jato pelo anel da torre no chão do lado dele (o ferido é o tranco e o sangue)
	if (!bDead && !bStructure && !bInfantry)
	{
		if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
		{
			Fx->VehicleDamage(this, 1.0f - Damage);
		}
		UpdateVehicleState(1.0f - Damage);   // (01/11) a malha danificada (o casco e a torre)
	}
	// (30/10, os estados de dano do prédio) o fogo do telhado abaixo de 35%: só no prédio pronto (o telhado da obra ainda não existe) e
	// à vista (o fogo não pode contar ao inimigo o que a névoa esconde; o que já acendeu fica)
	if (!bDead && bStructure && BuildFraction >= 1.0f && !bFogHidden)
	{
		if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
		{
			TArray<FVector> Roof;
			GetRoofFirePoints(Roof);
			Fx->StructureDamage(this, 1.0f - Damage, Roof);
		}
		UpdateStructureState(1.0f - Damage);
		UpdateStructureHoles(1.0f - Damage);
	}
	if (bDead || !bStructure || FMath::Abs(Damage - DamageShown) < 0.02f)
	{
		return;
	}
	DamageShown = Damage;
	SetPartsFloat(DataDamage, Damage);
}

namespace RTSGroundCut
{
	// (01/11) os prédios com o recorte ligado (8 vagas: Cut0..7 = centro e meia-largura, Rot0..7 = cos e sen). Moram na coleção que o
	// chão já usa, a MPC_Ground: o material só aceita 2 coleções e o chão já tem a MPC_Ground e a MPC_Wind (a 3ª, a MPC_GroundCut,
	// derrubou a compilação do chão: o xadrez de 01/11 de manhã)
	TArray<TWeakObjectPtr<ARTSUnitActor>> Owners;
	constexpr int32 MaxCuts = 8;

	void Push(UWorld* World)
	{
		UMaterialParameterCollection* MPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_Ground.MPC_Ground"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!World || !MPC)
		{
			return;
		}
		TArray<FVector4> CH;
		TArray<FVector2D> CS;
		ARTSUnitActor::GetGroundCuts(CH, CS);
		for (int32 K = 0; K < MaxCuts; ++K)
		{
			const bool bOn = K < CH.Num();
			UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, FName(*FString::Printf(TEXT("Cut%d"), K)),
				bOn ? FLinearColor(float(CH[K].X), float(CH[K].Y), float(CH[K].Z), float(CH[K].W)) : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			UKismetMaterialLibrary::SetVectorParameterValue(World, MPC, FName(*FString::Printf(TEXT("Rot%d"), K)),
				bOn ? FLinearColor(float(CS[K].X), float(CS[K].Y), 0.0f, 0.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 0.0f));
		}
		UKismetMaterialLibrary::SetScalarParameterValue(World, MPC, TEXT("CutCount"), float(FMath::Min(CH.Num(), MaxCuts)));
		if (CH.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS recorte do chão: %d retângulo(s); o 1º em (%.0f, %.0f) cm, meia-largura %.0f × %.0f cm"),
				CH.Num(), CH[0].X, CH[0].Y, CH[0].Z, CH[0].W);
		}
	}
}

void ARTSUnitActor::GetGroundCuts(TArray<FVector4>& OutCenterHalf, TArray<FVector2D>& OutCosSin)
{
	RTSGroundCut::Owners.RemoveAll([](const TWeakObjectPtr<ARTSUnitActor>& A) { return !A.IsValid(); });
	for (const TWeakObjectPtr<ARTSUnitActor>& A : RTSGroundCut::Owners)
	{
		const FTransform& T = A->GroundCutAt;
		const float Yaw = FMath::DegreesToRadians(float(T.Rotator().Yaw));
		for (const FVector4& R : A->GroundCutsCm)
		{
			const FVector C = T.TransformPosition(FVector((R.X + R.Z) * 0.5, (R.Y + R.W) * 0.5, 0.0));
			OutCenterHalf.Add(FVector4(C.X, C.Y, FMath::Abs(R.Z - R.X) * 0.5, FMath::Abs(R.W - R.Y) * 0.5));
			OutCosSin.Add(FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)));
		}
	}
}

void ARTSUnitActor::UpdateGroundCut(bool bWant)
{
	// (01/11) o prédio pronto recorta o chão do mapa onde o modelo desce abaixo do terreno (a rampa do túnel do Arms Dealer): liga no
	// MPC_GroundCut quando a obra termina, desliga na venda e quando o ator some (a ruína fica com o buraco: a rampa continua nela)
	bWant = bWant && GroundCutsCm.Num() > 0 && Body;
	if (bWant == bGroundCutOn)
	{
		return;
	}
	bGroundCutOn = bWant;
	if (bWant)
	{
		GroundCutAt = Body->GetComponentTransform();
		RTSGroundCut::Owners.AddUnique(this);
	}
	else
	{
		RTSGroundCut::Owners.Remove(this);
	}
	RTSGroundCut::Push(GetWorld());
}

void ARTSUnitActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bGroundCutOn)
	{
		RTSGroundCut::Owners.Remove(this);
		bGroundCutOn = false;
		if (Reason != EEndPlayReason::EndPlayInEditor && Reason != EEndPlayReason::Quit)
		{
			RTSGroundCut::Push(GetWorld());
		}
	}
	Super::EndPlay(Reason);
}

bool ARTSUnitActor::TryStartTunnelExit(ARTSUnitActor& Unit, const FVector2D& SpawnCm) const
{
	// (01/11) a unidade que nasceu na saída desta fábrica (na frente dela, perto do eixo) sobe do túnel: os pontos do caminho no mundo
	if (TunnelPathCm.Num() < 2 || BuildFraction < 1.0f || bDead || !Body)
	{
		return false;
	}
	const FTransform T = Body->GetComponentTransform();
	const FVector L = T.InverseTransformPosition(FVector(SpawnCm.X, SpawnCm.Y, T.GetLocation().Z));
	const float FrontCm = float(TunnelPathCm.Last().X);
	if (FMath::Abs(L.Y) > 450.0f || L.X < FrontCm || L.X > FrontCm + 2500.0f)
	{
		return false;
	}
	TArray<FVector> Pts;
	for (const FVector2D& P : TunnelPathCm)
	{
		const FVector W = T.TransformPosition(FVector(P.X, 0.0, 0.0));
		Pts.Add(FVector(W.X, W.Y, P.Y));
	}
	Unit.StartTunnelExit(Pts, TunnelSpeedCm);
	return true;
}

void ARTSUnitActor::StartTunnelExit(const TArray<FVector>& Points, float SpeedCm)
{
	TunnelPts = Points;
	TunnelDoneCm = 0.0f;
	TunnelRunCm = FMath::Max(SpeedCm, 50.0f);
	TunnelPitchDeg = 0.0f;
	ApplyStencil();
}

bool ARTSUnitActor::ApplyTunnelExit(FVector& Location, float DeltaSeconds)
{
	// (01/11) a subida do túnel: anda TunnelRunCm/s pelos pontos (x, y no mundo; z = abaixo do chão) e depois liga na posição da
	// simulação (que já anda pra área de reunião); o casco inclina com a rampa. Só desenho: a simulação nasceu na saída
	if (TunnelPts.Num() < 2)
	{
		return false;
	}
	TunnelDoneCm += TunnelRunCm * DeltaSeconds;
	float Acc = 0.0f;
	FVector Prev = TunnelPts[0];
	for (int32 K = 1; K <= TunnelPts.Num(); ++K)
	{
		const FVector Next = K < TunnelPts.Num() ? TunnelPts[K] : FVector(Location.X, Location.Y, 0.0);
		const float Seg = float(FVector2D::Distance(FVector2D(Prev), FVector2D(Next)));
		if (TunnelDoneCm <= Acc + Seg)
		{
			const float U = Seg > 1.0f ? (TunnelDoneCm - Acc) / Seg : 1.0f;
			const FVector P = FMath::Lerp(Prev, Next, U);
			Location = FVector(P.X, P.Y, Location.Z + P.Z);
			const float Want = FMath::RadiansToDegrees(FMath::Atan2(float(Next.Z - Prev.Z), FMath::Max(Seg, 1.0f)));
			TunnelPitchDeg = FMath::FInterpTo(TunnelPitchDeg, Want, DeltaSeconds, 6.0f);
			return true;
		}
		Acc += Seg;
		Prev = Next;
	}
	TunnelPts.Reset();
	TunnelPitchDeg = 0.0f;
	ApplyStencil();   // fora do túnel: a silhueta volta a valer
	return false;
}

void ARTSUnitActor::StepTracks(float TravelCm, float DYawRad)
{
	// (01/11, a esteira que anda) a fase do elo de cada lagarta anda o que aquele lado andou no chão ÷ o passo. Acima de ~0,4 passo por
	// quadro o olho lê o elo parado ou andando pra trás (o efeito da roda de carroça): a esteira desenhada fica limitada a isso (a 30 qps,
	// ~2 m/s; as rodas seguem na velocidade de verdade). O dado 10 = round(passo do quadro × 1024) + a fase: o material tira dele o quadro
	// anterior (a velocidade pro TSR, sem o salto da volta da fase)
	for (int32 K = 0; K < TrackParts.Num(); ++K)
	{
		if (!TrackParts[K])
		{
			continue;
		}
		const float D = FMath::Clamp((TravelCm - DYawRad * TrackSideYCm[K]) / TrackStepCm, -0.4f, 0.4f);
		const float I = FMath::RoundToFloat(D * 1024.0f);
		TrackPhase[K] = FMath::Frac(TrackPhase[K] + I / 1024.0f);
		const float Value = I + TrackPhase[K];
		if (Value != TrackSent[K])
		{
			TrackParts[K]->SetCustomPrimitiveDataFloat(10, Value);
			TrackSent[K] = Value;
		}
	}
}

void ARTSUnitActor::UpdateVehicleState(float Health01)
{
	// (01/11, o item 12: "o veículo ferido parece intacto, só pegando fogo"; docs/ESTUDO_DANO_VEICULOS.md) o casco e a torre trocam abaixo de
	// 70% e de 35% e voltam consertados acima de 85% (o mesmo do prédio, os limiares do ZH). Coberta pelo que o BlastFx já solta nessas
	// passagens (aos 70% a fumaça clara, aos 35% o jato de chama pelo anel da torre). As tintas do jogo (as instâncias com a cor do time)
	// passam pela troca pelo NOME do slot: a malha nova do destrutor tem os mesmos slots, talvez noutra ordem
	int32 Want = StructState;
	if (Health01 < 0.35f)
	{
		Want = 2;
	}
	else if (Health01 < 0.70f)
	{
		Want = 1;
	}
	else if (Health01 >= 0.85f)
	{
		Want = 0;
	}
	if (Want == StructState || StructMeshPaths[1].IsEmpty())
	{
		return;
	}
	const auto Swap = [this](UStaticMeshComponent* Part, const FString& Path)
	{
		UStaticMesh* Mesh = Part && !Path.IsEmpty() ? LoadMesh(Path) : nullptr;
		if (!Mesh || Part->GetStaticMesh() == Mesh)
		{
			return;
		}
		TMap<FName, UMaterialInterface*> BySlot;
		for (int32 K = 0; K < Part->GetNumMaterials(); ++K)
		{
			BySlot.Add(Part->GetMaterialSlotNames().IsValidIndex(K) ? Part->GetMaterialSlotNames()[K] : NAME_None, Part->GetMaterial(K));
		}
		Part->SetStaticMesh(Mesh);
		Part->EmptyOverrideMaterials();
		const TArray<FName> Names = Part->GetMaterialSlotNames();
		for (int32 K = 0; K < Names.Num(); ++K)
		{
			if (UMaterialInterface* const* M = BySlot.Find(Names[K]))
			{
				Part->SetMaterial(K, *M);
			}
		}
	};
	const int32 S = StructMeshPaths[Want].IsEmpty() ? (Want > 0 ? 1 : 0) : Want;
	if (HullParts.Num() && HullParts[0])
	{
		Swap(HullParts[0], StructMeshPaths[S]);
	}
	if (TurretParts.Num() && TurretParts[0] && !TurretMeshPaths[S].IsEmpty())
	{
		Swap(TurretParts[0], TurretMeshPaths[S]);
	}
	StructState = Want;
}

void ARTSUnitActor::UpdateStructureState(float Health01)
{
	// (31/10, o pedido do Felipe: "as construções têm que degradar de forma linda, e não um jogo de lego"; os estados do ZH,
	// docs/ESTUDO_DEGRADACAO_PREDIOS.md) a malha do casco troca abaixo de 70% (o danificado: as mordidas na borda) e de 35% (o muito
	// danificado: a quina caída, a laje pendurada, o interior), e volta consertado acima de 85%. A troca vem coberta, como no ZH: o estouro
	// e a poeira (o StructureBreak) em cada lugar novo que quebrou. Os furos de antes continuam por cima
	int32 Want = StructState;
	if (Health01 < 0.35f)
	{
		Want = 2;
	}
	else if (Health01 < 0.70f)
	{
		Want = 1;
	}
	else if (Health01 >= 0.85f)
	{
		Want = 0;
	}
	if (Want == StructState || !HullParts.Num() || !HullParts[0] || StructMeshPaths[Want].IsEmpty())
	{
		return;
	}
	UStaticMesh* Mesh = LoadMesh(StructMeshPaths[Want]);
	if (!Mesh)
	{
		return;
	}
	UStaticMeshComponent* Part = HullParts[0];
	Part->SetStaticMesh(Mesh);
	Part->EmptyOverrideMaterials();
	if (HoleStage > 0)
	{
		for (int32 K = 0; K < Part->GetNumMaterials(); ++K)
		{
			Part->SetMaterial(K, BuildTwin(Part->GetMaterial(K), true, TEXT("_Holed")));
		}
	}
	const bool bWorse = Want > StructState;
	StructState = Want;
	RoofFirePointsCm = StructFireCm[Want];
	if (bWorse && !bFogHidden)
	{
		if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
		{
			const FTransform& T = Part->GetComponentTransform();
			for (int32 K = 0; K < FMath::Min(3, StructFireCm[Want].Num()); ++K)
			{
				const FVector At = T.TransformPosition(StructFireCm[Want][K]);
				Fx->StructureBreak(At, FVector::UpVector, 260.0f, HashCombine(SimId, 0x57A7u + uint32(Want) * 131u + uint32(K)),
					float(GetActorLocation().Z), WallTint());
			}
		}
	}
}

void ARTSUnitActor::UpdateStructureHoles(float Health01)
{
	// (30/10, item 31, o Felipe: "quando tá com pouca vida, além de só pegar fogo, quero danificações, desabamentos também enquanto tá
	// sendo destruído") o prédio cede por partes: a cada passagem (70, 50, 35 e 20% da vida; os estados do ZH são 70 e 35) abre um furo no
	// telhado ou na parede (o material corta o que caiu e mostra o escuro de dentro) e o pedaço cai dele com o estouro e o pó. Consertado
	// acima de 85%, fecha (o ZH troca o modelo de volta)
	static constexpr float Steps[] = { 0.70f, 0.50f, 0.35f, 0.20f };
	if (Health01 >= 0.85f)
	{
		if (HoleStage > 0)
		{
			ClearStructureHoles();
		}
		return;
	}
	int32 Want = 0;
	for (const float S : Steps)
	{
		Want += Health01 <= S ? 1 : 0;
	}
	while (HoleStage < Want)
	{
		FVector At, N;
		// o furo de ~1,3 m no 1º ao ~2,8 m no 4º, na escala da altura do prédio (a War Factory tem ~9 m)
		const float Radius = (130.0f + 50.0f * float(HoleStage)) * FMath::Clamp(GetHeightCm() / 900.0f, 0.6f, 1.4f);
		const uint32 Seed = HashCombine(SimId, 0x40E5u + uint32(HoleStage) * 977u);
		if (OpenStructureHole(Seed, Radius, At, N))
		{
			if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
			{
				Fx->StructureBreak(At, N, Radius, Seed, float(GetActorLocation().Z), WallTint());
			}
		}
		++HoleStage;
	}
}

float ARTSUnitActor::CollapseSunk01(float T01)
{
	// (01/11: as mesmas, em frações do tempo do desabamento: CollapseSeconds) as etapas em segundos (no total de 3,5 s): o 1º andar cede de 0 a 0,55 s (35%), assenta até 1,0 s (40%); o 2º de 1,0 a 1,6 s (75%),
	// assenta até 2,1 s (78%); o resto de 2,1 a 3,0 s (100%). Cada queda acelera (u²: a gravidade), cada pausa anda devagar
	const float S = FMath::Clamp(T01, 0.0f, 1.0f) * 3.5f;   // (o desenho das etapas em 3,5 s; o tempo de verdade é CollapseSeconds)
	struct FSeg { float S0, S1, F0, F1; bool bFall; };
	static constexpr FSeg Segs[] = { { 0.0f, 0.55f, 0.0f, 0.35f, true }, { 0.55f, 1.0f, 0.35f, 0.40f, false }, { 1.0f, 1.6f, 0.40f, 0.75f, true },
		{ 1.6f, 2.1f, 0.75f, 0.78f, false }, { 2.1f, 3.0f, 0.78f, 1.0f, true } };
	for (const FSeg& G : Segs)
	{
		if (S < G.S1)
		{
			const float U = FMath::Clamp((S - G.S0) / (G.S1 - G.S0), 0.0f, 1.0f);
			return FMath::Lerp(G.F0, G.F1, G.bFall ? U * U : U);
		}
	}
	return 1.0f;
}

FLinearColor ARTSUnitActor::WallTint() const
{
	// (01/11, a prova de perto: as lascas cinza-claras liam como papel branco na areia) a cor da parede: o 1º material de concreto, reboco
	// ou corpo do casco (a cor base da instância, o mesmo que a fachada mostra); sem ele, o concreto sujo
	for (const UStaticMeshComponent* Part : HullParts)
	{
		for (int32 K = 0; Part && K < Part->GetNumMaterials(); ++K)
		{
			const UMaterialInterface* M = Part->GetMaterial(K);
			const FString Name = M ? M->GetName() : FString();
			FLinearColor C;
			if ((Name.Contains(TEXT("Concrete")) || Name.Contains(TEXT("Plaster")) || Name.Contains(TEXT("Body")))
				&& M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColor")), C))
			{
				return FLinearColor(C.R, C.G, C.B, 1.0f);
			}
		}
	}
	return FLinearColor(0.20f, 0.19f, 0.17f, 1.0f);
}

bool ARTSUnitActor::OpenStructureHole(uint32 Seed, float HoleRadiusCm, FVector& OutCm, FVector& OutNormal)
{
	// o lugar: 2 em 3 no telhado (o que a câmera do RTS vê), o resto na parede de 45–85% da altura; pelo raio no modelo (RayStructure),
	// a superfície de verdade
	FRandomStream R(int32(Seed & 0x7fffffffu));
	FVector C, E;
	GetActorBounds(false, C, E);
	FVector Local = FVector::ZeroVector;
	UStaticMeshComponent* Part = nullptr;
	bool bHit = false;
	// (01/11, a avaliação do item 10: "onde o impacto cai fica machucado a construção") o 1º e o 3º furo abrem na parede onde os tiros mais
	// recentes bateram (a marca de tiro de concreto mais nova, o tipo 2 do MarkHull), não num lugar sorteado; o 2º e o 4º seguem no telhado
	// (o que a câmera do RTS vê) ou na parede sorteada
	if (HoleStage % 2 == 0)
	{
		constexpr int32 MarkBase = 12;
		float BestBorn = -1.0f;
		UStaticMeshComponent* MarkPart = nullptr;
		FVector MarkLocal = FVector::ZeroVector;
		for (UStaticMeshComponent* P : HullParts)
		{
			if (!P)
			{
				continue;
			}
			const TArray<float>& D = P->GetCustomPrimitiveData().Data;
			for (int32 K = 0; K < 6; ++K)
			{
				const int32 B = MarkBase + 4 * K;
				const float W = D.IsValidIndex(B + 3) ? D[B + 3] : 0.0f;
				const float Kind = FMath::FloorToFloat(W / 1.0e5f);
				const float Born = W - 1.0e5f * Kind;
				if (Kind == 2.0f && Born > BestBorn)
				{
					BestBorn = Born;
					MarkPart = P;
					MarkLocal = FVector(D[B], D[B + 1], D[B + 2]);
				}
			}
		}
		if (MarkPart)
		{
			const FVector MarkW = MarkPart->GetComponentTransform().TransformPosition(MarkLocal);
			const FVector Out = FVector(MarkW.X - C.X, MarkW.Y - C.Y, 0.0).GetSafeNormal();
			bHit = !Out.IsNearlyZero() && RayStructure(MarkW + Out * (E.Size2D() + 500.0), MarkW - Out * 300.0, OutCm, OutNormal, Part, Local) && Part;
		}
	}
	for (int32 Try = 0; Try < 10 && !bHit; ++Try)
	{
		FVector From, To;
		if (R.FRand() < 0.66f)
		{
			From = FVector(C.X + R.FRandRange(-0.7f, 0.7f) * E.X, C.Y + R.FRandRange(-0.7f, 0.7f) * E.Y, C.Z + E.Z + 500.0);
			To = From - FVector(0.0, 0.0, 2.0 * E.Z + 1000.0);
		}
		else
		{
			const float A = R.FRandRange(0.0f, 2.0f * PI);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0);
			const double Z = C.Z - E.Z + 2.0 * E.Z * R.FRandRange(0.45f, 0.85f);
			To = FVector(C.X, C.Y, Z);
			From = To + Dir * (E.Size2D() + 500.0);
		}
		bHit = RayStructure(From, To, OutCm, OutNormal, Part, Local) && Part;
	}
	if (!bHit)
	{
		return false;
	}
	// a vaga: as marcas de tiro do material (dados 12–35, 6 de 4: x, y, z na malha em cm e o w); o furo é o tipo 3, w = 300 000 + o raio
	// (cm), nas vagas de trás pra frente (as marcas de tiro ficam nas da frente: UBlastFxDirector::MarkHull pula as do tipo 3)
	constexpr int32 HitMarkBase = 12;
	const int32 Slot = 5 - FMath::Min(HoleStage, 3);
	Part->SetCustomPrimitiveDataFloat(HitMarkBase + 4 * Slot + 0, float(Local.X));
	Part->SetCustomPrimitiveDataFloat(HitMarkBase + 4 * Slot + 1, float(Local.Y));
	Part->SetCustomPrimitiveDataFloat(HitMarkBase + 4 * Slot + 2, float(Local.Z));
	Part->SetCustomPrimitiveDataFloat(HitMarkBase + 4 * Slot + 3, 300000.0f + HoleRadiusCm);
	// o material que corta (o do prédio furado: o mesmo visual, mascarado e de dois lados, o lado de dentro escuro), só nesta peça. (v9: com
	// o da obra o prédio inteiro escurecia, e o corte da obra, o dado 5, ficava 0 quando os dados cresciam até as vagas: o telhado sumia)
	Part->SetCustomPrimitiveDataFloat(DataCut, 1000.0f);
	for (int32 K = 0; K < Part->GetNumMaterials(); ++K)
	{
		Part->SetMaterial(K, BuildTwin(Part->GetMaterial(K), true, TEXT("_Holed")));
	}
	return true;
}

void ARTSUnitActor::ClearStructureHoles()
{
	constexpr int32 HitMarkBase = 12;
	for (UStaticMeshComponent* Part : HullParts)
	{
		if (!Part)
		{
			continue;
		}
		for (int32 Slot = 2; Slot < 6; ++Slot)
		{
			const TArray<float>& D = Part->GetCustomPrimitiveData().Data;
			const int32 W = HitMarkBase + 4 * Slot + 3;
			if (D.IsValidIndex(W) && D[W] >= 300000.0f)
			{
				Part->SetCustomPrimitiveDataFloat(W, 0.0f);
			}
		}
		for (int32 K = 0; K < Part->GetNumMaterials(); ++K)
		{
			Part->SetMaterial(K, BuildTwin(Part->GetMaterial(K), false, TEXT("_Holed")));
		}
	}
	HoleStage = 0;
}

void ARTSUnitActor::GetRoofFirePoints(TArray<FVector>& OutCm) const
{
	OutCm.Reset();
	const UStaticMeshComponent* Hull = HullParts.Num() ? HullParts[0].Get() : nullptr;
	if (!Hull)
	{
		return;
	}
	const FTransform& T = Hull->GetComponentTransform();
	for (const FVector& P : RoofFirePointsCm)
	{
		OutCm.Add(T.TransformPosition(P));
	}
}

bool ARTSUnitActor::RayStructure(const FVector& FromCm, const FVector& ToCm, FVector& OutCm, FVector& OutNormal, UStaticMeshComponent*& OutPart, FVector& OutLocalCm) const
{
	UStaticMeshComponent* Hull = HullParts.Num() ? HullParts[0].Get() : nullptr;
	if (Tris.IsValid() && Hull)
	{
		// (30/10, o Felipe: "o impacto na construção não tá no hitbox dos models dela") o raio contra os triângulos do modelo (Möller–Trumbore),
		// só os das células de 1 m que ele atravessa em XY: o acerto mais perto, com a normal da face virada pro tiro
		const FTriGrid& G = *Tris;
		const FTransform& T = Hull->GetComponentTransform();
		const FVector3f A(T.InverseTransformPosition(FromCm));
		const FVector3f B(T.InverseTransformPosition(ToCm));
		const FVector3f D = B - A;
		TSet<int32> Seen;
		float BestT = 2.0f;
		int32 Best = INDEX_NONE;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FVector2f(D.X, D.Y).Size() / (G.CellCm * 0.5f)), 1, 2000);
		for (int32 K = 0; K <= Steps; ++K)
		{
			const FVector3f P = A + D * (float(K) / Steps);
			const int32 I = FMath::FloorToInt32((P.X - G.MinCm.X) / G.CellCm), J = FMath::FloorToInt32((P.Y - G.MinCm.Y) / G.CellCm);
			for (int32 Di = -1; Di <= 1; ++Di)
			{
				for (int32 Dj = -1; Dj <= 1; ++Dj)
				{
					const int32 Ci = I + Di, Cj = J + Dj;
					if (Ci < 0 || Cj < 0 || Ci >= G.Nx || Cj >= G.Ny)
					{
						continue;
					}
					for (const int32 Tri : G.Cells[Ci * G.Ny + Cj])
					{
						bool bAlready = false;
						Seen.Add(Tri, &bAlready);
						if (bAlready)
						{
							continue;
						}
						const FVector3f& V0 = G.V[Tri * 3];
						const FVector3f E1 = G.V[Tri * 3 + 1] - V0, E2 = G.V[Tri * 3 + 2] - V0;
						const FVector3f Pv = FVector3f::CrossProduct(D, E2);
						const float Det = FVector3f::DotProduct(E1, Pv);
						if (FMath::Abs(Det) < 1e-6f)
						{
							continue;
						}
						const float Inv = 1.0f / Det;
						const FVector3f Tv = A - V0;
						const float U = FVector3f::DotProduct(Tv, Pv) * Inv;
						if (U < 0.0f || U > 1.0f)
						{
							continue;
						}
						const FVector3f Q = FVector3f::CrossProduct(Tv, E1);
						const float W = FVector3f::DotProduct(D, Q) * Inv;
						if (W < 0.0f || U + W > 1.0f)
						{
							continue;
						}
						const float Tt = FVector3f::DotProduct(E2, Q) * Inv;
						if (Tt >= 0.0f && Tt < BestT)
						{
							BestT = Tt;
							Best = Tri;
						}
					}
				}
			}
			if (Best != INDEX_NONE && float(K) / Steps > BestT + 0.02f)
			{
				break;   // já passou do acerto mais perto: as células à frente não chegam antes
			}
		}
		if (Best != INDEX_NONE && BestT <= 1.0f)
		{
			const FVector3f& V0 = G.V[Best * 3];
			FVector3f N = FVector3f::CrossProduct(G.V[Best * 3 + 1] - V0, G.V[Best * 3 + 2] - V0).GetSafeNormal();
			if (FVector3f::DotProduct(N, D) > 0.0f)
			{
				N = -N;
			}
			const FVector Hit = FVector(A + D * BestT);
			OutLocalCm = Hit;
			OutCm = T.TransformPosition(Hit);
			OutNormal = T.TransformVectorNoScale(FVector(N)).GetSafeNormal();
			OutPart = Hull;
			return true;
		}
		return false;
	}
	if (!Heights.IsValid() || !Hull)
	{
		return false;
	}
	const FHeightGrid& G = *Heights;
	const FTransform& T = Hull->GetComponentTransform();
	const FVector A = T.InverseTransformPosition(FromCm);
	const FVector B = T.InverseTransformPosition(ToCm);
	auto Cell = [&G](const FVector& P, int32& I, int32& J)
	{
		I = FMath::FloorToInt32(float((P.X - G.MinCm.X) / G.CellCm));
		J = FMath::FloorToInt32(float((P.Y - G.MinCm.Y) / G.CellCm));
		return I >= 0 && J >= 0 && I < G.Nx && J < G.Ny ? G.H[I * G.Ny + J] : -1.0f;
	};
	// em passos de 1/3 da célula; dentro = abaixo da altura da célula. O 1º passo dentro: a busca binária acha a borda
	const double Len = FVector::Dist(A, B);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt32(float(Len / (G.CellCm / 3.0f))), 2, 4000);
	FVector Prev = A;
	for (int32 K = 1; K <= Steps; ++K)
	{
		const FVector P = A + (B - A) * (double(K) / Steps);
		int32 I, J;
		const float H = Cell(P, I, J);
		if (H < 0.0f || P.Z > H)
		{
			Prev = P;
			continue;
		}
		FVector Lo = Prev, Hi = P;
		for (int32 It = 0; It < 8; ++It)
		{
			const FVector M = 0.5 * (Lo + Hi);
			int32 Mi, Mj;
			const float Hm = Cell(M, Mi, Mj);
			(Hm >= 0.0f && M.Z <= Hm ? Hi : Lo) = M;
		}
		// a normal: entrou por cima (a mesma célula do passo de fora) = o teto; por um lado = a parede daquele lado
		int32 Li, Lj, Hi2, Hj;
		Cell(Lo, Li, Lj);
		Cell(Hi, Hi2, Hj);
		FVector NL = FVector::UpVector;
		if (Li != Hi2 || Lj != Hj)
		{
			NL = FMath::Abs(Li - Hi2) >= FMath::Abs(Lj - Hj) ? FVector(FMath::Sign(float(Li - Hi2)), 0.0, 0.0) : FVector(0.0, FMath::Sign(float(Lj - Hj)), 0.0);
		}
		OutLocalCm = Hi;
		OutCm = T.TransformPosition(Hi);
		OutNormal = T.TransformVectorNoScale(NL).GetSafeNormal();
		OutPart = Hull;
		return true;
	}
	return false;
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
	// (01/11) subindo do túnel a unidade está debaixo do chão de propósito: sem a silhueta (ela pintava uma mancha da cor do dono no pátio
	// do Arms Dealer, o vídeo elenco_gla_v3)
	const bool bInTunnel = TunnelPts.Num() >= 2;
	const int32 Stencil = (bUnitMask ? 128 : 0) | (bXRayOn && !bInTunnel ? SilhouetteStencil : 0);
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

void ARTSUnitActor::OpenHatch(int32 Slot, const FVector& LocalCm, const FVector2D& SizeCm)
{
	if (Slot < 0 || Slot > 7 || !Body || !GetWorld())
	{
		return;
	}
	if (HatchLids.Num() <= Slot)
	{
		HatchLids.SetNum(Slot + 1);
	}
	FHatchLid& H = HatchLids[Slot];
	H.HoldUntil = GetWorld()->GetTimeSeconds() + 2.0;
	if (H.Lid)
	{
		return;
	}
	// a tampa na tinta do casco (o 1º material "TankPaint" das peças; senão o 1º) e o vão escuro embaixo (a mesma tinta quase preta)
	static TWeakObjectPtr<UStaticMesh> Cube;
	if (!Cube.IsValid())
	{
		Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	UMaterialInterface* Paint = nullptr;
	for (const UStaticMeshComponent* Part : HullParts)
	{
		for (int32 K = 0; Part && K < Part->GetNumMaterials() && !Paint; ++K)
		{
			UMaterialInterface* Mi = Part->GetMaterial(K);
			Paint = Mi && Mi->GetName().Contains(TEXT("TankPaint")) ? Mi : Paint;
		}
	}
	if (!Paint && HullParts.Num() && HullParts[0])
	{
		Paint = HullParts[0]->GetMaterial(0);
	}
	if (!Cube.IsValid())
	{
		return;
	}
	H.LocalCm = LocalCm;
	H.SizeCm = SizeCm;
	H.Lid = NewObject<UStaticMeshComponent>(this);
	H.Lid->SetupAttachment(Body);
	H.Lid->SetStaticMesh(Cube.Get());
	H.Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H.Lid->SetVisibility(false);
	if (Paint)
	{
		H.Lid->SetMaterial(0, Paint);
	}
	H.Lid->RegisterComponent();
	H.Hole = NewObject<UStaticMeshComponent>(this);
	H.Hole->SetupAttachment(Body);
	H.Hole->SetStaticMesh(Cube.Get());
	H.Hole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H.Hole->SetCastShadow(false);
	H.Hole->SetVisibility(false);
	// o vão escuro: o material básico da Unreal (o parâmetro "Color"), quase preto (a tinta do casco não tem o parâmetro de cor)
	static TWeakObjectPtr<UMaterialInterface> Basic;
	if (!Basic.IsValid())
	{
		Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (UMaterialInstanceDynamic* Dark = Basic.IsValid() ? UMaterialInstanceDynamic::Create(Basic.Get(), this) : nullptr)
	{
		Dark->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.008f, 0.008f, 0.008f, 1.0f));
		H.Hole->SetMaterial(0, Dark);
	}
	// o vão: 85% da tampa, rente ao teto (1 cm acima, pra não brigar com a chapa)
	H.Hole->SetRelativeLocation(LocalCm + FVector(0.0f, 0.0f, 1.0f));
	H.Hole->SetRelativeScale3D(FVector(SizeCm.X * 0.85f / 100.0f, SizeCm.Y * 0.85f / 100.0f, 0.01f));
	H.Hole->RegisterComponent();
}

void ARTSUnitActor::StepHatches(float DeltaSeconds)
{
	if (HatchLids.Num() == 0 || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (FHatchLid& H : HatchLids)
	{
		if (!H.Lid)
		{
			continue;
		}
		// abre em ~0,15 s (a mola da tampa), fecha em ~0,4 s, 2 s depois do último tiro
		const bool bWant = Now < H.HoldUntil && !bDead;
		H.Open01 = FMath::FInterpConstantTo(H.Open01, bWant ? 1.0f : 0.0f, DeltaSeconds, bWant ? 6.5f : 2.5f);
		const bool bShow = H.Open01 > 0.01f;
		H.Lid->SetVisibility(bShow);
		H.Hole->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		// a dobradiça no lado do meio do casco (y → 0): a tampa gira em volta do eixo X até 105°, abrindo pra fora
		const float Side = H.LocalCm.Y >= 0.0f ? 1.0f : -1.0f;
		const float Ang = 105.0f * FMath::InterpEaseOut(0.0f, 1.0f, H.Open01, 2.0f);
		const FVector Hinge = H.LocalCm + FVector(0.0f, -Side * H.SizeCm.Y * 0.5f, 2.5f);
		const FQuat Q(FVector::ForwardVector, FMath::DegreesToRadians(Side * Ang));   // (+y gira pra cima: a tampa abre pra fora e pra cima)
		const FVector Center = Hinge + Q.RotateVector(FVector(0.0f, Side * H.SizeCm.Y * 0.5f, 0.0f));
		H.Lid->SetRelativeLocationAndRotation(Center, Q);
		H.Lid->SetRelativeScale3D(FVector(H.SizeCm.X / 100.0f, H.SizeCm.Y / 100.0f, 0.045f));
	}
}

void ARTSUnitActor::PulseLight(const FVector& AtCm, float LightRadiusCm, float PeakCd, float TauS, const FLinearColor& Color)
{
	if (!GetWorld() || bFogHidden)
	{
		return;
	}
	if (!PulseLightComp)
	{
		PulseLightComp = NewObject<UPointLightComponent>(this);
		PulseLightComp->SetupAttachment(GetRootComponent());
		PulseLightComp->SetIntensityUnits(ELightUnits::Candelas);
		PulseLightComp->SetCastShadows(false);
		PulseLightComp->SetSourceRadius(15.0f);
		PulseLightComp->SetIntensity(0.0f);
		PulseLightComp->RegisterComponent();
	}
	PulseLightComp->SetWorldLocation(AtCm);
	PulseLightComp->SetAttenuationRadius(LightRadiusCm);
	PulseLightComp->SetLightColor(Color);
	PulsePeakCd = PeakCd;
	PulseTauS = FMath::Max(TauS, 0.02f);
	PulseAt = GetWorld()->GetTimeSeconds();
	PulseLightComp->SetIntensity(PeakCd);
	PulseLightComp->SetVisibility(true);
}

void ARTSUnitActor::StepPulse()
{
	if (!PulseLightComp || !GetWorld() || PulsePeakCd <= 0.0f)
	{
		return;
	}
	// cai como o do ZH: linear até zero no tempo dele (o "IncreaseTime 0, DecreaseTime" do LightPulse)
	const float K = 1.0f - float(GetWorld()->GetTimeSeconds() - PulseAt) / PulseTauS;
	if (K <= 0.0f)
	{
		PulseLightComp->SetIntensity(0.0f);
		PulseLightComp->SetVisibility(false);
		PulsePeakCd = 0.0f;
		return;
	}
	PulseLightComp->SetIntensity(PulsePeakCd * K);
}

void ARTSUnitActor::NotePassengerShot(uint32 MissileId, const FVector& SimCm, const FVector& MouthCm, const FVector& Dir)
{
	FLaunchBlend& Blend = LaunchBlends[LaunchBlendNext];
	LaunchBlendNext = (LaunchBlendNext + 1) % UE_ARRAY_COUNT(LaunchBlends);
	Blend = FLaunchBlend();
	Blend.Id = MissileId;
	Blend.OffsetCm = MouthCm - SimCm;
	Blend.Dir = Dir;
	Blend.MouthCm = MouthCm;
	Blend.bSoft = true;
}

void ARTSUnitActor::SetPassenger(bool bInside, float Up01)
{
	bInsideTransport = bInside;
	PassengerUp01 = bInside ? FMath::Clamp(Up01, 0.0f, 1.0f) : 0.0f;
	const bool bHide = bInside && PassengerUp01 < 0.02f;
	if (bHide != bPassengerHidden)
	{
		bPassengerHidden = bHide;
		SetActorHiddenInGame(bFogHidden || bPassengerHidden);
	}
}

void ARTSUnitActor::SetFogHidden(bool bHide)
{
	if (bHide != bFogHidden)
	{
		bFogHidden = bHide;
		SetActorHiddenInGame(bHide || bPassengerHidden);   // (simulação 35) o de dentro do transporte segue escondido
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

void ARTSUnitActor::DieInfantry(bool bCrushed)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathAge = 0.0f;
	bCrushedDeath = bCrushed;
	FallSign = (CosmeticHash(SimId) & 1u) ? 1.0f : -1.0f;
	DeathBaseQ = Body->GetComponentQuat();
	for (USceneComponent* Hip : LegPivots)
	{
		if (Hip)
		{
			Hip->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	SetSilhouette(0);
	SetLifeSpan(8.2f);   // o DestructionDelay do ZH (8 s)
	if (SoldierMesh && !bCrushed)
	{
		// (30/10, a infantaria nova) a morte animada: uma das 6 do modelo (frente, costas, lados), sorteada pelo soldado (a direção do tiro
		// não chega aqui ainda); o fuzil cai da mão (some)
		if (URTSSoldierAnimInstance* Anim = Cast<URTSSoldierAnimInstance>(SoldierMesh->GetAnimInstance()))
		{
			// (31/10, o Felipe: "tem uma das mortes que tá horrível, sem sentido nenhum") a MM_Death_Back_01 (3) é a única que cai de
			// bruços: no fim, o boneco de pano abria braços e pernas com a mochila pra cima. Sai do sorteio; ficam as outras 5
			static constexpr int32 Deaths[5] = { 0, 1, 2, 4, 5 };
			Anim->Death = Deaths[CosmeticHash(SimId) % 5u];
		}
		if (SoldierRifle)
		{
			SoldierRifle->SetVisibility(false);
		}
	}
	if (bCrushed)
	{
		for (UStaticMeshComponent* Piece : SoldierPieceParts)
		{
			if (Piece && Piece->GetAttachSocketName() == FName(TEXT("head")))
			{
				Piece->SetVisibility(false);   // (30/10) o capacete sai do achatado (o resto fica achatado com o corpo)
			}
		}
	}
	if (SoldierHelmet && bCrushed)
	{
		SoldierHelmet->SetVisibility(false);
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
	StopAirFx();   // (24/10, escopo B) a carcaça da aeronave: sem disco, chama nem luz da queda (o fogo é o da carcaça)
	SetSilhouette(0);   // a carcaça não tem silhueta nem máscara (a poeira passa por cima dela)
	if (bStructure)
	{
		// (01/11) as bandeiras do prédio caem com ele (o pano some sob as explosões da morte; o mastro desce com a malha)
		for (UStaticMeshComponent* FlagMesh : FlagMeshes)
		{
			if (FlagMesh)
			{
				FlagMesh->SetVisibility(false);
			}
		}
	}
	// (31/10, a revisão: "o tanque vira queimado e a bandeira some 1 a 2 quadros antes do fogo cobrir") o queimado e a bandeira saem
	// quando a bola de fogo já cobre o casco (a da Carga Pesada fica opaca em ~3 quadros), no Tick: a troca escondida pelo clarão
	bCharPending = true;
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
		// (31/10) a ruína com a cara do prédio (Tools/model_damage.py: as muretas, os ossos e o monte na cor dele) nasce presa no chão, sob
		// as explosões da morte; o prédio afunda inteiro pra dentro dela (no lugar do tapete de pedras). 1,2% maior em planta: as muretas
		// ficam por fora das paredes que descem (sem piscar uma na outra). O prédio que morreu em obra não deixa ruína
		if (HasRuinMesh() && HullParts.Num() && HullParts[0])
		{
			if (UStaticMesh* Ruin = LoadMesh(RuinMeshPath))
			{
				RuinPart = NewObject<UStaticMeshComponent>(this);
				RuinPart->SetStaticMesh(Ruin);
				RuinPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				RuinPart->SetupAttachment(GetRootComponent());
				RuinPart->RegisterComponent();
				const FTransform T = HullParts[0]->GetComponentTransform();
				RuinPart->SetWorldTransform(FTransform(T.GetRotation(), T.GetLocation(), T.GetScale3D() * FVector(1.012, 1.012, 1.0)));
				// os dados do casco (a cor do jogador, a energia), sem as marcas de tiro e os furos (12–35)
				const TArray<float>& Data = HullParts[0]->GetCustomPrimitiveData().Data;
				for (int32 K = 0; K < Data.Num(); ++K)
				{
					RuinPart->SetCustomPrimitiveDataFloat(K, (K >= 12 && K < 36) ? 0.0f : Data[K]);
				}
			}
		}
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
	// (30/10) o veículo leve (death_scale < 1): a arma solta voa menos (a altura ∝ v², ∝ o tamanho da morte: a DShK da Technical sobe
	// ~0,4 da torre do tanque; docs/ESTUDO_IMPACTOS.md D)
	const float LaunchK = FMath::Sqrt(FMath::Clamp(DeathScale, 0.2f, 1.0f));
	const float Up = TurretUpCmS * LaunchK * (1.0f + TurretSpeedJitter * (2.0f * Random01(Seed) - 1.0f));
	const float SideSpeed = TurretSideCmS * LaunchK * (1.0f + TurretSpeedJitter * (2.0f * Random01(Seed) - 1.0f));
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

void ARTSUnitActor::SetRuinSink(float P01)
{
	// (31/10, a revisão de vídeo do fx_sucata: a ruína sumia num quadro no fim do desmanche) o ator inteiro desce (a ruína, o corpo
	// já afundado no desabar e o fogo, que segue o ator), a altura da ruína (ou do que sobrou do corpo) + 30 cm, no smoothstep
	P01 = FMath::Min(P01, 1.0f);
	if (!bDead || !bStructure || P01 <= RuinSinkP)
	{
		return;
	}
	if (RuinSinkP <= 0.0f)
	{
		RuinSinkBase = GetActorLocation();
		RuinSinkDepthCm = (RuinPart ? float(RuinPart->Bounds.BoxExtent.Z) * 2.0f : 0.0f) + 30.0f;   // sem ruína o corpo já desceu inteiro
	}
	RuinSinkP = P01;
	const float Ease = P01 * P01 * (3.0f - 2.0f * P01);
	SetActorLocation(RuinSinkBase - FVector(0.0, 0.0, RuinSinkDepthCm * Ease));
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
	if (FlyingBlades.Num())
	{
		TickFlyingBlades(DeltaSeconds);   // (24/10, escopo B) a pá do rotor que voou na queda
	}
	RecoilCm = FMath::Max(0.0f, RecoilCm - DeltaSeconds * 120.0f);
	// (30/10) o cano volta devagar (o contrarrecuo do 120 mm: ~0,4–0,6 s; a ida é instantânea, no quadro do tiro)
	BarrelRecoilCm = FMath::Max(0.0f, BarrelRecoilCm - DeltaSeconds * 110.0f);
	if (!bDead)
	{
		if (bStructure)
		{
			TickStructure(DeltaSeconds);
		}
		return;
	}
	DeathAge += DeltaSeconds;
	if (bCharPending && DeathAge >= 0.1f)
	{
		bCharPending = false;
		Char();
		for (UStaticMeshComponent* FlagMesh : FlagMeshes)
		{
			if (FlagMesh)
			{
				FlagMesh->SetVisibility(false);   // (30/10) a bandeira some no fogo da morte
			}
		}
	}
	if (bInfantry)
	{
		// (30/10, simulação 26; o SlowDeathBehavior do soldado no ZH: SinkDelay 3 s, afunda a 0,13 m/s, some aos 8 s) cai como tábua em
		// ~0,45 s (a gravidade: o ângulo ∝ t²), pra frente ou pra trás, e fica; o esmagado achata na hora (o casco passou por cima)
		if (bCrushedDeath)
		{
			Body->SetRelativeScale3D(FVector(1.15f, 1.15f, FMath::Lerp(1.0f, 0.1f, FMath::Clamp(DeathAge / 0.08f, 0.0f, 1.0f))));
		}
		else if (!SoldierMesh)
		{
			const float Fall = 86.0f * FMath::Square(FMath::Clamp(DeathAge / 0.45f, 0.0f, 1.0f));
			Body->SetWorldRotation(DeathBaseQ * FRotator(FallSign * Fall, 0.0f, 0.0f).Quaternion());
		}
		PlaceSoldierHelmet();   // (30/10) o capacete cai junto com a cabeça
		// (30/10, v8: as 6 mortes do modelo terminam de joelhos: no Lyra, de onde vieram, o boneco de pano assume no fim do clipe) no fim do
		// clipe o corpo vira boneco de pano (o PA_Mannequin) e deita no chão pela física; as peças presas nos ossos vão junto. Só desenho
		if (SoldierMesh && !bCrushedDeath && !bSoldierRagdoll)
		{
			const URTSSoldierAnimInstance* Anim = Cast<URTSSoldierAnimInstance>(SoldierMesh->GetAnimInstance());
			const float Len = Anim ? Anim->DeathLength() : 0.0f;
			if (Len > 0.0f && DeathAge >= Len - 0.12f && SoldierMesh->GetPhysicsAsset())
			{
				bSoldierRagdoll = true;
				SoldierMesh->SetCollisionProfileName(TEXT("Ragdoll"));
				SoldierMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				SoldierMesh->SetAllBodiesSimulatePhysics(true);
				SoldierMesh->SetSimulatePhysics(true);
				SoldierMesh->WakeAllRigidBodies();
			}
		}
		if (!bSoldierRagdoll)
		{
			Body->SetRelativeLocation(FVector(0.0f, 0.0f, -13.0f * FMath::Max(DeathAge - 3.0f, 0.0f)));
		}
		return;
	}
	if (!bStructure && DeathKind == 1 && DeathAge < 1.2f)
	{
		// (30/10, a assinatura da gasolina; docs/ESTUDO_IMPACTOS.md D: "a carroceria pula 1–2 m") a picape salta na bola do tanque de
		// gasolina e cai: a parábola com 5 m/s pra cima (o pico de ~1,3 m aos 0,5 s, no chão de novo em ~1 s), o nariz levantando e um
		// giro de lado que voltam a zero no pouso
		const float FlightS = 2.0f * 500.0f / GravityCm;
		const float T = FMath::Min(DeathAge, FlightS);
		const float Z = FMath::Max(0.0f, 500.0f * T - 0.5f * GravityCm * T * T);
		const uint32 H = CosmeticHash(SimId);
		const float Tilt = FMath::Sin(FMath::Clamp(DeathAge / FlightS, 0.0f, 1.0f) * PI) * (8.0f + float(H % 7u));
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, Z));
		Body->SetRelativeRotation(FRotator(Tilt, Body->GetRelativeRotation().Yaw, (H & 1u) ? Tilt * 0.6f : -Tilt * 0.6f));
	}
	if (bStructure && DeathAge < StructureSinkSeconds + 0.1f)
	{
		// desaba: afunda no chão (mais rápido no começo) e inclina um pouco pro lado. (01/11) Por etapas: 3 andares cedendo (CollapseSunk01),
		// e cada um que cede solta o pó e os pedaços na altura em que o prédio está (o StructureBreak dos furos, maior)
		const float T = FMath::Clamp((DeathAge - CollapseDelayS) / CollapseSeconds, 0.0f, 1.0f);
		const float Ease = CollapseSunk01(T);
		// (01/11) o pó de cada andar que cede: o 1º na largada (a morte se vê na hora, sem as explosões na frente), o 2º e o 3º
		static constexpr float StageAt[] = { 0.0f, 0.286f, 0.6f };
		while (CollapseStage < 3 && DeathAge >= CollapseDelayS && T >= StageAt[CollapseStage])
		{
			FVector C, E;
			GetActorBounds(false, C, E);
			const uint32 Seed = HashCombine(SimId, 0xC011u + uint32(CollapseStage) * 7919u);
			const float A = float(Seed % 6283u) / 1000.0f;
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.0f);
			const float TopCm = float(GetActorLocation().Z) + HeightCm * (1.0f - StructureSinkFraction * Ease);
			if (UBlastFxDirector* Fx = UBlastFxDirector::Get(this))
			{
				Fx->StructureBreak(FVector(C.X, C.Y, TopCm) + Out * float(FMath::Min(E.X, E.Y)) * 0.9f, Out, 260.0f, Seed, float(GetActorLocation().Z), WallTint());
			}
			++CollapseStage;
		}
		SinkCm = HeightCm * StructureSinkFraction * Ease;   // (31/10) inteiro: pra dentro da ruína, ou do monte de entulho
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
