#include "RTSAirlift.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Sim/Types.h"

namespace
{
	// os modelos provisórios da v4 (game.ps1 v4models; o caminho que o import_provisional.py gera: <pasta>/<peça>/StaticMeshes/<peça>)
	const TCHAR* PlaneMeshPath = TEXT("/Game/Units/USA_CargoPlane/SM_USA_CargoPlane/StaticMeshes/SM_USA_CargoPlane");
	const TCHAR* CanopyMeshPath = TEXT("/Game/Structures/SupplyDrop/SM_SupplyDrop_Canopy/StaticMeshes/SM_SupplyDrop_Canopy");
	const TCHAR* DropCrateMeshPath = TEXT("/Game/Structures/SupplyDrop/SM_SupplyDrop_Crate/StaticMeshes/SM_SupplyDrop_Crate");
	// o desenho de reserva: as formas da engine nos materiais dos prédios provisórios
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder");
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere");
	const TCHAR* BodyMaterialPath = TEXT("/Game/Structures/Materials/MI_Body.MI_Body");
	const TCHAR* CrateMaterialPath = TEXT("/Game/Structures/Materials/MI_Crate.MI_Crate");
	const TCHAR* PlayerMaterialPath = TEXT("/Game/Structures/Materials/MI_Player.MI_Player");

	constexpr double Tps = double(Sim::TicksPerSecond);
	// o voo (o spec v4, usa_drop_zone): 60 m/s, sobre o pátio 6,5 s antes do pouso (com o aviso de 10 s: nasce 210 m antes)
	constexpr double PlaneSpeedCmS = 6000.0;
	constexpr double PassBeforeLandS = 6.5;
	/** De onde as caixas saem (atrás da rampa aberta), no quadro do avião: o DROP_AT do model_cargo_plane.py. */
	const FVector DropAtCm(-660.0, 0.0, -160.0);
	/** Os paletes saem um atrás do outro pela rampa, na chegada (antes do pátio: ReleaseTicksOf). */
	constexpr double ChuteStaggerS = 0.25;
	/** A queda mais curta (um aviso curto nos dados não faz o paraquedas cair em meio segundo). */
	constexpr double MinFallS = 3.0;
	/** O avião some depois de passar da borda do mapa (com folga, já fora de vista) e, em todo caso, 60 s depois do pouso. */
	constexpr double PlaneExitMarginCm = 20000.0;
	constexpr double PlaneMaxLifeS = 60.0;
	// a queda: a carga sai da rampa mais rápida (2,2× a descida estável) e o pano freia em τ 0,8 s enquanto abre (~0,9 s);
	// depois desce a velocidade constante, escolhida pra tocar o chão no passo do pouso (o relógio é o da simulação)
	constexpr double CanopyOpenS = 0.9;
	constexpr double FallTauS = 0.8;
	constexpr double FallOpenRatio = 2.2;
	/** O embalo da rampa pra frente (os 60 m/s do avião), freado pelo pano em τ 0,5 s (~30 m de arrasto): o do último palete.
	 *  Os de antes saem mais longe e o pano freia mais devagar (τ = o arremesso / 60 m/s): todos saem na velocidade do avião e
	 *  param na vertical da vaga, sem passar dela e voltar contra o rumo (a revisão v4: chute_path_fix.py). */
	constexpr double DriftTauS = 0.5;
	/** A carga chega na vertical da vaga aos 80% da queda (o resto desce reto, o balanço sumindo). */
	constexpr double SpotBlendEnd = 0.8;
	/** O balanço do pêndulo (o pano e a carga): 0,5 m e 5° num período de 2,6 s (a física do conjunto de ~6 m dá ~4,9 s; mais
	 *  curto pra ler na câmera do jogo). */
	constexpr double SwayAmpCm = 50.0;
	constexpr double SwayTiltDeg = 5.0;
	constexpr double SwayPeriodS = 2.6;
	constexpr double CanopySpinDps = 12.0;
	/** O pano do modelo (5,4 m, feito pra uma caixa) segurando o palete de 2 × 2 caixas (~3 × 2,6 m): 1,35× maior. */
	constexpr double CanopyScale = 1.35;
	/** O pano murcha depois do toque e some. */
	constexpr double DeflateS = 0.6;
	/** Sem o AirliftLanded (a zona morreu no caminho), o paraquedas termina sozinho 1 s depois da hora do pouso. */
	constexpr double LandedFallbackS = 1.0;
	/** O alvo anda pro palete certo quando a empilhadeira leva um durante a queda. */
	constexpr double SpotFollowPerS = 3.0;
	// os dados de primitiva do M_Structure (Game/Tools/import_provisional.py; os mesmos do ARTSUnitActor): a cor do dono, as
	// luzes acesas (as de navegação e a baliza do avião) e o corte da obra fora do caminho. Com o prefixo: no unity build o
	// anônimo deste .cpp e o do RTSUnitActor.cpp viram um só, e os mesmos nomes davam o C2374 (redefinição)
	constexpr int32 AirliftDataPlayerColor = 0;
	constexpr int32 AirliftDataPower = 4;
	constexpr int32 AirliftDataCut = 5;

	/** As 5 vagas de palete do model_drop_zone.py (o centro e os 4 cantos do pátio, na ordem do Units.json) e as 4 caixas de
	 *  um palete em volta do centro dele, no quadro do lote (cm; a laje do pátio tem 14 cm). */
	constexpr int32 NumDefaultSpots = 5;
	constexpr int32 NumDefaultPallet = 4;
	const FVector DefaultSpotsCm[NumDefaultSpots] = { FVector(0.0, 0.0, 0.0), FVector(260.0, 260.0, 0.0), FVector(260.0, -260.0, 0.0), FVector(-260.0, 260.0, 0.0),
		FVector(-260.0, -260.0, 0.0) };
	const FVector DefaultPalletCm[NumDefaultPallet] = { FVector(-75.0, -65.0, 14.0), FVector(75.0, -65.0, 14.0), FVector(-75.0, 65.0, 14.0), FVector(75.0, 65.0, 14.0) };

	int32 SpotCount(const FRTSAirliftZone& Z)
	{
		const int32 Bpp = FMath::Max(1, Z.BoxesPerPallet);
		return Z.SlotsCm.Num() >= Bpp ? Z.SlotsCm.Num() / Bpp : NumDefaultSpots;
	}

	/** O centro do palete Spot no chão do lote (a média das vagas dele, z 0). */
	FVector SpotCenterCm(const FRTSAirliftZone& Z, int32 Spot)
	{
		const int32 Bpp = FMath::Max(1, Z.BoxesPerPallet);
		if (Z.SlotsCm.Num() < Bpp)
		{
			return DefaultSpotsCm[Spot % NumDefaultSpots];
		}
		FVector Sum = FVector::ZeroVector;
		for (int32 K = 0; K < Bpp; ++K)
		{
			Sum += Z.SlotsCm[(Spot * Bpp + K) % Z.SlotsCm.Num()];
		}
		Sum /= double(Bpp);
		return FVector(Sum.X, Sum.Y, 0.0);
	}

	/** As caixas do palete Spot em volta do centro dele (com a altura da vaga). */
	void PalletOffsetsCm(const FRTSAirliftZone& Z, int32 Spot, TArray<FVector>& Out)
	{
		Out.Reset();
		const int32 Bpp = FMath::Max(1, Z.BoxesPerPallet);
		if (Z.SlotsCm.Num() < Bpp)
		{
			for (int32 K = 0; K < Bpp; ++K)
			{
				Out.Add(DefaultPalletCm[K % NumDefaultPallet]);
			}
			return;
		}
		const FVector Center = SpotCenterCm(Z, Spot);
		for (int32 K = 0; K < Bpp; ++K)
		{
			Out.Add(Z.SlotsCm[(Spot * Bpp + K) % Z.SlotsCm.Num()] - Center);
		}
	}

	UStaticMeshComponent* AddMesh(AActor* A, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& AtCm, const FRotator& Rot, const FVector& Scale,
		UMaterialInterface* Material, const FLinearColor& Color)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(A);
		M->SetupAttachment(Parent);
		M->SetStaticMesh(Mesh);
		M->SetRelativeLocation(AtCm);
		M->SetRelativeRotation(Rot);
		M->SetRelativeScale3D(Scale);
		if (Material)
		{
			for (int32 K = 0; K < FMath::Max(1, M->GetNumMaterials()); ++K)
			{
				M->SetMaterial(K, Material);
			}
		}
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->SetReceivesDecals(false);
		M->SetCanEverAffectNavigation(false);
		M->SetMobility(EComponentMobility::Movable);
		M->RegisterComponent();
		M->SetCustomPrimitiveDataVector4(AirliftDataPlayerColor, FVector4(Color.R, Color.G, Color.B, 1.0f));
		M->SetCustomPrimitiveDataFloat(AirliftDataPower, 1.0f);
		M->SetCustomPrimitiveDataFloat(AirliftDataCut, 1000.0f);
		return M;
	}

	FRotator YawOf(const FVector2D& Dir)
	{
		return FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0);
	}
}

FRTSAirlift::~FRTSAirlift()
{
	Shutdown();
}

void FRTSAirlift::Shutdown()
{
	for (FFlight& F : Flights)
	{
		if (AActor* P = F.Plane.Get())
		{
			P->Destroy();
		}
		for (FChute& C : F.Chutes)
		{
			if (AActor* A = C.Actor.Get())
			{
				A->Destroy();
			}
		}
	}
	Flights.Reset();
	Meshes.Reset();
	Materials.Reset();
	LastNowTicks = -1.0;
}

UStaticMesh* FRTSAirlift::MeshAt(const FString& PackagePath)
{
	if (PackagePath.IsEmpty())
	{
		return nullptr;
	}
	if (const TStrongObjectPtr<UStaticMesh>* Have = Meshes.Find(PackagePath))
	{
		return Have->Get();
	}
	const FString Name = FPaths::GetBaseFilename(PackagePath);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTS avião de carga: sem a malha %s (fica o desenho de reserva)"), *PackagePath);
	}
	Meshes.Add(PackagePath, TStrongObjectPtr<UStaticMesh>(Mesh));
	return Mesh;
}

UMaterialInterface* FRTSAirlift::MaterialAt(const TCHAR* ObjectPath)
{
	const FString Key(ObjectPath);
	if (const TStrongObjectPtr<UMaterialInterface>* Have = Materials.Find(Key))
	{
		return Have->Get();
	}
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Materials.Add(Key, TStrongObjectPtr<UMaterialInterface>(M));
	return M;
}

AActor* FRTSAirlift::SpawnBare(const TCHAR* Name) const
{
	UWorld* World = WorldRef.Get();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!A)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(A, Name);
	Root->SetMobility(EComponentMobility::Movable);
	A->SetRootComponent(Root);
	Root->RegisterComponent();
	return A;
}

void FRTSAirlift::SpawnPlane(FFlight& F)
{
	AActor* A = SpawnBare(TEXT("CargoPlane"));
	if (!A)
	{
		return;
	}
	USceneComponent* Root = A->GetRootComponent();
	if (UStaticMesh* Mesh = MeshAt(PlaneMeshPath))
	{
		// o C-130 provisório (model_cargo_plane.py): origem no eixo da fuselagem embaixo da asa, nariz em +X, a faixa e as
		// pontas de asa na cor do dono, as luzes de navegação e a baliza acesas
		AddMesh(A, Root, Mesh, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, nullptr, F.Color);
	}
	else if (UStaticMesh* Cube = MeshAt(CubePath))
	{
		// o desenho de reserva: a fuselagem (22 m), a asa alta (30 m), o estabilizador e a deriva na cor do dono
		UMaterialInterface* Body = MaterialAt(BodyMaterialPath);
		if (UStaticMesh* Cylinder = MeshAt(CylinderPath))
		{
			AddMesh(A, Root, Cylinder, FVector(0.0, 0.0, 30.0), FRotator(90.0, 0.0, 0.0), FVector(3.3, 3.3, 22.0), Body, F.Color);
		}
		AddMesh(A, Root, Cube, FVector(100.0, 0.0, 205.0), FRotator::ZeroRotator, FVector(3.7, 30.0, 0.4), Body, F.Color);
		AddMesh(A, Root, Cube, FVector(-950.0, 0.0, 160.0), FRotator::ZeroRotator, FVector(2.4, 10.0, 0.3), Body, F.Color);
		AddMesh(A, Root, Cube, FVector(-950.0, 0.0, 350.0), FRotator::ZeroRotator, FVector(3.0, 0.3, 4.0), MaterialAt(PlayerMaterialPath), F.Color);
	}
	F.Plane = A;
}

void FRTSAirlift::SpawnChute(FFlight& F, FChute& C)
{
	AActor* A = SpawnBare(TEXT("SupplyChute"));
	if (!A)
	{
		return;
	}
	USceneComponent* Root = A->GetRootComponent();
	A->SetActorLocation(C.ReleaseCm);
	// a carga: o palete de caixas do pátio (a mesma malha e as mesmas vagas: no toque, a troca pelas caixas do prédio não pula)
	const FRTSAirliftZone& Z = F.LastZone;
	const int32 Bpp = FMath::Max(1, Z.BoxesPerPallet);
	const int32 Spot = ((FMath::Max(0, Z.Cargo) + Bpp - 1) / Bpp + C.Index) % SpotCount(Z);
	TArray<FVector> Offsets;
	PalletOffsetsCm(Z, Spot, Offsets);
	UStaticMesh* Crate = MeshAt(Z.CrateMesh);
	if (!Crate)
	{
		Crate = MeshAt(DropCrateMeshPath);   // a zona no visual de reserva (sem caixas no pátio): a caixa do lançamento
	}
	const bool bCube = Crate == nullptr;
	if (bCube)
	{
		Crate = MeshAt(CubePath);
	}
	for (const FVector& O : Offsets)
	{
		if (Crate)
		{
			C.Crates.Add(AddMesh(A, Root, Crate, bCube ? O + FVector(0.0, 0.0, 50.0) : O, FRotator::ZeroRotator,
				bCube ? FVector(1.2, 1.0, 1.0) : FVector::OneVector, bCube ? MaterialAt(CrateMaterialPath) : nullptr, F.Color));
		}
	}
	// o pano num pivô na origem da carga (abre, gira devagar, murcha): o modelo tem a mesma origem da caixa
	USceneComponent* Pivot = NewObject<USceneComponent>(A, TEXT("Canopy"));
	Pivot->SetupAttachment(Root);
	Pivot->SetMobility(EComponentMobility::Movable);
	Pivot->RegisterComponent();
	if (UStaticMesh* Canopy = MeshAt(CanopyMeshPath))
	{
		AddMesh(A, Pivot, Canopy, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, nullptr, F.Color);
	}
	else if (UStaticMesh* Sphere = MeshAt(SpherePath))
	{
		// o desenho de reserva: a cúpula de 5,4 m na cor do dono, ~5 m acima da caixa
		AddMesh(A, Pivot, Sphere, FVector(0.0, 0.0, 520.0), FRotator::ZeroRotator, FVector(5.4, 5.4, 1.8), MaterialAt(PlayerMaterialPath), F.Color);
	}
	C.Canopy = Pivot;
	C.Actor = A;
}

FVector FRTSAirlift::PlaneAt(const FFlight& F, double Ticks) const
{
	const FVector Pad = F.LastZone.Body.GetLocation();
	const FVector2D Xy = FVector2D(Pad.X, Pad.Y) + F.Dir * (PlaneSpeedCmS * (Ticks - F.PassTicks) / Tps);
	return FVector(Xy.X, Xy.Y, Pad.Z + F.AltitudeCm);
}

double FRTSAirlift::ReleaseTicksOf(const FFlight& F, int32 Index) const
{
	// a soltura antes do pátio (o ponto de soltura calculado dos lançamentos de verdade): a rampa do último palete passa 30 m
	// (o arremesso nominal) antes do centro do pátio e os outros saem 0,25 s antes, um a um; o avião passa sobre o pátio
	// depois (os 6,5 s do spec)
	const double LastS = (-PlaneSpeedCmS * DriftTauS - DropAtCm.X) / PlaneSpeedCmS;
	const double AheadS = double(F.Pallets - 1 - Index) * ChuteStaggerS;
	return FMath::Min(F.PassTicks + (LastS - AheadS) * Tps, F.LandTicks - MinFallS * Tps);
}

void FRTSAirlift::Inbound(UWorld* World, uint32 Zone, int32 Pallets, double LandTicks, const FRTSAirliftZone& Info, const FVector2D& FromCm,
	float AltitudeM, const FLinearColor& OwnerColor)
{
	if (!World)
	{
		return;
	}
	WorldRef = World;
	FFlight& F = Flights.AddDefaulted_GetRef();
	F.Zone = Zone;
	F.Pallets = FMath::Clamp(Pallets, 0, 16);
	F.LandTicks = LandTicks;
	F.PassTicks = LandTicks - PassBeforeLandS * Tps;
	F.LastZone = Info;
	// a rota: da largada do dono passando pela zona (o avião chega "do nosso lado"); a zona em cima da largada voa em +X
	const FVector Pad = Info.Body.GetLocation();
	const FVector2D Way = FVector2D(Pad.X, Pad.Y) - FromCm;
	F.Dir = Way.Size() > 100.0 ? Way.GetSafeNormal() : FVector2D(1.0, 0.0);
	F.AltitudeCm = FMath::Max(10.0f, AltitudeM) * 100.0f;
	F.Color = OwnerColor;
	SpawnPlane(F);
	UE_LOG(LogTemp, Log, TEXT("RTS avião de carga: zona %u, %d paletes, passa sobre o pátio no passo %.0f e pousa no %.0f (a %.0f m, rumo %.0f°)%s"),
		Zone, F.Pallets, F.PassTicks, F.LandTicks, AltitudeM, YawOf(F.Dir).Yaw, MeshAt(PlaneMeshPath) ? TEXT("") : TEXT(", desenho de reserva"));
}

void FRTSAirlift::MarkLanded(FFlight& F, double AtTicks)
{
	if (F.bLanded)
	{
		return;
	}
	F.bLanded = true;
	F.Released = F.Pallets;   // os que nem saíram da rampa não saem mais
	for (FChute& C : F.Chutes)
	{
		if (C.LandedTicks >= 0.0)
		{
			continue;
		}
		C.LandedTicks = AtTicks;
		for (const TWeakObjectPtr<UStaticMeshComponent>& Crate : C.Crates)
		{
			if (UStaticMeshComponent* M = Crate.Get())
			{
				M->SetVisibility(false);   // as caixas do prédio aparecem no mesmo quadro (SetCargo)
			}
		}
	}
}

void FRTSAirlift::Landed(uint32 Zone)
{
	for (FFlight& F : Flights)
	{
		if (F.Zone == Zone && !F.bLanded)
		{
			UE_LOG(LogTemp, Log, TEXT("RTS avião de carga: zona %u, %d paraquedas no pátio (passo do desenho %.1f, o do pouso %.0f)"), Zone, F.Chutes.Num(),
				LastNowTicks, F.LandTicks);
			MarkLanded(F, LastNowTicks >= 0.0 ? LastNowTicks : F.LandTicks);
			return;
		}
	}
}

void FRTSAirlift::UpdateChute(FFlight& F, FChute& C, double Now, float DtS, TFunctionRef<bool(const FVector2D&)> IsShown)
{
	AActor* A = C.Actor.Get();
	if (!A)
	{
		return;
	}
	if (C.LandedTicks >= 0.0)
	{
		// no chão: o pano murcha por cima das caixas e some
		const double E = FMath::Clamp((Now - C.LandedTicks) / Tps / DeflateS, 0.0, 1.0);
		if (USceneComponent* Pivot = C.Canopy.Get())
		{
			Pivot->SetRelativeScale3D(FVector(CanopyScale * (1.0 + 0.15 * E), CanopyScale * (1.0 + 0.15 * E), CanopyScale * FMath::Max(0.05, 1.0 - 0.9 * E)));
		}
		if (E >= 1.0)
		{
			A->Destroy();
			C.Actor = nullptr;
		}
		return;
	}
	// o alvo: a vaga do palete (os paletes no pátio agora + a ordem na rampa), andando devagar se a empilhadeira levar um
	const FRTSAirliftZone& Z = F.LastZone;
	const int32 Bpp = FMath::Max(1, Z.BoxesPerPallet);
	const int32 Spot = ((FMath::Max(0, Z.Cargo) + Bpp - 1) / Bpp + C.Index) % SpotCount(Z);
	const FVector Target = Z.Body.TransformPosition(SpotCenterCm(Z, Spot));
	const FVector2D Target2(Target.X, Target.Y);
	const FVector2D ReleaseXy(C.ReleaseCm.X, C.ReleaseCm.Y);
	if (!C.bSpot)
	{
		C.SpotCm = Target2;
		C.bSpot = true;
		// o arremesso: da soltura até a vaga, no rumo (a soltura sai antes do pátio: sempre pra frente)
		C.ThrowCm = FMath::Max(100.0, FVector2D::DotProduct(Target2 - ReleaseXy, F.Dir));
	}
	else
	{
		C.SpotCm += (Target2 - C.SpotCm) * (1.0 - FMath::Exp(-double(DtS) * SpotFollowPerS));
	}
	// a altura: a descida que toca o chão no passo do pouso (a forma da curva normalizada pela queda inteira)
	const double PadZ = Z.Body.GetLocation().Z;
	const double Tt = FMath::Max(0.0, (Now - C.ReleaseTicks) / Tps);
	const double Tf = FMath::Max(0.5, (F.LandTicks - C.ReleaseTicks) / Tps);
	const double U = FMath::Clamp(Tt / Tf, 0.0, 1.0);
	const auto Fallen = [](double T) { return T + (FallOpenRatio - 1.0) * FallTauS * (1.0 - FMath::Exp(-T / FallTauS)); };
	const double H0 = FMath::Max(0.0, C.ReleaseCm.Z - PadZ);
	const double H = FMath::Max(0.0, H0 * (1.0 - Fallen(FMath::Min(Tt, Tf)) / Fallen(Tf)));
	// o lugar: o embalo da rampa (sai a 60 m/s e o pano freia até a vertical da vaga), o resto (o lado da vaga) entrando até
	// os 80% da queda, mais o balanço do pêndulo (some perto do chão); com a vaga parada, só anda pra frente no rumo
	const double Drift = C.ThrowCm * (1.0 - FMath::Exp(-Tt * PlaneSpeedCmS / C.ThrowCm));
	const FVector2D From = ReleaseXy + F.Dir * Drift;
	const double Env = FMath::SmoothStep(0.05, 0.3, U) * (1.0 - FMath::SmoothStep(0.7, 1.0, U));
	const double Ph = 2.0 * UE_PI * Tt / SwayPeriodS + double(C.Phase);
	const FVector2D Side(-F.Dir.Y, F.Dir.X);
	const FVector2D Xy = FMath::Lerp(From, C.SpotCm, FMath::SmoothStep(0.0, SpotBlendEnd, U)) + Side * (SwayAmpCm * Env * FMath::Sin(Ph));
	const FQuat Tilt(FVector(F.Dir.X, F.Dir.Y, 0.0), FMath::DegreesToRadians(SwayTiltDeg * Env * FMath::Cos(Ph)));
	A->SetActorLocationAndRotation(FVector(Xy.X, Xy.Y, PadZ + H), Tilt * Z.Body.GetRotation());
	if (USceneComponent* Pivot = C.Canopy.Get())
	{
		// o pano abre (do saco fechado à cúpula) e gira devagar
		const double O = FMath::SmoothStep(0.0, CanopyOpenS, Tt);
		Pivot->SetRelativeScale3D(FVector(CanopyScale * (0.2 + 0.8 * O), CanopyScale * (0.2 + 0.8 * O), CanopyScale * (0.35 + 0.65 * O)));
		Pivot->SetRelativeRotation(FRotator(0.0, FMath::Fmod(CanopySpinDps * Tt + FMath::RadiansToDegrees(double(C.Phase)), 360.0), 0.0));
	}
	const bool bShown = IsShown(Xy);
	if (bShown != C.bShown)
	{
		C.bShown = bShown;
		A->SetActorHiddenInGame(!bShown);
	}
}

void FRTSAirlift::Tick(double NowTicks, TFunctionRef<bool(uint32, FRTSAirliftZone&)> ZoneOf, TFunctionRef<bool(const FVector2D&)> IsShown,
	const FBox2D& MapCm)
{
	if (Flights.IsEmpty())
	{
		LastNowTicks = NowTicks;
		return;
	}
	// o relógio só anda pra frente (na LAN o acumulador do passo pode recuar um pouco)
	const double Now = LastNowTicks < 0.0 ? NowTicks : FMath::Max(NowTicks, LastNowTicks);
	const float DtS = LastNowTicks < 0.0 ? 0.0f : float((Now - LastNowTicks) / Tps);
	LastNowTicks = Now;
	const FBox2D Exit = MapCm.ExpandBy(PlaneExitMarginCm);
	for (int32 I = Flights.Num() - 1; I >= 0; --I)
	{
		FFlight& F = Flights[I];
		FRTSAirliftZone Fresh;
		if (ZoneOf(F.Zone, Fresh))
		{
			F.LastZone = MoveTemp(Fresh);
		}
		// o avião
		if (!F.bPlaneGone)
		{
			const FVector At = PlaneAt(F, Now);
			AActor* Plane = F.Plane.Get();
			if (Plane)
			{
				Plane->SetActorLocationAndRotation(At, YawOf(F.Dir));
				const bool bShown = IsShown(FVector2D(At.X, At.Y));
				if (bShown != F.bPlaneShown)
				{
					F.bPlaneShown = bShown;
					Plane->SetActorHiddenInGame(!bShown);
				}
			}
			const bool bOut = Now > F.PassTicks && !Exit.IsInside(FVector2D(At.X, At.Y));
			if (!Plane || bOut || (Now - F.LandTicks) / Tps > PlaneMaxLifeS)
			{
				if (Plane)
				{
					Plane->Destroy();
				}
				F.Plane = nullptr;
				F.bPlaneGone = true;
			}
		}
		// os paletes saindo pela rampa, um por um
		while (!F.bLanded && F.Released < F.Pallets && Now >= ReleaseTicksOf(F, F.Released))
		{
			FChute& New = F.Chutes.AddDefaulted_GetRef();
			New.Index = F.Released;
			New.ReleaseTicks = ReleaseTicksOf(F, F.Released);
			New.ReleaseCm = PlaneAt(F, New.ReleaseTicks) + YawOf(F.Dir).RotateVector(DropAtCm);
			New.Phase = float(New.Index) * 2.4f + float(F.Zone % 7u) * 0.9f;
			SpawnChute(F, New);
			++F.Released;
		}
		if (!F.bLanded && Now >= F.LandTicks + LandedFallbackS * Tps)
		{
			MarkLanded(F, F.LandTicks);
		}
		bool bAnyChute = false;
		for (FChute& C : F.Chutes)
		{
			UpdateChute(F, C, Now, DtS, IsShown);
			bAnyChute |= C.Actor.IsValid();
		}
		if (F.bPlaneGone && F.bLanded && !bAnyChute)
		{
			Flights.RemoveAt(I);
		}
	}
}
